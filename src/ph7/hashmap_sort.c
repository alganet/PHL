/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
/*
 * Section:
 *    Hashmap (array) sorting: php's zend_sort hybrid, its comparator
 *    set and the sort()/usort()/ksort() builtin family.
 * Status:
 *    Stable.
 */
/*
 * The ordering primitive.
 *
 * php sorts a PRIVATE COPY of the array (zend_array_dup in php_usort and
 * friends), so a comparator that reads the array being sorted always sees it
 * exactly as it was at the call, and a write the comparator makes to it is
 * discarded when the sorted copy is installed. PHL used to thread the order
 * through the nodes' own pPrev/pNext links, which are the map's ONLY list:
 * while the sort ran, `pMap->pFirst` led into half-merged runs, so a comparator
 * that so much as printed the array walked freed-looking garbage and the
 * interpreter died -- `usort($a, function($x,$y) use (&$a){ ... $a ... })` is a
 * segmentation fault, not a wrong answer.
 *
 * The order is decided over a VECTOR of node pointers instead, and the map's
 * list is relinked once, after the last comparison. The array therefore stays
 * fully walkable, in its pre-call order, for as long as user code can see it --
 * which is php's answer -- and the nodes are never left in an inconsistent
 * state for anything else to trip over.
 *
 * WHICH order the vector is put in is itself observable, and this used to be a
 * bottom-up merge sort where php runs the hybrid insertion/quick sort of
 * `zend_sort` (itself derived from libc++'s std::sort). Two programs can tell
 * the difference:
 *
 *   - A comparator counts, prints or throws. `array_udiff([1,2,3],[2,3,4],$f)`
 *     entered $f six times here and nine times in php; a comparator that logs is
 *     reading the engine's own decisions, so the SEQUENCE of pairs is contract,
 *     not an implementation detail.
 *   - The comparison is not an ordering. NAN answers 1 against everything in
 *     both directions, so no two algorithms need agree: `sort([3,NAN,1,2])` is
 *     `1 2 NAN 3` under a merge and `NAN 1 2 3` in php.
 *
 * Stability is php's too, and is NOT a property of this sort: `zend_sort` is a
 * quicksort and reorders equal elements freely. php's sorts have been stable
 * since 8.0 because `zend_hash_sort_internal` stamps every bucket with its
 * original position first, and the sort builtins' comparators fall back on that
 * stamp when the real comparison answers 0. nOrd is that stamp, and bStable
 * picks the comparator that consults it -- php's diff/intersect helpers sort
 * their operand lists with the UNSTABLE variants instead.
 */
typedef struct HashmapSortCtx HashmapSortCtx;
struct HashmapSortCtx {
	ProcNodeCmp xCmp;        /* The caller's node comparison */
	void *pCmpData;          /* Opaque comparison data */
	int bStable;             /* Break a tie on nOrd rather than leaving it to the sort */
};
static sxi32 HashmapSortCmp(HashmapSortCtx *pCtx,HashmapSortEnt *pA,HashmapSortEnt *pB)
{
	sxi32 rc = pCtx->xCmp(pA->pNode,pB->pNode,pCtx->pCmpData);
	if( rc == 0 && pCtx->bStable ){
		/* php's stable_sort_fallback(): the original positions, never equal */
		rc = ( pA->nOrd > pB->nOrd ) ? 1 : -1;
	}
	return rc;
}
static void HashmapSortSwap(HashmapSortEnt *pA,HashmapSortEnt *pB)
{
	HashmapSortEnt sTmp = *pA;
	*pA = *pB;
	*pB = sTmp;
}
/*
 * The fixed-size sorting networks. Each one is a decision tree, so both the
 * number of comparisons and the pairs compared depend on the answers: this is
 * where most of the count divergence against php lived.
 */
