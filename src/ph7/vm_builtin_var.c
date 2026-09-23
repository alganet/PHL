/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
/*
 * Section:
 *    Variable-introspection builtins: isset, unset, get_defined_vars,
 *    gettype, get_resource_type, var_dump, print_r and var_export.
 *    Registration rows stay in vm.c's aVmFunc[].
 * Status:
 *    Stable.
 */
/*
 * bool isset($var,...)
 *  Finds out whether a variable is set.
 * Parameters
 *  One or more variable to check.
 * Return
 *  1 if var exists and has value other than NULL, 0 otherwise.
 */
PH7_PRIVATE int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pObj;
	int res = 0;
	int i;
	if( nArg < 1 ){
		/* Missing arguments,return false */
		ph7_result_bool(pCtx,res);
		return SXRET_OK;
	}
	/* Iterate over available arguments */
	for( i = 0 ; i < nArg ; ++i ){
		pObj = apArg[i];
		if( pObj->nIdx == SXU32_HIGH ){
			/* Skip the "expecting a variable" warning for MEMOBJ_BOOL —
			 * synthesized by LOAD_IDX iP2=4 (ArrayAccess::offsetExists) and
			 * by anyone passing a bool literal (rare, harmless). */
			if( (pObj->iFlags & (MEMOBJ_NULL|MEMOBJ_BOOL)) == 0 ){
				/* Not so fatal,Throw a warning */
				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");
			}
		}
		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;
		if( !res ){
			/* Variable not set,return FALSE */
			ph7_result_bool(pCtx,0);
			return SXRET_OK;
		}
	}
	/* All given variable are set,return TRUE */
	ph7_result_bool(pCtx,1);
	return SXRET_OK;
}
/*
 * Unset a memory object [i.e: a ph7_value],remove it from the current
 * frame,the reference table and discard it's contents.
 * This function never fail and always return SXRET_OK.
 */
/*
 * unset($name) for a SIMPLE variable: drop exactly one NAME binding.
 *
 * PH7 routed every unset() through PH7_VmUnsetMemObj(), which releases the shared memory
 * object and then has VmRefObjUnlink() delete EVERY name bound to that slot and unlink
 * EVERY array node pointing at it. For an aliased variable that is data loss, not an
 * unset: `$b = &$a; unset($b);` destroyed $a, `$r = &$arr[$k]; unset($r);` deleted the
 * array element, and `function f(&$p){ unset($p); }` wiped out the caller's variable.
 * php removes the NAME and nothing else; the value survives as long as anything still
 * refers to it.
 *
 * So: unlink this one name, forget it in the slot's reference record, and release the
 * slot only once no name and no array entry still holds it.
 */
