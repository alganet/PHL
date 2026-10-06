# src/ph7/hashmap_sort.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 884/939 lines (94.14%)

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
|  170542 |   60 | `static sxi32 HashmapSortCmp(HashmapSortCtx *pCtx,HashmapSortEnt *pA,HashmapSortEnt *pB)` |
|       5 |   61 | `{` |
|  170547 |   62 | `	sxi32 rc = pCtx->xCmp(pA->pNode,pB->pNode,pCtx->pCmpData);` |
|  170547 |   63 | `	if( rc == 0 && pCtx->bStable ){` |
|       - |   64 | `		/* php's stable_sort_fallback(): the original positions, never equal */` |
|     441 |   65 | `		rc = ( pA->nOrd > pB->nOrd ) ? 1 : -1;` |
|     218 |   66 | `	}` |
|  170547 |   67 | `	return rc;` |
|       5 |   68 | `}` |
|   40151 |   69 | `static void HashmapSortSwap(HashmapSortEnt *pA,HashmapSortEnt *pB)` |
|       5 |   70 | `{` |
|   40156 |   71 | `	HashmapSortEnt sTmp = *pA;` |
|   40156 |   72 | `	*pA = *pB;` |
|   40156 |   73 | `	*pB = sTmp;` |
|   40156 |   74 | `}` |
|       - |   75 | `/*` |
|       - |   76 | ` * The fixed-size sorting networks. Each one is a decision tree, so both the` |
|       - |   77 | ` * number of comparisons and the pairs compared depend on the answers: this is` |
|       - |   78 | ` * where most of the count divergence against php lived.` |
|       - |   79 | ` */` |
|     496 |   80 | `static void HashmapSort2(HashmapSortEnt *a,HashmapSortEnt *b,HashmapSortCtx *pCtx)` |
|       5 |   81 | `{` |
|     501 |   82 | `	if( HashmapSortCmp(pCtx,a,b) > 0 ){` |
|     194 |   83 | `		HashmapSortSwap(a,b);` |
|     101 |   84 | `	}` |
|     501 |   85 | `}` |
|    3378 |   86 | `static void HashmapSort3(HashmapSortEnt *a,HashmapSortEnt *b,HashmapSortEnt *c,HashmapSortCtx *pCtx)` |
|       5 |   87 | `{` |
|    3383 |   88 | `	if( !(HashmapSortCmp(pCtx,a,b) > 0) ){` |
|    2681 |   89 | `		if( !(HashmapSortCmp(pCtx,b,c) > 0) ){` |
|    2068 |   90 | `			return;` |
|       - |   91 | `		}` |
|     618 |   92 | `		HashmapSortSwap(b,c);` |
|     618 |   93 | `		if( HashmapSortCmp(pCtx,a,b) > 0 ){` |
|     201 |   94 | `			HashmapSortSwap(a,b);` |
|      99 |   95 | `		}` |
|     618 |   96 | `		return;` |
|       - |   97 | `	}` |
|     707 |   98 | `	if( !(HashmapSortCmp(pCtx,c,b) > 0) ){` |
|     206 |   99 | `		HashmapSortSwap(a,c);` |
|     206 |  100 | `		return;` |
|       - |  101 | `	}` |
|     506 |  102 | `	HashmapSortSwap(a,b);` |
|     506 |  103 | `	if( HashmapSortCmp(pCtx,b,c) > 0 ){` |
|     332 |  104 | `		HashmapSortSwap(b,c);` |
|     147 |  105 | `	}` |
|    1702 |  106 | `}` |
|     748 |  107 | `static void HashmapSort4(HashmapSortEnt *a,HashmapSortEnt *b,HashmapSortEnt *c,HashmapSortEnt *d,HashmapSortCtx *pCtx)` |
|       5 |  108 | `{` |
|     753 |  109 | `	HashmapSort3(a,b,c,pCtx);` |
|     753 |  110 | `	if( HashmapSortCmp(pCtx,c,d) > 0 ){` |
|     446 |  111 | `		HashmapSortSwap(c,d);` |
|     446 |  112 | `		if( HashmapSortCmp(pCtx,b,c) > 0 ){` |
|     200 |  113 | `			HashmapSortSwap(b,c);` |
|     200 |  114 | `			if( HashmapSortCmp(pCtx,a,b) > 0 ){` |
|      74 |  115 | `				HashmapSortSwap(a,b);` |
|      38 |  116 | `			}` |
|      74 |  117 | `		}` |
|     215 |  118 | `	}` |
|     753 |  119 | `}` |
|     322 |  120 | `static void HashmapSort5(HashmapSortEnt *a,HashmapSortEnt *b,HashmapSortEnt *c,HashmapSortEnt *d,HashmapSortEnt *e,HashmapSortCtx *pCtx)` |
|       5 |  121 | `{` |
|     327 |  122 | `	HashmapSort4(a,b,c,d,pCtx);` |
|     327 |  123 | `	if( HashmapSortCmp(pCtx,d,e) > 0 ){` |
|     227 |  124 | `		HashmapSortSwap(d,e);` |
|     227 |  125 | `		if( HashmapSortCmp(pCtx,c,d) > 0 ){` |
|     135 |  126 | `			HashmapSortSwap(c,d);` |
|     135 |  127 | `			if( HashmapSortCmp(pCtx,b,c) > 0 ){` |
|      46 |  128 | `				HashmapSortSwap(b,c);` |
|      46 |  129 | `				if( HashmapSortCmp(pCtx,a,b) > 0 ){` |
|      17 |  130 | `					HashmapSortSwap(a,b);` |
|       6 |  131 | `				}` |
|      13 |  132 | `			}` |
|      62 |  133 | `		}` |
|     111 |  134 | `	}` |
|     327 |  135 | `}` |
|       - |  136 | `/*` |
|       - |  137 | ` * Insertion sort, php's zend_insert_sort: the networks up to five entries, then` |
|       - |  138 | ` * a linear scan back for the first six and a two-at-a-time scan back after that` |
|       - |  139 | ` * (which is why the sixth element onwards costs a different number of` |
|       - |  140 | ` * comparisons from the fifth).` |
|       - |  141 | ` */` |
|    4307 |  142 | `static void HashmapInsertSort(HashmapSortEnt *aEnt,sxu32 n,HashmapSortCtx *pCtx)` |
|       5 |  143 | `{` |
|    4312 |  144 | `	switch( n ){` |
|       7 |  145 | `		case 0:` |
|       - |  146 | `		case 1:` |
|      17 |  147 | `			break;` |
|     247 |  148 | `		case 2:` |
|     501 |  149 | `			HashmapSort2(aEnt,&aEnt[1],pCtx);` |
|     501 |  150 | `			break;` |
|     522 |  151 | `		case 3:` |
|    1048 |  152 | `			HashmapSort3(aEnt,&aEnt[1],&aEnt[2],pCtx);` |
|    1048 |  153 | `			break;` |
|     212 |  154 | `		case 4:` |
|     431 |  155 | `			HashmapSort4(aEnt,&aEnt[1],&aEnt[2],&aEnt[3],pCtx);` |
|     431 |  156 | `			break;` |
|     155 |  157 | `		case 5:` |
|     319 |  158 | `			HashmapSort5(aEnt,&aEnt[1],&aEnt[2],&aEnt[3],&aEnt[4],pCtx);` |
|     319 |  159 | `			break;` |
|    1011 |  160 | `		default: {` |
|       - |  161 | `			HashmapSortEnt *i,*j,*k;` |
|    2021 |  162 | `			HashmapSortEnt *start = aEnt;` |
|    2021 |  163 | `			HashmapSortEnt *end = &aEnt[n];` |
|    2021 |  164 | `			HashmapSortEnt *sentry = &aEnt[6]; /* n >= 6 here, so this is in range */` |
|   12101 |  165 | `			for( i = start + 1 ; i < sentry ; i++ ){` |
|   10085 |  166 | `				j = i - 1;` |
|   10085 |  167 | `				if( !(HashmapSortCmp(pCtx,j,i) > 0) ){` |
|    6496 |  168 | `					continue;` |
|       - |  169 | `				}` |
|    7143 |  170 | `				while( j != start ){` |
|    6036 |  171 | `					j--;` |
|    6036 |  172 | `					if( !(HashmapSortCmp(pCtx,j,i) > 0) ){` |
|    2487 |  173 | `						j++;` |
|    2487 |  174 | `						break;` |
|       - |  175 | `					}` |
|       5 |  176 | `				}` |
|   10732 |  177 | `				for( k = i ; k > j ; k-- ){` |
|    7143 |  178 | `					HashmapSortSwap(k,k - 1);` |
|    3339 |  179 | `				}` |
|    1704 |  180 | `			}` |
|   11998 |  181 | `			for( i = sentry ; i < end ; i++ ){` |
|    9982 |  182 | `				j = i - 1;` |
|    9982 |  183 | `				if( !(HashmapSortCmp(pCtx,j,i) > 0) ){` |
|    5915 |  184 | `					continue;` |
|       - |  185 | `				}` |
|       - |  186 | `				/* j starts at i-3 >= start+3 and steps back by two, so the two` |
|       - |  187 | `				 * guards below are what keeps it from walking off the front. */` |
|    4843 |  188 | `				for( ;; ){` |
|    9959 |  189 | `					j -= 2;` |
|    9959 |  190 | `					if( !(HashmapSortCmp(pCtx,j,i) > 0) ){` |
|    3493 |  191 | `						j++;` |
|    3493 |  192 | `						if( !(HashmapSortCmp(pCtx,j,i) > 0) ){` |
|    1785 |  193 | `							j++;` |
|     878 |  194 | `						}` |
|    3493 |  195 | `						break;` |
|       - |  196 | `					}` |
|    6471 |  197 | `					if( j == start ){` |
|     163 |  198 | `						break;` |
|       - |  199 | `					}` |
|    6313 |  200 | `					if( j == start + 1 ){` |
|     426 |  201 | `						j--;` |
|     426 |  202 | `						if( HashmapSortCmp(pCtx,i,j) > 0 ){` |
|     234 |  203 | `							j++;` |
|     104 |  204 | `						}` |
|     426 |  205 | `						break;` |
|       - |  206 | `					}` |
|       5 |  207 | `				}` |
|   22971 |  208 | `				for( k = i ; k > j ; k-- ){` |
|   18904 |  209 | `					HashmapSortSwap(k,k - 1);` |
|    9182 |  210 | `				}` |
|    1994 |  211 | `			}` |
|    2016 |  212 | `			break;` |
|       - |  213 | `		}` |
|       - |  214 | `	}` |
|    4312 |  215 | `}` |
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
|    4307 |  227 | `static void HashmapZendSort(HashmapSortEnt *aEnt,sxu32 n,HashmapSortCtx *pCtx)` |
|       5 |  228 | `{` |
|    2956 |  229 | `	for(;;){` |
|    5907 |  230 | `		if( n <= 16 ){` |
|    4312 |  231 | `			HashmapInsertSort(aEnt,n,pCtx);` |
|    4312 |  232 | `			return;` |
|     ! 0 |  233 | `		}else{` |
|       - |  234 | `			HashmapSortEnt *i,*j;` |
|    1600 |  235 | `			HashmapSortEnt *start = aEnt;` |
|    1600 |  236 | `			HashmapSortEnt *end = &aEnt[n];` |
|    1600 |  237 | `			sxu32 offset = n >> 1;` |
|    1600 |  238 | `			HashmapSortEnt *pivot = &start[offset];` |
|    1600 |  239 | `			if( n >> 10 ){` |
|      13 |  240 | `				sxu32 delta = offset >> 1;` |
|      13 |  241 | `				HashmapSort5(start,&start[delta],pivot,&pivot[delta],end - 1,pCtx);` |
|       9 |  242 | `			}else{` |
|    1592 |  243 | `				HashmapSort3(start,pivot,end - 1,pCtx);` |
|       - |  244 | `			}` |
|    1600 |  245 | `			HashmapSortSwap(start + 1,pivot);` |
|    1600 |  246 | `			pivot = start + 1;` |
|    1600 |  247 | `			i = pivot + 1;` |
|    1600 |  248 | `			j = end - 1;` |
|    4593 |  249 | `			for(;;){` |
|   60447 |  250 | `				while( HashmapSortCmp(pCtx,pivot,i) > 0 ){` |
|   51411 |  251 | `					i++;` |
|   51411 |  252 | `					if( i == j ){` |
|     206 |  253 | `						goto done;` |
|       - |  254 | `					}` |
|       5 |  255 | `				}` |
|    9041 |  256 | `				j--;` |
|    9041 |  257 | `				if( j == i ){` |
|     161 |  258 | `					goto done;` |
|       - |  259 | `				}` |
|   59688 |  260 | `				while( HashmapSortCmp(pCtx,j,pivot) > 0 ){` |
|   51907 |  261 | `					j--;` |
|   51907 |  262 | `					if( j == i ){` |
|    1104 |  263 | `						goto done;` |
|       - |  264 | `					}` |
|       5 |  265 | `				}` |
|    7786 |  266 | `				HashmapSortSwap(i,j);` |
|    7786 |  267 | `				i++;` |
|    7786 |  268 | `				if( i == j ){` |
|     142 |  269 | `					goto done;` |
|       - |  270 | `				}` |
|       5 |  271 | `			}` |
|     792 |  272 | `done:` |
|    1600 |  273 | `			HashmapSortSwap(pivot,i - 1);` |
|    1600 |  274 | `			if( (i - 1) - start < end - i ){` |
|     385 |  275 | `				HashmapZendSort(start,(sxu32)(i - start) - 1,pCtx);` |
|     385 |  276 | `				aEnt = i;` |
|     385 |  277 | `				n = (sxu32)(end - i);` |
|     204 |  278 | `			}else{` |
|    1220 |  279 | `				HashmapZendSort(i,(sxu32)(end - i),pCtx);` |
|    1220 |  280 | `				n = (sxu32)(i - start) - 1;` |
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
|    2552 |  295 | `PH7_PRIVATE sxi32 HashmapNodeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData)` |
|       5 |  296 | `{` |
|       - |  297 | `	HashmapSortEnt *aEnt;` |
|       - |  298 | `	HashmapSortCtx sCtx;` |
|       - |  299 | `	ph7_hashmap_node *pEntry;` |
|       - |  300 | `	sxu32 n,i;` |
|    2557 |  301 | `	n = pMap->nEntry;` |
|    2557 |  302 | `	if( n < 2 ){` |
|     ! 0 |  303 | `		pMap->pCur = pMap->pFirst;` |
|     ! 0 |  304 | `		return SXRET_OK;` |
|       - |  305 | `	}` |
|    2557 |  306 | `	aEnt = (HashmapSortEnt *)SyMemBackendAlloc(&pMap->pVm->sAllocator,n * sizeof(HashmapSortEnt));` |
|    2557 |  307 | `	if( aEnt == 0 ){` |
|     ! 0 |  308 | `		return SXERR_MEM;` |
|       - |  309 | `	}` |
|       - |  310 | `	/* Collect the nodes in iteration order (pPrev is the FORWARD link here),` |
|       - |  311 | `	 * stamping each with its position: that stamp IS the sort's stability. */` |
|    2557 |  312 | `	i = 0;` |
|   33192 |  313 | `	for( pEntry = pMap->pFirst ; pEntry && i < n ; pEntry = pEntry->pPrev ){` |
|   30640 |  314 | `		aEnt[i].pNode = pEntry;` |
|   30640 |  315 | `		aEnt[i].nOrd = i;` |
|   30640 |  316 | `		i++;` |
|   15290 |  317 | `	}` |
|    2557 |  318 | `	n = i; /* Defensive: honour the list, not the counter, if they disagree */` |
|    2557 |  319 | `	if( n < 2 ){` |
|       - |  320 | `		/* An empty or single-node list has no order to decide, and the relink` |
|       - |  321 | `		 * below indexes the vector unconditionally. */` |
|     ! 0 |  322 | `		SyMemBackendFree(&pMap->pVm->sAllocator,(void *)aEnt);` |
|     ! 0 |  323 | `		pMap->pCur = pMap->pFirst;` |
|     ! 0 |  324 | `		return SXRET_OK;` |
|       - |  325 | `	}` |
|    2557 |  326 | `	sCtx.xCmp = xCmp;` |
|    2557 |  327 | `	sCtx.pCmpData = pCmpData;` |
|    2557 |  328 | `	sCtx.bStable = 1;` |
|    2557 |  329 | `	HashmapZendSort(aEnt,n,&sCtx);` |
|       - |  330 | `	/* Relink the map's list from the decided order */` |
|   33192 |  331 | `	for( i = 0 ; i < n ; i++ ){` |
|   30640 |  332 | `		aEnt[i].pNode->pPrev = ( i + 1 < n ) ? aEnt[i+1].pNode : 0;` |
|   30640 |  333 | `		aEnt[i].pNode->pNext = ( i > 0 ) ? aEnt[i-1].pNode : 0;` |
|   15290 |  334 | `	}` |
|    2557 |  335 | `	pMap->pFirst = aEnt[0].pNode;` |
|    2557 |  336 | `	pMap->pLast = aEnt[n-1].pNode;` |
|       - |  337 | `	/* php's sorts rewind the array's internal pointer */` |
|    2557 |  338 | `	pMap->pCur = pMap->pFirst;` |
|    2557 |  339 | `	SyMemBackendFree(&pMap->pVm->sAllocator,(void *)aEnt);` |
|    2557 |  340 | `	return SXRET_OK;` |
|    1275 |  341 | `}` |
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
|     256 |  352 | `PH7_PRIVATE void PH7_HashmapSortEntVector(HashmapSortEnt *aEnt,sxu32 n,ProcNodeCmp xCmp,void *pCmpData)` |
|       5 |  353 | `{` |
|       - |  354 | `	HashmapSortCtx sCtx;` |
|     261 |  355 | `	if( n < 2 ){` |
|     100 |  356 | `		return;` |
|       - |  357 | `	}` |
|     164 |  358 | `	sCtx.xCmp = xCmp;` |
|     164 |  359 | `	sCtx.pCmpData = pCmpData;` |
|     164 |  360 | `	sCtx.bStable = 0;` |
|     164 |  361 | `	HashmapZendSort(aEnt,n,&sCtx);` |
|     133 |  362 | `}` |
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
|    5020 |  380 | `static void HashmapSortInstall(ph7_value *pVal,ph7_hashmap *pMap)` |
|       5 |  381 | `{` |
|    5025 |  382 | `	if( (pVal->iFlags & MEMOBJ_HASHMAP) && (ph7_hashmap *)pVal->x.pOther == pMap ){` |
|    5015 |  383 | `		return; /* Untouched: the common case, and no work at all */` |
|       - |  384 | `	}` |
|      11 |  385 | `	PH7_MemObjRelease(pVal);` |
|      11 |  386 | `	MemObjSetType(pVal,MEMOBJ_HASHMAP);` |
|      11 |  387 | `	pVal->x.pOther = pMap;` |
|      11 |  388 | `	pMap->iRef++;` |
|    2503 |  389 | `}` |
|    2552 |  390 | `static void HashmapSortDrive(` |
|       - |  391 | `	ph7_vm *pVm,` |
|       - |  392 | `	ph7_value *pArray,        /* The by-reference array argument */` |
|       - |  393 | `	ph7_hashmap *pMap,        /* Its map, already COW-separated by the driver */` |
|       - |  394 | `	ProcNodeCmp xCmp,void *pCmpData` |
|       - |  395 | `	)` |
|       5 |  396 | `{` |
|    2557 |  397 | `	ph7_value *pBacking = 0;` |
|    2557 |  398 | `	pMap->iRef++;` |
|    2557 |  399 | `	HashmapNodeSort(pMap,xCmp,pCmpData);` |
|    2557 |  400 | `	if( pArray->nIdx != SXU32_HIGH ){` |
|       - |  401 | `		/* The argument may be a stack copy of the caller's variable, and a` |
|       - |  402 | `		 * comparator's write lands on the variable: install into both. */` |
|    2531 |  403 | `		pBacking = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pArray->nIdx);` |
|    1257 |  404 | `	}` |
|    2557 |  405 | `	if( pBacking && pBacking != pArray ){` |
|    2473 |  406 | `		HashmapSortInstall(pBacking,pMap);` |
|    1228 |  407 | `	}` |
|    2557 |  408 | `	HashmapSortInstall(pArray,pMap);` |
|    2557 |  409 | `	PH7_HashmapUnref(pMap);` |
|    2557 |  410 | `}` |
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
|     588 |  427 | `static void HashmapFlagStringify(ph7_value *pVal)` |
|       5 |  428 | `{` |
|     593 |  429 | `	ph7_vm *pVm = pVal->pVm;` |
|     593 |  430 | `	sxi32 rc = PH7_EXCEPTION;` |
|     588 |  431 | `	if( (pVal->iFlags & MEMOBJ_OBJ) && pVm` |
|     177 |  432 | `	 && (pVm->iCmpCallbackExc != 0 \|\| PH7_CALLBACK_UNWOUND(pVm->nBoundaryRc)) ){` |
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
|     571 |  448 | `	if( !PH7_MemObjIsNotStringable(pVal) ){` |
|     533 |  449 | `		rc = PH7_MemObjToStringUV(pVal);` |
|     533 |  450 | `		if( rc == SXRET_OK ){` |
|     525 |  451 | `			return;` |
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
|     297 |  465 | `}` |
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
| 1490664 |  478 | `static sxi32 HashmapScalarFlagCmp(ph7_value *pA,ph7_value *pB,int base,int bFold)` |
|       5 |  479 | `{` |
|       - |  480 | `	sxi32 rc;` |
| 1490669 |  481 | `	if( base == 1 ){` |
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
| 1490429 |  504 | `		if( (pA->iFlags & MEMOBJ_STRING) == 0 ){ HashmapFlagStringify(pA); }` |
| 1490429 |  505 | `		if( (pB->iFlags & MEMOBJ_STRING) == 0 ){ HashmapFlagStringify(pB); }` |
| 1490429 |  506 | `		zA = (const char *)SyBlobData(&pA->sBlob);` |
| 1490429 |  507 | `		zB = (const char *)SyBlobData(&pB->sBlob);` |
| 1490429 |  508 | `		nA = SyBlobLength(&pA->sBlob);` |
| 1490429 |  509 | `		nB = SyBlobLength(&pB->sBlob);` |
| 1490429 |  510 | `		if( base == 6 ){` |
|     196 |  511 | `			rc = PH7_StrNatCmp(zA,(int)nA,zB,(int)nB,bFold);` |
|     100 |  512 | `		}else{` |
|       - |  513 | `			/* Lexicographic comparison (binary-safe), case-folded on request. */` |
| 1490237 |  514 | `			nMin = nA < nB ? nA : nB;` |
| 1490237 |  515 | `			rc = 0;` |
| 3053220 |  516 | `			for( i = 0 ; i < nMin ; ++i ){` |
| 3050538 |  517 | `				int ca = (unsigned char)zA[i];` |
| 3050538 |  518 | `				int cb = (unsigned char)zB[i];` |
| 3050538 |  519 | `				if( bFold ){ ca = SyToLower(ca); cb = SyToLower(cb); }` |
| 3050538 |  520 | `				if( ca != cb ){ rc = ca < cb ? -1 : 1; break; }` |
|  802029 |  521 | `			}` |
| 1490237 |  522 | `			if( rc == 0 ){` |
|    2687 |  523 | `				if( nA < nB ) rc = -1;` |
|     795 |  524 | `				else if( nA > nB ) rc = 1;` |
|    1334 |  525 | `			}` |
|       - |  526 | `		}` |
|       - |  527 | `	}` |
| 1490669 |  528 | `	return rc;` |
|       5 |  529 | `}` |
|       - |  530 | `/*` |
|       - |  531 | ` * Are two live values equal under an array_unique() sort_flags? A non-mutating` |
|       - |  532 | ` * wrapper (works on private copies): base 0 = SORT_REGULAR loose comparison,` |
|       - |  533 | ` * explicit flags route through HashmapScalarFlagCmp. Used by array_unique.` |
|       - |  534 | ` */` |
| 1415922 |  535 | `PH7_PRIVATE int HashmapValueFlagEqual(ph7_vm *pVm,ph7_value *pA,ph7_value *pB,int base,int bFold)` |
|       5 |  536 | `{` |
|       - |  537 | `	ph7_value sA,sB;` |
|       - |  538 | `	sxi32 rc;` |
| 1415927 |  539 | `	PH7_MemObjInit(pVm,&sA);` |
| 1415927 |  540 | `	PH7_MemObjInit(pVm,&sB);` |
| 1415927 |  541 | `	PH7_MemObjStore(pA,&sA);` |
| 1415927 |  542 | `	PH7_MemObjStore(pB,&sB);` |
| 1415927 |  543 | `	if( base == 0 ){` |
|      18 |  544 | `		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0); /* SORT_REGULAR: loose comparison */` |
|      10 |  545 | `	}else{` |
| 1415911 |  546 | `		rc = HashmapScalarFlagCmp(&sA,&sB,base,bFold);` |
|       - |  547 | `	}` |
| 1415927 |  548 | `	PH7_MemObjRelease(&sA);` |
| 1415927 |  549 | `	PH7_MemObjRelease(&sB);` |
| 1415927 |  550 | `	return rc == 0;` |
|       5 |  551 | `}` |
|       - |  552 | `/*` |
|       - |  553 | ` * Compare two node VALUES under php's sort_flags (base 0 = SORT_REGULAR uses the` |
|       - |  554 | ` * standard value comparison; explicit flags route through HashmapScalarFlagCmp).` |
|       - |  555 | ` */` |
|   74728 |  556 | `static sxi32 HashmapFlagValueCmp(ph7_hashmap_node *pA,ph7_hashmap_node *pB,sxi32 iFlags)` |
|       5 |  557 | `{` |
|       - |  558 | `	ph7_value sA,sB;` |
|   74733 |  559 | `	int base = iFlags & ~8;      /* strip SORT_FLAG_CASE */` |
|   74733 |  560 | `	int bFold = (iFlags & 8) != 0;` |
|       - |  561 | `	sxi32 rc;` |
|   74733 |  562 | `	if( base == 0 ){` |
|       - |  563 | `		/* SORT_REGULAR */` |
|      94 |  564 | `		return HashmapNodeCmp(pA,pB,FALSE,0);` |
|       - |  565 | `	}` |
|   74643 |  566 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|   74643 |  567 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|   74643 |  568 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|   74643 |  569 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|   74643 |  570 | `	rc = HashmapScalarFlagCmp(&sA,&sB,base,bFold);` |
|   74643 |  571 | `	PH7_MemObjRelease(&sA);` |
|   74643 |  572 | `	PH7_MemObjRelease(&sB);` |
|   74643 |  573 | `	return rc;` |
|   36855 |  574 | `}` |
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
|  168244 |  594 | `static void HashmapCmpLatch(ph7_vm *pVm)` |
|       5 |  595 | `{` |
|  168244 |  596 | `	if( PH7_CALLBACK_UNWOUND(pVm->nBoundaryRc)` |
|   83695 |  597 | `	 && (pVm->iCmpCallbackExc == 0 \|\| pVm->nBoundaryRc == PH7_ABORT) ){` |
|     121 |  598 | `		pVm->iCmpCallbackExc = pVm->nBoundaryRc;` |
|      75 |  599 | `	}` |
|  168219 |  600 | `}` |
|  167210 |  601 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       5 |  602 | `{` |
|  167215 |  603 | `	ph7_vm *pVm = pA->pMap->pVm;` |
|       - |  604 | `	sxi32 rc;` |
|  167215 |  605 | `	if( pCmpData == 0 ){` |
|       - |  606 | `		/* SORT_REGULAR fast path */` |
|   92981 |  607 | `		rc = HashmapNodeCmp(pA,pB,FALSE,0);` |
|   46496 |  608 | `	}else{` |
|   74239 |  609 | `		rc = HashmapFlagValueCmp(pA,pB,SX_PTR_TO_INT(pCmpData));` |
|       - |  610 | `	}` |
|  167215 |  611 | `	HashmapCmpLatch(pVm);` |
|  167215 |  612 | `	return rc;` |
|       5 |  613 | `}` |
|       - |  614 | `/*` |
|       - |  615 | ` * Materialise a node's KEY as a scalar ph7_value (int key -> integer, string key` |
|       - |  616 | ` * -> string) for a flag-aware key comparison.` |
|       - |  617 | ` */` |
|     900 |  618 | `static void HashmapNodeKeyToValue(ph7_hashmap_node *pNode,ph7_value *pOut)` |
|       3 |  619 | `{` |
|     903 |  620 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|     126 |  621 | `		PH7_MemObjInitFromInt(pNode->pMap->pVm,pOut,pNode->xKey.iKey);` |
|      64 |  622 | `	}else{` |
|     779 |  623 | `		PH7_MemObjInitFromString(pNode->pMap->pVm,pOut,0);` |
|    1167 |  624 | `		PH7_MemObjStringAppend(pOut,(const char *)SyBlobData(&pNode->xKey.sKey),` |
|     388 |  625 | `			SyBlobLength(&pNode->xKey.sKey));` |
|       - |  626 | `	}` |
|     903 |  627 | `}` |
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
|     388 |  639 | `static sxi32 HashmapKeyNodeCmp(ph7_hashmap_node *pA,ph7_hashmap_node *pB)` |
|       3 |  640 | `{` |
|       - |  641 | `	ph7_value sA,sB;` |
|       - |  642 | `	sxi32 rc;` |
|     391 |  643 | `	if( pA->iType == HASHMAP_INT_NODE && pB->iType == HASHMAP_INT_NODE ){` |
|       - |  644 | `		/* Two integer keys: the common case, and no allocation needed */` |
|      59 |  645 | `		return pA->xKey.iKey < pB->xKey.iKey ? -1 : (pA->xKey.iKey > pB->xKey.iKey ? 1 : 0);` |
|       - |  646 | `	}` |
|     333 |  647 | `	HashmapNodeKeyToValue(pA,&sA);` |
|     333 |  648 | `	HashmapNodeKeyToValue(pB,&sB);` |
|     333 |  649 | `	rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|     333 |  650 | `	PH7_MemObjRelease(&sA);` |
|     333 |  651 | `	PH7_MemObjRelease(&sB);` |
|     333 |  652 | `	return rc;` |
|     197 |  653 | `}` |
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
|     440 |  679 | `static sxi32 HashmapCmpCallback2(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       3 |  680 | `{` |
|     443 |  681 | `	ph7_vm *pVm = pA->pMap->pVm;` |
|       - |  682 | `	sxi32 rc;` |
|     443 |  683 | `	if( pCmpData == 0 ){` |
|     329 |  684 | `		rc = HashmapKeyNodeCmp(pA,pB);` |
|     166 |  685 | `	}else{` |
|     117 |  686 | `		rc = HashmapFlagKeyCmp(pA,pB,SX_PTR_TO_INT(pCmpData));` |
|       - |  687 | `	}` |
|     443 |  688 | `	HashmapCmpLatch(pVm);` |
|     443 |  689 | `	return rc;` |
|       3 |  690 | `}` |
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
|       - |  709 | ` * One comparison asked of a user callback, reduced to -1/0/1 the way php's` |
|       - |  710 | ` * php_array_user_compare_unstable() does. Shared by the three sorts and by the` |
|       - |  711 | ` * diff/intersect family.` |
|       - |  712 | ` *` |
|       - |  713 | ` * The reduction is by SIGN over the full 64 bits: a bare (int) cast made a` |
|       - |  714 | `` * callback answering `($a <=> $b) << 32` compare every pair equal.`` |
|       - |  715 | ` *` |
|       - |  716 | ` * With bBoolRetry, a callback answering a BOOL is php 8's deprecated` |
|       - |  717 | `` * `return $a > $b;` comparator. php raises "Returning bool from comparison`` |
|       - |  718 | ` * function is deprecated" once per builtin call (bCmpBoolRaised), and because` |
|       - |  719 | `` * `false` cannot tell "less" from "equal" it asks again with the operands`` |
|       - |  720 | ` * SWAPPED and answers the negation of that: true means greater, false-then-true` |
|       - |  721 | `` * means less, false-then-false means equal. Without the retry `false` read as`` |
|       - |  722 | ` * equal, which every sort of this shape survives by luck and the merge in` |
|       - |  723 | ` * array_udiff() and its neighbours does not -- a whole diff came back empty.` |
|       - |  724 | ` * array_udiff_assoc() and array_uintersect_assoc() ask their value callback` |
|       - |  725 | ` * through php's zval_user_compare(), which does neither: they pass FALSE.` |
|       - |  726 | ` *` |
|       - |  727 | ` * Returns the dispatch status. A callback that did not return leaves *pCmp 0;` |
|       - |  728 | ` * a deprecation whose error handler threw leaves the pair's answer as php has` |
|       - |  729 | ` * it at that moment (the swapped call is never made with an exception` |
|       - |  730 | ` * pending) and returns that status for the caller to latch.` |
|       - |  731 | ` */` |
|    2758 |  732 | `PH7_PRIVATE sxi32 PH7_HashmapUserCmp(ph7_context *pCtx,ph7_value *pCallback,ph7_value *pA,ph7_value *pB,` |
|       - |  733 | `	int bBoolRetry,int *pCmp)` |
|       5 |  734 | `{` |
|    2763 |  735 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - |  736 | `	ph7_value *apArg[2];` |
|       - |  737 | `	ph7_value sResult;` |
|    2763 |  738 | `	int bNegate = 0;` |
|       - |  739 | `	sxi32 rc;` |
|    2763 |  740 | `	*pCmp = 0;` |
|    2763 |  741 | `	PH7_MemObjInit(pVm,&sResult);` |
|    2763 |  742 | `	apArg[0] = pA;` |
|    2763 |  743 | `	apArg[1] = pB;` |
|    2763 |  744 | `	rc = PH7_VmCallCallbackByValue(pVm,pCallback,2,apArg,&sResult,0);` |
|    2763 |  745 | `	if( PH7_CALLBACK_UNWOUND(rc) ){` |
|      61 |  746 | `		PH7_MemObjRelease(&sResult);` |
|      61 |  747 | `		return rc;` |
|       - |  748 | `	}` |
|    2705 |  749 | `	if( rc == SXRET_OK && bBoolRetry && (sResult.iFlags & MEMOBJ_BOOL) ){` |
|     263 |  750 | `		int bTrue = sResult.x.iVal != 0;` |
|     263 |  751 | `		if( !pVm->bCmpBoolRaised ){` |
|       - |  752 | `			SyString sName;` |
|      45 |  753 | `			pVm->bCmpBoolRaised = 1;` |
|      45 |  754 | `			if( pVm->pNativeCall && pVm->pNativeCall->pName ){` |
|       - |  755 | `				/* ArrayObject::uasort() runs php's uasort(), which names itself */` |
|      45 |  756 | `				sName = *pVm->pNativeCall->pName;` |
|      23 |  757 | `			}else{` |
|     ! 0 |  758 | `				sName = pCtx->pFunc->sName;` |
|       - |  759 | `			}` |
|      45 |  760 | `			rc = PH7_VmThrowError(pVm,&sName,8192 /* E_DEPRECATED */,` |
|       - |  761 | `				"Returning bool from comparison function is deprecated, "` |
|       - |  762 | `				"return an integer less than, equal to, or greater than zero");` |
|      45 |  763 | `			if( !PH7_CALLBACK_UNWOUND(rc) && PH7_CALLBACK_UNWOUND(pVm->nBoundaryRc) ){` |
|       - |  764 | `				/* an error handler's throw is parked, not returned */` |
|       9 |  765 | `				rc = pVm->nBoundaryRc;` |
|       4 |  766 | `			}` |
|      45 |  767 | `			if( PH7_CALLBACK_UNWOUND(rc) ){` |
|       9 |  768 | `				*pCmp = bTrue;` |
|       9 |  769 | `				PH7_MemObjRelease(&sResult);` |
|       9 |  770 | `				return rc;` |
|       - |  771 | `			}` |
|      37 |  772 | `			rc = SXRET_OK;` |
|      18 |  773 | `		}` |
|     255 |  774 | `		if( !bTrue ){` |
|     167 |  775 | `			PH7_MemObjRelease(&sResult);` |
|     167 |  776 | `			PH7_MemObjInit(pVm,&sResult);` |
|     167 |  777 | `			apArg[0] = pB;` |
|     167 |  778 | `			apArg[1] = pA;` |
|     167 |  779 | `			rc = PH7_VmCallCallbackByValue(pVm,pCallback,2,apArg,&sResult,0);` |
|     167 |  780 | `			if( PH7_CALLBACK_UNWOUND(rc) ){` |
|     ! 0 |  781 | `				PH7_MemObjRelease(&sResult);` |
|     ! 0 |  782 | `				return rc;` |
|       - |  783 | `			}` |
|     167 |  784 | `			bNegate = 1;` |
|      83 |  785 | `		}` |
|     127 |  786 | `	}` |
|    2697 |  787 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  788 | `		*pCmp = -1; /* a failed dispatch compares unequal */` |
|     ! 0 |  789 | `	}else{` |
|    2697 |  790 | `		if( (sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|     263 |  791 | `			PH7_MemObjToInteger(&sResult);` |
|     131 |  792 | `		}` |
|    2697 |  793 | `		*pCmp = (sResult.x.iVal < 0) ? -1 : (sResult.x.iVal > 0 ? 1 : 0);` |
|    2697 |  794 | `		if( bNegate ){` |
|     167 |  795 | `			*pCmp = -*pCmp;` |
|      83 |  796 | `		}` |
|       - |  797 | `	}` |
|    2697 |  798 | `	PH7_MemObjRelease(&sResult);` |
|    2697 |  799 | `	return SXRET_OK;` |
|    1359 |  800 | `}` |
|       - |  801 | `/*` |
|       - |  802 | ` * What usort()/uasort()/uksort() hand their node comparison: the callback and` |
|       - |  803 | ` * the context the deprecation above is raised from.` |
|       - |  804 | ` */` |
|       - |  805 | `typedef struct HashmapUserSort HashmapUserSort;` |
|       - |  806 | `struct HashmapUserSort {` |
|       - |  807 | `	ph7_context *pCtx;` |
|       - |  808 | `	ph7_value *pCallback;` |
|       - |  809 | `};` |
|       - |  810 | `/*` |
|       - |  811 | ` * The comparator did not RETURN (or its deprecation's handler threw): latch the` |
|       - |  812 | ` * STATUS so the sort driver aborts and propagates exactly it (an UNCAUGHT throw` |
|       - |  813 | ` * is PH7_ABORT, and testing only PH7_EXCEPTION left the sort running --` |
|       - |  814 | ` * re-entering the comparator, and the fatal report, for every remaining pair).` |
|       - |  815 | ` */` |
|    1992 |  816 | `static sxi32 HashmapUserSortCmp(ph7_vm *pVm,HashmapUserSort *pSort,ph7_value *pA,ph7_value *pB)` |
|       5 |  817 | `{` |
|    1997 |  818 | `	int iCmp = 0;` |
|    1997 |  819 | `	sxi32 rc = PH7_HashmapUserCmp(pSort->pCtx,pSort->pCallback,pA,pB,TRUE,&iCmp);` |
|    1997 |  820 | `	if( rc != SXRET_OK ){` |
|      49 |  821 | `		pVm->iCmpCallbackExc = rc;` |
|      23 |  822 | `	}` |
|    1997 |  823 | `	return (sxi32)iCmp;` |
|       5 |  824 | `}` |
|       - |  825 | `/*` |
|       - |  826 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|       - |  827 | ` * used-by: [usort(),uasort()]` |
|       - |  828 | ` */` |
|    1972 |  829 | `static sxi32 HashmapCmpCallback4(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       5 |  830 | `{` |
|    1977 |  831 | `	ph7_vm *pVm = pA->pMap->pVm;` |
|    1977 |  832 | `	if( pVm->iCmpCallbackExc ){` |
|       - |  833 | `		/* A previous comparison already raised: stop invoking the callback so` |
|       - |  834 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|      28 |  835 | `		return 0;` |
|       - |  836 | `	}` |
|    2899 |  837 | `	return HashmapUserSortCmp(pVm,(HashmapUserSort *)pCmpData,` |
|     948 |  838 | `		HashmapExtractNodeValue(pA),HashmapExtractNodeValue(pB));` |
|     966 |  839 | `}` |
|       - |  840 | `/*` |
|       - |  841 | ` * Node comparison callback: Compare nodes by keys only.` |
|       - |  842 | ` * used-by: [krsort()]` |
|       - |  843 | ` */` |
|      68 |  844 | `static sxi32 HashmapCmpCallback5(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       2 |  845 | `{` |
|      70 |  846 | `	if( pCmpData == 0 ){` |
|      63 |  847 | `		return -HashmapKeyNodeCmp(pA,pB); /* Reverse result */` |
|       - |  848 | `	}` |
|       8 |  849 | `	return -HashmapFlagKeyCmp(pA,pB,SX_PTR_TO_INT(pCmpData));` |
|      36 |  850 | `}` |
|       - |  851 | `/*` |
|       - |  852 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|       - |  853 | ` * used-by: [uksort()]` |
|       - |  854 | ` */` |
|      54 |  855 | `static sxi32 HashmapCmpCallback6(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       3 |  856 | `{` |
|      57 |  857 | `	ph7_vm *pVm = pA->pMap->pVm;` |
|       - |  858 | `	ph7_value sK1,sK2;` |
|       - |  859 | `	sxi32 rc;` |
|      57 |  860 | `	if( pVm->iCmpCallbackExc ){` |
|       - |  861 | `		/* A previous comparison already raised: stop invoking the callback so` |
|       - |  862 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|      10 |  863 | `		return 0;` |
|       - |  864 | `	}` |
|      49 |  865 | `	PH7_MemObjInit(pVm,&sK1);` |
|      49 |  866 | `	PH7_MemObjInit(pVm,&sK2);` |
|       - |  867 | `	/* Extract nodes keys */` |
|      49 |  868 | `	PH7_HashmapExtractNodeKey(pA,&sK1);` |
|      49 |  869 | `	PH7_HashmapExtractNodeKey(pB,&sK2);` |
|       - |  870 | `	/* Mark keys as constants */` |
|      49 |  871 | `	sK1.nIdx = SXU32_HIGH;` |
|      49 |  872 | `	sK2.nIdx = SXU32_HIGH;` |
|      49 |  873 | `	rc = HashmapUserSortCmp(pVm,(HashmapUserSort *)pCmpData,&sK1,&sK2);` |
|      49 |  874 | `	PH7_MemObjRelease(&sK1);` |
|      49 |  875 | `	PH7_MemObjRelease(&sK2);` |
|      49 |  876 | `	return rc;` |
|      30 |  877 | `}` |
|       - |  878 | `/*` |
|       - |  879 | ` * Permute a hashmap's entries the way php's shuffle() does: Fisher-Yates over` |
|       - |  880 | ` * the buckets, drawing each index from the MT19937 generator through` |
|       - |  881 | ` * php_mt_rand_range(). The caller rehashes afterwards (php reindexes the array` |
|       - |  882 | ` * 0..n-1 and drops string keys).` |
|       - |  883 | ` *` |
|       - |  884 | ` * This used to be a sort with a coin-flip comparator, which is a permuting` |
|       - |  885 | ` * shuffle but not a UNIFORM one — the distribution a random comparator produces` |
|       - |  886 | ` * is skewed and depends on the sort's internals — and it consumed the generator` |
|       - |  887 | ` * in a different order, so a seeded run answered a different permutation from` |
|       - |  888 | ` * php's for every seed.` |
|       - |  889 | ` */` |
|       8 |  890 | `PH7_PRIVATE sxi32 PH7_HashmapShuffle(ph7_hashmap *pMap)` |
|       2 |  891 | `{` |
|       - |  892 | `	ph7_hashmap_node **apNode,*pNode;` |
|       - |  893 | `	sxu32 n,nLeft;` |
|       - |  894 | `	SySet aNode;` |
|      10 |  895 | `	if( pMap->nEntry < 2 ){` |
|     ! 0 |  896 | `		return SXRET_OK;` |
|       - |  897 | `	}` |
|      10 |  898 | `	SySetInit(&aNode,&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|      50 |  899 | `	for( pNode = pMap->pFirst ; pNode ; pNode = pNode->pPrev ){` |
|      42 |  900 | `		if( SySetPut(&aNode,(const void *)&pNode) != SXRET_OK ){` |
|     ! 0 |  901 | `			SySetRelease(&aNode);` |
|     ! 0 |  902 | `			return SXERR_MEM;` |
|       - |  903 | `		}` |
|      22 |  904 | `	}` |
|      10 |  905 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&aNode);` |
|      10 |  906 | `	n = SySetUsed(&aNode);` |
|       - |  907 | `	/* php walks DOWN from the last index, swapping with a draw in [0,n_left]. */` |
|      42 |  908 | `	for( nLeft = n - 1 ; nLeft > 0 ; --nLeft ){` |
|      34 |  909 | `		sxu32 nPick = (sxu32)PH7_VmMtRandRange(pMap->pVm,0,(sxi64)nLeft);` |
|      34 |  910 | `		if( nPick != nLeft ){` |
|      24 |  911 | `			ph7_hashmap_node *pTmp = apNode[nLeft];` |
|      24 |  912 | `			apNode[nLeft] = apNode[nPick];` |
|      24 |  913 | `			apNode[nPick] = pTmp;` |
|      11 |  914 | `		}` |
|      18 |  915 | `	}` |
|       - |  916 | `	/* Relink in the new order. pPrev is the forward link and pNext the back one` |
|       - |  917 | `	 * (the whole map is built that way); the rehash after this fixes pLast. */` |
|      50 |  918 | `	for( n = 0 ; n < SySetUsed(&aNode) ; ++n ){` |
|      42 |  919 | `		apNode[n]->pPrev = (n + 1 < SySetUsed(&aNode)) ? apNode[n+1] : 0;` |
|      42 |  920 | `		apNode[n]->pNext = (n > 0) ? apNode[n-1] : 0;` |
|      22 |  921 | `	}` |
|      10 |  922 | `	pMap->pFirst = apNode[0];` |
|      10 |  923 | `	pMap->pLast = apNode[SySetUsed(&aNode) - 1];` |
|      10 |  924 | `	pMap->pCur = pMap->pFirst;` |
|      10 |  925 | `	SySetRelease(&aNode);` |
|      10 |  926 | `	return SXRET_OK;` |
|       6 |  927 | `}` |
|       - |  928 | `/*` |
|       - |  929 | ` * Rehash all nodes keys after a sort have been applied.` |
|       - |  930 | ` * Used by [sort(),usort() and rsort()].` |
|       - |  931 | ` */` |
|    2392 |  932 | `PH7_PRIVATE void HashmapSortRehash(ph7_hashmap *pMap)` |
|       5 |  933 | `{` |
|       - |  934 | `	ph7_hashmap_node *p,*pLast;` |
|       - |  935 | `	sxu32 i;` |
|       - |  936 | `	/* php's sorts rewind the array's internal pointer. The reordering paths reset` |
|       - |  937 | `	 * it themselves, but a ONE-element array is reindexed without being reordered` |
|       - |  938 | ``	 * — and `$a = ['x'=>1]; next($a); sort($a);` then left current() past the end,`` |
|       - |  939 | `	 * answering false where php answers the element. */` |
|    2397 |  940 | `	pMap->pCur = pMap->pFirst;` |
|       - |  941 | `	/* Rehash all entries */` |
|    2397 |  942 | `	pLast = p = pMap->pFirst;` |
|    2397 |  943 | `	pMap->iNextIdx = 0;` |
|    2397 |  944 | `	pMap->bIntKeySeen = 0; /* Reset the automatic index */` |
|    2397 |  945 | `	i = 0;` |
|   16029 |  946 | `	for( ;; ){` |
|   32140 |  947 | `		if( i >= pMap->nEntry ){` |
|    2397 |  948 | `			pMap->pLast = pLast;` |
|    2397 |  949 | `			break;` |
|       - |  950 | `		}` |
|   29748 |  951 | `		if( p->iType == HASHMAP_BLOB_NODE ){` |
|       - |  952 | `			/* Do not maintain index association as requested by the PHP specification */` |
|      50 |  953 | `			SyBlobRelease(&p->xKey.sKey);` |
|       - |  954 | `			/* Change key type */` |
|      50 |  955 | `			p->iType = HASHMAP_INT_NODE;` |
|      24 |  956 | `		}` |
|   29748 |  957 | `		HashmapRehashIntNode(p);` |
|       - |  958 | `		/* Point to the next entry */` |
|   29748 |  959 | `		i++;` |
|   29748 |  960 | `		pLast = p;` |
|   29748 |  961 | `		p = p->pPrev; /* Reverse link */` |
|       5 |  962 | `	}` |
|    2397 |  963 | `}` |
|       - |  964 | `/*` |
|       - |  965 | ` * Array functions implementation.` |
|       - |  966 | ` * Status:` |
|       - |  967 | ` *  Stable.` |
|       - |  968 | ` */` |
|       - |  969 | `/*` |
|       - |  970 | ` * Reset / report the comparator-throw flag around a FLAG sort. The string sort` |
|       - |  971 | ` * flags coerce their operands user-visibly (HashmapScalarFlagCmp), and a` |
|       - |  972 | ` * not-stringable object raises php's Error there; the comparator can only flag` |
|       - |  973 | ` * it, so every flag-sort driver clears the flag before its sort and` |
|       - |  974 | ` * answers PH7_EXCEPTION after it. php's array is sorted after the throw too, so` |
|       - |  975 | ` * the rehash still runs.` |
|       - |  976 | ` */` |
|    2843 |  977 | `static sxi32 HashmapFlagSortStatus(ph7_context *pCtx)` |
|       5 |  978 | `{` |
|    2848 |  979 | `	if( pCtx->pVm->iCmpCallbackExc ){` |
|      80 |  980 | `		sxi32 rcExc = pCtx->pVm->iCmpCallbackExc;` |
|      80 |  981 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|      80 |  982 | `		pCtx->nThrowRc = rcExc;` |
|      80 |  983 | `		return rcExc;` |
|       - |  984 | `	}` |
|    2770 |  985 | `	return PH7_OK;` |
|    1424 |  986 | `}` |
|       - |  987 | `/*` |
|       - |  988 | ` * bool sort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - |  989 | ` * Sort an array.` |
|       - |  990 | ` * Parameters` |
|       - |  991 | ` *  $array` |
|       - |  992 | ` *   The input array.` |
|       - |  993 | ` * $sort_flags` |
|       - |  994 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - |  995 | ` *  Sorting type flags:` |
|       - |  996 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - |  997 | ` *   SORT_NUMERIC - compare items numerically` |
|       - |  998 | ` *   SORT_STRING - compare items as strings` |
|       - |  999 | ` * Return` |
|       - | 1000 | ` *  TRUE on success or FALSE on failure.` |
|       - | 1001 | ` *` |
|       - | 1002 | ` */` |
|    2241 | 1003 | `PH7_PRIVATE int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1004 | `{` |
|       - | 1005 | `	ph7_hashmap *pMap;` |
|       - | 1006 | `	/* Make sure we are dealing with a valid hashmap */` |
|    2246 | 1007 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 1008 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 1009 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1010 | `		return PH7_OK;` |
|       - | 1011 | `	}` |
|       - | 1012 | `	/* Point to the internal representation of the input hashmap */` |
|    2246 | 1013 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|    2246 | 1014 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|    2246 | 1015 | `	if( pMap->nEntry > 1 ){` |
|    1958 | 1016 | `		sxi32 iCmpFlags = 0;` |
|    1958 | 1017 | `		if( nArg > 1 ){` |
|       - | 1018 | `			/* Extract comparison flags */` |
|    1746 | 1019 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     868 | 1020 | `		}` |
|       - | 1021 | `		/* Decide the order */` |
|    1958 | 1022 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|    1958 | 1023 | `		HashmapSortDrive(pCtx->pVm,apArg[0],pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 1024 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|    1958 | 1025 | `		HashmapSortRehash(pMap);` |
|    1267 | 1026 | `	}else if( pMap->nEntry == 1 ){` |
|       - | 1027 | `		/* php reindexes even a single-element array: a string key becomes 0 */` |
|     157 | 1028 | `		HashmapSortRehash(pMap);` |
|      77 | 1029 | `	}` |
|       - | 1030 | `	{` |
|    2246 | 1031 | `		sxi32 rcCmp = HashmapFlagSortStatus(pCtx);` |
|    2246 | 1032 | `		if( rcCmp != PH7_OK ){` |
|      38 | 1033 | `			return rcCmp;` |
|       - | 1034 | `		}` |
|       - | 1035 | `	}` |
|       - | 1036 | `	/* All done,return TRUE */` |
|    2210 | 1037 | `	ph7_result_bool(pCtx,1);` |
|    2210 | 1038 | `	return PH7_OK;` |
|    1123 | 1039 | `}` |
|       - | 1040 | `/*` |
|       - | 1041 | ` * bool asort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 1042 | ` *  Sort an array and maintain index association.` |
|       - | 1043 | ` * Parameters` |
|       - | 1044 | ` *  $array` |
|       - | 1045 | ` *   The input array.` |
|       - | 1046 | ` * $sort_flags` |
|       - | 1047 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 1048 | ` *  Sorting type flags:` |
|       - | 1049 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 1050 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 1051 | ` *   SORT_STRING - compare items as strings` |
|       - | 1052 | ` * Return` |
|       - | 1053 | ` *  TRUE on success or FALSE on failure.` |
|       - | 1054 | ` */` |
|      74 | 1055 | `PH7_PRIVATE int ph7_hashmap_asort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1056 | `{` |
|       - | 1057 | `	char zGiven[64];` |
|       - | 1058 | `	ph7_hashmap *pMap;` |
|       - | 1059 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|      79 | 1060 | `	if( nArg < 1 ){` |
|     ! 0 | 1061 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1062 | `			"ArgumentCountError",` |
|       - | 1063 | `			"asort() expects at least 1 argument, 0 given"` |
|       - | 1064 | `			);` |
|       - | 1065 | `	}` |
|       - | 1066 | `	/* PHP 8: TypeError if first argument is not an array */` |
|      79 | 1067 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 1068 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1069 | `			"TypeError",` |
|       - | 1070 | `			"asort(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 1071 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 1072 | `			);` |
|       - | 1073 | `	}` |
|       - | 1074 | `	/* Point to the internal representation of the input hashmap */` |
|      79 | 1075 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      79 | 1076 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      79 | 1077 | `	if( pMap->nEntry > 1 ){` |
|      73 | 1078 | `		sxi32 iCmpFlags = 0;` |
|      73 | 1079 | `		if( nArg > 1 ){` |
|       - | 1080 | `			/* Extract comparison flags */` |
|      42 | 1081 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|      19 | 1082 | `		}` |
|       - | 1083 | `		/* Decide the order */` |
|      73 | 1084 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|      73 | 1085 | `		HashmapSortDrive(pCtx->pVm,apArg[0],pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|      34 | 1086 | `	}` |
|       - | 1087 | `	{` |
|      79 | 1088 | `		sxi32 rcCmp = HashmapFlagSortStatus(pCtx);` |
|      79 | 1089 | `		if( rcCmp != PH7_OK ){` |
|      16 | 1090 | `			return rcCmp;` |
|       - | 1091 | `		}` |
|       - | 1092 | `	}` |
|       - | 1093 | `	/* All done,return TRUE */` |
|      65 | 1094 | `	ph7_result_bool(pCtx,1);` |
|      65 | 1095 | `	return PH7_OK;` |
|      42 | 1096 | `}` |
|       - | 1097 | `/*` |
|       - | 1098 | ` * bool natsort(array &$array)` |
|       - | 1099 | ` * bool natcasesort(array &$array)` |
|       - | 1100 | ` *  Sort an array with php's "natural order" algorithm, maintaining index` |
|       - | 1101 | ` *  association: exactly asort() under SORT_NATURAL (plus SORT_FLAG_CASE for the` |
|       - | 1102 | ` *  case-insensitive twin), which is how php implements them too.` |
|       - | 1103 | ` *` |
|       - | 1104 | `` *  They used to be PRELUDE wrappers over `uasort($array, 'strnatcmp')`, and that`` |
|       - | 1105 | ` *  is a different function: uasort hands each element to a userland callback, so` |
|       - | 1106 | `` *  every element had to satisfy strnatcmp's `string` ZPP row. php's natsort`` |
|       - | 1107 | ` *  COERCES each element the way any string comparison does, so` |
|       - | 1108 | `` *  `natsort([10, "9", null])` — nothing exotic, just a mixed array — was a`` |
|       - | 1109 | ` *  TypeError in PHL and a sorted array in php, and an object with no` |
|       - | 1110 | ` *  __toString() answered strnatcmp's ZPP TypeError instead of php's coercion` |
|       - | 1111 | ` *  Error. Routing through the flag comparator picks up HashmapFlagStringify,` |
|       - | 1112 | ` *  which already renders arrays with php's "Array to string conversion" warning` |
|       - | 1113 | ` *  and raises the coercion Error once per sort.` |
|       - | 1114 | ` */` |
|      26 | 1115 | `PH7_PRIVATE int ph7_hashmap_natsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 1116 | `{` |
|       - | 1117 | `	char zGiven[64];` |
|      29 | 1118 | `	const char *zName = ph7_function_name(pCtx);` |
|       - | 1119 | `	/* natcasesort() is the SORT_FLAG_CASE twin; match the whole name, not a byte. */` |
|      34 | 1120 | `	int bFold = zName && SyStrlen(zName) == sizeof("natcasesort")-1` |
|      39 | 1121 | `		&& SyMemcmp(zName,"natcasesort",sizeof("natcasesort")-1) == 0;` |
|       - | 1122 | `	ph7_hashmap *pMap;` |
|      29 | 1123 | `	if( nArg < 1 ){` |
|     ! 0 | 1124 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1125 | `			"ArgumentCountError",` |
|     ! 0 | 1126 | `			"%s() expects exactly 1 argument, 0 given",zName` |
|       - | 1127 | `			);` |
|       - | 1128 | `	}` |
|      29 | 1129 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 1130 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1131 | `			"TypeError",` |
|       - | 1132 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 1133 | `			zName,VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 1134 | `			);` |
|       - | 1135 | `	}` |
|      29 | 1136 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      29 | 1137 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      29 | 1138 | `	if( pMap->nEntry > 1 ){` |
|       - | 1139 | `		/* SORT_NATURAL (6), optionally \| SORT_FLAG_CASE (8) — the same iFlags` |
|       - | 1140 | `		 * word asort() forwards, so this is asort($a, SORT_NATURAL) exactly. */` |
|      25 | 1141 | `		sxi32 iCmpFlags = bFold ? (6\|8) : 6;` |
|      25 | 1142 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|      25 | 1143 | `		HashmapSortDrive(pCtx->pVm,apArg[0],pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|      25 | 1144 | `		while(pMap->pLast->pPrev){` |
|     ! 0 | 1145 | `			pMap->pLast = pMap->pLast->pPrev;` |
|     ! 0 | 1146 | `		}` |
|      11 | 1147 | `	}` |
|       - | 1148 | `	{` |
|      29 | 1149 | `		sxi32 rcCmp = HashmapFlagSortStatus(pCtx);` |
|      29 | 1150 | `		if( rcCmp != PH7_OK ){` |
|       5 | 1151 | `			return rcCmp;` |
|       - | 1152 | `		}` |
|       - | 1153 | `	}` |
|      25 | 1154 | `	ph7_result_bool(pCtx,1);` |
|      25 | 1155 | `	return PH7_OK;` |
|      16 | 1156 | `}` |
|       - | 1157 | `/*` |
|       - | 1158 | ` * bool arsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 1159 | ` *  Sort an array in reverse order and maintain index association.` |
|       - | 1160 | ` * Parameters` |
|       - | 1161 | ` *  $array` |
|       - | 1162 | ` *   The input array.` |
|       - | 1163 | ` * $sort_flags` |
|       - | 1164 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 1165 | ` *  Sorting type flags:` |
|       - | 1166 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 1167 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 1168 | ` *   SORT_STRING - compare items as strings` |
|       - | 1169 | ` * Return` |
|       - | 1170 | ` *  TRUE on success or FALSE on failure.` |
|       - | 1171 | ` */` |
|      28 | 1172 | `PH7_PRIVATE int ph7_hashmap_arsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 1173 | `{` |
|       - | 1174 | `	char zGiven[64];` |
|       - | 1175 | `	ph7_hashmap *pMap;` |
|       - | 1176 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|      31 | 1177 | `	if( nArg < 1 ){` |
|     ! 0 | 1178 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1179 | `			"ArgumentCountError",` |
|       - | 1180 | `			"arsort() expects at least 1 argument, 0 given"` |
|       - | 1181 | `			);` |
|       - | 1182 | `	}` |
|       - | 1183 | `	/* PHP 8: TypeError if first argument is not an array */` |
|      31 | 1184 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 1185 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1186 | `			"TypeError",` |
|       - | 1187 | `			"arsort(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 1188 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 1189 | `			);` |
|       - | 1190 | `	}` |
|       - | 1191 | `	/* Point to the internal representation of the input hashmap */` |
|      31 | 1192 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      31 | 1193 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      31 | 1194 | `	if( pMap->nEntry > 1 ){` |
|      27 | 1195 | `		sxi32 iCmpFlags = 0;` |
|      27 | 1196 | `		if( nArg > 1 ){` |
|       - | 1197 | `			/* Extract comparison flags */` |
|      13 | 1198 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       5 | 1199 | `		}` |
|       - | 1200 | `		/* Decide the order */` |
|      27 | 1201 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|      27 | 1202 | `		HashmapSortDrive(pCtx->pVm,apArg[0],pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|      12 | 1203 | `	}` |
|       - | 1204 | `	{` |
|      31 | 1205 | `		sxi32 rcCmp = HashmapFlagSortStatus(pCtx);` |
|      31 | 1206 | `		if( rcCmp != PH7_OK ){` |
|       3 | 1207 | `			return rcCmp;` |
|       - | 1208 | `		}` |
|       - | 1209 | `	}` |
|       - | 1210 | `	/* All done,return TRUE */` |
|      28 | 1211 | `	ph7_result_bool(pCtx,1);` |
|      28 | 1212 | `	return PH7_OK;` |
|      17 | 1213 | `}` |
|       - | 1214 | `/*` |
|       - | 1215 | ` * bool ksort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 1216 | ` *  Sort an array by key.` |
|       - | 1217 | ` * Parameters` |
|       - | 1218 | ` *  $array` |
|       - | 1219 | ` *   The input array.` |
|       - | 1220 | ` * $sort_flags` |
|       - | 1221 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 1222 | ` *  Sorting type flags:` |
|       - | 1223 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 1224 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 1225 | ` *   SORT_STRING - compare items as strings` |
|       - | 1226 | ` * Return` |
|       - | 1227 | ` *  TRUE on success or FALSE on failure.` |
|       - | 1228 | ` */` |
|     370 | 1229 | `PH7_PRIVATE int ph7_hashmap_ksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 1230 | `{` |
|       - | 1231 | `	ph7_hashmap *pMap;` |
|       - | 1232 | `	/* Make sure we are dealing with a valid hashmap */` |
|     373 | 1233 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 1234 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 1235 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1236 | `		return PH7_OK;` |
|       - | 1237 | `	}` |
|       - | 1238 | `	/* Point to the internal representation of the input hashmap */` |
|     373 | 1239 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|     373 | 1240 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     373 | 1241 | `	if( pMap->nEntry > 1 ){` |
|     133 | 1242 | `		sxi32 iCmpFlags = 0;` |
|     133 | 1243 | `		if( nArg > 1 ){` |
|       - | 1244 | `			/* Extract comparison flags */` |
|      57 | 1245 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|      27 | 1246 | `		}` |
|       - | 1247 | `		/* Decide the order */` |
|     133 | 1248 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|     133 | 1249 | `		HashmapSortDrive(pCtx->pVm,apArg[0],pMap,HashmapCmpCallback2,SX_INT_TO_PTR(iCmpFlags));` |
|      65 | 1250 | `	}` |
|       - | 1251 | `	{` |
|     373 | 1252 | `		sxi32 rcCmp = HashmapFlagSortStatus(pCtx);` |
|     373 | 1253 | `		if( rcCmp != PH7_OK ){` |
|     ! 0 | 1254 | `			return rcCmp;` |
|       - | 1255 | `		}` |
|       - | 1256 | `	}` |
|       - | 1257 | `	/* All done,return TRUE */` |
|     373 | 1258 | `	ph7_result_bool(pCtx,1);` |
|     373 | 1259 | `	return PH7_OK;` |
|     188 | 1260 | `}` |
|       - | 1261 | `/*` |
|       - | 1262 | ` * bool krsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 1263 | ` *  Sort an array by key in reverse order.` |
|       - | 1264 | ` * Parameters` |
|       - | 1265 | ` *  $array` |
|       - | 1266 | ` *   The input array.` |
|       - | 1267 | ` * $sort_flags` |
|       - | 1268 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 1269 | ` *  Sorting type flags:` |
|       - | 1270 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 1271 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 1272 | ` *   SORT_STRING - compare items as strings` |
|       - | 1273 | ` * Return` |
|       - | 1274 | ` *  TRUE on success or FALSE on failure.` |
|       - | 1275 | ` */` |
|      30 | 1276 | `PH7_PRIVATE int ph7_hashmap_krsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1277 | `{` |
|       - | 1278 | `	ph7_hashmap *pMap;` |
|       - | 1279 | `	/* Make sure we are dealing with a valid hashmap */` |
|      32 | 1280 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 1281 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 1282 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1283 | `		return PH7_OK;` |
|       - | 1284 | `	}` |
|       - | 1285 | `	/* Point to the internal representation of the input hashmap */` |
|      32 | 1286 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      32 | 1287 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      32 | 1288 | `	if( pMap->nEntry > 1 ){` |
|      32 | 1289 | `		sxi32 iCmpFlags = 0;` |
|      32 | 1290 | `		if( nArg > 1 ){` |
|       - | 1291 | `			/* Extract comparison flags */` |
|       6 | 1292 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       2 | 1293 | `		}` |
|       - | 1294 | `		/* Decide the order */` |
|      32 | 1295 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|      32 | 1296 | `		HashmapSortDrive(pCtx->pVm,apArg[0],pMap,HashmapCmpCallback5,SX_INT_TO_PTR(iCmpFlags));` |
|      15 | 1297 | `	}` |
|       - | 1298 | `	{` |
|      32 | 1299 | `		sxi32 rcCmp = HashmapFlagSortStatus(pCtx);` |
|      32 | 1300 | `		if( rcCmp != PH7_OK ){` |
|     ! 0 | 1301 | `			return rcCmp;` |
|       - | 1302 | `		}` |
|       - | 1303 | `	}` |
|       - | 1304 | `	/* All done,return TRUE */` |
|      32 | 1305 | `	ph7_result_bool(pCtx,1);` |
|      32 | 1306 | `	return PH7_OK;` |
|      17 | 1307 | `}` |
|       - | 1308 | `/*` |
|       - | 1309 | ` * bool rsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 1310 | ` * Sort an array in reverse order.` |
|       - | 1311 | ` * Parameters` |
|       - | 1312 | ` *  $array` |
|       - | 1313 | ` *   The input array.` |
|       - | 1314 | ` * $sort_flags` |
|       - | 1315 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 1316 | ` *  Sorting type flags:` |
|       - | 1317 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 1318 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 1319 | ` *   SORT_STRING - compare items as strings` |
|       - | 1320 | ` * Return` |
|       - | 1321 | ` *  TRUE on success or FALSE on failure.` |
|       - | 1322 | ` */` |
|      40 | 1323 | `PH7_PRIVATE int ph7_hashmap_rsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1324 | `{` |
|       - | 1325 | `	ph7_hashmap *pMap;` |
|       - | 1326 | `	/* Make sure we are dealing with a valid hashmap */` |
|      45 | 1327 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 1328 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 1329 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1330 | `		return PH7_OK;` |
|       - | 1331 | `	}` |
|       - | 1332 | `	/* Point to the internal representation of the input hashmap */` |
|      45 | 1333 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      45 | 1334 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      45 | 1335 | `	if( pMap->nEntry > 1 ){` |
|      43 | 1336 | `		sxi32 iCmpFlags = 0;` |
|      43 | 1337 | `		if( nArg > 1 ){` |
|       - | 1338 | `			/* Extract comparison flags */` |
|      28 | 1339 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|      12 | 1340 | `		}` |
|       - | 1341 | `		/* Decide the order */` |
|      43 | 1342 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|      43 | 1343 | `		HashmapSortDrive(pCtx->pVm,apArg[0],pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 1344 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|      43 | 1345 | `		HashmapSortRehash(pMap);` |
|      22 | 1346 | `	}else if( pMap->nEntry == 1 ){` |
|       - | 1347 | `		/* php reindexes even a single-element array: a string key becomes 0 */` |
|       3 | 1348 | `		HashmapSortRehash(pMap);` |
|       1 | 1349 | `	}` |
|       - | 1350 | `	{` |
|      45 | 1351 | `		sxi32 rcCmp = HashmapFlagSortStatus(pCtx);` |
|      45 | 1352 | `		if( rcCmp != PH7_OK ){` |
|      14 | 1353 | `			return rcCmp;` |
|       - | 1354 | `		}` |
|       - | 1355 | `	}` |
|       - | 1356 | `	/* All done,return TRUE */` |
|      32 | 1357 | `	ph7_result_bool(pCtx,1);` |
|      32 | 1358 | `	return PH7_OK;` |
|      25 | 1359 | `}` |
|       - | 1360 | `/*` |
|       - | 1361 | ` * bool usort(array &$array,callable $cmp_function)` |
|       - | 1362 | ` *  Sort an array by values using a user-defined comparison function.` |
|       - | 1363 | ` * Parameters` |
|       - | 1364 | ` *  $array` |
|       - | 1365 | ` *   The input array.` |
|       - | 1366 | ` * $cmp_function` |
|       - | 1367 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 1368 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 1369 | ` *  to, or greater than the second.` |
|       - | 1370 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 1371 | ` * Return` |
|       - | 1372 | ` *  TRUE on success or FALSE on failure.` |
|       - | 1373 | ` */` |
|     241 | 1374 | `PH7_PRIVATE int ph7_hashmap_usort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1375 | `{` |
|       - | 1376 | `	ph7_hashmap *pMap;` |
|       - | 1377 | `	/* php's compare_deprecation_thrown, cleared by every entry (see PH7_HashmapUserCmp) */` |
|     246 | 1378 | `	pCtx->pVm->bCmpBoolRaised = 0;` |
|       - | 1379 | `	/* Make sure we are dealing with a valid hashmap */` |
|     246 | 1380 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 1381 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 1382 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1383 | `		return PH7_OK;` |
|       - | 1384 | `	}` |
|     246 | 1385 | `	if( nArg > 1 ){` |
|       - | 1386 | `		/* php rejects an uncallable comparator with a TypeError, BEFORE touching the` |
|       - | 1387 | `		 * array. PH7 handed it to the dispatcher, which answers NULL in silence -- so` |
|       - | 1388 | `		 * usort() with a bogus comparator reordered the array anyway, by nothing. */` |
|     246 | 1389 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",0);` |
|     246 | 1390 | `		if( rcCb != PH7_OK ){` |
|       9 | 1391 | `			return rcCb;` |
|       - | 1392 | `		}` |
|     113 | 1393 | `	}` |
|       - | 1394 | `	/* Point to the internal representation of the input hashmap */` |
|     238 | 1395 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|     238 | 1396 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     238 | 1397 | `	if( pMap->nEntry > 1 ){` |
|       - | 1398 | `		HashmapUserSort sSort;` |
|     236 | 1399 | `		void *pCmpData = 0;` |
|       - | 1400 | `		ProcNodeCmp xCmp;` |
|     236 | 1401 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|     236 | 1402 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 1403 | `			/* Point to the desired callback */` |
|     236 | 1404 | `			sSort.pCtx = pCtx;` |
|     236 | 1405 | `			sSort.pCallback = apArg[1];` |
|     236 | 1406 | `			pCmpData = (void *)&sSort;` |
|     117 | 1407 | `		}else{` |
|       - | 1408 | `			/* Use the default comparison function */` |
|     ! 0 | 1409 | `			xCmp = HashmapCmpCallback1;` |
|       - | 1410 | `		}` |
|       - | 1411 | `		/* Decide the order */` |
|     236 | 1412 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|     236 | 1413 | `		HashmapSortDrive(pCtx->pVm,apArg[0],pMap,xCmp,pCmpData);` |
|       - | 1414 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|     236 | 1415 | `		HashmapSortRehash(pMap);` |
|     236 | 1416 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 1417 | `			/* The comparison callback did not return: propagate its status so the` |
|       - | 1418 | `			 * dispatcher unwinds. */` |
|      39 | 1419 | `			sxi32 rcExc = pCtx->pVm->iCmpCallbackExc;` |
|      39 | 1420 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|      39 | 1421 | `			return rcExc;` |
|       5 | 1422 | `		}` |
|      97 | 1423 | `	}else if( pMap->nEntry == 1 ){` |
|       - | 1424 | `		/* php reindexes even a single-element array: a string key becomes 0 */` |
|       3 | 1425 | `		HashmapSortRehash(pMap);` |
|       1 | 1426 | `	}` |
|       - | 1427 | `	/* All done,return TRUE */` |
|     202 | 1428 | `	ph7_result_bool(pCtx,1);` |
|     202 | 1429 | `	return PH7_OK;` |
|     122 | 1430 | `}` |
|       - | 1431 | `/*` |
|       - | 1432 | ` * bool uasort(array &$array,callable $cmp_function)` |
|       - | 1433 | ` *  Sort an array by values using a user-defined comparison function` |
|       - | 1434 | ` *  and maintain index association.` |
|       - | 1435 | ` * Parameters` |
|       - | 1436 | ` *  $array` |
|       - | 1437 | ` *   The input array.` |
|       - | 1438 | ` * $cmp_function` |
|       - | 1439 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 1440 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 1441 | ` *  to, or greater than the second.` |
|       - | 1442 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 1443 | ` * Return` |
|       - | 1444 | ` *  TRUE on success or FALSE on failure.` |
|       - | 1445 | ` */` |
|      34 | 1446 | `PH7_PRIVATE int ph7_hashmap_uasort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1447 | `{` |
|       - | 1448 | `	ph7_hashmap *pMap;` |
|       - | 1449 | `	/* php's compare_deprecation_thrown, cleared by every entry (see PH7_HashmapUserCmp) */` |
|      39 | 1450 | `	pCtx->pVm->bCmpBoolRaised = 0;` |
|       - | 1451 | `	/* Make sure we are dealing with a valid hashmap */` |
|      39 | 1452 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 1453 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 1454 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1455 | `		return PH7_OK;` |
|       - | 1456 | `	}` |
|      39 | 1457 | `	if( nArg > 1 ){` |
|       - | 1458 | `		/* php rejects an uncallable comparator with a TypeError, BEFORE touching the` |
|       - | 1459 | `		 * array. PH7 handed it to the dispatcher, which answers NULL in silence -- so` |
|       - | 1460 | `		 * usort() with a bogus comparator reordered the array anyway, by nothing. */` |
|      39 | 1461 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",0);` |
|      39 | 1462 | `		if( rcCb != PH7_OK ){` |
|       5 | 1463 | `			return rcCb;` |
|       - | 1464 | `		}` |
|      15 | 1465 | `	}` |
|       - | 1466 | `	/* Point to the internal representation of the input hashmap */` |
|      35 | 1467 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      35 | 1468 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      35 | 1469 | `	if( pMap->nEntry > 1 ){` |
|       - | 1470 | `		HashmapUserSort sSort;` |
|      35 | 1471 | `		void *pCmpData = 0;` |
|       - | 1472 | `		ProcNodeCmp xCmp;` |
|      35 | 1473 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|      35 | 1474 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 1475 | `			/* Point to the desired callback */` |
|      35 | 1476 | `			sSort.pCtx = pCtx;` |
|      35 | 1477 | `			sSort.pCallback = apArg[1];` |
|      35 | 1478 | `			pCmpData = (void *)&sSort;` |
|      20 | 1479 | `		}else{` |
|       - | 1480 | `			/* Use the default comparison function */` |
|     ! 0 | 1481 | `			xCmp = HashmapCmpCallback1;` |
|       - | 1482 | `		}` |
|       - | 1483 | `		/* Decide the order */` |
|      35 | 1484 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|      35 | 1485 | `		HashmapSortDrive(pCtx->pVm,apArg[0],pMap,xCmp,pCmpData);` |
|      35 | 1486 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 1487 | `			/* The comparison callback did not return: propagate its status so the` |
|       - | 1488 | `			 * dispatcher unwinds. */` |
|       5 | 1489 | `			sxi32 rcExc = pCtx->pVm->iCmpCallbackExc;` |
|       5 | 1490 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       5 | 1491 | `			return rcExc;` |
|       - | 1492 | `		}` |
|      13 | 1493 | `	}` |
|       - | 1494 | `	/* All done,return TRUE */` |
|      31 | 1495 | `	ph7_result_bool(pCtx,1);` |
|      31 | 1496 | `	return PH7_OK;` |
|      22 | 1497 | `}` |
|       - | 1498 | `/*` |
|       - | 1499 | ` * bool uksort(array &$array,callable $cmp_function)` |
|       - | 1500 | ` *  Sort an array by keys using a user-defined comparison` |
|       - | 1501 | ` *  function and maintain index association.` |
|       - | 1502 | ` * Parameters` |
|       - | 1503 | ` *  $array` |
|       - | 1504 | ` *   The input array.` |
|       - | 1505 | ` * $cmp_function` |
|       - | 1506 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 1507 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 1508 | ` *  to, or greater than the second.` |
|       - | 1509 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 1510 | ` * Return` |
|       - | 1511 | ` *  TRUE on success or FALSE on failure.` |
|       - | 1512 | ` */` |
|      28 | 1513 | `PH7_PRIVATE int ph7_hashmap_uksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 1514 | `{` |
|       - | 1515 | `	ph7_hashmap *pMap;` |
|       - | 1516 | `	/* php's compare_deprecation_thrown, cleared by every entry (see PH7_HashmapUserCmp) */` |
|      31 | 1517 | `	pCtx->pVm->bCmpBoolRaised = 0;` |
|       - | 1518 | `	/* Make sure we are dealing with a valid hashmap */` |
|      31 | 1519 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 1520 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 1521 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1522 | `		return PH7_OK;` |
|       - | 1523 | `	}` |
|      31 | 1524 | `	if( nArg > 1 ){` |
|       - | 1525 | `		/* php rejects an uncallable comparator with a TypeError, BEFORE touching the` |
|       - | 1526 | `		 * array. PH7 handed it to the dispatcher, which answers NULL in silence -- so` |
|       - | 1527 | `		 * usort() with a bogus comparator reordered the array anyway, by nothing. */` |
|      31 | 1528 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",0);` |
|      31 | 1529 | `		if( rcCb != PH7_OK ){` |
|       3 | 1530 | `			return rcCb;` |
|       - | 1531 | `		}` |
|      13 | 1532 | `	}` |
|       - | 1533 | `	/* Point to the internal representation of the input hashmap */` |
|      29 | 1534 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      29 | 1535 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      29 | 1536 | `	if( pMap->nEntry > 1 ){` |
|       - | 1537 | `		HashmapUserSort sSort;` |
|      29 | 1538 | `		void *pCmpData = 0;` |
|       - | 1539 | `		ProcNodeCmp xCmp;` |
|      29 | 1540 | `		xCmp = HashmapCmpCallback6; /* User-defined function as the comparison callback */` |
|      29 | 1541 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 1542 | `			/* Point to the desired callback */` |
|      29 | 1543 | `			sSort.pCtx = pCtx;` |
|      29 | 1544 | `			sSort.pCallback = apArg[1];` |
|      29 | 1545 | `			pCmpData = (void *)&sSort;` |
|      16 | 1546 | `		}else{` |
|       - | 1547 | `			/* Use the default comparison function */` |
|     ! 0 | 1548 | `			xCmp = HashmapCmpCallback2;` |
|       - | 1549 | `		}` |
|       - | 1550 | `		/* Decide the order */` |
|      29 | 1551 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|      29 | 1552 | `		HashmapSortDrive(pCtx->pVm,apArg[0],pMap,xCmp,pCmpData);` |
|      29 | 1553 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 1554 | `			/* The comparison callback did not return: propagate its status so the` |
|       - | 1555 | `			 * dispatcher unwinds. */` |
|       8 | 1556 | `			sxi32 rcExc = pCtx->pVm->iCmpCallbackExc;` |
|       8 | 1557 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       8 | 1558 | `			return rcExc;` |
|       - | 1559 | `		}` |
|      10 | 1560 | `	}` |
|       - | 1561 | `	/* All done,return TRUE */` |
|      23 | 1562 | `	ph7_result_bool(pCtx,1);` |
|      23 | 1563 | `	return PH7_OK;` |
|      17 | 1564 | `}` |
|       - | 1565 | `/*` |
|       - | 1566 | ` * bool array_multisort(array &$array, mixed $array1_sort_order = SORT_ASC,` |
|       - | 1567 | ` *                      mixed $array1_sort_flags = SORT_REGULAR, mixed &...$rest)` |
|       - | 1568 | ` *  Sort multiple arrays at once: the argument list is a little language read` |
|       - | 1569 | ` *  left to right — an ARRAY opens a column, and each column may be followed by` |
|       - | 1570 | ` *  at most one sort ORDER (SORT_ASC/SORT_DESC) and at most one sort FLAGS` |
|       - | 1571 | ` *  value, in either order. Rows are compared column by column, a tie in one` |
|       - | 1572 | ` *  column falling through to the next; the resulting permutation is applied to` |
|       - | 1573 | ` *  EVERY column. String keys are kept, numeric keys are renumbered, and the` |
|       - | 1574 | ` *  sort is stable (php 8's own guarantee). php's error shapes, pinned by` |
|       - | 1575 | `` *  probe: a non-int non-array is `Argument #N must be an array or a sort`` |
|       - | 1576 | `` *  flag`; a misplaced or repeated order/flags is the same text with`` |
|       - | 1577 | `` *  `... that has not already been specified`; an int that is no flag at all is`` |
|       - | 1578 | `` *  the ValueError `must be a valid sort flag`; mismatched lengths are`` |
|       - | 1579 | `` *  `Array sizes are inconsistent` with no function prefix; and only`` |
|       - | 1580 | `` *  Argument #1 carries its `($array)` name. php declares the whole list`` |
|       - | 1581 | ` *  prefer-ref (VmBuiltinPrefersRef), so a literal sorts a temporary silently.` |
|       - | 1582 | ` */` |
|       - | 1583 | `/* One column of the multisort: the caller's array plus its sort spec. */` |
|       - | 1584 | `typedef struct MultisortCol MultisortCol;` |
|       - | 1585 | `struct MultisortCol` |
|       - | 1586 | `{` |
|       - | 1587 | `	ph7_value *pArr;        /* The caller's argument slot */` |
|       - | 1588 | `	ph7_hashmap *pMap;      /* Its hashmap */` |
|       - | 1589 | `	ph7_hashmap_node **apNode; /* Nodes in ORIGINAL iteration order */` |
|       - | 1590 | `	sxi32 iFlags;           /* SORT_* comparison flags */` |
|       - | 1591 | `	int iDir;               /* +1 SORT_ASC, -1 SORT_DESC */` |
|       - | 1592 | `	int bOrderSeen;         /* An order argument already attached */` |
|       - | 1593 | `	int bFlagsSeen;         /* A flags argument already attached */` |
|       - | 1594 | `};` |
|       - | 1595 | `/* Compare two ROWS, column by column with each column's own direction/flags. */` |
|      96 | 1596 | `static sxi32 MultisortRowCmp(MultisortCol *aCol,sxu32 nCol,sxu32 iA,sxu32 iB)` |
|       4 | 1597 | `{` |
|     100 | 1598 | `	ph7_vm *pVm = aCol[0].pMap->pVm;` |
|       - | 1599 | `	sxu32 c;` |
|     110 | 1600 | `	for( c = 0 ; c < nCol ; c++ ){` |
|     110 | 1601 | `		sxi32 rc = HashmapFlagValueCmp(aCol[c].apNode[iA],aCol[c].apNode[iB],aCol[c].iFlags);` |
|     110 | 1602 | `		HashmapCmpLatch(pVm);` |
|     110 | 1603 | `		if( rc != 0 ){` |
|     100 | 1604 | `			return aCol[c].iDir < 0 ? -rc : rc;` |
|       - | 1605 | `		}` |
|       6 | 1606 | `	}` |
|     ! 0 | 1607 | `	return 0;` |
|      52 | 1608 | `}` |
|       - | 1609 | `/*` |
|       - | 1610 | ` * Stable bottom-up merge sort over the row-index permutation. Iterative on` |
|       - | 1611 | ` * purpose — the recursive shape would put O(log n) frames on the native stack` |
|       - | 1612 | ` * (the embedder C-stack family).` |
|       - | 1613 | ` */` |
|      34 | 1614 | `static void MultisortSortIdx(MultisortCol *aCol,sxu32 nCol,sxu32 *aIdx,sxu32 *aTmp,sxu32 n)` |
|       4 | 1615 | `{` |
|       - | 1616 | `	sxu32 nWidth,iLo;` |
|     102 | 1617 | `	for( nWidth = 1 ; nWidth < n ; nWidth *= 2 ){` |
|     162 | 1618 | `		for( iLo = 0 ; iLo < n ; iLo += 2 * nWidth ){` |
|      98 | 1619 | `			sxu32 iMid = iLo + nWidth;` |
|      98 | 1620 | `			sxu32 iHi = iLo + 2 * nWidth;` |
|       - | 1621 | `			sxu32 i,j,k;` |
|      98 | 1622 | `			if( iMid > n ){ iMid = n; }` |
|      98 | 1623 | `			if( iHi > n ){ iHi = n; }` |
|      98 | 1624 | `			i = iLo; j = iMid; k = iLo;` |
|     194 | 1625 | `			while( i < iMid && j < iHi ){` |
|       - | 1626 | `				/* <= keeps the run stable: on a full tie the left row wins */` |
|     100 | 1627 | `				if( MultisortRowCmp(aCol,nCol,aIdx[i],aIdx[j]) <= 0 ){` |
|      44 | 1628 | `					aTmp[k++] = aIdx[i++];` |
|      24 | 1629 | `				}else{` |
|      60 | 1630 | `					aTmp[k++] = aIdx[j++];` |
|       - | 1631 | `				}` |
|       4 | 1632 | `			}` |
|     182 | 1633 | `			while( i < iMid ){ aTmp[k++] = aIdx[i++]; }` |
|     114 | 1634 | `			while( j < iHi ){ aTmp[k++] = aIdx[j++]; }` |
|      51 | 1635 | `		}` |
|       - | 1636 | `		/* aTmp holds the merged runs for this width; swap roles by copying` |
|       - | 1637 | `		 * back — n is bounded by the array count, one memcpy per doubling. */` |
|      68 | 1638 | `		SyMemcpy(aTmp,aIdx,(sxu32)(n * sizeof(sxu32)));` |
|      36 | 1639 | `	}` |
|      38 | 1640 | `}` |
|      60 | 1641 | `PH7_PRIVATE int ph7_hashmap_multisort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 1642 | `{` |
|       - | 1643 | `	MultisortCol *aCol;` |
|       - | 1644 | `	sxu32 *aIdx,*aTmp;` |
|      64 | 1645 | `	sxu32 nCol = 0,nRow,i;` |
|       - | 1646 | `	sxi32 rcStatus;` |
|       - | 1647 | `	int iArg;` |
|       - | 1648 |  |
|      64 | 1649 | `	if( nArg < 1 ){` |
|     ! 0 | 1650 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1651 | `			"ArgumentCountError",` |
|       - | 1652 | `			"array_multisort() expects at least 1 argument, %d given",` |
|     ! 0 | 1653 | `			nArg` |
|       - | 1654 | `			);` |
|       - | 1655 | `	}` |
|      94 | 1656 | `	aCol = (MultisortCol *)ph7_context_alloc_chunk(pCtx,` |
|      60 | 1657 | `		(unsigned int)(sizeof(MultisortCol) * (sxu32)nArg),TRUE,TRUE);` |
|      64 | 1658 | `	if( aCol == 0 ){` |
|     ! 0 | 1659 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 1660 | `	}` |
|       - | 1661 | `	/* Read the argument list's little language, php's own state machine. */` |
|     162 | 1662 | `	for( iArg = 0 ; iArg < nArg ; iArg++ ){` |
|     124 | 1663 | `		ph7_value *pArg = apArg[iArg];` |
|       - | 1664 | `		/* Only Argument #1 carries its parameter name in php's messages. */` |
|     124 | 1665 | `		const char *zName = (iArg == 0) ? " ($array)" : "";` |
|     124 | 1666 | `		if( ph7_value_is_array(pArg) ){` |
|      80 | 1667 | `			MultisortCol *pCol = &aCol[nCol++];` |
|      80 | 1668 | `			pCol->pArr = pArg;` |
|      80 | 1669 | `			pCol->pMap = (ph7_hashmap *)pArg->x.pOther;` |
|      80 | 1670 | `			pCol->apNode = 0;` |
|      80 | 1671 | `			pCol->iFlags = 0; /* SORT_REGULAR */` |
|      80 | 1672 | `			pCol->iDir = 1;   /* SORT_ASC */` |
|      80 | 1673 | `			pCol->bOrderSeen = pCol->bFlagsSeen = 0;` |
|      80 | 1674 | `			continue;` |
|       - | 1675 | `		}` |
|      47 | 1676 | `		if( ph7_value_is_float(pArg) \|\| (pArg->iFlags & MEMOBJ_INT) == 0 ){` |
|       - | 1677 | `			/* A REAL int only, float asked FIRST (ph7_type_name's rule: an` |
|       - | 1678 | `			 * integer-valued real caches an int and would pass a bare flag` |
|       - | 1679 | `			 * test). php coerces nothing here: "4", 4.0 and true are all` |
|       - | 1680 | `			 * refused. */` |
|      17 | 1681 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1682 | `				"array_multisort(): Argument #%d%s must be an array or a sort flag",` |
|       5 | 1683 | `				iArg + 1,zName);` |
|       - | 1684 | `		}` |
|       - | 1685 | `		{` |
|      37 | 1686 | `			sxi64 iVal = ph7_value_to_int64(pArg);` |
|       - | 1687 | `			/* php masks SORT_FLAG_CASE off for the ORDER match too, but reads` |
|       - | 1688 | `			 * the direction from the UNMASKED value — so SORT_DESC\|SORT_FLAG_CASE` |
|       - | 1689 | `			 * (11) consumes the order slot and quirkily sorts ASCENDING. */` |
|      37 | 1690 | `			if( (iVal & ~(sxi64)8) == 3 /* SORT_DESC */ \|\| (iVal & ~(sxi64)8) == 4 /* SORT_ASC */ ){` |
|      23 | 1691 | `				if( nCol < 1 \|\| aCol[nCol - 1].bOrderSeen ){` |
|      11 | 1692 | `					return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1693 | `						"array_multisort(): Argument #%d%s must be an array or a sort flag that has not already been specified",` |
|       3 | 1694 | `						iArg + 1,zName);` |
|       - | 1695 | `				}` |
|      17 | 1696 | `				aCol[nCol - 1].iDir = (iVal == 3) ? -1 : 1;` |
|      17 | 1697 | `				aCol[nCol - 1].bOrderSeen = 1;` |
|      24 | 1698 | `			}else if( (iVal & ~(sxi64)8 /* SORT_FLAG_CASE */) == 0 /* SORT_REGULAR */` |
|      13 | 1699 | `			       \|\| (iVal & ~(sxi64)8) == 1 /* SORT_NUMERIC */` |
|      11 | 1700 | `			       \|\| (iVal & ~(sxi64)8) == 2 /* SORT_STRING */` |
|       8 | 1701 | `			       \|\| (iVal & ~(sxi64)8) == 5 /* SORT_LOCALE_STRING */` |
|       7 | 1702 | `			       \|\| (iVal & ~(sxi64)8) == 6 /* SORT_NATURAL */ ){` |
|      14 | 1703 | `				if( nCol < 1 \|\| aCol[nCol - 1].bFlagsSeen ){` |
|       7 | 1704 | `					return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1705 | `						"array_multisort(): Argument #%d%s must be an array or a sort flag that has not already been specified",` |
|       2 | 1706 | `						iArg + 1,zName);` |
|       - | 1707 | `				}` |
|      10 | 1708 | `				aCol[nCol - 1].iFlags = (sxi32)iVal;` |
|      10 | 1709 | `				aCol[nCol - 1].bFlagsSeen = 1;` |
|       6 | 1710 | `			}else{` |
|       4 | 1711 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1712 | `					"array_multisort(): Argument #%d%s must be a valid sort flag",` |
|       1 | 1713 | `					iArg + 1,zName);` |
|       - | 1714 | `			}` |
|       - | 1715 | `		}` |
|      14 | 1716 | `	}` |
|       - | 1717 | `	/* Every column must hold the same number of rows; php's message carries no` |
|       - | 1718 | `	 * function prefix. */` |
|      42 | 1719 | `	nRow = aCol[0].pMap->nEntry;` |
|      58 | 1720 | `	for( i = 1 ; i < nCol ; i++ ){` |
|      21 | 1721 | `		if( aCol[i].pMap->nEntry != nRow ){` |
|       3 | 1722 | `			return PH7_VmThrowException(pCtx,"ValueError","Array sizes are inconsistent");` |
|       - | 1723 | `		}` |
|      10 | 1724 | `	}` |
|      40 | 1725 | `	if( nRow > 0 ){` |
|       - | 1726 | `		/* Collect each column's nodes in original order. */` |
|      86 | 1727 | `		for( i = 0 ; i < nCol ; i++ ){` |
|      52 | 1728 | `			ph7_hashmap_node *pNode = aCol[i].pMap->pFirst;` |
|       - | 1729 | `			sxu32 r;` |
|      76 | 1730 | `			aCol[i].apNode = (ph7_hashmap_node **)ph7_context_alloc_chunk(pCtx,` |
|      24 | 1731 | `				(unsigned int)(sizeof(ph7_hashmap_node *) * nRow),FALSE,TRUE);` |
|      52 | 1732 | `			if( aCol[i].apNode == 0 ){` |
|     ! 0 | 1733 | `				return PH7_ContextMemoryError(pCtx);` |
|       - | 1734 | `			}` |
|     196 | 1735 | `			for( r = 0 ; r < nRow && pNode ; r++ ){` |
|     148 | 1736 | `				aCol[i].apNode[r] = pNode;` |
|     148 | 1737 | `				pNode = pNode->pPrev; /* Reverse link */` |
|      76 | 1738 | `			}` |
|      28 | 1739 | `		}` |
|      55 | 1740 | `		aIdx = (sxu32 *)ph7_context_alloc_chunk(pCtx,` |
|      17 | 1741 | `			(unsigned int)(sizeof(sxu32) * nRow * 2),FALSE,TRUE);` |
|      38 | 1742 | `		if( aIdx == 0 ){` |
|     ! 0 | 1743 | `			return PH7_ContextMemoryError(pCtx);` |
|       - | 1744 | `		}` |
|      38 | 1745 | `		aTmp = &aIdx[nRow];` |
|     140 | 1746 | `		for( i = 0 ; i < nRow ; i++ ){` |
|     106 | 1747 | `			aIdx[i] = i;` |
|      55 | 1748 | `		}` |
|       - | 1749 | `		/* The string flags coerce user-visibly and can only FLAG a throw` |
|       - | 1750 | `		 * (iCmpCallbackExc); clear it, sort, and report after — the flag-sort` |
|       - | 1751 | `		 * drivers' shared pattern. */` |
|      38 | 1752 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|      38 | 1753 | `		MultisortSortIdx(aCol,nCol,aIdx,aTmp,nRow);` |
|      38 | 1754 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 1755 | `			/* A comparison raised. php's array_multisort leaves EVERY column as` |
|       - | 1756 | `			 * it found it in that case -- unlike sort(), which still writes back` |
|       - | 1757 | `			 * the order it reached -- so the permutation is dropped rather than` |
|       - | 1758 | `			 * applied. */` |
|      11 | 1759 | `			return HashmapFlagSortStatus(pCtx);` |
|       - | 1760 | `		}` |
|       - | 1761 | `		/* Apply the permutation to every column: rebuild in sorted order,` |
|       - | 1762 | `		 * keeping string keys and renumbering int keys, then hand the fresh` |
|       - | 1763 | `		 * array back through the by-ref slot (a literal has none and the` |
|       - | 1764 | `		 * result is silently dropped — php's prefer-ref). */` |
|      63 | 1765 | `		for( i = 0 ; i < nCol ; i++ ){` |
|      39 | 1766 | `			ph7_value *pNew = ph7_context_new_array(pCtx);` |
|       - | 1767 | `			sxu32 r;` |
|      39 | 1768 | `			if( pNew == 0 ){` |
|     ! 0 | 1769 | `				return PH7_ContextMemoryError(pCtx);` |
|       - | 1770 | `			}` |
|     147 | 1771 | `			for( r = 0 ; r < nRow ; r++ ){` |
|     111 | 1772 | `				ph7_hashmap_node *pNode = aCol[i].apNode[aIdx[r]];` |
|     165 | 1773 | `				HashmapInsertNode((ph7_hashmap *)pNew->x.pOther,pNode,` |
|     108 | 1774 | `					pNode->iType == HASHMAP_BLOB_NODE ? TRUE : FALSE);` |
|      57 | 1775 | `			}` |
|      39 | 1776 | `			PH7_VmStoreArgByRef(pCtx->pVm,aCol[i].pArr,pNew);` |
|      21 | 1777 | `		}` |
|      27 | 1778 | `		rcStatus = HashmapFlagSortStatus(pCtx);` |
|      27 | 1779 | `		if( rcStatus != PH7_OK ){` |
|     ! 0 | 1780 | `			return rcStatus;` |
|       - | 1781 | `		}` |
|      12 | 1782 | `	}` |
|      29 | 1783 | `	ph7_result_bool(pCtx,1);` |
|      29 | 1784 | `	return PH7_OK;` |
|      34 | 1785 | `}` |
|       - | 1786 |  |