static void HashmapSort2(HashmapSortEnt *a,HashmapSortEnt *b,HashmapSortCtx *pCtx)
{
	if( HashmapSortCmp(pCtx,a,b) > 0 ){
		HashmapSortSwap(a,b);
	}
}
static void HashmapSort3(HashmapSortEnt *a,HashmapSortEnt *b,HashmapSortEnt *c,HashmapSortCtx *pCtx)
{
	if( !(HashmapSortCmp(pCtx,a,b) > 0) ){
		if( !(HashmapSortCmp(pCtx,b,c) > 0) ){
			return;
		}
		HashmapSortSwap(b,c);
		if( HashmapSortCmp(pCtx,a,b) > 0 ){
			HashmapSortSwap(a,b);
		}
		return;
	}
	if( !(HashmapSortCmp(pCtx,c,b) > 0) ){
		HashmapSortSwap(a,c);
		return;
	}
	HashmapSortSwap(a,b);
	if( HashmapSortCmp(pCtx,b,c) > 0 ){
		HashmapSortSwap(b,c);
	}
}
static void HashmapSort4(HashmapSortEnt *a,HashmapSortEnt *b,HashmapSortEnt *c,HashmapSortEnt *d,HashmapSortCtx *pCtx)
{
	HashmapSort3(a,b,c,pCtx);
	if( HashmapSortCmp(pCtx,c,d) > 0 ){
		HashmapSortSwap(c,d);
		if( HashmapSortCmp(pCtx,b,c) > 0 ){
			HashmapSortSwap(b,c);
			if( HashmapSortCmp(pCtx,a,b) > 0 ){
				HashmapSortSwap(a,b);
			}
		}
	}
}
static void HashmapSort5(HashmapSortEnt *a,HashmapSortEnt *b,HashmapSortEnt *c,HashmapSortEnt *d,HashmapSortEnt *e,HashmapSortCtx *pCtx)
{
	HashmapSort4(a,b,c,d,pCtx);
	if( HashmapSortCmp(pCtx,d,e) > 0 ){
		HashmapSortSwap(d,e);
		if( HashmapSortCmp(pCtx,c,d) > 0 ){
			HashmapSortSwap(c,d);
			if( HashmapSortCmp(pCtx,b,c) > 0 ){
				HashmapSortSwap(b,c);
				if( HashmapSortCmp(pCtx,a,b) > 0 ){
					HashmapSortSwap(a,b);
				}
			}
		}
	}
}
/*
 * Insertion sort, php's zend_insert_sort: the networks up to five entries, then
 * a linear scan back for the first six and a two-at-a-time scan back after that
 * (which is why the sixth element onwards costs a different number of
 * comparisons from the fifth).
 */
static void HashmapInsertSort(HashmapSortEnt *aEnt,sxu32 n,HashmapSortCtx *pCtx)
{
	switch( n ){
		case 0:
		case 1:
			break;
		case 2:
			HashmapSort2(aEnt,&aEnt[1],pCtx);
			break;
		case 3:
			HashmapSort3(aEnt,&aEnt[1],&aEnt[2],pCtx);
			break;
		case 4:
			HashmapSort4(aEnt,&aEnt[1],&aEnt[2],&aEnt[3],pCtx);
			break;
		case 5:
			HashmapSort5(aEnt,&aEnt[1],&aEnt[2],&aEnt[3],&aEnt[4],pCtx);
			break;
		default: {
			HashmapSortEnt *i,*j,*k;
			HashmapSortEnt *start = aEnt;
			HashmapSortEnt *end = &aEnt[n];
			HashmapSortEnt *sentry = &aEnt[6]; /* n >= 6 here, so this is in range */
			for( i = start + 1 ; i < sentry ; i++ ){
				j = i - 1;
				if( !(HashmapSortCmp(pCtx,j,i) > 0) ){
					continue;
				}
				while( j != start ){
					j--;
					if( !(HashmapSortCmp(pCtx,j,i) > 0) ){
						j++;
						break;
					}
				}
				for( k = i ; k > j ; k-- ){
					HashmapSortSwap(k,k - 1);
				}
			}
			for( i = sentry ; i < end ; i++ ){
				j = i - 1;
				if( !(HashmapSortCmp(pCtx,j,i) > 0) ){
					continue;
				}
				/* j starts at i-3 >= start+3 and steps back by two, so the two
				 * guards below are what keeps it from walking off the front. */
				for( ;; ){
					j -= 2;
					if( !(HashmapSortCmp(pCtx,j,i) > 0) ){
						j++;
						if( !(HashmapSortCmp(pCtx,j,i) > 0) ){
							j++;
						}
						break;
					}
					if( j == start ){
						break;
					}
					if( j == start + 1 ){
						j--;
						if( HashmapSortCmp(pCtx,i,j) > 0 ){
							j++;
						}
						break;
					}
				}
				for( k = i ; k > j ; k-- ){
					HashmapSortSwap(k,k - 1);
				}
			}
			break;
		}
	}
}
/*
 * php's zend_sort: insertion sort at sixteen entries or fewer, otherwise a
 * median-of-three (median-of-five past 1024 entries) quicksort that recurses
 * into the SMALLER partition and loops on the larger, which bounds the
 * recursion at log2(n) frames.
 *
 * The partition loop advances i and j only through an `== ` guard against each
 * other, never through the comparator's answer, so an inconsistent comparator
 * -- NAN's, or a user callback that answers at random -- can produce a garbage
 * ORDER but can never walk off either end.
 */