PH7_PRIVATE sxi32 VmUnsetVarByName(ph7_vm *pVm,VmFrame *pFrame,const char *zName,sxu32 nByte)
{
	SyHashEntry *pEntry;
	VmRefObj *pRef;
	sxu32 nIdx;
	/* php 8.1 forbids unset($GLOBALS) outright. Checked by NAME: the superglobal is not an
	 * ordinary hVar binding, so the slot-index test below never sees it. */
	if( nByte == sizeof("GLOBALS")-1 && SyMemcmp(zName,"GLOBALS",nByte) == 0 ){
		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,
			"$GLOBALS can only be modified using the $GLOBALS[$name] = $value syntax");
		pVm->iExitStatus = 255;
		pVm->bHaltRequested = 1;
		return PH7_ABORT;
	}
	pEntry = SyHashGet(&pFrame->hVar,(const void *)zName,nByte);
	if( pEntry == 0 ){
		/* No such variable: unset() is a no-op on an undefined name, as in php */
		return SXRET_OK;
	}
	nIdx = SX_PTR_TO_INT(pEntry->pUserData);
	if( nIdx == pVm->nGlobalIdx ){
		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,
			"$GLOBALS can only be modified using the $GLOBALS[$name] = $value syntax");
		pVm->iExitStatus = 255;
		pVm->bHaltRequested = 1;
		return PH7_ABORT;
	}
	pRef = VmRefObjExtract(&(*pVm),nIdx);
	/*
	 * A GLOBAL variable is ALSO an entry of the $GLOBALS array, and that node points at the
	 * same slot. php's unset($x) in global scope drops $GLOBALS['x'] too, so unlink the node
	 * — otherwise it stays a live holder and the value is never released ($o = new D;
	 * unset($o); stopped running the destructor). Clear the reference table's row for the
	 * node BEFORE unlinking it: unlinking frees the node, and the holder count below would
	 * otherwise dereference freed memory.
	 */
	if( pFrame->pParent == 0 ){
		ph7_value *pGlobals = (ph7_value *)SySetAt(&pVm->aMemObj,pVm->nGlobalIdx);
		if( pGlobals && (pGlobals->iFlags & MEMOBJ_HASHMAP) ){
			ph7_hashmap_node *pNode = 0;
			ph7_value sKey;
			SyString sName;
			PH7_MemObjInit(&(*pVm),&sKey);
			SyStringInitFromBuf(&sName,zName,nByte);
			PH7_MemObjInitFromString(&(*pVm),&sKey,&sName);
			if( SXRET_OK == PH7_HashmapLookup((ph7_hashmap *)pGlobals->x.pOther,&sKey,&pNode)
			 && pNode && pNode->nValIdx == nIdx ){
				if( pRef ){
					ph7_hashmap_node **apN = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);
					sxu32 k;
					for( k = 0 ; k < SySetUsed(&pRef->aArrEntries) ; ++k ){
						if( apN[k] == pNode ){
							apN[k] = 0;
						}
					}
				}
				PH7_HashmapUnlinkNode(pNode,FALSE);
			}
			PH7_MemObjRelease(&sKey);
		}
	}

	if( pRef == 0 ){
		/* Unaliased variable: nobody else holds the slot, so the old path is right */
		SyHashDeleteEntry2(pEntry);
		PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);
		/* The slot is back in the free pool; drop its stale local-teardown entry so a
		 * later reuse of the index (e.g. an object property) is not double-freed when
		 * this frame exits. */
		VmDropFrameLocalSlot(&(*pVm),nIdx);
		return SXRET_OK;
	}
	{
		SyHashEntry **apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);
		ph7_hashmap_node **apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);
		sxu32 n, nLive = 0;
		/* Forget THIS name in the slot's reference record (leave the others alone) */
		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; ++n ){
			if( apEntry[n] == pEntry ){
				apEntry[n] = 0;
			}
		}
		SyHashDeleteEntry2(pEntry);
		/* Anything else still holding the slot? */
		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; ++n ){
			if( apEntry[n] ){
				nLive++;
			}
		}
		for( n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){
			/* Only a node that STILL points at this slot is a holder. The reference table
			 * keeps stale rows (a slot index is recycled through the free list, and the row
			 * outlives the node that put it there), so an un-filtered count reports holders
			 * that no longer exist and the value would never be released — the destructor
			 * of `$o = new D; unset($o);` stopped running. */
			if( apNode[n] && apNode[n]->nValIdx == nIdx ){
				nLive++;
			}
		}
		if( nLive < 1 ){
			/* Last holder gone: now the value may go too */
			PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);
			/* Slot returned to the free pool: drop its stale local-teardown entry so a
			 * later reuse of the index is not double-freed on frame exit (see
			 * VmDropFrameLocalSlot). */
			VmDropFrameLocalSlot(&(*pVm),nIdx);
		}
	}
	return SXRET_OK;
}
/*
 * Is this memory slot aliased — i.e. does anything other than its owner refer to it?
 * var_dump marks such an array element with '&' ("&int(2)"). PH7 only flagged nodes that
 * were FOREIGN (`array(&$x)`, where the node points at an outside slot) and so missed the
 * common case, a reference taken TO an element (`$r = &$a[1]`), where the array still owns
 * the value but is no longer its only holder.
 */
PH7_PRIVATE int PH7_VmSlotIsReferenced(ph7_vm *pVm,sxu32 nIdx)
{
	VmRefObj *pRef;
	sxu32 n, nLive = 0;
	SyHashEntry **apEntry;
	if( nIdx == SXU32_HIGH ){
		return 0;
	}
	pRef = VmRefObjExtract(&(*pVm),nIdx);
	if( pRef == 0 ){
		return 0;
	}
	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);
	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; ++n ){
		if( apEntry[n] ){
			nLive++;
		}
	}
	return nLive > 0;
}
PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)
{
	ph7_value *pObj;
	VmRefObj *pRef;
	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);
	if( pObj ){
		/* Release the object */
		PH7_MemObjRelease(pObj);
	}
	/* Remove old reference links */
	pRef = VmRefObjExtract(&(*pVm),nObjIdx);
	if( pRef ){
		sxi32 iFlags = pRef->iFlags;
		/* Unlink from the reference table */
		VmRefObjUnlink(&(*pVm),pRef);
		if( (bForce == TRUE) || (iFlags & VM_REF_IDX_KEEP) == 0 ){
			VmSlot sFree;
			/* Restore to the free list */
			sFree.nIdx = nObjIdx;
			sFree.pUserData = 0;
			SySetPut(&pVm->aFreeObj,(const void *)&sFree);
		}
	}
	return SXRET_OK;
}
/*
 * void unset($var,...)
 *   Unset one or more given variable.
 * Parameters
 *  One or more variable to unset.
 * Return
 *  Nothing.
 */
