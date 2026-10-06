# src/ph7/vm_builtin_var.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 486/524 lines (92.75%)

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
|        - |    8 | ` * Section:` |
|        - |    9 | ` *    Variable-introspection builtins: isset, unset, get_defined_vars,` |
|        - |   10 | ` *    gettype, get_resource_type, var_dump, print_r and var_export.` |
|        - |   11 | ` *    Registration rows stay in vm.c's aVmFunc[].` |
|        - |   12 | ` * Status:` |
|        - |   13 | ` *    Stable.` |
|        - |   14 | ` */` |
|        - |   15 | `/*` |
|        - |   16 | ` * bool isset($var,...)` |
|        - |   17 | ` *  Finds out whether a variable is set.` |
|        - |   18 | ` * Parameters` |
|        - |   19 | ` *  One or more variable to check.` |
|        - |   20 | ` * Return` |
|        - |   21 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - |   22 | ` */` |
|   180264 |   23 | `PH7_PRIVATE int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |   24 | `{` |
|        - |   25 | `	ph7_value *pObj;` |
|   180269 |   26 | `	int res = 0;` |
|        - |   27 | `	int i;` |
|   180269 |   28 | `	if( nArg < 1 ){` |
|        - |   29 | `		/* Missing arguments,return false */` |
|      ! 0 |   30 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 |   31 | `		return SXRET_OK;` |
|        - |   32 | `	}` |
|        - |   33 | `	/* Iterate over available arguments */` |
|   220931 |   34 | `	for( i = 0 ; i < nArg ; ++i ){` |
|   180269 |   35 | `		pObj = apArg[i];` |
|   180269 |   36 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|        - |   37 | `			/* Skip the "expecting a variable" warning for MEMOBJ_BOOL —` |
|        - |   38 | `			 * synthesized by LOAD_IDX iP2=4 (ArrayAccess::offsetExists) and` |
|        - |   39 | `			 * by anyone passing a bool literal (rare, harmless). */` |
|   180025 |   40 | `			if( (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|        - |   41 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 |   42 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 |   43 | `			}` |
|    89982 |   44 | `		}` |
|   180269 |   45 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|   180269 |   46 | `		if( !res ){` |
|        - |   47 | `			/* Variable not set,return FALSE */` |
|   139607 |   48 | `			ph7_result_bool(pCtx,0);` |
|   139607 |   49 | `			return SXRET_OK;` |
|        - |   50 | `		}` |
|    20332 |   51 | `	}` |
|        - |   52 | `	/* All given variable are set,return TRUE */` |
|    40667 |   53 | `	ph7_result_bool(pCtx,1);` |
|    40667 |   54 | `	return SXRET_OK;` |
|    90109 |   55 | `}` |
|        - |   56 | `/*` |
|        - |   57 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - |   58 | ` * frame,the reference table and discard it's contents.` |
|        - |   59 | ` * This function never fail and always return SXRET_OK.` |
|        - |   60 | ` */` |
|        - |   61 | `/*` |
|        - |   62 | ` * unset($name) for a SIMPLE variable: drop exactly one NAME binding.` |
|        - |   63 | ` *` |
|        - |   64 | ` * PH7 routed every unset() through PH7_VmUnsetMemObj(), which releases the shared memory` |
|        - |   65 | ` * object and then has VmRefObjUnlink() delete EVERY name bound to that slot and unlink` |
|        - |   66 | ` * EVERY array node pointing at it. For an aliased variable that is data loss, not an` |
|        - |   67 | `` * unset: `$b = &$a; unset($b);` destroyed $a, `$r = &$arr[$k]; unset($r);` deleted the`` |
|        - |   68 | `` * array element, and `function f(&$p){ unset($p); }` wiped out the caller's variable.`` |
|        - |   69 | ` * php removes the NAME and nothing else; the value survives as long as anything still` |
|        - |   70 | ` * refers to it.` |
|        - |   71 | ` *` |
|        - |   72 | ` * So: unlink this one name, forget it in the slot's reference record, and release the` |
|        - |   73 | ` * slot only once no name and no array entry still holds it.` |
|        - |   74 | ` */` |
|        - |   75 | `/*` |
|        - |   76 | `` * bNameGuard says the NAME is the spelling the user unset — `unset($GLOBALS)` must be`` |
|        - |   77 | `` * refused there. `unset($GLOBALS['GLOBALS'])` reaches the same body through the element`` |
|        - |   78 | ` * path with the guard OFF: its target is the ordinary symbol-table entry that` |
|        - |   79 | `` * `$GLOBALS['GLOBALS'] = 5` creates, not the superglobal (which the slot test below`` |
|        - |   80 | ` * still protects).` |
|        - |   81 | ` */` |
|    13422 |   82 | `PH7_PRIVATE sxi32 VmUnsetVarByNameEx(ph7_vm *pVm,VmFrame *pFrame,const char *zName,sxu32 nByte,` |
|        - |   83 | `	int bNameGuard)` |
|        5 |   84 | `{` |
|        - |   85 | `	SyHashEntry *pEntry;` |
|        - |   86 | `	int bRegistered;` |
|        - |   87 | `	sxu32 nIdx;` |
|        - |   88 | `	/* php 8.1 forbids unset($GLOBALS) outright. Checked by NAME: the superglobal is not an` |
|        - |   89 | `	 * ordinary hVar binding, so the slot-index test below never sees it. */` |
|    13427 |   90 | `	if( bNameGuard && nByte == sizeof("GLOBALS")-1 && SyMemcmp(zName,"GLOBALS",nByte) == 0 ){` |
|        3 |   91 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |   92 | `			"$GLOBALS can only be modified using the $GLOBALS[$name] = $value syntax");` |
|        3 |   93 | `		pVm->iExitStatus = 255;` |
|        3 |   94 | `		pVm->bHaltRequested = 1;` |
|        3 |   95 | `		return PH7_ABORT;` |
|        - |   96 | `	}` |
|    13425 |   97 | `	pEntry = SyHashGet(&pFrame->hVar,(const void *)zName,nByte);` |
|    13425 |   98 | `	if( pEntry == 0 ){` |
|        - |   99 | `		/* No such variable: unset() is a no-op on an undefined name, as in php */` |
|     1901 |  100 | `		return SXRET_OK;` |
|        - |  101 | `	}` |
|        - |  102 | `	/* The binding about to go may be memoized on the frame (see VmFrame). */` |
|    11529 |  103 | `	VmVarMemoFlush(pFrame);` |
|    11529 |  104 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|    11529 |  105 | `	if( nIdx == pVm->nGlobalIdx ){` |
|      ! 0 |  106 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  107 | `			"$GLOBALS can only be modified using the $GLOBALS[$name] = $value syntax");` |
|      ! 0 |  108 | `		pVm->iExitStatus = 255;` |
|      ! 0 |  109 | `		pVm->bHaltRequested = 1;` |
|      ! 0 |  110 | `		return PH7_ABORT;` |
|        - |  111 | `	}` |
|    11529 |  112 | `	bRegistered = PH7_VmSlotRegistered(&(*pVm),nIdx);` |
|        - |  113 | `	/*` |
|        - |  114 | `	 * A GLOBAL variable is ALSO an entry of the $GLOBALS array, and that node points at the` |
|        - |  115 | `	 * same slot. php's unset($x) in global scope drops $GLOBALS['x'] too, so unlink the node` |
|        - |  116 | `	 * — otherwise it stays a live holder and the value is never released ($o = new D;` |
|        - |  117 | `	 * unset($o); stopped running the destructor). Clear the reference table's row for the` |
|        - |  118 | `	 * node BEFORE unlinking it: unlinking frees the node, and the holder count below would` |
|        - |  119 | `	 * otherwise dereference freed memory.` |
|        - |  120 | `	 */` |
|    11529 |  121 | `	if( pFrame->pParent == 0 ){` |
|    11367 |  122 | `		ph7_value *pGlobals = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pVm->nGlobalIdx);` |
|    11367 |  123 | `		if( pGlobals && (pGlobals->iFlags & MEMOBJ_HASHMAP) ){` |
|    11367 |  124 | `			ph7_hashmap_node *pNode = 0;` |
|        - |  125 | `			ph7_value sKey;` |
|        - |  126 | `			SyString sName;` |
|    11367 |  127 | `			PH7_MemObjInit(&(*pVm),&sKey);` |
|    11367 |  128 | `			SyStringInitFromBuf(&sName,zName,nByte);` |
|    11367 |  129 | `			PH7_MemObjInitFromString(&(*pVm),&sKey,&sName);` |
|    11362 |  130 | `			if( SXRET_OK == PH7_HashmapLookup((ph7_hashmap *)pGlobals->x.pOther,&sKey,&pNode)` |
|    11359 |  131 | `			 && pNode && pNode->nValIdx == nIdx ){` |
|    11351 |  132 | `				if( bRegistered ){` |
|    11351 |  133 | `					PH7_VmRefObjRemove(&(*pVm),nIdx,0,pNode);` |
|     5668 |  134 | `				}` |
|    11351 |  135 | `				PH7_HashmapUnlinkNode(pNode,FALSE);` |
|     5668 |  136 | `			}` |
|    11367 |  137 | `			PH7_MemObjRelease(&sKey);` |
|     5676 |  138 | `		}` |
|     5676 |  139 | `	}` |
|        - |  140 |  |
|        - |  141 | `	/* The frame's own "release this reference at exit" row for the binding about to go:` |
|        - |  142 | `	 * the entry is freed below, so the row would dangle (and a foreach value variable` |
|        - |  143 | `	 * unset once per loop filed one row per loop, which nothing consumed until the` |
|        - |  144 | `	 * function returned). */` |
|    11529 |  145 | `	VmDropFrameRefEntry(&(*pVm),nIdx,pEntry);` |
|    11529 |  146 | `	if( !bRegistered ){` |
|        - |  147 | `		/* Unaliased variable: nobody else holds the slot, so the old path is right */` |
|      ! 0 |  148 | `		SyHashDeleteEntry2(pEntry);` |
|      ! 0 |  149 | `		PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|        - |  150 | `		/* The slot is back in the free pool; drop its stale local-teardown entry so a` |
|        - |  151 | `		 * later reuse of the index (e.g. an object property) is not double-freed when` |
|        - |  152 | `		 * this frame exits. */` |
|      ! 0 |  153 | `		VmDropFrameLocalSlot(&(*pVm),nIdx);` |
|      ! 0 |  154 | `		return SXRET_OK;` |
|        - |  155 | `	}` |
|        - |  156 | `	{` |
|        - |  157 | `		/* Forget THIS name in the slot's reference record (leave the others alone) */` |
|    11529 |  158 | `		PH7_VmRefObjRemove(&(*pVm),nIdx,pEntry,0);` |
|    11529 |  159 | `		SyHashDeleteEntry2(pEntry);` |
|        - |  160 | `		/* The value goes with the LAST holder and not before — the one rule, counted in` |
|        - |  161 | `		 * one place: other names, array nodes that still point here, and a PIN (a static's` |
|        - |  162 | ``		 * storage, a `use (&$x)` capture, a reference-bound property), which is a holder`` |
|        - |  163 | `		 * this table cannot name. Unsetting the name of a static used to release the` |
|        - |  164 | `		 * static's value, so the next call started over from the initializer. */` |
|    11529 |  165 | `		PH7_VmReleaseUnheldSlot(&(*pVm),nIdx);` |
|        - |  166 | `	}` |
|    11529 |  167 | `	return SXRET_OK;` |
|     6706 |  168 | `}` |
|     9007 |  169 | `PH7_PRIVATE sxi32 VmUnsetVarByName(ph7_vm *pVm,VmFrame *pFrame,const char *zName,sxu32 nByte)` |
|        5 |  170 | `{` |
|     9012 |  171 | `	return VmUnsetVarByNameEx(&(*pVm),pFrame,zName,nByte,TRUE);` |
|        5 |  172 | `}` |
| 28402607 |  173 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        5 |  174 | `{` |
|        - |  175 | `	ph7_value *pObj;` |
| 28402612 |  176 | `	pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nObjIdx);` |
| 28402612 |  177 | `	if( pObj ){` |
|        - |  178 | `		/* Release the object */` |
| 28402612 |  179 | `		PH7_MemObjRelease(pObj);` |
| 14200469 |  180 | `	}` |
|        - |  181 | `	/* Remove old reference links. The permanent pin is read BEFORE the unlink --` |
|        - |  182 | `	 * it is what decides whether the index goes back to the free pool, and the` |
|        - |  183 | `	 * unlink is what takes the answer away.` |
|        - |  184 | `	 *` |
|        - |  185 | `	 * A bare mark answers all three of those questions with one compare, and it is` |
|        - |  186 | `	 * what the overwhelming majority of the slots reaching here carry -- see` |
|        - |  187 | `	 * PH7_VmSlotDropIfBare. It has no permanent pin by construction, so the index` |
|        - |  188 | `	 * goes back to the pool whether or not the caller forced it. */` |
| 28402612 |  189 | `	if( PH7_VmSlotDropIfBare(&(*pVm),nObjIdx) ){` |
| 28386262 |  190 | `		VmMemPoolFreeSlot(&pVm->aMemObj,nObjIdx);` |
| 28386262 |  191 | `		return SXRET_OK;` |
|        - |  192 | `	}` |
|    16355 |  193 | `	if( PH7_VmSlotRegistered(&(*pVm),nObjIdx) ){` |
|    16305 |  194 | `		int bKeep = PH7_VmSlotKeepPinned(&(*pVm),nObjIdx);` |
|        - |  195 | `		/* Unlink from the reference table */` |
|    16305 |  196 | `		PH7_VmSlotUnlink(&(*pVm),nObjIdx);` |
|    16305 |  197 | `		if( (bForce == TRUE) \|\| bKeep == 0 ){` |
|        - |  198 | `			/* Restore to the free list */` |
|    16305 |  199 | `			VmMemPoolFreeSlot(&pVm->aMemObj,nObjIdx);` |
|     8145 |  200 | `		}` |
|     8145 |  201 | `	}` |
|    16355 |  202 | `	return SXRET_OK;` |
| 14200474 |  203 | `}` |
|        - |  204 | `/*` |
|        - |  205 | ` * void unset($var,...)` |
|        - |  206 | ` *   Unset one or more given variable.` |
|        - |  207 | ` * Parameters` |
|        - |  208 | ` *  One or more variable to unset.` |
|        - |  209 | ` * Return` |
|        - |  210 | ` *  Nothing.` |
|        - |  211 | ` */` |
|     1606 |  212 | `PH7_PRIVATE int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  213 | `{` |
|        - |  214 | `	ph7_value *pObj;` |
|        - |  215 | `	ph7_vm *pVm;` |
|        - |  216 | `	int i;` |
|        - |  217 | `	/* Point to the target VM */` |
|     1611 |  218 | `	pVm = pCtx->pVm;` |
|        - |  219 | `	/* Iterate and unset */` |
|     3217 |  220 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     1611 |  221 | `		pObj = apArg[i];` |
|     1611 |  222 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|     1609 |  223 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  224 | `				/* Throw an error */` |
|      ! 0 |  225 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 |  226 | `			}` |
|      807 |  227 | `		}else{` |
|        3 |  228 | `			sxu32 nIdx = pObj->nIdx;` |
|        3 |  229 | `			if( nIdx == pVm->nGlobalIdx ){` |
|        - |  230 | `				/* php 8.1: unset($GLOBALS) is forbidden — the same fatal as` |
|        - |  231 | `				 * re-assigning it (compile-time in php, raised here). */` |
|      ! 0 |  232 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  233 | `					"$GLOBALS can only be modified using the $GLOBALS[$name] = $value syntax");` |
|      ! 0 |  234 | `				pVm->iExitStatus = 255;` |
|      ! 0 |  235 | `				pVm->bHaltRequested = 1;` |
|      ! 0 |  236 | `				return PH7_ABORT;` |
|        - |  237 | `			}` |
|        3 |  238 | `			PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|        - |  239 | `			/* Drop the stale local-teardown entry for the freed slot (see` |
|        - |  240 | `			 * VmDropFrameLocalSlot) so a later index reuse is not double-freed. */` |
|        3 |  241 | `			VmDropFrameLocalSlot(&(*pVm),nIdx);` |
|        - |  242 | `		}` |
|      808 |  243 | `	}` |
|     1611 |  244 | `	return SXRET_OK;` |
|      808 |  245 | `}` |
|        - |  246 | `/*` |
|        - |  247 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - |  248 | ` */` |
|    16524 |  249 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        4 |  250 | `{` |
|    16528 |  251 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|    16528 |  252 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  253 | `	ph7_value *pObj;` |
|        - |  254 | `	sxu32 nIdx;` |
|        - |  255 | `	/* php excludes $this from get_defined_vars() (it is bound implicitly, not a` |
|        - |  256 | `	 * declared local). PHL keeps it in the frame's hVar, so skip it here. */` |
|    16524 |  257 | `	if( pEntry->nKeyLen == sizeof("this")-1` |
|     9607 |  258 | `	 && SyMemcmp(pEntry->pKey,"this",sizeof("this")-1) == 0 ){` |
|        6 |  259 | `		return SXRET_OK;` |
|        - |  260 | `	}` |
|        - |  261 | `	/* Engine temporaries (a foreach destructuring/target slot) are not variables the` |
|        - |  262 | `	 * program declared — php compiles those into slots with no name at all. */` |
|    16524 |  263 | `	if( PH7_VmVarNameIsInternal((const char *)pEntry->pKey,pEntry->nKeyLen) ){` |
|      881 |  264 | `		return SXRET_OK;` |
|        - |  265 | `	}` |
|        - |  266 | `	/* Extract the memory object */` |
|    15644 |  267 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|    15644 |  268 | `	pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nIdx);` |
|    15644 |  269 | `	if( pObj ){` |
|    15644 |  270 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|    15632 |  271 | `			if( pEntry->nKeyLen > 0 ){` |
|        - |  272 | `				SyString sName;` |
|        - |  273 | `				ph7_value sKey;` |
|        - |  274 | `				/* Perform the insertion (pObj may point into pVm->aMemObj; the` |
|        - |  275 | `				 * inserter snapshots the source before reserving, so the pool may` |
|        - |  276 | `				 * safely move underneath it — see HashmapInsertIntKey/BlobKey). */` |
|    15632 |  277 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|    15632 |  278 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|    15632 |  279 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|    15632 |  280 | `				PH7_MemObjRelease(&sKey);` |
|     7814 |  281 | `			}` |
|     7814 |  282 | `		}` |
|     7820 |  283 | `	}` |
|    15644 |  284 | `	return SXRET_OK;` |
|     8266 |  285 | `}` |
|        - |  286 | `/*` |
|        - |  287 | ` * array get_defined_vars(void)` |
|        - |  288 | ` *  Returns an array of all defined variables.` |
|        - |  289 | ` * Parameter` |
|        - |  290 | ` *  None` |
|        - |  291 | ` * Return` |
|        - |  292 | ` *  An array with all the variables defined in the current scope.` |
|        - |  293 | ` */` |
|       80 |  294 | `PH7_PRIVATE int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        4 |  295 | `{` |
|       84 |  296 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  297 | `	ph7_value *pArray;` |
|        - |  298 | `	VmFrame *pFrame;` |
|        - |  299 | `	sxi32 rc;` |
|        - |  300 | `	/* php's one screen before the walk: the scope it would report is the CALLER's,` |
|        - |  301 | `	 * and a dynamic call has an internal frame where that caller should be. */` |
|       84 |  302 | `	if( (rc = PH7_VmForbidDynamicCall(pCtx)) != PH7_OK ){` |
|       11 |  303 | `		return rc;` |
|        - |  304 | `	}` |
|        - |  305 | `	/* Create a new array */` |
|       74 |  306 | `	pArray = ph7_context_new_array(pCtx);` |
|       74 |  307 | ` 	if( pArray == 0 ){` |
|      ! 0 |  308 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  309 | `		SXUNUSED(apArg);` |
|        - |  310 | `		/* Return NULL */` |
|      ! 0 |  311 | `		ph7_result_null(pCtx);` |
|      ! 0 |  312 | `		return SXRET_OK;` |
|        - |  313 | `	}` |
|        - |  314 | `	/* A try/catch block pushes an EXCEPTION frame that holds no variables of its` |
|        - |  315 | `	 * own, so the enclosing function (or global) frame is the one php reports.` |
|        - |  316 | `	 * Reading pVm->pFrame raw answered [] for every call inside a try/catch —` |
|        - |  317 | `	 * including the superglobal test below, which saw a non-NULL pParent and` |
|        - |  318 | `	 * dropped $argv/$_SERVER at global scope. Every other frame-lookup site` |
|        - |  319 | `	 * (VmExtractMemObj, compact(), func_get_args()) already skips them. */` |
|       74 |  320 | `	pFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|        - |  321 | `	/* Superglobals appear in get_defined_vars() ONLY at the global scope (php).` |
|        - |  322 | `	 * Inside a function the result is the local symbol table alone — a leak of` |
|        - |  323 | `	 * $argv/$_SERVER/... into every function's scope breaks callers that treat the` |
|        - |  324 | `	 * keys as real locals (e.g. PHPUnit's doubled-method template reflects each` |
|        - |  325 | `	 * get_defined_vars() name as a ReflectionParameter). */` |
|       74 |  326 | `	if( pFrame->pParent == 0 ){` |
|       14 |  327 | `		SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        6 |  328 | `	}` |
|        - |  329 | `	/* Then variables defined in the current frame, in DECLARATION order.` |
|        - |  330 | `	 * The frame table is head-pushed (SyHashInsert), so its forward order is` |
|        - |  331 | `	 * reverse-insertion; walk it backward to match php, which returns locals in` |
|        - |  332 | `	 * the order they first appeared (a,b,c — a reassignment reuses the slot and` |
|        - |  333 | `	 * keeps its original position). */` |
|       74 |  334 | `	SyHashForEachReverse(&pFrame->hVar,VmHashVarWalker,pArray);` |
|        - |  335 | `	/* Finally,return the created array */` |
|       74 |  336 | `	ph7_result_value(pCtx,pArray);` |
|       74 |  337 | `	return SXRET_OK;` |
|       44 |  338 | `}` |
|        - |  339 | `/*` |
|        - |  340 | ` * string get_debug_type(mixed $value)` |
|        - |  341 | ` *  php 8.0's type name for diagnostics: the SHORT scalar names, and a class` |
|        - |  342 | ` *  name for an object. Distinct from gettype(), which keeps php 4's long` |
|        - |  343 | ` *  spellings ("integer"/"boolean"/"NULL") for compatibility.` |
|        - |  344 | ` *` |
|        - |  345 | ` *  This was a prelude function in the Reflection chunk, where the TypeError` |
|        - |  346 | ` *  messages needed it; it is what php ships natively, and the SPL chunk's` |
|        - |  347 | ` *  messages want it too.` |
|        - |  348 | ` */` |
|      208 |  349 | `PH7_PRIVATE int vm_builtin_get_debug_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  350 | `{` |
|      213 |  351 | `	const char *zType = "null";` |
|      213 |  352 | `	if( nArg > 0 ){` |
|      213 |  353 | `		ph7_value *pVal = apArg[0];` |
|      213 |  354 | `		if( pVal->iFlags & MEMOBJ_OBJ ){` |
|       87 |  355 | `			ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|        - |  356 | `			/* php prints this one with the class name it SHOWS: an anonymous class` |
|        - |  357 | ``			 * answers `Base@anonymous`, not the whole synthesized name. */`` |
|      128 |  358 | `			ph7_result_string(pCtx,SyStringData(&pThis->pClass->sDisp),` |
|       82 |  359 | `				(int)SyStringLength(&pThis->pClass->sDisp));` |
|       87 |  360 | `			return SXRET_OK;` |
|        - |  361 | `		}` |
|      130 |  362 | `		if( pVal->iFlags & MEMOBJ_NULL ){` |
|        8 |  363 | `			zType = "null";` |
|      127 |  364 | `		}else if( pVal->iFlags & MEMOBJ_REAL ){` |
|        - |  365 | `			/* REAL wins over a cached MEMOBJ_INT, as it does for gettype() */` |
|       16 |  366 | `			zType = "float";` |
|      117 |  367 | `		}else if( pVal->iFlags & MEMOBJ_INT ){` |
|       31 |  368 | `			zType = "int";` |
|       96 |  369 | `		}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|       17 |  370 | `			zType = "string";` |
|       75 |  371 | `		}else if( pVal->iFlags & MEMOBJ_BOOL ){` |
|       32 |  372 | `			zType = "bool";` |
|       53 |  373 | `		}else if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|       13 |  374 | `			zType = "array";` |
|       32 |  375 | `		}else if( pVal->iFlags & MEMOBJ_RES ){` |
|        - |  376 | `			/* php names the resource's TYPE here, and this used to answer the` |
|        - |  377 | `			 * bare "resource" gettype() answers — so the function whose whole` |
|        - |  378 | `			 * job is to NAME a value's type had a different answer from php for` |
|        - |  379 | `			 * every open handle, in exactly the diagnostics it exists for.` |
|        - |  380 | `			 * (The note this replaced said the kind was unavailable; it is what` |
|        - |  381 | `			 * get_resource_type() has been answering all along.) */` |
|       27 |  382 | `			if( PH7_VfsResourceIsClosed(pVal->x.pOther) ){` |
|        5 |  383 | `				zType = "resource (closed)";` |
|        3 |  384 | `			}else{` |
|       33 |  385 | `				ph7_result_string_format(pCtx,"resource (%s)",` |
|       10 |  386 | `					PH7_VfsResourceType(pVal->x.pOther));` |
|       23 |  387 | `				return SXRET_OK;` |
|        - |  388 | `			}` |
|        2 |  389 | `		}` |
|       53 |  390 | `	}` |
|      109 |  391 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|      109 |  392 | `	return SXRET_OK;` |
|      109 |  393 | `}` |
|        - |  394 | `/*` |
|        - |  395 | ` * bool gettype($var)` |
|        - |  396 | ` *  Get the type of a variable` |
|        - |  397 | ` * Parameters` |
|        - |  398 | ` *   $var` |
|        - |  399 | ` *    The variable being type checked.` |
|        - |  400 | ` * Return` |
|        - |  401 | ` *   String representation of the given variable type.` |
|        - |  402 | ` */` |
|      608 |  403 | `PH7_PRIVATE int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  404 | `{` |
|        - |  405 | `	/* php's gettype() uses LONG type names ("integer"/"boolean"/"NULL"), distinct` |
|        - |  406 | `	 * from get_debug_type()'s short ones ("int"/"bool"/"null") — never route this` |
|        - |  407 | `	 * through PH7_MemObjTypeDump, which yields the short forms. */` |
|      613 |  408 | `	const char *zType = "unknown type";` |
|      613 |  409 | `	if( nArg > 0 ){` |
|      613 |  410 | `		ph7_value *pVal = apArg[0];` |
|      613 |  411 | `		if( pVal->iFlags & MEMOBJ_NULL ){` |
|       15 |  412 | `			zType = "NULL";` |
|      606 |  413 | `		}else if( pVal->iFlags & MEMOBJ_REAL ){` |
|        - |  414 | `			/* REAL wins over a cached MEMOBJ_INT: 1.0 is "double" (php). */` |
|       92 |  415 | `			zType = "double";` |
|      554 |  416 | `		}else if( pVal->iFlags & MEMOBJ_INT ){` |
|      207 |  417 | `			zType = "integer";` |
|      404 |  418 | `		}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|      193 |  419 | `			zType = "string";` |
|      208 |  420 | `		}else if( pVal->iFlags & MEMOBJ_BOOL ){` |
|       26 |  421 | `			zType = "boolean";` |
|      100 |  422 | `		}else if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|       46 |  423 | `			zType = "array";` |
|       66 |  424 | `		}else if( pVal->iFlags & MEMOBJ_OBJ ){` |
|       24 |  425 | `			zType = "object";` |
|       32 |  426 | `		}else if( pVal->iFlags & MEMOBJ_RES ){` |
|        - |  427 | `			/* php reports an fclose()'d handle as "resource (closed)" */` |
|       21 |  428 | `			zType = PH7_VfsResourceIsClosed(pVal->x.pOther) ? "resource (closed)" : "resource";` |
|        9 |  429 | `		}` |
|      301 |  430 | `	}` |
|        - |  431 | `	/* Return the variable type */` |
|      613 |  432 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|      613 |  433 | `	return SXRET_OK;` |
|        5 |  434 | `}` |
|        - |  435 | `/*` |
|        - |  436 | ` * bool settype(mixed &$var, string $type)` |
|        - |  437 | ` *  Convert $var IN PLACE to the type named by $type, mirroring the (type) cast` |
|        - |  438 | ` *  operators exactly (same PH7_MemObjTo* helpers CVT_INT/REAL/STR/BOOL/ARRAY/OBJ` |
|        - |  439 | ` *  use), and write the result back through the by-ref out-param.` |
|        - |  440 | ` * Parameters` |
|        - |  441 | ` *   &$var : the variable to convert (compile.c's GenStateByRefBuiltinMask marks` |
|        - |  442 | ` *           position 0 by-reference so an undefined variable is auto-vivified).` |
|        - |  443 | ` *   $type : "int"/"integer", "float"/"double", "string", "bool"/"boolean",` |
|        - |  444 | ` *           "array", "object", "null" (case-insensitive).` |
|        - |  445 | ` * Return` |
|        - |  446 | ` *   Always true on success; a non-referenceable $var is an Error, an unknown` |
|        - |  447 | ` *   $type (or the un-castable "resource") is a ValueError — php-exact.` |
|        - |  448 | ` */` |
|       84 |  449 | `PH7_PRIVATE int vm_builtin_settype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        4 |  450 | `{` |
|        - |  451 | `	const char *zType;` |
|        - |  452 | `	int nLen;` |
|        - |  453 | `	ph7_value *pNew;` |
|       31 |  454 | `	SXUNUSED(nArg); /* arity (exactly 2) enforced by the central arity table */` |
|        - |  455 | `	/* php binds $var by reference at the CALL, and the refusal is the call site's` |
|        - |  456 | `	 * to raise (PH7_VmScreenByRefArgShapes) — only the compiler can tell a literal,` |
|        - |  457 | `	 * which php refuses, from the result of a call, which php accepts with a notice` |
|        - |  458 | ``	 * and converts in place on the temporary. The `nIdx == SXU32_HIGH` test that`` |
|        - |  459 | `	 * used to sit here conflated the two. */` |
|       88 |  460 | `	zType = ph7_value_to_string(apArg[1],&nLen);` |
|        - |  461 | `	/* Validate the target type up-front (php checks $type before touching $var):` |
|        - |  462 | `	 * "resource" is a distinct ValueError, any other unknown name is the generic` |
|        - |  463 | `	 * invalid-type ValueError. */` |
|       88 |  464 | `	if( nLen == 8 && SyStrnicmp(zType,"resource",8) == 0 ){` |
|        3 |  465 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|        - |  466 | `			"Cannot convert to resource type");` |
|        - |  467 | `	}` |
|       87 |  468 | `	if( !(  (nLen == 3 && SyStrnicmp(zType,"int",3) == 0)` |
|       62 |  469 | `	     \|\| (nLen == 7 && SyStrnicmp(zType,"integer",7) == 0)` |
|       55 |  470 | `	     \|\| (nLen == 5 && SyStrnicmp(zType,"float",5) == 0)` |
|       54 |  471 | `	     \|\| (nLen == 6 && SyStrnicmp(zType,"double",6) == 0)` |
|       53 |  472 | `	     \|\| (nLen == 6 && SyStrnicmp(zType,"string",6) == 0)` |
|       37 |  473 | `	     \|\| (nLen == 4 && SyStrnicmp(zType,"bool",4) == 0)` |
|       11 |  474 | `	     \|\| (nLen == 7 && SyStrnicmp(zType,"boolean",7) == 0)` |
|       10 |  475 | `	     \|\| (nLen == 5 && SyStrnicmp(zType,"array",5) == 0)` |
|        8 |  476 | `	     \|\| (nLen == 6 && SyStrnicmp(zType,"object",6) == 0)` |
|        3 |  477 | `	     \|\| (nLen == 4 && SyStrnicmp(zType,"null",4) == 0) ) ){` |
|        3 |  478 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|        - |  479 | `			"settype(): Argument #2 ($type) must be a valid type");` |
|        - |  480 | `	}` |
|        - |  481 | `	/* Convert a COPY of the current value (the To* helpers mutate in place), then` |
|        - |  482 | `	 * store it through the by-ref out-param — settype changes $var's TYPE, so the` |
|        - |  483 | `	 * new value must land in the caller slot (nIdx), not just in shared contents. */` |
|       94 |  484 | `	pNew = ph7_context_new_scalar(pCtx);` |
|       94 |  485 | `	if( pNew == 0 ){` |
|      ! 0 |  486 | `		return PH7_ContextMemoryError(pCtx);` |
|        - |  487 | `	}` |
|       94 |  488 | `	PH7_MemObjStore(apArg[0],pNew);` |
|       90 |  489 | `	if( (nLen == 3 && SyStrnicmp(zType,"int",3) == 0)` |
|       74 |  490 | `	 \|\| (nLen == 7 && SyStrnicmp(zType,"integer",7) == 0) ){` |
|        - |  491 | `		/* settype() IS the cast operator, warning included. */` |
|       56 |  492 | `		PH7_MemObjWarnIntCast(pNew);` |
|       56 |  493 | `		PH7_MemObjToInteger(pNew);` |
|       56 |  494 | `		MemObjSetType(pNew,MEMOBJ_INT);` |
|       80 |  495 | `	}else if( (nLen == 5 && SyStrnicmp(zType,"float",5) == 0)` |
|       53 |  496 | `	       \|\| (nLen == 6 && SyStrnicmp(zType,"double",6) == 0) ){` |
|        5 |  497 | `		PH7_MemObjToReal(pNew);` |
|        5 |  498 | `		MemObjSetType(pNew,MEMOBJ_REAL);` |
|       61 |  499 | `	}else if( nLen == 6 && SyStrnicmp(zType,"string",6) == 0 ){` |
|        - |  500 | `		/* php emits "Array to string conversion" for settype($arr,'string'), and` |
|        - |  501 | `		 * throws "Object of class X could not be converted to string" for an` |
|        - |  502 | `		 * object with no __toString() (or propagates one that threw). php's` |
|        - |  503 | `		 * convert_to_string() has already blanked the zval by then, so a CAUGHT` |
|        - |  504 | `		 * settype() leaves $var === "" — store that, then propagate instead of` |
|        - |  505 | `		 * answering true. */` |
|       36 |  506 | `		sxi32 rcSv = PH7_MemObjToStringUV(pNew);` |
|       36 |  507 | `		if( rcSv != SXRET_OK ){` |
|       16 |  508 | `			PH7_MemObjRelease(pNew);` |
|       16 |  509 | `			MemObjSetType(pNew,MEMOBJ_STRING);` |
|       16 |  510 | `			PH7_VmStoreArgByRef(pCtx->pVm,apArg[0],pNew);` |
|       16 |  511 | `			pCtx->nThrowRc = rcSv;` |
|       16 |  512 | `			return rcSv;` |
|        - |  513 | `		}` |
|       27 |  514 | `	}else if( (nLen == 4 && SyStrnicmp(zType,"bool",4) == 0)` |
|       14 |  515 | `	       \|\| (nLen == 7 && SyStrnicmp(zType,"boolean",7) == 0) ){` |
|        8 |  516 | `		PH7_MemObjToBool(pNew);` |
|       12 |  517 | `	}else if( nLen == 5 && SyStrnicmp(zType,"array",5) == 0 ){` |
|        5 |  518 | `		PH7_MemObjToHashmap(pNew);` |
|        7 |  519 | `	}else if( nLen == 6 && SyStrnicmp(zType,"object",6) == 0 ){` |
|        3 |  520 | `		PH7_MemObjToObject(pNew);` |
|        2 |  521 | `	}else{` |
|        - |  522 | `		/* "null" — the only validated name left */` |
|        3 |  523 | `		PH7_MemObjToNull(pNew);` |
|        - |  524 | `	}` |
|       92 |  525 | `	PH7_VmStoreArgByRef(pCtx->pVm,apArg[0],pNew);` |
|       92 |  526 | `	ph7_result_bool(pCtx,1);` |
|       92 |  527 | `	return SXRET_OK;` |
|       57 |  528 | `}` |
|        - |  529 | `/*` |
|        - |  530 | ` * string get_resource_type(resource $handle)` |
|        - |  531 | ` *  This function gets the type of the given resource.` |
|        - |  532 | ` * Parameters` |
|        - |  533 | ` *  $handle` |
|        - |  534 | ` *  The evaluated resource handle.` |
|        - |  535 | ` * Return` |
|        - |  536 | ` *  If the given handle is a resource, this function will return a string` |
|        - |  537 | ` *  representing its type. If the type is not identified by this function` |
|        - |  538 | ` *  the return value will be the string Unknown.` |
|        - |  539 | ` *  This function will return FALSE and generate an error if handle` |
|        - |  540 | ` *  is not a resource.` |
|        - |  541 | ` */` |
|       78 |  542 | `PH7_PRIVATE int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        4 |  543 | `{` |
|        - |  544 | `	const char *zType;` |
|       82 |  545 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - |  546 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 |  547 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  548 | `		return PH7_OK;` |
|        - |  549 | `	}` |
|        - |  550 | `	/* PHP returns the resource TYPE name (e.g. "stream" for IO handles), not an id. */` |
|       82 |  551 | `	zType = PH7_VfsResourceType(apArg[0]->x.pOther);` |
|       82 |  552 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       82 |  553 | `	return SXRET_OK;` |
|       43 |  554 | `}` |
|        - |  555 | `/*` |
|        - |  556 | ` * int get_resource_id(resource $resource)` |
|        - |  557 | ` *  Returns the integer id of a resource — the same number (int) casts to and` |
|        - |  558 | ` *  "Resource id #N" renders (php 8.0).` |
|        - |  559 | ` */` |
|        4 |  560 | `PH7_PRIVATE int vm_builtin_get_resource_id(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  561 | `{` |
|        - |  562 | `	char zGiven[64];` |
|        6 |  563 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 |  564 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|        - |  565 | `			"get_resource_id(): Argument #1 ($resource) must be of type resource, %s given",` |
|      ! 0 |  566 | `			nArg < 1 ? "none" : VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));` |
|        - |  567 | `	}` |
|        6 |  568 | `	ph7_result_int64(pCtx,(ph7_int64)PH7_VmResourceId(pCtx->pVm,apArg[0]->x.pOther));` |
|        6 |  569 | `	return SXRET_OK;` |
|        4 |  570 | `}` |
|        - |  571 | `/*` |
|        - |  572 | ` * void var_dump(expression,....)` |
|        - |  573 | ` *   var_dump � Dumps information about a variable` |
|        - |  574 | ` * Parameters` |
|        - |  575 | ` *   One or more expression to dump.` |
|        - |  576 | ` * Returns` |
|        - |  577 | ` *  Nothing.` |
|        - |  578 | ` */` |
|    12031 |  579 | `PH7_PRIVATE int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  580 | `{` |
|        - |  581 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - |  582 | `	int i;` |
|    12036 |  583 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - |  584 | `	/* Dump one or more expressions */` |
|    29578 |  585 | `	for( i = 0 ; i < nArg ; i++ ){` |
|    17547 |  586 | `		ph7_value *pObj = apArg[i];` |
|        - |  587 | `		/* Reset the working buffer */` |
|    17547 |  588 | `		SyBlobReset(&sDump);` |
|        - |  589 | `		/* Dump the given expression */` |
|    17547 |  590 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - |  591 | `		/* Output */` |
|    17547 |  592 | `		if( SyBlobLength(&sDump) > 0 ){` |
|    17547 |  593 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|     8750 |  594 | `		}` |
|     8755 |  595 | `	}` |
|        - |  596 | `	/* Release the working buffer */` |
|    12036 |  597 | `	SyBlobRelease(&sDump);` |
|    12036 |  598 | `	return SXRET_OK;` |
|        5 |  599 | `}` |
|        - |  600 | `/*` |
|        - |  601 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - |  602 | ` *   print-r - Prints human-readable information about a variable` |
|        - |  603 | ` * Parameters` |
|        - |  604 | ` *   expression: Expression to dump` |
|        - |  605 | ` *   return : If you would like to capture the output of print_r() use` |
|        - |  606 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - |  607 | ` *            print_r() will return the information rather than print it.` |
|        - |  608 | ` * Return` |
|        - |  609 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - |  610 | ` *  Otherwise, the return value is TRUE.` |
|        - |  611 | ` */` |
|      338 |  612 | `PH7_PRIVATE int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  613 | `{` |
|      343 |  614 | `	int ret_string = 0;` |
|        - |  615 | `	SyBlob sDump;` |
|      343 |  616 | `	if( nArg < 1 ){` |
|        - |  617 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  618 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  619 | `		return SXRET_OK;` |
|        - |  620 | `	}` |
|      343 |  621 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|      343 |  622 | `	if ( nArg > 1 ){` |
|        - |  623 | `		/* Where to redirect output */` |
|       27 |  624 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|       12 |  625 | `	}` |
|        - |  626 | `	/* Generate dump */` |
|      343 |  627 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|      343 |  628 | `	if( !ret_string ){` |
|        - |  629 | `		/* Output dump */` |
|      319 |  630 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  631 | `		/* Return true */` |
|      319 |  632 | `		ph7_result_bool(pCtx,1);` |
|      162 |  633 | `	}else{` |
|        - |  634 | `		/* Generated dump as return value */` |
|       27 |  635 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  636 | `	}` |
|        - |  637 | `	/* Release the working buffer */` |
|      343 |  638 | `	SyBlobRelease(&sDump);` |
|      343 |  639 | `	return SXRET_OK;` |
|      174 |  640 | `}` |
|        - |  641 | `/*` |
|        - |  642 | ` * var_export() — PHP-exact evaluable representation of a value.` |
|        - |  643 | ` *` |
|        - |  644 | ` * Distinct from print_r/var_dump (which share PH7_MemObjDump): var_export emits` |
|        - |  645 | ` * parseable PHP — 'single-quoted' strings, true/false/NULL, array (\n  k => v,\n),` |
|        - |  646 | ` * and \Class::__set_state(array(...)) for objects. PHP's indentation is quirky` |
|        - |  647 | ` * (2-space array entries; 3-space object property lines; a composite value always` |
|        - |  648 | ` * opens at containerIndent+2) but deterministic; this matches it byte-for-byte.` |
|        - |  649 | ` */` |
|        - |  650 | `/* The recursion marks (HASHMAP_DUMPING / VM_INSTANCE_DUMPING, ph7int.h) are php's` |
|        - |  651 | ` * one GC_PROTECT_RECURSION bit, shared with var_dump and print_r. */` |
|        - |  652 | `typedef struct VmExportCtx VmExportCtx;` |
|        - |  653 | `struct VmExportCtx` |
|        - |  654 | `{` |
|        - |  655 | `	SyBlob *pOut;` |
|        - |  656 | `	int nIndent;  /* indentation of the container emitting this entry */` |
|        - |  657 | `	int depth;    /* recursion guard */` |
|        - |  658 | `	int bPresented; /* the entries are a native class's PRESENTED shape, which` |
|        - |  659 | `	                 * carries the object's own table MANGLED. var_export prints a` |
|        - |  660 | `	                 * non-public property under its PLAIN name (the attribute loop` |
|        - |  661 | `	                 * below already does), so the key is unmangled here too. */` |
|        - |  662 | `	int nKeyExtra; /* extra columns the KEY line carries beyond nIndent+2, and the` |
|        - |  663 | `	                * value's container indent does NOT. An object body's keys sit` |
|        - |  664 | `	                * one deeper than an array's while a nested array under one of` |
|        - |  665 | `	                * them sits at the OBJECT's indent -- which is why a` |
|        - |  666 | ``	                * SimpleXMLElement's `@attributes` row prints its array two`` |
|        - |  667 | `	                * columns in and not three. 0 for a plain array. */` |
|        - |  668 | `};` |
|        - |  669 | `static void VmExportValue(SyBlob *pOut, ph7_value *pVal, int nIndent, int depth);` |
|        - |  670 | `/* Append nIndent spaces. */` |
|    10187 |  671 | `static void VmExportIndent(SyBlob *pOut, int nIndent)` |
|        5 |  672 | `{` |
|        - |  673 | `	int i;` |
|    28606 |  674 | `	for( i = 0; i < nIndent; i++ ){ SyBlobAppend(pOut," ",1); }` |
|    10192 |  675 | `}` |
|        - |  676 | `/* Append a single-quoted PHP string literal (escapes ' and \, batching safe runs` |
|        - |  677 | ` * in one append). A NUL byte can't live in a single-quoted literal, so PHP splits` |
|        - |  678 | ` * it out as ' . "\0" . ' — match that. */` |
|    15793 |  679 | `static void VmExportQuoted(SyBlob *pOut, const char *z, int n)` |
|        5 |  680 | `{` |
|    15798 |  681 | `	int i, run = 0;` |
|    15798 |  682 | `	SyBlobAppend(pOut,"'",1);` |
|   173953 |  683 | `	for( i = 0; i < n; i++ ){` |
|   158160 |  684 | `		char c = z[i];` |
|   158160 |  685 | `		if( c != '\0' && c != '\'' && c != '\\' ){ continue; } /* extend the safe run */` |
|      664 |  686 | `		if( i > run ){ SyBlobAppend(pOut,&z[run],(sxu32)(i-run)); }` |
|      664 |  687 | `		if( c == '\0' ){ SyBlobAppend(pOut,"' . \"\\0\" . '",12); }` |
|      567 |  688 | `		else { SyBlobAppend(pOut,"\\",1); SyBlobAppend(pOut,&z[i],1); }` |
|      664 |  689 | `		run = i+1;` |
|      334 |  690 | `	}` |
|    15798 |  691 | `	if( n > run ){ SyBlobAppend(pOut,&z[run],(sxu32)(n-run)); }` |
|    15798 |  692 | `	SyBlobAppend(pOut,"'",1);` |
|    15798 |  693 | `}` |
|        - |  694 | `/*` |
|        - |  695 | ` * A container that is its own ancestor. php cannot write an evaluable expression` |
|        - |  696 | ` * for one, so it warns -- once per cycle it meets, at E_WARNING and with no` |
|        - |  697 | ` * function prefix -- and emits NULL in its place, in BOTH the printing and the` |
|        - |  698 | `` * returning form. PHL emitted the NULL and stayed silent, so a `var_export()` of`` |
|        - |  699 | ` * a cyclic structure looked like a faithful export of a structure that had a real` |
|        - |  700 | ` * NULL in it.` |
|        - |  701 | ` */` |
|       10 |  702 | `static void VmExportCycleNull(SyBlob *pOut, ph7_value *pVal)` |
|        2 |  703 | `{` |
|       12 |  704 | `	if( pVal->pVm ){` |
|       12 |  705 | `		PH7_VmThrowError(pVal->pVm,0,PH7_CTX_WARNING,` |
|        - |  706 | `			"var_export does not handle circular references");` |
|        5 |  707 | `	}` |
|       12 |  708 | `	SyBlobAppend(pOut,"NULL",4);` |
|       12 |  709 | `}` |
|        - |  710 | `/* Emit " => " then the value: a scalar inline, a composite on its own line at` |
|        - |  711 | ` * containerIndent+2. A circular reference renders inline as NULL (like PHP). */` |
|     6330 |  712 | `static void VmExportEntryValue(SyBlob *pOut, ph7_value *pVal, int nContainerIndent, int depth)` |
|        5 |  713 | `{` |
|     6335 |  714 | `	SyBlobAppend(pOut," => ",4);` |
|     6335 |  715 | `	if( PH7_MemObjDumpIsRecursive(pVal) ){` |
|       12 |  716 | `		VmExportCycleNull(pOut,pVal);` |
|     6330 |  717 | `	}else if( ph7_value_is_array(pVal) \|\| ph7_value_is_object(pVal) ){` |
|      688 |  718 | `		SyBlobAppend(pOut,"\n",1);` |
|      688 |  719 | `		VmExportIndent(pOut,nContainerIndent+2);` |
|      688 |  720 | `		VmExportValue(pOut,pVal,nContainerIndent+2,depth+1);` |
|      345 |  721 | `	}else{` |
|     5642 |  722 | `		VmExportValue(pOut,pVal,nContainerIndent+2,depth+1);` |
|        - |  723 | `	}` |
|     6335 |  724 | `	SyBlobAppend(pOut,",\n",2);` |
|     6335 |  725 | `}` |
|        - |  726 | `/* Array walker: "<indent+2>key => value,\n" for each entry. */` |
|     6222 |  727 | `static int VmExportArrayWalk(ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|        5 |  728 | `{` |
|     6227 |  729 | `	VmExportCtx *pC = (VmExportCtx *)pUserData;` |
|     6227 |  730 | `	VmExportIndent(pC->pOut,pC->nIndent+2+pC->nKeyExtra);` |
|     6227 |  731 | `	if( ph7_value_is_string(pKey) ){` |
|        - |  732 | `		int n;` |
|     1438 |  733 | `		const char *z = ph7_value_to_string(pKey,&n);` |
|     1446 |  734 | `		if( pC->bPresented && n > 0 && z[0] == 0 ){` |
|        - |  735 | `			SyString sUnmCls, sUnmName;` |
|       17 |  736 | `			SyStringInitFromBuf(&sUnmName,z,n);` |
|       17 |  737 | `			PH7_UnmangleAttrName(z,(sxu32)n,&sUnmCls,&sUnmName);` |
|       17 |  738 | `			VmExportQuoted(pC->pOut,sUnmName.zString,(int)sUnmName.nByte);` |
|        9 |  739 | `		}else{` |
|     1422 |  740 | `			VmExportQuoted(pC->pOut,z,n);` |
|        - |  741 | `		}` |
|      721 |  742 | `	}else{` |
|     4794 |  743 | `		SyBlobFormat(pC->pOut,"%qd",ph7_value_to_int64(pKey));` |
|        - |  744 | `	}` |
|     6227 |  745 | `	VmExportEntryValue(pC->pOut,pValue,pC->nIndent,pC->depth);` |
|     6227 |  746 | `	return PH7_OK;` |
|        5 |  747 | `}` |
|    39130 |  748 | `static void VmExportValue(SyBlob *pOut, ph7_value *pVal, int nIndent, int depth)` |
|        5 |  749 | `{` |
|    39135 |  750 | `	if( depth > PH7_DUMP_MAX_DEPTH ){ return; } /* backstop for pathological finite nesting */` |
|    39135 |  751 | `	if( ph7_value_is_null(pVal) ){` |
|     1729 |  752 | `		SyBlobAppend(pOut,"NULL",4);` |
|    38273 |  753 | `	}else if( ph7_value_is_bool(pVal) ){` |
|     8333 |  754 | `		if( ph7_value_to_bool(pVal) ){ SyBlobAppend(pOut,"true",4); }` |
|     4385 |  755 | `		else { SyBlobAppend(pOut,"false",5); }` |
|    33188 |  756 | `	}else if( ph7_value_is_float(pVal) ){` |
|        - |  757 | `		/* float before int: ph7_value_is_int is lenient (true for integer reals). */` |
|     1527 |  758 | `		sxu32 before = SyBlobLength(pOut), i, after;` |
|        - |  759 | `		const char *z;` |
|     1527 |  760 | `		int plain = 1;` |
|     1527 |  761 | `		PH7_AppendShortestReal(pOut,ph7_value_to_double(pVal));` |
|     1527 |  762 | `		z = (const char *)SyBlobData(pOut);` |
|     1527 |  763 | `		after = SyBlobLength(pOut);` |
|     3485 |  764 | `		for( i = before; i < after; i++ ){` |
|     2975 |  765 | `			if( !((z[i]>='0'&&z[i]<='9')\|\|z[i]=='-') ){ plain = 0; break; }` |
|      984 |  766 | `		}` |
|     1527 |  767 | `		if( plain ){ SyBlobAppend(pOut,".0",2); } /* integer-form floats render 1.0/100.0 */` |
|    28322 |  768 | `	}else if( ph7_value_is_int(pVal) ){` |
|    10141 |  769 | `		sxi64 iVal = ph7_value_to_int64(pVal);` |
|    10141 |  770 | `		if( iVal == SMALLEST_INT64 ){` |
|        - |  771 | `			/* php renders LONG_MIN as an expression: the positive literal` |
|        - |  772 | `			 * 9223372036854775808 would not be representable when eval'd. */` |
|       19 |  773 | `			SyBlobAppend(pOut,"-9223372036854775807-1",sizeof("-9223372036854775807-1")-1);` |
|       10 |  774 | `		}else{` |
|    10123 |  775 | `			SyBlobFormat(pOut,"%qd",iVal);` |
|        5 |  776 | `		}` |
|    22481 |  777 | `	}else if( ph7_value_is_string(pVal) ){` |
|        - |  778 | `		int n;` |
|    14249 |  779 | `		const char *z = ph7_value_to_string(pVal,&n);` |
|    14249 |  780 | `		VmExportQuoted(pOut,z,n);` |
|    10205 |  781 | `	}else if( ph7_value_is_array(pVal) ){` |
|     2993 |  782 | `		ph7_hashmap *pMap = (ph7_hashmap *)pVal->x.pOther;` |
|     2993 |  783 | `		if( pMap->iFlags & HASHMAP_DUMPING ){` |
|      ! 0 |  784 | `			VmExportCycleNull(pOut,pVal);` |
|      ! 0 |  785 | `		}else{` |
|        - |  786 | `			VmExportCtx ctx;` |
|     2993 |  787 | `			ctx.bPresented = 0;` |
|     2993 |  788 | `			ctx.nKeyExtra = 0;` |
|     2993 |  789 | `			pMap->iFlags \|= HASHMAP_DUMPING;` |
|     2993 |  790 | `			SyBlobAppend(pOut,"array (\n",8);` |
|     2993 |  791 | `			ctx.pOut = pOut; ctx.nIndent = nIndent; ctx.depth = depth;` |
|     2993 |  792 | `			ph7_array_walk(pVal,VmExportArrayWalk,&ctx);` |
|     2993 |  793 | `			VmExportIndent(pOut,nIndent);` |
|     2993 |  794 | `			SyBlobAppend(pOut,")",1);` |
|     2993 |  795 | `			pMap->iFlags &= ~HASHMAP_DUMPING;` |
|        5 |  796 | `		}` |
|     1666 |  797 | `	}else if( ph7_value_is_object(pVal) ){` |
|      193 |  798 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|      193 |  799 | `		if( pThis->pClass->iFlags & PH7_CLASS_ENUM ){` |
|        - |  800 | ``			/* php 8.1: an enum case exports as `\S::A` */`` |
|       11 |  801 | `			ph7_value *pName = PH7_EnumCaseNameValue(pThis);` |
|       11 |  802 | `			SyBlobAppend(pOut,"\\",1);` |
|       11 |  803 | `			SyBlobAppend(pOut,pThis->pClass->sName.zString,pThis->pClass->sName.nByte);` |
|       11 |  804 | `			SyBlobAppend(pOut,"::",2);` |
|       11 |  805 | `			if( pName && SyBlobLength(&pName->sBlob) > 0 ){` |
|       11 |  806 | `				SyBlobAppend(pOut,SyBlobData(&pName->sBlob),SyBlobLength(&pName->sBlob));` |
|        6 |  807 | `			}` |
|      188 |  808 | `		}else if( pThis->iFlags & VM_INSTANCE_DUMPING ){` |
|      ! 0 |  809 | `			VmExportCycleNull(pOut,pVal);` |
|      ! 0 |  810 | `		}else{` |
|      183 |  811 | `			SyString *pClassName = &pThis->pClass->sName;` |
|        - |  812 | `			SyHashEntry *pEntry;` |
|        - |  813 | `			SySet sNames;` |
|        - |  814 | `			SyString *aName;` |
|        - |  815 | `			sxu32 iName,nName;` |
|        - |  816 | ``			/* php exports a plain stdClass as a CAST — `(object) array(...)` — and`` |
|        - |  817 | `			 * every other class through __set_state(); a SUBCLASS of stdClass takes` |
|        - |  818 | `			 * the __set_state form, so this is the exact class and not an` |
|        - |  819 | `			 * inheritance test. The two forms differ by one closing paren. */` |
|      183 |  820 | `			int bStdObj = pThis->pClass == pThis->pVm->pStdClass;` |
|      183 |  821 | `			pThis->iFlags \|= VM_INSTANCE_DUMPING;` |
|      183 |  822 | `			if( bStdObj ){` |
|        3 |  823 | `				SyBlobAppend(pOut,"(object) array(\n",sizeof("(object) array(\n")-1);` |
|        2 |  824 | `			}else{` |
|      181 |  825 | `				SyBlobAppend(pOut,"\\",1);` |
|      181 |  826 | `				SyBlobAppend(pOut,pClassName->zString,pClassName->nByte);` |
|      181 |  827 | `				SyBlobAppend(pOut,"::__set_state(array(\n",21);` |
|        - |  828 | `			}` |
|        - |  829 | `			{` |
|        - |  830 | `				/* A native class's PRESENTATION (php's get_properties): var_export` |
|        - |  831 | `				 * shows a DateTime as date/timezone_type/timezone, the same shape` |
|        - |  832 | `				 * the (array) cast produces and NOT the hidden engine slots. A` |
|        - |  833 | `				 * debug-only hook (WeakReference) fills nothing, which is php's` |
|        - |  834 | `				 * empty export. */` |
|        - |  835 | `				ph7_value sPresent;` |
|      183 |  836 | `				ph7_hashmap *pPresent = PH7_NewHashmap(pThis->pVm,0,0);` |
|      183 |  837 | `				PH7_MemObjInit(pThis->pVm,&sPresent);` |
|      183 |  838 | `				if( pPresent ){` |
|      183 |  839 | `					sPresent.x.pOther = pPresent;` |
|      183 |  840 | `					MemObjSetType(&sPresent,MEMOBJ_HASHMAP);` |
|      183 |  841 | `					if( PH7_ClassInstancePresent(pThis,&sPresent,0) ){` |
|        - |  842 | `						/* Same line shape the attribute loop below produces: an` |
|        - |  843 | `						 * object body's entries sit one deeper than an array's. */` |
|        - |  844 | `						VmExportCtx sCtx;` |
|      105 |  845 | `						sCtx.pOut = pOut;` |
|      105 |  846 | `						sCtx.nIndent = nIndent;` |
|      105 |  847 | `						sCtx.depth = depth;` |
|      105 |  848 | `						sCtx.bPresented = 1;` |
|      105 |  849 | `						sCtx.nKeyExtra = 1;` |
|      105 |  850 | `						ph7_array_walk(&sPresent,VmExportArrayWalk,&sCtx);` |
|      105 |  851 | `						PH7_MemObjRelease(&sPresent);` |
|      105 |  852 | `						VmExportIndent(pOut,nIndent);` |
|      105 |  853 | `						SyBlobAppend(pOut,bStdObj ? ")" : "))",bStdObj ? 1 : 2);` |
|      105 |  854 | `						pThis->iFlags &= ~VM_INSTANCE_DUMPING;` |
|      105 |  855 | `						return;` |
|        - |  856 | `					}` |
|       81 |  857 | `					PH7_MemObjRelease(&sPresent);` |
|       38 |  858 | `				}` |
|        - |  859 | `			}` |
|        - |  860 | `			/* SNAPSHOT the attribute names first: a PHP 8.4 get hook dispatched` |
|        - |  861 | `			 * mid-walk may re-enter an hAttr walk on this instance (the hash has` |
|        - |  862 | `			 * a single embedded loop cursor) or unset()/create properties; names` |
|        - |  863 | `			 * point into class-owned attr storage and each is re-looked-up. */` |
|       81 |  864 | `			SySetInit(&sNames,&pThis->pVm->sAllocator,sizeof(SyString));` |
|       81 |  865 | `			SyHashResetLoopCursor(&pThis->hAttr);` |
|      273 |  866 | `			while( (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|      197 |  867 | `				VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|      197 |  868 | `				if( PH7_ATTR_UNPRESENTED(pVmAttr) ){ continue; }` |
|      141 |  869 | `				if( PH7_ClassAttrUninitializedForRead(pVmAttr) ){` |
|       22 |  870 | `					continue; /* typed, never written: not there yet (php) */` |
|        - |  871 | `				}` |
|      116 |  872 | `				if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_VIRTUAL))` |
|       63 |  873 | `				 == PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|      ! 0 |  874 | `					continue; /* virtual set-only property: no value to export (php) */` |
|        - |  875 | `				}` |
|        - |  876 | `				{` |
|        - |  877 | `					/* The STORAGE key, which is php's MANGLED name for an inherited` |
|        - |  878 | `					 * private -- snapshotting the plain one re-looked-up the object's` |
|        - |  879 | `					 * OWN property of that name and exported its value twice. */` |
|        - |  880 | `					SyString sKey;` |
|      121 |  881 | `					SyStringInitFromBuf(&sKey,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|      121 |  882 | `					SySetPut(&sNames,(const void *)&sKey);` |
|        - |  883 | `				}` |
|        5 |  884 | `			}` |
|       81 |  885 | `			aName = (SyString *)SySetBasePtr(&sNames);` |
|       81 |  886 | `			nName = SySetUsed(&sNames);` |
|      197 |  887 | `			for( iName = 0 ; iName < nName ; ++iName ){` |
|      121 |  888 | `				SyString *pAName = &aName[iName];` |
|        - |  889 | `				VmClassAttr *pVmAttr;` |
|        - |  890 | `				ph7_value *pAttrVal;` |
|      121 |  891 | `				pEntry = PH7_ClassInstanceAttrEntry(pThis,pAName->zString,pAName->nByte);` |
|      121 |  892 | `				if( pEntry == 0 ){ continue; } /* unset by an earlier hook */` |
|      121 |  893 | `				pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|      121 |  894 | `				VmExportIndent(pOut,nIndent+3); /* object property lines sit one deeper than arrays */` |
|      123 |  895 | `				if( pAName->nByte > 0 && pAName->zString[0] == 0 ){` |
|        - |  896 | `					/* A MANGLED key -- an inherited private's storage name, or the` |
|        - |  897 | `					 * __PHP_Incomplete_Class carrier's raw payload key: php's` |
|        - |  898 | `					 * var_export prints the PLAIN name ('bp' => 1). */` |
|        - |  899 | `					SyString sUnmCls, sUnmName;` |
|        5 |  900 | `					SyStringInitFromBuf(&sUnmName,pAName->zString,pAName->nByte);` |
|        5 |  901 | `					PH7_UnmangleAttrName(pAName->zString,pAName->nByte,&sUnmCls,&sUnmName);` |
|        5 |  902 | `					VmExportQuoted(pOut,sUnmName.zString,(int)sUnmName.nByte);` |
|        3 |  903 | `				}else{` |
|      117 |  904 | `					VmExportQuoted(pOut,pAName->zString,(int)pAName->nByte);` |
|        - |  905 | `				}` |
|        - |  906 | `				/* PHP 8.4 property hooks: var_export() reads through the get hook` |
|        - |  907 | `				 * (every visibility — php exports private hooked values too). A` |
|        - |  908 | `				 * throwing hook parks on the boundary rail; the helper's boundary` |
|        - |  909 | `				 * gate keeps LATER hooks from running (the raw values the tail of` |
|        - |  910 | `				 * the export falls back to are discarded when the throw routes). */` |
|        - |  911 | `				{` |
|        - |  912 | `					ph7_value sHookVal;` |
|        - |  913 | `					sxi32 rcHk;` |
|      121 |  914 | `					PH7_MemObjInit(pThis->pVm,&sHookVal);` |
|      121 |  915 | `					rcHk = PH7_VmHookGetAttrValue(pThis,pVmAttr,&sHookVal);` |
|      121 |  916 | `					if( rcHk == SXRET_OK ){` |
|       16 |  917 | `						VmExportEntryValue(pOut,&sHookVal,nIndent,depth);` |
|       16 |  918 | `						PH7_MemObjRelease(&sHookVal);` |
|       20 |  919 | `						continue;` |
|        - |  920 | `					}` |
|      107 |  921 | `					PH7_MemObjRelease(&sHookVal);` |
|      107 |  922 | `					if( rcHk != SXERR_NOTFOUND ){` |
|        - |  923 | `						/* the hook threw (parked on the boundary rail): NULL` |
|        - |  924 | `						 * placeholder keeps the output well-formed */` |
|       10 |  925 | `						SyBlobAppend(pOut," => NULL,\n",10);` |
|       10 |  926 | `						continue;` |
|        - |  927 | `					}` |
|        - |  928 | `				}` |
|       99 |  929 | `				pAttrVal = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       99 |  930 | `				if( pAttrVal ){ VmExportEntryValue(pOut,pAttrVal,nIndent,depth); }` |
|      ! 0 |  931 | `				else { SyBlobAppend(pOut," => NULL,\n",10); }` |
|       52 |  932 | `			}` |
|       81 |  933 | `			SySetRelease(&sNames);` |
|       81 |  934 | `			VmExportIndent(pOut,nIndent);` |
|       81 |  935 | `			SyBlobAppend(pOut,bStdObj ? ")" : "))",bStdObj ? 1 : 2);` |
|       81 |  936 | `			pThis->iFlags &= ~VM_INSTANCE_DUMPING;` |
|        - |  937 | `		}` |
|       48 |  938 | `	}else{` |
|        - |  939 | `		/* resource / other -> PHP emits NULL (with a warning we omit) */` |
|      ! 0 |  940 | `		SyBlobAppend(pOut,"NULL",4);` |
|        - |  941 | `	}` |
|    19380 |  942 | `}` |
|        - |  943 | `/*` |
|        - |  944 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - |  945 | ` *  PHP-exact evaluable representation (see VmExportValue).` |
|        - |  946 | ` */` |
|    32810 |  947 | `PH7_PRIVATE int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  948 | `{` |
|    32815 |  949 | `	int ret_string = 0;` |
|        - |  950 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|    32815 |  951 | `	if( nArg < 1 ){` |
|        - |  952 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  953 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  954 | `		return SXRET_OK;` |
|        - |  955 | `	}` |
|    32815 |  956 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|    32815 |  957 | `	if ( nArg > 1 ){` |
|        - |  958 | `		/* Where to redirect output */` |
|    31521 |  959 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|    15623 |  960 | `	}` |
|        - |  961 | `	/* Generate the PHP-exact evaluable representation */` |
|    32815 |  962 | `	VmExportValue(&sDump,apArg[0],0,0);` |
|    32815 |  963 | `	if( PH7_CALLBACK_UNWOUND(pCtx->pVm->nBoundaryRc) ){` |
|        - |  964 | ``		/* A php 8.4 `get` hook threw (or exited) part-way through: php's var_export`` |
|        - |  965 | `		 * builds its string before it prints anything, so a throw from inside it` |
|        - |  966 | ``		 * reaches the caller with NOTHING written. The `$return = true` form was`` |
|        - |  967 | `		 * already right — the parked throw discards the RESULT — but the printing` |
|        - |  968 | `		 * form had already handed the half-built text to the output layer, so a` |
|        - |  969 | `		 * caught throw was followed by an export naming properties as NULL. */` |
|       10 |  970 | `		SyBlobRelease(&sDump);` |
|       10 |  971 | `		ph7_result_null(pCtx);` |
|       10 |  972 | `		return SXRET_OK;` |
|        - |  973 | `	}` |
|    32807 |  974 | `	if( !ret_string ){` |
|        - |  975 | `		/* Output dump */` |
|     1293 |  976 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  977 | `		/* Return NULL */` |
|     1293 |  978 | `		ph7_result_null(pCtx);` |
|      649 |  979 | `	}else{` |
|        - |  980 | `		/* Generated dump as return value */` |
|    31519 |  981 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  982 | `	}` |
|        - |  983 | `	/* Release the working buffer */` |
|    32807 |  984 | `	SyBlobRelease(&sDump);` |
|    32807 |  985 | `	return SXRET_OK;` |
|    16275 |  986 | `}` |
|        - |  987 |  |
