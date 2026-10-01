# src/ph7/hashmap_sort.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 672/724 lines (92.82%)

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
|       - |    9 | ` *    Hashmap (array) sorting: the SQLite-derived merge sort, its comparator` |
|       - |   10 | ` *    set and the sort()/usort()/ksort() builtin family.` |
|       - |   11 | ` * Status:` |
|       - |   12 | ` *    Stable.` |
|       - |   13 | ` */` |
|       - |   14 | `/* SPDX-SnippetBegin */` |
|       - |   15 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|       - |   16 | `/* SPDX-License-Identifier: blessing */` |
|       - |   17 | `/*` |
|       - |   18 | ` * Merge sort.` |
|       - |   19 | ` * The merge sort implementation is based on the one found in the SQLite3 source tree.` |
|       - |   20 | ` * Status: Public domain` |
|       - |   21 | ` */` |
|       - |   22 | `/* Node comparison callback signature */` |
|       - |   23 | `/*` |
|       - |   24 | `** Inputs:` |
|       - |   25 | `**   a:       A sorted, null-terminated linked list.  (May be null).` |
|       - |   26 | `**   b:       A sorted, null-terminated linked list.  (May be null).` |
|       - |   27 | `**   cmp:     A pointer to the comparison function.` |
|       - |   28 | `**` |
|       - |   29 | `** Return Value:` |
|       - |   30 | `**   A pointer to the head of a sorted list containing the elements` |
|       - |   31 | `**   of both a and b.` |
|       - |   32 | `**` |
|       - |   33 | `** Side effects:` |
|       - |   34 | `**   The "next","prev" pointers for elements in the lists a and b are` |
|       - |   35 | `**   changed.` |
|       - |   36 | `*/` |
|   96434 |   37 | `static ph7_hashmap_node * HashmapNodeMerge(ph7_hashmap_node *pA,ph7_hashmap_node *pB,ProcNodeCmp xCmp,void *pCmpData)` |
|       5 |   38 | `{` |
|       - |   39 | `	ph7_hashmap_node result,*pTail;` |
|       - |   40 | `    /* Prevent compiler warning */` |
|   96439 |   41 | `	result.pNext = result.pPrev = 0;` |
|   96439 |   42 | `	pTail = &result;` |
|  222422 |   43 | `	while( pA && pB ){` |
|  125988 |   44 | `		if( xCmp(pA,pB,pCmpData) <= 0 ){` |
|   98904 |   45 | `			pTail->pPrev = pA;` |
|   98904 |   46 | `			pA->pNext = pTail;` |
|   98904 |   47 | `			pTail = pA;` |
|   98904 |   48 | `			pA = pA->pPrev;` |
|   49649 |   49 | `		}else{` |
|   27089 |   50 | `			pTail->pPrev = pB;` |
|   27089 |   51 | `			pB->pNext = pTail;` |
|   27089 |   52 | `			pTail = pB;` |
|   27089 |   53 | `			pB = pB->pPrev;` |
|       - |   54 | `		}` |
|       5 |   55 | `	}` |
|   96439 |   56 | `	if( pA ){` |
|    7350 |   57 | `		pTail->pPrev = pA;` |
|    7350 |   58 | `		pA->pNext = pTail;` |
|   92606 |   59 | `	}else if( pB ){` |
|   88540 |   60 | `		pTail->pPrev = pB;` |
|   88540 |   61 | `		pB->pNext = pTail;` |
|   44301 |   62 | `	}else{` |
|     559 |   63 | `		pTail->pPrev = pTail->pNext = 0;` |
|       - |   64 | `	}` |
|   96439 |   65 | `	return result.pPrev;` |
|       5 |   66 | `}` |
|       - |   67 | `/*` |
|       - |   68 | `** Inputs:` |
|       - |   69 | `**   Map:       Input hashmap` |
|       - |   70 | `**   cmp:       A comparison function.` |
|       - |   71 | `**` |
|       - |   72 | `** Return Value:` |
|       - |   73 | `**   Sorted hashmap.` |
|       - |   74 | `**` |
|       - |   75 | `** Side effects:` |
|       - |   76 | `**   The "next" pointers for elements in list are changed.` |
|       - |   77 | `*/` |
|       - |   78 | `#define N_SORT_BUCKET  32` |
|    2354 |   79 | `PH7_PRIVATE sxi32 HashmapMergeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData)` |
|       5 |   80 | `{` |
|       - |   81 | `	ph7_hashmap_node *a[N_SORT_BUCKET], *p,*pIn;` |
|       - |   82 | `	sxu32 i;` |
|    2359 |   83 | `	SyZero(a,sizeof(a));` |
|       - |   84 | `	/* Point to the first inserted entry */` |
|    2359 |   85 | `	pIn = pMap->pFirst;` |
|   30055 |   86 | `	while( pIn ){` |
|   27701 |   87 | `		p = pIn;` |
|   27701 |   88 | `		pIn = p->pPrev;` |
|   27701 |   89 | `		p->pPrev = 0;` |
|   51161 |   90 | `		for(i=0; i<N_SORT_BUCKET-1; i++){` |
|   51161 |   91 | `			if( a[i]==0 ){` |
|   27701 |   92 | `				a[i] = p;` |
|   27701 |   93 | `				break;` |
|     ! 0 |   94 | `			}else{` |
|   23465 |   95 | `				p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|   23465 |   96 | `				a[i] = 0;` |
|       - |   97 | `			}` |
|   11722 |   98 | `		}` |
|   27701 |   99 | `		if( i==N_SORT_BUCKET-1 ){` |
|       - |  100 | `			/* To get here, there need to be 2^(N_SORT_BUCKET) elements in he input list.` |
|       - |  101 | `			 * But that is impossible.` |
|       - |  102 | `			 */` |
|     ! 0 |  103 | `			a[i] = HashmapNodeMerge(a[i], p,xCmp,pCmpData);` |
|     ! 0 |  104 | `		}` |
|       5 |  105 | `	}` |
|    2359 |  106 | `	p = a[0];` |
|   75333 |  107 | `	for(i=1; i<N_SORT_BUCKET; i++){` |
|       - |  108 | `		/* Higher-index buckets hold EARLIER-inserted (and larger) runs, so the` |
|       - |  109 | `		 * bucket must be the LEFT operand: HashmapNodeMerge favors its left arg on` |
|       - |  110 | `		 * a tie (cmp <= 0), and php's sorts are stable (PHP 8.0+) — equal elements` |
|       - |  111 | `		 * keep their original order. Passing p (the later elements) on the left` |
|       - |  112 | `		 * reversed equal runs (e.g. usort of five tie-keyed items moved the last to` |
|       - |  113 | `		 * the front). */` |
|   72979 |  114 | `		p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|   36368 |  115 | `	}` |
|    2359 |  116 | `	p->pNext = 0;` |
|       - |  117 | `	/* Reflect the change */` |
|    2359 |  118 | `	pMap->pFirst = p;` |
|       - |  119 | `	/* Reset the loop cursor */` |
|    2359 |  120 | `	pMap->pCur = pMap->pFirst;` |
|    2359 |  121 | `	return SXRET_OK;` |
|       5 |  122 | `}` |
|       - |  123 | `/* SPDX-SnippetEnd */` |
|       - |  124 | `/*` |
|       - |  125 | ` * Coerce one operand of a STRING-flag comparison (SORT_STRING and friends), the` |
|       - |  126 | ` * way php's zval_get_string() does. An ARRAY warns "Array to string conversion"` |
|       - |  127 | ` * and renders as "Array"; an object with no __toString() raises php's catchable` |
|       - |  128 | ` * "could not be converted to string" Error and renders as the EMPTY string --` |
|       - |  129 | ` * which is why php's array comes out fully SORTED after the throw, with the` |
|       - |  130 | ` * object first. PHL used to render it as the literal "Object" and sort on that,` |
|       - |  131 | ` * silently.` |
|       - |  132 | ` *` |
|       - |  133 | ` * A comparator has no status channel, so the Error is raised once per sort and` |
|       - |  134 | ` * flagged on the VM through iCmpCallbackExc -- the rail a throwing user callback` |
|       - |  135 | ` * already uses; every flag-sort driver clears the flag before its merge sort and` |
|       - |  136 | ` * answers PH7_EXCEPTION after it (HashmapFlagSortStatus), and array_unique() does` |
|       - |  137 | ` * the same around its walk. The flag is also what keeps the second and later` |
|       - |  138 | ` * comparisons from raising the same Error again.` |
|       - |  139 | ` */` |
|     482 |  140 | `static void HashmapFlagStringify(ph7_value *pVal)` |
|       4 |  141 | `{` |
|     486 |  142 | `	ph7_vm *pVm = pVal->pVm;` |
|     486 |  143 | `	sxi32 rc = PH7_EXCEPTION;` |
|     482 |  144 | `	if( (pVal->iFlags & MEMOBJ_OBJ) && pVm` |
|      86 |  145 | `	 && (pVm->iCmpCallbackExc != 0 \|\| PH7_CALLBACK_UNWOUND(pVm->nBoundaryRc)) ){` |
|       - |  146 | `		/* This sort already coerced an object once and it did not succeed: php` |
|       - |  147 | `		 * raises the refusal Error once, and it enters no PHP function at all` |
|       - |  148 | `		 * while an exception is pending (zend_call_function bails on` |
|       - |  149 | `		 * EG(exception)), so neither arm runs again -- the operand just renders` |
|       - |  150 | `		 * as the empty string a refused cast gives. The raise-once guard used to` |
|       - |  151 | `		 * cover only the NOT-STRINGABLE arm, so a throwing __toString() body went` |
|       - |  152 | `		 * round again on the next pair; uncaught, it reported the fatal once per` |
|       - |  153 | `		 * comparison, and after a catch had run in place the second throw was` |
|       - |  154 | `		 * uncaught and killed a script php merely says "caught" in.` |
|       - |  155 | `		 * OBJECTS only: blanking an int or a string here would re-order the rest` |
|       - |  156 | `		 * of the array. */` |
|      26 |  157 | `		PH7_MemObjRelease(pVal);` |
|      26 |  158 | `		MemObjSetType(pVal,MEMOBJ_STRING);` |
|      26 |  159 | `		return;` |
|       - |  160 | `	}` |
|     462 |  161 | `	if( !PH7_MemObjIsNotStringable(pVal) ){` |
|     424 |  162 | `		rc = PH7_MemObjToStringUV(pVal);` |
|     424 |  163 | `		if( rc == SXRET_OK ){` |
|     416 |  164 | `			return;` |
|       - |  165 | `		}` |
|       - |  166 | `		/* A __toString() that THREW. Same shape as a refused cast from here on. */` |
|       5 |  167 | `	}else{` |
|      40 |  168 | `		rc = PH7_MemObjToStringUV(pVal); /* raises php's Error */` |
|       - |  169 | `	}` |
|      48 |  170 | `	if( pVm ){` |
|       - |  171 | `		/* Latch the STATUS the coercion answered, so an UNCAUGHT throw (or an` |
|       - |  172 | `		 * exit()) out of __toString() leaves the sort with PH7_ABORT rather than` |
|       - |  173 | `		 * a downgraded PH7_EXCEPTION. */` |
|      48 |  174 | `		pVm->iCmpCallbackExc = PH7_CALLBACK_UNWOUND(rc) ? rc : PH7_EXCEPTION;` |
|      23 |  175 | `	}` |
|      48 |  176 | `	PH7_MemObjRelease(pVal);` |
|      48 |  177 | `	MemObjSetType(pVal,MEMOBJ_STRING);` |
|     243 |  178 | `}` |
|       - |  179 | `/*` |
|       - |  180 | ` * Node comparison callback.` |
|       - |  181 | ` * used-by: [sort(),asort(),...]` |
|       - |  182 | ` */` |
|       - |  183 | `/*` |
|       - |  184 | ` * Compare two scalar values under an EXPLICIT php sort base type (never 0 —` |
|       - |  185 | ` * SORT_REGULAR is handled by the callers, which differ for keys vs values):` |
|       - |  186 | ` *   SORT_NUMERIC 1 · SORT_STRING 2 · SORT_LOCALE_STRING 5 · SORT_NATURAL 6,` |
|       - |  187 | ` * with bFold applying SORT_FLAG_CASE. PHL has no locale tables, so` |
|       - |  188 | ` * SORT_LOCALE_STRING behaves like SORT_STRING. Mutates both operands (numeric or` |
|       - |  189 | ` * string cast); the caller owns and releases them.` |
|       - |  190 | ` */` |
| 1437916 |  191 | `static sxi32 HashmapScalarFlagCmp(ph7_value *pA,ph7_value *pB,int base,int bFold)` |
|       5 |  192 | `{` |
|       - |  193 | `	sxi32 rc;` |
| 1437921 |  194 | `	if( base == 1 ){` |
|       - |  195 | `		/* SORT_NUMERIC compares as DOUBLES. php's numeric_compare_function is` |
|       - |  196 | ``		 * `zval_get_double(a)` vs `zval_get_double(b)` under ZEND_THREEWAY_COMPARE`` |
|       - |  197 | `		 * — not a value comparison over whatever type each operand happens to` |
|       - |  198 | `		 * settle on. Two consequences PHL got wrong by folding to a NUMBER and` |
|       - |  199 | `		 * calling the ordinary comparator: the diagnostic named the wrong type` |
|       - |  200 | `		 * (an object warned "could not be converted to int" where php says` |
|       - |  201 | `		 * "to float"), and integers above 2^53 were ordered EXACTLY where php's` |
|       - |  202 | ``		 * doubles tie — `sort([PHP_INT_MAX, PHP_INT_MAX-1], SORT_NUMERIC)` left`` |
|       - |  203 | `		 * php's stable sort's original order and PHL re-ordered them. Matching php` |
|       - |  204 | `		 * means adopting its precision loss, which is what parity is (§10).` |
|       - |  205 | `		 * The == / < shape is ZEND_THREEWAY_COMPARE's, so NaN — equal to nothing,` |
|       - |  206 | `		 * less than nothing — answers 1 in both engines. */` |
|       - |  207 | `		ph7_real rA,rB;` |
|     230 |  208 | `		PH7_MemObjToReal(pA);` |
|     230 |  209 | `		PH7_MemObjToReal(pB);` |
|     230 |  210 | `		rA = pA->rVal;` |
|     230 |  211 | `		rB = pB->rVal;` |
|     230 |  212 | `		rc = (rA == rB) ? 0 : ((rA < rB) ? -1 : 1);` |
|     116 |  213 | `	}else{` |
|       - |  214 | `		/* SORT_STRING (2) / SORT_LOCALE_STRING (5) / SORT_NATURAL (6) */` |
|       - |  215 | `		const char *zA,*zB;` |
|       - |  216 | `		sxu32 nA,nB,nMin,i;` |
| 1437693 |  217 | `		if( (pA->iFlags & MEMOBJ_STRING) == 0 ){ HashmapFlagStringify(pA); }` |
| 1437693 |  218 | `		if( (pB->iFlags & MEMOBJ_STRING) == 0 ){ HashmapFlagStringify(pB); }` |
| 1437693 |  219 | `		zA = (const char *)SyBlobData(&pA->sBlob);` |
| 1437693 |  220 | `		zB = (const char *)SyBlobData(&pB->sBlob);` |
| 1437693 |  221 | `		nA = SyBlobLength(&pA->sBlob);` |
| 1437693 |  222 | `		nB = SyBlobLength(&pB->sBlob);` |
| 1437693 |  223 | `		if( base == 6 ){` |
|     189 |  224 | `			rc = PH7_StrNatCmp(zA,(int)nA,zB,(int)nB,bFold);` |
|      96 |  225 | `		}else{` |
|       - |  226 | `			/* Lexicographic comparison (binary-safe), case-folded on request. */` |
| 1437507 |  227 | `			nMin = nA < nB ? nA : nB;` |
| 1437507 |  228 | `			rc = 0;` |
| 2933162 |  229 | `			for( i = 0 ; i < nMin ; ++i ){` |
| 2930558 |  230 | `				int ca = (unsigned char)zA[i];` |
| 2930558 |  231 | `				int cb = (unsigned char)zB[i];` |
| 2930558 |  232 | `				if( bFold ){ ca = SyToLower(ca); cb = SyToLower(cb); }` |
| 2930558 |  233 | `				if( ca != cb ){ rc = ca < cb ? -1 : 1; break; }` |
|  764908 |  234 | `			}` |
| 1437507 |  235 | `			if( rc == 0 ){` |
|    2609 |  236 | `				if( nA < nB ) rc = -1;` |
|     639 |  237 | `				else if( nA > nB ) rc = 1;` |
|    1306 |  238 | `			}` |
|       - |  239 | `		}` |
|       - |  240 | `	}` |
| 1437921 |  241 | `	return rc;` |
|       5 |  242 | `}` |
|       - |  243 | `/*` |
|       - |  244 | ` * Are two live values equal under an array_unique() sort_flags? A non-mutating` |
|       - |  245 | ` * wrapper (works on private copies): base 0 = SORT_REGULAR loose comparison,` |
|       - |  246 | ` * explicit flags route through HashmapScalarFlagCmp. Used by array_unique.` |
|       - |  247 | ` */` |
| 1375076 |  248 | `PH7_PRIVATE int HashmapValueFlagEqual(ph7_vm *pVm,ph7_value *pA,ph7_value *pB,int base,int bFold)` |
|       5 |  249 | `{` |
|       - |  250 | `	ph7_value sA,sB;` |
|       - |  251 | `	sxi32 rc;` |
| 1375081 |  252 | `	PH7_MemObjInit(pVm,&sA);` |
| 1375081 |  253 | `	PH7_MemObjInit(pVm,&sB);` |
| 1375081 |  254 | `	PH7_MemObjStore(pA,&sA);` |
| 1375081 |  255 | `	PH7_MemObjStore(pB,&sB);` |
| 1375081 |  256 | `	if( base == 0 ){` |
|      18 |  257 | `		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0); /* SORT_REGULAR: loose comparison */` |
|      10 |  258 | `	}else{` |
| 1375065 |  259 | `		rc = HashmapScalarFlagCmp(&sA,&sB,base,bFold);` |
|       - |  260 | `	}` |
| 1375081 |  261 | `	PH7_MemObjRelease(&sA);` |
| 1375081 |  262 | `	PH7_MemObjRelease(&sB);` |
| 1375081 |  263 | `	return rc == 0;` |
|       5 |  264 | `}` |
|       - |  265 | `/*` |
|       - |  266 | ` * Compare two node VALUES under php's sort_flags (base 0 = SORT_REGULAR uses the` |
|       - |  267 | ` * standard value comparison; explicit flags route through HashmapScalarFlagCmp).` |
|       - |  268 | ` */` |
|   62818 |  269 | `static sxi32 HashmapFlagValueCmp(ph7_hashmap_node *pA,ph7_hashmap_node *pB,sxi32 iFlags)` |
|       5 |  270 | `{` |
|       - |  271 | `	ph7_value sA,sB;` |
|   62823 |  272 | `	int base = iFlags & ~8;      /* strip SORT_FLAG_CASE */` |
|   62823 |  273 | `	int bFold = (iFlags & 8) != 0;` |
|       - |  274 | `	sxi32 rc;` |
|   62823 |  275 | `	if( base == 0 ){` |
|       - |  276 | `		/* SORT_REGULAR */` |
|      93 |  277 | `		return HashmapNodeCmp(pA,pB,FALSE);` |
|       - |  278 | `	}` |
|   62733 |  279 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|   62733 |  280 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|   62733 |  281 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|   62733 |  282 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|   62733 |  283 | `	rc = HashmapScalarFlagCmp(&sA,&sB,base,bFold);` |
|   62733 |  284 | `	PH7_MemObjRelease(&sA);` |
|   62733 |  285 | `	PH7_MemObjRelease(&sB);` |
|   62733 |  286 | `	return rc;` |
|   31348 |  287 | `}` |
|       - |  288 | `/*` |
|       - |  289 | ` * A sort comparison can run USER code with no way to report what it did: an` |
|       - |  290 | `` * object operand's `__toString()` is reached by SORT_REGULAR's value comparison`` |
|       - |  291 | ` * as well as by the string flags' coercion, and that body can throw or exit().` |
|       - |  292 | ` * PH7_MemObjCmp only ever answers an ORDERING, so the status is read off the VM` |
|       - |  293 | ` * instead — every C->PHP dispatch parks an unwind in nBoundaryRc (VmBoundaryPark)` |
|       - |  294 | ` * and nothing inside a builtin consumes it, so it is still there when the` |
|       - |  295 | ` * comparison returns.` |
|       - |  296 | ` *` |
|       - |  297 | ` * Two things follow, and both were missing: the merge sort must STAND DOWN (a` |
|       - |  298 | `` * throwing `__toString()` ran again on the next pair -- and since the enclosing`` |
|       - |  299 | ` * catch had already run in place, the second throw was UNCAUGHT and killed a` |
|       - |  300 | ` * script php merely reports "caught" in), and the driver must answer with that` |
|       - |  301 | `` * status rather than `true`.`` |
|       - |  302 | ` *` |
|       - |  303 | ` * Deliberately NOT triggered by a not-stringable object's own coercion Error:` |
|       - |  304 | ` * that one parks nothing and is flagged by HashmapFlagStringify, and php goes on` |
|       - |  305 | ` * comparing after it (its array comes out fully sorted, the object first).` |
|       - |  306 | ` */` |
|  124907 |  307 | `static void HashmapCmpLatch(ph7_vm *pVm)` |
|       5 |  308 | `{` |
|  124907 |  309 | `	if( PH7_CALLBACK_UNWOUND(pVm->nBoundaryRc)` |
|   62471 |  310 | `	 && (pVm->iCmpCallbackExc == 0 \|\| pVm->nBoundaryRc == PH7_ABORT) ){` |
|     123 |  311 | `		pVm->iCmpCallbackExc = pVm->nBoundaryRc;` |
|      75 |  312 | `	}` |
|  124884 |  313 | `}` |
|  123974 |  314 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       5 |  315 | `{` |
|  123979 |  316 | `	ph7_vm *pVm = pA->pMap->pVm;` |
|       - |  317 | `	sxi32 rc;` |
|  123979 |  318 | `	if( pCmpData == 0 ){` |
|       - |  319 | `		/* SORT_REGULAR fast path */` |
|   61598 |  320 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|   30800 |  321 | `	}else{` |
|   62386 |  322 | `		rc = HashmapFlagValueCmp(pA,pB,SX_PTR_TO_INT(pCmpData));` |
|       - |  323 | `	}` |
|  123979 |  324 | `	HashmapCmpLatch(pVm);` |
|  123979 |  325 | `	return rc;` |
|       5 |  326 | `}` |
|       - |  327 | `/*` |
|       - |  328 | ` * Materialise a node's KEY as a scalar ph7_value (int key -> integer, string key` |
|       - |  329 | ` * -> string) for a flag-aware key comparison.` |
|       - |  330 | ` */` |
|     812 |  331 | `static void HashmapNodeKeyToValue(ph7_hashmap_node *pNode,ph7_value *pOut)` |
|       4 |  332 | `{` |
|     816 |  333 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|     110 |  334 | `		PH7_MemObjInitFromInt(pNode->pMap->pVm,pOut,pNode->xKey.iKey);` |
|      56 |  335 | `	}else{` |
|     708 |  336 | `		PH7_MemObjInitFromString(pNode->pMap->pVm,pOut,0);` |
|    1060 |  337 | `		PH7_MemObjStringAppend(pOut,(const char *)SyBlobData(&pNode->xKey.sKey),` |
|     352 |  338 | `			SyBlobLength(&pNode->xKey.sKey));` |
|       - |  339 | `	}` |
|     816 |  340 | `}` |
|       - |  341 | `/*` |
|       - |  342 | ` * Shared key comparison for ksort()/krsort() under SORT_REGULAR: php compares` |
|       - |  343 | ` * two array KEYS exactly the way it compares two VALUES, so materialise them` |
|       - |  344 | `` * and hand them to the standard comparison — the same one sort()/`<=>` use.`` |
|       - |  345 | ` *` |
|       - |  346 | ` * The hand-rolled version this replaces got the mixed int/string case right but` |
|       - |  347 | ` * compared two STRING keys BYTEWISE, so two numeric strings sorted by their` |
|       - |  348 | ` * bytes: ksort(['10.0'=>1,'9.0'=>2]) answered ['10.0','9.0'] where php answers` |
|       - |  349 | ` * ['9.0','10.0'], ksort(['1e3'=>1,'20'=>2]) put 1e3 (1000) first, and keys that` |
|       - |  350 | ` * compare EQUAL ('1.0', '01', 1) lost php's stable order.` |
|       - |  351 | ` */` |
|     336 |  352 | `static sxi32 HashmapKeyNodeCmp(ph7_hashmap_node *pA,ph7_hashmap_node *pB)` |
|       2 |  353 | `{` |
|       - |  354 | `	ph7_value sA,sB;` |
|       - |  355 | `	sxi32 rc;` |
|     338 |  356 | `	if( pA->iType == HASHMAP_INT_NODE && pB->iType == HASHMAP_INT_NODE ){` |
|       - |  357 | `		/* Two integer keys: the common case, and no allocation needed */` |
|      59 |  358 | `		return pA->xKey.iKey < pB->xKey.iKey ? -1 : (pA->xKey.iKey > pB->xKey.iKey ? 1 : 0);` |
|       - |  359 | `	}` |
|     280 |  360 | `	HashmapNodeKeyToValue(pA,&sA);` |
|     280 |  361 | `	HashmapNodeKeyToValue(pB,&sB);` |
|     280 |  362 | `	rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|     280 |  363 | `	PH7_MemObjRelease(&sA);` |
|     280 |  364 | `	PH7_MemObjRelease(&sB);` |
|     280 |  365 | `	return rc;` |
|     170 |  366 | `}` |
|       - |  367 | `/*` |
|       - |  368 | ` * Compare two node KEYS under php's sort_flags. base 0 = SORT_REGULAR keeps the` |
|       - |  369 | ` * php-8 mixed int/string key semantics (HashmapKeyNodeCmp); explicit flags route` |
|       - |  370 | ` * the materialised keys through HashmapScalarFlagCmp.` |
|       - |  371 | ` */` |
|     128 |  372 | `static sxi32 HashmapFlagKeyCmp(ph7_hashmap_node *pA,ph7_hashmap_node *pB,sxi32 iFlags)` |
|       3 |  373 | `{` |
|       - |  374 | `	ph7_value sA,sB;` |
|     131 |  375 | `	int base = iFlags & ~8;      /* strip SORT_FLAG_CASE */` |
|     131 |  376 | `	int bFold = (iFlags & 8) != 0;` |
|       - |  377 | `	sxi32 rc;` |
|     131 |  378 | `	if( base == 0 ){` |
|     ! 0 |  379 | `		return HashmapKeyNodeCmp(pA,pB);` |
|       - |  380 | `	}` |
|     131 |  381 | `	HashmapNodeKeyToValue(pA,&sA);` |
|     131 |  382 | `	HashmapNodeKeyToValue(pB,&sB);` |
|     131 |  383 | `	rc = HashmapScalarFlagCmp(&sA,&sB,base,bFold);` |
|     131 |  384 | `	PH7_MemObjRelease(&sA);` |
|     131 |  385 | `	PH7_MemObjRelease(&sB);` |
|     131 |  386 | `	return rc;` |
|      67 |  387 | `}` |
|       - |  388 | `/*` |
|       - |  389 | ` * Node comparison callback: Compare nodes by keys only.` |
|       - |  390 | ` * used-by: [ksort()]` |
|       - |  391 | ` */` |
|     398 |  392 | `static sxi32 HashmapCmpCallback2(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       4 |  393 | `{` |
|     402 |  394 | `	ph7_vm *pVm = pA->pMap->pVm;` |
|       - |  395 | `	sxi32 rc;` |
|     402 |  396 | `	if( pCmpData == 0 ){` |
|     278 |  397 | `		rc = HashmapKeyNodeCmp(pA,pB);` |
|     140 |  398 | `	}else{` |
|     125 |  399 | `		rc = HashmapFlagKeyCmp(pA,pB,SX_PTR_TO_INT(pCmpData));` |
|       - |  400 | `	}` |
|     402 |  401 | `	HashmapCmpLatch(pVm);` |
|     402 |  402 | `	return rc;` |
|       4 |  403 | `}` |
|       - |  404 | `/*` |
|       - |  405 | ` * Node comparison callback.` |
|       - |  406 | ` * Used by: [rsort(),arsort()];` |
|       - |  407 | ` */` |
|     429 |  408 | `static sxi32 HashmapCmpCallback3(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       5 |  409 | `{` |
|     434 |  410 | `	ph7_vm *pVm = pA->pMap->pVm;` |
|       - |  411 | `	sxi32 rc;` |
|     434 |  412 | `	if( pCmpData == 0 ){` |
|       - |  413 | `		/* SORT_REGULAR fast path, reversed */` |
|     101 |  414 | `		rc = -HashmapNodeCmp(pA,pB,FALSE);` |
|      52 |  415 | `	}else{` |
|     335 |  416 | `		rc = -HashmapFlagValueCmp(pA,pB,SX_PTR_TO_INT(pCmpData));` |
|       - |  417 | `	}` |
|     434 |  418 | `	HashmapCmpLatch(pVm);` |
|     434 |  419 | `	return rc;` |
|       5 |  420 | `}` |
|       - |  421 | `/*` |
|       - |  422 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|       - |  423 | ` * used-by: [usort(),uasort()]` |
|       - |  424 | ` */` |
|    1096 |  425 | `static sxi32 HashmapCmpCallback4(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       5 |  426 | `{` |
|       - |  427 | `	ph7_value sResult,*pCallback;` |
|       - |  428 | `	ph7_value *pV1,*pV2;` |
|       - |  429 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|       - |  430 | `	sxi32 rc;` |
|       - |  431 | `	/* Point to the desired callback */` |
|    1101 |  432 | `	pCallback = (ph7_value *)pCmpData;` |
|    1101 |  433 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|       - |  434 | `		/* A previous comparison already raised: stop invoking the callback so` |
|       - |  435 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|      27 |  436 | `		return 0;` |
|       - |  437 | `	}` |
|       - |  438 | `	/* initialize the result value */` |
|    1077 |  439 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|       - |  440 | `	/* Extract nodes values */` |
|    1077 |  441 | `	pV1 = HashmapExtractNodeValue(pA);` |
|    1077 |  442 | `	pV2 = HashmapExtractNodeValue(pB);` |
|    1077 |  443 | `	apArg[0] = pV1;` |
|    1077 |  444 | `	apArg[1] = pV2;` |
|       - |  445 | `	/* Invoke the callback */` |
|    1077 |  446 | `	rc = PH7_VmCallCallbackByValue(pA->pMap->pVm,pCallback,2,apArg,&sResult,0);` |
|    1077 |  447 | `	if( PH7_CALLBACK_UNWOUND(rc) ){` |
|       - |  448 | `		/* The comparator did not RETURN: latch the STATUS so the sort driver` |
|       - |  449 | `		 * aborts and propagates exactly it (an UNCAUGHT throw is PH7_ABORT, and` |
|       - |  450 | `		 * testing only PH7_EXCEPTION left the sort running -- re-entering the` |
|       - |  451 | `		 * comparator, and the fatal report, for every remaining pair), and order` |
|       - |  452 | `		 * this pair arbitrarily for the rest of the run. */` |
|      19 |  453 | `		pA->pMap->pVm->iCmpCallbackExc = rc;` |
|      19 |  454 | `		rc = 0;` |
|    1069 |  455 | `	}else if( rc != SXRET_OK ){` |
|       - |  456 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|     ! 0 |  457 | `		rc = -1; /* Set a dummy result */` |
|     ! 0 |  458 | `	}else{` |
|       - |  459 | `		/* Extract callback result */` |
|    1061 |  460 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  461 | `			/* Perform an int cast */` |
|     ! 0 |  462 | `			PH7_MemObjToInteger(&sResult);` |
|     ! 0 |  463 | `		}` |
|    1061 |  464 | `		rc = (sxi32)sResult.x.iVal;` |
|       - |  465 | `	}` |
|    1077 |  466 | `	PH7_MemObjRelease(&sResult);` |
|       - |  467 | `	/* Callback result */` |
|    1077 |  468 | `	return rc;` |
|     518 |  469 | `}` |
|       - |  470 | `/*` |
|       - |  471 | ` * Node comparison callback: Compare nodes by keys only.` |
|       - |  472 | ` * used-by: [krsort()]` |
|       - |  473 | ` */` |
|      66 |  474 | `static sxi32 HashmapCmpCallback5(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       2 |  475 | `{` |
|      68 |  476 | `	if( pCmpData == 0 ){` |
|      61 |  477 | `		return -HashmapKeyNodeCmp(pA,pB); /* Reverse result */` |
|       - |  478 | `	}` |
|       8 |  479 | `	return -HashmapFlagKeyCmp(pA,pB,SX_PTR_TO_INT(pCmpData));` |
|      35 |  480 | `}` |
|       - |  481 | `/*` |
|       - |  482 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|       - |  483 | ` * used-by: [uksort()]` |
|       - |  484 | ` */` |
|      20 |  485 | `static sxi32 HashmapCmpCallback6(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       3 |  486 | `{` |
|       - |  487 | `	ph7_value sResult,*pCallback;` |
|       - |  488 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|       - |  489 | `	ph7_value sK1,sK2;` |
|       - |  490 | `	sxi32 rc;` |
|       - |  491 | `	/* Point to the desired callback */` |
|      23 |  492 | `	pCallback = (ph7_value *)pCmpData;` |
|      23 |  493 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|       - |  494 | `		/* A previous comparison already raised: stop invoking the callback so` |
|       - |  495 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|       7 |  496 | `		return 0;` |
|       - |  497 | `	}` |
|       - |  498 | `	/* initialize the result value */` |
|      17 |  499 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|      17 |  500 | `	PH7_MemObjInit(pA->pMap->pVm,&sK1);` |
|      17 |  501 | `	PH7_MemObjInit(pA->pMap->pVm,&sK2);` |
|       - |  502 | `	/* Extract nodes keys */` |
|      17 |  503 | `	PH7_HashmapExtractNodeKey(pA,&sK1);` |
|      17 |  504 | `	PH7_HashmapExtractNodeKey(pB,&sK2);` |
|      17 |  505 | `	apArg[0] = &sK1;` |
|      17 |  506 | `	apArg[1] = &sK2;` |
|       - |  507 | `	/* Mark keys as constants */` |
|      17 |  508 | `	sK1.nIdx = SXU32_HIGH;` |
|      17 |  509 | `	sK2.nIdx = SXU32_HIGH;` |
|       - |  510 | `	/* Invoke the callback */` |
|      17 |  511 | `	rc = PH7_VmCallCallbackByValue(pA->pMap->pVm,pCallback,2,apArg,&sResult,0);` |
|      17 |  512 | `	if( PH7_CALLBACK_UNWOUND(rc) ){` |
|       - |  513 | `		/* The comparator did not RETURN: latch the STATUS so the sort driver` |
|       - |  514 | `		 * aborts and propagates exactly it (an UNCAUGHT throw is PH7_ABORT, and` |
|       - |  515 | `		 * testing only PH7_EXCEPTION left the sort running -- re-entering the` |
|       - |  516 | `		 * comparator, and the fatal report, for every remaining pair), and order` |
|       - |  517 | `		 * this pair arbitrarily for the rest of the run. */` |
|       3 |  518 | `		pA->pMap->pVm->iCmpCallbackExc = rc;` |
|       3 |  519 | `		rc = 0;` |
|      15 |  520 | `	}else if( rc != SXRET_OK ){` |
|       - |  521 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|     ! 0 |  522 | `		rc = -1; /* Set a dummy result */` |
|     ! 0 |  523 | `	}else{` |
|       - |  524 | `		/* Extract callback result */` |
|      14 |  525 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  526 | `			/* Perform an int cast */` |
|     ! 0 |  527 | `			PH7_MemObjToInteger(&sResult);` |
|     ! 0 |  528 | `		}` |
|      14 |  529 | `		rc = (sxi32)sResult.x.iVal;` |
|       - |  530 | `	}` |
|      17 |  531 | `	PH7_MemObjRelease(&sResult);` |
|      17 |  532 | `	PH7_MemObjRelease(&sK1);` |
|      17 |  533 | `	PH7_MemObjRelease(&sK2);` |
|       - |  534 | `	/* Callback result */` |
|      17 |  535 | `	return rc;` |
|      13 |  536 | `}` |
|       - |  537 | `/*` |
|       - |  538 | ` * Permute a hashmap's entries the way php's shuffle() does: Fisher-Yates over` |
|       - |  539 | ` * the buckets, drawing each index from the MT19937 generator through` |
|       - |  540 | ` * php_mt_rand_range(). The caller rehashes afterwards (php reindexes the array` |
|       - |  541 | ` * 0..n-1 and drops string keys).` |
|       - |  542 | ` *` |
|       - |  543 | ` * This used to be a merge sort with a coin-flip comparator, which is a permuting` |
|       - |  544 | ` * shuffle but not a UNIFORM one — the distribution a random comparator produces` |
|       - |  545 | ` * is skewed and depends on the sort's internals — and it consumed the generator` |
|       - |  546 | ` * in a different order, so a seeded run answered a different permutation from` |
|       - |  547 | ` * php's for every seed.` |
|       - |  548 | ` */` |
|       8 |  549 | `PH7_PRIVATE sxi32 PH7_HashmapShuffle(ph7_hashmap *pMap)` |
|       2 |  550 | `{` |
|       - |  551 | `	ph7_hashmap_node **apNode,*pNode;` |
|       - |  552 | `	sxu32 n,nLeft;` |
|       - |  553 | `	SySet aNode;` |
|      10 |  554 | `	if( pMap->nEntry < 2 ){` |
|     ! 0 |  555 | `		return SXRET_OK;` |
|       - |  556 | `	}` |
|      10 |  557 | `	SySetInit(&aNode,&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|      50 |  558 | `	for( pNode = pMap->pFirst ; pNode ; pNode = pNode->pPrev ){` |
|      42 |  559 | `		if( SySetPut(&aNode,(const void *)&pNode) != SXRET_OK ){` |
|     ! 0 |  560 | `			SySetRelease(&aNode);` |
|     ! 0 |  561 | `			return SXERR_MEM;` |
|       - |  562 | `		}` |
|      22 |  563 | `	}` |
|      10 |  564 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&aNode);` |
|      10 |  565 | `	n = SySetUsed(&aNode);` |
|       - |  566 | `	/* php walks DOWN from the last index, swapping with a draw in [0,n_left]. */` |
|      42 |  567 | `	for( nLeft = n - 1 ; nLeft > 0 ; --nLeft ){` |
|      34 |  568 | `		sxu32 nPick = (sxu32)PH7_VmMtRandRange(pMap->pVm,0,(sxi64)nLeft);` |
|      34 |  569 | `		if( nPick != nLeft ){` |
|      24 |  570 | `			ph7_hashmap_node *pTmp = apNode[nLeft];` |
|      24 |  571 | `			apNode[nLeft] = apNode[nPick];` |
|      24 |  572 | `			apNode[nPick] = pTmp;` |
|      11 |  573 | `		}` |
|      18 |  574 | `	}` |
|       - |  575 | `	/* Relink in the new order. pPrev is the forward link and pNext the back one` |
|       - |  576 | `	 * (the whole map is built that way); the rehash after this fixes pLast. */` |
|      50 |  577 | `	for( n = 0 ; n < SySetUsed(&aNode) ; ++n ){` |
|      42 |  578 | `		apNode[n]->pPrev = (n + 1 < SySetUsed(&aNode)) ? apNode[n+1] : 0;` |
|      42 |  579 | `		apNode[n]->pNext = (n > 0) ? apNode[n-1] : 0;` |
|      22 |  580 | `	}` |
|      10 |  581 | `	pMap->pFirst = apNode[0];` |
|      10 |  582 | `	pMap->pLast = apNode[SySetUsed(&aNode) - 1];` |
|      10 |  583 | `	pMap->pCur = pMap->pFirst;` |
|      10 |  584 | `	SySetRelease(&aNode);` |
|      10 |  585 | `	return SXRET_OK;` |
|       6 |  586 | `}` |
|       - |  587 | `/*` |
|       - |  588 | ` * Rehash all nodes keys after a merge-sort have been applied.` |
|       - |  589 | ` * Used by [sort(),usort() and rsort()].` |
|       - |  590 | ` */` |
|    2192 |  591 | `PH7_PRIVATE void HashmapSortRehash(ph7_hashmap *pMap)` |
|       5 |  592 | `{` |
|       - |  593 | `	ph7_hashmap_node *p,*pLast;` |
|       - |  594 | `	sxu32 i;` |
|       - |  595 | `	/* php's sorts rewind the array's internal pointer. The reordering paths reset` |
|       - |  596 | `	 * it themselves, but a ONE-element array is reindexed without being reordered` |
|       - |  597 | ``	 * — and `$a = ['x'=>1]; next($a); sort($a);` then left current() past the end,`` |
|       - |  598 | `	 * answering false where php answers the element. */` |
|    2197 |  599 | `	pMap->pCur = pMap->pFirst;` |
|       - |  600 | `	/* Rehash all entries */` |
|    2197 |  601 | `	pLast = p = pMap->pFirst;` |
|    2197 |  602 | `	pMap->iNextIdx = 0;` |
|    2197 |  603 | `	pMap->bIntKeySeen = 0; /* Reset the automatic index */` |
|    2197 |  604 | `	i = 0;` |
|   14559 |  605 | `	for( ;; ){` |
|   29167 |  606 | `		if( i >= pMap->nEntry ){` |
|    2197 |  607 | `			pMap->pLast = pLast; /* Fix the last link broken by the merge-sort */` |
|    2197 |  608 | `			break;` |
|       - |  609 | `		}` |
|   26975 |  610 | `		if( p->iType == HASHMAP_BLOB_NODE ){` |
|       - |  611 | `			/* Do not maintain index association as requested by the PHP specification */` |
|      50 |  612 | `			SyBlobRelease(&p->xKey.sKey);` |
|       - |  613 | `			/* Change key type */` |
|      50 |  614 | `			p->iType = HASHMAP_INT_NODE;` |
|      24 |  615 | `		}` |
|   26975 |  616 | `		HashmapRehashIntNode(p);` |
|       - |  617 | `		/* Point to the next entry */` |
|   26975 |  618 | `		i++;` |
|   26975 |  619 | `		pLast = p;` |
|   26975 |  620 | `		p = p->pPrev; /* Reverse link */` |
|       5 |  621 | `	}` |
|    2197 |  622 | `}` |
|       - |  623 | `/*` |
|       - |  624 | ` * Array functions implementation.` |
|       - |  625 | ` * Status:` |
|       - |  626 | ` *  Stable.` |
|       - |  627 | ` */` |
|       - |  628 | `/*` |
|       - |  629 | ` * Reset / report the comparator-throw flag around a FLAG sort. The string sort` |
|       - |  630 | ` * flags coerce their operands user-visibly (HashmapScalarFlagCmp), and a` |
|       - |  631 | ` * not-stringable object raises php's Error there; the comparator can only flag` |
|       - |  632 | ` * it, so every flag-sort driver clears the flag before its merge sort and` |
|       - |  633 | ` * answers PH7_EXCEPTION after it. php's array is sorted after the throw too, so` |
|       - |  634 | ` * the rehash still runs.` |
|       - |  635 | ` */` |
|    2687 |  636 | `static sxi32 HashmapFlagSortStatus(ph7_context *pCtx)` |
|       5 |  637 | `{` |
|    2692 |  638 | `	if( pCtx->pVm->iCmpCallbackExc ){` |
|      80 |  639 | `		sxi32 rcExc = pCtx->pVm->iCmpCallbackExc;` |
|      80 |  640 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|      80 |  641 | `		pCtx->nThrowRc = rcExc;` |
|      80 |  642 | `		return rcExc;` |
|       - |  643 | `	}` |
|    2614 |  644 | `	return PH7_OK;` |
|    1348 |  645 | `}` |
|       - |  646 | `/*` |
|       - |  647 | ` * bool sort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - |  648 | ` * Sort an array.` |
|       - |  649 | ` * Parameters` |
|       - |  650 | ` *  $array` |
|       - |  651 | ` *   The input array.` |
|       - |  652 | ` * $sort_flags` |
|       - |  653 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - |  654 | ` *  Sorting type flags:` |
|       - |  655 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - |  656 | ` *   SORT_NUMERIC - compare items numerically` |
|       - |  657 | ` *   SORT_STRING - compare items as strings` |
|       - |  658 | ` * Return` |
|       - |  659 | ` *  TRUE on success or FALSE on failure.` |
|       - |  660 | ` *` |
|       - |  661 | ` */` |
|    2101 |  662 | `PH7_PRIVATE int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  663 | `{` |
|       - |  664 | `	ph7_hashmap *pMap;` |
|       - |  665 | `	/* Make sure we are dealing with a valid hashmap */` |
|    2106 |  666 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - |  667 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 |  668 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  669 | `		return PH7_OK;` |
|       - |  670 | `	}` |
|       - |  671 | `	/* Point to the internal representation of the input hashmap */` |
|    2106 |  672 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|    2106 |  673 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|    2106 |  674 | `	if( pMap->nEntry > 1 ){` |
|    1892 |  675 | `		sxi32 iCmpFlags = 0;` |
|    1892 |  676 | `		if( nArg > 1 ){` |
|       - |  677 | `			/* Extract comparison flags */` |
|    1706 |  678 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     850 |  679 | `		}` |
|       - |  680 | `		/* Do the merge sort */` |
|    1892 |  681 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|    1892 |  682 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|       - |  683 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|    1892 |  684 | `		HashmapSortRehash(pMap);` |
|    1162 |  685 | `	}else if( pMap->nEntry == 1 ){` |
|       - |  686 | `		/* php reindexes even a single-element array: a string key becomes 0 */` |
|     107 |  687 | `		HashmapSortRehash(pMap);` |
|      52 |  688 | `	}` |
|       - |  689 | `	{` |
|    2106 |  690 | `		sxi32 rcCmp = HashmapFlagSortStatus(pCtx);` |
|    2106 |  691 | `		if( rcCmp != PH7_OK ){` |
|      38 |  692 | `			return rcCmp;` |
|       - |  693 | `		}` |
|       - |  694 | `	}` |
|       - |  695 | `	/* All done,return TRUE */` |
|    2070 |  696 | `	ph7_result_bool(pCtx,1);` |
|    2070 |  697 | `	return PH7_OK;` |
|    1055 |  698 | `}` |
|       - |  699 | `/*` |
|       - |  700 | ` * bool asort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - |  701 | ` *  Sort an array and maintain index association.` |
|       - |  702 | ` * Parameters` |
|       - |  703 | ` *  $array` |
|       - |  704 | ` *   The input array.` |
|       - |  705 | ` * $sort_flags` |
|       - |  706 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - |  707 | ` *  Sorting type flags:` |
|       - |  708 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - |  709 | ` *   SORT_NUMERIC - compare items numerically` |
|       - |  710 | ` *   SORT_STRING - compare items as strings` |
|       - |  711 | ` * Return` |
|       - |  712 | ` *  TRUE on success or FALSE on failure.` |
|       - |  713 | ` */` |
|      62 |  714 | `PH7_PRIVATE int ph7_hashmap_asort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 |  715 | `{` |
|       - |  716 | `	char zGiven[64];` |
|       - |  717 | `	ph7_hashmap *pMap;` |
|       - |  718 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|      66 |  719 | `	if( nArg < 1 ){` |
|     ! 0 |  720 | `		return PH7_VmThrowException(pCtx,` |
|       - |  721 | `			"ArgumentCountError",` |
|       - |  722 | `			"asort() expects at least 1 argument, 0 given"` |
|       - |  723 | `			);` |
|       - |  724 | `	}` |
|       - |  725 | `	/* PHP 8: TypeError if first argument is not an array */` |
|      66 |  726 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 |  727 | `		return PH7_VmThrowException(pCtx,` |
|       - |  728 | `			"TypeError",` |
|       - |  729 | `			"asort(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 |  730 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - |  731 | `			);` |
|       - |  732 | `	}` |
|       - |  733 | `	/* Point to the internal representation of the input hashmap */` |
|      66 |  734 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      66 |  735 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      66 |  736 | `	if( pMap->nEntry > 1 ){` |
|      60 |  737 | `		sxi32 iCmpFlags = 0;` |
|      60 |  738 | `		if( nArg > 1 ){` |
|       - |  739 | `			/* Extract comparison flags */` |
|      29 |  740 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|      13 |  741 | `		}` |
|       - |  742 | `		/* Do the merge sort */` |
|      60 |  743 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|      60 |  744 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|       - |  745 | `		/* Fix the last link broken by the merge */` |
|     136 |  746 | `		while(pMap->pLast->pPrev){` |
|      80 |  747 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       4 |  748 | `		}` |
|      28 |  749 | `	}` |
|       - |  750 | `	{` |
|      66 |  751 | `		sxi32 rcCmp = HashmapFlagSortStatus(pCtx);` |
|      66 |  752 | `		if( rcCmp != PH7_OK ){` |
|      16 |  753 | `			return rcCmp;` |
|       - |  754 | `		}` |
|       - |  755 | `	}` |
|       - |  756 | `	/* All done,return TRUE */` |
|      52 |  757 | `	ph7_result_bool(pCtx,1);` |
|      52 |  758 | `	return PH7_OK;` |
|      35 |  759 | `}` |
|       - |  760 | `/*` |
|       - |  761 | ` * bool natsort(array &$array)` |
|       - |  762 | ` * bool natcasesort(array &$array)` |
|       - |  763 | ` *  Sort an array with php's "natural order" algorithm, maintaining index` |
|       - |  764 | ` *  association: exactly asort() under SORT_NATURAL (plus SORT_FLAG_CASE for the` |
|       - |  765 | ` *  case-insensitive twin), which is how php implements them too.` |
|       - |  766 | ` *` |
|       - |  767 | `` *  They used to be PRELUDE wrappers over `uasort($array, 'strnatcmp')`, and that`` |
|       - |  768 | ` *  is a different function: uasort hands each element to a userland callback, so` |
|       - |  769 | `` *  every element had to satisfy strnatcmp's `string` ZPP row. php's natsort`` |
|       - |  770 | ` *  COERCES each element the way any string comparison does, so` |
|       - |  771 | `` *  `natsort([10, "9", null])` — nothing exotic, just a mixed array — was a`` |
|       - |  772 | ` *  TypeError in PHL and a sorted array in php, and an object with no` |
|       - |  773 | ` *  __toString() answered strnatcmp's ZPP TypeError instead of php's coercion` |
|       - |  774 | ` *  Error. Routing through the flag comparator picks up HashmapFlagStringify,` |
|       - |  775 | ` *  which already renders arrays with php's "Array to string conversion" warning` |
|       - |  776 | ` *  and raises the coercion Error once per sort.` |
|       - |  777 | ` */` |
|      26 |  778 | `PH7_PRIVATE int ph7_hashmap_natsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  779 | `{` |
|       - |  780 | `	char zGiven[64];` |
|      29 |  781 | `	const char *zName = ph7_function_name(pCtx);` |
|       - |  782 | `	/* natcasesort() is the SORT_FLAG_CASE twin; match the whole name, not a byte. */` |
|      34 |  783 | `	int bFold = zName && SyStrlen(zName) == sizeof("natcasesort")-1` |
|      39 |  784 | `		&& SyMemcmp(zName,"natcasesort",sizeof("natcasesort")-1) == 0;` |
|       - |  785 | `	ph7_hashmap *pMap;` |
|      29 |  786 | `	if( nArg < 1 ){` |
|     ! 0 |  787 | `		return PH7_VmThrowException(pCtx,` |
|       - |  788 | `			"ArgumentCountError",` |
|     ! 0 |  789 | `			"%s() expects exactly 1 argument, 0 given",zName` |
|       - |  790 | `			);` |
|       - |  791 | `	}` |
|      29 |  792 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 |  793 | `		return PH7_VmThrowException(pCtx,` |
|       - |  794 | `			"TypeError",` |
|       - |  795 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 |  796 | `			zName,VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - |  797 | `			);` |
|       - |  798 | `	}` |
|      29 |  799 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      29 |  800 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      29 |  801 | `	if( pMap->nEntry > 1 ){` |
|       - |  802 | `		/* SORT_NATURAL (6), optionally \| SORT_FLAG_CASE (8) — the same iFlags` |
|       - |  803 | `		 * word asort() forwards, so this is asort($a, SORT_NATURAL) exactly. */` |
|      25 |  804 | `		sxi32 iCmpFlags = bFold ? (6\|8) : 6;` |
|      25 |  805 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|      25 |  806 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|      57 |  807 | `		while(pMap->pLast->pPrev){` |
|      35 |  808 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       3 |  809 | `		}` |
|      11 |  810 | `	}` |
|       - |  811 | `	{` |
|      29 |  812 | `		sxi32 rcCmp = HashmapFlagSortStatus(pCtx);` |
|      29 |  813 | `		if( rcCmp != PH7_OK ){` |
|       5 |  814 | `			return rcCmp;` |
|       - |  815 | `		}` |
|       - |  816 | `	}` |
|      25 |  817 | `	ph7_result_bool(pCtx,1);` |
|      25 |  818 | `	return PH7_OK;` |
|      16 |  819 | `}` |
|       - |  820 | `/*` |
|       - |  821 | ` * bool arsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - |  822 | ` *  Sort an array in reverse order and maintain index association.` |
|       - |  823 | ` * Parameters` |
|       - |  824 | ` *  $array` |
|       - |  825 | ` *   The input array.` |
|       - |  826 | ` * $sort_flags` |
|       - |  827 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - |  828 | ` *  Sorting type flags:` |
|       - |  829 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - |  830 | ` *   SORT_NUMERIC - compare items numerically` |
|       - |  831 | ` *   SORT_STRING - compare items as strings` |
|       - |  832 | ` * Return` |
|       - |  833 | ` *  TRUE on success or FALSE on failure.` |
|       - |  834 | ` */` |
|      28 |  835 | `PH7_PRIVATE int ph7_hashmap_arsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  836 | `{` |
|       - |  837 | `	char zGiven[64];` |
|       - |  838 | `	ph7_hashmap *pMap;` |
|       - |  839 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|      31 |  840 | `	if( nArg < 1 ){` |
|     ! 0 |  841 | `		return PH7_VmThrowException(pCtx,` |
|       - |  842 | `			"ArgumentCountError",` |
|       - |  843 | `			"arsort() expects at least 1 argument, 0 given"` |
|       - |  844 | `			);` |
|       - |  845 | `	}` |
|       - |  846 | `	/* PHP 8: TypeError if first argument is not an array */` |
|      31 |  847 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 |  848 | `		return PH7_VmThrowException(pCtx,` |
|       - |  849 | `			"TypeError",` |
|       - |  850 | `			"arsort(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 |  851 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - |  852 | `			);` |
|       - |  853 | `	}` |
|       - |  854 | `	/* Point to the internal representation of the input hashmap */` |
|      31 |  855 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      31 |  856 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      31 |  857 | `	if( pMap->nEntry > 1 ){` |
|      27 |  858 | `		sxi32 iCmpFlags = 0;` |
|      27 |  859 | `		if( nArg > 1 ){` |
|       - |  860 | `			/* Extract comparison flags */` |
|      13 |  861 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       5 |  862 | `		}` |
|       - |  863 | `		/* Do the merge sort */` |
|      27 |  864 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|      27 |  865 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|       - |  866 | `		/* Fix the last link broken by the merge */` |
|      45 |  867 | `		while(pMap->pLast->pPrev){` |
|      20 |  868 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       2 |  869 | `		}` |
|      12 |  870 | `	}` |
|       - |  871 | `	{` |
|      31 |  872 | `		sxi32 rcCmp = HashmapFlagSortStatus(pCtx);` |
|      31 |  873 | `		if( rcCmp != PH7_OK ){` |
|       3 |  874 | `			return rcCmp;` |
|       - |  875 | `		}` |
|       - |  876 | `	}` |
|       - |  877 | `	/* All done,return TRUE */` |
|      28 |  878 | `	ph7_result_bool(pCtx,1);` |
|      28 |  879 | `	return PH7_OK;` |
|      17 |  880 | `}` |
|       - |  881 | `/*` |
|       - |  882 | ` * bool ksort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - |  883 | ` *  Sort an array by key.` |
|       - |  884 | ` * Parameters` |
|       - |  885 | ` *  $array` |
|       - |  886 | ` *   The input array.` |
|       - |  887 | ` * $sort_flags` |
|       - |  888 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - |  889 | ` *  Sorting type flags:` |
|       - |  890 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - |  891 | ` *   SORT_NUMERIC - compare items numerically` |
|       - |  892 | ` *   SORT_STRING - compare items as strings` |
|       - |  893 | ` * Return` |
|       - |  894 | ` *  TRUE on success or FALSE on failure.` |
|       - |  895 | ` */` |
|     366 |  896 | `PH7_PRIVATE int ph7_hashmap_ksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 |  897 | `{` |
|       - |  898 | `	ph7_hashmap *pMap;` |
|       - |  899 | `	/* Make sure we are dealing with a valid hashmap */` |
|     370 |  900 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - |  901 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 |  902 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  903 | `		return PH7_OK;` |
|       - |  904 | `	}` |
|       - |  905 | `	/* Point to the internal representation of the input hashmap */` |
|     370 |  906 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|     370 |  907 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     370 |  908 | `	if( pMap->nEntry > 1 ){` |
|     130 |  909 | `		sxi32 iCmpFlags = 0;` |
|     130 |  910 | `		if( nArg > 1 ){` |
|       - |  911 | `			/* Extract comparison flags */` |
|      57 |  912 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|      27 |  913 | `		}` |
|       - |  914 | `		/* Do the merge sort */` |
|     130 |  915 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|     130 |  916 | `		HashmapMergeSort(pMap,HashmapCmpCallback2,SX_INT_TO_PTR(iCmpFlags));` |
|       - |  917 | `		/* Fix the last link broken by the merge */` |
|     248 |  918 | `		while(pMap->pLast->pPrev){` |
|     121 |  919 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       3 |  920 | `		}` |
|      63 |  921 | `	}` |
|       - |  922 | `	{` |
|     370 |  923 | `		sxi32 rcCmp = HashmapFlagSortStatus(pCtx);` |
|     370 |  924 | `		if( rcCmp != PH7_OK ){` |
|     ! 0 |  925 | `			return rcCmp;` |
|       - |  926 | `		}` |
|       - |  927 | `	}` |
|       - |  928 | `	/* All done,return TRUE */` |
|     370 |  929 | `	ph7_result_bool(pCtx,1);` |
|     370 |  930 | `	return PH7_OK;` |
|     187 |  931 | `}` |
|       - |  932 | `/*` |
|       - |  933 | ` * bool krsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - |  934 | ` *  Sort an array by key in reverse order.` |
|       - |  935 | ` * Parameters` |
|       - |  936 | ` *  $array` |
|       - |  937 | ` *   The input array.` |
|       - |  938 | ` * $sort_flags` |
|       - |  939 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - |  940 | ` *  Sorting type flags:` |
|       - |  941 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - |  942 | ` *   SORT_NUMERIC - compare items numerically` |
|       - |  943 | ` *   SORT_STRING - compare items as strings` |
|       - |  944 | ` * Return` |
|       - |  945 | ` *  TRUE on success or FALSE on failure.` |
|       - |  946 | ` */` |
|      30 |  947 | `PH7_PRIVATE int ph7_hashmap_krsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  948 | `{` |
|       - |  949 | `	ph7_hashmap *pMap;` |
|       - |  950 | `	/* Make sure we are dealing with a valid hashmap */` |
|      32 |  951 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - |  952 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 |  953 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  954 | `		return PH7_OK;` |
|       - |  955 | `	}` |
|       - |  956 | `	/* Point to the internal representation of the input hashmap */` |
|      32 |  957 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      32 |  958 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      32 |  959 | `	if( pMap->nEntry > 1 ){` |
|      32 |  960 | `		sxi32 iCmpFlags = 0;` |
|      32 |  961 | `		if( nArg > 1 ){` |
|       - |  962 | `			/* Extract comparison flags */` |
|       6 |  963 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       2 |  964 | `		}` |
|       - |  965 | `		/* Do the merge sort */` |
|      32 |  966 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|      32 |  967 | `		HashmapMergeSort(pMap,HashmapCmpCallback5,SX_INT_TO_PTR(iCmpFlags));` |
|       - |  968 | `		/* Fix the last link broken by the merge */` |
|      54 |  969 | `		while(pMap->pLast->pPrev){` |
|      23 |  970 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 |  971 | `		}` |
|      15 |  972 | `	}` |
|       - |  973 | `	{` |
|      32 |  974 | `		sxi32 rcCmp = HashmapFlagSortStatus(pCtx);` |
|      32 |  975 | `		if( rcCmp != PH7_OK ){` |
|     ! 0 |  976 | `			return rcCmp;` |
|       - |  977 | `		}` |
|       - |  978 | `	}` |
|       - |  979 | `	/* All done,return TRUE */` |
|      32 |  980 | `	ph7_result_bool(pCtx,1);` |
|      32 |  981 | `	return PH7_OK;` |
|      17 |  982 | `}` |
|       - |  983 | `/*` |
|       - |  984 | ` * bool rsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - |  985 | ` * Sort an array in reverse order.` |
|       - |  986 | ` * Parameters` |
|       - |  987 | ` *  $array` |
|       - |  988 | ` *   The input array.` |
|       - |  989 | ` * $sort_flags` |
|       - |  990 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - |  991 | ` *  Sorting type flags:` |
|       - |  992 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - |  993 | ` *   SORT_NUMERIC - compare items numerically` |
|       - |  994 | ` *   SORT_STRING - compare items as strings` |
|       - |  995 | ` * Return` |
|       - |  996 | ` *  TRUE on success or FALSE on failure.` |
|       - |  997 | ` */` |
|      40 |  998 | `PH7_PRIVATE int ph7_hashmap_rsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  999 | `{` |
|       - | 1000 | `	ph7_hashmap *pMap;` |
|       - | 1001 | `	/* Make sure we are dealing with a valid hashmap */` |
|      45 | 1002 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 1003 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 1004 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1005 | `		return PH7_OK;` |
|       - | 1006 | `	}` |
|       - | 1007 | `	/* Point to the internal representation of the input hashmap */` |
|      45 | 1008 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      45 | 1009 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      45 | 1010 | `	if( pMap->nEntry > 1 ){` |
|      43 | 1011 | `		sxi32 iCmpFlags = 0;` |
|      43 | 1012 | `		if( nArg > 1 ){` |
|       - | 1013 | `			/* Extract comparison flags */` |
|      28 | 1014 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|      12 | 1015 | `		}` |
|       - | 1016 | `		/* Do the merge sort */` |
|      43 | 1017 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|      43 | 1018 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 1019 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|      43 | 1020 | `		HashmapSortRehash(pMap);` |
|      22 | 1021 | `	}else if( pMap->nEntry == 1 ){` |
|       - | 1022 | `		/* php reindexes even a single-element array: a string key becomes 0 */` |
|       3 | 1023 | `		HashmapSortRehash(pMap);` |
|       1 | 1024 | `	}` |
|       - | 1025 | `	{` |
|      45 | 1026 | `		sxi32 rcCmp = HashmapFlagSortStatus(pCtx);` |
|      45 | 1027 | `		if( rcCmp != PH7_OK ){` |
|      14 | 1028 | `			return rcCmp;` |
|       - | 1029 | `		}` |
|       - | 1030 | `	}` |
|       - | 1031 | `	/* All done,return TRUE */` |
|      32 | 1032 | `	ph7_result_bool(pCtx,1);` |
|      32 | 1033 | `	return PH7_OK;` |
|      25 | 1034 | `}` |
|       - | 1035 | `/*` |
|       - | 1036 | ` * bool usort(array &$array,callable $cmp_function)` |
|       - | 1037 | ` *  Sort an array by values using a user-defined comparison function.` |
|       - | 1038 | ` * Parameters` |
|       - | 1039 | ` *  $array` |
|       - | 1040 | ` *   The input array.` |
|       - | 1041 | ` * $cmp_function` |
|       - | 1042 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 1043 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 1044 | ` *  to, or greater than the second.` |
|       - | 1045 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 1046 | ` * Return` |
|       - | 1047 | ` *  TRUE on success or FALSE on failure.` |
|       - | 1048 | ` */` |
|     155 | 1049 | `PH7_PRIVATE int ph7_hashmap_usort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1050 | `{` |
|       - | 1051 | `	ph7_hashmap *pMap;` |
|       - | 1052 | `	/* Make sure we are dealing with a valid hashmap */` |
|     160 | 1053 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 1054 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 1055 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1056 | `		return PH7_OK;` |
|       - | 1057 | `	}` |
|     160 | 1058 | `	if( nArg > 1 ){` |
|       - | 1059 | `		/* php rejects an uncallable comparator with a TypeError, BEFORE touching the` |
|       - | 1060 | `		 * array. PH7 handed it to the dispatcher, which answers NULL in silence -- so` |
|       - | 1061 | `		 * usort() with a bogus comparator reordered the array anyway, by nothing. */` |
|     160 | 1062 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",0);` |
|     160 | 1063 | `		if( rcCb != PH7_OK ){` |
|       7 | 1064 | `			return rcCb;` |
|       - | 1065 | `		}` |
|      71 | 1066 | `	}` |
|       - | 1067 | `	/* Point to the internal representation of the input hashmap */` |
|     154 | 1068 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|     154 | 1069 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     154 | 1070 | `	if( pMap->nEntry > 1 ){` |
|     152 | 1071 | `		ph7_value *pCallback = 0;` |
|       - | 1072 | `		ProcNodeCmp xCmp;` |
|     152 | 1073 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|     152 | 1074 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 1075 | `			/* Point to the desired callback */` |
|     152 | 1076 | `			pCallback = apArg[1];` |
|      75 | 1077 | `		}else{` |
|       - | 1078 | `			/* Use the default comparison function */` |
|     ! 0 | 1079 | `			xCmp = HashmapCmpCallback1;` |
|       - | 1080 | `		}` |
|       - | 1081 | `		/* Do the merge sort */` |
|     152 | 1082 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|     152 | 1083 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 1084 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|     152 | 1085 | `		HashmapSortRehash(pMap);` |
|     152 | 1086 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 1087 | `			/* The comparison callback did not return: propagate its status so the` |
|       - | 1088 | `			 * dispatcher unwinds. */` |
|      17 | 1089 | `			sxi32 rcExc = pCtx->pVm->iCmpCallbackExc;` |
|      17 | 1090 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|      17 | 1091 | `			return rcExc;` |
|       5 | 1092 | `		}` |
|      66 | 1093 | `	}else if( pMap->nEntry == 1 ){` |
|       - | 1094 | `		/* php reindexes even a single-element array: a string key becomes 0 */` |
|       3 | 1095 | `		HashmapSortRehash(pMap);` |
|       1 | 1096 | `	}` |
|       - | 1097 | `	/* All done,return TRUE */` |
|     140 | 1098 | `	ph7_result_bool(pCtx,1);` |
|     140 | 1099 | `	return PH7_OK;` |
|      79 | 1100 | `}` |
|       - | 1101 | `/*` |
|       - | 1102 | ` * bool uasort(array &$array,callable $cmp_function)` |
|       - | 1103 | ` *  Sort an array by values using a user-defined comparison function` |
|       - | 1104 | ` *  and maintain index association.` |
|       - | 1105 | ` * Parameters` |
|       - | 1106 | ` *  $array` |
|       - | 1107 | ` *   The input array.` |
|       - | 1108 | ` * $cmp_function` |
|       - | 1109 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 1110 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 1111 | ` *  to, or greater than the second.` |
|       - | 1112 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 1113 | ` * Return` |
|       - | 1114 | ` *  TRUE on success or FALSE on failure.` |
|       - | 1115 | ` */` |
|      18 | 1116 | `PH7_PRIVATE int ph7_hashmap_uasort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 1117 | `{` |
|       - | 1118 | `	ph7_hashmap *pMap;` |
|       - | 1119 | `	/* Make sure we are dealing with a valid hashmap */` |
|      21 | 1120 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 1121 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 1122 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1123 | `		return PH7_OK;` |
|       - | 1124 | `	}` |
|      21 | 1125 | `	if( nArg > 1 ){` |
|       - | 1126 | `		/* php rejects an uncallable comparator with a TypeError, BEFORE touching the` |
|       - | 1127 | `		 * array. PH7 handed it to the dispatcher, which answers NULL in silence -- so` |
|       - | 1128 | `		 * usort() with a bogus comparator reordered the array anyway, by nothing. */` |
|      21 | 1129 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",0);` |
|      21 | 1130 | `		if( rcCb != PH7_OK ){` |
|       5 | 1131 | `			return rcCb;` |
|       - | 1132 | `		}` |
|       7 | 1133 | `	}` |
|       - | 1134 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 1135 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      17 | 1136 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      17 | 1137 | `	if( pMap->nEntry > 1 ){` |
|      17 | 1138 | `		ph7_value *pCallback = 0;` |
|       - | 1139 | `		ProcNodeCmp xCmp;` |
|      17 | 1140 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|      17 | 1141 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 1142 | `			/* Point to the desired callback */` |
|      17 | 1143 | `			pCallback = apArg[1];` |
|      10 | 1144 | `		}else{` |
|       - | 1145 | `			/* Use the default comparison function */` |
|     ! 0 | 1146 | `			xCmp = HashmapCmpCallback1;` |
|       - | 1147 | `		}` |
|       - | 1148 | `		/* Do the merge sort */` |
|      17 | 1149 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|      17 | 1150 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 1151 | `		/* Fix the last link broken by the merge */` |
|      29 | 1152 | `		while(pMap->pLast->pPrev){` |
|      13 | 1153 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 1154 | `		}` |
|      17 | 1155 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 1156 | `			/* The comparison callback did not return: propagate its status so the` |
|       - | 1157 | `			 * dispatcher unwinds. */` |
|       3 | 1158 | `			sxi32 rcExc = pCtx->pVm->iCmpCallbackExc;` |
|       3 | 1159 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 1160 | `			return rcExc;` |
|       - | 1161 | `		}` |
|       6 | 1162 | `	}` |
|       - | 1163 | `	/* All done,return TRUE */` |
|      14 | 1164 | `	ph7_result_bool(pCtx,1);` |
|      14 | 1165 | `	return PH7_OK;` |
|      12 | 1166 | `}` |
|       - | 1167 | `/*` |
|       - | 1168 | ` * bool uksort(array &$array,callable $cmp_function)` |
|       - | 1169 | ` *  Sort an array by keys using a user-defined comparison` |
|       - | 1170 | ` *  function and maintain index association.` |
|       - | 1171 | ` * Parameters` |
|       - | 1172 | ` *  $array` |
|       - | 1173 | ` *   The input array.` |
|       - | 1174 | ` * $cmp_function` |
|       - | 1175 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 1176 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 1177 | ` *  to, or greater than the second.` |
|       - | 1178 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 1179 | ` * Return` |
|       - | 1180 | ` *  TRUE on success or FALSE on failure.` |
|       - | 1181 | ` */` |
|      12 | 1182 | `PH7_PRIVATE int ph7_hashmap_uksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 1183 | `{` |
|       - | 1184 | `	ph7_hashmap *pMap;` |
|       - | 1185 | `	/* Make sure we are dealing with a valid hashmap */` |
|      15 | 1186 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 1187 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 1188 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1189 | `		return PH7_OK;` |
|       - | 1190 | `	}` |
|      15 | 1191 | `	if( nArg > 1 ){` |
|       - | 1192 | `		/* php rejects an uncallable comparator with a TypeError, BEFORE touching the` |
|       - | 1193 | `		 * array. PH7 handed it to the dispatcher, which answers NULL in silence -- so` |
|       - | 1194 | `		 * usort() with a bogus comparator reordered the array anyway, by nothing. */` |
|      15 | 1195 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",0);` |
|      15 | 1196 | `		if( rcCb != PH7_OK ){` |
|       3 | 1197 | `			return rcCb;` |
|       - | 1198 | `		}` |
|       5 | 1199 | `	}` |
|       - | 1200 | `	/* Point to the internal representation of the input hashmap */` |
|      13 | 1201 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      13 | 1202 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      13 | 1203 | `	if( pMap->nEntry > 1 ){` |
|      13 | 1204 | `		ph7_value *pCallback = 0;` |
|       - | 1205 | `		ProcNodeCmp xCmp;` |
|      13 | 1206 | `		xCmp = HashmapCmpCallback6; /* User-defined function as the comparison callback */` |
|      13 | 1207 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 1208 | `			/* Point to the desired callback */` |
|      13 | 1209 | `			pCallback = apArg[1];` |
|       8 | 1210 | `		}else{` |
|       - | 1211 | `			/* Use the default comparison function */` |
|     ! 0 | 1212 | `			xCmp = HashmapCmpCallback2;` |
|       - | 1213 | `		}` |
|       - | 1214 | `		/* Do the merge sort */` |
|      13 | 1215 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|      13 | 1216 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 1217 | `		/* Fix the last link broken by the merge */` |
|      15 | 1218 | `		while(pMap->pLast->pPrev){` |
|       3 | 1219 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 1220 | `		}` |
|      13 | 1221 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 1222 | `			/* The comparison callback did not return: propagate its status so the` |
|       - | 1223 | `			 * dispatcher unwinds. */` |
|       3 | 1224 | `			sxi32 rcExc = pCtx->pVm->iCmpCallbackExc;` |
|       3 | 1225 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 1226 | `			return rcExc;` |
|       - | 1227 | `		}` |
|       4 | 1228 | `	}` |
|       - | 1229 | `	/* All done,return TRUE */` |
|      10 | 1230 | `	ph7_result_bool(pCtx,1);` |
|      10 | 1231 | `	return PH7_OK;` |
|       9 | 1232 | `}` |
|       - | 1233 | `/*` |
|       - | 1234 | ` * bool array_multisort(array &$array, mixed $array1_sort_order = SORT_ASC,` |
|       - | 1235 | ` *                      mixed $array1_sort_flags = SORT_REGULAR, mixed &...$rest)` |
|       - | 1236 | ` *  Sort multiple arrays at once: the argument list is a little language read` |
|       - | 1237 | ` *  left to right — an ARRAY opens a column, and each column may be followed by` |
|       - | 1238 | ` *  at most one sort ORDER (SORT_ASC/SORT_DESC) and at most one sort FLAGS` |
|       - | 1239 | ` *  value, in either order. Rows are compared column by column, a tie in one` |
|       - | 1240 | ` *  column falling through to the next; the resulting permutation is applied to` |
|       - | 1241 | ` *  EVERY column. String keys are kept, numeric keys are renumbered, and the` |
|       - | 1242 | ` *  sort is stable (php 8's own guarantee). php's error shapes, pinned by` |
|       - | 1243 | `` *  probe: a non-int non-array is `Argument #N must be an array or a sort`` |
|       - | 1244 | `` *  flag`; a misplaced or repeated order/flags is the same text with`` |
|       - | 1245 | `` *  `... that has not already been specified`; an int that is no flag at all is`` |
|       - | 1246 | `` *  the ValueError `must be a valid sort flag`; mismatched lengths are`` |
|       - | 1247 | `` *  `Array sizes are inconsistent` with no function prefix; and only`` |
|       - | 1248 | `` *  Argument #1 carries its `($array)` name. php declares the whole list`` |
|       - | 1249 | ` *  prefer-ref (VmBuiltinPrefersRef), so a literal sorts a temporary silently.` |
|       - | 1250 | ` */` |
|       - | 1251 | `/* One column of the multisort: the caller's array plus its sort spec. */` |
|       - | 1252 | `typedef struct MultisortCol MultisortCol;` |
|       - | 1253 | `struct MultisortCol` |
|       - | 1254 | `{` |
|       - | 1255 | `	ph7_value *pArr;        /* The caller's argument slot */` |
|       - | 1256 | `	ph7_hashmap *pMap;      /* Its hashmap */` |
|       - | 1257 | `	ph7_hashmap_node **apNode; /* Nodes in ORIGINAL iteration order */` |
|       - | 1258 | `	sxi32 iFlags;           /* SORT_* comparison flags */` |
|       - | 1259 | `	int iDir;               /* +1 SORT_ASC, -1 SORT_DESC */` |
|       - | 1260 | `	int bOrderSeen;         /* An order argument already attached */` |
|       - | 1261 | `	int bFlagsSeen;         /* A flags argument already attached */` |
|       - | 1262 | `};` |
|       - | 1263 | `/* Compare two ROWS, column by column with each column's own direction/flags. */` |
|      96 | 1264 | `static sxi32 MultisortRowCmp(MultisortCol *aCol,sxu32 nCol,sxu32 iA,sxu32 iB)` |
|       3 | 1265 | `{` |
|      99 | 1266 | `	ph7_vm *pVm = aCol[0].pMap->pVm;` |
|       - | 1267 | `	sxu32 c;` |
|     109 | 1268 | `	for( c = 0 ; c < nCol ; c++ ){` |
|     109 | 1269 | `		sxi32 rc = HashmapFlagValueCmp(aCol[c].apNode[iA],aCol[c].apNode[iB],aCol[c].iFlags);` |
|     109 | 1270 | `		HashmapCmpLatch(pVm);` |
|     109 | 1271 | `		if( rc != 0 ){` |
|      99 | 1272 | `			return aCol[c].iDir < 0 ? -rc : rc;` |
|       - | 1273 | `		}` |
|       6 | 1274 | `	}` |
|     ! 0 | 1275 | `	return 0;` |
|      51 | 1276 | `}` |
|       - | 1277 | `/*` |
|       - | 1278 | ` * Stable bottom-up merge sort over the row-index permutation. Iterative on` |
|       - | 1279 | ` * purpose — the recursive shape would put O(log n) frames on the native stack` |
|       - | 1280 | ` * (the §7 embedder C-stack family).` |
|       - | 1281 | ` */` |
|      34 | 1282 | `static void MultisortSortIdx(MultisortCol *aCol,sxu32 nCol,sxu32 *aIdx,sxu32 *aTmp,sxu32 n)` |
|       3 | 1283 | `{` |
|       - | 1284 | `	sxu32 nWidth,iLo;` |
|     101 | 1285 | `	for( nWidth = 1 ; nWidth < n ; nWidth *= 2 ){` |
|     161 | 1286 | `		for( iLo = 0 ; iLo < n ; iLo += 2 * nWidth ){` |
|      97 | 1287 | `			sxu32 iMid = iLo + nWidth;` |
|      97 | 1288 | `			sxu32 iHi = iLo + 2 * nWidth;` |
|       - | 1289 | `			sxu32 i,j,k;` |
|      97 | 1290 | `			if( iMid > n ){ iMid = n; }` |
|      97 | 1291 | `			if( iHi > n ){ iHi = n; }` |
|      97 | 1292 | `			i = iLo; j = iMid; k = iLo;` |
|     193 | 1293 | `			while( i < iMid && j < iHi ){` |
|       - | 1294 | `				/* <= keeps the run stable: on a full tie the left row wins */` |
|      99 | 1295 | `				if( MultisortRowCmp(aCol,nCol,aIdx[i],aIdx[j]) <= 0 ){` |
|      43 | 1296 | `					aTmp[k++] = aIdx[i++];` |
|      23 | 1297 | `				}else{` |
|      59 | 1298 | `					aTmp[k++] = aIdx[j++];` |
|       - | 1299 | `				}` |
|       3 | 1300 | `			}` |
|     181 | 1301 | `			while( i < iMid ){ aTmp[k++] = aIdx[i++]; }` |
|     113 | 1302 | `			while( j < iHi ){ aTmp[k++] = aIdx[j++]; }` |
|      50 | 1303 | `		}` |
|       - | 1304 | `		/* aTmp holds the merged runs for this width; swap roles by copying` |
|       - | 1305 | `		 * back — n is bounded by the array count, one memcpy per doubling. */` |
|      67 | 1306 | `		SyMemcpy(aTmp,aIdx,(sxu32)(n * sizeof(sxu32)));` |
|      35 | 1307 | `	}` |
|      37 | 1308 | `}` |
|      60 | 1309 | `PH7_PRIVATE int ph7_hashmap_multisort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 1310 | `{` |
|       - | 1311 | `	MultisortCol *aCol;` |
|       - | 1312 | `	sxu32 *aIdx,*aTmp;` |
|      63 | 1313 | `	sxu32 nCol = 0,nRow,i;` |
|       - | 1314 | `	sxi32 rcStatus;` |
|       - | 1315 | `	int iArg;` |
|       - | 1316 |  |
|      63 | 1317 | `	if( nArg < 1 ){` |
|     ! 0 | 1318 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1319 | `			"ArgumentCountError",` |
|       - | 1320 | `			"array_multisort() expects at least 1 argument, %d given",` |
|     ! 0 | 1321 | `			nArg` |
|       - | 1322 | `			);` |
|       - | 1323 | `	}` |
|      93 | 1324 | `	aCol = (MultisortCol *)ph7_context_alloc_chunk(pCtx,` |
|      60 | 1325 | `		(unsigned int)(sizeof(MultisortCol) * (sxu32)nArg),TRUE,TRUE);` |
|      63 | 1326 | `	if( aCol == 0 ){` |
|     ! 0 | 1327 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 1328 | `	}` |
|       - | 1329 | `	/* Read the argument list's little language, php's own state machine. */` |
|     161 | 1330 | `	for( iArg = 0 ; iArg < nArg ; iArg++ ){` |
|     123 | 1331 | `		ph7_value *pArg = apArg[iArg];` |
|       - | 1332 | `		/* Only Argument #1 carries its parameter name in php's messages. */` |
|     123 | 1333 | `		const char *zName = (iArg == 0) ? " ($array)" : "";` |
|     123 | 1334 | `		if( ph7_value_is_array(pArg) ){` |
|      79 | 1335 | `			MultisortCol *pCol = &aCol[nCol++];` |
|      79 | 1336 | `			pCol->pArr = pArg;` |
|      79 | 1337 | `			pCol->pMap = (ph7_hashmap *)pArg->x.pOther;` |
|      79 | 1338 | `			pCol->apNode = 0;` |
|      79 | 1339 | `			pCol->iFlags = 0; /* SORT_REGULAR */` |
|      79 | 1340 | `			pCol->iDir = 1;   /* SORT_ASC */` |
|      79 | 1341 | `			pCol->bOrderSeen = pCol->bFlagsSeen = 0;` |
|      79 | 1342 | `			continue;` |
|       - | 1343 | `		}` |
|      47 | 1344 | `		if( ph7_value_is_float(pArg) \|\| (pArg->iFlags & MEMOBJ_INT) == 0 ){` |
|       - | 1345 | `			/* A REAL int only, float asked FIRST (ph7_type_name's rule: an` |
|       - | 1346 | `			 * integer-valued real caches an int and would pass a bare flag` |
|       - | 1347 | `			 * test). php coerces nothing here: "4", 4.0 and true are all` |
|       - | 1348 | `			 * refused. */` |
|      17 | 1349 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1350 | `				"array_multisort(): Argument #%d%s must be an array or a sort flag",` |
|       5 | 1351 | `				iArg + 1,zName);` |
|       - | 1352 | `		}` |
|       - | 1353 | `		{` |
|      37 | 1354 | `			sxi64 iVal = ph7_value_to_int64(pArg);` |
|       - | 1355 | `			/* php masks SORT_FLAG_CASE off for the ORDER match too, but reads` |
|       - | 1356 | `			 * the direction from the UNMASKED value — so SORT_DESC\|SORT_FLAG_CASE` |
|       - | 1357 | `			 * (11) consumes the order slot and quirkily sorts ASCENDING. */` |
|      37 | 1358 | `			if( (iVal & ~(sxi64)8) == 3 /* SORT_DESC */ \|\| (iVal & ~(sxi64)8) == 4 /* SORT_ASC */ ){` |
|      23 | 1359 | `				if( nCol < 1 \|\| aCol[nCol - 1].bOrderSeen ){` |
|      11 | 1360 | `					return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1361 | `						"array_multisort(): Argument #%d%s must be an array or a sort flag that has not already been specified",` |
|       3 | 1362 | `						iArg + 1,zName);` |
|       - | 1363 | `				}` |
|      17 | 1364 | `				aCol[nCol - 1].iDir = (iVal == 3) ? -1 : 1;` |
|      17 | 1365 | `				aCol[nCol - 1].bOrderSeen = 1;` |
|      24 | 1366 | `			}else if( (iVal & ~(sxi64)8 /* SORT_FLAG_CASE */) == 0 /* SORT_REGULAR */` |
|      13 | 1367 | `			       \|\| (iVal & ~(sxi64)8) == 1 /* SORT_NUMERIC */` |
|      11 | 1368 | `			       \|\| (iVal & ~(sxi64)8) == 2 /* SORT_STRING */` |
|       8 | 1369 | `			       \|\| (iVal & ~(sxi64)8) == 5 /* SORT_LOCALE_STRING */` |
|       7 | 1370 | `			       \|\| (iVal & ~(sxi64)8) == 6 /* SORT_NATURAL */ ){` |
|      14 | 1371 | `				if( nCol < 1 \|\| aCol[nCol - 1].bFlagsSeen ){` |
|       7 | 1372 | `					return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1373 | `						"array_multisort(): Argument #%d%s must be an array or a sort flag that has not already been specified",` |
|       2 | 1374 | `						iArg + 1,zName);` |
|       - | 1375 | `				}` |
|      10 | 1376 | `				aCol[nCol - 1].iFlags = (sxi32)iVal;` |
|      10 | 1377 | `				aCol[nCol - 1].bFlagsSeen = 1;` |
|       6 | 1378 | `			}else{` |
|       4 | 1379 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1380 | `					"array_multisort(): Argument #%d%s must be a valid sort flag",` |
|       1 | 1381 | `					iArg + 1,zName);` |
|       - | 1382 | `			}` |
|       - | 1383 | `		}` |
|      14 | 1384 | `	}` |
|       - | 1385 | `	/* Every column must hold the same number of rows; php's message carries no` |
|       - | 1386 | `	 * function prefix. */` |
|      41 | 1387 | `	nRow = aCol[0].pMap->nEntry;` |
|      57 | 1388 | `	for( i = 1 ; i < nCol ; i++ ){` |
|      20 | 1389 | `		if( aCol[i].pMap->nEntry != nRow ){` |
|       3 | 1390 | `			return PH7_VmThrowException(pCtx,"ValueError","Array sizes are inconsistent");` |
|       - | 1391 | `		}` |
|      10 | 1392 | `	}` |
|      39 | 1393 | `	if( nRow > 0 ){` |
|       - | 1394 | `		/* Collect each column's nodes in original order. */` |
|      85 | 1395 | `		for( i = 0 ; i < nCol ; i++ ){` |
|      51 | 1396 | `			ph7_hashmap_node *pNode = aCol[i].pMap->pFirst;` |
|       - | 1397 | `			sxu32 r;` |
|      75 | 1398 | `			aCol[i].apNode = (ph7_hashmap_node **)ph7_context_alloc_chunk(pCtx,` |
|      24 | 1399 | `				(unsigned int)(sizeof(ph7_hashmap_node *) * nRow),FALSE,TRUE);` |
|      51 | 1400 | `			if( aCol[i].apNode == 0 ){` |
|     ! 0 | 1401 | `				return PH7_ContextMemoryError(pCtx);` |
|       - | 1402 | `			}` |
|     195 | 1403 | `			for( r = 0 ; r < nRow && pNode ; r++ ){` |
|     147 | 1404 | `				aCol[i].apNode[r] = pNode;` |
|     147 | 1405 | `				pNode = pNode->pPrev; /* Reverse link */` |
|      75 | 1406 | `			}` |
|      27 | 1407 | `		}` |
|      54 | 1408 | `		aIdx = (sxu32 *)ph7_context_alloc_chunk(pCtx,` |
|      17 | 1409 | `			(unsigned int)(sizeof(sxu32) * nRow * 2),FALSE,TRUE);` |
|      37 | 1410 | `		if( aIdx == 0 ){` |
|     ! 0 | 1411 | `			return PH7_ContextMemoryError(pCtx);` |
|       - | 1412 | `		}` |
|      37 | 1413 | `		aTmp = &aIdx[nRow];` |
|     139 | 1414 | `		for( i = 0 ; i < nRow ; i++ ){` |
|     105 | 1415 | `			aIdx[i] = i;` |
|      54 | 1416 | `		}` |
|       - | 1417 | `		/* The string flags coerce user-visibly and can only FLAG a throw` |
|       - | 1418 | `		 * (iCmpCallbackExc); clear it, sort, and report after — the flag-sort` |
|       - | 1419 | `		 * drivers' shared pattern. */` |
|      37 | 1420 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|      37 | 1421 | `		MultisortSortIdx(aCol,nCol,aIdx,aTmp,nRow);` |
|      37 | 1422 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 1423 | `			/* A comparison raised. php's array_multisort leaves EVERY column as` |
|       - | 1424 | `			 * it found it in that case -- unlike sort(), which still writes back` |
|       - | 1425 | `			 * the order it reached -- so the permutation is dropped rather than` |
|       - | 1426 | `			 * applied. */` |
|      11 | 1427 | `			return HashmapFlagSortStatus(pCtx);` |
|       - | 1428 | `		}` |
|       - | 1429 | `		/* Apply the permutation to every column: rebuild in sorted order,` |
|       - | 1430 | `		 * keeping string keys and renumbering int keys, then hand the fresh` |
|       - | 1431 | `		 * array back through the by-ref slot (a literal has none and the` |
|       - | 1432 | `		 * result is silently dropped — php's prefer-ref). */` |
|      63 | 1433 | `		for( i = 0 ; i < nCol ; i++ ){` |
|      39 | 1434 | `			ph7_value *pNew = ph7_context_new_array(pCtx);` |
|       - | 1435 | `			sxu32 r;` |
|      39 | 1436 | `			if( pNew == 0 ){` |
|     ! 0 | 1437 | `				return PH7_ContextMemoryError(pCtx);` |
|       - | 1438 | `			}` |
|     147 | 1439 | `			for( r = 0 ; r < nRow ; r++ ){` |
|     111 | 1440 | `				ph7_hashmap_node *pNode = aCol[i].apNode[aIdx[r]];` |
|     165 | 1441 | `				HashmapInsertNode((ph7_hashmap *)pNew->x.pOther,pNode,` |
|     108 | 1442 | `					pNode->iType == HASHMAP_BLOB_NODE ? TRUE : FALSE);` |
|      57 | 1443 | `			}` |
|      39 | 1444 | `			PH7_VmStoreArgByRef(pCtx->pVm,aCol[i].pArr,pNew);` |
|      21 | 1445 | `		}` |
|      27 | 1446 | `		rcStatus = HashmapFlagSortStatus(pCtx);` |
|      27 | 1447 | `		if( rcStatus != PH7_OK ){` |
|     ! 0 | 1448 | `			return rcStatus;` |
|       - | 1449 | `		}` |
|      12 | 1450 | `	}` |
|      29 | 1451 | `	ph7_result_bool(pCtx,1);` |
|      29 | 1452 | `	return PH7_OK;` |
|      33 | 1453 | `}` |
|       - | 1454 |  |