PH7_PRIVATE int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pObj;
	ph7_vm *pVm;
	int i;
	/* Point to the target VM */
	pVm = pCtx->pVm;
	/* Iterate and unset */
	for( i = 0 ; i < nArg ; ++i ){
		pObj = apArg[i];
		if( pObj->nIdx == SXU32_HIGH ){
			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){
				/* Throw an error */
				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");
			}
		}else{
			sxu32 nIdx = pObj->nIdx;
			if( nIdx == pVm->nGlobalIdx ){
				/* php 8.1: unset($GLOBALS) is forbidden — the same fatal as
				 * re-assigning it (compile-time in php, raised here). */
				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,
					"$GLOBALS can only be modified using the $GLOBALS[$name] = $value syntax");
				pVm->iExitStatus = 255;
				pVm->bHaltRequested = 1;
				return PH7_ABORT;
			}
			PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);
			/* Drop the stale local-teardown entry for the freed slot (see
			 * VmDropFrameLocalSlot) so a later index reuse is not double-freed. */
			VmDropFrameLocalSlot(&(*pVm),nIdx);
		}
	}
	return SXRET_OK;
}
/*
 * Hash walker callback used by the [get_defined_vars()] function.
 */
static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)
{
	ph7_value *pArray = (ph7_value *)pUserData;
	ph7_vm *pVm = pArray->pVm;
	ph7_value *pObj;
	sxu32 nIdx;
	/* php excludes $this from get_defined_vars() (it is bound implicitly, not a
	 * declared local). PHL keeps it in the frame's hVar, so skip it here. */
	if( pEntry->nKeyLen == sizeof("this")-1
	 && SyMemcmp(pEntry->pKey,"this",sizeof("this")-1) == 0 ){
		return SXRET_OK;
	}
	/* Extract the memory object */
	nIdx = SX_PTR_TO_INT(pEntry->pUserData);
	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);
	if( pObj ){
		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 || (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){
			if( pEntry->nKeyLen > 0 ){
				SyString sName;
				ph7_value sKey;
				/* Perform the insertion (pObj may point into pVm->aMemObj; the
				 * inserter snapshots the source before reserving, so the pool may
				 * safely move underneath it — see HashmapInsertIntKey/BlobKey). */
				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);
				PH7_MemObjInitFromString(pVm,&sKey,&sName);
				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);
				PH7_MemObjRelease(&sKey);
			}
		}
	}
	return SXRET_OK;
}
/*
 * array get_defined_vars(void)
 *  Returns an array of all defined variables.
 * Parameter
 *  None
 * Return
 *  An array with all the variables defined in the current scope.
 */
PH7_PRIVATE int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_value *pArray;
	VmFrame *pFrame;
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
 	if( pArray == 0 ){
		SXUNUSED(nArg); /* cc warning */
		SXUNUSED(apArg);
		/* Return NULL */
		ph7_result_null(pCtx);
		return SXRET_OK;
	}
	/* A try/catch block pushes an EXCEPTION frame that holds no variables of its
	 * own, so the enclosing function (or global) frame is the one php reports.
	 * Reading pVm->pFrame raw answered [] for every call inside a try/catch —
	 * including the superglobal test below, which saw a non-NULL pParent and
	 * dropped $argv/$_SERVER at global scope. Every other frame-lookup site
	 * (VmExtractMemObj, compact(), func_get_args()) already skips them. */
	pFrame = VmSkipExceptionFrames(pVm->pFrame);
	/* Superglobals appear in get_defined_vars() ONLY at the global scope (php).
	 * Inside a function the result is the local symbol table alone — a leak of
	 * $argv/$_SERVER/... into every function's scope breaks callers that treat the
	 * keys as real locals (e.g. PHPUnit's doubled-method template reflects each
	 * get_defined_vars() name as a ReflectionParameter). */
	if( pFrame->pParent == 0 ){
		SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);
	}
	/* Then variables defined in the current frame, in DECLARATION order.
	 * The frame table is head-pushed (SyHashInsert), so its forward order is
	 * reverse-insertion; walk it backward to match php, which returns locals in
	 * the order they first appeared (a,b,c — a reassignment reuses the slot and
	 * keeps its original position). */
	SyHashForEachReverse(&pFrame->hVar,VmHashVarWalker,pArray);
	/* Finally,return the created array */
	ph7_result_value(pCtx,pArray);
	return SXRET_OK;
}
/*
 * string get_debug_type(mixed $value)
 *  php 8.0's type name for diagnostics: the SHORT scalar names, and a class
 *  name for an object. Distinct from gettype(), which keeps php 4's long
 *  spellings ("integer"/"boolean"/"NULL") for compatibility.
 *
 *  This was a prelude function in the Reflection chunk, where the TypeError
 *  messages needed it; it is what php ships natively, and the SPL chunk's
 *  messages want it too.
 */
