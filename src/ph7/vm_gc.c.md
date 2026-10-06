# src/ph7/vm_gc.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 408/423 lines (96.45%)

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
|        - |   68 | ` * with them is not, this box is shared and cannot be timed). A hundred thousand is ~2 MB` |
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
|  2609065 |   86 | `static int VmGcSlotOwned(ph7_vm *pVm,sxu32 nIdx,ph7_hashmap_node *pNode)` |
|        4 |   87 | `{` |
|  2609069 |   88 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |   89 | `		return 0;` |
|        - |   90 | `	}` |
|  2609069 |   91 | `	if( !PH7_VmSlotRegistered(&(*pVm),nIdx) ){` |
|      ! 0 |   92 | `		return 1;` |
|        - |   93 | `	}` |
|  2609069 |   94 | `	if( PH7_VmSlotPinCount(&(*pVm),nIdx) > 0 ){` |
|      101 |   95 | ``		return 0; /* counted pin: the slot is BOUND to somebody else (`$o->p =& $x`) */`` |
|        - |   96 | `	}` |
|  2608969 |   97 | `	if( PH7_VmSlotEntryCount(&(*pVm),nIdx) > 0 ){` |
|      145 |   98 | `		return 0; /* a NAME holds it too */` |
|        - |   99 | `	}` |
|  2608825 |  100 | `	if( pNode == 0 ){` |
|        - |  101 | `		/* A property. Its slot is pinned VM_REF_IDX_KEEP from the moment the object` |
|        - |  102 | `		 * is built -- that pin IS the property's own hold, which is why it is not` |
|        - |  103 | `		 * disqualifying here and is for an element. Any node row on top of it is` |
|        - |  104 | `		 * somebody else pointing at the same slot. */` |
|  2406878 |  105 | `		return PH7_VmSlotNodeCount(&(*pVm),nIdx) == 0;` |
|        - |  106 | `	}` |
|   201951 |  107 | `	if( PH7_VmSlotKeepPinned(&(*pVm),nIdx) ){` |
|      ! 0 |  108 | `		return 0; /* an element pinned past its frame: a by-ref return, a capture */` |
|        - |  109 | `	}` |
|   201951 |  110 | `	return PH7_VmSlotSoleNodeIs(&(*pVm),nIdx,pNode);` |
|  1303791 |  111 | `}` |
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
| 13627099 |  122 | `static int VmGcCollectable(ph7_vm *pVm,void *pCont,int bMap)` |
|        5 |  123 | `{` |
| 13627104 |  124 | `	if( bMap ){` |
|  6581407 |  125 | `		ph7_hashmap *pMap = (ph7_hashmap *)pCont;` |
|  6581407 |  126 | `		return pMap != pVm->pGlobal && pMap->pActiveSteps == 0;` |
|      ! 0 |  127 | `	}else{` |
|  7045702 |  128 | `		ph7_class_instance *pThis = (ph7_class_instance *)pCont;` |
|        - |  129 | `		/* DESTROYED: the object is already mid-release and its table is being torn` |
|        - |  130 | `		 * down under us. */` |
| 10567886 |  131 | `		return pThis->pActiveIters == 0` |
|  7045697 |  132 | `			&& (pThis->iFlags & CLASS_INSTANCE_DESTROYED) == 0;` |
|        - |  133 | `	}` |
|  6812225 |  134 | `}` |
|  1355730 |  135 | `static sxu8 VmGcColor(void *pCont,int bMap)` |
|        4 |  136 | `{` |
|   677876 |  137 | `	return bMap ? ((ph7_hashmap *)pCont)->iGcColor` |
|   983103 |  138 | `	            : ((ph7_class_instance *)pCont)->iGcColor;` |
|        4 |  139 | `}` |
|   874533 |  140 | `static void VmGcSetColor(void *pCont,int bMap,sxu8 iColor)` |
|        4 |  141 | `{` |
|   874537 |  142 | `	if( bMap ){` |
|   488853 |  143 | `		((ph7_hashmap *)pCont)->iGcColor = iColor;` |
|   244434 |  144 | `	}else{` |
|   385688 |  145 | `		((ph7_class_instance *)pCont)->iGcColor = iColor;` |
|        - |  146 | `	}` |
|   874537 |  147 | `}` |
|   274501 |  148 | `static sxi32 VmGcRefCount(void *pCont,int bMap)` |
|        4 |  149 | `{` |
|   274505 |  150 | `	return bMap ? ((ph7_hashmap *)pCont)->iRef : ((ph7_class_instance *)pCont)->iRef;` |
|        4 |  151 | `}` |
|   374742 |  152 | `static void VmGcAddRef(void *pCont,int bMap,sxi32 iDelta)` |
|        4 |  153 | `{` |
|   374746 |  154 | `	if( bMap ){` |
|   287142 |  155 | `		((ph7_hashmap *)pCont)->iRef += iDelta;` |
|   143573 |  156 | `	}else{` |
|    87608 |  157 | `		((ph7_class_instance *)pCont)->iRef += iDelta;` |
|        - |  158 | `	}` |
|   374746 |  159 | `}` |
|        - |  160 | `/* The container a value holds, or 0 when it holds none. */` |
|  2608733 |  161 | `static void * VmGcValueTarget(ph7_value *pVal,int *pbMap)` |
|        4 |  162 | `{` |
|  2608737 |  163 | `	if( pVal == 0 \|\| pVal->x.pOther == 0 ){` |
|  1634476 |  164 | `		return 0;` |
|        - |  165 | `	}` |
|   974265 |  166 | `	if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|   318132 |  167 | `		*pbMap = 1;` |
|   318132 |  168 | `		return pVal->x.pOther;` |
|        - |  169 | `	}` |
|   656137 |  170 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|   120042 |  171 | `		*pbMap = 0;` |
|   120042 |  172 | `		return pVal->x.pOther;` |
|        - |  173 | `	}` |
|   536099 |  174 | `	return 0;` |
|  1303625 |  175 | `}` |
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
|   753088 |  192 | `static void VmGcWalkChildren(VmGcCtx *pCtx,void *pCont,int bMap,ProcGcVisit xVisit)` |
|        4 |  193 | `{` |
|   753092 |  194 | `	ph7_vm *pVm = pCtx->pVm;` |
|   753092 |  195 | `	int bChildMap = 0;` |
|        - |  196 | `	void *pChild;` |
|   753092 |  197 | `	if( bMap ){` |
|   425884 |  198 | `		ph7_hashmap *pMap = (ph7_hashmap *)pCont;` |
|   425884 |  199 | `		ph7_hashmap_node *pNode = pMap->pFirst;` |
|   425884 |  200 | `		sxu32 n = pMap->nEntry;` |
|   627995 |  201 | `		while( n > 0 && pNode ){` |
|   202115 |  202 | `			ph7_hashmap_node *pNext = pNode->pPrev; /* reverse link -- insertion order */` |
|   202115 |  203 | `			if( VmGcSlotOwned(&(*pVm),pNode->nValIdx,pNode) ){` |
|   201863 |  204 | `				ph7_value *pVal = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pNode->nValIdx);` |
|   201863 |  205 | `				pChild = VmGcValueTarget(pVal,&bChildMap);` |
|   201863 |  206 | `				if( pChild && VmGcCollectable(&(*pVm),pChild,bChildMap) ){` |
|    13932 |  207 | `					xVisit(pCtx,pChild,bChildMap,pVal);` |
|     6964 |  208 | `				}` |
|   100182 |  209 | `			}` |
|   202115 |  210 | `			pNode = pNext;` |
|   202115 |  211 | `			n--;` |
|        4 |  212 | `		}` |
|   212947 |  213 | `	}else{` |
|   327212 |  214 | `		ph7_class_instance *pThis = (ph7_class_instance *)pCont;` |
|   327212 |  215 | `		SyHashEntry *pEntry = SyHashFirstEntry(&pThis->hAttr);` |
|  2734316 |  216 | `		while( pEntry ){` |
|  2407108 |  217 | `			VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|  2407108 |  218 | `			SyHashEntry *pNext = SyHashEntryNext(pEntry);` |
|        - |  219 | `			/* pInst == 0 is a class STATIC or a class constant: one slot shared by` |
|        - |  220 | `			 * every instance, so it appears in every instance's table and is not` |
|        - |  221 | `			 * this object's edge at all. Counting it once per instance would` |
|        - |  222 | `			 * subtract a reference per instance for a value held once. */` |
|  2407108 |  223 | `			if( pVmAttr && PH7_VmAttrInst(pVmAttr) == pThis && VmGcSlotOwned(&(*pVm),pVmAttr->nIdx,0) ){` |
|  2406878 |  224 | `				ph7_value *pVal = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pVmAttr->nIdx);` |
|  2406878 |  225 | `				pChild = VmGcValueTarget(pVal,&bChildMap);` |
|  2406878 |  226 | `				if( pChild && VmGcCollectable(&(*pVm),pChild,bChildMap) ){` |
|   424213 |  227 | `					xVisit(pCtx,pChild,bChildMap,pVal);` |
|   212105 |  228 | `				}` |
|  1203439 |  229 | `			}` |
|  2407108 |  230 | `			pEntry = pNext;` |
|        4 |  231 | `		}` |
|        - |  232 | `	}` |
|   753092 |  233 | `}` |
|        - |  234 | `/* ------------------------------------------------------------------- buffering */` |
|        - |  235 | `/*` |
|        - |  236 | ` * A container whose refcount just dropped WITHOUT reaching zero: the only event` |
|        - |  237 | ` * that can strand a cycle, and so the only one worth remembering.` |
|        - |  238 | ` */` |
| 12396198 |  239 | `PH7_PRIVATE void PH7_GcPossibleRoot(ph7_vm *pVm,void *pCont,int bMap)` |
|        5 |  240 | `{` |
|        - |  241 | `	VmGcRef sRef;` |
| 12396203 |  242 | `	if( pVm->bGcEnabled == 0 \|\| pVm->bGcRunning \|\| pVm->bInReset ){` |
|  3633453 |  243 | `		return;` |
|        - |  244 | `	}` |
| 12365459 |  245 | `	if( !VmGcCollectable(&(*pVm),pCont,bMap) ){` |
|    52994 |  246 | `		return;` |
|        - |  247 | `	}` |
| 12312470 |  248 | `	if( bMap ){` |
|  5774659 |  249 | `		ph7_hashmap *pMap = (ph7_hashmap *)pCont;` |
|  5774659 |  250 | `		if( pMap->nGcRoot != 0 ){` |
|  2293865 |  251 | `			return; /* already buffered */` |
|        - |  252 | `		}` |
|  3480799 |  253 | `		pMap->iGcColor = PH7_GC_PURPLE;` |
|  3480799 |  254 | `		pMap->nGcRoot = SySetUsed(&pVm->aGcRoot) + 1;` |
|  1740077 |  255 | `	}else{` |
|  6537816 |  256 | `		ph7_class_instance *pThis = (ph7_class_instance *)pCont;` |
|  6537816 |  257 | `		if( pThis->nGcRoot != 0 ){` |
|  4856874 |  258 | `			return;` |
|        - |  259 | `		}` |
|  1680947 |  260 | `		pThis->iGcColor = PH7_GC_PURPLE;` |
|  1680947 |  261 | `		pThis->nGcRoot = SySetUsed(&pVm->aGcRoot) + 1;` |
|        - |  262 | `	}` |
|  5161741 |  263 | `	sRef.pPtr = pCont;` |
|  5161741 |  264 | `	sRef.bMap = (sxu8)bMap;` |
|  5161741 |  265 | `	if( SySetPut(&pVm->aGcRoot,(const void *)&sRef) != SXRET_OK ){` |
|        - |  266 | `		/* No room to remember it: forget it rather than record it wrong */` |
|      ! 0 |  267 | `		if( bMap ){` |
|      ! 0 |  268 | `			((ph7_hashmap *)pCont)->nGcRoot = 0;` |
|      ! 0 |  269 | `		}else{` |
|      ! 0 |  270 | `			((ph7_class_instance *)pCont)->nGcRoot = 0;` |
|        - |  271 | `		}` |
|      ! 0 |  272 | `		return;` |
|        - |  273 | `	}` |
|  5161741 |  274 | `	if( SySetUsed(&pVm->aGcRoot) >= pVm->nGcThreshold ){` |
|        - |  275 | `		/* Ask the VM to collect at its next fetch point -- NOT here, which is the` |
|        - |  276 | `		 * middle of somebody's refcount drop and so the middle of an opcode. */` |
|       68 |  277 | `		pVm->bGcWanted = 1;` |
|       34 |  278 | `	}` |
|  6196767 |  279 | `}` |
|        - |  280 | `/*` |
|        - |  281 | ` * A buffered container is dying. Its row has to stop naming it before the memory` |
|        - |  282 | ` * goes back to the pool, or the next collection walks freed memory.` |
|        - |  283 | ` */` |
|  6542248 |  284 | `PH7_PRIVATE void PH7_GcForget(ph7_vm *pVm,void *pCont,int bMap)` |
|        5 |  285 | `{` |
|  6542253 |  286 | `	sxu32 nRoot = bMap ? ((ph7_hashmap *)pCont)->nGcRoot` |
|  4060178 |  287 | `	                   : ((ph7_class_instance *)pCont)->nGcRoot;` |
|  6542253 |  288 | `	if( nRoot != 0 && nRoot <= SySetUsed(&pVm->aGcRoot) ){` |
|  4846227 |  289 | `		VmGcRef *aRoot = (VmGcRef *)SySetBasePtr(&pVm->aGcRoot);` |
|  4846227 |  290 | `		if( aRoot[nRoot-1].pPtr == pCont ){` |
|  4846227 |  291 | `			aRoot[nRoot-1].pPtr = 0;` |
|  2422732 |  292 | `		}` |
|  2422732 |  293 | `	}` |
|  6542253 |  294 | `	if( bMap ){` |
|  4964720 |  295 | `		((ph7_hashmap *)pCont)->nGcRoot = 0;` |
|  2482075 |  296 | `	}else{` |
|  1577538 |  297 | `		((ph7_class_instance *)pCont)->nGcRoot = 0;` |
|        - |  298 | `	}` |
|  6542253 |  299 | `}` |
|        - |  300 | `/* --------------------------------------------------------------- the traversal */` |
|        - |  301 |  |
|        - |  302 | `/* The worklists are VM-owned, so a collection allocates nothing per run. */` |
|   986664 |  303 | `static void VmGcPush(SySet *pWork,void *pCont,int bMap)` |
|        4 |  304 | `{` |
|        - |  305 | `	VmGcRef sRef;` |
|   986668 |  306 | `	sRef.pPtr = pCont;` |
|   986668 |  307 | `	sRef.bMap = (sxu8)bMap;` |
|   986668 |  308 | `	SySetPut(pWork,(const void *)&sRef);` |
|   986668 |  309 | `}` |
|   141402 |  310 | `static void VmGcMarkVisit(VmGcCtx *pCtx,void *pChild,int bChildMap,ph7_value *pVal)` |
|        4 |  311 | `{` |
|    70701 |  312 | `	SXUNUSED(pVal);` |
|   141406 |  313 | `	VmGcAddRef(pChild,bChildMap,-1);` |
|   141406 |  314 | `	if( VmGcColor(pChild,bChildMap) != PH7_GC_GREY ){` |
|    18792 |  315 | `		VmGcSetColor(pChild,bChildMap,PH7_GC_GREY);` |
|    18792 |  316 | `		VmGcPush(pCtx->pWork,pChild,bChildMap);` |
|     9394 |  317 | `	}` |
|   141406 |  318 | `}` |
|   337552 |  319 | `static void VmGcDrain(VmGcCtx *pCtx,ProcGcVisit xVisit)` |
|        4 |  320 | `{` |
|   633471 |  321 | `	for(;;){` |
|   802248 |  322 | `		VmGcRef *pTop = (VmGcRef *)SySetPop(pCtx->pWork);` |
|        - |  323 | `		VmGcRef sCur;` |
|   802248 |  324 | `		if( pTop == 0 ){` |
|   337556 |  325 | `			break;` |
|        - |  326 | `		}` |
|   464696 |  327 | `		sCur = *pTop;` |
|   464696 |  328 | `		VmGcWalkChildren(pCtx,sCur.pPtr,sCur.bMap,xVisit);` |
|        4 |  329 | `	}` |
|   337556 |  330 | `}` |
|   229417 |  331 | `static void VmGcMarkGrey(VmGcCtx *pCtx,void *pRoot,int bMap)` |
|        4 |  332 | `{` |
|   229421 |  333 | `	if( VmGcColor(pRoot,bMap) == PH7_GC_GREY ){` |
|      542 |  334 | `		return;` |
|        - |  335 | `	}` |
|   228883 |  336 | `	VmGcSetColor(pRoot,bMap,PH7_GC_GREY);` |
|   228883 |  337 | `	pCtx->pWork = &pCtx->pVm->aGcWork;` |
|   228883 |  338 | `	SySetReset(pCtx->pWork);` |
|   228883 |  339 | `	VmGcPush(pCtx->pWork,pRoot,bMap);` |
|   228883 |  340 | `	VmGcDrain(pCtx,VmGcMarkVisit);` |
|   114714 |  341 | `}` |
|   110670 |  342 | `static void VmGcBlackVisit(VmGcCtx *pCtx,void *pChild,int bChildMap,ph7_value *pVal)` |
|        4 |  343 | `{` |
|    55335 |  344 | `	SXUNUSED(pVal);` |
|   110674 |  345 | `	VmGcAddRef(pChild,bChildMap,1);` |
|   110674 |  346 | `	if( VmGcColor(pChild,bChildMap) != PH7_GC_BLACK ){` |
|   108356 |  347 | `		VmGcSetColor(pChild,bChildMap,PH7_GC_BLACK);` |
|   108356 |  348 | `		VmGcPush(pCtx->pWork,pChild,bChildMap);` |
|    54176 |  349 | `	}` |
|   110674 |  350 | `}` |
|        - |  351 | `/* Runs on its OWN worklist: the scan below is mid-drain of the primary one. */` |
|   108673 |  352 | `static void VmGcScanBlack(VmGcCtx *pCtx,void *pRoot,int bMap)` |
|        4 |  353 | `{` |
|        - |  354 | `	VmGcCtx sSub;` |
|   108677 |  355 | `	sSub.pVm = pCtx->pVm;` |
|   108677 |  356 | `	sSub.pDead = pCtx->pDead;` |
|   108677 |  357 | `	sSub.pWork = &pCtx->pVm->aGcAux;` |
|   108677 |  358 | `	SySetReset(sSub.pWork);` |
|   108677 |  359 | `	VmGcSetColor(pRoot,bMap,PH7_GC_BLACK);` |
|   108677 |  360 | `	VmGcPush(sSub.pWork,pRoot,bMap);` |
|   108677 |  361 | `	VmGcDrain(&sSub,VmGcBlackVisit);` |
|   108677 |  362 | `}` |
|    63138 |  363 | `static void VmGcPushVisit(VmGcCtx *pCtx,void *pChild,int bChildMap,ph7_value *pVal)` |
|        4 |  364 | `{` |
|    31569 |  365 | `	SXUNUSED(pVal);` |
|    63142 |  366 | `	VmGcPush(pCtx->pWork,pChild,bChildMap);` |
|    63142 |  367 | `}` |
|   229417 |  368 | `static void VmGcScan(VmGcCtx *pCtx,void *pRoot,int bMap)` |
|        4 |  369 | `{` |
|   229421 |  370 | `	pCtx->pWork = &pCtx->pVm->aGcWork;` |
|   229421 |  371 | `	SySetReset(pCtx->pWork);` |
|   229421 |  372 | `	VmGcPush(pCtx->pWork,pRoot,bMap);` |
|   367551 |  373 | `	for(;;){` |
|   491244 |  374 | `		VmGcRef *pTop = (VmGcRef *)SySetPop(pCtx->pWork);` |
|        - |  375 | `		VmGcRef sCur;` |
|   491244 |  376 | `		if( pTop == 0 ){` |
|   229421 |  377 | `			break;` |
|        - |  378 | `		}` |
|   261827 |  379 | `		sCur = *pTop;` |
|   261827 |  380 | `		if( VmGcColor(sCur.pPtr,sCur.bMap) != PH7_GC_GREY ){` |
|    17968 |  381 | `			continue;` |
|        - |  382 | `		}` |
|        - |  383 | `		/* A count left over is an outside hold -- and so is a state the traversal` |
|        - |  384 | `		 * cannot see through. A container that acquired a walk in flight between` |
|        - |  385 | `		 * being buffered and being scanned is held by that cursor, which no` |
|        - |  386 | `		 * refcount names, so it is put back exactly like one that still counts. */` |
|   243859 |  387 | `		if( VmGcRefCount(sCur.pPtr,sCur.bMap) > 0` |
|   189528 |  388 | `		 \|\| !VmGcCollectable(pCtx->pVm,sCur.pPtr,sCur.bMap) ){` |
|   108677 |  389 | `			VmGcScanBlack(pCtx,sCur.pPtr,sCur.bMap);` |
|    54342 |  390 | `		}else{` |
|   135190 |  391 | `			VmGcSetColor(sCur.pPtr,sCur.bMap,PH7_GC_WHITE);` |
|   135190 |  392 | `			VmGcWalkChildren(pCtx,sCur.pPtr,sCur.bMap,VmGcPushVisit);` |
|        - |  393 | `		}` |
|        4 |  394 | `	}` |
|   229421 |  395 | `}` |
|        - |  396 | `/* Everything still white is garbage: move it to the dead list, once. */` |
|   229417 |  397 | `static void VmGcCollectWhite(VmGcCtx *pCtx,void *pRoot,int bMap)` |
|        4 |  398 | `{` |
|   229421 |  399 | `	pCtx->pWork = &pCtx->pVm->aGcWork;` |
|   229421 |  400 | `	SySetReset(pCtx->pWork);` |
|   229421 |  401 | `	VmGcPush(pCtx->pWork,pRoot,bMap);` |
|   260104 |  402 | `	for(;;){` |
|   489570 |  403 | `		VmGcRef *pTop = (VmGcRef *)SySetPop(pCtx->pWork);` |
|        - |  404 | `		VmGcRef sCur;` |
|   489570 |  405 | `		if( pTop == 0 ){` |
|   229421 |  406 | `			break;` |
|        - |  407 | `		}` |
|   260153 |  408 | `		sCur = *pTop;` |
|   260153 |  409 | `		if( VmGcColor(sCur.pPtr,sCur.bMap) != PH7_GC_WHITE ){` |
|   229511 |  410 | `			continue;` |
|        - |  411 | `		}` |
|    30643 |  412 | `		VmGcSetColor(sCur.pPtr,sCur.bMap,PH7_GC_DEAD);` |
|    30643 |  413 | `		SySetPut(pCtx->pDead,(const void *)&sCur);` |
|    30643 |  414 | `		VmGcWalkChildren(pCtx,sCur.pPtr,sCur.bMap,VmGcPushVisit);` |
|        1 |  415 | `	}` |
|   229421 |  416 | `}` |
|        - |  417 | `/* --------------------------------------------------------- proof, and the free */` |
|        - |  418 |  |
|    30732 |  419 | `static void VmGcUncountVisit(VmGcCtx *pCtx,void *pChild,int bChildMap,ph7_value *pVal)` |
|        1 |  420 | `{` |
|    15366 |  421 | `	SXUNUSED(pCtx); SXUNUSED(pVal);` |
|    30733 |  422 | `	if( VmGcColor(pChild,bChildMap) == PH7_GC_DEAD ){` |
|    30649 |  423 | `		VmGcAddRef(pChild,bChildMap,-1);` |
|    15324 |  424 | `	}` |
|    30733 |  425 | `}` |
|    30732 |  426 | `static void VmGcRecountVisit(VmGcCtx *pCtx,void *pChild,int bChildMap,ph7_value *pVal)` |
|        1 |  427 | `{` |
|    15366 |  428 | `	SXUNUSED(pCtx); SXUNUSED(pVal);` |
|    30733 |  429 | `	if( VmGcColor(pChild,bChildMap) == PH7_GC_DEAD ){` |
|    30649 |  430 | `		VmGcAddRef(pChild,bChildMap,1);` |
|    15324 |  431 | `	}` |
|    30733 |  432 | `}` |
|    30732 |  433 | `static void VmGcCutVisit(VmGcCtx *pCtx,void *pChild,int bChildMap,ph7_value *pVal)` |
|        1 |  434 | `{` |
|    15366 |  435 | `	SXUNUSED(pCtx);` |
|    30733 |  436 | `	if( VmGcColor(pChild,bChildMap) == PH7_GC_DEAD ){` |
|        - |  437 | `		/* The child is pinned for the duration, so this drops the edge and nothing else */` |
|    30649 |  438 | `		PH7_MemObjRelease(pVal);` |
|    15324 |  439 | `	}` |
|    30733 |  440 | `}` |
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
|    30732 |  455 | `static void VmGcRestoreVisit(VmGcCtx *pCtx,void *pChild,int bChildMap,ph7_value *pVal)` |
|        1 |  456 | `{` |
|    15366 |  457 | `	SXUNUSED(pCtx); SXUNUSED(pVal);` |
|    30733 |  458 | `	VmGcAddRef(pChild,bChildMap,1);` |
|    30733 |  459 | `}` |
|       18 |  460 | `static void VmGcRestoreDead(VmGcCtx *pCtx)` |
|        1 |  461 | `{` |
|       19 |  462 | `	VmGcRef *aDead = (VmGcRef *)SySetBasePtr(pCtx->pDead);` |
|       19 |  463 | `	sxu32 n, nDead = SySetUsed(pCtx->pDead);` |
|    30661 |  464 | `	for( n = 0 ; n < nDead ; ++n ){` |
|    30643 |  465 | `		VmGcWalkChildren(pCtx,aDead[n].pPtr,aDead[n].bMap,VmGcRestoreVisit);` |
|    15322 |  466 | `	}` |
|       19 |  467 | `}` |
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
|       18 |  480 | `static int VmGcVerifyDead(VmGcCtx *pCtx)` |
|        1 |  481 | `{` |
|       19 |  482 | `	VmGcRef *aDead = (VmGcRef *)SySetBasePtr(pCtx->pDead);` |
|       19 |  483 | `	sxu32 n, nDead = SySetUsed(pCtx->pDead);` |
|       19 |  484 | `	int bOk = TRUE;` |
|    30661 |  485 | `	for( n = 0 ; n < nDead ; ++n ){` |
|    30643 |  486 | `		VmGcWalkChildren(pCtx,aDead[n].pPtr,aDead[n].bMap,VmGcUncountVisit);` |
|    15322 |  487 | `	}` |
|    30661 |  488 | `	for( n = 0 ; n < nDead ; ++n ){` |
|    30643 |  489 | `		if( VmGcRefCount(aDead[n].pPtr,aDead[n].bMap) != 1 ){` |
|      ! 0 |  490 | `			bOk = FALSE;` |
|      ! 0 |  491 | `			break;` |
|        - |  492 | `		}` |
|    15322 |  493 | `	}` |
|    30661 |  494 | `	for( n = 0 ; n < nDead ; ++n ){` |
|    30643 |  495 | `		VmGcWalkChildren(pCtx,aDead[n].pPtr,aDead[n].bMap,VmGcRecountVisit);` |
|    15322 |  496 | `	}` |
|       19 |  497 | `	return bOk;` |
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
|        - |  515 | ` * shared here and cannot be timed.` |
|        - |  516 | ` */` |
|       60 |  517 | `static void VmGcAdjustThreshold(ph7_vm *pVm,sxu32 nCollected,sxu32 nRoots)` |
|        4 |  518 | `{` |
|       64 |  519 | `	if( nCollected * 4 < nRoots ){` |
|       60 |  520 | `		if( pVm->nGcThreshold < VM_GC_THRESHOLD_MAX ){` |
|       34 |  521 | `			pVm->nGcThreshold <<= 1;` |
|       15 |  522 | `		}` |
|       32 |  523 | `	}else{` |
|        5 |  524 | `		pVm->nGcThreshold = VM_GC_THRESHOLD_MIN;` |
|        - |  525 | `	}` |
|       64 |  526 | `}` |
|       60 |  527 | `PH7_PRIVATE sxu32 PH7_GcCollect(ph7_vm *pVm)` |
|        4 |  528 | `{` |
|        - |  529 | `	VmGcCtx sCtx;` |
|        - |  530 | `	VmGcRef *aRoot, *aDead;` |
|       64 |  531 | `	sxu32 n, nRoots, nDead, nCollected = 0;` |
|       64 |  532 | `	pVm->bGcWanted = 0;` |
|       64 |  533 | `	if( pVm->bGcRunning \|\| pVm->bInReset ){` |
|      ! 0 |  534 | `		return 0;` |
|        - |  535 | `	}` |
|       64 |  536 | `	if( SySetUsed(&pVm->aGcRoot) < 1 ){` |
|      ! 0 |  537 | `		return 0;` |
|        - |  538 | `	}` |
|       64 |  539 | `	pVm->bGcRunning = 1;` |
|       64 |  540 | `	pVm->nGcRuns++;` |
|       64 |  541 | `	sCtx.pVm = pVm;` |
|       64 |  542 | `	sCtx.pWork = &pVm->aGcWork;` |
|       64 |  543 | `	sCtx.pDead = &pVm->aGcDead;` |
|       64 |  544 | `	SySetReset(&pVm->aGcDead);` |
|        - |  545 |  |
|        - |  546 | `	/* (1) mark: subtract every reference internal to the buffered subgraph */` |
|       64 |  547 | `	nRoots = SySetUsed(&pVm->aGcRoot);` |
|  4941935 |  548 | `	for( n = 0 ; n < nRoots ; ++n ){` |
|  4941875 |  549 | `		aRoot = (VmGcRef *)SySetBasePtr(&pVm->aGcRoot);` |
|  4941875 |  550 | `		if( aRoot[n].pPtr == 0 \|\| !VmGcCollectable(pVm,aRoot[n].pPtr,aRoot[n].bMap) ){` |
|  4712458 |  551 | `			continue;` |
|        - |  552 | `		}` |
|   229421 |  553 | `		VmGcMarkGrey(&sCtx,aRoot[n].pPtr,aRoot[n].bMap);` |
|   114714 |  554 | `	}` |
|        - |  555 | `	/* (2) scan: put back what is still reachable from outside */` |
|  4941935 |  556 | `	for( n = 0 ; n < nRoots ; ++n ){` |
|  4941875 |  557 | `		aRoot = (VmGcRef *)SySetBasePtr(&pVm->aGcRoot);` |
|  4941875 |  558 | `		if( aRoot[n].pPtr == 0 \|\| !VmGcCollectable(pVm,aRoot[n].pPtr,aRoot[n].bMap) ){` |
|  4712458 |  559 | `			continue;` |
|        - |  560 | `		}` |
|   229421 |  561 | `		VmGcScan(&sCtx,aRoot[n].pPtr,aRoot[n].bMap);` |
|   114714 |  562 | `	}` |
|        - |  563 | `	/* (3) gather what stayed white */` |
|  4941935 |  564 | `	for( n = 0 ; n < nRoots ; ++n ){` |
|  4941875 |  565 | `		aRoot = (VmGcRef *)SySetBasePtr(&pVm->aGcRoot);` |
|  4941875 |  566 | `		if( aRoot[n].pPtr == 0 \|\| !VmGcCollectable(pVm,aRoot[n].pPtr,aRoot[n].bMap) ){` |
|  4712458 |  567 | `			continue;` |
|        - |  568 | `		}` |
|   229421 |  569 | `		VmGcCollectWhite(&sCtx,aRoot[n].pPtr,aRoot[n].bMap);` |
|   114714 |  570 | `	}` |
|        - |  571 | `	/* The buffer is spent either way; every surviving root is black again. */` |
|       64 |  572 | `	aRoot = (VmGcRef *)SySetBasePtr(&pVm->aGcRoot);` |
|  4941935 |  573 | `	for( n = 0 ; n < nRoots ; ++n ){` |
|  4941875 |  574 | `		if( aRoot[n].pPtr == 0 ){` |
|  4712444 |  575 | `			continue;` |
|        - |  576 | `		}` |
|   229435 |  577 | `		if( VmGcColor(aRoot[n].pPtr,aRoot[n].bMap) != PH7_GC_DEAD ){` |
|   213375 |  578 | `			VmGcSetColor(aRoot[n].pPtr,aRoot[n].bMap,PH7_GC_BLACK);` |
|   106688 |  579 | `		}` |
|   229435 |  580 | `		if( aRoot[n].bMap ){` |
|   106817 |  581 | `			((ph7_hashmap *)aRoot[n].pPtr)->nGcRoot = 0;` |
|    53413 |  582 | `		}else{` |
|   122622 |  583 | `			((ph7_class_instance *)aRoot[n].pPtr)->nGcRoot = 0;` |
|        - |  584 | `		}` |
|   114722 |  585 | `	}` |
|       64 |  586 | `	SySetReset(&pVm->aGcRoot);` |
|        - |  587 |  |
|       64 |  588 | `	nDead = SySetUsed(&pVm->aGcDead);` |
|       64 |  589 | `	if( nDead < 1 ){` |
|        - |  590 | `		/* Nothing was reclaimable. That is a full mark-and-scan spent, and the` |
|        - |  591 | `		 * threshold has to answer for it -- see VmGcAdjustThreshold. */` |
|       46 |  592 | `		VmGcAdjustThreshold(pVm,0,nRoots);` |
|       46 |  593 | `		pVm->bGcRunning = 0;` |
|       46 |  594 | `		return 0;` |
|        - |  595 | `	}` |
|        - |  596 | `	/* (4) hold everything dead while the destructors run: a destructor reads its` |
|        - |  597 | `	 * own object's properties, and those are other members of the same dead set.` |
|        - |  598 | `	 * Then put back what the MARK phase subtracted, so from here on the dead set` |
|        - |  599 | `	 * carries its TRUE refcount plus that one hold -- which is what lets a` |
|        - |  600 | `	 * destructor run arbitrary PHP over it and leave the numbers exact. */` |
|       19 |  601 | `	aDead = (VmGcRef *)SySetBasePtr(&pVm->aGcDead);` |
|    30661 |  602 | `	for( n = 0 ; n < nDead ; ++n ){` |
|    30643 |  603 | `		VmGcAddRef(aDead[n].pPtr,aDead[n].bMap,1);` |
|    15322 |  604 | `	}` |
|       19 |  605 | `	VmGcRestoreDead(&sCtx);` |
|    30661 |  606 | `	for( n = 0 ; n < nDead ; ++n ){` |
|    30643 |  607 | `		aDead = (VmGcRef *)SySetBasePtr(&pVm->aGcDead);` |
|    30643 |  608 | `		if( aDead[n].bMap == 0 ){` |
|    16037 |  609 | `			ph7_class_instance *pThis = (ph7_class_instance *)aDead[n].pPtr;` |
|    16037 |  610 | `			if( (pThis->iFlags & CLASS_INSTANCE_DTOR_CALLED) == 0 ){` |
|    16037 |  611 | `				PH7_ClassInstanceCallDestructor(pThis);` |
|     8018 |  612 | `			}` |
|     8018 |  613 | `		}` |
|    15322 |  614 | `	}` |
|        - |  615 | `	/* (5) prove it, then break the edges inside the dead set */` |
|       19 |  616 | `	if( VmGcVerifyDead(&sCtx) ){` |
|       19 |  617 | `		aDead = (VmGcRef *)SySetBasePtr(&pVm->aGcDead);` |
|    30661 |  618 | `		for( n = 0 ; n < nDead ; ++n ){` |
|    30643 |  619 | `			VmGcWalkChildren(&sCtx,aDead[n].pPtr,aDead[n].bMap,VmGcCutVisit);` |
|    15322 |  620 | `		}` |
|       19 |  621 | `		nCollected = nDead;` |
|        9 |  622 | `	}` |
|        - |  623 | `	/* (6) give back the hold. With the edges cut every member falls to zero and` |
|        - |  624 | `	 * dies through its ordinary release path -- native teardown, weak cells, the` |
|        - |  625 | `	 * slot free list, all of it. When the proof failed nothing was cut, so this` |
|        - |  626 | `	 * only puts the counts back where they were.` |
|        - |  627 | `	 *` |
|        - |  628 | `	 * bGcRunning stays UP across it. Freeing a member runs its holders' teardown,` |
|        - |  629 | `	 * which runs PHP, which reaches a fetch point -- and a collection entered from` |
|        - |  630 | `	 * there would reset the very list this loop is walking. */` |
|       19 |  631 | `	aDead = (VmGcRef *)SySetBasePtr(&pVm->aGcDead);` |
|    30661 |  632 | `	for( n = 0 ; n < nDead ; ++n ){` |
|    30643 |  633 | `		if( VmGcColor(aDead[n].pPtr,aDead[n].bMap) == PH7_GC_DEAD ){` |
|    30643 |  634 | `			VmGcSetColor(aDead[n].pPtr,aDead[n].bMap,PH7_GC_BLACK);` |
|    15321 |  635 | `		}` |
|    15322 |  636 | `	}` |
|    30661 |  637 | `	for( n = 0 ; n < nDead ; ++n ){` |
|    30643 |  638 | `		aDead = (VmGcRef *)SySetBasePtr(&pVm->aGcDead);` |
|    30643 |  639 | `		if( aDead[n].bMap ){` |
|    14607 |  640 | `			PH7_HashmapUnref((ph7_hashmap *)aDead[n].pPtr);` |
|     7304 |  641 | `		}else{` |
|    16037 |  642 | `			PH7_ClassInstanceUnref((ph7_class_instance *)aDead[n].pPtr);` |
|        - |  643 | `		}` |
|    15322 |  644 | `	}` |
|       19 |  645 | `	SySetReset(&pVm->aGcDead);` |
|       19 |  646 | `	pVm->nGcCollected += nCollected;` |
|       19 |  647 | `	VmGcAdjustThreshold(pVm,nCollected,nRoots);` |
|       19 |  648 | `	pVm->bGcRunning = 0;` |
|       19 |  649 | `	return nCollected;` |
|       34 |  650 | `}` |
|        - |  651 | `/* ------------------------------------------------------------------- lifecycle */` |
|        - |  652 |  |
|     8445 |  653 | `PH7_PRIVATE void PH7_GcInit(ph7_vm *pVm)` |
|        5 |  654 | `{` |
|     8450 |  655 | `	pVm->nGcThreshold = VM_GC_THRESHOLD_MIN;` |
|     8450 |  656 | `	SySetInit(&pVm->aGcRoot,&pVm->sAllocator,sizeof(VmGcRef));` |
|     8450 |  657 | `	SySetInit(&pVm->aGcWork,&pVm->sAllocator,sizeof(VmGcRef));` |
|     8450 |  658 | `	SySetInit(&pVm->aGcAux,&pVm->sAllocator,sizeof(VmGcRef));` |
|     8450 |  659 | `	SySetInit(&pVm->aGcDead,&pVm->sAllocator,sizeof(VmGcRef));` |
|     8450 |  660 | `}` |
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
|     6995 |  687 | `PH7_PRIVATE void PH7_GcRelease(ph7_vm *pVm)` |
|        5 |  688 | `{` |
|     7000 |  689 | `	SySetRelease(&pVm->aGcRoot);` |
|     7000 |  690 | `	SySetRelease(&pVm->aGcWork);` |
|     7000 |  691 | `	SySetRelease(&pVm->aGcAux);` |
|     7000 |  692 | `	SySetRelease(&pVm->aGcDead);` |
|     7000 |  693 | `}` |
|        - |  694 |  |
