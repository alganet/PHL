/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
/*
 * Section:
 *    Opcode handlers extracted from vm.c's dispatch loop. Each handler runs
 *    one opcode arm against the caller's VmExecState: the loop syncs pTos/pc
 *    in, calls the handler, reloads them and routes the returned VmOpRc onto
 *    its labels (same idiom as VmCallFinish).
 * Status:
 *    Stable.
 */
/* Bind the dispatch-routing exits to handler semantics (see vm_dispatch.h),
 * and map the macros' sState references onto our state parameter. */
#define VM_EXIT_BREAK      { pState->pTos = pTos; pState->pc = pc; return VM_OP_NEXT; }
#define VM_EXIT_ABORT      { pState->pTos = pTos; pState->pc = pc; return VM_OP_ABORT; }
#define VM_EXIT_EXCEPTION  { pState->pTos = pTos; pState->pc = pc; return VM_OP_EXCEPTION; }
#include "vm_dispatch.h"
#define sState (*pState)

/*
 * OP_STORE_REF: body moved verbatim from the OP_STORE_REF arm of
 * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.
 */
PH7_PRIVATE VmOpRc VmExecOpStoreRef(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)
{
	ph7_value *pTos = pState->pTos;
	ph7_value *pStack = pState->pStack;
	VmInstr *aInstr = pState->aInstr;
	sxi32 pc = pState->pc;
	sxi32 rc;
	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);
	 SyString sName = { 0 , 0 };
	 VmFrame *pFrameLocal;
	SyHashEntry *pEntry;
	sxu32 nIdx;
#ifdef UNTRUST
	if( pTos < pStack ){
		VM_EXIT_ABORT;
	}
#endif
	if( pInstr->iP2 == 1 ){
		/* Member reference target: `$o->p =& $x` / `self::$s =& $x`. The
		 * preceding OP_MEMBER (PH7_MEMBER_REF_TARGET) resolved the property slot
		 * and stashed it. Stack: [ ... , source, member-result(top) ]. Rebind the
		 * property's nIdx to alias the source variable's slot and pin that slot
		 * past its owning frame (like a use(&$x) capture) so neither frame
		 * teardown nor a later unset recycles it while the property aliases it. */
		ph7_value *pSrc = &pTos[-1];
		sxu32 nSrcIdx = pSrc->nIdx;
		VmClassAttr *pVmAttr = pVm->pRefTargetAttr;
		ph7_class_attr *pStAttr = pVm->pRefTargetStaticAttr;
		if( pSrc->iFlags & MEMOBJ_AUX_STROFFSET ){
			/* `$o->p =& $s[1]`: a string offset is not a slot (its index is the
			 * BASE STRING's), and php refuses the reference outright. Settle the
			 * stashed target state exactly as the success path does, then throw. */
			if( pVm->pRefTargetThis ){
				PH7_ClassInstanceUnref(pVm->pRefTargetThis);
			}
			pVm->pRefTargetAttr = 0;
			pVm->pRefTargetStaticAttr = 0;
			pVm->pRefTargetThis = 0;
			VmPopOperand(&pTos,1); /* the member result */
			rc = VmThrowFromVm(&(*pVm),"Error","Cannot create references to/from string offsets",
				sizeof("Cannot create references to/from string offsets")-1);
			if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }
			PH7_THROW_ROUTE_MIDEXPR(rc)
		}
		if( nSrcIdx == SXU32_HIGH ){
			/* php: the RHS of `=&` must be a variable, not a constant expression.
			 * (The compiler already rejects the obvious literal forms.) */
			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,
				"Reference operator require a variable not a constant as it's right operand");
		}else if( pVmAttr ){
			sxu32 nOldIdx = pVmAttr->nIdx;
			if( nOldIdx != nSrcIdx ){
				if( (pVmAttr->iState & VM_CLASS_ATTR_REFBOUND) == 0 ){
					/* Release this property's own (unshared) slot before repointing.
					 * A reference-bound property bypasses typed coercion in php, so
					 * drop any typed-slot enforcement entry too. */
					if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){
						SyHashDeleteEntry(&pVm->hTypedSlot,(const void *)&nOldIdx,sizeof(sxu32),0);
					}
					PH7_VmUnsetMemObj(&(*pVm),nOldIdx,TRUE);
				}else{
					/* Already bound elsewhere: give that slot its pin back, which
					 * releases it when this property was its last holder. */
					VmUnpinMemObjSlot(&(*pVm),nOldIdx);
				}
				pVmAttr->nIdx = nSrcIdx;
				pVmAttr->iState |= VM_CLASS_ATTR_REFBOUND;
				pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;
				VmPinMemObjSlotCounted(&(*pVm),nSrcIdx);
			}
		}else if( pStAttr ){
			sxu32 nOldIdx = pStAttr->nIdx;
			if( nOldIdx != nSrcIdx ){
				/* Give the previous target back, exactly as the instance arm above does.
				 * A permanent pin was left on every slot the property had ever named, so
				 * each of them stayed a REFERENCE for the rest of the script — which an
				 * ordinary array COPY then shared (`$d = $a; $d[0] = 99;` wrote through
				 * to `$a[0]`, silently), since "is this element a reference" is answered
				 * by who still holds it. */
				if( (pStAttr->iFlags & PH7_CLASS_ATTR_REFBOUND) == 0 ){
					/* The static's own (unshared) slot. A reference-bound property bypasses
					 * typed coercion in php, so drop any typed-slot enforcement entry too. */
					if( pStAttr->iFlags & PH7_CLASS_ATTR_TYPED ){
						SyHashDeleteEntry(&pVm->hTypedSlot,(const void *)&nOldIdx,sizeof(sxu32),0);
					}
					PH7_VmUnsetMemObj(&(*pVm),nOldIdx,TRUE);
				}else{
					VmUnpinMemObjSlot(&(*pVm),nOldIdx);
				}
				pStAttr->nIdx = nSrcIdx;
				pStAttr->iFlags |= PH7_CLASS_ATTR_REFBOUND;
				VmPinMemObjSlotCounted(&(*pVm),nSrcIdx);
			}
		}
		if( pVm->pRefTargetThis ){
			PH7_ClassInstanceUnref(pVm->pRefTargetThis);
		}
		pVm->pRefTargetAttr = 0;
		pVm->pRefTargetStaticAttr = 0;
		pVm->pRefTargetThis = 0;
		/* Pop the member-result; leave the source as the expression value. */
		VmPopOperand(&pTos,1);
		VM_EXIT_BREAK;
	}
	if( pInstr->p3 == 0 ){
		char *zName;
		/* Take the variable name from the Next on the stack */
		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){
			/* Force a string cast */
			PH7_MemObjToString(pTos);
		}
		if( SyBlobLength(&pTos->sBlob) > 0 ){
			zName = SyMemBackendStrDup(&pVm->sAllocator,
				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));
			if( zName ){
				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));
			}
		}
		PH7_MemObjRelease(pTos);
		pTos--;
	}else{
		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));
	}
	if( pTos->iFlags & MEMOBJ_AUX_STROFFSET ){
		/* php: a string OFFSET cannot be either end of a reference. The value read
		 * out of a string still carries the BASE VARIABLE's slot, so binding it
		 * aliased the whole string and a later write through the reference REPLACED
		 * it ($s = "abc"; $r = &$s[1]; $r = "Z"; left $s === "Z"). Same Error the
		 * by-ref ARGUMENT path raises for f($s[1]). */
		rc = VmThrowFromVm(&(*pVm),"Error","Cannot create references to/from string offsets",
			sizeof("Cannot create references to/from string offsets")-1);
		PH7_MemObjRelease(pTos);
		MemObjSetType(pTos,MEMOBJ_NULL);
		pTos->nIdx = SXU32_HIGH;
		if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }
		PH7_THROW_ROUTE_MIDEXPR(rc)
	}
	nIdx = pTos->nIdx;
	if(nIdx == SXU32_HIGH ){
		if( (pTos->iFlags & (MEMOBJ_OBJ|MEMOBJ_HASHMAP|MEMOBJ_RES)) == 0 ){
			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,
				"Reference operator require a variable not a constant as it's right operand");
		}else{
			ph7_value *pObj;
			/* Extract the desired variable and if not available dynamically create it */
			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);
			if( pObj == 0 ){
				VmErrorFormat(&(*pVm),PH7_CTX_ERR,
					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);
				VM_EXIT_ABORT;
			}
			/* Perform the store operation */
			PH7_MemObjStore(pTos,pObj);
			pTos->nIdx = pObj->nIdx;
		}
	}else if( sName.nByte > 0){
		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){
			/* php 8.1's non-catchable fatal (compile-time there) */
			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot acquire reference to $GLOBALS");
			pVm->iExitStatus = 255;
			pVm->bHaltRequested = 1;
			VM_EXIT_ABORT;
		}else{
			pFrameLocal = pVm->pFrame;
			pFrameLocal = VmSkipExceptionFrames(pFrameLocal);
			/* Query the local frame */
			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);
			if( pEntry ){
				/* php RE-BINDS a name that already exists (`$y = 2; $y = &$x;`, and the
				 * `$r = &$a[$k]` idiom from the second loop step on) — the old binding
				 * goes, its value with it if nothing else holds it. */
				PH7_VmRebindVarSlot(&(*pVm),pFrameLocal,pEntry,sName.zString,sName.nByte,nIdx);
				if( pInstr->p3 == 0 && sName.zString ){
					/* The name was duplicated for a symbol-table key this rebind does
					 * not need — the entry keeps the key it was created with. */
					SyMemBackendFree(&pVm->sAllocator,(void *)sName.zString);
				}
			}else{
				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));
				if( pFrameLocal->pParent == 0 ){
					/* Insert in the $GLOBALS array */
					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);
				}
				if( rc == SXRET_OK ){
					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);
				}
			}
		}
	}
	VM_EXIT_BREAK;
	VM_EXIT_BREAK;
}

/* LOAD_IDX's own iP2 context codes (they do NOT line up with PH7_MEMBER_*). */
#define VM_IDX_CTX_ISSET 4
#define VM_IDX_CTX_UNSET 5
/* An INTERMEDIATE subscript of an unset chain (`unset($a['k']['n'])`): every unset
 * rule below applies to it — COW-separate the parent, never vivify a missing key,
 * unset's own wording for a bad base — except the removal itself, which belongs to
 * the OUTERMOST subscript alone. */
#define VM_IDX_CTX_UNSET_BASE 10
#define VM_IDX_IS_UNSET(iP2) ((iP2) == VM_IDX_CTX_UNSET || (iP2) == VM_IDX_CTX_UNSET_BASE)
#define VM_IDX_CTX_EMPTY 6
/*
 * php rejects an OBJECT or an ARRAY used as an array offset, naming the offending
 * type the way get_debug_type() does — the CLASS name for an object, "array" for
 * an array — and wording the failure by context:
 *
 *   read/write   Cannot access offset of type Foo on array
 *   isset/empty  Cannot access offset of type Foo in isset or empty
 *   unset        Cannot unset offset of type Foo on array
 *
 * A RESOURCE is deliberately absent: php does not reject it, it warns
 * ("Resource ID#N used as offset, casting to integer (N)") and uses the id as an
 * integer key. VmOffsetResourceWarn() below handles that half.
 *
 * Returns TRUE and fills pMsg when the key must be rejected. iCtx is the
 * instruction's iP2 (any value other than the isset/unset/empty codes reads as
 * an access).
 */
static int VmOffsetTypeRejected(ph7_vm *pVm,ph7_value *pKey,int iCtx,SyBlob *pMsg)
{
	const char *zType;
	SyString *pClass = 0;
	if( pKey == 0 ){
		return FALSE;
	}
	if( pKey->iFlags & MEMOBJ_OBJ ){
		ph7_class_instance *pInst = (ph7_class_instance *)pKey->x.pOther;
		if( pInst && pInst->pClass ){
			pClass = &pInst->pClass->sName;
		}
		zType = "object";
	}else if( pKey->iFlags & MEMOBJ_HASHMAP ){
		zType = "array";
	}else{
		return FALSE;
	}
	SyBlobInit(pMsg,&pVm->sAllocator);
	if( VM_IDX_IS_UNSET(iCtx) ){
		SyBlobAppend(pMsg,"Cannot unset offset of type ",sizeof("Cannot unset offset of type ")-1);
	}else{
		SyBlobAppend(pMsg,"Cannot access offset of type ",sizeof("Cannot access offset of type ")-1);
	}
	if( pClass ){
		SyBlobAppend(pMsg,pClass->zString,pClass->nByte);
	}else{
		SyBlobAppend(pMsg,zType,(sxu32)SyStrlen(zType));
	}
	if( iCtx == VM_IDX_CTX_ISSET || iCtx == VM_IDX_CTX_EMPTY ){
		SyBlobAppend(pMsg," in isset or empty",sizeof(" in isset or empty")-1);
	}else{
		SyBlobAppend(pMsg," on array",sizeof(" on array")-1);
	}
	return TRUE;
}
/*
 * php's other half of the offset-type rules: a RESOURCE offset is accepted, with
 * `Warning: Resource ID#N used as offset, casting to integer (N)`, and the key
 * becomes that integer. Rewrites pKey in place so the normal integer-key path
 * takes over.
 */