PH7_PRIVATE int vm_builtin_get_debug_type(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zType = "null";
	if( nArg > 0 ){
		ph7_value *pVal = apArg[0];
		if( pVal->iFlags & MEMOBJ_OBJ ){
			ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;
			ph7_result_string(pCtx,SyStringData(&pThis->pClass->sName),
				(int)SyStringLength(&pThis->pClass->sName));
			return SXRET_OK;
		}
		if( pVal->iFlags & MEMOBJ_NULL ){
			zType = "null";
		}else if( pVal->iFlags & MEMOBJ_REAL ){
			/* REAL wins over a cached MEMOBJ_INT, as it does for gettype() */
			zType = "float";
		}else if( pVal->iFlags & MEMOBJ_INT ){
			zType = "int";
		}else if( pVal->iFlags & MEMOBJ_STRING ){
			zType = "string";
		}else if( pVal->iFlags & MEMOBJ_BOOL ){
			zType = "bool";
		}else if( pVal->iFlags & MEMOBJ_HASHMAP ){
			zType = "array";
		}else if( pVal->iFlags & MEMOBJ_RES ){
			/* php names the stream kind here; PHL reports what gettype() does,
			 * which is the same gap gettype() already has (§7.4). */
			zType = PH7_VfsResourceIsClosed(pVal->x.pOther) ? "resource (closed)" : "resource";
		}
	}
	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);
	return SXRET_OK;
}
/*
 * bool gettype($var)
 *  Get the type of a variable
 * Parameters
 *   $var
 *    The variable being type checked.
 * Return
 *   String representation of the given variable type.
 */
PH7_PRIVATE int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	/* php's gettype() uses LONG type names ("integer"/"boolean"/"NULL"), distinct
	 * from get_debug_type()'s short ones ("int"/"bool"/"null") — never route this
	 * through PH7_MemObjTypeDump, which yields the short forms. */
	const char *zType = "unknown type";
	if( nArg > 0 ){
		ph7_value *pVal = apArg[0];
		if( pVal->iFlags & MEMOBJ_NULL ){
			zType = "NULL";
		}else if( pVal->iFlags & MEMOBJ_REAL ){
			/* REAL wins over a cached MEMOBJ_INT: 1.0 is "double" (php). */
			zType = "double";
		}else if( pVal->iFlags & MEMOBJ_INT ){
			zType = "integer";
		}else if( pVal->iFlags & MEMOBJ_STRING ){
			zType = "string";
		}else if( pVal->iFlags & MEMOBJ_BOOL ){
			zType = "boolean";
		}else if( pVal->iFlags & MEMOBJ_HASHMAP ){
			zType = "array";
		}else if( pVal->iFlags & MEMOBJ_OBJ ){
			zType = "object";
		}else if( pVal->iFlags & MEMOBJ_RES ){
			/* php reports an fclose()'d handle as "resource (closed)" */
			zType = PH7_VfsResourceIsClosed(pVal->x.pOther) ? "resource (closed)" : "resource";
		}
	}
	/* Return the variable type */
	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);
	return SXRET_OK;
}
/*
 * bool settype(mixed &$var, string $type)
 *  Convert $var IN PLACE to the type named by $type, mirroring the (type) cast
 *  operators exactly (same PH7_MemObjTo* helpers CVT_INT/REAL/STR/BOOL/ARRAY/OBJ
 *  use), and write the result back through the by-ref out-param.
 * Parameters
 *   &$var : the variable to convert (compile.c's GenStateByRefBuiltinMask marks
 *           position 0 by-reference so an undefined variable is auto-vivified).
 *   $type : "int"/"integer", "float"/"double", "string", "bool"/"boolean",
 *           "array", "object", "null" (case-insensitive).
 * Return
 *   Always true on success; a non-referenceable $var is an Error, an unknown
 *   $type (or the un-castable "resource") is a ValueError — php-exact.
 */
