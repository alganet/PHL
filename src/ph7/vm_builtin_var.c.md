# src/ph7/vm_builtin_var.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 440/481 lines (91.48%)

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
|  3027154 |   23 | `PH7_PRIVATE int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |   24 | `{` |
|        - |   25 | `	ph7_value *pObj;` |
|  3027159 |   26 | `	int res = 0;` |
|        - |   27 | `	int i;` |
|  3027159 |   28 | `	if( nArg < 1 ){` |
|        - |   29 | `		/* Missing arguments,return false */` |
|      ! 0 |   30 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 |   31 | `		return SXRET_OK;` |
|        - |   32 | `	}` |
|        - |   33 | `	/* Iterate over available arguments */` |
|  4508975 |   34 | `	for( i = 0 ; i < nArg ; ++i ){` |
|  3027171 |   35 | `		pObj = apArg[i];` |
|  3027171 |   36 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|        - |   37 | `			/* Skip the "expecting a variable" warning for MEMOBJ_BOOL —` |
|        - |   38 | `			 * synthesized by LOAD_IDX iP2=4 (ArrayAccess::offsetExists) and` |
|        - |   39 | `			 * by anyone passing a bool literal (rare, harmless). */` |
|    95861 |   40 | `			if( (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|        - |   41 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 |   42 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 |   43 | `			}` |
|    47928 |   44 | `		}` |
|  3027171 |   45 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|  3027171 |   46 | `		if( !res ){` |
|        - |   47 | `			/* Variable not set,return FALSE */` |
|  1545355 |   48 | `			ph7_result_bool(pCtx,0);` |
|  1545355 |   49 | `			return SXRET_OK;` |
|        - |   50 | `		}` |
|   740913 |   51 | `	}` |
|        - |   52 | `	/* All given variable are set,return TRUE */` |
|  1481809 |   53 | `	ph7_result_bool(pCtx,1);` |
|  1481809 |   54 | `	return SXRET_OK;` |
|  1513582 |   55 | `}` |
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
|     6994 |   75 | `PH7_PRIVATE sxi32 VmUnsetVarByName(ph7_vm *pVm,VmFrame *pFrame,const char *zName,sxu32 nByte)` |
|        5 |   76 | `{` |
|        - |   77 | `	SyHashEntry *pEntry;` |
|        - |   78 | `	VmRefObj *pRef;` |
|        - |   79 | `	sxu32 nIdx;` |
|        - |   80 | `	/* php 8.1 forbids unset($GLOBALS) outright. Checked by NAME: the superglobal is not an` |
|        - |   81 | `	 * ordinary hVar binding, so the slot-index test below never sees it. */` |
|     6999 |   82 | `	if( nByte == sizeof("GLOBALS")-1 && SyMemcmp(zName,"GLOBALS",nByte) == 0 ){` |
|        3 |   83 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |   84 | `			"$GLOBALS can only be modified using the $GLOBALS[$name] = $value syntax");` |
|        3 |   85 | `		pVm->iExitStatus = 255;` |
|        3 |   86 | `		pVm->bHaltRequested = 1;` |
|        3 |   87 | `		return PH7_ABORT;` |
|        - |   88 | `	}` |
|     6997 |   89 | `	pEntry = SyHashGet(&pFrame->hVar,(const void *)zName,nByte);` |
|     6997 |   90 | `	if( pEntry == 0 ){` |
|        - |   91 | `		/* No such variable: unset() is a no-op on an undefined name, as in php */` |
|     1057 |   92 | `		return SXRET_OK;` |
|        - |   93 | `	}` |
|     5944 |   94 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|     5944 |   95 | `	if( nIdx == pVm->nGlobalIdx ){` |
|      ! 0 |   96 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |   97 | `			"$GLOBALS can only be modified using the $GLOBALS[$name] = $value syntax");` |
|      ! 0 |   98 | `		pVm->iExitStatus = 255;` |
|      ! 0 |   99 | `		pVm->bHaltRequested = 1;` |
|      ! 0 |  100 | `		return PH7_ABORT;` |
|        - |  101 | `	}` |
|     5944 |  102 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|        - |  103 | `	/*` |
|        - |  104 | `	 * A GLOBAL variable is ALSO an entry of the $GLOBALS array, and that node points at the` |
|        - |  105 | `	 * same slot. php's unset($x) in global scope drops $GLOBALS['x'] too, so unlink the node` |
|        - |  106 | `	 * — otherwise it stays a live holder and the value is never released ($o = new D;` |
|        - |  107 | `	 * unset($o); stopped running the destructor). Clear the reference table's row for the` |
|        - |  108 | `	 * node BEFORE unlinking it: unlinking frees the node, and the holder count below would` |
|        - |  109 | `	 * otherwise dereference freed memory.` |
|        - |  110 | `	 */` |
|     5944 |  111 | `	if( pFrame->pParent == 0 ){` |
|     5931 |  112 | `		ph7_value *pGlobals = (ph7_value *)SySetAt(&pVm->aMemObj,pVm->nGlobalIdx);` |
|     5931 |  113 | `		if( pGlobals && (pGlobals->iFlags & MEMOBJ_HASHMAP) ){` |
|     5931 |  114 | `			ph7_hashmap_node *pNode = 0;` |
|        - |  115 | `			ph7_value sKey;` |
|        - |  116 | `			SyString sName;` |
|     5931 |  117 | `			PH7_MemObjInit(&(*pVm),&sKey);` |
|     5931 |  118 | `			SyStringInitFromBuf(&sName,zName,nByte);` |
|     5931 |  119 | `			PH7_MemObjInitFromString(&(*pVm),&sKey,&sName);` |
|     5928 |  120 | `			if( SXRET_OK == PH7_HashmapLookup((ph7_hashmap *)pGlobals->x.pOther,&sKey,&pNode)` |
|     5931 |  121 | `			 && pNode && pNode->nValIdx == nIdx ){` |
|     5929 |  122 | `				if( pRef ){` |
|     5929 |  123 | `					ph7_hashmap_node **apN = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|        - |  124 | `					sxu32 k;` |
|    11953 |  125 | `					for( k = 0 ; k < SySetUsed(&pRef->aArrEntries) ; ++k ){` |
|     6027 |  126 | `						if( apN[k] == pNode ){` |
|     5929 |  127 | `							apN[k] = 0;` |
|     2963 |  128 | `						}` |
|     3015 |  129 | `					}` |
|     2963 |  130 | `				}` |
|     5929 |  131 | `				PH7_HashmapUnlinkNode(pNode,FALSE);` |
|     2963 |  132 | `			}` |
|     5931 |  133 | `			PH7_MemObjRelease(&sKey);` |
|     2964 |  134 | `		}` |
|     2964 |  135 | `	}` |
|        - |  136 |  |
|     5944 |  137 | `	if( pRef == 0 ){` |
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
|     5944 |  148 | `		SyHashEntry **apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|     5944 |  149 | `		ph7_hashmap_node **apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|     5944 |  150 | `		sxu32 n, nLive = 0;` |
|        - |  151 | `		/* Forget THIS name in the slot's reference record (leave the others alone) */` |
|    11980 |  152 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; ++n ){` |
|     6040 |  153 | `			if( apEntry[n] == pEntry ){` |
|     5938 |  154 | `				apEntry[n] = 0;` |
|     2967 |  155 | `			}` |
|     3022 |  156 | `		}` |
|     5944 |  157 | `		SyHashDeleteEntry2(pEntry);` |
|        - |  158 | `		/* Anything else still holding the slot? */` |
|    11980 |  159 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; ++n ){` |
|     6040 |  160 | `			if( apEntry[n] ){` |
|       29 |  161 | `				nLive++;` |
|       14 |  162 | `			}` |
|     3022 |  163 | `		}` |
|    11970 |  164 | `		for( n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|        - |  165 | `			/* Only a node that STILL points at this slot is a holder. The reference table` |
|        - |  166 | `			 * keeps stale rows (a slot index is recycled through the free list, and the row` |
|        - |  167 | `			 * outlives the node that put it there), so an un-filtered count reports holders` |
|        - |  168 | `			 * that no longer exist and the value would never be released — the destructor` |
|        - |  169 | ``			 * of `$o = new D; unset($o);` stopped running. */`` |
|     6029 |  170 | `			if( apNode[n] && apNode[n]->nValIdx == nIdx ){` |
|       63 |  171 | `				nLive++;` |
|       31 |  172 | `			}` |
|     3016 |  173 | `		}` |
|     5944 |  174 | `		if( nLive < 1 ){` |
|        - |  175 | `			/* Last holder gone: now the value may go too */` |
|     5888 |  176 | `			PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|        - |  177 | `			/* Slot returned to the free pool: drop its stale local-teardown entry so a` |
|        - |  178 | `			 * later reuse of the index is not double-freed on frame exit (see` |
|        - |  179 | `			 * VmDropFrameLocalSlot). */` |
|     5888 |  180 | `			VmDropFrameLocalSlot(&(*pVm),nIdx);` |
|     2942 |  181 | `		}` |
|        - |  182 | `	}` |
|     5944 |  183 | `	return SXRET_OK;` |
|     3502 |  184 | `}` |
|        - |  185 | `/*` |
|        - |  186 | ` * Is this memory slot aliased — i.e. does anything other than its owner refer to it?` |
|        - |  187 | ` * var_dump marks such an array element with '&' ("&int(2)"). PH7 only flagged nodes that` |
|        - |  188 | `` * were FOREIGN (`array(&$x)`, where the node points at an outside slot) and so missed the`` |
|        - |  189 | `` * common case, a reference taken TO an element (`$r = &$a[1]`), where the array still owns`` |
|        - |  190 | ` * the value but is no longer its only holder.` |
|        - |  191 | ` */` |
|   769668 |  192 | `PH7_PRIVATE int PH7_VmSlotIsReferenced(ph7_vm *pVm,sxu32 nIdx)` |
|        5 |  193 | `{` |
|        - |  194 | `	VmRefObj *pRef;` |
|   769673 |  195 | `	sxu32 n, nLive = 0;` |
|        - |  196 | `	SyHashEntry **apEntry;` |
|   769673 |  197 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  198 | `		return 0;` |
|        - |  199 | `	}` |
|   769673 |  200 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|   769673 |  201 | `	if( pRef == 0 ){` |
|      ! 0 |  202 | `		return 0;` |
|        - |  203 | `	}` |
|   769673 |  204 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|   769679 |  205 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; ++n ){` |
|        8 |  206 | `		if( apEntry[n] ){` |
|        8 |  207 | `			nLive++;` |
|        3 |  208 | `		}` |
|        5 |  209 | `	}` |
|   769673 |  210 | `	return nLive > 0;` |
|   384839 |  211 | `}` |
| 22357734 |  212 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        5 |  213 | `{` |
|        - |  214 | `	ph7_value *pObj;` |
|        - |  215 | `	VmRefObj *pRef;` |
| 22357739 |  216 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
| 22357739 |  217 | `	if( pObj ){` |
|        - |  218 | `		/* Release the object */` |
| 22357739 |  219 | `		PH7_MemObjRelease(pObj);` |
| 11180055 |  220 | `	}` |
|        - |  221 | `	/* Remove old reference links */` |
| 22357739 |  222 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
| 22357739 |  223 | `	if( pRef ){` |
| 22357047 |  224 | `		sxi32 iFlags = pRef->iFlags;` |
|        - |  225 | `		/* Unlink from the reference table */` |
| 22357047 |  226 | `		VmRefObjUnlink(&(*pVm),pRef);` |
| 22357047 |  227 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - |  228 | `			VmSlot sFree;` |
|        - |  229 | `			/* Restore to the free list */` |
| 22357029 |  230 | `			sFree.nIdx = nObjIdx;` |
| 22357029 |  231 | `			sFree.pUserData = 0;` |
| 22357029 |  232 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
| 11179700 |  233 | `		}` |
| 11179709 |  234 | `	}` |
| 22357739 |  235 | `	return SXRET_OK;` |
|        5 |  236 | `}` |
|        - |  237 | `/*` |
|        - |  238 | ` * void unset($var,...)` |
|        - |  239 | ` *   Unset one or more given variable.` |
|        - |  240 | ` * Parameters` |
|        - |  241 | ` *  One or more variable to unset.` |
|        - |  242 | ` * Return` |
|        - |  243 | ` *  Nothing.` |
|        - |  244 | ` */` |
|     1230 |  245 | `PH7_PRIVATE int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  246 | `{` |
|        - |  247 | `	ph7_value *pObj;` |
|        - |  248 | `	ph7_vm *pVm;` |
|        - |  249 | `	int i;` |
|        - |  250 | `	/* Point to the target VM */` |
|     1235 |  251 | `	pVm = pCtx->pVm;` |
|        - |  252 | `	/* Iterate and unset */` |
|     2465 |  253 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     1235 |  254 | `		pObj = apArg[i];` |
|     1235 |  255 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      332 |  256 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  257 | `				/* Throw an error */` |
|      ! 0 |  258 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 |  259 | `			}` |
|      168 |  260 | `		}else{` |
|      907 |  261 | `			sxu32 nIdx = pObj->nIdx;` |
|      907 |  262 | `			if( nIdx == pVm->nGlobalIdx ){` |
|        - |  263 | `				/* php 8.1: unset($GLOBALS) is forbidden — the same fatal as` |
|        - |  264 | `				 * re-assigning it (compile-time in php, raised here). */` |
|      ! 0 |  265 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  266 | `					"$GLOBALS can only be modified using the $GLOBALS[$name] = $value syntax");` |
|      ! 0 |  267 | `				pVm->iExitStatus = 255;` |
|      ! 0 |  268 | `				pVm->bHaltRequested = 1;` |
|      ! 0 |  269 | `				return PH7_ABORT;` |
|        - |  270 | `			}` |
|      907 |  271 | `			PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|        - |  272 | `			/* Drop the stale local-teardown entry for the freed slot (see` |
|        - |  273 | `			 * VmDropFrameLocalSlot) so a later index reuse is not double-freed. */` |
|      907 |  274 | `			VmDropFrameLocalSlot(&(*pVm),nIdx);` |
|        - |  275 | `		}` |
|      620 |  276 | `	}` |
|     1235 |  277 | `	return SXRET_OK;` |
|      620 |  278 | `}` |
|        - |  279 | `/*` |
|        - |  280 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - |  281 | ` */` |
|      806 |  282 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        4 |  283 | `{` |
|      810 |  284 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      810 |  285 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  286 | `	ph7_value *pObj;` |
|        - |  287 | `	sxu32 nIdx;` |
|        - |  288 | `	/* php excludes $this from get_defined_vars() (it is bound implicitly, not a` |
|        - |  289 | `	 * declared local). PHL keeps it in the frame's hVar, so skip it here. */` |
|      806 |  290 | `	if( pEntry->nKeyLen == sizeof("this")-1` |
|      472 |  291 | `	 && SyMemcmp(pEntry->pKey,"this",sizeof("this")-1) == 0 ){` |
|        6 |  292 | `		return SXRET_OK;` |
|        - |  293 | `	}` |
|        - |  294 | `	/* Extract the memory object */` |
|      806 |  295 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      806 |  296 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      806 |  297 | `	if( pObj ){` |
|      806 |  298 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      802 |  299 | `			if( pEntry->nKeyLen > 0 ){` |
|        - |  300 | `				SyString sName;` |
|        - |  301 | `				ph7_value sKey;` |
|        - |  302 | `				/* Perform the insertion (pObj may point into pVm->aMemObj; the` |
|        - |  303 | `				 * inserter snapshots the source before reserving, so the pool may` |
|        - |  304 | `				 * safely move underneath it — see HashmapInsertIntKey/BlobKey). */` |
|      802 |  305 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      802 |  306 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      802 |  307 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      802 |  308 | `				PH7_MemObjRelease(&sKey);` |
|      399 |  309 | `			}` |
|      399 |  310 | `		}` |
|      401 |  311 | `	}` |
|      806 |  312 | `	return SXRET_OK;` |
|      407 |  313 | `}` |
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
|      166 |  370 | `PH7_PRIVATE int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 |  371 | `{` |
|        - |  372 | `	/* php's gettype() uses LONG type names ("integer"/"boolean"/"NULL"), distinct` |
|        - |  373 | `	 * from get_debug_type()'s short ones ("int"/"bool"/"null") — never route this` |
|        - |  374 | `	 * through PH7_MemObjTypeDump, which yields the short forms. */` |
|      169 |  375 | `	const char *zType = "unknown type";` |
|      169 |  376 | `	if( nArg > 0 ){` |
|      169 |  377 | `		ph7_value *pVal = apArg[0];` |
|      169 |  378 | `		if( pVal->iFlags & MEMOBJ_NULL ){` |
|        3 |  379 | `			zType = "NULL";` |
|      168 |  380 | `		}else if( pVal->iFlags & MEMOBJ_REAL ){` |
|        - |  381 | `			/* REAL wins over a cached MEMOBJ_INT: 1.0 is "double" (php). */` |
|        9 |  382 | `			zType = "double";` |
|      163 |  383 | `		}else if( pVal->iFlags & MEMOBJ_INT ){` |
|       16 |  384 | `			zType = "integer";` |
|      151 |  385 | `		}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|      112 |  386 | `			zType = "string";` |
|       89 |  387 | `		}else if( pVal->iFlags & MEMOBJ_BOOL ){` |
|        5 |  388 | `			zType = "boolean";` |
|       32 |  389 | `		}else if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|        8 |  390 | `			zType = "array";` |
|       26 |  391 | `		}else if( pVal->iFlags & MEMOBJ_OBJ ){` |
|        9 |  392 | `			zType = "object";` |
|       19 |  393 | `		}else if( pVal->iFlags & MEMOBJ_RES ){` |
|        - |  394 | `			/* php reports an fclose()'d handle as "resource (closed)" */` |
|       15 |  395 | `			zType = PH7_VfsResourceIsClosed(pVal->x.pOther) ? "resource (closed)" : "resource";` |
|        7 |  396 | `		}` |
|       83 |  397 | `	}` |
|        - |  398 | `	/* Return the variable type */` |
|      169 |  399 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|      169 |  400 | `	return SXRET_OK;` |
|        3 |  401 | `}` |
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
|       40 |  416 | `PH7_PRIVATE int vm_builtin_settype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        4 |  417 | `{` |
|        - |  418 | `	const char *zType;` |
|        - |  419 | `	int nLen;` |
|        - |  420 | `	ph7_value *pNew;` |
|       10 |  421 | `	SXUNUSED(nArg); /* arity (exactly 2) enforced by the central arity table */` |
|        - |  422 | `	/* php binds $var by reference at the call boundary: a literal/constant (no` |
|        - |  423 | `	 * caller slot, nIdx == SXU32_HIGH) is a catchable Error, raised BEFORE the` |
|        - |  424 | `	 * $type validation — the same signal + wording array_pop() & co use. */` |
|       44 |  425 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|        3 |  426 | `		return PH7_VmThrowException(pCtx,"Error",` |
|        - |  427 | `			"settype(): Argument #1 ($var) could not be passed by reference");` |
|        - |  428 | `	}` |
|       41 |  429 | `	zType = ph7_value_to_string(apArg[1],&nLen);` |
|        - |  430 | `	/* Validate the target type up-front (php checks $type before touching $var):` |
|        - |  431 | `	 * "resource" is a distinct ValueError, any other unknown name is the generic` |
|        - |  432 | `	 * invalid-type ValueError. */` |
|       41 |  433 | `	if( nLen == 8 && SyStrnicmp(zType,"resource",8) == 0 ){` |
|        3 |  434 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|        - |  435 | `			"Cannot convert to resource type");` |
|        - |  436 | `	}` |
|       40 |  437 | `	if( !(  (nLen == 3 && SyStrnicmp(zType,"int",3) == 0)` |
|       35 |  438 | `	     \|\| (nLen == 7 && SyStrnicmp(zType,"integer",7) == 0)` |
|       51 |  439 | `	     \|\| (nLen == 5 && SyStrnicmp(zType,"float",5) == 0)` |
|       50 |  440 | `	     \|\| (nLen == 6 && SyStrnicmp(zType,"double",6) == 0)` |
|       49 |  441 | `	     \|\| (nLen == 6 && SyStrnicmp(zType,"string",6) == 0)` |
|       34 |  442 | `	     \|\| (nLen == 4 && SyStrnicmp(zType,"bool",4) == 0)` |
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
|       51 |  453 | `	pNew = ph7_context_new_scalar(pCtx);` |
|       51 |  454 | `	if( pNew == 0 ){` |
|      ! 0 |  455 | `		return PH7_ContextMemoryError(pCtx);` |
|        - |  456 | `	}` |
|       51 |  457 | `	PH7_MemObjStore(apArg[0],pNew);` |
|       48 |  458 | `	if( (nLen == 3 && SyStrnicmp(zType,"int",3) == 0)` |
|       50 |  459 | `	 \|\| (nLen == 7 && SyStrnicmp(zType,"integer",7) == 0) ){` |
|       10 |  460 | `		PH7_MemObjToInteger(pNew);` |
|       10 |  461 | `		MemObjSetType(pNew,MEMOBJ_INT);` |
|       52 |  462 | `	}else if( (nLen == 5 && SyStrnicmp(zType,"float",5) == 0)` |
|       48 |  463 | `	       \|\| (nLen == 6 && SyStrnicmp(zType,"double",6) == 0) ){` |
|        5 |  464 | `		PH7_MemObjToReal(pNew);` |
|        5 |  465 | `		MemObjSetType(pNew,MEMOBJ_REAL);` |
|       55 |  466 | `	}else if( nLen == 6 && SyStrnicmp(zType,"string",6) == 0 ){` |
|        - |  467 | `		/* php emits "Array to string conversion" for settype($arr,'string'), and` |
|        - |  468 | `		 * throws "Object of class X could not be converted to string" for an` |
|        - |  469 | `		 * object with no __toString() (or propagates one that threw). php's` |
|        - |  470 | `		 * convert_to_string() has already blanked the zval by then, so a CAUGHT` |
|        - |  471 | `		 * settype() leaves $var === "" — store that, then propagate instead of` |
|        - |  472 | `		 * answering true. */` |
|       33 |  473 | `		sxi32 rcSv = PH7_MemObjToStringUV(pNew);` |
|       33 |  474 | `		if( rcSv != SXRET_OK ){` |
|       16 |  475 | `			PH7_MemObjRelease(pNew);` |
|       16 |  476 | `			MemObjSetType(pNew,MEMOBJ_STRING);` |
|       16 |  477 | `			PH7_VmStoreArgByRef(pCtx->pVm,apArg[0],pNew);` |
|       16 |  478 | `			pCtx->nThrowRc = rcSv;` |
|       16 |  479 | `			return rcSv;` |
|        - |  480 | `		}` |
|       22 |  481 | `	}else if( (nLen == 4 && SyStrnicmp(zType,"bool",4) == 0)` |
|       12 |  482 | `	       \|\| (nLen == 7 && SyStrnicmp(zType,"boolean",7) == 0) ){` |
|        5 |  483 | `		PH7_MemObjToBool(pNew);` |
|       11 |  484 | `	}else if( nLen == 5 && SyStrnicmp(zType,"array",5) == 0 ){` |
|        5 |  485 | `		PH7_MemObjToHashmap(pNew);` |
|        7 |  486 | `	}else if( nLen == 6 && SyStrnicmp(zType,"object",6) == 0 ){` |
|        3 |  487 | `		PH7_MemObjToObject(pNew);` |
|        2 |  488 | `	}else{` |
|        - |  489 | `		/* "null" — the only validated name left */` |
|        3 |  490 | `		PH7_MemObjToNull(pNew);` |
|        - |  491 | `	}` |
|       42 |  492 | `	PH7_VmStoreArgByRef(pCtx->pVm,apArg[0],pNew);` |
|       42 |  493 | `	ph7_result_bool(pCtx,1);` |
|       42 |  494 | `	return SXRET_OK;` |
|       34 |  495 | `}` |
|        - |  496 | `/*` |
|        - |  497 | ` * string get_resource_type(resource $handle)` |
|        - |  498 | ` *  This function gets the type of the given resource.` |
|        - |  499 | ` * Parameters` |
|        - |  500 | ` *  $handle` |
|        - |  501 | ` *  The evaluated resource handle.` |
|        - |  502 | ` * Return` |
|        - |  503 | ` *  If the given handle is a resource, this function will return a string` |
|        - |  504 | ` *  representing its type. If the type is not identified by this function` |
|        - |  505 | ` *  the return value will be the string Unknown.` |
|        - |  506 | ` *  This function will return FALSE and generate an error if handle` |
|        - |  507 | ` *  is not a resource.` |
|        - |  508 | ` */` |
|        8 |  509 | `PH7_PRIVATE int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  510 | `{` |
|        - |  511 | `	const char *zType;` |
|        9 |  512 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - |  513 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 |  514 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  515 | `		return PH7_OK;` |
|        - |  516 | `	}` |
|        - |  517 | `	/* PHP returns the resource TYPE name (e.g. "stream" for IO handles), not an id. */` |
|        9 |  518 | `	zType = PH7_VfsResourceType(apArg[0]->x.pOther);` |
|        9 |  519 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|        9 |  520 | `	return SXRET_OK;` |
|        5 |  521 | `}` |
|        - |  522 | `/*` |
|        - |  523 | ` * int get_resource_id(resource $resource)` |
|        - |  524 | ` *  Returns the integer id of a resource — the same number (int) casts to and` |
|        - |  525 | ` *  "Resource id #N" renders (php 8.0).` |
|        - |  526 | ` */` |
|        4 |  527 | `PH7_PRIVATE int vm_builtin_get_resource_id(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  528 | `{` |
|        6 |  529 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 |  530 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|        - |  531 | `			"get_resource_id(): Argument #1 ($resource) must be of type resource, %s given",` |
|      ! 0 |  532 | `			nArg < 1 ? "none" : ph7_type_name(apArg[0]));` |
|        - |  533 | `	}` |
|        6 |  534 | `	ph7_result_int64(pCtx,(ph7_int64)PH7_VmResourceId(pCtx->pVm,apArg[0]->x.pOther));` |
|        6 |  535 | `	return SXRET_OK;` |
|        4 |  536 | `}` |
|        - |  537 | `/*` |
|        - |  538 | ` * void var_dump(expression,....)` |
|        - |  539 | ` *   var_dump � Dumps information about a variable` |
|        - |  540 | ` * Parameters` |
|        - |  541 | ` *   One or more expression to dump.` |
|        - |  542 | ` * Returns` |
|        - |  543 | ` *  Nothing.` |
|        - |  544 | ` */` |
|     2320 |  545 | `PH7_PRIVATE int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  546 | `{` |
|        - |  547 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - |  548 | `	int i;` |
|     2325 |  549 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - |  550 | `	/* Dump one or more expressions */` |
|     5041 |  551 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     2721 |  552 | `		ph7_value *pObj = apArg[i];` |
|        - |  553 | `		/* Reset the working buffer */` |
|     2721 |  554 | `		SyBlobReset(&sDump);` |
|        - |  555 | `		/* Dump the given expression */` |
|     2721 |  556 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - |  557 | `		/* Output */` |
|     2721 |  558 | `		if( SyBlobLength(&sDump) > 0 ){` |
|     2721 |  559 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|     1358 |  560 | `		}` |
|     1363 |  561 | `	}` |
|        - |  562 | `	/* Release the working buffer */` |
|     2325 |  563 | `	SyBlobRelease(&sDump);` |
|     2325 |  564 | `	return SXRET_OK;` |
|        5 |  565 | `}` |
|        - |  566 | `/*` |
|        - |  567 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - |  568 | ` *   print-r - Prints human-readable information about a variable` |
|        - |  569 | ` * Parameters` |
|        - |  570 | ` *   expression: Expression to dump` |
|        - |  571 | ` *   return : If you would like to capture the output of print_r() use` |
|        - |  572 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - |  573 | ` *            print_r() will return the information rather than print it.` |
|        - |  574 | ` * Return` |
|        - |  575 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - |  576 | ` *  Otherwise, the return value is TRUE.` |
|        - |  577 | ` */` |
|      106 |  578 | `PH7_PRIVATE int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        4 |  579 | `{` |
|      110 |  580 | `	int ret_string = 0;` |
|        - |  581 | `	SyBlob sDump;` |
|      110 |  582 | `	if( nArg < 1 ){` |
|        - |  583 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  584 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  585 | `		return SXRET_OK;` |
|        - |  586 | `	}` |
|      110 |  587 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|      110 |  588 | `	if ( nArg > 1 ){` |
|        - |  589 | `		/* Where to redirect output */` |
|       15 |  590 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        6 |  591 | `	}` |
|        - |  592 | `	/* Generate dump */` |
|      110 |  593 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|      110 |  594 | `	if( !ret_string ){` |
|        - |  595 | `		/* Output dump */` |
|       98 |  596 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  597 | `		/* Return true */` |
|       98 |  598 | `		ph7_result_bool(pCtx,1);` |
|       51 |  599 | `	}else{` |
|        - |  600 | `		/* Generated dump as return value */` |
|       15 |  601 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  602 | `	}` |
|        - |  603 | `	/* Release the working buffer */` |
|      110 |  604 | `	SyBlobRelease(&sDump);` |
|      110 |  605 | `	return SXRET_OK;` |
|       57 |  606 | `}` |
|        - |  607 | `/*` |
|        - |  608 | ` * var_export() — PHP-exact evaluable representation of a value.` |
|        - |  609 | ` *` |
|        - |  610 | ` * Distinct from print_r/var_dump (which share PH7_MemObjDump): var_export emits` |
|        - |  611 | ` * parseable PHP — 'single-quoted' strings, true/false/NULL, array (\n  k => v,\n),` |
|        - |  612 | ` * and \Class::__set_state(array(...)) for objects. PHP's indentation is quirky` |
|        - |  613 | ` * (2-space array entries; 3-space object property lines; a composite value always` |
|        - |  614 | ` * opens at containerIndent+2) but deterministic; this matches it byte-for-byte.` |
|        - |  615 | ` */` |
|        - |  616 | `/* Instance flag (distinct from oo.c's CLASS_INSTANCE_DESTROYED 0x001) marking an` |
|        - |  617 | ` * object currently on the var_export recursion stack, for cycle detection. */` |
|        - |  618 | `#define VM_INSTANCE_DUMPING 0x002` |
|        - |  619 | `typedef struct VmExportCtx VmExportCtx;` |
|        - |  620 | `struct VmExportCtx` |
|        - |  621 | `{` |
|        - |  622 | `	SyBlob *pOut;` |
|        - |  623 | `	int nIndent;  /* indentation of the container emitting this entry */` |
|        - |  624 | `	int depth;    /* recursion guard */` |
|        - |  625 | `};` |
|        - |  626 | `static void VmExportValue(SyBlob *pOut, ph7_value *pVal, int nIndent, int depth);` |
|        - |  627 | `/* Append nIndent spaces. */` |
|      874 |  628 | `static void VmExportIndent(SyBlob *pOut, int nIndent)` |
|        5 |  629 | `{` |
|        - |  630 | `	int i;` |
|     2459 |  631 | `	for( i = 0; i < nIndent; i++ ){ SyBlobAppend(pOut," ",1); }` |
|      879 |  632 | `}` |
|        - |  633 | `/* Append a single-quoted PHP string literal (escapes ' and \, batching safe runs` |
|        - |  634 | ` * in one append). A NUL byte can't live in a single-quoted literal, so PHP splits` |
|        - |  635 | ` * it out as ' . "\0" . ' — match that. */` |
|     1164 |  636 | `static void VmExportQuoted(SyBlob *pOut, const char *z, int n)` |
|        5 |  637 | `{` |
|     1169 |  638 | `	int i, run = 0;` |
|     1169 |  639 | `	SyBlobAppend(pOut,"'",1);` |
|     7133 |  640 | `	for( i = 0; i < n; i++ ){` |
|     5969 |  641 | `		char c = z[i];` |
|     5969 |  642 | `		if( c != '\0' && c != '\'' && c != '\\' ){ continue; } /* extend the safe run */` |
|       31 |  643 | `		if( i > run ){ SyBlobAppend(pOut,&z[run],(sxu32)(i-run)); }` |
|       31 |  644 | `		if( c == '\0' ){ SyBlobAppend(pOut,"' . \"\\0\" . '",12); }` |
|       29 |  645 | `		else { SyBlobAppend(pOut,"\\",1); SyBlobAppend(pOut,&z[i],1); }` |
|       31 |  646 | `		run = i+1;` |
|       16 |  647 | `	}` |
|     1169 |  648 | `	if( n > run ){ SyBlobAppend(pOut,&z[run],(sxu32)(n-run)); }` |
|     1169 |  649 | `	SyBlobAppend(pOut,"'",1);` |
|     1169 |  650 | `}` |
|        - |  651 | `/* True if the array/object is already on the var_export recursion stack. */` |
|      554 |  652 | `static int VmExportIsCycle(ph7_value *pVal)` |
|        5 |  653 | `{` |
|      559 |  654 | `	if( ph7_value_is_array(pVal) ){` |
|       55 |  655 | `		return (((ph7_hashmap *)pVal->x.pOther)->iFlags & HASHMAP_DUMPING) != 0;` |
|        - |  656 | `	}` |
|      509 |  657 | `	if( ph7_value_is_object(pVal) ){` |
|        8 |  658 | `		return (((ph7_class_instance *)pVal->x.pOther)->iFlags & VM_INSTANCE_DUMPING) != 0;` |
|        - |  659 | `	}` |
|      503 |  660 | `	return 0;` |
|      282 |  661 | `}` |
|        - |  662 | `/* Emit " => " then the value: a scalar inline, a composite on its own line at` |
|        - |  663 | ` * containerIndent+2. A circular reference renders inline as NULL (like PHP). */` |
|      554 |  664 | `static void VmExportEntryValue(SyBlob *pOut, ph7_value *pVal, int nContainerIndent, int depth)` |
|        5 |  665 | `{` |
|      559 |  666 | `	SyBlobAppend(pOut," => ",4);` |
|      559 |  667 | `	if( VmExportIsCycle(pVal) ){` |
|        5 |  668 | `		SyBlobAppend(pOut,"NULL",4);` |
|      557 |  669 | `	}else if( ph7_value_is_array(pVal) \|\| ph7_value_is_object(pVal) ){` |
|       57 |  670 | `		SyBlobAppend(pOut,"\n",1);` |
|       57 |  671 | `		VmExportIndent(pOut,nContainerIndent+2);` |
|       57 |  672 | `		VmExportValue(pOut,pVal,nContainerIndent+2,depth+1);` |
|       31 |  673 | `	}else{` |
|      503 |  674 | `		VmExportValue(pOut,pVal,nContainerIndent+2,depth+1);` |
|        - |  675 | `	}` |
|      559 |  676 | `	SyBlobAppend(pOut,",\n",2);` |
|      559 |  677 | `}` |
|        - |  678 | `/* Array walker: "<indent+2>key => value,\n" for each entry. */` |
|      510 |  679 | `static int VmExportArrayWalk(ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|        5 |  680 | `{` |
|      515 |  681 | `	VmExportCtx *pC = (VmExportCtx *)pUserData;` |
|      515 |  682 | `	VmExportIndent(pC->pOut,pC->nIndent+2);` |
|      515 |  683 | `	if( ph7_value_is_string(pKey) ){` |
|        - |  684 | `		int n;` |
|      129 |  685 | `		const char *z = ph7_value_to_string(pKey,&n);` |
|      129 |  686 | `		VmExportQuoted(pC->pOut,z,n);` |
|       67 |  687 | `	}else{` |
|      391 |  688 | `		SyBlobFormat(pC->pOut,"%qd",ph7_value_to_int64(pKey));` |
|        - |  689 | `	}` |
|      515 |  690 | `	VmExportEntryValue(pC->pOut,pValue,pC->nIndent,pC->depth);` |
|      515 |  691 | `	return PH7_OK;` |
|        5 |  692 | `}` |
|     3540 |  693 | `static void VmExportValue(SyBlob *pOut, ph7_value *pVal, int nIndent, int depth)` |
|        5 |  694 | `{` |
|     3545 |  695 | `	if( depth > 4096 ){ return; } /* backstop for pathological finite nesting */` |
|     3545 |  696 | `	if( ph7_value_is_null(pVal) ){` |
|      117 |  697 | `		SyBlobAppend(pOut,"NULL",4);` |
|     3489 |  698 | `	}else if( ph7_value_is_bool(pVal) ){` |
|     1027 |  699 | `		if( ph7_value_to_bool(pVal) ){ SyBlobAppend(pOut,"true",4); }` |
|      537 |  700 | `		else { SyBlobAppend(pOut,"false",5); }` |
|     2922 |  701 | `	}else if( ph7_value_is_float(pVal) ){` |
|        - |  702 | `		/* float before int: ph7_value_is_int is lenient (true for integer reals). */` |
|      372 |  703 | `		sxu32 before = SyBlobLength(pOut), i, after;` |
|        - |  704 | `		const char *z;` |
|      372 |  705 | `		int plain = 1;` |
|      372 |  706 | `		PH7_AppendShortestReal(pOut,ph7_value_to_double(pVal));` |
|      372 |  707 | `		z = (const char *)SyBlobData(pOut);` |
|      372 |  708 | `		after = SyBlobLength(pOut);` |
|      928 |  709 | `		for( i = before; i < after; i++ ){` |
|      776 |  710 | `			if( !((z[i]>='0'&&z[i]<='9')\|\|z[i]=='-') ){ plain = 0; break; }` |
|      282 |  711 | `		}` |
|      372 |  712 | `		if( plain ){ SyBlobAppend(pOut,".0",2); } /* integer-form floats render 1.0/100.0 */` |
|     2227 |  713 | `	}else if( ph7_value_is_int(pVal) ){` |
|      777 |  714 | `		sxi64 iVal = ph7_value_to_int64(pVal);` |
|      777 |  715 | `		if( iVal == SMALLEST_INT64 ){` |
|        - |  716 | `			/* php renders LONG_MIN as an expression: the positive literal` |
|        - |  717 | `			 * 9223372036854775808 would not be representable when eval'd. */` |
|        9 |  718 | `			SyBlobAppend(pOut,"-9223372036854775807-1",sizeof("-9223372036854775807-1")-1);` |
|        5 |  719 | `		}else{` |
|      769 |  720 | `			SyBlobFormat(pOut,"%qd",iVal);` |
|        5 |  721 | `		}` |
|     1657 |  722 | `	}else if( ph7_value_is_string(pVal) ){` |
|        - |  723 | `		int n;` |
|     1001 |  724 | `		const char *z = ph7_value_to_string(pVal,&n);` |
|     1001 |  725 | `		VmExportQuoted(pOut,z,n);` |
|      773 |  726 | `	}else if( ph7_value_is_array(pVal) ){` |
|      247 |  727 | `		ph7_hashmap *pMap = (ph7_hashmap *)pVal->x.pOther;` |
|      247 |  728 | `		if( pMap->iFlags & HASHMAP_DUMPING ){` |
|      ! 0 |  729 | `			SyBlobAppend(pOut,"NULL",4); /* circular reference -> NULL, like PHP */` |
|      ! 0 |  730 | `		}else{` |
|        - |  731 | `			VmExportCtx ctx;` |
|      247 |  732 | `			pMap->iFlags \|= HASHMAP_DUMPING;` |
|      247 |  733 | `			SyBlobAppend(pOut,"array (\n",8);` |
|      247 |  734 | `			ctx.pOut = pOut; ctx.nIndent = nIndent; ctx.depth = depth;` |
|      247 |  735 | `			ph7_array_walk(pVal,VmExportArrayWalk,&ctx);` |
|      247 |  736 | `			VmExportIndent(pOut,nIndent);` |
|      247 |  737 | `			SyBlobAppend(pOut,")",1);` |
|      247 |  738 | `			pMap->iFlags &= ~HASHMAP_DUMPING;` |
|        5 |  739 | `		}` |
|      152 |  740 | `	}else if( ph7_value_is_object(pVal) ){` |
|       31 |  741 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|       31 |  742 | `		if( pThis->pClass->iFlags & PH7_CLASS_ENUM ){` |
|        - |  743 | ``			/* php 8.1: an enum case exports as `\S::A` */`` |
|        3 |  744 | `			ph7_value *pName = PH7_EnumCaseNameValue(pThis);` |
|        3 |  745 | `			SyBlobAppend(pOut,"\\",1);` |
|        3 |  746 | `			SyBlobAppend(pOut,pThis->pClass->sName.zString,pThis->pClass->sName.nByte);` |
|        3 |  747 | `			SyBlobAppend(pOut,"::",2);` |
|        3 |  748 | `			if( pName && SyBlobLength(&pName->sBlob) > 0 ){` |
|        3 |  749 | `				SyBlobAppend(pOut,SyBlobData(&pName->sBlob),SyBlobLength(&pName->sBlob));` |
|        2 |  750 | `			}` |
|       30 |  751 | `		}else if( pThis->iFlags & VM_INSTANCE_DUMPING ){` |
|      ! 0 |  752 | `			SyBlobAppend(pOut,"NULL",4); /* circular reference -> NULL, like PHP */` |
|      ! 0 |  753 | `		}else{` |
|       29 |  754 | `			SyString *pClassName = &pThis->pClass->sName;` |
|        - |  755 | `			SyHashEntry *pEntry;` |
|        - |  756 | `			SySet sNames;` |
|        - |  757 | `			SyString *aName;` |
|        - |  758 | `			sxu32 iName,nName;` |
|       29 |  759 | `			pThis->iFlags \|= VM_INSTANCE_DUMPING;` |
|       29 |  760 | `			SyBlobAppend(pOut,"\\",1);` |
|       29 |  761 | `			SyBlobAppend(pOut,pClassName->zString,pClassName->nByte);` |
|       29 |  762 | `			SyBlobAppend(pOut,"::__set_state(array(\n",21);` |
|        - |  763 | `			/* SNAPSHOT the attribute names first: a PHP 8.4 get hook dispatched` |
|        - |  764 | `			 * mid-walk may re-enter an hAttr walk on this instance (the hash has` |
|        - |  765 | `			 * a single embedded loop cursor) or unset()/create properties; names` |
|        - |  766 | `			 * point into class-owned attr storage and each is re-looked-up. */` |
|       29 |  767 | `			SySetInit(&sNames,&pThis->pVm->sAllocator,sizeof(SyString));` |
|       29 |  768 | `			SyHashResetLoopCursor(&pThis->hAttr);` |
|       75 |  769 | `			while( (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       48 |  770 | `				VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|       48 |  771 | `				if( pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){ continue; }` |
|       46 |  772 | `				if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_VIRTUAL))` |
|       25 |  773 | `				 == PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|        3 |  774 | `					continue; /* virtual set-only property: no value to export (php) */` |
|        - |  775 | `				}` |
|       46 |  776 | `				SySetPut(&sNames,(const void *)&pVmAttr->pAttr->sName);` |
|        2 |  777 | `			}` |
|       29 |  778 | `			aName = (SyString *)SySetBasePtr(&sNames);` |
|       29 |  779 | `			nName = SySetUsed(&sNames);` |
|       73 |  780 | `			for( iName = 0 ; iName < nName ; ++iName ){` |
|       46 |  781 | `				SyString *pAName = &aName[iName];` |
|        - |  782 | `				VmClassAttr *pVmAttr;` |
|        - |  783 | `				ph7_value *pAttrVal;` |
|       46 |  784 | `				pEntry = SyHashGet(&pThis->hAttr,(const void *)pAName->zString,pAName->nByte);` |
|       46 |  785 | `				if( pEntry == 0 ){ continue; } /* unset by an earlier hook */` |
|       46 |  786 | `				pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|       46 |  787 | `				VmExportIndent(pOut,nIndent+3); /* object property lines sit one deeper than arrays */` |
|       46 |  788 | `				VmExportQuoted(pOut,pAName->zString,(int)pAName->nByte);` |
|        - |  789 | `				/* PHP 8.4 property hooks: var_export() reads through the get hook` |
|        - |  790 | `				 * (every visibility — php exports private hooked values too). A` |
|        - |  791 | `				 * throwing hook parks on the boundary rail; the helper's boundary` |
|        - |  792 | `				 * gate keeps LATER hooks from running (the raw values the tail of` |
|        - |  793 | `				 * the export falls back to are discarded when the throw routes). */` |
|        - |  794 | `				{` |
|        - |  795 | `					ph7_value sHookVal;` |
|        - |  796 | `					sxi32 rcHk;` |
|       46 |  797 | `					PH7_MemObjInit(pThis->pVm,&sHookVal);` |
|       46 |  798 | `					rcHk = PH7_VmHookGetAttrValue(pThis,pVmAttr,&sHookVal);` |
|       46 |  799 | `					if( rcHk == SXRET_OK ){` |
|       13 |  800 | `						VmExportEntryValue(pOut,&sHookVal,nIndent,depth);` |
|       13 |  801 | `						PH7_MemObjRelease(&sHookVal);` |
|       13 |  802 | `						continue;` |
|        - |  803 | `					}` |
|       34 |  804 | `					PH7_MemObjRelease(&sHookVal);` |
|       34 |  805 | `					if( rcHk != SXERR_NOTFOUND ){` |
|        - |  806 | `						/* the hook threw (parked on the boundary rail): NULL` |
|        - |  807 | `						 * placeholder keeps the output well-formed */` |
|      ! 0 |  808 | `						SyBlobAppend(pOut," => NULL,\n",10);` |
|      ! 0 |  809 | `						continue;` |
|        - |  810 | `					}` |
|        - |  811 | `				}` |
|       34 |  812 | `				pAttrVal = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       34 |  813 | `				if( pAttrVal ){ VmExportEntryValue(pOut,pAttrVal,nIndent,depth); }` |
|      ! 0 |  814 | `				else { SyBlobAppend(pOut," => NULL,\n",10); }` |
|       18 |  815 | `			}` |
|       29 |  816 | `			SySetRelease(&sNames);` |
|       29 |  817 | `			VmExportIndent(pOut,nIndent);` |
|       29 |  818 | `			SyBlobAppend(pOut,"))",2);` |
|       29 |  819 | `			pThis->iFlags &= ~VM_INSTANCE_DUMPING;` |
|        - |  820 | `		}` |
|       17 |  821 | `	}else{` |
|        - |  822 | `		/* resource / other -> PHP emits NULL (with a warning we omit) */` |
|      ! 0 |  823 | `		SyBlobAppend(pOut,"NULL",4);` |
|        - |  824 | `	}` |
|     1775 |  825 | `}` |
|        - |  826 | `/*` |
|        - |  827 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - |  828 | ` *  PHP-exact evaluable representation (see VmExportValue).` |
|        - |  829 | ` */` |
|     2990 |  830 | `PH7_PRIVATE int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  831 | `{` |
|     2995 |  832 | `	int ret_string = 0;` |
|        - |  833 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|     2995 |  834 | `	if( nArg < 1 ){` |
|        - |  835 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  836 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  837 | `		return SXRET_OK;` |
|        - |  838 | `	}` |
|     2995 |  839 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|     2995 |  840 | `	if ( nArg > 1 ){` |
|        - |  841 | `		/* Where to redirect output */` |
|     2549 |  842 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|     1272 |  843 | `	}` |
|        - |  844 | `	/* Generate the PHP-exact evaluable representation */` |
|     2995 |  845 | `	VmExportValue(&sDump,apArg[0],0,0);` |
|     2995 |  846 | `	if( !ret_string ){` |
|        - |  847 | `		/* Output dump */` |
|      451 |  848 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  849 | `		/* Return NULL */` |
|      451 |  850 | `		ph7_result_null(pCtx);` |
|      228 |  851 | `	}else{` |
|        - |  852 | `		/* Generated dump as return value */` |
|     2549 |  853 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  854 | `	}` |
|        - |  855 | `	/* Release the working buffer */` |
|     2995 |  856 | `	SyBlobRelease(&sDump);` |
|     2995 |  857 | `	return SXRET_OK;` |
|     1500 |  858 | `}` |
|        - |  859 |  |