static void HashmapZendSort(HashmapSortEnt *aEnt,sxu32 n,HashmapSortCtx *pCtx)
{
	for(;;){
		if( n <= 16 ){
			HashmapInsertSort(aEnt,n,pCtx);
			return;
		}else{
			HashmapSortEnt *i,*j;
			HashmapSortEnt *start = aEnt;
			HashmapSortEnt *end = &aEnt[n];
			sxu32 offset = n >> 1;
			HashmapSortEnt *pivot = &start[offset];
			if( n >> 10 ){
				sxu32 delta = offset >> 1;
				HashmapSort5(start,&start[delta],pivot,&pivot[delta],end - 1,pCtx);
			}else{
				HashmapSort3(start,pivot,end - 1,pCtx);
			}
			HashmapSortSwap(start + 1,pivot);
			pivot = start + 1;
			i = pivot + 1;
			j = end - 1;
			for(;;){
				while( HashmapSortCmp(pCtx,pivot,i) > 0 ){
					i++;
					if( i == j ){
						goto done;
					}
				}
				j--;
				if( j == i ){
					goto done;
				}
				while( HashmapSortCmp(pCtx,j,pivot) > 0 ){
					j--;
					if( j == i ){
						goto done;
					}
				}
				HashmapSortSwap(i,j);
				i++;
				if( i == j ){
					goto done;
				}
			}
done:
			HashmapSortSwap(pivot,i - 1);
			if( (i - 1) - start < end - i ){
				HashmapZendSort(start,(sxu32)(i - start) - 1,pCtx);
				aEnt = i;
				n = (sxu32)(end - i);
			}else{
				HashmapZendSort(i,(sxu32)(end - i),pCtx);
				n = (sxu32)(i - start) - 1;
			}
		}
	}
}
/*
** Inputs:
**   Map:       Input hashmap
**   cmp:       A comparison function.
**
** Side effects:
**   The node list of the given hashmap is relinked in sorted order, ONCE, after
**   the last comparison. SXERR_MEM leaves the array untouched and unsorted (the
**   comparison order cannot be decided without the vector).
*/
PH7_PRIVATE sxi32 HashmapNodeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData)
{
	HashmapSortEnt *aEnt;
	HashmapSortCtx sCtx;
	ph7_hashmap_node *pEntry;
	sxu32 n,i;
	n = pMap->nEntry;
	if( n < 2 ){
		pMap->pCur = pMap->pFirst;
		return SXRET_OK;
	}
	aEnt = (HashmapSortEnt *)SyMemBackendAlloc(&pMap->pVm->sAllocator,n * sizeof(HashmapSortEnt));
	if( aEnt == 0 ){
		return SXERR_MEM;
	}
	/* Collect the nodes in iteration order (pPrev is the FORWARD link here),
	 * stamping each with its position: that stamp IS the sort's stability. */
	i = 0;
	for( pEntry = pMap->pFirst ; pEntry && i < n ; pEntry = pEntry->pPrev ){
		aEnt[i].pNode = pEntry;
		aEnt[i].nOrd = i;
		i++;
	}
	n = i; /* Defensive: honour the list, not the counter, if they disagree */
	if( n < 2 ){
		/* An empty or single-node list has no order to decide, and the relink
		 * below indexes the vector unconditionally. */
		SyMemBackendFree(&pMap->pVm->sAllocator,(void *)aEnt);
		pMap->pCur = pMap->pFirst;
		return SXRET_OK;
	}
	sCtx.xCmp = xCmp;
	sCtx.pCmpData = pCmpData;
	sCtx.bStable = 1;
	HashmapZendSort(aEnt,n,&sCtx);
	/* Relink the map's list from the decided order */
	for( i = 0 ; i < n ; i++ ){
		aEnt[i].pNode->pPrev = ( i + 1 < n ) ? aEnt[i+1].pNode : 0;
		aEnt[i].pNode->pNext = ( i > 0 ) ? aEnt[i-1].pNode : 0;
	}
	pMap->pFirst = aEnt[0].pNode;
	pMap->pLast = aEnt[n-1].pNode;
	/* php's sorts rewind the array's internal pointer */
	pMap->pCur = pMap->pFirst;
	SyMemBackendFree(&pMap->pVm->sAllocator,(void *)aEnt);
	return SXRET_OK;
}
/*
 * Order a caller's OWN vector of entries, leaving every map alone.
 *
 * php's diff/intersect helpers sort a private list of each argument array and
 * merge the sorted lists; the lists are theirs, so nothing is relinked, and they
 * pass the UNSTABLE comparators -- equal entries end up wherever the quicksort
 * left them and nothing downstream asks which. Each entry still carries its
 * position, because the caller needs it to put its answer back in the source
 * array's order.
 */