PH7_PRIVATE int vm_builtin_settype(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zType;
	int nLen;
	ph7_value *pNew;
	SXUNUSED(nArg); /* arity (exactly 2) enforced by the central arity table */
	/* php binds $var by reference at the CALL, and the refusal is the call site's
	 * to raise (PH7_VmScreenByRefArgShapes) — only the compiler can tell a literal,
	 * which php refuses, from the result of a call, which php accepts with a notice
	 * and converts in place on the temporary. The `nIdx == SXU32_HIGH` test that
	 * used to sit here conflated the two. */
	zType = ph7_value_to_string(apArg[1],&nLen);
	/* Validate the target type up-front (php checks $type before touching $var):
	 * "resource" is a distinct ValueError, any other unknown name is the generic
	 * invalid-type ValueError. */
	if( nLen == 8 && SyStrnicmp(zType,"resource",8) == 0 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"Cannot convert to resource type");
	}
	if( !(  (nLen == 3 && SyStrnicmp(zType,"int",3) == 0)
	     || (nLen == 7 && SyStrnicmp(zType,"integer",7) == 0)
	     || (nLen == 5 && SyStrnicmp(zType,"float",5) == 0)
	     || (nLen == 6 && SyStrnicmp(zType,"double",6) == 0)
	     || (nLen == 6 && SyStrnicmp(zType,"string",6) == 0)
	     || (nLen == 4 && SyStrnicmp(zType,"bool",4) == 0)
	     || (nLen == 7 && SyStrnicmp(zType,"boolean",7) == 0)
	     || (nLen == 5 && SyStrnicmp(zType,"array",5) == 0)
	     || (nLen == 6 && SyStrnicmp(zType,"object",6) == 0)
	     || (nLen == 4 && SyStrnicmp(zType,"null",4) == 0) ) ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"settype(): Argument #2 ($type) must be a valid type");
	}
	/* Convert a COPY of the current value (the To* helpers mutate in place), then
	 * store it through the by-ref out-param — settype changes $var's TYPE, so the
	 * new value must land in the caller slot (nIdx), not just in shared contents. */
	pNew = ph7_context_new_scalar(pCtx);
	if( pNew == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	PH7_MemObjStore(apArg[0],pNew);
	if( (nLen == 3 && SyStrnicmp(zType,"int",3) == 0)
	 || (nLen == 7 && SyStrnicmp(zType,"integer",7) == 0) ){
		PH7_MemObjToInteger(pNew);
		MemObjSetType(pNew,MEMOBJ_INT);
	}else if( (nLen == 5 && SyStrnicmp(zType,"float",5) == 0)
	       || (nLen == 6 && SyStrnicmp(zType,"double",6) == 0) ){
		PH7_MemObjToReal(pNew);
		MemObjSetType(pNew,MEMOBJ_REAL);
	}else if( nLen == 6 && SyStrnicmp(zType,"string",6) == 0 ){
		/* php emits "Array to string conversion" for settype($arr,'string'), and
		 * throws "Object of class X could not be converted to string" for an
		 * object with no __toString() (or propagates one that threw). php's
		 * convert_to_string() has already blanked the zval by then, so a CAUGHT
		 * settype() leaves $var === "" — store that, then propagate instead of
		 * answering true. */
		sxi32 rcSv = PH7_MemObjToStringUV(pNew);
		if( rcSv != SXRET_OK ){
			PH7_MemObjRelease(pNew);
			MemObjSetType(pNew,MEMOBJ_STRING);
			PH7_VmStoreArgByRef(pCtx->pVm,apArg[0],pNew);
			pCtx->nThrowRc = rcSv;
			return rcSv;
		}
	}else if( (nLen == 4 && SyStrnicmp(zType,"bool",4) == 0)
	       || (nLen == 7 && SyStrnicmp(zType,"boolean",7) == 0) ){
		PH7_MemObjToBool(pNew);
	}else if( nLen == 5 && SyStrnicmp(zType,"array",5) == 0 ){
		PH7_MemObjToHashmap(pNew);
	}else if( nLen == 6 && SyStrnicmp(zType,"object",6) == 0 ){
		PH7_MemObjToObject(pNew);
	}else{
		/* "null" — the only validated name left */
		PH7_MemObjToNull(pNew);
	}
	PH7_VmStoreArgByRef(pCtx->pVm,apArg[0],pNew);
	ph7_result_bool(pCtx,1);
	return SXRET_OK;
}
/*
 * string get_resource_type(resource $handle)
 *  This function gets the type of the given resource.
 * Parameters
 *  $handle
 *  The evaluated resource handle.
 * Return
 *  If the given handle is a resource, this function will return a string
 *  representing its type. If the type is not identified by this function
 *  the return value will be the string Unknown.
 *  This function will return FALSE and generate an error if handle
 *  is not a resource.
 */
PH7_PRIVATE int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zType;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE*/
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* PHP returns the resource TYPE name (e.g. "stream" for IO handles), not an id. */
	zType = PH7_VfsResourceType(apArg[0]->x.pOther);
	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);
	return SXRET_OK;
}
/*
 * int get_resource_id(resource $resource)
 *  Returns the integer id of a resource — the same number (int) casts to and
 *  "Resource id #N" renders (php 8.0).
 */
PH7_PRIVATE int vm_builtin_get_resource_id(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		return PH7_VmThrowException(pCtx,"TypeError",
			"get_resource_id(): Argument #1 ($resource) must be of type resource, %s given",
			nArg < 1 ? "none" : ph7_type_name(apArg[0]));
	}
	ph7_result_int64(pCtx,(ph7_int64)PH7_VmResourceId(pCtx->pVm,apArg[0]->x.pOther));
	return SXRET_OK;
}
/*
 * void var_dump(expression,....)
 *   var_dump � Dumps information about a variable
 * Parameters
 *   One or more expression to dump.
 * Returns
 *  Nothing.
 */
PH7_PRIVATE int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SyBlob sDump; /* Generated dump is stored here */
	int i;
	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);
	/* Dump one or more expressions */
	for( i = 0 ; i < nArg ; i++ ){
		ph7_value *pObj = apArg[i];
		/* Reset the working buffer */
		SyBlobReset(&sDump);
		/* Dump the given expression */
		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);
		/* Output */
		if( SyBlobLength(&sDump) > 0 ){
			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));
		}
	}
	/* Release the working buffer */
	SyBlobRelease(&sDump);
	return SXRET_OK;
}
/*
 * string/bool print_r(expression,[bool $return = FALSE])
 *   print-r - Prints human-readable information about a variable
 * Parameters
 *   expression: Expression to dump
 *   return : If you would like to capture the output of print_r() use
 *            the return parameter. When this parameter is set to TRUE
 *            print_r() will return the information rather than print it.
 * Return
 *  When the return parameter is TRUE, this function will return a string.
 *  Otherwise, the return value is TRUE.
 */