static void VmOffsetResourceWarn(ph7_vm *pVm,ph7_value *pKey)
{
	sxu32 nId;
	if( pKey == 0 || (pKey->iFlags & MEMOBJ_RES) == 0 ){
		return;
	}
	nId = PH7_VmResourceId(pVm,pKey->x.pOther);
	VmErrorFormat(&(*pVm),PH7_CTX_WARNING,
		"Resource ID#%u used as offset, casting to integer (%u)",nId,nId);
	PH7_MemObjRelease(pKey);
	pKey->x.iVal = (sxi64)nId;
	MemObjSetType(pKey,MEMOBJ_INT);
}
/*
 * php DEPRECATES (it does not reject) a NULL array offset, then normalizes it to
 * the empty-string key "": `$a[null]`, `$a[$undef]` and `[null => v]` all warn
 * `Using null as an array offset is deprecated, use an empty string instead` and
 * then read/write the "" slot. Emit that E_DEPRECATED notice and return TRUE so
 * the caller falls through to the ordinary lookup/insert, which already casts
 * NULL->"" (HashmapLookup / HashmapInsert). A LOSSY-FLOAT offset stays a rejected
 * TypeError: PHL deliberately targets php's NON-deprecated surface for the
 * float->int truncation (VmRejectFloatOperand policy, §2), so only the null case
 * is coerced here. Returns FALSE (no notice) for a non-null key.
 */
static int VmNullOffsetDeprecate(ph7_vm *pVm,ph7_value *pKey)
{
	if( pKey == 0 || (pKey->iFlags & MEMOBJ_NULL) == 0 ){
		return FALSE;
	}
	PH7_VmThrowError(&(*pVm),0,E_DEPRECATED,
		"Using null as an array offset is deprecated, use an empty string instead");
	return TRUE;
}
/*
 * The three rules above, applied to a BUILTIN's key argument.
 *
 * php's array_key_exists() does not run a `string|int` ZPP row on its $key — it
 * hands the value to the same offset machinery `$a[$key]` uses, so the two agree
 * on every type: an object or an array is the catchable
 * `Cannot access offset of type X on array`, a RESOURCE warns and becomes its
 * integer id, `null` deprecates and reads the "" key, and a bool/float/numeric
 * string folds the way any subscript folds. PHL's builtin had its own narrower
 * check and therefore its own answers — a `string|int` TypeError for the two
 * cases php ACCEPTS (null, lossy float) and a silent `false` for the three php
 * REJECTS or coerces (object, array, resource). The object case was the worst of
 * them: a __toString() object was stringified and could answer TRUE for a key
 * php refuses to look up at all.
 *
 * bZppWording picks which of php's two messages the caller reports. php words the
 * illegal-type rejection differently in the alias than in the function itself —
 * `key_exists(): Argument #1 ($key) must be a valid array offset type` vs the
 * engine's offset Error — verified against 8.5.8; the null-key DEPRECATION is the
 * array_key_exists() wording in both.
 *
 * pKey is rewritten in place (resource -> int), so callers pass a private copy,
 * not the caller's own argument slot. Returns SXRET_OK when the key is usable, or
 * the status of the TypeError thrown.
 */
PH7_PRIVATE sxi32 PH7_VmArrayKeyArg(ph7_context *pCtx,ph7_value *pKey,int bZppWording)
{
	ph7_vm *pVm = pCtx->pVm;
	SyBlob sMsg;
	if( VmOffsetTypeRejected(pVm,pKey,0,&sMsg) ){
		sxi32 rc;
		if( bZppWording ){
			SyBlobRelease(&sMsg);
			return PH7_VmThrowException(pCtx,"TypeError",
				"%s(): Argument #1 ($key) must be a valid array offset type",
				ph7_function_name(pCtx));
		}
		rc = PH7_VmThrowException(pCtx,"TypeError","%.*s",
			(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));
		SyBlobRelease(&sMsg);
		return rc;
	}
	VmOffsetResourceWarn(pVm,pKey);
	if( (pKey->iFlags & MEMOBJ_REAL)
	 && pKey->rVal != (ph7_real)(sxi64)pKey->rVal ){
		/* php DEPRECATES the lossy float and truncates; PHL rejects it, here as
		 * everywhere else (§10) — and with the SAME message `$a[5.7]` gives, so
		 * the builtin and the subscript stay one rule. */
		return PH7_VmThrowException(pCtx,"TypeError",
			"Cannot access offset of type float on array");
	}
	if( pKey->iFlags & MEMOBJ_NULL ){
		/* php names the parameter here rather than "an array offset"; the effect
		 * is the engine's — the lookup below reads the "" key. */
		PH7_VmThrowError(pVm,0,E_DEPRECATED,
			"Using null as the key parameter for array_key_exists() is deprecated, "
			"use an empty string instead");
	}
	return SXRET_OK;
}
/*
 * Does this string START with an integer php's is_numeric_string would answer
 * IS_LONG for? Returns 0 when it does not — no digits at all ("", "-", "p"), a
 * FLOAT-shaped prefix ("1.5", ".5", "1.", "1e2", "1E2x") or a run that overflows
 * a signed 64-bit int ("9223372036854775808") — 1 when it does and only optional
 * trailing WHITESPACE follows (" 12 ", "+12", "007"), and 2 when it does but
 * trailing DATA follows ("12abc", "0x1", "1e", "1 x"), which is php's
 * `Illegal string offset` warning. *piVal receives the integer for 1 and 2.
 *
 * php reaches the same three answers through is_numeric_string_ex(allow_errors=1)
 * plus its IS_LONG/IS_DOUBLE verdict: a fraction or a well-formed exponent makes
 * the verdict IS_DOUBLE, which a string offset refuses, while a bare 'e' is just
 * trailing data.
 */
static int VmStringOffsetInt(const char *zIn,sxu32 nByte,sxi64 *piVal)
{
	const char *z = zIn, *zEnd = &zIn[nByte], *zDigit;
	sxu64 uVal = 0, uLimit;
	int isNeg = 0, nDigit, i;
	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){
		z++;
	}
	if( z < zEnd && (z[0] == '+' || z[0] == '-') ){
		isNeg = z[0] == '-';
		z++;
	}
	zDigit = z;
	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){
		z++;
	}
	nDigit = (int)(z - zDigit);
	if( nDigit < 1 ){
		return 0;
	}
	if( z < zEnd && z[0] == '.' ){
		/* "1." and "1.5" alike: php reads a double from here. */
		return 0;
	}
	if( z < zEnd && (z[0] == 'e' || z[0] == 'E') ){
		const char *zExp = &z[1];
		if( zExp < zEnd && (zExp[0] == '+' || zExp[0] == '-') ){
			zExp++;
		}
		if( zExp < zEnd && (unsigned char)zExp[0] < 0xc0 && SyisDigit(zExp[0]) ){
			return 0;
		}
	}
	/* Accumulate unsigned so PHP_INT_MIN's magnitude (2^63) is representable —
	 * "-9223372036854775808" is a legal offset, "9223372036854775808" is not. */
	while( nDigit > 1 && zDigit[0] == '0' ){
		zDigit++; nDigit--;
	}
	uLimit = isNeg ? (sxu64)SXI64_HIGH + 1 : (sxu64)SXI64_HIGH;
	if( nDigit > 19 ){
		return 0;
	}
	for( i = 0 ; i < nDigit ; ++i ){
		sxu64 d = (sxu64)(zDigit[i] - '0');
		if( uVal > (uLimit - d)/10 ){
			return 0;
		}
		uVal = uVal*10 + d;
	}
	*piVal = isNeg ? (sxi64)(~uVal + 1) : (sxi64)uVal;
	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){
		z++;
	}
	return z == zEnd ? 1 : 2;
}
/*
 * php's offset rules for a STRING container (zend_check_string_offset, and the
 * isset/empty arm of ZEND_ISSET_ISEMPTY_DIM_OBJ). They are NOT the array rules,
 * and PHL applied none of them: every offset went through an int cast, so
 * `$s[""]`, `$s["-"]` and `$s["p"]` all answered `$s[0]` — a silent wrong answer
 * on code php refuses to run. php's table:
 *
 *   int                     the offset
 *   null / bool / float     Warning: String offset cast occurred, then cast
 *   integer-shaped string   the offset (leading/trailing space and '+' allowed)
 *   int-then-garbage string Warning: Illegal string offset "12abc", then 12
 *   any other string        TypeError: Cannot access offset of type string on string
 *   array/object/resource   TypeError, naming the type (an object's CLASS)
 *
 * isset()/empty() raise NOTHING and answer "not set" for every shape the read
 * path would reject OR warn about: `isset($s["0x1"])` is false even though
 * reading it warns and yields `$s[0]`. A `??` fetch sits BETWEEN that and a real
 * read — it suppresses the not-set diagnostics but still warns about the offset
 * SHAPE and still reads it. iLevel selects which of the three (VM_STROFF_LOUD /
 * _COALESCE / _ISSET).
 */
PH7_PRIVATE int VmStringOffsetResolve(ph7_vm *pVm,ph7_value *pIdx,int iLevel,sxi64 *piOfft,SyBlob *pMsg)
{
	if( (pIdx->iFlags & MEMOBJ_INT) != 0 && (pIdx->iFlags & MEMOBJ_REAL) == 0 ){
		*piOfft = pIdx->x.iVal;
		return VM_STROFF_OK;
	}
	if( pIdx->iFlags & MEMOBJ_STRING ){
		int eInt = VmStringOffsetInt((const char *)SyBlobData(&pIdx->sBlob),
			SyBlobLength(&pIdx->sBlob),piOfft);
		if( eInt == 1 ){
			return VM_STROFF_OK;
		}
		if( eInt == 2 && iLevel != VM_STROFF_ISSET ){
			/* int-then-garbage. This is the one diagnostic a `??` fetch keeps:
			 * `$s["1x"] ?? "d"` warns and answers $s[1] (PHL called it "not set"
			 * and answered the default — a wrong VALUE, not just a missing
			 * warning). Only isset()/empty() stay silent about it. */
			SyString sKey;
			SyStringInitFromBuf(&sKey,SyBlobData(&pIdx->sBlob),SyBlobLength(&pIdx->sBlob));
			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Illegal string offset \"%z\"",&sKey);
			return VM_STROFF_OK;
		}
		if( iLevel != VM_STROFF_LOUD ){
			return VM_STROFF_MISS;
		}
	}else if( (pIdx->iFlags & (MEMOBJ_HASHMAP|MEMOBJ_OBJ|MEMOBJ_RES)) == 0 ){
		/* null / bool / float: php casts, but says so in a real read or write. */
		if( iLevel == VM_STROFF_LOUD ){
			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"String offset cast occurred");
		}
		PH7_MemObjToInteger(pIdx);
		*piOfft = pIdx->x.iVal;
		return VM_STROFF_OK;
	}else if( iLevel == VM_STROFF_ISSET ){
		/* An array/object/resource offset is "not set" for isset()/empty() — but a
		 * `??` fetch RAISES for it (probed: `$s[[]] ?? "d"` is the TypeError while
		 * `isset($s[[]])` is false), so only the fully-quiet level answers MISS. */
		return VM_STROFF_MISS;
	}
	{
		char zBuf[128];
		SyBlobInit(pMsg,&pVm->sAllocator);
		SyBlobFormat(pMsg,"Cannot access offset of type %s on string",
			VmValueGivenName(pIdx,zBuf,sizeof(zBuf)));
	}
	return VM_STROFF_REJECT;
}
/*
 * OP_STORE_IDX_REF: body moved verbatim from the OP_STORE_IDX_REF arm of
 * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.
 */