PH7_PRIVATE void PH7_HashmapSortEntVector(HashmapSortEnt *aEnt,sxu32 n,ProcNodeCmp xCmp,void *pCmpData)
{
	HashmapSortCtx sCtx;
	if( n < 2 ){
		return;
	}
	sCtx.xCmp = xCmp;
	sCtx.pCmpData = pCmpData;
	sCtx.bStable = 0;
	HashmapZendSort(aEnt,n,&sCtx);
}
/*
 * A sort LENDS the map to itself for the duration.
 *
 * php sorts a private copy, so a write the comparator makes to the array being
 * sorted is discarded: `usort($a, function($x,$y) use (&$a){ $a[]=9; ... })`
 * answers the sorted PRE-CALL array in php, with no 9 in it, and an
 * `unset($a[0])` in there changes nothing either. PHL sorts the LIVE map --
 * which is what keeps a comparator's reads php-exact (see HashmapNodeSort) --
 * so it lends the map one extra reference first: a write through the variable
 * then copy-on-write separates a map of its own and the sort's nodes are never
 * relinked, or freed, underneath the vector it is deciding an order over.
 *
 * Taking the loan back installs the sorted map over whatever the comparator left
 * behind, which is php's `ZVAL_ARR(array, arr)`. It also has to happen before the
 * driver rehashes: nothing else may hold the map by then, so the plain unref
 * would free it under the driver's feet.
 */