PH7_PRIVATE int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int ret_string = 0;
	SyBlob sDump;
	if( nArg < 1 ){
		/* Nothing to output,return FALSE */
		ph7_result_bool(pCtx,0);
		return SXRET_OK;
	}
	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);
	if ( nArg > 1 ){
		/* Where to redirect output */
		ret_string = ph7_value_to_bool(apArg[1]);
	}
	/* Generate dump */
	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);
	if( !ret_string ){
		/* Output dump */
		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));
		/* Return true */
		ph7_result_bool(pCtx,1);
	}else{
		/* Generated dump as return value */
		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));
	}
	/* Release the working buffer */
	SyBlobRelease(&sDump);
	return SXRET_OK;
}
/*
 * var_export() — PHP-exact evaluable representation of a value.
 *
 * Distinct from print_r/var_dump (which share PH7_MemObjDump): var_export emits
 * parseable PHP — 'single-quoted' strings, true/false/NULL, array (\n  k => v,\n),
 * and \Class::__set_state(array(...)) for objects. PHP's indentation is quirky
 * (2-space array entries; 3-space object property lines; a composite value always
 * opens at containerIndent+2) but deterministic; this matches it byte-for-byte.
 */
/* Instance flag (distinct from oo.c's CLASS_INSTANCE_DESTROYED 0x001) marking an
 * object currently on the var_export recursion stack, for cycle detection. */
#define VM_INSTANCE_DUMPING 0x002
typedef struct VmExportCtx VmExportCtx;
struct VmExportCtx
{
	SyBlob *pOut;
	int nIndent;  /* indentation of the container emitting this entry */
	int depth;    /* recursion guard */
};
static void VmExportValue(SyBlob *pOut, ph7_value *pVal, int nIndent, int depth);
/* Append nIndent spaces. */
static void VmExportIndent(SyBlob *pOut, int nIndent)
{
	int i;
	for( i = 0; i < nIndent; i++ ){ SyBlobAppend(pOut," ",1); }
}
/* Append a single-quoted PHP string literal (escapes ' and \, batching safe runs
 * in one append). A NUL byte can't live in a single-quoted literal, so PHP splits
 * it out as ' . "\0" . ' — match that. */
static void VmExportQuoted(SyBlob *pOut, const char *z, int n)
{
	int i, run = 0;
	SyBlobAppend(pOut,"'",1);
	for( i = 0; i < n; i++ ){
		char c = z[i];
		if( c != '\0' && c != '\'' && c != '\\' ){ continue; } /* extend the safe run */
		if( i > run ){ SyBlobAppend(pOut,&z[run],(sxu32)(i-run)); }
		if( c == '\0' ){ SyBlobAppend(pOut,"' . \"\\0\" . '",12); }
		else { SyBlobAppend(pOut,"\\",1); SyBlobAppend(pOut,&z[i],1); }
		run = i+1;
	}
	if( n > run ){ SyBlobAppend(pOut,&z[run],(sxu32)(n-run)); }
	SyBlobAppend(pOut,"'",1);
}
/* True if the array/object is already on the var_export recursion stack. */
static int VmExportIsCycle(ph7_value *pVal)
{
	if( ph7_value_is_array(pVal) ){
		return (((ph7_hashmap *)pVal->x.pOther)->iFlags & HASHMAP_DUMPING) != 0;
	}
	if( ph7_value_is_object(pVal) ){
		return (((ph7_class_instance *)pVal->x.pOther)->iFlags & VM_INSTANCE_DUMPING) != 0;
	}
	return 0;
}
/* Emit " => " then the value: a scalar inline, a composite on its own line at
 * containerIndent+2. A circular reference renders inline as NULL (like PHP). */
