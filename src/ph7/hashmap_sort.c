/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
/*
 * Section:
 *    Hashmap (array) sorting: the SQLite-derived merge sort, its comparator
 *    set and the sort()/usort()/ksort() builtin family.
 * Status:
 *    Stable.
 */
/* SPDX-SnippetBegin */
/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */
/* SPDX-License-Identifier: blessing */
/*
 * Merge sort.
 * The merge sort implementation is based on the one found in the SQLite3 source tree.
 * Status: Public domain
 */
/* Node comparison callback signature */
/*
** Inputs:
**   a:       A sorted, null-terminated linked list.  (May be null).
**   b:       A sorted, null-terminated linked list.  (May be null).
**   cmp:     A pointer to the comparison function.
**
** Return Value:
**   A pointer to the head of a sorted list containing the elements
**   of both a and b.
**
** Side effects:
**   The "next","prev" pointers for elements in the lists a and b are
**   changed.
*/
static ph7_hashmap_node * HashmapNodeMerge(ph7_hashmap_node *pA,ph7_hashmap_node *pB,ProcNodeCmp xCmp,void *pCmpData)
{
	ph7_hashmap_node result,*pTail;
    /* Prevent compiler warning */
	result.pNext = result.pPrev = 0;
	pTail = &result;
	while( pA && pB ){
		if( xCmp(pA,pB,pCmpData) <= 0 ){
			pTail->pPrev = pA;
			pA->pNext = pTail;
			pTail = pA;
			pA = pA->pPrev;
		}else{
			pTail->pPrev = pB;
			pB->pNext = pTail;
			pTail = pB;
			pB = pB->pPrev;
		}
	}
	if( pA ){
		pTail->pPrev = pA;
		pA->pNext = pTail;
	}else if( pB ){
		pTail->pPrev = pB;
		pB->pNext = pTail;
	}else{
		pTail->pPrev = pTail->pNext = 0;
	}
	return result.pPrev;
}
/*
** Inputs:
**   Map:       Input hashmap
**   cmp:       A comparison function.
**
** Return Value:
**   Sorted hashmap.
**
** Side effects:
**   The "next" pointers for elements in list are changed.
*/
#define N_SORT_BUCKET  32
PH7_PRIVATE sxi32 HashmapMergeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData)
{
	ph7_hashmap_node *a[N_SORT_BUCKET], *p,*pIn;
	sxu32 i;
	SyZero(a,sizeof(a));
	/* Point to the first inserted entry */
	pIn = pMap->pFirst;
	while( pIn ){
		p = pIn;
		pIn = p->pPrev;
		p->pPrev = 0;
		for(i=0; i<N_SORT_BUCKET-1; i++){
			if( a[i]==0 ){
				a[i] = p;
				break;
			}else{
				p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);
				a[i] = 0;
			}
		}
		if( i==N_SORT_BUCKET-1 ){
			/* To get here, there need to be 2^(N_SORT_BUCKET) elements in he input list.
			 * But that is impossible.
			 */
			a[i] = HashmapNodeMerge(a[i], p,xCmp,pCmpData);
		}
	}
	p = a[0];
	for(i=1; i<N_SORT_BUCKET; i++){
		/* Higher-index buckets hold EARLIER-inserted (and larger) runs, so the
		 * bucket must be the LEFT operand: HashmapNodeMerge favors its left arg on
		 * a tie (cmp <= 0), and php's sorts are stable (PHP 8.0+) — equal elements
		 * keep their original order. Passing p (the later elements) on the left
		 * reversed equal runs (e.g. usort of five tie-keyed items moved the last to
		 * the front). */
		p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);
	}
	p->pNext = 0;
	/* Reflect the change */
	pMap->pFirst = p;
	/* Reset the loop cursor */
	pMap->pCur = pMap->pFirst;
	return SXRET_OK;
}
/* SPDX-SnippetEnd */
/*
 * Coerce one operand of a STRING-flag comparison (SORT_STRING and friends), the
 * way php's zval_get_string() does. An ARRAY warns "Array to string conversion"
 * and renders as "Array"; an object with no __toString() raises php's catchable
 * "could not be converted to string" Error and renders as the EMPTY string --
 * which is why php's array comes out fully SORTED after the throw, with the
 * object first. PHL used to render it as the literal "Object" and sort on that,
 * silently.
 *
 * A comparator has no status channel, so the Error is raised once per sort and
 * flagged on the VM through iCmpCallbackExc -- the rail a throwing user callback
 * already uses; every flag-sort driver clears the flag before its merge sort and
 * answers PH7_EXCEPTION after it (HashmapFlagSortStatus), and array_unique() does
 * the same around its walk. The flag is also what keeps the second and later
 * comparisons from raising the same Error again.
 */
static void HashmapFlagStringify(ph7_value *pVal)
{
	ph7_vm *pVm = pVal->pVm;
	sxi32 rc = PH7_EXCEPTION;
	if( (pVal->iFlags & MEMOBJ_OBJ) && pVm
	 && (pVm->iCmpCallbackExc != 0 || PH7_CALLBACK_UNWOUND(pVm->nBoundaryRc)) ){
		/* This sort already coerced an object once and it did not succeed: php
		 * raises the refusal Error once, and it enters no PHP function at all
		 * while an exception is pending (zend_call_function bails on
		 * EG(exception)), so neither arm runs again -- the operand just renders
		 * as the empty string a refused cast gives. The raise-once guard used to
		 * cover only the NOT-STRINGABLE arm, so a throwing __toString() body went
		 * round again on the next pair; uncaught, it reported the fatal once per
		 * comparison, and after a catch had run in place the second throw was
		 * uncaught and killed a script php merely says "caught" in.
		 * OBJECTS only: blanking an int or a string here would re-order the rest
		 * of the array. */
		PH7_MemObjRelease(pVal);
		MemObjSetType(pVal,MEMOBJ_STRING);
		return;
	}
	if( !PH7_MemObjIsNotStringable(pVal) ){
		rc = PH7_MemObjToStringUV(pVal);
		if( rc == SXRET_OK ){
			return;
		}
		/* A __toString() that THREW. Same shape as a refused cast from here on. */
	}else{
		rc = PH7_MemObjToStringUV(pVal); /* raises php's Error */
	}
	if( pVm ){
		/* Latch the STATUS the coercion answered, so an UNCAUGHT throw (or an
		 * exit()) out of __toString() leaves the sort with PH7_ABORT rather than
		 * a downgraded PH7_EXCEPTION. */
		pVm->iCmpCallbackExc = PH7_CALLBACK_UNWOUND(rc) ? rc : PH7_EXCEPTION;
	}
	PH7_MemObjRelease(pVal);
	MemObjSetType(pVal,MEMOBJ_STRING);
}
/*
 * Node comparison callback.
 * used-by: [sort(),asort(),...]
 */