static void HashmapSortInstall(ph7_value *pVal,ph7_hashmap *pMap)
{
	if( (pVal->iFlags & MEMOBJ_HASHMAP) && (ph7_hashmap *)pVal->x.pOther == pMap ){
		return; /* Untouched: the common case, and no work at all */
	}
	PH7_MemObjRelease(pVal);
	MemObjSetType(pVal,MEMOBJ_HASHMAP);
	pVal->x.pOther = pMap;
	pMap->iRef++;
}
static void HashmapSortDrive(
	ph7_vm *pVm,
	ph7_value *pArray,        /* The by-reference array argument */
	ph7_hashmap *pMap,        /* Its map, already COW-separated by the driver */
	ProcNodeCmp xCmp,void *pCmpData
	)
{
	ph7_value *pBacking = 0;
	pMap->iRef++;
	HashmapNodeSort(pMap,xCmp,pCmpData);
	if( pArray->nIdx != SXU32_HIGH ){
		/* The argument may be a stack copy of the caller's variable, and a
		 * comparator's write lands on the variable: install into both. */
		pBacking = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pArray->nIdx);
	}
	if( pBacking && pBacking != pArray ){
		HashmapSortInstall(pBacking,pMap);
	}
	HashmapSortInstall(pArray,pMap);
	PH7_HashmapUnref(pMap);
}
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
 * already uses; every flag-sort driver clears the flag before its sort and
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
		 * means adopting its precision loss, which is what parity is (the scope policy).
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
		return HashmapNodeCmp(pA,pB,FALSE,0);
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
 * Two things follow, and both were missing: the sort must STAND DOWN (a
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
		rc = HashmapNodeCmp(pA,pB,FALSE,0);
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
		rc = -HashmapNodeCmp(pA,pB,FALSE,0);
	}else{
		rc = -HashmapFlagValueCmp(pA,pB,SX_PTR_TO_INT(pCmpData));
	}
	HashmapCmpLatch(pVm);
	return rc;
}
/*
 * One comparison asked of a user callback, reduced to -1/0/1 the way php's
 * php_array_user_compare_unstable() does. Shared by the three sorts and by the
 * diff/intersect family.
 *
 * The reduction is by SIGN over the full 64 bits: a bare (int) cast made a
 * callback answering `($a <=> $b) << 32` compare every pair equal.
 *
 * With bBoolRetry, a callback answering a BOOL is php 8's deprecated
 * `return $a > $b;` comparator. php raises "Returning bool from comparison
 * function is deprecated" once per builtin call (bCmpBoolRaised), and because
 * `false` cannot tell "less" from "equal" it asks again with the operands
 * SWAPPED and answers the negation of that: true means greater, false-then-true
 * means less, false-then-false means equal. Without the retry `false` read as
 * equal, which every sort of this shape survives by luck and the merge in
 * array_udiff() and its neighbours does not -- a whole diff came back empty.
 * array_udiff_assoc() and array_uintersect_assoc() ask their value callback
 * through php's zval_user_compare(), which does neither: they pass FALSE.
 *
 * Returns the dispatch status. A callback that did not return leaves *pCmp 0;
 * a deprecation whose error handler threw leaves the pair's answer as php has
 * it at that moment (the swapped call is never made with an exception
 * pending) and returns that status for the caller to latch.
 */
