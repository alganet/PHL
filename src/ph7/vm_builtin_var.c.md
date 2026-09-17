# src/ph7/vm_builtin_var.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 388/428 lines (90.65%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits | Line | Source |
| ------: | ---: | :--- |
|       - |    1 | `/**` |
|       - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|       - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |    5 | ` */` |
|       - |    6 | `#include "ph7int.h"` |
|       - |    7 | `/*` |
|       - |    8 | ` * Section:` |
|       - |    9 | ` *    Variable-introspection builtins: isset, unset, get_defined_vars,` |
|       - |   10 | ` *    gettype, get_resource_type, var_dump, print_r and var_export.` |
|       - |   11 | ` *    Registration rows stay in vm.c's aVmFunc[].` |
|       - |   12 | ` * Status:` |
|       - |   13 | ` *    Stable.` |
|       - |   14 | ` */` |
|       - |   15 | `/*` |
|       - |   16 | ` * bool isset($var,...)` |
|       - |   17 | ` *  Finds out whether a variable is set.` |
|       - |   18 | ` * Parameters` |
|       - |   19 | ` *  One or more variable to check.` |
|       - |   20 | ` * Return` |
|       - |   21 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|       - |   22 | ` */` |
|  115774 |   23 | `PH7_PRIVATE int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |   24 | `{` |
|       - |   25 | `	ph7_value *pObj;` |
|  115779 |   26 | `	int res = 0;` |
|       - |   27 | `	int i;` |
|  115779 |   28 | `	if( nArg < 1 ){` |
|       - |   29 | `		/* Missing arguments,return false */` |
|     ! 0 |   30 | `		ph7_result_bool(pCtx,res);` |
|     ! 0 |   31 | `		return SXRET_OK;` |
|       - |   32 | `	}` |
|       - |   33 | `	/* Iterate over available arguments */` |
|  148561 |   34 | `	for( i = 0 ; i < nArg ; ++i ){` |
|  115791 |   35 | `		pObj = apArg[i];` |
|  115791 |   36 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|       - |   37 | `			/* Skip the "expecting a variable" warning for MEMOBJ_BOOL —` |
|       - |   38 | `			 * synthesized by LOAD_IDX iP2=4 (ArrayAccess::offsetExists) and` |
|       - |   39 | `			 * by anyone passing a bool literal (rare, harmless). */` |
|   80131 |   40 | `			if( (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|       - |   41 | `				/* Not so fatal,Throw a warning */` |
|     ! 0 |   42 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|     ! 0 |   43 | `			}` |
|   40063 |   44 | `		}` |
|  115791 |   45 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|  115791 |   46 | `		if( !res ){` |
|       - |   47 | `			/* Variable not set,return FALSE */` |
|   83009 |   48 | `			ph7_result_bool(pCtx,0);` |
|   83009 |   49 | `			return SXRET_OK;` |
|       - |   50 | `		}` |
|   16396 |   51 | `	}` |
|       - |   52 | `	/* All given variable are set,return TRUE */` |
|   32775 |   53 | `	ph7_result_bool(pCtx,1);` |
|   32775 |   54 | `	return SXRET_OK;` |
|   57892 |   55 | `}` |
|       - |   56 | `/*` |
|       - |   57 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|       - |   58 | ` * frame,the reference table and discard it's contents.` |
|       - |   59 | ` * This function never fail and always return SXRET_OK.` |
|       - |   60 | ` */` |
|       - |   61 | `/*` |
|       - |   62 | ` * unset($name) for a SIMPLE variable: drop exactly one NAME binding.` |
|       - |   63 | ` *` |
|       - |   64 | ` * PH7 routed every unset() through PH7_VmUnsetMemObj(), which releases the shared memory` |
|       - |   65 | ` * object and then has VmRefObjUnlink() delete EVERY name bound to that slot and unlink` |
|       - |   66 | ` * EVERY array node pointing at it. For an aliased variable that is data loss, not an` |
|       - |   67 | `` * unset: `$b = &$a; unset($b);` destroyed $a, `$r = &$arr[$k]; unset($r);` deleted the`` |
|       - |   68 | `` * array element, and `function f(&$p){ unset($p); }` wiped out the caller's variable.`` |
|       - |   69 | ` * php removes the NAME and nothing else; the value survives as long as anything still` |
|       - |   70 | ` * refers to it.` |
|       - |   71 | ` *` |
|       - |   72 | ` * So: unlink this one name, forget it in the slot's reference record, and release the` |
|       - |   73 | ` * slot only once no name and no array entry still holds it.` |
|       - |   74 | ` */` |
|    6738 |   75 | `PH7_PRIVATE sxi32 VmUnsetVarByName(ph7_vm *pVm,VmFrame *pFrame,const char *zName,sxu32 nByte)` |
|       5 |   76 | `{` |
|       - |   77 | `	SyHashEntry *pEntry;` |
|       - |   78 | `	VmRefObj *pRef;` |
|       - |   79 | `	sxu32 nIdx;` |
|       - |   80 | `	/* php 8.1 forbids unset($GLOBALS) outright. Checked by NAME: the superglobal is not an` |
|       - |   81 | `	 * ordinary hVar binding, so the slot-index test below never sees it. */` |
|    6743 |   82 | `	if( nByte == sizeof("GLOBALS")-1 && SyMemcmp(zName,"GLOBALS",nByte) == 0 ){` |
|       3 |   83 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|       - |   84 | `			"$GLOBALS can only be modified using the $GLOBALS[$name] = $value syntax");` |
|       3 |   85 | `		pVm->iExitStatus = 255;` |
|       3 |   86 | `		pVm->bHaltRequested = 1;` |
|       3 |   87 | `		return PH7_ABORT;` |
|       - |   88 | `	}` |
|    6741 |   89 | `	pEntry = SyHashGet(&pFrame->hVar,(const void *)zName,nByte);` |
|    6741 |   90 | `	if( pEntry == 0 ){` |
|       - |   91 | `		/* No such variable: unset() is a no-op on an undefined name, as in php */` |
|     935 |   92 | `		return SXRET_OK;` |
|       - |   93 | `	}` |
|    5809 |   94 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|    5809 |   95 | `	if( nIdx == pVm->nGlobalIdx ){` |
|     ! 0 |   96 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|       - |   97 | `			"$GLOBALS can only be modified using the $GLOBALS[$name] = $value syntax");` |
|     ! 0 |   98 | `		pVm->iExitStatus = 255;` |
|     ! 0 |   99 | `		pVm->bHaltRequested = 1;` |
|     ! 0 |  100 | `		return PH7_ABORT;` |
|       - |  101 | `	}` |
|    5809 |  102 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|       - |  103 | `	/*` |
|       - |  104 | `	 * A GLOBAL variable is ALSO an entry of the $GLOBALS array, and that node points at the` |
|       - |  105 | `	 * same slot. php's unset($x) in global scope drops $GLOBALS['x'] too, so unlink the node` |
|       - |  106 | `	 * — otherwise it stays a live holder and the value is never released ($o = new D;` |
|       - |  107 | `	 * unset($o); stopped running the destructor). Clear the reference table's row for the` |
|       - |  108 | `	 * node BEFORE unlinking it: unlinking frees the node, and the holder count below would` |
|       - |  109 | `	 * otherwise dereference freed memory.` |
|       - |  110 | `	 */` |
|    5809 |  111 | `	if( pFrame->pParent == 0 ){` |
|    5807 |  112 | `		ph7_value *pGlobals = (ph7_value *)SySetAt(&pVm->aMemObj,pVm->nGlobalIdx);` |
|    5807 |  113 | `		if( pGlobals && (pGlobals->iFlags & MEMOBJ_HASHMAP) ){` |
|    5807 |  114 | `			ph7_hashmap_node *pNode = 0;` |
|       - |  115 | `			ph7_value sKey;` |
|       - |  116 | `			SyString sName;` |
|    5807 |  117 | `			PH7_MemObjInit(&(*pVm),&sKey);` |
|    5807 |  118 | `			SyStringInitFromBuf(&sName,zName,nByte);` |
|    5807 |  119 | `			PH7_MemObjInitFromString(&(*pVm),&sKey,&sName);` |
|    5804 |  120 | `			if( SXRET_OK == PH7_HashmapLookup((ph7_hashmap *)pGlobals->x.pOther,&sKey,&pNode)` |
|    5807 |  121 | `			 && pNode && pNode->nValIdx == nIdx ){` |
|    5805 |  122 | `				if( pRef ){` |
|    5805 |  123 | `					ph7_hashmap_node **apN = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|       - |  124 | `					sxu32 k;` |
|   11705 |  125 | `					for( k = 0 ; k < SySetUsed(&pRef->aArrEntries) ; ++k ){` |
|    5903 |  126 | `						if( apN[k] == pNode ){` |
|    5805 |  127 | `							apN[k] = 0;` |
|    2901 |  128 | `						}` |
|    2953 |  129 | `					}` |
|    2901 |  130 | `				}` |
|    5805 |  131 | `				PH7_HashmapUnlinkNode(pNode,FALSE);` |
|    2901 |  132 | `			}` |
|    5807 |  133 | `			PH7_MemObjRelease(&sKey);` |
|    2902 |  134 | `		}` |
|    2902 |  135 | `	}` |
|       - |  136 |  |
|    5809 |  137 | `	if( pRef == 0 ){` |
|       - |  138 | `		/* Unaliased variable: nobody else holds the slot, so the old path is right */` |
|     ! 0 |  139 | `		SyHashDeleteEntry2(pEntry);` |
|     ! 0 |  140 | `		PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|       - |  141 | `		/* The slot is back in the free pool; drop its stale local-teardown entry so a` |
|       - |  142 | `		 * later reuse of the index (e.g. an object property) is not double-freed when` |
|       - |  143 | `		 * this frame exits. */` |
|     ! 0 |  144 | `		VmDropFrameLocalSlot(&(*pVm),nIdx);` |
|     ! 0 |  145 | `		return SXRET_OK;` |
|       - |  146 | `	}` |
|       - |  147 | `	{` |
|    5809 |  148 | `		SyHashEntry **apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|    5809 |  149 | `		ph7_hashmap_node **apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|    5809 |  150 | `		sxu32 n, nLive = 0;` |
|       - |  151 | `		/* Forget THIS name in the slot's reference record (leave the others alone) */` |
|   11711 |  152 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; ++n ){` |
|    5905 |  153 | `			if( apEntry[n] == pEntry ){` |
|    5803 |  154 | `				apEntry[n] = 0;` |
|    2900 |  155 | `			}` |
|    2954 |  156 | `		}` |
|    5809 |  157 | `		SyHashDeleteEntry2(pEntry);` |
|       - |  158 | `		/* Anything else still holding the slot? */` |
|   11711 |  159 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; ++n ){` |
|    5905 |  160 | `			if( apEntry[n] ){` |
|      29 |  161 | `				nLive++;` |
|      14 |  162 | `			}` |
|    2954 |  163 | `		}` |
|   11711 |  164 | `		for( n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|       - |  165 | `			/* Only a node that STILL points at this slot is a holder. The reference table` |
|       - |  166 | `			 * keeps stale rows (a slot index is recycled through the free list, and the row` |
|       - |  167 | `			 * outlives the node that put it there), so an un-filtered count reports holders` |
|       - |  168 | `			 * that no longer exist and the value would never be released — the destructor` |
|       - |  169 | ``			 * of `$o = new D; unset($o);` stopped running. */`` |
|    5905 |  170 | `			if( apNode[n] && apNode[n]->nValIdx == nIdx ){` |
|      63 |  171 | `				nLive++;` |
|      31 |  172 | `			}` |
|    2954 |  173 | `		}` |
|    5809 |  174 | `		if( nLive < 1 ){` |
|       - |  175 | `			/* Last holder gone: now the value may go too */` |
|    5753 |  176 | `			PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|       - |  177 | `			/* Slot returned to the free pool: drop its stale local-teardown entry so a` |
|       - |  178 | `			 * later reuse of the index is not double-freed on frame exit (see` |
|       - |  179 | `			 * VmDropFrameLocalSlot). */` |
|    5753 |  180 | `			VmDropFrameLocalSlot(&(*pVm),nIdx);` |
|    2875 |  181 | `		}` |
|       - |  182 | `	}` |
|    5809 |  183 | `	return SXRET_OK;` |
|    3374 |  184 | `}` |
|       - |  185 | `/*` |
|       - |  186 | ` * Is this memory slot aliased — i.e. does anything other than its owner refer to it?` |
|       - |  187 | ` * var_dump marks such an array element with '&' ("&int(2)"). PH7 only flagged nodes that` |
|       - |  188 | `` * were FOREIGN (`array(&$x)`, where the node points at an outside slot) and so missed the`` |
|       - |  189 | `` * common case, a reference taken TO an element (`$r = &$a[1]`), where the array still owns`` |
|       - |  190 | ` * the value but is no longer its only holder.` |
|       - |  191 | ` */` |
|  709812 |  192 | `PH7_PRIVATE int PH7_VmSlotIsReferenced(ph7_vm *pVm,sxu32 nIdx)` |
|       5 |  193 | `{` |
|       - |  194 | `	VmRefObj *pRef;` |
|  709817 |  195 | `	sxu32 n, nLive = 0;` |
|       - |  196 | `	SyHashEntry **apEntry;` |
|  709817 |  197 | `	if( nIdx == SXU32_HIGH ){` |
|     ! 0 |  198 | `		return 0;` |
|       - |  199 | `	}` |
|  709817 |  200 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  709817 |  201 | `	if( pRef == 0 ){` |
|     ! 0 |  202 | `		return 0;` |
|       - |  203 | `	}` |
|  709817 |  204 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|  709821 |  205 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; ++n ){` |
|       5 |  206 | `		if( apEntry[n] ){` |
|       5 |  207 | `			nLive++;` |
|       2 |  208 | `		}` |
|       3 |  209 | `	}` |
|  709817 |  210 | `	return nLive > 0;` |
|  354911 |  211 | `}` |
| 4248503 |  212 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|       5 |  213 | `{` |
|       - |  214 | `	ph7_value *pObj;` |
|       - |  215 | `	VmRefObj *pRef;` |
| 4248508 |  216 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
| 4248508 |  217 | `	if( pObj ){` |
|       - |  218 | `		/* Release the object */` |
| 4248508 |  219 | `		PH7_MemObjRelease(pObj);` |
| 2124869 |  220 | `	}` |
|       - |  221 | `	/* Remove old reference links */` |
| 4248508 |  222 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
| 4248508 |  223 | `	if( pRef ){` |
| 4248508 |  224 | `		sxi32 iFlags = pRef->iFlags;` |
|       - |  225 | `		/* Unlink from the reference table */` |
| 4248508 |  226 | `		VmRefObjUnlink(&(*pVm),pRef);` |
| 4248508 |  227 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|       - |  228 | `			VmSlot sFree;` |
|       - |  229 | `			/* Restore to the free list */` |
| 4248490 |  230 | `			sFree.nIdx = nObjIdx;` |
| 4248490 |  231 | `			sFree.pUserData = 0;` |
| 4248490 |  232 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
| 2124860 |  233 | `		}` |
| 2124869 |  234 | `	}` |
| 4248508 |  235 | `	return SXRET_OK;` |
|       5 |  236 | `}` |
|       - |  237 | `/*` |
|       - |  238 | ` * void unset($var,...)` |
|       - |  239 | ` *   Unset one or more given variable.` |
|       - |  240 | ` * Parameters` |
|       - |  241 | ` *  One or more variable to unset.` |
|       - |  242 | ` * Return` |
|       - |  243 | ` *  Nothing.` |
|       - |  244 | ` */` |
|    1096 |  245 | `PH7_PRIVATE int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 |  246 | `{` |
|       - |  247 | `	ph7_value *pObj;` |
|       - |  248 | `	ph7_vm *pVm;` |
|       - |  249 | `	int i;` |
|       - |  250 | `	/* Point to the target VM */` |
|    1100 |  251 | `	pVm = pCtx->pVm;` |
|       - |  252 | `	/* Iterate and unset */` |
|    2196 |  253 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    1100 |  254 | `		pObj = apArg[i];` |
|    1100 |  255 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      52 |  256 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|       - |  257 | `				/* Throw an error */` |
|     ! 0 |  258 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|     ! 0 |  259 | `			}` |
|      28 |  260 | `		}else{` |
|    1052 |  261 | `			sxu32 nIdx = pObj->nIdx;` |
|    1052 |  262 | `			if( nIdx == pVm->nGlobalIdx ){` |
|       - |  263 | `				/* php 8.1: unset($GLOBALS) is forbidden — the same fatal as` |
|       - |  264 | `				 * re-assigning it (compile-time in php, raised here). */` |
|     ! 0 |  265 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|       - |  266 | `					"$GLOBALS can only be modified using the $GLOBALS[$name] = $value syntax");` |
|     ! 0 |  267 | `				pVm->iExitStatus = 255;` |
|     ! 0 |  268 | `				pVm->bHaltRequested = 1;` |
|     ! 0 |  269 | `				return PH7_ABORT;` |
|       - |  270 | `			}` |
|    1052 |  271 | `			PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|       - |  272 | `			/* Drop the stale local-teardown entry for the freed slot (see` |
|       - |  273 | `			 * VmDropFrameLocalSlot) so a later index reuse is not double-freed. */` |
|    1052 |  274 | `			VmDropFrameLocalSlot(&(*pVm),nIdx);` |
|       - |  275 | `		}` |
|     552 |  276 | `	}` |
|    1100 |  277 | `	return SXRET_OK;` |
|     552 |  278 | `}` |
|       - |  279 | `/*` |
|       - |  280 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|       - |  281 | ` */` |
|     424 |  282 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|       1 |  283 | `{` |
|     425 |  284 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|     425 |  285 | `	ph7_vm *pVm = pArray->pVm;` |
|       - |  286 | `	ph7_value *pObj;` |
|       - |  287 | `	sxu32 nIdx;` |
|       - |  288 | `	/* php excludes $this from get_defined_vars() (it is bound implicitly, not a` |
|       - |  289 | `	 * declared local). PHL keeps it in the frame's hVar, so skip it here. */` |
|     424 |  290 | `	if( pEntry->nKeyLen == sizeof("this")-1` |
|     261 |  291 | `	 && SyMemcmp(pEntry->pKey,"this",sizeof("this")-1) == 0 ){` |
|       3 |  292 | `		return SXRET_OK;` |
|       - |  293 | `	}` |
|       - |  294 | `	/* Extract the memory object */` |
|     423 |  295 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|     423 |  296 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     423 |  297 | `	if( pObj ){` |
|     423 |  298 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|     421 |  299 | `			if( pEntry->nKeyLen > 0 ){` |
|       - |  300 | `				SyString sName;` |
|       - |  301 | `				ph7_value sKey;` |
|       - |  302 | `				/* Perform the insertion (pObj may point into pVm->aMemObj; the` |
|       - |  303 | `				 * inserter snapshots the source before reserving, so the pool may` |
|       - |  304 | `				 * safely move underneath it — see HashmapInsertIntKey/BlobKey). */` |
|     421 |  305 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|     421 |  306 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|     421 |  307 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|     421 |  308 | `				PH7_MemObjRelease(&sKey);` |
|     210 |  309 | `			}` |
|     210 |  310 | `		}` |
|     211 |  311 | `	}` |
|     423 |  312 | `	return SXRET_OK;` |
|     213 |  313 | `}` |
|       - |  314 | `/*` |
|       - |  315 | ` * array get_defined_vars(void)` |
|       - |  316 | ` *  Returns an array of all defined variables.` |
|       - |  317 | ` * Parameter` |
|       - |  318 | ` *  None` |
|       - |  319 | ` * Return` |
|       - |  320 | ` *  An array with all the variables defined in the current scope.` |
|       - |  321 | ` */` |
|       6 |  322 | `PH7_PRIVATE int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  323 | `{` |
|       7 |  324 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - |  325 | `	ph7_value *pArray;` |
|       - |  326 | `	/* Create a new array */` |
|       7 |  327 | `	pArray = ph7_context_new_array(pCtx);` |
|       7 |  328 | ` 	if( pArray == 0 ){` |
|     ! 0 |  329 | `		SXUNUSED(nArg); /* cc warning */` |
|     ! 0 |  330 | `		SXUNUSED(apArg);` |
|       - |  331 | `		/* Return NULL */` |
|     ! 0 |  332 | `		ph7_result_null(pCtx);` |
|     ! 0 |  333 | `		return SXRET_OK;` |
|       - |  334 | `	}` |
|       - |  335 | `	/* Superglobals appear in get_defined_vars() ONLY at the global scope (php).` |
|       - |  336 | `	 * Inside a function the result is the local symbol table alone — a leak of` |
|       - |  337 | `	 * $argv/$_SERVER/... into every function's scope breaks callers that treat the` |
|       - |  338 | `	 * keys as real locals (e.g. PHPUnit's doubled-method template reflects each` |
|       - |  339 | `	 * get_defined_vars() name as a ReflectionParameter). */` |
|       7 |  340 | `	if( pVm->pFrame->pParent == 0 ){` |
|       3 |  341 | `		SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|       1 |  342 | `	}` |
|       - |  343 | `	/* Then variable defined in the current frame */` |
|       7 |  344 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|       - |  345 | `	/* Finally,return the created array */` |
|       7 |  346 | `	ph7_result_value(pCtx,pArray);` |
|       7 |  347 | `	return SXRET_OK;` |
|       4 |  348 | `}` |
|       - |  349 | `/*` |
|       - |  350 | ` * bool gettype($var)` |
|       - |  351 | ` *  Get the type of a variable` |
|       - |  352 | ` * Parameters` |
|       - |  353 | ` *   $var` |
|       - |  354 | ` *    The variable being type checked.` |
|       - |  355 | ` * Return` |
|       - |  356 | ` *   String representation of the given variable type.` |
|       - |  357 | ` */` |
|      78 |  358 | `PH7_PRIVATE int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  359 | `{` |
|       - |  360 | `	/* php's gettype() uses LONG type names ("integer"/"boolean"/"NULL"), distinct` |
|       - |  361 | `	 * from get_debug_type()'s short ones ("int"/"bool"/"null") — never route this` |
|       - |  362 | `	 * through PH7_MemObjTypeDump, which yields the short forms. */` |
|      81 |  363 | `	const char *zType = "unknown type";` |
|      81 |  364 | `	if( nArg > 0 ){` |
|      81 |  365 | `		ph7_value *pVal = apArg[0];` |
|      81 |  366 | `		if( pVal->iFlags & MEMOBJ_NULL ){` |
|       3 |  367 | `			zType = "NULL";` |
|      80 |  368 | `		}else if( pVal->iFlags & MEMOBJ_REAL ){` |
|       - |  369 | `			/* REAL wins over a cached MEMOBJ_INT: 1.0 is "double" (php). */` |
|       9 |  370 | `			zType = "double";` |
|      75 |  371 | `		}else if( pVal->iFlags & MEMOBJ_INT ){` |
|       3 |  372 | `			zType = "integer";` |
|      70 |  373 | `		}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|      41 |  374 | `			zType = "string";` |
|      48 |  375 | `		}else if( pVal->iFlags & MEMOBJ_BOOL ){` |
|       5 |  376 | `			zType = "boolean";` |
|      27 |  377 | `		}else if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|       3 |  378 | `			zType = "array";` |
|      24 |  379 | `		}else if( pVal->iFlags & MEMOBJ_OBJ ){` |
|       9 |  380 | `			zType = "object";` |
|      19 |  381 | `		}else if( pVal->iFlags & MEMOBJ_RES ){` |
|       - |  382 | `			/* php reports an fclose()'d handle as "resource (closed)" */` |
|      15 |  383 | `			zType = PH7_VfsResourceIsClosed(pVal->x.pOther) ? "resource (closed)" : "resource";` |
|       7 |  384 | `		}` |
|      39 |  385 | `	}` |
|       - |  386 | `	/* Return the variable type */` |
|      81 |  387 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|      81 |  388 | `	return SXRET_OK;` |
|       3 |  389 | `}` |
|       - |  390 | `/*` |
|       - |  391 | ` * string get_resource_type(resource $handle)` |
|       - |  392 | ` *  This function gets the type of the given resource.` |
|       - |  393 | ` * Parameters` |
|       - |  394 | ` *  $handle` |
|       - |  395 | ` *  The evaluated resource handle.` |
|       - |  396 | ` * Return` |
|       - |  397 | ` *  If the given handle is a resource, this function will return a string` |
|       - |  398 | ` *  representing its type. If the type is not identified by this function` |
|       - |  399 | ` *  the return value will be the string Unknown.` |
|       - |  400 | ` *  This function will return FALSE and generate an error if handle` |
|       - |  401 | ` *  is not a resource.` |
|       - |  402 | ` */` |
|       8 |  403 | `PH7_PRIVATE int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  404 | `{` |
|       - |  405 | `	const char *zType;` |
|       9 |  406 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|       - |  407 | `		/* Missing/Invalid arguments,return FALSE*/` |
|     ! 0 |  408 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  409 | `		return PH7_OK;` |
|       - |  410 | `	}` |
|       - |  411 | `	/* PHP returns the resource TYPE name (e.g. "stream" for IO handles), not an id. */` |
|       9 |  412 | `	zType = PH7_VfsResourceType(apArg[0]->x.pOther);` |
|       9 |  413 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       9 |  414 | `	return SXRET_OK;` |
|       5 |  415 | `}` |
|       - |  416 | `/*` |
|       - |  417 | ` * int get_resource_id(resource $resource)` |
|       - |  418 | ` *  Returns the integer id of a resource — the same number (int) casts to and` |
|       - |  419 | ` *  "Resource id #N" renders (php 8.0).` |
|       - |  420 | ` */` |
|       4 |  421 | `PH7_PRIVATE int vm_builtin_get_resource_id(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  422 | `{` |
|       6 |  423 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|     ! 0 |  424 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  425 | `			"get_resource_id(): Argument #1 ($resource) must be of type resource, %s given",` |
|     ! 0 |  426 | `			nArg < 1 ? "none" : ph7_type_name(apArg[0]));` |
|       - |  427 | `	}` |
|       6 |  428 | `	ph7_result_int64(pCtx,(ph7_int64)PH7_VmResourceId(pCtx->pVm,apArg[0]->x.pOther));` |
|       6 |  429 | `	return SXRET_OK;` |
|       4 |  430 | `}` |
|       - |  431 | `/*` |
|       - |  432 | ` * void var_dump(expression,....)` |
|       - |  433 | ` *   var_dump � Dumps information about a variable` |
|       - |  434 | ` * Parameters` |
|       - |  435 | ` *   One or more expression to dump.` |
|       - |  436 | ` * Returns` |
|       - |  437 | ` *  Nothing.` |
|       - |  438 | ` */` |
|     648 |  439 | `PH7_PRIVATE int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  440 | `{` |
|       - |  441 | `	SyBlob sDump; /* Generated dump is stored here */` |
|       - |  442 | `	int i;` |
|     653 |  443 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       - |  444 | `	/* Dump one or more expressions */` |
|    1383 |  445 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     735 |  446 | `		ph7_value *pObj = apArg[i];` |
|       - |  447 | `		/* Reset the working buffer */` |
|     735 |  448 | `		SyBlobReset(&sDump);` |
|       - |  449 | `		/* Dump the given expression */` |
|     735 |  450 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|       - |  451 | `		/* Output */` |
|     735 |  452 | `		if( SyBlobLength(&sDump) > 0 ){` |
|     735 |  453 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|     365 |  454 | `		}` |
|     370 |  455 | `	}` |
|       - |  456 | `	/* Release the working buffer */` |
|     653 |  457 | `	SyBlobRelease(&sDump);` |
|     653 |  458 | `	return SXRET_OK;` |
|       5 |  459 | `}` |
|       - |  460 | `/*` |
|       - |  461 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|       - |  462 | ` *   print-r - Prints human-readable information about a variable` |
|       - |  463 | ` * Parameters` |
|       - |  464 | ` *   expression: Expression to dump` |
|       - |  465 | ` *   return : If you would like to capture the output of print_r() use` |
|       - |  466 | ` *            the return parameter. When this parameter is set to TRUE` |
|       - |  467 | ` *            print_r() will return the information rather than print it.` |
|       - |  468 | ` * Return` |
|       - |  469 | ` *  When the return parameter is TRUE, this function will return a string.` |
|       - |  470 | ` *  Otherwise, the return value is TRUE.` |
|       - |  471 | ` */` |
|      84 |  472 | `PH7_PRIVATE int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  473 | `{` |
|      86 |  474 | `	int ret_string = 0;` |
|       - |  475 | `	SyBlob sDump;` |
|      86 |  476 | `	if( nArg < 1 ){` |
|       - |  477 | `		/* Nothing to output,return FALSE */` |
|     ! 0 |  478 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  479 | `		return SXRET_OK;` |
|       - |  480 | `	}` |
|      86 |  481 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|      86 |  482 | `	if ( nArg > 1 ){` |
|       - |  483 | `		/* Where to redirect output */` |
|       9 |  484 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|       4 |  485 | `	}` |
|       - |  486 | `	/* Generate dump */` |
|      86 |  487 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|      86 |  488 | `	if( !ret_string ){` |
|       - |  489 | `		/* Output dump */` |
|      78 |  490 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|       - |  491 | `		/* Return true */` |
|      78 |  492 | `		ph7_result_bool(pCtx,1);` |
|      40 |  493 | `	}else{` |
|       - |  494 | `		/* Generated dump as return value */` |
|       9 |  495 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|       - |  496 | `	}` |
|       - |  497 | `	/* Release the working buffer */` |
|      86 |  498 | `	SyBlobRelease(&sDump);` |
|      86 |  499 | `	return SXRET_OK;` |
|      44 |  500 | `}` |
|       - |  501 | `/*` |
|       - |  502 | ` * var_export() — PHP-exact evaluable representation of a value.` |
|       - |  503 | ` *` |
|       - |  504 | ` * Distinct from print_r/var_dump (which share PH7_MemObjDump): var_export emits` |
|       - |  505 | ` * parseable PHP — 'single-quoted' strings, true/false/NULL, array (\n  k => v,\n),` |
|       - |  506 | ` * and \Class::__set_state(array(...)) for objects. PHP's indentation is quirky` |
|       - |  507 | ` * (2-space array entries; 3-space object property lines; a composite value always` |
|       - |  508 | ` * opens at containerIndent+2) but deterministic; this matches it byte-for-byte.` |
|       - |  509 | ` */` |
|       - |  510 | `/* Instance flag (distinct from oo.c's CLASS_INSTANCE_DESTROYED 0x001) marking an` |
|       - |  511 | ` * object currently on the var_export recursion stack, for cycle detection. */` |
|       - |  512 | `#define VM_INSTANCE_DUMPING 0x002` |
|       - |  513 | `typedef struct VmExportCtx VmExportCtx;` |
|       - |  514 | `struct VmExportCtx` |
|       - |  515 | `{` |
|       - |  516 | `	SyBlob *pOut;` |
|       - |  517 | `	int nIndent;  /* indentation of the container emitting this entry */` |
|       - |  518 | `	int depth;    /* recursion guard */` |
|       - |  519 | `};` |
|       - |  520 | `static void VmExportValue(SyBlob *pOut, ph7_value *pVal, int nIndent, int depth);` |
|       - |  521 | `/* Append nIndent spaces. */` |
|     586 |  522 | `static void VmExportIndent(SyBlob *pOut, int nIndent)` |
|       4 |  523 | `{` |
|       - |  524 | `	int i;` |
|    1712 |  525 | `	for( i = 0; i < nIndent; i++ ){ SyBlobAppend(pOut," ",1); }` |
|     590 |  526 | `}` |
|       - |  527 | `/* Append a single-quoted PHP string literal (escapes ' and \, batching safe runs` |
|       - |  528 | ` * in one append). A NUL byte can't live in a single-quoted literal, so PHP splits` |
|       - |  529 | ` * it out as ' . "\0" . ' — match that. */` |
|     486 |  530 | `static void VmExportQuoted(SyBlob *pOut, const char *z, int n)` |
|       5 |  531 | `{` |
|     491 |  532 | `	int i, run = 0;` |
|     491 |  533 | `	SyBlobAppend(pOut,"'",1);` |
|    2525 |  534 | `	for( i = 0; i < n; i++ ){` |
|    2039 |  535 | `		char c = z[i];` |
|    2039 |  536 | `		if( c != '\0' && c != '\'' && c != '\\' ){ continue; } /* extend the safe run */` |
|       7 |  537 | `		if( i > run ){ SyBlobAppend(pOut,&z[run],(sxu32)(i-run)); }` |
|       7 |  538 | `		if( c == '\0' ){ SyBlobAppend(pOut,"' . \"\\0\" . '",12); }` |
|       5 |  539 | `		else { SyBlobAppend(pOut,"\\",1); SyBlobAppend(pOut,&z[i],1); }` |
|       7 |  540 | `		run = i+1;` |
|       4 |  541 | `	}` |
|     491 |  542 | `	if( n > run ){ SyBlobAppend(pOut,&z[run],(sxu32)(n-run)); }` |
|     491 |  543 | `	SyBlobAppend(pOut,"'",1);` |
|     491 |  544 | `}` |
|       - |  545 | `/* True if the array/object is already on the var_export recursion stack. */` |
|     382 |  546 | `static int VmExportIsCycle(ph7_value *pVal)` |
|       4 |  547 | `{` |
|     386 |  548 | `	if( ph7_value_is_array(pVal) ){` |
|      37 |  549 | `		return (((ph7_hashmap *)pVal->x.pOther)->iFlags & HASHMAP_DUMPING) != 0;` |
|       - |  550 | `	}` |
|     352 |  551 | `	if( ph7_value_is_object(pVal) ){` |
|       5 |  552 | `		return (((ph7_class_instance *)pVal->x.pOther)->iFlags & VM_INSTANCE_DUMPING) != 0;` |
|       - |  553 | `	}` |
|     348 |  554 | `	return 0;` |
|     195 |  555 | `}` |
|       - |  556 | `/* Emit " => " then the value: a scalar inline, a composite on its own line at` |
|       - |  557 | ` * containerIndent+2. A circular reference renders inline as NULL (like PHP). */` |
|     382 |  558 | `static void VmExportEntryValue(SyBlob *pOut, ph7_value *pVal, int nContainerIndent, int depth)` |
|       4 |  559 | `{` |
|     386 |  560 | `	SyBlobAppend(pOut," => ",4);` |
|     386 |  561 | `	if( VmExportIsCycle(pVal) ){` |
|       5 |  562 | `		SyBlobAppend(pOut,"NULL",4);` |
|     384 |  563 | `	}else if( ph7_value_is_array(pVal) \|\| ph7_value_is_object(pVal) ){` |
|      37 |  564 | `		SyBlobAppend(pOut,"\n",1);` |
|      37 |  565 | `		VmExportIndent(pOut,nContainerIndent+2);` |
|      37 |  566 | `		VmExportValue(pOut,pVal,nContainerIndent+2,depth+1);` |
|      20 |  567 | `	}else{` |
|     348 |  568 | `		VmExportValue(pOut,pVal,nContainerIndent+2,depth+1);` |
|       - |  569 | `	}` |
|     386 |  570 | `	SyBlobAppend(pOut,",\n",2);` |
|     386 |  571 | `}` |
|       - |  572 | `/* Array walker: "<indent+2>key => value,\n" for each entry. */` |
|     340 |  573 | `static int VmExportArrayWalk(ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|       4 |  574 | `{` |
|     344 |  575 | `	VmExportCtx *pC = (VmExportCtx *)pUserData;` |
|     344 |  576 | `	VmExportIndent(pC->pOut,pC->nIndent+2);` |
|     344 |  577 | `	if( ph7_value_is_string(pKey) ){` |
|       - |  578 | `		int n;` |
|      76 |  579 | `		const char *z = ph7_value_to_string(pKey,&n);` |
|      76 |  580 | `		VmExportQuoted(pC->pOut,z,n);` |
|      40 |  581 | `	}else{` |
|     272 |  582 | `		SyBlobFormat(pC->pOut,"%qd",ph7_value_to_int64(pKey));` |
|       - |  583 | `	}` |
|     344 |  584 | `	VmExportEntryValue(pC->pOut,pValue,pC->nIndent,pC->depth);` |
|     344 |  585 | `	return PH7_OK;` |
|       4 |  586 | `}` |
|    2090 |  587 | `static void VmExportValue(SyBlob *pOut, ph7_value *pVal, int nIndent, int depth)` |
|       5 |  588 | `{` |
|    2095 |  589 | `	if( depth > 4096 ){ return; } /* backstop for pathological finite nesting */` |
|    2095 |  590 | `	if( ph7_value_is_null(pVal) ){` |
|      93 |  591 | `		SyBlobAppend(pOut,"NULL",4);` |
|    2051 |  592 | `	}else if( ph7_value_is_bool(pVal) ){` |
|     619 |  593 | `		if( ph7_value_to_bool(pVal) ){ SyBlobAppend(pOut,"true",4); }` |
|     283 |  594 | `		else { SyBlobAppend(pOut,"false",5); }` |
|    1700 |  595 | `	}else if( ph7_value_is_float(pVal) ){` |
|       - |  596 | `		/* float before int: ph7_value_is_int is lenient (true for integer reals). */` |
|     328 |  597 | `		sxu32 before = SyBlobLength(pOut), i, after;` |
|       - |  598 | `		const char *z;` |
|     328 |  599 | `		int plain = 1;` |
|     328 |  600 | `		PH7_AppendShortestReal(pOut,ph7_value_to_double(pVal));` |
|     328 |  601 | `		z = (const char *)SyBlobData(pOut);` |
|     328 |  602 | `		after = SyBlobLength(pOut);` |
|     810 |  603 | `		for( i = before; i < after; i++ ){` |
|     666 |  604 | `			if( !((z[i]>='0'&&z[i]<='9')\|\|z[i]=='-') ){ plain = 0; break; }` |
|     245 |  605 | `		}` |
|     328 |  606 | `		if( plain ){ SyBlobAppend(pOut,".0",2); } /* integer-form floats render 1.0/100.0 */` |
|    1231 |  607 | `	}else if( ph7_value_is_int(pVal) ){` |
|     525 |  608 | `		sxi64 iVal = ph7_value_to_int64(pVal);` |
|     525 |  609 | `		if( iVal == SMALLEST_INT64 ){` |
|       - |  610 | `			/* php renders LONG_MIN as an expression: the positive literal` |
|       - |  611 | `			 * 9223372036854775808 would not be representable when eval'd. */` |
|       5 |  612 | `			SyBlobAppend(pOut,"-9223372036854775807-1",sizeof("-9223372036854775807-1")-1);` |
|       3 |  613 | `		}else{` |
|     521 |  614 | `			SyBlobFormat(pOut,"%qd",iVal);` |
|       5 |  615 | `		}` |
|     809 |  616 | `	}else if( ph7_value_is_string(pVal) ){` |
|       - |  617 | `		int n;` |
|     377 |  618 | `		const char *z = ph7_value_to_string(pVal,&n);` |
|     377 |  619 | `		VmExportQuoted(pOut,z,n);` |
|     362 |  620 | `	}else if( ph7_value_is_array(pVal) ){` |
|     152 |  621 | `		ph7_hashmap *pMap = (ph7_hashmap *)pVal->x.pOther;` |
|     152 |  622 | `		if( pMap->iFlags & HASHMAP_DUMPING ){` |
|     ! 0 |  623 | `			SyBlobAppend(pOut,"NULL",4); /* circular reference -> NULL, like PHP */` |
|     ! 0 |  624 | `		}else{` |
|       - |  625 | `			VmExportCtx ctx;` |
|     152 |  626 | `			pMap->iFlags \|= HASHMAP_DUMPING;` |
|     152 |  627 | `			SyBlobAppend(pOut,"array (\n",8);` |
|     152 |  628 | `			ctx.pOut = pOut; ctx.nIndent = nIndent; ctx.depth = depth;` |
|     152 |  629 | `			ph7_array_walk(pVal,VmExportArrayWalk,&ctx);` |
|     152 |  630 | `			VmExportIndent(pOut,nIndent);` |
|     152 |  631 | `			SyBlobAppend(pOut,")",1);` |
|     152 |  632 | `			pMap->iFlags &= ~HASHMAP_DUMPING;` |
|       4 |  633 | `		}` |
|     100 |  634 | `	}else if( ph7_value_is_object(pVal) ){` |
|      26 |  635 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|      26 |  636 | `		if( pThis->pClass->iFlags & PH7_CLASS_ENUM ){` |
|       - |  637 | ``			/* php 8.1: an enum case exports as `\S::A` */`` |
|       3 |  638 | `			ph7_value *pName = PH7_EnumCaseNameValue(pThis);` |
|       3 |  639 | `			SyBlobAppend(pOut,"\\",1);` |
|       3 |  640 | `			SyBlobAppend(pOut,pThis->pClass->sName.zString,pThis->pClass->sName.nByte);` |
|       3 |  641 | `			SyBlobAppend(pOut,"::",2);` |
|       3 |  642 | `			if( pName && SyBlobLength(&pName->sBlob) > 0 ){` |
|       3 |  643 | `				SyBlobAppend(pOut,SyBlobData(&pName->sBlob),SyBlobLength(&pName->sBlob));` |
|       2 |  644 | `			}` |
|      25 |  645 | `		}else if( pThis->iFlags & VM_INSTANCE_DUMPING ){` |
|     ! 0 |  646 | `			SyBlobAppend(pOut,"NULL",4); /* circular reference -> NULL, like PHP */` |
|     ! 0 |  647 | `		}else{` |
|      24 |  648 | `			SyString *pClassName = &pThis->pClass->sName;` |
|       - |  649 | `			SyHashEntry *pEntry;` |
|       - |  650 | `			SySet sNames;` |
|       - |  651 | `			SyString *aName;` |
|       - |  652 | `			sxu32 iName,nName;` |
|      24 |  653 | `			pThis->iFlags \|= VM_INSTANCE_DUMPING;` |
|      24 |  654 | `			SyBlobAppend(pOut,"\\",1);` |
|      24 |  655 | `			SyBlobAppend(pOut,pClassName->zString,pClassName->nByte);` |
|      24 |  656 | `			SyBlobAppend(pOut,"::__set_state(array(\n",21);` |
|       - |  657 | `			/* SNAPSHOT the attribute names first: a PHP 8.4 get hook dispatched` |
|       - |  658 | `			 * mid-walk may re-enter an hAttr walk on this instance (the hash has` |
|       - |  659 | `			 * a single embedded loop cursor) or unset()/create properties; names` |
|       - |  660 | `			 * point into class-owned attr storage and each is re-looked-up. */` |
|      24 |  661 | `			SySetInit(&sNames,&pThis->pVm->sAllocator,sizeof(SyString));` |
|      24 |  662 | `			SyHashResetLoopCursor(&pThis->hAttr);` |
|      68 |  663 | `			while( (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|      46 |  664 | `				VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|      46 |  665 | `				if( pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){ continue; }` |
|      44 |  666 | `				if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_VIRTUAL))` |
|      24 |  667 | `				 == PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|       3 |  668 | `					continue; /* virtual set-only property: no value to export (php) */` |
|       - |  669 | `				}` |
|      44 |  670 | `				SySetPut(&sNames,(const void *)&pVmAttr->pAttr->sName);` |
|       2 |  671 | `			}` |
|      24 |  672 | `			aName = (SyString *)SySetBasePtr(&sNames);` |
|      24 |  673 | `			nName = SySetUsed(&sNames);` |
|      66 |  674 | `			for( iName = 0 ; iName < nName ; ++iName ){` |
|      44 |  675 | `				SyString *pAName = &aName[iName];` |
|       - |  676 | `				VmClassAttr *pVmAttr;` |
|       - |  677 | `				ph7_value *pAttrVal;` |
|      44 |  678 | `				pEntry = SyHashGet(&pThis->hAttr,(const void *)pAName->zString,pAName->nByte);` |
|      44 |  679 | `				if( pEntry == 0 ){ continue; } /* unset by an earlier hook */` |
|      44 |  680 | `				pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|      44 |  681 | `				VmExportIndent(pOut,nIndent+3); /* object property lines sit one deeper than arrays */` |
|      44 |  682 | `				VmExportQuoted(pOut,pAName->zString,(int)pAName->nByte);` |
|       - |  683 | `				/* PHP 8.4 property hooks: var_export() reads through the get hook` |
|       - |  684 | `				 * (every visibility — php exports private hooked values too). A` |
|       - |  685 | `				 * throwing hook parks on the boundary rail; the helper's boundary` |
|       - |  686 | `				 * gate keeps LATER hooks from running (the raw values the tail of` |
|       - |  687 | `				 * the export falls back to are discarded when the throw routes). */` |
|       - |  688 | `				{` |
|       - |  689 | `					ph7_value sHookVal;` |
|       - |  690 | `					sxi32 rcHk;` |
|      44 |  691 | `					PH7_MemObjInit(pThis->pVm,&sHookVal);` |
|      44 |  692 | `					rcHk = PH7_VmHookGetAttrValue(pThis,pVmAttr,&sHookVal);` |
|      44 |  693 | `					if( rcHk == SXRET_OK ){` |
|      13 |  694 | `						VmExportEntryValue(pOut,&sHookVal,nIndent,depth);` |
|      13 |  695 | `						PH7_MemObjRelease(&sHookVal);` |
|      13 |  696 | `						continue;` |
|       - |  697 | `					}` |
|      32 |  698 | `					PH7_MemObjRelease(&sHookVal);` |
|      32 |  699 | `					if( rcHk != SXERR_NOTFOUND ){` |
|       - |  700 | `						/* the hook threw (parked on the boundary rail): NULL` |
|       - |  701 | `						 * placeholder keeps the output well-formed */` |
|     ! 0 |  702 | `						SyBlobAppend(pOut," => NULL,\n",10);` |
|     ! 0 |  703 | `						continue;` |
|       - |  704 | `					}` |
|       - |  705 | `				}` |
|      32 |  706 | `				pAttrVal = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|      32 |  707 | `				if( pAttrVal ){ VmExportEntryValue(pOut,pAttrVal,nIndent,depth); }` |
|     ! 0 |  708 | `				else { SyBlobAppend(pOut," => NULL,\n",10); }` |
|      17 |  709 | `			}` |
|      24 |  710 | `			SySetRelease(&sNames);` |
|      24 |  711 | `			VmExportIndent(pOut,nIndent);` |
|      24 |  712 | `			SyBlobAppend(pOut,"))",2);` |
|      24 |  713 | `			pThis->iFlags &= ~VM_INSTANCE_DUMPING;` |
|       - |  714 | `		}` |
|      14 |  715 | `	}else{` |
|       - |  716 | `		/* resource / other -> PHP emits NULL (with a warning we omit) */` |
|     ! 0 |  717 | `		SyBlobAppend(pOut,"NULL",4);` |
|       - |  718 | `	}` |
|    1050 |  719 | `}` |
|       - |  720 | `/*` |
|       - |  721 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|       - |  722 | ` *  PHP-exact evaluable representation (see VmExportValue).` |
|       - |  723 | ` */` |
|    1712 |  724 | `PH7_PRIVATE int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  725 | `{` |
|    1717 |  726 | `	int ret_string = 0;` |
|       - |  727 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|    1717 |  728 | `	if( nArg < 1 ){` |
|       - |  729 | `		/* Nothing to output,return FALSE */` |
|     ! 0 |  730 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  731 | `		return SXRET_OK;` |
|       - |  732 | `	}` |
|    1717 |  733 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|    1717 |  734 | `	if ( nArg > 1 ){` |
|       - |  735 | `		/* Where to redirect output */` |
|    1361 |  736 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|     678 |  737 | `	}` |
|       - |  738 | `	/* Generate the PHP-exact evaluable representation */` |
|    1717 |  739 | `	VmExportValue(&sDump,apArg[0],0,0);` |
|    1717 |  740 | `	if( !ret_string ){` |
|       - |  741 | `		/* Output dump */` |
|     361 |  742 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|       - |  743 | `		/* Return NULL */` |
|     361 |  744 | `		ph7_result_null(pCtx);` |
|     183 |  745 | `	}else{` |
|       - |  746 | `		/* Generated dump as return value */` |
|    1361 |  747 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|       - |  748 | `	}` |
|       - |  749 | `	/* Release the working buffer */` |
|    1717 |  750 | `	SyBlobRelease(&sDump);` |
|    1717 |  751 | `	return SXRET_OK;` |
|     861 |  752 | `}` |
|       - |  753 |  |