PH7_PRIVATE VmOpRc VmExecOpStoreIdxRef(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)
{
	ph7_value *pTos = pState->pTos;
	ph7_value *pStack = pState->pStack;
	VmInstr *aInstr = pState->aInstr;
	sxi32 pc = pState->pc;
	sxi32 rc;
	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);
	ph7_hashmap *pMap = 0; /* cc  warning */
	ph7_value *pKey;
	sxu32 nIdx;
	if( pInstr->iP1 ){
		/* Key is next on stack */
		pKey = pTos;
		pTos--;
	}else{
		pKey = 0;
	}
		/* php DEPRECATES a null / lossy-float write subscript (then normalizes "" /
		 * truncates); it does not reject either. A null key — by-VALUE (`$a[$k]=v`)
		 * OR by-REF (`$a[$k]=&$x`) — deprecates + coerces to the "" key: the notice is
		 * emitted DOWN AT THE INSERT (below), not here, so a null/false container that
		 * auto-vivifies (`$x=null; $x[null]=v`) gets it too (the base is not yet a
		 * hashmap at this point), and PH7_HashmapInsert / HashmapInsertByRef both cast
		 * NULL->"". A lossy-FLOAT subscript stays a loud TypeError in BOTH forms (the
		 * recorded non-deprecated-surface policy, §2). */
		if( pKey && (pTos->iFlags & MEMOBJ_HASHMAP) ){
			SyBlob sTypeMsg;
			/* An object/array key is php's TypeError; a resource key warns and
			 * becomes its integer id. Both used to be stringified silently. */
			if( VmOffsetTypeRejected(&(*pVm),pKey,0,&sTypeMsg) ){
				sxi32 rcSc;
				PH7_MemObjRelease(pKey);
				VmPopOperand(&pTos,1);
				rcSc = VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg);
				if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }
				rc = rcSc;
				PH7_THROW_ROUTE_MIDEXPR(rc)
			}
			VmOffsetResourceWarn(&(*pVm),pKey);
			if( (pKey->iFlags & MEMOBJ_REAL)
			 && pKey->rVal != (ph7_real)(sxi64)pKey->rVal ){
				sxi32 rcSc;
				const char *zErr = "Cannot access offset of type float on array";
				PH7_MemObjRelease(pKey);
				VmPopOperand(&pTos,1);
				rcSc = VmThrowFromVm(&(*pVm),"TypeError",zErr,(sxu32)SyStrlen(zErr));
				if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }
				rc = rcSc;
				PH7_THROW_ROUTE_MIDEXPR(rc)
			}
		}
	nIdx = pTos->nIdx;
	{
		/* ArrayAccess::offsetSet dispatch.
		 * Container may be on the stack as MEMOBJ_OBJ, or referenced via
		 * the backing variable slot at nIdx. */
		ph7_class_instance *pInst = 0;
		if( pTos->iFlags & MEMOBJ_OBJ ){
			pInst = (ph7_class_instance *)pTos->x.pOther;
		}else if( nIdx != SXU32_HIGH ){
			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);
			if( pBacking && (pBacking->iFlags & MEMOBJ_OBJ) ){
				pInst = (ph7_class_instance *)pBacking->x.pOther;
			}
		}
		if( pInst ){
			ph7_class *pArrayAccess = pVm->pArrayAccessClass;
			if( pArrayAccess && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){
				ph7_class_method *pMeth;
				ph7_value sNullKey;
				ph7_value *apArg[2];
				if( pInstr->iOp == PH7_OP_STORE_IDX_REF ){
					PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,
						"Cannot assign by reference to overloaded object");
					if( pKey ){ PH7_MemObjRelease(pKey); }
					VmPopOperand(&pTos,2); /* container + value */
					VM_EXIT_BREAK;
				}
				pMeth = PH7_ClassExtractMethod(pInst->pClass,
					"offsetSet",sizeof("offsetSet")-1);
				/* Pop container; pTos now points to the value */
				VmPopOperand(&pTos,1);
				if( pKey == 0 ){
					PH7_MemObjInit(&(*pVm),&sNullKey);
					apArg[0] = &sNullKey;
				}else{
					apArg[0] = pKey;
				}
				apArg[1] = pTos;
				if( pMeth ){
					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,0,2,apArg);
				}
				if( pKey ){
					PH7_MemObjRelease(pKey);
				}else{
					PH7_MemObjRelease(&sNullKey);
				}
				/* Pop the value */
				VmPopOperand(&pTos,1);
				VM_EXIT_BREAK;
			}
			/* Object without ArrayAccess: PHP throws a fatal Error rather
			 * than silently coercing the object into a hashmap (which is
			 * what the legacy PH7 fall-through would do via MemObjToHashmap
			 * a few lines below). Match PHP. */
			{
				char zMsg[256];
				SyString *pName = &pInst->pClass->sName;
				sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),
					"Cannot use object of type %.*s as array",
					(int)pName->nByte,pName->zString);
				rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);
				if( pKey ){ PH7_MemObjRelease(pKey); }
				VmPopOperand(&pTos,2); /* container + value */
				if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }
				PH7_THROW_ROUTE_MIDEXPR(rc)
			}
		}
	}
	if( pTos->iFlags & MEMOBJ_HASHMAP ){
		/* Hashmap already loaded on stack — COW separate the backing variable.
		 * The stack holds a temporary ref (from LOAD), so undo it before
		 * checking true sharing count, then re-add after separation. */
		if( nIdx != SXU32_HIGH ){
			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);
			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){
				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;
				/* Only adjust refcount / perform COW if the backing variable
				 * is still sharing the same hashmap instance. This mirrors
				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting
				 * refcounts if the backing array was already separated. */
				if( pBacking->x.pOther == (void *)pCur ){
					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */
					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);
					pMap->iRef++;  /* Re-add stack ref */
					pTos->x.pOther = pMap;
				}else{
					/* Backing variable no longer points at pCur: skip COW here
					 * and operate on the hashmap currently on the stack. */
					pMap = pCur;
				}
			}else{
				pMap = (ph7_hashmap *)pTos->x.pOther;
			}
		}else{
			pMap = (ph7_hashmap *)pTos->x.pOther;
		}
		if( pMap->iRef < 2 ){
			/* TICKET 1433-48: Prevent garbage collection during insertion.
			 * This inflation is safe with COW: VmPopOperand below will call
			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,
			 * no code checks iRef for COW decisions. */
			pMap->iRef = 2;
		}
	}else{
		ph7_value *pObj;
		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);
		if( pObj == 0 ){
			if( pKey ){
			  PH7_MemObjRelease(pKey);
			}
			VmPopOperand(&pTos,1);
			VM_EXIT_BREAK;
		}
		/* Phase#1: Load the array */
		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){
			VmPopOperand(&pTos,1);
			if( pKey == 0 ){
				/* `$s[] = 'x'` on a STRING: php raises the catchable Error
				 * "[] operator not supported for strings" and leaves the string
				 * untouched. PHL silently APPENDED, so code that meant to build
				 * an array from a variable holding a string quietly produced a
				 * longer string instead of failing — a wrong answer, not a
				 * missing diagnostic. */
				SyBlob sErrMsg;
				SyBlobInit(&sErrMsg,&pVm->sAllocator);
				SyBlobAppend(&sErrMsg,"[] operator not supported for strings",
					sizeof("[] operator not supported for strings")-1);
				VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));
				VM_EXIT_BREAK;
			}else{
				sxi64 iOfft = 0;
				SyBlob sTypeMsg;
				/* php's offset rules run BEFORE the RHS is looked at: an offset it
				 * refuses is the TypeError alone. The RHS cast below used to happen
				 * first, so every rejected shape — and `$s[] = [1,2]` above — came
				 * with a spurious `Array to string conversion` in front of it. */
				if( VmStringOffsetResolve(&(*pVm),pKey,VM_STROFF_LOUD,&iOfft,&sTypeMsg) == VM_STROFF_REJECT ){
					sxi32 rcSc;
					PH7_MemObjRelease(pKey);
					rcSc = VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg);
					if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }
					rc = rcSc;
					PH7_THROW_ROUTE_MIDEXPR(rc)
				}
				/* Force a string cast on the RHS (user-visible: an array warns
				 * "Array to string conversion" before the offset write, §2, and a
				 * not-stringable object throws — the target string is untouched) */
				{
					sxi32 rcSv = PH7_MemObjToStringUV(pTos);
					if( rcSv != SXRET_OK ){
						PH7_MemObjRelease(pKey);
						PH7_DISPATCH_TOSTRING_RC(rcSv)
					}
				}
				if( VmStringOffsetWrite(&(*pVm),pObj,iOfft,pTos) != SXRET_OK ){
					sxi32 rcEm;
					PH7_MemObjRelease(pKey);
					rcEm = VmThrowFromVm(&(*pVm),"Error",
						"Cannot assign an empty string to a string offset",
						sizeof("Cannot assign an empty string to a string offset")-1);
					if( rcEm == SXERR_ABORT ){ VM_EXIT_ABORT; }
					rc = rcEm;
					PH7_THROW_ROUTE_MIDEXPR(rc)
				}
			}
			if( pKey ){
			  PH7_MemObjRelease(pKey);
			}
			VM_EXIT_BREAK;
		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){
			/* php: only NULL and FALSE auto-vivify into an array. Writing an index into an
			 * int/float/resource/TRUE is a catchable Error -- PH7 quietly REPLACED the value
			 * with an array, destroying it ($x = 5; $x[0] = 1; left $x === [1]). */
			/* php auto-vivifies false into an array with an 8.1 DEPRECATION; PHL
			 * rejects any scalar base, false included (null still auto-vivifies). */
			int bScalar = (pObj->iFlags & (MEMOBJ_INT|MEMOBJ_REAL|MEMOBJ_RES|MEMOBJ_BOOL)) != 0;
			if( bScalar ){
				sxi32 rcSc;
				if( pKey ){
					PH7_MemObjRelease(pKey);
				}
				VmPopOperand(&pTos,1);
				rcSc = VmThrowFromVm(&(*pVm),"Error","Cannot use a scalar value as an array",
					sizeof("Cannot use a scalar value as an array")-1);
				if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }
				rc = rcSc;
				PH7_THROW_ROUTE_MIDEXPR(rc)
			}
			/* Force a hashmap cast  */
			rc = PH7_MemObjToHashmap(pObj);
			if( rc != SXRET_OK ){
				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");
				VM_EXIT_ABORT;
			}
		}
		/* COW separate the backing variable before mutation */
		pMap = PH7_HashmapCowSeparate(&(*pVm),pObj);
	}
	VmPopOperand(&pTos,1);
	/* Phase#2: Perform the insertion. A null key deprecates + normalizes to "" for
	 * BOTH the by-value and the by-ref store — emitted HERE, after a null/false
	 * container has auto-vivified to an array, so `$x[null]=v` and `$x[null]=&$y` on
	 * an undefined $x get the notice too (the top-of-handler check runs before
	 * vivification, when the base is not yet a hashmap). HashmapInsert /
	 * HashmapInsertByRef both then cast NULL->"". A null pKey==0 (an append, no key)
	 * is not a null OFFSET and is left alone. */
	VmNullOffsetDeprecate(&(*pVm),pKey);
	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && (pTos->iFlags & MEMOBJ_AUX_STROFFSET) ){
		/* `$a[] = &$s[1]`: the source is a string OFFSET, which php refuses to
		 * reference — and whose slot index is the BASE STRING's, so binding it
		 * aliased the whole string. Same Error the `=&` / by-ref-argument paths
		 * raise. The key is already popped; drop it and abandon the store. */
		sxi32 rcSc;
		if( pKey ){
			PH7_MemObjRelease(pKey);
		}
		rcSc = VmThrowFromVm(&(*pVm),"Error","Cannot create references to/from string offsets",
			sizeof("Cannot create references to/from string offsets")-1);
		if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }
		rc = rcSc;
		PH7_THROW_ROUTE_MIDEXPR(rc)
	}
	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){
		if( pMap == pVm->pGlobal ){
			/* php 8.1: $GLOBALS['y'] =& $x binds the global $y to $x's
			 * slot; an append has no name to bind (catchable Error). */
			if( pKey == 0 ){
				rc = PH7_VmThrowGlobalsAppendError(&(*pVm));
			}else{
				if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){
					PH7_MemObjToString(pKey);
				}
				if( SyBlobLength(&pKey->sBlob) < 1 ){
					/* Pathological empty name: keep the legacy diagnostic */
					PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,
						"$GLOBALS is a read-only array,insertion is forbidden");
					rc = SXRET_OK;
				}else{
					rc = PH7_VmInstallGlobalVar(&(*pVm),
						(const char *)SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),
						0,pTos->nIdx);
				}
			}
		}else{
			/* Insertion by reference */
			rc = PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);
		}
	}else{
		rc = PH7_HashmapInsert(pMap,pKey,pTos);
	}
	if( pKey ){
		PH7_MemObjRelease(pKey);
	}
	/* An append onto the occupied saturated auto-index threw php's catchable
	 * Error (PH7_VmThrowArrayNextIndexError) — dispatch it like any other
	 * store-path throw. Plain failures (OOM) keep their existing routes. */
	PH7_DISPATCH_ENFORCE_RC(rc)
	VM_EXIT_BREAK;
	VM_EXIT_BREAK;
}

/*
 * OP_LOAD_CLOSURE: body moved verbatim from the OP_LOAD_CLOSURE arm of
 * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.
 */