static void VmExportEntryValue(SyBlob *pOut, ph7_value *pVal, int nContainerIndent, int depth)
{
	SyBlobAppend(pOut," => ",4);
	if( VmExportIsCycle(pVal) ){
		SyBlobAppend(pOut,"NULL",4);
	}else if( ph7_value_is_array(pVal) || ph7_value_is_object(pVal) ){
		SyBlobAppend(pOut,"\n",1);
		VmExportIndent(pOut,nContainerIndent+2);
		VmExportValue(pOut,pVal,nContainerIndent+2,depth+1);
	}else{
		VmExportValue(pOut,pVal,nContainerIndent+2,depth+1);
	}
	SyBlobAppend(pOut,",\n",2);
}
/* Array walker: "<indent+2>key => value,\n" for each entry. */
static int VmExportArrayWalk(ph7_value *pKey, ph7_value *pValue, void *pUserData)
{
	VmExportCtx *pC = (VmExportCtx *)pUserData;
	VmExportIndent(pC->pOut,pC->nIndent+2);
	if( ph7_value_is_string(pKey) ){
		int n;
		const char *z = ph7_value_to_string(pKey,&n);
		VmExportQuoted(pC->pOut,z,n);
	}else{
		SyBlobFormat(pC->pOut,"%qd",ph7_value_to_int64(pKey));
	}
	VmExportEntryValue(pC->pOut,pValue,pC->nIndent,pC->depth);
	return PH7_OK;
}
static void VmExportValue(SyBlob *pOut, ph7_value *pVal, int nIndent, int depth)
{
	if( depth > 4096 ){ return; } /* backstop for pathological finite nesting */
	if( ph7_value_is_null(pVal) ){
		SyBlobAppend(pOut,"NULL",4);
	}else if( ph7_value_is_bool(pVal) ){
		if( ph7_value_to_bool(pVal) ){ SyBlobAppend(pOut,"true",4); }
		else { SyBlobAppend(pOut,"false",5); }
	}else if( ph7_value_is_float(pVal) ){
		/* float before int: ph7_value_is_int is lenient (true for integer reals). */
		sxu32 before = SyBlobLength(pOut), i, after;
		const char *z;
		int plain = 1;
		PH7_AppendShortestReal(pOut,ph7_value_to_double(pVal));
		z = (const char *)SyBlobData(pOut);
		after = SyBlobLength(pOut);
		for( i = before; i < after; i++ ){
			if( !((z[i]>='0'&&z[i]<='9')||z[i]=='-') ){ plain = 0; break; }
		}
		if( plain ){ SyBlobAppend(pOut,".0",2); } /* integer-form floats render 1.0/100.0 */
	}else if( ph7_value_is_int(pVal) ){
		sxi64 iVal = ph7_value_to_int64(pVal);
		if( iVal == SMALLEST_INT64 ){
			/* php renders LONG_MIN as an expression: the positive literal
			 * 9223372036854775808 would not be representable when eval'd. */
			SyBlobAppend(pOut,"-9223372036854775807-1",sizeof("-9223372036854775807-1")-1);
		}else{
			SyBlobFormat(pOut,"%qd",iVal);
		}
	}else if( ph7_value_is_string(pVal) ){
		int n;
		const char *z = ph7_value_to_string(pVal,&n);
		VmExportQuoted(pOut,z,n);
	}else if( ph7_value_is_array(pVal) ){
		ph7_hashmap *pMap = (ph7_hashmap *)pVal->x.pOther;
		if( pMap->iFlags & HASHMAP_DUMPING ){
			SyBlobAppend(pOut,"NULL",4); /* circular reference -> NULL, like PHP */
		}else{
			VmExportCtx ctx;
			pMap->iFlags |= HASHMAP_DUMPING;
			SyBlobAppend(pOut,"array (\n",8);
			ctx.pOut = pOut; ctx.nIndent = nIndent; ctx.depth = depth;
			ph7_array_walk(pVal,VmExportArrayWalk,&ctx);
			VmExportIndent(pOut,nIndent);
			SyBlobAppend(pOut,")",1);
			pMap->iFlags &= ~HASHMAP_DUMPING;
		}
	}else if( ph7_value_is_object(pVal) ){
		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;
		if( pThis->pClass->iFlags & PH7_CLASS_ENUM ){
			/* php 8.1: an enum case exports as `\S::A` */
			ph7_value *pName = PH7_EnumCaseNameValue(pThis);
			SyBlobAppend(pOut,"\\",1);
			SyBlobAppend(pOut,pThis->pClass->sName.zString,pThis->pClass->sName.nByte);
			SyBlobAppend(pOut,"::",2);
			if( pName && SyBlobLength(&pName->sBlob) > 0 ){
				SyBlobAppend(pOut,SyBlobData(&pName->sBlob),SyBlobLength(&pName->sBlob));
			}
		}else if( pThis->iFlags & VM_INSTANCE_DUMPING ){
			SyBlobAppend(pOut,"NULL",4); /* circular reference -> NULL, like PHP */
		}else{
			SyString *pClassName = &pThis->pClass->sName;
			SyHashEntry *pEntry;
			SySet sNames;
			SyString *aName;
			sxu32 iName,nName;
			pThis->iFlags |= VM_INSTANCE_DUMPING;
			SyBlobAppend(pOut,"\\",1);
			SyBlobAppend(pOut,pClassName->zString,pClassName->nByte);
			SyBlobAppend(pOut,"::__set_state(array(\n",21);
			{
				/* A native class's PRESENTATION (php's get_properties): var_export
				 * shows a DateTime as date/timezone_type/timezone, the same shape
				 * the (array) cast produces and NOT the hidden engine slots. A
				 * debug-only hook (WeakReference) fills nothing, which is php's
				 * empty export. */
				ph7_value sPresent;
				ph7_hashmap *pPresent = PH7_NewHashmap(pThis->pVm,0,0);
				PH7_MemObjInit(pThis->pVm,&sPresent);
				if( pPresent ){
					sPresent.x.pOther = pPresent;
					MemObjSetType(&sPresent,MEMOBJ_HASHMAP);
					if( PH7_ClassInstancePresent(pThis,&sPresent,0) ){
						/* Same line shape the attribute loop below produces: an
						 * object body's entries sit one deeper than an array's. */
						VmExportCtx sCtx;
						sCtx.pOut = pOut;
						sCtx.nIndent = nIndent + 1;
						sCtx.depth = depth;
						ph7_array_walk(&sPresent,VmExportArrayWalk,&sCtx);
						PH7_MemObjRelease(&sPresent);
						VmExportIndent(pOut,nIndent);
						SyBlobAppend(pOut,"))",sizeof("))")-1);
						pThis->iFlags &= ~VM_INSTANCE_DUMPING;
						return;
					}
					PH7_MemObjRelease(&sPresent);
				}
			}
			/* SNAPSHOT the attribute names first: a PHP 8.4 get hook dispatched
			 * mid-walk may re-enter an hAttr walk on this instance (the hash has
			 * a single embedded loop cursor) or unset()/create properties; names
			 * point into class-owned attr storage and each is re-looked-up. */
			SySetInit(&sNames,&pThis->pVm->sAllocator,sizeof(SyString));
			SyHashResetLoopCursor(&pThis->hAttr);
			while( (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){
				VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;
				if( pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC|PH7_CLASS_ATTR_CONSTANT|PH7_CLASS_ATTR_HIDDEN) ){ continue; }
				if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET|PH7_CLASS_ATTR_HOOK_VIRTUAL))
				 == PH7_CLASS_ATTR_HOOK_VIRTUAL ){
					continue; /* virtual set-only property: no value to export (php) */
				}
				SySetPut(&sNames,(const void *)&pVmAttr->pAttr->sName);
			}
			aName = (SyString *)SySetBasePtr(&sNames);
			nName = SySetUsed(&sNames);
			for( iName = 0 ; iName < nName ; ++iName ){
				SyString *pAName = &aName[iName];
				VmClassAttr *pVmAttr;
				ph7_value *pAttrVal;
				pEntry = SyHashGet(&pThis->hAttr,(const void *)pAName->zString,pAName->nByte);
				if( pEntry == 0 ){ continue; } /* unset by an earlier hook */
				pVmAttr = (VmClassAttr *)pEntry->pUserData;
				VmExportIndent(pOut,nIndent+3); /* object property lines sit one deeper than arrays */
				VmExportQuoted(pOut,pAName->zString,(int)pAName->nByte);
				/* PHP 8.4 property hooks: var_export() reads through the get hook
				 * (every visibility — php exports private hooked values too). A
				 * throwing hook parks on the boundary rail; the helper's boundary
				 * gate keeps LATER hooks from running (the raw values the tail of
				 * the export falls back to are discarded when the throw routes). */
				{
					ph7_value sHookVal;
					sxi32 rcHk;
					PH7_MemObjInit(pThis->pVm,&sHookVal);
					rcHk = PH7_VmHookGetAttrValue(pThis,pVmAttr,&sHookVal);
					if( rcHk == SXRET_OK ){
						VmExportEntryValue(pOut,&sHookVal,nIndent,depth);
						PH7_MemObjRelease(&sHookVal);
						continue;
					}
					PH7_MemObjRelease(&sHookVal);
					if( rcHk != SXERR_NOTFOUND ){
						/* the hook threw (parked on the boundary rail): NULL
						 * placeholder keeps the output well-formed */
						SyBlobAppend(pOut," => NULL,\n",10);
						continue;
					}
				}
				pAttrVal = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);
				if( pAttrVal ){ VmExportEntryValue(pOut,pAttrVal,nIndent,depth); }
				else { SyBlobAppend(pOut," => NULL,\n",10); }
			}
			SySetRelease(&sNames);
			VmExportIndent(pOut,nIndent);
			SyBlobAppend(pOut,"))",2);
			pThis->iFlags &= ~VM_INSTANCE_DUMPING;
		}
	}else{
		/* resource / other -> PHP emits NULL (with a warning we omit) */
		SyBlobAppend(pOut,"NULL",4);
	}
}
/*
 * string/null var_export(expression,[bool $return = FALSE])
 *  PHP-exact evaluable representation (see VmExportValue).
 */
PH7_PRIVATE int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int ret_string = 0;
	SyBlob sDump;      /* Dump is stored in this BLOB */
	if( nArg < 1 ){
		/* Nothing to output,return FALSE */
		ph7_result_bool(pCtx,0);
		return SXRET_OK;
	}
	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);
	if ( nArg > 1 ){
		/* Where to redirect output */
		ret_string = ph7_value_to_bool(apArg[1]);
	}
	/* Generate the PHP-exact evaluable representation */
	VmExportValue(&sDump,apArg[0],0,0);
	if( !ret_string ){
		/* Output dump */
		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));
		/* Return NULL */
		ph7_result_null(pCtx);
	}else{
		/* Generated dump as return value */
		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));
	}
	/* Release the working buffer */
	SyBlobRelease(&sDump);
	return SXRET_OK;
}