/*
 * Compare two scalar values under an EXPLICIT php sort base type (never 0 —
 * SORT_REGULAR is handled by the callers, which differ for keys vs values):
 *   SORT_NUMERIC 1 · SORT_STRING 2 · SORT_LOCALE_STRING 5 · SORT_NATURAL 6,
 * with bFold applying SORT_FLAG_CASE. PHL has no locale tables, so
 * SORT_LOCALE_STRING behaves like SORT_STRING. Mutates both operands (numeric or
 * string cast); the caller owns and releases them.
 */
static sxi32 HashmapScalarFlagCmp(ph7_value *pA,ph7_value *pB,int base,int bFold)
{
	sxi32 rc;
	if( base == 1 ){
		/* SORT_NUMERIC compares as DOUBLES. php's numeric_compare_function is
		 * `zval_get_double(a)` vs `zval_get_double(b)` under ZEND_THREEWAY_COMPARE
		 * — not a value comparison over whatever type each operand happens to
		 * settle on. Two consequences PHL got wrong by folding to a NUMBER and
		 * calling the ordinary comparator: the diagnostic named the wrong type
		 * (an object warned "could not be converted to int" where php says
		 * "to float"), and integers above 2^53 were ordered EXACTLY where php's
		 * doubles tie — `sort([PHP_INT_MAX, PHP_INT_MAX-1], SORT_NUMERIC)` left
		 * php's stable sort's original order and PHL re-ordered them. Matching php
		 * means adopting its precision loss, which is what parity is (§10).
		 * The == / < shape is ZEND_THREEWAY_COMPARE's, so NaN — equal to nothing,
		 * less than nothing — answers 1 in both engines. */
		ph7_real rA,rB;
		PH7_MemObjToReal(pA);
		PH7_MemObjToReal(pB);
		rA = pA->rVal;
		rB = pB->rVal;
		rc = (rA == rB) ? 0 : ((rA < rB) ? -1 : 1);
	}else{
		/* SORT_STRING (2) / SORT_LOCALE_STRING (5) / SORT_NATURAL (6) */
		const char *zA,*zB;
		sxu32 nA,nB,nMin,i;
		if( (pA->iFlags & MEMOBJ_STRING) == 0 ){ HashmapFlagStringify(pA); }
		if( (pB->iFlags & MEMOBJ_STRING) == 0 ){ HashmapFlagStringify(pB); }
		zA = (const char *)SyBlobData(&pA->sBlob);
		zB = (const char *)SyBlobData(&pB->sBlob);
		nA = SyBlobLength(&pA->sBlob);
		nB = SyBlobLength(&pB->sBlob);
		if( base == 6 ){
			rc = PH7_StrNatCmp(zA,(int)nA,zB,(int)nB,bFold);
		}else{
			/* Lexicographic comparison (binary-safe), case-folded on request. */
			nMin = nA < nB ? nA : nB;
			rc = 0;
			for( i = 0 ; i < nMin ; ++i ){
				int ca = (unsigned char)zA[i];
				int cb = (unsigned char)zB[i];
				if( bFold ){ ca = SyToLower(ca); cb = SyToLower(cb); }
				if( ca != cb ){ rc = ca < cb ? -1 : 1; break; }
			}
			if( rc == 0 ){
				if( nA < nB ) rc = -1;
				else if( nA > nB ) rc = 1;
			}
		}
	}
	return rc;
}
/*
 * Are two live values equal under an array_unique() sort_flags? A non-mutating
 * wrapper (works on private copies): base 0 = SORT_REGULAR loose comparison,
 * explicit flags route through HashmapScalarFlagCmp. Used by array_unique.
 */
PH7_PRIVATE int HashmapValueFlagEqual(ph7_vm *pVm,ph7_value *pA,ph7_value *pB,int base,int bFold)
{
	ph7_value sA,sB;
	sxi32 rc;
	PH7_MemObjInit(pVm,&sA);
	PH7_MemObjInit(pVm,&sB);
	PH7_MemObjStore(pA,&sA);
	PH7_MemObjStore(pB,&sB);
	if( base == 0 ){
		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0); /* SORT_REGULAR: loose comparison */
	}else{
		rc = HashmapScalarFlagCmp(&sA,&sB,base,bFold);
	}
	PH7_MemObjRelease(&sA);
	PH7_MemObjRelease(&sB);
	return rc == 0;
}
/*
 * Compare two node VALUES under php's sort_flags (base 0 = SORT_REGULAR uses the
 * standard value comparison; explicit flags route through HashmapScalarFlagCmp).
 */
static sxi32 HashmapFlagValueCmp(ph7_hashmap_node *pA,ph7_hashmap_node *pB,sxi32 iFlags)
{
	ph7_value sA,sB;
	int base = iFlags & ~8;      /* strip SORT_FLAG_CASE */
	int bFold = (iFlags & 8) != 0;
	sxi32 rc;
	if( base == 0 ){
		/* SORT_REGULAR */
		return HashmapNodeCmp(pA,pB,FALSE);
	}
	PH7_MemObjInit(pA->pMap->pVm,&sA);
	PH7_MemObjInit(pA->pMap->pVm,&sB);
	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);
	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);
	rc = HashmapScalarFlagCmp(&sA,&sB,base,bFold);
	PH7_MemObjRelease(&sA);
	PH7_MemObjRelease(&sB);
	return rc;
}
/*
 * A sort comparison can run USER code with no way to report what it did: an
 * object operand's `__toString()` is reached by SORT_REGULAR's value comparison
 * as well as by the string flags' coercion, and that body can throw or exit().
 * PH7_MemObjCmp only ever answers an ORDERING, so the status is read off the VM
 * instead — every C->PHP dispatch parks an unwind in nBoundaryRc (VmBoundaryPark)
 * and nothing inside a builtin consumes it, so it is still there when the
 * comparison returns.
 *
 * Two things follow, and both were missing: the merge sort must STAND DOWN (a
 * throwing `__toString()` ran again on the next pair -- and since the enclosing
 * catch had already run in place, the second throw was UNCAUGHT and killed a
 * script php merely reports "caught" in), and the driver must answer with that
 * status rather than `true`.
 *
 * Deliberately NOT triggered by a not-stringable object's own coercion Error:
 * that one parks nothing and is flagged by HashmapFlagStringify, and php goes on
 * comparing after it (its array comes out fully sorted, the object first).
 */
static void HashmapCmpLatch(ph7_vm *pVm)
{
	if( PH7_CALLBACK_UNWOUND(pVm->nBoundaryRc)
	 && (pVm->iCmpCallbackExc == 0 || pVm->nBoundaryRc == PH7_ABORT) ){
		pVm->iCmpCallbackExc = pVm->nBoundaryRc;
	}
}
static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)
{
	ph7_vm *pVm = pA->pMap->pVm;
	sxi32 rc;
	if( pCmpData == 0 ){
		/* SORT_REGULAR fast path */
		rc = HashmapNodeCmp(pA,pB,FALSE);
	}else{
		rc = HashmapFlagValueCmp(pA,pB,SX_PTR_TO_INT(pCmpData));
	}
	HashmapCmpLatch(pVm);
	return rc;
}
/*
 * Materialise a node's KEY as a scalar ph7_value (int key -> integer, string key
 * -> string) for a flag-aware key comparison.
 */