PH7_PRIVATE VmOpRc VmExecOpLoadClosure(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)
{
	ph7_value *pTos = pState->pTos;
	ph7_value *pStack = pState->pStack;
	VmInstr *aInstr = pState->aInstr;
	sxi32 pc = pState->pc;
	sxi32 rc;
	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);
	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;
	/* The function whose name the Closure object will wrap: a fresh per-instantiation
	 * copy for a real closure (built below), or the shared lambda function itself for a
	 * plain anonymous function with no captured environment. */
	ph7_vm_func *pTarget = pFunc;
	/* A no-capture lambda declared inside a class method is not VM_FUNC_CLOSURE
	 * (its env is empty), yet php still binds the creation-site class as its scope
	 * so `self::`/private access inside the body works. Detect the enclosing class
	 * here and route such a lambda through the per-instance closure path (a global
	 * no-capture lambda peeks NULL and stays a shared function). */
	ph7_class *pLoadScope = (pFunc->iFlags & VM_FUNC_CLOSURE) ? 0 : PH7_VmPeekDeclaringClass(pVm);
	if( (pFunc->iFlags & VM_FUNC_CLOSURE) || pLoadScope ){
		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;
		ph7_vm_func *pClosure;
		char *zName;
		sxu32 mLen;
		sxu32 n;
		/* Create a new VM function */
		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));
		/* Generate an unique closure name */
		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);
		if( pClosure == 0 || zName == 0){
			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");
			VM_EXIT_ABORT;
		}
		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);
		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){
			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);
		}
		/* Zero the stucture */
		SyZero(pClosure,sizeof(ph7_vm_func));
		/* Perform a structure assignment on read-only items */
		pClosure->aArgs = pFunc->aArgs;
		pClosure->aByteCode = pFunc->aByteCode;
		pClosure->aStatic = pFunc->aStatic;
		pClosure->iFlags = pFunc->iFlags;
		/* An in-class no-capture lambda routed here (pLoadScope set) must be a
		 * real closure so PH7_VmClassMemberAccess honors its pUserData scope. */
		pClosure->iFlags |= VM_FUNC_CLOSURE;
		pClosure->pUserData = pFunc->pUserData;
		pClosure->sSignature = pFunc->sSignature;
		pClosure->nReturnType = pFunc->nReturnType;
		pClosure->sReturnClass = pFunc->sReturnClass;
		pClosure->aReturnUnion = pFunc->aReturnUnion;
		pClosure->sReturnTypeName = pFunc->sReturnTypeName;
		pClosure->bStrictTypes = pFunc->bStrictTypes;
		pClosure->nMaxStack = pFunc->nMaxStack;
		if( pClosure->pUserData == 0 ){
			/* Stamp the creation-site class scope (see PH7_VmPeekDeclaringClass):
			 * a closure made in a method — or in another closure, whose own stamp
			 * the peek reads — resolves self::/parent:: against it like php. */
			pClosure->pUserData = (void *)PH7_VmPeekDeclaringClass(pVm);
		}
		/* Capture the creation-site late-static-binding class so `static::` inside
		 * the closure body resolves like php (the "called class", which may differ
		 * from the declaring scope stamped above — e.g. a closure made in an
		 * inherited method). A closure made outside any class captures NULL. */
		pClosure->pLsbClass = (void *)PH7_VmPeekTopClass(pVm);
		/* Reflection descriptor fields (getDocComment/getAttributes/getStartLine/
		 * getEndLine/getFileName read the INSTANTIATED copy via $__fn) */
		pClosure->aAttrs = pFunc->aAttrs;
		pClosure->sDoc = pFunc->sDoc;
		pClosure->sFile = pFunc->sFile;
		pClosure->nLine = pFunc->nLine;
		pClosure->nEndLine = pFunc->nEndLine;
		/* php's visible `{closure:...}` name is a property of the DECLARATION, so every
		 * per-instantiation copy answers the same one (php has a single op_array here). */
		pClosure->sClosureName = pFunc->sClosureName;
		SyStringInitFromBuf(&pClosure->sName,zName,mLen);
		/* Register the closure */
		PH7_VmInstallUserFunction(pVm,pClosure,0);
		/* Set up closure environment */
		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));
		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);
		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){
			ph7_value *pValue;
			pEnv = &aEnv[n];
			sEnv.sName  = pEnv->sName;
			sEnv.iFlags = pEnv->iFlags;
			sEnv.nLine = pEnv->nLine;
			sEnv.nIdx = SXU32_HIGH;
			PH7_MemObjInit(pVm,&sEnv.sValue);
			if( (sEnv.iFlags & (VM_FUNC_ARG_BY_REF|VM_FUNC_ARG_IGNORE)) == VM_FUNC_ARG_BY_REF
			 && !(SyStringLength(&sEnv.sName) == sizeof("this")-1
				&& SyMemcmp(SyStringData(&sEnv.sName),"this",sizeof("this")-1) == 0) ){
				/* Capture by reference: bind the env entry to the variable's
				 * memory slot — creating a fresh null variable when missing,
				 * as php does (`use (&$f)` before $f is assigned) — and pin
				 * the slot past the creating frame's teardown so the closure
				 * can outlive its birth scope. The call-time env install
				 * aliases the name to this slot instead of copying a value. */
				pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,TRUE);
				if( pValue ){
					sEnv.nIdx = pValue->nIdx;
					VmPinMemObjSlot(pVm,pValue->nIdx);
				}
			}else{
				/* Standard pass by value */
				pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);
				if( pValue ){
					/* Copy imported value */
					PH7_MemObjStore(pValue,&sEnv.sValue);
				}else if( (sEnv.iFlags & (VM_FUNC_ARG_BY_REF|VM_FUNC_ARG_IGNORE)) == 0
					&& !(SyStringLength(&sEnv.sName) == sizeof("this")-1
						&& SyMemcmp(SyStringData(&sEnv.sName),"this",sizeof("this")-1) == 0) ){
					if( pFunc->iFlags & VM_FUNC_ARROW ){
						/* An arrow function auto-captures free variables by value, but
						 * php does NOT capture one that is UNDEFINED at creation: the
						 * isolated body scope then simply has no such variable, so a
						 * read of it there raises the normal "Undefined variable"
						 * warning (and reflection's getClosureUsedVariables omits it).
						 * Skip installing the capture so the body READ — not the
						 * creation — warns, matching php. (A later assignment to the
						 * outer variable does not retro-capture: arrow scope is
						 * isolated.) An explicit by-value use() instead warns here and
						 * binds NULL, handled just below. */
						continue;
					}
					/* php reads a by-value `use ($q)` capture AT CLOSURE CREATION and
					 * warns when the variable is undefined there (the by-ref form
					 * `use (&$q)` above stays silent — it creates the binding). The
					 * auto-injected $this (VM_FUNC_ARG_IGNORE) never warns. The
					 * capture still proceeds as NULL, as php does. php attributes the
					 * warning to the capture's own line (which can differ from the
					 * OP_LOAD_CLOSURE instruction line when the use-clause wraps), so
					 * borrow the recorded line for the emission and restore it. */
					sxu32 nSavedLine = pVm->nCurLine;
					if( sEnv.nLine ){
						pVm->nCurLine = sEnv.nLine;
					}
					VmErrorFormat(pVm,PH7_CTX_WARNING,"Undefined variable $%z",&sEnv.sName);
					pVm->nCurLine = nSavedLine;
				}
			}
			/* Insert the imported variable */
			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);
		}
		pTarget = pClosure;
	}
	/* Wrap the target function in a Closure object and push it. Its captured environment
	 * (incl. any `$this`) stays in pTarget->aClosureEnv and is delivered by the normal call
	 * path when the closure is dispatched by name. */
	pTos++;
	{
		ph7_class_instance *pCloObj = VmCreateClosure(pVm, &pTarget->sName, 0, 0);
		if( pCloObj ){
			pCloObj->iRef++;
			pTos->x.pOther = pCloObj;
			MemObjSetType(pTos, MEMOBJ_OBJ);
		}else{
			/* OOM fallback: the name string is still a usable callable. */
			PH7_MemObjStringAppend(pTos, pTarget->sName.zString, pTarget->sName.nByte);
		}
	}
	VM_EXIT_BREAK;
	VM_EXIT_BREAK;
}


/*
 * TRUE when this LOAD_IDX feeds a `??`: the coalesce test is the very next
 * instruction. php evaluates the whole left operand of `??` in isset-context,
 * so the access must stay SILENT and, when it misses, must yield NULL — an
 * out-of-range string offset that yielded "" instead made `$s[99] ?? $d`
 * evaluate to "" rather than $d, a wrong answer rather than a stray notice.
 */
static int VmIdxFeedsCoalesce(const VmInstr *pInstr)
{
	return (pInstr+1)->iOp == PH7_OP_NULLC || (pInstr+1)->iOp == PH7_OP_NULLC_JMP;
}
/*
 * php's string-offset STORE, shared by `$s[i] = v` (OP_STORE_IDX) and
 * `$s[i] ??= v` (OP_NULLC_STORE — `??=` is not an assign-op, so php performs a
 * real offset write there): resolve iRawOfft against the string's CURRENT length
 * (a negative offset counts back from the end and, when it still lands before the
 * start, warns `Illegal string offset` and writes NOTHING), refuse an EMPTY
 * replacement, warn when more than one byte was handed over, PAD WITH SPACES up
 * to the offset, then write the first byte.
 *
 * pVal must ALREADY be a string: that coercion is user-visible (it warns for an
 * array, throws for a not-stringable object) and stays with the callers, which
 * are the only places that can route a throw. Answers SXRET_OK when the store
 * happened or was skipped, and SXERR_INVALID for php's `Cannot assign an empty
 * string to a string offset` Error — raised by the caller for the same reason.
 */
PH7_PRIVATE sxi32 VmStringOffsetWrite(ph7_vm *pVm,ph7_value *pStr,sxi64 iRawOfft,ph7_value *pVal)
{
	sxi64 nLen = (sxi64)SyBlobLength(&pStr->sBlob);
	sxi64 iOfft = iRawOfft;
	const char *zVal;
	if( iOfft < 0 ){
		/* php 7.1: a negative offset writes back from the end. */
		iOfft += nLen;
		if( iOfft < 0 ){
			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Illegal string offset %qd",iRawOfft);
			return SXRET_OK;
		}
	}
	if( SyBlobLength(&pVal->sBlob) < 1 ){
		/* php refuses to write NOTHING into an offset — `$s[2] = ""`, and the
		 * `= null` / `= false` that stringify to "" — where PHL silently ignored
		 * the store. */
		return SXERR_INVALID;
	}
	zVal = (const char *)SyBlobData(&pVal->sBlob);
	if( SyBlobLength(&pVal->sBlob) > 1 ){
		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,
			"Only the first byte will be assigned to the string offset");
	}
	if( iOfft >= nLen ){
		/* php PADS WITH SPACES up to the offset. PH7 simply appended the byte, so
		 * "abc" with [6]="Z" became "abcZ" rather than "abc   Z" -- a silently
		 * wrong string. */
		sxi64 nPad;
		for( nPad = nLen ; nPad < iOfft ; ++nPad ){
			SyBlobAppend(&pStr->sBlob," ",sizeof(char));
		}
		SyBlobAppend(&pStr->sBlob,(const void *)zVal,sizeof(char));
	}else{
		char *zData = (char *)SyBlobData(&pStr->sBlob);
		zData[iOfft] = zVal[0];
	}
	return SXRET_OK;
}
/*
 * Does this LOAD_IDX feed a `??=` (rather than a plain `??`)? The compiler emits
 * the LHS peek, then OP_NULLC_JMP, then the RHS, then OP_NULLC_STORE — so the
 * NULLC_JMP right after is what distinguishes the assigning form, whose store
 * still has to happen when the peek answers null.
 */
static int VmIdxFeedsCoalesceAssign(const VmInstr *pInstr)
{
	return (pInstr+1)->iOp == PH7_OP_NULLC_JMP;
}
/*
 * Arm the write-back half of `$o[$k] op= v` on an ArrayAccess container. The
 * value offsetGet just answered goes into a fresh SCRATCH memobj that pTos
 * points at, so the compound-assign op computes IN that slot the way it would
 * in an ordinary variable; the pending entry then makes the op's tail dispatch
 * `offsetSet($k, computed)`. The KEY gets a reserved slot of its own (pIdx is
 * released as soon as this opcode returns, and the write happens one opcode
 * later); an ABSENT key — `$o[] op= v` — is carried as NULL, which is the value
 * php hands both accessors for that shape.
 *
 * A failed reservation simply leaves the value unarmed: pTos keeps its
 * no-slot temp and the op falls back to the pre-existing refusal.
 */
static void VmDimRmwArm(
	ph7_vm *pVm,
	ph7_class_instance *pInst,
	ph7_value *pIdx,
	ph7_value *pTos,
	void *pOwnerStack,
	void *pInstrs,
	sxu32 nPc
	)
{
	ph7_value *pSlot;
	sxu32 nScratch;
	sxu32 nKey;
	VmHookRmw sRmw;
	pSlot = PH7_ReserveMemObj(&(*pVm));
	if( pSlot == 0 ){
		return;
	}
	nScratch = pSlot->nIdx;
	pSlot = PH7_ReserveMemObj(&(*pVm));
	if( pSlot == 0 ){
		VmHookRmwFreeScratch(&(*pVm),nScratch);
		return;
	}
	nKey = pSlot->nIdx;
	/* Reserving can GROW the aMemObj set, so address both slots by index from
	 * here on — the pointer the first reservation handed back may be stale. */
	if( pIdx ){
		PH7_MemObjStore(pIdx,pSlot);
	}
	pSlot = (ph7_value *)SySetAt(&pVm->aMemObj,nScratch);
	if( pSlot == 0 ){
		VmHookRmwFreeScratch(&(*pVm),nKey);
		VmHookRmwFreeScratch(&(*pVm),nScratch);
		return;
	}
	PH7_MemObjStore(pTos,pSlot);
	sRmw.iKind = VM_HOOK_PEND_RMW_DIM;
	sRmw.pThis = pInst;
	sRmw.pAttr = 0;
	sRmw.nBackIdx = nKey;
	sRmw.nScratchIdx = nScratch;
	SyBlobInit(&sRmw.sName,&pVm->sAllocator);
	sRmw.pOwnerStack = pOwnerStack;
	sRmw.pInstrs = pInstrs;
	sRmw.nJmpPc = nPc;  /* the modify op ... */
	sRmw.nPc = nPc;     /* ... is the whole window */
	pInst->iRef++;
	pTos->nIdx = nScratch;
	SySetPut(&pVm->aHookRmw,(const void *)&sRmw);
}
/*
 * Is this LOAD_IDX fetching the dimension for WRITING — php's BP_VAR_W / BP_VAR_RW
 * / BP_VAR_UNSET fetch, the one that asks the container for a slot to MODIFY
 * rather than a value to read?
 *
 * iP2 answers for most of it. The two shapes it cannot are the ones where the
 * fetch is compiled as a plain read and the NEXT instruction is what makes it a
 * write: binding a reference to the element (`$r = &$o[$k]`) and iterating it by
 * reference (`foreach ($o[$k] as &$v)`). A compound assign is the reverse case —
 * iP2 says write, but php compiles it to ASSIGN_DIM_OP, a read plus a write
 * through the container's own handlers (VmDimRmwArm), not a write FETCH.
 */
