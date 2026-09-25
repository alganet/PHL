# src/ph7/vm_builtin_var.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 483/521 lines (92.71%)

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
|   138070 |   23 | `PH7_PRIVATE int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |   24 | `{` |
|        - |   25 | `	ph7_value *pObj;` |
|   138075 |   26 | `	int res = 0;` |
|        - |   27 | `	int i;` |
|   138075 |   28 | `	if( nArg < 1 ){` |
|        - |   29 | `		/* Missing arguments,return false */` |
|      ! 0 |   30 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 |   31 | `		return SXRET_OK;` |
|        - |   32 | `	}` |
|        - |   33 | `	/* Iterate over available arguments */` |
|   170997 |   34 | `	for( i = 0 ; i < nArg ; ++i ){` |
|   138087 |   35 | `		pObj = apArg[i];` |
|   138087 |   36 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|        - |   37 | `			/* Skip the "expecting a variable" warning for MEMOBJ_BOOL —` |
|        - |   38 | `			 * synthesized by LOAD_IDX iP2=4 (ArrayAccess::offsetExists) and` |
|        - |   39 | `			 * by anyone passing a bool literal (rare, harmless). */` |
|   105183 |   40 | `			if( (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|        - |   41 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 |   42 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 |   43 | `			}` |
|    52589 |   44 | `		}` |
|   138087 |   45 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|   138087 |   46 | `		if( !res ){` |
|        - |   47 | `			/* Variable not set,return FALSE */` |
|   105165 |   48 | `			ph7_result_bool(pCtx,0);` |
|   105165 |   49 | `			return SXRET_OK;` |
|        - |   50 | `		}` |
|    16466 |   51 | `	}` |
|        - |   52 | `	/* All given variable are set,return TRUE */` |
|    32915 |   53 | `	ph7_result_bool(pCtx,1);` |
|    32915 |   54 | `	return SXRET_OK;` |
|    69040 |   55 | `}` |
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
|     7874 |   82 | `PH7_PRIVATE sxi32 VmUnsetVarByNameEx(ph7_vm *pVm,VmFrame *pFrame,const char *zName,sxu32 nByte,` |
|        - |   83 | `	int bNameGuard)` |
|        5 |   84 | `{` |
|        - |   85 | `	SyHashEntry *pEntry;` |
|        - |   86 | `	VmRefObj *pRef;` |
|        - |   87 | `	sxu32 nIdx;` |
|        - |   88 | `	/* php 8.1 forbids unset($GLOBALS) outright. Checked by NAME: the superglobal is not an` |
|        - |   89 | `	 * ordinary hVar binding, so the slot-index test below never sees it. */` |
|     7879 |   90 | `	if( bNameGuard && nByte == sizeof("GLOBALS")-1 && SyMemcmp(zName,"GLOBALS",nByte) == 0 ){` |
|        3 |   91 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |   92 | `			"$GLOBALS can only be modified using the $GLOBALS[$name] = $value syntax");` |
|        3 |   93 | `		pVm->iExitStatus = 255;` |
|        3 |   94 | `		pVm->bHaltRequested = 1;` |
|        3 |   95 | `		return PH7_ABORT;` |
|        - |   96 | `	}` |
|     7877 |   97 | `	pEntry = SyHashGet(&pFrame->hVar,(const void *)zName,nByte);` |
|     7877 |   98 | `	if( pEntry == 0 ){` |
|        - |   99 | `		/* No such variable: unset() is a no-op on an undefined name, as in php */` |
|     1401 |  100 | `		return SXRET_OK;` |
|        - |  101 | `	}` |
|     6481 |  102 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|     6481 |  103 | `	if( nIdx == pVm->nGlobalIdx ){` |
|      ! 0 |  104 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  105 | `			"$GLOBALS can only be modified using the $GLOBALS[$name] = $value syntax");` |
|      ! 0 |  106 | `		pVm->iExitStatus = 255;` |
|      ! 0 |  107 | `		pVm->bHaltRequested = 1;` |
|      ! 0 |  108 | `		return PH7_ABORT;` |
|        - |  109 | `	}` |
|     6481 |  110 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|        - |  111 | `	/*` |
|        - |  112 | `	 * A GLOBAL variable is ALSO an entry of the $GLOBALS array, and that node points at the` |
|        - |  113 | `	 * same slot. php's unset($x) in global scope drops $GLOBALS['x'] too, so unlink the node` |
|        - |  114 | `	 * — otherwise it stays a live holder and the value is never released ($o = new D;` |
|        - |  115 | `	 * unset($o); stopped running the destructor). Clear the reference table's row for the` |
|        - |  116 | `	 * node BEFORE unlinking it: unlinking frees the node, and the holder count below would` |
|        - |  117 | `	 * otherwise dereference freed memory.` |
|        - |  118 | `	 */` |
|     6481 |  119 | `	if( pFrame->pParent == 0 ){` |
|     6461 |  120 | `		ph7_value *pGlobals = (ph7_value *)SySetAt(&pVm->aMemObj,pVm->nGlobalIdx);` |
|     6461 |  121 | `		if( pGlobals && (pGlobals->iFlags & MEMOBJ_HASHMAP) ){` |
|     6461 |  122 | `			ph7_hashmap_node *pNode = 0;` |
|        - |  123 | `			ph7_value sKey;` |
|        - |  124 | `			SyString sName;` |
|     6461 |  125 | `			PH7_MemObjInit(&(*pVm),&sKey);` |
|     6461 |  126 | `			SyStringInitFromBuf(&sName,zName,nByte);` |
|     6461 |  127 | `			PH7_MemObjInitFromString(&(*pVm),&sKey,&sName);` |
|     6456 |  128 | `			if( SXRET_OK == PH7_HashmapLookup((ph7_hashmap *)pGlobals->x.pOther,&sKey,&pNode)` |
|     6456 |  129 | `			 && pNode && pNode->nValIdx == nIdx ){` |
|     6451 |  130 | `				if( pRef ){` |
|     6451 |  131 | `					ph7_hashmap_node **apN = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|        - |  132 | `					sxu32 k;` |
|    13129 |  133 | `					for( k = 0 ; k < SySetUsed(&pRef->aArrEntries) ; ++k ){` |
|     6683 |  134 | `						if( apN[k] == pNode ){` |
|     6451 |  135 | `							apN[k] = 0;` |
|     3223 |  136 | `						}` |
|     3344 |  137 | `					}` |
|     3223 |  138 | `				}` |
|     6451 |  139 | `				PH7_HashmapUnlinkNode(pNode,FALSE);` |
|     3223 |  140 | `			}` |
|     6461 |  141 | `			PH7_MemObjRelease(&sKey);` |
|     3228 |  142 | `		}` |
|     3228 |  143 | `	}` |
|        - |  144 |  |
|        - |  145 | `	/* The frame's own "release this reference at exit" row for the binding about to go:` |
|        - |  146 | `	 * the entry is freed below, so the row would dangle (and a foreach value variable` |
|        - |  147 | `	 * unset once per loop filed one row per loop, which nothing consumed until the` |
|        - |  148 | `	 * function returned). */` |
|     6481 |  149 | `	VmDropFrameRefEntry(&(*pVm),nIdx,pEntry);` |
|     6481 |  150 | `	if( pRef == 0 ){` |
|        - |  151 | `		/* Unaliased variable: nobody else holds the slot, so the old path is right */` |
|      ! 0 |  152 | `		SyHashDeleteEntry2(pEntry);` |
|      ! 0 |  153 | `		PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|        - |  154 | `		/* The slot is back in the free pool; drop its stale local-teardown entry so a` |
|        - |  155 | `		 * later reuse of the index (e.g. an object property) is not double-freed when` |
|        - |  156 | `		 * this frame exits. */` |
|      ! 0 |  157 | `		VmDropFrameLocalSlot(&(*pVm),nIdx);` |
|      ! 0 |  158 | `		return SXRET_OK;` |
|        - |  159 | `	}` |
|        - |  160 | `	{` |
|     6481 |  161 | `		SyHashEntry **apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - |  162 | `		sxu32 n;` |
|        - |  163 | `		/* Forget THIS name in the slot's reference record (leave the others alone) */` |
|    13089 |  164 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; ++n ){` |
|     6613 |  165 | `			if( apEntry[n] == pEntry ){` |
|     6469 |  166 | `				apEntry[n] = 0;` |
|     3232 |  167 | `			}` |
|     3309 |  168 | `		}` |
|     6481 |  169 | `		SyHashDeleteEntry2(pEntry);` |
|        - |  170 | `		/* The value goes with the LAST holder and not before — the one rule, counted in` |
|        - |  171 | `		 * one place: other names, array nodes that still point here, and a PIN (a static's` |
|        - |  172 | ``		 * storage, a `use (&$x)` capture, a reference-bound property), which is a holder`` |
|        - |  173 | `		 * this table cannot name. Unsetting the name of a static used to release the` |
|        - |  174 | `		 * static's value, so the next call started over from the initializer. */` |
|     6481 |  175 | `		PH7_VmReleaseUnheldSlot(&(*pVm),nIdx);` |
|        - |  176 | `	}` |
|     6481 |  177 | `	return SXRET_OK;` |
|     3942 |  178 | `}` |
|     7716 |  179 | `PH7_PRIVATE sxi32 VmUnsetVarByName(ph7_vm *pVm,VmFrame *pFrame,const char *zName,sxu32 nByte)` |
|        5 |  180 | `{` |
|     7721 |  181 | `	return VmUnsetVarByNameEx(&(*pVm),pFrame,zName,nByte,TRUE);` |
|        5 |  182 | `}` |
| 18335346 |  183 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        5 |  184 | `{` |
|        - |  185 | `	ph7_value *pObj;` |
|        - |  186 | `	VmRefObj *pRef;` |
| 18335351 |  187 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
| 18335351 |  188 | `	if( pObj ){` |
|        - |  189 | `		/* Release the object */` |
| 18335351 |  190 | `		PH7_MemObjRelease(pObj);` |
|  9168991 |  191 | `	}` |
|        - |  192 | `	/* Remove old reference links */` |
| 18335351 |  193 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
| 18335351 |  194 | `	if( pRef ){` |
| 18335307 |  195 | `		sxi32 iFlags = pRef->iFlags;` |
|        - |  196 | `		/* Unlink from the reference table */` |
| 18335307 |  197 | `		VmRefObjUnlink(&(*pVm),pRef);` |
| 18335307 |  198 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - |  199 | `			VmSlot sFree;` |
|        - |  200 | `			/* Restore to the free list */` |
| 18335307 |  201 | `			sFree.nIdx = nObjIdx;` |
| 18335307 |  202 | `			sFree.pUserData = 0;` |
| 18335307 |  203 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  9168969 |  204 | `		}` |
|  9168969 |  205 | `	}` |
| 18335351 |  206 | `	return SXRET_OK;` |
|        5 |  207 | `}` |
|        - |  208 | `/*` |
|        - |  209 | ` * void unset($var,...)` |
|        - |  210 | ` *   Unset one or more given variable.` |
|        - |  211 | ` * Parameters` |
|        - |  212 | ` *  One or more variable to unset.` |
|        - |  213 | ` * Return` |
|        - |  214 | ` *  Nothing.` |
|        - |  215 | ` */` |
|     1274 |  216 | `PH7_PRIVATE int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  217 | `{` |
|        - |  218 | `	ph7_value *pObj;` |
|        - |  219 | `	ph7_vm *pVm;` |
|        - |  220 | `	int i;` |
|        - |  221 | `	/* Point to the target VM */` |
|     1279 |  222 | `	pVm = pCtx->pVm;` |
|        - |  223 | `	/* Iterate and unset */` |
|     2553 |  224 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     1279 |  225 | `		pObj = apArg[i];` |
|     1279 |  226 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|     1277 |  227 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  228 | `				/* Throw an error */` |
|      ! 0 |  229 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 |  230 | `			}` |
|      641 |  231 | `		}else{` |
|        3 |  232 | `			sxu32 nIdx = pObj->nIdx;` |
|        3 |  233 | `			if( nIdx == pVm->nGlobalIdx ){` |
|        - |  234 | `				/* php 8.1: unset($GLOBALS) is forbidden — the same fatal as` |
|        - |  235 | `				 * re-assigning it (compile-time in php, raised here). */` |
|      ! 0 |  236 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  237 | `					"$GLOBALS can only be modified using the $GLOBALS[$name] = $value syntax");` |
|      ! 0 |  238 | `				pVm->iExitStatus = 255;` |
|      ! 0 |  239 | `				pVm->bHaltRequested = 1;` |
|      ! 0 |  240 | `				return PH7_ABORT;` |
|        - |  241 | `			}` |
|        3 |  242 | `			PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|        - |  243 | `			/* Drop the stale local-teardown entry for the freed slot (see` |
|        - |  244 | `			 * VmDropFrameLocalSlot) so a later index reuse is not double-freed. */` |
|        3 |  245 | `			VmDropFrameLocalSlot(&(*pVm),nIdx);` |
|        - |  246 | `		}` |
|      642 |  247 | `	}` |
|     1279 |  248 | `	return SXRET_OK;` |
|      642 |  249 | `}` |
|        - |  250 | `/*` |
|        - |  251 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - |  252 | ` */` |
|     7218 |  253 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        4 |  254 | `{` |
|     7222 |  255 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|     7222 |  256 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  257 | `	ph7_value *pObj;` |
|        - |  258 | `	sxu32 nIdx;` |
|        - |  259 | `	/* php excludes $this from get_defined_vars() (it is bound implicitly, not a` |
|        - |  260 | `	 * declared local). PHL keeps it in the frame's hVar, so skip it here. */` |
|     7218 |  261 | `	if( pEntry->nKeyLen == sizeof("this")-1` |
|     4207 |  262 | `	 && SyMemcmp(pEntry->pKey,"this",sizeof("this")-1) == 0 ){` |
|        6 |  263 | `		return SXRET_OK;` |
|        - |  264 | `	}` |
|        - |  265 | `	/* Engine temporaries (a foreach destructuring/target slot) are not variables the` |
|        - |  266 | `	 * program declared — php compiles those into slots with no name at all. */` |
|     7218 |  267 | `	if( PH7_VmVarNameIsInternal((const char *)pEntry->pKey,pEntry->nKeyLen) ){` |
|      453 |  268 | `		return SXRET_OK;` |
|        - |  269 | `	}` |
|        - |  270 | `	/* Extract the memory object */` |
|     6766 |  271 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|     6766 |  272 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     6766 |  273 | `	if( pObj ){` |
|     6766 |  274 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|     6754 |  275 | `			if( pEntry->nKeyLen > 0 ){` |
|        - |  276 | `				SyString sName;` |
|        - |  277 | `				ph7_value sKey;` |
|        - |  278 | `				/* Perform the insertion (pObj may point into pVm->aMemObj; the` |
|        - |  279 | `				 * inserter snapshots the source before reserving, so the pool may` |
|        - |  280 | `				 * safely move underneath it — see HashmapInsertIntKey/BlobKey). */` |
|     6754 |  281 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|     6754 |  282 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|     6754 |  283 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|     6754 |  284 | `				PH7_MemObjRelease(&sKey);` |
|     3375 |  285 | `			}` |
|     3375 |  286 | `		}` |
|     3381 |  287 | `	}` |
|     6766 |  288 | `	return SXRET_OK;` |
|     3613 |  289 | `}` |
|        - |  290 | `/*` |
|        - |  291 | ` * array get_defined_vars(void)` |
|        - |  292 | ` *  Returns an array of all defined variables.` |
|        - |  293 | ` * Parameter` |
|        - |  294 | ` *  None` |
|        - |  295 | ` * Return` |
|        - |  296 | ` *  An array with all the variables defined in the current scope.` |
|        - |  297 | ` */` |
|       70 |  298 | `PH7_PRIVATE int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        4 |  299 | `{` |
|       74 |  300 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  301 | `	ph7_value *pArray;` |
|        - |  302 | `	VmFrame *pFrame;` |
|        - |  303 | `	/* Create a new array */` |
|       74 |  304 | `	pArray = ph7_context_new_array(pCtx);` |
|       74 |  305 | ` 	if( pArray == 0 ){` |
|      ! 0 |  306 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  307 | `		SXUNUSED(apArg);` |
|        - |  308 | `		/* Return NULL */` |
|      ! 0 |  309 | `		ph7_result_null(pCtx);` |
|      ! 0 |  310 | `		return SXRET_OK;` |
|        - |  311 | `	}` |
|        - |  312 | `	/* A try/catch block pushes an EXCEPTION frame that holds no variables of its` |
|        - |  313 | `	 * own, so the enclosing function (or global) frame is the one php reports.` |
|        - |  314 | `	 * Reading pVm->pFrame raw answered [] for every call inside a try/catch —` |
|        - |  315 | `	 * including the superglobal test below, which saw a non-NULL pParent and` |
|        - |  316 | `	 * dropped $argv/$_SERVER at global scope. Every other frame-lookup site` |
|        - |  317 | `	 * (VmExtractMemObj, compact(), func_get_args()) already skips them. */` |
|       74 |  318 | `	pFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|        - |  319 | `	/* Superglobals appear in get_defined_vars() ONLY at the global scope (php).` |
|        - |  320 | `	 * Inside a function the result is the local symbol table alone — a leak of` |
|        - |  321 | `	 * $argv/$_SERVER/... into every function's scope breaks callers that treat the` |
|        - |  322 | `	 * keys as real locals (e.g. PHPUnit's doubled-method template reflects each` |
|        - |  323 | `	 * get_defined_vars() name as a ReflectionParameter). */` |
|       74 |  324 | `	if( pFrame->pParent == 0 ){` |
|       14 |  325 | `		SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        6 |  326 | `	}` |
|        - |  327 | `	/* Then variables defined in the current frame, in DECLARATION order.` |
|        - |  328 | `	 * The frame table is head-pushed (SyHashInsert), so its forward order is` |
|        - |  329 | `	 * reverse-insertion; walk it backward to match php, which returns locals in` |
|        - |  330 | `	 * the order they first appeared (a,b,c — a reassignment reuses the slot and` |
|        - |  331 | `	 * keeps its original position). */` |
|       74 |  332 | `	SyHashForEachReverse(&pFrame->hVar,VmHashVarWalker,pArray);` |
|        - |  333 | `	/* Finally,return the created array */` |
|       74 |  334 | `	ph7_result_value(pCtx,pArray);` |
|       74 |  335 | `	return SXRET_OK;` |
|       39 |  336 | `}` |
|        - |  337 | `/*` |
|        - |  338 | ` * string get_debug_type(mixed $value)` |
|        - |  339 | ` *  php 8.0's type name for diagnostics: the SHORT scalar names, and a class` |
|        - |  340 | ` *  name for an object. Distinct from gettype(), which keeps php 4's long` |
|        - |  341 | ` *  spellings ("integer"/"boolean"/"NULL") for compatibility.` |
|        - |  342 | ` *` |
|        - |  343 | ` *  This was a prelude function in the Reflection chunk, where the TypeError` |
|        - |  344 | ` *  messages needed it; it is what php ships natively, and the SPL chunk's` |
|        - |  345 | ` *  messages want it too.` |
|        - |  346 | ` */` |
|       72 |  347 | `PH7_PRIVATE int vm_builtin_get_debug_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  348 | `{` |
|       73 |  349 | `	const char *zType = "null";` |
|       73 |  350 | `	if( nArg > 0 ){` |
|       73 |  351 | `		ph7_value *pVal = apArg[0];` |
|       73 |  352 | `		if( pVal->iFlags & MEMOBJ_OBJ ){` |
|        7 |  353 | `			ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|       10 |  354 | `			ph7_result_string(pCtx,SyStringData(&pThis->pClass->sName),` |
|        6 |  355 | `				(int)SyStringLength(&pThis->pClass->sName));` |
|        7 |  356 | `			return SXRET_OK;` |
|        - |  357 | `		}` |
|       67 |  358 | `		if( pVal->iFlags & MEMOBJ_NULL ){` |
|        5 |  359 | `			zType = "null";` |
|       65 |  360 | `		}else if( pVal->iFlags & MEMOBJ_REAL ){` |
|        - |  361 | `			/* REAL wins over a cached MEMOBJ_INT, as it does for gettype() */` |
|       13 |  362 | `			zType = "float";` |
|       57 |  363 | `		}else if( pVal->iFlags & MEMOBJ_INT ){` |
|       19 |  364 | `			zType = "int";` |
|       42 |  365 | `		}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|        5 |  366 | `			zType = "string";` |
|       31 |  367 | `		}else if( pVal->iFlags & MEMOBJ_BOOL ){` |
|        5 |  368 | `			zType = "bool";` |
|       27 |  369 | `		}else if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|        5 |  370 | `			zType = "array";` |
|       23 |  371 | `		}else if( pVal->iFlags & MEMOBJ_RES ){` |
|        - |  372 | `			/* php names the resource's TYPE here, and this used to answer the` |
|        - |  373 | `			 * bare "resource" gettype() answers — so the function whose whole` |
|        - |  374 | `			 * job is to NAME a value's type had a different answer from php for` |
|        - |  375 | `			 * every open handle, in exactly the diagnostics it exists for.` |
|        - |  376 | `			 * (The note this replaced said the kind was unavailable; it is what` |
|        - |  377 | `			 * get_resource_type() has been answering all along.) */` |
|       21 |  378 | `			if( PH7_VfsResourceIsClosed(pVal->x.pOther) ){` |
|        5 |  379 | `				zType = "resource (closed)";` |
|        3 |  380 | `			}else{` |
|       25 |  381 | `				ph7_result_string_format(pCtx,"resource (%s)",` |
|        8 |  382 | `					PH7_VfsResourceType(pVal->x.pOther));` |
|       17 |  383 | `				return SXRET_OK;` |
|        - |  384 | `			}` |
|        2 |  385 | `		}` |
|       25 |  386 | `	}` |
|       51 |  387 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       51 |  388 | `	return SXRET_OK;` |
|       37 |  389 | `}` |
|        - |  390 | `/*` |
|        - |  391 | ` * bool gettype($var)` |
|        - |  392 | ` *  Get the type of a variable` |
|        - |  393 | ` * Parameters` |
|        - |  394 | ` *   $var` |
|        - |  395 | ` *    The variable being type checked.` |
|        - |  396 | ` * Return` |
|        - |  397 | ` *   String representation of the given variable type.` |
|        - |  398 | ` */` |
|      182 |  399 | `PH7_PRIVATE int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        4 |  400 | `{` |
|        - |  401 | `	/* php's gettype() uses LONG type names ("integer"/"boolean"/"NULL"), distinct` |
|        - |  402 | `	 * from get_debug_type()'s short ones ("int"/"bool"/"null") — never route this` |
|        - |  403 | `	 * through PH7_MemObjTypeDump, which yields the short forms. */` |
|      186 |  404 | `	const char *zType = "unknown type";` |
|      186 |  405 | `	if( nArg > 0 ){` |
|      186 |  406 | `		ph7_value *pVal = apArg[0];` |
|      186 |  407 | `		if( pVal->iFlags & MEMOBJ_NULL ){` |
|        3 |  408 | `			zType = "NULL";` |
|      185 |  409 | `		}else if( pVal->iFlags & MEMOBJ_REAL ){` |
|        - |  410 | `			/* REAL wins over a cached MEMOBJ_INT: 1.0 is "double" (php). */` |
|        9 |  411 | `			zType = "double";` |
|      180 |  412 | `		}else if( pVal->iFlags & MEMOBJ_INT ){` |
|       16 |  413 | `			zType = "integer";` |
|      169 |  414 | `		}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|      115 |  415 | `			zType = "string";` |
|      105 |  416 | `		}else if( pVal->iFlags & MEMOBJ_BOOL ){` |
|        5 |  417 | `			zType = "boolean";` |
|       47 |  418 | `		}else if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|       12 |  419 | `			zType = "array";` |
|       39 |  420 | `		}else if( pVal->iFlags & MEMOBJ_OBJ ){` |
|       15 |  421 | `			zType = "object";` |
|       27 |  422 | `		}else if( pVal->iFlags & MEMOBJ_RES ){` |
|        - |  423 | `			/* php reports an fclose()'d handle as "resource (closed)" */` |
|       20 |  424 | `			zType = PH7_VfsResourceIsClosed(pVal->x.pOther) ? "resource (closed)" : "resource";` |
|        9 |  425 | `		}` |
|       91 |  426 | `	}` |
|        - |  427 | `	/* Return the variable type */` |
|      186 |  428 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|      186 |  429 | `	return SXRET_OK;` |
|        4 |  430 | `}` |
|        - |  431 | `/*` |
|        - |  432 | ` * bool settype(mixed &$var, string $type)` |
|        - |  433 | ` *  Convert $var IN PLACE to the type named by $type, mirroring the (type) cast` |
|        - |  434 | ` *  operators exactly (same PH7_MemObjTo* helpers CVT_INT/REAL/STR/BOOL/ARRAY/OBJ` |
|        - |  435 | ` *  use), and write the result back through the by-ref out-param.` |
|        - |  436 | ` * Parameters` |
|        - |  437 | ` *   &$var : the variable to convert (compile.c's GenStateByRefBuiltinMask marks` |
|        - |  438 | ` *           position 0 by-reference so an undefined variable is auto-vivified).` |
|        - |  439 | ` *   $type : "int"/"integer", "float"/"double", "string", "bool"/"boolean",` |
|        - |  440 | ` *           "array", "object", "null" (case-insensitive).` |
|        - |  441 | ` * Return` |
|        - |  442 | ` *   Always true on success; a non-referenceable $var is an Error, an unknown` |
|        - |  443 | ` *   $type (or the un-castable "resource") is a ValueError — php-exact.` |
|        - |  444 | ` */` |
|       40 |  445 | `PH7_PRIVATE int vm_builtin_settype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  446 | `{` |
|        - |  447 | `	const char *zType;` |
|        - |  448 | `	int nLen;` |
|        - |  449 | `	ph7_value *pNew;` |
|       10 |  450 | `	SXUNUSED(nArg); /* arity (exactly 2) enforced by the central arity table */` |
|        - |  451 | `	/* php binds $var by reference at the CALL, and the refusal is the call site's` |
|        - |  452 | `	 * to raise (PH7_VmScreenByRefArgShapes) — only the compiler can tell a literal,` |
|        - |  453 | `	 * which php refuses, from the result of a call, which php accepts with a notice` |
|        - |  454 | ``	 * and converts in place on the temporary. The `nIdx == SXU32_HIGH` test that`` |
|        - |  455 | `	 * used to sit here conflated the two. */` |
|       45 |  456 | `	zType = ph7_value_to_string(apArg[1],&nLen);` |
|        - |  457 | `	/* Validate the target type up-front (php checks $type before touching $var):` |
|        - |  458 | `	 * "resource" is a distinct ValueError, any other unknown name is the generic` |
|        - |  459 | `	 * invalid-type ValueError. */` |
|       45 |  460 | `	if( nLen == 8 && SyStrnicmp(zType,"resource",8) == 0 ){` |
|        3 |  461 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|        - |  462 | `			"Cannot convert to resource type");` |
|        - |  463 | `	}` |
|       44 |  464 | `	if( !(  (nLen == 3 && SyStrnicmp(zType,"int",3) == 0)` |
|       37 |  465 | `	     \|\| (nLen == 7 && SyStrnicmp(zType,"integer",7) == 0)` |
|       51 |  466 | `	     \|\| (nLen == 5 && SyStrnicmp(zType,"float",5) == 0)` |
|       50 |  467 | `	     \|\| (nLen == 6 && SyStrnicmp(zType,"double",6) == 0)` |
|       49 |  468 | `	     \|\| (nLen == 6 && SyStrnicmp(zType,"string",6) == 0)` |
|       34 |  469 | `	     \|\| (nLen == 4 && SyStrnicmp(zType,"bool",4) == 0)` |
|       11 |  470 | `	     \|\| (nLen == 7 && SyStrnicmp(zType,"boolean",7) == 0)` |
|       10 |  471 | `	     \|\| (nLen == 5 && SyStrnicmp(zType,"array",5) == 0)` |
|        8 |  472 | `	     \|\| (nLen == 6 && SyStrnicmp(zType,"object",6) == 0)` |
|        3 |  473 | `	     \|\| (nLen == 4 && SyStrnicmp(zType,"null",4) == 0) ) ){` |
|        3 |  474 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|        - |  475 | `			"settype(): Argument #2 ($type) must be a valid type");` |
|        - |  476 | `	}` |
|        - |  477 | `	/* Convert a COPY of the current value (the To* helpers mutate in place), then` |
|        - |  478 | `	 * store it through the by-ref out-param — settype changes $var's TYPE, so the` |
|        - |  479 | `	 * new value must land in the caller slot (nIdx), not just in shared contents. */` |
|       53 |  480 | `	pNew = ph7_context_new_scalar(pCtx);` |
|       53 |  481 | `	if( pNew == 0 ){` |
|      ! 0 |  482 | `		return PH7_ContextMemoryError(pCtx);` |
|        - |  483 | `	}` |
|       53 |  484 | `	PH7_MemObjStore(apArg[0],pNew);` |
|       48 |  485 | `	if( (nLen == 3 && SyStrnicmp(zType,"int",3) == 0)` |
|       52 |  486 | `	 \|\| (nLen == 7 && SyStrnicmp(zType,"integer",7) == 0) ){` |
|       13 |  487 | `		PH7_MemObjToInteger(pNew);` |
|       13 |  488 | `		MemObjSetType(pNew,MEMOBJ_INT);` |
|       54 |  489 | `	}else if( (nLen == 5 && SyStrnicmp(zType,"float",5) == 0)` |
|       48 |  490 | `	       \|\| (nLen == 6 && SyStrnicmp(zType,"double",6) == 0) ){` |
|        5 |  491 | `		PH7_MemObjToReal(pNew);` |
|        5 |  492 | `		MemObjSetType(pNew,MEMOBJ_REAL);` |
|       55 |  493 | `	}else if( nLen == 6 && SyStrnicmp(zType,"string",6) == 0 ){` |
|        - |  494 | `		/* php emits "Array to string conversion" for settype($arr,'string'), and` |
|        - |  495 | `		 * throws "Object of class X could not be converted to string" for an` |
|        - |  496 | `		 * object with no __toString() (or propagates one that threw). php's` |
|        - |  497 | `		 * convert_to_string() has already blanked the zval by then, so a CAUGHT` |
|        - |  498 | `		 * settype() leaves $var === "" — store that, then propagate instead of` |
|        - |  499 | `		 * answering true. */` |
|       33 |  500 | `		sxi32 rcSv = PH7_MemObjToStringUV(pNew);` |
|       33 |  501 | `		if( rcSv != SXRET_OK ){` |
|       16 |  502 | `			PH7_MemObjRelease(pNew);` |
|       16 |  503 | `			MemObjSetType(pNew,MEMOBJ_STRING);` |
|       16 |  504 | `			PH7_VmStoreArgByRef(pCtx->pVm,apArg[0],pNew);` |
|       16 |  505 | `			pCtx->nThrowRc = rcSv;` |
|       16 |  506 | `			return rcSv;` |
|        - |  507 | `		}` |
|       23 |  508 | `	}else if( (nLen == 4 && SyStrnicmp(zType,"bool",4) == 0)` |
|       12 |  509 | `	       \|\| (nLen == 7 && SyStrnicmp(zType,"boolean",7) == 0) ){` |
|        5 |  510 | `		PH7_MemObjToBool(pNew);` |
|       11 |  511 | `	}else if( nLen == 5 && SyStrnicmp(zType,"array",5) == 0 ){` |
|        5 |  512 | `		PH7_MemObjToHashmap(pNew);` |
|        7 |  513 | `	}else if( nLen == 6 && SyStrnicmp(zType,"object",6) == 0 ){` |
|        3 |  514 | `		PH7_MemObjToObject(pNew);` |
|        2 |  515 | `	}else{` |
|        - |  516 | `		/* "null" — the only validated name left */` |
|        3 |  517 | `		PH7_MemObjToNull(pNew);` |
|        - |  518 | `	}` |
|       47 |  519 | `	PH7_VmStoreArgByRef(pCtx->pVm,apArg[0],pNew);` |
|       47 |  520 | `	ph7_result_bool(pCtx,1);` |
|       47 |  521 | `	return SXRET_OK;` |
|       35 |  522 | `}` |
|        - |  523 | `/*` |
|        - |  524 | ` * string get_resource_type(resource $handle)` |
|        - |  525 | ` *  This function gets the type of the given resource.` |
|        - |  526 | ` * Parameters` |
|        - |  527 | ` *  $handle` |
|        - |  528 | ` *  The evaluated resource handle.` |
|        - |  529 | ` * Return` |
|        - |  530 | ` *  If the given handle is a resource, this function will return a string` |
|        - |  531 | ` *  representing its type. If the type is not identified by this function` |
|        - |  532 | ` *  the return value will be the string Unknown.` |
|        - |  533 | ` *  This function will return FALSE and generate an error if handle` |
|        - |  534 | ` *  is not a resource.` |
|        - |  535 | ` */` |
|       54 |  536 | `PH7_PRIVATE int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  537 | `{` |
|        - |  538 | `	const char *zType;` |
|       56 |  539 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - |  540 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 |  541 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  542 | `		return PH7_OK;` |
|        - |  543 | `	}` |
|        - |  544 | `	/* PHP returns the resource TYPE name (e.g. "stream" for IO handles), not an id. */` |
|       56 |  545 | `	zType = PH7_VfsResourceType(apArg[0]->x.pOther);` |
|       56 |  546 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       56 |  547 | `	return SXRET_OK;` |
|       29 |  548 | `}` |
|        - |  549 | `/*` |
|        - |  550 | ` * int get_resource_id(resource $resource)` |
|        - |  551 | ` *  Returns the integer id of a resource — the same number (int) casts to and` |
|        - |  552 | ` *  "Resource id #N" renders (php 8.0).` |
|        - |  553 | ` */` |
|        4 |  554 | `PH7_PRIVATE int vm_builtin_get_resource_id(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  555 | `{` |
|        6 |  556 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 |  557 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|        - |  558 | `			"get_resource_id(): Argument #1 ($resource) must be of type resource, %s given",` |
|      ! 0 |  559 | `			nArg < 1 ? "none" : ph7_type_name(apArg[0]));` |
|        - |  560 | `	}` |
|        6 |  561 | `	ph7_result_int64(pCtx,(ph7_int64)PH7_VmResourceId(pCtx->pVm,apArg[0]->x.pOther));` |
|        6 |  562 | `	return SXRET_OK;` |
|        4 |  563 | `}` |
|        - |  564 | `/*` |
|        - |  565 | ` * void var_dump(expression,....)` |
|        - |  566 | ` *   var_dump � Dumps information about a variable` |
|        - |  567 | ` * Parameters` |
|        - |  568 | ` *   One or more expression to dump.` |
|        - |  569 | ` * Returns` |
|        - |  570 | ` *  Nothing.` |
|        - |  571 | ` */` |
|     4918 |  572 | `PH7_PRIVATE int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  573 | `{` |
|        - |  574 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - |  575 | `	int i;` |
|     4923 |  576 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - |  577 | `	/* Dump one or more expressions */` |
|    11147 |  578 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     6229 |  579 | `		ph7_value *pObj = apArg[i];` |
|        - |  580 | `		/* Reset the working buffer */` |
|     6229 |  581 | `		SyBlobReset(&sDump);` |
|        - |  582 | `		/* Dump the given expression */` |
|     6229 |  583 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - |  584 | `		/* Output */` |
|     6229 |  585 | `		if( SyBlobLength(&sDump) > 0 ){` |
|     6229 |  586 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|     3112 |  587 | `		}` |
|     3117 |  588 | `	}` |
|        - |  589 | `	/* Release the working buffer */` |
|     4923 |  590 | `	SyBlobRelease(&sDump);` |
|     4923 |  591 | `	return SXRET_OK;` |
|        5 |  592 | `}` |
|        - |  593 | `/*` |
|        - |  594 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - |  595 | ` *   print-r - Prints human-readable information about a variable` |
|        - |  596 | ` * Parameters` |
|        - |  597 | ` *   expression: Expression to dump` |
|        - |  598 | ` *   return : If you would like to capture the output of print_r() use` |
|        - |  599 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - |  600 | ` *            print_r() will return the information rather than print it.` |
|        - |  601 | ` * Return` |
|        - |  602 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - |  603 | ` *  Otherwise, the return value is TRUE.` |
|        - |  604 | ` */` |
|      176 |  605 | `PH7_PRIVATE int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  606 | `{` |
|      181 |  607 | `	int ret_string = 0;` |
|        - |  608 | `	SyBlob sDump;` |
|      181 |  609 | `	if( nArg < 1 ){` |
|        - |  610 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  611 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  612 | `		return SXRET_OK;` |
|        - |  613 | `	}` |
|      181 |  614 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|      181 |  615 | `	if ( nArg > 1 ){` |
|        - |  616 | `		/* Where to redirect output */` |
|       17 |  617 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        7 |  618 | `	}` |
|        - |  619 | `	/* Generate dump */` |
|      181 |  620 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|      181 |  621 | `	if( !ret_string ){` |
|        - |  622 | `		/* Output dump */` |
|      167 |  623 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  624 | `		/* Return true */` |
|      167 |  625 | `		ph7_result_bool(pCtx,1);` |
|       86 |  626 | `	}else{` |
|        - |  627 | `		/* Generated dump as return value */` |
|       17 |  628 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  629 | `	}` |
|        - |  630 | `	/* Release the working buffer */` |
|      181 |  631 | `	SyBlobRelease(&sDump);` |
|      181 |  632 | `	return SXRET_OK;` |
|       93 |  633 | `}` |
|        - |  634 | `/*` |
|        - |  635 | ` * var_export() — PHP-exact evaluable representation of a value.` |
|        - |  636 | ` *` |
|        - |  637 | ` * Distinct from print_r/var_dump (which share PH7_MemObjDump): var_export emits` |
|        - |  638 | ` * parseable PHP — 'single-quoted' strings, true/false/NULL, array (\n  k => v,\n),` |
|        - |  639 | ` * and \Class::__set_state(array(...)) for objects. PHP's indentation is quirky` |
|        - |  640 | ` * (2-space array entries; 3-space object property lines; a composite value always` |
|        - |  641 | ` * opens at containerIndent+2) but deterministic; this matches it byte-for-byte.` |
|        - |  642 | ` */` |
|        - |  643 | `/* Instance flag (distinct from oo.c's CLASS_INSTANCE_DESTROYED 0x001) marking an` |
|        - |  644 | ` * object currently on the var_export recursion stack, for cycle detection. */` |
|        - |  645 | `#define VM_INSTANCE_DUMPING 0x002` |
|        - |  646 | `typedef struct VmExportCtx VmExportCtx;` |
|        - |  647 | `struct VmExportCtx` |
|        - |  648 | `{` |
|        - |  649 | `	SyBlob *pOut;` |
|        - |  650 | `	int nIndent;  /* indentation of the container emitting this entry */` |
|        - |  651 | `	int depth;    /* recursion guard */` |
|        - |  652 | `};` |
|        - |  653 | `static void VmExportValue(SyBlob *pOut, ph7_value *pVal, int nIndent, int depth);` |
|        - |  654 | `/* Append nIndent spaces. */` |
|     4388 |  655 | `static void VmExportIndent(SyBlob *pOut, int nIndent)` |
|        5 |  656 | `{` |
|        - |  657 | `	int i;` |
|    12531 |  658 | `	for( i = 0; i < nIndent; i++ ){ SyBlobAppend(pOut," ",1); }` |
|     4393 |  659 | `}` |
|        - |  660 | `/* Append a single-quoted PHP string literal (escapes ' and \, batching safe runs` |
|        - |  661 | ` * in one append). A NUL byte can't live in a single-quoted literal, so PHP splits` |
|        - |  662 | ` * it out as ' . "\0" . ' — match that. */` |
|     3918 |  663 | `static void VmExportQuoted(SyBlob *pOut, const char *z, int n)` |
|        5 |  664 | `{` |
|     3923 |  665 | `	int i, run = 0;` |
|     3923 |  666 | `	SyBlobAppend(pOut,"'",1);` |
|    32275 |  667 | `	for( i = 0; i < n; i++ ){` |
|    28357 |  668 | `		char c = z[i];` |
|    28357 |  669 | `		if( c != '\0' && c != '\'' && c != '\\' ){ continue; } /* extend the safe run */` |
|      297 |  670 | `		if( i > run ){ SyBlobAppend(pOut,&z[run],(sxu32)(i-run)); }` |
|      297 |  671 | `		if( c == '\0' ){ SyBlobAppend(pOut,"' . \"\\0\" . '",12); }` |
|      275 |  672 | `		else { SyBlobAppend(pOut,"\\",1); SyBlobAppend(pOut,&z[i],1); }` |
|      297 |  673 | `		run = i+1;` |
|      150 |  674 | `	}` |
|     3923 |  675 | `	if( n > run ){ SyBlobAppend(pOut,&z[run],(sxu32)(n-run)); }` |
|     3923 |  676 | `	SyBlobAppend(pOut,"'",1);` |
|     3923 |  677 | `}` |
|        - |  678 | `/* True if the array/object is already on the var_export recursion stack. */` |
|     2748 |  679 | `static int VmExportIsCycle(ph7_value *pVal)` |
|        5 |  680 | `{` |
|     2753 |  681 | `	if( ph7_value_is_array(pVal) ){` |
|      292 |  682 | `		return (((ph7_hashmap *)pVal->x.pOther)->iFlags & HASHMAP_DUMPING) != 0;` |
|        - |  683 | `	}` |
|     2465 |  684 | `	if( ph7_value_is_object(pVal) ){` |
|       16 |  685 | `		return (((ph7_class_instance *)pVal->x.pOther)->iFlags & VM_INSTANCE_DUMPING) != 0;` |
|        - |  686 | `	}` |
|     2453 |  687 | `	return 0;` |
|     1379 |  688 | `}` |
|        - |  689 | `/* Emit " => " then the value: a scalar inline, a composite on its own line at` |
|        - |  690 | ` * containerIndent+2. A circular reference renders inline as NULL (like PHP). */` |
|     2748 |  691 | `static void VmExportEntryValue(SyBlob *pOut, ph7_value *pVal, int nContainerIndent, int depth)` |
|        5 |  692 | `{` |
|     2753 |  693 | `	SyBlobAppend(pOut," => ",4);` |
|     2753 |  694 | `	if( VmExportIsCycle(pVal) ){` |
|        5 |  695 | `		SyBlobAppend(pOut,"NULL",4);` |
|     2751 |  696 | `	}else if( ph7_value_is_array(pVal) \|\| ph7_value_is_object(pVal) ){` |
|      301 |  697 | `		SyBlobAppend(pOut,"\n",1);` |
|      301 |  698 | `		VmExportIndent(pOut,nContainerIndent+2);` |
|      301 |  699 | `		VmExportValue(pOut,pVal,nContainerIndent+2,depth+1);` |
|      153 |  700 | `	}else{` |
|     2453 |  701 | `		VmExportValue(pOut,pVal,nContainerIndent+2,depth+1);` |
|        - |  702 | `	}` |
|     2753 |  703 | `	SyBlobAppend(pOut,",\n",2);` |
|     2753 |  704 | `}` |
|        - |  705 | `/* Array walker: "<indent+2>key => value,\n" for each entry. */` |
|     2660 |  706 | `static int VmExportArrayWalk(ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|        5 |  707 | `{` |
|     2665 |  708 | `	VmExportCtx *pC = (VmExportCtx *)pUserData;` |
|     2665 |  709 | `	VmExportIndent(pC->pOut,pC->nIndent+2);` |
|     2665 |  710 | `	if( ph7_value_is_string(pKey) ){` |
|        - |  711 | `		int n;` |
|      577 |  712 | `		const char *z = ph7_value_to_string(pKey,&n);` |
|      577 |  713 | `		VmExportQuoted(pC->pOut,z,n);` |
|      291 |  714 | `	}else{` |
|     2093 |  715 | `		SyBlobFormat(pC->pOut,"%qd",ph7_value_to_int64(pKey));` |
|        - |  716 | `	}` |
|     2665 |  717 | `	VmExportEntryValue(pC->pOut,pValue,pC->nIndent,pC->depth);` |
|     2665 |  718 | `	return PH7_OK;` |
|        5 |  719 | `}` |
|     9772 |  720 | `static void VmExportValue(SyBlob *pOut, ph7_value *pVal, int nIndent, int depth)` |
|        5 |  721 | `{` |
|     9777 |  722 | `	if( depth > 4096 ){ return; } /* backstop for pathological finite nesting */` |
|     9777 |  723 | `	if( ph7_value_is_null(pVal) ){` |
|      291 |  724 | `		SyBlobAppend(pOut,"NULL",4);` |
|     9634 |  725 | `	}else if( ph7_value_is_bool(pVal) ){` |
|     2321 |  726 | `		if( ph7_value_to_bool(pVal) ){ SyBlobAppend(pOut,"true",4); }` |
|     1101 |  727 | `		else { SyBlobAppend(pOut,"false",5); }` |
|     8333 |  728 | `	}else if( ph7_value_is_float(pVal) ){` |
|        - |  729 | `		/* float before int: ph7_value_is_int is lenient (true for integer reals). */` |
|      514 |  730 | `		sxu32 before = SyBlobLength(pOut), i, after;` |
|        - |  731 | `		const char *z;` |
|      514 |  732 | `		int plain = 1;` |
|      514 |  733 | `		PH7_AppendShortestReal(pOut,ph7_value_to_double(pVal));` |
|      514 |  734 | `		z = (const char *)SyBlobData(pOut);` |
|      514 |  735 | `		after = SyBlobLength(pOut);` |
|     1244 |  736 | `		for( i = before; i < after; i++ ){` |
|     1008 |  737 | `			if( !((z[i]>='0'&&z[i]<='9')\|\|z[i]=='-') ){ plain = 0; break; }` |
|      369 |  738 | `		}` |
|      514 |  739 | `		if( plain ){ SyBlobAppend(pOut,".0",2); } /* integer-form floats render 1.0/100.0 */` |
|     6920 |  740 | `	}else if( ph7_value_is_int(pVal) ){` |
|     2077 |  741 | `		sxi64 iVal = ph7_value_to_int64(pVal);` |
|     2077 |  742 | `		if( iVal == SMALLEST_INT64 ){` |
|        - |  743 | `			/* php renders LONG_MIN as an expression: the positive literal` |
|        - |  744 | `			 * 9223372036854775808 would not be representable when eval'd. */` |
|        9 |  745 | `			SyBlobAppend(pOut,"-9223372036854775807-1",sizeof("-9223372036854775807-1")-1);` |
|        5 |  746 | `		}else{` |
|     2069 |  747 | `			SyBlobFormat(pOut,"%qd",iVal);` |
|        5 |  748 | `		}` |
|     5629 |  749 | `	}else if( ph7_value_is_string(pVal) ){` |
|        - |  750 | `		int n;` |
|     3255 |  751 | `		const char *z = ph7_value_to_string(pVal,&n);` |
|     3255 |  752 | `		VmExportQuoted(pOut,z,n);` |
|     2968 |  753 | `	}else if( ph7_value_is_array(pVal) ){` |
|     1253 |  754 | `		ph7_hashmap *pMap = (ph7_hashmap *)pVal->x.pOther;` |
|     1253 |  755 | `		if( pMap->iFlags & HASHMAP_DUMPING ){` |
|      ! 0 |  756 | `			SyBlobAppend(pOut,"NULL",4); /* circular reference -> NULL, like PHP */` |
|      ! 0 |  757 | `		}else{` |
|        - |  758 | `			VmExportCtx ctx;` |
|     1253 |  759 | `			pMap->iFlags \|= HASHMAP_DUMPING;` |
|     1253 |  760 | `			SyBlobAppend(pOut,"array (\n",8);` |
|     1253 |  761 | `			ctx.pOut = pOut; ctx.nIndent = nIndent; ctx.depth = depth;` |
|     1253 |  762 | `			ph7_array_walk(pVal,VmExportArrayWalk,&ctx);` |
|     1253 |  763 | `			VmExportIndent(pOut,nIndent);` |
|     1253 |  764 | `			SyBlobAppend(pOut,")",1);` |
|     1253 |  765 | `			pMap->iFlags &= ~HASHMAP_DUMPING;` |
|        5 |  766 | `		}` |
|      719 |  767 | `	}else if( ph7_value_is_object(pVal) ){` |
|       95 |  768 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|       95 |  769 | `		if( pThis->pClass->iFlags & PH7_CLASS_ENUM ){` |
|        - |  770 | ``			/* php 8.1: an enum case exports as `\S::A` */`` |
|        3 |  771 | `			ph7_value *pName = PH7_EnumCaseNameValue(pThis);` |
|        3 |  772 | `			SyBlobAppend(pOut,"\\",1);` |
|        3 |  773 | `			SyBlobAppend(pOut,pThis->pClass->sName.zString,pThis->pClass->sName.nByte);` |
|        3 |  774 | `			SyBlobAppend(pOut,"::",2);` |
|        3 |  775 | `			if( pName && SyBlobLength(&pName->sBlob) > 0 ){` |
|        3 |  776 | `				SyBlobAppend(pOut,SyBlobData(&pName->sBlob),SyBlobLength(&pName->sBlob));` |
|        2 |  777 | `			}` |
|       94 |  778 | `		}else if( pThis->iFlags & VM_INSTANCE_DUMPING ){` |
|      ! 0 |  779 | `			SyBlobAppend(pOut,"NULL",4); /* circular reference -> NULL, like PHP */` |
|      ! 0 |  780 | `		}else{` |
|       93 |  781 | `			SyString *pClassName = &pThis->pClass->sName;` |
|        - |  782 | `			SyHashEntry *pEntry;` |
|        - |  783 | `			SySet sNames;` |
|        - |  784 | `			SyString *aName;` |
|        - |  785 | `			sxu32 iName,nName;` |
|        - |  786 | ``			/* php exports a plain stdClass as a CAST — `(object) array(...)` — and`` |
|        - |  787 | `			 * every other class through __set_state(); a SUBCLASS of stdClass takes` |
|        - |  788 | `			 * the __set_state form, so this is the exact class and not an` |
|        - |  789 | `			 * inheritance test. The two forms differ by one closing paren. */` |
|       93 |  790 | `			int bStdObj = pThis->pClass == pThis->pVm->pStdClass;` |
|       93 |  791 | `			pThis->iFlags \|= VM_INSTANCE_DUMPING;` |
|       93 |  792 | `			if( bStdObj ){` |
|        3 |  793 | `				SyBlobAppend(pOut,"(object) array(\n",sizeof("(object) array(\n")-1);` |
|        2 |  794 | `			}else{` |
|       91 |  795 | `				SyBlobAppend(pOut,"\\",1);` |
|       91 |  796 | `				SyBlobAppend(pOut,pClassName->zString,pClassName->nByte);` |
|       91 |  797 | `				SyBlobAppend(pOut,"::__set_state(array(\n",21);` |
|        - |  798 | `			}` |
|        - |  799 | `			{` |
|        - |  800 | `				/* A native class's PRESENTATION (php's get_properties): var_export` |
|        - |  801 | `				 * shows a DateTime as date/timezone_type/timezone, the same shape` |
|        - |  802 | `				 * the (array) cast produces and NOT the hidden engine slots. A` |
|        - |  803 | `				 * debug-only hook (WeakReference) fills nothing, which is php's` |
|        - |  804 | `				 * empty export. */` |
|        - |  805 | `				ph7_value sPresent;` |
|       93 |  806 | `				ph7_hashmap *pPresent = PH7_NewHashmap(pThis->pVm,0,0);` |
|       93 |  807 | `				PH7_MemObjInit(pThis->pVm,&sPresent);` |
|       93 |  808 | `				if( pPresent ){` |
|       93 |  809 | `					sPresent.x.pOther = pPresent;` |
|       93 |  810 | `					MemObjSetType(&sPresent,MEMOBJ_HASHMAP);` |
|       93 |  811 | `					if( PH7_ClassInstancePresent(pThis,&sPresent,0) ){` |
|        - |  812 | `						/* Same line shape the attribute loop below produces: an` |
|        - |  813 | `						 * object body's entries sit one deeper than an array's. */` |
|        - |  814 | `						VmExportCtx sCtx;` |
|       32 |  815 | `						sCtx.pOut = pOut;` |
|       32 |  816 | `						sCtx.nIndent = nIndent + 1;` |
|       32 |  817 | `						sCtx.depth = depth;` |
|       32 |  818 | `						ph7_array_walk(&sPresent,VmExportArrayWalk,&sCtx);` |
|       32 |  819 | `						PH7_MemObjRelease(&sPresent);` |
|       32 |  820 | `						VmExportIndent(pOut,nIndent);` |
|       32 |  821 | `						SyBlobAppend(pOut,bStdObj ? ")" : "))",bStdObj ? 1 : 2);` |
|       32 |  822 | `						pThis->iFlags &= ~VM_INSTANCE_DUMPING;` |
|       32 |  823 | `						return;` |
|        - |  824 | `					}` |
|       63 |  825 | `					PH7_MemObjRelease(&sPresent);` |
|       29 |  826 | `				}` |
|        - |  827 | `			}` |
|        - |  828 | `			/* SNAPSHOT the attribute names first: a PHP 8.4 get hook dispatched` |
|        - |  829 | `			 * mid-walk may re-enter an hAttr walk on this instance (the hash has` |
|        - |  830 | `			 * a single embedded loop cursor) or unset()/create properties; names` |
|        - |  831 | `			 * point into class-owned attr storage and each is re-looked-up. */` |
|       63 |  832 | `			SySetInit(&sNames,&pThis->pVm->sAllocator,sizeof(SyString));` |
|       63 |  833 | `			SyHashResetLoopCursor(&pThis->hAttr);` |
|      215 |  834 | `			while( (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|      157 |  835 | `				VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|      157 |  836 | `				if( pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_HIDDEN) ){ continue; }` |
|      121 |  837 | `				if( PH7_ClassAttrUninitializedForRead(pVmAttr) ){` |
|       22 |  838 | `					continue; /* typed, never written: not there yet (php) */` |
|        - |  839 | `				}` |
|       96 |  840 | `				if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_VIRTUAL))` |
|       53 |  841 | `				 == PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|      ! 0 |  842 | `					continue; /* virtual set-only property: no value to export (php) */` |
|        - |  843 | `				}` |
|      101 |  844 | `				SySetPut(&sNames,(const void *)&pVmAttr->pAttr->sName);` |
|        5 |  845 | `			}` |
|       63 |  846 | `			aName = (SyString *)SySetBasePtr(&sNames);` |
|       63 |  847 | `			nName = SySetUsed(&sNames);` |
|      159 |  848 | `			for( iName = 0 ; iName < nName ; ++iName ){` |
|      101 |  849 | `				SyString *pAName = &aName[iName];` |
|        - |  850 | `				VmClassAttr *pVmAttr;` |
|        - |  851 | `				ph7_value *pAttrVal;` |
|      101 |  852 | `				pEntry = PH7_ClassInstanceAttrEntry(pThis,pAName->zString,pAName->nByte);` |
|      101 |  853 | `				if( pEntry == 0 ){ continue; } /* unset by an earlier hook */` |
|      101 |  854 | `				pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|      101 |  855 | `				VmExportIndent(pOut,nIndent+3); /* object property lines sit one deeper than arrays */` |
|      103 |  856 | `				if( pAName->nByte > 0 && pAName->zString[0] == 0 ){` |
|        - |  857 | `					/* A MANGLED key stored raw (the __PHP_Incomplete_Class carrier):` |
|        - |  858 | `					 * php's var_export prints the PLAIN name ('bp' => 1). */` |
|        - |  859 | `					SyString sUnmCls, sUnmName;` |
|        5 |  860 | `					SyStringInitFromBuf(&sUnmName,pAName->zString,pAName->nByte);` |
|        5 |  861 | `					PH7_UnmangleAttrName(pAName->zString,pAName->nByte,&sUnmCls,&sUnmName);` |
|        5 |  862 | `					VmExportQuoted(pOut,sUnmName.zString,(int)sUnmName.nByte);` |
|        3 |  863 | `				}else{` |
|       97 |  864 | `					VmExportQuoted(pOut,pAName->zString,(int)pAName->nByte);` |
|        - |  865 | `				}` |
|        - |  866 | `				/* PHP 8.4 property hooks: var_export() reads through the get hook` |
|        - |  867 | `				 * (every visibility — php exports private hooked values too). A` |
|        - |  868 | `				 * throwing hook parks on the boundary rail; the helper's boundary` |
|        - |  869 | `				 * gate keeps LATER hooks from running (the raw values the tail of` |
|        - |  870 | `				 * the export falls back to are discarded when the throw routes). */` |
|        - |  871 | `				{` |
|        - |  872 | `					ph7_value sHookVal;` |
|        - |  873 | `					sxi32 rcHk;` |
|      101 |  874 | `					PH7_MemObjInit(pThis->pVm,&sHookVal);` |
|      101 |  875 | `					rcHk = PH7_VmHookGetAttrValue(pThis,pVmAttr,&sHookVal);` |
|      101 |  876 | `					if( rcHk == SXRET_OK ){` |
|       16 |  877 | `						VmExportEntryValue(pOut,&sHookVal,nIndent,depth);` |
|       16 |  878 | `						PH7_MemObjRelease(&sHookVal);` |
|       20 |  879 | `						continue;` |
|        - |  880 | `					}` |
|       87 |  881 | `					PH7_MemObjRelease(&sHookVal);` |
|       87 |  882 | `					if( rcHk != SXERR_NOTFOUND ){` |
|        - |  883 | `						/* the hook threw (parked on the boundary rail): NULL` |
|        - |  884 | `						 * placeholder keeps the output well-formed */` |
|       10 |  885 | `						SyBlobAppend(pOut," => NULL,\n",10);` |
|       10 |  886 | `						continue;` |
|        - |  887 | `					}` |
|        - |  888 | `				}` |
|       79 |  889 | `				pAttrVal = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       79 |  890 | `				if( pAttrVal ){ VmExportEntryValue(pOut,pAttrVal,nIndent,depth); }` |
|      ! 0 |  891 | `				else { SyBlobAppend(pOut," => NULL,\n",10); }` |
|       42 |  892 | `			}` |
|       63 |  893 | `			SySetRelease(&sNames);` |
|       63 |  894 | `			VmExportIndent(pOut,nIndent);` |
|       63 |  895 | `			SyBlobAppend(pOut,bStdObj ? ")" : "))",bStdObj ? 1 : 2);` |
|       63 |  896 | `			pThis->iFlags &= ~VM_INSTANCE_DUMPING;` |
|        - |  897 | `		}` |
|       35 |  898 | `	}else{` |
|        - |  899 | `		/* resource / other -> PHP emits NULL (with a warning we omit) */` |
|      ! 0 |  900 | `		SyBlobAppend(pOut,"NULL",4);` |
|        - |  901 | `	}` |
|     4891 |  902 | `}` |
|        - |  903 | `/*` |
|        - |  904 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - |  905 | ` *  PHP-exact evaluable representation (see VmExportValue).` |
|        - |  906 | ` */` |
|     7028 |  907 | `PH7_PRIVATE int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  908 | `{` |
|     7033 |  909 | `	int ret_string = 0;` |
|        - |  910 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|     7033 |  911 | `	if( nArg < 1 ){` |
|        - |  912 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  913 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  914 | `		return SXRET_OK;` |
|        - |  915 | `	}` |
|     7033 |  916 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|     7033 |  917 | `	if ( nArg > 1 ){` |
|        - |  918 | `		/* Where to redirect output */` |
|     6575 |  919 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|     3285 |  920 | `	}` |
|        - |  921 | `	/* Generate the PHP-exact evaluable representation */` |
|     7033 |  922 | `	VmExportValue(&sDump,apArg[0],0,0);` |
|     7033 |  923 | `	if( PH7_CALLBACK_UNWOUND(pCtx->pVm->nBoundaryRc) ){` |
|        - |  924 | ``		/* A php 8.4 `get` hook threw (or exited) part-way through: php's var_export`` |
|        - |  925 | `		 * builds its string before it prints anything, so a throw from inside it` |
|        - |  926 | ``		 * reaches the caller with NOTHING written. The `$return = true` form was`` |
|        - |  927 | `		 * already right — the parked throw discards the RESULT — but the printing` |
|        - |  928 | `		 * form had already handed the half-built text to the output layer, so a` |
|        - |  929 | `		 * caught throw was followed by an export naming properties as NULL. */` |
|       10 |  930 | `		SyBlobRelease(&sDump);` |
|       10 |  931 | `		ph7_result_null(pCtx);` |
|       10 |  932 | `		return SXRET_OK;` |
|        - |  933 | `	}` |
|     7025 |  934 | `	if( !ret_string ){` |
|        - |  935 | `		/* Output dump */` |
|      457 |  936 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  937 | `		/* Return NULL */` |
|      457 |  938 | `		ph7_result_null(pCtx);` |
|      231 |  939 | `	}else{` |
|        - |  940 | `		/* Generated dump as return value */` |
|     6573 |  941 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  942 | `	}` |
|        - |  943 | `	/* Release the working buffer */` |
|     7025 |  944 | `	SyBlobRelease(&sDump);` |
|     7025 |  945 | `	return SXRET_OK;` |
|     3519 |  946 | `}` |
|        - |  947 |  |
