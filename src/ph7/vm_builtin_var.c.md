# src/ph7/vm_builtin_var.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 471/510 lines (92.35%)

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
|   132328 |   23 | `PH7_PRIVATE int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |   24 | `{` |
|        - |   25 | `	ph7_value *pObj;` |
|   132333 |   26 | `	int res = 0;` |
|        - |   27 | `	int i;` |
|   132333 |   28 | `	if( nArg < 1 ){` |
|        - |   29 | `		/* Missing arguments,return false */` |
|      ! 0 |   30 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 |   31 | `		return SXRET_OK;` |
|        - |   32 | `	}` |
|        - |   33 | `	/* Iterate over available arguments */` |
|   163961 |   34 | `	for( i = 0 ; i < nArg ; ++i ){` |
|   132345 |   35 | `		pObj = apArg[i];` |
|   132345 |   36 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|        - |   37 | `			/* Skip the "expecting a variable" warning for MEMOBJ_BOOL —` |
|        - |   38 | `			 * synthesized by LOAD_IDX iP2=4 (ArrayAccess::offsetExists) and` |
|        - |   39 | `			 * by anyone passing a bool literal (rare, harmless). */` |
|   100735 |   40 | `			if( (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|        - |   41 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 |   42 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 |   43 | `			}` |
|    50365 |   44 | `		}` |
|   132345 |   45 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|   132345 |   46 | `		if( !res ){` |
|        - |   47 | `			/* Variable not set,return FALSE */` |
|   100717 |   48 | `			ph7_result_bool(pCtx,0);` |
|   100717 |   49 | `			return SXRET_OK;` |
|        - |   50 | `		}` |
|    15819 |   51 | `	}` |
|        - |   52 | `	/* All given variable are set,return TRUE */` |
|    31621 |   53 | `	ph7_result_bool(pCtx,1);` |
|    31621 |   54 | `	return SXRET_OK;` |
|    66169 |   55 | `}` |
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
|     7554 |   82 | `PH7_PRIVATE sxi32 VmUnsetVarByNameEx(ph7_vm *pVm,VmFrame *pFrame,const char *zName,sxu32 nByte,` |
|        - |   83 | `	int bNameGuard)` |
|        5 |   84 | `{` |
|        - |   85 | `	SyHashEntry *pEntry;` |
|        - |   86 | `	VmRefObj *pRef;` |
|        - |   87 | `	sxu32 nIdx;` |
|        - |   88 | `	/* php 8.1 forbids unset($GLOBALS) outright. Checked by NAME: the superglobal is not an` |
|        - |   89 | `	 * ordinary hVar binding, so the slot-index test below never sees it. */` |
|     7559 |   90 | `	if( bNameGuard && nByte == sizeof("GLOBALS")-1 && SyMemcmp(zName,"GLOBALS",nByte) == 0 ){` |
|        3 |   91 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |   92 | `			"$GLOBALS can only be modified using the $GLOBALS[$name] = $value syntax");` |
|        3 |   93 | `		pVm->iExitStatus = 255;` |
|        3 |   94 | `		pVm->bHaltRequested = 1;` |
|        3 |   95 | `		return PH7_ABORT;` |
|        - |   96 | `	}` |
|     7557 |   97 | `	pEntry = SyHashGet(&pFrame->hVar,(const void *)zName,nByte);` |
|     7557 |   98 | `	if( pEntry == 0 ){` |
|        - |   99 | `		/* No such variable: unset() is a no-op on an undefined name, as in php */` |
|     1107 |  100 | `		return SXRET_OK;` |
|        - |  101 | `	}` |
|     6455 |  102 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|     6455 |  103 | `	if( nIdx == pVm->nGlobalIdx ){` |
|      ! 0 |  104 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  105 | `			"$GLOBALS can only be modified using the $GLOBALS[$name] = $value syntax");` |
|      ! 0 |  106 | `		pVm->iExitStatus = 255;` |
|      ! 0 |  107 | `		pVm->bHaltRequested = 1;` |
|      ! 0 |  108 | `		return PH7_ABORT;` |
|        - |  109 | `	}` |
|     6455 |  110 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|        - |  111 | `	/*` |
|        - |  112 | `	 * A GLOBAL variable is ALSO an entry of the $GLOBALS array, and that node points at the` |
|        - |  113 | `	 * same slot. php's unset($x) in global scope drops $GLOBALS['x'] too, so unlink the node` |
|        - |  114 | `	 * — otherwise it stays a live holder and the value is never released ($o = new D;` |
|        - |  115 | `	 * unset($o); stopped running the destructor). Clear the reference table's row for the` |
|        - |  116 | `	 * node BEFORE unlinking it: unlinking frees the node, and the holder count below would` |
|        - |  117 | `	 * otherwise dereference freed memory.` |
|        - |  118 | `	 */` |
|     6455 |  119 | `	if( pFrame->pParent == 0 ){` |
|     6435 |  120 | `		ph7_value *pGlobals = (ph7_value *)SySetAt(&pVm->aMemObj,pVm->nGlobalIdx);` |
|     6435 |  121 | `		if( pGlobals && (pGlobals->iFlags & MEMOBJ_HASHMAP) ){` |
|     6435 |  122 | `			ph7_hashmap_node *pNode = 0;` |
|        - |  123 | `			ph7_value sKey;` |
|        - |  124 | `			SyString sName;` |
|     6435 |  125 | `			PH7_MemObjInit(&(*pVm),&sKey);` |
|     6435 |  126 | `			SyStringInitFromBuf(&sName,zName,nByte);` |
|     6435 |  127 | `			PH7_MemObjInitFromString(&(*pVm),&sKey,&sName);` |
|     6430 |  128 | `			if( SXRET_OK == PH7_HashmapLookup((ph7_hashmap *)pGlobals->x.pOther,&sKey,&pNode)` |
|     6430 |  129 | `			 && pNode && pNode->nValIdx == nIdx ){` |
|     6425 |  130 | `				if( pRef ){` |
|     6425 |  131 | `					ph7_hashmap_node **apN = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|        - |  132 | `					sxu32 k;` |
|    13077 |  133 | `					for( k = 0 ; k < SySetUsed(&pRef->aArrEntries) ; ++k ){` |
|     6657 |  134 | `						if( apN[k] == pNode ){` |
|     6425 |  135 | `							apN[k] = 0;` |
|     3210 |  136 | `						}` |
|     3331 |  137 | `					}` |
|     3210 |  138 | `				}` |
|     6425 |  139 | `				PH7_HashmapUnlinkNode(pNode,FALSE);` |
|     3210 |  140 | `			}` |
|     6435 |  141 | `			PH7_MemObjRelease(&sKey);` |
|     3215 |  142 | `		}` |
|     3215 |  143 | `	}` |
|        - |  144 |  |
|        - |  145 | `	/* The frame's own "release this reference at exit" row for the binding about to go:` |
|        - |  146 | `	 * the entry is freed below, so the row would dangle (and a foreach value variable` |
|        - |  147 | `	 * unset once per loop filed one row per loop, which nothing consumed until the` |
|        - |  148 | `	 * function returned). */` |
|     6455 |  149 | `	VmDropFrameRefEntry(&(*pVm),nIdx,pEntry);` |
|     6455 |  150 | `	if( pRef == 0 ){` |
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
|     6455 |  161 | `		SyHashEntry **apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - |  162 | `		sxu32 n;` |
|        - |  163 | `		/* Forget THIS name in the slot's reference record (leave the others alone) */` |
|    13037 |  164 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; ++n ){` |
|     6587 |  165 | `			if( apEntry[n] == pEntry ){` |
|     6443 |  166 | `				apEntry[n] = 0;` |
|     3219 |  167 | `			}` |
|     3296 |  168 | `		}` |
|     6455 |  169 | `		SyHashDeleteEntry2(pEntry);` |
|        - |  170 | `		/* The value goes with the LAST holder and not before — the one rule, counted in` |
|        - |  171 | `		 * one place: other names, array nodes that still point here, and a PIN (a static's` |
|        - |  172 | ``		 * storage, a `use (&$x)` capture, a reference-bound property), which is a holder`` |
|        - |  173 | `		 * this table cannot name. Unsetting the name of a static used to release the` |
|        - |  174 | `		 * static's value, so the next call started over from the initializer. */` |
|     6455 |  175 | `		PH7_VmReleaseUnheldSlot(&(*pVm),nIdx);` |
|        - |  176 | `	}` |
|     6455 |  177 | `	return SXRET_OK;` |
|     3782 |  178 | `}` |
|     7400 |  179 | `PH7_PRIVATE sxi32 VmUnsetVarByName(ph7_vm *pVm,VmFrame *pFrame,const char *zName,sxu32 nByte)` |
|        5 |  180 | `{` |
|     7405 |  181 | `	return VmUnsetVarByNameEx(&(*pVm),pFrame,zName,nByte,TRUE);` |
|        5 |  182 | `}` |
| 18111196 |  183 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        5 |  184 | `{` |
|        - |  185 | `	ph7_value *pObj;` |
|        - |  186 | `	VmRefObj *pRef;` |
| 18111201 |  187 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
| 18111201 |  188 | `	if( pObj ){` |
|        - |  189 | `		/* Release the object */` |
| 18111201 |  190 | `		PH7_MemObjRelease(pObj);` |
|  9056870 |  191 | `	}` |
|        - |  192 | `	/* Remove old reference links */` |
| 18111201 |  193 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
| 18111201 |  194 | `	if( pRef ){` |
| 18111197 |  195 | `		sxi32 iFlags = pRef->iFlags;` |
|        - |  196 | `		/* Unlink from the reference table */` |
| 18111197 |  197 | `		VmRefObjUnlink(&(*pVm),pRef);` |
| 18111197 |  198 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - |  199 | `			VmSlot sFree;` |
|        - |  200 | `			/* Restore to the free list */` |
| 18111197 |  201 | `			sFree.nIdx = nObjIdx;` |
| 18111197 |  202 | `			sFree.pUserData = 0;` |
| 18111197 |  203 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  9056868 |  204 | `		}` |
|  9056868 |  205 | `	}` |
| 18111201 |  206 | `	return SXRET_OK;` |
|        5 |  207 | `}` |
|        - |  208 | `/*` |
|        - |  209 | ` * void unset($var,...)` |
|        - |  210 | ` *   Unset one or more given variable.` |
|        - |  211 | ` * Parameters` |
|        - |  212 | ` *  One or more variable to unset.` |
|        - |  213 | ` * Return` |
|        - |  214 | ` *  Nothing.` |
|        - |  215 | ` */` |
|     1254 |  216 | `PH7_PRIVATE int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  217 | `{` |
|        - |  218 | `	ph7_value *pObj;` |
|        - |  219 | `	ph7_vm *pVm;` |
|        - |  220 | `	int i;` |
|        - |  221 | `	/* Point to the target VM */` |
|     1259 |  222 | `	pVm = pCtx->pVm;` |
|        - |  223 | `	/* Iterate and unset */` |
|     2513 |  224 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     1259 |  225 | `		pObj = apArg[i];` |
|     1259 |  226 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|     1257 |  227 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  228 | `				/* Throw an error */` |
|      ! 0 |  229 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 |  230 | `			}` |
|      631 |  231 | `		}else{` |
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
|      632 |  247 | `	}` |
|     1259 |  248 | `	return SXRET_OK;` |
|      632 |  249 | `}` |
|        - |  250 | `/*` |
|        - |  251 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - |  252 | ` */` |
|     5864 |  253 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        5 |  254 | `{` |
|     5869 |  255 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|     5869 |  256 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  257 | `	ph7_value *pObj;` |
|        - |  258 | `	sxu32 nIdx;` |
|        - |  259 | `	/* php excludes $this from get_defined_vars() (it is bound implicitly, not a` |
|        - |  260 | `	 * declared local). PHL keeps it in the frame's hVar, so skip it here. */` |
|     5864 |  261 | `	if( pEntry->nKeyLen == sizeof("this")-1` |
|     3470 |  262 | `	 && SyMemcmp(pEntry->pKey,"this",sizeof("this")-1) == 0 ){` |
|        6 |  263 | `		return SXRET_OK;` |
|        - |  264 | `	}` |
|        - |  265 | `	/* Engine temporaries (a foreach destructuring/target slot) are not variables the` |
|        - |  266 | `	 * program declared — php compiles those into slots with no name at all. */` |
|     5865 |  267 | `	if( PH7_VmVarNameIsInternal((const char *)pEntry->pKey,pEntry->nKeyLen) ){` |
|      373 |  268 | `		return SXRET_OK;` |
|        - |  269 | `	}` |
|        - |  270 | `	/* Extract the memory object */` |
|     5493 |  271 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|     5493 |  272 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     5493 |  273 | `	if( pObj ){` |
|     5493 |  274 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|     5481 |  275 | `			if( pEntry->nKeyLen > 0 ){` |
|        - |  276 | `				SyString sName;` |
|        - |  277 | `				ph7_value sKey;` |
|        - |  278 | `				/* Perform the insertion (pObj may point into pVm->aMemObj; the` |
|        - |  279 | `				 * inserter snapshots the source before reserving, so the pool may` |
|        - |  280 | `				 * safely move underneath it — see HashmapInsertIntKey/BlobKey). */` |
|     5481 |  281 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|     5481 |  282 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|     5481 |  283 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|     5481 |  284 | `				PH7_MemObjRelease(&sKey);` |
|     2738 |  285 | `			}` |
|     2738 |  286 | `		}` |
|     2744 |  287 | `	}` |
|     5493 |  288 | `	return SXRET_OK;` |
|     2937 |  289 | `}` |
|        - |  290 | `/*` |
|        - |  291 | ` * array get_defined_vars(void)` |
|        - |  292 | ` *  Returns an array of all defined variables.` |
|        - |  293 | ` * Parameter` |
|        - |  294 | ` *  None` |
|        - |  295 | ` * Return` |
|        - |  296 | ` *  An array with all the variables defined in the current scope.` |
|        - |  297 | ` */` |
|       70 |  298 | `PH7_PRIVATE int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  299 | `{` |
|       75 |  300 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  301 | `	ph7_value *pArray;` |
|        - |  302 | `	VmFrame *pFrame;` |
|        - |  303 | `	/* Create a new array */` |
|       75 |  304 | `	pArray = ph7_context_new_array(pCtx);` |
|       75 |  305 | ` 	if( pArray == 0 ){` |
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
|       75 |  318 | `	pFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|        - |  319 | `	/* Superglobals appear in get_defined_vars() ONLY at the global scope (php).` |
|        - |  320 | `	 * Inside a function the result is the local symbol table alone — a leak of` |
|        - |  321 | `	 * $argv/$_SERVER/... into every function's scope breaks callers that treat the` |
|        - |  322 | `	 * keys as real locals (e.g. PHPUnit's doubled-method template reflects each` |
|        - |  323 | `	 * get_defined_vars() name as a ReflectionParameter). */` |
|       75 |  324 | `	if( pFrame->pParent == 0 ){` |
|       14 |  325 | `		SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        6 |  326 | `	}` |
|        - |  327 | `	/* Then variables defined in the current frame, in DECLARATION order.` |
|        - |  328 | `	 * The frame table is head-pushed (SyHashInsert), so its forward order is` |
|        - |  329 | `	 * reverse-insertion; walk it backward to match php, which returns locals in` |
|        - |  330 | `	 * the order they first appeared (a,b,c — a reassignment reuses the slot and` |
|        - |  331 | `	 * keeps its original position). */` |
|       75 |  332 | `	SyHashForEachReverse(&pFrame->hVar,VmHashVarWalker,pArray);` |
|        - |  333 | `	/* Finally,return the created array */` |
|       75 |  334 | `	ph7_result_value(pCtx,pArray);` |
|       75 |  335 | `	return SXRET_OK;` |
|       40 |  336 | `}` |
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
|       38 |  347 | `PH7_PRIVATE int vm_builtin_get_debug_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  348 | `{` |
|       39 |  349 | `	const char *zType = "null";` |
|       39 |  350 | `	if( nArg > 0 ){` |
|       39 |  351 | `		ph7_value *pVal = apArg[0];` |
|       39 |  352 | `		if( pVal->iFlags & MEMOBJ_OBJ ){` |
|        5 |  353 | `			ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|        7 |  354 | `			ph7_result_string(pCtx,SyStringData(&pThis->pClass->sName),` |
|        4 |  355 | `				(int)SyStringLength(&pThis->pClass->sName));` |
|        5 |  356 | `			return SXRET_OK;` |
|        - |  357 | `		}` |
|       35 |  358 | `		if( pVal->iFlags & MEMOBJ_NULL ){` |
|        3 |  359 | `			zType = "null";` |
|       34 |  360 | `		}else if( pVal->iFlags & MEMOBJ_REAL ){` |
|        - |  361 | `			/* REAL wins over a cached MEMOBJ_INT, as it does for gettype() */` |
|        9 |  362 | `			zType = "float";` |
|       29 |  363 | `		}else if( pVal->iFlags & MEMOBJ_INT ){` |
|       17 |  364 | `			zType = "int";` |
|       17 |  365 | `		}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|        3 |  366 | `			zType = "string";` |
|        8 |  367 | `		}else if( pVal->iFlags & MEMOBJ_BOOL ){` |
|        3 |  368 | `			zType = "bool";` |
|        6 |  369 | `		}else if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|        3 |  370 | `			zType = "array";` |
|        4 |  371 | `		}else if( pVal->iFlags & MEMOBJ_RES ){` |
|        - |  372 | `			/* php names the stream kind here; PHL reports what gettype() does,` |
|        - |  373 | `			 * which is the same gap gettype() already has (§7.4). */` |
|        3 |  374 | `			zType = PH7_VfsResourceIsClosed(pVal->x.pOther) ? "resource (closed)" : "resource";` |
|        1 |  375 | `		}` |
|       17 |  376 | `	}` |
|       35 |  377 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       35 |  378 | `	return SXRET_OK;` |
|       20 |  379 | `}` |
|        - |  380 | `/*` |
|        - |  381 | ` * bool gettype($var)` |
|        - |  382 | ` *  Get the type of a variable` |
|        - |  383 | ` * Parameters` |
|        - |  384 | ` *   $var` |
|        - |  385 | ` *    The variable being type checked.` |
|        - |  386 | ` * Return` |
|        - |  387 | ` *   String representation of the given variable type.` |
|        - |  388 | ` */` |
|      176 |  389 | `PH7_PRIVATE int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  390 | `{` |
|        - |  391 | `	/* php's gettype() uses LONG type names ("integer"/"boolean"/"NULL"), distinct` |
|        - |  392 | `	 * from get_debug_type()'s short ones ("int"/"bool"/"null") — never route this` |
|        - |  393 | `	 * through PH7_MemObjTypeDump, which yields the short forms. */` |
|      181 |  394 | `	const char *zType = "unknown type";` |
|      181 |  395 | `	if( nArg > 0 ){` |
|      181 |  396 | `		ph7_value *pVal = apArg[0];` |
|      181 |  397 | `		if( pVal->iFlags & MEMOBJ_NULL ){` |
|        3 |  398 | `			zType = "NULL";` |
|      180 |  399 | `		}else if( pVal->iFlags & MEMOBJ_REAL ){` |
|        - |  400 | `			/* REAL wins over a cached MEMOBJ_INT: 1.0 is "double" (php). */` |
|        9 |  401 | `			zType = "double";` |
|      175 |  402 | `		}else if( pVal->iFlags & MEMOBJ_INT ){` |
|       16 |  403 | `			zType = "integer";` |
|      163 |  404 | `		}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|      112 |  405 | `			zType = "string";` |
|      100 |  406 | `		}else if( pVal->iFlags & MEMOBJ_BOOL ){` |
|        5 |  407 | `			zType = "boolean";` |
|       43 |  408 | `		}else if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|       12 |  409 | `			zType = "array";` |
|       35 |  410 | `		}else if( pVal->iFlags & MEMOBJ_OBJ ){` |
|       15 |  411 | `			zType = "object";` |
|       23 |  412 | `		}else if( pVal->iFlags & MEMOBJ_RES ){` |
|        - |  413 | `			/* php reports an fclose()'d handle as "resource (closed)" */` |
|       16 |  414 | `			zType = PH7_VfsResourceIsClosed(pVal->x.pOther) ? "resource (closed)" : "resource";` |
|        7 |  415 | `		}` |
|       88 |  416 | `	}` |
|        - |  417 | `	/* Return the variable type */` |
|      181 |  418 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|      181 |  419 | `	return SXRET_OK;` |
|        5 |  420 | `}` |
|        - |  421 | `/*` |
|        - |  422 | ` * bool settype(mixed &$var, string $type)` |
|        - |  423 | ` *  Convert $var IN PLACE to the type named by $type, mirroring the (type) cast` |
|        - |  424 | ` *  operators exactly (same PH7_MemObjTo* helpers CVT_INT/REAL/STR/BOOL/ARRAY/OBJ` |
|        - |  425 | ` *  use), and write the result back through the by-ref out-param.` |
|        - |  426 | ` * Parameters` |
|        - |  427 | ` *   &$var : the variable to convert (compile.c's GenStateByRefBuiltinMask marks` |
|        - |  428 | ` *           position 0 by-reference so an undefined variable is auto-vivified).` |
|        - |  429 | ` *   $type : "int"/"integer", "float"/"double", "string", "bool"/"boolean",` |
|        - |  430 | ` *           "array", "object", "null" (case-insensitive).` |
|        - |  431 | ` * Return` |
|        - |  432 | ` *   Always true on success; a non-referenceable $var is an Error, an unknown` |
|        - |  433 | ` *   $type (or the un-castable "resource") is a ValueError — php-exact.` |
|        - |  434 | ` */` |
|       38 |  435 | `PH7_PRIVATE int vm_builtin_settype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 |  436 | `{` |
|        - |  437 | `	const char *zType;` |
|        - |  438 | `	int nLen;` |
|        - |  439 | `	ph7_value *pNew;` |
|        9 |  440 | `	SXUNUSED(nArg); /* arity (exactly 2) enforced by the central arity table */` |
|        - |  441 | `	/* php binds $var by reference at the CALL, and the refusal is the call site's` |
|        - |  442 | `	 * to raise (PH7_VmScreenByRefArgShapes) — only the compiler can tell a literal,` |
|        - |  443 | `	 * which php refuses, from the result of a call, which php accepts with a notice` |
|        - |  444 | ``	 * and converts in place on the temporary. The `nIdx == SXU32_HIGH` test that`` |
|        - |  445 | `	 * used to sit here conflated the two. */` |
|       41 |  446 | `	zType = ph7_value_to_string(apArg[1],&nLen);` |
|        - |  447 | `	/* Validate the target type up-front (php checks $type before touching $var):` |
|        - |  448 | `	 * "resource" is a distinct ValueError, any other unknown name is the generic` |
|        - |  449 | `	 * invalid-type ValueError. */` |
|       41 |  450 | `	if( nLen == 8 && SyStrnicmp(zType,"resource",8) == 0 ){` |
|        3 |  451 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|        - |  452 | `			"Cannot convert to resource type");` |
|        - |  453 | `	}` |
|       40 |  454 | `	if( !(  (nLen == 3 && SyStrnicmp(zType,"int",3) == 0)` |
|       35 |  455 | `	     \|\| (nLen == 7 && SyStrnicmp(zType,"integer",7) == 0)` |
|       51 |  456 | `	     \|\| (nLen == 5 && SyStrnicmp(zType,"float",5) == 0)` |
|       50 |  457 | `	     \|\| (nLen == 6 && SyStrnicmp(zType,"double",6) == 0)` |
|       49 |  458 | `	     \|\| (nLen == 6 && SyStrnicmp(zType,"string",6) == 0)` |
|       34 |  459 | `	     \|\| (nLen == 4 && SyStrnicmp(zType,"bool",4) == 0)` |
|       11 |  460 | `	     \|\| (nLen == 7 && SyStrnicmp(zType,"boolean",7) == 0)` |
|       10 |  461 | `	     \|\| (nLen == 5 && SyStrnicmp(zType,"array",5) == 0)` |
|        8 |  462 | `	     \|\| (nLen == 6 && SyStrnicmp(zType,"object",6) == 0)` |
|        3 |  463 | `	     \|\| (nLen == 4 && SyStrnicmp(zType,"null",4) == 0) ) ){` |
|        3 |  464 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|        - |  465 | `			"settype(): Argument #2 ($type) must be a valid type");` |
|        - |  466 | `	}` |
|        - |  467 | `	/* Convert a COPY of the current value (the To* helpers mutate in place), then` |
|        - |  468 | `	 * store it through the by-ref out-param — settype changes $var's TYPE, so the` |
|        - |  469 | `	 * new value must land in the caller slot (nIdx), not just in shared contents. */` |
|       51 |  470 | `	pNew = ph7_context_new_scalar(pCtx);` |
|       51 |  471 | `	if( pNew == 0 ){` |
|      ! 0 |  472 | `		return PH7_ContextMemoryError(pCtx);` |
|        - |  473 | `	}` |
|       51 |  474 | `	PH7_MemObjStore(apArg[0],pNew);` |
|       48 |  475 | `	if( (nLen == 3 && SyStrnicmp(zType,"int",3) == 0)` |
|       50 |  476 | `	 \|\| (nLen == 7 && SyStrnicmp(zType,"integer",7) == 0) ){` |
|       10 |  477 | `		PH7_MemObjToInteger(pNew);` |
|       10 |  478 | `		MemObjSetType(pNew,MEMOBJ_INT);` |
|       52 |  479 | `	}else if( (nLen == 5 && SyStrnicmp(zType,"float",5) == 0)` |
|       48 |  480 | `	       \|\| (nLen == 6 && SyStrnicmp(zType,"double",6) == 0) ){` |
|        5 |  481 | `		PH7_MemObjToReal(pNew);` |
|        5 |  482 | `		MemObjSetType(pNew,MEMOBJ_REAL);` |
|       55 |  483 | `	}else if( nLen == 6 && SyStrnicmp(zType,"string",6) == 0 ){` |
|        - |  484 | `		/* php emits "Array to string conversion" for settype($arr,'string'), and` |
|        - |  485 | `		 * throws "Object of class X could not be converted to string" for an` |
|        - |  486 | `		 * object with no __toString() (or propagates one that threw). php's` |
|        - |  487 | `		 * convert_to_string() has already blanked the zval by then, so a CAUGHT` |
|        - |  488 | `		 * settype() leaves $var === "" — store that, then propagate instead of` |
|        - |  489 | `		 * answering true. */` |
|       33 |  490 | `		sxi32 rcSv = PH7_MemObjToStringUV(pNew);` |
|       33 |  491 | `		if( rcSv != SXRET_OK ){` |
|       16 |  492 | `			PH7_MemObjRelease(pNew);` |
|       16 |  493 | `			MemObjSetType(pNew,MEMOBJ_STRING);` |
|       16 |  494 | `			PH7_VmStoreArgByRef(pCtx->pVm,apArg[0],pNew);` |
|       16 |  495 | `			pCtx->nThrowRc = rcSv;` |
|       16 |  496 | `			return rcSv;` |
|        - |  497 | `		}` |
|       22 |  498 | `	}else if( (nLen == 4 && SyStrnicmp(zType,"bool",4) == 0)` |
|       12 |  499 | `	       \|\| (nLen == 7 && SyStrnicmp(zType,"boolean",7) == 0) ){` |
|        5 |  500 | `		PH7_MemObjToBool(pNew);` |
|       11 |  501 | `	}else if( nLen == 5 && SyStrnicmp(zType,"array",5) == 0 ){` |
|        5 |  502 | `		PH7_MemObjToHashmap(pNew);` |
|        7 |  503 | `	}else if( nLen == 6 && SyStrnicmp(zType,"object",6) == 0 ){` |
|        3 |  504 | `		PH7_MemObjToObject(pNew);` |
|        2 |  505 | `	}else{` |
|        - |  506 | `		/* "null" — the only validated name left */` |
|        3 |  507 | `		PH7_MemObjToNull(pNew);` |
|        - |  508 | `	}` |
|       43 |  509 | `	PH7_VmStoreArgByRef(pCtx->pVm,apArg[0],pNew);` |
|       43 |  510 | `	ph7_result_bool(pCtx,1);` |
|       43 |  511 | `	return SXRET_OK;` |
|       32 |  512 | `}` |
|        - |  513 | `/*` |
|        - |  514 | ` * string get_resource_type(resource $handle)` |
|        - |  515 | ` *  This function gets the type of the given resource.` |
|        - |  516 | ` * Parameters` |
|        - |  517 | ` *  $handle` |
|        - |  518 | ` *  The evaluated resource handle.` |
|        - |  519 | ` * Return` |
|        - |  520 | ` *  If the given handle is a resource, this function will return a string` |
|        - |  521 | ` *  representing its type. If the type is not identified by this function` |
|        - |  522 | ` *  the return value will be the string Unknown.` |
|        - |  523 | ` *  This function will return FALSE and generate an error if handle` |
|        - |  524 | ` *  is not a resource.` |
|        - |  525 | ` */` |
|        8 |  526 | `PH7_PRIVATE int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  527 | `{` |
|        - |  528 | `	const char *zType;` |
|        9 |  529 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - |  530 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 |  531 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  532 | `		return PH7_OK;` |
|        - |  533 | `	}` |
|        - |  534 | `	/* PHP returns the resource TYPE name (e.g. "stream" for IO handles), not an id. */` |
|        9 |  535 | `	zType = PH7_VfsResourceType(apArg[0]->x.pOther);` |
|        9 |  536 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|        9 |  537 | `	return SXRET_OK;` |
|        5 |  538 | `}` |
|        - |  539 | `/*` |
|        - |  540 | ` * int get_resource_id(resource $resource)` |
|        - |  541 | ` *  Returns the integer id of a resource — the same number (int) casts to and` |
|        - |  542 | ` *  "Resource id #N" renders (php 8.0).` |
|        - |  543 | ` */` |
|        4 |  544 | `PH7_PRIVATE int vm_builtin_get_resource_id(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  545 | `{` |
|        6 |  546 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 |  547 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|        - |  548 | `			"get_resource_id(): Argument #1 ($resource) must be of type resource, %s given",` |
|      ! 0 |  549 | `			nArg < 1 ? "none" : ph7_type_name(apArg[0]));` |
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
|     3754 |  562 | `PH7_PRIVATE int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  563 | `{` |
|        - |  564 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - |  565 | `	int i;` |
|     3759 |  566 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - |  567 | `	/* Dump one or more expressions */` |
|     8251 |  568 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     4497 |  569 | `		ph7_value *pObj = apArg[i];` |
|        - |  570 | `		/* Reset the working buffer */` |
|     4497 |  571 | `		SyBlobReset(&sDump);` |
|        - |  572 | `		/* Dump the given expression */` |
|     4497 |  573 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - |  574 | `		/* Output */` |
|     4497 |  575 | `		if( SyBlobLength(&sDump) > 0 ){` |
|     4497 |  576 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|     2246 |  577 | `		}` |
|     2251 |  578 | `	}` |
|        - |  579 | `	/* Release the working buffer */` |
|     3759 |  580 | `	SyBlobRelease(&sDump);` |
|     3759 |  581 | `	return SXRET_OK;` |
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
|      154 |  595 | `PH7_PRIVATE int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        4 |  596 | `{` |
|      158 |  597 | `	int ret_string = 0;` |
|        - |  598 | `	SyBlob sDump;` |
|      158 |  599 | `	if( nArg < 1 ){` |
|        - |  600 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  601 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  602 | `		return SXRET_OK;` |
|        - |  603 | `	}` |
|      158 |  604 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|      158 |  605 | `	if ( nArg > 1 ){` |
|        - |  606 | `		/* Where to redirect output */` |
|       17 |  607 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        7 |  608 | `	}` |
|        - |  609 | `	/* Generate dump */` |
|      158 |  610 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|      158 |  611 | `	if( !ret_string ){` |
|        - |  612 | `		/* Output dump */` |
|      144 |  613 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  614 | `		/* Return true */` |
|      144 |  615 | `		ph7_result_bool(pCtx,1);` |
|       74 |  616 | `	}else{` |
|        - |  617 | `		/* Generated dump as return value */` |
|       17 |  618 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  619 | `	}` |
|        - |  620 | `	/* Release the working buffer */` |
|      158 |  621 | `	SyBlobRelease(&sDump);` |
|      158 |  622 | `	return SXRET_OK;` |
|       81 |  623 | `}` |
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
|        - |  642 | `};` |
|        - |  643 | `static void VmExportValue(SyBlob *pOut, ph7_value *pVal, int nIndent, int depth);` |
|        - |  644 | `/* Append nIndent spaces. */` |
|     3450 |  645 | `static void VmExportIndent(SyBlob *pOut, int nIndent)` |
|        5 |  646 | `{` |
|        - |  647 | `	int i;` |
|     9477 |  648 | `	for( i = 0; i < nIndent; i++ ){ SyBlobAppend(pOut," ",1); }` |
|     3455 |  649 | `}` |
|        - |  650 | `/* Append a single-quoted PHP string literal (escapes ' and \, batching safe runs` |
|        - |  651 | ` * in one append). A NUL byte can't live in a single-quoted literal, so PHP splits` |
|        - |  652 | ` * it out as ' . "\0" . ' — match that. */` |
|     2876 |  653 | `static void VmExportQuoted(SyBlob *pOut, const char *z, int n)` |
|        5 |  654 | `{` |
|     2881 |  655 | `	int i, run = 0;` |
|     2881 |  656 | `	SyBlobAppend(pOut,"'",1);` |
|    24213 |  657 | `	for( i = 0; i < n; i++ ){` |
|    21337 |  658 | `		char c = z[i];` |
|    21337 |  659 | `		if( c != '\0' && c != '\'' && c != '\\' ){ continue; } /* extend the safe run */` |
|      269 |  660 | `		if( i > run ){ SyBlobAppend(pOut,&z[run],(sxu32)(i-run)); }` |
|      269 |  661 | `		if( c == '\0' ){ SyBlobAppend(pOut,"' . \"\\0\" . '",12); }` |
|      253 |  662 | `		else { SyBlobAppend(pOut,"\\",1); SyBlobAppend(pOut,&z[i],1); }` |
|      269 |  663 | `		run = i+1;` |
|      135 |  664 | `	}` |
|     2881 |  665 | `	if( n > run ){ SyBlobAppend(pOut,&z[run],(sxu32)(n-run)); }` |
|     2881 |  666 | `	SyBlobAppend(pOut,"'",1);` |
|     2881 |  667 | `}` |
|        - |  668 | `/* True if the array/object is already on the var_export recursion stack. */` |
|     2154 |  669 | `static int VmExportIsCycle(ph7_value *pVal)` |
|        5 |  670 | `{` |
|     2159 |  671 | `	if( ph7_value_is_array(pVal) ){` |
|      207 |  672 | `		return (((ph7_hashmap *)pVal->x.pOther)->iFlags & HASHMAP_DUMPING) != 0;` |
|        - |  673 | `	}` |
|     1955 |  674 | `	if( ph7_value_is_object(pVal) ){` |
|       10 |  675 | `		return (((ph7_class_instance *)pVal->x.pOther)->iFlags & VM_INSTANCE_DUMPING) != 0;` |
|        - |  676 | `	}` |
|     1947 |  677 | `	return 0;` |
|     1082 |  678 | `}` |
|        - |  679 | `/* Emit " => " then the value: a scalar inline, a composite on its own line at` |
|        - |  680 | ` * containerIndent+2. A circular reference renders inline as NULL (like PHP). */` |
|     2154 |  681 | `static void VmExportEntryValue(SyBlob *pOut, ph7_value *pVal, int nContainerIndent, int depth)` |
|        5 |  682 | `{` |
|     2159 |  683 | `	SyBlobAppend(pOut," => ",4);` |
|     2159 |  684 | `	if( VmExportIsCycle(pVal) ){` |
|        5 |  685 | `		SyBlobAppend(pOut,"NULL",4);` |
|     2157 |  686 | `	}else if( ph7_value_is_array(pVal) \|\| ph7_value_is_object(pVal) ){` |
|      212 |  687 | `		SyBlobAppend(pOut,"\n",1);` |
|      212 |  688 | `		VmExportIndent(pOut,nContainerIndent+2);` |
|      212 |  689 | `		VmExportValue(pOut,pVal,nContainerIndent+2,depth+1);` |
|      108 |  690 | `	}else{` |
|     1947 |  691 | `		VmExportValue(pOut,pVal,nContainerIndent+2,depth+1);` |
|        - |  692 | `	}` |
|     2159 |  693 | `	SyBlobAppend(pOut,",\n",2);` |
|     2159 |  694 | `}` |
|        - |  695 | `/* Array walker: "<indent+2>key => value,\n" for each entry. */` |
|     2086 |  696 | `static int VmExportArrayWalk(ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|        5 |  697 | `{` |
|     2091 |  698 | `	VmExportCtx *pC = (VmExportCtx *)pUserData;` |
|     2091 |  699 | `	VmExportIndent(pC->pOut,pC->nIndent+2);` |
|     2091 |  700 | `	if( ph7_value_is_string(pKey) ){` |
|        - |  701 | `		int n;` |
|      438 |  702 | `		const char *z = ph7_value_to_string(pKey,&n);` |
|      438 |  703 | `		VmExportQuoted(pC->pOut,z,n);` |
|      221 |  704 | `	}else{` |
|     1657 |  705 | `		SyBlobFormat(pC->pOut,"%qd",ph7_value_to_int64(pKey));` |
|        - |  706 | `	}` |
|     2091 |  707 | `	VmExportEntryValue(pC->pOut,pValue,pC->nIndent,pC->depth);` |
|     2091 |  708 | `	return PH7_OK;` |
|        5 |  709 | `}` |
|     7644 |  710 | `static void VmExportValue(SyBlob *pOut, ph7_value *pVal, int nIndent, int depth)` |
|        5 |  711 | `{` |
|     7649 |  712 | `	if( depth > 4096 ){ return; } /* backstop for pathological finite nesting */` |
|     7649 |  713 | `	if( ph7_value_is_null(pVal) ){` |
|      245 |  714 | `		SyBlobAppend(pOut,"NULL",4);` |
|     7529 |  715 | `	}else if( ph7_value_is_bool(pVal) ){` |
|     1653 |  716 | `		if( ph7_value_to_bool(pVal) ){ SyBlobAppend(pOut,"true",4); }` |
|      813 |  717 | `		else { SyBlobAppend(pOut,"false",5); }` |
|     6585 |  718 | `	}else if( ph7_value_is_float(pVal) ){` |
|        - |  719 | `		/* float before int: ph7_value_is_int is lenient (true for integer reals). */` |
|      475 |  720 | `		sxu32 before = SyBlobLength(pOut), i, after;` |
|        - |  721 | `		const char *z;` |
|      475 |  722 | `		int plain = 1;` |
|      475 |  723 | `		PH7_AppendShortestReal(pOut,ph7_value_to_double(pVal));` |
|      475 |  724 | `		z = (const char *)SyBlobData(pOut);` |
|      475 |  725 | `		after = SyBlobLength(pOut);` |
|     1133 |  726 | `		for( i = before; i < after; i++ ){` |
|      915 |  727 | `			if( !((z[i]>='0'&&z[i]<='9')\|\|z[i]=='-') ){ plain = 0; break; }` |
|      332 |  728 | `		}` |
|      475 |  729 | `		if( plain ){ SyBlobAppend(pOut,".0",2); } /* integer-form floats render 1.0/100.0 */` |
|     5525 |  730 | `	}else if( ph7_value_is_int(pVal) ){` |
|     1825 |  731 | `		sxi64 iVal = ph7_value_to_int64(pVal);` |
|     1825 |  732 | `		if( iVal == SMALLEST_INT64 ){` |
|        - |  733 | `			/* php renders LONG_MIN as an expression: the positive literal` |
|        - |  734 | `			 * 9223372036854775808 would not be representable when eval'd. */` |
|        9 |  735 | `			SyBlobAppend(pOut,"-9223372036854775807-1",sizeof("-9223372036854775807-1")-1);` |
|        5 |  736 | `		}else{` |
|     1817 |  737 | `			SyBlobFormat(pOut,"%qd",iVal);` |
|        5 |  738 | `		}` |
|     4379 |  739 | `	}else if( ph7_value_is_string(pVal) ){` |
|        - |  740 | `		int n;` |
|     2379 |  741 | `		const char *z = ph7_value_to_string(pVal,&n);` |
|     2379 |  742 | `		VmExportQuoted(pOut,z,n);` |
|     2282 |  743 | `	}else if( ph7_value_is_array(pVal) ){` |
|     1019 |  744 | `		ph7_hashmap *pMap = (ph7_hashmap *)pVal->x.pOther;` |
|     1019 |  745 | `		if( pMap->iFlags & HASHMAP_DUMPING ){` |
|      ! 0 |  746 | `			SyBlobAppend(pOut,"NULL",4); /* circular reference -> NULL, like PHP */` |
|      ! 0 |  747 | `		}else{` |
|        - |  748 | `			VmExportCtx ctx;` |
|     1019 |  749 | `			pMap->iFlags \|= HASHMAP_DUMPING;` |
|     1019 |  750 | `			SyBlobAppend(pOut,"array (\n",8);` |
|     1019 |  751 | `			ctx.pOut = pOut; ctx.nIndent = nIndent; ctx.depth = depth;` |
|     1019 |  752 | `			ph7_array_walk(pVal,VmExportArrayWalk,&ctx);` |
|     1019 |  753 | `			VmExportIndent(pOut,nIndent);` |
|     1019 |  754 | `			SyBlobAppend(pOut,")",1);` |
|     1019 |  755 | `			pMap->iFlags &= ~HASHMAP_DUMPING;` |
|        5 |  756 | `		}` |
|      587 |  757 | `	}else if( ph7_value_is_object(pVal) ){` |
|       80 |  758 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|       80 |  759 | `		if( pThis->pClass->iFlags & PH7_CLASS_ENUM ){` |
|        - |  760 | ``			/* php 8.1: an enum case exports as `\S::A` */`` |
|        3 |  761 | `			ph7_value *pName = PH7_EnumCaseNameValue(pThis);` |
|        3 |  762 | `			SyBlobAppend(pOut,"\\",1);` |
|        3 |  763 | `			SyBlobAppend(pOut,pThis->pClass->sName.zString,pThis->pClass->sName.nByte);` |
|        3 |  764 | `			SyBlobAppend(pOut,"::",2);` |
|        3 |  765 | `			if( pName && SyBlobLength(&pName->sBlob) > 0 ){` |
|        3 |  766 | `				SyBlobAppend(pOut,SyBlobData(&pName->sBlob),SyBlobLength(&pName->sBlob));` |
|        2 |  767 | `			}` |
|       79 |  768 | `		}else if( pThis->iFlags & VM_INSTANCE_DUMPING ){` |
|      ! 0 |  769 | `			SyBlobAppend(pOut,"NULL",4); /* circular reference -> NULL, like PHP */` |
|      ! 0 |  770 | `		}else{` |
|       78 |  771 | `			SyString *pClassName = &pThis->pClass->sName;` |
|        - |  772 | `			SyHashEntry *pEntry;` |
|        - |  773 | `			SySet sNames;` |
|        - |  774 | `			SyString *aName;` |
|        - |  775 | `			sxu32 iName,nName;` |
|        - |  776 | ``			/* php exports a plain stdClass as a CAST — `(object) array(...)` — and`` |
|        - |  777 | `			 * every other class through __set_state(); a SUBCLASS of stdClass takes` |
|        - |  778 | `			 * the __set_state form, so this is the exact class and not an` |
|        - |  779 | `			 * inheritance test. The two forms differ by one closing paren. */` |
|       78 |  780 | `			int bStdObj = pThis->pClass == pThis->pVm->pStdClass;` |
|       78 |  781 | `			pThis->iFlags \|= VM_INSTANCE_DUMPING;` |
|       78 |  782 | `			if( bStdObj ){` |
|        3 |  783 | `				SyBlobAppend(pOut,"(object) array(\n",sizeof("(object) array(\n")-1);` |
|        2 |  784 | `			}else{` |
|       76 |  785 | `				SyBlobAppend(pOut,"\\",1);` |
|       76 |  786 | `				SyBlobAppend(pOut,pClassName->zString,pClassName->nByte);` |
|       76 |  787 | `				SyBlobAppend(pOut,"::__set_state(array(\n",21);` |
|        - |  788 | `			}` |
|        - |  789 | `			{` |
|        - |  790 | `				/* A native class's PRESENTATION (php's get_properties): var_export` |
|        - |  791 | `				 * shows a DateTime as date/timezone_type/timezone, the same shape` |
|        - |  792 | `				 * the (array) cast produces and NOT the hidden engine slots. A` |
|        - |  793 | `				 * debug-only hook (WeakReference) fills nothing, which is php's` |
|        - |  794 | `				 * empty export. */` |
|        - |  795 | `				ph7_value sPresent;` |
|       78 |  796 | `				ph7_hashmap *pPresent = PH7_NewHashmap(pThis->pVm,0,0);` |
|       78 |  797 | `				PH7_MemObjInit(pThis->pVm,&sPresent);` |
|       78 |  798 | `				if( pPresent ){` |
|       78 |  799 | `					sPresent.x.pOther = pPresent;` |
|       78 |  800 | `					MemObjSetType(&sPresent,MEMOBJ_HASHMAP);` |
|       78 |  801 | `					if( PH7_ClassInstancePresent(pThis,&sPresent,0) ){` |
|        - |  802 | `						/* Same line shape the attribute loop below produces: an` |
|        - |  803 | `						 * object body's entries sit one deeper than an array's. */` |
|        - |  804 | `						VmExportCtx sCtx;` |
|       32 |  805 | `						sCtx.pOut = pOut;` |
|       32 |  806 | `						sCtx.nIndent = nIndent + 1;` |
|       32 |  807 | `						sCtx.depth = depth;` |
|       32 |  808 | `						ph7_array_walk(&sPresent,VmExportArrayWalk,&sCtx);` |
|       32 |  809 | `						PH7_MemObjRelease(&sPresent);` |
|       32 |  810 | `						VmExportIndent(pOut,nIndent);` |
|       32 |  811 | `						SyBlobAppend(pOut,bStdObj ? ")" : "))",bStdObj ? 1 : 2);` |
|       32 |  812 | `						pThis->iFlags &= ~VM_INSTANCE_DUMPING;` |
|       32 |  813 | `						return;` |
|        - |  814 | `					}` |
|       48 |  815 | `					PH7_MemObjRelease(&sPresent);` |
|       22 |  816 | `				}` |
|        - |  817 | `			}` |
|        - |  818 | `			/* SNAPSHOT the attribute names first: a PHP 8.4 get hook dispatched` |
|        - |  819 | `			 * mid-walk may re-enter an hAttr walk on this instance (the hash has` |
|        - |  820 | `			 * a single embedded loop cursor) or unset()/create properties; names` |
|        - |  821 | `			 * point into class-owned attr storage and each is re-looked-up. */` |
|       48 |  822 | `			SySetInit(&sNames,&pThis->pVm->sAllocator,sizeof(SyString));` |
|       48 |  823 | `			SyHashResetLoopCursor(&pThis->hAttr);` |
|      152 |  824 | `			while( (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|      107 |  825 | `				VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|      107 |  826 | `				if( pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_HIDDEN) ){ continue; }` |
|       70 |  827 | `				if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_VIRTUAL))` |
|       38 |  828 | `				 == PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|        3 |  829 | `					continue; /* virtual set-only property: no value to export (php) */` |
|        - |  830 | `				}` |
|       71 |  831 | `				SySetPut(&sNames,(const void *)&pVmAttr->pAttr->sName);` |
|        3 |  832 | `			}` |
|       48 |  833 | `			aName = (SyString *)SySetBasePtr(&sNames);` |
|       48 |  834 | `			nName = SySetUsed(&sNames);` |
|      116 |  835 | `			for( iName = 0 ; iName < nName ; ++iName ){` |
|       71 |  836 | `				SyString *pAName = &aName[iName];` |
|        - |  837 | `				VmClassAttr *pVmAttr;` |
|        - |  838 | `				ph7_value *pAttrVal;` |
|       71 |  839 | `				pEntry = PH7_ClassInstanceAttrEntry(pThis,pAName->zString,pAName->nByte);` |
|       71 |  840 | `				if( pEntry == 0 ){ continue; } /* unset by an earlier hook */` |
|       71 |  841 | `				pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|       71 |  842 | `				VmExportIndent(pOut,nIndent+3); /* object property lines sit one deeper than arrays */` |
|       73 |  843 | `				if( pAName->nByte > 0 && pAName->zString[0] == 0 ){` |
|        - |  844 | `					/* A MANGLED key stored raw (the __PHP_Incomplete_Class carrier):` |
|        - |  845 | `					 * php's var_export prints the PLAIN name ('bp' => 1). */` |
|        - |  846 | `					SyString sUnmCls, sUnmName;` |
|        5 |  847 | `					SyStringInitFromBuf(&sUnmName,pAName->zString,pAName->nByte);` |
|        5 |  848 | `					PH7_UnmangleAttrName(pAName->zString,pAName->nByte,&sUnmCls,&sUnmName);` |
|        5 |  849 | `					VmExportQuoted(pOut,sUnmName.zString,(int)sUnmName.nByte);` |
|        3 |  850 | `				}else{` |
|       67 |  851 | `					VmExportQuoted(pOut,pAName->zString,(int)pAName->nByte);` |
|        - |  852 | `				}` |
|        - |  853 | `				/* PHP 8.4 property hooks: var_export() reads through the get hook` |
|        - |  854 | `				 * (every visibility — php exports private hooked values too). A` |
|        - |  855 | `				 * throwing hook parks on the boundary rail; the helper's boundary` |
|        - |  856 | `				 * gate keeps LATER hooks from running (the raw values the tail of` |
|        - |  857 | `				 * the export falls back to are discarded when the throw routes). */` |
|        - |  858 | `				{` |
|        - |  859 | `					ph7_value sHookVal;` |
|        - |  860 | `					sxi32 rcHk;` |
|       71 |  861 | `					PH7_MemObjInit(pThis->pVm,&sHookVal);` |
|       71 |  862 | `					rcHk = PH7_VmHookGetAttrValue(pThis,pVmAttr,&sHookVal);` |
|       71 |  863 | `					if( rcHk == SXRET_OK ){` |
|       13 |  864 | `						VmExportEntryValue(pOut,&sHookVal,nIndent,depth);` |
|       13 |  865 | `						PH7_MemObjRelease(&sHookVal);` |
|       13 |  866 | `						continue;` |
|        - |  867 | `					}` |
|       59 |  868 | `					PH7_MemObjRelease(&sHookVal);` |
|       59 |  869 | `					if( rcHk != SXERR_NOTFOUND ){` |
|        - |  870 | `						/* the hook threw (parked on the boundary rail): NULL` |
|        - |  871 | `						 * placeholder keeps the output well-formed */` |
|      ! 0 |  872 | `						SyBlobAppend(pOut," => NULL,\n",10);` |
|      ! 0 |  873 | `						continue;` |
|        - |  874 | `					}` |
|        - |  875 | `				}` |
|       59 |  876 | `				pAttrVal = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       59 |  877 | `				if( pAttrVal ){ VmExportEntryValue(pOut,pAttrVal,nIndent,depth); }` |
|      ! 0 |  878 | `				else { SyBlobAppend(pOut," => NULL,\n",10); }` |
|       31 |  879 | `			}` |
|       48 |  880 | `			SySetRelease(&sNames);` |
|       48 |  881 | `			VmExportIndent(pOut,nIndent);` |
|       48 |  882 | `			SyBlobAppend(pOut,bStdObj ? ")" : "))",bStdObj ? 1 : 2);` |
|       48 |  883 | `			pThis->iFlags &= ~VM_INSTANCE_DUMPING;` |
|        - |  884 | `		}` |
|       27 |  885 | `	}else{` |
|        - |  886 | `		/* resource / other -> PHP emits NULL (with a warning we omit) */` |
|      ! 0 |  887 | `		SyBlobAppend(pOut,"NULL",4);` |
|        - |  888 | `	}` |
|     3827 |  889 | `}` |
|        - |  890 | `/*` |
|        - |  891 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - |  892 | ` *  PHP-exact evaluable representation (see VmExportValue).` |
|        - |  893 | ` */` |
|     5494 |  894 | `PH7_PRIVATE int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  895 | `{` |
|     5499 |  896 | `	int ret_string = 0;` |
|        - |  897 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|     5499 |  898 | `	if( nArg < 1 ){` |
|        - |  899 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  900 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  901 | `		return SXRET_OK;` |
|        - |  902 | `	}` |
|     5499 |  903 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|     5499 |  904 | `	if ( nArg > 1 ){` |
|        - |  905 | `		/* Where to redirect output */` |
|     5051 |  906 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|     2523 |  907 | `	}` |
|        - |  908 | `	/* Generate the PHP-exact evaluable representation */` |
|     5499 |  909 | `	VmExportValue(&sDump,apArg[0],0,0);` |
|     5499 |  910 | `	if( !ret_string ){` |
|        - |  911 | `		/* Output dump */` |
|      453 |  912 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  913 | `		/* Return NULL */` |
|      453 |  914 | `		ph7_result_null(pCtx);` |
|      229 |  915 | `	}else{` |
|        - |  916 | `		/* Generated dump as return value */` |
|     5051 |  917 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  918 | `	}` |
|        - |  919 | `	/* Release the working buffer */` |
|     5499 |  920 | `	SyBlobRelease(&sDump);` |
|     5499 |  921 | `	return SXRET_OK;` |
|     2752 |  922 | `}` |
|        - |  923 |  |
