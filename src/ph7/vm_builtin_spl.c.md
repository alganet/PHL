# src/ph7/vm_builtin_spl.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 5308/5962 lines (89.03%)

[Root index](../../index.md) | [Directory index](index.md)

|  Hits | Line | Source |
| ----: | ---: | :--- |
|     - |    1 | `/**` |
|     - |    2 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|     - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|     - |    4 | ` */` |
|     - |    5 | `#include "ph7int.h"` |
|     - |    6 | `#include <errno.h>   /* getLinkTarget names the errno text its readlink failed with */` |
|     - |    7 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|     - |    8 | `/*` |
|     - |    9 | ` * SPL iterators, slice 1 (NEWPLAN band D): SeekableIterator, ArrayIterator,` |
|     - |   10 | ` * ArrayObject. (natsort()/natcasesort() used to be declared here as prelude` |
|     - |   11 | ` * wrappers over uasort(...,'strnatcmp'); they are C builtins in hashmap_sort.c` |
|     - |   12 | ` * now -- see ph7_hashmap_natsort -- and the methods below delegate to them.)` |
|     - |   13 | ` * Embedded-PHP chunk following the Reflection architecture — installed` |
|     - |   14 | ` * inside the bCompilingBuiltin window, backed by the engine's native array` |
|     - |   15 | ` * internal-pointer builtins (reset/next/key/current keep their position on a` |
|     - |   16 | ` * property, so ArrayIterator's cursor IS the backing array's pointer).` |
|     - |   17 | ` */` |
|     - |   18 |  |
|     - |   19 | `/*` |
|     - |   20 | ` * ---------------------------------------------------------------------------` |
|     - |   21 | ` * The weak-reference family: WeakReference and WeakMap, declared and bodied in C.` |
|     - |   22 | ` *` |
|     - |   23 | `` * They used to be PHP classes over three global thunks (`__weak_create()`,`` |
|     - |   24 | `` * `__weak_get()`, `__weak_drop()`) that traded the cell pointer back and forth as`` |
|     - |   25 | ` * an opaque int. The cell never belonged in PHP: it is a C lifetime, and the moment` |
|     - |   26 | ` * a class can have C METHOD bodies the thunks are just the methods, spelled with` |
|     - |   27 | `` * the pointer left in the open. They are gone; `__h` is the only slot left, and it`` |
|     - |   28 | ` * is private to a final, uncloneable class.` |
|     - |   29 | ` * ---------------------------------------------------------------------------` |
|     - |   30 | ` */` |
|     - |   31 | `/* The shared cell for a target, created on first use. Takes ONE handle. */` |
|    72 |   32 | `static VmWeakCell * WkCellFor(ph7_vm *pVm,ph7_class_instance *pObj)` |
|     2 |   33 | `{` |
|    74 |   34 | `	SyHashEntry *pEntry = SyHashGet(&pVm->hWeakCell,(const void *)&pObj,sizeof(void *));` |
|     - |   35 | `	VmWeakCell *pCell;` |
|    74 |   36 | `	if( pEntry ){` |
|    20 |   37 | `		pCell = (VmWeakCell *)pEntry->pUserData;` |
|    20 |   38 | `		pCell->nRef++;` |
|    20 |   39 | `		return pCell;` |
|     - |   40 | `	}` |
|    56 |   41 | `	pCell = (VmWeakCell *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmWeakCell));` |
|    56 |   42 | `	if( pCell == 0 ){` |
|   ! 0 |   43 | `		return 0;` |
|     - |   44 | `	}` |
|    56 |   45 | `	pCell->pObj = pObj;` |
|    56 |   46 | `	pCell->pRef = 0;` |
|    56 |   47 | `	pCell->nRef = 1;` |
|     - |   48 | `	/* SyHash stores the key POINTER (no copy): key off the cell's own pObj field —` |
|     - |   49 | `	 * heap-stable for the entry's whole lifetime, and it holds the live pointer` |
|     - |   50 | `	 * bytes until the release hook nulls it (which happens only after the entry` |
|     - |   51 | `	 * is deleted). */` |
|    56 |   52 | `	if( SyHashInsert(&pVm->hWeakCell,(const void *)&pCell->pObj,sizeof(void *),pCell) != SXRET_OK ){` |
|   ! 0 |   53 | `		SyMemBackendFree(&pVm->sAllocator,pCell);` |
|   ! 0 |   54 | `		return 0;` |
|     - |   55 | `	}` |
|    56 |   56 | `	return pCell;` |
|    38 |   57 | `}` |
|     - |   58 | `/* Give one handle back; the last one frees the cell. */` |
|    56 |   59 | `static void WkCellDrop(ph7_vm *pVm,VmWeakCell *pCell)` |
|     2 |   60 | `{` |
|    58 |   61 | `	if( pCell == 0 \|\| pCell->nRef == 0 ){` |
|   ! 0 |   62 | `		return;` |
|     - |   63 | `	}` |
|    58 |   64 | `	pCell->nRef--;` |
|    58 |   65 | `	if( pCell->nRef == 0 ){` |
|    40 |   66 | `		if( pCell->pObj ){` |
|     - |   67 | `			/* Target still alive: unhook the registry entry before freeing. */` |
|    14 |   68 | `			void *pDummy = 0;` |
|    14 |   69 | `			SyHashDeleteEntry(&pVm->hWeakCell,(const void *)&pCell->pObj,sizeof(void *),&pDummy);` |
|     6 |   70 | `		}` |
|    40 |   71 | `		SyMemBackendFree(&pVm->sAllocator,pCell);` |
|    19 |   72 | `	}` |
|    30 |   73 | `}` |
|     - |   74 | `/* The cell a WeakReference instance holds, or NULL. */` |
|   204 |   75 | `static VmWeakCell * WkCellOf(ph7_class_instance *pRef)` |
|     2 |   76 | `{` |
|   206 |   77 | `	return (VmWeakCell *)(sxuptr)(sxu64)PH7_NativeAttrInt(pRef,"__h");` |
|     2 |   78 | `}` |
|     - |   79 | `/* Hand an instance back without owning a reference of our own (ph7_result_value's` |
|     - |   80 | ` * MemObjStore takes the one the result needs). */` |
|    96 |   81 | `static void SplResultBorrowed(ph7_context *pCtx,ph7_class_instance *pObj)` |
|     2 |   82 | `{` |
|     - |   83 | `	ph7_value sObj;` |
|    98 |   84 | `	PH7_MemObjInit(pCtx->pVm,&sObj);` |
|    98 |   85 | `	sObj.x.pOther = pObj;` |
|    98 |   86 | `	MemObjSetType(&sObj,MEMOBJ_OBJ);` |
|    98 |   87 | `	ph7_result_value(pCtx,&sObj);` |
|    98 |   88 | `}` |
|     - |   89 | `/*` |
|     - |   90 | ` * The ONE WeakReference published for a target, created on first ask.` |
|     - |   91 | ` *` |
|     - |   92 | ` * php answers the same object for the same target every time, so the cell caches` |
|     - |   93 | `` * what it handed out. `*pbOwned` says whether the caller holds the fresh instance's`` |
|     - |   94 | ` * reference (and so must give it up once it has been stored somewhere) or is merely` |
|     - |   95 | ` * borrowing the published one.` |
|     - |   96 | ` */` |
|    72 |   97 | `static ph7_class_instance * WkRefFor(ph7_vm *pVm,ph7_class_instance *pObj,int *pbOwned)` |
|     2 |   98 | `{` |
|    74 |   99 | `	VmWeakCell *pCell = WkCellFor(pVm,pObj);` |
|     - |  100 | `	ph7_class *pClass;` |
|     - |  101 | `	ph7_class_instance *pRef;` |
|    74 |  102 | `	*pbOwned = 0;` |
|    74 |  103 | `	if( pCell == 0 ){` |
|   ! 0 |  104 | `		return 0;` |
|     - |  105 | `	}` |
|    74 |  106 | `	if( pCell->pRef ){` |
|     - |  107 | `		/* Give back the handle WkCellFor just took: the publication owns the only one. */` |
|    20 |  108 | `		WkCellDrop(pVm,pCell);` |
|    20 |  109 | `		return pCell->pRef;` |
|     - |  110 | `	}` |
|     - |  111 | ``	/* Built directly rather than through `new`, whose constructor exists only to`` |
|     - |  112 | `	 * refuse (below). */` |
|    56 |  113 | `	pClass = PH7_VmExtractClass(pVm,"WeakReference",sizeof("WeakReference")-1,FALSE,0);` |
|    56 |  114 | `	pRef = pClass ? PH7_NewClassInstance(pVm,pClass) : 0;` |
|    56 |  115 | `	if( pRef == 0 ){` |
|   ! 0 |  116 | `		WkCellDrop(pVm,pCell);` |
|   ! 0 |  117 | `		return 0;` |
|     - |  118 | `	}` |
|    56 |  119 | `	PH7_NativeSetAttrInt(pVm,pRef,"__h",(sxi64)(sxu64)(sxuptr)pCell);` |
|    56 |  120 | `	pCell->pRef = pRef;` |
|    56 |  121 | `	*pbOwned = 1;` |
|    56 |  122 | `	return pRef;` |
|    38 |  123 | `}` |
|     - |  124 | `/* WeakReference::create(object $object): WeakReference */` |
|    34 |  125 | `static int vm_builtin_WeakReference_create(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  126 | `{` |
|     - |  127 | `	ph7_class_instance *pRef;` |
|    36 |  128 | `	int bOwned = 0;` |
|    36 |  129 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|   ! 0 |  130 | `		ph7_result_null(pCtx);` |
|   ! 0 |  131 | `		return PH7_OK;` |
|     - |  132 | `	}` |
|    36 |  133 | `	pRef = WkRefFor(pCtx->pVm,(ph7_class_instance *)apArg[0]->x.pOther,&bOwned);` |
|    36 |  134 | `	if( pRef == 0 ){` |
|   ! 0 |  135 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  136 | `	}` |
|    36 |  137 | `	if( bOwned ){` |
|    28 |  138 | `		PH7_NativeResultObject(pCtx,pRef);` |
|    15 |  139 | `	}else{` |
|    10 |  140 | `		SplResultBorrowed(pCtx,pRef);` |
|     - |  141 | `	}` |
|    36 |  142 | `	return PH7_OK;` |
|    19 |  143 | `}` |
|     - |  144 | `/* WeakReference::get(): ?object — the target, or null once it has died. */` |
|    12 |  145 | `static int vm_builtin_WeakReference_get(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  146 | `{` |
|    14 |  147 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    14 |  148 | `	VmWeakCell *pCell = pThis ? WkCellOf(pThis) : 0;` |
|     6 |  149 | `	SXUNUSED(nArg);` |
|     6 |  150 | `	SXUNUSED(apArg);` |
|    14 |  151 | `	if( pCell == 0 \|\| pCell->pObj == 0 ){` |
|    10 |  152 | `		ph7_result_null(pCtx);` |
|    10 |  153 | `		return PH7_OK;` |
|     - |  154 | `	}` |
|     5 |  155 | `	SplResultBorrowed(pCtx,pCell->pObj);` |
|     5 |  156 | `	return PH7_OK;` |
|     8 |  157 | `}` |
|     - |  158 | `/*` |
|     - |  159 | ` * php's DEBUG presentation for a WeakReference (ph7_class::xPresent).` |
|     - |  160 | ` *` |
|     - |  161 | ` * var_dump/print_r show ["object"] => the target, or NULL once it has died. The` |
|     - |  162 | ` * (array) cast and var_export show NOTHING — php's get_debug_info and` |
|     - |  163 | ` * get_properties disagree here, which is why the callback is told which is asking.` |
|     - |  164 | ` */` |
|    16 |  165 | `static sxi32 WkPresent(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)` |
|     1 |  166 | `{` |
|     - |  167 | `	VmWeakCell *pCell;` |
|     - |  168 | `	ph7_value sKey, sVal;` |
|    17 |  169 | `	if( !bDebug ){` |
|    13 |  170 | `		return SXRET_OK; /* get_properties: php presents no property at all */` |
|     - |  171 | `	}` |
|     5 |  172 | `	pCell = WkCellOf(pThis);` |
|     5 |  173 | `	PH7_MemObjInitFromString(&(*pVm),&sKey,0);` |
|     5 |  174 | `	PH7_MemObjStringAppend(&sKey,"object",sizeof("object")-1);` |
|     5 |  175 | `	PH7_MemObjInit(&(*pVm),&sVal);` |
|     5 |  176 | `	if( pCell && pCell->pObj ){` |
|     5 |  177 | `		sVal.x.pOther = pCell->pObj;` |
|     5 |  178 | `		MemObjSetType(&sVal,MEMOBJ_OBJ);` |
|     2 |  179 | `	}` |
|     5 |  180 | `	ph7_array_add_elem(pOut,&sKey,&sVal); /* takes its OWN reference */` |
|     5 |  181 | `	PH7_MemObjRelease(&sKey);` |
|     - |  182 | `	/* Rule 16, the other half: this carrier never HELD a reference — the cell's` |
|     - |  183 | `	 * is the only one — so releasing it as a MEMOBJ_OBJ would unref the target a` |
|     - |  184 | `	 * second time and free a live object under its owner (a segfault two` |
|     - |  185 | `	 * statements later, not here). Blank the carrier before letting it go. */` |
|     5 |  186 | `	sVal.x.pOther = 0;` |
|     5 |  187 | `	sVal.iFlags = MEMOBJ_NULL;` |
|     5 |  188 | `	PH7_MemObjRelease(&sVal);` |
|     5 |  189 | `	return SXRET_OK;` |
|     9 |  190 | `}` |
|     - |  191 | `/*` |
|     - |  192 | ` * WeakReference::__construct()` |
|     - |  193 | ` *` |
|     - |  194 | ` * php declares it PUBLIC and refuses to run it: the class has no way to be built` |
|     - |  195 | ` * except through create(), and the refusal is an Error rather than a visibility` |
|     - |  196 | `` * failure, so `(new ReflectionClass('WeakReference'))->newInstance()` says the same`` |
|     - |  197 | `` * thing `new WeakReference()` does.`` |
|     - |  198 | ` */` |
|     4 |  199 | `static int vm_builtin_WeakReference_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  200 | `{` |
|     2 |  201 | `	SXUNUSED(nArg);` |
|     2 |  202 | `	SXUNUSED(apArg);` |
|     5 |  203 | `	return PH7_VmThrowException(pCtx,"Error",` |
|     - |  204 | `		"Direct instantiation of WeakReference is not allowed, use WeakReference::create instead");` |
|     1 |  205 | `}` |
|     - |  206 | `/* The handle dies with the instance. This is xRelease, not __destruct: php's` |
|     - |  207 | ` * WeakReference declares no destructor and Reflection must not grow one. */` |
|    42 |  208 | `static void WkRefRelease(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     2 |  209 | `{` |
|    44 |  210 | `	VmWeakCell *pCell = WkCellOf(pThis);` |
|    44 |  211 | `	if( pCell == 0 ){` |
|     5 |  212 | `		return;` |
|     - |  213 | `	}` |
|    40 |  214 | `	if( pCell->pRef == pThis ){` |
|    40 |  215 | `		pCell->pRef = 0;   /* stop publishing an object that is going away */` |
|    19 |  216 | `	}` |
|    40 |  217 | `	PH7_NativeSetAttrInt(pVm,pThis,"__h",0);` |
|    40 |  218 | `	WkCellDrop(pVm,pCell);` |
|    23 |  219 | `}` |
|     - |  220 | `/*` |
|     - |  221 | ` * WeakMap.` |
|     - |  222 | ` *` |
|     - |  223 | `` * Two private arrays keyed by the target's object id (`spl_object_id`, which this`` |
|     - |  224 | ` * engine never reuses): __r holds the WeakReference for the key, __v the value` |
|     - |  225 | ` * mapped to it. Building the weakness out of WeakReference rather than out of raw` |
|     - |  226 | `` * cells is what makes `clone $map` right for free -- the copied array increments`` |
|     - |  227 | ` * each WeakReference's own reference count, and no cell is dropped twice.` |
|     - |  228 | ` */` |
|     - |  229 | `#define WM_REFS "__r"` |
|     - |  230 | `#define WM_VALS "__v"` |
|     - |  231 | `/* One of the two backing arrays, materialized and separated from any copy that` |
|     - |  232 | ` * shares it (a cloned WeakMap starts out sharing both). */` |
|   366 |  233 | `static ph7_hashmap * WmStore(ph7_vm *pVm,ph7_class_instance *pWm,const char *zSlot)` |
|     2 |  234 | `{` |
|   368 |  235 | `	ph7_value *pSlot = PH7_NativeAttr(pWm,zSlot);` |
|   368 |  236 | `	if( pSlot == 0 ){` |
|   ! 0 |  237 | `		return 0;` |
|     - |  238 | `	}` |
|   368 |  239 | `	if( (pSlot->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    50 |  240 | `		if( PH7_MemObjToHashmap(pSlot) != SXRET_OK ){` |
|   ! 0 |  241 | `			return 0;` |
|     - |  242 | `		}` |
|    24 |  243 | `	}` |
|   368 |  244 | `	return PH7_HashmapCowSeparate(pVm,pSlot);` |
|   185 |  245 | `}` |
|     - |  246 | `/* The entry for an object id, or NULL. */` |
|   178 |  247 | `static ph7_hashmap_node * WmFind(ph7_vm *pVm,ph7_hashmap *pMap,sxi64 iId)` |
|     2 |  248 | `{` |
|     - |  249 | `	ph7_value sKey;` |
|   180 |  250 | `	ph7_hashmap_node *pNode = 0;` |
|   180 |  251 | `	if( pMap == 0 ){` |
|   ! 0 |  252 | `		return 0;` |
|     - |  253 | `	}` |
|   180 |  254 | `	PH7_MemObjInitFromInt(pVm,&sKey,iId);` |
|   180 |  255 | `	if( PH7_HashmapLookup(pMap,&sKey,&pNode) != SXRET_OK ){` |
|    52 |  256 | `		pNode = 0;` |
|    25 |  257 | `	}` |
|   180 |  258 | `	PH7_MemObjRelease(&sKey);` |
|   180 |  259 | `	return pNode;` |
|    91 |  260 | `}` |
|    76 |  261 | `static void WmPut(ph7_vm *pVm,ph7_hashmap *pMap,sxi64 iId,ph7_value *pVal)` |
|     2 |  262 | `{` |
|     - |  263 | `	ph7_value sKey;` |
|    78 |  264 | `	if( pMap == 0 ){` |
|   ! 0 |  265 | `		return;` |
|     - |  266 | `	}` |
|    78 |  267 | `	PH7_MemObjInitFromInt(pVm,&sKey,iId);` |
|    78 |  268 | `	PH7_HashmapInsert(pMap,&sKey,pVal);` |
|    78 |  269 | `	PH7_MemObjRelease(&sKey);` |
|    40 |  270 | `}` |
|    22 |  271 | `static void WmErase(ph7_vm *pVm,ph7_hashmap *pMap,sxi64 iId)` |
|     2 |  272 | `{` |
|    24 |  273 | `	ph7_hashmap_node *pNode = WmFind(pVm,pMap,iId);` |
|    24 |  274 | `	if( pNode ){` |
|    20 |  275 | `		PH7_HashmapUnlinkNode(pNode,TRUE);` |
|     9 |  276 | `	}` |
|    24 |  277 | `}` |
|     - |  278 | `/* The target a __r entry still points at, or NULL once it has died. */` |
|   154 |  279 | `static ph7_class_instance * WmNodeTarget(ph7_hashmap_node *pNode)` |
|     2 |  280 | `{` |
|   156 |  281 | `	ph7_value *pVal = pNode ? HashmapExtractNodeValue(pNode) : 0;` |
|     - |  282 | `	VmWeakCell *pCell;` |
|   156 |  283 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     9 |  284 | `		return 0;` |
|     - |  285 | `	}` |
|   148 |  286 | `	pCell = WkCellOf((ph7_class_instance *)pVal->x.pOther);` |
|   148 |  287 | `	return pCell ? pCell->pObj : 0;` |
|    79 |  288 | `}` |
|     - |  289 | `/* Forget every entry whose key has died. php prunes on count() and on iteration,` |
|     - |  290 | ` * which is the only reason a WeakMap's size ever changes on its own. */` |
|    52 |  291 | `static void WmPrune(ph7_vm *pVm,ph7_class_instance *pWm)` |
|     2 |  292 | `{` |
|    54 |  293 | `	ph7_hashmap *pRefs = WmStore(pVm,pWm,WM_REFS);` |
|    54 |  294 | `	ph7_hashmap *pVals = WmStore(pVm,pWm,WM_VALS);` |
|     - |  295 | `	ph7_hashmap_node *pNode,*pNext;` |
|    54 |  296 | `	if( pRefs == 0 ){` |
|   ! 0 |  297 | `		return;` |
|     - |  298 | `	}` |
|     - |  299 | `	/* pFirst then the pPrev chain IS insertion order here: MACRO_LD_PUSH links a` |
|     - |  300 | `	 * new node in through pNext, so pNext points at the OLDER neighbour. */` |
|   136 |  301 | `	for( pNode = pRefs->pFirst ; pNode ; pNode = pNext ){` |
|    84 |  302 | `		pNext = pNode->pPrev;` |
|    84 |  303 | `		if( WmNodeTarget(pNode) == 0 ){` |
|     8 |  304 | `			sxi64 iId = pNode->xKey.iKey;` |
|     8 |  305 | `			PH7_HashmapUnlinkNode(pNode,TRUE);` |
|     8 |  306 | `			WmErase(pVm,pVals,iId);` |
|     3 |  307 | `		}` |
|    43 |  308 | `	}` |
|    28 |  309 | `}` |
|     - |  310 | `/* Every WeakMap entry point but count() takes an object key and says so the same` |
|     - |  311 | ` * way php does. Answers the id, or -1 after raising the TypeError. */` |
|    54 |  312 | `static sxi64 WmKeyId(ph7_context *pCtx,int nArg,ph7_value **apArg,ph7_class_instance **ppObj)` |
|     2 |  313 | `{` |
|    56 |  314 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     3 |  315 | `		PH7_VmThrowException(pCtx,"TypeError","WeakMap key must be an object");` |
|     3 |  316 | `		return -1;` |
|     - |  317 | `	}` |
|    54 |  318 | `	*ppObj = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    54 |  319 | `	return (sxi64)(*ppObj)->nObjId;` |
|    29 |  320 | `}` |
|    40 |  321 | `static int vm_builtin_WeakMap_offsetSet(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  322 | `{` |
|    42 |  323 | `	ph7_vm *pVm = pCtx->pVm;` |
|    42 |  324 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    42 |  325 | `	ph7_class_instance *pObj = 0;` |
|     - |  326 | `	ph7_hashmap *pRefs,*pVals;` |
|    42 |  327 | `	sxi64 iId = WmKeyId(pCtx,nArg,apArg,&pObj);` |
|    42 |  328 | `	if( pThis == 0 \|\| iId < 0 ){` |
|     3 |  329 | `		return PH7_OK;` |
|     - |  330 | `	}` |
|    40 |  331 | `	pRefs = WmStore(pVm,pThis,WM_REFS);` |
|    40 |  332 | `	pVals = WmStore(pVm,pThis,WM_VALS);` |
|    40 |  333 | `	if( WmFind(pVm,pRefs,iId) == 0 ){` |
|     - |  334 | `		/* First value for this key: hold it through the very WeakReference` |
|     - |  335 | `		 * create() publishes, so the map and userland share one cell. */` |
|    40 |  336 | `		int bOwned = 0;` |
|    40 |  337 | `		ph7_class_instance *pRef = WkRefFor(pVm,pObj,&bOwned);` |
|     - |  338 | `		ph7_value sRef;` |
|    40 |  339 | `		if( pRef == 0 ){` |
|   ! 0 |  340 | `			return PH7_ContextMemoryError(pCtx);` |
|     - |  341 | `		}` |
|    40 |  342 | `		PH7_MemObjInit(pVm,&sRef);` |
|    40 |  343 | `		sRef.x.pOther = pRef;` |
|    40 |  344 | `		MemObjSetType(&sRef,MEMOBJ_OBJ);` |
|    40 |  345 | `		WmPut(pVm,pRefs,iId,&sRef);   /* takes a reference of its own */` |
|    40 |  346 | `		if( bOwned ){` |
|    30 |  347 | `			PH7_ClassInstanceUnref(pRef);` |
|    14 |  348 | `		}` |
|    19 |  349 | `	}` |
|    40 |  350 | `	WmPut(pVm,pVals,iId,nArg > 1 ? apArg[1] : 0);` |
|    40 |  351 | `	return PH7_OK;` |
|    22 |  352 | `}` |
|     4 |  353 | `static int vm_builtin_WeakMap_offsetGet(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  354 | `{` |
|     5 |  355 | `	ph7_vm *pVm = pCtx->pVm;` |
|     5 |  356 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     5 |  357 | `	ph7_class_instance *pObj = 0;` |
|     - |  358 | `	ph7_hashmap_node *pNode;` |
|     5 |  359 | `	sxi64 iId = WmKeyId(pCtx,nArg,apArg,&pObj);` |
|     5 |  360 | `	if( pThis == 0 \|\| iId < 0 ){` |
|   ! 0 |  361 | `		return PH7_OK;` |
|     - |  362 | `	}` |
|     5 |  363 | `	if( WmNodeTarget(WmFind(pVm,WmStore(pVm,pThis,WM_REFS),iId)) != pObj ){` |
|     7 |  364 | `		return PH7_VmThrowException(pCtx,"Error","Object %z#%d not contained in WeakMap",` |
|     4 |  365 | `			&pObj->pClass->sName,(int)pObj->nObjId);` |
|     - |  366 | `	}` |
|   ! 0 |  367 | `	pNode = WmFind(pVm,WmStore(pVm,pThis,WM_VALS),iId);` |
|   ! 0 |  368 | `	if( pNode ){` |
|   ! 0 |  369 | `		ph7_result_value(pCtx,HashmapExtractNodeValue(pNode));` |
|   ! 0 |  370 | `	}` |
|   ! 0 |  371 | `	return PH7_OK;` |
|     3 |  372 | `}` |
|     2 |  373 | `static int vm_builtin_WeakMap_offsetExists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  374 | `{` |
|     3 |  375 | `	ph7_vm *pVm = pCtx->pVm;` |
|     3 |  376 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     3 |  377 | `	ph7_class_instance *pObj = 0;` |
|     3 |  378 | `	sxi64 iId = WmKeyId(pCtx,nArg,apArg,&pObj);` |
|     3 |  379 | `	if( pThis == 0 \|\| iId < 0 ){` |
|   ! 0 |  380 | `		return PH7_OK;` |
|     - |  381 | `	}` |
|     3 |  382 | `	ph7_result_bool(pCtx,WmNodeTarget(WmFind(pVm,WmStore(pVm,pThis,WM_REFS),iId)) == pObj);` |
|     3 |  383 | `	return PH7_OK;` |
|     2 |  384 | `}` |
|     8 |  385 | `static int vm_builtin_WeakMap_offsetUnset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  386 | `{` |
|    10 |  387 | `	ph7_vm *pVm = pCtx->pVm;` |
|    10 |  388 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    10 |  389 | `	ph7_class_instance *pObj = 0;` |
|    10 |  390 | `	sxi64 iId = WmKeyId(pCtx,nArg,apArg,&pObj);` |
|    10 |  391 | `	if( pThis == 0 \|\| iId < 0 ){` |
|   ! 0 |  392 | `		return PH7_OK;` |
|     - |  393 | `	}` |
|    10 |  394 | `	WmErase(pVm,WmStore(pVm,pThis,WM_REFS),iId);` |
|    10 |  395 | `	WmErase(pVm,WmStore(pVm,pThis,WM_VALS),iId);` |
|    10 |  396 | `	return PH7_OK;` |
|     6 |  397 | `}` |
|    18 |  398 | `static int vm_builtin_WeakMap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  399 | `{` |
|    20 |  400 | `	ph7_vm *pVm = pCtx->pVm;` |
|    20 |  401 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  402 | `	ph7_hashmap *pRefs;` |
|     9 |  403 | `	SXUNUSED(nArg);` |
|     9 |  404 | `	SXUNUSED(apArg);` |
|    20 |  405 | `	if( pThis == 0 ){` |
|   ! 0 |  406 | `		ph7_result_int(pCtx,0);` |
|   ! 0 |  407 | `		return PH7_OK;` |
|     - |  408 | `	}` |
|    20 |  409 | `	WmPrune(pVm,pThis);` |
|    20 |  410 | `	pRefs = WmStore(pVm,pThis,WM_REFS);` |
|    20 |  411 | `	ph7_result_int64(pCtx,pRefs ? (ph7_int64)pRefs->nEntry : 0);` |
|    20 |  412 | `	return PH7_OK;` |
|    11 |  413 | `}` |
|     - |  414 | `/*` |
|     - |  415 | ` * The WeakMap walk, as the vtable an InternalIterator drives.` |
|     - |  416 | ` *` |
|     - |  417 | ` * The cursor is the object id it sits on (POS), plus the id it expects to move to` |
|     - |  418 | ` * (AUX). Both are looked up fresh at every step, which is what makes the walk LIVE:` |
|     - |  419 | ` * an entry added during a foreach is reached (it is the current node's new pNext),` |
|     - |  420 | ` * one removed ahead of the cursor is skipped, and removing the CURRENT entry -- the` |
|     - |  421 | `` * `foreach($m as $k=>$v) unset($m[$k]);` idiom -- still lands on the successor AUX`` |
|     - |  422 | ` * recorded when the cursor settled.` |
|     - |  423 | ` */` |
|    60 |  424 | `static void WmSettle(ph7_vm *pVm,ph7_class_instance *pIt,ph7_hashmap_node *pNode)` |
|     2 |  425 | `{` |
|    62 |  426 | `	ph7_class_instance *pWm = PH7_NativeAttrObj(pIt,PH7_NATIVE_IT_SRC);` |
|    62 |  427 | `	ph7_class_instance *pTarget = 0;` |
|     - |  428 | `	ph7_hashmap_node *pVal;` |
|     - |  429 | `	ph7_value *pSlot;` |
|    62 |  430 | `	while( pNode ){` |
|    44 |  431 | `		pTarget = WmNodeTarget(pNode);` |
|    44 |  432 | `		if( pTarget ){` |
|    44 |  433 | `			break;` |
|     - |  434 | `		}` |
|   ! 0 |  435 | `		pNode = pNode->pPrev;   /* a key that died since the last prune */` |
|   ! 0 |  436 | `	}` |
|    62 |  437 | `	if( pNode == 0 \|\| pTarget == 0 \|\| pWm == 0 ){` |
|    20 |  438 | `		PH7_NativeSetAttrBool(pVm,pIt,PH7_NATIVE_IT_DONE,1);` |
|    20 |  439 | `		return;` |
|     - |  440 | `	}` |
|    44 |  441 | `	PH7_NativeSetAttrInt(pVm,pIt,PH7_NATIVE_IT_POS,pNode->xKey.iKey);` |
|    44 |  442 | `	PH7_NativeSetAttrInt(pVm,pIt,PH7_NATIVE_IT_AUX,pNode->pPrev ? pNode->pPrev->xKey.iKey : -1);` |
|    44 |  443 | `	PH7_NativeSetAttrObj(pVm,pIt,PH7_NATIVE_IT_KEY,pTarget);` |
|    44 |  444 | `	pVal = WmFind(pVm,WmStore(pVm,pWm,WM_VALS),pNode->xKey.iKey);` |
|    44 |  445 | `	pSlot = pVal ? PH7_NativeAttr(pIt,PH7_NATIVE_IT_CUR) : 0;` |
|    44 |  446 | `	if( pSlot ){` |
|    44 |  447 | `		PH7_MemObjStore(HashmapExtractNodeValue(pVal),pSlot);` |
|    23 |  448 | `	}else{` |
|   ! 0 |  449 | `		PH7_NativeSetAttrObj(pVm,pIt,PH7_NATIVE_IT_CUR,0);   /* stores NULL */` |
|     - |  450 | `	}` |
|    44 |  451 | `	PH7_NativeSetAttrBool(pVm,pIt,PH7_NATIVE_IT_DONE,0);` |
|    32 |  452 | `}` |
|    34 |  453 | `static void WmRewind(ph7_vm *pVm,ph7_class_instance *pIt)` |
|     2 |  454 | `{` |
|    36 |  455 | `	ph7_class_instance *pWm = PH7_NativeAttrObj(pIt,PH7_NATIVE_IT_SRC);` |
|     - |  456 | `	ph7_hashmap *pRefs;` |
|    36 |  457 | `	if( pWm == 0 ){` |
|   ! 0 |  458 | `		PH7_NativeSetAttrBool(pVm,pIt,PH7_NATIVE_IT_DONE,1);` |
|   ! 0 |  459 | `		return;` |
|     - |  460 | `	}` |
|    36 |  461 | `	WmPrune(pVm,pWm);` |
|    36 |  462 | `	pRefs = WmStore(pVm,pWm,WM_REFS);` |
|    36 |  463 | `	WmSettle(pVm,pIt,pRefs ? pRefs->pFirst : 0);` |
|    19 |  464 | `}` |
|    26 |  465 | `static void WmNext(ph7_vm *pVm,ph7_class_instance *pIt)` |
|     2 |  466 | `{` |
|    28 |  467 | `	ph7_class_instance *pWm = PH7_NativeAttrObj(pIt,PH7_NATIVE_IT_SRC);` |
|    28 |  468 | `	ph7_hashmap *pRefs = pWm ? WmStore(pVm,pWm,WM_REFS) : 0;` |
|    28 |  469 | `	ph7_hashmap_node *pNode = WmFind(pVm,pRefs,PH7_NativeAttrInt(pIt,PH7_NATIVE_IT_POS));` |
|    28 |  470 | `	if( pNode ){` |
|    28 |  471 | `		pNode = pNode->pPrev;   /* the next-inserted entry; see WmPrune */` |
|    15 |  472 | `	}else{` |
|     - |  473 | `		/* The entry we were sitting on is gone: fall back on the successor` |
|     - |  474 | `		 * recorded when it settled. */` |
|   ! 0 |  475 | `		sxi64 iAux = PH7_NativeAttrInt(pIt,PH7_NATIVE_IT_AUX);` |
|   ! 0 |  476 | `		pNode = iAux < 0 ? 0 : WmFind(pVm,pRefs,iAux);` |
|     - |  477 | `	}` |
|    28 |  478 | `	WmSettle(pVm,pIt,pNode);` |
|    28 |  479 | `}` |
|     - |  480 | `static const PH7_NativeIterVtab sWmIterVtab = { WmRewind, WmNext };` |
|     - |  481 | `/* WeakMap::getIterator(): Iterator — a PHP GENERATOR before, which a C body cannot` |
|     - |  482 | ` * be; php answers an InternalIterator here, and so does this. */` |
|    18 |  483 | `static int vm_builtin_WeakMap_getIterator(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  484 | `{` |
|    20 |  485 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  486 | `	ph7_class_instance *pIt;` |
|     9 |  487 | `	SXUNUSED(nArg);` |
|     9 |  488 | `	SXUNUSED(apArg);` |
|    20 |  489 | `	if( pThis == 0 ){` |
|   ! 0 |  490 | `		return PH7_OK;` |
|     - |  491 | `	}` |
|    20 |  492 | `	pIt = PH7_NativeIteratorNew(pCtx->pVm,pThis);` |
|    20 |  493 | `	if( pIt == 0 ){` |
|   ! 0 |  494 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  495 | `	}` |
|    20 |  496 | `	PH7_NativeResultObject(pCtx,pIt);` |
|    20 |  497 | `	return PH7_OK;` |
|    11 |  498 | `}` |
|     - |  499 | `/*` |
|     - |  500 | ` * Declare both classes. WeakMap's three interfaces all declare METHODS, so they are` |
|     - |  501 | ` * attached only once its own exist -- PH7_ClassImplement stubs a missing one as` |
|     - |  502 | ` * ABSTRACT, which would leave the class uninstantiable.` |
|     - |  503 | ` */` |
|  5146 |  504 | `static sxi32 VmInstallWeak(ph7_vm *pVm)` |
|     5 |  505 | `{` |
|     - |  506 | `	static const PH7_NativePropDef aRefProp[] = {` |
|     - |  507 | `		/* The shared cell, as a pointer. Private to a final class and never handed` |
|     - |  508 | ``		 * to PHP -- what `__weak_create()` used to return into a userland slot. */`` |
|     - |  509 | `		{ "__h", PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  510 | `	};` |
|     - |  511 | `	static const PH7_NativeMethodDef aRefMethod[] = {` |
|     - |  512 | `		{ "__construct", PH7_MOD_PUBLIC, "", "", vm_builtin_WeakReference_construct },` |
|     - |  513 | `		{ "create",      PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "object $object", "WeakReference",` |
|     - |  514 | `		  vm_builtin_WeakReference_create },` |
|     - |  515 | `		{ "get",         PH7_MOD_PUBLIC, "", "?object", vm_builtin_WeakReference_get },` |
|     - |  516 | `	};` |
|     - |  517 | `	static const PH7_NativePropDef aMapProp[] = {` |
|     - |  518 | `		{ WM_REFS, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  519 | `		{ WM_VALS, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  520 | `	};` |
|     - |  521 | `	static const PH7_NativeMethodDef aMapMethod[] = {` |
|     - |  522 | `		/* php leaves the key parameter UNTYPED and screens it in the body, so that` |
|     - |  523 | `		 * a scalar key is "WeakMap key must be an object" rather than a ZPP report. */` |
|     - |  524 | `		/* php's declaration order, which is the order Reflection reports. */` |
|     - |  525 | `		{ "offsetGet",    PH7_MOD_PUBLIC, "$object", "mixed", vm_builtin_WeakMap_offsetGet },` |
|     - |  526 | `		{ "offsetSet",    PH7_MOD_PUBLIC, "$object, mixed $value", "void", vm_builtin_WeakMap_offsetSet },` |
|     - |  527 | `		{ "offsetExists", PH7_MOD_PUBLIC, "$object", "bool", vm_builtin_WeakMap_offsetExists },` |
|     - |  528 | `		{ "offsetUnset",  PH7_MOD_PUBLIC, "$object", "void", vm_builtin_WeakMap_offsetUnset },` |
|     - |  529 | `		{ "count",        PH7_MOD_PUBLIC, "", "int", vm_builtin_WeakMap_count },` |
|     - |  530 | `		{ "getIterator",  PH7_MOD_PUBLIC, "", "Iterator", vm_builtin_WeakMap_getIterator },` |
|     - |  531 | `	};` |
|     - |  532 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - |  533 | ``		/* Uncloneable in php too: the cell `__h` names is shared, and a slot-by-slot`` |
|     - |  534 | `		 * copy would drop it twice. */` |
|     - |  535 | `		{ "WeakReference", 0, 0, PH7_CLASS_FINAL\|PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|     - |  536 | `		  aRefMethod, SX_ARRAYSIZE(aRefMethod), 0, 0, aRefProp, SX_ARRAYSIZE(aRefProp),` |
|     - |  537 | `		  WkRefRelease, 0, WkPresent },` |
|     - |  538 | `		/* php's WeakMap read_dimension answers the stored zval itself, so an` |
|     - |  539 | `		 * indirect modification through it lands (PH7_CLASS_DIM_WRITABLE). */` |
|     - |  540 | `		{ "WeakMap", 0, 0, PH7_CLASS_FINAL\|PH7_CLASS_NOSERIALIZE\|PH7_CLASS_DIM_WRITABLE,` |
|     - |  541 | `		  aMapMethod, SX_ARRAYSIZE(aMapMethod), 0, 0, aMapProp, SX_ARRAYSIZE(aMapProp),` |
|     - |  542 | `		  0, &sWmIterVtab, 0 },` |
|     - |  543 | `	};` |
|     - |  544 | `	static const char *azMapIface[] = { "ArrayAccess", "Countable", "IteratorAggregate" };` |
|     - |  545 | `	ph7_class *pMap;` |
|     - |  546 | `	sxu32 n;` |
|  5151 |  547 | `	sxi32 rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|  5151 |  548 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  549 | `		return rc;` |
|     - |  550 | `	}` |
|  5151 |  551 | `	pMap = PH7_VmExtractClass(&(*pVm),"WeakMap",sizeof("WeakMap")-1,FALSE,0);` |
|  5151 |  552 | `	if( pMap == 0 ){` |
|   ! 0 |  553 | `		return SXERR_NOTFOUND;` |
|     - |  554 | `	}` |
| 20589 |  555 | `	for( n = 0 ; n < SX_ARRAYSIZE(azMapIface) ; n++ ){` |
| 23162 |  556 | `		ph7_class *pIface = PH7_VmExtractClass(&(*pVm),azMapIface[n],` |
| 15438 |  557 | `			(sxu32)SyStrlen(azMapIface[n]),FALSE,0);` |
| 15443 |  558 | `		if( pIface == 0 ){` |
|   ! 0 |  559 | `			return SXERR_NOTFOUND;` |
|     - |  560 | `		}` |
| 15443 |  561 | `		rc = PH7_ClassImplement(pMap,pIface);` |
| 15443 |  562 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  563 | `			return rc;` |
|     - |  564 | `		}` |
|  7724 |  565 | `	}` |
|  5151 |  566 | `	return SXRET_OK;` |
|  2578 |  567 | `}` |
|     - |  568 |  |
|     - |  569 | `/*` |
|     - |  570 | ` * ---------------------------------------------------------------------------` |
|     - |  571 | `` * The MEMBERS slot every SPL container's `__serialize()` carries.`` |
|     - |  572 | ` *` |
|     - |  573 | ` * php's payload for these classes is its own state plus a members array, and that` |
|     - |  574 | `` * array is `zend_std_get_properties()` — the instance's REAL properties. For a bare`` |
|     - |  575 | ` * SplObjectStorage or SplStack there are none, which is why every one of these bodies` |
|     - |  576 | ` * shipped with a hardcoded empty array; the moment a user SUBCLASSES one, its declared` |
|     - |  577 | `` * properties belong in the payload and PHL dropped them. `serialize()` then round-`` |
|     - |  578 | ` * tripped a SplStack subclass back to its property DEFAULTS with nothing failing —` |
|     - |  579 | ` * the same shape of silent wrong answer the date family's hidden slots had, one layer` |
|     - |  580 | ` * up. These two are that walk, shared by every container below.` |
|     - |  581 | ` *` |
|     - |  582 | `` * The walk must skip `PH7_CLASS_ATTR_HIDDEN`: a native class's engine slots are`` |
|     - |  583 | ` * exactly what the pair exists to replace. The load side writes only slots the class` |
|     - |  584 | ` * DECLARES, which is what the engine's own unserialize does with an unknown name` |
|     - |  585 | ` * (PHL has no dynamic properties).` |
|     - |  586 | ` * ---------------------------------------------------------------------------` |
|     - |  587 | ` */` |
|   104 |  588 | `static void SplAddMembers(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut)` |
|     1 |  589 | `{` |
|     - |  590 | `	SyHashEntry *pEntry;` |
|   105 |  591 | `	if( pThis == 0 ){` |
|   ! 0 |  592 | `		return;` |
|     - |  593 | `	}` |
|   105 |  594 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|   405 |  595 | `	while( (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|   301 |  596 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|   301 |  597 | `		SyString *pName = &pVmAttr->pAttr->sName;` |
|     - |  598 | `		ph7_value *pVal;` |
|     - |  599 | `		ph7_value sKey;` |
|   301 |  600 | `		if( pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT` |
|     - |  601 | `			\|PH7_CLASS_ATTR_HIDDEN\|PH7_CLASS_ATTR_HOOK_VIRTUAL) ){` |
|   273 |  602 | `			continue;` |
|     - |  603 | `		}` |
|    29 |  604 | `		pVal = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|    29 |  605 | `		if( pVal == 0 ){` |
|   ! 0 |  606 | `			continue;` |
|     - |  607 | `		}` |
|    29 |  608 | `		PH7_MemObjInitFromString(&(*pVm),&sKey,0);` |
|    29 |  609 | `		PH7_MemObjStringAppend(&sKey,pName->zString,pName->nByte);` |
|    29 |  610 | `		ph7_array_add_elem(pOut,&sKey,pVal);` |
|    29 |  611 | `		PH7_MemObjRelease(&sKey);` |
|     1 |  612 | `	}` |
|    53 |  613 | `}` |
|     - |  614 | `/* The same walk as a standalone array, which is the shape most payloads want. */` |
|    74 |  615 | `static sxi32 SplMembersOf(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut)` |
|     1 |  616 | `{` |
|    75 |  617 | `	PH7_MemObjInit(&(*pVm),pOut);` |
|    75 |  618 | `	if( PH7_MemObjToHashmap(pOut) != SXRET_OK ){` |
|   ! 0 |  619 | `		return SXERR_MEM;` |
|     - |  620 | `	}` |
|    75 |  621 | `	SplAddMembers(&(*pVm),pThis,pOut);` |
|    75 |  622 | `	return SXRET_OK;` |
|    38 |  623 | `}` |
|    10 |  624 | `static int SplMembersWalk(ph7_value *pKey,ph7_value *pVal,void *pUserData)` |
|     1 |  625 | `{` |
|    11 |  626 | `	ph7_class_instance *pThis = (ph7_class_instance *)pUserData;` |
|     - |  627 | `	const char *zKey;` |
|     - |  628 | `	int nKey;` |
|    11 |  629 | `	if( !ph7_value_is_string(pKey) ){` |
|   ! 0 |  630 | `		return PH7_OK;` |
|     - |  631 | `	}` |
|    11 |  632 | `	zKey = ph7_value_to_string(pKey,&nKey);` |
|    11 |  633 | `	PH7_NativeSetProp(pThis->pVm,pThis,zKey,(sxu32)nKey,pVal);` |
|    11 |  634 | `	return PH7_OK;` |
|     6 |  635 | `}` |
|    38 |  636 | `static void SplMembersLoad(ph7_class_instance *pThis,ph7_value *pMembers)` |
|     1 |  637 | `{` |
|    39 |  638 | `	if( pThis == 0 \|\| pMembers == 0 \|\| (pMembers->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|   ! 0 |  639 | `		return;` |
|     - |  640 | `	}` |
|    39 |  641 | `	ph7_array_walk(pMembers,SplMembersWalk,pThis);` |
|    20 |  642 | `}` |
|     - |  643 |  |
|     - |  644 | `/*` |
|     - |  645 | ` * ArrayIterator / ArrayObject — the array STORE, in C.` |
|     - |  646 | ` *` |
|     - |  647 | `` * These two shared one implementation through `trait __SplStoreT`, the last PHL-only`` |
|     - |  648 | ` * TRAIT and the last name in the §4 ledger that was not a function. php shares nothing` |
|     - |  649 | ` * between them at the TYPE level: both have no parent and no common interface beyond` |
|     - |  650 | `` * ArrayAccess/Countable, and the storage lives in ext/spl's own `spl_array_object` struct`` |
|     - |  651 | ` * behind handlers. The recorded decision follows php: no shared type at all —` |
|     - |  652 | ` * one set of C bodies, named by BOTH spec rows. The builder installs a method table per` |
|     - |  653 | ` * class anyway, so "replaying the method table" is a second row and nothing else, and the` |
|     - |  654 | ` * php-visible shape stays exact (a native abstract BASE would have given both classes a` |
|     - |  655 | ` * parent php does not have).` |
|     - |  656 | ` *` |
|     - |  657 | ` * The store itself stays a plain PHP array in a declared private slot, exactly as the trait` |
|     - |  658 | `` * had it: nothing here is a C handle, so `clone` and `serialize()` keep working as php's do`` |
|     - |  659 | ` * and neither class wants the NOCLONE/NOSERIALIZE flags an engine-state class needs. The` |
|     - |  660 | ` * bodies delegate to the engine's OWN array builtins (asort, ksort, uasort, reset, current,` |
|     - |  661 | ` * next, key), which is what the PHP did — one layer down, with no dispatcher round trip.` |
|     - |  662 | ` */` |
|     - |  663 | `#define SPL_D  "__d"  /* the stored array */` |
|     - |  664 | `#define SPL_F  "__f"  /* the flags word */` |
|     - |  665 | `#define SPL_IT "__it" /* ArrayObject's iterator class name */` |
|     - |  666 | `/*` |
|     - |  667 | `` * php's `~SPL_ARRAY_INT_MASK`: the flags word keeps only its low 16 bits, so`` |
|     - |  668 | `` * `setFlags(-1)` then `getFlags()` answers 65535 rather than -1. php masks on the`` |
|     - |  669 | ` * WRITE, which is why every reader — getFlags(), __serialize(), the ARRAY_AS_PROPS` |
|     - |  670 | ` * test — sees the same masked value without asking.` |
|     - |  671 | ` */` |
|     - |  672 | `#define SPL_FLAG_MASK 0xFFFF` |
|     - |  673 | `/* php's SPL_ARRAY_STD_PROP_LIST: the non-debug presentation surfaces answer the` |
|     - |  674 | ` * ordinary property table instead of the storage. */` |
|     - |  675 | `#define SPL_STD_PROP_LIST 0x0001` |
|     - |  676 | `/*` |
|     - |  677 | ` * The instance's storage slot, separated for writing (every caller may mutate it). Answers` |
|     - |  678 | ` * the SLOT rather than the hashmap because that is what the array builtins below take.` |
|     - |  679 | ` */` |
|  5828 |  680 | `static ph7_value * SplStoreSlot(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     5 |  681 | `{` |
|  5833 |  682 | `	ph7_value *pSlot = pThis ? PH7_NativeAttr(pThis,SPL_D) : 0;` |
|  5833 |  683 | `	if( pSlot == 0 ){` |
|   ! 0 |  684 | `		return 0;` |
|     - |  685 | `	}` |
|  5833 |  686 | `	if( (pSlot->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|   ! 0 |  687 | `		if( PH7_MemObjToHashmap(pSlot) != SXRET_OK ){` |
|   ! 0 |  688 | `			return 0;` |
|     - |  689 | `		}` |
|   ! 0 |  690 | `	}` |
|  5833 |  691 | `	if( PH7_HashmapCowSeparate(pVm,pSlot) == 0 ){` |
|   ! 0 |  692 | `		return 0;` |
|     - |  693 | `	}` |
|  5833 |  694 | `	return pSlot;` |
|  2919 |  695 | `}` |
|  3348 |  696 | `static ph7_hashmap * SplStore(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     4 |  697 | `{` |
|  3352 |  698 | `	ph7_value *pSlot = SplStoreSlot(pVm,pThis);` |
|  3352 |  699 | `	return pSlot ? (ph7_hashmap *)pSlot->x.pOther : 0;` |
|     4 |  700 | `}` |
|     - |  701 | `/*` |
|     - |  702 | `` * php's `spl_array_read_dimension` / `zend_weakmap_read_dimension` FAST PATH: a fetch on`` |
|     - |  703 | ` * one of these containers answers with the store's OWN element, not with a copy of it.` |
|     - |  704 | ` * That is the whole difference between a class that supports indirect modification and` |
|     - |  705 | `` * one that does not (PH7_VmDimFetchWritable, oo.c) — `$ao['a']` reached through`` |
|     - |  706 | `` * `offsetGet` is a VALUE however native the method is, so `$r = &$ao['a']`,`` |
|     - |  707 | ``  * `sort($ao['a'])`, `unset($ao['a'][0])`, `foreach ($ao['a'] as &$v)` and `$ao['n']++` `` |
|     - |  708 | ` * all wrote into a temporary and left the store as it was, in silence.` |
|     - |  709 | ` *` |
|     - |  710 | ` * Answers the element's aMemObj index, which the subscript op hands on as the result's` |
|     - |  711 | `` * slot exactly as an array element's `nValIdx` is handed on. SXU32_HIGH means "not`` |
|     - |  712 | `` * available" and the caller falls back to the ordinary `offsetGet` dispatch, which is`` |
|     - |  713 | ` * what keeps every diagnostic (WeakMap's not-contained Error, the store's own` |
|     - |  714 | `` * `Undefined array key`) in the one place that already words it.`` |
|     - |  715 | ` *` |
|     - |  716 | ` * bCreate is php's write-context vivification: a missing ArrayObject/ArrayIterator key` |
|     - |  717 | `` * IS created by a W fetch (`$ao['new']['k'] = 1` works there), while a WeakMap never`` |
|     - |  718 | ` * creates one — its missing key is an Error, raised by the accessor below.` |
|     - |  719 | ` */` |
|   122 |  720 | `PH7_PRIVATE sxu32 PH7_SplDimElemSlot(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pKey,int bCreate)` |
|     2 |  721 | `{` |
|   124 |  722 | `	ph7_hashmap_node *pNode = 0;` |
|   124 |  723 | `	if( pThis == 0 \|\| pKey == 0 ){` |
|   ! 0 |  724 | `		return SXU32_HIGH;` |
|     - |  725 | `	}` |
|   124 |  726 | `	if( pKey->iFlags & MEMOBJ_NULL ){` |
|     - |  727 | `		/* PH7_HashmapLookup folds a NULL key to "" IN PLACE, and a declined fast path` |
|     - |  728 | `		 * has to hand the accessor the key it was written with (php deprecates the null` |
|     - |  729 | `		 * offset there). Cheaper to stand down than to probe on a copy. */` |
|   ! 0 |  730 | `		return SXU32_HIGH;` |
|     - |  731 | `	}` |
|   124 |  732 | `	if( PH7_NativeAttr(pThis,SPL_D) != 0 ){` |
|    97 |  733 | `		ph7_value *pSlot = SplStoreSlot(pVm,pThis);` |
|    97 |  734 | `		ph7_hashmap *pMap = pSlot ? (ph7_hashmap *)pSlot->x.pOther : 0;` |
|    97 |  735 | `		if( pMap == 0 ){` |
|   ! 0 |  736 | `			return SXU32_HIGH;` |
|     - |  737 | `		}` |
|    97 |  738 | `		if( PH7_HashmapLookup(pMap,pKey,&pNode) != SXRET_OK ){` |
|     9 |  739 | `			if( !bCreate \|\| PH7_HashmapInsert(pMap,pKey,0) != SXRET_OK ){` |
|     3 |  740 | `				return SXU32_HIGH;` |
|     - |  741 | `			}` |
|     7 |  742 | `			pNode = pMap->pLast;` |
|     3 |  743 | `		}` |
|    95 |  744 | `		return pNode ? pNode->nValIdx : SXU32_HIGH;` |
|     - |  745 | `	}` |
|    28 |  746 | `	if( PH7_NativeAttr(pThis,WM_VALS) != 0 && (pKey->iFlags & MEMOBJ_OBJ) ){` |
|    26 |  747 | `		ph7_class_instance *pObj = (ph7_class_instance *)pKey->x.pOther;` |
|    26 |  748 | `		sxi64 iId = (sxi64)pObj->nObjId;` |
|    26 |  749 | `		if( WmNodeTarget(WmFind(pVm,WmStore(pVm,pThis,WM_REFS),iId)) != pObj ){` |
|     5 |  750 | `			return SXU32_HIGH; /* not contained: offsetGet raises php's Error */` |
|     - |  751 | `		}` |
|    22 |  752 | `		pNode = WmFind(pVm,WmStore(pVm,pThis,WM_VALS),iId);` |
|    22 |  753 | `		return pNode ? pNode->nValIdx : SXU32_HIGH;` |
|     - |  754 | `	}` |
|     3 |  755 | `	return SXU32_HIGH;` |
|    63 |  756 | `}` |
|     - |  757 | `/*` |
|     - |  758 | `` * `$this->__d = $array` for the constructor and exchangeArray(), with php's refusal.`` |
|     - |  759 | ` *` |
|     - |  760 | `` * php DECLARES `object\|array $array` — which is what Reflection prints — and then words the`` |
|     - |  761 | `` * refusal as `must be of type array`, so the shared ZPP screen cannot say both (rule 41's`` |
|     - |  762 | ` * shape) and the check is written here. An OBJECT contributes its properties, as the PHP` |
|     - |  763 | ` * did through get_object_vars().` |
|     - |  764 | ` */` |
|   800 |  765 | `static sxi32 SplInitStore(ph7_context *pCtx,ph7_class_instance *pThis,` |
|     - |  766 | `	ph7_value *pArray,const char *zOwner)` |
|     5 |  767 | `{` |
|   805 |  768 | `	ph7_vm *pVm = pCtx->pVm;` |
|   805 |  769 | `	ph7_value *pSlot = pThis ? PH7_NativeAttr(pThis,SPL_D) : 0;` |
|   805 |  770 | `	if( pSlot == 0 ){` |
|   ! 0 |  771 | `		return PH7_OK;` |
|     - |  772 | `	}` |
|   805 |  773 | `	if( pArray == 0 ){` |
|     - |  774 | ``		/* No argument at all: php's `$array = []` default. A native method has no compiled`` |
|     - |  775 | `		 * parameter records for the defaults to live in (rule 33's neighbour), so the body` |
|     - |  776 | `		 * applies it — and an EXPLICIT null still has to reach the refusal below, which is` |
|     - |  777 | `		 * why the two cases are distinguished here rather than by a NULL check. */` |
|    83 |  778 | `		ph7_hashmap *pEmpty = PH7_NewHashmap(pVm,0,0);` |
|    83 |  779 | `		if( pEmpty == 0 ){` |
|   ! 0 |  780 | `			return PH7_ContextMemoryError(pCtx);` |
|     - |  781 | `		}` |
|    83 |  782 | `		PH7_MemObjRelease(pSlot);` |
|    83 |  783 | `		pSlot->x.pOther = pEmpty;` |
|    83 |  784 | `		MemObjSetType(pSlot,MEMOBJ_HASHMAP);` |
|    83 |  785 | `		return PH7_OK;` |
|     - |  786 | `	}` |
|   723 |  787 | `	if( pArray->iFlags & MEMOBJ_HASHMAP ){` |
|   707 |  788 | `		PH7_MemObjRelease(pSlot);` |
|   707 |  789 | `		PH7_MemObjStore(pArray,pSlot); /* a copy: the store is the object's own */` |
|   707 |  790 | `		return PH7_OK;` |
|     - |  791 | `	}` |
|    17 |  792 | `	if( pArray->iFlags & MEMOBJ_OBJ ){` |
|     - |  793 | `		/* The PHP read get_object_vars($array): the properties this scope can see, by` |
|     - |  794 | `		 * their plain names. php itself keeps the OBJECT and reads its property table` |
|     - |  795 | `		 * live (so getArrayCopy() answers the mangled private names and count() answers` |
|     - |  796 | `		 * the visible ones) — a divergence this conversion carries over unchanged rather` |
|     - |  797 | `		 * than widening, recorded in §7.4. */` |
|     5 |  798 | `		ph7_class_instance *pObj = (ph7_class_instance *)pArray->x.pOther;` |
|     - |  799 | `		ph7_hashmap *pMap;` |
|     - |  800 | `		SyHashEntry *pEntry;` |
|     5 |  801 | `		PH7_MemObjRelease(pSlot);` |
|     5 |  802 | `		pMap = PH7_NewHashmap(pVm,0,0);` |
|     5 |  803 | `		if( pMap == 0 ){` |
|   ! 0 |  804 | `			return PH7_ContextMemoryError(pCtx);` |
|     - |  805 | `		}` |
|     5 |  806 | `		SyHashResetLoopCursor(&pObj->hAttr);` |
|    13 |  807 | `		while( pObj && (pEntry = SyHashGetNextEntry(&pObj->hAttr)) != 0 ){` |
|     9 |  808 | `			VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     - |  809 | `			ph7_value sKey;` |
|     - |  810 | `			ph7_value *pVal;` |
|     9 |  811 | `			if( pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|   ! 0 |  812 | `				continue;` |
|     - |  813 | `			}` |
|     9 |  814 | `			if( pVmAttr->pAttr->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|   ! 0 |  815 | `				continue;` |
|     - |  816 | `			}` |
|     9 |  817 | `			pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pVmAttr->nIdx);` |
|     9 |  818 | `			if( pVal == 0 ){` |
|   ! 0 |  819 | `				continue;` |
|     - |  820 | `			}` |
|     9 |  821 | `			PH7_MemObjInitFromString(pVm,&sKey,&pVmAttr->pAttr->sName);` |
|     9 |  822 | `			PH7_HashmapInsert(pMap,&sKey,pVal);` |
|     9 |  823 | `			PH7_MemObjRelease(&sKey);` |
|     1 |  824 | `		}` |
|     5 |  825 | `		pSlot->x.pOther = pMap;` |
|     5 |  826 | `		MemObjSetType(pSlot,MEMOBJ_HASHMAP);` |
|     5 |  827 | `		return PH7_OK;` |
|     - |  828 | `	}` |
|    19 |  829 | `	return PH7_VmThrowException(pCtx,"TypeError",` |
|     - |  830 | `		"%s(): Argument #1 ($array) must be of type array, %s given",` |
|     6 |  831 | `		zOwner,ph7_type_name(pArray));` |
|   405 |  832 | `}` |
|     - |  833 | `/* Hand one of the engine's own array builtins the instance's storage slot. */` |
|  2246 |  834 | `static int SplArrayCall(ph7_context *pCtx,ProchHostFunction xFunc,ph7_value *pExtra)` |
|     4 |  835 | `{` |
|  2250 |  836 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  837 | `	ph7_value *apCall[2];` |
|  2250 |  838 | `	ph7_value *pSlot = SplStoreSlot(pCtx->pVm,pThis);` |
|  2250 |  839 | `	if( pSlot == 0 ){` |
|   ! 0 |  840 | `		return PH7_OK;` |
|     - |  841 | `	}` |
|  2250 |  842 | `	apCall[0] = pSlot;` |
|  2250 |  843 | `	apCall[1] = pExtra;` |
|  2250 |  844 | `	return xFunc(pCtx,pExtra ? 2 : 1,apCall);` |
|  1127 |  845 | `}` |
|    16 |  846 | `static int vm_builtin_SplStore_offsetExists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  847 | `{` |
|    18 |  848 | `	ph7_hashmap *pMap = SplStore(pCtx->pVm,PH7_ContextThis(pCtx));` |
|    18 |  849 | `	ph7_hashmap_node *pNode = 0;` |
|    18 |  850 | `	int bFound = 0;` |
|    18 |  851 | `	if( pMap && nArg > 0 ){` |
|     - |  852 | `		/* array_key_exists(), not isset(): php's offsetExists() answers true for a key` |
|     - |  853 | `		 * holding NULL (the PHP said array_key_exists too). */` |
|    18 |  854 | `		bFound = PH7_HashmapLookup(pMap,apArg[0],&pNode) == SXRET_OK;` |
|     8 |  855 | `	}` |
|    18 |  856 | `	ph7_result_bool(pCtx,bFound);` |
|    18 |  857 | `	return PH7_OK;` |
|     2 |  858 | `}` |
|    24 |  859 | `static int vm_builtin_SplStore_offsetGet(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  860 | `{` |
|    26 |  861 | `	ph7_vm *pVm = pCtx->pVm;` |
|    26 |  862 | `	ph7_hashmap *pMap = SplStore(pVm,PH7_ContextThis(pCtx));` |
|    26 |  863 | `	ph7_hashmap_node *pNode = 0;` |
|    26 |  864 | `	if( pMap == 0 \|\| nArg < 1 \|\| PH7_HashmapLookup(pMap,apArg[0],&pNode) != SXRET_OK ){` |
|     - |  865 | `		/* php warns "Undefined array key" for a missing offset, with the key rendered` |
|     - |  866 | `		 * the way the LOOKUP folded it (an integer bare, a string quoted) — the same` |
|     - |  867 | `		 * pair OP_LOAD_IDX prints. This one now reports the CALLER's line, where the` |
|     - |  868 | `		 * PHP reported the chunk's. */` |
|     3 |  869 | `		if( nArg > 0 ){` |
|     - |  870 | `			SyBlob sMsg;` |
|     3 |  871 | `			SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     3 |  872 | `			if( PH7_HashmapKeyIsInt(apArg[0]) ){` |
|   ! 0 |  873 | `				if( (apArg[0]->iFlags & MEMOBJ_INT) == 0 ){` |
|   ! 0 |  874 | `					PH7_MemObjToInteger(apArg[0]);` |
|   ! 0 |  875 | `				}` |
|   ! 0 |  876 | `				SyBlobFormat(&sMsg,"Undefined array key %qd",apArg[0]->x.iVal);` |
|   ! 0 |  877 | `			}else{` |
|     - |  878 | `				SyString sKey;` |
|     3 |  879 | `				SyStringInitFromBuf(&sKey,SyBlobData(&apArg[0]->sBlob),` |
|     - |  880 | `					SyBlobLength(&apArg[0]->sBlob));` |
|     3 |  881 | `				SyBlobFormat(&sMsg,"Undefined array key \"%z\"",&sKey);` |
|     - |  882 | `			}` |
|     3 |  883 | `			SyBlobNullAppend(&sMsg);` |
|     3 |  884 | `			PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,(const char *)SyBlobData(&sMsg));` |
|     3 |  885 | `			SyBlobRelease(&sMsg);` |
|     1 |  886 | `		}` |
|     3 |  887 | `		ph7_result_null(pCtx);` |
|     3 |  888 | `		return PH7_OK;` |
|     - |  889 | `	}` |
|    24 |  890 | `	ph7_result_value(pCtx,(ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx));` |
|    24 |  891 | `	return PH7_OK;` |
|    14 |  892 | `}` |
|     - |  893 | `/*` |
|     - |  894 | ` * Insert into the store, keeping php's cursor rule.` |
|     - |  895 | ` *` |
|     - |  896 | ` * php's ArrayIterator position is an INTEGER index into the bucket array, so a` |
|     - |  897 | ` * cursor that ran off the end sits AT the element count: inserting a new key there` |
|     - |  898 | ` * makes it valid again and the iterator RESUMES on the element just added. PHL` |
|     - |  899 | ` * carries a node POINTER, which is null past the end and loses that. Re-point it` |
|     - |  900 | ` * here -- the only place the difference shows, since overwriting an EXISTING key` |
|     - |  901 | ` * inserts no node and php's dead cursor stays dead. AppendIterator depends on this:` |
|     - |  902 | ` * php's append() after exhaustion is what makes the walk continue.` |
|     - |  903 | ` */` |
|   228 |  904 | `static void SplStoreInsert(ph7_hashmap *pMap,ph7_value *pKey,ph7_value *pVal)` |
|     1 |  905 | `{` |
|   229 |  906 | `	sxu32 nBefore = pMap->nEntry;` |
|   229 |  907 | `	int bPastEnd = pMap->pCur == 0;` |
|   229 |  908 | `	PH7_HashmapInsert(pMap,pKey,pVal);` |
|   229 |  909 | `	if( bPastEnd && pMap->nEntry > nBefore ){` |
|   121 |  910 | `		pMap->pCur = pMap->pLast;` |
|    60 |  911 | `	}` |
|   229 |  912 | `}` |
|    28 |  913 | `static int vm_builtin_SplStore_offsetSet(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  914 | `{` |
|    29 |  915 | `	ph7_hashmap *pMap = SplStore(pCtx->pVm,PH7_ContextThis(pCtx));` |
|    29 |  916 | `	if( pMap && nArg > 1 ){` |
|     - |  917 | ``		/* A NULL key is `$o[] = $v` — the append form, which is how php's offsetSet()`` |
|     - |  918 | `		 * receives it. */` |
|    29 |  919 | `		SplStoreInsert(pMap,(apArg[0]->iFlags & MEMOBJ_NULL) ? 0 : apArg[0],apArg[1]);` |
|    14 |  920 | `	}` |
|    29 |  921 | `	return PH7_OK;` |
|     1 |  922 | `}` |
|     6 |  923 | `static int vm_builtin_SplStore_offsetUnset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  924 | `{` |
|     7 |  925 | `	ph7_hashmap *pMap = SplStore(pCtx->pVm,PH7_ContextThis(pCtx));` |
|     7 |  926 | `	ph7_hashmap_node *pNode = 0;` |
|     7 |  927 | `	if( pMap && nArg > 0 && PH7_HashmapLookup(pMap,apArg[0],&pNode) == SXRET_OK ){` |
|     7 |  928 | `		PH7_HashmapUnlinkNode(pNode,TRUE);` |
|     3 |  929 | `	}` |
|     7 |  930 | `	return PH7_OK;` |
|     1 |  931 | `}` |
|     8 |  932 | `static int vm_builtin_SplStore_append(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  933 | `{` |
|     9 |  934 | `	ph7_hashmap *pMap = SplStore(pCtx->pVm,PH7_ContextThis(pCtx));` |
|     9 |  935 | `	if( pMap && nArg > 0 ){` |
|     9 |  936 | `		SplStoreInsert(pMap,0,apArg[0]);` |
|     4 |  937 | `	}` |
|     9 |  938 | `	return PH7_OK;` |
|     1 |  939 | `}` |
|    70 |  940 | `static int vm_builtin_SplStore_getArrayCopy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  941 | `{` |
|    73 |  942 | `	ph7_value *pSlot = SplStoreSlot(pCtx->pVm,PH7_ContextThis(pCtx));` |
|    35 |  943 | `	SXUNUSED(nArg);` |
|    35 |  944 | `	SXUNUSED(apArg);` |
|    73 |  945 | `	if( pSlot ){` |
|    73 |  946 | `		ph7_result_value(pCtx,pSlot); /* a COPY: the caller must not alias the store */` |
|    35 |  947 | `	}` |
|    73 |  948 | `	return PH7_OK;` |
|     3 |  949 | `}` |
|    24 |  950 | `static int vm_builtin_SplStore_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  951 | `{` |
|    25 |  952 | `	ph7_hashmap *pMap = SplStore(pCtx->pVm,PH7_ContextThis(pCtx));` |
|    12 |  953 | `	SXUNUSED(nArg);` |
|    12 |  954 | `	SXUNUSED(apArg);` |
|    25 |  955 | `	ph7_result_int64(pCtx,pMap ? (ph7_int64)pMap->nEntry : 0);` |
|    25 |  956 | `	return PH7_OK;` |
|     1 |  957 | `}` |
|    14 |  958 | `static int vm_builtin_SplStore_getFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  959 | `{` |
|    15 |  960 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     7 |  961 | `	SXUNUSED(nArg);` |
|     7 |  962 | `	SXUNUSED(apArg);` |
|    15 |  963 | `	ph7_result_int64(pCtx,pThis ? PH7_NativeAttrInt(pThis,SPL_F) : 0);` |
|    15 |  964 | `	return PH7_OK;` |
|     1 |  965 | `}` |
|     2 |  966 | `static int vm_builtin_SplStore_setFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  967 | `{` |
|     3 |  968 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     3 |  969 | `	if( pThis && nArg > 0 ){` |
|     4 |  970 | `		PH7_NativeSetAttrInt(pCtx->pVm,pThis,SPL_F,` |
|     2 |  971 | `			ph7_value_to_int64(apArg[0]) & SPL_FLAG_MASK);` |
|     1 |  972 | `	}` |
|     3 |  973 | `	return PH7_OK;` |
|     1 |  974 | `}` |
|     - |  975 | `/*` |
|     - |  976 | ` * The six sorts. Each is the engine's own builtin over the stored array — including the` |
|     - |  977 | `` * `$flags` the PHP DROPPED on the floor (`asort($this->__d)` ignored its own parameter, so`` |
|     - |  978 | `` * `$it->asort(SORT_STRING)` sorted numerically). natsort/natcasesort go through asort with`` |
|     - |  979 | ` * php's own flag pair rather than by name: the shared body reads ph7_function_name() to tell` |
|     - |  980 | `` * the two apart, and a native method's name is `ArrayIterator::natcasesort`.`` |
|     - |  981 | ` */` |
|     6 |  982 | `static int vm_builtin_SplStore_asort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  983 | `{` |
|     7 |  984 | `	return SplArrayCall(pCtx,ph7_hashmap_asort,nArg > 0 ? apArg[0] : 0);` |
|     1 |  985 | `}` |
|     6 |  986 | `static int vm_builtin_SplStore_ksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  987 | `{` |
|     7 |  988 | `	return SplArrayCall(pCtx,ph7_hashmap_ksort,nArg > 0 ? apArg[0] : 0);` |
|     1 |  989 | `}` |
|     4 |  990 | `static int vm_builtin_SplStore_uasort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  991 | `{` |
|     5 |  992 | `	return SplArrayCall(pCtx,ph7_hashmap_uasort,nArg > 0 ? apArg[0] : 0);` |
|     1 |  993 | `}` |
|     2 |  994 | `static int vm_builtin_SplStore_uksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  995 | `{` |
|     3 |  996 | `	return SplArrayCall(pCtx,ph7_hashmap_uksort,nArg > 0 ? apArg[0] : 0);` |
|     1 |  997 | `}` |
|    12 |  998 | `static int SplNatSort(ph7_context *pCtx,int bFold)` |
|     3 |  999 | `{` |
|     - | 1000 | `	ph7_value sFlags;` |
|     - | 1001 | `	int rc;` |
|     - | 1002 | `	/* SORT_NATURAL (6), plus SORT_FLAG_CASE (8) for the folding twin — the same pair` |
|     - | 1003 | `	 * ph7_hashmap_natsort forwards to asort(). */` |
|    15 | 1004 | `	PH7_MemObjInitFromInt(pCtx->pVm,&sFlags,bFold ? (6\|8) : 6);` |
|    15 | 1005 | `	rc = SplArrayCall(pCtx,ph7_hashmap_asort,&sFlags);` |
|    15 | 1006 | `	PH7_MemObjRelease(&sFlags);` |
|    15 | 1007 | `	return rc;` |
|     3 | 1008 | `}` |
|     8 | 1009 | `static int vm_builtin_SplStore_natsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 | 1010 | `{` |
|     4 | 1011 | `	SXUNUSED(nArg);` |
|     4 | 1012 | `	SXUNUSED(apArg);` |
|    11 | 1013 | `	return SplNatSort(pCtx,0);` |
|     3 | 1014 | `}` |
|     4 | 1015 | `static int vm_builtin_SplStore_natcasesort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 1016 | `{` |
|     2 | 1017 | `	SXUNUSED(nArg);` |
|     2 | 1018 | `	SXUNUSED(apArg);` |
|     6 | 1019 | `	return SplNatSort(pCtx,1);` |
|     2 | 1020 | `}` |
|     - | 1021 | `/* ArrayIterator's cursor: the stored array's own internal pointer, as the PHP had it. */` |
|   670 | 1022 | `static int vm_builtin_ArrayIterator_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 | 1023 | `{` |
|   673 | 1024 | `	ph7_hashmap *pMap = SplStore(pCtx->pVm,PH7_ContextThis(pCtx));` |
|   335 | 1025 | `	SXUNUSED(nArg);` |
|   335 | 1026 | `	SXUNUSED(apArg);` |
|   673 | 1027 | `	if( pMap == 0 \|\| pMap->pCur == 0 ){` |
|     - | 1028 | `		/* Past the end php answers NULL, where current() the FUNCTION answers false. */` |
|     7 | 1029 | `		ph7_result_null(pCtx);` |
|     7 | 1030 | `		return PH7_OK;` |
|     - | 1031 | `	}` |
|   667 | 1032 | `	return SplArrayCall(pCtx,ph7_hashmap_current,0);` |
|   338 | 1033 | `}` |
|   620 | 1034 | `static int vm_builtin_ArrayIterator_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 1035 | `{` |
|   310 | 1036 | `	SXUNUSED(nArg);` |
|   310 | 1037 | `	SXUNUSED(apArg);` |
|   622 | 1038 | `	return SplArrayCall(pCtx,ph7_hashmap_simple_key,0);` |
|     2 | 1039 | `}` |
|   560 | 1040 | `static int vm_builtin_ArrayIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1041 | `{` |
|   280 | 1042 | `	SXUNUSED(nArg);` |
|   280 | 1043 | `	SXUNUSED(apArg);` |
|   561 | 1044 | `	SplArrayCall(pCtx,ph7_hashmap_next,0);` |
|   561 | 1045 | `	ph7_result_null(pCtx); /* next() the METHOD returns void */` |
|   561 | 1046 | `	return PH7_OK;` |
|     1 | 1047 | `}` |
|   372 | 1048 | `static int vm_builtin_ArrayIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 | 1049 | `{` |
|   186 | 1050 | `	SXUNUSED(nArg);` |
|   186 | 1051 | `	SXUNUSED(apArg);` |
|   375 | 1052 | `	SplArrayCall(pCtx,ph7_hashmap_reset,0);` |
|   375 | 1053 | `	ph7_result_null(pCtx);` |
|   375 | 1054 | `	return PH7_OK;` |
|     3 | 1055 | `}` |
|  1218 | 1056 | `static int vm_builtin_ArrayIterator_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 | 1057 | `{` |
|  1221 | 1058 | `	ph7_hashmap *pMap = SplStore(pCtx->pVm,PH7_ContextThis(pCtx));` |
|   609 | 1059 | `	SXUNUSED(nArg);` |
|   609 | 1060 | `	SXUNUSED(apArg);` |
|  1221 | 1061 | `	ph7_result_bool(pCtx,pMap && pMap->pCur ? 1 : 0);` |
|  1221 | 1062 | `	return PH7_OK;` |
|     3 | 1063 | `}` |
|    34 | 1064 | `static int vm_builtin_ArrayIterator_seek(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1065 | `{` |
|    35 | 1066 | `	ph7_hashmap *pMap = SplStore(pCtx->pVm,PH7_ContextThis(pCtx));` |
|    35 | 1067 | `	ph7_int64 iOffset = nArg > 0 ? ph7_value_to_int(apArg[0]) : 0;` |
|     - | 1068 | `	ph7_int64 i;` |
|    35 | 1069 | `	if( pMap == 0 ){` |
|   ! 0 | 1070 | `		return PH7_OK;` |
|     - | 1071 | `	}` |
|    35 | 1072 | `	if( iOffset < 0 \|\| iOffset >= (ph7_int64)pMap->nEntry ){` |
|    10 | 1073 | `		return PH7_VmThrowException(pCtx,"OutOfBoundsException",` |
|     3 | 1074 | `			"Seek position %qd is out of range",iOffset);` |
|     - | 1075 | `	}` |
|    29 | 1076 | `	pMap->pCur = pMap->pFirst;` |
|    65 | 1077 | `	for( i = 0 ; i < iOffset && pMap->pCur ; ++i ){` |
|    37 | 1078 | `		pMap->pCur = pMap->pCur->pPrev; /* insertion order: pFirst, then the pPrev chain */` |
|    19 | 1079 | `	}` |
|    29 | 1080 | `	return PH7_OK;` |
|    18 | 1081 | `}` |
|   614 | 1082 | `static int vm_builtin_ArrayIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     4 | 1083 | `{` |
|   618 | 1084 | `	ph7_vm *pVm = pCtx->pVm;` |
|   618 | 1085 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 1086 | `	ph7_hashmap *pMap;` |
|   618 | 1087 | `	sxi32 rc = SplInitStore(pCtx,pThis,nArg > 0 ? apArg[0] : 0,` |
|     - | 1088 | `		"ArrayIterator::__construct");` |
|   618 | 1089 | `	if( rc != PH7_OK ){` |
|    11 | 1090 | `		return rc;` |
|     - | 1091 | `	}` |
|   608 | 1092 | `	if( pThis && nArg > 1 ){` |
|    99 | 1093 | `		PH7_NativeSetAttrInt(pVm,pThis,SPL_F,ph7_value_to_int64(apArg[1]) & SPL_FLAG_MASK);` |
|    49 | 1094 | `	}` |
|   608 | 1095 | `	pMap = SplStore(pVm,pThis);` |
|   608 | 1096 | `	if( pMap ){` |
|   608 | 1097 | `		pMap->pCur = pMap->pFirst; /* reset($this->__d) */` |
|   302 | 1098 | `	}` |
|   608 | 1099 | `	return PH7_OK;` |
|   311 | 1100 | `}` |
|     - | 1101 | `/* ArrayObject */` |
|    16 | 1102 | `static int vm_builtin_ArrayObject_setIteratorClass(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1103 | `{` |
|    17 | 1104 | `	ph7_vm *pVm = pCtx->pVm;` |
|    17 | 1105 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 1106 | `	const char *zName;` |
|     - | 1107 | `	int nName;` |
|    17 | 1108 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|   ! 0 | 1109 | `		return PH7_OK;` |
|     - | 1110 | `	}` |
|    17 | 1111 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|    16 | 1112 | `	if( nName != (int)sizeof("ArrayIterator")-1` |
|     9 | 1113 | `	 \|\| SyMemcmp(zName,"ArrayIterator",sizeof("ArrayIterator")-1) != 0 ){` |
|    17 | 1114 | `		ph7_class *pClass = PH7_VmExtractClass(pVm,zName,(sxu32)nName,FALSE,0);` |
|    17 | 1115 | `		ph7_class *pBase = PH7_VmExtractClass(pVm,"ArrayIterator",` |
|     - | 1116 | `			sizeof("ArrayIterator")-1,FALSE,0);` |
|    16 | 1117 | `		if( pClass == 0 \|\| pBase == 0 \|\| pClass == pBase` |
|    15 | 1118 | `		 \|\| !PH7_VmInstanceOf(pClass,pBase) ){` |
|    13 | 1119 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 1120 | `				"ArrayObject::setIteratorClass(): Argument #1 ($iteratorClass) must be "` |
|     4 | 1121 | `				"a class name derived from ArrayIterator, %.*s given",nName,zName);` |
|     - | 1122 | `		}` |
|     4 | 1123 | `	}` |
|     9 | 1124 | `	PH7_NativeSetAttrStr(pVm,pThis,SPL_IT,zName,nName);` |
|     9 | 1125 | `	return PH7_OK;` |
|     9 | 1126 | `}` |
|   162 | 1127 | `static int vm_builtin_ArrayObject_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     4 | 1128 | `{` |
|   166 | 1129 | `	ph7_vm *pVm = pCtx->pVm;` |
|   166 | 1130 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   166 | 1131 | `	sxi32 rc = SplInitStore(pCtx,pThis,nArg > 0 ? apArg[0] : 0,` |
|     - | 1132 | `		"ArrayObject::__construct");` |
|   166 | 1133 | `	if( rc != PH7_OK ){` |
|     3 | 1134 | `		return rc;` |
|     - | 1135 | `	}` |
|   164 | 1136 | `	if( pThis && nArg > 1 ){` |
|    19 | 1137 | `		PH7_NativeSetAttrInt(pVm,pThis,SPL_F,ph7_value_to_int64(apArg[1]) & SPL_FLAG_MASK);` |
|     9 | 1138 | `	}` |
|   164 | 1139 | `	if( nArg > 2 ){` |
|     7 | 1140 | `		return vm_builtin_ArrayObject_setIteratorClass(pCtx,1,&apArg[2]);` |
|     - | 1141 | `	}` |
|   158 | 1142 | `	return PH7_OK;` |
|    85 | 1143 | `}` |
|     4 | 1144 | `static int vm_builtin_ArrayObject_exchangeArray(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1145 | `{` |
|     5 | 1146 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     5 | 1147 | `	ph7_value *pSlot = pThis ? PH7_NativeAttr(pThis,SPL_D) : 0;` |
|     - | 1148 | `	sxi32 rc;` |
|     5 | 1149 | `	if( pSlot ){` |
|     5 | 1150 | `		ph7_result_value(pCtx,pSlot); /* the OLD store is the return value */` |
|     2 | 1151 | `	}` |
|     5 | 1152 | `	rc = SplInitStore(pCtx,pThis,nArg > 0 ? apArg[0] : 0,"ArrayObject::exchangeArray");` |
|     5 | 1153 | `	return rc;` |
|     1 | 1154 | `}` |
|    10 | 1155 | `static int vm_builtin_ArrayObject_getIteratorClass(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1156 | `{` |
|    11 | 1157 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    11 | 1158 | `	const char *zName = 0;` |
|    11 | 1159 | `	int nName = 0;` |
|     5 | 1160 | `	SXUNUSED(nArg);` |
|     5 | 1161 | `	SXUNUSED(apArg);` |
|    11 | 1162 | `	if( pThis ){` |
|    11 | 1163 | `		PH7_NativeAttrStr(pThis,SPL_IT,&zName,&nName);` |
|     5 | 1164 | `	}` |
|    11 | 1165 | `	ph7_result_string(pCtx,nName > 0 ? zName : "ArrayIterator",nName > 0 ? nName : -1);` |
|    11 | 1166 | `	return PH7_OK;` |
|     1 | 1167 | `}` |
|     - | 1168 | `/*` |
|     - | 1169 | ` * ---------------------------------------------------------------------------` |
|     - | 1170 | ` * php's __serialize()/__unserialize() for the array store.` |
|     - | 1171 | ` *` |
|     - | 1172 | ` * php's payload is a four-element LIST -- [flags, storage, members, iterator class]` |
|     - | 1173 | ` * -- and nothing else can express it: the state lives in ext/spl's own struct, so` |
|     - | 1174 | ` * there are no properties to walk. PHL walked its HIDDEN slots instead and wrote` |
|     - | 1175 | `` * `O:11:"ArrayObject":3:{s:16:"\0ArrayObject\0__d";…}`, which round-tripped inside`` |
|     - | 1176 | ` * PHL and could not read a byte string php produced (nor be read by php).` |
|     - | 1177 | ` *` |
|     - | 1178 | ` * Two details worth keeping. The last element is NULL when the class is the default` |
|     - | 1179 | ` * ArrayIterator, and it is always NULL for an ArrayIterator payload -- php shares one` |
|     - | 1180 | ` * C body between both classes, so ArrayIterator carries the slot it has no use for.` |
|     - | 1181 | ` * And the iterator-class check here is LOOSER than setIteratorClass()'s: restoring` |
|     - | 1182 | `` * accepts any `Iterator`, while the setter and the constructor demand a class derived`` |
|     - | 1183 | `` * from ArrayIterator. php words the refusal with `ArrayObject` either way, even when`` |
|     - | 1184 | ` * ArrayIterator is the receiver.` |
|     - | 1185 | ` * ---------------------------------------------------------------------------` |
|     - | 1186 | ` */` |
|     8 | 1187 | `static sxi32 SplStoreIllTyped(ph7_context *pCtx)` |
|     1 | 1188 | `{` |
|     9 | 1189 | `	return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     - | 1190 | `		"Incomplete or ill-typed serialization data");` |
|     1 | 1191 | `}` |
|    26 | 1192 | `static int vm_builtin_SplStore_serializeMagic(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1193 | `{` |
|    27 | 1194 | `	ph7_vm *pVm = pCtx->pVm;` |
|    27 | 1195 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 1196 | `	ph7_value sOut,sVal,*pStore;` |
|    27 | 1197 | `	const char *zIt = 0;` |
|    27 | 1198 | `	int nIt = 0;` |
|    13 | 1199 | `	SXUNUSED(nArg);` |
|    13 | 1200 | `	SXUNUSED(apArg);` |
|    27 | 1201 | `	PH7_MemObjInit(pVm,&sOut);` |
|    27 | 1202 | `	if( PH7_MemObjToHashmap(&sOut) != SXRET_OK ){` |
|   ! 0 | 1203 | `		PH7_MemObjRelease(&sOut);` |
|   ! 0 | 1204 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 1205 | `	}` |
|     - | 1206 | `	/* [0] the flags word */` |
|    27 | 1207 | `	PH7_MemObjInitFromInt(pVm,&sVal,pThis ? PH7_NativeAttrInt(pThis,SPL_F) : 0);` |
|    27 | 1208 | `	ph7_array_add_elem(&sOut,0,&sVal);` |
|    27 | 1209 | `	PH7_MemObjRelease(&sVal);` |
|     - | 1210 | `	/* [1] the storage */` |
|    27 | 1211 | `	pStore = SplStoreSlot(pVm,pThis);` |
|    27 | 1212 | `	if( pStore ){` |
|    27 | 1213 | `		ph7_array_add_elem(&sOut,0,pStore);` |
|    13 | 1214 | `	}` |
|     - | 1215 | `	/* [2] the instance's own properties */` |
|    27 | 1216 | `	if( SplMembersOf(pVm,pThis,&sVal) != SXRET_OK ){` |
|   ! 0 | 1217 | `		PH7_MemObjRelease(&sOut);` |
|   ! 0 | 1218 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 1219 | `	}` |
|    27 | 1220 | `	ph7_array_add_elem(&sOut,0,&sVal);` |
|    27 | 1221 | `	PH7_MemObjRelease(&sVal);` |
|     - | 1222 | `	/* [3] the iterator class, NULL for the default one and for ArrayIterator */` |
|    27 | 1223 | `	if( pThis ){` |
|    27 | 1224 | `		PH7_NativeAttrStr(pThis,SPL_IT,&zIt,&nIt);` |
|    13 | 1225 | `	}` |
|    27 | 1226 | `	PH7_MemObjInit(pVm,&sVal);` |
|    27 | 1227 | `	if( nIt > 0 && (nIt != (int)sizeof("ArrayIterator")-1` |
|    14 | 1228 | `	 \|\| SyMemcmp(zIt,"ArrayIterator",sizeof("ArrayIterator")-1) != 0) ){` |
|     5 | 1229 | `		PH7_MemObjStringAppend(&sVal,zIt,(sxu32)nIt);` |
|     2 | 1230 | `	}` |
|    27 | 1231 | `	ph7_array_add_elem(&sOut,0,&sVal);` |
|    27 | 1232 | `	PH7_MemObjRelease(&sVal);` |
|    27 | 1233 | `	ph7_result_value(pCtx,&sOut);` |
|    27 | 1234 | `	PH7_MemObjRelease(&sOut);` |
|    27 | 1235 | `	return PH7_OK;` |
|    14 | 1236 | `}` |
|    36 | 1237 | `static int vm_builtin_SplStore_unserializeMagic(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1238 | `{` |
|    37 | 1239 | `	ph7_vm *pVm = pCtx->pVm;` |
|    37 | 1240 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 1241 | `	ph7_hashmap *pData;` |
|    37 | 1242 | `	ph7_hashmap_node *pNode = 0;` |
|    37 | 1243 | `	ph7_value *pFlags,*pStorage,*pMembers,*pIt = 0;` |
|     - | 1244 | `	sxi32 rc;` |
|    37 | 1245 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     - | 1246 | `		char zBuf[64];` |
|   ! 0 | 1247 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 1248 | `			"%s(): Argument #1 ($data) must be of type array, %s given",` |
|   ! 0 | 1249 | `			ph7_function_name(pCtx),` |
|   ! 0 | 1250 | `			nArg > 0 ? VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)) : "none");` |
|     - | 1251 | `	}` |
|    37 | 1252 | `	if( pThis == 0 ){` |
|   ! 0 | 1253 | `		return PH7_OK;` |
|     - | 1254 | `	}` |
|    37 | 1255 | `	pData = (ph7_hashmap *)apArg[0]->x.pOther;` |
|    37 | 1256 | `	if( HashmapLookupIntKey(pData,0,&pNode) != SXRET_OK ){` |
|   ! 0 | 1257 | `		return SplStoreIllTyped(pCtx);` |
|     - | 1258 | `	}` |
|    37 | 1259 | `	pFlags = HashmapExtractNodeValue(pNode);` |
|    37 | 1260 | `	pNode = 0;` |
|    37 | 1261 | `	if( HashmapLookupIntKey(pData,1,&pNode) != SXRET_OK ){` |
|   ! 0 | 1262 | `		return SplStoreIllTyped(pCtx);` |
|     - | 1263 | `	}` |
|    37 | 1264 | `	pStorage = HashmapExtractNodeValue(pNode);` |
|    37 | 1265 | `	pNode = 0;` |
|    37 | 1266 | `	if( HashmapLookupIntKey(pData,2,&pNode) != SXRET_OK ){` |
|     3 | 1267 | `		return SplStoreIllTyped(pCtx);` |
|     - | 1268 | `	}` |
|    35 | 1269 | `	pMembers = HashmapExtractNodeValue(pNode);` |
|    35 | 1270 | `	pNode = 0;` |
|    35 | 1271 | `	if( HashmapLookupIntKey(pData,3,&pNode) == SXRET_OK ){` |
|    27 | 1272 | `		pIt = HashmapExtractNodeValue(pNode);` |
|    13 | 1273 | `	}` |
|    34 | 1274 | `	if( pFlags == 0 \|\| (pFlags->iFlags & MEMOBJ_INT) == 0` |
|    33 | 1275 | `	 \|\| pMembers == 0 \|\| (pMembers->iFlags & MEMOBJ_HASHMAP) == 0` |
|    32 | 1276 | `	 \|\| (pIt != 0 && (pIt->iFlags & (MEMOBJ_NULL\|MEMOBJ_STRING)) == 0) ){` |
|     7 | 1277 | `		return SplStoreIllTyped(pCtx);` |
|     - | 1278 | `	}` |
|     - | 1279 | `	/* php's own wording, and its own exception CLASS, for the storage slot. */` |
|    29 | 1280 | `	if( pStorage == 0 \|\| (pStorage->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 ){` |
|     3 | 1281 | `		return PH7_VmThrowException(pCtx,"InvalidArgumentException",` |
|     - | 1282 | `			"Passed variable is not an array or object");` |
|     - | 1283 | `	}` |
|    27 | 1284 | `	if( pIt != 0 && (pIt->iFlags & MEMOBJ_STRING) != 0 && SyBlobLength(&pIt->sBlob) > 0 ){` |
|    11 | 1285 | `		const char *zIt = (const char *)SyBlobData(&pIt->sBlob);` |
|    11 | 1286 | `		int nIt = (int)SyBlobLength(&pIt->sBlob);` |
|    11 | 1287 | `		ph7_class *pClass = PH7_VmExtractClass(pVm,zIt,(sxu32)nIt,FALSE,0);` |
|    11 | 1288 | `		ph7_class *pIface = PH7_VmExtractClass(pVm,"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|    11 | 1289 | `		if( pClass == 0 ){` |
|     4 | 1290 | `			return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     - | 1291 | `				"Cannot deserialize ArrayObject with iterator class '%.*s'; "` |
|     1 | 1292 | `				"no such class exists",nIt,zIt);` |
|     - | 1293 | `		}` |
|     9 | 1294 | `		if( pIface == 0 \|\| !PH7_VmInstanceOf(pClass,pIface) ){` |
|     7 | 1295 | `			return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     - | 1296 | `				"Cannot deserialize ArrayObject with iterator class '%.*s'; "` |
|     2 | 1297 | `				"this class does not implement the Iterator interface",nIt,zIt);` |
|     - | 1298 | `		}` |
|     5 | 1299 | `		if( PH7_NativeAttr(pThis,SPL_IT) ){` |
|     5 | 1300 | `			PH7_NativeSetAttrStr(pVm,pThis,SPL_IT,zIt,nIt);` |
|     2 | 1301 | `		}` |
|     2 | 1302 | `	}` |
|    21 | 1303 | `	PH7_NativeSetAttrInt(pVm,pThis,SPL_F,ph7_value_to_int64(pFlags) & SPL_FLAG_MASK);` |
|    21 | 1304 | `	rc = SplInitStore(pCtx,pThis,pStorage,"ArrayObject::__unserialize");` |
|    21 | 1305 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 1306 | `		return rc;` |
|     - | 1307 | `	}` |
|    21 | 1308 | `	SplMembersLoad(pThis,pMembers);` |
|    21 | 1309 | `	return PH7_OK;` |
|    19 | 1310 | `}` |
|     - | 1311 | `/*` |
|     - | 1312 | ` * ---------------------------------------------------------------------------` |
|     - | 1313 | ` * php's presentation for the array store (ph7_class::xPresent).` |
|     - | 1314 | ` *` |
|     - | 1315 | ` * php has two handlers here and they DISAGREE, which is the whole reason the hook` |
|     - | 1316 | `` * is told which is asking. `spl_array_get_debug_info` always shows ONE entry —`` |
|     - | 1317 | ` * the storage under its MANGLED private name — after whatever real properties the` |
|     - | 1318 | `` * instance has; `spl_array_get_properties_for` answers the storage's ELEMENTS`` |
|     - | 1319 | `` * directly for the var_export / (array) / json purposes, with no `storage` key at`` |
|     - | 1320 | ` * all, and hands back the ordinary property table when STD_PROP_LIST is set. The` |
|     - | 1321 | ` * flag is therefore visible on one surface and invisible on the other: a` |
|     - | 1322 | ` * STD_PROP_LIST ArrayObject still var_dumps its storage.` |
|     - | 1323 | ` *` |
|     - | 1324 | ` * The mangled name always spells the ROOT class, never the receiver's: a` |
|     - | 1325 | `` * RecursiveArrayIterator shows `["storage":"ArrayIterator":private]`.`` |
|     - | 1326 | ` * ---------------------------------------------------------------------------` |
|     - | 1327 | ` */` |
|    12 | 1328 | `static int SplStoreMangledKey(ph7_class_instance *pThis,char *zBuf,int nBuf)` |
|     1 | 1329 | `{` |
|    13 | 1330 | `	const char *zRoot = "ArrayObject";` |
|     - | 1331 | `	ph7_class *pClass;` |
|    13 | 1332 | `	int nRoot,nOut = 0;` |
|    25 | 1333 | `	for( pClass = pThis->pClass ; pClass ; pClass = pClass->pBase ){` |
|    16 | 1334 | `		if( pClass->sName.nByte == sizeof("ArrayIterator")-1` |
|    11 | 1335 | `		 && SyMemcmp(pClass->sName.zString,"ArrayIterator",sizeof("ArrayIterator")-1) == 0 ){` |
|     5 | 1336 | `			zRoot = "ArrayIterator";` |
|     5 | 1337 | `			break;` |
|     - | 1338 | `		}` |
|     7 | 1339 | `	}` |
|    13 | 1340 | `	nRoot = (int)SyStrlen(zRoot);` |
|    13 | 1341 | `	if( nRoot + (int)sizeof("\0\0storage") > nBuf ){` |
|   ! 0 | 1342 | `		return 0;` |
|     - | 1343 | `	}` |
|    13 | 1344 | `	zBuf[nOut++] = 0;` |
|    13 | 1345 | `	SyMemcpy(zRoot,&zBuf[nOut],(sxu32)nRoot);` |
|    13 | 1346 | `	nOut += nRoot;` |
|    13 | 1347 | `	zBuf[nOut++] = 0;` |
|    13 | 1348 | `	SyMemcpy("storage",&zBuf[nOut],sizeof("storage")-1);` |
|    13 | 1349 | `	nOut += (int)sizeof("storage")-1;` |
|    13 | 1350 | `	return nOut;` |
|     7 | 1351 | `}` |
|    24 | 1352 | `static int SplPresentWalk(ph7_value *pKey,ph7_value *pVal,void *pUserData)` |
|     2 | 1353 | `{` |
|    26 | 1354 | `	ph7_array_add_elem((ph7_value *)pUserData,pKey,pVal);` |
|    26 | 1355 | `	return PH7_OK;` |
|     2 | 1356 | `}` |
|    36 | 1357 | `static sxi32 SplStorePresent(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)` |
|     2 | 1358 | `{` |
|    38 | 1359 | `	ph7_value *pStore = pThis ? PH7_NativeAttr(pThis,SPL_D) : 0;` |
|    38 | 1360 | `	if( bDebug ){` |
|     - | 1361 | `		ph7_value sKey;` |
|     - | 1362 | `		char zKey[64];` |
|     - | 1363 | `		int nKey;` |
|     - | 1364 | `		/* The instance's OWN properties come first — php's debug info starts from` |
|     - | 1365 | `		 * the standard table and appends the storage entry to it. */` |
|    13 | 1366 | `		SplAddMembers(&(*pVm),pThis,pOut);` |
|    13 | 1367 | `		nKey = SplStoreMangledKey(pThis,zKey,(int)sizeof(zKey));` |
|    13 | 1368 | `		if( nKey > 0 && pStore ){` |
|    13 | 1369 | `			PH7_MemObjInitFromString(&(*pVm),&sKey,0);` |
|    13 | 1370 | `			PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKey);` |
|    13 | 1371 | `			ph7_array_add_elem(pOut,&sKey,pStore);` |
|    13 | 1372 | `			PH7_MemObjRelease(&sKey);` |
|     6 | 1373 | `		}` |
|    13 | 1374 | `		return SXRET_OK;` |
|     - | 1375 | `	}` |
|    26 | 1376 | `	if( (PH7_NativeAttrInt(pThis,SPL_F) & SPL_STD_PROP_LIST) != 0 ){` |
|     5 | 1377 | `		SplAddMembers(&(*pVm),pThis,pOut);` |
|     5 | 1378 | `		return SXRET_OK;` |
|     - | 1379 | `	}` |
|    22 | 1380 | `	if( pStore && (pStore->iFlags & MEMOBJ_HASHMAP) != 0 ){` |
|    22 | 1381 | `		ph7_array_walk(pStore,SplPresentWalk,pOut);` |
|    10 | 1382 | `	}` |
|    22 | 1383 | `	return SXRET_OK;` |
|    20 | 1384 | `}` |
|    16 | 1385 | `static int vm_builtin_ArrayObject_getIterator(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 1386 | `{` |
|    18 | 1387 | `	ph7_vm *pVm = pCtx->pVm;` |
|    18 | 1388 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 1389 | `	ph7_class_instance *pIt;` |
|     - | 1390 | `	ph7_class *pClass;` |
|    18 | 1391 | `	const char *zName = 0;` |
|    18 | 1392 | `	int nName = 0;` |
|     - | 1393 | `	ph7_value *pSlot;` |
|     - | 1394 | `	ph7_class_method *pCons;` |
|     8 | 1395 | `	SXUNUSED(nArg);` |
|     8 | 1396 | `	SXUNUSED(apArg);` |
|    18 | 1397 | `	if( pThis == 0 ){` |
|   ! 0 | 1398 | `		return PH7_OK;` |
|     - | 1399 | `	}` |
|    18 | 1400 | `	PH7_NativeAttrStr(pThis,SPL_IT,&zName,&nName);` |
|    18 | 1401 | `	if( nName < 1 ){` |
|   ! 0 | 1402 | `		zName = "ArrayIterator";` |
|   ! 0 | 1403 | `		nName = (int)sizeof("ArrayIterator")-1;` |
|   ! 0 | 1404 | `	}` |
|    18 | 1405 | `	pClass = PH7_VmExtractClass(pVm,zName,(sxu32)nName,FALSE,0);` |
|    18 | 1406 | `	if( pClass == 0 ){` |
|   ! 0 | 1407 | `		return PH7_OK;` |
|     - | 1408 | `	}` |
|    18 | 1409 | `	pIt = PH7_NewClassInstance(pVm,pClass);` |
|    18 | 1410 | `	if( pIt == 0 ){` |
|   ! 0 | 1411 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 1412 | `	}` |
|    18 | 1413 | `	pIt->iRef++;` |
|    18 | 1414 | `	pSlot = SplStoreSlot(pVm,pThis);` |
|    18 | 1415 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|    18 | 1416 | `	if( pCons && pSlot ){` |
|     - | 1417 | ``		/* `new $c($this->__d)`: the iterator gets a COPY of the store, as the PHP did —`` |
|     - | 1418 | `		 * a user subclass of ArrayIterator runs its own constructor here. */` |
|     - | 1419 | `		ph7_value *apCtor[1];` |
|    18 | 1420 | `		apCtor[0] = pSlot;` |
|    18 | 1421 | `		PH7_VmCallClassMethod(pVm,pIt,pCons,0,1,apCtor);` |
|     8 | 1422 | `	}` |
|    18 | 1423 | `	PH7_NativeResultObject(pCtx,pIt);` |
|    18 | 1424 | `	PH7_ClassInstanceUnref(pIt);` |
|    18 | 1425 | `	return PH7_OK;` |
|    10 | 1426 | `}` |
|     - | 1427 | `/*` |
|     - | 1428 | ` * ARRAY_AS_PROPS (flag 2) reaches the store through the four magic accessors, which is how` |
|     - | 1429 | ` * the PHP did it. php has no such methods — it implements the flag in its property handler,` |
|     - | 1430 | `` * so `getMethods()` does not list them (a surface divergence carried over, §7.4).`` |
|     - | 1431 | ` */` |
|     4 | 1432 | `static int vm_builtin_ArrayObject_get(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1433 | `{` |
|     5 | 1434 | `	ph7_vm *pVm = pCtx->pVm;` |
|     5 | 1435 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 1436 | `	ph7_hashmap *pMap;` |
|     5 | 1437 | `	ph7_hashmap_node *pNode = 0;` |
|     5 | 1438 | `	if( pThis == 0 \|\| nArg < 1 \|\| (PH7_NativeAttrInt(pThis,SPL_F) & 2) == 0 ){` |
|   ! 0 | 1439 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1440 | `		return PH7_OK;` |
|     - | 1441 | `	}` |
|     5 | 1442 | `	pMap = SplStore(pVm,pThis);` |
|     5 | 1443 | `	if( pMap && PH7_HashmapLookup(pMap,apArg[0],&pNode) == SXRET_OK ){` |
|     5 | 1444 | `		ph7_result_value(pCtx,(ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx));` |
|     5 | 1445 | `		return PH7_OK;` |
|     - | 1446 | `	}` |
|     - | 1447 | ``	/* `?? null`: a missing key must not raise the undefined-key warning from in here —`` |
|     - | 1448 | `	 * php reports the missing PROPERTY, and PHL's magic-read path already does. */` |
|   ! 0 | 1449 | `	ph7_result_null(pCtx);` |
|   ! 0 | 1450 | `	return PH7_OK;` |
|     3 | 1451 | `}` |
|     4 | 1452 | `static int vm_builtin_ArrayObject_set(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1453 | `{` |
|     5 | 1454 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 1455 | `	ph7_hashmap *pMap;` |
|     5 | 1456 | `	if( pThis == 0 \|\| nArg < 2 \|\| (PH7_NativeAttrInt(pThis,SPL_F) & 2) == 0 ){` |
|   ! 0 | 1457 | `		return PH7_OK;` |
|     - | 1458 | `	}` |
|     5 | 1459 | `	pMap = SplStore(pCtx->pVm,pThis);` |
|     5 | 1460 | `	if( pMap ){` |
|     5 | 1461 | `		PH7_HashmapInsert(pMap,apArg[0],apArg[1]);` |
|     2 | 1462 | `	}` |
|     5 | 1463 | `	return PH7_OK;` |
|     3 | 1464 | `}` |
|    10 | 1465 | `static int vm_builtin_ArrayObject_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1466 | `{` |
|    11 | 1467 | `	ph7_vm *pVm = pCtx->pVm;` |
|    11 | 1468 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 1469 | `	ph7_hashmap *pMap;` |
|    11 | 1470 | `	ph7_hashmap_node *pNode = 0;` |
|    11 | 1471 | `	int bSet = 0;` |
|    11 | 1472 | `	if( pThis && nArg > 0 && (PH7_NativeAttrInt(pThis,SPL_F) & 2) ){` |
|    11 | 1473 | `		pMap = SplStore(pVm,pThis);` |
|    11 | 1474 | `		if( pMap && PH7_HashmapLookup(pMap,apArg[0],&pNode) == SXRET_OK ){` |
|     5 | 1475 | `			ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);` |
|     5 | 1476 | `			bSet = pVal && (pVal->iFlags & MEMOBJ_NULL) == 0; /* isset(), not exists */` |
|     2 | 1477 | `		}` |
|     5 | 1478 | `	}` |
|    11 | 1479 | `	ph7_result_bool(pCtx,bSet);` |
|    11 | 1480 | `	return PH7_OK;` |
|     1 | 1481 | `}` |
|     2 | 1482 | `static int vm_builtin_ArrayObject_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1483 | `{` |
|     3 | 1484 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 1485 | `	ph7_hashmap *pMap;` |
|     3 | 1486 | `	ph7_hashmap_node *pNode = 0;` |
|     3 | 1487 | `	if( pThis && nArg > 0 && (PH7_NativeAttrInt(pThis,SPL_F) & 2) ){` |
|     3 | 1488 | `		pMap = SplStore(pCtx->pVm,pThis);` |
|     3 | 1489 | `		if( pMap && PH7_HashmapLookup(pMap,apArg[0],&pNode) == SXRET_OK ){` |
|     3 | 1490 | `			PH7_HashmapUnlinkNode(pNode,TRUE);` |
|     1 | 1491 | `		}` |
|     1 | 1492 | `	}` |
|     3 | 1493 | `	return PH7_OK;` |
|     1 | 1494 | `}` |
|     - | 1495 | `/*` |
|     - | 1496 | ` * Declare both classes plus SeekableIterator, which ArrayIterator implements and which` |
|     - | 1497 | ` * therefore cannot wait for the chunk. RecursiveArrayIterator still lives there and extends` |
|     - | 1498 | ` * ArrayIterator, so this install has to run BEFORE the chunk is evaluated.` |
|     - | 1499 | ` */` |
|  5146 | 1500 | `static sxi32 VmInstallSplStore(ph7_vm *pVm)` |
|     5 | 1501 | `{` |
|     - | 1502 | `	static const PH7_NativeMethodDef aSeekMethod[] = {` |
|     - | 1503 | `		{ "seek", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "int $offset", 0, 0 },` |
|     - | 1504 | `	};` |
|     - | 1505 | `	static const PH7_NativePropDef aItProp[] = {` |
|     - | 1506 | `		{ SPL_D, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - | 1507 | `		{ SPL_F, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - | 1508 | `	};` |
|     - | 1509 | `	static const PH7_NativePropDef aObjProp[] = {` |
|     - | 1510 | `		{ SPL_D,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - | 1511 | `		{ SPL_F,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - | 1512 | `		{ SPL_IT, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "ArrayIterator", 0.0 }, 0 },` |
|     - | 1513 | `	};` |
|     - | 1514 | `	static const PH7_NativeConstDef aConst[] = {` |
|     - | 1515 | `		{ "STD_PROP_LIST",  PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 1, 0, 0.0 },` |
|     - | 1516 | `		{ "ARRAY_AS_PROPS", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2, 0, 0.0 },` |
|     - | 1517 | `	};` |
|     - | 1518 | `	/* php's declaration order, which is the order Reflection reports. */` |
|     - | 1519 | `	static const PH7_NativeMethodDef aItMethod[] = {` |
|     - | 1520 | `		{ "__construct",  PH7_MOD_PUBLIC, "object\|array $array = [], int $flags = 0", 0,` |
|     - | 1521 | `		  vm_builtin_ArrayIterator_construct },` |
|     - | 1522 | `		{ "offsetExists", PH7_MOD_PUBLIC, "mixed $key", "@bool", vm_builtin_SplStore_offsetExists },` |
|     - | 1523 | `		{ "offsetGet",    PH7_MOD_PUBLIC, "mixed $key", "@mixed", vm_builtin_SplStore_offsetGet },` |
|     - | 1524 | `		{ "offsetSet",    PH7_MOD_PUBLIC, "mixed $key, mixed $value", "@void", vm_builtin_SplStore_offsetSet },` |
|     - | 1525 | `		{ "offsetUnset",  PH7_MOD_PUBLIC, "mixed $key", "@void", vm_builtin_SplStore_offsetUnset },` |
|     - | 1526 | `		{ "append",       PH7_MOD_PUBLIC, "mixed $value", "@void", vm_builtin_SplStore_append },` |
|     - | 1527 | `		{ "getArrayCopy", PH7_MOD_PUBLIC, "", "@array", vm_builtin_SplStore_getArrayCopy },` |
|     - | 1528 | `		{ "count",        PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplStore_count },` |
|     - | 1529 | `		{ "getFlags",     PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplStore_getFlags },` |
|     - | 1530 | `		{ "setFlags",     PH7_MOD_PUBLIC, "int $flags", "@void", vm_builtin_SplStore_setFlags },` |
|     - | 1531 | `		{ "asort",        PH7_MOD_PUBLIC, "int $flags = 0", "@true", vm_builtin_SplStore_asort },` |
|     - | 1532 | `		{ "ksort",        PH7_MOD_PUBLIC, "int $flags = 0", "@true", vm_builtin_SplStore_ksort },` |
|     - | 1533 | `		{ "uasort",       PH7_MOD_PUBLIC, "callable $callback", "@true", vm_builtin_SplStore_uasort },` |
|     - | 1534 | `		{ "uksort",       PH7_MOD_PUBLIC, "callable $callback", "@true", vm_builtin_SplStore_uksort },` |
|     - | 1535 | `		{ "natsort",      PH7_MOD_PUBLIC, "", "@true", vm_builtin_SplStore_natsort },` |
|     - | 1536 | `		{ "natcasesort",  PH7_MOD_PUBLIC, "", "@true", vm_builtin_SplStore_natcasesort },` |
|     - | 1537 | `		{ "current",      PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_ArrayIterator_current },` |
|     - | 1538 | `		{ "key",          PH7_MOD_PUBLIC, "", "@string\|int\|null", vm_builtin_ArrayIterator_key },` |
|     - | 1539 | `		{ "next",         PH7_MOD_PUBLIC, "", "@void", vm_builtin_ArrayIterator_next },` |
|     - | 1540 | `		{ "rewind",       PH7_MOD_PUBLIC, "", "@void", vm_builtin_ArrayIterator_rewind },` |
|     - | 1541 | `		{ "valid",        PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ArrayIterator_valid },` |
|     - | 1542 | `		{ "seek",         PH7_MOD_PUBLIC, "int $offset", "@void", vm_builtin_ArrayIterator_seek },` |
|     - | 1543 | `		{ "__serialize",  PH7_MOD_PUBLIC, "", "@array", vm_builtin_SplStore_serializeMagic },` |
|     - | 1544 | `		{ "__unserialize", PH7_MOD_PUBLIC, "array $data", "@void",` |
|     - | 1545 | `		  vm_builtin_SplStore_unserializeMagic },` |
|     - | 1546 | `	};` |
|     - | 1547 | `	static const PH7_NativeMethodDef aObjMethod[] = {` |
|     - | 1548 | `		{ "__construct",      PH7_MOD_PUBLIC,` |
|     - | 1549 | `		  "object\|array $array = [], int $flags = 0, string $iteratorClass = 'ArrayIterator'", 0,` |
|     - | 1550 | `		  vm_builtin_ArrayObject_construct },` |
|     - | 1551 | `		{ "offsetExists",     PH7_MOD_PUBLIC, "mixed $key", "@bool", vm_builtin_SplStore_offsetExists },` |
|     - | 1552 | `		{ "offsetGet",        PH7_MOD_PUBLIC, "mixed $key", "@mixed", vm_builtin_SplStore_offsetGet },` |
|     - | 1553 | `		{ "offsetSet",        PH7_MOD_PUBLIC, "mixed $key, mixed $value", "@void", vm_builtin_SplStore_offsetSet },` |
|     - | 1554 | `		{ "offsetUnset",      PH7_MOD_PUBLIC, "mixed $key", "@void", vm_builtin_SplStore_offsetUnset },` |
|     - | 1555 | `		{ "append",           PH7_MOD_PUBLIC, "mixed $value", "@void", vm_builtin_SplStore_append },` |
|     - | 1556 | `		{ "getArrayCopy",     PH7_MOD_PUBLIC, "", "@array", vm_builtin_SplStore_getArrayCopy },` |
|     - | 1557 | `		{ "count",            PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplStore_count },` |
|     - | 1558 | `		{ "getFlags",         PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplStore_getFlags },` |
|     - | 1559 | `		{ "setFlags",         PH7_MOD_PUBLIC, "int $flags", "@void", vm_builtin_SplStore_setFlags },` |
|     - | 1560 | `		{ "asort",            PH7_MOD_PUBLIC, "int $flags = 0", "@true", vm_builtin_SplStore_asort },` |
|     - | 1561 | `		{ "ksort",            PH7_MOD_PUBLIC, "int $flags = 0", "@true", vm_builtin_SplStore_ksort },` |
|     - | 1562 | `		{ "uasort",           PH7_MOD_PUBLIC, "callable $callback", "@true", vm_builtin_SplStore_uasort },` |
|     - | 1563 | `		{ "uksort",           PH7_MOD_PUBLIC, "callable $callback", "@true", vm_builtin_SplStore_uksort },` |
|     - | 1564 | `		{ "natsort",          PH7_MOD_PUBLIC, "", "@true", vm_builtin_SplStore_natsort },` |
|     - | 1565 | `		{ "natcasesort",      PH7_MOD_PUBLIC, "", "@true", vm_builtin_SplStore_natcasesort },` |
|     - | 1566 | `		{ "exchangeArray",    PH7_MOD_PUBLIC, "object\|array $array", "@array", vm_builtin_ArrayObject_exchangeArray },` |
|     - | 1567 | `		{ "getIterator",      PH7_MOD_PUBLIC, "", "@Iterator", vm_builtin_ArrayObject_getIterator },` |
|     - | 1568 | `		{ "setIteratorClass", PH7_MOD_PUBLIC, "string $iteratorClass", "@void", vm_builtin_ArrayObject_setIteratorClass },` |
|     - | 1569 | `		{ "getIteratorClass", PH7_MOD_PUBLIC, "", "@string", vm_builtin_ArrayObject_getIteratorClass },` |
|     - | 1570 | `		{ "__get",            PH7_MOD_PUBLIC, "$name", 0, vm_builtin_ArrayObject_get },` |
|     - | 1571 | `		{ "__set",            PH7_MOD_PUBLIC, "$name, $value", 0, vm_builtin_ArrayObject_set },` |
|     - | 1572 | `		{ "__isset",          PH7_MOD_PUBLIC, "$name", 0, vm_builtin_ArrayObject_isset },` |
|     - | 1573 | `		{ "__unset",          PH7_MOD_PUBLIC, "$name", 0, vm_builtin_ArrayObject_unset },` |
|     - | 1574 | `		{ "__serialize",      PH7_MOD_PUBLIC, "", "@array", vm_builtin_SplStore_serializeMagic },` |
|     - | 1575 | `		{ "__unserialize",    PH7_MOD_PUBLIC, "array $data", "@void",` |
|     - | 1576 | `		  vm_builtin_SplStore_unserializeMagic },` |
|     - | 1577 | `	};` |
|     - | 1578 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - | 1579 | ``		/* `interface X extends Iterator` is a PARENT, not an implemented interface:`` |
|     - | 1580 | `		 * the compiler puts it in pBase and Reflection walks pBase to answer which` |
|     - | 1581 | `		 * class DECLARED an inherited method. Naming it in zImplements instead made` |
|     - | 1582 | `		 * current()/key()/next()/rewind()/valid() report this interface as their` |
|     - | 1583 | `		 * declaring class where php reports Iterator. */` |
|     - | 1584 | `		{ "SeekableIterator", "Iterator", 0, PH7_CLASS_INTERFACE,` |
|     - | 1585 | `		  aSeekMethod, SX_ARRAYSIZE(aSeekMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - | 1586 | `		/* PH7_CLASS_DIM_WRITABLE: php's spl_array read_dimension hands back the` |
|     - | 1587 | ``		 * REAL element for a write fetch, so `$ao['k']['n'] = v` lands — unlike`` |
|     - | 1588 | `		 * SplFixedArray / SplDoublyLinkedList / SplObjectStorage, which keep the` |
|     - | 1589 | `		 * standard handler and get php's indirect-modification notice. */` |
|     - | 1590 | `		{ "ArrayIterator", 0, "SeekableIterator,ArrayAccess,Countable", PH7_CLASS_DIM_WRITABLE,` |
|     - | 1591 | `		  aItMethod, SX_ARRAYSIZE(aItMethod), aConst, SX_ARRAYSIZE(aConst),` |
|     - | 1592 | `		  aItProp, SX_ARRAYSIZE(aItProp), 0, 0, SplStorePresent },` |
|     - | 1593 | `		{ "ArrayObject", 0, "IteratorAggregate,ArrayAccess,Countable", PH7_CLASS_DIM_WRITABLE,` |
|     - | 1594 | `		  aObjMethod, SX_ARRAYSIZE(aObjMethod), aConst, SX_ARRAYSIZE(aConst),` |
|     - | 1595 | `		  aObjProp, SX_ARRAYSIZE(aObjProp), 0, 0, SplStorePresent },` |
|     - | 1596 | `	};` |
|  5151 | 1597 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|     5 | 1598 | `}` |
|     - | 1599 | `/*` |
|     - | 1600 | ` * ---------------------------------------------------------------------------` |
|     - | 1601 | ` * The SPL DUAL ITERATORS: IteratorIterator and the decorators built on it.` |
|     - | 1602 | ` *` |
|     - | 1603 | `` * php's `spl_dual_it_object` is a CACHE, and that is the whole design. rewind()`` |
|     - | 1604 | ` * and next() move the INNER iterator and then COPY its current()/key() onto the` |
|     - | 1605 | ` * decorator; valid(), current() and key() answer out of that copy and never reach` |
|     - | 1606 | ` * the inner iterator again. The chunk forwarded all five live, which is three` |
|     - | 1607 | ` * observable divergences at once: a fresh decorator was valid() BEFORE rewind()` |
|     - | 1608 | ` * (php answers false — nothing has been fetched yet), current() followed an inner` |
|     - | 1609 | ` * iterator that had been moved behind the decorator's back (php answers what it` |
|     - | 1610 | ` * cached), and a decorator left past the end still answered the inner's stale` |
|     - | 1611 | ` * key(). Everything below is written around the cache because the cache IS the` |
|     - | 1612 | ` * class.` |
|     - | 1613 | ` *` |
|     - | 1614 | `` * Two slots hold what php holds in two fields: `__in` is `inner.zobject` — the`` |
|     - | 1615 | `` * object getInnerIterator() answers — and `__it` is `inner.iterator`, the Iterator`` |
|     - | 1616 | ` * actually driven. They differ for exactly one input: an IteratorAggregate whose` |
|     - | 1617 | ` * getIterator() answers another IteratorAggregate. php unwraps ONE level in the` |
|     - | 1618 | ` * constructor and lets the engine's get_iterator handler unwrap the rest at` |
|     - | 1619 | `` * iteration time, so `new IteratorIterator($aggOfAgg)` answers the inner AGGREGATE`` |
|     - | 1620 | `` * from getInnerIterator() and still iterates. The chunk's `while` loop unwrapped`` |
|     - | 1621 | ` * to the bottom and answered the ArrayIterator instead.` |
|     - | 1622 | ` */` |
|     - | 1623 | `#define IT_IN  "__in"   /* php's inner.zobject: what getInnerIterator() answers */` |
|     - | 1624 | `#define IT_IT  "__it"   /* php's inner.iterator: the Iterator actually driven */` |
|     - | 1625 | `#define IT_CD  "__cd"   /* the cached current() */` |
|     - | 1626 | `#define IT_CK  "__ck"   /* the cached key() */` |
|     - | 1627 | `#define IT_CF  "__cf"   /* 1 while the cached pair is live (php's IS_UNDEF check) */` |
|     - | 1628 | `#define IT_CP  "__cp"   /* php's current.pos */` |
|     - | 1629 | `#define IT_OFF "__off"  /* LimitIterator's offset */` |
|     - | 1630 | `#define IT_LIM "__lim"  /* LimitIterator's count, -1 for "all" */` |
|     - | 1631 | `#define IT_CB  "__cb"   /* CallbackFilterIterator's callback */` |
|     - | 1632 | ``#define AP_LIST "__ai"  /* AppendIterator's php `u.append.zarrayit`: the real ArrayIterator`` |
|     - | 1633 | `                         * holding everything append()ed, whose OWN cursor is php's` |
|     - | 1634 | ``                         * `u.append.iterator` -- one position, which is why`` |
|     - | 1635 | `                         * getArrayIterator()->rewind() moves getIteratorIndex(). */` |
|     - | 1636 |  |
|     - | 1637 | `/*` |
|     - | 1638 | ` * php's SPL_FETCH_AND_CHECK_DUAL_IT: a subclass whose constructor never called` |
|     - | 1639 | ` * parent::__construct() has no inner iterator, and php refuses every method on it` |
|     - | 1640 | ` * rather than answering a null-flavoured nothing.` |
|     - | 1641 | ` */` |
|     2 | 1642 | `static sxi32 DualNotReady(ph7_context *pCtx)` |
|     1 | 1643 | `{` |
|     3 | 1644 | `	return PH7_VmThrowException(pCtx,"Error",` |
|     - | 1645 | `		"The object is in an invalid state as the parent constructor was not called");` |
|     1 | 1646 | `}` |
|  2976 | 1647 | `static ph7_class_instance * DualDriver(ph7_class_instance *pThis)` |
|     1 | 1648 | `{` |
|  2977 | 1649 | `	return pThis ? PH7_NativeAttrObj(pThis,IT_IT) : 0;` |
|     1 | 1650 | `}` |
|  1128 | 1651 | `static int DualFilled(ph7_class_instance *pThis)` |
|     1 | 1652 | `{` |
|  1129 | 1653 | `	return pThis && PH7_NativeAttrInt(pThis,IT_CF) != 0;` |
|     1 | 1654 | `}` |
|     - | 1655 | `/*` |
|     - | 1656 | `` * php's SPL_FETCH_AND_CHECK_DUAL_IT tests `dit_type`, which means "the constructor`` |
|     - | 1657 | ` * ran" -- and for every decorator but one that is the same thing as "an inner` |
|     - | 1658 | ` * iterator exists". AppendIterator's constructor takes NO iterator: it builds an` |
|     - | 1659 | ` * empty list and is immediately usable (valid() false, current()/key() null, no` |
|     - | 1660 | ` * refusal), so its readiness lives in the list slot instead.` |
|     - | 1661 | ` */` |
|  1074 | 1662 | `static int DualReady(ph7_class_instance *pThis)` |
|     1 | 1663 | `{` |
|  1667 | 1664 | `	return pThis && (PH7_NativeAttrObj(pThis,IT_IT) != 0` |
|   592 | 1665 | `		\|\| PH7_NativeAttrObj(pThis,AP_LIST) != 0);` |
|     1 | 1666 | `}` |
|     - | 1667 | `/*` |
|     - | 1668 | ` * Assign one of the instance's own slots. The slot pointer is re-resolved here on` |
|     - | 1669 | ` * purpose: it lives inside pVm->aMemObj, a SySet that REALLOCATES as the VM` |
|     - | 1670 | ` * reserves objects, so any pointer taken before a call into user code (and every` |
|     - | 1671 | ` * inner->current() is one) may be stale by the time the call returns.` |
|     - | 1672 | ` */` |
|   836 | 1673 | `static void DualSetSlot(ph7_vm *pVm,ph7_class_instance *pThis,const char *zName,ph7_value *pVal)` |
|     1 | 1674 | `{` |
|   837 | 1675 | `	ph7_value *pSlot = PH7_NativeAttr(pThis,zName);` |
|   418 | 1676 | `	SXUNUSED(pVm);` |
|   837 | 1677 | `	if( pSlot ){` |
|   837 | 1678 | `		PH7_MemObjStore(pVal,pSlot);` |
|   418 | 1679 | `	}` |
|   837 | 1680 | `}` |
|     - | 1681 | `/* php's spl_dual_it_free: drop the cached pair. */` |
|  1106 | 1682 | `static void DualFree(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 | 1683 | `{` |
|     - | 1684 | `	ph7_value *pSlot;` |
|  1107 | 1685 | `	if( pThis == 0 ){` |
|   ! 0 | 1686 | `		return;` |
|     - | 1687 | `	}` |
|  1107 | 1688 | `	pSlot = PH7_NativeAttr(pThis,IT_CD);` |
|  1107 | 1689 | `	if( pSlot ){ PH7_MemObjRelease(pSlot); }` |
|  1107 | 1690 | `	pSlot = PH7_NativeAttr(pThis,IT_CK);` |
|  1107 | 1691 | `	if( pSlot ){ PH7_MemObjRelease(pSlot); }` |
|  1107 | 1692 | `	PH7_NativeSetAttrInt(pVm,pThis,IT_CF,0);` |
|   554 | 1693 | `}` |
|     - | 1694 | `/* Call a zero-argument method on the driven iterator, propagating a throw (rule:` |
|     - | 1695 | ` * a native body that answers PH7_OK with an exception in flight lets the caller` |
|     - | 1696 | ` * carry on). A missing method is the foreach opcode's leniency, not an error. */` |
|  1946 | 1697 | `static sxi32 DualCall(ph7_vm *pVm,ph7_class_instance *pThis,const char *zName,sxu32 nLen,` |
|     - | 1698 | `	ph7_value *pResult)` |
|     1 | 1699 | `{` |
|  1947 | 1700 | `	ph7_class_instance *pIn = DualDriver(pThis);` |
|  1947 | 1701 | `	if( pIn == 0 ){` |
|    41 | 1702 | `		return SXRET_OK;` |
|     - | 1703 | `	}` |
|  1907 | 1704 | `	return VmIterCallMethod(pVm,pIn,zName,nLen,pResult);` |
|   974 | 1705 | `}` |
|     - | 1706 | `/* php's spl_dual_it_valid: the INNER's valid(), not the cache's. */` |
|   658 | 1707 | `static sxi32 DualInnerValid(ph7_vm *pVm,ph7_class_instance *pThis,int *pbValid)` |
|     1 | 1708 | `{` |
|     - | 1709 | `	ph7_value sVal;` |
|     - | 1710 | `	sxi32 rc;` |
|   659 | 1711 | `	*pbValid = 0;` |
|   659 | 1712 | `	PH7_MemObjInit(pVm,&sVal);` |
|   659 | 1713 | `	rc = DualCall(pVm,pThis,"valid",sizeof("valid")-1,&sVal);` |
|   659 | 1714 | `	if( rc == SXRET_OK ){` |
|   659 | 1715 | `		PH7_MemObjToBool(&sVal);          /* a STATUS, not the answer */` |
|   659 | 1716 | `		*pbValid = sVal.x.iVal != 0;` |
|   329 | 1717 | `	}` |
|   659 | 1718 | `	PH7_MemObjRelease(&sVal);` |
|   659 | 1719 | `	return rc;` |
|     1 | 1720 | `}` |
|     - | 1721 | `/*` |
|     - | 1722 | ``  * php's spl_dual_it_fetch: refill the cache from the inner iterator. `bCheckMore` `` |
|     - | 1723 | ` * is php's check_more — false means "the caller already knows the inner is valid",` |
|     - | 1724 | ` * which is how LimitIterator's seek and InfiniteIterator's wrap-around fetch.` |
|     - | 1725 | ` */` |
|   480 | 1726 | `static sxi32 DualFetch(ph7_vm *pVm,ph7_class_instance *pThis,int bCheckMore)` |
|     1 | 1727 | `{` |
|     - | 1728 | `	ph7_value sVal;` |
|     - | 1729 | `	sxi32 rc;` |
|   481 | 1730 | `	int bValid = 1;` |
|   481 | 1731 | `	DualFree(pVm,pThis);` |
|   481 | 1732 | `	if( DualDriver(pThis) == 0 ){` |
|     5 | 1733 | `		return SXRET_OK;` |
|     - | 1734 | `	}` |
|   477 | 1735 | `	if( bCheckMore ){` |
|   361 | 1736 | `		rc = DualInnerValid(pVm,pThis,&bValid);` |
|   361 | 1737 | `		if( rc != SXRET_OK ){` |
|   ! 0 | 1738 | `			return rc;` |
|     - | 1739 | `		}` |
|   180 | 1740 | `	}` |
|   477 | 1741 | `	if( !bValid ){` |
|    85 | 1742 | `		return SXRET_OK;` |
|     - | 1743 | `	}` |
|   393 | 1744 | `	PH7_MemObjInit(pVm,&sVal);` |
|   393 | 1745 | `	rc = DualCall(pVm,pThis,"current",sizeof("current")-1,&sVal);` |
|   393 | 1746 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 1747 | `		PH7_MemObjRelease(&sVal);` |
|   ! 0 | 1748 | `		return rc;` |
|     - | 1749 | `	}` |
|   393 | 1750 | `	DualSetSlot(pVm,pThis,IT_CD,&sVal);` |
|   393 | 1751 | `	PH7_MemObjRelease(&sVal);` |
|   393 | 1752 | `	PH7_MemObjInit(pVm,&sVal);` |
|   393 | 1753 | `	rc = DualCall(pVm,pThis,"key",sizeof("key")-1,&sVal);` |
|   393 | 1754 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 1755 | `		PH7_MemObjRelease(&sVal);` |
|   ! 0 | 1756 | `		DualFree(pVm,pThis);   /* php drops the half-filled pair when key() throws */` |
|   ! 0 | 1757 | `		return rc;` |
|     - | 1758 | `	}` |
|   393 | 1759 | `	DualSetSlot(pVm,pThis,IT_CK,&sVal);` |
|   393 | 1760 | `	PH7_MemObjRelease(&sVal);` |
|   393 | 1761 | `	PH7_NativeSetAttrInt(pVm,pThis,IT_CF,1);` |
|   393 | 1762 | `	return SXRET_OK;` |
|   241 | 1763 | `}` |
|     - | 1764 | `/* php's spl_dual_it_rewind: free, position back to zero, rewind the inner. */` |
|   202 | 1765 | `static sxi32 DualRewindInner(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 | 1766 | `{` |
|   203 | 1767 | `	DualFree(pVm,pThis);` |
|   203 | 1768 | `	PH7_NativeSetAttrInt(pVm,pThis,IT_CP,0);` |
|   203 | 1769 | `	return DualCall(pVm,pThis,"rewind",sizeof("rewind")-1,0);` |
|     1 | 1770 | `}` |
|     - | 1771 | `/* php's spl_dual_it_next: free, advance the inner, count the step. */` |
|   214 | 1772 | `static sxi32 DualNextInner(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 | 1773 | `{` |
|     - | 1774 | `	sxi32 rc;` |
|   215 | 1775 | `	DualFree(pVm,pThis);` |
|   215 | 1776 | `	rc = DualCall(pVm,pThis,"next",sizeof("next")-1,0);` |
|   215 | 1777 | `	PH7_NativeSetAttrInt(pVm,pThis,IT_CP,PH7_NativeAttrInt(pThis,IT_CP)+1);` |
|   215 | 1778 | `	return rc;` |
|     1 | 1779 | `}` |
|     - | 1780 | `/* Hand back a cached slot, or php's null for an empty cache. */` |
|   434 | 1781 | `static int DualResultSlot(ph7_context *pCtx,const char *zName)` |
|     1 | 1782 | `{` |
|   435 | 1783 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 1784 | `	ph7_value *pSlot;` |
|   435 | 1785 | `	if( !DualReady(pThis) ){` |
|   ! 0 | 1786 | `		return DualNotReady(pCtx);` |
|     - | 1787 | `	}` |
|   435 | 1788 | `	if( !DualFilled(pThis) ){` |
|    21 | 1789 | `		ph7_result_null(pCtx);` |
|    21 | 1790 | `		return PH7_OK;` |
|     - | 1791 | `	}` |
|   415 | 1792 | `	pSlot = PH7_NativeAttr(pThis,zName);` |
|   415 | 1793 | `	if( pSlot ){` |
|   415 | 1794 | `		ph7_result_value(pCtx,pSlot);` |
|   207 | 1795 | `	}` |
|   415 | 1796 | `	return PH7_OK;` |
|   218 | 1797 | `}` |
|     - | 1798 | `/*` |
|     - | 1799 | ` * The constructor every dual iterator shares. php words the "already built" refusal` |
|     - | 1800 | ` * with the DECLARING class's name and with getIterator() rather than __construct(),` |
|     - | 1801 | ` * so each class hands its own name in.` |
|     - | 1802 | ` */` |
|   176 | 1803 | `static sxi32 DualConstruct(ph7_context *pCtx,const char *zOwner,int nArg,ph7_value **apArg)` |
|     1 | 1804 | `{` |
|   177 | 1805 | `	ph7_vm *pVm = pCtx->pVm;` |
|   177 | 1806 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 1807 | `	ph7_class_instance *pObj;` |
|     - | 1808 | `	ph7_class *pIterCls, *pAggCls, *pTravCls;` |
|   177 | 1809 | `	ph7_class *pCast = 0;` |
|   177 | 1810 | `	ph7_class_instance *pHold = 0;   /* the unwrapped iterator, kept alive across levels */` |
|     - | 1811 | `	int nLevel;` |
|   177 | 1812 | `	if( pThis == 0 ){` |
|   ! 0 | 1813 | `		return PH7_OK;` |
|     - | 1814 | `	}` |
|   177 | 1815 | `	if( PH7_NativeAttrObj(pThis,IT_IN) != 0 ){` |
|     4 | 1816 | `		return PH7_VmThrowException(pCtx,"BadMethodCallException",` |
|     1 | 1817 | `			"%s::getIterator() must be called exactly once per instance",zOwner);` |
|     - | 1818 | `	}` |
|   175 | 1819 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|   ! 0 | 1820 | `		return PH7_OK;   /* the shared ZPP screen already refused a non-object */` |
|     - | 1821 | `	}` |
|   175 | 1822 | `	pObj = (ph7_class_instance *)apArg[0]->x.pOther;` |
|   175 | 1823 | `	pIterCls = PH7_VmExtractClass(pVm,"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|   175 | 1824 | `	pAggCls = PH7_VmExtractClass(pVm,"IteratorAggregate",sizeof("IteratorAggregate")-1,FALSE,0);` |
|   175 | 1825 | `	pTravCls = PH7_VmExtractClass(pVm,"Traversable",sizeof("Traversable")-1,FALSE,0);` |
|   175 | 1826 | `	if( pIterCls && PH7_VmInstanceOf(pObj->pClass,pIterCls) ){` |
|     - | 1827 | `		/* Already an Iterator: php ignores $class entirely on this path. */` |
|   165 | 1828 | `		PH7_NativeSetAttrObj(pVm,pThis,IT_IN,pObj);` |
|   165 | 1829 | `		PH7_NativeSetAttrObj(pVm,pThis,IT_IT,pObj);` |
|   165 | 1830 | `		return PH7_OK;` |
|     - | 1831 | `	}` |
|    11 | 1832 | `	if( nArg > 1 && (apArg[1]->iFlags & MEMOBJ_NULL) == 0 ){` |
|     - | 1833 | `		/* php's DOWNCAST: $class names the class whose getIterator() to run, which is` |
|     - | 1834 | `		 * how a subclass asks for its parent's traversal. It must be a base of the` |
|     - | 1835 | `		 * argument AND traversable itself. */` |
|     - | 1836 | `		int nName;` |
|     5 | 1837 | `		const char *zName = ph7_value_to_string(apArg[1],&nName);` |
|     5 | 1838 | `		pCast = nName > 0 ? PH7_VmExtractClass(pVm,zName,(sxu32)nName,FALSE,0) : 0;` |
|     4 | 1839 | `		if( pCast == 0 \|\| !PH7_VmInstanceOf(pObj->pClass,pCast)` |
|     3 | 1840 | `		 \|\| (pTravCls && !PH7_VmInstanceOf(pCast,pTravCls)) ){` |
|     5 | 1841 | `			return PH7_VmThrowException(pCtx,"LogicException",` |
|     - | 1842 | `				"Class to downcast to not found or not base class or does not implement Traversable");` |
|     - | 1843 | `		}` |
|   ! 0 | 1844 | `	}` |
|     - | 1845 | `	/*` |
|     - | 1846 | `	 * An IteratorAggregate: run getIterator() — the DOWNCAST class's when one was` |
|     - | 1847 | `	 * named — and keep its answer as the inner object. php stops after one level` |
|     - | 1848 | `	 * here; the loop below is the engine's get_iterator handler, which resolves the` |
|     - | 1849 | `	 * rest lazily, done eagerly because PHL drives the inner through the METHOD` |
|     - | 1850 | `	 * protocol and nothing else would unwrap it.` |
|     - | 1851 | `	 */` |
|     9 | 1852 | `	for( nLevel = 0 ; nLevel < 16 ; ++nLevel ){` |
|     9 | 1853 | `		ph7_class *pFrom = pCast ? pCast : pObj->pClass;` |
|     - | 1854 | `		ph7_class_method *pMethod;` |
|     - | 1855 | `		ph7_value sInner;` |
|     - | 1856 | `		sxi32 rc;` |
|     9 | 1857 | `		if( pAggCls == 0 \|\| !PH7_VmInstanceOf(pObj->pClass,pAggCls) ){` |
|   ! 0 | 1858 | `			break;` |
|     - | 1859 | `		}` |
|     9 | 1860 | `		pMethod = PH7_ClassExtractMethod(pFrom,"getIterator",sizeof("getIterator")-1);` |
|     9 | 1861 | `		if( pMethod == 0 ){` |
|   ! 0 | 1862 | `			break;` |
|     - | 1863 | `		}` |
|     9 | 1864 | `		PH7_MemObjInit(pVm,&sInner);` |
|     9 | 1865 | `		rc = PH7_VmCallClassMethod(pVm,pObj,pMethod,&sInner,0,0);` |
|     9 | 1866 | `		if( rc != SXRET_OK ){` |
|   ! 0 | 1867 | `			PH7_MemObjRelease(&sInner);` |
|   ! 0 | 1868 | `			return rc;` |
|     - | 1869 | `		}` |
|     8 | 1870 | `		if( (sInner.iFlags & MEMOBJ_OBJ) == 0 \|\| sInner.x.pOther == 0` |
|     9 | 1871 | `		 \|\| (pTravCls && !PH7_VmInstanceOf(((ph7_class_instance *)sInner.x.pOther)->pClass,pTravCls)) ){` |
|   ! 0 | 1872 | `			SyString *pName = &pFrom->sName;` |
|   ! 0 | 1873 | `			PH7_MemObjRelease(&sInner);` |
|   ! 0 | 1874 | `			return PH7_VmThrowException(pCtx,"LogicException",` |
|   ! 0 | 1875 | `				"%z::getIterator() must return an object that implements Traversable",pName);` |
|     - | 1876 | `		}` |
|     9 | 1877 | `		pObj = (ph7_class_instance *)sInner.x.pOther;` |
|     9 | 1878 | `		pObj->iRef++;                 /* survive the release of the call result */` |
|     9 | 1879 | `		PH7_MemObjRelease(&sInner);` |
|     9 | 1880 | `		if( pHold ){` |
|     3 | 1881 | `			PH7_ClassInstanceUnref(pHold);` |
|     1 | 1882 | `		}` |
|     9 | 1883 | `		pHold = pObj;                 /* this function owns exactly one reference */` |
|     9 | 1884 | `		if( nLevel == 0 ){` |
|     - | 1885 | `			/* php's inner.zobject is the FIRST unwrap and nothing deeper. */` |
|     7 | 1886 | `			PH7_NativeSetAttrObj(pVm,pThis,IT_IN,pObj);` |
|     3 | 1887 | `		}` |
|     9 | 1888 | `		pCast = 0;` |
|     9 | 1889 | `		if( pIterCls && PH7_VmInstanceOf(pObj->pClass,pIterCls) ){` |
|     7 | 1890 | `			break;` |
|     - | 1891 | `		}` |
|     2 | 1892 | `	}` |
|     7 | 1893 | `	if( PH7_NativeAttrObj(pThis,IT_IN) == 0 ){` |
|   ! 0 | 1894 | `		PH7_NativeSetAttrObj(pVm,pThis,IT_IN,pObj);` |
|   ! 0 | 1895 | `	}` |
|     7 | 1896 | `	PH7_NativeSetAttrObj(pVm,pThis,IT_IT,pObj);` |
|     7 | 1897 | `	if( pHold ){` |
|     7 | 1898 | `		PH7_ClassInstanceUnref(pHold);   /* both slots hold their own now */` |
|     3 | 1899 | `	}` |
|     7 | 1900 | `	return PH7_OK;` |
|    89 | 1901 | `}` |
|    36 | 1902 | `static int vm_builtin_IteratorIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1903 | `{` |
|    37 | 1904 | `	return DualConstruct(pCtx,"IteratorIterator",nArg,apArg);` |
|     1 | 1905 | `}` |
|     2 | 1906 | `static int vm_builtin_FilterIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1907 | `{` |
|     3 | 1908 | `	return DualConstruct(pCtx,"FilterIterator",nArg,apArg);` |
|     1 | 1909 | `}` |
|     8 | 1910 | `static int vm_builtin_CallbackFilterIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1911 | `{` |
|     - | 1912 | `	ph7_class_instance *pThis;` |
|     - | 1913 | `	sxi32 rc;` |
|     9 | 1914 | `	if( nArg > 1 ){` |
|     - | 1915 | ``		/* The shared ZPP screen leaves `callable` to the builtin's own check (a string`` |
|     - | 1916 | `		 * satisfies the declared type; whether it NAMES a function does not), so php's` |
|     - | 1917 | `		 * "must be a valid callback, function "x" not found" only appears if the body` |
|     - | 1918 | `		 * asks for it — as every callback-taking builtin already does. */` |
|     9 | 1919 | `		rc = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",FALSE);` |
|     9 | 1920 | `		if( rc != PH7_OK ){` |
|     3 | 1921 | `			return rc;` |
|     - | 1922 | `		}` |
|     3 | 1923 | `	}` |
|     7 | 1924 | `	rc = DualConstruct(pCtx,"CallbackFilterIterator",nArg,apArg);` |
|     7 | 1925 | `	pThis = PH7_ContextThis(pCtx);` |
|     7 | 1926 | `	if( rc == PH7_OK && pThis && nArg > 1 ){` |
|     7 | 1927 | `		DualSetSlot(pCtx->pVm,pThis,IT_CB,apArg[1]);` |
|     3 | 1928 | `	}` |
|     7 | 1929 | `	return rc;` |
|     5 | 1930 | `}` |
|     8 | 1931 | `static int vm_builtin_InfiniteIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1932 | `{` |
|     9 | 1933 | `	return DualConstruct(pCtx,"InfiniteIterator",nArg,apArg);` |
|     1 | 1934 | `}` |
|     8 | 1935 | `static int vm_builtin_NoRewindIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1936 | `{` |
|     9 | 1937 | `	return DualConstruct(pCtx,"NoRewindIterator",nArg,apArg);` |
|     1 | 1938 | `}` |
|    40 | 1939 | `static int vm_builtin_Dual_getInnerIterator(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1940 | `{` |
|    41 | 1941 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 1942 | `	ph7_class_instance *pIn;` |
|    20 | 1943 | `	SXUNUSED(nArg);` |
|    20 | 1944 | `	SXUNUSED(apArg);` |
|    41 | 1945 | `	if( !DualReady(pThis) ){` |
|   ! 0 | 1946 | `		return DualNotReady(pCtx);` |
|     - | 1947 | `	}` |
|    41 | 1948 | `	pIn = PH7_NativeAttrObj(pThis,IT_IN);` |
|    41 | 1949 | `	if( pIn ){` |
|    39 | 1950 | `		SplResultBorrowed(pCtx,pIn);` |
|    20 | 1951 | `	}else{` |
|     3 | 1952 | `		ph7_result_null(pCtx);` |
|     - | 1953 | `	}` |
|    41 | 1954 | `	return PH7_OK;` |
|    21 | 1955 | `}` |
|   336 | 1956 | `static int vm_builtin_Dual_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1957 | `{` |
|   337 | 1958 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   168 | 1959 | `	SXUNUSED(nArg);` |
|   168 | 1960 | `	SXUNUSED(apArg);` |
|   337 | 1961 | `	if( !DualReady(pThis) ){` |
|     3 | 1962 | `		return DualNotReady(pCtx);` |
|     - | 1963 | `	}` |
|   335 | 1964 | `	ph7_result_bool(pCtx,DualFilled(pThis));` |
|   335 | 1965 | `	return PH7_OK;` |
|   169 | 1966 | `}` |
|   208 | 1967 | `static int vm_builtin_Dual_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1968 | `{` |
|   104 | 1969 | `	SXUNUSED(nArg);` |
|   104 | 1970 | `	SXUNUSED(apArg);` |
|   209 | 1971 | `	return DualResultSlot(pCtx,IT_CD);` |
|     1 | 1972 | `}` |
|   162 | 1973 | `static int vm_builtin_Dual_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1974 | `{` |
|    81 | 1975 | `	SXUNUSED(nArg);` |
|    81 | 1976 | `	SXUNUSED(apArg);` |
|   163 | 1977 | `	return DualResultSlot(pCtx,IT_CK);` |
|     1 | 1978 | `}` |
|    22 | 1979 | `static int vm_builtin_IteratorIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1980 | `{` |
|    23 | 1981 | `	ph7_vm *pVm = pCtx->pVm;` |
|    23 | 1982 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 1983 | `	sxi32 rc;` |
|    11 | 1984 | `	SXUNUSED(nArg);` |
|    11 | 1985 | `	SXUNUSED(apArg);` |
|    23 | 1986 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 | 1987 | `		return DualNotReady(pCtx);` |
|     - | 1988 | `	}` |
|    23 | 1989 | `	rc = DualRewindInner(pVm,pThis);` |
|    23 | 1990 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 1991 | `		return rc;` |
|     - | 1992 | `	}` |
|    23 | 1993 | `	return DualFetch(pVm,pThis,TRUE);` |
|    12 | 1994 | `}` |
|    22 | 1995 | `static int vm_builtin_IteratorIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1996 | `{` |
|    23 | 1997 | `	ph7_vm *pVm = pCtx->pVm;` |
|    23 | 1998 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 1999 | `	sxi32 rc;` |
|    11 | 2000 | `	SXUNUSED(nArg);` |
|    11 | 2001 | `	SXUNUSED(apArg);` |
|    23 | 2002 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 | 2003 | `		return DualNotReady(pCtx);` |
|     - | 2004 | `	}` |
|    23 | 2005 | `	rc = DualNextInner(pVm,pThis);` |
|    23 | 2006 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 2007 | `		return rc;` |
|     - | 2008 | `	}` |
|    23 | 2009 | `	return DualFetch(pVm,pThis,TRUE);` |
|    12 | 2010 | `}` |
|     - | 2011 | `/*` |
|     - | 2012 | ` * FilterIterator. php's spl_filter_it_fetch: fetch, ask accept(), and on a refusal` |
|     - | 2013 | ` * step the INNER on directly — without counting the step, which is why a filtered` |
|     - | 2014 | ` * element does not move current.pos. accept() is called on $this, so a user` |
|     - | 2015 | ` * subclass's body is what decides.` |
|     - | 2016 | ` */` |
|   162 | 2017 | `static sxi32 DualAccept(ph7_vm *pVm,ph7_class_instance *pThis,int *pbAccept)` |
|     1 | 2018 | `{` |
|   163 | 2019 | `	ph7_class_method *pMethod = PH7_ClassExtractMethod(pThis->pClass,"accept",sizeof("accept")-1);` |
|     - | 2020 | `	ph7_value sRes;` |
|     - | 2021 | `	sxi32 rc;` |
|   163 | 2022 | `	*pbAccept = 0;` |
|   163 | 2023 | `	if( pMethod == 0 ){` |
|   ! 0 | 2024 | `		return SXRET_OK;` |
|     - | 2025 | `	}` |
|   163 | 2026 | `	PH7_MemObjInit(pVm,&sRes);` |
|   163 | 2027 | `	rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sRes,0,0);` |
|   163 | 2028 | `	if( rc == SXRET_OK ){` |
|   163 | 2029 | `		PH7_MemObjToBool(&sRes);` |
|   163 | 2030 | `		*pbAccept = sRes.x.iVal != 0;` |
|    81 | 2031 | `	}` |
|   163 | 2032 | `	PH7_MemObjRelease(&sRes);` |
|   163 | 2033 | `	return rc;` |
|    82 | 2034 | `}` |
|   164 | 2035 | `static sxi32 DualFilterFetch(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 | 2036 | `{` |
|   144 | 2037 | `	for(;;){` |
|   227 | 2038 | `		int bAccept = 0;` |
|   227 | 2039 | `		sxi32 rc = DualFetch(pVm,pThis,TRUE);` |
|   227 | 2040 | `		if( rc != SXRET_OK ){` |
|   ! 0 | 2041 | `			return rc;` |
|     - | 2042 | `		}` |
|   227 | 2043 | `		if( !DualFilled(pThis) ){` |
|    65 | 2044 | `			break;` |
|     - | 2045 | `		}` |
|   163 | 2046 | `		rc = DualAccept(pVm,pThis,&bAccept);` |
|   163 | 2047 | `		if( rc != SXRET_OK ){` |
|   ! 0 | 2048 | `			return rc;` |
|     - | 2049 | `		}` |
|   163 | 2050 | `		if( bAccept ){` |
|   101 | 2051 | `			return SXRET_OK;` |
|     - | 2052 | `		}` |
|    63 | 2053 | `		rc = DualCall(pVm,pThis,"next",sizeof("next")-1,0);` |
|    63 | 2054 | `		if( rc != SXRET_OK ){` |
|   ! 0 | 2055 | `			return rc;` |
|     - | 2056 | `		}` |
|     1 | 2057 | `	}` |
|    65 | 2058 | `	DualFree(pVm,pThis);` |
|    65 | 2059 | `	return SXRET_OK;` |
|    83 | 2060 | `}` |
|    68 | 2061 | `static int vm_builtin_FilterIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2062 | `{` |
|    69 | 2063 | `	ph7_vm *pVm = pCtx->pVm;` |
|    69 | 2064 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 2065 | `	sxi32 rc;` |
|    34 | 2066 | `	SXUNUSED(nArg);` |
|    34 | 2067 | `	SXUNUSED(apArg);` |
|    69 | 2068 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 | 2069 | `		return DualNotReady(pCtx);` |
|     - | 2070 | `	}` |
|    69 | 2071 | `	rc = DualRewindInner(pVm,pThis);` |
|    69 | 2072 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 2073 | `		return rc;` |
|     - | 2074 | `	}` |
|    69 | 2075 | `	return DualFilterFetch(pVm,pThis);` |
|    35 | 2076 | `}` |
|    96 | 2077 | `static int vm_builtin_FilterIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2078 | `{` |
|    97 | 2079 | `	ph7_vm *pVm = pCtx->pVm;` |
|    97 | 2080 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 2081 | `	sxi32 rc;` |
|    48 | 2082 | `	SXUNUSED(nArg);` |
|    48 | 2083 | `	SXUNUSED(apArg);` |
|    97 | 2084 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 | 2085 | `		return DualNotReady(pCtx);` |
|     - | 2086 | `	}` |
|    97 | 2087 | `	rc = DualNextInner(pVm,pThis);` |
|    97 | 2088 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 2089 | `		return rc;` |
|     - | 2090 | `	}` |
|    97 | 2091 | `	return DualFilterFetch(pVm,pThis);` |
|    49 | 2092 | `}` |
|     - | 2093 | `/* CallbackFilterIterator::accept(): the callback sees the CACHED pair and the inner` |
|     - | 2094 | ` * iterator, and an empty cache is refused without calling it at all. */` |
|    16 | 2095 | `static int vm_builtin_CallbackFilterIterator_accept(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2096 | `{` |
|    17 | 2097 | `	ph7_vm *pVm = pCtx->pVm;` |
|    17 | 2098 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 2099 | `	ph7_value *apCall[3];` |
|     - | 2100 | `	ph7_value sInner,sRes,*pCb;` |
|     - | 2101 | `	sxi32 rc;` |
|     8 | 2102 | `	SXUNUSED(nArg);` |
|     8 | 2103 | `	SXUNUSED(apArg);` |
|    17 | 2104 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 | 2105 | `		return DualNotReady(pCtx);` |
|     - | 2106 | `	}` |
|    17 | 2107 | `	if( !DualFilled(pThis) ){` |
|   ! 0 | 2108 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2109 | `		return PH7_OK;` |
|     - | 2110 | `	}` |
|    17 | 2111 | `	pCb = PH7_NativeAttr(pThis,IT_CB);` |
|    17 | 2112 | `	if( pCb == 0 ){` |
|   ! 0 | 2113 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2114 | `		return PH7_OK;` |
|     - | 2115 | `	}` |
|    17 | 2116 | `	PH7_MemObjInit(pVm,&sInner);` |
|    17 | 2117 | `	sInner.x.pOther = PH7_NativeAttrObj(pThis,IT_IN);` |
|    17 | 2118 | `	if( sInner.x.pOther ){` |
|    17 | 2119 | `		MemObjSetType(&sInner,MEMOBJ_OBJ);` |
|    17 | 2120 | `		((ph7_class_instance *)sInner.x.pOther)->iRef++;` |
|     8 | 2121 | `	}` |
|    17 | 2122 | `	apCall[0] = PH7_NativeAttr(pThis,IT_CD);` |
|    17 | 2123 | `	apCall[1] = PH7_NativeAttr(pThis,IT_CK);` |
|    17 | 2124 | `	apCall[2] = &sInner;` |
|    17 | 2125 | `	PH7_MemObjInit(pVm,&sRes);` |
|    17 | 2126 | `	rc = PH7_VmCallUserFunction(pVm,pCb,3,apCall,&sRes);` |
|    17 | 2127 | `	PH7_MemObjRelease(&sInner);` |
|    17 | 2128 | `	if( rc == SXRET_OK ){` |
|    17 | 2129 | `		ph7_result_value(pCtx,&sRes);` |
|     8 | 2130 | `	}` |
|    17 | 2131 | `	PH7_MemObjRelease(&sRes);` |
|    17 | 2132 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|     9 | 2133 | `}` |
|     - | 2134 | `/*` |
|     - | 2135 | ` * LimitIterator. The window is (offset, count) over the inner iterator's own` |
|     - | 2136 | `` * positions, and `__cp` counts them: php's valid() is "inside the window AND the`` |
|     - | 2137 | ` * cache is filled", and next() only refills while the window still has room.` |
|     - | 2138 | ` */` |
|    28 | 2139 | `static sxi32 DualLimitSeek(ph7_context *pCtx,ph7_class_instance *pThis,sxi64 iPos)` |
|     1 | 2140 | `{` |
|    29 | 2141 | `	ph7_vm *pVm = pCtx->pVm;` |
|    29 | 2142 | `	sxi64 iOff = PH7_NativeAttrInt(pThis,IT_OFF);` |
|    29 | 2143 | `	sxi64 iLim = PH7_NativeAttrInt(pThis,IT_LIM);` |
|    29 | 2144 | `	ph7_class_instance *pIn = DualDriver(pThis);` |
|     - | 2145 | `	ph7_class *pSeekCls;` |
|     - | 2146 | `	sxi32 rc;` |
|     - | 2147 | `	int bValid;` |
|    29 | 2148 | `	DualFree(pVm,pThis);` |
|    29 | 2149 | `	if( iPos < iOff ){` |
|     7 | 2150 | `		return PH7_VmThrowException(pCtx,"OutOfBoundsException",` |
|     2 | 2151 | `			"Cannot seek to %qd which is below the offset %qd",iPos,iOff);` |
|     - | 2152 | `	}` |
|    25 | 2153 | `	if( iLim != -1 && (iPos - iOff) >= iLim ){` |
|     4 | 2154 | `		return PH7_VmThrowException(pCtx,"OutOfBoundsException",` |
|     1 | 2155 | `			"Cannot seek to %qd which is behind offset %qd plus count %qd",iPos,iOff,iLim);` |
|     - | 2156 | `	}` |
|    23 | 2157 | `	pSeekCls = PH7_VmExtractClass(pVm,"SeekableIterator",sizeof("SeekableIterator")-1,FALSE,0);` |
|    22 | 2158 | `	if( iPos != PH7_NativeAttrInt(pThis,IT_CP) && pIn && pSeekCls` |
|    17 | 2159 | `	 && PH7_VmInstanceOf(pIn->pClass,pSeekCls) ){` |
|     - | 2160 | `		/* The inner knows how to jump: hand it the ABSOLUTE position and let its own` |
|     - | 2161 | `		 * refusal (ArrayIterator's "Seek position N is out of range") surface. */` |
|    17 | 2162 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pIn->pClass,"seek",sizeof("seek")-1);` |
|     - | 2163 | `		ph7_value sPos,*apArg[1];` |
|    17 | 2164 | `		PH7_MemObjInitFromInt(pVm,&sPos,iPos);` |
|    17 | 2165 | `		apArg[0] = &sPos;` |
|    17 | 2166 | `		rc = pMethod ? PH7_VmCallClassMethod(pVm,pIn,pMethod,0,1,apArg) : SXRET_OK;` |
|    17 | 2167 | `		PH7_MemObjRelease(&sPos);` |
|    17 | 2168 | `		if( rc != SXRET_OK ){` |
|   ! 0 | 2169 | `			return rc;` |
|     - | 2170 | `		}` |
|    17 | 2171 | `		PH7_NativeSetAttrInt(pVm,pThis,IT_CP,iPos);` |
|    17 | 2172 | `		rc = DualInnerValid(pVm,pThis,&bValid);` |
|    17 | 2173 | `		if( rc != SXRET_OK ){` |
|   ! 0 | 2174 | `			return rc;` |
|     - | 2175 | `		}` |
|    17 | 2176 | `		if( bValid ){` |
|    17 | 2177 | `			return DualFetch(pVm,pThis,FALSE);` |
|     - | 2178 | `		}` |
|   ! 0 | 2179 | `		return SXRET_OK;` |
|     - | 2180 | `	}` |
|     - | 2181 | `	/* Otherwise emulate: a backward seek is a rewind followed by next() calls. */` |
|     7 | 2182 | `	if( iPos < PH7_NativeAttrInt(pThis,IT_CP) ){` |
|   ! 0 | 2183 | `		rc = DualRewindInner(pVm,pThis);` |
|   ! 0 | 2184 | `		if( rc != SXRET_OK ){` |
|   ! 0 | 2185 | `			return rc;` |
|     - | 2186 | `		}` |
|   ! 0 | 2187 | `	}` |
|     3 | 2188 | `	for(;;){` |
|     7 | 2189 | `		if( iPos <= PH7_NativeAttrInt(pThis,IT_CP) ){` |
|     7 | 2190 | `			break;` |
|     - | 2191 | `		}` |
|   ! 0 | 2192 | `		rc = DualInnerValid(pVm,pThis,&bValid);` |
|   ! 0 | 2193 | `		if( rc != SXRET_OK ){` |
|   ! 0 | 2194 | `			return rc;` |
|     - | 2195 | `		}` |
|   ! 0 | 2196 | `		if( !bValid ){` |
|   ! 0 | 2197 | `			break;` |
|     - | 2198 | `		}` |
|   ! 0 | 2199 | `		rc = DualNextInner(pVm,pThis);` |
|   ! 0 | 2200 | `		if( rc != SXRET_OK ){` |
|   ! 0 | 2201 | `			return rc;` |
|     - | 2202 | `		}` |
|   ! 0 | 2203 | `	}` |
|     7 | 2204 | `	rc = DualInnerValid(pVm,pThis,&bValid);` |
|     7 | 2205 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 2206 | `		return rc;` |
|     - | 2207 | `	}` |
|     7 | 2208 | `	if( bValid ){` |
|     7 | 2209 | `		return DualFetch(pVm,pThis,TRUE);` |
|     - | 2210 | `	}` |
|   ! 0 | 2211 | `	return SXRET_OK;` |
|    15 | 2212 | `}` |
|    46 | 2213 | `static int vm_builtin_LimitIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2214 | `{` |
|    47 | 2215 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 2216 | `	ph7_class_instance *pThis;` |
|    47 | 2217 | `	sxi64 iOff = 0,iLim = -1;` |
|     - | 2218 | `	sxi32 rc;` |
|     - | 2219 | `	/* php screens the two bounds BEFORE it remembers the iterator, so a refused` |
|     - | 2220 | `	 * LimitIterator can still be constructed again. PH7_IntArgResolve is the shared` |
|     - | 2221 | ``	 * `int` ZPP: the central signature screen does not cover a non-numeric STRING`` |
|     - | 2222 | `	 * against an int parameter, and every builtin that takes one calls this. */` |
|    47 | 2223 | `	if( nArg > 1 ){` |
|    41 | 2224 | `		rc = PH7_IntArgResolve(pCtx,apArg[1],ph7_function_name(pCtx),2,"$offset","int",&iOff);` |
|    41 | 2225 | `		if( rc != PH7_OK ){` |
|   ! 0 | 2226 | `			return rc;` |
|     - | 2227 | `		}` |
|    41 | 2228 | `		if( iOff < 0 ){` |
|     7 | 2229 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 2230 | `				"%s(): Argument #2 ($offset) must be greater than or equal to 0",` |
|     2 | 2231 | `				ph7_function_name(pCtx));` |
|     - | 2232 | `		}` |
|    18 | 2233 | `	}` |
|    43 | 2234 | `	if( nArg > 2 ){` |
|    33 | 2235 | `		rc = PH7_IntArgResolve(pCtx,apArg[2],ph7_function_name(pCtx),3,"$limit","int",&iLim);` |
|    33 | 2236 | `		if( rc != PH7_OK ){` |
|   ! 0 | 2237 | `			return rc;` |
|     - | 2238 | `		}` |
|    33 | 2239 | `		if( iLim < -1 ){` |
|     7 | 2240 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 2241 | `				"%s(): Argument #3 ($limit) must be greater than or equal to -1",` |
|     2 | 2242 | `				ph7_function_name(pCtx));` |
|     - | 2243 | `		}` |
|    14 | 2244 | `	}` |
|    39 | 2245 | `	rc = DualConstruct(pCtx,"LimitIterator",nArg,apArg);` |
|    39 | 2246 | `	if( rc != PH7_OK ){` |
|     3 | 2247 | `		return rc;` |
|     - | 2248 | `	}` |
|    37 | 2249 | `	pThis = PH7_ContextThis(pCtx);` |
|    37 | 2250 | `	if( pThis ){` |
|    37 | 2251 | `		PH7_NativeSetAttrInt(pVm,pThis,IT_OFF,iOff);` |
|    37 | 2252 | `		PH7_NativeSetAttrInt(pVm,pThis,IT_LIM,iLim);` |
|    18 | 2253 | `	}` |
|    37 | 2254 | `	return PH7_OK;` |
|    24 | 2255 | `}` |
|    16 | 2256 | `static int vm_builtin_LimitIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2257 | `{` |
|    17 | 2258 | `	ph7_vm *pVm = pCtx->pVm;` |
|    17 | 2259 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 2260 | `	sxi32 rc;` |
|     8 | 2261 | `	SXUNUSED(nArg);` |
|     8 | 2262 | `	SXUNUSED(apArg);` |
|    17 | 2263 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 | 2264 | `		return DualNotReady(pCtx);` |
|     - | 2265 | `	}` |
|    17 | 2266 | `	rc = DualRewindInner(pVm,pThis);` |
|    17 | 2267 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 2268 | `		return rc;` |
|     - | 2269 | `	}` |
|    17 | 2270 | `	return DualLimitSeek(pCtx,pThis,PH7_NativeAttrInt(pThis,IT_OFF));` |
|     9 | 2271 | `}` |
|    40 | 2272 | `static int vm_builtin_LimitIterator_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2273 | `{` |
|    41 | 2274 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 2275 | `	sxi64 iLim;` |
|    20 | 2276 | `	SXUNUSED(nArg);` |
|    20 | 2277 | `	SXUNUSED(apArg);` |
|    41 | 2278 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 | 2279 | `		return DualNotReady(pCtx);` |
|     - | 2280 | `	}` |
|    41 | 2281 | `	iLim = PH7_NativeAttrInt(pThis,IT_LIM);` |
|    81 | 2282 | `	ph7_result_bool(pCtx,` |
|    20 | 2283 | `		(iLim == -1` |
|    33 | 2284 | `		 \|\| (PH7_NativeAttrInt(pThis,IT_CP) - PH7_NativeAttrInt(pThis,IT_OFF)) < iLim)` |
|    35 | 2285 | `		&& DualFilled(pThis));` |
|    41 | 2286 | `	return PH7_OK;` |
|    21 | 2287 | `}` |
|    34 | 2288 | `static int vm_builtin_LimitIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2289 | `{` |
|    35 | 2290 | `	ph7_vm *pVm = pCtx->pVm;` |
|    35 | 2291 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 2292 | `	sxi64 iLim;` |
|     - | 2293 | `	sxi32 rc;` |
|    17 | 2294 | `	SXUNUSED(nArg);` |
|    17 | 2295 | `	SXUNUSED(apArg);` |
|    35 | 2296 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 | 2297 | `		return DualNotReady(pCtx);` |
|     - | 2298 | `	}` |
|    35 | 2299 | `	rc = DualNextInner(pVm,pThis);` |
|    35 | 2300 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 2301 | `		return rc;` |
|     - | 2302 | `	}` |
|    35 | 2303 | `	iLim = PH7_NativeAttrInt(pThis,IT_LIM);` |
|    34 | 2304 | `	if( iLim == -1` |
|    30 | 2305 | `	 \|\| (PH7_NativeAttrInt(pThis,IT_CP) - PH7_NativeAttrInt(pThis,IT_OFF)) < iLim ){` |
|    25 | 2306 | `		return DualFetch(pVm,pThis,TRUE);` |
|     - | 2307 | `	}` |
|    11 | 2308 | `	return PH7_OK;   /* past the window: the cache stays empty, so current() is null */` |
|    18 | 2309 | `}` |
|    12 | 2310 | `static int vm_builtin_LimitIterator_seek(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2311 | `{` |
|    13 | 2312 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    13 | 2313 | `	sxi64 iPos = 0;` |
|     - | 2314 | `	sxi32 rc;` |
|    13 | 2315 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 | 2316 | `		return DualNotReady(pCtx);` |
|     - | 2317 | `	}` |
|    13 | 2318 | `	if( nArg > 0 ){` |
|    13 | 2319 | `		rc = PH7_IntArgResolve(pCtx,apArg[0],ph7_function_name(pCtx),1,"$offset","int",&iPos);` |
|    13 | 2320 | `		if( rc != PH7_OK ){` |
|   ! 0 | 2321 | `			return rc;` |
|     - | 2322 | `		}` |
|     6 | 2323 | `	}` |
|    13 | 2324 | `	rc = DualLimitSeek(pCtx,pThis,iPos);` |
|    13 | 2325 | `	if( rc != PH7_OK ){` |
|     7 | 2326 | `		return rc;` |
|     - | 2327 | `	}` |
|     7 | 2328 | `	ph7_result_int64(pCtx,PH7_NativeAttrInt(pThis,IT_CP));` |
|     7 | 2329 | `	return PH7_OK;` |
|     7 | 2330 | `}` |
|    18 | 2331 | `static int vm_builtin_LimitIterator_getPosition(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2332 | `{` |
|    19 | 2333 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     9 | 2334 | `	SXUNUSED(nArg);` |
|     9 | 2335 | `	SXUNUSED(apArg);` |
|    19 | 2336 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 | 2337 | `		return DualNotReady(pCtx);` |
|     - | 2338 | `	}` |
|    19 | 2339 | `	ph7_result_int64(pCtx,PH7_NativeAttrInt(pThis,IT_CP));` |
|    19 | 2340 | `	return PH7_OK;` |
|    10 | 2341 | `}` |
|     - | 2342 | `/* InfiniteIterator::next(): step, and on exhaustion rewind and step into the head` |
|     - | 2343 | ` * again. Both refills are php's check_more=0 form — the validity was just tested. */` |
|    16 | 2344 | `static int vm_builtin_InfiniteIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2345 | `{` |
|    17 | 2346 | `	ph7_vm *pVm = pCtx->pVm;` |
|    17 | 2347 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 2348 | `	sxi32 rc;` |
|     - | 2349 | `	int bValid;` |
|     8 | 2350 | `	SXUNUSED(nArg);` |
|     8 | 2351 | `	SXUNUSED(apArg);` |
|    17 | 2352 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 | 2353 | `		return DualNotReady(pCtx);` |
|     - | 2354 | `	}` |
|    17 | 2355 | `	rc = DualNextInner(pVm,pThis);` |
|    17 | 2356 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 2357 | `		return rc;` |
|     - | 2358 | `	}` |
|    17 | 2359 | `	rc = DualInnerValid(pVm,pThis,&bValid);` |
|    17 | 2360 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 2361 | `		return rc;` |
|     - | 2362 | `	}` |
|    17 | 2363 | `	if( !bValid ){` |
|     9 | 2364 | `		rc = DualRewindInner(pVm,pThis);` |
|     9 | 2365 | `		if( rc != SXRET_OK ){` |
|   ! 0 | 2366 | `			return rc;` |
|     - | 2367 | `		}` |
|     9 | 2368 | `		rc = DualInnerValid(pVm,pThis,&bValid);` |
|     9 | 2369 | `		if( rc != SXRET_OK ){` |
|   ! 0 | 2370 | `			return rc;` |
|     - | 2371 | `		}` |
|     4 | 2372 | `	}` |
|    17 | 2373 | `	if( bValid ){` |
|    17 | 2374 | `		return DualFetch(pVm,pThis,FALSE);` |
|     - | 2375 | `	}` |
|   ! 0 | 2376 | `	return PH7_OK;` |
|     9 | 2377 | `}` |
|     - | 2378 | `/*` |
|     - | 2379 | ` * NoRewindIterator. Its rewind() does nothing at all — and because the four` |
|     - | 2380 | ` * accessors read the INNER live rather than the cache, an instance is usable` |
|     - | 2381 | ` * without ever being rewound, which is the entire point of the class.` |
|     - | 2382 | ` */` |
|     4 | 2383 | `static int vm_builtin_NoRewindIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2384 | `{` |
|     2 | 2385 | `	SXUNUSED(nArg);` |
|     2 | 2386 | `	SXUNUSED(apArg);` |
|     5 | 2387 | `	if( PH7_ContextThis(pCtx) == 0 \|\| DualDriver(PH7_ContextThis(pCtx)) == 0 ){` |
|   ! 0 | 2388 | `		return DualNotReady(pCtx);` |
|     - | 2389 | `	}` |
|     5 | 2390 | `	return PH7_OK;` |
|     3 | 2391 | `}` |
|    14 | 2392 | `static int vm_builtin_NoRewindIterator_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2393 | `{` |
|    15 | 2394 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 2395 | `	sxi32 rc;` |
|     - | 2396 | `	int bValid;` |
|     7 | 2397 | `	SXUNUSED(nArg);` |
|     7 | 2398 | `	SXUNUSED(apArg);` |
|    15 | 2399 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 | 2400 | `		return DualNotReady(pCtx);` |
|     - | 2401 | `	}` |
|    15 | 2402 | `	rc = DualInnerValid(pCtx->pVm,pThis,&bValid);` |
|    15 | 2403 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 2404 | `		return rc;` |
|     - | 2405 | `	}` |
|    15 | 2406 | `	ph7_result_bool(pCtx,bValid);` |
|    15 | 2407 | `	return PH7_OK;` |
|     8 | 2408 | `}` |
|    26 | 2409 | `static int DualForwardLive(ph7_context *pCtx,const char *zName,sxu32 nLen,int bResult)` |
|     1 | 2410 | `{` |
|    27 | 2411 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 2412 | `	ph7_value sVal;` |
|     - | 2413 | `	sxi32 rc;` |
|    27 | 2414 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 | 2415 | `		return DualNotReady(pCtx);` |
|     - | 2416 | `	}` |
|    27 | 2417 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|    27 | 2418 | `	rc = DualCall(pCtx->pVm,pThis,zName,nLen,bResult ? &sVal : 0);` |
|    27 | 2419 | `	if( rc == SXRET_OK && bResult ){` |
|    17 | 2420 | `		ph7_result_value(pCtx,&sVal);` |
|     8 | 2421 | `	}` |
|    27 | 2422 | `	PH7_MemObjRelease(&sVal);` |
|    27 | 2423 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|    14 | 2424 | `}` |
|    10 | 2425 | `static int vm_builtin_NoRewindIterator_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2426 | `{` |
|     5 | 2427 | `	SXUNUSED(nArg);` |
|     5 | 2428 | `	SXUNUSED(apArg);` |
|    11 | 2429 | `	return DualForwardLive(pCtx,"current",sizeof("current")-1,TRUE);` |
|     1 | 2430 | `}` |
|     6 | 2431 | `static int vm_builtin_NoRewindIterator_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2432 | `{` |
|     3 | 2433 | `	SXUNUSED(nArg);` |
|     3 | 2434 | `	SXUNUSED(apArg);` |
|     7 | 2435 | `	return DualForwardLive(pCtx,"key",sizeof("key")-1,TRUE);` |
|     1 | 2436 | `}` |
|    10 | 2437 | `static int vm_builtin_NoRewindIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2438 | `{` |
|     5 | 2439 | `	SXUNUSED(nArg);` |
|     5 | 2440 | `	SXUNUSED(apArg);` |
|    11 | 2441 | `	return DualForwardLive(pCtx,"next",sizeof("next")-1,FALSE);` |
|     1 | 2442 | `}` |
|     - | 2443 | `/* EmptyIterator: valid() is false forever, and asking for a value or a key is a` |
|     - | 2444 | ` * BadMethodCallException rather than a null. */` |
|     4 | 2445 | `static int vm_builtin_EmptyIterator_nop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2446 | `{` |
|     2 | 2447 | `	SXUNUSED(nArg);` |
|     2 | 2448 | `	SXUNUSED(apArg);` |
|     5 | 2449 | `	ph7_result_null(pCtx);` |
|     5 | 2450 | `	return PH7_OK;` |
|     1 | 2451 | `}` |
|     8 | 2452 | `static int vm_builtin_EmptyIterator_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2453 | `{` |
|     4 | 2454 | `	SXUNUSED(nArg);` |
|     4 | 2455 | `	SXUNUSED(apArg);` |
|     9 | 2456 | `	ph7_result_bool(pCtx,0);` |
|     9 | 2457 | `	return PH7_OK;` |
|     1 | 2458 | `}` |
|     4 | 2459 | `static int vm_builtin_EmptyIterator_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2460 | `{` |
|     2 | 2461 | `	SXUNUSED(nArg);` |
|     2 | 2462 | `	SXUNUSED(apArg);` |
|     5 | 2463 | `	return PH7_VmThrowException(pCtx,"BadMethodCallException",` |
|     - | 2464 | `		"Accessing the value of an EmptyIterator");` |
|     1 | 2465 | `}` |
|     4 | 2466 | `static int vm_builtin_EmptyIterator_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2467 | `{` |
|     2 | 2468 | `	SXUNUSED(nArg);` |
|     2 | 2469 | `	SXUNUSED(apArg);` |
|     5 | 2470 | `	return PH7_VmThrowException(pCtx,"BadMethodCallException",` |
|     - | 2471 | `		"Accessing the key of an EmptyIterator");` |
|     1 | 2472 | `}` |
|     - | 2473 | `/*` |
|     - | 2474 | ` * ---------------------------------------------------------------------------` |
|     - | 2475 | ` * RegexIterator: a FilterIterator whose accept() runs a regex over the CACHE.` |
|     - | 2476 | ` *` |
|     - | 2477 | ` * Everything that matters here follows from the cache the decorators already` |
|     - | 2478 | `` * keep. php's accept() reads `current.data` (or `current.key` under USE_KEY) and`` |
|     - | 2479 | ` * -- in every mode but MATCH -- WRITES THE RESULT BACK INTO THAT SAME SLOT, which` |
|     - | 2480 | ` * is why the class declares no current() of its own: the inherited one already` |
|     - | 2481 | `` * answers the transformed value. The PHP chunk kept a private `$__cur` and`` |
|     - | 2482 | ` * overrode current(), and that is where its two wrong answers came from: a` |
|     - | 2483 | ` * REPLACE under USE_KEY must replace into the KEY (php leaves current() alone),` |
|     - | 2484 | ` * and an ARRAY current() is refused outright rather than matched as the string` |
|     - | 2485 | ` * "Array".` |
|     - | 2486 | ` */` |
|     - | 2487 | `#define IT_RE  "__re"   /* php's u.regex.regex: the pattern, as given */` |
|     - | 2488 | `#define IT_RM  "__rm"   /* php's u.regex.mode */` |
|     - | 2489 | `#define IT_RF  "__rf"   /* php's u.regex.flags (USE_KEY / INVERT_MATCH) */` |
|     - | 2490 | `#define IT_RP  "__rp"   /* php's u.regex.preg_flags */` |
|     - | 2491 | `#define REGIT_USE_KEY  1` |
|     - | 2492 | `#define REGIT_INVERTED 2` |
|     - | 2493 | `/* php's ValueError for a mode outside the five. The constructor and setMode()` |
|     - | 2494 | ` * word it identically and differ only in the argument they name. */` |
|     4 | 2495 | `static int RegitBadMode(ph7_context *pCtx,const char *zWhere)` |
|     1 | 2496 | `{` |
|     7 | 2497 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 2498 | `		"%s must be RegexIterator::MATCH, RegexIterator::GET_MATCH, "` |
|     - | 2499 | `		"RegexIterator::ALL_MATCHES, RegexIterator::SPLIT, or RegexIterator::REPLACE",` |
|     2 | 2500 | `		zWhere);` |
|     1 | 2501 | `}` |
|    62 | 2502 | `static int vm_builtin_RegexIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2503 | `{` |
|    63 | 2504 | `	ph7_vm *pVm = pCtx->pVm;` |
|    63 | 2505 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 2506 | `	const char *zPat;` |
|     - | 2507 | `	int nPat;` |
|    63 | 2508 | `	sxi64 iMode = PH7_REGIT_MATCH;` |
|     - | 2509 | `	char zErr[288];` |
|     - | 2510 | `	sxi32 rc;` |
|    63 | 2511 | `	if( pThis == 0 ){` |
|   ! 0 | 2512 | `		return PH7_OK;` |
|     - | 2513 | `	}` |
|    63 | 2514 | `	if( PH7_NativeAttrObj(pThis,IT_IN) != 0 ){` |
|     - | 2515 | `		/* php makes the "already built" refusal before it reads any argument, so` |
|     - | 2516 | `		 * hand this straight to the shared constructor, which words it. */` |
|   ! 0 | 2517 | `		return DualConstruct(pCtx,"RegexIterator",nArg,apArg);` |
|     - | 2518 | `	}` |
|    63 | 2519 | `	if( nArg < 2 ){` |
|   ! 0 | 2520 | `		return PH7_OK;   /* the arity screen already refused */` |
|     - | 2521 | `	}` |
|    63 | 2522 | `	if( nArg > 2 ){` |
|    35 | 2523 | `		iMode = ph7_value_to_int(apArg[2]);` |
|    17 | 2524 | `	}` |
|    63 | 2525 | `	if( iMode < PH7_REGIT_MATCH \|\| iMode > PH7_REGIT_REPLACE ){` |
|     3 | 2526 | `		return RegitBadMode(pCtx,"RegexIterator::__construct(): Argument #3 ($mode)");` |
|     - | 2527 | `	}` |
|     - | 2528 | `	/* php compiles the pattern HERE and promotes pcre's warning to an` |
|     - | 2529 | ``	 * InvalidArgumentException, so a bad pattern is refused by `new` rather than`` |
|     - | 2530 | `	 * warning once per element from accept(). */` |
|    61 | 2531 | `	zPat = ph7_value_to_string(apArg[1],&nPat);` |
|    61 | 2532 | `	if( !PH7_PcrePatternCheck(pVm,zPat,nPat,zErr,sizeof(zErr)) ){` |
|     4 | 2533 | `		return PH7_VmThrowException(pCtx,"InvalidArgumentException",` |
|     1 | 2534 | `			"RegexIterator::__construct(): %s",zErr);` |
|     - | 2535 | `	}` |
|    59 | 2536 | `	rc = DualConstruct(pCtx,"RegexIterator",nArg,apArg);` |
|    59 | 2537 | `	if( rc != PH7_OK ){` |
|   ! 0 | 2538 | `		return rc;` |
|     - | 2539 | `	}` |
|    59 | 2540 | `	PH7_NativeSetAttrStr(pVm,pThis,IT_RE,zPat,(sxu32)nPat);` |
|    59 | 2541 | `	PH7_NativeSetAttrInt(pVm,pThis,IT_RM,iMode);` |
|    59 | 2542 | `	PH7_NativeSetAttrInt(pVm,pThis,IT_RF,nArg > 3 ? ph7_value_to_int(apArg[3]) : 0);` |
|    59 | 2543 | `	PH7_NativeSetAttrInt(pVm,pThis,IT_RP,nArg > 4 ? ph7_value_to_int(apArg[4]) : 0);` |
|    59 | 2544 | `	return PH7_OK;` |
|    32 | 2545 | `}` |
|    88 | 2546 | `static int vm_builtin_RegexIterator_accept(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2547 | `{` |
|    89 | 2548 | `	ph7_vm *pVm = pCtx->pVm;` |
|    89 | 2549 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 2550 | `	ph7_value sSubject,sPattern,sRepl,sOut,*pSlot;` |
|    89 | 2551 | `	int iMode,iFlags,bUseKey,bOk = 0;` |
|     - | 2552 | `	sxi32 rc;` |
|    44 | 2553 | `	SXUNUSED(nArg);` |
|    44 | 2554 | `	SXUNUSED(apArg);` |
|    89 | 2555 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 | 2556 | `		return DualNotReady(pCtx);` |
|     - | 2557 | `	}` |
|    89 | 2558 | `	if( !DualFilled(pThis) ){` |
|     - | 2559 | `		/* Nothing has been fetched: php answers false without touching the regex. */` |
|     3 | 2560 | `		ph7_result_bool(pCtx,0);` |
|     3 | 2561 | `		return PH7_OK;` |
|     - | 2562 | `	}` |
|    87 | 2563 | `	iMode = (int)PH7_NativeAttrInt(pThis,IT_RM);` |
|    87 | 2564 | `	iFlags = (int)PH7_NativeAttrInt(pThis,IT_RF);` |
|    87 | 2565 | `	bUseKey = (iFlags & REGIT_USE_KEY) != 0;` |
|    87 | 2566 | `	pSlot = PH7_NativeAttr(pThis,bUseKey ? IT_CK : IT_CD);` |
|    87 | 2567 | `	if( pSlot == 0 ){` |
|   ! 0 | 2568 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2569 | `		return PH7_OK;` |
|     - | 2570 | `	}` |
|    87 | 2571 | `	if( !bUseKey && (pSlot->iFlags & MEMOBJ_HASHMAP) ){` |
|     - | 2572 | ``		/* php's `Z_TYPE(current.data) == IS_ARRAY -> RETURN_FALSE`, ahead of every`` |
|     - | 2573 | `		 * mode. The chunk's (string)$subject matched the word "Array" instead. */` |
|     5 | 2574 | `		ph7_result_bool(pCtx,0);` |
|     5 | 2575 | `		return PH7_OK;` |
|     - | 2576 | `	}` |
|     - | 2577 | `	/* Take the subject as a VALUE: the slot pointer does not survive a call into` |
|     - | 2578 | `	 * user code, and an object subject reaches __toString() below. */` |
|    83 | 2579 | `	PH7_MemObjInit(pVm,&sSubject);` |
|    83 | 2580 | `	PH7_MemObjStore(pSlot,&sSubject);` |
|    83 | 2581 | `	rc = PH7_MemObjToStringUV(&sSubject);` |
|    83 | 2582 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 2583 | `		PH7_MemObjRelease(&sSubject);` |
|   ! 0 | 2584 | `		return rc;` |
|     - | 2585 | `	}` |
|    83 | 2586 | `	PH7_MemObjInit(pVm,&sPattern);` |
|    83 | 2587 | `	PH7_MemObjInit(pVm,&sRepl);` |
|    83 | 2588 | `	PH7_MemObjInit(pVm,&sOut);` |
|     - | 2589 | `	{` |
|    83 | 2590 | `		ph7_value *pRe = PH7_NativeAttr(pThis,IT_RE);` |
|    83 | 2591 | `		if( pRe ){` |
|    83 | 2592 | `			PH7_MemObjStore(pRe,&sPattern);` |
|    41 | 2593 | `		}` |
|     - | 2594 | `	}` |
|    83 | 2595 | `	if( iMode == PH7_REGIT_REPLACE ){` |
|     - | 2596 | `		/* php reads the public $replacement property, whose declared ?string makes` |
|     - | 2597 | `		 * the read total: a null answers the empty string. */` |
|    17 | 2598 | `		ph7_value *pRepl = PH7_NativeAttr(pThis,"replacement");` |
|    17 | 2599 | `		if( pRepl ){` |
|    17 | 2600 | `			PH7_MemObjStore(pRepl,&sRepl);` |
|     8 | 2601 | `		}` |
|    17 | 2602 | `		PH7_MemObjToString(&sRepl);` |
|     8 | 2603 | `	}` |
|   124 | 2604 | `	rc = PH7_PcreRegitApply(pCtx,iMode,&sPattern,&sSubject,` |
|    82 | 2605 | `		(int)PH7_NativeAttrInt(pThis,IT_RP),&sRepl,&sOut,&bOk);` |
|    83 | 2606 | `	if( rc == PH7_OK && iMode != PH7_REGIT_MATCH ){` |
|     - | 2607 | `		/* php writes the transformed value over the cached pair -- into the KEY when` |
|     - | 2608 | `		 * a REPLACE is keyed, into current() otherwise -- so the inherited current()` |
|     - | 2609 | `		 * and key() present it. */` |
|    47 | 2610 | `		DualSetSlot(pVm,pThis,(iMode == PH7_REGIT_REPLACE && bUseKey) ? IT_CK : IT_CD,&sOut);` |
|    23 | 2611 | `	}` |
|    83 | 2612 | `	PH7_MemObjRelease(&sSubject);` |
|    83 | 2613 | `	PH7_MemObjRelease(&sPattern);` |
|    83 | 2614 | `	PH7_MemObjRelease(&sRepl);` |
|    83 | 2615 | `	PH7_MemObjRelease(&sOut);` |
|    83 | 2616 | `	if( rc != PH7_OK ){` |
|   ! 0 | 2617 | `		return rc;` |
|     - | 2618 | `	}` |
|    83 | 2619 | `	ph7_result_bool(pCtx,(iFlags & REGIT_INVERTED) ? !bOk : bOk);` |
|    83 | 2620 | `	return PH7_OK;` |
|    45 | 2621 | `}` |
|     4 | 2622 | `static int vm_builtin_RegexIterator_getRegex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2623 | `{` |
|     5 | 2624 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 2625 | `	ph7_value *pRe;` |
|     2 | 2626 | `	SXUNUSED(nArg);` |
|     2 | 2627 | `	SXUNUSED(apArg);` |
|     5 | 2628 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 | 2629 | `		return DualNotReady(pCtx);` |
|     - | 2630 | `	}` |
|     5 | 2631 | `	pRe = PH7_NativeAttr(pThis,IT_RE);` |
|     5 | 2632 | `	if( pRe ){` |
|     5 | 2633 | `		ph7_result_value(pCtx,pRe);` |
|     2 | 2634 | `	}` |
|     5 | 2635 | `	return PH7_OK;` |
|     3 | 2636 | `}` |
|     - | 2637 | `/* The three getters and the three setters are one pair per slot; only setMode()` |
|     - | 2638 | ` * screens its value, which is php's own asymmetry (setFlags/setPregFlags take` |
|     - | 2639 | ` * any integer). */` |
|    18 | 2640 | `static int RegitGet(ph7_context *pCtx,const char *zSlot)` |
|     1 | 2641 | `{` |
|    19 | 2642 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    19 | 2643 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 | 2644 | `		return DualNotReady(pCtx);` |
|     - | 2645 | `	}` |
|    19 | 2646 | `	ph7_result_int64(pCtx,PH7_NativeAttrInt(pThis,zSlot));` |
|    19 | 2647 | `	return PH7_OK;` |
|    10 | 2648 | `}` |
|     8 | 2649 | `static int RegitSet(ph7_context *pCtx,int nArg,ph7_value **apArg,const char *zSlot)` |
|     1 | 2650 | `{` |
|     9 | 2651 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     9 | 2652 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 | 2653 | `		return DualNotReady(pCtx);` |
|     - | 2654 | `	}` |
|     9 | 2655 | `	if( nArg > 0 ){` |
|     9 | 2656 | `		PH7_NativeSetAttrInt(pCtx->pVm,pThis,zSlot,ph7_value_to_int64(apArg[0]));` |
|     4 | 2657 | `	}` |
|     9 | 2658 | `	return PH7_OK;` |
|     5 | 2659 | `}` |
|     6 | 2660 | `static int vm_builtin_RegexIterator_getMode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2661 | `{` |
|     3 | 2662 | `	SXUNUSED(nArg);` |
|     3 | 2663 | `	SXUNUSED(apArg);` |
|     7 | 2664 | `	return RegitGet(pCtx,IT_RM);` |
|     1 | 2665 | `}` |
|     6 | 2666 | `static int vm_builtin_RegexIterator_setMode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2667 | `{` |
|     7 | 2668 | `	if( nArg > 0 ){` |
|     7 | 2669 | `		sxi64 iMode = ph7_value_to_int64(apArg[0]);` |
|     7 | 2670 | `		if( iMode < PH7_REGIT_MATCH \|\| iMode > PH7_REGIT_REPLACE ){` |
|     - | 2671 | `			/* php screens the VALUE before it even fetches the object. */` |
|     3 | 2672 | `			return RegitBadMode(pCtx,"RegexIterator::setMode(): Argument #1 ($mode)");` |
|     - | 2673 | `		}` |
|     2 | 2674 | `	}` |
|     5 | 2675 | `	return RegitSet(pCtx,nArg,apArg,IT_RM);` |
|     4 | 2676 | `}` |
|     6 | 2677 | `static int vm_builtin_RegexIterator_getFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2678 | `{` |
|     3 | 2679 | `	SXUNUSED(nArg);` |
|     3 | 2680 | `	SXUNUSED(apArg);` |
|     7 | 2681 | `	return RegitGet(pCtx,IT_RF);` |
|     1 | 2682 | `}` |
|     2 | 2683 | `static int vm_builtin_RegexIterator_setFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2684 | `{` |
|     3 | 2685 | `	return RegitSet(pCtx,nArg,apArg,IT_RF);` |
|     1 | 2686 | `}` |
|     6 | 2687 | `static int vm_builtin_RegexIterator_getPregFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2688 | `{` |
|     3 | 2689 | `	SXUNUSED(nArg);` |
|     3 | 2690 | `	SXUNUSED(apArg);` |
|     7 | 2691 | `	return RegitGet(pCtx,IT_RP);` |
|     1 | 2692 | `}` |
|     2 | 2693 | `static int vm_builtin_RegexIterator_setPregFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2694 | `{` |
|     3 | 2695 | `	return RegitSet(pCtx,nArg,apArg,IT_RP);` |
|     1 | 2696 | `}` |
|     - | 2697 | `/*` |
|     - | 2698 | ` * ---------------------------------------------------------------------------` |
|     - | 2699 | ` * AppendIterator: an IteratorIterator whose inner iterator is whatever entry a` |
|     - | 2700 | ` * real ArrayIterator is currently pointing at.` |
|     - | 2701 | ` *` |
|     - | 2702 | `` * php keeps the appended iterators in an actual `ArrayIterator` INSTANCE`` |
|     - | 2703 | `` * (`u.append.zarrayit`, the object getArrayIterator() hands out) and walks it with`` |
|     - | 2704 | `` * a cursor over the SAME storage (`u.append.iterator`). Both halves are`` |
|     - | 2705 | ` * php-visible and the chunk had neither: it kept a private PHP array and answered` |
|     - | 2706 | ` * getArrayIterator() with a fresh ArrayIterator over a COPY, so appending through` |
|     - | 2707 | `` * the returned object iterated nothing and `$ai->rewind()` did not restart the`` |
|     - | 2708 | `` * walk. The list cursor here is that one ArrayIterator's own `pCur`, driven`` |
|     - | 2709 | ` * directly the way php drives its iterator funcs -- not through the class's` |
|     - | 2710 | ` * methods, which php does not call either.` |
|     - | 2711 | ` */` |
|   438 | 2712 | `static ph7_class_instance * ApList(ph7_class_instance *pThis)` |
|     1 | 2713 | `{` |
|   439 | 2714 | `	return pThis ? PH7_NativeAttrObj(pThis,AP_LIST) : 0;` |
|     1 | 2715 | `}` |
|   352 | 2716 | `static ph7_hashmap * ApMap(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 | 2717 | `{` |
|   353 | 2718 | `	return SplStore(pVm,ApList(pThis));` |
|     1 | 2719 | `}` |
|     - | 2720 | `/* The iterator the list cursor points at, or 0 past the end. */` |
|   118 | 2721 | `static ph7_class_instance * ApCurrent(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 | 2722 | `{` |
|   119 | 2723 | `	ph7_hashmap *pMap = ApMap(pVm,pThis);` |
|   119 | 2724 | `	ph7_value *pVal = (pMap && pMap->pCur) ? HashmapExtractNodeValue(pMap->pCur) : 0;` |
|   119 | 2725 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|    31 | 2726 | `		return 0;` |
|     - | 2727 | `	}` |
|    89 | 2728 | `	return (ph7_class_instance *)pVal->x.pOther;` |
|    60 | 2729 | `}` |
|    28 | 2730 | `static void ApListRewind(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 | 2731 | `{` |
|    29 | 2732 | `	ph7_hashmap *pMap = ApMap(pVm,pThis);` |
|    29 | 2733 | `	if( pMap ){` |
|    29 | 2734 | `		pMap->pCur = pMap->pFirst;` |
|    14 | 2735 | `	}` |
|    29 | 2736 | `}` |
|    50 | 2737 | `static void ApListNext(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 | 2738 | `{` |
|    51 | 2739 | `	ph7_hashmap *pMap = ApMap(pVm,pThis);` |
|    51 | 2740 | `	if( pMap && pMap->pCur ){` |
|    51 | 2741 | `		pMap->pCur = pMap->pCur->pPrev;   /* insertion order: pFirst, then the pPrev chain */` |
|    25 | 2742 | `	}` |
|    51 | 2743 | `}` |
|     - | 2744 | `/*` |
|     - | 2745 | ` * php's spl_append_it_next_iterator: drop the cache and the current inner, then` |
|     - | 2746 | ` * adopt whatever the list cursor points at (rewound). *pbOk is php's SUCCESS --` |
|     - | 2747 | ` * false means the list is exhausted and this iterator has nothing left.` |
|     - | 2748 | ` */` |
|   118 | 2749 | `static sxi32 ApAdoptCurrent(ph7_vm *pVm,ph7_class_instance *pThis,int *pbOk)` |
|     1 | 2750 | `{` |
|     - | 2751 | `	ph7_class_instance *pIt;` |
|   119 | 2752 | `	*pbOk = 0;` |
|   119 | 2753 | `	DualFree(pVm,pThis);` |
|   119 | 2754 | `	PH7_NativeSetAttrObj(pVm,pThis,IT_IN,0);` |
|   119 | 2755 | `	PH7_NativeSetAttrObj(pVm,pThis,IT_IT,0);` |
|   119 | 2756 | `	pIt = ApCurrent(pVm,pThis);` |
|   119 | 2757 | `	if( pIt == 0 ){` |
|    31 | 2758 | `		return SXRET_OK;` |
|     - | 2759 | `	}` |
|    89 | 2760 | `	PH7_NativeSetAttrObj(pVm,pThis,IT_IN,pIt);` |
|    89 | 2761 | `	PH7_NativeSetAttrObj(pVm,pThis,IT_IT,pIt);` |
|    89 | 2762 | `	*pbOk = 1;` |
|    89 | 2763 | `	return DualRewindInner(pVm,pThis);` |
|    60 | 2764 | `}` |
|     - | 2765 | `/*` |
|     - | 2766 | ` * php's spl_append_it_fetch: step over every exhausted inner iterator, then fill` |
|     - | 2767 | ` * the cache without re-asking valid() (php's check_more = 0 -- the loop above just` |
|     - | 2768 | ` * established it).` |
|     - | 2769 | ` */` |
|   114 | 2770 | `static sxi32 ApFetch(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 | 2771 | `{` |
|    77 | 2772 | `	for(;;){` |
|   135 | 2773 | `		int bValid = 0, bOk = 0;` |
|   135 | 2774 | `		sxi32 rc = DualInnerValid(pVm,pThis,&bValid);` |
|   135 | 2775 | `		if( rc != SXRET_OK ){` |
|   ! 0 | 2776 | `			return rc;` |
|     - | 2777 | `		}` |
|   135 | 2778 | `		if( bValid ){` |
|    85 | 2779 | `			break;` |
|     - | 2780 | `		}` |
|    51 | 2781 | `		ApListNext(pVm,pThis);` |
|    51 | 2782 | `		rc = ApAdoptCurrent(pVm,pThis,&bOk);` |
|    51 | 2783 | `		if( rc != SXRET_OK ){` |
|   ! 0 | 2784 | `			return rc;` |
|     - | 2785 | `		}` |
|    51 | 2786 | `		if( !bOk ){` |
|    31 | 2787 | `			return SXRET_OK;   /* nothing left: the cache stays empty and valid() is false */` |
|     - | 2788 | `		}` |
|     1 | 2789 | `	}` |
|    85 | 2790 | `	return DualFetch(pVm,pThis,FALSE);` |
|    58 | 2791 | `}` |
|    48 | 2792 | `static int vm_builtin_AppendIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2793 | `{` |
|    49 | 2794 | `	ph7_vm *pVm = pCtx->pVm;` |
|    49 | 2795 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 2796 | `	ph7_class_instance *pList;` |
|     - | 2797 | `	ph7_class *pClass;` |
|     - | 2798 | `	ph7_class_method *pCons;` |
|    24 | 2799 | `	SXUNUSED(nArg);` |
|    24 | 2800 | `	SXUNUSED(apArg);` |
|    49 | 2801 | `	if( pThis == 0 ){` |
|   ! 0 | 2802 | `		return PH7_OK;` |
|     - | 2803 | `	}` |
|    49 | 2804 | `	if( ApList(pThis) != 0 ){` |
|     - | 2805 | `		/* php's "already built" refusal, worded from the DECLARING class as everywhere` |
|     - | 2806 | `		 * else in the family. */` |
|   ! 0 | 2807 | `		return PH7_VmThrowException(pCtx,"BadMethodCallException",` |
|     - | 2808 | `			"AppendIterator::getIterator() must be called exactly once per instance");` |
|     - | 2809 | `	}` |
|    49 | 2810 | `	pClass = PH7_VmExtractClass(pVm,"ArrayIterator",sizeof("ArrayIterator")-1,FALSE,0);` |
|    49 | 2811 | `	if( pClass == 0 ){` |
|   ! 0 | 2812 | `		return PH7_OK;` |
|     - | 2813 | `	}` |
|    49 | 2814 | `	pList = PH7_NewClassInstance(pVm,pClass);` |
|    49 | 2815 | `	if( pList == 0 ){` |
|   ! 0 | 2816 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 2817 | `	}` |
|    49 | 2818 | `	pList->iRef++;` |
|    49 | 2819 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|    49 | 2820 | `	if( pCons ){` |
|    49 | 2821 | `		PH7_VmCallClassMethod(pVm,pList,pCons,0,0,0);` |
|    24 | 2822 | `	}` |
|    49 | 2823 | `	PH7_NativeSetAttrObj(pVm,pThis,AP_LIST,pList);   /* the slot takes its own reference */` |
|    49 | 2824 | `	PH7_ClassInstanceUnref(pList);` |
|    49 | 2825 | `	return PH7_OK;` |
|    25 | 2826 | `}` |
|     - | 2827 | `/*` |
|     - | 2828 | ` * append(). php's own sequence, and every branch of it is observable:` |
|     - | 2829 | ` *   - a list cursor sitting on a LIVE entry whose cache is empty means the walk has` |
|     - | 2830 | ` *     consumed that entry, so the new iterator goes in behind it and the cursor steps` |
|     - | 2831 | ` *     over;` |
|     - | 2832 | ` *   - if nothing is being iterated yet (or the cache is empty), the cursor is walked` |
|     - | 2833 | ` *     forward until it reaches the iterator just appended, and the fetch resumes there.` |
|     - | 2834 | ` * That second half is what makes an AppendIterator RESUME after exhaustion, and it` |
|     - | 2835 | ` * relies on ArrayIterator::append() reviving a cursor that ran off the end (see` |
|     - | 2836 | ` * SplStoreInsert).` |
|     - | 2837 | ` */` |
|    58 | 2838 | `static int vm_builtin_AppendIterator_append(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2839 | `{` |
|    59 | 2840 | `	ph7_vm *pVm = pCtx->pVm;` |
|    59 | 2841 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 2842 | `	ph7_class_instance *pIt;` |
|     - | 2843 | `	ph7_hashmap *pMap;` |
|    59 | 2844 | `	int bListValid,bInnerValid = 0,nGuard;` |
|     - | 2845 | `	sxi32 rc;` |
|    59 | 2846 | `	if( !DualReady(pThis) ){` |
|   ! 0 | 2847 | `		return DualNotReady(pCtx);` |
|     - | 2848 | `	}` |
|    59 | 2849 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|   ! 0 | 2850 | `		return PH7_OK;   /* the shared ZPP screen already refused a non-Iterator */` |
|     - | 2851 | `	}` |
|    59 | 2852 | `	pIt = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    59 | 2853 | `	pMap = ApMap(pVm,pThis);` |
|    59 | 2854 | `	bListValid = pMap && pMap->pCur;` |
|     - | 2855 | `	/* php's spl_dual_it_valid, both times it appears below: the INNER iterator's` |
|     - | 2856 | `	 * valid() (false when there is no inner at all), NOT the cache. */` |
|    59 | 2857 | `	rc = DualInnerValid(pVm,pThis,&bInnerValid);` |
|    59 | 2858 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 2859 | `		return rc;` |
|     - | 2860 | `	}` |
|    59 | 2861 | `	pMap = ApMap(pVm,pThis);   /* that call ran user code: re-resolve */` |
|    59 | 2862 | `	if( pMap ){` |
|    59 | 2863 | `		SplStoreInsert(pMap,0,apArg[0]);` |
|    29 | 2864 | `	}` |
|    59 | 2865 | `	if( bListValid && !bInnerValid ){` |
|   ! 0 | 2866 | `		ApListNext(pVm,pThis);` |
|   ! 0 | 2867 | `	}` |
|    59 | 2868 | `	if( PH7_NativeAttrObj(pThis,IT_IT) != 0 && bInnerValid ){` |
|    19 | 2869 | `		return PH7_OK;   /* mid-walk with a live element: the new tail waits its turn */` |
|     - | 2870 | `	}` |
|    41 | 2871 | `	pMap = ApMap(pVm,pThis);` |
|    41 | 2872 | `	if( pMap && pMap->pCur == 0 ){` |
|   ! 0 | 2873 | `		ApListRewind(pVm,pThis);` |
|   ! 0 | 2874 | `	}` |
|    21 | 2875 | `	for( nGuard = 0 ; ; ++nGuard ){` |
|    41 | 2876 | `		int bOk = 0;` |
|    41 | 2877 | `		rc = ApAdoptCurrent(pVm,pThis,&bOk);` |
|    41 | 2878 | `		if( rc != SXRET_OK ){` |
|   ! 0 | 2879 | `			return rc;` |
|     - | 2880 | `		}` |
|    41 | 2881 | `		if( !bOk \|\| PH7_NativeAttrObj(pThis,IT_IN) == pIt ){` |
|    21 | 2882 | `			break;` |
|     - | 2883 | `		}` |
|   ! 0 | 2884 | `		ApListNext(pVm,pThis);` |
|   ! 0 | 2885 | `		if( nGuard > 100000 ){` |
|   ! 0 | 2886 | `			break;   /* php's loop has no bound; ours refuses to spin on a mutated list */` |
|     - | 2887 | `		}` |
|   ! 0 | 2888 | `	}` |
|    41 | 2889 | `	rc = ApFetch(pVm,pThis);` |
|    41 | 2890 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|    30 | 2891 | `}` |
|    28 | 2892 | `static int vm_builtin_AppendIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2893 | `{` |
|    29 | 2894 | `	ph7_vm *pVm = pCtx->pVm;` |
|    29 | 2895 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 2896 | `	sxi32 rc;` |
|    29 | 2897 | `	int bOk = 0;` |
|    14 | 2898 | `	SXUNUSED(nArg);` |
|    14 | 2899 | `	SXUNUSED(apArg);` |
|    29 | 2900 | `	if( !DualReady(pThis) ){` |
|   ! 0 | 2901 | `		return DualNotReady(pCtx);` |
|     - | 2902 | `	}` |
|    29 | 2903 | `	ApListRewind(pVm,pThis);` |
|    29 | 2904 | `	rc = ApAdoptCurrent(pVm,pThis,&bOk);` |
|    29 | 2905 | `	if( rc == SXRET_OK && bOk ){` |
|    29 | 2906 | `		rc = ApFetch(pVm,pThis);` |
|    14 | 2907 | `	}` |
|    29 | 2908 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|    15 | 2909 | `}` |
|    46 | 2910 | `static int vm_builtin_AppendIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2911 | `{` |
|    47 | 2912 | `	ph7_vm *pVm = pCtx->pVm;` |
|    47 | 2913 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 2914 | `	sxi32 rc;` |
|    47 | 2915 | `	int bValid = 0;` |
|    23 | 2916 | `	SXUNUSED(nArg);` |
|    23 | 2917 | `	SXUNUSED(apArg);` |
|    47 | 2918 | `	if( !DualReady(pThis) ){` |
|   ! 0 | 2919 | `		return DualNotReady(pCtx);` |
|     - | 2920 | `	}` |
|    47 | 2921 | `	rc = DualInnerValid(pVm,pThis,&bValid);` |
|    47 | 2922 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 2923 | `		return rc;` |
|     - | 2924 | `	}` |
|    47 | 2925 | `	if( bValid ){` |
|    47 | 2926 | `		rc = DualNextInner(pVm,pThis);` |
|    47 | 2927 | `		if( rc != SXRET_OK ){` |
|   ! 0 | 2928 | `			return rc;` |
|     - | 2929 | `		}` |
|    23 | 2930 | `	}` |
|    47 | 2931 | `	rc = ApFetch(pVm,pThis);` |
|    47 | 2932 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|    24 | 2933 | `}` |
|     - | 2934 | `/* php re-fetches here (spl_dual_it_fetch with check_more), which is why an` |
|     - | 2935 | ` * AppendIterator FOLLOWS an inner iterator moved behind its back where every other` |
|     - | 2936 | ` * decorator answers its cache. */` |
|    64 | 2937 | `static int vm_builtin_AppendIterator_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2938 | `{` |
|    65 | 2939 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 2940 | `	sxi32 rc;` |
|    32 | 2941 | `	SXUNUSED(nArg);` |
|    32 | 2942 | `	SXUNUSED(apArg);` |
|    65 | 2943 | `	if( !DualReady(pThis) ){` |
|   ! 0 | 2944 | `		return DualNotReady(pCtx);` |
|     - | 2945 | `	}` |
|    65 | 2946 | `	rc = DualFetch(pCtx->pVm,pThis,TRUE);` |
|    65 | 2947 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 2948 | `		return rc;` |
|     - | 2949 | `	}` |
|    65 | 2950 | `	return DualResultSlot(pCtx,IT_CD);` |
|    33 | 2951 | `}` |
|     - | 2952 | `/* The list cursor's KEY, which is php's index into the appended iterators -- and` |
|     - | 2953 | ` * NULL once the cursor has run off the end, where the chunk kept answering the last` |
|     - | 2954 | ` * index it had seen. */` |
|    26 | 2955 | `static int vm_builtin_AppendIterator_getIteratorIndex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2956 | `{` |
|    27 | 2957 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 2958 | `	ph7_value *pSlot,*apCall[1];` |
|    13 | 2959 | `	SXUNUSED(nArg);` |
|    13 | 2960 | `	SXUNUSED(apArg);` |
|    27 | 2961 | `	if( !DualReady(pThis) ){` |
|   ! 0 | 2962 | `		return DualNotReady(pCtx);` |
|     - | 2963 | `	}` |
|    27 | 2964 | `	pSlot = SplStoreSlot(pCtx->pVm,ApList(pThis));` |
|    27 | 2965 | `	if( pSlot == 0 ){` |
|   ! 0 | 2966 | `		ph7_result_null(pCtx);` |
|   ! 0 | 2967 | `		return PH7_OK;` |
|     - | 2968 | `	}` |
|    27 | 2969 | `	apCall[0] = pSlot;` |
|    27 | 2970 | `	return ph7_hashmap_simple_key(pCtx,1,apCall);` |
|    14 | 2971 | `}` |
|     - | 2972 | `/*` |
|     - | 2973 | ` * ---------------------------------------------------------------------------` |
|     - | 2974 | ` * The RECURSIVE pair: RecursiveArrayIterator (an ArrayIterator that descends into` |
|     - | 2975 | ` * its own entries) and RecursiveFilterIterator (a FilterIterator that forwards the` |
|     - | 2976 | ` * two recursion methods to its inner iterator).` |
|     - | 2977 | ` *` |
|     - | 2978 | ` * RecursiveArrayIterator is where php's CHILD_ARRAYS_ONLY flag lives, and the` |
|     - | 2979 | ``  * chunk's two-line `is_array($c) \|\| is_object($c)` / `new $c($this->current())` `` |
|     - | 2980 | ` * ignored it in both directions: an OBJECT entry claimed children under a flag that` |
|     - | 2981 | ` * exists to say it has none, and the child iterator was built WITHOUT the parent's` |
|     - | 2982 | ` * flags, so the restriction lasted exactly one level. php also answers null rather` |
|     - | 2983 | ` * than descending when there is no current element, and hands back an entry that is` |
|     - | 2984 | ` * ALREADY an instance of the called class instead of wrapping it again.` |
|     - | 2985 | ` */` |
|     - | 2986 | `#define RAI_CHILD_ARRAYS_ONLY 4` |
|     - | 2987 | `/* The entry the store cursor is on, or 0 past the end (php's` |
|     - | 2988 | ` * zend_hash_get_current_data_ex, which every one of these four bodies starts with). */` |
|   344 | 2989 | `static ph7_value * RaiCurrentEntry(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 | 2990 | `{` |
|   345 | 2991 | `	ph7_hashmap *pMap = SplStore(pVm,pThis);` |
|   345 | 2992 | `	return (pMap && pMap->pCur) ? HashmapExtractNodeValue(pMap->pCur) : 0;` |
|     1 | 2993 | `}` |
|   244 | 2994 | `static int vm_builtin_RecursiveArrayIterator_hasChildren(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2995 | `{` |
|   245 | 2996 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   245 | 2997 | `	ph7_value *pEntry = RaiCurrentEntry(pCtx->pVm,pThis);` |
|   245 | 2998 | `	int bHas = 0;` |
|   122 | 2999 | `	SXUNUSED(nArg);` |
|   122 | 3000 | `	SXUNUSED(apArg);` |
|   245 | 3001 | `	if( pEntry ){` |
|   243 | 3002 | `		if( pEntry->iFlags & MEMOBJ_HASHMAP ){` |
|    93 | 3003 | `			bHas = 1;` |
|   197 | 3004 | `		}else if( pEntry->iFlags & MEMOBJ_OBJ ){` |
|     - | 3005 | `			/* php: an object is a child UNLESS the iterator was told arrays only. */` |
|     9 | 3006 | `			bHas = (PH7_NativeAttrInt(pThis,SPL_F) & RAI_CHILD_ARRAYS_ONLY) == 0;` |
|     4 | 3007 | `		}` |
|   121 | 3008 | `	}` |
|   245 | 3009 | `	ph7_result_bool(pCtx,bHas);` |
|   245 | 3010 | `	return PH7_OK;` |
|     1 | 3011 | `}` |
|   100 | 3012 | `static int vm_builtin_RecursiveArrayIterator_getChildren(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 3013 | `{` |
|   101 | 3014 | `	ph7_vm *pVm = pCtx->pVm;` |
|   101 | 3015 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   101 | 3016 | `	ph7_value *pEntry = RaiCurrentEntry(pVm,pThis);` |
|     - | 3017 | `	ph7_class_instance *pChild;` |
|     - | 3018 | `	ph7_class_method *pCons;` |
|     - | 3019 | `	ph7_value sEntry,sFlags,*apCtor[2];` |
|     - | 3020 | `	sxi64 iFlags;` |
|     - | 3021 | `	sxi32 rc;` |
|    50 | 3022 | `	SXUNUSED(nArg);` |
|    50 | 3023 | `	SXUNUSED(apArg);` |
|   101 | 3024 | `	if( pThis == 0 \|\| pEntry == 0 ){` |
|     3 | 3025 | `		ph7_result_null(pCtx);   /* php descends into nothing when nothing is current */` |
|     3 | 3026 | `		return PH7_OK;` |
|     - | 3027 | `	}` |
|    99 | 3028 | `	iFlags = PH7_NativeAttrInt(pThis,SPL_F);` |
|    99 | 3029 | `	if( pEntry->iFlags & MEMOBJ_OBJ ){` |
|     9 | 3030 | `		ph7_class_instance *pObj = (ph7_class_instance *)pEntry->x.pOther;` |
|     9 | 3031 | `		if( iFlags & RAI_CHILD_ARRAYS_ONLY ){` |
|     3 | 3032 | `			ph7_result_null(pCtx);` |
|     3 | 3033 | `			return PH7_OK;` |
|     - | 3034 | `		}` |
|     7 | 3035 | `		if( pObj && PH7_VmInstanceOf(pObj->pClass,pThis->pClass) ){` |
|     - | 3036 | `			/* Already one of us: php hands the entry back rather than wrapping it. */` |
|     3 | 3037 | `			SplResultBorrowed(pCtx,pObj);` |
|     3 | 3038 | `			return PH7_OK;` |
|     - | 3039 | `		}` |
|     2 | 3040 | `	}` |
|     - | 3041 | `	/* php's spl_instantiate_child_arg: the CALLED class, constructed with the entry` |
|     - | 3042 | `	 * AND the parent's flags -- which is what carries CHILD_ARRAYS_ONLY down. */` |
|    95 | 3043 | `	pChild = PH7_NewClassInstance(pVm,pThis->pClass);` |
|    95 | 3044 | `	if( pChild == 0 ){` |
|   ! 0 | 3045 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 3046 | `	}` |
|    95 | 3047 | `	pChild->iRef++;` |
|    95 | 3048 | `	PH7_MemObjInit(pVm,&sEntry);` |
|    95 | 3049 | `	PH7_MemObjStore(pEntry,&sEntry);` |
|    95 | 3050 | `	PH7_MemObjInitFromInt(pVm,&sFlags,iFlags);` |
|    95 | 3051 | `	apCtor[0] = &sEntry;` |
|    95 | 3052 | `	apCtor[1] = &sFlags;` |
|    95 | 3053 | `	pCons = PH7_ClassExtractMethod(pThis->pClass,"__construct",sizeof("__construct")-1);` |
|    95 | 3054 | `	rc = pCons ? PH7_VmCallClassMethod(pVm,pChild,pCons,0,2,apCtor) : SXRET_OK;` |
|    95 | 3055 | `	PH7_MemObjRelease(&sEntry);` |
|    95 | 3056 | `	PH7_MemObjRelease(&sFlags);` |
|    95 | 3057 | `	if( rc != SXRET_OK ){` |
|     7 | 3058 | `		PH7_ClassInstanceUnref(pChild);` |
|     7 | 3059 | `		return rc;` |
|     - | 3060 | `	}` |
|    89 | 3061 | `	PH7_NativeResultObject(pCtx,pChild);` |
|    89 | 3062 | `	PH7_ClassInstanceUnref(pChild);` |
|    89 | 3063 | `	return PH7_OK;` |
|    51 | 3064 | `}` |
|     - | 3065 | `/* RecursiveFilterIterator forwards both methods to the object getInnerIterator()` |
|     - | 3066 | ` * answers (php calls on inner.zobject), and wraps the children in ITS OWN class. */` |
|    20 | 3067 | `static int vm_builtin_RecursiveFilterIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 3068 | `{` |
|    21 | 3069 | `	return DualConstruct(pCtx,"RecursiveFilterIterator",nArg,apArg);` |
|     1 | 3070 | `}` |
|    30 | 3071 | `static sxi32 RfiCallInner(ph7_context *pCtx,const char *zName,sxu32 nName,ph7_value *pOut)` |
|     1 | 3072 | `{` |
|    31 | 3073 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    31 | 3074 | `	ph7_class_instance *pIn = pThis ? PH7_NativeAttrObj(pThis,IT_IN) : 0;` |
|    31 | 3075 | `	ph7_class_method *pMethod = pIn ? PH7_ClassExtractMethod(pIn->pClass,zName,nName) : 0;` |
|    31 | 3076 | `	if( pMethod == 0 ){` |
|   ! 0 | 3077 | `		return SXRET_OK;` |
|     - | 3078 | `	}` |
|    31 | 3079 | `	return PH7_VmCallClassMethod(pCtx->pVm,pIn,pMethod,pOut,0,0);` |
|    16 | 3080 | `}` |
|    20 | 3081 | `static int vm_builtin_RecursiveFilterIterator_hasChildren(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 3082 | `{` |
|    21 | 3083 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 3084 | `	ph7_value sRes;` |
|     - | 3085 | `	sxi32 rc;` |
|    10 | 3086 | `	SXUNUSED(nArg);` |
|    10 | 3087 | `	SXUNUSED(apArg);` |
|    21 | 3088 | `	if( !DualReady(pThis) ){` |
|   ! 0 | 3089 | `		return DualNotReady(pCtx);` |
|     - | 3090 | `	}` |
|    21 | 3091 | `	PH7_MemObjInit(pCtx->pVm,&sRes);` |
|    21 | 3092 | `	rc = RfiCallInner(pCtx,"hasChildren",sizeof("hasChildren")-1,&sRes);` |
|    21 | 3093 | `	if( rc == SXRET_OK ){` |
|    21 | 3094 | `		ph7_result_value(pCtx,&sRes);` |
|    10 | 3095 | `	}` |
|    21 | 3096 | `	PH7_MemObjRelease(&sRes);` |
|    21 | 3097 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|    11 | 3098 | `}` |
|    10 | 3099 | `static int vm_builtin_RecursiveFilterIterator_getChildren(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 3100 | `{` |
|    11 | 3101 | `	ph7_vm *pVm = pCtx->pVm;` |
|    11 | 3102 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 3103 | `	ph7_class_instance *pChild;` |
|     - | 3104 | `	ph7_class_method *pCons;` |
|     - | 3105 | `	ph7_value sInner,*apCtor[1];` |
|     - | 3106 | `	sxi32 rc;` |
|     5 | 3107 | `	SXUNUSED(nArg);` |
|     5 | 3108 | `	SXUNUSED(apArg);` |
|    11 | 3109 | `	if( !DualReady(pThis) ){` |
|   ! 0 | 3110 | `		return DualNotReady(pCtx);` |
|     - | 3111 | `	}` |
|    11 | 3112 | `	PH7_MemObjInit(pVm,&sInner);` |
|    11 | 3113 | `	rc = RfiCallInner(pCtx,"getChildren",sizeof("getChildren")-1,&sInner);` |
|    11 | 3114 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3115 | `		PH7_MemObjRelease(&sInner);` |
|   ! 0 | 3116 | `		return rc;` |
|     - | 3117 | `	}` |
|    11 | 3118 | `	pChild = PH7_NewClassInstance(pVm,pThis->pClass);` |
|    11 | 3119 | `	if( pChild == 0 ){` |
|   ! 0 | 3120 | `		PH7_MemObjRelease(&sInner);` |
|   ! 0 | 3121 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 3122 | `	}` |
|    11 | 3123 | `	pChild->iRef++;` |
|    11 | 3124 | `	apCtor[0] = &sInner;` |
|    11 | 3125 | `	pCons = PH7_ClassExtractMethod(pThis->pClass,"__construct",sizeof("__construct")-1);` |
|    11 | 3126 | `	rc = pCons ? PH7_VmCallClassMethod(pVm,pChild,pCons,0,1,apCtor) : SXRET_OK;` |
|    11 | 3127 | `	PH7_MemObjRelease(&sInner);` |
|    11 | 3128 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3129 | `		PH7_ClassInstanceUnref(pChild);` |
|   ! 0 | 3130 | `		return rc;` |
|     - | 3131 | `	}` |
|    11 | 3132 | `	PH7_NativeResultObject(pCtx,pChild);` |
|    11 | 3133 | `	PH7_ClassInstanceUnref(pChild);` |
|    11 | 3134 | `	return PH7_OK;` |
|     6 | 3135 | `}` |
|    12 | 3136 | `static int vm_builtin_AppendIterator_getArrayIterator(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 3137 | `{` |
|    13 | 3138 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 3139 | `	ph7_class_instance *pList;` |
|     6 | 3140 | `	SXUNUSED(nArg);` |
|     6 | 3141 | `	SXUNUSED(apArg);` |
|    13 | 3142 | `	if( !DualReady(pThis) ){` |
|   ! 0 | 3143 | `		return DualNotReady(pCtx);` |
|     - | 3144 | `	}` |
|    13 | 3145 | `	pList = ApList(pThis);` |
|    13 | 3146 | `	if( pList ){` |
|    13 | 3147 | `		SplResultBorrowed(pCtx,pList);` |
|     7 | 3148 | `	}else{` |
|   ! 0 | 3149 | `		ph7_result_null(pCtx);` |
|     - | 3150 | `	}` |
|    13 | 3151 | `	return PH7_OK;` |
|     7 | 3152 | `}` |
|     - | 3153 | `/*` |
|     - | 3154 | ` * The declarations. php's method ORDER is the order Reflection reports, so each` |
|     - | 3155 | ` * table follows spl_iterators.stub.php line for line; the parameter types are the` |
|     - | 3156 | `` * stub's too, which is what makes `Iterator $iterator` refuse an IteratorAggregate`` |
|     - | 3157 | ` * everywhere except IteratorIterator (the one class that declares Traversable and` |
|     - | 3158 | ` * unwraps).` |
|     - | 3159 | ` *` |
|     - | 3160 | ` * No RETURN type is declared, on purpose: php marks every one of these` |
|     - | 3161 | `` * `@tentative-return-type`, and a tentative type answers NULL from getReturnType()`` |
|     - | 3162 | ` * and false from hasReturnType() — which is exactly what an undeclared zRet answers` |
|     - | 3163 | `` * here. Declaring them would print `Return [ bool ]` where php prints`` |
|     - | 3164 | `` * `Tentative return [ bool ]` AND make getReturnType() disagree; leaving them off`` |
|     - | 3165 | ` * costs only getTentativeReturnType(). PHL has no tentative-return concept at all` |
|     - | 3166 | ` * (§7.4) — DateTime and the reflectors already report a plain return type where php` |
|     - | 3167 | ` * reports a tentative one.` |
|     - | 3168 | ` */` |
|  5146 | 3169 | `static sxi32 VmInstallSplDualIterators(ph7_vm *pVm)` |
|     5 | 3170 | `{` |
|     - | 3171 | `	static const PH7_NativePropDef aDualProp[] = {` |
|     - | 3172 | `		{ IT_IN, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - | 3173 | `		{ IT_IT, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - | 3174 | `		{ IT_CD, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - | 3175 | `		{ IT_CK, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - | 3176 | `		{ IT_CF, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - | 3177 | `		{ IT_CP, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - | 3178 | `	};` |
|     - | 3179 | `	static const PH7_NativePropDef aLimitProp[] = {` |
|     - | 3180 | `		{ IT_OFF, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - | 3181 | `		{ IT_LIM, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, -1, 0, 0.0 }, 0 },` |
|     - | 3182 | `	};` |
|     - | 3183 | `	static const PH7_NativePropDef aCbProp[] = {` |
|     - | 3184 | `		{ IT_CB, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - | 3185 | `	};` |
|     - | 3186 | `	static const PH7_NativeMethodDef aOuterMethod[] = {` |
|     - | 3187 | `		{ "getInnerIterator", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", 0, 0 },` |
|     - | 3188 | `	};` |
|     - | 3189 | `	static const PH7_NativeMethodDef aIterIterMethod[] = {` |
|     - | 3190 | `		{ "__construct",      PH7_MOD_PUBLIC, "Traversable $iterator, ?string $class = null", 0,` |
|     - | 3191 | `		  vm_builtin_IteratorIterator_construct },` |
|     - | 3192 | `		{ "getInnerIterator", PH7_MOD_PUBLIC, "", "@?Iterator", vm_builtin_Dual_getInnerIterator },` |
|     - | 3193 | `		{ "rewind",           PH7_MOD_PUBLIC, "", "@void", vm_builtin_IteratorIterator_rewind },` |
|     - | 3194 | `		{ "valid",            PH7_MOD_PUBLIC, "", "@bool", vm_builtin_Dual_valid },` |
|     - | 3195 | `		{ "key",              PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_Dual_key },` |
|     - | 3196 | `		{ "current",          PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_Dual_current },` |
|     - | 3197 | `		{ "next",             PH7_MOD_PUBLIC, "", "@void", vm_builtin_IteratorIterator_next },` |
|     - | 3198 | `	};` |
|     - | 3199 | `	static const PH7_NativeMethodDef aFilterMethod[] = {` |
|     - | 3200 | `		{ "accept",      PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@bool", 0 },` |
|     - | 3201 | `		{ "__construct", PH7_MOD_PUBLIC, "Iterator $iterator", 0, vm_builtin_FilterIterator_construct },` |
|     - | 3202 | `		{ "rewind",      PH7_MOD_PUBLIC, "", "@void", vm_builtin_FilterIterator_rewind },` |
|     - | 3203 | `		{ "next",        PH7_MOD_PUBLIC, "", "@void", vm_builtin_FilterIterator_next },` |
|     - | 3204 | `	};` |
|     - | 3205 | `	static const PH7_NativeMethodDef aCbFilterMethod[] = {` |
|     - | 3206 | `		{ "__construct", PH7_MOD_PUBLIC, "Iterator $iterator, callable $callback", 0,` |
|     - | 3207 | `		  vm_builtin_CallbackFilterIterator_construct },` |
|     - | 3208 | `		{ "accept",      PH7_MOD_PUBLIC, "", "@bool", vm_builtin_CallbackFilterIterator_accept },` |
|     - | 3209 | `	};` |
|     - | 3210 | `	static const PH7_NativeMethodDef aLimitMethod[] = {` |
|     - | 3211 | `		{ "__construct", PH7_MOD_PUBLIC, "Iterator $iterator, int $offset = 0, int $limit = -1", 0,` |
|     - | 3212 | `		  vm_builtin_LimitIterator_construct },` |
|     - | 3213 | `		{ "rewind",      PH7_MOD_PUBLIC, "", "@void", vm_builtin_LimitIterator_rewind },` |
|     - | 3214 | `		{ "valid",       PH7_MOD_PUBLIC, "", "@bool", vm_builtin_LimitIterator_valid },` |
|     - | 3215 | `		{ "next",        PH7_MOD_PUBLIC, "", "@void", vm_builtin_LimitIterator_next },` |
|     - | 3216 | `		{ "seek",        PH7_MOD_PUBLIC, "int $offset", "@int", vm_builtin_LimitIterator_seek },` |
|     - | 3217 | `		{ "getPosition", PH7_MOD_PUBLIC, "", "@int", vm_builtin_LimitIterator_getPosition },` |
|     - | 3218 | `	};` |
|     - | 3219 | `	static const PH7_NativeMethodDef aInfiniteMethod[] = {` |
|     - | 3220 | `		{ "__construct", PH7_MOD_PUBLIC, "Iterator $iterator", 0,` |
|     - | 3221 | `		  vm_builtin_InfiniteIterator_construct },` |
|     - | 3222 | `		{ "next",        PH7_MOD_PUBLIC, "", "@void", vm_builtin_InfiniteIterator_next },` |
|     - | 3223 | `	};` |
|     - | 3224 | `	static const PH7_NativeMethodDef aNoRewindMethod[] = {` |
|     - | 3225 | `		{ "__construct", PH7_MOD_PUBLIC, "Iterator $iterator", 0,` |
|     - | 3226 | `		  vm_builtin_NoRewindIterator_construct },` |
|     - | 3227 | `		{ "rewind",      PH7_MOD_PUBLIC, "", "@void", vm_builtin_NoRewindIterator_rewind },` |
|     - | 3228 | `		{ "valid",       PH7_MOD_PUBLIC, "", "@bool", vm_builtin_NoRewindIterator_valid },` |
|     - | 3229 | `		{ "key",         PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_NoRewindIterator_key },` |
|     - | 3230 | `		{ "current",     PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_NoRewindIterator_current },` |
|     - | 3231 | `		{ "next",        PH7_MOD_PUBLIC, "", "@void", vm_builtin_NoRewindIterator_next },` |
|     - | 3232 | `	};` |
|     - | 3233 | `	static const PH7_NativePropDef aRegexProp[] = {` |
|     - | 3234 | `		/* The one slot php PRESENTS, declared as php declares it: a ?string, so a` |
|     - | 3235 | ``		 * `$it->replacement = 5` coerces and an array is a TypeError. */`` |
|     - | 3236 | `		{ "replacement", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, "?string" },` |
|     - | 3237 | `		{ IT_RE, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },` |
|     - | 3238 | `		{ IT_RM, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - | 3239 | `		{ IT_RF, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - | 3240 | `		{ IT_RP, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - | 3241 | `	};` |
|     - | 3242 | `	static const PH7_NativeConstDef aRegexConst[] = {` |
|     - | 3243 | `		{ "USE_KEY",      PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, REGIT_USE_KEY,  0, 0.0 },` |
|     - | 3244 | `		{ "INVERT_MATCH", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, REGIT_INVERTED, 0, 0.0 },` |
|     - | 3245 | `		{ "MATCH",        PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PH7_REGIT_MATCH,       0, 0.0 },` |
|     - | 3246 | `		{ "GET_MATCH",    PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PH7_REGIT_GET_MATCH,   0, 0.0 },` |
|     - | 3247 | `		{ "ALL_MATCHES",  PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PH7_REGIT_ALL_MATCHES, 0, 0.0 },` |
|     - | 3248 | `		{ "SPLIT",        PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PH7_REGIT_SPLIT,       0, 0.0 },` |
|     - | 3249 | `		{ "REPLACE",      PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PH7_REGIT_REPLACE,     0, 0.0 },` |
|     - | 3250 | `	};` |
|     - | 3251 | `	static const PH7_NativeMethodDef aRegexMethod[] = {` |
|     - | 3252 | `		{ "__construct",  PH7_MOD_PUBLIC,` |
|     - | 3253 | ``		  /* php's stub spells this default `RegexIterator::MATCH`, and one zSig field`` |
|     - | 3254 | `		   * cannot say both the TEXT and the VALUE: the constant spelling prints php's` |
|     - | 3255 | `		   * export line but makes getDefaultValue() a "Failed to retrieve" throw, so the` |
|     - | 3256 | `		   * VALUE wins here, as it does in the aBuiltinSig rows with the same shape. */` |
|     - | 3257 | `		  "Iterator $iterator, string $pattern, int $mode = 0, int $flags = 0, int $pregFlags = 0", 0,` |
|     - | 3258 | `		  vm_builtin_RegexIterator_construct },` |
|     - | 3259 | `		{ "accept",       PH7_MOD_PUBLIC, "", "@bool", vm_builtin_RegexIterator_accept },` |
|     - | 3260 | `		{ "getMode",      PH7_MOD_PUBLIC, "", "@int", vm_builtin_RegexIterator_getMode },` |
|     - | 3261 | `		{ "setMode",      PH7_MOD_PUBLIC, "int $mode", "@void", vm_builtin_RegexIterator_setMode },` |
|     - | 3262 | `		{ "getFlags",     PH7_MOD_PUBLIC, "", "@int", vm_builtin_RegexIterator_getFlags },` |
|     - | 3263 | `		{ "setFlags",     PH7_MOD_PUBLIC, "int $flags", "@void", vm_builtin_RegexIterator_setFlags },` |
|     - | 3264 | `		{ "getRegex",     PH7_MOD_PUBLIC, "", "@string", vm_builtin_RegexIterator_getRegex },` |
|     - | 3265 | `		{ "getPregFlags", PH7_MOD_PUBLIC, "", "@int", vm_builtin_RegexIterator_getPregFlags },` |
|     - | 3266 | `		{ "setPregFlags", PH7_MOD_PUBLIC, "int $pregFlags", "@void", vm_builtin_RegexIterator_setPregFlags },` |
|     - | 3267 | `	};` |
|     - | 3268 | `	static const PH7_NativeMethodDef aRecursiveMethod[] = {` |
|     - | 3269 | `		{ "hasChildren", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", 0, 0 },` |
|     - | 3270 | `		{ "getChildren", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", 0, 0 },` |
|     - | 3271 | `	};` |
|     - | 3272 | `	static const PH7_NativeConstDef aRaiConst[] = {` |
|     - | 3273 | `		{ "CHILD_ARRAYS_ONLY", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, RAI_CHILD_ARRAYS_ONLY, 0, 0.0 },` |
|     - | 3274 | `	};` |
|     - | 3275 | `	static const PH7_NativeMethodDef aRaiMethod[] = {` |
|     - | 3276 | `		{ "hasChildren", PH7_MOD_PUBLIC, "", "@bool",` |
|     - | 3277 | `		  vm_builtin_RecursiveArrayIterator_hasChildren },` |
|     - | 3278 | `		{ "getChildren", PH7_MOD_PUBLIC, "", "@?RecursiveArrayIterator",` |
|     - | 3279 | `		  vm_builtin_RecursiveArrayIterator_getChildren },` |
|     - | 3280 | `	};` |
|     - | 3281 | `	static const PH7_NativeMethodDef aRfiMethod[] = {` |
|     - | 3282 | `		{ "__construct", PH7_MOD_PUBLIC, "RecursiveIterator $iterator", 0,` |
|     - | 3283 | `		  vm_builtin_RecursiveFilterIterator_construct },` |
|     - | 3284 | `		{ "hasChildren", PH7_MOD_PUBLIC, "", "@bool",` |
|     - | 3285 | `		  vm_builtin_RecursiveFilterIterator_hasChildren },` |
|     - | 3286 | `		{ "getChildren", PH7_MOD_PUBLIC, "", "@?RecursiveFilterIterator",` |
|     - | 3287 | `		  vm_builtin_RecursiveFilterIterator_getChildren },` |
|     - | 3288 | `	};` |
|     - | 3289 | `	static const PH7_NativePropDef aAppendProp[] = {` |
|     - | 3290 | `		{ AP_LIST, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - | 3291 | `	};` |
|     - | 3292 | `	static const PH7_NativeMethodDef aAppendMethod[] = {` |
|     - | 3293 | `		{ "__construct",      PH7_MOD_PUBLIC, "", 0, vm_builtin_AppendIterator_construct },` |
|     - | 3294 | `		{ "append",           PH7_MOD_PUBLIC, "Iterator $iterator", "@void",` |
|     - | 3295 | `		  vm_builtin_AppendIterator_append },` |
|     - | 3296 | `		{ "rewind",           PH7_MOD_PUBLIC, "", "@void", vm_builtin_AppendIterator_rewind },` |
|     - | 3297 | `		{ "valid",            PH7_MOD_PUBLIC, "", "@bool", vm_builtin_Dual_valid },` |
|     - | 3298 | `		{ "current",          PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_AppendIterator_current },` |
|     - | 3299 | `		{ "next",             PH7_MOD_PUBLIC, "", "@void", vm_builtin_AppendIterator_next },` |
|     - | 3300 | `		{ "getIteratorIndex", PH7_MOD_PUBLIC, "", "@?int",` |
|     - | 3301 | `		  vm_builtin_AppendIterator_getIteratorIndex },` |
|     - | 3302 | `		{ "getArrayIterator", PH7_MOD_PUBLIC, "", "@ArrayIterator",` |
|     - | 3303 | `		  vm_builtin_AppendIterator_getArrayIterator },` |
|     - | 3304 | `	};` |
|     - | 3305 | `	static const PH7_NativeMethodDef aEmptyMethod[] = {` |
|     - | 3306 | `		{ "current", PH7_MOD_PUBLIC, "", "@never", vm_builtin_EmptyIterator_current },` |
|     - | 3307 | `		{ "next",    PH7_MOD_PUBLIC, "", "@void", vm_builtin_EmptyIterator_nop },` |
|     - | 3308 | `		{ "key",     PH7_MOD_PUBLIC, "", "@never", vm_builtin_EmptyIterator_key },` |
|     - | 3309 | `		{ "valid",   PH7_MOD_PUBLIC, "", "@false", vm_builtin_EmptyIterator_valid },` |
|     - | 3310 | `		{ "rewind",  PH7_MOD_PUBLIC, "", "@void", vm_builtin_EmptyIterator_nop },` |
|     - | 3311 | `	};` |
|     - | 3312 | `	/*` |
|     - | 3313 | ``	 * PH7_CLASS_NOCLONE on every dual iterator: php refuses `clone` for all of them`` |
|     - | 3314 | `	 * (its inner iterator handle cannot be duplicated), and a slot-by-slot copy here` |
|     - | 3315 | `	 * would share the inner iterator's cursor between two decorators. EmptyIterator` |
|     - | 3316 | `	 * has no state and php clones it happily.` |
|     - | 3317 | `	 */` |
|     - | 3318 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - | 3319 | `		{ "OuterIterator", "Iterator", 0, PH7_CLASS_INTERFACE,` |
|     - | 3320 | `		  aOuterMethod, SX_ARRAYSIZE(aOuterMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - | 3321 | `		{ "IteratorIterator", 0, "OuterIterator", PH7_CLASS_NOCLONE,` |
|     - | 3322 | `		  aIterIterMethod, SX_ARRAYSIZE(aIterIterMethod), 0, 0,` |
|     - | 3323 | `		  aDualProp, SX_ARRAYSIZE(aDualProp), 0, 0, 0 },` |
|     - | 3324 | `		{ "FilterIterator", "IteratorIterator", 0, PH7_CLASS_ABSTRACT\|PH7_CLASS_NOCLONE,` |
|     - | 3325 | `		  aFilterMethod, SX_ARRAYSIZE(aFilterMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - | 3326 | `		{ "CallbackFilterIterator", "FilterIterator", 0, PH7_CLASS_NOCLONE,` |
|     - | 3327 | `		  aCbFilterMethod, SX_ARRAYSIZE(aCbFilterMethod), 0, 0,` |
|     - | 3328 | `		  aCbProp, SX_ARRAYSIZE(aCbProp), 0, 0, 0 },` |
|     - | 3329 | `		{ "LimitIterator", "IteratorIterator", 0, PH7_CLASS_NOCLONE,` |
|     - | 3330 | `		  aLimitMethod, SX_ARRAYSIZE(aLimitMethod), 0, 0,` |
|     - | 3331 | `		  aLimitProp, SX_ARRAYSIZE(aLimitProp), 0, 0, 0 },` |
|     - | 3332 | `		{ "InfiniteIterator", "IteratorIterator", 0, PH7_CLASS_NOCLONE,` |
|     - | 3333 | `		  aInfiniteMethod, SX_ARRAYSIZE(aInfiniteMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - | 3334 | `		{ "NoRewindIterator", "IteratorIterator", 0, PH7_CLASS_NOCLONE,` |
|     - | 3335 | `		  aNoRewindMethod, SX_ARRAYSIZE(aNoRewindMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - | 3336 | `		{ "RegexIterator", "FilterIterator", 0, PH7_CLASS_NOCLONE,` |
|     - | 3337 | `		  aRegexMethod, SX_ARRAYSIZE(aRegexMethod),` |
|     - | 3338 | `		  aRegexConst, SX_ARRAYSIZE(aRegexConst),` |
|     - | 3339 | `		  aRegexProp, SX_ARRAYSIZE(aRegexProp), 0, 0, 0 },` |
|     - | 3340 | `		{ "AppendIterator", "IteratorIterator", 0, PH7_CLASS_NOCLONE,` |
|     - | 3341 | `		  aAppendMethod, SX_ARRAYSIZE(aAppendMethod), 0, 0,` |
|     - | 3342 | `		  aAppendProp, SX_ARRAYSIZE(aAppendProp), 0, 0, 0 },` |
|     - | 3343 | `		{ "RecursiveIterator", "Iterator", 0, PH7_CLASS_INTERFACE,` |
|     - | 3344 | `		  aRecursiveMethod, SX_ARRAYSIZE(aRecursiveMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - | 3345 | `		/* RecursiveArrayIterator is CLONEABLE (php clones an ArrayIterator happily) and` |
|     - | 3346 | `		 * inherits every one of its parent's C bodies, storage slots included. */` |
|     - | 3347 | `		{ "RecursiveArrayIterator", "ArrayIterator", "RecursiveIterator", 0,` |
|     - | 3348 | `		  aRaiMethod, SX_ARRAYSIZE(aRaiMethod),` |
|     - | 3349 | `		  aRaiConst, SX_ARRAYSIZE(aRaiConst), 0, 0, 0, 0, 0 },` |
|     - | 3350 | `		{ "RecursiveFilterIterator", "FilterIterator", "RecursiveIterator",` |
|     - | 3351 | `		  PH7_CLASS_ABSTRACT\|PH7_CLASS_NOCLONE,` |
|     - | 3352 | `		  aRfiMethod, SX_ARRAYSIZE(aRfiMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - | 3353 | `		{ "EmptyIterator", 0, "Iterator", 0,` |
|     - | 3354 | `		  aEmptyMethod, SX_ARRAYSIZE(aEmptyMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - | 3355 | `	};` |
|  5151 | 3356 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|     5 | 3357 | `}` |
|     - | 3358 | `/*` |
|     - | 3359 | ` * ---------------------------------------------------------------------------` |
|     - | 3360 | ` * RecursiveIteratorIterator.` |
|     - | 3361 | ` *` |
|     - | 3362 | `` * php's `spl_recursive_it_object` is a STACK OF LEVELS plus a five-value state`` |
|     - | 3363 | ` * machine, and reading the struct before the methods (rule 43) is what this` |
|     - | 3364 | ` * conversion turns on. Each level carries the sub-iterator AND its own` |
|     - | 3365 | `` * RecursiveIteratorState; `move_forward` is one loop over that pair, and every`` |
|     - | 3366 | ` * method is a two-line reader of it. The chunk instead kept a stack of iterators` |
|     - | 3367 | `` * with the state implied by two booleans (`__post`, `__live`), which is where all`` |
|     - | 3368 | ` * eight of its divergences came from:` |
|     - | 3369 | ` *` |
|     - | 3370 | ` *   - getDepth()/getSubIterator()/getInnerIterator() answered from an EMPTY stack` |
|     - | 3371 | ` *     before the first rewind(), so they reported -1 and null where php reports 0` |
|     - | 3372 | ` *     and the root -- php seeds level 0 in the CONSTRUCTOR and never unseeds it.` |
|     - | 3373 | `` *   - valid() answered a `__live` flag that only rewind() sets; php ASKS the`` |
|     - | 3374 | ` *     levels (any valid sub-iterator, walking down), so a fresh instance over a` |
|     - | 3375 | ` *     non-empty iterator is already valid().` |
|     - | 3376 | ` *   - LEAVES_ONLY past max depth YIELDED the container; php skips it, which is` |
|     - | 3377 | ``  *     the whole point of the mode (`walk-leaves-maxdepth0` returned the `b` `` |
|     - | 3378 | ` *     array as if it were a leaf).` |
|     - | 3379 | `` *   - the mode was `$mode \| $flags` masked with & 3, so CATCH_GET_CHILD passed`` |
|     - | 3380 | ` *     as $mode descended like LEAVES_ONLY; php compares mode EXACTLY and an` |
|     - | 3381 | ` *     unknown mode matches no arm at all, descending nowhere.` |
|     - | 3382 | ` *   - hasChildren() was called on the sub-iterator DIRECTLY, so a subclass` |
|     - | 3383 | ` *     overriding callHasChildren() -- php's documented hook -- was never asked.` |
|     - | 3384 | ` *   - endChildren() ran AFTER the pop, reporting a depth one too low and firing` |
|     - | 3385 | ` *     a spurious final call at depth -1; php calls it before the pop.` |
|     - | 3386 | ` *   - a second rewind() fired beginIteration() again; php's in_iteration latch` |
|     - | 3387 | ` *     makes it once per iteration.` |
|     - | 3388 | ` *   - getChildren() returning a non-RecursiveIterator was silently treated as` |
|     - | 3389 | ` *     "no children"; php throws UnexpectedValueException.` |
|     - | 3390 | ` *` |
|     - | 3391 | ` * The level stack lives in two parallel arrays indexed by level rather than in a` |
|     - | 3392 | `` * C block behind a handle: php SERIALIZES this class (`O:25:"…":0:{}`), and a raw`` |
|     - | 3393 | ` * pointer in a hidden slot is exactly what rule 19 exists to keep out of` |
|     - | 3394 | ` * serialize() output. Every slot is PH7_MOD_HIDDEN, so php's zero-property` |
|     - | 3395 | ` * presentation holds for var_dump, print_r, var_export, (array) and Reflection.` |
|     - | 3396 | ` */` |
|     - | 3397 | `#define RIT_ST   "__st"   /* php's iterators[level].zobject */` |
|     - | 3398 | `#define RIT_SS   "__ss"   /* php's iterators[level].state */` |
|     - | 3399 | `#define RIT_LVL  "__lvl"  /* php's object->level */` |
|     - | 3400 | `#define RIT_MD   "__md"   /* php's object->mode, stored UNMASKED */` |
|     - | 3401 | `#define RIT_FL   "__fl"   /* php's object->flags */` |
|     - | 3402 | `#define RIT_MX   "__mx"   /* php's object->max_depth, -1 = unlimited */` |
|     - | 3403 | `#define RIT_II   "__ii"   /* php's object->in_iteration */` |
|     - | 3404 | ``#define RIT_RD   "__rd"   /* php's `object->iterators != NULL`: the parent ctor ran */`` |
|     - | 3405 |  |
|     - | 3406 | `/* php's RecursiveIteratorState */` |
|     - | 3407 | `#define RS_NEXT  0` |
|     - | 3408 | `#define RS_TEST  1` |
|     - | 3409 | `#define RS_SELF  2` |
|     - | 3410 | `#define RS_CHILD 3` |
|     - | 3411 | `#define RS_START 4` |
|     - | 3412 |  |
|     - | 3413 | `/* php's RecursiveIteratorMode + the one flag */` |
|     - | 3414 | `#define RIT_LEAVES_ONLY     0` |
|     - | 3415 | `#define RIT_SELF_FIRST      1` |
|     - | 3416 | `#define RIT_CHILD_FIRST     2` |
|     - | 3417 | `#define RIT_CATCH_GET_CHILD 16` |
|     - | 3418 |  |
|     - | 3419 | `/*` |
|     - | 3420 | `` * php's `object->iterators != NULL`. Its get_method handler refuses EVERY method`` |
|     - | 3421 | ` * on an instance whose parent constructor never ran -- not the individual bodies,` |
|     - | 3422 | ` * which is why the refusal is an Error naming the RUNTIME class and why even` |
|     - | 3423 | ` * getDepth() raises it.` |
|     - | 3424 | ` */` |
|  1844 | 3425 | `static int RitReady(ph7_class_instance *pThis)` |
|     1 | 3426 | `{` |
|  1845 | 3427 | `	return pThis && PH7_NativeAttrInt(pThis,RIT_RD) != 0;` |
|     1 | 3428 | `}` |
|    10 | 3429 | `static sxi32 RitNotReady(ph7_context *pCtx)` |
|     1 | 3430 | `{` |
|    11 | 3431 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    11 | 3432 | `	SyString *pName = pThis ? &pThis->pClass->sName : 0;` |
|    16 | 3433 | `	return PH7_VmThrowException(pCtx,"Error",` |
|     5 | 3434 | `		"The %z instance wasn't initialized properly",pName);` |
|     1 | 3435 | `}` |
|  2670 | 3436 | `static int RitInt(ph7_class_instance *pThis,const char *zSlot)` |
|     1 | 3437 | `{` |
|  2671 | 3438 | `	return (int)PH7_NativeAttrInt(pThis,zSlot);` |
|     1 | 3439 | `}` |
|     - | 3440 | `/* One of the two level-indexed arrays, materialized on first use. */` |
|  3520 | 3441 | `static ph7_hashmap * RitMap(ph7_vm *pVm,ph7_class_instance *pThis,const char *zSlot)` |
|     1 | 3442 | `{` |
|  3521 | 3443 | `	ph7_value *pSlot = PH7_NativeAttr(pThis,zSlot);` |
|  3521 | 3444 | `	if( pSlot == 0 ){` |
|   ! 0 | 3445 | `		return 0;` |
|     - | 3446 | `	}` |
|  3521 | 3447 | `	if( (pSlot->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|   173 | 3448 | `		if( PH7_MemObjToHashmap(pSlot) != SXRET_OK ){` |
|   ! 0 | 3449 | `			return 0;` |
|     - | 3450 | `		}` |
|    86 | 3451 | `	}` |
|  3521 | 3452 | `	return PH7_HashmapCowSeparate(pVm,pSlot);` |
|  1761 | 3453 | `}` |
|  2304 | 3454 | `static ph7_value * RitAt(ph7_vm *pVm,ph7_class_instance *pThis,const char *zSlot,int iLevel)` |
|     1 | 3455 | `{` |
|  2305 | 3456 | `	ph7_hashmap *pMap = RitMap(pVm,pThis,zSlot);` |
|  2305 | 3457 | `	ph7_hashmap_node *pNode = 0;` |
|  2305 | 3458 | `	if( pMap == 0 \|\| HashmapLookupIntKey(pMap,(sxi64)iLevel,&pNode) != SXRET_OK ){` |
|   ! 0 | 3459 | `		return 0;` |
|     - | 3460 | `	}` |
|  2305 | 3461 | `	return HashmapExtractNodeValue(pNode);` |
|  1153 | 3462 | `}` |
|   908 | 3463 | `static void RitPut(ph7_vm *pVm,ph7_class_instance *pThis,const char *zSlot,int iLevel,ph7_value *pVal)` |
|     1 | 3464 | `{` |
|   909 | 3465 | `	ph7_hashmap *pMap = RitMap(pVm,pThis,zSlot);` |
|     - | 3466 | `	ph7_value sKey;` |
|   909 | 3467 | `	if( pMap == 0 ){` |
|   ! 0 | 3468 | `		return;` |
|     - | 3469 | `	}` |
|   909 | 3470 | `	PH7_MemObjInitFromInt(pVm,&sKey,(sxi64)iLevel);` |
|   909 | 3471 | `	PH7_HashmapInsert(pMap,&sKey,pVal);` |
|   909 | 3472 | `	PH7_MemObjRelease(&sKey);` |
|   455 | 3473 | `}` |
|   308 | 3474 | `static void RitErase(ph7_vm *pVm,ph7_class_instance *pThis,const char *zSlot,int iLevel)` |
|     1 | 3475 | `{` |
|   309 | 3476 | `	ph7_hashmap *pMap = RitMap(pVm,pThis,zSlot);` |
|   309 | 3477 | `	ph7_hashmap_node *pNode = 0;` |
|   309 | 3478 | `	if( pMap && HashmapLookupIntKey(pMap,(sxi64)iLevel,&pNode) == SXRET_OK ){` |
|   137 | 3479 | `		PH7_HashmapUnlinkNode(pNode,TRUE);` |
|    68 | 3480 | `	}` |
|   309 | 3481 | `}` |
|     - | 3482 | `/*` |
|     - | 3483 | ` * The sub-iterator at a level. Re-resolved on every use on purpose: RitAt()` |
|     - | 3484 | ` * hands back a pointer into pVm->aMemObj, which REALLOCATES as the VM reserves` |
|     - | 3485 | ` * objects, and every call into a user iterator reserves some (rule 47).` |
|     - | 3486 | ` */` |
|  1862 | 3487 | `static ph7_class_instance * RitSub(ph7_vm *pVm,ph7_class_instance *pThis,int iLevel)` |
|     1 | 3488 | `{` |
|  1863 | 3489 | `	ph7_value *pVal = RitAt(pVm,pThis,RIT_ST,iLevel);` |
|  1863 | 3490 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|   ! 0 | 3491 | `		return 0;` |
|     - | 3492 | `	}` |
|  1863 | 3493 | `	return (ph7_class_instance *)pVal->x.pOther;` |
|   932 | 3494 | `}` |
|   442 | 3495 | `static int RitState(ph7_vm *pVm,ph7_class_instance *pThis,int iLevel)` |
|     1 | 3496 | `{` |
|   443 | 3497 | `	ph7_value *pVal = RitAt(pVm,pThis,RIT_SS,iLevel);` |
|   443 | 3498 | `	return pVal ? (int)ph7_value_to_int64(pVal) : RS_START;` |
|     1 | 3499 | `}` |
|   754 | 3500 | `static void RitSetState(ph7_vm *pVm,ph7_class_instance *pThis,int iLevel,int iState)` |
|     1 | 3501 | `{` |
|     - | 3502 | `	ph7_value sVal;` |
|   755 | 3503 | `	PH7_MemObjInitFromInt(pVm,&sVal,(sxi64)iState);` |
|   755 | 3504 | `	RitPut(pVm,pThis,RIT_SS,iLevel,&sVal);` |
|   755 | 3505 | `	PH7_MemObjRelease(&sVal);` |
|   755 | 3506 | `}` |
|     - | 3507 | ``/* php's `iterators = erealloc(…, ++level+1)` plus the two field writes. */`` |
|    68 | 3508 | `static void RitPush(ph7_vm *pVm,ph7_class_instance *pThis,ph7_class_instance *pChild)` |
|     1 | 3509 | `{` |
|    69 | 3510 | `	int iLevel = RitInt(pThis,RIT_LVL) + 1;` |
|     - | 3511 | `	ph7_value sObj;` |
|    69 | 3512 | `	PH7_MemObjInit(pVm,&sObj);` |
|    69 | 3513 | `	sObj.x.pOther = pChild;` |
|    69 | 3514 | `	MemObjSetType(&sObj,MEMOBJ_OBJ);` |
|     - | 3515 | `	/* The map takes its OWN reference through the store; the carrier is blanked` |
|     - | 3516 | `	 * rather than released, because releasing a MEMOBJ_OBJ carrier would unref an` |
|     - | 3517 | `	 * instance this frame never referenced (rule 16). */` |
|    69 | 3518 | `	RitPut(pVm,pThis,RIT_ST,iLevel,&sObj);` |
|    69 | 3519 | `	sObj.x.pOther = 0;` |
|    69 | 3520 | `	MemObjSetType(&sObj,MEMOBJ_NULL);` |
|    69 | 3521 | `	PH7_MemObjRelease(&sObj);` |
|    69 | 3522 | `	RitSetState(pVm,pThis,iLevel,RS_START);` |
|    69 | 3523 | `	PH7_NativeSetAttrInt(pVm,pThis,RIT_LVL,iLevel);` |
|    69 | 3524 | `}` |
|    68 | 3525 | `static void RitPop(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 | 3526 | `{` |
|    69 | 3527 | `	int iLevel = RitInt(pThis,RIT_LVL);` |
|    69 | 3528 | `	if( iLevel <= 0 ){` |
|   ! 0 | 3529 | `		return;` |
|     - | 3530 | `	}` |
|    69 | 3531 | `	RitErase(pVm,pThis,RIT_ST,iLevel);` |
|    69 | 3532 | `	RitErase(pVm,pThis,RIT_SS,iLevel);` |
|    69 | 3533 | `	PH7_NativeSetAttrInt(pVm,pThis,RIT_LVL,iLevel-1);` |
|    35 | 3534 | `}` |
|     - | 3535 | `/* Drop every level: php's spl_RecursiveIteratorIterator_free_iterators. */` |
|    86 | 3536 | `static void RitClear(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 | 3537 | `{` |
|    87 | 3538 | `	int iLevel = RitInt(pThis,RIT_LVL);` |
|   173 | 3539 | `	while( iLevel >= 0 ){` |
|    87 | 3540 | `		RitErase(pVm,pThis,RIT_ST,iLevel);` |
|    87 | 3541 | `		RitErase(pVm,pThis,RIT_SS,iLevel);` |
|    87 | 3542 | `		iLevel--;` |
|     1 | 3543 | `	}` |
|    87 | 3544 | `	PH7_NativeSetAttrInt(pVm,pThis,RIT_LVL,0);` |
|    87 | 3545 | `}` |
|     - | 3546 | `/*` |
|     - | 3547 | ` * Call a method, optionally SWALLOWING what it throws -- php clears the exception` |
|     - | 3548 | ` * at four sites when RIT_CATCH_GET_CHILD is set, and PH7_VmCallMethodSwallow is` |
|     - | 3549 | ` * the only way to spell that here (a throw raised under a C call site is` |
|     - | 3550 | ` * dispatched INLINE, so an enclosing user catch would run before this returns).` |
|     - | 3551 | ` * *pbThrew reports a swallowed throw, which php reads back as "retval is UNDEF".` |
|     - | 3552 | ` */` |
|  2232 | 3553 | `static sxi32 RitCall(ph7_context *pCtx,ph7_class_instance *pObj,const char *zName,sxu32 nName,` |
|     - | 3554 | `	ph7_value *pOut,int bCatch,int *pbThrew)` |
|     1 | 3555 | `{` |
|  2233 | 3556 | `	ph7_class_method *pMethod = pObj ? PH7_ClassExtractMethod(pObj->pClass,zName,nName) : 0;` |
|  2233 | 3557 | `	if( pbThrew ){` |
|   821 | 3558 | `		*pbThrew = FALSE;` |
|   410 | 3559 | `	}` |
|  2233 | 3560 | `	if( pMethod == 0 ){` |
|   ! 0 | 3561 | `		return SXRET_OK;` |
|     - | 3562 | `	}` |
|  2233 | 3563 | `	if( bCatch ){` |
|    13 | 3564 | `		return PH7_VmCallMethodSwallow(pCtx->pVm,pObj,pMethod,pOut,0,0,pbThrew);` |
|     - | 3565 | `	}` |
|  2221 | 3566 | `	return PH7_VmCallClassMethod(pCtx->pVm,pObj,pMethod,pOut,0,0);` |
|  1117 | 3567 | `}` |
|     - | 3568 | `/*` |
|     - | 3569 | ` * A hook on $this. php caches which of the seven the SUBCLASS overrides and calls` |
|     - | 3570 | ` * the sub-iterator directly when none does; dispatching through $this every time` |
|     - | 3571 | ` * reaches the same body -- the base ones are the no-ops php would have skipped --` |
|     - | 3572 | ` * with the override found automatically.` |
|     - | 3573 | ` */` |
|   708 | 3574 | `static sxi32 RitHook(ph7_context *pCtx,const char *zName,sxu32 nName,ph7_value *pOut,` |
|     - | 3575 | `	int bCatch,int *pbThrew)` |
|     1 | 3576 | `{` |
|   709 | 3577 | `	return RitCall(pCtx,PH7_ContextThis(pCtx),zName,nName,pOut,bCatch,pbThrew);` |
|     1 | 3578 | `}` |
|   226 | 3579 | `static int RitCatches(ph7_class_instance *pThis)` |
|     1 | 3580 | `{` |
|   227 | 3581 | `	return (RitInt(pThis,RIT_FL) & RIT_CATCH_GET_CHILD) != 0;` |
|     1 | 3582 | `}` |
|     - | 3583 | `/*` |
|     - | 3584 | ` * php's spl_recursive_it_move_forward_ex, transcribed. The switch's fallthroughs` |
|     - | 3585 | ` * (RS_NEXT into RS_START into RS_TEST) are written as a sequential if-chain, and` |
|     - | 3586 | `` * php's `goto next_step` is this loop's `continue`.`` |
|     - | 3587 | ` */` |
|   226 | 3588 | `static sxi32 RitMoveForward(ph7_context *pCtx)` |
|     1 | 3589 | `{` |
|   227 | 3590 | `	ph7_vm *pVm = pCtx->pVm;` |
|   227 | 3591 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 3592 | `	int bCatch;` |
|   227 | 3593 | `	if( !RitReady(pThis) ){` |
|   ! 0 | 3594 | `		return RitNotReady(pCtx);` |
|     - | 3595 | `	}` |
|   227 | 3596 | `	bCatch = RitCatches(pThis);` |
|   255 | 3597 | `	for(;;){` |
|     - | 3598 | `		ph7_class_instance *pSub;` |
|   443 | 3599 | `		int iLevel = RitInt(pThis,RIT_LVL);` |
|   443 | 3600 | `		int iState = RitState(pVm,pThis,iLevel);` |
|   443 | 3601 | `		int bThrew = 0;` |
|   443 | 3602 | `		int bExhausted = 0;` |
|     - | 3603 | `		sxi32 rc;` |
|   443 | 3604 | `		pSub = RitSub(pVm,pThis,iLevel);` |
|   443 | 3605 | `		if( pSub == 0 ){` |
|   ! 0 | 3606 | `			return PH7_OK;` |
|     - | 3607 | `		}` |
|   443 | 3608 | `		if( iState == RS_NEXT ){` |
|   215 | 3609 | `			rc = RitCall(pCtx,pSub,"next",sizeof("next")-1,0,bCatch,&bThrew);` |
|   215 | 3610 | `			if( rc != SXRET_OK ){` |
|   ! 0 | 3611 | `				return rc;` |
|     - | 3612 | `			}` |
|   215 | 3613 | `			pSub = RitSub(pVm,pThis,iLevel);   /* the call may have moved aMemObj */` |
|   215 | 3614 | `			if( pSub == 0 ){` |
|   ! 0 | 3615 | `				return PH7_OK;` |
|     - | 3616 | `			}` |
|   215 | 3617 | `			iState = RS_START;                 /* php's fallthrough */` |
|   107 | 3618 | `		}` |
|   443 | 3619 | `		if( iState == RS_START ){` |
|     - | 3620 | `			ph7_value sValid;` |
|   339 | 3621 | `			PH7_MemObjInit(pVm,&sValid);` |
|   339 | 3622 | `			rc = RitCall(pCtx,pSub,"valid",sizeof("valid")-1,&sValid,FALSE,0);` |
|   339 | 3623 | `			if( rc != SXRET_OK ){` |
|   ! 0 | 3624 | `				PH7_MemObjRelease(&sValid);` |
|   ! 0 | 3625 | `				return rc;` |
|     - | 3626 | `			}` |
|   339 | 3627 | `			bExhausted = !ph7_value_to_bool(&sValid);` |
|   339 | 3628 | `			PH7_MemObjRelease(&sValid);` |
|   339 | 3629 | `			if( !bExhausted ){` |
|     - | 3630 | `				/* php re-reads the level here and returns outright when the valid()` |
|     - | 3631 | `				 * call RE-ENTERED this iterator (a sub-iterator that drove the` |
|     - | 3632 | `				 * decorator behind its back); the stack it was walking is gone. */` |
|   223 | 3633 | `				if( RitInt(pThis,RIT_LVL) != iLevel \|\| RitSub(pVm,pThis,iLevel) != pSub ){` |
|   ! 0 | 3634 | `					return PH7_OK;` |
|     - | 3635 | `				}` |
|   223 | 3636 | `				RitSetState(pVm,pThis,iLevel,RS_TEST);` |
|   223 | 3637 | `				iState = RS_TEST;` |
|   111 | 3638 | `			}` |
|   169 | 3639 | `		}` |
|   443 | 3640 | `		if( !bExhausted && iState == RS_TEST ){` |
|     - | 3641 | `			ph7_value sHas;` |
|   223 | 3642 | `			int bDescend = 0;` |
|   223 | 3643 | `			PH7_MemObjInit(pVm,&sHas);` |
|   223 | 3644 | `			rc = RitHook(pCtx,"callHasChildren",sizeof("callHasChildren")-1,&sHas,bCatch,&bThrew);` |
|   223 | 3645 | `			if( rc != SXRET_OK ){` |
|     - | 3646 | `				/* php leaves the level on RS_NEXT so a caught-and-resumed traversal` |
|     - | 3647 | `				 * moves on rather than re-asking the same element. */` |
|   ! 0 | 3648 | `				RitSetState(pVm,pThis,iLevel,RS_NEXT);` |
|   ! 0 | 3649 | `				PH7_MemObjRelease(&sHas);` |
|   ! 0 | 3650 | `				return rc;` |
|     - | 3651 | `			}` |
|     - | 3652 | `			/* A SWALLOWED throw leaves php's retval UNDEF, which skips the` |
|     - | 3653 | `			 * has-children test entirely and yields the element. */` |
|   223 | 3654 | `			if( !bThrew && ph7_value_to_bool(&sHas) ){` |
|    89 | 3655 | `				int iMax = RitInt(pThis,RIT_MX);` |
|    89 | 3656 | `				int iMode = RitInt(pThis,RIT_MD);` |
|    89 | 3657 | `				if( iMax == -1 \|\| iMax > iLevel ){` |
|     - | 3658 | `					/* php compares the mode EXACTLY: an unrecognized mode matches no` |
|     - | 3659 | `					 * arm, falls out of the switch and yields without descending. */` |
|    79 | 3660 | `					if( iMode == RIT_LEAVES_ONLY \|\| iMode == RIT_CHILD_FIRST ){` |
|    61 | 3661 | `						RitSetState(pVm,pThis,iLevel,RS_CHILD);` |
|    61 | 3662 | `						bDescend = 1;` |
|    49 | 3663 | `					}else if( iMode == RIT_SELF_FIRST ){` |
|    15 | 3664 | `						RitSetState(pVm,pThis,iLevel,RS_SELF);` |
|    15 | 3665 | `						bDescend = 1;` |
|     8 | 3666 | `					}` |
|    50 | 3667 | `				}else if( iMode == RIT_LEAVES_ONLY ){` |
|     - | 3668 | `					/* Too deep to recurse into and NOT a leaf, so php skips it —` |
|     - | 3669 | `					 * the mode's defining rule, and the one the chunk dropped. */` |
|     5 | 3670 | `					RitSetState(pVm,pThis,iLevel,RS_NEXT);` |
|     5 | 3671 | `					bDescend = 1;` |
|     2 | 3672 | `				}` |
|    44 | 3673 | `			}` |
|   223 | 3674 | `			PH7_MemObjRelease(&sHas);` |
|   223 | 3675 | `			if( bDescend ){` |
|    79 | 3676 | `				continue;                      /* php's goto next_step */` |
|     - | 3677 | `			}` |
|   145 | 3678 | `			rc = RitHook(pCtx,"nextElement",sizeof("nextElement")-1,0,bCatch,&bThrew);` |
|   145 | 3679 | `			RitSetState(pVm,pThis,iLevel,RS_NEXT);` |
|   145 | 3680 | `			if( rc != SXRET_OK ){` |
|   ! 0 | 3681 | `				return rc;` |
|     - | 3682 | `			}` |
|   145 | 3683 | `			return PH7_OK;                     /* yield this element */` |
|     - | 3684 | `		}` |
|   221 | 3685 | `		if( !bExhausted && iState == RS_SELF ){` |
|    31 | 3686 | `			int iMode = RitInt(pThis,RIT_MD);` |
|    31 | 3687 | `			if( iMode == RIT_SELF_FIRST \|\| iMode == RIT_CHILD_FIRST ){` |
|    31 | 3688 | `				rc = RitHook(pCtx,"nextElement",sizeof("nextElement")-1,0,bCatch,&bThrew);` |
|    31 | 3689 | `				if( rc != SXRET_OK ){` |
|   ! 0 | 3690 | `					return rc;` |
|     - | 3691 | `				}` |
|    15 | 3692 | `			}` |
|    31 | 3693 | `			RitSetState(pVm,pThis,iLevel,iMode == RIT_SELF_FIRST ? RS_CHILD : RS_NEXT);` |
|    31 | 3694 | `			return PH7_OK;                     /* yield this element */` |
|     - | 3695 | `		}` |
|   191 | 3696 | `		if( !bExhausted && iState == RS_CHILD ){` |
|     - | 3697 | `			ph7_class *pRecCls;` |
|     - | 3698 | `			ph7_class_instance *pChild;` |
|     - | 3699 | `			ph7_value sChild;` |
|    75 | 3700 | `			int iMode = RitInt(pThis,RIT_MD);` |
|    75 | 3701 | `			PH7_MemObjInit(pVm,&sChild);` |
|    75 | 3702 | `			rc = RitHook(pCtx,"callGetChildren",sizeof("callGetChildren")-1,&sChild,bCatch,&bThrew);` |
|    75 | 3703 | `			if( rc != SXRET_OK ){` |
|     3 | 3704 | `				PH7_MemObjRelease(&sChild);` |
|     4 | 3705 | `				return rc;` |
|     - | 3706 | `			}` |
|    73 | 3707 | `			if( bThrew ){` |
|     - | 3708 | `				/* Caught: php drops the element and moves to the next one. */` |
|     3 | 3709 | `				PH7_MemObjRelease(&sChild);` |
|     3 | 3710 | `				RitSetState(pVm,pThis,iLevel,RS_NEXT);` |
|    37 | 3711 | `				continue;` |
|     - | 3712 | `			}` |
|    71 | 3713 | `			pRecCls = PH7_VmExtractClass(pVm,"RecursiveIterator",` |
|     - | 3714 | `				sizeof("RecursiveIterator")-1,FALSE,0);` |
|   106 | 3715 | `			pChild = (sChild.iFlags & MEMOBJ_OBJ) != 0` |
|    69 | 3716 | `				? (ph7_class_instance *)sChild.x.pOther : 0;` |
|    71 | 3717 | `			if( pChild == 0 \|\| (pRecCls && !PH7_VmInstanceOf(pChild->pClass,pRecCls)) ){` |
|     3 | 3718 | `				PH7_MemObjRelease(&sChild);` |
|     3 | 3719 | `				return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     - | 3720 | `					"Objects returned by RecursiveIterator::getChildren() must implement RecursiveIterator");` |
|     - | 3721 | `			}` |
|    69 | 3722 | `			pChild->iRef++;                    /* survive the release of the call result */` |
|    69 | 3723 | `			PH7_MemObjRelease(&sChild);` |
|    69 | 3724 | `			RitSetState(pVm,pThis,iLevel,iMode == RIT_CHILD_FIRST ? RS_SELF : RS_NEXT);` |
|    69 | 3725 | `			RitPush(pVm,pThis,pChild);` |
|    69 | 3726 | `			PH7_ClassInstanceUnref(pChild);    /* the level's slot holds it now */` |
|    69 | 3727 | `			rc = RitCall(pCtx,pChild,"rewind",sizeof("rewind")-1,0,FALSE,0);` |
|    69 | 3728 | `			if( rc != SXRET_OK ){` |
|   ! 0 | 3729 | `				return rc;` |
|     - | 3730 | `			}` |
|    69 | 3731 | `			rc = RitHook(pCtx,"beginChildren",sizeof("beginChildren")-1,0,bCatch,&bThrew);` |
|    69 | 3732 | `			if( rc != SXRET_OK ){` |
|   ! 0 | 3733 | `				return rc;` |
|     - | 3734 | `			}` |
|    69 | 3735 | `			continue;                          /* php's goto next_step */` |
|     - | 3736 | `		}` |
|     - | 3737 | `		/* No more elements at this level. */` |
|   117 | 3738 | `		if( iLevel <= 0 ){` |
|    49 | 3739 | `			return PH7_OK;                     /* done completely */` |
|     - | 3740 | `		}` |
|     - | 3741 | `		/* php calls endChildren BEFORE the pop, so the hook sees the depth it is` |
|     - | 3742 | `		 * leaving rather than the one it lands on. */` |
|    69 | 3743 | `		rc = RitHook(pCtx,"endChildren",sizeof("endChildren")-1,0,bCatch,&bThrew);` |
|    69 | 3744 | `		if( rc != SXRET_OK ){` |
|   ! 0 | 3745 | `			return rc;` |
|     - | 3746 | `		}` |
|    69 | 3747 | `		if( RitInt(pThis,RIT_LVL) > 0 && RitSub(pVm,pThis,RitInt(pThis,RIT_LVL)) == pSub ){` |
|    69 | 3748 | `			RitPop(pVm,pThis);` |
|    34 | 3749 | `		}` |
|     1 | 3750 | `	}` |
|   114 | 3751 | `}` |
|     - | 3752 | `/*` |
|     - | 3753 | ` * php's spl_recursive_it_valid_ex: ASK the levels, walking down from the current` |
|     - | 3754 | ` * one, and fire endIteration the first time the answer is no.` |
|     - | 3755 | ` */` |
|   222 | 3756 | `static sxi32 RitValidEx(ph7_context *pCtx,int *pbValid)` |
|     1 | 3757 | `{` |
|   223 | 3758 | `	ph7_vm *pVm = pCtx->pVm;` |
|   223 | 3759 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   223 | 3760 | `	int iLevel = RitInt(pThis,RIT_LVL);` |
|     - | 3761 | `	sxi32 rc;` |
|   223 | 3762 | `	*pbValid = FALSE;` |
|   273 | 3763 | `	while( iLevel >= 0 ){` |
|   223 | 3764 | `		ph7_class_instance *pSub = RitSub(pVm,pThis,iLevel);` |
|     - | 3765 | `		ph7_value sValid;` |
|     - | 3766 | `		int bOk;` |
|   223 | 3767 | `		if( pSub == 0 ){` |
|   ! 0 | 3768 | `			iLevel--;` |
|   ! 0 | 3769 | `			continue;` |
|     - | 3770 | `		}` |
|   223 | 3771 | `		PH7_MemObjInit(pVm,&sValid);` |
|   223 | 3772 | `		rc = RitCall(pCtx,pSub,"valid",sizeof("valid")-1,&sValid,FALSE,0);` |
|   223 | 3773 | `		if( rc != SXRET_OK ){` |
|   ! 0 | 3774 | `			PH7_MemObjRelease(&sValid);` |
|   ! 0 | 3775 | `			return rc;` |
|     - | 3776 | `		}` |
|   223 | 3777 | `		bOk = ph7_value_to_bool(&sValid);` |
|   223 | 3778 | `		PH7_MemObjRelease(&sValid);` |
|   223 | 3779 | `		if( bOk ){` |
|   173 | 3780 | `			*pbValid = TRUE;` |
|   173 | 3781 | `			return PH7_OK;` |
|     - | 3782 | `		}` |
|    51 | 3783 | `		iLevel--;` |
|     1 | 3784 | `	}` |
|    51 | 3785 | `	if( RitInt(pThis,RIT_II) ){` |
|    49 | 3786 | `		rc = RitHook(pCtx,"endIteration",sizeof("endIteration")-1,0,FALSE,0);` |
|    49 | 3787 | `		PH7_NativeSetAttrInt(pVm,pThis,RIT_II,0);` |
|    49 | 3788 | `		if( rc != SXRET_OK ){` |
|   ! 0 | 3789 | `			return rc;` |
|     - | 3790 | `		}` |
|    24 | 3791 | `	}` |
|    51 | 3792 | `	PH7_NativeSetAttrInt(pVm,pThis,RIT_II,0);` |
|    51 | 3793 | `	return PH7_OK;` |
|   112 | 3794 | `}` |
|    90 | 3795 | `static int vm_builtin_RecursiveIteratorIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 3796 | `{` |
|    91 | 3797 | `	ph7_vm *pVm = pCtx->pVm;` |
|    91 | 3798 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 3799 | `	ph7_class_instance *pObj;` |
|    91 | 3800 | `	ph7_class_instance *pHold = 0;` |
|     - | 3801 | `	ph7_class *pAggCls,*pRecCls,*pTravCls;` |
|    91 | 3802 | `	sxi64 iMode = RIT_LEAVES_ONLY,iFlags = 0;` |
|     - | 3803 | `	sxi32 rc;` |
|    91 | 3804 | `	if( pThis == 0 ){` |
|   ! 0 | 3805 | `		return PH7_OK;` |
|     - | 3806 | `	}` |
|     - | 3807 | `	/*` |
|     - | 3808 | `	 * php's ZPP here is "o\|ll" -- a bare OBJECT -- while the stub declares` |
|     - | 3809 | ``	 * `Traversable $iterator`, so the declared type and the refusal text disagree`` |
|     - | 3810 | `	 * (rule 41's neighbour). The spec row carries the declared type for Reflection` |
|     - | 3811 | `	 * and this body words both refusals, which is why the method sits on` |
|     - | 3812 | `	 * azSelfChecked[].` |
|     - | 3813 | `	 */` |
|    91 | 3814 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 \|\| apArg[0]->x.pOther == 0 ){` |
|     - | 3815 | `		char zGiven[64];` |
|     5 | 3816 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 3817 | `			"RecursiveIteratorIterator::__construct(): Argument #1 ($iterator) "` |
|     - | 3818 | `			"must be of type object, %s given",` |
|     2 | 3819 | `			nArg < 1 ? "none" : VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));` |
|     - | 3820 | `	}` |
|    89 | 3821 | `	if( nArg > 1 ){` |
|    31 | 3822 | `		rc = PH7_IntArgResolve(pCtx,apArg[1],"RecursiveIteratorIterator::__construct",2,"$mode","int",&iMode);` |
|    31 | 3823 | `		if( rc != SXRET_OK ){` |
|   ! 0 | 3824 | `			return rc;` |
|     - | 3825 | `		}` |
|    15 | 3826 | `	}` |
|    89 | 3827 | `	if( nArg > 2 ){` |
|    21 | 3828 | `		rc = PH7_IntArgResolve(pCtx,apArg[2],"RecursiveIteratorIterator::__construct",3,"$flags","int",&iFlags);` |
|    21 | 3829 | `		if( rc != SXRET_OK ){` |
|   ! 0 | 3830 | `			return rc;` |
|     - | 3831 | `		}` |
|    10 | 3832 | `	}` |
|    89 | 3833 | `	pObj = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    89 | 3834 | `	pAggCls = PH7_VmExtractClass(pVm,"IteratorAggregate",sizeof("IteratorAggregate")-1,FALSE,0);` |
|    89 | 3835 | `	pRecCls = PH7_VmExtractClass(pVm,"RecursiveIterator",sizeof("RecursiveIterator")-1,FALSE,0);` |
|    89 | 3836 | `	pTravCls = PH7_VmExtractClass(pVm,"Traversable",sizeof("Traversable")-1,FALSE,0);` |
|     - | 3837 | `	/*` |
|     - | 3838 | `	 * php's spl_get_iterator_from_aggregate: ONE getIterator() and no more. An` |
|     - | 3839 | `	 * IteratorAggregate whose getIterator() answers another aggregate therefore` |
|     - | 3840 | `	 * fails the RecursiveIterator test below rather than being unwrapped further.` |
|     - | 3841 | `	 */` |
|    89 | 3842 | `	if( pAggCls && PH7_VmInstanceOf(pObj->pClass,pAggCls) ){` |
|     3 | 3843 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pObj->pClass,"getIterator",` |
|     - | 3844 | `			sizeof("getIterator")-1);` |
|     - | 3845 | `		ph7_value sInner;` |
|     3 | 3846 | `		PH7_MemObjInit(pVm,&sInner);` |
|     3 | 3847 | `		rc = pMethod ? PH7_VmCallClassMethod(pVm,pObj,pMethod,&sInner,0,0) : SXRET_OK;` |
|     3 | 3848 | `		if( rc != SXRET_OK ){` |
|   ! 0 | 3849 | `			PH7_MemObjRelease(&sInner);` |
|   ! 0 | 3850 | `			return rc;` |
|     - | 3851 | `		}` |
|     2 | 3852 | `		if( (sInner.iFlags & MEMOBJ_OBJ) == 0 \|\| sInner.x.pOther == 0` |
|     3 | 3853 | `		 \|\| (pTravCls && !PH7_VmInstanceOf(((ph7_class_instance *)sInner.x.pOther)->pClass,pTravCls)) ){` |
|   ! 0 | 3854 | `			SyString *pName = &pObj->pClass->sName;` |
|   ! 0 | 3855 | `			PH7_MemObjRelease(&sInner);` |
|   ! 0 | 3856 | `			return PH7_VmThrowException(pCtx,"LogicException",` |
|   ! 0 | 3857 | `				"%z::getIterator() must return an object that implements Traversable",pName);` |
|     - | 3858 | `		}` |
|     3 | 3859 | `		pObj = (ph7_class_instance *)sInner.x.pOther;` |
|     3 | 3860 | `		pObj->iRef++;` |
|     3 | 3861 | `		PH7_MemObjRelease(&sInner);` |
|     3 | 3862 | `		pHold = pObj;` |
|     1 | 3863 | `	}` |
|    89 | 3864 | `	if( pRecCls == 0 \|\| !PH7_VmInstanceOf(pObj->pClass,pRecCls) ){` |
|     3 | 3865 | `		if( pHold ){` |
|   ! 0 | 3866 | `			PH7_ClassInstanceUnref(pHold);` |
|   ! 0 | 3867 | `		}` |
|     - | 3868 | `		/* php refuses here rather than from the declared type, so a plain Iterator` |
|     - | 3869 | `		 * gets this sentence and not a TypeError. */` |
|     3 | 3870 | `		return PH7_VmThrowException(pCtx,"InvalidArgumentException",` |
|     - | 3871 | `			"An instance of RecursiveIterator or IteratorAggregate creating it is required");` |
|     - | 3872 | `	}` |
|    87 | 3873 | `	RitClear(pVm,pThis);` |
|    87 | 3874 | `	PH7_NativeSetAttrInt(pVm,pThis,RIT_LVL,0);` |
|    87 | 3875 | `	PH7_NativeSetAttrInt(pVm,pThis,RIT_MD,iMode);` |
|    87 | 3876 | `	PH7_NativeSetAttrInt(pVm,pThis,RIT_FL,iFlags);` |
|    87 | 3877 | `	PH7_NativeSetAttrInt(pVm,pThis,RIT_MX,-1);` |
|    87 | 3878 | `	PH7_NativeSetAttrInt(pVm,pThis,RIT_II,0);` |
|     - | 3879 | `	{` |
|     - | 3880 | `		ph7_value sObj;` |
|    87 | 3881 | `		PH7_MemObjInit(pVm,&sObj);` |
|    87 | 3882 | `		sObj.x.pOther = pObj;` |
|    87 | 3883 | `		MemObjSetType(&sObj,MEMOBJ_OBJ);` |
|    87 | 3884 | `		RitPut(pVm,pThis,RIT_ST,0,&sObj);` |
|    87 | 3885 | `		sObj.x.pOther = 0;` |
|    87 | 3886 | `		MemObjSetType(&sObj,MEMOBJ_NULL);` |
|    87 | 3887 | `		PH7_MemObjRelease(&sObj);` |
|     - | 3888 | `	}` |
|    87 | 3889 | `	RitSetState(pVm,pThis,0,RS_START);` |
|     - | 3890 | `	/* Level 0 exists from HERE, which is what makes getDepth() answer 0 and` |
|     - | 3891 | `	 * getSubIterator() answer the root before any rewind(). */` |
|    87 | 3892 | `	PH7_NativeSetAttrInt(pVm,pThis,RIT_RD,1);` |
|    87 | 3893 | `	if( pHold ){` |
|     3 | 3894 | `		PH7_ClassInstanceUnref(pHold);` |
|     1 | 3895 | `	}` |
|    43 | 3896 | `	SXUNUSED(nArg);` |
|    87 | 3897 | `	return PH7_OK;` |
|    46 | 3898 | `}` |
|    58 | 3899 | `static int vm_builtin_RecursiveIteratorIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 3900 | `{` |
|    59 | 3901 | `	ph7_vm *pVm = pCtx->pVm;` |
|    59 | 3902 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 3903 | `	ph7_class_instance *pRoot;` |
|     - | 3904 | `	sxi32 rc;` |
|    29 | 3905 | `	SXUNUSED(nArg);` |
|    29 | 3906 | `	SXUNUSED(apArg);` |
|    59 | 3907 | `	if( !RitReady(pThis) ){` |
|     3 | 3908 | `		return RitNotReady(pCtx);` |
|     - | 3909 | `	}` |
|     - | 3910 | `	/* php pops the level FIRST and calls endChildren after, so the hook reports the` |
|     - | 3911 | `	 * depth it has landed on -- the opposite order from the traversal's own pop. */` |
|    57 | 3912 | `	while( RitInt(pThis,RIT_LVL) > 0 ){` |
|   ! 0 | 3913 | `		RitPop(pVm,pThis);` |
|   ! 0 | 3914 | `		rc = RitHook(pCtx,"endChildren",sizeof("endChildren")-1,0,FALSE,0);` |
|   ! 0 | 3915 | `		if( rc != SXRET_OK ){` |
|   ! 0 | 3916 | `			return rc;` |
|     - | 3917 | `		}` |
|   ! 0 | 3918 | `	}` |
|    57 | 3919 | `	RitSetState(pVm,pThis,0,RS_START);` |
|    57 | 3920 | `	pRoot = RitSub(pVm,pThis,0);` |
|    57 | 3921 | `	rc = RitCall(pCtx,pRoot,"rewind",sizeof("rewind")-1,0,FALSE,0);` |
|    57 | 3922 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3923 | `		return rc;` |
|     - | 3924 | `	}` |
|     - | 3925 | `	/* php's in_iteration latch: a second rewind() does NOT re-announce the` |
|     - | 3926 | `	 * iteration, which is the only reason the flag exists. */` |
|    57 | 3927 | `	if( !RitInt(pThis,RIT_II) ){` |
|    55 | 3928 | `		rc = RitHook(pCtx,"beginIteration",sizeof("beginIteration")-1,0,FALSE,0);` |
|    55 | 3929 | `		if( rc != SXRET_OK ){` |
|   ! 0 | 3930 | `			PH7_NativeSetAttrInt(pVm,pThis,RIT_II,1);` |
|   ! 0 | 3931 | `			return rc;` |
|     - | 3932 | `		}` |
|    27 | 3933 | `	}` |
|    57 | 3934 | `	PH7_NativeSetAttrInt(pVm,pThis,RIT_II,1);` |
|    57 | 3935 | `	return RitMoveForward(pCtx);` |
|    30 | 3936 | `}` |
|   224 | 3937 | `static int vm_builtin_RecursiveIteratorIterator_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 3938 | `{` |
|   225 | 3939 | `	int bValid = FALSE;` |
|     - | 3940 | `	sxi32 rc;` |
|   112 | 3941 | `	SXUNUSED(nArg);` |
|   112 | 3942 | `	SXUNUSED(apArg);` |
|   225 | 3943 | `	if( !RitReady(PH7_ContextThis(pCtx)) ){` |
|     3 | 3944 | `		return RitNotReady(pCtx);` |
|     - | 3945 | `	}` |
|   223 | 3946 | `	rc = RitValidEx(pCtx,&bValid);` |
|   223 | 3947 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3948 | `		return rc;` |
|     - | 3949 | `	}` |
|   223 | 3950 | `	ph7_result_bool(pCtx,bValid);` |
|   223 | 3951 | `	return PH7_OK;` |
|   113 | 3952 | `}` |
|     - | 3953 | `/* current() and key() read the CURRENT LEVEL live -- php keeps no cache here, the` |
|     - | 3954 | ` * one place the recursive iterator differs from every dual iterator (rule 43). */` |
|   326 | 3955 | `static sxi32 RitCurrentLevelCall(ph7_context *pCtx,const char *zName,sxu32 nName)` |
|     1 | 3956 | `{` |
|   327 | 3957 | `	ph7_vm *pVm = pCtx->pVm;` |
|   327 | 3958 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 3959 | `	ph7_class_instance *pSub;` |
|     - | 3960 | `	ph7_value sRes;` |
|     - | 3961 | `	sxi32 rc;` |
|   327 | 3962 | `	if( !RitReady(pThis) ){` |
|   ! 0 | 3963 | `		return RitNotReady(pCtx);` |
|     - | 3964 | `	}` |
|   327 | 3965 | `	pSub = RitSub(pVm,pThis,RitInt(pThis,RIT_LVL));` |
|   327 | 3966 | `	if( pSub == 0 ){` |
|   ! 0 | 3967 | `		ph7_result_null(pCtx);` |
|   ! 0 | 3968 | `		return PH7_OK;` |
|     - | 3969 | `	}` |
|   327 | 3970 | `	PH7_MemObjInit(pVm,&sRes);` |
|   327 | 3971 | `	rc = RitCall(pCtx,pSub,zName,nName,&sRes,FALSE,0);` |
|   327 | 3972 | `	if( rc == SXRET_OK ){` |
|   327 | 3973 | `		ph7_result_value(pCtx,&sRes);` |
|   163 | 3974 | `	}` |
|   327 | 3975 | `	PH7_MemObjRelease(&sRes);` |
|   327 | 3976 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|   164 | 3977 | `}` |
|   156 | 3978 | `static int vm_builtin_RecursiveIteratorIterator_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 3979 | `{` |
|    78 | 3980 | `	SXUNUSED(nArg);` |
|    78 | 3981 | `	SXUNUSED(apArg);` |
|   157 | 3982 | `	return RitCurrentLevelCall(pCtx,"key",sizeof("key")-1);` |
|     1 | 3983 | `}` |
|   170 | 3984 | `static int vm_builtin_RecursiveIteratorIterator_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 3985 | `{` |
|    85 | 3986 | `	SXUNUSED(nArg);` |
|    85 | 3987 | `	SXUNUSED(apArg);` |
|   171 | 3988 | `	return RitCurrentLevelCall(pCtx,"current",sizeof("current")-1);` |
|     1 | 3989 | `}` |
|   170 | 3990 | `static int vm_builtin_RecursiveIteratorIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 3991 | `{` |
|    85 | 3992 | `	SXUNUSED(nArg);` |
|    85 | 3993 | `	SXUNUSED(apArg);` |
|   171 | 3994 | `	if( !RitReady(PH7_ContextThis(pCtx)) ){` |
|   ! 0 | 3995 | `		return RitNotReady(pCtx);` |
|     - | 3996 | `	}` |
|   171 | 3997 | `	return RitMoveForward(pCtx);` |
|    86 | 3998 | `}` |
|   108 | 3999 | `static int vm_builtin_RecursiveIteratorIterator_getDepth(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 4000 | `{` |
|   109 | 4001 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    54 | 4002 | `	SXUNUSED(nArg);` |
|    54 | 4003 | `	SXUNUSED(apArg);` |
|   109 | 4004 | `	if( !RitReady(pThis) ){` |
|     3 | 4005 | `		return RitNotReady(pCtx);` |
|     - | 4006 | `	}` |
|   107 | 4007 | `	ph7_result_int64(pCtx,(ph7_int64)RitInt(pThis,RIT_LVL));` |
|   107 | 4008 | `	return PH7_OK;` |
|    55 | 4009 | `}` |
|    14 | 4010 | `static int vm_builtin_RecursiveIteratorIterator_getSubIterator(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 4011 | `{` |
|    15 | 4012 | `	ph7_vm *pVm = pCtx->pVm;` |
|    15 | 4013 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 4014 | `	ph7_class_instance *pSub;` |
|     - | 4015 | `	int iLevel;` |
|    15 | 4016 | `	if( !RitReady(pThis) ){` |
|   ! 0 | 4017 | `		return RitNotReady(pCtx);` |
|     - | 4018 | `	}` |
|    15 | 4019 | `	iLevel = RitInt(pThis,RIT_LVL);` |
|    15 | 4020 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_NULL) == 0 ){` |
|     9 | 4021 | `		sxi64 iWant = 0;` |
|     9 | 4022 | `		sxi32 rc = PH7_IntArgResolve(pCtx,apArg[0],"RecursiveIteratorIterator::getSubIterator",` |
|     - | 4023 | `			1,"$level","?int",&iWant);` |
|     9 | 4024 | `		if( rc != SXRET_OK ){` |
|   ! 0 | 4025 | `			return rc;` |
|     - | 4026 | `		}` |
|     9 | 4027 | `		if( iWant < 0 \|\| iWant > (sxi64)iLevel ){` |
|     7 | 4028 | `			ph7_result_null(pCtx);` |
|     7 | 4029 | `			return PH7_OK;` |
|     - | 4030 | `		}` |
|     3 | 4031 | `		iLevel = (int)iWant;` |
|     1 | 4032 | `	}` |
|     9 | 4033 | `	pSub = RitSub(pVm,pThis,iLevel);` |
|     9 | 4034 | `	if( pSub ){` |
|     9 | 4035 | `		SplResultBorrowed(pCtx,pSub);` |
|     5 | 4036 | `	}else{` |
|   ! 0 | 4037 | `		ph7_result_null(pCtx);` |
|     - | 4038 | `	}` |
|     9 | 4039 | `	return PH7_OK;` |
|     8 | 4040 | `}` |
|     6 | 4041 | `static int vm_builtin_RecursiveIteratorIterator_getInnerIterator(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 4042 | `{` |
|     7 | 4043 | `	ph7_vm *pVm = pCtx->pVm;` |
|     7 | 4044 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 4045 | `	ph7_class_instance *pSub;` |
|     3 | 4046 | `	SXUNUSED(nArg);` |
|     3 | 4047 | `	SXUNUSED(apArg);` |
|     7 | 4048 | `	if( !RitReady(pThis) ){` |
|     3 | 4049 | `		return RitNotReady(pCtx);` |
|     - | 4050 | `	}` |
|     5 | 4051 | `	pSub = RitSub(pVm,pThis,RitInt(pThis,RIT_LVL));` |
|     5 | 4052 | `	if( pSub ){` |
|     5 | 4053 | `		SplResultBorrowed(pCtx,pSub);` |
|     3 | 4054 | `	}else{` |
|   ! 0 | 4055 | `		ph7_result_null(pCtx);` |
|     - | 4056 | `	}` |
|     5 | 4057 | `	return PH7_OK;` |
|     4 | 4058 | `}` |
|     - | 4059 | `/* The five hooks php declares with empty bodies. They exist to be OVERRIDDEN and` |
|     - | 4060 | ` * to be reachable through parent:: from an override. */` |
|   386 | 4061 | `static int vm_builtin_RecursiveIteratorIterator_nop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 4062 | `{` |
|   193 | 4063 | `	SXUNUSED(nArg);` |
|   193 | 4064 | `	SXUNUSED(apArg);` |
|   387 | 4065 | `	if( !RitReady(PH7_ContextThis(pCtx)) ){` |
|   ! 0 | 4066 | `		return RitNotReady(pCtx);` |
|     - | 4067 | `	}` |
|   387 | 4068 | `	return PH7_OK;` |
|   194 | 4069 | `}` |
|     - | 4070 | `/* php's callHasChildren/callGetChildren ask the CURRENT LEVEL's iterator, which` |
|     - | 4071 | ` * is what makes them the documented interception point for both. */` |
|   226 | 4072 | `static int vm_builtin_RecursiveIteratorIterator_callHasChildren(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 4073 | `{` |
|   227 | 4074 | `	ph7_vm *pVm = pCtx->pVm;` |
|   227 | 4075 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 4076 | `	ph7_class_instance *pSub;` |
|     - | 4077 | `	ph7_value sRes;` |
|     - | 4078 | `	sxi32 rc;` |
|   113 | 4079 | `	SXUNUSED(nArg);` |
|   113 | 4080 | `	SXUNUSED(apArg);` |
|   227 | 4081 | `	if( !RitReady(pThis) ){` |
|     3 | 4082 | `		return RitNotReady(pCtx);` |
|     - | 4083 | `	}` |
|   225 | 4084 | `	pSub = RitSub(pVm,pThis,RitInt(pThis,RIT_LVL));` |
|   225 | 4085 | `	if( pSub == 0 ){` |
|   ! 0 | 4086 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 4087 | `		return PH7_OK;` |
|     - | 4088 | `	}` |
|   225 | 4089 | `	PH7_MemObjInit(pVm,&sRes);` |
|   225 | 4090 | `	rc = RitCall(pCtx,pSub,"hasChildren",sizeof("hasChildren")-1,&sRes,FALSE,0);` |
|   225 | 4091 | `	if( rc == SXRET_OK ){` |
|   225 | 4092 | `		ph7_result_bool(pCtx,ph7_value_to_bool(&sRes));` |
|   112 | 4093 | `	}` |
|   225 | 4094 | `	PH7_MemObjRelease(&sRes);` |
|   225 | 4095 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|   114 | 4096 | `}` |
|    76 | 4097 | `static int vm_builtin_RecursiveIteratorIterator_callGetChildren(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 4098 | `{` |
|    77 | 4099 | `	ph7_vm *pVm = pCtx->pVm;` |
|    77 | 4100 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 4101 | `	ph7_class_instance *pSub;` |
|     - | 4102 | `	ph7_value sRes;` |
|     - | 4103 | `	sxi32 rc;` |
|    38 | 4104 | `	SXUNUSED(nArg);` |
|    38 | 4105 | `	SXUNUSED(apArg);` |
|    77 | 4106 | `	if( !RitReady(pThis) ){` |
|   ! 0 | 4107 | `		return RitNotReady(pCtx);` |
|     - | 4108 | `	}` |
|    77 | 4109 | `	pSub = RitSub(pVm,pThis,RitInt(pThis,RIT_LVL));` |
|    77 | 4110 | `	if( pSub == 0 ){` |
|   ! 0 | 4111 | `		ph7_result_null(pCtx);` |
|   ! 0 | 4112 | `		return PH7_OK;` |
|     - | 4113 | `	}` |
|    77 | 4114 | `	PH7_MemObjInit(pVm,&sRes);` |
|    77 | 4115 | `	rc = RitCall(pCtx,pSub,"getChildren",sizeof("getChildren")-1,&sRes,FALSE,0);` |
|    77 | 4116 | `	if( rc == SXRET_OK ){` |
|    71 | 4117 | `		ph7_result_value(pCtx,&sRes);` |
|    35 | 4118 | `	}` |
|    77 | 4119 | `	PH7_MemObjRelease(&sRes);` |
|    77 | 4120 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|    39 | 4121 | `}` |
|    16 | 4122 | `static int vm_builtin_RecursiveIteratorIterator_setMaxDepth(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 4123 | `{` |
|    17 | 4124 | `	ph7_vm *pVm = pCtx->pVm;` |
|    17 | 4125 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    17 | 4126 | `	sxi64 iMax = -1;` |
|    17 | 4127 | `	if( !RitReady(pThis) ){` |
|   ! 0 | 4128 | `		return RitNotReady(pCtx);` |
|     - | 4129 | `	}` |
|    17 | 4130 | `	if( nArg > 0 ){` |
|    15 | 4131 | `		sxi32 rc = PH7_IntArgResolve(pCtx,apArg[0],"RecursiveIteratorIterator::setMaxDepth",` |
|     - | 4132 | `			1,"$maxDepth","int",&iMax);` |
|    15 | 4133 | `		if( rc != SXRET_OK ){` |
|   ! 0 | 4134 | `			return rc;` |
|     - | 4135 | `		}` |
|     7 | 4136 | `	}` |
|    17 | 4137 | `	if( iMax < -1 ){` |
|     3 | 4138 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 4139 | `			"RecursiveIteratorIterator::setMaxDepth(): Argument #1 ($maxDepth) "` |
|     - | 4140 | `			"must be greater than or equal to -1");` |
|     - | 4141 | `	}` |
|    15 | 4142 | `	if( iMax > SXI32_HIGH ){` |
|   ! 0 | 4143 | `		iMax = SXI32_HIGH;   /* php clamps to INT_MAX; max_depth is an int there */` |
|   ! 0 | 4144 | `	}` |
|    15 | 4145 | `	PH7_NativeSetAttrInt(pVm,pThis,RIT_MX,iMax);` |
|    15 | 4146 | `	return PH7_OK;` |
|     9 | 4147 | `}` |
|     8 | 4148 | `static int vm_builtin_RecursiveIteratorIterator_getMaxDepth(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 4149 | `{` |
|     9 | 4150 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 4151 | `	int iMax;` |
|     4 | 4152 | `	SXUNUSED(nArg);` |
|     4 | 4153 | `	SXUNUSED(apArg);` |
|     9 | 4154 | `	if( !RitReady(pThis) ){` |
|   ! 0 | 4155 | `		return RitNotReady(pCtx);` |
|     - | 4156 | `	}` |
|     9 | 4157 | `	iMax = RitInt(pThis,RIT_MX);` |
|     9 | 4158 | `	if( iMax == -1 ){` |
|     5 | 4159 | ``		ph7_result_bool(pCtx,0);   /* php's `int\|false`: false means "any depth" */`` |
|     3 | 4160 | `	}else{` |
|     5 | 4161 | `		ph7_result_int64(pCtx,(ph7_int64)iMax);` |
|     - | 4162 | `	}` |
|     9 | 4163 | `	return PH7_OK;` |
|     5 | 4164 | `}` |
|     - | 4165 | `/*` |
|     - | 4166 | ` * The declaration. Method ORDER follows spl_iterators.stub.php line for line,` |
|     - | 4167 | ` * because that is the order Reflection reports. Every return type is php's` |
|     - | 4168 | `` * `@tentative-return-type` kind (rule 45).`` |
|     - | 4169 | ` */` |
|  5146 | 4170 | `static sxi32 VmInstallSplRecursiveIt(ph7_vm *pVm)` |
|     5 | 4171 | `{` |
|     - | 4172 | `	static const PH7_NativePropDef aRitProp[] = {` |
|     - | 4173 | `		{ RIT_ST,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - | 4174 | `		{ RIT_SS,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - | 4175 | `		{ RIT_LVL, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - | 4176 | `		{ RIT_MD,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - | 4177 | `		{ RIT_FL,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - | 4178 | `		{ RIT_MX,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, -1, 0, 0.0 }, 0 },` |
|     - | 4179 | `		{ RIT_II,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - | 4180 | `		{ RIT_RD,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - | 4181 | `	};` |
|     - | 4182 | `	static const PH7_NativeConstDef aRitConst[] = {` |
|     - | 4183 | `		{ "LEAVES_ONLY",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, RIT_LEAVES_ONLY, 0, 0.0 },` |
|     - | 4184 | `		{ "SELF_FIRST",      PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, RIT_SELF_FIRST, 0, 0.0 },` |
|     - | 4185 | `		{ "CHILD_FIRST",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, RIT_CHILD_FIRST, 0, 0.0 },` |
|     - | 4186 | `		{ "CATCH_GET_CHILD", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, RIT_CATCH_GET_CHILD, 0, 0.0 },` |
|     - | 4187 | `	};` |
|     - | 4188 | `	static const PH7_NativeMethodDef aRitMethod[] = {` |
|     - | 4189 | `		{ "__construct",      PH7_MOD_PUBLIC,` |
|     - | 4190 | ``		  /* php's stub spells the default `RecursiveIteratorIterator::LEAVES_ONLY`;`` |
|     - | 4191 | `		   * one zSig field cannot carry both the TEXT and the VALUE, and the value` |
|     - | 4192 | `		   * wins here for the same reason it does on RegexIterator's row. */` |
|     - | 4193 | `		  "Traversable $iterator, int $mode = 0, int $flags = 0", 0,` |
|     - | 4194 | `		  vm_builtin_RecursiveIteratorIterator_construct },` |
|     - | 4195 | `		{ "rewind",           PH7_MOD_PUBLIC, "", "@void",` |
|     - | 4196 | `		  vm_builtin_RecursiveIteratorIterator_rewind },` |
|     - | 4197 | `		{ "valid",            PH7_MOD_PUBLIC, "", "@bool",` |
|     - | 4198 | `		  vm_builtin_RecursiveIteratorIterator_valid },` |
|     - | 4199 | `		{ "key",              PH7_MOD_PUBLIC, "", "@mixed",` |
|     - | 4200 | `		  vm_builtin_RecursiveIteratorIterator_key },` |
|     - | 4201 | `		{ "current",          PH7_MOD_PUBLIC, "", "@mixed",` |
|     - | 4202 | `		  vm_builtin_RecursiveIteratorIterator_current },` |
|     - | 4203 | `		{ "next",             PH7_MOD_PUBLIC, "", "@void",` |
|     - | 4204 | `		  vm_builtin_RecursiveIteratorIterator_next },` |
|     - | 4205 | `		{ "getDepth",         PH7_MOD_PUBLIC, "", "@int",` |
|     - | 4206 | `		  vm_builtin_RecursiveIteratorIterator_getDepth },` |
|     - | 4207 | `		{ "getSubIterator",   PH7_MOD_PUBLIC, "?int $level = null", "@?RecursiveIterator",` |
|     - | 4208 | `		  vm_builtin_RecursiveIteratorIterator_getSubIterator },` |
|     - | 4209 | `		{ "getInnerIterator", PH7_MOD_PUBLIC, "", "@RecursiveIterator",` |
|     - | 4210 | `		  vm_builtin_RecursiveIteratorIterator_getInnerIterator },` |
|     - | 4211 | `		{ "beginIteration",   PH7_MOD_PUBLIC, "", "@void",` |
|     - | 4212 | `		  vm_builtin_RecursiveIteratorIterator_nop },` |
|     - | 4213 | `		{ "endIteration",     PH7_MOD_PUBLIC, "", "@void",` |
|     - | 4214 | `		  vm_builtin_RecursiveIteratorIterator_nop },` |
|     - | 4215 | `		{ "callHasChildren",  PH7_MOD_PUBLIC, "", "@bool",` |
|     - | 4216 | `		  vm_builtin_RecursiveIteratorIterator_callHasChildren },` |
|     - | 4217 | `		{ "callGetChildren",  PH7_MOD_PUBLIC, "", "@?RecursiveIterator",` |
|     - | 4218 | `		  vm_builtin_RecursiveIteratorIterator_callGetChildren },` |
|     - | 4219 | `		{ "beginChildren",    PH7_MOD_PUBLIC, "", "@void",` |
|     - | 4220 | `		  vm_builtin_RecursiveIteratorIterator_nop },` |
|     - | 4221 | `		{ "endChildren",      PH7_MOD_PUBLIC, "", "@void",` |
|     - | 4222 | `		  vm_builtin_RecursiveIteratorIterator_nop },` |
|     - | 4223 | `		{ "nextElement",      PH7_MOD_PUBLIC, "", "@void",` |
|     - | 4224 | `		  vm_builtin_RecursiveIteratorIterator_nop },` |
|     - | 4225 | `		{ "setMaxDepth",      PH7_MOD_PUBLIC, "int $maxDepth = -1", "@void",` |
|     - | 4226 | `		  vm_builtin_RecursiveIteratorIterator_setMaxDepth },` |
|     - | 4227 | `		{ "getMaxDepth",      PH7_MOD_PUBLIC, "", "@int\|false",` |
|     - | 4228 | `		  vm_builtin_RecursiveIteratorIterator_getMaxDepth },` |
|     - | 4229 | `	};` |
|     - | 4230 | ``	/* PH7_CLASS_NOCLONE: php refuses `clone` outright ("Trying to clone an`` |
|     - | 4231 | `	 * uncloneable object"), and a slot-by-slot copy would share one level stack --` |
|     - | 4232 | `	 * and with it one cursor -- between two traversals. */` |
|     - | 4233 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - | 4234 | `		{ "RecursiveIteratorIterator", 0, "OuterIterator", PH7_CLASS_NOCLONE,` |
|     - | 4235 | `		  aRitMethod, SX_ARRAYSIZE(aRitMethod),` |
|     - | 4236 | `		  aRitConst, SX_ARRAYSIZE(aRitConst),` |
|     - | 4237 | `		  aRitProp, SX_ARRAYSIZE(aRitProp), 0, 0, 0 },` |
|     - | 4238 | `	};` |
|  5151 | 4239 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|     5 | 4240 | `}` |
|     - | 4241 | `/*` |
|     - | 4242 | ` * ---------------------------------------------------------------------------` |
|     - | 4243 | ` * SplDoublyLinkedList, SplStack and SplQueue.` |
|     - | 4244 | ` *` |
|     - | 4245 | `` * php's `spl_dllist_object` is a linked list plus a FLAGS word, and the flags are`` |
|     - | 4246 | ` * where the family's shape lives: SPL_DLLIST_IT_LIFO (2) and IT_DELETE (1) are the` |
|     - | 4247 | ` * iteration mode, and IT_FIX (4) is a bit no CONSTANT names and no user can set --` |
|     - | 4248 | ` * the object handler stamps it at creation for SplStack and SplQueue, and it is` |
|     - | 4249 | ``  * both what freezes their LIFO/FIFO choice and why `(new SplStack)->getIteratorMode()` `` |
|     - | 4250 | ` * answers 6 rather than 2. The chunk had no notion of it, so both classes reported` |
|     - | 4251 | `` * the wrong mode and `setIteratorMode()` reported the wrong result.`` |
|     - | 4252 | ` *` |
|     - | 4253 | ` * Two rules follow from php's own code and neither is guessable from the methods:` |
|     - | 4254 | ` *` |
|     - | 4255 | ` *   - **every ArrayAccess offset is measured from the END of a LIFO list.**` |
|     - | 4256 | `` *     php resolves them through `spl_ptr_llist_offset(llist, offset, flags & LIFO)`,`` |
|     - | 4257 | `` *     so `$stack[0]` is the element `top()` answers, not the one `bottom()` does.`` |
|     - | 4258 | ` *     The chunk indexed the backing array directly and had the whole SplStack` |
|     - | 4259 | ` *     subscript surface reversed.` |
|     - | 4260 | ` *   - **the traverse POSITION is the list index in both modes.** A LIFO rewind` |
|     - | 4261 | `` *     seeds `count-1` and counts down, a FIFO rewind seeds 0 and counts up, so`` |
|     - | 4262 | `` *     `key()` and the element's own place in the list agree either way -- except`` |
|     - | 4263 | ` *     under IT_DELETE in FIFO order, where php consumes the head and deliberately` |
|     - | 4264 | ` *     does NOT advance the position (every element reports key 0).` |
|     - | 4265 | ` *` |
|     - | 4266 | `` * `toArray()` was a PHL INVENTION -- php has no such method on any of the three --`` |
|     - | 4267 | `` * and it is gone. What php has instead, and the chunk had none of: `__debugInfo()`,`` |
|     - | 4268 | `` * the `Serializable` interface with its `serialize()`/`unserialize()` pair, and the`` |
|     - | 4269 | `` * `__serialize()`/`__unserialize()` pair that php actually uses (which is why the`` |
|     - | 4270 | `` * serialized form is `O:19:"SplDoublyLinkedList":3:{i:0;…}` and not a property dump).`` |
|     - | 4271 | ` *` |
|     - | 4272 | ` * The store is a php array in a hidden slot, head->tail, so push/pop/shift/unshift` |
|     - | 4273 | ` * are the engine's OWN array builtins called with the slot (rule 7, and rule 39's` |
|     - | 4274 | ` * reference rule already lives inside them). php's element-POINTER cursor is not` |
|     - | 4275 | ` * modelled: a manual walk that mutates the list under itself resolves by position` |
|     - | 4276 | ` * here and by identity there. That is one probe line (§7.4) and the only one.` |
|     - | 4277 | ` */` |
|     - | 4278 | `#define DLL_Q  "__q"   /* php's llist, head -> tail */` |
|     - | 4279 | `#define DLL_FL "__fl"  /* php's flags word, IT_FIX included */` |
|     - | 4280 | `#define DLL_I  "__i"   /* php's traverse_position */` |
|     - | 4281 |  |
|     - | 4282 | `#define DLL_IT_DELETE 1` |
|     - | 4283 | `#define DLL_IT_LIFO   2` |
|     - | 4284 | `#define DLL_IT_FIX    4   /* php's SPL_DLLIST_IT_FIX: stamped at creation, never by a user */` |
|     - | 4285 | `#define DLL_IT_MASK   3` |
|     - | 4286 |  |
|   582 | 4287 | `static ph7_value * DllSlot(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 | 4288 | `{` |
|   583 | 4289 | `	ph7_value *pSlot = pThis ? PH7_NativeAttr(pThis,DLL_Q) : 0;` |
|   583 | 4290 | `	if( pSlot == 0 ){` |
|   ! 0 | 4291 | `		return 0;` |
|     - | 4292 | `	}` |
|   583 | 4293 | `	if( (pSlot->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    89 | 4294 | `		if( PH7_MemObjToHashmap(pSlot) != SXRET_OK ){` |
|   ! 0 | 4295 | `			return 0;` |
|     - | 4296 | `		}` |
|    44 | 4297 | `	}` |
|   583 | 4298 | `	if( PH7_HashmapCowSeparate(pVm,pSlot) == 0 ){` |
|   ! 0 | 4299 | `		return 0;` |
|     - | 4300 | `	}` |
|   583 | 4301 | `	return pSlot;` |
|   292 | 4302 | `}` |
|   328 | 4303 | `static ph7_hashmap * DllMap(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 | 4304 | `{` |
|   329 | 4305 | `	ph7_value *pSlot = DllSlot(pVm,pThis);` |
|   329 | 4306 | `	return pSlot ? (ph7_hashmap *)pSlot->x.pOther : 0;` |
|     1 | 4307 | `}` |
|   238 | 4308 | `static sxi64 DllCount(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 | 4309 | `{` |
|   239 | 4310 | `	ph7_hashmap *pMap = DllMap(pVm,pThis);` |
|   239 | 4311 | `	return pMap ? (sxi64)pMap->nEntry : 0;` |
|     1 | 4312 | `}` |
|   162 | 4313 | `static int DllFlags(ph7_class_instance *pThis)` |
|     1 | 4314 | `{` |
|   163 | 4315 | `	return pThis ? (int)PH7_NativeAttrInt(pThis,DLL_FL) : 0;` |
|     1 | 4316 | `}` |
|     - | 4317 | `/* The value at a LIST index (head = 0), or NULL. */` |
|    86 | 4318 | `static ph7_value * DllAt(ph7_vm *pVm,ph7_class_instance *pThis,sxi64 iIndex)` |
|     1 | 4319 | `{` |
|    87 | 4320 | `	ph7_hashmap *pMap = DllMap(pVm,pThis);` |
|    87 | 4321 | `	ph7_hashmap_node *pNode = 0;` |
|    87 | 4322 | `	if( pMap == 0 \|\| HashmapLookupIntKey(pMap,iIndex,&pNode) != SXRET_OK ){` |
|   ! 0 | 4323 | `		return 0;` |
|     - | 4324 | `	}` |
|    87 | 4325 | `	return HashmapExtractNodeValue(pNode);` |
|    44 | 4326 | `}` |
|     - | 4327 | `/*` |
|     - | 4328 | ` * php's spl_ptr_llist_offset: an ArrayAccess offset counts from the TAIL when the` |
|     - | 4329 | ` * list iterates LIFO. Every offsetGet/offsetSet/offsetUnset/add goes through here.` |
|     - | 4330 | ` */` |
|    24 | 4331 | `static sxi64 DllOffsetToIndex(ph7_class_instance *pThis,sxi64 iOffset,sxi64 nCount)` |
|     1 | 4332 | `{` |
|    25 | 4333 | `	if( DllFlags(pThis) & DLL_IT_LIFO ){` |
|    11 | 4334 | `		return nCount - 1 - iOffset;` |
|     - | 4335 | `	}` |
|    15 | 4336 | `	return iOffset;` |
|    13 | 4337 | `}` |
|     - | 4338 | `/* Hand one of the engine's own array builtins this instance's storage slot. */` |
|   234 | 4339 | `static int DllArrayCall(ph7_context *pCtx,ProchHostFunction xFunc,ph7_value **apExtra,int nExtra)` |
|     1 | 4340 | `{` |
|   235 | 4341 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 4342 | `	ph7_value *apCall[4];` |
|   235 | 4343 | `	ph7_value *pSlot = DllSlot(pCtx->pVm,pThis);` |
|     - | 4344 | `	int i;` |
|   235 | 4345 | `	if( pSlot == 0 ){` |
|   ! 0 | 4346 | `		return PH7_OK;` |
|     - | 4347 | `	}` |
|   235 | 4348 | `	apCall[0] = pSlot;` |
|   449 | 4349 | `	for( i = 0 ; i < nExtra && i < 3 ; ++i ){` |
|   215 | 4350 | `		apCall[i+1] = apExtra[i];` |
|   108 | 4351 | `	}` |
|   235 | 4352 | `	return xFunc(pCtx,nExtra+1,apCall);` |
|   118 | 4353 | `}` |
|     - | 4354 | `/* php's four "empty datastructure" refusals, which differ only in the verb. */` |
|    18 | 4355 | `static sxi32 DllEmpty(ph7_context *pCtx,const char *zVerb)` |
|     1 | 4356 | `{` |
|    28 | 4357 | `	return PH7_VmThrowException(pCtx,"RuntimeException",` |
|     9 | 4358 | `		"Can't %s an empty datastructure",zVerb);` |
|     1 | 4359 | `}` |
|     - | 4360 | `/*` |
|     - | 4361 | ` * php words every out-of-range offset from the DECLARING class, not the runtime` |
|     - | 4362 | `` * one: `SplStack::add()` on an out-of-range index still says`` |
|     - | 4363 | `` * `SplDoublyLinkedList::add()`. The chunk used get_class($this) and reported the`` |
|     - | 4364 | ` * subclass.` |
|     - | 4365 | ` */` |
|    14 | 4366 | `static sxi32 DllOutOfRange(ph7_context *pCtx,const char *zMethod)` |
|     1 | 4367 | `{` |
|    22 | 4368 | `	return PH7_VmThrowException(pCtx,"OutOfRangeException",` |
|     7 | 4369 | `		"SplDoublyLinkedList::%s(): Argument #1 ($index) is out of range",zMethod);` |
|     1 | 4370 | `}` |
|     - | 4371 | `/*` |
|     - | 4372 | ``  * php's ZPP for the four ArrayAccess offsets and add(): the stub leaves `$index` `` |
|     - | 4373 | ` * UNTYPED (which is what Reflection prints) while the ZPP is Z_PARAM_LONG, whose` |
|     - | 4374 | `` * TypeError says `must be of type int`. An untyped signature is not screened`` |
|     - | 4375 | ` * centrally, so the rule is applied here — rule 41's disagreement, resolved without` |
|     - | 4376 | ` * an azSelfChecked[] row because the declared type is absent rather than different.` |
|     - | 4377 | ` */` |
|    46 | 4378 | `static sxi32 DllIndexArg(ph7_context *pCtx,const char *zMethod,ph7_value *pArg,sxi64 *piOut)` |
|     1 | 4379 | `{` |
|     - | 4380 | `	char zFunc[64];` |
|    47 | 4381 | `	SyBufferFormat(zFunc,sizeof(zFunc),"SplDoublyLinkedList::%s",zMethod);` |
|    47 | 4382 | `	return PH7_IntArgResolve(pCtx,pArg,zFunc,1,"$index","int",piOut);` |
|     1 | 4383 | `}` |
|   198 | 4384 | `static int vm_builtin_SplDll_push(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 4385 | `{` |
|     - | 4386 | `	ph7_value *apExtra[1];` |
|   199 | 4387 | `	if( nArg < 1 ){` |
|   ! 0 | 4388 | `		return PH7_OK;` |
|     - | 4389 | `	}` |
|   199 | 4390 | `	apExtra[0] = apArg[0];` |
|   199 | 4391 | `	DllArrayCall(pCtx,ph7_hashmap_push,apExtra,1);` |
|   199 | 4392 | `	ph7_result_null(pCtx);   /* array_push answers the new count; php's push is void */` |
|   199 | 4393 | `	return PH7_OK;` |
|   100 | 4394 | `}` |
|     2 | 4395 | `static int vm_builtin_SplDll_unshift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 4396 | `{` |
|     - | 4397 | `	ph7_value *apExtra[1];` |
|     3 | 4398 | `	if( nArg < 1 ){` |
|   ! 0 | 4399 | `		return PH7_OK;` |
|     - | 4400 | `	}` |
|     3 | 4401 | `	apExtra[0] = apArg[0];` |
|     3 | 4402 | `	DllArrayCall(pCtx,ph7_hashmap_unshift,apExtra,1);` |
|     3 | 4403 | `	ph7_result_null(pCtx);` |
|     3 | 4404 | `	return PH7_OK;` |
|     2 | 4405 | `}` |
|     6 | 4406 | `static int vm_builtin_SplDll_pop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 4407 | `{` |
|     3 | 4408 | `	SXUNUSED(nArg);` |
|     3 | 4409 | `	SXUNUSED(apArg);` |
|     7 | 4410 | `	if( DllCount(pCtx->pVm,PH7_ContextThis(pCtx)) == 0 ){` |
|     5 | 4411 | `		return DllEmpty(pCtx,"pop from");` |
|     - | 4412 | `	}` |
|     3 | 4413 | `	return DllArrayCall(pCtx,ph7_hashmap_pop,0,0);` |
|     4 | 4414 | `}` |
|    10 | 4415 | `static int vm_builtin_SplDll_shift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 4416 | `{` |
|     5 | 4417 | `	SXUNUSED(nArg);` |
|     5 | 4418 | `	SXUNUSED(apArg);` |
|    11 | 4419 | `	if( DllCount(pCtx->pVm,PH7_ContextThis(pCtx)) == 0 ){` |
|     7 | 4420 | `		return DllEmpty(pCtx,"shift from");` |
|     - | 4421 | `	}` |
|     5 | 4422 | `	return DllArrayCall(pCtx,ph7_hashmap_shift,0,0);` |
|     6 | 4423 | `}` |
|     - | 4424 | `/* top() is the TAIL and bottom() the HEAD, whatever the iteration mode: php reads` |
|     - | 4425 | ` * llist->tail/llist->head directly and never consults the flags here. */` |
|    10 | 4426 | `static int vm_builtin_SplDll_top(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 4427 | `{` |
|    11 | 4428 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    11 | 4429 | `	sxi64 nCount = DllCount(pCtx->pVm,pThis);` |
|     - | 4430 | `	ph7_value *pVal;` |
|     5 | 4431 | `	SXUNUSED(nArg);` |
|     5 | 4432 | `	SXUNUSED(apArg);` |
|    11 | 4433 | `	if( nCount == 0 ){` |
|     5 | 4434 | `		return DllEmpty(pCtx,"peek at");` |
|     - | 4435 | `	}` |
|     7 | 4436 | `	pVal = DllAt(pCtx->pVm,pThis,nCount-1);` |
|     7 | 4437 | `	if( pVal ){` |
|     7 | 4438 | `		ph7_result_value(pCtx,pVal);` |
|     3 | 4439 | `	}` |
|     7 | 4440 | `	return PH7_OK;` |
|     6 | 4441 | `}` |
|    10 | 4442 | `static int vm_builtin_SplDll_bottom(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 4443 | `{` |
|    11 | 4444 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 4445 | `	ph7_value *pVal;` |
|     5 | 4446 | `	SXUNUSED(nArg);` |
|     5 | 4447 | `	SXUNUSED(apArg);` |
|    11 | 4448 | `	if( DllCount(pCtx->pVm,pThis) == 0 ){` |
|     5 | 4449 | `		return DllEmpty(pCtx,"peek at");` |
|     - | 4450 | `	}` |
|     7 | 4451 | `	pVal = DllAt(pCtx->pVm,pThis,0);` |
|     7 | 4452 | `	if( pVal ){` |
|     7 | 4453 | `		ph7_result_value(pCtx,pVal);` |
|     3 | 4454 | `	}` |
|     7 | 4455 | `	return PH7_OK;` |
|     6 | 4456 | `}` |
|    16 | 4457 | `static int vm_builtin_SplDll_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 4458 | `{` |
|     8 | 4459 | `	SXUNUSED(nArg);` |
|     8 | 4460 | `	SXUNUSED(apArg);` |
|    17 | 4461 | `	ph7_result_int64(pCtx,DllCount(pCtx->pVm,PH7_ContextThis(pCtx)));` |
|    17 | 4462 | `	return PH7_OK;` |
|     1 | 4463 | `}` |
|     2 | 4464 | `static int vm_builtin_SplDll_isEmpty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 4465 | `{` |
|     1 | 4466 | `	SXUNUSED(nArg);` |
|     1 | 4467 | `	SXUNUSED(apArg);` |
|     3 | 4468 | `	ph7_result_bool(pCtx,DllCount(pCtx->pVm,PH7_ContextThis(pCtx)) == 0);` |
|     3 | 4469 | `	return PH7_OK;` |
|     1 | 4470 | `}` |
|    22 | 4471 | `static int vm_builtin_SplDll_setIteratorMode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 4472 | `{` |
|    23 | 4473 | `	ph7_vm *pVm = pCtx->pVm;` |
|    23 | 4474 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    23 | 4475 | `	int iFlags = DllFlags(pThis);` |
|    23 | 4476 | `	sxi64 iMode = 0;` |
|     - | 4477 | `	sxi32 rc;` |
|    23 | 4478 | `	if( pThis == 0 ){` |
|   ! 0 | 4479 | `		return PH7_OK;` |
|     - | 4480 | `	}` |
|    23 | 4481 | `	if( nArg < 1 ){` |
|   ! 0 | 4482 | `		return PH7_OK;   /* the arity screen already refused */` |
|     - | 4483 | `	}` |
|    23 | 4484 | `	rc = PH7_IntArgResolve(pCtx,apArg[0],"SplDoublyLinkedList::setIteratorMode",1,"$mode","int",&iMode);` |
|    23 | 4485 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 4486 | `		return rc;` |
|     - | 4487 | `	}` |
|    23 | 4488 | `	if( (iFlags & DLL_IT_FIX) && (iFlags & DLL_IT_LIFO) != ((int)iMode & DLL_IT_LIFO) ){` |
|     7 | 4489 | `		return PH7_VmThrowException(pCtx,"RuntimeException",` |
|     - | 4490 | `			"Iterators' LIFO/FIFO modes for SplStack/SplQueue objects are frozen");` |
|     - | 4491 | `	}` |
|     - | 4492 | `	/* php MASKS the value to the two mode bits and re-adds IT_FIX, so a nonsense` |
|     - | 4493 | `	 * mode is silently reduced rather than refused — and the ANSWER is the stored` |
|     - | 4494 | `	 * word, which is how a caller sees the fix bit at all. */` |
|    17 | 4495 | `	iFlags = ((int)iMode & DLL_IT_MASK) \| (iFlags & DLL_IT_FIX);` |
|    17 | 4496 | `	PH7_NativeSetAttrInt(pVm,pThis,DLL_FL,iFlags);` |
|    17 | 4497 | `	ph7_result_int64(pCtx,(ph7_int64)iFlags);` |
|    17 | 4498 | `	return PH7_OK;` |
|    12 | 4499 | `}` |
|    12 | 4500 | `static int vm_builtin_SplDll_getIteratorMode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 4501 | `{` |
|     6 | 4502 | `	SXUNUSED(nArg);` |
|     6 | 4503 | `	SXUNUSED(apArg);` |
|    13 | 4504 | `	ph7_result_int64(pCtx,(ph7_int64)DllFlags(PH7_ContextThis(pCtx)));` |
|    13 | 4505 | `	return PH7_OK;` |
|     1 | 4506 | `}` |
|     6 | 4507 | `static int vm_builtin_SplDll_add(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 4508 | `{` |
|     7 | 4509 | `	ph7_vm *pVm = pCtx->pVm;` |
|     7 | 4510 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     7 | 4511 | `	sxi64 nCount = DllCount(pVm,pThis);` |
|     7 | 4512 | `	sxi64 iIndex = 0;` |
|     - | 4513 | `	ph7_value sOff,sLen,sRep,*apExtra[3];` |
|     - | 4514 | `	ph7_hashmap *pRep;` |
|     - | 4515 | `	sxi32 rc;` |
|     7 | 4516 | `	if( nArg < 2 ){` |
|   ! 0 | 4517 | `		return PH7_OK;` |
|     - | 4518 | `	}` |
|     7 | 4519 | `	rc = DllIndexArg(pCtx,"add",apArg[0],&iIndex);` |
|     7 | 4520 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 4521 | `		return rc;` |
|     - | 4522 | `	}` |
|     7 | 4523 | `	if( iIndex < 0 \|\| iIndex > nCount ){` |
|     5 | 4524 | `		return DllOutOfRange(pCtx,"add");` |
|     - | 4525 | `	}` |
|     3 | 4526 | `	if( iIndex == nCount ){` |
|     - | 4527 | `		/* php: "the last entry + 1" is a push, because there is nothing to insert` |
|     - | 4528 | `		 * before. Note this is the LIST tail in both modes. */` |
|     - | 4529 | `		ph7_value *apOne[1];` |
|   ! 0 | 4530 | `		apOne[0] = apArg[1];` |
|   ! 0 | 4531 | `		DllArrayCall(pCtx,ph7_hashmap_push,apOne,1);` |
|   ! 0 | 4532 | `		ph7_result_null(pCtx);` |
|   ! 0 | 4533 | `		return PH7_OK;` |
|     - | 4534 | `	}` |
|     3 | 4535 | `	pRep = PH7_NewHashmap(pVm,0,0);` |
|     3 | 4536 | `	if( pRep == 0 ){` |
|   ! 0 | 4537 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 4538 | `	}` |
|     3 | 4539 | `	PH7_MemObjInit(pVm,&sRep);` |
|     3 | 4540 | `	sRep.x.pOther = pRep;` |
|     3 | 4541 | `	MemObjSetType(&sRep,MEMOBJ_HASHMAP);` |
|     3 | 4542 | `	PH7_HashmapInsert(pRep,0,apArg[1]);` |
|     3 | 4543 | `	PH7_MemObjInitFromInt(pVm,&sOff,DllOffsetToIndex(pThis,iIndex,nCount));` |
|     3 | 4544 | `	PH7_MemObjInitFromInt(pVm,&sLen,0);` |
|     3 | 4545 | `	apExtra[0] = &sOff;` |
|     3 | 4546 | `	apExtra[1] = &sLen;` |
|     3 | 4547 | `	apExtra[2] = &sRep;` |
|     3 | 4548 | `	DllArrayCall(pCtx,ph7_hashmap_splice,apExtra,3);` |
|     3 | 4549 | `	PH7_MemObjRelease(&sOff);` |
|     3 | 4550 | `	PH7_MemObjRelease(&sLen);` |
|     3 | 4551 | `	PH7_MemObjRelease(&sRep);` |
|     3 | 4552 | `	ph7_result_null(pCtx);   /* array_splice answers what it removed; add() is void */` |
|     3 | 4553 | `	return PH7_OK;` |
|     4 | 4554 | `}` |
|     6 | 4555 | `static int vm_builtin_SplDll_offsetExists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 4556 | `{` |
|     7 | 4557 | `	sxi64 iIndex = 0;` |
|     - | 4558 | `	sxi32 rc;` |
|     7 | 4559 | `	if( nArg < 1 ){` |
|   ! 0 | 4560 | `		return PH7_OK;` |
|     - | 4561 | `	}` |
|     7 | 4562 | `	rc = DllIndexArg(pCtx,"offsetExists",apArg[0],&iIndex);` |
|     7 | 4563 | `	if( rc != SXRET_OK ){` |
|     3 | 4564 | `		return rc;` |
|     - | 4565 | `	}` |
|     5 | 4566 | `	ph7_result_bool(pCtx,iIndex >= 0 && iIndex < DllCount(pCtx->pVm,PH7_ContextThis(pCtx)));` |
|     5 | 4567 | `	return PH7_OK;` |
|     4 | 4568 | `}` |
|    22 | 4569 | `static int vm_builtin_SplDll_offsetGet(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 4570 | `{` |
|    23 | 4571 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    23 | 4572 | `	sxi64 nCount = DllCount(pCtx->pVm,pThis);` |
|    23 | 4573 | `	sxi64 iIndex = 0;` |
|     - | 4574 | `	ph7_value *pVal;` |
|     - | 4575 | `	sxi32 rc;` |
|    23 | 4576 | `	if( nArg < 1 ){` |
|   ! 0 | 4577 | `		return PH7_OK;` |
|     - | 4578 | `	}` |
|    23 | 4579 | `	rc = DllIndexArg(pCtx,"offsetGet",apArg[0],&iIndex);` |
|    23 | 4580 | `	if( rc != SXRET_OK ){` |
|     3 | 4581 | `		return rc;` |
|     - | 4582 | `	}` |
|    21 | 4583 | `	if( iIndex < 0 \|\| iIndex >= nCount ){` |
|     5 | 4584 | `		return DllOutOfRange(pCtx,"offsetGet");` |
|     - | 4585 | `	}` |
|    17 | 4586 | `	pVal = DllAt(pCtx->pVm,pThis,DllOffsetToIndex(pThis,iIndex,nCount));` |
|    17 | 4587 | `	if( pVal ){` |
|    17 | 4588 | `		ph7_result_value(pCtx,pVal);` |
|     8 | 4589 | `	}` |
|    17 | 4590 | `	return PH7_OK;` |
|    12 | 4591 | `}` |
|     6 | 4592 | `static int vm_builtin_SplDll_offsetSet(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 4593 | `{` |
|     7 | 4594 | `	ph7_vm *pVm = pCtx->pVm;` |
|     7 | 4595 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     7 | 4596 | `	sxi64 nCount = DllCount(pVm,pThis);` |
|     7 | 4597 | `	sxi64 iIndex = 0;` |
|     - | 4598 | `	ph7_hashmap *pMap;` |
|     7 | 4599 | `	ph7_hashmap_node *pNode = 0;` |
|     - | 4600 | `	ph7_value sKey;` |
|     - | 4601 | `	sxi32 rc;` |
|     7 | 4602 | `	if( nArg < 2 ){` |
|   ! 0 | 4603 | `		return PH7_OK;` |
|     - | 4604 | `	}` |
|     7 | 4605 | `	if( apArg[0]->iFlags & MEMOBJ_NULL ){` |
|     - | 4606 | ``		/* php: a null offset is `$dll[] = v`, which pushes. */`` |
|     - | 4607 | `		ph7_value *apOne[1];` |
|   ! 0 | 4608 | `		apOne[0] = apArg[1];` |
|   ! 0 | 4609 | `		DllArrayCall(pCtx,ph7_hashmap_push,apOne,1);` |
|   ! 0 | 4610 | `		ph7_result_null(pCtx);` |
|   ! 0 | 4611 | `		return PH7_OK;` |
|     - | 4612 | `	}` |
|     7 | 4613 | `	rc = DllIndexArg(pCtx,"offsetSet",apArg[0],&iIndex);` |
|     7 | 4614 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 4615 | `		return rc;` |
|     - | 4616 | `	}` |
|     7 | 4617 | `	if( iIndex < 0 \|\| iIndex >= nCount ){` |
|     5 | 4618 | `		return DllOutOfRange(pCtx,"offsetSet");` |
|     - | 4619 | `	}` |
|     3 | 4620 | `	pMap = DllMap(pVm,pThis);` |
|     3 | 4621 | `	PH7_MemObjInitFromInt(pVm,&sKey,DllOffsetToIndex(pThis,iIndex,nCount));` |
|     3 | 4622 | `	if( pMap && PH7_HashmapLookup(pMap,&sKey,&pNode) == SXRET_OK ){` |
|     3 | 4623 | `		ph7_value *pDest = HashmapExtractNodeValue(pNode);` |
|     3 | 4624 | `		if( pDest ){` |
|     3 | 4625 | `			PH7_MemObjStore(apArg[1],pDest);` |
|     1 | 4626 | `		}` |
|     1 | 4627 | `	}` |
|     3 | 4628 | `	PH7_MemObjRelease(&sKey);` |
|     3 | 4629 | `	return PH7_OK;` |
|     4 | 4630 | `}` |
|     6 | 4631 | `static int vm_builtin_SplDll_offsetUnset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 4632 | `{` |
|     7 | 4633 | `	ph7_vm *pVm = pCtx->pVm;` |
|     7 | 4634 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     7 | 4635 | `	sxi64 nCount = DllCount(pVm,pThis);` |
|     7 | 4636 | `	sxi64 iIndex = 0;` |
|     - | 4637 | `	ph7_value sOff,sLen,*apExtra[2];` |
|     - | 4638 | `	sxi32 rc;` |
|     7 | 4639 | `	if( nArg < 1 ){` |
|   ! 0 | 4640 | `		return PH7_OK;` |
|     - | 4641 | `	}` |
|     7 | 4642 | `	rc = DllIndexArg(pCtx,"offsetUnset",apArg[0],&iIndex);` |
|     7 | 4643 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 4644 | `		return rc;` |
|     - | 4645 | `	}` |
|     7 | 4646 | `	if( iIndex < 0 \|\| iIndex >= nCount ){` |
|     3 | 4647 | `		return DllOutOfRange(pCtx,"offsetUnset");` |
|     - | 4648 | `	}` |
|     5 | 4649 | `	PH7_MemObjInitFromInt(pVm,&sOff,DllOffsetToIndex(pThis,iIndex,nCount));` |
|     5 | 4650 | `	PH7_MemObjInitFromInt(pVm,&sLen,1);` |
|     5 | 4651 | `	apExtra[0] = &sOff;` |
|     5 | 4652 | `	apExtra[1] = &sLen;` |
|     5 | 4653 | `	DllArrayCall(pCtx,ph7_hashmap_splice,apExtra,2);` |
|     5 | 4654 | `	PH7_MemObjRelease(&sOff);` |
|     5 | 4655 | `	PH7_MemObjRelease(&sLen);` |
|     5 | 4656 | `	ph7_result_null(pCtx);` |
|     5 | 4657 | `	return PH7_OK;` |
|     4 | 4658 | `}` |
|     - | 4659 | `/*` |
|     - | 4660 | ` * The cursor. php's traverse_position IS the list index in both directions — a` |
|     - | 4661 | ` * LIFO rewind seeds count-1 and counts down — so current() and key() need no mode` |
|     - | 4662 | ` * test at all. IT_DELETE is the exception: in FIFO order php consumes the head and` |
|     - | 4663 | ` * leaves the position alone, so every element of a consuming walk reports key 0.` |
|     - | 4664 | ` */` |
|    24 | 4665 | `static int vm_builtin_SplDll_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 4666 | `{` |
|    25 | 4667 | `	ph7_vm *pVm = pCtx->pVm;` |
|    25 | 4668 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    12 | 4669 | `	SXUNUSED(nArg);` |
|    12 | 4670 | `	SXUNUSED(apArg);` |
|    25 | 4671 | `	if( pThis == 0 ){` |
|   ! 0 | 4672 | `		return PH7_OK;` |
|     - | 4673 | `	}` |
|    37 | 4674 | `	PH7_NativeSetAttrInt(pVm,pThis,DLL_I,` |
|    24 | 4675 | `		(DllFlags(pThis) & DLL_IT_LIFO) ? DllCount(pVm,pThis)-1 : 0);` |
|    25 | 4676 | `	return PH7_OK;` |
|    13 | 4677 | `}` |
|    82 | 4678 | `static int vm_builtin_SplDll_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 4679 | `{` |
|    83 | 4680 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    83 | 4681 | `	sxi64 iPos = pThis ? PH7_NativeAttrInt(pThis,DLL_I) : 0;` |
|    41 | 4682 | `	SXUNUSED(nArg);` |
|    41 | 4683 | `	SXUNUSED(apArg);` |
|    83 | 4684 | `	ph7_result_bool(pCtx,iPos >= 0 && iPos < DllCount(pCtx->pVm,pThis));` |
|    83 | 4685 | `	return PH7_OK;` |
|     1 | 4686 | `}` |
|    58 | 4687 | `static int vm_builtin_SplDll_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 4688 | `{` |
|    59 | 4689 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    59 | 4690 | `	ph7_value *pVal = pThis ? DllAt(pCtx->pVm,pThis,PH7_NativeAttrInt(pThis,DLL_I)) : 0;` |
|    29 | 4691 | `	SXUNUSED(nArg);` |
|    29 | 4692 | `	SXUNUSED(apArg);` |
|    59 | 4693 | `	if( pVal ){` |
|    59 | 4694 | `		ph7_result_value(pCtx,pVal);` |
|    30 | 4695 | `	}else{` |
|   ! 0 | 4696 | `		ph7_result_null(pCtx);` |
|     - | 4697 | `	}` |
|    59 | 4698 | `	return PH7_OK;` |
|     1 | 4699 | `}` |
|    48 | 4700 | `static int vm_builtin_SplDll_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 4701 | `{` |
|    49 | 4702 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    24 | 4703 | `	SXUNUSED(nArg);` |
|    24 | 4704 | `	SXUNUSED(apArg);` |
|    49 | 4705 | `	ph7_result_int64(pCtx,pThis ? PH7_NativeAttrInt(pThis,DLL_I) : 0);` |
|    49 | 4706 | `	return PH7_OK;` |
|     1 | 4707 | `}` |
|     - | 4708 | ``/* php's move_forward, with the direction flipped for prev() (its `flags ^ LIFO`). */`` |
|    58 | 4709 | `static int DllStep(ph7_context *pCtx,int bFlip)` |
|     1 | 4710 | `{` |
|    59 | 4711 | `	ph7_vm *pVm = pCtx->pVm;` |
|    59 | 4712 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 4713 | `	int iFlags;` |
|     - | 4714 | `	sxi64 iPos;` |
|    59 | 4715 | `	if( pThis == 0 ){` |
|   ! 0 | 4716 | `		return PH7_OK;` |
|     - | 4717 | `	}` |
|    59 | 4718 | `	iFlags = DllFlags(pThis);` |
|    59 | 4719 | `	if( bFlip ){` |
|   ! 0 | 4720 | `		iFlags ^= DLL_IT_LIFO;` |
|   ! 0 | 4721 | `	}` |
|    59 | 4722 | `	iPos = PH7_NativeAttrInt(pThis,DLL_I);` |
|    59 | 4723 | `	if( iPos < 0 \|\| iPos >= DllCount(pVm,pThis) ){` |
|     - | 4724 | `		/* php only steps a LIVE pointer; off the end nothing moves and nothing is` |
|     - | 4725 | `		 * consumed. The position still has to move for a plain walk, though, or` |
|     - | 4726 | `		 * prev() past the head could never come back. */` |
|   ! 0 | 4727 | `		if( (iFlags & DLL_IT_DELETE) == 0 ){` |
|   ! 0 | 4728 | `			PH7_NativeSetAttrInt(pVm,pThis,DLL_I,` |
|   ! 0 | 4729 | `				iPos + ((iFlags & DLL_IT_LIFO) ? -1 : 1));` |
|   ! 0 | 4730 | `		}` |
|   ! 0 | 4731 | `		return PH7_OK;` |
|     - | 4732 | `	}` |
|    59 | 4733 | `	if( iFlags & DLL_IT_DELETE ){` |
|    23 | 4734 | `		if( iFlags & DLL_IT_LIFO ){` |
|     7 | 4735 | `			DllArrayCall(pCtx,ph7_hashmap_pop,0,0);` |
|     7 | 4736 | `			PH7_NativeSetAttrInt(pVm,pThis,DLL_I,iPos-1);` |
|     4 | 4737 | `		}else{` |
|     - | 4738 | `			/* php consumes the head and does NOT advance: the walk stays at 0. */` |
|    17 | 4739 | `			DllArrayCall(pCtx,ph7_hashmap_shift,0,0);` |
|     - | 4740 | `		}` |
|    23 | 4741 | `		ph7_result_null(pCtx);` |
|    23 | 4742 | `		return PH7_OK;` |
|     - | 4743 | `	}` |
|    37 | 4744 | `	PH7_NativeSetAttrInt(pVm,pThis,DLL_I,iPos + ((iFlags & DLL_IT_LIFO) ? -1 : 1));` |
|    37 | 4745 | `	return PH7_OK;` |
|    30 | 4746 | `}` |
|    58 | 4747 | `static int vm_builtin_SplDll_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 4748 | `{` |
|    29 | 4749 | `	SXUNUSED(nArg);` |
|    29 | 4750 | `	SXUNUSED(apArg);` |
|    59 | 4751 | `	return DllStep(pCtx,FALSE);` |
|     1 | 4752 | `}` |
|   ! 0 | 4753 | `static int vm_builtin_SplDll_prev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 4754 | `{` |
|   ! 0 | 4755 | `	SXUNUSED(nArg);` |
|   ! 0 | 4756 | `	SXUNUSED(apArg);` |
|   ! 0 | 4757 | `	return DllStep(pCtx,TRUE);` |
|   ! 0 | 4758 | `}` |
|     - | 4759 | `/*` |
|     - | 4760 | `` * php's get_debug_info: `flags` then `dllist`, and NOTHING for the (array) cast --`` |
|     - | 4761 | ` * the same var_dump/cast disagreement WeakReference has, which is why xPresent is` |
|     - | 4762 | `` * told which surface is asking. `__debugInfo()` is the same array, reachable by`` |
|     - | 4763 | ` * name because php declares it.` |
|     - | 4764 | ` */` |
|     2 | 4765 | `static sxi32 DllFillDebug(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut)` |
|     1 | 4766 | `{` |
|     - | 4767 | `	ph7_value sKey,sVal,*pStore;` |
|     3 | 4768 | `	PH7_MemObjInitFromString(pVm,&sKey,0);` |
|     3 | 4769 | `	PH7_MemObjStringAppend(&sKey,"flags",sizeof("flags")-1);` |
|     3 | 4770 | `	PH7_MemObjInitFromInt(pVm,&sVal,DllFlags(pThis));` |
|     3 | 4771 | `	ph7_array_add_elem(pOut,&sKey,&sVal);` |
|     3 | 4772 | `	PH7_MemObjRelease(&sKey);` |
|     3 | 4773 | `	PH7_MemObjRelease(&sVal);` |
|     3 | 4774 | `	pStore = DllSlot(pVm,pThis);` |
|     3 | 4775 | `	PH7_MemObjInitFromString(pVm,&sKey,0);` |
|     3 | 4776 | `	PH7_MemObjStringAppend(&sKey,"dllist",sizeof("dllist")-1);` |
|     3 | 4777 | `	if( pStore ){` |
|     3 | 4778 | `		ph7_array_add_elem(pOut,&sKey,pStore);` |
|     1 | 4779 | `	}` |
|     3 | 4780 | `	PH7_MemObjRelease(&sKey);` |
|     3 | 4781 | `	return PH7_OK;` |
|     1 | 4782 | `}` |
|     2 | 4783 | `static sxi32 DllPresent(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)` |
|     1 | 4784 | `{` |
|     3 | 4785 | `	if( !bDebug ){` |
|     3 | 4786 | `		return PH7_OK;   /* php's (array) cast and var_export show nothing */` |
|     - | 4787 | `	}` |
|   ! 0 | 4788 | `	return DllFillDebug(pVm,pThis,pOut);` |
|     2 | 4789 | `}` |
|     2 | 4790 | `static int vm_builtin_SplDll_debugInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 4791 | `{` |
|     3 | 4792 | `	ph7_vm *pVm = pCtx->pVm;` |
|     3 | 4793 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 4794 | `	ph7_value sOut;` |
|     1 | 4795 | `	SXUNUSED(nArg);` |
|     1 | 4796 | `	SXUNUSED(apArg);` |
|     3 | 4797 | `	PH7_MemObjInit(pVm,&sOut);` |
|     3 | 4798 | `	if( PH7_MemObjToHashmap(&sOut) != SXRET_OK ){` |
|   ! 0 | 4799 | `		PH7_MemObjRelease(&sOut);` |
|   ! 0 | 4800 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 4801 | `	}` |
|     3 | 4802 | `	DllFillDebug(pVm,pThis,&sOut);` |
|     3 | 4803 | `	ph7_result_value(pCtx,&sOut);` |
|     3 | 4804 | `	PH7_MemObjRelease(&sOut);` |
|     3 | 4805 | `	return PH7_OK;` |
|     2 | 4806 | `}` |
|     - | 4807 | `/*` |
|     - | 4808 | ` * php's __serialize(): [flags, elements, dynamic members]. This is what` |
|     - | 4809 | ` * serialize() actually uses -- the Serializable pair below exists because the` |
|     - | 4810 | ` * interface is still declared, and php words its own legacy format there.` |
|     - | 4811 | ` */` |
|    18 | 4812 | `static int vm_builtin_SplDll_serializeMagic(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 4813 | `{` |
|    19 | 4814 | `	ph7_vm *pVm = pCtx->pVm;` |
|    19 | 4815 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 4816 | `	ph7_value sOut,sVal,*pStore;` |
|     9 | 4817 | `	SXUNUSED(nArg);` |
|     9 | 4818 | `	SXUNUSED(apArg);` |
|    19 | 4819 | `	PH7_MemObjInit(pVm,&sOut);` |
|    19 | 4820 | `	if( PH7_MemObjToHashmap(&sOut) != SXRET_OK ){` |
|   ! 0 | 4821 | `		PH7_MemObjRelease(&sOut);` |
|   ! 0 | 4822 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 4823 | `	}` |
|    19 | 4824 | `	PH7_MemObjInitFromInt(pVm,&sVal,DllFlags(pThis));` |
|    19 | 4825 | `	ph7_array_add_elem(&sOut,0,&sVal);` |
|    19 | 4826 | `	PH7_MemObjRelease(&sVal);` |
|    19 | 4827 | `	pStore = DllSlot(pVm,pThis);` |
|    19 | 4828 | `	if( pStore ){` |
|    19 | 4829 | `		ph7_array_add_elem(&sOut,0,pStore);` |
|     9 | 4830 | `	}` |
|     - | 4831 | `	/* The members slot: php's own properties — empty for a bare SplStack, a` |
|     - | 4832 | `	 * SUBCLASS's declared slots when there is one. */` |
|    19 | 4833 | `	if( SplMembersOf(pVm,pThis,&sVal) == SXRET_OK ){` |
|    19 | 4834 | `		ph7_array_add_elem(&sOut,0,&sVal);` |
|     9 | 4835 | `	}` |
|    19 | 4836 | `	PH7_MemObjRelease(&sVal);` |
|    19 | 4837 | `	ph7_result_value(pCtx,&sOut);` |
|    19 | 4838 | `	PH7_MemObjRelease(&sOut);` |
|    19 | 4839 | `	return PH7_OK;` |
|    10 | 4840 | `}` |
|     6 | 4841 | `static int vm_builtin_SplDll_unserializeMagic(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 4842 | `{` |
|     7 | 4843 | `	ph7_vm *pVm = pCtx->pVm;` |
|     7 | 4844 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 4845 | `	ph7_hashmap *pData;` |
|     7 | 4846 | `	ph7_hashmap_node *pNode = 0;` |
|     - | 4847 | `	ph7_value *pFlags,*pStore,*pSlot;` |
|     7 | 4848 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_HASHMAP) == 0 \|\| pThis == 0 ){` |
|   ! 0 | 4849 | `		return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     - | 4850 | `			"Incomplete or ill-typed serialization data");` |
|     - | 4851 | `	}` |
|     7 | 4852 | `	pData = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     7 | 4853 | `	if( HashmapLookupIntKey(pData,0,&pNode) != SXRET_OK ){` |
|   ! 0 | 4854 | `		return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     - | 4855 | `			"Incomplete or ill-typed serialization data");` |
|     - | 4856 | `	}` |
|     7 | 4857 | `	pFlags = HashmapExtractNodeValue(pNode);` |
|     6 | 4858 | `	if( pFlags == 0 \|\| (pFlags->iFlags & MEMOBJ_INT) == 0` |
|     7 | 4859 | `	 \|\| HashmapLookupIntKey(pData,1,&pNode) != SXRET_OK ){` |
|   ! 0 | 4860 | `		return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     - | 4861 | `			"Incomplete or ill-typed serialization data");` |
|     - | 4862 | `	}` |
|     7 | 4863 | `	pStore = HashmapExtractNodeValue(pNode);` |
|     7 | 4864 | `	if( pStore == 0 \|\| (pStore->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|   ! 0 | 4865 | `		return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     - | 4866 | `			"Incomplete or ill-typed serialization data");` |
|     - | 4867 | `	}` |
|     7 | 4868 | `	PH7_NativeSetAttrInt(pVm,pThis,DLL_FL,ph7_value_to_int64(pFlags));` |
|     7 | 4869 | `	pSlot = PH7_NativeAttr(pThis,DLL_Q);` |
|     7 | 4870 | `	if( pSlot ){` |
|     7 | 4871 | `		PH7_MemObjRelease(pSlot);` |
|     7 | 4872 | `		PH7_MemObjStore(pStore,pSlot);` |
|     3 | 4873 | `	}` |
|     7 | 4874 | `	if( HashmapLookupIntKey(pData,2,&pNode) == SXRET_OK ){` |
|     7 | 4875 | `		SplMembersLoad(pThis,HashmapExtractNodeValue(pNode));` |
|     3 | 4876 | `	}` |
|     7 | 4877 | `	return PH7_OK;` |
|     4 | 4878 | `}` |
|     - | 4879 | `/*` |
|     - | 4880 | ` * php's Serializable pair, kept because the interface is still declared: the` |
|     - | 4881 | ` * format is the serialized FLAGS followed by one ':' + serialized value per` |
|     - | 4882 | ` * element ("i:0;:i:1;:i:2;"), which nothing else in php produces or reads.` |
|     - | 4883 | ` */` |
|     - | 4884 | `/*` |
|     - | 4885 | ` * One serialized value, appended to a blob. The RESET is the point: the engine's` |
|     - | 4886 | ` * serialize() writes through ph7_value_string, which APPENDS to the context's` |
|     - | 4887 | ` * return slot rather than replacing it, so a loop that calls it per element` |
|     - | 4888 | ` * accumulates every previous answer into the next one. Shared with` |
|     - | 4889 | ` * SplObjectStorage's legacy format, which is built the same way.` |
|     - | 4890 | ` */` |
|    32 | 4891 | `static void SplSerializeInto(ph7_context *pCtx,ph7_value **apCall,SyBlob *pOut)` |
|     1 | 4892 | `{` |
|    33 | 4893 | `	int nLen = 0;` |
|     - | 4894 | `	const char *zTxt;` |
|    33 | 4895 | `	if( pCtx->pRet ){` |
|    33 | 4896 | `		PH7_MemObjRelease(pCtx->pRet);` |
|    16 | 4897 | `	}` |
|    33 | 4898 | `	vm_builtin_serialize(pCtx,1,apCall);` |
|    33 | 4899 | `	if( pCtx->pRet == 0 ){` |
|   ! 0 | 4900 | `		return;` |
|     - | 4901 | `	}` |
|    33 | 4902 | `	zTxt = ph7_value_to_string(pCtx->pRet,&nLen);` |
|    33 | 4903 | `	SyBlobAppend(pOut,zTxt,(sxu32)nLen);` |
|    17 | 4904 | `}` |
|     2 | 4905 | `static int vm_builtin_SplDll_serialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 4906 | `{` |
|     3 | 4907 | `	ph7_vm *pVm = pCtx->pVm;` |
|     3 | 4908 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     3 | 4909 | `	ph7_hashmap *pMap = DllMap(pVm,pThis);` |
|     - | 4910 | `	ph7_hashmap_node *pNode;` |
|     - | 4911 | `	SyBlob sOut;` |
|     - | 4912 | `	ph7_value sFlags,*apCall[1];` |
|     3 | 4913 | `	sxi64 n,nCount = pMap ? (sxi64)pMap->nEntry : 0;` |
|     1 | 4914 | `	SXUNUSED(nArg);` |
|     1 | 4915 | `	SXUNUSED(apArg);` |
|     3 | 4916 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|     3 | 4917 | `	PH7_MemObjInitFromInt(pVm,&sFlags,DllFlags(pThis));` |
|     3 | 4918 | `	apCall[0] = &sFlags;` |
|     3 | 4919 | `	SplSerializeInto(pCtx,apCall,&sOut);` |
|     3 | 4920 | `	PH7_MemObjRelease(&sFlags);` |
|     9 | 4921 | `	for( n = 0 ; n < nCount ; ++n ){` |
|     - | 4922 | `		ph7_value *pVal;` |
|     7 | 4923 | `		pNode = 0;` |
|     7 | 4924 | `		if( HashmapLookupIntKey(pMap,n,&pNode) != SXRET_OK ){` |
|   ! 0 | 4925 | `			continue;` |
|     - | 4926 | `		}` |
|     7 | 4927 | `		pVal = HashmapExtractNodeValue(pNode);` |
|     7 | 4928 | `		if( pVal == 0 ){` |
|   ! 0 | 4929 | `			continue;` |
|     - | 4930 | `		}` |
|     7 | 4931 | `		apCall[0] = pVal;` |
|     7 | 4932 | `		SyBlobAppend(&sOut,":",1);` |
|     7 | 4933 | `		SplSerializeInto(pCtx,apCall,&sOut);` |
|     4 | 4934 | `	}` |
|     - | 4935 | `	/* ph7_result_string APPENDS too, and pRet still holds the LAST element's` |
|     - | 4936 | `	 * serialization from the loop above — drop it before writing the answer. */` |
|     3 | 4937 | `	if( pCtx->pRet ){` |
|     3 | 4938 | `		PH7_MemObjRelease(pCtx->pRet);` |
|     1 | 4939 | `	}` |
|     3 | 4940 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|     3 | 4941 | `	SyBlobRelease(&sOut);` |
|     3 | 4942 | `	return PH7_OK;` |
|     1 | 4943 | `}` |
|   ! 0 | 4944 | `static int vm_builtin_SplDll_unserialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 4945 | `{` |
|   ! 0 | 4946 | `	ph7_vm *pVm = pCtx->pVm;` |
|   ! 0 | 4947 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 4948 | `	const char *zData,*zCur,*zEnd;` |
|   ! 0 | 4949 | `	int nData = 0;` |
|   ! 0 | 4950 | `	int bFirst = 1;` |
|     - | 4951 | `	ph7_value *pSlot;` |
|     - | 4952 | `	ph7_hashmap *pMap;` |
|   ! 0 | 4953 | `	if( nArg < 1 \|\| pThis == 0 ){` |
|   ! 0 | 4954 | `		return PH7_OK;` |
|     - | 4955 | `	}` |
|   ! 0 | 4956 | `	zData = ph7_value_to_string(apArg[0],&nData);` |
|   ! 0 | 4957 | `	if( nData < 1 ){` |
|   ! 0 | 4958 | `		return PH7_OK;   /* php returns without touching the list */` |
|     - | 4959 | `	}` |
|   ! 0 | 4960 | `	pSlot = DllSlot(pVm,pThis);` |
|   ! 0 | 4961 | `	pMap = pSlot ? (ph7_hashmap *)pSlot->x.pOther : 0;` |
|   ! 0 | 4962 | `	if( pMap == 0 ){` |
|   ! 0 | 4963 | `		return PH7_OK;` |
|     - | 4964 | `	}` |
|     - | 4965 | `	/* php empties the list first, then reads the flags, then one ':'-prefixed` |
|     - | 4966 | `	 * value per element. A malformed tail is an UnexpectedValueException naming` |
|     - | 4967 | `	 * the byte offset — reproduced here from the same position arithmetic. */` |
|   ! 0 | 4968 | `	while( pMap->pFirst ){` |
|   ! 0 | 4969 | `		PH7_HashmapUnlinkNode(pMap->pFirst,TRUE);` |
|   ! 0 | 4970 | `	}` |
|   ! 0 | 4971 | `	zCur = zData;` |
|   ! 0 | 4972 | `	zEnd = &zData[nData];` |
|   ! 0 | 4973 | `	while( zCur < zEnd ){` |
|     - | 4974 | `		ph7_value sPart,sRes,*apCall[1];` |
|     - | 4975 | `		int nPart;` |
|   ! 0 | 4976 | `		const char *zStop = zCur;` |
|   ! 0 | 4977 | `		if( !bFirst ){` |
|   ! 0 | 4978 | `			if( zCur[0] != ':' ){` |
|   ! 0 | 4979 | `				break;` |
|     - | 4980 | `			}` |
|   ! 0 | 4981 | `			zCur++;` |
|   ! 0 | 4982 | `		}` |
|     - | 4983 | `		/* One serialized scalar reaches up to and including its ';'. */` |
|   ! 0 | 4984 | `		while( zStop < zEnd && zStop[0] != ';' ){` |
|   ! 0 | 4985 | `			zStop++;` |
|   ! 0 | 4986 | `		}` |
|   ! 0 | 4987 | `		if( zStop >= zEnd ){` |
|   ! 0 | 4988 | `			zStop = zEnd;` |
|   ! 0 | 4989 | `		}else{` |
|   ! 0 | 4990 | `			zStop++;` |
|     - | 4991 | `		}` |
|   ! 0 | 4992 | `		nPart = (int)(zStop - zCur);` |
|   ! 0 | 4993 | `		if( nPart <= 0 ){` |
|   ! 0 | 4994 | `			break;` |
|     - | 4995 | `		}` |
|   ! 0 | 4996 | `		PH7_MemObjInitFromString(pVm,&sPart,0);` |
|   ! 0 | 4997 | `		PH7_MemObjStringAppend(&sPart,zCur,(sxu32)nPart);` |
|   ! 0 | 4998 | `		apCall[0] = &sPart;` |
|   ! 0 | 4999 | `		PH7_MemObjInit(pVm,&sRes);` |
|   ! 0 | 5000 | `		if( pCtx->pRet ){` |
|   ! 0 | 5001 | `			PH7_MemObjRelease(pCtx->pRet);   /* see DllSerializeInto: pRet is appended to */` |
|   ! 0 | 5002 | `		}` |
|   ! 0 | 5003 | `		vm_builtin_unserialize(pCtx,1,apCall);` |
|   ! 0 | 5004 | `		if( pCtx->pRet ){` |
|   ! 0 | 5005 | `			PH7_MemObjStore(pCtx->pRet,&sRes);` |
|   ! 0 | 5006 | `		}` |
|   ! 0 | 5007 | `		if( bFirst ){` |
|   ! 0 | 5008 | `			PH7_NativeSetAttrInt(pVm,pThis,DLL_FL,ph7_value_to_int64(&sRes));` |
|   ! 0 | 5009 | `			bFirst = 0;` |
|   ! 0 | 5010 | `		}else{` |
|   ! 0 | 5011 | `			PH7_HashmapInsert(pMap,0,&sRes);` |
|     - | 5012 | `		}` |
|   ! 0 | 5013 | `		PH7_MemObjRelease(&sPart);` |
|   ! 0 | 5014 | `		PH7_MemObjRelease(&sRes);` |
|   ! 0 | 5015 | `		zCur = zStop;` |
|   ! 0 | 5016 | `	}` |
|   ! 0 | 5017 | `	ph7_result_null(pCtx);` |
|   ! 0 | 5018 | `	return PH7_OK;` |
|   ! 0 | 5019 | `}` |
|     - | 5020 | `/*` |
|     - | 5021 | ` * The declaration. Method ORDER is spl_dllist.stub.php's, php declares NO` |
|     - | 5022 | ` * constructor for any of the three, and the IT_FIX bit is a per-class DEFAULT on` |
|     - | 5023 | ` * the flags slot -- which is exactly how php does it (the create handler stamps` |
|     - | 5024 | ` * the flags; there is no constructor to run).` |
|     - | 5025 | ` */` |
|  5146 | 5026 | `static sxi32 VmInstallSplDllist(ph7_vm *pVm)` |
|     5 | 5027 | `{` |
|     - | 5028 | `	static const PH7_NativeMethodDef aDllMethod[] = {` |
|     - | 5029 | `		{ "add",             PH7_MOD_PUBLIC, "int $index, mixed $value", "@void",` |
|     - | 5030 | `		  vm_builtin_SplDll_add },` |
|     - | 5031 | `		{ "pop",             PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplDll_pop },` |
|     - | 5032 | `		{ "shift",           PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplDll_shift },` |
|     - | 5033 | `		{ "push",            PH7_MOD_PUBLIC, "mixed $value", "@void", vm_builtin_SplDll_push },` |
|     - | 5034 | `		{ "unshift",         PH7_MOD_PUBLIC, "mixed $value", "@void", vm_builtin_SplDll_unshift },` |
|     - | 5035 | `		{ "top",             PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplDll_top },` |
|     - | 5036 | `		{ "bottom",          PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplDll_bottom },` |
|     - | 5037 | `		{ "__debugInfo",     PH7_MOD_PUBLIC, "", "@array", vm_builtin_SplDll_debugInfo },` |
|     - | 5038 | `		{ "count",           PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplDll_count },` |
|     - | 5039 | `		{ "isEmpty",         PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplDll_isEmpty },` |
|     - | 5040 | `		{ "setIteratorMode", PH7_MOD_PUBLIC, "int $mode", "@int",` |
|     - | 5041 | `		  vm_builtin_SplDll_setIteratorMode },` |
|     - | 5042 | `		{ "getIteratorMode", PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplDll_getIteratorMode },` |
|     - | 5043 | ``		/* php's stub leaves these four offsets UNTYPED (a `@param int` docblock, which`` |
|     - | 5044 | `		 * Reflection does not print) while the ZPP enforces int -- so the signature says` |
|     - | 5045 | `		 * nothing and each body runs PH7_IntArgResolve itself. */` |
|     - | 5046 | `		{ "offsetExists",    PH7_MOD_PUBLIC, "$index", "@bool", vm_builtin_SplDll_offsetExists },` |
|     - | 5047 | `		{ "offsetGet",       PH7_MOD_PUBLIC, "$index", "@mixed", vm_builtin_SplDll_offsetGet },` |
|     - | 5048 | `		{ "offsetSet",       PH7_MOD_PUBLIC, "$index, mixed $value", "@void",` |
|     - | 5049 | `		  vm_builtin_SplDll_offsetSet },` |
|     - | 5050 | `		{ "offsetUnset",     PH7_MOD_PUBLIC, "$index", "@void", vm_builtin_SplDll_offsetUnset },` |
|     - | 5051 | `		{ "rewind",          PH7_MOD_PUBLIC, "", "@void", vm_builtin_SplDll_rewind },` |
|     - | 5052 | `		{ "current",         PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplDll_current },` |
|     - | 5053 | `		{ "key",             PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplDll_key },` |
|     - | 5054 | `		{ "prev",            PH7_MOD_PUBLIC, "", "@void", vm_builtin_SplDll_prev },` |
|     - | 5055 | `		{ "next",            PH7_MOD_PUBLIC, "", "@void", vm_builtin_SplDll_next },` |
|     - | 5056 | `		{ "valid",           PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplDll_valid },` |
|     - | 5057 | `		{ "unserialize",     PH7_MOD_PUBLIC, "string $data", "@void",` |
|     - | 5058 | `		  vm_builtin_SplDll_unserialize },` |
|     - | 5059 | `		{ "serialize",       PH7_MOD_PUBLIC, "", "@string", vm_builtin_SplDll_serialize },` |
|     - | 5060 | `		{ "__serialize",     PH7_MOD_PUBLIC, "", "@array", vm_builtin_SplDll_serializeMagic },` |
|     - | 5061 | `		{ "__unserialize",   PH7_MOD_PUBLIC, "array $data", "@void",` |
|     - | 5062 | `		  vm_builtin_SplDll_unserializeMagic },` |
|     - | 5063 | `	};` |
|     - | 5064 | `	static const PH7_NativeMethodDef aQueueMethod[] = {` |
|     - | 5065 | `		/* php's @implementation-alias: the same C bodies under the queue's names. */` |
|     - | 5066 | `		{ "enqueue", PH7_MOD_PUBLIC, "mixed $value", "@void", vm_builtin_SplDll_push },` |
|     - | 5067 | `		{ "dequeue", PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplDll_shift },` |
|     - | 5068 | `	};` |
|     - | 5069 | `	static const PH7_NativeConstDef aDllConst[] = {` |
|     - | 5070 | `		{ "IT_MODE_LIFO",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, DLL_IT_LIFO, 0, 0.0 },` |
|     - | 5071 | `		{ "IT_MODE_FIFO",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 0, 0, 0.0 },` |
|     - | 5072 | `		{ "IT_MODE_DELETE", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, DLL_IT_DELETE, 0, 0.0 },` |
|     - | 5073 | `		{ "IT_MODE_KEEP",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 0, 0, 0.0 },` |
|     - | 5074 | `	};` |
|     - | 5075 | `	static const PH7_NativePropDef aDllProp[] = {` |
|     - | 5076 | `		{ DLL_Q,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - | 5077 | `		{ DLL_FL, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - | 5078 | `		{ DLL_I,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - | 5079 | `	};` |
|     - | 5080 | `	/* php's object handler stamps IT_FIX (and LIFO for a stack) at CREATION, which` |
|     - | 5081 | `	 * is why neither subclass declares a constructor and why the bit survives every` |
|     - | 5082 | `	 * setIteratorMode(). A per-class default on the flags slot says the same thing. */` |
|     - | 5083 | `	static const PH7_NativePropDef aQueueProp[] = {` |
|     - | 5084 | `		{ DLL_FL, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN,` |
|     - | 5085 | `		  { 0, 0, PH7_NATIVE_VAL_INT, DLL_IT_FIX, 0, 0.0 }, 0 },` |
|     - | 5086 | `	};` |
|     - | 5087 | `	static const PH7_NativePropDef aStackProp[] = {` |
|     - | 5088 | `		{ DLL_FL, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN,` |
|     - | 5089 | `		  { 0, 0, PH7_NATIVE_VAL_INT, DLL_IT_FIX\|DLL_IT_LIFO, 0, 0.0 }, 0 },` |
|     - | 5090 | `	};` |
|     - | 5091 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - | 5092 | `		{ "SplDoublyLinkedList", 0, "Iterator,Countable,ArrayAccess,Serializable", 0,` |
|     - | 5093 | `		  aDllMethod, SX_ARRAYSIZE(aDllMethod),` |
|     - | 5094 | `		  aDllConst, SX_ARRAYSIZE(aDllConst),` |
|     - | 5095 | `		  aDllProp, SX_ARRAYSIZE(aDllProp), 0, 0, DllPresent },` |
|     - | 5096 | `		{ "SplQueue", "SplDoublyLinkedList", 0, 0,` |
|     - | 5097 | `		  aQueueMethod, SX_ARRAYSIZE(aQueueMethod), 0, 0,` |
|     - | 5098 | `		  aQueueProp, SX_ARRAYSIZE(aQueueProp), 0, 0, DllPresent },` |
|     - | 5099 | `		{ "SplStack", "SplDoublyLinkedList", 0, 0,` |
|     - | 5100 | `		  0, 0, 0, 0,` |
|     - | 5101 | `		  aStackProp, SX_ARRAYSIZE(aStackProp), 0, 0, DllPresent },` |
|     - | 5102 | `	};` |
|  5151 | 5103 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|     5 | 5104 | `}` |
|     - | 5105 | `/*` |
|     - | 5106 | ` * ---------------------------------------------------------------------------` |
|     - | 5107 | ` * SplHeap, SplMinHeap, SplMaxHeap and SplPriorityQueue.` |
|     - | 5108 | ` *` |
|     - | 5109 | `` * php's `spl_heap_object` is an array plus a FLAGS word, and the flags carry the`` |
|     - | 5110 | ` * thing the chunk could not express at all: **SPL_HEAP_CORRUPTED**. php sets it` |
|     - | 5111 | ` * when an exception escapes the user's compare() mid-sift -- the heap invariant is` |
|     - | 5112 | ` * then unknown -- and every operation that DEPENDS on the invariant refuses with` |
|     - | 5113 | ` * "Heap is corrupted, heap properties are no longer ensured." until` |
|     - | 5114 | `` * recoverFromCorruption() clears it. The chunk hardcoded `isCorrupted()` to false`` |
|     - | 5115 | `` * and `recoverFromCorruption()` to true, so a throwing comparator left a silently`` |
|     - | 5116 | ` * mis-ordered heap that kept answering.` |
|     - | 5117 | ` *` |
|     - | 5118 | ` * Which operations refuse is not guessable and was mapped against the oracle:` |
|     - | 5119 | ` * insert, extract, top, next and __serialize/__unserialize refuse; count,` |
|     - | 5120 | ` * isEmpty, rewind, valid, current, key, isCorrupted, recoverFromCorruption and` |
|     - | 5121 | `` * __debugInfo all keep working. (A `foreach` refuses because it reaches next().)`` |
|     - | 5122 | ` *` |
|     - | 5123 | ` * php's priority-queue node is exactly {data, priority} -- the chunk carried a` |
|     - | 5124 | `` * third field, a descending `__serial` it never compared with, which leaked into`` |
|     - | 5125 | ` * serialize(), var_dump() and the (array) cast as a nonsense PHP_INT_MAX-relative` |
|     - | 5126 | ` * integer. It is gone; equal priorities keep the order php's strictly-greater` |
|     - | 5127 | ` * swap gives them.` |
|     - | 5128 | ` *` |
|     - | 5129 | ` * The other three the chunk lacked, the same three the SplDoublyLinkedList` |
|     - | 5130 | `` * conversion lacked: `__debugInfo()` (flags / isCorrupted / heap, and for the queue`` |
|     - | 5131 | ` * the heap entries are rendered EXTR_BOTH-style whatever the extract flags say),` |
|     - | 5132 | `` * and the `__serialize()`/`__unserialize()` pair, whose payload is`` |
|     - | 5133 | ` * [members, {flags, heap_elements}] and whose reader VALIDATES -- a plain heap` |
|     - | 5134 | ` * refuses a non-zero flags word, the queue refuses a zero one.` |
|     - | 5135 | ` */` |
|     - | 5136 | `#define HP_H  "__h"   /* the heap array, in heap order */` |
|     - | 5137 | `#define HP_FL "__fl"  /* php's intern->flags: the queue's EXTR bits, 0 for a heap */` |
|     - | 5138 | `#define HP_CR "__cr"  /* php's SPL_HEAP_CORRUPTED */` |
|     - | 5139 |  |
|     - | 5140 | `#define PQ_EXTR_DATA     1` |
|     - | 5141 | `#define PQ_EXTR_PRIORITY 2` |
|     - | 5142 | `#define PQ_EXTR_BOTH     3` |
|     - | 5143 | `#define PQ_EXTR_MASK     3` |
|     - | 5144 |  |
|  1872 | 5145 | `static ph7_value * HeapSlot(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 | 5146 | `{` |
|  1873 | 5147 | `	ph7_value *pSlot = pThis ? PH7_NativeAttr(pThis,HP_H) : 0;` |
|  1873 | 5148 | `	if( pSlot == 0 ){` |
|   ! 0 | 5149 | `		return 0;` |
|     - | 5150 | `	}` |
|  1873 | 5151 | `	if( (pSlot->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    87 | 5152 | `		if( PH7_MemObjToHashmap(pSlot) != SXRET_OK ){` |
|   ! 0 | 5153 | `			return 0;` |
|     - | 5154 | `		}` |
|    43 | 5155 | `	}` |
|  1873 | 5156 | `	if( PH7_HashmapCowSeparate(pVm,pSlot) == 0 ){` |
|   ! 0 | 5157 | `		return 0;` |
|     - | 5158 | `	}` |
|  1873 | 5159 | `	return pSlot;` |
|   937 | 5160 | `}` |
|  1852 | 5161 | `static ph7_hashmap * HeapMap(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 | 5162 | `{` |
|  1853 | 5163 | `	ph7_value *pSlot = HeapSlot(pVm,pThis);` |
|  1853 | 5164 | `	return pSlot ? (ph7_hashmap *)pSlot->x.pOther : 0;` |
|     1 | 5165 | `}` |
|   584 | 5166 | `static sxi64 HeapCount(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 | 5167 | `{` |
|   585 | 5168 | `	ph7_hashmap *pMap = HeapMap(pVm,pThis);` |
|   585 | 5169 | `	return pMap ? (sxi64)pMap->nEntry : 0;` |
|     1 | 5170 | `}` |
|     - | 5171 | `/* Re-resolved on every use: any call into the user's compare() may have moved` |
|     - | 5172 | ` * pVm->aMemObj under us (rule 47). */` |
|   746 | 5173 | `static ph7_value * HeapAt(ph7_vm *pVm,ph7_class_instance *pThis,sxi64 i)` |
|     1 | 5174 | `{` |
|   747 | 5175 | `	ph7_hashmap *pMap = HeapMap(pVm,pThis);` |
|   747 | 5176 | `	ph7_hashmap_node *pNode = 0;` |
|   747 | 5177 | `	if( pMap == 0 \|\| HashmapLookupIntKey(pMap,i,&pNode) != SXRET_OK ){` |
|   ! 0 | 5178 | `		return 0;` |
|     - | 5179 | `	}` |
|   747 | 5180 | `	return HashmapExtractNodeValue(pNode);` |
|   374 | 5181 | `}` |
|   220 | 5182 | `static void HeapPut(ph7_vm *pVm,ph7_class_instance *pThis,sxi64 i,ph7_value *pVal)` |
|     1 | 5183 | `{` |
|   221 | 5184 | `	ph7_hashmap *pMap = HeapMap(pVm,pThis);` |
|     - | 5185 | `	ph7_value sKey;` |
|   221 | 5186 | `	if( pMap == 0 ){` |
|   ! 0 | 5187 | `		return;` |
|     - | 5188 | `	}` |
|   221 | 5189 | `	PH7_MemObjInitFromInt(pVm,&sKey,i);` |
|   221 | 5190 | `	PH7_HashmapInsert(pMap,&sKey,pVal);` |
|   221 | 5191 | `	PH7_MemObjRelease(&sKey);` |
|   111 | 5192 | `}` |
|    80 | 5193 | `static void HeapSwap(ph7_vm *pVm,ph7_class_instance *pThis,sxi64 i,sxi64 j)` |
|     1 | 5194 | `{` |
|     - | 5195 | `	ph7_value sI,sJ,*pV;` |
|    81 | 5196 | `	PH7_MemObjInit(pVm,&sI);` |
|    81 | 5197 | `	PH7_MemObjInit(pVm,&sJ);` |
|    81 | 5198 | `	pV = HeapAt(pVm,pThis,i);` |
|    81 | 5199 | `	if( pV ){` |
|    81 | 5200 | `		PH7_MemObjStore(pV,&sI);` |
|    40 | 5201 | `	}` |
|    81 | 5202 | `	pV = HeapAt(pVm,pThis,j);` |
|    81 | 5203 | `	if( pV ){` |
|    81 | 5204 | `		PH7_MemObjStore(pV,&sJ);` |
|    40 | 5205 | `	}` |
|    81 | 5206 | `	HeapPut(pVm,pThis,i,&sJ);` |
|    81 | 5207 | `	HeapPut(pVm,pThis,j,&sI);` |
|    81 | 5208 | `	PH7_MemObjRelease(&sI);` |
|    81 | 5209 | `	PH7_MemObjRelease(&sJ);` |
|    81 | 5210 | `}` |
|   390 | 5211 | `static int HeapCorrupted(ph7_class_instance *pThis)` |
|     1 | 5212 | `{` |
|   391 | 5213 | `	return pThis ? (int)PH7_NativeAttrInt(pThis,HP_CR) : 0;` |
|     1 | 5214 | `}` |
|     - | 5215 | `/*` |
|     - | 5216 | ` * php's spl_heap_consistency_validations. Only the operations that DEPEND on the` |
|     - | 5217 | ` * heap invariant call it -- count()/current()/key() answer from the array and are` |
|     - | 5218 | ` * left alone, which is why a corrupted heap still reports its size.` |
|     - | 5219 | ` */` |
|   382 | 5220 | `static sxi32 HeapCheck(ph7_context *pCtx)` |
|     1 | 5221 | `{` |
|   383 | 5222 | `	if( !HeapCorrupted(PH7_ContextThis(pCtx)) ){` |
|   371 | 5223 | `		return SXRET_OK;` |
|     - | 5224 | `	}` |
|    13 | 5225 | `	return PH7_VmThrowException(pCtx,"RuntimeException",` |
|     - | 5226 | `		"Heap is corrupted, heap properties are no longer ensured.");` |
|   192 | 5227 | `}` |
|     - | 5228 | `/* A priority-queue node is php's {data, priority}: nothing else, and in that order. */` |
|   550 | 5229 | `static int HeapIsPq(ph7_class_instance *pThis)` |
|     1 | 5230 | `{` |
|     - | 5231 | `	ph7_class *pPq;` |
|   551 | 5232 | `	if( pThis == 0 ){` |
|   ! 0 | 5233 | `		return FALSE;` |
|     - | 5234 | `	}` |
|   551 | 5235 | `	pPq = PH7_VmExtractClass(pThis->pVm,"SplPriorityQueue",sizeof("SplPriorityQueue")-1,FALSE,0);` |
|   551 | 5236 | `	return pPq && PH7_VmInstanceOf(pThis->pClass,pPq);` |
|   276 | 5237 | `}` |
|   168 | 5238 | `static ph7_value * HeapNodePart(ph7_value *pNode,const char *zKey)` |
|     1 | 5239 | `{` |
|     - | 5240 | `	ph7_hashmap *pMap;` |
|   169 | 5241 | `	ph7_hashmap_node *pEnt = 0;` |
|   169 | 5242 | `	if( pNode == 0 \|\| (pNode->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|   ! 0 | 5243 | `		return 0;` |
|     - | 5244 | `	}` |
|   169 | 5245 | `	pMap = (ph7_hashmap *)pNode->x.pOther;` |
|   169 | 5246 | `	if( HashmapLookupBlobKey(pMap,zKey,(sxu32)SyStrlen(zKey),&pEnt) != SXRET_OK ){` |
|   ! 0 | 5247 | `		return 0;` |
|     - | 5248 | `	}` |
|   169 | 5249 | `	return HashmapExtractNodeValue(pEnt);` |
|    85 | 5250 | `}` |
|    74 | 5251 | `static sxi32 HeapMakeNode(ph7_vm *pVm,ph7_value *pData,ph7_value *pPrio,ph7_value *pOut)` |
|     1 | 5252 | `{` |
|     - | 5253 | `	ph7_value sKey;` |
|    75 | 5254 | `	if( PH7_MemObjToHashmap(pOut) != SXRET_OK ){` |
|   ! 0 | 5255 | `		return SXERR_MEM;` |
|     - | 5256 | `	}` |
|    75 | 5257 | `	PH7_MemObjInitFromString(pVm,&sKey,0);` |
|    75 | 5258 | `	PH7_MemObjStringAppend(&sKey,"data",sizeof("data")-1);` |
|    75 | 5259 | `	ph7_array_add_elem(pOut,&sKey,pData);` |
|    75 | 5260 | `	PH7_MemObjRelease(&sKey);` |
|    75 | 5261 | `	PH7_MemObjInitFromString(pVm,&sKey,0);` |
|    75 | 5262 | `	PH7_MemObjStringAppend(&sKey,"priority",sizeof("priority")-1);` |
|    75 | 5263 | `	ph7_array_add_elem(pOut,&sKey,pPrio);` |
|    75 | 5264 | `	PH7_MemObjRelease(&sKey);` |
|    75 | 5265 | `	return SXRET_OK;` |
|    38 | 5266 | `}` |
|     - | 5267 | `/*` |
|     - | 5268 | ` * Run the user's compare(). For a queue php compares the PRIORITIES, so the node's` |
|     - | 5269 | `` * `priority` is what is handed over. A throw here is php's corruption trigger: the`` |
|     - | 5270 | ` * bit is set, and the throw still propagates.` |
|     - | 5271 | ` */` |
|   200 | 5272 | `static sxi32 HeapCompare(ph7_context *pCtx,sxi64 iA,sxi64 iB,int *piCmp)` |
|     1 | 5273 | `{` |
|   201 | 5274 | `	ph7_vm *pVm = pCtx->pVm;` |
|   201 | 5275 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 5276 | `	ph7_class_method *pMethod;` |
|     - | 5277 | `	ph7_value sA,sB,sRes,*apArg[2],*pV;` |
|   201 | 5278 | `	int bPq = HeapIsPq(pThis);` |
|     - | 5279 | `	sxi32 rc;` |
|   201 | 5280 | `	*piCmp = 0;` |
|   201 | 5281 | `	pMethod = pThis ? PH7_ClassExtractMethod(pThis->pClass,"compare",sizeof("compare")-1) : 0;` |
|   201 | 5282 | `	if( pMethod == 0 ){` |
|   ! 0 | 5283 | `		return SXRET_OK;` |
|     - | 5284 | `	}` |
|   201 | 5285 | `	PH7_MemObjInit(pVm,&sA);` |
|   201 | 5286 | `	PH7_MemObjInit(pVm,&sB);` |
|   201 | 5287 | `	pV = HeapAt(pVm,pThis,iA);` |
|   201 | 5288 | `	if( bPq ){` |
|    59 | 5289 | `		pV = HeapNodePart(pV,"priority");` |
|    29 | 5290 | `	}` |
|   201 | 5291 | `	if( pV ){` |
|   201 | 5292 | `		PH7_MemObjStore(pV,&sA);` |
|   100 | 5293 | `	}` |
|   201 | 5294 | `	pV = HeapAt(pVm,pThis,iB);` |
|   201 | 5295 | `	if( bPq ){` |
|    59 | 5296 | `		pV = HeapNodePart(pV,"priority");` |
|    29 | 5297 | `	}` |
|   201 | 5298 | `	if( pV ){` |
|   201 | 5299 | `		PH7_MemObjStore(pV,&sB);` |
|   100 | 5300 | `	}` |
|   201 | 5301 | `	apArg[0] = &sA;` |
|   201 | 5302 | `	apArg[1] = &sB;` |
|   201 | 5303 | `	PH7_MemObjInit(pVm,&sRes);` |
|     - | 5304 | `	/* php dispatches through its cached fptr_cmp and never consults visibility --` |
|     - | 5305 | `	 * SplHeap::compare() is PROTECTED and is meant to be called by the heap. */` |
|   201 | 5306 | `	rc = PH7_VmCallMethodUnchecked(pVm,pThis,pMethod,&sRes,2,apArg);` |
|   201 | 5307 | `	if( rc == SXRET_OK ){` |
|   171 | 5308 | `		sxi64 iVal = ph7_value_to_int64(&sRes);` |
|   171 | 5309 | `		*piCmp = iVal < 0 ? -1 : (iVal > 0 ? 1 : 0);` |
|    86 | 5310 | `	}else{` |
|     - | 5311 | `		/* php finishes the sift with the exception in flight and marks the heap` |
|     - | 5312 | `		 * CORRUPTED afterwards; the element it was placing still lands. */` |
|    31 | 5313 | `		PH7_NativeSetAttrInt(pVm,pThis,HP_CR,1);` |
|     - | 5314 | `	}` |
|   201 | 5315 | `	PH7_MemObjRelease(&sA);` |
|   201 | 5316 | `	PH7_MemObjRelease(&sB);` |
|   201 | 5317 | `	PH7_MemObjRelease(&sRes);` |
|   201 | 5318 | `	return rc;` |
|   101 | 5319 | `}` |
|   218 | 5320 | `static sxi32 HeapSiftUp(ph7_context *pCtx,sxi64 i)` |
|     1 | 5321 | `{` |
|   219 | 5322 | `	ph7_vm *pVm = pCtx->pVm;` |
|   219 | 5323 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   287 | 5324 | `	while( i > 0 ){` |
|   147 | 5325 | `		sxi64 p = (i - 1) / 2;` |
|   147 | 5326 | `		int iCmp = 0;` |
|   147 | 5327 | `		sxi32 rc = HeapCompare(pCtx,i,p,&iCmp);` |
|   147 | 5328 | `		if( rc != SXRET_OK ){` |
|    31 | 5329 | `			return rc;` |
|     - | 5330 | `		}` |
|   117 | 5331 | `		if( iCmp <= 0 ){` |
|    49 | 5332 | `			break;` |
|     - | 5333 | `		}` |
|    69 | 5334 | `		HeapSwap(pVm,pThis,i,p);` |
|    69 | 5335 | `		i = p;` |
|     1 | 5336 | `	}` |
|   189 | 5337 | `	return SXRET_OK;` |
|   110 | 5338 | `}` |
|    60 | 5339 | `static sxi32 HeapSiftDown(ph7_context *pCtx,sxi64 i)` |
|     1 | 5340 | `{` |
|    61 | 5341 | `	ph7_vm *pVm = pCtx->pVm;` |
|    61 | 5342 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    42 | 5343 | `	for(;;){` |
|    73 | 5344 | `		sxi64 n = HeapCount(pVm,pThis);` |
|    73 | 5345 | `		sxi64 l = 2*i + 1, r = l + 1, b = i;` |
|    73 | 5346 | `		int iCmp = 0;` |
|     - | 5347 | `		sxi32 rc;` |
|    73 | 5348 | `		if( l < n ){` |
|    43 | 5349 | `			rc = HeapCompare(pCtx,l,b,&iCmp);` |
|    43 | 5350 | `			if( rc != SXRET_OK ){` |
|   ! 0 | 5351 | `				return rc;` |
|     - | 5352 | `			}` |
|    43 | 5353 | `			if( iCmp > 0 ){` |
|    13 | 5354 | `				b = l;` |
|     6 | 5355 | `			}` |
|    21 | 5356 | `		}` |
|    73 | 5357 | `		if( r < n ){` |
|    13 | 5358 | `			rc = HeapCompare(pCtx,r,b,&iCmp);` |
|    13 | 5359 | `			if( rc != SXRET_OK ){` |
|   ! 0 | 5360 | `				return rc;` |
|     - | 5361 | `			}` |
|    13 | 5362 | `			if( iCmp > 0 ){` |
|     3 | 5363 | `				b = r;` |
|     1 | 5364 | `			}` |
|     6 | 5365 | `		}` |
|    73 | 5366 | `		if( b == i ){` |
|    61 | 5367 | `			break;` |
|     - | 5368 | `		}` |
|    13 | 5369 | `		HeapSwap(pVm,pThis,i,b);` |
|    13 | 5370 | `		i = b;` |
|     1 | 5371 | `	}` |
|    61 | 5372 | `	return SXRET_OK;` |
|    31 | 5373 | `}` |
|     - | 5374 | `/* php's spl_pqueue_extract_helper: BOTH wins over either single bit. */` |
|    46 | 5375 | `static void HeapPqShape(ph7_context *pCtx,ph7_value *pNode)` |
|     1 | 5376 | `{` |
|    47 | 5377 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    47 | 5378 | `	int iFlags = pThis ? (int)PH7_NativeAttrInt(pThis,HP_FL) : PQ_EXTR_DATA;` |
|     - | 5379 | `	ph7_value *pPart;` |
|    47 | 5380 | `	if( (iFlags & PQ_EXTR_BOTH) == PQ_EXTR_BOTH ){` |
|     7 | 5381 | `		ph7_result_value(pCtx,pNode);` |
|     7 | 5382 | `		return;` |
|     - | 5383 | `	}` |
|    41 | 5384 | `	pPart = HeapNodePart(pNode,(iFlags & PQ_EXTR_DATA) ? "data" : "priority");` |
|    41 | 5385 | `	if( pPart ){` |
|    41 | 5386 | `		ph7_result_value(pCtx,pPart);` |
|    21 | 5387 | `	}else{` |
|   ! 0 | 5388 | `		ph7_result_null(pCtx);` |
|     - | 5389 | `	}` |
|    24 | 5390 | `}` |
|     - | 5391 | `/* Hand back element 0 the way this class presents it. */` |
|   126 | 5392 | `static void HeapResultTop(ph7_context *pCtx,ph7_value *pNode)` |
|     1 | 5393 | `{` |
|   127 | 5394 | `	if( HeapIsPq(PH7_ContextThis(pCtx)) ){` |
|    47 | 5395 | `		HeapPqShape(pCtx,pNode);` |
|    24 | 5396 | `	}else{` |
|    81 | 5397 | `		ph7_result_value(pCtx,pNode);` |
|     - | 5398 | `	}` |
|   127 | 5399 | `}` |
|   220 | 5400 | `static int vm_builtin_SplHeap_insert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 5401 | `{` |
|   221 | 5402 | `	ph7_vm *pVm = pCtx->pVm;` |
|   221 | 5403 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 5404 | `	ph7_hashmap *pMap;` |
|   221 | 5405 | `	sxi32 rc = HeapCheck(pCtx);` |
|   221 | 5406 | `	if( rc != SXRET_OK \|\| nArg < 1 ){` |
|     3 | 5407 | `		return rc;` |
|     - | 5408 | `	}` |
|   219 | 5409 | `	pMap = HeapMap(pVm,pThis);` |
|   219 | 5410 | `	if( pMap == 0 ){` |
|   ! 0 | 5411 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 5412 | `	}` |
|   219 | 5413 | `	if( HeapIsPq(pThis) ){` |
|     - | 5414 | `		ph7_value sNode;` |
|    75 | 5415 | `		if( nArg < 2 ){` |
|   ! 0 | 5416 | `			return PH7_OK;` |
|     - | 5417 | `		}` |
|    75 | 5418 | `		PH7_MemObjInit(pVm,&sNode);` |
|    75 | 5419 | `		if( HeapMakeNode(pVm,apArg[0],apArg[1],&sNode) != SXRET_OK ){` |
|   ! 0 | 5420 | `			PH7_MemObjRelease(&sNode);` |
|   ! 0 | 5421 | `			return PH7_ContextMemoryError(pCtx);` |
|     - | 5422 | `		}` |
|    75 | 5423 | `		PH7_HashmapInsert(pMap,0,&sNode);` |
|    75 | 5424 | `		PH7_MemObjRelease(&sNode);` |
|    38 | 5425 | `	}else{` |
|   145 | 5426 | `		PH7_HashmapInsert(pMap,0,apArg[0]);` |
|     - | 5427 | `	}` |
|   219 | 5428 | `	rc = HeapSiftUp(pCtx,HeapCount(pVm,pThis)-1);` |
|   219 | 5429 | `	if( rc != SXRET_OK ){` |
|    31 | 5430 | `		return rc;` |
|     - | 5431 | `	}` |
|   189 | 5432 | ``	ph7_result_bool(pCtx,1);   /* php's `true` return type */`` |
|   189 | 5433 | `	return PH7_OK;` |
|   111 | 5434 | `}` |
|    90 | 5435 | `static int vm_builtin_SplHeap_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 5436 | `{` |
|    91 | 5437 | `	ph7_vm *pVm = pCtx->pVm;` |
|    91 | 5438 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 5439 | `	sxi64 n;` |
|     - | 5440 | `	ph7_value sTop,*pV;` |
|    91 | 5441 | `	sxi32 rc = HeapCheck(pCtx);` |
|    45 | 5442 | `	SXUNUSED(nArg);` |
|    45 | 5443 | `	SXUNUSED(apArg);` |
|    91 | 5444 | `	if( rc != SXRET_OK ){` |
|     3 | 5445 | `		return rc;` |
|     - | 5446 | `	}` |
|    89 | 5447 | `	n = HeapCount(pVm,pThis);` |
|    89 | 5448 | `	if( n == 0 ){` |
|     5 | 5449 | `		return PH7_VmThrowException(pCtx,"RuntimeException","Can't extract from an empty heap");` |
|     - | 5450 | `	}` |
|    85 | 5451 | `	PH7_MemObjInit(pVm,&sTop);` |
|    85 | 5452 | `	pV = HeapAt(pVm,pThis,0);` |
|    85 | 5453 | `	if( pV ){` |
|    85 | 5454 | `		PH7_MemObjStore(pV,&sTop);` |
|    42 | 5455 | `	}` |
|    85 | 5456 | `	if( n > 1 ){` |
|     - | 5457 | `		ph7_value sLast;` |
|    61 | 5458 | `		PH7_MemObjInit(pVm,&sLast);` |
|    61 | 5459 | `		pV = HeapAt(pVm,pThis,n-1);` |
|    61 | 5460 | `		if( pV ){` |
|    61 | 5461 | `			PH7_MemObjStore(pV,&sLast);` |
|    30 | 5462 | `		}` |
|    61 | 5463 | `		HeapPut(pVm,pThis,0,&sLast);` |
|    61 | 5464 | `		PH7_MemObjRelease(&sLast);` |
|    30 | 5465 | `	}` |
|     - | 5466 | `	{` |
|    85 | 5467 | `		ph7_hashmap *pMap = HeapMap(pVm,pThis);` |
|    85 | 5468 | `		ph7_hashmap_node *pNode = 0;` |
|    85 | 5469 | `		if( pMap && HashmapLookupIntKey(pMap,n-1,&pNode) == SXRET_OK ){` |
|    85 | 5470 | `			PH7_HashmapUnlinkNode(pNode,TRUE);` |
|    42 | 5471 | `		}` |
|     - | 5472 | `	}` |
|    85 | 5473 | `	if( n > 1 ){` |
|    61 | 5474 | `		rc = HeapSiftDown(pCtx,0);` |
|    61 | 5475 | `		if( rc != SXRET_OK ){` |
|   ! 0 | 5476 | `			PH7_MemObjRelease(&sTop);` |
|   ! 0 | 5477 | `			return rc;` |
|     - | 5478 | `		}` |
|    30 | 5479 | `	}` |
|    85 | 5480 | `	HeapResultTop(pCtx,&sTop);` |
|    85 | 5481 | `	PH7_MemObjRelease(&sTop);` |
|    85 | 5482 | `	return PH7_OK;` |
|    46 | 5483 | `}` |
|    12 | 5484 | `static int vm_builtin_SplHeap_top(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 5485 | `{` |
|    13 | 5486 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 5487 | `	ph7_value *pV;` |
|    13 | 5488 | `	sxi32 rc = HeapCheck(pCtx);` |
|     6 | 5489 | `	SXUNUSED(nArg);` |
|     6 | 5490 | `	SXUNUSED(apArg);` |
|    13 | 5491 | `	if( rc != SXRET_OK ){` |
|     3 | 5492 | `		return rc;` |
|     - | 5493 | `	}` |
|    11 | 5494 | `	if( HeapCount(pCtx->pVm,pThis) == 0 ){` |
|     3 | 5495 | `		return PH7_VmThrowException(pCtx,"RuntimeException","Can't peek at an empty heap");` |
|     - | 5496 | `	}` |
|     9 | 5497 | `	pV = HeapAt(pCtx->pVm,pThis,0);` |
|     9 | 5498 | `	if( pV ){` |
|     9 | 5499 | `		HeapResultTop(pCtx,pV);` |
|     4 | 5500 | `	}` |
|     9 | 5501 | `	return PH7_OK;` |
|     7 | 5502 | `}` |
|    22 | 5503 | `static int vm_builtin_SplHeap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 5504 | `{` |
|    11 | 5505 | `	SXUNUSED(nArg);` |
|    11 | 5506 | `	SXUNUSED(apArg);` |
|    23 | 5507 | `	ph7_result_int64(pCtx,HeapCount(pCtx->pVm,PH7_ContextThis(pCtx)));` |
|    23 | 5508 | `	return PH7_OK;` |
|     1 | 5509 | `}` |
|    50 | 5510 | `static int vm_builtin_SplHeap_isEmpty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 5511 | `{` |
|    25 | 5512 | `	SXUNUSED(nArg);` |
|    25 | 5513 | `	SXUNUSED(apArg);` |
|    51 | 5514 | `	ph7_result_bool(pCtx,HeapCount(pCtx->pVm,PH7_ContextThis(pCtx)) == 0);` |
|    51 | 5515 | `	return PH7_OK;` |
|     1 | 5516 | `}` |
|    12 | 5517 | `static int vm_builtin_SplHeap_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 5518 | `{` |
|     6 | 5519 | `	SXUNUSED(nArg);` |
|     6 | 5520 | `	SXUNUSED(apArg);` |
|     6 | 5521 | `	SXUNUSED(pCtx);` |
|    13 | 5522 | `	return PH7_OK;   /* php's rewind is a no-op: a heap is walked by extraction */` |
|     1 | 5523 | `}` |
|    44 | 5524 | `static int vm_builtin_SplHeap_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 5525 | `{` |
|    22 | 5526 | `	SXUNUSED(nArg);` |
|    22 | 5527 | `	SXUNUSED(apArg);` |
|    45 | 5528 | `	ph7_result_bool(pCtx,HeapCount(pCtx->pVm,PH7_ContextThis(pCtx)) > 0);` |
|    45 | 5529 | `	return PH7_OK;` |
|     1 | 5530 | `}` |
|    34 | 5531 | `static int vm_builtin_SplHeap_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 5532 | `{` |
|    35 | 5533 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 5534 | `	ph7_value *pV;` |
|    17 | 5535 | `	SXUNUSED(nArg);` |
|    17 | 5536 | `	SXUNUSED(apArg);` |
|    35 | 5537 | `	if( HeapCount(pCtx->pVm,pThis) == 0 ){` |
|   ! 0 | 5538 | `		ph7_result_null(pCtx);` |
|   ! 0 | 5539 | `		return PH7_OK;` |
|     - | 5540 | `	}` |
|    35 | 5541 | `	pV = HeapAt(pCtx->pVm,pThis,0);` |
|    35 | 5542 | `	if( pV ){` |
|    35 | 5543 | `		HeapResultTop(pCtx,pV);` |
|    17 | 5544 | `	}` |
|    35 | 5545 | `	return PH7_OK;` |
|    18 | 5546 | `}` |
|    16 | 5547 | `static int vm_builtin_SplHeap_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 5548 | `{` |
|     8 | 5549 | `	SXUNUSED(nArg);` |
|     8 | 5550 | `	SXUNUSED(apArg);` |
|    17 | 5551 | `	ph7_result_int64(pCtx,HeapCount(pCtx->pVm,PH7_ContextThis(pCtx))-1);` |
|    17 | 5552 | `	return PH7_OK;` |
|     1 | 5553 | `}` |
|    34 | 5554 | `static int vm_builtin_SplHeap_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 5555 | `{` |
|    35 | 5556 | `	sxi32 rc = HeapCheck(pCtx);` |
|    35 | 5557 | `	if( rc != SXRET_OK ){` |
|     5 | 5558 | `		return rc;` |
|     - | 5559 | `	}` |
|    31 | 5560 | `	if( HeapCount(pCtx->pVm,PH7_ContextThis(pCtx)) == 0 ){` |
|   ! 0 | 5561 | `		return PH7_OK;` |
|     - | 5562 | `	}` |
|    31 | 5563 | `	rc = vm_builtin_SplHeap_extract(pCtx,nArg,apArg);` |
|    31 | 5564 | `	ph7_result_null(pCtx);   /* php's next() is void; the extracted value is dropped */` |
|    31 | 5565 | `	return rc;` |
|    18 | 5566 | `}` |
|     6 | 5567 | `static int vm_builtin_SplHeap_isCorrupted(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 5568 | `{` |
|     3 | 5569 | `	SXUNUSED(nArg);` |
|     3 | 5570 | `	SXUNUSED(apArg);` |
|     7 | 5571 | `	ph7_result_bool(pCtx,HeapCorrupted(PH7_ContextThis(pCtx)));` |
|     7 | 5572 | `	return PH7_OK;` |
|     1 | 5573 | `}` |
|     4 | 5574 | `static int vm_builtin_SplHeap_recover(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 5575 | `{` |
|     2 | 5576 | `	SXUNUSED(nArg);` |
|     2 | 5577 | `	SXUNUSED(apArg);` |
|     5 | 5578 | `	PH7_NativeSetAttrInt(pCtx->pVm,PH7_ContextThis(pCtx),HP_CR,0);` |
|     5 | 5579 | ``	ph7_result_bool(pCtx,1);   /* php's `true` return type */`` |
|     5 | 5580 | `	return PH7_OK;` |
|     1 | 5581 | `}` |
|    30 | 5582 | `static int vm_builtin_SplMinHeap_compare(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 5583 | `{` |
|     - | 5584 | `	/* php: $value2 <=> $value1 — the SMALLEST value sits on top. */` |
|    31 | 5585 | `	if( nArg < 2 ){` |
|   ! 0 | 5586 | `		return PH7_OK;` |
|     - | 5587 | `	}` |
|    31 | 5588 | `	ph7_result_int64(pCtx,(ph7_int64)PH7_MemObjCmp(apArg[1],apArg[0],FALSE,0));` |
|    31 | 5589 | `	return PH7_OK;` |
|    16 | 5590 | `}` |
|    48 | 5591 | `static int vm_builtin_SplMaxHeap_compare(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 5592 | `{` |
|    49 | 5593 | `	if( nArg < 2 ){` |
|   ! 0 | 5594 | `		return PH7_OK;` |
|     - | 5595 | `	}` |
|    49 | 5596 | `	ph7_result_int64(pCtx,(ph7_int64)PH7_MemObjCmp(apArg[0],apArg[1],FALSE,0));` |
|    49 | 5597 | `	return PH7_OK;` |
|    25 | 5598 | `}` |
|    58 | 5599 | `static int vm_builtin_SplPq_compare(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 5600 | `{` |
|    59 | 5601 | `	if( nArg < 2 ){` |
|   ! 0 | 5602 | `		return PH7_OK;` |
|     - | 5603 | `	}` |
|    59 | 5604 | `	ph7_result_int64(pCtx,(ph7_int64)PH7_MemObjCmp(apArg[0],apArg[1],FALSE,0));` |
|    59 | 5605 | `	return PH7_OK;` |
|    30 | 5606 | `}` |
|    14 | 5607 | `static int vm_builtin_SplPq_setExtractFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 5608 | `{` |
|    15 | 5609 | `	ph7_vm *pVm = pCtx->pVm;` |
|    15 | 5610 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    15 | 5611 | `	sxi64 iFlags = 0;` |
|     - | 5612 | `	sxi32 rc;` |
|    15 | 5613 | `	if( nArg < 1 ){` |
|   ! 0 | 5614 | `		return PH7_OK;` |
|     - | 5615 | `	}` |
|    15 | 5616 | `	rc = PH7_IntArgResolve(pCtx,apArg[0],"SplPriorityQueue::setExtractFlags",1,"$flags","int",&iFlags);` |
|    15 | 5617 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 5618 | `		return rc;` |
|     - | 5619 | `	}` |
|     - | 5620 | `	/* php masks to the two bits and then REFUSES an empty selection — a nonsense` |
|     - | 5621 | `	 * value is reduced, but asking for neither half is an error. */` |
|    15 | 5622 | `	iFlags &= PQ_EXTR_MASK;` |
|    15 | 5623 | `	if( iFlags == 0 ){` |
|     3 | 5624 | `		return PH7_VmThrowException(pCtx,"RuntimeException","Must specify at least one extract flag");` |
|     - | 5625 | `	}` |
|    13 | 5626 | `	PH7_NativeSetAttrInt(pVm,pThis,HP_FL,iFlags);` |
|    13 | 5627 | `	ph7_result_int64(pCtx,(ph7_int64)iFlags);` |
|    13 | 5628 | `	return PH7_OK;` |
|     8 | 5629 | `}` |
|     6 | 5630 | `static int vm_builtin_SplPq_getExtractFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 5631 | `{` |
|     3 | 5632 | `	SXUNUSED(nArg);` |
|     3 | 5633 | `	SXUNUSED(apArg);` |
|     7 | 5634 | `	ph7_result_int64(pCtx,PH7_NativeAttrInt(PH7_ContextThis(pCtx),HP_FL));` |
|     7 | 5635 | `	return PH7_OK;` |
|     1 | 5636 | `}` |
|     - | 5637 | `/*` |
|     - | 5638 | ` * php's get_debug_info: flags, isCorrupted, heap. The QUEUE renders its entries` |
|     - | 5639 | ` * EXTR_BOTH-style whatever the extract flags say, because the debug view is of the` |
|     - | 5640 | ` * STORAGE rather than of what extract() would hand back.` |
|     - | 5641 | ` */` |
|     2 | 5642 | `static sxi32 HeapFillDebug(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut)` |
|     1 | 5643 | `{` |
|     - | 5644 | `	ph7_value sKey,sVal,*pStore;` |
|     3 | 5645 | `	PH7_MemObjInitFromString(pVm,&sKey,0);` |
|     3 | 5646 | `	PH7_MemObjStringAppend(&sKey,"flags",sizeof("flags")-1);` |
|     3 | 5647 | `	PH7_MemObjInitFromInt(pVm,&sVal,PH7_NativeAttrInt(pThis,HP_FL));` |
|     3 | 5648 | `	ph7_array_add_elem(pOut,&sKey,&sVal);` |
|     3 | 5649 | `	PH7_MemObjRelease(&sKey);` |
|     3 | 5650 | `	PH7_MemObjRelease(&sVal);` |
|     3 | 5651 | `	PH7_MemObjInitFromString(pVm,&sKey,0);` |
|     3 | 5652 | `	PH7_MemObjStringAppend(&sKey,"isCorrupted",sizeof("isCorrupted")-1);` |
|     3 | 5653 | `	PH7_MemObjInitFromBool(pVm,&sVal,HeapCorrupted(pThis));` |
|     3 | 5654 | `	ph7_array_add_elem(pOut,&sKey,&sVal);` |
|     3 | 5655 | `	PH7_MemObjRelease(&sKey);` |
|     3 | 5656 | `	PH7_MemObjRelease(&sVal);` |
|     3 | 5657 | `	PH7_MemObjInitFromString(pVm,&sKey,0);` |
|     3 | 5658 | `	PH7_MemObjStringAppend(&sKey,"heap",sizeof("heap")-1);` |
|     3 | 5659 | `	pStore = HeapSlot(pVm,pThis);` |
|     3 | 5660 | `	if( pStore ){` |
|     3 | 5661 | `		ph7_array_add_elem(pOut,&sKey,pStore);` |
|     1 | 5662 | `	}` |
|     3 | 5663 | `	PH7_MemObjRelease(&sKey);` |
|     3 | 5664 | `	return PH7_OK;` |
|     1 | 5665 | `}` |
|     2 | 5666 | `static sxi32 HeapPresent(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)` |
|     1 | 5667 | `{` |
|     3 | 5668 | `	if( !bDebug ){` |
|     3 | 5669 | `		return PH7_OK;   /* php's (array) cast shows nothing */` |
|     - | 5670 | `	}` |
|   ! 0 | 5671 | `	return HeapFillDebug(pVm,pThis,pOut);` |
|     2 | 5672 | `}` |
|     2 | 5673 | `static int vm_builtin_SplHeap_debugInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 5674 | `{` |
|     3 | 5675 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 5676 | `	ph7_value sOut;` |
|     1 | 5677 | `	SXUNUSED(nArg);` |
|     1 | 5678 | `	SXUNUSED(apArg);` |
|     3 | 5679 | `	PH7_MemObjInit(pVm,&sOut);` |
|     3 | 5680 | `	if( PH7_MemObjToHashmap(&sOut) != SXRET_OK ){` |
|   ! 0 | 5681 | `		PH7_MemObjRelease(&sOut);` |
|   ! 0 | 5682 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 5683 | `	}` |
|     3 | 5684 | `	HeapFillDebug(pVm,PH7_ContextThis(pCtx),&sOut);` |
|     3 | 5685 | `	ph7_result_value(pCtx,&sOut);` |
|     3 | 5686 | `	PH7_MemObjRelease(&sOut);` |
|     3 | 5687 | `	return PH7_OK;` |
|     2 | 5688 | `}` |
|     - | 5689 | `/*` |
|     - | 5690 | ` * php's __serialize(): [members, {flags, heap_elements}]. Note the OUTER array is` |
|     - | 5691 | ` * a two-element list whose first entry is the instance's own property table —` |
|     - | 5692 | ` * empty for a bare heap, a SUBCLASS's declared slots when there is one.` |
|     - | 5693 | ` */` |
|    20 | 5694 | `static int vm_builtin_SplHeap_serializeMagic(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 5695 | `{` |
|    21 | 5696 | `	ph7_vm *pVm = pCtx->pVm;` |
|    21 | 5697 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 5698 | `	ph7_value sOut,sMembers,sState,sKey,sVal,*pStore;` |
|    21 | 5699 | `	sxi32 rc = HeapCheck(pCtx);` |
|    10 | 5700 | `	SXUNUSED(nArg);` |
|    10 | 5701 | `	SXUNUSED(apArg);` |
|    21 | 5702 | `	if( rc != SXRET_OK ){` |
|     3 | 5703 | `		return rc;` |
|     - | 5704 | `	}` |
|    19 | 5705 | `	PH7_MemObjInit(pVm,&sOut);` |
|    19 | 5706 | `	PH7_MemObjInit(pVm,&sState);` |
|    18 | 5707 | `	if( SplMembersOf(pVm,pThis,&sMembers) != SXRET_OK` |
|    18 | 5708 | `	 \|\| PH7_MemObjToHashmap(&sOut) != SXRET_OK` |
|    19 | 5709 | `	 \|\| PH7_MemObjToHashmap(&sState) != SXRET_OK ){` |
|   ! 0 | 5710 | `		PH7_MemObjRelease(&sOut);` |
|   ! 0 | 5711 | `		PH7_MemObjRelease(&sMembers);` |
|   ! 0 | 5712 | `		PH7_MemObjRelease(&sState);` |
|   ! 0 | 5713 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 5714 | `	}` |
|    19 | 5715 | `	PH7_MemObjInitFromString(pVm,&sKey,0);` |
|    19 | 5716 | `	PH7_MemObjStringAppend(&sKey,"flags",sizeof("flags")-1);` |
|    19 | 5717 | `	PH7_MemObjInitFromInt(pVm,&sVal,PH7_NativeAttrInt(pThis,HP_FL));` |
|    19 | 5718 | `	ph7_array_add_elem(&sState,&sKey,&sVal);` |
|    19 | 5719 | `	PH7_MemObjRelease(&sKey);` |
|    19 | 5720 | `	PH7_MemObjRelease(&sVal);` |
|    19 | 5721 | `	PH7_MemObjInitFromString(pVm,&sKey,0);` |
|    19 | 5722 | `	PH7_MemObjStringAppend(&sKey,"heap_elements",sizeof("heap_elements")-1);` |
|    19 | 5723 | `	pStore = HeapSlot(pVm,pThis);` |
|    19 | 5724 | `	if( pStore ){` |
|    19 | 5725 | `		ph7_array_add_elem(&sState,&sKey,pStore);` |
|     9 | 5726 | `	}` |
|    19 | 5727 | `	PH7_MemObjRelease(&sKey);` |
|    19 | 5728 | `	ph7_array_add_elem(&sOut,0,&sMembers);` |
|    19 | 5729 | `	ph7_array_add_elem(&sOut,0,&sState);` |
|    19 | 5730 | `	ph7_result_value(pCtx,&sOut);` |
|    19 | 5731 | `	PH7_MemObjRelease(&sOut);` |
|    19 | 5732 | `	PH7_MemObjRelease(&sMembers);` |
|    19 | 5733 | `	PH7_MemObjRelease(&sState);` |
|    19 | 5734 | `	return PH7_OK;` |
|    11 | 5735 | `}` |
|   ! 0 | 5736 | `static sxi32 HeapUnserializeFail(ph7_context *pCtx)` |
|   ! 0 | 5737 | `{` |
|   ! 0 | 5738 | `	return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     - | 5739 | `		"Unexpected data found in serialization payload");` |
|   ! 0 | 5740 | `}` |
|     6 | 5741 | `static int vm_builtin_SplHeap_unserializeMagic(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 5742 | `{` |
|     7 | 5743 | `	ph7_vm *pVm = pCtx->pVm;` |
|     7 | 5744 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 5745 | `	ph7_hashmap *pData;` |
|     7 | 5746 | `	ph7_hashmap_node *pNode = 0;` |
|     - | 5747 | `	ph7_value *pState,*pFlags,*pElems,*pSlot;` |
|     - | 5748 | `	sxi64 iFlags;` |
|     - | 5749 | `	int bPq;` |
|     7 | 5750 | `	sxi32 rc = HeapCheck(pCtx);` |
|     7 | 5751 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 5752 | `		return rc;` |
|     - | 5753 | `	}` |
|     7 | 5754 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_HASHMAP) == 0 \|\| pThis == 0 ){` |
|   ! 0 | 5755 | `		return HeapUnserializeFail(pCtx);` |
|     - | 5756 | `	}` |
|     7 | 5757 | `	pData = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     7 | 5758 | `	if( HashmapLookupIntKey(pData,1,&pNode) != SXRET_OK ){` |
|   ! 0 | 5759 | `		return HeapUnserializeFail(pCtx);` |
|     - | 5760 | `	}` |
|     7 | 5761 | `	pState = HashmapExtractNodeValue(pNode);` |
|     7 | 5762 | `	if( pState == 0 \|\| (pState->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|   ! 0 | 5763 | `		return HeapUnserializeFail(pCtx);` |
|     - | 5764 | `	}` |
|     7 | 5765 | `	pFlags = HeapNodePart(pState,"flags");` |
|     7 | 5766 | `	pElems = HeapNodePart(pState,"heap_elements");` |
|     6 | 5767 | `	if( pFlags == 0 \|\| (pFlags->iFlags & MEMOBJ_INT) == 0` |
|     7 | 5768 | `	 \|\| pElems == 0 \|\| (pElems->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|   ! 0 | 5769 | `		return HeapUnserializeFail(pCtx);` |
|     - | 5770 | `	}` |
|     - | 5771 | `	/* php VALIDATES the flags against the class: a plain heap has no user-visible` |
|     - | 5772 | `	 * flags at all, the queue must name at least one half to extract. */` |
|     7 | 5773 | `	iFlags = ph7_value_to_int64(pFlags);` |
|     7 | 5774 | `	bPq = HeapIsPq(pThis);` |
|     7 | 5775 | `	if( bPq ){` |
|     5 | 5776 | `		iFlags &= PQ_EXTR_MASK;` |
|     5 | 5777 | `		if( iFlags == 0 ){` |
|   ! 0 | 5778 | `			return HeapUnserializeFail(pCtx);` |
|     1 | 5779 | `		}` |
|     5 | 5780 | `	}else if( iFlags != 0 ){` |
|   ! 0 | 5781 | `		return HeapUnserializeFail(pCtx);` |
|     - | 5782 | `	}` |
|     7 | 5783 | `	PH7_NativeSetAttrInt(pVm,pThis,HP_FL,iFlags);` |
|     7 | 5784 | `	pSlot = PH7_NativeAttr(pThis,HP_H);` |
|     7 | 5785 | `	if( pSlot ){` |
|     7 | 5786 | `		PH7_MemObjRelease(pSlot);` |
|     7 | 5787 | `		PH7_MemObjStore(pElems,pSlot);` |
|     3 | 5788 | `	}` |
|     7 | 5789 | `	if( HashmapLookupIntKey(pData,0,&pNode) == SXRET_OK ){` |
|     7 | 5790 | `		SplMembersLoad(pThis,HashmapExtractNodeValue(pNode));` |
|     3 | 5791 | `	}` |
|     7 | 5792 | `	return PH7_OK;` |
|     4 | 5793 | `}` |
|     - | 5794 | `/*` |
|     - | 5795 | ` * The declaration. Method ORDER is spl_heap.stub.php's; php declares no` |
|     - | 5796 | ` * constructor for any of the four, SplHeap::compare is ABSTRACT PROTECTED (so` |
|     - | 5797 | ` * SplHeap itself cannot be instantiated) while the queue's is PUBLIC, and the` |
|     - | 5798 | ` * queue's default extract mode is EXTR_DATA, stamped as a property default the` |
|     - | 5799 | ` * way the DLL family's fix bit is.` |
|     - | 5800 | ` */` |
|  5146 | 5801 | `static sxi32 VmInstallSplHeap(ph7_vm *pVm)` |
|     5 | 5802 | `{` |
|     - | 5803 | `	static const PH7_NativePropDef aHeapProp[] = {` |
|     - | 5804 | `		{ HP_H,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - | 5805 | `		{ HP_FL, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - | 5806 | `		{ HP_CR, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - | 5807 | `	};` |
|     - | 5808 | `	static const PH7_NativePropDef aPqProp[] = {` |
|     - | 5809 | `		{ HP_H,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - | 5810 | `		{ HP_FL, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN,` |
|     - | 5811 | `		  { 0, 0, PH7_NATIVE_VAL_INT, PQ_EXTR_DATA, 0, 0.0 }, 0 },` |
|     - | 5812 | `		{ HP_CR, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - | 5813 | `	};` |
|     - | 5814 | `	static const PH7_NativeMethodDef aHeapMethod[] = {` |
|     - | 5815 | `		{ "extract",               PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplHeap_extract },` |
|     - | 5816 | `		{ "insert",                PH7_MOD_PUBLIC, "mixed $value", "@true",` |
|     - | 5817 | `		  vm_builtin_SplHeap_insert },` |
|     - | 5818 | `		{ "top",                   PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplHeap_top },` |
|     - | 5819 | `		{ "count",                 PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplHeap_count },` |
|     - | 5820 | `		{ "isEmpty",               PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplHeap_isEmpty },` |
|     - | 5821 | `		{ "rewind",                PH7_MOD_PUBLIC, "", "@void", vm_builtin_SplHeap_rewind },` |
|     - | 5822 | `		{ "current",               PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplHeap_current },` |
|     - | 5823 | `		{ "key",                   PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplHeap_key },` |
|     - | 5824 | `		{ "next",                  PH7_MOD_PUBLIC, "", "@void", vm_builtin_SplHeap_next },` |
|     - | 5825 | `		{ "valid",                 PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplHeap_valid },` |
|     - | 5826 | `		{ "recoverFromCorruption", PH7_MOD_PUBLIC, "", "@true", vm_builtin_SplHeap_recover },` |
|     - | 5827 | `		{ "compare",               PH7_MOD_PROTECTED\|PH7_MOD_ABSTRACT,` |
|     - | 5828 | `		  "mixed $value1, mixed $value2", "@int", 0 },` |
|     - | 5829 | `		{ "isCorrupted",           PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplHeap_isCorrupted },` |
|     - | 5830 | `		{ "__debugInfo",           PH7_MOD_PUBLIC, "", "@array", vm_builtin_SplHeap_debugInfo },` |
|     - | 5831 | `		{ "__serialize",           PH7_MOD_PUBLIC, "", "@array",` |
|     - | 5832 | `		  vm_builtin_SplHeap_serializeMagic },` |
|     - | 5833 | `		{ "__unserialize",         PH7_MOD_PUBLIC, "array $data", "@void",` |
|     - | 5834 | `		  vm_builtin_SplHeap_unserializeMagic },` |
|     - | 5835 | `	};` |
|     - | 5836 | `	static const PH7_NativeMethodDef aMinMethod[] = {` |
|     - | 5837 | `		{ "compare", PH7_MOD_PROTECTED, "mixed $value1, mixed $value2", "@int",` |
|     - | 5838 | `		  vm_builtin_SplMinHeap_compare },` |
|     - | 5839 | `	};` |
|     - | 5840 | `	static const PH7_NativeMethodDef aMaxMethod[] = {` |
|     - | 5841 | `		{ "compare", PH7_MOD_PROTECTED, "mixed $value1, mixed $value2", "@int",` |
|     - | 5842 | `		  vm_builtin_SplMaxHeap_compare },` |
|     - | 5843 | `	};` |
|     - | 5844 | `	static const PH7_NativeConstDef aPqConst[] = {` |
|     - | 5845 | `		{ "EXTR_BOTH",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PQ_EXTR_BOTH, 0, 0.0 },` |
|     - | 5846 | `		{ "EXTR_PRIORITY", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PQ_EXTR_PRIORITY, 0, 0.0 },` |
|     - | 5847 | `		{ "EXTR_DATA",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PQ_EXTR_DATA, 0, 0.0 },` |
|     - | 5848 | `	};` |
|     - | 5849 | `	static const PH7_NativeMethodDef aPqMethod[] = {` |
|     - | 5850 | `		{ "compare",               PH7_MOD_PUBLIC, "mixed $priority1, mixed $priority2", "@int",` |
|     - | 5851 | `		  vm_builtin_SplPq_compare },` |
|     - | 5852 | `		{ "insert",                PH7_MOD_PUBLIC, "mixed $value, mixed $priority", "@true",` |
|     - | 5853 | `		  vm_builtin_SplHeap_insert },` |
|     - | 5854 | `		{ "setExtractFlags",       PH7_MOD_PUBLIC, "int $flags", "@int",` |
|     - | 5855 | `		  vm_builtin_SplPq_setExtractFlags },` |
|     - | 5856 | `		{ "top",                   PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplHeap_top },` |
|     - | 5857 | `		{ "extract",               PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplHeap_extract },` |
|     - | 5858 | `		{ "count",                 PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplHeap_count },` |
|     - | 5859 | `		{ "isEmpty",               PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplHeap_isEmpty },` |
|     - | 5860 | `		{ "rewind",                PH7_MOD_PUBLIC, "", "@void", vm_builtin_SplHeap_rewind },` |
|     - | 5861 | `		{ "current",               PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplHeap_current },` |
|     - | 5862 | `		{ "key",                   PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplHeap_key },` |
|     - | 5863 | `		{ "next",                  PH7_MOD_PUBLIC, "", "@void", vm_builtin_SplHeap_next },` |
|     - | 5864 | `		{ "valid",                 PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplHeap_valid },` |
|     - | 5865 | `		{ "recoverFromCorruption", PH7_MOD_PUBLIC, "", "@true", vm_builtin_SplHeap_recover },` |
|     - | 5866 | `		{ "isCorrupted",           PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplHeap_isCorrupted },` |
|     - | 5867 | `		{ "getExtractFlags",       PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplPq_getExtractFlags },` |
|     - | 5868 | `		{ "__debugInfo",           PH7_MOD_PUBLIC, "", "@array", vm_builtin_SplHeap_debugInfo },` |
|     - | 5869 | `		{ "__serialize",           PH7_MOD_PUBLIC, "", "@array",` |
|     - | 5870 | `		  vm_builtin_SplHeap_serializeMagic },` |
|     - | 5871 | `		{ "__unserialize",         PH7_MOD_PUBLIC, "array $data", "@void",` |
|     - | 5872 | `		  vm_builtin_SplHeap_unserializeMagic },` |
|     - | 5873 | `	};` |
|     - | 5874 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - | 5875 | `		{ "SplPriorityQueue", 0, "Iterator,Countable", 0,` |
|     - | 5876 | `		  aPqMethod, SX_ARRAYSIZE(aPqMethod),` |
|     - | 5877 | `		  aPqConst, SX_ARRAYSIZE(aPqConst),` |
|     - | 5878 | `		  aPqProp, SX_ARRAYSIZE(aPqProp), 0, 0, HeapPresent },` |
|     - | 5879 | `		{ "SplHeap", 0, "Iterator,Countable", PH7_CLASS_ABSTRACT,` |
|     - | 5880 | `		  aHeapMethod, SX_ARRAYSIZE(aHeapMethod), 0, 0,` |
|     - | 5881 | `		  aHeapProp, SX_ARRAYSIZE(aHeapProp), 0, 0, HeapPresent },` |
|     - | 5882 | `		{ "SplMinHeap", "SplHeap", 0, 0,` |
|     - | 5883 | `		  aMinMethod, SX_ARRAYSIZE(aMinMethod), 0, 0, 0, 0, 0, 0, HeapPresent },` |
|     - | 5884 | `		{ "SplMaxHeap", "SplHeap", 0, 0,` |
|     - | 5885 | `		  aMaxMethod, SX_ARRAYSIZE(aMaxMethod), 0, 0, 0, 0, 0, 0, HeapPresent },` |
|     - | 5886 | `	};` |
|  5151 | 5887 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|     5 | 5888 | `}` |
|     - | 5889 | `/*` |
|     - | 5890 | ` * ---------------------------------------------------------------------------` |
|     - | 5891 | ` * SplFixedArray.` |
|     - | 5892 | ` *` |
|     - | 5893 | `` * php PRESENTS this one as its own elements: `var_dump` shows`` |
|     - | 5894 | `` * `object(SplFixedArray)#1 (3) { [0]=> … }`, the `(array)` cast yields the`` |
|     - | 5895 | `` * elements with their integer keys, and `serialize()` writes them as INTEGER`` |
|     - | 5896 | ``  * property names (`O:13:"SplFixedArray":3:{i:0;…}`) because `__serialize()` `` |
|     - | 5897 | `` * simply hands the element array back. The chunk exposed `__a`/`__n` on all`` |
|     - | 5898 | ` * three surfaces instead.` |
|     - | 5899 | ` *` |
|     - | 5900 | `` * `getIterator()` answers php's **InternalIterator**, not a Generator. The chunk`` |
|     - | 5901 | ` * yielded, which is one class name wrong on a php-visible surface and also the` |
|     - | 5902 | `` * thing rule 5's InternalIterator exists for — `pIterVtab` plus`` |
|     - | 5903 | `` * `PH7_NativeIteratorNew()` is the whole implementation, and it gets php's`` |
|     - | 5904 | ` * independent-cursor behaviour (two getIterator() calls, or nested foreach, walk` |
|     - | 5905 | ` * separately) for free.` |
|     - | 5906 | ` *` |
|     - | 5907 | ` * php's offset rule is its own: an int, a bool and an INTEGER-LIKE string are` |
|     - | 5908 | `` * accepted, everything else is `Cannot access offset of type %s on SplFixedArray`.`` |
|     - | 5909 | ` * The chunk refused bools. A FLOAT offset stays refused here, which is not php's` |
|     - | 5910 | ` * answer (php truncates, with a precision deprecation when it is lossy) but IS` |
|     - | 5911 | `` * PHL's engine-wide one — `$a[1.5]` on a plain array raises the same TypeError,`` |
|     - | 5912 | ` * so the class stays consistent with the engine it lives in rather than uniquely` |
|     - | 5913 | ` * permissive (§10).` |
|     - | 5914 | ` */` |
|     - | 5915 | `#define FA_A "__a"   /* the elements, 0..n-1 */` |
|     - | 5916 | `#define FA_N "__n"   /* php's size */` |
|     - | 5917 |  |
|   764 | 5918 | `static ph7_value * FaSlot(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 | 5919 | `{` |
|   765 | 5920 | `	ph7_value *pSlot = pThis ? PH7_NativeAttr(pThis,FA_A) : 0;` |
|   765 | 5921 | `	if( pSlot == 0 ){` |
|   ! 0 | 5922 | `		return 0;` |
|     - | 5923 | `	}` |
|   765 | 5924 | `	if( (pSlot->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|   107 | 5925 | `		if( PH7_MemObjToHashmap(pSlot) != SXRET_OK ){` |
|   ! 0 | 5926 | `			return 0;` |
|     - | 5927 | `		}` |
|    53 | 5928 | `	}` |
|   765 | 5929 | `	if( PH7_HashmapCowSeparate(pVm,pSlot) == 0 ){` |
|   ! 0 | 5930 | `		return 0;` |
|     - | 5931 | `	}` |
|   765 | 5932 | `	return pSlot;` |
|   383 | 5933 | `}` |
|   718 | 5934 | `static ph7_hashmap * FaMap(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 | 5935 | `{` |
|   719 | 5936 | `	ph7_value *pSlot = FaSlot(pVm,pThis);` |
|   719 | 5937 | `	return pSlot ? (ph7_hashmap *)pSlot->x.pOther : 0;` |
|     1 | 5938 | `}` |
|   452 | 5939 | `static sxi64 FaSize(ph7_class_instance *pThis)` |
|     1 | 5940 | `{` |
|   453 | 5941 | `	return pThis ? PH7_NativeAttrInt(pThis,FA_N) : 0;` |
|     1 | 5942 | `}` |
|    94 | 5943 | `static ph7_value * FaAt(ph7_vm *pVm,ph7_class_instance *pThis,sxi64 i)` |
|     1 | 5944 | `{` |
|    95 | 5945 | `	ph7_hashmap *pMap = FaMap(pVm,pThis);` |
|    95 | 5946 | `	ph7_hashmap_node *pNode = 0;` |
|    95 | 5947 | `	if( pMap == 0 \|\| HashmapLookupIntKey(pMap,i,&pNode) != SXRET_OK ){` |
|   ! 0 | 5948 | `		return 0;` |
|     - | 5949 | `	}` |
|    95 | 5950 | `	return HashmapExtractNodeValue(pNode);` |
|    48 | 5951 | `}` |
|   508 | 5952 | `static void FaPut(ph7_vm *pVm,ph7_class_instance *pThis,sxi64 i,ph7_value *pVal)` |
|     1 | 5953 | `{` |
|   509 | 5954 | `	ph7_hashmap *pMap = FaMap(pVm,pThis);` |
|     - | 5955 | `	ph7_value sKey;` |
|   509 | 5956 | `	if( pMap == 0 ){` |
|   ! 0 | 5957 | `		return;` |
|     - | 5958 | `	}` |
|   509 | 5959 | `	PH7_MemObjInitFromInt(pVm,&sKey,i);` |
|   509 | 5960 | `	PH7_HashmapInsert(pMap,&sKey,pVal);` |
|   509 | 5961 | `	PH7_MemObjRelease(&sKey);` |
|   255 | 5962 | `}` |
|     - | 5963 | `/*` |
|     - | 5964 | ` * php's offset decode. An INTEGER-LIKE string is accepted (php's own` |
|     - | 5965 | `` * `ZEND_HANDLE_NUMERIC_STRING`), a bool is its 0/1, and every other type is named`` |
|     - | 5966 | ` * in the refusal. Returns 0 and leaves a TypeError raised when it cannot decode.` |
|     - | 5967 | ` */` |
|   268 | 5968 | `static int FaOffset(ph7_context *pCtx,ph7_value *pArg,sxi64 *piOut,sxi32 *pRc)` |
|     1 | 5969 | `{` |
|   269 | 5970 | `	*pRc = PH7_OK;` |
|   269 | 5971 | `	if( pArg == 0 ){` |
|   ! 0 | 5972 | `		*piOut = 0;` |
|   ! 0 | 5973 | `		return 1;` |
|     - | 5974 | `	}` |
|   269 | 5975 | `	if( pArg->iFlags & MEMOBJ_INT ){` |
|   249 | 5976 | `		*piOut = pArg->x.iVal;` |
|   249 | 5977 | `		return 1;` |
|     - | 5978 | `	}` |
|    21 | 5979 | `	if( pArg->iFlags & MEMOBJ_BOOL ){` |
|     5 | 5980 | `		*piOut = pArg->x.iVal ? 1 : 0;` |
|     5 | 5981 | `		return 1;` |
|     - | 5982 | `	}` |
|    17 | 5983 | `	if( (pArg->iFlags & MEMOBJ_STRING) && PH7_MemObjStringIsNumeric(pArg) ){` |
|     - | 5984 | `		ph7_value sTmp;` |
|     5 | 5985 | `		PH7_MemObjInit(pCtx->pVm,&sTmp);` |
|     5 | 5986 | `		PH7_MemObjStore(pArg,&sTmp);` |
|     5 | 5987 | `		PH7_MemObjToInteger(&sTmp);` |
|     5 | 5988 | `		*piOut = sTmp.x.iVal;` |
|     5 | 5989 | `		PH7_MemObjRelease(&sTmp);` |
|     5 | 5990 | `		return 1;` |
|     - | 5991 | `	}` |
|    13 | 5992 | `	*piOut = 0;` |
|    19 | 5993 | `	*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|     6 | 5994 | `		"Cannot access offset of type %s on SplFixedArray",ph7_type_name(pArg));` |
|    13 | 5995 | `	return 0;` |
|   135 | 5996 | `}` |
|    10 | 5997 | `static sxi32 FaOutOfBounds(ph7_context *pCtx)` |
|     1 | 5998 | `{` |
|    11 | 5999 | `	return PH7_VmThrowException(pCtx,"OutOfBoundsException","Index invalid or out of range");` |
|     1 | 6000 | `}` |
|     - | 6001 | ``/* php's setSize: grow with nulls, shrink by dropping the tail, answer `true`. */`` |
|   116 | 6002 | `static sxi32 FaResize(ph7_vm *pVm,ph7_class_instance *pThis,sxi64 nNew)` |
|     1 | 6003 | `{` |
|   117 | 6004 | `	sxi64 nOld = FaSize(pThis);` |
|   117 | 6005 | `	ph7_hashmap *pMap = FaMap(pVm,pThis);` |
|     - | 6006 | `	sxi64 i;` |
|   117 | 6007 | `	if( pMap == 0 ){` |
|   ! 0 | 6008 | `		return SXERR_MEM;` |
|     - | 6009 | `	}` |
|   127 | 6010 | `	for( i = nNew ; i < nOld ; ++i ){` |
|    11 | 6011 | `		ph7_hashmap_node *pNode = 0;` |
|    11 | 6012 | `		if( HashmapLookupIntKey(pMap,i,&pNode) == SXRET_OK ){` |
|    11 | 6013 | `			PH7_HashmapUnlinkNode(pNode,TRUE);` |
|     5 | 6014 | `		}` |
|     6 | 6015 | `	}` |
|   389 | 6016 | `	for( i = nOld ; i < nNew ; ++i ){` |
|     - | 6017 | `		ph7_value sNull;` |
|   273 | 6018 | `		PH7_MemObjInit(pVm,&sNull);` |
|   273 | 6019 | `		FaPut(pVm,pThis,i,&sNull);` |
|   273 | 6020 | `		PH7_MemObjRelease(&sNull);` |
|   137 | 6021 | `	}` |
|   117 | 6022 | `	PH7_NativeSetAttrInt(pVm,pThis,FA_N,nNew);` |
|   117 | 6023 | `	return SXRET_OK;` |
|    59 | 6024 | `}` |
|    90 | 6025 | `static int vm_builtin_SplFixedArray_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 6026 | `{` |
|    91 | 6027 | `	ph7_vm *pVm = pCtx->pVm;` |
|    91 | 6028 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    91 | 6029 | `	sxi64 nSize = 0;` |
|    91 | 6030 | `	if( pThis == 0 ){` |
|   ! 0 | 6031 | `		return PH7_OK;` |
|     - | 6032 | `	}` |
|    91 | 6033 | `	if( nArg > 0 ){` |
|    87 | 6034 | `		sxi32 rc = PH7_IntArgResolve(pCtx,apArg[0],"SplFixedArray::__construct",1,"$size","int",&nSize);` |
|    87 | 6035 | `		if( rc != SXRET_OK ){` |
|   ! 0 | 6036 | `			return rc;` |
|     - | 6037 | `		}` |
|    43 | 6038 | `	}` |
|    91 | 6039 | `	if( nSize < 0 ){` |
|     - | 6040 | `		/* php words this from __construct(), not from the setSize() it forwards to —` |
|     - | 6041 | ``		 * which is what the chunk's `$this->setSize()` reported. */`` |
|     3 | 6042 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 6043 | `			"SplFixedArray::__construct(): Argument #1 ($size) must be greater than or equal to 0");` |
|     - | 6044 | `	}` |
|    89 | 6045 | `	FaResize(pVm,pThis,nSize);` |
|    89 | 6046 | `	return PH7_OK;` |
|    46 | 6047 | `}` |
|     6 | 6048 | `static int vm_builtin_SplFixedArray_getSize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 6049 | `{` |
|     3 | 6050 | `	SXUNUSED(nArg);` |
|     3 | 6051 | `	SXUNUSED(apArg);` |
|     7 | 6052 | `	ph7_result_int64(pCtx,FaSize(PH7_ContextThis(pCtx)));` |
|     7 | 6053 | `	return PH7_OK;` |
|     1 | 6054 | `}` |
|    12 | 6055 | `static int vm_builtin_SplFixedArray_setSize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 6056 | `{` |
|    13 | 6057 | `	sxi64 nSize = 0;` |
|     - | 6058 | `	sxi32 rc;` |
|    13 | 6059 | `	if( nArg < 1 ){` |
|   ! 0 | 6060 | `		return PH7_OK;` |
|     - | 6061 | `	}` |
|    13 | 6062 | `	rc = PH7_IntArgResolve(pCtx,apArg[0],"SplFixedArray::setSize",1,"$size","int",&nSize);` |
|    13 | 6063 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 6064 | `		return rc;` |
|     - | 6065 | `	}` |
|    13 | 6066 | `	if( nSize < 0 ){` |
|     3 | 6067 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 6068 | `			"SplFixedArray::setSize(): Argument #1 ($size) must be greater than or equal to 0");` |
|     - | 6069 | `	}` |
|    11 | 6070 | `	FaResize(pCtx->pVm,PH7_ContextThis(pCtx),nSize);` |
|    11 | 6071 | ``	ph7_result_bool(pCtx,1);   /* php's `true` return type */`` |
|    11 | 6072 | `	return PH7_OK;` |
|     7 | 6073 | `}` |
|    12 | 6074 | `static int vm_builtin_SplFixedArray_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 6075 | `{` |
|     6 | 6076 | `	SXUNUSED(nArg);` |
|     6 | 6077 | `	SXUNUSED(apArg);` |
|    13 | 6078 | `	ph7_result_int64(pCtx,FaSize(PH7_ContextThis(pCtx)));` |
|    13 | 6079 | `	return PH7_OK;` |
|     1 | 6080 | `}` |
|    32 | 6081 | `static int vm_builtin_SplFixedArray_toArray(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 6082 | `{` |
|    33 | 6083 | `	ph7_value *pSlot = FaSlot(pCtx->pVm,PH7_ContextThis(pCtx));` |
|    16 | 6084 | `	SXUNUSED(nArg);` |
|    16 | 6085 | `	SXUNUSED(apArg);` |
|    33 | 6086 | `	if( pSlot ){` |
|    33 | 6087 | `		ph7_result_value(pCtx,pSlot);` |
|    16 | 6088 | `	}` |
|    33 | 6089 | `	return PH7_OK;` |
|     1 | 6090 | `}` |
|    20 | 6091 | `static int vm_builtin_SplFixedArray_offsetExists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 6092 | `{` |
|    21 | 6093 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    21 | 6094 | `	sxi64 iIdx = 0;` |
|    21 | 6095 | `	sxi32 rc = PH7_OK;` |
|     - | 6096 | `	ph7_value *pVal;` |
|    21 | 6097 | `	if( nArg < 1 ){` |
|   ! 0 | 6098 | `		return PH7_OK;` |
|     - | 6099 | `	}` |
|     - | 6100 | `	/* offsetExists RAISES for an undecodable offset exactly as the other three do —` |
|     - | 6101 | ``	 * `isset($f['x'])` is a TypeError, not a false — and answers false only for a`` |
|     - | 6102 | `	 * decodable index that is out of range or holds null. */` |
|    21 | 6103 | `	if( !FaOffset(pCtx,apArg[0],&iIdx,&rc) ){` |
|     5 | 6104 | `		return rc;` |
|     - | 6105 | `	}` |
|    17 | 6106 | `	if( iIdx < 0 \|\| iIdx >= FaSize(pThis) ){` |
|     7 | 6107 | `		ph7_result_bool(pCtx,0);` |
|     7 | 6108 | `		return PH7_OK;` |
|     - | 6109 | `	}` |
|     - | 6110 | `	/* php's isset() semantics: an unset slot holds null and is NOT set. */` |
|    11 | 6111 | `	pVal = FaAt(pCtx->pVm,pThis,iIdx);` |
|    11 | 6112 | `	ph7_result_bool(pCtx,pVal != 0 && (pVal->iFlags & MEMOBJ_NULL) == 0);` |
|    11 | 6113 | `	return PH7_OK;` |
|    11 | 6114 | `}` |
|    36 | 6115 | `static int vm_builtin_SplFixedArray_offsetGet(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 6116 | `{` |
|    37 | 6117 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    37 | 6118 | `	sxi64 iIdx = 0;` |
|    37 | 6119 | `	sxi32 rc = PH7_OK;` |
|     - | 6120 | `	ph7_value *pVal;` |
|    37 | 6121 | `	if( nArg < 1 ){` |
|   ! 0 | 6122 | `		return PH7_OK;` |
|     - | 6123 | `	}` |
|    37 | 6124 | `	if( !FaOffset(pCtx,apArg[0],&iIdx,&rc) ){` |
|     7 | 6125 | `		return rc;` |
|     - | 6126 | `	}` |
|    31 | 6127 | `	if( iIdx < 0 \|\| iIdx >= FaSize(pThis) ){` |
|     7 | 6128 | `		return FaOutOfBounds(pCtx);` |
|     - | 6129 | `	}` |
|    25 | 6130 | `	pVal = FaAt(pCtx->pVm,pThis,iIdx);` |
|    25 | 6131 | `	if( pVal ){` |
|    25 | 6132 | `		ph7_result_value(pCtx,pVal);` |
|    12 | 6133 | `	}` |
|    25 | 6134 | `	return PH7_OK;` |
|    19 | 6135 | `}` |
|   210 | 6136 | `static int vm_builtin_SplFixedArray_offsetSet(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 6137 | `{` |
|   211 | 6138 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   211 | 6139 | `	sxi64 iIdx = 0;` |
|   211 | 6140 | `	sxi32 rc = PH7_OK;` |
|   211 | 6141 | `	if( nArg < 2 ){` |
|   ! 0 | 6142 | `		return PH7_OK;` |
|     - | 6143 | `	}` |
|   211 | 6144 | `	if( !FaOffset(pCtx,apArg[0],&iIdx,&rc) ){` |
|     3 | 6145 | `		return rc;` |
|     - | 6146 | `	}` |
|   209 | 6147 | `	if( iIdx < 0 \|\| iIdx >= FaSize(pThis) ){` |
|     5 | 6148 | `		return FaOutOfBounds(pCtx);` |
|     - | 6149 | `	}` |
|   205 | 6150 | `	FaPut(pCtx->pVm,pThis,iIdx,apArg[1]);` |
|   205 | 6151 | `	return PH7_OK;` |
|   106 | 6152 | `}` |
|     2 | 6153 | `static int vm_builtin_SplFixedArray_offsetUnset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 6154 | `{` |
|     3 | 6155 | `	ph7_vm *pVm = pCtx->pVm;` |
|     3 | 6156 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     3 | 6157 | `	sxi64 iIdx = 0;` |
|     3 | 6158 | `	sxi32 rc = PH7_OK;` |
|     - | 6159 | `	ph7_value sNull;` |
|     3 | 6160 | `	if( nArg < 1 ){` |
|   ! 0 | 6161 | `		return PH7_OK;` |
|     - | 6162 | `	}` |
|     3 | 6163 | `	if( !FaOffset(pCtx,apArg[0],&iIdx,&rc) ){` |
|   ! 0 | 6164 | `		return rc;` |
|     - | 6165 | `	}` |
|     3 | 6166 | `	if( iIdx < 0 \|\| iIdx >= FaSize(pThis) ){` |
|   ! 0 | 6167 | `		return FaOutOfBounds(pCtx);` |
|     - | 6168 | `	}` |
|     - | 6169 | `	/* The slot survives at its index and becomes null: the array is FIXED. */` |
|     3 | 6170 | `	PH7_MemObjInit(pVm,&sNull);` |
|     3 | 6171 | `	FaPut(pVm,pThis,iIdx,&sNull);` |
|     3 | 6172 | `	PH7_MemObjRelease(&sNull);` |
|     3 | 6173 | `	return PH7_OK;` |
|     2 | 6174 | `}` |
|    24 | 6175 | `static int vm_builtin_SplFixedArray_fromArray(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 6176 | `{` |
|    25 | 6177 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 6178 | `	ph7_class *pCls;` |
|     - | 6179 | `	ph7_class_instance *pNew;` |
|     - | 6180 | `	ph7_hashmap *pSrc;` |
|     - | 6181 | `	ph7_hashmap_node *pNode,*pPrev;` |
|    25 | 6182 | `	int bPreserve = 1;` |
|    25 | 6183 | `	sxi64 nMax = -1, nNext = 0;` |
|    25 | 6184 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|   ! 0 | 6185 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 6186 | `			"SplFixedArray::fromArray(): Argument #1 ($array) must be of type array, %s given",` |
|   ! 0 | 6187 | `			nArg < 1 ? "none" : ph7_type_name(apArg[0]));` |
|     - | 6188 | `	}` |
|    25 | 6189 | `	if( nArg > 1 ){` |
|     7 | 6190 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|     3 | 6191 | `	}` |
|    25 | 6192 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     - | 6193 | `	/* php walks the keys FIRST and refuses the whole call before building anything. */` |
|    25 | 6194 | `	if( bPreserve ){` |
|    39 | 6195 | `		for( pNode = pSrc->pFirst ; pNode ; pNode = pPrev ){` |
|    27 | 6196 | `			pPrev = pNode->pPrev;` |
|    27 | 6197 | `			if( pNode->iType != HASHMAP_INT_NODE \|\| pNode->xKey.iKey < 0 ){` |
|     7 | 6198 | `				return PH7_VmThrowException(pCtx,"InvalidArgumentException",` |
|     - | 6199 | `					"array must contain only positive integer keys");` |
|     - | 6200 | `			}` |
|    21 | 6201 | `			if( pNode->xKey.iKey > nMax ){` |
|    19 | 6202 | `				nMax = pNode->xKey.iKey;` |
|     9 | 6203 | `			}` |
|    21 | 6204 | `			if( pNode == pSrc->pFirst && pPrev == 0 ){` |
|   ! 0 | 6205 | `				break;` |
|     - | 6206 | `			}` |
|    11 | 6207 | `		}` |
|     6 | 6208 | `	}` |
|    19 | 6209 | `	pCls = PH7_VmExtractClass(pVm,"SplFixedArray",sizeof("SplFixedArray")-1,FALSE,0);` |
|    19 | 6210 | `	if( pCls == 0 ){` |
|   ! 0 | 6211 | `		return PH7_OK;` |
|     - | 6212 | `	}` |
|    19 | 6213 | `	pNew = PH7_NewClassInstance(pVm,pCls);` |
|    19 | 6214 | `	if( pNew == 0 ){` |
|   ! 0 | 6215 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 6216 | `	}` |
|    19 | 6217 | `	pNew->iRef++;` |
|    19 | 6218 | `	FaResize(pVm,pNew,bPreserve ? nMax + 1 : (sxi64)pSrc->nEntry);` |
|    35 | 6219 | `	for( pNode = pSrc->pFirst ; pNode ; pNode = pPrev ){` |
|    31 | 6220 | `		ph7_value *pVal = HashmapExtractNodeValue(pNode);` |
|    31 | 6221 | `		pPrev = pNode->pPrev;` |
|    31 | 6222 | `		if( pVal ){` |
|    31 | 6223 | `			FaPut(pVm,pNew,bPreserve ? pNode->xKey.iKey : nNext,pVal);` |
|    15 | 6224 | `		}` |
|    31 | 6225 | `		nNext++;` |
|    31 | 6226 | `		if( pPrev == 0 ){` |
|    15 | 6227 | `			break;` |
|     - | 6228 | `		}` |
|     9 | 6229 | `	}` |
|    19 | 6230 | `	PH7_NativeResultObject(pCtx,pNew);` |
|    19 | 6231 | `	PH7_ClassInstanceUnref(pNew);` |
|    19 | 6232 | `	return PH7_OK;` |
|    13 | 6233 | `}` |
|     - | 6234 | `/* php's getIterator() answers an InternalIterator over the elements — the same` |
|     - | 6235 | ` * machinery every native IteratorAggregate here uses, which is also what makes` |
|     - | 6236 | ` * two iterators over one array independent. */` |
|    66 | 6237 | `static void FaIterSettle(ph7_vm *pVm,ph7_class_instance *pIt)` |
|     1 | 6238 | `{` |
|    67 | 6239 | `	ph7_class_instance *pSrc = PH7_NativeAttrObj(pIt,PH7_NATIVE_IT_SRC);` |
|    67 | 6240 | `	sxi64 iPos = PH7_NativeAttrInt(pIt,PH7_NATIVE_IT_POS);` |
|     - | 6241 | `	ph7_value *pVal;` |
|    67 | 6242 | `	if( pSrc == 0 \|\| iPos < 0 \|\| iPos >= FaSize(pSrc) ){` |
|    13 | 6243 | `		PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);` |
|    13 | 6244 | `		return;` |
|     - | 6245 | `	}` |
|    55 | 6246 | `	pVal = FaAt(&(*pVm),pSrc,iPos);` |
|    55 | 6247 | `	if( pVal ){` |
|    82 | 6248 | `		PH7_NativeSetProp(&(*pVm),pIt,PH7_NATIVE_IT_CUR,` |
|    27 | 6249 | `			(int)SyStrlen(PH7_NATIVE_IT_CUR),pVal);` |
|    27 | 6250 | `	}` |
|    55 | 6251 | `	PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_KEY,iPos);` |
|    55 | 6252 | `	PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,0);` |
|    34 | 6253 | `}` |
|    34 | 6254 | `static void FaIterRewind(ph7_vm *pVm,ph7_class_instance *pIt)` |
|     1 | 6255 | `{` |
|    35 | 6256 | `	PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_POS,0);` |
|    35 | 6257 | `	FaIterSettle(&(*pVm),pIt);` |
|    35 | 6258 | `}` |
|    32 | 6259 | `static void FaIterNext(ph7_vm *pVm,ph7_class_instance *pIt)` |
|     1 | 6260 | `{` |
|    49 | 6261 | `	PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_POS,` |
|    32 | 6262 | `		PH7_NativeAttrInt(pIt,PH7_NATIVE_IT_POS) + 1);` |
|    33 | 6263 | `	FaIterSettle(&(*pVm),pIt);` |
|    33 | 6264 | `}` |
|     - | 6265 | `static const PH7_NativeIterVtab sFaIterVtab = { FaIterRewind, FaIterNext };` |
|    18 | 6266 | `static int vm_builtin_SplFixedArray_getIterator(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 6267 | `{` |
|    19 | 6268 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 6269 | `	ph7_class_instance *pIt;` |
|     9 | 6270 | `	SXUNUSED(nArg);` |
|     9 | 6271 | `	SXUNUSED(apArg);` |
|    19 | 6272 | `	if( pThis == 0 ){` |
|   ! 0 | 6273 | `		return PH7_OK;` |
|     - | 6274 | `	}` |
|    19 | 6275 | `	pIt = PH7_NativeIteratorNew(pCtx->pVm,pThis);` |
|    19 | 6276 | `	if( pIt == 0 ){` |
|   ! 0 | 6277 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 6278 | `	}` |
|    19 | 6279 | `	PH7_NativeResultObject(pCtx,pIt);` |
|    19 | 6280 | `	return PH7_OK;` |
|    10 | 6281 | `}` |
|   ! 0 | 6282 | `static int vm_builtin_SplFixedArray_wakeup(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 6283 | `{` |
|   ! 0 | 6284 | `	SXUNUSED(nArg);` |
|   ! 0 | 6285 | `	SXUNUSED(apArg);` |
|   ! 0 | 6286 | `	SXUNUSED(pCtx);` |
|   ! 0 | 6287 | `	return PH7_OK;   /* php 8.4 keeps it, deprecated, doing nothing */` |
|   ! 0 | 6288 | `}` |
|     - | 6289 | `/*` |
|     - | 6290 | ` * php's __serialize() here is NOT toArray(): the members ride in the SAME array as` |
|     - | 6291 | ` * the elements, told apart by their key — an INT key is an element and a STRING key` |
|     - | 6292 | ` * is a property. That is the whole reason the payload of a SplFixedArray subclass` |
|     - | 6293 | `` * reads `{i:0;N;i:1;N;s:1:"p";i:9;}` and not a nested pair.`` |
|     - | 6294 | ` */` |
|    14 | 6295 | `static int vm_builtin_SplFixedArray_serializeMagic(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 6296 | `{` |
|    15 | 6297 | `	ph7_vm *pVm = pCtx->pVm;` |
|    15 | 6298 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 6299 | `	ph7_value sOut,*pSlot;` |
|     7 | 6300 | `	SXUNUSED(nArg);` |
|     7 | 6301 | `	SXUNUSED(apArg);` |
|    15 | 6302 | `	pSlot = FaSlot(pVm,pThis);` |
|    15 | 6303 | `	PH7_MemObjInit(pVm,&sOut);` |
|    15 | 6304 | `	if( pSlot ){` |
|    15 | 6305 | `		PH7_MemObjStore(pSlot,&sOut);` |
|     7 | 6306 | `	}` |
|    15 | 6307 | `	if( PH7_MemObjToHashmap(&sOut) != SXRET_OK ){` |
|   ! 0 | 6308 | `		PH7_MemObjRelease(&sOut);` |
|   ! 0 | 6309 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 6310 | `	}` |
|    15 | 6311 | `	SplAddMembers(pVm,pThis,&sOut);   /* string keys, beside the int-keyed elements */` |
|    15 | 6312 | `	ph7_result_value(pCtx,&sOut);` |
|    15 | 6313 | `	PH7_MemObjRelease(&sOut);` |
|    15 | 6314 | `	return PH7_OK;` |
|     8 | 6315 | `}` |
|     - | 6316 | `/* Split the payload back apart: int keys rebuild the elements, string keys the` |
|     - | 6317 | ` * properties. The element count is what the INT half holds, not the whole array. */` |
|     - | 6318 | `typedef struct fa_unser_ctx fa_unser_ctx;` |
|     - | 6319 | `struct fa_unser_ctx` |
|     - | 6320 | `{` |
|     - | 6321 | `	ph7_class_instance *pThis;` |
|     - | 6322 | `	ph7_value *pElems;` |
|     - | 6323 | `	sxi64 nElem;` |
|     - | 6324 | `};` |
|    18 | 6325 | `static int FaUnserWalk(ph7_value *pKey,ph7_value *pVal,void *pUserData)` |
|     1 | 6326 | `{` |
|    19 | 6327 | `	fa_unser_ctx *pFa = (fa_unser_ctx *)pUserData;` |
|    19 | 6328 | `	if( ph7_value_is_string(pKey) ){` |
|     - | 6329 | `		int nKey;` |
|     5 | 6330 | `		const char *zKey = ph7_value_to_string(pKey,&nKey);` |
|     5 | 6331 | `		PH7_NativeSetProp(pFa->pThis->pVm,pFa->pThis,zKey,(sxu32)nKey,pVal);` |
|     5 | 6332 | `		return PH7_OK;` |
|     - | 6333 | `	}` |
|    15 | 6334 | `	ph7_array_add_elem(pFa->pElems,0,pVal);` |
|    15 | 6335 | `	pFa->nElem++;` |
|    15 | 6336 | `	return PH7_OK;` |
|    10 | 6337 | `}` |
|     6 | 6338 | `static int vm_builtin_SplFixedArray_unserializeMagic(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 6339 | `{` |
|     7 | 6340 | `	ph7_vm *pVm = pCtx->pVm;` |
|     7 | 6341 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 6342 | `	fa_unser_ctx sFa;` |
|     - | 6343 | `	ph7_value sElems,*pSlot;` |
|     7 | 6344 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_HASHMAP) == 0 \|\| pThis == 0 ){` |
|   ! 0 | 6345 | `		return PH7_OK;` |
|     - | 6346 | `	}` |
|     7 | 6347 | `	PH7_MemObjInit(pVm,&sElems);` |
|     7 | 6348 | `	if( PH7_MemObjToHashmap(&sElems) != SXRET_OK ){` |
|   ! 0 | 6349 | `		PH7_MemObjRelease(&sElems);` |
|   ! 0 | 6350 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 6351 | `	}` |
|     7 | 6352 | `	sFa.pThis = pThis;` |
|     7 | 6353 | `	sFa.pElems = &sElems;` |
|     7 | 6354 | `	sFa.nElem = 0;` |
|     7 | 6355 | `	ph7_array_walk(apArg[0],FaUnserWalk,&sFa);` |
|     7 | 6356 | `	pSlot = PH7_NativeAttr(pThis,FA_A);` |
|     7 | 6357 | `	if( pSlot ){` |
|     7 | 6358 | `		PH7_MemObjRelease(pSlot);` |
|     7 | 6359 | `		PH7_MemObjStore(&sElems,pSlot);` |
|     3 | 6360 | `	}` |
|     7 | 6361 | `	PH7_MemObjRelease(&sElems);` |
|     7 | 6362 | `	PH7_NativeSetAttrInt(pVm,pThis,FA_N,sFa.nElem);` |
|     7 | 6363 | `	return PH7_OK;` |
|     4 | 6364 | `}` |
|     - | 6365 | `/*` |
|     - | 6366 | ` * php's get_properties: the ELEMENTS, keyed by index, on every surface —` |
|     - | 6367 | ` * var_dump, print_r, the (array) cast and (through __serialize) serialize(). This` |
|     - | 6368 | ` * is the one native class so far whose presentation is the same for the debug and` |
|     - | 6369 | ` * the cast form.` |
|     - | 6370 | ` */` |
|     2 | 6371 | `static sxi32 FaPresent(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)` |
|     1 | 6372 | `{` |
|     3 | 6373 | `	sxi64 n = FaSize(pThis), i;` |
|     1 | 6374 | `	SXUNUSED(bDebug);` |
|     9 | 6375 | `	for( i = 0 ; i < n ; ++i ){` |
|     7 | 6376 | `		ph7_value sKey,*pVal = FaAt(&(*pVm),pThis,i);` |
|     7 | 6377 | `		if( pVal == 0 ){` |
|   ! 0 | 6378 | `			continue;` |
|     - | 6379 | `		}` |
|     7 | 6380 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,i);` |
|     7 | 6381 | `		ph7_array_add_elem(pOut,&sKey,pVal);` |
|     7 | 6382 | `		PH7_MemObjRelease(&sKey);` |
|     4 | 6383 | `	}` |
|     3 | 6384 | `	return PH7_OK;` |
|     1 | 6385 | `}` |
|     - | 6386 | `/*` |
|     - | 6387 | ` * The declaration. Method ORDER and the interface list are spl_fixedarray.stub's;` |
|     - | 6388 | ` * note that __construct, __serialize, __unserialize, getIterator and jsonSerialize` |
|     - | 6389 | ` * are the FIVE methods php does NOT mark tentative here.` |
|     - | 6390 | ` */` |
|  5146 | 6391 | `static sxi32 VmInstallSplFixedArray(ph7_vm *pVm)` |
|     5 | 6392 | `{` |
|     - | 6393 | `	static const PH7_NativePropDef aFaProp[] = {` |
|     - | 6394 | `		{ FA_A, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - | 6395 | `		{ FA_N, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - | 6396 | `	};` |
|     - | 6397 | `	static const PH7_NativeMethodDef aFaMethod[] = {` |
|     - | 6398 | `		{ "__construct",   PH7_MOD_PUBLIC, "int $size = 0", 0,` |
|     - | 6399 | `		  vm_builtin_SplFixedArray_construct },` |
|     - | 6400 | `		{ "__wakeup",      PH7_MOD_PUBLIC, "", "@void", vm_builtin_SplFixedArray_wakeup },` |
|     - | 6401 | `		{ "__serialize",   PH7_MOD_PUBLIC, "", "array", vm_builtin_SplFixedArray_serializeMagic },` |
|     - | 6402 | `		{ "__unserialize", PH7_MOD_PUBLIC, "array $data", "void",` |
|     - | 6403 | `		  vm_builtin_SplFixedArray_unserializeMagic },` |
|     - | 6404 | `		{ "count",         PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplFixedArray_count },` |
|     - | 6405 | `		{ "toArray",       PH7_MOD_PUBLIC, "", "@array", vm_builtin_SplFixedArray_toArray },` |
|     - | 6406 | `		{ "fromArray",     PH7_MOD_PUBLIC\|PH7_MOD_STATIC,` |
|     - | 6407 | `		  "array $array, bool $preserveKeys = true", "@SplFixedArray",` |
|     - | 6408 | `		  vm_builtin_SplFixedArray_fromArray },` |
|     - | 6409 | `		{ "getSize",       PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplFixedArray_getSize },` |
|     - | 6410 | `		{ "setSize",       PH7_MOD_PUBLIC, "int $size", "@true", vm_builtin_SplFixedArray_setSize },` |
|     - | 6411 | `		/* php's stub leaves the four offsets UNTYPED and decodes them itself, the` |
|     - | 6412 | `		 * same shape SplDoublyLinkedList has — but a different rule and a different` |
|     - | 6413 | `		 * refusal, so FaOffset() rather than the DLL's PH7_IntArgResolve. */` |
|     - | 6414 | `		{ "offsetExists",  PH7_MOD_PUBLIC, "$index", "@bool",` |
|     - | 6415 | `		  vm_builtin_SplFixedArray_offsetExists },` |
|     - | 6416 | `		{ "offsetGet",     PH7_MOD_PUBLIC, "$index", "@mixed", vm_builtin_SplFixedArray_offsetGet },` |
|     - | 6417 | `		{ "offsetSet",     PH7_MOD_PUBLIC, "$index, mixed $value", "@void",` |
|     - | 6418 | `		  vm_builtin_SplFixedArray_offsetSet },` |
|     - | 6419 | `		{ "offsetUnset",   PH7_MOD_PUBLIC, "$index", "@void",` |
|     - | 6420 | `		  vm_builtin_SplFixedArray_offsetUnset },` |
|     - | 6421 | `		{ "getIterator",   PH7_MOD_PUBLIC, "", "Iterator", vm_builtin_SplFixedArray_getIterator },` |
|     - | 6422 | `		{ "jsonSerialize", PH7_MOD_PUBLIC, "", "array", vm_builtin_SplFixedArray_toArray },` |
|     - | 6423 | `	};` |
|     - | 6424 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - | 6425 | `		{ "SplFixedArray", 0, "IteratorAggregate,ArrayAccess,Countable,JsonSerializable", 0,` |
|     - | 6426 | `		  aFaMethod, SX_ARRAYSIZE(aFaMethod), 0, 0,` |
|     - | 6427 | `		  aFaProp, SX_ARRAYSIZE(aFaProp), 0, &sFaIterVtab, FaPresent },` |
|     - | 6428 | `	};` |
|  5151 | 6429 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|     5 | 6430 | `}` |
|     - | 6431 | `/*` |
|     - | 6432 | ` * ---------------------------------------------------------------------------` |
|     - | 6433 | ` * SplObjectStorage, SplObserver and SplSubject.` |
|     - | 6434 | ` *` |
|     - | 6435 | `` * php's `spl_SplObjectStorage` is a hashtable of {obj, inf} pairs keyed by the`` |
|     - | 6436 | `` * object HANDLE, plus TWO cursors that are not the same thing: `pos` walks the`` |
|     - | 6437 | `` * table and `index` is the integer `key()` reports. Every method that changes the`` |
|     - | 6438 | ` * membership resets one or both, and the chunk -- which kept a single integer` |
|     - | 6439 | `` * offset -- had none of that: `detach()` mid-walk left the walk where it was`` |
|     - | 6440 | `` * (php restarts it), and `addAll()` left `key()` counting from wherever it stood.`` |
|     - | 6441 | ` *` |
|     - | 6442 | `` * What the chunk did not have AT ALL, which is most of the class: `seek()` and the`` |
|     - | 6443 | `` * `SeekableIterator` interface it comes from, `Serializable` with its`` |
|     - | 6444 | `` * `serialize()`/`unserialize()` pair, the `__serialize()`/`__unserialize()` pair`` |
|     - | 6445 | `` * php actually uses, and `__debugInfo()`. Six methods and two interfaces missing`` |
|     - | 6446 | ` * from a 25-method class -- rule 53, and the reason a method-by-method reading is` |
|     - | 6447 | ` * not a conversion.` |
|     - | 6448 | ` *` |
|     - | 6449 | `` * Two more the model hides. **`current()` on an invalid iterator RAISES**`` |
|     - | 6450 | `` * (`Called current() on invalid iterator`) where the chunk answered null, and`` |
|     - | 6451 | `` * **an overridden `getHash()` is what keys the table** -- php looks the method up`` |
|     - | 6452 | `` * once per instance (`fptr_get_hash`) and every attach/detach/contains goes`` |
|     - | 6453 | ` * through it, so a subclass that hashes two distinct objects the same stores ONE` |
|     - | 6454 | `` * entry. The chunk called `spl_object_id()` directly and ignored its own`` |
|     - | 6455 | `` * `getHash()`, so overriding it did nothing.`` |
|     - | 6456 | ` *` |
|     - | 6457 | ` * php DEPRECATES attach/detach/contains since 8.5 and PHL says nothing, which is` |
|     - | 6458 | ` * the same non-deprecated-compatibility policy the chunk carried (the notice is` |
|     - | 6459 | ` * the only difference and no valid php depends on it).` |
|     - | 6460 | ` */` |
|     - | 6461 | `#define SOS_S "__s"   /* php's storage: key -> ['obj' => object, 'inf' => info] */` |
|     - | 6462 | `#define SOS_I "__i"   /* php's index: what key() reports, NOT a position */` |
|     - | 6463 |  |
|     - | 6464 | `/* The storage slot, separated for writing (every caller may mutate it). */` |
|   480 | 6465 | `static ph7_value * SosSlot(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 | 6466 | `{` |
|   481 | 6467 | `	ph7_value *pSlot = pThis ? PH7_NativeAttr(pThis,SOS_S) : 0;` |
|   481 | 6468 | `	if( pSlot == 0 ){` |
|   ! 0 | 6469 | `		return 0;` |
|     - | 6470 | `	}` |
|   481 | 6471 | `	if( (pSlot->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    79 | 6472 | `		if( PH7_MemObjToHashmap(pSlot) != SXRET_OK ){` |
|   ! 0 | 6473 | `			return 0;` |
|     - | 6474 | `		}` |
|    39 | 6475 | `	}` |
|   481 | 6476 | `	if( PH7_HashmapCowSeparate(pVm,pSlot) == 0 ){` |
|   ! 0 | 6477 | `		return 0;` |
|     - | 6478 | `	}` |
|   481 | 6479 | `	return pSlot;` |
|   241 | 6480 | `}` |
|   480 | 6481 | `static ph7_hashmap * SosMap(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 | 6482 | `{` |
|   481 | 6483 | `	ph7_value *pSlot = SosSlot(pVm,pThis);` |
|   481 | 6484 | `	return pSlot ? (ph7_hashmap *)pSlot->x.pOther : 0;` |
|     1 | 6485 | `}` |
|     - | 6486 | `/* One half of a stored pair: php's element->obj / element->inf. */` |
|   190 | 6487 | `static ph7_value * SosPart(ph7_value *pPair,const char *zKey)` |
|     1 | 6488 | `{` |
|   191 | 6489 | `	ph7_hashmap_node *pNode = 0;` |
|   191 | 6490 | `	if( pPair == 0 \|\| (pPair->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     5 | 6491 | `		return 0;` |
|     - | 6492 | `	}` |
|   279 | 6493 | `	if( HashmapLookupBlobKey((ph7_hashmap *)pPair->x.pOther,zKey,` |
|   280 | 6494 | `		(sxu32)SyStrlen(zKey),&pNode) != SXRET_OK ){` |
|   ! 0 | 6495 | `		return 0;` |
|     - | 6496 | `	}` |
|   187 | 6497 | `	return HashmapExtractNodeValue(pNode);` |
|    96 | 6498 | `}` |
|     - | 6499 | ``/* Write one half of a pair; a NULL value is php's `ZVAL_NULL(&element->inf)`. */`` |
|   276 | 6500 | `static void SosSetPart(ph7_vm *pVm,ph7_value *pPair,const char *zKey,ph7_value *pVal)` |
|     1 | 6501 | `{` |
|     - | 6502 | `	ph7_value sKey,sNull;` |
|   277 | 6503 | `	PH7_MemObjInitFromString(pVm,&sKey,0);` |
|   277 | 6504 | `	PH7_MemObjStringAppend(&sKey,zKey,(sxu32)SyStrlen(zKey));` |
|   277 | 6505 | `	if( pVal ){` |
|   277 | 6506 | `		ph7_array_add_elem(pPair,&sKey,pVal);` |
|   139 | 6507 | `	}else{` |
|   ! 0 | 6508 | `		PH7_MemObjInit(pVm,&sNull);` |
|   ! 0 | 6509 | `		ph7_array_add_elem(pPair,&sKey,&sNull);` |
|   ! 0 | 6510 | `		PH7_MemObjRelease(&sNull);` |
|     - | 6511 | `	}` |
|   277 | 6512 | `	PH7_MemObjRelease(&sKey);` |
|   277 | 6513 | `}` |
|     - | 6514 | ``/* The pair a node holds, separated: php mutates `element->inf` in place, and here`` |
|     - | 6515 | ` * that is a NESTED array whose COW copy has to be broken first -- a pair handed` |
|     - | 6516 | ` * out by __serialize()/__debugInfo() would otherwise change with it. */` |
|     8 | 6517 | `static ph7_value * SosPairForWrite(ph7_vm *pVm,ph7_hashmap_node *pNode)` |
|     1 | 6518 | `{` |
|     9 | 6519 | `	ph7_value *pPair = HashmapExtractNodeValue(pNode);` |
|     9 | 6520 | `	if( pPair == 0 \|\| (pPair->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|   ! 0 | 6521 | `		return 0;` |
|     - | 6522 | `	}` |
|     9 | 6523 | `	return PH7_HashmapCowSeparate(pVm,pPair) ? pPair : 0;` |
|     5 | 6524 | `}` |
|     - | 6525 | `/*` |
|     - | 6526 | `` * php's `fptr_get_hash`: the class caches the method ONLY when a subclass declares`` |
|     - | 6527 | ` * its own, and every keyed operation then runs it. The one native body is this` |
|     - | 6528 | ` * class's own, so a non-native getHash() IS the override.` |
|     - | 6529 | ` */` |
|   190 | 6530 | `static ph7_class_method * SosUserHash(ph7_class_instance *pThis)` |
|     1 | 6531 | `{` |
|     - | 6532 | `	ph7_class_method *pMethod;` |
|   191 | 6533 | `	if( pThis == 0 ){` |
|   ! 0 | 6534 | `		return 0;` |
|     - | 6535 | `	}` |
|   191 | 6536 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"getHash",sizeof("getHash")-1);` |
|   191 | 6537 | `	if( pMethod == 0 \|\| (pMethod->sFunc.iFlags & VM_FUNC_NATIVE) ){` |
|   185 | 6538 | `		return 0;` |
|     - | 6539 | `	}` |
|     7 | 6540 | `	return pMethod;` |
|    96 | 6541 | `}` |
|     - | 6542 | `/*` |
|     - | 6543 | ` * php's spl_object_storage_get_hash: the object HANDLE, or the STRING an` |
|     - | 6544 | ` * overridden getHash() answers. php checks the returned type itself (its own` |
|     - | 6545 | ` * return declaration would coerce first, so this only fires for an untyped` |
|     - | 6546 | ` * override) and names the RUNTIME class in the refusal.` |
|     - | 6547 | ` */` |
|   190 | 6548 | `static sxi32 SosKey(ph7_context *pCtx,ph7_class_instance *pThis,ph7_value *pObj,ph7_value *pKey)` |
|     1 | 6549 | `{` |
|   191 | 6550 | `	ph7_vm *pVm = pCtx->pVm;` |
|   191 | 6551 | `	ph7_class_method *pHash = SosUserHash(pThis);` |
|     - | 6552 | `	ph7_value sRes,*apArg[1];` |
|     - | 6553 | `	sxi32 rc;` |
|   191 | 6554 | `	if( pHash == 0 ){` |
|   185 | 6555 | `		ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|   185 | 6556 | `		PH7_MemObjRelease(pKey);` |
|   185 | 6557 | `		PH7_MemObjInitFromInt(pVm,pKey,(sxi64)pInst->nObjId);` |
|   185 | 6558 | `		return PH7_OK;` |
|     - | 6559 | `	}` |
|     7 | 6560 | `	PH7_MemObjInit(pVm,&sRes);` |
|     7 | 6561 | `	apArg[0] = pObj;` |
|     7 | 6562 | `	rc = PH7_VmCallClassMethod(pVm,pThis,pHash,&sRes,1,apArg);` |
|     7 | 6563 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 6564 | `		PH7_MemObjRelease(&sRes);` |
|   ! 0 | 6565 | `		return rc;` |
|     - | 6566 | `	}` |
|     7 | 6567 | `	if( (sRes.iFlags & MEMOBJ_STRING) == 0 ){` |
|     - | 6568 | `		char zGiven[64];` |
|   ! 0 | 6569 | `		SyString *pName = &pThis->pClass->sName;` |
|   ! 0 | 6570 | `		PH7_MemObjRelease(&sRes);` |
|   ! 0 | 6571 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 6572 | `			"%z::getHash(): Return value must be of type string, %s returned",` |
|   ! 0 | 6573 | `			pName,VmValueGivenName(&sRes,zGiven,sizeof(zGiven)));` |
|     - | 6574 | `	}` |
|     7 | 6575 | `	PH7_MemObjRelease(pKey);` |
|     7 | 6576 | `	PH7_MemObjInit(pVm,pKey);` |
|     7 | 6577 | `	PH7_MemObjStore(&sRes,pKey);` |
|     7 | 6578 | `	PH7_MemObjRelease(&sRes);` |
|     7 | 6579 | `	return PH7_OK;` |
|    96 | 6580 | `}` |
|     - | 6581 | `/*` |
|     - | 6582 | ` * php's Z_PARAM_OBJ for the four ArrayAccess offsets: their stub leaves $object` |
|     - | 6583 | `` * UNTYPED (a `@param object` docblock, which Reflection does not print) while the`` |
|     - | 6584 | ` * ZPP is an object, so the declared type says nothing and each body words the` |
|     - | 6585 | ` * refusal here -- SplDoublyLinkedList's $index has the same shape one type over.` |
|     - | 6586 | ` * The name is always this class's, even from a subclass (php's).` |
|     - | 6587 | ` */` |
|   156 | 6588 | `static sxi32 SosObjectArg(ph7_context *pCtx,const char *zMethod,int nArg,ph7_value **apArg,` |
|     - | 6589 | `	ph7_value **ppObj)` |
|     1 | 6590 | `{` |
|     - | 6591 | `	char zGiven[64];` |
|   157 | 6592 | `	*ppObj = 0;` |
|   157 | 6593 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) && apArg[0]->x.pOther ){` |
|   151 | 6594 | `		*ppObj = apArg[0];` |
|   151 | 6595 | `		return PH7_OK;` |
|     - | 6596 | `	}` |
|    13 | 6597 | `	return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 6598 | `		"SplObjectStorage::%s(): Argument #1 ($object) must be of type object, %s given",` |
|     6 | 6599 | `		zMethod,nArg < 1 ? "none" : VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));` |
|    79 | 6600 | `}` |
|     - | 6601 | `/* The other storage a set operation takes; php's ZPP already screened the class. */` |
|    12 | 6602 | `static ph7_class_instance * SosOther(int nArg,ph7_value **apArg)` |
|     1 | 6603 | `{` |
|    13 | 6604 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|   ! 0 | 6605 | `		return 0;` |
|     - | 6606 | `	}` |
|    13 | 6607 | `	return (ph7_class_instance *)apArg[0]->x.pOther;` |
|     7 | 6608 | `}` |
|     - | 6609 | `/*` |
|     - | 6610 | ` * php's spl_object_storage_attach. The two values are COPIED first: computing the` |
|     - | 6611 | ` * key can run an overridden getHash(), and any call into user code moves every` |
|     - | 6612 | ` * ph7_value the caller is holding (rule 47).` |
|     - | 6613 | ` */` |
|   140 | 6614 | `static sxi32 SosAttach(ph7_context *pCtx,ph7_class_instance *pThis,ph7_value *pObj,ph7_value *pInf)` |
|     1 | 6615 | `{` |
|   141 | 6616 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 6617 | `	ph7_hashmap *pMap;` |
|   141 | 6618 | `	ph7_hashmap_node *pNode = 0;` |
|     - | 6619 | `	ph7_value sObj,sInf,sKey,sPair;` |
|     - | 6620 | `	sxi32 rc;` |
|   141 | 6621 | `	PH7_MemObjInit(pVm,&sObj);` |
|   141 | 6622 | `	PH7_MemObjInit(pVm,&sInf);` |
|   141 | 6623 | `	PH7_MemObjInit(pVm,&sKey);` |
|   141 | 6624 | `	PH7_MemObjStore(pObj,&sObj);` |
|   141 | 6625 | `	if( pInf ){` |
|   141 | 6626 | `		PH7_MemObjStore(pInf,&sInf);` |
|    70 | 6627 | `	}` |
|   141 | 6628 | `	rc = SosKey(pCtx,pThis,&sObj,&sKey);` |
|   141 | 6629 | `	if( rc != PH7_OK ){` |
|   ! 0 | 6630 | `		goto done;` |
|     - | 6631 | `	}` |
|   141 | 6632 | `	pMap = SosMap(pVm,pThis);` |
|   141 | 6633 | `	if( pMap == 0 ){` |
|   ! 0 | 6634 | `		goto done;` |
|     - | 6635 | `	}` |
|   141 | 6636 | `	if( PH7_HashmapLookup(pMap,&sKey,&pNode) == SXRET_OK ){` |
|     7 | 6637 | `		ph7_value *pPair = SosPairForWrite(pVm,pNode);` |
|     7 | 6638 | `		if( pPair ){` |
|     7 | 6639 | `			SosSetPart(pVm,pPair,"inf",&sInf);` |
|     3 | 6640 | `		}` |
|     7 | 6641 | `		goto done;` |
|     - | 6642 | `	}` |
|   135 | 6643 | `	PH7_MemObjInit(pVm,&sPair);` |
|   135 | 6644 | `	if( PH7_MemObjToHashmap(&sPair) != SXRET_OK ){` |
|   ! 0 | 6645 | `		PH7_MemObjRelease(&sPair);` |
|   ! 0 | 6646 | `		rc = PH7_ContextMemoryError(pCtx);` |
|   ! 0 | 6647 | `		goto done;` |
|     - | 6648 | `	}` |
|   135 | 6649 | `	SosSetPart(pVm,&sPair,"obj",&sObj);` |
|   135 | 6650 | `	SosSetPart(pVm,&sPair,"inf",&sInf);` |
|     - | 6651 | `	/* php's position is an INTEGER index into the bucket array, so a cursor that` |
|     - | 6652 | `	 * ran off the end is revived by the insert and the walk resumes on the new` |
|     - | 6653 | `	 * element -- what addAll() mid-iteration does there. */` |
|   135 | 6654 | `	SplStoreInsert(pMap,&sKey,&sPair);` |
|   135 | 6655 | `	PH7_MemObjRelease(&sPair);` |
|    70 | 6656 | `done:` |
|   141 | 6657 | `	PH7_MemObjRelease(&sObj);` |
|   141 | 6658 | `	PH7_MemObjRelease(&sInf);` |
|   141 | 6659 | `	PH7_MemObjRelease(&sKey);` |
|   141 | 6660 | `	return rc;` |
|     1 | 6661 | `}` |
|     - | 6662 | `/* php's spl_object_storage_detach: drop the entry, saying whether there was one. */` |
|    14 | 6663 | `static sxi32 SosDetach(ph7_context *pCtx,ph7_class_instance *pThis,ph7_value *pObj,int *pbGone)` |
|     1 | 6664 | `{` |
|    15 | 6665 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 6666 | `	ph7_hashmap *pMap;` |
|    15 | 6667 | `	ph7_hashmap_node *pNode = 0;` |
|     - | 6668 | `	ph7_value sObj,sKey;` |
|     - | 6669 | `	sxi32 rc;` |
|    15 | 6670 | `	if( pbGone ){` |
|   ! 0 | 6671 | `		*pbGone = 0;` |
|   ! 0 | 6672 | `	}` |
|    15 | 6673 | `	PH7_MemObjInit(pVm,&sObj);` |
|    15 | 6674 | `	PH7_MemObjInit(pVm,&sKey);` |
|    15 | 6675 | `	PH7_MemObjStore(pObj,&sObj);` |
|    15 | 6676 | `	rc = SosKey(pCtx,pThis,&sObj,&sKey);` |
|    15 | 6677 | `	if( rc == PH7_OK ){` |
|    15 | 6678 | `		pMap = SosMap(pVm,pThis);` |
|    15 | 6679 | `		if( pMap && PH7_HashmapLookup(pMap,&sKey,&pNode) == SXRET_OK ){` |
|    15 | 6680 | `			PH7_HashmapUnlinkNode(pNode,TRUE);` |
|    15 | 6681 | `			if( pbGone ){` |
|   ! 0 | 6682 | `				*pbGone = 1;` |
|   ! 0 | 6683 | `			}` |
|     7 | 6684 | `		}` |
|     7 | 6685 | `	}` |
|    15 | 6686 | `	PH7_MemObjRelease(&sObj);` |
|    15 | 6687 | `	PH7_MemObjRelease(&sKey);` |
|    15 | 6688 | `	return rc;` |
|     1 | 6689 | `}` |
|     - | 6690 | `/* php's spl_object_storage_contains: an entry EXISTS, whatever its info holds. */` |
|    14 | 6691 | `static sxi32 SosContains(ph7_context *pCtx,ph7_class_instance *pThis,ph7_value *pObj,int *pbFound)` |
|     1 | 6692 | `{` |
|    15 | 6693 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 6694 | `	ph7_hashmap *pMap;` |
|    15 | 6695 | `	ph7_hashmap_node *pNode = 0;` |
|     - | 6696 | `	ph7_value sObj,sKey;` |
|     - | 6697 | `	sxi32 rc;` |
|    15 | 6698 | `	*pbFound = 0;` |
|    15 | 6699 | `	PH7_MemObjInit(pVm,&sObj);` |
|    15 | 6700 | `	PH7_MemObjInit(pVm,&sKey);` |
|    15 | 6701 | `	PH7_MemObjStore(pObj,&sObj);` |
|    15 | 6702 | `	rc = SosKey(pCtx,pThis,&sObj,&sKey);` |
|    15 | 6703 | `	if( rc == PH7_OK ){` |
|    15 | 6704 | `		pMap = SosMap(pVm,pThis);` |
|    15 | 6705 | `		*pbFound = pMap && PH7_HashmapLookup(pMap,&sKey,&pNode) == SXRET_OK;` |
|     7 | 6706 | `	}` |
|    15 | 6707 | `	PH7_MemObjRelease(&sObj);` |
|    15 | 6708 | `	PH7_MemObjRelease(&sKey);` |
|    15 | 6709 | `	return rc;` |
|     1 | 6710 | `}` |
|     - | 6711 | ``/* php's `zend_hash_internal_pointer_reset_ex(&storage, &pos); index = 0`. */`` |
|    36 | 6712 | `static void SosRewind(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 | 6713 | `{` |
|    37 | 6714 | `	ph7_hashmap *pMap = SosMap(pVm,pThis);` |
|    37 | 6715 | `	if( pMap ){` |
|    37 | 6716 | `		pMap->pCur = pMap->pFirst;` |
|    18 | 6717 | `	}` |
|    37 | 6718 | `	PH7_NativeSetAttrInt(pVm,pThis,SOS_I,0);` |
|    37 | 6719 | `}` |
|     - | 6720 | `/* The pair the cursor is on, or 0 past the end. */` |
|    84 | 6721 | `static ph7_value * SosCurrentPair(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 | 6722 | `{` |
|    85 | 6723 | `	ph7_hashmap *pMap = SosMap(pVm,pThis);` |
|    85 | 6724 | `	if( pMap == 0 \|\| pMap->pCur == 0 ){` |
|     5 | 6725 | `		return 0;` |
|     - | 6726 | `	}` |
|    81 | 6727 | `	return HashmapExtractNodeValue(pMap->pCur);` |
|    43 | 6728 | `}` |
|     - | 6729 | `/*` |
|     - | 6730 | ` * A SNAPSHOT of one storage's pairs as a plain list. Every set operation walks one` |
|     - | 6731 | ` * storage while writing to another -- and either walk can run an overridden` |
|     - | 6732 | ` * getHash(), which moves things (rule 47) and can even mutate the map being walked.` |
|     - | 6733 | ` * php's own SPL_SAFE_HASH_FOREACH_PTR is the same precaution one layer down.` |
|     - | 6734 | ` */` |
|    12 | 6735 | `static sxi32 SosSnapshot(ph7_vm *pVm,ph7_class_instance *pFrom,ph7_value *pOut)` |
|     1 | 6736 | `{` |
|    13 | 6737 | `	ph7_hashmap *pMap = SosMap(pVm,pFrom);` |
|     - | 6738 | `	ph7_hashmap_node *pNode;` |
|     - | 6739 | `	sxu32 n;` |
|    13 | 6740 | `	if( PH7_MemObjToHashmap(pOut) != SXRET_OK ){` |
|   ! 0 | 6741 | `		return SXERR_MEM;` |
|     - | 6742 | `	}` |
|    13 | 6743 | `	if( pMap == 0 ){` |
|   ! 0 | 6744 | `		return SXRET_OK;` |
|     - | 6745 | `	}` |
|    13 | 6746 | `	pNode = pMap->pFirst;` |
|    37 | 6747 | `	for( n = 0 ; n < pMap->nEntry && pNode ; ++n ){` |
|    25 | 6748 | `		ph7_value *pPair = HashmapExtractNodeValue(pNode);` |
|    25 | 6749 | `		if( pPair ){` |
|    25 | 6750 | `			ph7_array_add_elem(pOut,0,pPair);` |
|    12 | 6751 | `		}` |
|    25 | 6752 | `		pNode = pNode->pPrev;   /* insertion order: pFirst, then the pPrev chain */` |
|    13 | 6753 | `	}` |
|    13 | 6754 | `	return SXRET_OK;` |
|     7 | 6755 | `}` |
|     - | 6756 | `/* One pair of a snapshot, re-resolved by index because a user call may have moved` |
|     - | 6757 | ` * every value in the pool since the last one. */` |
|    28 | 6758 | `static ph7_value * SosSnapAt(ph7_value *pSnap,sxi64 i)` |
|     1 | 6759 | `{` |
|    29 | 6760 | `	ph7_hashmap_node *pNode = 0;` |
|    28 | 6761 | `	if( (pSnap->iFlags & MEMOBJ_HASHMAP) == 0` |
|    29 | 6762 | `	 \|\| HashmapLookupIntKey((ph7_hashmap *)pSnap->x.pOther,i,&pNode) != SXRET_OK ){` |
|   ! 0 | 6763 | `		return 0;` |
|     - | 6764 | `	}` |
|    29 | 6765 | `	return HashmapExtractNodeValue(pNode);` |
|    15 | 6766 | `}` |
|   ! 0 | 6767 | `static int vm_builtin_SplObjectStorage_attach(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 6768 | `{` |
|     - | 6769 | `	ph7_value *pObj;` |
|   ! 0 | 6770 | `	sxi32 rc = SosObjectArg(pCtx,"attach",nArg,apArg,&pObj);` |
|   ! 0 | 6771 | `	if( rc != PH7_OK ){` |
|   ! 0 | 6772 | `		return rc;` |
|     - | 6773 | `	}` |
|   ! 0 | 6774 | `	return SosAttach(pCtx,PH7_ContextThis(pCtx),pObj,nArg > 1 ? apArg[1] : 0);` |
|   ! 0 | 6775 | `}` |
|     - | 6776 | `/* php's offsetSet is an @implementation-alias of attach, and its refusal says so. */` |
|   114 | 6777 | `static int vm_builtin_SplObjectStorage_offsetSet(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 6778 | `{` |
|     - | 6779 | `	ph7_value *pObj;` |
|   115 | 6780 | `	sxi32 rc = SosObjectArg(pCtx,"offsetSet",nArg,apArg,&pObj);` |
|   115 | 6781 | `	if( rc != PH7_OK ){` |
|     3 | 6782 | `		return rc;` |
|     - | 6783 | `	}` |
|   113 | 6784 | `	return SosAttach(pCtx,PH7_ContextThis(pCtx),pObj,nArg > 1 ? apArg[1] : 0);` |
|    58 | 6785 | `}` |
|     - | 6786 | `/* detach() RESTARTS the walk: php resets both the position and the index, whether` |
|     - | 6787 | ` * or not anything was removed. */` |
|     4 | 6788 | `static sxi32 SosDetachMethod(ph7_context *pCtx,const char *zMethod,int nArg,ph7_value **apArg)` |
|     1 | 6789 | `{` |
|     5 | 6790 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 6791 | `	ph7_value *pObj;` |
|     5 | 6792 | `	sxi32 rc = SosObjectArg(pCtx,zMethod,nArg,apArg,&pObj);` |
|     5 | 6793 | `	if( rc != PH7_OK ){` |
|   ! 0 | 6794 | `		return rc;` |
|     - | 6795 | `	}` |
|     5 | 6796 | `	rc = SosDetach(pCtx,pThis,pObj,0);` |
|     5 | 6797 | `	if( rc != PH7_OK ){` |
|   ! 0 | 6798 | `		return rc;` |
|     - | 6799 | `	}` |
|     5 | 6800 | `	SosRewind(pCtx->pVm,pThis);` |
|     5 | 6801 | `	return PH7_OK;` |
|     3 | 6802 | `}` |
|   ! 0 | 6803 | `static int vm_builtin_SplObjectStorage_detach(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 6804 | `{` |
|   ! 0 | 6805 | `	return SosDetachMethod(pCtx,"detach",nArg,apArg);` |
|   ! 0 | 6806 | `}` |
|     4 | 6807 | `static int vm_builtin_SplObjectStorage_offsetUnset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 6808 | `{` |
|     5 | 6809 | `	return SosDetachMethod(pCtx,"offsetUnset",nArg,apArg);` |
|     1 | 6810 | `}` |
|     8 | 6811 | `static sxi32 SosContainsMethod(ph7_context *pCtx,const char *zMethod,int nArg,ph7_value **apArg)` |
|     1 | 6812 | `{` |
|     - | 6813 | `	ph7_value *pObj;` |
|     9 | 6814 | `	int bFound = 0;` |
|     9 | 6815 | `	sxi32 rc = SosObjectArg(pCtx,zMethod,nArg,apArg,&pObj);` |
|     9 | 6816 | `	if( rc != PH7_OK ){` |
|   ! 0 | 6817 | `		return rc;` |
|     - | 6818 | `	}` |
|     9 | 6819 | `	rc = SosContains(pCtx,PH7_ContextThis(pCtx),pObj,&bFound);` |
|     9 | 6820 | `	if( rc != PH7_OK ){` |
|   ! 0 | 6821 | `		return rc;` |
|     - | 6822 | `	}` |
|     9 | 6823 | `	ph7_result_bool(pCtx,bFound);` |
|     9 | 6824 | `	return PH7_OK;` |
|     5 | 6825 | `}` |
|   ! 0 | 6826 | `static int vm_builtin_SplObjectStorage_contains(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 6827 | `{` |
|   ! 0 | 6828 | `	return SosContainsMethod(pCtx,"contains",nArg,apArg);` |
|   ! 0 | 6829 | `}` |
|     8 | 6830 | `static int vm_builtin_SplObjectStorage_offsetExists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 6831 | `{` |
|     9 | 6832 | `	return SosContainsMethod(pCtx,"offsetExists",nArg,apArg);` |
|     1 | 6833 | `}` |
|     - | 6834 | ``/* php's offsetGet: the info, or `Object not found` -- NOT null, and not false. */`` |
|    26 | 6835 | `static int vm_builtin_SplObjectStorage_offsetGet(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 6836 | `{` |
|    27 | 6837 | `	ph7_vm *pVm = pCtx->pVm;` |
|    27 | 6838 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 6839 | `	ph7_hashmap *pMap;` |
|    27 | 6840 | `	ph7_hashmap_node *pNode = 0;` |
|     - | 6841 | `	ph7_value sObj,sKey,*pObj,*pInf;` |
|    27 | 6842 | `	sxi32 rc = SosObjectArg(pCtx,"offsetGet",nArg,apArg,&pObj);` |
|    27 | 6843 | `	if( rc != PH7_OK ){` |
|     5 | 6844 | `		return rc;` |
|     - | 6845 | `	}` |
|    23 | 6846 | `	PH7_MemObjInit(pVm,&sObj);` |
|    23 | 6847 | `	PH7_MemObjInit(pVm,&sKey);` |
|    23 | 6848 | `	PH7_MemObjStore(pObj,&sObj);` |
|    23 | 6849 | `	rc = SosKey(pCtx,pThis,&sObj,&sKey);` |
|    23 | 6850 | `	if( rc == PH7_OK ){` |
|    23 | 6851 | `		pMap = SosMap(pVm,pThis);` |
|    23 | 6852 | `		if( pMap == 0 \|\| PH7_HashmapLookup(pMap,&sKey,&pNode) != SXRET_OK ){` |
|     5 | 6853 | `			rc = PH7_VmThrowException(pCtx,"UnexpectedValueException","Object not found");` |
|     3 | 6854 | `		}else{` |
|    19 | 6855 | `			pInf = SosPart(HashmapExtractNodeValue(pNode),"inf");` |
|    19 | 6856 | `			if( pInf ){` |
|    19 | 6857 | `				ph7_result_value(pCtx,pInf);` |
|    10 | 6858 | `			}else{` |
|   ! 0 | 6859 | `				ph7_result_null(pCtx);` |
|     - | 6860 | `			}` |
|     - | 6861 | `		}` |
|    11 | 6862 | `	}` |
|    23 | 6863 | `	PH7_MemObjRelease(&sObj);` |
|    23 | 6864 | `	PH7_MemObjRelease(&sKey);` |
|    23 | 6865 | `	return rc;` |
|    14 | 6866 | `}` |
|     - | 6867 | `/*` |
|     - | 6868 | ` * php's addAll: attach every pair of the other storage, then reset the INDEX only` |
|     - | 6869 | ` * -- the position is deliberately left where it stood, which is why an insert can` |
|     - | 6870 | ` * revive a walk that had run out.` |
|     - | 6871 | ` */` |
|     8 | 6872 | `static int vm_builtin_SplObjectStorage_addAll(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 6873 | `{` |
|     9 | 6874 | `	ph7_vm *pVm = pCtx->pVm;` |
|     9 | 6875 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     9 | 6876 | `	ph7_class_instance *pOther = SosOther(nArg,apArg);` |
|     - | 6877 | `	ph7_hashmap *pMap;` |
|     - | 6878 | `	ph7_value sSnap;` |
|     - | 6879 | `	sxi64 i,n;` |
|     9 | 6880 | `	sxi32 rc = PH7_OK;` |
|     9 | 6881 | `	if( pOther == 0 \|\| pThis == 0 ){` |
|   ! 0 | 6882 | `		return PH7_OK;` |
|     - | 6883 | `	}` |
|     9 | 6884 | `	PH7_MemObjInit(pVm,&sSnap);` |
|     9 | 6885 | `	if( SosSnapshot(pVm,pOther,&sSnap) != SXRET_OK ){` |
|   ! 0 | 6886 | `		PH7_MemObjRelease(&sSnap);` |
|   ! 0 | 6887 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 6888 | `	}` |
|     9 | 6889 | `	n = (sxi64)((ph7_hashmap *)sSnap.x.pOther)->nEntry;` |
|    21 | 6890 | `	for( i = 0 ; i < n && rc == PH7_OK ; ++i ){` |
|    13 | 6891 | `		ph7_value *pPair = SosSnapAt(&sSnap,i);` |
|    13 | 6892 | `		ph7_value *pObj = SosPart(pPair,"obj");` |
|    13 | 6893 | `		ph7_value *pInf = SosPart(pPair,"inf");` |
|    13 | 6894 | `		if( pObj ){` |
|    13 | 6895 | `			rc = SosAttach(pCtx,pThis,pObj,pInf);` |
|     6 | 6896 | `		}` |
|     7 | 6897 | `	}` |
|     9 | 6898 | `	PH7_MemObjRelease(&sSnap);` |
|     9 | 6899 | `	if( rc != PH7_OK ){` |
|   ! 0 | 6900 | `		return rc;` |
|     - | 6901 | `	}` |
|     9 | 6902 | `	PH7_NativeSetAttrInt(pVm,pThis,SOS_I,0);` |
|     9 | 6903 | `	pMap = SosMap(pVm,pThis);` |
|     9 | 6904 | `	ph7_result_int64(pCtx,pMap ? (ph7_int64)pMap->nEntry : 0);` |
|     9 | 6905 | `	return PH7_OK;` |
|     5 | 6906 | `}` |
|     - | 6907 | `/* php's removeAll: detach everything the other storage holds, then RESTART the walk. */` |
|     2 | 6908 | `static int vm_builtin_SplObjectStorage_removeAll(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 6909 | `{` |
|     3 | 6910 | `	ph7_vm *pVm = pCtx->pVm;` |
|     3 | 6911 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     3 | 6912 | `	ph7_class_instance *pOther = SosOther(nArg,apArg);` |
|     - | 6913 | `	ph7_hashmap *pMap;` |
|     - | 6914 | `	ph7_value sSnap;` |
|     - | 6915 | `	sxi64 i,n;` |
|     3 | 6916 | `	sxi32 rc = PH7_OK;` |
|     3 | 6917 | `	if( pOther == 0 \|\| pThis == 0 ){` |
|   ! 0 | 6918 | `		return PH7_OK;` |
|     - | 6919 | `	}` |
|     3 | 6920 | `	PH7_MemObjInit(pVm,&sSnap);` |
|     3 | 6921 | `	if( SosSnapshot(pVm,pOther,&sSnap) != SXRET_OK ){` |
|   ! 0 | 6922 | `		PH7_MemObjRelease(&sSnap);` |
|   ! 0 | 6923 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 6924 | `	}` |
|     3 | 6925 | `	n = (sxi64)((ph7_hashmap *)sSnap.x.pOther)->nEntry;` |
|     9 | 6926 | `	for( i = 0 ; i < n && rc == PH7_OK ; ++i ){` |
|     7 | 6927 | `		ph7_value *pObj = SosPart(SosSnapAt(&sSnap,i),"obj");` |
|     7 | 6928 | `		if( pObj ){` |
|     7 | 6929 | `			rc = SosDetach(pCtx,pThis,pObj,0);` |
|     3 | 6930 | `		}` |
|     4 | 6931 | `	}` |
|     3 | 6932 | `	PH7_MemObjRelease(&sSnap);` |
|     3 | 6933 | `	if( rc != PH7_OK ){` |
|   ! 0 | 6934 | `		return rc;` |
|     - | 6935 | `	}` |
|     3 | 6936 | `	SosRewind(pVm,pThis);` |
|     3 | 6937 | `	pMap = SosMap(pVm,pThis);` |
|     3 | 6938 | `	ph7_result_int64(pCtx,pMap ? (ph7_int64)pMap->nEntry : 0);` |
|     3 | 6939 | `	return PH7_OK;` |
|     2 | 6940 | `}` |
|     - | 6941 | `/* php's removeAllExcept: the INTERSECTION, walked over this storage's own pairs. */` |
|     2 | 6942 | `static int vm_builtin_SplObjectStorage_removeAllExcept(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 6943 | `{` |
|     3 | 6944 | `	ph7_vm *pVm = pCtx->pVm;` |
|     3 | 6945 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     3 | 6946 | `	ph7_class_instance *pOther = SosOther(nArg,apArg);` |
|     - | 6947 | `	ph7_hashmap *pMap;` |
|     - | 6948 | `	ph7_value sSnap;` |
|     - | 6949 | `	sxi64 i,n;` |
|     3 | 6950 | `	sxi32 rc = PH7_OK;` |
|     3 | 6951 | `	if( pOther == 0 \|\| pThis == 0 ){` |
|   ! 0 | 6952 | `		return PH7_OK;` |
|     - | 6953 | `	}` |
|     3 | 6954 | `	PH7_MemObjInit(pVm,&sSnap);` |
|     3 | 6955 | `	if( SosSnapshot(pVm,pThis,&sSnap) != SXRET_OK ){` |
|   ! 0 | 6956 | `		PH7_MemObjRelease(&sSnap);` |
|   ! 0 | 6957 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 6958 | `	}` |
|     3 | 6959 | `	n = (sxi64)((ph7_hashmap *)sSnap.x.pOther)->nEntry;` |
|     9 | 6960 | `	for( i = 0 ; i < n && rc == PH7_OK ; ++i ){` |
|     7 | 6961 | `		ph7_value *pObj = SosPart(SosSnapAt(&sSnap,i),"obj");` |
|     7 | 6962 | `		int bFound = 0;` |
|     7 | 6963 | `		if( pObj == 0 ){` |
|   ! 0 | 6964 | `			continue;` |
|     - | 6965 | `		}` |
|     7 | 6966 | `		rc = SosContains(pCtx,pOther,pObj,&bFound);` |
|     7 | 6967 | `		if( rc == PH7_OK && !bFound ){` |
|     5 | 6968 | `			pObj = SosPart(SosSnapAt(&sSnap,i),"obj");   /* re-resolved: getHash may have run */` |
|     5 | 6969 | `			if( pObj ){` |
|     5 | 6970 | `				rc = SosDetach(pCtx,pThis,pObj,0);` |
|     2 | 6971 | `			}` |
|     2 | 6972 | `		}` |
|     4 | 6973 | `	}` |
|     3 | 6974 | `	PH7_MemObjRelease(&sSnap);` |
|     3 | 6975 | `	if( rc != PH7_OK ){` |
|   ! 0 | 6976 | `		return rc;` |
|     - | 6977 | `	}` |
|     3 | 6978 | `	SosRewind(pVm,pThis);` |
|     3 | 6979 | `	pMap = SosMap(pVm,pThis);` |
|     3 | 6980 | `	ph7_result_int64(pCtx,pMap ? (ph7_int64)pMap->nEntry : 0);` |
|     3 | 6981 | `	return PH7_OK;` |
|     2 | 6982 | `}` |
|     - | 6983 | `/*` |
|     - | 6984 | ` * php's count(): COUNT_RECURSIVE is accepted and changes nothing -- the storage` |
|     - | 6985 | ` * holds C structs rather than zvals there, so nothing recurses.` |
|     - | 6986 | ` */` |
|    22 | 6987 | `static int vm_builtin_SplObjectStorage_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 6988 | `{` |
|    23 | 6989 | `	ph7_hashmap *pMap = SosMap(pCtx->pVm,PH7_ContextThis(pCtx));` |
|    11 | 6990 | `	SXUNUSED(nArg);` |
|    11 | 6991 | `	SXUNUSED(apArg);` |
|    23 | 6992 | `	ph7_result_int64(pCtx,pMap ? (ph7_int64)pMap->nEntry : 0);` |
|    23 | 6993 | `	return PH7_OK;` |
|     1 | 6994 | `}` |
|     4 | 6995 | `static int vm_builtin_SplObjectStorage_getHash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 6996 | `{` |
|     - | 6997 | `	ph7_value *pObj;` |
|     5 | 6998 | `	sxi32 rc = SosObjectArg(pCtx,"getHash",nArg,apArg,&pObj);` |
|     5 | 6999 | `	if( rc != PH7_OK ){` |
|   ! 0 | 7000 | `		return rc;` |
|     - | 7001 | `	}` |
|     - | 7002 | `	/* php's getHash() IS php_spl_object_hash(), the same one the function answers. */` |
|     5 | 7003 | `	return vm_builtin_spl_object_hash(pCtx,nArg,apArg);` |
|     3 | 7004 | `}` |
|    28 | 7005 | `static int vm_builtin_SplObjectStorage_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 7006 | `{` |
|    14 | 7007 | `	SXUNUSED(nArg);` |
|    14 | 7008 | `	SXUNUSED(apArg);` |
|    29 | 7009 | `	SosRewind(pCtx->pVm,PH7_ContextThis(pCtx));` |
|    29 | 7010 | `	return PH7_OK;` |
|     1 | 7011 | `}` |
|    52 | 7012 | `static int vm_builtin_SplObjectStorage_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 7013 | `{` |
|    53 | 7014 | `	ph7_hashmap *pMap = SosMap(pCtx->pVm,PH7_ContextThis(pCtx));` |
|    26 | 7015 | `	SXUNUSED(nArg);` |
|    26 | 7016 | `	SXUNUSED(apArg);` |
|    53 | 7017 | `	ph7_result_bool(pCtx,pMap && pMap->pCur ? 1 : 0);` |
|    53 | 7018 | `	return PH7_OK;` |
|     1 | 7019 | `}` |
|     - | 7020 | `/* php's key() is the INDEX, a counter of its own: next() advances it past the end` |
|     - | 7021 | ` * too, and only rewind()/detach()/addAll() and friends put it back to zero. */` |
|    40 | 7022 | `static int vm_builtin_SplObjectStorage_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 7023 | `{` |
|    20 | 7024 | `	SXUNUSED(nArg);` |
|    20 | 7025 | `	SXUNUSED(apArg);` |
|    41 | 7026 | `	ph7_result_int64(pCtx,PH7_NativeAttrInt(PH7_ContextThis(pCtx),SOS_I));` |
|    41 | 7027 | `	return PH7_OK;` |
|     1 | 7028 | `}` |
|     - | 7029 | `/* php RAISES here rather than answering null: the chunk's null was a wrong answer` |
|     - | 7030 | `` * every `foreach` hid, because a foreach never asks past valid(). */`` |
|    46 | 7031 | `static int vm_builtin_SplObjectStorage_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 7032 | `{` |
|    47 | 7033 | `	ph7_value *pObj = SosPart(SosCurrentPair(pCtx->pVm,PH7_ContextThis(pCtx)),"obj");` |
|    23 | 7034 | `	SXUNUSED(nArg);` |
|    23 | 7035 | `	SXUNUSED(apArg);` |
|    47 | 7036 | `	if( pObj == 0 ){` |
|     3 | 7037 | `		return PH7_VmThrowException(pCtx,"RuntimeException",` |
|     - | 7038 | `			"Called current() on invalid iterator");` |
|     - | 7039 | `	}` |
|    45 | 7040 | `	ph7_result_value(pCtx,pObj);` |
|    45 | 7041 | `	return PH7_OK;` |
|    24 | 7042 | `}` |
|    44 | 7043 | `static int vm_builtin_SplObjectStorage_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 7044 | `{` |
|    45 | 7045 | `	ph7_vm *pVm = pCtx->pVm;` |
|    45 | 7046 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    45 | 7047 | `	ph7_hashmap *pMap = SosMap(pVm,pThis);` |
|    22 | 7048 | `	SXUNUSED(nArg);` |
|    22 | 7049 | `	SXUNUSED(apArg);` |
|    45 | 7050 | `	if( pMap && pMap->pCur ){` |
|    45 | 7051 | `		pMap->pCur = pMap->pCur->pPrev;` |
|    22 | 7052 | `	}` |
|    45 | 7053 | `	PH7_NativeSetAttrInt(pVm,pThis,SOS_I,PH7_NativeAttrInt(pThis,SOS_I) + 1);` |
|    45 | 7054 | `	return PH7_OK;` |
|     1 | 7055 | `}` |
|     - | 7056 | `/* php's getInfo(): null past the end, where current() raises. */` |
|    38 | 7057 | `static int vm_builtin_SplObjectStorage_getInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 7058 | `{` |
|    39 | 7059 | `	ph7_value *pInf = SosPart(SosCurrentPair(pCtx->pVm,PH7_ContextThis(pCtx)),"inf");` |
|    19 | 7060 | `	SXUNUSED(nArg);` |
|    19 | 7061 | `	SXUNUSED(apArg);` |
|    39 | 7062 | `	if( pInf ){` |
|    37 | 7063 | `		ph7_result_value(pCtx,pInf);` |
|    19 | 7064 | `	}else{` |
|     3 | 7065 | `		ph7_result_null(pCtx);` |
|     - | 7066 | `	}` |
|    39 | 7067 | `	return PH7_OK;` |
|     1 | 7068 | `}` |
|     2 | 7069 | `static int vm_builtin_SplObjectStorage_setInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 7070 | `{` |
|     3 | 7071 | `	ph7_vm *pVm = pCtx->pVm;` |
|     3 | 7072 | `	ph7_hashmap *pMap = SosMap(pVm,PH7_ContextThis(pCtx));` |
|     - | 7073 | `	ph7_value *pPair;` |
|     3 | 7074 | `	if( pMap == 0 \|\| pMap->pCur == 0 \|\| nArg < 1 ){` |
|   ! 0 | 7075 | `		return PH7_OK;   /* php returns without touching anything */` |
|     - | 7076 | `	}` |
|     3 | 7077 | `	pPair = SosPairForWrite(pVm,pMap->pCur);` |
|     3 | 7078 | `	if( pPair ){` |
|     3 | 7079 | `		SosSetPart(pVm,pPair,"inf",apArg[0]);` |
|     1 | 7080 | `	}` |
|     3 | 7081 | `	return PH7_OK;` |
|     2 | 7082 | `}` |
|     - | 7083 | `/*` |
|     - | 7084 | ` * php's seek(): a position outside the storage is an OutOfBoundsException, and` |
|     - | 7085 | ` * the index follows the position exactly (php walks its hash cursor either way` |
|     - | 7086 | ` * and counts; the destination is the same).` |
|     - | 7087 | ` */` |
|     4 | 7088 | `static int vm_builtin_SplObjectStorage_seek(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 7089 | `{` |
|     5 | 7090 | `	ph7_vm *pVm = pCtx->pVm;` |
|     5 | 7091 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     5 | 7092 | `	ph7_hashmap *pMap = SosMap(pVm,pThis);` |
|     5 | 7093 | `	ph7_int64 iPos = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;` |
|     - | 7094 | `	ph7_int64 i;` |
|     5 | 7095 | `	if( pMap == 0 ){` |
|   ! 0 | 7096 | `		return PH7_OK;` |
|     - | 7097 | `	}` |
|     5 | 7098 | `	if( iPos < 0 \|\| iPos >= (ph7_int64)pMap->nEntry ){` |
|     4 | 7099 | `		return PH7_VmThrowException(pCtx,"OutOfBoundsException",` |
|     1 | 7100 | `			"Seek position %qd is out of range",iPos);` |
|     - | 7101 | `	}` |
|     3 | 7102 | `	pMap->pCur = pMap->pFirst;` |
|     7 | 7103 | `	for( i = 0 ; i < iPos && pMap->pCur ; ++i ){` |
|     5 | 7104 | `		pMap->pCur = pMap->pCur->pPrev;` |
|     3 | 7105 | `	}` |
|     3 | 7106 | `	PH7_NativeSetAttrInt(pVm,pThis,SOS_I,iPos);` |
|     3 | 7107 | `	return PH7_OK;` |
|     3 | 7108 | `}` |
|     - | 7109 | `/*` |
|     - | 7110 | ` * php's get_debug_info: ONE entry, the storage, under its own MANGLED private key` |
|     - | 7111 | `` * -- `["storage":"SplObjectStorage":private]` on screen. The pairs are re-indexed`` |
|     - | 7112 | ` * from zero and shown as {obj, inf}, which is the shape stored here already.` |
|     - | 7113 | `` * `__debugInfo()` is the same array, reachable by name because php declares it.`` |
|     - | 7114 | ` */` |
|     6 | 7115 | `static sxi32 SosFillDebug(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut)` |
|     1 | 7116 | `{` |
|     7 | 7117 | `	ph7_hashmap *pMap = SosMap(pVm,pThis);` |
|     - | 7118 | `	ph7_hashmap_node *pNode;` |
|     - | 7119 | `	ph7_value sKey,sList;` |
|     - | 7120 | `	sxu32 n;` |
|     7 | 7121 | `	PH7_MemObjInit(pVm,&sList);` |
|     7 | 7122 | `	if( PH7_MemObjToHashmap(&sList) != SXRET_OK ){` |
|   ! 0 | 7123 | `		PH7_MemObjRelease(&sList);` |
|   ! 0 | 7124 | `		return SXERR_MEM;` |
|     - | 7125 | `	}` |
|     7 | 7126 | `	if( pMap ){` |
|     7 | 7127 | `		pNode = pMap->pFirst;` |
|    13 | 7128 | `		for( n = 0 ; n < pMap->nEntry && pNode ; ++n ){` |
|     7 | 7129 | `			ph7_value *pPair = HashmapExtractNodeValue(pNode);` |
|     7 | 7130 | `			if( pPair ){` |
|     7 | 7131 | `				ph7_array_add_elem(&sList,0,pPair);` |
|     3 | 7132 | `			}` |
|     7 | 7133 | `			pNode = pNode->pPrev;` |
|     4 | 7134 | `		}` |
|     3 | 7135 | `	}` |
|     7 | 7136 | `	PH7_MemObjInitFromString(pVm,&sKey,0);` |
|     7 | 7137 | `	PH7_MemObjStringAppend(&sKey,"\0SplObjectStorage\0storage",` |
|     - | 7138 | `		sizeof("\0SplObjectStorage\0storage")-1);` |
|     7 | 7139 | `	ph7_array_add_elem(pOut,&sKey,&sList);` |
|     7 | 7140 | `	PH7_MemObjRelease(&sKey);` |
|     7 | 7141 | `	PH7_MemObjRelease(&sList);` |
|     7 | 7142 | `	return PH7_OK;` |
|     4 | 7143 | `}` |
|     8 | 7144 | `static sxi32 SosPresent(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)` |
|     1 | 7145 | `{` |
|     9 | 7146 | `	if( !bDebug ){` |
|     5 | 7147 | `		return PH7_OK;   /* php's (array) cast and var_export show nothing */` |
|     - | 7148 | `	}` |
|     5 | 7149 | `	return SosFillDebug(pVm,pThis,pOut);` |
|     5 | 7150 | `}` |
|     2 | 7151 | `static int vm_builtin_SplObjectStorage_debugInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 7152 | `{` |
|     3 | 7153 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 7154 | `	ph7_value sOut;` |
|     1 | 7155 | `	SXUNUSED(nArg);` |
|     1 | 7156 | `	SXUNUSED(apArg);` |
|     3 | 7157 | `	PH7_MemObjInit(pVm,&sOut);` |
|     3 | 7158 | `	if( PH7_MemObjToHashmap(&sOut) != SXRET_OK ){` |
|   ! 0 | 7159 | `		PH7_MemObjRelease(&sOut);` |
|   ! 0 | 7160 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 7161 | `	}` |
|     3 | 7162 | `	SosFillDebug(pVm,PH7_ContextThis(pCtx),&sOut);` |
|     3 | 7163 | `	ph7_result_value(pCtx,&sOut);` |
|     3 | 7164 | `	PH7_MemObjRelease(&sOut);` |
|     3 | 7165 | `	return PH7_OK;` |
|     2 | 7166 | `}` |
|     - | 7167 | `/*` |
|     - | 7168 | ` * php's __serialize(): [[obj, inf, obj, inf, …], members]. This is what serialize()` |
|     - | 7169 | ` * actually uses; the Serializable pair below is the legacy format nothing else in` |
|     - | 7170 | ` * php reads or writes. The members slot is the instance's own properties — empty for` |
|     - | 7171 | ` * a bare SplObjectStorage, a SUBCLASS's declared slots when there is one.` |
|     - | 7172 | ` */` |
|    12 | 7173 | `static int vm_builtin_SplObjectStorage_serializeMagic(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 7174 | `{` |
|    13 | 7175 | `	ph7_vm *pVm = pCtx->pVm;` |
|    13 | 7176 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    13 | 7177 | `	ph7_hashmap *pMap = SosMap(pVm,pThis);` |
|     - | 7178 | `	ph7_hashmap_node *pNode;` |
|     - | 7179 | `	ph7_value sOut,sFlat,sMembers;` |
|     - | 7180 | `	sxu32 n;` |
|     6 | 7181 | `	SXUNUSED(nArg);` |
|     6 | 7182 | `	SXUNUSED(apArg);` |
|    13 | 7183 | `	PH7_MemObjInit(pVm,&sOut);` |
|    13 | 7184 | `	PH7_MemObjInit(pVm,&sFlat);` |
|    12 | 7185 | `	if( SplMembersOf(pVm,pThis,&sMembers) != SXRET_OK` |
|    12 | 7186 | `	 \|\| PH7_MemObjToHashmap(&sOut) != SXRET_OK` |
|    13 | 7187 | `	 \|\| PH7_MemObjToHashmap(&sFlat) != SXRET_OK ){` |
|   ! 0 | 7188 | `		PH7_MemObjRelease(&sOut);` |
|   ! 0 | 7189 | `		PH7_MemObjRelease(&sFlat);` |
|   ! 0 | 7190 | `		PH7_MemObjRelease(&sMembers);` |
|   ! 0 | 7191 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 7192 | `	}` |
|    13 | 7193 | `	if( pMap ){` |
|    13 | 7194 | `		pNode = pMap->pFirst;` |
|    29 | 7195 | `		for( n = 0 ; n < pMap->nEntry && pNode ; ++n ){` |
|    17 | 7196 | `			ph7_value *pPair = HashmapExtractNodeValue(pNode);` |
|    17 | 7197 | `			ph7_value *pObj = SosPart(pPair,"obj");` |
|    17 | 7198 | `			ph7_value *pInf = SosPart(pPair,"inf");` |
|    17 | 7199 | `			if( pObj ){` |
|    17 | 7200 | `				ph7_array_add_elem(&sFlat,0,pObj);` |
|    17 | 7201 | `				if( pInf ){` |
|    17 | 7202 | `					ph7_array_add_elem(&sFlat,0,pInf);` |
|     8 | 7203 | `				}` |
|     8 | 7204 | `			}` |
|    17 | 7205 | `			pNode = pNode->pPrev;` |
|     9 | 7206 | `		}` |
|     6 | 7207 | `	}` |
|    13 | 7208 | `	ph7_array_add_elem(&sOut,0,&sFlat);` |
|    13 | 7209 | `	ph7_array_add_elem(&sOut,0,&sMembers);` |
|    13 | 7210 | `	ph7_result_value(pCtx,&sOut);` |
|    13 | 7211 | `	PH7_MemObjRelease(&sOut);` |
|    13 | 7212 | `	PH7_MemObjRelease(&sFlat);` |
|    13 | 7213 | `	PH7_MemObjRelease(&sMembers);` |
|    13 | 7214 | `	return PH7_OK;` |
|     7 | 7215 | `}` |
|     2 | 7216 | `static sxi32 SosIllTyped(ph7_context *pCtx)` |
|     1 | 7217 | `{` |
|     3 | 7218 | `	return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     - | 7219 | `		"Incomplete or ill-typed serialization data");` |
|     1 | 7220 | `}` |
|    12 | 7221 | `static int vm_builtin_SplObjectStorage_unserializeMagic(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 7222 | `{` |
|    13 | 7223 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 7224 | `	ph7_hashmap *pFlat;` |
|    13 | 7225 | `	ph7_hashmap_node *pNode = 0;` |
|     - | 7226 | `	ph7_value *pStorage,*pMembers;` |
|     - | 7227 | `	sxi64 i,n;` |
|    13 | 7228 | `	sxi32 rc = PH7_OK;` |
|    13 | 7229 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_HASHMAP) == 0 \|\| pThis == 0 ){` |
|   ! 0 | 7230 | `		return SosIllTyped(pCtx);` |
|     - | 7231 | `	}` |
|    13 | 7232 | `	if( HashmapLookupIntKey((ph7_hashmap *)apArg[0]->x.pOther,0,&pNode) != SXRET_OK ){` |
|   ! 0 | 7233 | `		return SosIllTyped(pCtx);` |
|     - | 7234 | `	}` |
|    13 | 7235 | `	pStorage = HashmapExtractNodeValue(pNode);` |
|    13 | 7236 | `	pNode = 0;` |
|    13 | 7237 | `	if( HashmapLookupIntKey((ph7_hashmap *)apArg[0]->x.pOther,1,&pNode) != SXRET_OK ){` |
|   ! 0 | 7238 | `		return SosIllTyped(pCtx);` |
|     - | 7239 | `	}` |
|    13 | 7240 | `	pMembers = HashmapExtractNodeValue(pNode);` |
|    12 | 7241 | `	if( pStorage == 0 \|\| (pStorage->iFlags & MEMOBJ_HASHMAP) == 0` |
|    12 | 7242 | `	 \|\| pMembers == 0 \|\| (pMembers->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     3 | 7243 | `		return SosIllTyped(pCtx);` |
|     - | 7244 | `	}` |
|    11 | 7245 | `	pFlat = (ph7_hashmap *)pStorage->x.pOther;` |
|    11 | 7246 | `	n = (sxi64)pFlat->nEntry;` |
|    11 | 7247 | `	if( n % 2 != 0 ){` |
|     3 | 7248 | `		return PH7_VmThrowException(pCtx,"UnexpectedValueException","Odd number of elements");` |
|     - | 7249 | `	}` |
|    19 | 7250 | `	for( i = 0 ; i < n && rc == PH7_OK ; i += 2 ){` |
|     - | 7251 | `		ph7_value *pObj,*pInf;` |
|    13 | 7252 | `		pNode = 0;` |
|    13 | 7253 | `		if( HashmapLookupIntKey(pFlat,i,&pNode) != SXRET_OK ){` |
|   ! 0 | 7254 | `			return SosIllTyped(pCtx);` |
|     - | 7255 | `		}` |
|    13 | 7256 | `		pObj = HashmapExtractNodeValue(pNode);` |
|    13 | 7257 | `		if( pObj == 0 \|\| (pObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     3 | 7258 | `			return PH7_VmThrowException(pCtx,"UnexpectedValueException","Non-object key");` |
|     - | 7259 | `		}` |
|    11 | 7260 | `		pNode = 0;` |
|    11 | 7261 | `		pInf = HashmapLookupIntKey(pFlat,i+1,&pNode) == SXRET_OK` |
|    10 | 7262 | `			? HashmapExtractNodeValue(pNode) : 0;` |
|    11 | 7263 | `		rc = SosAttach(pCtx,pThis,pObj,pInf);` |
|     6 | 7264 | `	}` |
|     7 | 7265 | `	if( rc == PH7_OK ){` |
|     7 | 7266 | `		SplMembersLoad(pThis,pMembers);` |
|     3 | 7267 | `	}` |
|     7 | 7268 | `	return rc;` |
|     7 | 7269 | `}` |
|     - | 7270 | `/*` |
|     - | 7271 | ` * php's Serializable pair, kept because the interface is still declared. The` |
|     - | 7272 | `` * format is `x:` + the serialized COUNT, then one `<obj>,<inf>;` per element, then`` |
|     - | 7273 | `` * `m:` + the serialized members -- and php writes it through ONE serializer state,`` |
|     - | 7274 | `` * so an object that appears twice becomes an `r:` back-reference there and a`` |
|     - | 7275 | ` * second copy here (§10; the DLL's legacy pair has the same shape).` |
|     - | 7276 | ` */` |
|     4 | 7277 | `static int vm_builtin_SplObjectStorage_serialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 7278 | `{` |
|     5 | 7279 | `	ph7_vm *pVm = pCtx->pVm;` |
|     5 | 7280 | `	ph7_hashmap *pMap = SosMap(pVm,PH7_ContextThis(pCtx));` |
|     - | 7281 | `	ph7_hashmap_node *pNode;` |
|     - | 7282 | `	SyBlob sOut;` |
|     - | 7283 | `	ph7_value sVal,*apCall[1];` |
|     - | 7284 | `	sxu32 n;` |
|     2 | 7285 | `	SXUNUSED(nArg);` |
|     2 | 7286 | `	SXUNUSED(apArg);` |
|     5 | 7287 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|     5 | 7288 | `	SyBlobAppend(&sOut,"x:",sizeof("x:")-1);` |
|     5 | 7289 | `	PH7_MemObjInitFromInt(pVm,&sVal,pMap ? (sxi64)pMap->nEntry : 0);` |
|     5 | 7290 | `	apCall[0] = &sVal;` |
|     5 | 7291 | `	SplSerializeInto(pCtx,apCall,&sOut);` |
|     5 | 7292 | `	PH7_MemObjRelease(&sVal);` |
|     5 | 7293 | `	if( pMap ){` |
|     5 | 7294 | `		pNode = pMap->pFirst;` |
|    13 | 7295 | `		for( n = 0 ; n < pMap->nEntry && pNode ; ++n ){` |
|     9 | 7296 | `			ph7_value *pPair = HashmapExtractNodeValue(pNode);` |
|     9 | 7297 | `			ph7_value *pObj = SosPart(pPair,"obj");` |
|     9 | 7298 | `			ph7_value *pInf = SosPart(pPair,"inf");` |
|     - | 7299 | `			ph7_value sNull;` |
|     9 | 7300 | `			if( pObj ){` |
|     9 | 7301 | `				apCall[0] = pObj;` |
|     9 | 7302 | `				SplSerializeInto(pCtx,apCall,&sOut);` |
|     9 | 7303 | `				SyBlobAppend(&sOut,",",1);` |
|     9 | 7304 | `				PH7_MemObjInit(pVm,&sNull);` |
|     9 | 7305 | `				apCall[0] = pInf ? pInf : &sNull;` |
|     9 | 7306 | `				SplSerializeInto(pCtx,apCall,&sOut);` |
|     9 | 7307 | `				PH7_MemObjRelease(&sNull);` |
|     9 | 7308 | `				SyBlobAppend(&sOut,";",1);` |
|     4 | 7309 | `			}` |
|     9 | 7310 | `			pNode = pNode->pPrev;` |
|     5 | 7311 | `		}` |
|     2 | 7312 | `	}` |
|     5 | 7313 | `	SyBlobAppend(&sOut,"m:",sizeof("m:")-1);` |
|     5 | 7314 | `	PH7_MemObjInit(pVm,&sVal);` |
|     5 | 7315 | `	if( PH7_MemObjToHashmap(&sVal) == SXRET_OK ){` |
|     5 | 7316 | `		apCall[0] = &sVal;` |
|     5 | 7317 | `		SplSerializeInto(pCtx,apCall,&sOut);` |
|     2 | 7318 | `	}` |
|     5 | 7319 | `	PH7_MemObjRelease(&sVal);` |
|     - | 7320 | `	/* ph7_result_string APPENDS too, and pRet still holds the last nested answer` |
|     - | 7321 | `	 * (rule 54) -- drop it before writing this one. */` |
|     5 | 7322 | `	if( pCtx->pRet ){` |
|     5 | 7323 | `		PH7_MemObjRelease(pCtx->pRet);` |
|     2 | 7324 | `	}` |
|     5 | 7325 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|     5 | 7326 | `	SyBlobRelease(&sOut);` |
|     5 | 7327 | `	return PH7_OK;` |
|     1 | 7328 | `}` |
|     - | 7329 | `/* php reports WHERE its parse gave up, in bytes, and every failure below is that` |
|     - | 7330 | ` * one exception. */` |
|     2 | 7331 | `static sxi32 SosOffsetErr(ph7_context *pCtx,int nAt,int nTotal)` |
|     1 | 7332 | `{` |
|     4 | 7333 | `	return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     1 | 7334 | `		"Error at offset %d of %d bytes",nAt,nTotal);` |
|     1 | 7335 | `}` |
|     6 | 7336 | `static int vm_builtin_SplObjectStorage_unserialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 7337 | `{` |
|     7 | 7338 | `	ph7_vm *pVm = pCtx->pVm;` |
|     7 | 7339 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 7340 | `	const char *zData;` |
|     7 | 7341 | `	int nData = 0,nAt = 0,nRead = 0;` |
|     - | 7342 | `	ph7_value sVal;` |
|     - | 7343 | `	sxi64 nCount,i;` |
|     - | 7344 | `	sxi32 rc;` |
|     7 | 7345 | `	if( nArg < 1 \|\| pThis == 0 ){` |
|   ! 0 | 7346 | `		return PH7_OK;` |
|     - | 7347 | `	}` |
|     7 | 7348 | `	zData = ph7_value_to_string(apArg[0],&nData);` |
|     7 | 7349 | `	if( nData < 1 ){` |
|     3 | 7350 | `		return PH7_OK;   /* php returns without touching the storage */` |
|     - | 7351 | `	}` |
|     5 | 7352 | `	if( nData < 2 \|\| zData[0] != 'x' \|\| zData[1] != ':' ){` |
|   ! 0 | 7353 | `		return SosOffsetErr(pCtx,zData[0] == 'x' ? 1 : 0,nData);` |
|     - | 7354 | `	}` |
|     5 | 7355 | `	nAt = 2;` |
|     5 | 7356 | `	PH7_MemObjInit(pVm,&sVal);` |
|     5 | 7357 | `	rc = PH7_VmUnserializeOne(pCtx,&zData[nAt],nData - nAt,&nRead,&sVal);` |
|     5 | 7358 | `	if( rc == PH7_EXCEPTION ){` |
|   ! 0 | 7359 | `		PH7_MemObjRelease(&sVal);` |
|   ! 0 | 7360 | `		return rc;` |
|     - | 7361 | `	}` |
|     5 | 7362 | `	if( rc != SXRET_OK \|\| (sVal.iFlags & MEMOBJ_INT) == 0 ){` |
|     - | 7363 | `		/* php reports where its parser STOPPED, which for a well-formed value of` |
|     - | 7364 | `		 * the wrong type is the byte after it. */` |
|     3 | 7365 | `		PH7_MemObjRelease(&sVal);` |
|     3 | 7366 | `		return SosOffsetErr(pCtx,nAt + nRead,nData);` |
|     - | 7367 | `	}` |
|     3 | 7368 | `	nCount = ph7_value_to_int64(&sVal);` |
|     3 | 7369 | `	PH7_MemObjRelease(&sVal);` |
|     3 | 7370 | `	nAt += nRead - 1;   /* php steps back onto the ';' that ends the count */` |
|     3 | 7371 | `	if( nCount < 0 ){` |
|   ! 0 | 7372 | `		return SosOffsetErr(pCtx,nAt,nData);` |
|     - | 7373 | `	}` |
|     9 | 7374 | `	for( i = 0 ; i < nCount ; ++i ){` |
|     - | 7375 | `		ph7_value sObj,sInf;` |
|     7 | 7376 | `		if( nAt >= nData \|\| zData[nAt] != ';' ){` |
|   ! 0 | 7377 | `			return SosOffsetErr(pCtx,nAt,nData);` |
|     - | 7378 | `		}` |
|     7 | 7379 | `		nAt++;` |
|     7 | 7380 | `		if( nAt >= nData \|\| (zData[nAt] != 'O' && zData[nAt] != 'C' && zData[nAt] != 'r') ){` |
|   ! 0 | 7381 | `			return SosOffsetErr(pCtx,nAt,nData);` |
|     - | 7382 | `		}` |
|     7 | 7383 | `		PH7_MemObjInit(pVm,&sObj);` |
|     7 | 7384 | `		PH7_MemObjInit(pVm,&sInf);` |
|     7 | 7385 | `		rc = PH7_VmUnserializeOne(pCtx,&zData[nAt],nData - nAt,&nRead,&sObj);` |
|     7 | 7386 | `		if( rc == PH7_EXCEPTION ){` |
|   ! 0 | 7387 | `			PH7_MemObjRelease(&sObj);` |
|   ! 0 | 7388 | `			PH7_MemObjRelease(&sInf);` |
|   ! 0 | 7389 | `			return rc;` |
|     - | 7390 | `		}` |
|     7 | 7391 | `		if( rc != SXRET_OK \|\| (sObj.iFlags & MEMOBJ_OBJ) == 0 ){` |
|   ! 0 | 7392 | `			PH7_MemObjRelease(&sObj);` |
|   ! 0 | 7393 | `			PH7_MemObjRelease(&sInf);` |
|   ! 0 | 7394 | `			return SosOffsetErr(pCtx,nAt + nRead,nData);` |
|     - | 7395 | `		}` |
|     7 | 7396 | `		nAt += nRead;` |
|     7 | 7397 | `		if( nAt < nData && zData[nAt] == ',' ){` |
|     7 | 7398 | `			nAt++;` |
|     7 | 7399 | `			rc = PH7_VmUnserializeOne(pCtx,&zData[nAt],nData - nAt,&nRead,&sInf);` |
|     7 | 7400 | `			if( rc == PH7_EXCEPTION ){` |
|   ! 0 | 7401 | `				PH7_MemObjRelease(&sObj);` |
|   ! 0 | 7402 | `				PH7_MemObjRelease(&sInf);` |
|   ! 0 | 7403 | `				return rc;` |
|     - | 7404 | `			}` |
|     7 | 7405 | `			if( rc != SXRET_OK ){` |
|   ! 0 | 7406 | `				PH7_MemObjRelease(&sObj);` |
|   ! 0 | 7407 | `				PH7_MemObjRelease(&sInf);` |
|   ! 0 | 7408 | `				return SosOffsetErr(pCtx,nAt + nRead,nData);` |
|     - | 7409 | `			}` |
|     7 | 7410 | `			nAt += nRead;` |
|     3 | 7411 | `		}` |
|     7 | 7412 | `		rc = SosAttach(pCtx,pThis,&sObj,&sInf);` |
|     7 | 7413 | `		PH7_MemObjRelease(&sObj);` |
|     7 | 7414 | `		PH7_MemObjRelease(&sInf);` |
|     7 | 7415 | `		if( rc != PH7_OK ){` |
|   ! 0 | 7416 | `			return rc;` |
|     - | 7417 | `		}` |
|     4 | 7418 | `	}` |
|     3 | 7419 | `	if( nAt >= nData \|\| zData[nAt] != ';' ){` |
|   ! 0 | 7420 | `		return SosOffsetErr(pCtx,nAt,nData);` |
|     - | 7421 | `	}` |
|     3 | 7422 | `	nAt++;` |
|     3 | 7423 | `	if( nAt + 1 >= nData \|\| zData[nAt] != 'm' \|\| zData[nAt+1] != ':' ){` |
|   ! 0 | 7424 | `		return SosOffsetErr(pCtx,nAt < nData && zData[nAt] == 'm' ? nAt + 1 : nAt,nData);` |
|     - | 7425 | `	}` |
|     3 | 7426 | `	nAt += 2;` |
|     3 | 7427 | `	PH7_MemObjInit(pVm,&sVal);` |
|     3 | 7428 | `	rc = PH7_VmUnserializeOne(pCtx,&zData[nAt],nData - nAt,&nRead,&sVal);` |
|     3 | 7429 | `	if( rc == PH7_EXCEPTION ){` |
|   ! 0 | 7430 | `		PH7_MemObjRelease(&sVal);` |
|   ! 0 | 7431 | `		return rc;` |
|     - | 7432 | `	}` |
|     3 | 7433 | `	if( rc != SXRET_OK \|\| (sVal.iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|   ! 0 | 7434 | `		PH7_MemObjRelease(&sVal);` |
|   ! 0 | 7435 | `		return SosOffsetErr(pCtx,nAt + nRead,nData);` |
|     - | 7436 | `	}` |
|     - | 7437 | `	/* php loads the members onto the object here; a native class declares none` |
|     - | 7438 | `	 * that a payload could name and PHL has no dynamic properties to create. */` |
|     3 | 7439 | `	PH7_MemObjRelease(&sVal);` |
|     3 | 7440 | `	return PH7_OK;` |
|     4 | 7441 | `}` |
|     - | 7442 | `/*` |
|     - | 7443 | ` * The declaration. Method ORDER is spl_observer.stub.php's, the two observer` |
|     - | 7444 | ` * interfaces are methodless-but-typed contracts php declares beside it, and` |
|     - | 7445 | ` * seek() is the ONE method php does not mark tentative.` |
|     - | 7446 | ` */` |
|  5146 | 7447 | `static sxi32 VmInstallSplObjectStorage(ph7_vm *pVm)` |
|     5 | 7448 | `{` |
|     - | 7449 | `	static const PH7_NativeMethodDef aObserverMethod[] = {` |
|     - | 7450 | `		{ "update", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "SplSubject $subject", "@void", 0 },` |
|     - | 7451 | `	};` |
|     - | 7452 | `	static const PH7_NativeMethodDef aSubjectMethod[] = {` |
|     - | 7453 | `		{ "attach", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "SplObserver $observer", "@void", 0 },` |
|     - | 7454 | `		{ "detach", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "SplObserver $observer", "@void", 0 },` |
|     - | 7455 | `		{ "notify", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@void", 0 },` |
|     - | 7456 | `	};` |
|     - | 7457 | `	static const PH7_NativePropDef aSosProp[] = {` |
|     - | 7458 | `		{ SOS_S, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - | 7459 | `		{ SOS_I, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - | 7460 | `	};` |
|     - | 7461 | `	static const PH7_NativeMethodDef aSosMethod[] = {` |
|     - | 7462 | `		{ "attach",          PH7_MOD_PUBLIC, "object $object, mixed $info = null", "@void",` |
|     - | 7463 | `		  vm_builtin_SplObjectStorage_attach },` |
|     - | 7464 | `		{ "detach",          PH7_MOD_PUBLIC, "object $object", "@void",` |
|     - | 7465 | `		  vm_builtin_SplObjectStorage_detach },` |
|     - | 7466 | `		{ "contains",        PH7_MOD_PUBLIC, "object $object", "@bool",` |
|     - | 7467 | `		  vm_builtin_SplObjectStorage_contains },` |
|     - | 7468 | `		{ "addAll",          PH7_MOD_PUBLIC, "SplObjectStorage $storage", "@int",` |
|     - | 7469 | `		  vm_builtin_SplObjectStorage_addAll },` |
|     - | 7470 | `		{ "removeAll",       PH7_MOD_PUBLIC, "SplObjectStorage $storage", "@int",` |
|     - | 7471 | `		  vm_builtin_SplObjectStorage_removeAll },` |
|     - | 7472 | `		{ "removeAllExcept", PH7_MOD_PUBLIC, "SplObjectStorage $storage", "@int",` |
|     - | 7473 | `		  vm_builtin_SplObjectStorage_removeAllExcept },` |
|     - | 7474 | `		{ "getInfo",         PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplObjectStorage_getInfo },` |
|     - | 7475 | `		{ "setInfo",         PH7_MOD_PUBLIC, "mixed $info", "@void",` |
|     - | 7476 | `		  vm_builtin_SplObjectStorage_setInfo },` |
|     - | 7477 | `		{ "count",           PH7_MOD_PUBLIC, "int $mode = 0", "@int",` |
|     - | 7478 | `		  vm_builtin_SplObjectStorage_count },` |
|     - | 7479 | `		{ "rewind",          PH7_MOD_PUBLIC, "", "@void", vm_builtin_SplObjectStorage_rewind },` |
|     - | 7480 | `		{ "valid",           PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplObjectStorage_valid },` |
|     - | 7481 | `		{ "key",             PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplObjectStorage_key },` |
|     - | 7482 | `		{ "current",         PH7_MOD_PUBLIC, "", "@object", vm_builtin_SplObjectStorage_current },` |
|     - | 7483 | `		{ "next",            PH7_MOD_PUBLIC, "", "@void", vm_builtin_SplObjectStorage_next },` |
|     - | 7484 | `		{ "seek",            PH7_MOD_PUBLIC, "int $offset", "void",` |
|     - | 7485 | `		  vm_builtin_SplObjectStorage_seek },` |
|     - | 7486 | `		{ "unserialize",     PH7_MOD_PUBLIC, "string $data", "@void",` |
|     - | 7487 | `		  vm_builtin_SplObjectStorage_unserialize },` |
|     - | 7488 | `		{ "serialize",       PH7_MOD_PUBLIC, "", "@string",` |
|     - | 7489 | `		  vm_builtin_SplObjectStorage_serialize },` |
|     - | 7490 | ``		/* php's stub leaves these four offsets UNTYPED (a `@param object` docblock`` |
|     - | 7491 | `		 * Reflection does not print) while the ZPP takes an object -- so the` |
|     - | 7492 | `		 * signature says nothing and each body words its own refusal. */` |
|     - | 7493 | `		{ "offsetExists",    PH7_MOD_PUBLIC, "$object", "@bool",` |
|     - | 7494 | `		  vm_builtin_SplObjectStorage_offsetExists },` |
|     - | 7495 | `		{ "offsetGet",       PH7_MOD_PUBLIC, "$object", "@mixed",` |
|     - | 7496 | `		  vm_builtin_SplObjectStorage_offsetGet },` |
|     - | 7497 | `		{ "offsetSet",       PH7_MOD_PUBLIC, "$object, mixed $info = null", "@void",` |
|     - | 7498 | `		  vm_builtin_SplObjectStorage_offsetSet },` |
|     - | 7499 | `		{ "offsetUnset",     PH7_MOD_PUBLIC, "$object", "@void",` |
|     - | 7500 | `		  vm_builtin_SplObjectStorage_offsetUnset },` |
|     - | 7501 | `		{ "getHash",         PH7_MOD_PUBLIC, "object $object", "@string",` |
|     - | 7502 | `		  vm_builtin_SplObjectStorage_getHash },` |
|     - | 7503 | `		{ "__serialize",     PH7_MOD_PUBLIC, "", "@array",` |
|     - | 7504 | `		  vm_builtin_SplObjectStorage_serializeMagic },` |
|     - | 7505 | `		{ "__unserialize",   PH7_MOD_PUBLIC, "array $data", "@void",` |
|     - | 7506 | `		  vm_builtin_SplObjectStorage_unserializeMagic },` |
|     - | 7507 | `		{ "__debugInfo",     PH7_MOD_PUBLIC, "", "@array",` |
|     - | 7508 | `		  vm_builtin_SplObjectStorage_debugInfo },` |
|     - | 7509 | `	};` |
|     - | 7510 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - | 7511 | `		{ "SplObserver", 0, 0, PH7_CLASS_INTERFACE,` |
|     - | 7512 | `		  aObserverMethod, SX_ARRAYSIZE(aObserverMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - | 7513 | `		{ "SplSubject", 0, 0, PH7_CLASS_INTERFACE,` |
|     - | 7514 | `		  aSubjectMethod, SX_ARRAYSIZE(aSubjectMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - | 7515 | `		{ "SplObjectStorage", 0, "Countable,SeekableIterator,Serializable,ArrayAccess", 0,` |
|     - | 7516 | `		  aSosMethod, SX_ARRAYSIZE(aSosMethod), 0, 0,` |
|     - | 7517 | `		  aSosProp, SX_ARRAYSIZE(aSosProp), 0, 0, SosPresent },` |
|     - | 7518 | `	};` |
|  5151 | 7519 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|     5 | 7520 | `}` |
|     - | 7521 | `/*` |
|     - | 7522 | ` * ---------------------------------------------------------------------------` |
|     - | 7523 | ` * SplFileInfo.` |
|     - | 7524 | ` *` |
|     - | 7525 | `` * php's `spl_filesystem_object` keeps TWO strings for a path, and which one a`` |
|     - | 7526 | `` * method reads is the whole model: `file_name` is the pathname with any trailing`` |
|     - | 7527 | `` * slashes stripped, and `path` is everything before the LAST slash of it -- which`` |
|     - | 7528 | ` * is EMPTY when the name has no slash before its last component, so` |
|     - | 7529 | `` * `(new SplFileInfo('/a.txt'))->getPath()` is `''` and `getFilename()` answers the`` |
|     - | 7530 | `` * whole `/a.txt`. Every accessor is a slice of that pair (php's own`` |
|     - | 7531 | `` * `spl_filesystem_info_set_filename`), and the chunk, which called `basename()` and`` |
|     - | 7532 | `` * `dirname()` per method instead, disagreed on all of it.`` |
|     - | 7533 | ` *` |
|     - | 7534 | `` * The stat family is php's `FileInfoFunction` macro: `php_stat()` with the error`` |
|     - | 7535 | ` * handler REPLACED, so the warning a failed stat would print becomes a` |
|     - | 7536 | `` * `RuntimeException` instead -- `getSize()` on a missing file RAISES there and`` |
|     - | 7537 | ` * warned-then-answered-false here. Two of the fifteen lstat rather than stat` |
|     - | 7538 | `` * (`getType`, `isLink`), which is php's IS_LINK_OPERATION set.`` |
|     - | 7539 | ` *` |
|     - | 7540 | ` * The two slots are PRIVATE and PRESENTED: php declares no properties at all` |
|     - | 7541 | `` * (`getProperties()`, the `(array)` cast and `get_object_vars()` are empty) while`` |
|     - | 7542 | `` * `var_dump` shows `pathName`/`fileName` under their mangled private keys, and`` |
|     - | 7543 | `` * `__debugInfo()` hands back that same array. The class is `@not-serializable`.`` |
|     - | 7544 | ` *` |
|     - | 7545 | `` * NOT converted, because they need a class PHL does not have: `openFile()` and`` |
|     - | 7546 | `` * `setFileClass()` answer an `SplFileObject`. `setInfoClass()` and the `?string`` |
|     - | 7547 | `` * $class` argument of `getFileInfo()`/`getPathInfo()` are here in full -- they`` |
|     - | 7548 | ` * only ever name a class derived from this one.` |
|     - | 7549 | ` */` |
|     - | 7550 | `#define SFI_N  "__n"   /* php's file_name: the pathname, trailing slashes stripped */` |
|     - | 7551 | `#define SFI_P  "__p"   /* php's path: everything before its last slash */` |
|     - | 7552 | `#define SFI_IC "__ic"  /* php's info_class */` |
|     - | 7553 | `` /* The directory-iterator half of php's struct, on the same instance: its `u.dir` `` |
|     - | 7554 | ` * arm minus the handle, which cannot live in a php-visible slot (see VmDirHandle).` |
|     - | 7555 | `` * Declared by DirectoryIterator, so `SplDirIs()` is what tells the two apart. */`` |
|     - | 7556 | `#define SDI_E  "__e"   /* php's u.dir.entry.d_name; "" once the walk has run out */` |
|     - | 7557 | `#define SDI_I  "__i"   /* php's u.dir.index: what key() answers */` |
|     - | 7558 | `#define SDI_F  "__f"   /* php's flags */` |
|     - | 7559 | `#define SDI_S  "__s"   /* php's u.dir.sub_path (RecursiveDirectoryIterator) */` |
|     - | 7560 |  |
|     - | 7561 | `/* php's IS_SLASH is PLATFORM-dependent: a backslash separates on Windows and is an` |
|     - | 7562 | ``  * ordinary filename byte everywhere else, which is why `new SplFileInfo('C:\\x\\y')` `` |
|     - | 7563 | ` * has an empty path on unix. PH7_ExtractDirName draws the same line. */` |
|     - | 7564 | `#ifdef __WINNT__` |
|     - | 7565 | `# define SFI_IS_SLASH(c) ((c) == '/' \|\| (c) == '\\')` |
|     - | 7566 | `#else` |
|     - | 7567 | `# define SFI_IS_SLASH(c) ((c) == '/')` |
|     - | 7568 | `#endif` |
|     - | 7569 |  |
|     - | 7570 | `/*` |
|     - | 7571 | ` * The directory-iterator half of this family, declared up here because php's` |
|     - | 7572 | `` * SplFileInfo bodies BRANCH on `spl_filesystem_object::type`: a DIR instance`` |
|     - | 7573 | ` * keeps its pathname lazily (path + slash + the current entry, rebuilt after` |
|     - | 7574 | ` * every read) and answers nothing at all once the walk has run out. Exactly` |
|     - | 7575 | ` * five accessors below ask, which is the same five php branches in.` |
|     - | 7576 | ` */` |
|     - | 7577 | `static int SplDirIs(ph7_vm *pVm,ph7_class_instance *pThis);` |
|     - | 7578 | `static const char * SplDirName(ph7_vm *pVm,ph7_class_instance *pThis,int *pnLen);` |
|     - | 7579 | `static int SplDirAtEnd(ph7_class_instance *pThis);` |
|     - | 7580 | `static VmDirHandle * SplDirState(ph7_vm *pVm,ph7_class_instance *pThis);` |
|     - | 7581 | `/* One of the two path slots, as bytes. */` |
|  1047 | 7582 | `static const char * SfiStr(ph7_class_instance *pThis,const char *zSlot,int *pnLen)` |
|     1 | 7583 | `{` |
|  1048 | 7584 | `	ph7_value *pVal = pThis ? PH7_NativeAttr(pThis,zSlot) : 0;` |
|  1048 | 7585 | `	*pnLen = 0;` |
|  1048 | 7586 | `	if( pVal == 0 ){` |
|   ! 0 | 7587 | `		return "";` |
|     - | 7588 | `	}` |
|  1048 | 7589 | `	return ph7_value_to_string(pVal,pnLen);` |
|   526 | 7590 | `}` |
|     - | 7591 | `/*` |
|     - | 7592 | ` * php's spl_filesystem_info_set_filename: strip the trailing slashes (never the` |
|     - | 7593 | ` * only character), then cut the path at the last slash of what is left. A name` |
|     - | 7594 | ` * with no slash before its final component keeps an EMPTY path, which is what` |
|     - | 7595 | ` * makes getFilename() answer the whole thing.` |
|     - | 7596 | ` */` |
|    72 | 7597 | `static void SfiSetName(ph7_vm *pVm,ph7_class_instance *pThis,const char *zPath,int nPath)` |
|     1 | 7598 | `{` |
|    73 | 7599 | `	int nFile = nPath;` |
|     - | 7600 | `	int nDir;` |
|    73 | 7601 | `	if( nFile > 1 && SFI_IS_SLASH(zPath[nFile-1]) ){` |
|     4 | 7602 | `		do{` |
|     9 | 7603 | `			nFile--;` |
|     9 | 7604 | `		}while( nFile > 1 && SFI_IS_SLASH(zPath[nFile-1]) );` |
|     4 | 7605 | `	}` |
|    73 | 7606 | `	nDir = nFile;` |
|   468 | 7607 | `	while( nDir > 1 && !SFI_IS_SLASH(zPath[nDir-1]) ){` |
|   396 | 7608 | `		nDir--;` |
|     1 | 7609 | `	}` |
|    73 | 7610 | `	if( nDir > 0 ){` |
|    69 | 7611 | `		nDir--;` |
|    34 | 7612 | `	}` |
|    73 | 7613 | `	PH7_NativeSetAttrStr(pVm,pThis,SFI_N,zPath,nFile);` |
|    73 | 7614 | `	PH7_NativeSetAttrStr(pVm,pThis,SFI_P,zPath,nDir);` |
|    73 | 7615 | `}` |
|     - | 7616 | `/*` |
|     - | 7617 | `` * php's `file_name`: the slot for a plain SplFileInfo, and the lazily rebuilt`` |
|     - | 7618 | ` * path+slash+entry for a directory iterator. Every accessor that works on the` |
|     - | 7619 | ` * whole pathname goes through here.` |
|     - | 7620 | ` */` |
|   302 | 7621 | `static const char * SfiName(ph7_vm *pVm,ph7_class_instance *pThis,int *pnLen)` |
|     1 | 7622 | `{` |
|   303 | 7623 | `	if( SplDirIs(pVm,pThis) ){` |
|    71 | 7624 | `		return SplDirName(pVm,pThis,pnLen);` |
|     - | 7625 | `	}` |
|   233 | 7626 | `	return SfiStr(pThis,SFI_N,pnLen);` |
|   152 | 7627 | `}` |
|     - | 7628 | `/*` |
|     - | 7629 | `` * php's "the file name without the path": the slice after `path` + its slash when`` |
|     - | 7630 | ` * the path is a real prefix, and the whole name otherwise. getFilename(),` |
|     - | 7631 | ` * getBasename() and getExtension() all start here.` |
|     - | 7632 | ` */` |
|    92 | 7633 | `static const char * SfiTail(ph7_vm *pVm,ph7_class_instance *pThis,int *pnLen)` |
|     1 | 7634 | `{` |
|    93 | 7635 | `	int nName = 0,nPath = 0;` |
|    93 | 7636 | `	const char *zName = SfiName(pVm,pThis,&nName);` |
|    93 | 7637 | `	SfiStr(pThis,SFI_P,&nPath);` |
|    93 | 7638 | `	if( nPath > 0 && nPath < nName ){` |
|    39 | 7639 | `		*pnLen = nName - (nPath + 1);` |
|    39 | 7640 | `		return &zName[nPath + 1];` |
|     - | 7641 | `	}` |
|    55 | 7642 | `	*pnLen = nName;` |
|    55 | 7643 | `	return zName;` |
|    47 | 7644 | `}` |
|     - | 7645 | `/* The path this instance stands for, as a NUL-terminated buffer the VFS can take. */` |
|    72 | 7646 | `static sxi32 SfiPathBuf(ph7_vm *pVm,ph7_class_instance *pThis,char *zBuf,int nBuf)` |
|     1 | 7647 | `{` |
|    73 | 7648 | `	int nName = 0;` |
|    73 | 7649 | `	const char *zName = SfiName(pVm,pThis,&nName);` |
|    73 | 7650 | `	if( nName < 1 \|\| nName >= nBuf ){` |
|   ! 0 | 7651 | `		return SXERR_INVALID;` |
|     - | 7652 | `	}` |
|    73 | 7653 | `	SyMemcpy(zName,zBuf,(sxu32)nName);` |
|    73 | 7654 | `	zBuf[nName] = 0;` |
|    73 | 7655 | `	return SXRET_OK;` |
|    37 | 7656 | `}` |
|     - | 7657 | `/*` |
|     - | 7658 | ` * php's get_file_name() ahead of an accessor that needs a path: an object whose` |
|     - | 7659 | ` * parent constructor never ran has no name AT ALL and raises Error rather than` |
|     - | 7660 | ` * failing a stat -- which for a directory iterator is the case where the open` |
|     - | 7661 | ` * never happened. Answers 0 when the caller must return *pRc.` |
|     - | 7662 | ` */` |
|    72 | 7663 | `static int SfiDirReady(ph7_context *pCtx,sxi32 *pRc)` |
|     1 | 7664 | `{` |
|    73 | 7665 | `	ph7_vm *pVm = pCtx->pVm;` |
|    73 | 7666 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    73 | 7667 | `	*pRc = PH7_OK;` |
|    73 | 7668 | `	if( SplDirIs(pVm,pThis) && SplDirState(pVm,pThis) == 0 ){` |
|     7 | 7669 | `		*pRc = PH7_VmThrowException(pCtx,"Error","Object not initialized");` |
|     7 | 7670 | `		return 0;` |
|     - | 7671 | `	}` |
|    67 | 7672 | `	return 1;` |
|    37 | 7673 | `}` |
|     - | 7674 | `/*` |
|     - | 7675 | ` * php's FileInfoFunction: the stat that backs one accessor, with the failure` |
|     - | 7676 | ` * promoted to a RuntimeException carrying the WARNING php would otherwise print.` |
|     - | 7677 | ` * The two lstat users say "Lstat failed" there, which is php's own text.` |
|     - | 7678 | ` */` |
|    38 | 7679 | `static sxi32 SfiStat(ph7_context *pCtx,const char *zMethod,int bLstat,ph7_value *pOut)` |
|     1 | 7680 | `{` |
|    39 | 7681 | `	ph7_vm *pVm = pCtx->pVm;` |
|    39 | 7682 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    39 | 7683 | `	const ph7_vfs *pVfs = pVm->pEngine->pVfs;` |
|     - | 7684 | `	ph7_value sWorker;` |
|     - | 7685 | `	char zPath[4096];` |
|    39 | 7686 | `	int rc = -1;` |
|     - | 7687 | `	sxi32 rcReady;` |
|    39 | 7688 | `	if( !SfiDirReady(pCtx,&rcReady) ){` |
|     3 | 7689 | `		return rcReady;` |
|     - | 7690 | `	}` |
|    37 | 7691 | `	if( PH7_MemObjToHashmap(pOut) != SXRET_OK ){` |
|   ! 0 | 7692 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 7693 | `	}` |
|    37 | 7694 | `	PH7_MemObjInit(pVm,&sWorker);` |
|    37 | 7695 | `	if( SfiPathBuf(pVm,pThis,zPath,(int)sizeof(zPath)) == SXRET_OK && pVfs ){` |
|    37 | 7696 | `		if( bLstat ){` |
|   ! 0 | 7697 | `			rc = pVfs->xlStat ? pVfs->xlStat(zPath,pOut,&sWorker) : -1;` |
|   ! 0 | 7698 | `		}else{` |
|    37 | 7699 | `			rc = pVfs->xStat ? pVfs->xStat(zPath,pOut,&sWorker) : -1;` |
|     - | 7700 | `		}` |
|    18 | 7701 | `	}` |
|    37 | 7702 | `	PH7_MemObjRelease(&sWorker);` |
|    37 | 7703 | `	if( rc != PH7_OK ){` |
|    17 | 7704 | `		int nName = 0;` |
|    17 | 7705 | `		const char *zName = SfiName(pVm,pThis,&nName);` |
|    25 | 7706 | `		return PH7_VmThrowException(pCtx,"RuntimeException",` |
|     8 | 7707 | `			"SplFileInfo::%s(): %s failed for %.*s",zMethod,bLstat ? "Lstat" : "stat",` |
|     8 | 7708 | `			nName,zName);` |
|     - | 7709 | `	}` |
|    21 | 7710 | `	return PH7_OK;` |
|    20 | 7711 | `}` |
|     - | 7712 | `/* One field of a stat array, as php's int. */` |
|    38 | 7713 | `static int SfiStatField(ph7_context *pCtx,const char *zMethod,int bLstat,const char *zField,` |
|     - | 7714 | `	sxi64 *piOut)` |
|     1 | 7715 | `{` |
|     - | 7716 | `	ph7_value sStat,*pField;` |
|     - | 7717 | `	sxi32 rc;` |
|    39 | 7718 | `	*piOut = 0;` |
|    39 | 7719 | `	PH7_MemObjInit(pCtx->pVm,&sStat);` |
|    39 | 7720 | `	rc = SfiStat(pCtx,zMethod,bLstat,&sStat);` |
|    39 | 7721 | `	if( rc != PH7_OK ){` |
|    19 | 7722 | `		PH7_MemObjRelease(&sStat);` |
|    19 | 7723 | `		return rc;` |
|     - | 7724 | `	}` |
|    21 | 7725 | `	pField = ph7_array_fetch(&sStat,zField,(int)SyStrlen(zField));` |
|    21 | 7726 | `	if( pField ){` |
|    21 | 7727 | `		*piOut = ph7_value_to_int64(pField);` |
|    10 | 7728 | `	}` |
|    21 | 7729 | `	PH7_MemObjRelease(&sStat);` |
|    21 | 7730 | `	return PH7_OK;` |
|    20 | 7731 | `}` |
|     - | 7732 | `/* The eight stat accessors that answer an int, all with the same body. */` |
|    38 | 7733 | `static int SfiStatInt(ph7_context *pCtx,const char *zMethod,const char *zField)` |
|     1 | 7734 | `{` |
|    39 | 7735 | `	sxi64 iVal = 0;` |
|    39 | 7736 | `	sxi32 rc = SfiStatField(pCtx,zMethod,FALSE,zField,&iVal);` |
|    39 | 7737 | `	if( rc != PH7_OK ){` |
|    19 | 7738 | `		return rc;` |
|     - | 7739 | `	}` |
|    21 | 7740 | `	ph7_result_int64(pCtx,iVal);` |
|    21 | 7741 | `	return PH7_OK;` |
|    20 | 7742 | `}` |
|    60 | 7743 | `static int vm_builtin_SplFileInfo_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 7744 | `{` |
|    61 | 7745 | `	ph7_vm *pVm = pCtx->pVm;` |
|    61 | 7746 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 7747 | `	const char *zPath;` |
|    61 | 7748 | `	int nPath = 0;` |
|    61 | 7749 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|   ! 0 | 7750 | `		return PH7_OK;` |
|     - | 7751 | `	}` |
|    61 | 7752 | `	zPath = ph7_value_to_string(apArg[0],&nPath);` |
|    61 | 7753 | `	SfiSetName(pVm,pThis,zPath,nPath);` |
|    61 | 7754 | `	return PH7_OK;` |
|    31 | 7755 | `}` |
|    32 | 7756 | `static int vm_builtin_SplFileInfo_getPath(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 7757 | `{` |
|    33 | 7758 | `	int nPath = 0;` |
|    33 | 7759 | `	const char *zPath = SfiStr(PH7_ContextThis(pCtx),SFI_P,&nPath);` |
|    16 | 7760 | `	SXUNUSED(nArg);` |
|    16 | 7761 | `	SXUNUSED(apArg);` |
|    33 | 7762 | `	ph7_result_string(pCtx,zPath,nPath);` |
|    33 | 7763 | `	return PH7_OK;` |
|     1 | 7764 | `}` |
|     - | 7765 | `/*` |
|     - | 7766 | `` * php's getPathname() is `spl_filesystem_object_get_pathname`, and for a DIR it`` |
|     - | 7767 | ` * answers NOTHING once the walk has run out — the empty string, without` |
|     - | 7768 | ``  * materializing the lazy name the stat family would still build (`getSize()` `` |
|     - | 7769 | `` * past the end stats the directory itself, and `var_dump` shows the difference).`` |
|     - | 7770 | ` */` |
|    42 | 7771 | `static int vm_builtin_SplFileInfo_getPathname(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 7772 | `{` |
|    43 | 7773 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    43 | 7774 | `	int nName = 0;` |
|     - | 7775 | `	const char *zName;` |
|    21 | 7776 | `	SXUNUSED(nArg);` |
|    21 | 7777 | `	SXUNUSED(apArg);` |
|    43 | 7778 | `	if( SplDirIs(pCtx->pVm,pThis) && SplDirAtEnd(pThis) ){` |
|     5 | 7779 | `		ph7_result_string(pCtx,"",0);` |
|     5 | 7780 | `		return PH7_OK;` |
|     - | 7781 | `	}` |
|    39 | 7782 | `	zName = SfiName(pCtx->pVm,pThis,&nName);` |
|    39 | 7783 | `	ph7_result_string(pCtx,zName,nName);` |
|    39 | 7784 | `	return PH7_OK;` |
|    22 | 7785 | `}` |
|    30 | 7786 | `static int vm_builtin_SplFileInfo_getFilename(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 7787 | `{` |
|    31 | 7788 | `	int nTail = 0;` |
|    31 | 7789 | `	const char *zTail = SfiTail(pCtx->pVm,PH7_ContextThis(pCtx),&nTail);` |
|    15 | 7790 | `	SXUNUSED(nArg);` |
|    15 | 7791 | `	SXUNUSED(apArg);` |
|    31 | 7792 | `	ph7_result_string(pCtx,zTail,nTail);` |
|    31 | 7793 | `	return PH7_OK;` |
|     1 | 7794 | `}` |
|     - | 7795 | `/* php's getBasename(): php_basename() of the tail, suffix rule included. */` |
|    30 | 7796 | `static int vm_builtin_SplFileInfo_getBasename(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 7797 | `{` |
|    31 | 7798 | `	int nTail = 0,nBase = 0;` |
|    31 | 7799 | `	const char *zTail = SfiTail(pCtx->pVm,PH7_ContextThis(pCtx),&nTail);` |
|    31 | 7800 | `	const char *zBase = PH7_ExtractBaseName(zTail,nTail,&nBase);` |
|    31 | 7801 | `	if( nArg > 0 ){` |
|     5 | 7802 | `		int nSuffix = 0;` |
|     5 | 7803 | `		const char *zSuffix = ph7_value_to_string(apArg[0],&nSuffix);` |
|     4 | 7804 | `		if( nSuffix > 0 && nSuffix < nBase` |
|     4 | 7805 | `		 && SyMemcmp(&zBase[nBase - nSuffix],zSuffix,(sxu32)nSuffix) == 0 ){` |
|     3 | 7806 | `			nBase -= nSuffix;` |
|     1 | 7807 | `		}` |
|     2 | 7808 | `	}` |
|    31 | 7809 | `	ph7_result_string(pCtx,zBase,nBase);` |
|    31 | 7810 | `	return PH7_OK;` |
|     1 | 7811 | `}` |
|     - | 7812 | `/* php's getExtension(): everything after the LAST dot of the basename, and the` |
|     - | 7813 | ` * empty string when there is none -- a leading dot counts, so '.hidden' has the` |
|     - | 7814 | ` * extension 'hidden'. */` |
|    26 | 7815 | `static int vm_builtin_SplFileInfo_getExtension(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 7816 | `{` |
|    27 | 7817 | `	int nTail = 0,nBase = 0,i;` |
|    27 | 7818 | `	const char *zTail = SfiTail(pCtx->pVm,PH7_ContextThis(pCtx),&nTail);` |
|    27 | 7819 | `	const char *zBase = PH7_ExtractBaseName(zTail,nTail,&nBase);` |
|    13 | 7820 | `	SXUNUSED(nArg);` |
|    13 | 7821 | `	SXUNUSED(apArg);` |
|    81 | 7822 | `	for( i = nBase - 1 ; i >= 0 ; --i ){` |
|    69 | 7823 | `		if( zBase[i] == '.' ){` |
|    15 | 7824 | `			ph7_result_string(pCtx,&zBase[i+1],nBase - i - 1);` |
|    15 | 7825 | `			return PH7_OK;` |
|     - | 7826 | `		}` |
|    28 | 7827 | `	}` |
|    13 | 7828 | `	ph7_result_string(pCtx,"",0);` |
|    13 | 7829 | `	return PH7_OK;` |
|    14 | 7830 | `}` |
|     4 | 7831 | `static int vm_builtin_SplFileInfo_getPerms(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 7832 | `{` |
|     2 | 7833 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     5 | 7834 | `	return SfiStatInt(pCtx,"getPerms","mode");` |
|     1 | 7835 | `}` |
|     4 | 7836 | `static int vm_builtin_SplFileInfo_getInode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 7837 | `{` |
|     2 | 7838 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     5 | 7839 | `	return SfiStatInt(pCtx,"getInode","ino");` |
|     1 | 7840 | `}` |
|    10 | 7841 | `static int vm_builtin_SplFileInfo_getSize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 7842 | `{` |
|     5 | 7843 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    11 | 7844 | `	return SfiStatInt(pCtx,"getSize","size");` |
|     1 | 7845 | `}` |
|     4 | 7846 | `static int vm_builtin_SplFileInfo_getOwner(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 7847 | `{` |
|     2 | 7848 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     5 | 7849 | `	return SfiStatInt(pCtx,"getOwner","uid");` |
|     1 | 7850 | `}` |
|     4 | 7851 | `static int vm_builtin_SplFileInfo_getGroup(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 7852 | `{` |
|     2 | 7853 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     5 | 7854 | `	return SfiStatInt(pCtx,"getGroup","gid");` |
|     1 | 7855 | `}` |
|     4 | 7856 | `static int vm_builtin_SplFileInfo_getATime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 7857 | `{` |
|     2 | 7858 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     5 | 7859 | `	return SfiStatInt(pCtx,"getATime","atime");` |
|     1 | 7860 | `}` |
|     4 | 7861 | `static int vm_builtin_SplFileInfo_getMTime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 7862 | `{` |
|     2 | 7863 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     5 | 7864 | `	return SfiStatInt(pCtx,"getMTime","mtime");` |
|     1 | 7865 | `}` |
|     4 | 7866 | `static int vm_builtin_SplFileInfo_getCTime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 7867 | `{` |
|     2 | 7868 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     5 | 7869 | `	return SfiStatInt(pCtx,"getCTime","ctime");` |
|     1 | 7870 | `}` |
|     - | 7871 | `/*` |
|     - | 7872 | ` * php's getType() is FS_TYPE: an LSTAT, so a symlink answers "link" rather than` |
|     - | 7873 | ` * what it points at. The VFS's own xFiletype IS that question -- decoding a stat` |
|     - | 7874 | ` * mode here instead would have answered "unknown" on Windows, where the mode` |
|     - | 7875 | ` * field is not filled and the attributes are what carry the answer.` |
|     - | 7876 | ` */` |
|     8 | 7877 | `static int vm_builtin_SplFileInfo_getType(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 7878 | `{` |
|     9 | 7879 | `	const ph7_vfs *pVfs = pCtx->pVm->pEngine->pVfs;` |
|     9 | 7880 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 7881 | `	char zPath[4096];` |
|     9 | 7882 | `	int rc = -1;` |
|     - | 7883 | `	sxi32 rcReady;` |
|     4 | 7884 | `	SXUNUSED(nArg);` |
|     4 | 7885 | `	SXUNUSED(apArg);` |
|     9 | 7886 | `	if( !SfiDirReady(pCtx,&rcReady) ){` |
|     3 | 7887 | `		return rcReady;` |
|     - | 7888 | `	}` |
|     6 | 7889 | `	if( pVfs && pVfs->xFiletype` |
|     7 | 7890 | `	 && SfiPathBuf(pCtx->pVm,pThis,zPath,(int)sizeof(zPath)) == SXRET_OK ){` |
|     7 | 7891 | `		rc = pVfs->xFiletype(zPath,pCtx);` |
|     3 | 7892 | `	}` |
|     7 | 7893 | `	if( rc != PH7_OK ){` |
|     3 | 7894 | `		int nName = 0;` |
|     3 | 7895 | `		const char *zName = SfiName(pCtx->pVm,pThis,&nName);` |
|     3 | 7896 | `		if( pCtx->pRet ){` |
|     3 | 7897 | `			PH7_MemObjRelease(pCtx->pRet);   /* xFiletype wrote "unknown" (rule 54) */` |
|     1 | 7898 | `		}` |
|     4 | 7899 | `		return PH7_VmThrowException(pCtx,"RuntimeException",` |
|     1 | 7900 | `			"SplFileInfo::getType(): Lstat failed for %.*s",nName,zName);` |
|     - | 7901 | `	}` |
|     5 | 7902 | `	return PH7_OK;` |
|     5 | 7903 | `}` |
|     - | 7904 | `/* The six predicates: a VFS question each, and never a diagnostic -- php answers` |
|     - | 7905 | ` * false for a path that does not exist. */` |
|    24 | 7906 | `static int SfiPredicate(ph7_context *pCtx,int (*xTest)(const char *))` |
|     1 | 7907 | `{` |
|     - | 7908 | `	char zPath[4096];` |
|    25 | 7909 | `	int bYes = 0;` |
|     - | 7910 | `	sxi32 rcReady;` |
|    25 | 7911 | `	if( !SfiDirReady(pCtx,&rcReady) ){` |
|     3 | 7912 | `		return rcReady;` |
|     - | 7913 | `	}` |
|    22 | 7914 | `	if( xTest` |
|    23 | 7915 | `	 && SfiPathBuf(pCtx->pVm,PH7_ContextThis(pCtx),zPath,(int)sizeof(zPath)) == SXRET_OK ){` |
|    23 | 7916 | `		bYes = xTest(zPath) == PH7_OK;` |
|    11 | 7917 | `	}` |
|    23 | 7918 | `	ph7_result_bool(pCtx,bYes);` |
|    23 | 7919 | `	return PH7_OK;` |
|    13 | 7920 | `}` |
|     4 | 7921 | `static int vm_builtin_SplFileInfo_isWritable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 7922 | `{` |
|     2 | 7923 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     5 | 7924 | `	return SfiPredicate(pCtx,pCtx->pVm->pEngine->pVfs ? pCtx->pVm->pEngine->pVfs->xWritable : 0);` |
|     1 | 7925 | `}` |
|     4 | 7926 | `static int vm_builtin_SplFileInfo_isReadable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 7927 | `{` |
|     2 | 7928 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     5 | 7929 | `	return SfiPredicate(pCtx,pCtx->pVm->pEngine->pVfs ? pCtx->pVm->pEngine->pVfs->xReadable : 0);` |
|     1 | 7930 | `}` |
|     2 | 7931 | `static int vm_builtin_SplFileInfo_isExecutable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 7932 | `{` |
|     1 | 7933 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     3 | 7934 | `	return SfiPredicate(pCtx,pCtx->pVm->pEngine->pVfs ? pCtx->pVm->pEngine->pVfs->xExecutable : 0);` |
|     1 | 7935 | `}` |
|     6 | 7936 | `static int vm_builtin_SplFileInfo_isFile(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 7937 | `{` |
|     3 | 7938 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     7 | 7939 | `	return SfiPredicate(pCtx,pCtx->pVm->pEngine->pVfs ? pCtx->pVm->pEngine->pVfs->xIsfile : 0);` |
|     1 | 7940 | `}` |
|     4 | 7941 | `static int vm_builtin_SplFileInfo_isDir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 7942 | `{` |
|     2 | 7943 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     5 | 7944 | `	return SfiPredicate(pCtx,pCtx->pVm->pEngine->pVfs ? pCtx->pVm->pEngine->pVfs->xIsdir : 0);` |
|     1 | 7945 | `}` |
|     4 | 7946 | `static int vm_builtin_SplFileInfo_isLink(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 7947 | `{` |
|     2 | 7948 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     5 | 7949 | `	return SfiPredicate(pCtx,pCtx->pVm->pEngine->pVfs ? pCtx->pVm->pEngine->pVfs->xIslink : 0);` |
|     1 | 7950 | `}` |
|     - | 7951 | `/* php's getLinkTarget(): readlink(), and a RuntimeException naming the errno text` |
|     - | 7952 | ` * when it fails -- which includes asking a plain file for its target. */` |
|     2 | 7953 | `static int vm_builtin_SplFileInfo_getLinkTarget(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 7954 | `{` |
|     3 | 7955 | `	const ph7_vfs *pVfs = pCtx->pVm->pEngine->pVfs;` |
|     3 | 7956 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 7957 | `	char zPath[4096];` |
|     3 | 7958 | `	int rc = -1;` |
|     - | 7959 | `	sxi32 rcReady;` |
|     1 | 7960 | `	SXUNUSED(nArg);` |
|     1 | 7961 | `	SXUNUSED(apArg);` |
|     3 | 7962 | `	if( !SfiDirReady(pCtx,&rcReady) ){` |
|   ! 0 | 7963 | `		return rcReady;` |
|     - | 7964 | `	}` |
|     2 | 7965 | `	if( pVfs && pVfs->xReadlink` |
|     3 | 7966 | `	 && SfiPathBuf(pCtx->pVm,pThis,zPath,(int)sizeof(zPath)) == SXRET_OK ){` |
|     3 | 7967 | `		rc = pVfs->xReadlink(zPath,pCtx);` |
|     1 | 7968 | `	}` |
|     3 | 7969 | `	if( rc != PH7_OK ){` |
|     2 | 7970 | `		int nName = 0;` |
|     2 | 7971 | `		const char *zName = SfiName(pCtx->pVm,pThis,&nName);` |
|     3 | 7972 | `		return PH7_VmThrowException(pCtx,"RuntimeException",` |
|     2 | 7973 | `			"Unable to read link %.*s, error: %s",nName,zName,VfsStrerror(errno));` |
|     - | 7974 | `	}` |
|     1 | 7975 | `	return PH7_OK;` |
|     2 | 7976 | `}` |
|     4 | 7977 | `static int vm_builtin_SplFileInfo_getRealPath(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 7978 | `{` |
|     5 | 7979 | `	const ph7_vfs *pVfs = pCtx->pVm->pEngine->pVfs;` |
|     - | 7980 | `	char zPath[4096];` |
|     5 | 7981 | `	int rc = -1;` |
|     2 | 7982 | `	SXUNUSED(nArg);` |
|     2 | 7983 | `	SXUNUSED(apArg);` |
|     4 | 7984 | `	if( pVfs && pVfs->xRealpath` |
|     5 | 7985 | `	 && SfiPathBuf(pCtx->pVm,PH7_ContextThis(pCtx),zPath,(int)sizeof(zPath)) == SXRET_OK ){` |
|     5 | 7986 | `		rc = pVfs->xRealpath(zPath,pCtx);` |
|     2 | 7987 | `	}` |
|     5 | 7988 | `	if( rc != PH7_OK ){` |
|     3 | 7989 | `		ph7_result_bool(pCtx,0);   /* php answers false, with no diagnostic */` |
|     1 | 7990 | `	}` |
|     5 | 7991 | `	return PH7_OK;` |
|     1 | 7992 | `}` |
|     - | 7993 | `/*` |
|     - | 7994 | ` * The class getFileInfo()/getPathInfo() build with: the argument when it names` |
|     - | 7995 | ` * one, this instance's info_class otherwise. php refuses anything not derived` |
|     - | 7996 | ` * from SplFileInfo, and words the refusal from the ARGUMENT position.` |
|     - | 7997 | ` */` |
|    52 | 7998 | `static sxi32 SfiInfoClass(ph7_context *pCtx,const char *zMethod,ph7_value *pArg,` |
|     - | 7999 | `	ph7_class **ppOut)` |
|     1 | 8000 | `{` |
|    53 | 8001 | `	ph7_vm *pVm = pCtx->pVm;` |
|    53 | 8002 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    53 | 8003 | `	ph7_class *pBase = PH7_VmExtractClass(pVm,"SplFileInfo",sizeof("SplFileInfo")-1,FALSE,0);` |
|    53 | 8004 | `	ph7_class *pClass = 0;` |
|     - | 8005 | `	const char *zName;` |
|    53 | 8006 | `	int nName = 0;` |
|    53 | 8007 | `	if( pArg && (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|     9 | 8008 | `		zName = ph7_value_to_string(pArg,&nName);` |
|     9 | 8009 | `		pClass = PH7_VmExtractClass(pVm,zName,(sxu32)nName,TRUE,0);` |
|     9 | 8010 | `		if( pClass == 0 \|\| pBase == 0 \|\| !PH7_VmInstanceOf(pClass,pBase) ){` |
|     4 | 8011 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 8012 | `				"SplFileInfo::%s(): Argument #1 ($class) must be a class name derived "` |
|     1 | 8013 | `				"from SplFileInfo or null, %.*s given",zMethod,nName,zName);` |
|     - | 8014 | `		}` |
|     4 | 8015 | `	}else{` |
|    45 | 8016 | `		int nCur = 0;` |
|    45 | 8017 | `		const char *zCur = SfiStr(pThis,SFI_IC,&nCur);` |
|    45 | 8018 | `		pClass = PH7_VmExtractClass(pVm,zCur,(sxu32)nCur,TRUE,0);` |
|     - | 8019 | `	}` |
|    51 | 8020 | `	if( pClass == 0 ){` |
|   ! 0 | 8021 | `		pClass = pBase;` |
|   ! 0 | 8022 | `	}` |
|    51 | 8023 | `	*ppOut = pClass;` |
|    51 | 8024 | `	return pClass ? PH7_OK : PH7_ContextMemoryError(pCtx);` |
|    27 | 8025 | `}` |
|     - | 8026 | `/*` |
|     - | 8027 | ` * Build one of these for a path. php calls the CONSTRUCTOR when the class` |
|     - | 8028 | ` * declares its own (a subclass may want it) and fills the slots directly when it` |
|     - | 8029 | ` * does not -- reproduced here, because a subclass constructor is user code and` |
|     - | 8030 | ` * skipping it would be visible.` |
|     - | 8031 | ` */` |
|    42 | 8032 | `static sxi32 SfiMakeInfoEx(ph7_context *pCtx,ph7_class *pClass,const char *zPath,int nPath,` |
|     - | 8033 | `	const char *zDir,int nDir)` |
|     1 | 8034 | `{` |
|    43 | 8035 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 8036 | `	ph7_class_instance *pNew;` |
|     - | 8037 | `	ph7_class_method *pCons;` |
|    43 | 8038 | `	sxi32 rc = SXRET_OK;` |
|    43 | 8039 | `	pNew = PH7_NewClassInstance(pVm,pClass);` |
|    43 | 8040 | `	if( pNew == 0 ){` |
|   ! 0 | 8041 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 8042 | `	}` |
|    43 | 8043 | `	pNew->iRef++;` |
|    43 | 8044 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|    44 | 8045 | `	if( pCons && (pCons->sFunc.iFlags & VM_FUNC_NATIVE) == 0 ){` |
|     - | 8046 | `		ph7_value sArg,*apArg[1];` |
|     3 | 8047 | `		PH7_MemObjInitFromString(pVm,&sArg,0);` |
|     3 | 8048 | `		PH7_MemObjStringAppend(&sArg,zPath,(sxu32)nPath);` |
|     3 | 8049 | `		apArg[0] = &sArg;` |
|     3 | 8050 | `		rc = PH7_VmCallClassMethod(pVm,pNew,pCons,0,1,apArg);` |
|     3 | 8051 | `		PH7_MemObjRelease(&sArg);` |
|    42 | 8052 | `	}else if( zDir ){` |
|     - | 8053 | `		/* php's create_type for a DIR source hands the child BOTH strings rather` |
|     - | 8054 | `		 * than re-deriving the second: the path is the directory being walked, so` |
|     - | 8055 | ``		 * `new DirectoryIterator('/')`'s entry keeps the path `/` and the name`` |
|     - | 8056 | ``		 * `//x` that the walk itself produced. */`` |
|    29 | 8057 | `		PH7_NativeSetAttrStr(pVm,pNew,SFI_N,zPath,nPath);` |
|    29 | 8058 | `		PH7_NativeSetAttrStr(pVm,pNew,SFI_P,zDir,nDir);` |
|    15 | 8059 | `	}else{` |
|    13 | 8060 | `		SfiSetName(pVm,pNew,zPath,nPath);` |
|     - | 8061 | `	}` |
|    43 | 8062 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 8063 | `		PH7_ClassInstanceUnref(pNew);` |
|   ! 0 | 8064 | `		return rc;` |
|     - | 8065 | `	}` |
|    43 | 8066 | `	PH7_NativeResultObject(pCtx,pNew);` |
|    43 | 8067 | `	PH7_ClassInstanceUnref(pNew);` |
|    43 | 8068 | `	return PH7_OK;` |
|    22 | 8069 | `}` |
|     6 | 8070 | `static sxi32 SfiMakeInfo(ph7_context *pCtx,ph7_class *pClass,const char *zPath,int nPath)` |
|     1 | 8071 | `{` |
|     7 | 8072 | `	return SfiMakeInfoEx(pCtx,pClass,zPath,nPath,0,0);` |
|     1 | 8073 | `}` |
|    18 | 8074 | `static int vm_builtin_SplFileInfo_getFileInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 8075 | `{` |
|    19 | 8076 | `	ph7_vm *pVm = pCtx->pVm;` |
|    19 | 8077 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    19 | 8078 | `	ph7_class *pClass = 0;` |
|    19 | 8079 | `	int nName = 0,nDir = 0;` |
|    19 | 8080 | `	const char *zName,*zDir = 0;` |
|    19 | 8081 | `	sxi32 rc = SfiInfoClass(pCtx,"getFileInfo",nArg > 0 ? apArg[0] : 0,&pClass);` |
|    19 | 8082 | `	if( rc != PH7_OK ){` |
|     3 | 8083 | `		return rc;` |
|     - | 8084 | `	}` |
|    17 | 8085 | `	if( SplDirIs(pVm,pThis) ){` |
|     9 | 8086 | `		if( SplDirState(pVm,pThis) == 0 ){` |
|     3 | 8087 | `			return PH7_VmThrowException(pCtx,"Error","Object not initialized");` |
|     - | 8088 | `		}` |
|     - | 8089 | `		/* php's create_type refuses to describe an entry that is not there —` |
|     - | 8090 | `		 * the same RuntimeException a FilesystemIterator::current() past the end` |
|     - | 8091 | `		 * raises, because it goes through this. */` |
|     7 | 8092 | `		if( SplDirAtEnd(pThis) ){` |
|     3 | 8093 | `			return PH7_VmThrowException(pCtx,"RuntimeException","Could not open file");` |
|     - | 8094 | `		}` |
|     5 | 8095 | `		zDir = SfiStr(pThis,SFI_P,&nDir);` |
|     2 | 8096 | `	}` |
|    13 | 8097 | `	zName = SfiName(pVm,pThis,&nName);` |
|    13 | 8098 | `	return SfiMakeInfoEx(pCtx,pClass,zName,nName,zDir,nDir);` |
|    10 | 8099 | `}` |
|     - | 8100 | `/* php's getPathInfo(): the DIRNAME of the pathname, and nothing at all (null) for` |
|     - | 8101 | ` * an empty one — which for a directory iterator includes one that has run out. */` |
|    10 | 8102 | `static int vm_builtin_SplFileInfo_getPathInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 8103 | `{` |
|    11 | 8104 | `	ph7_vm *pVm = pCtx->pVm;` |
|    11 | 8105 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    11 | 8106 | `	ph7_class *pClass = 0;` |
|    11 | 8107 | `	int nName = 0,nDir = 0;` |
|     - | 8108 | `	const char *zName,*zDir;` |
|    11 | 8109 | `	sxi32 rc = SfiInfoClass(pCtx,"getPathInfo",nArg > 0 ? apArg[0] : 0,&pClass);` |
|    11 | 8110 | `	if( rc != PH7_OK ){` |
|   ! 0 | 8111 | `		return rc;` |
|     - | 8112 | `	}` |
|    11 | 8113 | `	if( SplDirIs(pVm,pThis) && SplDirAtEnd(pThis) ){` |
|     3 | 8114 | `		ph7_result_null(pCtx);` |
|     3 | 8115 | `		return PH7_OK;` |
|     - | 8116 | `	}` |
|     9 | 8117 | `	zName = SfiName(pVm,pThis,&nName);` |
|     9 | 8118 | `	if( nName < 1 ){` |
|     3 | 8119 | `		ph7_result_null(pCtx);` |
|     3 | 8120 | `		return PH7_OK;` |
|     - | 8121 | `	}` |
|     7 | 8122 | `	zDir = PH7_ExtractDirName(zName,nName,&nDir);` |
|     7 | 8123 | `	return SfiMakeInfo(pCtx,pClass,zDir,nDir);` |
|     6 | 8124 | `}` |
|     4 | 8125 | `static int vm_builtin_SplFileInfo_setInfoClass(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 8126 | `{` |
|     5 | 8127 | `	ph7_vm *pVm = pCtx->pVm;` |
|     5 | 8128 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     5 | 8129 | `	ph7_class *pBase = PH7_VmExtractClass(pVm,"SplFileInfo",sizeof("SplFileInfo")-1,FALSE,0);` |
|     - | 8130 | `	ph7_class *pClass;` |
|     5 | 8131 | `	const char *zName = "SplFileInfo";` |
|     5 | 8132 | `	int nName = (int)sizeof("SplFileInfo")-1;` |
|     5 | 8133 | `	if( nArg > 0 ){` |
|     5 | 8134 | `		zName = ph7_value_to_string(apArg[0],&nName);` |
|     2 | 8135 | `	}` |
|     5 | 8136 | `	pClass = PH7_VmExtractClass(pVm,zName,(sxu32)nName,TRUE,0);` |
|     5 | 8137 | `	if( pClass == 0 \|\| pBase == 0 \|\| !PH7_VmInstanceOf(pClass,pBase) ){` |
|     - | 8138 | `		/* php words this one WITHOUT the "or null" half getFileInfo() has: the` |
|     - | 8139 | `		 * parameter is not nullable here. */` |
|     4 | 8140 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 8141 | `			"SplFileInfo::setInfoClass(): Argument #1 ($class) must be a class name "` |
|     1 | 8142 | `			"derived from SplFileInfo, %.*s given",nName,zName);` |
|     - | 8143 | `	}` |
|     3 | 8144 | `	PH7_NativeSetAttrStr(pVm,pThis,SFI_IC,zName,nName);` |
|     3 | 8145 | `	return PH7_OK;` |
|     3 | 8146 | `}` |
|     - | 8147 | ``/* One `"\0Class\0member" => <string>` entry of a debug array. */`` |
|    20 | 8148 | `static void SfiDebugStr(ph7_vm *pVm,ph7_value *pOut,const char *zKey,int nKey,` |
|     - | 8149 | `	const char *zVal,int nVal)` |
|     1 | 8150 | `{` |
|     - | 8151 | `	ph7_value sKey,sVal;` |
|    21 | 8152 | `	PH7_MemObjInitFromString(pVm,&sKey,0);` |
|    21 | 8153 | `	PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKey);` |
|    21 | 8154 | `	PH7_MemObjInitFromString(pVm,&sVal,0);` |
|    21 | 8155 | `	PH7_MemObjStringAppend(&sVal,zVal,(sxu32)nVal);` |
|    21 | 8156 | `	ph7_array_add_elem(pOut,&sKey,&sVal);` |
|    21 | 8157 | `	PH7_MemObjRelease(&sKey);` |
|    21 | 8158 | `	PH7_MemObjRelease(&sVal);` |
|    21 | 8159 | `}` |
|     - | 8160 | `/*` |
|     - | 8161 | ` * php's get_debug_info: the two slots under their MANGLED private names, which is` |
|     - | 8162 | ` * how a class with no declared properties still shows something. __debugInfo()` |
|     - | 8163 | ` * hands back the same array.` |
|     - | 8164 | ` *` |
|     - | 8165 | `` * A DIRECTORY iterator shows two more (`glob`, always false here — PHL has no`` |
|     - | 8166 | `` * GlobIterator — and `subPathName`), and shows `fileName` only if the pathname`` |
|     - | 8167 | `` * has been MATERIALIZED: php's `if (intern->file_name)` is the lazy name's`` |
|     - | 8168 | ` * presence, so an exhausted iterator has one key fewer until something asks it` |
|     - | 8169 | ` * for a path.` |
|     - | 8170 | ` */` |
|    10 | 8171 | `static sxi32 SfiFillDebug(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut)` |
|     1 | 8172 | `{` |
|     - | 8173 | ``	/* php's test is `type == SPL_FS_DIR`, which an object whose constructor never`` |
|     - | 8174 | ``	 * ran does NOT satisfy: it shows the single `pathName` key a bare SplFileInfo`` |
|     - | 8175 | `	 * would, and neither of the two directory ones. */` |
|    11 | 8176 | `	int bDir = SplDirIs(pVm,pThis) && SplDirState(pVm,pThis) != 0;` |
|    11 | 8177 | `	int nName = 0,nTail = 0,nSub = 0;` |
|     - | 8178 | `	const char *zName;` |
|    11 | 8179 | `	int bLive = bDir ? !SplDirAtEnd(pThis) : !SplDirIs(pVm,pThis);` |
|    11 | 8180 | `	if( bLive ){` |
|     7 | 8181 | `		zName = SfiName(pVm,pThis,&nName);` |
|     4 | 8182 | `	}else{` |
|     5 | 8183 | `		zName = SfiStr(pThis,SFI_N,&nName);   /* whatever a stat left behind, or "" */` |
|     5 | 8184 | `		nName = 0;` |
|     - | 8185 | `	}` |
|    16 | 8186 | `	SfiDebugStr(pVm,pOut,"\0SplFileInfo\0pathName",` |
|     5 | 8187 | `		(int)sizeof("\0SplFileInfo\0pathName")-1,zName,nName);` |
|     - | 8188 | `	/* Re-read: the append above may have moved the slot the first read borrowed. */` |
|    11 | 8189 | `	SfiStr(pThis,SFI_N,&nName);` |
|    11 | 8190 | `	if( bLive \|\| (bDir && nName > 0) ){` |
|     7 | 8191 | `		const char *zTail = SfiTail(pVm,pThis,&nTail);` |
|    10 | 8192 | `		SfiDebugStr(pVm,pOut,"\0SplFileInfo\0fileName",` |
|     3 | 8193 | `			(int)sizeof("\0SplFileInfo\0fileName")-1,zTail,nTail);` |
|     3 | 8194 | `	}` |
|    11 | 8195 | `	if( bDir ){` |
|     - | 8196 | `		ph7_value sKey,sVal;` |
|     - | 8197 | `		const char *zSub;` |
|     5 | 8198 | `		PH7_MemObjInitFromString(pVm,&sKey,0);` |
|     5 | 8199 | `		PH7_MemObjStringAppend(&sKey,"\0DirectoryIterator\0glob",` |
|     - | 8200 | `			sizeof("\0DirectoryIterator\0glob")-1);` |
|     5 | 8201 | `		PH7_MemObjInitFromBool(pVm,&sVal,0);` |
|     5 | 8202 | `		ph7_array_add_elem(pOut,&sKey,&sVal);` |
|     5 | 8203 | `		PH7_MemObjRelease(&sKey);` |
|     5 | 8204 | `		PH7_MemObjRelease(&sVal);` |
|     5 | 8205 | `		zSub = SfiStr(pThis,SDI_S,&nSub);` |
|     7 | 8206 | `		SfiDebugStr(pVm,pOut,"\0RecursiveDirectoryIterator\0subPathName",` |
|     2 | 8207 | `			(int)sizeof("\0RecursiveDirectoryIterator\0subPathName")-1,zSub,nSub);` |
|     2 | 8208 | `	}` |
|    11 | 8209 | `	return PH7_OK;` |
|     1 | 8210 | `}` |
|     6 | 8211 | `static sxi32 SfiPresent(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)` |
|     1 | 8212 | `{` |
|     7 | 8213 | `	if( !bDebug ){` |
|     5 | 8214 | `		return PH7_OK;   /* php's (array) cast and var_export show nothing */` |
|     - | 8215 | `	}` |
|     3 | 8216 | `	return SfiFillDebug(pVm,pThis,pOut);` |
|     4 | 8217 | `}` |
|     8 | 8218 | `static int vm_builtin_SplFileInfo_debugInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 8219 | `{` |
|     9 | 8220 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 8221 | `	ph7_value sOut;` |
|     4 | 8222 | `	SXUNUSED(nArg);` |
|     4 | 8223 | `	SXUNUSED(apArg);` |
|     9 | 8224 | `	PH7_MemObjInit(pVm,&sOut);` |
|     9 | 8225 | `	if( PH7_MemObjToHashmap(&sOut) != SXRET_OK ){` |
|   ! 0 | 8226 | `		PH7_MemObjRelease(&sOut);` |
|   ! 0 | 8227 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 8228 | `	}` |
|     9 | 8229 | `	SfiFillDebug(pVm,PH7_ContextThis(pCtx),&sOut);` |
|     9 | 8230 | `	ph7_result_value(pCtx,&sOut);` |
|     9 | 8231 | `	PH7_MemObjRelease(&sOut);` |
|     9 | 8232 | `	return PH7_OK;` |
|     5 | 8233 | `}` |
|     - | 8234 | `/* php's own escape hatch for a subclass that forgot to call parent::__construct.` |
|     - | 8235 | ` * It exists to be THROWN, and php marks it deprecated rather than removing it. */` |
|   ! 0 | 8236 | `static int vm_builtin_SplFileInfo_badState(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 8237 | `{` |
|   ! 0 | 8238 | `	SXUNUSED(nArg);` |
|   ! 0 | 8239 | `	SXUNUSED(apArg);` |
|   ! 0 | 8240 | `	return PH7_VmThrowException(pCtx,"Error",` |
|     - | 8241 | `		"The parent constructor was not called: the object is in an invalid state");` |
|   ! 0 | 8242 | `}` |
|     - | 8243 | `/*` |
|     - | 8244 | ` * The declaration. Method ORDER is spl_directory.stub.php's; openFile() and` |
|     - | 8245 | ` * setFileClass() are absent because SplFileObject is (§7), and everything else is` |
|     - | 8246 | ` * php's, tentative return types included.` |
|     - | 8247 | ` */` |
|  5146 | 8248 | `static sxi32 VmInstallSplFileInfo(ph7_vm *pVm)` |
|     5 | 8249 | `{` |
|     - | 8250 | `	static const PH7_NativePropDef aSfiProp[] = {` |
|     - | 8251 | `		{ SFI_N,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },` |
|     - | 8252 | `		{ SFI_P,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },` |
|     - | 8253 | `		{ SFI_IC, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN,` |
|     - | 8254 | `		  { 0, 0, PH7_NATIVE_VAL_STRING, 0, "SplFileInfo", 0.0 }, 0 },` |
|     - | 8255 | `	};` |
|     - | 8256 | `	static const PH7_NativeMethodDef aSfiMethod[] = {` |
|     - | 8257 | `		{ "__construct",   PH7_MOD_PUBLIC, "string $filename", 0,` |
|     - | 8258 | `		  vm_builtin_SplFileInfo_construct },` |
|     - | 8259 | `		{ "getPath",       PH7_MOD_PUBLIC, "", "@string", vm_builtin_SplFileInfo_getPath },` |
|     - | 8260 | `		{ "getFilename",   PH7_MOD_PUBLIC, "", "@string", vm_builtin_SplFileInfo_getFilename },` |
|     - | 8261 | `		{ "getExtension",  PH7_MOD_PUBLIC, "", "@string", vm_builtin_SplFileInfo_getExtension },` |
|     - | 8262 | `		{ "getBasename",   PH7_MOD_PUBLIC, "string $suffix = \"\"", "@string",` |
|     - | 8263 | `		  vm_builtin_SplFileInfo_getBasename },` |
|     - | 8264 | `		{ "getPathname",   PH7_MOD_PUBLIC, "", "@string", vm_builtin_SplFileInfo_getPathname },` |
|     - | 8265 | `		{ "getPerms",      PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_SplFileInfo_getPerms },` |
|     - | 8266 | `		{ "getInode",      PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_SplFileInfo_getInode },` |
|     - | 8267 | `		{ "getSize",       PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_SplFileInfo_getSize },` |
|     - | 8268 | `		{ "getOwner",      PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_SplFileInfo_getOwner },` |
|     - | 8269 | `		{ "getGroup",      PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_SplFileInfo_getGroup },` |
|     - | 8270 | `		{ "getATime",      PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_SplFileInfo_getATime },` |
|     - | 8271 | `		{ "getMTime",      PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_SplFileInfo_getMTime },` |
|     - | 8272 | `		{ "getCTime",      PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_SplFileInfo_getCTime },` |
|     - | 8273 | `		{ "getType",       PH7_MOD_PUBLIC, "", "@string\|false", vm_builtin_SplFileInfo_getType },` |
|     - | 8274 | `		{ "isWritable",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplFileInfo_isWritable },` |
|     - | 8275 | `		{ "isReadable",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplFileInfo_isReadable },` |
|     - | 8276 | `		{ "isExecutable",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplFileInfo_isExecutable },` |
|     - | 8277 | `		{ "isFile",        PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplFileInfo_isFile },` |
|     - | 8278 | `		{ "isDir",         PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplFileInfo_isDir },` |
|     - | 8279 | `		{ "isLink",        PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplFileInfo_isLink },` |
|     - | 8280 | `		{ "getLinkTarget", PH7_MOD_PUBLIC, "", "@string\|false",` |
|     - | 8281 | `		  vm_builtin_SplFileInfo_getLinkTarget },` |
|     - | 8282 | `		{ "getRealPath",   PH7_MOD_PUBLIC, "", "@string\|false",` |
|     - | 8283 | `		  vm_builtin_SplFileInfo_getRealPath },` |
|     - | 8284 | `		{ "getFileInfo",   PH7_MOD_PUBLIC, "?string $class = null", "@SplFileInfo",` |
|     - | 8285 | `		  vm_builtin_SplFileInfo_getFileInfo },` |
|     - | 8286 | `		{ "getPathInfo",   PH7_MOD_PUBLIC, "?string $class = null", "@?SplFileInfo",` |
|     - | 8287 | `		  vm_builtin_SplFileInfo_getPathInfo },` |
|     - | 8288 | `		{ "setInfoClass",  PH7_MOD_PUBLIC, "string $class = SplFileInfo::class", "@void",` |
|     - | 8289 | `		  vm_builtin_SplFileInfo_setInfoClass },` |
|     - | 8290 | `		{ "__toString",    PH7_MOD_PUBLIC, "", "string", vm_builtin_SplFileInfo_getPathname },` |
|     - | 8291 | `		{ "__debugInfo",   PH7_MOD_PUBLIC, "", "@array", vm_builtin_SplFileInfo_debugInfo },` |
|     - | 8292 | `		/* php does NOT mark this one tentative -- it is the only method here that` |
|     - | 8293 | ``		 * prints `Return [ void ]` rather than `Tentative return [ void ]`. */`` |
|     - | 8294 | `		{ "_bad_state_ex", PH7_MOD_PUBLIC\|PH7_MOD_FINAL, "", "void",` |
|     - | 8295 | `		  vm_builtin_SplFileInfo_badState },` |
|     - | 8296 | `	};` |
|     - | 8297 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - | 8298 | `		{ "SplFileInfo", 0, "Stringable", PH7_CLASS_NOSERIALIZE,` |
|     - | 8299 | `		  aSfiMethod, SX_ARRAYSIZE(aSfiMethod), 0, 0,` |
|     - | 8300 | `		  aSfiProp, SX_ARRAYSIZE(aSfiProp), 0, 0, SfiPresent },` |
|     - | 8301 | `	};` |
|  5151 | 8302 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|     5 | 8303 | `}` |
|     - | 8304 | `/*` |
|     - | 8305 | ` * ---------------------------------------------------------------------------` |
|     - | 8306 | ` * DirectoryIterator, FilesystemIterator and RecursiveDirectoryIterator.` |
|     - | 8307 | ` *` |
|     - | 8308 | `` * php's `spl_filesystem_object` holds an OPEN directory stream and ONE entry at`` |
|     - | 8309 | `` * a time (`u.dir.dirp`, `u.dir.entry`, `u.dir.index`); the chunk read the whole`` |
|     - | 8310 | ` * directory into an array at construction, and every difference followed from` |
|     - | 8311 | `` * that one choice (rule 52). php's `rewind()` re-opens the directory and SEES A`` |
|     - | 8312 | `` * FILE CREATED SINCE, its `key()` is the read index rather than an array offset,`` |
|     - | 8313 | `` * its `seek()` walks FORWARD through the object's own valid()/next() — so a`` |
|     - | 8314 | `` * subclass overriding either is obeyed — and a `clone` opens the directory again`` |
|     - | 8315 | ` * and reads forward to the same index rather than sharing a cursor.` |
|     - | 8316 | ` *` |
|     - | 8317 | ` * The handle cannot live in a property slot, because CLONE copies slots: two` |
|     - | 8318 | ` * objects would share one directory stream and close it twice. It lives in` |
|     - | 8319 | `` * `pVm->hDirHandle` keyed by the instance, with the class's xRelease closing it,`` |
|     - | 8320 | ` * and a clone — finding no entry of its own — re-opens on first use, which IS` |
|     - | 8321 | ` * php's clone handler, deferred. The one thing that deferral costs is a clone` |
|     - | 8322 | ` * whose directory is removed before it is first used: php has the stream open` |
|     - | 8323 | ` * already and answers, PHL raises "Object not initialized" (§7).` |
|     - | 8324 | ` *` |
|     - | 8325 | `` * `file_name` is LAZY here as it is in php: the path, a slash and the current`` |
|     - | 8326 | ` * entry, invalidated by every read and rebuilt on demand. That is php-visible` |
|     - | 8327 | `` * twice over -- `getPathname()` answers "" past the end while `getSize()` stats`` |
|     - | 8328 | `` * the DIRECTORY (the join with an empty entry), and `var_dump` shows one key`` |
|     - | 8329 | ` * fewer until something has asked.` |
|     - | 8330 | ` *` |
|     - | 8331 | `` * The chunk had also INVENTED `DirectoryIterator::getFlags()` (php has no such`` |
|     - | 8332 | ``  * method; only FilesystemIterator does), inherited SplFileInfo's `__toString()` `` |
|     - | 8333 | ` * where php aliases getFilename(), and mis-stated two constants:` |
|     - | 8334 | ` * FOLLOW_SYMLINKS is 16384 (it said 512, colliding with nothing but reading as` |
|     - | 8335 | ` * false for every real flags value) and OTHER_MODE_MASK is 28672.` |
|     - | 8336 | ` * ---------------------------------------------------------------------------` |
|     - | 8337 | ` */` |
|     - | 8338 | `/* php's spl_directory.h flag set, verbatim -- the values the class constants` |
|     - | 8339 | ` * publish and the masks its accessors compare with. */` |
|     - | 8340 | `#define SDI_CURRENT_AS_FILEINFO 0x0000` |
|     - | 8341 | `#define SDI_CURRENT_AS_SELF     0x0010` |
|     - | 8342 | `#define SDI_CURRENT_AS_PATHNAME 0x0020` |
|     - | 8343 | `#define SDI_CURRENT_MODE_MASK   0x00F0` |
|     - | 8344 | `#define SDI_KEY_AS_PATHNAME     0x0000` |
|     - | 8345 | `#define SDI_KEY_AS_FILENAME     0x0100` |
|     - | 8346 | `#define SDI_KEY_MODE_MASK       0x0F00` |
|     - | 8347 | `#define SDI_SKIPDOTS            0x1000` |
|     - | 8348 | `#define SDI_UNIXPATHS           0x2000` |
|     - | 8349 | `#define SDI_FOLLOW_SYMLINKS     0x4000` |
|     - | 8350 | `#define SDI_OTHERS_MASK         0x7000` |
|     - | 8351 | `#define SDI_FLAGS_MASK (SDI_KEY_MODE_MASK\|SDI_CURRENT_MODE_MASK\|SDI_OTHERS_MASK)` |
|     - | 8352 |  |
|     - | 8353 | `/* php's DEFAULT_SLASH, and the UNIX_PATHS flag that overrides it. */` |
|    48 | 8354 | `static char SplDirSlash(sxi64 iFlags)` |
|     1 | 8355 | `{` |
|     - | 8356 | `#ifdef __WINNT__` |
|     1 | 8357 | `	return (iFlags & SDI_UNIXPATHS) ? '/' : '\\';` |
|     - | 8358 | `#else` |
|    24 | 8359 | `	SXUNUSED(iFlags);` |
|    48 | 8360 | `	return '/';` |
|     - | 8361 | `#endif` |
|     1 | 8362 | `}` |
|     - | 8363 | `/* php's spl_filesystem_is_dot. */` |
|   153 | 8364 | `static int SplDirIsDot(const char *zName,int nName)` |
|     1 | 8365 | `{` |
|   155 | 8366 | `	return (nName == 1 && zName[0] == '.')` |
|   173 | 8367 | `		\|\| (nName == 2 && zName[0] == '.' && zName[1] == '.');` |
|     1 | 8368 | `}` |
|     - | 8369 | ``/* Does this instance carry php's `u.dir` arm? Asked by the five SplFileInfo`` |
|     - | 8370 | ` * bodies that branch on the object TYPE, so it has to be the class question and` |
|     - | 8371 | ` * not "does it have a __e slot" — a user class may declare anything. */` |
|   458 | 8372 | `static int SplDirIs(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 | 8373 | `{` |
|     - | 8374 | `	ph7_class *pDir;` |
|   459 | 8375 | `	if( pThis == 0 ){` |
|   ! 0 | 8376 | `		return 0;` |
|     - | 8377 | `	}` |
|   459 | 8378 | `	pDir = PH7_VmExtractClass(pVm,"DirectoryIterator",sizeof("DirectoryIterator")-1,FALSE,0);` |
|   459 | 8379 | `	return pDir && PH7_VmInstanceOf(pThis->pClass,pDir);` |
|   230 | 8380 | `}` |
|     - | 8381 | ``/* php's `!intern->u.dir.entry.d_name[0]`: the walk has nothing to describe. */`` |
|   149 | 8382 | `static int SplDirAtEnd(ph7_class_instance *pThis)` |
|     1 | 8383 | `{` |
|   150 | 8384 | `	int nEntry = 0;` |
|   150 | 8385 | `	SfiStr(pThis,SDI_E,&nEntry);` |
|   150 | 8386 | `	return nEntry < 1;` |
|     1 | 8387 | `}` |
|     - | 8388 | `/* The registry entry for this instance, or 0. */` |
|   505 | 8389 | `static VmDirHandle * SplDirFind(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 | 8390 | `{` |
|     - | 8391 | `	SyHashEntry *pEntry;` |
|   506 | 8392 | `	if( pThis == 0 \|\| SyHashTotalEntry(&pVm->hDirHandle) < 1 ){` |
|     5 | 8393 | `		return 0;` |
|     - | 8394 | `	}` |
|   502 | 8395 | `	pEntry = SyHashGet(&pVm->hDirHandle,(const void *)&pThis,sizeof(void *));` |
|   502 | 8396 | `	return pEntry ? (VmDirHandle *)pEntry->pUserData : 0;` |
|   255 | 8397 | `}` |
|     - | 8398 | `/* Close the handle this instance owns, if any. The class's xRelease, and the` |
|     - | 8399 | ` * first half of a re-open. */` |
|    38 | 8400 | `static void SplDirClose(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 | 8401 | `{` |
|    39 | 8402 | `	void *pData = 0;` |
|    38 | 8403 | `	if( SyHashDeleteEntry(&pVm->hDirHandle,(const void *)&pThis,sizeof(void *),&pData) == SXRET_OK` |
|    36 | 8404 | `	 && pData ){` |
|    33 | 8405 | `		VmDirHandle *pH = (VmDirHandle *)pData;` |
|    33 | 8406 | `		if( pH->pStream && pH->pStream->xCloseDir ){` |
|    33 | 8407 | `			pH->pStream->xCloseDir(pH->pHandle);` |
|    16 | 8408 | `		}` |
|    33 | 8409 | `		SyMemBackendFree(&pVm->sAllocator,pH);` |
|    16 | 8410 | `	}` |
|    39 | 8411 | `}` |
|     - | 8412 | `/*` |
|     - | 8413 | ` * php's spl_filesystem_dir_read: invalidate the lazy name, then take ONE entry` |
|     - | 8414 | ` * from the stream; running out leaves the entry empty, which is what valid()` |
|     - | 8415 | ` * reports. The read goes through a scratch call context because the VFS reports` |
|     - | 8416 | ` * a name by writing a RESULT -- borrowing the method's own return slot would` |
|     - | 8417 | ` * append to whatever the body is about to answer (rule 54).` |
|     - | 8418 | ` */` |
|   232 | 8419 | `static void SplDirRead(ph7_vm *pVm,ph7_class_instance *pThis,VmDirHandle *pH)` |
|     1 | 8420 | `{` |
|     - | 8421 | `	ph7_context sCtx;` |
|     - | 8422 | `	ph7_value sOut;` |
|   233 | 8423 | `	int rc = -1;` |
|   233 | 8424 | `	PH7_NativeSetAttrStr(pVm,pThis,SFI_N,"",0);` |
|   233 | 8425 | `	PH7_MemObjInit(pVm,&sOut);` |
|   233 | 8426 | `	VmInitCallContext(&sCtx,pVm,0,&sOut,0);` |
|   233 | 8427 | `	if( pH && pH->pStream && pH->pStream->xReadDir ){` |
|   233 | 8428 | `		rc = pH->pStream->xReadDir(pH->pHandle,&sCtx);` |
|   117 | 8429 | `	}` |
|   233 | 8430 | `	if( rc == PH7_OK ){` |
|   209 | 8431 | `		int nName = 0;` |
|   209 | 8432 | `		const char *zName = ph7_value_to_string(&sOut,&nName);` |
|   209 | 8433 | `		PH7_NativeSetAttrStr(pVm,pThis,SDI_E,zName,nName);` |
|   106 | 8434 | `	}else{` |
|    25 | 8435 | `		PH7_NativeSetAttrStr(pVm,pThis,SDI_E,"",0);` |
|     - | 8436 | `	}` |
|   233 | 8437 | `	VmReleaseCallContext(&sCtx);` |
|   233 | 8438 | `	PH7_MemObjRelease(&sOut);` |
|   233 | 8439 | `}` |
|     - | 8440 | `/* php's read loop: one entry, then more while SKIP_DOTS and this is a dot. */` |
|   155 | 8441 | `static void SplDirReadSkip(ph7_vm *pVm,ph7_class_instance *pThis,VmDirHandle *pH)` |
|     1 | 8442 | `{` |
|   156 | 8443 | `	sxi64 iFlags = PH7_NativeAttrInt(pThis,SDI_F);` |
|   149 | 8444 | `	for(;;){` |
|   227 | 8445 | `		int nEntry = 0;` |
|     - | 8446 | `		const char *zEntry;` |
|   227 | 8447 | `		SplDirRead(pVm,pThis,pH);` |
|   227 | 8448 | `		if( (iFlags & SDI_SKIPDOTS) == 0 ){` |
|   122 | 8449 | `			return;` |
|     - | 8450 | `		}` |
|   140 | 8451 | `		zEntry = SfiStr(pThis,SDI_E,&nEntry);` |
|   140 | 8452 | `		if( !SplDirIsDot(zEntry,nEntry) ){` |
|    69 | 8453 | `			return;` |
|     - | 8454 | `		}` |
|     1 | 8455 | `	}` |
|    79 | 8456 | `}` |
|     - | 8457 | `/*` |
|     - | 8458 | ` * php's spl_filesystem_dir_open: open the directory, remember it under the path` |
|     - | 8459 | ` * MINUS one trailing slash, and read the first entry. Answers 0 when the open` |
|     - | 8460 | ` * failed, having still written the path (php sets it either way, so a caught` |
|     - | 8461 | ` * constructor failure leaves the same shape behind).` |
|     - | 8462 | ` */` |
|    54 | 8463 | `static VmDirHandle * SplDirOpen(ph7_vm *pVm,ph7_class_instance *pThis,` |
|     - | 8464 | `	const char *zPath,int nPath)` |
|     1 | 8465 | `{` |
|     - | 8466 | `	const ph7_io_stream *pStream;` |
|     - | 8467 | `	const char *zDevice;` |
|     - | 8468 | `	VmDirHandle *pH;` |
|     - | 8469 | `	char zBuf[4096];` |
|    55 | 8470 | `	void *pHandle = 0;` |
|    55 | 8471 | `	int nKeep = nPath;` |
|    55 | 8472 | `	if( nPath < 1 \|\| nPath >= (int)sizeof(zBuf) ){` |
|   ! 0 | 8473 | `		return 0;` |
|     - | 8474 | `	}` |
|    55 | 8475 | `	SyMemcpy(zPath,zBuf,(sxu32)nPath);` |
|    55 | 8476 | `	zBuf[nPath] = 0;` |
|    55 | 8477 | `	zDevice = zBuf;` |
|    55 | 8478 | `	pStream = PH7_VmGetStreamDevice(pVm,&zDevice,nPath);` |
|    55 | 8479 | `	if( nKeep > 1 && SFI_IS_SLASH(zPath[nKeep-1]) ){` |
|   ! 0 | 8480 | `		nKeep--;` |
|   ! 0 | 8481 | `	}` |
|    55 | 8482 | `	PH7_NativeSetAttrStr(pVm,pThis,SFI_P,zPath,nKeep);` |
|    55 | 8483 | `	PH7_NativeSetAttrInt(pVm,pThis,SDI_I,0);` |
|    55 | 8484 | `	PH7_NativeSetAttrStr(pVm,pThis,SDI_E,"",0);` |
|    55 | 8485 | `	PH7_NativeSetAttrStr(pVm,pThis,SFI_N,"",0);` |
|    54 | 8486 | `	if( pStream == 0 \|\| pStream->xOpenDir == 0` |
|    55 | 8487 | `	 \|\| pStream->xOpenDir(zDevice,0,&pHandle) != PH7_OK ){` |
|     3 | 8488 | `		return 0;` |
|     - | 8489 | `	}` |
|    53 | 8490 | `	pH = (VmDirHandle *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmDirHandle));` |
|    53 | 8491 | `	if( pH == 0 ){` |
|   ! 0 | 8492 | `		if( pStream->xCloseDir ){` |
|   ! 0 | 8493 | `			pStream->xCloseDir(pHandle);` |
|   ! 0 | 8494 | `		}` |
|   ! 0 | 8495 | `		return 0;` |
|     - | 8496 | `	}` |
|    53 | 8497 | `	pH->pStream = pStream;` |
|    53 | 8498 | `	pH->pHandle = pHandle;` |
|    53 | 8499 | `	pH->pThis = pThis;` |
|     - | 8500 | `	/* SyHashInsert BORROWS the key bytes: key off the record's own field, which` |
|     - | 8501 | `	 * lives exactly as long as the entry does (rule 22). */` |
|    53 | 8502 | `	if( SyHashInsert(&pVm->hDirHandle,(const void *)&pH->pThis,sizeof(void *),pH) != SXRET_OK ){` |
|   ! 0 | 8503 | `		if( pStream->xCloseDir ){` |
|   ! 0 | 8504 | `			pStream->xCloseDir(pHandle);` |
|   ! 0 | 8505 | `		}` |
|   ! 0 | 8506 | `		SyMemBackendFree(&pVm->sAllocator,pH);` |
|   ! 0 | 8507 | `		return 0;` |
|     - | 8508 | `	}` |
|    53 | 8509 | `	return pH;` |
|    28 | 8510 | `}` |
|     - | 8511 | `/*` |
|     - | 8512 | ` * The open handle behind this instance, RE-OPENING it for a fresh clone.` |
|     - | 8513 | ` *` |
|     - | 8514 | ` * php's clone handler opens the directory again and reads forward to the` |
|     - | 8515 | ` * source's index, because a directory stream cannot be duplicated; PHL does the` |
|     - | 8516 | ` * same work on first use instead, which is what keeps the handle out of every` |
|     - | 8517 | ``  * php-visible surface — a property slot carrying it would make `$a == clone $a` `` |
|     - | 8518 | ` * false, and php says true.` |
|     - | 8519 | ` */` |
|   401 | 8520 | `static VmDirHandle * SplDirState(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 | 8521 | `{` |
|   402 | 8522 | `	VmDirHandle *pH = SplDirFind(pVm,pThis);` |
|     - | 8523 | `	sxi64 iIndex;` |
|   402 | 8524 | `	int nPath = 0;` |
|     - | 8525 | `	const char *zPath;` |
|     - | 8526 | `	SyBlob sPath;` |
|   402 | 8527 | `	if( pH ){` |
|   376 | 8528 | `		return pH;` |
|     - | 8529 | `	}` |
|    27 | 8530 | `	zPath = SfiStr(pThis,SFI_P,&nPath);` |
|    27 | 8531 | `	if( nPath < 1 ){` |
|    25 | 8532 | `		return 0;   /* never constructed: php's "Object not initialized" */` |
|     - | 8533 | `	}` |
|     - | 8534 | `	/* The path slot is about to be rewritten by the open, so copy it out first. */` |
|     3 | 8535 | `	SyBlobInit(&sPath,&pVm->sAllocator);` |
|     3 | 8536 | `	SyBlobAppend(&sPath,zPath,(sxu32)nPath);` |
|     3 | 8537 | `	iIndex = PH7_NativeAttrInt(pThis,SDI_I);` |
|     3 | 8538 | `	pH = SplDirOpen(pVm,pThis,(const char *)SyBlobData(&sPath),(int)SyBlobLength(&sPath));` |
|     3 | 8539 | `	SyBlobRelease(&sPath);` |
|     3 | 8540 | `	if( pH == 0 ){` |
|   ! 0 | 8541 | `		return 0;` |
|     - | 8542 | `	}` |
|     3 | 8543 | `	SplDirReadSkip(pVm,pThis,pH);` |
|     - | 8544 | `	{` |
|     3 | 8545 | `		sxi64 iAt = iIndex;` |
|     5 | 8546 | `		while( iAt-- > 0 ){` |
|     3 | 8547 | `			SplDirReadSkip(pVm,pThis,pH);` |
|     1 | 8548 | `		}` |
|     - | 8549 | `	}` |
|     - | 8550 | `	/* The open above reset the index; the clone stands where the source stood. */` |
|     3 | 8551 | `	PH7_NativeSetAttrInt(pVm,pThis,SDI_I,iIndex);` |
|     3 | 8552 | `	return pH;` |
|   203 | 8553 | `}` |
|     - | 8554 | `/*` |
|     - | 8555 | ` * php's spl_filesystem_object_get_file_name for a DIR: the path, a slash and the` |
|     - | 8556 | ` * current entry, cached until the next read drops it. Called through SfiName(),` |
|     - | 8557 | ` * so every SplFileInfo accessor sees the same lazy value php's do.` |
|     - | 8558 | ` */` |
|    70 | 8559 | `static const char * SplDirName(ph7_vm *pVm,ph7_class_instance *pThis,int *pnLen)` |
|     1 | 8560 | `{` |
|    71 | 8561 | `	int nName = 0,nPath = 0,nEntry = 0;` |
|    71 | 8562 | `	const char *zName = SfiStr(pThis,SFI_N,&nName);` |
|     - | 8563 | `	const char *zPath,*zEntry;` |
|     - | 8564 | `	SyBlob sName;` |
|    71 | 8565 | `	if( nName > 0 ){` |
|    25 | 8566 | `		*pnLen = nName;` |
|    25 | 8567 | `		return zName;` |
|     - | 8568 | `	}` |
|    47 | 8569 | `	zPath = SfiStr(pThis,SFI_P,&nPath);` |
|    47 | 8570 | `	if( nPath < 1 ){` |
|   ! 0 | 8571 | `		*pnLen = 0;` |
|   ! 0 | 8572 | `		return "";` |
|     - | 8573 | `	}` |
|    47 | 8574 | `	SyBlobInit(&sName,&pVm->sAllocator);` |
|    47 | 8575 | `	SyBlobAppend(&sName,zPath,(sxu32)nPath);` |
|     - | 8576 | `	{` |
|    47 | 8577 | `		char cSlash = SplDirSlash(PH7_NativeAttrInt(pThis,SDI_F));` |
|    47 | 8578 | `		SyBlobAppend(&sName,(const void *)&cSlash,sizeof(char));` |
|     - | 8579 | `	}` |
|    47 | 8580 | `	zEntry = SfiStr(pThis,SDI_E,&nEntry);` |
|    47 | 8581 | `	SyBlobAppend(&sName,zEntry,(sxu32)nEntry);` |
|    70 | 8582 | `	PH7_NativeSetAttrStr(pVm,pThis,SFI_N,` |
|    46 | 8583 | `		(const char *)SyBlobData(&sName),(int)SyBlobLength(&sName));` |
|    47 | 8584 | `	SyBlobRelease(&sName);` |
|    47 | 8585 | `	return SfiStr(pThis,SFI_N,pnLen);` |
|    36 | 8586 | `}` |
|     - | 8587 | `/* php's CHECK_DIRECTORY_ITERATOR_IS_INITIALIZED: every DirectoryIterator method` |
|     - | 8588 | ` * refuses an object whose parent constructor never ran. */` |
|   303 | 8589 | `static VmDirHandle * SplDirChecked(ph7_context *pCtx,sxi32 *pRc)` |
|     1 | 8590 | `{` |
|   304 | 8591 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   304 | 8592 | `	VmDirHandle *pH = SplDirState(pCtx->pVm,pThis);` |
|   304 | 8593 | `	*pRc = PH7_OK;` |
|   304 | 8594 | `	if( pH == 0 ){` |
|    15 | 8595 | `		*pRc = PH7_VmThrowException(pCtx,"Error","Object not initialized");` |
|     7 | 8596 | `	}` |
|   304 | 8597 | `	return pH;` |
|     1 | 8598 | `}` |
|     - | 8599 | `/*` |
|     - | 8600 | ` * The shared constructor: php's spl_filesystem_object_construct, whose two` |
|     - | 8601 | ` * refusals are a ValueError for an empty path and an UnexpectedValueException` |
|     - | 8602 | ` * carrying the OPEN's own errno text (php promotes the opendir warning, so the` |
|     - | 8603 | ` * message is the warning's, prefixed with the constructor that raised it).` |
|     - | 8604 | ` */` |
|    58 | 8605 | `static int SplDirConstruct(ph7_context *pCtx,const char *zClass,int nArg,ph7_value **apArg,` |
|     - | 8606 | `	sxi64 iFlags)` |
|     1 | 8607 | `{` |
|    59 | 8608 | `	ph7_vm *pVm = pCtx->pVm;` |
|    59 | 8609 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 8610 | `	const char *zPath;` |
|    59 | 8611 | `	int nPath = 0;` |
|    59 | 8612 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|   ! 0 | 8613 | `		return PH7_OK;` |
|     - | 8614 | `	}` |
|    59 | 8615 | `	zPath = ph7_value_to_string(apArg[0],&nPath);` |
|    59 | 8616 | `	if( nPath < 1 ){` |
|     7 | 8617 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     2 | 8618 | `			"%s::__construct(): Argument #1 ($directory) must not be empty",zClass);` |
|     - | 8619 | `	}` |
|    55 | 8620 | `	if( SplDirFind(pVm,pThis) ){` |
|     3 | 8621 | `		return PH7_VmThrowException(pCtx,"Error","Directory object is already initialized");` |
|     - | 8622 | `	}` |
|    53 | 8623 | `	PH7_NativeSetAttrInt(pVm,pThis,SDI_F,iFlags);` |
|    53 | 8624 | `	if( SplDirOpen(pVm,pThis,zPath,nPath) == 0 ){` |
|     4 | 8625 | `		return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     1 | 8626 | `			"%s::__construct(%.*s): Failed to open directory: %s",zClass,nPath,zPath,` |
|     2 | 8627 | `			VfsStrerror(errno));` |
|     - | 8628 | `	}` |
|    51 | 8629 | `	SplDirReadSkip(pVm,pThis,SplDirFind(pVm,pThis));` |
|    51 | 8630 | `	return PH7_OK;` |
|    30 | 8631 | `}` |
|     - | 8632 | `/* DirectoryIterator::__construct(string $directory) — php's flags for this one` |
|     - | 8633 | ` * are KEY_AS_PATHNAME\|CURRENT_AS_SELF, and it takes no flags argument. */` |
|    32 | 8634 | `static int vm_builtin_DirectoryIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 8635 | `{` |
|    33 | 8636 | `	return SplDirConstruct(pCtx,"DirectoryIterator",nArg,apArg,` |
|     - | 8637 | `		SDI_KEY_AS_PATHNAME\|SDI_CURRENT_AS_SELF);` |
|     1 | 8638 | `}` |
|     - | 8639 | `/* The flags argument the two subclasses share: php's ZPP overwrites the whole` |
|     - | 8640 | ` * default when one is given, so SKIP_DOTS is NOT implied by passing flags. */` |
|    26 | 8641 | `static sxi64 SplDirFlagArg(int nArg,ph7_value **apArg,sxi64 iDefault)` |
|     1 | 8642 | `{` |
|    27 | 8643 | `	return nArg > 1 ? ph7_value_to_int64(apArg[1]) : iDefault;` |
|     1 | 8644 | `}` |
|    16 | 8645 | `static int vm_builtin_FilesystemIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 8646 | `{` |
|    25 | 8647 | `	return SplDirConstruct(pCtx,"FilesystemIterator",nArg,apArg,` |
|     8 | 8648 | `		SplDirFlagArg(nArg,apArg,SDI_KEY_AS_PATHNAME\|SDI_CURRENT_AS_FILEINFO\|SDI_SKIPDOTS));` |
|     1 | 8649 | `}` |
|    10 | 8650 | `static int vm_builtin_RecursiveDirectoryIterator_construct(ph7_context *pCtx,int nArg,` |
|     - | 8651 | `	ph7_value **apArg)` |
|     1 | 8652 | `{` |
|    16 | 8653 | `	return SplDirConstruct(pCtx,"RecursiveDirectoryIterator",nArg,apArg,` |
|     5 | 8654 | `		SplDirFlagArg(nArg,apArg,SDI_KEY_AS_PATHNAME\|SDI_CURRENT_AS_FILEINFO));` |
|     1 | 8655 | `}` |
|     - | 8656 | `/* DirectoryIterator::rewind(): php re-opens nothing — it rewinds the STREAM and` |
|     - | 8657 | ` * takes one entry, with no dot skipping at this level. */` |
|     8 | 8658 | `static int vm_builtin_DirectoryIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 8659 | `{` |
|     - | 8660 | `	sxi32 rc;` |
|     9 | 8661 | `	VmDirHandle *pH = SplDirChecked(pCtx,&rc);` |
|     4 | 8662 | `	SXUNUSED(nArg);` |
|     4 | 8663 | `	SXUNUSED(apArg);` |
|     9 | 8664 | `	if( pH == 0 ){` |
|     3 | 8665 | `		return rc;` |
|     - | 8666 | `	}` |
|     7 | 8667 | `	PH7_NativeSetAttrInt(pCtx->pVm,PH7_ContextThis(pCtx),SDI_I,0);` |
|     7 | 8668 | `	if( pH->pStream->xRewindDir ){` |
|     7 | 8669 | `		pH->pStream->xRewindDir(pH->pHandle);` |
|     3 | 8670 | `	}` |
|     7 | 8671 | `	SplDirRead(pCtx->pVm,PH7_ContextThis(pCtx),pH);` |
|     7 | 8672 | `	return PH7_OK;` |
|     5 | 8673 | `}` |
|     - | 8674 | `/* FilesystemIterator::rewind(): the same, plus the dot skipping, and php does` |
|     - | 8675 | ` * NOT check the handle here (an uninitialized object simply rewinds to nothing). */` |
|    20 | 8676 | `static int vm_builtin_FilesystemIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 8677 | `{` |
|    21 | 8678 | `	ph7_vm *pVm = pCtx->pVm;` |
|    21 | 8679 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    21 | 8680 | `	VmDirHandle *pH = SplDirState(pVm,pThis);` |
|    10 | 8681 | `	SXUNUSED(nArg);` |
|    10 | 8682 | `	SXUNUSED(apArg);` |
|    21 | 8683 | `	PH7_NativeSetAttrInt(pVm,pThis,SDI_I,0);` |
|    21 | 8684 | `	if( pH && pH->pStream->xRewindDir ){` |
|    21 | 8685 | `		pH->pStream->xRewindDir(pH->pHandle);` |
|    10 | 8686 | `	}` |
|    21 | 8687 | `	SplDirReadSkip(pVm,pThis,pH);` |
|    21 | 8688 | `	return PH7_OK;` |
|     1 | 8689 | `}` |
|    83 | 8690 | `static int vm_builtin_DirectoryIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 8691 | `{` |
|    84 | 8692 | `	ph7_vm *pVm = pCtx->pVm;` |
|    84 | 8693 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 8694 | `	sxi32 rc;` |
|    84 | 8695 | `	VmDirHandle *pH = SplDirChecked(pCtx,&rc);` |
|    42 | 8696 | `	SXUNUSED(nArg);` |
|    42 | 8697 | `	SXUNUSED(apArg);` |
|    84 | 8698 | `	if( pH == 0 ){` |
|     3 | 8699 | `		return rc;` |
|     - | 8700 | `	}` |
|     - | 8701 | `	/* php advances the index PAST the end too, which is why key() keeps counting` |
|     - | 8702 | `	 * once valid() is false. */` |
|    82 | 8703 | `	PH7_NativeSetAttrInt(pVm,pThis,SDI_I,PH7_NativeAttrInt(pThis,SDI_I) + 1);` |
|    82 | 8704 | `	SplDirReadSkip(pVm,pThis,pH);` |
|    82 | 8705 | `	return PH7_OK;` |
|    43 | 8706 | `}` |
|   109 | 8707 | `static int vm_builtin_DirectoryIterator_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 8708 | `{` |
|     - | 8709 | `	sxi32 rc;` |
|   110 | 8710 | `	VmDirHandle *pH = SplDirChecked(pCtx,&rc);` |
|    55 | 8711 | `	SXUNUSED(nArg);` |
|    55 | 8712 | `	SXUNUSED(apArg);` |
|   110 | 8713 | `	if( pH == 0 ){` |
|     3 | 8714 | `		return rc;` |
|     - | 8715 | `	}` |
|   108 | 8716 | `	ph7_result_bool(pCtx,!SplDirAtEnd(PH7_ContextThis(pCtx)));` |
|   108 | 8717 | `	return PH7_OK;` |
|    56 | 8718 | `}` |
|    26 | 8719 | `static int vm_builtin_DirectoryIterator_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 8720 | `{` |
|     - | 8721 | `	sxi32 rc;` |
|    27 | 8722 | `	VmDirHandle *pH = SplDirChecked(pCtx,&rc);` |
|    13 | 8723 | `	SXUNUSED(nArg);` |
|    13 | 8724 | `	SXUNUSED(apArg);` |
|    27 | 8725 | `	if( pH == 0 ){` |
|     3 | 8726 | `		return rc;` |
|     - | 8727 | `	}` |
|    25 | 8728 | `	ph7_result_int64(pCtx,PH7_NativeAttrInt(PH7_ContextThis(pCtx),SDI_I));` |
|    25 | 8729 | `	return PH7_OK;` |
|    14 | 8730 | `}` |
|    10 | 8731 | `static int vm_builtin_DirectoryIterator_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 8732 | `{` |
|     - | 8733 | `	sxi32 rc;` |
|    11 | 8734 | `	VmDirHandle *pH = SplDirChecked(pCtx,&rc);` |
|     5 | 8735 | `	SXUNUSED(nArg);` |
|     5 | 8736 | `	SXUNUSED(apArg);` |
|    11 | 8737 | `	if( pH == 0 ){` |
|     3 | 8738 | `		return rc;` |
|     - | 8739 | `	}` |
|     9 | 8740 | `	SplResultBorrowed(pCtx,PH7_ContextThis(pCtx));` |
|     9 | 8741 | `	return PH7_OK;` |
|     6 | 8742 | `}` |
|    12 | 8743 | `static int vm_builtin_DirectoryIterator_isDot(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 8744 | `{` |
|    13 | 8745 | `	int nEntry = 0;` |
|     - | 8746 | `	const char *zEntry;` |
|     - | 8747 | `	sxi32 rc;` |
|    13 | 8748 | `	VmDirHandle *pH = SplDirChecked(pCtx,&rc);` |
|     6 | 8749 | `	SXUNUSED(nArg);` |
|     6 | 8750 | `	SXUNUSED(apArg);` |
|    13 | 8751 | `	if( pH == 0 ){` |
|     3 | 8752 | `		return rc;` |
|     - | 8753 | `	}` |
|    11 | 8754 | `	zEntry = SfiStr(PH7_ContextThis(pCtx),SDI_E,&nEntry);` |
|    11 | 8755 | `	ph7_result_bool(pCtx,SplDirIsDot(zEntry,nEntry));` |
|    11 | 8756 | `	return PH7_OK;` |
|     7 | 8757 | `}` |
|     - | 8758 | `/*` |
|     - | 8759 | ` * php's seek(): rewind if the target is behind us, then walk forward through` |
|     - | 8760 | ` * the OBJECT's own valid()/next() — a subclass overriding either is obeyed, and` |
|     - | 8761 | ` * running out raises php's OutOfBoundsException with the iterator left standing` |
|     - | 8762 | ` * where the walk stopped.` |
|     - | 8763 | ` */` |
|    10 | 8764 | `static int vm_builtin_DirectoryIterator_seek(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 8765 | `{` |
|    11 | 8766 | `	ph7_vm *pVm = pCtx->pVm;` |
|    11 | 8767 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 8768 | `	ph7_class_method *pMethod;` |
|     - | 8769 | `	sxi64 iPos;` |
|     - | 8770 | `	sxi32 rc;` |
|    11 | 8771 | `	VmDirHandle *pH = SplDirChecked(pCtx,&rc);` |
|    11 | 8772 | `	if( pH == 0 ){` |
|   ! 0 | 8773 | `		return rc;` |
|     - | 8774 | `	}` |
|    11 | 8775 | `	iPos = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;` |
|    11 | 8776 | `	if( PH7_NativeAttrInt(pThis,SDI_I) > iPos ){` |
|     5 | 8777 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);` |
|     5 | 8778 | `		if( pMethod ){` |
|     5 | 8779 | `			rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,0,0,0);` |
|     5 | 8780 | `			if( rc != SXRET_OK ){` |
|   ! 0 | 8781 | `				return rc;` |
|     - | 8782 | `			}` |
|     2 | 8783 | `		}` |
|     2 | 8784 | `	}` |
|    27 | 8785 | `	while( PH7_NativeAttrInt(pThis,SDI_I) < iPos ){` |
|     - | 8786 | `		ph7_value sRet;` |
|     - | 8787 | `		int bValid;` |
|    19 | 8788 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);` |
|    19 | 8789 | `		if( pMethod == 0 ){` |
|   ! 0 | 8790 | `			break;` |
|     - | 8791 | `		}` |
|    19 | 8792 | `		PH7_MemObjInit(pVm,&sRet);` |
|    19 | 8793 | `		rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sRet,0,0);` |
|    19 | 8794 | `		bValid = rc == SXRET_OK && ph7_value_to_bool(&sRet);` |
|    19 | 8795 | `		PH7_MemObjRelease(&sRet);` |
|    19 | 8796 | `		if( rc != SXRET_OK ){` |
|   ! 0 | 8797 | `			return rc;` |
|     - | 8798 | `		}` |
|    19 | 8799 | `		if( !bValid ){` |
|     4 | 8800 | `			return PH7_VmThrowException(pCtx,"OutOfBoundsException",` |
|     1 | 8801 | `				"Seek position %qd is out of range",iPos);` |
|     - | 8802 | `		}` |
|    17 | 8803 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);` |
|    17 | 8804 | `		if( pMethod == 0 ){` |
|   ! 0 | 8805 | `			break;` |
|     - | 8806 | `		}` |
|    17 | 8807 | `		rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,0,0,0);` |
|    17 | 8808 | `		if( rc != SXRET_OK ){` |
|   ! 0 | 8809 | `			return rc;` |
|     - | 8810 | `		}` |
|     1 | 8811 | `	}` |
|     9 | 8812 | `	return PH7_OK;` |
|     6 | 8813 | `}` |
|     - | 8814 | `/* DirectoryIterator's three name accessors read the ENTRY, not the pathname —` |
|     - | 8815 | `` * which is why `getFilename()` answers `..` where SplFileInfo's would answer the`` |
|     - | 8816 | `` * whole path, and why `__toString()` is aliased to this one rather than to`` |
|     - | 8817 | ` * getPathname(). */` |
|    41 | 8818 | `static int vm_builtin_DirectoryIterator_getFilename(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 8819 | `{` |
|    42 | 8820 | `	int nEntry = 0;` |
|     - | 8821 | `	const char *zEntry;` |
|     - | 8822 | `	sxi32 rc;` |
|    42 | 8823 | `	VmDirHandle *pH = SplDirChecked(pCtx,&rc);` |
|    21 | 8824 | `	SXUNUSED(nArg);` |
|    21 | 8825 | `	SXUNUSED(apArg);` |
|    42 | 8826 | `	if( pH == 0 ){` |
|     3 | 8827 | `		return rc;` |
|     - | 8828 | `	}` |
|    40 | 8829 | `	zEntry = SfiStr(PH7_ContextThis(pCtx),SDI_E,&nEntry);` |
|    40 | 8830 | `	ph7_result_string(pCtx,zEntry,nEntry);` |
|    40 | 8831 | `	return PH7_OK;` |
|    22 | 8832 | `}` |
|     2 | 8833 | `static int vm_builtin_DirectoryIterator_getBasename(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 8834 | `{` |
|     3 | 8835 | `	int nEntry = 0,nBase = 0;` |
|     - | 8836 | `	const char *zEntry,*zBase;` |
|     - | 8837 | `	sxi32 rc;` |
|     3 | 8838 | `	VmDirHandle *pH = SplDirChecked(pCtx,&rc);` |
|     3 | 8839 | `	if( pH == 0 ){` |
|   ! 0 | 8840 | `		return rc;` |
|     - | 8841 | `	}` |
|     3 | 8842 | `	zEntry = SfiStr(PH7_ContextThis(pCtx),SDI_E,&nEntry);` |
|     3 | 8843 | `	zBase = PH7_ExtractBaseName(zEntry,nEntry,&nBase);` |
|     3 | 8844 | `	if( nArg > 0 ){` |
|     3 | 8845 | `		int nSuffix = 0;` |
|     3 | 8846 | `		const char *zSuffix = ph7_value_to_string(apArg[0],&nSuffix);` |
|     2 | 8847 | `		if( nSuffix > 0 && nSuffix < nBase` |
|     3 | 8848 | `		 && SyMemcmp(&zBase[nBase - nSuffix],zSuffix,(sxu32)nSuffix) == 0 ){` |
|     3 | 8849 | `			nBase -= nSuffix;` |
|     1 | 8850 | `		}` |
|     1 | 8851 | `	}` |
|     3 | 8852 | `	ph7_result_string(pCtx,zBase,nBase);` |
|     3 | 8853 | `	return PH7_OK;` |
|     2 | 8854 | `}` |
|     2 | 8855 | `static int vm_builtin_DirectoryIterator_getExtension(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 8856 | `{` |
|     3 | 8857 | `	int nEntry = 0,nBase = 0,i;` |
|     - | 8858 | `	const char *zEntry,*zBase;` |
|     - | 8859 | `	sxi32 rc;` |
|     3 | 8860 | `	VmDirHandle *pH = SplDirChecked(pCtx,&rc);` |
|     1 | 8861 | `	SXUNUSED(nArg);` |
|     1 | 8862 | `	SXUNUSED(apArg);` |
|     3 | 8863 | `	if( pH == 0 ){` |
|   ! 0 | 8864 | `		return rc;` |
|     - | 8865 | `	}` |
|     3 | 8866 | `	zEntry = SfiStr(PH7_ContextThis(pCtx),SDI_E,&nEntry);` |
|     3 | 8867 | `	zBase = PH7_ExtractBaseName(zEntry,nEntry,&nBase);` |
|     9 | 8868 | `	for( i = nBase - 1 ; i >= 0 ; --i ){` |
|     9 | 8869 | `		if( zBase[i] == '.' ){` |
|     3 | 8870 | `			ph7_result_string(pCtx,&zBase[i+1],nBase - i - 1);` |
|     3 | 8871 | `			return PH7_OK;` |
|     - | 8872 | `		}` |
|     4 | 8873 | `	}` |
|   ! 0 | 8874 | `	ph7_result_string(pCtx,"",0);` |
|   ! 0 | 8875 | `	return PH7_OK;` |
|     2 | 8876 | `}` |
|     - | 8877 | `/*` |
|     - | 8878 | ` * FilesystemIterator::key()/current(): php compares the flag against its MASK` |
|     - | 8879 | `` * (`(flags & MODE_MASK) == mode`) rather than testing a bit, so a stray bit in`` |
|     - | 8880 | ``  * another field cannot change either answer — which the chunk's `& KEY_AS_FILENAME` `` |
|     - | 8881 | `` * and `=== CURRENT_AS_PATHNAME` both got wrong in one direction or the other.`` |
|     - | 8882 | ` */` |
|    28 | 8883 | `static int vm_builtin_FilesystemIterator_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 8884 | `{` |
|    29 | 8885 | `	ph7_vm *pVm = pCtx->pVm;` |
|    29 | 8886 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    29 | 8887 | `	sxi64 iFlags = PH7_NativeAttrInt(pThis,SDI_F);` |
|    29 | 8888 | `	int nOut = 0;` |
|     - | 8889 | `	const char *zOut;` |
|    14 | 8890 | `	SXUNUSED(nArg);` |
|    14 | 8891 | `	SXUNUSED(apArg);` |
|    29 | 8892 | `	if( (iFlags & SDI_KEY_MODE_MASK) == SDI_KEY_AS_FILENAME ){` |
|     5 | 8893 | `		zOut = SfiStr(pThis,SDI_E,&nOut);` |
|     5 | 8894 | `		ph7_result_string(pCtx,zOut,nOut);` |
|     5 | 8895 | `		return PH7_OK;` |
|     - | 8896 | `	}` |
|    25 | 8897 | `	if( SplDirState(pVm,pThis) == 0 ){` |
|   ! 0 | 8898 | `		return PH7_VmThrowException(pCtx,"Error","Object not initialized");` |
|     - | 8899 | `	}` |
|    25 | 8900 | `	zOut = SfiName(pVm,pThis,&nOut);` |
|    25 | 8901 | `	ph7_result_string(pCtx,zOut,nOut);` |
|    25 | 8902 | `	return PH7_OK;` |
|    15 | 8903 | `}` |
|    40 | 8904 | `static int vm_builtin_FilesystemIterator_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 8905 | `{` |
|    41 | 8906 | `	ph7_vm *pVm = pCtx->pVm;` |
|    41 | 8907 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    41 | 8908 | `	sxi64 iMode = PH7_NativeAttrInt(pThis,SDI_F) & SDI_CURRENT_MODE_MASK;` |
|    20 | 8909 | `	SXUNUSED(nArg);` |
|    20 | 8910 | `	SXUNUSED(apArg);` |
|    41 | 8911 | `	if( iMode == SDI_CURRENT_AS_PATHNAME \|\| iMode == SDI_CURRENT_AS_FILEINFO ){` |
|    29 | 8912 | `		if( SplDirState(pVm,pThis) == 0 ){` |
|   ! 0 | 8913 | `			return PH7_VmThrowException(pCtx,"Error","Object not initialized");` |
|     - | 8914 | `		}` |
|    14 | 8915 | `	}` |
|    41 | 8916 | `	if( iMode == SDI_CURRENT_AS_PATHNAME ){` |
|     5 | 8917 | `		int nName = 0;` |
|     5 | 8918 | `		const char *zName = SfiName(pVm,pThis,&nName);` |
|     5 | 8919 | `		ph7_result_string(pCtx,zName,nName);` |
|     5 | 8920 | `		return PH7_OK;` |
|     - | 8921 | `	}` |
|    37 | 8922 | `	if( iMode == SDI_CURRENT_AS_FILEINFO ){` |
|    25 | 8923 | `		ph7_class *pClass = 0;` |
|    25 | 8924 | `		int nName = 0,nDir = 0;` |
|     - | 8925 | `		const char *zName,*zDir;` |
|     - | 8926 | `		sxi32 rc;` |
|    25 | 8927 | `		if( SplDirAtEnd(pThis) ){` |
|     - | 8928 | `			/* php's create_type again: there is no entry to describe. */` |
|   ! 0 | 8929 | `			return PH7_VmThrowException(pCtx,"RuntimeException","Could not open file");` |
|     - | 8930 | `		}` |
|    25 | 8931 | `		rc = SfiInfoClass(pCtx,"current",0,&pClass);` |
|    25 | 8932 | `		if( rc != PH7_OK ){` |
|   ! 0 | 8933 | `			return rc;` |
|     - | 8934 | `		}` |
|    25 | 8935 | `		zDir = SfiStr(pThis,SFI_P,&nDir);` |
|    25 | 8936 | `		zName = SfiName(pVm,pThis,&nName);` |
|    25 | 8937 | `		return SfiMakeInfoEx(pCtx,pClass,zName,nName,zDir,nDir);` |
|     - | 8938 | `	}` |
|    13 | 8939 | `	SplResultBorrowed(pCtx,pThis);` |
|    13 | 8940 | `	return PH7_OK;` |
|    21 | 8941 | `}` |
|     - | 8942 | `/* php's getFlags()/setFlags() answer and accept only the three mode fields;` |
|     - | 8943 | ` * everything else in the word is engine state the class keeps to itself. */` |
|     8 | 8944 | `static int vm_builtin_FilesystemIterator_getFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 8945 | `{` |
|     4 | 8946 | `	SXUNUSED(nArg);` |
|     4 | 8947 | `	SXUNUSED(apArg);` |
|     9 | 8948 | `	ph7_result_int64(pCtx,PH7_NativeAttrInt(PH7_ContextThis(pCtx),SDI_F) & SDI_FLAGS_MASK);` |
|     9 | 8949 | `	return PH7_OK;` |
|     1 | 8950 | `}` |
|     2 | 8951 | `static int vm_builtin_FilesystemIterator_setFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 8952 | `{` |
|     3 | 8953 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     3 | 8954 | `	sxi64 iFlags = PH7_NativeAttrInt(pThis,SDI_F);` |
|     3 | 8955 | `	sxi64 iNew = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;` |
|     4 | 8956 | `	PH7_NativeSetAttrInt(pCtx->pVm,pThis,SDI_F,(iFlags & ~(sxi64)SDI_FLAGS_MASK)` |
|     2 | 8957 | `		\| (iNew & (sxi64)SDI_FLAGS_MASK));` |
|     3 | 8958 | `	return PH7_OK;` |
|     1 | 8959 | `}` |
|     - | 8960 | `/*` |
|     - | 8961 | ` * RecursiveDirectoryIterator::hasChildren(bool $allowLinks = false).` |
|     - | 8962 | ` *` |
|     - | 8963 | ` * php lstats the entry and then asks two separate questions of it: a plain` |
|     - | 8964 | ` * directory has children, and a SYMLINK has them only when the walk was told to` |
|     - | 8965 | ` * follow links. Asked of the VFS rather than of a mode word, because the mode is` |
|     - | 8966 | ` * not filled on Windows (the same lesson getType() learned).` |
|     - | 8967 | ` */` |
|     4 | 8968 | `static int vm_builtin_RecursiveDirectoryIterator_hasChildren(ph7_context *pCtx,int nArg,` |
|     - | 8969 | `	ph7_value **apArg)` |
|     1 | 8970 | `{` |
|     5 | 8971 | `	ph7_vm *pVm = pCtx->pVm;` |
|     5 | 8972 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     5 | 8973 | `	const ph7_vfs *pVfs = pVm->pEngine->pVfs;` |
|     5 | 8974 | `	int nEntry = 0;` |
|     5 | 8975 | `	const char *zEntry = SfiStr(pThis,SDI_E,&nEntry);` |
|     - | 8976 | `	char zPath[4096];` |
|     5 | 8977 | `	int bAllow = nArg > 0 ? ph7_value_to_bool(apArg[0]) : 0;` |
|     5 | 8978 | `	sxi64 iFlags = PH7_NativeAttrInt(pThis,SDI_F);` |
|     4 | 8979 | `	if( nEntry < 1 \|\| SplDirIsDot(zEntry,nEntry) \|\| pVfs == 0` |
|     3 | 8980 | `	 \|\| SfiPathBuf(pVm,pThis,zPath,(int)sizeof(zPath)) != SXRET_OK ){` |
|     3 | 8981 | `		ph7_result_bool(pCtx,0);` |
|     3 | 8982 | `		return PH7_OK;` |
|     - | 8983 | `	}` |
|     2 | 8984 | `	if( pVfs->xIslink && pVfs->xIslink(zPath) == PH7_OK` |
|     2 | 8985 | `	 && !bAllow && (iFlags & SDI_FOLLOW_SYMLINKS) == 0 ){` |
|   ! 0 | 8986 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 8987 | `		return PH7_OK;` |
|     - | 8988 | `	}` |
|     3 | 8989 | `	ph7_result_bool(pCtx,pVfs->xIsdir && pVfs->xIsdir(zPath) == PH7_OK);` |
|     3 | 8990 | `	return PH7_OK;` |
|     3 | 8991 | `}` |
|     - | 8992 | `/*` |
|     - | 8993 | ` * getChildren(): php builds an instance of the RUNTIME class through its` |
|     - | 8994 | ` * constructor with (pathname, flags), then hands it the sub path — which is what` |
|     - | 8995 | ` * makes getSubPathname() name the whole nested route rather than just the entry` |
|     - | 8996 | `` * (the chunk answered `''` and the filename, wrong at every depth below one).`` |
|     - | 8997 | ` */` |
|     2 | 8998 | `static int vm_builtin_RecursiveDirectoryIterator_getChildren(ph7_context *pCtx,int nArg,` |
|     - | 8999 | `	ph7_value **apArg)` |
|     1 | 9000 | `{` |
|     3 | 9001 | `	ph7_vm *pVm = pCtx->pVm;` |
|     3 | 9002 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 9003 | `	ph7_class_instance *pNew;` |
|     - | 9004 | `	ph7_class_method *pCons;` |
|     - | 9005 | `	ph7_value sPath,sFlags,*apCall[2];` |
|     3 | 9006 | `	int nName = 0,nSub = 0,nEntry = 0;` |
|     - | 9007 | `	const char *zName;` |
|     3 | 9008 | `	sxi64 iFlags = PH7_NativeAttrInt(pThis,SDI_F);` |
|     - | 9009 | `	sxi32 rc;` |
|     - | 9010 | `	SyBlob sSub;` |
|     1 | 9011 | `	SXUNUSED(nArg);` |
|     1 | 9012 | `	SXUNUSED(apArg);` |
|     3 | 9013 | `	if( SplDirState(pVm,pThis) == 0 ){` |
|   ! 0 | 9014 | `		return PH7_VmThrowException(pCtx,"Error","Object not initialized");` |
|     - | 9015 | `	}` |
|     3 | 9016 | `	pNew = PH7_NewClassInstance(pVm,pThis->pClass);` |
|     3 | 9017 | `	if( pNew == 0 ){` |
|   ! 0 | 9018 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 9019 | `	}` |
|     3 | 9020 | `	pNew->iRef++;` |
|     3 | 9021 | `	zName = SfiName(pVm,pThis,&nName);` |
|     3 | 9022 | `	PH7_MemObjInitFromString(pVm,&sPath,0);` |
|     3 | 9023 | `	PH7_MemObjStringAppend(&sPath,zName,(sxu32)nName);` |
|     3 | 9024 | `	PH7_MemObjInitFromInt(pVm,&sFlags,iFlags);` |
|     3 | 9025 | `	apCall[0] = &sPath;` |
|     3 | 9026 | `	apCall[1] = &sFlags;` |
|     3 | 9027 | `	pCons = PH7_ClassExtractMethod(pThis->pClass,"__construct",sizeof("__construct")-1);` |
|     3 | 9028 | `	rc = pCons ? PH7_VmCallClassMethod(pVm,pNew,pCons,0,2,apCall) : SXRET_OK;` |
|     3 | 9029 | `	PH7_MemObjRelease(&sPath);` |
|     3 | 9030 | `	PH7_MemObjRelease(&sFlags);` |
|     3 | 9031 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 9032 | `		PH7_ClassInstanceUnref(pNew);` |
|   ! 0 | 9033 | `		return rc;` |
|     - | 9034 | `	}` |
|     - | 9035 | `	/* php's sub_path: the parent's, this entry appended. */` |
|     3 | 9036 | `	SyBlobInit(&sSub,&pVm->sAllocator);` |
|     - | 9037 | `	{` |
|     3 | 9038 | `		const char *zSub = SfiStr(pThis,SDI_S,&nSub);` |
|     3 | 9039 | `		SyBlobAppend(&sSub,zSub,(sxu32)nSub);` |
|     - | 9040 | `	}` |
|     3 | 9041 | `	if( nSub > 0 ){` |
|   ! 0 | 9042 | `		char cSlash = SplDirSlash(iFlags);` |
|   ! 0 | 9043 | `		SyBlobAppend(&sSub,(const void *)&cSlash,sizeof(char));` |
|   ! 0 | 9044 | `	}` |
|     - | 9045 | `	{` |
|     3 | 9046 | `		const char *zEntry = SfiStr(pThis,SDI_E,&nEntry);` |
|     3 | 9047 | `		SyBlobAppend(&sSub,zEntry,(sxu32)nEntry);` |
|     - | 9048 | `	}` |
|     4 | 9049 | `	PH7_NativeSetAttrStr(pVm,pNew,SDI_S,` |
|     2 | 9050 | `		(const char *)SyBlobData(&sSub),(int)SyBlobLength(&sSub));` |
|     3 | 9051 | `	SyBlobRelease(&sSub);` |
|     - | 9052 | `	{` |
|     3 | 9053 | `		int nInfo = 0;` |
|     3 | 9054 | `		const char *zInfo = SfiStr(pThis,SFI_IC,&nInfo);` |
|     3 | 9055 | `		PH7_NativeSetAttrStr(pVm,pNew,SFI_IC,zInfo,nInfo);` |
|     - | 9056 | `	}` |
|     3 | 9057 | `	PH7_NativeResultObject(pCtx,pNew);` |
|     3 | 9058 | `	PH7_ClassInstanceUnref(pNew);` |
|     3 | 9059 | `	return PH7_OK;` |
|     2 | 9060 | `}` |
|     4 | 9061 | `static int vm_builtin_RecursiveDirectoryIterator_getSubPath(ph7_context *pCtx,int nArg,` |
|     - | 9062 | `	ph7_value **apArg)` |
|     1 | 9063 | `{` |
|     5 | 9064 | `	int nSub = 0;` |
|     5 | 9065 | `	const char *zSub = SfiStr(PH7_ContextThis(pCtx),SDI_S,&nSub);` |
|     2 | 9066 | `	SXUNUSED(nArg);` |
|     2 | 9067 | `	SXUNUSED(apArg);` |
|     5 | 9068 | `	ph7_result_string(pCtx,zSub,nSub);` |
|     5 | 9069 | `	return PH7_OK;` |
|     1 | 9070 | `}` |
|     4 | 9071 | `static int vm_builtin_RecursiveDirectoryIterator_getSubPathname(ph7_context *pCtx,int nArg,` |
|     - | 9072 | `	ph7_value **apArg)` |
|     1 | 9073 | `{` |
|     5 | 9074 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     5 | 9075 | `	int nSub = 0,nEntry = 0;` |
|     5 | 9076 | `	const char *zSub = SfiStr(pThis,SDI_S,&nSub);` |
|     - | 9077 | `	SyBlob sOut;` |
|     2 | 9078 | `	SXUNUSED(nArg);` |
|     2 | 9079 | `	SXUNUSED(apArg);` |
|     5 | 9080 | `	if( nSub < 1 ){` |
|     3 | 9081 | `		const char *zEntry = SfiStr(pThis,SDI_E,&nEntry);` |
|     3 | 9082 | `		ph7_result_string(pCtx,zEntry,nEntry);` |
|     3 | 9083 | `		return PH7_OK;` |
|     - | 9084 | `	}` |
|     3 | 9085 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|     3 | 9086 | `	SyBlobAppend(&sOut,zSub,(sxu32)nSub);` |
|     - | 9087 | `	{` |
|     3 | 9088 | `		char cSlash = SplDirSlash(PH7_NativeAttrInt(pThis,SDI_F));` |
|     3 | 9089 | `		SyBlobAppend(&sOut,(const void *)&cSlash,sizeof(char));` |
|     - | 9090 | `	}` |
|     - | 9091 | `	{` |
|     3 | 9092 | `		const char *zEntry = SfiStr(pThis,SDI_E,&nEntry);` |
|     3 | 9093 | `		SyBlobAppend(&sOut,zEntry,(sxu32)nEntry);` |
|     - | 9094 | `	}` |
|     3 | 9095 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|     3 | 9096 | `	SyBlobRelease(&sOut);` |
|     3 | 9097 | `	return PH7_OK;` |
|     3 | 9098 | `}` |
|     - | 9099 | `/*` |
|     - | 9100 | ` * The three declarations. Method ORDER, signatures and tentative return types` |
|     - | 9101 | `` * are spl_directory.stub.php's; the four slots are php's `u.dir` arm and carry`` |
|     - | 9102 | ` * PH7_MOD_HIDDEN because php declares no property at all here. Each class` |
|     - | 9103 | ` * restates NOSERIALIZE and the presentation hook: a native subclass inherits` |
|     - | 9104 | ` * neither (rule 29).` |
|     - | 9105 | ` */` |
|  5146 | 9106 | `static sxi32 VmInstallSplDirIterators(ph7_vm *pVm)` |
|     5 | 9107 | `{` |
|     - | 9108 | `	static const PH7_NativePropDef aDirProp[] = {` |
|     - | 9109 | `		{ SDI_E, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },` |
|     - | 9110 | `		{ SDI_I, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - | 9111 | `		{ SDI_F, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - | 9112 | `		{ SDI_S, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },` |
|     - | 9113 | `	};` |
|     - | 9114 | `	static const PH7_NativeMethodDef aDirMethod[] = {` |
|     - | 9115 | `		{ "__construct",  PH7_MOD_PUBLIC, "string $directory", 0,` |
|     - | 9116 | `		  vm_builtin_DirectoryIterator_construct },` |
|     - | 9117 | `		{ "getFilename",  PH7_MOD_PUBLIC, "", "@string",` |
|     - | 9118 | `		  vm_builtin_DirectoryIterator_getFilename },` |
|     - | 9119 | `		{ "getExtension", PH7_MOD_PUBLIC, "", "@string",` |
|     - | 9120 | `		  vm_builtin_DirectoryIterator_getExtension },` |
|     - | 9121 | `		{ "getBasename",  PH7_MOD_PUBLIC, "string $suffix = \"\"", "@string",` |
|     - | 9122 | `		  vm_builtin_DirectoryIterator_getBasename },` |
|     - | 9123 | `		{ "isDot",        PH7_MOD_PUBLIC, "", "@bool", vm_builtin_DirectoryIterator_isDot },` |
|     - | 9124 | `		{ "rewind",       PH7_MOD_PUBLIC, "", "@void", vm_builtin_DirectoryIterator_rewind },` |
|     - | 9125 | `		{ "valid",        PH7_MOD_PUBLIC, "", "@bool", vm_builtin_DirectoryIterator_valid },` |
|     - | 9126 | `		{ "key",          PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_DirectoryIterator_key },` |
|     - | 9127 | `		{ "current",      PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_DirectoryIterator_current },` |
|     - | 9128 | `		{ "next",         PH7_MOD_PUBLIC, "", "@void", vm_builtin_DirectoryIterator_next },` |
|     - | 9129 | `		{ "seek",         PH7_MOD_PUBLIC, "int $offset", "@void",` |
|     - | 9130 | `		  vm_builtin_DirectoryIterator_seek },` |
|     - | 9131 | ``		/* php aliases this one to getFilename(), so `echo $it` prints the ENTRY where`` |
|     - | 9132 | `		 * SplFileInfo's __toString prints the whole pathname. Not tentative. */` |
|     - | 9133 | `		{ "__toString",   PH7_MOD_PUBLIC, "", "string",` |
|     - | 9134 | `		  vm_builtin_DirectoryIterator_getFilename },` |
|     - | 9135 | `	};` |
|     - | 9136 | `	static const PH7_NativeConstDef aFsConst[] = {` |
|     - | 9137 | `		{ "CURRENT_MODE_MASK",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_CURRENT_MODE_MASK, 0, 0.0 },` |
|     - | 9138 | `		{ "CURRENT_AS_PATHNAME", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_CURRENT_AS_PATHNAME, 0, 0.0 },` |
|     - | 9139 | `		{ "CURRENT_AS_FILEINFO", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_CURRENT_AS_FILEINFO, 0, 0.0 },` |
|     - | 9140 | `		{ "CURRENT_AS_SELF",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_CURRENT_AS_SELF, 0, 0.0 },` |
|     - | 9141 | `		{ "KEY_MODE_MASK",       PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_KEY_MODE_MASK, 0, 0.0 },` |
|     - | 9142 | `		{ "KEY_AS_PATHNAME",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_KEY_AS_PATHNAME, 0, 0.0 },` |
|     - | 9143 | `		{ "FOLLOW_SYMLINKS",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_FOLLOW_SYMLINKS, 0, 0.0 },` |
|     - | 9144 | `		{ "KEY_AS_FILENAME",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_KEY_AS_FILENAME, 0, 0.0 },` |
|     - | 9145 | `		{ "NEW_CURRENT_AND_KEY", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_KEY_AS_FILENAME\|SDI_CURRENT_AS_FILEINFO, 0, 0.0 },` |
|     - | 9146 | `		{ "OTHER_MODE_MASK",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_OTHERS_MASK, 0, 0.0 },` |
|     - | 9147 | `		{ "SKIP_DOTS",           PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_SKIPDOTS, 0, 0.0 },` |
|     - | 9148 | `		{ "UNIX_PATHS",          PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_UNIXPATHS, 0, 0.0 },` |
|     - | 9149 | `	};` |
|     - | 9150 | `	static const PH7_NativeMethodDef aFsMethod[] = {` |
|     - | 9151 | `		{ "__construct", PH7_MOD_PUBLIC,` |
|     - | 9152 | `		  "string $directory, int $flags = FilesystemIterator::KEY_AS_PATHNAME \| "` |
|     - | 9153 | `		  "FilesystemIterator::CURRENT_AS_FILEINFO \| FilesystemIterator::SKIP_DOTS", 0,` |
|     - | 9154 | `		  vm_builtin_FilesystemIterator_construct },` |
|     - | 9155 | `		{ "rewind",      PH7_MOD_PUBLIC, "", "@void", vm_builtin_FilesystemIterator_rewind },` |
|     - | 9156 | `		{ "key",         PH7_MOD_PUBLIC, "", "@string", vm_builtin_FilesystemIterator_key },` |
|     - | 9157 | `		{ "current",     PH7_MOD_PUBLIC, "", "@SplFileInfo\|FilesystemIterator\|string",` |
|     - | 9158 | `		  vm_builtin_FilesystemIterator_current },` |
|     - | 9159 | `		{ "getFlags",    PH7_MOD_PUBLIC, "", "@int", vm_builtin_FilesystemIterator_getFlags },` |
|     - | 9160 | `		{ "setFlags",    PH7_MOD_PUBLIC, "int $flags", "@void",` |
|     - | 9161 | `		  vm_builtin_FilesystemIterator_setFlags },` |
|     - | 9162 | `	};` |
|     - | 9163 | `	static const PH7_NativeMethodDef aRdiMethod[] = {` |
|     - | 9164 | `		{ "__construct",    PH7_MOD_PUBLIC,` |
|     - | 9165 | `		  "string $directory, int $flags = FilesystemIterator::KEY_AS_PATHNAME \| "` |
|     - | 9166 | `		  "FilesystemIterator::CURRENT_AS_FILEINFO", 0,` |
|     - | 9167 | `		  vm_builtin_RecursiveDirectoryIterator_construct },` |
|     - | 9168 | `		{ "hasChildren",    PH7_MOD_PUBLIC, "bool $allowLinks = false", "@bool",` |
|     - | 9169 | `		  vm_builtin_RecursiveDirectoryIterator_hasChildren },` |
|     - | 9170 | `		{ "getChildren",    PH7_MOD_PUBLIC, "", "@RecursiveDirectoryIterator",` |
|     - | 9171 | `		  vm_builtin_RecursiveDirectoryIterator_getChildren },` |
|     - | 9172 | `		{ "getSubPath",     PH7_MOD_PUBLIC, "", "@string",` |
|     - | 9173 | `		  vm_builtin_RecursiveDirectoryIterator_getSubPath },` |
|     - | 9174 | `		{ "getSubPathname", PH7_MOD_PUBLIC, "", "@string",` |
|     - | 9175 | `		  vm_builtin_RecursiveDirectoryIterator_getSubPathname },` |
|     - | 9176 | `	};` |
|     - | 9177 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - | 9178 | `		{ "DirectoryIterator", "SplFileInfo", "SeekableIterator", PH7_CLASS_NOSERIALIZE,` |
|     - | 9179 | `		  aDirMethod, SX_ARRAYSIZE(aDirMethod), 0, 0,` |
|     - | 9180 | `		  aDirProp, SX_ARRAYSIZE(aDirProp), SplDirClose, 0, SfiPresent },` |
|     - | 9181 | `		{ "FilesystemIterator", "DirectoryIterator", 0, PH7_CLASS_NOSERIALIZE,` |
|     - | 9182 | `		  aFsMethod, SX_ARRAYSIZE(aFsMethod), aFsConst, SX_ARRAYSIZE(aFsConst),` |
|     - | 9183 | `		  0, 0, SplDirClose, 0, SfiPresent },` |
|     - | 9184 | `		{ "RecursiveDirectoryIterator", "FilesystemIterator", "RecursiveIterator",` |
|     - | 9185 | `		  PH7_CLASS_NOSERIALIZE,` |
|     - | 9186 | `		  aRdiMethod, SX_ARRAYSIZE(aRdiMethod), 0, 0,` |
|     - | 9187 | `		  0, 0, SplDirClose, 0, SfiPresent },` |
|     - | 9188 | `	};` |
|  5151 | 9189 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|     5 | 9190 | `}` |
|  5146 | 9191 | `PH7_PRIVATE sxi32 PH7_VmInstallSpl(ph7_vm *pVm)` |
|     5 | 9192 | `{` |
|  5151 | 9193 | `	sxi32 rc = VmInstallWeak(&(*pVm));` |
|  5151 | 9194 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 9195 | `		return rc;` |
|     - | 9196 | `	}` |
|     - | 9197 | `	/* Ordering, now that zSplLib is gone: the remaining PHP in this subsystem is` |
|     - | 9198 | `	 * the tokenizer chunk's, so these only have to satisfy each OTHER. */` |
|  5151 | 9199 | `	rc = VmInstallSplStore(&(*pVm));` |
|  5151 | 9200 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 9201 | `		return rc;` |
|     - | 9202 | `	}` |
|  5151 | 9203 | `	rc = VmInstallSplDualIterators(&(*pVm));` |
|  5151 | 9204 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 9205 | `		return rc;` |
|     - | 9206 | `	}` |
|     - | 9207 | `	/* After the dual iterators: RecursiveIteratorIterator names OuterIterator and` |
|     - | 9208 | `	 * RecursiveIterator, both declared by that table. */` |
|  5151 | 9209 | `	rc = VmInstallSplRecursiveIt(&(*pVm));` |
|  5151 | 9210 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 9211 | `		return rc;` |
|     - | 9212 | `	}` |
|  5151 | 9213 | `	rc = VmInstallSplDllist(&(*pVm));` |
|  5151 | 9214 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 9215 | `		return rc;` |
|     - | 9216 | `	}` |
|  5151 | 9217 | `	rc = VmInstallSplHeap(&(*pVm));` |
|  5151 | 9218 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 9219 | `		return rc;` |
|     - | 9220 | `	}` |
|  5151 | 9221 | `	rc = VmInstallSplFixedArray(&(*pVm));` |
|  5151 | 9222 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 9223 | `		return rc;` |
|     - | 9224 | `	}` |
|  5151 | 9225 | `	rc = VmInstallSplObjectStorage(&(*pVm));` |
|  5151 | 9226 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 9227 | `		return rc;` |
|     - | 9228 | `	}` |
|  5151 | 9229 | `	rc = VmInstallSplFileInfo(&(*pVm));` |
|  5151 | 9230 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 9231 | `		return rc;` |
|     - | 9232 | `	}` |
|     - | 9233 | `	/* After SplFileInfo: DirectoryIterator extends it, and PH7_ClassInherit copies` |
|     - | 9234 | `	 * the base's methods DOWN (rule 14). */` |
|  5151 | 9235 | `	return VmInstallSplDirIterators(&(*pVm));` |
|  2578 | 9236 | `}` |
|     - | 9237 |  |
|     - | 9238 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     - | 9239 |  |
|     - | 9240 | `#ifdef PH7_DISABLE_BUILTIN_FUNC` |
|     - | 9241 | `/* Tiny build: no SPL (builtin layer disabled) */` |
|     - | 9242 | `PH7_PRIVATE sxi32 PH7_VmInstallSpl(ph7_vm *pVm){ (void)pVm; return SXRET_OK; }` |
|     - | 9243 | `/* The writable-container fast path is called unconditionally by OP_LOAD_IDX, and its` |
|     - | 9244 | ` * SXU32_HIGH answer already means "no slot available — take the ordinary offsetGet` |
|     - | 9245 | ` * dispatch". With no SPL classes in this build that is the only answer there is, so the` |
|     - | 9246 | ` * stub keeps the tiny target LINKING without a second #ifdef at the call site. */` |
|     - | 9247 | `PH7_PRIVATE sxu32 PH7_SplDimElemSlot(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pKey,int bCreate)` |
|     - | 9248 | `{` |
|     - | 9249 | `	(void)pVm; (void)pThis; (void)pKey; (void)bCreate;` |
|     - | 9250 | `	return SXU32_HIGH;` |
|     - | 9251 | `}` |
|     - | 9252 | `#endif` |
|     - | 9253 |  |
