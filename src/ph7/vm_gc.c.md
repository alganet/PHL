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
|  1594871 |   86 | `static int VmGcSlotOwned(ph7_vm *pVm,sxu32 nIdx,ph7_hashmap_node *pNode)` |
|        4 |   87 | `{` |
|  1594875 |   88 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |   89 | `		return 0;` |
|        - |   90 | `	}` |
|  1594875 |   91 | `	if( !PH7_VmSlotRegistered(&(*pVm),nIdx) ){` |
|      ! 0 |   92 | `		return 1;` |
|        - |   93 | `	}` |
|  1594875 |   94 | `	if( PH7_VmSlotPinCount(&(*pVm),nIdx) > 0 ){` |
|      ! 0 |   95 | ``		return 0; /* counted pin: the slot is BOUND to somebody else (`$o->p =& $x`) */`` |
|        - |   96 | `	}` |
|  1594875 |   97 | `	if( PH7_VmSlotEntryCount(&(*pVm),nIdx) > 0 ){` |
|       41 |   98 | `		return 0; /* a NAME holds it too */` |
|        - |   99 | `	}` |
|  1594835 |  100 | `	if( pNode == 0 ){` |
|        - |  101 | `		/* A property. Its slot is pinned VM_REF_IDX_KEEP from the moment the object` |
|        - |  102 | `		 * is built -- that pin IS the property's own hold, which is why it is not` |
|        - |  103 | `		 * disqualifying here and is for an element. Any node row on top of it is` |
|        - |  104 | `		 * somebody else pointing at the same slot. */` |
|  1474855 |  105 | `		return PH7_VmSlotNodeCount(&(*pVm),nIdx) == 0;` |
|        - |  106 | `	}` |
|   119983 |  107 | `	if( PH7_VmSlotKeepPinned(&(*pVm),nIdx) ){` |
|      ! 0 |  108 | `		return 0; /* an element pinned past its frame: a by-ref return, a capture */` |
|        - |  109 | `	}` |
|   119983 |  110 | `	return PH7_VmSlotSoleNodeIs(&(*pVm),nIdx,pNode);` |
|   796901 |  111 | `}` |
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
| 12815997 |  122 | `static int VmGcCollectable(ph7_vm *pVm,void *pCont,int bMap)` |
|        5 |  123 | `{` |
| 12816002 |  124 | `	if( bMap ){` |
|  6308557 |  125 | `		ph7_hashmap *pMap = (ph7_hashmap *)pCont;` |
|  6308557 |  126 | `		return pMap != pVm->pGlobal && pMap->pActiveSteps == 0;` |
|      ! 0 |  127 | `	}else{` |
|  6507450 |  128 | `		ph7_class_instance *pThis = (ph7_class_instance *)pCont;` |
|        - |  129 | `		/* DESTROYED: the object is already mid-release and its table is being torn` |
|        - |  130 | `		 * down under us. */` |
|  9760515 |  131 | `		return pThis->pActiveIters == 0` |
|  6507445 |  132 | `			&& (pThis->iFlags & CLASS_INSTANCE_DESTROYED) == 0;` |
|        - |  133 | `	}` |
|  6406669 |  134 | `}` |
|  1061868 |  135 | `static sxu8 VmGcColor(void *pCont,int bMap)` |
|        4 |  136 | `{` |
|   530936 |  137 | `	return bMap ? ((ph7_hashmap *)pCont)->iGcColor` |
|   745859 |  138 | `	            : ((ph7_class_instance *)pCont)->iGcColor;` |
|        4 |  139 | `}` |
|   734331 |  140 | `static void VmGcSetColor(void *pCont,int bMap,sxu8 iColor)` |
|        4 |  141 | `{` |
|   734335 |  142 | `	if( bMap ){` |
|   420489 |  143 | `		((ph7_hashmap *)pCont)->iGcColor = iColor;` |
|   210245 |  144 | `	}else{` |
|   313849 |  145 | `		((ph7_class_instance *)pCont)->iGcColor = iColor;` |
|        - |  146 | `	}` |
|   734335 |  147 | `}` |
|   211919 |  148 | `static sxi32 VmGcRefCount(void *pCont,int bMap)` |
|        4 |  149 | `{` |
|   211923 |  150 | `	return bMap ? ((ph7_hashmap *)pCont)->iRef : ((ph7_class_instance *)pCont)->iRef;` |
|        4 |  151 | `}` |
|   225486 |  152 | `static void VmGcAddRef(void *pCont,int bMap,sxi32 iDelta)` |
|        4 |  153 | `{` |
|   225490 |  154 | `	if( bMap ){` |
|   214426 |  155 | `		((ph7_hashmap *)pCont)->iRef += iDelta;` |
|   107215 |  156 | `	}else{` |
|    11067 |  157 | `		((ph7_class_instance *)pCont)->iRef += iDelta;` |
|        - |  158 | `	}` |
|   225490 |  159 | `}` |
|        - |  160 | `/* The container a value holds, or 0 when it holds none. */` |
|  1594775 |  161 | `static void * VmGcValueTarget(ph7_value *pVal,int *pbMap)` |
|        4 |  162 | `{` |
|  1594779 |  163 | `	if( pVal == 0 \|\| pVal->x.pOther == 0 ){` |
|  1067598 |  164 | `		return 0;` |
|        - |  165 | `	}` |
|   527185 |  166 | `	if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|   219016 |  167 | `		*pbMap = 1;` |
|   219016 |  168 | `		return pVal->x.pOther;` |
|        - |  169 | `	}` |
|   308173 |  170 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|    14621 |  171 | `		*pbMap = 0;` |
|    14621 |  172 | `		return pVal->x.pOther;` |
|        - |  173 | `	}` |
|   293555 |  174 | `	return 0;` |
|   796853 |  175 | `}` |
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
|   538868 |  192 | `static void VmGcWalkChildren(VmGcCtx *pCtx,void *pCont,int bMap,ProcGcVisit xVisit)` |
|        4 |  193 | `{` |
|   538872 |  194 | `	ph7_vm *pVm = pCtx->pVm;` |
|   538872 |  195 | `	int bChildMap = 0;` |
|        - |  196 | `	void *pChild;` |
|   538872 |  197 | `	if( bMap ){` |
|   322098 |  198 | `		ph7_hashmap *pMap = (ph7_hashmap *)pCont;` |
|   322098 |  199 | `		ph7_hashmap_node *pNode = pMap->pFirst;` |
|   322098 |  200 | `		sxu32 n = pMap->nEntry;` |
|   442117 |  201 | `		while( n > 0 && pNode ){` |
|   120023 |  202 | `			ph7_hashmap_node *pNext = pNode->pPrev; /* reverse link -- insertion order */` |
|   120023 |  203 | `			if( VmGcSlotOwned(&(*pVm),pNode->nValIdx,pNode) ){` |
|   119927 |  204 | `				ph7_value *pVal = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pNode->nValIdx);` |
|   119927 |  205 | `				pChild = VmGcValueTarget(pVal,&bChildMap);` |
|   119927 |  206 | `				if( pChild && VmGcCollectable(&(*pVm),pChild,bChildMap) ){` |
|     7386 |  207 | `					xVisit(pCtx,pChild,bChildMap,pVal);` |
|     3691 |  208 | `				}` |
|    59423 |  209 | `			}` |
|   120023 |  210 | `			pNode = pNext;` |
|   120023 |  211 | `			n--;` |
|        4 |  212 | `		}` |
|   161050 |  213 | `	}else{` |
|   216777 |  214 | `		ph7_class_instance *pThis = (ph7_class_instance *)pCont;` |
|   216777 |  215 | `		SyHashEntry *pEntry = SyHashFirstEntry(&pThis->hAttr);` |
|  1691693 |  216 | `		while( pEntry ){` |
|  1474919 |  217 | `			VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|  1474919 |  218 | `			SyHashEntry *pNext = SyHashEntryNext(pEntry);` |
|        - |  219 | `			/* pInst == 0 is a class STATIC or a class constant: one slot shared by` |
|        - |  220 | `			 * every instance, so it appears in every instance's table and is not` |
|        - |  221 | `			 * this object's edge at all. Counting it once per instance would` |
|        - |  222 | `			 * subtract a reference per instance for a value held once. */` |
|  1474919 |  223 | `			if( pVmAttr && PH7_VmAttrInst(pVmAttr) == pThis && VmGcSlotOwned(&(*pVm),pVmAttr->nIdx,0) ){` |
|  1474855 |  224 | `				ph7_value *pVal = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pVmAttr->nIdx);` |
|  1474855 |  225 | `				pChild = VmGcValueTarget(pVal,&bChildMap);` |
|  1474855 |  226 | `				if( pChild && VmGcCollectable(&(*pVm),pChild,bChildMap) ){` |
|   226223 |  227 | `					xVisit(pCtx,pChild,bChildMap,pVal);` |
|   113110 |  228 | `				}` |
|   737426 |  229 | `			}` |
|  1474919 |  230 | `			pEntry = pNext;` |
|        3 |  231 | `		}` |
|        - |  232 | `	}` |
|   538872 |  233 | `}` |
|        - |  234 | `/* ------------------------------------------------------------------- buffering */` |
|        - |  235 | `/*` |
|        - |  236 | ` * A container whose refcount just dropped WITHOUT reaching zero: the only event` |
|        - |  237 | ` * that can strand a cycle, and so the only one worth remembering.` |
|        - |  238 | ` */` |
| 11859050 |  239 | `PH7_PRIVATE void PH7_GcPossibleRoot(ph7_vm *pVm,void *pCont,int bMap)` |
|        5 |  240 | `{` |
|        - |  241 | `	VmGcRef sRef;` |
| 11859055 |  242 | `	if( pVm->bGcEnabled == 0 \|\| pVm->bGcRunning \|\| pVm->bInReset ){` |
|  3394424 |  243 | `		return;` |
|        - |  244 | `	}` |
| 11855657 |  245 | `	if( !VmGcCollectable(&(*pVm),pCont,bMap) ){` |
|    48720 |  246 | `		return;` |
|        - |  247 | `	}` |
| 11806942 |  248 | `	if( bMap ){` |
|  5630205 |  249 | `		ph7_hashmap *pMap = (ph7_hashmap *)pCont;` |
|  5630205 |  250 | `		if( pMap->nGcRoot != 0 ){` |
|  2181747 |  251 | `			return; /* already buffered */` |
|        - |  252 | `		}` |
|  3448463 |  253 | `		pMap->iGcColor = PH7_GC_PURPLE;` |
|  3448463 |  254 | `		pMap->nGcRoot = SySetUsed(&pVm->aGcRoot) + 1;` |
|  1723911 |  255 | `	}else{` |
|  6176742 |  256 | `		ph7_class_instance *pThis = (ph7_class_instance *)pCont;` |
|  6176742 |  257 | `		if( pThis->nGcRoot != 0 ){` |
|  4549903 |  258 | `			return;` |
|        - |  259 | `		}` |
|  1626844 |  260 | `		pThis->iGcColor = PH7_GC_PURPLE;` |
|  1626844 |  261 | `		pThis->nGcRoot = SySetUsed(&pVm->aGcRoot) + 1;` |
|        - |  262 | `	}` |
|  5075302 |  263 | `	sRef.pPtr = pCont;` |
|  5075302 |  264 | `	sRef.bMap = (sxu8)bMap;` |
|  5075302 |  265 | `	if( SySetPut(&pVm->aGcRoot,(const void *)&sRef) != SXRET_OK ){` |
|        - |  266 | `		/* No room to remember it: forget it rather than record it wrong */` |
|      ! 0 |  267 | `		if( bMap ){` |
|      ! 0 |  268 | `			((ph7_hashmap *)pCont)->nGcRoot = 0;` |
|      ! 0 |  269 | `		}else{` |
|      ! 0 |  270 | `			((ph7_class_instance *)pCont)->nGcRoot = 0;` |
|        - |  271 | `		}` |
|      ! 0 |  272 | `		return;` |
|        - |  273 | `	}` |
|  5075302 |  274 | `	if( SySetUsed(&pVm->aGcRoot) >= pVm->nGcThreshold ){` |
|        - |  275 | `		/* Ask the VM to collect at its next fetch point -- NOT here, which is the` |
|        - |  276 | `		 * middle of somebody's refcount drop and so the middle of an opcode. */` |
|       51 |  277 | `		pVm->bGcWanted = 1;` |
|       24 |  278 | `	}` |
|  5928197 |  279 | `}` |
|        - |  280 | `/*` |
|        - |  281 | ` * A buffered container is dying. Its row has to stop naming it before the memory` |
|        - |  282 | ` * goes back to the pool, or the next collection walks freed memory.` |
|        - |  283 | ` */` |
|  6442993 |  284 | `PH7_PRIVATE void PH7_GcForget(ph7_vm *pVm,void *pCont,int bMap)` |
|        5 |  285 | `{` |
|  6442998 |  286 | `	sxu32 nRoot = bMap ? ((ph7_hashmap *)pCont)->nGcRoot` |
|  3985028 |  287 | `	                   : ((ph7_class_instance *)pCont)->nGcRoot;` |
|  6442998 |  288 | `	if( nRoot != 0 && nRoot <= SySetUsed(&pVm->aGcRoot) ){` |
|  4783890 |  289 | `		VmGcRef *aRoot = (VmGcRef *)SySetBasePtr(&pVm->aGcRoot);` |
|  4783890 |  290 | `		if( aRoot[nRoot-1].pPtr == pCont ){` |
|  4783890 |  291 | `			aRoot[nRoot-1].pPtr = 0;` |
|  2391568 |  292 | `		}` |
|  2391568 |  293 | `	}` |
|  6442998 |  294 | `	if( bMap ){` |
|  4916510 |  295 | `		((ph7_hashmap *)pCont)->nGcRoot = 0;` |
|  2457970 |  296 | `	}else{` |
|  1526493 |  297 | `		((ph7_class_instance *)pCont)->nGcRoot = 0;` |
|        - |  298 | `	}` |
|  6442998 |  299 | `}` |
|        - |  300 | `/* --------------------------------------------------------------- the traversal */` |
|        - |  301 |  |
|        - |  302 | `/* The worklists are VM-owned, so a collection allocates nothing per run. */` |
|   838942 |  303 | `static void VmGcPush(SySet *pWork,void *pCont,int bMap)` |
|        4 |  304 | `{` |
|        - |  305 | `	VmGcRef sRef;` |
|   838946 |  306 | `	sRef.pPtr = pCont;` |
|   838946 |  307 | `	sRef.bMap = (sxu8)bMap;` |
|   838946 |  308 | `	SySetPut(pWork,(const void *)&sRef);` |
|   838946 |  309 | `}` |
|   107784 |  310 | `static void VmGcMarkVisit(VmGcCtx *pCtx,void *pChild,int bChildMap,ph7_value *pVal)` |
|        4 |  311 | `{` |
|    53892 |  312 | `	SXUNUSED(pVal);` |
|   107788 |  313 | `	VmGcAddRef(pChild,bChildMap,-1);` |
|   107788 |  314 | `	if( VmGcColor(pChild,bChildMap) != PH7_GC_GREY ){` |
|     3204 |  315 | `		VmGcSetColor(pChild,bChildMap,PH7_GC_GREY);` |
|     3204 |  316 | `		VmGcPush(pCtx->pWork,pChild,bChildMap);` |
|     1600 |  317 | `	}` |
|   107788 |  318 | `}` |
|   310072 |  319 | `static void VmGcDrain(VmGcCtx *pCtx,ProcGcVisit xVisit)` |
|        4 |  320 | `{` |
|   571931 |  321 | `	for(;;){` |
|   726972 |  322 | `		VmGcRef *pTop = (VmGcRef *)SySetPop(pCtx->pWork);` |
|        - |  323 | `		VmGcRef sCur;` |
|   726972 |  324 | `		if( pTop == 0 ){` |
|   310076 |  325 | `			break;` |
|        - |  326 | `		}` |
|   416900 |  327 | `		sCur = *pTop;` |
|   416900 |  328 | `		VmGcWalkChildren(pCtx,sCur.pPtr,sCur.bMap,xVisit);` |
|        4 |  329 | `	}` |
|   310076 |  330 | `}` |
|   207085 |  331 | `static void VmGcMarkGrey(VmGcCtx *pCtx,void *pRoot,int bMap)` |
|        4 |  332 | `{` |
|   207089 |  333 | `	if( VmGcColor(pRoot,bMap) == PH7_GC_GREY ){` |
|      188 |  334 | `		return;` |
|        - |  335 | `	}` |
|   206905 |  336 | `	VmGcSetColor(pRoot,bMap,PH7_GC_GREY);` |
|   206905 |  337 | `	pCtx->pWork = &pCtx->pVm->aGcWork;` |
|   206905 |  338 | `	SySetReset(pCtx->pWork);` |
|   206905 |  339 | `	VmGcPush(pCtx->pWork,pRoot,bMap);` |
|   206905 |  340 | `	VmGcDrain(pCtx,VmGcMarkVisit);` |
|   103546 |  341 | `}` |
|   104398 |  342 | `static void VmGcBlackVisit(VmGcCtx *pCtx,void *pChild,int bChildMap,ph7_value *pVal)` |
|        4 |  343 | `{` |
|    52199 |  344 | `	SXUNUSED(pVal);` |
|   104402 |  345 | `	VmGcAddRef(pChild,bChildMap,1);` |
|   104402 |  346 | `	if( VmGcColor(pChild,bChildMap) != PH7_GC_BLACK ){` |
|   103628 |  347 | `		VmGcSetColor(pChild,bChildMap,PH7_GC_BLACK);` |
|   103628 |  348 | `		VmGcPush(pCtx->pWork,pChild,bChildMap);` |
|    51812 |  349 | `	}` |
|   104402 |  350 | `}` |
|        - |  351 | `/* Runs on its OWN worklist: the scan below is mid-drain of the primary one. */` |
|   103171 |  352 | `static void VmGcScanBlack(VmGcCtx *pCtx,void *pRoot,int bMap)` |
|        4 |  353 | `{` |
|        - |  354 | `	VmGcCtx sSub;` |
|   103175 |  355 | `	sSub.pVm = pCtx->pVm;` |
|   103175 |  356 | `	sSub.pDead = pCtx->pDead;` |
|   103175 |  357 | `	sSub.pWork = &pCtx->pVm->aGcAux;` |
|   103175 |  358 | `	SySetReset(sSub.pWork);` |
|   103175 |  359 | `	VmGcSetColor(pRoot,bMap,PH7_GC_BLACK);` |
|   103175 |  360 | `	VmGcPush(sSub.pWork,pRoot,bMap);` |
|   103175 |  361 | `	VmGcDrain(&sSub,VmGcBlackVisit);` |
|   103175 |  362 | `}` |
|     7876 |  363 | `static void VmGcPushVisit(VmGcCtx *pCtx,void *pChild,int bChildMap,ph7_value *pVal)` |
|        3 |  364 | `{` |
|     3938 |  365 | `	SXUNUSED(pVal);` |
|     7879 |  366 | `	VmGcPush(pCtx->pWork,pChild,bChildMap);` |
|     7879 |  367 | `}` |
|   207085 |  368 | `static void VmGcScan(VmGcCtx *pCtx,void *pRoot,int bMap)` |
|        4 |  369 | `{` |
|   207089 |  370 | `	pCtx->pWork = &pCtx->pVm->aGcWork;` |
|   207089 |  371 | `	SySetReset(pCtx->pWork);` |
|   207089 |  372 | `	VmGcPush(pCtx->pWork,pRoot,bMap);` |
|   313636 |  373 | `	for(;;){` |
|   418664 |  374 | `		VmGcRef *pTop = (VmGcRef *)SySetPop(pCtx->pWork);` |
|        - |  375 | `		VmGcRef sCur;` |
|   418664 |  376 | `		if( pTop == 0 ){` |
|   207089 |  377 | `			break;` |
|        - |  378 | `		}` |
|   211579 |  379 | `		sCur = *pTop;` |
|   211579 |  380 | `		if( VmGcColor(sCur.pPtr,sCur.bMap) != PH7_GC_GREY ){` |
|     2966 |  381 | `			continue;` |
|        - |  382 | `		}` |
|        - |  383 | `		/* A count left over is an outside hold -- and so is a state the traversal` |
|        - |  384 | `		 * cannot see through. A container that acquired a walk in flight between` |
|        - |  385 | `		 * being buffered and being scanned is held by that cursor, which no` |
|        - |  386 | `		 * refcount names, so it is put back exactly like one that still counts. */` |
|   208613 |  387 | `		if( VmGcRefCount(sCur.pPtr,sCur.bMap) > 0` |
|   157031 |  388 | `		 \|\| !VmGcCollectable(pCtx->pVm,sCur.pPtr,sCur.bMap) ){` |
|   103175 |  389 | `			VmGcScanBlack(pCtx,sCur.pPtr,sCur.bMap);` |
|    51589 |  390 | `		}else{` |
|   105445 |  391 | `			VmGcSetColor(sCur.pPtr,sCur.bMap,PH7_GC_WHITE);` |
|   105445 |  392 | `			VmGcWalkChildren(pCtx,sCur.pPtr,sCur.bMap,VmGcPushVisit);` |
|        - |  393 | `		}` |
|        4 |  394 | `	}` |
|   207089 |  395 | `}` |
|        - |  396 | `/* Everything still white is garbage: move it to the dead list, once. */` |
|   207085 |  397 | `static void VmGcCollectWhite(VmGcCtx *pCtx,void *pRoot,int bMap)` |
|        4 |  398 | `{` |
|   207089 |  399 | `	pCtx->pWork = &pCtx->pVm->aGcWork;` |
|   207089 |  400 | `	SySetReset(pCtx->pWork);` |
|   207089 |  401 | `	VmGcPush(pCtx->pWork,pRoot,bMap);` |
|   210431 |  402 | `	for(;;){` |
|   417560 |  403 | `		VmGcRef *pTop = (VmGcRef *)SySetPop(pCtx->pWork);` |
|        - |  404 | `		VmGcRef sCur;` |
|   417560 |  405 | `		if( pTop == 0 ){` |
|   207089 |  406 | `			break;` |
|        - |  407 | `		}` |
|   210475 |  408 | `		sCur = *pTop;` |
|   210475 |  409 | `		if( VmGcColor(sCur.pPtr,sCur.bMap) != PH7_GC_WHITE ){` |
|   207169 |  410 | `			continue;` |
|        - |  411 | `		}` |
|     3307 |  412 | `		VmGcSetColor(sCur.pPtr,sCur.bMap,PH7_GC_DEAD);` |
|     3307 |  413 | `		SySetPut(pCtx->pDead,(const void *)&sCur);` |
|     3307 |  414 | `		VmGcWalkChildren(pCtx,sCur.pPtr,sCur.bMap,VmGcPushVisit);` |
|        1 |  415 | `	}` |
|   207089 |  416 | `}` |
|        - |  417 | `/* --------------------------------------------------------- proof, and the free */` |
|        - |  418 |  |
|     3386 |  419 | `static void VmGcUncountVisit(VmGcCtx *pCtx,void *pChild,int bChildMap,ph7_value *pVal)` |
|        1 |  420 | `{` |
|     1693 |  421 | `	SXUNUSED(pCtx); SXUNUSED(pVal);` |
|     3387 |  422 | `	if( VmGcColor(pChild,bChildMap) == PH7_GC_DEAD ){` |
|     3307 |  423 | `		VmGcAddRef(pChild,bChildMap,-1);` |
|     1653 |  424 | `	}` |
|     3387 |  425 | `}` |
|     3386 |  426 | `static void VmGcRecountVisit(VmGcCtx *pCtx,void *pChild,int bChildMap,ph7_value *pVal)` |
|        1 |  427 | `{` |
|     1693 |  428 | `	SXUNUSED(pCtx); SXUNUSED(pVal);` |
|     3387 |  429 | `	if( VmGcColor(pChild,bChildMap) == PH7_GC_DEAD ){` |
|     3307 |  430 | `		VmGcAddRef(pChild,bChildMap,1);` |
|     1653 |  431 | `	}` |
|     3387 |  432 | `}` |
|     3386 |  433 | `static void VmGcCutVisit(VmGcCtx *pCtx,void *pChild,int bChildMap,ph7_value *pVal)` |
|        1 |  434 | `{` |
|     1693 |  435 | `	SXUNUSED(pCtx);` |
|     3387 |  436 | `	if( VmGcColor(pChild,bChildMap) == PH7_GC_DEAD ){` |
|        - |  437 | `		/* The child is pinned for the duration, so this drops the edge and nothing else */` |
|     3307 |  438 | `		PH7_MemObjRelease(pVal);` |
|     1653 |  439 | `	}` |
|     3387 |  440 | `}` |
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
|     3386 |  455 | `static void VmGcRestoreVisit(VmGcCtx *pCtx,void *pChild,int bChildMap,ph7_value *pVal)` |
|        1 |  456 | `{` |
|     1693 |  457 | `	SXUNUSED(pCtx); SXUNUSED(pVal);` |
|     3387 |  458 | `	VmGcAddRef(pChild,bChildMap,1);` |
|     3387 |  459 | `}` |
|       10 |  460 | `static void VmGcRestoreDead(VmGcCtx *pCtx)` |
|        1 |  461 | `{` |
|       11 |  462 | `	VmGcRef *aDead = (VmGcRef *)SySetBasePtr(pCtx->pDead);` |
|       11 |  463 | `	sxu32 n, nDead = SySetUsed(pCtx->pDead);` |
|     3317 |  464 | `	for( n = 0 ; n < nDead ; ++n ){` |
|     3307 |  465 | `		VmGcWalkChildren(pCtx,aDead[n].pPtr,aDead[n].bMap,VmGcRestoreVisit);` |
|     1654 |  466 | `	}` |
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
|     3317 |  485 | `	for( n = 0 ; n < nDead ; ++n ){` |
|     3307 |  486 | `		VmGcWalkChildren(pCtx,aDead[n].pPtr,aDead[n].bMap,VmGcUncountVisit);` |
|     1654 |  487 | `	}` |
|     3317 |  488 | `	for( n = 0 ; n < nDead ; ++n ){` |
|     3307 |  489 | `		if( VmGcRefCount(aDead[n].pPtr,aDead[n].bMap) != 1 ){` |
|      ! 0 |  490 | `			bOk = FALSE;` |
|      ! 0 |  491 | `			break;` |
|        - |  492 | `		}` |
|     1654 |  493 | `	}` |
|     3317 |  494 | `	for( n = 0 ; n < nDead ; ++n ){` |
|     3307 |  495 | `		VmGcWalkChildren(pCtx,aDead[n].pPtr,aDead[n].bMap,VmGcRecountVisit);` |
|     1654 |  496 | `	}` |
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
|        - |  515 | ` * shared here and cannot be timed.` |
|        - |  516 | ` */` |
|       50 |  517 | `static void VmGcAdjustThreshold(ph7_vm *pVm,sxu32 nCollected,sxu32 nRoots)` |
|        4 |  518 | `{` |
|       54 |  519 | `	if( nCollected * 4 < nRoots ){` |
|       54 |  520 | `		if( pVm->nGcThreshold < VM_GC_THRESHOLD_MAX ){` |
|       30 |  521 | `			pVm->nGcThreshold <<= 1;` |
|       13 |  522 | `		}` |
|       29 |  523 | `	}else{` |
|      ! 0 |  524 | `		pVm->nGcThreshold = VM_GC_THRESHOLD_MIN;` |
|        - |  525 | `	}` |
|       54 |  526 | `}` |
|       50 |  527 | `PH7_PRIVATE sxu32 PH7_GcCollect(ph7_vm *pVm)` |
|        4 |  528 | `{` |
|        - |  529 | `	VmGcCtx sCtx;` |
|        - |  530 | `	VmGcRef *aRoot, *aDead;` |
|       54 |  531 | `	sxu32 n, nRoots, nDead, nCollected = 0;` |
|       54 |  532 | `	pVm->bGcWanted = 0;` |
|       54 |  533 | `	if( pVm->bGcRunning \|\| pVm->bInReset ){` |
|      ! 0 |  534 | `		return 0;` |
|        - |  535 | `	}` |
|       54 |  536 | `	if( SySetUsed(&pVm->aGcRoot) < 1 ){` |
|      ! 0 |  537 | `		return 0;` |
|        - |  538 | `	}` |
|       54 |  539 | `	pVm->bGcRunning = 1;` |
|       54 |  540 | `	pVm->nGcRuns++;` |
|       54 |  541 | `	sCtx.pVm = pVm;` |
|       54 |  542 | `	sCtx.pWork = &pVm->aGcWork;` |
|       54 |  543 | `	sCtx.pDead = &pVm->aGcDead;` |
|       54 |  544 | `	SySetReset(&pVm->aGcDead);` |
|        - |  545 |  |
|        - |  546 | `	/* (1) mark: subtract every reference internal to the buffered subgraph */` |
|       54 |  547 | `	nRoots = SySetUsed(&pVm->aGcRoot);` |
|  4573179 |  548 | `	for( n = 0 ; n < nRoots ; ++n ){` |
|  4573129 |  549 | `		aRoot = (VmGcRef *)SySetBasePtr(&pVm->aGcRoot);` |
|  4573129 |  550 | `		if( aRoot[n].pPtr == 0 \|\| !VmGcCollectable(pVm,aRoot[n].pPtr,aRoot[n].bMap) ){` |
|  4366044 |  551 | `			continue;` |
|        - |  552 | `		}` |
|   207089 |  553 | `		VmGcMarkGrey(&sCtx,aRoot[n].pPtr,aRoot[n].bMap);` |
|   103546 |  554 | `	}` |
|        - |  555 | `	/* (2) scan: put back what is still reachable from outside */` |
|  4573179 |  556 | `	for( n = 0 ; n < nRoots ; ++n ){` |
|  4573129 |  557 | `		aRoot = (VmGcRef *)SySetBasePtr(&pVm->aGcRoot);` |
|  4573129 |  558 | `		if( aRoot[n].pPtr == 0 \|\| !VmGcCollectable(pVm,aRoot[n].pPtr,aRoot[n].bMap) ){` |
|  4366044 |  559 | `			continue;` |
|        - |  560 | `		}` |
|   207089 |  561 | `		VmGcScan(&sCtx,aRoot[n].pPtr,aRoot[n].bMap);` |
|   103546 |  562 | `	}` |
|        - |  563 | `	/* (3) gather what stayed white */` |
|  4573179 |  564 | `	for( n = 0 ; n < nRoots ; ++n ){` |
|  4573129 |  565 | `		aRoot = (VmGcRef *)SySetBasePtr(&pVm->aGcRoot);` |
|  4573129 |  566 | `		if( aRoot[n].pPtr == 0 \|\| !VmGcCollectable(pVm,aRoot[n].pPtr,aRoot[n].bMap) ){` |
|  4366044 |  567 | `			continue;` |
|        - |  568 | `		}` |
|   207089 |  569 | `		VmGcCollectWhite(&sCtx,aRoot[n].pPtr,aRoot[n].bMap);` |
|   103546 |  570 | `	}` |
|        - |  571 | `	/* The buffer is spent either way; every surviving root is black again. */` |
|       54 |  572 | `	aRoot = (VmGcRef *)SySetBasePtr(&pVm->aGcRoot);` |
|  4573179 |  573 | `	for( n = 0 ; n < nRoots ; ++n ){` |
|  4573129 |  574 | `		if( aRoot[n].pPtr == 0 ){` |
|  4366038 |  575 | `			continue;` |
|        - |  576 | `		}` |
|   207095 |  577 | `		if( VmGcColor(aRoot[n].pPtr,aRoot[n].bMap) != PH7_GC_DEAD ){` |
|   205385 |  578 | `			VmGcSetColor(aRoot[n].pPtr,aRoot[n].bMap,PH7_GC_BLACK);` |
|   102690 |  579 | `		}` |
|   207095 |  580 | `		if( aRoot[n].bMap ){` |
|   103287 |  581 | `			((ph7_hashmap *)aRoot[n].pPtr)->nGcRoot = 0;` |
|    51645 |  582 | `		}else{` |
|   103811 |  583 | `			((ph7_class_instance *)aRoot[n].pPtr)->nGcRoot = 0;` |
|        - |  584 | `		}` |
|   103549 |  585 | `	}` |
|       54 |  586 | `	SySetReset(&pVm->aGcRoot);` |
|        - |  587 |  |
|       54 |  588 | `	nDead = SySetUsed(&pVm->aGcDead);` |
|       54 |  589 | `	if( nDead < 1 ){` |
|        - |  590 | `		/* Nothing was reclaimable. That is a full mark-and-scan spent, and the` |
|        - |  591 | `		 * threshold has to answer for it -- see VmGcAdjustThreshold. */` |
|       44 |  592 | `		VmGcAdjustThreshold(pVm,0,nRoots);` |
|       44 |  593 | `		pVm->bGcRunning = 0;` |
|       44 |  594 | `		return 0;` |
|        - |  595 | `	}` |
|        - |  596 | `	/* (4) hold everything dead while the destructors run: a destructor reads its` |
|        - |  597 | `	 * own object's properties, and those are other members of the same dead set.` |
|        - |  598 | `	 * Then put back what the MARK phase subtracted, so from here on the dead set` |
|        - |  599 | `	 * carries its TRUE refcount plus that one hold -- which is what lets a` |
|        - |  600 | `	 * destructor run arbitrary PHP over it and leave the numbers exact. */` |
|       11 |  601 | `	aDead = (VmGcRef *)SySetBasePtr(&pVm->aGcDead);` |
|     3317 |  602 | `	for( n = 0 ; n < nDead ; ++n ){` |
|     3307 |  603 | `		VmGcAddRef(aDead[n].pPtr,aDead[n].bMap,1);` |
|     1654 |  604 | `	}` |
|       11 |  605 | `	VmGcRestoreDead(&sCtx);` |
|     3317 |  606 | `	for( n = 0 ; n < nDead ; ++n ){` |
|     3307 |  607 | `		aDead = (VmGcRef *)SySetBasePtr(&pVm->aGcDead);` |
|     3307 |  608 | `		if( aDead[n].bMap == 0 ){` |
|     1685 |  609 | `			ph7_class_instance *pThis = (ph7_class_instance *)aDead[n].pPtr;` |
|     1685 |  610 | `			if( (pThis->iFlags & CLASS_INSTANCE_DTOR_CALLED) == 0 ){` |
|     1685 |  611 | `				PH7_ClassInstanceCallDestructor(pThis);` |
|      842 |  612 | `			}` |
|      842 |  613 | `		}` |
|     1654 |  614 | `	}` |
|        - |  615 | `	/* (5) prove it, then break the edges inside the dead set */` |
|       11 |  616 | `	if( VmGcVerifyDead(&sCtx) ){` |
|       11 |  617 | `		aDead = (VmGcRef *)SySetBasePtr(&pVm->aGcDead);` |
|     3317 |  618 | `		for( n = 0 ; n < nDead ; ++n ){` |
|     3307 |  619 | `			VmGcWalkChildren(&sCtx,aDead[n].pPtr,aDead[n].bMap,VmGcCutVisit);` |
|     1654 |  620 | `		}` |
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
|     3317 |  632 | `	for( n = 0 ; n < nDead ; ++n ){` |
|     3307 |  633 | `		if( VmGcColor(aDead[n].pPtr,aDead[n].bMap) == PH7_GC_DEAD ){` |
|     3307 |  634 | `			VmGcSetColor(aDead[n].pPtr,aDead[n].bMap,PH7_GC_BLACK);` |
|     1653 |  635 | `		}` |
|     1654 |  636 | `	}` |
|     3317 |  637 | `	for( n = 0 ; n < nDead ; ++n ){` |
|     3307 |  638 | `		aDead = (VmGcRef *)SySetBasePtr(&pVm->aGcDead);` |
|     3307 |  639 | `		if( aDead[n].bMap ){` |
|     1623 |  640 | `			PH7_HashmapUnref((ph7_hashmap *)aDead[n].pPtr);` |
|      812 |  641 | `		}else{` |
|     1685 |  642 | `			PH7_ClassInstanceUnref((ph7_class_instance *)aDead[n].pPtr);` |
|        - |  643 | `		}` |
|     1654 |  644 | `	}` |
|       11 |  645 | `	SySetReset(&pVm->aGcDead);` |
|       11 |  646 | `	pVm->nGcCollected += nCollected;` |
|       11 |  647 | `	VmGcAdjustThreshold(pVm,nCollected,nRoots);` |
|       11 |  648 | `	pVm->bGcRunning = 0;` |
|       11 |  649 | `	return nCollected;` |
|       29 |  650 | `}` |
|        - |  651 | `/* ------------------------------------------------------------------- lifecycle */` |
|        - |  652 |  |
|     7925 |  653 | `PH7_PRIVATE void PH7_GcInit(ph7_vm *pVm)` |
|        5 |  654 | `{` |
|     7930 |  655 | `	pVm->nGcThreshold = VM_GC_THRESHOLD_MIN;` |
|     7930 |  656 | `	SySetInit(&pVm->aGcRoot,&pVm->sAllocator,sizeof(VmGcRef));` |
|     7930 |  657 | `	SySetInit(&pVm->aGcWork,&pVm->sAllocator,sizeof(VmGcRef));` |
|     7930 |  658 | `	SySetInit(&pVm->aGcAux,&pVm->sAllocator,sizeof(VmGcRef));` |
|     7930 |  659 | `	SySetInit(&pVm->aGcDead,&pVm->sAllocator,sizeof(VmGcRef));` |
|     7930 |  660 | `}` |
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
|     6701 |  687 | `PH7_PRIVATE void PH7_GcRelease(ph7_vm *pVm)` |
|        5 |  688 | `{` |
|     6706 |  689 | `	SySetRelease(&pVm->aGcRoot);` |
|     6706 |  690 | `	SySetRelease(&pVm->aGcWork);` |
|     6706 |  691 | `	SySetRelease(&pVm->aGcAux);` |
|     6706 |  692 | `	SySetRelease(&pVm->aGcDead);` |
|     6706 |  693 | `}` |
|        - |  694 |  |