static int VmIdxFetchForWrite(const VmInstr *pInstr,sxi32 iP2)
{
	const VmInstr *pNext = pInstr + 1;
	if( iP2 == 1 ){
		return !VmNextIsCompoundAssign(pNext);
	}
	if( iP2 == VM_IDX_CTX_UNSET_BASE ){
		/* An INTERMEDIATE subscript of an unset chain: php fetches it for
		 * writing so the removal one level down can land. */
		return 1;
	}
	if( iP2 == 0 ){
		if( pNext->iOp == PH7_OP_STORE_REF ){
			return 1;
		}
		if( pNext->iOp == PH7_OP_FOREACH_INIT && pNext->p3 ){
			return (((ph7_foreach_info *)pNext->p3)->iFlags & PH7_4EACH_STEP_REF) != 0;
		}
	}
	return 0;
}
/*
 * php's `Indirect modification of overloaded element of C has no effect`: the
 * write-context fetch above landed on a container that answers with a COPY, so
 * whatever the rest of the expression writes is thrown away. php says so and
 * carries on.
 *
 * PHL had neither half. The notice was missing, and the copy was not a copy: a
 * userland offsetGet returns the container's own nested hashmap by COW, and
 * OP_STORE_IDX on a base with no slot index writes STRAIGHT INTO the shared map —
 * so `$o['a']['b'] = 9`, `$o['a'][] = 5` and `foreach ($o['a'] as &$v)` all
 * modified the object php leaves untouched, silently. Separating the value here
 * is what makes the write land nowhere.
 *
 * php stays silent for an OBJECT, and so does this: an object is a handle, the
 * write through it is not lost, and nothing about it is indirect.
 */
static void VmOverloadedElemNotice(ph7_vm *pVm,ph7_class *pClass,ph7_value *pVal)
{
	if( pVal->iFlags & MEMOBJ_OBJ ){
		return;
	}
	VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,
		"Indirect modification of overloaded element of %z has no effect",
		&pClass->sName);
	if( pVal->iFlags & MEMOBJ_HASHMAP ){
		PH7_HashmapCowSeparate(&(*pVm),pVal);
	}
}
/*
 * OP_LOAD_IDX: body moved verbatim from the OP_LOAD_IDX arm of
 * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.
 */