PH7_PRIVATE sxi32 PH7_HashmapUserCmp(ph7_context *pCtx,ph7_value *pCallback,ph7_value *pA,ph7_value *pB,
	int bBoolRetry,int *pCmp)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_value *apArg[2];
	ph7_value sResult;
	int bNegate = 0;
	sxi32 rc;
	*pCmp = 0;
	PH7_MemObjInit(pVm,&sResult);
	apArg[0] = pA;
	apArg[1] = pB;
	rc = PH7_VmCallCallbackByValue(pVm,pCallback,2,apArg,&sResult,0);
	if( PH7_CALLBACK_UNWOUND(rc) ){
		PH7_MemObjRelease(&sResult);
		return rc;
	}
	if( rc == SXRET_OK && bBoolRetry && (sResult.iFlags & MEMOBJ_BOOL) ){
		int bTrue = sResult.x.iVal != 0;
		if( !pVm->bCmpBoolRaised ){
			SyString sName;
			pVm->bCmpBoolRaised = 1;
			if( pVm->pNativeCall && pVm->pNativeCall->pName ){
				/* ArrayObject::uasort() runs php's uasort(), which names itself */
				sName = *pVm->pNativeCall->pName;
			}else{
				sName = pCtx->pFunc->sName;
			}
			rc = PH7_VmThrowError(pVm,&sName,8192 /* E_DEPRECATED */,
				"Returning bool from comparison function is deprecated, "
				"return an integer less than, equal to, or greater than zero");
			if( !PH7_CALLBACK_UNWOUND(rc) && PH7_CALLBACK_UNWOUND(pVm->nBoundaryRc) ){
				/* an error handler's throw is parked, not returned */
				rc = pVm->nBoundaryRc;
			}
			if( PH7_CALLBACK_UNWOUND(rc) ){
				*pCmp = bTrue;
				PH7_MemObjRelease(&sResult);
				return rc;
			}
			rc = SXRET_OK;
		}
		if( !bTrue ){
			PH7_MemObjRelease(&sResult);
			PH7_MemObjInit(pVm,&sResult);
			apArg[0] = pB;
			apArg[1] = pA;
			rc = PH7_VmCallCallbackByValue(pVm,pCallback,2,apArg,&sResult,0);
			if( PH7_CALLBACK_UNWOUND(rc) ){
				PH7_MemObjRelease(&sResult);
				return rc;
			}
			bNegate = 1;
		}
	}
	if( rc != SXRET_OK ){
		*pCmp = -1; /* a failed dispatch compares unequal */
	}else{
		if( (sResult.iFlags & MEMOBJ_INT) == 0 ){
			PH7_MemObjToInteger(&sResult);
		}
		*pCmp = (sResult.x.iVal < 0) ? -1 : (sResult.x.iVal > 0 ? 1 : 0);
		if( bNegate ){
			*pCmp = -*pCmp;
		}
	}
	PH7_MemObjRelease(&sResult);
	return SXRET_OK;
}
/*
 * What usort()/uasort()/uksort() hand their node comparison: the callback and
 * the context the deprecation above is raised from.
 */
typedef struct HashmapUserSort HashmapUserSort;
struct HashmapUserSort {
	ph7_context *pCtx;
	ph7_value *pCallback;
};
/*
 * The comparator did not RETURN (or its deprecation's handler threw): latch the
 * STATUS so the sort driver aborts and propagates exactly it (an UNCAUGHT throw
 * is PH7_ABORT, and testing only PH7_EXCEPTION left the sort running --
 * re-entering the comparator, and the fatal report, for every remaining pair).
 */
static sxi32 HashmapUserSortCmp(ph7_vm *pVm,HashmapUserSort *pSort,ph7_value *pA,ph7_value *pB)
{
	int iCmp = 0;
	sxi32 rc = PH7_HashmapUserCmp(pSort->pCtx,pSort->pCallback,pA,pB,TRUE,&iCmp);
	if( rc != SXRET_OK ){
		pVm->iCmpCallbackExc = rc;
	}
	return (sxi32)iCmp;
}
/*
 * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.
 * used-by: [usort(),uasort()]
 */
static sxi32 HashmapCmpCallback4(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)
{
	ph7_vm *pVm = pA->pMap->pVm;
	if( pVm->iCmpCallbackExc ){
		/* A previous comparison already raised: stop invoking the callback so
		 * the exception is not thrown again, and let the sort wind down. */
		return 0;
	}
	return HashmapUserSortCmp(pVm,(HashmapUserSort *)pCmpData,
		HashmapExtractNodeValue(pA),HashmapExtractNodeValue(pB));
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
	ph7_vm *pVm = pA->pMap->pVm;
	ph7_value sK1,sK2;
	sxi32 rc;
	if( pVm->iCmpCallbackExc ){
		/* A previous comparison already raised: stop invoking the callback so
		 * the exception is not thrown again, and let the sort wind down. */
		return 0;
	}
	PH7_MemObjInit(pVm,&sK1);
	PH7_MemObjInit(pVm,&sK2);
	/* Extract nodes keys */
	PH7_HashmapExtractNodeKey(pA,&sK1);
	PH7_HashmapExtractNodeKey(pB,&sK2);
	/* Mark keys as constants */
	sK1.nIdx = SXU32_HIGH;
	sK2.nIdx = SXU32_HIGH;
	rc = HashmapUserSortCmp(pVm,(HashmapUserSort *)pCmpData,&sK1,&sK2);
	PH7_MemObjRelease(&sK1);
	PH7_MemObjRelease(&sK2);
	return rc;
}
/*
 * Permute a hashmap's entries the way php's shuffle() does: Fisher-Yates over
 * the buckets, drawing each index from the MT19937 generator through
 * php_mt_rand_range(). The caller rehashes afterwards (php reindexes the array
 * 0..n-1 and drops string keys).
 *
 * This used to be a sort with a coin-flip comparator, which is a permuting
 * shuffle but not a UNIFORM one — the distribution a random comparator produces
 * is skewed and depends on the sort's internals — and it consumed the generator
 * in a different order, so a seeded run answered a different permutation from
 * php's for every seed.
 */
