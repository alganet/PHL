# src/ph7/vm_builtin_var.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 491/529 lines (92.82%)

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
|   150032 |   23 | `PH7_PRIVATE int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |   24 | `{` |
|        - |   25 | `	ph7_value *pObj;` |
|   150037 |   26 | `	int res = 0;` |
|        - |   27 | `	int i;` |
|   150037 |   28 | `	if( nArg < 1 ){` |
|        - |   29 | `		/* Missing arguments,return false */` |
|      ! 0 |   30 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 |   31 | `		return SXRET_OK;` |
|        - |   32 | `	}` |
|        - |   33 | `	/* Iterate over available arguments */` |
|   185045 |   34 | `	for( i = 0 ; i < nArg ; ++i ){` |
|   150067 |   35 | `		pObj = apArg[i];` |
|   150067 |   36 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|        - |   37 | `			/* Skip the "expecting a variable" warning for MEMOBJ_BOOL —` |
|        - |   38 | `			 * synthesized by LOAD_IDX iP2=4 (ArrayAccess::offsetExists) and` |
|        - |   39 | `			 * by anyone passing a bool literal (rare, harmless). */` |
|   149945 |   40 | `			if( (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|        - |   41 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 |   42 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 |   43 | `			}` |
|    74970 |   44 | `		}` |
|   150067 |   45 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|   150067 |   46 | `		if( !res ){` |
|        - |   47 | `			/* Variable not set,return FALSE */` |
|   115059 |   48 | `			ph7_result_bool(pCtx,0);` |
|   115059 |   49 | `			return SXRET_OK;` |
|        - |   50 | `		}` |
|    17509 |   51 | `	}` |
|        - |   52 | `	/* All given variable are set,return TRUE */` |
|    34983 |   53 | `	ph7_result_bool(pCtx,1);` |
|    34983 |   54 | `	return SXRET_OK;` |
|    75021 |   55 | `}` |
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
|     8320 |   82 | `PH7_PRIVATE sxi32 VmUnsetVarByNameEx(ph7_vm *pVm,VmFrame *pFrame,const char *zName,sxu32 nByte,` |
|        - |   83 | `	int bNameGuard)` |
|        5 |   84 | `{` |
|        - |   85 | `	SyHashEntry *pEntry;` |
|        - |   86 | `	VmRefObj *pRef;` |
|        - |   87 | `	sxu32 nIdx;` |
|        - |   88 | `	/* php 8.1 forbids unset($GLOBALS) outright. Checked by NAME: the superglobal is not an` |
|        - |   89 | `	 * ordinary hVar binding, so the slot-index test below never sees it. */` |
|     8325 |   90 | `	if( bNameGuard && nByte == sizeof("GLOBALS")-1 && SyMemcmp(zName,"GLOBALS",nByte) == 0 ){` |
|        3 |   91 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |   92 | `			"$GLOBALS can only be modified using the $GLOBALS[$name] = $value syntax");` |
|        3 |   93 | `		pVm->iExitStatus = 255;` |
|        3 |   94 | `		pVm->bHaltRequested = 1;` |
|        3 |   95 | `		return PH7_ABORT;` |
|        - |   96 | `	}` |
|     8323 |   97 | `	pEntry = SyHashGet(&pFrame->hVar,(const void *)zName,nByte);` |
|     8323 |   98 | `	if( pEntry == 0 ){` |
|        - |   99 | `		/* No such variable: unset() is a no-op on an undefined name, as in php */` |
|     1625 |  100 | `		return SXRET_OK;` |
|        - |  101 | `	}` |
|     6702 |  102 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|     6702 |  103 | `	if( nIdx == pVm->nGlobalIdx ){` |
|      ! 0 |  104 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  105 | `			"$GLOBALS can only be modified using the $GLOBALS[$name] = $value syntax");` |
|      ! 0 |  106 | `		pVm->iExitStatus = 255;` |
|      ! 0 |  107 | `		pVm->bHaltRequested = 1;` |
|      ! 0 |  108 | `		return PH7_ABORT;` |
|        - |  109 | `	}` |
|     6702 |  110 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|        - |  111 | `	/*` |
|        - |  112 | `	 * A GLOBAL variable is ALSO an entry of the $GLOBALS array, and that node points at the` |
|        - |  113 | `	 * same slot. php's unset($x) in global scope drops $GLOBALS['x'] too, so unlink the node` |
|        - |  114 | `	 * — otherwise it stays a live holder and the value is never released ($o = new D;` |
|        - |  115 | `	 * unset($o); stopped running the destructor). Clear the reference table's row for the` |
|        - |  116 | `	 * node BEFORE unlinking it: unlinking frees the node, and the holder count below would` |
|        - |  117 | `	 * otherwise dereference freed memory.` |
|        - |  118 | `	 */` |
|     6702 |  119 | `	if( pFrame->pParent == 0 ){` |
|     6672 |  120 | `		ph7_value *pGlobals = (ph7_value *)SySetAt(&pVm->aMemObj,pVm->nGlobalIdx);` |
|     6672 |  121 | `		if( pGlobals && (pGlobals->iFlags & MEMOBJ_HASHMAP) ){` |
|     6672 |  122 | `			ph7_hashmap_node *pNode = 0;` |
|        - |  123 | `			ph7_value sKey;` |
|        - |  124 | `			SyString sName;` |
|     6672 |  125 | `			PH7_MemObjInit(&(*pVm),&sKey);` |
|     6672 |  126 | `			SyStringInitFromBuf(&sName,zName,nByte);` |
|     6672 |  127 | `			PH7_MemObjInitFromString(&(*pVm),&sKey,&sName);` |
|     6668 |  128 | `			if( SXRET_OK == PH7_HashmapLookup((ph7_hashmap *)pGlobals->x.pOther,&sKey,&pNode)` |
|     6667 |  129 | `			 && pNode && pNode->nValIdx == nIdx ){` |
|     6662 |  130 | `				if( pRef ){` |
|     6662 |  131 | `					ph7_hashmap_node **apN = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|        - |  132 | `					sxu32 k;` |
|    13552 |  133 | `					for( k = 0 ; k < SySetUsed(&pRef->aArrEntries) ; ++k ){` |
|     6894 |  134 | `						if( apN[k] == pNode ){` |
|     6662 |  135 | `							apN[k] = 0;` |
|     3329 |  136 | `						}` |
|     3449 |  137 | `					}` |
|     3329 |  138 | `				}` |
|     6662 |  139 | `				PH7_HashmapUnlinkNode(pNode,FALSE);` |
|     3329 |  140 | `			}` |
|     6672 |  141 | `			PH7_MemObjRelease(&sKey);` |
|     3334 |  142 | `		}` |
|     3334 |  143 | `	}` |
|        - |  144 |  |
|        - |  145 | `	/* The frame's own "release this reference at exit" row for the binding about to go:` |
|        - |  146 | `	 * the entry is freed below, so the row would dangle (and a foreach value variable` |
|        - |  147 | `	 * unset once per loop filed one row per loop, which nothing consumed until the` |
|        - |  148 | `	 * function returned). */` |
|     6702 |  149 | `	VmDropFrameRefEntry(&(*pVm),nIdx,pEntry);` |
|     6702 |  150 | `	if( pRef == 0 ){` |
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
|     6702 |  161 | `		SyHashEntry **apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - |  162 | `		sxu32 n;` |
|        - |  163 | `		/* Forget THIS name in the slot's reference record (leave the others alone) */` |
|    13536 |  164 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; ++n ){` |
|     6838 |  165 | `			if( apEntry[n] == pEntry ){` |
|     6690 |  166 | `				apEntry[n] = 0;` |
|     3343 |  167 | `			}` |
|     3421 |  168 | `		}` |
|     6702 |  169 | `		SyHashDeleteEntry2(pEntry);` |
|        - |  170 | `		/* The value goes with the LAST holder and not before — the one rule, counted in` |
|        - |  171 | `		 * one place: other names, array nodes that still point here, and a PIN (a static's` |
|        - |  172 | ``		 * storage, a `use (&$x)` capture, a reference-bound property), which is a holder`` |
|        - |  173 | `		 * this table cannot name. Unsetting the name of a static used to release the` |
|        - |  174 | `		 * static's value, so the next call started over from the initializer. */` |
|     6702 |  175 | `		PH7_VmReleaseUnheldSlot(&(*pVm),nIdx);` |
|        - |  176 | `	}` |
|     6702 |  177 | `	return SXRET_OK;` |
|     4165 |  178 | `}` |
|     8162 |  179 | `PH7_PRIVATE sxi32 VmUnsetVarByName(ph7_vm *pVm,VmFrame *pFrame,const char *zName,sxu32 nByte)` |
|        5 |  180 | `{` |
|     8167 |  181 | `	return VmUnsetVarByNameEx(&(*pVm),pFrame,zName,nByte,TRUE);` |
|        5 |  182 | `}` |
| 21655055 |  183 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        5 |  184 | `{` |
|        - |  185 | `	ph7_value *pObj;` |
|        - |  186 | `	VmRefObj *pRef;` |
| 21655060 |  187 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
| 21655060 |  188 | `	if( pObj ){` |
|        - |  189 | `		/* Release the object */` |
| 21655060 |  190 | `		PH7_MemObjRelease(pObj);` |
| 10828846 |  191 | `	}` |
|        - |  192 | `	/* Remove old reference links */` |
| 21655060 |  193 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
| 21655060 |  194 | `	if( pRef ){` |
| 21655016 |  195 | `		sxi32 iFlags = pRef->iFlags;` |
|        - |  196 | `		/* Unlink from the reference table */` |
| 21655016 |  197 | `		VmRefObjUnlink(&(*pVm),pRef);` |
| 21655016 |  198 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - |  199 | `			VmSlot sFree;` |
|        - |  200 | `			/* Restore to the free list */` |
| 21655016 |  201 | `			sFree.nIdx = nObjIdx;` |
| 21655016 |  202 | `			sFree.pUserData = 0;` |
| 21655016 |  203 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
| 10828824 |  204 | `		}` |
| 10828824 |  205 | `	}` |
| 21655060 |  206 | `	return SXRET_OK;` |
|        5 |  207 | `}` |
|        - |  208 | `/*` |
|        - |  209 | ` * void unset($var,...)` |
|        - |  210 | ` *   Unset one or more given variable.` |
|        - |  211 | ` * Parameters` |
|        - |  212 | ` *  One or more variable to unset.` |
|        - |  213 | ` * Return` |
|        - |  214 | ` *  Nothing.` |
|        - |  215 | ` */` |
|     1494 |  216 | `PH7_PRIVATE int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  217 | `{` |
|        - |  218 | `	ph7_value *pObj;` |
|        - |  219 | `	ph7_vm *pVm;` |
|        - |  220 | `	int i;` |
|        - |  221 | `	/* Point to the target VM */` |
|     1499 |  222 | `	pVm = pCtx->pVm;` |
|        - |  223 | `	/* Iterate and unset */` |
|     2993 |  224 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     1499 |  225 | `		pObj = apArg[i];` |
|     1499 |  226 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|     1497 |  227 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  228 | `				/* Throw an error */` |
|      ! 0 |  229 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 |  230 | `			}` |
|      751 |  231 | `		}else{` |
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
|      752 |  247 | `	}` |
|     1499 |  248 | `	return SXRET_OK;` |
|      752 |  249 | `}` |
|        - |  250 | `/*` |
|        - |  251 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - |  252 | ` */` |
|    11248 |  253 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        4 |  254 | `{` |
|    11252 |  255 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|    11252 |  256 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  257 | `	ph7_value *pObj;` |
|        - |  258 | `	sxu32 nIdx;` |
|        - |  259 | `	/* php excludes $this from get_defined_vars() (it is bound implicitly, not a` |
|        - |  260 | `	 * declared local). PHL keeps it in the frame's hVar, so skip it here. */` |
|    11248 |  261 | `	if( pEntry->nKeyLen == sizeof("this")-1` |
|     6727 |  262 | `	 && SyMemcmp(pEntry->pKey,"this",sizeof("this")-1) == 0 ){` |
|        6 |  263 | `		return SXRET_OK;` |
|        - |  264 | `	}` |
|        - |  265 | `	/* Engine temporaries (a foreach destructuring/target slot) are not variables the` |
|        - |  266 | `	 * program declared — php compiles those into slots with no name at all. */` |
|    11248 |  267 | `	if( PH7_VmVarNameIsInternal((const char *)pEntry->pKey,pEntry->nKeyLen) ){` |
|      743 |  268 | `		return SXRET_OK;` |
|        - |  269 | `	}` |
|        - |  270 | `	/* Extract the memory object */` |
|    10506 |  271 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|    10506 |  272 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|    10506 |  273 | `	if( pObj ){` |
|    10506 |  274 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|    10494 |  275 | `			if( pEntry->nKeyLen > 0 ){` |
|        - |  276 | `				SyString sName;` |
|        - |  277 | `				ph7_value sKey;` |
|        - |  278 | `				/* Perform the insertion (pObj may point into pVm->aMemObj; the` |
|        - |  279 | `				 * inserter snapshots the source before reserving, so the pool may` |
|        - |  280 | `				 * safely move underneath it — see HashmapInsertIntKey/BlobKey). */` |
|    10494 |  281 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|    10494 |  282 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|    10494 |  283 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|    10494 |  284 | `				PH7_MemObjRelease(&sKey);` |
|     5245 |  285 | `			}` |
|     5245 |  286 | `		}` |
|     5251 |  287 | `	}` |
|    10506 |  288 | `	return SXRET_OK;` |
|     5628 |  289 | `}` |
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
|       76 |  347 | `PH7_PRIVATE int vm_builtin_get_debug_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  348 | `{` |
|       77 |  349 | `	const char *zType = "null";` |
|       77 |  350 | `	if( nArg > 0 ){` |
|       77 |  351 | `		ph7_value *pVal = apArg[0];` |
|       77 |  352 | `		if( pVal->iFlags & MEMOBJ_OBJ ){` |
|        7 |  353 | `			ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|       10 |  354 | `			ph7_result_string(pCtx,SyStringData(&pThis->pClass->sName),` |
|        6 |  355 | `				(int)SyStringLength(&pThis->pClass->sName));` |
|        7 |  356 | `			return SXRET_OK;` |
|        - |  357 | `		}` |
|       71 |  358 | `		if( pVal->iFlags & MEMOBJ_NULL ){` |
|        5 |  359 | `			zType = "null";` |
|       69 |  360 | `		}else if( pVal->iFlags & MEMOBJ_REAL ){` |
|        - |  361 | `			/* REAL wins over a cached MEMOBJ_INT, as it does for gettype() */` |
|       13 |  362 | `			zType = "float";` |
|       61 |  363 | `		}else if( pVal->iFlags & MEMOBJ_INT ){` |
|       19 |  364 | `			zType = "int";` |
|       46 |  365 | `		}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|        5 |  366 | `			zType = "string";` |
|       35 |  367 | `		}else if( pVal->iFlags & MEMOBJ_BOOL ){` |
|        7 |  368 | `			zType = "bool";` |
|       30 |  369 | `		}else if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|        7 |  370 | `			zType = "array";` |
|       24 |  371 | `		}else if( pVal->iFlags & MEMOBJ_RES ){` |
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
|       27 |  386 | `	}` |
|       55 |  387 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       55 |  388 | `	return SXRET_OK;` |
|       39 |  389 | `}` |
|        - |  390 | `/*` |
|        - |  391 | ` * bool gettype($var)` |
|        - |  392 | ` *  Get the type of a variable` |
|        - |  393 | ` * Parameters` |
|        - |  394 | ` *   $var` |
|        - |  395 | ` *    The variable being type checked.` |
|        - |  396 | ` * Return` |
|        - |  397 | ` *   String representation of the given variable type.` |
|        - |  398 | ` */` |
|      504 |  399 | `PH7_PRIVATE int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        4 |  400 | `{` |
|        - |  401 | `	/* php's gettype() uses LONG type names ("integer"/"boolean"/"NULL"), distinct` |
|        - |  402 | `	 * from get_debug_type()'s short ones ("int"/"bool"/"null") — never route this` |
|        - |  403 | `	 * through PH7_MemObjTypeDump, which yields the short forms. */` |
|      508 |  404 | `	const char *zType = "unknown type";` |
|      508 |  405 | `	if( nArg > 0 ){` |
|      508 |  406 | `		ph7_value *pVal = apArg[0];` |
|      508 |  407 | `		if( pVal->iFlags & MEMOBJ_NULL ){` |
|       13 |  408 | `			zType = "NULL";` |
|      502 |  409 | `		}else if( pVal->iFlags & MEMOBJ_REAL ){` |
|        - |  410 | `			/* REAL wins over a cached MEMOBJ_INT: 1.0 is "double" (php). */` |
|       87 |  411 | `			zType = "double";` |
|      453 |  412 | `		}else if( pVal->iFlags & MEMOBJ_INT ){` |
|      154 |  413 | `			zType = "integer";` |
|      334 |  414 | `		}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|      187 |  415 | `			zType = "string";` |
|      165 |  416 | `		}else if( pVal->iFlags & MEMOBJ_BOOL ){` |
|       15 |  417 | `			zType = "boolean";` |
|       66 |  418 | `		}else if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|       24 |  419 | `			zType = "array";` |
|       47 |  420 | `		}else if( pVal->iFlags & MEMOBJ_OBJ ){` |
|       17 |  421 | `			zType = "object";` |
|       28 |  422 | `		}else if( pVal->iFlags & MEMOBJ_RES ){` |
|        - |  423 | `			/* php reports an fclose()'d handle as "resource (closed)" */` |
|       20 |  424 | `			zType = PH7_VfsResourceIsClosed(pVal->x.pOther) ? "resource (closed)" : "resource";` |
|        9 |  425 | `		}` |
|      252 |  426 | `	}` |
|        - |  427 | `	/* Return the variable type */` |
|      508 |  428 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|      508 |  429 | `	return SXRET_OK;` |
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
|       82 |  445 | `PH7_PRIVATE int vm_builtin_settype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  446 | `{` |
|        - |  447 | `	const char *zType;` |
|        - |  448 | `	int nLen;` |
|        - |  449 | `	ph7_value *pNew;` |
|       30 |  450 | `	SXUNUSED(nArg); /* arity (exactly 2) enforced by the central arity table */` |
|        - |  451 | `	/* php binds $var by reference at the CALL, and the refusal is the call site's` |
|        - |  452 | `	 * to raise (PH7_VmScreenByRefArgShapes) — only the compiler can tell a literal,` |
|        - |  453 | `	 * which php refuses, from the result of a call, which php accepts with a notice` |
|        - |  454 | ``	 * and converts in place on the temporary. The `nIdx == SXU32_HIGH` test that`` |
|        - |  455 | `	 * used to sit here conflated the two. */` |
|       87 |  456 | `	zType = ph7_value_to_string(apArg[1],&nLen);` |
|        - |  457 | `	/* Validate the target type up-front (php checks $type before touching $var):` |
|        - |  458 | `	 * "resource" is a distinct ValueError, any other unknown name is the generic` |
|        - |  459 | `	 * invalid-type ValueError. */` |
|       87 |  460 | `	if( nLen == 8 && SyStrnicmp(zType,"resource",8) == 0 ){` |
|        3 |  461 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|        - |  462 | `			"Cannot convert to resource type");` |
|        - |  463 | `	}` |
|       86 |  464 | `	if( !(  (nLen == 3 && SyStrnicmp(zType,"int",3) == 0)` |
|       60 |  465 | `	     \|\| (nLen == 7 && SyStrnicmp(zType,"integer",7) == 0)` |
|       55 |  466 | `	     \|\| (nLen == 5 && SyStrnicmp(zType,"float",5) == 0)` |
|       54 |  467 | `	     \|\| (nLen == 6 && SyStrnicmp(zType,"double",6) == 0)` |
|       53 |  468 | `	     \|\| (nLen == 6 && SyStrnicmp(zType,"string",6) == 0)` |
|       37 |  469 | `	     \|\| (nLen == 4 && SyStrnicmp(zType,"bool",4) == 0)` |
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
|       95 |  480 | `	pNew = ph7_context_new_scalar(pCtx);` |
|       95 |  481 | `	if( pNew == 0 ){` |
|      ! 0 |  482 | `		return PH7_ContextMemoryError(pCtx);` |
|        - |  483 | `	}` |
|       95 |  484 | `	PH7_MemObjStore(apArg[0],pNew);` |
|       90 |  485 | `	if( (nLen == 3 && SyStrnicmp(zType,"int",3) == 0)` |
|       75 |  486 | `	 \|\| (nLen == 7 && SyStrnicmp(zType,"integer",7) == 0) ){` |
|        - |  487 | `		/* settype() IS the cast operator, warning included. */` |
|       54 |  488 | `		PH7_MemObjWarnIntCast(pNew);` |
|       54 |  489 | `		PH7_MemObjToInteger(pNew);` |
|       54 |  490 | `		MemObjSetType(pNew,MEMOBJ_INT);` |
|       79 |  491 | `	}else if( (nLen == 5 && SyStrnicmp(zType,"float",5) == 0)` |
|       53 |  492 | `	       \|\| (nLen == 6 && SyStrnicmp(zType,"double",6) == 0) ){` |
|        5 |  493 | `		PH7_MemObjToReal(pNew);` |
|        5 |  494 | `		MemObjSetType(pNew,MEMOBJ_REAL);` |
|       61 |  495 | `	}else if( nLen == 6 && SyStrnicmp(zType,"string",6) == 0 ){` |
|        - |  496 | `		/* php emits "Array to string conversion" for settype($arr,'string'), and` |
|        - |  497 | `		 * throws "Object of class X could not be converted to string" for an` |
|        - |  498 | `		 * object with no __toString() (or propagates one that threw). php's` |
|        - |  499 | `		 * convert_to_string() has already blanked the zval by then, so a CAUGHT` |
|        - |  500 | `		 * settype() leaves $var === "" — store that, then propagate instead of` |
|        - |  501 | `		 * answering true. */` |
|       36 |  502 | `		sxi32 rcSv = PH7_MemObjToStringUV(pNew);` |
|       36 |  503 | `		if( rcSv != SXRET_OK ){` |
|       16 |  504 | `			PH7_MemObjRelease(pNew);` |
|       16 |  505 | `			MemObjSetType(pNew,MEMOBJ_STRING);` |
|       16 |  506 | `			PH7_VmStoreArgByRef(pCtx->pVm,apArg[0],pNew);` |
|       16 |  507 | `			pCtx->nThrowRc = rcSv;` |
|       16 |  508 | `			return rcSv;` |
|        - |  509 | `		}` |
|       26 |  510 | `	}else if( (nLen == 4 && SyStrnicmp(zType,"bool",4) == 0)` |
|       14 |  511 | `	       \|\| (nLen == 7 && SyStrnicmp(zType,"boolean",7) == 0) ){` |
|        8 |  512 | `		PH7_MemObjToBool(pNew);` |
|       12 |  513 | `	}else if( nLen == 5 && SyStrnicmp(zType,"array",5) == 0 ){` |
|        5 |  514 | `		PH7_MemObjToHashmap(pNew);` |
|        7 |  515 | `	}else if( nLen == 6 && SyStrnicmp(zType,"object",6) == 0 ){` |
|        3 |  516 | `		PH7_MemObjToObject(pNew);` |
|        2 |  517 | `	}else{` |
|        - |  518 | `		/* "null" — the only validated name left */` |
|        3 |  519 | `		PH7_MemObjToNull(pNew);` |
|        - |  520 | `	}` |
|       91 |  521 | `	PH7_VmStoreArgByRef(pCtx->pVm,apArg[0],pNew);` |
|       91 |  522 | `	ph7_result_bool(pCtx,1);` |
|       91 |  523 | `	return SXRET_OK;` |
|       57 |  524 | `}` |
|        - |  525 | `/*` |
|        - |  526 | ` * string get_resource_type(resource $handle)` |
|        - |  527 | ` *  This function gets the type of the given resource.` |
|        - |  528 | ` * Parameters` |
|        - |  529 | ` *  $handle` |
|        - |  530 | ` *  The evaluated resource handle.` |
|        - |  531 | ` * Return` |
|        - |  532 | ` *  If the given handle is a resource, this function will return a string` |
|        - |  533 | ` *  representing its type. If the type is not identified by this function` |
|        - |  534 | ` *  the return value will be the string Unknown.` |
|        - |  535 | ` *  This function will return FALSE and generate an error if handle` |
|        - |  536 | ` *  is not a resource.` |
|        - |  537 | ` */` |
|       58 |  538 | `PH7_PRIVATE int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  539 | `{` |
|        - |  540 | `	const char *zType;` |
|       60 |  541 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - |  542 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 |  543 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  544 | `		return PH7_OK;` |
|        - |  545 | `	}` |
|        - |  546 | `	/* PHP returns the resource TYPE name (e.g. "stream" for IO handles), not an id. */` |
|       60 |  547 | `	zType = PH7_VfsResourceType(apArg[0]->x.pOther);` |
|       60 |  548 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       60 |  549 | `	return SXRET_OK;` |
|       31 |  550 | `}` |
|        - |  551 | `/*` |
|        - |  552 | ` * int get_resource_id(resource $resource)` |
|        - |  553 | ` *  Returns the integer id of a resource — the same number (int) casts to and` |
|        - |  554 | ` *  "Resource id #N" renders (php 8.0).` |
|        - |  555 | ` */` |
|        4 |  556 | `PH7_PRIVATE int vm_builtin_get_resource_id(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  557 | `{` |
|        6 |  558 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 |  559 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|        - |  560 | `			"get_resource_id(): Argument #1 ($resource) must be of type resource, %s given",` |
|      ! 0 |  561 | `			nArg < 1 ? "none" : ph7_type_name(apArg[0]));` |
|        - |  562 | `	}` |
|        6 |  563 | `	ph7_result_int64(pCtx,(ph7_int64)PH7_VmResourceId(pCtx->pVm,apArg[0]->x.pOther));` |
|        6 |  564 | `	return SXRET_OK;` |
|        4 |  565 | `}` |
|        - |  566 | `/*` |
|        - |  567 | ` * void var_dump(expression,....)` |
|        - |  568 | ` *   var_dump � Dumps information about a variable` |
|        - |  569 | ` * Parameters` |
|        - |  570 | ` *   One or more expression to dump.` |
|        - |  571 | ` * Returns` |
|        - |  572 | ` *  Nothing.` |
|        - |  573 | ` */` |
|     7700 |  574 | `PH7_PRIVATE int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  575 | `{` |
|        - |  576 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - |  577 | `	int i;` |
|     7705 |  578 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - |  579 | `	/* Dump one or more expressions */` |
|    18553 |  580 | `	for( i = 0 ; i < nArg ; i++ ){` |
|    10853 |  581 | `		ph7_value *pObj = apArg[i];` |
|        - |  582 | `		/* Reset the working buffer */` |
|    10853 |  583 | `		SyBlobReset(&sDump);` |
|        - |  584 | `		/* Dump the given expression */` |
|    10853 |  585 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - |  586 | `		/* Output */` |
|    10853 |  587 | `		if( SyBlobLength(&sDump) > 0 ){` |
|    10853 |  588 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|     5424 |  589 | `		}` |
|     5429 |  590 | `	}` |
|        - |  591 | `	/* Release the working buffer */` |
|     7705 |  592 | `	SyBlobRelease(&sDump);` |
|     7705 |  593 | `	return SXRET_OK;` |
|        5 |  594 | `}` |
|        - |  595 | `/*` |
|        - |  596 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - |  597 | ` *   print-r - Prints human-readable information about a variable` |
|        - |  598 | ` * Parameters` |
|        - |  599 | ` *   expression: Expression to dump` |
|        - |  600 | ` *   return : If you would like to capture the output of print_r() use` |
|        - |  601 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - |  602 | ` *            print_r() will return the information rather than print it.` |
|        - |  603 | ` * Return` |
|        - |  604 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - |  605 | ` *  Otherwise, the return value is TRUE.` |
|        - |  606 | ` */` |
|      254 |  607 | `PH7_PRIVATE int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  608 | `{` |
|      259 |  609 | `	int ret_string = 0;` |
|        - |  610 | `	SyBlob sDump;` |
|      259 |  611 | `	if( nArg < 1 ){` |
|        - |  612 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  613 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  614 | `		return SXRET_OK;` |
|        - |  615 | `	}` |
|      259 |  616 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|      259 |  617 | `	if ( nArg > 1 ){` |
|        - |  618 | `		/* Where to redirect output */` |
|       21 |  619 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        9 |  620 | `	}` |
|        - |  621 | `	/* Generate dump */` |
|      259 |  622 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|      259 |  623 | `	if( !ret_string ){` |
|        - |  624 | `		/* Output dump */` |
|      241 |  625 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  626 | `		/* Return true */` |
|      241 |  627 | `		ph7_result_bool(pCtx,1);` |
|      123 |  628 | `	}else{` |
|        - |  629 | `		/* Generated dump as return value */` |
|       21 |  630 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  631 | `	}` |
|        - |  632 | `	/* Release the working buffer */` |
|      259 |  633 | `	SyBlobRelease(&sDump);` |
|      259 |  634 | `	return SXRET_OK;` |
|      132 |  635 | `}` |
|        - |  636 | `/*` |
|        - |  637 | ` * var_export() — PHP-exact evaluable representation of a value.` |
|        - |  638 | ` *` |
|        - |  639 | ` * Distinct from print_r/var_dump (which share PH7_MemObjDump): var_export emits` |
|        - |  640 | ` * parseable PHP — 'single-quoted' strings, true/false/NULL, array (\n  k => v,\n),` |
|        - |  641 | ` * and \Class::__set_state(array(...)) for objects. PHP's indentation is quirky` |
|        - |  642 | ` * (2-space array entries; 3-space object property lines; a composite value always` |
|        - |  643 | ` * opens at containerIndent+2) but deterministic; this matches it byte-for-byte.` |
|        - |  644 | ` */` |
|        - |  645 | `/* Instance flag (distinct from oo.c's CLASS_INSTANCE_DESTROYED 0x001) marking an` |
|        - |  646 | ` * object currently on the var_export recursion stack, for cycle detection. */` |
|        - |  647 | `#define VM_INSTANCE_DUMPING 0x002` |
|        - |  648 | `typedef struct VmExportCtx VmExportCtx;` |
|        - |  649 | `struct VmExportCtx` |
|        - |  650 | `{` |
|        - |  651 | `	SyBlob *pOut;` |
|        - |  652 | `	int nIndent;  /* indentation of the container emitting this entry */` |
|        - |  653 | `	int depth;    /* recursion guard */` |
|        - |  654 | `	int bPresented; /* the entries are a native class's PRESENTED shape, which` |
|        - |  655 | `	                 * carries the object's own table MANGLED. var_export prints a` |
|        - |  656 | `	                 * non-public property under its PLAIN name (the attribute loop` |
|        - |  657 | `	                 * below already does), so the key is unmangled here too. */` |
|        - |  658 | `};` |
|        - |  659 | `static void VmExportValue(SyBlob *pOut, ph7_value *pVal, int nIndent, int depth);` |
|        - |  660 | `/* Append nIndent spaces. */` |
|     7188 |  661 | `static void VmExportIndent(SyBlob *pOut, int nIndent)` |
|        5 |  662 | `{` |
|        - |  663 | `	int i;` |
|    20345 |  664 | `	for( i = 0; i < nIndent; i++ ){ SyBlobAppend(pOut," ",1); }` |
|     7193 |  665 | `}` |
|        - |  666 | `/* Append a single-quoted PHP string literal (escapes ' and \, batching safe runs` |
|        - |  667 | ` * in one append). A NUL byte can't live in a single-quoted literal, so PHP splits` |
|        - |  668 | ` * it out as ' . "\0" . ' — match that. */` |
|     8994 |  669 | `static void VmExportQuoted(SyBlob *pOut, const char *z, int n)` |
|        5 |  670 | `{` |
|     8999 |  671 | `	int i, run = 0;` |
|     8999 |  672 | `	SyBlobAppend(pOut,"'",1);` |
|    71575 |  673 | `	for( i = 0; i < n; i++ ){` |
|    62581 |  674 | `		char c = z[i];` |
|    62581 |  675 | `		if( c != '\0' && c != '\'' && c != '\\' ){ continue; } /* extend the safe run */` |
|      493 |  676 | `		if( i > run ){ SyBlobAppend(pOut,&z[run],(sxu32)(i-run)); }` |
|      493 |  677 | `		if( c == '\0' ){ SyBlobAppend(pOut,"' . \"\\0\" . '",12); }` |
|      399 |  678 | `		else { SyBlobAppend(pOut,"\\",1); SyBlobAppend(pOut,&z[i],1); }` |
|      493 |  679 | `		run = i+1;` |
|      248 |  680 | `	}` |
|     8999 |  681 | `	if( n > run ){ SyBlobAppend(pOut,&z[run],(sxu32)(n-run)); }` |
|     8999 |  682 | `	SyBlobAppend(pOut,"'",1);` |
|     8999 |  683 | `}` |
|        - |  684 | `/* True if the array/object is already on the var_export recursion stack. */` |
|     4620 |  685 | `static int VmExportIsCycle(ph7_value *pVal)` |
|        5 |  686 | `{` |
|     4625 |  687 | `	if( ph7_value_is_array(pVal) ){` |
|      399 |  688 | `		return (((ph7_hashmap *)pVal->x.pOther)->iFlags & HASHMAP_DUMPING) != 0;` |
|        - |  689 | `	}` |
|     4229 |  690 | `	if( ph7_value_is_object(pVal) ){` |
|       23 |  691 | `		return (((ph7_class_instance *)pVal->x.pOther)->iFlags & VM_INSTANCE_DUMPING) != 0;` |
|        - |  692 | `	}` |
|     4209 |  693 | `	return 0;` |
|     2315 |  694 | `}` |
|        - |  695 | `/* Emit " => " then the value: a scalar inline, a composite on its own line at` |
|        - |  696 | ` * containerIndent+2. A circular reference renders inline as NULL (like PHP). */` |
|     4620 |  697 | `static void VmExportEntryValue(SyBlob *pOut, ph7_value *pVal, int nContainerIndent, int depth)` |
|        5 |  698 | `{` |
|     4625 |  699 | `	SyBlobAppend(pOut," => ",4);` |
|     4625 |  700 | `	if( VmExportIsCycle(pVal) ){` |
|        5 |  701 | `		SyBlobAppend(pOut,"NULL",4);` |
|     4623 |  702 | `	}else if( ph7_value_is_array(pVal) \|\| ph7_value_is_object(pVal) ){` |
|      416 |  703 | `		SyBlobAppend(pOut,"\n",1);` |
|      416 |  704 | `		VmExportIndent(pOut,nContainerIndent+2);` |
|      416 |  705 | `		VmExportValue(pOut,pVal,nContainerIndent+2,depth+1);` |
|      210 |  706 | `	}else{` |
|     4209 |  707 | `		VmExportValue(pOut,pVal,nContainerIndent+2,depth+1);` |
|        - |  708 | `	}` |
|     4625 |  709 | `	SyBlobAppend(pOut,",\n",2);` |
|     4625 |  710 | `}` |
|        - |  711 | `/* Array walker: "<indent+2>key => value,\n" for each entry. */` |
|     4528 |  712 | `static int VmExportArrayWalk(ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|        5 |  713 | `{` |
|     4533 |  714 | `	VmExportCtx *pC = (VmExportCtx *)pUserData;` |
|     4533 |  715 | `	VmExportIndent(pC->pOut,pC->nIndent+2);` |
|     4533 |  716 | `	if( ph7_value_is_string(pKey) ){` |
|        - |  717 | `		int n;` |
|      937 |  718 | `		const char *z = ph7_value_to_string(pKey,&n);` |
|      945 |  719 | `		if( pC->bPresented && n > 0 && z[0] == 0 ){` |
|        - |  720 | `			SyString sUnmCls, sUnmName;` |
|       17 |  721 | `			SyStringInitFromBuf(&sUnmName,z,n);` |
|       17 |  722 | `			PH7_UnmangleAttrName(z,(sxu32)n,&sUnmCls,&sUnmName);` |
|       17 |  723 | `			VmExportQuoted(pC->pOut,sUnmName.zString,(int)sUnmName.nByte);` |
|        9 |  724 | `		}else{` |
|      921 |  725 | `			VmExportQuoted(pC->pOut,z,n);` |
|        - |  726 | `		}` |
|      471 |  727 | `	}else{` |
|     3601 |  728 | `		SyBlobFormat(pC->pOut,"%qd",ph7_value_to_int64(pKey));` |
|        - |  729 | `	}` |
|     4533 |  730 | `	VmExportEntryValue(pC->pOut,pValue,pC->nIndent,pC->depth);` |
|     4533 |  731 | `	return PH7_OK;` |
|        5 |  732 | `}` |
|    25026 |  733 | `static void VmExportValue(SyBlob *pOut, ph7_value *pVal, int nIndent, int depth)` |
|        5 |  734 | `{` |
|    25031 |  735 | `	if( depth > 4096 ){ return; } /* backstop for pathological finite nesting */` |
|    25031 |  736 | `	if( ph7_value_is_null(pVal) ){` |
|     1007 |  737 | `		SyBlobAppend(pOut,"NULL",4);` |
|    24530 |  738 | `	}else if( ph7_value_is_bool(pVal) ){` |
|     4681 |  739 | `		if( ph7_value_to_bool(pVal) ){ SyBlobAppend(pOut,"true",4); }` |
|     2291 |  740 | `		else { SyBlobAppend(pOut,"false",5); }` |
|    21691 |  741 | `	}else if( ph7_value_is_float(pVal) ){` |
|        - |  742 | `		/* float before int: ph7_value_is_int is lenient (true for integer reals). */` |
|     1327 |  743 | `		sxu32 before = SyBlobLength(pOut), i, after;` |
|        - |  744 | `		const char *z;` |
|     1327 |  745 | `		int plain = 1;` |
|     1327 |  746 | `		PH7_AppendShortestReal(pOut,ph7_value_to_double(pVal));` |
|     1327 |  747 | `		z = (const char *)SyBlobData(pOut);` |
|     1327 |  748 | `		after = SyBlobLength(pOut);` |
|     3085 |  749 | `		for( i = before; i < after; i++ ){` |
|     2607 |  750 | `			if( !((z[i]>='0'&&z[i]<='9')\|\|z[i]=='-') ){ plain = 0; break; }` |
|      884 |  751 | `		}` |
|     1327 |  752 | `		if( plain ){ SyBlobAppend(pOut,".0",2); } /* integer-form floats render 1.0/100.0 */` |
|    18692 |  753 | `	}else if( ph7_value_is_int(pVal) ){` |
|     7919 |  754 | `		sxi64 iVal = ph7_value_to_int64(pVal);` |
|     7919 |  755 | `		if( iVal == SMALLEST_INT64 ){` |
|        - |  756 | `			/* php renders LONG_MIN as an expression: the positive literal` |
|        - |  757 | `			 * 9223372036854775808 would not be representable when eval'd. */` |
|       19 |  758 | `			SyBlobAppend(pOut,"-9223372036854775807-1",sizeof("-9223372036854775807-1")-1);` |
|       10 |  759 | `		}else{` |
|     7901 |  760 | `			SyBlobFormat(pOut,"%qd",iVal);` |
|        5 |  761 | `		}` |
|    14074 |  762 | `	}else if( ph7_value_is_string(pVal) ){` |
|        - |  763 | `		int n;` |
|     7967 |  764 | `		const char *z = ph7_value_to_string(pVal,&n);` |
|     7967 |  765 | `		VmExportQuoted(pOut,z,n);` |
|     6136 |  766 | `	}else if( ph7_value_is_array(pVal) ){` |
|     2021 |  767 | `		ph7_hashmap *pMap = (ph7_hashmap *)pVal->x.pOther;` |
|     2021 |  768 | `		if( pMap->iFlags & HASHMAP_DUMPING ){` |
|      ! 0 |  769 | `			SyBlobAppend(pOut,"NULL",4); /* circular reference -> NULL, like PHP */` |
|      ! 0 |  770 | `		}else{` |
|        - |  771 | `			VmExportCtx ctx;` |
|     2021 |  772 | `			ctx.bPresented = 0;` |
|     2021 |  773 | `			pMap->iFlags \|= HASHMAP_DUMPING;` |
|     2021 |  774 | `			SyBlobAppend(pOut,"array (\n",8);` |
|     2021 |  775 | `			ctx.pOut = pOut; ctx.nIndent = nIndent; ctx.depth = depth;` |
|     2021 |  776 | `			ph7_array_walk(pVal,VmExportArrayWalk,&ctx);` |
|     2021 |  777 | `			VmExportIndent(pOut,nIndent);` |
|     2021 |  778 | `			SyBlobAppend(pOut,")",1);` |
|     2021 |  779 | `			pMap->iFlags &= ~HASHMAP_DUMPING;` |
|        5 |  780 | `		}` |
|     1147 |  781 | `	}else if( ph7_value_is_object(pVal) ){` |
|      139 |  782 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|      139 |  783 | `		if( pThis->pClass->iFlags & PH7_CLASS_ENUM ){` |
|        - |  784 | ``			/* php 8.1: an enum case exports as `\S::A` */`` |
|        3 |  785 | `			ph7_value *pName = PH7_EnumCaseNameValue(pThis);` |
|        3 |  786 | `			SyBlobAppend(pOut,"\\",1);` |
|        3 |  787 | `			SyBlobAppend(pOut,pThis->pClass->sName.zString,pThis->pClass->sName.nByte);` |
|        3 |  788 | `			SyBlobAppend(pOut,"::",2);` |
|        3 |  789 | `			if( pName && SyBlobLength(&pName->sBlob) > 0 ){` |
|        3 |  790 | `				SyBlobAppend(pOut,SyBlobData(&pName->sBlob),SyBlobLength(&pName->sBlob));` |
|        2 |  791 | `			}` |
|      138 |  792 | `		}else if( pThis->iFlags & VM_INSTANCE_DUMPING ){` |
|      ! 0 |  793 | `			SyBlobAppend(pOut,"NULL",4); /* circular reference -> NULL, like PHP */` |
|      ! 0 |  794 | `		}else{` |
|      137 |  795 | `			SyString *pClassName = &pThis->pClass->sName;` |
|        - |  796 | `			SyHashEntry *pEntry;` |
|        - |  797 | `			SySet sNames;` |
|        - |  798 | `			SyString *aName;` |
|        - |  799 | `			sxu32 iName,nName;` |
|        - |  800 | ``			/* php exports a plain stdClass as a CAST — `(object) array(...)` — and`` |
|        - |  801 | `			 * every other class through __set_state(); a SUBCLASS of stdClass takes` |
|        - |  802 | `			 * the __set_state form, so this is the exact class and not an` |
|        - |  803 | `			 * inheritance test. The two forms differ by one closing paren. */` |
|      137 |  804 | `			int bStdObj = pThis->pClass == pThis->pVm->pStdClass;` |
|      137 |  805 | `			pThis->iFlags \|= VM_INSTANCE_DUMPING;` |
|      137 |  806 | `			if( bStdObj ){` |
|        3 |  807 | `				SyBlobAppend(pOut,"(object) array(\n",sizeof("(object) array(\n")-1);` |
|        2 |  808 | `			}else{` |
|      135 |  809 | `				SyBlobAppend(pOut,"\\",1);` |
|      135 |  810 | `				SyBlobAppend(pOut,pClassName->zString,pClassName->nByte);` |
|      135 |  811 | `				SyBlobAppend(pOut,"::__set_state(array(\n",21);` |
|        - |  812 | `			}` |
|        - |  813 | `			{` |
|        - |  814 | `				/* A native class's PRESENTATION (php's get_properties): var_export` |
|        - |  815 | `				 * shows a DateTime as date/timezone_type/timezone, the same shape` |
|        - |  816 | `				 * the (array) cast produces and NOT the hidden engine slots. A` |
|        - |  817 | `				 * debug-only hook (WeakReference) fills nothing, which is php's` |
|        - |  818 | `				 * empty export. */` |
|        - |  819 | `				ph7_value sPresent;` |
|      137 |  820 | `				ph7_hashmap *pPresent = PH7_NewHashmap(pThis->pVm,0,0);` |
|      137 |  821 | `				PH7_MemObjInit(pThis->pVm,&sPresent);` |
|      137 |  822 | `				if( pPresent ){` |
|      137 |  823 | `					sPresent.x.pOther = pPresent;` |
|      137 |  824 | `					MemObjSetType(&sPresent,MEMOBJ_HASHMAP);` |
|      137 |  825 | `					if( PH7_ClassInstancePresent(pThis,&sPresent,0) ){` |
|        - |  826 | `						/* Same line shape the attribute loop below produces: an` |
|        - |  827 | `						 * object body's entries sit one deeper than an array's. */` |
|        - |  828 | `						VmExportCtx sCtx;` |
|       66 |  829 | `						sCtx.pOut = pOut;` |
|       66 |  830 | `						sCtx.nIndent = nIndent + 1;` |
|       66 |  831 | `						sCtx.depth = depth;` |
|       66 |  832 | `						sCtx.bPresented = 1;` |
|       66 |  833 | `						ph7_array_walk(&sPresent,VmExportArrayWalk,&sCtx);` |
|       66 |  834 | `						PH7_MemObjRelease(&sPresent);` |
|       66 |  835 | `						VmExportIndent(pOut,nIndent);` |
|       66 |  836 | `						SyBlobAppend(pOut,bStdObj ? ")" : "))",bStdObj ? 1 : 2);` |
|       66 |  837 | `						pThis->iFlags &= ~VM_INSTANCE_DUMPING;` |
|       66 |  838 | `						return;` |
|        - |  839 | `					}` |
|       73 |  840 | `					PH7_MemObjRelease(&sPresent);` |
|       34 |  841 | `				}` |
|        - |  842 | `			}` |
|        - |  843 | `			/* SNAPSHOT the attribute names first: a PHP 8.4 get hook dispatched` |
|        - |  844 | `			 * mid-walk may re-enter an hAttr walk on this instance (the hash has` |
|        - |  845 | `			 * a single embedded loop cursor) or unset()/create properties; names` |
|        - |  846 | `			 * point into class-owned attr storage and each is re-looked-up. */` |
|       73 |  847 | `			SySetInit(&sNames,&pThis->pVm->sAllocator,sizeof(SyString));` |
|       73 |  848 | `			SyHashResetLoopCursor(&pThis->hAttr);` |
|      239 |  849 | `			while( (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|      171 |  850 | `				VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|      171 |  851 | `				if( PH7_ATTR_UNPRESENTED(pVmAttr) ){ continue; }` |
|      125 |  852 | `				if( PH7_ClassAttrUninitializedForRead(pVmAttr) ){` |
|       22 |  853 | `					continue; /* typed, never written: not there yet (php) */` |
|        - |  854 | `				}` |
|      100 |  855 | `				if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_VIRTUAL))` |
|       55 |  856 | `				 == PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|      ! 0 |  857 | `					continue; /* virtual set-only property: no value to export (php) */` |
|        - |  858 | `				}` |
|      105 |  859 | `				SySetPut(&sNames,(const void *)&pVmAttr->pAttr->sName);` |
|        5 |  860 | `			}` |
|       73 |  861 | `			aName = (SyString *)SySetBasePtr(&sNames);` |
|       73 |  862 | `			nName = SySetUsed(&sNames);` |
|      173 |  863 | `			for( iName = 0 ; iName < nName ; ++iName ){` |
|      105 |  864 | `				SyString *pAName = &aName[iName];` |
|        - |  865 | `				VmClassAttr *pVmAttr;` |
|        - |  866 | `				ph7_value *pAttrVal;` |
|      105 |  867 | `				pEntry = PH7_ClassInstanceAttrEntry(pThis,pAName->zString,pAName->nByte);` |
|      105 |  868 | `				if( pEntry == 0 ){ continue; } /* unset by an earlier hook */` |
|      105 |  869 | `				pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|      105 |  870 | `				VmExportIndent(pOut,nIndent+3); /* object property lines sit one deeper than arrays */` |
|      107 |  871 | `				if( pAName->nByte > 0 && pAName->zString[0] == 0 ){` |
|        - |  872 | `					/* A MANGLED key stored raw (the __PHP_Incomplete_Class carrier):` |
|        - |  873 | `					 * php's var_export prints the PLAIN name ('bp' => 1). */` |
|        - |  874 | `					SyString sUnmCls, sUnmName;` |
|        5 |  875 | `					SyStringInitFromBuf(&sUnmName,pAName->zString,pAName->nByte);` |
|        5 |  876 | `					PH7_UnmangleAttrName(pAName->zString,pAName->nByte,&sUnmCls,&sUnmName);` |
|        5 |  877 | `					VmExportQuoted(pOut,sUnmName.zString,(int)sUnmName.nByte);` |
|        3 |  878 | `				}else{` |
|      101 |  879 | `					VmExportQuoted(pOut,pAName->zString,(int)pAName->nByte);` |
|        - |  880 | `				}` |
|        - |  881 | `				/* PHP 8.4 property hooks: var_export() reads through the get hook` |
|        - |  882 | `				 * (every visibility — php exports private hooked values too). A` |
|        - |  883 | `				 * throwing hook parks on the boundary rail; the helper's boundary` |
|        - |  884 | `				 * gate keeps LATER hooks from running (the raw values the tail of` |
|        - |  885 | `				 * the export falls back to are discarded when the throw routes). */` |
|        - |  886 | `				{` |
|        - |  887 | `					ph7_value sHookVal;` |
|        - |  888 | `					sxi32 rcHk;` |
|      105 |  889 | `					PH7_MemObjInit(pThis->pVm,&sHookVal);` |
|      105 |  890 | `					rcHk = PH7_VmHookGetAttrValue(pThis,pVmAttr,&sHookVal);` |
|      105 |  891 | `					if( rcHk == SXRET_OK ){` |
|       16 |  892 | `						VmExportEntryValue(pOut,&sHookVal,nIndent,depth);` |
|       16 |  893 | `						PH7_MemObjRelease(&sHookVal);` |
|       20 |  894 | `						continue;` |
|        - |  895 | `					}` |
|       91 |  896 | `					PH7_MemObjRelease(&sHookVal);` |
|       91 |  897 | `					if( rcHk != SXERR_NOTFOUND ){` |
|        - |  898 | `						/* the hook threw (parked on the boundary rail): NULL` |
|        - |  899 | `						 * placeholder keeps the output well-formed */` |
|       10 |  900 | `						SyBlobAppend(pOut," => NULL,\n",10);` |
|       10 |  901 | `						continue;` |
|        - |  902 | `					}` |
|        - |  903 | `				}` |
|       83 |  904 | `				pAttrVal = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       83 |  905 | `				if( pAttrVal ){ VmExportEntryValue(pOut,pAttrVal,nIndent,depth); }` |
|      ! 0 |  906 | `				else { SyBlobAppend(pOut," => NULL,\n",10); }` |
|       44 |  907 | `			}` |
|       73 |  908 | `			SySetRelease(&sNames);` |
|       73 |  909 | `			VmExportIndent(pOut,nIndent);` |
|       73 |  910 | `			SyBlobAppend(pOut,bStdObj ? ")" : "))",bStdObj ? 1 : 2);` |
|       73 |  911 | `			pThis->iFlags &= ~VM_INSTANCE_DUMPING;` |
|        - |  912 | `		}` |
|       40 |  913 | `	}else{` |
|        - |  914 | `		/* resource / other -> PHP emits NULL (with a warning we omit) */` |
|      ! 0 |  915 | `		SyBlobAppend(pOut,"NULL",4);` |
|        - |  916 | `	}` |
|    12518 |  917 | `}` |
|        - |  918 | `/*` |
|        - |  919 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - |  920 | ` *  PHP-exact evaluable representation (see VmExportValue).` |
|        - |  921 | ` */` |
|    20410 |  922 | `PH7_PRIVATE int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  923 | `{` |
|    20415 |  924 | `	int ret_string = 0;` |
|        - |  925 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|    20415 |  926 | `	if( nArg < 1 ){` |
|        - |  927 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  928 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  929 | `		return SXRET_OK;` |
|        - |  930 | `	}` |
|    20415 |  931 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|    20415 |  932 | `	if ( nArg > 1 ){` |
|        - |  933 | `		/* Where to redirect output */` |
|    19695 |  934 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|     9845 |  935 | `	}` |
|        - |  936 | `	/* Generate the PHP-exact evaluable representation */` |
|    20415 |  937 | `	VmExportValue(&sDump,apArg[0],0,0);` |
|    20415 |  938 | `	if( PH7_CALLBACK_UNWOUND(pCtx->pVm->nBoundaryRc) ){` |
|        - |  939 | ``		/* A php 8.4 `get` hook threw (or exited) part-way through: php's var_export`` |
|        - |  940 | `		 * builds its string before it prints anything, so a throw from inside it` |
|        - |  941 | ``		 * reaches the caller with NOTHING written. The `$return = true` form was`` |
|        - |  942 | `		 * already right — the parked throw discards the RESULT — but the printing` |
|        - |  943 | `		 * form had already handed the half-built text to the output layer, so a` |
|        - |  944 | `		 * caught throw was followed by an export naming properties as NULL. */` |
|       10 |  945 | `		SyBlobRelease(&sDump);` |
|       10 |  946 | `		ph7_result_null(pCtx);` |
|       10 |  947 | `		return SXRET_OK;` |
|        - |  948 | `	}` |
|    20407 |  949 | `	if( !ret_string ){` |
|        - |  950 | `		/* Output dump */` |
|      719 |  951 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  952 | `		/* Return NULL */` |
|      719 |  953 | `		ph7_result_null(pCtx);` |
|      362 |  954 | `	}else{` |
|        - |  955 | `		/* Generated dump as return value */` |
|    19693 |  956 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  957 | `	}` |
|        - |  958 | `	/* Release the working buffer */` |
|    20407 |  959 | `	SyBlobRelease(&sDump);` |
|    20407 |  960 | `	return SXRET_OK;` |
|    10210 |  961 | `}` |
|        - |  962 |  |