static void HashmapNodeKeyToValue(ph7_hashmap_node *pNode,ph7_value *pOut)
{
	if( pNode->iType == HASHMAP_INT_NODE ){
		PH7_MemObjInitFromInt(pNode->pMap->pVm,pOut,pNode->xKey.iKey);
	}else{
		PH7_MemObjInitFromString(pNode->pMap->pVm,pOut,0);
		PH7_MemObjStringAppend(pOut,(const char *)SyBlobData(&pNode->xKey.sKey),
			SyBlobLength(&pNode->xKey.sKey));
	}
}
/*
 * Shared key comparison for ksort()/krsort() under SORT_REGULAR: php compares
 * two array KEYS exactly the way it compares two VALUES, so materialise them
 * and hand them to the standard comparison — the same one sort()/`<=>` use.
 *
 * The hand-rolled version this replaces got the mixed int/string case right but
 * compared two STRING keys BYTEWISE, so two numeric strings sorted by their
 * bytes: ksort(['10.0'=>1,'9.0'=>2]) answered ['10.0','9.0'] where php answers
 * ['9.0','10.0'], ksort(['1e3'=>1,'20'=>2]) put 1e3 (1000) first, and keys that
 * compare EQUAL ('1.0', '01', 1) lost php's stable order.
 */
static sxi32 HashmapKeyNodeCmp(ph7_hashmap_node *pA,ph7_hashmap_node *pB)
{
	ph7_value sA,sB;
	sxi32 rc;
	if( pA->iType == HASHMAP_INT_NODE && pB->iType == HASHMAP_INT_NODE ){
		/* Two integer keys: the common case, and no allocation needed */
		return pA->xKey.iKey < pB->xKey.iKey ? -1 : (pA->xKey.iKey > pB->xKey.iKey ? 1 : 0);
	}
	HashmapNodeKeyToValue(pA,&sA);
	HashmapNodeKeyToValue(pB,&sB);
	rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);
	PH7_MemObjRelease(&sA);
	PH7_MemObjRelease(&sB);
	return rc;
}
/*
 * Compare two node KEYS under php's sort_flags. base 0 = SORT_REGULAR keeps the
 * php-8 mixed int/string key semantics (HashmapKeyNodeCmp); explicit flags route
 * the materialised keys through HashmapScalarFlagCmp.
 */
static sxi32 HashmapFlagKeyCmp(ph7_hashmap_node *pA,ph7_hashmap_node *pB,sxi32 iFlags)
{
	ph7_value sA,sB;
	int base = iFlags & ~8;      /* strip SORT_FLAG_CASE */
	int bFold = (iFlags & 8) != 0;
	sxi32 rc;
	if( base == 0 ){
		return HashmapKeyNodeCmp(pA,pB);
	}
	HashmapNodeKeyToValue(pA,&sA);
	HashmapNodeKeyToValue(pB,&sB);
	rc = HashmapScalarFlagCmp(&sA,&sB,base,bFold);
	PH7_MemObjRelease(&sA);
	PH7_MemObjRelease(&sB);
	return rc;
}
/*
 * Node comparison callback: Compare nodes by keys only.
 * used-by: [ksort()]
 */
static sxi32 HashmapCmpCallback2(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)
{
	ph7_vm *pVm = pA->pMap->pVm;
	sxi32 rc;
	if( pCmpData == 0 ){
		rc = HashmapKeyNodeCmp(pA,pB);
	}else{
		rc = HashmapFlagKeyCmp(pA,pB,SX_PTR_TO_INT(pCmpData));
	}
	HashmapCmpLatch(pVm);
	return rc;
}
/*
 * Node comparison callback.
 * Used by: [rsort(),arsort()];
 */
static sxi32 HashmapCmpCallback3(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)
{
	ph7_vm *pVm = pA->pMap->pVm;
	sxi32 rc;
	if( pCmpData == 0 ){
		/* SORT_REGULAR fast path, reversed */
		rc = -HashmapNodeCmp(pA,pB,FALSE);
	}else{
		rc = -HashmapFlagValueCmp(pA,pB,SX_PTR_TO_INT(pCmpData));
	}
	HashmapCmpLatch(pVm);
	return rc;
}
/*
 * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.
 * used-by: [usort(),uasort()]
 */
static sxi32 HashmapCmpCallback4(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)
{
	ph7_value sResult,*pCallback;
	ph7_value *pV1,*pV2;
	ph7_value *apArg[2];  /* Callback arguments */
	sxi32 rc;
	/* Point to the desired callback */
	pCallback = (ph7_value *)pCmpData;
	if( pA->pMap->pVm->iCmpCallbackExc ){
		/* A previous comparison already raised: stop invoking the callback so
		 * the exception is not thrown again, and let the sort wind down. */
		return 0;
	}
	/* initialize the result value */
	PH7_MemObjInit(pA->pMap->pVm,&sResult);
	/* Extract nodes values */
	pV1 = HashmapExtractNodeValue(pA);
	pV2 = HashmapExtractNodeValue(pB);
	apArg[0] = pV1;
	apArg[1] = pV2;
	/* Invoke the callback */
	rc = PH7_VmCallCallbackByValue(pA->pMap->pVm,pCallback,2,apArg,&sResult,0);
	if( PH7_CALLBACK_UNWOUND(rc) ){
		/* The comparator did not RETURN: latch the STATUS so the sort driver
		 * aborts and propagates exactly it (an UNCAUGHT throw is PH7_ABORT, and
		 * testing only PH7_EXCEPTION left the sort running -- re-entering the
		 * comparator, and the fatal report, for every remaining pair), and order
		 * this pair arbitrarily for the rest of the run. */
		pA->pMap->pVm->iCmpCallbackExc = rc;
		rc = 0;
	}else if( rc != SXRET_OK ){
		/* An error occured while calling user defined function [i.e: not defined] */
		rc = -1; /* Set a dummy result */
	}else{
		/* Extract callback result */
		if((sResult.iFlags & MEMOBJ_INT) == 0 ){
			/* Perform an int cast */
			PH7_MemObjToInteger(&sResult);
		}
		rc = (sxi32)sResult.x.iVal;
	}
	PH7_MemObjRelease(&sResult);
	/* Callback result */
	return rc;
}
/*
 * Node comparison callback: Compare nodes by keys only.
 * used-by: [krsort()]
 */
static sxi32 HashmapCmpCallback5(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)
{
	if( pCmpData == 0 ){
		return -HashmapKeyNodeCmp(pA,pB); /* Reverse result */
	}
	return -HashmapFlagKeyCmp(pA,pB,SX_PTR_TO_INT(pCmpData));
}
/*
 * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.
 * used-by: [uksort()]
 */