PH7_PRIVATE sxi32 PH7_HashmapShuffle(ph7_hashmap *pMap)
{
	ph7_hashmap_node **apNode,*pNode;
	sxu32 n,nLeft;
	SySet aNode;
	if( pMap->nEntry < 2 ){
		return SXRET_OK;
	}
	SySetInit(&aNode,&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node *));
	for( pNode = pMap->pFirst ; pNode ; pNode = pNode->pPrev ){
		if( SySetPut(&aNode,(const void *)&pNode) != SXRET_OK ){
			SySetRelease(&aNode);
			return SXERR_MEM;
		}
	}
	apNode = (ph7_hashmap_node **)SySetBasePtr(&aNode);
	n = SySetUsed(&aNode);
	/* php walks DOWN from the last index, swapping with a draw in [0,n_left]. */
	for( nLeft = n - 1 ; nLeft > 0 ; --nLeft ){
		sxu32 nPick = (sxu32)PH7_VmMtRandRange(pMap->pVm,0,(sxi64)nLeft);
		if( nPick != nLeft ){
			ph7_hashmap_node *pTmp = apNode[nLeft];
			apNode[nLeft] = apNode[nPick];
			apNode[nPick] = pTmp;
		}
	}
	/* Relink in the new order. pPrev is the forward link and pNext the back one
	 * (the whole map is built that way); the rehash after this fixes pLast. */
	for( n = 0 ; n < SySetUsed(&aNode) ; ++n ){
		apNode[n]->pPrev = (n + 1 < SySetUsed(&aNode)) ? apNode[n+1] : 0;
		apNode[n]->pNext = (n > 0) ? apNode[n-1] : 0;
	}
	pMap->pFirst = apNode[0];
	pMap->pLast = apNode[SySetUsed(&aNode) - 1];
	pMap->pCur = pMap->pFirst;
	SySetRelease(&aNode);
	return SXRET_OK;
}
/*
 * Rehash all nodes keys after a sort have been applied.
 * Used by [sort(),usort() and rsort()].
 */