PH7_PRIVATE VmOpRc VmExecOpLoadIdx(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)
{
	ph7_value *pTos = pState->pTos;
	ph7_value *pStack = pState->pStack;
	VmInstr *aInstr = pState->aInstr;
	sxi32 pc = pState->pc;
	sxi32 rc;
	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);
	ph7_hashmap_node *pNode = 0; /* cc warning */
	ph7_hashmap *pMap = 0;
	ph7_value *pIdx;
	/* D1 commit 2: iP2==9 is the deferred-record mode. It behaves exactly like a plain read
	 * (iP2==0) for base dispatch / the read tail, EXCEPT the dedicated block right after the
	 * index is popped, which — on a lookup MISS with a reachable base — captures the lvalue
	 * path (MEMOBJ_AUX_DEFPATH) instead of warning, and exits. Everything else sees iP2. */
	sxi32 iP2 = (pInstr->iP2 == 9) ? 0 : pInstr->iP2;
	pIdx = 0;
	if( pInstr->iP1 == 0 ){
		if( !iP2){
			/* No available index,load NULL */
			if( pTos >= pStack ){
				PH7_MemObjRelease(pTos);
			}else{
				/* TICKET 1433-020: Empty stack */
				pTos++;
				MemObjSetType(pTos,MEMOBJ_NULL);
				pTos->nIdx = SXU32_HIGH;
			}
			/* Emit a notice */
			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,
				"Array: Attempt to access an undefined index,PH7 is loading NULL");
			VM_EXIT_BREAK;
		}
	}else{
		pIdx = pTos;
		pTos--;
	}
	if( pInstr->iP2 == 9 && pIdx ){
		/* D1 commit 2 record mode. Decide whether to CAPTURE this subscript as a deferred
		 * lvalue step (the by-ref/by-value decision is not known until OP_CALL), or fall
		 * through to a plain read (iP2 was normalized to 0 above). We defer only when the
		 * base can be reached again at resolve time: an existing descriptor (nested), the
		 * commit-1 undefined-variable marker, or a real container/string/scalar slot
		 * (nIdx != SXU32_HIGH). On a present array key we DO NOT defer — a hit already
		 * yields the aliasable read slot the by-ref binder needs. */
		VmDeferredPath *pPath = 0;
		int bDefer = 0, eRoot = 0;
		if( pTos->iFlags & MEMOBJ_AUX_DEFPATH ){
			/* Nested: the base already carries a descriptor — extend it in place. */
			pPath = (VmDeferredPath *)pTos->x.pOther;
			bDefer = 1;
		}else if( pTos->iFlags & MEMOBJ_AUX_DEFERRED ){
			/* Undefined base variable (commit-1 marker): root the descriptor by name. */
			SyString sRootName;
			SyStringInitFromBuf(&sRootName,(const char *)pTos->x.pOther,
				pTos->x.pOther ? SyStrlen((const char *)pTos->x.pOther) : 0);
			pPath = VmDeferPathNew(&(*pVm),1,SXU32_HIGH,&sRootName);
			pTos->iFlags &= ~MEMOBJ_AUX_DEFERRED; /* borrowed name; don't free x.pOther */
			pTos->x.pOther = 0;
			bDefer = (pPath != 0);
		}else if( pTos->nIdx != SXU32_HIGH ){
			/* A real base slot: array/scalar -> eRoot 0 (by-ref may vivify), string -> 2. */
			if( pTos->iFlags & MEMOBJ_HASHMAP ){
				/* Probe hit/miss on a COPY of the key: PH7_HashmapLookup casts a NULL key to
				 * "" in place, which would suppress the fall-through read's null-offset
				 * deprecation (hit) and capture the wrong key in the step (miss). */
				ph7_value idxProbe;
				pMap = (ph7_hashmap *)pTos->x.pOther;
				PH7_MemObjInit(&(*pVm),&idxProbe);
				PH7_MemObjStore(pIdx,&idxProbe);
				if( PH7_HashmapLookup(pMap,&idxProbe,&pNode) == SXRET_OK ){
					bDefer = 0; /* present key: fall through and read it as an aliasable slot */
				}else{
					eRoot = 0; bDefer = 1;
				}
				PH7_MemObjRelease(&idxProbe);
			}else if( pTos->iFlags & MEMOBJ_OBJ ){
				bDefer = 0; /* ArrayAccess: not a deferrable lvalue — read normally */
			}else if( pTos->iFlags & MEMOBJ_STRING ){
				eRoot = 2; bDefer = 1;
			}else{
				eRoot = 0; bDefer = 1; /* NULL/other reachable scalar base */
			}
			if( bDefer && pPath == 0 ){
				pPath = VmDeferPathNew(&(*pVm),eRoot,pTos->nIdx,0);
				if( pPath == 0 ){
					bDefer = 0;
				}
			}
		}
		if( bDefer ){
			/* Append this element step (deep-copies pIdx) and leave the carrier on pTos. */
			if( VmDeferPathPushElem(pPath,pIdx) == SXRET_OK ){
				if( (pTos->iFlags & MEMOBJ_AUX_DEFPATH) == 0 ){
					/* Collapse the base value into the descriptor carrier. */
					PH7_MemObjRelease(pTos);
					pTos->x.pOther = pPath;
					pTos->iFlags = MEMOBJ_NULL | MEMOBJ_AUX_DEFPATH;
					pTos->nIdx = SXU32_HIGH;
				}
				PH7_MemObjRelease(pIdx);
				VM_EXIT_BREAK;
			}
			/* Out of memory appending a step. */
			if( pTos->iFlags & MEMOBJ_AUX_DEFPATH ){
				/* Nested base: pPath IS pTos's carrier and stays owned by it — keep it (a
				 * step short) and exit, rather than fall through and misread a NULL-typed
				 * carrier as an array base. Resolves the shorter path; OOM-only degradation. */
				PH7_MemObjRelease(pIdx);
				VM_EXIT_BREAK;
			}
			/* A freshly-allocated path we still own: drop it and fall back to a plain read. */
			VmFreeDeferredPath(pPath);
		}
	}
	if( iP2 == 7 && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){
		/* Keyed list destructuring `["k"=>$v] = $src` from a NON-array source: yield NULL
		 * (never char-index a string), warning once per key — matching PHP, which warns per
		 * key here. A NULL source is silent; unlike the positional OP_LOAD_LIST path, a bool
		 * source DOES warn (PHP warns for bool in keyed destructuring). */
		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){
			VmWarnCannotUseAsArray(&(*pVm),pTos->iFlags);
		}
		if( pIdx ){
			/* Release the key (a string literal for keyed destructuring), like the
			 * normal hashmap-read exit below — otherwise its blob is orphaned. */
			PH7_MemObjRelease(pIdx);
		}
		PH7_MemObjRelease(pTos);
		MemObjSetType(pTos,MEMOBJ_NULL);
		VM_EXIT_BREAK;
	}
	if( pTos->iFlags & MEMOBJ_STRING ){
		/* String access */
		if( VM_IDX_IS_UNSET(iP2) ){
			/* php: a string offset cannot be unset AT ALL — `unset($s[0])` is the
			 * catchable `Error: Cannot unset string offsets`, whatever the offset is
			 * and whether or not it is in range. PHL read the character but left the
			 * BASE VARIABLE's slot index on the result, so the trailing unset()
			 * builtin freed the base itself: `$s = "abc"; unset($s[1]);` left $s
			 * UNDEFINED, and `unset($a["k"][0])` deleted the whole element. */
			rc = VmThrowFromVm(&(*pVm),"Error","Cannot unset string offsets",
				sizeof("Cannot unset string offsets")-1);
			if( pIdx ){
				PH7_MemObjRelease(pIdx);
			}
			PH7_MemObjRelease(pTos);
			MemObjSetType(pTos,MEMOBJ_NULL);
			pTos->nIdx = SXU32_HIGH;
			if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }
			PH7_THROW_ROUTE_MIDEXPR(rc)
		}
		if( pIdx ){
			sxi64 iOfft = 0, iRaw;
			sxi64 nLen = (sxi64)SyBlobLength(&pTos->sBlob);
			/* LOAD_IDX carries its OWN iP2 codes, which do NOT line up with the
			 * PH7_MEMBER_* ones: 4 = isset, 5 = unset, 6 = empty, 8 = `??` read,
			 * 3 = `??=` peek. All are LOOKUPS, but php splits them into TWO
			 * levels: isset()/empty()/unset() say nothing at all, while a
			 * `??`/`??=` fetch still warns about the offset SHAPE and reads it
			 * (VM_STROFF_COALESCE). The ??= peek is recognised by the NULLC_JMP
			 * that follows it, since its iP2 does not distinguish the base type. */
			int iOfftLevel = (iP2 == 4 || VM_IDX_IS_UNSET(iP2) || iP2 == 6) ? VM_STROFF_ISSET
				: ((iP2 == 8 || VmIdxFeedsCoalesce(pInstr)) ? VM_STROFF_COALESCE
				: VM_STROFF_LOUD);
			int bQuiet = iOfftLevel != VM_STROFF_LOUD;
			SyBlob sTypeMsg;
			int eOfft;
			VmCoalStrOff *pCoalOff = 0;
			if( VmIdxFeedsCoalesceAssign(pInstr) ){
				/* `$s[k] ??= v`: the OP_NULLC_STORE ahead has to write into the
				 * string OFFSET, and by then the offset is gone — this op consumes
				 * it. Carry the RAW index to the store on the peek's own result
				 * (MEMOBJ_AUX_COALSTROFF), which nests and cannot leak; the copy
				 * must predate the resolution below, which casts a float/null/bool
				 * in place, because php re-resolves the offset LOUDLY at the store:
				 * the peek is the quiet half of its pair. */
				pCoalOff = VmCoalStrOffNew(&(*pVm),pIdx);
			}
			eOfft = VmStringOffsetResolve(&(*pVm),pIdx,iOfftLevel,&iOfft,&sTypeMsg);
			if( eOfft == VM_STROFF_MISS ){
				/* A lookup over an offset php refuses: not set, in silence. */
				PH7_MemObjRelease(pIdx);
				PH7_MemObjRelease(pTos);
				MemObjSetType(pTos,MEMOBJ_NULL);
				if( pCoalOff ){
					/* A `??=` whose offset the READ refuses: php raises at the
					 * STORE instead, so keep the base slot reachable and hand the
					 * offset to OP_NULLC_STORE. */
					pTos->x.pOther = (void *)pCoalOff;
					pTos->iFlags |= MEMOBJ_AUX_STROFFSET|MEMOBJ_AUX_COALSTROFF;
				}else{
					pTos->nIdx = SXU32_HIGH;
				}
				VM_EXIT_BREAK;
			}
			if( eOfft == VM_STROFF_REJECT ){
				/* php's TypeError for an offset type a string refuses. PHL cast
				 * every offset to int, so `$s[""]`, `$s["-"]` and `$s["p"]` all
				 * answered `$s[0]`. Routed as a mid-expression throw (this opcode
				 * is not a call boundary), so the rest of the expression is
				 * abandoned the way php abandons it. */
				VmFreeCoalStrOff(pCoalOff);
				rc = VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg);
				PH7_MemObjRelease(pIdx);
				PH7_MemObjRelease(pTos);
				MemObjSetType(pTos,MEMOBJ_NULL);
				pTos->nIdx = SXU32_HIGH;
				if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }
				PH7_THROW_ROUTE_MIDEXPR(rc)
			}
			iRaw = iOfft;
			/* php 7.1: a NEGATIVE offset counts back from the end ($s[-1] is the last
			 * character). The offset used to be cast to UNSIGNED, so -1 became a huge
			 * number, ran past the end and quietly produced NULL. */
			if( iOfft < 0 ){
				iOfft += nLen;
			}
			if( iOfft < 0 || iOfft >= nLen ){
				/* Out of range. In an isset()/empty() lookup php answers FALSE, so load
				 * NULL there; everywhere else it WARNS and yields the empty string (PH7
				 * silently produced NULL in both cases). */
				PH7_MemObjRelease(pTos);
				if( bQuiet ){
					MemObjSetType(pTos,MEMOBJ_NULL);
				}else{
					MemObjSetType(pTos,MEMOBJ_STRING);
					VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Uninitialized string offset %qd",
						iRaw);
				}
			}else{
				const char *zData = (const char *)SyBlobData(&pTos->sBlob);
				int c = zData[iOfft];
				PH7_MemObjRelease(pTos);
				MemObjSetType(pTos,MEMOBJ_STRING);
				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));
			}
			if( pCoalOff ){
				if( pTos->iFlags & MEMOBJ_NULL ){
					/* Out of range: the `??=` will store, so hand the offset over. */
					pTos->x.pOther = (void *)pCoalOff;
					pTos->iFlags |= MEMOBJ_AUX_COALSTROFF;
				}else{
					/* A real byte: the `??=` short-circuits over the store. */
					VmFreeCoalStrOff(pCoalOff);
				}
			}
			/* The result still carries the BASE VARIABLE's slot index, which is
			 * harmless for a plain read and WRONG for anything that would ALIAS it:
			 * a string offset is not a slot. Mark it so the reference-binding sites
			 * raise php's Error instead of aliasing the whole string --
			 * `$r = &$s[1]; $r = "Z";` REPLACED $s with "Z". */
			pTos->iFlags |= MEMOBJ_AUX_STROFFSET;
		}else{
			/* No available index,load NULL */
			MemObjSetType(pTos,MEMOBJ_NULL);
		}
		VM_EXIT_BREAK;
	}
	if( pTos->iFlags & MEMOBJ_OBJ ){
		/* Object subscript: ArrayAccess dispatch.
		 * iP2 codes:
		 *   0 = read       → offsetGet
		 *   3 = ??= peek   → offsetExists; offsetGet on hit; arm coalesce
		 *                    target on miss for the upcoming NULLC_STORE
		 *   4 = isset()    → offsetExists
		 *   5 = unset()    → offsetUnset
		 *   6 = empty()    → offsetExists, then offsetGet on hit
		 *   8 = `??` read  → same probe as 6 (offsetExists, then offsetGet on a
		 *                    hit) and no diagnostics anywhere in this op: php
		 *                    evaluates the whole left operand of `??` in
		 *                    isset-context, and calling offsetGet blindly also
		 *                    surfaced warnings raised INSIDE a userland
		 *                    offsetGet (e.g. ArrayObject's own array read). */
		ph7_class_instance *pInst = (ph7_class_instance *)pTos->x.pOther;
		ph7_class *pArrayAccess = pVm->pArrayAccessClass;
		if( pArrayAccess && pInst && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){
			ph7_class_method *pMeth;
			ph7_value sResult;
			ph7_value sNullIdx;
			ph7_value *apArg[1];
			if( (iP2 == 0 || iP2 == 3 || iP2 == 8) && pIdx == 0 ){
				/* `$obj[]` read — PHP rejects this. */
				PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,
					"Cannot use [] for reading");
				PH7_MemObjRelease(pTos);
				pTos->nIdx = SXU32_HIGH;
				VM_EXIT_BREAK;
			}
			PH7_MemObjInit(&(*pVm),&sResult);
			if( iP2 == 4 || iP2 == 6 || iP2 == 3 || iP2 == 8 ){
				/* isset, empty, ??= and `??` all start with offsetExists. */
				pMeth = PH7_ClassExtractMethod(pInst->pClass,
					"offsetExists",sizeof("offsetExists")-1);
				apArg[0] = pIdx;
				if( pMeth ){
					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);
				}
			}else if( iP2 == 5 ){
				pMeth = PH7_ClassExtractMethod(pInst->pClass,
					"offsetUnset",sizeof("offsetUnset")-1);
				apArg[0] = pIdx;
				if( pMeth ){
					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);
				}
			}else{
				pMeth = PH7_ClassExtractMethod(pInst->pClass,
					"offsetGet",sizeof("offsetGet")-1);
				if( pIdx == 0 ){
					/* `$o[] op= v` — the one read that reaches here without a key.
					 * php hands the accessors NULL for the absent offset (its
					 * read_dimension substitutes one), so passing NO argument
					 * turned an assignment php performs into an
					 * ArgumentCountError against the class's own offsetGet. */
					PH7_MemObjInit(&(*pVm),&sNullIdx);
					pIdx = &sNullIdx;
				}
				apArg[0] = pIdx;
				if( pMeth ){
					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);
				}
			}
			if( iP2 == 4 ){
				/* isset: push MEMOBJ_BOOL so vm_builtin_isset reports the
				 * right truth value AND skips its "Expecting a variable not
				 * a constant" warning (keyed on MEMOBJ_BOOL). */
				int bExists = ph7_value_to_bool(&sResult);
				PH7_MemObjRelease(pTos);
				pTos->nIdx = SXU32_HIGH;
				if( bExists ){
					MemObjSetType(pTos,MEMOBJ_BOOL);
					pTos->x.iVal = 1;
				}else{
					MemObjSetType(pTos,MEMOBJ_NULL);
				}
			}else if( iP2 == 5 ){
				/* offsetUnset return is discarded; push NULL so the trailing
				 * vm_builtin_unset is a harmless no-op. */
				PH7_MemObjRelease(pTos);
				pTos->nIdx = SXU32_HIGH;
				MemObjSetType(pTos,MEMOBJ_NULL);
			}else if( iP2 == 6 || iP2 == 8 ){
				/* empty: if offsetExists is false, push NULL so empty=true
				 * without calling offsetGet. If true, call offsetGet and
				 * push the value so PH7_builtin_empty evaluates emptiness.
				 * `??` (8) needs the identical shape: NULL on a miss so the
				 * coalesce takes the default, the real value on a hit. */
				int bExists = ph7_value_to_bool(&sResult);
				PH7_MemObjRelease(&sResult);
				PH7_MemObjRelease(pTos);
				pTos->nIdx = SXU32_HIGH;
				if( !bExists ){
					MemObjSetType(pTos,MEMOBJ_NULL);
				}else{
					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,
						"offsetGet",sizeof("offsetGet")-1);
					ph7_value sValue;
					PH7_MemObjInit(&(*pVm),&sValue);
					apArg[0] = pIdx;
					if( pGet ){
						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);
					}
					PH7_MemObjStore(&sValue,pTos);
					PH7_MemObjRelease(&sValue);
				}
				if( pIdx ){ PH7_MemObjRelease(pIdx); }
				VM_EXIT_BREAK; /* skip the duplicate sResult release below */
			}else if( iP2 == 3 ){
				/* ?? null-coalesce peek: emulate PHP semantics —
				 *   if !offsetExists OR offsetGet() === null → arm
				 *     coalesce slot (NULLC_STORE will call offsetSet)
				 *     and push NULL.
				 *   else → push offsetGet's value (NULLC_JMP skips). */
				int bExists = ph7_value_to_bool(&sResult);
				int bShouldArm = !bExists;
				ph7_value sValue;
				PH7_MemObjRelease(&sResult);
				/* Reset any prior arming defensively */
				VmCoalesceDisarm(pVm);
				PH7_MemObjInit(&(*pVm),&sValue);
				if( bExists ){
					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,
						"offsetGet",sizeof("offsetGet")-1);
					apArg[0] = pIdx;
					if( pGet ){
						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);
					}
					if( sValue.iFlags & MEMOBJ_NULL ){
						bShouldArm = 1;
					}
				}
				PH7_MemObjRelease(pTos);
				pTos->nIdx = SXU32_HIGH;
				if( bShouldArm ){
					/* Arm: remember (object, key) so NULLC_STORE dispatches
					 * to offsetSet. Hold a ref on the instance to survive
					 * intervening expression evaluation. */
					MemObjSetType(pTos,MEMOBJ_NULL);
					if( pIdx ){
						PH7_MemObjStore(pIdx,&pVm->sCoalesceKey);
					}
					pVm->pCoalesceObj = pInst;
					pInst->iRef++;
					pVm->bCoalesceArmed = 1;
				}else{
					PH7_MemObjStore(&sValue,pTos);
				}
				PH7_MemObjRelease(&sValue);
				if( pIdx ){ PH7_MemObjRelease(pIdx); }
				VM_EXIT_BREAK;
			}else{
				/* offsetGet: replace pTos with the returned value. */
				PH7_MemObjRelease(pTos);
				PH7_MemObjStore(&sResult,pTos);
				pTos->nIdx = SXU32_HIGH;
				if( iP2 == 1 && VmNextIsCompoundAssign(pInstr + 1) ){
					/* `$o[$k] op= v` is php's ASSIGN_DIM_OP: offsetGet gave the
					 * current value, the op computes on it, and the result goes
					 * back through offsetSet($k, …). PHL had no write-back at
					 * all here — the fetched value carried no slot, so every
					 * compound assign on an ArrayAccess element died on
					 * "Cannot perform assignment on a constant class attribute"
					 * and stored nothing. Arm the scratch slot the op mutates;
					 * its tail (PH7_HOOK_RMW_WRITEBACK) dispatches offsetSet. */
					VmDimRmwArm(&(*pVm),pInst,pIdx,pTos,
						(void *)pStack,(void *)aInstr,(sxu32)(pc + 1));
				}else if( VmIdxFetchForWrite(pInstr,iP2)
				       && !PH7_VmDimFetchWritable(pInst->pClass) ){
					VmOverloadedElemNotice(&(*pVm),pInst->pClass,pTos);
				}
			}
			PH7_MemObjRelease(&sResult);
			if( pIdx ){
				PH7_MemObjRelease(pIdx);
			}
			VM_EXIT_BREAK;
		}
		/* Object without ArrayAccess: PHP throws fatal Error in all subscript
		 * contexts (read, isset, unset, empty). Match it. */
		if( pInst ){
			char zMsg[256];
			SyString *pName = &pInst->pClass->sName;
			sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),
				"Cannot use object of type %.*s as array",
				(int)pName->nByte,pName->zString);
			rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);
			if( pIdx ){ PH7_MemObjRelease(pIdx); }
			PH7_MemObjRelease(pTos);
			MemObjSetType(pTos,MEMOBJ_NULL);
			pTos->nIdx = SXU32_HIGH;
			if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }
			/* `break` used to resume at the NEXT instruction: the catch ran and then
			 * execution carried on inside the try block. */
			PH7_THROW_ROUTE_MIDEXPR(rc)
		}
	}
	if( (iP2 == 1 || iP2 == 3 || VM_IDX_IS_UNSET(iP2)) && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){
		if( pTos->nIdx != SXU32_HIGH ){
			ph7_value *pObj;
			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){
				/* php 8 write-context auto-vivify rules: NULL converts to array
				 * silently; FALSE converts with the 8.1 deprecation; any other
				 * scalar base — int/float/true/resource — is php's catchable
				 * "Cannot use a scalar value as an array" Error and the variable
				 * stays untouched (pre-fix the base was silently CONVERTED,
				 * corrupting e.g. `$i = 5; $i[0]++` into array(0 => 6); string
				 * bases were intercepted by the string-offset paths above). */
				/* php auto-converts false to an array with an 8.1 DEPRECATION; PHL
				 * rejects it like any other scalar base (null still auto-vivifies —
				 * it is not a bool). */
				if( (pObj->iFlags & (MEMOBJ_INT|MEMOBJ_REAL|MEMOBJ_RES|MEMOBJ_BOOL)) != 0 ){
					/* unset() has its own wording for the same base: php's
					 * "Cannot unset offset in a non-array variable". */
					const char *zErr = VM_IDX_IS_UNSET(iP2)
						? "Cannot unset offset in a non-array variable"
						: "Cannot use a scalar value as an array";
					SyBlob sErrMsg;
					SyBlobInit(&sErrMsg,&pVm->sAllocator);
					SyBlobAppend(&sErrMsg,zErr,(sxu32)SyStrlen(zErr));
					VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));
					if( pIdx ){
						PH7_MemObjRelease(pIdx);
					}
					PH7_MemObjRelease(pTos);
					pTos->nIdx = SXU32_HIGH;
					VM_EXIT_BREAK;
				}
				/* unset() CREATES NOTHING: a null/undefined base stays null where PHL
				 * converted it to an empty array (`$n = null; unset($n[0]);` left
				 * $n === []), which is also what materialised the missing intermediate
				 * in `unset($a["y"]["z"])`. Leaving the base alone, the lookup below
				 * misses, the tail loads NULL with no slot index, and the trailing
				 * unset() builtin is the no-op php's is. */
				if( !VM_IDX_IS_UNSET(iP2) ){
					PH7_MemObjToHashmap(pObj);
					PH7_MemObjLoad(pObj,pTos);
				}
			}
		}
	}
	rc = SXERR_NOTFOUND; /* Assume the index is invalid */
	/* php DEPRECATES both a null offset and a lossy-float subscript (then normalizes
	 * "" / truncates). PHL now matches php on the NULL offset (deprecate + coerce to
	 * the "" key, §2) but still rejects the lossy-FLOAT subscript with a TypeError on
	 * a READ or WRITE (iP2 0/1) — the recorded non-deprecated-surface policy. Both
	 * stay lenient in isset()/empty()/`??`/unset(), where a throw would be wrong. */
	/* An object/array key is rejected in EVERY context, including isset()/empty()/
	 * unset() where php still throws (only the wording changes) — unlike the
	 * null/float deprecations below, which stay lenient there. A resource key is
	 * accepted with a warning and becomes its integer id. */
	if( (pTos->iFlags & MEMOBJ_HASHMAP) && pIdx ){
		SyBlob sTypeMsg;
		if( VmOffsetTypeRejected(&(*pVm),pIdx,iP2,&sTypeMsg) ){
			/* Routed as a mid-expression throw, like the STRING arm above: this
			 * opcode is not a call boundary, so PARKING it let the rest of the
			 * expression run first — `str_repeat($a[$obj], 2)` reached the builtin
			 * with the NULL the abandoned read left and died on its ZPP TypeError
			 * instead, and a CAUGHT one still ran the enclosing call afterwards. */
			rc = VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg);
			PH7_MemObjRelease(pIdx);
			PH7_MemObjRelease(pTos);
			MemObjSetType(pTos,MEMOBJ_NULL);
			pTos->nIdx = SXU32_HIGH;
			if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }
			PH7_THROW_ROUTE_MIDEXPR(rc)
		}
		VmOffsetResourceWarn(&(*pVm),pIdx);
	}
	if( (pTos->iFlags & MEMOBJ_HASHMAP) && pIdx ){
		/* php DEPRECATES a null offset in EVERY subscript context except unset()
		 * (iP2==5), then normalizes it to the "" key — read, write, isset (4),
		 * empty (6), `??`/`??=` (3), the coalesce-chain probe (8), destructure
		 * (2/7). Emit it here so all of them get it, not just read/write; the
		 * lookup/insert below casts NULL->"", and a plain read miss then warns
		 * `Undefined array key ""` on the now-string pIdx (php's exact pair). */
		if( (pIdx->iFlags & MEMOBJ_NULL) && !VM_IDX_IS_UNSET(iP2) ){
			VmNullOffsetDeprecate(&(*pVm),pIdx);
		}
		/* A lossy-FLOAT subscript stays a rejected TypeError on a READ or WRITE
		 * (iP2 0/1) — the recorded non-deprecated-surface policy (§2); the lenient
		 * contexts (isset/empty/??/unset) truncate quietly as php's value does. */
		if( (iP2 == 0 || iP2 == 1)
		 && (pIdx->iFlags & MEMOBJ_REAL)
		 && pIdx->rVal != (ph7_real)(sxi64)pIdx->rVal ){
			SyBlob sErrMsg;
			SyBlobInit(&sErrMsg,&pVm->sAllocator);
			SyBlobAppend(&sErrMsg,"Cannot access offset of type float on array",
				sizeof("Cannot access offset of type float on array")-1);
			VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));
			PH7_MemObjRelease(pIdx);
			PH7_MemObjRelease(pTos);
			pTos->nIdx = SXU32_HIGH;
			VM_EXIT_BREAK;
		}
	}
	if( pTos->iFlags & MEMOBJ_HASHMAP ){
		if( iP2 == 1 || VM_IDX_IS_UNSET(iP2) ){
			/* Write-context access (iP2 = create-if-missing).  COW-separate
			 * the parent so nested writes like $b[0][0] = 99 don't leak
			 * through shared outer arrays.  Read-only loads (iP2 == 0) must
			 * NOT separate — that would defeat COW on every element read.
			 * iP2=5 is unset-context, treated like iP2=1 for arrays so the
			 * trailing unset() builtin can drop the slot via pTos->nIdx. */
			PH7_HashmapCowSeparate(&(*pVm),pTos);
		}
		/* Point to the hashmap */
		pMap = (ph7_hashmap *)pTos->x.pOther;
		if( pIdx ){
			/* Load the desired entry */
			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);
		}
		if( iP2 == 3 ){
			/* Null coalescing assign peek mode: separate only when we will
			 * actually write back. If the looked-up value is non-null, the
			 * caller's NULLC_JMP will short-circuit and no store happens, so
			 * the parent can stay shared. If the value is null or the key is
			 * missing, separate and re-lookup so the upcoming NULLC_STORE
			 * writes into our own copy. Inner levels of a nested LHS still
			 * use iP2 == 1 (eager separation), which keeps the cascade
			 * correct for the outermost write. */
			int needWrite = (rc != SXRET_OK);
			if( !needWrite && pNode ){
				ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);
				if( pVal == 0 || (pVal->iFlags & MEMOBJ_NULL) ){
					needWrite = 1;
				}
			}
			if( needWrite ){
				PH7_HashmapCowSeparate(&(*pVm),pTos);
				if( pMap != (ph7_hashmap *)pTos->x.pOther ){
					/* The map was actually copied — re-lookup so pNode points
					 * into the new map's storage. */
					pMap = (ph7_hashmap *)pTos->x.pOther;
					if( pIdx ){
						rc = PH7_HashmapLookup(pMap,pIdx,&pNode);
					}
				}
			}
		}
		/* iP2 == 5 (unset) is deliberately NOT here: php's unset() never creates
		 * the key it is about to remove, so a MISS must stay a miss. The
		 * insert-then-unset round trip was invisible on the LAST step but left
		 * every INTERMEDIATE behind -- `unset($a["y"]["z"])` grew an empty
		 * $a["y"]. The COW separation the unset context needs happened above and
		 * does not depend on this insert. */
		if( rc != SXRET_OK && (iP2 == 1 || iP2 == 3) ){
			/* Create a new empty entry */
			rc = PH7_HashmapInsert(pMap,pIdx,0);
			if( rc == SXRET_OK ){
				/* Point to the last inserted entry */
				pNode = pMap->pLast;
			}else{
				/* An append lvalue (`$a[][...] = v`) whose saturated auto-index
				 * is occupied threw php's catchable Error. Dispatch it here —
				 * falling through with a stale pMap->pLast is what silently
				 * overwrote $a[PHP_INT_MAX]. */
				PH7_DISPATCH_ENFORCE_RC(rc)
			}
		}
	}
	if( rc != SXRET_OK && pIdx && (iP2 == 2 || iP2 == 0)
	 && (pTos->iFlags & MEMOBJ_HASHMAP)
	 && !VmIdxFeedsCoalesce(pInstr) ){
		/* `$a['k'] ?? $d` compiles its LHS as a plain read (iP2 == 0) followed
		 * by NULLC/NULLC_JMP — php does NOT warn there, so peek ahead and stay
		 * silent (same guard the magic-accessor read path uses). */
		/* php warns when a missing key is READ (iP2 == 0) or destructured
		 * (iP2 == 2). isset/empty/??/unset (iP2 3-6) and write-context
		 * vivification (iP2 == 1) stay silent, as does a read on a non-array
		 * base (already diagnosed above). php prints an INT key bare and a
		 * STRING key quoted, and it decides which one the key IS by the same fold
		 * the lookup just used: `$a["10"]` looked for the INTEGER key 10, so php
		 * says `Undefined array key 10`. PHL rendered the operand as WRITTEN --
		 * quoting every string and, worse, printing `$a[false]` as the "" key it
		 * never looked in (false is the integer key 0). The canonical-numeric rule
		 * is the hashmap's own, so ask it rather than re-derive it. */
		SyBlob sMsg;
		SyBlobInit(&sMsg,&pVm->sAllocator);
		if( PH7_HashmapKeyIsInt(pIdx) ){
			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){
				PH7_MemObjToInteger(pIdx);
			}
			SyBlobFormat(&sMsg,"Undefined array key %qd",pIdx->x.iVal);
		}else{
			/* Not an integer key: PH7_HashmapKeyIsInt left it a printable string. */
			SyString sKey;
			SyStringInitFromBuf(&sKey,SyBlobData(&pIdx->sBlob),SyBlobLength(&pIdx->sBlob));
			SyBlobFormat(&sMsg,"Undefined array key \"%z\"",&sKey);
		}
		SyBlobNullAppend(&sMsg);
		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,(const char *)SyBlobData(&sMsg));
		SyBlobRelease(&sMsg);
	}
	if( (pTos->iFlags & (MEMOBJ_HASHMAP|MEMOBJ_STRING|MEMOBJ_OBJ)) == 0
	 && (iP2 == 0 || iP2 == 2)
	 && !VmIdxFeedsCoalesce(pInstr) ){
		/* Subscripting a scalar base is a WARNING in php ("Trying to access array offset
		 * on int") that yields NULL. PH7 yielded NULL in silence. */
		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Trying to access array offset on %s",
			VmArithTypeName(pTos));
	}
	if( iP2 == VM_IDX_CTX_UNSET && rc == SXRET_OK && pNode != 0
	 && (pTos->iFlags & MEMOBJ_HASHMAP) ){
		/* php's `unset($a[k])` removes the ELEMENT. PH7 left the element's value slot
		 * on the stack and let the trailing unset() builtin drop it — but dropping a
		 * SLOT unlinks everything that holds it, so `$r = &$a['k']; unset($a['k']);`
		 * destroyed $r too (`Undefined variable $r`) where php leaves it reading the
		 * value it still refers to. Unlink the node itself, which releases the value
		 * only when this element was its last holder, and leave the builtin nothing. */
		ph7_hashmap *pTarget = (ph7_hashmap *)pTos->x.pOther;
		int bDone = 0;
		if( pTarget == pVm->pGlobal && pIdx ){
			/* `$GLOBALS['x']` IS the global $x, so this is an unset of the NAME: it has
			 * to drop the symbol-table entry as well as this node, and it must not
			 * destroy the value another holder still refers to — exactly what
			 * VmUnsetVarByName does for `unset($x)`. A superglobal has no entry in the
			 * global frame and falls through to the plain node unlink below. */
			VmFrame *pGlobalFrame = pVm->pFrame;
			SyHashEntry *pNameEntry;
			while( pGlobalFrame->pParent ){
				pGlobalFrame = pGlobalFrame->pParent;
			}
			if( (pIdx->iFlags & MEMOBJ_STRING) == 0 ){
				PH7_MemObjToString(pIdx);
			}
			pNameEntry = SyHashGet(&pGlobalFrame->hVar,
				(const void *)SyBlobData(&pIdx->sBlob),SyBlobLength(&pIdx->sBlob));
			if( pNameEntry ){
				sxi32 rcUnset = VmUnsetVarByNameEx(&(*pVm),pGlobalFrame,
					(const char *)SyBlobData(&pIdx->sBlob),SyBlobLength(&pIdx->sBlob),FALSE);
				bDone = 1;
				if( rcUnset == PH7_ABORT ){
					PH7_MemObjRelease(pIdx);
					VM_EXIT_ABORT;
				}
			}
		}
		if( !bDone ){
			PH7_HashmapUnlinkNode(pNode,TRUE);
		}
		if( pIdx ){
			PH7_MemObjRelease(pIdx);
		}
		PH7_MemObjRelease(pTos);
		MemObjSetType(pTos,MEMOBJ_NULL);
		pTos->nIdx = SXU32_HIGH;
		VM_EXIT_BREAK;
	}
	if( pIdx ){
		PH7_MemObjRelease(pIdx);
	}
	if( rc == SXRET_OK ){
		/* Load entry contents */
		if( pMap->iRef < 2 ){
			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy
			 * of the entry value,rather than pointing to it.
			 */
			pTos->nIdx = SXU32_HIGH;
			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);
		}else{
			pTos->nIdx = pNode->nValIdx;
			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);
			PH7_HashmapUnref(pMap);
		}
	}else{
		/* No such entry,load NULL */
		PH7_MemObjRelease(pTos);
		pTos->nIdx = SXU32_HIGH;
	}
	VM_EXIT_BREAK;
	VM_EXIT_BREAK;
}