static sxi32 HashmapCmpCallback6(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)
{
	ph7_value sResult,*pCallback;
	ph7_value *apArg[2];  /* Callback arguments */
	ph7_value sK1,sK2;
	sxi32 rc;
	/* Point to the desired callback */
	pCallback = (ph7_value *)pCmpData;
	if( pA->pMap->pVm->iCmpCallbackExc ){
		/* A previous comparison already raised: stop invoking the callback so
		 * the exception is not thrown again, and let the sort wind down. */
		return 0;
	}
	/* initialize the result value */
	PH7_MemObjInit(pA->pMap->pVm,&sResult);
	PH7_MemObjInit(pA->pMap->pVm,&sK1);
	PH7_MemObjInit(pA->pMap->pVm,&sK2);
	/* Extract nodes keys */
	PH7_HashmapExtractNodeKey(pA,&sK1);
	PH7_HashmapExtractNodeKey(pB,&sK2);
	apArg[0] = &sK1;
	apArg[1] = &sK2;
	/* Mark keys as constants */
	sK1.nIdx = SXU32_HIGH;
	sK2.nIdx = SXU32_HIGH;
	/* Invoke the callback */
	rc = PH7_VmCallCallbackByValue(pA->pMap->pVm,pCallback,2,apArg,&sResult,0);
	if( PH7_CALLBACK_UNWOUND(rc) ){
		/* The comparator did not RETURN: latch the STATUS so the sort driver
		 * aborts and propagates exactly it (an UNCAUGHT throw is PH7_ABORT, and
		 * testing only PH7_EXCEPTION left the sort running -- re-entering the
		 * comparator, and the fatal report, for every remaining pair), and order
		 * this pair arbitrarily for the rest of the run. */
		pA->pMap->pVm->iCmpCallbackExc = rc;
		rc = 0;
	}else if( rc != SXRET_OK ){
		/* An error occured while calling user defined function [i.e: not defined] */
		rc = -1; /* Set a dummy result */
	}else{
		/* Extract callback result */
		if((sResult.iFlags & MEMOBJ_INT) == 0 ){
			/* Perform an int cast */
			PH7_MemObjToInteger(&sResult);
		}
		rc = (sxi32)sResult.x.iVal;
	}
	PH7_MemObjRelease(&sResult);
	PH7_MemObjRelease(&sK1);
	PH7_MemObjRelease(&sK2);
	/* Callback result */
	return rc;
}
/*
 * Node comparison callback: Random node comparison.
 * used-by: [shuffle()]
 */
PH7_PRIVATE sxi32 HashmapCmpCallback7(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)
{
	sxu32 n;
	SXUNUSED(pB); /* cc warning */
	SXUNUSED(pCmpData);
	/* Grab a random number from the MT19937 generator so shuffle()/array_rand()
	 * respond to srand()/mt_srand() (reproducible under a seed), like php. This
	 * is a random-comparator merge sort, not php's Fisher-Yates, so the ordering
	 * is deterministic-under-seed but not value-parity with php. */
	n = PH7_VmMtRand(pA->pMap->pVm);
	/* if the random number is odd then the first node 'pA' is greater then
	 * the second node 'pB'. Otherwise the reverse is assumed.
	 */
	return n&1 ? 1 : -1;
}
/*
 * Rehash all nodes keys after a merge-sort have been applied.
 * Used by [sort(),usort() and rsort()].
 */
PH7_PRIVATE void HashmapSortRehash(ph7_hashmap *pMap)
{
	ph7_hashmap_node *p,*pLast;
	sxu32 i;
	/* Rehash all entries */
	pLast = p = pMap->pFirst;
	pMap->iNextIdx = 0;
	pMap->bIntKeySeen = 0; /* Reset the automatic index */
	i = 0;
	for( ;; ){
		if( i >= pMap->nEntry ){
			pMap->pLast = pLast; /* Fix the last link broken by the merge-sort */
			break;
		}
		if( p->iType == HASHMAP_BLOB_NODE ){
			/* Do not maintain index association as requested by the PHP specification */
			SyBlobRelease(&p->xKey.sKey);
			/* Change key type */
			p->iType = HASHMAP_INT_NODE;
		}
		HashmapRehashIntNode(p);
		/* Point to the next entry */
		i++;
		pLast = p;
		p = p->pPrev; /* Reverse link */
	}
}
/*
 * Array functions implementation.
 * Status:
 *  Stable.
 */
/*
 * Reset / report the comparator-throw flag around a FLAG sort. The string sort
 * flags coerce their operands user-visibly (HashmapScalarFlagCmp), and a
 * not-stringable object raises php's Error there; the comparator can only flag
 * it, so every flag-sort driver clears the flag before its merge sort and
 * answers PH7_EXCEPTION after it. php's array is sorted after the throw too, so
 * the rehash still runs.
 */
static sxi32 HashmapFlagSortStatus(ph7_context *pCtx)
{
	if( pCtx->pVm->iCmpCallbackExc ){
		sxi32 rcExc = pCtx->pVm->iCmpCallbackExc;
		pCtx->pVm->iCmpCallbackExc = 0;
		pCtx->nThrowRc = rcExc;
		return rcExc;
	}
	return PH7_OK;
}
/*
 * bool sort(array &$array[,int $sort_flags = SORT_REGULAR ] )
 * Sort an array.
 * Parameters
 *  $array
 *   The input array.
 * $sort_flags
 *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:
 *  Sorting type flags:
 *   SORT_REGULAR - compare items normally (don't change types)
 *   SORT_NUMERIC - compare items numerically
 *   SORT_STRING - compare items as strings
 * Return
 *  TRUE on success or FALSE on failure.
 *
 */
