# src/ph7/vm_builtin_var.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 482/520 lines (92.69%)

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
|   163073 |   23 | `PH7_PRIVATE int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |   24 | `{` |
|        - |   25 | `	ph7_value *pObj;` |
|   163078 |   26 | `	int res = 0;` |
|        - |   27 | `	int i;` |
|   163078 |   28 | `	if( nArg < 1 ){` |
|        - |   29 | `		/* Missing arguments,return false */` |
|      ! 0 |   30 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 |   31 | `		return SXRET_OK;` |
|        - |   32 | `	}` |
|        - |   33 | `	/* Iterate over available arguments */` |
|   200603 |   34 | `	for( i = 0 ; i < nArg ; ++i ){` |
|   163078 |   35 | `		pObj = apArg[i];` |
|   163078 |   36 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|        - |   37 | `			/* Skip the "expecting a variable" warning for MEMOBJ_BOOL —` |
|        - |   38 | `			 * synthesized by LOAD_IDX iP2=4 (ArrayAccess::offsetExists) and` |
|        - |   39 | `			 * by anyone passing a bool literal (rare, harmless). */` |
|   162878 |   40 | `			if( (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|        - |   41 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 |   42 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 |   43 | `			}` |
|    81412 |   44 | `		}` |
|   163078 |   45 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|   163078 |   46 | `		if( !res ){` |
|        - |   47 | `			/* Variable not set,return FALSE */` |
|   125553 |   48 | `			ph7_result_bool(pCtx,0);` |
|   125553 |   49 | `			return SXRET_OK;` |
|        - |   50 | `		}` |
|    18764 |   51 | `	}` |
|        - |   52 | `	/* All given variable are set,return TRUE */` |
|    37530 |   53 | `	ph7_result_bool(pCtx,1);` |
|    37530 |   54 | `	return SXRET_OK;` |
|    81517 |   55 | `}` |
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
|    12311 |   82 | `PH7_PRIVATE sxi32 VmUnsetVarByNameEx(ph7_vm *pVm,VmFrame *pFrame,const char *zName,sxu32 nByte,` |
|        - |   83 | `	int bNameGuard)` |
|        5 |   84 | `{` |
|        - |   85 | `	SyHashEntry *pEntry;` |
|        - |   86 | `	int bRegistered;` |
|        - |   87 | `	sxu32 nIdx;` |
|        - |   88 | `	/* php 8.1 forbids unset($GLOBALS) outright. Checked by NAME: the superglobal is not an` |
|        - |   89 | `	 * ordinary hVar binding, so the slot-index test below never sees it. */` |
|    12316 |   90 | `	if( bNameGuard && nByte == sizeof("GLOBALS")-1 && SyMemcmp(zName,"GLOBALS",nByte) == 0 ){` |
|        3 |   91 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |   92 | `			"$GLOBALS can only be modified using the $GLOBALS[$name] = $value syntax");` |
|        3 |   93 | `		pVm->iExitStatus = 255;` |
|        3 |   94 | `		pVm->bHaltRequested = 1;` |
|        3 |   95 | `		return PH7_ABORT;` |
|        - |   96 | `	}` |
|    12314 |   97 | `	pEntry = SyHashGet(&pFrame->hVar,(const void *)zName,nByte);` |
|    12314 |   98 | `	if( pEntry == 0 ){` |
|        - |   99 | `		/* No such variable: unset() is a no-op on an undefined name, as in php */` |
|     1813 |  100 | `		return SXRET_OK;` |
|        - |  101 | `	}` |
|        - |  102 | `	/* The binding about to go may be memoized on the frame (see VmFrame). */` |
|    10506 |  103 | `	VmVarMemoFlush(pFrame);` |
|    10506 |  104 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|    10506 |  105 | `	if( nIdx == pVm->nGlobalIdx ){` |
|      ! 0 |  106 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  107 | `			"$GLOBALS can only be modified using the $GLOBALS[$name] = $value syntax");` |
|      ! 0 |  108 | `		pVm->iExitStatus = 255;` |
|      ! 0 |  109 | `		pVm->bHaltRequested = 1;` |
|      ! 0 |  110 | `		return PH7_ABORT;` |
|        - |  111 | `	}` |
|    10506 |  112 | `	bRegistered = PH7_VmSlotRegistered(&(*pVm),nIdx);` |
|        - |  113 | `	/*` |
|        - |  114 | `	 * A GLOBAL variable is ALSO an entry of the $GLOBALS array, and that node points at the` |
|        - |  115 | `	 * same slot. php's unset($x) in global scope drops $GLOBALS['x'] too, so unlink the node` |
|        - |  116 | `	 * — otherwise it stays a live holder and the value is never released ($o = new D;` |
|        - |  117 | `	 * unset($o); stopped running the destructor). Clear the reference table's row for the` |
|        - |  118 | `	 * node BEFORE unlinking it: unlinking frees the node, and the holder count below would` |
|        - |  119 | `	 * otherwise dereference freed memory.` |
|        - |  120 | `	 */` |
|    10506 |  121 | `	if( pFrame->pParent == 0 ){` |
|    10376 |  122 | `		ph7_value *pGlobals = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pVm->nGlobalIdx);` |
|    10376 |  123 | `		if( pGlobals && (pGlobals->iFlags & MEMOBJ_HASHMAP) ){` |
|    10376 |  124 | `			ph7_hashmap_node *pNode = 0;` |
|        - |  125 | `			ph7_value sKey;` |
|        - |  126 | `			SyString sName;` |
|    10376 |  127 | `			PH7_MemObjInit(&(*pVm),&sKey);` |
|    10376 |  128 | `			SyStringInitFromBuf(&sName,zName,nByte);` |
|    10376 |  129 | `			PH7_MemObjInitFromString(&(*pVm),&sKey,&sName);` |
|    10371 |  130 | `			if( SXRET_OK == PH7_HashmapLookup((ph7_hashmap *)pGlobals->x.pOther,&sKey,&pNode)` |
|    10369 |  131 | `			 && pNode && pNode->nValIdx == nIdx ){` |
|    10362 |  132 | `				if( bRegistered ){` |
|    10362 |  133 | `					PH7_VmRefObjRemove(&(*pVm),nIdx,0,pNode);` |
|     5174 |  134 | `				}` |
|    10362 |  135 | `				PH7_HashmapUnlinkNode(pNode,FALSE);` |
|     5174 |  136 | `			}` |
|    10376 |  137 | `			PH7_MemObjRelease(&sKey);` |
|     5181 |  138 | `		}` |
|     5181 |  139 | `	}` |
|        - |  140 |  |
|        - |  141 | `	/* The frame's own "release this reference at exit" row for the binding about to go:` |
|        - |  142 | `	 * the entry is freed below, so the row would dangle (and a foreach value variable` |
|        - |  143 | `	 * unset once per loop filed one row per loop, which nothing consumed until the` |
|        - |  144 | `	 * function returned). */` |
|    10506 |  145 | `	VmDropFrameRefEntry(&(*pVm),nIdx,pEntry);` |
|    10506 |  146 | `	if( !bRegistered ){` |
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
|    10506 |  158 | `		PH7_VmRefObjRemove(&(*pVm),nIdx,pEntry,0);` |
|    10506 |  159 | `		SyHashDeleteEntry2(pEntry);` |
|        - |  160 | `		/* The value goes with the LAST holder and not before — the one rule, counted in` |
|        - |  161 | `		 * one place: other names, array nodes that still point here, and a PIN (a static's` |
|        - |  162 | ``		 * storage, a `use (&$x)` capture, a reference-bound property), which is a holder`` |
|        - |  163 | `		 * this table cannot name. Unsetting the name of a static used to release the` |
|        - |  164 | `		 * static's value, so the next call started over from the initializer. */` |
|    10506 |  165 | `		PH7_VmReleaseUnheldSlot(&(*pVm),nIdx);` |
|        - |  166 | `	}` |
|    10506 |  167 | `	return SXRET_OK;` |
|     6156 |  168 | `}` |
|     8772 |  169 | `PH7_PRIVATE sxi32 VmUnsetVarByName(ph7_vm *pVm,VmFrame *pFrame,const char *zName,sxu32 nByte)` |
|        5 |  170 | `{` |
|     8777 |  171 | `	return VmUnsetVarByNameEx(&(*pVm),pFrame,zName,nByte,TRUE);` |
|        5 |  172 | `}` |
| 22489665 |  173 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        5 |  174 | `{` |
|        - |  175 | `	ph7_value *pObj;` |
| 22489670 |  176 | `	pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nObjIdx);` |
| 22489670 |  177 | `	if( pObj ){` |
|        - |  178 | `		/* Release the object */` |
| 22489670 |  179 | `		PH7_MemObjRelease(pObj);` |
| 11242440 |  180 | `	}` |
|        - |  181 | `	/* Remove old reference links. The permanent pin is read BEFORE the unlink --` |
|        - |  182 | `	 * it is what decides whether the index goes back to the free pool, and the` |
|        - |  183 | `	 * unlink is what takes the answer away. */` |
| 22489670 |  184 | `	if( PH7_VmSlotRegistered(&(*pVm),nObjIdx) ){` |
| 22489624 |  185 | `		int bKeep = PH7_VmSlotKeepPinned(&(*pVm),nObjIdx);` |
|        - |  186 | `		/* Unlink from the reference table */` |
| 22489624 |  187 | `		PH7_VmSlotUnlink(&(*pVm),nObjIdx);` |
| 22489624 |  188 | `		if( (bForce == TRUE) \|\| bKeep == 0 ){` |
|        - |  189 | `			/* Restore to the free list */` |
| 22489624 |  190 | `			VmMemPoolFreeSlot(&pVm->aMemObj,nObjIdx);` |
| 11242417 |  191 | `		}` |
| 11242417 |  192 | `	}` |
| 22489670 |  193 | `	return SXRET_OK;` |
|        5 |  194 | `}` |
|        - |  195 | `/*` |
|        - |  196 | ` * void unset($var,...)` |
|        - |  197 | ` *   Unset one or more given variable.` |
|        - |  198 | ` * Parameters` |
|        - |  199 | ` *  One or more variable to unset.` |
|        - |  200 | ` * Return` |
|        - |  201 | ` *  Nothing.` |
|        - |  202 | ` */` |
|     1586 |  203 | `PH7_PRIVATE int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  204 | `{` |
|        - |  205 | `	ph7_value *pObj;` |
|        - |  206 | `	ph7_vm *pVm;` |
|        - |  207 | `	int i;` |
|        - |  208 | `	/* Point to the target VM */` |
|     1591 |  209 | `	pVm = pCtx->pVm;` |
|        - |  210 | `	/* Iterate and unset */` |
|     3177 |  211 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     1591 |  212 | `		pObj = apArg[i];` |
|     1591 |  213 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|     1589 |  214 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  215 | `				/* Throw an error */` |
|      ! 0 |  216 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 |  217 | `			}` |
|      797 |  218 | `		}else{` |
|        3 |  219 | `			sxu32 nIdx = pObj->nIdx;` |
|        3 |  220 | `			if( nIdx == pVm->nGlobalIdx ){` |
|        - |  221 | `				/* php 8.1: unset($GLOBALS) is forbidden — the same fatal as` |
|        - |  222 | `				 * re-assigning it (compile-time in php, raised here). */` |
|      ! 0 |  223 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  224 | `					"$GLOBALS can only be modified using the $GLOBALS[$name] = $value syntax");` |
|      ! 0 |  225 | `				pVm->iExitStatus = 255;` |
|      ! 0 |  226 | `				pVm->bHaltRequested = 1;` |
|      ! 0 |  227 | `				return PH7_ABORT;` |
|        - |  228 | `			}` |
|        3 |  229 | `			PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|        - |  230 | `			/* Drop the stale local-teardown entry for the freed slot (see` |
|        - |  231 | `			 * VmDropFrameLocalSlot) so a later index reuse is not double-freed. */` |
|        3 |  232 | `			VmDropFrameLocalSlot(&(*pVm),nIdx);` |
|        - |  233 | `		}` |
|      798 |  234 | `	}` |
|     1591 |  235 | `	return SXRET_OK;` |
|      798 |  236 | `}` |
|        - |  237 | `/*` |
|        - |  238 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - |  239 | ` */` |
|    12552 |  240 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        4 |  241 | `{` |
|    12556 |  242 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|    12556 |  243 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  244 | `	ph7_value *pObj;` |
|        - |  245 | `	sxu32 nIdx;` |
|        - |  246 | `	/* php excludes $this from get_defined_vars() (it is bound implicitly, not a` |
|        - |  247 | `	 * declared local). PHL keeps it in the frame's hVar, so skip it here. */` |
|    12552 |  248 | `	if( pEntry->nKeyLen == sizeof("this")-1` |
|     7452 |  249 | `	 && SyMemcmp(pEntry->pKey,"this",sizeof("this")-1) == 0 ){` |
|        6 |  250 | `		return SXRET_OK;` |
|        - |  251 | `	}` |
|        - |  252 | `	/* Engine temporaries (a foreach destructuring/target slot) are not variables the` |
|        - |  253 | `	 * program declared — php compiles those into slots with no name at all. */` |
|    12552 |  254 | `	if( PH7_VmVarNameIsInternal((const char *)pEntry->pKey,pEntry->nKeyLen) ){` |
|      771 |  255 | `		return SXRET_OK;` |
|        - |  256 | `	}` |
|        - |  257 | `	/* Extract the memory object */` |
|    11782 |  258 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|    11782 |  259 | `	pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nIdx);` |
|    11782 |  260 | `	if( pObj ){` |
|    11782 |  261 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|    11770 |  262 | `			if( pEntry->nKeyLen > 0 ){` |
|        - |  263 | `				SyString sName;` |
|        - |  264 | `				ph7_value sKey;` |
|        - |  265 | `				/* Perform the insertion (pObj may point into pVm->aMemObj; the` |
|        - |  266 | `				 * inserter snapshots the source before reserving, so the pool may` |
|        - |  267 | `				 * safely move underneath it — see HashmapInsertIntKey/BlobKey). */` |
|    11770 |  268 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|    11770 |  269 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|    11770 |  270 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|    11770 |  271 | `				PH7_MemObjRelease(&sKey);` |
|     5883 |  272 | `			}` |
|     5883 |  273 | `		}` |
|     5889 |  274 | `	}` |
|    11782 |  275 | `	return SXRET_OK;` |
|     6280 |  276 | `}` |
|        - |  277 | `/*` |
|        - |  278 | ` * array get_defined_vars(void)` |
|        - |  279 | ` *  Returns an array of all defined variables.` |
|        - |  280 | ` * Parameter` |
|        - |  281 | ` *  None` |
|        - |  282 | ` * Return` |
|        - |  283 | ` *  An array with all the variables defined in the current scope.` |
|        - |  284 | ` */` |
|       70 |  285 | `PH7_PRIVATE int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        4 |  286 | `{` |
|       74 |  287 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  288 | `	ph7_value *pArray;` |
|        - |  289 | `	VmFrame *pFrame;` |
|        - |  290 | `	/* Create a new array */` |
|       74 |  291 | `	pArray = ph7_context_new_array(pCtx);` |
|       74 |  292 | ` 	if( pArray == 0 ){` |
|      ! 0 |  293 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  294 | `		SXUNUSED(apArg);` |
|        - |  295 | `		/* Return NULL */` |
|      ! 0 |  296 | `		ph7_result_null(pCtx);` |
|      ! 0 |  297 | `		return SXRET_OK;` |
|        - |  298 | `	}` |
|        - |  299 | `	/* A try/catch block pushes an EXCEPTION frame that holds no variables of its` |
|        - |  300 | `	 * own, so the enclosing function (or global) frame is the one php reports.` |
|        - |  301 | `	 * Reading pVm->pFrame raw answered [] for every call inside a try/catch —` |
|        - |  302 | `	 * including the superglobal test below, which saw a non-NULL pParent and` |
|        - |  303 | `	 * dropped $argv/$_SERVER at global scope. Every other frame-lookup site` |
|        - |  304 | `	 * (VmExtractMemObj, compact(), func_get_args()) already skips them. */` |
|       74 |  305 | `	pFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|        - |  306 | `	/* Superglobals appear in get_defined_vars() ONLY at the global scope (php).` |
|        - |  307 | `	 * Inside a function the result is the local symbol table alone — a leak of` |
|        - |  308 | `	 * $argv/$_SERVER/... into every function's scope breaks callers that treat the` |
|        - |  309 | `	 * keys as real locals (e.g. PHPUnit's doubled-method template reflects each` |
|        - |  310 | `	 * get_defined_vars() name as a ReflectionParameter). */` |
|       74 |  311 | `	if( pFrame->pParent == 0 ){` |
|       14 |  312 | `		SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        6 |  313 | `	}` |
|        - |  314 | `	/* Then variables defined in the current frame, in DECLARATION order.` |
|        - |  315 | `	 * The frame table is head-pushed (SyHashInsert), so its forward order is` |
|        - |  316 | `	 * reverse-insertion; walk it backward to match php, which returns locals in` |
|        - |  317 | `	 * the order they first appeared (a,b,c — a reassignment reuses the slot and` |
|        - |  318 | `	 * keeps its original position). */` |
|       74 |  319 | `	SyHashForEachReverse(&pFrame->hVar,VmHashVarWalker,pArray);` |
|        - |  320 | `	/* Finally,return the created array */` |
|       74 |  321 | `	ph7_result_value(pCtx,pArray);` |
|       74 |  322 | `	return SXRET_OK;` |
|       39 |  323 | `}` |
|        - |  324 | `/*` |
|        - |  325 | ` * string get_debug_type(mixed $value)` |
|        - |  326 | ` *  php 8.0's type name for diagnostics: the SHORT scalar names, and a class` |
|        - |  327 | ` *  name for an object. Distinct from gettype(), which keeps php 4's long` |
|        - |  328 | ` *  spellings ("integer"/"boolean"/"NULL") for compatibility.` |
|        - |  329 | ` *` |
|        - |  330 | ` *  This was a prelude function in the Reflection chunk, where the TypeError` |
|        - |  331 | ` *  messages needed it; it is what php ships natively, and the SPL chunk's` |
|        - |  332 | ` *  messages want it too.` |
|        - |  333 | ` */` |
|      194 |  334 | `PH7_PRIVATE int vm_builtin_get_debug_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        4 |  335 | `{` |
|      198 |  336 | `	const char *zType = "null";` |
|      198 |  337 | `	if( nArg > 0 ){` |
|      198 |  338 | `		ph7_value *pVal = apArg[0];` |
|      198 |  339 | `		if( pVal->iFlags & MEMOBJ_OBJ ){` |
|       76 |  340 | `			ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|      112 |  341 | `			ph7_result_string(pCtx,SyStringData(&pThis->pClass->sName),` |
|       72 |  342 | `				(int)SyStringLength(&pThis->pClass->sName));` |
|       76 |  343 | `			return SXRET_OK;` |
|        - |  344 | `		}` |
|      126 |  345 | `		if( pVal->iFlags & MEMOBJ_NULL ){` |
|        8 |  346 | `			zType = "null";` |
|      123 |  347 | `		}else if( pVal->iFlags & MEMOBJ_REAL ){` |
|        - |  348 | `			/* REAL wins over a cached MEMOBJ_INT, as it does for gettype() */` |
|       16 |  349 | `			zType = "float";` |
|      113 |  350 | `		}else if( pVal->iFlags & MEMOBJ_INT ){` |
|       29 |  351 | `			zType = "int";` |
|       93 |  352 | `		}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|       17 |  353 | `			zType = "string";` |
|       73 |  354 | `		}else if( pVal->iFlags & MEMOBJ_BOOL ){` |
|       32 |  355 | `			zType = "bool";` |
|       51 |  356 | `		}else if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|       10 |  357 | `			zType = "array";` |
|       31 |  358 | `		}else if( pVal->iFlags & MEMOBJ_RES ){` |
|        - |  359 | `			/* php names the resource's TYPE here, and this used to answer the` |
|        - |  360 | `			 * bare "resource" gettype() answers — so the function whose whole` |
|        - |  361 | `			 * job is to NAME a value's type had a different answer from php for` |
|        - |  362 | `			 * every open handle, in exactly the diagnostics it exists for.` |
|        - |  363 | `			 * (The note this replaced said the kind was unavailable; it is what` |
|        - |  364 | `			 * get_resource_type() has been answering all along.) */` |
|       27 |  365 | `			if( PH7_VfsResourceIsClosed(pVal->x.pOther) ){` |
|        5 |  366 | `				zType = "resource (closed)";` |
|        3 |  367 | `			}else{` |
|       33 |  368 | `				ph7_result_string_format(pCtx,"resource (%s)",` |
|       10 |  369 | `					PH7_VfsResourceType(pVal->x.pOther));` |
|       23 |  370 | `				return SXRET_OK;` |
|        - |  371 | `			}` |
|        2 |  372 | `		}` |
|       51 |  373 | `	}` |
|      106 |  374 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|      106 |  375 | `	return SXRET_OK;` |
|      101 |  376 | `}` |
|        - |  377 | `/*` |
|        - |  378 | ` * bool gettype($var)` |
|        - |  379 | ` *  Get the type of a variable` |
|        - |  380 | ` * Parameters` |
|        - |  381 | ` *   $var` |
|        - |  382 | ` *    The variable being type checked.` |
|        - |  383 | ` * Return` |
|        - |  384 | ` *   String representation of the given variable type.` |
|        - |  385 | ` */` |
|      603 |  386 | `PH7_PRIVATE int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 |  387 | `{` |
|        - |  388 | `	/* php's gettype() uses LONG type names ("integer"/"boolean"/"NULL"), distinct` |
|        - |  389 | `	 * from get_debug_type()'s short ones ("int"/"bool"/"null") — never route this` |
|        - |  390 | `	 * through PH7_MemObjTypeDump, which yields the short forms. */` |
|      606 |  391 | `	const char *zType = "unknown type";` |
|      606 |  392 | `	if( nArg > 0 ){` |
|      606 |  393 | `		ph7_value *pVal = apArg[0];` |
|      606 |  394 | `		if( pVal->iFlags & MEMOBJ_NULL ){` |
|       15 |  395 | `			zType = "NULL";` |
|      599 |  396 | `		}else if( pVal->iFlags & MEMOBJ_REAL ){` |
|        - |  397 | `			/* REAL wins over a cached MEMOBJ_INT: 1.0 is "double" (php). */` |
|       92 |  398 | `			zType = "double";` |
|      547 |  399 | `		}else if( pVal->iFlags & MEMOBJ_INT ){` |
|      206 |  400 | `			zType = "integer";` |
|      398 |  401 | `		}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|      193 |  402 | `			zType = "string";` |
|      204 |  403 | `		}else if( pVal->iFlags & MEMOBJ_BOOL ){` |
|       26 |  404 | `			zType = "boolean";` |
|       97 |  405 | `		}else if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|       46 |  406 | `			zType = "array";` |
|       63 |  407 | `		}else if( pVal->iFlags & MEMOBJ_OBJ ){` |
|       22 |  408 | `			zType = "object";` |
|       30 |  409 | `		}else if( pVal->iFlags & MEMOBJ_RES ){` |
|        - |  410 | `			/* php reports an fclose()'d handle as "resource (closed)" */` |
|       20 |  411 | `			zType = PH7_VfsResourceIsClosed(pVal->x.pOther) ? "resource (closed)" : "resource";` |
|        9 |  412 | `		}` |
|      299 |  413 | `	}` |
|        - |  414 | `	/* Return the variable type */` |
|      606 |  415 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|      606 |  416 | `	return SXRET_OK;` |
|        3 |  417 | `}` |
|        - |  418 | `/*` |
|        - |  419 | ` * bool settype(mixed &$var, string $type)` |
|        - |  420 | ` *  Convert $var IN PLACE to the type named by $type, mirroring the (type) cast` |
|        - |  421 | ` *  operators exactly (same PH7_MemObjTo* helpers CVT_INT/REAL/STR/BOOL/ARRAY/OBJ` |
|        - |  422 | ` *  use), and write the result back through the by-ref out-param.` |
|        - |  423 | ` * Parameters` |
|        - |  424 | ` *   &$var : the variable to convert (compile.c's GenStateByRefBuiltinMask marks` |
|        - |  425 | ` *           position 0 by-reference so an undefined variable is auto-vivified).` |
|        - |  426 | ` *   $type : "int"/"integer", "float"/"double", "string", "bool"/"boolean",` |
|        - |  427 | ` *           "array", "object", "null" (case-insensitive).` |
|        - |  428 | ` * Return` |
|        - |  429 | ` *   Always true on success; a non-referenceable $var is an Error, an unknown` |
|        - |  430 | ` *   $type (or the un-castable "resource") is a ValueError — php-exact.` |
|        - |  431 | ` */` |
|       84 |  432 | `PH7_PRIVATE int vm_builtin_settype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  433 | `{` |
|        - |  434 | `	const char *zType;` |
|        - |  435 | `	int nLen;` |
|        - |  436 | `	ph7_value *pNew;` |
|       31 |  437 | `	SXUNUSED(nArg); /* arity (exactly 2) enforced by the central arity table */` |
|        - |  438 | `	/* php binds $var by reference at the CALL, and the refusal is the call site's` |
|        - |  439 | `	 * to raise (PH7_VmScreenByRefArgShapes) — only the compiler can tell a literal,` |
|        - |  440 | `	 * which php refuses, from the result of a call, which php accepts with a notice` |
|        - |  441 | ``	 * and converts in place on the temporary. The `nIdx == SXU32_HIGH` test that`` |
|        - |  442 | `	 * used to sit here conflated the two. */` |
|       89 |  443 | `	zType = ph7_value_to_string(apArg[1],&nLen);` |
|        - |  444 | `	/* Validate the target type up-front (php checks $type before touching $var):` |
|        - |  445 | `	 * "resource" is a distinct ValueError, any other unknown name is the generic` |
|        - |  446 | `	 * invalid-type ValueError. */` |
|       89 |  447 | `	if( nLen == 8 && SyStrnicmp(zType,"resource",8) == 0 ){` |
|        3 |  448 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|        - |  449 | `			"Cannot convert to resource type");` |
|        - |  450 | `	}` |
|       88 |  451 | `	if( !(  (nLen == 3 && SyStrnicmp(zType,"int",3) == 0)` |
|       62 |  452 | `	     \|\| (nLen == 7 && SyStrnicmp(zType,"integer",7) == 0)` |
|       55 |  453 | `	     \|\| (nLen == 5 && SyStrnicmp(zType,"float",5) == 0)` |
|       54 |  454 | `	     \|\| (nLen == 6 && SyStrnicmp(zType,"double",6) == 0)` |
|       53 |  455 | `	     \|\| (nLen == 6 && SyStrnicmp(zType,"string",6) == 0)` |
|       37 |  456 | `	     \|\| (nLen == 4 && SyStrnicmp(zType,"bool",4) == 0)` |
|       11 |  457 | `	     \|\| (nLen == 7 && SyStrnicmp(zType,"boolean",7) == 0)` |
|       10 |  458 | `	     \|\| (nLen == 5 && SyStrnicmp(zType,"array",5) == 0)` |
|        8 |  459 | `	     \|\| (nLen == 6 && SyStrnicmp(zType,"object",6) == 0)` |
|        3 |  460 | `	     \|\| (nLen == 4 && SyStrnicmp(zType,"null",4) == 0) ) ){` |
|        3 |  461 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|        - |  462 | `			"settype(): Argument #2 ($type) must be a valid type");` |
|        - |  463 | `	}` |
|        - |  464 | `	/* Convert a COPY of the current value (the To* helpers mutate in place), then` |
|        - |  465 | `	 * store it through the by-ref out-param — settype changes $var's TYPE, so the` |
|        - |  466 | `	 * new value must land in the caller slot (nIdx), not just in shared contents. */` |
|       95 |  467 | `	pNew = ph7_context_new_scalar(pCtx);` |
|       95 |  468 | `	if( pNew == 0 ){` |
|      ! 0 |  469 | `		return PH7_ContextMemoryError(pCtx);` |
|        - |  470 | `	}` |
|       95 |  471 | `	PH7_MemObjStore(apArg[0],pNew);` |
|       90 |  472 | `	if( (nLen == 3 && SyStrnicmp(zType,"int",3) == 0)` |
|       75 |  473 | `	 \|\| (nLen == 7 && SyStrnicmp(zType,"integer",7) == 0) ){` |
|        - |  474 | `		/* settype() IS the cast operator, warning included. */` |
|       55 |  475 | `		PH7_MemObjWarnIntCast(pNew);` |
|       55 |  476 | `		PH7_MemObjToInteger(pNew);` |
|       55 |  477 | `		MemObjSetType(pNew,MEMOBJ_INT);` |
|       79 |  478 | `	}else if( (nLen == 5 && SyStrnicmp(zType,"float",5) == 0)` |
|       53 |  479 | `	       \|\| (nLen == 6 && SyStrnicmp(zType,"double",6) == 0) ){` |
|        5 |  480 | `		PH7_MemObjToReal(pNew);` |
|        5 |  481 | `		MemObjSetType(pNew,MEMOBJ_REAL);` |
|       61 |  482 | `	}else if( nLen == 6 && SyStrnicmp(zType,"string",6) == 0 ){` |
|        - |  483 | `		/* php emits "Array to string conversion" for settype($arr,'string'), and` |
|        - |  484 | `		 * throws "Object of class X could not be converted to string" for an` |
|        - |  485 | `		 * object with no __toString() (or propagates one that threw). php's` |
|        - |  486 | `		 * convert_to_string() has already blanked the zval by then, so a CAUGHT` |
|        - |  487 | `		 * settype() leaves $var === "" — store that, then propagate instead of` |
|        - |  488 | `		 * answering true. */` |
|       36 |  489 | `		sxi32 rcSv = PH7_MemObjToStringUV(pNew);` |
|       36 |  490 | `		if( rcSv != SXRET_OK ){` |
|       16 |  491 | `			PH7_MemObjRelease(pNew);` |
|       16 |  492 | `			MemObjSetType(pNew,MEMOBJ_STRING);` |
|       16 |  493 | `			PH7_VmStoreArgByRef(pCtx->pVm,apArg[0],pNew);` |
|       16 |  494 | `			pCtx->nThrowRc = rcSv;` |
|       16 |  495 | `			return rcSv;` |
|        - |  496 | `		}` |
|       27 |  497 | `	}else if( (nLen == 4 && SyStrnicmp(zType,"bool",4) == 0)` |
|       14 |  498 | `	       \|\| (nLen == 7 && SyStrnicmp(zType,"boolean",7) == 0) ){` |
|        8 |  499 | `		PH7_MemObjToBool(pNew);` |
|       12 |  500 | `	}else if( nLen == 5 && SyStrnicmp(zType,"array",5) == 0 ){` |
|        5 |  501 | `		PH7_MemObjToHashmap(pNew);` |
|        7 |  502 | `	}else if( nLen == 6 && SyStrnicmp(zType,"object",6) == 0 ){` |
|        3 |  503 | `		PH7_MemObjToObject(pNew);` |
|        2 |  504 | `	}else{` |
|        - |  505 | `		/* "null" — the only validated name left */` |
|        3 |  506 | `		PH7_MemObjToNull(pNew);` |
|        - |  507 | `	}` |
|       93 |  508 | `	PH7_VmStoreArgByRef(pCtx->pVm,apArg[0],pNew);` |
|       93 |  509 | `	ph7_result_bool(pCtx,1);` |
|       93 |  510 | `	return SXRET_OK;` |
|       58 |  511 | `}` |
|        - |  512 | `/*` |
|        - |  513 | ` * string get_resource_type(resource $handle)` |
|        - |  514 | ` *  This function gets the type of the given resource.` |
|        - |  515 | ` * Parameters` |
|        - |  516 | ` *  $handle` |
|        - |  517 | ` *  The evaluated resource handle.` |
|        - |  518 | ` * Return` |
|        - |  519 | ` *  If the given handle is a resource, this function will return a string` |
|        - |  520 | ` *  representing its type. If the type is not identified by this function` |
|        - |  521 | ` *  the return value will be the string Unknown.` |
|        - |  522 | ` *  This function will return FALSE and generate an error if handle` |
|        - |  523 | ` *  is not a resource.` |
|        - |  524 | ` */` |
|       70 |  525 | `PH7_PRIVATE int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 |  526 | `{` |
|        - |  527 | `	const char *zType;` |
|       73 |  528 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - |  529 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 |  530 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  531 | `		return PH7_OK;` |
|        - |  532 | `	}` |
|        - |  533 | `	/* PHP returns the resource TYPE name (e.g. "stream" for IO handles), not an id. */` |
|       73 |  534 | `	zType = PH7_VfsResourceType(apArg[0]->x.pOther);` |
|       73 |  535 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       73 |  536 | `	return SXRET_OK;` |
|       38 |  537 | `}` |
|        - |  538 | `/*` |
|        - |  539 | ` * int get_resource_id(resource $resource)` |
|        - |  540 | ` *  Returns the integer id of a resource — the same number (int) casts to and` |
|        - |  541 | ` *  "Resource id #N" renders (php 8.0).` |
|        - |  542 | ` */` |
|        4 |  543 | `PH7_PRIVATE int vm_builtin_get_resource_id(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  544 | `{` |
|        - |  545 | `	char zGiven[64];` |
|        6 |  546 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 |  547 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|        - |  548 | `			"get_resource_id(): Argument #1 ($resource) must be of type resource, %s given",` |
|      ! 0 |  549 | `			nArg < 1 ? "none" : VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));` |
|        - |  550 | `	}` |
|        6 |  551 | `	ph7_result_int64(pCtx,(ph7_int64)PH7_VmResourceId(pCtx->pVm,apArg[0]->x.pOther));` |
|        6 |  552 | `	return SXRET_OK;` |
|        4 |  553 | `}` |
|        - |  554 | `/*` |
|        - |  555 | ` * void var_dump(expression,....)` |
|        - |  556 | ` *   var_dump � Dumps information about a variable` |
|        - |  557 | ` * Parameters` |
|        - |  558 | ` *   One or more expression to dump.` |
|        - |  559 | ` * Returns` |
|        - |  560 | ` *  Nothing.` |
|        - |  561 | ` */` |
|     9835 |  562 | `PH7_PRIVATE int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  563 | `{` |
|        - |  564 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - |  565 | `	int i;` |
|     9840 |  566 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - |  567 | `	/* Dump one or more expressions */` |
|    24097 |  568 | `	for( i = 0 ; i < nArg ; i++ ){` |
|    14262 |  569 | `		ph7_value *pObj = apArg[i];` |
|        - |  570 | `		/* Reset the working buffer */` |
|    14262 |  571 | `		SyBlobReset(&sDump);` |
|        - |  572 | `		/* Dump the given expression */` |
|    14262 |  573 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - |  574 | `		/* Output */` |
|    14262 |  575 | `		if( SyBlobLength(&sDump) > 0 ){` |
|    14262 |  576 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|     7112 |  577 | `		}` |
|     7117 |  578 | `	}` |
|        - |  579 | `	/* Release the working buffer */` |
|     9840 |  580 | `	SyBlobRelease(&sDump);` |
|     9840 |  581 | `	return SXRET_OK;` |
|        5 |  582 | `}` |
|        - |  583 | `/*` |
|        - |  584 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - |  585 | ` *   print-r - Prints human-readable information about a variable` |
|        - |  586 | ` * Parameters` |
|        - |  587 | ` *   expression: Expression to dump` |
|        - |  588 | ` *   return : If you would like to capture the output of print_r() use` |
|        - |  589 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - |  590 | ` *            print_r() will return the information rather than print it.` |
|        - |  591 | ` * Return` |
|        - |  592 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - |  593 | ` *  Otherwise, the return value is TRUE.` |
|        - |  594 | ` */` |
|      320 |  595 | `PH7_PRIVATE int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  596 | `{` |
|      325 |  597 | `	int ret_string = 0;` |
|        - |  598 | `	SyBlob sDump;` |
|      325 |  599 | `	if( nArg < 1 ){` |
|        - |  600 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  601 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  602 | `		return SXRET_OK;` |
|        - |  603 | `	}` |
|      325 |  604 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|      325 |  605 | `	if ( nArg > 1 ){` |
|        - |  606 | `		/* Where to redirect output */` |
|       28 |  607 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|       12 |  608 | `	}` |
|        - |  609 | `	/* Generate dump */` |
|      325 |  610 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|      325 |  611 | `	if( !ret_string ){` |
|        - |  612 | `		/* Output dump */` |
|      301 |  613 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  614 | `		/* Return true */` |
|      301 |  615 | `		ph7_result_bool(pCtx,1);` |
|      153 |  616 | `	}else{` |
|        - |  617 | `		/* Generated dump as return value */` |
|       28 |  618 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  619 | `	}` |
|        - |  620 | `	/* Release the working buffer */` |
|      325 |  621 | `	SyBlobRelease(&sDump);` |
|      325 |  622 | `	return SXRET_OK;` |
|      165 |  623 | `}` |
|        - |  624 | `/*` |
|        - |  625 | ` * var_export() — PHP-exact evaluable representation of a value.` |
|        - |  626 | ` *` |
|        - |  627 | ` * Distinct from print_r/var_dump (which share PH7_MemObjDump): var_export emits` |
|        - |  628 | ` * parseable PHP — 'single-quoted' strings, true/false/NULL, array (\n  k => v,\n),` |
|        - |  629 | ` * and \Class::__set_state(array(...)) for objects. PHP's indentation is quirky` |
|        - |  630 | ` * (2-space array entries; 3-space object property lines; a composite value always` |
|        - |  631 | ` * opens at containerIndent+2) but deterministic; this matches it byte-for-byte.` |
|        - |  632 | ` */` |
|        - |  633 | `/* Instance flag (distinct from oo.c's CLASS_INSTANCE_DESTROYED 0x001) marking an` |
|        - |  634 | ` * object currently on the var_export recursion stack, for cycle detection. */` |
|        - |  635 | `#define VM_INSTANCE_DUMPING 0x002` |
|        - |  636 | `typedef struct VmExportCtx VmExportCtx;` |
|        - |  637 | `struct VmExportCtx` |
|        - |  638 | `{` |
|        - |  639 | `	SyBlob *pOut;` |
|        - |  640 | `	int nIndent;  /* indentation of the container emitting this entry */` |
|        - |  641 | `	int depth;    /* recursion guard */` |
|        - |  642 | `	int bPresented; /* the entries are a native class's PRESENTED shape, which` |
|        - |  643 | `	                 * carries the object's own table MANGLED. var_export prints a` |
|        - |  644 | `	                 * non-public property under its PLAIN name (the attribute loop` |
|        - |  645 | `	                 * below already does), so the key is unmangled here too. */` |
|        - |  646 | `	int nKeyExtra; /* extra columns the KEY line carries beyond nIndent+2, and the` |
|        - |  647 | `	                * value's container indent does NOT. An object body's keys sit` |
|        - |  648 | `	                * one deeper than an array's while a nested array under one of` |
|        - |  649 | `	                * them sits at the OBJECT's indent -- which is why a` |
|        - |  650 | ``	                * SimpleXMLElement's `@attributes` row prints its array two`` |
|        - |  651 | `	                * columns in and not three. 0 for a plain array. */` |
|        - |  652 | `};` |
|        - |  653 | `static void VmExportValue(SyBlob *pOut, ph7_value *pVal, int nIndent, int depth);` |
|        - |  654 | `/* Append nIndent spaces. */` |
|     8815 |  655 | `static void VmExportIndent(SyBlob *pOut, int nIndent)` |
|        5 |  656 | `{` |
|        - |  657 | `	int i;` |
|    24526 |  658 | `	for( i = 0; i < nIndent; i++ ){ SyBlobAppend(pOut," ",1); }` |
|     8820 |  659 | `}` |
|        - |  660 | `/* Append a single-quoted PHP string literal (escapes ' and \, batching safe runs` |
|        - |  661 | ` * in one append). A NUL byte can't live in a single-quoted literal, so PHP splits` |
|        - |  662 | ` * it out as ' . "\0" . ' — match that. */` |
|    11645 |  663 | `static void VmExportQuoted(SyBlob *pOut, const char *z, int n)` |
|        5 |  664 | `{` |
|    11650 |  665 | `	int i, run = 0;` |
|    11650 |  666 | `	SyBlobAppend(pOut,"'",1);` |
|   138947 |  667 | `	for( i = 0; i < n; i++ ){` |
|   127302 |  668 | `		char c = z[i];` |
|   127302 |  669 | `		if( c != '\0' && c != '\'' && c != '\\' ){ continue; } /* extend the safe run */` |
|      591 |  670 | `		if( i > run ){ SyBlobAppend(pOut,&z[run],(sxu32)(i-run)); }` |
|      591 |  671 | `		if( c == '\0' ){ SyBlobAppend(pOut,"' . \"\\0\" . '",12); }` |
|      496 |  672 | `		else { SyBlobAppend(pOut,"\\",1); SyBlobAppend(pOut,&z[i],1); }` |
|      591 |  673 | `		run = i+1;` |
|      297 |  674 | `	}` |
|    11650 |  675 | `	if( n > run ){ SyBlobAppend(pOut,&z[run],(sxu32)(n-run)); }` |
|    11650 |  676 | `	SyBlobAppend(pOut,"'",1);` |
|    11650 |  677 | `}` |
|        - |  678 | `/* True if the array/object is already on the var_export recursion stack. */` |
|     5576 |  679 | `static int VmExportIsCycle(ph7_value *pVal)` |
|        5 |  680 | `{` |
|     5581 |  681 | `	if( ph7_value_is_array(pVal) ){` |
|      469 |  682 | `		return (((ph7_hashmap *)pVal->x.pOther)->iFlags & HASHMAP_DUMPING) != 0;` |
|        - |  683 | `	}` |
|     5116 |  684 | `	if( ph7_value_is_object(pVal) ){` |
|       60 |  685 | `		return (((ph7_class_instance *)pVal->x.pOther)->iFlags & VM_INSTANCE_DUMPING) != 0;` |
|        - |  686 | `	}` |
|     5060 |  687 | `	return 0;` |
|     2738 |  688 | `}` |
|        - |  689 | `/* Emit " => " then the value: a scalar inline, a composite on its own line at` |
|        - |  690 | ` * containerIndent+2. A circular reference renders inline as NULL (like PHP). */` |
|     5576 |  691 | `static void VmExportEntryValue(SyBlob *pOut, ph7_value *pVal, int nContainerIndent, int depth)` |
|        5 |  692 | `{` |
|     5581 |  693 | `	SyBlobAppend(pOut," => ",4);` |
|     5581 |  694 | `	if( VmExportIsCycle(pVal) ){` |
|        5 |  695 | `		SyBlobAppend(pOut,"NULL",4);` |
|     5579 |  696 | `	}else if( ph7_value_is_array(pVal) \|\| ph7_value_is_object(pVal) ){` |
|      522 |  697 | `		SyBlobAppend(pOut,"\n",1);` |
|      522 |  698 | `		VmExportIndent(pOut,nContainerIndent+2);` |
|      522 |  699 | `		VmExportValue(pOut,pVal,nContainerIndent+2,depth+1);` |
|      262 |  700 | `	}else{` |
|     5060 |  701 | `		VmExportValue(pOut,pVal,nContainerIndent+2,depth+1);` |
|        - |  702 | `	}` |
|     5581 |  703 | `	SyBlobAppend(pOut,",\n",2);` |
|     5581 |  704 | `}` |
|        - |  705 | `/* Array walker: "<indent+2>key => value,\n" for each entry. */` |
|     5484 |  706 | `static int VmExportArrayWalk(ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|        5 |  707 | `{` |
|     5489 |  708 | `	VmExportCtx *pC = (VmExportCtx *)pUserData;` |
|     5489 |  709 | `	VmExportIndent(pC->pOut,pC->nIndent+2+pC->nKeyExtra);` |
|     5489 |  710 | `	if( ph7_value_is_string(pKey) ){` |
|        - |  711 | `		int n;` |
|     1088 |  712 | `		const char *z = ph7_value_to_string(pKey,&n);` |
|     1096 |  713 | `		if( pC->bPresented && n > 0 && z[0] == 0 ){` |
|        - |  714 | `			SyString sUnmCls, sUnmName;` |
|       17 |  715 | `			SyStringInitFromBuf(&sUnmName,z,n);` |
|       17 |  716 | `			PH7_UnmangleAttrName(z,(sxu32)n,&sUnmCls,&sUnmName);` |
|       17 |  717 | `			VmExportQuoted(pC->pOut,sUnmName.zString,(int)sUnmName.nByte);` |
|        9 |  718 | `		}else{` |
|     1072 |  719 | `			VmExportQuoted(pC->pOut,z,n);` |
|        - |  720 | `		}` |
|      546 |  721 | `	}else{` |
|     4406 |  722 | `		SyBlobFormat(pC->pOut,"%qd",ph7_value_to_int64(pKey));` |
|        - |  723 | `	}` |
|     5489 |  724 | `	VmExportEntryValue(pC->pOut,pValue,pC->nIndent,pC->depth);` |
|     5489 |  725 | `	return PH7_OK;` |
|        5 |  726 | `}` |
|    32178 |  727 | `static void VmExportValue(SyBlob *pOut, ph7_value *pVal, int nIndent, int depth)` |
|        5 |  728 | `{` |
|    32183 |  729 | `	if( depth > 4096 ){ return; } /* backstop for pathological finite nesting */` |
|    32183 |  730 | `	if( ph7_value_is_null(pVal) ){` |
|     1183 |  731 | `		SyBlobAppend(pOut,"NULL",4);` |
|    31594 |  732 | `	}else if( ph7_value_is_bool(pVal) ){` |
|     6929 |  733 | `		if( ph7_value_to_bool(pVal) ){ SyBlobAppend(pOut,"true",4); }` |
|     3485 |  734 | `		else { SyBlobAppend(pOut,"false",5); }` |
|    27484 |  735 | `	}else if( ph7_value_is_float(pVal) ){` |
|        - |  736 | `		/* float before int: ph7_value_is_int is lenient (true for integer reals). */` |
|     1346 |  737 | `		sxu32 before = SyBlobLength(pOut), i, after;` |
|        - |  738 | `		const char *z;` |
|     1346 |  739 | `		int plain = 1;` |
|     1346 |  740 | `		PH7_AppendShortestReal(pOut,ph7_value_to_double(pVal));` |
|     1346 |  741 | `		z = (const char *)SyBlobData(pOut);` |
|     1346 |  742 | `		after = SyBlobLength(pOut);` |
|     3126 |  743 | `		for( i = before; i < after; i++ ){` |
|     2646 |  744 | `			if( !((z[i]>='0'&&z[i]<='9')\|\|z[i]=='-') ){ plain = 0; break; }` |
|      894 |  745 | `		}` |
|     1346 |  746 | `		if( plain ){ SyBlobAppend(pOut,".0",2); } /* integer-form floats render 1.0/100.0 */` |
|    23410 |  747 | `	}else if( ph7_value_is_int(pVal) ){` |
|     9553 |  748 | `		sxi64 iVal = ph7_value_to_int64(pVal);` |
|     9553 |  749 | `		if( iVal == SMALLEST_INT64 ){` |
|        - |  750 | `			/* php renders LONG_MIN as an expression: the positive literal` |
|        - |  751 | `			 * 9223372036854775808 would not be representable when eval'd. */` |
|       19 |  752 | `			SyBlobAppend(pOut,"-9223372036854775807-1",sizeof("-9223372036854775807-1")-1);` |
|       10 |  753 | `		}else{` |
|     9535 |  754 | `			SyBlobFormat(pOut,"%qd",iVal);` |
|        5 |  755 | `		}` |
|    17953 |  756 | `	}else if( ph7_value_is_string(pVal) ){` |
|        - |  757 | `		int n;` |
|    10467 |  758 | `		const char *z = ph7_value_to_string(pVal,&n);` |
|    10467 |  759 | `		VmExportQuoted(pOut,z,n);` |
|     7862 |  760 | `	}else if( ph7_value_is_array(pVal) ){` |
|     2547 |  761 | `		ph7_hashmap *pMap = (ph7_hashmap *)pVal->x.pOther;` |
|     2547 |  762 | `		if( pMap->iFlags & HASHMAP_DUMPING ){` |
|      ! 0 |  763 | `			SyBlobAppend(pOut,"NULL",4); /* circular reference -> NULL, like PHP */` |
|      ! 0 |  764 | `		}else{` |
|        - |  765 | `			VmExportCtx ctx;` |
|     2547 |  766 | `			ctx.bPresented = 0;` |
|     2547 |  767 | `			ctx.nKeyExtra = 0;` |
|     2547 |  768 | `			pMap->iFlags \|= HASHMAP_DUMPING;` |
|     2547 |  769 | `			SyBlobAppend(pOut,"array (\n",8);` |
|     2547 |  770 | `			ctx.pOut = pOut; ctx.nIndent = nIndent; ctx.depth = depth;` |
|     2547 |  771 | `			ph7_array_walk(pVal,VmExportArrayWalk,&ctx);` |
|     2547 |  772 | `			VmExportIndent(pOut,nIndent);` |
|     2547 |  773 | `			SyBlobAppend(pOut,")",1);` |
|     2547 |  774 | `			pMap->iFlags &= ~HASHMAP_DUMPING;` |
|        5 |  775 | `		}` |
|     1437 |  776 | `	}else if( ph7_value_is_object(pVal) ){` |
|      187 |  777 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|      187 |  778 | `		if( pThis->pClass->iFlags & PH7_CLASS_ENUM ){` |
|        - |  779 | ``			/* php 8.1: an enum case exports as `\S::A` */`` |
|       11 |  780 | `			ph7_value *pName = PH7_EnumCaseNameValue(pThis);` |
|       11 |  781 | `			SyBlobAppend(pOut,"\\",1);` |
|       11 |  782 | `			SyBlobAppend(pOut,pThis->pClass->sName.zString,pThis->pClass->sName.nByte);` |
|       11 |  783 | `			SyBlobAppend(pOut,"::",2);` |
|       11 |  784 | `			if( pName && SyBlobLength(&pName->sBlob) > 0 ){` |
|       11 |  785 | `				SyBlobAppend(pOut,SyBlobData(&pName->sBlob),SyBlobLength(&pName->sBlob));` |
|        6 |  786 | `			}` |
|      182 |  787 | `		}else if( pThis->iFlags & VM_INSTANCE_DUMPING ){` |
|      ! 0 |  788 | `			SyBlobAppend(pOut,"NULL",4); /* circular reference -> NULL, like PHP */` |
|      ! 0 |  789 | `		}else{` |
|      177 |  790 | `			SyString *pClassName = &pThis->pClass->sName;` |
|        - |  791 | `			SyHashEntry *pEntry;` |
|        - |  792 | `			SySet sNames;` |
|        - |  793 | `			SyString *aName;` |
|        - |  794 | `			sxu32 iName,nName;` |
|        - |  795 | ``			/* php exports a plain stdClass as a CAST — `(object) array(...)` — and`` |
|        - |  796 | `			 * every other class through __set_state(); a SUBCLASS of stdClass takes` |
|        - |  797 | `			 * the __set_state form, so this is the exact class and not an` |
|        - |  798 | `			 * inheritance test. The two forms differ by one closing paren. */` |
|      177 |  799 | `			int bStdObj = pThis->pClass == pThis->pVm->pStdClass;` |
|      177 |  800 | `			pThis->iFlags \|= VM_INSTANCE_DUMPING;` |
|      177 |  801 | `			if( bStdObj ){` |
|        3 |  802 | `				SyBlobAppend(pOut,"(object) array(\n",sizeof("(object) array(\n")-1);` |
|        2 |  803 | `			}else{` |
|      175 |  804 | `				SyBlobAppend(pOut,"\\",1);` |
|      175 |  805 | `				SyBlobAppend(pOut,pClassName->zString,pClassName->nByte);` |
|      175 |  806 | `				SyBlobAppend(pOut,"::__set_state(array(\n",21);` |
|        - |  807 | `			}` |
|        - |  808 | `			{` |
|        - |  809 | `				/* A native class's PRESENTATION (php's get_properties): var_export` |
|        - |  810 | `				 * shows a DateTime as date/timezone_type/timezone, the same shape` |
|        - |  811 | `				 * the (array) cast produces and NOT the hidden engine slots. A` |
|        - |  812 | `				 * debug-only hook (WeakReference) fills nothing, which is php's` |
|        - |  813 | `				 * empty export. */` |
|        - |  814 | `				ph7_value sPresent;` |
|      177 |  815 | `				ph7_hashmap *pPresent = PH7_NewHashmap(pThis->pVm,0,0);` |
|      177 |  816 | `				PH7_MemObjInit(pThis->pVm,&sPresent);` |
|      177 |  817 | `				if( pPresent ){` |
|      177 |  818 | `					sPresent.x.pOther = pPresent;` |
|      177 |  819 | `					MemObjSetType(&sPresent,MEMOBJ_HASHMAP);` |
|      177 |  820 | `					if( PH7_ClassInstancePresent(pThis,&sPresent,0) ){` |
|        - |  821 | `						/* Same line shape the attribute loop below produces: an` |
|        - |  822 | `						 * object body's entries sit one deeper than an array's. */` |
|        - |  823 | `						VmExportCtx sCtx;` |
|      105 |  824 | `						sCtx.pOut = pOut;` |
|      105 |  825 | `						sCtx.nIndent = nIndent;` |
|      105 |  826 | `						sCtx.depth = depth;` |
|      105 |  827 | `						sCtx.bPresented = 1;` |
|      105 |  828 | `						sCtx.nKeyExtra = 1;` |
|      105 |  829 | `						ph7_array_walk(&sPresent,VmExportArrayWalk,&sCtx);` |
|      105 |  830 | `						PH7_MemObjRelease(&sPresent);` |
|      105 |  831 | `						VmExportIndent(pOut,nIndent);` |
|      105 |  832 | `						SyBlobAppend(pOut,bStdObj ? ")" : "))",bStdObj ? 1 : 2);` |
|      105 |  833 | `						pThis->iFlags &= ~VM_INSTANCE_DUMPING;` |
|      105 |  834 | `						return;` |
|        - |  835 | `					}` |
|       75 |  836 | `					PH7_MemObjRelease(&sPresent);` |
|       35 |  837 | `				}` |
|        - |  838 | `			}` |
|        - |  839 | `			/* SNAPSHOT the attribute names first: a PHP 8.4 get hook dispatched` |
|        - |  840 | `			 * mid-walk may re-enter an hAttr walk on this instance (the hash has` |
|        - |  841 | `			 * a single embedded loop cursor) or unset()/create properties; names` |
|        - |  842 | `			 * point into class-owned attr storage and each is re-looked-up. */` |
|       75 |  843 | `			SySetInit(&sNames,&pThis->pVm->sAllocator,sizeof(SyString));` |
|       75 |  844 | `			SyHashResetLoopCursor(&pThis->hAttr);` |
|      251 |  845 | `			while( (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|      181 |  846 | `				VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|      181 |  847 | `				if( PH7_ATTR_UNPRESENTED(pVmAttr) ){ continue; }` |
|      125 |  848 | `				if( PH7_ClassAttrUninitializedForRead(pVmAttr) ){` |
|       22 |  849 | `					continue; /* typed, never written: not there yet (php) */` |
|        - |  850 | `				}` |
|      100 |  851 | `				if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_VIRTUAL))` |
|       55 |  852 | `				 == PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|      ! 0 |  853 | `					continue; /* virtual set-only property: no value to export (php) */` |
|        - |  854 | `				}` |
|        - |  855 | `				{` |
|        - |  856 | `					/* The STORAGE key, which is php's MANGLED name for an inherited` |
|        - |  857 | `					 * private -- snapshotting the plain one re-looked-up the object's` |
|        - |  858 | `					 * OWN property of that name and exported its value twice. */` |
|        - |  859 | `					SyString sKey;` |
|      105 |  860 | `					SyStringInitFromBuf(&sKey,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|      105 |  861 | `					SySetPut(&sNames,(const void *)&sKey);` |
|        - |  862 | `				}` |
|        5 |  863 | `			}` |
|       75 |  864 | `			aName = (SyString *)SySetBasePtr(&sNames);` |
|       75 |  865 | `			nName = SySetUsed(&sNames);` |
|      175 |  866 | `			for( iName = 0 ; iName < nName ; ++iName ){` |
|      105 |  867 | `				SyString *pAName = &aName[iName];` |
|        - |  868 | `				VmClassAttr *pVmAttr;` |
|        - |  869 | `				ph7_value *pAttrVal;` |
|      105 |  870 | `				pEntry = PH7_ClassInstanceAttrEntry(pThis,pAName->zString,pAName->nByte);` |
|      105 |  871 | `				if( pEntry == 0 ){ continue; } /* unset by an earlier hook */` |
|      105 |  872 | `				pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|      105 |  873 | `				VmExportIndent(pOut,nIndent+3); /* object property lines sit one deeper than arrays */` |
|      107 |  874 | `				if( pAName->nByte > 0 && pAName->zString[0] == 0 ){` |
|        - |  875 | `					/* A MANGLED key -- an inherited private's storage name, or the` |
|        - |  876 | `					 * __PHP_Incomplete_Class carrier's raw payload key: php's` |
|        - |  877 | `					 * var_export prints the PLAIN name ('bp' => 1). */` |
|        - |  878 | `					SyString sUnmCls, sUnmName;` |
|        5 |  879 | `					SyStringInitFromBuf(&sUnmName,pAName->zString,pAName->nByte);` |
|        5 |  880 | `					PH7_UnmangleAttrName(pAName->zString,pAName->nByte,&sUnmCls,&sUnmName);` |
|        5 |  881 | `					VmExportQuoted(pOut,sUnmName.zString,(int)sUnmName.nByte);` |
|        3 |  882 | `				}else{` |
|      101 |  883 | `					VmExportQuoted(pOut,pAName->zString,(int)pAName->nByte);` |
|        - |  884 | `				}` |
|        - |  885 | `				/* PHP 8.4 property hooks: var_export() reads through the get hook` |
|        - |  886 | `				 * (every visibility — php exports private hooked values too). A` |
|        - |  887 | `				 * throwing hook parks on the boundary rail; the helper's boundary` |
|        - |  888 | `				 * gate keeps LATER hooks from running (the raw values the tail of` |
|        - |  889 | `				 * the export falls back to are discarded when the throw routes). */` |
|        - |  890 | `				{` |
|        - |  891 | `					ph7_value sHookVal;` |
|        - |  892 | `					sxi32 rcHk;` |
|      105 |  893 | `					PH7_MemObjInit(pThis->pVm,&sHookVal);` |
|      105 |  894 | `					rcHk = PH7_VmHookGetAttrValue(pThis,pVmAttr,&sHookVal);` |
|      105 |  895 | `					if( rcHk == SXRET_OK ){` |
|       16 |  896 | `						VmExportEntryValue(pOut,&sHookVal,nIndent,depth);` |
|       16 |  897 | `						PH7_MemObjRelease(&sHookVal);` |
|       20 |  898 | `						continue;` |
|        - |  899 | `					}` |
|       91 |  900 | `					PH7_MemObjRelease(&sHookVal);` |
|       91 |  901 | `					if( rcHk != SXERR_NOTFOUND ){` |
|        - |  902 | `						/* the hook threw (parked on the boundary rail): NULL` |
|        - |  903 | `						 * placeholder keeps the output well-formed */` |
|       10 |  904 | `						SyBlobAppend(pOut," => NULL,\n",10);` |
|       10 |  905 | `						continue;` |
|        - |  906 | `					}` |
|        - |  907 | `				}` |
|       83 |  908 | `				pAttrVal = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       83 |  909 | `				if( pAttrVal ){ VmExportEntryValue(pOut,pAttrVal,nIndent,depth); }` |
|      ! 0 |  910 | `				else { SyBlobAppend(pOut," => NULL,\n",10); }` |
|       44 |  911 | `			}` |
|       75 |  912 | `			SySetRelease(&sNames);` |
|       75 |  913 | `			VmExportIndent(pOut,nIndent);` |
|       75 |  914 | `			SyBlobAppend(pOut,bStdObj ? ")" : "))",bStdObj ? 1 : 2);` |
|       75 |  915 | `			pThis->iFlags &= ~VM_INSTANCE_DUMPING;` |
|        - |  916 | `		}` |
|       45 |  917 | `	}else{` |
|        - |  918 | `		/* resource / other -> PHP emits NULL (with a warning we omit) */` |
|      ! 0 |  919 | `		SyBlobAppend(pOut,"NULL",4);` |
|        - |  920 | `	}` |
|    15904 |  921 | `}` |
|        - |  922 | `/*` |
|        - |  923 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - |  924 | ` *  PHP-exact evaluable representation (see VmExportValue).` |
|        - |  925 | ` */` |
|    26606 |  926 | `PH7_PRIVATE int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  927 | `{` |
|    26611 |  928 | `	int ret_string = 0;` |
|        - |  929 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|    26611 |  930 | `	if( nArg < 1 ){` |
|        - |  931 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  932 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  933 | `		return SXRET_OK;` |
|        - |  934 | `	}` |
|    26611 |  935 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|    26611 |  936 | `	if ( nArg > 1 ){` |
|        - |  937 | `		/* Where to redirect output */` |
|    25547 |  938 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|    12636 |  939 | `	}` |
|        - |  940 | `	/* Generate the PHP-exact evaluable representation */` |
|    26611 |  941 | `	VmExportValue(&sDump,apArg[0],0,0);` |
|    26611 |  942 | `	if( PH7_CALLBACK_UNWOUND(pCtx->pVm->nBoundaryRc) ){` |
|        - |  943 | ``		/* A php 8.4 `get` hook threw (or exited) part-way through: php's var_export`` |
|        - |  944 | `		 * builds its string before it prints anything, so a throw from inside it` |
|        - |  945 | ``		 * reaches the caller with NOTHING written. The `$return = true` form was`` |
|        - |  946 | `		 * already right — the parked throw discards the RESULT — but the printing` |
|        - |  947 | `		 * form had already handed the half-built text to the output layer, so a` |
|        - |  948 | `		 * caught throw was followed by an export naming properties as NULL. */` |
|       10 |  949 | `		SyBlobRelease(&sDump);` |
|       10 |  950 | `		ph7_result_null(pCtx);` |
|       10 |  951 | `		return SXRET_OK;` |
|        - |  952 | `	}` |
|    26603 |  953 | `	if( !ret_string ){` |
|        - |  954 | `		/* Output dump */` |
|     1063 |  955 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  956 | `		/* Return NULL */` |
|     1063 |  957 | `		ph7_result_null(pCtx);` |
|      534 |  958 | `	}else{` |
|        - |  959 | `		/* Generated dump as return value */` |
|    25545 |  960 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  961 | `	}` |
|        - |  962 | `	/* Release the working buffer */` |
|    26603 |  963 | `	SyBlobRelease(&sDump);` |
|    26603 |  964 | `	return SXRET_OK;` |
|    13173 |  965 | `}` |
|        - |  966 |  |