/*
 * OP_LOAD_MAP: body moved verbatim from the OP_LOAD_MAP arm of
 * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.
 */
PH7_PRIVATE VmOpRc VmExecOpLoadMap(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)
{
	ph7_value *pTos = pState->pTos;
	ph7_value *pStack = pState->pStack;
	VmInstr *aInstr = pState->aInstr;
	sxi32 pc = pState->pc;
	sxi32 rc;
	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);
	ph7_hashmap *pMap;
	/* Allocate a new hashmap instance */
	pMap = PH7_NewHashmap(&(*pVm),0,0);
	if( pMap == 0 ){
		VmErrorFormat(&(*pVm),PH7_CTX_ERR,
			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);
		VM_EXIT_ABORT;
	}
	if( pInstr->iP1 > 0 ){
		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */
		sxi32 rcSpread = SXRET_OK;
		/* Perform the insertion */
		while( pEntry < pTos ){
			if( pEntry[1].iFlags & MEMOBJ_AUX_SPREAD ){
				/* Array unpacking: '...$expr'. Merge entries with PHP 8.1
				 * semantics — string keys preserved (later wins), int keys
				 * renumbered. Same routine that backs array_merge. */
				if( pEntry[1].iFlags & MEMOBJ_HASHMAP ){
					sxi32 rcMerge = PH7_HashmapMerge((ph7_hashmap *)pEntry[1].x.pOther,pMap);
					if( rcMerge != SXRET_OK ){
						/* Merge failure (OOM): match the PH7_NewHashmap OOM
						 * path — emit fatal and abort, leaving no partial
						 * map dangling. */
						VmErrorFormat(&(*pVm),PH7_CTX_ERR,
							"Fatal, PH7 engine is running out of memory while spreading array at instruction #:%d",pc);
						rcSpread = PH7_ABORT;
						break;
					}
				}else if( VmValueIsTraversable(pVm,&pEntry[1]) ){
					/* Traversable unpacking (PHP 8.1): walk it into the map using the
					 * same key rules as array spread (string keys kept, int renumbered). */
					sxi32 rcW = PH7_VmIteratorWalk(&(*pVm),&pEntry[1],VmSpreadMergeStep,pMap);
					if( rcW == PH7_EXCEPTION || rcW == PH7_ABORT ){
						rcSpread = rcW;
						break;
					}
				}else{
					/* Throw a catchable Error matching PHP semantics. */
					rcSpread = VmThrowSpreadError(&(*pVm),&pEntry[1]);
					break;
				}
			}else if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){
				/* Insertion by reference */
				PH7_HashmapInsertByRef(pMap,
					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,
					(sxu32)pEntry[1].x.iVal
					);
			}else{
				/* An explicit key in an array LITERAL gets the same php diagnostics a
				 * subscript does — a float key that truncates deprecates, and so does an
				 * explicit null key. Only the subscript sites (LOAD_IDX/STORE_IDX) used to
				 * emit these, so `[1.5 => "v"]` and `[null => "v"]` were silent. A key that
				 * is ABSENT (auto-index) is a NULL slot here, not a null key, so the
				 * MEMOBJ_NULL check below is what tells the two apart. */
					if( (pEntry->iFlags & MEMOBJ_AUX_NOKEY) == 0 ){
						/* An object/array literal key is php's TypeError, a resource one
						 * warns and becomes its id — same rules as a subscript. */
						SyBlob sTypeMsg;
						if( VmOffsetTypeRejected(&(*pVm),pEntry,0,&sTypeMsg) ){
							VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg));
						}else{
							VmOffsetResourceWarn(&(*pVm),pEntry);
						}
						/* php DEPRECATES a null literal key (then normalizes to "") and
						 * rejects nothing there; PHL matches that (deprecate + fall
						 * through, PH7_HashmapInsert casts NULL->""). A lossy-FLOAT
						 * literal key still rejects with a TypeError — the recorded
						 * non-deprecated-surface policy, same as the subscript site. */
						int bNull = (pEntry->iFlags & MEMOBJ_NULL) != 0;
						int bLossyFloat = (pEntry->iFlags & MEMOBJ_REAL) != 0
							&& pEntry->rVal != (ph7_real)(sxi64)pEntry->rVal;
						if( bNull ){
							VmNullOffsetDeprecate(&(*pVm),pEntry);
						}else if( bLossyFloat ){
							const char *zErr = "Cannot access offset of type float on array";
							SyBlob sErrMsg;
							SyBlobInit(&sErrMsg,&pVm->sAllocator);
							SyBlobAppend(&sErrMsg,zErr,(sxu32)SyStrlen(zErr));
							VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));
						}
					}
				/* Standard insertion */
				PH7_HashmapInsert(pMap,
					(pEntry->iFlags & MEMOBJ_AUX_NOKEY) ? 0 /* Automatic index assign */ : pEntry,
					&pEntry[1]
				);
			}
			/* Next pair on the stack */
			pEntry += 2;
		}
		/* Pop P1 elements */
		VmPopOperand(&pTos,pInstr->iP1);
		if( rcSpread != SXRET_OK ){
			/* Discard the partially-built map and propagate the exception. */
			PH7_HashmapRelease(pMap,TRUE);
			if( rcSpread == PH7_ABORT ){
				VM_EXIT_ABORT;
			}
			{
				sxi32 iRp;
				if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){
					pc = iRp;
					VM_EXIT_BREAK;
				}
			}
			VM_EXIT_EXCEPTION;
		}
	}
	/* Push the hashmap */
	pTos++;
	pTos->nIdx = SXU32_HIGH;
	pTos->x.pOther = pMap;
	MemObjSetType(pTos,MEMOBJ_HASHMAP);
	VM_EXIT_BREAK;
	VM_EXIT_BREAK;
}

