# src/ph7/vm_builtin_spl.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 7623/8476 lines (89.94%)

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
|     - |     9 | ` * SPL iterators, slice 1: SeekableIterator, ArrayIterator,` |
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
|     4 |   365 | `			&pObj->pClass->sDisp,(int)pObj->nObjId);` |
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
|  8445 |   504 | `static sxi32 VmInstallWeak(ph7_vm *pVm)` |
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
|  8450 |   547 | `	sxi32 rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|  8450 |   548 | `	if( rc != SXRET_OK ){` |
|   ! 0 |   549 | `		return rc;` |
|     - |   550 | `	}` |
|  8450 |   551 | `	pMap = PH7_VmExtractClass(&(*pVm),"WeakMap",sizeof("WeakMap")-1,FALSE,0);` |
|  8450 |   552 | `	if( pMap == 0 ){` |
|   ! 0 |   553 | `		return SXERR_NOTFOUND;` |
|     - |   554 | `	}` |
| 33785 |   555 | `	for( n = 0 ; n < SX_ARRAYSIZE(azMapIface) ; n++ ){` |
| 37991 |   556 | `		ph7_class *pIface = PH7_VmExtractClass(&(*pVm),azMapIface[n],` |
| 25335 |   557 | `			(sxu32)SyStrlen(azMapIface[n]),FALSE,0);` |
| 25340 |   558 | `		if( pIface == 0 ){` |
|   ! 0 |   559 | `			return SXERR_NOTFOUND;` |
|     - |   560 | `		}` |
| 25340 |   561 | `		rc = PH7_ClassImplement(pMap,pIface);` |
| 25340 |   562 | `		if( rc != SXRET_OK ){` |
|   ! 0 |   563 | `			return rc;` |
|     - |   564 | `		}` |
| 12656 |   565 | `	}` |
|  8450 |   566 | `	return SXRET_OK;` |
|  4222 |   567 | `}` |
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
|     - |   669 | ` * TRAIT and the last name in the native-class ledger that was not a function. php shares nothing` |
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
|  9406 |   711 | `static ph7_value * SplStoreSlot(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     5 |   712 | `{` |
|  9411 |   713 | `	ph7_value *pSlot = pThis ? PH7_NativeAttr(pThis,SPL_D) : 0;` |
|  9411 |   714 | `	if( pSlot == 0 ){` |
|   ! 0 |   715 | `		return 0;` |
|     - |   716 | `	}` |
|  9411 |   717 | `	if( (pSlot->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|   ! 0 |   718 | `		if( PH7_MemObjToHashmap(pSlot) != SXRET_OK ){` |
|   ! 0 |   719 | `			return 0;` |
|     - |   720 | `		}` |
|   ! 0 |   721 | `	}` |
|  9411 |   722 | `	if( PH7_HashmapCowSeparate(pVm,pSlot) == 0 ){` |
|   ! 0 |   723 | `		return 0;` |
|     - |   724 | `	}` |
|  9411 |   725 | `	return PH7_NativeAttr(pThis,SPL_D);` |
|  4708 |   726 | `}` |
|  5542 |   727 | `static ph7_hashmap * SplStore(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     5 |   728 | `{` |
|  5547 |   729 | `	ph7_value *pSlot = SplStoreSlot(pVm,pThis);` |
|  5547 |   730 | `	return pSlot ? (ph7_hashmap *)pSlot->x.pOther : 0;` |
|     5 |   731 | `}` |
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
|     3 |   752 | `{` |
|   157 |   753 | `	ph7_hashmap_node *pNode = 0;` |
|   157 |   754 | `	if( pThis == 0 \|\| pKey == 0 ){` |
|   ! 0 |   755 | `		return SXU32_HIGH;` |
|     - |   756 | `	}` |
|   157 |   757 | `	if( pKey->iFlags & MEMOBJ_NULL ){` |
|     - |   758 | `		/* PH7_HashmapLookup folds a NULL key to "" IN PLACE, and a declined fast path` |
|     - |   759 | `		 * has to hand the accessor the key it was written with (php deprecates the null` |
|     - |   760 | `		 * offset there). Cheaper to stand down than to probe on a copy. */` |
|     9 |   761 | `		return SXU32_HIGH;` |
|     - |   762 | `	}` |
|   146 |   763 | `	if( (pKey->iFlags & MEMOBJ_RES)` |
|   145 |   764 | `	 \|\| ((pKey->iFlags & MEMOBJ_REAL) && pKey->rVal != (ph7_real)(sxi64)pKey->rVal) ){` |
|     - |   765 | `		/* Same reason, for the two keys the accessor answers with a DIAGNOSTIC: a` |
|     - |   766 | `		 * resource is php's warning plus its integer id, and a lossy float is the scope policy's` |
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
|    80 |   805 | `}` |
|     - |   806 | `/*` |
|     - |   807 | `` * `$this->__d = $array` for the constructor and exchangeArray(), with php's refusal.`` |
|     - |   808 | ` *` |
|     - |   809 | `` * php DECLARES `object\|array $array` — which is what Reflection prints — and then words the`` |
|     - |   810 | `` * refusal as `must be of type array`, so the shared ZPP screen cannot say both (rule 41's`` |
|     - |   811 | ` * shape) and the check is written here. An OBJECT contributes its properties, as the PHP` |
|     - |   812 | ` * did through get_object_vars().` |
|     - |   813 | ` */` |
|  1568 |   814 | `static sxi32 SplInitStore(ph7_context *pCtx,ph7_class_instance *pThis,` |
|     - |   815 | `	ph7_value *pArray,const char *zOwner)` |
|     5 |   816 | `{` |
|     - |   817 | `	char zGiven[64];` |
|  1573 |   818 | `	ph7_vm *pVm = pCtx->pVm;` |
|  1573 |   819 | `	ph7_value *pSlot = pThis ? PH7_NativeAttr(pThis,SPL_D) : 0;` |
|  1573 |   820 | `	if( pSlot == 0 ){` |
|   ! 0 |   821 | `		return PH7_OK;` |
|     - |   822 | `	}` |
|  1573 |   823 | `	if( pArray == 0 ){` |
|     - |   824 | ``		/* No argument at all: php's `$array = []` default. A native method has no compiled`` |
|     - |   825 | `		 * parameter records for the defaults to live in (rule 33's neighbour), so the body` |
|     - |   826 | `		 * applies it — and an EXPLICIT null still has to reach the refusal below, which is` |
|     - |   827 | `		 * why the two cases are distinguished here rather than by a NULL check. */` |
|   183 |   828 | `		ph7_hashmap *pEmpty = PH7_NewHashmap(pVm,0,0);` |
|   183 |   829 | `		if( pEmpty == 0 ){` |
|   ! 0 |   830 | `			return PH7_ContextMemoryError(pCtx);` |
|     - |   831 | `		}` |
|   183 |   832 | `		PH7_MemObjRelease(pSlot);` |
|   183 |   833 | `		pSlot->x.pOther = pEmpty;` |
|   183 |   834 | `		MemObjSetType(pSlot,MEMOBJ_HASHMAP);` |
|   183 |   835 | `		return PH7_OK;` |
|     - |   836 | `	}` |
|  1395 |   837 | `	if( pArray->iFlags & MEMOBJ_HASHMAP ){` |
|  1379 |   838 | `		PH7_MemObjRelease(pSlot);` |
|  1379 |   839 | `		PH7_MemObjStore(pArray,pSlot); /* a copy: the store is the object's own */` |
|  1379 |   840 | `		return PH7_OK;` |
|     - |   841 | `	}` |
|    17 |   842 | `	if( pArray->iFlags & MEMOBJ_OBJ ){` |
|     - |   843 | `		/* The PHP read get_object_vars($array): the properties this scope can see, by` |
|     - |   844 | `		 * their plain names. php itself keeps the OBJECT and reads its property table` |
|     - |   845 | `		 * live (so getArrayCopy() answers the mangled private names and count() answers` |
|     - |   846 | `		 * the visible ones) — a divergence this conversion carries over unchanged rather` |
|     - |   847 | `		 * than widening, recorded. */` |
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
|   789 |   888 | `}` |
|     - |   889 | `/* Hand one of the engine's own array builtins the instance's storage slot. */` |
|  3488 |   890 | `static int SplArrayCall(ph7_context *pCtx,ProchHostFunction xFunc,ph7_value *pExtra)` |
|     3 |   891 | `{` |
|  3491 |   892 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |   893 | `	ph7_value *apCall[2];` |
|  3491 |   894 | `	ph7_value *pSlot = SplStoreSlot(pCtx->pVm,pThis);` |
|  3491 |   895 | `	if( pSlot == 0 ){` |
|   ! 0 |   896 | `		return PH7_OK;` |
|     - |   897 | `	}` |
|  3491 |   898 | `	apCall[0] = pSlot;` |
|  3491 |   899 | `	apCall[1] = pExtra;` |
|  3491 |   900 | `	return xFunc(pCtx,pExtra ? 2 : 1,apCall);` |
|  1747 |   901 | `}` |
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
|     - |   917 | ` *   LOSSY FLOAT      the scope policy: php deprecates and truncates, PHL refuses — but only` |
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
|   320 |   936 | `static int SplOffsetKeyArg(ph7_context *pCtx,ph7_value *pKey,int iKind,int *pRc)` |
|     4 |   937 | `{` |
|   324 |   938 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   324 |   939 | `	SyString *pOwner = pThis ? &pThis->pClass->sName : 0;` |
|   324 |   940 | `	SyString *pClass = 0;` |
|   324 |   941 | `	const char *zType = 0;` |
|   324 |   942 | `	*pRc = PH7_OK;` |
|   324 |   943 | `	if( pKey->iFlags & MEMOBJ_OBJ ){` |
|    62 |   944 | `		ph7_class_instance *pInst = (ph7_class_instance *)pKey->x.pOther;` |
|    62 |   945 | `		if( pInst && pInst->pClass ){` |
|    62 |   946 | `			pClass = &pInst->pClass->sName;` |
|    30 |   947 | `		}` |
|    62 |   948 | `		zType = "object";` |
|   294 |   949 | `	}else if( pKey->iFlags & MEMOBJ_HASHMAP ){` |
|    60 |   950 | `		zType = "array";` |
|   233 |   951 | `	}else if( (pKey->iFlags & MEMOBJ_REAL)` |
|   111 |   952 | `	       && pKey->rVal != (ph7_real)(sxi64)pKey->rVal` |
|    22 |   953 | `	       && (iKind == SPL_OFF_ACCESS \|\| iKind == SPL_OFF_SET) ){` |
|    11 |   954 | `		zType = "float";` |
|     5 |   955 | `	}` |
|   324 |   956 | `	if( zType == 0 ){` |
|   196 |   957 | `		PH7_VmOffsetResourceWarn(pCtx->pVm,pKey);` |
|   196 |   958 | `		if( iKind != SPL_OFF_SET ){` |
|   120 |   959 | `			PH7_VmNullOffsetDeprecate(pCtx->pVm,pKey);` |
|    58 |   960 | `		}` |
|   196 |   961 | `		return 0;` |
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
|   164 |   978 | `}` |
|    80 |   979 | `static int vm_builtin_SplStore_offsetExists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     4 |   980 | `{` |
|    84 |   981 | `	ph7_hashmap *pMap = SplStore(pCtx->pVm,PH7_ContextThis(pCtx));` |
|    84 |   982 | `	ph7_hashmap_node *pNode = 0;` |
|    84 |   983 | `	int bFound = 0, rc;` |
|    84 |   984 | `	if( nArg > 0 && SplOffsetKeyArg(pCtx,apArg[0],SPL_OFF_ISSET,&rc) ){` |
|    46 |   985 | `		return rc;` |
|     - |   986 | `	}` |
|    40 |   987 | `	if( pMap && nArg > 0 ){` |
|     - |   988 | `		/* array_key_exists(), not isset(): php's offsetExists() answers true for a key` |
|     - |   989 | `		 * holding NULL (the PHP said array_key_exists too). */` |
|    40 |   990 | `		bFound = PH7_HashmapLookup(pMap,apArg[0],&pNode) == SXRET_OK;` |
|    18 |   991 | `	}` |
|    40 |   992 | `	ph7_result_bool(pCtx,bFound);` |
|    40 |   993 | `	return PH7_OK;` |
|    44 |   994 | `}` |
|    88 |   995 | `static int vm_builtin_SplStore_offsetGet(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     4 |   996 | `{` |
|    92 |   997 | `	ph7_vm *pVm = pCtx->pVm;` |
|    92 |   998 | `	ph7_hashmap *pMap = SplStore(pVm,PH7_ContextThis(pCtx));` |
|    92 |   999 | `	ph7_hashmap_node *pNode = 0;` |
|     - |  1000 | `	int rcKey;` |
|    92 |  1001 | `	if( nArg > 0 && SplOffsetKeyArg(pCtx,apArg[0],SPL_OFF_ACCESS,&rcKey) ){` |
|    34 |  1002 | `		return rcKey;` |
|     - |  1003 | `	}` |
|    59 |  1004 | `	if( pMap == 0 \|\| nArg < 1 \|\| PH7_HashmapLookup(pMap,apArg[0],&pNode) != SXRET_OK ){` |
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
|    57 |  1030 | `	ph7_result_value(pCtx,(ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pNode->nValIdx));` |
|    57 |  1031 | `	return PH7_OK;` |
|    48 |  1032 | `}` |
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
|   134 |  1088 | `static int vm_builtin_SplStore_getArrayCopy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  1089 | `{` |
|   137 |  1090 | `	ph7_value *pSlot = SplStoreSlot(pCtx->pVm,PH7_ContextThis(pCtx));` |
|    67 |  1091 | `	SXUNUSED(nArg);` |
|    67 |  1092 | `	SXUNUSED(apArg);` |
|   137 |  1093 | `	if( pSlot ){` |
|   137 |  1094 | `		ph7_result_value(pCtx,pSlot); /* a COPY: the caller must not alias the store */` |
|    67 |  1095 | `	}` |
|   137 |  1096 | `	return PH7_OK;` |
|     3 |  1097 | `}` |
|    34 |  1098 | `static int vm_builtin_SplStore_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  1099 | `{` |
|    36 |  1100 | `	ph7_hashmap *pMap = SplStore(pCtx->pVm,PH7_ContextThis(pCtx));` |
|    17 |  1101 | `	SXUNUSED(nArg);` |
|    17 |  1102 | `	SXUNUSED(apArg);` |
|    36 |  1103 | `	ph7_result_int64(pCtx,pMap ? (ph7_int64)pMap->nEntry : 0);` |
|    36 |  1104 | `	return PH7_OK;` |
|     2 |  1105 | `}` |
|    20 |  1106 | `static int vm_builtin_SplStore_getFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  1107 | `{` |
|    22 |  1108 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    10 |  1109 | `	SXUNUSED(nArg);` |
|    10 |  1110 | `	SXUNUSED(apArg);` |
|    22 |  1111 | `	ph7_result_int64(pCtx,pThis ? PH7_NativeAttrInt(pThis,SPL_F) : 0);` |
|    22 |  1112 | `	return PH7_OK;` |
|     2 |  1113 | `}` |
|     4 |  1114 | `static int vm_builtin_SplStore_setFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  1115 | `{` |
|     6 |  1116 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     6 |  1117 | `	if( pThis && nArg > 0 ){` |
|     8 |  1118 | `		PH7_NativeSetAttrInt(pCtx->pVm,pThis,SPL_F,` |
|     4 |  1119 | `			ph7_value_to_int64(apArg[0]) & SPL_FLAG_MASK);` |
|     2 |  1120 | `	}` |
|     6 |  1121 | `	return PH7_OK;` |
|     2 |  1122 | `}` |
|     - |  1123 | `/*` |
|     - |  1124 | ` * The six sorts. Each is the engine's own builtin over the stored array — including the` |
|     - |  1125 | `` * `$flags` the PHP DROPPED on the floor (`asort($this->__d)` ignored its own parameter, so`` |
|     - |  1126 | `` * `$it->asort(SORT_STRING)` sorted numerically). natsort/natcasesort go through asort with`` |
|     - |  1127 | ` * php's own flag pair rather than by name: the shared body reads ph7_function_name() to tell` |
|     - |  1128 | `` * the two apart, and a native method's name is `ArrayIterator::natcasesort`.`` |
|     - |  1129 | ` */` |
|     - |  1130 | `/*` |
|     - |  1131 | ` * php runs each sort as a real call to the array function of the same name` |
|     - |  1132 | ` * (spl_array_method), so that function has a frame of its own between the method and` |
|     - |  1133 | ` * anything the sort reaches for -- a comparator, a __toString, an error handler:` |
|     - |  1134 | `` * `[internal function]: uasort()` over `f.php(N): ArrayObject->uasort()`. Its record`` |
|     - |  1135 | ` * lists php's arguments, not the ones the method was given: the array and the flags` |
|     - |  1136 | ` * (0 when none were passed) for asort/ksort, the array and the callback for` |
|     - |  1137 | ` * uasort/uksort, the array alone for natsort/natcasesort. pRecArg is 0 for the last.` |
|     - |  1138 | ` */` |
|    58 |  1139 | `static int SplArraySortCall(ph7_context *pCtx,const char *zName,ProchHostFunction xFunc,` |
|     - |  1140 | `	ph7_value *pExtra,ph7_value *pRecArg)` |
|     4 |  1141 | `{` |
|    62 |  1142 | `	ph7_vm *pVm = pCtx->pVm;` |
|    62 |  1143 | `	ph7_value *pSlot = SplStoreSlot(pVm,PH7_ContextThis(pCtx));` |
|     - |  1144 | `	ph7_value *apCall[2];` |
|     - |  1145 | `	ph7_value *apRec[2];` |
|     - |  1146 | `	VmNativeCall sRec;` |
|     - |  1147 | `	SyString sName;` |
|     - |  1148 | `	int rc;` |
|    62 |  1149 | `	if( pSlot == 0 ){` |
|   ! 0 |  1150 | `		return PH7_OK;` |
|     - |  1151 | `	}` |
|    62 |  1152 | `	SyStringInitFromBuf(&sName,zName,SyStrlen(zName));` |
|    62 |  1153 | `	sRec.pName = &sName;` |
|    62 |  1154 | `	sRec.pClass = 0;` |
|    62 |  1155 | `	sRec.bStatic = 0;` |
|    62 |  1156 | `	sRec.nLine = pVm->nCurLine;` |
|    62 |  1157 | `	sRec.pFrame = (void *)pVm->pFrame;` |
|    62 |  1158 | `	sRec.nIncDepth = SySetUsed(&pVm->aIncFrame);` |
|    62 |  1159 | `	sRec.bElided = 0;` |
|    62 |  1160 | `	sRec.bFrameless = 0;` |
|    62 |  1161 | `	sRec.pPrev = pVm->pNativeCall;` |
|    62 |  1162 | `	apRec[0] = pSlot;` |
|    62 |  1163 | `	apRec[1] = pRecArg;` |
|    62 |  1164 | `	sRec.apArg = apRec;` |
|    62 |  1165 | `	sRec.nArg = pRecArg ? 2 : 1;` |
|    62 |  1166 | `	apCall[0] = pSlot;` |
|    62 |  1167 | `	apCall[1] = pExtra;` |
|    62 |  1168 | `	pVm->pNativeCall = &sRec;` |
|    62 |  1169 | `	rc = xFunc(pCtx,pExtra ? 2 : 1,apCall);` |
|    62 |  1170 | `	pVm->pNativeCall = sRec.pPrev;` |
|    62 |  1171 | `	return rc;` |
|    33 |  1172 | `}` |
|    18 |  1173 | `static int SplFlagSort(ph7_context *pCtx,const char *zName,ProchHostFunction xFunc,` |
|     - |  1174 | `	int nArg,ph7_value **apArg)` |
|     2 |  1175 | `{` |
|     - |  1176 | `	ph7_value sZero;` |
|     - |  1177 | `	int rc;` |
|    20 |  1178 | `	if( nArg > 0 ){` |
|    12 |  1179 | `		return SplArraySortCall(pCtx,zName,xFunc,apArg[0],apArg[0]);` |
|     - |  1180 | `	}` |
|     9 |  1181 | `	PH7_MemObjInitFromInt(pCtx->pVm,&sZero,0);` |
|     9 |  1182 | `	rc = SplArraySortCall(pCtx,zName,xFunc,0,&sZero);` |
|     9 |  1183 | `	PH7_MemObjRelease(&sZero);` |
|     9 |  1184 | `	return rc;` |
|    11 |  1185 | `}` |
|    12 |  1186 | `static int vm_builtin_SplStore_asort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  1187 | `{` |
|    14 |  1188 | `	return SplFlagSort(pCtx,"asort",ph7_hashmap_asort,nArg,apArg);` |
|     2 |  1189 | `}` |
|     6 |  1190 | `static int vm_builtin_SplStore_ksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  1191 | `{` |
|     7 |  1192 | `	return SplFlagSort(pCtx,"ksort",ph7_hashmap_ksort,nArg,apArg);` |
|     1 |  1193 | `}` |
|    14 |  1194 | `static int vm_builtin_SplStore_uasort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  1195 | `{` |
|    17 |  1196 | `	ph7_value *pCb = nArg > 0 ? apArg[0] : 0;` |
|    17 |  1197 | `	return SplArraySortCall(pCtx,"uasort",ph7_hashmap_uasort,pCb,pCb);` |
|     3 |  1198 | `}` |
|    10 |  1199 | `static int vm_builtin_SplStore_uksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  1200 | `{` |
|    12 |  1201 | `	ph7_value *pCb = nArg > 0 ? apArg[0] : 0;` |
|    12 |  1202 | `	return SplArraySortCall(pCtx,"uksort",ph7_hashmap_uksort,pCb,pCb);` |
|     2 |  1203 | `}` |
|    16 |  1204 | `static int SplNatSort(ph7_context *pCtx,int bFold)` |
|     4 |  1205 | `{` |
|     - |  1206 | `	ph7_value sFlags;` |
|     - |  1207 | `	int rc;` |
|     - |  1208 | `	/* SORT_NATURAL (6), plus SORT_FLAG_CASE (8) for the folding twin — the same pair` |
|     - |  1209 | `	 * ph7_hashmap_natsort forwards to asort(). */` |
|    20 |  1210 | `	PH7_MemObjInitFromInt(pCtx->pVm,&sFlags,bFold ? (6\|8) : 6);` |
|    20 |  1211 | `	rc = SplArraySortCall(pCtx,bFold ? "natcasesort" : "natsort",ph7_hashmap_asort,&sFlags,0);` |
|    20 |  1212 | `	PH7_MemObjRelease(&sFlags);` |
|    20 |  1213 | `	return rc;` |
|     4 |  1214 | `}` |
|    10 |  1215 | `static int vm_builtin_SplStore_natsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     4 |  1216 | `{` |
|     5 |  1217 | `	SXUNUSED(nArg);` |
|     5 |  1218 | `	SXUNUSED(apArg);` |
|    14 |  1219 | `	return SplNatSort(pCtx,0);` |
|     4 |  1220 | `}` |
|     6 |  1221 | `static int vm_builtin_SplStore_natcasesort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  1222 | `{` |
|     3 |  1223 | `	SXUNUSED(nArg);` |
|     3 |  1224 | `	SXUNUSED(apArg);` |
|     9 |  1225 | `	return SplNatSort(pCtx,1);` |
|     3 |  1226 | `}` |
|     - |  1227 | `/* ArrayIterator's cursor: the stored array's own internal pointer, as the PHP had it. */` |
|  1014 |  1228 | `static int vm_builtin_ArrayIterator_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  1229 | `{` |
|  1017 |  1230 | `	ph7_hashmap *pMap = SplStore(pCtx->pVm,PH7_ContextThis(pCtx));` |
|   507 |  1231 | `	SXUNUSED(nArg);` |
|   507 |  1232 | `	SXUNUSED(apArg);` |
|  1017 |  1233 | `	if( pMap == 0 \|\| pMap->pCur == 0 ){` |
|     - |  1234 | `		/* Past the end php answers NULL, where current() the FUNCTION answers false. */` |
|     7 |  1235 | `		ph7_result_null(pCtx);` |
|     7 |  1236 | `		return PH7_OK;` |
|     - |  1237 | `	}` |
|  1011 |  1238 | `	return SplArrayCall(pCtx,ph7_hashmap_current,0);` |
|   510 |  1239 | `}` |
|   968 |  1240 | `static int vm_builtin_ArrayIterator_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  1241 | `{` |
|   484 |  1242 | `	SXUNUSED(nArg);` |
|   484 |  1243 | `	SXUNUSED(apArg);` |
|   971 |  1244 | `	return SplArrayCall(pCtx,ph7_hashmap_simple_key,0);` |
|     3 |  1245 | `}` |
|   916 |  1246 | `static int vm_builtin_ArrayIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  1247 | `{` |
|   458 |  1248 | `	SXUNUSED(nArg);` |
|   458 |  1249 | `	SXUNUSED(apArg);` |
|   919 |  1250 | `	SplArrayCall(pCtx,ph7_hashmap_next,0);` |
|   919 |  1251 | `	ph7_result_null(pCtx); /* next() the METHOD returns void */` |
|   919 |  1252 | `	return PH7_OK;` |
|     3 |  1253 | `}` |
|   596 |  1254 | `static int vm_builtin_ArrayIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  1255 | `{` |
|   298 |  1256 | `	SXUNUSED(nArg);` |
|   298 |  1257 | `	SXUNUSED(apArg);` |
|   599 |  1258 | `	SplArrayCall(pCtx,ph7_hashmap_reset,0);` |
|   599 |  1259 | `	ph7_result_null(pCtx);` |
|   599 |  1260 | `	return PH7_OK;` |
|     3 |  1261 | `}` |
|  1990 |  1262 | `static int vm_builtin_ArrayIterator_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  1263 | `{` |
|  1993 |  1264 | `	ph7_hashmap *pMap = SplStore(pCtx->pVm,PH7_ContextThis(pCtx));` |
|   995 |  1265 | `	SXUNUSED(nArg);` |
|   995 |  1266 | `	SXUNUSED(apArg);` |
|  1993 |  1267 | `	ph7_result_bool(pCtx,pMap && pMap->pCur ? 1 : 0);` |
|  1993 |  1268 | `	return PH7_OK;` |
|     3 |  1269 | `}` |
|    34 |  1270 | `static int vm_builtin_ArrayIterator_seek(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  1271 | `{` |
|    35 |  1272 | `	ph7_hashmap *pMap = SplStore(pCtx->pVm,PH7_ContextThis(pCtx));` |
|    35 |  1273 | `	ph7_int64 iOffset = nArg > 0 ? ph7_value_to_int(apArg[0]) : 0;` |
|     - |  1274 | `	ph7_int64 i;` |
|    35 |  1275 | `	if( pMap == 0 ){` |
|   ! 0 |  1276 | `		return PH7_OK;` |
|     - |  1277 | `	}` |
|    35 |  1278 | `	if( iOffset < 0 \|\| iOffset >= (ph7_int64)pMap->nEntry ){` |
|    10 |  1279 | `		return PH7_VmThrowException(pCtx,"OutOfBoundsException",` |
|     3 |  1280 | `			"Seek position %qd is out of range",iOffset);` |
|     - |  1281 | `	}` |
|    29 |  1282 | `	pMap->pCur = pMap->pFirst;` |
|    65 |  1283 | `	for( i = 0 ; i < iOffset && pMap->pCur ; ++i ){` |
|    37 |  1284 | `		pMap->pCur = pMap->pCur->pPrev; /* insertion order: pFirst, then the pPrev chain */` |
|    19 |  1285 | `	}` |
|    29 |  1286 | `	return PH7_OK;` |
|    18 |  1287 | `}` |
|  1106 |  1288 | `static int vm_builtin_ArrayIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 |  1289 | `{` |
|  1111 |  1290 | `	ph7_vm *pVm = pCtx->pVm;` |
|  1111 |  1291 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  1292 | `	ph7_hashmap *pMap;` |
|  1111 |  1293 | `	sxi32 rc = SplInitStore(pCtx,pThis,nArg > 0 ? apArg[0] : 0,` |
|     - |  1294 | `		"ArrayIterator::__construct");` |
|  1111 |  1295 | `	if( rc != PH7_OK ){` |
|    11 |  1296 | `		return rc;` |
|     - |  1297 | `	}` |
|  1101 |  1298 | `	if( pThis && nArg > 1 ){` |
|   170 |  1299 | `		PH7_NativeSetAttrInt(pVm,pThis,SPL_F,ph7_value_to_int64(apArg[1]) & SPL_FLAG_MASK);` |
|    84 |  1300 | `	}` |
|  1101 |  1301 | `	pMap = SplStore(pVm,pThis);` |
|  1101 |  1302 | `	if( pMap ){` |
|  1101 |  1303 | `		pMap->pCur = pMap->pFirst; /* reset($this->__d) */` |
|   548 |  1304 | `	}` |
|  1101 |  1305 | `	return PH7_OK;` |
|   558 |  1306 | `}` |
|     - |  1307 | `/* ArrayObject */` |
|     - |  1308 | `/*` |
|     - |  1309 | `` * The iterator class, taken by two doors: `setIteratorClass()` and the`` |
|     - |  1310 | ` * constructor's third argument. php refuses the same names at both but words` |
|     - |  1311 | ` * the refusal from the door it came in by, so the method NAME and the argument` |
|     - |  1312 | ` * POSITION travel with the check -- PHL reported the constructor's refusal as` |
|     - |  1313 | ` * setIteratorClass()'s Argument #1.` |
|     - |  1314 | ` *` |
|     - |  1315 | ` * The cast is php's user-visible one (zend's class-name parameter): an array` |
|     - |  1316 | `` * warns `Array to string conversion` and is refused as the name "Array", and an`` |
|     - |  1317 | `` * object with no __toString() is the catchable `could not be converted to`` |
|     - |  1318 | `` * string` Error rather than a refusal naming the placeholder "Object".`` |
|     - |  1319 | ` */` |
|    42 |  1320 | `static int SplSetIteratorClass(ph7_context *pCtx,ph7_value *pArg,const char *zWhere)` |
|     1 |  1321 | `{` |
|    43 |  1322 | `	ph7_vm *pVm = pCtx->pVm;` |
|    43 |  1323 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  1324 | `	const char *zName;` |
|     - |  1325 | `	int nName;` |
|     - |  1326 | `	sxi32 rcSv;` |
|    43 |  1327 | `	if( pThis == 0 \|\| pArg == 0 ){` |
|   ! 0 |  1328 | `		return PH7_OK;` |
|     - |  1329 | `	}` |
|    43 |  1330 | `	rcSv = PH7_ValueToStringUV(pCtx,pArg,&zName,&nName);` |
|    43 |  1331 | `	if( rcSv != SXRET_OK ){` |
|     5 |  1332 | `		return rcSv;` |
|     - |  1333 | `	}` |
|    38 |  1334 | `	if( nName != (int)sizeof("ArrayIterator")-1` |
|    20 |  1335 | `	 \|\| SyMemcmp(zName,"ArrayIterator",sizeof("ArrayIterator")-1) != 0 ){` |
|    39 |  1336 | `		ph7_class *pClass = PH7_VmExtractClass(pVm,zName,(sxu32)nName,FALSE,0);` |
|    39 |  1337 | `		ph7_class *pBase = PH7_VmExtractClass(pVm,"ArrayIterator",` |
|     - |  1338 | `			sizeof("ArrayIterator")-1,FALSE,0);` |
|    38 |  1339 | `		if( pClass == 0 \|\| pBase == 0 \|\| pClass == pBase` |
|    25 |  1340 | `		 \|\| !PH7_VmInstanceOf(pClass,pBase) ){` |
|    43 |  1341 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|     - |  1342 | `				"%s must be a class name derived from ArrayIterator, %.*s given",` |
|    14 |  1343 | `				zWhere,nName,zName);` |
|     - |  1344 | `		}` |
|     5 |  1345 | `	}` |
|    11 |  1346 | `	PH7_NativeSetAttrStr(pVm,pThis,SPL_IT,zName,nName);` |
|    11 |  1347 | `	return PH7_OK;` |
|    22 |  1348 | `}` |
|    24 |  1349 | `static int vm_builtin_ArrayObject_setIteratorClass(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  1350 | `{` |
|    25 |  1351 | `	return SplSetIteratorClass(pCtx,nArg > 0 ? apArg[0] : 0,` |
|     - |  1352 | `		"ArrayObject::setIteratorClass(): Argument #1 ($iteratorClass)");` |
|     1 |  1353 | `}` |
|   432 |  1354 | `static int vm_builtin_ArrayObject_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 |  1355 | `{` |
|   437 |  1356 | `	ph7_vm *pVm = pCtx->pVm;` |
|   437 |  1357 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   437 |  1358 | `	sxi32 rc = SplInitStore(pCtx,pThis,nArg > 0 ? apArg[0] : 0,` |
|     - |  1359 | `		"ArrayObject::__construct");` |
|   437 |  1360 | `	if( rc != PH7_OK ){` |
|     3 |  1361 | `		return rc;` |
|     - |  1362 | `	}` |
|   435 |  1363 | `	if( pThis && nArg > 1 ){` |
|    42 |  1364 | `		PH7_NativeSetAttrInt(pVm,pThis,SPL_F,ph7_value_to_int64(apArg[1]) & SPL_FLAG_MASK);` |
|    20 |  1365 | `	}` |
|   435 |  1366 | `	if( nArg > 2 ){` |
|    19 |  1367 | `		return SplSetIteratorClass(pCtx,apArg[2],` |
|     - |  1368 | `			"ArrayObject::__construct(): Argument #3 ($iteratorClass)");` |
|     - |  1369 | `	}` |
|   417 |  1370 | `	return PH7_OK;` |
|   221 |  1371 | `}` |
|     4 |  1372 | `static int vm_builtin_ArrayObject_exchangeArray(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  1373 | `{` |
|     5 |  1374 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     5 |  1375 | `	ph7_value *pSlot = pThis ? PH7_NativeAttr(pThis,SPL_D) : 0;` |
|     - |  1376 | `	sxi32 rc;` |
|     5 |  1377 | `	if( pSlot ){` |
|     5 |  1378 | `		ph7_result_value(pCtx,pSlot); /* the OLD store is the return value */` |
|     2 |  1379 | `	}` |
|     5 |  1380 | `	rc = SplInitStore(pCtx,pThis,nArg > 0 ? apArg[0] : 0,"ArrayObject::exchangeArray");` |
|     5 |  1381 | `	return rc;` |
|     1 |  1382 | `}` |
|    10 |  1383 | `static int vm_builtin_ArrayObject_getIteratorClass(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  1384 | `{` |
|    11 |  1385 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    11 |  1386 | `	const char *zName = 0;` |
|    11 |  1387 | `	int nName = 0;` |
|     5 |  1388 | `	SXUNUSED(nArg);` |
|     5 |  1389 | `	SXUNUSED(apArg);` |
|    11 |  1390 | `	if( pThis ){` |
|    11 |  1391 | `		PH7_NativeAttrStr(pThis,SPL_IT,&zName,&nName);` |
|     5 |  1392 | `	}` |
|    11 |  1393 | `	ph7_result_string(pCtx,nName > 0 ? zName : "ArrayIterator",nName > 0 ? nName : -1);` |
|    11 |  1394 | `	return PH7_OK;` |
|     1 |  1395 | `}` |
|     - |  1396 | `/*` |
|     - |  1397 | ` * ---------------------------------------------------------------------------` |
|     - |  1398 | ` * php's __serialize()/__unserialize() for the array store.` |
|     - |  1399 | ` *` |
|     - |  1400 | ` * php's payload is a four-element LIST -- [flags, storage, members, iterator class]` |
|     - |  1401 | ` * -- and nothing else can express it: the state lives in ext/spl's own struct, so` |
|     - |  1402 | ` * there are no properties to walk. PHL walked its HIDDEN slots instead and wrote` |
|     - |  1403 | `` * `O:11:"ArrayObject":3:{s:16:"\0ArrayObject\0__d";…}`, which round-tripped inside`` |
|     - |  1404 | ` * PHL and could not read a byte string php produced (nor be read by php).` |
|     - |  1405 | ` *` |
|     - |  1406 | ` * Two details worth keeping. The last element is NULL when the class is the default` |
|     - |  1407 | ` * ArrayIterator, and it is always NULL for an ArrayIterator payload -- php shares one` |
|     - |  1408 | ` * C body between both classes, so ArrayIterator carries the slot it has no use for.` |
|     - |  1409 | ` * And the iterator-class check here is LOOSER than setIteratorClass()'s: restoring` |
|     - |  1410 | `` * accepts any `Iterator`, while the setter and the constructor demand a class derived`` |
|     - |  1411 | `` * from ArrayIterator. php words the refusal with `ArrayObject` either way, even when`` |
|     - |  1412 | ` * ArrayIterator is the receiver.` |
|     - |  1413 | ` * ---------------------------------------------------------------------------` |
|     - |  1414 | ` */` |
|     8 |  1415 | `static sxi32 SplStoreIllTyped(ph7_context *pCtx)` |
|     1 |  1416 | `{` |
|     9 |  1417 | `	return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     - |  1418 | `		"Incomplete or ill-typed serialization data");` |
|     1 |  1419 | `}` |
|    26 |  1420 | `static int vm_builtin_SplStore_serializeMagic(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  1421 | `{` |
|    27 |  1422 | `	ph7_vm *pVm = pCtx->pVm;` |
|    27 |  1423 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  1424 | `	ph7_value sOut,sVal,*pStore;` |
|    27 |  1425 | `	const char *zIt = 0;` |
|    27 |  1426 | `	int nIt = 0;` |
|    13 |  1427 | `	SXUNUSED(nArg);` |
|    13 |  1428 | `	SXUNUSED(apArg);` |
|    27 |  1429 | `	PH7_MemObjInit(pVm,&sOut);` |
|    27 |  1430 | `	if( PH7_MemObjToHashmap(&sOut) != SXRET_OK ){` |
|   ! 0 |  1431 | `		PH7_MemObjRelease(&sOut);` |
|   ! 0 |  1432 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  1433 | `	}` |
|     - |  1434 | `	/* [0] the flags word */` |
|    27 |  1435 | `	PH7_MemObjInitFromInt(pVm,&sVal,pThis ? PH7_NativeAttrInt(pThis,SPL_F) : 0);` |
|    27 |  1436 | `	ph7_array_add_elem(&sOut,0,&sVal);` |
|    27 |  1437 | `	PH7_MemObjRelease(&sVal);` |
|     - |  1438 | `	/* [1] the storage */` |
|    27 |  1439 | `	pStore = SplStoreSlot(pVm,pThis);` |
|    27 |  1440 | `	if( pStore ){` |
|    27 |  1441 | `		ph7_array_add_elem(&sOut,0,pStore);` |
|    13 |  1442 | `	}` |
|     - |  1443 | `	/* [2] the instance's own properties */` |
|    27 |  1444 | `	if( SplMembersOf(pVm,pThis,&sVal) != SXRET_OK ){` |
|   ! 0 |  1445 | `		PH7_MemObjRelease(&sOut);` |
|   ! 0 |  1446 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  1447 | `	}` |
|    27 |  1448 | `	ph7_array_add_elem(&sOut,0,&sVal);` |
|    27 |  1449 | `	PH7_MemObjRelease(&sVal);` |
|     - |  1450 | `	/* [3] the iterator class, NULL for the default one and for ArrayIterator */` |
|    27 |  1451 | `	if( pThis ){` |
|    27 |  1452 | `		PH7_NativeAttrStr(pThis,SPL_IT,&zIt,&nIt);` |
|    13 |  1453 | `	}` |
|    27 |  1454 | `	PH7_MemObjInit(pVm,&sVal);` |
|    27 |  1455 | `	if( nIt > 0 && (nIt != (int)sizeof("ArrayIterator")-1` |
|    14 |  1456 | `	 \|\| SyMemcmp(zIt,"ArrayIterator",sizeof("ArrayIterator")-1) != 0) ){` |
|     5 |  1457 | `		PH7_MemObjStringAppend(&sVal,zIt,(sxu32)nIt);` |
|     2 |  1458 | `	}` |
|    27 |  1459 | `	ph7_array_add_elem(&sOut,0,&sVal);` |
|    27 |  1460 | `	PH7_MemObjRelease(&sVal);` |
|    27 |  1461 | `	ph7_result_value(pCtx,&sOut);` |
|    27 |  1462 | `	PH7_MemObjRelease(&sOut);` |
|    27 |  1463 | `	return PH7_OK;` |
|    14 |  1464 | `}` |
|     - |  1465 | `static void SplSerializeInto(ph7_context *pCtx,ph7_value **apCall,SyBlob *pOut);` |
|     - |  1466 | `/*` |
|     - |  1467 | ` * php's Serializable pair for the array store -- the LEGACY byte format the` |
|     - |  1468 | `` * magic pair above replaced, which php still declares (`ArrayObject implements`` |
|     - |  1469 | `` * ... Serializable`) and still answers. Neither name existed here, so the`` |
|     - |  1470 | `` * interface could not be declared either: `$ao instanceof Serializable` was`` |
|     - |  1471 | `` * false and `$ao->serialize()` a `Call to undefined method`.`` |
|     - |  1472 | ` *` |
|     - |  1473 | `` * The string is `x:<flags><storage>;m:<members>`, each part php's own`` |
|     - |  1474 | ` * serialize() output -- so a payload written here reads back in php and the` |
|     - |  1475 | ` * other way round. php's reader is a hand-rolled walk and its refusal reports` |
|     - |  1476 | ` * WHERE it gave up, which is why the offsets below are spelled out one by one:` |
|     - |  1477 | ` * a value it could not read at all blames the position it started from, while` |
|     - |  1478 | ` * one it read and then rejected for its TYPE blames the position after it.` |
|     - |  1479 | ` * The storage is screened by its type BYTE before the read, so a well-formed` |
|     - |  1480 | `` * `i:5;` there is refused at the byte rather than after it.`` |
|     - |  1481 | ` */` |
|     8 |  1482 | `static int vm_builtin_SplStore_serialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  1483 | `{` |
|     9 |  1484 | `	ph7_vm *pVm = pCtx->pVm;` |
|     9 |  1485 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  1486 | `	ph7_value sVal,*pStore,*apCall[1];` |
|     - |  1487 | `	SyBlob sOut;` |
|     4 |  1488 | `	SXUNUSED(nArg);` |
|     4 |  1489 | `	SXUNUSED(apArg);` |
|     9 |  1490 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|     9 |  1491 | `	SyBlobAppend(&sOut,"x:",sizeof("x:")-1);` |
|     9 |  1492 | `	PH7_MemObjInitFromInt(pVm,&sVal,pThis ? PH7_NativeAttrInt(pThis,SPL_F) : 0);` |
|     9 |  1493 | `	apCall[0] = &sVal;` |
|     9 |  1494 | `	SplSerializeInto(pCtx,apCall,&sOut);` |
|     9 |  1495 | `	PH7_MemObjRelease(&sVal);` |
|     9 |  1496 | `	pStore = SplStoreSlot(pVm,pThis);` |
|     9 |  1497 | `	if( pStore ){` |
|     9 |  1498 | `		apCall[0] = pStore;` |
|     9 |  1499 | `		SplSerializeInto(pCtx,apCall,&sOut);` |
|     4 |  1500 | `	}` |
|     9 |  1501 | `	SyBlobAppend(&sOut,";m:",sizeof(";m:")-1);` |
|     9 |  1502 | `	if( SplMembersOf(pVm,pThis,&sVal) == SXRET_OK ){` |
|     9 |  1503 | `		apCall[0] = &sVal;` |
|     9 |  1504 | `		SplSerializeInto(pCtx,apCall,&sOut);` |
|     9 |  1505 | `		PH7_MemObjRelease(&sVal);` |
|     4 |  1506 | `	}` |
|     - |  1507 | `	/* ph7_result_string APPENDS, and pRet still holds the last nested answer. */` |
|     9 |  1508 | `	if( pCtx->pRet ){` |
|     9 |  1509 | `		PH7_MemObjRelease(pCtx->pRet);` |
|     4 |  1510 | `	}` |
|     9 |  1511 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|     9 |  1512 | `	SyBlobRelease(&sOut);` |
|     9 |  1513 | `	return PH7_OK;` |
|     1 |  1514 | `}` |
|     - |  1515 | `/* php reports WHERE its walk gave up, in bytes. */` |
|    30 |  1516 | `static sxi32 SplStoreOffsetErr(ph7_context *pCtx,int nAt,int nTotal)` |
|     1 |  1517 | `{` |
|    46 |  1518 | `	return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|    15 |  1519 | `		"Error at offset %d of %d bytes",nAt,nTotal);` |
|     1 |  1520 | `}` |
|    36 |  1521 | `static int vm_builtin_SplStore_unserialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  1522 | `{` |
|    37 |  1523 | `	ph7_vm *pVm = pCtx->pVm;` |
|    37 |  1524 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  1525 | `	const char *zData;` |
|    37 |  1526 | `	int nData = 0,nAt,nRead = 0;` |
|     - |  1527 | `	ph7_value sFlags,sStore,sMembers;` |
|     - |  1528 | `	sxi32 rc;` |
|    37 |  1529 | `	if( nArg < 1 \|\| pThis == 0 ){` |
|   ! 0 |  1530 | `		return PH7_OK;` |
|     - |  1531 | `	}` |
|    37 |  1532 | `	zData = ph7_value_to_string(apArg[0],&nData);` |
|    37 |  1533 | `	if( nData < 1 ){` |
|     3 |  1534 | `		return PH7_OK;   /* php returns without touching the store */` |
|     - |  1535 | `	}` |
|    35 |  1536 | `	if( zData[0] != 'x' ){` |
|     3 |  1537 | `		return SplStoreOffsetErr(pCtx,0,nData);` |
|     - |  1538 | `	}` |
|    33 |  1539 | `	if( nData < 2 \|\| zData[1] != ':' ){` |
|     5 |  1540 | `		return SplStoreOffsetErr(pCtx,1,nData);` |
|     - |  1541 | `	}` |
|    29 |  1542 | `	nAt = 2;` |
|    29 |  1543 | `	PH7_MemObjInit(pVm,&sFlags);` |
|    29 |  1544 | `	PH7_MemObjInit(pVm,&sStore);` |
|    29 |  1545 | `	PH7_MemObjInit(pVm,&sMembers);` |
|    29 |  1546 | `	rc = PH7_VmUnserializeOne(pCtx,&zData[nAt],nData - nAt,&nRead,&sFlags);` |
|    29 |  1547 | `	if( rc == PH7_EXCEPTION ){` |
|   ! 0 |  1548 | `		goto Done;` |
|     - |  1549 | `	}` |
|    29 |  1550 | `	if( rc != SXRET_OK ){` |
|     5 |  1551 | `		rc = SplStoreOffsetErr(pCtx,nAt,nData);` |
|     5 |  1552 | `		goto Done;` |
|     - |  1553 | `	}` |
|    25 |  1554 | `	nAt += nRead;` |
|    25 |  1555 | `	if( (sFlags.iFlags & MEMOBJ_INT) == 0 ){` |
|     3 |  1556 | `		rc = SplStoreOffsetErr(pCtx,nAt,nData);` |
|     3 |  1557 | `		goto Done;` |
|     - |  1558 | `	}` |
|     - |  1559 | `	/* The storage's type BYTE decides before the read: php accepts an array,` |
|     - |  1560 | `	 * an object of any of its three spellings, or a back-reference. */` |
|    26 |  1561 | `	if( nAt >= nData \|\| (zData[nAt] != 'a' && zData[nAt] != 'O'` |
|     6 |  1562 | `	 && zData[nAt] != 'C' && zData[nAt] != 'r') ){` |
|     9 |  1563 | `		rc = SplStoreOffsetErr(pCtx,nAt,nData);` |
|     9 |  1564 | `		goto Done;` |
|     - |  1565 | `	}` |
|    15 |  1566 | `	rc = PH7_VmUnserializeOne(pCtx,&zData[nAt],nData - nAt,&nRead,&sStore);` |
|    15 |  1567 | `	if( rc == PH7_EXCEPTION ){` |
|   ! 0 |  1568 | `		goto Done;` |
|     - |  1569 | `	}` |
|    15 |  1570 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  1571 | `		rc = SplStoreOffsetErr(pCtx,nAt,nData);` |
|   ! 0 |  1572 | `		goto Done;` |
|     - |  1573 | `	}` |
|    15 |  1574 | `	nAt += nRead;` |
|    15 |  1575 | `	if( nAt >= nData \|\| zData[nAt] != ';' ){` |
|     3 |  1576 | `		rc = SplStoreOffsetErr(pCtx,nAt,nData);` |
|     3 |  1577 | `		goto Done;` |
|     - |  1578 | `	}` |
|    13 |  1579 | `	nAt++;` |
|    13 |  1580 | `	if( nAt >= nData \|\| zData[nAt] != 'm' ){` |
|     3 |  1581 | `		rc = SplStoreOffsetErr(pCtx,nAt,nData);` |
|     3 |  1582 | `		goto Done;` |
|     - |  1583 | `	}` |
|    11 |  1584 | `	nAt++;` |
|    11 |  1585 | `	if( nAt >= nData \|\| zData[nAt] != ':' ){` |
|     3 |  1586 | `		rc = SplStoreOffsetErr(pCtx,nAt,nData);` |
|     3 |  1587 | `		goto Done;` |
|     - |  1588 | `	}` |
|     9 |  1589 | `	nAt++;` |
|     9 |  1590 | `	rc = PH7_VmUnserializeOne(pCtx,&zData[nAt],nData - nAt,&nRead,&sMembers);` |
|     9 |  1591 | `	if( rc == PH7_EXCEPTION ){` |
|   ! 0 |  1592 | `		goto Done;` |
|     - |  1593 | `	}` |
|     9 |  1594 | `	if( rc != SXRET_OK ){` |
|     3 |  1595 | `		rc = SplStoreOffsetErr(pCtx,nAt,nData);` |
|     3 |  1596 | `		goto Done;` |
|     - |  1597 | `	}` |
|     7 |  1598 | `	nAt += nRead;` |
|     7 |  1599 | `	if( (sMembers.iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     3 |  1600 | `		rc = SplStoreOffsetErr(pCtx,nAt,nData);` |
|     3 |  1601 | `		goto Done;` |
|     - |  1602 | `	}` |
|     5 |  1603 | `	PH7_NativeSetAttrInt(pVm,pThis,SPL_F,ph7_value_to_int64(&sFlags) & SPL_FLAG_MASK);` |
|     5 |  1604 | `	rc = SplInitStore(pCtx,pThis,&sStore,"ArrayObject::unserialize");` |
|     7 |  1605 | `	if( rc == SXRET_OK ){` |
|     5 |  1606 | `		SplMembersLoad(pThis,&sMembers);` |
|     5 |  1607 | `		rc = PH7_OK;` |
|     2 |  1608 | `	}` |
|   ! 0 |  1609 | `Done:` |
|    29 |  1610 | `	PH7_MemObjRelease(&sFlags);` |
|    29 |  1611 | `	PH7_MemObjRelease(&sStore);` |
|    29 |  1612 | `	PH7_MemObjRelease(&sMembers);` |
|    29 |  1613 | `	return rc;` |
|    19 |  1614 | `}` |
|    38 |  1615 | `static int vm_builtin_SplStore_unserializeMagic(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  1616 | `{` |
|    39 |  1617 | `	ph7_vm *pVm = pCtx->pVm;` |
|    39 |  1618 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  1619 | `	ph7_hashmap *pData;` |
|    39 |  1620 | `	ph7_hashmap_node *pNode = 0;` |
|    39 |  1621 | `	ph7_value *pFlags,*pStorage,*pMembers,*pIt = 0;` |
|     - |  1622 | `	sxi32 rc;` |
|    39 |  1623 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     - |  1624 | `		char zBuf[64];` |
|   ! 0 |  1625 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - |  1626 | `			"%s(): Argument #1 ($data) must be of type array, %s given",` |
|   ! 0 |  1627 | `			ph7_function_name(pCtx),` |
|   ! 0 |  1628 | `			nArg > 0 ? VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)) : "none");` |
|     - |  1629 | `	}` |
|    39 |  1630 | `	if( pThis == 0 ){` |
|   ! 0 |  1631 | `		return PH7_OK;` |
|     - |  1632 | `	}` |
|    39 |  1633 | `	pData = (ph7_hashmap *)apArg[0]->x.pOther;` |
|    39 |  1634 | `	if( HashmapLookupIntKey(pData,0,&pNode) != SXRET_OK ){` |
|   ! 0 |  1635 | `		return SplStoreIllTyped(pCtx);` |
|     - |  1636 | `	}` |
|    39 |  1637 | `	pFlags = HashmapExtractNodeValue(pNode);` |
|    39 |  1638 | `	pNode = 0;` |
|    39 |  1639 | `	if( HashmapLookupIntKey(pData,1,&pNode) != SXRET_OK ){` |
|   ! 0 |  1640 | `		return SplStoreIllTyped(pCtx);` |
|     - |  1641 | `	}` |
|    39 |  1642 | `	pStorage = HashmapExtractNodeValue(pNode);` |
|    39 |  1643 | `	pNode = 0;` |
|    39 |  1644 | `	if( HashmapLookupIntKey(pData,2,&pNode) != SXRET_OK ){` |
|     3 |  1645 | `		return SplStoreIllTyped(pCtx);` |
|     - |  1646 | `	}` |
|    37 |  1647 | `	pMembers = HashmapExtractNodeValue(pNode);` |
|    37 |  1648 | `	pNode = 0;` |
|    37 |  1649 | `	if( HashmapLookupIntKey(pData,3,&pNode) == SXRET_OK ){` |
|    29 |  1650 | `		pIt = HashmapExtractNodeValue(pNode);` |
|    14 |  1651 | `	}` |
|    36 |  1652 | `	if( pFlags == 0 \|\| (pFlags->iFlags & MEMOBJ_INT) == 0` |
|    35 |  1653 | `	 \|\| pMembers == 0 \|\| (pMembers->iFlags & MEMOBJ_HASHMAP) == 0` |
|    34 |  1654 | `	 \|\| (pIt != 0 && (pIt->iFlags & (MEMOBJ_NULL\|MEMOBJ_STRING)) == 0) ){` |
|     7 |  1655 | `		return SplStoreIllTyped(pCtx);` |
|     - |  1656 | `	}` |
|     - |  1657 | `	/* php's own wording, and its own exception CLASS, for the storage slot. */` |
|    31 |  1658 | `	if( pStorage == 0 \|\| (pStorage->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 ){` |
|     3 |  1659 | `		return PH7_VmThrowException(pCtx,"InvalidArgumentException",` |
|     - |  1660 | `			"Passed variable is not an array or object");` |
|     - |  1661 | `	}` |
|    29 |  1662 | `	if( pIt != 0 && (pIt->iFlags & MEMOBJ_STRING) != 0 && SyBlobLength(&pIt->sBlob) > 0 ){` |
|    11 |  1663 | `		const char *zIt = (const char *)SyBlobData(&pIt->sBlob);` |
|    11 |  1664 | `		int nIt = (int)SyBlobLength(&pIt->sBlob);` |
|    11 |  1665 | `		ph7_class *pClass = PH7_VmExtractClass(pVm,zIt,(sxu32)nIt,FALSE,0);` |
|    11 |  1666 | `		ph7_class *pIface = PH7_VmExtractClass(pVm,"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|    11 |  1667 | `		if( pClass == 0 ){` |
|     4 |  1668 | `			return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     - |  1669 | `				"Cannot deserialize ArrayObject with iterator class '%.*s'; "` |
|     1 |  1670 | `				"no such class exists",nIt,zIt);` |
|     - |  1671 | `		}` |
|     9 |  1672 | `		if( pIface == 0 \|\| !PH7_VmInstanceOf(pClass,pIface) ){` |
|     7 |  1673 | `			return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     - |  1674 | `				"Cannot deserialize ArrayObject with iterator class '%.*s'; "` |
|     2 |  1675 | `				"this class does not implement the Iterator interface",nIt,zIt);` |
|     - |  1676 | `		}` |
|     5 |  1677 | `		if( PH7_NativeAttr(pThis,SPL_IT) ){` |
|     5 |  1678 | `			PH7_NativeSetAttrStr(pVm,pThis,SPL_IT,zIt,nIt);` |
|     2 |  1679 | `		}` |
|     2 |  1680 | `	}` |
|    23 |  1681 | `	PH7_NativeSetAttrInt(pVm,pThis,SPL_F,ph7_value_to_int64(pFlags) & SPL_FLAG_MASK);` |
|    23 |  1682 | `	rc = SplInitStore(pCtx,pThis,pStorage,"ArrayObject::__unserialize");` |
|    23 |  1683 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  1684 | `		return rc;` |
|     - |  1685 | `	}` |
|    23 |  1686 | `	SplMembersLoad(pThis,pMembers);` |
|    23 |  1687 | `	return PH7_OK;` |
|    20 |  1688 | `}` |
|     - |  1689 | `/*` |
|     - |  1690 | ` * ---------------------------------------------------------------------------` |
|     - |  1691 | ` * php's presentation for the array store (ph7_class::xPresent).` |
|     - |  1692 | ` *` |
|     - |  1693 | ` * php has two handlers here and they DISAGREE, which is the whole reason the hook` |
|     - |  1694 | `` * is told which is asking. `spl_array_get_debug_info` always shows ONE entry —`` |
|     - |  1695 | ` * the storage under its MANGLED private name — after whatever real properties the` |
|     - |  1696 | `` * instance has; `spl_array_get_properties_for` answers the storage's ELEMENTS`` |
|     - |  1697 | `` * directly for the var_export / (array) / json purposes, with no `storage` key at`` |
|     - |  1698 | ` * all, and hands back the ordinary property table when STD_PROP_LIST is set. The` |
|     - |  1699 | ` * flag is therefore visible on one surface and invisible on the other: a` |
|     - |  1700 | ` * STD_PROP_LIST ArrayObject still var_dumps its storage.` |
|     - |  1701 | ` *` |
|     - |  1702 | ` * The mangled name always spells the ROOT class, never the receiver's: a` |
|     - |  1703 | `` * RecursiveArrayIterator shows `["storage":"ArrayIterator":private]`.`` |
|     - |  1704 | ` * ---------------------------------------------------------------------------` |
|     - |  1705 | ` */` |
|    14 |  1706 | `static int SplStoreMangledKey(ph7_class_instance *pThis,char *zBuf,int nBuf)` |
|     1 |  1707 | `{` |
|    15 |  1708 | `	const char *zRoot = "ArrayObject";` |
|     - |  1709 | `	ph7_class *pClass;` |
|    15 |  1710 | `	int nRoot,nOut = 0;` |
|    27 |  1711 | `	for( pClass = pThis->pClass ; pClass ; pClass = pClass->pBase ){` |
|    18 |  1712 | `		if( pClass->sName.nByte == sizeof("ArrayIterator")-1` |
|    13 |  1713 | `		 && SyMemcmp(pClass->sName.zString,"ArrayIterator",sizeof("ArrayIterator")-1) == 0 ){` |
|     7 |  1714 | `			zRoot = "ArrayIterator";` |
|     7 |  1715 | `			break;` |
|     - |  1716 | `		}` |
|     7 |  1717 | `	}` |
|    15 |  1718 | `	nRoot = (int)SyStrlen(zRoot);` |
|    15 |  1719 | `	if( nRoot + (int)sizeof("\0\0storage") > nBuf ){` |
|   ! 0 |  1720 | `		return 0;` |
|     - |  1721 | `	}` |
|    15 |  1722 | `	zBuf[nOut++] = 0;` |
|    15 |  1723 | `	SyMemcpy(zRoot,&zBuf[nOut],(sxu32)nRoot);` |
|    15 |  1724 | `	nOut += nRoot;` |
|    15 |  1725 | `	zBuf[nOut++] = 0;` |
|    15 |  1726 | `	SyMemcpy("storage",&zBuf[nOut],sizeof("storage")-1);` |
|    15 |  1727 | `	nOut += (int)sizeof("storage")-1;` |
|    15 |  1728 | `	return nOut;` |
|     8 |  1729 | `}` |
|    24 |  1730 | `static int SplPresentWalk(ph7_value *pKey,ph7_value *pVal,void *pUserData)` |
|     2 |  1731 | `{` |
|    26 |  1732 | `	ph7_array_add_elem((ph7_value *)pUserData,pKey,pVal);` |
|    26 |  1733 | `	return PH7_OK;` |
|     2 |  1734 | `}` |
|    38 |  1735 | `static sxi32 SplStorePresent(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)` |
|     2 |  1736 | `{` |
|    40 |  1737 | `	ph7_value *pStore = pThis ? PH7_NativeAttr(pThis,SPL_D) : 0;` |
|    40 |  1738 | `	if( bDebug ){` |
|     - |  1739 | `		ph7_value sKey;` |
|     - |  1740 | `		char zKey[64];` |
|     - |  1741 | `		int nKey;` |
|     - |  1742 | `		/* The instance's OWN properties come first — php's debug info starts from` |
|     - |  1743 | `		 * the standard table and appends the storage entry to it. */` |
|    15 |  1744 | `		SplAddMembers(&(*pVm),pThis,pOut);` |
|    15 |  1745 | `		nKey = SplStoreMangledKey(pThis,zKey,(int)sizeof(zKey));` |
|    15 |  1746 | `		if( nKey > 0 && pStore ){` |
|    15 |  1747 | `			PH7_MemObjInitFromString(&(*pVm),&sKey,0);` |
|    15 |  1748 | `			PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKey);` |
|    15 |  1749 | `			ph7_array_add_elem(pOut,&sKey,pStore);` |
|    15 |  1750 | `			PH7_MemObjRelease(&sKey);` |
|     7 |  1751 | `		}` |
|    15 |  1752 | `		return SXRET_OK;` |
|     - |  1753 | `	}` |
|    26 |  1754 | `	if( (PH7_NativeAttrInt(pThis,SPL_F) & SPL_STD_PROP_LIST) != 0 ){` |
|     5 |  1755 | `		SplAddMembers(&(*pVm),pThis,pOut);` |
|     5 |  1756 | `		return SXRET_OK;` |
|     - |  1757 | `	}` |
|    22 |  1758 | `	if( pStore && (pStore->iFlags & MEMOBJ_HASHMAP) != 0 ){` |
|    22 |  1759 | `		ph7_array_walk(pStore,SplPresentWalk,pOut);` |
|    10 |  1760 | `	}` |
|    22 |  1761 | `	return SXRET_OK;` |
|    21 |  1762 | `}` |
|    18 |  1763 | `static int vm_builtin_ArrayObject_getIterator(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  1764 | `{` |
|    20 |  1765 | `	ph7_vm *pVm = pCtx->pVm;` |
|    20 |  1766 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  1767 | `	ph7_class_instance *pIt;` |
|     - |  1768 | `	ph7_class *pClass;` |
|    20 |  1769 | `	const char *zName = 0;` |
|    20 |  1770 | `	int nName = 0;` |
|     - |  1771 | `	ph7_value *pSlot;` |
|     - |  1772 | `	ph7_class_method *pCons;` |
|     9 |  1773 | `	SXUNUSED(nArg);` |
|     9 |  1774 | `	SXUNUSED(apArg);` |
|    20 |  1775 | `	if( pThis == 0 ){` |
|   ! 0 |  1776 | `		return PH7_OK;` |
|     - |  1777 | `	}` |
|    20 |  1778 | `	PH7_NativeAttrStr(pThis,SPL_IT,&zName,&nName);` |
|    20 |  1779 | `	if( nName < 1 ){` |
|   ! 0 |  1780 | `		zName = "ArrayIterator";` |
|   ! 0 |  1781 | `		nName = (int)sizeof("ArrayIterator")-1;` |
|   ! 0 |  1782 | `	}` |
|    20 |  1783 | `	pClass = PH7_VmExtractClass(pVm,zName,(sxu32)nName,FALSE,0);` |
|    20 |  1784 | `	if( pClass == 0 ){` |
|   ! 0 |  1785 | `		return PH7_OK;` |
|     - |  1786 | `	}` |
|    20 |  1787 | `	pIt = PH7_NewClassInstance(pVm,pClass);` |
|    20 |  1788 | `	if( pIt == 0 ){` |
|   ! 0 |  1789 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  1790 | `	}` |
|    20 |  1791 | `	pIt->iRef++;` |
|    20 |  1792 | `	pSlot = SplStoreSlot(pVm,pThis);` |
|    20 |  1793 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|    20 |  1794 | `	if( pCons && pSlot ){` |
|     - |  1795 | ``		/* `new $c($this->__d)`: the iterator gets a COPY of the store, as the PHP did —`` |
|     - |  1796 | `		 * a user subclass of ArrayIterator runs its own constructor here. */` |
|     - |  1797 | `		ph7_value *apCtor[1];` |
|    20 |  1798 | `		apCtor[0] = pSlot;` |
|    20 |  1799 | `		PH7_VmCallClassMethod(pVm,pIt,pCons,0,1,apCtor);` |
|     9 |  1800 | `	}` |
|    20 |  1801 | `	PH7_NativeResultObject(pCtx,pIt);` |
|    20 |  1802 | `	PH7_ClassInstanceUnref(pIt);` |
|    20 |  1803 | `	return PH7_OK;` |
|    11 |  1804 | `}` |
|     - |  1805 | `/*` |
|     - |  1806 | ` * php's read_property / has_property / write_property / unset_property for the two` |
|     - |  1807 | ` * store containers, as ph7_class::xProp -- and the FLAG is the whole handler.` |
|     - |  1808 | ` *` |
|     - |  1809 | `` * php's `spl_array_get_hash_table` answers the storage only when`` |
|     - |  1810 | ` * SPL_ARRAY_ARRAY_AS_PROPS is set, and its handlers stand down entirely when it is` |
|     - |  1811 | ` * not: the object's ordinary property table answers instead, so a read of a` |
|     - |  1812 | `` * storage key on the DEFAULT object is php's `Undefined property` warning and a`` |
|     - |  1813 | ` * write creates a property BESIDE the storage. PHL reached the storage through` |
|     - |  1814 | `` * `__get`/`__set`/`__isset`/`__unset` -- four methods php does not have, which`` |
|     - |  1815 | `` * `get_class_methods()` reported -- and those bodies could not stand down: with`` |
|     - |  1816 | ` * the flag off they answered null and swallowed the write in silence, and with it` |
|     - |  1817 | ` * on they answered a missing key in silence where php warns.` |
|     - |  1818 | ` *` |
|     - |  1819 | ` * A name the object holds a REAL property for never reaches here at all (the` |
|     - |  1820 | ` * member opcode consults the handler only on a miss), which is php's own order:` |
|     - |  1821 | `` * `class S extends ArrayObject { public $own; }` writes `$s->own` to the slot and`` |
|     - |  1822 | `` * `$s->x` to the storage.`` |
|     - |  1823 | ` */` |
|     - |  1824 | `#define SPL_ARRAY_AS_PROPS 0x0002` |
|     - |  1825 | `/* The store's node for this property name, or 0. bCreate is php's write-context` |
|     - |  1826 | ` * vivification -- the same rule the DIMENSION fast path above follows. */` |
|    64 |  1827 | `static ph7_hashmap_node * SplPropNode(ph7_vm *pVm,ph7_class_instance *pThis,` |
|     - |  1828 | `	const SyString *pName,int bCreate)` |
|     1 |  1829 | `{` |
|    65 |  1830 | `	ph7_hashmap *pMap = SplStore(pVm,pThis);` |
|    65 |  1831 | `	ph7_hashmap_node *pNode = 0;` |
|     - |  1832 | `	ph7_value sKey;` |
|     - |  1833 | `	int bHit;` |
|    65 |  1834 | `	if( pMap == 0 ){` |
|   ! 0 |  1835 | `		return 0;` |
|     - |  1836 | `	}` |
|    65 |  1837 | `	PH7_MemObjInitFromString(pVm,&sKey,pName);` |
|    65 |  1838 | `	bHit = PH7_HashmapLookup(pMap,&sKey,&pNode) == SXRET_OK;` |
|    65 |  1839 | `	if( !bHit && bCreate ){` |
|     5 |  1840 | `		if( PH7_HashmapInsert(pMap,&sKey,0) == SXRET_OK ){` |
|     5 |  1841 | `			pNode = pMap->pLast;` |
|     5 |  1842 | `			bHit = pNode != 0;` |
|     2 |  1843 | `		}` |
|     2 |  1844 | `	}` |
|    65 |  1845 | `	PH7_MemObjRelease(&sKey);` |
|    65 |  1846 | `	return bHit ? pNode : 0;` |
|    33 |  1847 | `}` |
|   134 |  1848 | `static void SplArrayProp(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativePropCtx *pCtx)` |
|     2 |  1849 | `{` |
|     - |  1850 | `	ph7_hashmap_node *pNode;` |
|     - |  1851 | `	ph7_value *pVal;` |
|   136 |  1852 | `	if( pThis == 0 \|\| (PH7_NativeAttrInt(pThis,SPL_F) & SPL_ARRAY_AS_PROPS) == 0 ){` |
|    12 |  1853 | `		return;   /* php's handler stands down: the standard property path answers */` |
|     - |  1854 | `	}` |
|   125 |  1855 | `	if( pCtx->iMode == PH7_NATIVE_PROP_WRITE ){` |
|    15 |  1856 | `		return;   /* the value does not exist yet; OWNS routes it to STORE */` |
|     - |  1857 | `	}` |
|   111 |  1858 | `	if( pCtx->iMode == PH7_NATIVE_PROP_OWNS ){` |
|     - |  1859 | `		/* Every name is the storage's once the flag is on -- php's handler does not` |
|     - |  1860 | `		 * consult the keys to decide, which is why a write to a name no key carries` |
|     - |  1861 | `		 * CREATES one rather than falling through to a dynamic property. */` |
|    33 |  1862 | `		pCtx->bAnswered = 1;` |
|    33 |  1863 | `		return;` |
|     - |  1864 | `	}` |
|    79 |  1865 | `	if( pCtx->iMode == PH7_NATIVE_PROP_STORE ){` |
|    15 |  1866 | `		ph7_hashmap *pMap = SplStore(pVm,pThis);` |
|    15 |  1867 | `		if( pMap ){` |
|     - |  1868 | `			ph7_value sKey;` |
|    15 |  1869 | `			PH7_MemObjInitFromString(pVm,&sKey,pCtx->pName);` |
|    15 |  1870 | `			PH7_HashmapInsert(pMap,&sKey,pCtx->pResult);` |
|    15 |  1871 | `			PH7_MemObjRelease(&sKey);` |
|     7 |  1872 | `		}` |
|    15 |  1873 | `		pCtx->bAnswered = 1;` |
|    15 |  1874 | `		return;` |
|     - |  1875 | `	}` |
|    65 |  1876 | `	if( pCtx->iMode == PH7_NATIVE_PROP_UNSET ){` |
|     5 |  1877 | `		pNode = SplPropNode(pVm,pThis,pCtx->pName,FALSE);` |
|     5 |  1878 | `		if( pNode ){` |
|     5 |  1879 | `			PH7_HashmapUnlinkNode(pNode,TRUE);` |
|     2 |  1880 | `		}` |
|     5 |  1881 | `		pCtx->bAnswered = 1;` |
|     5 |  1882 | `		return;` |
|     - |  1883 | `	}` |
|    61 |  1884 | `	pCtx->bAnswered = 1;` |
|     - |  1885 | `	/* A WRITE-context read is php's get_property_ptr_ptr: it hands back the store's` |
|     - |  1886 | `	 * OWN element -- creating the key when there is none, silently, exactly as the` |
|     - |  1887 | ``	 * dimension form does -- so `$ao->list[] = 1` and `$ao->deep['k'] = 1` land in`` |
|     - |  1888 | `	 * the store rather than in a temporary nothing else can see. An element that` |
|     - |  1889 | `	 * already EXISTS is handed out for a plain read too, which is what makes` |
|     - |  1890 | ``	 * `sort($ao->nums)` and `foreach ($ao->rows as &$r)` write through. */`` |
|    75 |  1891 | `	pNode = SplPropNode(pVm,pThis,pCtx->pName,` |
|    60 |  1892 | `		pCtx->iMode == PH7_NATIVE_PROP_READ && pCtx->bWriteCtx);` |
|    61 |  1893 | `	pVal = pNode ? (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pNode->nValIdx) : 0;` |
|    61 |  1894 | `	if( pCtx->iMode != PH7_NATIVE_PROP_READ ){` |
|     - |  1895 | `		/* php's three has_property questions, each judging the value it just` |
|     - |  1896 | `		 * fetched: NULL-ness for isset(), TRUTH for the check_empty question` |
|     - |  1897 | ``		 * `empty()` NEGATES, and mere EXISTENCE for property_exists(). A key`` |
|     - |  1898 | ``		 * holding 0 answers true, false and true -- so it is `isset()`, it IS`` |
|     - |  1899 | ``		 * `empty()`, and `property_exists()` finds it. */`` |
|     - |  1900 | `		int bSet;` |
|    33 |  1901 | `		if( pCtx->iMode == PH7_NATIVE_PROP_ISSET ){` |
|    17 |  1902 | `			bSet = pVal != 0 && (pVal->iFlags & MEMOBJ_NULL) == 0;` |
|    25 |  1903 | `		}else if( pCtx->iMode == PH7_NATIVE_PROP_NOTEMPTY ){` |
|     - |  1904 | `			/* The truth of a COPY: this element is the store's own slot and` |
|     - |  1905 | `			 * PH7_MemObjToBool retypes what it is handed, so asking the question` |
|     - |  1906 | ``			 * here would turn `$ao->n` from the int it holds into the bool the`` |
|     - |  1907 | `			 * answer is -- and leave it that way for every later read. */` |
|     9 |  1908 | `			bSet = 0;` |
|     9 |  1909 | `			if( pVal ){` |
|     - |  1910 | `				ph7_value sTruth;` |
|     7 |  1911 | `				PH7_MemObjInit(pVm,&sTruth);` |
|     7 |  1912 | `				PH7_MemObjStore(pVal,&sTruth);` |
|     7 |  1913 | `				PH7_MemObjToBool(&sTruth);` |
|     7 |  1914 | `				bSet = sTruth.x.iVal != 0;` |
|     7 |  1915 | `				PH7_MemObjRelease(&sTruth);` |
|     3 |  1916 | `			}` |
|     5 |  1917 | `		}else{` |
|     9 |  1918 | `			bSet = pVal != 0;` |
|     - |  1919 | `		}` |
|    33 |  1920 | `		ph7_value_bool(pCtx->pResult,bSet);` |
|    33 |  1921 | `		return;` |
|     - |  1922 | `	}` |
|    29 |  1923 | `	if( pVal == 0 ){` |
|     - |  1924 | `		/* php reports the missing ARRAY KEY here, not a missing property: the` |
|     - |  1925 | `		 * handler took the access and the storage is where it looked. Silent for` |
|     - |  1926 | ``		 * the lookup `??` makes, as every read-miss diagnostic is. */`` |
|     7 |  1927 | `		if( !pCtx->bQuiet ){` |
|     3 |  1928 | `			VmErrorFormat(pVm,PH7_CTX_WARNING,"Undefined array key \"%z\"",pCtx->pName);` |
|     1 |  1929 | `		}` |
|     7 |  1930 | `		return;` |
|     - |  1931 | `	}` |
|    23 |  1932 | `	PH7_MemObjStore(pVal,pCtx->pResult);` |
|    23 |  1933 | `	pCtx->nSlot = pNode->nValIdx;` |
|    69 |  1934 | `}` |
|     - |  1935 | `/*` |
|     - |  1936 | `` * php's `__debugInfo()`, which both classes declare so a program can read by name`` |
|     - |  1937 | `` * the array `var_dump()` prints: the object's own properties under php's mangled`` |
|     - |  1938 | `` * keys, then the storage under the mangled `storage` key of the class that`` |
|     - |  1939 | `` * DECLARED it (an `ArrayIterator` subclass shows `\0ArrayIterator\0storage`).`` |
|     - |  1940 | ` * The same array the debug presentation hook builds, which is php's own sharing.` |
|     - |  1941 | ` */` |
|     2 |  1942 | `static int vm_builtin_SplStore_debugInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  1943 | `{` |
|     3 |  1944 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  1945 | `	ph7_value *pOut;` |
|     1 |  1946 | `	SXUNUSED(nArg);` |
|     1 |  1947 | `	SXUNUSED(apArg);` |
|     3 |  1948 | `	pOut = ph7_context_new_array(pCtx);` |
|     3 |  1949 | `	if( pOut == 0 ){` |
|   ! 0 |  1950 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  1951 | `	}` |
|     3 |  1952 | `	SplStorePresent(pCtx->pVm,pThis,pOut,TRUE);` |
|     3 |  1953 | `	ph7_result_value(pCtx,pOut);` |
|     3 |  1954 | `	return PH7_OK;` |
|     2 |  1955 | `}` |
|     - |  1956 | `/*` |
|     - |  1957 | ` * Declare both classes plus SeekableIterator, which ArrayIterator implements and which` |
|     - |  1958 | ` * therefore cannot wait for the chunk. RecursiveArrayIterator still lives there and extends` |
|     - |  1959 | ` * ArrayIterator, so this install has to run BEFORE the chunk is evaluated.` |
|     - |  1960 | ` */` |
|  8445 |  1961 | `static sxi32 VmInstallSplStore(ph7_vm *pVm)` |
|     5 |  1962 | `{` |
|     - |  1963 | ``	/* php's `@tentative-return-type void`, as on the other SPL contracts`` |
|     - |  1964 | `	 * (VmInstallSplDualIterators). */` |
|     - |  1965 | `	static const PH7_NativeMethodDef aSeekMethod[] = {` |
|     - |  1966 | `		{ "seek", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "int $offset", "@void", 0 },` |
|     - |  1967 | `	};` |
|     - |  1968 | `	static const PH7_NativePropDef aItProp[] = {` |
|     - |  1969 | `		{ SPL_D, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  1970 | `		{ SPL_F, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  1971 | `	};` |
|     - |  1972 | `	static const PH7_NativePropDef aObjProp[] = {` |
|     - |  1973 | `		{ SPL_D,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  1974 | `		{ SPL_F,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  1975 | `		{ SPL_IT, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "ArrayIterator", 0.0 }, 0 },` |
|     - |  1976 | `	};` |
|     - |  1977 | `	static const PH7_NativeConstDef aConst[] = {` |
|     - |  1978 | `		{ "STD_PROP_LIST",  PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 1, 0, 0.0 },` |
|     - |  1979 | `		{ "ARRAY_AS_PROPS", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2, 0, 0.0 },` |
|     - |  1980 | `	};` |
|     - |  1981 | `	/* php's declaration order, which is the order Reflection reports. */` |
|     - |  1982 | `	static const PH7_NativeMethodDef aItMethod[] = {` |
|     - |  1983 | `		{ "__construct",  PH7_MOD_PUBLIC, "object\|array $array = [], int $flags = 0", 0,` |
|     - |  1984 | `		  vm_builtin_ArrayIterator_construct },` |
|     - |  1985 | `		{ "offsetExists", PH7_MOD_PUBLIC, "mixed $key", "@bool", vm_builtin_SplStore_offsetExists },` |
|     - |  1986 | `		{ "offsetGet",    PH7_MOD_PUBLIC, "mixed $key", "@mixed", vm_builtin_SplStore_offsetGet },` |
|     - |  1987 | `		{ "offsetSet",    PH7_MOD_PUBLIC, "mixed $key, mixed $value", "@void", vm_builtin_SplStore_offsetSet },` |
|     - |  1988 | `		{ "offsetUnset",  PH7_MOD_PUBLIC, "mixed $key", "@void", vm_builtin_SplStore_offsetUnset },` |
|     - |  1989 | `		{ "append",       PH7_MOD_PUBLIC, "mixed $value", "@void", vm_builtin_SplStore_append },` |
|     - |  1990 | `		{ "getArrayCopy", PH7_MOD_PUBLIC, "", "@array", vm_builtin_SplStore_getArrayCopy },` |
|     - |  1991 | `		{ "count",        PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplStore_count },` |
|     - |  1992 | `		{ "getFlags",     PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplStore_getFlags },` |
|     - |  1993 | `		{ "setFlags",     PH7_MOD_PUBLIC, "int $flags", "@void", vm_builtin_SplStore_setFlags },` |
|     - |  1994 | `		{ "asort",        PH7_MOD_PUBLIC, "int $flags = SORT_REGULAR", "@true", vm_builtin_SplStore_asort },` |
|     - |  1995 | `		{ "ksort",        PH7_MOD_PUBLIC, "int $flags = SORT_REGULAR", "@true", vm_builtin_SplStore_ksort },` |
|     - |  1996 | `		{ "uasort",       PH7_MOD_PUBLIC, "callable $callback", "@true", vm_builtin_SplStore_uasort },` |
|     - |  1997 | `		{ "uksort",       PH7_MOD_PUBLIC, "callable $callback", "@true", vm_builtin_SplStore_uksort },` |
|     - |  1998 | `		{ "natsort",      PH7_MOD_PUBLIC, "", "@true", vm_builtin_SplStore_natsort },` |
|     - |  1999 | `		{ "natcasesort",  PH7_MOD_PUBLIC, "", "@true", vm_builtin_SplStore_natcasesort },` |
|     - |  2000 | `		{ "unserialize",  PH7_MOD_PUBLIC, "string $data", "@void", vm_builtin_SplStore_unserialize },` |
|     - |  2001 | `		{ "serialize",    PH7_MOD_PUBLIC, "", "@string", vm_builtin_SplStore_serialize },` |
|     - |  2002 | `		{ "__serialize",  PH7_MOD_PUBLIC, "", "@array", vm_builtin_SplStore_serializeMagic },` |
|     - |  2003 | `		{ "__unserialize", PH7_MOD_PUBLIC, "array $data", "@void",` |
|     - |  2004 | `		  vm_builtin_SplStore_unserializeMagic },` |
|     - |  2005 | `		/* php's own order for the Iterator five -- rewind FIRST, which is the order` |
|     - |  2006 | `		 * its stub declares them in and therefore the order get_class_methods() and` |
|     - |  2007 | `		 * Reflection report. */` |
|     - |  2008 | `		{ "rewind",       PH7_MOD_PUBLIC, "", "@void", vm_builtin_ArrayIterator_rewind },` |
|     - |  2009 | `		{ "current",      PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_ArrayIterator_current },` |
|     - |  2010 | `		{ "key",          PH7_MOD_PUBLIC, "", "@string\|int\|null", vm_builtin_ArrayIterator_key },` |
|     - |  2011 | `		{ "next",         PH7_MOD_PUBLIC, "", "@void", vm_builtin_ArrayIterator_next },` |
|     - |  2012 | `		{ "valid",        PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ArrayIterator_valid },` |
|     - |  2013 | `		{ "seek",         PH7_MOD_PUBLIC, "int $offset", "@void", vm_builtin_ArrayIterator_seek },` |
|     - |  2014 | `		{ "__debugInfo",  PH7_MOD_PUBLIC, "", 0, vm_builtin_SplStore_debugInfo },` |
|     - |  2015 | `	};` |
|     - |  2016 | `	static const PH7_NativeMethodDef aObjMethod[] = {` |
|     - |  2017 | `		{ "__construct",      PH7_MOD_PUBLIC,` |
|     - |  2018 | `		  "object\|array $array = [], int $flags = 0, ~string $iteratorClass = ArrayIterator::class", 0,` |
|     - |  2019 | `		  vm_builtin_ArrayObject_construct },` |
|     - |  2020 | `		{ "offsetExists",     PH7_MOD_PUBLIC, "mixed $key", "@bool", vm_builtin_SplStore_offsetExists },` |
|     - |  2021 | `		{ "offsetGet",        PH7_MOD_PUBLIC, "mixed $key", "@mixed", vm_builtin_SplStore_offsetGet },` |
|     - |  2022 | `		{ "offsetSet",        PH7_MOD_PUBLIC, "mixed $key, mixed $value", "@void", vm_builtin_SplStore_offsetSet },` |
|     - |  2023 | `		{ "offsetUnset",      PH7_MOD_PUBLIC, "mixed $key", "@void", vm_builtin_SplStore_offsetUnset },` |
|     - |  2024 | `		{ "append",           PH7_MOD_PUBLIC, "mixed $value", "@void", vm_builtin_SplStore_append },` |
|     - |  2025 | `		{ "getArrayCopy",     PH7_MOD_PUBLIC, "", "@array", vm_builtin_SplStore_getArrayCopy },` |
|     - |  2026 | `		{ "count",            PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplStore_count },` |
|     - |  2027 | `		{ "getFlags",         PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplStore_getFlags },` |
|     - |  2028 | `		{ "setFlags",         PH7_MOD_PUBLIC, "int $flags", "@void", vm_builtin_SplStore_setFlags },` |
|     - |  2029 | `		{ "asort",            PH7_MOD_PUBLIC, "int $flags = SORT_REGULAR", "@true", vm_builtin_SplStore_asort },` |
|     - |  2030 | `		{ "ksort",            PH7_MOD_PUBLIC, "int $flags = SORT_REGULAR", "@true", vm_builtin_SplStore_ksort },` |
|     - |  2031 | `		{ "uasort",           PH7_MOD_PUBLIC, "callable $callback", "@true", vm_builtin_SplStore_uasort },` |
|     - |  2032 | `		{ "uksort",           PH7_MOD_PUBLIC, "callable $callback", "@true", vm_builtin_SplStore_uksort },` |
|     - |  2033 | `		{ "natsort",          PH7_MOD_PUBLIC, "", "@true", vm_builtin_SplStore_natsort },` |
|     - |  2034 | `		{ "natcasesort",      PH7_MOD_PUBLIC, "", "@true", vm_builtin_SplStore_natcasesort },` |
|     - |  2035 | `		{ "unserialize",      PH7_MOD_PUBLIC, "string $data", "@void", vm_builtin_SplStore_unserialize },` |
|     - |  2036 | `		{ "serialize",        PH7_MOD_PUBLIC, "", "@string", vm_builtin_SplStore_serialize },` |
|     - |  2037 | `		{ "__serialize",      PH7_MOD_PUBLIC, "", "@array", vm_builtin_SplStore_serializeMagic },` |
|     - |  2038 | `		{ "__unserialize",    PH7_MOD_PUBLIC, "array $data", "@void",` |
|     - |  2039 | `		  vm_builtin_SplStore_unserializeMagic },` |
|     - |  2040 | `		{ "getIterator",      PH7_MOD_PUBLIC, "", "@Iterator", vm_builtin_ArrayObject_getIterator },` |
|     - |  2041 | `		{ "exchangeArray",    PH7_MOD_PUBLIC, "object\|array $array", "@array", vm_builtin_ArrayObject_exchangeArray },` |
|     - |  2042 | `		{ "setIteratorClass", PH7_MOD_PUBLIC, "~string $iteratorClass", "@void", vm_builtin_ArrayObject_setIteratorClass },` |
|     - |  2043 | `		{ "getIteratorClass", PH7_MOD_PUBLIC, "", "@string", vm_builtin_ArrayObject_getIteratorClass },` |
|     - |  2044 | `		{ "__debugInfo",      PH7_MOD_PUBLIC, "", 0, vm_builtin_SplStore_debugInfo },` |
|     - |  2045 | `	};` |
|     - |  2046 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - |  2047 | ``		/* `interface X extends Iterator` is a PARENT, not an implemented interface:`` |
|     - |  2048 | `		 * the compiler puts it in pBase and Reflection walks pBase to answer which` |
|     - |  2049 | `		 * class DECLARED an inherited method. Naming it in zImplements instead made` |
|     - |  2050 | `		 * current()/key()/next()/rewind()/valid() report this interface as their` |
|     - |  2051 | `		 * declaring class where php reports Iterator. */` |
|     - |  2052 | `		{ "SeekableIterator", "Iterator", 0, PH7_CLASS_INTERFACE,` |
|     - |  2053 | `		  aSeekMethod, SX_ARRAYSIZE(aSeekMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - |  2054 | `		/* PH7_CLASS_DIM_WRITABLE: php's spl_array read_dimension hands back the` |
|     - |  2055 | ``		 * REAL element for a write fetch, so `$ao['k']['n'] = v` lands — unlike`` |
|     - |  2056 | `		 * SplFixedArray / SplDoublyLinkedList / SplObjectStorage, which keep the` |
|     - |  2057 | `		 * standard handler and get php's indirect-modification notice. */` |
|     - |  2058 | `		{ "ArrayIterator", 0, "SeekableIterator,ArrayAccess,Serializable,Countable", PH7_CLASS_DIM_WRITABLE,` |
|     - |  2059 | `		  aItMethod, SX_ARRAYSIZE(aItMethod), aConst, SX_ARRAYSIZE(aConst),` |
|     - |  2060 | `		  aItProp, SX_ARRAYSIZE(aItProp), 0, 0, SplStorePresent },` |
|     - |  2061 | `		{ "ArrayObject", 0, "IteratorAggregate,ArrayAccess,Serializable,Countable", PH7_CLASS_DIM_WRITABLE,` |
|     - |  2062 | `		  aObjMethod, SX_ARRAYSIZE(aObjMethod), aConst, SX_ARRAYSIZE(aConst),` |
|     - |  2063 | `		  aObjProp, SX_ARRAYSIZE(aObjProp), 0, 0, SplStorePresent },` |
|     - |  2064 | `	};` |
|  8450 |  2065 | `	sxi32 rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|  8450 |  2066 | `	if( rc == SXRET_OK ){` |
|     - |  2067 | `		/* The property handler, assigned here for the same reason the clone and` |
|     - |  2068 | `		 * dimension hooks are: PH7_NativeClassSpec carries no field for one. Both` |
|     - |  2069 | `		 * roots wear it, and RecursiveArrayIterator reaches ArrayIterator's through` |
|     - |  2070 | `		 * the engine's base-chain walk -- php's handler inheritance. */` |
|  8450 |  2071 | `		PH7_NativeClassInstallPropHook(&(*pVm),"ArrayObject",SplArrayProp);` |
|  8450 |  2072 | `		PH7_NativeClassInstallPropHook(&(*pVm),"ArrayIterator",SplArrayProp);` |
|  4217 |  2073 | `	}` |
|  8450 |  2074 | `	return rc;` |
|     5 |  2075 | `}` |
|     - |  2076 | `/*` |
|     - |  2077 | ` * ---------------------------------------------------------------------------` |
|     - |  2078 | ` * The SPL DUAL ITERATORS: IteratorIterator and the decorators built on it.` |
|     - |  2079 | ` *` |
|     - |  2080 | `` * php's `spl_dual_it_object` is a CACHE, and that is the whole design. rewind()`` |
|     - |  2081 | ` * and next() move the INNER iterator and then COPY its current()/key() onto the` |
|     - |  2082 | ` * decorator; valid(), current() and key() answer out of that copy and never reach` |
|     - |  2083 | ` * the inner iterator again. The chunk forwarded all five live, which is three` |
|     - |  2084 | ` * observable divergences at once: a fresh decorator was valid() BEFORE rewind()` |
|     - |  2085 | ` * (php answers false — nothing has been fetched yet), current() followed an inner` |
|     - |  2086 | ` * iterator that had been moved behind the decorator's back (php answers what it` |
|     - |  2087 | ` * cached), and a decorator left past the end still answered the inner's stale` |
|     - |  2088 | ` * key(). Everything below is written around the cache because the cache IS the` |
|     - |  2089 | ` * class.` |
|     - |  2090 | ` *` |
|     - |  2091 | `` * Two slots hold what php holds in two fields: `__in` is `inner.zobject` — the`` |
|     - |  2092 | `` * object getInnerIterator() answers — and `__it` is `inner.iterator`, the Iterator`` |
|     - |  2093 | ` * actually driven. They differ for exactly one input: an IteratorAggregate whose` |
|     - |  2094 | ` * getIterator() answers another IteratorAggregate. php unwraps ONE level in the` |
|     - |  2095 | ` * constructor and lets the engine's get_iterator handler unwrap the rest at` |
|     - |  2096 | `` * iteration time, so `new IteratorIterator($aggOfAgg)` answers the inner AGGREGATE`` |
|     - |  2097 | `` * from getInnerIterator() and still iterates. The chunk's `while` loop unwrapped`` |
|     - |  2098 | ` * to the bottom and answered the ArrayIterator instead.` |
|     - |  2099 | ` */` |
|     - |  2100 | `#define IT_IN  "__in"   /* php's inner.zobject: what getInnerIterator() answers */` |
|     - |  2101 | `#define IT_IT  "__it"   /* php's inner.iterator: the Iterator actually driven */` |
|     - |  2102 | `#define IT_CD  "__cd"   /* the cached current() */` |
|     - |  2103 | `#define IT_CK  "__ck"   /* the cached key() */` |
|     - |  2104 | `#define IT_CF  "__cf"   /* 1 while the cached pair is live (php's IS_UNDEF check) */` |
|     - |  2105 | `#define IT_CP  "__cp"   /* php's current.pos */` |
|     - |  2106 | `#define IT_OFF "__off"  /* LimitIterator's offset */` |
|     - |  2107 | `#define IT_LIM "__lim"  /* LimitIterator's count, -1 for "all" */` |
|     - |  2108 | `#define IT_CB  "__cb"   /* CallbackFilterIterator's callback */` |
|     - |  2109 | ``#define AP_LIST "__ai"  /* AppendIterator's php `u.append.zarrayit`: the real ArrayIterator`` |
|     - |  2110 | `                         * holding everything append()ed, whose OWN cursor is php's` |
|     - |  2111 | ``                         * `u.append.iterator` -- one position, which is why`` |
|     - |  2112 | `                         * getArrayIterator()->rewind() moves getIteratorIndex(). */` |
|     - |  2113 |  |
|     - |  2114 | `/*` |
|     - |  2115 | ` * php's SPL_FETCH_AND_CHECK_DUAL_IT: a subclass whose constructor never called` |
|     - |  2116 | ` * parent::__construct() has no inner iterator, and php refuses every method on it` |
|     - |  2117 | ` * rather than answering a null-flavoured nothing.` |
|     - |  2118 | ` */` |
|    34 |  2119 | `static sxi32 DualNotReady(ph7_context *pCtx)` |
|     1 |  2120 | `{` |
|    35 |  2121 | `	return PH7_VmThrowException(pCtx,"Error",` |
|     - |  2122 | `		"The object is in an invalid state as the parent constructor was not called");` |
|     1 |  2123 | `}` |
|  5106 |  2124 | `static ph7_class_instance * DualDriver(ph7_class_instance *pThis)` |
|     4 |  2125 | `{` |
|  5110 |  2126 | `	return pThis ? PH7_NativeAttrObj(pThis,IT_IT) : 0;` |
|     4 |  2127 | `}` |
|  1986 |  2128 | `static int DualFilled(ph7_class_instance *pThis)` |
|     4 |  2129 | `{` |
|  1990 |  2130 | `	return pThis && PH7_NativeAttrInt(pThis,IT_CF) != 0;` |
|     4 |  2131 | `}` |
|     - |  2132 | `/*` |
|     - |  2133 | `` * php's SPL_FETCH_AND_CHECK_DUAL_IT tests `dit_type`, which means "the constructor`` |
|     - |  2134 | ` * ran" -- and for every decorator but one that is the same thing as "an inner` |
|     - |  2135 | ` * iterator exists". AppendIterator's constructor takes NO iterator: it builds an` |
|     - |  2136 | ` * empty list and is immediately usable (valid() false, current()/key() null, no` |
|     - |  2137 | ` * refusal), so its readiness lives in the list slot instead.` |
|     - |  2138 | ` */` |
|  2716 |  2139 | `static int DualReady(ph7_class_instance *pThis)` |
|     4 |  2140 | `{` |
|  4148 |  2141 | `	return pThis && (PH7_NativeAttrObj(pThis,IT_IT) != 0` |
|  1428 |  2142 | `		\|\| PH7_NativeAttrObj(pThis,AP_LIST) != 0);` |
|     4 |  2143 | `}` |
|     - |  2144 | `/*` |
|     - |  2145 | ` * Assign one of the instance's own slots. The slot is re-resolved BY NAME here on` |
|     - |  2146 | ` * purpose: a call into user code (and every inner->current() is one) can unset and` |
|     - |  2147 | ` * re-create the property underneath, which gives it a different pool slot. It used` |
|     - |  2148 | ` * to matter for a second reason as well -- the pool itself reallocated as the VM` |
|     - |  2149 | ` * reserved objects -- and that one is gone since P1 (fixed segments).` |
|     - |  2150 | ` */` |
|  1478 |  2151 | `static void DualSetSlot(ph7_vm *pVm,ph7_class_instance *pThis,const char *zName,ph7_value *pVal)` |
|     4 |  2152 | `{` |
|  1482 |  2153 | `	ph7_value *pSlot = PH7_NativeAttr(pThis,zName);` |
|   739 |  2154 | `	SXUNUSED(pVm);` |
|  1482 |  2155 | `	if( pSlot ){` |
|  1482 |  2156 | `		PH7_MemObjStore(pVal,pSlot);` |
|   739 |  2157 | `	}` |
|  1482 |  2158 | `}` |
|     - |  2159 | `/* php's spl_dual_it_free: drop the cached pair. */` |
|  1744 |  2160 | `static void DualFree(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     4 |  2161 | `{` |
|     - |  2162 | `	ph7_value *pSlot;` |
|  1748 |  2163 | `	if( pThis == 0 ){` |
|   ! 0 |  2164 | `		return;` |
|     - |  2165 | `	}` |
|  1748 |  2166 | `	pSlot = PH7_NativeAttr(pThis,IT_CD);` |
|  1748 |  2167 | `	if( pSlot ){ PH7_MemObjRelease(pSlot); }` |
|  1748 |  2168 | `	pSlot = PH7_NativeAttr(pThis,IT_CK);` |
|  1748 |  2169 | `	if( pSlot ){ PH7_MemObjRelease(pSlot); }` |
|  1748 |  2170 | `	PH7_NativeSetAttrInt(pVm,pThis,IT_CF,0);` |
|   876 |  2171 | `}` |
|     - |  2172 | `/* Call a zero-argument method on the driven iterator, propagating a throw (rule:` |
|     - |  2173 | ` * a native body that answers PH7_OK with an exception in flight lets the caller` |
|     - |  2174 | ` * carry on). A missing method is the foreach opcode's leniency, not an error. */` |
|  3522 |  2175 | `static sxi32 DualCall(ph7_vm *pVm,ph7_class_instance *pThis,const char *zName,sxu32 nLen,` |
|     - |  2176 | `	ph7_value *pResult)` |
|     4 |  2177 | `{` |
|  3526 |  2178 | `	ph7_class_instance *pIn = DualDriver(pThis);` |
|  3526 |  2179 | `	if( pIn == 0 ){` |
|    43 |  2180 | `		return SXRET_OK;` |
|     - |  2181 | `	}` |
|  3484 |  2182 | `	return VmIterCallMethod(pVm,pIn,zName,nLen,pResult);` |
|  1765 |  2183 | `}` |
|     - |  2184 | `/* php's spl_dual_it_valid: the INNER's valid(), not the cache's. */` |
|  1242 |  2185 | `static sxi32 DualInnerValid(ph7_vm *pVm,ph7_class_instance *pThis,int *pbValid)` |
|     4 |  2186 | `{` |
|     - |  2187 | `	ph7_value sVal;` |
|     - |  2188 | `	sxi32 rc;` |
|  1246 |  2189 | `	*pbValid = 0;` |
|  1246 |  2190 | `	PH7_MemObjInit(pVm,&sVal);` |
|  1246 |  2191 | `	rc = DualCall(pVm,pThis,"valid",sizeof("valid")-1,&sVal);` |
|  1246 |  2192 | `	if( rc == SXRET_OK ){` |
|  1246 |  2193 | `		PH7_MemObjToBool(&sVal);          /* a STATUS, not the answer */` |
|  1246 |  2194 | `		*pbValid = sVal.x.iVal != 0;` |
|   621 |  2195 | `	}` |
|  1246 |  2196 | `	PH7_MemObjRelease(&sVal);` |
|  1246 |  2197 | `	return rc;` |
|     4 |  2198 | `}` |
|     - |  2199 | `/*` |
|     - |  2200 | ``  * php's spl_dual_it_fetch: refill the cache from the inner iterator. `bCheckMore` `` |
|     - |  2201 | ` * is php's check_more — false means "the caller already knows the inner is valid",` |
|     - |  2202 | ` * which is how LimitIterator's seek and InfiniteIterator's wrap-around fetch.` |
|     - |  2203 | ` */` |
|   870 |  2204 | `static sxi32 DualFetch(ph7_vm *pVm,ph7_class_instance *pThis,int bCheckMore)` |
|     4 |  2205 | `{` |
|     - |  2206 | `	ph7_value sVal;` |
|     - |  2207 | `	sxi32 rc;` |
|   874 |  2208 | `	int bValid = 1;` |
|   874 |  2209 | `	DualFree(pVm,pThis);` |
|   874 |  2210 | `	if( DualDriver(pThis) == 0 ){` |
|     5 |  2211 | `		return SXRET_OK;` |
|     - |  2212 | `	}` |
|   870 |  2213 | `	if( bCheckMore ){` |
|   752 |  2214 | `		rc = DualInnerValid(pVm,pThis,&bValid);` |
|   752 |  2215 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  2216 | `			return rc;` |
|     - |  2217 | `		}` |
|   374 |  2218 | `	}` |
|   870 |  2219 | `	if( !bValid ){` |
|   195 |  2220 | `		return SXRET_OK;` |
|     - |  2221 | `	}` |
|   678 |  2222 | `	PH7_MemObjInit(pVm,&sVal);` |
|   678 |  2223 | `	rc = DualCall(pVm,pThis,"current",sizeof("current")-1,&sVal);` |
|   678 |  2224 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  2225 | `		PH7_MemObjRelease(&sVal);` |
|   ! 0 |  2226 | `		return rc;` |
|     - |  2227 | `	}` |
|   678 |  2228 | `	DualSetSlot(pVm,pThis,IT_CD,&sVal);` |
|   678 |  2229 | `	PH7_MemObjRelease(&sVal);` |
|   678 |  2230 | `	PH7_MemObjInit(pVm,&sVal);` |
|   678 |  2231 | `	rc = DualCall(pVm,pThis,"key",sizeof("key")-1,&sVal);` |
|   678 |  2232 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  2233 | `		PH7_MemObjRelease(&sVal);` |
|   ! 0 |  2234 | `		DualFree(pVm,pThis);   /* php drops the half-filled pair when key() throws */` |
|   ! 0 |  2235 | `		return rc;` |
|     - |  2236 | `	}` |
|   678 |  2237 | `	DualSetSlot(pVm,pThis,IT_CK,&sVal);` |
|   678 |  2238 | `	PH7_MemObjRelease(&sVal);` |
|   678 |  2239 | `	PH7_NativeSetAttrInt(pVm,pThis,IT_CF,1);` |
|   678 |  2240 | `	return SXRET_OK;` |
|   439 |  2241 | `}` |
|     - |  2242 | `/* php's spl_dual_it_rewind: free, position back to zero, rewind the inner. */` |
|   374 |  2243 | `static sxi32 DualRewindInner(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     4 |  2244 | `{` |
|   378 |  2245 | `	DualFree(pVm,pThis);` |
|   378 |  2246 | `	PH7_NativeSetAttrInt(pVm,pThis,IT_CP,0);` |
|   378 |  2247 | `	return DualCall(pVm,pThis,"rewind",sizeof("rewind")-1,0);` |
|     4 |  2248 | `}` |
|     - |  2249 | `/*` |
|     - |  2250 | ``  * php's spl_dual_it_next: free, advance the inner, count the step. Its `do_free` `` |
|     - |  2251 | ` * is FALSE for exactly one caller — CachingIterator, which has just copied the` |
|     - |  2252 | ` * pair it is standing on and steps the inner one ahead of it, so dropping the` |
|     - |  2253 | ` * cache here would erase the very element the decorator answers.` |
|     - |  2254 | ` */` |
|   442 |  2255 | `static sxi32 DualNextInnerEx(ph7_vm *pVm,ph7_class_instance *pThis,int bFree)` |
|     3 |  2256 | `{` |
|     - |  2257 | `	sxi32 rc;` |
|   445 |  2258 | `	if( bFree ){` |
|   260 |  2259 | `		DualFree(pVm,pThis);` |
|   129 |  2260 | `	}` |
|   445 |  2261 | `	rc = DualCall(pVm,pThis,"next",sizeof("next")-1,0);` |
|   445 |  2262 | `	PH7_NativeSetAttrInt(pVm,pThis,IT_CP,PH7_NativeAttrInt(pThis,IT_CP)+1);` |
|   445 |  2263 | `	return rc;` |
|     3 |  2264 | `}` |
|   258 |  2265 | `static sxi32 DualNextInner(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     2 |  2266 | `{` |
|   260 |  2267 | `	return DualNextInnerEx(pVm,pThis,TRUE);` |
|     2 |  2268 | `}` |
|     - |  2269 | `/* Hand back a cached slot, or php's null for an empty cache. */` |
|   744 |  2270 | `static int DualResultSlot(ph7_context *pCtx,const char *zName)` |
|     4 |  2271 | `{` |
|   748 |  2272 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  2273 | `	ph7_value *pSlot;` |
|   748 |  2274 | `	if( !DualReady(pThis) ){` |
|   ! 0 |  2275 | `		return DualNotReady(pCtx);` |
|     - |  2276 | `	}` |
|   748 |  2277 | `	if( !DualFilled(pThis) ){` |
|    27 |  2278 | `		ph7_result_null(pCtx);` |
|    27 |  2279 | `		return PH7_OK;` |
|     - |  2280 | `	}` |
|   722 |  2281 | `	pSlot = PH7_NativeAttr(pThis,zName);` |
|   722 |  2282 | `	if( pSlot ){` |
|   722 |  2283 | `		ph7_result_value(pCtx,pSlot);` |
|   359 |  2284 | `	}` |
|   722 |  2285 | `	return PH7_OK;` |
|   376 |  2286 | `}` |
|     - |  2287 | `/*` |
|     - |  2288 | ` * The constructor every dual iterator shares. php words the "already built" refusal` |
|     - |  2289 | ` * with the DECLARING class's name and with getIterator() rather than __construct(),` |
|     - |  2290 | ` * so each class hands its own name in.` |
|     - |  2291 | ` */` |
|   448 |  2292 | `static sxi32 DualConstruct(ph7_context *pCtx,const char *zOwner,int nArg,ph7_value **apArg)` |
|     4 |  2293 | `{` |
|   452 |  2294 | `	ph7_vm *pVm = pCtx->pVm;` |
|   452 |  2295 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  2296 | `	ph7_class_instance *pObj;` |
|     - |  2297 | `	ph7_class *pIterCls, *pAggCls, *pTravCls;` |
|   452 |  2298 | `	ph7_class *pCast = 0;` |
|   452 |  2299 | `	ph7_class_instance *pHold = 0;   /* the unwrapped iterator, kept alive across levels */` |
|     - |  2300 | `	int nLevel;` |
|   452 |  2301 | `	if( pThis == 0 ){` |
|   ! 0 |  2302 | `		return PH7_OK;` |
|     - |  2303 | `	}` |
|   452 |  2304 | `	if( PH7_NativeAttrObj(pThis,IT_IN) != 0 ){` |
|     4 |  2305 | `		return PH7_VmThrowException(pCtx,"BadMethodCallException",` |
|     1 |  2306 | `			"%s::getIterator() must be called exactly once per instance",zOwner);` |
|     - |  2307 | `	}` |
|   450 |  2308 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|   ! 0 |  2309 | `		return PH7_OK;   /* the shared ZPP screen already refused a non-object */` |
|     - |  2310 | `	}` |
|   450 |  2311 | `	pObj = (ph7_class_instance *)apArg[0]->x.pOther;` |
|   450 |  2312 | `	pIterCls = PH7_VmExtractClass(pVm,"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|   450 |  2313 | `	pAggCls = PH7_VmExtractClass(pVm,"IteratorAggregate",sizeof("IteratorAggregate")-1,FALSE,0);` |
|   450 |  2314 | `	pTravCls = PH7_VmExtractClass(pVm,"Traversable",sizeof("Traversable")-1,FALSE,0);` |
|   450 |  2315 | `	if( pIterCls && PH7_VmInstanceOf(pObj->pClass,pIterCls) ){` |
|     - |  2316 | `		/* Already an Iterator: php ignores $class entirely on this path. */` |
|   440 |  2317 | `		PH7_NativeSetAttrObj(pVm,pThis,IT_IN,pObj);` |
|   440 |  2318 | `		PH7_NativeSetAttrObj(pVm,pThis,IT_IT,pObj);` |
|   440 |  2319 | `		return PH7_OK;` |
|     - |  2320 | `	}` |
|    11 |  2321 | `	if( nArg > 1 && (apArg[1]->iFlags & MEMOBJ_NULL) == 0 ){` |
|     - |  2322 | `		/* php's DOWNCAST: $class names the class whose getIterator() to run, which is` |
|     - |  2323 | `		 * how a subclass asks for its parent's traversal. It must be a base of the` |
|     - |  2324 | `		 * argument AND traversable itself. */` |
|     - |  2325 | `		int nName;` |
|     5 |  2326 | `		const char *zName = ph7_value_to_string(apArg[1],&nName);` |
|     5 |  2327 | `		pCast = nName > 0 ? PH7_VmExtractClass(pVm,zName,(sxu32)nName,FALSE,0) : 0;` |
|     4 |  2328 | `		if( pCast == 0 \|\| !PH7_VmInstanceOf(pObj->pClass,pCast)` |
|     3 |  2329 | `		 \|\| (pTravCls && !PH7_VmInstanceOf(pCast,pTravCls)) ){` |
|     5 |  2330 | `			return PH7_VmThrowException(pCtx,"LogicException",` |
|     - |  2331 | `				"Class to downcast to not found or not base class or does not implement Traversable");` |
|     - |  2332 | `		}` |
|   ! 0 |  2333 | `	}` |
|     - |  2334 | `	/*` |
|     - |  2335 | `	 * An IteratorAggregate: run getIterator() — the DOWNCAST class's when one was` |
|     - |  2336 | `	 * named — and keep its answer as the inner object. php stops after one level` |
|     - |  2337 | `	 * here; the loop below is the engine's get_iterator handler, which resolves the` |
|     - |  2338 | `	 * rest lazily, done eagerly because PHL drives the inner through the METHOD` |
|     - |  2339 | `	 * protocol and nothing else would unwrap it.` |
|     - |  2340 | `	 */` |
|     9 |  2341 | `	for( nLevel = 0 ; nLevel < 16 ; ++nLevel ){` |
|     9 |  2342 | `		ph7_class *pFrom = pCast ? pCast : pObj->pClass;` |
|     - |  2343 | `		ph7_class_method *pMethod;` |
|     - |  2344 | `		ph7_value sInner;` |
|     - |  2345 | `		sxi32 rc;` |
|     9 |  2346 | `		if( pAggCls == 0 \|\| !PH7_VmInstanceOf(pObj->pClass,pAggCls) ){` |
|   ! 0 |  2347 | `			break;` |
|     - |  2348 | `		}` |
|     9 |  2349 | `		pMethod = PH7_ClassExtractMethod(pFrom,"getIterator",sizeof("getIterator")-1);` |
|     9 |  2350 | `		if( pMethod == 0 ){` |
|   ! 0 |  2351 | `			break;` |
|     - |  2352 | `		}` |
|     9 |  2353 | `		PH7_MemObjInit(pVm,&sInner);` |
|     9 |  2354 | `		rc = PH7_VmCallClassMethod(pVm,pObj,pMethod,&sInner,0,0);` |
|     9 |  2355 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  2356 | `			PH7_MemObjRelease(&sInner);` |
|   ! 0 |  2357 | `			return rc;` |
|     - |  2358 | `		}` |
|     8 |  2359 | `		if( (sInner.iFlags & MEMOBJ_OBJ) == 0 \|\| sInner.x.pOther == 0` |
|     9 |  2360 | `		 \|\| (pTravCls && !PH7_VmInstanceOf(((ph7_class_instance *)sInner.x.pOther)->pClass,pTravCls)) ){` |
|   ! 0 |  2361 | `			SyString *pName = &pFrom->sName;` |
|   ! 0 |  2362 | `			PH7_MemObjRelease(&sInner);` |
|   ! 0 |  2363 | `			return PH7_VmThrowException(pCtx,"LogicException",` |
|   ! 0 |  2364 | `				"%z::getIterator() must return an object that implements Traversable",pName);` |
|     - |  2365 | `		}` |
|     9 |  2366 | `		pObj = (ph7_class_instance *)sInner.x.pOther;` |
|     9 |  2367 | `		pObj->iRef++;                 /* survive the release of the call result */` |
|     9 |  2368 | `		PH7_MemObjRelease(&sInner);` |
|     9 |  2369 | `		if( pHold ){` |
|     3 |  2370 | `			PH7_ClassInstanceUnref(pHold);` |
|     1 |  2371 | `		}` |
|     9 |  2372 | `		pHold = pObj;                 /* this function owns exactly one reference */` |
|     9 |  2373 | `		if( nLevel == 0 ){` |
|     - |  2374 | `			/* php's inner.zobject is the FIRST unwrap and nothing deeper. */` |
|     7 |  2375 | `			PH7_NativeSetAttrObj(pVm,pThis,IT_IN,pObj);` |
|     3 |  2376 | `		}` |
|     9 |  2377 | `		pCast = 0;` |
|     9 |  2378 | `		if( pIterCls && PH7_VmInstanceOf(pObj->pClass,pIterCls) ){` |
|     7 |  2379 | `			break;` |
|     - |  2380 | `		}` |
|     2 |  2381 | `	}` |
|     7 |  2382 | `	if( PH7_NativeAttrObj(pThis,IT_IN) == 0 ){` |
|   ! 0 |  2383 | `		PH7_NativeSetAttrObj(pVm,pThis,IT_IN,pObj);` |
|   ! 0 |  2384 | `	}` |
|     7 |  2385 | `	PH7_NativeSetAttrObj(pVm,pThis,IT_IT,pObj);` |
|     7 |  2386 | `	if( pHold ){` |
|     7 |  2387 | `		PH7_ClassInstanceUnref(pHold);   /* both slots hold their own now */` |
|     3 |  2388 | `	}` |
|     7 |  2389 | `	return PH7_OK;` |
|   228 |  2390 | `}` |
|    46 |  2391 | `static int vm_builtin_IteratorIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  2392 | `{` |
|    48 |  2393 | `	return DualConstruct(pCtx,"IteratorIterator",nArg,apArg);` |
|     2 |  2394 | `}` |
|     6 |  2395 | `static int vm_builtin_FilterIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2396 | `{` |
|     7 |  2397 | `	return DualConstruct(pCtx,"FilterIterator",nArg,apArg);` |
|     1 |  2398 | `}` |
|    12 |  2399 | `static int vm_builtin_CallbackFilterIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2400 | `{` |
|     - |  2401 | `	ph7_class_instance *pThis;` |
|     - |  2402 | `	sxi32 rc;` |
|    13 |  2403 | `	if( nArg > 1 ){` |
|     - |  2404 | ``		/* The shared ZPP screen leaves `callable` to the builtin's own check (a string`` |
|     - |  2405 | `		 * satisfies the declared type; whether it NAMES a function does not), so php's` |
|     - |  2406 | `		 * "must be a valid callback, function "x" not found" only appears if the body` |
|     - |  2407 | `		 * asks for it — as every callback-taking builtin already does. */` |
|    13 |  2408 | `		rc = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",FALSE);` |
|    13 |  2409 | `		if( rc != PH7_OK ){` |
|     3 |  2410 | `			return rc;` |
|     - |  2411 | `		}` |
|     5 |  2412 | `	}` |
|    11 |  2413 | `	rc = DualConstruct(pCtx,"CallbackFilterIterator",nArg,apArg);` |
|    11 |  2414 | `	pThis = PH7_ContextThis(pCtx);` |
|    11 |  2415 | `	if( rc == PH7_OK && pThis && nArg > 1 ){` |
|    11 |  2416 | `		DualSetSlot(pCtx->pVm,pThis,IT_CB,apArg[1]);` |
|     5 |  2417 | `	}` |
|    11 |  2418 | `	return rc;` |
|     7 |  2419 | `}` |
|    10 |  2420 | `static int vm_builtin_InfiniteIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2421 | `{` |
|    11 |  2422 | `	return DualConstruct(pCtx,"InfiniteIterator",nArg,apArg);` |
|     1 |  2423 | `}` |
|    10 |  2424 | `static int vm_builtin_NoRewindIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2425 | `{` |
|    11 |  2426 | `	return DualConstruct(pCtx,"NoRewindIterator",nArg,apArg);` |
|     1 |  2427 | `}` |
|     - |  2428 | `/*` |
|     - |  2429 | `` * php's `spl_dual_it_call_method`: every iterator that extends IteratorIterator --`` |
|     - |  2430 | ` * FilterIterator and its whole family, LimitIterator, CachingIterator,` |
|     - |  2431 | ` * NoRewindIterator, InfiniteIterator, RegexIterator, AppendIterator -- forwards a` |
|     - |  2432 | ` * method it does not itself have to the iterator it WRAPS. Symfony's Finder is built` |
|     - |  2433 | ``  * on that: `ExcludeDirectoryFilterIterator::accept()` calls `$this->getFilename()` `` |
|     - |  2434 | `` * and means the RecursiveDirectoryIterator's, so composer's `dump-autoload` stopped`` |
|     - |  2435 | `` * with `Call to undefined method …::getFilename()` without it.`` |
|     - |  2436 | ` *` |
|     - |  2437 | `` * It is NOT a `__call` method: php's is an internal handler, so`` |
|     - |  2438 | `` * `method_exists($it,'__call')` is false and `get_class_methods()` never lists one.`` |
|     - |  2439 | ` * RecursiveIteratorIterator, RecursiveTreeIterator and MultipleIterator are not dual` |
|     - |  2440 | ` * iterators and forward nothing -- which is why the test is the CLASS, not the` |
|     - |  2441 | ` * presence of an inner slot.` |
|     - |  2442 | ` *` |
|     - |  2443 | ` * Answers 1 with *ppInner / *ppMeth filled when the call should be re-targeted.` |
|     - |  2444 | ` */` |
|   122 |  2445 | `PH7_PRIVATE int PH7_SplOuterForward(ph7_vm *pVm,ph7_class_instance *pThis,SyString *pName,` |
|     - |  2446 | `	ph7_class_instance **ppInner,ph7_class_method **ppMeth)` |
|     4 |  2447 | `{` |
|     - |  2448 | `	ph7_class *pDual;` |
|     - |  2449 | `	ph7_class_instance *pInner;` |
|     - |  2450 | `	ph7_class_method *pMeth;` |
|   126 |  2451 | `	if( pThis == 0 \|\| pName == 0 \|\| pName->nByte < 1 ){` |
|   ! 0 |  2452 | `		return 0;` |
|     - |  2453 | `	}` |
|   126 |  2454 | `	pDual = PH7_VmExtractClass(pVm,"IteratorIterator",sizeof("IteratorIterator")-1,TRUE,0);` |
|   126 |  2455 | `	if( pDual == 0 \|\| pThis->pClass == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pDual) ){` |
|    82 |  2456 | `		return 0;` |
|     - |  2457 | `	}` |
|     - |  2458 | ``	/* php forwards to `inner.zobject` -- what getInnerIterator() answers -- and an`` |
|     - |  2459 | `	 * instance that has none (a fresh AppendIterator) forwards nothing. */` |
|    45 |  2460 | `	pInner = PH7_NativeAttrObj(pThis,IT_IN);` |
|    45 |  2461 | `	if( pInner == 0 \|\| pInner->pClass == 0 ){` |
|     3 |  2462 | `		return 0;` |
|     - |  2463 | `	}` |
|    43 |  2464 | `	pMeth = PH7_ClassExtractMethod(pInner->pClass,pName->zString,pName->nByte);` |
|    43 |  2465 | `	if( pMeth == 0 ){` |
|     - |  2466 | `		/* php reports the OUTER class in that case, which is what the caller's own` |
|     - |  2467 | `		 * undefined-method path already says. */` |
|    19 |  2468 | `		return 0;` |
|     - |  2469 | `	}` |
|    25 |  2470 | `	*ppInner = pInner;` |
|    25 |  2471 | `	*ppMeth = pMeth;` |
|    25 |  2472 | `	return 1;` |
|    65 |  2473 | `}` |
|    42 |  2474 | `static int vm_builtin_Dual_getInnerIterator(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2475 | `{` |
|    43 |  2476 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  2477 | `	ph7_class_instance *pIn;` |
|    21 |  2478 | `	SXUNUSED(nArg);` |
|    21 |  2479 | `	SXUNUSED(apArg);` |
|    43 |  2480 | `	if( !DualReady(pThis) ){` |
|   ! 0 |  2481 | `		return DualNotReady(pCtx);` |
|     - |  2482 | `	}` |
|    43 |  2483 | `	pIn = PH7_NativeAttrObj(pThis,IT_IN);` |
|    43 |  2484 | `	if( pIn ){` |
|    41 |  2485 | `		SplResultBorrowed(pCtx,pIn);` |
|    21 |  2486 | `	}else{` |
|     3 |  2487 | `		ph7_result_null(pCtx);` |
|     - |  2488 | `	}` |
|    43 |  2489 | `	return PH7_OK;` |
|    22 |  2490 | `}` |
|   434 |  2491 | `static int vm_builtin_Dual_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  2492 | `{` |
|   436 |  2493 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   217 |  2494 | `	SXUNUSED(nArg);` |
|   217 |  2495 | `	SXUNUSED(apArg);` |
|   436 |  2496 | `	if( !DualReady(pThis) ){` |
|     3 |  2497 | `		return DualNotReady(pCtx);` |
|     - |  2498 | `	}` |
|   434 |  2499 | `	ph7_result_bool(pCtx,DualFilled(pThis));` |
|   434 |  2500 | `	return PH7_OK;` |
|   219 |  2501 | `}` |
|   404 |  2502 | `static int vm_builtin_Dual_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     4 |  2503 | `{` |
|   202 |  2504 | `	SXUNUSED(nArg);` |
|   202 |  2505 | `	SXUNUSED(apArg);` |
|   408 |  2506 | `	return DualResultSlot(pCtx,IT_CD);` |
|     4 |  2507 | `}` |
|   276 |  2508 | `static int vm_builtin_Dual_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  2509 | `{` |
|   138 |  2510 | `	SXUNUSED(nArg);` |
|   138 |  2511 | `	SXUNUSED(apArg);` |
|   278 |  2512 | `	return DualResultSlot(pCtx,IT_CK);` |
|     2 |  2513 | `}` |
|    24 |  2514 | `static int vm_builtin_IteratorIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  2515 | `{` |
|    26 |  2516 | `	ph7_vm *pVm = pCtx->pVm;` |
|    26 |  2517 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  2518 | `	sxi32 rc;` |
|    12 |  2519 | `	SXUNUSED(nArg);` |
|    12 |  2520 | `	SXUNUSED(apArg);` |
|    26 |  2521 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 |  2522 | `		return DualNotReady(pCtx);` |
|     - |  2523 | `	}` |
|    26 |  2524 | `	rc = DualRewindInner(pVm,pThis);` |
|    26 |  2525 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  2526 | `		return rc;` |
|     - |  2527 | `	}` |
|    26 |  2528 | `	return DualFetch(pVm,pThis,TRUE);` |
|    14 |  2529 | `}` |
|    22 |  2530 | `static int vm_builtin_IteratorIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2531 | `{` |
|    23 |  2532 | `	ph7_vm *pVm = pCtx->pVm;` |
|    23 |  2533 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  2534 | `	sxi32 rc;` |
|    11 |  2535 | `	SXUNUSED(nArg);` |
|    11 |  2536 | `	SXUNUSED(apArg);` |
|    23 |  2537 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 |  2538 | `		return DualNotReady(pCtx);` |
|     - |  2539 | `	}` |
|    23 |  2540 | `	rc = DualNextInner(pVm,pThis);` |
|    23 |  2541 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  2542 | `		return rc;` |
|     - |  2543 | `	}` |
|    23 |  2544 | `	return DualFetch(pVm,pThis,TRUE);` |
|    12 |  2545 | `}` |
|     - |  2546 | `/*` |
|     - |  2547 | ` * FilterIterator. php's spl_filter_it_fetch: fetch, ask accept(), and on a refusal` |
|     - |  2548 | ` * step the INNER on directly — without counting the step, which is why a filtered` |
|     - |  2549 | ` * element does not move current.pos. accept() is called on $this, so a user` |
|     - |  2550 | ` * subclass's body is what decides.` |
|     - |  2551 | ` */` |
|   246 |  2552 | `static sxi32 DualAccept(ph7_vm *pVm,ph7_class_instance *pThis,int *pbAccept)` |
|     2 |  2553 | `{` |
|   248 |  2554 | `	ph7_class_method *pMethod = PH7_ClassExtractMethod(pThis->pClass,"accept",sizeof("accept")-1);` |
|     - |  2555 | `	ph7_value sRes;` |
|     - |  2556 | `	sxi32 rc;` |
|   248 |  2557 | `	*pbAccept = 0;` |
|   248 |  2558 | `	if( pMethod == 0 ){` |
|   ! 0 |  2559 | `		return SXRET_OK;` |
|     - |  2560 | `	}` |
|   248 |  2561 | `	PH7_MemObjInit(pVm,&sRes);` |
|   248 |  2562 | `	rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sRes,0,0);` |
|   248 |  2563 | `	if( rc == SXRET_OK ){` |
|   248 |  2564 | `		PH7_MemObjToBool(&sRes);` |
|   248 |  2565 | `		*pbAccept = sRes.x.iVal != 0;` |
|   123 |  2566 | `	}` |
|   248 |  2567 | `	PH7_MemObjRelease(&sRes);` |
|   248 |  2568 | `	return rc;` |
|   125 |  2569 | `}` |
|   250 |  2570 | `static sxi32 DualFilterFetch(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     2 |  2571 | `{` |
|   215 |  2572 | `	for(;;){` |
|   342 |  2573 | `		int bAccept = 0;` |
|   342 |  2574 | `		sxi32 rc = DualFetch(pVm,pThis,TRUE);` |
|   342 |  2575 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  2576 | `			return rc;` |
|     - |  2577 | `		}` |
|   342 |  2578 | `		if( !DualFilled(pThis) ){` |
|    96 |  2579 | `			break;` |
|     - |  2580 | `		}` |
|   248 |  2581 | `		rc = DualAccept(pVm,pThis,&bAccept);` |
|   248 |  2582 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  2583 | `			return rc;` |
|     - |  2584 | `		}` |
|   248 |  2585 | `		if( bAccept ){` |
|   158 |  2586 | `			return SXRET_OK;` |
|     - |  2587 | `		}` |
|    92 |  2588 | `		rc = DualCall(pVm,pThis,"next",sizeof("next")-1,0);` |
|    92 |  2589 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  2590 | `			return rc;` |
|     - |  2591 | `		}` |
|     2 |  2592 | `	}` |
|    96 |  2593 | `	DualFree(pVm,pThis);` |
|    96 |  2594 | `	return SXRET_OK;` |
|   127 |  2595 | `}` |
|   110 |  2596 | `static int vm_builtin_FilterIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  2597 | `{` |
|   112 |  2598 | `	ph7_vm *pVm = pCtx->pVm;` |
|   112 |  2599 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  2600 | `	sxi32 rc;` |
|    55 |  2601 | `	SXUNUSED(nArg);` |
|    55 |  2602 | `	SXUNUSED(apArg);` |
|   112 |  2603 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 |  2604 | `		return DualNotReady(pCtx);` |
|     - |  2605 | `	}` |
|   112 |  2606 | `	rc = DualRewindInner(pVm,pThis);` |
|   112 |  2607 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  2608 | `		return rc;` |
|     - |  2609 | `	}` |
|   112 |  2610 | `	return DualFilterFetch(pVm,pThis);` |
|    57 |  2611 | `}` |
|   140 |  2612 | `static int vm_builtin_FilterIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  2613 | `{` |
|   142 |  2614 | `	ph7_vm *pVm = pCtx->pVm;` |
|   142 |  2615 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  2616 | `	sxi32 rc;` |
|    70 |  2617 | `	SXUNUSED(nArg);` |
|    70 |  2618 | `	SXUNUSED(apArg);` |
|   142 |  2619 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 |  2620 | `		return DualNotReady(pCtx);` |
|     - |  2621 | `	}` |
|   142 |  2622 | `	rc = DualNextInner(pVm,pThis);` |
|   142 |  2623 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  2624 | `		return rc;` |
|     - |  2625 | `	}` |
|   142 |  2626 | `	return DualFilterFetch(pVm,pThis);` |
|    72 |  2627 | `}` |
|     - |  2628 | `/* CallbackFilterIterator::accept(): the callback sees the CACHED pair and the inner` |
|     - |  2629 | ` * iterator, and an empty cache is refused without calling it at all. */` |
|    38 |  2630 | `static int vm_builtin_CallbackFilterIterator_accept(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2631 | `{` |
|    39 |  2632 | `	ph7_vm *pVm = pCtx->pVm;` |
|    39 |  2633 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  2634 | `	ph7_value *apCall[3];` |
|     - |  2635 | `	ph7_value sInner,sRes,*pCb;` |
|     - |  2636 | `	sxi32 rc;` |
|    19 |  2637 | `	SXUNUSED(nArg);` |
|    19 |  2638 | `	SXUNUSED(apArg);` |
|    39 |  2639 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|     3 |  2640 | `		return DualNotReady(pCtx);` |
|     - |  2641 | `	}` |
|    37 |  2642 | `	if( !DualFilled(pThis) ){` |
|   ! 0 |  2643 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  2644 | `		return PH7_OK;` |
|     - |  2645 | `	}` |
|    37 |  2646 | `	pCb = PH7_NativeAttr(pThis,IT_CB);` |
|    37 |  2647 | `	if( pCb == 0 ){` |
|   ! 0 |  2648 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  2649 | `		return PH7_OK;` |
|     - |  2650 | `	}` |
|    37 |  2651 | `	PH7_MemObjInit(pVm,&sInner);` |
|    37 |  2652 | `	sInner.x.pOther = PH7_NativeAttrObj(pThis,IT_IN);` |
|    37 |  2653 | `	if( sInner.x.pOther ){` |
|    37 |  2654 | `		MemObjSetType(&sInner,MEMOBJ_OBJ);` |
|    37 |  2655 | `		((ph7_class_instance *)sInner.x.pOther)->iRef++;` |
|    18 |  2656 | `	}` |
|    37 |  2657 | `	apCall[0] = PH7_NativeAttr(pThis,IT_CD);` |
|    37 |  2658 | `	apCall[1] = PH7_NativeAttr(pThis,IT_CK);` |
|    37 |  2659 | `	apCall[2] = &sInner;` |
|    37 |  2660 | `	PH7_MemObjInit(pVm,&sRes);` |
|    37 |  2661 | `	rc = PH7_VmCallUserFunction(pVm,pCb,3,apCall,&sRes);` |
|    37 |  2662 | `	PH7_MemObjRelease(&sInner);` |
|    37 |  2663 | `	if( rc == SXRET_OK ){` |
|    37 |  2664 | `		ph7_result_value(pCtx,&sRes);` |
|    18 |  2665 | `	}` |
|    37 |  2666 | `	PH7_MemObjRelease(&sRes);` |
|    37 |  2667 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|    20 |  2668 | `}` |
|     - |  2669 | `/*` |
|     - |  2670 | ` * LimitIterator. The window is (offset, count) over the inner iterator's own` |
|     - |  2671 | `` * positions, and `__cp` counts them: php's valid() is "inside the window AND the`` |
|     - |  2672 | ` * cache is filled", and next() only refills while the window still has room.` |
|     - |  2673 | ` */` |
|    28 |  2674 | `static sxi32 DualLimitSeek(ph7_context *pCtx,ph7_class_instance *pThis,sxi64 iPos)` |
|     1 |  2675 | `{` |
|    29 |  2676 | `	ph7_vm *pVm = pCtx->pVm;` |
|    29 |  2677 | `	sxi64 iOff = PH7_NativeAttrInt(pThis,IT_OFF);` |
|    29 |  2678 | `	sxi64 iLim = PH7_NativeAttrInt(pThis,IT_LIM);` |
|    29 |  2679 | `	ph7_class_instance *pIn = DualDriver(pThis);` |
|     - |  2680 | `	ph7_class *pSeekCls;` |
|     - |  2681 | `	sxi32 rc;` |
|     - |  2682 | `	int bValid;` |
|    29 |  2683 | `	DualFree(pVm,pThis);` |
|    29 |  2684 | `	if( iPos < iOff ){` |
|     7 |  2685 | `		return PH7_VmThrowException(pCtx,"OutOfBoundsException",` |
|     2 |  2686 | `			"Cannot seek to %qd which is below the offset %qd",iPos,iOff);` |
|     - |  2687 | `	}` |
|    25 |  2688 | `	if( iLim != -1 && (iPos - iOff) >= iLim ){` |
|     4 |  2689 | `		return PH7_VmThrowException(pCtx,"OutOfBoundsException",` |
|     1 |  2690 | `			"Cannot seek to %qd which is behind offset %qd plus count %qd",iPos,iOff,iLim);` |
|     - |  2691 | `	}` |
|    23 |  2692 | `	pSeekCls = PH7_VmExtractClass(pVm,"SeekableIterator",sizeof("SeekableIterator")-1,FALSE,0);` |
|    22 |  2693 | `	if( iPos != PH7_NativeAttrInt(pThis,IT_CP) && pIn && pSeekCls` |
|    17 |  2694 | `	 && PH7_VmInstanceOf(pIn->pClass,pSeekCls) ){` |
|     - |  2695 | `		/* The inner knows how to jump: hand it the ABSOLUTE position and let its own` |
|     - |  2696 | `		 * refusal (ArrayIterator's "Seek position N is out of range") surface. */` |
|    17 |  2697 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pIn->pClass,"seek",sizeof("seek")-1);` |
|     - |  2698 | `		ph7_value sPos,*apArg[1];` |
|    17 |  2699 | `		PH7_MemObjInitFromInt(pVm,&sPos,iPos);` |
|    17 |  2700 | `		apArg[0] = &sPos;` |
|    17 |  2701 | `		rc = pMethod ? PH7_VmCallClassMethod(pVm,pIn,pMethod,0,1,apArg) : SXRET_OK;` |
|    17 |  2702 | `		PH7_MemObjRelease(&sPos);` |
|    17 |  2703 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  2704 | `			return rc;` |
|     - |  2705 | `		}` |
|    17 |  2706 | `		PH7_NativeSetAttrInt(pVm,pThis,IT_CP,iPos);` |
|    17 |  2707 | `		rc = DualInnerValid(pVm,pThis,&bValid);` |
|    17 |  2708 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  2709 | `			return rc;` |
|     - |  2710 | `		}` |
|    17 |  2711 | `		if( bValid ){` |
|    17 |  2712 | `			return DualFetch(pVm,pThis,FALSE);` |
|     - |  2713 | `		}` |
|   ! 0 |  2714 | `		return SXRET_OK;` |
|     - |  2715 | `	}` |
|     - |  2716 | `	/* Otherwise emulate: a backward seek is a rewind followed by next() calls. */` |
|     7 |  2717 | `	if( iPos < PH7_NativeAttrInt(pThis,IT_CP) ){` |
|   ! 0 |  2718 | `		rc = DualRewindInner(pVm,pThis);` |
|   ! 0 |  2719 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  2720 | `			return rc;` |
|     - |  2721 | `		}` |
|   ! 0 |  2722 | `	}` |
|     3 |  2723 | `	for(;;){` |
|     7 |  2724 | `		if( iPos <= PH7_NativeAttrInt(pThis,IT_CP) ){` |
|     7 |  2725 | `			break;` |
|     - |  2726 | `		}` |
|   ! 0 |  2727 | `		rc = DualInnerValid(pVm,pThis,&bValid);` |
|   ! 0 |  2728 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  2729 | `			return rc;` |
|     - |  2730 | `		}` |
|   ! 0 |  2731 | `		if( !bValid ){` |
|   ! 0 |  2732 | `			break;` |
|     - |  2733 | `		}` |
|   ! 0 |  2734 | `		rc = DualNextInner(pVm,pThis);` |
|   ! 0 |  2735 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  2736 | `			return rc;` |
|     - |  2737 | `		}` |
|   ! 0 |  2738 | `	}` |
|     7 |  2739 | `	rc = DualInnerValid(pVm,pThis,&bValid);` |
|     7 |  2740 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  2741 | `		return rc;` |
|     - |  2742 | `	}` |
|     7 |  2743 | `	if( bValid ){` |
|     7 |  2744 | `		return DualFetch(pVm,pThis,TRUE);` |
|     - |  2745 | `	}` |
|   ! 0 |  2746 | `	return SXRET_OK;` |
|    15 |  2747 | `}` |
|    48 |  2748 | `static int vm_builtin_LimitIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2749 | `{` |
|    49 |  2750 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  2751 | `	ph7_class_instance *pThis;` |
|    49 |  2752 | `	sxi64 iOff = 0,iLim = -1;` |
|     - |  2753 | `	sxi32 rc;` |
|     - |  2754 | `	/* php screens the two bounds BEFORE it remembers the iterator, so a refused` |
|     - |  2755 | `	 * LimitIterator can still be constructed again. PH7_IntArgResolve is the shared` |
|     - |  2756 | ``	 * `int` ZPP: the central signature screen does not cover a non-numeric STRING`` |
|     - |  2757 | `	 * against an int parameter, and every builtin that takes one calls this. */` |
|    49 |  2758 | `	if( nArg > 1 ){` |
|    41 |  2759 | `		rc = PH7_IntArgResolve(pCtx,apArg[1],ph7_function_name(pCtx),2,"$offset","int",&iOff);` |
|    41 |  2760 | `		if( rc != PH7_OK ){` |
|   ! 0 |  2761 | `			return rc;` |
|     - |  2762 | `		}` |
|    41 |  2763 | `		if( iOff < 0 ){` |
|     7 |  2764 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  2765 | `				"%s(): Argument #2 ($offset) must be greater than or equal to 0",` |
|     2 |  2766 | `				ph7_function_name(pCtx));` |
|     - |  2767 | `		}` |
|    18 |  2768 | `	}` |
|    45 |  2769 | `	if( nArg > 2 ){` |
|    33 |  2770 | `		rc = PH7_IntArgResolve(pCtx,apArg[2],ph7_function_name(pCtx),3,"$limit","int",&iLim);` |
|    33 |  2771 | `		if( rc != PH7_OK ){` |
|   ! 0 |  2772 | `			return rc;` |
|     - |  2773 | `		}` |
|    33 |  2774 | `		if( iLim < -1 ){` |
|     7 |  2775 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  2776 | `				"%s(): Argument #3 ($limit) must be greater than or equal to -1",` |
|     2 |  2777 | `				ph7_function_name(pCtx));` |
|     - |  2778 | `		}` |
|    14 |  2779 | `	}` |
|    41 |  2780 | `	rc = DualConstruct(pCtx,"LimitIterator",nArg,apArg);` |
|    41 |  2781 | `	if( rc != PH7_OK ){` |
|     3 |  2782 | `		return rc;` |
|     - |  2783 | `	}` |
|    39 |  2784 | `	pThis = PH7_ContextThis(pCtx);` |
|    39 |  2785 | `	if( pThis ){` |
|    39 |  2786 | `		PH7_NativeSetAttrInt(pVm,pThis,IT_OFF,iOff);` |
|    39 |  2787 | `		PH7_NativeSetAttrInt(pVm,pThis,IT_LIM,iLim);` |
|    19 |  2788 | `	}` |
|    39 |  2789 | `	return PH7_OK;` |
|    25 |  2790 | `}` |
|    16 |  2791 | `static int vm_builtin_LimitIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2792 | `{` |
|    17 |  2793 | `	ph7_vm *pVm = pCtx->pVm;` |
|    17 |  2794 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  2795 | `	sxi32 rc;` |
|     8 |  2796 | `	SXUNUSED(nArg);` |
|     8 |  2797 | `	SXUNUSED(apArg);` |
|    17 |  2798 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 |  2799 | `		return DualNotReady(pCtx);` |
|     - |  2800 | `	}` |
|    17 |  2801 | `	rc = DualRewindInner(pVm,pThis);` |
|    17 |  2802 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  2803 | `		return rc;` |
|     - |  2804 | `	}` |
|    17 |  2805 | `	return DualLimitSeek(pCtx,pThis,PH7_NativeAttrInt(pThis,IT_OFF));` |
|     9 |  2806 | `}` |
|    40 |  2807 | `static int vm_builtin_LimitIterator_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2808 | `{` |
|    41 |  2809 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  2810 | `	sxi64 iLim;` |
|    20 |  2811 | `	SXUNUSED(nArg);` |
|    20 |  2812 | `	SXUNUSED(apArg);` |
|    41 |  2813 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 |  2814 | `		return DualNotReady(pCtx);` |
|     - |  2815 | `	}` |
|    41 |  2816 | `	iLim = PH7_NativeAttrInt(pThis,IT_LIM);` |
|    81 |  2817 | `	ph7_result_bool(pCtx,` |
|    20 |  2818 | `		(iLim == -1` |
|    33 |  2819 | `		 \|\| (PH7_NativeAttrInt(pThis,IT_CP) - PH7_NativeAttrInt(pThis,IT_OFF)) < iLim)` |
|    35 |  2820 | `		&& DualFilled(pThis));` |
|    41 |  2821 | `	return PH7_OK;` |
|    21 |  2822 | `}` |
|    34 |  2823 | `static int vm_builtin_LimitIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2824 | `{` |
|    35 |  2825 | `	ph7_vm *pVm = pCtx->pVm;` |
|    35 |  2826 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  2827 | `	sxi64 iLim;` |
|     - |  2828 | `	sxi32 rc;` |
|    17 |  2829 | `	SXUNUSED(nArg);` |
|    17 |  2830 | `	SXUNUSED(apArg);` |
|    35 |  2831 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 |  2832 | `		return DualNotReady(pCtx);` |
|     - |  2833 | `	}` |
|    35 |  2834 | `	rc = DualNextInner(pVm,pThis);` |
|    35 |  2835 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  2836 | `		return rc;` |
|     - |  2837 | `	}` |
|    35 |  2838 | `	iLim = PH7_NativeAttrInt(pThis,IT_LIM);` |
|    34 |  2839 | `	if( iLim == -1` |
|    30 |  2840 | `	 \|\| (PH7_NativeAttrInt(pThis,IT_CP) - PH7_NativeAttrInt(pThis,IT_OFF)) < iLim ){` |
|    25 |  2841 | `		return DualFetch(pVm,pThis,TRUE);` |
|     - |  2842 | `	}` |
|    11 |  2843 | `	return PH7_OK;   /* past the window: the cache stays empty, so current() is null */` |
|    18 |  2844 | `}` |
|    12 |  2845 | `static int vm_builtin_LimitIterator_seek(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2846 | `{` |
|    13 |  2847 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    13 |  2848 | `	sxi64 iPos = 0;` |
|     - |  2849 | `	sxi32 rc;` |
|    13 |  2850 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 |  2851 | `		return DualNotReady(pCtx);` |
|     - |  2852 | `	}` |
|    13 |  2853 | `	if( nArg > 0 ){` |
|    13 |  2854 | `		rc = PH7_IntArgResolve(pCtx,apArg[0],ph7_function_name(pCtx),1,"$offset","int",&iPos);` |
|    13 |  2855 | `		if( rc != PH7_OK ){` |
|   ! 0 |  2856 | `			return rc;` |
|     - |  2857 | `		}` |
|     6 |  2858 | `	}` |
|    13 |  2859 | `	rc = DualLimitSeek(pCtx,pThis,iPos);` |
|    13 |  2860 | `	if( rc != PH7_OK ){` |
|     7 |  2861 | `		return rc;` |
|     - |  2862 | `	}` |
|     7 |  2863 | `	ph7_result_int64(pCtx,PH7_NativeAttrInt(pThis,IT_CP));` |
|     7 |  2864 | `	return PH7_OK;` |
|     7 |  2865 | `}` |
|    18 |  2866 | `static int vm_builtin_LimitIterator_getPosition(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2867 | `{` |
|    19 |  2868 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     9 |  2869 | `	SXUNUSED(nArg);` |
|     9 |  2870 | `	SXUNUSED(apArg);` |
|    19 |  2871 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 |  2872 | `		return DualNotReady(pCtx);` |
|     - |  2873 | `	}` |
|    19 |  2874 | `	ph7_result_int64(pCtx,PH7_NativeAttrInt(pThis,IT_CP));` |
|    19 |  2875 | `	return PH7_OK;` |
|    10 |  2876 | `}` |
|     - |  2877 | `/* InfiniteIterator::next(): step, and on exhaustion rewind and step into the head` |
|     - |  2878 | ` * again. Both refills are php's check_more=0 form — the validity was just tested. */` |
|    16 |  2879 | `static int vm_builtin_InfiniteIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2880 | `{` |
|    17 |  2881 | `	ph7_vm *pVm = pCtx->pVm;` |
|    17 |  2882 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  2883 | `	sxi32 rc;` |
|     - |  2884 | `	int bValid;` |
|     8 |  2885 | `	SXUNUSED(nArg);` |
|     8 |  2886 | `	SXUNUSED(apArg);` |
|    17 |  2887 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 |  2888 | `		return DualNotReady(pCtx);` |
|     - |  2889 | `	}` |
|    17 |  2890 | `	rc = DualNextInner(pVm,pThis);` |
|    17 |  2891 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  2892 | `		return rc;` |
|     - |  2893 | `	}` |
|    17 |  2894 | `	rc = DualInnerValid(pVm,pThis,&bValid);` |
|    17 |  2895 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  2896 | `		return rc;` |
|     - |  2897 | `	}` |
|    17 |  2898 | `	if( !bValid ){` |
|     9 |  2899 | `		rc = DualRewindInner(pVm,pThis);` |
|     9 |  2900 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  2901 | `			return rc;` |
|     - |  2902 | `		}` |
|     9 |  2903 | `		rc = DualInnerValid(pVm,pThis,&bValid);` |
|     9 |  2904 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  2905 | `			return rc;` |
|     - |  2906 | `		}` |
|     4 |  2907 | `	}` |
|    17 |  2908 | `	if( bValid ){` |
|    17 |  2909 | `		return DualFetch(pVm,pThis,FALSE);` |
|     - |  2910 | `	}` |
|   ! 0 |  2911 | `	return PH7_OK;` |
|     9 |  2912 | `}` |
|     - |  2913 | `/*` |
|     - |  2914 | ` * NoRewindIterator. Its rewind() does nothing at all — and because the four` |
|     - |  2915 | ` * accessors read the INNER live rather than the cache, an instance is usable` |
|     - |  2916 | ` * without ever being rewound, which is the entire point of the class.` |
|     - |  2917 | ` */` |
|     4 |  2918 | `static int vm_builtin_NoRewindIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2919 | `{` |
|     2 |  2920 | `	SXUNUSED(nArg);` |
|     2 |  2921 | `	SXUNUSED(apArg);` |
|     5 |  2922 | `	if( PH7_ContextThis(pCtx) == 0 \|\| DualDriver(PH7_ContextThis(pCtx)) == 0 ){` |
|   ! 0 |  2923 | `		return DualNotReady(pCtx);` |
|     - |  2924 | `	}` |
|     5 |  2925 | `	return PH7_OK;` |
|     3 |  2926 | `}` |
|    14 |  2927 | `static int vm_builtin_NoRewindIterator_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2928 | `{` |
|    15 |  2929 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  2930 | `	sxi32 rc;` |
|     - |  2931 | `	int bValid;` |
|     7 |  2932 | `	SXUNUSED(nArg);` |
|     7 |  2933 | `	SXUNUSED(apArg);` |
|    15 |  2934 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 |  2935 | `		return DualNotReady(pCtx);` |
|     - |  2936 | `	}` |
|    15 |  2937 | `	rc = DualInnerValid(pCtx->pVm,pThis,&bValid);` |
|    15 |  2938 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  2939 | `		return rc;` |
|     - |  2940 | `	}` |
|    15 |  2941 | `	ph7_result_bool(pCtx,bValid);` |
|    15 |  2942 | `	return PH7_OK;` |
|     8 |  2943 | `}` |
|    26 |  2944 | `static int DualForwardLive(ph7_context *pCtx,const char *zName,sxu32 nLen,int bResult)` |
|     1 |  2945 | `{` |
|    27 |  2946 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  2947 | `	ph7_value sVal;` |
|     - |  2948 | `	sxi32 rc;` |
|    27 |  2949 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 |  2950 | `		return DualNotReady(pCtx);` |
|     - |  2951 | `	}` |
|    27 |  2952 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|    27 |  2953 | `	rc = DualCall(pCtx->pVm,pThis,zName,nLen,bResult ? &sVal : 0);` |
|    27 |  2954 | `	if( rc == SXRET_OK && bResult ){` |
|    17 |  2955 | `		ph7_result_value(pCtx,&sVal);` |
|     8 |  2956 | `	}` |
|    27 |  2957 | `	PH7_MemObjRelease(&sVal);` |
|    27 |  2958 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|    14 |  2959 | `}` |
|    10 |  2960 | `static int vm_builtin_NoRewindIterator_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2961 | `{` |
|     5 |  2962 | `	SXUNUSED(nArg);` |
|     5 |  2963 | `	SXUNUSED(apArg);` |
|    11 |  2964 | `	return DualForwardLive(pCtx,"current",sizeof("current")-1,TRUE);` |
|     1 |  2965 | `}` |
|     6 |  2966 | `static int vm_builtin_NoRewindIterator_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2967 | `{` |
|     3 |  2968 | `	SXUNUSED(nArg);` |
|     3 |  2969 | `	SXUNUSED(apArg);` |
|     7 |  2970 | `	return DualForwardLive(pCtx,"key",sizeof("key")-1,TRUE);` |
|     1 |  2971 | `}` |
|    10 |  2972 | `static int vm_builtin_NoRewindIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2973 | `{` |
|     5 |  2974 | `	SXUNUSED(nArg);` |
|     5 |  2975 | `	SXUNUSED(apArg);` |
|    11 |  2976 | `	return DualForwardLive(pCtx,"next",sizeof("next")-1,FALSE);` |
|     1 |  2977 | `}` |
|     - |  2978 | `/* EmptyIterator: valid() is false forever, and asking for a value or a key is a` |
|     - |  2979 | ` * BadMethodCallException rather than a null. */` |
|     4 |  2980 | `static int vm_builtin_EmptyIterator_nop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2981 | `{` |
|     2 |  2982 | `	SXUNUSED(nArg);` |
|     2 |  2983 | `	SXUNUSED(apArg);` |
|     5 |  2984 | `	ph7_result_null(pCtx);` |
|     5 |  2985 | `	return PH7_OK;` |
|     1 |  2986 | `}` |
|     8 |  2987 | `static int vm_builtin_EmptyIterator_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2988 | `{` |
|     4 |  2989 | `	SXUNUSED(nArg);` |
|     4 |  2990 | `	SXUNUSED(apArg);` |
|     9 |  2991 | `	ph7_result_bool(pCtx,0);` |
|     9 |  2992 | `	return PH7_OK;` |
|     1 |  2993 | `}` |
|     4 |  2994 | `static int vm_builtin_EmptyIterator_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2995 | `{` |
|     2 |  2996 | `	SXUNUSED(nArg);` |
|     2 |  2997 | `	SXUNUSED(apArg);` |
|     5 |  2998 | `	return PH7_VmThrowException(pCtx,"BadMethodCallException",` |
|     - |  2999 | `		"Accessing the value of an EmptyIterator");` |
|     1 |  3000 | `}` |
|     4 |  3001 | `static int vm_builtin_EmptyIterator_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3002 | `{` |
|     2 |  3003 | `	SXUNUSED(nArg);` |
|     2 |  3004 | `	SXUNUSED(apArg);` |
|     5 |  3005 | `	return PH7_VmThrowException(pCtx,"BadMethodCallException",` |
|     - |  3006 | `		"Accessing the key of an EmptyIterator");` |
|     1 |  3007 | `}` |
|     - |  3008 | `/*` |
|     - |  3009 | ` * ---------------------------------------------------------------------------` |
|     - |  3010 | ` * RegexIterator: a FilterIterator whose accept() runs a regex over the CACHE.` |
|     - |  3011 | ` *` |
|     - |  3012 | ` * Everything that matters here follows from the cache the decorators already` |
|     - |  3013 | `` * keep. php's accept() reads `current.data` (or `current.key` under USE_KEY) and`` |
|     - |  3014 | ` * -- in every mode but MATCH -- WRITES THE RESULT BACK INTO THAT SAME SLOT, which` |
|     - |  3015 | ` * is why the class declares no current() of its own: the inherited one already` |
|     - |  3016 | `` * answers the transformed value. The PHP chunk kept a private `$__cur` and`` |
|     - |  3017 | ` * overrode current(), and that is where its two wrong answers came from: a` |
|     - |  3018 | ` * REPLACE under USE_KEY must replace into the KEY (php leaves current() alone),` |
|     - |  3019 | ` * and an ARRAY current() is refused outright rather than matched as the string` |
|     - |  3020 | ` * "Array".` |
|     - |  3021 | ` */` |
|     - |  3022 | `#define IT_RE  "__re"   /* php's u.regex.regex: the pattern, as given */` |
|     - |  3023 | `#define IT_RM  "__rm"   /* php's u.regex.mode */` |
|     - |  3024 | `#define IT_RF  "__rf"   /* php's u.regex.flags (USE_KEY / INVERT_MATCH) */` |
|     - |  3025 | `#define IT_RP  "__rp"   /* php's u.regex.preg_flags */` |
|     - |  3026 | `#define REGIT_USE_KEY  1` |
|     - |  3027 | `#define REGIT_INVERTED 2` |
|     - |  3028 | `/* php's ValueError for a mode outside the five. The constructor and setMode()` |
|     - |  3029 | ` * word it identically and differ only in the argument they name. */` |
|     8 |  3030 | `static int RegitBadMode(ph7_context *pCtx,const char *zWhere)` |
|     1 |  3031 | `{` |
|    13 |  3032 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  3033 | `		"%s must be RegexIterator::MATCH, RegexIterator::GET_MATCH, "` |
|     - |  3034 | `		"RegexIterator::ALL_MATCHES, RegexIterator::SPLIT, or RegexIterator::REPLACE",` |
|     4 |  3035 | `		zWhere);` |
|     1 |  3036 | `}` |
|     - |  3037 | `/*` |
|     - |  3038 | ` * The constructor both regex iterators run. Every diagnostic it raises names the` |
|     - |  3039 | ` * class that was CONSTRUCTED (php's are its own method's scope), so the owner is a` |
|     - |  3040 | ` * parameter rather than a literal -- and the ValueError still names the mode` |
|     - |  3041 | ` * constants on RegexIterator, which is where php declares them.` |
|     - |  3042 | ` */` |
|    96 |  3043 | `static int RegitConstruct(ph7_context *pCtx,const char *zOwner,int nArg,ph7_value **apArg)` |
|     2 |  3044 | `{` |
|    98 |  3045 | `	ph7_vm *pVm = pCtx->pVm;` |
|    98 |  3046 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  3047 | `	const char *zPat;` |
|     - |  3048 | `	char zWhere[128];` |
|     - |  3049 | `	int nPat;` |
|    98 |  3050 | `	sxi64 iMode = PH7_REGIT_MATCH;` |
|     - |  3051 | `	char zErr[288];` |
|     - |  3052 | `	sxi32 rc;` |
|    98 |  3053 | `	if( pThis == 0 ){` |
|   ! 0 |  3054 | `		return PH7_OK;` |
|     - |  3055 | `	}` |
|    98 |  3056 | `	if( PH7_NativeAttrObj(pThis,IT_IN) != 0 ){` |
|     - |  3057 | `		/* php makes the "already built" refusal before it reads any argument, so` |
|     - |  3058 | `		 * hand this straight to the shared constructor, which words it. */` |
|   ! 0 |  3059 | `		return DualConstruct(pCtx,zOwner,nArg,apArg);` |
|     - |  3060 | `	}` |
|    98 |  3061 | `	if( nArg < 2 ){` |
|   ! 0 |  3062 | `		return PH7_OK;   /* the arity screen already refused */` |
|     - |  3063 | `	}` |
|    98 |  3064 | `	if( nArg > 2 ){` |
|    52 |  3065 | `		iMode = ph7_value_to_int(apArg[2]);` |
|    25 |  3066 | `	}` |
|    98 |  3067 | `	if( iMode < PH7_REGIT_MATCH \|\| iMode > PH7_REGIT_REPLACE ){` |
|     5 |  3068 | `		SyBufferFormat(zWhere,sizeof(zWhere),"%s::__construct(): Argument #3 ($mode)",zOwner);` |
|     5 |  3069 | `		return RegitBadMode(pCtx,zWhere);` |
|     - |  3070 | `	}` |
|     - |  3071 | `	/* php compiles the pattern HERE and promotes pcre's warning to an` |
|     - |  3072 | ``	 * InvalidArgumentException, so a bad pattern is refused by `new` rather than`` |
|     - |  3073 | `	 * warning once per element from accept(). */` |
|    94 |  3074 | `	zPat = ph7_value_to_string(apArg[1],&nPat);` |
|    94 |  3075 | `	if( !PH7_PcrePatternCheck(pVm,zPat,nPat,zErr,sizeof(zErr)) ){` |
|    10 |  3076 | `		return PH7_VmThrowException(pCtx,"InvalidArgumentException",` |
|     3 |  3077 | `			"%s::__construct(): %s",zOwner,zErr);` |
|     - |  3078 | `	}` |
|    88 |  3079 | `	rc = DualConstruct(pCtx,zOwner,nArg,apArg);` |
|    88 |  3080 | `	if( rc != PH7_OK ){` |
|   ! 0 |  3081 | `		return rc;` |
|     - |  3082 | `	}` |
|    88 |  3083 | `	PH7_NativeSetAttrStr(pVm,pThis,IT_RE,zPat,(sxu32)nPat);` |
|    88 |  3084 | `	PH7_NativeSetAttrInt(pVm,pThis,IT_RM,iMode);` |
|    88 |  3085 | `	PH7_NativeSetAttrInt(pVm,pThis,IT_RF,nArg > 3 ? ph7_value_to_int(apArg[3]) : 0);` |
|    88 |  3086 | `	PH7_NativeSetAttrInt(pVm,pThis,IT_RP,nArg > 4 ? ph7_value_to_int(apArg[4]) : 0);` |
|    88 |  3087 | `	return PH7_OK;` |
|    50 |  3088 | `}` |
|    70 |  3089 | `static int vm_builtin_RegexIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  3090 | `{` |
|    72 |  3091 | `	return RegitConstruct(pCtx,"RegexIterator",nArg,apArg);` |
|     2 |  3092 | `}` |
|   106 |  3093 | `static int vm_builtin_RegexIterator_accept(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  3094 | `{` |
|   108 |  3095 | `	ph7_vm *pVm = pCtx->pVm;` |
|   108 |  3096 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  3097 | `	ph7_value sSubject,sPattern,sRepl,sOut,*pSlot;` |
|   108 |  3098 | `	int iMode,iFlags,bUseKey,bOk = 0;` |
|     - |  3099 | `	sxi32 rc;` |
|    53 |  3100 | `	SXUNUSED(nArg);` |
|    53 |  3101 | `	SXUNUSED(apArg);` |
|   108 |  3102 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 |  3103 | `		return DualNotReady(pCtx);` |
|     - |  3104 | `	}` |
|   108 |  3105 | `	if( !DualFilled(pThis) ){` |
|     - |  3106 | `		/* Nothing has been fetched: php answers false without touching the regex. */` |
|     3 |  3107 | `		ph7_result_bool(pCtx,0);` |
|     3 |  3108 | `		return PH7_OK;` |
|     - |  3109 | `	}` |
|   106 |  3110 | `	iMode = (int)PH7_NativeAttrInt(pThis,IT_RM);` |
|   106 |  3111 | `	iFlags = (int)PH7_NativeAttrInt(pThis,IT_RF);` |
|   106 |  3112 | `	bUseKey = (iFlags & REGIT_USE_KEY) != 0;` |
|   106 |  3113 | `	pSlot = PH7_NativeAttr(pThis,bUseKey ? IT_CK : IT_CD);` |
|   106 |  3114 | `	if( pSlot == 0 ){` |
|   ! 0 |  3115 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  3116 | `		return PH7_OK;` |
|     - |  3117 | `	}` |
|   106 |  3118 | `	if( !bUseKey && (pSlot->iFlags & MEMOBJ_HASHMAP) ){` |
|     - |  3119 | ``		/* php's `Z_TYPE(current.data) == IS_ARRAY -> RETURN_FALSE`, ahead of every`` |
|     - |  3120 | `		 * mode. The chunk's (string)$subject matched the word "Array" instead. */` |
|     5 |  3121 | `		ph7_result_bool(pCtx,0);` |
|     5 |  3122 | `		return PH7_OK;` |
|     - |  3123 | `	}` |
|     - |  3124 | `	/* Take the subject as a VALUE: the slot pointer does not survive a call into` |
|     - |  3125 | `	 * user code, and an object subject reaches __toString() below. */` |
|   102 |  3126 | `	PH7_MemObjInit(pVm,&sSubject);` |
|   102 |  3127 | `	PH7_MemObjStore(pSlot,&sSubject);` |
|   102 |  3128 | `	rc = PH7_MemObjToStringUV(&sSubject);` |
|   102 |  3129 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  3130 | `		PH7_MemObjRelease(&sSubject);` |
|   ! 0 |  3131 | `		return rc;` |
|     - |  3132 | `	}` |
|   102 |  3133 | `	PH7_MemObjInit(pVm,&sPattern);` |
|   102 |  3134 | `	PH7_MemObjInit(pVm,&sRepl);` |
|   102 |  3135 | `	PH7_MemObjInit(pVm,&sOut);` |
|     - |  3136 | `	{` |
|   102 |  3137 | `		ph7_value *pRe = PH7_NativeAttr(pThis,IT_RE);` |
|   102 |  3138 | `		if( pRe ){` |
|   102 |  3139 | `			PH7_MemObjStore(pRe,&sPattern);` |
|    50 |  3140 | `		}` |
|     - |  3141 | `	}` |
|   102 |  3142 | `	if( iMode == PH7_REGIT_REPLACE ){` |
|     - |  3143 | `		/* php reads the public $replacement property, whose declared ?string makes` |
|     - |  3144 | `		 * the read total: a null answers the empty string. */` |
|    17 |  3145 | `		ph7_value *pRepl = PH7_NativeAttr(pThis,"replacement");` |
|    17 |  3146 | `		if( pRepl ){` |
|    17 |  3147 | `			PH7_MemObjStore(pRepl,&sRepl);` |
|     8 |  3148 | `		}` |
|    17 |  3149 | `		PH7_MemObjToString(&sRepl);` |
|     8 |  3150 | `	}` |
|   152 |  3151 | `	rc = PH7_PcreRegitApply(pCtx,iMode,&sPattern,&sSubject,` |
|   100 |  3152 | `		(int)PH7_NativeAttrInt(pThis,IT_RP),&sRepl,&sOut,&bOk);` |
|   102 |  3153 | `	if( rc == PH7_OK && iMode != PH7_REGIT_MATCH ){` |
|     - |  3154 | `		/* php writes the transformed value over the cached pair -- into the KEY when` |
|     - |  3155 | `		 * a REPLACE is keyed, into current() otherwise -- so the inherited current()` |
|     - |  3156 | `		 * and key() present it. */` |
|    47 |  3157 | `		DualSetSlot(pVm,pThis,(iMode == PH7_REGIT_REPLACE && bUseKey) ? IT_CK : IT_CD,&sOut);` |
|    23 |  3158 | `	}` |
|   102 |  3159 | `	PH7_MemObjRelease(&sSubject);` |
|   102 |  3160 | `	PH7_MemObjRelease(&sPattern);` |
|   102 |  3161 | `	PH7_MemObjRelease(&sRepl);` |
|   102 |  3162 | `	PH7_MemObjRelease(&sOut);` |
|   102 |  3163 | `	if( rc != PH7_OK ){` |
|   ! 0 |  3164 | `		return rc;` |
|     - |  3165 | `	}` |
|   102 |  3166 | `	ph7_result_bool(pCtx,(iFlags & REGIT_INVERTED) ? !bOk : bOk);` |
|   102 |  3167 | `	return PH7_OK;` |
|    55 |  3168 | `}` |
|     6 |  3169 | `static int vm_builtin_RegexIterator_getRegex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3170 | `{` |
|     7 |  3171 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  3172 | `	ph7_value *pRe;` |
|     3 |  3173 | `	SXUNUSED(nArg);` |
|     3 |  3174 | `	SXUNUSED(apArg);` |
|     7 |  3175 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 |  3176 | `		return DualNotReady(pCtx);` |
|     - |  3177 | `	}` |
|     7 |  3178 | `	pRe = PH7_NativeAttr(pThis,IT_RE);` |
|     7 |  3179 | `	if( pRe ){` |
|     7 |  3180 | `		ph7_result_value(pCtx,pRe);` |
|     3 |  3181 | `	}` |
|     7 |  3182 | `	return PH7_OK;` |
|     4 |  3183 | `}` |
|     - |  3184 | `/* The three getters and the three setters are one pair per slot; only setMode()` |
|     - |  3185 | ` * screens its value, which is php's own asymmetry (setFlags/setPregFlags take` |
|     - |  3186 | ` * any integer). */` |
|    24 |  3187 | `static int RegitGet(ph7_context *pCtx,const char *zSlot)` |
|     1 |  3188 | `{` |
|    25 |  3189 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    25 |  3190 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 |  3191 | `		return DualNotReady(pCtx);` |
|     - |  3192 | `	}` |
|    25 |  3193 | `	ph7_result_int64(pCtx,PH7_NativeAttrInt(pThis,zSlot));` |
|    25 |  3194 | `	return PH7_OK;` |
|    13 |  3195 | `}` |
|     8 |  3196 | `static int RegitSet(ph7_context *pCtx,int nArg,ph7_value **apArg,const char *zSlot)` |
|     1 |  3197 | `{` |
|     9 |  3198 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     9 |  3199 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 |  3200 | `		return DualNotReady(pCtx);` |
|     - |  3201 | `	}` |
|     9 |  3202 | `	if( nArg > 0 ){` |
|     9 |  3203 | `		PH7_NativeSetAttrInt(pCtx->pVm,pThis,zSlot,ph7_value_to_int64(apArg[0]));` |
|     4 |  3204 | `	}` |
|     9 |  3205 | `	return PH7_OK;` |
|     5 |  3206 | `}` |
|     8 |  3207 | `static int vm_builtin_RegexIterator_getMode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3208 | `{` |
|     4 |  3209 | `	SXUNUSED(nArg);` |
|     4 |  3210 | `	SXUNUSED(apArg);` |
|     9 |  3211 | `	return RegitGet(pCtx,IT_RM);` |
|     1 |  3212 | `}` |
|     8 |  3213 | `static int vm_builtin_RegexIterator_setMode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3214 | `{` |
|     9 |  3215 | `	if( nArg > 0 ){` |
|     9 |  3216 | `		sxi64 iMode = ph7_value_to_int64(apArg[0]);` |
|     9 |  3217 | `		if( iMode < PH7_REGIT_MATCH \|\| iMode > PH7_REGIT_REPLACE ){` |
|     - |  3218 | `			/* php screens the VALUE before it even fetches the object. */` |
|     5 |  3219 | `			return RegitBadMode(pCtx,"RegexIterator::setMode(): Argument #1 ($mode)");` |
|     - |  3220 | `		}` |
|     2 |  3221 | `	}` |
|     5 |  3222 | `	return RegitSet(pCtx,nArg,apArg,IT_RM);` |
|     5 |  3223 | `}` |
|     8 |  3224 | `static int vm_builtin_RegexIterator_getFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3225 | `{` |
|     4 |  3226 | `	SXUNUSED(nArg);` |
|     4 |  3227 | `	SXUNUSED(apArg);` |
|     9 |  3228 | `	return RegitGet(pCtx,IT_RF);` |
|     1 |  3229 | `}` |
|     2 |  3230 | `static int vm_builtin_RegexIterator_setFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3231 | `{` |
|     3 |  3232 | `	return RegitSet(pCtx,nArg,apArg,IT_RF);` |
|     1 |  3233 | `}` |
|     8 |  3234 | `static int vm_builtin_RegexIterator_getPregFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3235 | `{` |
|     4 |  3236 | `	SXUNUSED(nArg);` |
|     4 |  3237 | `	SXUNUSED(apArg);` |
|     9 |  3238 | `	return RegitGet(pCtx,IT_RP);` |
|     1 |  3239 | `}` |
|     2 |  3240 | `static int vm_builtin_RegexIterator_setPregFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3241 | `{` |
|     3 |  3242 | `	return RegitSet(pCtx,nArg,apArg,IT_RP);` |
|     1 |  3243 | `}` |
|     - |  3244 | `/*` |
|     - |  3245 | ` * ---------------------------------------------------------------------------` |
|     - |  3246 | ` * AppendIterator: an IteratorIterator whose inner iterator is whatever entry a` |
|     - |  3247 | ` * real ArrayIterator is currently pointing at.` |
|     - |  3248 | ` *` |
|     - |  3249 | `` * php keeps the appended iterators in an actual `ArrayIterator` INSTANCE`` |
|     - |  3250 | `` * (`u.append.zarrayit`, the object getArrayIterator() hands out) and walks it with`` |
|     - |  3251 | `` * a cursor over the SAME storage (`u.append.iterator`). Both halves are`` |
|     - |  3252 | ` * php-visible and the chunk had neither: it kept a private PHP array and answered` |
|     - |  3253 | ` * getArrayIterator() with a fresh ArrayIterator over a COPY, so appending through` |
|     - |  3254 | `` * the returned object iterated nothing and `$ai->rewind()` did not restart the`` |
|     - |  3255 | `` * walk. The list cursor here is that one ArrayIterator's own `pCur`, driven`` |
|     - |  3256 | ` * directly the way php drives its iterator funcs -- not through the class's` |
|     - |  3257 | ` * methods, which php does not call either.` |
|     - |  3258 | ` */` |
|   448 |  3259 | `static ph7_class_instance * ApList(ph7_class_instance *pThis)` |
|     1 |  3260 | `{` |
|   449 |  3261 | `	return pThis ? PH7_NativeAttrObj(pThis,AP_LIST) : 0;` |
|     1 |  3262 | `}` |
|   360 |  3263 | `static ph7_hashmap * ApMap(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  3264 | `{` |
|   361 |  3265 | `	return SplStore(pVm,ApList(pThis));` |
|     1 |  3266 | `}` |
|     - |  3267 | `/* The iterator the list cursor points at, or 0 past the end. */` |
|   120 |  3268 | `static ph7_class_instance * ApCurrent(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  3269 | `{` |
|   121 |  3270 | `	ph7_hashmap *pMap = ApMap(pVm,pThis);` |
|   121 |  3271 | `	ph7_value *pVal = (pMap && pMap->pCur) ? HashmapExtractNodeValue(pMap->pCur) : 0;` |
|   121 |  3272 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|    31 |  3273 | `		return 0;` |
|     - |  3274 | `	}` |
|    91 |  3275 | `	return (ph7_class_instance *)pVal->x.pOther;` |
|    61 |  3276 | `}` |
|    28 |  3277 | `static void ApListRewind(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  3278 | `{` |
|    29 |  3279 | `	ph7_hashmap *pMap = ApMap(pVm,pThis);` |
|    29 |  3280 | `	if( pMap ){` |
|    29 |  3281 | `		pMap->pCur = pMap->pFirst;` |
|    14 |  3282 | `	}` |
|    29 |  3283 | `}` |
|    50 |  3284 | `static void ApListNext(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  3285 | `{` |
|    51 |  3286 | `	ph7_hashmap *pMap = ApMap(pVm,pThis);` |
|    51 |  3287 | `	if( pMap && pMap->pCur ){` |
|    51 |  3288 | `		pMap->pCur = pMap->pCur->pPrev;   /* insertion order: pFirst, then the pPrev chain */` |
|    25 |  3289 | `	}` |
|    51 |  3290 | `}` |
|     - |  3291 | `/*` |
|     - |  3292 | ` * php's spl_append_it_next_iterator: drop the cache and the current inner, then` |
|     - |  3293 | ` * adopt whatever the list cursor points at (rewound). *pbOk is php's SUCCESS --` |
|     - |  3294 | ` * false means the list is exhausted and this iterator has nothing left.` |
|     - |  3295 | ` */` |
|   120 |  3296 | `static sxi32 ApAdoptCurrent(ph7_vm *pVm,ph7_class_instance *pThis,int *pbOk)` |
|     1 |  3297 | `{` |
|     - |  3298 | `	ph7_class_instance *pIt;` |
|   121 |  3299 | `	*pbOk = 0;` |
|   121 |  3300 | `	DualFree(pVm,pThis);` |
|   121 |  3301 | `	PH7_NativeSetAttrObj(pVm,pThis,IT_IN,0);` |
|   121 |  3302 | `	PH7_NativeSetAttrObj(pVm,pThis,IT_IT,0);` |
|   121 |  3303 | `	pIt = ApCurrent(pVm,pThis);` |
|   121 |  3304 | `	if( pIt == 0 ){` |
|    31 |  3305 | `		return SXRET_OK;` |
|     - |  3306 | `	}` |
|    91 |  3307 | `	PH7_NativeSetAttrObj(pVm,pThis,IT_IN,pIt);` |
|    91 |  3308 | `	PH7_NativeSetAttrObj(pVm,pThis,IT_IT,pIt);` |
|    91 |  3309 | `	*pbOk = 1;` |
|    91 |  3310 | `	return DualRewindInner(pVm,pThis);` |
|    61 |  3311 | `}` |
|     - |  3312 | `/*` |
|     - |  3313 | ` * php's spl_append_it_fetch: step over every exhausted inner iterator, then fill` |
|     - |  3314 | ` * the cache without re-asking valid() (php's check_more = 0 -- the loop above just` |
|     - |  3315 | ` * established it).` |
|     - |  3316 | ` */` |
|   116 |  3317 | `static sxi32 ApFetch(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  3318 | `{` |
|    78 |  3319 | `	for(;;){` |
|   137 |  3320 | `		int bValid = 0, bOk = 0;` |
|   137 |  3321 | `		sxi32 rc = DualInnerValid(pVm,pThis,&bValid);` |
|   137 |  3322 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  3323 | `			return rc;` |
|     - |  3324 | `		}` |
|   137 |  3325 | `		if( bValid ){` |
|    87 |  3326 | `			break;` |
|     - |  3327 | `		}` |
|    51 |  3328 | `		ApListNext(pVm,pThis);` |
|    51 |  3329 | `		rc = ApAdoptCurrent(pVm,pThis,&bOk);` |
|    51 |  3330 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  3331 | `			return rc;` |
|     - |  3332 | `		}` |
|    51 |  3333 | `		if( !bOk ){` |
|    31 |  3334 | `			return SXRET_OK;   /* nothing left: the cache stays empty and valid() is false */` |
|     - |  3335 | `		}` |
|     1 |  3336 | `	}` |
|    87 |  3337 | `	return DualFetch(pVm,pThis,FALSE);` |
|    59 |  3338 | `}` |
|    50 |  3339 | `static int vm_builtin_AppendIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3340 | `{` |
|    51 |  3341 | `	ph7_vm *pVm = pCtx->pVm;` |
|    51 |  3342 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  3343 | `	ph7_class_instance *pList;` |
|     - |  3344 | `	ph7_class *pClass;` |
|     - |  3345 | `	ph7_class_method *pCons;` |
|    25 |  3346 | `	SXUNUSED(nArg);` |
|    25 |  3347 | `	SXUNUSED(apArg);` |
|    51 |  3348 | `	if( pThis == 0 ){` |
|   ! 0 |  3349 | `		return PH7_OK;` |
|     - |  3350 | `	}` |
|    51 |  3351 | `	if( ApList(pThis) != 0 ){` |
|     - |  3352 | `		/* php's "already built" refusal, worded from the DECLARING class as everywhere` |
|     - |  3353 | `		 * else in the family. */` |
|   ! 0 |  3354 | `		return PH7_VmThrowException(pCtx,"BadMethodCallException",` |
|     - |  3355 | `			"AppendIterator::getIterator() must be called exactly once per instance");` |
|     - |  3356 | `	}` |
|    51 |  3357 | `	pClass = PH7_VmExtractClass(pVm,"ArrayIterator",sizeof("ArrayIterator")-1,FALSE,0);` |
|    51 |  3358 | `	if( pClass == 0 ){` |
|   ! 0 |  3359 | `		return PH7_OK;` |
|     - |  3360 | `	}` |
|    51 |  3361 | `	pList = PH7_NewClassInstance(pVm,pClass);` |
|    51 |  3362 | `	if( pList == 0 ){` |
|   ! 0 |  3363 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  3364 | `	}` |
|    51 |  3365 | `	pList->iRef++;` |
|    51 |  3366 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|    51 |  3367 | `	if( pCons ){` |
|    51 |  3368 | `		PH7_VmCallClassMethod(pVm,pList,pCons,0,0,0);` |
|    25 |  3369 | `	}` |
|    51 |  3370 | `	PH7_NativeSetAttrObj(pVm,pThis,AP_LIST,pList);   /* the slot takes its own reference */` |
|    51 |  3371 | `	PH7_ClassInstanceUnref(pList);` |
|    51 |  3372 | `	return PH7_OK;` |
|    26 |  3373 | `}` |
|     - |  3374 | `/*` |
|     - |  3375 | ` * append(). php's own sequence, and every branch of it is observable:` |
|     - |  3376 | ` *   - a list cursor sitting on a LIVE entry whose cache is empty means the walk has` |
|     - |  3377 | ` *     consumed that entry, so the new iterator goes in behind it and the cursor steps` |
|     - |  3378 | ` *     over;` |
|     - |  3379 | ` *   - if nothing is being iterated yet (or the cache is empty), the cursor is walked` |
|     - |  3380 | ` *     forward until it reaches the iterator just appended, and the fetch resumes there.` |
|     - |  3381 | ` * That second half is what makes an AppendIterator RESUME after exhaustion, and it` |
|     - |  3382 | ` * relies on ArrayIterator::append() reviving a cursor that ran off the end (see` |
|     - |  3383 | ` * SplStoreInsert).` |
|     - |  3384 | ` */` |
|    60 |  3385 | `static int vm_builtin_AppendIterator_append(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3386 | `{` |
|    61 |  3387 | `	ph7_vm *pVm = pCtx->pVm;` |
|    61 |  3388 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  3389 | `	ph7_class_instance *pIt;` |
|     - |  3390 | `	ph7_hashmap *pMap;` |
|    61 |  3391 | `	int bListValid,bInnerValid = 0,nGuard;` |
|     - |  3392 | `	sxi32 rc;` |
|    61 |  3393 | `	if( !DualReady(pThis) ){` |
|   ! 0 |  3394 | `		return DualNotReady(pCtx);` |
|     - |  3395 | `	}` |
|    61 |  3396 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|   ! 0 |  3397 | `		return PH7_OK;   /* the shared ZPP screen already refused a non-Iterator */` |
|     - |  3398 | `	}` |
|    61 |  3399 | `	pIt = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    61 |  3400 | `	pMap = ApMap(pVm,pThis);` |
|    61 |  3401 | `	bListValid = pMap && pMap->pCur;` |
|     - |  3402 | `	/* php's spl_dual_it_valid, both times it appears below: the INNER iterator's` |
|     - |  3403 | `	 * valid() (false when there is no inner at all), NOT the cache. */` |
|    61 |  3404 | `	rc = DualInnerValid(pVm,pThis,&bInnerValid);` |
|    61 |  3405 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  3406 | `		return rc;` |
|     - |  3407 | `	}` |
|    61 |  3408 | `	pMap = ApMap(pVm,pThis);   /* that call ran user code: re-resolve */` |
|    61 |  3409 | `	if( pMap ){` |
|    61 |  3410 | `		SplStoreInsert(pMap,0,apArg[0]);` |
|    30 |  3411 | `	}` |
|    61 |  3412 | `	if( bListValid && !bInnerValid ){` |
|   ! 0 |  3413 | `		ApListNext(pVm,pThis);` |
|   ! 0 |  3414 | `	}` |
|    61 |  3415 | `	if( PH7_NativeAttrObj(pThis,IT_IT) != 0 && bInnerValid ){` |
|    19 |  3416 | `		return PH7_OK;   /* mid-walk with a live element: the new tail waits its turn */` |
|     - |  3417 | `	}` |
|    43 |  3418 | `	pMap = ApMap(pVm,pThis);` |
|    43 |  3419 | `	if( pMap && pMap->pCur == 0 ){` |
|   ! 0 |  3420 | `		ApListRewind(pVm,pThis);` |
|   ! 0 |  3421 | `	}` |
|    22 |  3422 | `	for( nGuard = 0 ; ; ++nGuard ){` |
|    43 |  3423 | `		int bOk = 0;` |
|    43 |  3424 | `		rc = ApAdoptCurrent(pVm,pThis,&bOk);` |
|    43 |  3425 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  3426 | `			return rc;` |
|     - |  3427 | `		}` |
|    43 |  3428 | `		if( !bOk \|\| PH7_NativeAttrObj(pThis,IT_IN) == pIt ){` |
|    22 |  3429 | `			break;` |
|     - |  3430 | `		}` |
|   ! 0 |  3431 | `		ApListNext(pVm,pThis);` |
|   ! 0 |  3432 | `		if( nGuard > 100000 ){` |
|   ! 0 |  3433 | `			break;   /* php's loop has no bound; ours refuses to spin on a mutated list */` |
|     - |  3434 | `		}` |
|   ! 0 |  3435 | `	}` |
|    43 |  3436 | `	rc = ApFetch(pVm,pThis);` |
|    43 |  3437 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|    31 |  3438 | `}` |
|    28 |  3439 | `static int vm_builtin_AppendIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3440 | `{` |
|    29 |  3441 | `	ph7_vm *pVm = pCtx->pVm;` |
|    29 |  3442 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  3443 | `	sxi32 rc;` |
|    29 |  3444 | `	int bOk = 0;` |
|    14 |  3445 | `	SXUNUSED(nArg);` |
|    14 |  3446 | `	SXUNUSED(apArg);` |
|    29 |  3447 | `	if( !DualReady(pThis) ){` |
|   ! 0 |  3448 | `		return DualNotReady(pCtx);` |
|     - |  3449 | `	}` |
|    29 |  3450 | `	ApListRewind(pVm,pThis);` |
|    29 |  3451 | `	rc = ApAdoptCurrent(pVm,pThis,&bOk);` |
|    29 |  3452 | `	if( rc == SXRET_OK && bOk ){` |
|    29 |  3453 | `		rc = ApFetch(pVm,pThis);` |
|    14 |  3454 | `	}` |
|    29 |  3455 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|    15 |  3456 | `}` |
|    46 |  3457 | `static int vm_builtin_AppendIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3458 | `{` |
|    47 |  3459 | `	ph7_vm *pVm = pCtx->pVm;` |
|    47 |  3460 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  3461 | `	sxi32 rc;` |
|    47 |  3462 | `	int bValid = 0;` |
|    23 |  3463 | `	SXUNUSED(nArg);` |
|    23 |  3464 | `	SXUNUSED(apArg);` |
|    47 |  3465 | `	if( !DualReady(pThis) ){` |
|   ! 0 |  3466 | `		return DualNotReady(pCtx);` |
|     - |  3467 | `	}` |
|    47 |  3468 | `	rc = DualInnerValid(pVm,pThis,&bValid);` |
|    47 |  3469 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  3470 | `		return rc;` |
|     - |  3471 | `	}` |
|    47 |  3472 | `	if( bValid ){` |
|    47 |  3473 | `		rc = DualNextInner(pVm,pThis);` |
|    47 |  3474 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  3475 | `			return rc;` |
|     - |  3476 | `		}` |
|    23 |  3477 | `	}` |
|    47 |  3478 | `	rc = ApFetch(pVm,pThis);` |
|    47 |  3479 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|    24 |  3480 | `}` |
|     - |  3481 | `/* php re-fetches here (spl_dual_it_fetch with check_more), which is why an` |
|     - |  3482 | ` * AppendIterator FOLLOWS an inner iterator moved behind its back where every other` |
|     - |  3483 | ` * decorator answers its cache. */` |
|    64 |  3484 | `static int vm_builtin_AppendIterator_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3485 | `{` |
|    65 |  3486 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  3487 | `	sxi32 rc;` |
|    32 |  3488 | `	SXUNUSED(nArg);` |
|    32 |  3489 | `	SXUNUSED(apArg);` |
|    65 |  3490 | `	if( !DualReady(pThis) ){` |
|   ! 0 |  3491 | `		return DualNotReady(pCtx);` |
|     - |  3492 | `	}` |
|    65 |  3493 | `	rc = DualFetch(pCtx->pVm,pThis,TRUE);` |
|    65 |  3494 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  3495 | `		return rc;` |
|     - |  3496 | `	}` |
|    65 |  3497 | `	return DualResultSlot(pCtx,IT_CD);` |
|    33 |  3498 | `}` |
|     - |  3499 | `/* The list cursor's KEY, which is php's index into the appended iterators -- and` |
|     - |  3500 | ` * NULL once the cursor has run off the end, where the chunk kept answering the last` |
|     - |  3501 | ` * index it had seen. */` |
|    26 |  3502 | `static int vm_builtin_AppendIterator_getIteratorIndex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3503 | `{` |
|    27 |  3504 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  3505 | `	ph7_value *pSlot,*apCall[1];` |
|    13 |  3506 | `	SXUNUSED(nArg);` |
|    13 |  3507 | `	SXUNUSED(apArg);` |
|    27 |  3508 | `	if( !DualReady(pThis) ){` |
|   ! 0 |  3509 | `		return DualNotReady(pCtx);` |
|     - |  3510 | `	}` |
|    27 |  3511 | `	pSlot = SplStoreSlot(pCtx->pVm,ApList(pThis));` |
|    27 |  3512 | `	if( pSlot == 0 ){` |
|   ! 0 |  3513 | `		ph7_result_null(pCtx);` |
|   ! 0 |  3514 | `		return PH7_OK;` |
|     - |  3515 | `	}` |
|    27 |  3516 | `	apCall[0] = pSlot;` |
|    27 |  3517 | `	return ph7_hashmap_simple_key(pCtx,1,apCall);` |
|    14 |  3518 | `}` |
|     - |  3519 | `/*` |
|     - |  3520 | ` * ---------------------------------------------------------------------------` |
|     - |  3521 | ` * The RECURSIVE pair: RecursiveArrayIterator (an ArrayIterator that descends into` |
|     - |  3522 | ` * its own entries) and RecursiveFilterIterator (a FilterIterator that forwards the` |
|     - |  3523 | ` * two recursion methods to its inner iterator).` |
|     - |  3524 | ` *` |
|     - |  3525 | ` * RecursiveArrayIterator is where php's CHILD_ARRAYS_ONLY flag lives, and the` |
|     - |  3526 | ``  * chunk's two-line `is_array($c) \|\| is_object($c)` / `new $c($this->current())` `` |
|     - |  3527 | ` * ignored it in both directions: an OBJECT entry claimed children under a flag that` |
|     - |  3528 | ` * exists to say it has none, and the child iterator was built WITHOUT the parent's` |
|     - |  3529 | ` * flags, so the restriction lasted exactly one level. php also answers null rather` |
|     - |  3530 | ` * than descending when there is no current element, and hands back an entry that is` |
|     - |  3531 | ` * ALREADY an instance of the called class instead of wrapping it again.` |
|     - |  3532 | ` */` |
|     - |  3533 | `#define RAI_CHILD_ARRAYS_ONLY 4` |
|     - |  3534 | `/* The entry the store cursor is on, or 0 past the end (php's` |
|     - |  3535 | ` * zend_hash_get_current_data_ex, which every one of these four bodies starts with). */` |
|   608 |  3536 | `static ph7_value * RaiCurrentEntry(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     2 |  3537 | `{` |
|   610 |  3538 | `	ph7_hashmap *pMap = SplStore(pVm,pThis);` |
|   610 |  3539 | `	return (pMap && pMap->pCur) ? HashmapExtractNodeValue(pMap->pCur) : 0;` |
|     2 |  3540 | `}` |
|   444 |  3541 | `static int vm_builtin_RecursiveArrayIterator_hasChildren(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  3542 | `{` |
|   446 |  3543 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   446 |  3544 | `	ph7_value *pEntry = RaiCurrentEntry(pCtx->pVm,pThis);` |
|   446 |  3545 | `	int bHas = 0;` |
|   222 |  3546 | `	SXUNUSED(nArg);` |
|   222 |  3547 | `	SXUNUSED(apArg);` |
|   446 |  3548 | `	if( pEntry ){` |
|   444 |  3549 | `		if( pEntry->iFlags & MEMOBJ_HASHMAP ){` |
|   168 |  3550 | `			bHas = 1;` |
|   361 |  3551 | `		}else if( pEntry->iFlags & MEMOBJ_OBJ ){` |
|     - |  3552 | `			/* php: an object is a child UNLESS the iterator was told arrays only. */` |
|    13 |  3553 | `			bHas = (PH7_NativeAttrInt(pThis,SPL_F) & RAI_CHILD_ARRAYS_ONLY) == 0;` |
|     6 |  3554 | `		}` |
|   221 |  3555 | `	}` |
|   446 |  3556 | `	ph7_result_bool(pCtx,bHas);` |
|   446 |  3557 | `	return PH7_OK;` |
|     2 |  3558 | `}` |
|   164 |  3559 | `static int vm_builtin_RecursiveArrayIterator_getChildren(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  3560 | `{` |
|   166 |  3561 | `	ph7_vm *pVm = pCtx->pVm;` |
|   166 |  3562 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   166 |  3563 | `	ph7_value *pEntry = RaiCurrentEntry(pVm,pThis);` |
|     - |  3564 | `	ph7_class_instance *pChild;` |
|     - |  3565 | `	ph7_class_method *pCons;` |
|     - |  3566 | `	ph7_value sEntry,sFlags,*apCtor[2];` |
|     - |  3567 | `	sxi64 iFlags;` |
|     - |  3568 | `	sxi32 rc;` |
|    82 |  3569 | `	SXUNUSED(nArg);` |
|    82 |  3570 | `	SXUNUSED(apArg);` |
|   166 |  3571 | `	if( pThis == 0 \|\| pEntry == 0 ){` |
|     3 |  3572 | `		ph7_result_null(pCtx);   /* php descends into nothing when nothing is current */` |
|     3 |  3573 | `		return PH7_OK;` |
|     - |  3574 | `	}` |
|   164 |  3575 | `	iFlags = PH7_NativeAttrInt(pThis,SPL_F);` |
|   164 |  3576 | `	if( pEntry->iFlags & MEMOBJ_OBJ ){` |
|     9 |  3577 | `		ph7_class_instance *pObj = (ph7_class_instance *)pEntry->x.pOther;` |
|     9 |  3578 | `		if( iFlags & RAI_CHILD_ARRAYS_ONLY ){` |
|     3 |  3579 | `			ph7_result_null(pCtx);` |
|     3 |  3580 | `			return PH7_OK;` |
|     - |  3581 | `		}` |
|     7 |  3582 | `		if( pObj && PH7_VmInstanceOf(pObj->pClass,pThis->pClass) ){` |
|     - |  3583 | `			/* Already one of us: php hands the entry back rather than wrapping it. */` |
|     3 |  3584 | `			SplResultBorrowed(pCtx,pObj);` |
|     3 |  3585 | `			return PH7_OK;` |
|     - |  3586 | `		}` |
|     2 |  3587 | `	}` |
|     - |  3588 | `	/* php's spl_instantiate_child_arg: the CALLED class, constructed with the entry` |
|     - |  3589 | `	 * AND the parent's flags -- which is what carries CHILD_ARRAYS_ONLY down. */` |
|   160 |  3590 | `	pChild = PH7_NewClassInstance(pVm,pThis->pClass);` |
|   160 |  3591 | `	if( pChild == 0 ){` |
|   ! 0 |  3592 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  3593 | `	}` |
|   160 |  3594 | `	pChild->iRef++;` |
|   160 |  3595 | `	PH7_MemObjInit(pVm,&sEntry);` |
|   160 |  3596 | `	PH7_MemObjStore(pEntry,&sEntry);` |
|   160 |  3597 | `	PH7_MemObjInitFromInt(pVm,&sFlags,iFlags);` |
|   160 |  3598 | `	apCtor[0] = &sEntry;` |
|   160 |  3599 | `	apCtor[1] = &sFlags;` |
|   160 |  3600 | `	pCons = PH7_ClassExtractMethod(pThis->pClass,"__construct",sizeof("__construct")-1);` |
|   160 |  3601 | `	rc = pCons ? PH7_VmCallClassMethod(pVm,pChild,pCons,0,2,apCtor) : SXRET_OK;` |
|   160 |  3602 | `	PH7_MemObjRelease(&sEntry);` |
|   160 |  3603 | `	PH7_MemObjRelease(&sFlags);` |
|   160 |  3604 | `	if( rc != SXRET_OK ){` |
|     7 |  3605 | `		PH7_ClassInstanceUnref(pChild);` |
|     7 |  3606 | `		return rc;` |
|     - |  3607 | `	}` |
|   154 |  3608 | `	PH7_NativeResultObject(pCtx,pChild);` |
|   154 |  3609 | `	PH7_ClassInstanceUnref(pChild);` |
|   154 |  3610 | `	return PH7_OK;` |
|    84 |  3611 | `}` |
|     - |  3612 | `/* RecursiveFilterIterator forwards both methods to the object getInnerIterator()` |
|     - |  3613 | ` * answers (php calls on inner.zobject), and wraps the children in ITS OWN class. */` |
|    20 |  3614 | `static int vm_builtin_RecursiveFilterIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3615 | `{` |
|    21 |  3616 | `	return DualConstruct(pCtx,"RecursiveFilterIterator",nArg,apArg);` |
|     1 |  3617 | `}` |
|   104 |  3618 | `static sxi32 RfiCallInner(ph7_context *pCtx,const char *zName,sxu32 nName,ph7_value *pOut)` |
|     1 |  3619 | `{` |
|   105 |  3620 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   105 |  3621 | `	ph7_class_instance *pIn = pThis ? PH7_NativeAttrObj(pThis,IT_IN) : 0;` |
|   105 |  3622 | `	ph7_class_method *pMethod = pIn ? PH7_ClassExtractMethod(pIn->pClass,zName,nName) : 0;` |
|   105 |  3623 | `	if( pMethod == 0 ){` |
|   ! 0 |  3624 | `		return SXRET_OK;` |
|     - |  3625 | `	}` |
|   105 |  3626 | `	return PH7_VmCallClassMethod(pCtx->pVm,pIn,pMethod,pOut,0,0);` |
|    53 |  3627 | `}` |
|    46 |  3628 | `static int vm_builtin_RecursiveFilterIterator_hasChildren(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3629 | `{` |
|    47 |  3630 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  3631 | `	ph7_value sRes;` |
|     - |  3632 | `	sxi32 rc;` |
|    23 |  3633 | `	SXUNUSED(nArg);` |
|    23 |  3634 | `	SXUNUSED(apArg);` |
|    47 |  3635 | `	if( !DualReady(pThis) ){` |
|     5 |  3636 | `		return DualNotReady(pCtx);` |
|     - |  3637 | `	}` |
|    43 |  3638 | `	PH7_MemObjInit(pCtx->pVm,&sRes);` |
|    43 |  3639 | `	rc = RfiCallInner(pCtx,"hasChildren",sizeof("hasChildren")-1,&sRes);` |
|    43 |  3640 | `	if( rc == SXRET_OK ){` |
|    43 |  3641 | `		ph7_result_value(pCtx,&sRes);` |
|    21 |  3642 | `	}` |
|    43 |  3643 | `	PH7_MemObjRelease(&sRes);` |
|    43 |  3644 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|    24 |  3645 | `}` |
|     - |  3646 | `/*` |
|     - |  3647 | ` * php's spl_instantiate_arg_ex1/2/3 for the recursive filters: fetch the INNER` |
|     - |  3648 | ` * iterator's children and hand them to a fresh instance of the CALLED class` |
|     - |  3649 | `` * (`Z_OBJCE_P(ZEND_THIS)`, so a user subclass answers its own type), followed by`` |
|     - |  3650 | ` * whatever the subclass's constructor needs after the iterator -- the callback for` |
|     - |  3651 | ` * RecursiveCallbackFilterIterator, the four regex arguments for` |
|     - |  3652 | ` * RecursiveRegexIterator, nothing for the other two.` |
|     - |  3653 | ` */` |
|    26 |  3654 | `static int RfiBuildChild(ph7_context *pCtx,ph7_value **apExtra,int nExtra)` |
|     1 |  3655 | `{` |
|    27 |  3656 | `	ph7_vm *pVm = pCtx->pVm;` |
|    27 |  3657 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  3658 | `	ph7_class_instance *pChild;` |
|     - |  3659 | `	ph7_class_method *pCons;` |
|     - |  3660 | `	ph7_value sInner,*apCtor[5];` |
|     - |  3661 | `	int i;` |
|     - |  3662 | `	sxi32 rc;` |
|    27 |  3663 | `	if( !DualReady(pThis) ){` |
|   ! 0 |  3664 | `		return DualNotReady(pCtx);` |
|     - |  3665 | `	}` |
|    27 |  3666 | `	if( nExtra > (int)SX_ARRAYSIZE(apCtor) - 1 ){` |
|   ! 0 |  3667 | `		nExtra = (int)SX_ARRAYSIZE(apCtor) - 1;` |
|   ! 0 |  3668 | `	}` |
|    27 |  3669 | `	PH7_MemObjInit(pVm,&sInner);` |
|    27 |  3670 | `	rc = RfiCallInner(pCtx,"getChildren",sizeof("getChildren")-1,&sInner);` |
|    27 |  3671 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  3672 | `		PH7_MemObjRelease(&sInner);` |
|   ! 0 |  3673 | `		return rc;` |
|     - |  3674 | `	}` |
|    27 |  3675 | `	pChild = PH7_NewClassInstance(pVm,pThis->pClass);` |
|    27 |  3676 | `	if( pChild == 0 ){` |
|   ! 0 |  3677 | `		PH7_MemObjRelease(&sInner);` |
|   ! 0 |  3678 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  3679 | `	}` |
|    27 |  3680 | `	pChild->iRef++;` |
|    27 |  3681 | `	apCtor[0] = &sInner;` |
|    49 |  3682 | `	for( i = 0 ; i < nExtra ; ++i ){` |
|    23 |  3683 | `		apCtor[i+1] = apExtra[i];` |
|    12 |  3684 | `	}` |
|    27 |  3685 | `	pCons = PH7_ClassExtractMethod(pThis->pClass,"__construct",sizeof("__construct")-1);` |
|    27 |  3686 | `	rc = pCons ? PH7_VmCallClassMethod(pVm,pChild,pCons,0,nExtra+1,apCtor) : SXRET_OK;` |
|    27 |  3687 | `	PH7_MemObjRelease(&sInner);` |
|    27 |  3688 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  3689 | `		PH7_ClassInstanceUnref(pChild);` |
|   ! 0 |  3690 | `		return rc;` |
|     - |  3691 | `	}` |
|    27 |  3692 | `	PH7_NativeResultObject(pCtx,pChild);` |
|    27 |  3693 | `	PH7_ClassInstanceUnref(pChild);` |
|    27 |  3694 | `	return PH7_OK;` |
|    14 |  3695 | `}` |
|    16 |  3696 | `static int vm_builtin_RecursiveFilterIterator_getChildren(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3697 | `{` |
|     8 |  3698 | `	SXUNUSED(nArg);` |
|     8 |  3699 | `	SXUNUSED(apArg);` |
|    17 |  3700 | `	return RfiBuildChild(pCtx,0,0);` |
|     1 |  3701 | `}` |
|     - |  3702 | `/*` |
|     - |  3703 | ` * ParentIterator: the RecursiveFilterIterator whose accept() IS the question` |
|     - |  3704 | ` * "does the current element have children?". php asks the INNER iterator` |
|     - |  3705 | `` * (`inner.zobject`), not `$this`, so overriding hasChildren() on the`` |
|     - |  3706 | ` * ParentIterator subclass changes nothing and overriding it on the inner` |
|     - |  3707 | ` * RecursiveIterator changes everything -- and the answer is cast to a bool.` |
|     - |  3708 | ` */` |
|    22 |  3709 | `static int vm_builtin_ParentIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3710 | `{` |
|    23 |  3711 | `	return DualConstruct(pCtx,"ParentIterator",nArg,apArg);` |
|     1 |  3712 | `}` |
|    38 |  3713 | `static int vm_builtin_ParentIterator_accept(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3714 | `{` |
|    39 |  3715 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  3716 | `	ph7_value sRes;` |
|     - |  3717 | `	sxi32 rc;` |
|    19 |  3718 | `	SXUNUSED(nArg);` |
|    19 |  3719 | `	SXUNUSED(apArg);` |
|    39 |  3720 | `	if( !DualReady(pThis) ){` |
|     3 |  3721 | `		return DualNotReady(pCtx);` |
|     - |  3722 | `	}` |
|    37 |  3723 | `	PH7_MemObjInit(pCtx->pVm,&sRes);` |
|    37 |  3724 | `	rc = RfiCallInner(pCtx,"hasChildren",sizeof("hasChildren")-1,&sRes);` |
|    37 |  3725 | `	if( rc == SXRET_OK ){` |
|    37 |  3726 | `		PH7_MemObjToBool(&sRes);          /* a STATUS, not the answer */` |
|    37 |  3727 | `		ph7_result_bool(pCtx,sRes.x.iVal != 0);` |
|    18 |  3728 | `	}` |
|    37 |  3729 | `	PH7_MemObjRelease(&sRes);` |
|    37 |  3730 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|    20 |  3731 | `}` |
|     - |  3732 | `/*` |
|     - |  3733 | ` * RecursiveCallbackFilterIterator: the callback filter's recursive twin. Both` |
|     - |  3734 | ` * halves are inherited behaviour -- accept() is CallbackFilterIterator's and` |
|     - |  3735 | ` * hasChildren() is RecursiveFilterIterator's -- but php DECLARES all four names on` |
|     - |  3736 | ` * the class, and getChildren() has to carry the callback down to the child.` |
|     - |  3737 | ` */` |
|    14 |  3738 | `static int vm_builtin_RecursiveCallbackFilterIterator_construct(ph7_context *pCtx,int nArg,` |
|     - |  3739 | `	ph7_value **apArg)` |
|     1 |  3740 | `{` |
|     - |  3741 | `	ph7_class_instance *pThis;` |
|     - |  3742 | `	sxi32 rc;` |
|    15 |  3743 | `	if( nArg > 1 ){` |
|    15 |  3744 | `		rc = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",FALSE);` |
|    15 |  3745 | `		if( rc != PH7_OK ){` |
|     3 |  3746 | `			return rc;` |
|     - |  3747 | `		}` |
|     6 |  3748 | `	}` |
|    13 |  3749 | `	rc = DualConstruct(pCtx,"RecursiveCallbackFilterIterator",nArg,apArg);` |
|    13 |  3750 | `	pThis = PH7_ContextThis(pCtx);` |
|    13 |  3751 | `	if( rc == PH7_OK && pThis && nArg > 1 ){` |
|    13 |  3752 | `		DualSetSlot(pCtx->pVm,pThis,IT_CB,apArg[1]);` |
|     6 |  3753 | `	}` |
|    13 |  3754 | `	return rc;` |
|     8 |  3755 | `}` |
|     8 |  3756 | `static int vm_builtin_RecursiveCallbackFilterIterator_getChildren(ph7_context *pCtx,int nArg,` |
|     - |  3757 | `	ph7_value **apArg)` |
|     1 |  3758 | `{` |
|     9 |  3759 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  3760 | `	ph7_value sCb,*apExtra[1];` |
|     - |  3761 | `	int rc;` |
|     4 |  3762 | `	SXUNUSED(nArg);` |
|     4 |  3763 | `	SXUNUSED(apArg);` |
|     9 |  3764 | `	if( !DualReady(pThis) ){` |
|     3 |  3765 | `		return DualNotReady(pCtx);` |
|     - |  3766 | `	}` |
|     - |  3767 | `	/* Take the callback as a VALUE: the child's constructor runs user code, and a` |
|     - |  3768 | `	 * pointer into pVm->aMemObj does not survive that. */` |
|     7 |  3769 | `	PH7_MemObjInit(pCtx->pVm,&sCb);` |
|     - |  3770 | `	{` |
|     7 |  3771 | `		ph7_value *pCb = PH7_NativeAttr(pThis,IT_CB);` |
|     7 |  3772 | `		if( pCb ){` |
|     7 |  3773 | `			PH7_MemObjStore(pCb,&sCb);` |
|     3 |  3774 | `		}` |
|     - |  3775 | `	}` |
|     7 |  3776 | `	apExtra[0] = &sCb;` |
|     7 |  3777 | `	rc = RfiBuildChild(pCtx,apExtra,1);` |
|     7 |  3778 | `	PH7_MemObjRelease(&sCb);` |
|     7 |  3779 | `	return rc;` |
|     5 |  3780 | `}` |
|     - |  3781 | `/*` |
|     - |  3782 | ` * RecursiveRegexIterator: the regex filter's recursive twin. Its accept() has one` |
|     - |  3783 | ` * rule of its own, and it comes BEFORE everything RegexIterator does: a current()` |
|     - |  3784 | ` * that is an ARRAY is accepted when it is non-empty, whatever the mode, the` |
|     - |  3785 | ` * pattern, USE_KEY or INVERT_MATCH say -- a container is kept so the walk can` |
|     - |  3786 | ` * descend into it, and only its LEAVES are matched. (RegexIterator itself refuses` |
|     - |  3787 | ` * an array outright, which is what makes the plain class useless recursively.)` |
|     - |  3788 | ``  * getChildren() carries the four regex arguments down; php passes `replacement` `` |
|     - |  3789 | ` * to nothing, so a child starts with the declared NULL.` |
|     - |  3790 | ` */` |
|    26 |  3791 | `static int vm_builtin_RecursiveRegexIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3792 | `{` |
|    27 |  3793 | `	return RegitConstruct(pCtx,"RecursiveRegexIterator",nArg,apArg);` |
|     1 |  3794 | `}` |
|    28 |  3795 | `static int vm_builtin_RecursiveRegexIterator_accept(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3796 | `{` |
|    29 |  3797 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    29 |  3798 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|     3 |  3799 | `		return DualNotReady(pCtx);` |
|     - |  3800 | `	}` |
|    27 |  3801 | `	if( DualFilled(pThis) ){` |
|    27 |  3802 | `		ph7_value *pCur = PH7_NativeAttr(pThis,IT_CD);` |
|    27 |  3803 | `		if( pCur && (pCur->iFlags & MEMOBJ_HASHMAP) && pCur->x.pOther ){` |
|    17 |  3804 | `			ph7_result_bool(pCtx,((ph7_hashmap *)pCur->x.pOther)->nEntry > 0);` |
|    17 |  3805 | `			return PH7_OK;` |
|     - |  3806 | `		}` |
|     5 |  3807 | `	}` |
|    11 |  3808 | `	return vm_builtin_RegexIterator_accept(pCtx,nArg,apArg);` |
|    15 |  3809 | `}` |
|     6 |  3810 | `static int vm_builtin_RecursiveRegexIterator_getChildren(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3811 | `{` |
|     7 |  3812 | `	ph7_vm *pVm = pCtx->pVm;` |
|     7 |  3813 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  3814 | `	ph7_value sRe,sMode,sFlags,sPreg,*apExtra[4];` |
|     - |  3815 | `	ph7_value *pRe;` |
|     - |  3816 | `	int rc;` |
|     3 |  3817 | `	SXUNUSED(nArg);` |
|     3 |  3818 | `	SXUNUSED(apArg);` |
|     7 |  3819 | `	if( !DualReady(pThis) ){` |
|     3 |  3820 | `		return DualNotReady(pCtx);` |
|     - |  3821 | `	}` |
|     5 |  3822 | `	PH7_MemObjInit(pVm,&sRe);` |
|     5 |  3823 | `	pRe = PH7_NativeAttr(pThis,IT_RE);` |
|     5 |  3824 | `	if( pRe ){` |
|     5 |  3825 | `		PH7_MemObjStore(pRe,&sRe);` |
|     2 |  3826 | `	}` |
|     5 |  3827 | `	PH7_MemObjInitFromInt(pVm,&sMode,PH7_NativeAttrInt(pThis,IT_RM));` |
|     5 |  3828 | `	PH7_MemObjInitFromInt(pVm,&sFlags,PH7_NativeAttrInt(pThis,IT_RF));` |
|     5 |  3829 | `	PH7_MemObjInitFromInt(pVm,&sPreg,PH7_NativeAttrInt(pThis,IT_RP));` |
|     5 |  3830 | `	apExtra[0] = &sRe;` |
|     5 |  3831 | `	apExtra[1] = &sMode;` |
|     5 |  3832 | `	apExtra[2] = &sFlags;` |
|     5 |  3833 | `	apExtra[3] = &sPreg;` |
|     5 |  3834 | `	rc = RfiBuildChild(pCtx,apExtra,4);` |
|     5 |  3835 | `	PH7_MemObjRelease(&sRe);` |
|     5 |  3836 | `	PH7_MemObjRelease(&sMode);` |
|     5 |  3837 | `	PH7_MemObjRelease(&sFlags);` |
|     5 |  3838 | `	PH7_MemObjRelease(&sPreg);` |
|     5 |  3839 | `	return rc;` |
|     4 |  3840 | `}` |
|    12 |  3841 | `static int vm_builtin_AppendIterator_getArrayIterator(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3842 | `{` |
|    13 |  3843 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  3844 | `	ph7_class_instance *pList;` |
|     6 |  3845 | `	SXUNUSED(nArg);` |
|     6 |  3846 | `	SXUNUSED(apArg);` |
|    13 |  3847 | `	if( !DualReady(pThis) ){` |
|   ! 0 |  3848 | `		return DualNotReady(pCtx);` |
|     - |  3849 | `	}` |
|    13 |  3850 | `	pList = ApList(pThis);` |
|    13 |  3851 | `	if( pList ){` |
|    13 |  3852 | `		SplResultBorrowed(pCtx,pList);` |
|     7 |  3853 | `	}else{` |
|   ! 0 |  3854 | `		ph7_result_null(pCtx);` |
|     - |  3855 | `	}` |
|    13 |  3856 | `	return PH7_OK;` |
|     7 |  3857 | `}` |
|     - |  3858 | `/*` |
|     - |  3859 | ` * ---------------------------------------------------------------------------` |
|     - |  3860 | ` * CachingIterator and RecursiveCachingIterator.` |
|     - |  3861 | ` *` |
|     - |  3862 | ` * The caching iterator is one step AHEAD of the iterator it decorates: each fetch` |
|     - |  3863 | ` * copies current()/key() into the cache every dual iterator keeps and then ADVANCES` |
|     - |  3864 | ` * the inner one, which is what makes hasNext() answerable at all — it is the` |
|     - |  3865 | ` * inner's live valid(), asked after that step. Everything else this class does` |
|     - |  3866 | ` * happens inside the same fetch, in php's order: the FULL_CACHE entry is written,` |
|     - |  3867 | ` * then (for the recursive twin) the CHILDREN are built, then the string form is` |
|     - |  3868 | ` * computed, then the inner is advanced.` |
|     - |  3869 | ` *` |
|     - |  3870 | ` * That eager string is the class's least obvious rule. CALL_TOSTRING casts the` |
|     - |  3871 | ` * ELEMENT and TOSTRING_USE_INNER casts the inner ITERATOR, both at FETCH time, so` |
|     - |  3872 | `` * the default `new CachingIterator($it)` over objects with no __toString throws`` |
|     - |  3873 | `` * from rewind() and over arrays warns `Array to string conversion` once per`` |
|     - |  3874 | `` * element — neither waits for anyone to write `(string)$it`. The other two`` |
|     - |  3875 | ` * spellings (TOSTRING_USE_KEY / TOSTRING_USE_CURRENT) are read out of the cache at` |
|     - |  3876 | ` * __toString() time instead, and NO spelling at all is a BadMethodCallException` |
|     - |  3877 | ` * that names the RECEIVER's class.` |
|     - |  3878 | ` *` |
|     - |  3879 | ` * getFlags() answers the RAW word, php's private CIT_VALID (0x10000) included, so` |
|     - |  3880 | ` * a fetched iterator reports 65537 where its constructor was handed 1. setFlags()` |
|     - |  3881 | ` * keeps the high half and replaces the low one, and refuses to unset either of the` |
|     - |  3882 | ` * two flags whose machinery cannot be turned off mid-walk; the CONSTRUCTOR masks` |
|     - |  3883 | `` * with CIT_PUBLIC instead, so `new CachingIterator($it, 1024)` reports 1024 while`` |
|     - |  3884 | ` * setFlags(1024) on a default instance is a refusal.` |
|     - |  3885 | ` */` |
|     - |  3886 | `#define CIT_FL   "__cfl"    /* php's u.caching.flags, its private CIT_VALID included */` |
|     - |  3887 | `#define CIT_STR  "__cstr"   /* php's u.caching.zstr: the string computed at fetch */` |
|     - |  3888 | `#define CIT_CCH  "__ccch"   /* php's u.caching.zcache */` |
|     - |  3889 | `#define CIT_KIDS "__ckid"   /* php's u.caching.zchildren, the recursive twin's only state */` |
|     - |  3890 |  |
|     - |  3891 | `#define CIT_CALL_TOSTRING     0x00000001` |
|     - |  3892 | `#define CIT_TOSTRING_USE_KEY  0x00000002` |
|     - |  3893 | `#define CIT_TOSTRING_USE_CUR  0x00000004` |
|     - |  3894 | `#define CIT_TOSTRING_USE_INN  0x00000008` |
|     - |  3895 | `#define CIT_CATCH_GET_CHILD   0x00000010` |
|     - |  3896 | `#define CIT_FULL_CACHE        0x00000100` |
|     - |  3897 | `#define CIT_PUBLIC            0x0000FFFF` |
|     - |  3898 | `#define CIT_VALID             0x00010000` |
|     - |  3899 |  |
|   942 |  3900 | `static sxi64 CitFlags(ph7_class_instance *pThis)` |
|     4 |  3901 | `{` |
|   946 |  3902 | `	return pThis ? PH7_NativeAttrInt(pThis,CIT_FL) : 0;` |
|     4 |  3903 | `}` |
|     - |  3904 | `/* php's spl_cit_check_flags: at most ONE of the four string spellings. */` |
|   198 |  3905 | `static int CitCheckFlags(sxi64 iFlags)` |
|     4 |  3906 | `{` |
|   202 |  3907 | `	int n = 0;` |
|   202 |  3908 | `	if( iFlags & CIT_CALL_TOSTRING ){ n++; }` |
|   202 |  3909 | `	if( iFlags & CIT_TOSTRING_USE_KEY ){ n++; }` |
|   202 |  3910 | `	if( iFlags & CIT_TOSTRING_USE_CUR ){ n++; }` |
|   202 |  3911 | `	if( iFlags & CIT_TOSTRING_USE_INN ){ n++; }` |
|   202 |  3912 | `	return n <= 1;` |
|     4 |  3913 | `}` |
|     - |  3914 | `/* Both of this class's refusals name the RECEIVER's class and point at` |
|     - |  3915 | ` * CachingIterator::__construct whatever that receiver is. */` |
|    16 |  3916 | `static int CitRefuse(ph7_context *pCtx,ph7_class_instance *pThis,const char *zWhat)` |
|     1 |  3917 | `{` |
|    17 |  3918 | `	SyString *pName = &pThis->pClass->sName;` |
|    25 |  3919 | `	return PH7_VmThrowException(pCtx,"BadMethodCallException",` |
|     8 |  3920 | `		"%z does not %s (see CachingIterator::__construct)",pName,zWhat);` |
|     1 |  3921 | `}` |
|     - |  3922 | `/* The cache slot, separated for writing (every caller may mutate it). */` |
|    84 |  3923 | `static ph7_value * CitCacheSlot(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     3 |  3924 | `{` |
|    87 |  3925 | `	ph7_value *pSlot = pThis ? PH7_NativeAttr(pThis,CIT_CCH) : 0;` |
|    87 |  3926 | `	if( pSlot == 0 ){` |
|   ! 0 |  3927 | `		return 0;` |
|     - |  3928 | `	}` |
|    87 |  3929 | `	if( (pSlot->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     3 |  3930 | `		if( PH7_MemObjToHashmap(pSlot) != SXRET_OK ){` |
|   ! 0 |  3931 | `			return 0;` |
|     - |  3932 | `		}` |
|     1 |  3933 | `	}` |
|    87 |  3934 | `	if( PH7_HashmapCowSeparate(pVm,pSlot) == 0 ){` |
|   ! 0 |  3935 | `		return 0;` |
|     - |  3936 | `	}` |
|     - |  3937 | `	/* Re-ask: the separation can MOVE the memobj pool this slot lives in` |
|     - |  3938 | `	 * (SplStoreSlot explains it). */` |
|    87 |  3939 | `	return PH7_NativeAttr(pThis,CIT_CCH);` |
|    45 |  3940 | `}` |
|     - |  3941 | `/* Every cache reader is refused outright without FULL_CACHE, php's own guard. */` |
|    74 |  3942 | `static ph7_value * CitCacheChecked(ph7_context *pCtx,int *pRc)` |
|     3 |  3943 | `{` |
|    77 |  3944 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    77 |  3945 | `	*pRc = PH7_OK;` |
|    77 |  3946 | `	if( !DualReady(pThis) ){` |
|     5 |  3947 | `		*pRc = DualNotReady(pCtx);` |
|     5 |  3948 | `		return 0;` |
|     - |  3949 | `	}` |
|    73 |  3950 | `	if( (CitFlags(pThis) & CIT_FULL_CACHE) == 0 ){` |
|    13 |  3951 | `		*pRc = CitRefuse(pCtx,pThis,"use a full cache");` |
|    13 |  3952 | `		return 0;` |
|     - |  3953 | `	}` |
|    61 |  3954 | `	return CitCacheSlot(pCtx->pVm,pThis);` |
|    40 |  3955 | `}` |
|     - |  3956 | `/*` |
|     - |  3957 | ` * php's spl_caching_it_next tail: the string the class will answer from. The two` |
|     - |  3958 | ` * eager spellings are exclusive (spl_cit_check_flags saw to that), and the cast is` |
|     - |  3959 | ` * php's own, warnings and refusals included.` |
|     - |  3960 | ` */` |
|    16 |  3961 | `static sxi32 CitMakeString(ph7_context *pCtx,ph7_class_instance *pThis,sxi64 iFlags)` |
|     1 |  3962 | `{` |
|    17 |  3963 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  3964 | `	ph7_value sVal;` |
|     - |  3965 | `	sxi32 rc;` |
|    17 |  3966 | `	PH7_MemObjInit(pVm,&sVal);` |
|    17 |  3967 | `	if( iFlags & CIT_TOSTRING_USE_INN ){` |
|     3 |  3968 | `		ph7_class_instance *pIn = PH7_NativeAttrObj(pThis,IT_IN);` |
|     3 |  3969 | `		if( pIn ){` |
|     - |  3970 | `			/* The temporary OWNS this reference: PH7_MemObjRelease below drops one,` |
|     - |  3971 | `			 * and the cast itself may retype the slot out from under the object. */` |
|     3 |  3972 | `			pIn->iRef++;` |
|     3 |  3973 | `			sVal.x.pOther = pIn;` |
|     3 |  3974 | `			MemObjSetType(&sVal,MEMOBJ_OBJ);` |
|     1 |  3975 | `		}` |
|     2 |  3976 | `	}else{` |
|    15 |  3977 | `		ph7_value *pCur = PH7_NativeAttr(pThis,IT_CD);` |
|    15 |  3978 | `		if( pCur ){` |
|    15 |  3979 | `			PH7_MemObjStore(pCur,&sVal);` |
|     7 |  3980 | `		}` |
|     - |  3981 | `	}` |
|    17 |  3982 | `	rc = PH7_MemObjToStringUV(&sVal);` |
|    17 |  3983 | `	if( rc == SXRET_OK ){` |
|    15 |  3984 | `		DualSetSlot(pVm,pThis,CIT_STR,&sVal);` |
|     7 |  3985 | `	}` |
|    17 |  3986 | `	PH7_MemObjRelease(&sVal);` |
|    17 |  3987 | `	return rc;` |
|     1 |  3988 | `}` |
|     - |  3989 | `/*` |
|     - |  3990 | ` * php's recursion half of the same fetch: ask the INNER iterator whether the` |
|     - |  3991 | ` * element has children and, if it does, build the child decorator EAGERLY —` |
|     - |  3992 | ` * getChildren() only hands back what this already made. CATCH_GET_CHILD swallows` |
|     - |  3993 | ` * a throw from any of the three steps (hasChildren, getChildren, and the child's` |
|     - |  3994 | ` * own constructor), which is php's clear-the-exception-and-carry-on.` |
|     - |  3995 | ` *` |
|     - |  3996 | ` * The child is a plain RecursiveCachingIterator even when the receiver is a` |
|     - |  3997 | ` * SUBCLASS: php names the class entry here rather than reading ZEND_THIS's, which` |
|     - |  3998 | ` * is the opposite of what the recursive FILTERS do.` |
|     - |  3999 | ` */` |
|   142 |  4000 | `static sxi32 CitBuildChildren(ph7_context *pCtx,ph7_class_instance *pThis)` |
|     1 |  4001 | `{` |
|   143 |  4002 | `	ph7_vm *pVm = pCtx->pVm;` |
|   143 |  4003 | `	ph7_class_instance *pIn = PH7_NativeAttrObj(pThis,IT_IN);` |
|     - |  4004 | `	ph7_class_instance *pChild;` |
|     - |  4005 | `	ph7_class *pCls;` |
|     - |  4006 | `	ph7_class_method *pMethod;` |
|     - |  4007 | `	ph7_value sRes,sFlags,*apCtor[2];` |
|   143 |  4008 | `	int bCatch = (CitFlags(pThis) & CIT_CATCH_GET_CHILD) != 0;` |
|   143 |  4009 | `	int bThrew = FALSE;` |
|     - |  4010 | `	sxi32 rc;` |
|   143 |  4011 | `	PH7_NativeSetAttrObj(pVm,pThis,CIT_KIDS,0);` |
|   143 |  4012 | `	pMethod = pIn ? PH7_ClassExtractMethod(pIn->pClass,"hasChildren",sizeof("hasChildren")-1) : 0;` |
|   143 |  4013 | `	if( pMethod == 0 ){` |
|   ! 0 |  4014 | `		return SXRET_OK;` |
|     - |  4015 | `	}` |
|   143 |  4016 | `	PH7_MemObjInit(pVm,&sRes);` |
|   127 |  4017 | `	rc = bCatch ? PH7_VmCallMethodSwallow(pVm,pIn,pMethod,&sRes,0,0,&bThrew)` |
|    87 |  4018 | `	            : PH7_VmCallClassMethod(pVm,pIn,pMethod,&sRes,0,0);` |
|   143 |  4019 | `	if( rc != SXRET_OK \|\| bThrew ){` |
|     5 |  4020 | `		PH7_MemObjRelease(&sRes);` |
|     5 |  4021 | `		return rc;` |
|     - |  4022 | `	}` |
|   139 |  4023 | `	PH7_MemObjToBool(&sRes);              /* a STATUS, not the answer */` |
|   139 |  4024 | `	if( sRes.x.iVal == 0 ){` |
|    89 |  4025 | `		PH7_MemObjRelease(&sRes);` |
|    89 |  4026 | `		return SXRET_OK;` |
|     - |  4027 | `	}` |
|    51 |  4028 | `	PH7_MemObjRelease(&sRes);` |
|    51 |  4029 | `	pMethod = PH7_ClassExtractMethod(pIn->pClass,"getChildren",sizeof("getChildren")-1);` |
|    51 |  4030 | `	if( pMethod == 0 ){` |
|   ! 0 |  4031 | `		return SXRET_OK;` |
|     - |  4032 | `	}` |
|    51 |  4033 | `	PH7_MemObjInit(pVm,&sRes);` |
|    45 |  4034 | `	rc = bCatch ? PH7_VmCallMethodSwallow(pVm,pIn,pMethod,&sRes,0,0,&bThrew)` |
|    31 |  4035 | `	            : PH7_VmCallClassMethod(pVm,pIn,pMethod,&sRes,0,0);` |
|    51 |  4036 | `	if( rc != SXRET_OK \|\| bThrew ){` |
|   ! 0 |  4037 | `		PH7_MemObjRelease(&sRes);` |
|   ! 0 |  4038 | `		return rc;` |
|     - |  4039 | `	}` |
|    51 |  4040 | `	pCls = PH7_VmExtractClass(pVm,"RecursiveCachingIterator",` |
|     - |  4041 | `		sizeof("RecursiveCachingIterator")-1,FALSE,0);` |
|    51 |  4042 | `	pMethod = pCls ? PH7_ClassExtractMethod(pCls,"__construct",sizeof("__construct")-1) : 0;` |
|    51 |  4043 | `	if( pMethod == 0 ){` |
|   ! 0 |  4044 | `		PH7_MemObjRelease(&sRes);` |
|   ! 0 |  4045 | `		return SXRET_OK;` |
|     - |  4046 | `	}` |
|    51 |  4047 | `	pChild = PH7_NewClassInstance(pVm,pCls);` |
|    51 |  4048 | `	if( pChild == 0 ){` |
|   ! 0 |  4049 | `		PH7_MemObjRelease(&sRes);` |
|   ! 0 |  4050 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  4051 | `	}` |
|    51 |  4052 | `	pChild->iRef++;` |
|    51 |  4053 | `	PH7_MemObjInitFromInt(pVm,&sFlags,CitFlags(pThis) & CIT_PUBLIC);` |
|    51 |  4054 | `	apCtor[0] = &sRes;` |
|    51 |  4055 | `	apCtor[1] = &sFlags;` |
|    45 |  4056 | `	rc = bCatch ? PH7_VmCallMethodSwallow(pVm,pChild,pMethod,0,2,apCtor,&bThrew)` |
|    31 |  4057 | `	            : PH7_VmCallClassMethod(pVm,pChild,pMethod,0,2,apCtor);` |
|    51 |  4058 | `	PH7_MemObjRelease(&sRes);` |
|    51 |  4059 | `	PH7_MemObjRelease(&sFlags);` |
|    51 |  4060 | `	if( rc == SXRET_OK && !bThrew ){` |
|    47 |  4061 | `		PH7_NativeSetAttrObj(pVm,pThis,CIT_KIDS,pChild);` |
|    23 |  4062 | `	}` |
|    51 |  4063 | `	PH7_ClassInstanceUnref(pChild);` |
|    51 |  4064 | `	return rc;` |
|    72 |  4065 | `}` |
|     - |  4066 | `/*` |
|     - |  4067 | `` * php's `intern->dit_type == DIT_RecursiveCachingIterator`. rewind() and next()`` |
|     - |  4068 | ` * are declared on CachingIterator ALONE and inherited by the twin, so the one body` |
|     - |  4069 | ` * they share has to ask what it is standing on.` |
|     - |  4070 | ` */` |
|   272 |  4071 | `static int CitIsRecursive(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     4 |  4072 | `{` |
|   276 |  4073 | `	ph7_class *pCls = PH7_VmExtractClass(pVm,"RecursiveCachingIterator",` |
|     - |  4074 | `		sizeof("RecursiveCachingIterator")-1,FALSE,0);` |
|   276 |  4075 | `	return pThis && pCls && PH7_VmInstanceOf(pThis->pClass,pCls);` |
|     4 |  4076 | `}` |
|     - |  4077 | `/*` |
|     - |  4078 | ` * php's spl_caching_it_next: fetch, record, and step the inner iterator on. The` |
|     - |  4079 | ` * ORDER below is php's and is observable — a loud inner iterator sees` |
|     - |  4080 | ` * valid/current/key, then hasChildren/getChildren, then the __toString cast, then` |
|     - |  4081 | ` * next.` |
|     - |  4082 | ` */` |
|   272 |  4083 | `static sxi32 CitFetch(ph7_context *pCtx)` |
|     4 |  4084 | `{` |
|   276 |  4085 | `	ph7_vm *pVm = pCtx->pVm;` |
|   276 |  4086 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   276 |  4087 | `	int bRecursive = CitIsRecursive(pVm,pThis);` |
|     - |  4088 | `	sxi64 iFlags;` |
|     - |  4089 | `	sxi32 rc,rcStr;` |
|     - |  4090 | `	/* php's spl_dual_it_free for this type drops the string and the children with` |
|     - |  4091 | ``	 * the cached pair, which is what makes `(string)$it` empty and hasChildren()`` |
|     - |  4092 | `	 * false once the walk has run off the end. */` |
|     - |  4093 | `	{` |
|   276 |  4094 | `		ph7_value *pStr = PH7_NativeAttr(pThis,CIT_STR);` |
|   276 |  4095 | `		if( pStr ){` |
|   276 |  4096 | `			PH7_MemObjRelease(pStr);` |
|   136 |  4097 | `		}` |
|     - |  4098 | `	}` |
|   276 |  4099 | `	PH7_NativeSetAttrObj(pVm,pThis,CIT_KIDS,0);` |
|   276 |  4100 | `	rc = DualFetch(pVm,pThis,TRUE);` |
|   276 |  4101 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  4102 | `		return rc;` |
|     - |  4103 | `	}` |
|   276 |  4104 | `	iFlags = CitFlags(pThis);` |
|   276 |  4105 | `	if( !DualFilled(pThis) ){` |
|    81 |  4106 | `		PH7_NativeSetAttrInt(pVm,pThis,CIT_FL,iFlags & ~(sxi64)CIT_VALID);` |
|    81 |  4107 | `		return SXRET_OK;` |
|     - |  4108 | `	}` |
|   198 |  4109 | `	PH7_NativeSetAttrInt(pVm,pThis,CIT_FL,iFlags \| CIT_VALID);` |
|   198 |  4110 | `	if( iFlags & CIT_FULL_CACHE ){` |
|    36 |  4111 | `		ph7_value *pKey = PH7_NativeAttr(pThis,IT_CK);` |
|    36 |  4112 | `		ph7_value *pCur = PH7_NativeAttr(pThis,IT_CD);` |
|    36 |  4113 | `		if( pKey && pCur ){` |
|     - |  4114 | `			/* php's array_set_zval_key: the WHOLE array-key rule set, which is the` |
|     - |  4115 | `			 * engine's own subscript rule set (PH7_VmArrayKeyArg) — an object or an` |
|     - |  4116 | `			 * array key refused, a RESOURCE key warned about and cached under its` |
|     - |  4117 | `			 * integer id (the string cast had been caching it under the literal` |
|     - |  4118 | `			 * "Resource id #N"), a NULL key deprecated and cached under "".` |
|     - |  4119 | `			 * The pair is copied out of the instance first: the rail rewrites a` |
|     - |  4120 | `			 * resource key in place, which must not touch the iterator's own` |
|     - |  4121 | `			 * key() slot, and its diagnostics can reach a user error handler that` |
|     - |  4122 | `			 * no borrowed ph7_value* survives — the cache slot included, which is` |
|     - |  4123 | `			 * why it is fetched only once the screen is through. */` |
|     - |  4124 | `			ph7_value sKey,sVal,*pCache;` |
|    36 |  4125 | `			PH7_MemObjInit(pVm,&sKey);` |
|    36 |  4126 | `			PH7_MemObjInit(pVm,&sVal);` |
|    36 |  4127 | `			PH7_MemObjStore(pKey,&sKey);` |
|    36 |  4128 | `			PH7_MemObjStore(pCur,&sVal);` |
|    36 |  4129 | `			rc = PH7_VmArrayKeyArg(pCtx,&sKey,PH7_ARRAYKEY_OFFSET);` |
|    36 |  4130 | `			if( rc != SXRET_OK ){` |
|     8 |  4131 | `				PH7_MemObjRelease(&sKey);` |
|     8 |  4132 | `				PH7_MemObjRelease(&sVal);` |
|     8 |  4133 | `				return rc;` |
|     - |  4134 | `			}` |
|    29 |  4135 | `			pCache = CitCacheSlot(pVm,pThis);` |
|    29 |  4136 | `			if( pCache ){` |
|    29 |  4137 | `				ph7_array_add_elem(pCache,&sKey,&sVal);` |
|    13 |  4138 | `			}` |
|    29 |  4139 | `			PH7_MemObjRelease(&sKey);` |
|    29 |  4140 | `			PH7_MemObjRelease(&sVal);` |
|    13 |  4141 | `		}` |
|    13 |  4142 | `	}` |
|   191 |  4143 | `	if( bRecursive ){` |
|     - |  4144 | `		/* php checks EG(exception) here and RETURNS, so a throw from the children` |
|     - |  4145 | `		 * half leaves the inner iterator where it stands. */` |
|   143 |  4146 | `		rc = CitBuildChildren(pCtx,pThis);` |
|   143 |  4147 | `		if( rc != SXRET_OK ){` |
|     5 |  4148 | `			return rc;` |
|     - |  4149 | `		}` |
|    69 |  4150 | `	}` |
|   187 |  4151 | `	rcStr = SXRET_OK;` |
|   187 |  4152 | `	if( iFlags & (CIT_CALL_TOSTRING\|CIT_TOSTRING_USE_INN) ){` |
|    17 |  4153 | `		rcStr = CitMakeString(pCtx,pThis,iFlags);` |
|     8 |  4154 | `	}` |
|     - |  4155 | `	/* php makes no such check after the CAST, so an element with no __toString` |
|     - |  4156 | `	 * throws AND leaves the inner iterator one step on: hasNext() answers from` |
|     - |  4157 | `	 * where the walk really is, not from where the throw interrupted it. */` |
|   187 |  4158 | `	rc = DualNextInnerEx(pVm,pThis,FALSE);` |
|   187 |  4159 | `	return rcStr != SXRET_OK ? rcStr : rc;` |
|   140 |  4160 | `}` |
|   188 |  4161 | `static int CitConstruct(ph7_context *pCtx,const char *zOwner,int nArg,ph7_value **apArg)` |
|     4 |  4162 | `{` |
|   192 |  4163 | `	ph7_vm *pVm = pCtx->pVm;` |
|   192 |  4164 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   192 |  4165 | `	sxi64 iFlags = CIT_CALL_TOSTRING;` |
|     - |  4166 | `	sxi32 rc;` |
|   192 |  4167 | `	if( pThis == 0 ){` |
|   ! 0 |  4168 | `		return PH7_OK;` |
|     - |  4169 | `	}` |
|   192 |  4170 | `	if( nArg > 1 ){` |
|   154 |  4171 | `		iFlags = ph7_value_to_int64(apArg[1]);` |
|    75 |  4172 | `	}` |
|   192 |  4173 | `	if( !CitCheckFlags(iFlags) ){` |
|     4 |  4174 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  4175 | `			"%s::__construct(): Argument #2 ($flags) must contain only one of "` |
|     - |  4176 | `			"CachingIterator::CALL_TOSTRING, CachingIterator::TOSTRING_USE_KEY, "` |
|     - |  4177 | `			"CachingIterator::TOSTRING_USE_CURRENT, or CachingIterator::TOSTRING_USE_INNER",` |
|     1 |  4178 | `			zOwner);` |
|     - |  4179 | `	}` |
|     - |  4180 | `	/* Only the ITERATOR reaches the shared constructor: its second argument is` |
|     - |  4181 | ``	 * IteratorIterator's `$class` downcast, and this one's is an int. */`` |
|   190 |  4182 | `	rc = DualConstruct(pCtx,zOwner,nArg > 0 ? 1 : 0,apArg);` |
|   190 |  4183 | `	if( rc != PH7_OK ){` |
|   ! 0 |  4184 | `		return rc;` |
|     - |  4185 | `	}` |
|   190 |  4186 | `	PH7_NativeSetAttrInt(pVm,pThis,CIT_FL,iFlags & CIT_PUBLIC);` |
|   190 |  4187 | `	return PH7_OK;` |
|    98 |  4188 | `}` |
|    80 |  4189 | `static int vm_builtin_CachingIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     4 |  4190 | `{` |
|    84 |  4191 | `	return CitConstruct(pCtx,"CachingIterator",nArg,apArg);` |
|     4 |  4192 | `}` |
|     - |  4193 | `/*` |
|     - |  4194 | `` * php's stub DECLARES `Iterator $iterator` here and its body then asks for`` |
|     - |  4195 | ` * spl_ce_RecursiveIterator, so Reflection reports the looser type while the` |
|     - |  4196 | ` * refusal names the tighter one. Both halves are reproduced: the signature above` |
|     - |  4197 | ` * is the stub's, this check is the body's.` |
|     - |  4198 | ` */` |
|   118 |  4199 | `static int vm_builtin_RecursiveCachingIterator_construct(ph7_context *pCtx,int nArg,` |
|     - |  4200 | `	ph7_value **apArg)` |
|     1 |  4201 | `{` |
|   119 |  4202 | `	if( nArg > 0 ){` |
|   119 |  4203 | `		ph7_class *pRec = PH7_VmExtractClass(pCtx->pVm,"RecursiveIterator",` |
|     - |  4204 | `			sizeof("RecursiveIterator")-1,FALSE,0);` |
|   178 |  4205 | `		ph7_class_instance *pObj = (apArg[0]->iFlags & MEMOBJ_OBJ)` |
|   116 |  4206 | `			? (ph7_class_instance *)apArg[0]->x.pOther : 0;` |
|   119 |  4207 | `		if( pRec && (pObj == 0 \|\| !PH7_VmInstanceOf(pObj->pClass,pRec)) ){` |
|     - |  4208 | `			char zBuf[64];` |
|    16 |  4209 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|     - |  4210 | `				"RecursiveCachingIterator::__construct(): Argument #1 ($iterator) must be "` |
|     - |  4211 | `				"of type RecursiveIterator, %s given",` |
|     5 |  4212 | `				VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)));` |
|     - |  4213 | `		}` |
|    54 |  4214 | `	}` |
|   109 |  4215 | `	return CitConstruct(pCtx,"RecursiveCachingIterator",nArg,apArg);` |
|    60 |  4216 | `}` |
|   128 |  4217 | `static int vm_builtin_CachingIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     4 |  4218 | `{` |
|   132 |  4219 | `	ph7_vm *pVm = pCtx->pVm;` |
|   132 |  4220 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  4221 | `	ph7_value *pCache;` |
|     - |  4222 | `	sxi32 rc;` |
|    64 |  4223 | `	SXUNUSED(nArg);` |
|    64 |  4224 | `	SXUNUSED(apArg);` |
|   132 |  4225 | `	if( !DualReady(pThis) ){` |
|     3 |  4226 | `		return DualNotReady(pCtx);` |
|     - |  4227 | `	}` |
|   130 |  4228 | `	pCache = PH7_NativeAttr(pThis,CIT_CCH);` |
|   130 |  4229 | `	if( pCache ){` |
|     - |  4230 | `		/* php's zend_hash_clean: a rewind starts the cache over. */` |
|   130 |  4231 | `		PH7_MemObjRelease(pCache);` |
|   130 |  4232 | `		PH7_MemObjToHashmap(pCache);` |
|    63 |  4233 | `	}` |
|   130 |  4234 | `	rc = DualRewindInner(pVm,pThis);` |
|   130 |  4235 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  4236 | `		return rc;` |
|     - |  4237 | `	}` |
|   130 |  4238 | `	rc = CitFetch(pCtx);` |
|   130 |  4239 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|    68 |  4240 | `}` |
|   148 |  4241 | `static int vm_builtin_CachingIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  4242 | `{` |
|   151 |  4243 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  4244 | `	sxi32 rc;` |
|    74 |  4245 | `	SXUNUSED(nArg);` |
|    74 |  4246 | `	SXUNUSED(apArg);` |
|   151 |  4247 | `	if( !DualReady(pThis) ){` |
|     3 |  4248 | `		return DualNotReady(pCtx);` |
|     - |  4249 | `	}` |
|   149 |  4250 | `	rc = CitFetch(pCtx);` |
|   149 |  4251 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|    77 |  4252 | `}` |
|     - |  4253 | `/* valid() is the private CIT_VALID bit, not the inner iterator's answer: the` |
|     - |  4254 | ` * decorator stands on what it fetched and the inner has already moved past it. */` |
|   352 |  4255 | `static int vm_builtin_CachingIterator_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  4256 | `{` |
|   355 |  4257 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   176 |  4258 | `	SXUNUSED(nArg);` |
|   176 |  4259 | `	SXUNUSED(apArg);` |
|   355 |  4260 | `	if( !DualReady(pThis) ){` |
|     3 |  4261 | `		return DualNotReady(pCtx);` |
|     - |  4262 | `	}` |
|   353 |  4263 | `	ph7_result_bool(pCtx,(CitFlags(pThis) & CIT_VALID) != 0);` |
|   353 |  4264 | `	return PH7_OK;` |
|   179 |  4265 | `}` |
|     - |  4266 | `/* hasNext() is the inner iterator's LIVE valid(), which is why moving the inner` |
|     - |  4267 | ` * behind the decorator's back changes the answer. */` |
|   194 |  4268 | `static int vm_builtin_CachingIterator_hasNext(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  4269 | `{` |
|   195 |  4270 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   195 |  4271 | `	int bValid = 0;` |
|     - |  4272 | `	sxi32 rc;` |
|    97 |  4273 | `	SXUNUSED(nArg);` |
|    97 |  4274 | `	SXUNUSED(apArg);` |
|   195 |  4275 | `	if( !DualReady(pThis) ){` |
|     3 |  4276 | `		return DualNotReady(pCtx);` |
|     - |  4277 | `	}` |
|   193 |  4278 | `	rc = DualInnerValid(pCtx->pVm,pThis,&bValid);` |
|   193 |  4279 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  4280 | `		return rc;` |
|     - |  4281 | `	}` |
|   193 |  4282 | `	ph7_result_bool(pCtx,bValid);` |
|   193 |  4283 | `	return PH7_OK;` |
|    98 |  4284 | `}` |
|    22 |  4285 | `static int vm_builtin_CachingIterator_toString(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  4286 | `{` |
|    23 |  4287 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  4288 | `	sxi64 iFlags;` |
|     - |  4289 | `	ph7_value sOut,*pSrc;` |
|     - |  4290 | `	sxi32 rc;` |
|    11 |  4291 | `	SXUNUSED(nArg);` |
|    11 |  4292 | `	SXUNUSED(apArg);` |
|    23 |  4293 | `	if( !DualReady(pThis) ){` |
|     3 |  4294 | `		return DualNotReady(pCtx);` |
|     - |  4295 | `	}` |
|    21 |  4296 | `	iFlags = CitFlags(pThis);` |
|    20 |  4297 | `	if( (iFlags & (CIT_CALL_TOSTRING\|CIT_TOSTRING_USE_KEY\|CIT_TOSTRING_USE_CUR` |
|    11 |  4298 | `		\|CIT_TOSTRING_USE_INN)) == 0 ){` |
|     5 |  4299 | `		return CitRefuse(pCtx,pThis,"fetch string value");` |
|     - |  4300 | `	}` |
|    17 |  4301 | `	if( iFlags & (CIT_TOSTRING_USE_KEY\|CIT_TOSTRING_USE_CUR) ){` |
|     - |  4302 | `		/* Read out of the CACHE at call time, converted then and there. */` |
|     5 |  4303 | `		pSrc = PH7_NativeAttr(pThis,(iFlags & CIT_TOSTRING_USE_KEY) ? IT_CK : IT_CD);` |
|     5 |  4304 | `		PH7_MemObjInit(pCtx->pVm,&sOut);` |
|     5 |  4305 | `		if( pSrc ){` |
|     5 |  4306 | `			PH7_MemObjStore(pSrc,&sOut);` |
|     2 |  4307 | `		}` |
|     5 |  4308 | `		rc = PH7_MemObjToStringUV(&sOut);` |
|     5 |  4309 | `		if( rc == SXRET_OK ){` |
|     5 |  4310 | `			ph7_result_value(pCtx,&sOut);` |
|     2 |  4311 | `		}` |
|     5 |  4312 | `		PH7_MemObjRelease(&sOut);` |
|     5 |  4313 | `		return rc == SXRET_OK ? PH7_OK : rc;` |
|     - |  4314 | `	}` |
|    13 |  4315 | `	pSrc = PH7_NativeAttr(pThis,CIT_STR);` |
|    13 |  4316 | `	if( pSrc && (pSrc->iFlags & MEMOBJ_STRING) ){` |
|    11 |  4317 | `		ph7_result_value(pCtx,pSrc);` |
|     6 |  4318 | `	}else{` |
|     - |  4319 | ``		/* php's `zstr is not a string` — nothing has been fetched. */`` |
|     3 |  4320 | `		ph7_result_string(pCtx,"",0);` |
|     - |  4321 | `	}` |
|    13 |  4322 | `	return PH7_OK;` |
|    12 |  4323 | `}` |
|    32 |  4324 | `static int vm_builtin_CachingIterator_getFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  4325 | `{` |
|    33 |  4326 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    16 |  4327 | `	SXUNUSED(nArg);` |
|    16 |  4328 | `	SXUNUSED(apArg);` |
|    33 |  4329 | `	if( !DualReady(pThis) ){` |
|     3 |  4330 | `		return DualNotReady(pCtx);` |
|     - |  4331 | `	}` |
|    31 |  4332 | `	ph7_result_int64(pCtx,CitFlags(pThis));` |
|    31 |  4333 | `	return PH7_OK;` |
|    17 |  4334 | `}` |
|    10 |  4335 | `static int vm_builtin_CachingIterator_setFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  4336 | `{` |
|    11 |  4337 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  4338 | `	sxi64 iOld,iNew;` |
|    11 |  4339 | `	if( nArg < 1 ){` |
|   ! 0 |  4340 | `		return PH7_OK;` |
|     - |  4341 | `	}` |
|    11 |  4342 | `	iNew = ph7_value_to_int64(apArg[0]);` |
|    11 |  4343 | `	if( !CitCheckFlags(iNew) ){` |
|     3 |  4344 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  4345 | `			"CachingIterator::setFlags(): Argument #1 ($flags) must contain only one of "` |
|     - |  4346 | `			"CachingIterator::CALL_TOSTRING, CachingIterator::TOSTRING_USE_KEY, "` |
|     - |  4347 | `			"CachingIterator::TOSTRING_USE_CURRENT, or CachingIterator::TOSTRING_USE_INNER");` |
|     - |  4348 | `	}` |
|     9 |  4349 | `	if( !DualReady(pThis) ){` |
|   ! 0 |  4350 | `		return DualNotReady(pCtx);` |
|     - |  4351 | `	}` |
|     - |  4352 | `	/* The two eager spellings are computed at FETCH time, so php refuses to turn` |
|     - |  4353 | `	 * either off mid-walk rather than leaving a stale string behind. */` |
|     9 |  4354 | `	iOld = CitFlags(pThis);` |
|     9 |  4355 | `	if( (iOld & CIT_CALL_TOSTRING) != 0 && (iNew & CIT_CALL_TOSTRING) == 0 ){` |
|     5 |  4356 | `		return PH7_VmThrowException(pCtx,"InvalidArgumentException",` |
|     - |  4357 | `			"Unsetting flag CALL_TO_STRING is not possible");` |
|     - |  4358 | `	}` |
|     5 |  4359 | `	if( (iOld & CIT_TOSTRING_USE_INN) != 0 && (iNew & CIT_TOSTRING_USE_INN) == 0 ){` |
|     3 |  4360 | `		return PH7_VmThrowException(pCtx,"InvalidArgumentException",` |
|     - |  4361 | `			"Unsetting flag TOSTRING_USE_INNER is not possible");` |
|     - |  4362 | `	}` |
|     3 |  4363 | `	PH7_NativeSetAttrInt(pCtx->pVm,pThis,CIT_FL,(iOld & ~(sxi64)CIT_PUBLIC) \| (iNew & CIT_PUBLIC));` |
|     3 |  4364 | `	return PH7_OK;` |
|     6 |  4365 | `}` |
|    20 |  4366 | `static int vm_builtin_CachingIterator_getCache(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  4367 | `{` |
|     - |  4368 | `	ph7_value *pCache;` |
|     - |  4369 | `	int rc;` |
|    10 |  4370 | `	SXUNUSED(nArg);` |
|    10 |  4371 | `	SXUNUSED(apArg);` |
|    23 |  4372 | `	pCache = CitCacheChecked(pCtx,&rc);` |
|    23 |  4373 | `	if( pCache == 0 ){` |
|     5 |  4374 | `		return rc;` |
|     - |  4375 | `	}` |
|    19 |  4376 | `	ph7_result_value(pCtx,pCache);` |
|    19 |  4377 | `	return PH7_OK;` |
|    13 |  4378 | `}` |
|    14 |  4379 | `static int vm_builtin_CachingIterator_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  4380 | `{` |
|     - |  4381 | `	ph7_value *pCache;` |
|     - |  4382 | `	int rc;` |
|     7 |  4383 | `	SXUNUSED(nArg);` |
|     7 |  4384 | `	SXUNUSED(apArg);` |
|    15 |  4385 | `	pCache = CitCacheChecked(pCtx,&rc);` |
|    15 |  4386 | `	if( pCache == 0 ){` |
|     5 |  4387 | `		return rc;` |
|     - |  4388 | `	}` |
|    11 |  4389 | `	ph7_result_int64(pCtx,pCache->x.pOther ? ((ph7_hashmap *)pCache->x.pOther)->nEntry : 0);` |
|    11 |  4390 | `	return PH7_OK;` |
|     8 |  4391 | `}` |
|     - |  4392 | `/*` |
|     - |  4393 | `` * The four ArrayAccess members read and write that same cache. php's `$key` is`` |
|     - |  4394 | ` * DECLARED untyped and screened as a string by the body, so a non-stringable key` |
|     - |  4395 | `` * is a TypeError naming `string` while an int or a float becomes an array key the`` |
|     - |  4396 | ` * ordinary way.` |
|     - |  4397 | ` */` |
|    32 |  4398 | `static ph7_value * CitOffsetKey(ph7_context *pCtx,const char *zMethod,ph7_value *pKey,` |
|     - |  4399 | `	ph7_value *pOut,int *pRc)` |
|     2 |  4400 | `{` |
|     - |  4401 | `	char zBuf[64];` |
|    34 |  4402 | `	*pRc = PH7_OK;` |
|    34 |  4403 | `	if( !PH7_ArgSatisfiesString(pKey) ){` |
|    20 |  4404 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|     - |  4405 | `			"CachingIterator::%s(): Argument #1 ($key) must be of type string, %s given",` |
|     6 |  4406 | `			zMethod,VmValueGivenName(pKey,zBuf,sizeof(zBuf)));` |
|    14 |  4407 | `		return 0;` |
|     - |  4408 | `	}` |
|    22 |  4409 | `	PH7_MemObjInit(pCtx->pVm,pOut);` |
|    22 |  4410 | `	PH7_MemObjStore(pKey,pOut);` |
|    22 |  4411 | `	if( PH7_MemObjToString(pOut) != SXRET_OK ){` |
|   ! 0 |  4412 | `		PH7_MemObjRelease(pOut);` |
|   ! 0 |  4413 | `		*pRc = PH7_OK;` |
|   ! 0 |  4414 | `		return 0;` |
|     - |  4415 | `	}` |
|    22 |  4416 | `	return pOut;` |
|    18 |  4417 | `}` |
|    16 |  4418 | `static int vm_builtin_CachingIterator_offsetGet(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  4419 | `{` |
|     - |  4420 | `	ph7_value *pCache,*pKey,sKey;` |
|    18 |  4421 | `	ph7_hashmap_node *pNode = 0;` |
|     - |  4422 | `	int rc;` |
|    18 |  4423 | `	pCache = CitCacheChecked(pCtx,&rc);` |
|    18 |  4424 | `	if( pCache == 0 ){` |
|     3 |  4425 | `		return rc;` |
|     - |  4426 | `	}` |
|    16 |  4427 | `	if( nArg < 1 ){` |
|   ! 0 |  4428 | `		return PH7_OK;` |
|     - |  4429 | `	}` |
|    16 |  4430 | `	pKey = CitOffsetKey(pCtx,"offsetGet",apArg[0],&sKey,&rc);` |
|    16 |  4431 | `	if( pKey == 0 ){` |
|     6 |  4432 | `		return rc;` |
|     - |  4433 | `	}` |
|    11 |  4434 | `	if( PH7_HashmapLookup((ph7_hashmap *)pCache->x.pOther,pKey,&pNode) == SXRET_OK ){` |
|     5 |  4435 | `		ph7_value *pVal = HashmapExtractNodeValue(pNode);` |
|     5 |  4436 | `		if( pVal ){` |
|     5 |  4437 | `			ph7_result_value(pCtx,pVal);` |
|     2 |  4438 | `		}` |
|     3 |  4439 | `	}else{` |
|     - |  4440 | `		/* php reads the cache as an ARRAY here, warning included — but it has already` |
|     - |  4441 | `		 * cast the key to a STRING, so the key is QUOTED even where a plain array read` |
|     - |  4442 | ``		 * would print a bare integer ($c[0] on a missing key says `"0"`). */`` |
|     - |  4443 | `		SyBlob sMsg;` |
|     - |  4444 | `		SyString sKeyText;` |
|     7 |  4445 | `		int nKey = 0;` |
|     7 |  4446 | `		const char *zKey = ph7_value_to_string(pKey,&nKey);` |
|     7 |  4447 | `		SyStringInitFromBuf(&sKeyText,zKey,(sxu32)nKey);` |
|     7 |  4448 | `		SyBlobInit(&sMsg,&pCtx->pVm->sAllocator);` |
|     7 |  4449 | `		SyBlobFormat(&sMsg,"Undefined array key \"%z\"",&sKeyText);` |
|     7 |  4450 | `		SyBlobNullAppend(&sMsg);` |
|     7 |  4451 | `		PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,(const char *)SyBlobData(&sMsg));` |
|     7 |  4452 | `		SyBlobRelease(&sMsg);` |
|     - |  4453 | `	}` |
|    11 |  4454 | `	PH7_MemObjRelease(&sKey);` |
|    11 |  4455 | `	return PH7_OK;` |
|    10 |  4456 | `}` |
|    10 |  4457 | `static int vm_builtin_CachingIterator_offsetExists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  4458 | `{` |
|     - |  4459 | `	ph7_value *pCache,*pKey,sKey;` |
|    12 |  4460 | `	ph7_hashmap_node *pNode = 0;` |
|     - |  4461 | `	int rc;` |
|    12 |  4462 | `	pCache = CitCacheChecked(pCtx,&rc);` |
|    12 |  4463 | `	if( pCache == 0 ){` |
|     3 |  4464 | `		return rc;` |
|     - |  4465 | `	}` |
|    10 |  4466 | `	if( nArg < 1 ){` |
|   ! 0 |  4467 | `		return PH7_OK;` |
|     - |  4468 | `	}` |
|    10 |  4469 | `	pKey = CitOffsetKey(pCtx,"offsetExists",apArg[0],&sKey,&rc);` |
|    10 |  4470 | `	if( pKey == 0 ){` |
|     3 |  4471 | `		return rc;` |
|     - |  4472 | `	}` |
|    11 |  4473 | `	ph7_result_bool(pCtx,` |
|     6 |  4474 | `		PH7_HashmapLookup((ph7_hashmap *)pCache->x.pOther,pKey,&pNode) == SXRET_OK);` |
|     8 |  4475 | `	PH7_MemObjRelease(&sKey);` |
|     8 |  4476 | `	return PH7_OK;` |
|     7 |  4477 | `}` |
|     8 |  4478 | `static int vm_builtin_CachingIterator_offsetSet(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  4479 | `{` |
|     - |  4480 | `	ph7_value *pCache,*pKey,sKey;` |
|     - |  4481 | `	int rc;` |
|    10 |  4482 | `	pCache = CitCacheChecked(pCtx,&rc);` |
|    10 |  4483 | `	if( pCache == 0 ){` |
|     3 |  4484 | `		return rc;` |
|     - |  4485 | `	}` |
|     8 |  4486 | `	if( nArg < 2 ){` |
|   ! 0 |  4487 | `		return PH7_OK;` |
|     - |  4488 | `	}` |
|     8 |  4489 | `	pKey = CitOffsetKey(pCtx,"offsetSet",apArg[0],&sKey,&rc);` |
|     8 |  4490 | `	if( pKey == 0 ){` |
|     5 |  4491 | `		return rc;` |
|     - |  4492 | `	}` |
|     3 |  4493 | `	ph7_array_add_elem(pCache,pKey,apArg[1]);` |
|     3 |  4494 | `	PH7_MemObjRelease(&sKey);` |
|     3 |  4495 | `	return PH7_OK;` |
|     6 |  4496 | `}` |
|     6 |  4497 | `static int vm_builtin_CachingIterator_offsetUnset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  4498 | `{` |
|     - |  4499 | `	ph7_value *pCache,*pKey,sKey;` |
|     8 |  4500 | `	ph7_hashmap_node *pNode = 0;` |
|     - |  4501 | `	int rc;` |
|     8 |  4502 | `	pCache = CitCacheChecked(pCtx,&rc);` |
|     8 |  4503 | `	if( pCache == 0 ){` |
|     3 |  4504 | `		return rc;` |
|     - |  4505 | `	}` |
|     6 |  4506 | `	if( nArg < 1 ){` |
|   ! 0 |  4507 | `		return PH7_OK;` |
|     - |  4508 | `	}` |
|     6 |  4509 | `	pKey = CitOffsetKey(pCtx,"offsetUnset",apArg[0],&sKey,&rc);` |
|     6 |  4510 | `	if( pKey == 0 ){` |
|     3 |  4511 | `		return rc;` |
|     - |  4512 | `	}` |
|     3 |  4513 | `	if( PH7_HashmapLookup((ph7_hashmap *)pCache->x.pOther,pKey,&pNode) == SXRET_OK ){` |
|     3 |  4514 | `		PH7_HashmapUnlinkNode(pNode,TRUE);` |
|     1 |  4515 | `	}` |
|     3 |  4516 | `	PH7_MemObjRelease(&sKey);` |
|     3 |  4517 | `	return PH7_OK;` |
|     5 |  4518 | `}` |
|     - |  4519 | `/* The recursive twin answers what the FETCH built and nothing else: no children` |
|     - |  4520 | ` * means the fetch found none, and two calls hand back the SAME object. */` |
|   130 |  4521 | `static int vm_builtin_RecursiveCachingIterator_hasChildren(ph7_context *pCtx,int nArg,` |
|     - |  4522 | `	ph7_value **apArg)` |
|     1 |  4523 | `{` |
|   131 |  4524 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    65 |  4525 | `	SXUNUSED(nArg);` |
|    65 |  4526 | `	SXUNUSED(apArg);` |
|   131 |  4527 | `	if( !DualReady(pThis) ){` |
|     3 |  4528 | `		return DualNotReady(pCtx);` |
|     - |  4529 | `	}` |
|   129 |  4530 | `	ph7_result_bool(pCtx,PH7_NativeAttrObj(pThis,CIT_KIDS) != 0);` |
|   129 |  4531 | `	return PH7_OK;` |
|    66 |  4532 | `}` |
|    48 |  4533 | `static int vm_builtin_RecursiveCachingIterator_getChildren(ph7_context *pCtx,int nArg,` |
|     - |  4534 | `	ph7_value **apArg)` |
|     1 |  4535 | `{` |
|    49 |  4536 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  4537 | `	ph7_class_instance *pKids;` |
|    24 |  4538 | `	SXUNUSED(nArg);` |
|    24 |  4539 | `	SXUNUSED(apArg);` |
|    49 |  4540 | `	if( !DualReady(pThis) ){` |
|   ! 0 |  4541 | `		return DualNotReady(pCtx);` |
|     - |  4542 | `	}` |
|    49 |  4543 | `	pKids = PH7_NativeAttrObj(pThis,CIT_KIDS);` |
|    49 |  4544 | `	if( pKids ){` |
|    45 |  4545 | `		SplResultBorrowed(pCtx,pKids);` |
|    23 |  4546 | `	}else{` |
|     5 |  4547 | `		ph7_result_null(pCtx);` |
|     - |  4548 | `	}` |
|    49 |  4549 | `	return PH7_OK;` |
|    25 |  4550 | `}` |
|     - |  4551 | `/*` |
|     - |  4552 | ` * The declarations. php's method ORDER is the order Reflection reports, so each` |
|     - |  4553 | ` * table follows spl_iterators.stub.php line for line; the parameter types are the` |
|     - |  4554 | `` * stub's too, which is what makes `Iterator $iterator` refuse an IteratorAggregate`` |
|     - |  4555 | ` * everywhere except IteratorIterator (the one class that declares Traversable and` |
|     - |  4556 | ` * unwraps).` |
|     - |  4557 | ` *` |
|     - |  4558 | ` * No RETURN type is declared, on purpose: php marks every one of these` |
|     - |  4559 | `` * `@tentative-return-type`, and a tentative type answers NULL from getReturnType()`` |
|     - |  4560 | ` * and false from hasReturnType() — which is exactly what an undeclared zRet answers` |
|     - |  4561 | `` * here. Declaring them would print `Return [ bool ]` where php prints`` |
|     - |  4562 | `` * `Tentative return [ bool ]` AND make getReturnType() disagree; leaving them off`` |
|     - |  4563 | ` * costs only getTentativeReturnType(). PHL has no tentative-return concept at all` |
|     - |  4564 | ` * (recorded) — DateTime and the reflectors already report a plain return type where php` |
|     - |  4565 | ` * reports a tentative one.` |
|     - |  4566 | ` */` |
|  8445 |  4567 | `static sxi32 VmInstallSplDualIterators(ph7_vm *pVm)` |
|     5 |  4568 | `{` |
|     - |  4569 | `	static const PH7_NativePropDef aDualProp[] = {` |
|     - |  4570 | `		{ IT_IN, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  4571 | `		{ IT_IT, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  4572 | `		{ IT_CD, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  4573 | `		{ IT_CK, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  4574 | `		{ IT_CF, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  4575 | `		{ IT_CP, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  4576 | `	};` |
|     - |  4577 | `	static const PH7_NativePropDef aLimitProp[] = {` |
|     - |  4578 | `		{ IT_OFF, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  4579 | `		{ IT_LIM, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, -1, 0, 0.0 }, 0 },` |
|     - |  4580 | `	};` |
|     - |  4581 | `	static const PH7_NativePropDef aCbProp[] = {` |
|     - |  4582 | `		{ IT_CB, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  4583 | `	};` |
|     - |  4584 | `	/* php types the SPL contracts as it types the core interfaces: a TENTATIVE` |
|     - |  4585 | `	 * return on every method, so an iterator written before php 8.1 still` |
|     - |  4586 | `	 * satisfies them. Both of the ones declared here, and SeekableIterator's` |
|     - |  4587 | ``	 * `seek` above, had no return type at all. */`` |
|     - |  4588 | `	static const PH7_NativeMethodDef aOuterMethod[] = {` |
|     - |  4589 | `		{ "getInnerIterator", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@?Iterator", 0 },` |
|     - |  4590 | `	};` |
|     - |  4591 | `	static const PH7_NativeMethodDef aIterIterMethod[] = {` |
|     - |  4592 | `		{ "__construct",      PH7_MOD_PUBLIC, "Traversable $iterator, ?string $class = null", 0,` |
|     - |  4593 | `		  vm_builtin_IteratorIterator_construct },` |
|     - |  4594 | `		{ "getInnerIterator", PH7_MOD_PUBLIC, "", "@?Iterator", vm_builtin_Dual_getInnerIterator },` |
|     - |  4595 | `		{ "rewind",           PH7_MOD_PUBLIC, "", "@void", vm_builtin_IteratorIterator_rewind },` |
|     - |  4596 | `		{ "valid",            PH7_MOD_PUBLIC, "", "@bool", vm_builtin_Dual_valid },` |
|     - |  4597 | `		{ "key",              PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_Dual_key },` |
|     - |  4598 | `		{ "current",          PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_Dual_current },` |
|     - |  4599 | `		{ "next",             PH7_MOD_PUBLIC, "", "@void", vm_builtin_IteratorIterator_next },` |
|     - |  4600 | `	};` |
|     - |  4601 | `	static const PH7_NativeMethodDef aFilterMethod[] = {` |
|     - |  4602 | `		{ "accept",      PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@bool", 0 },` |
|     - |  4603 | `		{ "__construct", PH7_MOD_PUBLIC, "Iterator $iterator", 0, vm_builtin_FilterIterator_construct },` |
|     - |  4604 | `		{ "rewind",      PH7_MOD_PUBLIC, "", "@void", vm_builtin_FilterIterator_rewind },` |
|     - |  4605 | `		{ "next",        PH7_MOD_PUBLIC, "", "@void", vm_builtin_FilterIterator_next },` |
|     - |  4606 | `	};` |
|     - |  4607 | `	static const PH7_NativeMethodDef aCbFilterMethod[] = {` |
|     - |  4608 | `		{ "__construct", PH7_MOD_PUBLIC, "Iterator $iterator, callable $callback", 0,` |
|     - |  4609 | `		  vm_builtin_CallbackFilterIterator_construct },` |
|     - |  4610 | `		{ "accept",      PH7_MOD_PUBLIC, "", "@bool", vm_builtin_CallbackFilterIterator_accept },` |
|     - |  4611 | `	};` |
|     - |  4612 | `	static const PH7_NativeMethodDef aLimitMethod[] = {` |
|     - |  4613 | `		{ "__construct", PH7_MOD_PUBLIC, "Iterator $iterator, int $offset = 0, int $limit = -1", 0,` |
|     - |  4614 | `		  vm_builtin_LimitIterator_construct },` |
|     - |  4615 | `		{ "rewind",      PH7_MOD_PUBLIC, "", "@void", vm_builtin_LimitIterator_rewind },` |
|     - |  4616 | `		{ "valid",       PH7_MOD_PUBLIC, "", "@bool", vm_builtin_LimitIterator_valid },` |
|     - |  4617 | `		{ "next",        PH7_MOD_PUBLIC, "", "@void", vm_builtin_LimitIterator_next },` |
|     - |  4618 | `		{ "seek",        PH7_MOD_PUBLIC, "int $offset", "@int", vm_builtin_LimitIterator_seek },` |
|     - |  4619 | `		{ "getPosition", PH7_MOD_PUBLIC, "", "@int", vm_builtin_LimitIterator_getPosition },` |
|     - |  4620 | `	};` |
|     - |  4621 | `	static const PH7_NativeMethodDef aInfiniteMethod[] = {` |
|     - |  4622 | `		{ "__construct", PH7_MOD_PUBLIC, "Iterator $iterator", 0,` |
|     - |  4623 | `		  vm_builtin_InfiniteIterator_construct },` |
|     - |  4624 | `		{ "next",        PH7_MOD_PUBLIC, "", "@void", vm_builtin_InfiniteIterator_next },` |
|     - |  4625 | `	};` |
|     - |  4626 | `	static const PH7_NativeMethodDef aNoRewindMethod[] = {` |
|     - |  4627 | `		{ "__construct", PH7_MOD_PUBLIC, "Iterator $iterator", 0,` |
|     - |  4628 | `		  vm_builtin_NoRewindIterator_construct },` |
|     - |  4629 | `		{ "rewind",      PH7_MOD_PUBLIC, "", "@void", vm_builtin_NoRewindIterator_rewind },` |
|     - |  4630 | `		{ "valid",       PH7_MOD_PUBLIC, "", "@bool", vm_builtin_NoRewindIterator_valid },` |
|     - |  4631 | `		{ "key",         PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_NoRewindIterator_key },` |
|     - |  4632 | `		{ "current",     PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_NoRewindIterator_current },` |
|     - |  4633 | `		{ "next",        PH7_MOD_PUBLIC, "", "@void", vm_builtin_NoRewindIterator_next },` |
|     - |  4634 | `	};` |
|     - |  4635 | `	static const PH7_NativePropDef aRegexProp[] = {` |
|     - |  4636 | `		/* The one slot php PRESENTS, declared as php declares it: a ?string, so a` |
|     - |  4637 | ``		 * `$it->replacement = 5` coerces and an array is a TypeError. */`` |
|     - |  4638 | `		{ "replacement", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, "?string" },` |
|     - |  4639 | `		{ IT_RE, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },` |
|     - |  4640 | `		{ IT_RM, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  4641 | `		{ IT_RF, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  4642 | `		{ IT_RP, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  4643 | `	};` |
|     - |  4644 | `	static const PH7_NativeConstDef aRegexConst[] = {` |
|     - |  4645 | `		{ "USE_KEY",      PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, REGIT_USE_KEY,  0, 0.0 },` |
|     - |  4646 | `		{ "INVERT_MATCH", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, REGIT_INVERTED, 0, 0.0 },` |
|     - |  4647 | `		{ "MATCH",        PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PH7_REGIT_MATCH,       0, 0.0 },` |
|     - |  4648 | `		{ "GET_MATCH",    PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PH7_REGIT_GET_MATCH,   0, 0.0 },` |
|     - |  4649 | `		{ "ALL_MATCHES",  PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PH7_REGIT_ALL_MATCHES, 0, 0.0 },` |
|     - |  4650 | `		{ "SPLIT",        PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PH7_REGIT_SPLIT,       0, 0.0 },` |
|     - |  4651 | `		{ "REPLACE",      PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PH7_REGIT_REPLACE,     0, 0.0 },` |
|     - |  4652 | `	};` |
|     - |  4653 | `	static const PH7_NativeMethodDef aRegexMethod[] = {` |
|     - |  4654 | `		{ "__construct",  PH7_MOD_PUBLIC,` |
|     - |  4655 | ``		  /* php's stub spells this default `RegexIterator::MATCH`, and one zSig field`` |
|     - |  4656 | `		   * cannot say both the TEXT and the VALUE: the constant spelling prints php's` |
|     - |  4657 | `		   * export line but makes getDefaultValue() a "Failed to retrieve" throw, so the` |
|     - |  4658 | `		   * VALUE wins here, as it does in the aBuiltinSig rows with the same shape. */` |
|     - |  4659 | `		  "Iterator $iterator, string $pattern, int $mode = RegexIterator::MATCH, "` |
|     - |  4660 | `		  "int $flags = 0, int $pregFlags = 0", 0,` |
|     - |  4661 | `		  vm_builtin_RegexIterator_construct },` |
|     - |  4662 | `		{ "accept",       PH7_MOD_PUBLIC, "", "@bool", vm_builtin_RegexIterator_accept },` |
|     - |  4663 | `		{ "getMode",      PH7_MOD_PUBLIC, "", "@int", vm_builtin_RegexIterator_getMode },` |
|     - |  4664 | `		{ "setMode",      PH7_MOD_PUBLIC, "int $mode", "@void", vm_builtin_RegexIterator_setMode },` |
|     - |  4665 | `		{ "getFlags",     PH7_MOD_PUBLIC, "", "@int", vm_builtin_RegexIterator_getFlags },` |
|     - |  4666 | `		{ "setFlags",     PH7_MOD_PUBLIC, "int $flags", "@void", vm_builtin_RegexIterator_setFlags },` |
|     - |  4667 | `		{ "getRegex",     PH7_MOD_PUBLIC, "", "@string", vm_builtin_RegexIterator_getRegex },` |
|     - |  4668 | `		{ "getPregFlags", PH7_MOD_PUBLIC, "", "@int", vm_builtin_RegexIterator_getPregFlags },` |
|     - |  4669 | `		{ "setPregFlags", PH7_MOD_PUBLIC, "int $pregFlags", "@void", vm_builtin_RegexIterator_setPregFlags },` |
|     - |  4670 | `	};` |
|     - |  4671 | `	static const PH7_NativeMethodDef aRecursiveMethod[] = {` |
|     - |  4672 | `		{ "hasChildren", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@bool", 0 },` |
|     - |  4673 | `		{ "getChildren", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@?RecursiveIterator", 0 },` |
|     - |  4674 | `	};` |
|     - |  4675 | `	static const PH7_NativeConstDef aRaiConst[] = {` |
|     - |  4676 | `		{ "CHILD_ARRAYS_ONLY", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, RAI_CHILD_ARRAYS_ONLY, 0, 0.0 },` |
|     - |  4677 | `	};` |
|     - |  4678 | `	static const PH7_NativeMethodDef aRaiMethod[] = {` |
|     - |  4679 | `		{ "hasChildren", PH7_MOD_PUBLIC, "", "@bool",` |
|     - |  4680 | `		  vm_builtin_RecursiveArrayIterator_hasChildren },` |
|     - |  4681 | `		{ "getChildren", PH7_MOD_PUBLIC, "", "@?RecursiveArrayIterator",` |
|     - |  4682 | `		  vm_builtin_RecursiveArrayIterator_getChildren },` |
|     - |  4683 | `	};` |
|     - |  4684 | `	static const PH7_NativeMethodDef aRfiMethod[] = {` |
|     - |  4685 | `		{ "__construct", PH7_MOD_PUBLIC, "RecursiveIterator $iterator", 0,` |
|     - |  4686 | `		  vm_builtin_RecursiveFilterIterator_construct },` |
|     - |  4687 | `		{ "hasChildren", PH7_MOD_PUBLIC, "", "@bool",` |
|     - |  4688 | `		  vm_builtin_RecursiveFilterIterator_hasChildren },` |
|     - |  4689 | `		{ "getChildren", PH7_MOD_PUBLIC, "", "@?RecursiveFilterIterator",` |
|     - |  4690 | `		  vm_builtin_RecursiveFilterIterator_getChildren },` |
|     - |  4691 | `	};` |
|     - |  4692 | `	static const PH7_NativeMethodDef aParentMethod[] = {` |
|     - |  4693 | `		{ "__construct", PH7_MOD_PUBLIC, "RecursiveIterator $iterator", 0,` |
|     - |  4694 | `		  vm_builtin_ParentIterator_construct },` |
|     - |  4695 | `		{ "accept",      PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ParentIterator_accept },` |
|     - |  4696 | `	};` |
|     - |  4697 | `	static const PH7_NativeMethodDef aRcbfMethod[] = {` |
|     - |  4698 | `		{ "__construct", PH7_MOD_PUBLIC, "RecursiveIterator $iterator, callable $callback", 0,` |
|     - |  4699 | `		  vm_builtin_RecursiveCallbackFilterIterator_construct },` |
|     - |  4700 | `		{ "hasChildren", PH7_MOD_PUBLIC, "", "@bool",` |
|     - |  4701 | `		  vm_builtin_RecursiveFilterIterator_hasChildren },` |
|     - |  4702 | `		/* NOT nullable, where the RecursiveFilterIterator row above it is: php's` |
|     - |  4703 | `		 * nine getChildren stubs disagree with each other about a child every` |
|     - |  4704 | `		 * one of them builds the same way, and the declaration is what a` |
|     - |  4705 | `		 * program reads. */` |
|     - |  4706 | `		{ "getChildren", PH7_MOD_PUBLIC, "", "@RecursiveCallbackFilterIterator",` |
|     - |  4707 | `		  vm_builtin_RecursiveCallbackFilterIterator_getChildren },` |
|     - |  4708 | `	};` |
|     - |  4709 | `	static const PH7_NativeMethodDef aRregexMethod[] = {` |
|     - |  4710 | `		{ "__construct", PH7_MOD_PUBLIC,` |
|     - |  4711 | `		  "RecursiveIterator $iterator, string $pattern, "` |
|     - |  4712 | `		  "int $mode = RecursiveRegexIterator::MATCH, int $flags = 0, "` |
|     - |  4713 | `		  "int $pregFlags = 0", 0,` |
|     - |  4714 | `		  vm_builtin_RecursiveRegexIterator_construct },` |
|     - |  4715 | `		{ "accept",      PH7_MOD_PUBLIC, "", "@bool", vm_builtin_RecursiveRegexIterator_accept },` |
|     - |  4716 | `		{ "hasChildren", PH7_MOD_PUBLIC, "", "@bool",` |
|     - |  4717 | `		  vm_builtin_RecursiveFilterIterator_hasChildren },` |
|     - |  4718 | `		{ "getChildren", PH7_MOD_PUBLIC, "", "@RecursiveRegexIterator",` |
|     - |  4719 | `		  vm_builtin_RecursiveRegexIterator_getChildren },` |
|     - |  4720 | `	};` |
|     - |  4721 | `	static const PH7_NativeConstDef aCitConst[] = {` |
|     - |  4722 | `		{ "CALL_TOSTRING",       PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, CIT_CALL_TOSTRING, 0, 0.0 },` |
|     - |  4723 | `		{ "CATCH_GET_CHILD",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, CIT_CATCH_GET_CHILD, 0, 0.0 },` |
|     - |  4724 | `		{ "TOSTRING_USE_KEY",    PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, CIT_TOSTRING_USE_KEY, 0, 0.0 },` |
|     - |  4725 | `		{ "TOSTRING_USE_CURRENT",PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, CIT_TOSTRING_USE_CUR, 0, 0.0 },` |
|     - |  4726 | `		{ "TOSTRING_USE_INNER",  PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, CIT_TOSTRING_USE_INN, 0, 0.0 },` |
|     - |  4727 | `		{ "FULL_CACHE",          PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, CIT_FULL_CACHE, 0, 0.0 },` |
|     - |  4728 | `	};` |
|     - |  4729 | `	static const PH7_NativePropDef aCitProp[] = {` |
|     - |  4730 | `		{ CIT_FL,   PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  4731 | `		{ CIT_STR,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  4732 | `		{ CIT_CCH,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  4733 | `		{ CIT_KIDS, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  4734 | `	};` |
|     - |  4735 | `	static const PH7_NativeMethodDef aCitMethod[] = {` |
|     - |  4736 | `		{ "__construct", PH7_MOD_PUBLIC,` |
|     - |  4737 | `		  "Iterator $iterator, int $flags = CachingIterator::CALL_TOSTRING", 0,` |
|     - |  4738 | `		  vm_builtin_CachingIterator_construct },` |
|     - |  4739 | `		{ "rewind",      PH7_MOD_PUBLIC, "", "@void", vm_builtin_CachingIterator_rewind },` |
|     - |  4740 | `		{ "valid",       PH7_MOD_PUBLIC, "", "@bool", vm_builtin_CachingIterator_valid },` |
|     - |  4741 | `		{ "next",        PH7_MOD_PUBLIC, "", "@void", vm_builtin_CachingIterator_next },` |
|     - |  4742 | `		{ "hasNext",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_CachingIterator_hasNext },` |
|     - |  4743 | `		{ "__toString",  PH7_MOD_PUBLIC, "", "string", vm_builtin_CachingIterator_toString },` |
|     - |  4744 | `		{ "getFlags",    PH7_MOD_PUBLIC, "", "@int", vm_builtin_CachingIterator_getFlags },` |
|     - |  4745 | `		{ "setFlags",    PH7_MOD_PUBLIC, "int $flags", "@void", vm_builtin_CachingIterator_setFlags },` |
|     - |  4746 | `		{ "offsetGet",   PH7_MOD_PUBLIC, "$key", "@mixed", vm_builtin_CachingIterator_offsetGet },` |
|     - |  4747 | `		{ "offsetSet",   PH7_MOD_PUBLIC, "$key, mixed $value", "@void",` |
|     - |  4748 | `		  vm_builtin_CachingIterator_offsetSet },` |
|     - |  4749 | `		{ "offsetUnset", PH7_MOD_PUBLIC, "$key", "@void", vm_builtin_CachingIterator_offsetUnset },` |
|     - |  4750 | `		{ "offsetExists",PH7_MOD_PUBLIC, "$key", "@bool", vm_builtin_CachingIterator_offsetExists },` |
|     - |  4751 | `		{ "getCache",    PH7_MOD_PUBLIC, "", "@array", vm_builtin_CachingIterator_getCache },` |
|     - |  4752 | `		{ "count",       PH7_MOD_PUBLIC, "", "@int", vm_builtin_CachingIterator_count },` |
|     - |  4753 | `	};` |
|     - |  4754 | `	static const PH7_NativeMethodDef aRcitMethod[] = {` |
|     - |  4755 | ``		/* `~Iterator`: php DECLARES Iterator here and its body asks for a`` |
|     - |  4756 | `		 * RecursiveIterator, so the screen stands aside and the constructor below` |
|     - |  4757 | `		 * raises php's own refusal. */` |
|     - |  4758 | `		{ "__construct", PH7_MOD_PUBLIC,` |
|     - |  4759 | `		  "~Iterator $iterator, int $flags = RecursiveCachingIterator::CALL_TOSTRING", 0,` |
|     - |  4760 | `		  vm_builtin_RecursiveCachingIterator_construct },` |
|     - |  4761 | `		{ "hasChildren", PH7_MOD_PUBLIC, "", "@bool",` |
|     - |  4762 | `		  vm_builtin_RecursiveCachingIterator_hasChildren },` |
|     - |  4763 | `		{ "getChildren", PH7_MOD_PUBLIC, "", "@?RecursiveCachingIterator",` |
|     - |  4764 | `		  vm_builtin_RecursiveCachingIterator_getChildren },` |
|     - |  4765 | `	};` |
|     - |  4766 | `	static const PH7_NativePropDef aAppendProp[] = {` |
|     - |  4767 | `		{ AP_LIST, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  4768 | `	};` |
|     - |  4769 | `	static const PH7_NativeMethodDef aAppendMethod[] = {` |
|     - |  4770 | `		{ "__construct",      PH7_MOD_PUBLIC, "", 0, vm_builtin_AppendIterator_construct },` |
|     - |  4771 | `		{ "append",           PH7_MOD_PUBLIC, "Iterator $iterator", "@void",` |
|     - |  4772 | `		  vm_builtin_AppendIterator_append },` |
|     - |  4773 | `		{ "rewind",           PH7_MOD_PUBLIC, "", "@void", vm_builtin_AppendIterator_rewind },` |
|     - |  4774 | `		{ "valid",            PH7_MOD_PUBLIC, "", "@bool", vm_builtin_Dual_valid },` |
|     - |  4775 | `		{ "current",          PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_AppendIterator_current },` |
|     - |  4776 | `		{ "next",             PH7_MOD_PUBLIC, "", "@void", vm_builtin_AppendIterator_next },` |
|     - |  4777 | `		{ "getIteratorIndex", PH7_MOD_PUBLIC, "", "@?int",` |
|     - |  4778 | `		  vm_builtin_AppendIterator_getIteratorIndex },` |
|     - |  4779 | `		{ "getArrayIterator", PH7_MOD_PUBLIC, "", "@ArrayIterator",` |
|     - |  4780 | `		  vm_builtin_AppendIterator_getArrayIterator },` |
|     - |  4781 | `	};` |
|     - |  4782 | `	static const PH7_NativeMethodDef aEmptyMethod[] = {` |
|     - |  4783 | `		{ "current", PH7_MOD_PUBLIC, "", "@never", vm_builtin_EmptyIterator_current },` |
|     - |  4784 | `		{ "next",    PH7_MOD_PUBLIC, "", "@void", vm_builtin_EmptyIterator_nop },` |
|     - |  4785 | `		{ "key",     PH7_MOD_PUBLIC, "", "@never", vm_builtin_EmptyIterator_key },` |
|     - |  4786 | `		{ "valid",   PH7_MOD_PUBLIC, "", "@false", vm_builtin_EmptyIterator_valid },` |
|     - |  4787 | `		{ "rewind",  PH7_MOD_PUBLIC, "", "@void", vm_builtin_EmptyIterator_nop },` |
|     - |  4788 | `	};` |
|     - |  4789 | `	/*` |
|     - |  4790 | ``	 * PH7_CLASS_NOCLONE on every dual iterator: php refuses `clone` for all of them`` |
|     - |  4791 | `	 * (its inner iterator handle cannot be duplicated), and a slot-by-slot copy here` |
|     - |  4792 | `	 * would share the inner iterator's cursor between two decorators. EmptyIterator` |
|     - |  4793 | `	 * has no state and php clones it happily.` |
|     - |  4794 | `	 */` |
|     - |  4795 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - |  4796 | `		{ "OuterIterator", "Iterator", 0, PH7_CLASS_INTERFACE,` |
|     - |  4797 | `		  aOuterMethod, SX_ARRAYSIZE(aOuterMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - |  4798 | `		{ "IteratorIterator", 0, "OuterIterator", PH7_CLASS_NOCLONE,` |
|     - |  4799 | `		  aIterIterMethod, SX_ARRAYSIZE(aIterIterMethod), 0, 0,` |
|     - |  4800 | `		  aDualProp, SX_ARRAYSIZE(aDualProp), 0, 0, 0 },` |
|     - |  4801 | `		{ "FilterIterator", "IteratorIterator", 0, PH7_CLASS_ABSTRACT\|PH7_CLASS_NOCLONE,` |
|     - |  4802 | `		  aFilterMethod, SX_ARRAYSIZE(aFilterMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - |  4803 | `		{ "CallbackFilterIterator", "FilterIterator", 0, PH7_CLASS_NOCLONE,` |
|     - |  4804 | `		  aCbFilterMethod, SX_ARRAYSIZE(aCbFilterMethod), 0, 0,` |
|     - |  4805 | `		  aCbProp, SX_ARRAYSIZE(aCbProp), 0, 0, 0 },` |
|     - |  4806 | `		{ "LimitIterator", "IteratorIterator", 0, PH7_CLASS_NOCLONE,` |
|     - |  4807 | `		  aLimitMethod, SX_ARRAYSIZE(aLimitMethod), 0, 0,` |
|     - |  4808 | `		  aLimitProp, SX_ARRAYSIZE(aLimitProp), 0, 0, 0 },` |
|     - |  4809 | `		{ "InfiniteIterator", "IteratorIterator", 0, PH7_CLASS_NOCLONE,` |
|     - |  4810 | `		  aInfiniteMethod, SX_ARRAYSIZE(aInfiniteMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - |  4811 | `		{ "NoRewindIterator", "IteratorIterator", 0, PH7_CLASS_NOCLONE,` |
|     - |  4812 | `		  aNoRewindMethod, SX_ARRAYSIZE(aNoRewindMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - |  4813 | `		{ "RegexIterator", "FilterIterator", 0, PH7_CLASS_NOCLONE,` |
|     - |  4814 | `		  aRegexMethod, SX_ARRAYSIZE(aRegexMethod),` |
|     - |  4815 | `		  aRegexConst, SX_ARRAYSIZE(aRegexConst),` |
|     - |  4816 | `		  aRegexProp, SX_ARRAYSIZE(aRegexProp), 0, 0, 0 },` |
|     - |  4817 | `		{ "AppendIterator", "IteratorIterator", 0, PH7_CLASS_NOCLONE,` |
|     - |  4818 | `		  aAppendMethod, SX_ARRAYSIZE(aAppendMethod), 0, 0,` |
|     - |  4819 | `		  aAppendProp, SX_ARRAYSIZE(aAppendProp), 0, 0, 0 },` |
|     - |  4820 | `		{ "RecursiveIterator", "Iterator", 0, PH7_CLASS_INTERFACE,` |
|     - |  4821 | `		  aRecursiveMethod, SX_ARRAYSIZE(aRecursiveMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - |  4822 | `		/* RecursiveArrayIterator is CLONEABLE (php clones an ArrayIterator happily) and` |
|     - |  4823 | `		 * inherits every one of its parent's C bodies, storage slots included. */` |
|     - |  4824 | `		{ "RecursiveArrayIterator", "ArrayIterator", "RecursiveIterator", 0,` |
|     - |  4825 | `		  aRaiMethod, SX_ARRAYSIZE(aRaiMethod),` |
|     - |  4826 | `		  aRaiConst, SX_ARRAYSIZE(aRaiConst), 0, 0, 0, 0, 0 },` |
|     - |  4827 | `		{ "RecursiveFilterIterator", "FilterIterator", "RecursiveIterator",` |
|     - |  4828 | `		  PH7_CLASS_ABSTRACT\|PH7_CLASS_NOCLONE,` |
|     - |  4829 | `		  aRfiMethod, SX_ARRAYSIZE(aRfiMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - |  4830 | `		/* The three recursive twins. The two whose parent is a PLAIN filter name` |
|     - |  4831 | `		 * RecursiveIterator themselves; ParentIterator inherits it from` |
|     - |  4832 | `		 * RecursiveFilterIterator, which is where php has it too. */` |
|     - |  4833 | `		{ "ParentIterator", "RecursiveFilterIterator", 0, PH7_CLASS_NOCLONE,` |
|     - |  4834 | `		  aParentMethod, SX_ARRAYSIZE(aParentMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - |  4835 | `		{ "RecursiveCallbackFilterIterator", "CallbackFilterIterator", "RecursiveIterator",` |
|     - |  4836 | `		  PH7_CLASS_NOCLONE,` |
|     - |  4837 | `		  aRcbfMethod, SX_ARRAYSIZE(aRcbfMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - |  4838 | `		{ "RecursiveRegexIterator", "RegexIterator", "RecursiveIterator", PH7_CLASS_NOCLONE,` |
|     - |  4839 | `		  aRregexMethod, SX_ARRAYSIZE(aRregexMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - |  4840 | `		/* CachingIterator is Stringable through __toString, and Countable/ArrayAccess` |
|     - |  4841 | `		 * over the FULL_CACHE array — three interfaces the class refuses to serve` |
|     - |  4842 | `		 * unless it was built with that flag. */` |
|     - |  4843 | `		{ "CachingIterator", "IteratorIterator", "ArrayAccess,Countable,Stringable",` |
|     - |  4844 | `		  PH7_CLASS_NOCLONE,` |
|     - |  4845 | `		  aCitMethod, SX_ARRAYSIZE(aCitMethod), aCitConst, SX_ARRAYSIZE(aCitConst),` |
|     - |  4846 | `		  aCitProp, SX_ARRAYSIZE(aCitProp), 0, 0, 0 },` |
|     - |  4847 | `		{ "RecursiveCachingIterator", "CachingIterator", "RecursiveIterator", PH7_CLASS_NOCLONE,` |
|     - |  4848 | `		  aRcitMethod, SX_ARRAYSIZE(aRcitMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - |  4849 | `		{ "EmptyIterator", 0, "Iterator", 0,` |
|     - |  4850 | `		  aEmptyMethod, SX_ARRAYSIZE(aEmptyMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - |  4851 | `	};` |
|  8450 |  4852 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|     5 |  4853 | `}` |
|     - |  4854 | `/*` |
|     - |  4855 | ` * ---------------------------------------------------------------------------` |
|     - |  4856 | ` * RecursiveIteratorIterator.` |
|     - |  4857 | ` *` |
|     - |  4858 | `` * php's `spl_recursive_it_object` is a STACK OF LEVELS plus a five-value state`` |
|     - |  4859 | ` * machine, and reading the struct before the methods (rule 43) is what this` |
|     - |  4860 | ` * conversion turns on. Each level carries the sub-iterator AND its own` |
|     - |  4861 | `` * RecursiveIteratorState; `move_forward` is one loop over that pair, and every`` |
|     - |  4862 | ` * method is a two-line reader of it. The chunk instead kept a stack of iterators` |
|     - |  4863 | `` * with the state implied by two booleans (`__post`, `__live`), which is where all`` |
|     - |  4864 | ` * eight of its divergences came from:` |
|     - |  4865 | ` *` |
|     - |  4866 | ` *   - getDepth()/getSubIterator()/getInnerIterator() answered from an EMPTY stack` |
|     - |  4867 | ` *     before the first rewind(), so they reported -1 and null where php reports 0` |
|     - |  4868 | ` *     and the root -- php seeds level 0 in the CONSTRUCTOR and never unseeds it.` |
|     - |  4869 | `` *   - valid() answered a `__live` flag that only rewind() sets; php ASKS the`` |
|     - |  4870 | ` *     levels (any valid sub-iterator, walking down), so a fresh instance over a` |
|     - |  4871 | ` *     non-empty iterator is already valid().` |
|     - |  4872 | ` *   - LEAVES_ONLY past max depth YIELDED the container; php skips it, which is` |
|     - |  4873 | ``  *     the whole point of the mode (`walk-leaves-maxdepth0` returned the `b` `` |
|     - |  4874 | ` *     array as if it were a leaf).` |
|     - |  4875 | `` *   - the mode was `$mode \| $flags` masked with & 3, so CATCH_GET_CHILD passed`` |
|     - |  4876 | ` *     as $mode descended like LEAVES_ONLY; php compares mode EXACTLY and an` |
|     - |  4877 | ` *     unknown mode matches no arm at all, descending nowhere.` |
|     - |  4878 | ` *   - hasChildren() was called on the sub-iterator DIRECTLY, so a subclass` |
|     - |  4879 | ` *     overriding callHasChildren() -- php's documented hook -- was never asked.` |
|     - |  4880 | ` *   - endChildren() ran AFTER the pop, reporting a depth one too low and firing` |
|     - |  4881 | ` *     a spurious final call at depth -1; php calls it before the pop.` |
|     - |  4882 | ` *   - a second rewind() fired beginIteration() again; php's in_iteration latch` |
|     - |  4883 | ` *     makes it once per iteration.` |
|     - |  4884 | ` *   - getChildren() returning a non-RecursiveIterator was silently treated as` |
|     - |  4885 | ` *     "no children"; php throws UnexpectedValueException.` |
|     - |  4886 | ` *` |
|     - |  4887 | ` * The level stack lives in two parallel arrays indexed by level rather than in a` |
|     - |  4888 | `` * C block behind a handle: php SERIALIZES this class (`O:25:"…":0:{}`), and a raw`` |
|     - |  4889 | ` * pointer in a hidden slot is exactly what rule 19 exists to keep out of` |
|     - |  4890 | ` * serialize() output. Every slot is PH7_MOD_HIDDEN, so php's zero-property` |
|     - |  4891 | ` * presentation holds for var_dump, print_r, var_export, (array) and Reflection.` |
|     - |  4892 | ` */` |
|     - |  4893 | `#define RIT_ST   "__st"   /* php's iterators[level].zobject */` |
|     - |  4894 | `#define RIT_SS   "__ss"   /* php's iterators[level].state */` |
|     - |  4895 | `#define RIT_LVL  "__lvl"  /* php's object->level */` |
|     - |  4896 | `#define RIT_MD   "__md"   /* php's object->mode, stored UNMASKED */` |
|     - |  4897 | `#define RIT_FL   "__fl"   /* php's object->flags */` |
|     - |  4898 | `#define RIT_MX   "__mx"   /* php's object->max_depth, -1 = unlimited */` |
|     - |  4899 | `#define RIT_II   "__ii"   /* php's object->in_iteration */` |
|     - |  4900 | ``#define RIT_RD   "__rd"   /* php's `object->iterators != NULL`: the parent ctor ran */`` |
|     - |  4901 |  |
|     - |  4902 | `/* php's RecursiveIteratorState */` |
|     - |  4903 | `#define RS_NEXT  0` |
|     - |  4904 | `#define RS_TEST  1` |
|     - |  4905 | `#define RS_SELF  2` |
|     - |  4906 | `#define RS_CHILD 3` |
|     - |  4907 | `#define RS_START 4` |
|     - |  4908 |  |
|     - |  4909 | `/* php's RecursiveIteratorMode + the one flag */` |
|     - |  4910 | `#define RIT_LEAVES_ONLY     0` |
|     - |  4911 | `#define RIT_SELF_FIRST      1` |
|     - |  4912 | `#define RIT_CHILD_FIRST     2` |
|     - |  4913 | `#define RIT_CATCH_GET_CHILD 16` |
|     - |  4914 |  |
|     - |  4915 | `/*` |
|     - |  4916 | `` * php's `object->iterators != NULL`. Its get_method handler refuses EVERY method`` |
|     - |  4917 | ` * on an instance whose parent constructor never ran -- not the individual bodies,` |
|     - |  4918 | ` * which is why the refusal is an Error naming the RUNTIME class and why even` |
|     - |  4919 | ` * getDepth() raises it.` |
|     - |  4920 | ` */` |
|  3436 |  4921 | `static int RitReady(ph7_class_instance *pThis)` |
|     3 |  4922 | `{` |
|  3439 |  4923 | `	return pThis && PH7_NativeAttrInt(pThis,RIT_RD) != 0;` |
|     3 |  4924 | `}` |
|    20 |  4925 | `static sxi32 RitNotReady(ph7_context *pCtx)` |
|     1 |  4926 | `{` |
|    21 |  4927 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    21 |  4928 | `	SyString *pName = pThis ? &pThis->pClass->sName : 0;` |
|    31 |  4929 | `	return PH7_VmThrowException(pCtx,"Error",` |
|    10 |  4930 | `		"The %z instance wasn't initialized properly",pName);` |
|     1 |  4931 | `}` |
|  5076 |  4932 | `static int RitInt(ph7_class_instance *pThis,const char *zSlot)` |
|     3 |  4933 | `{` |
|  5079 |  4934 | `	return (int)PH7_NativeAttrInt(pThis,zSlot);` |
|     3 |  4935 | `}` |
|     - |  4936 | `/* One of the two level-indexed arrays, materialized on first use. */` |
|  6590 |  4937 | `static ph7_hashmap * RitMap(ph7_vm *pVm,ph7_class_instance *pThis,const char *zSlot)` |
|     3 |  4938 | `{` |
|  6593 |  4939 | `	ph7_value *pSlot = PH7_NativeAttr(pThis,zSlot);` |
|  6593 |  4940 | `	if( pSlot == 0 ){` |
|   ! 0 |  4941 | `		return 0;` |
|     - |  4942 | `	}` |
|  6593 |  4943 | `	if( (pSlot->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|   295 |  4944 | `		if( PH7_MemObjToHashmap(pSlot) != SXRET_OK ){` |
|   ! 0 |  4945 | `			return 0;` |
|     - |  4946 | `		}` |
|   146 |  4947 | `	}` |
|  6593 |  4948 | `	return PH7_HashmapCowSeparate(pVm,pSlot);` |
|  3298 |  4949 | `}` |
|  4392 |  4950 | `static ph7_value * RitAt(ph7_vm *pVm,ph7_class_instance *pThis,const char *zSlot,int iLevel)` |
|     3 |  4951 | `{` |
|  4395 |  4952 | `	ph7_hashmap *pMap = RitMap(pVm,pThis,zSlot);` |
|  4395 |  4953 | `	ph7_hashmap_node *pNode = 0;` |
|  4395 |  4954 | `	if( pMap == 0 \|\| HashmapLookupIntKey(pMap,(sxi64)iLevel,&pNode) != SXRET_OK ){` |
|   ! 0 |  4955 | `		return 0;` |
|     - |  4956 | `	}` |
|  4395 |  4957 | `	return HashmapExtractNodeValue(pNode);` |
|  2199 |  4958 | `}` |
|  1654 |  4959 | `static void RitPut(ph7_vm *pVm,ph7_class_instance *pThis,const char *zSlot,int iLevel,ph7_value *pVal)` |
|     3 |  4960 | `{` |
|  1657 |  4961 | `	ph7_hashmap *pMap = RitMap(pVm,pThis,zSlot);` |
|     - |  4962 | `	ph7_value sKey;` |
|  1657 |  4963 | `	if( pMap == 0 ){` |
|   ! 0 |  4964 | `		return;` |
|     - |  4965 | `	}` |
|  1657 |  4966 | `	PH7_MemObjInitFromInt(pVm,&sKey,(sxi64)iLevel);` |
|  1657 |  4967 | `	PH7_HashmapInsert(pMap,&sKey,pVal);` |
|  1657 |  4968 | `	PH7_MemObjRelease(&sKey);` |
|   830 |  4969 | `}` |
|   544 |  4970 | `static void RitErase(ph7_vm *pVm,ph7_class_instance *pThis,const char *zSlot,int iLevel)` |
|     3 |  4971 | `{` |
|   547 |  4972 | `	ph7_hashmap *pMap = RitMap(pVm,pThis,zSlot);` |
|   547 |  4973 | `	ph7_hashmap_node *pNode = 0;` |
|   547 |  4974 | `	if( pMap && HashmapLookupIntKey(pMap,(sxi64)iLevel,&pNode) == SXRET_OK ){` |
|   255 |  4975 | `		PH7_HashmapUnlinkNode(pNode,TRUE);` |
|   126 |  4976 | `	}` |
|   547 |  4977 | `}` |
|     - |  4978 | `/*` |
|     - |  4979 | ` * The sub-iterator at a level. Re-resolved on every use on purpose: every call into` |
|     - |  4980 | ` * a user iterator can rewrite the level's own storage (rule 47). It used to matter` |
|     - |  4981 | ` * for a second reason as well -- RitAt() hands back a pointer into pVm->aMemObj and` |
|     - |  4982 | ` * the pool reallocated as the VM reserved objects -- and that one is gone since P1.` |
|     - |  4983 | ` */` |
|  3572 |  4984 | `static ph7_class_instance * RitSub(ph7_vm *pVm,ph7_class_instance *pThis,int iLevel)` |
|     3 |  4985 | `{` |
|  3575 |  4986 | `	ph7_value *pVal = RitAt(pVm,pThis,RIT_ST,iLevel);` |
|  3575 |  4987 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|   ! 0 |  4988 | `		return 0;` |
|     - |  4989 | `	}` |
|  3575 |  4990 | `	return (ph7_class_instance *)pVal->x.pOther;` |
|  1789 |  4991 | `}` |
|   820 |  4992 | `static int RitState(ph7_vm *pVm,ph7_class_instance *pThis,int iLevel)` |
|     3 |  4993 | `{` |
|   823 |  4994 | `	ph7_value *pVal = RitAt(pVm,pThis,RIT_SS,iLevel);` |
|   823 |  4995 | `	return pVal ? (int)ph7_value_to_int64(pVal) : RS_START;` |
|     3 |  4996 | `}` |
|  1382 |  4997 | `static void RitSetState(ph7_vm *pVm,ph7_class_instance *pThis,int iLevel,int iState)` |
|     3 |  4998 | `{` |
|     - |  4999 | `	ph7_value sVal;` |
|  1385 |  5000 | `	PH7_MemObjInitFromInt(pVm,&sVal,(sxi64)iState);` |
|  1385 |  5001 | `	RitPut(pVm,pThis,RIT_SS,iLevel,&sVal);` |
|  1385 |  5002 | `	PH7_MemObjRelease(&sVal);` |
|  1385 |  5003 | `}` |
|     - |  5004 | ``/* php's `iterators = erealloc(…, ++level+1)` plus the two field writes. */`` |
|   126 |  5005 | `static void RitPush(ph7_vm *pVm,ph7_class_instance *pThis,ph7_class_instance *pChild)` |
|     3 |  5006 | `{` |
|   129 |  5007 | `	int iLevel = RitInt(pThis,RIT_LVL) + 1;` |
|     - |  5008 | `	ph7_value sObj;` |
|   129 |  5009 | `	PH7_MemObjInit(pVm,&sObj);` |
|   129 |  5010 | `	sObj.x.pOther = pChild;` |
|   129 |  5011 | `	MemObjSetType(&sObj,MEMOBJ_OBJ);` |
|     - |  5012 | `	/* The map takes its OWN reference through the store; the carrier is blanked` |
|     - |  5013 | `	 * rather than released, because releasing a MEMOBJ_OBJ carrier would unref an` |
|     - |  5014 | `	 * instance this frame never referenced (rule 16). */` |
|   129 |  5015 | `	RitPut(pVm,pThis,RIT_ST,iLevel,&sObj);` |
|   129 |  5016 | `	sObj.x.pOther = 0;` |
|   129 |  5017 | `	MemObjSetType(&sObj,MEMOBJ_NULL);` |
|   129 |  5018 | `	PH7_MemObjRelease(&sObj);` |
|   129 |  5019 | `	RitSetState(pVm,pThis,iLevel,RS_START);` |
|   129 |  5020 | `	PH7_NativeSetAttrInt(pVm,pThis,RIT_LVL,iLevel);` |
|   129 |  5021 | `}` |
|   126 |  5022 | `static void RitPop(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     3 |  5023 | `{` |
|   129 |  5024 | `	int iLevel = RitInt(pThis,RIT_LVL);` |
|   129 |  5025 | `	if( iLevel <= 0 ){` |
|   ! 0 |  5026 | `		return;` |
|     - |  5027 | `	}` |
|   129 |  5028 | `	RitErase(pVm,pThis,RIT_ST,iLevel);` |
|   129 |  5029 | `	RitErase(pVm,pThis,RIT_SS,iLevel);` |
|   129 |  5030 | `	PH7_NativeSetAttrInt(pVm,pThis,RIT_LVL,iLevel-1);` |
|    66 |  5031 | `}` |
|     - |  5032 | `/* Drop every level: php's spl_RecursiveIteratorIterator_free_iterators. */` |
|   146 |  5033 | `static void RitClear(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     3 |  5034 | `{` |
|   149 |  5035 | `	int iLevel = RitInt(pThis,RIT_LVL);` |
|   295 |  5036 | `	while( iLevel >= 0 ){` |
|   149 |  5037 | `		RitErase(pVm,pThis,RIT_ST,iLevel);` |
|   149 |  5038 | `		RitErase(pVm,pThis,RIT_SS,iLevel);` |
|   149 |  5039 | `		iLevel--;` |
|     3 |  5040 | `	}` |
|   149 |  5041 | `	PH7_NativeSetAttrInt(pVm,pThis,RIT_LVL,0);` |
|   149 |  5042 | `}` |
|     - |  5043 | `/*` |
|     - |  5044 | ` * Call a method, optionally SWALLOWING what it throws -- php clears the exception` |
|     - |  5045 | ` * at four sites when RIT_CATCH_GET_CHILD is set, and PH7_VmCallMethodSwallow is` |
|     - |  5046 | ` * the only way to spell that here (a throw raised under a C call site is` |
|     - |  5047 | ` * dispatched INLINE, so an enclosing user catch would run before this returns).` |
|     - |  5048 | ` * *pbThrew reports a swallowed throw, which php reads back as "retval is UNDEF".` |
|     - |  5049 | ` */` |
|  3956 |  5050 | `static sxi32 RitCall(ph7_context *pCtx,ph7_class_instance *pObj,const char *zName,sxu32 nName,` |
|     - |  5051 | `	ph7_value *pOut,int bCatch,int *pbThrew)` |
|     3 |  5052 | `{` |
|  3959 |  5053 | `	ph7_class_method *pMethod = pObj ? PH7_ClassExtractMethod(pObj->pClass,zName,nName) : 0;` |
|  3959 |  5054 | `	if( pbThrew ){` |
|  1505 |  5055 | `		*pbThrew = FALSE;` |
|   751 |  5056 | `	}` |
|  3959 |  5057 | `	if( pMethod == 0 ){` |
|   ! 0 |  5058 | `		return SXRET_OK;` |
|     - |  5059 | `	}` |
|  3959 |  5060 | `	if( bCatch ){` |
|    13 |  5061 | `		return PH7_VmCallMethodSwallow(pCtx->pVm,pObj,pMethod,pOut,0,0,pbThrew);` |
|     - |  5062 | `	}` |
|  3947 |  5063 | `	return PH7_VmCallClassMethod(pCtx->pVm,pObj,pMethod,pOut,0,0);` |
|  1981 |  5064 | `}` |
|     - |  5065 | `/*` |
|     - |  5066 | ` * A hook on $this. php caches which of the seven the SUBCLASS overrides and calls` |
|     - |  5067 | ` * the sub-iterator directly when none does; dispatching through $this every time` |
|     - |  5068 | ` * reaches the same body -- the base ones are the no-ops php would have skipped --` |
|     - |  5069 | ` * with the override found automatically.` |
|     - |  5070 | ` */` |
|  1312 |  5071 | `static sxi32 RitHook(ph7_context *pCtx,const char *zName,sxu32 nName,ph7_value *pOut,` |
|     - |  5072 | `	int bCatch,int *pbThrew)` |
|     3 |  5073 | `{` |
|  1315 |  5074 | `	return RitCall(pCtx,PH7_ContextThis(pCtx),zName,nName,pOut,bCatch,pbThrew);` |
|     3 |  5075 | `}` |
|   428 |  5076 | `static int RitCatches(ph7_class_instance *pThis)` |
|     3 |  5077 | `{` |
|   431 |  5078 | `	return (RitInt(pThis,RIT_FL) & RIT_CATCH_GET_CHILD) != 0;` |
|     3 |  5079 | `}` |
|     - |  5080 | `/*` |
|     - |  5081 | ` * php's spl_recursive_it_move_forward_ex, transcribed. The switch's fallthroughs` |
|     - |  5082 | ` * (RS_NEXT into RS_START into RS_TEST) are written as a sequential if-chain, and` |
|     - |  5083 | `` * php's `goto next_step` is this loop's `continue`.`` |
|     - |  5084 | ` */` |
|   428 |  5085 | `static sxi32 RitMoveForward(ph7_context *pCtx)` |
|     3 |  5086 | `{` |
|   431 |  5087 | `	ph7_vm *pVm = pCtx->pVm;` |
|   431 |  5088 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  5089 | `	int bCatch;` |
|   431 |  5090 | `	if( !RitReady(pThis) ){` |
|   ! 0 |  5091 | `		return RitNotReady(pCtx);` |
|     - |  5092 | `	}` |
|   431 |  5093 | `	bCatch = RitCatches(pThis);` |
|   473 |  5094 | `	for(;;){` |
|     - |  5095 | `		ph7_class_instance *pSub;` |
|   823 |  5096 | `		int iLevel = RitInt(pThis,RIT_LVL);` |
|   823 |  5097 | `		int iState = RitState(pVm,pThis,iLevel);` |
|   823 |  5098 | `		int bThrew = 0;` |
|   823 |  5099 | `		int bExhausted = 0;` |
|     - |  5100 | `		sxi32 rc;` |
|   823 |  5101 | `		pSub = RitSub(pVm,pThis,iLevel);` |
|   823 |  5102 | `		if( pSub == 0 ){` |
|   ! 0 |  5103 | `			return PH7_OK;` |
|     - |  5104 | `		}` |
|   823 |  5105 | `		if( iState == RS_NEXT ){` |
|   383 |  5106 | `			rc = RitCall(pCtx,pSub,"next",sizeof("next")-1,0,bCatch,&bThrew);` |
|   383 |  5107 | `			if( rc != SXRET_OK ){` |
|   ! 0 |  5108 | `				return rc;` |
|     - |  5109 | `			}` |
|   383 |  5110 | `			pSub = RitSub(pVm,pThis,iLevel);   /* the call may have rewritten the level */` |
|   383 |  5111 | `			if( pSub == 0 ){` |
|   ! 0 |  5112 | `				return PH7_OK;` |
|     - |  5113 | `			}` |
|   383 |  5114 | `			iState = RS_START;                 /* php's fallthrough */` |
|   190 |  5115 | `		}` |
|   823 |  5116 | `		if( iState == RS_START ){` |
|     - |  5117 | `			ph7_value sValid;` |
|   615 |  5118 | `			PH7_MemObjInit(pVm,&sValid);` |
|   615 |  5119 | `			rc = RitCall(pCtx,pSub,"valid",sizeof("valid")-1,&sValid,FALSE,0);` |
|   615 |  5120 | `			if( rc != SXRET_OK ){` |
|   ! 0 |  5121 | `				PH7_MemObjRelease(&sValid);` |
|   ! 0 |  5122 | `				return rc;` |
|     - |  5123 | `			}` |
|   615 |  5124 | `			bExhausted = !ph7_value_to_bool(&sValid);` |
|   615 |  5125 | `			PH7_MemObjRelease(&sValid);` |
|   615 |  5126 | `			if( !bExhausted ){` |
|     - |  5127 | `				/* php re-reads the level here and returns outright when the valid()` |
|     - |  5128 | `				 * call RE-ENTERED this iterator (a sub-iterator that drove the` |
|     - |  5129 | `				 * decorator behind its back); the stack it was walking is gone. */` |
|   403 |  5130 | `				if( RitInt(pThis,RIT_LVL) != iLevel \|\| RitSub(pVm,pThis,iLevel) != pSub ){` |
|   ! 0 |  5131 | `					return PH7_OK;` |
|     - |  5132 | `				}` |
|   403 |  5133 | `				RitSetState(pVm,pThis,iLevel,RS_TEST);` |
|   403 |  5134 | `				iState = RS_TEST;` |
|   200 |  5135 | `			}` |
|   306 |  5136 | `		}` |
|   823 |  5137 | `		if( !bExhausted && iState == RS_TEST ){` |
|     - |  5138 | `			ph7_value sHas;` |
|   403 |  5139 | `			int bDescend = 0;` |
|   403 |  5140 | `			PH7_MemObjInit(pVm,&sHas);` |
|   403 |  5141 | `			rc = RitHook(pCtx,"callHasChildren",sizeof("callHasChildren")-1,&sHas,bCatch,&bThrew);` |
|   403 |  5142 | `			if( rc != SXRET_OK ){` |
|     - |  5143 | `				/* php leaves the level on RS_NEXT so a caught-and-resumed traversal` |
|     - |  5144 | `				 * moves on rather than re-asking the same element. */` |
|   ! 0 |  5145 | `				RitSetState(pVm,pThis,iLevel,RS_NEXT);` |
|   ! 0 |  5146 | `				PH7_MemObjRelease(&sHas);` |
|   ! 0 |  5147 | `				return rc;` |
|     - |  5148 | `			}` |
|     - |  5149 | `			/* A SWALLOWED throw leaves php's retval UNDEF, which skips the` |
|     - |  5150 | `			 * has-children test entirely and yields the element. */` |
|   403 |  5151 | `			if( !bThrew && ph7_value_to_bool(&sHas) ){` |
|   153 |  5152 | `				int iMax = RitInt(pThis,RIT_MX);` |
|   153 |  5153 | `				int iMode = RitInt(pThis,RIT_MD);` |
|   153 |  5154 | `				if( iMax == -1 \|\| iMax > iLevel ){` |
|     - |  5155 | `					/* php compares the mode EXACTLY: an unrecognized mode matches no` |
|     - |  5156 | `					 * arm, falls out of the switch and yields without descending. */` |
|   141 |  5157 | `					if( iMode == RIT_LEAVES_ONLY \|\| iMode == RIT_CHILD_FIRST ){` |
|    78 |  5158 | `						RitSetState(pVm,pThis,iLevel,RS_CHILD);` |
|    78 |  5159 | `						bDescend = 1;` |
|   102 |  5160 | `					}else if( iMode == RIT_SELF_FIRST ){` |
|    60 |  5161 | `						RitSetState(pVm,pThis,iLevel,RS_SELF);` |
|    60 |  5162 | `						bDescend = 1;` |
|    32 |  5163 | `					}` |
|    82 |  5164 | `				}else if( iMode == RIT_LEAVES_ONLY ){` |
|     - |  5165 | `					/* Too deep to recurse into and NOT a leaf, so php skips it —` |
|     - |  5166 | `					 * the mode's defining rule, and the one the chunk dropped. */` |
|     5 |  5167 | `					RitSetState(pVm,pThis,iLevel,RS_NEXT);` |
|     5 |  5168 | `					bDescend = 1;` |
|     2 |  5169 | `				}` |
|    75 |  5170 | `			}` |
|   403 |  5171 | `			PH7_MemObjRelease(&sHas);` |
|   403 |  5172 | `			if( bDescend ){` |
|   141 |  5173 | `				continue;                      /* php's goto next_step */` |
|     - |  5174 | `			}` |
|   265 |  5175 | `			rc = RitHook(pCtx,"nextElement",sizeof("nextElement")-1,0,bCatch,&bThrew);` |
|   265 |  5176 | `			RitSetState(pVm,pThis,iLevel,RS_NEXT);` |
|   265 |  5177 | `			if( rc != SXRET_OK ){` |
|   ! 0 |  5178 | `				return rc;` |
|     - |  5179 | `			}` |
|   265 |  5180 | `			return PH7_OK;                     /* yield this element */` |
|     - |  5181 | `		}` |
|   423 |  5182 | `		if( !bExhausted && iState == RS_SELF ){` |
|    78 |  5183 | `			int iMode = RitInt(pThis,RIT_MD);` |
|    78 |  5184 | `			if( iMode == RIT_SELF_FIRST \|\| iMode == RIT_CHILD_FIRST ){` |
|    78 |  5185 | `				rc = RitHook(pCtx,"nextElement",sizeof("nextElement")-1,0,bCatch,&bThrew);` |
|    78 |  5186 | `				if( rc != SXRET_OK ){` |
|   ! 0 |  5187 | `					return rc;` |
|     - |  5188 | `				}` |
|    38 |  5189 | `			}` |
|    78 |  5190 | `			RitSetState(pVm,pThis,iLevel,iMode == RIT_SELF_FIRST ? RS_CHILD : RS_NEXT);` |
|    78 |  5191 | `			return PH7_OK;                     /* yield this element */` |
|     - |  5192 | `		}` |
|   347 |  5193 | `		if( !bExhausted && iState == RS_CHILD ){` |
|     - |  5194 | `			ph7_class *pRecCls;` |
|     - |  5195 | `			ph7_class_instance *pChild;` |
|     - |  5196 | `			ph7_value sChild;` |
|   135 |  5197 | `			int iMode = RitInt(pThis,RIT_MD);` |
|   135 |  5198 | `			PH7_MemObjInit(pVm,&sChild);` |
|   135 |  5199 | `			rc = RitHook(pCtx,"callGetChildren",sizeof("callGetChildren")-1,&sChild,bCatch,&bThrew);` |
|   135 |  5200 | `			if( rc != SXRET_OK ){` |
|     3 |  5201 | `				PH7_MemObjRelease(&sChild);` |
|     4 |  5202 | `				return rc;` |
|     - |  5203 | `			}` |
|   133 |  5204 | `			if( bThrew ){` |
|     - |  5205 | `				/* Caught: php drops the element and moves to the next one. */` |
|     3 |  5206 | `				PH7_MemObjRelease(&sChild);` |
|     3 |  5207 | `				RitSetState(pVm,pThis,iLevel,RS_NEXT);` |
|    66 |  5208 | `				continue;` |
|     - |  5209 | `			}` |
|   131 |  5210 | `			pRecCls = PH7_VmExtractClass(pVm,"RecursiveIterator",` |
|     - |  5211 | `				sizeof("RecursiveIterator")-1,FALSE,0);` |
|   195 |  5212 | `			pChild = (sChild.iFlags & MEMOBJ_OBJ) != 0` |
|   127 |  5213 | `				? (ph7_class_instance *)sChild.x.pOther : 0;` |
|   131 |  5214 | `			if( pChild == 0 \|\| (pRecCls && !PH7_VmInstanceOf(pChild->pClass,pRecCls)) ){` |
|     3 |  5215 | `				PH7_MemObjRelease(&sChild);` |
|     3 |  5216 | `				return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     - |  5217 | `					"Objects returned by RecursiveIterator::getChildren() must implement RecursiveIterator");` |
|     - |  5218 | `			}` |
|   129 |  5219 | `			pChild->iRef++;                    /* survive the release of the call result */` |
|   129 |  5220 | `			PH7_MemObjRelease(&sChild);` |
|   129 |  5221 | `			RitSetState(pVm,pThis,iLevel,iMode == RIT_CHILD_FIRST ? RS_SELF : RS_NEXT);` |
|   129 |  5222 | `			RitPush(pVm,pThis,pChild);` |
|   129 |  5223 | `			PH7_ClassInstanceUnref(pChild);    /* the level's slot holds it now */` |
|   129 |  5224 | `			rc = RitCall(pCtx,pChild,"rewind",sizeof("rewind")-1,0,FALSE,0);` |
|   129 |  5225 | `			if( rc != SXRET_OK ){` |
|   ! 0 |  5226 | `				return rc;` |
|     - |  5227 | `			}` |
|   129 |  5228 | `			rc = RitHook(pCtx,"beginChildren",sizeof("beginChildren")-1,0,bCatch,&bThrew);` |
|   129 |  5229 | `			if( rc != SXRET_OK ){` |
|   ! 0 |  5230 | `				return rc;` |
|     - |  5231 | `			}` |
|   129 |  5232 | `			continue;                          /* php's goto next_step */` |
|     - |  5233 | `		}` |
|     - |  5234 | `		/* No more elements at this level. */` |
|   215 |  5235 | `		if( iLevel <= 0 ){` |
|    89 |  5236 | `			return PH7_OK;                     /* done completely */` |
|     - |  5237 | `		}` |
|     - |  5238 | `		/* php calls endChildren BEFORE the pop, so the hook sees the depth it is` |
|     - |  5239 | `		 * leaving rather than the one it lands on. */` |
|   129 |  5240 | `		rc = RitHook(pCtx,"endChildren",sizeof("endChildren")-1,0,bCatch,&bThrew);` |
|   129 |  5241 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  5242 | `			return rc;` |
|     - |  5243 | `		}` |
|   129 |  5244 | `		if( RitInt(pThis,RIT_LVL) > 0 && RitSub(pVm,pThis,RitInt(pThis,RIT_LVL)) == pSub ){` |
|   129 |  5245 | `			RitPop(pVm,pThis);` |
|    63 |  5246 | `		}` |
|     3 |  5247 | `	}` |
|   217 |  5248 | `}` |
|     - |  5249 | `/*` |
|     - |  5250 | ` * php's spl_recursive_it_valid_ex: ASK the levels, walking down from the current` |
|     - |  5251 | ` * one, and fire endIteration the first time the answer is no.` |
|     - |  5252 | ` */` |
|   410 |  5253 | `static sxi32 RitValidEx(ph7_context *pCtx,int *pbValid)` |
|     3 |  5254 | `{` |
|   413 |  5255 | `	ph7_vm *pVm = pCtx->pVm;` |
|   413 |  5256 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   413 |  5257 | `	int iLevel = RitInt(pThis,RIT_LVL);` |
|     - |  5258 | `	sxi32 rc;` |
|   413 |  5259 | `	*pbValid = FALSE;` |
|   501 |  5260 | `	while( iLevel >= 0 ){` |
|   413 |  5261 | `		ph7_class_instance *pSub = RitSub(pVm,pThis,iLevel);` |
|     - |  5262 | `		ph7_value sValid;` |
|     - |  5263 | `		int bOk;` |
|   413 |  5264 | `		if( pSub == 0 ){` |
|   ! 0 |  5265 | `			iLevel--;` |
|   ! 0 |  5266 | `			continue;` |
|     - |  5267 | `		}` |
|   413 |  5268 | `		PH7_MemObjInit(pVm,&sValid);` |
|   413 |  5269 | `		rc = RitCall(pCtx,pSub,"valid",sizeof("valid")-1,&sValid,FALSE,0);` |
|   413 |  5270 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  5271 | `			PH7_MemObjRelease(&sValid);` |
|   ! 0 |  5272 | `			return rc;` |
|     - |  5273 | `		}` |
|   413 |  5274 | `		bOk = ph7_value_to_bool(&sValid);` |
|   413 |  5275 | `		PH7_MemObjRelease(&sValid);` |
|   413 |  5276 | `		if( bOk ){` |
|   325 |  5277 | `			*pbValid = TRUE;` |
|   325 |  5278 | `			return PH7_OK;` |
|     - |  5279 | `		}` |
|    91 |  5280 | `		iLevel--;` |
|     3 |  5281 | `	}` |
|    91 |  5282 | `	if( RitInt(pThis,RIT_II) ){` |
|    89 |  5283 | `		rc = RitHook(pCtx,"endIteration",sizeof("endIteration")-1,0,FALSE,0);` |
|    89 |  5284 | `		PH7_NativeSetAttrInt(pVm,pThis,RIT_II,0);` |
|    89 |  5285 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  5286 | `			return rc;` |
|     - |  5287 | `		}` |
|    43 |  5288 | `	}` |
|    91 |  5289 | `	PH7_NativeSetAttrInt(pVm,pThis,RIT_II,0);` |
|    91 |  5290 | `	return PH7_OK;` |
|   208 |  5291 | `}` |
|   150 |  5292 | `static int vm_builtin_RecursiveIteratorIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  5293 | `{` |
|   153 |  5294 | `	ph7_vm *pVm = pCtx->pVm;` |
|   153 |  5295 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  5296 | `	ph7_class_instance *pObj;` |
|   153 |  5297 | `	ph7_class_instance *pHold = 0;` |
|     - |  5298 | `	ph7_class *pAggCls,*pRecCls,*pTravCls;` |
|   153 |  5299 | `	sxi64 iMode = RIT_LEAVES_ONLY,iFlags = 0;` |
|     - |  5300 | `	sxi32 rc;` |
|   153 |  5301 | `	if( pThis == 0 ){` |
|   ! 0 |  5302 | `		return PH7_OK;` |
|     - |  5303 | `	}` |
|     - |  5304 | `	/*` |
|     - |  5305 | `	 * php's ZPP here is "o\|ll" -- a bare OBJECT -- while the stub declares` |
|     - |  5306 | ``	 * `Traversable $iterator`, so the declared type and the refusal text disagree`` |
|     - |  5307 | `	 * (rule 41's neighbour). The spec row carries the declared type for Reflection` |
|     - |  5308 | `	 * and this body words both refusals, which is why the method sits on` |
|     - |  5309 | `	 * azSelfChecked[].` |
|     - |  5310 | `	 */` |
|   153 |  5311 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 \|\| apArg[0]->x.pOther == 0 ){` |
|     - |  5312 | `		char zGiven[64];` |
|     5 |  5313 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - |  5314 | `			"RecursiveIteratorIterator::__construct(): Argument #1 ($iterator) "` |
|     - |  5315 | `			"must be of type object, %s given",` |
|     2 |  5316 | `			nArg < 1 ? "none" : VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));` |
|     - |  5317 | `	}` |
|   151 |  5318 | `	if( nArg > 1 ){` |
|    85 |  5319 | `		rc = PH7_IntArgResolve(pCtx,apArg[1],"RecursiveIteratorIterator::__construct",2,"$mode","int",&iMode);` |
|    85 |  5320 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  5321 | `			return rc;` |
|     - |  5322 | `		}` |
|    41 |  5323 | `	}` |
|   151 |  5324 | `	if( nArg > 2 ){` |
|    66 |  5325 | `		rc = PH7_IntArgResolve(pCtx,apArg[2],"RecursiveIteratorIterator::__construct",3,"$flags","int",&iFlags);` |
|    66 |  5326 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  5327 | `			return rc;` |
|     - |  5328 | `		}` |
|    32 |  5329 | `	}` |
|   151 |  5330 | `	pObj = (ph7_class_instance *)apArg[0]->x.pOther;` |
|   151 |  5331 | `	pAggCls = PH7_VmExtractClass(pVm,"IteratorAggregate",sizeof("IteratorAggregate")-1,FALSE,0);` |
|   151 |  5332 | `	pRecCls = PH7_VmExtractClass(pVm,"RecursiveIterator",sizeof("RecursiveIterator")-1,FALSE,0);` |
|   151 |  5333 | `	pTravCls = PH7_VmExtractClass(pVm,"Traversable",sizeof("Traversable")-1,FALSE,0);` |
|     - |  5334 | `	/*` |
|     - |  5335 | `	 * php's spl_get_iterator_from_aggregate: ONE getIterator() and no more. An` |
|     - |  5336 | `	 * IteratorAggregate whose getIterator() answers another aggregate therefore` |
|     - |  5337 | `	 * fails the RecursiveIterator test below rather than being unwrapped further.` |
|     - |  5338 | `	 */` |
|   151 |  5339 | `	if( pAggCls && PH7_VmInstanceOf(pObj->pClass,pAggCls) ){` |
|     3 |  5340 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pObj->pClass,"getIterator",` |
|     - |  5341 | `			sizeof("getIterator")-1);` |
|     - |  5342 | `		ph7_value sInner;` |
|     3 |  5343 | `		PH7_MemObjInit(pVm,&sInner);` |
|     3 |  5344 | `		rc = pMethod ? PH7_VmCallClassMethod(pVm,pObj,pMethod,&sInner,0,0) : SXRET_OK;` |
|     3 |  5345 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  5346 | `			PH7_MemObjRelease(&sInner);` |
|   ! 0 |  5347 | `			return rc;` |
|     - |  5348 | `		}` |
|     2 |  5349 | `		if( (sInner.iFlags & MEMOBJ_OBJ) == 0 \|\| sInner.x.pOther == 0` |
|     3 |  5350 | `		 \|\| (pTravCls && !PH7_VmInstanceOf(((ph7_class_instance *)sInner.x.pOther)->pClass,pTravCls)) ){` |
|   ! 0 |  5351 | `			SyString *pName = &pObj->pClass->sName;` |
|   ! 0 |  5352 | `			PH7_MemObjRelease(&sInner);` |
|   ! 0 |  5353 | `			return PH7_VmThrowException(pCtx,"LogicException",` |
|   ! 0 |  5354 | `				"%z::getIterator() must return an object that implements Traversable",pName);` |
|     - |  5355 | `		}` |
|     3 |  5356 | `		pObj = (ph7_class_instance *)sInner.x.pOther;` |
|     3 |  5357 | `		pObj->iRef++;` |
|     3 |  5358 | `		PH7_MemObjRelease(&sInner);` |
|     3 |  5359 | `		pHold = pObj;` |
|     1 |  5360 | `	}` |
|   151 |  5361 | `	if( pRecCls == 0 \|\| !PH7_VmInstanceOf(pObj->pClass,pRecCls) ){` |
|     3 |  5362 | `		if( pHold ){` |
|   ! 0 |  5363 | `			PH7_ClassInstanceUnref(pHold);` |
|   ! 0 |  5364 | `		}` |
|     - |  5365 | `		/* php refuses here rather than from the declared type, so a plain Iterator` |
|     - |  5366 | `		 * gets this sentence and not a TypeError. */` |
|     3 |  5367 | `		return PH7_VmThrowException(pCtx,"InvalidArgumentException",` |
|     - |  5368 | `			"An instance of RecursiveIterator or IteratorAggregate creating it is required");` |
|     - |  5369 | `	}` |
|   149 |  5370 | `	RitClear(pVm,pThis);` |
|   149 |  5371 | `	PH7_NativeSetAttrInt(pVm,pThis,RIT_LVL,0);` |
|   149 |  5372 | `	PH7_NativeSetAttrInt(pVm,pThis,RIT_MD,iMode);` |
|   149 |  5373 | `	PH7_NativeSetAttrInt(pVm,pThis,RIT_FL,iFlags);` |
|   149 |  5374 | `	PH7_NativeSetAttrInt(pVm,pThis,RIT_MX,-1);` |
|   149 |  5375 | `	PH7_NativeSetAttrInt(pVm,pThis,RIT_II,0);` |
|     - |  5376 | `	{` |
|     - |  5377 | `		ph7_value sObj;` |
|   149 |  5378 | `		PH7_MemObjInit(pVm,&sObj);` |
|   149 |  5379 | `		sObj.x.pOther = pObj;` |
|   149 |  5380 | `		MemObjSetType(&sObj,MEMOBJ_OBJ);` |
|   149 |  5381 | `		RitPut(pVm,pThis,RIT_ST,0,&sObj);` |
|   149 |  5382 | `		sObj.x.pOther = 0;` |
|   149 |  5383 | `		MemObjSetType(&sObj,MEMOBJ_NULL);` |
|   149 |  5384 | `		PH7_MemObjRelease(&sObj);` |
|     - |  5385 | `	}` |
|   149 |  5386 | `	RitSetState(pVm,pThis,0,RS_START);` |
|     - |  5387 | `	/* Level 0 exists from HERE, which is what makes getDepth() answer 0 and` |
|     - |  5388 | `	 * getSubIterator() answer the root before any rewind(). */` |
|   149 |  5389 | `	PH7_NativeSetAttrInt(pVm,pThis,RIT_RD,1);` |
|   149 |  5390 | `	if( pHold ){` |
|     3 |  5391 | `		PH7_ClassInstanceUnref(pHold);` |
|     1 |  5392 | `	}` |
|    73 |  5393 | `	SXUNUSED(nArg);` |
|   149 |  5394 | `	return PH7_OK;` |
|    78 |  5395 | `}` |
|   108 |  5396 | `static int vm_builtin_RecursiveIteratorIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  5397 | `{` |
|   111 |  5398 | `	ph7_vm *pVm = pCtx->pVm;` |
|   111 |  5399 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  5400 | `	ph7_class_instance *pRoot;` |
|     - |  5401 | `	sxi32 rc;` |
|    54 |  5402 | `	SXUNUSED(nArg);` |
|    54 |  5403 | `	SXUNUSED(apArg);` |
|   111 |  5404 | `	if( !RitReady(pThis) ){` |
|     3 |  5405 | `		return RitNotReady(pCtx);` |
|     - |  5406 | `	}` |
|     - |  5407 | `	/* php pops the level FIRST and calls endChildren after, so the hook reports the` |
|     - |  5408 | `	 * depth it has landed on -- the opposite order from the traversal's own pop. */` |
|   109 |  5409 | `	while( RitInt(pThis,RIT_LVL) > 0 ){` |
|   ! 0 |  5410 | `		RitPop(pVm,pThis);` |
|   ! 0 |  5411 | `		rc = RitHook(pCtx,"endChildren",sizeof("endChildren")-1,0,FALSE,0);` |
|   ! 0 |  5412 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  5413 | `			return rc;` |
|     - |  5414 | `		}` |
|   ! 0 |  5415 | `	}` |
|   109 |  5416 | `	RitSetState(pVm,pThis,0,RS_START);` |
|   109 |  5417 | `	pRoot = RitSub(pVm,pThis,0);` |
|   109 |  5418 | `	rc = RitCall(pCtx,pRoot,"rewind",sizeof("rewind")-1,0,FALSE,0);` |
|   109 |  5419 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  5420 | `		return rc;` |
|     - |  5421 | `	}` |
|     - |  5422 | `	/* php's in_iteration latch: a second rewind() does NOT re-announce the` |
|     - |  5423 | `	 * iteration, which is the only reason the flag exists. */` |
|   109 |  5424 | `	if( !RitInt(pThis,RIT_II) ){` |
|   107 |  5425 | `		rc = RitHook(pCtx,"beginIteration",sizeof("beginIteration")-1,0,FALSE,0);` |
|   107 |  5426 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  5427 | `			PH7_NativeSetAttrInt(pVm,pThis,RIT_II,1);` |
|   ! 0 |  5428 | `			return rc;` |
|     - |  5429 | `		}` |
|    52 |  5430 | `	}` |
|   109 |  5431 | `	PH7_NativeSetAttrInt(pVm,pThis,RIT_II,1);` |
|   109 |  5432 | `	return RitMoveForward(pCtx);` |
|    57 |  5433 | `}` |
|   412 |  5434 | `static int vm_builtin_RecursiveIteratorIterator_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  5435 | `{` |
|   415 |  5436 | `	int bValid = FALSE;` |
|     - |  5437 | `	sxi32 rc;` |
|   206 |  5438 | `	SXUNUSED(nArg);` |
|   206 |  5439 | `	SXUNUSED(apArg);` |
|   415 |  5440 | `	if( !RitReady(PH7_ContextThis(pCtx)) ){` |
|     3 |  5441 | `		return RitNotReady(pCtx);` |
|     - |  5442 | `	}` |
|   413 |  5443 | `	rc = RitValidEx(pCtx,&bValid);` |
|   413 |  5444 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  5445 | `		return rc;` |
|     - |  5446 | `	}` |
|   413 |  5447 | `	ph7_result_bool(pCtx,bValid);` |
|   413 |  5448 | `	return PH7_OK;` |
|   209 |  5449 | `}` |
|     - |  5450 | `/* current() and key() read the CURRENT LEVEL live -- php keeps no cache here, the` |
|     - |  5451 | ` * one place the recursive iterator differs from every dual iterator (rule 43). */` |
|   474 |  5452 | `static sxi32 RitCurrentLevelCall(ph7_context *pCtx,const char *zName,sxu32 nName)` |
|     3 |  5453 | `{` |
|   477 |  5454 | `	ph7_vm *pVm = pCtx->pVm;` |
|   477 |  5455 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  5456 | `	ph7_class_instance *pSub;` |
|     - |  5457 | `	ph7_value sRes;` |
|     - |  5458 | `	sxi32 rc;` |
|   477 |  5459 | `	if( !RitReady(pThis) ){` |
|   ! 0 |  5460 | `		return RitNotReady(pCtx);` |
|     - |  5461 | `	}` |
|   477 |  5462 | `	pSub = RitSub(pVm,pThis,RitInt(pThis,RIT_LVL));` |
|   477 |  5463 | `	if( pSub == 0 ){` |
|   ! 0 |  5464 | `		ph7_result_null(pCtx);` |
|   ! 0 |  5465 | `		return PH7_OK;` |
|     - |  5466 | `	}` |
|   477 |  5467 | `	PH7_MemObjInit(pVm,&sRes);` |
|   477 |  5468 | `	rc = RitCall(pCtx,pSub,zName,nName,&sRes,FALSE,0);` |
|   477 |  5469 | `	if( rc == SXRET_OK ){` |
|   477 |  5470 | `		ph7_result_value(pCtx,&sRes);` |
|   237 |  5471 | `	}` |
|   477 |  5472 | `	PH7_MemObjRelease(&sRes);` |
|   477 |  5473 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|   240 |  5474 | `}` |
|   186 |  5475 | `static int vm_builtin_RecursiveIteratorIterator_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  5476 | `{` |
|    93 |  5477 | `	SXUNUSED(nArg);` |
|    93 |  5478 | `	SXUNUSED(apArg);` |
|   189 |  5479 | `	return RitCurrentLevelCall(pCtx,"key",sizeof("key")-1);` |
|     3 |  5480 | `}` |
|   226 |  5481 | `static int vm_builtin_RecursiveIteratorIterator_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  5482 | `{` |
|   113 |  5483 | `	SXUNUSED(nArg);` |
|   113 |  5484 | `	SXUNUSED(apArg);` |
|   229 |  5485 | `	return RitCurrentLevelCall(pCtx,"current",sizeof("current")-1);` |
|     3 |  5486 | `}` |
|   322 |  5487 | `static int vm_builtin_RecursiveIteratorIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  5488 | `{` |
|   161 |  5489 | `	SXUNUSED(nArg);` |
|   161 |  5490 | `	SXUNUSED(apArg);` |
|   325 |  5491 | `	if( !RitReady(PH7_ContextThis(pCtx)) ){` |
|   ! 0 |  5492 | `		return RitNotReady(pCtx);` |
|     - |  5493 | `	}` |
|   325 |  5494 | `	return RitMoveForward(pCtx);` |
|   164 |  5495 | `}` |
|   124 |  5496 | `static int vm_builtin_RecursiveIteratorIterator_getDepth(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  5497 | `{` |
|   126 |  5498 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    62 |  5499 | `	SXUNUSED(nArg);` |
|    62 |  5500 | `	SXUNUSED(apArg);` |
|   126 |  5501 | `	if( !RitReady(pThis) ){` |
|     3 |  5502 | `		return RitNotReady(pCtx);` |
|     - |  5503 | `	}` |
|   124 |  5504 | `	ph7_result_int64(pCtx,(ph7_int64)RitInt(pThis,RIT_LVL));` |
|   124 |  5505 | `	return PH7_OK;` |
|    64 |  5506 | `}` |
|    20 |  5507 | `static int vm_builtin_RecursiveIteratorIterator_getSubIterator(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  5508 | `{` |
|    21 |  5509 | `	ph7_vm *pVm = pCtx->pVm;` |
|    21 |  5510 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  5511 | `	ph7_class_instance *pSub;` |
|     - |  5512 | `	int iLevel;` |
|    21 |  5513 | `	if( !RitReady(pThis) ){` |
|   ! 0 |  5514 | `		return RitNotReady(pCtx);` |
|     - |  5515 | `	}` |
|    21 |  5516 | `	iLevel = RitInt(pThis,RIT_LVL);` |
|    21 |  5517 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_NULL) == 0 ){` |
|     9 |  5518 | `		sxi64 iWant = 0;` |
|     9 |  5519 | `		sxi32 rc = PH7_IntArgResolve(pCtx,apArg[0],"RecursiveIteratorIterator::getSubIterator",` |
|     - |  5520 | `			1,"$level","?int",&iWant);` |
|     9 |  5521 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  5522 | `			return rc;` |
|     - |  5523 | `		}` |
|     9 |  5524 | `		if( iWant < 0 \|\| iWant > (sxi64)iLevel ){` |
|     7 |  5525 | `			ph7_result_null(pCtx);` |
|     7 |  5526 | `			return PH7_OK;` |
|     - |  5527 | `		}` |
|     3 |  5528 | `		iLevel = (int)iWant;` |
|     1 |  5529 | `	}` |
|    15 |  5530 | `	pSub = RitSub(pVm,pThis,iLevel);` |
|    15 |  5531 | `	if( pSub ){` |
|    15 |  5532 | `		SplResultBorrowed(pCtx,pSub);` |
|     8 |  5533 | `	}else{` |
|   ! 0 |  5534 | `		ph7_result_null(pCtx);` |
|     - |  5535 | `	}` |
|    15 |  5536 | `	return PH7_OK;` |
|    11 |  5537 | `}` |
|     8 |  5538 | `static int vm_builtin_RecursiveIteratorIterator_getInnerIterator(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  5539 | `{` |
|     9 |  5540 | `	ph7_vm *pVm = pCtx->pVm;` |
|     9 |  5541 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  5542 | `	ph7_class_instance *pSub;` |
|     4 |  5543 | `	SXUNUSED(nArg);` |
|     4 |  5544 | `	SXUNUSED(apArg);` |
|     9 |  5545 | `	if( !RitReady(pThis) ){` |
|     3 |  5546 | `		return RitNotReady(pCtx);` |
|     - |  5547 | `	}` |
|     7 |  5548 | `	pSub = RitSub(pVm,pThis,RitInt(pThis,RIT_LVL));` |
|     7 |  5549 | `	if( pSub ){` |
|     7 |  5550 | `		SplResultBorrowed(pCtx,pSub);` |
|     4 |  5551 | `	}else{` |
|   ! 0 |  5552 | `		ph7_result_null(pCtx);` |
|     - |  5553 | `	}` |
|     7 |  5554 | `	return PH7_OK;` |
|     5 |  5555 | `}` |
|     - |  5556 | `/* The five hooks php declares with empty bodies. They exist to be OVERRIDDEN and` |
|     - |  5557 | ` * to be reachable through parent:: from an override. */` |
|   754 |  5558 | `static int vm_builtin_RecursiveIteratorIterator_nop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  5559 | `{` |
|   377 |  5560 | `	SXUNUSED(nArg);` |
|   377 |  5561 | `	SXUNUSED(apArg);` |
|   757 |  5562 | `	if( !RitReady(PH7_ContextThis(pCtx)) ){` |
|   ! 0 |  5563 | `		return RitNotReady(pCtx);` |
|     - |  5564 | `	}` |
|   757 |  5565 | `	return PH7_OK;` |
|   380 |  5566 | `}` |
|     - |  5567 | `/* php's callHasChildren/callGetChildren ask the CURRENT LEVEL's iterator, which` |
|     - |  5568 | ` * is what makes them the documented interception point for both. */` |
|   404 |  5569 | `static int vm_builtin_RecursiveIteratorIterator_callHasChildren(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  5570 | `{` |
|   407 |  5571 | `	ph7_vm *pVm = pCtx->pVm;` |
|   407 |  5572 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  5573 | `	ph7_class_instance *pSub;` |
|     - |  5574 | `	ph7_value sRes;` |
|     - |  5575 | `	sxi32 rc;` |
|   202 |  5576 | `	SXUNUSED(nArg);` |
|   202 |  5577 | `	SXUNUSED(apArg);` |
|   407 |  5578 | `	if( !RitReady(pThis) ){` |
|     3 |  5579 | `		return RitNotReady(pCtx);` |
|     - |  5580 | `	}` |
|   405 |  5581 | `	pSub = RitSub(pVm,pThis,RitInt(pThis,RIT_LVL));` |
|   405 |  5582 | `	if( pSub == 0 ){` |
|   ! 0 |  5583 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  5584 | `		return PH7_OK;` |
|     - |  5585 | `	}` |
|   405 |  5586 | `	PH7_MemObjInit(pVm,&sRes);` |
|   405 |  5587 | `	rc = RitCall(pCtx,pSub,"hasChildren",sizeof("hasChildren")-1,&sRes,FALSE,0);` |
|   405 |  5588 | `	if( rc == SXRET_OK ){` |
|   405 |  5589 | `		ph7_result_bool(pCtx,ph7_value_to_bool(&sRes));` |
|   201 |  5590 | `	}` |
|   405 |  5591 | `	PH7_MemObjRelease(&sRes);` |
|   405 |  5592 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|   205 |  5593 | `}` |
|   134 |  5594 | `static int vm_builtin_RecursiveIteratorIterator_callGetChildren(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  5595 | `{` |
|   137 |  5596 | `	ph7_vm *pVm = pCtx->pVm;` |
|   137 |  5597 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  5598 | `	ph7_class_instance *pSub;` |
|     - |  5599 | `	ph7_value sRes;` |
|     - |  5600 | `	sxi32 rc;` |
|    67 |  5601 | `	SXUNUSED(nArg);` |
|    67 |  5602 | `	SXUNUSED(apArg);` |
|   137 |  5603 | `	if( !RitReady(pThis) ){` |
|   ! 0 |  5604 | `		return RitNotReady(pCtx);` |
|     - |  5605 | `	}` |
|   137 |  5606 | `	pSub = RitSub(pVm,pThis,RitInt(pThis,RIT_LVL));` |
|   137 |  5607 | `	if( pSub == 0 ){` |
|   ! 0 |  5608 | `		ph7_result_null(pCtx);` |
|   ! 0 |  5609 | `		return PH7_OK;` |
|     - |  5610 | `	}` |
|   137 |  5611 | `	PH7_MemObjInit(pVm,&sRes);` |
|   137 |  5612 | `	rc = RitCall(pCtx,pSub,"getChildren",sizeof("getChildren")-1,&sRes,FALSE,0);` |
|   137 |  5613 | `	if( rc == SXRET_OK ){` |
|   131 |  5614 | `		ph7_result_value(pCtx,&sRes);` |
|    64 |  5615 | `	}` |
|   137 |  5616 | `	PH7_MemObjRelease(&sRes);` |
|   137 |  5617 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|    70 |  5618 | `}` |
|    18 |  5619 | `static int vm_builtin_RecursiveIteratorIterator_setMaxDepth(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  5620 | `{` |
|    19 |  5621 | `	ph7_vm *pVm = pCtx->pVm;` |
|    19 |  5622 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    19 |  5623 | `	sxi64 iMax = -1;` |
|    19 |  5624 | `	if( !RitReady(pThis) ){` |
|   ! 0 |  5625 | `		return RitNotReady(pCtx);` |
|     - |  5626 | `	}` |
|    19 |  5627 | `	if( nArg > 0 ){` |
|    17 |  5628 | `		sxi32 rc = PH7_IntArgResolve(pCtx,apArg[0],"RecursiveIteratorIterator::setMaxDepth",` |
|     - |  5629 | `			1,"$maxDepth","int",&iMax);` |
|    17 |  5630 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  5631 | `			return rc;` |
|     - |  5632 | `		}` |
|     8 |  5633 | `	}` |
|    19 |  5634 | `	if( iMax < -1 ){` |
|     3 |  5635 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  5636 | `			"RecursiveIteratorIterator::setMaxDepth(): Argument #1 ($maxDepth) "` |
|     - |  5637 | `			"must be greater than or equal to -1");` |
|     - |  5638 | `	}` |
|    17 |  5639 | `	if( iMax > SXI32_HIGH ){` |
|   ! 0 |  5640 | `		iMax = SXI32_HIGH;   /* php clamps to INT_MAX; max_depth is an int there */` |
|   ! 0 |  5641 | `	}` |
|    17 |  5642 | `	PH7_NativeSetAttrInt(pVm,pThis,RIT_MX,iMax);` |
|    17 |  5643 | `	return PH7_OK;` |
|    10 |  5644 | `}` |
|    10 |  5645 | `static int vm_builtin_RecursiveIteratorIterator_getMaxDepth(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  5646 | `{` |
|    11 |  5647 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  5648 | `	int iMax;` |
|     5 |  5649 | `	SXUNUSED(nArg);` |
|     5 |  5650 | `	SXUNUSED(apArg);` |
|    11 |  5651 | `	if( !RitReady(pThis) ){` |
|   ! 0 |  5652 | `		return RitNotReady(pCtx);` |
|     - |  5653 | `	}` |
|    11 |  5654 | `	iMax = RitInt(pThis,RIT_MX);` |
|    11 |  5655 | `	if( iMax == -1 ){` |
|     5 |  5656 | ``		ph7_result_bool(pCtx,0);   /* php's `int\|false`: false means "any depth" */`` |
|     3 |  5657 | `	}else{` |
|     7 |  5658 | `		ph7_result_int64(pCtx,(ph7_int64)iMax);` |
|     - |  5659 | `	}` |
|    11 |  5660 | `	return PH7_OK;` |
|     6 |  5661 | `}` |
|     - |  5662 | `/*` |
|     - |  5663 | ` * ---------------------------------------------------------------------------` |
|     - |  5664 | ` * RecursiveTreeIterator: the RecursiveIteratorIterator that draws the tree.` |
|     - |  5665 | ` *` |
|     - |  5666 | ` * It is the same traversal with a STRING built around each element, and the` |
|     - |  5667 | ` * drawing is why php wraps the iterator it is handed in a` |
|     - |  5668 | ` * RecursiveCachingIterator: the ASCII branches need to know whether a level has` |
|     - |  5669 | ` * a NEXT element, and hasNext() is the one question only the caching decorator` |
|     - |  5670 | ` * answers. So the sub-iterator at every level here is a` |
|     - |  5671 | ` * RecursiveCachingIterator, getSubIterator()/getInnerIterator() report one, and` |
|     - |  5672 | `` * `$cachingIteratorFlags` is what that wrapper is built with — CATCH_GET_CHILD`` |
|     - |  5673 | ` * when the caller says nothing, and EXACTLY what the caller says otherwise.` |
|     - |  5674 | ` *` |
|     - |  5675 | ` * The prefix is six parts: a fixed LEFT, one MID per level above this one` |
|     - |  5676 | ` * (chosen by whether that level has a next element), one END for this level` |
|     - |  5677 | ` * (chosen the same way) and a fixed RIGHT. current() is prefix + entry + postfix` |
|     - |  5678 | ` * and key() is prefix + key + postfix, each bypassable through its own flag —` |
|     - |  5679 | ` * and those flags live in the SAME word as RecursiveIteratorIterator's` |
|     - |  5680 | ` * CATCH_GET_CHILD, which is why php declares that constant on both classes.` |
|     - |  5681 | ` */` |
|     - |  5682 | `#define RTI_PFX "__pfx"   /* php's prefix[6] */` |
|     - |  5683 | `#define RTI_PST "__pst"   /* php's postfix */` |
|     - |  5684 |  |
|     - |  5685 | `#define RTIT_BYPASS_CURRENT     4` |
|     - |  5686 | `#define RTIT_BYPASS_KEY         8` |
|     - |  5687 | `#define RTIT_PREFIX_LEFT        0` |
|     - |  5688 | `#define RTIT_PREFIX_MID_HAS_NEXT 1` |
|     - |  5689 | `#define RTIT_PREFIX_MID_LAST    2` |
|     - |  5690 | `#define RTIT_PREFIX_END_HAS_NEXT 3` |
|     - |  5691 | `#define RTIT_PREFIX_END_LAST    4` |
|     - |  5692 | `#define RTIT_PREFIX_RIGHT       5` |
|     - |  5693 |  |
|     - |  5694 | ``/* php's `object->prefix[N]` defaults, set in the constructor. */`` |
|     - |  5695 | `static const char * const azRtiPrefix[] = { "", "\| ", "  ", "\|-", "\\-", "" };` |
|     - |  5696 |  |
|   466 |  5697 | `static ph7_value * RtiPrefixSlot(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  5698 | `{` |
|   467 |  5699 | `	ph7_value *pSlot = pThis ? PH7_NativeAttr(pThis,RTI_PFX) : 0;` |
|   467 |  5700 | `	if( pSlot == 0 ){` |
|   ! 0 |  5701 | `		return 0;` |
|     - |  5702 | `	}` |
|   467 |  5703 | `	if( (pSlot->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    47 |  5704 | `		if( PH7_MemObjToHashmap(pSlot) != SXRET_OK ){` |
|   ! 0 |  5705 | `			return 0;` |
|     - |  5706 | `		}` |
|    23 |  5707 | `	}` |
|   467 |  5708 | `	if( PH7_HashmapCowSeparate(pVm,pSlot) == 0 ){` |
|   ! 0 |  5709 | `		return 0;` |
|     - |  5710 | `	}` |
|     - |  5711 | `	/* Re-ask: the separation can MOVE the memobj pool this slot lives in` |
|     - |  5712 | `	 * (SplStoreSlot explains it). */` |
|   467 |  5713 | `	return PH7_NativeAttr(pThis,RTI_PFX);` |
|   234 |  5714 | `}` |
|     - |  5715 | `/* One prefix part, appended to pOut. A part nothing has written is php's own` |
|     - |  5716 | ` * default rather than the empty string. */` |
|   404 |  5717 | `static void RtiAppendPart(ph7_vm *pVm,ph7_class_instance *pThis,int iPart,ph7_value *pOut)` |
|     1 |  5718 | `{` |
|   405 |  5719 | `	ph7_value *pSlot = RtiPrefixSlot(pVm,pThis);` |
|   405 |  5720 | `	ph7_hashmap_node *pNode = 0;` |
|   405 |  5721 | `	ph7_value *pVal = 0;` |
|     - |  5722 | `	const char *zTxt;` |
|     - |  5723 | `	int nTxt;` |
|   405 |  5724 | `	if( pSlot && HashmapLookupIntKey((ph7_hashmap *)pSlot->x.pOther,(sxi64)iPart,&pNode) == SXRET_OK ){` |
|   405 |  5725 | `		pVal = HashmapExtractNodeValue(pNode);` |
|   202 |  5726 | `	}` |
|   405 |  5727 | `	if( pVal == 0 ){` |
|   ! 0 |  5728 | `		return;` |
|     - |  5729 | `	}` |
|   405 |  5730 | `	zTxt = ph7_value_to_string(pVal,&nTxt);` |
|   405 |  5731 | `	if( nTxt > 0 ){` |
|   205 |  5732 | `		PH7_MemObjStringAppend(pOut,zTxt,(sxu32)nTxt);` |
|   102 |  5733 | `	}` |
|   203 |  5734 | `}` |
|     - |  5735 | ``/* php's `hasnext` on one level's sub-iterator; a level with no answer draws`` |
|     - |  5736 | ` * nothing at all. */` |
|   180 |  5737 | `static sxi32 RtiLevelHasNext(ph7_context *pCtx,int iLevel,int *pbHas,int *pbAnswered)` |
|     1 |  5738 | `{` |
|   181 |  5739 | `	ph7_vm *pVm = pCtx->pVm;` |
|   181 |  5740 | `	ph7_class_instance *pSub = RitSub(pVm,PH7_ContextThis(pCtx),iLevel);` |
|   181 |  5741 | `	ph7_class_method *pMethod = pSub` |
|   180 |  5742 | `		? PH7_ClassExtractMethod(pSub->pClass,"hasNext",sizeof("hasNext")-1) : 0;` |
|     - |  5743 | `	ph7_value sRes;` |
|     - |  5744 | `	sxi32 rc;` |
|   181 |  5745 | `	*pbHas = 0;` |
|   181 |  5746 | `	*pbAnswered = 0;` |
|   181 |  5747 | `	if( pMethod == 0 ){` |
|   ! 0 |  5748 | `		return SXRET_OK;` |
|     - |  5749 | `	}` |
|   181 |  5750 | `	PH7_MemObjInit(pVm,&sRes);` |
|   181 |  5751 | `	rc = PH7_VmCallClassMethod(pVm,pSub,pMethod,&sRes,0,0);` |
|   181 |  5752 | `	if( rc == SXRET_OK ){` |
|   181 |  5753 | `		PH7_MemObjToBool(&sRes);          /* a STATUS, not the answer */` |
|   181 |  5754 | `		*pbHas = sRes.x.iVal != 0;` |
|   181 |  5755 | `		*pbAnswered = 1;` |
|    90 |  5756 | `	}` |
|   181 |  5757 | `	PH7_MemObjRelease(&sRes);` |
|   181 |  5758 | `	return rc;` |
|    91 |  5759 | `}` |
|   112 |  5760 | `static sxi32 RtiBuildPrefix(ph7_context *pCtx,ph7_value *pOut)` |
|     1 |  5761 | `{` |
|   113 |  5762 | `	ph7_vm *pVm = pCtx->pVm;` |
|   113 |  5763 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   113 |  5764 | `	int iLevel = RitInt(pThis,RIT_LVL);` |
|     - |  5765 | `	int i,bHas,bAnswered;` |
|     - |  5766 | `	sxi32 rc;` |
|   113 |  5767 | `	RtiAppendPart(pVm,pThis,RTIT_PREFIX_LEFT,pOut);` |
|   181 |  5768 | `	for( i = 0 ; i < iLevel ; ++i ){` |
|    69 |  5769 | `		rc = RtiLevelHasNext(pCtx,i,&bHas,&bAnswered);` |
|    69 |  5770 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  5771 | `			return rc;` |
|     - |  5772 | `		}` |
|    69 |  5773 | `		if( bAnswered ){` |
|    69 |  5774 | `			RtiAppendPart(pVm,pThis,bHas ? RTIT_PREFIX_MID_HAS_NEXT : RTIT_PREFIX_MID_LAST,pOut);` |
|    34 |  5775 | `		}` |
|    35 |  5776 | `	}` |
|   113 |  5777 | `	rc = RtiLevelHasNext(pCtx,iLevel,&bHas,&bAnswered);` |
|   113 |  5778 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  5779 | `		return rc;` |
|     - |  5780 | `	}` |
|   113 |  5781 | `	if( bAnswered ){` |
|   113 |  5782 | `		RtiAppendPart(pVm,pThis,bHas ? RTIT_PREFIX_END_HAS_NEXT : RTIT_PREFIX_END_LAST,pOut);` |
|    56 |  5783 | `	}` |
|   113 |  5784 | `	RtiAppendPart(pVm,pThis,RTIT_PREFIX_RIGHT,pOut);` |
|   113 |  5785 | `	return SXRET_OK;` |
|    57 |  5786 | `}` |
|     - |  5787 | `/*` |
|     - |  5788 | ` * php's get_entry: the CACHED current() of this level's caching iterator, as a` |
|     - |  5789 | ` * string. An ARRAY is the word "Array" and says nothing while doing it — php` |
|     - |  5790 | ` * never runs a cast here — and an object with no __toString still raises.` |
|     - |  5791 | ` */` |
|    96 |  5792 | `static sxi32 RtiBuildEntry(ph7_context *pCtx,ph7_value *pOut)` |
|     1 |  5793 | `{` |
|    97 |  5794 | `	ph7_vm *pVm = pCtx->pVm;` |
|    97 |  5795 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    97 |  5796 | `	ph7_class_instance *pSub = RitSub(pVm,pThis,RitInt(pThis,RIT_LVL));` |
|    97 |  5797 | `	ph7_class_method *pMethod = pSub` |
|    96 |  5798 | `		? PH7_ClassExtractMethod(pSub->pClass,"current",sizeof("current")-1) : 0;` |
|     - |  5799 | `	ph7_value sRes;` |
|     - |  5800 | `	sxi32 rc;` |
|    97 |  5801 | `	if( pMethod == 0 ){` |
|   ! 0 |  5802 | `		return SXRET_OK;` |
|     - |  5803 | `	}` |
|    97 |  5804 | `	PH7_MemObjInit(pVm,&sRes);` |
|    97 |  5805 | `	rc = PH7_VmCallClassMethod(pVm,pSub,pMethod,&sRes,0,0);` |
|    97 |  5806 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  5807 | `		PH7_MemObjRelease(&sRes);` |
|   ! 0 |  5808 | `		return rc;` |
|     - |  5809 | `	}` |
|    97 |  5810 | `	if( sRes.iFlags & MEMOBJ_HASHMAP ){` |
|    35 |  5811 | `		PH7_MemObjStringAppend(pOut,"Array",sizeof("Array")-1);` |
|    80 |  5812 | `	}else if( (sRes.iFlags & MEMOBJ_NULL) == 0 ){` |
|    61 |  5813 | `		rc = PH7_MemObjToStringUV(&sRes);` |
|    61 |  5814 | `		if( rc == SXRET_OK ){` |
|     - |  5815 | `			int nTxt;` |
|    59 |  5816 | `			const char *zTxt = ph7_value_to_string(&sRes,&nTxt);` |
|    59 |  5817 | `			if( nTxt > 0 ){` |
|    59 |  5818 | `				PH7_MemObjStringAppend(pOut,zTxt,(sxu32)nTxt);` |
|    29 |  5819 | `			}` |
|    29 |  5820 | `		}` |
|    30 |  5821 | `	}` |
|    97 |  5822 | `	PH7_MemObjRelease(&sRes);` |
|    97 |  5823 | `	return rc;` |
|    49 |  5824 | `}` |
|    98 |  5825 | `static void RtiAppendPostfix(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut)` |
|     1 |  5826 | `{` |
|    99 |  5827 | `	ph7_value *pSlot = PH7_NativeAttr(pThis,RTI_PST);` |
|     - |  5828 | `	const char *zTxt;` |
|     - |  5829 | `	int nTxt;` |
|    99 |  5830 | `	if( pSlot == 0 ){` |
|   ! 0 |  5831 | `		return;` |
|     - |  5832 | `	}` |
|    99 |  5833 | `	zTxt = ph7_value_to_string(pSlot,&nTxt);` |
|    99 |  5834 | `	if( nTxt > 0 ){` |
|    13 |  5835 | `		PH7_MemObjStringAppend(pOut,zTxt,(sxu32)nTxt);` |
|     6 |  5836 | `	}` |
|    49 |  5837 | `	SXUNUSED(pVm);` |
|    50 |  5838 | `}` |
|    50 |  5839 | `static int vm_builtin_RecursiveTreeIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  5840 | `{` |
|    51 |  5841 | `	ph7_vm *pVm = pCtx->pVm;` |
|    51 |  5842 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  5843 | `	ph7_class *pCls;` |
|     - |  5844 | `	ph7_class_method *pCons;` |
|     - |  5845 | `	ph7_class_instance *pWrap;` |
|     - |  5846 | `	ph7_value sFlags,sMode,sCache,sSrc,sWrap,*apCtor[3],*pSlot;` |
|    51 |  5847 | `	sxi64 iFlags = RTIT_BYPASS_KEY, iCache = RIT_CATCH_GET_CHILD, iMode = RIT_SELF_FIRST;` |
|     - |  5848 | `	int i;` |
|     - |  5849 | `	sxi32 rc;` |
|    51 |  5850 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|   ! 0 |  5851 | `		return PH7_OK;` |
|     - |  5852 | `	}` |
|     - |  5853 | `	/* php's ZPP is "o\|lzl" — a bare OBJECT — while the stub declares the union` |
|     - |  5854 | `` 	 * this row carries for Reflection, so the type screen stands aside (the `~` `` |
|     - |  5855 | `	 * marker) and the refusal is worded here. */` |
|    51 |  5856 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 \|\| apArg[0]->x.pOther == 0 ){` |
|     - |  5857 | `		char zGiven[64];` |
|     7 |  5858 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - |  5859 | `			"RecursiveTreeIterator::__construct(): Argument #1 ($iterator) "` |
|     - |  5860 | `			"must be of type object, %s given",` |
|     2 |  5861 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));` |
|     - |  5862 | `	}` |
|    47 |  5863 | `	if( nArg > 1 ){` |
|    13 |  5864 | `		iFlags = ph7_value_to_int64(apArg[1]);` |
|     6 |  5865 | `	}` |
|    47 |  5866 | `	if( nArg > 2 ){` |
|     - |  5867 | `		/* A caller's value REPLACES the CATCH_GET_CHILD default rather than joining` |
|     - |  5868 | `` 		 * it — `new RecursiveTreeIterator($it, 8, CachingIterator::TOSTRING_USE_KEY)` `` |
|     - |  5869 | `		 * builds a wrapper that does NOT catch. */` |
|     5 |  5870 | `		iCache = ph7_value_to_int64(apArg[2]);` |
|     2 |  5871 | `	}` |
|    47 |  5872 | `	if( nArg > 3 ){` |
|     3 |  5873 | `		iMode = ph7_value_to_int64(apArg[3]);` |
|     1 |  5874 | `	}` |
|     - |  5875 | `	/* The six prefix parts and the postfix php seeds every instance with. */` |
|    47 |  5876 | `	pSlot = RtiPrefixSlot(pVm,pThis);` |
|   323 |  5877 | `	for( i = 0 ; pSlot && i < (int)SX_ARRAYSIZE(azRtiPrefix) ; ++i ){` |
|     - |  5878 | `		ph7_value sPart;` |
|   277 |  5879 | `		PH7_MemObjInitFromString(pVm,&sPart,0);` |
|   277 |  5880 | `		PH7_MemObjStringAppend(&sPart,azRtiPrefix[i],(sxu32)SyStrlen(azRtiPrefix[i]));` |
|   277 |  5881 | `		ph7_array_add_intkey_elem(pSlot,i,&sPart);` |
|   277 |  5882 | `		PH7_MemObjRelease(&sPart);` |
|   139 |  5883 | `	}` |
|     - |  5884 | `	{` |
|     - |  5885 | `		ph7_value sPost;` |
|    47 |  5886 | `		PH7_MemObjInitFromString(pVm,&sPost,0);` |
|    47 |  5887 | `		DualSetSlot(pVm,pThis,RTI_PST,&sPost);` |
|    47 |  5888 | `		PH7_MemObjRelease(&sPost);` |
|     - |  5889 | `	}` |
|     - |  5890 | `	/* php wraps the iterator FIRST, so a source the wrapper refuses is reported by` |
|     - |  5891 | `	 * RecursiveCachingIterator::__construct and never reaches the traversal. */` |
|    47 |  5892 | `	pCls = PH7_VmExtractClass(pVm,"RecursiveCachingIterator",` |
|     - |  5893 | `		sizeof("RecursiveCachingIterator")-1,FALSE,0);` |
|    47 |  5894 | `	pCons = pCls ? PH7_ClassExtractMethod(pCls,"__construct",sizeof("__construct")-1) : 0;` |
|    47 |  5895 | `	if( pCons == 0 ){` |
|   ! 0 |  5896 | `		return PH7_OK;` |
|     - |  5897 | `	}` |
|    47 |  5898 | `	pWrap = PH7_NewClassInstance(pVm,pCls);` |
|    47 |  5899 | `	if( pWrap == 0 ){` |
|   ! 0 |  5900 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  5901 | `	}` |
|    47 |  5902 | `	pWrap->iRef++;` |
|     - |  5903 | `	/* php unwraps an IteratorAggregate BEFORE it wraps: the caching iterator has` |
|     - |  5904 | ``	 * to be handed the RecursiveIterator itself, so `getIterator()` runs here and`` |
|     - |  5905 | `	 * exactly once. */` |
|    47 |  5906 | `	PH7_MemObjInit(pVm,&sSrc);` |
|    47 |  5907 | `	PH7_MemObjStore(apArg[0],&sSrc);` |
|     - |  5908 | `	{` |
|    47 |  5909 | `		ph7_class *pAggCls = PH7_VmExtractClass(pVm,"IteratorAggregate",` |
|     - |  5910 | `			sizeof("IteratorAggregate")-1,FALSE,0);` |
|    47 |  5911 | `		ph7_class_instance *pSrc = (ph7_class_instance *)sSrc.x.pOther;` |
|    47 |  5912 | `		if( pAggCls && PH7_VmInstanceOf(pSrc->pClass,pAggCls) ){` |
|     3 |  5913 | `			ph7_class_method *pGet = PH7_ClassExtractMethod(pSrc->pClass,"getIterator",` |
|     - |  5914 | `				sizeof("getIterator")-1);` |
|     - |  5915 | `			ph7_value sInner;` |
|     3 |  5916 | `			PH7_MemObjInit(pVm,&sInner);` |
|     3 |  5917 | `			rc = pGet ? PH7_VmCallClassMethod(pVm,pSrc,pGet,&sInner,0,0) : SXRET_OK;` |
|     3 |  5918 | `			if( rc != SXRET_OK ){` |
|   ! 0 |  5919 | `				PH7_MemObjRelease(&sInner);` |
|   ! 0 |  5920 | `				PH7_MemObjRelease(&sSrc);` |
|   ! 0 |  5921 | `				PH7_ClassInstanceUnref(pWrap);` |
|   ! 0 |  5922 | `				return rc;` |
|     - |  5923 | `			}` |
|     3 |  5924 | `			PH7_MemObjStore(&sInner,&sSrc);` |
|     3 |  5925 | `			PH7_MemObjRelease(&sInner);` |
|     1 |  5926 | `		}` |
|     - |  5927 | `	}` |
|    47 |  5928 | `	PH7_MemObjInitFromInt(pVm,&sCache,iCache);` |
|    47 |  5929 | `	apCtor[0] = &sSrc;` |
|    47 |  5930 | `	apCtor[1] = &sCache;` |
|    47 |  5931 | `	rc = PH7_VmCallClassMethod(pVm,pWrap,pCons,0,2,apCtor);` |
|    47 |  5932 | `	PH7_MemObjRelease(&sCache);` |
|    47 |  5933 | `	PH7_MemObjRelease(&sSrc);` |
|    47 |  5934 | `	if( rc != SXRET_OK ){` |
|     5 |  5935 | `		PH7_ClassInstanceUnref(pWrap);` |
|     5 |  5936 | `		return rc;` |
|     - |  5937 | `	}` |
|    43 |  5938 | `	PH7_MemObjInit(pVm,&sWrap);` |
|    43 |  5939 | `	sWrap.x.pOther = pWrap;` |
|    43 |  5940 | `	MemObjSetType(&sWrap,MEMOBJ_OBJ);` |
|    43 |  5941 | `	pWrap->iRef++;                    /* the temporary owns one of its own */` |
|    43 |  5942 | `	PH7_MemObjInitFromInt(pVm,&sMode,iMode);` |
|    43 |  5943 | `	PH7_MemObjInitFromInt(pVm,&sFlags,iFlags);` |
|    43 |  5944 | `	apCtor[0] = &sWrap;` |
|    43 |  5945 | `	apCtor[1] = &sMode;` |
|    43 |  5946 | `	apCtor[2] = &sFlags;` |
|    43 |  5947 | `	rc = vm_builtin_RecursiveIteratorIterator_construct(pCtx,3,apCtor);` |
|    43 |  5948 | `	PH7_MemObjRelease(&sWrap);` |
|    43 |  5949 | `	PH7_MemObjRelease(&sMode);` |
|    43 |  5950 | `	PH7_MemObjRelease(&sFlags);` |
|    43 |  5951 | `	PH7_ClassInstanceUnref(pWrap);` |
|    43 |  5952 | `	return rc;` |
|    26 |  5953 | `}` |
|    16 |  5954 | `static int vm_builtin_RecursiveTreeIterator_getPrefix(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  5955 | `{` |
|     - |  5956 | `	ph7_value sOut;` |
|     - |  5957 | `	sxi32 rc;` |
|     8 |  5958 | `	SXUNUSED(nArg);` |
|     8 |  5959 | `	SXUNUSED(apArg);` |
|    17 |  5960 | `	if( !RitReady(PH7_ContextThis(pCtx)) ){` |
|     3 |  5961 | `		return RitNotReady(pCtx);` |
|     - |  5962 | `	}` |
|    15 |  5963 | `	PH7_MemObjInitFromString(pCtx->pVm,&sOut,0);` |
|    15 |  5964 | `	rc = RtiBuildPrefix(pCtx,&sOut);` |
|    15 |  5965 | `	if( rc == SXRET_OK ){` |
|    15 |  5966 | `		ph7_result_value(pCtx,&sOut);` |
|     7 |  5967 | `	}` |
|    15 |  5968 | `	PH7_MemObjRelease(&sOut);` |
|    15 |  5969 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|     9 |  5970 | `}` |
|    24 |  5971 | `static int vm_builtin_RecursiveTreeIterator_getEntry(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  5972 | `{` |
|     - |  5973 | `	ph7_value sOut;` |
|     - |  5974 | `	sxi32 rc;` |
|    12 |  5975 | `	SXUNUSED(nArg);` |
|    12 |  5976 | `	SXUNUSED(apArg);` |
|    25 |  5977 | `	if( !RitReady(PH7_ContextThis(pCtx)) ){` |
|     3 |  5978 | `		return RitNotReady(pCtx);` |
|     - |  5979 | `	}` |
|    23 |  5980 | `	PH7_MemObjInitFromString(pCtx->pVm,&sOut,0);` |
|    23 |  5981 | `	rc = RtiBuildEntry(pCtx,&sOut);` |
|    23 |  5982 | `	if( rc == SXRET_OK ){` |
|    21 |  5983 | `		ph7_result_value(pCtx,&sOut);` |
|    10 |  5984 | `	}` |
|    23 |  5985 | `	PH7_MemObjRelease(&sOut);` |
|    23 |  5986 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|    13 |  5987 | `}` |
|    16 |  5988 | `static int vm_builtin_RecursiveTreeIterator_getPostfix(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  5989 | `{` |
|    17 |  5990 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  5991 | `	ph7_value *pSlot;` |
|     8 |  5992 | `	SXUNUSED(nArg);` |
|     8 |  5993 | `	SXUNUSED(apArg);` |
|    17 |  5994 | `	if( !RitReady(pThis) ){` |
|     3 |  5995 | `		return RitNotReady(pCtx);` |
|     - |  5996 | `	}` |
|    15 |  5997 | `	pSlot = PH7_NativeAttr(pThis,RTI_PST);` |
|    15 |  5998 | `	if( pSlot ){` |
|    15 |  5999 | `		ph7_result_value(pCtx,pSlot);` |
|     7 |  6000 | `	}` |
|    15 |  6001 | `	return PH7_OK;` |
|     9 |  6002 | `}` |
|     2 |  6003 | `static int vm_builtin_RecursiveTreeIterator_setPostfix(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6004 | `{` |
|     3 |  6005 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     3 |  6006 | `	if( nArg < 1 \|\| pThis == 0 ){` |
|   ! 0 |  6007 | `		return PH7_OK;` |
|     - |  6008 | `	}` |
|     3 |  6009 | `	DualSetSlot(pCtx->pVm,pThis,RTI_PST,apArg[0]);` |
|     3 |  6010 | `	return PH7_OK;` |
|     2 |  6011 | `}` |
|    20 |  6012 | `static int vm_builtin_RecursiveTreeIterator_setPrefixPart(ph7_context *pCtx,int nArg,` |
|     - |  6013 | `	ph7_value **apArg)` |
|     1 |  6014 | `{` |
|    21 |  6015 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  6016 | `	ph7_value *pSlot;` |
|     - |  6017 | `	sxi64 iPart;` |
|    21 |  6018 | `	if( nArg < 2 \|\| pThis == 0 ){` |
|   ! 0 |  6019 | `		return PH7_OK;` |
|     - |  6020 | `	}` |
|    21 |  6021 | `	iPart = ph7_value_to_int64(apArg[0]);` |
|    21 |  6022 | `	if( iPart < RTIT_PREFIX_LEFT \|\| iPart > RTIT_PREFIX_RIGHT ){` |
|     5 |  6023 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  6024 | `			"RecursiveTreeIterator::setPrefixPart(): Argument #1 ($part) must be a "` |
|     - |  6025 | `			"RecursiveTreeIterator::PREFIX_* constant");` |
|     - |  6026 | `	}` |
|    17 |  6027 | `	pSlot = RtiPrefixSlot(pCtx->pVm,pThis);` |
|    17 |  6028 | `	if( pSlot ){` |
|    17 |  6029 | `		ph7_array_add_intkey_elem(pSlot,(int)iPart,apArg[1]);` |
|     8 |  6030 | `	}` |
|    17 |  6031 | `	return PH7_OK;` |
|    11 |  6032 | `}` |
|     - |  6033 | `/* php's current()/key(): the traversal's own answer, wrapped unless its BYPASS` |
|     - |  6034 | ` * flag is set. The wrapped form is always a STRING, prefix and postfix included. */` |
|   164 |  6035 | `static int RtiWrapped(ph7_context *pCtx,int bKey)` |
|     1 |  6036 | `{` |
|   165 |  6037 | `	ph7_vm *pVm = pCtx->pVm;` |
|   165 |  6038 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  6039 | `	ph7_value sOut;` |
|     - |  6040 | `	sxi32 rc;` |
|   165 |  6041 | `	if( !RitReady(pThis) ){` |
|     5 |  6042 | `		return RitNotReady(pCtx);` |
|     - |  6043 | `	}` |
|   161 |  6044 | `	if( RitInt(pThis,RIT_FL) & (bKey ? RTIT_BYPASS_KEY : RTIT_BYPASS_CURRENT) ){` |
|    51 |  6045 | `		return bKey ? RitCurrentLevelCall(pCtx,"key",sizeof("key")-1)` |
|    62 |  6046 | `		            : RitCurrentLevelCall(pCtx,"current",sizeof("current")-1);` |
|     - |  6047 | `	}` |
|    99 |  6048 | `	PH7_MemObjInitFromString(pVm,&sOut,0);` |
|    99 |  6049 | `	rc = RtiBuildPrefix(pCtx,&sOut);` |
|    99 |  6050 | `	if( rc == SXRET_OK ){` |
|    99 |  6051 | `		if( bKey ){` |
|    25 |  6052 | `			ph7_class_instance *pSub = RitSub(pVm,pThis,RitInt(pThis,RIT_LVL));` |
|    25 |  6053 | `			ph7_class_method *pMethod = pSub` |
|    24 |  6054 | `				? PH7_ClassExtractMethod(pSub->pClass,"key",sizeof("key")-1) : 0;` |
|    25 |  6055 | `			if( pMethod ){` |
|     - |  6056 | `				ph7_value sKey;` |
|    25 |  6057 | `				PH7_MemObjInit(pVm,&sKey);` |
|    25 |  6058 | `				rc = PH7_VmCallClassMethod(pVm,pSub,pMethod,&sKey,0,0);` |
|    25 |  6059 | `				if( rc == SXRET_OK && (sKey.iFlags & MEMOBJ_NULL) == 0 ){` |
|     - |  6060 | `					int nTxt;` |
|     - |  6061 | `					const char *zTxt;` |
|    25 |  6062 | `					rc = PH7_MemObjToStringUV(&sKey);` |
|    25 |  6063 | `					zTxt = ph7_value_to_string(&sKey,&nTxt);` |
|    25 |  6064 | `					if( rc == SXRET_OK && nTxt > 0 ){` |
|    25 |  6065 | `						PH7_MemObjStringAppend(&sOut,zTxt,(sxu32)nTxt);` |
|    12 |  6066 | `					}` |
|    12 |  6067 | `				}` |
|    25 |  6068 | `				PH7_MemObjRelease(&sKey);` |
|    12 |  6069 | `			}` |
|    13 |  6070 | `		}else{` |
|    75 |  6071 | `			rc = RtiBuildEntry(pCtx,&sOut);` |
|     - |  6072 | `		}` |
|    49 |  6073 | `	}` |
|    99 |  6074 | `	if( rc == SXRET_OK ){` |
|    99 |  6075 | `		RtiAppendPostfix(pVm,pThis,&sOut);` |
|    99 |  6076 | `		ph7_result_value(pCtx,&sOut);` |
|    49 |  6077 | `	}` |
|    99 |  6078 | `	PH7_MemObjRelease(&sOut);` |
|    99 |  6079 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|    83 |  6080 | `}` |
|   100 |  6081 | `static int vm_builtin_RecursiveTreeIterator_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6082 | `{` |
|    50 |  6083 | `	SXUNUSED(nArg);` |
|    50 |  6084 | `	SXUNUSED(apArg);` |
|   101 |  6085 | `	return RtiWrapped(pCtx,FALSE);` |
|     1 |  6086 | `}` |
|    64 |  6087 | `static int vm_builtin_RecursiveTreeIterator_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6088 | `{` |
|    32 |  6089 | `	SXUNUSED(nArg);` |
|    32 |  6090 | `	SXUNUSED(apArg);` |
|    65 |  6091 | `	return RtiWrapped(pCtx,TRUE);` |
|     1 |  6092 | `}` |
|     - |  6093 | `/*` |
|     - |  6094 | ` * The declaration. Method ORDER follows spl_iterators.stub.php line for line,` |
|     - |  6095 | ` * because that is the order Reflection reports. Every return type is php's` |
|     - |  6096 | `` * `@tentative-return-type` kind (rule 45).`` |
|     - |  6097 | ` */` |
|  8445 |  6098 | `static sxi32 VmInstallSplRecursiveIt(ph7_vm *pVm)` |
|     5 |  6099 | `{` |
|     - |  6100 | `	static const PH7_NativePropDef aRitProp[] = {` |
|     - |  6101 | `		{ RIT_ST,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  6102 | `		{ RIT_SS,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  6103 | `		{ RIT_LVL, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  6104 | `		{ RIT_MD,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  6105 | `		{ RIT_FL,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  6106 | `		{ RIT_MX,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, -1, 0, 0.0 }, 0 },` |
|     - |  6107 | `		{ RIT_II,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  6108 | `		{ RIT_RD,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  6109 | `	};` |
|     - |  6110 | `	static const PH7_NativeConstDef aRitConst[] = {` |
|     - |  6111 | `		{ "LEAVES_ONLY",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, RIT_LEAVES_ONLY, 0, 0.0 },` |
|     - |  6112 | `		{ "SELF_FIRST",      PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, RIT_SELF_FIRST, 0, 0.0 },` |
|     - |  6113 | `		{ "CHILD_FIRST",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, RIT_CHILD_FIRST, 0, 0.0 },` |
|     - |  6114 | `		{ "CATCH_GET_CHILD", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, RIT_CATCH_GET_CHILD, 0, 0.0 },` |
|     - |  6115 | `	};` |
|     - |  6116 | `	static const PH7_NativeMethodDef aRitMethod[] = {` |
|     - |  6117 | `		{ "__construct",      PH7_MOD_PUBLIC,` |
|     - |  6118 | ``		  /* php's stub spells the default `RecursiveIteratorIterator::LEAVES_ONLY`;`` |
|     - |  6119 | `		   * one zSig field cannot carry both the TEXT and the VALUE, and the value` |
|     - |  6120 | `		   * wins here for the same reason it does on RegexIterator's row. */` |
|     - |  6121 | `		  "Traversable $iterator, int $mode = RecursiveIteratorIterator::LEAVES_ONLY, "` |
|     - |  6122 | `		  "int $flags = 0", 0,` |
|     - |  6123 | `		  vm_builtin_RecursiveIteratorIterator_construct },` |
|     - |  6124 | `		{ "rewind",           PH7_MOD_PUBLIC, "", "@void",` |
|     - |  6125 | `		  vm_builtin_RecursiveIteratorIterator_rewind },` |
|     - |  6126 | `		{ "valid",            PH7_MOD_PUBLIC, "", "@bool",` |
|     - |  6127 | `		  vm_builtin_RecursiveIteratorIterator_valid },` |
|     - |  6128 | `		{ "key",              PH7_MOD_PUBLIC, "", "@mixed",` |
|     - |  6129 | `		  vm_builtin_RecursiveIteratorIterator_key },` |
|     - |  6130 | `		{ "current",          PH7_MOD_PUBLIC, "", "@mixed",` |
|     - |  6131 | `		  vm_builtin_RecursiveIteratorIterator_current },` |
|     - |  6132 | `		{ "next",             PH7_MOD_PUBLIC, "", "@void",` |
|     - |  6133 | `		  vm_builtin_RecursiveIteratorIterator_next },` |
|     - |  6134 | `		{ "getDepth",         PH7_MOD_PUBLIC, "", "@int",` |
|     - |  6135 | `		  vm_builtin_RecursiveIteratorIterator_getDepth },` |
|     - |  6136 | `		{ "getSubIterator",   PH7_MOD_PUBLIC, "?int $level = null", "@?RecursiveIterator",` |
|     - |  6137 | `		  vm_builtin_RecursiveIteratorIterator_getSubIterator },` |
|     - |  6138 | `		{ "getInnerIterator", PH7_MOD_PUBLIC, "", "@RecursiveIterator",` |
|     - |  6139 | `		  vm_builtin_RecursiveIteratorIterator_getInnerIterator },` |
|     - |  6140 | `		{ "beginIteration",   PH7_MOD_PUBLIC, "", "@void",` |
|     - |  6141 | `		  vm_builtin_RecursiveIteratorIterator_nop },` |
|     - |  6142 | `		{ "endIteration",     PH7_MOD_PUBLIC, "", "@void",` |
|     - |  6143 | `		  vm_builtin_RecursiveIteratorIterator_nop },` |
|     - |  6144 | `		{ "callHasChildren",  PH7_MOD_PUBLIC, "", "@bool",` |
|     - |  6145 | `		  vm_builtin_RecursiveIteratorIterator_callHasChildren },` |
|     - |  6146 | `		{ "callGetChildren",  PH7_MOD_PUBLIC, "", "@?RecursiveIterator",` |
|     - |  6147 | `		  vm_builtin_RecursiveIteratorIterator_callGetChildren },` |
|     - |  6148 | `		{ "beginChildren",    PH7_MOD_PUBLIC, "", "@void",` |
|     - |  6149 | `		  vm_builtin_RecursiveIteratorIterator_nop },` |
|     - |  6150 | `		{ "endChildren",      PH7_MOD_PUBLIC, "", "@void",` |
|     - |  6151 | `		  vm_builtin_RecursiveIteratorIterator_nop },` |
|     - |  6152 | `		{ "nextElement",      PH7_MOD_PUBLIC, "", "@void",` |
|     - |  6153 | `		  vm_builtin_RecursiveIteratorIterator_nop },` |
|     - |  6154 | `		{ "setMaxDepth",      PH7_MOD_PUBLIC, "int $maxDepth = -1", "@void",` |
|     - |  6155 | `		  vm_builtin_RecursiveIteratorIterator_setMaxDepth },` |
|     - |  6156 | `		{ "getMaxDepth",      PH7_MOD_PUBLIC, "", "@int\|false",` |
|     - |  6157 | `		  vm_builtin_RecursiveIteratorIterator_getMaxDepth },` |
|     - |  6158 | `	};` |
|     - |  6159 | ``	/* PH7_CLASS_NOCLONE: php refuses `clone` outright ("Trying to clone an`` |
|     - |  6160 | `	 * uncloneable object"), and a slot-by-slot copy would share one level stack --` |
|     - |  6161 | `	 * and with it one cursor -- between two traversals. */` |
|     - |  6162 | `	static const PH7_NativeConstDef aRtiConst[] = {` |
|     - |  6163 | `		{ "BYPASS_CURRENT",        PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, RTIT_BYPASS_CURRENT, 0, 0.0 },` |
|     - |  6164 | `		{ "BYPASS_KEY",            PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, RTIT_BYPASS_KEY, 0, 0.0 },` |
|     - |  6165 | `		{ "PREFIX_LEFT",           PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, RTIT_PREFIX_LEFT, 0, 0.0 },` |
|     - |  6166 | `		{ "PREFIX_MID_HAS_NEXT",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT,` |
|     - |  6167 | `		  RTIT_PREFIX_MID_HAS_NEXT, 0, 0.0 },` |
|     - |  6168 | `		{ "PREFIX_MID_LAST",       PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, RTIT_PREFIX_MID_LAST, 0, 0.0 },` |
|     - |  6169 | `		{ "PREFIX_END_HAS_NEXT",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT,` |
|     - |  6170 | `		  RTIT_PREFIX_END_HAS_NEXT, 0, 0.0 },` |
|     - |  6171 | `		{ "PREFIX_END_LAST",       PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, RTIT_PREFIX_END_LAST, 0, 0.0 },` |
|     - |  6172 | `		{ "PREFIX_RIGHT",          PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, RTIT_PREFIX_RIGHT, 0, 0.0 },` |
|     - |  6173 | `	};` |
|     - |  6174 | `	static const PH7_NativePropDef aRtiProp[] = {` |
|     - |  6175 | `		{ RTI_PFX, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  6176 | `		{ RTI_PST, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  6177 | `	};` |
|     - |  6178 | `	static const PH7_NativeMethodDef aRtiMethod[] = {` |
|     - |  6179 | `		{ "__construct",   PH7_MOD_PUBLIC,` |
|     - |  6180 | `		  "~RecursiveIterator\|IteratorAggregate $iterator, "` |
|     - |  6181 | `		  "int $flags = RecursiveTreeIterator::BYPASS_KEY, "` |
|     - |  6182 | `		  "int $cachingIteratorFlags = CachingIterator::CATCH_GET_CHILD, "` |
|     - |  6183 | `		  "int $mode = RecursiveTreeIterator::SELF_FIRST", 0,` |
|     - |  6184 | `		  vm_builtin_RecursiveTreeIterator_construct },` |
|     - |  6185 | `		{ "key",           PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_RecursiveTreeIterator_key },` |
|     - |  6186 | `		{ "current",       PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_RecursiveTreeIterator_current },` |
|     - |  6187 | `		{ "getPrefix",     PH7_MOD_PUBLIC, "", "@string",` |
|     - |  6188 | `		  vm_builtin_RecursiveTreeIterator_getPrefix },` |
|     - |  6189 | `		{ "setPostfix",    PH7_MOD_PUBLIC, "string $postfix", "@void",` |
|     - |  6190 | `		  vm_builtin_RecursiveTreeIterator_setPostfix },` |
|     - |  6191 | `		{ "setPrefixPart", PH7_MOD_PUBLIC, "int $part, string $value", "@void",` |
|     - |  6192 | `		  vm_builtin_RecursiveTreeIterator_setPrefixPart },` |
|     - |  6193 | `		{ "getEntry",      PH7_MOD_PUBLIC, "", "@string",` |
|     - |  6194 | `		  vm_builtin_RecursiveTreeIterator_getEntry },` |
|     - |  6195 | `		{ "getPostfix",    PH7_MOD_PUBLIC, "", "@string",` |
|     - |  6196 | `		  vm_builtin_RecursiveTreeIterator_getPostfix },` |
|     - |  6197 | `	};` |
|     - |  6198 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - |  6199 | `		{ "RecursiveIteratorIterator", 0, "OuterIterator", PH7_CLASS_NOCLONE,` |
|     - |  6200 | `		  aRitMethod, SX_ARRAYSIZE(aRitMethod),` |
|     - |  6201 | `		  aRitConst, SX_ARRAYSIZE(aRitConst),` |
|     - |  6202 | `		  aRitProp, SX_ARRAYSIZE(aRitProp), 0, 0, 0 },` |
|     - |  6203 | `		/* Its four inherited mode constants come with the parent; the eight below` |
|     - |  6204 | `		 * are its own, and BYPASS_* share the flags word CATCH_GET_CHILD lives in. */` |
|     - |  6205 | `		{ "RecursiveTreeIterator", "RecursiveIteratorIterator", 0, PH7_CLASS_NOCLONE,` |
|     - |  6206 | `		  aRtiMethod, SX_ARRAYSIZE(aRtiMethod),` |
|     - |  6207 | `		  aRtiConst, SX_ARRAYSIZE(aRtiConst),` |
|     - |  6208 | `		  aRtiProp, SX_ARRAYSIZE(aRtiProp), 0, 0, 0 },` |
|     - |  6209 | `	};` |
|  8450 |  6210 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|     5 |  6211 | `}` |
|     - |  6212 | `/*` |
|     - |  6213 | ` * ---------------------------------------------------------------------------` |
|     - |  6214 | ` * SplDoublyLinkedList, SplStack and SplQueue.` |
|     - |  6215 | ` *` |
|     - |  6216 | `` * php's `spl_dllist_object` is a linked list plus a FLAGS word, and the flags are`` |
|     - |  6217 | ` * where the family's shape lives: SPL_DLLIST_IT_LIFO (2) and IT_DELETE (1) are the` |
|     - |  6218 | ` * iteration mode, and IT_FIX (4) is a bit no CONSTANT names and no user can set --` |
|     - |  6219 | ` * the object handler stamps it at creation for SplStack and SplQueue, and it is` |
|     - |  6220 | ``  * both what freezes their LIFO/FIFO choice and why `(new SplStack)->getIteratorMode()` `` |
|     - |  6221 | ` * answers 6 rather than 2. The chunk had no notion of it, so both classes reported` |
|     - |  6222 | `` * the wrong mode and `setIteratorMode()` reported the wrong result.`` |
|     - |  6223 | ` *` |
|     - |  6224 | ` * Two rules follow from php's own code and neither is guessable from the methods:` |
|     - |  6225 | ` *` |
|     - |  6226 | ` *   - **every ArrayAccess offset is measured from the END of a LIFO list.**` |
|     - |  6227 | `` *     php resolves them through `spl_ptr_llist_offset(llist, offset, flags & LIFO)`,`` |
|     - |  6228 | `` *     so `$stack[0]` is the element `top()` answers, not the one `bottom()` does.`` |
|     - |  6229 | ` *     The chunk indexed the backing array directly and had the whole SplStack` |
|     - |  6230 | ` *     subscript surface reversed.` |
|     - |  6231 | ` *   - **the traverse POSITION is the list index in both modes.** A LIFO rewind` |
|     - |  6232 | `` *     seeds `count-1` and counts down, a FIFO rewind seeds 0 and counts up, so`` |
|     - |  6233 | `` *     `key()` and the element's own place in the list agree either way -- except`` |
|     - |  6234 | ` *     under IT_DELETE in FIFO order, where php consumes the head and deliberately` |
|     - |  6235 | ` *     does NOT advance the position (every element reports key 0).` |
|     - |  6236 | ` *` |
|     - |  6237 | `` * `toArray()` was a PHL INVENTION -- php has no such method on any of the three --`` |
|     - |  6238 | `` * and it is gone. What php has instead, and the chunk had none of: `__debugInfo()`,`` |
|     - |  6239 | `` * the `Serializable` interface with its `serialize()`/`unserialize()` pair, and the`` |
|     - |  6240 | `` * `__serialize()`/`__unserialize()` pair that php actually uses (which is why the`` |
|     - |  6241 | `` * serialized form is `O:19:"SplDoublyLinkedList":3:{i:0;…}` and not a property dump).`` |
|     - |  6242 | ` *` |
|     - |  6243 | ` * The store is a php array in a hidden slot, head->tail, so push/pop/shift/unshift` |
|     - |  6244 | ` * are the engine's OWN array builtins called with the slot (rule 7, and rule 39's` |
|     - |  6245 | ` * reference rule already lives inside them). php's element-POINTER cursor is not` |
|     - |  6246 | ` * modelled: a manual walk that mutates the list under itself resolves by position` |
|     - |  6247 | ` * here and by identity there. That is one probe line (recorded) and the only one.` |
|     - |  6248 | ` */` |
|     - |  6249 | `#define DLL_Q  "__q"   /* php's llist, head -> tail */` |
|     - |  6250 | `#define DLL_FL "__fl"  /* php's flags word, IT_FIX included */` |
|     - |  6251 | `#define DLL_I  "__i"   /* php's traverse_position */` |
|     - |  6252 |  |
|     - |  6253 | `#define DLL_IT_DELETE 1` |
|     - |  6254 | `#define DLL_IT_LIFO   2` |
|     - |  6255 | `#define DLL_IT_FIX    4   /* php's SPL_DLLIST_IT_FIX: stamped at creation, never by a user */` |
|     - |  6256 | `#define DLL_IT_MASK   3` |
|     - |  6257 |  |
|   614 |  6258 | `static ph7_value * DllSlot(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  6259 | `{` |
|   615 |  6260 | `	ph7_value *pSlot = pThis ? PH7_NativeAttr(pThis,DLL_Q) : 0;` |
|   615 |  6261 | `	if( pSlot == 0 ){` |
|   ! 0 |  6262 | `		return 0;` |
|     - |  6263 | `	}` |
|   615 |  6264 | `	if( (pSlot->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    97 |  6265 | `		if( PH7_MemObjToHashmap(pSlot) != SXRET_OK ){` |
|   ! 0 |  6266 | `			return 0;` |
|     - |  6267 | `		}` |
|    48 |  6268 | `	}` |
|   615 |  6269 | `	if( PH7_HashmapCowSeparate(pVm,pSlot) == 0 ){` |
|   ! 0 |  6270 | `		return 0;` |
|     - |  6271 | `	}` |
|     - |  6272 | `	/* Re-ask: the separation can MOVE the memobj pool this slot lives in` |
|     - |  6273 | `	 * (SplStoreSlot explains it). */` |
|   615 |  6274 | `	return PH7_NativeAttr(pThis,DLL_Q);` |
|   308 |  6275 | `}` |
|   328 |  6276 | `static ph7_hashmap * DllMap(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  6277 | `{` |
|   329 |  6278 | `	ph7_value *pSlot = DllSlot(pVm,pThis);` |
|   329 |  6279 | `	return pSlot ? (ph7_hashmap *)pSlot->x.pOther : 0;` |
|     1 |  6280 | `}` |
|   238 |  6281 | `static sxi64 DllCount(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  6282 | `{` |
|   239 |  6283 | `	ph7_hashmap *pMap = DllMap(pVm,pThis);` |
|   239 |  6284 | `	return pMap ? (sxi64)pMap->nEntry : 0;` |
|     1 |  6285 | `}` |
|   186 |  6286 | `static int DllFlags(ph7_class_instance *pThis)` |
|     1 |  6287 | `{` |
|   187 |  6288 | `	return pThis ? (int)PH7_NativeAttrInt(pThis,DLL_FL) : 0;` |
|     1 |  6289 | `}` |
|     - |  6290 | `/* The value at a LIST index (head = 0), or NULL. */` |
|    86 |  6291 | `static ph7_value * DllAt(ph7_vm *pVm,ph7_class_instance *pThis,sxi64 iIndex)` |
|     1 |  6292 | `{` |
|    87 |  6293 | `	ph7_hashmap *pMap = DllMap(pVm,pThis);` |
|    87 |  6294 | `	ph7_hashmap_node *pNode = 0;` |
|    87 |  6295 | `	if( pMap == 0 \|\| HashmapLookupIntKey(pMap,iIndex,&pNode) != SXRET_OK ){` |
|   ! 0 |  6296 | `		return 0;` |
|     - |  6297 | `	}` |
|    87 |  6298 | `	return HashmapExtractNodeValue(pNode);` |
|    44 |  6299 | `}` |
|     - |  6300 | `/*` |
|     - |  6301 | ` * php's spl_ptr_llist_offset: an ArrayAccess offset counts from the TAIL when the` |
|     - |  6302 | ` * list iterates LIFO. Every offsetGet/offsetSet/offsetUnset/add goes through here.` |
|     - |  6303 | ` */` |
|    24 |  6304 | `static sxi64 DllOffsetToIndex(ph7_class_instance *pThis,sxi64 iOffset,sxi64 nCount)` |
|     1 |  6305 | `{` |
|    25 |  6306 | `	if( DllFlags(pThis) & DLL_IT_LIFO ){` |
|    11 |  6307 | `		return nCount - 1 - iOffset;` |
|     - |  6308 | `	}` |
|    15 |  6309 | `	return iOffset;` |
|    13 |  6310 | `}` |
|     - |  6311 | `/* Hand one of the engine's own array builtins this instance's storage slot. */` |
|   242 |  6312 | `static int DllArrayCall(ph7_context *pCtx,ProchHostFunction xFunc,ph7_value **apExtra,int nExtra)` |
|     1 |  6313 | `{` |
|   243 |  6314 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  6315 | `	ph7_value *apCall[4];` |
|   243 |  6316 | `	ph7_value *pSlot = DllSlot(pCtx->pVm,pThis);` |
|     - |  6317 | `	int i;` |
|   243 |  6318 | `	if( pSlot == 0 ){` |
|   ! 0 |  6319 | `		return PH7_OK;` |
|     - |  6320 | `	}` |
|   243 |  6321 | `	apCall[0] = pSlot;` |
|   465 |  6322 | `	for( i = 0 ; i < nExtra && i < 3 ; ++i ){` |
|   223 |  6323 | `		apCall[i+1] = apExtra[i];` |
|   112 |  6324 | `	}` |
|   243 |  6325 | `	return xFunc(pCtx,nExtra+1,apCall);` |
|   122 |  6326 | `}` |
|     - |  6327 | `/* php's four "empty datastructure" refusals, which differ only in the verb. */` |
|    18 |  6328 | `static sxi32 DllEmpty(ph7_context *pCtx,const char *zVerb)` |
|     1 |  6329 | `{` |
|    28 |  6330 | `	return PH7_VmThrowException(pCtx,"RuntimeException",` |
|     9 |  6331 | `		"Can't %s an empty datastructure",zVerb);` |
|     1 |  6332 | `}` |
|     - |  6333 | `/*` |
|     - |  6334 | ` * php words every out-of-range offset from the DECLARING class, not the runtime` |
|     - |  6335 | `` * one: `SplStack::add()` on an out-of-range index still says`` |
|     - |  6336 | `` * `SplDoublyLinkedList::add()`. The chunk used get_class($this) and reported the`` |
|     - |  6337 | ` * subclass.` |
|     - |  6338 | ` */` |
|    14 |  6339 | `static sxi32 DllOutOfRange(ph7_context *pCtx,const char *zMethod)` |
|     1 |  6340 | `{` |
|    22 |  6341 | `	return PH7_VmThrowException(pCtx,"OutOfRangeException",` |
|     7 |  6342 | `		"SplDoublyLinkedList::%s(): Argument #1 ($index) is out of range",zMethod);` |
|     1 |  6343 | `}` |
|     - |  6344 | `/*` |
|     - |  6345 | ``  * php's ZPP for the four ArrayAccess offsets and add(): the stub leaves `$index` `` |
|     - |  6346 | ` * UNTYPED (which is what Reflection prints) while the ZPP is Z_PARAM_LONG, whose` |
|     - |  6347 | `` * TypeError says `must be of type int`. An untyped signature is not screened`` |
|     - |  6348 | ` * centrally, so the rule is applied here — rule 41's disagreement, resolved without` |
|     - |  6349 | ` * an azSelfChecked[] row because the declared type is absent rather than different.` |
|     - |  6350 | ` */` |
|    46 |  6351 | `static sxi32 DllIndexArg(ph7_context *pCtx,const char *zMethod,ph7_value *pArg,sxi64 *piOut)` |
|     1 |  6352 | `{` |
|     - |  6353 | `	char zFunc[64];` |
|    47 |  6354 | `	SyBufferFormat(zFunc,sizeof(zFunc),"SplDoublyLinkedList::%s",zMethod);` |
|    47 |  6355 | `	return PH7_IntArgResolve(pCtx,pArg,zFunc,1,"$index","int",piOut);` |
|     1 |  6356 | `}` |
|   206 |  6357 | `static int vm_builtin_SplDll_push(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6358 | `{` |
|     - |  6359 | `	ph7_value *apExtra[1];` |
|   207 |  6360 | `	if( nArg < 1 ){` |
|   ! 0 |  6361 | `		return PH7_OK;` |
|     - |  6362 | `	}` |
|   207 |  6363 | `	apExtra[0] = apArg[0];` |
|   207 |  6364 | `	DllArrayCall(pCtx,ph7_hashmap_push,apExtra,1);` |
|   207 |  6365 | `	ph7_result_null(pCtx);   /* array_push answers the new count; php's push is void */` |
|   207 |  6366 | `	return PH7_OK;` |
|   104 |  6367 | `}` |
|     2 |  6368 | `static int vm_builtin_SplDll_unshift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6369 | `{` |
|     - |  6370 | `	ph7_value *apExtra[1];` |
|     3 |  6371 | `	if( nArg < 1 ){` |
|   ! 0 |  6372 | `		return PH7_OK;` |
|     - |  6373 | `	}` |
|     3 |  6374 | `	apExtra[0] = apArg[0];` |
|     3 |  6375 | `	DllArrayCall(pCtx,ph7_hashmap_unshift,apExtra,1);` |
|     3 |  6376 | `	ph7_result_null(pCtx);` |
|     3 |  6377 | `	return PH7_OK;` |
|     2 |  6378 | `}` |
|     6 |  6379 | `static int vm_builtin_SplDll_pop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6380 | `{` |
|     3 |  6381 | `	SXUNUSED(nArg);` |
|     3 |  6382 | `	SXUNUSED(apArg);` |
|     7 |  6383 | `	if( DllCount(pCtx->pVm,PH7_ContextThis(pCtx)) == 0 ){` |
|     5 |  6384 | `		return DllEmpty(pCtx,"pop from");` |
|     - |  6385 | `	}` |
|     3 |  6386 | `	return DllArrayCall(pCtx,ph7_hashmap_pop,0,0);` |
|     4 |  6387 | `}` |
|    10 |  6388 | `static int vm_builtin_SplDll_shift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6389 | `{` |
|     5 |  6390 | `	SXUNUSED(nArg);` |
|     5 |  6391 | `	SXUNUSED(apArg);` |
|    11 |  6392 | `	if( DllCount(pCtx->pVm,PH7_ContextThis(pCtx)) == 0 ){` |
|     7 |  6393 | `		return DllEmpty(pCtx,"shift from");` |
|     - |  6394 | `	}` |
|     5 |  6395 | `	return DllArrayCall(pCtx,ph7_hashmap_shift,0,0);` |
|     6 |  6396 | `}` |
|     - |  6397 | `/* top() is the TAIL and bottom() the HEAD, whatever the iteration mode: php reads` |
|     - |  6398 | ` * llist->tail/llist->head directly and never consults the flags here. */` |
|    10 |  6399 | `static int vm_builtin_SplDll_top(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6400 | `{` |
|    11 |  6401 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    11 |  6402 | `	sxi64 nCount = DllCount(pCtx->pVm,pThis);` |
|     - |  6403 | `	ph7_value *pVal;` |
|     5 |  6404 | `	SXUNUSED(nArg);` |
|     5 |  6405 | `	SXUNUSED(apArg);` |
|    11 |  6406 | `	if( nCount == 0 ){` |
|     5 |  6407 | `		return DllEmpty(pCtx,"peek at");` |
|     - |  6408 | `	}` |
|     7 |  6409 | `	pVal = DllAt(pCtx->pVm,pThis,nCount-1);` |
|     7 |  6410 | `	if( pVal ){` |
|     7 |  6411 | `		ph7_result_value(pCtx,pVal);` |
|     3 |  6412 | `	}` |
|     7 |  6413 | `	return PH7_OK;` |
|     6 |  6414 | `}` |
|    10 |  6415 | `static int vm_builtin_SplDll_bottom(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6416 | `{` |
|    11 |  6417 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  6418 | `	ph7_value *pVal;` |
|     5 |  6419 | `	SXUNUSED(nArg);` |
|     5 |  6420 | `	SXUNUSED(apArg);` |
|    11 |  6421 | `	if( DllCount(pCtx->pVm,pThis) == 0 ){` |
|     5 |  6422 | `		return DllEmpty(pCtx,"peek at");` |
|     - |  6423 | `	}` |
|     7 |  6424 | `	pVal = DllAt(pCtx->pVm,pThis,0);` |
|     7 |  6425 | `	if( pVal ){` |
|     7 |  6426 | `		ph7_result_value(pCtx,pVal);` |
|     3 |  6427 | `	}` |
|     7 |  6428 | `	return PH7_OK;` |
|     6 |  6429 | `}` |
|    16 |  6430 | `static int vm_builtin_SplDll_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6431 | `{` |
|     8 |  6432 | `	SXUNUSED(nArg);` |
|     8 |  6433 | `	SXUNUSED(apArg);` |
|    17 |  6434 | `	ph7_result_int64(pCtx,DllCount(pCtx->pVm,PH7_ContextThis(pCtx)));` |
|    17 |  6435 | `	return PH7_OK;` |
|     1 |  6436 | `}` |
|     2 |  6437 | `static int vm_builtin_SplDll_isEmpty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6438 | `{` |
|     1 |  6439 | `	SXUNUSED(nArg);` |
|     1 |  6440 | `	SXUNUSED(apArg);` |
|     3 |  6441 | `	ph7_result_bool(pCtx,DllCount(pCtx->pVm,PH7_ContextThis(pCtx)) == 0);` |
|     3 |  6442 | `	return PH7_OK;` |
|     1 |  6443 | `}` |
|    22 |  6444 | `static int vm_builtin_SplDll_setIteratorMode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6445 | `{` |
|    23 |  6446 | `	ph7_vm *pVm = pCtx->pVm;` |
|    23 |  6447 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    23 |  6448 | `	int iFlags = DllFlags(pThis);` |
|    23 |  6449 | `	sxi64 iMode = 0;` |
|     - |  6450 | `	sxi32 rc;` |
|    23 |  6451 | `	if( pThis == 0 ){` |
|   ! 0 |  6452 | `		return PH7_OK;` |
|     - |  6453 | `	}` |
|    23 |  6454 | `	if( nArg < 1 ){` |
|   ! 0 |  6455 | `		return PH7_OK;   /* the arity screen already refused */` |
|     - |  6456 | `	}` |
|    23 |  6457 | `	rc = PH7_IntArgResolve(pCtx,apArg[0],"SplDoublyLinkedList::setIteratorMode",1,"$mode","int",&iMode);` |
|    23 |  6458 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  6459 | `		return rc;` |
|     - |  6460 | `	}` |
|    23 |  6461 | `	if( (iFlags & DLL_IT_FIX) && (iFlags & DLL_IT_LIFO) != ((int)iMode & DLL_IT_LIFO) ){` |
|     7 |  6462 | `		return PH7_VmThrowException(pCtx,"RuntimeException",` |
|     - |  6463 | `			"Iterators' LIFO/FIFO modes for SplStack/SplQueue objects are frozen");` |
|     - |  6464 | `	}` |
|     - |  6465 | `	/* php MASKS the value to the two mode bits and re-adds IT_FIX, so a nonsense` |
|     - |  6466 | `	 * mode is silently reduced rather than refused — and the ANSWER is the stored` |
|     - |  6467 | `	 * word, which is how a caller sees the fix bit at all. */` |
|    17 |  6468 | `	iFlags = ((int)iMode & DLL_IT_MASK) \| (iFlags & DLL_IT_FIX);` |
|    17 |  6469 | `	PH7_NativeSetAttrInt(pVm,pThis,DLL_FL,iFlags);` |
|    17 |  6470 | `	ph7_result_int64(pCtx,(ph7_int64)iFlags);` |
|    17 |  6471 | `	return PH7_OK;` |
|    12 |  6472 | `}` |
|    12 |  6473 | `static int vm_builtin_SplDll_getIteratorMode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6474 | `{` |
|     6 |  6475 | `	SXUNUSED(nArg);` |
|     6 |  6476 | `	SXUNUSED(apArg);` |
|    13 |  6477 | `	ph7_result_int64(pCtx,(ph7_int64)DllFlags(PH7_ContextThis(pCtx)));` |
|    13 |  6478 | `	return PH7_OK;` |
|     1 |  6479 | `}` |
|     6 |  6480 | `static int vm_builtin_SplDll_add(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6481 | `{` |
|     7 |  6482 | `	ph7_vm *pVm = pCtx->pVm;` |
|     7 |  6483 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     7 |  6484 | `	sxi64 nCount = DllCount(pVm,pThis);` |
|     7 |  6485 | `	sxi64 iIndex = 0;` |
|     - |  6486 | `	ph7_value sOff,sLen,sRep,*apExtra[3];` |
|     - |  6487 | `	ph7_hashmap *pRep;` |
|     - |  6488 | `	sxi32 rc;` |
|     7 |  6489 | `	if( nArg < 2 ){` |
|   ! 0 |  6490 | `		return PH7_OK;` |
|     - |  6491 | `	}` |
|     7 |  6492 | `	rc = DllIndexArg(pCtx,"add",apArg[0],&iIndex);` |
|     7 |  6493 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  6494 | `		return rc;` |
|     - |  6495 | `	}` |
|     7 |  6496 | `	if( iIndex < 0 \|\| iIndex > nCount ){` |
|     5 |  6497 | `		return DllOutOfRange(pCtx,"add");` |
|     - |  6498 | `	}` |
|     3 |  6499 | `	if( iIndex == nCount ){` |
|     - |  6500 | `		/* php: "the last entry + 1" is a push, because there is nothing to insert` |
|     - |  6501 | `		 * before. Note this is the LIST tail in both modes. */` |
|     - |  6502 | `		ph7_value *apOne[1];` |
|   ! 0 |  6503 | `		apOne[0] = apArg[1];` |
|   ! 0 |  6504 | `		DllArrayCall(pCtx,ph7_hashmap_push,apOne,1);` |
|   ! 0 |  6505 | `		ph7_result_null(pCtx);` |
|   ! 0 |  6506 | `		return PH7_OK;` |
|     - |  6507 | `	}` |
|     3 |  6508 | `	pRep = PH7_NewHashmap(pVm,0,0);` |
|     3 |  6509 | `	if( pRep == 0 ){` |
|   ! 0 |  6510 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  6511 | `	}` |
|     3 |  6512 | `	PH7_MemObjInit(pVm,&sRep);` |
|     3 |  6513 | `	sRep.x.pOther = pRep;` |
|     3 |  6514 | `	MemObjSetType(&sRep,MEMOBJ_HASHMAP);` |
|     3 |  6515 | `	PH7_HashmapInsert(pRep,0,apArg[1]);` |
|     3 |  6516 | `	PH7_MemObjInitFromInt(pVm,&sOff,DllOffsetToIndex(pThis,iIndex,nCount));` |
|     3 |  6517 | `	PH7_MemObjInitFromInt(pVm,&sLen,0);` |
|     3 |  6518 | `	apExtra[0] = &sOff;` |
|     3 |  6519 | `	apExtra[1] = &sLen;` |
|     3 |  6520 | `	apExtra[2] = &sRep;` |
|     3 |  6521 | `	DllArrayCall(pCtx,ph7_hashmap_splice,apExtra,3);` |
|     3 |  6522 | `	PH7_MemObjRelease(&sOff);` |
|     3 |  6523 | `	PH7_MemObjRelease(&sLen);` |
|     3 |  6524 | `	PH7_MemObjRelease(&sRep);` |
|     3 |  6525 | `	ph7_result_null(pCtx);   /* array_splice answers what it removed; add() is void */` |
|     3 |  6526 | `	return PH7_OK;` |
|     4 |  6527 | `}` |
|     6 |  6528 | `static int vm_builtin_SplDll_offsetExists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6529 | `{` |
|     7 |  6530 | `	sxi64 iIndex = 0;` |
|     - |  6531 | `	sxi32 rc;` |
|     7 |  6532 | `	if( nArg < 1 ){` |
|   ! 0 |  6533 | `		return PH7_OK;` |
|     - |  6534 | `	}` |
|     7 |  6535 | `	rc = DllIndexArg(pCtx,"offsetExists",apArg[0],&iIndex);` |
|     7 |  6536 | `	if( rc != SXRET_OK ){` |
|     3 |  6537 | `		return rc;` |
|     - |  6538 | `	}` |
|     5 |  6539 | `	ph7_result_bool(pCtx,iIndex >= 0 && iIndex < DllCount(pCtx->pVm,PH7_ContextThis(pCtx)));` |
|     5 |  6540 | `	return PH7_OK;` |
|     4 |  6541 | `}` |
|    22 |  6542 | `static int vm_builtin_SplDll_offsetGet(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6543 | `{` |
|    23 |  6544 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    23 |  6545 | `	sxi64 nCount = DllCount(pCtx->pVm,pThis);` |
|    23 |  6546 | `	sxi64 iIndex = 0;` |
|     - |  6547 | `	ph7_value *pVal;` |
|     - |  6548 | `	sxi32 rc;` |
|    23 |  6549 | `	if( nArg < 1 ){` |
|   ! 0 |  6550 | `		return PH7_OK;` |
|     - |  6551 | `	}` |
|    23 |  6552 | `	rc = DllIndexArg(pCtx,"offsetGet",apArg[0],&iIndex);` |
|    23 |  6553 | `	if( rc != SXRET_OK ){` |
|     3 |  6554 | `		return rc;` |
|     - |  6555 | `	}` |
|    21 |  6556 | `	if( iIndex < 0 \|\| iIndex >= nCount ){` |
|     5 |  6557 | `		return DllOutOfRange(pCtx,"offsetGet");` |
|     - |  6558 | `	}` |
|    17 |  6559 | `	pVal = DllAt(pCtx->pVm,pThis,DllOffsetToIndex(pThis,iIndex,nCount));` |
|    17 |  6560 | `	if( pVal ){` |
|    17 |  6561 | `		ph7_result_value(pCtx,pVal);` |
|     8 |  6562 | `	}` |
|    17 |  6563 | `	return PH7_OK;` |
|    12 |  6564 | `}` |
|     6 |  6565 | `static int vm_builtin_SplDll_offsetSet(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6566 | `{` |
|     7 |  6567 | `	ph7_vm *pVm = pCtx->pVm;` |
|     7 |  6568 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     7 |  6569 | `	sxi64 nCount = DllCount(pVm,pThis);` |
|     7 |  6570 | `	sxi64 iIndex = 0;` |
|     - |  6571 | `	ph7_hashmap *pMap;` |
|     7 |  6572 | `	ph7_hashmap_node *pNode = 0;` |
|     - |  6573 | `	ph7_value sKey;` |
|     - |  6574 | `	sxi32 rc;` |
|     7 |  6575 | `	if( nArg < 2 ){` |
|   ! 0 |  6576 | `		return PH7_OK;` |
|     - |  6577 | `	}` |
|     7 |  6578 | `	if( apArg[0]->iFlags & MEMOBJ_NULL ){` |
|     - |  6579 | ``		/* php: a null offset is `$dll[] = v`, which pushes. */`` |
|     - |  6580 | `		ph7_value *apOne[1];` |
|   ! 0 |  6581 | `		apOne[0] = apArg[1];` |
|   ! 0 |  6582 | `		DllArrayCall(pCtx,ph7_hashmap_push,apOne,1);` |
|   ! 0 |  6583 | `		ph7_result_null(pCtx);` |
|   ! 0 |  6584 | `		return PH7_OK;` |
|     - |  6585 | `	}` |
|     7 |  6586 | `	rc = DllIndexArg(pCtx,"offsetSet",apArg[0],&iIndex);` |
|     7 |  6587 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  6588 | `		return rc;` |
|     - |  6589 | `	}` |
|     7 |  6590 | `	if( iIndex < 0 \|\| iIndex >= nCount ){` |
|     5 |  6591 | `		return DllOutOfRange(pCtx,"offsetSet");` |
|     - |  6592 | `	}` |
|     3 |  6593 | `	pMap = DllMap(pVm,pThis);` |
|     3 |  6594 | `	PH7_MemObjInitFromInt(pVm,&sKey,DllOffsetToIndex(pThis,iIndex,nCount));` |
|     3 |  6595 | `	if( pMap && PH7_HashmapLookup(pMap,&sKey,&pNode) == SXRET_OK ){` |
|     3 |  6596 | `		ph7_value *pDest = HashmapExtractNodeValue(pNode);` |
|     3 |  6597 | `		if( pDest ){` |
|     3 |  6598 | `			PH7_MemObjStore(apArg[1],pDest);` |
|     1 |  6599 | `		}` |
|     1 |  6600 | `	}` |
|     3 |  6601 | `	PH7_MemObjRelease(&sKey);` |
|     3 |  6602 | `	return PH7_OK;` |
|     4 |  6603 | `}` |
|     6 |  6604 | `static int vm_builtin_SplDll_offsetUnset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6605 | `{` |
|     7 |  6606 | `	ph7_vm *pVm = pCtx->pVm;` |
|     7 |  6607 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     7 |  6608 | `	sxi64 nCount = DllCount(pVm,pThis);` |
|     7 |  6609 | `	sxi64 iIndex = 0;` |
|     - |  6610 | `	ph7_value sOff,sLen,*apExtra[2];` |
|     - |  6611 | `	sxi32 rc;` |
|     7 |  6612 | `	if( nArg < 1 ){` |
|   ! 0 |  6613 | `		return PH7_OK;` |
|     - |  6614 | `	}` |
|     7 |  6615 | `	rc = DllIndexArg(pCtx,"offsetUnset",apArg[0],&iIndex);` |
|     7 |  6616 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  6617 | `		return rc;` |
|     - |  6618 | `	}` |
|     7 |  6619 | `	if( iIndex < 0 \|\| iIndex >= nCount ){` |
|     3 |  6620 | `		return DllOutOfRange(pCtx,"offsetUnset");` |
|     - |  6621 | `	}` |
|     5 |  6622 | `	PH7_MemObjInitFromInt(pVm,&sOff,DllOffsetToIndex(pThis,iIndex,nCount));` |
|     5 |  6623 | `	PH7_MemObjInitFromInt(pVm,&sLen,1);` |
|     5 |  6624 | `	apExtra[0] = &sOff;` |
|     5 |  6625 | `	apExtra[1] = &sLen;` |
|     5 |  6626 | `	DllArrayCall(pCtx,ph7_hashmap_splice,apExtra,2);` |
|     5 |  6627 | `	PH7_MemObjRelease(&sOff);` |
|     5 |  6628 | `	PH7_MemObjRelease(&sLen);` |
|     5 |  6629 | `	ph7_result_null(pCtx);` |
|     5 |  6630 | `	return PH7_OK;` |
|     4 |  6631 | `}` |
|     - |  6632 | `/*` |
|     - |  6633 | ` * The cursor. php's traverse_position IS the list index in both directions — a` |
|     - |  6634 | ` * LIFO rewind seeds count-1 and counts down — so current() and key() need no mode` |
|     - |  6635 | ` * test at all. IT_DELETE is the exception: in FIFO order php consumes the head and` |
|     - |  6636 | ` * leaves the position alone, so every element of a consuming walk reports key 0.` |
|     - |  6637 | ` */` |
|    24 |  6638 | `static int vm_builtin_SplDll_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6639 | `{` |
|    25 |  6640 | `	ph7_vm *pVm = pCtx->pVm;` |
|    25 |  6641 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    12 |  6642 | `	SXUNUSED(nArg);` |
|    12 |  6643 | `	SXUNUSED(apArg);` |
|    25 |  6644 | `	if( pThis == 0 ){` |
|   ! 0 |  6645 | `		return PH7_OK;` |
|     - |  6646 | `	}` |
|    37 |  6647 | `	PH7_NativeSetAttrInt(pVm,pThis,DLL_I,` |
|    24 |  6648 | `		(DllFlags(pThis) & DLL_IT_LIFO) ? DllCount(pVm,pThis)-1 : 0);` |
|    25 |  6649 | `	return PH7_OK;` |
|    13 |  6650 | `}` |
|    82 |  6651 | `static int vm_builtin_SplDll_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6652 | `{` |
|    83 |  6653 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    83 |  6654 | `	sxi64 iPos = pThis ? PH7_NativeAttrInt(pThis,DLL_I) : 0;` |
|    41 |  6655 | `	SXUNUSED(nArg);` |
|    41 |  6656 | `	SXUNUSED(apArg);` |
|    83 |  6657 | `	ph7_result_bool(pCtx,iPos >= 0 && iPos < DllCount(pCtx->pVm,pThis));` |
|    83 |  6658 | `	return PH7_OK;` |
|     1 |  6659 | `}` |
|    58 |  6660 | `static int vm_builtin_SplDll_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6661 | `{` |
|    59 |  6662 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    59 |  6663 | `	ph7_value *pVal = pThis ? DllAt(pCtx->pVm,pThis,PH7_NativeAttrInt(pThis,DLL_I)) : 0;` |
|    29 |  6664 | `	SXUNUSED(nArg);` |
|    29 |  6665 | `	SXUNUSED(apArg);` |
|    59 |  6666 | `	if( pVal ){` |
|    59 |  6667 | `		ph7_result_value(pCtx,pVal);` |
|    30 |  6668 | `	}else{` |
|   ! 0 |  6669 | `		ph7_result_null(pCtx);` |
|     - |  6670 | `	}` |
|    59 |  6671 | `	return PH7_OK;` |
|     1 |  6672 | `}` |
|    48 |  6673 | `static int vm_builtin_SplDll_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6674 | `{` |
|    49 |  6675 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    24 |  6676 | `	SXUNUSED(nArg);` |
|    24 |  6677 | `	SXUNUSED(apArg);` |
|    49 |  6678 | `	ph7_result_int64(pCtx,pThis ? PH7_NativeAttrInt(pThis,DLL_I) : 0);` |
|    49 |  6679 | `	return PH7_OK;` |
|     1 |  6680 | `}` |
|     - |  6681 | ``/* php's move_forward, with the direction flipped for prev() (its `flags ^ LIFO`). */`` |
|    58 |  6682 | `static int DllStep(ph7_context *pCtx,int bFlip)` |
|     1 |  6683 | `{` |
|    59 |  6684 | `	ph7_vm *pVm = pCtx->pVm;` |
|    59 |  6685 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  6686 | `	int iFlags;` |
|     - |  6687 | `	sxi64 iPos;` |
|    59 |  6688 | `	if( pThis == 0 ){` |
|   ! 0 |  6689 | `		return PH7_OK;` |
|     - |  6690 | `	}` |
|    59 |  6691 | `	iFlags = DllFlags(pThis);` |
|    59 |  6692 | `	if( bFlip ){` |
|   ! 0 |  6693 | `		iFlags ^= DLL_IT_LIFO;` |
|   ! 0 |  6694 | `	}` |
|    59 |  6695 | `	iPos = PH7_NativeAttrInt(pThis,DLL_I);` |
|    59 |  6696 | `	if( iPos < 0 \|\| iPos >= DllCount(pVm,pThis) ){` |
|     - |  6697 | `		/* php only steps a LIVE pointer; off the end nothing moves and nothing is` |
|     - |  6698 | `		 * consumed. The position still has to move for a plain walk, though, or` |
|     - |  6699 | `		 * prev() past the head could never come back. */` |
|   ! 0 |  6700 | `		if( (iFlags & DLL_IT_DELETE) == 0 ){` |
|   ! 0 |  6701 | `			PH7_NativeSetAttrInt(pVm,pThis,DLL_I,` |
|   ! 0 |  6702 | `				iPos + ((iFlags & DLL_IT_LIFO) ? -1 : 1));` |
|   ! 0 |  6703 | `		}` |
|   ! 0 |  6704 | `		return PH7_OK;` |
|     - |  6705 | `	}` |
|    59 |  6706 | `	if( iFlags & DLL_IT_DELETE ){` |
|    23 |  6707 | `		if( iFlags & DLL_IT_LIFO ){` |
|     7 |  6708 | `			DllArrayCall(pCtx,ph7_hashmap_pop,0,0);` |
|     7 |  6709 | `			PH7_NativeSetAttrInt(pVm,pThis,DLL_I,iPos-1);` |
|     4 |  6710 | `		}else{` |
|     - |  6711 | `			/* php consumes the head and does NOT advance: the walk stays at 0. */` |
|    17 |  6712 | `			DllArrayCall(pCtx,ph7_hashmap_shift,0,0);` |
|     - |  6713 | `		}` |
|    23 |  6714 | `		ph7_result_null(pCtx);` |
|    23 |  6715 | `		return PH7_OK;` |
|     - |  6716 | `	}` |
|    37 |  6717 | `	PH7_NativeSetAttrInt(pVm,pThis,DLL_I,iPos + ((iFlags & DLL_IT_LIFO) ? -1 : 1));` |
|    37 |  6718 | `	return PH7_OK;` |
|    30 |  6719 | `}` |
|    58 |  6720 | `static int vm_builtin_SplDll_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6721 | `{` |
|    29 |  6722 | `	SXUNUSED(nArg);` |
|    29 |  6723 | `	SXUNUSED(apArg);` |
|    59 |  6724 | `	return DllStep(pCtx,FALSE);` |
|     1 |  6725 | `}` |
|   ! 0 |  6726 | `static int vm_builtin_SplDll_prev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 |  6727 | `{` |
|   ! 0 |  6728 | `	SXUNUSED(nArg);` |
|   ! 0 |  6729 | `	SXUNUSED(apArg);` |
|   ! 0 |  6730 | `	return DllStep(pCtx,TRUE);` |
|   ! 0 |  6731 | `}` |
|     - |  6732 | `/*` |
|     - |  6733 | `` * php's get_debug_info: `flags` then `dllist`, and NOTHING for the (array) cast --`` |
|     - |  6734 | ` * the same var_dump/cast disagreement WeakReference has, which is why xPresent is` |
|     - |  6735 | `` * told which surface is asking. `__debugInfo()` is the same array, reachable by`` |
|     - |  6736 | ` * name because php declares it.` |
|     - |  6737 | ` *` |
|     - |  6738 | ` * Every key is php's MANGLED private name, which is what makes the dump print` |
|     - |  6739 | ``  * `[flags:SplDoublyLinkedList:private]` rather than a plain `[flags]` `` |
|     - |  6740 | ` * (SplObjectStorage's storage key has read that way all along).` |
|     - |  6741 | ` */` |
|     - |  6742 | `/*` |
|     - |  6743 | `` * One `"\0Class\0member"` key, under the class php says DECLARES the slot -- which`` |
|     - |  6744 | ` * for every container here is the ROOT of the chain, so SplStack, SplQueue,` |
|     - |  6745 | ` * SplMinHeap and any userland subclass all show the base's name rather than their` |
|     - |  6746 | ` * own, and SplPriorityQueue (which extends nothing) shows itself.` |
|     - |  6747 | ` */` |
|   112 |  6748 | `static sxi32 SplRootDebugKey(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pKey,` |
|     - |  6749 | `	const char *zMember)` |
|     1 |  6750 | `{` |
|   113 |  6751 | `	ph7_class *pRoot = pThis->pClass;` |
|   215 |  6752 | `	while( pRoot->pBase ){` |
|   103 |  6753 | `		pRoot = pRoot->pBase;` |
|     1 |  6754 | `	}` |
|   113 |  6755 | `	PH7_MemObjInitFromString(pVm,pKey,0);` |
|   113 |  6756 | `	PH7_MemObjStringAppend(pKey,"\0",1);` |
|   113 |  6757 | `	PH7_MemObjStringAppend(pKey,SyStringData(&pRoot->sName),SyStringLength(&pRoot->sName));` |
|   113 |  6758 | `	PH7_MemObjStringAppend(pKey,"\0",1);` |
|   113 |  6759 | `	PH7_MemObjStringAppend(pKey,zMember,(sxu32)SyStrlen(zMember));` |
|   113 |  6760 | `	return PH7_OK;` |
|     1 |  6761 | `}` |
|    26 |  6762 | `static sxi32 DllFillDebug(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut)` |
|     1 |  6763 | `{` |
|     - |  6764 | `	ph7_value sKey,sVal,*pStore;` |
|    27 |  6765 | `	SplRootDebugKey(pVm,pThis,&sKey,"flags");` |
|    27 |  6766 | `	PH7_MemObjInitFromInt(pVm,&sVal,DllFlags(pThis));` |
|    27 |  6767 | `	ph7_array_add_elem(pOut,&sKey,&sVal);` |
|    27 |  6768 | `	PH7_MemObjRelease(&sKey);` |
|    27 |  6769 | `	PH7_MemObjRelease(&sVal);` |
|    27 |  6770 | `	pStore = DllSlot(pVm,pThis);` |
|    27 |  6771 | `	SplRootDebugKey(pVm,pThis,&sKey,"dllist");` |
|    27 |  6772 | `	if( pStore ){` |
|    27 |  6773 | `		ph7_array_add_elem(pOut,&sKey,pStore);` |
|    13 |  6774 | `	}` |
|    27 |  6775 | `	PH7_MemObjRelease(&sKey);` |
|    27 |  6776 | `	return PH7_OK;` |
|     1 |  6777 | `}` |
|    26 |  6778 | `static sxi32 DllPresent(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)` |
|     1 |  6779 | `{` |
|    27 |  6780 | `	if( !bDebug ){` |
|    11 |  6781 | `		return PH7_OK;   /* php's (array) cast and var_export show nothing */` |
|     - |  6782 | `	}` |
|    17 |  6783 | `	return DllFillDebug(pVm,pThis,pOut);` |
|    14 |  6784 | `}` |
|    10 |  6785 | `static int vm_builtin_SplDll_debugInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6786 | `{` |
|    11 |  6787 | `	ph7_vm *pVm = pCtx->pVm;` |
|    11 |  6788 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  6789 | `	ph7_value sOut;` |
|     5 |  6790 | `	SXUNUSED(nArg);` |
|     5 |  6791 | `	SXUNUSED(apArg);` |
|    11 |  6792 | `	PH7_MemObjInit(pVm,&sOut);` |
|    11 |  6793 | `	if( PH7_MemObjToHashmap(&sOut) != SXRET_OK ){` |
|   ! 0 |  6794 | `		PH7_MemObjRelease(&sOut);` |
|   ! 0 |  6795 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  6796 | `	}` |
|    11 |  6797 | `	DllFillDebug(pVm,pThis,&sOut);` |
|    11 |  6798 | `	ph7_result_value(pCtx,&sOut);` |
|    11 |  6799 | `	PH7_MemObjRelease(&sOut);` |
|    11 |  6800 | `	return PH7_OK;` |
|     6 |  6801 | `}` |
|     - |  6802 | `/*` |
|     - |  6803 | ` * php's __serialize(): [flags, elements, dynamic members]. This is what` |
|     - |  6804 | ` * serialize() actually uses -- the Serializable pair below exists because the` |
|     - |  6805 | ` * interface is still declared, and php words its own legacy format there.` |
|     - |  6806 | ` */` |
|    18 |  6807 | `static int vm_builtin_SplDll_serializeMagic(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6808 | `{` |
|    19 |  6809 | `	ph7_vm *pVm = pCtx->pVm;` |
|    19 |  6810 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  6811 | `	ph7_value sOut,sVal,*pStore;` |
|     9 |  6812 | `	SXUNUSED(nArg);` |
|     9 |  6813 | `	SXUNUSED(apArg);` |
|    19 |  6814 | `	PH7_MemObjInit(pVm,&sOut);` |
|    19 |  6815 | `	if( PH7_MemObjToHashmap(&sOut) != SXRET_OK ){` |
|   ! 0 |  6816 | `		PH7_MemObjRelease(&sOut);` |
|   ! 0 |  6817 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  6818 | `	}` |
|    19 |  6819 | `	PH7_MemObjInitFromInt(pVm,&sVal,DllFlags(pThis));` |
|    19 |  6820 | `	ph7_array_add_elem(&sOut,0,&sVal);` |
|    19 |  6821 | `	PH7_MemObjRelease(&sVal);` |
|    19 |  6822 | `	pStore = DllSlot(pVm,pThis);` |
|    19 |  6823 | `	if( pStore ){` |
|    19 |  6824 | `		ph7_array_add_elem(&sOut,0,pStore);` |
|     9 |  6825 | `	}` |
|     - |  6826 | `	/* The members slot: php's own properties — empty for a bare SplStack, a` |
|     - |  6827 | `	 * SUBCLASS's declared slots when there is one. */` |
|    19 |  6828 | `	if( SplMembersOf(pVm,pThis,&sVal) == SXRET_OK ){` |
|    19 |  6829 | `		ph7_array_add_elem(&sOut,0,&sVal);` |
|     9 |  6830 | `	}` |
|    19 |  6831 | `	PH7_MemObjRelease(&sVal);` |
|    19 |  6832 | `	ph7_result_value(pCtx,&sOut);` |
|    19 |  6833 | `	PH7_MemObjRelease(&sOut);` |
|    19 |  6834 | `	return PH7_OK;` |
|    10 |  6835 | `}` |
|     6 |  6836 | `static int vm_builtin_SplDll_unserializeMagic(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6837 | `{` |
|     7 |  6838 | `	ph7_vm *pVm = pCtx->pVm;` |
|     7 |  6839 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  6840 | `	ph7_hashmap *pData;` |
|     7 |  6841 | `	ph7_hashmap_node *pNode = 0;` |
|     - |  6842 | `	ph7_value *pFlags,*pStore,*pSlot;` |
|     7 |  6843 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_HASHMAP) == 0 \|\| pThis == 0 ){` |
|   ! 0 |  6844 | `		return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     - |  6845 | `			"Incomplete or ill-typed serialization data");` |
|     - |  6846 | `	}` |
|     7 |  6847 | `	pData = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     7 |  6848 | `	if( HashmapLookupIntKey(pData,0,&pNode) != SXRET_OK ){` |
|   ! 0 |  6849 | `		return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     - |  6850 | `			"Incomplete or ill-typed serialization data");` |
|     - |  6851 | `	}` |
|     7 |  6852 | `	pFlags = HashmapExtractNodeValue(pNode);` |
|     6 |  6853 | `	if( pFlags == 0 \|\| (pFlags->iFlags & MEMOBJ_INT) == 0` |
|     7 |  6854 | `	 \|\| HashmapLookupIntKey(pData,1,&pNode) != SXRET_OK ){` |
|   ! 0 |  6855 | `		return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     - |  6856 | `			"Incomplete or ill-typed serialization data");` |
|     - |  6857 | `	}` |
|     7 |  6858 | `	pStore = HashmapExtractNodeValue(pNode);` |
|     7 |  6859 | `	if( pStore == 0 \|\| (pStore->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|   ! 0 |  6860 | `		return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     - |  6861 | `			"Incomplete or ill-typed serialization data");` |
|     - |  6862 | `	}` |
|     7 |  6863 | `	PH7_NativeSetAttrInt(pVm,pThis,DLL_FL,ph7_value_to_int64(pFlags));` |
|     7 |  6864 | `	pSlot = PH7_NativeAttr(pThis,DLL_Q);` |
|     7 |  6865 | `	if( pSlot ){` |
|     7 |  6866 | `		PH7_MemObjRelease(pSlot);` |
|     7 |  6867 | `		PH7_MemObjStore(pStore,pSlot);` |
|     3 |  6868 | `	}` |
|     7 |  6869 | `	if( HashmapLookupIntKey(pData,2,&pNode) == SXRET_OK ){` |
|     7 |  6870 | `		SplMembersLoad(pThis,HashmapExtractNodeValue(pNode));` |
|     3 |  6871 | `	}` |
|     7 |  6872 | `	return PH7_OK;` |
|     4 |  6873 | `}` |
|     - |  6874 | `/*` |
|     - |  6875 | ` * php's Serializable pair, kept because the interface is still declared: the` |
|     - |  6876 | ` * format is the serialized FLAGS followed by one ':' + serialized value per` |
|     - |  6877 | ` * element ("i:0;:i:1;:i:2;"), which nothing else in php produces or reads.` |
|     - |  6878 | ` */` |
|     - |  6879 | `/*` |
|     - |  6880 | ` * One serialized value, appended to a blob. The RESET is the point: the engine's` |
|     - |  6881 | ` * serialize() writes through ph7_value_string, which APPENDS to the context's` |
|     - |  6882 | ` * return slot rather than replacing it, so a loop that calls it per element` |
|     - |  6883 | ` * accumulates every previous answer into the next one. Shared with` |
|     - |  6884 | ` * SplObjectStorage's legacy format, which is built the same way.` |
|     - |  6885 | ` */` |
|    56 |  6886 | `static void SplSerializeInto(ph7_context *pCtx,ph7_value **apCall,SyBlob *pOut)` |
|     1 |  6887 | `{` |
|    57 |  6888 | `	int nLen = 0;` |
|     - |  6889 | `	const char *zTxt;` |
|    57 |  6890 | `	if( pCtx->pRet ){` |
|    57 |  6891 | `		PH7_MemObjRelease(pCtx->pRet);` |
|    28 |  6892 | `	}` |
|    57 |  6893 | `	vm_builtin_serialize(pCtx,1,apCall);` |
|    57 |  6894 | `	if( pCtx->pRet == 0 ){` |
|   ! 0 |  6895 | `		return;` |
|     - |  6896 | `	}` |
|    57 |  6897 | `	zTxt = ph7_value_to_string(pCtx->pRet,&nLen);` |
|    57 |  6898 | `	SyBlobAppend(pOut,zTxt,(sxu32)nLen);` |
|    29 |  6899 | `}` |
|     2 |  6900 | `static int vm_builtin_SplDll_serialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6901 | `{` |
|     3 |  6902 | `	ph7_vm *pVm = pCtx->pVm;` |
|     3 |  6903 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     3 |  6904 | `	ph7_hashmap *pMap = DllMap(pVm,pThis);` |
|     - |  6905 | `	ph7_hashmap_node *pNode;` |
|     - |  6906 | `	SyBlob sOut;` |
|     - |  6907 | `	ph7_value sFlags,*apCall[1];` |
|     3 |  6908 | `	sxi64 n,nCount = pMap ? (sxi64)pMap->nEntry : 0;` |
|     1 |  6909 | `	SXUNUSED(nArg);` |
|     1 |  6910 | `	SXUNUSED(apArg);` |
|     3 |  6911 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|     3 |  6912 | `	PH7_MemObjInitFromInt(pVm,&sFlags,DllFlags(pThis));` |
|     3 |  6913 | `	apCall[0] = &sFlags;` |
|     3 |  6914 | `	SplSerializeInto(pCtx,apCall,&sOut);` |
|     3 |  6915 | `	PH7_MemObjRelease(&sFlags);` |
|     9 |  6916 | `	for( n = 0 ; n < nCount ; ++n ){` |
|     - |  6917 | `		ph7_value *pVal;` |
|     7 |  6918 | `		pNode = 0;` |
|     7 |  6919 | `		if( HashmapLookupIntKey(pMap,n,&pNode) != SXRET_OK ){` |
|   ! 0 |  6920 | `			continue;` |
|     - |  6921 | `		}` |
|     7 |  6922 | `		pVal = HashmapExtractNodeValue(pNode);` |
|     7 |  6923 | `		if( pVal == 0 ){` |
|   ! 0 |  6924 | `			continue;` |
|     - |  6925 | `		}` |
|     7 |  6926 | `		apCall[0] = pVal;` |
|     7 |  6927 | `		SyBlobAppend(&sOut,":",1);` |
|     7 |  6928 | `		SplSerializeInto(pCtx,apCall,&sOut);` |
|     4 |  6929 | `	}` |
|     - |  6930 | `	/* ph7_result_string APPENDS too, and pRet still holds the LAST element's` |
|     - |  6931 | `	 * serialization from the loop above — drop it before writing the answer. */` |
|     3 |  6932 | `	if( pCtx->pRet ){` |
|     3 |  6933 | `		PH7_MemObjRelease(pCtx->pRet);` |
|     1 |  6934 | `	}` |
|     3 |  6935 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|     3 |  6936 | `	SyBlobRelease(&sOut);` |
|     3 |  6937 | `	return PH7_OK;` |
|     1 |  6938 | `}` |
|   ! 0 |  6939 | `static int vm_builtin_SplDll_unserialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 |  6940 | `{` |
|   ! 0 |  6941 | `	ph7_vm *pVm = pCtx->pVm;` |
|   ! 0 |  6942 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  6943 | `	const char *zData,*zCur,*zEnd;` |
|   ! 0 |  6944 | `	int nData = 0;` |
|   ! 0 |  6945 | `	int bFirst = 1;` |
|     - |  6946 | `	ph7_value *pSlot;` |
|     - |  6947 | `	ph7_hashmap *pMap;` |
|   ! 0 |  6948 | `	if( nArg < 1 \|\| pThis == 0 ){` |
|   ! 0 |  6949 | `		return PH7_OK;` |
|     - |  6950 | `	}` |
|   ! 0 |  6951 | `	zData = ph7_value_to_string(apArg[0],&nData);` |
|   ! 0 |  6952 | `	if( nData < 1 ){` |
|   ! 0 |  6953 | `		return PH7_OK;   /* php returns without touching the list */` |
|     - |  6954 | `	}` |
|   ! 0 |  6955 | `	pSlot = DllSlot(pVm,pThis);` |
|   ! 0 |  6956 | `	pMap = pSlot ? (ph7_hashmap *)pSlot->x.pOther : 0;` |
|   ! 0 |  6957 | `	if( pMap == 0 ){` |
|   ! 0 |  6958 | `		return PH7_OK;` |
|     - |  6959 | `	}` |
|     - |  6960 | `	/* php empties the list first, then reads the flags, then one ':'-prefixed` |
|     - |  6961 | `	 * value per element. A malformed tail is an UnexpectedValueException naming` |
|     - |  6962 | `	 * the byte offset — reproduced here from the same position arithmetic. */` |
|   ! 0 |  6963 | `	while( pMap->pFirst ){` |
|   ! 0 |  6964 | `		PH7_HashmapUnlinkNode(pMap->pFirst,TRUE);` |
|   ! 0 |  6965 | `	}` |
|   ! 0 |  6966 | `	zCur = zData;` |
|   ! 0 |  6967 | `	zEnd = &zData[nData];` |
|   ! 0 |  6968 | `	while( zCur < zEnd ){` |
|     - |  6969 | `		ph7_value sPart,sRes,*apCall[1];` |
|     - |  6970 | `		int nPart;` |
|   ! 0 |  6971 | `		const char *zStop = zCur;` |
|   ! 0 |  6972 | `		if( !bFirst ){` |
|   ! 0 |  6973 | `			if( zCur[0] != ':' ){` |
|   ! 0 |  6974 | `				break;` |
|     - |  6975 | `			}` |
|   ! 0 |  6976 | `			zCur++;` |
|   ! 0 |  6977 | `		}` |
|     - |  6978 | `		/* One serialized scalar reaches up to and including its ';'. */` |
|   ! 0 |  6979 | `		while( zStop < zEnd && zStop[0] != ';' ){` |
|   ! 0 |  6980 | `			zStop++;` |
|   ! 0 |  6981 | `		}` |
|   ! 0 |  6982 | `		if( zStop >= zEnd ){` |
|   ! 0 |  6983 | `			zStop = zEnd;` |
|   ! 0 |  6984 | `		}else{` |
|   ! 0 |  6985 | `			zStop++;` |
|     - |  6986 | `		}` |
|   ! 0 |  6987 | `		nPart = (int)(zStop - zCur);` |
|   ! 0 |  6988 | `		if( nPart <= 0 ){` |
|   ! 0 |  6989 | `			break;` |
|     - |  6990 | `		}` |
|   ! 0 |  6991 | `		PH7_MemObjInitFromString(pVm,&sPart,0);` |
|   ! 0 |  6992 | `		PH7_MemObjStringAppend(&sPart,zCur,(sxu32)nPart);` |
|   ! 0 |  6993 | `		apCall[0] = &sPart;` |
|   ! 0 |  6994 | `		PH7_MemObjInit(pVm,&sRes);` |
|   ! 0 |  6995 | `		if( pCtx->pRet ){` |
|   ! 0 |  6996 | `			PH7_MemObjRelease(pCtx->pRet);   /* see DllSerializeInto: pRet is appended to */` |
|   ! 0 |  6997 | `		}` |
|   ! 0 |  6998 | `		vm_builtin_unserialize(pCtx,1,apCall);` |
|   ! 0 |  6999 | `		if( pCtx->pRet ){` |
|   ! 0 |  7000 | `			PH7_MemObjStore(pCtx->pRet,&sRes);` |
|   ! 0 |  7001 | `		}` |
|   ! 0 |  7002 | `		if( bFirst ){` |
|   ! 0 |  7003 | `			PH7_NativeSetAttrInt(pVm,pThis,DLL_FL,ph7_value_to_int64(&sRes));` |
|   ! 0 |  7004 | `			bFirst = 0;` |
|   ! 0 |  7005 | `		}else{` |
|   ! 0 |  7006 | `			PH7_HashmapInsert(pMap,0,&sRes);` |
|     - |  7007 | `		}` |
|   ! 0 |  7008 | `		PH7_MemObjRelease(&sPart);` |
|   ! 0 |  7009 | `		PH7_MemObjRelease(&sRes);` |
|   ! 0 |  7010 | `		zCur = zStop;` |
|   ! 0 |  7011 | `	}` |
|   ! 0 |  7012 | `	ph7_result_null(pCtx);` |
|   ! 0 |  7013 | `	return PH7_OK;` |
|   ! 0 |  7014 | `}` |
|     - |  7015 | `/*` |
|     - |  7016 | ` * The declaration. Method ORDER is spl_dllist.stub.php's, php declares NO` |
|     - |  7017 | ` * constructor for any of the three, and the IT_FIX bit is a per-class DEFAULT on` |
|     - |  7018 | ` * the flags slot -- which is exactly how php does it (the create handler stamps` |
|     - |  7019 | ` * the flags; there is no constructor to run).` |
|     - |  7020 | ` */` |
|  8445 |  7021 | `static sxi32 VmInstallSplDllist(ph7_vm *pVm)` |
|     5 |  7022 | `{` |
|     - |  7023 | `	static const PH7_NativeMethodDef aDllMethod[] = {` |
|     - |  7024 | `		{ "add",             PH7_MOD_PUBLIC, "int $index, mixed $value", "@void",` |
|     - |  7025 | `		  vm_builtin_SplDll_add },` |
|     - |  7026 | `		{ "pop",             PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplDll_pop },` |
|     - |  7027 | `		{ "shift",           PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplDll_shift },` |
|     - |  7028 | `		{ "push",            PH7_MOD_PUBLIC, "mixed $value", "@void", vm_builtin_SplDll_push },` |
|     - |  7029 | `		{ "unshift",         PH7_MOD_PUBLIC, "mixed $value", "@void", vm_builtin_SplDll_unshift },` |
|     - |  7030 | `		{ "top",             PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplDll_top },` |
|     - |  7031 | `		{ "bottom",          PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplDll_bottom },` |
|     - |  7032 | `		{ "__debugInfo",     PH7_MOD_PUBLIC, "", "@array", vm_builtin_SplDll_debugInfo },` |
|     - |  7033 | `		{ "count",           PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplDll_count },` |
|     - |  7034 | `		{ "isEmpty",         PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplDll_isEmpty },` |
|     - |  7035 | `		{ "setIteratorMode", PH7_MOD_PUBLIC, "int $mode", "@int",` |
|     - |  7036 | `		  vm_builtin_SplDll_setIteratorMode },` |
|     - |  7037 | `		{ "getIteratorMode", PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplDll_getIteratorMode },` |
|     - |  7038 | ``		/* php's stub leaves these four offsets UNTYPED (a `@param int` docblock, which`` |
|     - |  7039 | `		 * Reflection does not print) while the ZPP enforces int -- so the signature says` |
|     - |  7040 | `		 * nothing and each body runs PH7_IntArgResolve itself. */` |
|     - |  7041 | `		{ "offsetExists",    PH7_MOD_PUBLIC, "$index", "@bool", vm_builtin_SplDll_offsetExists },` |
|     - |  7042 | `		{ "offsetGet",       PH7_MOD_PUBLIC, "$index", "@mixed", vm_builtin_SplDll_offsetGet },` |
|     - |  7043 | `		{ "offsetSet",       PH7_MOD_PUBLIC, "$index, mixed $value", "@void",` |
|     - |  7044 | `		  vm_builtin_SplDll_offsetSet },` |
|     - |  7045 | `		{ "offsetUnset",     PH7_MOD_PUBLIC, "$index", "@void", vm_builtin_SplDll_offsetUnset },` |
|     - |  7046 | `		{ "rewind",          PH7_MOD_PUBLIC, "", "@void", vm_builtin_SplDll_rewind },` |
|     - |  7047 | `		{ "current",         PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplDll_current },` |
|     - |  7048 | `		{ "key",             PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplDll_key },` |
|     - |  7049 | `		{ "prev",            PH7_MOD_PUBLIC, "", "@void", vm_builtin_SplDll_prev },` |
|     - |  7050 | `		{ "next",            PH7_MOD_PUBLIC, "", "@void", vm_builtin_SplDll_next },` |
|     - |  7051 | `		{ "valid",           PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplDll_valid },` |
|     - |  7052 | `		{ "unserialize",     PH7_MOD_PUBLIC, "string $data", "@void",` |
|     - |  7053 | `		  vm_builtin_SplDll_unserialize },` |
|     - |  7054 | `		{ "serialize",       PH7_MOD_PUBLIC, "", "@string", vm_builtin_SplDll_serialize },` |
|     - |  7055 | `		{ "__serialize",     PH7_MOD_PUBLIC, "", "@array", vm_builtin_SplDll_serializeMagic },` |
|     - |  7056 | `		{ "__unserialize",   PH7_MOD_PUBLIC, "array $data", "@void",` |
|     - |  7057 | `		  vm_builtin_SplDll_unserializeMagic },` |
|     - |  7058 | `	};` |
|     - |  7059 | `	static const PH7_NativeMethodDef aQueueMethod[] = {` |
|     - |  7060 | `		/* php's @implementation-alias: the same C bodies under the queue's names. */` |
|     - |  7061 | `		{ "enqueue", PH7_MOD_PUBLIC, "mixed $value", "@void", vm_builtin_SplDll_push },` |
|     - |  7062 | `		{ "dequeue", PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplDll_shift },` |
|     - |  7063 | `	};` |
|     - |  7064 | `	static const PH7_NativeConstDef aDllConst[] = {` |
|     - |  7065 | `		{ "IT_MODE_LIFO",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, DLL_IT_LIFO, 0, 0.0 },` |
|     - |  7066 | `		{ "IT_MODE_FIFO",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 0, 0, 0.0 },` |
|     - |  7067 | `		{ "IT_MODE_DELETE", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, DLL_IT_DELETE, 0, 0.0 },` |
|     - |  7068 | `		{ "IT_MODE_KEEP",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 0, 0, 0.0 },` |
|     - |  7069 | `	};` |
|     - |  7070 | `	static const PH7_NativePropDef aDllProp[] = {` |
|     - |  7071 | `		{ DLL_Q,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  7072 | `		{ DLL_FL, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  7073 | `		{ DLL_I,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  7074 | `	};` |
|     - |  7075 | `	/* php's object handler stamps IT_FIX (and LIFO for a stack) at CREATION, which` |
|     - |  7076 | `	 * is why neither subclass declares a constructor and why the bit survives every` |
|     - |  7077 | `	 * setIteratorMode(). A per-class default on the flags slot says the same thing. */` |
|     - |  7078 | `	static const PH7_NativePropDef aQueueProp[] = {` |
|     - |  7079 | `		{ DLL_FL, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN,` |
|     - |  7080 | `		  { 0, 0, PH7_NATIVE_VAL_INT, DLL_IT_FIX, 0, 0.0 }, 0 },` |
|     - |  7081 | `	};` |
|     - |  7082 | `	static const PH7_NativePropDef aStackProp[] = {` |
|     - |  7083 | `		{ DLL_FL, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN,` |
|     - |  7084 | `		  { 0, 0, PH7_NATIVE_VAL_INT, DLL_IT_FIX\|DLL_IT_LIFO, 0, 0.0 }, 0 },` |
|     - |  7085 | `	};` |
|     - |  7086 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - |  7087 | `		{ "SplDoublyLinkedList", 0, "Iterator,Countable,ArrayAccess,Serializable", 0,` |
|     - |  7088 | `		  aDllMethod, SX_ARRAYSIZE(aDllMethod),` |
|     - |  7089 | `		  aDllConst, SX_ARRAYSIZE(aDllConst),` |
|     - |  7090 | `		  aDllProp, SX_ARRAYSIZE(aDllProp), 0, 0, DllPresent },` |
|     - |  7091 | `		{ "SplQueue", "SplDoublyLinkedList", 0, 0,` |
|     - |  7092 | `		  aQueueMethod, SX_ARRAYSIZE(aQueueMethod), 0, 0,` |
|     - |  7093 | `		  aQueueProp, SX_ARRAYSIZE(aQueueProp), 0, 0, DllPresent },` |
|     - |  7094 | `		{ "SplStack", "SplDoublyLinkedList", 0, 0,` |
|     - |  7095 | `		  0, 0, 0, 0,` |
|     - |  7096 | `		  aStackProp, SX_ARRAYSIZE(aStackProp), 0, 0, DllPresent },` |
|     - |  7097 | `	};` |
|  8450 |  7098 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|     5 |  7099 | `}` |
|     - |  7100 | `/*` |
|     - |  7101 | ` * ---------------------------------------------------------------------------` |
|     - |  7102 | ` * SplHeap, SplMinHeap, SplMaxHeap and SplPriorityQueue.` |
|     - |  7103 | ` *` |
|     - |  7104 | `` * php's `spl_heap_object` is an array plus a FLAGS word, and the flags carry the`` |
|     - |  7105 | ` * thing the chunk could not express at all: **SPL_HEAP_CORRUPTED**. php sets it` |
|     - |  7106 | ` * when an exception escapes the user's compare() mid-sift -- the heap invariant is` |
|     - |  7107 | ` * then unknown -- and every operation that DEPENDS on the invariant refuses with` |
|     - |  7108 | ` * "Heap is corrupted, heap properties are no longer ensured." until` |
|     - |  7109 | `` * recoverFromCorruption() clears it. The chunk hardcoded `isCorrupted()` to false`` |
|     - |  7110 | `` * and `recoverFromCorruption()` to true, so a throwing comparator left a silently`` |
|     - |  7111 | ` * mis-ordered heap that kept answering.` |
|     - |  7112 | ` *` |
|     - |  7113 | ` * Which operations refuse is not guessable and was mapped against the oracle:` |
|     - |  7114 | ` * insert, extract, top, next and __serialize/__unserialize refuse; count,` |
|     - |  7115 | ` * isEmpty, rewind, valid, current, key, isCorrupted, recoverFromCorruption and` |
|     - |  7116 | `` * __debugInfo all keep working. (A `foreach` refuses because it reaches next().)`` |
|     - |  7117 | ` *` |
|     - |  7118 | ` * php's priority-queue node is exactly {data, priority} -- the chunk carried a` |
|     - |  7119 | `` * third field, a descending `__serial` it never compared with, which leaked into`` |
|     - |  7120 | ` * serialize(), var_dump() and the (array) cast as a nonsense PHP_INT_MAX-relative` |
|     - |  7121 | ` * integer. It is gone; equal priorities keep the order php's strictly-greater` |
|     - |  7122 | ` * swap gives them.` |
|     - |  7123 | ` *` |
|     - |  7124 | ` * The other three the chunk lacked, the same three the SplDoublyLinkedList` |
|     - |  7125 | `` * conversion lacked: `__debugInfo()` (flags / isCorrupted / heap, and for the queue`` |
|     - |  7126 | ` * the heap entries are rendered EXTR_BOTH-style whatever the extract flags say),` |
|     - |  7127 | `` * and the `__serialize()`/`__unserialize()` pair, whose payload is`` |
|     - |  7128 | ` * [members, {flags, heap_elements}] and whose reader VALIDATES -- a plain heap` |
|     - |  7129 | ` * refuses a non-zero flags word, the queue refuses a zero one.` |
|     - |  7130 | ` */` |
|     - |  7131 | `#define HP_H  "__h"   /* the heap array, in heap order */` |
|     - |  7132 | `#define HP_FL "__fl"  /* php's intern->flags: the queue's EXTR bits, 0 for a heap */` |
|     - |  7133 | `#define HP_CR "__cr"  /* php's SPL_HEAP_CORRUPTED */` |
|     - |  7134 |  |
|     - |  7135 | `#define PQ_EXTR_DATA     1` |
|     - |  7136 | `#define PQ_EXTR_PRIORITY 2` |
|     - |  7137 | `#define PQ_EXTR_BOTH     3` |
|     - |  7138 | `#define PQ_EXTR_MASK     3` |
|     - |  7139 |  |
|  1918 |  7140 | `static ph7_value * HeapSlot(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  7141 | `{` |
|  1919 |  7142 | `	ph7_value *pSlot = pThis ? PH7_NativeAttr(pThis,HP_H) : 0;` |
|  1919 |  7143 | `	if( pSlot == 0 ){` |
|   ! 0 |  7144 | `		return 0;` |
|     - |  7145 | `	}` |
|  1919 |  7146 | `	if( (pSlot->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    93 |  7147 | `		if( PH7_MemObjToHashmap(pSlot) != SXRET_OK ){` |
|   ! 0 |  7148 | `			return 0;` |
|     - |  7149 | `		}` |
|    46 |  7150 | `	}` |
|  1919 |  7151 | `	if( PH7_HashmapCowSeparate(pVm,pSlot) == 0 ){` |
|   ! 0 |  7152 | `		return 0;` |
|     - |  7153 | `	}` |
|     - |  7154 | `	/* Re-ask: the separation can MOVE the memobj pool this slot lives in` |
|     - |  7155 | `	 * (SplStoreSlot explains it). */` |
|  1919 |  7156 | `	return PH7_NativeAttr(pThis,HP_H);` |
|   960 |  7157 | `}` |
|  1880 |  7158 | `static ph7_hashmap * HeapMap(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  7159 | `{` |
|  1881 |  7160 | `	ph7_value *pSlot = HeapSlot(pVm,pThis);` |
|  1881 |  7161 | `	return pSlot ? (ph7_hashmap *)pSlot->x.pOther : 0;` |
|     1 |  7162 | `}` |
|   592 |  7163 | `static sxi64 HeapCount(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  7164 | `{` |
|   593 |  7165 | `	ph7_hashmap *pMap = HeapMap(pVm,pThis);` |
|   593 |  7166 | `	return pMap ? (sxi64)pMap->nEntry : 0;` |
|     1 |  7167 | `}` |
|     - |  7168 | `/* Re-resolved on every use: any call into the user's compare() may have rewritten` |
|     - |  7169 | ` * the heap's own storage under us (rule 47). It also used to move the pool, which` |
|     - |  7170 | ` * P1's fixed segments took care of. */` |
|   754 |  7171 | `static ph7_value * HeapAt(ph7_vm *pVm,ph7_class_instance *pThis,sxi64 i)` |
|     1 |  7172 | `{` |
|   755 |  7173 | `	ph7_hashmap *pMap = HeapMap(pVm,pThis);` |
|   755 |  7174 | `	ph7_hashmap_node *pNode = 0;` |
|   755 |  7175 | `	if( pMap == 0 \|\| HashmapLookupIntKey(pMap,i,&pNode) != SXRET_OK ){` |
|   ! 0 |  7176 | `		return 0;` |
|     - |  7177 | `	}` |
|   755 |  7178 | `	return HashmapExtractNodeValue(pNode);` |
|   378 |  7179 | `}` |
|   224 |  7180 | `static void HeapPut(ph7_vm *pVm,ph7_class_instance *pThis,sxi64 i,ph7_value *pVal)` |
|     1 |  7181 | `{` |
|   225 |  7182 | `	ph7_hashmap *pMap = HeapMap(pVm,pThis);` |
|     - |  7183 | `	ph7_value sKey;` |
|   225 |  7184 | `	if( pMap == 0 ){` |
|   ! 0 |  7185 | `		return;` |
|     - |  7186 | `	}` |
|   225 |  7187 | `	PH7_MemObjInitFromInt(pVm,&sKey,i);` |
|   225 |  7188 | `	PH7_HashmapInsert(pMap,&sKey,pVal);` |
|   225 |  7189 | `	PH7_MemObjRelease(&sKey);` |
|   113 |  7190 | `}` |
|    82 |  7191 | `static void HeapSwap(ph7_vm *pVm,ph7_class_instance *pThis,sxi64 i,sxi64 j)` |
|     1 |  7192 | `{` |
|     - |  7193 | `	ph7_value sI,sJ,*pV;` |
|    83 |  7194 | `	PH7_MemObjInit(pVm,&sI);` |
|    83 |  7195 | `	PH7_MemObjInit(pVm,&sJ);` |
|    83 |  7196 | `	pV = HeapAt(pVm,pThis,i);` |
|    83 |  7197 | `	if( pV ){` |
|    83 |  7198 | `		PH7_MemObjStore(pV,&sI);` |
|    41 |  7199 | `	}` |
|    83 |  7200 | `	pV = HeapAt(pVm,pThis,j);` |
|    83 |  7201 | `	if( pV ){` |
|    83 |  7202 | `		PH7_MemObjStore(pV,&sJ);` |
|    41 |  7203 | `	}` |
|    83 |  7204 | `	HeapPut(pVm,pThis,i,&sJ);` |
|    83 |  7205 | `	HeapPut(pVm,pThis,j,&sI);` |
|    83 |  7206 | `	PH7_MemObjRelease(&sI);` |
|    83 |  7207 | `	PH7_MemObjRelease(&sJ);` |
|    83 |  7208 | `}` |
|   416 |  7209 | `static int HeapCorrupted(ph7_class_instance *pThis)` |
|     1 |  7210 | `{` |
|   417 |  7211 | `	return pThis ? (int)PH7_NativeAttrInt(pThis,HP_CR) : 0;` |
|     1 |  7212 | `}` |
|     - |  7213 | `/*` |
|     - |  7214 | ` * php's spl_heap_consistency_validations. Only the operations that DEPEND on the` |
|     - |  7215 | ` * heap invariant call it -- count()/current()/key() answer from the array and are` |
|     - |  7216 | ` * left alone, which is why a corrupted heap still reports its size.` |
|     - |  7217 | ` */` |
|   390 |  7218 | `static sxi32 HeapCheck(ph7_context *pCtx)` |
|     1 |  7219 | `{` |
|   391 |  7220 | `	if( !HeapCorrupted(PH7_ContextThis(pCtx)) ){` |
|   379 |  7221 | `		return SXRET_OK;` |
|     - |  7222 | `	}` |
|    13 |  7223 | `	return PH7_VmThrowException(pCtx,"RuntimeException",` |
|     - |  7224 | `		"Heap is corrupted, heap properties are no longer ensured.");` |
|   196 |  7225 | `}` |
|     - |  7226 | `/* A priority-queue node is php's {data, priority}: nothing else, and in that order. */` |
|   560 |  7227 | `static int HeapIsPq(ph7_class_instance *pThis)` |
|     1 |  7228 | `{` |
|     - |  7229 | `	ph7_class *pPq;` |
|   561 |  7230 | `	if( pThis == 0 ){` |
|   ! 0 |  7231 | `		return FALSE;` |
|     - |  7232 | `	}` |
|   561 |  7233 | `	pPq = PH7_VmExtractClass(pThis->pVm,"SplPriorityQueue",sizeof("SplPriorityQueue")-1,FALSE,0);` |
|   561 |  7234 | `	return pPq && PH7_VmInstanceOf(pThis->pClass,pPq);` |
|   281 |  7235 | `}` |
|   168 |  7236 | `static ph7_value * HeapNodePart(ph7_value *pNode,const char *zKey)` |
|     1 |  7237 | `{` |
|     - |  7238 | `	ph7_hashmap *pMap;` |
|   169 |  7239 | `	ph7_hashmap_node *pEnt = 0;` |
|   169 |  7240 | `	if( pNode == 0 \|\| (pNode->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|   ! 0 |  7241 | `		return 0;` |
|     - |  7242 | `	}` |
|   169 |  7243 | `	pMap = (ph7_hashmap *)pNode->x.pOther;` |
|   169 |  7244 | `	if( HashmapLookupBlobKey(pMap,zKey,(sxu32)SyStrlen(zKey),&pEnt) != SXRET_OK ){` |
|   ! 0 |  7245 | `		return 0;` |
|     - |  7246 | `	}` |
|   169 |  7247 | `	return HashmapExtractNodeValue(pEnt);` |
|    85 |  7248 | `}` |
|    76 |  7249 | `static sxi32 HeapMakeNode(ph7_vm *pVm,ph7_value *pData,ph7_value *pPrio,ph7_value *pOut)` |
|     1 |  7250 | `{` |
|     - |  7251 | `	ph7_value sKey;` |
|    77 |  7252 | `	if( PH7_MemObjToHashmap(pOut) != SXRET_OK ){` |
|   ! 0 |  7253 | `		return SXERR_MEM;` |
|     - |  7254 | `	}` |
|    77 |  7255 | `	PH7_MemObjInitFromString(pVm,&sKey,0);` |
|    77 |  7256 | `	PH7_MemObjStringAppend(&sKey,"data",sizeof("data")-1);` |
|    77 |  7257 | `	ph7_array_add_elem(pOut,&sKey,pData);` |
|    77 |  7258 | `	PH7_MemObjRelease(&sKey);` |
|    77 |  7259 | `	PH7_MemObjInitFromString(pVm,&sKey,0);` |
|    77 |  7260 | `	PH7_MemObjStringAppend(&sKey,"priority",sizeof("priority")-1);` |
|    77 |  7261 | `	ph7_array_add_elem(pOut,&sKey,pPrio);` |
|    77 |  7262 | `	PH7_MemObjRelease(&sKey);` |
|    77 |  7263 | `	return SXRET_OK;` |
|    39 |  7264 | `}` |
|     - |  7265 | `/*` |
|     - |  7266 | ` * Run the user's compare(). For a queue php compares the PRIORITIES, so the node's` |
|     - |  7267 | `` * `priority` is what is handed over. A throw here is php's corruption trigger: the`` |
|     - |  7268 | ` * bit is set, and the throw still propagates.` |
|     - |  7269 | ` */` |
|   202 |  7270 | `static sxi32 HeapCompare(ph7_context *pCtx,sxi64 iA,sxi64 iB,int *piCmp)` |
|     1 |  7271 | `{` |
|   203 |  7272 | `	ph7_vm *pVm = pCtx->pVm;` |
|   203 |  7273 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  7274 | `	ph7_class_method *pMethod;` |
|     - |  7275 | `	ph7_value sA,sB,sRes,*apArg[2],*pV;` |
|   203 |  7276 | `	int bPq = HeapIsPq(pThis);` |
|     - |  7277 | `	sxi32 rc;` |
|   203 |  7278 | `	*piCmp = 0;` |
|   203 |  7279 | `	pMethod = pThis ? PH7_ClassExtractMethod(pThis->pClass,"compare",sizeof("compare")-1) : 0;` |
|   203 |  7280 | `	if( pMethod == 0 ){` |
|   ! 0 |  7281 | `		return SXRET_OK;` |
|     - |  7282 | `	}` |
|   203 |  7283 | `	PH7_MemObjInit(pVm,&sA);` |
|   203 |  7284 | `	PH7_MemObjInit(pVm,&sB);` |
|   203 |  7285 | `	pV = HeapAt(pVm,pThis,iA);` |
|   203 |  7286 | `	if( bPq ){` |
|    59 |  7287 | `		pV = HeapNodePart(pV,"priority");` |
|    29 |  7288 | `	}` |
|   203 |  7289 | `	if( pV ){` |
|   203 |  7290 | `		PH7_MemObjStore(pV,&sA);` |
|   101 |  7291 | `	}` |
|   203 |  7292 | `	pV = HeapAt(pVm,pThis,iB);` |
|   203 |  7293 | `	if( bPq ){` |
|    59 |  7294 | `		pV = HeapNodePart(pV,"priority");` |
|    29 |  7295 | `	}` |
|   203 |  7296 | `	if( pV ){` |
|   203 |  7297 | `		PH7_MemObjStore(pV,&sB);` |
|   101 |  7298 | `	}` |
|   203 |  7299 | `	apArg[0] = &sA;` |
|   203 |  7300 | `	apArg[1] = &sB;` |
|   203 |  7301 | `	PH7_MemObjInit(pVm,&sRes);` |
|     - |  7302 | `	/* php dispatches through its cached fptr_cmp and never consults visibility --` |
|     - |  7303 | `	 * SplHeap::compare() is PROTECTED and is meant to be called by the heap. */` |
|   203 |  7304 | `	rc = PH7_VmCallMethodUnchecked(pVm,pThis,pMethod,&sRes,2,apArg);` |
|   203 |  7305 | `	if( rc == SXRET_OK ){` |
|   173 |  7306 | `		sxi64 iVal = ph7_value_to_int64(&sRes);` |
|   173 |  7307 | `		*piCmp = iVal < 0 ? -1 : (iVal > 0 ? 1 : 0);` |
|    87 |  7308 | `	}else{` |
|     - |  7309 | `		/* php finishes the sift with the exception in flight and marks the heap` |
|     - |  7310 | `		 * CORRUPTED afterwards; the element it was placing still lands. */` |
|    31 |  7311 | `		PH7_NativeSetAttrInt(pVm,pThis,HP_CR,1);` |
|     - |  7312 | `	}` |
|   203 |  7313 | `	PH7_MemObjRelease(&sA);` |
|   203 |  7314 | `	PH7_MemObjRelease(&sB);` |
|   203 |  7315 | `	PH7_MemObjRelease(&sRes);` |
|   203 |  7316 | `	return rc;` |
|   102 |  7317 | `}` |
|   226 |  7318 | `static sxi32 HeapSiftUp(ph7_context *pCtx,sxi64 i)` |
|     1 |  7319 | `{` |
|   227 |  7320 | `	ph7_vm *pVm = pCtx->pVm;` |
|   227 |  7321 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   297 |  7322 | `	while( i > 0 ){` |
|   149 |  7323 | `		sxi64 p = (i - 1) / 2;` |
|   149 |  7324 | `		int iCmp = 0;` |
|   149 |  7325 | `		sxi32 rc = HeapCompare(pCtx,i,p,&iCmp);` |
|   149 |  7326 | `		if( rc != SXRET_OK ){` |
|    31 |  7327 | `			return rc;` |
|     - |  7328 | `		}` |
|   119 |  7329 | `		if( iCmp <= 0 ){` |
|    49 |  7330 | `			break;` |
|     - |  7331 | `		}` |
|    71 |  7332 | `		HeapSwap(pVm,pThis,i,p);` |
|    71 |  7333 | `		i = p;` |
|     1 |  7334 | `	}` |
|   197 |  7335 | `	return SXRET_OK;` |
|   114 |  7336 | `}` |
|    60 |  7337 | `static sxi32 HeapSiftDown(ph7_context *pCtx,sxi64 i)` |
|     1 |  7338 | `{` |
|    61 |  7339 | `	ph7_vm *pVm = pCtx->pVm;` |
|    61 |  7340 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    42 |  7341 | `	for(;;){` |
|    73 |  7342 | `		sxi64 n = HeapCount(pVm,pThis);` |
|    73 |  7343 | `		sxi64 l = 2*i + 1, r = l + 1, b = i;` |
|    73 |  7344 | `		int iCmp = 0;` |
|     - |  7345 | `		sxi32 rc;` |
|    73 |  7346 | `		if( l < n ){` |
|    43 |  7347 | `			rc = HeapCompare(pCtx,l,b,&iCmp);` |
|    43 |  7348 | `			if( rc != SXRET_OK ){` |
|   ! 0 |  7349 | `				return rc;` |
|     - |  7350 | `			}` |
|    43 |  7351 | `			if( iCmp > 0 ){` |
|    13 |  7352 | `				b = l;` |
|     6 |  7353 | `			}` |
|    21 |  7354 | `		}` |
|    73 |  7355 | `		if( r < n ){` |
|    13 |  7356 | `			rc = HeapCompare(pCtx,r,b,&iCmp);` |
|    13 |  7357 | `			if( rc != SXRET_OK ){` |
|   ! 0 |  7358 | `				return rc;` |
|     - |  7359 | `			}` |
|    13 |  7360 | `			if( iCmp > 0 ){` |
|     3 |  7361 | `				b = r;` |
|     1 |  7362 | `			}` |
|     6 |  7363 | `		}` |
|    73 |  7364 | `		if( b == i ){` |
|    61 |  7365 | `			break;` |
|     - |  7366 | `		}` |
|    13 |  7367 | `		HeapSwap(pVm,pThis,i,b);` |
|    13 |  7368 | `		i = b;` |
|     1 |  7369 | `	}` |
|    61 |  7370 | `	return SXRET_OK;` |
|    31 |  7371 | `}` |
|     - |  7372 | `/* php's spl_pqueue_extract_helper: BOTH wins over either single bit. */` |
|    46 |  7373 | `static void HeapPqShape(ph7_context *pCtx,ph7_value *pNode)` |
|     1 |  7374 | `{` |
|    47 |  7375 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    47 |  7376 | `	int iFlags = pThis ? (int)PH7_NativeAttrInt(pThis,HP_FL) : PQ_EXTR_DATA;` |
|     - |  7377 | `	ph7_value *pPart;` |
|    47 |  7378 | `	if( (iFlags & PQ_EXTR_BOTH) == PQ_EXTR_BOTH ){` |
|     7 |  7379 | `		ph7_result_value(pCtx,pNode);` |
|     7 |  7380 | `		return;` |
|     - |  7381 | `	}` |
|    41 |  7382 | `	pPart = HeapNodePart(pNode,(iFlags & PQ_EXTR_DATA) ? "data" : "priority");` |
|    41 |  7383 | `	if( pPart ){` |
|    41 |  7384 | `		ph7_result_value(pCtx,pPart);` |
|    21 |  7385 | `	}else{` |
|   ! 0 |  7386 | `		ph7_result_null(pCtx);` |
|     - |  7387 | `	}` |
|    24 |  7388 | `}` |
|     - |  7389 | `/* Hand back element 0 the way this class presents it. */` |
|   126 |  7390 | `static void HeapResultTop(ph7_context *pCtx,ph7_value *pNode)` |
|     1 |  7391 | `{` |
|   127 |  7392 | `	if( HeapIsPq(PH7_ContextThis(pCtx)) ){` |
|    47 |  7393 | `		HeapPqShape(pCtx,pNode);` |
|    24 |  7394 | `	}else{` |
|    81 |  7395 | `		ph7_result_value(pCtx,pNode);` |
|     - |  7396 | `	}` |
|   127 |  7397 | `}` |
|   228 |  7398 | `static int vm_builtin_SplHeap_insert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7399 | `{` |
|   229 |  7400 | `	ph7_vm *pVm = pCtx->pVm;` |
|   229 |  7401 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  7402 | `	ph7_hashmap *pMap;` |
|   229 |  7403 | `	sxi32 rc = HeapCheck(pCtx);` |
|   229 |  7404 | `	if( rc != SXRET_OK \|\| nArg < 1 ){` |
|     3 |  7405 | `		return rc;` |
|     - |  7406 | `	}` |
|   227 |  7407 | `	pMap = HeapMap(pVm,pThis);` |
|   227 |  7408 | `	if( pMap == 0 ){` |
|   ! 0 |  7409 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  7410 | `	}` |
|   227 |  7411 | `	if( HeapIsPq(pThis) ){` |
|     - |  7412 | `		ph7_value sNode;` |
|    77 |  7413 | `		if( nArg < 2 ){` |
|   ! 0 |  7414 | `			return PH7_OK;` |
|     - |  7415 | `		}` |
|    77 |  7416 | `		PH7_MemObjInit(pVm,&sNode);` |
|    77 |  7417 | `		if( HeapMakeNode(pVm,apArg[0],apArg[1],&sNode) != SXRET_OK ){` |
|   ! 0 |  7418 | `			PH7_MemObjRelease(&sNode);` |
|   ! 0 |  7419 | `			return PH7_ContextMemoryError(pCtx);` |
|     - |  7420 | `		}` |
|    77 |  7421 | `		PH7_HashmapInsert(pMap,0,&sNode);` |
|    77 |  7422 | `		PH7_MemObjRelease(&sNode);` |
|    39 |  7423 | `	}else{` |
|   151 |  7424 | `		PH7_HashmapInsert(pMap,0,apArg[0]);` |
|     - |  7425 | `	}` |
|   227 |  7426 | `	rc = HeapSiftUp(pCtx,HeapCount(pVm,pThis)-1);` |
|   227 |  7427 | `	if( rc != SXRET_OK ){` |
|    31 |  7428 | `		return rc;` |
|     - |  7429 | `	}` |
|   197 |  7430 | ``	ph7_result_bool(pCtx,1);   /* php's `true` return type */`` |
|   197 |  7431 | `	return PH7_OK;` |
|   115 |  7432 | `}` |
|    90 |  7433 | `static int vm_builtin_SplHeap_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7434 | `{` |
|    91 |  7435 | `	ph7_vm *pVm = pCtx->pVm;` |
|    91 |  7436 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  7437 | `	sxi64 n;` |
|     - |  7438 | `	ph7_value sTop,*pV;` |
|    91 |  7439 | `	sxi32 rc = HeapCheck(pCtx);` |
|    45 |  7440 | `	SXUNUSED(nArg);` |
|    45 |  7441 | `	SXUNUSED(apArg);` |
|    91 |  7442 | `	if( rc != SXRET_OK ){` |
|     3 |  7443 | `		return rc;` |
|     - |  7444 | `	}` |
|    89 |  7445 | `	n = HeapCount(pVm,pThis);` |
|    89 |  7446 | `	if( n == 0 ){` |
|     5 |  7447 | `		return PH7_VmThrowException(pCtx,"RuntimeException","Can't extract from an empty heap");` |
|     - |  7448 | `	}` |
|    85 |  7449 | `	PH7_MemObjInit(pVm,&sTop);` |
|    85 |  7450 | `	pV = HeapAt(pVm,pThis,0);` |
|    85 |  7451 | `	if( pV ){` |
|    85 |  7452 | `		PH7_MemObjStore(pV,&sTop);` |
|    42 |  7453 | `	}` |
|    85 |  7454 | `	if( n > 1 ){` |
|     - |  7455 | `		ph7_value sLast;` |
|    61 |  7456 | `		PH7_MemObjInit(pVm,&sLast);` |
|    61 |  7457 | `		pV = HeapAt(pVm,pThis,n-1);` |
|    61 |  7458 | `		if( pV ){` |
|    61 |  7459 | `			PH7_MemObjStore(pV,&sLast);` |
|    30 |  7460 | `		}` |
|    61 |  7461 | `		HeapPut(pVm,pThis,0,&sLast);` |
|    61 |  7462 | `		PH7_MemObjRelease(&sLast);` |
|    30 |  7463 | `	}` |
|     - |  7464 | `	{` |
|    85 |  7465 | `		ph7_hashmap *pMap = HeapMap(pVm,pThis);` |
|    85 |  7466 | `		ph7_hashmap_node *pNode = 0;` |
|    85 |  7467 | `		if( pMap && HashmapLookupIntKey(pMap,n-1,&pNode) == SXRET_OK ){` |
|    85 |  7468 | `			PH7_HashmapUnlinkNode(pNode,TRUE);` |
|    42 |  7469 | `		}` |
|     - |  7470 | `	}` |
|    85 |  7471 | `	if( n > 1 ){` |
|    61 |  7472 | `		rc = HeapSiftDown(pCtx,0);` |
|    61 |  7473 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  7474 | `			PH7_MemObjRelease(&sTop);` |
|   ! 0 |  7475 | `			return rc;` |
|     - |  7476 | `		}` |
|    30 |  7477 | `	}` |
|    85 |  7478 | `	HeapResultTop(pCtx,&sTop);` |
|    85 |  7479 | `	PH7_MemObjRelease(&sTop);` |
|    85 |  7480 | `	return PH7_OK;` |
|    46 |  7481 | `}` |
|    12 |  7482 | `static int vm_builtin_SplHeap_top(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7483 | `{` |
|    13 |  7484 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  7485 | `	ph7_value *pV;` |
|    13 |  7486 | `	sxi32 rc = HeapCheck(pCtx);` |
|     6 |  7487 | `	SXUNUSED(nArg);` |
|     6 |  7488 | `	SXUNUSED(apArg);` |
|    13 |  7489 | `	if( rc != SXRET_OK ){` |
|     3 |  7490 | `		return rc;` |
|     - |  7491 | `	}` |
|    11 |  7492 | `	if( HeapCount(pCtx->pVm,pThis) == 0 ){` |
|     3 |  7493 | `		return PH7_VmThrowException(pCtx,"RuntimeException","Can't peek at an empty heap");` |
|     - |  7494 | `	}` |
|     9 |  7495 | `	pV = HeapAt(pCtx->pVm,pThis,0);` |
|     9 |  7496 | `	if( pV ){` |
|     9 |  7497 | `		HeapResultTop(pCtx,pV);` |
|     4 |  7498 | `	}` |
|     9 |  7499 | `	return PH7_OK;` |
|     7 |  7500 | `}` |
|    22 |  7501 | `static int vm_builtin_SplHeap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7502 | `{` |
|    11 |  7503 | `	SXUNUSED(nArg);` |
|    11 |  7504 | `	SXUNUSED(apArg);` |
|    23 |  7505 | `	ph7_result_int64(pCtx,HeapCount(pCtx->pVm,PH7_ContextThis(pCtx)));` |
|    23 |  7506 | `	return PH7_OK;` |
|     1 |  7507 | `}` |
|    50 |  7508 | `static int vm_builtin_SplHeap_isEmpty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7509 | `{` |
|    25 |  7510 | `	SXUNUSED(nArg);` |
|    25 |  7511 | `	SXUNUSED(apArg);` |
|    51 |  7512 | `	ph7_result_bool(pCtx,HeapCount(pCtx->pVm,PH7_ContextThis(pCtx)) == 0);` |
|    51 |  7513 | `	return PH7_OK;` |
|     1 |  7514 | `}` |
|    12 |  7515 | `static int vm_builtin_SplHeap_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7516 | `{` |
|     6 |  7517 | `	SXUNUSED(nArg);` |
|     6 |  7518 | `	SXUNUSED(apArg);` |
|     6 |  7519 | `	SXUNUSED(pCtx);` |
|    13 |  7520 | `	return PH7_OK;   /* php's rewind is a no-op: a heap is walked by extraction */` |
|     1 |  7521 | `}` |
|    44 |  7522 | `static int vm_builtin_SplHeap_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7523 | `{` |
|    22 |  7524 | `	SXUNUSED(nArg);` |
|    22 |  7525 | `	SXUNUSED(apArg);` |
|    45 |  7526 | `	ph7_result_bool(pCtx,HeapCount(pCtx->pVm,PH7_ContextThis(pCtx)) > 0);` |
|    45 |  7527 | `	return PH7_OK;` |
|     1 |  7528 | `}` |
|    34 |  7529 | `static int vm_builtin_SplHeap_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7530 | `{` |
|    35 |  7531 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  7532 | `	ph7_value *pV;` |
|    17 |  7533 | `	SXUNUSED(nArg);` |
|    17 |  7534 | `	SXUNUSED(apArg);` |
|    35 |  7535 | `	if( HeapCount(pCtx->pVm,pThis) == 0 ){` |
|   ! 0 |  7536 | `		ph7_result_null(pCtx);` |
|   ! 0 |  7537 | `		return PH7_OK;` |
|     - |  7538 | `	}` |
|    35 |  7539 | `	pV = HeapAt(pCtx->pVm,pThis,0);` |
|    35 |  7540 | `	if( pV ){` |
|    35 |  7541 | `		HeapResultTop(pCtx,pV);` |
|    17 |  7542 | `	}` |
|    35 |  7543 | `	return PH7_OK;` |
|    18 |  7544 | `}` |
|    16 |  7545 | `static int vm_builtin_SplHeap_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7546 | `{` |
|     8 |  7547 | `	SXUNUSED(nArg);` |
|     8 |  7548 | `	SXUNUSED(apArg);` |
|    17 |  7549 | `	ph7_result_int64(pCtx,HeapCount(pCtx->pVm,PH7_ContextThis(pCtx))-1);` |
|    17 |  7550 | `	return PH7_OK;` |
|     1 |  7551 | `}` |
|    34 |  7552 | `static int vm_builtin_SplHeap_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7553 | `{` |
|    35 |  7554 | `	sxi32 rc = HeapCheck(pCtx);` |
|    35 |  7555 | `	if( rc != SXRET_OK ){` |
|     5 |  7556 | `		return rc;` |
|     - |  7557 | `	}` |
|    31 |  7558 | `	if( HeapCount(pCtx->pVm,PH7_ContextThis(pCtx)) == 0 ){` |
|   ! 0 |  7559 | `		return PH7_OK;` |
|     - |  7560 | `	}` |
|    31 |  7561 | `	rc = vm_builtin_SplHeap_extract(pCtx,nArg,apArg);` |
|    31 |  7562 | `	ph7_result_null(pCtx);   /* php's next() is void; the extracted value is dropped */` |
|    31 |  7563 | `	return rc;` |
|    18 |  7564 | `}` |
|     6 |  7565 | `static int vm_builtin_SplHeap_isCorrupted(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7566 | `{` |
|     3 |  7567 | `	SXUNUSED(nArg);` |
|     3 |  7568 | `	SXUNUSED(apArg);` |
|     7 |  7569 | `	ph7_result_bool(pCtx,HeapCorrupted(PH7_ContextThis(pCtx)));` |
|     7 |  7570 | `	return PH7_OK;` |
|     1 |  7571 | `}` |
|     4 |  7572 | `static int vm_builtin_SplHeap_recover(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7573 | `{` |
|     2 |  7574 | `	SXUNUSED(nArg);` |
|     2 |  7575 | `	SXUNUSED(apArg);` |
|     5 |  7576 | `	PH7_NativeSetAttrInt(pCtx->pVm,PH7_ContextThis(pCtx),HP_CR,0);` |
|     5 |  7577 | ``	ph7_result_bool(pCtx,1);   /* php's `true` return type */`` |
|     5 |  7578 | `	return PH7_OK;` |
|     1 |  7579 | `}` |
|    32 |  7580 | `static int vm_builtin_SplMinHeap_compare(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7581 | `{` |
|     - |  7582 | `	/* php: $value2 <=> $value1 — the SMALLEST value sits on top. */` |
|    33 |  7583 | `	if( nArg < 2 ){` |
|   ! 0 |  7584 | `		return PH7_OK;` |
|     - |  7585 | `	}` |
|    33 |  7586 | `	ph7_result_int64(pCtx,(ph7_int64)PH7_MemObjCmp(apArg[1],apArg[0],FALSE,0));` |
|    33 |  7587 | `	return PH7_OK;` |
|    17 |  7588 | `}` |
|    48 |  7589 | `static int vm_builtin_SplMaxHeap_compare(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7590 | `{` |
|    49 |  7591 | `	if( nArg < 2 ){` |
|   ! 0 |  7592 | `		return PH7_OK;` |
|     - |  7593 | `	}` |
|    49 |  7594 | `	ph7_result_int64(pCtx,(ph7_int64)PH7_MemObjCmp(apArg[0],apArg[1],FALSE,0));` |
|    49 |  7595 | `	return PH7_OK;` |
|    25 |  7596 | `}` |
|    58 |  7597 | `static int vm_builtin_SplPq_compare(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7598 | `{` |
|    59 |  7599 | `	if( nArg < 2 ){` |
|   ! 0 |  7600 | `		return PH7_OK;` |
|     - |  7601 | `	}` |
|    59 |  7602 | `	ph7_result_int64(pCtx,(ph7_int64)PH7_MemObjCmp(apArg[0],apArg[1],FALSE,0));` |
|    59 |  7603 | `	return PH7_OK;` |
|    30 |  7604 | `}` |
|    14 |  7605 | `static int vm_builtin_SplPq_setExtractFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7606 | `{` |
|    15 |  7607 | `	ph7_vm *pVm = pCtx->pVm;` |
|    15 |  7608 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    15 |  7609 | `	sxi64 iFlags = 0;` |
|     - |  7610 | `	sxi32 rc;` |
|    15 |  7611 | `	if( nArg < 1 ){` |
|   ! 0 |  7612 | `		return PH7_OK;` |
|     - |  7613 | `	}` |
|    15 |  7614 | `	rc = PH7_IntArgResolve(pCtx,apArg[0],"SplPriorityQueue::setExtractFlags",1,"$flags","int",&iFlags);` |
|    15 |  7615 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  7616 | `		return rc;` |
|     - |  7617 | `	}` |
|     - |  7618 | `	/* php masks to the two bits and then REFUSES an empty selection — a nonsense` |
|     - |  7619 | `	 * value is reduced, but asking for neither half is an error. */` |
|    15 |  7620 | `	iFlags &= PQ_EXTR_MASK;` |
|    15 |  7621 | `	if( iFlags == 0 ){` |
|     3 |  7622 | `		return PH7_VmThrowException(pCtx,"RuntimeException","Must specify at least one extract flag");` |
|     - |  7623 | `	}` |
|    13 |  7624 | `	PH7_NativeSetAttrInt(pVm,pThis,HP_FL,iFlags);` |
|    13 |  7625 | `	ph7_result_int64(pCtx,(ph7_int64)iFlags);` |
|    13 |  7626 | `	return PH7_OK;` |
|     8 |  7627 | `}` |
|     6 |  7628 | `static int vm_builtin_SplPq_getExtractFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7629 | `{` |
|     3 |  7630 | `	SXUNUSED(nArg);` |
|     3 |  7631 | `	SXUNUSED(apArg);` |
|     7 |  7632 | `	ph7_result_int64(pCtx,PH7_NativeAttrInt(PH7_ContextThis(pCtx),HP_FL));` |
|     7 |  7633 | `	return PH7_OK;` |
|     1 |  7634 | `}` |
|     - |  7635 | `/*` |
|     - |  7636 | ` * php's get_debug_info: flags, isCorrupted, heap. The QUEUE renders its entries` |
|     - |  7637 | ` * EXTR_BOTH-style whatever the extract flags say, because the debug view is of the` |
|     - |  7638 | ` * STORAGE rather than of what extract() would hand back. All three keys are php's` |
|     - |  7639 | `` * mangled private names (SplRootDebugKey): `SplHeap` for SplMinHeap/SplMaxHeap and`` |
|     - |  7640 | `` * every subclass of either, `SplPriorityQueue` for the queue.`` |
|     - |  7641 | ` */` |
|    20 |  7642 | `static sxi32 HeapFillDebug(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut)` |
|     1 |  7643 | `{` |
|     - |  7644 | `	ph7_value sKey,sVal,*pStore;` |
|    21 |  7645 | `	SplRootDebugKey(pVm,pThis,&sKey,"flags");` |
|    21 |  7646 | `	PH7_MemObjInitFromInt(pVm,&sVal,PH7_NativeAttrInt(pThis,HP_FL));` |
|    21 |  7647 | `	ph7_array_add_elem(pOut,&sKey,&sVal);` |
|    21 |  7648 | `	PH7_MemObjRelease(&sKey);` |
|    21 |  7649 | `	PH7_MemObjRelease(&sVal);` |
|    21 |  7650 | `	SplRootDebugKey(pVm,pThis,&sKey,"isCorrupted");` |
|    21 |  7651 | `	PH7_MemObjInitFromBool(pVm,&sVal,HeapCorrupted(pThis));` |
|    21 |  7652 | `	ph7_array_add_elem(pOut,&sKey,&sVal);` |
|    21 |  7653 | `	PH7_MemObjRelease(&sKey);` |
|    21 |  7654 | `	PH7_MemObjRelease(&sVal);` |
|    21 |  7655 | `	SplRootDebugKey(pVm,pThis,&sKey,"heap");` |
|    21 |  7656 | `	pStore = HeapSlot(pVm,pThis);` |
|    21 |  7657 | `	if( pStore ){` |
|    21 |  7658 | `		ph7_array_add_elem(pOut,&sKey,pStore);` |
|    10 |  7659 | `	}` |
|    21 |  7660 | `	PH7_MemObjRelease(&sKey);` |
|    21 |  7661 | `	return PH7_OK;` |
|     1 |  7662 | `}` |
|    20 |  7663 | `static sxi32 HeapPresent(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)` |
|     1 |  7664 | `{` |
|    21 |  7665 | `	if( !bDebug ){` |
|     9 |  7666 | `		return PH7_OK;   /* php's (array) cast shows nothing */` |
|     - |  7667 | `	}` |
|    13 |  7668 | `	return HeapFillDebug(pVm,pThis,pOut);` |
|    11 |  7669 | `}` |
|     8 |  7670 | `static int vm_builtin_SplHeap_debugInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7671 | `{` |
|     9 |  7672 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  7673 | `	ph7_value sOut;` |
|     4 |  7674 | `	SXUNUSED(nArg);` |
|     4 |  7675 | `	SXUNUSED(apArg);` |
|     9 |  7676 | `	PH7_MemObjInit(pVm,&sOut);` |
|     9 |  7677 | `	if( PH7_MemObjToHashmap(&sOut) != SXRET_OK ){` |
|   ! 0 |  7678 | `		PH7_MemObjRelease(&sOut);` |
|   ! 0 |  7679 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  7680 | `	}` |
|     9 |  7681 | `	HeapFillDebug(pVm,PH7_ContextThis(pCtx),&sOut);` |
|     9 |  7682 | `	ph7_result_value(pCtx,&sOut);` |
|     9 |  7683 | `	PH7_MemObjRelease(&sOut);` |
|     9 |  7684 | `	return PH7_OK;` |
|     5 |  7685 | `}` |
|     - |  7686 | `/*` |
|     - |  7687 | ` * php's __serialize(): [members, {flags, heap_elements}]. Note the OUTER array is` |
|     - |  7688 | ` * a two-element list whose first entry is the instance's own property table —` |
|     - |  7689 | ` * empty for a bare heap, a SUBCLASS's declared slots when there is one.` |
|     - |  7690 | ` */` |
|    20 |  7691 | `static int vm_builtin_SplHeap_serializeMagic(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7692 | `{` |
|    21 |  7693 | `	ph7_vm *pVm = pCtx->pVm;` |
|    21 |  7694 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  7695 | `	ph7_value sOut,sMembers,sState,sKey,sVal,*pStore;` |
|    21 |  7696 | `	sxi32 rc = HeapCheck(pCtx);` |
|    10 |  7697 | `	SXUNUSED(nArg);` |
|    10 |  7698 | `	SXUNUSED(apArg);` |
|    21 |  7699 | `	if( rc != SXRET_OK ){` |
|     3 |  7700 | `		return rc;` |
|     - |  7701 | `	}` |
|    19 |  7702 | `	PH7_MemObjInit(pVm,&sOut);` |
|    19 |  7703 | `	PH7_MemObjInit(pVm,&sState);` |
|    18 |  7704 | `	if( SplMembersOf(pVm,pThis,&sMembers) != SXRET_OK` |
|    18 |  7705 | `	 \|\| PH7_MemObjToHashmap(&sOut) != SXRET_OK` |
|    19 |  7706 | `	 \|\| PH7_MemObjToHashmap(&sState) != SXRET_OK ){` |
|   ! 0 |  7707 | `		PH7_MemObjRelease(&sOut);` |
|   ! 0 |  7708 | `		PH7_MemObjRelease(&sMembers);` |
|   ! 0 |  7709 | `		PH7_MemObjRelease(&sState);` |
|   ! 0 |  7710 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  7711 | `	}` |
|    19 |  7712 | `	PH7_MemObjInitFromString(pVm,&sKey,0);` |
|    19 |  7713 | `	PH7_MemObjStringAppend(&sKey,"flags",sizeof("flags")-1);` |
|    19 |  7714 | `	PH7_MemObjInitFromInt(pVm,&sVal,PH7_NativeAttrInt(pThis,HP_FL));` |
|    19 |  7715 | `	ph7_array_add_elem(&sState,&sKey,&sVal);` |
|    19 |  7716 | `	PH7_MemObjRelease(&sKey);` |
|    19 |  7717 | `	PH7_MemObjRelease(&sVal);` |
|    19 |  7718 | `	PH7_MemObjInitFromString(pVm,&sKey,0);` |
|    19 |  7719 | `	PH7_MemObjStringAppend(&sKey,"heap_elements",sizeof("heap_elements")-1);` |
|    19 |  7720 | `	pStore = HeapSlot(pVm,pThis);` |
|    19 |  7721 | `	if( pStore ){` |
|    19 |  7722 | `		ph7_array_add_elem(&sState,&sKey,pStore);` |
|     9 |  7723 | `	}` |
|    19 |  7724 | `	PH7_MemObjRelease(&sKey);` |
|    19 |  7725 | `	ph7_array_add_elem(&sOut,0,&sMembers);` |
|    19 |  7726 | `	ph7_array_add_elem(&sOut,0,&sState);` |
|    19 |  7727 | `	ph7_result_value(pCtx,&sOut);` |
|    19 |  7728 | `	PH7_MemObjRelease(&sOut);` |
|    19 |  7729 | `	PH7_MemObjRelease(&sMembers);` |
|    19 |  7730 | `	PH7_MemObjRelease(&sState);` |
|    19 |  7731 | `	return PH7_OK;` |
|    11 |  7732 | `}` |
|   ! 0 |  7733 | `static sxi32 HeapUnserializeFail(ph7_context *pCtx)` |
|   ! 0 |  7734 | `{` |
|   ! 0 |  7735 | `	return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     - |  7736 | `		"Unexpected data found in serialization payload");` |
|   ! 0 |  7737 | `}` |
|     6 |  7738 | `static int vm_builtin_SplHeap_unserializeMagic(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7739 | `{` |
|     7 |  7740 | `	ph7_vm *pVm = pCtx->pVm;` |
|     7 |  7741 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  7742 | `	ph7_hashmap *pData;` |
|     7 |  7743 | `	ph7_hashmap_node *pNode = 0;` |
|     - |  7744 | `	ph7_value *pState,*pFlags,*pElems,*pSlot;` |
|     - |  7745 | `	sxi64 iFlags;` |
|     - |  7746 | `	int bPq;` |
|     7 |  7747 | `	sxi32 rc = HeapCheck(pCtx);` |
|     7 |  7748 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  7749 | `		return rc;` |
|     - |  7750 | `	}` |
|     7 |  7751 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_HASHMAP) == 0 \|\| pThis == 0 ){` |
|   ! 0 |  7752 | `		return HeapUnserializeFail(pCtx);` |
|     - |  7753 | `	}` |
|     7 |  7754 | `	pData = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     7 |  7755 | `	if( HashmapLookupIntKey(pData,1,&pNode) != SXRET_OK ){` |
|   ! 0 |  7756 | `		return HeapUnserializeFail(pCtx);` |
|     - |  7757 | `	}` |
|     7 |  7758 | `	pState = HashmapExtractNodeValue(pNode);` |
|     7 |  7759 | `	if( pState == 0 \|\| (pState->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|   ! 0 |  7760 | `		return HeapUnserializeFail(pCtx);` |
|     - |  7761 | `	}` |
|     7 |  7762 | `	pFlags = HeapNodePart(pState,"flags");` |
|     7 |  7763 | `	pElems = HeapNodePart(pState,"heap_elements");` |
|     6 |  7764 | `	if( pFlags == 0 \|\| (pFlags->iFlags & MEMOBJ_INT) == 0` |
|     7 |  7765 | `	 \|\| pElems == 0 \|\| (pElems->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|   ! 0 |  7766 | `		return HeapUnserializeFail(pCtx);` |
|     - |  7767 | `	}` |
|     - |  7768 | `	/* php VALIDATES the flags against the class: a plain heap has no user-visible` |
|     - |  7769 | `	 * flags at all, the queue must name at least one half to extract. */` |
|     7 |  7770 | `	iFlags = ph7_value_to_int64(pFlags);` |
|     7 |  7771 | `	bPq = HeapIsPq(pThis);` |
|     7 |  7772 | `	if( bPq ){` |
|     5 |  7773 | `		iFlags &= PQ_EXTR_MASK;` |
|     5 |  7774 | `		if( iFlags == 0 ){` |
|   ! 0 |  7775 | `			return HeapUnserializeFail(pCtx);` |
|     1 |  7776 | `		}` |
|     5 |  7777 | `	}else if( iFlags != 0 ){` |
|   ! 0 |  7778 | `		return HeapUnserializeFail(pCtx);` |
|     - |  7779 | `	}` |
|     7 |  7780 | `	PH7_NativeSetAttrInt(pVm,pThis,HP_FL,iFlags);` |
|     7 |  7781 | `	pSlot = PH7_NativeAttr(pThis,HP_H);` |
|     7 |  7782 | `	if( pSlot ){` |
|     7 |  7783 | `		PH7_MemObjRelease(pSlot);` |
|     7 |  7784 | `		PH7_MemObjStore(pElems,pSlot);` |
|     3 |  7785 | `	}` |
|     7 |  7786 | `	if( HashmapLookupIntKey(pData,0,&pNode) == SXRET_OK ){` |
|     7 |  7787 | `		SplMembersLoad(pThis,HashmapExtractNodeValue(pNode));` |
|     3 |  7788 | `	}` |
|     7 |  7789 | `	return PH7_OK;` |
|     4 |  7790 | `}` |
|     - |  7791 | `/*` |
|     - |  7792 | ` * The declaration. Method ORDER is spl_heap.stub.php's; php declares no` |
|     - |  7793 | ` * constructor for any of the four, SplHeap::compare is ABSTRACT PROTECTED (so` |
|     - |  7794 | ` * SplHeap itself cannot be instantiated) while the queue's is PUBLIC, and the` |
|     - |  7795 | ` * queue's default extract mode is EXTR_DATA, stamped as a property default the` |
|     - |  7796 | ` * way the DLL family's fix bit is.` |
|     - |  7797 | ` */` |
|  8445 |  7798 | `static sxi32 VmInstallSplHeap(ph7_vm *pVm)` |
|     5 |  7799 | `{` |
|     - |  7800 | `	static const PH7_NativePropDef aHeapProp[] = {` |
|     - |  7801 | `		{ HP_H,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  7802 | `		{ HP_FL, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  7803 | `		{ HP_CR, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  7804 | `	};` |
|     - |  7805 | `	static const PH7_NativePropDef aPqProp[] = {` |
|     - |  7806 | `		{ HP_H,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  7807 | `		{ HP_FL, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN,` |
|     - |  7808 | `		  { 0, 0, PH7_NATIVE_VAL_INT, PQ_EXTR_DATA, 0, 0.0 }, 0 },` |
|     - |  7809 | `		{ HP_CR, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  7810 | `	};` |
|     - |  7811 | `	static const PH7_NativeMethodDef aHeapMethod[] = {` |
|     - |  7812 | `		{ "extract",               PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplHeap_extract },` |
|     - |  7813 | `		{ "insert",                PH7_MOD_PUBLIC, "mixed $value", "@true",` |
|     - |  7814 | `		  vm_builtin_SplHeap_insert },` |
|     - |  7815 | `		{ "top",                   PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplHeap_top },` |
|     - |  7816 | `		{ "count",                 PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplHeap_count },` |
|     - |  7817 | `		{ "isEmpty",               PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplHeap_isEmpty },` |
|     - |  7818 | `		{ "rewind",                PH7_MOD_PUBLIC, "", "@void", vm_builtin_SplHeap_rewind },` |
|     - |  7819 | `		{ "current",               PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplHeap_current },` |
|     - |  7820 | `		{ "key",                   PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplHeap_key },` |
|     - |  7821 | `		{ "next",                  PH7_MOD_PUBLIC, "", "@void", vm_builtin_SplHeap_next },` |
|     - |  7822 | `		{ "valid",                 PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplHeap_valid },` |
|     - |  7823 | `		{ "recoverFromCorruption", PH7_MOD_PUBLIC, "", "@true", vm_builtin_SplHeap_recover },` |
|     - |  7824 | `		{ "compare",               PH7_MOD_PROTECTED\|PH7_MOD_ABSTRACT,` |
|     - |  7825 | `		  "mixed $value1, mixed $value2", "@int", 0 },` |
|     - |  7826 | `		{ "isCorrupted",           PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplHeap_isCorrupted },` |
|     - |  7827 | `		{ "__debugInfo",           PH7_MOD_PUBLIC, "", "@array", vm_builtin_SplHeap_debugInfo },` |
|     - |  7828 | `		{ "__serialize",           PH7_MOD_PUBLIC, "", "@array",` |
|     - |  7829 | `		  vm_builtin_SplHeap_serializeMagic },` |
|     - |  7830 | `		{ "__unserialize",         PH7_MOD_PUBLIC, "array $data", "@void",` |
|     - |  7831 | `		  vm_builtin_SplHeap_unserializeMagic },` |
|     - |  7832 | `	};` |
|     - |  7833 | `	static const PH7_NativeMethodDef aMinMethod[] = {` |
|     - |  7834 | `		{ "compare", PH7_MOD_PROTECTED, "mixed $value1, mixed $value2", "@int",` |
|     - |  7835 | `		  vm_builtin_SplMinHeap_compare },` |
|     - |  7836 | `	};` |
|     - |  7837 | `	static const PH7_NativeMethodDef aMaxMethod[] = {` |
|     - |  7838 | `		{ "compare", PH7_MOD_PROTECTED, "mixed $value1, mixed $value2", "@int",` |
|     - |  7839 | `		  vm_builtin_SplMaxHeap_compare },` |
|     - |  7840 | `	};` |
|     - |  7841 | `	static const PH7_NativeConstDef aPqConst[] = {` |
|     - |  7842 | `		{ "EXTR_BOTH",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PQ_EXTR_BOTH, 0, 0.0 },` |
|     - |  7843 | `		{ "EXTR_PRIORITY", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PQ_EXTR_PRIORITY, 0, 0.0 },` |
|     - |  7844 | `		{ "EXTR_DATA",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PQ_EXTR_DATA, 0, 0.0 },` |
|     - |  7845 | `	};` |
|     - |  7846 | `	static const PH7_NativeMethodDef aPqMethod[] = {` |
|     - |  7847 | `		{ "compare",               PH7_MOD_PUBLIC, "mixed $priority1, mixed $priority2", "@int",` |
|     - |  7848 | `		  vm_builtin_SplPq_compare },` |
|     - |  7849 | `		{ "insert",                PH7_MOD_PUBLIC, "mixed $value, mixed $priority", "@true",` |
|     - |  7850 | `		  vm_builtin_SplHeap_insert },` |
|     - |  7851 | `		{ "setExtractFlags",       PH7_MOD_PUBLIC, "int $flags", "@int",` |
|     - |  7852 | `		  vm_builtin_SplPq_setExtractFlags },` |
|     - |  7853 | `		{ "top",                   PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplHeap_top },` |
|     - |  7854 | `		{ "extract",               PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplHeap_extract },` |
|     - |  7855 | `		{ "count",                 PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplHeap_count },` |
|     - |  7856 | `		{ "isEmpty",               PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplHeap_isEmpty },` |
|     - |  7857 | `		{ "rewind",                PH7_MOD_PUBLIC, "", "@void", vm_builtin_SplHeap_rewind },` |
|     - |  7858 | `		{ "current",               PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplHeap_current },` |
|     - |  7859 | `		{ "key",                   PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplHeap_key },` |
|     - |  7860 | `		{ "next",                  PH7_MOD_PUBLIC, "", "@void", vm_builtin_SplHeap_next },` |
|     - |  7861 | `		{ "valid",                 PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplHeap_valid },` |
|     - |  7862 | `		{ "recoverFromCorruption", PH7_MOD_PUBLIC, "", "@true", vm_builtin_SplHeap_recover },` |
|     - |  7863 | `		{ "isCorrupted",           PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplHeap_isCorrupted },` |
|     - |  7864 | `		{ "getExtractFlags",       PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplPq_getExtractFlags },` |
|     - |  7865 | `		{ "__debugInfo",           PH7_MOD_PUBLIC, "", "@array", vm_builtin_SplHeap_debugInfo },` |
|     - |  7866 | `		{ "__serialize",           PH7_MOD_PUBLIC, "", "@array",` |
|     - |  7867 | `		  vm_builtin_SplHeap_serializeMagic },` |
|     - |  7868 | `		{ "__unserialize",         PH7_MOD_PUBLIC, "array $data", "@void",` |
|     - |  7869 | `		  vm_builtin_SplHeap_unserializeMagic },` |
|     - |  7870 | `	};` |
|     - |  7871 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - |  7872 | `		{ "SplPriorityQueue", 0, "Iterator,Countable", 0,` |
|     - |  7873 | `		  aPqMethod, SX_ARRAYSIZE(aPqMethod),` |
|     - |  7874 | `		  aPqConst, SX_ARRAYSIZE(aPqConst),` |
|     - |  7875 | `		  aPqProp, SX_ARRAYSIZE(aPqProp), 0, 0, HeapPresent },` |
|     - |  7876 | `		{ "SplHeap", 0, "Iterator,Countable", PH7_CLASS_ABSTRACT,` |
|     - |  7877 | `		  aHeapMethod, SX_ARRAYSIZE(aHeapMethod), 0, 0,` |
|     - |  7878 | `		  aHeapProp, SX_ARRAYSIZE(aHeapProp), 0, 0, HeapPresent },` |
|     - |  7879 | `		{ "SplMinHeap", "SplHeap", 0, 0,` |
|     - |  7880 | `		  aMinMethod, SX_ARRAYSIZE(aMinMethod), 0, 0, 0, 0, 0, 0, HeapPresent },` |
|     - |  7881 | `		{ "SplMaxHeap", "SplHeap", 0, 0,` |
|     - |  7882 | `		  aMaxMethod, SX_ARRAYSIZE(aMaxMethod), 0, 0, 0, 0, 0, 0, HeapPresent },` |
|     - |  7883 | `	};` |
|  8450 |  7884 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|     5 |  7885 | `}` |
|     - |  7886 | `/*` |
|     - |  7887 | ` * ---------------------------------------------------------------------------` |
|     - |  7888 | ` * SplFixedArray.` |
|     - |  7889 | ` *` |
|     - |  7890 | `` * php PRESENTS this one as its own elements: `var_dump` shows`` |
|     - |  7891 | `` * `object(SplFixedArray)#1 (3) { [0]=> … }`, the `(array)` cast yields the`` |
|     - |  7892 | `` * elements with their integer keys, and `serialize()` writes them as INTEGER`` |
|     - |  7893 | ``  * property names (`O:13:"SplFixedArray":3:{i:0;…}`) because `__serialize()` `` |
|     - |  7894 | `` * simply hands the element array back. The chunk exposed `__a`/`__n` on all`` |
|     - |  7895 | ` * three surfaces instead.` |
|     - |  7896 | ` *` |
|     - |  7897 | `` * `getIterator()` answers php's **InternalIterator**, not a Generator. The chunk`` |
|     - |  7898 | ` * yielded, which is one class name wrong on a php-visible surface and also the` |
|     - |  7899 | `` * thing rule 5's InternalIterator exists for — `pIterVtab` plus`` |
|     - |  7900 | `` * `PH7_NativeIteratorNew()` is the whole implementation, and it gets php's`` |
|     - |  7901 | ` * independent-cursor behaviour (two getIterator() calls, or nested foreach, walk` |
|     - |  7902 | ` * separately) for free.` |
|     - |  7903 | ` *` |
|     - |  7904 | ` * php's offset rule is its own: an int, a bool, an INTEGER-LIKE string and a` |
|     - |  7905 | ` * RESOURCE (which warns and becomes its id, php's engine-wide rule) are accepted,` |
|     - |  7906 | `` * everything else is `Cannot access offset of type %s on SplFixedArray`.`` |
|     - |  7907 | ` * The chunk refused bools. A FLOAT offset stays refused here, which is not php's` |
|     - |  7908 | ` * answer (php truncates, with a precision deprecation when it is lossy) but IS` |
|     - |  7909 | `` * PHL's engine-wide one — `$a[1.5]` on a plain array raises the same TypeError,`` |
|     - |  7910 | ` * so the class stays consistent with the engine it lives in rather than uniquely` |
|     - |  7911 | ` * permissive (the scope policy).` |
|     - |  7912 | ` */` |
|     - |  7913 | `#define FA_A "__a"   /* the elements, 0..n-1 */` |
|     - |  7914 | `#define FA_N "__n"   /* php's size */` |
|     - |  7915 |  |
|   886 |  7916 | `static ph7_value * FaSlot(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     3 |  7917 | `{` |
|   889 |  7918 | `	ph7_value *pSlot = pThis ? PH7_NativeAttr(pThis,FA_A) : 0;` |
|   889 |  7919 | `	if( pSlot == 0 ){` |
|   ! 0 |  7920 | `		return 0;` |
|     - |  7921 | `	}` |
|   889 |  7922 | `	if( (pSlot->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|   137 |  7923 | `		if( PH7_MemObjToHashmap(pSlot) != SXRET_OK ){` |
|   ! 0 |  7924 | `			return 0;` |
|     - |  7925 | `		}` |
|    67 |  7926 | `	}` |
|   889 |  7927 | `	if( PH7_HashmapCowSeparate(pVm,pSlot) == 0 ){` |
|   ! 0 |  7928 | `		return 0;` |
|     - |  7929 | `	}` |
|     - |  7930 | `	/* Re-ask: the separation can MOVE the memobj pool this slot lives in` |
|     - |  7931 | `	 * (SplStoreSlot explains it). */` |
|   889 |  7932 | `	return PH7_NativeAttr(pThis,FA_A);` |
|   446 |  7933 | `}` |
|   836 |  7934 | `static ph7_hashmap * FaMap(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     3 |  7935 | `{` |
|   839 |  7936 | `	ph7_value *pSlot = FaSlot(pVm,pThis);` |
|   839 |  7937 | `	return pSlot ? (ph7_hashmap *)pSlot->x.pOther : 0;` |
|     3 |  7938 | `}` |
|   504 |  7939 | `static sxi64 FaSize(ph7_class_instance *pThis)` |
|     3 |  7940 | `{` |
|   507 |  7941 | `	return pThis ? PH7_NativeAttrInt(pThis,FA_N) : 0;` |
|     3 |  7942 | `}` |
|   102 |  7943 | `static ph7_value * FaAt(ph7_vm *pVm,ph7_class_instance *pThis,sxi64 i)` |
|     2 |  7944 | `{` |
|   104 |  7945 | `	ph7_hashmap *pMap = FaMap(pVm,pThis);` |
|   104 |  7946 | `	ph7_hashmap_node *pNode = 0;` |
|   104 |  7947 | `	if( pMap == 0 \|\| HashmapLookupIntKey(pMap,i,&pNode) != SXRET_OK ){` |
|   ! 0 |  7948 | `		return 0;` |
|     - |  7949 | `	}` |
|   104 |  7950 | `	return HashmapExtractNodeValue(pNode);` |
|    53 |  7951 | `}` |
|   590 |  7952 | `static void FaPut(ph7_vm *pVm,ph7_class_instance *pThis,sxi64 i,ph7_value *pVal)` |
|     3 |  7953 | `{` |
|   593 |  7954 | `	ph7_hashmap *pMap = FaMap(pVm,pThis);` |
|     - |  7955 | `	ph7_value sKey;` |
|   593 |  7956 | `	if( pMap == 0 ){` |
|   ! 0 |  7957 | `		return;` |
|     - |  7958 | `	}` |
|   593 |  7959 | `	PH7_MemObjInitFromInt(pVm,&sKey,i);` |
|   593 |  7960 | `	PH7_HashmapInsert(pMap,&sKey,pVal);` |
|   593 |  7961 | `	PH7_MemObjRelease(&sKey);` |
|   298 |  7962 | `}` |
|     - |  7963 | `/*` |
|     - |  7964 | ` * php's offset decode. An INTEGER-LIKE string is accepted (php's own` |
|     - |  7965 | `` * `ZEND_HANDLE_NUMERIC_STRING`), a bool is its 0/1, and every other type is named`` |
|     - |  7966 | ` * in the refusal. Returns 0 and leaves a TypeError raised when it cannot decode.` |
|     - |  7967 | ` */` |
|   302 |  7968 | `static int FaOffset(ph7_context *pCtx,ph7_value *pArg,sxi64 *piOut,sxi32 *pRc)` |
|     3 |  7969 | `{` |
|   305 |  7970 | `	*pRc = PH7_OK;` |
|   305 |  7971 | `	if( pArg == 0 ){` |
|   ! 0 |  7972 | `		*piOut = 0;` |
|   ! 0 |  7973 | `		return 1;` |
|     - |  7974 | `	}` |
|   305 |  7975 | `	if( pArg->iFlags & MEMOBJ_INT ){` |
|   262 |  7976 | `		*piOut = pArg->x.iVal;` |
|   262 |  7977 | `		return 1;` |
|     - |  7978 | `	}` |
|    45 |  7979 | `	if( pArg->iFlags & MEMOBJ_BOOL ){` |
|     8 |  7980 | `		*piOut = pArg->x.iVal ? 1 : 0;` |
|     8 |  7981 | `		return 1;` |
|     - |  7982 | `	}` |
|    39 |  7983 | `	if( (pArg->iFlags & MEMOBJ_STRING) && PH7_MemObjStringIsNumeric(pArg) ){` |
|     - |  7984 | `		ph7_value sTmp;` |
|     8 |  7985 | `		PH7_MemObjInit(pCtx->pVm,&sTmp);` |
|     8 |  7986 | `		PH7_MemObjStore(pArg,&sTmp);` |
|     8 |  7987 | `		PH7_MemObjToInteger(&sTmp);` |
|     8 |  7988 | `		*piOut = sTmp.x.iVal;` |
|     8 |  7989 | `		PH7_MemObjRelease(&sTmp);` |
|     8 |  7990 | `		return 1;` |
|     - |  7991 | `	}` |
|    33 |  7992 | `	if( pArg->iFlags & MEMOBJ_RES ){` |
|     - |  7993 | `		/* php's offset rule again: a resource is not refused, it WARNS and becomes` |
|     - |  7994 | `		 * its integer id — which for a fixed array is then an ordinary out-of-range` |
|     - |  7995 | ``		 * index. The refusal below had named `resource` instead. Rewrites the`` |
|     - |  7996 | `		 * method's own argument copy, as the store's offsets do. */` |
|     9 |  7997 | `		PH7_VmOffsetResourceWarn(pCtx->pVm,pArg);` |
|     9 |  7998 | `		*piOut = pArg->x.iVal;` |
|     9 |  7999 | `		return 1;` |
|     - |  8000 | `	}` |
|    25 |  8001 | `	*piOut = 0;` |
|    25 |  8002 | `	if( pArg->iFlags & MEMOBJ_OBJ ){` |
|     - |  8003 | `		/* php names the CLASS here, as get_debug_type() does, not the word` |
|     - |  8004 | `		 * "object" — the same rule the store's offsets follow. */` |
|     6 |  8005 | `		ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|     6 |  8006 | `		SyString *pName = pInst && pInst->pClass ? &pInst->pClass->sName : 0;` |
|     8 |  8007 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|     2 |  8008 | `			"Cannot access offset of type %z on SplFixedArray",pName);` |
|     6 |  8009 | `		return 0;` |
|     - |  8010 | `	}` |
|    30 |  8011 | `	*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|     9 |  8012 | `		"Cannot access offset of type %s on SplFixedArray",ph7_type_name(pArg));` |
|    21 |  8013 | `	return 0;` |
|   154 |  8014 | `}` |
|    12 |  8015 | `static sxi32 FaOutOfBounds(ph7_context *pCtx)` |
|     2 |  8016 | `{` |
|    14 |  8017 | `	return PH7_VmThrowException(pCtx,"OutOfBoundsException","Index invalid or out of range");` |
|     2 |  8018 | `}` |
|     - |  8019 | ``/* php's setSize: grow with nulls, shrink by dropping the tail, answer `true`. */`` |
|   144 |  8020 | `static sxi32 FaResize(ph7_vm *pVm,ph7_class_instance *pThis,sxi64 nNew)` |
|     3 |  8021 | `{` |
|   147 |  8022 | `	sxi64 nOld = FaSize(pThis);` |
|   147 |  8023 | `	ph7_hashmap *pMap = FaMap(pVm,pThis);` |
|     - |  8024 | `	sxi64 i;` |
|   147 |  8025 | `	if( pMap == 0 ){` |
|   ! 0 |  8026 | `		return SXERR_MEM;` |
|     - |  8027 | `	}` |
|   157 |  8028 | `	for( i = nNew ; i < nOld ; ++i ){` |
|    11 |  8029 | `		ph7_hashmap_node *pNode = 0;` |
|    11 |  8030 | `		if( HashmapLookupIntKey(pMap,i,&pNode) == SXRET_OK ){` |
|    11 |  8031 | `			PH7_HashmapUnlinkNode(pNode,TRUE);` |
|     5 |  8032 | `		}` |
|     6 |  8033 | `	}` |
|   487 |  8034 | `	for( i = nOld ; i < nNew ; ++i ){` |
|     - |  8035 | `		ph7_value sNull;` |
|   343 |  8036 | `		PH7_MemObjInit(pVm,&sNull);` |
|   343 |  8037 | `		FaPut(pVm,pThis,i,&sNull);` |
|   343 |  8038 | `		PH7_MemObjRelease(&sNull);` |
|   173 |  8039 | `	}` |
|   147 |  8040 | `	PH7_NativeSetAttrInt(pVm,pThis,FA_N,nNew);` |
|   147 |  8041 | `	return SXRET_OK;` |
|    75 |  8042 | `}` |
|   118 |  8043 | `static int vm_builtin_SplFixedArray_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  8044 | `{` |
|   121 |  8045 | `	ph7_vm *pVm = pCtx->pVm;` |
|   121 |  8046 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   121 |  8047 | `	sxi64 nSize = 0;` |
|   121 |  8048 | `	if( pThis == 0 ){` |
|   ! 0 |  8049 | `		return PH7_OK;` |
|     - |  8050 | `	}` |
|   121 |  8051 | `	if( nArg > 0 ){` |
|   117 |  8052 | `		sxi32 rc = PH7_IntArgResolve(pCtx,apArg[0],"SplFixedArray::__construct",1,"$size","int",&nSize);` |
|   117 |  8053 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  8054 | `			return rc;` |
|     - |  8055 | `		}` |
|    57 |  8056 | `	}` |
|   121 |  8057 | `	if( nSize < 0 ){` |
|     - |  8058 | `		/* php words this from __construct(), not from the setSize() it forwards to —` |
|     - |  8059 | ``		 * which is what the chunk's `$this->setSize()` reported. */`` |
|     3 |  8060 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  8061 | `			"SplFixedArray::__construct(): Argument #1 ($size) must be greater than or equal to 0");` |
|     - |  8062 | `	}` |
|   119 |  8063 | `	FaResize(pVm,pThis,nSize);` |
|   119 |  8064 | `	return PH7_OK;` |
|    62 |  8065 | `}` |
|     6 |  8066 | `static int vm_builtin_SplFixedArray_getSize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8067 | `{` |
|     3 |  8068 | `	SXUNUSED(nArg);` |
|     3 |  8069 | `	SXUNUSED(apArg);` |
|     7 |  8070 | `	ph7_result_int64(pCtx,FaSize(PH7_ContextThis(pCtx)));` |
|     7 |  8071 | `	return PH7_OK;` |
|     1 |  8072 | `}` |
|    12 |  8073 | `static int vm_builtin_SplFixedArray_setSize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8074 | `{` |
|    13 |  8075 | `	sxi64 nSize = 0;` |
|     - |  8076 | `	sxi32 rc;` |
|    13 |  8077 | `	if( nArg < 1 ){` |
|   ! 0 |  8078 | `		return PH7_OK;` |
|     - |  8079 | `	}` |
|    13 |  8080 | `	rc = PH7_IntArgResolve(pCtx,apArg[0],"SplFixedArray::setSize",1,"$size","int",&nSize);` |
|    13 |  8081 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  8082 | `		return rc;` |
|     - |  8083 | `	}` |
|    13 |  8084 | `	if( nSize < 0 ){` |
|     3 |  8085 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  8086 | `			"SplFixedArray::setSize(): Argument #1 ($size) must be greater than or equal to 0");` |
|     - |  8087 | `	}` |
|    11 |  8088 | `	FaResize(pCtx->pVm,PH7_ContextThis(pCtx),nSize);` |
|    11 |  8089 | ``	ph7_result_bool(pCtx,1);   /* php's `true` return type */`` |
|    11 |  8090 | `	return PH7_OK;` |
|     7 |  8091 | `}` |
|    12 |  8092 | `static int vm_builtin_SplFixedArray_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8093 | `{` |
|     6 |  8094 | `	SXUNUSED(nArg);` |
|     6 |  8095 | `	SXUNUSED(apArg);` |
|    13 |  8096 | `	ph7_result_int64(pCtx,FaSize(PH7_ContextThis(pCtx)));` |
|    13 |  8097 | `	return PH7_OK;` |
|     1 |  8098 | `}` |
|    36 |  8099 | `static int vm_builtin_SplFixedArray_toArray(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  8100 | `{` |
|    38 |  8101 | `	ph7_value *pSlot = FaSlot(pCtx->pVm,PH7_ContextThis(pCtx));` |
|    18 |  8102 | `	SXUNUSED(nArg);` |
|    18 |  8103 | `	SXUNUSED(apArg);` |
|    38 |  8104 | `	if( pSlot ){` |
|    38 |  8105 | `		ph7_result_value(pCtx,pSlot);` |
|    18 |  8106 | `	}` |
|    38 |  8107 | `	return PH7_OK;` |
|     2 |  8108 | `}` |
|    22 |  8109 | `static int vm_builtin_SplFixedArray_offsetExists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  8110 | `{` |
|    24 |  8111 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    24 |  8112 | `	sxi64 iIdx = 0;` |
|    24 |  8113 | `	sxi32 rc = PH7_OK;` |
|     - |  8114 | `	ph7_value *pVal;` |
|    24 |  8115 | `	if( nArg < 1 ){` |
|   ! 0 |  8116 | `		return PH7_OK;` |
|     - |  8117 | `	}` |
|     - |  8118 | `	/* offsetExists RAISES for an undecodable offset exactly as the other three do —` |
|     - |  8119 | ``	 * `isset($f['x'])` is a TypeError, not a false — and answers false only for a`` |
|     - |  8120 | `	 * decodable index that is out of range or holds null. */` |
|    24 |  8121 | `	if( !FaOffset(pCtx,apArg[0],&iIdx,&rc) ){` |
|     5 |  8122 | `		return rc;` |
|     - |  8123 | `	}` |
|    20 |  8124 | `	if( iIdx < 0 \|\| iIdx >= FaSize(pThis) ){` |
|     7 |  8125 | `		ph7_result_bool(pCtx,0);` |
|     7 |  8126 | `		return PH7_OK;` |
|     - |  8127 | `	}` |
|     - |  8128 | `	/* php's isset() semantics: an unset slot holds null and is NOT set. */` |
|    14 |  8129 | `	pVal = FaAt(pCtx->pVm,pThis,iIdx);` |
|    14 |  8130 | `	ph7_result_bool(pCtx,pVal != 0 && (pVal->iFlags & MEMOBJ_NULL) == 0);` |
|    14 |  8131 | `	return PH7_OK;` |
|    13 |  8132 | `}` |
|    42 |  8133 | `static int vm_builtin_SplFixedArray_offsetGet(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  8134 | `{` |
|    44 |  8135 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    44 |  8136 | `	sxi64 iIdx = 0;` |
|    44 |  8137 | `	sxi32 rc = PH7_OK;` |
|     - |  8138 | `	ph7_value *pVal;` |
|    44 |  8139 | `	if( nArg < 1 ){` |
|   ! 0 |  8140 | `		return PH7_OK;` |
|     - |  8141 | `	}` |
|    44 |  8142 | `	if( !FaOffset(pCtx,apArg[0],&iIdx,&rc) ){` |
|     7 |  8143 | `		return rc;` |
|     - |  8144 | `	}` |
|    38 |  8145 | `	if( iIdx < 0 \|\| iIdx >= FaSize(pThis) ){` |
|     7 |  8146 | `		return FaOutOfBounds(pCtx);` |
|     - |  8147 | `	}` |
|    32 |  8148 | `	pVal = FaAt(pCtx->pVm,pThis,iIdx);` |
|    32 |  8149 | `	if( pVal ){` |
|    32 |  8150 | `		ph7_result_value(pCtx,pVal);` |
|    15 |  8151 | `	}` |
|    32 |  8152 | `	return PH7_OK;` |
|    23 |  8153 | `}` |
|   236 |  8154 | `static int vm_builtin_SplFixedArray_offsetSet(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  8155 | `{` |
|   239 |  8156 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   239 |  8157 | `	sxi64 iIdx = 0;` |
|   239 |  8158 | `	sxi32 rc = PH7_OK;` |
|   239 |  8159 | `	if( nArg < 2 ){` |
|   ! 0 |  8160 | `		return PH7_OK;` |
|     - |  8161 | `	}` |
|   239 |  8162 | `	if( !FaOffset(pCtx,apArg[0],&iIdx,&rc) ){` |
|    15 |  8163 | `		return rc;` |
|     - |  8164 | `	}` |
|   226 |  8165 | `	if( iIdx < 0 \|\| iIdx >= FaSize(pThis) ){` |
|     8 |  8166 | `		return FaOutOfBounds(pCtx);` |
|     - |  8167 | `	}` |
|   220 |  8168 | `	FaPut(pCtx->pVm,pThis,iIdx,apArg[1]);` |
|   220 |  8169 | `	return PH7_OK;` |
|   121 |  8170 | `}` |
|     2 |  8171 | `static int vm_builtin_SplFixedArray_offsetUnset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8172 | `{` |
|     3 |  8173 | `	ph7_vm *pVm = pCtx->pVm;` |
|     3 |  8174 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     3 |  8175 | `	sxi64 iIdx = 0;` |
|     3 |  8176 | `	sxi32 rc = PH7_OK;` |
|     - |  8177 | `	ph7_value sNull;` |
|     3 |  8178 | `	if( nArg < 1 ){` |
|   ! 0 |  8179 | `		return PH7_OK;` |
|     - |  8180 | `	}` |
|     3 |  8181 | `	if( !FaOffset(pCtx,apArg[0],&iIdx,&rc) ){` |
|   ! 0 |  8182 | `		return rc;` |
|     - |  8183 | `	}` |
|     3 |  8184 | `	if( iIdx < 0 \|\| iIdx >= FaSize(pThis) ){` |
|   ! 0 |  8185 | `		return FaOutOfBounds(pCtx);` |
|     - |  8186 | `	}` |
|     - |  8187 | `	/* The slot survives at its index and becomes null: the array is FIXED. */` |
|     3 |  8188 | `	PH7_MemObjInit(pVm,&sNull);` |
|     3 |  8189 | `	FaPut(pVm,pThis,iIdx,&sNull);` |
|     3 |  8190 | `	PH7_MemObjRelease(&sNull);` |
|     3 |  8191 | `	return PH7_OK;` |
|     2 |  8192 | `}` |
|    24 |  8193 | `static int vm_builtin_SplFixedArray_fromArray(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8194 | `{` |
|     - |  8195 | `	char zGiven[64];` |
|    25 |  8196 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  8197 | `	ph7_class *pCls;` |
|     - |  8198 | `	ph7_class_instance *pNew;` |
|     - |  8199 | `	ph7_hashmap *pSrc;` |
|     - |  8200 | `	ph7_hashmap_node *pNode,*pPrev;` |
|    25 |  8201 | `	int bPreserve = 1;` |
|    25 |  8202 | `	sxi64 nMax = -1, nNext = 0;` |
|    25 |  8203 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|   ! 0 |  8204 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - |  8205 | `			"SplFixedArray::fromArray(): Argument #1 ($array) must be of type array, %s given",` |
|   ! 0 |  8206 | `			nArg < 1 ? "none" : VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));` |
|     - |  8207 | `	}` |
|    25 |  8208 | `	if( nArg > 1 ){` |
|     7 |  8209 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|     3 |  8210 | `	}` |
|    25 |  8211 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     - |  8212 | `	/* php walks the keys FIRST and refuses the whole call before building anything. */` |
|    25 |  8213 | `	if( bPreserve ){` |
|    39 |  8214 | `		for( pNode = pSrc->pFirst ; pNode ; pNode = pPrev ){` |
|    27 |  8215 | `			pPrev = pNode->pPrev;` |
|    27 |  8216 | `			if( pNode->iType != HASHMAP_INT_NODE \|\| pNode->xKey.iKey < 0 ){` |
|     7 |  8217 | `				return PH7_VmThrowException(pCtx,"InvalidArgumentException",` |
|     - |  8218 | `					"array must contain only positive integer keys");` |
|     - |  8219 | `			}` |
|    21 |  8220 | `			if( pNode->xKey.iKey > nMax ){` |
|    19 |  8221 | `				nMax = pNode->xKey.iKey;` |
|     9 |  8222 | `			}` |
|    21 |  8223 | `			if( pNode == pSrc->pFirst && pPrev == 0 ){` |
|   ! 0 |  8224 | `				break;` |
|     - |  8225 | `			}` |
|    11 |  8226 | `		}` |
|     6 |  8227 | `	}` |
|    19 |  8228 | `	pCls = PH7_VmExtractClass(pVm,"SplFixedArray",sizeof("SplFixedArray")-1,FALSE,0);` |
|    19 |  8229 | `	if( pCls == 0 ){` |
|   ! 0 |  8230 | `		return PH7_OK;` |
|     - |  8231 | `	}` |
|    19 |  8232 | `	pNew = PH7_NewClassInstance(pVm,pCls);` |
|    19 |  8233 | `	if( pNew == 0 ){` |
|   ! 0 |  8234 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  8235 | `	}` |
|    19 |  8236 | `	pNew->iRef++;` |
|    19 |  8237 | `	FaResize(pVm,pNew,bPreserve ? nMax + 1 : (sxi64)pSrc->nEntry);` |
|    35 |  8238 | `	for( pNode = pSrc->pFirst ; pNode ; pNode = pPrev ){` |
|    31 |  8239 | `		ph7_value *pVal = HashmapExtractNodeValue(pNode);` |
|    31 |  8240 | `		pPrev = pNode->pPrev;` |
|    31 |  8241 | `		if( pVal ){` |
|    31 |  8242 | `			FaPut(pVm,pNew,bPreserve ? pNode->xKey.iKey : nNext,pVal);` |
|    15 |  8243 | `		}` |
|    31 |  8244 | `		nNext++;` |
|    31 |  8245 | `		if( pPrev == 0 ){` |
|    15 |  8246 | `			break;` |
|     - |  8247 | `		}` |
|     9 |  8248 | `	}` |
|    19 |  8249 | `	PH7_NativeResultObject(pCtx,pNew);` |
|    19 |  8250 | `	PH7_ClassInstanceUnref(pNew);` |
|    19 |  8251 | `	return PH7_OK;` |
|    13 |  8252 | `}` |
|     - |  8253 | `/* php's getIterator() answers an InternalIterator over the elements — the same` |
|     - |  8254 | ` * machinery every native IteratorAggregate here uses, which is also what makes` |
|     - |  8255 | ` * two iterators over one array independent. */` |
|    66 |  8256 | `static void FaIterSettle(ph7_vm *pVm,ph7_class_instance *pIt)` |
|     1 |  8257 | `{` |
|    67 |  8258 | `	ph7_class_instance *pSrc = PH7_NativeAttrObj(pIt,PH7_NATIVE_IT_SRC);` |
|    67 |  8259 | `	sxi64 iPos = PH7_NativeAttrInt(pIt,PH7_NATIVE_IT_POS);` |
|     - |  8260 | `	ph7_value *pVal;` |
|    67 |  8261 | `	if( pSrc == 0 \|\| iPos < 0 \|\| iPos >= FaSize(pSrc) ){` |
|    13 |  8262 | `		PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);` |
|    13 |  8263 | `		return;` |
|     - |  8264 | `	}` |
|    55 |  8265 | `	pVal = FaAt(&(*pVm),pSrc,iPos);` |
|    55 |  8266 | `	if( pVal ){` |
|    82 |  8267 | `		PH7_NativeSetProp(&(*pVm),pIt,PH7_NATIVE_IT_CUR,` |
|    27 |  8268 | `			(int)SyStrlen(PH7_NATIVE_IT_CUR),pVal);` |
|    27 |  8269 | `	}` |
|    55 |  8270 | `	PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_KEY,iPos);` |
|    55 |  8271 | `	PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,0);` |
|    34 |  8272 | `}` |
|    34 |  8273 | `static void FaIterRewind(ph7_vm *pVm,ph7_class_instance *pIt)` |
|     1 |  8274 | `{` |
|    35 |  8275 | `	PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_POS,0);` |
|    35 |  8276 | `	FaIterSettle(&(*pVm),pIt);` |
|    35 |  8277 | `}` |
|    32 |  8278 | `static void FaIterNext(ph7_vm *pVm,ph7_class_instance *pIt)` |
|     1 |  8279 | `{` |
|    49 |  8280 | `	PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_POS,` |
|    32 |  8281 | `		PH7_NativeAttrInt(pIt,PH7_NATIVE_IT_POS) + 1);` |
|    33 |  8282 | `	FaIterSettle(&(*pVm),pIt);` |
|    33 |  8283 | `}` |
|     - |  8284 | `static const PH7_NativeIterVtab sFaIterVtab = { FaIterRewind, FaIterNext, 0, 0 };` |
|    18 |  8285 | `static int vm_builtin_SplFixedArray_getIterator(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8286 | `{` |
|    19 |  8287 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  8288 | `	ph7_class_instance *pIt;` |
|     9 |  8289 | `	SXUNUSED(nArg);` |
|     9 |  8290 | `	SXUNUSED(apArg);` |
|    19 |  8291 | `	if( pThis == 0 ){` |
|   ! 0 |  8292 | `		return PH7_OK;` |
|     - |  8293 | `	}` |
|    19 |  8294 | `	pIt = PH7_NativeIteratorNew(pCtx->pVm,pThis);` |
|    19 |  8295 | `	if( pIt == 0 ){` |
|   ! 0 |  8296 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  8297 | `	}` |
|    19 |  8298 | `	PH7_NativeResultObject(pCtx,pIt);` |
|    19 |  8299 | `	return PH7_OK;` |
|    10 |  8300 | `}` |
|     2 |  8301 | `static int vm_builtin_SplFixedArray_wakeup(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8302 | `{` |
|     1 |  8303 | `	SXUNUSED(nArg);` |
|     1 |  8304 | `	SXUNUSED(apArg);` |
|     1 |  8305 | `	SXUNUSED(pCtx);` |
|     3 |  8306 | `	return PH7_OK;   /* php 8.4 keeps it, deprecated, doing nothing */` |
|     1 |  8307 | `}` |
|     - |  8308 | `/*` |
|     - |  8309 | ` * php's __serialize() here is NOT toArray(): the members ride in the SAME array as` |
|     - |  8310 | ` * the elements, told apart by their key — an INT key is an element and a STRING key` |
|     - |  8311 | ` * is a property. That is the whole reason the payload of a SplFixedArray subclass` |
|     - |  8312 | `` * reads `{i:0;N;i:1;N;s:1:"p";i:9;}` and not a nested pair.`` |
|     - |  8313 | ` */` |
|    14 |  8314 | `static int vm_builtin_SplFixedArray_serializeMagic(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8315 | `{` |
|    15 |  8316 | `	ph7_vm *pVm = pCtx->pVm;` |
|    15 |  8317 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  8318 | `	ph7_value sOut,*pSlot;` |
|     7 |  8319 | `	SXUNUSED(nArg);` |
|     7 |  8320 | `	SXUNUSED(apArg);` |
|    15 |  8321 | `	pSlot = FaSlot(pVm,pThis);` |
|    15 |  8322 | `	PH7_MemObjInit(pVm,&sOut);` |
|    15 |  8323 | `	if( pSlot ){` |
|    15 |  8324 | `		PH7_MemObjStore(pSlot,&sOut);` |
|     7 |  8325 | `	}` |
|    15 |  8326 | `	if( PH7_MemObjToHashmap(&sOut) != SXRET_OK ){` |
|   ! 0 |  8327 | `		PH7_MemObjRelease(&sOut);` |
|   ! 0 |  8328 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  8329 | `	}` |
|    15 |  8330 | `	SplAddMembers(pVm,pThis,&sOut);   /* string keys, beside the int-keyed elements */` |
|    15 |  8331 | `	ph7_result_value(pCtx,&sOut);` |
|    15 |  8332 | `	PH7_MemObjRelease(&sOut);` |
|    15 |  8333 | `	return PH7_OK;` |
|     8 |  8334 | `}` |
|     - |  8335 | `/* Split the payload back apart: int keys rebuild the elements, string keys the` |
|     - |  8336 | ` * properties. The element count is what the INT half holds, not the whole array. */` |
|     - |  8337 | `typedef struct fa_unser_ctx fa_unser_ctx;` |
|     - |  8338 | `struct fa_unser_ctx` |
|     - |  8339 | `{` |
|     - |  8340 | `	ph7_class_instance *pThis;` |
|     - |  8341 | `	ph7_value *pElems;` |
|     - |  8342 | `	sxi64 nElem;` |
|     - |  8343 | `};` |
|    18 |  8344 | `static int FaUnserWalk(ph7_value *pKey,ph7_value *pVal,void *pUserData)` |
|     1 |  8345 | `{` |
|    19 |  8346 | `	fa_unser_ctx *pFa = (fa_unser_ctx *)pUserData;` |
|    19 |  8347 | `	if( ph7_value_is_string(pKey) ){` |
|     - |  8348 | `		int nKey;` |
|     5 |  8349 | `		const char *zKey = ph7_value_to_string(pKey,&nKey);` |
|     5 |  8350 | `		PH7_NativeSetProp(pFa->pThis->pVm,pFa->pThis,zKey,(sxu32)nKey,pVal);` |
|     5 |  8351 | `		return PH7_OK;` |
|     - |  8352 | `	}` |
|    15 |  8353 | `	ph7_array_add_elem(pFa->pElems,0,pVal);` |
|    15 |  8354 | `	pFa->nElem++;` |
|    15 |  8355 | `	return PH7_OK;` |
|    10 |  8356 | `}` |
|     6 |  8357 | `static int vm_builtin_SplFixedArray_unserializeMagic(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8358 | `{` |
|     7 |  8359 | `	ph7_vm *pVm = pCtx->pVm;` |
|     7 |  8360 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  8361 | `	fa_unser_ctx sFa;` |
|     - |  8362 | `	ph7_value sElems,*pSlot;` |
|     7 |  8363 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_HASHMAP) == 0 \|\| pThis == 0 ){` |
|   ! 0 |  8364 | `		return PH7_OK;` |
|     - |  8365 | `	}` |
|     7 |  8366 | `	PH7_MemObjInit(pVm,&sElems);` |
|     7 |  8367 | `	if( PH7_MemObjToHashmap(&sElems) != SXRET_OK ){` |
|   ! 0 |  8368 | `		PH7_MemObjRelease(&sElems);` |
|   ! 0 |  8369 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  8370 | `	}` |
|     7 |  8371 | `	sFa.pThis = pThis;` |
|     7 |  8372 | `	sFa.pElems = &sElems;` |
|     7 |  8373 | `	sFa.nElem = 0;` |
|     7 |  8374 | `	ph7_array_walk(apArg[0],FaUnserWalk,&sFa);` |
|     7 |  8375 | `	pSlot = PH7_NativeAttr(pThis,FA_A);` |
|     7 |  8376 | `	if( pSlot ){` |
|     7 |  8377 | `		PH7_MemObjRelease(pSlot);` |
|     7 |  8378 | `		PH7_MemObjStore(&sElems,pSlot);` |
|     3 |  8379 | `	}` |
|     7 |  8380 | `	PH7_MemObjRelease(&sElems);` |
|     7 |  8381 | `	PH7_NativeSetAttrInt(pVm,pThis,FA_N,sFa.nElem);` |
|     7 |  8382 | `	return PH7_OK;` |
|     4 |  8383 | `}` |
|     - |  8384 | `/*` |
|     - |  8385 | ` * php's get_properties: the ELEMENTS, keyed by index, on every surface —` |
|     - |  8386 | ` * var_dump, print_r, the (array) cast and (through __serialize) serialize(). This` |
|     - |  8387 | ` * is the one native class so far whose presentation is the same for the debug and` |
|     - |  8388 | ` * the cast form.` |
|     - |  8389 | ` */` |
|     2 |  8390 | `static sxi32 FaPresent(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)` |
|     1 |  8391 | `{` |
|     3 |  8392 | `	sxi64 n = FaSize(pThis), i;` |
|     1 |  8393 | `	SXUNUSED(bDebug);` |
|     9 |  8394 | `	for( i = 0 ; i < n ; ++i ){` |
|     7 |  8395 | `		ph7_value sKey,*pVal = FaAt(&(*pVm),pThis,i);` |
|     7 |  8396 | `		if( pVal == 0 ){` |
|   ! 0 |  8397 | `			continue;` |
|     - |  8398 | `		}` |
|     7 |  8399 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,i);` |
|     7 |  8400 | `		ph7_array_add_elem(pOut,&sKey,pVal);` |
|     7 |  8401 | `		PH7_MemObjRelease(&sKey);` |
|     4 |  8402 | `	}` |
|     3 |  8403 | `	return PH7_OK;` |
|     1 |  8404 | `}` |
|     - |  8405 | `/*` |
|     - |  8406 | ` * The declaration. Method ORDER and the interface list are spl_fixedarray.stub's;` |
|     - |  8407 | ` * note that __construct, __serialize, __unserialize, getIterator and jsonSerialize` |
|     - |  8408 | ` * are the FIVE methods php does NOT mark tentative here.` |
|     - |  8409 | ` */` |
|  8445 |  8410 | `static sxi32 VmInstallSplFixedArray(ph7_vm *pVm)` |
|     5 |  8411 | `{` |
|     - |  8412 | `	static const PH7_NativePropDef aFaProp[] = {` |
|     - |  8413 | `		{ FA_A, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  8414 | `		{ FA_N, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  8415 | `	};` |
|     - |  8416 | `	static const PH7_NativeMethodDef aFaMethod[] = {` |
|     - |  8417 | `		{ "__construct",   PH7_MOD_PUBLIC, "int $size = 0", 0,` |
|     - |  8418 | `		  vm_builtin_SplFixedArray_construct },` |
|     - |  8419 | `		{ "__wakeup",      PH7_MOD_PUBLIC, "", "@void", vm_builtin_SplFixedArray_wakeup },` |
|     - |  8420 | `		{ "__serialize",   PH7_MOD_PUBLIC, "", "array", vm_builtin_SplFixedArray_serializeMagic },` |
|     - |  8421 | `		{ "__unserialize", PH7_MOD_PUBLIC, "array $data", "void",` |
|     - |  8422 | `		  vm_builtin_SplFixedArray_unserializeMagic },` |
|     - |  8423 | `		{ "count",         PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplFixedArray_count },` |
|     - |  8424 | `		{ "toArray",       PH7_MOD_PUBLIC, "", "@array", vm_builtin_SplFixedArray_toArray },` |
|     - |  8425 | `		{ "fromArray",     PH7_MOD_PUBLIC\|PH7_MOD_STATIC,` |
|     - |  8426 | `		  "array $array, bool $preserveKeys = true", "@SplFixedArray",` |
|     - |  8427 | `		  vm_builtin_SplFixedArray_fromArray },` |
|     - |  8428 | `		{ "getSize",       PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplFixedArray_getSize },` |
|     - |  8429 | `		{ "setSize",       PH7_MOD_PUBLIC, "int $size", "@true", vm_builtin_SplFixedArray_setSize },` |
|     - |  8430 | `		/* php's stub leaves the four offsets UNTYPED and decodes them itself, the` |
|     - |  8431 | `		 * same shape SplDoublyLinkedList has — but a different rule and a different` |
|     - |  8432 | `		 * refusal, so FaOffset() rather than the DLL's PH7_IntArgResolve. */` |
|     - |  8433 | `		{ "offsetExists",  PH7_MOD_PUBLIC, "$index", "@bool",` |
|     - |  8434 | `		  vm_builtin_SplFixedArray_offsetExists },` |
|     - |  8435 | `		{ "offsetGet",     PH7_MOD_PUBLIC, "$index", "@mixed", vm_builtin_SplFixedArray_offsetGet },` |
|     - |  8436 | `		{ "offsetSet",     PH7_MOD_PUBLIC, "$index, mixed $value", "@void",` |
|     - |  8437 | `		  vm_builtin_SplFixedArray_offsetSet },` |
|     - |  8438 | `		{ "offsetUnset",   PH7_MOD_PUBLIC, "$index", "@void",` |
|     - |  8439 | `		  vm_builtin_SplFixedArray_offsetUnset },` |
|     - |  8440 | `		{ "getIterator",   PH7_MOD_PUBLIC, "", "Iterator", vm_builtin_SplFixedArray_getIterator },` |
|     - |  8441 | `		{ "jsonSerialize", PH7_MOD_PUBLIC, "", "array", vm_builtin_SplFixedArray_toArray },` |
|     - |  8442 | `	};` |
|     - |  8443 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - |  8444 | `		{ "SplFixedArray", 0, "IteratorAggregate,ArrayAccess,Countable,JsonSerializable", 0,` |
|     - |  8445 | `		  aFaMethod, SX_ARRAYSIZE(aFaMethod), 0, 0,` |
|     - |  8446 | `		  aFaProp, SX_ARRAYSIZE(aFaProp), 0, &sFaIterVtab, FaPresent },` |
|     - |  8447 | `	};` |
|  8450 |  8448 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|     5 |  8449 | `}` |
|     - |  8450 | `/*` |
|     - |  8451 | ` * ---------------------------------------------------------------------------` |
|     - |  8452 | ` * SplObjectStorage, SplObserver and SplSubject.` |
|     - |  8453 | ` *` |
|     - |  8454 | `` * php's `spl_SplObjectStorage` is a hashtable of {obj, inf} pairs keyed by the`` |
|     - |  8455 | `` * object HANDLE, plus TWO cursors that are not the same thing: `pos` walks the`` |
|     - |  8456 | `` * table and `index` is the integer `key()` reports. Every method that changes the`` |
|     - |  8457 | ` * membership resets one or both, and the chunk -- which kept a single integer` |
|     - |  8458 | `` * offset -- had none of that: `detach()` mid-walk left the walk where it was`` |
|     - |  8459 | `` * (php restarts it), and `addAll()` left `key()` counting from wherever it stood.`` |
|     - |  8460 | ` *` |
|     - |  8461 | `` * What the chunk did not have AT ALL, which is most of the class: `seek()` and the`` |
|     - |  8462 | `` * `SeekableIterator` interface it comes from, `Serializable` with its`` |
|     - |  8463 | `` * `serialize()`/`unserialize()` pair, the `__serialize()`/`__unserialize()` pair`` |
|     - |  8464 | `` * php actually uses, and `__debugInfo()`. Six methods and two interfaces missing`` |
|     - |  8465 | ` * from a 25-method class -- rule 53, and the reason a method-by-method reading is` |
|     - |  8466 | ` * not a conversion.` |
|     - |  8467 | ` *` |
|     - |  8468 | `` * Two more the model hides. **`current()` on an invalid iterator RAISES**`` |
|     - |  8469 | `` * (`Called current() on invalid iterator`) where the chunk answered null, and`` |
|     - |  8470 | `` * **an overridden `getHash()` is what keys the table** -- php looks the method up`` |
|     - |  8471 | `` * once per instance (`fptr_get_hash`) and every attach/detach/contains goes`` |
|     - |  8472 | ` * through it, so a subclass that hashes two distinct objects the same stores ONE` |
|     - |  8473 | `` * entry. The chunk called `spl_object_id()` directly and ignored its own`` |
|     - |  8474 | `` * `getHash()`, so overriding it did nothing.`` |
|     - |  8475 | ` *` |
|     - |  8476 | ` * php DEPRECATES attach/detach/contains since 8.5 and PHL says nothing, which is` |
|     - |  8477 | ` * the same non-deprecated-compatibility policy the chunk carried (the notice is` |
|     - |  8478 | ` * the only difference and no valid php depends on it).` |
|     - |  8479 | ` */` |
|     - |  8480 | `#define SOS_S "__s"   /* php's storage: key -> ['obj' => object, 'inf' => info] */` |
|     - |  8481 | `#define SOS_I "__i"   /* php's index: what key() reports, NOT a position */` |
|     - |  8482 |  |
|     - |  8483 | `/* The storage slot, separated for writing (every caller may mutate it). */` |
|   742 |  8484 | `static ph7_value * SosSlot(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  8485 | `{` |
|   743 |  8486 | `	ph7_value *pSlot = pThis ? PH7_NativeAttr(pThis,SOS_S) : 0;` |
|   743 |  8487 | `	if( pSlot == 0 ){` |
|   ! 0 |  8488 | `		return 0;` |
|     - |  8489 | `	}` |
|   743 |  8490 | `	if( (pSlot->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|   129 |  8491 | `		if( PH7_MemObjToHashmap(pSlot) != SXRET_OK ){` |
|   ! 0 |  8492 | `			return 0;` |
|     - |  8493 | `		}` |
|    64 |  8494 | `	}` |
|   743 |  8495 | `	if( PH7_HashmapCowSeparate(pVm,pSlot) == 0 ){` |
|   ! 0 |  8496 | `		return 0;` |
|     - |  8497 | `	}` |
|     - |  8498 | `	/* Re-ask: the separation can MOVE the memobj pool this slot lives in` |
|     - |  8499 | `	 * (SplStoreSlot explains it). */` |
|   743 |  8500 | `	return PH7_NativeAttr(pThis,SOS_S);` |
|   372 |  8501 | `}` |
|   742 |  8502 | `static ph7_hashmap * SosMap(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  8503 | `{` |
|   743 |  8504 | `	ph7_value *pSlot = SosSlot(pVm,pThis);` |
|   743 |  8505 | `	return pSlot ? (ph7_hashmap *)pSlot->x.pOther : 0;` |
|     1 |  8506 | `}` |
|     - |  8507 | `/* One half of a stored pair: php's element->obj / element->inf. */` |
|   564 |  8508 | `static ph7_value * SosPart(ph7_value *pPair,const char *zKey)` |
|     1 |  8509 | `{` |
|   565 |  8510 | `	ph7_hashmap_node *pNode = 0;` |
|   565 |  8511 | `	if( pPair == 0 \|\| (pPair->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     5 |  8512 | `		return 0;` |
|     - |  8513 | `	}` |
|   840 |  8514 | `	if( HashmapLookupBlobKey((ph7_hashmap *)pPair->x.pOther,zKey,` |
|   841 |  8515 | `		(sxu32)SyStrlen(zKey),&pNode) != SXRET_OK ){` |
|   ! 0 |  8516 | `		return 0;` |
|     - |  8517 | `	}` |
|   561 |  8518 | `	return HashmapExtractNodeValue(pNode);` |
|   283 |  8519 | `}` |
|     - |  8520 | ``/* Write one half of a pair; a NULL value is php's `ZVAL_NULL(&element->inf)`. */`` |
|   390 |  8521 | `static void SosSetPart(ph7_vm *pVm,ph7_value *pPair,const char *zKey,ph7_value *pVal)` |
|     1 |  8522 | `{` |
|     - |  8523 | `	ph7_value sKey,sNull;` |
|   391 |  8524 | `	PH7_MemObjInitFromString(pVm,&sKey,0);` |
|   391 |  8525 | `	PH7_MemObjStringAppend(&sKey,zKey,(sxu32)SyStrlen(zKey));` |
|   391 |  8526 | `	if( pVal ){` |
|   391 |  8527 | `		ph7_array_add_elem(pPair,&sKey,pVal);` |
|   196 |  8528 | `	}else{` |
|   ! 0 |  8529 | `		PH7_MemObjInit(pVm,&sNull);` |
|   ! 0 |  8530 | `		ph7_array_add_elem(pPair,&sKey,&sNull);` |
|   ! 0 |  8531 | `		PH7_MemObjRelease(&sNull);` |
|     - |  8532 | `	}` |
|   391 |  8533 | `	PH7_MemObjRelease(&sKey);` |
|   391 |  8534 | `}` |
|     - |  8535 | ``/* The pair a node holds, separated: php mutates `element->inf` in place, and here`` |
|     - |  8536 | ` * that is a NESTED array whose COW copy has to be broken first -- a pair handed` |
|     - |  8537 | ` * out by __serialize()/__debugInfo() would otherwise change with it. */` |
|    10 |  8538 | `static ph7_value * SosPairForWrite(ph7_vm *pVm,ph7_hashmap_node *pNode)` |
|     1 |  8539 | `{` |
|    11 |  8540 | `	ph7_value *pPair = HashmapExtractNodeValue(pNode);` |
|    11 |  8541 | `	if( pPair == 0 \|\| (pPair->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|   ! 0 |  8542 | `		return 0;` |
|     - |  8543 | `	}` |
|    11 |  8544 | `	return PH7_HashmapCowSeparate(pVm,pPair) ? pPair : 0;` |
|     6 |  8545 | `}` |
|     - |  8546 | `/*` |
|     - |  8547 | `` * php's `fptr_get_hash`: the class caches the method ONLY when a subclass declares`` |
|     - |  8548 | ` * its own, and every keyed operation then runs it. The one native body is this` |
|     - |  8549 | ` * class's own, so a non-native getHash() IS the override.` |
|     - |  8550 | ` */` |
|   266 |  8551 | `static ph7_class_method * SosUserHash(ph7_class_instance *pThis)` |
|     1 |  8552 | `{` |
|     - |  8553 | `	ph7_class_method *pMethod;` |
|   267 |  8554 | `	if( pThis == 0 ){` |
|   ! 0 |  8555 | `		return 0;` |
|     - |  8556 | `	}` |
|   267 |  8557 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"getHash",sizeof("getHash")-1);` |
|   267 |  8558 | `	if( pMethod == 0 \|\| (pMethod->sFunc.iFlags & VM_FUNC_NATIVE) ){` |
|   261 |  8559 | `		return 0;` |
|     - |  8560 | `	}` |
|     7 |  8561 | `	return pMethod;` |
|   134 |  8562 | `}` |
|     - |  8563 | `/*` |
|     - |  8564 | ` * php's spl_object_storage_get_hash: the object HANDLE, or the STRING an` |
|     - |  8565 | ` * overridden getHash() answers. php checks the returned type itself (its own` |
|     - |  8566 | ` * return declaration would coerce first, so this only fires for an untyped` |
|     - |  8567 | ` * override) and names the RUNTIME class in the refusal.` |
|     - |  8568 | ` */` |
|   266 |  8569 | `static sxi32 SosKey(ph7_context *pCtx,ph7_class_instance *pThis,ph7_value *pObj,ph7_value *pKey)` |
|     1 |  8570 | `{` |
|   267 |  8571 | `	ph7_vm *pVm = pCtx->pVm;` |
|   267 |  8572 | `	ph7_class_method *pHash = SosUserHash(pThis);` |
|     - |  8573 | `	ph7_value sRes,*apArg[1];` |
|     - |  8574 | `	sxi32 rc;` |
|   267 |  8575 | `	if( pHash == 0 ){` |
|   261 |  8576 | `		ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|   261 |  8577 | `		PH7_MemObjRelease(pKey);` |
|   261 |  8578 | `		PH7_MemObjInitFromInt(pVm,pKey,(sxi64)pInst->nObjId);` |
|   261 |  8579 | `		return PH7_OK;` |
|     - |  8580 | `	}` |
|     7 |  8581 | `	PH7_MemObjInit(pVm,&sRes);` |
|     7 |  8582 | `	apArg[0] = pObj;` |
|     7 |  8583 | `	rc = PH7_VmCallClassMethod(pVm,pThis,pHash,&sRes,1,apArg);` |
|     7 |  8584 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  8585 | `		PH7_MemObjRelease(&sRes);` |
|   ! 0 |  8586 | `		return rc;` |
|     - |  8587 | `	}` |
|     7 |  8588 | `	if( (sRes.iFlags & MEMOBJ_STRING) == 0 ){` |
|     - |  8589 | `		char zGiven[64];` |
|   ! 0 |  8590 | `		SyString *pName = &pThis->pClass->sName;` |
|   ! 0 |  8591 | `		PH7_MemObjRelease(&sRes);` |
|   ! 0 |  8592 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - |  8593 | `			"%z::getHash(): Return value must be of type string, %s returned",` |
|   ! 0 |  8594 | `			pName,VmValueGivenName(&sRes,zGiven,sizeof(zGiven)));` |
|     - |  8595 | `	}` |
|     7 |  8596 | `	PH7_MemObjRelease(pKey);` |
|     7 |  8597 | `	PH7_MemObjInit(pVm,pKey);` |
|     7 |  8598 | `	PH7_MemObjStore(&sRes,pKey);` |
|     7 |  8599 | `	PH7_MemObjRelease(&sRes);` |
|     7 |  8600 | `	return PH7_OK;` |
|   134 |  8601 | `}` |
|     - |  8602 | `/*` |
|     - |  8603 | ` * php's Z_PARAM_OBJ for the four ArrayAccess offsets: their stub leaves $object` |
|     - |  8604 | `` * UNTYPED (a `@param object` docblock, which Reflection does not print) while the`` |
|     - |  8605 | ` * ZPP is an object, so the declared type says nothing and each body words the` |
|     - |  8606 | ` * refusal here -- SplDoublyLinkedList's $index has the same shape one type over.` |
|     - |  8607 | ` * The name is always this class's, even from a subclass (php's).` |
|     - |  8608 | ` */` |
|   172 |  8609 | `static sxi32 SosObjectArg(ph7_context *pCtx,const char *zMethod,int nArg,ph7_value **apArg,` |
|     - |  8610 | `	ph7_value **ppObj)` |
|     1 |  8611 | `{` |
|     - |  8612 | `	char zGiven[64];` |
|   173 |  8613 | `	*ppObj = 0;` |
|   173 |  8614 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) && apArg[0]->x.pOther ){` |
|   165 |  8615 | `		*ppObj = apArg[0];` |
|   165 |  8616 | `		return PH7_OK;` |
|     - |  8617 | `	}` |
|    17 |  8618 | `	return PH7_VmThrowException(pCtx,"TypeError",` |
|     - |  8619 | `		"SplObjectStorage::%s(): Argument #1 ($object) must be of type object, %s given",` |
|     8 |  8620 | `		zMethod,nArg < 1 ? "none" : VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));` |
|    87 |  8621 | `}` |
|     - |  8622 | `/* The other storage a set operation takes; php's ZPP already screened the class. */` |
|    12 |  8623 | `static ph7_class_instance * SosOther(int nArg,ph7_value **apArg)` |
|     1 |  8624 | `{` |
|    13 |  8625 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|   ! 0 |  8626 | `		return 0;` |
|     - |  8627 | `	}` |
|    13 |  8628 | `	return (ph7_class_instance *)apArg[0]->x.pOther;` |
|     7 |  8629 | `}` |
|     - |  8630 | `/*` |
|     - |  8631 | ` * php's spl_object_storage_attach. The two values are COPIED first: computing the` |
|     - |  8632 | ` * key can run an overridden getHash(), and any call into user code moves every` |
|     - |  8633 | ` * ph7_value the caller is holding (rule 47).` |
|     - |  8634 | ` */` |
|   198 |  8635 | `static sxi32 SosAttach(ph7_context *pCtx,ph7_class_instance *pThis,ph7_value *pObj,ph7_value *pInf)` |
|     1 |  8636 | `{` |
|   199 |  8637 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  8638 | `	ph7_hashmap *pMap;` |
|   199 |  8639 | `	ph7_hashmap_node *pNode = 0;` |
|     - |  8640 | `	ph7_value sObj,sInf,sKey,sPair;` |
|     - |  8641 | `	sxi32 rc;` |
|   199 |  8642 | `	PH7_MemObjInit(pVm,&sObj);` |
|   199 |  8643 | `	PH7_MemObjInit(pVm,&sInf);` |
|   199 |  8644 | `	PH7_MemObjInit(pVm,&sKey);` |
|   199 |  8645 | `	PH7_MemObjStore(pObj,&sObj);` |
|   199 |  8646 | `	if( pInf ){` |
|   195 |  8647 | `		PH7_MemObjStore(pInf,&sInf);` |
|    97 |  8648 | `	}` |
|   199 |  8649 | `	rc = SosKey(pCtx,pThis,&sObj,&sKey);` |
|   199 |  8650 | `	if( rc != PH7_OK ){` |
|   ! 0 |  8651 | `		goto done;` |
|     - |  8652 | `	}` |
|   199 |  8653 | `	pMap = SosMap(pVm,pThis);` |
|   199 |  8654 | `	if( pMap == 0 ){` |
|   ! 0 |  8655 | `		goto done;` |
|     - |  8656 | `	}` |
|   199 |  8657 | `	if( PH7_HashmapLookup(pMap,&sKey,&pNode) == SXRET_OK ){` |
|     9 |  8658 | `		ph7_value *pPair = SosPairForWrite(pVm,pNode);` |
|     9 |  8659 | `		if( pPair ){` |
|     9 |  8660 | `			SosSetPart(pVm,pPair,"inf",&sInf);` |
|     4 |  8661 | `		}` |
|     9 |  8662 | `		goto done;` |
|     - |  8663 | `	}` |
|   191 |  8664 | `	PH7_MemObjInit(pVm,&sPair);` |
|   191 |  8665 | `	if( PH7_MemObjToHashmap(&sPair) != SXRET_OK ){` |
|   ! 0 |  8666 | `		PH7_MemObjRelease(&sPair);` |
|   ! 0 |  8667 | `		rc = PH7_ContextMemoryError(pCtx);` |
|   ! 0 |  8668 | `		goto done;` |
|     - |  8669 | `	}` |
|   191 |  8670 | `	SosSetPart(pVm,&sPair,"obj",&sObj);` |
|   191 |  8671 | `	SosSetPart(pVm,&sPair,"inf",&sInf);` |
|     - |  8672 | `	/* php's position is an INTEGER index into the bucket array, so a cursor that` |
|     - |  8673 | `	 * ran off the end is revived by the insert and the walk resumes on the new` |
|     - |  8674 | `	 * element -- what addAll() mid-iteration does there. */` |
|   191 |  8675 | `	SplStoreInsert(pMap,&sKey,&sPair);` |
|   191 |  8676 | `	PH7_MemObjRelease(&sPair);` |
|    99 |  8677 | `done:` |
|   199 |  8678 | `	PH7_MemObjRelease(&sObj);` |
|   199 |  8679 | `	PH7_MemObjRelease(&sInf);` |
|   199 |  8680 | `	PH7_MemObjRelease(&sKey);` |
|   199 |  8681 | `	return rc;` |
|     1 |  8682 | `}` |
|     - |  8683 | `/* php's spl_object_storage_detach: drop the entry, saying whether there was one. */` |
|    20 |  8684 | `static sxi32 SosDetach(ph7_context *pCtx,ph7_class_instance *pThis,ph7_value *pObj,int *pbGone)` |
|     1 |  8685 | `{` |
|    21 |  8686 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  8687 | `	ph7_hashmap *pMap;` |
|    21 |  8688 | `	ph7_hashmap_node *pNode = 0;` |
|     - |  8689 | `	ph7_value sObj,sKey;` |
|     - |  8690 | `	sxi32 rc;` |
|    21 |  8691 | `	if( pbGone ){` |
|   ! 0 |  8692 | `		*pbGone = 0;` |
|   ! 0 |  8693 | `	}` |
|    21 |  8694 | `	PH7_MemObjInit(pVm,&sObj);` |
|    21 |  8695 | `	PH7_MemObjInit(pVm,&sKey);` |
|    21 |  8696 | `	PH7_MemObjStore(pObj,&sObj);` |
|    21 |  8697 | `	rc = SosKey(pCtx,pThis,&sObj,&sKey);` |
|    21 |  8698 | `	if( rc == PH7_OK ){` |
|    21 |  8699 | `		pMap = SosMap(pVm,pThis);` |
|    21 |  8700 | `		if( pMap && PH7_HashmapLookup(pMap,&sKey,&pNode) == SXRET_OK ){` |
|    19 |  8701 | `			PH7_HashmapUnlinkNode(pNode,TRUE);` |
|    19 |  8702 | `			if( pbGone ){` |
|   ! 0 |  8703 | `				*pbGone = 1;` |
|   ! 0 |  8704 | `			}` |
|     9 |  8705 | `		}` |
|    10 |  8706 | `	}` |
|    21 |  8707 | `	PH7_MemObjRelease(&sObj);` |
|    21 |  8708 | `	PH7_MemObjRelease(&sKey);` |
|    21 |  8709 | `	return rc;` |
|     1 |  8710 | `}` |
|     - |  8711 | `/* php's spl_object_storage_contains: an entry EXISTS, whatever its info holds. */` |
|    20 |  8712 | `static sxi32 SosContains(ph7_context *pCtx,ph7_class_instance *pThis,ph7_value *pObj,int *pbFound)` |
|     1 |  8713 | `{` |
|    21 |  8714 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  8715 | `	ph7_hashmap *pMap;` |
|    21 |  8716 | `	ph7_hashmap_node *pNode = 0;` |
|     - |  8717 | `	ph7_value sObj,sKey;` |
|     - |  8718 | `	sxi32 rc;` |
|    21 |  8719 | `	*pbFound = 0;` |
|    21 |  8720 | `	PH7_MemObjInit(pVm,&sObj);` |
|    21 |  8721 | `	PH7_MemObjInit(pVm,&sKey);` |
|    21 |  8722 | `	PH7_MemObjStore(pObj,&sObj);` |
|    21 |  8723 | `	rc = SosKey(pCtx,pThis,&sObj,&sKey);` |
|    21 |  8724 | `	if( rc == PH7_OK ){` |
|    21 |  8725 | `		pMap = SosMap(pVm,pThis);` |
|    21 |  8726 | `		*pbFound = pMap && PH7_HashmapLookup(pMap,&sKey,&pNode) == SXRET_OK;` |
|    10 |  8727 | `	}` |
|    21 |  8728 | `	PH7_MemObjRelease(&sObj);` |
|    21 |  8729 | `	PH7_MemObjRelease(&sKey);` |
|    21 |  8730 | `	return rc;` |
|     1 |  8731 | `}` |
|     - |  8732 | ``/* php's `zend_hash_internal_pointer_reset_ex(&storage, &pos); index = 0`. */`` |
|    36 |  8733 | `static void SosRewind(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  8734 | `{` |
|    37 |  8735 | `	ph7_hashmap *pMap = SosMap(pVm,pThis);` |
|    37 |  8736 | `	if( pMap ){` |
|    37 |  8737 | `		pMap->pCur = pMap->pFirst;` |
|    18 |  8738 | `	}` |
|    37 |  8739 | `	PH7_NativeSetAttrInt(pVm,pThis,SOS_I,0);` |
|    37 |  8740 | `}` |
|     - |  8741 | `/* The pair the cursor is on, or 0 past the end. */` |
|    84 |  8742 | `static ph7_value * SosCurrentPair(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  8743 | `{` |
|    85 |  8744 | `	ph7_hashmap *pMap = SosMap(pVm,pThis);` |
|    85 |  8745 | `	if( pMap == 0 \|\| pMap->pCur == 0 ){` |
|     5 |  8746 | `		return 0;` |
|     - |  8747 | `	}` |
|    81 |  8748 | `	return HashmapExtractNodeValue(pMap->pCur);` |
|    43 |  8749 | `}` |
|     - |  8750 | `/*` |
|     - |  8751 | ` * A SNAPSHOT of one storage's pairs as a plain list. Every set operation walks one` |
|     - |  8752 | ` * storage while writing to another -- and either walk can run an overridden` |
|     - |  8753 | ` * getHash(), which moves things (rule 47) and can even mutate the map being walked.` |
|     - |  8754 | ` * php's own SPL_SAFE_HASH_FOREACH_PTR is the same precaution one layer down.` |
|     - |  8755 | ` */` |
|    12 |  8756 | `static sxi32 SosSnapshot(ph7_vm *pVm,ph7_class_instance *pFrom,ph7_value *pOut)` |
|     1 |  8757 | `{` |
|    13 |  8758 | `	ph7_hashmap *pMap = SosMap(pVm,pFrom);` |
|     - |  8759 | `	ph7_hashmap_node *pNode;` |
|     - |  8760 | `	sxu32 n;` |
|    13 |  8761 | `	if( PH7_MemObjToHashmap(pOut) != SXRET_OK ){` |
|   ! 0 |  8762 | `		return SXERR_MEM;` |
|     - |  8763 | `	}` |
|    13 |  8764 | `	if( pMap == 0 ){` |
|   ! 0 |  8765 | `		return SXRET_OK;` |
|     - |  8766 | `	}` |
|    13 |  8767 | `	pNode = pMap->pFirst;` |
|    37 |  8768 | `	for( n = 0 ; n < pMap->nEntry && pNode ; ++n ){` |
|    25 |  8769 | `		ph7_value *pPair = HashmapExtractNodeValue(pNode);` |
|    25 |  8770 | `		if( pPair ){` |
|    25 |  8771 | `			ph7_array_add_elem(pOut,0,pPair);` |
|    12 |  8772 | `		}` |
|    25 |  8773 | `		pNode = pNode->pPrev;   /* insertion order: pFirst, then the pPrev chain */` |
|    13 |  8774 | `	}` |
|    13 |  8775 | `	return SXRET_OK;` |
|     7 |  8776 | `}` |
|     - |  8777 | `/* One pair of a snapshot, re-resolved by index because a user call may have moved` |
|     - |  8778 | ` * every value in the pool since the last one. */` |
|    28 |  8779 | `static ph7_value * SosSnapAt(ph7_value *pSnap,sxi64 i)` |
|     1 |  8780 | `{` |
|    29 |  8781 | `	ph7_hashmap_node *pNode = 0;` |
|    28 |  8782 | `	if( (pSnap->iFlags & MEMOBJ_HASHMAP) == 0` |
|    29 |  8783 | `	 \|\| HashmapLookupIntKey((ph7_hashmap *)pSnap->x.pOther,i,&pNode) != SXRET_OK ){` |
|   ! 0 |  8784 | `		return 0;` |
|     - |  8785 | `	}` |
|    29 |  8786 | `	return HashmapExtractNodeValue(pNode);` |
|    15 |  8787 | `}` |
|     4 |  8788 | `static int vm_builtin_SplObjectStorage_attach(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8789 | `{` |
|     - |  8790 | `	ph7_value *pObj;` |
|     5 |  8791 | `	sxi32 rc = SosObjectArg(pCtx,"attach",nArg,apArg,&pObj);` |
|     5 |  8792 | `	if( rc != PH7_OK ){` |
|   ! 0 |  8793 | `		return rc;` |
|     - |  8794 | `	}` |
|     5 |  8795 | `	return SosAttach(pCtx,PH7_ContextThis(pCtx),pObj,nArg > 1 ? apArg[1] : 0);` |
|     3 |  8796 | `}` |
|     - |  8797 | `/* php's offsetSet is an @implementation-alias of attach, and its refusal says so. */` |
|   116 |  8798 | `static int vm_builtin_SplObjectStorage_offsetSet(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8799 | `{` |
|     - |  8800 | `	ph7_value *pObj;` |
|   117 |  8801 | `	sxi32 rc = SosObjectArg(pCtx,"offsetSet",nArg,apArg,&pObj);` |
|   117 |  8802 | `	if( rc != PH7_OK ){` |
|     3 |  8803 | `		return rc;` |
|     - |  8804 | `	}` |
|   115 |  8805 | `	return SosAttach(pCtx,PH7_ContextThis(pCtx),pObj,nArg > 1 ? apArg[1] : 0);` |
|    59 |  8806 | `}` |
|     - |  8807 | `/* detach() RESTARTS the walk: php resets both the position and the index, whether` |
|     - |  8808 | ` * or not anything was removed. */` |
|     4 |  8809 | `static sxi32 SosDetachMethod(ph7_context *pCtx,const char *zMethod,int nArg,ph7_value **apArg)` |
|     1 |  8810 | `{` |
|     5 |  8811 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  8812 | `	ph7_value *pObj;` |
|     5 |  8813 | `	sxi32 rc = SosObjectArg(pCtx,zMethod,nArg,apArg,&pObj);` |
|     5 |  8814 | `	if( rc != PH7_OK ){` |
|   ! 0 |  8815 | `		return rc;` |
|     - |  8816 | `	}` |
|     5 |  8817 | `	rc = SosDetach(pCtx,pThis,pObj,0);` |
|     5 |  8818 | `	if( rc != PH7_OK ){` |
|   ! 0 |  8819 | `		return rc;` |
|     - |  8820 | `	}` |
|     5 |  8821 | `	SosRewind(pCtx->pVm,pThis);` |
|     5 |  8822 | `	return PH7_OK;` |
|     3 |  8823 | `}` |
|   ! 0 |  8824 | `static int vm_builtin_SplObjectStorage_detach(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 |  8825 | `{` |
|   ! 0 |  8826 | `	return SosDetachMethod(pCtx,"detach",nArg,apArg);` |
|   ! 0 |  8827 | `}` |
|     4 |  8828 | `static int vm_builtin_SplObjectStorage_offsetUnset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8829 | `{` |
|     5 |  8830 | `	return SosDetachMethod(pCtx,"offsetUnset",nArg,apArg);` |
|     1 |  8831 | `}` |
|    16 |  8832 | `static sxi32 SosContainsMethod(ph7_context *pCtx,const char *zMethod,int nArg,ph7_value **apArg)` |
|     1 |  8833 | `{` |
|     - |  8834 | `	ph7_value *pObj;` |
|    17 |  8835 | `	int bFound = 0;` |
|    17 |  8836 | `	sxi32 rc = SosObjectArg(pCtx,zMethod,nArg,apArg,&pObj);` |
|    17 |  8837 | `	if( rc != PH7_OK ){` |
|     3 |  8838 | `		return rc;` |
|     - |  8839 | `	}` |
|    15 |  8840 | `	rc = SosContains(pCtx,PH7_ContextThis(pCtx),pObj,&bFound);` |
|    15 |  8841 | `	if( rc != PH7_OK ){` |
|   ! 0 |  8842 | `		return rc;` |
|     - |  8843 | `	}` |
|    15 |  8844 | `	ph7_result_bool(pCtx,bFound);` |
|    15 |  8845 | `	return PH7_OK;` |
|     9 |  8846 | `}` |
|     6 |  8847 | `static int vm_builtin_SplObjectStorage_contains(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8848 | `{` |
|     7 |  8849 | `	return SosContainsMethod(pCtx,"contains",nArg,apArg);` |
|     1 |  8850 | `}` |
|    10 |  8851 | `static int vm_builtin_SplObjectStorage_offsetExists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8852 | `{` |
|    11 |  8853 | `	return SosContainsMethod(pCtx,"offsetExists",nArg,apArg);` |
|     1 |  8854 | `}` |
|     - |  8855 | ``/* php's offsetGet: the info, or `Object not found` -- NOT null, and not false. */`` |
|    28 |  8856 | `static int vm_builtin_SplObjectStorage_offsetGet(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8857 | `{` |
|    29 |  8858 | `	ph7_vm *pVm = pCtx->pVm;` |
|    29 |  8859 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  8860 | `	ph7_hashmap *pMap;` |
|    29 |  8861 | `	ph7_hashmap_node *pNode = 0;` |
|     - |  8862 | `	ph7_value sObj,sKey,*pObj,*pInf;` |
|    29 |  8863 | `	sxi32 rc = SosObjectArg(pCtx,"offsetGet",nArg,apArg,&pObj);` |
|    29 |  8864 | `	if( rc != PH7_OK ){` |
|     5 |  8865 | `		return rc;` |
|     - |  8866 | `	}` |
|    25 |  8867 | `	PH7_MemObjInit(pVm,&sObj);` |
|    25 |  8868 | `	PH7_MemObjInit(pVm,&sKey);` |
|    25 |  8869 | `	PH7_MemObjStore(pObj,&sObj);` |
|    25 |  8870 | `	rc = SosKey(pCtx,pThis,&sObj,&sKey);` |
|    25 |  8871 | `	if( rc == PH7_OK ){` |
|    25 |  8872 | `		pMap = SosMap(pVm,pThis);` |
|    25 |  8873 | `		if( pMap == 0 \|\| PH7_HashmapLookup(pMap,&sKey,&pNode) != SXRET_OK ){` |
|     5 |  8874 | `			rc = PH7_VmThrowException(pCtx,"UnexpectedValueException","Object not found");` |
|     3 |  8875 | `		}else{` |
|    21 |  8876 | `			pInf = SosPart(HashmapExtractNodeValue(pNode),"inf");` |
|    21 |  8877 | `			if( pInf ){` |
|    21 |  8878 | `				ph7_result_value(pCtx,pInf);` |
|    11 |  8879 | `			}else{` |
|   ! 0 |  8880 | `				ph7_result_null(pCtx);` |
|     - |  8881 | `			}` |
|     - |  8882 | `		}` |
|    12 |  8883 | `	}` |
|    25 |  8884 | `	PH7_MemObjRelease(&sObj);` |
|    25 |  8885 | `	PH7_MemObjRelease(&sKey);` |
|    25 |  8886 | `	return rc;` |
|    15 |  8887 | `}` |
|     - |  8888 | `/*` |
|     - |  8889 | ` * php's addAll: attach every pair of the other storage, then reset the INDEX only` |
|     - |  8890 | ` * -- the position is deliberately left where it stood, which is why an insert can` |
|     - |  8891 | ` * revive a walk that had run out.` |
|     - |  8892 | ` */` |
|     8 |  8893 | `static int vm_builtin_SplObjectStorage_addAll(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8894 | `{` |
|     9 |  8895 | `	ph7_vm *pVm = pCtx->pVm;` |
|     9 |  8896 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     9 |  8897 | `	ph7_class_instance *pOther = SosOther(nArg,apArg);` |
|     - |  8898 | `	ph7_hashmap *pMap;` |
|     - |  8899 | `	ph7_value sSnap;` |
|     - |  8900 | `	sxi64 i,n;` |
|     9 |  8901 | `	sxi32 rc = PH7_OK;` |
|     9 |  8902 | `	if( pOther == 0 \|\| pThis == 0 ){` |
|   ! 0 |  8903 | `		return PH7_OK;` |
|     - |  8904 | `	}` |
|     9 |  8905 | `	PH7_MemObjInit(pVm,&sSnap);` |
|     9 |  8906 | `	if( SosSnapshot(pVm,pOther,&sSnap) != SXRET_OK ){` |
|   ! 0 |  8907 | `		PH7_MemObjRelease(&sSnap);` |
|   ! 0 |  8908 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  8909 | `	}` |
|     9 |  8910 | `	n = (sxi64)((ph7_hashmap *)sSnap.x.pOther)->nEntry;` |
|    21 |  8911 | `	for( i = 0 ; i < n && rc == PH7_OK ; ++i ){` |
|    13 |  8912 | `		ph7_value *pPair = SosSnapAt(&sSnap,i);` |
|    13 |  8913 | `		ph7_value *pObj = SosPart(pPair,"obj");` |
|    13 |  8914 | `		ph7_value *pInf = SosPart(pPair,"inf");` |
|    13 |  8915 | `		if( pObj ){` |
|    13 |  8916 | `			rc = SosAttach(pCtx,pThis,pObj,pInf);` |
|     6 |  8917 | `		}` |
|     7 |  8918 | `	}` |
|     9 |  8919 | `	PH7_MemObjRelease(&sSnap);` |
|     9 |  8920 | `	if( rc != PH7_OK ){` |
|   ! 0 |  8921 | `		return rc;` |
|     - |  8922 | `	}` |
|     9 |  8923 | `	PH7_NativeSetAttrInt(pVm,pThis,SOS_I,0);` |
|     9 |  8924 | `	pMap = SosMap(pVm,pThis);` |
|     9 |  8925 | `	ph7_result_int64(pCtx,pMap ? (ph7_int64)pMap->nEntry : 0);` |
|     9 |  8926 | `	return PH7_OK;` |
|     5 |  8927 | `}` |
|     - |  8928 | `/* php's removeAll: detach everything the other storage holds, then RESTART the walk. */` |
|     2 |  8929 | `static int vm_builtin_SplObjectStorage_removeAll(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8930 | `{` |
|     3 |  8931 | `	ph7_vm *pVm = pCtx->pVm;` |
|     3 |  8932 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     3 |  8933 | `	ph7_class_instance *pOther = SosOther(nArg,apArg);` |
|     - |  8934 | `	ph7_hashmap *pMap;` |
|     - |  8935 | `	ph7_value sSnap;` |
|     - |  8936 | `	sxi64 i,n;` |
|     3 |  8937 | `	sxi32 rc = PH7_OK;` |
|     3 |  8938 | `	if( pOther == 0 \|\| pThis == 0 ){` |
|   ! 0 |  8939 | `		return PH7_OK;` |
|     - |  8940 | `	}` |
|     3 |  8941 | `	PH7_MemObjInit(pVm,&sSnap);` |
|     3 |  8942 | `	if( SosSnapshot(pVm,pOther,&sSnap) != SXRET_OK ){` |
|   ! 0 |  8943 | `		PH7_MemObjRelease(&sSnap);` |
|   ! 0 |  8944 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  8945 | `	}` |
|     3 |  8946 | `	n = (sxi64)((ph7_hashmap *)sSnap.x.pOther)->nEntry;` |
|     9 |  8947 | `	for( i = 0 ; i < n && rc == PH7_OK ; ++i ){` |
|     7 |  8948 | `		ph7_value *pObj = SosPart(SosSnapAt(&sSnap,i),"obj");` |
|     7 |  8949 | `		if( pObj ){` |
|     7 |  8950 | `			rc = SosDetach(pCtx,pThis,pObj,0);` |
|     3 |  8951 | `		}` |
|     4 |  8952 | `	}` |
|     3 |  8953 | `	PH7_MemObjRelease(&sSnap);` |
|     3 |  8954 | `	if( rc != PH7_OK ){` |
|   ! 0 |  8955 | `		return rc;` |
|     - |  8956 | `	}` |
|     3 |  8957 | `	SosRewind(pVm,pThis);` |
|     3 |  8958 | `	pMap = SosMap(pVm,pThis);` |
|     3 |  8959 | `	ph7_result_int64(pCtx,pMap ? (ph7_int64)pMap->nEntry : 0);` |
|     3 |  8960 | `	return PH7_OK;` |
|     2 |  8961 | `}` |
|     - |  8962 | `/* php's removeAllExcept: the INTERSECTION, walked over this storage's own pairs. */` |
|     2 |  8963 | `static int vm_builtin_SplObjectStorage_removeAllExcept(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8964 | `{` |
|     3 |  8965 | `	ph7_vm *pVm = pCtx->pVm;` |
|     3 |  8966 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     3 |  8967 | `	ph7_class_instance *pOther = SosOther(nArg,apArg);` |
|     - |  8968 | `	ph7_hashmap *pMap;` |
|     - |  8969 | `	ph7_value sSnap;` |
|     - |  8970 | `	sxi64 i,n;` |
|     3 |  8971 | `	sxi32 rc = PH7_OK;` |
|     3 |  8972 | `	if( pOther == 0 \|\| pThis == 0 ){` |
|   ! 0 |  8973 | `		return PH7_OK;` |
|     - |  8974 | `	}` |
|     3 |  8975 | `	PH7_MemObjInit(pVm,&sSnap);` |
|     3 |  8976 | `	if( SosSnapshot(pVm,pThis,&sSnap) != SXRET_OK ){` |
|   ! 0 |  8977 | `		PH7_MemObjRelease(&sSnap);` |
|   ! 0 |  8978 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  8979 | `	}` |
|     3 |  8980 | `	n = (sxi64)((ph7_hashmap *)sSnap.x.pOther)->nEntry;` |
|     9 |  8981 | `	for( i = 0 ; i < n && rc == PH7_OK ; ++i ){` |
|     7 |  8982 | `		ph7_value *pObj = SosPart(SosSnapAt(&sSnap,i),"obj");` |
|     7 |  8983 | `		int bFound = 0;` |
|     7 |  8984 | `		if( pObj == 0 ){` |
|   ! 0 |  8985 | `			continue;` |
|     - |  8986 | `		}` |
|     7 |  8987 | `		rc = SosContains(pCtx,pOther,pObj,&bFound);` |
|     7 |  8988 | `		if( rc == PH7_OK && !bFound ){` |
|     5 |  8989 | `			pObj = SosPart(SosSnapAt(&sSnap,i),"obj");   /* re-resolved: getHash may have run */` |
|     5 |  8990 | `			if( pObj ){` |
|     5 |  8991 | `				rc = SosDetach(pCtx,pThis,pObj,0);` |
|     2 |  8992 | `			}` |
|     2 |  8993 | `		}` |
|     4 |  8994 | `	}` |
|     3 |  8995 | `	PH7_MemObjRelease(&sSnap);` |
|     3 |  8996 | `	if( rc != PH7_OK ){` |
|   ! 0 |  8997 | `		return rc;` |
|     - |  8998 | `	}` |
|     3 |  8999 | `	SosRewind(pVm,pThis);` |
|     3 |  9000 | `	pMap = SosMap(pVm,pThis);` |
|     3 |  9001 | `	ph7_result_int64(pCtx,pMap ? (ph7_int64)pMap->nEntry : 0);` |
|     3 |  9002 | `	return PH7_OK;` |
|     2 |  9003 | `}` |
|     - |  9004 | `/*` |
|     - |  9005 | ` * php's count(): COUNT_RECURSIVE is accepted and changes nothing -- the storage` |
|     - |  9006 | ` * holds C structs rather than zvals there, so nothing recurses.` |
|     - |  9007 | ` */` |
|    24 |  9008 | `static int vm_builtin_SplObjectStorage_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9009 | `{` |
|    25 |  9010 | `	ph7_hashmap *pMap = SosMap(pCtx->pVm,PH7_ContextThis(pCtx));` |
|    12 |  9011 | `	SXUNUSED(nArg);` |
|    12 |  9012 | `	SXUNUSED(apArg);` |
|    25 |  9013 | `	ph7_result_int64(pCtx,pMap ? (ph7_int64)pMap->nEntry : 0);` |
|    25 |  9014 | `	return PH7_OK;` |
|     1 |  9015 | `}` |
|     4 |  9016 | `static int vm_builtin_SplObjectStorage_getHash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9017 | `{` |
|     - |  9018 | `	ph7_value *pObj;` |
|     5 |  9019 | `	sxi32 rc = SosObjectArg(pCtx,"getHash",nArg,apArg,&pObj);` |
|     5 |  9020 | `	if( rc != PH7_OK ){` |
|   ! 0 |  9021 | `		return rc;` |
|     - |  9022 | `	}` |
|     - |  9023 | `	/* php's getHash() IS php_spl_object_hash(), the same one the function answers. */` |
|     5 |  9024 | `	return vm_builtin_spl_object_hash(pCtx,nArg,apArg);` |
|     3 |  9025 | `}` |
|    28 |  9026 | `static int vm_builtin_SplObjectStorage_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9027 | `{` |
|    14 |  9028 | `	SXUNUSED(nArg);` |
|    14 |  9029 | `	SXUNUSED(apArg);` |
|    29 |  9030 | `	SosRewind(pCtx->pVm,PH7_ContextThis(pCtx));` |
|    29 |  9031 | `	return PH7_OK;` |
|     1 |  9032 | `}` |
|    52 |  9033 | `static int vm_builtin_SplObjectStorage_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9034 | `{` |
|    53 |  9035 | `	ph7_hashmap *pMap = SosMap(pCtx->pVm,PH7_ContextThis(pCtx));` |
|    26 |  9036 | `	SXUNUSED(nArg);` |
|    26 |  9037 | `	SXUNUSED(apArg);` |
|    53 |  9038 | `	ph7_result_bool(pCtx,pMap && pMap->pCur ? 1 : 0);` |
|    53 |  9039 | `	return PH7_OK;` |
|     1 |  9040 | `}` |
|     - |  9041 | `/* php's key() is the INDEX, a counter of its own: next() advances it past the end` |
|     - |  9042 | ` * too, and only rewind()/detach()/addAll() and friends put it back to zero. */` |
|    40 |  9043 | `static int vm_builtin_SplObjectStorage_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9044 | `{` |
|    20 |  9045 | `	SXUNUSED(nArg);` |
|    20 |  9046 | `	SXUNUSED(apArg);` |
|    41 |  9047 | `	ph7_result_int64(pCtx,PH7_NativeAttrInt(PH7_ContextThis(pCtx),SOS_I));` |
|    41 |  9048 | `	return PH7_OK;` |
|     1 |  9049 | `}` |
|     - |  9050 | `/* php RAISES here rather than answering null: the chunk's null was a wrong answer` |
|     - |  9051 | `` * every `foreach` hid, because a foreach never asks past valid(). */`` |
|    46 |  9052 | `static int vm_builtin_SplObjectStorage_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9053 | `{` |
|    47 |  9054 | `	ph7_value *pObj = SosPart(SosCurrentPair(pCtx->pVm,PH7_ContextThis(pCtx)),"obj");` |
|    23 |  9055 | `	SXUNUSED(nArg);` |
|    23 |  9056 | `	SXUNUSED(apArg);` |
|    47 |  9057 | `	if( pObj == 0 ){` |
|     3 |  9058 | `		return PH7_VmThrowException(pCtx,"RuntimeException",` |
|     - |  9059 | `			"Called current() on invalid iterator");` |
|     - |  9060 | `	}` |
|    45 |  9061 | `	ph7_result_value(pCtx,pObj);` |
|    45 |  9062 | `	return PH7_OK;` |
|    24 |  9063 | `}` |
|    44 |  9064 | `static int vm_builtin_SplObjectStorage_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9065 | `{` |
|    45 |  9066 | `	ph7_vm *pVm = pCtx->pVm;` |
|    45 |  9067 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    45 |  9068 | `	ph7_hashmap *pMap = SosMap(pVm,pThis);` |
|    22 |  9069 | `	SXUNUSED(nArg);` |
|    22 |  9070 | `	SXUNUSED(apArg);` |
|    45 |  9071 | `	if( pMap && pMap->pCur ){` |
|    45 |  9072 | `		pMap->pCur = pMap->pCur->pPrev;` |
|    22 |  9073 | `	}` |
|    45 |  9074 | `	PH7_NativeSetAttrInt(pVm,pThis,SOS_I,PH7_NativeAttrInt(pThis,SOS_I) + 1);` |
|    45 |  9075 | `	return PH7_OK;` |
|     1 |  9076 | `}` |
|     - |  9077 | `/* php's getInfo(): null past the end, where current() raises. */` |
|    38 |  9078 | `static int vm_builtin_SplObjectStorage_getInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9079 | `{` |
|    39 |  9080 | `	ph7_value *pInf = SosPart(SosCurrentPair(pCtx->pVm,PH7_ContextThis(pCtx)),"inf");` |
|    19 |  9081 | `	SXUNUSED(nArg);` |
|    19 |  9082 | `	SXUNUSED(apArg);` |
|    39 |  9083 | `	if( pInf ){` |
|    37 |  9084 | `		ph7_result_value(pCtx,pInf);` |
|    19 |  9085 | `	}else{` |
|     3 |  9086 | `		ph7_result_null(pCtx);` |
|     - |  9087 | `	}` |
|    39 |  9088 | `	return PH7_OK;` |
|     1 |  9089 | `}` |
|     2 |  9090 | `static int vm_builtin_SplObjectStorage_setInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9091 | `{` |
|     3 |  9092 | `	ph7_vm *pVm = pCtx->pVm;` |
|     3 |  9093 | `	ph7_hashmap *pMap = SosMap(pVm,PH7_ContextThis(pCtx));` |
|     - |  9094 | `	ph7_value *pPair;` |
|     3 |  9095 | `	if( pMap == 0 \|\| pMap->pCur == 0 \|\| nArg < 1 ){` |
|   ! 0 |  9096 | `		return PH7_OK;   /* php returns without touching anything */` |
|     - |  9097 | `	}` |
|     3 |  9098 | `	pPair = SosPairForWrite(pVm,pMap->pCur);` |
|     3 |  9099 | `	if( pPair ){` |
|     3 |  9100 | `		SosSetPart(pVm,pPair,"inf",apArg[0]);` |
|     1 |  9101 | `	}` |
|     3 |  9102 | `	return PH7_OK;` |
|     2 |  9103 | `}` |
|     - |  9104 | `/*` |
|     - |  9105 | ` * php's seek(): a position outside the storage is an OutOfBoundsException, and` |
|     - |  9106 | ` * the index follows the position exactly (php walks its hash cursor either way` |
|     - |  9107 | ` * and counts; the destination is the same).` |
|     - |  9108 | ` */` |
|     4 |  9109 | `static int vm_builtin_SplObjectStorage_seek(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9110 | `{` |
|     5 |  9111 | `	ph7_vm *pVm = pCtx->pVm;` |
|     5 |  9112 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     5 |  9113 | `	ph7_hashmap *pMap = SosMap(pVm,pThis);` |
|     5 |  9114 | `	ph7_int64 iPos = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;` |
|     - |  9115 | `	ph7_int64 i;` |
|     5 |  9116 | `	if( pMap == 0 ){` |
|   ! 0 |  9117 | `		return PH7_OK;` |
|     - |  9118 | `	}` |
|     5 |  9119 | `	if( iPos < 0 \|\| iPos >= (ph7_int64)pMap->nEntry ){` |
|     4 |  9120 | `		return PH7_VmThrowException(pCtx,"OutOfBoundsException",` |
|     1 |  9121 | `			"Seek position %qd is out of range",iPos);` |
|     - |  9122 | `	}` |
|     3 |  9123 | `	pMap->pCur = pMap->pFirst;` |
|     7 |  9124 | `	for( i = 0 ; i < iPos && pMap->pCur ; ++i ){` |
|     5 |  9125 | `		pMap->pCur = pMap->pCur->pPrev;` |
|     3 |  9126 | `	}` |
|     3 |  9127 | `	PH7_NativeSetAttrInt(pVm,pThis,SOS_I,iPos);` |
|     3 |  9128 | `	return PH7_OK;` |
|     3 |  9129 | `}` |
|     - |  9130 | `/*` |
|     - |  9131 | ` * php's get_debug_info: ONE entry, the storage, under its own MANGLED private key` |
|     - |  9132 | `` * -- `["storage":"SplObjectStorage":private]` on screen. The pairs are re-indexed`` |
|     - |  9133 | ` * from zero and shown as {obj, inf}, which is the shape stored here already.` |
|     - |  9134 | `` * `__debugInfo()` is the same array, reachable by name because php declares it.`` |
|     - |  9135 | ` */` |
|     8 |  9136 | `static sxi32 SosFillDebug(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut)` |
|     1 |  9137 | `{` |
|     9 |  9138 | `	ph7_hashmap *pMap = SosMap(pVm,pThis);` |
|     - |  9139 | `	ph7_hashmap_node *pNode;` |
|     - |  9140 | `	ph7_value sKey,sList;` |
|     - |  9141 | `	sxu32 n;` |
|     9 |  9142 | `	PH7_MemObjInit(pVm,&sList);` |
|     9 |  9143 | `	if( PH7_MemObjToHashmap(&sList) != SXRET_OK ){` |
|   ! 0 |  9144 | `		PH7_MemObjRelease(&sList);` |
|   ! 0 |  9145 | `		return SXERR_MEM;` |
|     - |  9146 | `	}` |
|     9 |  9147 | `	if( pMap ){` |
|     9 |  9148 | `		pNode = pMap->pFirst;` |
|    15 |  9149 | `		for( n = 0 ; n < pMap->nEntry && pNode ; ++n ){` |
|     7 |  9150 | `			ph7_value *pPair = HashmapExtractNodeValue(pNode);` |
|     7 |  9151 | `			if( pPair ){` |
|     7 |  9152 | `				ph7_array_add_elem(&sList,0,pPair);` |
|     3 |  9153 | `			}` |
|     7 |  9154 | `			pNode = pNode->pPrev;` |
|     4 |  9155 | `		}` |
|     4 |  9156 | `	}` |
|     9 |  9157 | `	PH7_MemObjInitFromString(pVm,&sKey,0);` |
|     9 |  9158 | `	PH7_MemObjStringAppend(&sKey,"\0SplObjectStorage\0storage",` |
|     - |  9159 | `		sizeof("\0SplObjectStorage\0storage")-1);` |
|     9 |  9160 | `	ph7_array_add_elem(pOut,&sKey,&sList);` |
|     9 |  9161 | `	PH7_MemObjRelease(&sKey);` |
|     9 |  9162 | `	PH7_MemObjRelease(&sList);` |
|     9 |  9163 | `	return PH7_OK;` |
|     5 |  9164 | `}` |
|     8 |  9165 | `static sxi32 SosPresent(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)` |
|     1 |  9166 | `{` |
|     9 |  9167 | `	if( !bDebug ){` |
|     5 |  9168 | `		return PH7_OK;   /* php's (array) cast and var_export show nothing */` |
|     - |  9169 | `	}` |
|     5 |  9170 | `	return SosFillDebug(pVm,pThis,pOut);` |
|     5 |  9171 | `}` |
|     4 |  9172 | `static int vm_builtin_SplObjectStorage_debugInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9173 | `{` |
|     5 |  9174 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  9175 | `	ph7_value sOut;` |
|     2 |  9176 | `	SXUNUSED(nArg);` |
|     2 |  9177 | `	SXUNUSED(apArg);` |
|     5 |  9178 | `	PH7_MemObjInit(pVm,&sOut);` |
|     5 |  9179 | `	if( PH7_MemObjToHashmap(&sOut) != SXRET_OK ){` |
|   ! 0 |  9180 | `		PH7_MemObjRelease(&sOut);` |
|   ! 0 |  9181 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  9182 | `	}` |
|     5 |  9183 | `	SosFillDebug(pVm,PH7_ContextThis(pCtx),&sOut);` |
|     5 |  9184 | `	ph7_result_value(pCtx,&sOut);` |
|     5 |  9185 | `	PH7_MemObjRelease(&sOut);` |
|     5 |  9186 | `	return PH7_OK;` |
|     3 |  9187 | `}` |
|     - |  9188 | `/*` |
|     - |  9189 | ` * php's __serialize(): [[obj, inf, obj, inf, …], members]. This is what serialize()` |
|     - |  9190 | ` * actually uses; the Serializable pair below is the legacy format nothing else in` |
|     - |  9191 | ` * php reads or writes. The members slot is the instance's own properties — empty for` |
|     - |  9192 | ` * a bare SplObjectStorage, a SUBCLASS's declared slots when there is one.` |
|     - |  9193 | ` */` |
|    12 |  9194 | `static int vm_builtin_SplObjectStorage_serializeMagic(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9195 | `{` |
|    13 |  9196 | `	ph7_vm *pVm = pCtx->pVm;` |
|    13 |  9197 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    13 |  9198 | `	ph7_hashmap *pMap = SosMap(pVm,pThis);` |
|     - |  9199 | `	ph7_hashmap_node *pNode;` |
|     - |  9200 | `	ph7_value sOut,sFlat,sMembers;` |
|     - |  9201 | `	sxu32 n;` |
|     6 |  9202 | `	SXUNUSED(nArg);` |
|     6 |  9203 | `	SXUNUSED(apArg);` |
|    13 |  9204 | `	PH7_MemObjInit(pVm,&sOut);` |
|    13 |  9205 | `	PH7_MemObjInit(pVm,&sFlat);` |
|    12 |  9206 | `	if( SplMembersOf(pVm,pThis,&sMembers) != SXRET_OK` |
|    12 |  9207 | `	 \|\| PH7_MemObjToHashmap(&sOut) != SXRET_OK` |
|    13 |  9208 | `	 \|\| PH7_MemObjToHashmap(&sFlat) != SXRET_OK ){` |
|   ! 0 |  9209 | `		PH7_MemObjRelease(&sOut);` |
|   ! 0 |  9210 | `		PH7_MemObjRelease(&sFlat);` |
|   ! 0 |  9211 | `		PH7_MemObjRelease(&sMembers);` |
|   ! 0 |  9212 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  9213 | `	}` |
|    13 |  9214 | `	if( pMap ){` |
|    13 |  9215 | `		pNode = pMap->pFirst;` |
|    29 |  9216 | `		for( n = 0 ; n < pMap->nEntry && pNode ; ++n ){` |
|    17 |  9217 | `			ph7_value *pPair = HashmapExtractNodeValue(pNode);` |
|    17 |  9218 | `			ph7_value *pObj = SosPart(pPair,"obj");` |
|    17 |  9219 | `			ph7_value *pInf = SosPart(pPair,"inf");` |
|    17 |  9220 | `			if( pObj ){` |
|    17 |  9221 | `				ph7_array_add_elem(&sFlat,0,pObj);` |
|    17 |  9222 | `				if( pInf ){` |
|    17 |  9223 | `					ph7_array_add_elem(&sFlat,0,pInf);` |
|     8 |  9224 | `				}` |
|     8 |  9225 | `			}` |
|    17 |  9226 | `			pNode = pNode->pPrev;` |
|     9 |  9227 | `		}` |
|     6 |  9228 | `	}` |
|    13 |  9229 | `	ph7_array_add_elem(&sOut,0,&sFlat);` |
|    13 |  9230 | `	ph7_array_add_elem(&sOut,0,&sMembers);` |
|    13 |  9231 | `	ph7_result_value(pCtx,&sOut);` |
|    13 |  9232 | `	PH7_MemObjRelease(&sOut);` |
|    13 |  9233 | `	PH7_MemObjRelease(&sFlat);` |
|    13 |  9234 | `	PH7_MemObjRelease(&sMembers);` |
|    13 |  9235 | `	return PH7_OK;` |
|     7 |  9236 | `}` |
|     2 |  9237 | `static sxi32 SosIllTyped(ph7_context *pCtx)` |
|     1 |  9238 | `{` |
|     3 |  9239 | `	return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     - |  9240 | `		"Incomplete or ill-typed serialization data");` |
|     1 |  9241 | `}` |
|    12 |  9242 | `static int vm_builtin_SplObjectStorage_unserializeMagic(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9243 | `{` |
|    13 |  9244 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  9245 | `	ph7_hashmap *pFlat;` |
|    13 |  9246 | `	ph7_hashmap_node *pNode = 0;` |
|     - |  9247 | `	ph7_value *pStorage,*pMembers;` |
|     - |  9248 | `	sxi64 i,n;` |
|    13 |  9249 | `	sxi32 rc = PH7_OK;` |
|    13 |  9250 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_HASHMAP) == 0 \|\| pThis == 0 ){` |
|   ! 0 |  9251 | `		return SosIllTyped(pCtx);` |
|     - |  9252 | `	}` |
|    13 |  9253 | `	if( HashmapLookupIntKey((ph7_hashmap *)apArg[0]->x.pOther,0,&pNode) != SXRET_OK ){` |
|   ! 0 |  9254 | `		return SosIllTyped(pCtx);` |
|     - |  9255 | `	}` |
|    13 |  9256 | `	pStorage = HashmapExtractNodeValue(pNode);` |
|    13 |  9257 | `	pNode = 0;` |
|    13 |  9258 | `	if( HashmapLookupIntKey((ph7_hashmap *)apArg[0]->x.pOther,1,&pNode) != SXRET_OK ){` |
|   ! 0 |  9259 | `		return SosIllTyped(pCtx);` |
|     - |  9260 | `	}` |
|    13 |  9261 | `	pMembers = HashmapExtractNodeValue(pNode);` |
|    12 |  9262 | `	if( pStorage == 0 \|\| (pStorage->iFlags & MEMOBJ_HASHMAP) == 0` |
|    12 |  9263 | `	 \|\| pMembers == 0 \|\| (pMembers->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     3 |  9264 | `		return SosIllTyped(pCtx);` |
|     - |  9265 | `	}` |
|    11 |  9266 | `	pFlat = (ph7_hashmap *)pStorage->x.pOther;` |
|    11 |  9267 | `	n = (sxi64)pFlat->nEntry;` |
|    11 |  9268 | `	if( n % 2 != 0 ){` |
|     3 |  9269 | `		return PH7_VmThrowException(pCtx,"UnexpectedValueException","Odd number of elements");` |
|     - |  9270 | `	}` |
|    19 |  9271 | `	for( i = 0 ; i < n && rc == PH7_OK ; i += 2 ){` |
|     - |  9272 | `		ph7_value *pObj,*pInf;` |
|    13 |  9273 | `		pNode = 0;` |
|    13 |  9274 | `		if( HashmapLookupIntKey(pFlat,i,&pNode) != SXRET_OK ){` |
|   ! 0 |  9275 | `			return SosIllTyped(pCtx);` |
|     - |  9276 | `		}` |
|    13 |  9277 | `		pObj = HashmapExtractNodeValue(pNode);` |
|    13 |  9278 | `		if( pObj == 0 \|\| (pObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     3 |  9279 | `			return PH7_VmThrowException(pCtx,"UnexpectedValueException","Non-object key");` |
|     - |  9280 | `		}` |
|    11 |  9281 | `		pNode = 0;` |
|    11 |  9282 | `		pInf = HashmapLookupIntKey(pFlat,i+1,&pNode) == SXRET_OK` |
|    10 |  9283 | `			? HashmapExtractNodeValue(pNode) : 0;` |
|    11 |  9284 | `		rc = SosAttach(pCtx,pThis,pObj,pInf);` |
|     6 |  9285 | `	}` |
|     7 |  9286 | `	if( rc == PH7_OK ){` |
|     7 |  9287 | `		SplMembersLoad(pThis,pMembers);` |
|     3 |  9288 | `	}` |
|     7 |  9289 | `	return rc;` |
|     7 |  9290 | `}` |
|     - |  9291 | `/*` |
|     - |  9292 | ` * php's Serializable pair, kept because the interface is still declared. The` |
|     - |  9293 | `` * format is `x:` + the serialized COUNT, then one `<obj>,<inf>;` per element, then`` |
|     - |  9294 | `` * `m:` + the serialized members -- and php writes it through ONE serializer state,`` |
|     - |  9295 | `` * so an object that appears twice becomes an `r:` back-reference there and a`` |
|     - |  9296 | ` * second copy here (the scope policy; the DLL's legacy pair has the same shape).` |
|     - |  9297 | ` */` |
|     4 |  9298 | `static int vm_builtin_SplObjectStorage_serialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9299 | `{` |
|     5 |  9300 | `	ph7_vm *pVm = pCtx->pVm;` |
|     5 |  9301 | `	ph7_hashmap *pMap = SosMap(pVm,PH7_ContextThis(pCtx));` |
|     - |  9302 | `	ph7_hashmap_node *pNode;` |
|     - |  9303 | `	SyBlob sOut;` |
|     - |  9304 | `	ph7_value sVal,*apCall[1];` |
|     - |  9305 | `	sxu32 n;` |
|     2 |  9306 | `	SXUNUSED(nArg);` |
|     2 |  9307 | `	SXUNUSED(apArg);` |
|     5 |  9308 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|     5 |  9309 | `	SyBlobAppend(&sOut,"x:",sizeof("x:")-1);` |
|     5 |  9310 | `	PH7_MemObjInitFromInt(pVm,&sVal,pMap ? (sxi64)pMap->nEntry : 0);` |
|     5 |  9311 | `	apCall[0] = &sVal;` |
|     5 |  9312 | `	SplSerializeInto(pCtx,apCall,&sOut);` |
|     5 |  9313 | `	PH7_MemObjRelease(&sVal);` |
|     5 |  9314 | `	if( pMap ){` |
|     5 |  9315 | `		pNode = pMap->pFirst;` |
|    13 |  9316 | `		for( n = 0 ; n < pMap->nEntry && pNode ; ++n ){` |
|     9 |  9317 | `			ph7_value *pPair = HashmapExtractNodeValue(pNode);` |
|     9 |  9318 | `			ph7_value *pObj = SosPart(pPair,"obj");` |
|     9 |  9319 | `			ph7_value *pInf = SosPart(pPair,"inf");` |
|     - |  9320 | `			ph7_value sNull;` |
|     9 |  9321 | `			if( pObj ){` |
|     9 |  9322 | `				apCall[0] = pObj;` |
|     9 |  9323 | `				SplSerializeInto(pCtx,apCall,&sOut);` |
|     9 |  9324 | `				SyBlobAppend(&sOut,",",1);` |
|     9 |  9325 | `				PH7_MemObjInit(pVm,&sNull);` |
|     9 |  9326 | `				apCall[0] = pInf ? pInf : &sNull;` |
|     9 |  9327 | `				SplSerializeInto(pCtx,apCall,&sOut);` |
|     9 |  9328 | `				PH7_MemObjRelease(&sNull);` |
|     9 |  9329 | `				SyBlobAppend(&sOut,";",1);` |
|     4 |  9330 | `			}` |
|     9 |  9331 | `			pNode = pNode->pPrev;` |
|     5 |  9332 | `		}` |
|     2 |  9333 | `	}` |
|     5 |  9334 | `	SyBlobAppend(&sOut,"m:",sizeof("m:")-1);` |
|     5 |  9335 | `	PH7_MemObjInit(pVm,&sVal);` |
|     5 |  9336 | `	if( PH7_MemObjToHashmap(&sVal) == SXRET_OK ){` |
|     5 |  9337 | `		apCall[0] = &sVal;` |
|     5 |  9338 | `		SplSerializeInto(pCtx,apCall,&sOut);` |
|     2 |  9339 | `	}` |
|     5 |  9340 | `	PH7_MemObjRelease(&sVal);` |
|     - |  9341 | `	/* ph7_result_string APPENDS too, and pRet still holds the last nested answer` |
|     - |  9342 | `	 * (rule 54) -- drop it before writing this one. */` |
|     5 |  9343 | `	if( pCtx->pRet ){` |
|     5 |  9344 | `		PH7_MemObjRelease(pCtx->pRet);` |
|     2 |  9345 | `	}` |
|     5 |  9346 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|     5 |  9347 | `	SyBlobRelease(&sOut);` |
|     5 |  9348 | `	return PH7_OK;` |
|     1 |  9349 | `}` |
|     - |  9350 | `/* php reports WHERE its parse gave up, in bytes, and every failure below is that` |
|     - |  9351 | ` * one exception. */` |
|     2 |  9352 | `static sxi32 SosOffsetErr(ph7_context *pCtx,int nAt,int nTotal)` |
|     1 |  9353 | `{` |
|     4 |  9354 | `	return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     1 |  9355 | `		"Error at offset %d of %d bytes",nAt,nTotal);` |
|     1 |  9356 | `}` |
|     6 |  9357 | `static int vm_builtin_SplObjectStorage_unserialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9358 | `{` |
|     7 |  9359 | `	ph7_vm *pVm = pCtx->pVm;` |
|     7 |  9360 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  9361 | `	const char *zData;` |
|     7 |  9362 | `	int nData = 0,nAt = 0,nRead = 0;` |
|     - |  9363 | `	ph7_value sVal;` |
|     - |  9364 | `	sxi64 nCount,i;` |
|     - |  9365 | `	sxi32 rc;` |
|     7 |  9366 | `	if( nArg < 1 \|\| pThis == 0 ){` |
|   ! 0 |  9367 | `		return PH7_OK;` |
|     - |  9368 | `	}` |
|     7 |  9369 | `	zData = ph7_value_to_string(apArg[0],&nData);` |
|     7 |  9370 | `	if( nData < 1 ){` |
|     3 |  9371 | `		return PH7_OK;   /* php returns without touching the storage */` |
|     - |  9372 | `	}` |
|     5 |  9373 | `	if( nData < 2 \|\| zData[0] != 'x' \|\| zData[1] != ':' ){` |
|   ! 0 |  9374 | `		return SosOffsetErr(pCtx,zData[0] == 'x' ? 1 : 0,nData);` |
|     - |  9375 | `	}` |
|     5 |  9376 | `	nAt = 2;` |
|     5 |  9377 | `	PH7_MemObjInit(pVm,&sVal);` |
|     5 |  9378 | `	rc = PH7_VmUnserializeOne(pCtx,&zData[nAt],nData - nAt,&nRead,&sVal);` |
|     5 |  9379 | `	if( rc == PH7_EXCEPTION ){` |
|   ! 0 |  9380 | `		PH7_MemObjRelease(&sVal);` |
|   ! 0 |  9381 | `		return rc;` |
|     - |  9382 | `	}` |
|     5 |  9383 | `	if( rc != SXRET_OK \|\| (sVal.iFlags & MEMOBJ_INT) == 0 ){` |
|     - |  9384 | `		/* php reports where its parser STOPPED, which for a well-formed value of` |
|     - |  9385 | `		 * the wrong type is the byte after it. */` |
|     3 |  9386 | `		PH7_MemObjRelease(&sVal);` |
|     3 |  9387 | `		return SosOffsetErr(pCtx,nAt + nRead,nData);` |
|     - |  9388 | `	}` |
|     3 |  9389 | `	nCount = ph7_value_to_int64(&sVal);` |
|     3 |  9390 | `	PH7_MemObjRelease(&sVal);` |
|     3 |  9391 | `	nAt += nRead - 1;   /* php steps back onto the ';' that ends the count */` |
|     3 |  9392 | `	if( nCount < 0 ){` |
|   ! 0 |  9393 | `		return SosOffsetErr(pCtx,nAt,nData);` |
|     - |  9394 | `	}` |
|     9 |  9395 | `	for( i = 0 ; i < nCount ; ++i ){` |
|     - |  9396 | `		ph7_value sObj,sInf;` |
|     7 |  9397 | `		if( nAt >= nData \|\| zData[nAt] != ';' ){` |
|   ! 0 |  9398 | `			return SosOffsetErr(pCtx,nAt,nData);` |
|     - |  9399 | `		}` |
|     7 |  9400 | `		nAt++;` |
|     7 |  9401 | `		if( nAt >= nData \|\| (zData[nAt] != 'O' && zData[nAt] != 'C' && zData[nAt] != 'r') ){` |
|   ! 0 |  9402 | `			return SosOffsetErr(pCtx,nAt,nData);` |
|     - |  9403 | `		}` |
|     7 |  9404 | `		PH7_MemObjInit(pVm,&sObj);` |
|     7 |  9405 | `		PH7_MemObjInit(pVm,&sInf);` |
|     7 |  9406 | `		rc = PH7_VmUnserializeOne(pCtx,&zData[nAt],nData - nAt,&nRead,&sObj);` |
|     7 |  9407 | `		if( rc == PH7_EXCEPTION ){` |
|   ! 0 |  9408 | `			PH7_MemObjRelease(&sObj);` |
|   ! 0 |  9409 | `			PH7_MemObjRelease(&sInf);` |
|   ! 0 |  9410 | `			return rc;` |
|     - |  9411 | `		}` |
|     7 |  9412 | `		if( rc != SXRET_OK \|\| (sObj.iFlags & MEMOBJ_OBJ) == 0 ){` |
|   ! 0 |  9413 | `			PH7_MemObjRelease(&sObj);` |
|   ! 0 |  9414 | `			PH7_MemObjRelease(&sInf);` |
|   ! 0 |  9415 | `			return SosOffsetErr(pCtx,nAt + nRead,nData);` |
|     - |  9416 | `		}` |
|     7 |  9417 | `		nAt += nRead;` |
|     7 |  9418 | `		if( nAt < nData && zData[nAt] == ',' ){` |
|     7 |  9419 | `			nAt++;` |
|     7 |  9420 | `			rc = PH7_VmUnserializeOne(pCtx,&zData[nAt],nData - nAt,&nRead,&sInf);` |
|     7 |  9421 | `			if( rc == PH7_EXCEPTION ){` |
|   ! 0 |  9422 | `				PH7_MemObjRelease(&sObj);` |
|   ! 0 |  9423 | `				PH7_MemObjRelease(&sInf);` |
|   ! 0 |  9424 | `				return rc;` |
|     - |  9425 | `			}` |
|     7 |  9426 | `			if( rc != SXRET_OK ){` |
|   ! 0 |  9427 | `				PH7_MemObjRelease(&sObj);` |
|   ! 0 |  9428 | `				PH7_MemObjRelease(&sInf);` |
|   ! 0 |  9429 | `				return SosOffsetErr(pCtx,nAt + nRead,nData);` |
|     - |  9430 | `			}` |
|     7 |  9431 | `			nAt += nRead;` |
|     3 |  9432 | `		}` |
|     7 |  9433 | `		rc = SosAttach(pCtx,pThis,&sObj,&sInf);` |
|     7 |  9434 | `		PH7_MemObjRelease(&sObj);` |
|     7 |  9435 | `		PH7_MemObjRelease(&sInf);` |
|     7 |  9436 | `		if( rc != PH7_OK ){` |
|   ! 0 |  9437 | `			return rc;` |
|     - |  9438 | `		}` |
|     4 |  9439 | `	}` |
|     3 |  9440 | `	if( nAt >= nData \|\| zData[nAt] != ';' ){` |
|   ! 0 |  9441 | `		return SosOffsetErr(pCtx,nAt,nData);` |
|     - |  9442 | `	}` |
|     3 |  9443 | `	nAt++;` |
|     3 |  9444 | `	if( nAt + 1 >= nData \|\| zData[nAt] != 'm' \|\| zData[nAt+1] != ':' ){` |
|   ! 0 |  9445 | `		return SosOffsetErr(pCtx,nAt < nData && zData[nAt] == 'm' ? nAt + 1 : nAt,nData);` |
|     - |  9446 | `	}` |
|     3 |  9447 | `	nAt += 2;` |
|     3 |  9448 | `	PH7_MemObjInit(pVm,&sVal);` |
|     3 |  9449 | `	rc = PH7_VmUnserializeOne(pCtx,&zData[nAt],nData - nAt,&nRead,&sVal);` |
|     3 |  9450 | `	if( rc == PH7_EXCEPTION ){` |
|   ! 0 |  9451 | `		PH7_MemObjRelease(&sVal);` |
|   ! 0 |  9452 | `		return rc;` |
|     - |  9453 | `	}` |
|     3 |  9454 | `	if( rc != SXRET_OK \|\| (sVal.iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|   ! 0 |  9455 | `		PH7_MemObjRelease(&sVal);` |
|   ! 0 |  9456 | `		return SosOffsetErr(pCtx,nAt + nRead,nData);` |
|     - |  9457 | `	}` |
|     - |  9458 | `	/* php loads the members onto the object here; a native class declares none` |
|     - |  9459 | `	 * that a payload could name and PHL has no dynamic properties to create. */` |
|     3 |  9460 | `	PH7_MemObjRelease(&sVal);` |
|     3 |  9461 | `	return PH7_OK;` |
|     4 |  9462 | `}` |
|     - |  9463 | `/*` |
|     - |  9464 | ` * The declaration. Method ORDER is spl_observer.stub.php's, the two observer` |
|     - |  9465 | ` * interfaces are methodless-but-typed contracts php declares beside it, and` |
|     - |  9466 | ` * seek() is the ONE method php does not mark tentative.` |
|     - |  9467 | ` */` |
|  8445 |  9468 | `static sxi32 VmInstallSplObjectStorage(ph7_vm *pVm)` |
|     5 |  9469 | `{` |
|     - |  9470 | `	static const PH7_NativeMethodDef aObserverMethod[] = {` |
|     - |  9471 | `		{ "update", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "SplSubject $subject", "@void", 0 },` |
|     - |  9472 | `	};` |
|     - |  9473 | `	static const PH7_NativeMethodDef aSubjectMethod[] = {` |
|     - |  9474 | `		{ "attach", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "SplObserver $observer", "@void", 0 },` |
|     - |  9475 | `		{ "detach", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "SplObserver $observer", "@void", 0 },` |
|     - |  9476 | `		{ "notify", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@void", 0 },` |
|     - |  9477 | `	};` |
|     - |  9478 | `	static const PH7_NativePropDef aSosProp[] = {` |
|     - |  9479 | `		{ SOS_S, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  9480 | `		{ SOS_I, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  9481 | `	};` |
|     - |  9482 | `	static const PH7_NativeMethodDef aSosMethod[] = {` |
|     - |  9483 | `		{ "attach",          PH7_MOD_PUBLIC, "object $object, mixed $info = null", "@void",` |
|     - |  9484 | `		  vm_builtin_SplObjectStorage_attach },` |
|     - |  9485 | `		{ "detach",          PH7_MOD_PUBLIC, "object $object", "@void",` |
|     - |  9486 | `		  vm_builtin_SplObjectStorage_detach },` |
|     - |  9487 | `		{ "contains",        PH7_MOD_PUBLIC, "object $object", "@bool",` |
|     - |  9488 | `		  vm_builtin_SplObjectStorage_contains },` |
|     - |  9489 | `		{ "addAll",          PH7_MOD_PUBLIC, "SplObjectStorage $storage", "@int",` |
|     - |  9490 | `		  vm_builtin_SplObjectStorage_addAll },` |
|     - |  9491 | `		{ "removeAll",       PH7_MOD_PUBLIC, "SplObjectStorage $storage", "@int",` |
|     - |  9492 | `		  vm_builtin_SplObjectStorage_removeAll },` |
|     - |  9493 | `		{ "removeAllExcept", PH7_MOD_PUBLIC, "SplObjectStorage $storage", "@int",` |
|     - |  9494 | `		  vm_builtin_SplObjectStorage_removeAllExcept },` |
|     - |  9495 | `		{ "getInfo",         PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplObjectStorage_getInfo },` |
|     - |  9496 | `		{ "setInfo",         PH7_MOD_PUBLIC, "mixed $info", "@void",` |
|     - |  9497 | `		  vm_builtin_SplObjectStorage_setInfo },` |
|     - |  9498 | `		{ "count",           PH7_MOD_PUBLIC, "int $mode = COUNT_NORMAL", "@int",` |
|     - |  9499 | `		  vm_builtin_SplObjectStorage_count },` |
|     - |  9500 | `		{ "rewind",          PH7_MOD_PUBLIC, "", "@void", vm_builtin_SplObjectStorage_rewind },` |
|     - |  9501 | `		{ "valid",           PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplObjectStorage_valid },` |
|     - |  9502 | `		{ "key",             PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplObjectStorage_key },` |
|     - |  9503 | `		{ "current",         PH7_MOD_PUBLIC, "", "@object", vm_builtin_SplObjectStorage_current },` |
|     - |  9504 | `		{ "next",            PH7_MOD_PUBLIC, "", "@void", vm_builtin_SplObjectStorage_next },` |
|     - |  9505 | `		{ "seek",            PH7_MOD_PUBLIC, "int $offset", "void",` |
|     - |  9506 | `		  vm_builtin_SplObjectStorage_seek },` |
|     - |  9507 | `		{ "unserialize",     PH7_MOD_PUBLIC, "string $data", "@void",` |
|     - |  9508 | `		  vm_builtin_SplObjectStorage_unserialize },` |
|     - |  9509 | `		{ "serialize",       PH7_MOD_PUBLIC, "", "@string",` |
|     - |  9510 | `		  vm_builtin_SplObjectStorage_serialize },` |
|     - |  9511 | ``		/* php's stub leaves these four offsets UNTYPED (a `@param object` docblock`` |
|     - |  9512 | `		 * Reflection does not print) while the ZPP takes an object -- so the` |
|     - |  9513 | `		 * signature says nothing and each body words its own refusal. */` |
|     - |  9514 | `		{ "offsetExists",    PH7_MOD_PUBLIC, "$object", "@bool",` |
|     - |  9515 | `		  vm_builtin_SplObjectStorage_offsetExists },` |
|     - |  9516 | `		{ "offsetGet",       PH7_MOD_PUBLIC, "$object", "@mixed",` |
|     - |  9517 | `		  vm_builtin_SplObjectStorage_offsetGet },` |
|     - |  9518 | `		{ "offsetSet",       PH7_MOD_PUBLIC, "$object, mixed $info = null", "@void",` |
|     - |  9519 | `		  vm_builtin_SplObjectStorage_offsetSet },` |
|     - |  9520 | `		{ "offsetUnset",     PH7_MOD_PUBLIC, "$object", "@void",` |
|     - |  9521 | `		  vm_builtin_SplObjectStorage_offsetUnset },` |
|     - |  9522 | `		{ "getHash",         PH7_MOD_PUBLIC, "object $object", "@string",` |
|     - |  9523 | `		  vm_builtin_SplObjectStorage_getHash },` |
|     - |  9524 | `		{ "__serialize",     PH7_MOD_PUBLIC, "", "@array",` |
|     - |  9525 | `		  vm_builtin_SplObjectStorage_serializeMagic },` |
|     - |  9526 | `		{ "__unserialize",   PH7_MOD_PUBLIC, "array $data", "@void",` |
|     - |  9527 | `		  vm_builtin_SplObjectStorage_unserializeMagic },` |
|     - |  9528 | `		{ "__debugInfo",     PH7_MOD_PUBLIC, "", "@array",` |
|     - |  9529 | `		  vm_builtin_SplObjectStorage_debugInfo },` |
|     - |  9530 | `	};` |
|     - |  9531 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - |  9532 | `		{ "SplObserver", 0, 0, PH7_CLASS_INTERFACE,` |
|     - |  9533 | `		  aObserverMethod, SX_ARRAYSIZE(aObserverMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - |  9534 | `		{ "SplSubject", 0, 0, PH7_CLASS_INTERFACE,` |
|     - |  9535 | `		  aSubjectMethod, SX_ARRAYSIZE(aSubjectMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - |  9536 | `		{ "SplObjectStorage", 0, "Countable,SeekableIterator,Serializable,ArrayAccess", 0,` |
|     - |  9537 | `		  aSosMethod, SX_ARRAYSIZE(aSosMethod), 0, 0,` |
|     - |  9538 | `		  aSosProp, SX_ARRAYSIZE(aSosProp), 0, 0, SosPresent },` |
|     - |  9539 | `	};` |
|  8450 |  9540 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|     5 |  9541 | `}` |
|     - |  9542 | `/*` |
|     - |  9543 | ` * ---------------------------------------------------------------------------` |
|     - |  9544 | ` * MultipleIterator: several iterators stepped in LOCKSTEP.` |
|     - |  9545 | ` *` |
|     - |  9546 | ` * php builds it on the very storage above — its C struct IS an` |
|     - |  9547 | `` * spl_SplObjectStorage, which is why `__debugInfo()` answers under`` |
|     - |  9548 | ` * SplObjectStorage's own mangled key — so this class holds the same {obj, inf}` |
|     - |  9549 | ` * table and reuses the same attach/detach/hash routines. What it adds is two` |
|     - |  9550 | ` * flags and the rule they make: MIT_NEED_ALL is valid only while EVERY` |
|     - |  9551 | ` * sub-iterator is, MIT_NEED_ANY while any one is, and an empty set is never` |
|     - |  9552 | ` * valid at all.` |
|     - |  9553 | ` *` |
|     - |  9554 | ` * current() and key() answer an ARRAY built in attach order, and how they treat` |
|     - |  9555 | ` * an exhausted member is the difference between the two modes: under NEED_ANY it` |
|     - |  9556 | ` * contributes NULL and the walk carries on, under NEED_ALL it is php's` |
|     - |  9557 | `` * `Called current() with non valid sub iterator` — a different refusal from the`` |
|     - |  9558 | `` * empty set's `Called current() on an invalid iterator`. MIT_KEYS_ASSOC keys that`` |
|     - |  9559 | `` * array by the `$info` each iterator was attached with, which is what makes a`` |
|     - |  9560 | ` * NULL info an error at KEY time rather than at attach time, and what makes a` |
|     - |  9561 | ` * DUPLICATE info an error at attach.` |
|     - |  9562 | ` */` |
|     - |  9563 | `#define MIT_NEED_ANY      0` |
|     - |  9564 | `#define MIT_NEED_ALL      1` |
|     - |  9565 | `#define MIT_KEYS_NUMERIC  0` |
|     - |  9566 | `#define MIT_KEYS_ASSOC    2` |
|     - |  9567 | `#define MIT_FL "__mfl"   /* php's flags word, stored raw */` |
|     - |  9568 |  |
|    98 |  9569 | `static sxi64 MitFlags(ph7_class_instance *pThis)` |
|     1 |  9570 | `{` |
|    99 |  9571 | `	return pThis ? PH7_NativeAttrInt(pThis,MIT_FL) : 0;` |
|     1 |  9572 | `}` |
|     - |  9573 | `/*` |
|     - |  9574 | `` * php compares two `$info`s with zend_is_identical, not with `==` and not as`` |
|     - |  9575 | ` * array keys: the string "5" and the int 5 are DIFFERENT infos and both may be` |
|     - |  9576 | `` * attached, while `true` and `1.5` are the same one because the ZPP narrowed`` |
|     - |  9577 | ` * both to the int 1.` |
|     - |  9578 | ` */` |
|    20 |  9579 | `static int MitInfoSame(ph7_value *pA,ph7_value *pB)` |
|     1 |  9580 | `{` |
|    21 |  9581 | `	if( (pA->iFlags & MEMOBJ_STRING) != (pB->iFlags & MEMOBJ_STRING) ){` |
|     3 |  9582 | `		return 0;` |
|     - |  9583 | `	}` |
|    19 |  9584 | `	if( pA->iFlags & MEMOBJ_STRING ){` |
|    17 |  9585 | `		sxu32 nA = SyBlobLength(&pA->sBlob), nB = SyBlobLength(&pB->sBlob);` |
|    17 |  9586 | `		return nA == nB` |
|    24 |  9587 | `			&& (nA == 0 \|\| SyMemcmp(SyBlobData(&pA->sBlob),SyBlobData(&pB->sBlob),nA) == 0);` |
|     - |  9588 | `	}` |
|     3 |  9589 | `	if( (pA->iFlags & MEMOBJ_NULL) \|\| (pB->iFlags & MEMOBJ_NULL) ){` |
|   ! 0 |  9590 | `		return 0;   /* a NULL info is never a duplicate: php only checks a given one */` |
|     - |  9591 | `	}` |
|     3 |  9592 | `	return pA->x.iVal == pB->x.iVal;` |
|    11 |  9593 | `}` |
|     - |  9594 | `/* Call a no-argument method on one sub-iterator. */` |
|   258 |  9595 | `static sxi32 MitCallOn(ph7_vm *pVm,ph7_class_instance *pIt,const char *zName,sxu32 nName,` |
|     - |  9596 | `	ph7_value *pOut)` |
|     1 |  9597 | `{` |
|   259 |  9598 | `	ph7_class_method *pMethod = pIt ? PH7_ClassExtractMethod(pIt->pClass,zName,nName) : 0;` |
|   259 |  9599 | `	if( pMethod == 0 ){` |
|   ! 0 |  9600 | `		return SXRET_OK;` |
|     - |  9601 | `	}` |
|   259 |  9602 | `	return PH7_VmCallClassMethod(pVm,pIt,pMethod,pOut,0,0);` |
|   130 |  9603 | `}` |
|     - |  9604 | `/*` |
|     - |  9605 | ` * SNAPSHOT the members before calling into any of them. A sub-iterator's own` |
|     - |  9606 | ` * rewind()/valid()/next() is user code and may attach or detach on this very` |
|     - |  9607 | ` * object, and a hash walk holding a node pointer across that call is reading a` |
|     - |  9608 | ` * table that moved. The snapshot is an ordinary array of the {obj, inf} pairs,` |
|     - |  9609 | ` * so it holds a reference to every member for the length of the pass.` |
|     - |  9610 | ` */` |
|   126 |  9611 | `static sxi32 MitSnapshot(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut)` |
|     1 |  9612 | `{` |
|   127 |  9613 | `	ph7_hashmap *pMap = SosMap(pVm,pThis);` |
|     - |  9614 | `	ph7_hashmap_node *pNode;` |
|     - |  9615 | `	sxu32 n;` |
|   127 |  9616 | `	PH7_MemObjInit(pVm,pOut);` |
|   127 |  9617 | `	if( PH7_MemObjToHashmap(pOut) != SXRET_OK ){` |
|   ! 0 |  9618 | `		return SXERR_MEM;` |
|     - |  9619 | `	}` |
|   127 |  9620 | `	if( pMap == 0 ){` |
|   ! 0 |  9621 | `		return SXRET_OK;` |
|     - |  9622 | `	}` |
|   343 |  9623 | `	for( pNode = pMap->pFirst, n = 0 ; pNode && n < pMap->nEntry ; ++n, pNode = pNode->pPrev ){` |
|   217 |  9624 | `		ph7_value *pPair = HashmapExtractNodeValue(pNode);` |
|   217 |  9625 | `		if( pPair ){` |
|   217 |  9626 | `			ph7_array_add_elem(pOut,0,pPair);` |
|   108 |  9627 | `		}` |
|   109 |  9628 | `	}` |
|   127 |  9629 | `	return SXRET_OK;` |
|    64 |  9630 | `}` |
|     - |  9631 | ``/* One snapshot entry's `obj` half as an instance, and its `inf` half. */`` |
|   236 |  9632 | `static ph7_class_instance * MitShotObj(ph7_value *pOut,sxu32 iAt,ph7_value **ppInf)` |
|     1 |  9633 | `{` |
|   237 |  9634 | `	ph7_hashmap_node *pNode = 0;` |
|     - |  9635 | `	ph7_value *pPair,*pObj;` |
|   237 |  9636 | `	if( ppInf ){` |
|   117 |  9637 | `		*ppInf = 0;` |
|    58 |  9638 | `	}` |
|   236 |  9639 | `	if( (pOut->iFlags & MEMOBJ_HASHMAP) == 0` |
|   237 |  9640 | `	 \|\| HashmapLookupIntKey((ph7_hashmap *)pOut->x.pOther,(sxi64)iAt,&pNode) != SXRET_OK ){` |
|   ! 0 |  9641 | `		return 0;` |
|     - |  9642 | `	}` |
|   237 |  9643 | `	pPair = HashmapExtractNodeValue(pNode);` |
|   237 |  9644 | `	if( ppInf ){` |
|   117 |  9645 | `		*ppInf = SosPart(pPair,"inf");` |
|    58 |  9646 | `	}` |
|   237 |  9647 | `	pObj = SosPart(pPair,"obj");` |
|   237 |  9648 | `	if( pObj == 0 \|\| (pObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|   ! 0 |  9649 | `		return 0;` |
|     - |  9650 | `	}` |
|   237 |  9651 | `	return (ph7_class_instance *)pObj->x.pOther;` |
|   119 |  9652 | `}` |
|   126 |  9653 | `static sxu32 MitShotCount(ph7_value *pShot)` |
|     1 |  9654 | `{` |
|   127 |  9655 | `	return (pShot->iFlags & MEMOBJ_HASHMAP) && pShot->x.pOther` |
|   189 |  9656 | `		? ((ph7_hashmap *)pShot->x.pOther)->nEntry : 0;` |
|     1 |  9657 | `}` |
|     - |  9658 | `/* Walk every sub-iterator, calling one no-argument method on each. */` |
|    48 |  9659 | `static sxi32 MitCallAll(ph7_context *pCtx,const char *zName,sxu32 nName)` |
|     1 |  9660 | `{` |
|    49 |  9661 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  9662 | `	ph7_value sShot;` |
|     - |  9663 | `	sxu32 i,nCount;` |
|    49 |  9664 | `	sxi32 rc = MitSnapshot(pVm,PH7_ContextThis(pCtx),&sShot);` |
|    49 |  9665 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  9666 | `		PH7_MemObjRelease(&sShot);` |
|   ! 0 |  9667 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  9668 | `	}` |
|    49 |  9669 | `	nCount = MitShotCount(&sShot);` |
|   133 |  9670 | `	for( i = 0 ; i < nCount ; ++i ){` |
|    85 |  9671 | `		ph7_class_instance *pIt = MitShotObj(&sShot,i,0);` |
|    85 |  9672 | `		rc = MitCallOn(pVm,pIt,zName,nName,0);` |
|    85 |  9673 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  9674 | `			PH7_MemObjRelease(&sShot);` |
|   ! 0 |  9675 | `			return rc;` |
|     - |  9676 | `		}` |
|    43 |  9677 | `	}` |
|    49 |  9678 | `	PH7_MemObjRelease(&sShot);` |
|    49 |  9679 | `	return PH7_OK;` |
|    25 |  9680 | `}` |
|   120 |  9681 | `static sxi32 MitSubValid(ph7_vm *pVm,ph7_class_instance *pIt,int *pbValid)` |
|     1 |  9682 | `{` |
|     - |  9683 | `	ph7_value sVal;` |
|     - |  9684 | `	sxi32 rc;` |
|   121 |  9685 | `	*pbValid = 0;` |
|   121 |  9686 | `	PH7_MemObjInit(pVm,&sVal);` |
|   121 |  9687 | `	rc = MitCallOn(pVm,pIt,"valid",sizeof("valid")-1,&sVal);` |
|   121 |  9688 | `	if( rc == SXRET_OK ){` |
|   121 |  9689 | `		PH7_MemObjToBool(&sVal);          /* a STATUS, not the answer */` |
|   121 |  9690 | `		*pbValid = sVal.x.iVal != 0;` |
|    60 |  9691 | `	}` |
|   121 |  9692 | `	PH7_MemObjRelease(&sVal);` |
|   121 |  9693 | `	return rc;` |
|     1 |  9694 | `}` |
|    66 |  9695 | `static int vm_builtin_MultipleIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9696 | `{` |
|    67 |  9697 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    67 |  9698 | `	if( pThis == 0 ){` |
|   ! 0 |  9699 | `		return PH7_OK;` |
|     - |  9700 | `	}` |
|   115 |  9701 | `	PH7_NativeSetAttrInt(pCtx->pVm,pThis,MIT_FL,` |
|    48 |  9702 | `		nArg > 0 ? ph7_value_to_int64(apArg[0]) : MIT_NEED_ALL);` |
|    67 |  9703 | `	return PH7_OK;` |
|    34 |  9704 | `}` |
|    20 |  9705 | `static int vm_builtin_MultipleIterator_getFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9706 | `{` |
|    10 |  9707 | `	SXUNUSED(nArg);` |
|    10 |  9708 | `	SXUNUSED(apArg);` |
|    21 |  9709 | `	ph7_result_int64(pCtx,MitFlags(PH7_ContextThis(pCtx)));` |
|    21 |  9710 | `	return PH7_OK;` |
|     1 |  9711 | `}` |
|     8 |  9712 | `static int vm_builtin_MultipleIterator_setFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9713 | `{` |
|     9 |  9714 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     9 |  9715 | `	if( nArg > 0 && pThis ){` |
|     - |  9716 | `		/* php screens nothing here: the word is stored as given. */` |
|     9 |  9717 | `		PH7_NativeSetAttrInt(pCtx->pVm,pThis,MIT_FL,ph7_value_to_int64(apArg[0]));` |
|     4 |  9718 | `	}` |
|     9 |  9719 | `	return PH7_OK;` |
|     1 |  9720 | `}` |
|     - |  9721 | `/*` |
|     - |  9722 | ` * php's attachIterator: the $info must be UNIQUE across the table, which is what` |
|     - |  9723 | ` * MIT_KEYS_ASSOC needs to build a key set — checked whatever the flags say, since` |
|     - |  9724 | ` * they can be turned on later.` |
|     - |  9725 | ` */` |
|    56 |  9726 | `static int vm_builtin_MultipleIterator_attachIterator(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9727 | `{` |
|    57 |  9728 | `	ph7_vm *pVm = pCtx->pVm;` |
|    57 |  9729 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  9730 | `	ph7_hashmap *pMap;` |
|     - |  9731 | `	ph7_hashmap_node *pNode;` |
|     - |  9732 | `	ph7_value sInf;` |
|     - |  9733 | `	sxu32 n;` |
|     - |  9734 | `	sxi32 rc;` |
|    57 |  9735 | `	if( nArg < 1 \|\| pThis == 0 ){` |
|   ! 0 |  9736 | `		return PH7_OK;` |
|     - |  9737 | `	}` |
|    57 |  9738 | `	PH7_MemObjInit(pVm,&sInf);` |
|    57 |  9739 | `	if( nArg > 1 && (apArg[1]->iFlags & MEMOBJ_NULL) == 0 ){` |
|    45 |  9740 | `		PH7_MemObjStore(apArg[1],&sInf);` |
|     - |  9741 | ``		/* php's `string\|int` ZPP: a numeric string stays a string, everything else`` |
|     - |  9742 | `		 * that is not already one becomes an int. */` |
|    45 |  9743 | `		if( (sInf.iFlags & MEMOBJ_STRING) == 0 ){` |
|     9 |  9744 | `			PH7_MemObjToInteger(&sInf);` |
|     4 |  9745 | `		}` |
|    45 |  9746 | `		pMap = SosMap(pVm,pThis);` |
|    61 |  9747 | `		for( pNode = pMap ? pMap->pFirst : 0, n = 0 ; pNode && n < pMap->nEntry ; ++n ){` |
|    21 |  9748 | `			ph7_value *pPair = HashmapExtractNodeValue(pNode);` |
|    21 |  9749 | `			ph7_value *pOld = SosPart(pPair,"inf");` |
|    21 |  9750 | `			if( pOld && MitInfoSame(pOld,&sInf) ){` |
|     5 |  9751 | `				PH7_MemObjRelease(&sInf);` |
|     5 |  9752 | `				return PH7_VmThrowException(pCtx,"InvalidArgumentException",` |
|     - |  9753 | `					"Key duplication error");` |
|     - |  9754 | `			}` |
|    17 |  9755 | `			pNode = pNode->pPrev;` |
|     9 |  9756 | `		}` |
|    20 |  9757 | `	}` |
|    53 |  9758 | `	rc = SosAttach(pCtx,pThis,apArg[0],&sInf);` |
|    53 |  9759 | `	PH7_MemObjRelease(&sInf);` |
|    53 |  9760 | `	return rc;` |
|    29 |  9761 | `}` |
|     6 |  9762 | `static int vm_builtin_MultipleIterator_detachIterator(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9763 | `{` |
|     7 |  9764 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     7 |  9765 | `	if( nArg < 1 \|\| pThis == 0 ){` |
|   ! 0 |  9766 | `		return PH7_OK;` |
|     - |  9767 | `	}` |
|     7 |  9768 | `	return SosDetach(pCtx,pThis,apArg[0],0);` |
|     4 |  9769 | `}` |
|     4 |  9770 | `static int vm_builtin_MultipleIterator_containsIterator(ph7_context *pCtx,int nArg,` |
|     - |  9771 | `	ph7_value **apArg)` |
|     1 |  9772 | `{` |
|     5 |  9773 | `	ph7_vm *pVm = pCtx->pVm;` |
|     5 |  9774 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  9775 | `	ph7_hashmap *pMap;` |
|     5 |  9776 | `	ph7_hashmap_node *pNode = 0;` |
|     - |  9777 | `	ph7_value sObj,sKey;` |
|     - |  9778 | `	sxi32 rc;` |
|     5 |  9779 | `	if( nArg < 1 \|\| pThis == 0 ){` |
|   ! 0 |  9780 | `		return PH7_OK;` |
|     - |  9781 | `	}` |
|     5 |  9782 | `	PH7_MemObjInit(pVm,&sObj);` |
|     5 |  9783 | `	PH7_MemObjInit(pVm,&sKey);` |
|     5 |  9784 | `	PH7_MemObjStore(apArg[0],&sObj);` |
|     5 |  9785 | `	rc = SosKey(pCtx,pThis,&sObj,&sKey);` |
|     5 |  9786 | `	if( rc == PH7_OK ){` |
|     5 |  9787 | `		pMap = SosMap(pVm,pThis);` |
|     9 |  9788 | `		ph7_result_bool(pCtx,` |
|     4 |  9789 | `			pMap && PH7_HashmapLookup(pMap,&sKey,&pNode) == SXRET_OK);` |
|     2 |  9790 | `	}` |
|     5 |  9791 | `	PH7_MemObjRelease(&sObj);` |
|     5 |  9792 | `	PH7_MemObjRelease(&sKey);` |
|     5 |  9793 | `	return rc;` |
|     3 |  9794 | `}` |
|    12 |  9795 | `static int vm_builtin_MultipleIterator_countIterators(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9796 | `{` |
|     - |  9797 | `	ph7_hashmap *pMap;` |
|     6 |  9798 | `	SXUNUSED(nArg);` |
|     6 |  9799 | `	SXUNUSED(apArg);` |
|    13 |  9800 | `	pMap = SosMap(pCtx->pVm,PH7_ContextThis(pCtx));` |
|    13 |  9801 | `	ph7_result_int64(pCtx,pMap ? (sxi64)pMap->nEntry : 0);` |
|    13 |  9802 | `	return PH7_OK;` |
|     1 |  9803 | `}` |
|    22 |  9804 | `static int vm_builtin_MultipleIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9805 | `{` |
|    11 |  9806 | `	SXUNUSED(nArg);` |
|    11 |  9807 | `	SXUNUSED(apArg);` |
|    23 |  9808 | `	return MitCallAll(pCtx,"rewind",sizeof("rewind")-1);` |
|     1 |  9809 | `}` |
|    26 |  9810 | `static int vm_builtin_MultipleIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9811 | `{` |
|    13 |  9812 | `	SXUNUSED(nArg);` |
|    13 |  9813 | `	SXUNUSED(apArg);` |
|    27 |  9814 | `	return MitCallAll(pCtx,"next",sizeof("next")-1);` |
|     1 |  9815 | `}` |
|    26 |  9816 | `static int vm_builtin_MultipleIterator_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9817 | `{` |
|    27 |  9818 | `	ph7_vm *pVm = pCtx->pVm;` |
|    27 |  9819 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    27 |  9820 | `	int bExpect = (MitFlags(pThis) & MIT_NEED_ALL) ? 1 : 0;` |
|     - |  9821 | `	ph7_value sShot;` |
|     - |  9822 | `	sxu32 i,nCount;` |
|     - |  9823 | `	sxi32 rc;` |
|    13 |  9824 | `	SXUNUSED(nArg);` |
|    13 |  9825 | `	SXUNUSED(apArg);` |
|    27 |  9826 | `	rc = MitSnapshot(pVm,pThis,&sShot);` |
|    27 |  9827 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  9828 | `		PH7_MemObjRelease(&sShot);` |
|   ! 0 |  9829 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  9830 | `	}` |
|    27 |  9831 | `	nCount = MitShotCount(&sShot);` |
|    27 |  9832 | `	if( nCount < 1 ){` |
|     - |  9833 | `		/* php: an empty set is never valid, whichever mode it is in. */` |
|     3 |  9834 | `		PH7_MemObjRelease(&sShot);` |
|     3 |  9835 | `		ph7_result_bool(pCtx,0);` |
|     3 |  9836 | `		return PH7_OK;` |
|     - |  9837 | `	}` |
|    45 |  9838 | `	for( i = 0 ; i < nCount ; ++i ){` |
|    37 |  9839 | `		int bValid = 0;` |
|    37 |  9840 | `		rc = MitSubValid(pVm,MitShotObj(&sShot,i,0),&bValid);` |
|    37 |  9841 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  9842 | `			PH7_MemObjRelease(&sShot);` |
|   ! 0 |  9843 | `			return rc;` |
|     - |  9844 | `		}` |
|    37 |  9845 | `		if( bValid != bExpect ){` |
|     - |  9846 | `			/* NEED_ALL stops at the first invalid one, NEED_ANY at the first valid` |
|     - |  9847 | `			 * one, and each answers the opposite of what it was looking for. */` |
|    17 |  9848 | `			PH7_MemObjRelease(&sShot);` |
|    17 |  9849 | `			ph7_result_bool(pCtx,!bExpect);` |
|    17 |  9850 | `			return PH7_OK;` |
|     - |  9851 | `		}` |
|    11 |  9852 | `	}` |
|     9 |  9853 | `	PH7_MemObjRelease(&sShot);` |
|     9 |  9854 | `	ph7_result_bool(pCtx,bExpect);` |
|     9 |  9855 | `	return PH7_OK;` |
|    14 |  9856 | `}` |
|     - |  9857 | `/*` |
|     - |  9858 | ` * php's spl_multiple_iterator_get_all, which current() and key() share. The` |
|     - |  9859 | ` * refusals differ by cause: an EMPTY set is "on an invalid iterator", an` |
|     - |  9860 | ` * exhausted member under NEED_ALL is "with non valid sub iterator", and a NULL` |
|     - |  9861 | ` * $info under MIT_KEYS_ASSOC is the InvalidArgumentException this is the only` |
|     - |  9862 | ` * site of.` |
|     - |  9863 | ` */` |
|    52 |  9864 | `static int MitGetAll(ph7_context *pCtx,int bKey)` |
|     1 |  9865 | `{` |
|    53 |  9866 | `	ph7_vm *pVm = pCtx->pVm;` |
|    53 |  9867 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    53 |  9868 | `	const char *zWhat = bKey ? "key" : "current";` |
|    53 |  9869 | `	sxi64 iFlags = MitFlags(pThis);` |
|     - |  9870 | `	ph7_value sShot,sOut,sVal;` |
|     - |  9871 | `	sxu32 i,nCount;` |
|     - |  9872 | `	sxi32 rc;` |
|    53 |  9873 | `	rc = MitSnapshot(pVm,pThis,&sShot);` |
|    53 |  9874 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  9875 | `		PH7_MemObjRelease(&sShot);` |
|   ! 0 |  9876 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  9877 | `	}` |
|    53 |  9878 | `	nCount = MitShotCount(&sShot);` |
|    53 |  9879 | `	if( nCount < 1 ){` |
|     5 |  9880 | `		PH7_MemObjRelease(&sShot);` |
|     7 |  9881 | `		return PH7_VmThrowException(pCtx,"RuntimeException",` |
|     2 |  9882 | `			"Called %s() on an invalid iterator",zWhat);` |
|     - |  9883 | `	}` |
|    49 |  9884 | `	PH7_MemObjInit(pVm,&sOut);` |
|    49 |  9885 | `	if( PH7_MemObjToHashmap(&sOut) != SXRET_OK ){` |
|   ! 0 |  9886 | `		PH7_MemObjRelease(&sShot);` |
|   ! 0 |  9887 | `		PH7_MemObjRelease(&sOut);` |
|   ! 0 |  9888 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  9889 | `	}` |
|   125 |  9890 | `	for( i = 0 ; i < nCount ; ++i ){` |
|    85 |  9891 | `		ph7_value *pInf = 0;` |
|    85 |  9892 | `		ph7_class_instance *pIt = MitShotObj(&sShot,i,&pInf);` |
|    85 |  9893 | `		int bValid = 0;` |
|     - |  9894 | `		/* php asks the sub-iterator FIRST and only then looks at the key it would` |
|     - |  9895 | `		 * file the answer under, so an exhausted member under NEED_ALL reports the` |
|     - |  9896 | `		 * iterator rather than the missing $info. */` |
|    85 |  9897 | `		rc = MitSubValid(pVm,pIt,&bValid);` |
|    85 |  9898 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  9899 | `			goto fail;` |
|     - |  9900 | `		}` |
|    85 |  9901 | `		PH7_MemObjInit(pVm,&sVal);` |
|    85 |  9902 | `		if( bValid ){` |
|    55 |  9903 | `			rc = MitCallOn(pVm,pIt,bKey ? "key" : "current",bKey ? 3 : 7,&sVal);` |
|    55 |  9904 | `			if( rc != SXRET_OK ){` |
|   ! 0 |  9905 | `				PH7_MemObjRelease(&sVal);` |
|   ! 0 |  9906 | `				goto fail;` |
|     1 |  9907 | `			}` |
|    58 |  9908 | `		}else if( iFlags & MIT_NEED_ALL ){` |
|     7 |  9909 | `			PH7_MemObjRelease(&sVal);` |
|     7 |  9910 | `			PH7_MemObjRelease(&sShot);` |
|     7 |  9911 | `			PH7_MemObjRelease(&sOut);` |
|    11 |  9912 | `			return PH7_VmThrowException(pCtx,"RuntimeException",` |
|     3 |  9913 | `				"Called %s() with non valid sub iterator",zWhat);` |
|     - |  9914 | `		}` |
|     - |  9915 | `		/* NEED_ANY leaves the null sVal in place: an exhausted member contributes` |
|     - |  9916 | `		 * php's null and the walk carries on. */` |
|    79 |  9917 | `		if( iFlags & MIT_KEYS_ASSOC ){` |
|     - |  9918 | ``			/* The snapshot's own `inf` slot: re-read after the call above, since it`` |
|     - |  9919 | `			 * lives in a hashmap the call may have moved. */` |
|    33 |  9920 | `			MitShotObj(&sShot,i,&pInf);` |
|    33 |  9921 | `			if( pInf == 0 \|\| (pInf->iFlags & MEMOBJ_NULL) ){` |
|     3 |  9922 | `				PH7_MemObjRelease(&sVal);` |
|     3 |  9923 | `				PH7_MemObjRelease(&sShot);` |
|     3 |  9924 | `				PH7_MemObjRelease(&sOut);` |
|     3 |  9925 | `				return PH7_VmThrowException(pCtx,"InvalidArgumentException",` |
|     - |  9926 | `					"Sub-Iterator is associated with NULL");` |
|     - |  9927 | `			}` |
|    15 |  9928 | `		}` |
|    77 |  9929 | `		ph7_array_add_elem(&sOut,(iFlags & MIT_KEYS_ASSOC) ? pInf : 0,&sVal);` |
|    77 |  9930 | `		PH7_MemObjRelease(&sVal);` |
|    39 |  9931 | `	}` |
|    41 |  9932 | `	PH7_MemObjRelease(&sShot);` |
|    41 |  9933 | `	ph7_result_value(pCtx,&sOut);` |
|    41 |  9934 | `	PH7_MemObjRelease(&sOut);` |
|    41 |  9935 | `	return PH7_OK;` |
|   ! 0 |  9936 | `fail:` |
|   ! 0 |  9937 | `	PH7_MemObjRelease(&sShot);` |
|   ! 0 |  9938 | `	PH7_MemObjRelease(&sOut);` |
|   ! 0 |  9939 | `	return rc;` |
|    27 |  9940 | `}` |
|    24 |  9941 | `static int vm_builtin_MultipleIterator_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9942 | `{` |
|    12 |  9943 | `	SXUNUSED(nArg);` |
|    12 |  9944 | `	SXUNUSED(apArg);` |
|    25 |  9945 | `	return MitGetAll(pCtx,FALSE);` |
|     1 |  9946 | `}` |
|    28 |  9947 | `static int vm_builtin_MultipleIterator_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9948 | `{` |
|    14 |  9949 | `	SXUNUSED(nArg);` |
|    14 |  9950 | `	SXUNUSED(apArg);` |
|    29 |  9951 | `	return MitGetAll(pCtx,TRUE);` |
|     1 |  9952 | `}` |
|  8445 |  9953 | `static sxi32 VmInstallSplMultipleIterator(ph7_vm *pVm)` |
|     5 |  9954 | `{` |
|     - |  9955 | `	static const PH7_NativeConstDef aMitConst[] = {` |
|     - |  9956 | `		{ "MIT_NEED_ANY",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, MIT_NEED_ANY, 0, 0.0 },` |
|     - |  9957 | `		{ "MIT_NEED_ALL",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, MIT_NEED_ALL, 0, 0.0 },` |
|     - |  9958 | `		{ "MIT_KEYS_NUMERIC", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, MIT_KEYS_NUMERIC, 0, 0.0 },` |
|     - |  9959 | `		{ "MIT_KEYS_ASSOC",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, MIT_KEYS_ASSOC, 0, 0.0 },` |
|     - |  9960 | `	};` |
|     - |  9961 | `	static const PH7_NativePropDef aMitProp[] = {` |
|     - |  9962 | `		{ SOS_S,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  9963 | `		{ MIT_FL, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  9964 | `	};` |
|     - |  9965 | `	static const PH7_NativeMethodDef aMitMethod[] = {` |
|     - |  9966 | `		{ "__construct",      PH7_MOD_PUBLIC, "int $flags = MultipleIterator::MIT_NEED_ALL \| MultipleIterator::MIT_KEYS_NUMERIC", 0,` |
|     - |  9967 | `		  vm_builtin_MultipleIterator_construct },` |
|     - |  9968 | `		{ "getFlags",         PH7_MOD_PUBLIC, "", "@int", vm_builtin_MultipleIterator_getFlags },` |
|     - |  9969 | `		{ "setFlags",         PH7_MOD_PUBLIC, "int $flags", "@void",` |
|     - |  9970 | `		  vm_builtin_MultipleIterator_setFlags },` |
|     - |  9971 | `		{ "attachIterator",   PH7_MOD_PUBLIC, "Iterator $iterator, string\|int\|null $info = null",` |
|     - |  9972 | `		  "@void", vm_builtin_MultipleIterator_attachIterator },` |
|     - |  9973 | `		{ "detachIterator",   PH7_MOD_PUBLIC, "Iterator $iterator", "@void",` |
|     - |  9974 | `		  vm_builtin_MultipleIterator_detachIterator },` |
|     - |  9975 | `		{ "containsIterator", PH7_MOD_PUBLIC, "Iterator $iterator", "@bool",` |
|     - |  9976 | `		  vm_builtin_MultipleIterator_containsIterator },` |
|     - |  9977 | `		{ "countIterators",   PH7_MOD_PUBLIC, "", "@int",` |
|     - |  9978 | `		  vm_builtin_MultipleIterator_countIterators },` |
|     - |  9979 | `		{ "rewind",           PH7_MOD_PUBLIC, "", "@void", vm_builtin_MultipleIterator_rewind },` |
|     - |  9980 | `		{ "valid",            PH7_MOD_PUBLIC, "", "@bool", vm_builtin_MultipleIterator_valid },` |
|     - |  9981 | `		{ "key",              PH7_MOD_PUBLIC, "", "@array", vm_builtin_MultipleIterator_key },` |
|     - |  9982 | `		{ "current",          PH7_MOD_PUBLIC, "", "@array", vm_builtin_MultipleIterator_current },` |
|     - |  9983 | `		{ "next",             PH7_MOD_PUBLIC, "", "@void", vm_builtin_MultipleIterator_next },` |
|     - |  9984 | `		{ "__debugInfo",      PH7_MOD_PUBLIC, "", "@array",` |
|     - |  9985 | `		  vm_builtin_SplObjectStorage_debugInfo },` |
|     - |  9986 | `	};` |
|     - |  9987 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - |  9988 | `		/* No presenter: php declares no properties and shows none, and the storage` |
|     - |  9989 | `		 * is reachable only through __debugInfo() — which is SplObjectStorage's own,` |
|     - |  9990 | `		 * mangled key included, because the struct behind both classes is one. */` |
|     - |  9991 | `		{ "MultipleIterator", 0, "Iterator", 0,` |
|     - |  9992 | `		  aMitMethod, SX_ARRAYSIZE(aMitMethod), aMitConst, SX_ARRAYSIZE(aMitConst),` |
|     - |  9993 | `		  aMitProp, SX_ARRAYSIZE(aMitProp), 0, 0, 0 },` |
|     - |  9994 | `	};` |
|  8450 |  9995 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|     5 |  9996 | `}` |
|     - |  9997 | `/*` |
|     - |  9998 | ` * ---------------------------------------------------------------------------` |
|     - |  9999 | ` * SplFileInfo.` |
|     - | 10000 | ` *` |
|     - | 10001 | `` * php's `spl_filesystem_object` keeps TWO strings for a path, and which one a`` |
|     - | 10002 | `` * method reads is the whole model: `file_name` is the pathname with any trailing`` |
|     - | 10003 | `` * slashes stripped, and `path` is everything before the LAST slash of it -- which`` |
|     - | 10004 | ` * is EMPTY when the name has no slash before its last component, so` |
|     - | 10005 | `` * `(new SplFileInfo('/a.txt'))->getPath()` is `''` and `getFilename()` answers the`` |
|     - | 10006 | `` * whole `/a.txt`. Every accessor is a slice of that pair (php's own`` |
|     - | 10007 | `` * `spl_filesystem_info_set_filename`), and the chunk, which called `basename()` and`` |
|     - | 10008 | `` * `dirname()` per method instead, disagreed on all of it.`` |
|     - | 10009 | ` *` |
|     - | 10010 | `` * The stat family is php's `FileInfoFunction` macro: `php_stat()` with the error`` |
|     - | 10011 | ` * handler REPLACED, so the warning a failed stat would print becomes a` |
|     - | 10012 | `` * `RuntimeException` instead -- `getSize()` on a missing file RAISES there and`` |
|     - | 10013 | ` * warned-then-answered-false here. Two of the fifteen lstat rather than stat` |
|     - | 10014 | `` * (`getType`, `isLink`), which is php's IS_LINK_OPERATION set.`` |
|     - | 10015 | ` *` |
|     - | 10016 | ` * The two slots are PRIVATE and PRESENTED: php declares no properties at all` |
|     - | 10017 | `` * (`getProperties()`, the `(array)` cast and `get_object_vars()` are empty) while`` |
|     - | 10018 | `` * `var_dump` shows `pathName`/`fileName` under their mangled private keys, and`` |
|     - | 10019 | `` * `__debugInfo()` hands back that same array. The class is `@not-serializable`.`` |
|     - | 10020 | ` *` |
|     - | 10021 | `` * `openFile()` and `setFileClass()` are the doors into SplFileObject and answer`` |
|     - | 10022 | `` * one; `setInfoClass()` and the `?string $class` argument of`` |
|     - | 10023 | `` * `getFileInfo()`/`getPathInfo()` name a class derived from THIS one. The two`` |
|     - | 10024 | ` * class names live in slots of their own, which is what makes them survive a` |
|     - | 10025 | ` * clone and travel to a directory iterator's children.` |
|     - | 10026 | ` */` |
|     - | 10027 | `#define SFI_N  "__n"   /* php's file_name: the pathname, trailing slashes stripped */` |
|     - | 10028 | `#define SFI_P  "__p"   /* php's path: everything before its last slash */` |
|     - | 10029 | `#define SFI_IC "__ic"  /* php's info_class */` |
|     - | 10030 | `#define SFI_FC "__fc"  /* php's file_class, what openFile() builds */` |
|     - | 10031 | `` /* The directory-iterator half of php's struct, on the same instance: its `u.dir` `` |
|     - | 10032 | ` * arm minus the handle, which cannot live in a php-visible slot (see VmDirHandle).` |
|     - | 10033 | `` * Declared by DirectoryIterator, so `SplDirIs()` is what tells the two apart. */`` |
|     - | 10034 | `#define SDI_E  "__e"   /* php's u.dir.entry.d_name; "" once the walk has run out */` |
|     - | 10035 | `#define SDI_I  "__i"   /* php's u.dir.index: what key() answers */` |
|     - | 10036 | `#define SDI_F  "__f"   /* php's flags */` |
|     - | 10037 | `#define SDI_S  "__s"   /* php's u.dir.sub_path (RecursiveDirectoryIterator) */` |
|     - | 10038 | ``/* And the FILE arm, php's `u.file`. Declared by SplFileObject; named up here`` |
|     - | 10039 | ` * because the shared presentation hook shows three of its slots. */` |
|     - | 10040 | `#define SFO_H  "__fh"  /* the open io_private, as a resource */` |
|     - | 10041 | `#define SFO_M  "__fo"  /* php's u.file.open_mode */` |
|     - | 10042 | `#define SFO_FL "__ff"  /* php's flags */` |
|     - | 10043 | `#define SFO_ML "__fm"  /* php's u.file.max_line_len */` |
|     - | 10044 | `#define SFO_D  "__fd"  /* php's u.file.delimiter */` |
|     - | 10045 | `#define SFO_EN "__fn"  /* php's u.file.enclosure */` |
|     - | 10046 | `#define SFO_ES "__fx"  /* php's u.file.escape (PH7_CSV_NO_ESCAPE = disabled) */` |
|     - | 10047 | `#define SFO_ED "__fq"  /* php's u.file.is_escape_default */` |
|     - | 10048 | `#define SFO_L  "__fl"  /* php's u.file.current_line */` |
|     - | 10049 | `#define SFO_Z  "__fz"  /* php's u.file.current_zval */` |
|     - | 10050 | `#define SFO_LS "__fs"  /* which of the two is live (SFO_HAS_*) */` |
|     - | 10051 | `#define SFO_K  "__fk"  /* php's u.file.current_line_num */` |
|     - | 10052 | `/* The open stream behind an SplFileObject, or 0 for one that has none. */` |
|     - | 10053 | `static io_private * SfoDev(ph7_class_instance *pThis);` |
|     - | 10054 |  |
|     - | 10055 | `/* php's IS_SLASH is PLATFORM-dependent: a backslash separates on Windows and is an` |
|     - | 10056 | ``  * ordinary filename byte everywhere else, which is why `new SplFileInfo('C:\\x\\y')` `` |
|     - | 10057 | ` * has an empty path on unix. PH7_ExtractDirName draws the same line. */` |
|     - | 10058 | `#ifdef __WINNT__` |
|     - | 10059 | `# define SFI_IS_SLASH(c) ((c) == '/' \|\| (c) == '\\')` |
|     - | 10060 | `#else` |
|     - | 10061 | `# define SFI_IS_SLASH(c) ((c) == '/')` |
|     - | 10062 | `#endif` |
|     - | 10063 |  |
|     - | 10064 | `/*` |
|     - | 10065 | ` * The directory-iterator half of this family, declared up here because php's` |
|     - | 10066 | `` * SplFileInfo bodies BRANCH on `spl_filesystem_object::type`: a DIR instance`` |
|     - | 10067 | ` * keeps its pathname lazily (path + slash + the current entry, rebuilt after` |
|     - | 10068 | ` * every read) and answers nothing at all once the walk has run out. Exactly` |
|     - | 10069 | ` * five accessors below ask, which is the same five php branches in.` |
|     - | 10070 | ` */` |
|     - | 10071 | `static int SplDirIs(ph7_vm *pVm,ph7_class_instance *pThis);` |
|     - | 10072 | `static const char * SplDirName(ph7_vm *pVm,ph7_class_instance *pThis,int *pnLen);` |
|     - | 10073 | `static int SplDirAtEnd(ph7_class_instance *pThis);` |
|     - | 10074 | `static VmDirHandle * SplDirState(ph7_vm *pVm,ph7_class_instance *pThis);` |
|     - | 10075 | `/* One of the two path slots, as bytes. */` |
|  2327 | 10076 | `static const char * SfiStr(ph7_class_instance *pThis,const char *zSlot,int *pnLen)` |
|     2 | 10077 | `{` |
|  2329 | 10078 | `	ph7_value *pVal = pThis ? PH7_NativeAttr(pThis,zSlot) : 0;` |
|  2329 | 10079 | `	*pnLen = 0;` |
|  2329 | 10080 | `	if( pVal == 0 ){` |
|   ! 0 | 10081 | `		return "";` |
|     - | 10082 | `	}` |
|  2329 | 10083 | `	return ph7_value_to_string(pVal,pnLen);` |
|  1173 | 10084 | `}` |
|     - | 10085 | `/*` |
|     - | 10086 | ` * php's spl_filesystem_info_set_filename: strip the trailing slashes (never the` |
|     - | 10087 | ` * only character), then cut the path at the last slash of what is left. A name` |
|     - | 10088 | ` * with no slash before its final component keeps an EMPTY path, which is what` |
|     - | 10089 | ` * makes getFilename() answer the whole thing.` |
|     - | 10090 | ` */` |
|   306 | 10091 | `static void SfiSetName(ph7_vm *pVm,ph7_class_instance *pThis,const char *zPath,int nPath)` |
|     3 | 10092 | `{` |
|   309 | 10093 | `	int nFile = nPath;` |
|     - | 10094 | `	int nDir;` |
|   309 | 10095 | `	if( nFile > 1 && SFI_IS_SLASH(zPath[nFile-1]) ){` |
|     4 | 10096 | `		do{` |
|     9 | 10097 | `			nFile--;` |
|     9 | 10098 | `		}while( nFile > 1 && SFI_IS_SLASH(zPath[nFile-1]) );` |
|     4 | 10099 | `	}` |
|   309 | 10100 | `	nDir = nFile;` |
|  4596 | 10101 | `	while( nDir > 1 && !SFI_IS_SLASH(zPath[nDir-1]) ){` |
|  4290 | 10102 | `		nDir--;` |
|     3 | 10103 | `	}` |
|   309 | 10104 | `	if( nDir > 0 ){` |
|   305 | 10105 | `		nDir--;` |
|   151 | 10106 | `	}` |
|   309 | 10107 | `	PH7_NativeSetAttrStr(pVm,pThis,SFI_N,zPath,nFile);` |
|   309 | 10108 | `	PH7_NativeSetAttrStr(pVm,pThis,SFI_P,zPath,nDir);` |
|   309 | 10109 | `}` |
|     - | 10110 | `/*` |
|     - | 10111 | `` * php's `file_name`: the slot for a plain SplFileInfo, and the lazily rebuilt`` |
|     - | 10112 | ` * path+slash+entry for a directory iterator. Every accessor that works on the` |
|     - | 10113 | ` * whole pathname goes through here.` |
|     - | 10114 | ` */` |
|   569 | 10115 | `static const char * SfiName(ph7_vm *pVm,ph7_class_instance *pThis,int *pnLen)` |
|     2 | 10116 | `{` |
|   571 | 10117 | `	if( SplDirIs(pVm,pThis) ){` |
|   169 | 10118 | `		return SplDirName(pVm,pThis,pnLen);` |
|     - | 10119 | `	}` |
|   404 | 10120 | `	return SfiStr(pThis,SFI_N,pnLen);` |
|   287 | 10121 | `}` |
|     - | 10122 | `/*` |
|     - | 10123 | `` * php's "the file name without the path": the slice after `path` + its slash when`` |
|     - | 10124 | ` * the path is a real prefix, and the whole name otherwise. getFilename(),` |
|     - | 10125 | ` * getBasename() and getExtension() all start here.` |
|     - | 10126 | ` */` |
|     - | 10127 | `/* A GlobIterator's path comes from its STREAM rather than from its slot` |
|     - | 10128 | ` * (defined with the directory machinery below); 0 for any other object. */` |
|     - | 10129 | `static const char * SplDirGlobPath(ph7_vm *pVm,ph7_class_instance *pThis,int *pnLen);` |
|   140 | 10130 | `static const char * SfiTail(ph7_vm *pVm,ph7_class_instance *pThis,int *pnLen)` |
|     1 | 10131 | `{` |
|   141 | 10132 | `	int nName = 0,nPath = 0;` |
|   141 | 10133 | `	const char *zName = SfiName(pVm,pThis,&nName);` |
|     - | 10134 | `	/* The name was JOINED from the walk's path, which for a glob handle is the` |
|     - | 10135 | ``	 * current match's directory and not the `glob://pattern` in the slot --`` |
|     - | 10136 | `	 * measuring against the slot left the whole joined name here. */` |
|   141 | 10137 | `	if( SplDirGlobPath(pVm,pThis,&nPath) == 0 ){` |
|   137 | 10138 | `		SfiStr(pThis,SFI_P,&nPath);` |
|    68 | 10139 | `	}` |
|   141 | 10140 | `	if( nPath > 0 && nPath < nName ){` |
|    77 | 10141 | `		*pnLen = nName - (nPath + 1);` |
|    77 | 10142 | `		return &zName[nPath + 1];` |
|     - | 10143 | `	}` |
|    65 | 10144 | `	*pnLen = nName;` |
|    65 | 10145 | `	return zName;` |
|    71 | 10146 | `}` |
|     - | 10147 | `/* The path this instance stands for, as a NUL-terminated buffer the VFS can take. */` |
|   151 | 10148 | `static sxi32 SfiPathBuf(ph7_vm *pVm,ph7_class_instance *pThis,char *zBuf,int nBuf)` |
|     2 | 10149 | `{` |
|   153 | 10150 | `	int nName = 0;` |
|   153 | 10151 | `	const char *zName = SfiName(pVm,pThis,&nName);` |
|   153 | 10152 | `	if( nName < 1 \|\| nName >= nBuf ){` |
|   ! 0 | 10153 | `		return SXERR_INVALID;` |
|     - | 10154 | `	}` |
|   153 | 10155 | `	SyMemcpy(zName,zBuf,(sxu32)nName);` |
|   153 | 10156 | `	zBuf[nName] = 0;` |
|   153 | 10157 | `	return SXRET_OK;` |
|    78 | 10158 | `}` |
|     - | 10159 | `/* The two refusals an accessor may owe before it reads anything (below). */` |
|     - | 10160 | `static int SfoChecked(ph7_context *pCtx,sxi32 *pRc);` |
|     - | 10161 | `/* The open the SplFileObject constructor and openFile() share (below). iCtxArg` |
|     - | 10162 | ` * is the php POSITION of the context argument, which differs between the two` |
|     - | 10163 | ` * spellings and is what a refused context is blamed on. */` |
|     - | 10164 | `static sxi32 SfoOpen(ph7_context *pCtx,ph7_class_instance *pThis,ph7_value *pPath,` |
|     - | 10165 | `	const char *zMode,int nMode,int bUseInclude,ph7_value *pCtxArg,int iCtxArg);` |
|     - | 10166 | `/*` |
|     - | 10167 | ` * php's get_file_name() ahead of an accessor that needs a path: an object whose` |
|     - | 10168 | ` * parent constructor never ran has no name AT ALL and raises Error rather than` |
|     - | 10169 | ` * failing a stat -- which for a directory iterator is the case where the open` |
|     - | 10170 | ` * never happened. Answers 0 when the caller must return *pRc.` |
|     - | 10171 | ` *` |
|     - | 10172 | ` * The SplFileObject family reaches these same accessors by inheritance and` |
|     - | 10173 | ` * refuses EARLIER and differently: php gives those classes a get_method handler` |
|     - | 10174 | ` * that stops every method on an uninitialized instance, so that check runs` |
|     - | 10175 | ` * first here too.` |
|     - | 10176 | ` */` |
|   137 | 10177 | `static int SfiDirReady(ph7_context *pCtx,sxi32 *pRc)` |
|     1 | 10178 | `{` |
|   138 | 10179 | `	ph7_vm *pVm = pCtx->pVm;` |
|   138 | 10180 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   138 | 10181 | `	if( !SfoChecked(pCtx,pRc) ){` |
|     3 | 10182 | `		return 0;` |
|     - | 10183 | `	}` |
|   136 | 10184 | `	if( SplDirIs(pVm,pThis) && SplDirState(pVm,pThis) == 0 ){` |
|     7 | 10185 | `		*pRc = PH7_VmThrowException(pCtx,"Error","Object not initialized");` |
|     7 | 10186 | `		return 0;` |
|     - | 10187 | `	}` |
|   130 | 10188 | `	return 1;` |
|    70 | 10189 | `}` |
|     - | 10190 | `/*` |
|     - | 10191 | ` * php's FileInfoFunction: the stat that backs one accessor, with the failure` |
|     - | 10192 | ` * promoted to a RuntimeException carrying the WARNING php would otherwise print.` |
|     - | 10193 | ` * The two lstat users say "Lstat failed" there, which is php's own text.` |
|     - | 10194 | ` */` |
|    64 | 10195 | `static sxi32 SfiStat(ph7_context *pCtx,const char *zMethod,int bLstat,ph7_value *pOut)` |
|     1 | 10196 | `{` |
|    65 | 10197 | `	ph7_vm *pVm = pCtx->pVm;` |
|    65 | 10198 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    65 | 10199 | `	const ph7_vfs *pVfs = pVm->pEngine->pVfs;` |
|     - | 10200 | `	ph7_value sWorker;` |
|     - | 10201 | `	char zPath[4096];` |
|    65 | 10202 | `	int rc = -1;` |
|     - | 10203 | `	sxi32 rcReady;` |
|    65 | 10204 | `	if( !SfiDirReady(pCtx,&rcReady) ){` |
|     5 | 10205 | `		return rcReady;` |
|     - | 10206 | `	}` |
|    61 | 10207 | `	if( PH7_MemObjToHashmap(pOut) != SXRET_OK ){` |
|   ! 0 | 10208 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 10209 | `	}` |
|    61 | 10210 | `	PH7_MemObjInit(pVm,&sWorker);` |
|    61 | 10211 | `	if( SfiPathBuf(pVm,pThis,zPath,(int)sizeof(zPath)) == SXRET_OK ){` |
|     - | 10212 | `		/* A path a userland stream wrapper owns is ITS stat, not the VFS's --` |
|     - | 10213 | `		 * php runs SplFileInfo's accessors through the same` |
|     - | 10214 | `		 * php_stream_url_stat_path every free function uses. zPath is a local` |
|     - | 10215 | `		 * buffer, so it survives the PHP the wrapper runs. */` |
|     - | 10216 | `		ph7_int64 aVal[13];` |
|    91 | 10217 | `		int rcU = PH7_VfsUserStatFields(pCtx,zPath,` |
|    30 | 10218 | `			bLstat ? PH7_STAT_ASK_LSTAT : PH7_STAT_ASK_STAT,aVal);` |
|    61 | 10219 | `		if( PH7_CALLBACK_UNWOUND(rcU) ){` |
|     - | 10220 | `			/* The wrapper threw: that exception is the answer, and stacking` |
|     - | 10221 | `			 * SplFileInfo's own RuntimeException on top of it left the first one` |
|     - | 10222 | `			 * already caught and the second one UNCAUGHT. */` |
|     3 | 10223 | `			PH7_MemObjRelease(&sWorker);` |
|     3 | 10224 | `			return rcU;` |
|     - | 10225 | `		}` |
|    59 | 10226 | `		if( rcU != PHL_URLSTAT_NOWRAP ){` |
|    21 | 10227 | `			rc = rcU == PHL_URLSTAT_OK ? PH7_VfsStatFill(pOut,&sWorker,aVal) : -1;` |
|    49 | 10228 | `		}else if( pVfs ){` |
|    39 | 10229 | `			if( bLstat ){` |
|   ! 0 | 10230 | `				rc = pVfs->xlStat ? pVfs->xlStat(zPath,pOut,&sWorker) : -1;` |
|   ! 0 | 10231 | `			}else{` |
|    39 | 10232 | `				rc = pVfs->xStat ? pVfs->xStat(zPath,pOut,&sWorker) : -1;` |
|     - | 10233 | `			}` |
|    19 | 10234 | `		}` |
|    29 | 10235 | `	}` |
|    59 | 10236 | `	PH7_MemObjRelease(&sWorker);` |
|    59 | 10237 | `	if( rc != PH7_OK ){` |
|    19 | 10238 | `		int nName = 0;` |
|    19 | 10239 | `		const char *zName = SfiName(pVm,pThis,&nName);` |
|    28 | 10240 | `		return PH7_VmThrowException(pCtx,"RuntimeException",` |
|     9 | 10241 | `			"SplFileInfo::%s(): %s failed for %.*s",zMethod,bLstat ? "Lstat" : "stat",` |
|     9 | 10242 | `			nName,zName);` |
|     - | 10243 | `	}` |
|    41 | 10244 | `	return PH7_OK;` |
|    33 | 10245 | `}` |
|     - | 10246 | `/* One field of a stat array, as php's int. */` |
|    64 | 10247 | `static int SfiStatField(ph7_context *pCtx,const char *zMethod,int bLstat,const char *zField,` |
|     - | 10248 | `	sxi64 *piOut)` |
|     1 | 10249 | `{` |
|     - | 10250 | `	ph7_value sStat,*pField;` |
|     - | 10251 | `	sxi32 rc;` |
|    65 | 10252 | `	*piOut = 0;` |
|    65 | 10253 | `	PH7_MemObjInit(pCtx->pVm,&sStat);` |
|    65 | 10254 | `	rc = SfiStat(pCtx,zMethod,bLstat,&sStat);` |
|    65 | 10255 | `	if( rc != PH7_OK ){` |
|    25 | 10256 | `		PH7_MemObjRelease(&sStat);` |
|    25 | 10257 | `		return rc;` |
|     - | 10258 | `	}` |
|    41 | 10259 | `	pField = ph7_array_fetch(&sStat,zField,(int)SyStrlen(zField));` |
|    41 | 10260 | `	if( pField ){` |
|    41 | 10261 | `		*piOut = ph7_value_to_int64(pField);` |
|    20 | 10262 | `	}` |
|    41 | 10263 | `	PH7_MemObjRelease(&sStat);` |
|    41 | 10264 | `	return PH7_OK;` |
|    33 | 10265 | `}` |
|     - | 10266 | `/* The eight stat accessors that answer an int, all with the same body. */` |
|    64 | 10267 | `static int SfiStatInt(ph7_context *pCtx,const char *zMethod,const char *zField)` |
|     1 | 10268 | `{` |
|    65 | 10269 | `	sxi64 iVal = 0;` |
|    65 | 10270 | `	sxi32 rc = SfiStatField(pCtx,zMethod,FALSE,zField,&iVal);` |
|    65 | 10271 | `	if( rc != PH7_OK ){` |
|    25 | 10272 | `		return rc;` |
|     - | 10273 | `	}` |
|    41 | 10274 | `	ph7_result_int64(pCtx,iVal);` |
|    41 | 10275 | `	return PH7_OK;` |
|    33 | 10276 | `}` |
|   156 | 10277 | `static int vm_builtin_SplFileInfo_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10278 | `{` |
|   157 | 10279 | `	ph7_vm *pVm = pCtx->pVm;` |
|   157 | 10280 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 10281 | `	const char *zPath;` |
|   157 | 10282 | `	int nPath = 0;` |
|   157 | 10283 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|   ! 0 | 10284 | `		return PH7_OK;` |
|     - | 10285 | `	}` |
|   157 | 10286 | `	zPath = ph7_value_to_string(apArg[0],&nPath);` |
|   157 | 10287 | `	SfiSetName(pVm,pThis,zPath,nPath);` |
|   157 | 10288 | `	return PH7_OK;` |
|    79 | 10289 | `}` |
|    54 | 10290 | `static int vm_builtin_SplFileInfo_getPath(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10291 | `{` |
|    55 | 10292 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 10293 | `	sxi32 rcChk;` |
|    55 | 10294 | `	int nPath = 0;` |
|     - | 10295 | `	const char *zPath;` |
|    27 | 10296 | `	SXUNUSED(nArg);` |
|    27 | 10297 | `	SXUNUSED(apArg);` |
|    55 | 10298 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|     3 | 10299 | `		return rcChk;` |
|     - | 10300 | `	}` |
|     - | 10301 | `	/* A GlobIterator answers the directory of the CURRENT match (see` |
|     - | 10302 | `	 * SplDirGlobPath): the pattern in its slot names no directory. */` |
|    53 | 10303 | `	zPath = SplDirGlobPath(pCtx->pVm,pThis,&nPath);` |
|    53 | 10304 | `	if( zPath == 0 ){` |
|    49 | 10305 | `		zPath = SfiStr(pThis,SFI_P,&nPath);` |
|    24 | 10306 | `	}` |
|    53 | 10307 | `	ph7_result_string(pCtx,zPath,nPath);` |
|    53 | 10308 | `	return PH7_OK;` |
|    28 | 10309 | `}` |
|     - | 10310 | `/*` |
|     - | 10311 | `` * php's getPathname() is `spl_filesystem_object_get_pathname`, and for a DIR it`` |
|     - | 10312 | ` * answers NOTHING once the walk has run out — the empty string, without` |
|     - | 10313 | ``  * materializing the lazy name the stat family would still build (`getSize()` `` |
|     - | 10314 | `` * past the end stats the directory itself, and `var_dump` shows the difference).`` |
|     - | 10315 | ` */` |
|    72 | 10316 | `static int vm_builtin_SplFileInfo_getPathname(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 10317 | `{` |
|     - | 10318 | `	sxi32 rcChk;` |
|    74 | 10319 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    74 | 10320 | `	int nName = 0;` |
|     - | 10321 | `	const char *zName;` |
|    36 | 10322 | `	SXUNUSED(nArg);` |
|    36 | 10323 | `	SXUNUSED(apArg);` |
|    74 | 10324 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|     5 | 10325 | `		return rcChk;` |
|     - | 10326 | `	}` |
|    70 | 10327 | `	if( SplDirIs(pCtx->pVm,pThis) && SplDirAtEnd(pThis) ){` |
|     7 | 10328 | `		ph7_result_string(pCtx,"",0);` |
|     7 | 10329 | `		return PH7_OK;` |
|     - | 10330 | `	}` |
|    64 | 10331 | `	zName = SfiName(pCtx->pVm,pThis,&nName);` |
|    64 | 10332 | `	ph7_result_string(pCtx,zName,nName);` |
|    64 | 10333 | `	return PH7_OK;` |
|    38 | 10334 | `}` |
|    62 | 10335 | `static int vm_builtin_SplFileInfo_getFilename(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10336 | `{` |
|     - | 10337 | `	sxi32 rcChk;` |
|    63 | 10338 | `	int nTail = 0;` |
|    63 | 10339 | `	const char *zTail = SfiTail(pCtx->pVm,PH7_ContextThis(pCtx),&nTail);` |
|    31 | 10340 | `	SXUNUSED(nArg);` |
|    31 | 10341 | `	SXUNUSED(apArg);` |
|    63 | 10342 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|     5 | 10343 | `		return rcChk;` |
|     - | 10344 | `	}` |
|    59 | 10345 | `	ph7_result_string(pCtx,zTail,nTail);` |
|    59 | 10346 | `	return PH7_OK;` |
|    32 | 10347 | `}` |
|     - | 10348 | `/* php's getBasename(): php_basename() of the tail, suffix rule included. */` |
|    32 | 10349 | `static int vm_builtin_SplFileInfo_getBasename(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10350 | `{` |
|     - | 10351 | `	sxi32 rcChk;` |
|    33 | 10352 | `	int nTail = 0,nBase = 0;` |
|    33 | 10353 | `	const char *zTail = SfiTail(pCtx->pVm,PH7_ContextThis(pCtx),&nTail);` |
|    33 | 10354 | `	const char *zBase = PH7_ExtractBaseName(zTail,nTail,&nBase);` |
|    33 | 10355 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 10356 | `		return rcChk;` |
|     - | 10357 | `	}` |
|    33 | 10358 | `	if( nArg > 0 ){` |
|     5 | 10359 | `		int nSuffix = 0;` |
|     5 | 10360 | `		const char *zSuffix = ph7_value_to_string(apArg[0],&nSuffix);` |
|     4 | 10361 | `		if( nSuffix > 0 && nSuffix < nBase` |
|     4 | 10362 | `		 && SyMemcmp(&zBase[nBase - nSuffix],zSuffix,(sxu32)nSuffix) == 0 ){` |
|     3 | 10363 | `			nBase -= nSuffix;` |
|     1 | 10364 | `		}` |
|     2 | 10365 | `	}` |
|    33 | 10366 | `	ph7_result_string(pCtx,zBase,nBase);` |
|    33 | 10367 | `	return PH7_OK;` |
|    17 | 10368 | `}` |
|     - | 10369 | `/* php's getExtension(): everything after the LAST dot of the basename, and the` |
|     - | 10370 | ` * empty string when there is none -- a leading dot counts, so '.hidden' has the` |
|     - | 10371 | ` * extension 'hidden'. */` |
|    28 | 10372 | `static int vm_builtin_SplFileInfo_getExtension(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10373 | `{` |
|     - | 10374 | `	sxi32 rcChk;` |
|    29 | 10375 | `	int nTail = 0,nBase = 0,i;` |
|    29 | 10376 | `	const char *zTail = SfiTail(pCtx->pVm,PH7_ContextThis(pCtx),&nTail);` |
|    29 | 10377 | `	const char *zBase = PH7_ExtractBaseName(zTail,nTail,&nBase);` |
|    14 | 10378 | `	SXUNUSED(nArg);` |
|    14 | 10379 | `	SXUNUSED(apArg);` |
|    29 | 10380 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 10381 | `		return rcChk;` |
|     - | 10382 | `	}` |
|    89 | 10383 | `	for( i = nBase - 1 ; i >= 0 ; --i ){` |
|    77 | 10384 | `		if( zBase[i] == '.' ){` |
|    17 | 10385 | `			ph7_result_string(pCtx,&zBase[i+1],nBase - i - 1);` |
|    17 | 10386 | `			return PH7_OK;` |
|     - | 10387 | `		}` |
|    31 | 10388 | `	}` |
|    13 | 10389 | `	ph7_result_string(pCtx,"",0);` |
|    13 | 10390 | `	return PH7_OK;` |
|    15 | 10391 | `}` |
|    10 | 10392 | `static int vm_builtin_SplFileInfo_getPerms(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10393 | `{` |
|     5 | 10394 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    11 | 10395 | `	return SfiStatInt(pCtx,"getPerms","mode");` |
|     1 | 10396 | `}` |
|     4 | 10397 | `static int vm_builtin_SplFileInfo_getInode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10398 | `{` |
|     2 | 10399 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     5 | 10400 | `	return SfiStatInt(pCtx,"getInode","ino");` |
|     1 | 10401 | `}` |
|    24 | 10402 | `static int vm_builtin_SplFileInfo_getSize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10403 | `{` |
|    12 | 10404 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    25 | 10405 | `	return SfiStatInt(pCtx,"getSize","size");` |
|     1 | 10406 | `}` |
|     4 | 10407 | `static int vm_builtin_SplFileInfo_getOwner(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10408 | `{` |
|     2 | 10409 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     5 | 10410 | `	return SfiStatInt(pCtx,"getOwner","uid");` |
|     1 | 10411 | `}` |
|     4 | 10412 | `static int vm_builtin_SplFileInfo_getGroup(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10413 | `{` |
|     2 | 10414 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     5 | 10415 | `	return SfiStatInt(pCtx,"getGroup","gid");` |
|     1 | 10416 | `}` |
|     4 | 10417 | `static int vm_builtin_SplFileInfo_getATime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10418 | `{` |
|     2 | 10419 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     5 | 10420 | `	return SfiStatInt(pCtx,"getATime","atime");` |
|     1 | 10421 | `}` |
|    10 | 10422 | `static int vm_builtin_SplFileInfo_getMTime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10423 | `{` |
|     5 | 10424 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    11 | 10425 | `	return SfiStatInt(pCtx,"getMTime","mtime");` |
|     1 | 10426 | `}` |
|     4 | 10427 | `static int vm_builtin_SplFileInfo_getCTime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10428 | `{` |
|     2 | 10429 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     5 | 10430 | `	return SfiStatInt(pCtx,"getCTime","ctime");` |
|     1 | 10431 | `}` |
|     - | 10432 | `/*` |
|     - | 10433 | ` * php's getType() is FS_TYPE: an LSTAT, so a symlink answers "link" rather than` |
|     - | 10434 | ` * what it points at. The VFS's own xFiletype IS that question -- decoding a stat` |
|     - | 10435 | ` * mode here instead would have answered "unknown" on Windows, where the mode` |
|     - | 10436 | ` * field is not filled and the attributes are what carry the answer.` |
|     - | 10437 | ` */` |
|    14 | 10438 | `static int vm_builtin_SplFileInfo_getType(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10439 | `{` |
|    15 | 10440 | `	const ph7_vfs *pVfs = pCtx->pVm->pEngine->pVfs;` |
|    15 | 10441 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 10442 | `	char zPath[4096];` |
|    15 | 10443 | `	int rc = -1;` |
|     - | 10444 | `	sxi32 rcReady;` |
|     7 | 10445 | `	SXUNUSED(nArg);` |
|     7 | 10446 | `	SXUNUSED(apArg);` |
|    15 | 10447 | `	if( !SfiDirReady(pCtx,&rcReady) ){` |
|     3 | 10448 | `		return rcReady;` |
|     - | 10449 | `	}` |
|    13 | 10450 | `	if( SfiPathBuf(pCtx->pVm,pThis,zPath,(int)sizeof(zPath)) == SXRET_OK ){` |
|     - | 10451 | `		ph7_int64 aVal[13];` |
|    13 | 10452 | `		int rcU = PH7_VfsUserStatFields(pCtx,zPath,PH7_STAT_ASK_TYPE,aVal);` |
|    13 | 10453 | `		if( PH7_CALLBACK_UNWOUND(rcU) ){` |
|   ! 0 | 10454 | `			return rcU;` |
|     - | 10455 | `		}` |
|    13 | 10456 | `		if( rcU != PHL_URLSTAT_NOWRAP ){` |
|     7 | 10457 | `			if( rcU == PHL_URLSTAT_OK ){` |
|     7 | 10458 | `				PH7_VfsUserStatResult(pCtx,PH7_STAT_ASK_TYPE,aVal);` |
|     7 | 10459 | `				rc = PH7_OK;` |
|     4 | 10460 | `			}` |
|    10 | 10461 | `		}else if( pVfs && pVfs->xFiletype ){` |
|     7 | 10462 | `			rc = pVfs->xFiletype(zPath,pCtx);` |
|     3 | 10463 | `		}` |
|     6 | 10464 | `	}` |
|    13 | 10465 | `	if( rc != PH7_OK ){` |
|     3 | 10466 | `		int nName = 0;` |
|     3 | 10467 | `		const char *zName = SfiName(pCtx->pVm,pThis,&nName);` |
|     3 | 10468 | `		if( pCtx->pRet ){` |
|     3 | 10469 | `			PH7_MemObjRelease(pCtx->pRet);   /* xFiletype wrote "unknown" (rule 54) */` |
|     1 | 10470 | `		}` |
|     4 | 10471 | `		return PH7_VmThrowException(pCtx,"RuntimeException",` |
|     1 | 10472 | `			"SplFileInfo::getType(): Lstat failed for %.*s",nName,zName);` |
|     - | 10473 | `	}` |
|    11 | 10474 | `	return PH7_OK;` |
|     8 | 10475 | `}` |
|     - | 10476 | `/* The six predicates: a VFS question each, and never a diagnostic -- php answers` |
|     - | 10477 | ` * false for a path that does not exist. */` |
|    57 | 10478 | `static int SfiPredicate(ph7_context *pCtx,int (*xTest)(const char *),int eAsk)` |
|     1 | 10479 | `{` |
|     - | 10480 | `	char zPath[4096];` |
|    58 | 10481 | `	int bYes = 0;` |
|     - | 10482 | `	sxi32 rcReady;` |
|    58 | 10483 | `	if( !SfiDirReady(pCtx,&rcReady) ){` |
|     3 | 10484 | `		return rcReady;` |
|     - | 10485 | `	}` |
|    56 | 10486 | `	if( SfiPathBuf(pCtx->pVm,PH7_ContextThis(pCtx),zPath,(int)sizeof(zPath)) == SXRET_OK ){` |
|     - | 10487 | `		/* Same door as the free functions: a wrapper's record answers the six,` |
|     - | 10488 | `		 * and a miss is the plain false php answers (these asks are QUIET). */` |
|     - | 10489 | `		ph7_int64 aVal[13];` |
|    56 | 10490 | `		int rcU = PH7_VfsUserStatFields(pCtx,zPath,eAsk,aVal);` |
|    56 | 10491 | `		if( PH7_CALLBACK_UNWOUND(rcU) ){` |
|    15 | 10492 | `			return rcU;` |
|     - | 10493 | `		}` |
|    54 | 10494 | `		if( rcU != PHL_URLSTAT_NOWRAP ){` |
|    25 | 10495 | `			if( rcU == PHL_URLSTAT_OK ){` |
|    25 | 10496 | `				PH7_VfsUserStatResult(pCtx,eAsk,aVal);` |
|    13 | 10497 | `			}else{` |
|   ! 0 | 10498 | `				ph7_result_bool(pCtx,0);` |
|     - | 10499 | `			}` |
|    25 | 10500 | `			return PH7_OK;` |
|     - | 10501 | `		}` |
|    30 | 10502 | `		if( xTest ){` |
|    30 | 10503 | `			bYes = xTest(zPath) == PH7_OK;` |
|    15 | 10504 | `		}` |
|    15 | 10505 | `	}` |
|    30 | 10506 | `	ph7_result_bool(pCtx,bYes);` |
|    30 | 10507 | `	return PH7_OK;` |
|    30 | 10508 | `}` |
|     4 | 10509 | `static int vm_builtin_SplFileInfo_isWritable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10510 | `{` |
|     2 | 10511 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     5 | 10512 | `	return SfiPredicate(pCtx,pCtx->pVm->pEngine->pVfs ? pCtx->pVm->pEngine->pVfs->xWritable : 0,PH7_STAT_ASK_IS_W);` |
|     1 | 10513 | `}` |
|    10 | 10514 | `static int vm_builtin_SplFileInfo_isReadable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10515 | `{` |
|     5 | 10516 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    11 | 10517 | `	return SfiPredicate(pCtx,pCtx->pVm->pEngine->pVfs ? pCtx->pVm->pEngine->pVfs->xReadable : 0,PH7_STAT_ASK_IS_R);` |
|     1 | 10518 | `}` |
|     2 | 10519 | `static int vm_builtin_SplFileInfo_isExecutable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10520 | `{` |
|     1 | 10521 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     3 | 10522 | `	return SfiPredicate(pCtx,pCtx->pVm->pEngine->pVfs ? pCtx->pVm->pEngine->pVfs->xExecutable : 0,PH7_STAT_ASK_IS_X);` |
|     1 | 10523 | `}` |
|    21 | 10524 | `static int vm_builtin_SplFileInfo_isFile(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10525 | `{` |
|    11 | 10526 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    22 | 10527 | `	return SfiPredicate(pCtx,pCtx->pVm->pEngine->pVfs ? pCtx->pVm->pEngine->pVfs->xIsfile : 0,PH7_STAT_ASK_IS_FILE);` |
|     1 | 10528 | `}` |
|    10 | 10529 | `static int vm_builtin_SplFileInfo_isDir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10530 | `{` |
|     5 | 10531 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    11 | 10532 | `	return SfiPredicate(pCtx,pCtx->pVm->pEngine->pVfs ? pCtx->pVm->pEngine->pVfs->xIsdir : 0,PH7_STAT_ASK_IS_DIR);` |
|     1 | 10533 | `}` |
|    10 | 10534 | `static int vm_builtin_SplFileInfo_isLink(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10535 | `{` |
|     5 | 10536 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    11 | 10537 | `	return SfiPredicate(pCtx,pCtx->pVm->pEngine->pVfs ? pCtx->pVm->pEngine->pVfs->xIslink : 0,PH7_STAT_ASK_IS_LINK);` |
|     1 | 10538 | `}` |
|     - | 10539 | `/* php's getLinkTarget(): readlink(), and a RuntimeException naming the errno text` |
|     - | 10540 | ` * when it fails -- which includes asking a plain file for its target. */` |
|     2 | 10541 | `static int vm_builtin_SplFileInfo_getLinkTarget(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10542 | `{` |
|     3 | 10543 | `	const ph7_vfs *pVfs = pCtx->pVm->pEngine->pVfs;` |
|     3 | 10544 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 10545 | `	char zPath[4096];` |
|     3 | 10546 | `	int rc = -1;` |
|     - | 10547 | `	sxi32 rcReady;` |
|     1 | 10548 | `	SXUNUSED(nArg);` |
|     1 | 10549 | `	SXUNUSED(apArg);` |
|     3 | 10550 | `	if( !SfiDirReady(pCtx,&rcReady) ){` |
|   ! 0 | 10551 | `		return rcReady;` |
|     - | 10552 | `	}` |
|     2 | 10553 | `	if( pVfs && pVfs->xReadlink` |
|     3 | 10554 | `	 && SfiPathBuf(pCtx->pVm,pThis,zPath,(int)sizeof(zPath)) == SXRET_OK ){` |
|     3 | 10555 | `		rc = pVfs->xReadlink(zPath,pCtx);` |
|     1 | 10556 | `	}` |
|     3 | 10557 | `	if( rc != PH7_OK ){` |
|     2 | 10558 | `		int nName = 0;` |
|     2 | 10559 | `		const char *zName = SfiName(pCtx->pVm,pThis,&nName);` |
|     3 | 10560 | `		return PH7_VmThrowException(pCtx,"RuntimeException",` |
|     2 | 10561 | `			"Unable to read link %.*s, error: %s",nName,zName,VfsStrerror(errno));` |
|     - | 10562 | `	}` |
|     1 | 10563 | `	return PH7_OK;` |
|     2 | 10564 | `}` |
|     6 | 10565 | `static int vm_builtin_SplFileInfo_getRealPath(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10566 | `{` |
|     - | 10567 | `	sxi32 rcChk;` |
|     7 | 10568 | `	const ph7_vfs *pVfs = pCtx->pVm->pEngine->pVfs;` |
|     - | 10569 | `	char zPath[4096];` |
|     7 | 10570 | `	int rc = -1;` |
|     3 | 10571 | `	SXUNUSED(nArg);` |
|     3 | 10572 | `	SXUNUSED(apArg);` |
|     7 | 10573 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 10574 | `		return rcChk;` |
|     - | 10575 | `	}` |
|     6 | 10576 | `	if( pVfs && pVfs->xRealpath` |
|     7 | 10577 | `	 && SfiPathBuf(pCtx->pVm,PH7_ContextThis(pCtx),zPath,(int)sizeof(zPath)) == SXRET_OK ){` |
|     7 | 10578 | `		rc = pVfs->xRealpath(zPath,pCtx);` |
|     3 | 10579 | `	}` |
|     7 | 10580 | `	if( rc != PH7_OK ){` |
|     5 | 10581 | `		ph7_result_bool(pCtx,0);   /* php answers false, with no diagnostic */` |
|     2 | 10582 | `	}` |
|     7 | 10583 | `	return PH7_OK;` |
|     4 | 10584 | `}` |
|     - | 10585 | `/*` |
|     - | 10586 | ` * The class getFileInfo()/getPathInfo() build with: the argument when it names` |
|     - | 10587 | ` * one, this instance's info_class otherwise. php refuses anything not derived` |
|     - | 10588 | ` * from SplFileInfo, and words the refusal from the ARGUMENT position.` |
|     - | 10589 | ` *` |
|     - | 10590 | ` * The two do not word it identically, and the split is php's: getPathInfo()` |
|     - | 10591 | ` * takes the argument through zend's CLASS-NAME parameter, which reports a name` |
|     - | 10592 | `` * NOTHING declares as `must be a valid class name or null` and leaves the`` |
|     - | 10593 | ` * derived-from check to the SPL code behind it, while getFileInfo() reports` |
|     - | 10594 | ` * both failures with the derived-from sentence. bValidFirst says which of the` |
|     - | 10595 | ` * two this door is.` |
|     - | 10596 | ` */` |
|   118 | 10597 | `static sxi32 SfiInfoClass(ph7_context *pCtx,const char *zMethod,ph7_value *pArg,` |
|     - | 10598 | `	int bValidFirst,ph7_class **ppOut)` |
|     2 | 10599 | `{` |
|   120 | 10600 | `	ph7_vm *pVm = pCtx->pVm;` |
|   120 | 10601 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   120 | 10602 | `	ph7_class *pBase = PH7_VmExtractClass(pVm,"SplFileInfo",sizeof("SplFileInfo")-1,FALSE,0);` |
|   120 | 10603 | `	ph7_class *pClass = 0;` |
|     - | 10604 | `	const char *zName;` |
|   120 | 10605 | `	int nName = 0;` |
|   126 | 10606 | `	if( pArg && (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|    35 | 10607 | `		sxi32 rcSv = PH7_ValueToStringUV(pCtx,pArg,&zName,&nName);` |
|    35 | 10608 | `		if( rcSv != SXRET_OK ){` |
|     5 | 10609 | `			return rcSv;` |
|     - | 10610 | `		}` |
|    31 | 10611 | `		pClass = PH7_VmExtractClass(pVm,zName,(sxu32)nName,TRUE,0);` |
|    31 | 10612 | `		if( pClass == 0 && bValidFirst ){` |
|    10 | 10613 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 10614 | `				"SplFileInfo::%s(): Argument #1 ($class) must be a valid class name "` |
|     3 | 10615 | `				"or null, %.*s given",zMethod,nName,zName);` |
|     - | 10616 | `		}` |
|    25 | 10617 | `		if( pClass == 0 \|\| pBase == 0 \|\| !PH7_VmInstanceOf(pClass,pBase) ){` |
|    19 | 10618 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 10619 | `				"SplFileInfo::%s(): Argument #1 ($class) must be a class name derived "` |
|     6 | 10620 | `				"from SplFileInfo or null, %.*s given",zMethod,nName,zName);` |
|     - | 10621 | `		}` |
|     7 | 10622 | `	}else{` |
|    86 | 10623 | `		int nCur = 0;` |
|    86 | 10624 | `		const char *zCur = SfiStr(pThis,SFI_IC,&nCur);` |
|    86 | 10625 | `		pClass = PH7_VmExtractClass(pVm,zCur,(sxu32)nCur,TRUE,0);` |
|     - | 10626 | `	}` |
|    98 | 10627 | `	if( pClass == 0 ){` |
|   ! 0 | 10628 | `		pClass = pBase;` |
|   ! 0 | 10629 | `	}` |
|    98 | 10630 | `	*ppOut = pClass;` |
|    98 | 10631 | `	return pClass ? PH7_OK : PH7_ContextMemoryError(pCtx);` |
|    61 | 10632 | `}` |
|     - | 10633 | `/*` |
|     - | 10634 | ` * Build one of these for a path. php calls the CONSTRUCTOR when the class` |
|     - | 10635 | ` * declares its own (a subclass may want it) and fills the slots directly when it` |
|     - | 10636 | ` * does not -- reproduced here, because a subclass constructor is user code and` |
|     - | 10637 | ` * skipping it would be visible.` |
|     - | 10638 | ` */` |
|    88 | 10639 | `static sxi32 SfiMakeInfoEx(ph7_context *pCtx,ph7_class *pClass,const char *zPath,int nPath,` |
|     - | 10640 | `	const char *zDir,int nDir)` |
|     2 | 10641 | `{` |
|    90 | 10642 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 10643 | `	ph7_class_instance *pNew;` |
|     - | 10644 | `	ph7_class_method *pCons;` |
|    90 | 10645 | `	sxi32 rc = SXRET_OK;` |
|    90 | 10646 | `	pNew = PH7_NewClassInstance(pVm,pClass);` |
|    90 | 10647 | `	if( pNew == 0 ){` |
|   ! 0 | 10648 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 10649 | `	}` |
|    90 | 10650 | `	pNew->iRef++;` |
|    90 | 10651 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|    91 | 10652 | `	if( pCons && (pCons->sFunc.iFlags & VM_FUNC_NATIVE) == 0 ){` |
|     - | 10653 | `		ph7_value sArg,*apArg[1];` |
|     3 | 10654 | `		PH7_MemObjInitFromString(pVm,&sArg,0);` |
|     3 | 10655 | `		PH7_MemObjStringAppend(&sArg,zPath,(sxu32)nPath);` |
|     3 | 10656 | `		apArg[0] = &sArg;` |
|     3 | 10657 | `		rc = PH7_VmCallClassMethod(pVm,pNew,pCons,0,1,apArg);` |
|     3 | 10658 | `		PH7_MemObjRelease(&sArg);` |
|    89 | 10659 | `	}else if( zDir ){` |
|     - | 10660 | `		/* php's create_type for a DIR source hands the child BOTH strings rather` |
|     - | 10661 | `		 * than re-deriving the second: the path is the directory being walked, so` |
|     - | 10662 | ``		 * `new DirectoryIterator('/')`'s entry keeps the path `/` and the name`` |
|     - | 10663 | ``		 * `//x` that the walk itself produced. */`` |
|    68 | 10664 | `		PH7_NativeSetAttrStr(pVm,pNew,SFI_N,zPath,nPath);` |
|    68 | 10665 | `		PH7_NativeSetAttrStr(pVm,pNew,SFI_P,zDir,nDir);` |
|    35 | 10666 | `	}else{` |
|    21 | 10667 | `		SfiSetName(pVm,pNew,zPath,nPath);` |
|     - | 10668 | `	}` |
|    90 | 10669 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 10670 | `		PH7_ClassInstanceUnref(pNew);` |
|   ! 0 | 10671 | `		return rc;` |
|     - | 10672 | `	}` |
|    90 | 10673 | `	PH7_NativeResultObject(pCtx,pNew);` |
|    90 | 10674 | `	PH7_ClassInstanceUnref(pNew);` |
|    90 | 10675 | `	return PH7_OK;` |
|    46 | 10676 | `}` |
|     8 | 10677 | `static sxi32 SfiMakeInfo(ph7_context *pCtx,ph7_class *pClass,const char *zPath,int nPath)` |
|     1 | 10678 | `{` |
|     9 | 10679 | `	return SfiMakeInfoEx(pCtx,pClass,zPath,nPath,0,0);` |
|     1 | 10680 | `}` |
|    34 | 10681 | `static int vm_builtin_SplFileInfo_getFileInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10682 | `{` |
|     - | 10683 | `	sxi32 rcChk,rc;` |
|    35 | 10684 | `	ph7_vm *pVm = pCtx->pVm;` |
|    35 | 10685 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    35 | 10686 | `	ph7_class *pClass = 0;` |
|    35 | 10687 | `	int nName = 0,nDir = 0;` |
|    35 | 10688 | `	const char *zName,*zDir = 0;` |
|    35 | 10689 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 10690 | `		return rcChk;` |
|     - | 10691 | `	}` |
|    35 | 10692 | `	rc = SfiInfoClass(pCtx,"getFileInfo",nArg > 0 ? apArg[0] : 0,0,&pClass);` |
|    35 | 10693 | `	if( rc != PH7_OK ){` |
|    13 | 10694 | `		return rc;` |
|     - | 10695 | `	}` |
|    23 | 10696 | `	if( SplDirIs(pVm,pThis) ){` |
|     9 | 10697 | `		if( SplDirState(pVm,pThis) == 0 ){` |
|     3 | 10698 | `			return PH7_VmThrowException(pCtx,"Error","Object not initialized");` |
|     - | 10699 | `		}` |
|     - | 10700 | `		/* php's create_type refuses to describe an entry that is not there —` |
|     - | 10701 | `		 * the same RuntimeException a FilesystemIterator::current() past the end` |
|     - | 10702 | `		 * raises, because it goes through this. */` |
|     7 | 10703 | `		if( SplDirAtEnd(pThis) ){` |
|     3 | 10704 | `			return PH7_VmThrowException(pCtx,"RuntimeException","Could not open file");` |
|     - | 10705 | `		}` |
|     5 | 10706 | `		zDir = SfiStr(pThis,SFI_P,&nDir);` |
|     2 | 10707 | `	}` |
|    19 | 10708 | `	zName = SfiName(pVm,pThis,&nName);` |
|    19 | 10709 | `	return SfiMakeInfoEx(pCtx,pClass,zName,nName,zDir,nDir);` |
|    18 | 10710 | `}` |
|     - | 10711 | `/* php's getPathInfo(): the DIRNAME of the pathname, and nothing at all (null) for` |
|     - | 10712 | ` * an empty one — which for a directory iterator includes one that has run out. */` |
|    22 | 10713 | `static int vm_builtin_SplFileInfo_getPathInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10714 | `{` |
|     - | 10715 | `	sxi32 rcChk,rc;` |
|    23 | 10716 | `	ph7_vm *pVm = pCtx->pVm;` |
|    23 | 10717 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    23 | 10718 | `	ph7_class *pClass = 0;` |
|    23 | 10719 | `	int nName = 0,nDir = 0;` |
|     - | 10720 | `	const char *zName,*zDir;` |
|    23 | 10721 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 10722 | `		return rcChk;` |
|     - | 10723 | `	}` |
|    23 | 10724 | `	rc = SfiInfoClass(pCtx,"getPathInfo",nArg > 0 ? apArg[0] : 0,1,&pClass);` |
|    23 | 10725 | `	if( rc != PH7_OK ){` |
|    11 | 10726 | `		return rc;` |
|     - | 10727 | `	}` |
|    13 | 10728 | `	if( SplDirIs(pVm,pThis) && SplDirAtEnd(pThis) ){` |
|     3 | 10729 | `		ph7_result_null(pCtx);` |
|     3 | 10730 | `		return PH7_OK;` |
|     - | 10731 | `	}` |
|    11 | 10732 | `	zName = SfiName(pVm,pThis,&nName);` |
|    11 | 10733 | `	if( nName < 1 ){` |
|     3 | 10734 | `		ph7_result_null(pCtx);` |
|     3 | 10735 | `		return PH7_OK;` |
|     - | 10736 | `	}` |
|     9 | 10737 | `	zDir = PH7_ExtractDirName(zName,nName,&nDir);` |
|     9 | 10738 | `	return SfiMakeInfo(pCtx,pClass,zDir,nDir);` |
|    12 | 10739 | `}` |
|    20 | 10740 | `static int vm_builtin_SplFileInfo_setInfoClass(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10741 | `{` |
|     - | 10742 | `	sxi32 rcChk;` |
|    21 | 10743 | `	ph7_vm *pVm = pCtx->pVm;` |
|    21 | 10744 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    21 | 10745 | `	ph7_class *pBase = PH7_VmExtractClass(pVm,"SplFileInfo",sizeof("SplFileInfo")-1,FALSE,0);` |
|     - | 10746 | `	ph7_class *pClass;` |
|    21 | 10747 | `	const char *zName = "SplFileInfo";` |
|    21 | 10748 | `	int nName = (int)sizeof("SplFileInfo")-1;` |
|    21 | 10749 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 10750 | `		return rcChk;` |
|     - | 10751 | `	}` |
|    21 | 10752 | `	if( nArg > 0 ){` |
|     - | 10753 | `		/* php's cast here is the USER-VISIBLE one: an array warns` |
|     - | 10754 | ``		 * `Array to string conversion` and is refused as the name "Array", and an`` |
|     - | 10755 | `		 * object with no __toString() is the catchable` |
|     - | 10756 | ``		 * `Object of class X could not be converted to string` rather than a`` |
|     - | 10757 | `		 * refusal naming the placeholder "Object". */` |
|    21 | 10758 | `		sxi32 rcSv = PH7_ValueToStringUV(pCtx,apArg[0],&zName,&nName);` |
|    21 | 10759 | `		if( rcSv != SXRET_OK ){` |
|     3 | 10760 | `			return rcSv;` |
|     - | 10761 | `		}` |
|     9 | 10762 | `	}` |
|    19 | 10763 | `	pClass = PH7_VmExtractClass(pVm,zName,(sxu32)nName,TRUE,0);` |
|    19 | 10764 | `	if( pClass == 0 \|\| pBase == 0 \|\| !PH7_VmInstanceOf(pClass,pBase) ){` |
|     - | 10765 | `		/* php words this one WITHOUT the "or null" half getFileInfo() has: the` |
|     - | 10766 | `		 * parameter is not nullable here. */` |
|    19 | 10767 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 10768 | `			"SplFileInfo::setInfoClass(): Argument #1 ($class) must be a class name "` |
|     6 | 10769 | `			"derived from SplFileInfo, %.*s given",nName,zName);` |
|     - | 10770 | `	}` |
|     7 | 10771 | `	PH7_NativeSetAttrStr(pVm,pThis,SFI_IC,zName,nName);` |
|     7 | 10772 | `	return PH7_OK;` |
|    11 | 10773 | `}` |
|     - | 10774 | `/*` |
|     - | 10775 | ` * php's setFileClass(): the class openFile() will build. Worded WITHOUT the` |
|     - | 10776 | ` * "or null" half getFileInfo() has, like its info_class twin -- the parameter` |
|     - | 10777 | ` * is not nullable, and a null coerces to the empty name the refusal prints.` |
|     - | 10778 | ` */` |
|    32 | 10779 | `static int vm_builtin_SplFileInfo_setFileClass(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10780 | `{` |
|     - | 10781 | `	sxi32 rcChk;` |
|    33 | 10782 | `	ph7_vm *pVm = pCtx->pVm;` |
|    33 | 10783 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    33 | 10784 | `	ph7_class *pBase = PH7_VmExtractClass(pVm,"SplFileObject",sizeof("SplFileObject")-1,FALSE,0);` |
|     - | 10785 | `	ph7_class *pClass;` |
|    33 | 10786 | `	const char *zName = "SplFileObject";` |
|    33 | 10787 | `	int nName = (int)sizeof("SplFileObject")-1;` |
|    33 | 10788 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 10789 | `		return rcChk;` |
|     - | 10790 | `	}` |
|    33 | 10791 | `	if( nArg > 0 ){` |
|    31 | 10792 | `		sxi32 rcSv = PH7_ValueToStringUV(pCtx,apArg[0],&zName,&nName);` |
|    31 | 10793 | `		if( rcSv != SXRET_OK ){` |
|     3 | 10794 | `			return rcSv;` |
|     - | 10795 | `		}` |
|    14 | 10796 | `	}` |
|    31 | 10797 | `	pClass = PH7_VmExtractClass(pVm,zName,(sxu32)nName,TRUE,0);` |
|    31 | 10798 | `	if( pClass == 0 \|\| pBase == 0 \|\| !PH7_VmInstanceOf(pClass,pBase) ){` |
|    25 | 10799 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 10800 | `			"SplFileInfo::setFileClass(): Argument #1 ($class) must be a class name "` |
|     8 | 10801 | `			"derived from SplFileObject, %.*s given",nName,zName);` |
|     - | 10802 | `	}` |
|    15 | 10803 | `	PH7_NativeSetAttrStr(pVm,pThis,SFI_FC,zName,nName);` |
|    15 | 10804 | `	return PH7_OK;` |
|    17 | 10805 | `}` |
|     - | 10806 | `/*` |
|     - | 10807 | ` * php's openFile(): spl_filesystem_object_create_type for SPL_FS_FILE. The` |
|     - | 10808 | ` * class is this instance's file_class, and php CALLS its constructor when the` |
|     - | 10809 | ` * class declares one of its own -- with the pathname and the MODE, two` |
|     - | 10810 | ` * arguments, which is how a subclass gets to see what it was opened as. When it` |
|     - | 10811 | ` * does not, php fills the slots and opens directly, which is what SfoOpen()` |
|     - | 10812 | ` * does here; the warning that open would print is promoted to a` |
|     - | 10813 | ` * RuntimeException worded from THIS method's name.` |
|     - | 10814 | ` */` |
|    30 | 10815 | `static int vm_builtin_SplFileInfo_openFile(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10816 | `{` |
|     - | 10817 | `	sxi32 rcChk,rc;` |
|    31 | 10818 | `	ph7_vm *pVm = pCtx->pVm;` |
|    31 | 10819 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 10820 | `	ph7_class_instance *pNew;` |
|     - | 10821 | `	ph7_class_method *pCons;` |
|     - | 10822 | `	ph7_class *pClass;` |
|     - | 10823 | `	ph7_value sPath;` |
|     - | 10824 | `	SyBlob sCls,sDir;` |
|    31 | 10825 | `	const char *zMode = "r",*zName,*zDir,*zCls;` |
|    31 | 10826 | `	int nMode = 1,nName = 0,nDir = 0,nCls = 0;` |
|    31 | 10827 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 10828 | `		return rcChk;` |
|     - | 10829 | `	}` |
|    31 | 10830 | `	if( SplDirIs(pVm,pThis) ){` |
|     5 | 10831 | `		if( SplDirState(pVm,pThis) == 0 ){` |
|   ! 0 | 10832 | `			return PH7_VmThrowException(pCtx,"Error","Object not initialized");` |
|     - | 10833 | `		}` |
|     - | 10834 | `		/* php's create_type refuses to describe an entry that is not there. */` |
|     5 | 10835 | `		if( SplDirAtEnd(pThis) ){` |
|     3 | 10836 | `			return PH7_VmThrowException(pCtx,"RuntimeException","Could not open file");` |
|     - | 10837 | `		}` |
|     1 | 10838 | `	}` |
|     - | 10839 | `	/* Both strings are copied OUT before anything allocates or runs user code:` |
|     - | 10840 | `	 * they are slots of the object being read, and a subclass constructor below` |
|     - | 10841 | `	 * can rewrite or unset either one (rule 22). */` |
|    29 | 10842 | `	zCls = SfiStr(pThis,SFI_FC,&nCls);` |
|    29 | 10843 | `	SyBlobInit(&sCls,&pVm->sAllocator);` |
|    29 | 10844 | `	SyBlobAppend(&sCls,zCls,(sxu32)nCls);` |
|    29 | 10845 | `	zDir = SfiStr(pThis,SFI_P,&nDir);` |
|    29 | 10846 | `	SyBlobInit(&sDir,&pVm->sAllocator);` |
|    29 | 10847 | `	SyBlobAppend(&sDir,zDir,(sxu32)nDir);` |
|    43 | 10848 | `	pClass = PH7_VmExtractClass(pVm,(const char *)SyBlobData(&sCls),` |
|    14 | 10849 | `		SyBlobLength(&sCls),TRUE,0);` |
|    29 | 10850 | `	if( pClass == 0 ){` |
|   ! 0 | 10851 | `		pClass = PH7_VmExtractClass(pVm,"SplFileObject",sizeof("SplFileObject")-1,FALSE,0);` |
|   ! 0 | 10852 | `	}` |
|    29 | 10853 | `	if( pClass == 0 ){` |
|   ! 0 | 10854 | `		SyBlobRelease(&sCls);` |
|   ! 0 | 10855 | `		SyBlobRelease(&sDir);` |
|   ! 0 | 10856 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 10857 | `	}` |
|    29 | 10858 | `	if( nArg > 0 ){` |
|    11 | 10859 | `		zMode = ph7_value_to_string(apArg[0],&nMode);` |
|     5 | 10860 | `	}` |
|     - | 10861 | `	/* The path is handed on as a VALUE: the opener needs one, and the slot it` |
|     - | 10862 | `	 * would otherwise borrow belongs to an object about to be written to. */` |
|    29 | 10863 | `	zName = SfiName(pVm,pThis,&nName);` |
|    29 | 10864 | `	PH7_MemObjInitFromString(pVm,&sPath,0);` |
|    29 | 10865 | `	PH7_MemObjStringAppend(&sPath,zName,(sxu32)nName);` |
|    29 | 10866 | `	pNew = PH7_NewClassInstance(pVm,pClass);` |
|    29 | 10867 | `	if( pNew == 0 ){` |
|   ! 0 | 10868 | `		PH7_MemObjRelease(&sPath);` |
|   ! 0 | 10869 | `		SyBlobRelease(&sCls);` |
|   ! 0 | 10870 | `		SyBlobRelease(&sDir);` |
|   ! 0 | 10871 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 10872 | `	}` |
|    29 | 10873 | `	pNew->iRef++;` |
|    29 | 10874 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|    30 | 10875 | `	if( pCons && (pCons->sFunc.iFlags & VM_FUNC_NATIVE) == 0 ){` |
|     - | 10876 | `		ph7_value sMode,*apCtor[2];` |
|     3 | 10877 | `		PH7_MemObjInitFromString(pVm,&sMode,0);` |
|     3 | 10878 | `		PH7_MemObjStringAppend(&sMode,zMode,(sxu32)nMode);` |
|     3 | 10879 | `		apCtor[0] = &sPath;` |
|     3 | 10880 | `		apCtor[1] = &sMode;` |
|     3 | 10881 | `		rc = PH7_VmCallClassMethod(pVm,pNew,pCons,0,2,apCtor);` |
|     3 | 10882 | `		PH7_MemObjRelease(&sMode);` |
|     2 | 10883 | `	}else{` |
|    44 | 10884 | `		rc = SfoOpen(pCtx,pNew,&sPath,zMode,nMode,` |
|    15 | 10885 | `			nArg > 1 ? ph7_value_to_bool(apArg[1]) : FALSE,` |
|    15 | 10886 | `			nArg > 2 && (apArg[2]->iFlags & MEMOBJ_NULL) == 0 ? apArg[2] : 0,3);` |
|    27 | 10887 | `		if( rc == PH7_OK ){` |
|     - | 10888 | `			/* php hands the child the SOURCE's path rather than re-deriving one` |
|     - | 10889 | `			 * from the name: a directory entry's getPath() keeps pointing at the` |
|     - | 10890 | ``			 * directory being walked, and a `php://temp` source keeps the EMPTY`` |
|     - | 10891 | ``			 * path a URI's last slash would otherwise cut to `php:/`. */`` |
|    31 | 10892 | `			PH7_NativeSetAttrStr(pVm,pNew,SFI_P,` |
|    20 | 10893 | `				(const char *)SyBlobData(&sDir),(int)SyBlobLength(&sDir));` |
|    10 | 10894 | `		}` |
|     - | 10895 | `	}` |
|    29 | 10896 | `	PH7_MemObjRelease(&sPath);` |
|    29 | 10897 | `	SyBlobRelease(&sCls);` |
|    29 | 10898 | `	SyBlobRelease(&sDir);` |
|    29 | 10899 | `	if( rc != PH7_OK ){` |
|     7 | 10900 | `		PH7_ClassInstanceUnref(pNew);` |
|     7 | 10901 | `		return rc;` |
|     - | 10902 | `	}` |
|    23 | 10903 | `	PH7_NativeResultObject(pCtx,pNew);` |
|    23 | 10904 | `	PH7_ClassInstanceUnref(pNew);` |
|    23 | 10905 | `	return PH7_OK;` |
|    16 | 10906 | `}` |
|     - | 10907 | ``/* One `"\0Class\0member" => <string>` entry of a debug array. */`` |
|    72 | 10908 | `static void SfiDebugStr(ph7_vm *pVm,ph7_value *pOut,const char *zKey,int nKey,` |
|     - | 10909 | `	const char *zVal,int nVal)` |
|     1 | 10910 | `{` |
|     - | 10911 | `	ph7_value sKey,sVal;` |
|    73 | 10912 | `	PH7_MemObjInitFromString(pVm,&sKey,0);` |
|    73 | 10913 | `	PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKey);` |
|    73 | 10914 | `	PH7_MemObjInitFromString(pVm,&sVal,0);` |
|    73 | 10915 | `	PH7_MemObjStringAppend(&sVal,zVal,(sxu32)nVal);` |
|    73 | 10916 | `	ph7_array_add_elem(pOut,&sKey,&sVal);` |
|    73 | 10917 | `	PH7_MemObjRelease(&sKey);` |
|    73 | 10918 | `	PH7_MemObjRelease(&sVal);` |
|    73 | 10919 | `}` |
|     - | 10920 | `/*` |
|     - | 10921 | ` * php's get_debug_info: the two slots under their MANGLED private names, which is` |
|     - | 10922 | ` * how a class with no declared properties still shows something. __debugInfo()` |
|     - | 10923 | ` * hands back the same array.` |
|     - | 10924 | ` *` |
|     - | 10925 | `` * A DIRECTORY iterator shows two more (`glob`, always false here — PHL has no`` |
|     - | 10926 | `` * GlobIterator — and `subPathName`), and shows `fileName` only if the pathname`` |
|     - | 10927 | `` * has been MATERIALIZED: php's `if (intern->file_name)` is the lazy name's`` |
|     - | 10928 | ` * presence, so an exhausted iterator has one key fewer until something asks it` |
|     - | 10929 | ` * for a path.` |
|     - | 10930 | ` */` |
|    24 | 10931 | `static sxi32 SfiFillDebug(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut)` |
|     1 | 10932 | `{` |
|     - | 10933 | ``	/* php's test is `type == SPL_FS_DIR`, which an object whose constructor never`` |
|     - | 10934 | ``	 * ran does NOT satisfy: it shows the single `pathName` key a bare SplFileInfo`` |
|     - | 10935 | `	 * would, and neither of the two directory ones. */` |
|    25 | 10936 | `	int bDir = SplDirIs(pVm,pThis) && SplDirState(pVm,pThis) != 0;` |
|    25 | 10937 | `	int nName = 0,nTail = 0,nSub = 0;` |
|     - | 10938 | `	const char *zName;` |
|    25 | 10939 | `	int bLive = bDir ? !SplDirAtEnd(pThis) : !SplDirIs(pVm,pThis);` |
|    25 | 10940 | `	if( bLive ){` |
|    19 | 10941 | `		zName = SfiName(pVm,pThis,&nName);` |
|    10 | 10942 | `	}else{` |
|     7 | 10943 | `		zName = SfiStr(pThis,SFI_N,&nName);   /* whatever a stat left behind, or "" */` |
|     7 | 10944 | `		nName = 0;` |
|     - | 10945 | `	}` |
|    37 | 10946 | `	SfiDebugStr(pVm,pOut,"\0SplFileInfo\0pathName",` |
|    12 | 10947 | `		(int)sizeof("\0SplFileInfo\0pathName")-1,zName,nName);` |
|     - | 10948 | `	/* Re-read: the append above may have moved the slot the first read borrowed. */` |
|    25 | 10949 | `	SfiStr(pThis,SFI_N,&nName);` |
|    25 | 10950 | `	if( bLive \|\| (bDir && nName > 0) ){` |
|    19 | 10951 | `		const char *zTail = SfiTail(pVm,pThis,&nTail);` |
|    28 | 10952 | `		SfiDebugStr(pVm,pOut,"\0SplFileInfo\0fileName",` |
|     9 | 10953 | `			(int)sizeof("\0SplFileInfo\0fileName")-1,zTail,nTail);` |
|     9 | 10954 | `	}` |
|    25 | 10955 | `	if( bDir ){` |
|     - | 10956 | `		ph7_value sKey,sVal;` |
|     - | 10957 | `		const char *zSub;` |
|    13 | 10958 | `		int nSlot = 0;` |
|    13 | 10959 | `		const char *zSlot = SfiStr(pThis,SFI_P,&nSlot);` |
|    13 | 10960 | `		PH7_MemObjInitFromString(pVm,&sKey,0);` |
|    13 | 10961 | `		PH7_MemObjStringAppend(&sKey,"\0DirectoryIterator\0glob",` |
|     - | 10962 | `			sizeof("\0DirectoryIterator\0glob")-1);` |
|     - | 10963 | `		/* php's own test, on the slot rather than on the stream: the whole` |
|     - | 10964 | ``		 * `glob://pattern` when the path carries that prefix, and FALSE for an`` |
|     - | 10965 | `		 * ordinary directory. A GlobIterator's constructor puts the prefix on` |
|     - | 10966 | `		 * whether or not the caller wrote it, so this is always the pattern. */` |
|    12 | 10967 | `		if( nSlot >= (int)sizeof("glob://")-1` |
|    13 | 10968 | `		 && SyMemcmp(zSlot,"glob://",sizeof("glob://")-1) == 0 ){` |
|     7 | 10969 | `			PH7_MemObjInitFromString(pVm,&sVal,0);` |
|     7 | 10970 | `			PH7_MemObjStringAppend(&sVal,zSlot,(sxu32)nSlot);` |
|     4 | 10971 | `		}else{` |
|     7 | 10972 | `			PH7_MemObjInitFromBool(pVm,&sVal,0);` |
|     - | 10973 | `		}` |
|    13 | 10974 | `		ph7_array_add_elem(pOut,&sKey,&sVal);` |
|    13 | 10975 | `		PH7_MemObjRelease(&sKey);` |
|    13 | 10976 | `		PH7_MemObjRelease(&sVal);` |
|    13 | 10977 | `		zSub = SfiStr(pThis,SDI_S,&nSub);` |
|    19 | 10978 | `		SfiDebugStr(pVm,pOut,"\0RecursiveDirectoryIterator\0subPathName",` |
|     6 | 10979 | `			(int)sizeof("\0RecursiveDirectoryIterator\0subPathName")-1,zSub,nSub);` |
|     6 | 10980 | `	}` |
|    25 | 10981 | `	if( SfoDev(pThis) ){` |
|     - | 10982 | ``		/* php's `type == SPL_FS_FILE` arm: the open mode and the two CSV`` |
|     - | 10983 | `		 * characters, which is the only place any of the three is visible. An` |
|     - | 10984 | `		 * SplFileObject whose constructor never ran is still SPL_FS_INFO and` |
|     - | 10985 | `		 * shows none of them. */` |
|     7 | 10986 | `		int nMode = 0,c;` |
|     7 | 10987 | `		const char *zMode = SfiStr(pThis,SFO_M,&nMode);` |
|    10 | 10988 | `		SfiDebugStr(pVm,pOut,"\0SplFileObject\0openMode",` |
|     3 | 10989 | `			(int)sizeof("\0SplFileObject\0openMode")-1,zMode,nMode);` |
|     7 | 10990 | `		c = (int)PH7_NativeAttrInt(pThis,SFO_D);` |
|     - | 10991 | `		{` |
|     7 | 10992 | `			char zChar = (char)c;` |
|     7 | 10993 | `			SfiDebugStr(pVm,pOut,"\0SplFileObject\0delimiter",` |
|     - | 10994 | `				(int)sizeof("\0SplFileObject\0delimiter")-1,&zChar,1);` |
|     7 | 10995 | `			zChar = (char)PH7_NativeAttrInt(pThis,SFO_EN);` |
|     7 | 10996 | `			SfiDebugStr(pVm,pOut,"\0SplFileObject\0enclosure",` |
|     - | 10997 | `				(int)sizeof("\0SplFileObject\0enclosure")-1,&zChar,1);` |
|     - | 10998 | `		}` |
|     3 | 10999 | `	}` |
|    25 | 11000 | `	return PH7_OK;` |
|     1 | 11001 | `}` |
|    12 | 11002 | `static sxi32 SfiPresent(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)` |
|     1 | 11003 | `{` |
|    13 | 11004 | `	if( !bDebug ){` |
|     7 | 11005 | `		return PH7_OK;   /* php's (array) cast and var_export show nothing */` |
|     - | 11006 | `	}` |
|     7 | 11007 | `	return SfiFillDebug(pVm,pThis,pOut);` |
|     7 | 11008 | `}` |
|    18 | 11009 | `static int vm_builtin_SplFileInfo_debugInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 11010 | `{` |
|     - | 11011 | `	sxi32 rcChk;` |
|    19 | 11012 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 11013 | `	ph7_value sOut;` |
|     9 | 11014 | `	SXUNUSED(nArg);` |
|     9 | 11015 | `	SXUNUSED(apArg);` |
|    19 | 11016 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 11017 | `		return rcChk;` |
|     - | 11018 | `	}` |
|    19 | 11019 | `	PH7_MemObjInit(pVm,&sOut);` |
|    19 | 11020 | `	if( PH7_MemObjToHashmap(&sOut) != SXRET_OK ){` |
|   ! 0 | 11021 | `		PH7_MemObjRelease(&sOut);` |
|   ! 0 | 11022 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 11023 | `	}` |
|    19 | 11024 | `	SfiFillDebug(pVm,PH7_ContextThis(pCtx),&sOut);` |
|    19 | 11025 | `	ph7_result_value(pCtx,&sOut);` |
|    19 | 11026 | `	PH7_MemObjRelease(&sOut);` |
|    19 | 11027 | `	return PH7_OK;` |
|    10 | 11028 | `}` |
|     - | 11029 | `/* php's own escape hatch for a subclass that forgot to call parent::__construct.` |
|     - | 11030 | ` * It exists to be THROWN, and php marks it deprecated rather than removing it. */` |
|     2 | 11031 | `static int vm_builtin_SplFileInfo_badState(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 11032 | `{` |
|     1 | 11033 | `	SXUNUSED(nArg);` |
|     1 | 11034 | `	SXUNUSED(apArg);` |
|     3 | 11035 | `	return PH7_VmThrowException(pCtx,"Error",` |
|     - | 11036 | `		"The parent constructor was not called: the object is in an invalid state");` |
|     1 | 11037 | `}` |
|     - | 11038 | `/*` |
|     - | 11039 | ` * The declaration. Method ORDER is spl_directory.stub.php's; openFile() and` |
|     - | 11040 | ` * setFileClass() are absent because SplFileObject is (recorded), and everything else is` |
|     - | 11041 | ` * php's, tentative return types included.` |
|     - | 11042 | ` */` |
|  8445 | 11043 | `static sxi32 VmInstallSplFileInfo(ph7_vm *pVm)` |
|     5 | 11044 | `{` |
|     - | 11045 | `	static const PH7_NativePropDef aSfiProp[] = {` |
|     - | 11046 | `		{ SFI_N,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },` |
|     - | 11047 | `		{ SFI_P,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },` |
|     - | 11048 | `		{ SFI_IC, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN,` |
|     - | 11049 | `		  { 0, 0, PH7_NATIVE_VAL_STRING, 0, "SplFileInfo", 0.0 }, 0 },` |
|     - | 11050 | `		{ SFI_FC, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN,` |
|     - | 11051 | `		  { 0, 0, PH7_NATIVE_VAL_STRING, 0, "SplFileObject", 0.0 }, 0 },` |
|     - | 11052 | `	};` |
|     - | 11053 | `	static const PH7_NativeMethodDef aSfiMethod[] = {` |
|     - | 11054 | `		{ "__construct",   PH7_MOD_PUBLIC, "string $filename", 0,` |
|     - | 11055 | `		  vm_builtin_SplFileInfo_construct },` |
|     - | 11056 | `		{ "getPath",       PH7_MOD_PUBLIC, "", "@string", vm_builtin_SplFileInfo_getPath },` |
|     - | 11057 | `		{ "getFilename",   PH7_MOD_PUBLIC, "", "@string", vm_builtin_SplFileInfo_getFilename },` |
|     - | 11058 | `		{ "getExtension",  PH7_MOD_PUBLIC, "", "@string", vm_builtin_SplFileInfo_getExtension },` |
|     - | 11059 | `		{ "getBasename",   PH7_MOD_PUBLIC, "string $suffix = \"\"", "@string",` |
|     - | 11060 | `		  vm_builtin_SplFileInfo_getBasename },` |
|     - | 11061 | `		{ "getPathname",   PH7_MOD_PUBLIC, "", "@string", vm_builtin_SplFileInfo_getPathname },` |
|     - | 11062 | `		{ "getPerms",      PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_SplFileInfo_getPerms },` |
|     - | 11063 | `		{ "getInode",      PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_SplFileInfo_getInode },` |
|     - | 11064 | `		{ "getSize",       PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_SplFileInfo_getSize },` |
|     - | 11065 | `		{ "getOwner",      PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_SplFileInfo_getOwner },` |
|     - | 11066 | `		{ "getGroup",      PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_SplFileInfo_getGroup },` |
|     - | 11067 | `		{ "getATime",      PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_SplFileInfo_getATime },` |
|     - | 11068 | `		{ "getMTime",      PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_SplFileInfo_getMTime },` |
|     - | 11069 | `		{ "getCTime",      PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_SplFileInfo_getCTime },` |
|     - | 11070 | `		{ "getType",       PH7_MOD_PUBLIC, "", "@string\|false", vm_builtin_SplFileInfo_getType },` |
|     - | 11071 | `		{ "isWritable",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplFileInfo_isWritable },` |
|     - | 11072 | `		{ "isReadable",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplFileInfo_isReadable },` |
|     - | 11073 | `		{ "isExecutable",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplFileInfo_isExecutable },` |
|     - | 11074 | `		{ "isFile",        PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplFileInfo_isFile },` |
|     - | 11075 | `		{ "isDir",         PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplFileInfo_isDir },` |
|     - | 11076 | `		{ "isLink",        PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplFileInfo_isLink },` |
|     - | 11077 | `		{ "getLinkTarget", PH7_MOD_PUBLIC, "", "@string\|false",` |
|     - | 11078 | `		  vm_builtin_SplFileInfo_getLinkTarget },` |
|     - | 11079 | `		{ "getRealPath",   PH7_MOD_PUBLIC, "", "@string\|false",` |
|     - | 11080 | `		  vm_builtin_SplFileInfo_getRealPath },` |
|     - | 11081 | `		{ "getFileInfo",   PH7_MOD_PUBLIC, "~?string $class = null", "@SplFileInfo",` |
|     - | 11082 | `		  vm_builtin_SplFileInfo_getFileInfo },` |
|     - | 11083 | `		{ "getPathInfo",   PH7_MOD_PUBLIC, "~?string $class = null", "@?SplFileInfo",` |
|     - | 11084 | `		  vm_builtin_SplFileInfo_getPathInfo },` |
|     - | 11085 | `		/* spl_directory.stub.php's order, which is what every method-enumeration` |
|     - | 11086 | `		 * surface reports: openFile, setFileClass, setInfoClass. */` |
|     - | 11087 | `		{ "openFile",      PH7_MOD_PUBLIC,` |
|     - | 11088 | `		  "string $mode = \"r\", bool $useIncludePath = false, $context = null",` |
|     - | 11089 | `		  "@SplFileObject", vm_builtin_SplFileInfo_openFile },` |
|     - | 11090 | `		{ "setFileClass",  PH7_MOD_PUBLIC, "~string $class = SplFileObject::class", "@void",` |
|     - | 11091 | `		  vm_builtin_SplFileInfo_setFileClass },` |
|     - | 11092 | `		{ "setInfoClass",  PH7_MOD_PUBLIC, "~string $class = SplFileInfo::class", "@void",` |
|     - | 11093 | `		  vm_builtin_SplFileInfo_setInfoClass },` |
|     - | 11094 | `		{ "__toString",    PH7_MOD_PUBLIC, "", "string", vm_builtin_SplFileInfo_getPathname },` |
|     - | 11095 | `		{ "__debugInfo",   PH7_MOD_PUBLIC, "", "@array", vm_builtin_SplFileInfo_debugInfo },` |
|     - | 11096 | `		/* php does NOT mark this one tentative -- it is the only method here that` |
|     - | 11097 | ``		 * prints `Return [ void ]` rather than `Tentative return [ void ]`. */`` |
|     - | 11098 | `		{ "_bad_state_ex", PH7_MOD_PUBLIC\|PH7_MOD_FINAL, "", "void",` |
|     - | 11099 | `		  vm_builtin_SplFileInfo_badState },` |
|     - | 11100 | `	};` |
|     - | 11101 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - | 11102 | `		{ "SplFileInfo", 0, "Stringable", PH7_CLASS_NOSERIALIZE,` |
|     - | 11103 | `		  aSfiMethod, SX_ARRAYSIZE(aSfiMethod), 0, 0,` |
|     - | 11104 | `		  aSfiProp, SX_ARRAYSIZE(aSfiProp), 0, 0, SfiPresent },` |
|     - | 11105 | `	};` |
|  8450 | 11106 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|     5 | 11107 | `}` |
|     - | 11108 | `/*` |
|     - | 11109 | ` * ---------------------------------------------------------------------------` |
|     - | 11110 | ` * DirectoryIterator, FilesystemIterator and RecursiveDirectoryIterator.` |
|     - | 11111 | ` *` |
|     - | 11112 | `` * php's `spl_filesystem_object` holds an OPEN directory stream and ONE entry at`` |
|     - | 11113 | `` * a time (`u.dir.dirp`, `u.dir.entry`, `u.dir.index`); the chunk read the whole`` |
|     - | 11114 | ` * directory into an array at construction, and every difference followed from` |
|     - | 11115 | `` * that one choice (rule 52). php's `rewind()` re-opens the directory and SEES A`` |
|     - | 11116 | `` * FILE CREATED SINCE, its `key()` is the read index rather than an array offset,`` |
|     - | 11117 | `` * its `seek()` walks FORWARD through the object's own valid()/next() — so a`` |
|     - | 11118 | `` * subclass overriding either is obeyed — and a `clone` opens the directory again`` |
|     - | 11119 | ` * and reads forward to the same index rather than sharing a cursor.` |
|     - | 11120 | ` *` |
|     - | 11121 | ` * The handle cannot live in a property slot, because CLONE copies slots: two` |
|     - | 11122 | ` * objects would share one directory stream and close it twice. It lives in` |
|     - | 11123 | `` * `pVm->hDirHandle` keyed by the instance, with the class's xRelease closing it,`` |
|     - | 11124 | ` * and a clone — finding no entry of its own — re-opens on first use, which IS` |
|     - | 11125 | ` * php's clone handler, deferred. The one thing that deferral costs is a clone` |
|     - | 11126 | ` * whose directory is removed before it is first used: php has the stream open` |
|     - | 11127 | ` * already and answers, PHL raises "Object not initialized" (recorded).` |
|     - | 11128 | ` *` |
|     - | 11129 | `` * `file_name` is LAZY here as it is in php: the path, a slash and the current`` |
|     - | 11130 | ` * entry, invalidated by every read and rebuilt on demand. That is php-visible` |
|     - | 11131 | `` * twice over -- `getPathname()` answers "" past the end while `getSize()` stats`` |
|     - | 11132 | `` * the DIRECTORY (the join with an empty entry), and `var_dump` shows one key`` |
|     - | 11133 | ` * fewer until something has asked.` |
|     - | 11134 | ` *` |
|     - | 11135 | `` * The chunk had also INVENTED `DirectoryIterator::getFlags()` (php has no such`` |
|     - | 11136 | ``  * method; only FilesystemIterator does), inherited SplFileInfo's `__toString()` `` |
|     - | 11137 | ` * where php aliases getFilename(), and mis-stated two constants:` |
|     - | 11138 | ` * FOLLOW_SYMLINKS is 16384 (it said 512, colliding with nothing but reading as` |
|     - | 11139 | ` * false for every real flags value) and OTHER_MODE_MASK is 28672.` |
|     - | 11140 | ` * ---------------------------------------------------------------------------` |
|     - | 11141 | ` */` |
|     - | 11142 | `/* php's spl_directory.h flag set, verbatim -- the values the class constants` |
|     - | 11143 | ` * publish and the masks its accessors compare with. */` |
|     - | 11144 | `#define SDI_CURRENT_AS_FILEINFO 0x0000` |
|     - | 11145 | `#define SDI_CURRENT_AS_SELF     0x0010` |
|     - | 11146 | `#define SDI_CURRENT_AS_PATHNAME 0x0020` |
|     - | 11147 | `#define SDI_CURRENT_MODE_MASK   0x00F0` |
|     - | 11148 | `#define SDI_KEY_AS_PATHNAME     0x0000` |
|     - | 11149 | `#define SDI_KEY_AS_FILENAME     0x0100` |
|     - | 11150 | `#define SDI_KEY_MODE_MASK       0x0F00` |
|     - | 11151 | `#define SDI_SKIPDOTS            0x1000` |
|     - | 11152 | `#define SDI_UNIXPATHS           0x2000` |
|     - | 11153 | `#define SDI_FOLLOW_SYMLINKS     0x4000` |
|     - | 11154 | `#define SDI_OTHERS_MASK         0x7000` |
|     - | 11155 | `#define SDI_FLAGS_MASK (SDI_KEY_MODE_MASK\|SDI_CURRENT_MODE_MASK\|SDI_OTHERS_MASK)` |
|     - | 11156 |  |
|     - | 11157 | `/* php's DEFAULT_SLASH, and the UNIX_PATHS flag that overrides it. */` |
|   109 | 11158 | `static char SplDirSlash(sxi64 iFlags)` |
|     2 | 11159 | `{` |
|     - | 11160 | `#ifdef __WINNT__` |
|     2 | 11161 | `	return (iFlags & SDI_UNIXPATHS) ? '/' : '\\';` |
|     - | 11162 | `#else` |
|    55 | 11163 | `	SXUNUSED(iFlags);` |
|   109 | 11164 | `	return '/';` |
|     - | 11165 | `#endif` |
|     2 | 11166 | `}` |
|     - | 11167 | `/* php's spl_filesystem_is_dot. */` |
|   272 | 11168 | `static int SplDirIsDot(const char *zName,int nName)` |
|     2 | 11169 | `{` |
|   289 | 11170 | `	return (nName == 1 && zName[0] == '.')` |
|   314 | 11171 | `		\|\| (nName == 2 && zName[0] == '.' && zName[1] == '.');` |
|     2 | 11172 | `}` |
|     - | 11173 | ``/* Does this instance carry php's `u.dir` arm? Asked by the five SplFileInfo`` |
|     - | 11174 | ` * bodies that branch on the object TYPE, so it has to be the class question and` |
|     - | 11175 | ` * not "does it have a __e slot" — a user class may declare anything. */` |
|   872 | 11176 | `static int SplDirIs(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     2 | 11177 | `{` |
|     - | 11178 | `	ph7_class *pDir;` |
|   874 | 11179 | `	if( pThis == 0 ){` |
|   ! 0 | 11180 | `		return 0;` |
|     - | 11181 | `	}` |
|   874 | 11182 | `	pDir = PH7_VmExtractClass(pVm,"DirectoryIterator",sizeof("DirectoryIterator")-1,FALSE,0);` |
|   874 | 11183 | `	return pDir && PH7_VmInstanceOf(pThis->pClass,pDir);` |
|   439 | 11184 | `}` |
|     - | 11185 | ``/* php's `!intern->u.dir.entry.d_name[0]`: the walk has nothing to describe. */`` |
|   288 | 11186 | `static int SplDirAtEnd(ph7_class_instance *pThis)` |
|     2 | 11187 | `{` |
|   290 | 11188 | `	int nEntry = 0;` |
|   290 | 11189 | `	SfiStr(pThis,SDI_E,&nEntry);` |
|   290 | 11190 | `	return nEntry < 1;` |
|     2 | 11191 | `}` |
|     - | 11192 | `/* The registry entry for this instance, or 0. */` |
|  1228 | 11193 | `static VmDirHandle * SplDirFind(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     2 | 11194 | `{` |
|     - | 11195 | `	SyHashEntry *pEntry;` |
|  1230 | 11196 | `	if( pThis == 0 \|\| SyHashTotalEntry(&pVm->hDirHandle) < 1 ){` |
|    24 | 11197 | `		return 0;` |
|     - | 11198 | `	}` |
|  1208 | 11199 | `	pEntry = SyHashGet(&pVm->hDirHandle,(const void *)&pThis,sizeof(void *));` |
|  1208 | 11200 | `	return pEntry ? (VmDirHandle *)pEntry->pUserData : 0;` |
|   620 | 11201 | `}` |
|     - | 11202 | `/* Is this a GlobIterator? The CLASS question, asked where there may be no` |
|     - | 11203 | ` * handle to ask -- an instance whose parent constructor never ran has one. */` |
|   166 | 11204 | `static int SplGlobIs(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 | 11205 | `{` |
|     - | 11206 | `	ph7_class *pGlob;` |
|   167 | 11207 | `	if( pThis == 0 ){` |
|   ! 0 | 11208 | `		return 0;` |
|     - | 11209 | `	}` |
|   167 | 11210 | `	pGlob = PH7_VmExtractClass(pVm,"GlobIterator",sizeof("GlobIterator")-1,FALSE,0);` |
|   167 | 11211 | `	return pGlob && PH7_VmInstanceOf(pThis->pClass,pGlob) ? 1 : 0;` |
|    84 | 11212 | `}` |
|     - | 11213 | `/*` |
|     - | 11214 | ` * php's spl_filesystem_object_get_path for a GLOB handle: the directory of the` |
|     - | 11215 | ` * match the last read handed out, which the STREAM tracks and the object does` |
|     - | 11216 | `` * not. It moves with the walk -- `glob://a/` + `*` + `/` + `*.txt` reports `a/sub1`, then`` |
|     - | 11217 | `` * `a/sub2` -- it is the EMPTY string for a match with no slash in it, and it is`` |
|     - | 11218 | ` * cleared when the walk runs out, which is what makes getPathname() answer ""` |
|     - | 11219 | ` * past the end.` |
|     - | 11220 | ` *` |
|     - | 11221 | ` * Answers 0 for any other handle, whose path is the slot the constructor wrote.` |
|     - | 11222 | ` * Asked through SplDirFind() rather than SplDirState(): a handle that is not` |
|     - | 11223 | ` * open has no current match to have a directory OF, and GlobIterator is` |
|     - | 11224 | ` * uncloneable, so the re-open SplDirState() exists for cannot arise here.` |
|     - | 11225 | ` */` |
|   359 | 11226 | `static const char * SplDirGlobPath(ph7_vm *pVm,ph7_class_instance *pThis,int *pnLen)` |
|     2 | 11227 | `{` |
|   361 | 11228 | `	VmDirHandle *pH = SplDirFind(pVm,pThis);` |
|   361 | 11229 | `	*pnLen = 0;` |
|   361 | 11230 | `	if( pH == 0 \|\| pH->pStream == 0 \|\| !PH7_GlobStreamIs(pH->pStream) ){` |
|   301 | 11231 | `		return 0;` |
|     - | 11232 | `	}` |
|    61 | 11233 | `	return PH7_GlobStreamPath(pH->pHandle,pnLen);` |
|   182 | 11234 | `}` |
|     - | 11235 | `/* Close the handle this instance owns, if any. The class's xRelease, and the` |
|     - | 11236 | ` * first half of a re-open. */` |
|   114 | 11237 | `static void SplDirClose(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     2 | 11238 | `{` |
|   116 | 11239 | `	void *pData = 0;` |
|   114 | 11240 | `	if( SyHashDeleteEntry(&pVm->hDirHandle,(const void *)&pThis,sizeof(void *),&pData) == SXRET_OK` |
|   108 | 11241 | `	 && pData ){` |
|   100 | 11242 | `		VmDirHandle *pH = (VmDirHandle *)pData;` |
|   100 | 11243 | `		if( pH->pStream && pH->pStream->xCloseDir ){` |
|   100 | 11244 | `			pH->pStream->xCloseDir(pH->pHandle);` |
|    49 | 11245 | `		}` |
|   100 | 11246 | `		SyMemBackendFree(&pVm->sAllocator,pH);` |
|    49 | 11247 | `	}` |
|   116 | 11248 | `}` |
|     - | 11249 | `/*` |
|     - | 11250 | ` * Close every DIR the program still held at VM shutdown. xRelease (SplDirClose` |
|     - | 11251 | ` * above) covers an instance the program DESTROYED; an iterator alive at script` |
|     - | 11252 | ` * end reaches PH7_VmRelease with its handle still open, and the OS stream` |
|     - | 11253 | ` * behind it lives outside SyMemBackend -- the wholesale release frees the` |
|     - | 11254 | ` * VmDirHandle record and leaks the DIR (the leak checker is what noticed:` |
|     - | 11255 | ` * glibc's opendir buffer, ~32KB per survivor). Called from PH7_VmRelease` |
|     - | 11256 | ` * before the backend goes; the records themselves are backend memory.` |
|     - | 11257 | ` */` |
|  6995 | 11258 | `PH7_PRIVATE void PH7_SplDirVmRelease(ph7_vm *pVm)` |
|     5 | 11259 | `{` |
|     - | 11260 | `	SyHashEntry *pEntry;` |
|  7000 | 11261 | `	SyHashResetLoopCursor(&pVm->hDirHandle);` |
|  7000 | 11262 | `	while( (pEntry = SyHashGetNextEntry(&pVm->hDirHandle)) != 0 ){` |
|   ! 0 | 11263 | `		VmDirHandle *pH = (VmDirHandle *)pEntry->pUserData;` |
|   ! 0 | 11264 | `		if( pH && pH->pStream && pH->pStream->xCloseDir ){` |
|   ! 0 | 11265 | `			pH->pStream->xCloseDir(pH->pHandle);` |
|   ! 0 | 11266 | `		}` |
|   ! 0 | 11267 | `	}` |
|  7000 | 11268 | `}` |
|     - | 11269 | `/*` |
|     - | 11270 | ` * php's spl_filesystem_dir_read: invalidate the lazy name, then take ONE entry` |
|     - | 11271 | ` * from the stream; running out leaves the entry empty, which is what valid()` |
|     - | 11272 | ` * reports. The read goes through a scratch call context because the VFS reports` |
|     - | 11273 | ` * a name by writing a RESULT -- borrowing the method's own return slot would` |
|     - | 11274 | ` * append to whatever the body is about to answer (rule 54).` |
|     - | 11275 | ` */` |
|   404 | 11276 | `static void SplDirRead(ph7_vm *pVm,ph7_class_instance *pThis,VmDirHandle *pH)` |
|     2 | 11277 | `{` |
|     - | 11278 | `	ph7_context sCtx;` |
|     - | 11279 | `	ph7_value sOut;` |
|   406 | 11280 | `	int rc = -1;` |
|   406 | 11281 | `	PH7_NativeSetAttrStr(pVm,pThis,SFI_N,"",0);` |
|   406 | 11282 | `	PH7_MemObjInit(pVm,&sOut);` |
|   406 | 11283 | `	VmInitCallContext(&sCtx,pVm,0,&sOut,0);` |
|   406 | 11284 | `	if( pH && pH->pStream && pH->pStream->xReadDir ){` |
|   406 | 11285 | `		rc = pH->pStream->xReadDir(pH->pHandle,&sCtx);` |
|   207 | 11286 | `	}` |
|   406 | 11287 | `	if( rc == PH7_OK ){` |
|   358 | 11288 | `		int nName = 0;` |
|   358 | 11289 | `		const char *zName = ph7_value_to_string(&sOut,&nName);` |
|   358 | 11290 | `		PH7_NativeSetAttrStr(pVm,pThis,SDI_E,zName,nName);` |
|   185 | 11291 | `	}else{` |
|    50 | 11292 | `		PH7_NativeSetAttrStr(pVm,pThis,SDI_E,"",0);` |
|     - | 11293 | `	}` |
|   406 | 11294 | `	VmReleaseCallContext(&sCtx);` |
|   406 | 11295 | `	PH7_MemObjRelease(&sOut);` |
|   406 | 11296 | `}` |
|     - | 11297 | `/* php's read loop: one entry, then more while SKIP_DOTS and this is a dot. */` |
|   270 | 11298 | `static void SplDirReadSkip(ph7_vm *pVm,ph7_class_instance *pThis,VmDirHandle *pH)` |
|     2 | 11299 | `{` |
|   272 | 11300 | `	sxi64 iFlags = PH7_NativeAttrInt(pThis,SDI_F);` |
|   260 | 11301 | `	for(;;){` |
|   396 | 11302 | `		int nEntry = 0;` |
|     - | 11303 | `		const char *zEntry;` |
|   396 | 11304 | `		SplDirRead(pVm,pThis,pH);` |
|   396 | 11305 | `		if( (iFlags & SDI_SKIPDOTS) == 0 ){` |
|   211 | 11306 | `			return;` |
|     - | 11307 | `		}` |
|   246 | 11308 | `		zEntry = SfiStr(pThis,SDI_E,&nEntry);` |
|   246 | 11309 | `		if( !SplDirIsDot(zEntry,nEntry) ){` |
|   122 | 11310 | `			return;` |
|     - | 11311 | `		}` |
|     2 | 11312 | `	}` |
|   138 | 11313 | `}` |
|     - | 11314 | `/*` |
|     - | 11315 | ` * php's spl_filesystem_dir_open: open the directory, remember it under the path` |
|     - | 11316 | ` * MINUS one trailing slash, and read the first entry. Answers 0 when the open` |
|     - | 11317 | ` * failed, having still written the path (php sets it either way, so a caught` |
|     - | 11318 | ` * constructor failure leaves the same shape behind).` |
|     - | 11319 | ` */` |
|   100 | 11320 | `static VmDirHandle * SplDirOpen(ph7_vm *pVm,ph7_class_instance *pThis,` |
|     - | 11321 | `	const char *zPath,int nPath)` |
|     2 | 11322 | `{` |
|     - | 11323 | `	const ph7_io_stream *pStream;` |
|     - | 11324 | `	const char *zDevice;` |
|     - | 11325 | `	VmDirHandle *pH;` |
|     - | 11326 | `	char zBuf[4096];` |
|   102 | 11327 | `	void *pHandle = 0;` |
|   102 | 11328 | `	int nKeep = nPath;` |
|   102 | 11329 | `	if( nPath < 1 \|\| nPath >= (int)sizeof(zBuf) ){` |
|   ! 0 | 11330 | `		return 0;` |
|     - | 11331 | `	}` |
|   102 | 11332 | `	SyMemcpy(zPath,zBuf,(sxu32)nPath);` |
|   102 | 11333 | `	zBuf[nPath] = 0;` |
|   102 | 11334 | `	zDevice = zBuf;` |
|   102 | 11335 | `	pStream = PH7_VmGetStreamDevice(pVm,&zDevice,nPath);` |
|   102 | 11336 | `	if( nKeep > 1 && SFI_IS_SLASH(zPath[nKeep-1]) ){` |
|     3 | 11337 | `		nKeep--;` |
|     1 | 11338 | `	}` |
|   102 | 11339 | `	PH7_NativeSetAttrStr(pVm,pThis,SFI_P,zPath,nKeep);` |
|   102 | 11340 | `	PH7_NativeSetAttrInt(pVm,pThis,SDI_I,0);` |
|   102 | 11341 | `	PH7_NativeSetAttrStr(pVm,pThis,SDI_E,"",0);` |
|   102 | 11342 | `	PH7_NativeSetAttrStr(pVm,pThis,SFI_N,"",0);` |
|   102 | 11343 | `	if( pStream == 0 \|\| pStream->xOpenDir == 0 ){` |
|   ! 0 | 11344 | `		return 0;` |
|     - | 11345 | `	}` |
|     - | 11346 | `	{` |
|     - | 11347 | `		/* The device takes the VM through this argument (see opendir). */` |
|     - | 11348 | `		ph7_value sDummy;` |
|     - | 11349 | `		int rc;` |
|   102 | 11350 | `		PH7_MemObjInit(pVm,&sDummy);` |
|   102 | 11351 | `		rc = pStream->xOpenDir(zDevice,&sDummy,&pHandle);` |
|   102 | 11352 | `		PH7_MemObjRelease(&sDummy);` |
|   102 | 11353 | `		if( rc != PH7_OK ){` |
|     3 | 11354 | `			return 0;` |
|     - | 11355 | `		}` |
|     - | 11356 | `	}` |
|   100 | 11357 | `	pH = (VmDirHandle *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmDirHandle));` |
|   100 | 11358 | `	if( pH == 0 ){` |
|   ! 0 | 11359 | `		if( pStream->xCloseDir ){` |
|   ! 0 | 11360 | `			pStream->xCloseDir(pHandle);` |
|   ! 0 | 11361 | `		}` |
|   ! 0 | 11362 | `		return 0;` |
|     - | 11363 | `	}` |
|   100 | 11364 | `	pH->pStream = pStream;` |
|   100 | 11365 | `	pH->pHandle = pHandle;` |
|   100 | 11366 | `	pH->pThis = pThis;` |
|     - | 11367 | `	/* SyHashInsert BORROWS the key bytes: key off the record's own field, which` |
|     - | 11368 | `	 * lives exactly as long as the entry does (rule 22). */` |
|   100 | 11369 | `	if( SyHashInsert(&pVm->hDirHandle,(const void *)&pH->pThis,sizeof(void *),pH) != SXRET_OK ){` |
|   ! 0 | 11370 | `		if( pStream->xCloseDir ){` |
|   ! 0 | 11371 | `			pStream->xCloseDir(pHandle);` |
|   ! 0 | 11372 | `		}` |
|   ! 0 | 11373 | `		SyMemBackendFree(&pVm->sAllocator,pH);` |
|   ! 0 | 11374 | `		return 0;` |
|     - | 11375 | `	}` |
|   100 | 11376 | `	return pH;` |
|    52 | 11377 | `}` |
|     - | 11378 | `/*` |
|     - | 11379 | ` * The open handle behind this instance, RE-OPENING it for a fresh clone.` |
|     - | 11380 | ` *` |
|     - | 11381 | ` * php's clone handler opens the directory again and reads forward to the` |
|     - | 11382 | ` * source's index, because a directory stream cannot be duplicated; PHL does the` |
|     - | 11383 | ` * same work on first use instead, which is what keeps the handle out of every` |
|     - | 11384 | ``  * php-visible surface — a property slot carrying it would make `$a == clone $a` `` |
|     - | 11385 | ` * false, and php says true.` |
|     - | 11386 | ` */` |
|   659 | 11387 | `static VmDirHandle * SplDirState(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     2 | 11388 | `{` |
|   661 | 11389 | `	VmDirHandle *pH = SplDirFind(pVm,pThis);` |
|     - | 11390 | `	sxi64 iIndex;` |
|   661 | 11391 | `	int nPath = 0;` |
|     - | 11392 | `	const char *zPath;` |
|     - | 11393 | `	SyBlob sPath;` |
|   661 | 11394 | `	if( pH ){` |
|   635 | 11395 | `		return pH;` |
|     - | 11396 | `	}` |
|    27 | 11397 | `	zPath = SfiStr(pThis,SFI_P,&nPath);` |
|    27 | 11398 | `	if( nPath < 1 ){` |
|    25 | 11399 | `		return 0;   /* never constructed: php's "Object not initialized" */` |
|     - | 11400 | `	}` |
|     - | 11401 | `	/* The path slot is about to be rewritten by the open, so copy it out first. */` |
|     3 | 11402 | `	SyBlobInit(&sPath,&pVm->sAllocator);` |
|     3 | 11403 | `	SyBlobAppend(&sPath,zPath,(sxu32)nPath);` |
|     3 | 11404 | `	iIndex = PH7_NativeAttrInt(pThis,SDI_I);` |
|     3 | 11405 | `	pH = SplDirOpen(pVm,pThis,(const char *)SyBlobData(&sPath),(int)SyBlobLength(&sPath));` |
|     3 | 11406 | `	SyBlobRelease(&sPath);` |
|     3 | 11407 | `	if( pH == 0 ){` |
|   ! 0 | 11408 | `		return 0;` |
|     - | 11409 | `	}` |
|     3 | 11410 | `	SplDirReadSkip(pVm,pThis,pH);` |
|     - | 11411 | `	{` |
|     3 | 11412 | `		sxi64 iAt = iIndex;` |
|     5 | 11413 | `		while( iAt-- > 0 ){` |
|     3 | 11414 | `			SplDirReadSkip(pVm,pThis,pH);` |
|     1 | 11415 | `		}` |
|     - | 11416 | `	}` |
|     - | 11417 | `	/* The open above reset the index; the clone stands where the source stood. */` |
|     3 | 11418 | `	PH7_NativeSetAttrInt(pVm,pThis,SDI_I,iIndex);` |
|     3 | 11419 | `	return pH;` |
|   335 | 11420 | `}` |
|     - | 11421 | `/*` |
|     - | 11422 | ` * php's spl_filesystem_object_get_file_name for a DIR: the path, a slash and the` |
|     - | 11423 | ` * current entry, cached until the next read drops it. Called through SfiName(),` |
|     - | 11424 | ` * so every SplFileInfo accessor sees the same lazy value php's do.` |
|     - | 11425 | ` */` |
|   167 | 11426 | `static const char * SplDirName(ph7_vm *pVm,ph7_class_instance *pThis,int *pnLen)` |
|     2 | 11427 | `{` |
|   169 | 11428 | `	int nName = 0,nPath = 0,nEntry = 0;` |
|   169 | 11429 | `	const char *zName = SfiStr(pThis,SFI_N,&nName);` |
|     - | 11430 | `	const char *zPath,*zEntry;` |
|     - | 11431 | `	SyBlob sName;` |
|   169 | 11432 | `	if( nName > 0 ){` |
|    64 | 11433 | `		*pnLen = nName;` |
|    64 | 11434 | `		return zName;` |
|     - | 11435 | `	}` |
|     - | 11436 | `	/* A glob handle's path is the current match's directory, not the slot --` |
|     - | 11437 | ``	 * the slot holds the whole `glob://pattern`, which is not a directory at`` |
|     - | 11438 | `	 * all. php's join then has a branch PHL never needed: when the path is` |
|     - | 11439 | `	 * EMPTY the name is the entry ALONE, which is what makes` |
|     - | 11440 | ``	 * `new GlobIterator('d')` answer `d` for getPathname() rather than `/d`. */`` |
|   107 | 11441 | `	zPath = SplDirGlobPath(pVm,pThis,&nPath);` |
|   107 | 11442 | `	if( zPath == 0 ){` |
|    77 | 11443 | `		zPath = SfiStr(pThis,SFI_P,&nPath);` |
|    77 | 11444 | `		if( nPath < 1 ){` |
|   ! 0 | 11445 | `			*pnLen = 0;` |
|   ! 0 | 11446 | `			return "";` |
|     - | 11447 | `		}` |
|    38 | 11448 | `	}` |
|   107 | 11449 | `	SyBlobInit(&sName,&pVm->sAllocator);` |
|   107 | 11450 | `	if( nPath > 0 ){` |
|   105 | 11451 | `		char cSlash = SplDirSlash(PH7_NativeAttrInt(pThis,SDI_F));` |
|   105 | 11452 | `		SyBlobAppend(&sName,zPath,(sxu32)nPath);` |
|   105 | 11453 | `		SyBlobAppend(&sName,(const void *)&cSlash,sizeof(char));` |
|    52 | 11454 | `	}` |
|   107 | 11455 | `	zEntry = SfiStr(pThis,SDI_E,&nEntry);` |
|   107 | 11456 | `	SyBlobAppend(&sName,zEntry,(sxu32)nEntry);` |
|   160 | 11457 | `	PH7_NativeSetAttrStr(pVm,pThis,SFI_N,` |
|   105 | 11458 | `		(const char *)SyBlobData(&sName),(int)SyBlobLength(&sName));` |
|   107 | 11459 | `	SyBlobRelease(&sName);` |
|   107 | 11460 | `	return SfiStr(pThis,SFI_N,pnLen);` |
|    86 | 11461 | `}` |
|     - | 11462 | `/* php's CHECK_DIRECTORY_ITERATOR_IS_INITIALIZED: every DirectoryIterator method` |
|     - | 11463 | ` * refuses an object whose parent constructor never ran. */` |
|   464 | 11464 | `static VmDirHandle * SplDirChecked(ph7_context *pCtx,sxi32 *pRc)` |
|     2 | 11465 | `{` |
|   466 | 11466 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 11467 | `	VmDirHandle *pH;` |
|     - | 11468 | ``	/* php's `check` object handlers run BEFORE the method does, so a`` |
|     - | 11469 | `	 * GlobIterator whose parent constructor never ran refuses with THAT` |
|     - | 11470 | `	 * sentence rather than this one -- and refuses methods this check would` |
|     - | 11471 | `	 * have let through. A class without those handlers passes straight by. */` |
|   466 | 11472 | `	if( !SfoChecked(pCtx,pRc) ){` |
|     7 | 11473 | `		return 0;` |
|     - | 11474 | `	}` |
|   460 | 11475 | `	pH = SplDirState(pCtx->pVm,pThis);` |
|   460 | 11476 | `	if( pH == 0 ){` |
|    15 | 11477 | `		*pRc = PH7_VmThrowException(pCtx,"Error","Object not initialized");` |
|     7 | 11478 | `	}` |
|   460 | 11479 | `	return pH;` |
|   237 | 11480 | `}` |
|     - | 11481 | `/*` |
|     - | 11482 | ` * The shared constructor: php's spl_filesystem_object_construct, whose two` |
|     - | 11483 | ` * refusals are a ValueError for an empty path and an UnexpectedValueException` |
|     - | 11484 | ` * carrying the OPEN's own errno text (php promotes the opendir warning, so the` |
|     - | 11485 | ` * message is the warning's, prefixed with the constructor that raised it).` |
|     - | 11486 | ` */` |
|   106 | 11487 | `static int SplDirConstructVal(ph7_context *pCtx,const char *zClass,const char *zArg,` |
|     - | 11488 | `	ph7_value *pPath,sxi64 iFlags)` |
|     2 | 11489 | `{` |
|   108 | 11490 | `	ph7_vm *pVm = pCtx->pVm;` |
|   108 | 11491 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 11492 | `	const char *zPath;` |
|   108 | 11493 | `	int nPath = 0;` |
|   108 | 11494 | `	if( pThis == 0 ){` |
|   ! 0 | 11495 | `		return PH7_OK;` |
|     - | 11496 | `	}` |
|   108 | 11497 | `	zPath = ph7_value_to_string(pPath,&nPath);` |
|   108 | 11498 | `	if( nPath < 1 ){` |
|    10 | 11499 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     3 | 11500 | `			"%s::__construct(): Argument #1 ($%s) must not be empty",zClass,zArg);` |
|     - | 11501 | `	}` |
|   102 | 11502 | `	if( SplDirFind(pVm,pThis) ){` |
|     3 | 11503 | `		return PH7_VmThrowException(pCtx,"Error","Directory object is already initialized");` |
|     - | 11504 | `	}` |
|   100 | 11505 | `	PH7_NativeSetAttrInt(pVm,pThis,SDI_F,iFlags);` |
|   100 | 11506 | `	if( SplDirOpen(pVm,pThis,zPath,nPath) == 0 ){` |
|     4 | 11507 | `		return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     1 | 11508 | `			"%s::__construct(%.*s): Failed to open directory: %s",zClass,nPath,zPath,` |
|     2 | 11509 | `			VfsStrerror(errno));` |
|     - | 11510 | `	}` |
|    98 | 11511 | `	SplDirReadSkip(pVm,pThis,SplDirFind(pVm,pThis));` |
|    98 | 11512 | `	return PH7_OK;` |
|    55 | 11513 | `}` |
|     - | 11514 | `/* The three directory classes take their path straight from the argument; only` |
|     - | 11515 | ` * GlobIterator rewrites it first, which is why the open takes a VALUE. */` |
|    78 | 11516 | `static int SplDirConstruct(ph7_context *pCtx,const char *zClass,int nArg,ph7_value **apArg,` |
|     - | 11517 | `	sxi64 iFlags)` |
|     2 | 11518 | `{` |
|    80 | 11519 | `	if( nArg < 1 ){` |
|   ! 0 | 11520 | `		return PH7_OK;` |
|     - | 11521 | `	}` |
|    80 | 11522 | `	return SplDirConstructVal(pCtx,zClass,"directory",apArg[0],iFlags);` |
|    41 | 11523 | `}` |
|     - | 11524 | `/* DirectoryIterator::__construct(string $directory) — php's flags for this one` |
|     - | 11525 | ` * are KEY_AS_PATHNAME\|CURRENT_AS_SELF, and it takes no flags argument. */` |
|    38 | 11526 | `static int vm_builtin_DirectoryIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 11527 | `{` |
|    39 | 11528 | `	return SplDirConstruct(pCtx,"DirectoryIterator",nArg,apArg,` |
|     - | 11529 | `		SDI_KEY_AS_PATHNAME\|SDI_CURRENT_AS_SELF);` |
|     1 | 11530 | `}` |
|     - | 11531 | `/* The flags argument the two subclasses share: php's ZPP overwrites the whole` |
|     - | 11532 | ` * default when one is given, so SKIP_DOTS is NOT implied by passing flags. */` |
|    68 | 11533 | `static sxi64 SplDirFlagArg(int nArg,ph7_value **apArg,sxi64 iDefault)` |
|     2 | 11534 | `{` |
|    70 | 11535 | `	return nArg > 1 ? ph7_value_to_int64(apArg[1]) : iDefault;` |
|     2 | 11536 | `}` |
|    22 | 11537 | `static int vm_builtin_FilesystemIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 11538 | `{` |
|    34 | 11539 | `	return SplDirConstruct(pCtx,"FilesystemIterator",nArg,apArg,` |
|    11 | 11540 | `		SplDirFlagArg(nArg,apArg,SDI_KEY_AS_PATHNAME\|SDI_CURRENT_AS_FILEINFO\|SDI_SKIPDOTS));` |
|     1 | 11541 | `}` |
|    18 | 11542 | `static int vm_builtin_RecursiveDirectoryIterator_construct(ph7_context *pCtx,int nArg,` |
|     - | 11543 | `	ph7_value **apArg)` |
|     2 | 11544 | `{` |
|    29 | 11545 | `	return SplDirConstruct(pCtx,"RecursiveDirectoryIterator",nArg,apArg,` |
|     9 | 11546 | `		SplDirFlagArg(nArg,apArg,SDI_KEY_AS_PATHNAME\|SDI_CURRENT_AS_FILEINFO));` |
|     2 | 11547 | `}` |
|     - | 11548 | `/* DirectoryIterator::rewind(): php re-opens nothing — it rewinds the STREAM and` |
|     - | 11549 | ` * takes one entry, with no dot skipping at this level. */` |
|    12 | 11550 | `static int vm_builtin_DirectoryIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 11551 | `{` |
|     - | 11552 | `	sxi32 rc;` |
|    13 | 11553 | `	VmDirHandle *pH = SplDirChecked(pCtx,&rc);` |
|     6 | 11554 | `	SXUNUSED(nArg);` |
|     6 | 11555 | `	SXUNUSED(apArg);` |
|    13 | 11556 | `	if( pH == 0 ){` |
|     3 | 11557 | `		return rc;` |
|     - | 11558 | `	}` |
|    11 | 11559 | `	PH7_NativeSetAttrInt(pCtx->pVm,PH7_ContextThis(pCtx),SDI_I,0);` |
|    11 | 11560 | `	if( pH->pStream->xRewindDir ){` |
|    11 | 11561 | `		pH->pStream->xRewindDir(pH->pHandle);` |
|     5 | 11562 | `	}` |
|    11 | 11563 | `	SplDirRead(pCtx->pVm,PH7_ContextThis(pCtx),pH);` |
|    11 | 11564 | `	return PH7_OK;` |
|     7 | 11565 | `}` |
|     - | 11566 | `/* FilesystemIterator::rewind(): the same, plus the dot skipping, and php does` |
|     - | 11567 | ` * NOT check the handle here (an uninitialized object simply rewinds to nothing). */` |
|    44 | 11568 | `static int vm_builtin_FilesystemIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 11569 | `{` |
|    46 | 11570 | `	ph7_vm *pVm = pCtx->pVm;` |
|    46 | 11571 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 11572 | `	VmDirHandle *pH;` |
|     - | 11573 | `	sxi32 rcChk;` |
|    22 | 11574 | `	SXUNUSED(nArg);` |
|    22 | 11575 | `	SXUNUSED(apArg);` |
|    46 | 11576 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|     3 | 11577 | `		return rcChk;` |
|     - | 11578 | `	}` |
|    44 | 11579 | `	pH = SplDirState(pVm,pThis);` |
|    44 | 11580 | `	PH7_NativeSetAttrInt(pVm,pThis,SDI_I,0);` |
|    44 | 11581 | `	if( pH && pH->pStream->xRewindDir ){` |
|    44 | 11582 | `		pH->pStream->xRewindDir(pH->pHandle);` |
|    21 | 11583 | `	}` |
|    44 | 11584 | `	SplDirReadSkip(pVm,pThis,pH);` |
|    44 | 11585 | `	return PH7_OK;` |
|    24 | 11586 | `}` |
|   130 | 11587 | `static int vm_builtin_DirectoryIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 11588 | `{` |
|   132 | 11589 | `	ph7_vm *pVm = pCtx->pVm;` |
|   132 | 11590 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 11591 | `	sxi32 rc;` |
|   132 | 11592 | `	VmDirHandle *pH = SplDirChecked(pCtx,&rc);` |
|    66 | 11593 | `	SXUNUSED(nArg);` |
|    66 | 11594 | `	SXUNUSED(apArg);` |
|   132 | 11595 | `	if( pH == 0 ){` |
|     3 | 11596 | `		return rc;` |
|     - | 11597 | `	}` |
|     - | 11598 | `	/* php advances the index PAST the end too, which is why key() keeps counting` |
|     - | 11599 | `	 * once valid() is false. */` |
|   130 | 11600 | `	PH7_NativeSetAttrInt(pVm,pThis,SDI_I,PH7_NativeAttrInt(pThis,SDI_I) + 1);` |
|   130 | 11601 | `	SplDirReadSkip(pVm,pThis,pH);` |
|   130 | 11602 | `	return PH7_OK;` |
|    68 | 11603 | `}` |
|   198 | 11604 | `static int vm_builtin_DirectoryIterator_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 11605 | `{` |
|     - | 11606 | `	sxi32 rc;` |
|   200 | 11607 | `	VmDirHandle *pH = SplDirChecked(pCtx,&rc);` |
|   100 | 11608 | `	SXUNUSED(nArg);` |
|   100 | 11609 | `	SXUNUSED(apArg);` |
|   200 | 11610 | `	if( pH == 0 ){` |
|     5 | 11611 | `		return rc;` |
|     - | 11612 | `	}` |
|   196 | 11613 | `	ph7_result_bool(pCtx,!SplDirAtEnd(PH7_ContextThis(pCtx)));` |
|   196 | 11614 | `	return PH7_OK;` |
|   102 | 11615 | `}` |
|    26 | 11616 | `static int vm_builtin_DirectoryIterator_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 11617 | `{` |
|     - | 11618 | `	sxi32 rc;` |
|    27 | 11619 | `	VmDirHandle *pH = SplDirChecked(pCtx,&rc);` |
|    13 | 11620 | `	SXUNUSED(nArg);` |
|    13 | 11621 | `	SXUNUSED(apArg);` |
|    27 | 11622 | `	if( pH == 0 ){` |
|     3 | 11623 | `		return rc;` |
|     - | 11624 | `	}` |
|    25 | 11625 | `	ph7_result_int64(pCtx,PH7_NativeAttrInt(PH7_ContextThis(pCtx),SDI_I));` |
|    25 | 11626 | `	return PH7_OK;` |
|    14 | 11627 | `}` |
|    21 | 11628 | `static int vm_builtin_DirectoryIterator_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 11629 | `{` |
|     - | 11630 | `	sxi32 rc;` |
|    22 | 11631 | `	VmDirHandle *pH = SplDirChecked(pCtx,&rc);` |
|    11 | 11632 | `	SXUNUSED(nArg);` |
|    11 | 11633 | `	SXUNUSED(apArg);` |
|    22 | 11634 | `	if( pH == 0 ){` |
|     3 | 11635 | `		return rc;` |
|     - | 11636 | `	}` |
|    20 | 11637 | `	SplResultBorrowed(pCtx,PH7_ContextThis(pCtx));` |
|    20 | 11638 | `	return PH7_OK;` |
|    12 | 11639 | `}` |
|    14 | 11640 | `static int vm_builtin_DirectoryIterator_isDot(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 11641 | `{` |
|    15 | 11642 | `	int nEntry = 0;` |
|     - | 11643 | `	const char *zEntry;` |
|     - | 11644 | `	sxi32 rc;` |
|    15 | 11645 | `	VmDirHandle *pH = SplDirChecked(pCtx,&rc);` |
|     7 | 11646 | `	SXUNUSED(nArg);` |
|     7 | 11647 | `	SXUNUSED(apArg);` |
|    15 | 11648 | `	if( pH == 0 ){` |
|     5 | 11649 | `		return rc;` |
|     - | 11650 | `	}` |
|    11 | 11651 | `	zEntry = SfiStr(PH7_ContextThis(pCtx),SDI_E,&nEntry);` |
|    11 | 11652 | `	ph7_result_bool(pCtx,SplDirIsDot(zEntry,nEntry));` |
|    11 | 11653 | `	return PH7_OK;` |
|     8 | 11654 | `}` |
|     - | 11655 | `/*` |
|     - | 11656 | ` * php's seek(): rewind if the target is behind us, then walk forward through` |
|     - | 11657 | ` * the OBJECT's own valid()/next() — a subclass overriding either is obeyed, and` |
|     - | 11658 | ` * running out raises php's OutOfBoundsException with the iterator left standing` |
|     - | 11659 | ` * where the walk stopped.` |
|     - | 11660 | ` */` |
|    10 | 11661 | `static int vm_builtin_DirectoryIterator_seek(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 11662 | `{` |
|    11 | 11663 | `	ph7_vm *pVm = pCtx->pVm;` |
|    11 | 11664 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 11665 | `	ph7_class_method *pMethod;` |
|     - | 11666 | `	sxi64 iPos;` |
|     - | 11667 | `	sxi32 rc;` |
|    11 | 11668 | `	VmDirHandle *pH = SplDirChecked(pCtx,&rc);` |
|    11 | 11669 | `	if( pH == 0 ){` |
|   ! 0 | 11670 | `		return rc;` |
|     - | 11671 | `	}` |
|    11 | 11672 | `	iPos = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;` |
|    11 | 11673 | `	if( PH7_NativeAttrInt(pThis,SDI_I) > iPos ){` |
|     5 | 11674 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);` |
|     5 | 11675 | `		if( pMethod ){` |
|     5 | 11676 | `			rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,0,0,0);` |
|     5 | 11677 | `			if( rc != SXRET_OK ){` |
|   ! 0 | 11678 | `				return rc;` |
|     - | 11679 | `			}` |
|     2 | 11680 | `		}` |
|     2 | 11681 | `	}` |
|    27 | 11682 | `	while( PH7_NativeAttrInt(pThis,SDI_I) < iPos ){` |
|     - | 11683 | `		ph7_value sRet;` |
|     - | 11684 | `		int bValid;` |
|    19 | 11685 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);` |
|    19 | 11686 | `		if( pMethod == 0 ){` |
|   ! 0 | 11687 | `			break;` |
|     - | 11688 | `		}` |
|    19 | 11689 | `		PH7_MemObjInit(pVm,&sRet);` |
|    19 | 11690 | `		rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sRet,0,0);` |
|    19 | 11691 | `		bValid = rc == SXRET_OK && ph7_value_to_bool(&sRet);` |
|    19 | 11692 | `		PH7_MemObjRelease(&sRet);` |
|    19 | 11693 | `		if( rc != SXRET_OK ){` |
|   ! 0 | 11694 | `			return rc;` |
|     - | 11695 | `		}` |
|    19 | 11696 | `		if( !bValid ){` |
|     4 | 11697 | `			return PH7_VmThrowException(pCtx,"OutOfBoundsException",` |
|     1 | 11698 | `				"Seek position %qd is out of range",iPos);` |
|     - | 11699 | `		}` |
|    17 | 11700 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);` |
|    17 | 11701 | `		if( pMethod == 0 ){` |
|   ! 0 | 11702 | `			break;` |
|     - | 11703 | `		}` |
|    17 | 11704 | `		rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,0,0,0);` |
|    17 | 11705 | `		if( rc != SXRET_OK ){` |
|   ! 0 | 11706 | `			return rc;` |
|     - | 11707 | `		}` |
|     1 | 11708 | `	}` |
|     9 | 11709 | `	return PH7_OK;` |
|     6 | 11710 | `}` |
|     - | 11711 | `/* DirectoryIterator's three name accessors read the ENTRY, not the pathname —` |
|     - | 11712 | `` * which is why `getFilename()` answers `..` where SplFileInfo's would answer the`` |
|     - | 11713 | `` * whole path, and why `__toString()` is aliased to this one rather than to`` |
|     - | 11714 | ` * getPathname(). */` |
|    49 | 11715 | `static int vm_builtin_DirectoryIterator_getFilename(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 11716 | `{` |
|    50 | 11717 | `	int nEntry = 0;` |
|     - | 11718 | `	const char *zEntry;` |
|     - | 11719 | `	sxi32 rc;` |
|    50 | 11720 | `	VmDirHandle *pH = SplDirChecked(pCtx,&rc);` |
|    25 | 11721 | `	SXUNUSED(nArg);` |
|    25 | 11722 | `	SXUNUSED(apArg);` |
|    50 | 11723 | `	if( pH == 0 ){` |
|     5 | 11724 | `		return rc;` |
|     - | 11725 | `	}` |
|    46 | 11726 | `	zEntry = SfiStr(PH7_ContextThis(pCtx),SDI_E,&nEntry);` |
|    46 | 11727 | `	ph7_result_string(pCtx,zEntry,nEntry);` |
|    46 | 11728 | `	return PH7_OK;` |
|    26 | 11729 | `}` |
|     2 | 11730 | `static int vm_builtin_DirectoryIterator_getBasename(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 11731 | `{` |
|     3 | 11732 | `	int nEntry = 0,nBase = 0;` |
|     - | 11733 | `	const char *zEntry,*zBase;` |
|     - | 11734 | `	sxi32 rc;` |
|     3 | 11735 | `	VmDirHandle *pH = SplDirChecked(pCtx,&rc);` |
|     3 | 11736 | `	if( pH == 0 ){` |
|   ! 0 | 11737 | `		return rc;` |
|     - | 11738 | `	}` |
|     3 | 11739 | `	zEntry = SfiStr(PH7_ContextThis(pCtx),SDI_E,&nEntry);` |
|     3 | 11740 | `	zBase = PH7_ExtractBaseName(zEntry,nEntry,&nBase);` |
|     3 | 11741 | `	if( nArg > 0 ){` |
|     3 | 11742 | `		int nSuffix = 0;` |
|     3 | 11743 | `		const char *zSuffix = ph7_value_to_string(apArg[0],&nSuffix);` |
|     2 | 11744 | `		if( nSuffix > 0 && nSuffix < nBase` |
|     3 | 11745 | `		 && SyMemcmp(&zBase[nBase - nSuffix],zSuffix,(sxu32)nSuffix) == 0 ){` |
|     3 | 11746 | `			nBase -= nSuffix;` |
|     1 | 11747 | `		}` |
|     1 | 11748 | `	}` |
|     3 | 11749 | `	ph7_result_string(pCtx,zBase,nBase);` |
|     3 | 11750 | `	return PH7_OK;` |
|     2 | 11751 | `}` |
|     2 | 11752 | `static int vm_builtin_DirectoryIterator_getExtension(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 11753 | `{` |
|     3 | 11754 | `	int nEntry = 0,nBase = 0,i;` |
|     - | 11755 | `	const char *zEntry,*zBase;` |
|     - | 11756 | `	sxi32 rc;` |
|     3 | 11757 | `	VmDirHandle *pH = SplDirChecked(pCtx,&rc);` |
|     1 | 11758 | `	SXUNUSED(nArg);` |
|     1 | 11759 | `	SXUNUSED(apArg);` |
|     3 | 11760 | `	if( pH == 0 ){` |
|   ! 0 | 11761 | `		return rc;` |
|     - | 11762 | `	}` |
|     3 | 11763 | `	zEntry = SfiStr(PH7_ContextThis(pCtx),SDI_E,&nEntry);` |
|     3 | 11764 | `	zBase = PH7_ExtractBaseName(zEntry,nEntry,&nBase);` |
|     9 | 11765 | `	for( i = nBase - 1 ; i >= 0 ; --i ){` |
|     9 | 11766 | `		if( zBase[i] == '.' ){` |
|     3 | 11767 | `			ph7_result_string(pCtx,&zBase[i+1],nBase - i - 1);` |
|     3 | 11768 | `			return PH7_OK;` |
|     - | 11769 | `		}` |
|     4 | 11770 | `	}` |
|   ! 0 | 11771 | `	ph7_result_string(pCtx,"",0);` |
|   ! 0 | 11772 | `	return PH7_OK;` |
|     2 | 11773 | `}` |
|     - | 11774 | `/*` |
|     - | 11775 | ` * FilesystemIterator::key()/current(): php compares the flag against its MASK` |
|     - | 11776 | `` * (`(flags & MODE_MASK) == mode`) rather than testing a bit, so a stray bit in`` |
|     - | 11777 | ``  * another field cannot change either answer — which the chunk's `& KEY_AS_FILENAME` `` |
|     - | 11778 | `` * and `=== CURRENT_AS_PATHNAME` both got wrong in one direction or the other.`` |
|     - | 11779 | ` */` |
|    50 | 11780 | `static int vm_builtin_FilesystemIterator_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 11781 | `{` |
|    51 | 11782 | `	ph7_vm *pVm = pCtx->pVm;` |
|    51 | 11783 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    51 | 11784 | `	sxi64 iFlags = PH7_NativeAttrInt(pThis,SDI_F);` |
|    51 | 11785 | `	int nOut = 0;` |
|     - | 11786 | `	const char *zOut;` |
|     - | 11787 | `	sxi32 rcChk;` |
|    25 | 11788 | `	SXUNUSED(nArg);` |
|    25 | 11789 | `	SXUNUSED(apArg);` |
|    51 | 11790 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|     3 | 11791 | `		return rcChk;` |
|     - | 11792 | `	}` |
|    49 | 11793 | `	if( (iFlags & SDI_KEY_MODE_MASK) == SDI_KEY_AS_FILENAME ){` |
|     7 | 11794 | `		zOut = SfiStr(pThis,SDI_E,&nOut);` |
|     7 | 11795 | `		ph7_result_string(pCtx,zOut,nOut);` |
|     7 | 11796 | `		return PH7_OK;` |
|     - | 11797 | `	}` |
|    43 | 11798 | `	if( SplDirState(pVm,pThis) == 0 ){` |
|   ! 0 | 11799 | `		return PH7_VmThrowException(pCtx,"Error","Object not initialized");` |
|     - | 11800 | `	}` |
|    43 | 11801 | `	zOut = SfiName(pVm,pThis,&nOut);` |
|    43 | 11802 | `	ph7_result_string(pCtx,zOut,nOut);` |
|    43 | 11803 | `	return PH7_OK;` |
|    26 | 11804 | `}` |
|    84 | 11805 | `static int vm_builtin_FilesystemIterator_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 11806 | `{` |
|    86 | 11807 | `	ph7_vm *pVm = pCtx->pVm;` |
|    86 | 11808 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    86 | 11809 | `	sxi64 iMode = PH7_NativeAttrInt(pThis,SDI_F) & SDI_CURRENT_MODE_MASK;` |
|     - | 11810 | `	sxi32 rcChk;` |
|    42 | 11811 | `	SXUNUSED(nArg);` |
|    42 | 11812 | `	SXUNUSED(apArg);` |
|    86 | 11813 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|     3 | 11814 | `		return rcChk;` |
|     - | 11815 | `	}` |
|    84 | 11816 | `	if( iMode == SDI_CURRENT_AS_PATHNAME \|\| iMode == SDI_CURRENT_AS_FILEINFO ){` |
|    70 | 11817 | `		if( SplDirState(pVm,pThis) == 0 ){` |
|   ! 0 | 11818 | `			return PH7_VmThrowException(pCtx,"Error","Object not initialized");` |
|     - | 11819 | `		}` |
|    34 | 11820 | `	}` |
|    84 | 11821 | `	if( iMode == SDI_CURRENT_AS_PATHNAME ){` |
|     7 | 11822 | `		int nName = 0;` |
|     7 | 11823 | `		const char *zName = SfiName(pVm,pThis,&nName);` |
|     7 | 11824 | `		ph7_result_string(pCtx,zName,nName);` |
|     7 | 11825 | `		return PH7_OK;` |
|     - | 11826 | `	}` |
|    78 | 11827 | `	if( iMode == SDI_CURRENT_AS_FILEINFO ){` |
|    64 | 11828 | `		ph7_class *pClass = 0;` |
|    64 | 11829 | `		int nName = 0,nDir = 0;` |
|     - | 11830 | `		const char *zName,*zDir;` |
|     - | 11831 | `		sxi32 rc;` |
|    64 | 11832 | `		if( SplDirAtEnd(pThis) ){` |
|     - | 11833 | `			/* php's create_type again: there is no entry to describe. */` |
|   ! 0 | 11834 | `			return PH7_VmThrowException(pCtx,"RuntimeException","Could not open file");` |
|     - | 11835 | `		}` |
|    64 | 11836 | `		rc = SfiInfoClass(pCtx,"current",0,0,&pClass);` |
|    64 | 11837 | `		if( rc != PH7_OK ){` |
|   ! 0 | 11838 | `			return rc;` |
|     - | 11839 | `		}` |
|     - | 11840 | `		/* The child's PATH is the directory the walk is in, which for a glob` |
|     - | 11841 | `		 * handle is the current match's own -- the slot holds the whole` |
|     - | 11842 | ``		 * `glob://pattern`, and handing THAT over made every SplFileInfo the`` |
|     - | 11843 | `		 * iterator produced answer the pattern for getPath() and the joined` |
|     - | 11844 | `		 * name for getFilename(). */` |
|    64 | 11845 | `		zDir = SplDirGlobPath(pVm,pThis,&nDir);` |
|    64 | 11846 | `		if( zDir == 0 ){` |
|    42 | 11847 | `			zDir = SfiStr(pThis,SFI_P,&nDir);` |
|    20 | 11848 | `		}` |
|    64 | 11849 | `		zName = SfiName(pVm,pThis,&nName);` |
|    64 | 11850 | `		return SfiMakeInfoEx(pCtx,pClass,zName,nName,zDir,nDir);` |
|     - | 11851 | `	}` |
|    15 | 11852 | `	SplResultBorrowed(pCtx,pThis);` |
|    15 | 11853 | `	return PH7_OK;` |
|    44 | 11854 | `}` |
|     - | 11855 | `/* php's getFlags()/setFlags() answer and accept only the three mode fields;` |
|     - | 11856 | ` * everything else in the word is engine state the class keeps to itself. */` |
|    12 | 11857 | `static int vm_builtin_FilesystemIterator_getFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 11858 | `{` |
|     - | 11859 | `	sxi32 rcChk;` |
|     6 | 11860 | `	SXUNUSED(nArg);` |
|     6 | 11861 | `	SXUNUSED(apArg);` |
|    13 | 11862 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|     3 | 11863 | `		return rcChk;` |
|     - | 11864 | `	}` |
|    11 | 11865 | `	ph7_result_int64(pCtx,PH7_NativeAttrInt(PH7_ContextThis(pCtx),SDI_F) & SDI_FLAGS_MASK);` |
|    11 | 11866 | `	return PH7_OK;` |
|     7 | 11867 | `}` |
|     2 | 11868 | `static int vm_builtin_FilesystemIterator_setFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 11869 | `{` |
|     3 | 11870 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 11871 | `	sxi32 rcChk;` |
|     3 | 11872 | `	sxi64 iFlags = PH7_NativeAttrInt(pThis,SDI_F);` |
|     3 | 11873 | `	sxi64 iNew = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;` |
|     3 | 11874 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 11875 | `		return rcChk;` |
|     - | 11876 | `	}` |
|     4 | 11877 | `	PH7_NativeSetAttrInt(pCtx->pVm,pThis,SDI_F,(iFlags & ~(sxi64)SDI_FLAGS_MASK)` |
|     2 | 11878 | `		\| (iNew & (sxi64)SDI_FLAGS_MASK));` |
|     3 | 11879 | `	return PH7_OK;` |
|     2 | 11880 | `}` |
|     - | 11881 | `/*` |
|     - | 11882 | ` * RecursiveDirectoryIterator::hasChildren(bool $allowLinks = false).` |
|     - | 11883 | ` *` |
|     - | 11884 | ` * php lstats the entry and then asks two separate questions of it: a plain` |
|     - | 11885 | ` * directory has children, and a SYMLINK has them only when the walk was told to` |
|     - | 11886 | ` * follow links. Asked of the VFS rather than of a mode word, because the mode is` |
|     - | 11887 | ` * not filled on Windows (the same lesson getType() learned).` |
|     - | 11888 | ` */` |
|    18 | 11889 | `static int vm_builtin_RecursiveDirectoryIterator_hasChildren(ph7_context *pCtx,int nArg,` |
|     - | 11890 | `	ph7_value **apArg)` |
|     2 | 11891 | `{` |
|    20 | 11892 | `	ph7_vm *pVm = pCtx->pVm;` |
|    20 | 11893 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    20 | 11894 | `	const ph7_vfs *pVfs = pVm->pEngine->pVfs;` |
|    20 | 11895 | `	int nEntry = 0;` |
|    20 | 11896 | `	const char *zEntry = SfiStr(pThis,SDI_E,&nEntry);` |
|     - | 11897 | `	char zPath[4096];` |
|    20 | 11898 | `	int bAllow = nArg > 0 ? ph7_value_to_bool(apArg[0]) : 0;` |
|    20 | 11899 | `	sxi64 iFlags = PH7_NativeAttrInt(pThis,SDI_F);` |
|    18 | 11900 | `	if( nEntry < 1 \|\| SplDirIsDot(zEntry,nEntry) \|\| pVfs == 0` |
|    18 | 11901 | `	 \|\| SfiPathBuf(pVm,pThis,zPath,(int)sizeof(zPath)) != SXRET_OK ){` |
|     3 | 11902 | `		ph7_result_bool(pCtx,0);` |
|     3 | 11903 | `		return PH7_OK;` |
|     - | 11904 | `	}` |
|     - | 11905 | `	{` |
|     - | 11906 | `		/* A path a userland wrapper owns is ITS answer, asked exactly as the two` |
|     - | 11907 | `		 * VFS questions below are: the link question of an lstat record, the` |
|     - | 11908 | `		 * directory question of a stat one. Without this the walk treated every` |
|     - | 11909 | `		 * directory a wrapper reported as a LEAF, so a recursive iteration over` |
|     - | 11910 | `		 * one never descended. */` |
|     - | 11911 | `		ph7_int64 aVal[13];` |
|    18 | 11912 | `		int rcU = PH7_VfsUserStatFields(pCtx,zPath,PH7_STAT_ASK_IS_LINK,aVal);` |
|    18 | 11913 | `		if( PH7_CALLBACK_UNWOUND(rcU) ){` |
|   ! 0 | 11914 | `			return rcU;` |
|     - | 11915 | `		}` |
|    18 | 11916 | `		if( rcU != PHL_URLSTAT_NOWRAP ){` |
|   ! 0 | 11917 | `			if( rcU == PHL_URLSTAT_OK && (aVal[2] & PH7_S_IFMT) == PH7_S_IFLNK` |
|   ! 0 | 11918 | `			 && !bAllow && (iFlags & SDI_FOLLOW_SYMLINKS) == 0 ){` |
|   ! 0 | 11919 | `				ph7_result_bool(pCtx,0);` |
|   ! 0 | 11920 | `				return PH7_OK;` |
|     - | 11921 | `			}` |
|   ! 0 | 11922 | `			rcU = PH7_VfsUserStatFields(pCtx,zPath,PH7_STAT_ASK_IS_DIR,aVal);` |
|   ! 0 | 11923 | `			if( PH7_CALLBACK_UNWOUND(rcU) ){` |
|   ! 0 | 11924 | `				return rcU;` |
|     - | 11925 | `			}` |
|   ! 0 | 11926 | `			ph7_result_bool(pCtx,rcU == PHL_URLSTAT_OK` |
|   ! 0 | 11927 | `				&& (aVal[2] & PH7_S_IFMT) == PH7_S_IFDIR);` |
|   ! 0 | 11928 | `			return PH7_OK;` |
|     - | 11929 | `		}` |
|     - | 11930 | `	}` |
|    16 | 11931 | `	if( pVfs->xIslink && pVfs->xIslink(zPath) == PH7_OK` |
|    10 | 11932 | `	 && !bAllow && (iFlags & SDI_FOLLOW_SYMLINKS) == 0 ){` |
|   ! 0 | 11933 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 11934 | `		return PH7_OK;` |
|     - | 11935 | `	}` |
|    18 | 11936 | `	ph7_result_bool(pCtx,pVfs->xIsdir && pVfs->xIsdir(zPath) == PH7_OK);` |
|    18 | 11937 | `	return PH7_OK;` |
|    11 | 11938 | `}` |
|     - | 11939 | `/*` |
|     - | 11940 | ` * getChildren(): php builds an instance of the RUNTIME class through its` |
|     - | 11941 | ` * constructor with (pathname, flags), then hands it the sub path — which is what` |
|     - | 11942 | ` * makes getSubPathname() name the whole nested route rather than just the entry` |
|     - | 11943 | `` * (the chunk answered `''` and the filename, wrong at every depth below one).`` |
|     - | 11944 | ` */` |
|     8 | 11945 | `static int vm_builtin_RecursiveDirectoryIterator_getChildren(ph7_context *pCtx,int nArg,` |
|     - | 11946 | `	ph7_value **apArg)` |
|     2 | 11947 | `{` |
|    10 | 11948 | `	ph7_vm *pVm = pCtx->pVm;` |
|    10 | 11949 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 11950 | `	ph7_class_instance *pNew;` |
|     - | 11951 | `	ph7_class_method *pCons;` |
|     - | 11952 | `	ph7_value sPath,sFlags,*apCall[2];` |
|    10 | 11953 | `	int nName = 0,nSub = 0,nEntry = 0;` |
|     - | 11954 | `	const char *zName;` |
|    10 | 11955 | `	sxi64 iFlags = PH7_NativeAttrInt(pThis,SDI_F);` |
|     - | 11956 | `	sxi32 rc;` |
|     - | 11957 | `	SyBlob sSub;` |
|     4 | 11958 | `	SXUNUSED(nArg);` |
|     4 | 11959 | `	SXUNUSED(apArg);` |
|    10 | 11960 | `	if( SplDirState(pVm,pThis) == 0 ){` |
|   ! 0 | 11961 | `		return PH7_VmThrowException(pCtx,"Error","Object not initialized");` |
|     - | 11962 | `	}` |
|    10 | 11963 | `	pNew = PH7_NewClassInstance(pVm,pThis->pClass);` |
|    10 | 11964 | `	if( pNew == 0 ){` |
|   ! 0 | 11965 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 11966 | `	}` |
|    10 | 11967 | `	pNew->iRef++;` |
|    10 | 11968 | `	zName = SfiName(pVm,pThis,&nName);` |
|    10 | 11969 | `	PH7_MemObjInitFromString(pVm,&sPath,0);` |
|    10 | 11970 | `	PH7_MemObjStringAppend(&sPath,zName,(sxu32)nName);` |
|    10 | 11971 | `	PH7_MemObjInitFromInt(pVm,&sFlags,iFlags);` |
|    10 | 11972 | `	apCall[0] = &sPath;` |
|    10 | 11973 | `	apCall[1] = &sFlags;` |
|    10 | 11974 | `	pCons = PH7_ClassExtractMethod(pThis->pClass,"__construct",sizeof("__construct")-1);` |
|    10 | 11975 | `	rc = pCons ? PH7_VmCallClassMethod(pVm,pNew,pCons,0,2,apCall) : SXRET_OK;` |
|    10 | 11976 | `	PH7_MemObjRelease(&sPath);` |
|    10 | 11977 | `	PH7_MemObjRelease(&sFlags);` |
|    10 | 11978 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 11979 | `		PH7_ClassInstanceUnref(pNew);` |
|   ! 0 | 11980 | `		return rc;` |
|     - | 11981 | `	}` |
|     - | 11982 | `	/* php's sub_path: the parent's, this entry appended. */` |
|    10 | 11983 | `	SyBlobInit(&sSub,&pVm->sAllocator);` |
|     - | 11984 | `	{` |
|    10 | 11985 | `		const char *zSub = SfiStr(pThis,SDI_S,&nSub);` |
|    10 | 11986 | `		SyBlobAppend(&sSub,zSub,(sxu32)nSub);` |
|     - | 11987 | `	}` |
|    10 | 11988 | `	if( nSub > 0 ){` |
|     5 | 11989 | `		char cSlash = SplDirSlash(iFlags);` |
|     5 | 11990 | `		SyBlobAppend(&sSub,(const void *)&cSlash,sizeof(char));` |
|     2 | 11991 | `	}` |
|     - | 11992 | `	{` |
|    10 | 11993 | `		const char *zEntry = SfiStr(pThis,SDI_E,&nEntry);` |
|    10 | 11994 | `		SyBlobAppend(&sSub,zEntry,(sxu32)nEntry);` |
|     - | 11995 | `	}` |
|    14 | 11996 | `	PH7_NativeSetAttrStr(pVm,pNew,SDI_S,` |
|     8 | 11997 | `		(const char *)SyBlobData(&sSub),(int)SyBlobLength(&sSub));` |
|    10 | 11998 | `	SyBlobRelease(&sSub);` |
|     - | 11999 | `	{` |
|    10 | 12000 | `		int nInfo = 0;` |
|    10 | 12001 | `		const char *zInfo = SfiStr(pThis,SFI_IC,&nInfo);` |
|    10 | 12002 | `		PH7_NativeSetAttrStr(pVm,pNew,SFI_IC,zInfo,nInfo);` |
|     - | 12003 | `	}` |
|    10 | 12004 | `	PH7_NativeResultObject(pCtx,pNew);` |
|    10 | 12005 | `	PH7_ClassInstanceUnref(pNew);` |
|    10 | 12006 | `	return PH7_OK;` |
|     6 | 12007 | `}` |
|     4 | 12008 | `static int vm_builtin_RecursiveDirectoryIterator_getSubPath(ph7_context *pCtx,int nArg,` |
|     - | 12009 | `	ph7_value **apArg)` |
|     1 | 12010 | `{` |
|     5 | 12011 | `	int nSub = 0;` |
|     5 | 12012 | `	const char *zSub = SfiStr(PH7_ContextThis(pCtx),SDI_S,&nSub);` |
|     2 | 12013 | `	SXUNUSED(nArg);` |
|     2 | 12014 | `	SXUNUSED(apArg);` |
|     5 | 12015 | `	ph7_result_string(pCtx,zSub,nSub);` |
|     5 | 12016 | `	return PH7_OK;` |
|     1 | 12017 | `}` |
|     4 | 12018 | `static int vm_builtin_RecursiveDirectoryIterator_getSubPathname(ph7_context *pCtx,int nArg,` |
|     - | 12019 | `	ph7_value **apArg)` |
|     1 | 12020 | `{` |
|     5 | 12021 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     5 | 12022 | `	int nSub = 0,nEntry = 0;` |
|     5 | 12023 | `	const char *zSub = SfiStr(pThis,SDI_S,&nSub);` |
|     - | 12024 | `	SyBlob sOut;` |
|     2 | 12025 | `	SXUNUSED(nArg);` |
|     2 | 12026 | `	SXUNUSED(apArg);` |
|     5 | 12027 | `	if( nSub < 1 ){` |
|     3 | 12028 | `		const char *zEntry = SfiStr(pThis,SDI_E,&nEntry);` |
|     3 | 12029 | `		ph7_result_string(pCtx,zEntry,nEntry);` |
|     3 | 12030 | `		return PH7_OK;` |
|     - | 12031 | `	}` |
|     3 | 12032 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|     3 | 12033 | `	SyBlobAppend(&sOut,zSub,(sxu32)nSub);` |
|     - | 12034 | `	{` |
|     3 | 12035 | `		char cSlash = SplDirSlash(PH7_NativeAttrInt(pThis,SDI_F));` |
|     3 | 12036 | `		SyBlobAppend(&sOut,(const void *)&cSlash,sizeof(char));` |
|     - | 12037 | `	}` |
|     - | 12038 | `	{` |
|     3 | 12039 | `		const char *zEntry = SfiStr(pThis,SDI_E,&nEntry);` |
|     3 | 12040 | `		SyBlobAppend(&sOut,zEntry,(sxu32)nEntry);` |
|     - | 12041 | `	}` |
|     3 | 12042 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|     3 | 12043 | `	SyBlobRelease(&sOut);` |
|     3 | 12044 | `	return PH7_OK;` |
|     3 | 12045 | `}` |
|     - | 12046 | `/*` |
|     - | 12047 | ` * GlobIterator::__construct(string $pattern, int $flags = 0)` |
|     - | 12048 | ` *` |
|     - | 12049 | `` * php's DIT_CTOR_GLOB: the `glob://` prefix goes on when the caller did not`` |
|     - | 12050 | `` * write one, and the whole `glob://pattern` is what the path slot keeps -- so`` |
|     - | 12051 | `` * the `glob` debug key shows it, and getPath() has to ask the STREAM instead`` |
|     - | 12052 | ` * (SplDirGlobPath). The default flags are 0, which is` |
|     - | 12053 | ` * KEY_AS_PATHNAME\|CURRENT_AS_FILEINFO with SKIP_DOTS OFF where` |
|     - | 12054 | `` * FilesystemIterator has it on: `glob('d/.*')` yields `.` and `..` through the`` |
|     - | 12055 | ` * iterator exactly as it does through the function.` |
|     - | 12056 | ` */` |
|    30 | 12057 | `static int vm_builtin_GlobIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12058 | `{` |
|    31 | 12059 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 12060 | `	const char *zPat;` |
|    31 | 12061 | `	int nPat = 0;` |
|     - | 12062 | `	ph7_value sUri;` |
|     - | 12063 | `	sxi32 rc;` |
|    31 | 12064 | `	if( PH7_ContextThis(pCtx) == 0 \|\| nArg < 1 ){` |
|   ! 0 | 12065 | `		return PH7_OK;` |
|     - | 12066 | `	}` |
|    31 | 12067 | `	zPat = ph7_value_to_string(apArg[0],&nPat);` |
|    31 | 12068 | `	if( nPat < 1 ){` |
|     - | 12069 | `		/* php's empty check runs on the ARGUMENT, before the prefix goes on. */` |
|     3 | 12070 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 12071 | `			"GlobIterator::__construct(): Argument #1 ($pattern) must not be empty");` |
|     - | 12072 | `	}` |
|    29 | 12073 | `	PH7_MemObjInitFromString(pVm,&sUri,0);` |
|     - | 12074 | `	/* Both of php's prefix tests are CASE-SENSITIVE, where the wrapper LOOKUP` |
|     - | 12075 | ``	 * that follows is not: `GLOB://x` is prefixed again, so the pattern the`` |
|     - | 12076 | ``	 * device is finally handed still spells `GLOB://x` and matches nothing --`` |
|     - | 12077 | `	 * and the debug key shows the doubled string. */` |
|    28 | 12078 | `	if( nPat < (int)sizeof("glob://")-1` |
|    29 | 12079 | `	 \|\| SyMemcmp(zPat,"glob://",sizeof("glob://")-1) != 0 ){` |
|    27 | 12080 | `		PH7_MemObjStringAppend(&sUri,"glob://",sizeof("glob://")-1);` |
|    13 | 12081 | `	}` |
|    29 | 12082 | `	PH7_MemObjStringAppend(&sUri,zPat,(sxu32)nPat);` |
|    43 | 12083 | `	rc = SplDirConstructVal(pCtx,"GlobIterator","pattern",&sUri,` |
|    14 | 12084 | `		SplDirFlagArg(nArg,apArg,SDI_KEY_AS_PATHNAME\|SDI_CURRENT_AS_FILEINFO));` |
|    29 | 12085 | `	PH7_MemObjRelease(&sUri);` |
|    29 | 12086 | `	return rc;` |
|    16 | 12087 | `}` |
|     - | 12088 | `/*` |
|     - | 12089 | ` * GlobIterator::count(): php's php_glob_stream_get_count, which is the number` |
|     - | 12090 | ` * of MATCHES and does not move with the walk -- a pattern ending in a slash` |
|     - | 12091 | ` * counts its directories and yields none of them, because their entry is the` |
|     - | 12092 | ` * empty string a walk reads as the end.` |
|     - | 12093 | ` */` |
|    16 | 12094 | `static int vm_builtin_GlobIterator_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12095 | `{` |
|    17 | 12096 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 12097 | `	VmDirHandle *pH;` |
|     - | 12098 | `	sxi32 rcChk;` |
|     8 | 12099 | `	SXUNUSED(nArg);` |
|     8 | 12100 | `	SXUNUSED(apArg);` |
|    17 | 12101 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|     3 | 12102 | `		return rcChk;` |
|     - | 12103 | `	}` |
|    15 | 12104 | `	pH = SplDirFind(pCtx->pVm,pThis);` |
|    15 | 12105 | `	if( pH == 0 \|\| pH->pStream == 0 \|\| !PH7_GlobStreamIs(pH->pStream) ){` |
|     - | 12106 | `		/* php's own "should not happen", raised as a fatal there. */` |
|   ! 0 | 12107 | `		return PH7_VmThrowException(pCtx,"Error","GlobIterator lost glob state");` |
|     - | 12108 | `	}` |
|    15 | 12109 | `	ph7_result_int64(pCtx,PH7_GlobStreamCount(pH->pHandle));` |
|    15 | 12110 | `	return PH7_OK;` |
|     9 | 12111 | `}` |
|     - | 12112 | `/*` |
|     - | 12113 | ` * The four declarations. Method ORDER, signatures and tentative return types` |
|     - | 12114 | `` * are spl_directory.stub.php's; the four slots are php's `u.dir` arm and carry`` |
|     - | 12115 | ` * PH7_MOD_HIDDEN because php declares no property at all here. Each class` |
|     - | 12116 | ` * restates NOSERIALIZE and the presentation hook: a native subclass inherits` |
|     - | 12117 | ` * neither (rule 29).` |
|     - | 12118 | ` */` |
|  8445 | 12119 | `static sxi32 VmInstallSplDirIterators(ph7_vm *pVm)` |
|     5 | 12120 | `{` |
|     - | 12121 | `	static const PH7_NativePropDef aDirProp[] = {` |
|     - | 12122 | `		{ SDI_E, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },` |
|     - | 12123 | `		{ SDI_I, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - | 12124 | `		{ SDI_F, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - | 12125 | `		{ SDI_S, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },` |
|     - | 12126 | `	};` |
|     - | 12127 | `	static const PH7_NativeMethodDef aDirMethod[] = {` |
|     - | 12128 | `		{ "__construct",  PH7_MOD_PUBLIC, "string $directory", 0,` |
|     - | 12129 | `		  vm_builtin_DirectoryIterator_construct },` |
|     - | 12130 | `		{ "getFilename",  PH7_MOD_PUBLIC, "", "@string",` |
|     - | 12131 | `		  vm_builtin_DirectoryIterator_getFilename },` |
|     - | 12132 | `		{ "getExtension", PH7_MOD_PUBLIC, "", "@string",` |
|     - | 12133 | `		  vm_builtin_DirectoryIterator_getExtension },` |
|     - | 12134 | `		{ "getBasename",  PH7_MOD_PUBLIC, "string $suffix = \"\"", "@string",` |
|     - | 12135 | `		  vm_builtin_DirectoryIterator_getBasename },` |
|     - | 12136 | `		{ "isDot",        PH7_MOD_PUBLIC, "", "@bool", vm_builtin_DirectoryIterator_isDot },` |
|     - | 12137 | `		{ "rewind",       PH7_MOD_PUBLIC, "", "@void", vm_builtin_DirectoryIterator_rewind },` |
|     - | 12138 | `		{ "valid",        PH7_MOD_PUBLIC, "", "@bool", vm_builtin_DirectoryIterator_valid },` |
|     - | 12139 | `		{ "key",          PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_DirectoryIterator_key },` |
|     - | 12140 | `		{ "current",      PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_DirectoryIterator_current },` |
|     - | 12141 | `		{ "next",         PH7_MOD_PUBLIC, "", "@void", vm_builtin_DirectoryIterator_next },` |
|     - | 12142 | `		{ "seek",         PH7_MOD_PUBLIC, "int $offset", "@void",` |
|     - | 12143 | `		  vm_builtin_DirectoryIterator_seek },` |
|     - | 12144 | ``		/* php aliases this one to getFilename(), so `echo $it` prints the ENTRY where`` |
|     - | 12145 | `		 * SplFileInfo's __toString prints the whole pathname. Not tentative. */` |
|     - | 12146 | `		{ "__toString",   PH7_MOD_PUBLIC, "", "string",` |
|     - | 12147 | `		  vm_builtin_DirectoryIterator_getFilename },` |
|     - | 12148 | `	};` |
|     - | 12149 | `	static const PH7_NativeConstDef aFsConst[] = {` |
|     - | 12150 | `		{ "CURRENT_MODE_MASK",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_CURRENT_MODE_MASK, 0, 0.0 },` |
|     - | 12151 | `		{ "CURRENT_AS_PATHNAME", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_CURRENT_AS_PATHNAME, 0, 0.0 },` |
|     - | 12152 | `		{ "CURRENT_AS_FILEINFO", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_CURRENT_AS_FILEINFO, 0, 0.0 },` |
|     - | 12153 | `		{ "CURRENT_AS_SELF",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_CURRENT_AS_SELF, 0, 0.0 },` |
|     - | 12154 | `		{ "KEY_MODE_MASK",       PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_KEY_MODE_MASK, 0, 0.0 },` |
|     - | 12155 | `		{ "KEY_AS_PATHNAME",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_KEY_AS_PATHNAME, 0, 0.0 },` |
|     - | 12156 | `		{ "FOLLOW_SYMLINKS",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_FOLLOW_SYMLINKS, 0, 0.0 },` |
|     - | 12157 | `		{ "KEY_AS_FILENAME",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_KEY_AS_FILENAME, 0, 0.0 },` |
|     - | 12158 | `		{ "NEW_CURRENT_AND_KEY", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_KEY_AS_FILENAME\|SDI_CURRENT_AS_FILEINFO, 0, 0.0 },` |
|     - | 12159 | `		{ "OTHER_MODE_MASK",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_OTHERS_MASK, 0, 0.0 },` |
|     - | 12160 | `		{ "SKIP_DOTS",           PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_SKIPDOTS, 0, 0.0 },` |
|     - | 12161 | `		{ "UNIX_PATHS",          PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_UNIXPATHS, 0, 0.0 },` |
|     - | 12162 | `	};` |
|     - | 12163 | `	static const PH7_NativeMethodDef aFsMethod[] = {` |
|     - | 12164 | `		{ "__construct", PH7_MOD_PUBLIC,` |
|     - | 12165 | `		  "string $directory, int $flags = FilesystemIterator::KEY_AS_PATHNAME \| "` |
|     - | 12166 | `		  "FilesystemIterator::CURRENT_AS_FILEINFO \| FilesystemIterator::SKIP_DOTS", 0,` |
|     - | 12167 | `		  vm_builtin_FilesystemIterator_construct },` |
|     - | 12168 | `		{ "rewind",      PH7_MOD_PUBLIC, "", "@void", vm_builtin_FilesystemIterator_rewind },` |
|     - | 12169 | `		{ "key",         PH7_MOD_PUBLIC, "", "@string", vm_builtin_FilesystemIterator_key },` |
|     - | 12170 | `		{ "current",     PH7_MOD_PUBLIC, "", "@SplFileInfo\|FilesystemIterator\|string",` |
|     - | 12171 | `		  vm_builtin_FilesystemIterator_current },` |
|     - | 12172 | `		{ "getFlags",    PH7_MOD_PUBLIC, "", "@int", vm_builtin_FilesystemIterator_getFlags },` |
|     - | 12173 | `		{ "setFlags",    PH7_MOD_PUBLIC, "int $flags", "@void",` |
|     - | 12174 | `		  vm_builtin_FilesystemIterator_setFlags },` |
|     - | 12175 | `	};` |
|     - | 12176 | `	static const PH7_NativeMethodDef aRdiMethod[] = {` |
|     - | 12177 | `		{ "__construct",    PH7_MOD_PUBLIC,` |
|     - | 12178 | `		  "string $directory, int $flags = FilesystemIterator::KEY_AS_PATHNAME \| "` |
|     - | 12179 | `		  "FilesystemIterator::CURRENT_AS_FILEINFO", 0,` |
|     - | 12180 | `		  vm_builtin_RecursiveDirectoryIterator_construct },` |
|     - | 12181 | `		{ "hasChildren",    PH7_MOD_PUBLIC, "bool $allowLinks = false", "@bool",` |
|     - | 12182 | `		  vm_builtin_RecursiveDirectoryIterator_hasChildren },` |
|     - | 12183 | `		{ "getChildren",    PH7_MOD_PUBLIC, "", "@RecursiveDirectoryIterator",` |
|     - | 12184 | `		  vm_builtin_RecursiveDirectoryIterator_getChildren },` |
|     - | 12185 | `		{ "getSubPath",     PH7_MOD_PUBLIC, "", "@string",` |
|     - | 12186 | `		  vm_builtin_RecursiveDirectoryIterator_getSubPath },` |
|     - | 12187 | `		{ "getSubPathname", PH7_MOD_PUBLIC, "", "@string",` |
|     - | 12188 | `		  vm_builtin_RecursiveDirectoryIterator_getSubPathname },` |
|     - | 12189 | `	};` |
|     - | 12190 | `	static const PH7_NativeMethodDef aGlobMethod[] = {` |
|     - | 12191 | `		{ "__construct", PH7_MOD_PUBLIC,` |
|     - | 12192 | `		  "string $pattern, int $flags = FilesystemIterator::KEY_AS_PATHNAME \| "` |
|     - | 12193 | `		  "FilesystemIterator::CURRENT_AS_FILEINFO", 0,` |
|     - | 12194 | `		  vm_builtin_GlobIterator_construct },` |
|     - | 12195 | `		{ "count",       PH7_MOD_PUBLIC, "", "@int", vm_builtin_GlobIterator_count },` |
|     - | 12196 | `	};` |
|     - | 12197 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - | 12198 | `		{ "DirectoryIterator", "SplFileInfo", "SeekableIterator", PH7_CLASS_NOSERIALIZE,` |
|     - | 12199 | `		  aDirMethod, SX_ARRAYSIZE(aDirMethod), 0, 0,` |
|     - | 12200 | `		  aDirProp, SX_ARRAYSIZE(aDirProp), SplDirClose, 0, SfiPresent },` |
|     - | 12201 | `		{ "FilesystemIterator", "DirectoryIterator", 0, PH7_CLASS_NOSERIALIZE,` |
|     - | 12202 | `		  aFsMethod, SX_ARRAYSIZE(aFsMethod), aFsConst, SX_ARRAYSIZE(aFsConst),` |
|     - | 12203 | `		  0, 0, SplDirClose, 0, SfiPresent },` |
|     - | 12204 | `		{ "RecursiveDirectoryIterator", "FilesystemIterator", "RecursiveIterator",` |
|     - | 12205 | `		  PH7_CLASS_NOSERIALIZE,` |
|     - | 12206 | `		  aRdiMethod, SX_ARRAYSIZE(aRdiMethod), 0, 0,` |
|     - | 12207 | `		  0, 0, SplDirClose, 0, SfiPresent },` |
|     - | 12208 | ``		/* php gives this one the `check` object handlers SplFileObject has, so it`` |
|     - | 12209 | `		 * is UNCLONEABLE and refuses every method -- inherited ones included --` |
|     - | 12210 | `		 * on an instance whose parent constructor never ran (SfoIsChecked). A` |
|     - | 12211 | `		 * native class inherits neither the refusals nor the hooks (rule 29), so` |
|     - | 12212 | `		 * both are restated here. */` |
|     - | 12213 | `		{ "GlobIterator", "FilesystemIterator", "Countable",` |
|     - | 12214 | `		  PH7_CLASS_NOSERIALIZE\|PH7_CLASS_NOCLONE,` |
|     - | 12215 | `		  aGlobMethod, SX_ARRAYSIZE(aGlobMethod), 0, 0,` |
|     - | 12216 | `		  0, 0, SplDirClose, 0, SfiPresent },` |
|     - | 12217 | `	};` |
|  8450 | 12218 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|     5 | 12219 | `}` |
|     - | 12220 | `/*` |
|     - | 12221 | ` * ---------------------------------------------------------------------------` |
|     - | 12222 | ` * SplFileObject.` |
|     - | 12223 | ` *` |
|     - | 12224 | `` * php's SPL_FS_FILE arm of the same `spl_filesystem_object`: an OPEN stream plus`` |
|     - | 12225 | ` * ONE line of look-ahead. The line is the whole model, and nearly every rule` |
|     - | 12226 | ` * below is about which of the two current values is live rather than about IO.` |
|     - | 12227 | `` * php keeps `current_line` (a string) and `current_zval` (the READ_CSV array)`` |
|     - | 12228 | `` * side by side, either or both present, and `current()` picks between them; the`` |
|     - | 12229 | ``  * line NUMBER is bumped by whatever read produced them, which is why `key()` `` |
|     - | 12230 | ``  * counts differently for `fgetc()` (only a `\n` advances it), for `fgets()` `` |
|     - | 12231 | `` * (always) and for `current()` (only when a line was already there).`` |
|     - | 12232 | ` *` |
|     - | 12233 | ` * The stream lives in a hidden slot as a RESOURCE rather than on a registry the` |
|     - | 12234 | ` * way a DirectoryIterator's DIR does, because php makes this class UNCLONEABLE` |
|     - | 12235 | `` * (its `check` object handlers null the clone handler out) -- so no second`` |
|     - | 12236 | ` * object can ever reach the same handle, and the class's xRelease is the only` |
|     - | 12237 | ` * teardown there is. That is also php's flush point: a file written through an` |
|     - | 12238 | ` * SplFileObject is finished when the OBJECT dies.` |
|     - | 12239 | ` *` |
|     - | 12240 | `` * Those same `check` handlers give the class a get_method that refuses EVERY`` |
|     - | 12241 | ` * method -- the ones inherited from SplFileInfo included -- on an instance whose` |
|     - | 12242 | ` * parent constructor never ran (a subclass that forgets it, a caught` |
|     - | 12243 | ` * constructor failure). SfiReady() below is where that check lives, so the` |
|     - | 12244 | ` * inherited accessors carry it too.` |
|     - | 12245 | ` * ---------------------------------------------------------------------------` |
|     - | 12246 | ` */` |
|     - | 12247 | `/* php's spl_directory.h SPL_FILE_OBJECT_* flags, and the mask getFlags() cuts` |
|     - | 12248 | ` * the stored word with. */` |
|     - | 12249 | `#define SFO_DROP_NEW_LINE 0x0001` |
|     - | 12250 | `#define SFO_READ_AHEAD    0x0002` |
|     - | 12251 | `#define SFO_SKIP_EMPTY    0x0004` |
|     - | 12252 | `#define SFO_READ_CSV      0x0008` |
|     - | 12253 | `#define SFO_FLAGS_MASK    0x000F` |
|     - | 12254 | `/* Which of php's two current values this instance is holding. Both may be live` |
|     - | 12255 | ` * at once: a CSV read keeps the raw line beside the parsed array. */` |
|     - | 12256 | `#define SFO_HAS_LINE 0x1` |
|     - | 12257 | `#define SFO_HAS_ZVAL 0x2` |
|     - | 12258 | ``/* The state slots are named beside SplFileInfo's, up with the `u.dir` arm: they`` |
|     - | 12259 | ` * are all hidden (php declares no property on this class either) and three of` |
|     - | 12260 | ` * them are what the shared presentation hook shows.` |
|     - | 12261 | ` *` |
|     - | 12262 | ` * What a read attempt did. php's zend_result plus the third case a THROW is:` |
|     - | 12263 | ` * the read routines run user code (a subclass's getCurrentLine) and raise` |
|     - | 12264 | ` * php's own RuntimeException, so a caller has to be able to abandon. */` |
|     - | 12265 | `#define SFO_READ_OK    0` |
|     - | 12266 | `#define SFO_READ_FAIL  1` |
|     - | 12267 | `#define SFO_READ_THROW 2` |
|     - | 12268 |  |
|     - | 12269 | `/* This class's own getCurrentLine body, named ahead of the read routine that` |
|     - | 12270 | ` * has to tell it apart from a subclass's override. */` |
|     - | 12271 | `static int vm_builtin_SplFileObject_fgets(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|     - | 12272 | ``/* php's `!intern->u.file.stream`: the uninitialized object. */`` |
|  1550 | 12273 | `static io_private * SfoDev(ph7_class_instance *pThis)` |
|     3 | 12274 | `{` |
|  1553 | 12275 | `	ph7_value *pVal = pThis ? PH7_NativeAttr(pThis,SFO_H) : 0;` |
|     - | 12276 | `	io_private *pDev;` |
|  1553 | 12277 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_RES) == 0 ){` |
|   361 | 12278 | `		return 0;` |
|     - | 12279 | `	}` |
|  1195 | 12280 | `	pDev = (io_private *)pVal->x.pOther;` |
|  1195 | 12281 | `	return IO_PRIVATE_INVALID(pDev) ? 0 : pDev;` |
|   778 | 12282 | `}` |
|     - | 12283 | ``/* Is this instance one of the classes php gives the `check` handlers to? Asked`` |
|     - | 12284 | ` * as a CLASS question for the same reason SplDirIs() is: a user class may` |
|     - | 12285 | ` * declare anything it likes. php gives them to TWO: SplFileObject and` |
|     - | 12286 | ` * GlobIterator, which is why an unconstructed GlobIterator refuses every method` |
|     - | 12287 | ` * with this sentence where an unconstructed FilesystemIterator answers` |
|     - | 12288 | `` * `Object not initialized` -- and why neither of the two can be cloned. */`` |
|  1525 | 12289 | `static int SfoIsChecked(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     2 | 12290 | `{` |
|     - | 12291 | `	ph7_class *pFile;` |
|  1527 | 12292 | `	if( pThis == 0 ){` |
|   ! 0 | 12293 | `		return 0;` |
|     - | 12294 | `	}` |
|  1527 | 12295 | `	pFile = PH7_VmExtractClass(pVm,"SplFileObject",sizeof("SplFileObject")-1,FALSE,0);` |
|  1527 | 12296 | `	if( pFile && PH7_VmInstanceOf(pThis->pClass,pFile) ){` |
|   348 | 12297 | `		return 1;` |
|     - | 12298 | `	}` |
|  1181 | 12299 | `	pFile = PH7_VmExtractClass(pVm,"GlobIterator",sizeof("GlobIterator")-1,FALSE,0);` |
|  1181 | 12300 | `	return pFile && PH7_VmInstanceOf(pThis->pClass,pFile) ? 1 : 0;` |
|   768 | 12301 | `}` |
|     - | 12302 | `/*` |
|     - | 12303 | ` * php's spl_filesystem_object_get_method_check, as a guard the bodies call:` |
|     - | 12304 | `` * an instance of a `check` class with nothing open answers NO method at all.`` |
|     - | 12305 | ` * Answers 0 when the caller must return *pRc.` |
|     - | 12306 | ` */` |
|     - | 12307 | `/*` |
|     - | 12308 | `` * php's `u.file.stream == NULL && orig_path == NULL`. The two classes that`` |
|     - | 12309 | ` * carry the check handlers fill different halves of it. SplFileObject has an` |
|     - | 12310 | ` * open BYTE stream -- and NO orig_path when its open FAILED, which is why a` |
|     - | 12311 | ` * subclass that catches its own parent constructor's exception is left` |
|     - | 12312 | ` * refusing every method. GlobIterator has a path instead: the whole` |
|     - | 12313 | `` * `glob://pattern`, written by the open whether the pattern matched anything`` |
|     - | 12314 | ` * or not, so an iterator over nothing is still a working object.` |
|     - | 12315 | ` */` |
|   380 | 12316 | `static int SfoCheckReady(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     2 | 12317 | `{` |
|   382 | 12318 | `	int nPath = 0;` |
|   382 | 12319 | `	if( SfoDev(pThis) != 0 ){` |
|   216 | 12320 | `		return 1;` |
|     - | 12321 | `	}` |
|   167 | 12322 | `	if( !SplGlobIs(pVm,pThis) ){` |
|    11 | 12323 | `		return 0;` |
|     - | 12324 | `	}` |
|   157 | 12325 | `	SfiStr(pThis,SFI_P,&nPath);` |
|   157 | 12326 | `	return nPath > 0;` |
|   192 | 12327 | `}` |
|  1403 | 12328 | `static int SfoChecked(ph7_context *pCtx,sxi32 *pRc)` |
|     2 | 12329 | `{` |
|  1405 | 12330 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|  1405 | 12331 | `	*pRc = PH7_OK;` |
|  1405 | 12332 | `	if( SfoIsChecked(pCtx->pVm,pThis) && !SfoCheckReady(pCtx->pVm,pThis) ){` |
|    31 | 12333 | `		*pRc = PH7_VmThrowException(pCtx,"Error",` |
|     - | 12334 | `			"The parent constructor was not called: the object is in an invalid state");` |
|    31 | 12335 | `		return 0;` |
|     - | 12336 | `	}` |
|  1375 | 12337 | `	return 1;` |
|   707 | 12338 | `}` |
|   804 | 12339 | `static sxi64 SfoFlags(ph7_class_instance *pThis)` |
|     1 | 12340 | `{` |
|   805 | 12341 | `	return PH7_NativeAttrInt(pThis,SFO_FL);` |
|     1 | 12342 | `}` |
|     - | 12343 | `/* php's spl_filesystem_file_free_line: BOTH current values go. */` |
|   360 | 12344 | `static void SfoFreeLine(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 | 12345 | `{` |
|     - | 12346 | `	ph7_value *pZ;` |
|   361 | 12347 | `	PH7_NativeSetAttrStr(pVm,pThis,SFO_L,"",0);` |
|   361 | 12348 | `	pZ = PH7_NativeAttr(pThis,SFO_Z);` |
|   361 | 12349 | `	if( pZ ){` |
|   361 | 12350 | `		PH7_MemObjRelease(pZ);` |
|   180 | 12351 | `	}` |
|   361 | 12352 | `	PH7_NativeSetAttrInt(pVm,pThis,SFO_LS,0);` |
|   361 | 12353 | `}` |
|     - | 12354 | `/* The current line's bytes. Only meaningful while SFO_HAS_LINE is set. */` |
|   212 | 12355 | `static const char * SfoLine(ph7_class_instance *pThis,int *pnLen)` |
|     1 | 12356 | `{` |
|   213 | 12357 | `	return SfiStr(pThis,SFO_L,pnLen);` |
|     1 | 12358 | `}` |
|     - | 12359 | `/*` |
|     - | 12360 | ` * php's spl_filesystem_file_read_ex: drop what is held, refuse at EOF (loudly` |
|     - | 12361 | ` * unless silent), take ONE line, apply DROP_NEW_LINE unless this is the CSV` |
|     - | 12362 | `` * path, and add `iLineAdd` to the line number.`` |
|     - | 12363 | ` */` |
|   176 | 12364 | `static int SfoReadEx(ph7_context *pCtx,int bSilent,int iLineAdd,int bCsv,sxi32 *pRc)` |
|     1 | 12365 | `{` |
|   177 | 12366 | `	ph7_vm *pVm = pCtx->pVm;` |
|   177 | 12367 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   177 | 12368 | `	io_private *pDev = SfoDev(pThis);` |
|   177 | 12369 | `	sxi64 iMax = PH7_NativeAttrInt(pThis,SFO_ML);` |
|   177 | 12370 | `	const char *zLine = 0;` |
|     - | 12371 | `	ph7_int64 n;` |
|   177 | 12372 | `	*pRc = PH7_OK;` |
|   177 | 12373 | `	SfoFreeLine(pVm,pThis);` |
|   177 | 12374 | `	if( pDev == 0 \|\| PH7_StreamAtEof(pDev) ){` |
|    17 | 12375 | `		if( !bSilent ){` |
|     3 | 12376 | `			int nName = 0;` |
|     3 | 12377 | `			const char *zName = SfiName(pVm,pThis,&nName);` |
|     4 | 12378 | `			*pRc = PH7_VmThrowException(pCtx,"RuntimeException",` |
|     1 | 12379 | `				"Cannot read from file %.*s",nName,zName);` |
|     3 | 12380 | `			return SFO_READ_THROW;` |
|     - | 12381 | `		}` |
|    15 | 12382 | `		return SFO_READ_FAIL;` |
|     - | 12383 | `	}` |
|   161 | 12384 | `	n = StreamReadLine(pDev,&zLine,iMax > 0 ? (ph7_int64)iMax : 0);` |
|   161 | 12385 | `	if( n < 1 ){` |
|     - | 12386 | `		/* The device's own notice, worded from THIS method: a read on a handle` |
|     - | 12387 | `		 * opened write-only says so here as it does from fgets(). */` |
|    27 | 12388 | `		StreamReportReadFailure(pCtx,pDev);` |
|     - | 12389 | `		/* php's buf == NULL: the line is the EMPTY string, not an absence. */` |
|    27 | 12390 | `		PH7_NativeSetAttrStr(pVm,pThis,SFO_L,"",0);` |
|    14 | 12391 | `	}else{` |
|   135 | 12392 | `		if( !bCsv && (SfoFlags(pThis) & SFO_DROP_NEW_LINE) ){` |
|    25 | 12393 | `			if( zLine[n-1] == '\n' ){` |
|    25 | 12394 | `				n--;` |
|    25 | 12395 | `				if( n > 0 && zLine[n-1] == '\r' ){` |
|   ! 0 | 12396 | `					n--;` |
|   ! 0 | 12397 | `				}` |
|    12 | 12398 | `			}` |
|    12 | 12399 | `		}` |
|   135 | 12400 | `		PH7_NativeSetAttrStr(pVm,pThis,SFO_L,zLine,(int)n);` |
|     - | 12401 | `	}` |
|   161 | 12402 | `	PH7_NativeSetAttrInt(pVm,pThis,SFO_LS,SFO_HAS_LINE);` |
|   161 | 12403 | `	PH7_NativeSetAttrInt(pVm,pThis,SFO_K,PH7_NativeAttrInt(pThis,SFO_K) + iLineAdd);` |
|   161 | 12404 | `	return SFO_READ_OK;` |
|    89 | 12405 | `}` |
|     - | 12406 | `/* php's spl_filesystem_file_read: the line number advances only when a line was` |
|     - | 12407 | ` * ALREADY there, which is what makes the first current() answer key 0. */` |
|   136 | 12408 | `static int SfoReadOne(ph7_context *pCtx,int bSilent,int bCsv,sxi32 *pRc)` |
|     1 | 12409 | `{` |
|   137 | 12410 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   137 | 12411 | `	int iAdd = (PH7_NativeAttrInt(pThis,SFO_LS) & SFO_HAS_LINE) ? 1 : 0;` |
|   137 | 12412 | `	return SfoReadEx(pCtx,bSilent,iAdd,bCsv,pRc);` |
|     1 | 12413 | `}` |
|     - | 12414 | `/* php's is_line_empty: an empty line, or -- under READ_CSV\|DROP_NEW_LINE, whose` |
|     - | 12415 | ` * combination does NOT strip the newline -- a line that is only one. */` |
|    56 | 12416 | `static int SfoLineEmpty(ph7_class_instance *pThis)` |
|     1 | 12417 | `{` |
|    57 | 12418 | `	int nLine = 0;` |
|    57 | 12419 | `	const char *zLine = SfoLine(pThis,&nLine);` |
|    57 | 12420 | `	sxi64 iFlags = SfoFlags(pThis);` |
|    57 | 12421 | `	if( nLine == 0 ){` |
|    13 | 12422 | `		return 1;` |
|     - | 12423 | `	}` |
|    45 | 12424 | `	if( (iFlags & SFO_READ_CSV) && (iFlags & SFO_DROP_NEW_LINE) ){` |
|    15 | 12425 | `		return (nLine == 1 && zLine[0] == '\n')` |
|    15 | 12426 | `			\|\| (nLine == 2 && zLine[0] == '\r' && zLine[1] == '\n');` |
|     - | 12427 | `	}` |
|    31 | 12428 | `	return 0;` |
|    29 | 12429 | `}` |
|     - | 12430 | `/*` |
|     - | 12431 | ` * php's spl_filesystem_file_read_csv: read lines until one is not empty (when` |
|     - | 12432 | ` * SKIP_EMPTY says so), then parse the RECORD -- which may run past the line,` |
|     - | 12433 | ` * because an open enclosure carries the newline inside the value.` |
|     - | 12434 | ` */` |
|    28 | 12435 | `static int SfoReadCsv(ph7_context *pCtx,int delim,int encl,int escape,ph7_value *pOut,` |
|     - | 12436 | `	int bSilent,sxi32 *pRc)` |
|     1 | 12437 | `{` |
|    29 | 12438 | `	ph7_vm *pVm = pCtx->pVm;` |
|    29 | 12439 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 12440 | `	io_private *pDev;` |
|     - | 12441 | `	ph7_value *pSlot;` |
|     - | 12442 | `	ph7_value sArray;` |
|     - | 12443 | `	SyBlob sRec;` |
|     - | 12444 | `	PH7_CsvScan sScan;` |
|    29 | 12445 | `	int nLine = 0,rc;` |
|     - | 12446 | `	const char *zLine;` |
|    14 | 12447 | `	do{` |
|    33 | 12448 | `		rc = SfoReadOne(pCtx,bSilent,TRUE,pRc);` |
|    33 | 12449 | `		if( rc != SFO_READ_OK ){` |
|     3 | 12450 | `			return rc;` |
|     - | 12451 | `		}` |
|    31 | 12452 | `	}while( SfoLineEmpty(pThis) && (SfoFlags(pThis) & SFO_SKIP_EMPTY) );` |
|    27 | 12453 | `	PH7_MemObjInit(pVm,&sArray);` |
|    27 | 12454 | `	if( PH7_MemObjToHashmap(&sArray) != SXRET_OK ){` |
|   ! 0 | 12455 | `		PH7_MemObjRelease(&sArray);` |
|   ! 0 | 12456 | `		*pRc = PH7_ContextMemoryError(pCtx);` |
|   ! 0 | 12457 | `		return SFO_READ_THROW;` |
|     - | 12458 | `	}` |
|    27 | 12459 | `	zLine = SfoLine(pThis,&nLine);` |
|    27 | 12460 | `	SyBlobInit(&sRec,&pVm->sAllocator);` |
|    27 | 12461 | `	SyBlobAppend(&sRec,(const void *)zLine,(sxu32)nLine);` |
|    27 | 12462 | `	pDev = SfoDev(pThis);` |
|    27 | 12463 | `	PH7_CsvScanInit(&sScan);` |
|    27 | 12464 | `	while( pDev && PH7_CsvScanOpen(&sScan,(const char *)SyBlobData(&sRec),` |
|    13 | 12465 | `			SyBlobLength(&sRec),delim,encl,escape) ){` |
|     - | 12466 | `		ph7_int64 n;` |
|   ! 0 | 12467 | `		if( SyBlobLength(&sRec) >= (sxu32)SXI32_HIGH ){` |
|   ! 0 | 12468 | `			break;   /* the parser measures in int; stop rather than wrap negative */` |
|     - | 12469 | `		}` |
|   ! 0 | 12470 | `		n = StreamReadLine(pDev,&zLine,0);` |
|   ! 0 | 12471 | `		if( n < 1 ){` |
|   ! 0 | 12472 | `			break;   /* EOF inside the enclosure: php answers what it has */` |
|     - | 12473 | `		}` |
|   ! 0 | 12474 | `		SyBlobAppend(&sRec,(const void *)zLine,(sxu32)n);` |
|   ! 0 | 12475 | `	}` |
|    40 | 12476 | `	PH7_ProcessCsv(&sArray,(const char *)SyBlobData(&sRec),(int)SyBlobLength(&sRec),` |
|    13 | 12477 | `		delim,encl,escape,0);` |
|    27 | 12478 | `	SyBlobRelease(&sRec);` |
|    27 | 12479 | `	pSlot = PH7_NativeAttr(pThis,SFO_Z);` |
|    27 | 12480 | `	if( pSlot ){` |
|    27 | 12481 | `		PH7_MemObjStore(&sArray,pSlot);` |
|    13 | 12482 | `	}` |
|    40 | 12483 | `	PH7_NativeSetAttrInt(pVm,pThis,SFO_LS,` |
|    26 | 12484 | `		(sxi64)(PH7_NativeAttrInt(pThis,SFO_LS) \| SFO_HAS_ZVAL));` |
|    27 | 12485 | `	if( pOut ){` |
|    11 | 12486 | `		ph7_value *pKeep = PH7_NativeAttr(pThis,SFO_Z);` |
|    11 | 12487 | `		if( pKeep ){` |
|    11 | 12488 | `			PH7_MemObjLoad(pKeep,pOut);` |
|     5 | 12489 | `		}` |
|     5 | 12490 | `	}` |
|    27 | 12491 | `	PH7_MemObjRelease(&sArray);` |
|    27 | 12492 | `	return SFO_READ_OK;` |
|    15 | 12493 | `}` |
|     - | 12494 | `/* This instance's three CSV settings. */` |
|    38 | 12495 | `static void SfoCsvControl(ph7_class_instance *pThis,int *pDelim,int *pEncl,int *pEsc)` |
|     1 | 12496 | `{` |
|    39 | 12497 | `	*pDelim = (int)PH7_NativeAttrInt(pThis,SFO_D);` |
|    39 | 12498 | `	*pEncl  = (int)PH7_NativeAttrInt(pThis,SFO_EN);` |
|    39 | 12499 | `	*pEsc   = (int)PH7_NativeAttrInt(pThis,SFO_ES);` |
|    39 | 12500 | `}` |
|     - | 12501 | `/*` |
|     - | 12502 | ` * php's spl_filesystem_file_read_line_ex, whose middle branch is the one only a` |
|     - | 12503 | ` * differential finds: a SUBCLASS that overrides getCurrentLine() drives the` |
|     - | 12504 | ` * iteration, so the line every accessor sees is whatever that method answered` |
|     - | 12505 | ` * -- and the line number is bumped once by the read the method made and again` |
|     - | 12506 | ` * here, which is why such a subclass counts in threes.` |
|     - | 12507 | ` */` |
|   136 | 12508 | `static int SfoReadLineEx(ph7_context *pCtx,int bSilent,sxi32 *pRc)` |
|     1 | 12509 | `{` |
|   137 | 12510 | `	ph7_vm *pVm = pCtx->pVm;` |
|   137 | 12511 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 12512 | `	ph7_class_method *pCur;` |
|     - | 12513 | `	int delim,encl,escape;` |
|   137 | 12514 | `	*pRc = PH7_OK;` |
|   137 | 12515 | `	if( SfoFlags(pThis) & SFO_READ_CSV ){` |
|    19 | 12516 | `		SfoCsvControl(pThis,&delim,&encl,&escape);` |
|    19 | 12517 | `		return SfoReadCsv(pCtx,delim,encl,escape,0,bSilent,pRc);` |
|     - | 12518 | `	}` |
|     - | 12519 | `	/* php compares the resolved method's declaring SCOPE with SplFileObject; a` |
|     - | 12520 | `	 * ph7_class_method carries no such pointer, and the C BODY answers the same` |
|     - | 12521 | `	 * question -- this class's getCurrentLine IS its fgets. */` |
|   119 | 12522 | `	pCur = PH7_ClassExtractMethod(pThis->pClass,"getCurrentLine",sizeof("getCurrentLine")-1);` |
|   119 | 12523 | `	if( pCur && (pCur->sFunc.pNative == 0` |
|   109 | 12524 | `	          \|\| pCur->sFunc.pNative->xFunc != vm_builtin_SplFileObject_fgets) ){` |
|     - | 12525 | `		io_private *pDev;` |
|     - | 12526 | `		ph7_value sRet;` |
|     - | 12527 | `		sxi32 rc;` |
|    19 | 12528 | `		SfoFreeLine(pVm,pThis);` |
|    19 | 12529 | `		pDev = SfoDev(pThis);` |
|    19 | 12530 | `		if( pDev == 0 \|\| PH7_StreamAtEof(pDev) ){` |
|   ! 0 | 12531 | `			if( !bSilent ){` |
|   ! 0 | 12532 | `				int nName = 0;` |
|   ! 0 | 12533 | `				const char *zName = SfiName(pVm,pThis,&nName);` |
|   ! 0 | 12534 | `				*pRc = PH7_VmThrowException(pCtx,"RuntimeException",` |
|   ! 0 | 12535 | `					"Cannot read from file %.*s",nName,zName);` |
|   ! 0 | 12536 | `				return SFO_READ_THROW;` |
|     - | 12537 | `			}` |
|   ! 0 | 12538 | `			return SFO_READ_FAIL;` |
|     - | 12539 | `		}` |
|    19 | 12540 | `		PH7_MemObjInit(pVm,&sRet);` |
|    19 | 12541 | `		rc = PH7_VmCallClassMethod(pVm,pThis,pCur,&sRet,0,0);` |
|    19 | 12542 | `		if( rc != SXRET_OK ){` |
|   ! 0 | 12543 | `			PH7_MemObjRelease(&sRet);` |
|   ! 0 | 12544 | `			*pRc = rc;` |
|   ! 0 | 12545 | `			return SFO_READ_THROW;` |
|     - | 12546 | `		}` |
|    19 | 12547 | `		if( (sRet.iFlags & MEMOBJ_STRING) == 0 ){` |
|     - | 12548 | ``			/* php's own TypeError: the declared `: string` return is checked by`` |
|     - | 12549 | `			 * the CALLER here, because the method may have no declared type. */` |
|   ! 0 | 12550 | `			const char *zGot = PH7_MemObjTypeDump(&sRet);` |
|   ! 0 | 12551 | `			*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 12552 | `				"%z::getCurrentLine(): Return value must be of type string, %s returned",` |
|   ! 0 | 12553 | `				&pThis->pClass->sDisp,zGot);` |
|   ! 0 | 12554 | `			PH7_MemObjRelease(&sRet);` |
|   ! 0 | 12555 | `			return SFO_READ_THROW;` |
|     - | 12556 | `		}` |
|    19 | 12557 | `		if( PH7_NativeAttrInt(pThis,SFO_LS) != 0 ){` |
|    11 | 12558 | `			PH7_NativeSetAttrInt(pVm,pThis,SFO_K,PH7_NativeAttrInt(pThis,SFO_K) + 1);` |
|     5 | 12559 | `		}` |
|    19 | 12560 | `		SfoFreeLine(pVm,pThis);` |
|     - | 12561 | `		{` |
|    19 | 12562 | `			int nRet = 0;` |
|    19 | 12563 | `			const char *zRet = ph7_value_to_string(&sRet,&nRet);` |
|    19 | 12564 | `			PH7_NativeSetAttrStr(pVm,pThis,SFO_L,zRet,nRet);` |
|     - | 12565 | `		}` |
|    19 | 12566 | `		PH7_NativeSetAttrInt(pVm,pThis,SFO_LS,SFO_HAS_LINE);` |
|    19 | 12567 | `		PH7_MemObjRelease(&sRet);` |
|    19 | 12568 | `		return SFO_READ_OK;` |
|     - | 12569 | `	}` |
|   101 | 12570 | `	return SfoReadOne(pCtx,bSilent,FALSE,pRc);` |
|    69 | 12571 | `}` |
|     - | 12572 | `/* php's spl_filesystem_file_read_line: the SKIP_EMPTY loop around it. */` |
|   130 | 12573 | `static int SfoReadLine(ph7_context *pCtx,int bSilent,sxi32 *pRc)` |
|     1 | 12574 | `{` |
|   131 | 12575 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   131 | 12576 | `	int rc = SfoReadLineEx(pCtx,bSilent,pRc);` |
|   137 | 12577 | `	while( (SfoFlags(pThis) & SFO_SKIP_EMPTY) && rc == SFO_READ_OK && SfoLineEmpty(pThis) ){` |
|     7 | 12578 | `		SfoFreeLine(pCtx->pVm,pThis);` |
|     7 | 12579 | `		rc = SfoReadLineEx(pCtx,bSilent,pRc);` |
|     1 | 12580 | `	}` |
|   131 | 12581 | `	return rc;` |
|     1 | 12582 | `}` |
|     - | 12583 | `/* php's spl_filesystem_file_rewind: seek to 0, drop the line, reset the count,` |
|     - | 12584 | ` * and read one ahead when READ_AHEAD asks. */` |
|    30 | 12585 | `static sxi32 SfoRewind(ph7_context *pCtx)` |
|     1 | 12586 | `{` |
|    31 | 12587 | `	ph7_vm *pVm = pCtx->pVm;` |
|    31 | 12588 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    31 | 12589 | `	io_private *pDev = SfoDev(pThis);` |
|    31 | 12590 | `	sxi32 rc = PH7_OK;` |
|    31 | 12591 | `	if( pDev == 0 ){` |
|   ! 0 | 12592 | `		return PH7_VmThrowException(pCtx,"Error","Object not initialized");` |
|     - | 12593 | `	}` |
|    31 | 12594 | `	if( PH7_StreamSeekWrapped(pDev,0,0 /* SEEK_SET */) != PH7_OK ){` |
|   ! 0 | 12595 | `		int nName = 0;` |
|   ! 0 | 12596 | `		const char *zName = SfiName(pVm,pThis,&nName);` |
|   ! 0 | 12597 | `		return PH7_VmThrowException(pCtx,"RuntimeException","Cannot rewind file %.*s",` |
|   ! 0 | 12598 | `			nName,zName);` |
|     - | 12599 | `	}` |
|    31 | 12600 | `	SfoFreeLine(pVm,pThis);` |
|    31 | 12601 | `	PH7_NativeSetAttrInt(pVm,pThis,SFO_K,0);` |
|    31 | 12602 | `	if( SfoFlags(pThis) & SFO_READ_AHEAD ){` |
|     5 | 12603 | `		SfoReadLine(pCtx,TRUE,&rc);` |
|     2 | 12604 | `	}` |
|    31 | 12605 | `	return rc;` |
|    16 | 12606 | `}` |
|     - | 12607 | `/*` |
|     - | 12608 | ` * The open both SplFileObject::__construct and SplFileInfo::openFile() are.` |
|     - | 12609 | ` * php promotes the warning its stream opener would print to a RuntimeException` |
|     - | 12610 | ` * (zend_replace_error_handling), so the text is the warning's -- worded from` |
|     - | 12611 | ` * this context's own qualified name, which is what the caller reports.` |
|     - | 12612 | ` */` |
|   156 | 12613 | `static sxi32 SfoOpen(ph7_context *pCtx,ph7_class_instance *pThis,ph7_value *pPath,` |
|     - | 12614 | `	const char *zMode,int nMode,int bUseInclude,ph7_value *pCtxArg,int iCtxArg)` |
|     3 | 12615 | `{` |
|   159 | 12616 | `	ph7_vm *pVm = pCtx->pVm;` |
|   159 | 12617 | `	const ph7_vfs *pVfs = pVm->pEngine->pVfs;` |
|     - | 12618 | `	phl_stream_ctx *pCtxRes;` |
|     - | 12619 | `	io_private *pDev;` |
|     - | 12620 | `	ph7_value *pSlot;` |
|     - | 12621 | `	const char *zErrUri,*zPath;` |
|     - | 12622 | `	ph7_value *apCtx[4];` |
|   159 | 12623 | `	int nPath = 0,iErr = 0,bThrew = 0,i;` |
|   159 | 12624 | `	zPath = ph7_value_to_string(pPath,&nPath);` |
|     - | 12625 | `	/* PH7_StreamCtxFromArg words its refusal from an argument VECTOR position,` |
|     - | 12626 | `	 * so the context is presented at the index php blames. */` |
|   783 | 12627 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apCtx) ; ++i ){` |
|   627 | 12628 | `		apCtx[i] = pPath;` |
|   315 | 12629 | `	}` |
|   159 | 12630 | `	if( iCtxArg >= 1 && iCtxArg <= (int)SX_ARRAYSIZE(apCtx) ){` |
|   133 | 12631 | `		apCtx[iCtxArg - 1] = pCtxArg;` |
|    65 | 12632 | `	}` |
|     - | 12633 | `	/* php's order, and each step is observable from the one before it: the NUL` |
|     - | 12634 | `	 * is ZPP's and comes first, then the already-open refusal, then the` |
|     - | 12635 | `	 * directory stat, and only then the EMPTY path -- which is the stream` |
|     - | 12636 | `	 * opener's ValueError rather than the constructor's. */` |
|   159 | 12637 | `	if( SyByteFind(zPath,(sxu32)nPath,'\0',0) == SXRET_OK ){` |
|     4 | 12638 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 12639 | `			"%s(): Argument #1 ($filename) must not contain any null bytes",` |
|     1 | 12640 | `			ph7_function_name(pCtx));` |
|     - | 12641 | `	}` |
|   157 | 12642 | `	if( SfoDev(pThis) != 0 ){` |
|     5 | 12643 | `		return PH7_VmThrowException(pCtx,"Error","Cannot call constructor twice");` |
|     - | 12644 | `	}` |
|   153 | 12645 | `	if( pVfs && pVfs->xIsdir && nPath > 0 && nPath < 4096 ){` |
|     - | 12646 | `		char zBuf[4096];` |
|   147 | 12647 | `		SyMemcpy(zPath,zBuf,(sxu32)nPath);` |
|   147 | 12648 | `		zBuf[nPath] = 0;` |
|   147 | 12649 | `		if( pVfs->xIsdir(zBuf) == PH7_OK ){` |
|     5 | 12650 | `			return PH7_VmThrowException(pCtx,"LogicException",` |
|     - | 12651 | `				"Cannot use SplFileObject with directories");` |
|     - | 12652 | `		}` |
|    70 | 12653 | `	}` |
|   149 | 12654 | `	if( nPath < 1 ){` |
|     8 | 12655 | `		return PH7_VmThrowException(pCtx,"ValueError","Path must not be empty");` |
|     - | 12656 | `	}` |
|   143 | 12657 | `	pCtxRes = pCtxArg` |
|     6 | 12658 | `		? PH7_StreamCtxFromArg(pCtx,iCtxArg,apCtx,iCtxArg - 1,"$context",0,&bThrew)` |
|   137 | 12659 | `		: PH7_StreamCtxDefault(pVm);` |
|   143 | 12660 | `	if( bThrew ){` |
|     5 | 12661 | `		return PH7_OK;` |
|     - | 12662 | `	}` |
|   139 | 12663 | `	pDev = PH7_StreamOpenPath(pCtx,pPath,zMode,nMode,bUseInclude,pCtxRes,pCtxArg,` |
|     - | 12664 | `		&iErr,&zErrUri);` |
|   139 | 12665 | `	if( pDev == 0 ){` |
|     7 | 12666 | `		if( iErr == PH7_STREAM_OPEN_NODEVICE ){` |
|   ! 0 | 12667 | `			int nScheme = 0;` |
|   ! 0 | 12668 | `			if( PH7_VmStreamDeviceIsRemoteHost(zPath,nPath,&nScheme) ){` |
|   ! 0 | 12669 | `				return PH7_VmThrowException(pCtx,"RuntimeException",` |
|     - | 12670 | `					"%s(%.*s): Failed to open stream: no suitable wrapper could be found",` |
|   ! 0 | 12671 | `					ph7_function_name(pCtx),nPath,zPath);` |
|     - | 12672 | `			}` |
|   ! 0 | 12673 | `			return PH7_VmThrowException(pCtx,"RuntimeException",` |
|     - | 12674 | `				"%s(): Unable to find the wrapper \"%.*s\" - did you forget to enable it "` |
|   ! 0 | 12675 | `				"when you configured PHP?",ph7_function_name(pCtx),nScheme,zPath);` |
|     - | 12676 | `		}` |
|     7 | 12677 | `		if( iErr == PH7_STREAM_OPEN_BADMODE ){` |
|     3 | 12678 | `			return PH7_VmThrowException(pCtx,"RuntimeException",` |
|     - | 12679 | ``				"%s(%s): Failed to open stream: `%.*s' is not a valid mode for fopen",`` |
|     2 | 12680 | `				ph7_function_name(pCtx),zErrUri ? zErrUri : "",nMode,zMode);` |
|     - | 12681 | `		}` |
|     5 | 12682 | `		if( iErr == PH7_STREAM_OPEN_NOMEM ){` |
|   ! 0 | 12683 | `			return PH7_ContextMemoryError(pCtx);` |
|     - | 12684 | `		}` |
|     5 | 12685 | `		return PH7_VmThrowException(pCtx,"RuntimeException",` |
|     2 | 12686 | `			"%s(%s): Failed to open stream: %s",ph7_function_name(pCtx),` |
|     4 | 12687 | `			zErrUri ? zErrUri : "",VfsStrerror(errno));` |
|     - | 12688 | `	}` |
|   133 | 12689 | `	pSlot = PH7_NativeAttr(pThis,SFO_H);` |
|   133 | 12690 | `	if( pSlot == 0 ){` |
|   ! 0 | 12691 | `		PH7_StreamCloseHandle(pDev->pStream,pDev->pHandle);` |
|   ! 0 | 12692 | `		MarkIOPrivateClosed(pDev);` |
|   ! 0 | 12693 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 12694 | `	}` |
|   133 | 12695 | `	PH7_MemObjRelease(pSlot);` |
|   133 | 12696 | `	pSlot->x.pOther = pDev;` |
|   133 | 12697 | `	MemObjSetType(pSlot,MEMOBJ_RES);` |
|     - | 12698 | `	/* The slot is written straight rather than through ph7_value_resource(), so` |
|     - | 12699 | `	 * it takes the handle's first count here -- see io_private.nValRef. */` |
|   133 | 12700 | `	if( PH7_StreamValueRef(pDev) ){` |
|   133 | 12701 | `		pSlot->iFlags \|= MEMOBJ_STREAMRES;` |
|    65 | 12702 | `	}` |
|   133 | 12703 | `	PH7_NativeSetAttrStr(pVm,pThis,SFO_M,zMode,nMode);` |
|   133 | 12704 | `	SfiSetName(pVm,pThis,zPath,nPath);` |
|   133 | 12705 | `	return PH7_OK;` |
|    81 | 12706 | `}` |
|     - | 12707 | `/* The class's teardown: php closes the stream with the OBJECT, which is when a` |
|     - | 12708 | ` * file written through one gets its last bytes. */` |
|   150 | 12709 | `static void SfoRelease(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     3 | 12710 | `{` |
|   153 | 12711 | `	io_private *pDev = SfoDev(pThis);` |
|    75 | 12712 | `	SXUNUSED(pVm);` |
|   153 | 12713 | `	if( pDev == 0 ){` |
|    22 | 12714 | `		return;` |
|     - | 12715 | `	}` |
|   133 | 12716 | `	PH7_StreamFilterReleaseChains(pDev);` |
|   133 | 12717 | `	PH7_StreamCloseHandle(pDev->pStream,pDev->pHandle);` |
|   133 | 12718 | `	MarkIOPrivateClosed(pDev);` |
|    78 | 12719 | `}` |
|     - | 12720 | `/*` |
|     - | 12721 | ` * SplFileObject::__construct(string $filename, string $mode = 'r',` |
|     - | 12722 | ` *                            bool $useIncludePath = false, $context = null)` |
|     - | 12723 | ` */` |
|   104 | 12724 | `static int vm_builtin_SplFileObject_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 | 12725 | `{` |
|   107 | 12726 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   107 | 12727 | `	const char *zMode = "r";` |
|   107 | 12728 | `	int nMode = 1;` |
|   107 | 12729 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|   ! 0 | 12730 | `		return PH7_OK;` |
|     - | 12731 | `	}` |
|   107 | 12732 | `	if( nArg > 1 ){` |
|    16 | 12733 | `		zMode = ph7_value_to_string(apArg[1],&nMode);` |
|     7 | 12734 | `	}` |
|   161 | 12735 | `	return SfoOpen(pCtx,pThis,apArg[0],zMode,nMode,` |
|    53 | 12736 | `		nArg > 2 ? ph7_value_to_bool(apArg[2]) : FALSE,` |
|    53 | 12737 | `		nArg > 3 && (apArg[3]->iFlags & MEMOBJ_NULL) == 0 ? apArg[3] : 0,4);` |
|    55 | 12738 | `}` |
|     - | 12739 | `/*` |
|     - | 12740 | ` * SplTempFileObject::__construct(int $maxMemory = 2097152)` |
|     - | 12741 | ` *` |
|     - | 12742 | `` * php builds a php:// URI from the argument and opens it `wb`, and the three`` |
|     - | 12743 | ` * arms are visible from outside because getPathname() answers the URI: a` |
|     - | 12744 | `` * NEGATIVE budget is `php://memory` (never spilled to disk), a budget NAMED is`` |
|     - | 12745 | `` * `php://temp/maxmemory:N` -- including 0, which spills immediately -- and no`` |
|     - | 12746 | `` * argument at all is a plain `php://temp` carrying php's own default. The path`` |
|     - | 12747 | `` * is the EMPTY string rather than the `php:/` a URI's last slash would cut.`` |
|     - | 12748 | ` */` |
|    26 | 12749 | `static int vm_builtin_SplTempFileObject_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 12750 | `{` |
|    28 | 12751 | `	ph7_vm *pVm = pCtx->pVm;` |
|    28 | 12752 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    28 | 12753 | `	sxi64 iMax = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;` |
|     - | 12754 | `	ph7_value sUri;` |
|     - | 12755 | `	sxi32 rc;` |
|    28 | 12756 | `	if( pThis == 0 ){` |
|   ! 0 | 12757 | `		return PH7_OK;` |
|     - | 12758 | `	}` |
|    28 | 12759 | `	PH7_MemObjInitFromString(pVm,&sUri,0);` |
|    28 | 12760 | `	if( iMax < 0 ){` |
|     3 | 12761 | `		PH7_MemObjStringAppend(&sUri,"php://memory",sizeof("php://memory")-1);` |
|    27 | 12762 | `	}else if( nArg > 0 ){` |
|     - | 12763 | `		char zBuf[64];` |
|     5 | 12764 | `		int n = SyBufferFormat(zBuf,sizeof(zBuf),"php://temp/maxmemory:%qd",iMax);` |
|     5 | 12765 | `		PH7_MemObjStringAppend(&sUri,zBuf,(sxu32)n);` |
|     3 | 12766 | `	}else{` |
|    22 | 12767 | `		PH7_MemObjStringAppend(&sUri,"php://temp",sizeof("php://temp")-1);` |
|     - | 12768 | `	}` |
|    28 | 12769 | `	rc = SfoOpen(pCtx,pThis,&sUri,"wb",2,FALSE,0,0);` |
|    28 | 12770 | `	PH7_MemObjRelease(&sUri);` |
|    28 | 12771 | `	if( rc == PH7_OK ){` |
|    26 | 12772 | `		PH7_NativeSetAttrStr(pVm,pThis,SFI_P,"",0);` |
|    12 | 12773 | `	}` |
|    28 | 12774 | `	return rc;` |
|    15 | 12775 | `}` |
|     - | 12776 | `/* The guard every method below opens with: the handle, or the refusal. */` |
|   470 | 12777 | `static io_private * SfoNeed(ph7_context *pCtx,sxi32 *pRc)` |
|     2 | 12778 | `{` |
|   472 | 12779 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   472 | 12780 | `	io_private *pDev = SfoDev(pThis);` |
|   472 | 12781 | `	*pRc = PH7_OK;` |
|   472 | 12782 | `	if( pDev == 0 ){` |
|     5 | 12783 | `		*pRc = PH7_VmThrowException(pCtx,"Error",` |
|     - | 12784 | `			"The parent constructor was not called: the object is in an invalid state");` |
|     2 | 12785 | `	}` |
|   472 | 12786 | `	return pDev;` |
|     2 | 12787 | `}` |
|    26 | 12788 | `static int vm_builtin_SplFileObject_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12789 | `{` |
|     - | 12790 | `	sxi32 rc;` |
|    13 | 12791 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    27 | 12792 | `	if( SfoNeed(pCtx,&rc) == 0 ){` |
|   ! 0 | 12793 | `		return rc;` |
|     - | 12794 | `	}` |
|    27 | 12795 | `	return SfoRewind(pCtx);` |
|    14 | 12796 | `}` |
|    18 | 12797 | `static int vm_builtin_SplFileObject_eof(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12798 | `{` |
|     - | 12799 | `	sxi32 rc;` |
|    19 | 12800 | `	io_private *pDev = SfoNeed(pCtx,&rc);` |
|     9 | 12801 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    19 | 12802 | `	if( pDev == 0 ){` |
|   ! 0 | 12803 | `		return rc;` |
|     - | 12804 | `	}` |
|    19 | 12805 | `	ph7_result_bool(pCtx,PH7_StreamAtEof(pDev) != 0);` |
|    19 | 12806 | `	return PH7_OK;` |
|    10 | 12807 | `}` |
|     - | 12808 | `/* php's valid(): the LINE decides under READ_AHEAD, the stream otherwise. */` |
|   122 | 12809 | `static int vm_builtin_SplFileObject_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12810 | `{` |
|   123 | 12811 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 12812 | `	sxi32 rc;` |
|     - | 12813 | `	io_private *pDev;` |
|    61 | 12814 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   123 | 12815 | `	if( SfoIsChecked(pCtx->pVm,pThis) && SfoDev(pThis) == 0 ){` |
|   ! 0 | 12816 | `		return PH7_VmThrowException(pCtx,"Error",` |
|     - | 12817 | `			"The parent constructor was not called: the object is in an invalid state");` |
|     - | 12818 | `	}` |
|   123 | 12819 | `	if( SfoFlags(pThis) & SFO_READ_AHEAD ){` |
|    25 | 12820 | `		ph7_result_bool(pCtx,PH7_NativeAttrInt(pThis,SFO_LS) != 0);` |
|    25 | 12821 | `		return PH7_OK;` |
|     - | 12822 | `	}` |
|    99 | 12823 | `	pDev = SfoNeed(pCtx,&rc);` |
|    99 | 12824 | `	if( pDev == 0 ){` |
|   ! 0 | 12825 | `		return rc;` |
|     - | 12826 | `	}` |
|    99 | 12827 | `	ph7_result_bool(pCtx,PH7_StreamAtEof(pDev) == 0);` |
|    99 | 12828 | `	return PH7_OK;` |
|    62 | 12829 | `}` |
|     - | 12830 | `/* php's fgets(), which is also this class's getCurrentLine(): a LOUD read that` |
|     - | 12831 | ` * always advances the line number. */` |
|    42 | 12832 | `static int vm_builtin_SplFileObject_fgets(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12833 | `{` |
|    43 | 12834 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 12835 | `	sxi32 rc;` |
|    43 | 12836 | `	int nLine = 0;` |
|     - | 12837 | `	const char *zLine;` |
|    21 | 12838 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    43 | 12839 | `	if( SfoNeed(pCtx,&rc) == 0 ){` |
|     3 | 12840 | `		return rc;` |
|     - | 12841 | `	}` |
|    41 | 12842 | `	if( SfoReadEx(pCtx,FALSE,1,FALSE,&rc) != SFO_READ_OK ){` |
|     3 | 12843 | `		return rc;` |
|     - | 12844 | `	}` |
|    39 | 12845 | `	zLine = SfoLine(pThis,&nLine);` |
|    39 | 12846 | `	ph7_result_string(pCtx,zLine,nLine);` |
|    39 | 12847 | `	return PH7_OK;` |
|    22 | 12848 | `}` |
|     - | 12849 | `/*` |
|     - | 12850 | ` * php's current(): read one line if nothing is held, then pick between the two` |
|     - | 12851 | ` * current values -- the STRING wins unless READ_CSV has an array beside it.` |
|     - | 12852 | ` */` |
|   110 | 12853 | `static int vm_builtin_SplFileObject_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12854 | `{` |
|   111 | 12855 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 12856 | `	sxi32 rc;` |
|     - | 12857 | `	sxi64 iHas;` |
|    55 | 12858 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   111 | 12859 | `	if( SfoNeed(pCtx,&rc) == 0 ){` |
|     3 | 12860 | `		return rc;` |
|     - | 12861 | `	}` |
|   109 | 12862 | `	if( PH7_NativeAttrInt(pThis,SFO_LS) == 0 ){` |
|    87 | 12863 | `		if( SfoReadLine(pCtx,TRUE,&rc) == SFO_READ_THROW ){` |
|   ! 0 | 12864 | `			return rc;` |
|     - | 12865 | `		}` |
|    43 | 12866 | `	}` |
|   109 | 12867 | `	iHas = PH7_NativeAttrInt(pThis,SFO_LS);` |
|   108 | 12868 | `	if( (iHas & SFO_HAS_LINE)` |
|   147 | 12869 | `	 && (!(SfoFlags(pThis) & SFO_READ_CSV) \|\| (iHas & SFO_HAS_ZVAL) == 0) ){` |
|    85 | 12870 | `		int nLine = 0;` |
|    85 | 12871 | `		const char *zLine = SfoLine(pThis,&nLine);` |
|    85 | 12872 | `		ph7_result_string(pCtx,zLine,nLine);` |
|    67 | 12873 | `	}else if( iHas & SFO_HAS_ZVAL ){` |
|    17 | 12874 | `		ph7_value *pZ = PH7_NativeAttr(pThis,SFO_Z);` |
|    17 | 12875 | `		if( pZ ){` |
|    17 | 12876 | `			ph7_result_value(pCtx,pZ);` |
|     9 | 12877 | `		}else{` |
|   ! 0 | 12878 | `			ph7_result_bool(pCtx,0);` |
|     - | 12879 | `		}` |
|     9 | 12880 | `	}else{` |
|     9 | 12881 | `		ph7_result_bool(pCtx,0);` |
|     - | 12882 | `	}` |
|   109 | 12883 | `	return PH7_OK;` |
|    56 | 12884 | `}` |
|     - | 12885 | `/* php's key(): the stored count, deliberately WITHOUT reading ahead -- which is` |
|     - | 12886 | ` * what lets fgetc() count newlines instead of lines. */` |
|   126 | 12887 | `static int vm_builtin_SplFileObject_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12888 | `{` |
|     - | 12889 | `	sxi32 rcChk;` |
|    63 | 12890 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   127 | 12891 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|     3 | 12892 | `		return rcChk;` |
|     - | 12893 | `	}` |
|   125 | 12894 | `	ph7_result_int64(pCtx,PH7_NativeAttrInt(PH7_ContextThis(pCtx),SFO_K));` |
|   125 | 12895 | `	return PH7_OK;` |
|    64 | 12896 | `}` |
|   100 | 12897 | `static int vm_builtin_SplFileObject_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12898 | `{` |
|   101 | 12899 | `	ph7_vm *pVm = pCtx->pVm;` |
|   101 | 12900 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 12901 | `	sxi32 rc;` |
|    50 | 12902 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   101 | 12903 | `	if( SfoNeed(pCtx,&rc) == 0 ){` |
|   ! 0 | 12904 | `		return rc;` |
|     - | 12905 | `	}` |
|   101 | 12906 | `	SfoFreeLine(pVm,pThis);` |
|   101 | 12907 | `	if( SfoFlags(pThis) & SFO_READ_AHEAD ){` |
|    21 | 12908 | `		if( SfoReadLine(pCtx,TRUE,&rc) == SFO_READ_THROW ){` |
|   ! 0 | 12909 | `			return rc;` |
|     - | 12910 | `		}` |
|    10 | 12911 | `	}` |
|   101 | 12912 | `	PH7_NativeSetAttrInt(pVm,pThis,SFO_K,PH7_NativeAttrInt(pThis,SFO_K) + 1);` |
|   101 | 12913 | `	return PH7_OK;` |
|    51 | 12914 | `}` |
|    18 | 12915 | `static int vm_builtin_SplFileObject_setFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12916 | `{` |
|     - | 12917 | `	sxi32 rcChk;` |
|    19 | 12918 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 12919 | `		return rcChk;` |
|     - | 12920 | `	}` |
|     - | 12921 | `	/* php stores the WHOLE word and masks only on the way out. */` |
|    37 | 12922 | `	PH7_NativeSetAttrInt(pCtx->pVm,PH7_ContextThis(pCtx),SFO_FL,` |
|    18 | 12923 | `		nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0);` |
|    19 | 12924 | `	return PH7_OK;` |
|    10 | 12925 | `}` |
|     4 | 12926 | `static int vm_builtin_SplFileObject_getFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12927 | `{` |
|     - | 12928 | `	sxi32 rcChk;` |
|     2 | 12929 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     5 | 12930 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 12931 | `		return rcChk;` |
|     - | 12932 | `	}` |
|     5 | 12933 | `	ph7_result_int64(pCtx,SfoFlags(PH7_ContextThis(pCtx)) & SFO_FLAGS_MASK);` |
|     5 | 12934 | `	return PH7_OK;` |
|     3 | 12935 | `}` |
|     6 | 12936 | `static int vm_builtin_SplFileObject_setMaxLineLen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 12937 | `{` |
|     8 | 12938 | `	sxi64 iLen = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;` |
|     - | 12939 | `	sxi32 rcChk;` |
|     8 | 12940 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 12941 | `		return rcChk;` |
|     - | 12942 | `	}` |
|     8 | 12943 | `	if( iLen < 0 ){` |
|     8 | 12944 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 12945 | `			"%s(): Argument #1 ($maxLength) must be greater than or equal to 0",` |
|     2 | 12946 | `			ph7_function_name(pCtx));` |
|     - | 12947 | `	}` |
|     3 | 12948 | `	PH7_NativeSetAttrInt(pCtx->pVm,PH7_ContextThis(pCtx),SFO_ML,iLen);` |
|     3 | 12949 | `	return PH7_OK;` |
|     5 | 12950 | `}` |
|     2 | 12951 | `static int vm_builtin_SplFileObject_getMaxLineLen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12952 | `{` |
|     - | 12953 | `	sxi32 rcChk;` |
|     1 | 12954 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     3 | 12955 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 12956 | `		return rcChk;` |
|     - | 12957 | `	}` |
|     3 | 12958 | `	ph7_result_int64(pCtx,PH7_NativeAttrInt(PH7_ContextThis(pCtx),SFO_ML));` |
|     3 | 12959 | `	return PH7_OK;` |
|     2 | 12960 | `}` |
|     - | 12961 | `/* php's RecursiveIterator half: a file has no children and says so. */` |
|     4 | 12962 | `static int vm_builtin_SplFileObject_hasChildren(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12963 | `{` |
|     - | 12964 | `	sxi32 rcChk;` |
|     2 | 12965 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     5 | 12966 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 12967 | `		return rcChk;` |
|     - | 12968 | `	}` |
|     5 | 12969 | `	ph7_result_bool(pCtx,0);` |
|     5 | 12970 | `	return PH7_OK;` |
|     3 | 12971 | `}` |
|     4 | 12972 | `static int vm_builtin_SplFileObject_getChildren(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12973 | `{` |
|     - | 12974 | `	sxi32 rcChk;` |
|     2 | 12975 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     5 | 12976 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 12977 | `		return rcChk;` |
|     - | 12978 | `	}` |
|     5 | 12979 | `	ph7_result_null(pCtx);` |
|     5 | 12980 | `	return PH7_OK;` |
|     3 | 12981 | `}` |
|     - | 12982 | `/*` |
|     - | 12983 | ` * php's fgetcsv(): the three arguments override this instance's settings for` |
|     - | 12984 | ` * ONE call, and the argument NUMBERS are this spelling's own.` |
|     - | 12985 | ` */` |
|    12 | 12986 | `static int vm_builtin_SplFileObject_fgetcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12987 | `{` |
|    13 | 12988 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 12989 | `	int delim,encl,escape;` |
|     - | 12990 | `	sxi32 rc;` |
|    13 | 12991 | `	if( SfoNeed(pCtx,&rc) == 0 ){` |
|   ! 0 | 12992 | `		return rc;` |
|     - | 12993 | `	}` |
|    13 | 12994 | `	SfoCsvControl(pThis,&delim,&encl,&escape);` |
|    13 | 12995 | `	if( nArg > 0 ){` |
|    13 | 12996 | `		rc = PH7_CsvCharArg(pCtx,apArg[0],1,"separator",0,&delim);` |
|    13 | 12997 | `		if( rc != PH7_OK ){` |
|   ! 0 | 12998 | `			return rc;` |
|     - | 12999 | `		}` |
|     6 | 13000 | `	}` |
|    13 | 13001 | `	if( nArg > 1 ){` |
|    13 | 13002 | `		rc = PH7_CsvCharArg(pCtx,apArg[1],2,"enclosure",0,&encl);` |
|    13 | 13003 | `		if( rc != PH7_OK ){` |
|     3 | 13004 | `			return rc;` |
|     - | 13005 | `		}` |
|     5 | 13006 | `	}` |
|    11 | 13007 | `	if( nArg > 2 ){` |
|    11 | 13008 | `		rc = PH7_CsvCharArg(pCtx,apArg[2],3,"escape",1,&escape);` |
|    11 | 13009 | `		if( rc != PH7_OK ){` |
|   ! 0 | 13010 | `			return rc;` |
|     - | 13011 | `		}` |
|     5 | 13012 | `	}` |
|     - | 13013 | `	{` |
|     - | 13014 | `		ph7_value sOut;` |
|     - | 13015 | `		int r;` |
|    11 | 13016 | `		PH7_MemObjInit(pCtx->pVm,&sOut);` |
|    11 | 13017 | `		r = SfoReadCsv(pCtx,delim,encl,escape,&sOut,TRUE,&rc);` |
|    11 | 13018 | `		if( r == SFO_READ_OK ){` |
|    11 | 13019 | `			ph7_result_value(pCtx,&sOut);` |
|     5 | 13020 | `		}else if( r == SFO_READ_FAIL ){` |
|   ! 0 | 13021 | `			ph7_result_bool(pCtx,0);` |
|   ! 0 | 13022 | `		}` |
|    11 | 13023 | `		PH7_MemObjRelease(&sOut);` |
|    11 | 13024 | `		return r == SFO_READ_THROW ? rc : PH7_OK;` |
|     - | 13025 | `	}` |
|     7 | 13026 | `}` |
|     - | 13027 | `/* php's fputcsv(): the same overrides, at the positions THIS method numbers. */` |
|     4 | 13028 | `static int vm_builtin_SplFileObject_fputcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 13029 | `{` |
|     5 | 13030 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 13031 | `	ph7_value *apOut[6];` |
|     - | 13032 | `	ph7_value sDelim,sEncl,sEsc;` |
|     - | 13033 | `	int delim,encl,escape,nOut,r;` |
|     - | 13034 | `	sxi32 rc;` |
|     5 | 13035 | `	if( SfoNeed(pCtx,&rc) == 0 ){` |
|   ! 0 | 13036 | `		return rc;` |
|     - | 13037 | `	}` |
|     5 | 13038 | `	if( nArg < 1 ){` |
|   ! 0 | 13039 | `		return PH7_OK;` |
|     - | 13040 | `	}` |
|     5 | 13041 | `	SfoCsvControl(pThis,&delim,&encl,&escape);` |
|     5 | 13042 | `	if( nArg > 1 ){` |
|     5 | 13043 | `		rc = PH7_CsvCharArg(pCtx,apArg[1],2,"separator",0,&delim);` |
|     5 | 13044 | `		if( rc != PH7_OK ){` |
|   ! 0 | 13045 | `			return rc;` |
|     - | 13046 | `		}` |
|     2 | 13047 | `	}` |
|     5 | 13048 | `	if( nArg > 2 ){` |
|     5 | 13049 | `		rc = PH7_CsvCharArg(pCtx,apArg[2],3,"enclosure",0,&encl);` |
|     5 | 13050 | `		if( rc != PH7_OK ){` |
|     3 | 13051 | `			return rc;` |
|     - | 13052 | `		}` |
|     1 | 13053 | `	}` |
|     3 | 13054 | `	if( nArg > 3 ){` |
|     3 | 13055 | `		rc = PH7_CsvCharArg(pCtx,apArg[3],4,"escape",1,&escape);` |
|     3 | 13056 | `		if( rc != PH7_OK ){` |
|   ! 0 | 13057 | `			return rc;` |
|     - | 13058 | `		}` |
|     1 | 13059 | `	}` |
|     - | 13060 | `	/* The writer lives in vfs_stream.c and takes the three as STRINGS, at` |
|     - | 13061 | `	 * fputcsv()'s own positions; the settings resolved above are handed over in` |
|     - | 13062 | `	 * that shape rather than re-parsed there. */` |
|     3 | 13063 | `	PH7_MemObjInitFromString(pCtx->pVm,&sDelim,0);` |
|     3 | 13064 | `	PH7_MemObjInitFromString(pCtx->pVm,&sEncl,0);` |
|     3 | 13065 | `	PH7_MemObjInitFromString(pCtx->pVm,&sEsc,0);` |
|     - | 13066 | `	{` |
|     3 | 13067 | `		char c = (char)delim;` |
|     3 | 13068 | `		PH7_MemObjStringAppend(&sDelim,&c,sizeof(char));` |
|     3 | 13069 | `		c = (char)encl;` |
|     3 | 13070 | `		PH7_MemObjStringAppend(&sEncl,&c,sizeof(char));` |
|     3 | 13071 | `		if( escape != PH7_CSV_NO_ESCAPE ){` |
|     3 | 13072 | `			c = (char)escape;` |
|     3 | 13073 | `			PH7_MemObjStringAppend(&sEsc,&c,sizeof(char));` |
|     1 | 13074 | `		}` |
|     - | 13075 | `	}` |
|     3 | 13076 | `	apOut[0] = PH7_NativeAttr(pThis,SFO_H);` |
|     3 | 13077 | `	apOut[1] = apArg[0];` |
|     3 | 13078 | `	apOut[2] = &sDelim;` |
|     3 | 13079 | `	apOut[3] = &sEncl;` |
|     3 | 13080 | `	apOut[4] = &sEsc;` |
|     3 | 13081 | `	nOut = 5;` |
|     3 | 13082 | `	if( nArg > 4 ){` |
|     3 | 13083 | `		apOut[5] = apArg[4];` |
|     3 | 13084 | `		nOut = 6;` |
|     1 | 13085 | `	}` |
|     3 | 13086 | `	r = PH7_builtin_fputcsv(pCtx,nOut,apOut);` |
|     3 | 13087 | `	PH7_MemObjRelease(&sDelim);` |
|     3 | 13088 | `	PH7_MemObjRelease(&sEncl);` |
|     3 | 13089 | `	PH7_MemObjRelease(&sEsc);` |
|     3 | 13090 | `	return r;` |
|     3 | 13091 | `}` |
|     4 | 13092 | `static int vm_builtin_SplFileObject_setCsvControl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 13093 | `{` |
|     5 | 13094 | `	ph7_vm *pVm = pCtx->pVm;` |
|     5 | 13095 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     5 | 13096 | `	int delim = ',',encl = '"',escape = '\\';` |
|     - | 13097 | `	sxi32 rc,rcChk;` |
|     5 | 13098 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 13099 | `		return rcChk;` |
|     - | 13100 | `	}` |
|     5 | 13101 | `	if( nArg > 0 ){` |
|     5 | 13102 | `		rc = PH7_CsvCharArg(pCtx,apArg[0],1,"separator",0,&delim);` |
|     5 | 13103 | `		if( rc != PH7_OK ){` |
|     3 | 13104 | `			return rc;` |
|     - | 13105 | `		}` |
|     1 | 13106 | `	}` |
|     3 | 13107 | `	if( nArg > 1 ){` |
|     3 | 13108 | `		rc = PH7_CsvCharArg(pCtx,apArg[1],2,"enclosure",0,&encl);` |
|     3 | 13109 | `		if( rc != PH7_OK ){` |
|   ! 0 | 13110 | `			return rc;` |
|     - | 13111 | `		}` |
|     1 | 13112 | `	}` |
|     3 | 13113 | `	if( nArg > 2 ){` |
|     3 | 13114 | `		rc = PH7_CsvCharArg(pCtx,apArg[2],3,"escape",1,&escape);` |
|     3 | 13115 | `		if( rc != PH7_OK ){` |
|   ! 0 | 13116 | `			return rc;` |
|     - | 13117 | `		}` |
|     - | 13118 | `		/* Naming the escape at all is what stops php's deprecation notice. */` |
|     3 | 13119 | `		PH7_NativeSetAttrInt(pVm,pThis,SFO_ED,0);` |
|     1 | 13120 | `	}` |
|     3 | 13121 | `	PH7_NativeSetAttrInt(pVm,pThis,SFO_D,delim);` |
|     3 | 13122 | `	PH7_NativeSetAttrInt(pVm,pThis,SFO_EN,encl);` |
|     3 | 13123 | `	PH7_NativeSetAttrInt(pVm,pThis,SFO_ES,escape);` |
|     3 | 13124 | `	return PH7_OK;` |
|     3 | 13125 | `}` |
|     4 | 13126 | `static int vm_builtin_SplFileObject_getCsvControl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 13127 | `{` |
|     5 | 13128 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 13129 | `	ph7_value *pArr,*pVal;` |
|     - | 13130 | `	int delim,encl,escape;` |
|     - | 13131 | `	char c;` |
|     - | 13132 | `	sxi32 rcChk;` |
|     2 | 13133 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     5 | 13134 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 13135 | `		return rcChk;` |
|     - | 13136 | `	}` |
|     5 | 13137 | `	SfoCsvControl(pThis,&delim,&encl,&escape);` |
|     5 | 13138 | `	pArr = ph7_context_new_array(pCtx);` |
|     5 | 13139 | `	pVal = ph7_context_new_scalar(pCtx);` |
|     5 | 13140 | `	if( pArr == 0 \|\| pVal == 0 ){` |
|   ! 0 | 13141 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 13142 | `	}` |
|     5 | 13143 | `	c = (char)delim;` |
|     5 | 13144 | `	ph7_value_string(pVal,&c,1);` |
|     5 | 13145 | `	ph7_array_add_elem(pArr,0,pVal);` |
|     5 | 13146 | `	ph7_value_reset_string_cursor(pVal);` |
|     5 | 13147 | `	c = (char)encl;` |
|     5 | 13148 | `	ph7_value_string(pVal,&c,1);` |
|     5 | 13149 | `	ph7_array_add_elem(pArr,0,pVal);` |
|     5 | 13150 | `	ph7_value_reset_string_cursor(pVal);` |
|     - | 13151 | `	/* A disabled escape is reported as the EMPTY string, not as a byte. */` |
|     5 | 13152 | `	if( escape != PH7_CSV_NO_ESCAPE ){` |
|     3 | 13153 | `		c = (char)escape;` |
|     3 | 13154 | `		ph7_value_string(pVal,&c,1);` |
|     2 | 13155 | `	}else{` |
|     3 | 13156 | `		ph7_value_string(pVal,"",0);` |
|     - | 13157 | `	}` |
|     5 | 13158 | `	ph7_array_add_elem(pArr,0,pVal);` |
|     5 | 13159 | `	ph7_result_value(pCtx,pArr);` |
|     5 | 13160 | `	return PH7_OK;` |
|     3 | 13161 | `}` |
|     - | 13162 | `/*` |
|     - | 13163 | ` * The eight methods that are the corresponding builtin over this object's own` |
|     - | 13164 | ` * handle. Each pre-validates what php validates IN THE METHOD -- the argument` |
|     - | 13165 | ` * numbers are the method's, one lower than the function's -- and then hands the` |
|     - | 13166 | ` * work to the one implementation there is. Every diagnostic the builtin raises` |
|     - | 13167 | ` * itself is worded from ph7_function_name(), which in here is the qualified` |
|     - | 13168 | ` * method name php prints.` |
|     - | 13169 | ` */` |
|    36 | 13170 | `static int SfoDelegate(ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|     - | 13171 | `	int (*xFunc)(ph7_context *,int,ph7_value **))` |
|     2 | 13172 | `{` |
|     - | 13173 | `	ph7_value *apOut[8];` |
|     - | 13174 | `	sxi32 rc;` |
|     - | 13175 | `	int i;` |
|    38 | 13176 | `	if( SfoNeed(pCtx,&rc) == 0 ){` |
|   ! 0 | 13177 | `		return rc;` |
|     - | 13178 | `	}` |
|    38 | 13179 | `	if( nArg > (int)(SX_ARRAYSIZE(apOut) - 1) ){` |
|   ! 0 | 13180 | `		nArg = (int)(SX_ARRAYSIZE(apOut) - 1);` |
|   ! 0 | 13181 | `	}` |
|    38 | 13182 | `	apOut[0] = PH7_NativeAttr(PH7_ContextThis(pCtx),SFO_H);` |
|    68 | 13183 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    32 | 13184 | `		apOut[i+1] = apArg[i];` |
|    17 | 13185 | `	}` |
|    38 | 13186 | `	return xFunc(pCtx,nArg + 1,apOut);` |
|    20 | 13187 | `}` |
|   ! 0 | 13188 | `static int vm_builtin_SplFileObject_fflush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 13189 | `{` |
|   ! 0 | 13190 | `	return SfoDelegate(pCtx,nArg,apArg,PH7_builtin_fflush);` |
|   ! 0 | 13191 | `}` |
|     8 | 13192 | `static int vm_builtin_SplFileObject_ftell(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 13193 | `{` |
|    10 | 13194 | `	return SfoDelegate(pCtx,nArg,apArg,PH7_builtin_ftell);` |
|     2 | 13195 | `}` |
|     2 | 13196 | `static int vm_builtin_SplFileObject_fstat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 13197 | `{` |
|     3 | 13198 | `	return SfoDelegate(pCtx,nArg,apArg,PH7_builtin_fstat);` |
|     1 | 13199 | `}` |
|     2 | 13200 | `static int vm_builtin_SplFileObject_fpassthru(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 13201 | `{` |
|     3 | 13202 | `	return SfoDelegate(pCtx,nArg,apArg,PH7_builtin_fpassthru);` |
|     1 | 13203 | `}` |
|    16 | 13204 | `static int vm_builtin_SplFileObject_fwrite(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 13205 | `{` |
|    18 | 13206 | `	return SfoDelegate(pCtx,nArg,apArg,PH7_builtin_fwrite);` |
|     2 | 13207 | `}` |
|     - | 13208 | `/* php validates the operation in the METHOD, so the refusal names argument #1. */` |
|     6 | 13209 | `static int vm_builtin_SplFileObject_flock(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 13210 | `{` |
|     - | 13211 | `	sxi32 rcChk;` |
|     7 | 13212 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 13213 | `		return rcChk;` |
|     - | 13214 | `	}` |
|     7 | 13215 | `	if( nArg > 0 && (ph7_value_to_int(apArg[0]) & 3) == 0 ){` |
|     4 | 13216 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 13217 | `			"%s(): Argument #1 ($operation) must be one of LOCK_SH, LOCK_EX, or LOCK_UN",` |
|     1 | 13218 | `			ph7_function_name(pCtx));` |
|     - | 13219 | `	}` |
|     5 | 13220 | `	return SfoDelegate(pCtx,nArg,apArg,PH7_builtin_flock);` |
|     4 | 13221 | `}` |
|     2 | 13222 | `static int vm_builtin_SplFileObject_ftruncate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 13223 | `{` |
|     - | 13224 | `	sxi32 rcChk;` |
|     3 | 13225 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 13226 | `		return rcChk;` |
|     - | 13227 | `	}` |
|     3 | 13228 | `	if( nArg > 0 && ph7_value_to_int64(apArg[0]) < 0 ){` |
|     4 | 13229 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 13230 | `			"%s(): Argument #1 ($size) must be greater than or equal to 0",` |
|     1 | 13231 | `			ph7_function_name(pCtx));` |
|     - | 13232 | `	}` |
|   ! 0 | 13233 | `	return SfoDelegate(pCtx,nArg,apArg,PH7_builtin_ftruncate);` |
|     2 | 13234 | `}` |
|     4 | 13235 | `static int vm_builtin_SplFileObject_fread(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 13236 | `{` |
|     - | 13237 | `	sxi32 rcChk;` |
|     5 | 13238 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 13239 | `		return rcChk;` |
|     - | 13240 | `	}` |
|     5 | 13241 | `	if( nArg < 1 \|\| ph7_value_to_int64(apArg[0]) <= 0 ){` |
|     4 | 13242 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 13243 | `			"%s(): Argument #1 ($length) must be greater than 0",` |
|     1 | 13244 | `			ph7_function_name(pCtx));` |
|     - | 13245 | `	}` |
|     3 | 13246 | `	return SfoDelegate(pCtx,nArg,apArg,PH7_builtin_fread);` |
|     3 | 13247 | `}` |
|     - | 13248 | `/* php's fseek() drops the held line first: the position moved, so what was read` |
|     - | 13249 | ` * ahead no longer describes it. */` |
|     2 | 13250 | `static int vm_builtin_SplFileObject_fseek(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 13251 | `{` |
|     - | 13252 | `	sxi32 rc;` |
|     3 | 13253 | `	if( SfoNeed(pCtx,&rc) == 0 ){` |
|   ! 0 | 13254 | `		return rc;` |
|     - | 13255 | `	}` |
|     3 | 13256 | `	SfoFreeLine(pCtx->pVm,PH7_ContextThis(pCtx));` |
|     3 | 13257 | `	return SfoDelegate(pCtx,nArg,apArg,PH7_builtin_fseek);` |
|     2 | 13258 | `}` |
|     - | 13259 | `/*` |
|     - | 13260 | ` * php's fgetc(): one byte, the held line dropped, and the line number advanced` |
|     - | 13261 | ` * only when the byte IS a newline.` |
|     - | 13262 | ` */` |
|     8 | 13263 | `static int vm_builtin_SplFileObject_fgetc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 13264 | `{` |
|     9 | 13265 | `	ph7_vm *pVm = pCtx->pVm;` |
|     9 | 13266 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 13267 | `	io_private *pDev;` |
|     - | 13268 | `	char c;` |
|     - | 13269 | `	sxi32 rc;` |
|     4 | 13270 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     9 | 13271 | `	pDev = SfoNeed(pCtx,&rc);` |
|     9 | 13272 | `	if( pDev == 0 ){` |
|   ! 0 | 13273 | `		return rc;` |
|     - | 13274 | `	}` |
|     9 | 13275 | `	SfoFreeLine(pVm,pThis);` |
|     9 | 13276 | `	if( PH7_StreamRead(pDev,&c,sizeof(char)) < 1 ){` |
|   ! 0 | 13277 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 13278 | `		return PH7_OK;` |
|     - | 13279 | `	}` |
|     9 | 13280 | `	if( c == '\n' ){` |
|     3 | 13281 | `		PH7_NativeSetAttrInt(pVm,pThis,SFO_K,PH7_NativeAttrInt(pThis,SFO_K) + 1);` |
|     1 | 13282 | `	}` |
|     9 | 13283 | `	ph7_result_string(pCtx,&c,sizeof(char));` |
|     9 | 13284 | `	return PH7_OK;` |
|     5 | 13285 | `}` |
|     - | 13286 | `/* php's fscanf(): the format is run over the NEXT LINE, read loudly. */` |
|     4 | 13287 | `static int vm_builtin_SplFileObject_fscanf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 13288 | `{` |
|     5 | 13289 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 13290 | `	const char *zFmt,*zLine;` |
|     5 | 13291 | `	int nFmt = 0,nLine = 0;` |
|     - | 13292 | `	SyBlob sLine;` |
|     - | 13293 | `	sxi32 rc;` |
|     5 | 13294 | `	if( SfoNeed(pCtx,&rc) == 0 ){` |
|   ! 0 | 13295 | `		return rc;` |
|     - | 13296 | `	}` |
|     5 | 13297 | `	if( nArg < 1 ){` |
|   ! 0 | 13298 | `		return PH7_OK;` |
|     - | 13299 | `	}` |
|     5 | 13300 | `	if( SfoReadOne(pCtx,FALSE,FALSE,&rc) != SFO_READ_OK ){` |
|   ! 0 | 13301 | `		return rc;` |
|     - | 13302 | `	}` |
|     - | 13303 | `	/* The scan allocates, and allocating moves the slot the line lives in. The` |
|     - | 13304 | `	 * length is read in its own statement: as one argument beside the call that` |
|     - | 13305 | `	 * WRITES it, nothing orders the two. */` |
|     5 | 13306 | `	zLine = SfoLine(pThis,&nLine);` |
|     5 | 13307 | `	SyBlobInit(&sLine,&pCtx->pVm->sAllocator);` |
|     5 | 13308 | `	SyBlobAppend(&sLine,zLine,(sxu32)nLine);` |
|     5 | 13309 | `	zFmt = ph7_value_to_string(apArg[0],&nFmt);` |
|     7 | 13310 | `	rc = PH7_ScanfRun(pCtx,(const char *)SyBlobData(&sLine),(int)SyBlobLength(&sLine),` |
|     2 | 13311 | `		zFmt,nFmt,&apArg[1],nArg - 1);` |
|     5 | 13312 | `	SyBlobRelease(&sLine);` |
|     5 | 13313 | `	return (int)rc;` |
|     3 | 13314 | `}` |
|     - | 13315 | `/*` |
|     - | 13316 | ` * php's seek(): rewind, then walk FORWARD through the object's own read -- so a` |
|     - | 13317 | ` * subclass's getCurrentLine() is obeyed -- and, without READ_AHEAD, land one` |
|     - | 13318 | ` * past with nothing held.` |
|     - | 13319 | ` */` |
|     6 | 13320 | `static int vm_builtin_SplFileObject_seek(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 13321 | `{` |
|     7 | 13322 | `	ph7_vm *pVm = pCtx->pVm;` |
|     7 | 13323 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 13324 | `	sxi64 iLine,i;` |
|     - | 13325 | `	sxi32 rc;` |
|     7 | 13326 | `	if( SfoNeed(pCtx,&rc) == 0 ){` |
|   ! 0 | 13327 | `		return rc;` |
|     - | 13328 | `	}` |
|     7 | 13329 | `	iLine = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;` |
|     7 | 13330 | `	if( iLine < 0 ){` |
|     4 | 13331 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 13332 | `			"%s(): Argument #1 ($line) must be greater than or equal to 0",` |
|     1 | 13333 | `			ph7_function_name(pCtx));` |
|     - | 13334 | `	}` |
|     5 | 13335 | `	rc = SfoRewind(pCtx);` |
|     5 | 13336 | `	if( rc != PH7_OK ){` |
|   ! 0 | 13337 | `		return rc;` |
|     - | 13338 | `	}` |
|    19 | 13339 | `	for( i = 0 ; i < iLine ; ++i ){` |
|    17 | 13340 | `		int r = SfoReadLine(pCtx,TRUE,&rc);` |
|    17 | 13341 | `		if( r == SFO_READ_THROW ){` |
|   ! 0 | 13342 | `			return rc;` |
|     - | 13343 | `		}` |
|    17 | 13344 | `		if( r != SFO_READ_OK ){` |
|     3 | 13345 | `			return PH7_OK;` |
|     - | 13346 | `		}` |
|     8 | 13347 | `	}` |
|     3 | 13348 | `	if( iLine > 0 && (SfoFlags(pThis) & SFO_READ_AHEAD) == 0 ){` |
|     3 | 13349 | `		PH7_NativeSetAttrInt(pVm,pThis,SFO_K,PH7_NativeAttrInt(pThis,SFO_K) + 1);` |
|     3 | 13350 | `		SfoFreeLine(pVm,pThis);` |
|     1 | 13351 | `	}` |
|     3 | 13352 | `	return PH7_OK;` |
|     4 | 13353 | `}` |
|     - | 13354 | `/* php's __toString(): the current line, read LOUDLY when there is none. */` |
|     4 | 13355 | `static int vm_builtin_SplFileObject_toString(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 13356 | `{` |
|     5 | 13357 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     5 | 13358 | `	int nLine = 0;` |
|     - | 13359 | `	const char *zLine;` |
|     - | 13360 | `	sxi32 rc;` |
|     2 | 13361 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     5 | 13362 | `	if( SfoNeed(pCtx,&rc) == 0 ){` |
|   ! 0 | 13363 | `		return rc;` |
|     - | 13364 | `	}` |
|     5 | 13365 | `	if( (PH7_NativeAttrInt(pThis,SFO_LS) & SFO_HAS_LINE) == 0 ){` |
|     5 | 13366 | `		if( SfoReadLine(pCtx,FALSE,&rc) != SFO_READ_OK ){` |
|   ! 0 | 13367 | `			return rc;` |
|     - | 13368 | `		}` |
|     2 | 13369 | `	}` |
|     5 | 13370 | `	zLine = SfoLine(pThis,&nLine);` |
|     5 | 13371 | `	ph7_result_string(pCtx,zLine,nLine);` |
|     5 | 13372 | `	return PH7_OK;` |
|     3 | 13373 | `}` |
|     - | 13374 | `/*` |
|     - | 13375 | ` * The declaration. Method ORDER, signatures and tentative return types are` |
|     - | 13376 | ` * spl_directory.stub.php's. php gives this class NO clone handler at all, which` |
|     - | 13377 | ` * is its "uncloneable" -- and the same @not-serializable SplFileInfo carries.` |
|     - | 13378 | ` */` |
|  8445 | 13379 | `static sxi32 VmInstallSplFileObject(ph7_vm *pVm)` |
|     5 | 13380 | `{` |
|     - | 13381 | `	static const PH7_NativePropDef aFileProp[] = {` |
|     - | 13382 | `		{ SFO_H,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - | 13383 | `		{ SFO_M,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "r", 0.0 }, 0 },` |
|     - | 13384 | `		{ SFO_FL, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - | 13385 | `		{ SFO_ML, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - | 13386 | `		{ SFO_D,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, ',', 0, 0.0 }, 0 },` |
|     - | 13387 | `		{ SFO_EN, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, '"', 0, 0.0 }, 0 },` |
|     - | 13388 | `		{ SFO_ES, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, '\\', 0, 0.0 }, 0 },` |
|     - | 13389 | `		{ SFO_ED, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 1, 0, 0.0 }, 0 },` |
|     - | 13390 | `		{ SFO_L,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },` |
|     - | 13391 | `		{ SFO_Z,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - | 13392 | `		{ SFO_LS, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - | 13393 | `		{ SFO_K,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - | 13394 | `	};` |
|     - | 13395 | `	static const PH7_NativeConstDef aFileConst[] = {` |
|     - | 13396 | `		{ "DROP_NEW_LINE", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SFO_DROP_NEW_LINE, 0, 0.0 },` |
|     - | 13397 | `		{ "READ_AHEAD",    PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SFO_READ_AHEAD, 0, 0.0 },` |
|     - | 13398 | `		{ "SKIP_EMPTY",    PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SFO_SKIP_EMPTY, 0, 0.0 },` |
|     - | 13399 | `		{ "READ_CSV",      PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SFO_READ_CSV, 0, 0.0 },` |
|     - | 13400 | `	};` |
|     - | 13401 | `	static const PH7_NativeMethodDef aFileMethod[] = {` |
|     - | 13402 | `		{ "__construct",   PH7_MOD_PUBLIC,` |
|     - | 13403 | `		  "string $filename, string $mode = \"r\", bool $useIncludePath = false, "` |
|     - | 13404 | `		  "$context = null", 0, vm_builtin_SplFileObject_construct },` |
|     - | 13405 | `		{ "rewind",        PH7_MOD_PUBLIC, "", "@void", vm_builtin_SplFileObject_rewind },` |
|     - | 13406 | `		{ "eof",           PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplFileObject_eof },` |
|     - | 13407 | `		{ "valid",         PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplFileObject_valid },` |
|     - | 13408 | `		{ "fgets",         PH7_MOD_PUBLIC, "", "@string", vm_builtin_SplFileObject_fgets },` |
|     - | 13409 | `		{ "fread",         PH7_MOD_PUBLIC, "int $length", "@string\|false",` |
|     - | 13410 | `		  vm_builtin_SplFileObject_fread },` |
|     - | 13411 | `		{ "fgetcsv",       PH7_MOD_PUBLIC,` |
|     - | 13412 | `		  "string $separator = \",\", string $enclosure = \"\\\"\", string $escape = \"\\\\\"",` |
|     - | 13413 | `		  "@array\|false", vm_builtin_SplFileObject_fgetcsv },` |
|     - | 13414 | `		{ "fputcsv",       PH7_MOD_PUBLIC,` |
|     - | 13415 | `		  "array $fields, string $separator = \",\", string $enclosure = \"\\\"\", "` |
|     - | 13416 | `		  "string $escape = \"\\\\\", string $eol = \"\n\"", "@int\|false",` |
|     - | 13417 | `		  vm_builtin_SplFileObject_fputcsv },` |
|     - | 13418 | `		{ "setCsvControl", PH7_MOD_PUBLIC,` |
|     - | 13419 | `		  "string $separator = \",\", string $enclosure = \"\\\"\", string $escape = \"\\\\\"",` |
|     - | 13420 | `		  "@void", vm_builtin_SplFileObject_setCsvControl },` |
|     - | 13421 | `		{ "getCsvControl", PH7_MOD_PUBLIC, "", "@array",` |
|     - | 13422 | `		  vm_builtin_SplFileObject_getCsvControl },` |
|     - | 13423 | `		{ "flock",         PH7_MOD_PUBLIC, "int $operation, &$wouldBlock = null", "@bool",` |
|     - | 13424 | `		  vm_builtin_SplFileObject_flock },` |
|     - | 13425 | `		{ "fflush",        PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplFileObject_fflush },` |
|     - | 13426 | `		{ "ftell",         PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_SplFileObject_ftell },` |
|     - | 13427 | `		{ "fseek",         PH7_MOD_PUBLIC, "int $offset, int $whence = SEEK_SET", "@int",` |
|     - | 13428 | `		  vm_builtin_SplFileObject_fseek },` |
|     - | 13429 | `		{ "fgetc",         PH7_MOD_PUBLIC, "", "@string\|false", vm_builtin_SplFileObject_fgetc },` |
|     - | 13430 | `		{ "fpassthru",     PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplFileObject_fpassthru },` |
|     - | 13431 | `		{ "fscanf",        PH7_MOD_PUBLIC, "string $format, mixed &...$vars", "@array\|int\|null",` |
|     - | 13432 | `		  vm_builtin_SplFileObject_fscanf },` |
|     - | 13433 | `		{ "fwrite",        PH7_MOD_PUBLIC, "string $data, ?int $length = null", "@int\|false",` |
|     - | 13434 | `		  vm_builtin_SplFileObject_fwrite },` |
|     - | 13435 | `		{ "fstat",         PH7_MOD_PUBLIC, "", "@array", vm_builtin_SplFileObject_fstat },` |
|     - | 13436 | `		{ "ftruncate",     PH7_MOD_PUBLIC, "int $size", "@bool",` |
|     - | 13437 | `		  vm_builtin_SplFileObject_ftruncate },` |
|     - | 13438 | `		{ "current",       PH7_MOD_PUBLIC, "", "@array\|string\|false",` |
|     - | 13439 | `		  vm_builtin_SplFileObject_current },` |
|     - | 13440 | `		{ "key",           PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplFileObject_key },` |
|     - | 13441 | `		{ "next",          PH7_MOD_PUBLIC, "", "@void", vm_builtin_SplFileObject_next },` |
|     - | 13442 | `		{ "setFlags",      PH7_MOD_PUBLIC, "int $flags", "@void",` |
|     - | 13443 | `		  vm_builtin_SplFileObject_setFlags },` |
|     - | 13444 | `		{ "getFlags",      PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplFileObject_getFlags },` |
|     - | 13445 | `		{ "setMaxLineLen", PH7_MOD_PUBLIC, "int $maxLength", "@void",` |
|     - | 13446 | `		  vm_builtin_SplFileObject_setMaxLineLen },` |
|     - | 13447 | `		{ "getMaxLineLen", PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplFileObject_getMaxLineLen },` |
|     - | 13448 | `		/* php types the two it answers CONSTANTLY with the constant itself:` |
|     - | 13449 | ``		 * `false` and `null`, not `bool` and `?RecursiveIterator`. A file has no`` |
|     - | 13450 | `		 * children, and both bodies say so on every path. */` |
|     - | 13451 | `		{ "hasChildren",   PH7_MOD_PUBLIC, "", "@false", vm_builtin_SplFileObject_hasChildren },` |
|     - | 13452 | `		{ "getChildren",   PH7_MOD_PUBLIC, "", "@null",` |
|     - | 13453 | `		  vm_builtin_SplFileObject_getChildren },` |
|     - | 13454 | `		{ "seek",          PH7_MOD_PUBLIC, "int $line", "@void", vm_builtin_SplFileObject_seek },` |
|     - | 13455 | `		/* php aliases getCurrentLine() to fgets(); the override branch in` |
|     - | 13456 | `		 * SfoReadLineEx() is what makes REPLACING it mean something. */` |
|     - | 13457 | `		{ "getCurrentLine",PH7_MOD_PUBLIC, "", "@string", vm_builtin_SplFileObject_fgets },` |
|     - | 13458 | `		{ "__toString",    PH7_MOD_PUBLIC, "", "string", vm_builtin_SplFileObject_toString },` |
|     - | 13459 | `	};` |
|     - | 13460 | `	static const PH7_NativeMethodDef aTempMethod[] = {` |
|     - | 13461 | `		{ "__construct", PH7_MOD_PUBLIC, "int $maxMemory = 2 * 1024 * 1024", 0,` |
|     - | 13462 | `		  vm_builtin_SplTempFileObject_construct },` |
|     - | 13463 | `	};` |
|     - | 13464 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - | 13465 | `		{ "SplFileObject", "SplFileInfo", "RecursiveIterator,SeekableIterator",` |
|     - | 13466 | `		  PH7_CLASS_NOSERIALIZE\|PH7_CLASS_NOCLONE,` |
|     - | 13467 | `		  aFileMethod, SX_ARRAYSIZE(aFileMethod), aFileConst, SX_ARRAYSIZE(aFileConst),` |
|     - | 13468 | `		  aFileProp, SX_ARRAYSIZE(aFileProp), SfoRelease, 0, SfiPresent },` |
|     - | 13469 | `		/* php gives SplTempFileObject no handlers of its own, so it INHERITS the` |
|     - | 13470 | `		 * check pair -- uncloneable, and every method refused on an instance` |
|     - | 13471 | `		 * whose parent constructor never ran. A native class here inherits` |
|     - | 13472 | `		 * neither the refusals nor the hooks (rule 29), so both are restated. */` |
|     - | 13473 | `		{ "SplTempFileObject", "SplFileObject", 0,` |
|     - | 13474 | `		  PH7_CLASS_NOSERIALIZE\|PH7_CLASS_NOCLONE,` |
|     - | 13475 | `		  aTempMethod, SX_ARRAYSIZE(aTempMethod), 0, 0,` |
|     - | 13476 | `		  0, 0, SfoRelease, 0, SfiPresent },` |
|     - | 13477 | `	};` |
|  8450 | 13478 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|     5 | 13479 | `}` |
|  8445 | 13480 | `PH7_PRIVATE sxi32 PH7_VmInstallSpl(ph7_vm *pVm)` |
|     5 | 13481 | `{` |
|  8450 | 13482 | `	sxi32 rc = VmInstallWeak(&(*pVm));` |
|  8450 | 13483 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 13484 | `		return rc;` |
|     - | 13485 | `	}` |
|     - | 13486 | `	/* Ordering, now that zSplLib is gone: the remaining PHP in this subsystem is` |
|     - | 13487 | `	 * the tokenizer chunk's, so these only have to satisfy each OTHER. */` |
|  8450 | 13488 | `	rc = VmInstallSplStore(&(*pVm));` |
|  8450 | 13489 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 13490 | `		return rc;` |
|     - | 13491 | `	}` |
|  8450 | 13492 | `	rc = VmInstallSplDualIterators(&(*pVm));` |
|  8450 | 13493 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 13494 | `		return rc;` |
|     - | 13495 | `	}` |
|     - | 13496 | `	/* After the dual iterators: RecursiveIteratorIterator names OuterIterator and` |
|     - | 13497 | `	 * RecursiveIterator, both declared by that table. */` |
|  8450 | 13498 | `	rc = VmInstallSplRecursiveIt(&(*pVm));` |
|  8450 | 13499 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 13500 | `		return rc;` |
|     - | 13501 | `	}` |
|  8450 | 13502 | `	rc = VmInstallSplDllist(&(*pVm));` |
|  8450 | 13503 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 13504 | `		return rc;` |
|     - | 13505 | `	}` |
|  8450 | 13506 | `	rc = VmInstallSplHeap(&(*pVm));` |
|  8450 | 13507 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 13508 | `		return rc;` |
|     - | 13509 | `	}` |
|  8450 | 13510 | `	rc = VmInstallSplFixedArray(&(*pVm));` |
|  8450 | 13511 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 13512 | `		return rc;` |
|     - | 13513 | `	}` |
|  8450 | 13514 | `	rc = VmInstallSplObjectStorage(&(*pVm));` |
|  8450 | 13515 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 13516 | `		return rc;` |
|     - | 13517 | `	}` |
|     - | 13518 | `	/* After SplObjectStorage: MultipleIterator holds the same storage and reaches` |
|     - | 13519 | `	 * its attach/detach/debug routines. */` |
|  8450 | 13520 | `	rc = VmInstallSplMultipleIterator(&(*pVm));` |
|  8450 | 13521 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 13522 | `		return rc;` |
|     - | 13523 | `	}` |
|  8450 | 13524 | `	rc = VmInstallSplFileInfo(&(*pVm));` |
|  8450 | 13525 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 13526 | `		return rc;` |
|     - | 13527 | `	}` |
|     - | 13528 | `	/* After SplFileInfo: DirectoryIterator extends it, and PH7_ClassInherit copies` |
|     - | 13529 | `	 * the base's methods DOWN (rule 14). */` |
|  8450 | 13530 | `	rc = VmInstallSplDirIterators(&(*pVm));` |
|  8450 | 13531 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 13532 | `		return rc;` |
|     - | 13533 | `	}` |
|     - | 13534 | `	/* After SplFileInfo for the same reason; the dir iterators are ahead of it` |
|     - | 13535 | `	 * only because they are declared together. */` |
|  8450 | 13536 | `	return VmInstallSplFileObject(&(*pVm));` |
|  4222 | 13537 | `}` |
|     - | 13538 |  |
|     - | 13539 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     - | 13540 |  |
|     - | 13541 | `#ifdef PH7_DISABLE_BUILTIN_FUNC` |
|     - | 13542 | `/* Tiny build: no SPL (builtin layer disabled) */` |
|     - | 13543 | `PH7_PRIVATE sxi32 PH7_VmInstallSpl(ph7_vm *pVm){ (void)pVm; return SXRET_OK; }` |
|     - | 13544 | `/* No directory iterators either, so shutdown has nothing to close. */` |
|     - | 13545 | `PH7_PRIVATE void PH7_SplDirVmRelease(ph7_vm *pVm){ (void)pVm; }` |
|     - | 13546 | `/* The writable-container fast path is called unconditionally by OP_LOAD_IDX, and its` |
|     - | 13547 | ` * SXU32_HIGH answer already means "no slot available — take the ordinary offsetGet` |
|     - | 13548 | ` * dispatch". With no SPL classes in this build that is the only answer there is, so the` |
|     - | 13549 | ` * stub keeps the tiny target LINKING without a second #ifdef at the call site. */` |
|     - | 13550 | `PH7_PRIVATE sxu32 PH7_SplDimElemSlot(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pKey,int bCreate)` |
|     - | 13551 | `{` |
|     - | 13552 | `	(void)pVm; (void)pThis; (void)pKey; (void)bCreate;` |
|     - | 13553 | `	return SXU32_HIGH;` |
|     - | 13554 | `}` |
|     - | 13555 | `/* Same reasoning for the dual-iterator method forward: OP_MEMBER asks it on every` |
|     - | 13556 | ` * missing method, and with no SPL classes in this build the answer is always "not` |
|     - | 13557 | ` * one of mine". */` |
|     - | 13558 | `PH7_PRIVATE int PH7_SplOuterForward(ph7_vm *pVm,ph7_class_instance *pThis,SyString *pName,` |
|     - | 13559 | `	ph7_class_instance **ppInner,ph7_class_method **ppMeth)` |
|     - | 13560 | `{` |
|     - | 13561 | `	(void)pVm; (void)pThis; (void)pName; (void)ppInner; (void)ppMeth;` |
|     - | 13562 | `	return 0;` |
|     - | 13563 | `}` |
|     - | 13564 | `#endif` |
|     - | 13565 |  |