PH7_PRIVATE void HashmapSortRehash(ph7_hashmap *pMap)
{
	ph7_hashmap_node *p,*pLast;
	sxu32 i;
	/* php's sorts rewind the array's internal pointer. The reordering paths reset
	 * it themselves, but a ONE-element array is reindexed without being reordered
	 * — and `$a = ['x'=>1]; next($a); sort($a);` then left current() past the end,
	 * answering false where php answers the element. */
	pMap->pCur = pMap->pFirst;
	/* Rehash all entries */
	pLast = p = pMap->pFirst;
	pMap->iNextIdx = 0;
	pMap->bIntKeySeen = 0; /* Reset the automatic index */
	i = 0;
	for( ;; ){
		if( i >= pMap->nEntry ){
			pMap->pLast = pLast;
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
 * it, so every flag-sort driver clears the flag before its sort and
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
		/* Decide the order */
		pCtx->pVm->iCmpCallbackExc = 0;
		HashmapSortDrive(pCtx->pVm,apArg[0],pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));
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
	char zGiven[64];
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
			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))
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
		/* Decide the order */
		pCtx->pVm->iCmpCallbackExc = 0;
		HashmapSortDrive(pCtx->pVm,apArg[0],pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));
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
	char zGiven[64];
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
			zName,VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))
			);
	}
	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	if( pMap->nEntry > 1 ){
		/* SORT_NATURAL (6), optionally | SORT_FLAG_CASE (8) — the same iFlags
		 * word asort() forwards, so this is asort($a, SORT_NATURAL) exactly. */
		sxi32 iCmpFlags = bFold ? (6|8) : 6;
		pCtx->pVm->iCmpCallbackExc = 0;
		HashmapSortDrive(pCtx->pVm,apArg[0],pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));
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
	char zGiven[64];
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
			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))
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
		/* Decide the order */
		pCtx->pVm->iCmpCallbackExc = 0;
		HashmapSortDrive(pCtx->pVm,apArg[0],pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));
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
		/* Decide the order */
		pCtx->pVm->iCmpCallbackExc = 0;
		HashmapSortDrive(pCtx->pVm,apArg[0],pMap,HashmapCmpCallback2,SX_INT_TO_PTR(iCmpFlags));
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
		/* Decide the order */
		pCtx->pVm->iCmpCallbackExc = 0;
		HashmapSortDrive(pCtx->pVm,apArg[0],pMap,HashmapCmpCallback5,SX_INT_TO_PTR(iCmpFlags));
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
		/* Decide the order */
		pCtx->pVm->iCmpCallbackExc = 0;
		HashmapSortDrive(pCtx->pVm,apArg[0],pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));
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
	/* php's compare_deprecation_thrown, cleared by every entry (see PH7_HashmapUserCmp) */
	pCtx->pVm->bCmpBoolRaised = 0;
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
		HashmapUserSort sSort;
		void *pCmpData = 0;
		ProcNodeCmp xCmp;
		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */
		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){
			/* Point to the desired callback */
			sSort.pCtx = pCtx;
			sSort.pCallback = apArg[1];
			pCmpData = (void *)&sSort;
		}else{
			/* Use the default comparison function */
			xCmp = HashmapCmpCallback1;
		}
		/* Decide the order */
		pCtx->pVm->iCmpCallbackExc = 0;
		HashmapSortDrive(pCtx->pVm,apArg[0],pMap,xCmp,pCmpData);
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
	/* php's compare_deprecation_thrown, cleared by every entry (see PH7_HashmapUserCmp) */
	pCtx->pVm->bCmpBoolRaised = 0;
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
		HashmapUserSort sSort;
		void *pCmpData = 0;
		ProcNodeCmp xCmp;
		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */
		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){
			/* Point to the desired callback */
			sSort.pCtx = pCtx;
			sSort.pCallback = apArg[1];
			pCmpData = (void *)&sSort;
		}else{
			/* Use the default comparison function */
			xCmp = HashmapCmpCallback1;
		}
		/* Decide the order */
		pCtx->pVm->iCmpCallbackExc = 0;
		HashmapSortDrive(pCtx->pVm,apArg[0],pMap,xCmp,pCmpData);
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
	/* php's compare_deprecation_thrown, cleared by every entry (see PH7_HashmapUserCmp) */
	pCtx->pVm->bCmpBoolRaised = 0;
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
		HashmapUserSort sSort;
		void *pCmpData = 0;
		ProcNodeCmp xCmp;
		xCmp = HashmapCmpCallback6; /* User-defined function as the comparison callback */
		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){
			/* Point to the desired callback */
			sSort.pCtx = pCtx;
			sSort.pCallback = apArg[1];
			pCmpData = (void *)&sSort;
		}else{
			/* Use the default comparison function */
			xCmp = HashmapCmpCallback2;
		}
		/* Decide the order */
		pCtx->pVm->iCmpCallbackExc = 0;
		HashmapSortDrive(pCtx->pVm,apArg[0],pMap,xCmp,pCmpData);
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
 * (the embedder C-stack family).
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