/*
 * OP_LOAD_LIST: body moved verbatim from the OP_LOAD_LIST arm of
 * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.
 */
PH7_PRIVATE VmOpRc VmExecOpLoadList(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)
{
	ph7_value *pTos = pState->pTos;
	ph7_value *pStack = pState->pStack;
	VmInstr *aInstr = pState->aInstr;
	sxi32 pc = pState->pc;
	sxi32 rc;
	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);
	ph7_value *pEntry;
	sxi32 rcEnforce = SXRET_OK;
	if( pInstr->iP1 <= 0 ){
		/* Empty list,break immediately */
		VM_EXIT_BREAK;
	}
	pEntry = &pTos[-pInstr->iP1+1];
#ifdef UNTRUST
	if( &pEntry[-1] < pStack ){
		VM_EXIT_ABORT;
	}
#endif
	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){
		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;
		ph7_hashmap_node *pNode;
		ph7_value sKey,*pObj;
		/* Start Copying */
		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);
		while( pEntry <= pTos ){
			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){
				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);
				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){
					int bTyped = SyHashTotalEntry(&pVm->hTypedSlot) > 0
						&& SyHashGet(&pVm->hTypedSlot,(const void *)&pEntry->nIdx,sizeof(sxu32)) != 0;
					if( rc != SXRET_OK ){
						/* Undefined array key */
						char zMsg[128];
						SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)sKey.x.iVal);
						PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);
					}
					if( !bTyped ){
						if( rc == SXRET_OK ){
							/* Store node value */
							PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);
						}else{
							PH7_MemObjRelease(pObj);
						}
					}else{
						/* Typed/readonly property target (`[$o->p] = [...]`): a
						 * direct slot write would bypass the typed-slot table, so
						 * enforce on a temp first — a TypeError leaves the property
						 * untouched, and a missing key assigns null, which a
						 * non-nullable type rejects exactly like php (warning, then
						 * "Cannot assign null to property ... of type ..."). */
						ph7_value sVal;
						PH7_MemObjInit(&(*pVm),&sVal);
						if( rc == SXRET_OK ){
							PH7_HashmapExtractNodeValue(pNode,&sVal,TRUE);
						}
						rcEnforce = VmEnforcePropertyTypeOnStore(&(*pVm),pEntry->nIdx,&sVal,0);
						if( rcEnforce != SXRET_OK ){
							/* Thrown: stop assigning (php aborts the list at the
							 * first failing element), settle the stack, route. */
							PH7_MemObjRelease(&sVal);
							break;
						}
						PH7_MemObjStore(&sVal,pObj);
						PH7_MemObjRelease(&sVal);
					}
				}
			}
			sKey.x.iVal++; /* Next numeric index */
			pEntry++;
		}
	}else{
		/* Source is not an array: php warns first (silencing ONLY null — a bool
		 * source warns too, php 8), then assigns null to every target. A typed
		 * property target receives that null THROUGH enforcement, so a
		 * non-nullable type throws "Cannot assign null to property ..." exactly
		 * like php instead of silently nulling the slot. PHL DIVERGENCE: bool
		 * FALSE stays silent (php warns) — it is the end-of-array sentinel of
		 * the documented each() extension's `while (list(..) = each($a))`
		 * idiom, which would otherwise warn on every normal loop exit. */
		ph7_value *pObj;
		int bFalseSrc = (pTos[-pInstr->iP1].iFlags & MEMOBJ_BOOL) != 0
			&& pTos[-pInstr->iP1].x.iVal == 0;
		if( (pTos[-pInstr->iP1].iFlags & MEMOBJ_NULL) == 0 && !bFalseSrc ){
			VmWarnCannotUseAsArray(&(*pVm),pTos[-pInstr->iP1].iFlags);
		}
		while( pEntry <= pTos ){
			if( pEntry->nIdx != SXU32_HIGH ){
				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){
					int bTyped = SyHashTotalEntry(&pVm->hTypedSlot) > 0
						&& SyHashGet(&pVm->hTypedSlot,(const void *)&pEntry->nIdx,sizeof(sxu32)) != 0;
					if( !bTyped ){
						PH7_MemObjRelease(pObj);
					}else{
						ph7_value sVal;
						PH7_MemObjInit(&(*pVm),&sVal);
						rcEnforce = VmEnforcePropertyTypeOnStore(&(*pVm),pEntry->nIdx,&sVal,0);
						if( rcEnforce != SXRET_OK ){
							PH7_MemObjRelease(&sVal);
							break;
						}
						PH7_MemObjStore(&sVal,pObj);
						PH7_MemObjRelease(&sVal);
					}
				}
			}
			pEntry++;
		}
	}
	if( rcEnforce != SXRET_OK ){
		/* Settle this op's own operands: the P1 entries AND the source value —
		 * its statement-level OP_POP is skipped when a catch resumes at the
		 * landing pad, so leaving it would leak one operand slot per caught
		 * throw. A NESTED destructure can still have the outer list's operands
		 * abandoned above the try's base, so on an in-place catch drain to the
		 * catching try's recorded depth (like the fetch-point router and the
		 * generator inject path), not just our own pops. */
		VmPopOperand(&pTos,pInstr->iP1 + 1);
		if( rcEnforce == PH7_ABORT ){
			VM_EXIT_ABORT;
		}
		{
			sxi32 _iRpL;
			PH7_INLINE_RESUME_BREAK()
			if( VmRecordedResume(pVm,&_iRpL,pState->pEntryFrame,aInstr) ){
				PH7_RESUME_DRAIN()
				pc = _iRpL;
				VM_EXIT_BREAK;
			}
		}
		VM_EXIT_EXCEPTION;
	}
	VmPopOperand(&pTos,pInstr->iP1);
	VM_EXIT_BREAK;
	VM_EXIT_BREAK;
}

/*
 * OP_UNSET_VAR: body moved verbatim from the OP_UNSET_VAR arm of
 * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.
 */
PH7_PRIVATE VmOpRc VmExecOpUnsetVar(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)
{
	ph7_value *pTos = pState->pTos;
	ph7_value *pStack = pState->pStack;
	VmInstr *aInstr = pState->aInstr;
	sxi32 pc = pState->pc;
	sxi32 rc;
	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);
	/* unset($name): p3 is the variable name. Drops the NAME only — see VmUnsetVarByName */
	SyString *pName = (SyString *)pInstr->p3;
	if( pName && pVm->pFrame ){
		/* Inside a try{} the VM pushes an EXCEPTION frame; variables live in the body
		 * frame below it, so skip past it exactly as every other variable path does.
		 * Without this, unset($x) inside a try silently found nothing and did nothing. */
		VmFrame *pVarFrame = VmSkipExceptionFrames(pVm->pFrame);
		sxi32 rcU = VmUnsetVarByName(&(*pVm),pVarFrame,pName->zString,pName->nByte);
		if( rcU == PH7_ABORT ){
			VM_EXIT_ABORT;
		}
		/* Releasing the last holder can run a __destruct(), and that destructor may
		 * throw. Such a throw is PARKED in nBoundaryRc by the boundary rail; consume it
		 * here and route it, or the catch runs and execution resumes inside the try
		 * ("resumed-dtor" instead of php's "caught-dtor"). */
		if( pVm->nBoundaryRc != 0 ){
			rc = pVm->nBoundaryRc;
			pVm->nBoundaryRc = 0;
			if( rc == PH7_ABORT ){
				VM_EXIT_ABORT;
			}
			PH7_THROW_ROUTE_MIDEXPR(rc)
		}
	}
	VM_EXIT_BREAK;
	VM_EXIT_BREAK;
}