PH7_PRIVATE int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap;
	/* Make sure we are dealing with a valid hashmap */
	if( nArg < 1 || !ph7_value_is_array(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the internal representation of the input hashmap */
	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	if( pMap->nEntry > 1 ){
		sxi32 iCmpFlags = 0;
		if( nArg > 1 ){
			/* Extract comparison flags */
			iCmpFlags = ph7_value_to_int(apArg[1]);
		}
		/* Do the merge sort */
		pCtx->pVm->iCmpCallbackExc = 0;
		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));
		/* Rehash [Do not maintain index association as requested by the PHP specification] */
		HashmapSortRehash(pMap);
	}else if( pMap->nEntry == 1 ){
		/* php reindexes even a single-element array: a string key becomes 0 */
		HashmapSortRehash(pMap);
	}
	{
		sxi32 rcCmp = HashmapFlagSortStatus(pCtx);
		if( rcCmp != PH7_OK ){
			return rcCmp;
		}
	}
	/* All done,return TRUE */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * bool asort(array &$array[,int $sort_flags = SORT_REGULAR ] )
 *  Sort an array and maintain index association.
 * Parameters
 *  $array
 *   The input array.
 * $sort_flags
 *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:
 *  Sorting type flags:
 *   SORT_REGULAR - compare items normally (don't change types)
 *   SORT_NUMERIC - compare items numerically
 *   SORT_STRING - compare items as strings
 * Return
 *  TRUE on success or FALSE on failure.
 */
PH7_PRIVATE int ph7_hashmap_asort(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap;
	/* PHP 8: ArgumentCountError if no arguments */
	if( nArg < 1 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"asort() expects at least 1 argument, 0 given"
			);
	}
	/* PHP 8: TypeError if first argument is not an array */
	if( !ph7_value_is_array(apArg[0]) ){
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"asort(): Argument #1 ($array) must be of type array, %s given",
			ph7_type_name(apArg[0])
			);
	}
	/* Point to the internal representation of the input hashmap */
	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	if( pMap->nEntry > 1 ){
		sxi32 iCmpFlags = 0;
		if( nArg > 1 ){
			/* Extract comparison flags */
			iCmpFlags = ph7_value_to_int(apArg[1]);
		}
		/* Do the merge sort */
		pCtx->pVm->iCmpCallbackExc = 0;
		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));
		/* Fix the last link broken by the merge */
		while(pMap->pLast->pPrev){
			pMap->pLast = pMap->pLast->pPrev;
		}
	}
	{
		sxi32 rcCmp = HashmapFlagSortStatus(pCtx);
		if( rcCmp != PH7_OK ){
			return rcCmp;
		}
	}
	/* All done,return TRUE */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * bool natsort(array &$array)
 * bool natcasesort(array &$array)
 *  Sort an array with php's "natural order" algorithm, maintaining index
 *  association: exactly asort() under SORT_NATURAL (plus SORT_FLAG_CASE for the
 *  case-insensitive twin), which is how php implements them too.
 *
 *  They used to be PRELUDE wrappers over `uasort($array, 'strnatcmp')`, and that
 *  is a different function: uasort hands each element to a userland callback, so
 *  every element had to satisfy strnatcmp's `string` ZPP row. php's natsort
 *  COERCES each element the way any string comparison does, so
 *  `natsort([10, "9", null])` — nothing exotic, just a mixed array — was a
 *  TypeError in PHL and a sorted array in php, and an object with no
 *  __toString() answered strnatcmp's ZPP TypeError instead of php's coercion
 *  Error. Routing through the flag comparator picks up HashmapFlagStringify,
 *  which already renders arrays with php's "Array to string conversion" warning
 *  and raises the coercion Error once per sort.
 */
