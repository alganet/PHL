# src/ph7/hashmap_sort.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 838/896 lines (93.53%)

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
|       - |    9 | ` *    Hashmap (array) sorting: php's zend_sort hybrid, its comparator` |
|       - |   10 | ` *    set and the sort()/usort()/ksort() builtin family.` |
|       - |   11 | ` * Status:` |
|       - |   12 | ` *    Stable.` |
|       - |   13 | ` */` |
|       - |   14 | `/*` |
|       - |   15 | ` * The ordering primitive.` |
|       - |   16 | ` *` |
|       - |   17 | ` * php sorts a PRIVATE COPY of the array (zend_array_dup in php_usort and` |
|       - |   18 | ` * friends), so a comparator that reads the array being sorted always sees it` |
|       - |   19 | ` * exactly as it was at the call, and a write the comparator makes to it is` |
|       - |   20 | ` * discarded when the sorted copy is installed. PHL used to thread the order` |
|       - |   21 | ` * through the nodes' own pPrev/pNext links, which are the map's ONLY list:` |
|       - |   22 | `` * while the sort ran, `pMap->pFirst` led into half-merged runs, so a comparator`` |
|       - |   23 | ` * that so much as printed the array walked freed-looking garbage and the` |
|       - |   24 | `` * interpreter died -- `usort($a, function($x,$y) use (&$a){ ... $a ... })` is a`` |
|       - |   25 | ` * segmentation fault, not a wrong answer.` |
|       - |   26 | ` *` |
|       - |   27 | ` * The order is decided over a VECTOR of node pointers instead, and the map's` |
|       - |   28 | ` * list is relinked once, after the last comparison. The array therefore stays` |
|       - |   29 | ` * fully walkable, in its pre-call order, for as long as user code can see it --` |
|       - |   30 | ` * which is php's answer -- and the nodes are never left in an inconsistent` |
|       - |   31 | ` * state for anything else to trip over.` |
|       - |   32 | ` *` |
|       - |   33 | ` * WHICH order the vector is put in is itself observable, and this used to be a` |
|       - |   34 | ` * bottom-up merge sort where php runs the hybrid insertion/quick sort of` |
|       - |   35 | `` * `zend_sort` (itself derived from libc++'s std::sort). Two programs can tell`` |
|       - |   36 | ` * the difference:` |
|       - |   37 | ` *` |
|       - |   38 | ``  *   - A comparator counts, prints or throws. `array_udiff([1,2,3],[2,3,4],$f)` `` |
|       - |   39 | ` *     entered $f six times here and nine times in php; a comparator that logs is` |
|       - |   40 | ` *     reading the engine's own decisions, so the SEQUENCE of pairs is contract,` |
|       - |   41 | ` *     not an implementation detail.` |
|       - |   42 | ` *   - The comparison is not an ordering. NAN answers 1 against everything in` |
|       - |   43 | `` *     both directions, so no two algorithms need agree: `sort([3,NAN,1,2])` is`` |
|       - |   44 | `` *     `1 2 NAN 3` under a merge and `NAN 1 2 3` in php.`` |
|       - |   45 | ` *` |
|       - |   46 | `` * Stability is php's too, and is NOT a property of this sort: `zend_sort` is a`` |
|       - |   47 | ` * quicksort and reorders equal elements freely. php's sorts have been stable` |
|       - |   48 | `` * since 8.0 because `zend_hash_sort_internal` stamps every bucket with its`` |
|       - |   49 | ` * original position first, and the sort builtins' comparators fall back on that` |
|       - |   50 | ` * stamp when the real comparison answers 0. nOrd is that stamp, and bStable` |
|       - |   51 | ` * picks the comparator that consults it -- php's diff/intersect helpers sort` |
|       - |   52 | ` * their operand lists with the UNSTABLE variants instead.` |
|       - |   53 | ` */` |
|       - |   54 | `typedef struct HashmapSortCtx HashmapSortCtx;` |
|       - |   55 | `struct HashmapSortCtx {` |
|       - |   56 | `	ProcNodeCmp xCmp;        /* The caller's node comparison */` |
|       - |   57 | `	void *pCmpData;          /* Opaque comparison data */` |
|       - |   58 | `	int bStable;             /* Break a tie on nOrd rather than leaving it to the sort */` |
|       - |   59 | `};` |
|  161009 |   60 | `static sxi32 HashmapSortCmp(HashmapSortCtx *pCtx,HashmapSortEnt *pA,HashmapSortEnt *pB)` |
|       5 |   61 | `{` |
|  161014 |   62 | `	sxi32 rc = pCtx->xCmp(pA->pNode,pB->pNode,pCtx->pCmpData);` |
|  161014 |   63 | `	if( rc == 0 && pCtx->bStable ){` |
|       - |   64 | `		/* php's stable_sort_fallback(): the original positions, never equal */` |
|     417 |   65 | `		rc = ( pA->nOrd > pB->nOrd ) ? 1 : -1;` |
|     206 |   66 | `	}` |
|  161014 |   67 | `	return rc;` |
|       5 |   68 | `}` |
|   37157 |   69 | `static void HashmapSortSwap(HashmapSortEnt *pA,HashmapSortEnt *pB)` |
|       5 |   70 | `{` |
|   37162 |   71 | `	HashmapSortEnt sTmp = *pA;` |
|   37162 |   72 | `	*pA = *pB;` |
|   37162 |   73 | `	*pB = sTmp;` |
|   37162 |   74 | `}` |
|       - |   75 | `/*` |
|       - |   76 | ` * The fixed-size sorting networks. Each one is a decision tree, so both the` |
|       - |   77 | ` * number of comparisons and the pairs compared depend on the answers: this is` |
|       - |   78 | ` * where most of the count divergence against php lived.` |
|       - |   79 | ` */` |
|     441 |   80 | `static void HashmapSort2(HashmapSortEnt *a,HashmapSortEnt *b,HashmapSortCtx *pCtx)` |
|       5 |   81 | `{` |
|     446 |   82 | `	if( HashmapSortCmp(pCtx,a,b) > 0 ){` |
|     159 |   83 | `		HashmapSortSwap(a,b);` |
|      84 |   84 | `	}` |
|     446 |   85 | `}` |
|    3292 |   86 | `static void HashmapSort3(HashmapSortEnt *a,HashmapSortEnt *b,HashmapSortEnt *c,HashmapSortCtx *pCtx)` |
|       5 |   87 | `{` |
|    3297 |   88 | `	if( !(HashmapSortCmp(pCtx,a,b) > 0) ){` |
|    2643 |   89 | `		if( !(HashmapSortCmp(pCtx,b,c) > 0) ){` |
|    2008 |   90 | `			return;` |
|       - |   91 | `		}` |
|     640 |   92 | `		HashmapSortSwap(b,c);` |
|     640 |   93 | `		if( HashmapSortCmp(pCtx,a,b) > 0 ){` |
|     205 |   94 | `			HashmapSortSwap(a,b);` |
|      97 |   95 | `		}` |
|     640 |   96 | `		return;` |
|       - |   97 | `	}` |
|     659 |   98 | `	if( !(HashmapSortCmp(pCtx,c,b) > 0) ){` |
|     197 |   99 | `		HashmapSortSwap(a,c);` |
|     197 |  100 | `		return;` |
|       - |  101 | `	}` |
|     467 |  102 | `	HashmapSortSwap(a,b);` |
|     467 |  103 | `	if( HashmapSortCmp(pCtx,b,c) > 0 ){` |
|     311 |  104 | `		HashmapSortSwap(b,c);` |
|     131 |  105 | `	}` |
|    1631 |  106 | `}` |
|     743 |  107 | `static void HashmapSort4(HashmapSortEnt *a,HashmapSortEnt *b,HashmapSortEnt *c,HashmapSortEnt *d,HashmapSortCtx *pCtx)` |
|       5 |  108 | `{` |
|     748 |  109 | `	HashmapSort3(a,b,c,pCtx);` |
|     748 |  110 | `	if( HashmapSortCmp(pCtx,c,d) > 0 ){` |
|     456 |  111 | `		HashmapSortSwap(c,d);` |
|     456 |  112 | `		if( HashmapSortCmp(pCtx,b,c) > 0 ){` |
|     206 |  113 | `			HashmapSortSwap(b,c);` |
|     206 |  114 | `			if( HashmapSortCmp(pCtx,a,b) > 0 ){` |
|      81 |  115 | `				HashmapSortSwap(a,b);` |
|      37 |  116 | `			}` |
|      68 |  117 | `		}` |
|     211 |  118 | `	}` |
|     748 |  119 | `}` |
|     324 |  120 | `static void HashmapSort5(HashmapSortEnt *a,HashmapSortEnt *b,HashmapSortEnt *c,HashmapSortEnt *d,HashmapSortEnt *e,HashmapSortCtx *pCtx)` |
|       5 |  121 | `{` |
|     329 |  122 | `	HashmapSort4(a,b,c,d,pCtx);` |
|     329 |  123 | `	if( HashmapSortCmp(pCtx,d,e) > 0 ){` |
|     229 |  124 | `		HashmapSortSwap(d,e);` |
|     229 |  125 | `		if( HashmapSortCmp(pCtx,c,d) > 0 ){` |
|     142 |  126 | `			HashmapSortSwap(c,d);` |
|     142 |  127 | `			if( HashmapSortCmp(pCtx,b,c) > 0 ){` |
|      49 |  128 | `				HashmapSortSwap(b,c);` |
|      49 |  129 | `				if( HashmapSortCmp(pCtx,a,b) > 0 ){` |
|      15 |  130 | `					HashmapSortSwap(a,b);` |
|       6 |  131 | `				}` |
|      14 |  132 | `			}` |
|      63 |  133 | `		}` |
|     107 |  134 | `	}` |
|     329 |  135 | `}` |
|       - |  136 | `/*` |
|       - |  137 | ` * Insertion sort, php's zend_insert_sort: the networks up to five entries, then` |
|       - |  138 | ` * a linear scan back for the first six and a two-at-a-time scan back after that` |
|       - |  139 | ` * (which is why the sixth element onwards costs a different number of` |
|       - |  140 | ` * comparisons from the fifth).` |
|       - |  141 | ` */` |
|    4149 |  142 | `static void HashmapInsertSort(HashmapSortEnt *aEnt,sxu32 n,HashmapSortCtx *pCtx)` |
|       5 |  143 | `{` |
|    4154 |  144 | `	switch( n ){` |
|      10 |  145 | `		case 0:` |
|       - |  146 | `		case 1:` |
|      19 |  147 | `			break;` |
|     222 |  148 | `		case 2:` |
|     446 |  149 | `			HashmapSort2(aEnt,&aEnt[1],pCtx);` |
|     446 |  150 | `			break;` |
|     500 |  151 | `		case 3:` |
|     999 |  152 | `			HashmapSort3(aEnt,&aEnt[1],&aEnt[2],pCtx);` |
|     999 |  153 | `			break;` |
|     215 |  154 | `		case 4:` |
|     424 |  155 | `			HashmapSort4(aEnt,&aEnt[1],&aEnt[2],&aEnt[3],pCtx);` |
|     424 |  156 | `			break;` |
|     162 |  157 | `		case 5:` |
|     321 |  158 | `			HashmapSort5(aEnt,&aEnt[1],&aEnt[2],&aEnt[3],&aEnt[4],pCtx);` |
|     321 |  159 | `			break;` |
|     979 |  160 | `		default: {` |
|       - |  161 | `			HashmapSortEnt *i,*j,*k;` |
|    1970 |  162 | `			HashmapSortEnt *start = aEnt;` |
|    1970 |  163 | `			HashmapSortEnt *end = &aEnt[n];` |
|    1970 |  164 | `			HashmapSortEnt *sentry = &aEnt[6]; /* n >= 6 here, so this is in range */` |
|   11795 |  165 | `			for( i = start + 1 ; i < sentry ; i++ ){` |
|    9830 |  166 | `				j = i - 1;` |
|    9830 |  167 | `				if( !(HashmapSortCmp(pCtx,j,i) > 0) ){` |
|    6431 |  168 | `					continue;` |
|       - |  169 | `				}` |
|    6676 |  170 | `				while( j != start ){` |
|    5703 |  171 | `					j--;` |
|    5703 |  172 | `					if( !(HashmapSortCmp(pCtx,j,i) > 0) ){` |
|    2431 |  173 | `						j++;` |
|    2431 |  174 | `						break;` |
|       - |  175 | `					}` |
|       5 |  176 | `				}` |
|   10075 |  177 | `				for( k = i ; k > j ; k-- ){` |
|    6676 |  178 | `					HashmapSortSwap(k,k - 1);` |
|    3200 |  179 | `				}` |
|    1623 |  180 | `			}` |
|   11029 |  181 | `			for( i = sentry ; i < end ; i++ ){` |
|    9064 |  182 | `				j = i - 1;` |
|    9064 |  183 | `				if( !(HashmapSortCmp(pCtx,j,i) > 0) ){` |
|    5333 |  184 | `					continue;` |
|       - |  185 | `				}` |
|       - |  186 | `				/* j starts at i-3 >= start+3 and steps back by two, so the two` |
|       - |  187 | `				 * guards below are what keeps it from walking off the front. */` |
|    4525 |  188 | `				for( ;; ){` |
|    9058 |  189 | `					j -= 2;` |
|    9058 |  190 | `					if( !(HashmapSortCmp(pCtx,j,i) > 0) ){` |
|    3196 |  191 | `						j++;` |
|    3196 |  192 | `						if( !(HashmapSortCmp(pCtx,j,i) > 0) ){` |
|    1658 |  193 | `							j++;` |
|     844 |  194 | `						}` |
|    3196 |  195 | `						break;` |
|       - |  196 | `					}` |
|    5867 |  197 | `					if( j == start ){` |
|     152 |  198 | `						break;` |
|       - |  199 | `					}` |
|    5720 |  200 | `					if( j == start + 1 ){` |
|     398 |  201 | `						j--;` |
|     398 |  202 | `						if( HashmapSortCmp(pCtx,i,j) > 0 ){` |
|     213 |  203 | `							j++;` |
|     100 |  204 | `						}` |
|     398 |  205 | `						break;` |
|       - |  206 | `					}` |
|       5 |  207 | `				}` |
|   20914 |  208 | `				for( k = i ; k > j ; k-- ){` |
|   17183 |  209 | `					HashmapSortSwap(k,k - 1);` |
|    8564 |  210 | `				}` |
|    1870 |  211 | `			}` |
|    1965 |  212 | `			break;` |
|       - |  213 | `		}` |
|       - |  214 | `	}` |
|    4154 |  215 | `}` |
|       - |  216 | `/*` |
|       - |  217 | ` * php's zend_sort: insertion sort at sixteen entries or fewer, otherwise a` |
|       - |  218 | ` * median-of-three (median-of-five past 1024 entries) quicksort that recurses` |
|       - |  219 | ` * into the SMALLER partition and loops on the larger, which bounds the` |
|       - |  220 | ` * recursion at log2(n) frames.` |
|       - |  221 | ` *` |
|       - |  222 | `` * The partition loop advances i and j only through an `== ` guard against each`` |
|       - |  223 | ` * other, never through the comparator's answer, so an inconsistent comparator` |
|       - |  224 | ` * -- NAN's, or a user callback that answers at random -- can produce a garbage` |
|       - |  225 | ` * ORDER but can never walk off either end.` |
|       - |  226 | ` */` |
|    4149 |  227 | `static void HashmapZendSort(HashmapSortEnt *aEnt,sxu32 n,HashmapSortCtx *pCtx)` |
|       5 |  228 | `{` |
|    2835 |  229 | `	for(;;){` |
|    5717 |  230 | `		if( n <= 16 ){` |
|    4154 |  231 | `			HashmapInsertSort(aEnt,n,pCtx);` |
|    4154 |  232 | `			return;` |
|     ! 0 |  233 | `		}else{` |
|       - |  234 | `			HashmapSortEnt *i,*j;` |
|    1568 |  235 | `			HashmapSortEnt *start = aEnt;` |
|    1568 |  236 | `			HashmapSortEnt *end = &aEnt[n];` |
|    1568 |  237 | `			sxu32 offset = n >> 1;` |
|    1568 |  238 | `			HashmapSortEnt *pivot = &start[offset];` |
|    1568 |  239 | `			if( n >> 10 ){` |
|      13 |  240 | `				sxu32 delta = offset >> 1;` |
|      13 |  241 | `				HashmapSort5(start,&start[delta],pivot,&pivot[delta],end - 1,pCtx);` |
|       9 |  242 | `			}else{` |
|    1560 |  243 | `				HashmapSort3(start,pivot,end - 1,pCtx);` |
|       - |  244 | `			}` |
|    1568 |  245 | `			HashmapSortSwap(start + 1,pivot);` |
|    1568 |  246 | `			pivot = start + 1;` |
|    1568 |  247 | `			i = pivot + 1;` |
|    1568 |  248 | `			j = end - 1;` |
|    4248 |  249 | `			for(;;){` |
|   56940 |  250 | `				while( HashmapSortCmp(pCtx,pivot,i) > 0 ){` |
|   48630 |  251 | `					i++;` |
|   48630 |  252 | `					if( i == j ){` |
|     211 |  253 | `						goto done;` |
|       - |  254 | `					}` |
|       5 |  255 | `				}` |
|    8315 |  256 | `				j--;` |
|    8315 |  257 | `				if( j == i ){` |
|     140 |  258 | `					goto done;` |
|       - |  259 | `				}` |
|   56613 |  260 | `				while( HashmapSortCmp(pCtx,j,pivot) > 0 ){` |
|   49527 |  261 | `					j--;` |
|   49527 |  262 | `					if( j == i ){` |
|    1094 |  263 | `						goto done;` |
|       - |  264 | `					}` |
|       5 |  265 | `				}` |
|    7091 |  266 | `				HashmapSortSwap(i,j);` |
|    7091 |  267 | `				i++;` |
|    7091 |  268 | `				if( i == j ){` |
|     138 |  269 | `					goto done;` |
|       - |  270 | `				}` |
|       5 |  271 | `			}` |
|     789 |  272 | `done:` |
|    1568 |  273 | `			HashmapSortSwap(pivot,i - 1);` |
|    1568 |  274 | `			if( (i - 1) - start < end - i ){` |
|     379 |  275 | `				HashmapZendSort(start,(sxu32)(i - start) - 1,pCtx);` |
|     379 |  276 | `				aEnt = i;` |
|     379 |  277 | `				n = (sxu32)(end - i);` |
|     192 |  278 | `			}else{` |
|    1194 |  279 | `				HashmapZendSort(i,(sxu32)(end - i),pCtx);` |
|    1194 |  280 | `				n = (sxu32)(i - start) - 1;` |
|       - |  281 | `			}` |
|       - |  282 | `		}` |
|       5 |  283 | `	}` |
|       5 |  284 | `}` |
|       - |  285 | `/*` |
|       - |  286 | `** Inputs:` |
|       - |  287 | `**   Map:       Input hashmap` |
|       - |  288 | `**   cmp:       A comparison function.` |
|       - |  289 | `**` |
|       - |  290 | `** Side effects:` |
|       - |  291 | `**   The node list of the given hashmap is relinked in sorted order, ONCE, after` |
|       - |  292 | `**   the last comparison. SXERR_MEM leaves the array untouched and unsorted (the` |
|       - |  293 | `**   comparison order cannot be decided without the vector).` |
|       - |  294 | `*/` |
|    2460 |  295 | `PH7_PRIVATE sxi32 HashmapNodeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData)` |
|       5 |  296 | `{` |
|       - |  297 | `	HashmapSortEnt *aEnt;` |
|       - |  298 | `	HashmapSortCtx sCtx;` |
|       - |  299 | `	ph7_hashmap_node *pEntry;` |
|       - |  300 | `	sxu32 n,i;` |
|    2465 |  301 | `	n = pMap->nEntry;` |
|    2465 |  302 | `	if( n < 2 ){` |
|     ! 0 |  303 | `		pMap->pCur = pMap->pFirst;` |
|     ! 0 |  304 | `		return SXRET_OK;` |
|       - |  305 | `	}` |
|    2465 |  306 | `	aEnt = (HashmapSortEnt *)SyMemBackendAlloc(&pMap->pVm->sAllocator,n * sizeof(HashmapSortEnt));` |
|    2465 |  307 | `	if( aEnt == 0 ){` |
|     ! 0 |  308 | `		return SXERR_MEM;` |
|       - |  309 | `	}` |
|       - |  310 | `	/* Collect the nodes in iteration order (pPrev is the FORWARD link here),` |
|       - |  311 | `	 * stamping each with its position: that stamp IS the sort's stability. */` |
|    2465 |  312 | `	i = 0;` |
|   31689 |  313 | `	for( pEntry = pMap->pFirst ; pEntry && i < n ; pEntry = pEntry->pPrev ){` |
|   29229 |  314 | `		aEnt[i].pNode = pEntry;` |
|   29229 |  315 | `		aEnt[i].nOrd = i;` |
|   29229 |  316 | `		i++;` |
|   14583 |  317 | `	}` |
|    2465 |  318 | `	n = i; /* Defensive: honour the list, not the counter, if they disagree */` |
|    2465 |  319 | `	if( n < 2 ){` |
|       - |  320 | `		/* An empty or single-node list has no order to decide, and the relink` |
|       - |  321 | `		 * below indexes the vector unconditionally. */` |
|     ! 0 |  322 | `		SyMemBackendFree(&pMap->pVm->sAllocator,(void *)aEnt);` |
|     ! 0 |  323 | `		pMap->pCur = pMap->pFirst;` |
|     ! 0 |  324 | `		return SXRET_OK;` |
|       - |  325 | `	}` |
|    2465 |  326 | `	sCtx.xCmp = xCmp;` |
|    2465 |  327 | `	sCtx.pCmpData = pCmpData;` |
|    2465 |  328 | `	sCtx.bStable = 1;` |
|    2465 |  329 | `	HashmapZendSort(aEnt,n,&sCtx);` |
|       - |  330 | `	/* Relink the map's list from the decided order */` |
|   31689 |  331 | `	for( i = 0 ; i < n ; i++ ){` |
|   29229 |  332 | `		aEnt[i].pNode->pPrev = ( i + 1 < n ) ? aEnt[i+1].pNode : 0;` |
|   29229 |  333 | `		aEnt[i].pNode->pNext = ( i > 0 ) ? aEnt[i-1].pNode : 0;` |
|   14583 |  334 | `	}` |
|    2465 |  335 | `	pMap->pFirst = aEnt[0].pNode;` |
|    2465 |  336 | `	pMap->pLast = aEnt[n-1].pNode;` |
|       - |  337 | `	/* php's sorts rewind the array's internal pointer */` |
|    2465 |  338 | `	pMap->pCur = pMap->pFirst;` |
|    2465 |  339 | `	SyMemBackendFree(&pMap->pVm->sAllocator,(void *)aEnt);` |
|    2465 |  340 | `	return SXRET_OK;` |
|    1229 |  341 | `}` |
|       - |  342 | `/*` |
|       - |  343 | ` * Order a caller's OWN vector of entries, leaving every map alone.` |
|       - |  344 | ` *` |
|       - |  345 | ` * php's diff/intersect helpers sort a private list of each argument array and` |
|       - |  346 | ` * merge the sorted lists; the lists are theirs, so nothing is relinked, and they` |
|       - |  347 | ` * pass the UNSTABLE comparators -- equal entries end up wherever the quicksort` |
|       - |  348 | ` * left them and nothing downstream asks which. Each entry still carries its` |
|       - |  349 | ` * position, because the caller needs it to put its answer back in the source` |
|       - |  350 | ` * array's order.` |
|       - |  351 | ` */` |
|     218 |  352 | `PH7_PRIVATE void PH7_HashmapSortEntVector(HashmapSortEnt *aEnt,sxu32 n,ProcNodeCmp xCmp,void *pCmpData)` |
|       5 |  353 | `{` |
|       - |  354 | `	HashmapSortCtx sCtx;` |
|     223 |  355 | `	if( n < 2 ){` |
|      97 |  356 | `		return;` |
|       - |  357 | `	}` |
|     130 |  358 | `	sCtx.xCmp = xCmp;` |
|     130 |  359 | `	sCtx.pCmpData = pCmpData;` |
|     130 |  360 | `	sCtx.bStable = 0;` |
|     130 |  361 | `	HashmapZendSort(aEnt,n,&sCtx);` |
|     114 |  362 | `}` |
|       - |  363 | `/*` |
|       - |  364 | ` * A sort LENDS the map to itself for the duration.` |
|       - |  365 | ` *` |
|       - |  366 | ` * php sorts a private copy, so a write the comparator makes to the array being` |
|       - |  367 | ``  * sorted is discarded: `usort($a, function($x,$y) use (&$a){ $a[]=9; ... })` `` |
|       - |  368 | ` * answers the sorted PRE-CALL array in php, with no 9 in it, and an` |
|       - |  369 | `` * `unset($a[0])` in there changes nothing either. PHL sorts the LIVE map --`` |
|       - |  370 | ` * which is what keeps a comparator's reads php-exact (see HashmapNodeSort) --` |
|       - |  371 | ` * so it lends the map one extra reference first: a write through the variable` |
|       - |  372 | ` * then copy-on-write separates a map of its own and the sort's nodes are never` |
|       - |  373 | ` * relinked, or freed, underneath the vector it is deciding an order over.` |
|       - |  374 | ` *` |
|       - |  375 | ` * Taking the loan back installs the sorted map over whatever the comparator left` |
|       - |  376 | `` * behind, which is php's `ZVAL_ARR(array, arr)`. It also has to happen before the`` |
|       - |  377 | ` * driver rehashes: nothing else may hold the map by then, so the plain unref` |
|       - |  378 | ` * would free it under the driver's feet.` |
|       - |  379 | ` */` |
|    4868 |  380 | `static void HashmapSortInstall(ph7_value *pVal,ph7_hashmap *pMap)` |
|       5 |  381 | `{` |
|    4873 |  382 | `	if( (pVal->iFlags & MEMOBJ_HASHMAP) && (ph7_hashmap *)pVal->x.pOther == pMap ){` |
|    4863 |  383 | `		return; /* Untouched: the common case, and no work at all */` |
|       - |  384 | `	}` |
|      11 |  385 | `	PH7_MemObjRelease(pVal);` |
|      11 |  386 | `	MemObjSetType(pVal,MEMOBJ_HASHMAP);` |
|      11 |  387 | `	pVal->x.pOther = pMap;` |
|      11 |  388 | `	pMap->iRef++;` |
|    2427 |  389 | `}` |
|    2460 |  390 | `static void HashmapSortDrive(` |
|       - |  391 | `	ph7_vm *pVm,` |
|       - |  392 | `	ph7_value *pArray,        /* The by-reference array argument */` |
|       - |  393 | `	ph7_hashmap *pMap,        /* Its map, already COW-separated by the driver */` |
|       - |  394 | `	ProcNodeCmp xCmp,void *pCmpData` |
|       - |  395 | `	)` |
|       5 |  396 | `{` |
|    2465 |  397 | `	ph7_value *pBacking = 0;` |
|    2465 |  398 | `	pMap->iRef++;` |
|    2465 |  399 | `	HashmapNodeSort(pMap,xCmp,pCmpData);` |
|    2465 |  400 | `	if( pArray->nIdx != SXU32_HIGH ){` |
|       - |  401 | `		/* The argument may be a stack copy of the caller's variable, and a` |
|       - |  402 | `		 * comparator's write lands on the variable: install into both. */` |
|    2443 |  403 | `		pBacking = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pArray->nIdx);` |
|    1213 |  404 | `	}` |
|    2465 |  405 | `	if( pBacking && pBacking != pArray ){` |
|    2413 |  406 | `		HashmapSortInstall(pBacking,pMap);` |
|    1198 |  407 | `	}` |
|    2465 |  408 | `	HashmapSortInstall(pArray,pMap);` |
|    2465 |  409 | `	PH7_HashmapUnref(pMap);` |
|    2465 |  410 | `}` |
|       - |  411 | `/*` |
|       - |  412 | ` * Coerce one operand of a STRING-flag comparison (SORT_STRING and friends), the` |
|       - |  413 | ` * way php's zval_get_string() does. An ARRAY warns "Array to string conversion"` |
|       - |  414 | ` * and renders as "Array"; an object with no __toString() raises php's catchable` |
|       - |  415 | ` * "could not be converted to string" Error and renders as the EMPTY string --` |
|       - |  416 | ` * which is why php's array comes out fully SORTED after the throw, with the` |
|       - |  417 | ` * object first. PHL used to render it as the literal "Object" and sort on that,` |
|       - |  418 | ` * silently.` |
|       - |  419 | ` *` |
|       - |  420 | ` * A comparator has no status channel, so the Error is raised once per sort and` |
|       - |  421 | ` * flagged on the VM through iCmpCallbackExc -- the rail a throwing user callback` |
|       - |  422 | ` * already uses; every flag-sort driver clears the flag before its sort and` |
|       - |  423 | ` * answers PH7_EXCEPTION after it (HashmapFlagSortStatus), and array_unique() does` |
|       - |  424 | ` * the same around its walk. The flag is also what keeps the second and later` |
|       - |  425 | ` * comparisons from raising the same Error again.` |
|       - |  426 | ` */` |
|     572 |  427 | `static void HashmapFlagStringify(ph7_value *pVal)` |
|       4 |  428 | `{` |
|     576 |  429 | `	ph7_vm *pVm = pVal->pVm;` |
|     576 |  430 | `	sxi32 rc = PH7_EXCEPTION;` |
|     572 |  431 | `	if( (pVal->iFlags & MEMOBJ_OBJ) && pVm` |
|     168 |  432 | `	 && (pVm->iCmpCallbackExc != 0 \|\| PH7_CALLBACK_UNWOUND(pVm->nBoundaryRc)) ){` |
|       - |  433 | `		/* This sort already coerced an object once and it did not succeed: php` |
|       - |  434 | `		 * raises the refusal Error once, and it enters no PHP function at all` |
|       - |  435 | `		 * while an exception is pending (zend_call_function bails on` |
|       - |  436 | `		 * EG(exception)), so neither arm runs again -- the operand just renders` |
|       - |  437 | `		 * as the empty string a refused cast gives. The raise-once guard used to` |
|       - |  438 | `		 * cover only the NOT-STRINGABLE arm, so a throwing __toString() body went` |
|       - |  439 | `		 * round again on the next pair; uncaught, it reported the fatal once per` |
|       - |  440 | `		 * comparison, and after a catch had run in place the second throw was` |
|       - |  441 | `		 * uncaught and killed a script php merely says "caught" in.` |
|       - |  442 | `		 * OBJECTS only: blanking an int or a string here would re-order the rest` |
|       - |  443 | `		 * of the array. */` |
|      24 |  444 | `		PH7_MemObjRelease(pVal);` |
|      24 |  445 | `		MemObjSetType(pVal,MEMOBJ_STRING);` |
|      24 |  446 | `		return;` |
|       - |  447 | `	}` |
|     554 |  448 | `	if( !PH7_MemObjIsNotStringable(pVal) ){` |
|     516 |  449 | `		rc = PH7_MemObjToStringUV(pVal);` |
|     516 |  450 | `		if( rc == SXRET_OK ){` |
|     508 |  451 | `			return;` |
|       - |  452 | `		}` |
|       - |  453 | `		/* A __toString() that THREW. Same shape as a refused cast from here on. */` |
|       5 |  454 | `	}else{` |
|      40 |  455 | `		rc = PH7_MemObjToStringUV(pVal); /* raises php's Error */` |
|       - |  456 | `	}` |
|      48 |  457 | `	if( pVm ){` |
|       - |  458 | `		/* Latch the STATUS the coercion answered, so an UNCAUGHT throw (or an` |
|       - |  459 | `		 * exit()) out of __toString() leaves the sort with PH7_ABORT rather than` |
|       - |  460 | `		 * a downgraded PH7_EXCEPTION. */` |
|      48 |  461 | `		pVm->iCmpCallbackExc = PH7_CALLBACK_UNWOUND(rc) ? rc : PH7_EXCEPTION;` |
|      23 |  462 | `	}` |
|      48 |  463 | `	PH7_MemObjRelease(pVal);` |
|      48 |  464 | `	MemObjSetType(pVal,MEMOBJ_STRING);` |
|     288 |  465 | `}` |
|       - |  466 | `/*` |
|       - |  467 | ` * Node comparison callback.` |
|       - |  468 | ` * used-by: [sort(),asort(),...]` |
|       - |  469 | ` */` |
|       - |  470 | `/*` |
|       - |  471 | ` * Compare two scalar values under an EXPLICIT php sort base type (never 0 —` |
|       - |  472 | ` * SORT_REGULAR is handled by the callers, which differ for keys vs values):` |
|       - |  473 | ` *   SORT_NUMERIC 1 · SORT_STRING 2 · SORT_LOCALE_STRING 5 · SORT_NATURAL 6,` |
|       - |  474 | ` * with bFold applying SORT_FLAG_CASE. PHL has no locale tables, so` |
|       - |  475 | ` * SORT_LOCALE_STRING behaves like SORT_STRING. Mutates both operands (numeric or` |
|       - |  476 | ` * string cast); the caller owns and releases them.` |
|       - |  477 | ` */` |
| 1484028 |  478 | `static sxi32 HashmapScalarFlagCmp(ph7_value *pA,ph7_value *pB,int base,int bFold)` |
|       5 |  479 | `{` |
|       - |  480 | `	sxi32 rc;` |
| 1484033 |  481 | `	if( base == 1 ){` |
|       - |  482 | `		/* SORT_NUMERIC compares as DOUBLES. php's numeric_compare_function is` |
|       - |  483 | ``		 * `zval_get_double(a)` vs `zval_get_double(b)` under ZEND_THREEWAY_COMPARE`` |
|       - |  484 | `		 * — not a value comparison over whatever type each operand happens to` |
|       - |  485 | `		 * settle on. Two consequences PHL got wrong by folding to a NUMBER and` |
|       - |  486 | `		 * calling the ordinary comparator: the diagnostic named the wrong type` |
|       - |  487 | `		 * (an object warned "could not be converted to int" where php says` |
|       - |  488 | `		 * "to float"), and integers above 2^53 were ordered EXACTLY where php's` |
|       - |  489 | ``		 * doubles tie — `sort([PHP_INT_MAX, PHP_INT_MAX-1], SORT_NUMERIC)` left`` |
|       - |  490 | `		 * php's stable sort's original order and PHL re-ordered them. Matching php` |
|       - |  491 | `		 * means adopting its precision loss, which is what parity is (the scope policy).` |
|       - |  492 | `		 * The == / < shape is ZEND_THREEWAY_COMPARE's, so NaN — equal to nothing,` |
|       - |  493 | `		 * less than nothing — answers 1 in both engines. */` |
|       - |  494 | `		ph7_real rA,rB;` |
|     243 |  495 | `		PH7_MemObjToReal(pA);` |
|     243 |  496 | `		PH7_MemObjToReal(pB);` |
|     243 |  497 | `		rA = pA->rVal;` |
|     243 |  498 | `		rB = pB->rVal;` |
|     243 |  499 | `		rc = (rA == rB) ? 0 : ((rA < rB) ? -1 : 1);` |
|     123 |  500 | `	}else{` |
|       - |  501 | `		/* SORT_STRING (2) / SORT_LOCALE_STRING (5) / SORT_NATURAL (6) */` |
|       - |  502 | `		const char *zA,*zB;` |
|       - |  503 | `		sxu32 nA,nB,nMin,i;` |
| 1483793 |  504 | `		if( (pA->iFlags & MEMOBJ_STRING) == 0 ){ HashmapFlagStringify(pA); }` |
| 1483793 |  505 | `		if( (pB->iFlags & MEMOBJ_STRING) == 0 ){ HashmapFlagStringify(pB); }` |
| 1483793 |  506 | `		zA = (const char *)SyBlobData(&pA->sBlob);` |
| 1483793 |  507 | `		zB = (const char *)SyBlobData(&pB->sBlob);` |
| 1483793 |  508 | `		nA = SyBlobLength(&pA->sBlob);` |
| 1483793 |  509 | `		nB = SyBlobLength(&pB->sBlob);` |
| 1483793 |  510 | `		if( base == 6 ){` |
|     191 |  511 | `			rc = PH7_StrNatCmp(zA,(int)nA,zB,(int)nB,bFold);` |
|      97 |  512 | `		}else{` |
|       - |  513 | `			/* Lexicographic comparison (binary-safe), case-folded on request. */` |
| 1483605 |  514 | `			nMin = nA < nB ? nA : nB;` |
| 1483605 |  515 | `			rc = 0;` |
| 3009541 |  516 | `			for( i = 0 ; i < nMin ; ++i ){` |
| 3006867 |  517 | `				int ca = (unsigned char)zA[i];` |
| 3006867 |  518 | `				int cb = (unsigned char)zB[i];` |
| 3006867 |  519 | `				if( bFold ){ ca = SyToLower(ca); cb = SyToLower(cb); }` |
| 3006867 |  520 | `				if( ca != cb ){ rc = ca < cb ? -1 : 1; break; }` |
|  770983 |  521 | `			}` |
| 1483605 |  522 | `			if( rc == 0 ){` |
|    2679 |  523 | `				if( nA < nB ) rc = -1;` |
|     800 |  524 | `				else if( nA > nB ) rc = 1;` |
|    1345 |  525 | `			}` |
|       - |  526 | `		}` |
|       - |  527 | `	}` |
| 1484033 |  528 | `	return rc;` |
|       5 |  529 | `}` |
|       - |  530 | `/*` |
|       - |  531 | ` * Are two live values equal under an array_unique() sort_flags? A non-mutating` |
|       - |  532 | ` * wrapper (works on private copies): base 0 = SORT_REGULAR loose comparison,` |
|       - |  533 | ` * explicit flags route through HashmapScalarFlagCmp. Used by array_unique.` |
|       - |  534 | ` */` |
| 1413632 |  535 | `PH7_PRIVATE int HashmapValueFlagEqual(ph7_vm *pVm,ph7_value *pA,ph7_value *pB,int base,int bFold)` |
|       5 |  536 | `{` |
|       - |  537 | `	ph7_value sA,sB;` |
|       - |  538 | `	sxi32 rc;` |
| 1413637 |  539 | `	PH7_MemObjInit(pVm,&sA);` |
| 1413637 |  540 | `	PH7_MemObjInit(pVm,&sB);` |
| 1413637 |  541 | `	PH7_MemObjStore(pA,&sA);` |
| 1413637 |  542 | `	PH7_MemObjStore(pB,&sB);` |
| 1413637 |  543 | `	if( base == 0 ){` |
|      18 |  544 | `		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0); /* SORT_REGULAR: loose comparison */` |
|      10 |  545 | `	}else{` |
| 1413621 |  546 | `		rc = HashmapScalarFlagCmp(&sA,&sB,base,bFold);` |
|       - |  547 | `	}` |
| 1413637 |  548 | `	PH7_MemObjRelease(&sA);` |
| 1413637 |  549 | `	PH7_MemObjRelease(&sB);` |
| 1413637 |  550 | `	return rc == 0;` |
|       5 |  551 | `}` |
|       - |  552 | `/*` |
|       - |  553 | ` * Compare two node VALUES under php's sort_flags (base 0 = SORT_REGULAR uses the` |
|       - |  554 | ` * standard value comparison; explicit flags route through HashmapScalarFlagCmp).` |
|       - |  555 | ` */` |
|   70382 |  556 | `static sxi32 HashmapFlagValueCmp(ph7_hashmap_node *pA,ph7_hashmap_node *pB,sxi32 iFlags)` |
|       5 |  557 | `{` |
|       - |  558 | `	ph7_value sA,sB;` |
|   70387 |  559 | `	int base = iFlags & ~8;      /* strip SORT_FLAG_CASE */` |
|   70387 |  560 | `	int bFold = (iFlags & 8) != 0;` |
|       - |  561 | `	sxi32 rc;` |
|   70387 |  562 | `	if( base == 0 ){` |
|       - |  563 | `		/* SORT_REGULAR */` |
|      93 |  564 | `		return HashmapNodeCmp(pA,pB,FALSE,0);` |
|       - |  565 | `	}` |
|   70297 |  566 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|   70297 |  567 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|   70297 |  568 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|   70297 |  569 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|   70297 |  570 | `	rc = HashmapScalarFlagCmp(&sA,&sB,base,bFold);` |
|   70297 |  571 | `	PH7_MemObjRelease(&sA);` |
|   70297 |  572 | `	PH7_MemObjRelease(&sB);` |
|   70297 |  573 | `	return rc;` |
|   34485 |  574 | `}` |
|       - |  575 | `/*` |
|       - |  576 | ` * A sort comparison can run USER code with no way to report what it did: an` |
|       - |  577 | `` * object operand's `__toString()` is reached by SORT_REGULAR's value comparison`` |
|       - |  578 | ` * as well as by the string flags' coercion, and that body can throw or exit().` |
|       - |  579 | ` * PH7_MemObjCmp only ever answers an ORDERING, so the status is read off the VM` |
|       - |  580 | ` * instead — every C->PHP dispatch parks an unwind in nBoundaryRc (VmBoundaryPark)` |
|       - |  581 | ` * and nothing inside a builtin consumes it, so it is still there when the` |
|       - |  582 | ` * comparison returns.` |
|       - |  583 | ` *` |
|       - |  584 | ` * Two things follow, and both were missing: the sort must STAND DOWN (a` |
|       - |  585 | `` * throwing `__toString()` ran again on the next pair -- and since the enclosing`` |
|       - |  586 | ` * catch had already run in place, the second throw was UNCAUGHT and killed a` |
|       - |  587 | ` * script php merely reports "caught" in), and the driver must answer with that` |
|       - |  588 | `` * status rather than `true`.`` |
|       - |  589 | ` *` |
|       - |  590 | ` * Deliberately NOT triggered by a not-stringable object's own coercion Error:` |
|       - |  591 | ` * that one parks nothing and is flagged by HashmapFlagStringify, and php goes on` |
|       - |  592 | ` * comparing after it (its array comes out fully sorted, the object first).` |
|       - |  593 | ` */` |
|  158929 |  594 | `static void HashmapCmpLatch(ph7_vm *pVm)` |
|       5 |  595 | `{` |
|  158929 |  596 | `	if( PH7_CALLBACK_UNWOUND(pVm->nBoundaryRc)` |
|   78839 |  597 | `	 && (pVm->iCmpCallbackExc == 0 \|\| pVm->nBoundaryRc == PH7_ABORT) ){` |
|     121 |  598 | `		pVm->iCmpCallbackExc = pVm->nBoundaryRc;` |
|      75 |  599 | `	}` |
|  158904 |  600 | `}` |
|  157897 |  601 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       5 |  602 | `{` |
|  157902 |  603 | `	ph7_vm *pVm = pA->pMap->pVm;` |
|       - |  604 | `	sxi32 rc;` |
|  157902 |  605 | `	if( pCmpData == 0 ){` |
|       - |  606 | `		/* SORT_REGULAR fast path */` |
|   88014 |  607 | `		rc = HashmapNodeCmp(pA,pB,FALSE,0);` |
|   44011 |  608 | `	}else{` |
|   69893 |  609 | `		rc = HashmapFlagValueCmp(pA,pB,SX_PTR_TO_INT(pCmpData));` |
|       - |  610 | `	}` |
|  157902 |  611 | `	HashmapCmpLatch(pVm);` |
|  157902 |  612 | `	return rc;` |
|       5 |  613 | `}` |
|       - |  614 | `/*` |
|       - |  615 | ` * Materialise a node's KEY as a scalar ph7_value (int key -> integer, string key` |
|       - |  616 | ` * -> string) for a flag-aware key comparison.` |
|       - |  617 | ` */` |
|     900 |  618 | `static void HashmapNodeKeyToValue(ph7_hashmap_node *pNode,ph7_value *pOut)` |
|       4 |  619 | `{` |
|     904 |  620 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|     126 |  621 | `		PH7_MemObjInitFromInt(pNode->pMap->pVm,pOut,pNode->xKey.iKey);` |
|      64 |  622 | `	}else{` |
|     780 |  623 | `		PH7_MemObjInitFromString(pNode->pMap->pVm,pOut,0);` |
|    1168 |  624 | `		PH7_MemObjStringAppend(pOut,(const char *)SyBlobData(&pNode->xKey.sKey),` |
|     388 |  625 | `			SyBlobLength(&pNode->xKey.sKey));` |
|       - |  626 | `	}` |
|     904 |  627 | `}` |
|       - |  628 | `/*` |
|       - |  629 | ` * Shared key comparison for ksort()/krsort() under SORT_REGULAR: php compares` |
|       - |  630 | ` * two array KEYS exactly the way it compares two VALUES, so materialise them` |
|       - |  631 | `` * and hand them to the standard comparison — the same one sort()/`<=>` use.`` |
|       - |  632 | ` *` |
|       - |  633 | ` * The hand-rolled version this replaces got the mixed int/string case right but` |
|       - |  634 | ` * compared two STRING keys BYTEWISE, so two numeric strings sorted by their` |
|       - |  635 | ` * bytes: ksort(['10.0'=>1,'9.0'=>2]) answered ['10.0','9.0'] where php answers` |
|       - |  636 | ` * ['9.0','10.0'], ksort(['1e3'=>1,'20'=>2]) put 1e3 (1000) first, and keys that` |
|       - |  637 | ` * compare EQUAL ('1.0', '01', 1) lost php's stable order.` |
|       - |  638 | ` */` |
|     386 |  639 | `static sxi32 HashmapKeyNodeCmp(ph7_hashmap_node *pA,ph7_hashmap_node *pB)` |
|       2 |  640 | `{` |
|       - |  641 | `	ph7_value sA,sB;` |
|       - |  642 | `	sxi32 rc;` |
|     388 |  643 | `	if( pA->iType == HASHMAP_INT_NODE && pB->iType == HASHMAP_INT_NODE ){` |
|       - |  644 | `		/* Two integer keys: the common case, and no allocation needed */` |
|      57 |  645 | `		return pA->xKey.iKey < pB->xKey.iKey ? -1 : (pA->xKey.iKey > pB->xKey.iKey ? 1 : 0);` |
|       - |  646 | `	}` |
|     332 |  647 | `	HashmapNodeKeyToValue(pA,&sA);` |
|     332 |  648 | `	HashmapNodeKeyToValue(pB,&sB);` |
|     332 |  649 | `	rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|     332 |  650 | `	PH7_MemObjRelease(&sA);` |
|     332 |  651 | `	PH7_MemObjRelease(&sB);` |
|     332 |  652 | `	return rc;` |
|     195 |  653 | `}` |
|       - |  654 | `/*` |
|       - |  655 | ` * Compare two node KEYS under php's sort_flags. base 0 = SORT_REGULAR keeps the` |
|       - |  656 | ` * php-8 mixed int/string key semantics (HashmapKeyNodeCmp); explicit flags route` |
|       - |  657 | ` * the materialised keys through HashmapScalarFlagCmp.` |
|       - |  658 | ` */` |
|     120 |  659 | `static sxi32 HashmapFlagKeyCmp(ph7_hashmap_node *pA,ph7_hashmap_node *pB,sxi32 iFlags)` |
|       3 |  660 | `{` |
|       - |  661 | `	ph7_value sA,sB;` |
|     123 |  662 | `	int base = iFlags & ~8;      /* strip SORT_FLAG_CASE */` |
|     123 |  663 | `	int bFold = (iFlags & 8) != 0;` |
|       - |  664 | `	sxi32 rc;` |
|     123 |  665 | `	if( base == 0 ){` |
|     ! 0 |  666 | `		return HashmapKeyNodeCmp(pA,pB);` |
|       - |  667 | `	}` |
|     123 |  668 | `	HashmapNodeKeyToValue(pA,&sA);` |
|     123 |  669 | `	HashmapNodeKeyToValue(pB,&sB);` |
|     123 |  670 | `	rc = HashmapScalarFlagCmp(&sA,&sB,base,bFold);` |
|     123 |  671 | `	PH7_MemObjRelease(&sA);` |
|     123 |  672 | `	PH7_MemObjRelease(&sB);` |
|     123 |  673 | `	return rc;` |
|      63 |  674 | `}` |
|       - |  675 | `/*` |
|       - |  676 | ` * Node comparison callback: Compare nodes by keys only.` |
|       - |  677 | ` * used-by: [ksort()]` |
|       - |  678 | ` */` |
|     438 |  679 | `static sxi32 HashmapCmpCallback2(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       4 |  680 | `{` |
|     442 |  681 | `	ph7_vm *pVm = pA->pMap->pVm;` |
|       - |  682 | `	sxi32 rc;` |
|     442 |  683 | `	if( pCmpData == 0 ){` |
|     326 |  684 | `		rc = HashmapKeyNodeCmp(pA,pB);` |
|     164 |  685 | `	}else{` |
|     117 |  686 | `		rc = HashmapFlagKeyCmp(pA,pB,SX_PTR_TO_INT(pCmpData));` |
|       - |  687 | `	}` |
|     442 |  688 | `	HashmapCmpLatch(pVm);` |
|     442 |  689 | `	return rc;` |
|       4 |  690 | `}` |
|       - |  691 | `/*` |
|       - |  692 | ` * Node comparison callback.` |
|       - |  693 | ` * Used by: [rsort(),arsort()];` |
|       - |  694 | ` */` |
|     488 |  695 | `static sxi32 HashmapCmpCallback3(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       5 |  696 | `{` |
|     493 |  697 | `	ph7_vm *pVm = pA->pMap->pVm;` |
|       - |  698 | `	sxi32 rc;` |
|     493 |  699 | `	if( pCmpData == 0 ){` |
|       - |  700 | `		/* SORT_REGULAR fast path, reversed */` |
|     103 |  701 | `		rc = -HashmapNodeCmp(pA,pB,FALSE,0);` |
|      53 |  702 | `	}else{` |
|     392 |  703 | `		rc = -HashmapFlagValueCmp(pA,pB,SX_PTR_TO_INT(pCmpData));` |
|       - |  704 | `	}` |
|     493 |  705 | `	HashmapCmpLatch(pVm);` |
|     493 |  706 | `	return rc;` |
|       5 |  707 | `}` |
|       - |  708 | `/*` |
|       - |  709 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|       - |  710 | ` * used-by: [usort(),uasort()]` |
|       - |  711 | ` */` |
|    1880 |  712 | `static sxi32 HashmapCmpCallback4(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       5 |  713 | `{` |
|       - |  714 | `	ph7_value sResult,*pCallback;` |
|       - |  715 | `	ph7_value *pV1,*pV2;` |
|       - |  716 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|       - |  717 | `	sxi32 rc;` |
|       - |  718 | `	/* Point to the desired callback */` |
|    1885 |  719 | `	pCallback = (ph7_value *)pCmpData;` |
|    1885 |  720 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|       - |  721 | `		/* A previous comparison already raised: stop invoking the callback so` |
|       - |  722 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|      20 |  723 | `		return 0;` |
|       - |  724 | `	}` |
|       - |  725 | `	/* initialize the result value */` |
|    1867 |  726 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|       - |  727 | `	/* Extract nodes values */` |
|    1867 |  728 | `	pV1 = HashmapExtractNodeValue(pA);` |
|    1867 |  729 | `	pV2 = HashmapExtractNodeValue(pB);` |
|    1867 |  730 | `	apArg[0] = pV1;` |
|    1867 |  731 | `	apArg[1] = pV2;` |
|       - |  732 | `	/* Invoke the callback */` |
|    1867 |  733 | `	rc = PH7_VmCallCallbackByValue(pA->pMap->pVm,pCallback,2,apArg,&sResult,0);` |
|    1867 |  734 | `	if( PH7_CALLBACK_UNWOUND(rc) ){` |
|       - |  735 | `		/* The comparator did not RETURN: latch the STATUS so the sort driver` |
|       - |  736 | `		 * aborts and propagates exactly it (an UNCAUGHT throw is PH7_ABORT, and` |
|       - |  737 | `		 * testing only PH7_EXCEPTION left the sort running -- re-entering the` |
|       - |  738 | `		 * comparator, and the fatal report, for every remaining pair), and order` |
|       - |  739 | `		 * this pair arbitrarily for the rest of the run. */` |
|      35 |  740 | `		pA->pMap->pVm->iCmpCallbackExc = rc;` |
|      35 |  741 | `		rc = 0;` |
|    1851 |  742 | `	}else if( rc != SXRET_OK ){` |
|       - |  743 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|     ! 0 |  744 | `		rc = -1; /* Set a dummy result */` |
|     ! 0 |  745 | `	}else{` |
|       - |  746 | `		/* Extract callback result */` |
|    1835 |  747 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  748 | `			/* Perform an int cast */` |
|     ! 0 |  749 | `			PH7_MemObjToInteger(&sResult);` |
|     ! 0 |  750 | `		}` |
|    1835 |  751 | `		rc = (sxi32)sResult.x.iVal;` |
|       - |  752 | `	}` |
|    1867 |  753 | `	PH7_MemObjRelease(&sResult);` |
|       - |  754 | `	/* Callback result */` |
|    1867 |  755 | `	return rc;` |
|     920 |  756 | `}` |
|       - |  757 | `/*` |
|       - |  758 | ` * Node comparison callback: Compare nodes by keys only.` |
|       - |  759 | ` * used-by: [krsort()]` |
|       - |  760 | ` */` |
|      68 |  761 | `static sxi32 HashmapCmpCallback5(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       2 |  762 | `{` |
|      70 |  763 | `	if( pCmpData == 0 ){` |
|      63 |  764 | `		return -HashmapKeyNodeCmp(pA,pB); /* Reverse result */` |
|       - |  765 | `	}` |
|       8 |  766 | `	return -HashmapFlagKeyCmp(pA,pB,SX_PTR_TO_INT(pCmpData));` |
|      36 |  767 | `}` |
|       - |  768 | `/*` |
|       - |  769 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|       - |  770 | ` * used-by: [uksort()]` |
|       - |  771 | ` */` |
|      24 |  772 | `static sxi32 HashmapCmpCallback6(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       2 |  773 | `{` |
|       - |  774 | `	ph7_value sResult,*pCallback;` |
|       - |  775 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|       - |  776 | `	ph7_value sK1,sK2;` |
|       - |  777 | `	sxi32 rc;` |
|       - |  778 | `	/* Point to the desired callback */` |
|      26 |  779 | `	pCallback = (ph7_value *)pCmpData;` |
|      26 |  780 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|       - |  781 | `		/* A previous comparison already raised: stop invoking the callback so` |
|       - |  782 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|       5 |  783 | `		return 0;` |
|       - |  784 | `	}` |
|       - |  785 | `	/* initialize the result value */` |
|      22 |  786 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|      22 |  787 | `	PH7_MemObjInit(pA->pMap->pVm,&sK1);` |
|      22 |  788 | `	PH7_MemObjInit(pA->pMap->pVm,&sK2);` |
|       - |  789 | `	/* Extract nodes keys */` |
|      22 |  790 | `	PH7_HashmapExtractNodeKey(pA,&sK1);` |
|      22 |  791 | `	PH7_HashmapExtractNodeKey(pB,&sK2);` |
|      22 |  792 | `	apArg[0] = &sK1;` |
|      22 |  793 | `	apArg[1] = &sK2;` |
|       - |  794 | `	/* Mark keys as constants */` |
|      22 |  795 | `	sK1.nIdx = SXU32_HIGH;` |
|      22 |  796 | `	sK2.nIdx = SXU32_HIGH;` |
|       - |  797 | `	/* Invoke the callback */` |
|      22 |  798 | `	rc = PH7_VmCallCallbackByValue(pA->pMap->pVm,pCallback,2,apArg,&sResult,0);` |
|      22 |  799 | `	if( PH7_CALLBACK_UNWOUND(rc) ){` |
|       - |  800 | `		/* The comparator did not RETURN: latch the STATUS so the sort driver` |
|       - |  801 | `		 * aborts and propagates exactly it (an UNCAUGHT throw is PH7_ABORT, and` |
|       - |  802 | `		 * testing only PH7_EXCEPTION left the sort running -- re-entering the` |
|       - |  803 | `		 * comparator, and the fatal report, for every remaining pair), and order` |
|       - |  804 | `		 * this pair arbitrarily for the rest of the run. */` |
|       3 |  805 | `		pA->pMap->pVm->iCmpCallbackExc = rc;` |
|       3 |  806 | `		rc = 0;` |
|      21 |  807 | `	}else if( rc != SXRET_OK ){` |
|       - |  808 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|     ! 0 |  809 | `		rc = -1; /* Set a dummy result */` |
|     ! 0 |  810 | `	}else{` |
|       - |  811 | `		/* Extract callback result */` |
|      20 |  812 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  813 | `			/* Perform an int cast */` |
|     ! 0 |  814 | `			PH7_MemObjToInteger(&sResult);` |
|     ! 0 |  815 | `		}` |
|      20 |  816 | `		rc = (sxi32)sResult.x.iVal;` |
|       - |  817 | `	}` |
|      22 |  818 | `	PH7_MemObjRelease(&sResult);` |
|      22 |  819 | `	PH7_MemObjRelease(&sK1);` |
|      22 |  820 | `	PH7_MemObjRelease(&sK2);` |
|       - |  821 | `	/* Callback result */` |
|      22 |  822 | `	return rc;` |
|      14 |  823 | `}` |
|       - |  824 | `/*` |
|       - |  825 | ` * Permute a hashmap's entries the way php's shuffle() does: Fisher-Yates over` |
|       - |  826 | ` * the buckets, drawing each index from the MT19937 generator through` |
|       - |  827 | ` * php_mt_rand_range(). The caller rehashes afterwards (php reindexes the array` |
|       - |  828 | ` * 0..n-1 and drops string keys).` |
|       - |  829 | ` *` |
|       - |  830 | ` * This used to be a sort with a coin-flip comparator, which is a permuting` |
|       - |  831 | ` * shuffle but not a UNIFORM one — the distribution a random comparator produces` |
|       - |  832 | ` * is skewed and depends on the sort's internals — and it consumed the generator` |
|       - |  833 | ` * in a different order, so a seeded run answered a different permutation from` |
|       - |  834 | ` * php's for every seed.` |
|       - |  835 | ` */` |
|       8 |  836 | `PH7_PRIVATE sxi32 PH7_HashmapShuffle(ph7_hashmap *pMap)` |
|       2 |  837 | `{` |
|       - |  838 | `	ph7_hashmap_node **apNode,*pNode;` |
|       - |  839 | `	sxu32 n,nLeft;` |
|       - |  840 | `	SySet aNode;` |
|      10 |  841 | `	if( pMap->nEntry < 2 ){` |
|     ! 0 |  842 | `		return SXRET_OK;` |
|       - |  843 | `	}` |
|      10 |  844 | `	SySetInit(&aNode,&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|      50 |  845 | `	for( pNode = pMap->pFirst ; pNode ; pNode = pNode->pPrev ){` |
|      42 |  846 | `		if( SySetPut(&aNode,(const void *)&pNode) != SXRET_OK ){` |
|     ! 0 |  847 | `			SySetRelease(&aNode);` |
|     ! 0 |  848 | `			return SXERR_MEM;` |
|       - |  849 | `		}` |
|      22 |  850 | `	}` |
|      10 |  851 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&aNode);` |
|      10 |  852 | `	n = SySetUsed(&aNode);` |
|       - |  853 | `	/* php walks DOWN from the last index, swapping with a draw in [0,n_left]. */` |
|      42 |  854 | `	for( nLeft = n - 1 ; nLeft > 0 ; --nLeft ){` |
|      34 |  855 | `		sxu32 nPick = (sxu32)PH7_VmMtRandRange(pMap->pVm,0,(sxi64)nLeft);` |
|      34 |  856 | `		if( nPick != nLeft ){` |
|      24 |  857 | `			ph7_hashmap_node *pTmp = apNode[nLeft];` |
|      24 |  858 | `			apNode[nLeft] = apNode[nPick];` |
|      24 |  859 | `			apNode[nPick] = pTmp;` |
|      11 |  860 | `		}` |
|      18 |  861 | `	}` |
|       - |  862 | `	/* Relink in the new order. pPrev is the forward link and pNext the back one` |
|       - |  863 | `	 * (the whole map is built that way); the rehash after this fixes pLast. */` |
|      50 |  864 | `	for( n = 0 ; n < SySetUsed(&aNode) ; ++n ){` |
|      42 |  865 | `		apNode[n]->pPrev = (n + 1 < SySetUsed(&aNode)) ? apNode[n+1] : 0;` |
|      42 |  866 | `		apNode[n]->pNext = (n > 0) ? apNode[n-1] : 0;` |
|      22 |  867 | `	}` |
|      10 |  868 | `	pMap->pFirst = apNode[0];` |
|      10 |  869 | `	pMap->pLast = apNode[SySetUsed(&aNode) - 1];` |
|      10 |  870 | `	pMap->pCur = pMap->pFirst;` |
|      10 |  871 | `	SySetRelease(&aNode);` |
|      10 |  872 | `	return SXRET_OK;` |
|       6 |  873 | `}` |
|       - |  874 | `/*` |
|       - |  875 | ` * Rehash all nodes keys after a sort have been applied.` |
|       - |  876 | ` * Used by [sort(),usort() and rsort()].` |
|       - |  877 | ` */` |
|    2320 |  878 | `PH7_PRIVATE void HashmapSortRehash(ph7_hashmap *pMap)` |
|       5 |  879 | `{` |
|       - |  880 | `	ph7_hashmap_node *p,*pLast;` |
|       - |  881 | `	sxu32 i;` |
|       - |  882 | `	/* php's sorts rewind the array's internal pointer. The reordering paths reset` |
|       - |  883 | `	 * it themselves, but a ONE-element array is reindexed without being reordered` |
|       - |  884 | ``	 * — and `$a = ['x'=>1]; next($a); sort($a);` then left current() past the end,`` |
|       - |  885 | `	 * answering false where php answers the element. */` |
|    2325 |  886 | `	pMap->pCur = pMap->pFirst;` |
|       - |  887 | `	/* Rehash all entries */` |
|    2325 |  888 | `	pLast = p = pMap->pFirst;` |
|    2325 |  889 | `	pMap->iNextIdx = 0;` |
|    2325 |  890 | `	pMap->bIntKeySeen = 0; /* Reset the automatic index */` |
|    2325 |  891 | `	i = 0;` |
|   15324 |  892 | `	for( ;; ){` |
|   30733 |  893 | `		if( i >= pMap->nEntry ){` |
|    2325 |  894 | `			pMap->pLast = pLast;` |
|    2325 |  895 | `			break;` |
|       - |  896 | `		}` |
|   28413 |  897 | `		if( p->iType == HASHMAP_BLOB_NODE ){` |
|       - |  898 | `			/* Do not maintain index association as requested by the PHP specification */` |
|      50 |  899 | `			SyBlobRelease(&p->xKey.sKey);` |
|       - |  900 | `			/* Change key type */` |
|      50 |  901 | `			p->iType = HASHMAP_INT_NODE;` |
|      24 |  902 | `		}` |
|   28413 |  903 | `		HashmapRehashIntNode(p);` |
|       - |  904 | `		/* Point to the next entry */` |
|   28413 |  905 | `		i++;` |
|   28413 |  906 | `		pLast = p;` |
|   28413 |  907 | `		p = p->pPrev; /* Reverse link */` |
|       5 |  908 | `	}` |
|    2325 |  909 | `}` |
|       - |  910 | `/*` |
|       - |  911 | ` * Array functions implementation.` |
|       - |  912 | ` * Status:` |
|       - |  913 | ` *  Stable.` |
|       - |  914 | ` */` |
|       - |  915 | `/*` |
|       - |  916 | ` * Reset / report the comparator-throw flag around a FLAG sort. The string sort` |
|       - |  917 | ` * flags coerce their operands user-visibly (HashmapScalarFlagCmp), and a` |
|       - |  918 | ` * not-stringable object raises php's Error there; the comparator can only flag` |
|       - |  919 | ` * it, so every flag-sort driver clears the flag before its sort and` |
|       - |  920 | ` * answers PH7_EXCEPTION after it. php's array is sorted after the throw too, so` |
|       - |  921 | ` * the rehash still runs.` |
|       - |  922 | ` */` |
|    2777 |  923 | `static sxi32 HashmapFlagSortStatus(ph7_context *pCtx)` |
|       5 |  924 | `{` |
|    2782 |  925 | `	if( pCtx->pVm->iCmpCallbackExc ){` |
|      80 |  926 | `		sxi32 rcExc = pCtx->pVm->iCmpCallbackExc;` |
|      80 |  927 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|      80 |  928 | `		pCtx->nThrowRc = rcExc;` |
|      80 |  929 | `		return rcExc;` |
|       - |  930 | `	}` |
|    2704 |  931 | `	return PH7_OK;` |
|    1391 |  932 | `}` |
|       - |  933 | `/*` |
|       - |  934 | ` * bool sort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - |  935 | ` * Sort an array.` |
|       - |  936 | ` * Parameters` |
|       - |  937 | ` *  $array` |
|       - |  938 | ` *   The input array.` |
|       - |  939 | ` * $sort_flags` |
|       - |  940 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - |  941 | ` *  Sorting type flags:` |
|       - |  942 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - |  943 | ` *   SORT_NUMERIC - compare items numerically` |
|       - |  944 | ` *   SORT_STRING - compare items as strings` |
|       - |  945 | ` * Return` |
|       - |  946 | ` *  TRUE on success or FALSE on failure.` |
|       - |  947 | ` *` |
|       - |  948 | ` */` |
|    2189 |  949 | `PH7_PRIVATE int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  950 | `{` |
|       - |  951 | `	ph7_hashmap *pMap;` |
|       - |  952 | `	/* Make sure we are dealing with a valid hashmap */` |
|    2194 |  953 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - |  954 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 |  955 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  956 | `		return PH7_OK;` |
|       - |  957 | `	}` |
|       - |  958 | `	/* Point to the internal representation of the input hashmap */` |
|    2194 |  959 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|    2194 |  960 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|    2194 |  961 | `	if( pMap->nEntry > 1 ){` |
|    1936 |  962 | `		sxi32 iCmpFlags = 0;` |
|    1936 |  963 | `		if( nArg > 1 ){` |
|       - |  964 | `			/* Extract comparison flags */` |
|    1738 |  965 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     864 |  966 | `		}` |
|       - |  967 | `		/* Decide the order */` |
|    1936 |  968 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|    1936 |  969 | `		HashmapSortDrive(pCtx->pVm,apArg[0],pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|       - |  970 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|    1936 |  971 | `		HashmapSortRehash(pMap);` |
|    1226 |  972 | `	}else if( pMap->nEntry == 1 ){` |
|       - |  973 | `		/* php reindexes even a single-element array: a string key becomes 0 */` |
|     138 |  974 | `		HashmapSortRehash(pMap);` |
|      67 |  975 | `	}` |
|       - |  976 | `	{` |
|    2194 |  977 | `		sxi32 rcCmp = HashmapFlagSortStatus(pCtx);` |
|    2194 |  978 | `		if( rcCmp != PH7_OK ){` |
|      38 |  979 | `			return rcCmp;` |
|       - |  980 | `		}` |
|       - |  981 | `	}` |
|       - |  982 | `	/* All done,return TRUE */` |
|    2158 |  983 | `	ph7_result_bool(pCtx,1);` |
|    2158 |  984 | `	return PH7_OK;` |
|    1097 |  985 | `}` |
|       - |  986 | `/*` |
|       - |  987 | ` * bool asort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - |  988 | ` *  Sort an array and maintain index association.` |
|       - |  989 | ` * Parameters` |
|       - |  990 | ` *  $array` |
|       - |  991 | ` *   The input array.` |
|       - |  992 | ` * $sort_flags` |
|       - |  993 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - |  994 | ` *  Sorting type flags:` |
|       - |  995 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - |  996 | ` *   SORT_NUMERIC - compare items numerically` |
|       - |  997 | ` *   SORT_STRING - compare items as strings` |
|       - |  998 | ` * Return` |
|       - |  999 | ` *  TRUE on success or FALSE on failure.` |
|       - | 1000 | ` */` |
|      62 | 1001 | `PH7_PRIVATE int ph7_hashmap_asort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1002 | `{` |
|       - | 1003 | `	char zGiven[64];` |
|       - | 1004 | `	ph7_hashmap *pMap;` |
|       - | 1005 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|      67 | 1006 | `	if( nArg < 1 ){` |
|     ! 0 | 1007 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1008 | `			"ArgumentCountError",` |
|       - | 1009 | `			"asort() expects at least 1 argument, 0 given"` |
|       - | 1010 | `			);` |
|       - | 1011 | `	}` |
|       - | 1012 | `	/* PHP 8: TypeError if first argument is not an array */` |
|      67 | 1013 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 1014 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1015 | `			"TypeError",` |
|       - | 1016 | `			"asort(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 1017 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 1018 | `			);` |
|       - | 1019 | `	}` |
|       - | 1020 | `	/* Point to the internal representation of the input hashmap */` |
|      67 | 1021 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      67 | 1022 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      67 | 1023 | `	if( pMap->nEntry > 1 ){` |
|      61 | 1024 | `		sxi32 iCmpFlags = 0;` |
|      61 | 1025 | `		if( nArg > 1 ){` |
|       - | 1026 | `			/* Extract comparison flags */` |
|      30 | 1027 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|      13 | 1028 | `		}` |
|       - | 1029 | `		/* Decide the order */` |
|      61 | 1030 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|      61 | 1031 | `		HashmapSortDrive(pCtx->pVm,apArg[0],pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|      28 | 1032 | `	}` |
|       - | 1033 | `	{` |
|      67 | 1034 | `		sxi32 rcCmp = HashmapFlagSortStatus(pCtx);` |
|      67 | 1035 | `		if( rcCmp != PH7_OK ){` |
|      16 | 1036 | `			return rcCmp;` |
|       - | 1037 | `		}` |
|       - | 1038 | `	}` |
|       - | 1039 | `	/* All done,return TRUE */` |
|      53 | 1040 | `	ph7_result_bool(pCtx,1);` |
|      53 | 1041 | `	return PH7_OK;` |
|      36 | 1042 | `}` |
|       - | 1043 | `/*` |
|       - | 1044 | ` * bool natsort(array &$array)` |
|       - | 1045 | ` * bool natcasesort(array &$array)` |
|       - | 1046 | ` *  Sort an array with php's "natural order" algorithm, maintaining index` |
|       - | 1047 | ` *  association: exactly asort() under SORT_NATURAL (plus SORT_FLAG_CASE for the` |
|       - | 1048 | ` *  case-insensitive twin), which is how php implements them too.` |
|       - | 1049 | ` *` |
|       - | 1050 | `` *  They used to be PRELUDE wrappers over `uasort($array, 'strnatcmp')`, and that`` |
|       - | 1051 | ` *  is a different function: uasort hands each element to a userland callback, so` |
|       - | 1052 | `` *  every element had to satisfy strnatcmp's `string` ZPP row. php's natsort`` |
|       - | 1053 | ` *  COERCES each element the way any string comparison does, so` |
|       - | 1054 | `` *  `natsort([10, "9", null])` — nothing exotic, just a mixed array — was a`` |
|       - | 1055 | ` *  TypeError in PHL and a sorted array in php, and an object with no` |
|       - | 1056 | ` *  __toString() answered strnatcmp's ZPP TypeError instead of php's coercion` |
|       - | 1057 | ` *  Error. Routing through the flag comparator picks up HashmapFlagStringify,` |
|       - | 1058 | ` *  which already renders arrays with php's "Array to string conversion" warning` |
|       - | 1059 | ` *  and raises the coercion Error once per sort.` |
|       - | 1060 | ` */` |
|      26 | 1061 | `PH7_PRIVATE int ph7_hashmap_natsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 1062 | `{` |
|       - | 1063 | `	char zGiven[64];` |
|      29 | 1064 | `	const char *zName = ph7_function_name(pCtx);` |
|       - | 1065 | `	/* natcasesort() is the SORT_FLAG_CASE twin; match the whole name, not a byte. */` |
|      34 | 1066 | `	int bFold = zName && SyStrlen(zName) == sizeof("natcasesort")-1` |
|      39 | 1067 | `		&& SyMemcmp(zName,"natcasesort",sizeof("natcasesort")-1) == 0;` |
|       - | 1068 | `	ph7_hashmap *pMap;` |
|      29 | 1069 | `	if( nArg < 1 ){` |
|     ! 0 | 1070 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1071 | `			"ArgumentCountError",` |
|     ! 0 | 1072 | `			"%s() expects exactly 1 argument, 0 given",zName` |
|       - | 1073 | `			);` |
|       - | 1074 | `	}` |
|      29 | 1075 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 1076 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1077 | `			"TypeError",` |
|       - | 1078 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 1079 | `			zName,VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 1080 | `			);` |
|       - | 1081 | `	}` |
|      29 | 1082 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      29 | 1083 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      29 | 1084 | `	if( pMap->nEntry > 1 ){` |
|       - | 1085 | `		/* SORT_NATURAL (6), optionally \| SORT_FLAG_CASE (8) — the same iFlags` |
|       - | 1086 | `		 * word asort() forwards, so this is asort($a, SORT_NATURAL) exactly. */` |
|      25 | 1087 | `		sxi32 iCmpFlags = bFold ? (6\|8) : 6;` |
|      25 | 1088 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|      25 | 1089 | `		HashmapSortDrive(pCtx->pVm,apArg[0],pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|      25 | 1090 | `		while(pMap->pLast->pPrev){` |
|     ! 0 | 1091 | `			pMap->pLast = pMap->pLast->pPrev;` |
|     ! 0 | 1092 | `		}` |
|      11 | 1093 | `	}` |
|       - | 1094 | `	{` |
|      29 | 1095 | `		sxi32 rcCmp = HashmapFlagSortStatus(pCtx);` |
|      29 | 1096 | `		if( rcCmp != PH7_OK ){` |
|       5 | 1097 | `			return rcCmp;` |
|       - | 1098 | `		}` |
|       - | 1099 | `	}` |
|      25 | 1100 | `	ph7_result_bool(pCtx,1);` |
|      25 | 1101 | `	return PH7_OK;` |
|      16 | 1102 | `}` |
|       - | 1103 | `/*` |
|       - | 1104 | ` * bool arsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 1105 | ` *  Sort an array in reverse order and maintain index association.` |
|       - | 1106 | ` * Parameters` |
|       - | 1107 | ` *  $array` |
|       - | 1108 | ` *   The input array.` |
|       - | 1109 | ` * $sort_flags` |
|       - | 1110 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 1111 | ` *  Sorting type flags:` |
|       - | 1112 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 1113 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 1114 | ` *   SORT_STRING - compare items as strings` |
|       - | 1115 | ` * Return` |
|       - | 1116 | ` *  TRUE on success or FALSE on failure.` |
|       - | 1117 | ` */` |
|      28 | 1118 | `PH7_PRIVATE int ph7_hashmap_arsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 1119 | `{` |
|       - | 1120 | `	char zGiven[64];` |
|       - | 1121 | `	ph7_hashmap *pMap;` |
|       - | 1122 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|      31 | 1123 | `	if( nArg < 1 ){` |
|     ! 0 | 1124 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1125 | `			"ArgumentCountError",` |
|       - | 1126 | `			"arsort() expects at least 1 argument, 0 given"` |
|       - | 1127 | `			);` |
|       - | 1128 | `	}` |
|       - | 1129 | `	/* PHP 8: TypeError if first argument is not an array */` |
|      31 | 1130 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 1131 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1132 | `			"TypeError",` |
|       - | 1133 | `			"arsort(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 1134 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 1135 | `			);` |
|       - | 1136 | `	}` |
|       - | 1137 | `	/* Point to the internal representation of the input hashmap */` |
|      31 | 1138 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      31 | 1139 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      31 | 1140 | `	if( pMap->nEntry > 1 ){` |
|      27 | 1141 | `		sxi32 iCmpFlags = 0;` |
|      27 | 1142 | `		if( nArg > 1 ){` |
|       - | 1143 | `			/* Extract comparison flags */` |
|      13 | 1144 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       5 | 1145 | `		}` |
|       - | 1146 | `		/* Decide the order */` |
|      27 | 1147 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|      27 | 1148 | `		HashmapSortDrive(pCtx->pVm,apArg[0],pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|      12 | 1149 | `	}` |
|       - | 1150 | `	{` |
|      31 | 1151 | `		sxi32 rcCmp = HashmapFlagSortStatus(pCtx);` |
|      31 | 1152 | `		if( rcCmp != PH7_OK ){` |
|       3 | 1153 | `			return rcCmp;` |
|       - | 1154 | `		}` |
|       - | 1155 | `	}` |
|       - | 1156 | `	/* All done,return TRUE */` |
|      28 | 1157 | `	ph7_result_bool(pCtx,1);` |
|      28 | 1158 | `	return PH7_OK;` |
|      17 | 1159 | `}` |
|       - | 1160 | `/*` |
|       - | 1161 | ` * bool ksort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 1162 | ` *  Sort an array by key.` |
|       - | 1163 | ` * Parameters` |
|       - | 1164 | ` *  $array` |
|       - | 1165 | ` *   The input array.` |
|       - | 1166 | ` * $sort_flags` |
|       - | 1167 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 1168 | ` *  Sorting type flags:` |
|       - | 1169 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 1170 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 1171 | ` *   SORT_STRING - compare items as strings` |
|       - | 1172 | ` * Return` |
|       - | 1173 | ` *  TRUE on success or FALSE on failure.` |
|       - | 1174 | ` */` |
|     368 | 1175 | `PH7_PRIVATE int ph7_hashmap_ksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 1176 | `{` |
|       - | 1177 | `	ph7_hashmap *pMap;` |
|       - | 1178 | `	/* Make sure we are dealing with a valid hashmap */` |
|     372 | 1179 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 1180 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 1181 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1182 | `		return PH7_OK;` |
|       - | 1183 | `	}` |
|       - | 1184 | `	/* Point to the internal representation of the input hashmap */` |
|     372 | 1185 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|     372 | 1186 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     372 | 1187 | `	if( pMap->nEntry > 1 ){` |
|     132 | 1188 | `		sxi32 iCmpFlags = 0;` |
|     132 | 1189 | `		if( nArg > 1 ){` |
|       - | 1190 | `			/* Extract comparison flags */` |
|      57 | 1191 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|      27 | 1192 | `		}` |
|       - | 1193 | `		/* Decide the order */` |
|     132 | 1194 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|     132 | 1195 | `		HashmapSortDrive(pCtx->pVm,apArg[0],pMap,HashmapCmpCallback2,SX_INT_TO_PTR(iCmpFlags));` |
|      64 | 1196 | `	}` |
|       - | 1197 | `	{` |
|     372 | 1198 | `		sxi32 rcCmp = HashmapFlagSortStatus(pCtx);` |
|     372 | 1199 | `		if( rcCmp != PH7_OK ){` |
|     ! 0 | 1200 | `			return rcCmp;` |
|       - | 1201 | `		}` |
|       - | 1202 | `	}` |
|       - | 1203 | `	/* All done,return TRUE */` |
|     372 | 1204 | `	ph7_result_bool(pCtx,1);` |
|     372 | 1205 | `	return PH7_OK;` |
|     188 | 1206 | `}` |
|       - | 1207 | `/*` |
|       - | 1208 | ` * bool krsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 1209 | ` *  Sort an array by key in reverse order.` |
|       - | 1210 | ` * Parameters` |
|       - | 1211 | ` *  $array` |
|       - | 1212 | ` *   The input array.` |
|       - | 1213 | ` * $sort_flags` |
|       - | 1214 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 1215 | ` *  Sorting type flags:` |
|       - | 1216 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 1217 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 1218 | ` *   SORT_STRING - compare items as strings` |
|       - | 1219 | ` * Return` |
|       - | 1220 | ` *  TRUE on success or FALSE on failure.` |
|       - | 1221 | ` */` |
|      30 | 1222 | `PH7_PRIVATE int ph7_hashmap_krsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1223 | `{` |
|       - | 1224 | `	ph7_hashmap *pMap;` |
|       - | 1225 | `	/* Make sure we are dealing with a valid hashmap */` |
|      32 | 1226 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 1227 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 1228 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1229 | `		return PH7_OK;` |
|       - | 1230 | `	}` |
|       - | 1231 | `	/* Point to the internal representation of the input hashmap */` |
|      32 | 1232 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      32 | 1233 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      32 | 1234 | `	if( pMap->nEntry > 1 ){` |
|      32 | 1235 | `		sxi32 iCmpFlags = 0;` |
|      32 | 1236 | `		if( nArg > 1 ){` |
|       - | 1237 | `			/* Extract comparison flags */` |
|       6 | 1238 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       2 | 1239 | `		}` |
|       - | 1240 | `		/* Decide the order */` |
|      32 | 1241 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|      32 | 1242 | `		HashmapSortDrive(pCtx->pVm,apArg[0],pMap,HashmapCmpCallback5,SX_INT_TO_PTR(iCmpFlags));` |
|      15 | 1243 | `	}` |
|       - | 1244 | `	{` |
|      32 | 1245 | `		sxi32 rcCmp = HashmapFlagSortStatus(pCtx);` |
|      32 | 1246 | `		if( rcCmp != PH7_OK ){` |
|     ! 0 | 1247 | `			return rcCmp;` |
|       - | 1248 | `		}` |
|       - | 1249 | `	}` |
|       - | 1250 | `	/* All done,return TRUE */` |
|      32 | 1251 | `	ph7_result_bool(pCtx,1);` |
|      32 | 1252 | `	return PH7_OK;` |
|      17 | 1253 | `}` |
|       - | 1254 | `/*` |
|       - | 1255 | ` * bool rsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 1256 | ` * Sort an array in reverse order.` |
|       - | 1257 | ` * Parameters` |
|       - | 1258 | ` *  $array` |
|       - | 1259 | ` *   The input array.` |
|       - | 1260 | ` * $sort_flags` |
|       - | 1261 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 1262 | ` *  Sorting type flags:` |
|       - | 1263 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 1264 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 1265 | ` *   SORT_STRING - compare items as strings` |
|       - | 1266 | ` * Return` |
|       - | 1267 | ` *  TRUE on success or FALSE on failure.` |
|       - | 1268 | ` */` |
|      40 | 1269 | `PH7_PRIVATE int ph7_hashmap_rsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1270 | `{` |
|       - | 1271 | `	ph7_hashmap *pMap;` |
|       - | 1272 | `	/* Make sure we are dealing with a valid hashmap */` |
|      45 | 1273 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 1274 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 1275 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1276 | `		return PH7_OK;` |
|       - | 1277 | `	}` |
|       - | 1278 | `	/* Point to the internal representation of the input hashmap */` |
|      45 | 1279 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      45 | 1280 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      45 | 1281 | `	if( pMap->nEntry > 1 ){` |
|      43 | 1282 | `		sxi32 iCmpFlags = 0;` |
|      43 | 1283 | `		if( nArg > 1 ){` |
|       - | 1284 | `			/* Extract comparison flags */` |
|      28 | 1285 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|      12 | 1286 | `		}` |
|       - | 1287 | `		/* Decide the order */` |
|      43 | 1288 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|      43 | 1289 | `		HashmapSortDrive(pCtx->pVm,apArg[0],pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 1290 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|      43 | 1291 | `		HashmapSortRehash(pMap);` |
|      22 | 1292 | `	}else if( pMap->nEntry == 1 ){` |
|       - | 1293 | `		/* php reindexes even a single-element array: a string key becomes 0 */` |
|       3 | 1294 | `		HashmapSortRehash(pMap);` |
|       1 | 1295 | `	}` |
|       - | 1296 | `	{` |
|      45 | 1297 | `		sxi32 rcCmp = HashmapFlagSortStatus(pCtx);` |
|      45 | 1298 | `		if( rcCmp != PH7_OK ){` |
|      14 | 1299 | `			return rcCmp;` |
|       - | 1300 | `		}` |
|       - | 1301 | `	}` |
|       - | 1302 | `	/* All done,return TRUE */` |
|      32 | 1303 | `	ph7_result_bool(pCtx,1);` |
|      32 | 1304 | `	return PH7_OK;` |
|      25 | 1305 | `}` |
|       - | 1306 | `/*` |
|       - | 1307 | ` * bool usort(array &$array,callable $cmp_function)` |
|       - | 1308 | ` *  Sort an array by values using a user-defined comparison function.` |
|       - | 1309 | ` * Parameters` |
|       - | 1310 | ` *  $array` |
|       - | 1311 | ` *   The input array.` |
|       - | 1312 | ` * $cmp_function` |
|       - | 1313 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 1314 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 1315 | ` *  to, or greater than the second.` |
|       - | 1316 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 1317 | ` * Return` |
|       - | 1318 | ` *  TRUE on success or FALSE on failure.` |
|       - | 1319 | ` */` |
|     209 | 1320 | `PH7_PRIVATE int ph7_hashmap_usort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1321 | `{` |
|       - | 1322 | `	ph7_hashmap *pMap;` |
|       - | 1323 | `	/* Make sure we are dealing with a valid hashmap */` |
|     214 | 1324 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 1325 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 1326 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1327 | `		return PH7_OK;` |
|       - | 1328 | `	}` |
|     214 | 1329 | `	if( nArg > 1 ){` |
|       - | 1330 | `		/* php rejects an uncallable comparator with a TypeError, BEFORE touching the` |
|       - | 1331 | `		 * array. PH7 handed it to the dispatcher, which answers NULL in silence -- so` |
|       - | 1332 | `		 * usort() with a bogus comparator reordered the array anyway, by nothing. */` |
|     214 | 1333 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",0);` |
|     214 | 1334 | `		if( rcCb != PH7_OK ){` |
|       7 | 1335 | `			return rcCb;` |
|       - | 1336 | `		}` |
|      98 | 1337 | `	}` |
|       - | 1338 | `	/* Point to the internal representation of the input hashmap */` |
|     208 | 1339 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|     208 | 1340 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     208 | 1341 | `	if( pMap->nEntry > 1 ){` |
|     206 | 1342 | `		ph7_value *pCallback = 0;` |
|       - | 1343 | `		ProcNodeCmp xCmp;` |
|     206 | 1344 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|     206 | 1345 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 1346 | `			/* Point to the desired callback */` |
|     206 | 1347 | `			pCallback = apArg[1];` |
|     102 | 1348 | `		}else{` |
|       - | 1349 | `			/* Use the default comparison function */` |
|     ! 0 | 1350 | `			xCmp = HashmapCmpCallback1;` |
|       - | 1351 | `		}` |
|       - | 1352 | `		/* Decide the order */` |
|     206 | 1353 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|     206 | 1354 | `		HashmapSortDrive(pCtx->pVm,apArg[0],pMap,xCmp,pCallback);` |
|       - | 1355 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|     206 | 1356 | `		HashmapSortRehash(pMap);` |
|     206 | 1357 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 1358 | `			/* The comparison callback did not return: propagate its status so the` |
|       - | 1359 | `			 * dispatcher unwinds. */` |
|      33 | 1360 | `			sxi32 rcExc = pCtx->pVm->iCmpCallbackExc;` |
|      33 | 1361 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|      33 | 1362 | `			return rcExc;` |
|       5 | 1363 | `		}` |
|      85 | 1364 | `	}else if( pMap->nEntry == 1 ){` |
|       - | 1365 | `		/* php reindexes even a single-element array: a string key becomes 0 */` |
|       3 | 1366 | `		HashmapSortRehash(pMap);` |
|       1 | 1367 | `	}` |
|       - | 1368 | `	/* All done,return TRUE */` |
|     178 | 1369 | `	ph7_result_bool(pCtx,1);` |
|     178 | 1370 | `	return PH7_OK;` |
|     106 | 1371 | `}` |
|       - | 1372 | `/*` |
|       - | 1373 | ` * bool uasort(array &$array,callable $cmp_function)` |
|       - | 1374 | ` *  Sort an array by values using a user-defined comparison function` |
|       - | 1375 | ` *  and maintain index association.` |
|       - | 1376 | ` * Parameters` |
|       - | 1377 | ` *  $array` |
|       - | 1378 | ` *   The input array.` |
|       - | 1379 | ` * $cmp_function` |
|       - | 1380 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 1381 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 1382 | ` *  to, or greater than the second.` |
|       - | 1383 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 1384 | ` * Return` |
|       - | 1385 | ` *  TRUE on success or FALSE on failure.` |
|       - | 1386 | ` */` |
|      22 | 1387 | `PH7_PRIVATE int ph7_hashmap_uasort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 1388 | `{` |
|       - | 1389 | `	ph7_hashmap *pMap;` |
|       - | 1390 | `	/* Make sure we are dealing with a valid hashmap */` |
|      25 | 1391 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 1392 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 1393 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1394 | `		return PH7_OK;` |
|       - | 1395 | `	}` |
|      25 | 1396 | `	if( nArg > 1 ){` |
|       - | 1397 | `		/* php rejects an uncallable comparator with a TypeError, BEFORE touching the` |
|       - | 1398 | `		 * array. PH7 handed it to the dispatcher, which answers NULL in silence -- so` |
|       - | 1399 | `		 * usort() with a bogus comparator reordered the array anyway, by nothing. */` |
|      25 | 1400 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",0);` |
|      25 | 1401 | `		if( rcCb != PH7_OK ){` |
|       5 | 1402 | `			return rcCb;` |
|       - | 1403 | `		}` |
|       9 | 1404 | `	}` |
|       - | 1405 | `	/* Point to the internal representation of the input hashmap */` |
|      21 | 1406 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      21 | 1407 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      21 | 1408 | `	if( pMap->nEntry > 1 ){` |
|      21 | 1409 | `		ph7_value *pCallback = 0;` |
|       - | 1410 | `		ProcNodeCmp xCmp;` |
|      21 | 1411 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|      21 | 1412 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 1413 | `			/* Point to the desired callback */` |
|      21 | 1414 | `			pCallback = apArg[1];` |
|      12 | 1415 | `		}else{` |
|       - | 1416 | `			/* Use the default comparison function */` |
|     ! 0 | 1417 | `			xCmp = HashmapCmpCallback1;` |
|       - | 1418 | `		}` |
|       - | 1419 | `		/* Decide the order */` |
|      21 | 1420 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|      21 | 1421 | `		HashmapSortDrive(pCtx->pVm,apArg[0],pMap,xCmp,pCallback);` |
|      21 | 1422 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 1423 | `			/* The comparison callback did not return: propagate its status so the` |
|       - | 1424 | `			 * dispatcher unwinds. */` |
|       3 | 1425 | `			sxi32 rcExc = pCtx->pVm->iCmpCallbackExc;` |
|       3 | 1426 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 1427 | `			return rcExc;` |
|       - | 1428 | `		}` |
|       8 | 1429 | `	}` |
|       - | 1430 | `	/* All done,return TRUE */` |
|      19 | 1431 | `	ph7_result_bool(pCtx,1);` |
|      19 | 1432 | `	return PH7_OK;` |
|      14 | 1433 | `}` |
|       - | 1434 | `/*` |
|       - | 1435 | ` * bool uksort(array &$array,callable $cmp_function)` |
|       - | 1436 | ` *  Sort an array by keys using a user-defined comparison` |
|       - | 1437 | ` *  function and maintain index association.` |
|       - | 1438 | ` * Parameters` |
|       - | 1439 | ` *  $array` |
|       - | 1440 | ` *   The input array.` |
|       - | 1441 | ` * $cmp_function` |
|       - | 1442 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 1443 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 1444 | ` *  to, or greater than the second.` |
|       - | 1445 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 1446 | ` * Return` |
|       - | 1447 | ` *  TRUE on success or FALSE on failure.` |
|       - | 1448 | ` */` |
|      14 | 1449 | `PH7_PRIVATE int ph7_hashmap_uksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1450 | `{` |
|       - | 1451 | `	ph7_hashmap *pMap;` |
|       - | 1452 | `	/* Make sure we are dealing with a valid hashmap */` |
|      16 | 1453 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 1454 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 1455 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1456 | `		return PH7_OK;` |
|       - | 1457 | `	}` |
|      16 | 1458 | `	if( nArg > 1 ){` |
|       - | 1459 | `		/* php rejects an uncallable comparator with a TypeError, BEFORE touching the` |
|       - | 1460 | `		 * array. PH7 handed it to the dispatcher, which answers NULL in silence -- so` |
|       - | 1461 | `		 * usort() with a bogus comparator reordered the array anyway, by nothing. */` |
|      16 | 1462 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",0);` |
|      16 | 1463 | `		if( rcCb != PH7_OK ){` |
|       3 | 1464 | `			return rcCb;` |
|       - | 1465 | `		}` |
|       6 | 1466 | `	}` |
|       - | 1467 | `	/* Point to the internal representation of the input hashmap */` |
|      14 | 1468 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      14 | 1469 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      14 | 1470 | `	if( pMap->nEntry > 1 ){` |
|      14 | 1471 | `		ph7_value *pCallback = 0;` |
|       - | 1472 | `		ProcNodeCmp xCmp;` |
|      14 | 1473 | `		xCmp = HashmapCmpCallback6; /* User-defined function as the comparison callback */` |
|      14 | 1474 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 1475 | `			/* Point to the desired callback */` |
|      14 | 1476 | `			pCallback = apArg[1];` |
|       8 | 1477 | `		}else{` |
|       - | 1478 | `			/* Use the default comparison function */` |
|     ! 0 | 1479 | `			xCmp = HashmapCmpCallback2;` |
|       - | 1480 | `		}` |
|       - | 1481 | `		/* Decide the order */` |
|      14 | 1482 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|      14 | 1483 | `		HashmapSortDrive(pCtx->pVm,apArg[0],pMap,xCmp,pCallback);` |
|      14 | 1484 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 1485 | `			/* The comparison callback did not return: propagate its status so the` |
|       - | 1486 | `			 * dispatcher unwinds. */` |
|       3 | 1487 | `			sxi32 rcExc = pCtx->pVm->iCmpCallbackExc;` |
|       3 | 1488 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 1489 | `			return rcExc;` |
|       - | 1490 | `		}` |
|       5 | 1491 | `	}` |
|       - | 1492 | `	/* All done,return TRUE */` |
|      12 | 1493 | `	ph7_result_bool(pCtx,1);` |
|      12 | 1494 | `	return PH7_OK;` |
|       9 | 1495 | `}` |
|       - | 1496 | `/*` |
|       - | 1497 | ` * bool array_multisort(array &$array, mixed $array1_sort_order = SORT_ASC,` |
|       - | 1498 | ` *                      mixed $array1_sort_flags = SORT_REGULAR, mixed &...$rest)` |
|       - | 1499 | ` *  Sort multiple arrays at once: the argument list is a little language read` |
|       - | 1500 | ` *  left to right — an ARRAY opens a column, and each column may be followed by` |
|       - | 1501 | ` *  at most one sort ORDER (SORT_ASC/SORT_DESC) and at most one sort FLAGS` |
|       - | 1502 | ` *  value, in either order. Rows are compared column by column, a tie in one` |
|       - | 1503 | ` *  column falling through to the next; the resulting permutation is applied to` |
|       - | 1504 | ` *  EVERY column. String keys are kept, numeric keys are renumbered, and the` |
|       - | 1505 | ` *  sort is stable (php 8's own guarantee). php's error shapes, pinned by` |
|       - | 1506 | `` *  probe: a non-int non-array is `Argument #N must be an array or a sort`` |
|       - | 1507 | `` *  flag`; a misplaced or repeated order/flags is the same text with`` |
|       - | 1508 | `` *  `... that has not already been specified`; an int that is no flag at all is`` |
|       - | 1509 | `` *  the ValueError `must be a valid sort flag`; mismatched lengths are`` |
|       - | 1510 | `` *  `Array sizes are inconsistent` with no function prefix; and only`` |
|       - | 1511 | `` *  Argument #1 carries its `($array)` name. php declares the whole list`` |
|       - | 1512 | ` *  prefer-ref (VmBuiltinPrefersRef), so a literal sorts a temporary silently.` |
|       - | 1513 | ` */` |
|       - | 1514 | `/* One column of the multisort: the caller's array plus its sort spec. */` |
|       - | 1515 | `typedef struct MultisortCol MultisortCol;` |
|       - | 1516 | `struct MultisortCol` |
|       - | 1517 | `{` |
|       - | 1518 | `	ph7_value *pArr;        /* The caller's argument slot */` |
|       - | 1519 | `	ph7_hashmap *pMap;      /* Its hashmap */` |
|       - | 1520 | `	ph7_hashmap_node **apNode; /* Nodes in ORIGINAL iteration order */` |
|       - | 1521 | `	sxi32 iFlags;           /* SORT_* comparison flags */` |
|       - | 1522 | `	int iDir;               /* +1 SORT_ASC, -1 SORT_DESC */` |
|       - | 1523 | `	int bOrderSeen;         /* An order argument already attached */` |
|       - | 1524 | `	int bFlagsSeen;         /* A flags argument already attached */` |
|       - | 1525 | `};` |
|       - | 1526 | `/* Compare two ROWS, column by column with each column's own direction/flags. */` |
|      96 | 1527 | `static sxi32 MultisortRowCmp(MultisortCol *aCol,sxu32 nCol,sxu32 iA,sxu32 iB)` |
|       3 | 1528 | `{` |
|      99 | 1529 | `	ph7_vm *pVm = aCol[0].pMap->pVm;` |
|       - | 1530 | `	sxu32 c;` |
|     109 | 1531 | `	for( c = 0 ; c < nCol ; c++ ){` |
|     109 | 1532 | `		sxi32 rc = HashmapFlagValueCmp(aCol[c].apNode[iA],aCol[c].apNode[iB],aCol[c].iFlags);` |
|     109 | 1533 | `		HashmapCmpLatch(pVm);` |
|     109 | 1534 | `		if( rc != 0 ){` |
|      99 | 1535 | `			return aCol[c].iDir < 0 ? -rc : rc;` |
|       - | 1536 | `		}` |
|       6 | 1537 | `	}` |
|     ! 0 | 1538 | `	return 0;` |
|      51 | 1539 | `}` |
|       - | 1540 | `/*` |
|       - | 1541 | ` * Stable bottom-up merge sort over the row-index permutation. Iterative on` |
|       - | 1542 | ` * purpose — the recursive shape would put O(log n) frames on the native stack` |
|       - | 1543 | ` * (the embedder C-stack family).` |
|       - | 1544 | ` */` |
|      34 | 1545 | `static void MultisortSortIdx(MultisortCol *aCol,sxu32 nCol,sxu32 *aIdx,sxu32 *aTmp,sxu32 n)` |
|       3 | 1546 | `{` |
|       - | 1547 | `	sxu32 nWidth,iLo;` |
|     101 | 1548 | `	for( nWidth = 1 ; nWidth < n ; nWidth *= 2 ){` |
|     161 | 1549 | `		for( iLo = 0 ; iLo < n ; iLo += 2 * nWidth ){` |
|      97 | 1550 | `			sxu32 iMid = iLo + nWidth;` |
|      97 | 1551 | `			sxu32 iHi = iLo + 2 * nWidth;` |
|       - | 1552 | `			sxu32 i,j,k;` |
|      97 | 1553 | `			if( iMid > n ){ iMid = n; }` |
|      97 | 1554 | `			if( iHi > n ){ iHi = n; }` |
|      97 | 1555 | `			i = iLo; j = iMid; k = iLo;` |
|     193 | 1556 | `			while( i < iMid && j < iHi ){` |
|       - | 1557 | `				/* <= keeps the run stable: on a full tie the left row wins */` |
|      99 | 1558 | `				if( MultisortRowCmp(aCol,nCol,aIdx[i],aIdx[j]) <= 0 ){` |
|      43 | 1559 | `					aTmp[k++] = aIdx[i++];` |
|      23 | 1560 | `				}else{` |
|      59 | 1561 | `					aTmp[k++] = aIdx[j++];` |
|       - | 1562 | `				}` |
|       3 | 1563 | `			}` |
|     181 | 1564 | `			while( i < iMid ){ aTmp[k++] = aIdx[i++]; }` |
|     113 | 1565 | `			while( j < iHi ){ aTmp[k++] = aIdx[j++]; }` |
|      50 | 1566 | `		}` |
|       - | 1567 | `		/* aTmp holds the merged runs for this width; swap roles by copying` |
|       - | 1568 | `		 * back — n is bounded by the array count, one memcpy per doubling. */` |
|      67 | 1569 | `		SyMemcpy(aTmp,aIdx,(sxu32)(n * sizeof(sxu32)));` |
|      35 | 1570 | `	}` |
|      37 | 1571 | `}` |
|      60 | 1572 | `PH7_PRIVATE int ph7_hashmap_multisort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 1573 | `{` |
|       - | 1574 | `	MultisortCol *aCol;` |
|       - | 1575 | `	sxu32 *aIdx,*aTmp;` |
|      63 | 1576 | `	sxu32 nCol = 0,nRow,i;` |
|       - | 1577 | `	sxi32 rcStatus;` |
|       - | 1578 | `	int iArg;` |
|       - | 1579 |  |
|      63 | 1580 | `	if( nArg < 1 ){` |
|     ! 0 | 1581 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1582 | `			"ArgumentCountError",` |
|       - | 1583 | `			"array_multisort() expects at least 1 argument, %d given",` |
|     ! 0 | 1584 | `			nArg` |
|       - | 1585 | `			);` |
|       - | 1586 | `	}` |
|      93 | 1587 | `	aCol = (MultisortCol *)ph7_context_alloc_chunk(pCtx,` |
|      60 | 1588 | `		(unsigned int)(sizeof(MultisortCol) * (sxu32)nArg),TRUE,TRUE);` |
|      63 | 1589 | `	if( aCol == 0 ){` |
|     ! 0 | 1590 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 1591 | `	}` |
|       - | 1592 | `	/* Read the argument list's little language, php's own state machine. */` |
|     161 | 1593 | `	for( iArg = 0 ; iArg < nArg ; iArg++ ){` |
|     123 | 1594 | `		ph7_value *pArg = apArg[iArg];` |
|       - | 1595 | `		/* Only Argument #1 carries its parameter name in php's messages. */` |
|     123 | 1596 | `		const char *zName = (iArg == 0) ? " ($array)" : "";` |
|     123 | 1597 | `		if( ph7_value_is_array(pArg) ){` |
|      79 | 1598 | `			MultisortCol *pCol = &aCol[nCol++];` |
|      79 | 1599 | `			pCol->pArr = pArg;` |
|      79 | 1600 | `			pCol->pMap = (ph7_hashmap *)pArg->x.pOther;` |
|      79 | 1601 | `			pCol->apNode = 0;` |
|      79 | 1602 | `			pCol->iFlags = 0; /* SORT_REGULAR */` |
|      79 | 1603 | `			pCol->iDir = 1;   /* SORT_ASC */` |
|      79 | 1604 | `			pCol->bOrderSeen = pCol->bFlagsSeen = 0;` |
|      79 | 1605 | `			continue;` |
|       - | 1606 | `		}` |
|      47 | 1607 | `		if( ph7_value_is_float(pArg) \|\| (pArg->iFlags & MEMOBJ_INT) == 0 ){` |
|       - | 1608 | `			/* A REAL int only, float asked FIRST (ph7_type_name's rule: an` |
|       - | 1609 | `			 * integer-valued real caches an int and would pass a bare flag` |
|       - | 1610 | `			 * test). php coerces nothing here: "4", 4.0 and true are all` |
|       - | 1611 | `			 * refused. */` |
|      17 | 1612 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1613 | `				"array_multisort(): Argument #%d%s must be an array or a sort flag",` |
|       5 | 1614 | `				iArg + 1,zName);` |
|       - | 1615 | `		}` |
|       - | 1616 | `		{` |
|      37 | 1617 | `			sxi64 iVal = ph7_value_to_int64(pArg);` |
|       - | 1618 | `			/* php masks SORT_FLAG_CASE off for the ORDER match too, but reads` |
|       - | 1619 | `			 * the direction from the UNMASKED value — so SORT_DESC\|SORT_FLAG_CASE` |
|       - | 1620 | `			 * (11) consumes the order slot and quirkily sorts ASCENDING. */` |
|      37 | 1621 | `			if( (iVal & ~(sxi64)8) == 3 /* SORT_DESC */ \|\| (iVal & ~(sxi64)8) == 4 /* SORT_ASC */ ){` |
|      23 | 1622 | `				if( nCol < 1 \|\| aCol[nCol - 1].bOrderSeen ){` |
|      11 | 1623 | `					return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1624 | `						"array_multisort(): Argument #%d%s must be an array or a sort flag that has not already been specified",` |
|       3 | 1625 | `						iArg + 1,zName);` |
|       - | 1626 | `				}` |
|      17 | 1627 | `				aCol[nCol - 1].iDir = (iVal == 3) ? -1 : 1;` |
|      17 | 1628 | `				aCol[nCol - 1].bOrderSeen = 1;` |
|      24 | 1629 | `			}else if( (iVal & ~(sxi64)8 /* SORT_FLAG_CASE */) == 0 /* SORT_REGULAR */` |
|      13 | 1630 | `			       \|\| (iVal & ~(sxi64)8) == 1 /* SORT_NUMERIC */` |
|      11 | 1631 | `			       \|\| (iVal & ~(sxi64)8) == 2 /* SORT_STRING */` |
|       8 | 1632 | `			       \|\| (iVal & ~(sxi64)8) == 5 /* SORT_LOCALE_STRING */` |
|       7 | 1633 | `			       \|\| (iVal & ~(sxi64)8) == 6 /* SORT_NATURAL */ ){` |
|      14 | 1634 | `				if( nCol < 1 \|\| aCol[nCol - 1].bFlagsSeen ){` |
|       7 | 1635 | `					return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1636 | `						"array_multisort(): Argument #%d%s must be an array or a sort flag that has not already been specified",` |
|       2 | 1637 | `						iArg + 1,zName);` |
|       - | 1638 | `				}` |
|      10 | 1639 | `				aCol[nCol - 1].iFlags = (sxi32)iVal;` |
|      10 | 1640 | `				aCol[nCol - 1].bFlagsSeen = 1;` |
|       6 | 1641 | `			}else{` |
|       4 | 1642 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1643 | `					"array_multisort(): Argument #%d%s must be a valid sort flag",` |
|       1 | 1644 | `					iArg + 1,zName);` |
|       - | 1645 | `			}` |
|       - | 1646 | `		}` |
|      14 | 1647 | `	}` |
|       - | 1648 | `	/* Every column must hold the same number of rows; php's message carries no` |
|       - | 1649 | `	 * function prefix. */` |
|      41 | 1650 | `	nRow = aCol[0].pMap->nEntry;` |
|      57 | 1651 | `	for( i = 1 ; i < nCol ; i++ ){` |
|      21 | 1652 | `		if( aCol[i].pMap->nEntry != nRow ){` |
|       3 | 1653 | `			return PH7_VmThrowException(pCtx,"ValueError","Array sizes are inconsistent");` |
|       - | 1654 | `		}` |
|      10 | 1655 | `	}` |
|      39 | 1656 | `	if( nRow > 0 ){` |
|       - | 1657 | `		/* Collect each column's nodes in original order. */` |
|      85 | 1658 | `		for( i = 0 ; i < nCol ; i++ ){` |
|      51 | 1659 | `			ph7_hashmap_node *pNode = aCol[i].pMap->pFirst;` |
|       - | 1660 | `			sxu32 r;` |
|      75 | 1661 | `			aCol[i].apNode = (ph7_hashmap_node **)ph7_context_alloc_chunk(pCtx,` |
|      24 | 1662 | `				(unsigned int)(sizeof(ph7_hashmap_node *) * nRow),FALSE,TRUE);` |
|      51 | 1663 | `			if( aCol[i].apNode == 0 ){` |
|     ! 0 | 1664 | `				return PH7_ContextMemoryError(pCtx);` |
|       - | 1665 | `			}` |
|     195 | 1666 | `			for( r = 0 ; r < nRow && pNode ; r++ ){` |
|     147 | 1667 | `				aCol[i].apNode[r] = pNode;` |
|     147 | 1668 | `				pNode = pNode->pPrev; /* Reverse link */` |
|      75 | 1669 | `			}` |
|      27 | 1670 | `		}` |
|      54 | 1671 | `		aIdx = (sxu32 *)ph7_context_alloc_chunk(pCtx,` |
|      17 | 1672 | `			(unsigned int)(sizeof(sxu32) * nRow * 2),FALSE,TRUE);` |
|      37 | 1673 | `		if( aIdx == 0 ){` |
|     ! 0 | 1674 | `			return PH7_ContextMemoryError(pCtx);` |
|       - | 1675 | `		}` |
|      37 | 1676 | `		aTmp = &aIdx[nRow];` |
|     139 | 1677 | `		for( i = 0 ; i < nRow ; i++ ){` |
|     105 | 1678 | `			aIdx[i] = i;` |
|      54 | 1679 | `		}` |
|       - | 1680 | `		/* The string flags coerce user-visibly and can only FLAG a throw` |
|       - | 1681 | `		 * (iCmpCallbackExc); clear it, sort, and report after — the flag-sort` |
|       - | 1682 | `		 * drivers' shared pattern. */` |
|      37 | 1683 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|      37 | 1684 | `		MultisortSortIdx(aCol,nCol,aIdx,aTmp,nRow);` |
|      37 | 1685 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 1686 | `			/* A comparison raised. php's array_multisort leaves EVERY column as` |
|       - | 1687 | `			 * it found it in that case -- unlike sort(), which still writes back` |
|       - | 1688 | `			 * the order it reached -- so the permutation is dropped rather than` |
|       - | 1689 | `			 * applied. */` |
|      11 | 1690 | `			return HashmapFlagSortStatus(pCtx);` |
|       - | 1691 | `		}` |
|       - | 1692 | `		/* Apply the permutation to every column: rebuild in sorted order,` |
|       - | 1693 | `		 * keeping string keys and renumbering int keys, then hand the fresh` |
|       - | 1694 | `		 * array back through the by-ref slot (a literal has none and the` |
|       - | 1695 | `		 * result is silently dropped — php's prefer-ref). */` |
|      63 | 1696 | `		for( i = 0 ; i < nCol ; i++ ){` |
|      39 | 1697 | `			ph7_value *pNew = ph7_context_new_array(pCtx);` |
|       - | 1698 | `			sxu32 r;` |
|      39 | 1699 | `			if( pNew == 0 ){` |
|     ! 0 | 1700 | `				return PH7_ContextMemoryError(pCtx);` |
|       - | 1701 | `			}` |
|     147 | 1702 | `			for( r = 0 ; r < nRow ; r++ ){` |
|     111 | 1703 | `				ph7_hashmap_node *pNode = aCol[i].apNode[aIdx[r]];` |
|     165 | 1704 | `				HashmapInsertNode((ph7_hashmap *)pNew->x.pOther,pNode,` |
|     108 | 1705 | `					pNode->iType == HASHMAP_BLOB_NODE ? TRUE : FALSE);` |
|      57 | 1706 | `			}` |
|      39 | 1707 | `			PH7_VmStoreArgByRef(pCtx->pVm,aCol[i].pArr,pNew);` |
|      21 | 1708 | `		}` |
|      27 | 1709 | `		rcStatus = HashmapFlagSortStatus(pCtx);` |
|      27 | 1710 | `		if( rcStatus != PH7_OK ){` |
|     ! 0 | 1711 | `			return rcStatus;` |
|       - | 1712 | `		}` |
|      12 | 1713 | `	}` |
|      29 | 1714 | `	ph7_result_bool(pCtx,1);` |
|      29 | 1715 | `	return PH7_OK;` |
|      33 | 1716 | `}` |
|       - | 1717 |  |
