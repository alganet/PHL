# src/ph7/vm_builtin_var.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 434/475 lines (91.37%)

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
|  3016770 |   23 | `PH7_PRIVATE int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |   24 | `{` |
|        - |   25 | `	ph7_value *pObj;` |
|  3016775 |   26 | `	int res = 0;` |
|        - |   27 | `	int i;` |
|  3016775 |   28 | `	if( nArg < 1 ){` |
|        - |   29 | `		/* Missing arguments,return false */` |
|      ! 0 |   30 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 |   31 | `		return SXRET_OK;` |
|        - |   32 | `	}` |
|        - |   33 | `	/* Iterate over available arguments */` |
|  4495177 |   34 | `	for( i = 0 ; i < nArg ; ++i ){` |
|  3016787 |   35 | `		pObj = apArg[i];` |
|  3016787 |   36 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|        - |   37 | `			/* Skip the "expecting a variable" warning for MEMOBJ_BOOL —` |
|        - |   38 | `			 * synthesized by LOAD_IDX iP2=4 (ArrayAccess::offsetExists) and` |
|        - |   39 | `			 * by anyone passing a bool literal (rare, harmless). */` |
|    90855 |   40 | `			if( (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|        - |   41 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 |   42 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 |   43 | `			}` |
|    45425 |   44 | `		}` |
|  3016787 |   45 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|  3016787 |   46 | `		if( !res ){` |
|        - |   47 | `			/* Variable not set,return FALSE */` |
|  1538385 |   48 | `			ph7_result_bool(pCtx,0);` |
|  1538385 |   49 | `			return SXRET_OK;` |
|        - |   50 | `		}` |
|   739206 |   51 | `	}` |
|        - |   52 | `	/* All given variable are set,return TRUE */` |
|  1478395 |   53 | `	ph7_result_bool(pCtx,1);` |
|  1478395 |   54 | `	return SXRET_OK;` |
|  1508390 |   55 | `}` |
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
|     6904 |   75 | `PH7_PRIVATE sxi32 VmUnsetVarByName(ph7_vm *pVm,VmFrame *pFrame,const char *zName,sxu32 nByte)` |
|        5 |   76 | `{` |
|        - |   77 | `	SyHashEntry *pEntry;` |
|        - |   78 | `	VmRefObj *pRef;` |
|        - |   79 | `	sxu32 nIdx;` |
|        - |   80 | `	/* php 8.1 forbids unset($GLOBALS) outright. Checked by NAME: the superglobal is not an` |
|        - |   81 | `	 * ordinary hVar binding, so the slot-index test below never sees it. */` |
|     6909 |   82 | `	if( nByte == sizeof("GLOBALS")-1 && SyMemcmp(zName,"GLOBALS",nByte) == 0 ){` |
|        3 |   83 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |   84 | `			"$GLOBALS can only be modified using the $GLOBALS[$name] = $value syntax");` |
|        3 |   85 | `		pVm->iExitStatus = 255;` |
|        3 |   86 | `		pVm->bHaltRequested = 1;` |
|        3 |   87 | `		return PH7_ABORT;` |
|        - |   88 | `	}` |
|     6907 |   89 | `	pEntry = SyHashGet(&pFrame->hVar,(const void *)zName,nByte);` |
|     6907 |   90 | `	if( pEntry == 0 ){` |
|        - |   91 | `		/* No such variable: unset() is a no-op on an undefined name, as in php */` |
|     1037 |   92 | `		return SXRET_OK;` |
|        - |   93 | `	}` |
|     5873 |   94 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|     5873 |   95 | `	if( nIdx == pVm->nGlobalIdx ){` |
|      ! 0 |   96 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |   97 | `			"$GLOBALS can only be modified using the $GLOBALS[$name] = $value syntax");` |
|      ! 0 |   98 | `		pVm->iExitStatus = 255;` |
|      ! 0 |   99 | `		pVm->bHaltRequested = 1;` |
|      ! 0 |  100 | `		return PH7_ABORT;` |
|        - |  101 | `	}` |
|     5873 |  102 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|        - |  103 | `	/*` |
|        - |  104 | `	 * A GLOBAL variable is ALSO an entry of the $GLOBALS array, and that node points at the` |
|        - |  105 | `	 * same slot. php's unset($x) in global scope drops $GLOBALS['x'] too, so unlink the node` |
|        - |  106 | `	 * — otherwise it stays a live holder and the value is never released ($o = new D;` |
|        - |  107 | `	 * unset($o); stopped running the destructor). Clear the reference table's row for the` |
|        - |  108 | `	 * node BEFORE unlinking it: unlinking frees the node, and the holder count below would` |
|        - |  109 | `	 * otherwise dereference freed memory.` |
|        - |  110 | `	 */` |
|     5873 |  111 | `	if( pFrame->pParent == 0 ){` |
|     5860 |  112 | `		ph7_value *pGlobals = (ph7_value *)SySetAt(&pVm->aMemObj,pVm->nGlobalIdx);` |
|     5860 |  113 | `		if( pGlobals && (pGlobals->iFlags & MEMOBJ_HASHMAP) ){` |
|     5860 |  114 | `			ph7_hashmap_node *pNode = 0;` |
|        - |  115 | `			ph7_value sKey;` |
|        - |  116 | `			SyString sName;` |
|     5860 |  117 | `			PH7_MemObjInit(&(*pVm),&sKey);` |
|     5860 |  118 | `			SyStringInitFromBuf(&sName,zName,nByte);` |
|     5860 |  119 | `			PH7_MemObjInitFromString(&(*pVm),&sKey,&sName);` |
|     5858 |  120 | `			if( SXRET_OK == PH7_HashmapLookup((ph7_hashmap *)pGlobals->x.pOther,&sKey,&pNode)` |
|     5860 |  121 | `			 && pNode && pNode->nValIdx == nIdx ){` |
|     5858 |  122 | `				if( pRef ){` |
|     5858 |  123 | `					ph7_hashmap_node **apN = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|        - |  124 | `					sxu32 k;` |
|    11812 |  125 | `					for( k = 0 ; k < SySetUsed(&pRef->aArrEntries) ; ++k ){` |
|     5956 |  126 | `						if( apN[k] == pNode ){` |
|     5858 |  127 | `							apN[k] = 0;` |
|     2928 |  128 | `						}` |
|     2979 |  129 | `					}` |
|     2928 |  130 | `				}` |
|     5858 |  131 | `				PH7_HashmapUnlinkNode(pNode,FALSE);` |
|     2928 |  132 | `			}` |
|     5860 |  133 | `			PH7_MemObjRelease(&sKey);` |
|     2929 |  134 | `		}` |
|     2929 |  135 | `	}` |
|        - |  136 |  |
|     5873 |  137 | `	if( pRef == 0 ){` |
|        - |  138 | `		/* Unaliased variable: nobody else holds the slot, so the old path is right */` |
|      ! 0 |  139 | `		SyHashDeleteEntry2(pEntry);` |
|      ! 0 |  140 | `		PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|        - |  141 | `		/* The slot is back in the free pool; drop its stale local-teardown entry so a` |
|        - |  142 | `		 * later reuse of the index (e.g. an object property) is not double-freed when` |
|        - |  143 | `		 * this frame exits. */` |
|      ! 0 |  144 | `		VmDropFrameLocalSlot(&(*pVm),nIdx);` |
|      ! 0 |  145 | `		return SXRET_OK;` |
|        - |  146 | `	}` |
|        - |  147 | `	{` |
|     5873 |  148 | `		SyHashEntry **apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|     5873 |  149 | `		ph7_hashmap_node **apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|     5873 |  150 | `		sxu32 n, nLive = 0;` |
|        - |  151 | `		/* Forget THIS name in the slot's reference record (leave the others alone) */` |
|    11839 |  152 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; ++n ){` |
|     5969 |  153 | `			if( apEntry[n] == pEntry ){` |
|     5867 |  154 | `				apEntry[n] = 0;` |
|     2932 |  155 | `			}` |
|     2986 |  156 | `		}` |
|     5873 |  157 | `		SyHashDeleteEntry2(pEntry);` |
|        - |  158 | `		/* Anything else still holding the slot? */` |
|    11839 |  159 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; ++n ){` |
|     5969 |  160 | `			if( apEntry[n] ){` |
|       29 |  161 | `				nLive++;` |
|       14 |  162 | `			}` |
|     2986 |  163 | `		}` |
|    11829 |  164 | `		for( n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|        - |  165 | `			/* Only a node that STILL points at this slot is a holder. The reference table` |
|        - |  166 | `			 * keeps stale rows (a slot index is recycled through the free list, and the row` |
|        - |  167 | `			 * outlives the node that put it there), so an un-filtered count reports holders` |
|        - |  168 | `			 * that no longer exist and the value would never be released — the destructor` |
|        - |  169 | ``			 * of `$o = new D; unset($o);` stopped running. */`` |
|     5958 |  170 | `			if( apNode[n] && apNode[n]->nValIdx == nIdx ){` |
|       63 |  171 | `				nLive++;` |
|       31 |  172 | `			}` |
|     2980 |  173 | `		}` |
|     5873 |  174 | `		if( nLive < 1 ){` |
|        - |  175 | `			/* Last holder gone: now the value may go too */` |
|     5817 |  176 | `			PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|        - |  177 | `			/* Slot returned to the free pool: drop its stale local-teardown entry so a` |
|        - |  178 | `			 * later reuse of the index is not double-freed on frame exit (see` |
|        - |  179 | `			 * VmDropFrameLocalSlot). */` |
|     5817 |  180 | `			VmDropFrameLocalSlot(&(*pVm),nIdx);` |
|     2907 |  181 | `		}` |
|        - |  182 | `	}` |
|     5873 |  183 | `	return SXRET_OK;` |
|     3457 |  184 | `}` |
|        - |  185 | `/*` |
|        - |  186 | ` * Is this memory slot aliased — i.e. does anything other than its owner refer to it?` |
|        - |  187 | ` * var_dump marks such an array element with '&' ("&int(2)"). PH7 only flagged nodes that` |
|        - |  188 | `` * were FOREIGN (`array(&$x)`, where the node points at an outside slot) and so missed the`` |
|        - |  189 | `` * common case, a reference taken TO an element (`$r = &$a[1]`), where the array still owns`` |
|        - |  190 | ` * the value but is no longer its only holder.` |
|        - |  191 | ` */` |
|   746704 |  192 | `PH7_PRIVATE int PH7_VmSlotIsReferenced(ph7_vm *pVm,sxu32 nIdx)` |
|        5 |  193 | `{` |
|        - |  194 | `	VmRefObj *pRef;` |
|   746709 |  195 | `	sxu32 n, nLive = 0;` |
|        - |  196 | `	SyHashEntry **apEntry;` |
|   746709 |  197 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  198 | `		return 0;` |
|        - |  199 | `	}` |
|   746709 |  200 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|   746709 |  201 | `	if( pRef == 0 ){` |
|      ! 0 |  202 | `		return 0;` |
|        - |  203 | `	}` |
|   746709 |  204 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|   746715 |  205 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; ++n ){` |
|        8 |  206 | `		if( apEntry[n] ){` |
|        8 |  207 | `			nLive++;` |
|        3 |  208 | `		}` |
|        5 |  209 | `	}` |
|   746709 |  210 | `	return nLive > 0;` |
|   373357 |  211 | `}` |
| 22322853 |  212 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        5 |  213 | `{` |
|        - |  214 | `	ph7_value *pObj;` |
|        - |  215 | `	VmRefObj *pRef;` |
| 22322858 |  216 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
| 22322858 |  217 | `	if( pObj ){` |
|        - |  218 | `		/* Release the object */` |
| 22322858 |  219 | `		PH7_MemObjRelease(pObj);` |
| 11162401 |  220 | `	}` |
|        - |  221 | `	/* Remove old reference links */` |
| 22322858 |  222 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
| 22322858 |  223 | `	if( pRef ){` |
| 22322858 |  224 | `		sxi32 iFlags = pRef->iFlags;` |
|        - |  225 | `		/* Unlink from the reference table */` |
| 22322858 |  226 | `		VmRefObjUnlink(&(*pVm),pRef);` |
| 22322858 |  227 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - |  228 | `			VmSlot sFree;` |
|        - |  229 | `			/* Restore to the free list */` |
| 22322840 |  230 | `			sFree.nIdx = nObjIdx;` |
| 22322840 |  231 | `			sFree.pUserData = 0;` |
| 22322840 |  232 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
| 11162392 |  233 | `		}` |
| 11162401 |  234 | `	}` |
| 22322858 |  235 | `	return SXRET_OK;` |
|        5 |  236 | `}` |
|        - |  237 | `/*` |
|        - |  238 | ` * void unset($var,...)` |
|        - |  239 | ` *   Unset one or more given variable.` |
|        - |  240 | ` * Parameters` |
|        - |  241 | ` *  One or more variable to unset.` |
|        - |  242 | ` * Return` |
|        - |  243 | ` *  Nothing.` |
|        - |  244 | ` */` |
|     1206 |  245 | `PH7_PRIVATE int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        4 |  246 | `{` |
|        - |  247 | `	ph7_value *pObj;` |
|        - |  248 | `	ph7_vm *pVm;` |
|        - |  249 | `	int i;` |
|        - |  250 | `	/* Point to the target VM */` |
|     1210 |  251 | `	pVm = pCtx->pVm;` |
|        - |  252 | `	/* Iterate and unset */` |
|     2416 |  253 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     1210 |  254 | `		pObj = apArg[i];` |
|     1210 |  255 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|       52 |  256 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  257 | `				/* Throw an error */` |
|      ! 0 |  258 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 |  259 | `			}` |
|       28 |  260 | `		}else{` |
|     1162 |  261 | `			sxu32 nIdx = pObj->nIdx;` |
|     1162 |  262 | `			if( nIdx == pVm->nGlobalIdx ){` |
|        - |  263 | `				/* php 8.1: unset($GLOBALS) is forbidden — the same fatal as` |
|        - |  264 | `				 * re-assigning it (compile-time in php, raised here). */` |
|      ! 0 |  265 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  266 | `					"$GLOBALS can only be modified using the $GLOBALS[$name] = $value syntax");` |
|      ! 0 |  267 | `				pVm->iExitStatus = 255;` |
|      ! 0 |  268 | `				pVm->bHaltRequested = 1;` |
|      ! 0 |  269 | `				return PH7_ABORT;` |
|        - |  270 | `			}` |
|     1162 |  271 | `			PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|        - |  272 | `			/* Drop the stale local-teardown entry for the freed slot (see` |
|        - |  273 | `			 * VmDropFrameLocalSlot) so a later index reuse is not double-freed. */` |
|     1162 |  274 | `			VmDropFrameLocalSlot(&(*pVm),nIdx);` |
|        - |  275 | `		}` |
|      607 |  276 | `	}` |
|     1210 |  277 | `	return SXRET_OK;` |
|      607 |  278 | `}` |
|        - |  279 | `/*` |
|        - |  280 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - |  281 | ` */` |
|      790 |  282 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        4 |  283 | `{` |
|      794 |  284 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      794 |  285 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  286 | `	ph7_value *pObj;` |
|        - |  287 | `	sxu32 nIdx;` |
|        - |  288 | `	/* php excludes $this from get_defined_vars() (it is bound implicitly, not a` |
|        - |  289 | `	 * declared local). PHL keeps it in the frame's hVar, so skip it here. */` |
|      790 |  290 | `	if( pEntry->nKeyLen == sizeof("this")-1` |
|      462 |  291 | `	 && SyMemcmp(pEntry->pKey,"this",sizeof("this")-1) == 0 ){` |
|        6 |  292 | `		return SXRET_OK;` |
|        - |  293 | `	}` |
|        - |  294 | `	/* Extract the memory object */` |
|      790 |  295 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      790 |  296 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      790 |  297 | `	if( pObj ){` |
|      790 |  298 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      786 |  299 | `			if( pEntry->nKeyLen > 0 ){` |
|        - |  300 | `				SyString sName;` |
|        - |  301 | `				ph7_value sKey;` |
|        - |  302 | `				/* Perform the insertion (pObj may point into pVm->aMemObj; the` |
|        - |  303 | `				 * inserter snapshots the source before reserving, so the pool may` |
|        - |  304 | `				 * safely move underneath it — see HashmapInsertIntKey/BlobKey). */` |
|      786 |  305 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      786 |  306 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      786 |  307 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      786 |  308 | `				PH7_MemObjRelease(&sKey);` |
|      391 |  309 | `			}` |
|      391 |  310 | `		}` |
|      393 |  311 | `	}` |
|      790 |  312 | `	return SXRET_OK;` |
|      399 |  313 | `}` |
|        - |  314 | `/*` |
|        - |  315 | ` * array get_defined_vars(void)` |
|        - |  316 | ` *  Returns an array of all defined variables.` |
|        - |  317 | ` * Parameter` |
|        - |  318 | ` *  None` |
|        - |  319 | ` * Return` |
|        - |  320 | ` *  An array with all the variables defined in the current scope.` |
|        - |  321 | ` */` |
|       58 |  322 | `PH7_PRIVATE int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        4 |  323 | `{` |
|       62 |  324 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  325 | `	ph7_value *pArray;` |
|        - |  326 | `	VmFrame *pFrame;` |
|        - |  327 | `	/* Create a new array */` |
|       62 |  328 | `	pArray = ph7_context_new_array(pCtx);` |
|       62 |  329 | ` 	if( pArray == 0 ){` |
|      ! 0 |  330 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  331 | `		SXUNUSED(apArg);` |
|        - |  332 | `		/* Return NULL */` |
|      ! 0 |  333 | `		ph7_result_null(pCtx);` |
|      ! 0 |  334 | `		return SXRET_OK;` |
|        - |  335 | `	}` |
|        - |  336 | `	/* A try/catch block pushes an EXCEPTION frame that holds no variables of its` |
|        - |  337 | `	 * own, so the enclosing function (or global) frame is the one php reports.` |
|        - |  338 | `	 * Reading pVm->pFrame raw answered [] for every call inside a try/catch —` |
|        - |  339 | `	 * including the superglobal test below, which saw a non-NULL pParent and` |
|        - |  340 | `	 * dropped $argv/$_SERVER at global scope. Every other frame-lookup site` |
|        - |  341 | `	 * (VmExtractMemObj, compact(), func_get_args()) already skips them. */` |
|       62 |  342 | `	pFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|        - |  343 | `	/* Superglobals appear in get_defined_vars() ONLY at the global scope (php).` |
|        - |  344 | `	 * Inside a function the result is the local symbol table alone — a leak of` |
|        - |  345 | `	 * $argv/$_SERVER/... into every function's scope breaks callers that treat the` |
|        - |  346 | `	 * keys as real locals (e.g. PHPUnit's doubled-method template reflects each` |
|        - |  347 | `	 * get_defined_vars() name as a ReflectionParameter). */` |
|       62 |  348 | `	if( pFrame->pParent == 0 ){` |
|        6 |  349 | `		SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        2 |  350 | `	}` |
|        - |  351 | `	/* Then variables defined in the current frame, in DECLARATION order.` |
|        - |  352 | `	 * The frame table is head-pushed (SyHashInsert), so its forward order is` |
|        - |  353 | `	 * reverse-insertion; walk it backward to match php, which returns locals in` |
|        - |  354 | `	 * the order they first appeared (a,b,c — a reassignment reuses the slot and` |
|        - |  355 | `	 * keeps its original position). */` |
|       62 |  356 | `	SyHashForEachReverse(&pFrame->hVar,VmHashVarWalker,pArray);` |
|        - |  357 | `	/* Finally,return the created array */` |
|       62 |  358 | `	ph7_result_value(pCtx,pArray);` |
|       62 |  359 | `	return SXRET_OK;` |
|       33 |  360 | `}` |
|        - |  361 | `/*` |
|        - |  362 | ` * bool gettype($var)` |
|        - |  363 | ` *  Get the type of a variable` |
|        - |  364 | ` * Parameters` |
|        - |  365 | ` *   $var` |
|        - |  366 | ` *    The variable being type checked.` |
|        - |  367 | ` * Return` |
|        - |  368 | ` *   String representation of the given variable type.` |
|        - |  369 | ` */` |
|       78 |  370 | `PH7_PRIVATE int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  371 | `{` |
|        - |  372 | `	/* php's gettype() uses LONG type names ("integer"/"boolean"/"NULL"), distinct` |
|        - |  373 | `	 * from get_debug_type()'s short ones ("int"/"bool"/"null") — never route this` |
|        - |  374 | `	 * through PH7_MemObjTypeDump, which yields the short forms. */` |
|       80 |  375 | `	const char *zType = "unknown type";` |
|       80 |  376 | `	if( nArg > 0 ){` |
|       80 |  377 | `		ph7_value *pVal = apArg[0];` |
|       80 |  378 | `		if( pVal->iFlags & MEMOBJ_NULL ){` |
|        3 |  379 | `			zType = "NULL";` |
|       79 |  380 | `		}else if( pVal->iFlags & MEMOBJ_REAL ){` |
|        - |  381 | `			/* REAL wins over a cached MEMOBJ_INT: 1.0 is "double" (php). */` |
|        9 |  382 | `			zType = "double";` |
|       74 |  383 | `		}else if( pVal->iFlags & MEMOBJ_INT ){` |
|        3 |  384 | `			zType = "integer";` |
|       69 |  385 | `		}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|       35 |  386 | `			zType = "string";` |
|       51 |  387 | `		}else if( pVal->iFlags & MEMOBJ_BOOL ){` |
|        5 |  388 | `			zType = "boolean";` |
|       32 |  389 | `		}else if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|        8 |  390 | `			zType = "array";` |
|       26 |  391 | `		}else if( pVal->iFlags & MEMOBJ_OBJ ){` |
|        9 |  392 | `			zType = "object";` |
|       19 |  393 | `		}else if( pVal->iFlags & MEMOBJ_RES ){` |
|        - |  394 | `			/* php reports an fclose()'d handle as "resource (closed)" */` |
|       15 |  395 | `			zType = PH7_VfsResourceIsClosed(pVal->x.pOther) ? "resource (closed)" : "resource";` |
|        7 |  396 | `		}` |
|       39 |  397 | `	}` |
|        - |  398 | `	/* Return the variable type */` |
|       80 |  399 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       80 |  400 | `	return SXRET_OK;` |
|        2 |  401 | `}` |
|        - |  402 | `/*` |
|        - |  403 | ` * bool settype(mixed &$var, string $type)` |
|        - |  404 | ` *  Convert $var IN PLACE to the type named by $type, mirroring the (type) cast` |
|        - |  405 | ` *  operators exactly (same PH7_MemObjTo* helpers CVT_INT/REAL/STR/BOOL/ARRAY/OBJ` |
|        - |  406 | ` *  use), and write the result back through the by-ref out-param.` |
|        - |  407 | ` * Parameters` |
|        - |  408 | ` *   &$var : the variable to convert (compile.c's GenStateByRefBuiltinMask marks` |
|        - |  409 | ` *           position 0 by-reference so an undefined variable is auto-vivified).` |
|        - |  410 | ` *   $type : "int"/"integer", "float"/"double", "string", "bool"/"boolean",` |
|        - |  411 | ` *           "array", "object", "null" (case-insensitive).` |
|        - |  412 | ` * Return` |
|        - |  413 | ` *   Always true on success; a non-referenceable $var is an Error, an unknown` |
|        - |  414 | ` *   $type (or the un-castable "resource") is a ValueError — php-exact.` |
|        - |  415 | ` */` |
|       22 |  416 | `PH7_PRIVATE int vm_builtin_settype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 |  417 | `{` |
|        - |  418 | `	const char *zType;` |
|        - |  419 | `	int nLen;` |
|        - |  420 | `	ph7_value *pNew;` |
|        5 |  421 | `	SXUNUSED(nArg); /* arity (exactly 2) enforced by the central arity table */` |
|        - |  422 | `	/* php binds $var by reference at the call boundary: a literal/constant (no` |
|        - |  423 | `	 * caller slot, nIdx == SXU32_HIGH) is a catchable Error, raised BEFORE the` |
|        - |  424 | `	 * $type validation — the same signal + wording array_pop() & co use. */` |
|       25 |  425 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|        3 |  426 | `		return PH7_VmThrowException(pCtx,"Error",` |
|        - |  427 | `			"settype(): Argument #1 ($var) could not be passed by reference");` |
|        - |  428 | `	}` |
|       22 |  429 | `	zType = ph7_value_to_string(apArg[1],&nLen);` |
|        - |  430 | `	/* Validate the target type up-front (php checks $type before touching $var):` |
|        - |  431 | `	 * "resource" is a distinct ValueError, any other unknown name is the generic` |
|        - |  432 | `	 * invalid-type ValueError. */` |
|       22 |  433 | `	if( nLen == 8 && SyStrnicmp(zType,"resource",8) == 0 ){` |
|        3 |  434 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|        - |  435 | `			"Cannot convert to resource type");` |
|        - |  436 | `	}` |
|       21 |  437 | `	if( !(  (nLen == 3 && SyStrnicmp(zType,"int",3) == 0)` |
|       17 |  438 | `	     \|\| (nLen == 7 && SyStrnicmp(zType,"integer",7) == 0)` |
|       16 |  439 | `	     \|\| (nLen == 5 && SyStrnicmp(zType,"float",5) == 0)` |
|       15 |  440 | `	     \|\| (nLen == 6 && SyStrnicmp(zType,"double",6) == 0)` |
|       14 |  441 | `	     \|\| (nLen == 6 && SyStrnicmp(zType,"string",6) == 0)` |
|       12 |  442 | `	     \|\| (nLen == 4 && SyStrnicmp(zType,"bool",4) == 0)` |
|       11 |  443 | `	     \|\| (nLen == 7 && SyStrnicmp(zType,"boolean",7) == 0)` |
|       10 |  444 | `	     \|\| (nLen == 5 && SyStrnicmp(zType,"array",5) == 0)` |
|        8 |  445 | `	     \|\| (nLen == 6 && SyStrnicmp(zType,"object",6) == 0)` |
|        3 |  446 | `	     \|\| (nLen == 4 && SyStrnicmp(zType,"null",4) == 0) ) ){` |
|        3 |  447 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|        - |  448 | `			"settype(): Argument #2 ($type) must be a valid type");` |
|        - |  449 | `	}` |
|        - |  450 | `	/* Convert a COPY of the current value (the To* helpers mutate in place), then` |
|        - |  451 | `	 * store it through the by-ref out-param — settype changes $var's TYPE, so the` |
|        - |  452 | `	 * new value must land in the caller slot (nIdx), not just in shared contents. */` |
|       24 |  453 | `	pNew = ph7_context_new_scalar(pCtx);` |
|       24 |  454 | `	if( pNew == 0 ){` |
|      ! 0 |  455 | `		return PH7_ContextMemoryError(pCtx);` |
|        - |  456 | `	}` |
|       24 |  457 | `	PH7_MemObjStore(apArg[0],pNew);` |
|       22 |  458 | `	if( (nLen == 3 && SyStrnicmp(zType,"int",3) == 0)` |
|       23 |  459 | `	 \|\| (nLen == 7 && SyStrnicmp(zType,"integer",7) == 0) ){` |
|       10 |  460 | `		PH7_MemObjToInteger(pNew);` |
|       10 |  461 | `		MemObjSetType(pNew,MEMOBJ_INT);` |
|       26 |  462 | `	}else if( (nLen == 5 && SyStrnicmp(zType,"float",5) == 0)` |
|       21 |  463 | `	       \|\| (nLen == 6 && SyStrnicmp(zType,"double",6) == 0) ){` |
|        5 |  464 | `		PH7_MemObjToReal(pNew);` |
|        5 |  465 | `		MemObjSetType(pNew,MEMOBJ_REAL);` |
|       20 |  466 | `	}else if( nLen == 6 && SyStrnicmp(zType,"string",6) == 0 ){` |
|        - |  467 | `		/* php emits "Array to string conversion" for settype($arr,'string') */` |
|        6 |  468 | `		PH7_MemObjToStringUV(pNew);` |
|       16 |  469 | `	}else if( (nLen == 4 && SyStrnicmp(zType,"bool",4) == 0)` |
|       12 |  470 | `	       \|\| (nLen == 7 && SyStrnicmp(zType,"boolean",7) == 0) ){` |
|        5 |  471 | `		PH7_MemObjToBool(pNew);` |
|       11 |  472 | `	}else if( nLen == 5 && SyStrnicmp(zType,"array",5) == 0 ){` |
|        5 |  473 | `		PH7_MemObjToHashmap(pNew);` |
|        7 |  474 | `	}else if( nLen == 6 && SyStrnicmp(zType,"object",6) == 0 ){` |
|        3 |  475 | `		PH7_MemObjToObject(pNew);` |
|        2 |  476 | `	}else{` |
|        - |  477 | `		/* "null" — the only validated name left */` |
|        3 |  478 | `		PH7_MemObjToNull(pNew);` |
|        - |  479 | `	}` |
|       30 |  480 | `	PH7_VmStoreArgByRef(pCtx->pVm,apArg[0],pNew);` |
|       30 |  481 | `	ph7_result_bool(pCtx,1);` |
|       30 |  482 | `	return SXRET_OK;` |
|       20 |  483 | `}` |
|        - |  484 | `/*` |
|        - |  485 | ` * string get_resource_type(resource $handle)` |
|        - |  486 | ` *  This function gets the type of the given resource.` |
|        - |  487 | ` * Parameters` |
|        - |  488 | ` *  $handle` |
|        - |  489 | ` *  The evaluated resource handle.` |
|        - |  490 | ` * Return` |
|        - |  491 | ` *  If the given handle is a resource, this function will return a string` |
|        - |  492 | ` *  representing its type. If the type is not identified by this function` |
|        - |  493 | ` *  the return value will be the string Unknown.` |
|        - |  494 | ` *  This function will return FALSE and generate an error if handle` |
|        - |  495 | ` *  is not a resource.` |
|        - |  496 | ` */` |
|        8 |  497 | `PH7_PRIVATE int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  498 | `{` |
|        - |  499 | `	const char *zType;` |
|        9 |  500 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - |  501 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 |  502 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  503 | `		return PH7_OK;` |
|        - |  504 | `	}` |
|        - |  505 | `	/* PHP returns the resource TYPE name (e.g. "stream" for IO handles), not an id. */` |
|        9 |  506 | `	zType = PH7_VfsResourceType(apArg[0]->x.pOther);` |
|        9 |  507 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|        9 |  508 | `	return SXRET_OK;` |
|        5 |  509 | `}` |
|        - |  510 | `/*` |
|        - |  511 | ` * int get_resource_id(resource $resource)` |
|        - |  512 | ` *  Returns the integer id of a resource — the same number (int) casts to and` |
|        - |  513 | ` *  "Resource id #N" renders (php 8.0).` |
|        - |  514 | ` */` |
|        4 |  515 | `PH7_PRIVATE int vm_builtin_get_resource_id(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  516 | `{` |
|        6 |  517 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 |  518 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|        - |  519 | `			"get_resource_id(): Argument #1 ($resource) must be of type resource, %s given",` |
|      ! 0 |  520 | `			nArg < 1 ? "none" : ph7_type_name(apArg[0]));` |
|        - |  521 | `	}` |
|        6 |  522 | `	ph7_result_int64(pCtx,(ph7_int64)PH7_VmResourceId(pCtx->pVm,apArg[0]->x.pOther));` |
|        6 |  523 | `	return SXRET_OK;` |
|        4 |  524 | `}` |
|        - |  525 | `/*` |
|        - |  526 | ` * void var_dump(expression,....)` |
|        - |  527 | ` *   var_dump � Dumps information about a variable` |
|        - |  528 | ` * Parameters` |
|        - |  529 | ` *   One or more expression to dump.` |
|        - |  530 | ` * Returns` |
|        - |  531 | ` *  Nothing.` |
|        - |  532 | ` */` |
|     1430 |  533 | `PH7_PRIVATE int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  534 | `{` |
|        - |  535 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - |  536 | `	int i;` |
|     1435 |  537 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - |  538 | `	/* Dump one or more expressions */` |
|     3045 |  539 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1615 |  540 | `		ph7_value *pObj = apArg[i];` |
|        - |  541 | `		/* Reset the working buffer */` |
|     1615 |  542 | `		SyBlobReset(&sDump);` |
|        - |  543 | `		/* Dump the given expression */` |
|     1615 |  544 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - |  545 | `		/* Output */` |
|     1615 |  546 | `		if( SyBlobLength(&sDump) > 0 ){` |
|     1615 |  547 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      805 |  548 | `		}` |
|      810 |  549 | `	}` |
|        - |  550 | `	/* Release the working buffer */` |
|     1435 |  551 | `	SyBlobRelease(&sDump);` |
|     1435 |  552 | `	return SXRET_OK;` |
|        5 |  553 | `}` |
|        - |  554 | `/*` |
|        - |  555 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - |  556 | ` *   print-r - Prints human-readable information about a variable` |
|        - |  557 | ` * Parameters` |
|        - |  558 | ` *   expression: Expression to dump` |
|        - |  559 | ` *   return : If you would like to capture the output of print_r() use` |
|        - |  560 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - |  561 | ` *            print_r() will return the information rather than print it.` |
|        - |  562 | ` * Return` |
|        - |  563 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - |  564 | ` *  Otherwise, the return value is TRUE.` |
|        - |  565 | ` */` |
|       94 |  566 | `PH7_PRIVATE int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 |  567 | `{` |
|       97 |  568 | `	int ret_string = 0;` |
|        - |  569 | `	SyBlob sDump;` |
|       97 |  570 | `	if( nArg < 1 ){` |
|        - |  571 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  572 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  573 | `		return SXRET_OK;` |
|        - |  574 | `	}` |
|       97 |  575 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       97 |  576 | `	if ( nArg > 1 ){` |
|        - |  577 | `		/* Where to redirect output */` |
|        9 |  578 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        4 |  579 | `	}` |
|        - |  580 | `	/* Generate dump */` |
|       97 |  581 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       97 |  582 | `	if( !ret_string ){` |
|        - |  583 | `		/* Output dump */` |
|       89 |  584 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  585 | `		/* Return true */` |
|       89 |  586 | `		ph7_result_bool(pCtx,1);` |
|       46 |  587 | `	}else{` |
|        - |  588 | `		/* Generated dump as return value */` |
|        9 |  589 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  590 | `	}` |
|        - |  591 | `	/* Release the working buffer */` |
|       97 |  592 | `	SyBlobRelease(&sDump);` |
|       97 |  593 | `	return SXRET_OK;` |
|       50 |  594 | `}` |
|        - |  595 | `/*` |
|        - |  596 | ` * var_export() — PHP-exact evaluable representation of a value.` |
|        - |  597 | ` *` |
|        - |  598 | ` * Distinct from print_r/var_dump (which share PH7_MemObjDump): var_export emits` |
|        - |  599 | ` * parseable PHP — 'single-quoted' strings, true/false/NULL, array (\n  k => v,\n),` |
|        - |  600 | ` * and \Class::__set_state(array(...)) for objects. PHP's indentation is quirky` |
|        - |  601 | ` * (2-space array entries; 3-space object property lines; a composite value always` |
|        - |  602 | ` * opens at containerIndent+2) but deterministic; this matches it byte-for-byte.` |
|        - |  603 | ` */` |
|        - |  604 | `/* Instance flag (distinct from oo.c's CLASS_INSTANCE_DESTROYED 0x001) marking an` |
|        - |  605 | ` * object currently on the var_export recursion stack, for cycle detection. */` |
|        - |  606 | `#define VM_INSTANCE_DUMPING 0x002` |
|        - |  607 | `typedef struct VmExportCtx VmExportCtx;` |
|        - |  608 | `struct VmExportCtx` |
|        - |  609 | `{` |
|        - |  610 | `	SyBlob *pOut;` |
|        - |  611 | `	int nIndent;  /* indentation of the container emitting this entry */` |
|        - |  612 | `	int depth;    /* recursion guard */` |
|        - |  613 | `};` |
|        - |  614 | `static void VmExportValue(SyBlob *pOut, ph7_value *pVal, int nIndent, int depth);` |
|        - |  615 | `/* Append nIndent spaces. */` |
|      708 |  616 | `static void VmExportIndent(SyBlob *pOut, int nIndent)` |
|        5 |  617 | `{` |
|        - |  618 | `	int i;` |
|     2023 |  619 | `	for( i = 0; i < nIndent; i++ ){ SyBlobAppend(pOut," ",1); }` |
|      713 |  620 | `}` |
|        - |  621 | `/* Append a single-quoted PHP string literal (escapes ' and \, batching safe runs` |
|        - |  622 | ` * in one append). A NUL byte can't live in a single-quoted literal, so PHP splits` |
|        - |  623 | ` * it out as ' . "\0" . ' — match that. */` |
|      666 |  624 | `static void VmExportQuoted(SyBlob *pOut, const char *z, int n)` |
|        5 |  625 | `{` |
|      671 |  626 | `	int i, run = 0;` |
|      671 |  627 | `	SyBlobAppend(pOut,"'",1);` |
|     3591 |  628 | `	for( i = 0; i < n; i++ ){` |
|     2925 |  629 | `		char c = z[i];` |
|     2925 |  630 | `		if( c != '\0' && c != '\'' && c != '\\' ){ continue; } /* extend the safe run */` |
|        7 |  631 | `		if( i > run ){ SyBlobAppend(pOut,&z[run],(sxu32)(i-run)); }` |
|        7 |  632 | `		if( c == '\0' ){ SyBlobAppend(pOut,"' . \"\\0\" . '",12); }` |
|        5 |  633 | `		else { SyBlobAppend(pOut,"\\",1); SyBlobAppend(pOut,&z[i],1); }` |
|        7 |  634 | `		run = i+1;` |
|        4 |  635 | `	}` |
|      671 |  636 | `	if( n > run ){ SyBlobAppend(pOut,&z[run],(sxu32)(n-run)); }` |
|      671 |  637 | `	SyBlobAppend(pOut,"'",1);` |
|      671 |  638 | `}` |
|        - |  639 | `/* True if the array/object is already on the var_export recursion stack. */` |
|      462 |  640 | `static int VmExportIsCycle(ph7_value *pVal)` |
|        5 |  641 | `{` |
|      467 |  642 | `	if( ph7_value_is_array(pVal) ){` |
|       41 |  643 | `		return (((ph7_hashmap *)pVal->x.pOther)->iFlags & HASHMAP_DUMPING) != 0;` |
|        - |  644 | `	}` |
|      429 |  645 | `	if( ph7_value_is_object(pVal) ){` |
|        5 |  646 | `		return (((ph7_class_instance *)pVal->x.pOther)->iFlags & VM_INSTANCE_DUMPING) != 0;` |
|        - |  647 | `	}` |
|      425 |  648 | `	return 0;` |
|      236 |  649 | `}` |
|        - |  650 | `/* Emit " => " then the value: a scalar inline, a composite on its own line at` |
|        - |  651 | ` * containerIndent+2. A circular reference renders inline as NULL (like PHP). */` |
|      462 |  652 | `static void VmExportEntryValue(SyBlob *pOut, ph7_value *pVal, int nContainerIndent, int depth)` |
|        5 |  653 | `{` |
|      467 |  654 | `	SyBlobAppend(pOut," => ",4);` |
|      467 |  655 | `	if( VmExportIsCycle(pVal) ){` |
|        5 |  656 | `		SyBlobAppend(pOut,"NULL",4);` |
|      465 |  657 | `	}else if( ph7_value_is_array(pVal) \|\| ph7_value_is_object(pVal) ){` |
|       41 |  658 | `		SyBlobAppend(pOut,"\n",1);` |
|       41 |  659 | `		VmExportIndent(pOut,nContainerIndent+2);` |
|       41 |  660 | `		VmExportValue(pOut,pVal,nContainerIndent+2,depth+1);` |
|       22 |  661 | `	}else{` |
|      425 |  662 | `		VmExportValue(pOut,pVal,nContainerIndent+2,depth+1);` |
|        - |  663 | `	}` |
|      467 |  664 | `	SyBlobAppend(pOut,",\n",2);` |
|      467 |  665 | `}` |
|        - |  666 | `/* Array walker: "<indent+2>key => value,\n" for each entry. */` |
|      420 |  667 | `static int VmExportArrayWalk(ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|        5 |  668 | `{` |
|      425 |  669 | `	VmExportCtx *pC = (VmExportCtx *)pUserData;` |
|      425 |  670 | `	VmExportIndent(pC->pOut,pC->nIndent+2);` |
|      425 |  671 | `	if( ph7_value_is_string(pKey) ){` |
|        - |  672 | `		int n;` |
|      109 |  673 | `		const char *z = ph7_value_to_string(pKey,&n);` |
|      109 |  674 | `		VmExportQuoted(pC->pOut,z,n);` |
|       57 |  675 | `	}else{` |
|      321 |  676 | `		SyBlobFormat(pC->pOut,"%qd",ph7_value_to_int64(pKey));` |
|        - |  677 | `	}` |
|      425 |  678 | `	VmExportEntryValue(pC->pOut,pValue,pC->nIndent,pC->depth);` |
|      425 |  679 | `	return PH7_OK;` |
|        5 |  680 | `}` |
|     2414 |  681 | `static void VmExportValue(SyBlob *pOut, ph7_value *pVal, int nIndent, int depth)` |
|        5 |  682 | `{` |
|     2419 |  683 | `	if( depth > 4096 ){ return; } /* backstop for pathological finite nesting */` |
|     2419 |  684 | `	if( ph7_value_is_null(pVal) ){` |
|       93 |  685 | `		SyBlobAppend(pOut,"NULL",4);` |
|     2375 |  686 | `	}else if( ph7_value_is_bool(pVal) ){` |
|      703 |  687 | `		if( ph7_value_to_bool(pVal) ){ SyBlobAppend(pOut,"true",4); }` |
|      313 |  688 | `		else { SyBlobAppend(pOut,"false",5); }` |
|     1982 |  689 | `	}else if( ph7_value_is_float(pVal) ){` |
|        - |  690 | `		/* float before int: ph7_value_is_int is lenient (true for integer reals). */` |
|      329 |  691 | `		sxu32 before = SyBlobLength(pOut), i, after;` |
|        - |  692 | `		const char *z;` |
|      329 |  693 | `		int plain = 1;` |
|      329 |  694 | `		PH7_AppendShortestReal(pOut,ph7_value_to_double(pVal));` |
|      329 |  695 | `		z = (const char *)SyBlobData(pOut);` |
|      329 |  696 | `		after = SyBlobLength(pOut);` |
|      813 |  697 | `		for( i = before; i < after; i++ ){` |
|      669 |  698 | `			if( !((z[i]>='0'&&z[i]<='9')\|\|z[i]=='-') ){ plain = 0; break; }` |
|      245 |  699 | `		}` |
|      329 |  700 | `		if( plain ){ SyBlobAppend(pOut,".0",2); } /* integer-form floats render 1.0/100.0 */` |
|     1470 |  701 | `	}else if( ph7_value_is_int(pVal) ){` |
|      577 |  702 | `		sxi64 iVal = ph7_value_to_int64(pVal);` |
|      577 |  703 | `		if( iVal == SMALLEST_INT64 ){` |
|        - |  704 | `			/* php renders LONG_MIN as an expression: the positive literal` |
|        - |  705 | `			 * 9223372036854775808 would not be representable when eval'd. */` |
|        5 |  706 | `			SyBlobAppend(pOut,"-9223372036854775807-1",sizeof("-9223372036854775807-1")-1);` |
|        3 |  707 | `		}else{` |
|      573 |  708 | `			SyBlobFormat(pOut,"%qd",iVal);` |
|        5 |  709 | `		}` |
|     1021 |  710 | `	}else if( ph7_value_is_string(pVal) ){` |
|        - |  711 | `		int n;` |
|      525 |  712 | `		const char *z = ph7_value_to_string(pVal,&n);` |
|      525 |  713 | `		VmExportQuoted(pOut,z,n);` |
|      475 |  714 | `	}else if( ph7_value_is_array(pVal) ){` |
|      191 |  715 | `		ph7_hashmap *pMap = (ph7_hashmap *)pVal->x.pOther;` |
|      191 |  716 | `		if( pMap->iFlags & HASHMAP_DUMPING ){` |
|      ! 0 |  717 | `			SyBlobAppend(pOut,"NULL",4); /* circular reference -> NULL, like PHP */` |
|      ! 0 |  718 | `		}else{` |
|        - |  719 | `			VmExportCtx ctx;` |
|      191 |  720 | `			pMap->iFlags \|= HASHMAP_DUMPING;` |
|      191 |  721 | `			SyBlobAppend(pOut,"array (\n",8);` |
|      191 |  722 | `			ctx.pOut = pOut; ctx.nIndent = nIndent; ctx.depth = depth;` |
|      191 |  723 | `			ph7_array_walk(pVal,VmExportArrayWalk,&ctx);` |
|      191 |  724 | `			VmExportIndent(pOut,nIndent);` |
|      191 |  725 | `			SyBlobAppend(pOut,")",1);` |
|      191 |  726 | `			pMap->iFlags &= ~HASHMAP_DUMPING;` |
|        5 |  727 | `		}` |
|      119 |  728 | `	}else if( ph7_value_is_object(pVal) ){` |
|       26 |  729 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|       26 |  730 | `		if( pThis->pClass->iFlags & PH7_CLASS_ENUM ){` |
|        - |  731 | ``			/* php 8.1: an enum case exports as `\S::A` */`` |
|        3 |  732 | `			ph7_value *pName = PH7_EnumCaseNameValue(pThis);` |
|        3 |  733 | `			SyBlobAppend(pOut,"\\",1);` |
|        3 |  734 | `			SyBlobAppend(pOut,pThis->pClass->sName.zString,pThis->pClass->sName.nByte);` |
|        3 |  735 | `			SyBlobAppend(pOut,"::",2);` |
|        3 |  736 | `			if( pName && SyBlobLength(&pName->sBlob) > 0 ){` |
|        3 |  737 | `				SyBlobAppend(pOut,SyBlobData(&pName->sBlob),SyBlobLength(&pName->sBlob));` |
|        2 |  738 | `			}` |
|       25 |  739 | `		}else if( pThis->iFlags & VM_INSTANCE_DUMPING ){` |
|      ! 0 |  740 | `			SyBlobAppend(pOut,"NULL",4); /* circular reference -> NULL, like PHP */` |
|      ! 0 |  741 | `		}else{` |
|       24 |  742 | `			SyString *pClassName = &pThis->pClass->sName;` |
|        - |  743 | `			SyHashEntry *pEntry;` |
|        - |  744 | `			SySet sNames;` |
|        - |  745 | `			SyString *aName;` |
|        - |  746 | `			sxu32 iName,nName;` |
|       24 |  747 | `			pThis->iFlags \|= VM_INSTANCE_DUMPING;` |
|       24 |  748 | `			SyBlobAppend(pOut,"\\",1);` |
|       24 |  749 | `			SyBlobAppend(pOut,pClassName->zString,pClassName->nByte);` |
|       24 |  750 | `			SyBlobAppend(pOut,"::__set_state(array(\n",21);` |
|        - |  751 | `			/* SNAPSHOT the attribute names first: a PHP 8.4 get hook dispatched` |
|        - |  752 | `			 * mid-walk may re-enter an hAttr walk on this instance (the hash has` |
|        - |  753 | `			 * a single embedded loop cursor) or unset()/create properties; names` |
|        - |  754 | `			 * point into class-owned attr storage and each is re-looked-up. */` |
|       24 |  755 | `			SySetInit(&sNames,&pThis->pVm->sAllocator,sizeof(SyString));` |
|       24 |  756 | `			SyHashResetLoopCursor(&pThis->hAttr);` |
|       68 |  757 | `			while( (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       46 |  758 | `				VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|       46 |  759 | `				if( pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){ continue; }` |
|       44 |  760 | `				if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_VIRTUAL))` |
|       24 |  761 | `				 == PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|        3 |  762 | `					continue; /* virtual set-only property: no value to export (php) */` |
|        - |  763 | `				}` |
|       44 |  764 | `				SySetPut(&sNames,(const void *)&pVmAttr->pAttr->sName);` |
|        2 |  765 | `			}` |
|       24 |  766 | `			aName = (SyString *)SySetBasePtr(&sNames);` |
|       24 |  767 | `			nName = SySetUsed(&sNames);` |
|       66 |  768 | `			for( iName = 0 ; iName < nName ; ++iName ){` |
|       44 |  769 | `				SyString *pAName = &aName[iName];` |
|        - |  770 | `				VmClassAttr *pVmAttr;` |
|        - |  771 | `				ph7_value *pAttrVal;` |
|       44 |  772 | `				pEntry = SyHashGet(&pThis->hAttr,(const void *)pAName->zString,pAName->nByte);` |
|       44 |  773 | `				if( pEntry == 0 ){ continue; } /* unset by an earlier hook */` |
|       44 |  774 | `				pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|       44 |  775 | `				VmExportIndent(pOut,nIndent+3); /* object property lines sit one deeper than arrays */` |
|       44 |  776 | `				VmExportQuoted(pOut,pAName->zString,(int)pAName->nByte);` |
|        - |  777 | `				/* PHP 8.4 property hooks: var_export() reads through the get hook` |
|        - |  778 | `				 * (every visibility — php exports private hooked values too). A` |
|        - |  779 | `				 * throwing hook parks on the boundary rail; the helper's boundary` |
|        - |  780 | `				 * gate keeps LATER hooks from running (the raw values the tail of` |
|        - |  781 | `				 * the export falls back to are discarded when the throw routes). */` |
|        - |  782 | `				{` |
|        - |  783 | `					ph7_value sHookVal;` |
|        - |  784 | `					sxi32 rcHk;` |
|       44 |  785 | `					PH7_MemObjInit(pThis->pVm,&sHookVal);` |
|       44 |  786 | `					rcHk = PH7_VmHookGetAttrValue(pThis,pVmAttr,&sHookVal);` |
|       44 |  787 | `					if( rcHk == SXRET_OK ){` |
|       13 |  788 | `						VmExportEntryValue(pOut,&sHookVal,nIndent,depth);` |
|       13 |  789 | `						PH7_MemObjRelease(&sHookVal);` |
|       13 |  790 | `						continue;` |
|        - |  791 | `					}` |
|       32 |  792 | `					PH7_MemObjRelease(&sHookVal);` |
|       32 |  793 | `					if( rcHk != SXERR_NOTFOUND ){` |
|        - |  794 | `						/* the hook threw (parked on the boundary rail): NULL` |
|        - |  795 | `						 * placeholder keeps the output well-formed */` |
|      ! 0 |  796 | `						SyBlobAppend(pOut," => NULL,\n",10);` |
|      ! 0 |  797 | `						continue;` |
|        - |  798 | `					}` |
|        - |  799 | `				}` |
|       32 |  800 | `				pAttrVal = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       32 |  801 | `				if( pAttrVal ){ VmExportEntryValue(pOut,pAttrVal,nIndent,depth); }` |
|      ! 0 |  802 | `				else { SyBlobAppend(pOut," => NULL,\n",10); }` |
|       17 |  803 | `			}` |
|       24 |  804 | `			SySetRelease(&sNames);` |
|       24 |  805 | `			VmExportIndent(pOut,nIndent);` |
|       24 |  806 | `			SyBlobAppend(pOut,"))",2);` |
|       24 |  807 | `			pThis->iFlags &= ~VM_INSTANCE_DUMPING;` |
|        - |  808 | `		}` |
|       14 |  809 | `	}else{` |
|        - |  810 | `		/* resource / other -> PHP emits NULL (with a warning we omit) */` |
|      ! 0 |  811 | `		SyBlobAppend(pOut,"NULL",4);` |
|        - |  812 | `	}` |
|     1212 |  813 | `}` |
|        - |  814 | `/*` |
|        - |  815 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - |  816 | ` *  PHP-exact evaluable representation (see VmExportValue).` |
|        - |  817 | ` */` |
|     1956 |  818 | `PH7_PRIVATE int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  819 | `{` |
|     1961 |  820 | `	int ret_string = 0;` |
|        - |  821 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|     1961 |  822 | `	if( nArg < 1 ){` |
|        - |  823 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  824 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  825 | `		return SXRET_OK;` |
|        - |  826 | `	}` |
|     1961 |  827 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|     1961 |  828 | `	if ( nArg > 1 ){` |
|        - |  829 | `		/* Where to redirect output */` |
|     1523 |  830 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|      759 |  831 | `	}` |
|        - |  832 | `	/* Generate the PHP-exact evaluable representation */` |
|     1961 |  833 | `	VmExportValue(&sDump,apArg[0],0,0);` |
|     1961 |  834 | `	if( !ret_string ){` |
|        - |  835 | `		/* Output dump */` |
|      443 |  836 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  837 | `		/* Return NULL */` |
|      443 |  838 | `		ph7_result_null(pCtx);` |
|      224 |  839 | `	}else{` |
|        - |  840 | `		/* Generated dump as return value */` |
|     1523 |  841 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  842 | `	}` |
|        - |  843 | `	/* Release the working buffer */` |
|     1961 |  844 | `	SyBlobRelease(&sDump);` |
|     1961 |  845 | `	return SXRET_OK;` |
|      983 |  846 | `}` |
|        - |  847 |  |