PH7_PRIVATE int ph7_hashmap_natsort(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zName = ph7_function_name(pCtx);
	/* natcasesort() is the SORT_FLAG_CASE twin; match the whole name, not a byte. */
	int bFold = zName && SyStrlen(zName) == sizeof("natcasesort")-1
		&& SyMemcmp(zName,"natcasesort",sizeof("natcasesort")-1) == 0;
	ph7_hashmap *pMap;
	if( nArg < 1 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"%s() expects exactly 1 argument, 0 given",zName
			);
	}
	if( !ph7_value_is_array(apArg[0]) ){
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"%s(): Argument #1 ($array) must be of type array, %s given",
			zName,ph7_type_name(apArg[0])
			);
	}
	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	if( pMap->nEntry > 1 ){
		/* SORT_NATURAL (6), optionally | SORT_FLAG_CASE (8) — the same iFlags
		 * word asort() forwards, so this is asort($a, SORT_NATURAL) exactly. */
		sxi32 iCmpFlags = bFold ? (6|8) : 6;
		pCtx->pVm->iCmpCallbackExc = 0;
		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));
		while(pMap->pLast->pPrev){
			pMap->pLast = pMap->pLast->pPrev;
		}
	}
	{
		sxi32 rcCmp = HashmapFlagSortStatus(pCtx);
		if( rcCmp != PH7_OK ){
			return rcCmp;
		}
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * bool arsort(array &$array[,int $sort_flags = SORT_REGULAR ] )
 *  Sort an array in reverse order and maintain index association.
 * Parameters
 *  $array
 *   The input array.
 * $sort_flags
 *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:
 *  Sorting type flags:
 *   SORT_REGULAR - compare items normally (don't change types)
 *   SORT_NUMERIC - compare items numerically
 *   SORT_STRING - compare items as strings
 * Return
 *  TRUE on success or FALSE on failure.
 */
PH7_PRIVATE int ph7_hashmap_arsort(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap;
	/* PHP 8: ArgumentCountError if no arguments */
	if( nArg < 1 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"arsort() expects at least 1 argument, 0 given"
			);
	}
	/* PHP 8: TypeError if first argument is not an array */
	if( !ph7_value_is_array(apArg[0]) ){
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"arsort(): Argument #1 ($array) must be of type array, %s given",
			ph7_type_name(apArg[0])
			);
	}
	/* Point to the internal representation of the input hashmap */
	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	if( pMap->nEntry > 1 ){
		sxi32 iCmpFlags = 0;
		if( nArg > 1 ){
			/* Extract comparison flags */
			iCmpFlags = ph7_value_to_int(apArg[1]);
		}
		/* Do the merge sort */
		pCtx->pVm->iCmpCallbackExc = 0;
		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));
		/* Fix the last link broken by the merge */
		while(pMap->pLast->pPrev){
			pMap->pLast = pMap->pLast->pPrev;
		}
	}
	{
		sxi32 rcCmp = HashmapFlagSortStatus(pCtx);
		if( rcCmp != PH7_OK ){
			return rcCmp;
		}
	}
	/* All done,return TRUE */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * bool ksort(array &$array[,int $sort_flags = SORT_REGULAR ] )
 *  Sort an array by key.
 * Parameters
 *  $array
 *   The input array.
 * $sort_flags
 *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:
 *  Sorting type flags:
 *   SORT_REGULAR - compare items normally (don't change types)
 *   SORT_NUMERIC - compare items numerically
 *   SORT_STRING - compare items as strings
 * Return
 *  TRUE on success or FALSE on failure.
 */
PH7_PRIVATE int ph7_hashmap_ksort(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap;
	/* Make sure we are dealing with a valid hashmap */
	if( nArg < 1 || !ph7_value_is_array(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the internal representation of the input hashmap */
	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	if( pMap->nEntry > 1 ){
		sxi32 iCmpFlags = 0;
		if( nArg > 1 ){
			/* Extract comparison flags */
			iCmpFlags = ph7_value_to_int(apArg[1]);
		}
		/* Do the merge sort */
		pCtx->pVm->iCmpCallbackExc = 0;
		HashmapMergeSort(pMap,HashmapCmpCallback2,SX_INT_TO_PTR(iCmpFlags));
		/* Fix the last link broken by the merge */
		while(pMap->pLast->pPrev){
			pMap->pLast = pMap->pLast->pPrev;
		}
	}
	{
		sxi32 rcCmp = HashmapFlagSortStatus(pCtx);
		if( rcCmp != PH7_OK ){
			return rcCmp;
		}
	}
	/* All done,return TRUE */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * bool krsort(array &$array[,int $sort_flags = SORT_REGULAR ] )
 *  Sort an array by key in reverse order.
 * Parameters
 *  $array
 *   The input array.
 * $sort_flags
 *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:
 *  Sorting type flags:
 *   SORT_REGULAR - compare items normally (don't change types)
 *   SORT_NUMERIC - compare items numerically
 *   SORT_STRING - compare items as strings
 * Return
 *  TRUE on success or FALSE on failure.
 */
PH7_PRIVATE int ph7_hashmap_krsort(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap;
	/* Make sure we are dealing with a valid hashmap */
	if( nArg < 1 || !ph7_value_is_array(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the internal representation of the input hashmap */
	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	if( pMap->nEntry > 1 ){
		sxi32 iCmpFlags = 0;
		if( nArg > 1 ){
			/* Extract comparison flags */
			iCmpFlags = ph7_value_to_int(apArg[1]);
		}
		/* Do the merge sort */
		pCtx->pVm->iCmpCallbackExc = 0;
		HashmapMergeSort(pMap,HashmapCmpCallback5,SX_INT_TO_PTR(iCmpFlags));
		/* Fix the last link broken by the merge */
		while(pMap->pLast->pPrev){
			pMap->pLast = pMap->pLast->pPrev;
		}
	}
	{
		sxi32 rcCmp = HashmapFlagSortStatus(pCtx);
		if( rcCmp != PH7_OK ){
			return rcCmp;
		}
	}
	/* All done,return TRUE */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * bool rsort(array &$array[,int $sort_flags = SORT_REGULAR ] )
 * Sort an array in reverse order.
 * Parameters
 *  $array
 *   The input array.
 * $sort_flags
 *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:
 *  Sorting type flags:
 *   SORT_REGULAR - compare items normally (don't change types)
 *   SORT_NUMERIC - compare items numerically
 *   SORT_STRING - compare items as strings
 * Return
 *  TRUE on success or FALSE on failure.
 */
PH7_PRIVATE int ph7_hashmap_rsort(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap;
	/* Make sure we are dealing with a valid hashmap */
	if( nArg < 1 || !ph7_value_is_array(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the internal representation of the input hashmap */
	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	if( pMap->nEntry > 1 ){
		sxi32 iCmpFlags = 0;
		if( nArg > 1 ){
			/* Extract comparison flags */
			iCmpFlags = ph7_value_to_int(apArg[1]);
		}
		/* Do the merge sort */
		pCtx->pVm->iCmpCallbackExc = 0;
		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));
		/* Rehash [Do not maintain index association as requested by the PHP specification] */
		HashmapSortRehash(pMap);
	}else if( pMap->nEntry == 1 ){
		/* php reindexes even a single-element array: a string key becomes 0 */
		HashmapSortRehash(pMap);
	}
	{
		sxi32 rcCmp = HashmapFlagSortStatus(pCtx);
		if( rcCmp != PH7_OK ){
			return rcCmp;
		}
	}
	/* All done,return TRUE */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * bool usort(array &$array,callable $cmp_function)
 *  Sort an array by values using a user-defined comparison function.
 * Parameters
 *  $array
 *   The input array.
 * $cmp_function
 *  The comparison function must return an integer less than, equal to, or greater
 *  than zero if the first argument is considered to be respectively less than, equal
 *  to, or greater than the second.
 *    int callback ( mixed $a, mixed $b )
 * Return
 *  TRUE on success or FALSE on failure.
 */
PH7_PRIVATE int ph7_hashmap_usort(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap;
	/* Make sure we are dealing with a valid hashmap */
	if( nArg < 1 || !ph7_value_is_array(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 1 ){
		/* php rejects an uncallable comparator with a TypeError, BEFORE touching the
		 * array. PH7 handed it to the dispatcher, which answers NULL in silence -- so
		 * usort() with a bogus comparator reordered the array anyway, by nothing. */
		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",0);
		if( rcCb != PH7_OK ){
			return rcCb;
		}
	}
	/* Point to the internal representation of the input hashmap */
	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	if( pMap->nEntry > 1 ){
		ph7_value *pCallback = 0;
		ProcNodeCmp xCmp;
		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */
		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){
			/* Point to the desired callback */
			pCallback = apArg[1];
		}else{
			/* Use the default comparison function */
			xCmp = HashmapCmpCallback1;
		}
		/* Do the merge sort */
		pCtx->pVm->iCmpCallbackExc = 0;
		HashmapMergeSort(pMap,xCmp,pCallback);
		/* Rehash [Do not maintain index association as requested by the PHP specification] */
		HashmapSortRehash(pMap);
		if( pCtx->pVm->iCmpCallbackExc ){
			/* The comparison callback did not return: propagate its status so the
			 * dispatcher unwinds. */
			sxi32 rcExc = pCtx->pVm->iCmpCallbackExc;
			pCtx->pVm->iCmpCallbackExc = 0;
			return rcExc;
		}
	}else if( pMap->nEntry == 1 ){
		/* php reindexes even a single-element array: a string key becomes 0 */
		HashmapSortRehash(pMap);
	}
	/* All done,return TRUE */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * bool uasort(array &$array,callable $cmp_function)
 *  Sort an array by values using a user-defined comparison function
 *  and maintain index association.
 * Parameters
 *  $array
 *   The input array.
 * $cmp_function
 *  The comparison function must return an integer less than, equal to, or greater
 *  than zero if the first argument is considered to be respectively less than, equal
 *  to, or greater than the second.
 *    int callback ( mixed $a, mixed $b )
 * Return
 *  TRUE on success or FALSE on failure.
 */
PH7_PRIVATE int ph7_hashmap_uasort(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap;
	/* Make sure we are dealing with a valid hashmap */
	if( nArg < 1 || !ph7_value_is_array(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 1 ){
		/* php rejects an uncallable comparator with a TypeError, BEFORE touching the
		 * array. PH7 handed it to the dispatcher, which answers NULL in silence -- so
		 * usort() with a bogus comparator reordered the array anyway, by nothing. */
		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",0);
		if( rcCb != PH7_OK ){
			return rcCb;
		}
	}
	/* Point to the internal representation of the input hashmap */
	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	if( pMap->nEntry > 1 ){
		ph7_value *pCallback = 0;
		ProcNodeCmp xCmp;
		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */
		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){
			/* Point to the desired callback */
			pCallback = apArg[1];
		}else{
			/* Use the default comparison function */
			xCmp = HashmapCmpCallback1;
		}
		/* Do the merge sort */
		pCtx->pVm->iCmpCallbackExc = 0;
		HashmapMergeSort(pMap,xCmp,pCallback);
		/* Fix the last link broken by the merge */
		while(pMap->pLast->pPrev){
			pMap->pLast = pMap->pLast->pPrev;
		}
		if( pCtx->pVm->iCmpCallbackExc ){
			/* The comparison callback did not return: propagate its status so the
			 * dispatcher unwinds. */
			sxi32 rcExc = pCtx->pVm->iCmpCallbackExc;
			pCtx->pVm->iCmpCallbackExc = 0;
			return rcExc;
		}
	}
	/* All done,return TRUE */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * bool uksort(array &$array,callable $cmp_function)
 *  Sort an array by keys using a user-defined comparison
 *  function and maintain index association.
 * Parameters
 *  $array
 *   The input array.
 * $cmp_function
 *  The comparison function must return an integer less than, equal to, or greater
 *  than zero if the first argument is considered to be respectively less than, equal
 *  to, or greater than the second.
 *    int callback ( mixed $a, mixed $b )
 * Return
 *  TRUE on success or FALSE on failure.
 */
PH7_PRIVATE int ph7_hashmap_uksort(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap;
	/* Make sure we are dealing with a valid hashmap */
	if( nArg < 1 || !ph7_value_is_array(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 1 ){
		/* php rejects an uncallable comparator with a TypeError, BEFORE touching the
		 * array. PH7 handed it to the dispatcher, which answers NULL in silence -- so
		 * usort() with a bogus comparator reordered the array anyway, by nothing. */
		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",0);
		if( rcCb != PH7_OK ){
			return rcCb;
		}
	}
	/* Point to the internal representation of the input hashmap */
	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	if( pMap->nEntry > 1 ){
		ph7_value *pCallback = 0;
		ProcNodeCmp xCmp;
		xCmp = HashmapCmpCallback6; /* User-defined function as the comparison callback */
		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){
			/* Point to the desired callback */
			pCallback = apArg[1];
		}else{
			/* Use the default comparison function */
			xCmp = HashmapCmpCallback2;
		}
		/* Do the merge sort */
		pCtx->pVm->iCmpCallbackExc = 0;
		HashmapMergeSort(pMap,xCmp,pCallback);
		/* Fix the last link broken by the merge */
		while(pMap->pLast->pPrev){
			pMap->pLast = pMap->pLast->pPrev;
		}
		if( pCtx->pVm->iCmpCallbackExc ){
			/* The comparison callback did not return: propagate its status so the
			 * dispatcher unwinds. */
			sxi32 rcExc = pCtx->pVm->iCmpCallbackExc;
			pCtx->pVm->iCmpCallbackExc = 0;
			return rcExc;
		}
	}
	/* All done,return TRUE */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * bool array_multisort(array &$array, mixed $array1_sort_order = SORT_ASC,
 *                      mixed $array1_sort_flags = SORT_REGULAR, mixed &...$rest)
 *  Sort multiple arrays at once: the argument list is a little language read
 *  left to right — an ARRAY opens a column, and each column may be followed by
 *  at most one sort ORDER (SORT_ASC/SORT_DESC) and at most one sort FLAGS
 *  value, in either order. Rows are compared column by column, a tie in one
 *  column falling through to the next; the resulting permutation is applied to
 *  EVERY column. String keys are kept, numeric keys are renumbered, and the
 *  sort is stable (php 8's own guarantee). php's error shapes, pinned by
 *  probe: a non-int non-array is `Argument #N must be an array or a sort
 *  flag`; a misplaced or repeated order/flags is the same text with
 *  `... that has not already been specified`; an int that is no flag at all is
 *  the ValueError `must be a valid sort flag`; mismatched lengths are
 *  `Array sizes are inconsistent` with no function prefix; and only
 *  Argument #1 carries its `($array)` name. php declares the whole list
 *  prefer-ref (VmBuiltinPrefersRef), so a literal sorts a temporary silently.
 */
/* One column of the multisort: the caller's array plus its sort spec. */
typedef struct MultisortCol MultisortCol;
struct MultisortCol
{
	ph7_value *pArr;        /* The caller's argument slot */
	ph7_hashmap *pMap;      /* Its hashmap */
	ph7_hashmap_node **apNode; /* Nodes in ORIGINAL iteration order */
	sxi32 iFlags;           /* SORT_* comparison flags */
	int iDir;               /* +1 SORT_ASC, -1 SORT_DESC */
	int bOrderSeen;         /* An order argument already attached */
	int bFlagsSeen;         /* A flags argument already attached */
};
/* Compare two ROWS, column by column with each column's own direction/flags. */
static sxi32 MultisortRowCmp(MultisortCol *aCol,sxu32 nCol,sxu32 iA,sxu32 iB)
{
	ph7_vm *pVm = aCol[0].pMap->pVm;
	sxu32 c;
	for( c = 0 ; c < nCol ; c++ ){
		sxi32 rc = HashmapFlagValueCmp(aCol[c].apNode[iA],aCol[c].apNode[iB],aCol[c].iFlags);
		HashmapCmpLatch(pVm);
		if( rc != 0 ){
			return aCol[c].iDir < 0 ? -rc : rc;
		}
	}
	return 0;
}
/*
 * Stable bottom-up merge sort over the row-index permutation. Iterative on
 * purpose — the recursive shape would put O(log n) frames on the native stack
 * (the §7 embedder C-stack family).
 */
static void MultisortSortIdx(MultisortCol *aCol,sxu32 nCol,sxu32 *aIdx,sxu32 *aTmp,sxu32 n)
{
	sxu32 nWidth,iLo;
	for( nWidth = 1 ; nWidth < n ; nWidth *= 2 ){
		for( iLo = 0 ; iLo < n ; iLo += 2 * nWidth ){
			sxu32 iMid = iLo + nWidth;
			sxu32 iHi = iLo + 2 * nWidth;
			sxu32 i,j,k;
			if( iMid > n ){ iMid = n; }
			if( iHi > n ){ iHi = n; }
			i = iLo; j = iMid; k = iLo;
			while( i < iMid && j < iHi ){
				/* <= keeps the run stable: on a full tie the left row wins */
				if( MultisortRowCmp(aCol,nCol,aIdx[i],aIdx[j]) <= 0 ){
					aTmp[k++] = aIdx[i++];
				}else{
					aTmp[k++] = aIdx[j++];
				}
			}
			while( i < iMid ){ aTmp[k++] = aIdx[i++]; }
			while( j < iHi ){ aTmp[k++] = aIdx[j++]; }
		}
		/* aTmp holds the merged runs for this width; swap roles by copying
		 * back — n is bounded by the array count, one memcpy per doubling. */
		SyMemcpy(aTmp,aIdx,(sxu32)(n * sizeof(sxu32)));
	}
}
PH7_PRIVATE int ph7_hashmap_multisort(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	MultisortCol *aCol;
	sxu32 *aIdx,*aTmp;
	sxu32 nCol = 0,nRow,i;
	sxi32 rcStatus;
	int iArg;

	if( nArg < 1 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"array_multisort() expects at least 1 argument, %d given",
			nArg
			);
	}
	aCol = (MultisortCol *)ph7_context_alloc_chunk(pCtx,
		(unsigned int)(sizeof(MultisortCol) * (sxu32)nArg),TRUE,TRUE);
	if( aCol == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	/* Read the argument list's little language, php's own state machine. */
	for( iArg = 0 ; iArg < nArg ; iArg++ ){
		ph7_value *pArg = apArg[iArg];
		/* Only Argument #1 carries its parameter name in php's messages. */
		const char *zName = (iArg == 0) ? " ($array)" : "";
		if( ph7_value_is_array(pArg) ){
			MultisortCol *pCol = &aCol[nCol++];
			pCol->pArr = pArg;
			pCol->pMap = (ph7_hashmap *)pArg->x.pOther;
			pCol->apNode = 0;
			pCol->iFlags = 0; /* SORT_REGULAR */
			pCol->iDir = 1;   /* SORT_ASC */
			pCol->bOrderSeen = pCol->bFlagsSeen = 0;
			continue;
		}
		if( ph7_value_is_float(pArg) || (pArg->iFlags & MEMOBJ_INT) == 0 ){
			/* A REAL int only, float asked FIRST (ph7_type_name's rule: an
			 * integer-valued real caches an int and would pass a bare flag
			 * test). php coerces nothing here: "4", 4.0 and true are all
			 * refused. */
			return PH7_VmThrowException(pCtx,"TypeError",
				"array_multisort(): Argument #%d%s must be an array or a sort flag",
				iArg + 1,zName);
		}
		{
			sxi64 iVal = ph7_value_to_int64(pArg);
			/* php masks SORT_FLAG_CASE off for the ORDER match too, but reads
			 * the direction from the UNMASKED value — so SORT_DESC|SORT_FLAG_CASE
			 * (11) consumes the order slot and quirkily sorts ASCENDING. */
			if( (iVal & ~(sxi64)8) == 3 /* SORT_DESC */ || (iVal & ~(sxi64)8) == 4 /* SORT_ASC */ ){
				if( nCol < 1 || aCol[nCol - 1].bOrderSeen ){
					return PH7_VmThrowException(pCtx,"TypeError",
						"array_multisort(): Argument #%d%s must be an array or a sort flag that has not already been specified",
						iArg + 1,zName);
				}
				aCol[nCol - 1].iDir = (iVal == 3) ? -1 : 1;
				aCol[nCol - 1].bOrderSeen = 1;
			}else if( (iVal & ~(sxi64)8 /* SORT_FLAG_CASE */) == 0 /* SORT_REGULAR */
			       || (iVal & ~(sxi64)8) == 1 /* SORT_NUMERIC */
			       || (iVal & ~(sxi64)8) == 2 /* SORT_STRING */
			       || (iVal & ~(sxi64)8) == 5 /* SORT_LOCALE_STRING */
			       || (iVal & ~(sxi64)8) == 6 /* SORT_NATURAL */ ){
				if( nCol < 1 || aCol[nCol - 1].bFlagsSeen ){
					return PH7_VmThrowException(pCtx,"TypeError",
						"array_multisort(): Argument #%d%s must be an array or a sort flag that has not already been specified",
						iArg + 1,zName);
				}
				aCol[nCol - 1].iFlags = (sxi32)iVal;
				aCol[nCol - 1].bFlagsSeen = 1;
			}else{
				return PH7_VmThrowException(pCtx,"ValueError",
					"array_multisort(): Argument #%d%s must be a valid sort flag",
					iArg + 1,zName);
			}
		}
	}
	/* Every column must hold the same number of rows; php's message carries no
	 * function prefix. */
	nRow = aCol[0].pMap->nEntry;
	for( i = 1 ; i < nCol ; i++ ){
		if( aCol[i].pMap->nEntry != nRow ){
			return PH7_VmThrowException(pCtx,"ValueError","Array sizes are inconsistent");
		}
	}
	if( nRow > 0 ){
		/* Collect each column's nodes in original order. */
		for( i = 0 ; i < nCol ; i++ ){
			ph7_hashmap_node *pNode = aCol[i].pMap->pFirst;
			sxu32 r;
			aCol[i].apNode = (ph7_hashmap_node **)ph7_context_alloc_chunk(pCtx,
				(unsigned int)(sizeof(ph7_hashmap_node *) * nRow),FALSE,TRUE);
			if( aCol[i].apNode == 0 ){
				return PH7_ContextMemoryError(pCtx);
			}
			for( r = 0 ; r < nRow && pNode ; r++ ){
				aCol[i].apNode[r] = pNode;
				pNode = pNode->pPrev; /* Reverse link */
			}
		}
		aIdx = (sxu32 *)ph7_context_alloc_chunk(pCtx,
			(unsigned int)(sizeof(sxu32) * nRow * 2),FALSE,TRUE);
		if( aIdx == 0 ){
			return PH7_ContextMemoryError(pCtx);
		}
		aTmp = &aIdx[nRow];
		for( i = 0 ; i < nRow ; i++ ){
			aIdx[i] = i;
		}
		/* The string flags coerce user-visibly and can only FLAG a throw
		 * (iCmpCallbackExc); clear it, sort, and report after — the flag-sort
		 * drivers' shared pattern. */
		pCtx->pVm->iCmpCallbackExc = 0;
		MultisortSortIdx(aCol,nCol,aIdx,aTmp,nRow);
		if( pCtx->pVm->iCmpCallbackExc ){
			/* A comparison raised. php's array_multisort leaves EVERY column as
			 * it found it in that case -- unlike sort(), which still writes back
			 * the order it reached -- so the permutation is dropped rather than
			 * applied. */
			return HashmapFlagSortStatus(pCtx);
		}
		/* Apply the permutation to every column: rebuild in sorted order,
		 * keeping string keys and renumbering int keys, then hand the fresh
		 * array back through the by-ref slot (a literal has none and the
		 * result is silently dropped — php's prefer-ref). */
		for( i = 0 ; i < nCol ; i++ ){
			ph7_value *pNew = ph7_context_new_array(pCtx);
			sxu32 r;
			if( pNew == 0 ){
				return PH7_ContextMemoryError(pCtx);
			}
			for( r = 0 ; r < nRow ; r++ ){
				ph7_hashmap_node *pNode = aCol[i].apNode[aIdx[r]];
				HashmapInsertNode((ph7_hashmap *)pNew->x.pOther,pNode,
					pNode->iType == HASHMAP_BLOB_NODE ? TRUE : FALSE);
			}
			PH7_VmStoreArgByRef(pCtx->pVm,aCol[i].pArr,pNew);
		}
		rcStatus = HashmapFlagSortStatus(pCtx);
		if( rcStatus != PH7_OK ){
			return rcStatus;
		}
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
