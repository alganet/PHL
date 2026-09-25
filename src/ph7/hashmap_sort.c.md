# src/ph7/hashmap_sort.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 672/724 lines (92.82%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|      - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    5 | ` */` |
|      - |    6 | `#include "ph7int.h"` |
|      - |    7 | `/*` |
|      - |    8 | ` * Section:` |
|      - |    9 | ` *    Hashmap (array) sorting: the SQLite-derived merge sort, its comparator` |
|      - |   10 | ` *    set and the sort()/usort()/ksort() builtin family.` |
|      - |   11 | ` * Status:` |
|      - |   12 | ` *    Stable.` |
|      - |   13 | ` */` |
|      - |   14 | `/* SPDX-SnippetBegin */` |
|      - |   15 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|      - |   16 | `/* SPDX-License-Identifier: blessing */` |
|      - |   17 | `/*` |
|      - |   18 | ` * Merge sort.` |
|      - |   19 | ` * The merge sort implementation is based on the one found in the SQLite3 source tree.` |
|      - |   20 | ` * Status: Public domain` |
|      - |   21 | ` */` |
|      - |   22 | `/* Node comparison callback signature */` |
|      - |   23 | `/*` |
|      - |   24 | `** Inputs:` |
|      - |   25 | `**   a:       A sorted, null-terminated linked list.  (May be null).` |
|      - |   26 | `**   b:       A sorted, null-terminated linked list.  (May be null).` |
|      - |   27 | `**   cmp:     A pointer to the comparison function.` |
|      - |   28 | `**` |
|      - |   29 | `** Return Value:` |
|      - |   30 | `**   A pointer to the head of a sorted list containing the elements` |
|      - |   31 | `**   of both a and b.` |
|      - |   32 | `**` |
|      - |   33 | `** Side effects:` |
|      - |   34 | `**   The "next","prev" pointers for elements in the lists a and b are` |
|      - |   35 | `**   changed.` |
|      - |   36 | `*/` |
|  75266 |   37 | `static ph7_hashmap_node * HashmapNodeMerge(ph7_hashmap_node *pA,ph7_hashmap_node *pB,ProcNodeCmp xCmp,void *pCmpData)` |
|      5 |   38 | `{` |
|      - |   39 | `	ph7_hashmap_node result,*pTail;` |
|      - |   40 | `    /* Prevent compiler warning */` |
|  75271 |   41 | `	result.pNext = result.pPrev = 0;` |
|  75271 |   42 | `	pTail = &result;` |
| 174204 |   43 | `	while( pA && pB ){` |
|  98938 |   44 | `		if( xCmp(pA,pB,pCmpData) <= 0 ){` |
|  77513 |   45 | `			pTail->pPrev = pA;` |
|  77513 |   46 | `			pA->pNext = pTail;` |
|  77513 |   47 | `			pTail = pA;` |
|  77513 |   48 | `			pA = pA->pPrev;` |
|  38884 |   49 | `		}else{` |
|  21430 |   50 | `			pTail->pPrev = pB;` |
|  21430 |   51 | `			pB->pNext = pTail;` |
|  21430 |   52 | `			pTail = pB;` |
|  21430 |   53 | `			pB = pB->pPrev;` |
|      - |   54 | `		}` |
|      5 |   55 | `	}` |
|  75271 |   56 | `	if( pA ){` |
|   5766 |   57 | `		pTail->pPrev = pA;` |
|   5766 |   58 | `		pA->pNext = pTail;` |
|  72288 |   59 | `	}else if( pB ){` |
|  69071 |   60 | `		pTail->pPrev = pB;` |
|  69071 |   61 | `		pB->pNext = pTail;` |
|  34624 |   62 | `	}else{` |
|    444 |   63 | `		pTail->pPrev = pTail->pNext = 0;` |
|      - |   64 | `	}` |
|  75271 |   65 | `	return result.pPrev;` |
|      5 |   66 | `}` |
|      - |   67 | `/*` |
|      - |   68 | `** Inputs:` |
|      - |   69 | `**   Map:       Input hashmap` |
|      - |   70 | `**   cmp:       A comparison function.` |
|      - |   71 | `**` |
|      - |   72 | `** Return Value:` |
|      - |   73 | `**   Sorted hashmap.` |
|      - |   74 | `**` |
|      - |   75 | `** Side effects:` |
|      - |   76 | `**   The "next" pointers for elements in list are changed.` |
|      - |   77 | `*/` |
|      - |   78 | `#define N_SORT_BUCKET  32` |
|   1837 |   79 | `PH7_PRIVATE sxi32 HashmapMergeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData)` |
|      5 |   80 | `{` |
|      - |   81 | `	ph7_hashmap_node *a[N_SORT_BUCKET], *p,*pIn;` |
|      - |   82 | `	sxu32 i;` |
|   1842 |   83 | `	SyZero(a,sizeof(a));` |
|      - |   84 | `	/* Point to the first inserted entry */` |
|   1842 |   85 | `	pIn = pMap->pFirst;` |
|  23508 |   86 | `	while( pIn ){` |
|  21671 |   87 | `		p = pIn;` |
|  21671 |   88 | `		pIn = p->pPrev;` |
|  21671 |   89 | `		p->pPrev = 0;` |
|  39990 |   90 | `		for(i=0; i<N_SORT_BUCKET-1; i++){` |
|  39990 |   91 | `			if( a[i]==0 ){` |
|  21671 |   92 | `				a[i] = p;` |
|  21671 |   93 | `				break;` |
|    ! 0 |   94 | `			}else{` |
|  18324 |   95 | `				p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|  18324 |   96 | `				a[i] = 0;` |
|      - |   97 | `			}` |
|   9163 |   98 | `		}` |
|  21671 |   99 | `		if( i==N_SORT_BUCKET-1 ){` |
|      - |  100 | `			/* To get here, there need to be 2^(N_SORT_BUCKET) elements in he input list.` |
|      - |  101 | `			 * But that is impossible.` |
|      - |  102 | `			 */` |
|    ! 0 |  103 | `			a[i] = HashmapNodeMerge(a[i], p,xCmp,pCmpData);` |
|    ! 0 |  104 | `		}` |
|      5 |  105 | `	}` |
|   1842 |  106 | `	p = a[0];` |
|  58789 |  107 | `	for(i=1; i<N_SORT_BUCKET; i++){` |
|      - |  108 | `		/* Higher-index buckets hold EARLIER-inserted (and larger) runs, so the` |
|      - |  109 | `		 * bucket must be the LEFT operand: HashmapNodeMerge favors its left arg on` |
|      - |  110 | `		 * a tie (cmp <= 0), and php's sorts are stable (PHP 8.0+) — equal elements` |
|      - |  111 | `		 * keep their original order. Passing p (the later elements) on the left` |
|      - |  112 | `		 * reversed equal runs (e.g. usort of five tie-keyed items moved the last to` |
|      - |  113 | `		 * the front). */` |
|  56952 |  114 | `		p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|  28463 |  115 | `	}` |
|   1842 |  116 | `	p->pNext = 0;` |
|      - |  117 | `	/* Reflect the change */` |
|   1842 |  118 | `	pMap->pFirst = p;` |
|      - |  119 | `	/* Reset the loop cursor */` |
|   1842 |  120 | `	pMap->pCur = pMap->pFirst;` |
|   1842 |  121 | `	return SXRET_OK;` |
|      5 |  122 | `}` |
|      - |  123 | `/* SPDX-SnippetEnd */` |
|      - |  124 | `/*` |
|      - |  125 | ` * Coerce one operand of a STRING-flag comparison (SORT_STRING and friends), the` |
|      - |  126 | ` * way php's zval_get_string() does. An ARRAY warns "Array to string conversion"` |
|      - |  127 | ` * and renders as "Array"; an object with no __toString() raises php's catchable` |
|      - |  128 | ` * "could not be converted to string" Error and renders as the EMPTY string --` |
|      - |  129 | ` * which is why php's array comes out fully SORTED after the throw, with the` |
|      - |  130 | ` * object first. PHL used to render it as the literal "Object" and sort on that,` |
|      - |  131 | ` * silently.` |
|      - |  132 | ` *` |
|      - |  133 | ` * A comparator has no status channel, so the Error is raised once per sort and` |
|      - |  134 | ` * flagged on the VM through iCmpCallbackExc -- the rail a throwing user callback` |
|      - |  135 | ` * already uses; every flag-sort driver clears the flag before its merge sort and` |
|      - |  136 | ` * answers PH7_EXCEPTION after it (HashmapFlagSortStatus), and array_unique() does` |
|      - |  137 | ` * the same around its walk. The flag is also what keeps the second and later` |
|      - |  138 | ` * comparisons from raising the same Error again.` |
|      - |  139 | ` */` |
|    418 |  140 | `static void HashmapFlagStringify(ph7_value *pVal)` |
|      5 |  141 | `{` |
|    423 |  142 | `	ph7_vm *pVm = pVal->pVm;` |
|    423 |  143 | `	sxi32 rc = PH7_EXCEPTION;` |
|    418 |  144 | `	if( (pVal->iFlags & MEMOBJ_OBJ) && pVm` |
|     87 |  145 | `	 && (pVm->iCmpCallbackExc != 0 \|\| PH7_CALLBACK_UNWOUND(pVm->nBoundaryRc)) ){` |
|      - |  146 | `		/* This sort already coerced an object once and it did not succeed: php` |
|      - |  147 | `		 * raises the refusal Error once, and it enters no PHP function at all` |
|      - |  148 | `		 * while an exception is pending (zend_call_function bails on` |
|      - |  149 | `		 * EG(exception)), so neither arm runs again -- the operand just renders` |
|      - |  150 | `		 * as the empty string a refused cast gives. The raise-once guard used to` |
|      - |  151 | `		 * cover only the NOT-STRINGABLE arm, so a throwing __toString() body went` |
|      - |  152 | `		 * round again on the next pair; uncaught, it reported the fatal once per` |
|      - |  153 | `		 * comparison, and after a catch had run in place the second throw was` |
|      - |  154 | `		 * uncaught and killed a script php merely says "caught" in.` |
|      - |  155 | `		 * OBJECTS only: blanking an int or a string here would re-order the rest` |
|      - |  156 | `		 * of the array. */` |
|     26 |  157 | `		PH7_MemObjRelease(pVal);` |
|     26 |  158 | `		MemObjSetType(pVal,MEMOBJ_STRING);` |
|     26 |  159 | `		return;` |
|      - |  160 | `	}` |
|    399 |  161 | `	if( !PH7_MemObjIsNotStringable(pVal) ){` |
|    361 |  162 | `		rc = PH7_MemObjToStringUV(pVal);` |
|    361 |  163 | `		if( rc == SXRET_OK ){` |
|    353 |  164 | `			return;` |
|      - |  165 | `		}` |
|      - |  166 | `		/* A __toString() that THREW. Same shape as a refused cast from here on. */` |
|      5 |  167 | `	}else{` |
|     40 |  168 | `		rc = PH7_MemObjToStringUV(pVal); /* raises php's Error */` |
|      - |  169 | `	}` |
|     48 |  170 | `	if( pVm ){` |
|      - |  171 | `		/* Latch the STATUS the coercion answered, so an UNCAUGHT throw (or an` |
|      - |  172 | `		 * exit()) out of __toString() leaves the sort with PH7_ABORT rather than` |
|      - |  173 | `		 * a downgraded PH7_EXCEPTION. */` |
|     48 |  174 | `		pVm->iCmpCallbackExc = PH7_CALLBACK_UNWOUND(rc) ? rc : PH7_EXCEPTION;` |
|     23 |  175 | `	}` |
|     48 |  176 | `	PH7_MemObjRelease(pVal);` |
|     48 |  177 | `	MemObjSetType(pVal,MEMOBJ_STRING);` |
|    214 |  178 | `}` |
|      - |  179 | `/*` |
|      - |  180 | ` * Node comparison callback.` |
|      - |  181 | ` * used-by: [sort(),asort(),...]` |
|      - |  182 | ` */` |
|      - |  183 | `/*` |
|      - |  184 | ` * Compare two scalar values under an EXPLICIT php sort base type (never 0 —` |
|      - |  185 | ` * SORT_REGULAR is handled by the callers, which differ for keys vs values):` |
|      - |  186 | ` *   SORT_NUMERIC 1 · SORT_STRING 2 · SORT_LOCALE_STRING 5 · SORT_NATURAL 6,` |
|      - |  187 | ` * with bFold applying SORT_FLAG_CASE. PHL has no locale tables, so` |
|      - |  188 | ` * SORT_LOCALE_STRING behaves like SORT_STRING. Mutates both operands (numeric or` |
|      - |  189 | ` * string cast); the caller owns and releases them.` |
|      - |  190 | ` */` |
| 520120 |  191 | `static sxi32 HashmapScalarFlagCmp(ph7_value *pA,ph7_value *pB,int base,int bFold)` |
|      5 |  192 | `{` |
|      - |  193 | `	sxi32 rc;` |
| 520125 |  194 | `	if( base == 1 ){` |
|      - |  195 | `		/* SORT_NUMERIC compares as DOUBLES. php's numeric_compare_function is` |
|      - |  196 | ``		 * `zval_get_double(a)` vs `zval_get_double(b)` under ZEND_THREEWAY_COMPARE`` |
|      - |  197 | `		 * — not a value comparison over whatever type each operand happens to` |
|      - |  198 | `		 * settle on. Two consequences PHL got wrong by folding to a NUMBER and` |
|      - |  199 | `		 * calling the ordinary comparator: the diagnostic named the wrong type` |
|      - |  200 | `		 * (an object warned "could not be converted to int" where php says` |
|      - |  201 | `		 * "to float"), and integers above 2^53 were ordered EXACTLY where php's` |
|      - |  202 | ``		 * doubles tie — `sort([PHP_INT_MAX, PHP_INT_MAX-1], SORT_NUMERIC)` left`` |
|      - |  203 | `		 * php's stable sort's original order and PHL re-ordered them. Matching php` |
|      - |  204 | `		 * means adopting its precision loss, which is what parity is (§10).` |
|      - |  205 | `		 * The == / < shape is ZEND_THREEWAY_COMPARE's, so NaN — equal to nothing,` |
|      - |  206 | `		 * less than nothing — answers 1 in both engines. */` |
|      - |  207 | `		ph7_real rA,rB;` |
|    230 |  208 | `		PH7_MemObjToReal(pA);` |
|    230 |  209 | `		PH7_MemObjToReal(pB);` |
|    230 |  210 | `		rA = pA->rVal;` |
|    230 |  211 | `		rB = pB->rVal;` |
|    230 |  212 | `		rc = (rA == rB) ? 0 : ((rA < rB) ? -1 : 1);` |
|    116 |  213 | `	}else{` |
|      - |  214 | `		/* SORT_STRING (2) / SORT_LOCALE_STRING (5) / SORT_NATURAL (6) */` |
|      - |  215 | `		const char *zA,*zB;` |
|      - |  216 | `		sxu32 nA,nB,nMin,i;` |
| 519897 |  217 | `		if( (pA->iFlags & MEMOBJ_STRING) == 0 ){ HashmapFlagStringify(pA); }` |
| 519897 |  218 | `		if( (pB->iFlags & MEMOBJ_STRING) == 0 ){ HashmapFlagStringify(pB); }` |
| 519897 |  219 | `		zA = (const char *)SyBlobData(&pA->sBlob);` |
| 519897 |  220 | `		zB = (const char *)SyBlobData(&pB->sBlob);` |
| 519897 |  221 | `		nA = SyBlobLength(&pA->sBlob);` |
| 519897 |  222 | `		nB = SyBlobLength(&pB->sBlob);` |
| 519897 |  223 | `		if( base == 6 ){` |
|    189 |  224 | `			rc = PH7_StrNatCmp(zA,(int)nA,zB,(int)nB,bFold);` |
|     96 |  225 | `		}else{` |
|      - |  226 | `			/* Lexicographic comparison (binary-safe), case-folded on request. */` |
| 519711 |  227 | `			nMin = nA < nB ? nA : nB;` |
| 519711 |  228 | `			rc = 0;` |
| 633189 |  229 | `			for( i = 0 ; i < nMin ; ++i ){` |
| 632769 |  230 | `				int ca = (unsigned char)zA[i];` |
| 632769 |  231 | `				int cb = (unsigned char)zB[i];` |
| 632769 |  232 | `				if( bFold ){ ca = SyToLower(ca); cb = SyToLower(cb); }` |
| 632769 |  233 | `				if( ca != cb ){ rc = ca < cb ? -1 : 1; break; }` |
|  56744 |  234 | `			}` |
| 519711 |  235 | `			if( rc == 0 ){` |
|    425 |  236 | `				if( nA < nB ) rc = -1;` |
|    271 |  237 | `				else if( nA > nB ) rc = 1;` |
|    210 |  238 | `			}` |
|      - |  239 | `		}` |
|      - |  240 | `	}` |
| 520125 |  241 | `	return rc;` |
|      5 |  242 | `}` |
|      - |  243 | `/*` |
|      - |  244 | ` * Are two live values equal under an array_unique() sort_flags? A non-mutating` |
|      - |  245 | ` * wrapper (works on private copies): base 0 = SORT_REGULAR loose comparison,` |
|      - |  246 | ` * explicit flags route through HashmapScalarFlagCmp. Used by array_unique.` |
|      - |  247 | ` */` |
| 519494 |  248 | `PH7_PRIVATE int HashmapValueFlagEqual(ph7_vm *pVm,ph7_value *pA,ph7_value *pB,int base,int bFold)` |
|      5 |  249 | `{` |
|      - |  250 | `	ph7_value sA,sB;` |
|      - |  251 | `	sxi32 rc;` |
| 519499 |  252 | `	PH7_MemObjInit(pVm,&sA);` |
| 519499 |  253 | `	PH7_MemObjInit(pVm,&sB);` |
| 519499 |  254 | `	PH7_MemObjStore(pA,&sA);` |
| 519499 |  255 | `	PH7_MemObjStore(pB,&sB);` |
| 519499 |  256 | `	if( base == 0 ){` |
|     14 |  257 | `		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0); /* SORT_REGULAR: loose comparison */` |
|      8 |  258 | `	}else{` |
| 519487 |  259 | `		rc = HashmapScalarFlagCmp(&sA,&sB,base,bFold);` |
|      - |  260 | `	}` |
| 519499 |  261 | `	PH7_MemObjRelease(&sA);` |
| 519499 |  262 | `	PH7_MemObjRelease(&sB);` |
| 519499 |  263 | `	return rc == 0;` |
|      5 |  264 | `}` |
|      - |  265 | `/*` |
|      - |  266 | ` * Compare two node VALUES under php's sort_flags (base 0 = SORT_REGULAR uses the` |
|      - |  267 | ` * standard value comparison; explicit flags route through HashmapScalarFlagCmp).` |
|      - |  268 | ` */` |
|    600 |  269 | `static sxi32 HashmapFlagValueCmp(ph7_hashmap_node *pA,ph7_hashmap_node *pB,sxi32 iFlags)` |
|      5 |  270 | `{` |
|      - |  271 | `	ph7_value sA,sB;` |
|    605 |  272 | `	int base = iFlags & ~8;      /* strip SORT_FLAG_CASE */` |
|    605 |  273 | `	int bFold = (iFlags & 8) != 0;` |
|      - |  274 | `	sxi32 rc;` |
|    605 |  275 | `	if( base == 0 ){` |
|      - |  276 | `		/* SORT_REGULAR */` |
|     94 |  277 | `		return HashmapNodeCmp(pA,pB,FALSE);` |
|      - |  278 | `	}` |
|    515 |  279 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|    515 |  280 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|    515 |  281 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|    515 |  282 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|    515 |  283 | `	rc = HashmapScalarFlagCmp(&sA,&sB,base,bFold);` |
|    515 |  284 | `	PH7_MemObjRelease(&sA);` |
|    515 |  285 | `	PH7_MemObjRelease(&sB);` |
|    515 |  286 | `	return rc;` |
|    305 |  287 | `}` |
|      - |  288 | `/*` |
|      - |  289 | ` * A sort comparison can run USER code with no way to report what it did: an` |
|      - |  290 | `` * object operand's `__toString()` is reached by SORT_REGULAR's value comparison`` |
|      - |  291 | ` * as well as by the string flags' coercion, and that body can throw or exit().` |
|      - |  292 | ` * PH7_MemObjCmp only ever answers an ORDERING, so the status is read off the VM` |
|      - |  293 | ` * instead — every C->PHP dispatch parks an unwind in nBoundaryRc (VmBoundaryPark)` |
|      - |  294 | ` * and nothing inside a builtin consumes it, so it is still there when the` |
|      - |  295 | ` * comparison returns.` |
|      - |  296 | ` *` |
|      - |  297 | ` * Two things follow, and both were missing: the merge sort must STAND DOWN (a` |
|      - |  298 | `` * throwing `__toString()` ran again on the next pair -- and since the enclosing`` |
|      - |  299 | ` * catch had already run in place, the second throw was UNCAUGHT and killed a` |
|      - |  300 | ` * script php merely reports "caught" in), and the driver must answer with that` |
|      - |  301 | `` * status rather than `true`.`` |
|      - |  302 | ` *` |
|      - |  303 | ` * Deliberately NOT triggered by a not-stringable object's own coercion Error:` |
|      - |  304 | ` * that one parks nothing and is flagged by HashmapFlagStringify, and php goes on` |
|      - |  305 | ` * comparing after it (its array comes out fully sorted, the object first).` |
|      - |  306 | ` */` |
|  98493 |  307 | `static void HashmapCmpLatch(ph7_vm *pVm)` |
|      5 |  308 | `{` |
|  98493 |  309 | `	if( PH7_CALLBACK_UNWOUND(pVm->nBoundaryRc)` |
|  49222 |  310 | `	 && (pVm->iCmpCallbackExc == 0 \|\| pVm->nBoundaryRc == PH7_ABORT) ){` |
|    123 |  311 | `		pVm->iCmpCallbackExc = pVm->nBoundaryRc;` |
|     75 |  312 | `	}` |
|  98470 |  313 | `}` |
|  97852 |  314 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|      5 |  315 | `{` |
|  97857 |  316 | `	ph7_vm *pVm = pA->pMap->pVm;` |
|      - |  317 | `	sxi32 rc;` |
|  97857 |  318 | `	if( pCmpData == 0 ){` |
|      - |  319 | `		/* SORT_REGULAR fast path */` |
|  97441 |  320 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|  48611 |  321 | `	}else{` |
|    420 |  322 | `		rc = HashmapFlagValueCmp(pA,pB,SX_PTR_TO_INT(pCmpData));` |
|      - |  323 | `	}` |
|  97857 |  324 | `	HashmapCmpLatch(pVm);` |
|  97857 |  325 | `	return rc;` |
|      5 |  326 | `}` |
|      - |  327 | `/*` |
|      - |  328 | ` * Materialise a node's KEY as a scalar ph7_value (int key -> integer, string key` |
|      - |  329 | ` * -> string) for a flag-aware key comparison.` |
|      - |  330 | ` */` |
|    680 |  331 | `static void HashmapNodeKeyToValue(ph7_hashmap_node *pNode,ph7_value *pOut)` |
|      3 |  332 | `{` |
|    683 |  333 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|    108 |  334 | `		PH7_MemObjInitFromInt(pNode->pMap->pVm,pOut,pNode->xKey.iKey);` |
|     55 |  335 | `	}else{` |
|    577 |  336 | `		PH7_MemObjInitFromString(pNode->pMap->pVm,pOut,0);` |
|    864 |  337 | `		PH7_MemObjStringAppend(pOut,(const char *)SyBlobData(&pNode->xKey.sKey),` |
|    287 |  338 | `			SyBlobLength(&pNode->xKey.sKey));` |
|      - |  339 | `	}` |
|    683 |  340 | `}` |
|      - |  341 | `/*` |
|      - |  342 | ` * Shared key comparison for ksort()/krsort() under SORT_REGULAR: php compares` |
|      - |  343 | ` * two array KEYS exactly the way it compares two VALUES, so materialise them` |
|      - |  344 | `` * and hand them to the standard comparison — the same one sort()/`<=>` use.`` |
|      - |  345 | ` *` |
|      - |  346 | ` * The hand-rolled version this replaces got the mixed int/string case right but` |
|      - |  347 | ` * compared two STRING keys BYTEWISE, so two numeric strings sorted by their` |
|      - |  348 | ` * bytes: ksort(['10.0'=>1,'9.0'=>2]) answered ['10.0','9.0'] where php answers` |
|      - |  349 | ` * ['9.0','10.0'], ksort(['1e3'=>1,'20'=>2]) put 1e3 (1000) first, and keys that` |
|      - |  350 | ` * compare EQUAL ('1.0', '01', 1) lost php's stable order.` |
|      - |  351 | ` */` |
|    234 |  352 | `static sxi32 HashmapKeyNodeCmp(ph7_hashmap_node *pA,ph7_hashmap_node *pB)` |
|      2 |  353 | `{` |
|      - |  354 | `	ph7_value sA,sB;` |
|      - |  355 | `	sxi32 rc;` |
|    236 |  356 | `	if( pA->iType == HASHMAP_INT_NODE && pB->iType == HASHMAP_INT_NODE ){` |
|      - |  357 | `		/* Two integer keys: the common case, and no allocation needed */` |
|     23 |  358 | `		return pA->xKey.iKey < pB->xKey.iKey ? -1 : (pA->xKey.iKey > pB->xKey.iKey ? 1 : 0);` |
|      - |  359 | `	}` |
|    214 |  360 | `	HashmapNodeKeyToValue(pA,&sA);` |
|    214 |  361 | `	HashmapNodeKeyToValue(pB,&sB);` |
|    214 |  362 | `	rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|    214 |  363 | `	PH7_MemObjRelease(&sA);` |
|    214 |  364 | `	PH7_MemObjRelease(&sB);` |
|    214 |  365 | `	return rc;` |
|    119 |  366 | `}` |
|      - |  367 | `/*` |
|      - |  368 | ` * Compare two node KEYS under php's sort_flags. base 0 = SORT_REGULAR keeps the` |
|      - |  369 | ` * php-8 mixed int/string key semantics (HashmapKeyNodeCmp); explicit flags route` |
|      - |  370 | ` * the materialised keys through HashmapScalarFlagCmp.` |
|      - |  371 | ` */` |
|    128 |  372 | `static sxi32 HashmapFlagKeyCmp(ph7_hashmap_node *pA,ph7_hashmap_node *pB,sxi32 iFlags)` |
|      3 |  373 | `{` |
|      - |  374 | `	ph7_value sA,sB;` |
|    131 |  375 | `	int base = iFlags & ~8;      /* strip SORT_FLAG_CASE */` |
|    131 |  376 | `	int bFold = (iFlags & 8) != 0;` |
|      - |  377 | `	sxi32 rc;` |
|    131 |  378 | `	if( base == 0 ){` |
|    ! 0 |  379 | `		return HashmapKeyNodeCmp(pA,pB);` |
|      - |  380 | `	}` |
|    131 |  381 | `	HashmapNodeKeyToValue(pA,&sA);` |
|    131 |  382 | `	HashmapNodeKeyToValue(pB,&sB);` |
|    131 |  383 | `	rc = HashmapScalarFlagCmp(&sA,&sB,base,bFold);` |
|    131 |  384 | `	PH7_MemObjRelease(&sA);` |
|    131 |  385 | `	PH7_MemObjRelease(&sB);` |
|    131 |  386 | `	return rc;` |
|     67 |  387 | `}` |
|      - |  388 | `/*` |
|      - |  389 | ` * Node comparison callback: Compare nodes by keys only.` |
|      - |  390 | ` * used-by: [ksort()]` |
|      - |  391 | ` */` |
|    296 |  392 | `static sxi32 HashmapCmpCallback2(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|      3 |  393 | `{` |
|    299 |  394 | `	ph7_vm *pVm = pA->pMap->pVm;` |
|      - |  395 | `	sxi32 rc;` |
|    299 |  396 | `	if( pCmpData == 0 ){` |
|    176 |  397 | `		rc = HashmapKeyNodeCmp(pA,pB);` |
|     89 |  398 | `	}else{` |
|    125 |  399 | `		rc = HashmapFlagKeyCmp(pA,pB,SX_PTR_TO_INT(pCmpData));` |
|      - |  400 | `	}` |
|    299 |  401 | `	HashmapCmpLatch(pVm);` |
|    299 |  402 | `	return rc;` |
|      3 |  403 | `}` |
|      - |  404 | `/*` |
|      - |  405 | ` * Node comparison callback.` |
|      - |  406 | ` * Used by: [rsort(),arsort()];` |
|      - |  407 | ` */` |
|    239 |  408 | `static sxi32 HashmapCmpCallback3(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|      4 |  409 | `{` |
|    243 |  410 | `	ph7_vm *pVm = pA->pMap->pVm;` |
|      - |  411 | `	sxi32 rc;` |
|    243 |  412 | `	if( pCmpData == 0 ){` |
|      - |  413 | `		/* SORT_REGULAR fast path, reversed */` |
|    164 |  414 | `		rc = -HashmapNodeCmp(pA,pB,FALSE);` |
|     86 |  415 | `	}else{` |
|     82 |  416 | `		rc = -HashmapFlagValueCmp(pA,pB,SX_PTR_TO_INT(pCmpData));` |
|      - |  417 | `	}` |
|    243 |  418 | `	HashmapCmpLatch(pVm);` |
|    243 |  419 | `	return rc;` |
|      4 |  420 | `}` |
|      - |  421 | `/*` |
|      - |  422 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|      - |  423 | ` * used-by: [usort(),uasort()]` |
|      - |  424 | ` */` |
|    462 |  425 | `static sxi32 HashmapCmpCallback4(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|      5 |  426 | `{` |
|      - |  427 | `	ph7_value sResult,*pCallback;` |
|      - |  428 | `	ph7_value *pV1,*pV2;` |
|      - |  429 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|      - |  430 | `	sxi32 rc;` |
|      - |  431 | `	/* Point to the desired callback */` |
|    467 |  432 | `	pCallback = (ph7_value *)pCmpData;` |
|    467 |  433 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|      - |  434 | `		/* A previous comparison already raised: stop invoking the callback so` |
|      - |  435 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|     27 |  436 | `		return 0;` |
|      - |  437 | `	}` |
|      - |  438 | `	/* initialize the result value */` |
|    443 |  439 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|      - |  440 | `	/* Extract nodes values */` |
|    443 |  441 | `	pV1 = HashmapExtractNodeValue(pA);` |
|    443 |  442 | `	pV2 = HashmapExtractNodeValue(pB);` |
|    443 |  443 | `	apArg[0] = pV1;` |
|    443 |  444 | `	apArg[1] = pV2;` |
|      - |  445 | `	/* Invoke the callback */` |
|    443 |  446 | `	rc = PH7_VmCallCallbackByValue(pA->pMap->pVm,pCallback,2,apArg,&sResult,0);` |
|    443 |  447 | `	if( PH7_CALLBACK_UNWOUND(rc) ){` |
|      - |  448 | `		/* The comparator did not RETURN: latch the STATUS so the sort driver` |
|      - |  449 | `		 * aborts and propagates exactly it (an UNCAUGHT throw is PH7_ABORT, and` |
|      - |  450 | `		 * testing only PH7_EXCEPTION left the sort running -- re-entering the` |
|      - |  451 | `		 * comparator, and the fatal report, for every remaining pair), and order` |
|      - |  452 | `		 * this pair arbitrarily for the rest of the run. */` |
|     15 |  453 | `		pA->pMap->pVm->iCmpCallbackExc = rc;` |
|     15 |  454 | `		rc = 0;` |
|    437 |  455 | `	}else if( rc != SXRET_OK ){` |
|      - |  456 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|    ! 0 |  457 | `		rc = -1; /* Set a dummy result */` |
|    ! 0 |  458 | `	}else{` |
|      - |  459 | `		/* Extract callback result */` |
|    431 |  460 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|      - |  461 | `			/* Perform an int cast */` |
|    ! 0 |  462 | `			PH7_MemObjToInteger(&sResult);` |
|    ! 0 |  463 | `		}` |
|    431 |  464 | `		rc = (sxi32)sResult.x.iVal;` |
|      - |  465 | `	}` |
|    443 |  466 | `	PH7_MemObjRelease(&sResult);` |
|      - |  467 | `	/* Callback result */` |
|    443 |  468 | `	return rc;` |
|    236 |  469 | `}` |
|      - |  470 | `/*` |
|      - |  471 | ` * Node comparison callback: Compare nodes by keys only.` |
|      - |  472 | ` * used-by: [krsort()]` |
|      - |  473 | ` */` |
|     66 |  474 | `static sxi32 HashmapCmpCallback5(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|      2 |  475 | `{` |
|     68 |  476 | `	if( pCmpData == 0 ){` |
|     61 |  477 | `		return -HashmapKeyNodeCmp(pA,pB); /* Reverse result */` |
|      - |  478 | `	}` |
|      8 |  479 | `	return -HashmapFlagKeyCmp(pA,pB,SX_PTR_TO_INT(pCmpData));` |
|     35 |  480 | `}` |
|      - |  481 | `/*` |
|      - |  482 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|      - |  483 | ` * used-by: [uksort()]` |
|      - |  484 | ` */` |
|     18 |  485 | `static sxi32 HashmapCmpCallback6(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|      2 |  486 | `{` |
|      - |  487 | `	ph7_value sResult,*pCallback;` |
|      - |  488 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|      - |  489 | `	ph7_value sK1,sK2;` |
|      - |  490 | `	sxi32 rc;` |
|      - |  491 | `	/* Point to the desired callback */` |
|     20 |  492 | `	pCallback = (ph7_value *)pCmpData;` |
|     20 |  493 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|      - |  494 | `		/* A previous comparison already raised: stop invoking the callback so` |
|      - |  495 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|      7 |  496 | `		return 0;` |
|      - |  497 | `	}` |
|      - |  498 | `	/* initialize the result value */` |
|     14 |  499 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|     14 |  500 | `	PH7_MemObjInit(pA->pMap->pVm,&sK1);` |
|     14 |  501 | `	PH7_MemObjInit(pA->pMap->pVm,&sK2);` |
|      - |  502 | `	/* Extract nodes keys */` |
|     14 |  503 | `	PH7_HashmapExtractNodeKey(pA,&sK1);` |
|     14 |  504 | `	PH7_HashmapExtractNodeKey(pB,&sK2);` |
|     14 |  505 | `	apArg[0] = &sK1;` |
|     14 |  506 | `	apArg[1] = &sK2;` |
|      - |  507 | `	/* Mark keys as constants */` |
|     14 |  508 | `	sK1.nIdx = SXU32_HIGH;` |
|     14 |  509 | `	sK2.nIdx = SXU32_HIGH;` |
|      - |  510 | `	/* Invoke the callback */` |
|     14 |  511 | `	rc = PH7_VmCallCallbackByValue(pA->pMap->pVm,pCallback,2,apArg,&sResult,0);` |
|     14 |  512 | `	if( PH7_CALLBACK_UNWOUND(rc) ){` |
|      - |  513 | `		/* The comparator did not RETURN: latch the STATUS so the sort driver` |
|      - |  514 | `		 * aborts and propagates exactly it (an UNCAUGHT throw is PH7_ABORT, and` |
|      - |  515 | `		 * testing only PH7_EXCEPTION left the sort running -- re-entering the` |
|      - |  516 | `		 * comparator, and the fatal report, for every remaining pair), and order` |
|      - |  517 | `		 * this pair arbitrarily for the rest of the run. */` |
|      3 |  518 | `		pA->pMap->pVm->iCmpCallbackExc = rc;` |
|      3 |  519 | `		rc = 0;` |
|     12 |  520 | `	}else if( rc != SXRET_OK ){` |
|      - |  521 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|    ! 0 |  522 | `		rc = -1; /* Set a dummy result */` |
|    ! 0 |  523 | `	}else{` |
|      - |  524 | `		/* Extract callback result */` |
|     11 |  525 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|      - |  526 | `			/* Perform an int cast */` |
|    ! 0 |  527 | `			PH7_MemObjToInteger(&sResult);` |
|    ! 0 |  528 | `		}` |
|     11 |  529 | `		rc = (sxi32)sResult.x.iVal;` |
|      - |  530 | `	}` |
|     14 |  531 | `	PH7_MemObjRelease(&sResult);` |
|     14 |  532 | `	PH7_MemObjRelease(&sK1);` |
|     14 |  533 | `	PH7_MemObjRelease(&sK2);` |
|      - |  534 | `	/* Callback result */` |
|     14 |  535 | `	return rc;` |
|     11 |  536 | `}` |
|      - |  537 | `/*` |
|      - |  538 | ` * Permute a hashmap's entries the way php's shuffle() does: Fisher-Yates over` |
|      - |  539 | ` * the buckets, drawing each index from the MT19937 generator through` |
|      - |  540 | ` * php_mt_rand_range(). The caller rehashes afterwards (php reindexes the array` |
|      - |  541 | ` * 0..n-1 and drops string keys).` |
|      - |  542 | ` *` |
|      - |  543 | ` * This used to be a merge sort with a coin-flip comparator, which is a permuting` |
|      - |  544 | ` * shuffle but not a UNIFORM one — the distribution a random comparator produces` |
|      - |  545 | ` * is skewed and depends on the sort's internals — and it consumed the generator` |
|      - |  546 | ` * in a different order, so a seeded run answered a different permutation from` |
|      - |  547 | ` * php's for every seed.` |
|      - |  548 | ` */` |
|      8 |  549 | `PH7_PRIVATE sxi32 PH7_HashmapShuffle(ph7_hashmap *pMap)` |
|      2 |  550 | `{` |
|      - |  551 | `	ph7_hashmap_node **apNode,*pNode;` |
|      - |  552 | `	sxu32 n,nLeft;` |
|      - |  553 | `	SySet aNode;` |
|     10 |  554 | `	if( pMap->nEntry < 2 ){` |
|    ! 0 |  555 | `		return SXRET_OK;` |
|      - |  556 | `	}` |
|     10 |  557 | `	SySetInit(&aNode,&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|     50 |  558 | `	for( pNode = pMap->pFirst ; pNode ; pNode = pNode->pPrev ){` |
|     42 |  559 | `		if( SySetPut(&aNode,(const void *)&pNode) != SXRET_OK ){` |
|    ! 0 |  560 | `			SySetRelease(&aNode);` |
|    ! 0 |  561 | `			return SXERR_MEM;` |
|      - |  562 | `		}` |
|     22 |  563 | `	}` |
|     10 |  564 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&aNode);` |
|     10 |  565 | `	n = SySetUsed(&aNode);` |
|      - |  566 | `	/* php walks DOWN from the last index, swapping with a draw in [0,n_left]. */` |
|     42 |  567 | `	for( nLeft = n - 1 ; nLeft > 0 ; --nLeft ){` |
|     34 |  568 | `		sxu32 nPick = (sxu32)PH7_VmMtRandRange(pMap->pVm,0,(sxi64)nLeft);` |
|     34 |  569 | `		if( nPick != nLeft ){` |
|     24 |  570 | `			ph7_hashmap_node *pTmp = apNode[nLeft];` |
|     24 |  571 | `			apNode[nLeft] = apNode[nPick];` |
|     24 |  572 | `			apNode[nPick] = pTmp;` |
|     11 |  573 | `		}` |
|     18 |  574 | `	}` |
|      - |  575 | `	/* Relink in the new order. pPrev is the forward link and pNext the back one` |
|      - |  576 | `	 * (the whole map is built that way); the rehash after this fixes pLast. */` |
|     50 |  577 | `	for( n = 0 ; n < SySetUsed(&aNode) ; ++n ){` |
|     42 |  578 | `		apNode[n]->pPrev = (n + 1 < SySetUsed(&aNode)) ? apNode[n+1] : 0;` |
|     42 |  579 | `		apNode[n]->pNext = (n > 0) ? apNode[n-1] : 0;` |
|     22 |  580 | `	}` |
|     10 |  581 | `	pMap->pFirst = apNode[0];` |
|     10 |  582 | `	pMap->pLast = apNode[SySetUsed(&aNode) - 1];` |
|     10 |  583 | `	pMap->pCur = pMap->pFirst;` |
|     10 |  584 | `	SySetRelease(&aNode);` |
|     10 |  585 | `	return SXRET_OK;` |
|      6 |  586 | `}` |
|      - |  587 | `/*` |
|      - |  588 | ` * Rehash all nodes keys after a merge-sort have been applied.` |
|      - |  589 | ` * Used by [sort(),usort() and rsort()].` |
|      - |  590 | ` */` |
|   1615 |  591 | `PH7_PRIVATE void HashmapSortRehash(ph7_hashmap *pMap)` |
|      5 |  592 | `{` |
|      - |  593 | `	ph7_hashmap_node *p,*pLast;` |
|      - |  594 | `	sxu32 i;` |
|      - |  595 | `	/* php's sorts rewind the array's internal pointer. The reordering paths reset` |
|      - |  596 | `	 * it themselves, but a ONE-element array is reindexed without being reordered` |
|      - |  597 | ``	 * — and `$a = ['x'=>1]; next($a); sort($a);` then left current() past the end,`` |
|      - |  598 | `	 * answering false where php answers the element. */` |
|   1620 |  599 | `	pMap->pCur = pMap->pFirst;` |
|      - |  600 | `	/* Rehash all entries */` |
|   1620 |  601 | `	pLast = p = pMap->pFirst;` |
|   1620 |  602 | `	pMap->iNextIdx = 0;` |
|   1620 |  603 | `	pMap->bIntKeySeen = 0; /* Reset the automatic index */` |
|   1620 |  604 | `	i = 0;` |
|  11274 |  605 | `	for( ;; ){` |
|  22558 |  606 | `		if( i >= pMap->nEntry ){` |
|   1620 |  607 | `			pMap->pLast = pLast; /* Fix the last link broken by the merge-sort */` |
|   1620 |  608 | `			break;` |
|      - |  609 | `		}` |
|  20943 |  610 | `		if( p->iType == HASHMAP_BLOB_NODE ){` |
|      - |  611 | `			/* Do not maintain index association as requested by the PHP specification */` |
|     50 |  612 | `			SyBlobRelease(&p->xKey.sKey);` |
|      - |  613 | `			/* Change key type */` |
|     50 |  614 | `			p->iType = HASHMAP_INT_NODE;` |
|     24 |  615 | `		}` |
|  20943 |  616 | `		HashmapRehashIntNode(p);` |
|      - |  617 | `		/* Point to the next entry */` |
|  20943 |  618 | `		i++;` |
|  20943 |  619 | `		pLast = p;` |
|  20943 |  620 | `		p = p->pPrev; /* Reverse link */` |
|      5 |  621 | `	}` |
|   1620 |  622 | `}` |
|      - |  623 | `/*` |
|      - |  624 | ` * Array functions implementation.` |
|      - |  625 | ` * Status:` |
|      - |  626 | ` *  Stable.` |
|      - |  627 | ` */` |
|      - |  628 | `/*` |
|      - |  629 | ` * Reset / report the comparator-throw flag around a FLAG sort. The string sort` |
|      - |  630 | ` * flags coerce their operands user-visibly (HashmapScalarFlagCmp), and a` |
|      - |  631 | ` * not-stringable object raises php's Error there; the comparator can only flag` |
|      - |  632 | ` * it, so every flag-sort driver clears the flag before its merge sort and` |
|      - |  633 | ` * answers PH7_EXCEPTION after it. php's array is sorted after the throw too, so` |
|      - |  634 | ` * the rehash still runs.` |
|      - |  635 | ` */` |
|   1807 |  636 | `static sxi32 HashmapFlagSortStatus(ph7_context *pCtx)` |
|      5 |  637 | `{` |
|   1812 |  638 | `	if( pCtx->pVm->iCmpCallbackExc ){` |
|     80 |  639 | `		sxi32 rcExc = pCtx->pVm->iCmpCallbackExc;` |
|     80 |  640 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|     80 |  641 | `		pCtx->nThrowRc = rcExc;` |
|     80 |  642 | `		return rcExc;` |
|      - |  643 | `	}` |
|   1734 |  644 | `	return PH7_OK;` |
|    908 |  645 | `}` |
|      - |  646 | `/*` |
|      - |  647 | ` * bool sort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|      - |  648 | ` * Sort an array.` |
|      - |  649 | ` * Parameters` |
|      - |  650 | ` *  $array` |
|      - |  651 | ` *   The input array.` |
|      - |  652 | ` * $sort_flags` |
|      - |  653 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|      - |  654 | ` *  Sorting type flags:` |
|      - |  655 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|      - |  656 | ` *   SORT_NUMERIC - compare items numerically` |
|      - |  657 | ` *   SORT_STRING - compare items as strings` |
|      - |  658 | ` * Return` |
|      - |  659 | ` *  TRUE on success or FALSE on failure.` |
|      - |  660 | ` *` |
|      - |  661 | ` */` |
|   1483 |  662 | `PH7_PRIVATE int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  663 | `{` |
|      - |  664 | `	ph7_hashmap *pMap;` |
|      - |  665 | `	/* Make sure we are dealing with a valid hashmap */` |
|   1488 |  666 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - |  667 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  668 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  669 | `		return PH7_OK;` |
|      - |  670 | `	}` |
|      - |  671 | `	/* Point to the internal representation of the input hashmap */` |
|   1488 |  672 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|   1488 |  673 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|   1488 |  674 | `	if( pMap->nEntry > 1 ){` |
|   1428 |  675 | `		sxi32 iCmpFlags = 0;` |
|   1428 |  676 | `		if( nArg > 1 ){` |
|      - |  677 | `			/* Extract comparison flags */` |
|     70 |  678 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     33 |  679 | `		}` |
|      - |  680 | `		/* Do the merge sort */` |
|   1428 |  681 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|   1428 |  682 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|      - |  683 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|   1428 |  684 | `		HashmapSortRehash(pMap);` |
|    776 |  685 | `	}else if( pMap->nEntry == 1 ){` |
|      - |  686 | `		/* php reindexes even a single-element array: a string key becomes 0 */` |
|     23 |  687 | `		HashmapSortRehash(pMap);` |
|     10 |  688 | `	}` |
|      - |  689 | `	{` |
|   1488 |  690 | `		sxi32 rcCmp = HashmapFlagSortStatus(pCtx);` |
|   1488 |  691 | `		if( rcCmp != PH7_OK ){` |
|     38 |  692 | `			return rcCmp;` |
|      - |  693 | `		}` |
|      - |  694 | `	}` |
|      - |  695 | `	/* All done,return TRUE */` |
|   1452 |  696 | `	ph7_result_bool(pCtx,1);` |
|   1452 |  697 | `	return PH7_OK;` |
|    746 |  698 | `}` |
|      - |  699 | `/*` |
|      - |  700 | ` * bool asort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|      - |  701 | ` *  Sort an array and maintain index association.` |
|      - |  702 | ` * Parameters` |
|      - |  703 | ` *  $array` |
|      - |  704 | ` *   The input array.` |
|      - |  705 | ` * $sort_flags` |
|      - |  706 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|      - |  707 | ` *  Sorting type flags:` |
|      - |  708 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|      - |  709 | ` *   SORT_NUMERIC - compare items numerically` |
|      - |  710 | ` *   SORT_STRING - compare items as strings` |
|      - |  711 | ` * Return` |
|      - |  712 | ` *  TRUE on success or FALSE on failure.` |
|      - |  713 | ` */` |
|     62 |  714 | `PH7_PRIVATE int ph7_hashmap_asort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  715 | `{` |
|      - |  716 | `	ph7_hashmap *pMap;` |
|      - |  717 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|     66 |  718 | `	if( nArg < 1 ){` |
|    ! 0 |  719 | `		return PH7_VmThrowException(pCtx,` |
|      - |  720 | `			"ArgumentCountError",` |
|      - |  721 | `			"asort() expects at least 1 argument, 0 given"` |
|      - |  722 | `			);` |
|      - |  723 | `	}` |
|      - |  724 | `	/* PHP 8: TypeError if first argument is not an array */` |
|     66 |  725 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|    ! 0 |  726 | `		return PH7_VmThrowException(pCtx,` |
|      - |  727 | `			"TypeError",` |
|      - |  728 | `			"asort(): Argument #1 ($array) must be of type array, %s given",` |
|    ! 0 |  729 | `			ph7_type_name(apArg[0])` |
|      - |  730 | `			);` |
|      - |  731 | `	}` |
|      - |  732 | `	/* Point to the internal representation of the input hashmap */` |
|     66 |  733 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|     66 |  734 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     66 |  735 | `	if( pMap->nEntry > 1 ){` |
|     60 |  736 | `		sxi32 iCmpFlags = 0;` |
|     60 |  737 | `		if( nArg > 1 ){` |
|      - |  738 | `			/* Extract comparison flags */` |
|     29 |  739 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     13 |  740 | `		}` |
|      - |  741 | `		/* Do the merge sort */` |
|     60 |  742 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|     60 |  743 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|      - |  744 | `		/* Fix the last link broken by the merge */` |
|    136 |  745 | `		while(pMap->pLast->pPrev){` |
|     80 |  746 | `			pMap->pLast = pMap->pLast->pPrev;` |
|      4 |  747 | `		}` |
|     28 |  748 | `	}` |
|      - |  749 | `	{` |
|     66 |  750 | `		sxi32 rcCmp = HashmapFlagSortStatus(pCtx);` |
|     66 |  751 | `		if( rcCmp != PH7_OK ){` |
|     16 |  752 | `			return rcCmp;` |
|      - |  753 | `		}` |
|      - |  754 | `	}` |
|      - |  755 | `	/* All done,return TRUE */` |
|     52 |  756 | `	ph7_result_bool(pCtx,1);` |
|     52 |  757 | `	return PH7_OK;` |
|     35 |  758 | `}` |
|      - |  759 | `/*` |
|      - |  760 | ` * bool natsort(array &$array)` |
|      - |  761 | ` * bool natcasesort(array &$array)` |
|      - |  762 | ` *  Sort an array with php's "natural order" algorithm, maintaining index` |
|      - |  763 | ` *  association: exactly asort() under SORT_NATURAL (plus SORT_FLAG_CASE for the` |
|      - |  764 | ` *  case-insensitive twin), which is how php implements them too.` |
|      - |  765 | ` *` |
|      - |  766 | `` *  They used to be PRELUDE wrappers over `uasort($array, 'strnatcmp')`, and that`` |
|      - |  767 | ` *  is a different function: uasort hands each element to a userland callback, so` |
|      - |  768 | `` *  every element had to satisfy strnatcmp's `string` ZPP row. php's natsort`` |
|      - |  769 | ` *  COERCES each element the way any string comparison does, so` |
|      - |  770 | `` *  `natsort([10, "9", null])` — nothing exotic, just a mixed array — was a`` |
|      - |  771 | ` *  TypeError in PHL and a sorted array in php, and an object with no` |
|      - |  772 | ` *  __toString() answered strnatcmp's ZPP TypeError instead of php's coercion` |
|      - |  773 | ` *  Error. Routing through the flag comparator picks up HashmapFlagStringify,` |
|      - |  774 | ` *  which already renders arrays with php's "Array to string conversion" warning` |
|      - |  775 | ` *  and raises the coercion Error once per sort.` |
|      - |  776 | ` */` |
|     26 |  777 | `PH7_PRIVATE int ph7_hashmap_natsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  778 | `{` |
|     29 |  779 | `	const char *zName = ph7_function_name(pCtx);` |
|      - |  780 | `	/* natcasesort() is the SORT_FLAG_CASE twin; match the whole name, not a byte. */` |
|     34 |  781 | `	int bFold = zName && SyStrlen(zName) == sizeof("natcasesort")-1` |
|     39 |  782 | `		&& SyMemcmp(zName,"natcasesort",sizeof("natcasesort")-1) == 0;` |
|      - |  783 | `	ph7_hashmap *pMap;` |
|     29 |  784 | `	if( nArg < 1 ){` |
|    ! 0 |  785 | `		return PH7_VmThrowException(pCtx,` |
|      - |  786 | `			"ArgumentCountError",` |
|    ! 0 |  787 | `			"%s() expects exactly 1 argument, 0 given",zName` |
|      - |  788 | `			);` |
|      - |  789 | `	}` |
|     29 |  790 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|    ! 0 |  791 | `		return PH7_VmThrowException(pCtx,` |
|      - |  792 | `			"TypeError",` |
|      - |  793 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|    ! 0 |  794 | `			zName,ph7_type_name(apArg[0])` |
|      - |  795 | `			);` |
|      - |  796 | `	}` |
|     29 |  797 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|     29 |  798 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     29 |  799 | `	if( pMap->nEntry > 1 ){` |
|      - |  800 | `		/* SORT_NATURAL (6), optionally \| SORT_FLAG_CASE (8) — the same iFlags` |
|      - |  801 | `		 * word asort() forwards, so this is asort($a, SORT_NATURAL) exactly. */` |
|     25 |  802 | `		sxi32 iCmpFlags = bFold ? (6\|8) : 6;` |
|     25 |  803 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|     25 |  804 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|     57 |  805 | `		while(pMap->pLast->pPrev){` |
|     35 |  806 | `			pMap->pLast = pMap->pLast->pPrev;` |
|      3 |  807 | `		}` |
|     11 |  808 | `	}` |
|      - |  809 | `	{` |
|     29 |  810 | `		sxi32 rcCmp = HashmapFlagSortStatus(pCtx);` |
|     29 |  811 | `		if( rcCmp != PH7_OK ){` |
|      5 |  812 | `			return rcCmp;` |
|      - |  813 | `		}` |
|      - |  814 | `	}` |
|     25 |  815 | `	ph7_result_bool(pCtx,1);` |
|     25 |  816 | `	return PH7_OK;` |
|     16 |  817 | `}` |
|      - |  818 | `/*` |
|      - |  819 | ` * bool arsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|      - |  820 | ` *  Sort an array in reverse order and maintain index association.` |
|      - |  821 | ` * Parameters` |
|      - |  822 | ` *  $array` |
|      - |  823 | ` *   The input array.` |
|      - |  824 | ` * $sort_flags` |
|      - |  825 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|      - |  826 | ` *  Sorting type flags:` |
|      - |  827 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|      - |  828 | ` *   SORT_NUMERIC - compare items numerically` |
|      - |  829 | ` *   SORT_STRING - compare items as strings` |
|      - |  830 | ` * Return` |
|      - |  831 | ` *  TRUE on success or FALSE on failure.` |
|      - |  832 | ` */` |
|     28 |  833 | `PH7_PRIVATE int ph7_hashmap_arsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  834 | `{` |
|      - |  835 | `	ph7_hashmap *pMap;` |
|      - |  836 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|     31 |  837 | `	if( nArg < 1 ){` |
|    ! 0 |  838 | `		return PH7_VmThrowException(pCtx,` |
|      - |  839 | `			"ArgumentCountError",` |
|      - |  840 | `			"arsort() expects at least 1 argument, 0 given"` |
|      - |  841 | `			);` |
|      - |  842 | `	}` |
|      - |  843 | `	/* PHP 8: TypeError if first argument is not an array */` |
|     31 |  844 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|    ! 0 |  845 | `		return PH7_VmThrowException(pCtx,` |
|      - |  846 | `			"TypeError",` |
|      - |  847 | `			"arsort(): Argument #1 ($array) must be of type array, %s given",` |
|    ! 0 |  848 | `			ph7_type_name(apArg[0])` |
|      - |  849 | `			);` |
|      - |  850 | `	}` |
|      - |  851 | `	/* Point to the internal representation of the input hashmap */` |
|     31 |  852 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|     31 |  853 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     31 |  854 | `	if( pMap->nEntry > 1 ){` |
|     27 |  855 | `		sxi32 iCmpFlags = 0;` |
|     27 |  856 | `		if( nArg > 1 ){` |
|      - |  857 | `			/* Extract comparison flags */` |
|     13 |  858 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|      5 |  859 | `		}` |
|      - |  860 | `		/* Do the merge sort */` |
|     27 |  861 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|     27 |  862 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|      - |  863 | `		/* Fix the last link broken by the merge */` |
|     45 |  864 | `		while(pMap->pLast->pPrev){` |
|     20 |  865 | `			pMap->pLast = pMap->pLast->pPrev;` |
|      2 |  866 | `		}` |
|     12 |  867 | `	}` |
|      - |  868 | `	{` |
|     31 |  869 | `		sxi32 rcCmp = HashmapFlagSortStatus(pCtx);` |
|     31 |  870 | `		if( rcCmp != PH7_OK ){` |
|      3 |  871 | `			return rcCmp;` |
|      - |  872 | `		}` |
|      - |  873 | `	}` |
|      - |  874 | `	/* All done,return TRUE */` |
|     28 |  875 | `	ph7_result_bool(pCtx,1);` |
|     28 |  876 | `	return PH7_OK;` |
|     17 |  877 | `}` |
|      - |  878 | `/*` |
|      - |  879 | ` * bool ksort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|      - |  880 | ` *  Sort an array by key.` |
|      - |  881 | ` * Parameters` |
|      - |  882 | ` *  $array` |
|      - |  883 | ` *   The input array.` |
|      - |  884 | ` * $sort_flags` |
|      - |  885 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|      - |  886 | ` *  Sorting type flags:` |
|      - |  887 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|      - |  888 | ` *   SORT_NUMERIC - compare items numerically` |
|      - |  889 | ` *   SORT_STRING - compare items as strings` |
|      - |  890 | ` * Return` |
|      - |  891 | ` *  TRUE on success or FALSE on failure.` |
|      - |  892 | ` */` |
|    108 |  893 | `PH7_PRIVATE int ph7_hashmap_ksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  894 | `{` |
|      - |  895 | `	ph7_hashmap *pMap;` |
|      - |  896 | `	/* Make sure we are dealing with a valid hashmap */` |
|    111 |  897 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - |  898 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  899 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  900 | `		return PH7_OK;` |
|      - |  901 | `	}` |
|      - |  902 | `	/* Point to the internal representation of the input hashmap */` |
|    111 |  903 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|    111 |  904 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|    111 |  905 | `	if( pMap->nEntry > 1 ){` |
|    111 |  906 | `		sxi32 iCmpFlags = 0;` |
|    111 |  907 | `		if( nArg > 1 ){` |
|      - |  908 | `			/* Extract comparison flags */` |
|     57 |  909 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     27 |  910 | `		}` |
|      - |  911 | `		/* Do the merge sort */` |
|    111 |  912 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|    111 |  913 | `		HashmapMergeSort(pMap,HashmapCmpCallback2,SX_INT_TO_PTR(iCmpFlags));` |
|      - |  914 | `		/* Fix the last link broken by the merge */` |
|    221 |  915 | `		while(pMap->pLast->pPrev){` |
|    112 |  916 | `			pMap->pLast = pMap->pLast->pPrev;` |
|      2 |  917 | `		}` |
|     54 |  918 | `	}` |
|      - |  919 | `	{` |
|    111 |  920 | `		sxi32 rcCmp = HashmapFlagSortStatus(pCtx);` |
|    111 |  921 | `		if( rcCmp != PH7_OK ){` |
|    ! 0 |  922 | `			return rcCmp;` |
|      - |  923 | `		}` |
|      - |  924 | `	}` |
|      - |  925 | `	/* All done,return TRUE */` |
|    111 |  926 | `	ph7_result_bool(pCtx,1);` |
|    111 |  927 | `	return PH7_OK;` |
|     57 |  928 | `}` |
|      - |  929 | `/*` |
|      - |  930 | ` * bool krsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|      - |  931 | ` *  Sort an array by key in reverse order.` |
|      - |  932 | ` * Parameters` |
|      - |  933 | ` *  $array` |
|      - |  934 | ` *   The input array.` |
|      - |  935 | ` * $sort_flags` |
|      - |  936 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|      - |  937 | ` *  Sorting type flags:` |
|      - |  938 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|      - |  939 | ` *   SORT_NUMERIC - compare items numerically` |
|      - |  940 | ` *   SORT_STRING - compare items as strings` |
|      - |  941 | ` * Return` |
|      - |  942 | ` *  TRUE on success or FALSE on failure.` |
|      - |  943 | ` */` |
|     30 |  944 | `PH7_PRIVATE int ph7_hashmap_krsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  945 | `{` |
|      - |  946 | `	ph7_hashmap *pMap;` |
|      - |  947 | `	/* Make sure we are dealing with a valid hashmap */` |
|     32 |  948 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - |  949 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  950 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  951 | `		return PH7_OK;` |
|      - |  952 | `	}` |
|      - |  953 | `	/* Point to the internal representation of the input hashmap */` |
|     32 |  954 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|     32 |  955 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     32 |  956 | `	if( pMap->nEntry > 1 ){` |
|     32 |  957 | `		sxi32 iCmpFlags = 0;` |
|     32 |  958 | `		if( nArg > 1 ){` |
|      - |  959 | `			/* Extract comparison flags */` |
|      6 |  960 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|      2 |  961 | `		}` |
|      - |  962 | `		/* Do the merge sort */` |
|     32 |  963 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|     32 |  964 | `		HashmapMergeSort(pMap,HashmapCmpCallback5,SX_INT_TO_PTR(iCmpFlags));` |
|      - |  965 | `		/* Fix the last link broken by the merge */` |
|     54 |  966 | `		while(pMap->pLast->pPrev){` |
|     23 |  967 | `			pMap->pLast = pMap->pLast->pPrev;` |
|      1 |  968 | `		}` |
|     15 |  969 | `	}` |
|      - |  970 | `	{` |
|     32 |  971 | `		sxi32 rcCmp = HashmapFlagSortStatus(pCtx);` |
|     32 |  972 | `		if( rcCmp != PH7_OK ){` |
|    ! 0 |  973 | `			return rcCmp;` |
|      - |  974 | `		}` |
|      - |  975 | `	}` |
|      - |  976 | `	/* All done,return TRUE */` |
|     32 |  977 | `	ph7_result_bool(pCtx,1);` |
|     32 |  978 | `	return PH7_OK;` |
|     17 |  979 | `}` |
|      - |  980 | `/*` |
|      - |  981 | ` * bool rsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|      - |  982 | ` * Sort an array in reverse order.` |
|      - |  983 | ` * Parameters` |
|      - |  984 | ` *  $array` |
|      - |  985 | ` *   The input array.` |
|      - |  986 | ` * $sort_flags` |
|      - |  987 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|      - |  988 | ` *  Sorting type flags:` |
|      - |  989 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|      - |  990 | ` *   SORT_NUMERIC - compare items numerically` |
|      - |  991 | ` *   SORT_STRING - compare items as strings` |
|      - |  992 | ` * Return` |
|      - |  993 | ` *  TRUE on success or FALSE on failure.` |
|      - |  994 | ` */` |
|     36 |  995 | `PH7_PRIVATE int ph7_hashmap_rsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  996 | `{` |
|      - |  997 | `	ph7_hashmap *pMap;` |
|      - |  998 | `	/* Make sure we are dealing with a valid hashmap */` |
|     40 |  999 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 1000 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1001 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1002 | `		return PH7_OK;` |
|      - | 1003 | `	}` |
|      - | 1004 | `	/* Point to the internal representation of the input hashmap */` |
|     40 | 1005 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|     40 | 1006 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     40 | 1007 | `	if( pMap->nEntry > 1 ){` |
|     38 | 1008 | `		sxi32 iCmpFlags = 0;` |
|     38 | 1009 | `		if( nArg > 1 ){` |
|      - | 1010 | `			/* Extract comparison flags */` |
|     16 | 1011 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|      6 | 1012 | `		}` |
|      - | 1013 | `		/* Do the merge sort */` |
|     38 | 1014 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|     38 | 1015 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|      - | 1016 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|     38 | 1017 | `		HashmapSortRehash(pMap);` |
|     20 | 1018 | `	}else if( pMap->nEntry == 1 ){` |
|      - | 1019 | `		/* php reindexes even a single-element array: a string key becomes 0 */` |
|      3 | 1020 | `		HashmapSortRehash(pMap);` |
|      1 | 1021 | `	}` |
|      - | 1022 | `	{` |
|     40 | 1023 | `		sxi32 rcCmp = HashmapFlagSortStatus(pCtx);` |
|     40 | 1024 | `		if( rcCmp != PH7_OK ){` |
|     14 | 1025 | `			return rcCmp;` |
|      - | 1026 | `		}` |
|      - | 1027 | `	}` |
|      - | 1028 | `	/* All done,return TRUE */` |
|     27 | 1029 | `	ph7_result_bool(pCtx,1);` |
|     27 | 1030 | `	return PH7_OK;` |
|     22 | 1031 | `}` |
|      - | 1032 | `/*` |
|      - | 1033 | ` * bool usort(array &$array,callable $cmp_function)` |
|      - | 1034 | ` *  Sort an array by values using a user-defined comparison function.` |
|      - | 1035 | ` * Parameters` |
|      - | 1036 | ` *  $array` |
|      - | 1037 | ` *   The input array.` |
|      - | 1038 | ` * $cmp_function` |
|      - | 1039 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|      - | 1040 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|      - | 1041 | ` *  to, or greater than the second.` |
|      - | 1042 | ` *    int callback ( mixed $a, mixed $b )` |
|      - | 1043 | ` * Return` |
|      - | 1044 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1045 | ` */` |
|    130 | 1046 | `PH7_PRIVATE int ph7_hashmap_usort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1047 | `{` |
|      - | 1048 | `	ph7_hashmap *pMap;` |
|      - | 1049 | `	/* Make sure we are dealing with a valid hashmap */` |
|    135 | 1050 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 1051 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1052 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1053 | `		return PH7_OK;` |
|      - | 1054 | `	}` |
|    135 | 1055 | `	if( nArg > 1 ){` |
|      - | 1056 | `		/* php rejects an uncallable comparator with a TypeError, BEFORE touching the` |
|      - | 1057 | `		 * array. PH7 handed it to the dispatcher, which answers NULL in silence -- so` |
|      - | 1058 | `		 * usort() with a bogus comparator reordered the array anyway, by nothing. */` |
|    135 | 1059 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",0);` |
|    135 | 1060 | `		if( rcCb != PH7_OK ){` |
|      7 | 1061 | `			return rcCb;` |
|      - | 1062 | `		}` |
|     62 | 1063 | `	}` |
|      - | 1064 | `	/* Point to the internal representation of the input hashmap */` |
|    129 | 1065 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|    129 | 1066 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|    129 | 1067 | `	if( pMap->nEntry > 1 ){` |
|    127 | 1068 | `		ph7_value *pCallback = 0;` |
|      - | 1069 | `		ProcNodeCmp xCmp;` |
|    127 | 1070 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|    127 | 1071 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|      - | 1072 | `			/* Point to the desired callback */` |
|    127 | 1073 | `			pCallback = apArg[1];` |
|     66 | 1074 | `		}else{` |
|      - | 1075 | `			/* Use the default comparison function */` |
|    ! 0 | 1076 | `			xCmp = HashmapCmpCallback1;` |
|      - | 1077 | `		}` |
|      - | 1078 | `		/* Do the merge sort */` |
|    127 | 1079 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|    127 | 1080 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|      - | 1081 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|    127 | 1082 | `		HashmapSortRehash(pMap);` |
|    127 | 1083 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|      - | 1084 | `			/* The comparison callback did not return: propagate its status so the` |
|      - | 1085 | `			 * dispatcher unwinds. */` |
|     13 | 1086 | `			sxi32 rcExc = pCtx->pVm->iCmpCallbackExc;` |
|     13 | 1087 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|     13 | 1088 | `			return rcExc;` |
|      5 | 1089 | `		}` |
|     59 | 1090 | `	}else if( pMap->nEntry == 1 ){` |
|      - | 1091 | `		/* php reindexes even a single-element array: a string key becomes 0 */` |
|      3 | 1092 | `		HashmapSortRehash(pMap);` |
|      1 | 1093 | `	}` |
|      - | 1094 | `	/* All done,return TRUE */` |
|    119 | 1095 | `	ph7_result_bool(pCtx,1);` |
|    119 | 1096 | `	return PH7_OK;` |
|     70 | 1097 | `}` |
|      - | 1098 | `/*` |
|      - | 1099 | ` * bool uasort(array &$array,callable $cmp_function)` |
|      - | 1100 | ` *  Sort an array by values using a user-defined comparison function` |
|      - | 1101 | ` *  and maintain index association.` |
|      - | 1102 | ` * Parameters` |
|      - | 1103 | ` *  $array` |
|      - | 1104 | ` *   The input array.` |
|      - | 1105 | ` * $cmp_function` |
|      - | 1106 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|      - | 1107 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|      - | 1108 | ` *  to, or greater than the second.` |
|      - | 1109 | ` *    int callback ( mixed $a, mixed $b )` |
|      - | 1110 | ` * Return` |
|      - | 1111 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1112 | ` */` |
|     14 | 1113 | `PH7_PRIVATE int ph7_hashmap_uasort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1114 | `{` |
|      - | 1115 | `	ph7_hashmap *pMap;` |
|      - | 1116 | `	/* Make sure we are dealing with a valid hashmap */` |
|     16 | 1117 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 1118 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1119 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1120 | `		return PH7_OK;` |
|      - | 1121 | `	}` |
|     16 | 1122 | `	if( nArg > 1 ){` |
|      - | 1123 | `		/* php rejects an uncallable comparator with a TypeError, BEFORE touching the` |
|      - | 1124 | `		 * array. PH7 handed it to the dispatcher, which answers NULL in silence -- so` |
|      - | 1125 | `		 * usort() with a bogus comparator reordered the array anyway, by nothing. */` |
|     16 | 1126 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",0);` |
|     16 | 1127 | `		if( rcCb != PH7_OK ){` |
|      5 | 1128 | `			return rcCb;` |
|      - | 1129 | `		}` |
|      5 | 1130 | `	}` |
|      - | 1131 | `	/* Point to the internal representation of the input hashmap */` |
|     12 | 1132 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|     12 | 1133 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     12 | 1134 | `	if( pMap->nEntry > 1 ){` |
|     12 | 1135 | `		ph7_value *pCallback = 0;` |
|      - | 1136 | `		ProcNodeCmp xCmp;` |
|     12 | 1137 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|     12 | 1138 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|      - | 1139 | `			/* Point to the desired callback */` |
|     12 | 1140 | `			pCallback = apArg[1];` |
|      7 | 1141 | `		}else{` |
|      - | 1142 | `			/* Use the default comparison function */` |
|    ! 0 | 1143 | `			xCmp = HashmapCmpCallback1;` |
|      - | 1144 | `		}` |
|      - | 1145 | `		/* Do the merge sort */` |
|     12 | 1146 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|     12 | 1147 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|      - | 1148 | `		/* Fix the last link broken by the merge */` |
|     24 | 1149 | `		while(pMap->pLast->pPrev){` |
|     13 | 1150 | `			pMap->pLast = pMap->pLast->pPrev;` |
|      1 | 1151 | `		}` |
|     12 | 1152 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|      - | 1153 | `			/* The comparison callback did not return: propagate its status so the` |
|      - | 1154 | `			 * dispatcher unwinds. */` |
|      3 | 1155 | `			sxi32 rcExc = pCtx->pVm->iCmpCallbackExc;` |
|      3 | 1156 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|      3 | 1157 | `			return rcExc;` |
|      - | 1158 | `		}` |
|      4 | 1159 | `	}` |
|      - | 1160 | `	/* All done,return TRUE */` |
|      9 | 1161 | `	ph7_result_bool(pCtx,1);` |
|      9 | 1162 | `	return PH7_OK;` |
|      9 | 1163 | `}` |
|      - | 1164 | `/*` |
|      - | 1165 | ` * bool uksort(array &$array,callable $cmp_function)` |
|      - | 1166 | ` *  Sort an array by keys using a user-defined comparison` |
|      - | 1167 | ` *  function and maintain index association.` |
|      - | 1168 | ` * Parameters` |
|      - | 1169 | ` *  $array` |
|      - | 1170 | ` *   The input array.` |
|      - | 1171 | ` * $cmp_function` |
|      - | 1172 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|      - | 1173 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|      - | 1174 | ` *  to, or greater than the second.` |
|      - | 1175 | ` *    int callback ( mixed $a, mixed $b )` |
|      - | 1176 | ` * Return` |
|      - | 1177 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1178 | ` */` |
|     10 | 1179 | `PH7_PRIVATE int ph7_hashmap_uksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1180 | `{` |
|      - | 1181 | `	ph7_hashmap *pMap;` |
|      - | 1182 | `	/* Make sure we are dealing with a valid hashmap */` |
|     12 | 1183 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 1184 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1185 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1186 | `		return PH7_OK;` |
|      - | 1187 | `	}` |
|     12 | 1188 | `	if( nArg > 1 ){` |
|      - | 1189 | `		/* php rejects an uncallable comparator with a TypeError, BEFORE touching the` |
|      - | 1190 | `		 * array. PH7 handed it to the dispatcher, which answers NULL in silence -- so` |
|      - | 1191 | `		 * usort() with a bogus comparator reordered the array anyway, by nothing. */` |
|     12 | 1192 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",0);` |
|     12 | 1193 | `		if( rcCb != PH7_OK ){` |
|      3 | 1194 | `			return rcCb;` |
|      - | 1195 | `		}` |
|      4 | 1196 | `	}` |
|      - | 1197 | `	/* Point to the internal representation of the input hashmap */` |
|     10 | 1198 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|     10 | 1199 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     10 | 1200 | `	if( pMap->nEntry > 1 ){` |
|     10 | 1201 | `		ph7_value *pCallback = 0;` |
|      - | 1202 | `		ProcNodeCmp xCmp;` |
|     10 | 1203 | `		xCmp = HashmapCmpCallback6; /* User-defined function as the comparison callback */` |
|     10 | 1204 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|      - | 1205 | `			/* Point to the desired callback */` |
|     10 | 1206 | `			pCallback = apArg[1];` |
|      6 | 1207 | `		}else{` |
|      - | 1208 | `			/* Use the default comparison function */` |
|    ! 0 | 1209 | `			xCmp = HashmapCmpCallback2;` |
|      - | 1210 | `		}` |
|      - | 1211 | `		/* Do the merge sort */` |
|     10 | 1212 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|     10 | 1213 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|      - | 1214 | `		/* Fix the last link broken by the merge */` |
|     12 | 1215 | `		while(pMap->pLast->pPrev){` |
|      3 | 1216 | `			pMap->pLast = pMap->pLast->pPrev;` |
|      1 | 1217 | `		}` |
|     10 | 1218 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|      - | 1219 | `			/* The comparison callback did not return: propagate its status so the` |
|      - | 1220 | `			 * dispatcher unwinds. */` |
|      3 | 1221 | `			sxi32 rcExc = pCtx->pVm->iCmpCallbackExc;` |
|      3 | 1222 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|      3 | 1223 | `			return rcExc;` |
|      - | 1224 | `		}` |
|      3 | 1225 | `	}` |
|      - | 1226 | `	/* All done,return TRUE */` |
|      7 | 1227 | `	ph7_result_bool(pCtx,1);` |
|      7 | 1228 | `	return PH7_OK;` |
|      7 | 1229 | `}` |
|      - | 1230 | `/*` |
|      - | 1231 | ` * bool array_multisort(array &$array, mixed $array1_sort_order = SORT_ASC,` |
|      - | 1232 | ` *                      mixed $array1_sort_flags = SORT_REGULAR, mixed &...$rest)` |
|      - | 1233 | ` *  Sort multiple arrays at once: the argument list is a little language read` |
|      - | 1234 | ` *  left to right — an ARRAY opens a column, and each column may be followed by` |
|      - | 1235 | ` *  at most one sort ORDER (SORT_ASC/SORT_DESC) and at most one sort FLAGS` |
|      - | 1236 | ` *  value, in either order. Rows are compared column by column, a tie in one` |
|      - | 1237 | ` *  column falling through to the next; the resulting permutation is applied to` |
|      - | 1238 | ` *  EVERY column. String keys are kept, numeric keys are renumbered, and the` |
|      - | 1239 | ` *  sort is stable (php 8's own guarantee). php's error shapes, pinned by` |
|      - | 1240 | `` *  probe: a non-int non-array is `Argument #N must be an array or a sort`` |
|      - | 1241 | `` *  flag`; a misplaced or repeated order/flags is the same text with`` |
|      - | 1242 | `` *  `... that has not already been specified`; an int that is no flag at all is`` |
|      - | 1243 | `` *  the ValueError `must be a valid sort flag`; mismatched lengths are`` |
|      - | 1244 | `` *  `Array sizes are inconsistent` with no function prefix; and only`` |
|      - | 1245 | `` *  Argument #1 carries its `($array)` name. php declares the whole list`` |
|      - | 1246 | ` *  prefer-ref (VmBuiltinPrefersRef), so a literal sorts a temporary silently.` |
|      - | 1247 | ` */` |
|      - | 1248 | `/* One column of the multisort: the caller's array plus its sort spec. */` |
|      - | 1249 | `typedef struct MultisortCol MultisortCol;` |
|      - | 1250 | `struct MultisortCol` |
|      - | 1251 | `{` |
|      - | 1252 | `	ph7_value *pArr;        /* The caller's argument slot */` |
|      - | 1253 | `	ph7_hashmap *pMap;      /* Its hashmap */` |
|      - | 1254 | `	ph7_hashmap_node **apNode; /* Nodes in ORIGINAL iteration order */` |
|      - | 1255 | `	sxi32 iFlags;           /* SORT_* comparison flags */` |
|      - | 1256 | `	int iDir;               /* +1 SORT_ASC, -1 SORT_DESC */` |
|      - | 1257 | `	int bOrderSeen;         /* An order argument already attached */` |
|      - | 1258 | `	int bFlagsSeen;         /* A flags argument already attached */` |
|      - | 1259 | `};` |
|      - | 1260 | `/* Compare two ROWS, column by column with each column's own direction/flags. */` |
|     96 | 1261 | `static sxi32 MultisortRowCmp(MultisortCol *aCol,sxu32 nCol,sxu32 iA,sxu32 iB)` |
|      4 | 1262 | `{` |
|    100 | 1263 | `	ph7_vm *pVm = aCol[0].pMap->pVm;` |
|      - | 1264 | `	sxu32 c;` |
|    110 | 1265 | `	for( c = 0 ; c < nCol ; c++ ){` |
|    110 | 1266 | `		sxi32 rc = HashmapFlagValueCmp(aCol[c].apNode[iA],aCol[c].apNode[iB],aCol[c].iFlags);` |
|    110 | 1267 | `		HashmapCmpLatch(pVm);` |
|    110 | 1268 | `		if( rc != 0 ){` |
|    100 | 1269 | `			return aCol[c].iDir < 0 ? -rc : rc;` |
|      - | 1270 | `		}` |
|      6 | 1271 | `	}` |
|    ! 0 | 1272 | `	return 0;` |
|     52 | 1273 | `}` |
|      - | 1274 | `/*` |
|      - | 1275 | ` * Stable bottom-up merge sort over the row-index permutation. Iterative on` |
|      - | 1276 | ` * purpose — the recursive shape would put O(log n) frames on the native stack` |
|      - | 1277 | ` * (the §7 embedder C-stack family).` |
|      - | 1278 | ` */` |
|     34 | 1279 | `static void MultisortSortIdx(MultisortCol *aCol,sxu32 nCol,sxu32 *aIdx,sxu32 *aTmp,sxu32 n)` |
|      4 | 1280 | `{` |
|      - | 1281 | `	sxu32 nWidth,iLo;` |
|    102 | 1282 | `	for( nWidth = 1 ; nWidth < n ; nWidth *= 2 ){` |
|    162 | 1283 | `		for( iLo = 0 ; iLo < n ; iLo += 2 * nWidth ){` |
|     98 | 1284 | `			sxu32 iMid = iLo + nWidth;` |
|     98 | 1285 | `			sxu32 iHi = iLo + 2 * nWidth;` |
|      - | 1286 | `			sxu32 i,j,k;` |
|     98 | 1287 | `			if( iMid > n ){ iMid = n; }` |
|     98 | 1288 | `			if( iHi > n ){ iHi = n; }` |
|     98 | 1289 | `			i = iLo; j = iMid; k = iLo;` |
|    194 | 1290 | `			while( i < iMid && j < iHi ){` |
|      - | 1291 | `				/* <= keeps the run stable: on a full tie the left row wins */` |
|    100 | 1292 | `				if( MultisortRowCmp(aCol,nCol,aIdx[i],aIdx[j]) <= 0 ){` |
|     44 | 1293 | `					aTmp[k++] = aIdx[i++];` |
|     24 | 1294 | `				}else{` |
|     60 | 1295 | `					aTmp[k++] = aIdx[j++];` |
|      - | 1296 | `				}` |
|      4 | 1297 | `			}` |
|    182 | 1298 | `			while( i < iMid ){ aTmp[k++] = aIdx[i++]; }` |
|    114 | 1299 | `			while( j < iHi ){ aTmp[k++] = aIdx[j++]; }` |
|     51 | 1300 | `		}` |
|      - | 1301 | `		/* aTmp holds the merged runs for this width; swap roles by copying` |
|      - | 1302 | `		 * back — n is bounded by the array count, one memcpy per doubling. */` |
|     68 | 1303 | `		SyMemcpy(aTmp,aIdx,(sxu32)(n * sizeof(sxu32)));` |
|     36 | 1304 | `	}` |
|     38 | 1305 | `}` |
|     60 | 1306 | `PH7_PRIVATE int ph7_hashmap_multisort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 1307 | `{` |
|      - | 1308 | `	MultisortCol *aCol;` |
|      - | 1309 | `	sxu32 *aIdx,*aTmp;` |
|     64 | 1310 | `	sxu32 nCol = 0,nRow,i;` |
|      - | 1311 | `	sxi32 rcStatus;` |
|      - | 1312 | `	int iArg;` |
|      - | 1313 |  |
|     64 | 1314 | `	if( nArg < 1 ){` |
|    ! 0 | 1315 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1316 | `			"ArgumentCountError",` |
|      - | 1317 | `			"array_multisort() expects at least 1 argument, %d given",` |
|    ! 0 | 1318 | `			nArg` |
|      - | 1319 | `			);` |
|      - | 1320 | `	}` |
|     94 | 1321 | `	aCol = (MultisortCol *)ph7_context_alloc_chunk(pCtx,` |
|     60 | 1322 | `		(unsigned int)(sizeof(MultisortCol) * (sxu32)nArg),TRUE,TRUE);` |
|     64 | 1323 | `	if( aCol == 0 ){` |
|    ! 0 | 1324 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1325 | `	}` |
|      - | 1326 | `	/* Read the argument list's little language, php's own state machine. */` |
|    162 | 1327 | `	for( iArg = 0 ; iArg < nArg ; iArg++ ){` |
|    124 | 1328 | `		ph7_value *pArg = apArg[iArg];` |
|      - | 1329 | `		/* Only Argument #1 carries its parameter name in php's messages. */` |
|    124 | 1330 | `		const char *zName = (iArg == 0) ? " ($array)" : "";` |
|    124 | 1331 | `		if( ph7_value_is_array(pArg) ){` |
|     80 | 1332 | `			MultisortCol *pCol = &aCol[nCol++];` |
|     80 | 1333 | `			pCol->pArr = pArg;` |
|     80 | 1334 | `			pCol->pMap = (ph7_hashmap *)pArg->x.pOther;` |
|     80 | 1335 | `			pCol->apNode = 0;` |
|     80 | 1336 | `			pCol->iFlags = 0; /* SORT_REGULAR */` |
|     80 | 1337 | `			pCol->iDir = 1;   /* SORT_ASC */` |
|     80 | 1338 | `			pCol->bOrderSeen = pCol->bFlagsSeen = 0;` |
|     80 | 1339 | `			continue;` |
|      - | 1340 | `		}` |
|     47 | 1341 | `		if( ph7_value_is_float(pArg) \|\| (pArg->iFlags & MEMOBJ_INT) == 0 ){` |
|      - | 1342 | `			/* A REAL int only, float asked FIRST (ph7_type_name's rule: an` |
|      - | 1343 | `			 * integer-valued real caches an int and would pass a bare flag` |
|      - | 1344 | `			 * test). php coerces nothing here: "4", 4.0 and true are all` |
|      - | 1345 | `			 * refused. */` |
|     17 | 1346 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1347 | `				"array_multisort(): Argument #%d%s must be an array or a sort flag",` |
|      5 | 1348 | `				iArg + 1,zName);` |
|      - | 1349 | `		}` |
|      - | 1350 | `		{` |
|     37 | 1351 | `			sxi64 iVal = ph7_value_to_int64(pArg);` |
|      - | 1352 | `			/* php masks SORT_FLAG_CASE off for the ORDER match too, but reads` |
|      - | 1353 | `			 * the direction from the UNMASKED value — so SORT_DESC\|SORT_FLAG_CASE` |
|      - | 1354 | `			 * (11) consumes the order slot and quirkily sorts ASCENDING. */` |
|     37 | 1355 | `			if( (iVal & ~(sxi64)8) == 3 /* SORT_DESC */ \|\| (iVal & ~(sxi64)8) == 4 /* SORT_ASC */ ){` |
|     23 | 1356 | `				if( nCol < 1 \|\| aCol[nCol - 1].bOrderSeen ){` |
|     11 | 1357 | `					return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1358 | `						"array_multisort(): Argument #%d%s must be an array or a sort flag that has not already been specified",` |
|      3 | 1359 | `						iArg + 1,zName);` |
|      - | 1360 | `				}` |
|     17 | 1361 | `				aCol[nCol - 1].iDir = (iVal == 3) ? -1 : 1;` |
|     17 | 1362 | `				aCol[nCol - 1].bOrderSeen = 1;` |
|     24 | 1363 | `			}else if( (iVal & ~(sxi64)8 /* SORT_FLAG_CASE */) == 0 /* SORT_REGULAR */` |
|     13 | 1364 | `			       \|\| (iVal & ~(sxi64)8) == 1 /* SORT_NUMERIC */` |
|     11 | 1365 | `			       \|\| (iVal & ~(sxi64)8) == 2 /* SORT_STRING */` |
|      8 | 1366 | `			       \|\| (iVal & ~(sxi64)8) == 5 /* SORT_LOCALE_STRING */` |
|      7 | 1367 | `			       \|\| (iVal & ~(sxi64)8) == 6 /* SORT_NATURAL */ ){` |
|     14 | 1368 | `				if( nCol < 1 \|\| aCol[nCol - 1].bFlagsSeen ){` |
|      7 | 1369 | `					return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1370 | `						"array_multisort(): Argument #%d%s must be an array or a sort flag that has not already been specified",` |
|      2 | 1371 | `						iArg + 1,zName);` |
|      - | 1372 | `				}` |
|     10 | 1373 | `				aCol[nCol - 1].iFlags = (sxi32)iVal;` |
|     10 | 1374 | `				aCol[nCol - 1].bFlagsSeen = 1;` |
|      6 | 1375 | `			}else{` |
|      4 | 1376 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1377 | `					"array_multisort(): Argument #%d%s must be a valid sort flag",` |
|      1 | 1378 | `					iArg + 1,zName);` |
|      - | 1379 | `			}` |
|      - | 1380 | `		}` |
|     14 | 1381 | `	}` |
|      - | 1382 | `	/* Every column must hold the same number of rows; php's message carries no` |
|      - | 1383 | `	 * function prefix. */` |
|     42 | 1384 | `	nRow = aCol[0].pMap->nEntry;` |
|     58 | 1385 | `	for( i = 1 ; i < nCol ; i++ ){` |
|     21 | 1386 | `		if( aCol[i].pMap->nEntry != nRow ){` |
|      3 | 1387 | `			return PH7_VmThrowException(pCtx,"ValueError","Array sizes are inconsistent");` |
|      - | 1388 | `		}` |
|     10 | 1389 | `	}` |
|     40 | 1390 | `	if( nRow > 0 ){` |
|      - | 1391 | `		/* Collect each column's nodes in original order. */` |
|     86 | 1392 | `		for( i = 0 ; i < nCol ; i++ ){` |
|     52 | 1393 | `			ph7_hashmap_node *pNode = aCol[i].pMap->pFirst;` |
|      - | 1394 | `			sxu32 r;` |
|     76 | 1395 | `			aCol[i].apNode = (ph7_hashmap_node **)ph7_context_alloc_chunk(pCtx,` |
|     24 | 1396 | `				(unsigned int)(sizeof(ph7_hashmap_node *) * nRow),FALSE,TRUE);` |
|     52 | 1397 | `			if( aCol[i].apNode == 0 ){` |
|    ! 0 | 1398 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 1399 | `			}` |
|    196 | 1400 | `			for( r = 0 ; r < nRow && pNode ; r++ ){` |
|    148 | 1401 | `				aCol[i].apNode[r] = pNode;` |
|    148 | 1402 | `				pNode = pNode->pPrev; /* Reverse link */` |
|     76 | 1403 | `			}` |
|     28 | 1404 | `		}` |
|     55 | 1405 | `		aIdx = (sxu32 *)ph7_context_alloc_chunk(pCtx,` |
|     17 | 1406 | `			(unsigned int)(sizeof(sxu32) * nRow * 2),FALSE,TRUE);` |
|     38 | 1407 | `		if( aIdx == 0 ){` |
|    ! 0 | 1408 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 1409 | `		}` |
|     38 | 1410 | `		aTmp = &aIdx[nRow];` |
|    140 | 1411 | `		for( i = 0 ; i < nRow ; i++ ){` |
|    106 | 1412 | `			aIdx[i] = i;` |
|     55 | 1413 | `		}` |
|      - | 1414 | `		/* The string flags coerce user-visibly and can only FLAG a throw` |
|      - | 1415 | `		 * (iCmpCallbackExc); clear it, sort, and report after — the flag-sort` |
|      - | 1416 | `		 * drivers' shared pattern. */` |
|     38 | 1417 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|     38 | 1418 | `		MultisortSortIdx(aCol,nCol,aIdx,aTmp,nRow);` |
|     38 | 1419 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|      - | 1420 | `			/* A comparison raised. php's array_multisort leaves EVERY column as` |
|      - | 1421 | `			 * it found it in that case -- unlike sort(), which still writes back` |
|      - | 1422 | `			 * the order it reached -- so the permutation is dropped rather than` |
|      - | 1423 | `			 * applied. */` |
|     11 | 1424 | `			return HashmapFlagSortStatus(pCtx);` |
|      - | 1425 | `		}` |
|      - | 1426 | `		/* Apply the permutation to every column: rebuild in sorted order,` |
|      - | 1427 | `		 * keeping string keys and renumbering int keys, then hand the fresh` |
|      - | 1428 | `		 * array back through the by-ref slot (a literal has none and the` |
|      - | 1429 | `		 * result is silently dropped — php's prefer-ref). */` |
|     63 | 1430 | `		for( i = 0 ; i < nCol ; i++ ){` |
|     39 | 1431 | `			ph7_value *pNew = ph7_context_new_array(pCtx);` |
|      - | 1432 | `			sxu32 r;` |
|     39 | 1433 | `			if( pNew == 0 ){` |
|    ! 0 | 1434 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 1435 | `			}` |
|    147 | 1436 | `			for( r = 0 ; r < nRow ; r++ ){` |
|    111 | 1437 | `				ph7_hashmap_node *pNode = aCol[i].apNode[aIdx[r]];` |
|    165 | 1438 | `				HashmapInsertNode((ph7_hashmap *)pNew->x.pOther,pNode,` |
|    108 | 1439 | `					pNode->iType == HASHMAP_BLOB_NODE ? TRUE : FALSE);` |
|     57 | 1440 | `			}` |
|     39 | 1441 | `			PH7_VmStoreArgByRef(pCtx->pVm,aCol[i].pArr,pNew);` |
|     21 | 1442 | `		}` |
|     27 | 1443 | `		rcStatus = HashmapFlagSortStatus(pCtx);` |
|     27 | 1444 | `		if( rcStatus != PH7_OK ){` |
|    ! 0 | 1445 | `			return rcStatus;` |
|      - | 1446 | `		}` |
|     12 | 1447 | `	}` |
|     29 | 1448 | `	ph7_result_bool(pCtx,1);` |
|     29 | 1449 | `	return PH7_OK;` |
|     34 | 1450 | `}` |
|      - | 1451 |  |
