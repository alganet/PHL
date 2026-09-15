# src/ph7/vm_builtin_var.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 382/420 lines (90.95%)

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
|  115190 |   23 | `PH7_PRIVATE int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |   24 | `{` |
|       - |   25 | `	ph7_value *pObj;` |
|  115195 |   26 | `	int res = 0;` |
|       - |   27 | `	int i;` |
|  115195 |   28 | `	if( nArg < 1 ){` |
|       - |   29 | `		/* Missing arguments,return false */` |
|     ! 0 |   30 | `		ph7_result_bool(pCtx,res);` |
|     ! 0 |   31 | `		return SXRET_OK;` |
|       - |   32 | `	}` |
|       - |   33 | `	/* Iterate over available arguments */` |
|  148053 |   34 | `	for( i = 0 ; i < nArg ; ++i ){` |
|  115207 |   35 | `		pObj = apArg[i];` |
|  115207 |   36 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|       - |   37 | `			/* Skip the "expecting a variable" warning for MEMOBJ_BOOL —` |
|       - |   38 | `			 * synthesized by LOAD_IDX iP2=4 (ArrayAccess::offsetExists) and` |
|       - |   39 | `			 * by anyone passing a bool literal (rare, harmless). */` |
|   79555 |   40 | `			if( (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|       - |   41 | `				/* Not so fatal,Throw a warning */` |
|     ! 0 |   42 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|     ! 0 |   43 | `			}` |
|   39775 |   44 | `		}` |
|  115207 |   45 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|  115207 |   46 | `		if( !res ){` |
|       - |   47 | `			/* Variable not set,return FALSE */` |
|   82349 |   48 | `			ph7_result_bool(pCtx,0);` |
|   82349 |   49 | `			return SXRET_OK;` |
|       - |   50 | `		}` |
|   16434 |   51 | `	}` |
|       - |   52 | `	/* All given variable are set,return TRUE */` |
|   32851 |   53 | `	ph7_result_bool(pCtx,1);` |
|   32851 |   54 | `	return SXRET_OK;` |
|   57600 |   55 | `}` |
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
|    6788 |   75 | `PH7_PRIVATE sxi32 VmUnsetVarByName(ph7_vm *pVm,VmFrame *pFrame,const char *zName,sxu32 nByte)` |
|       5 |   76 | `{` |
|       - |   77 | `	SyHashEntry *pEntry;` |
|       - |   78 | `	VmRefObj *pRef;` |
|       - |   79 | `	sxu32 nIdx;` |
|       - |   80 | `	/* php 8.1 forbids unset($GLOBALS) outright. Checked by NAME: the superglobal is not an` |
|       - |   81 | `	 * ordinary hVar binding, so the slot-index test below never sees it. */` |
|    6793 |   82 | `	if( nByte == sizeof("GLOBALS")-1 && SyMemcmp(zName,"GLOBALS",nByte) == 0 ){` |
|       3 |   83 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|       - |   84 | `			"$GLOBALS can only be modified using the $GLOBALS[$name] = $value syntax");` |
|       3 |   85 | `		pVm->iExitStatus = 255;` |
|       3 |   86 | `		pVm->bHaltRequested = 1;` |
|       3 |   87 | `		return PH7_ABORT;` |
|       - |   88 | `	}` |
|    6791 |   89 | `	pEntry = SyHashGet(&pFrame->hVar,(const void *)zName,nByte);` |
|    6791 |   90 | `	if( pEntry == 0 ){` |
|       - |   91 | `		/* No such variable: unset() is a no-op on an undefined name, as in php */` |
|     925 |   92 | `		return SXRET_OK;` |
|       - |   93 | `	}` |
|    5870 |   94 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|    5870 |   95 | `	if( nIdx == pVm->nGlobalIdx ){` |
|     ! 0 |   96 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|       - |   97 | `			"$GLOBALS can only be modified using the $GLOBALS[$name] = $value syntax");` |
|     ! 0 |   98 | `		pVm->iExitStatus = 255;` |
|     ! 0 |   99 | `		pVm->bHaltRequested = 1;` |
|     ! 0 |  100 | `		return PH7_ABORT;` |
|       - |  101 | `	}` |
|    5870 |  102 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|       - |  103 | `	/*` |
|       - |  104 | `	 * A GLOBAL variable is ALSO an entry of the $GLOBALS array, and that node points at the` |
|       - |  105 | `	 * same slot. php's unset($x) in global scope drops $GLOBALS['x'] too, so unlink the node` |
|       - |  106 | `	 * — otherwise it stays a live holder and the value is never released ($o = new D;` |
|       - |  107 | `	 * unset($o); stopped running the destructor). Clear the reference table's row for the` |
|       - |  108 | `	 * node BEFORE unlinking it: unlinking frees the node, and the holder count below would` |
|       - |  109 | `	 * otherwise dereference freed memory.` |
|       - |  110 | `	 */` |
|    5870 |  111 | `	if( pFrame->pParent == 0 ){` |
|    5868 |  112 | `		ph7_value *pGlobals = (ph7_value *)SySetAt(&pVm->aMemObj,pVm->nGlobalIdx);` |
|    5868 |  113 | `		if( pGlobals && (pGlobals->iFlags & MEMOBJ_HASHMAP) ){` |
|    5868 |  114 | `			ph7_hashmap_node *pNode = 0;` |
|       - |  115 | `			ph7_value sKey;` |
|       - |  116 | `			SyString sName;` |
|    5868 |  117 | `			PH7_MemObjInit(&(*pVm),&sKey);` |
|    5868 |  118 | `			SyStringInitFromBuf(&sName,zName,nByte);` |
|    5868 |  119 | `			PH7_MemObjInitFromString(&(*pVm),&sKey,&sName);` |
|    5864 |  120 | `			if( SXRET_OK == PH7_HashmapLookup((ph7_hashmap *)pGlobals->x.pOther,&sKey,&pNode)` |
|    5868 |  121 | `			 && pNode && pNode->nValIdx == nIdx ){` |
|    5866 |  122 | `				if( pRef ){` |
|    5866 |  123 | `					ph7_hashmap_node **apN = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|       - |  124 | `					sxu32 k;` |
|   11826 |  125 | `					for( k = 0 ; k < SySetUsed(&pRef->aArrEntries) ; ++k ){` |
|    5964 |  126 | `						if( apN[k] == pNode ){` |
|    5866 |  127 | `							apN[k] = 0;` |
|    2931 |  128 | `						}` |
|    2984 |  129 | `					}` |
|    2931 |  130 | `				}` |
|    5866 |  131 | `				PH7_HashmapUnlinkNode(pNode,FALSE);` |
|    2931 |  132 | `			}` |
|    5868 |  133 | `			PH7_MemObjRelease(&sKey);` |
|    2932 |  134 | `		}` |
|    2932 |  135 | `	}` |
|       - |  136 |  |
|    5870 |  137 | `	if( pRef == 0 ){` |
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
|    5870 |  148 | `		SyHashEntry **apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|    5870 |  149 | `		ph7_hashmap_node **apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|    5870 |  150 | `		sxu32 n, nLive = 0;` |
|       - |  151 | `		/* Forget THIS name in the slot's reference record (leave the others alone) */` |
|   11832 |  152 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; ++n ){` |
|    5966 |  153 | `			if( apEntry[n] == pEntry ){` |
|    5864 |  154 | `				apEntry[n] = 0;` |
|    2930 |  155 | `			}` |
|    2985 |  156 | `		}` |
|    5870 |  157 | `		SyHashDeleteEntry2(pEntry);` |
|       - |  158 | `		/* Anything else still holding the slot? */` |
|   11832 |  159 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; ++n ){` |
|    5966 |  160 | `			if( apEntry[n] ){` |
|      29 |  161 | `				nLive++;` |
|      14 |  162 | `			}` |
|    2985 |  163 | `		}` |
|   11832 |  164 | `		for( n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|       - |  165 | `			/* Only a node that STILL points at this slot is a holder. The reference table` |
|       - |  166 | `			 * keeps stale rows (a slot index is recycled through the free list, and the row` |
|       - |  167 | `			 * outlives the node that put it there), so an un-filtered count reports holders` |
|       - |  168 | `			 * that no longer exist and the value would never be released — the destructor` |
|       - |  169 | ``			 * of `$o = new D; unset($o);` stopped running. */`` |
|    5966 |  170 | `			if( apNode[n] && apNode[n]->nValIdx == nIdx ){` |
|      63 |  171 | `				nLive++;` |
|      31 |  172 | `			}` |
|    2985 |  173 | `		}` |
|    5870 |  174 | `		if( nLive < 1 ){` |
|       - |  175 | `			/* Last holder gone: now the value may go too */` |
|    5814 |  176 | `			PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|       - |  177 | `			/* Slot returned to the free pool: drop its stale local-teardown entry so a` |
|       - |  178 | `			 * later reuse of the index is not double-freed on frame exit (see` |
|       - |  179 | `			 * VmDropFrameLocalSlot). */` |
|    5814 |  180 | `			VmDropFrameLocalSlot(&(*pVm),nIdx);` |
|    2905 |  181 | `		}` |
|       - |  182 | `	}` |
|    5870 |  183 | `	return SXRET_OK;` |
|    3399 |  184 | `}` |
|       - |  185 | `/*` |
|       - |  186 | ` * Is this memory slot aliased — i.e. does anything other than its owner refer to it?` |
|       - |  187 | ` * var_dump marks such an array element with '&' ("&int(2)"). PH7 only flagged nodes that` |
|       - |  188 | `` * were FOREIGN (`array(&$x)`, where the node points at an outside slot) and so missed the`` |
|       - |  189 | `` * common case, a reference taken TO an element (`$r = &$a[1]`), where the array still owns`` |
|       - |  190 | ` * the value but is no longer its only holder.` |
|       - |  191 | ` */` |
|  709866 |  192 | `PH7_PRIVATE int PH7_VmSlotIsReferenced(ph7_vm *pVm,sxu32 nIdx)` |
|       5 |  193 | `{` |
|       - |  194 | `	VmRefObj *pRef;` |
|  709871 |  195 | `	sxu32 n, nLive = 0;` |
|       - |  196 | `	SyHashEntry **apEntry;` |
|  709871 |  197 | `	if( nIdx == SXU32_HIGH ){` |
|     ! 0 |  198 | `		return 0;` |
|       - |  199 | `	}` |
|  709871 |  200 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  709871 |  201 | `	if( pRef == 0 ){` |
|     ! 0 |  202 | `		return 0;` |
|       - |  203 | `	}` |
|  709871 |  204 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|  709875 |  205 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; ++n ){` |
|       5 |  206 | `		if( apEntry[n] ){` |
|       5 |  207 | `			nLive++;` |
|       2 |  208 | `		}` |
|       3 |  209 | `	}` |
|  709871 |  210 | `	return nLive > 0;` |
|  354938 |  211 | `}` |
| 4230669 |  212 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|       5 |  213 | `{` |
|       - |  214 | `	ph7_value *pObj;` |
|       - |  215 | `	VmRefObj *pRef;` |
| 4230674 |  216 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
| 4230674 |  217 | `	if( pObj ){` |
|       - |  218 | `		/* Release the object */` |
| 4230674 |  219 | `		PH7_MemObjRelease(pObj);` |
| 2115949 |  220 | `	}` |
|       - |  221 | `	/* Remove old reference links */` |
| 4230674 |  222 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
| 4230674 |  223 | `	if( pRef ){` |
| 4230674 |  224 | `		sxi32 iFlags = pRef->iFlags;` |
|       - |  225 | `		/* Unlink from the reference table */` |
| 4230674 |  226 | `		VmRefObjUnlink(&(*pVm),pRef);` |
| 4230674 |  227 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|       - |  228 | `			VmSlot sFree;` |
|       - |  229 | `			/* Restore to the free list */` |
| 4230662 |  230 | `			sFree.nIdx = nObjIdx;` |
| 4230662 |  231 | `			sFree.pUserData = 0;` |
| 4230662 |  232 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
| 2115943 |  233 | `		}` |
| 2115949 |  234 | `	}` |
| 4230674 |  235 | `	return SXRET_OK;` |
|       5 |  236 | `}` |
|       - |  237 | `/*` |
|       - |  238 | ` * void unset($var,...)` |
|       - |  239 | ` *   Unset one or more given variable.` |
|       - |  240 | ` * Parameters` |
|       - |  241 | ` *  One or more variable to unset.` |
|       - |  242 | ` * Return` |
|       - |  243 | ` *  Nothing.` |
|       - |  244 | ` */` |
|    1086 |  245 | `PH7_PRIVATE int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 |  246 | `{` |
|       - |  247 | `	ph7_value *pObj;` |
|       - |  248 | `	ph7_vm *pVm;` |
|       - |  249 | `	int i;` |
|       - |  250 | `	/* Point to the target VM */` |
|    1090 |  251 | `	pVm = pCtx->pVm;` |
|       - |  252 | `	/* Iterate and unset */` |
|    2176 |  253 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    1090 |  254 | `		pObj = apArg[i];` |
|    1090 |  255 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      52 |  256 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|       - |  257 | `				/* Throw an error */` |
|     ! 0 |  258 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|     ! 0 |  259 | `			}` |
|      28 |  260 | `		}else{` |
|    1042 |  261 | `			sxu32 nIdx = pObj->nIdx;` |
|    1042 |  262 | `			if( nIdx == pVm->nGlobalIdx ){` |
|       - |  263 | `				/* php 8.1: unset($GLOBALS) is forbidden — the same fatal as` |
|       - |  264 | `				 * re-assigning it (compile-time in php, raised here). */` |
|     ! 0 |  265 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|       - |  266 | `					"$GLOBALS can only be modified using the $GLOBALS[$name] = $value syntax");` |
|     ! 0 |  267 | `				pVm->iExitStatus = 255;` |
|     ! 0 |  268 | `				pVm->bHaltRequested = 1;` |
|     ! 0 |  269 | `				return PH7_ABORT;` |
|       - |  270 | `			}` |
|    1042 |  271 | `			PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|       - |  272 | `			/* Drop the stale local-teardown entry for the freed slot (see` |
|       - |  273 | `			 * VmDropFrameLocalSlot) so a later index reuse is not double-freed. */` |
|    1042 |  274 | `			VmDropFrameLocalSlot(&(*pVm),nIdx);` |
|       - |  275 | `		}` |
|     547 |  276 | `	}` |
|    1090 |  277 | `	return SXRET_OK;` |
|     547 |  278 | `}` |
|       - |  279 | `/*` |
|       - |  280 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|       - |  281 | ` */` |
|     414 |  282 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|       1 |  283 | `{` |
|     415 |  284 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|     415 |  285 | `	ph7_vm *pVm = pArray->pVm;` |
|       - |  286 | `	ph7_value *pObj;` |
|       - |  287 | `	sxu32 nIdx;` |
|       - |  288 | `	/* php excludes $this from get_defined_vars() (it is bound implicitly, not a` |
|       - |  289 | `	 * declared local). PHL keeps it in the frame's hVar, so skip it here. */` |
|     414 |  290 | `	if( pEntry->nKeyLen == sizeof("this")-1` |
|     256 |  291 | `	 && SyMemcmp(pEntry->pKey,"this",sizeof("this")-1) == 0 ){` |
|       3 |  292 | `		return SXRET_OK;` |
|       - |  293 | `	}` |
|       - |  294 | `	/* Extract the memory object */` |
|     413 |  295 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|     413 |  296 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     413 |  297 | `	if( pObj ){` |
|     413 |  298 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|     411 |  299 | `			if( pEntry->nKeyLen > 0 ){` |
|       - |  300 | `				SyString sName;` |
|       - |  301 | `				ph7_value sKey;` |
|       - |  302 | `				/* Perform the insertion (pObj may point into pVm->aMemObj; the` |
|       - |  303 | `				 * inserter snapshots the source before reserving, so the pool may` |
|       - |  304 | `				 * safely move underneath it — see HashmapInsertIntKey/BlobKey). */` |
|     411 |  305 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|     411 |  306 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|     411 |  307 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|     411 |  308 | `				PH7_MemObjRelease(&sKey);` |
|     205 |  309 | `			}` |
|     205 |  310 | `		}` |
|     206 |  311 | `	}` |
|     413 |  312 | `	return SXRET_OK;` |
|     208 |  313 | `}` |
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
|      58 |  358 | `PH7_PRIVATE int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  359 | `{` |
|       - |  360 | `	/* php's gettype() uses LONG type names ("integer"/"boolean"/"NULL"), distinct` |
|       - |  361 | `	 * from get_debug_type()'s short ones ("int"/"bool"/"null") — never route this` |
|       - |  362 | `	 * through PH7_MemObjTypeDump, which yields the short forms. */` |
|      61 |  363 | `	const char *zType = "unknown type";` |
|      61 |  364 | `	if( nArg > 0 ){` |
|      61 |  365 | `		ph7_value *pVal = apArg[0];` |
|      61 |  366 | `		if( pVal->iFlags & MEMOBJ_NULL ){` |
|       3 |  367 | `			zType = "NULL";` |
|      60 |  368 | `		}else if( pVal->iFlags & MEMOBJ_REAL ){` |
|       - |  369 | `			/* REAL wins over a cached MEMOBJ_INT: 1.0 is "double" (php). */` |
|       9 |  370 | `			zType = "double";` |
|      55 |  371 | `		}else if( pVal->iFlags & MEMOBJ_INT ){` |
|       3 |  372 | `			zType = "integer";` |
|      50 |  373 | `		}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|      25 |  374 | `			zType = "string";` |
|      36 |  375 | `		}else if( pVal->iFlags & MEMOBJ_BOOL ){` |
|       5 |  376 | `			zType = "boolean";` |
|      23 |  377 | `		}else if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|       3 |  378 | `			zType = "array";` |
|      20 |  379 | `		}else if( pVal->iFlags & MEMOBJ_OBJ ){` |
|       9 |  380 | `			zType = "object";` |
|      15 |  381 | `		}else if( pVal->iFlags & MEMOBJ_RES ){` |
|       - |  382 | `			/* php reports an fclose()'d handle as "resource (closed)" */` |
|      11 |  383 | `			zType = PH7_VfsResourceIsClosed(pVal->x.pOther) ? "resource (closed)" : "resource";` |
|       5 |  384 | `		}` |
|      29 |  385 | `	}` |
|       - |  386 | `	/* Return the variable type */` |
|      61 |  387 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|      61 |  388 | `	return SXRET_OK;` |
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
|       6 |  403 | `PH7_PRIVATE int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  404 | `{` |
|       - |  405 | `	const char *zType;` |
|       7 |  406 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|       - |  407 | `		/* Missing/Invalid arguments,return FALSE*/` |
|     ! 0 |  408 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  409 | `		return PH7_OK;` |
|       - |  410 | `	}` |
|       - |  411 | `	/* PHP returns the resource TYPE name (e.g. "stream" for IO handles), not an id. */` |
|       7 |  412 | `	zType = PH7_VfsResourceType(apArg[0]->x.pOther);` |
|       7 |  413 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       7 |  414 | `	return SXRET_OK;` |
|       4 |  415 | `}` |
|       - |  416 | `/*` |
|       - |  417 | ` * void var_dump(expression,....)` |
|       - |  418 | ` *   var_dump � Dumps information about a variable` |
|       - |  419 | ` * Parameters` |
|       - |  420 | ` *   One or more expression to dump.` |
|       - |  421 | ` * Returns` |
|       - |  422 | ` *  Nothing.` |
|       - |  423 | ` */` |
|     488 |  424 | `PH7_PRIVATE int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 |  425 | `{` |
|       - |  426 | `	SyBlob sDump; /* Generated dump is stored here */` |
|       - |  427 | `	int i;` |
|     492 |  428 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       - |  429 | `	/* Dump one or more expressions */` |
|    1058 |  430 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     570 |  431 | `		ph7_value *pObj = apArg[i];` |
|       - |  432 | `		/* Reset the working buffer */` |
|     570 |  433 | `		SyBlobReset(&sDump);` |
|       - |  434 | `		/* Dump the given expression */` |
|     570 |  435 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|       - |  436 | `		/* Output */` |
|     570 |  437 | `		if( SyBlobLength(&sDump) > 0 ){` |
|     570 |  438 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|     283 |  439 | `		}` |
|     287 |  440 | `	}` |
|       - |  441 | `	/* Release the working buffer */` |
|     492 |  442 | `	SyBlobRelease(&sDump);` |
|     492 |  443 | `	return SXRET_OK;` |
|       4 |  444 | `}` |
|       - |  445 | `/*` |
|       - |  446 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|       - |  447 | ` *   print-r - Prints human-readable information about a variable` |
|       - |  448 | ` * Parameters` |
|       - |  449 | ` *   expression: Expression to dump` |
|       - |  450 | ` *   return : If you would like to capture the output of print_r() use` |
|       - |  451 | ` *            the return parameter. When this parameter is set to TRUE` |
|       - |  452 | ` *            print_r() will return the information rather than print it.` |
|       - |  453 | ` * Return` |
|       - |  454 | ` *  When the return parameter is TRUE, this function will return a string.` |
|       - |  455 | ` *  Otherwise, the return value is TRUE.` |
|       - |  456 | ` */` |
|      84 |  457 | `PH7_PRIVATE int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  458 | `{` |
|      86 |  459 | `	int ret_string = 0;` |
|       - |  460 | `	SyBlob sDump;` |
|      86 |  461 | `	if( nArg < 1 ){` |
|       - |  462 | `		/* Nothing to output,return FALSE */` |
|     ! 0 |  463 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  464 | `		return SXRET_OK;` |
|       - |  465 | `	}` |
|      86 |  466 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|      86 |  467 | `	if ( nArg > 1 ){` |
|       - |  468 | `		/* Where to redirect output */` |
|       9 |  469 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|       4 |  470 | `	}` |
|       - |  471 | `	/* Generate dump */` |
|      86 |  472 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|      86 |  473 | `	if( !ret_string ){` |
|       - |  474 | `		/* Output dump */` |
|      78 |  475 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|       - |  476 | `		/* Return true */` |
|      78 |  477 | `		ph7_result_bool(pCtx,1);` |
|      40 |  478 | `	}else{` |
|       - |  479 | `		/* Generated dump as return value */` |
|       9 |  480 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|       - |  481 | `	}` |
|       - |  482 | `	/* Release the working buffer */` |
|      86 |  483 | `	SyBlobRelease(&sDump);` |
|      86 |  484 | `	return SXRET_OK;` |
|      44 |  485 | `}` |
|       - |  486 | `/*` |
|       - |  487 | ` * var_export() — PHP-exact evaluable representation of a value.` |
|       - |  488 | ` *` |
|       - |  489 | ` * Distinct from print_r/var_dump (which share PH7_MemObjDump): var_export emits` |
|       - |  490 | ` * parseable PHP — 'single-quoted' strings, true/false/NULL, array (\n  k => v,\n),` |
|       - |  491 | ` * and \Class::__set_state(array(...)) for objects. PHP's indentation is quirky` |
|       - |  492 | ` * (2-space array entries; 3-space object property lines; a composite value always` |
|       - |  493 | ` * opens at containerIndent+2) but deterministic; this matches it byte-for-byte.` |
|       - |  494 | ` */` |
|       - |  495 | `/* Instance flag (distinct from oo.c's CLASS_INSTANCE_DESTROYED 0x001) marking an` |
|       - |  496 | ` * object currently on the var_export recursion stack, for cycle detection. */` |
|       - |  497 | `#define VM_INSTANCE_DUMPING 0x002` |
|       - |  498 | `typedef struct VmExportCtx VmExportCtx;` |
|       - |  499 | `struct VmExportCtx` |
|       - |  500 | `{` |
|       - |  501 | `	SyBlob *pOut;` |
|       - |  502 | `	int nIndent;  /* indentation of the container emitting this entry */` |
|       - |  503 | `	int depth;    /* recursion guard */` |
|       - |  504 | `};` |
|       - |  505 | `static void VmExportValue(SyBlob *pOut, ph7_value *pVal, int nIndent, int depth);` |
|       - |  506 | `/* Append nIndent spaces. */` |
|     586 |  507 | `static void VmExportIndent(SyBlob *pOut, int nIndent)` |
|       5 |  508 | `{` |
|       - |  509 | `	int i;` |
|    1713 |  510 | `	for( i = 0; i < nIndent; i++ ){ SyBlobAppend(pOut," ",1); }` |
|     591 |  511 | `}` |
|       - |  512 | `/* Append a single-quoted PHP string literal (escapes ' and \, batching safe runs` |
|       - |  513 | ` * in one append). A NUL byte can't live in a single-quoted literal, so PHP splits` |
|       - |  514 | ` * it out as ' . "\0" . ' — match that. */` |
|     316 |  515 | `static void VmExportQuoted(SyBlob *pOut, const char *z, int n)` |
|       5 |  516 | `{` |
|     321 |  517 | `	int i, run = 0;` |
|     321 |  518 | `	SyBlobAppend(pOut,"'",1);` |
|    1809 |  519 | `	for( i = 0; i < n; i++ ){` |
|    1493 |  520 | `		char c = z[i];` |
|    1493 |  521 | `		if( c != '\0' && c != '\'' && c != '\\' ){ continue; } /* extend the safe run */` |
|       7 |  522 | `		if( i > run ){ SyBlobAppend(pOut,&z[run],(sxu32)(i-run)); }` |
|       7 |  523 | `		if( c == '\0' ){ SyBlobAppend(pOut,"' . \"\\0\" . '",12); }` |
|       5 |  524 | `		else { SyBlobAppend(pOut,"\\",1); SyBlobAppend(pOut,&z[i],1); }` |
|       7 |  525 | `		run = i+1;` |
|       4 |  526 | `	}` |
|     321 |  527 | `	if( n > run ){ SyBlobAppend(pOut,&z[run],(sxu32)(n-run)); }` |
|     321 |  528 | `	SyBlobAppend(pOut,"'",1);` |
|     321 |  529 | `}` |
|       - |  530 | `/* True if the array/object is already on the var_export recursion stack. */` |
|     382 |  531 | `static int VmExportIsCycle(ph7_value *pVal)` |
|       5 |  532 | `{` |
|     387 |  533 | `	if( ph7_value_is_array(pVal) ){` |
|      37 |  534 | `		return (((ph7_hashmap *)pVal->x.pOther)->iFlags & HASHMAP_DUMPING) != 0;` |
|       - |  535 | `	}` |
|     353 |  536 | `	if( ph7_value_is_object(pVal) ){` |
|       5 |  537 | `		return (((ph7_class_instance *)pVal->x.pOther)->iFlags & VM_INSTANCE_DUMPING) != 0;` |
|       - |  538 | `	}` |
|     349 |  539 | `	return 0;` |
|     196 |  540 | `}` |
|       - |  541 | `/* Emit " => " then the value: a scalar inline, a composite on its own line at` |
|       - |  542 | ` * containerIndent+2. A circular reference renders inline as NULL (like PHP). */` |
|     382 |  543 | `static void VmExportEntryValue(SyBlob *pOut, ph7_value *pVal, int nContainerIndent, int depth)` |
|       5 |  544 | `{` |
|     387 |  545 | `	SyBlobAppend(pOut," => ",4);` |
|     387 |  546 | `	if( VmExportIsCycle(pVal) ){` |
|       5 |  547 | `		SyBlobAppend(pOut,"NULL",4);` |
|     385 |  548 | `	}else if( ph7_value_is_array(pVal) \|\| ph7_value_is_object(pVal) ){` |
|      37 |  549 | `		SyBlobAppend(pOut,"\n",1);` |
|      37 |  550 | `		VmExportIndent(pOut,nContainerIndent+2);` |
|      37 |  551 | `		VmExportValue(pOut,pVal,nContainerIndent+2,depth+1);` |
|      20 |  552 | `	}else{` |
|     349 |  553 | `		VmExportValue(pOut,pVal,nContainerIndent+2,depth+1);` |
|       - |  554 | `	}` |
|     387 |  555 | `	SyBlobAppend(pOut,",\n",2);` |
|     387 |  556 | `}` |
|       - |  557 | `/* Array walker: "<indent+2>key => value,\n" for each entry. */` |
|     340 |  558 | `static int VmExportArrayWalk(ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|       4 |  559 | `{` |
|     344 |  560 | `	VmExportCtx *pC = (VmExportCtx *)pUserData;` |
|     344 |  561 | `	VmExportIndent(pC->pOut,pC->nIndent+2);` |
|     344 |  562 | `	if( ph7_value_is_string(pKey) ){` |
|       - |  563 | `		int n;` |
|      76 |  564 | `		const char *z = ph7_value_to_string(pKey,&n);` |
|      76 |  565 | `		VmExportQuoted(pC->pOut,z,n);` |
|      40 |  566 | `	}else{` |
|     272 |  567 | `		SyBlobFormat(pC->pOut,"%qd",ph7_value_to_int64(pKey));` |
|       - |  568 | `	}` |
|     344 |  569 | `	VmExportEntryValue(pC->pOut,pValue,pC->nIndent,pC->depth);` |
|     344 |  570 | `	return PH7_OK;` |
|       4 |  571 | `}` |
|    1884 |  572 | `static void VmExportValue(SyBlob *pOut, ph7_value *pVal, int nIndent, int depth)` |
|       5 |  573 | `{` |
|    1889 |  574 | `	if( depth > 4096 ){ return; } /* backstop for pathological finite nesting */` |
|    1889 |  575 | `	if( ph7_value_is_null(pVal) ){` |
|      93 |  576 | `		SyBlobAppend(pOut,"NULL",4);` |
|    1845 |  577 | `	}else if( ph7_value_is_bool(pVal) ){` |
|     589 |  578 | `		if( ph7_value_to_bool(pVal) ){ SyBlobAppend(pOut,"true",4); }` |
|     281 |  579 | `		else { SyBlobAppend(pOut,"false",5); }` |
|    1509 |  580 | `	}else if( ph7_value_is_float(pVal) ){` |
|       - |  581 | `		/* float before int: ph7_value_is_int is lenient (true for integer reals). */` |
|     328 |  582 | `		sxu32 before = SyBlobLength(pOut), i, after;` |
|       - |  583 | `		const char *z;` |
|     328 |  584 | `		int plain = 1;` |
|     328 |  585 | `		PH7_AppendShortestReal(pOut,ph7_value_to_double(pVal));` |
|     328 |  586 | `		z = (const char *)SyBlobData(pOut);` |
|     328 |  587 | `		after = SyBlobLength(pOut);` |
|     810 |  588 | `		for( i = before; i < after; i++ ){` |
|     666 |  589 | `			if( !((z[i]>='0'&&z[i]<='9')\|\|z[i]=='-') ){ plain = 0; break; }` |
|     245 |  590 | `		}` |
|     328 |  591 | `		if( plain ){ SyBlobAppend(pOut,".0",2); } /* integer-form floats render 1.0/100.0 */` |
|    1055 |  592 | `	}else if( ph7_value_is_int(pVal) ){` |
|     519 |  593 | `		sxi64 iVal = ph7_value_to_int64(pVal);` |
|     519 |  594 | `		if( iVal == SMALLEST_INT64 ){` |
|       - |  595 | `			/* php renders LONG_MIN as an expression: the positive literal` |
|       - |  596 | `			 * 9223372036854775808 would not be representable when eval'd. */` |
|       5 |  597 | `			SyBlobAppend(pOut,"-9223372036854775807-1",sizeof("-9223372036854775807-1")-1);` |
|       3 |  598 | `		}else{` |
|     515 |  599 | `			SyBlobFormat(pOut,"%qd",iVal);` |
|       5 |  600 | `		}` |
|     636 |  601 | `	}else if( ph7_value_is_string(pVal) ){` |
|       - |  602 | `		int n;` |
|     207 |  603 | `		const char *z = ph7_value_to_string(pVal,&n);` |
|     207 |  604 | `		VmExportQuoted(pOut,z,n);` |
|     278 |  605 | `	}else if( ph7_value_is_array(pVal) ){` |
|     152 |  606 | `		ph7_hashmap *pMap = (ph7_hashmap *)pVal->x.pOther;` |
|     152 |  607 | `		if( pMap->iFlags & HASHMAP_DUMPING ){` |
|     ! 0 |  608 | `			SyBlobAppend(pOut,"NULL",4); /* circular reference -> NULL, like PHP */` |
|     ! 0 |  609 | `		}else{` |
|       - |  610 | `			VmExportCtx ctx;` |
|     152 |  611 | `			pMap->iFlags \|= HASHMAP_DUMPING;` |
|     152 |  612 | `			SyBlobAppend(pOut,"array (\n",8);` |
|     152 |  613 | `			ctx.pOut = pOut; ctx.nIndent = nIndent; ctx.depth = depth;` |
|     152 |  614 | `			ph7_array_walk(pVal,VmExportArrayWalk,&ctx);` |
|     152 |  615 | `			VmExportIndent(pOut,nIndent);` |
|     152 |  616 | `			SyBlobAppend(pOut,")",1);` |
|     152 |  617 | `			pMap->iFlags &= ~HASHMAP_DUMPING;` |
|       4 |  618 | `		}` |
|     100 |  619 | `	}else if( ph7_value_is_object(pVal) ){` |
|      26 |  620 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|      26 |  621 | `		if( pThis->pClass->iFlags & PH7_CLASS_ENUM ){` |
|       - |  622 | ``			/* php 8.1: an enum case exports as `\S::A` */`` |
|       3 |  623 | `			ph7_value *pName = PH7_EnumCaseNameValue(pThis);` |
|       3 |  624 | `			SyBlobAppend(pOut,"\\",1);` |
|       3 |  625 | `			SyBlobAppend(pOut,pThis->pClass->sName.zString,pThis->pClass->sName.nByte);` |
|       3 |  626 | `			SyBlobAppend(pOut,"::",2);` |
|       3 |  627 | `			if( pName && SyBlobLength(&pName->sBlob) > 0 ){` |
|       3 |  628 | `				SyBlobAppend(pOut,SyBlobData(&pName->sBlob),SyBlobLength(&pName->sBlob));` |
|       2 |  629 | `			}` |
|      25 |  630 | `		}else if( pThis->iFlags & VM_INSTANCE_DUMPING ){` |
|     ! 0 |  631 | `			SyBlobAppend(pOut,"NULL",4); /* circular reference -> NULL, like PHP */` |
|     ! 0 |  632 | `		}else{` |
|      24 |  633 | `			SyString *pClassName = &pThis->pClass->sName;` |
|       - |  634 | `			SyHashEntry *pEntry;` |
|       - |  635 | `			SySet sNames;` |
|       - |  636 | `			SyString *aName;` |
|       - |  637 | `			sxu32 iName,nName;` |
|      24 |  638 | `			pThis->iFlags \|= VM_INSTANCE_DUMPING;` |
|      24 |  639 | `			SyBlobAppend(pOut,"\\",1);` |
|      24 |  640 | `			SyBlobAppend(pOut,pClassName->zString,pClassName->nByte);` |
|      24 |  641 | `			SyBlobAppend(pOut,"::__set_state(array(\n",21);` |
|       - |  642 | `			/* SNAPSHOT the attribute names first: a PHP 8.4 get hook dispatched` |
|       - |  643 | `			 * mid-walk may re-enter an hAttr walk on this instance (the hash has` |
|       - |  644 | `			 * a single embedded loop cursor) or unset()/create properties; names` |
|       - |  645 | `			 * point into class-owned attr storage and each is re-looked-up. */` |
|      24 |  646 | `			SySetInit(&sNames,&pThis->pVm->sAllocator,sizeof(SyString));` |
|      24 |  647 | `			SyHashResetLoopCursor(&pThis->hAttr);` |
|      68 |  648 | `			while( (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|      46 |  649 | `				VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|      46 |  650 | `				if( pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){ continue; }` |
|      44 |  651 | `				if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_VIRTUAL))` |
|      24 |  652 | `				 == PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|       3 |  653 | `					continue; /* virtual set-only property: no value to export (php) */` |
|       - |  654 | `				}` |
|      44 |  655 | `				SySetPut(&sNames,(const void *)&pVmAttr->pAttr->sName);` |
|       2 |  656 | `			}` |
|      24 |  657 | `			aName = (SyString *)SySetBasePtr(&sNames);` |
|      24 |  658 | `			nName = SySetUsed(&sNames);` |
|      66 |  659 | `			for( iName = 0 ; iName < nName ; ++iName ){` |
|      44 |  660 | `				SyString *pAName = &aName[iName];` |
|       - |  661 | `				VmClassAttr *pVmAttr;` |
|       - |  662 | `				ph7_value *pAttrVal;` |
|      44 |  663 | `				pEntry = SyHashGet(&pThis->hAttr,(const void *)pAName->zString,pAName->nByte);` |
|      44 |  664 | `				if( pEntry == 0 ){ continue; } /* unset by an earlier hook */` |
|      44 |  665 | `				pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|      44 |  666 | `				VmExportIndent(pOut,nIndent+3); /* object property lines sit one deeper than arrays */` |
|      44 |  667 | `				VmExportQuoted(pOut,pAName->zString,(int)pAName->nByte);` |
|       - |  668 | `				/* PHP 8.4 property hooks: var_export() reads through the get hook` |
|       - |  669 | `				 * (every visibility — php exports private hooked values too). A` |
|       - |  670 | `				 * throwing hook parks on the boundary rail; the helper's boundary` |
|       - |  671 | `				 * gate keeps LATER hooks from running (the raw values the tail of` |
|       - |  672 | `				 * the export falls back to are discarded when the throw routes). */` |
|       - |  673 | `				{` |
|       - |  674 | `					ph7_value sHookVal;` |
|       - |  675 | `					sxi32 rcHk;` |
|      44 |  676 | `					PH7_MemObjInit(pThis->pVm,&sHookVal);` |
|      44 |  677 | `					rcHk = PH7_VmHookGetAttrValue(pThis,pVmAttr,&sHookVal);` |
|      44 |  678 | `					if( rcHk == SXRET_OK ){` |
|      13 |  679 | `						VmExportEntryValue(pOut,&sHookVal,nIndent,depth);` |
|      13 |  680 | `						PH7_MemObjRelease(&sHookVal);` |
|      13 |  681 | `						continue;` |
|       - |  682 | `					}` |
|      32 |  683 | `					PH7_MemObjRelease(&sHookVal);` |
|      32 |  684 | `					if( rcHk != SXERR_NOTFOUND ){` |
|       - |  685 | `						/* the hook threw (parked on the boundary rail): NULL` |
|       - |  686 | `						 * placeholder keeps the output well-formed */` |
|     ! 0 |  687 | `						SyBlobAppend(pOut," => NULL,\n",10);` |
|     ! 0 |  688 | `						continue;` |
|       - |  689 | `					}` |
|       - |  690 | `				}` |
|      32 |  691 | `				pAttrVal = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|      32 |  692 | `				if( pAttrVal ){ VmExportEntryValue(pOut,pAttrVal,nIndent,depth); }` |
|     ! 0 |  693 | `				else { SyBlobAppend(pOut," => NULL,\n",10); }` |
|      17 |  694 | `			}` |
|      24 |  695 | `			SySetRelease(&sNames);` |
|      24 |  696 | `			VmExportIndent(pOut,nIndent);` |
|      24 |  697 | `			SyBlobAppend(pOut,"))",2);` |
|      24 |  698 | `			pThis->iFlags &= ~VM_INSTANCE_DUMPING;` |
|       - |  699 | `		}` |
|      14 |  700 | `	}else{` |
|       - |  701 | `		/* resource / other -> PHP emits NULL (with a warning we omit) */` |
|     ! 0 |  702 | `		SyBlobAppend(pOut,"NULL",4);` |
|       - |  703 | `	}` |
|     947 |  704 | `}` |
|       - |  705 | `/*` |
|       - |  706 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|       - |  707 | ` *  PHP-exact evaluable representation (see VmExportValue).` |
|       - |  708 | ` */` |
|    1506 |  709 | `PH7_PRIVATE int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  710 | `{` |
|    1511 |  711 | `	int ret_string = 0;` |
|       - |  712 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|    1511 |  713 | `	if( nArg < 1 ){` |
|       - |  714 | `		/* Nothing to output,return FALSE */` |
|     ! 0 |  715 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  716 | `		return SXRET_OK;` |
|       - |  717 | `	}` |
|    1511 |  718 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|    1511 |  719 | `	if ( nArg > 1 ){` |
|       - |  720 | `		/* Where to redirect output */` |
|    1155 |  721 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|     575 |  722 | `	}` |
|       - |  723 | `	/* Generate the PHP-exact evaluable representation */` |
|    1511 |  724 | `	VmExportValue(&sDump,apArg[0],0,0);` |
|    1511 |  725 | `	if( !ret_string ){` |
|       - |  726 | `		/* Output dump */` |
|     361 |  727 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|       - |  728 | `		/* Return NULL */` |
|     361 |  729 | `		ph7_result_null(pCtx);` |
|     183 |  730 | `	}else{` |
|       - |  731 | `		/* Generated dump as return value */` |
|    1155 |  732 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|       - |  733 | `	}` |
|       - |  734 | `	/* Release the working buffer */` |
|    1511 |  735 | `	SyBlobRelease(&sDump);` |
|    1511 |  736 | `	return SXRET_OK;` |
|     758 |  737 | `}` |
|       - |  738 |  |
