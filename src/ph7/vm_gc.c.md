# src/ph7/vm_gc.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 406/423 lines (95.98%)

[Root index](../../index.md) | [Directory index](index.md)

|     Hits | Line | Source |
| -------: | ---: | :--- |
|        - |    1 | `/**` |
|        - |    2 | ` * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|        - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|        - |    4 | ` */` |
|        - |    5 | `/*` |
|        - |    6 | ` * The cycle collector.` |
|        - |    7 | ` *` |
|        - |    8 | ` * PHL frees a value when the last reference to it goes, which is exact for` |
|        - |    9 | ` * everything except a CYCLE: two objects that hold each other, an array holding` |
|        - |   10 | ` * itself, a closure capturing the object that holds the closure. Nothing ever` |
|        - |   11 | ` * drops those to zero, so a long-running program grew without bound where php` |
|        - |   12 | ` * holds flat -- phpcs over one project spent ~4.9 MB per file and reached` |
|        - |   13 | ` * 3976 MB against php's 50.` |
|        - |   14 | ` *` |
|        - |   15 | ` * The algorithm is php's, and Bacon & Rajan's before it: TRIAL DELETION. When a` |
|        - |   16 | ` * container's refcount drops WITHOUT reaching zero it becomes a possible root and` |
|        - |   17 | ` * is buffered -- that is the only event that can strand a cycle, which is what` |
|        - |   18 | ` * lets this work with no root set at all (this engine has no registry of live` |
|        - |   19 | ` * objects, so a mark-and-sweep was never available). A collection then runs over` |
|        - |   20 | ` * the buffered subgraph only:` |
|        - |   21 | ` *` |
|        - |   22 | ` *   mark    -- subtract every reference that comes from INSIDE the subgraph;` |
|        - |   23 | ` *   scan    -- whatever still has a count is held from outside, so put its` |
|        - |   24 | ` *              references back and blacken everything under it;` |
|        - |   25 | ` *   collect -- what counted zero is a cycle nothing outside holds.` |
|        - |   26 | ` *` |
|        - |   27 | ` * WHAT AN EDGE IS. A container's refcount is the number of ph7_value structs` |
|        - |   28 | ` * pointing at it, and every value a container owns is a slot in the VM's aMemObj` |
|        - |   29 | ` * set (a hashmap node's nValIdx, an object property's VmClassAttr::nIdx). One` |
|        - |   30 | ` * owned slot is one edge. A slot can be SHARED, which is what a PHP reference is,` |
|        - |   31 | ` * and subtracting an edge somebody else also holds would free a live value.` |
|        - |   32 | ` * VmGcSlotOwned is the guard: it subtracts only what it can prove this container` |
|        - |   33 | ` * holds alone and treats anything it cannot prove as an outside hold. That costs` |
|        - |   34 | ` * a cycle it cannot collect; it never costs correctness.` |
|        - |   35 | ` *` |
|        - |   36 | ` * WHEN IT RUNS. Only from the VM's fetch-point safe check, between two` |
|        - |   37 | ` * instructions, where the operand stack is consistent and no C builtin holds a` |
|        - |   38 | ` * raw ph7_value* across it -- never from inside the refcount drop that buffered` |
|        - |   39 | ` * the root, which is the middle of an opcode's C body.` |
|        - |   40 | ` *` |
|        - |   41 | ` * WHAT PROVES IT. Before anything is freed the collector re-derives the answer:` |
|        - |   42 | ` * it subtracts the edges INSIDE the dead set and requires every member to fall to` |
|        - |   43 | ` * exactly the one reference the collector itself is holding. A destructor that` |
|        - |   44 | ` * resurrected something, or an edge this file cannot see, shows up as a member` |
|        - |   45 | ` * that does not, and the whole round is abandoned with every count restored. A` |
|        - |   46 | ` * round that collects nothing is a missed reclaim; a round that collects` |
|        - |   47 | ` * something live is a crash, so the check is unconditional.` |
|        - |   48 | ` */` |
|        - |   49 | `#include "ph7int.h"` |
|        - |   50 |  |
|        - |   51 | `/* Buffered roots before the VM is asked to collect. php's own starting point is` |
|        - |   52 | ` * 10001, and like php's it MOVES: a program with no cycles in it buffers a root on` |
|        - |   53 | ` * every refcount drop that does not reach zero -- which is most of them -- and` |
|        - |   54 | ` * would otherwise pay a full mark-and-scan over ten thousand live containers, over` |
|        - |   55 | ` * and over, to find nothing. A run that reclaims little doubles the threshold; one` |
|        - |   56 | ` * that reclaims a real share of what it looked at puts it back. */` |
|        - |   57 | `#define VM_GC_THRESHOLD_MIN 10000` |
|        - |   58 | `/*` |
|        - |   59 | ` * ...and the ceiling, which is OURS and not php's million. A root buffer holds` |
|        - |   60 | ` * at most one row per live container -- PH7_GcPossibleRoot dedupes on nGcRoot --` |
|        - |   61 | ` * so a threshold above the number of collectable containers alive just means` |
|        - |   62 | ` * "never collect", and the rows are not free: a VmGcRef is 16 bytes, and a SySet` |
|        - |   63 | ` * doubles, so a million-root threshold is a THIRTY-TWO megabyte buffer. The gate's` |
|        - |   64 | ` * phpcs step has ~110,000 live collectable containers at its peak, so most of that` |
|        - |   65 | ` * million is buffer for roots that cannot exist -- and it costs: engine-reported` |
|        - |   66 | ` * peak went from 153.18 to 184.93 MB under php's ceiling and to 156.93 under this` |
|        - |   67 | ` * one (those two figures are exact and repeatable; the wall-clock win that came` |
|        - |   68 | ` * with them is not, this box is shared -- PERF.md §7). A hundred thousand is ~2 MB` |
|        - |   69 | ` * and still turns that run's 523 collections into about fifty, which is the ratio` |
|        - |   70 | ` * the choice actually rests on.` |
|        - |   71 | ` */` |
|        - |   72 | `#define VM_GC_THRESHOLD_MAX 100000` |
|        - |   73 |  |
|        - |   74 | `/*` |
|        - |   75 | ` * Is this slot held by nothing except the container that owns it?` |
|        - |   76 | ` *` |
|        - |   77 | ` * The reference table records the holders it can NAME -- frame variables and` |
|        - |   78 | ` * array nodes -- plus a count of the ones it cannot (a static's storage, a` |
|        - |   79 | `` * `use (&$x)` capture, a reference-bound property). A slot with NO record has`` |
|        - |   80 | ` * never been shared with anything, which is exactly the ordinary declared` |
|        - |   81 | ` * property and the ordinary array element nobody aliased.` |
|        - |   82 | ` *` |
|        - |   83 | ` * pNode is the node the walk arrived through, or 0 for an object property (which` |
|        - |   84 | ` * files no row of its own, so any node row at all means somebody else holds it).` |
|        - |   85 | ` */` |
|  1643933 |   86 | `static int VmGcSlotOwned(ph7_vm *pVm,sxu32 nIdx,ph7_hashmap_node *pNode)` |
|        2 |   87 | `{` |
|  1643935 |   88 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |   89 | `		return 0;` |
|        - |   90 | `	}` |
|  1643935 |   91 | `	if( !PH7_VmSlotRegistered(&(*pVm),nIdx) ){` |
|      ! 0 |   92 | `		return 1;` |
|        - |   93 | `	}` |
|  1643935 |   94 | `	if( PH7_VmSlotPinCount(&(*pVm),nIdx) > 0 ){` |
|      ! 0 |   95 | ``		return 0; /* counted pin: the slot is BOUND to somebody else (`$o->p =& $x`) */`` |
|        - |   96 | `	}` |
|  1643935 |   97 | `	if( PH7_VmSlotEntryCount(&(*pVm),nIdx) > 0 ){` |
|       41 |   98 | `		return 0; /* a NAME holds it too */` |
|        - |   99 | `	}` |
|  1643895 |  100 | `	if( pNode == 0 ){` |
|        - |  101 | `		/* A property. Its slot is pinned VM_REF_IDX_KEEP from the moment the object` |
|        - |  102 | `		 * is built -- that pin IS the property's own hold, which is why it is not` |
|        - |  103 | `		 * disqualifying here and is for an element. Any node row on top of it is` |
|        - |  104 | `		 * somebody else pointing at the same slot. */` |
|  1528228 |  105 | `		return PH7_VmSlotNodeCount(&(*pVm),nIdx) == 0;` |
|        - |  106 | `	}` |
|   115669 |  107 | `	if( PH7_VmSlotKeepPinned(&(*pVm),nIdx) ){` |
|      ! 0 |  108 | `		return 0; /* an element pinned past its frame: a by-ref return, a capture */` |
|        - |  109 | `	}` |
|   115669 |  110 | `	return PH7_VmSlotSoleNodeIs(&(*pVm),nIdx,pNode);` |
|   821726 |  111 | `}` |
|        - |  112 | `/*` |
|        - |  113 | ` * May the collector touch this container's refcount at all?` |
|        - |  114 | ` *` |
|        - |  115 | ` * $GLOBALS is never collected -- the engine holds it directly and its release` |
|        - |  116 | ``  * path refuses. Neither is a container with a walk in flight over it: a `foreach` `` |
|        - |  117 | ` * step or an array_walk holds a cursor into its table that no refcount names, so` |
|        - |  118 | ` * freeing it under one leaves the walk on a dead node. Both are simply invisible` |
|        - |  119 | ` * to the traversal: no decrement, no restore, nothing pushed. They act as outside` |
|        - |  120 | ` * roots, which is what they are.` |
|        - |  121 | ` */` |
| 12537017 |  122 | `static int VmGcCollectable(ph7_vm *pVm,void *pCont,int bMap)` |
|        5 |  123 | `{` |
| 12537022 |  124 | `	if( bMap ){` |
|  6014177 |  125 | `		ph7_hashmap *pMap = (ph7_hashmap *)pCont;` |
|  6014177 |  126 | `		return pMap != pVm->pGlobal && pMap->pActiveSteps == 0;` |
|      ! 0 |  127 | `	}else{` |
|  6522850 |  128 | `		ph7_class_instance *pThis = (ph7_class_instance *)pCont;` |
|        - |  129 | `		/* DESTROYED: the object is already mid-release and its table is being torn` |
|        - |  130 | `		 * down under us. */` |
|  9783615 |  131 | `		return pThis->pActiveIters == 0` |
|  6522845 |  132 | `			&& (pThis->iFlags & CLASS_INSTANCE_DESTROYED) == 0;` |
|        - |  133 | `	}` |
|  6267258 |  134 | `}` |
|  1107166 |  135 | `static sxu8 VmGcColor(void *pCont,int bMap)` |
|        2 |  136 | `{` |
|   553575 |  137 | `	return bMap ? ((ph7_hashmap *)pCont)->iGcColor` |
|   791404 |  138 | `	            : ((ph7_class_instance *)pCont)->iGcColor;` |
|        2 |  139 | `}` |
|   745700 |  140 | `static void VmGcSetColor(void *pCont,int bMap,sxu8 iColor)` |
|        2 |  141 | `{` |
|   745702 |  142 | `	if( bMap ){` |
|   420186 |  143 | `		((ph7_hashmap *)pCont)->iGcColor = iColor;` |
|   210088 |  144 | `	}else{` |
|   325518 |  145 | `		((ph7_class_instance *)pCont)->iGcColor = iColor;` |
|        - |  146 | `	}` |
|   745702 |  147 | `}` |
|   216539 |  148 | `static sxi32 VmGcRefCount(void *pCont,int bMap)` |
|        2 |  149 | `{` |
|   216541 |  150 | `	return bMap ? ((ph7_hashmap *)pCont)->iRef : ((ph7_class_instance *)pCont)->iRef;` |
|        2 |  151 | `}` |
|   250426 |  152 | `static void VmGcAddRef(void *pCont,int bMap,sxi32 iDelta)` |
|        2 |  153 | `{` |
|   250428 |  154 | `	if( bMap ){` |
|   214222 |  155 | `		((ph7_hashmap *)pCont)->iRef += iDelta;` |
|   107108 |  156 | `	}else{` |
|    36208 |  157 | `		((ph7_class_instance *)pCont)->iRef += iDelta;` |
|        - |  158 | `	}` |
|   250428 |  159 | `}` |
|        - |  160 | `/* The container a value holds, or 0 when it holds none. */` |
|  1643849 |  161 | `static void * VmGcValueTarget(ph7_value *pVal,int *pbMap)` |
|        2 |  162 | `{` |
|  1643851 |  163 | `	if( pVal == 0 \|\| pVal->x.pOther == 0 ){` |
|  1082220 |  164 | `		return 0;` |
|        - |  165 | `	}` |
|   561633 |  166 | `	if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|   218819 |  167 | `		*pbMap = 1;` |
|   218819 |  168 | `		return pVal->x.pOther;` |
|        - |  169 | `	}` |
|   342816 |  170 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|    52154 |  171 | `		*pbMap = 0;` |
|    52154 |  172 | `		return pVal->x.pOther;` |
|        - |  173 | `	}` |
|   290664 |  174 | `	return 0;` |
|   821684 |  175 | `}` |
|        - |  176 | `/*` |
|        - |  177 | ` * Walk every container this one holds through a slot it owns outright.` |
|        - |  178 | ` *` |
|        - |  179 | ` * The instance side walks hAttr through SyHashFirstEntry/SyHashEntryNext rather` |
|        - |  180 | ` * than the table's embedded loop cursor: that cursor belongs to whoever else may` |
|        - |  181 | ` * be walking, and a collection has to leave every other walk where it found it.` |
|        - |  182 | ` */` |
|        - |  183 | `typedef struct VmGcCtx VmGcCtx;` |
|        - |  184 | `struct VmGcCtx` |
|        - |  185 | `{` |
|        - |  186 | `	ph7_vm *pVm;` |
|        - |  187 | `	SySet *pWork;  /* the worklist this phase is draining */` |
|        - |  188 | `	SySet *pDead;  /* where the collect phase files what it found */` |
|        - |  189 | `};` |
|        - |  190 | `typedef void (*ProcGcVisit)(VmGcCtx *pCtx,void *pChild,int bChildMap,ph7_value *pVal);` |
|        - |  191 |  |
|   556576 |  192 | `static void VmGcWalkChildren(VmGcCtx *pCtx,void *pCont,int bMap,ProcGcVisit xVisit)` |
|        2 |  193 | `{` |
|   556578 |  194 | `	ph7_vm *pVm = pCtx->pVm;` |
|   556578 |  195 | `	int bChildMap = 0;` |
|        - |  196 | `	void *pChild;` |
|   556578 |  197 | `	if( bMap ){` |
|   322004 |  198 | `		ph7_hashmap *pMap = (ph7_hashmap *)pCont;` |
|   322004 |  199 | `		ph7_hashmap_node *pNode = pMap->pFirst;` |
|   322004 |  200 | `		sxu32 n = pMap->nEntry;` |
|   437711 |  201 | `		while( n > 0 && pNode ){` |
|   115709 |  202 | `			ph7_hashmap_node *pNext = pNode->pPrev; /* reverse link -- insertion order */` |
|   115709 |  203 | `			if( VmGcSlotOwned(&(*pVm),pNode->nValIdx,pNode) ){` |
|   115625 |  204 | `				ph7_value *pVal = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pNode->nValIdx);` |
|   115625 |  205 | `				pChild = VmGcValueTarget(pVal,&bChildMap);` |
|   115625 |  206 | `				if( pChild && VmGcCollectable(&(*pVm),pChild,bChildMap) ){` |
|    26691 |  207 | `					xVisit(pCtx,pChild,bChildMap,pVal);` |
|    13340 |  208 | `				}` |
|    57569 |  209 | `			}` |
|   115709 |  210 | `			pNode = pNext;` |
|   115709 |  211 | `			n--;` |
|        2 |  212 | `		}` |
|   160998 |  213 | `	}else{` |
|   234576 |  214 | `		ph7_class_instance *pThis = (ph7_class_instance *)pCont;` |
|   234576 |  215 | `		SyHashEntry *pEntry = SyHashFirstEntry(&pThis->hAttr);` |
|  1762866 |  216 | `		while( pEntry ){` |
|  1528292 |  217 | `			VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|  1528292 |  218 | `			SyHashEntry *pNext = SyHashEntryNext(pEntry);` |
|        - |  219 | `			/* pInst == 0 is a class STATIC or a class constant: one slot shared by` |
|        - |  220 | `			 * every instance, so it appears in every instance's table and is not` |
|        - |  221 | `			 * this object's edge at all. Counting it once per instance would` |
|        - |  222 | `			 * subtract a reference per instance for a value held once. */` |
|  1528292 |  223 | `			if( pVmAttr && pVmAttr->pInst == pThis && VmGcSlotOwned(&(*pVm),pVmAttr->nIdx,0) ){` |
|  1528228 |  224 | `				ph7_value *pVal = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pVmAttr->nIdx);` |
|  1528228 |  225 | `				pChild = VmGcValueTarget(pVal,&bChildMap);` |
|  1528228 |  226 | `				if( pChild && VmGcCollectable(&(*pVm),pChild,bChildMap) ){` |
|   244254 |  227 | `					xVisit(pCtx,pChild,bChildMap,pVal);` |
|   122126 |  228 | `				}` |
|   764113 |  229 | `			}` |
|  1528292 |  230 | `			pEntry = pNext;` |
|        2 |  231 | `		}` |
|        - |  232 | `	}` |
|   556578 |  233 | `}` |
|        - |  234 | `/* ------------------------------------------------------------------- buffering */` |
|        - |  235 | `/*` |
|        - |  236 | ` * A container whose refcount just dropped WITHOUT reaching zero: the only event` |
|        - |  237 | ` * that can strand a cycle, and so the only one worth remembering.` |
|        - |  238 | ` */` |
| 11537264 |  239 | `PH7_PRIVATE void PH7_GcPossibleRoot(ph7_vm *pVm,void *pCont,int bMap)` |
|        5 |  240 | `{` |
|        - |  241 | `	VmGcRef sRef;` |
| 11537269 |  242 | `	if( pVm->bGcEnabled == 0 \|\| pVm->bGcRunning \|\| pVm->bInReset ){` |
|  3344132 |  243 | `		return;` |
|        - |  244 | `	}` |
| 11529225 |  245 | `	if( !VmGcCollectable(&(*pVm),pCont,bMap) ){` |
|    45791 |  246 | `		return;` |
|        - |  247 | `	}` |
| 11483439 |  248 | `	if( bMap ){` |
|  5339110 |  249 | `		ph7_hashmap *pMap = (ph7_hashmap *)pCont;` |
|  5339110 |  250 | `		if( pMap->nGcRoot != 0 ){` |
|  2101127 |  251 | `			return; /* already buffered */` |
|        - |  252 | `		}` |
|  3237988 |  253 | `		pMap->iGcColor = PH7_GC_PURPLE;` |
|  3237988 |  254 | `		pMap->nGcRoot = SySetUsed(&pVm->aGcRoot) + 1;` |
|  1618716 |  255 | `	}else{` |
|  6144334 |  256 | `		ph7_class_instance *pThis = (ph7_class_instance *)pCont;` |
|  6144334 |  257 | `		if( pThis->nGcRoot != 0 ){` |
|  4523664 |  258 | `			return;` |
|        - |  259 | `		}` |
|  1620675 |  260 | `		pThis->iGcColor = PH7_GC_PURPLE;` |
|  1620675 |  261 | `		pThis->nGcRoot = SySetUsed(&pVm->aGcRoot) + 1;` |
|        - |  262 | `	}` |
|  4858658 |  263 | `	sRef.pPtr = pCont;` |
|  4858658 |  264 | `	sRef.bMap = (sxu8)bMap;` |
|  4858658 |  265 | `	if( SySetPut(&pVm->aGcRoot,(const void *)&sRef) != SXRET_OK ){` |
|        - |  266 | `		/* No room to remember it: forget it rather than record it wrong */` |
|      ! 0 |  267 | `		if( bMap ){` |
|      ! 0 |  268 | `			((ph7_hashmap *)pCont)->nGcRoot = 0;` |
|      ! 0 |  269 | `		}else{` |
|      ! 0 |  270 | `			((ph7_class_instance *)pCont)->nGcRoot = 0;` |
|        - |  271 | `		}` |
|      ! 0 |  272 | `		return;` |
|        - |  273 | `	}` |
|  4858658 |  274 | `	if( SySetUsed(&pVm->aGcRoot) >= pVm->nGcThreshold ){` |
|        - |  275 | `		/* Ask the VM to collect at its next fetch point -- NOT here, which is the` |
|        - |  276 | `		 * middle of somebody's refcount drop and so the middle of an opcode. */` |
|       45 |  277 | `		pVm->bGcWanted = 1;` |
|       21 |  278 | `	}` |
|  5767391 |  279 | `}` |
|        - |  280 | `/*` |
|        - |  281 | ` * A buffered container is dying. Its row has to stop naming it before the memory` |
|        - |  282 | ` * goes back to the pool, or the next collection walks freed memory.` |
|        - |  283 | ` */` |
|  6077736 |  284 | `PH7_PRIVATE void PH7_GcForget(ph7_vm *pVm,void *pCont,int bMap)` |
|        5 |  285 | `{` |
|  6077741 |  286 | `	sxu32 nRoot = bMap ? ((ph7_hashmap *)pCont)->nGcRoot` |
|  3798744 |  287 | `	                   : ((ph7_class_instance *)pCont)->nGcRoot;` |
|  6077741 |  288 | `	if( nRoot != 0 && nRoot <= SySetUsed(&pVm->aGcRoot) ){` |
|  4579673 |  289 | `		VmGcRef *aRoot = (VmGcRef *)SySetBasePtr(&pVm->aGcRoot);` |
|  4579673 |  290 | `		if( aRoot[nRoot-1].pPtr == pCont ){` |
|  4579673 |  291 | `			aRoot[nRoot-1].pPtr = 0;` |
|  2289490 |  292 | `		}` |
|  2289490 |  293 | `	}` |
|  6077741 |  294 | `	if( bMap ){` |
|  4558505 |  295 | `		((ph7_hashmap *)pCont)->nGcRoot = 0;` |
|  2278997 |  296 | `	}else{` |
|  1519241 |  297 | `		((ph7_class_instance *)pCont)->nGcRoot = 0;` |
|        - |  298 | `	}` |
|  6077741 |  299 | `}` |
|        - |  300 | `/* --------------------------------------------------------------- the traversal */` |
|        - |  301 |  |
|        - |  302 | `/* The worklists are VM-owned, so a collection allocates nothing per run. */` |
|   858191 |  303 | `static void VmGcPush(SySet *pWork,void *pCont,int bMap)` |
|        2 |  304 | `{` |
|        - |  305 | `	VmGcRef sRef;` |
|   858193 |  306 | `	sRef.pPtr = pCont;` |
|   858193 |  307 | `	sRef.bMap = (sxu8)bMap;` |
|   858193 |  308 | `	SySetPut(pWork,(const void *)&sRef);` |
|   858193 |  309 | `}` |
|   114474 |  310 | `static void VmGcMarkVisit(VmGcCtx *pCtx,void *pChild,int bChildMap,ph7_value *pVal)` |
|        2 |  311 | `{` |
|    57235 |  312 | `	SXUNUSED(pVal);` |
|   114476 |  313 | `	VmGcAddRef(pChild,bChildMap,-1);` |
|   114476 |  314 | `	if( VmGcColor(pChild,bChildMap) != PH7_GC_GREY ){` |
|     6558 |  315 | `		VmGcSetColor(pChild,bChildMap,PH7_GC_GREY);` |
|     6558 |  316 | `		VmGcPush(pCtx->pWork,pChild,bChildMap);` |
|     3278 |  317 | `	}` |
|   114476 |  318 | `}` |
|   309882 |  319 | `static void VmGcDrain(VmGcCtx *pCtx,ProcGcVisit xVisit)` |
|        2 |  320 | `{` |
|   575892 |  321 | `	for(;;){` |
|   730836 |  322 | `		VmGcRef *pTop = (VmGcRef *)SySetPop(pCtx->pWork);` |
|        - |  323 | `		VmGcRef sCur;` |
|   730836 |  324 | `		if( pTop == 0 ){` |
|   309884 |  325 | `			break;` |
|        - |  326 | `		}` |
|   420954 |  327 | `		sCur = *pTop;` |
|   420954 |  328 | `		VmGcWalkChildren(pCtx,sCur.pPtr,sCur.bMap,xVisit);` |
|        2 |  329 | `	}` |
|   309884 |  330 | `}` |
|   209671 |  331 | `static void VmGcMarkGrey(VmGcCtx *pCtx,void *pRoot,int bMap)` |
|        2 |  332 | `{` |
|   209673 |  333 | `	if( VmGcColor(pRoot,bMap) == PH7_GC_GREY ){` |
|     2970 |  334 | `		return;` |
|        - |  335 | `	}` |
|   206705 |  336 | `	VmGcSetColor(pRoot,bMap,PH7_GC_GREY);` |
|   206705 |  337 | `	pCtx->pWork = &pCtx->pVm->aGcWork;` |
|   206705 |  338 | `	SySetReset(pCtx->pWork);` |
|   206705 |  339 | `	VmGcPush(pCtx->pWork,pRoot,bMap);` |
|   206705 |  340 | `	VmGcDrain(pCtx,VmGcMarkVisit);` |
|   104836 |  341 | `}` |
|   106442 |  342 | `static void VmGcBlackVisit(VmGcCtx *pCtx,void *pChild,int bChildMap,ph7_value *pVal)` |
|        2 |  343 | `{` |
|    53219 |  344 | `	SXUNUSED(pVal);` |
|   106444 |  345 | `	VmGcAddRef(pChild,bChildMap,1);` |
|   106444 |  346 | `	if( VmGcColor(pChild,bChildMap) != PH7_GC_BLACK ){` |
|   104516 |  347 | `		VmGcSetColor(pChild,bChildMap,PH7_GC_BLACK);` |
|   104516 |  348 | `		VmGcPush(pCtx->pWork,pChild,bChildMap);` |
|    52255 |  349 | `	}` |
|   106444 |  350 | `}` |
|        - |  351 | `/* Runs on its OWN worklist: the scan below is mid-drain of the primary one. */` |
|   103179 |  352 | `static void VmGcScanBlack(VmGcCtx *pCtx,void *pRoot,int bMap)` |
|        2 |  353 | `{` |
|        - |  354 | `	VmGcCtx sSub;` |
|   103181 |  355 | `	sSub.pVm = pCtx->pVm;` |
|   103181 |  356 | `	sSub.pDead = pCtx->pDead;` |
|   103181 |  357 | `	sSub.pWork = &pCtx->pVm->aGcAux;` |
|   103181 |  358 | `	SySetReset(sSub.pWork);` |
|   103181 |  359 | `	VmGcSetColor(pRoot,bMap,PH7_GC_BLACK);` |
|   103181 |  360 | `	VmGcPush(sSub.pWork,pRoot,bMap);` |
|   103181 |  361 | `	VmGcDrain(&sSub,VmGcBlackVisit);` |
|   103181 |  362 | `}` |
|    17897 |  363 | `static void VmGcPushVisit(VmGcCtx *pCtx,void *pChild,int bChildMap,ph7_value *pVal)` |
|        2 |  364 | `{` |
|     8948 |  365 | `	SXUNUSED(pVal);` |
|    17899 |  366 | `	VmGcPush(pCtx->pWork,pChild,bChildMap);` |
|    17899 |  367 | `}` |
|   209671 |  368 | `static void VmGcScan(VmGcCtx *pCtx,void *pRoot,int bMap)` |
|        2 |  369 | `{` |
|   209673 |  370 | `	pCtx->pWork = &pCtx->pVm->aGcWork;` |
|   209673 |  371 | `	SySetReset(pCtx->pWork);` |
|   209673 |  372 | `	VmGcPush(pCtx->pWork,pRoot,bMap);` |
|   320089 |  373 | `	for(;;){` |
|   429209 |  374 | `		VmGcRef *pTop = (VmGcRef *)SySetPop(pCtx->pWork);` |
|        - |  375 | `		VmGcRef sCur;` |
|   429209 |  376 | `		if( pTop == 0 ){` |
|   209673 |  377 | `			break;` |
|        - |  378 | `		}` |
|   219538 |  379 | `		sCur = *pTop;` |
|   219538 |  380 | `		if( VmGcColor(sCur.pPtr,sCur.bMap) != PH7_GC_GREY ){` |
|     8565 |  381 | `			continue;` |
|        - |  382 | `		}` |
|        - |  383 | `		/* A count left over is an outside hold -- and so is a state the traversal` |
|        - |  384 | `		 * cannot see through. A container that acquired a walk in flight between` |
|        - |  385 | `		 * being buffered and being scanned is held by that cursor, which no` |
|        - |  386 | `		 * refcount names, so it is put back exactly like one that still counts. */` |
|   210973 |  387 | `		if( VmGcRefCount(sCur.pPtr,sCur.bMap) > 0` |
|   159386 |  388 | `		 \|\| !VmGcCollectable(pCtx->pVm,sCur.pPtr,sCur.bMap) ){` |
|   103181 |  389 | `			VmGcScanBlack(pCtx,sCur.pPtr,sCur.bMap);` |
|    51592 |  390 | `		}else{` |
|   107796 |  391 | `			VmGcSetColor(sCur.pPtr,sCur.bMap,PH7_GC_WHITE);` |
|   107796 |  392 | `			VmGcWalkChildren(pCtx,sCur.pPtr,sCur.bMap,VmGcPushVisit);` |
|        - |  393 | `		}` |
|        2 |  394 | `	}` |
|   209673 |  395 | `}` |
|        - |  396 | `/* Everything still white is garbage: move it to the dead list, once. */` |
|   209671 |  397 | `static void VmGcCollectWhite(VmGcCtx *pCtx,void *pRoot,int bMap)` |
|        2 |  398 | `{` |
|   209673 |  399 | `	pCtx->pWork = &pCtx->pVm->aGcWork;` |
|   209673 |  400 | `	SySetReset(pCtx->pWork);` |
|   209673 |  401 | `	VmGcPush(pCtx->pWork,pRoot,bMap);` |
|   216470 |  402 | `	for(;;){` |
|   427376 |  403 | `		VmGcRef *pTop = (VmGcRef *)SySetPop(pCtx->pWork);` |
|        - |  404 | `		VmGcRef sCur;` |
|   427376 |  405 | `		if( pTop == 0 ){` |
|   209673 |  406 | `			break;` |
|        - |  407 | `		}` |
|   217705 |  408 | `		sCur = *pTop;` |
|   217705 |  409 | `		if( VmGcColor(sCur.pPtr,sCur.bMap) != PH7_GC_WHITE ){` |
|   212139 |  410 | `			continue;` |
|        - |  411 | `		}` |
|     5567 |  412 | `		VmGcSetColor(sCur.pPtr,sCur.bMap,PH7_GC_DEAD);` |
|     5567 |  413 | `		SySetPut(pCtx->pDead,(const void *)&sCur);` |
|     5567 |  414 | `		VmGcWalkChildren(pCtx,sCur.pPtr,sCur.bMap,VmGcPushVisit);` |
|        1 |  415 | `	}` |
|   209673 |  416 | `}` |
|        - |  417 | `/* --------------------------------------------------------- proof, and the free */` |
|        - |  418 |  |
|     8032 |  419 | `static void VmGcUncountVisit(VmGcCtx *pCtx,void *pChild,int bChildMap,ph7_value *pVal)` |
|        1 |  420 | `{` |
|     4016 |  421 | `	SXUNUSED(pCtx); SXUNUSED(pVal);` |
|     8033 |  422 | `	if( VmGcColor(pChild,bChildMap) == PH7_GC_DEAD ){` |
|     7957 |  423 | `		VmGcAddRef(pChild,bChildMap,-1);` |
|     3978 |  424 | `	}` |
|     8033 |  425 | `}` |
|     8032 |  426 | `static void VmGcRecountVisit(VmGcCtx *pCtx,void *pChild,int bChildMap,ph7_value *pVal)` |
|        1 |  427 | `{` |
|     4016 |  428 | `	SXUNUSED(pCtx); SXUNUSED(pVal);` |
|     8033 |  429 | `	if( VmGcColor(pChild,bChildMap) == PH7_GC_DEAD ){` |
|     7957 |  430 | `		VmGcAddRef(pChild,bChildMap,1);` |
|     3978 |  431 | `	}` |
|     8033 |  432 | `}` |
|     8032 |  433 | `static void VmGcCutVisit(VmGcCtx *pCtx,void *pChild,int bChildMap,ph7_value *pVal)` |
|        1 |  434 | `{` |
|     4016 |  435 | `	SXUNUSED(pCtx);` |
|     8033 |  436 | `	if( VmGcColor(pChild,bChildMap) == PH7_GC_DEAD ){` |
|        - |  437 | `		/* The child is pinned for the duration, so this drops the edge and nothing else */` |
|     7957 |  438 | `		PH7_MemObjRelease(pVal);` |
|     3978 |  439 | `	}` |
|     8033 |  440 | `}` |
|        - |  441 | `/*` |
|        - |  442 | ` * Put back the edges the MARK phase subtracted and nothing has put back.` |
|        - |  443 | ` *` |
|        - |  444 | ` * Mark decremented every child of every grey node. Scan restored the children of` |
|        - |  445 | ` * the ones that turned out BLACK; the ones that stayed white did not, so this` |
|        - |  446 | ` * walks the dead set and restores ALL of their children -- the ones that are dead` |
|        - |  447 | ` * too AND the live ones they point at. Restoring only the dead ones left a live` |
|        - |  448 | ` * child short by one for each dead parent naming it, and the parent's ordinary` |
|        - |  449 | ` * release then decremented it a SECOND time: a live value freed on a count that` |
|        - |  450 | ` * was never real.` |
|        - |  451 | ` *` |
|        - |  452 | ` * From here on ordinary refcounting is exact for the dead set: a destructor may` |
|        - |  453 | ` * add a reference, drop one, or rewire the graph, and the count follows.` |
|        - |  454 | ` */` |
|     8032 |  455 | `static void VmGcRestoreVisit(VmGcCtx *pCtx,void *pChild,int bChildMap,ph7_value *pVal)` |
|        1 |  456 | `{` |
|     4016 |  457 | `	SXUNUSED(pCtx); SXUNUSED(pVal);` |
|     8033 |  458 | `	VmGcAddRef(pChild,bChildMap,1);` |
|     8033 |  459 | `}` |
|       10 |  460 | `static void VmGcRestoreDead(VmGcCtx *pCtx)` |
|        1 |  461 | `{` |
|       11 |  462 | `	VmGcRef *aDead = (VmGcRef *)SySetBasePtr(pCtx->pDead);` |
|       11 |  463 | `	sxu32 n, nDead = SySetUsed(pCtx->pDead);` |
|     5577 |  464 | `	for( n = 0 ; n < nDead ; ++n ){` |
|     5567 |  465 | `		VmGcWalkChildren(pCtx,aDead[n].pPtr,aDead[n].bMap,VmGcRestoreVisit);` |
|     2784 |  466 | `	}` |
|       11 |  467 | `}` |
|        - |  468 | `/*` |
|        - |  469 | ` * Re-derive the answer before acting on it: subtract the edges INSIDE the dead` |
|        - |  470 | ` * set and require every member to fall to exactly the one reference the collector` |
|        - |  471 | ` * itself is holding. Anything else -- a destructor that took a new reference, an` |
|        - |  472 | ` * edge this file cannot see -- and the round is abandoned with every count put` |
|        - |  473 | ` * back. Missing a reclaim is a missed reclaim; freeing something live is a crash,` |
|        - |  474 | ` * so the check is unconditional.` |
|        - |  475 | ` *` |
|        - |  476 | ` * It runs on the graph as it stands NOW, which is what makes it robust to a` |
|        - |  477 | ` * destructor that rewired something: refcounting stayed exact through the` |
|        - |  478 | ` * destructor, so subtracting the edges that exist now is the right subtraction.` |
|        - |  479 | ` */` |
|       10 |  480 | `static int VmGcVerifyDead(VmGcCtx *pCtx)` |
|        1 |  481 | `{` |
|       11 |  482 | `	VmGcRef *aDead = (VmGcRef *)SySetBasePtr(pCtx->pDead);` |
|       11 |  483 | `	sxu32 n, nDead = SySetUsed(pCtx->pDead);` |
|       11 |  484 | `	int bOk = TRUE;` |
|     5577 |  485 | `	for( n = 0 ; n < nDead ; ++n ){` |
|     5567 |  486 | `		VmGcWalkChildren(pCtx,aDead[n].pPtr,aDead[n].bMap,VmGcUncountVisit);` |
|     2784 |  487 | `	}` |
|     5577 |  488 | `	for( n = 0 ; n < nDead ; ++n ){` |
|     5567 |  489 | `		if( VmGcRefCount(aDead[n].pPtr,aDead[n].bMap) != 1 ){` |
|      ! 0 |  490 | `			bOk = FALSE;` |
|      ! 0 |  491 | `			break;` |
|        - |  492 | `		}` |
|     2784 |  493 | `	}` |
|     5577 |  494 | `	for( n = 0 ; n < nDead ; ++n ){` |
|     5567 |  495 | `		VmGcWalkChildren(pCtx,aDead[n].pPtr,aDead[n].bMap,VmGcRecountVisit);` |
|     2784 |  496 | `	}` |
|       11 |  497 | `	return bOk;` |
|        1 |  498 | `}` |
|        - |  499 | `/* ---------------------------------------------------------------- the collection */` |
|        - |  500 |  |
|        - |  501 | `/*` |
|        - |  502 | ` * What a round cost, against what it bought. Rewarding a productive run with the` |
|        - |  503 | ` * low threshold keeps a cycle-heavy program collecting often; doubling after a` |
|        - |  504 | ` * barren one is what stops a cycle-FREE program paying for the search.` |
|        - |  505 | ` *` |
|        - |  506 | ` * EVERY round has to come through here, and the BARREN ones most of all -- they` |
|        - |  507 | ` * are the entire reason the rule exists. This used to be written inline at the` |
|        - |  508 | `` * end of PH7_GcCollect, past the `nDead < 1` early return, so the one case it`` |
|        - |  509 | ` * was for was the one case that never reached it: a program with no reclaimable` |
|        - |  510 | ` * cycles rescanned ten thousand live containers for nothing, over and over, with` |
|        - |  511 | ` * the throttle pinned at its minimum for the life of the process. The ecosystem` |
|        - |  512 | ` * gate's phpcs step ran the collector 523 times, collected NOTHING, and finished` |
|        - |  513 | ` * with the threshold still at 10000 (143rd session). Those three counters are the` |
|        - |  514 | ` * evidence -- they are facts about the program, not about the machine, which is` |
|        - |  515 | ` * shared here and cannot be timed (PERF.md §7).` |
|        - |  516 | ` */` |
|       42 |  517 | `static void VmGcAdjustThreshold(ph7_vm *pVm,sxu32 nCollected,sxu32 nRoots)` |
|        2 |  518 | `{` |
|       44 |  519 | `	if( nCollected * 4 < nRoots ){` |
|       44 |  520 | `		if( pVm->nGcThreshold < VM_GC_THRESHOLD_MAX ){` |
|       20 |  521 | `			pVm->nGcThreshold <<= 1;` |
|        9 |  522 | `		}` |
|       23 |  523 | `	}else{` |
|      ! 0 |  524 | `		pVm->nGcThreshold = VM_GC_THRESHOLD_MIN;` |
|        - |  525 | `	}` |
|       44 |  526 | `}` |
|       42 |  527 | `PH7_PRIVATE sxu32 PH7_GcCollect(ph7_vm *pVm)` |
|        2 |  528 | `{` |
|        - |  529 | `	VmGcCtx sCtx;` |
|        - |  530 | `	VmGcRef *aRoot, *aDead;` |
|       44 |  531 | `	sxu32 n, nRoots, nDead, nCollected = 0;` |
|       44 |  532 | `	pVm->bGcWanted = 0;` |
|       44 |  533 | `	if( pVm->bGcRunning \|\| pVm->bInReset ){` |
|      ! 0 |  534 | `		return 0;` |
|        - |  535 | `	}` |
|       44 |  536 | `	if( SySetUsed(&pVm->aGcRoot) < 1 ){` |
|      ! 0 |  537 | `		return 0;` |
|        - |  538 | `	}` |
|       44 |  539 | `	pVm->bGcRunning = 1;` |
|       44 |  540 | `	pVm->nGcRuns++;` |
|       44 |  541 | `	sCtx.pVm = pVm;` |
|       44 |  542 | `	sCtx.pWork = &pVm->aGcWork;` |
|       44 |  543 | `	sCtx.pDead = &pVm->aGcDead;` |
|       44 |  544 | `	SySetReset(&pVm->aGcDead);` |
|        - |  545 |  |
|        - |  546 | `	/* (1) mark: subtract every reference internal to the buffered subgraph */` |
|       44 |  547 | `	nRoots = SySetUsed(&pVm->aGcRoot);` |
|  4424790 |  548 | `	for( n = 0 ; n < nRoots ; ++n ){` |
|  4424748 |  549 | `		aRoot = (VmGcRef *)SySetBasePtr(&pVm->aGcRoot);` |
|  4424748 |  550 | `		if( aRoot[n].pPtr == 0 \|\| !VmGcCollectable(pVm,aRoot[n].pPtr,aRoot[n].bMap) ){` |
|  4215077 |  551 | `			continue;` |
|        - |  552 | `		}` |
|   209673 |  553 | `		VmGcMarkGrey(&sCtx,aRoot[n].pPtr,aRoot[n].bMap);` |
|   104836 |  554 | `	}` |
|        - |  555 | `	/* (2) scan: put back what is still reachable from outside */` |
|  4424790 |  556 | `	for( n = 0 ; n < nRoots ; ++n ){` |
|  4424748 |  557 | `		aRoot = (VmGcRef *)SySetBasePtr(&pVm->aGcRoot);` |
|  4424748 |  558 | `		if( aRoot[n].pPtr == 0 \|\| !VmGcCollectable(pVm,aRoot[n].pPtr,aRoot[n].bMap) ){` |
|  4215077 |  559 | `			continue;` |
|        - |  560 | `		}` |
|   209673 |  561 | `		VmGcScan(&sCtx,aRoot[n].pPtr,aRoot[n].bMap);` |
|   104836 |  562 | `	}` |
|        - |  563 | `	/* (3) gather what stayed white */` |
|  4424790 |  564 | `	for( n = 0 ; n < nRoots ; ++n ){` |
|  4424748 |  565 | `		aRoot = (VmGcRef *)SySetBasePtr(&pVm->aGcRoot);` |
|  4424748 |  566 | `		if( aRoot[n].pPtr == 0 \|\| !VmGcCollectable(pVm,aRoot[n].pPtr,aRoot[n].bMap) ){` |
|  4215077 |  567 | `			continue;` |
|        - |  568 | `		}` |
|   209673 |  569 | `		VmGcCollectWhite(&sCtx,aRoot[n].pPtr,aRoot[n].bMap);` |
|   104836 |  570 | `	}` |
|        - |  571 | `	/* The buffer is spent either way; every surviving root is black again. */` |
|       44 |  572 | `	aRoot = (VmGcRef *)SySetBasePtr(&pVm->aGcRoot);` |
|  4424790 |  573 | `	for( n = 0 ; n < nRoots ; ++n ){` |
|  4424748 |  574 | `		if( aRoot[n].pPtr == 0 ){` |
|  4215070 |  575 | `			continue;` |
|        - |  576 | `		}` |
|   209680 |  577 | `		if( VmGcColor(aRoot[n].pPtr,aRoot[n].bMap) != PH7_GC_DEAD ){` |
|   205824 |  578 | `			VmGcSetColor(aRoot[n].pPtr,aRoot[n].bMap,PH7_GC_BLACK);` |
|   102910 |  579 | `		}` |
|   209680 |  580 | `		if( aRoot[n].bMap ){` |
|   103214 |  581 | `			((ph7_hashmap *)aRoot[n].pPtr)->nGcRoot = 0;` |
|    51607 |  582 | `		}else{` |
|   106468 |  583 | `			((ph7_class_instance *)aRoot[n].pPtr)->nGcRoot = 0;` |
|        - |  584 | `		}` |
|   104840 |  585 | `	}` |
|       44 |  586 | `	SySetReset(&pVm->aGcRoot);` |
|        - |  587 |  |
|       44 |  588 | `	nDead = SySetUsed(&pVm->aGcDead);` |
|       44 |  589 | `	if( nDead < 1 ){` |
|        - |  590 | `		/* Nothing was reclaimable. That is a full mark-and-scan spent, and the` |
|        - |  591 | `		 * threshold has to answer for it -- see VmGcAdjustThreshold. */` |
|       34 |  592 | `		VmGcAdjustThreshold(pVm,0,nRoots);` |
|       34 |  593 | `		pVm->bGcRunning = 0;` |
|       34 |  594 | `		return 0;` |
|        - |  595 | `	}` |
|        - |  596 | `	/* (4) hold everything dead while the destructors run: a destructor reads its` |
|        - |  597 | `	 * own object's properties, and those are other members of the same dead set.` |
|        - |  598 | `	 * Then put back what the MARK phase subtracted, so from here on the dead set` |
|        - |  599 | `	 * carries its TRUE refcount plus that one hold -- which is what lets a` |
|        - |  600 | `	 * destructor run arbitrary PHP over it and leave the numbers exact. */` |
|       11 |  601 | `	aDead = (VmGcRef *)SySetBasePtr(&pVm->aGcDead);` |
|     5577 |  602 | `	for( n = 0 ; n < nDead ; ++n ){` |
|     5567 |  603 | `		VmGcAddRef(aDead[n].pPtr,aDead[n].bMap,1);` |
|     2784 |  604 | `	}` |
|       11 |  605 | `	VmGcRestoreDead(&sCtx);` |
|     5577 |  606 | `	for( n = 0 ; n < nDead ; ++n ){` |
|     5567 |  607 | `		aDead = (VmGcRef *)SySetBasePtr(&pVm->aGcDead);` |
|     5567 |  608 | `		if( aDead[n].bMap == 0 ){` |
|     3899 |  609 | `			ph7_class_instance *pThis = (ph7_class_instance *)aDead[n].pPtr;` |
|     3899 |  610 | `			if( (pThis->iFlags & CLASS_INSTANCE_DTOR_CALLED) == 0 ){` |
|     3899 |  611 | `				PH7_ClassInstanceCallDestructor(pThis);` |
|     1949 |  612 | `			}` |
|     1949 |  613 | `		}` |
|     2784 |  614 | `	}` |
|        - |  615 | `	/* (5) prove it, then break the edges inside the dead set */` |
|       11 |  616 | `	if( VmGcVerifyDead(&sCtx) ){` |
|       11 |  617 | `		aDead = (VmGcRef *)SySetBasePtr(&pVm->aGcDead);` |
|     5577 |  618 | `		for( n = 0 ; n < nDead ; ++n ){` |
|     5567 |  619 | `			VmGcWalkChildren(&sCtx,aDead[n].pPtr,aDead[n].bMap,VmGcCutVisit);` |
|     2784 |  620 | `		}` |
|       11 |  621 | `		nCollected = nDead;` |
|        5 |  622 | `	}` |
|        - |  623 | `	/* (6) give back the hold. With the edges cut every member falls to zero and` |
|        - |  624 | `	 * dies through its ordinary release path -- native teardown, weak cells, the` |
|        - |  625 | `	 * slot free list, all of it. When the proof failed nothing was cut, so this` |
|        - |  626 | `	 * only puts the counts back where they were.` |
|        - |  627 | `	 *` |
|        - |  628 | `	 * bGcRunning stays UP across it. Freeing a member runs its holders' teardown,` |
|        - |  629 | `	 * which runs PHP, which reaches a fetch point -- and a collection entered from` |
|        - |  630 | `	 * there would reset the very list this loop is walking. */` |
|       11 |  631 | `	aDead = (VmGcRef *)SySetBasePtr(&pVm->aGcDead);` |
|     5577 |  632 | `	for( n = 0 ; n < nDead ; ++n ){` |
|     5567 |  633 | `		if( VmGcColor(aDead[n].pPtr,aDead[n].bMap) == PH7_GC_DEAD ){` |
|     5567 |  634 | `			VmGcSetColor(aDead[n].pPtr,aDead[n].bMap,PH7_GC_BLACK);` |
|     2783 |  635 | `		}` |
|     2784 |  636 | `	}` |
|     5577 |  637 | `	for( n = 0 ; n < nDead ; ++n ){` |
|     5567 |  638 | `		aDead = (VmGcRef *)SySetBasePtr(&pVm->aGcDead);` |
|     5567 |  639 | `		if( aDead[n].bMap ){` |
|     1669 |  640 | `			PH7_HashmapUnref((ph7_hashmap *)aDead[n].pPtr);` |
|      835 |  641 | `		}else{` |
|     3899 |  642 | `			PH7_ClassInstanceUnref((ph7_class_instance *)aDead[n].pPtr);` |
|        - |  643 | `		}` |
|     2784 |  644 | `	}` |
|       11 |  645 | `	SySetReset(&pVm->aGcDead);` |
|       11 |  646 | `	pVm->nGcCollected += nCollected;` |
|       11 |  647 | `	VmGcAdjustThreshold(pVm,nCollected,nRoots);` |
|       11 |  648 | `	pVm->bGcRunning = 0;` |
|       11 |  649 | `	return nCollected;` |
|       23 |  650 | `}` |
|        - |  651 | `/* ------------------------------------------------------------------- lifecycle */` |
|        - |  652 |  |
|     6721 |  653 | `PH7_PRIVATE void PH7_GcInit(ph7_vm *pVm)` |
|        5 |  654 | `{` |
|     6726 |  655 | `	pVm->nGcThreshold = VM_GC_THRESHOLD_MIN;` |
|     6726 |  656 | `	SySetInit(&pVm->aGcRoot,&pVm->sAllocator,sizeof(VmGcRef));` |
|     6726 |  657 | `	SySetInit(&pVm->aGcWork,&pVm->sAllocator,sizeof(VmGcRef));` |
|     6726 |  658 | `	SySetInit(&pVm->aGcAux,&pVm->sAllocator,sizeof(VmGcRef));` |
|     6726 |  659 | `	SySetInit(&pVm->aGcDead,&pVm->sAllocator,sizeof(VmGcRef));` |
|     6726 |  660 | `}` |
|        - |  661 | `/*` |
|        - |  662 | ` * Forget every buffered root without touching the containers: ph7_vm_reset is` |
|        - |  663 | ` * about to release the whole object pool, so a row that outlived it would name` |
|        - |  664 | ` * freed memory on the next run.` |
|        - |  665 | ` */` |
|       16 |  666 | `PH7_PRIVATE void PH7_GcResetBuffer(ph7_vm *pVm)` |
|      ! 0 |  667 | `{` |
|       16 |  668 | `	VmGcRef *aRoot = (VmGcRef *)SySetBasePtr(&pVm->aGcRoot);` |
|        - |  669 | `	sxu32 n;` |
|      180 |  670 | `	for( n = 0 ; n < SySetUsed(&pVm->aGcRoot) ; ++n ){` |
|      164 |  671 | `		if( aRoot[n].pPtr == 0 ){` |
|       12 |  672 | `			continue;` |
|        - |  673 | `		}` |
|      152 |  674 | `		if( aRoot[n].bMap ){` |
|      152 |  675 | `			((ph7_hashmap *)aRoot[n].pPtr)->nGcRoot = 0;` |
|       76 |  676 | `		}else{` |
|      ! 0 |  677 | `			((ph7_class_instance *)aRoot[n].pPtr)->nGcRoot = 0;` |
|        - |  678 | `		}` |
|       76 |  679 | `	}` |
|       16 |  680 | `	SySetReset(&pVm->aGcRoot);` |
|       16 |  681 | `	SySetReset(&pVm->aGcWork);` |
|       16 |  682 | `	SySetReset(&pVm->aGcAux);` |
|       16 |  683 | `	SySetReset(&pVm->aGcDead);` |
|       16 |  684 | `	pVm->bGcWanted = 0;` |
|       16 |  685 | `	pVm->nGcThreshold = VM_GC_THRESHOLD_MIN;` |
|       16 |  686 | `}` |
|     5629 |  687 | `PH7_PRIVATE void PH7_GcRelease(ph7_vm *pVm)` |
|        5 |  688 | `{` |
|     5634 |  689 | `	SySetRelease(&pVm->aGcRoot);` |
|     5634 |  690 | `	SySetRelease(&pVm->aGcWork);` |
|     5634 |  691 | `	SySetRelease(&pVm->aGcAux);` |
|     5634 |  692 | `	SySetRelease(&pVm->aGcDead);` |
|     5634 |  693 | `}` |
|        - |  694 |  |
