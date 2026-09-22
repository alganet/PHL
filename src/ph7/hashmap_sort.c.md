# src/ph7/hashmap_sort.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 501/547 lines (91.59%)

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
|  62522 |   37 | `static ph7_hashmap_node * HashmapNodeMerge(ph7_hashmap_node *pA,ph7_hashmap_node *pB,ProcNodeCmp xCmp,void *pCmpData)` |
|      5 |   38 | `{` |
|      - |   39 | `	ph7_hashmap_node result,*pTail;` |
|      - |   40 | `    /* Prevent compiler warning */` |
|  62527 |   41 | `	result.pNext = result.pPrev = 0;` |
|  62527 |   42 | `	pTail = &result;` |
| 150594 |   43 | `	while( pA && pB ){` |
|  88072 |   44 | `		if( xCmp(pA,pB,pCmpData) <= 0 ){` |
|  69197 |   45 | `			pTail->pPrev = pA;` |
|  69197 |   46 | `			pA->pNext = pTail;` |
|  69197 |   47 | `			pTail = pA;` |
|  69197 |   48 | `			pA = pA->pPrev;` |
|  34661 |   49 | `		}else{` |
|  18880 |   50 | `			pTail->pPrev = pB;` |
|  18880 |   51 | `			pB->pNext = pTail;` |
|  18880 |   52 | `			pTail = pB;` |
|  18880 |   53 | `			pB = pB->pPrev;` |
|      - |   54 | `		}` |
|      5 |   55 | `	}` |
|  62527 |   56 | `	if( pA ){` |
|   4928 |   57 | `		pTail->pPrev = pA;` |
|   4928 |   58 | `		pA->pNext = pTail;` |
|  59975 |   59 | `	}else if( pB ){` |
|  57228 |   60 | `		pTail->pPrev = pB;` |
|  57228 |   61 | `		pB->pNext = pTail;` |
|  28707 |   62 | `	}else{` |
|    381 |   63 | `		pTail->pPrev = pTail->pNext = 0;` |
|      - |   64 | `	}` |
|  62527 |   65 | `	return result.pPrev;` |
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
|   1496 |   79 | `PH7_PRIVATE sxi32 HashmapMergeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData)` |
|      5 |   80 | `{` |
|      - |   81 | `	ph7_hashmap_node *a[N_SORT_BUCKET], *p,*pIn;` |
|      - |   82 | `	sxu32 i;` |
|   1501 |   83 | `	SyZero(a,sizeof(a));` |
|      - |   84 | `	/* Point to the first inserted entry */` |
|   1501 |   85 | `	pIn = pMap->pFirst;` |
|  20435 |   86 | `	while( pIn ){` |
|  18939 |   87 | `		p = pIn;` |
|  18939 |   88 | `		pIn = p->pPrev;` |
|  18939 |   89 | `		p->pPrev = 0;` |
|  35085 |   90 | `		for(i=0; i<N_SORT_BUCKET-1; i++){` |
|  35085 |   91 | `			if( a[i]==0 ){` |
|  18939 |   92 | `				a[i] = p;` |
|  18939 |   93 | `				break;` |
|    ! 0 |   94 | `			}else{` |
|  16151 |   95 | `				p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|  16151 |   96 | `				a[i] = 0;` |
|      - |   97 | `			}` |
|   8078 |   98 | `		}` |
|  18939 |   99 | `		if( i==N_SORT_BUCKET-1 ){` |
|      - |  100 | `			/* To get here, there need to be 2^(N_SORT_BUCKET) elements in he input list.` |
|      - |  101 | `			 * But that is impossible.` |
|      - |  102 | `			 */` |
|    ! 0 |  103 | `			a[i] = HashmapNodeMerge(a[i], p,xCmp,pCmpData);` |
|    ! 0 |  104 | `		}` |
|      5 |  105 | `	}` |
|   1501 |  106 | `	p = a[0];` |
|  47877 |  107 | `	for(i=1; i<N_SORT_BUCKET; i++){` |
|      - |  108 | `		/* Higher-index buckets hold EARLIER-inserted (and larger) runs, so the` |
|      - |  109 | `		 * bucket must be the LEFT operand: HashmapNodeMerge favors its left arg on` |
|      - |  110 | `		 * a tie (cmp <= 0), and php's sorts are stable (PHP 8.0+) — equal elements` |
|      - |  111 | `		 * keep their original order. Passing p (the later elements) on the left` |
|      - |  112 | `		 * reversed equal runs (e.g. usort of five tie-keyed items moved the last to` |
|      - |  113 | `		 * the front). */` |
|  46381 |  114 | `		p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|  23193 |  115 | `	}` |
|   1501 |  116 | `	p->pNext = 0;` |
|      - |  117 | `	/* Reflect the change */` |
|   1501 |  118 | `	pMap->pFirst = p;` |
|      - |  119 | `	/* Reset the loop cursor */` |
|   1501 |  120 | `	pMap->pCur = pMap->pFirst;` |
|   1501 |  121 | `	return SXRET_OK;` |
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
|    330 |  140 | `static void HashmapFlagStringify(ph7_value *pVal)` |
|      3 |  141 | `{` |
|    333 |  142 | `	ph7_vm *pVm = pVal->pVm;` |
|    333 |  143 | `	if( !PH7_MemObjIsNotStringable(pVal) ){` |
|    295 |  144 | `		if( PH7_MemObjToStringUV(pVal) == SXRET_OK ){` |
|    295 |  145 | `			return;` |
|    ! 0 |  146 | `		}` |
|      - |  147 | `		/* A __toString() that THREW. Same shape as a refused cast from here on. */` |
|     39 |  148 | `	}else if( pVm == 0 \|\| pVm->iCmpCallbackExc == 0 ){` |
|     31 |  149 | `		PH7_MemObjToStringUV(pVal); /* raises php's Error */` |
|     15 |  150 | `	}` |
|     39 |  151 | `	if( pVm ){` |
|     39 |  152 | `		pVm->iCmpCallbackExc = 1;` |
|     19 |  153 | `	}` |
|     39 |  154 | `	PH7_MemObjRelease(pVal);` |
|     39 |  155 | `	MemObjSetType(pVal,MEMOBJ_STRING);` |
|    168 |  156 | `}` |
|      - |  157 | `/*` |
|      - |  158 | ` * Node comparison callback.` |
|      - |  159 | ` * used-by: [sort(),asort(),...]` |
|      - |  160 | ` */` |
|      - |  161 | `/*` |
|      - |  162 | ` * Compare two scalar values under an EXPLICIT php sort base type (never 0 —` |
|      - |  163 | ` * SORT_REGULAR is handled by the callers, which differ for keys vs values):` |
|      - |  164 | ` *   SORT_NUMERIC 1 · SORT_STRING 2 · SORT_LOCALE_STRING 5 · SORT_NATURAL 6,` |
|      - |  165 | ` * with bFold applying SORT_FLAG_CASE. PHL has no locale tables, so` |
|      - |  166 | ` * SORT_LOCALE_STRING behaves like SORT_STRING. Mutates both operands (numeric or` |
|      - |  167 | ` * string cast); the caller owns and releases them.` |
|      - |  168 | ` */` |
|    686 |  169 | `static sxi32 HashmapScalarFlagCmp(ph7_value *pA,ph7_value *pB,int base,int bFold)` |
|      5 |  170 | `{` |
|      - |  171 | `	sxi32 rc;` |
|    691 |  172 | `	if( base == 1 ){` |
|      - |  173 | `		/* SORT_NUMERIC compares as DOUBLES. php's numeric_compare_function is` |
|      - |  174 | ``		 * `zval_get_double(a)` vs `zval_get_double(b)` under ZEND_THREEWAY_COMPARE`` |
|      - |  175 | `		 * — not a value comparison over whatever type each operand happens to` |
|      - |  176 | `		 * settle on. Two consequences PHL got wrong by folding to a NUMBER and` |
|      - |  177 | `		 * calling the ordinary comparator: the diagnostic named the wrong type` |
|      - |  178 | `		 * (an object warned "could not be converted to int" where php says` |
|      - |  179 | `		 * "to float"), and integers above 2^53 were ordered EXACTLY where php's` |
|      - |  180 | ``		 * doubles tie — `sort([PHP_INT_MAX, PHP_INT_MAX-1], SORT_NUMERIC)` left`` |
|      - |  181 | `		 * php's stable sort's original order and PHL re-ordered them. Matching php` |
|      - |  182 | `		 * means adopting its precision loss, which is what parity is (§10).` |
|      - |  183 | `		 * The == / < shape is ZEND_THREEWAY_COMPARE's, so NaN — equal to nothing,` |
|      - |  184 | `		 * less than nothing — answers 1 in both engines. */` |
|      - |  185 | `		ph7_real rA,rB;` |
|    230 |  186 | `		PH7_MemObjToReal(pA);` |
|    230 |  187 | `		PH7_MemObjToReal(pB);` |
|    230 |  188 | `		rA = pA->rVal;` |
|    230 |  189 | `		rB = pB->rVal;` |
|    230 |  190 | `		rc = (rA == rB) ? 0 : ((rA < rB) ? -1 : 1);` |
|    116 |  191 | `	}else{` |
|      - |  192 | `		/* SORT_STRING (2) / SORT_LOCALE_STRING (5) / SORT_NATURAL (6) */` |
|      - |  193 | `		const char *zA,*zB;` |
|      - |  194 | `		sxu32 nA,nB,nMin,i;` |
|    462 |  195 | `		if( (pA->iFlags & MEMOBJ_STRING) == 0 ){ HashmapFlagStringify(pA); }` |
|    462 |  196 | `		if( (pB->iFlags & MEMOBJ_STRING) == 0 ){ HashmapFlagStringify(pB); }` |
|    462 |  197 | `		zA = (const char *)SyBlobData(&pA->sBlob);` |
|    462 |  198 | `		zB = (const char *)SyBlobData(&pB->sBlob);` |
|    462 |  199 | `		nA = SyBlobLength(&pA->sBlob);` |
|    462 |  200 | `		nB = SyBlobLength(&pB->sBlob);` |
|    462 |  201 | `		if( base == 6 ){` |
|    173 |  202 | `			rc = PH7_StrNatCmp(zA,(int)nA,zB,(int)nB,bFold);` |
|     88 |  203 | `		}else{` |
|      - |  204 | `			/* Lexicographic comparison (binary-safe), case-folded on request. */` |
|    292 |  205 | `			nMin = nA < nB ? nA : nB;` |
|    292 |  206 | `			rc = 0;` |
|    480 |  207 | `			for( i = 0 ; i < nMin ; ++i ){` |
|    372 |  208 | `				int ca = (unsigned char)zA[i];` |
|    372 |  209 | `				int cb = (unsigned char)zB[i];` |
|    372 |  210 | `				if( bFold ){ ca = SyToLower(ca); cb = SyToLower(cb); }` |
|    372 |  211 | `				if( ca != cb ){ rc = ca < cb ? -1 : 1; break; }` |
|     98 |  212 | `			}` |
|    292 |  213 | `			if( rc == 0 ){` |
|    112 |  214 | `				if( nA < nB ) rc = -1;` |
|     78 |  215 | `				else if( nA > nB ) rc = 1;` |
|     54 |  216 | `			}` |
|      - |  217 | `		}` |
|      - |  218 | `	}` |
|    691 |  219 | `	return rc;` |
|      5 |  220 | `}` |
|      - |  221 | `/*` |
|      - |  222 | ` * Are two live values equal under an array_unique() sort_flags? A non-mutating` |
|      - |  223 | ` * wrapper (works on private copies): base 0 = SORT_REGULAR loose comparison,` |
|      - |  224 | ` * explicit flags route through HashmapScalarFlagCmp. Used by array_unique.` |
|      - |  225 | ` */` |
|    164 |  226 | `PH7_PRIVATE int HashmapValueFlagEqual(ph7_vm *pVm,ph7_value *pA,ph7_value *pB,int base,int bFold)` |
|      5 |  227 | `{` |
|      - |  228 | `	ph7_value sA,sB;` |
|      - |  229 | `	sxi32 rc;` |
|    169 |  230 | `	PH7_MemObjInit(pVm,&sA);` |
|    169 |  231 | `	PH7_MemObjInit(pVm,&sB);` |
|    169 |  232 | `	PH7_MemObjStore(pA,&sA);` |
|    169 |  233 | `	PH7_MemObjStore(pB,&sB);` |
|    169 |  234 | `	if( base == 0 ){` |
|     14 |  235 | `		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0); /* SORT_REGULAR: loose comparison */` |
|      8 |  236 | `	}else{` |
|    157 |  237 | `		rc = HashmapScalarFlagCmp(&sA,&sB,base,bFold);` |
|      - |  238 | `	}` |
|    169 |  239 | `	PH7_MemObjRelease(&sA);` |
|    169 |  240 | `	PH7_MemObjRelease(&sB);` |
|    169 |  241 | `	return rc == 0;` |
|      5 |  242 | `}` |
|      - |  243 | `/*` |
|      - |  244 | ` * Compare two node VALUES under php's sort_flags (base 0 = SORT_REGULAR uses the` |
|      - |  245 | ` * standard value comparison; explicit flags route through HashmapScalarFlagCmp).` |
|      - |  246 | ` */` |
|    412 |  247 | `static sxi32 HashmapFlagValueCmp(ph7_hashmap_node *pA,ph7_hashmap_node *pB,sxi32 iFlags)` |
|      4 |  248 | `{` |
|      - |  249 | `	ph7_value sA,sB;` |
|    416 |  250 | `	int base = iFlags & ~8;      /* strip SORT_FLAG_CASE */` |
|    416 |  251 | `	int bFold = (iFlags & 8) != 0;` |
|      - |  252 | `	sxi32 rc;` |
|    416 |  253 | `	if( base == 0 ){` |
|      - |  254 | `		/* SORT_REGULAR */` |
|    ! 0 |  255 | `		return HashmapNodeCmp(pA,pB,FALSE);` |
|      - |  256 | `	}` |
|    416 |  257 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|    416 |  258 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|    416 |  259 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|    416 |  260 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|    416 |  261 | `	rc = HashmapScalarFlagCmp(&sA,&sB,base,bFold);` |
|    416 |  262 | `	PH7_MemObjRelease(&sA);` |
|    416 |  263 | `	PH7_MemObjRelease(&sB);` |
|    416 |  264 | `	return rc;` |
|    210 |  265 | `}` |
|  87306 |  266 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|      5 |  267 | `{` |
|  87311 |  268 | `	if( pCmpData == 0 ){` |
|      - |  269 | `		/* SORT_REGULAR fast path */` |
|  86957 |  270 | `		return HashmapNodeCmp(pA,pB,FALSE);` |
|      - |  271 | `	}` |
|    358 |  272 | `	return HashmapFlagValueCmp(pA,pB,SX_PTR_TO_INT(pCmpData));` |
|  43507 |  273 | `}` |
|      - |  274 | `/*` |
|      - |  275 | ` * Materialise a node's KEY as a scalar ph7_value (int key -> integer, string key` |
|      - |  276 | ` * -> string) for a flag-aware key comparison.` |
|      - |  277 | ` */` |
|    656 |  278 | `static void HashmapNodeKeyToValue(ph7_hashmap_node *pNode,ph7_value *pOut)` |
|      4 |  279 | `{` |
|    660 |  280 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|     96 |  281 | `		PH7_MemObjInitFromInt(pNode->pMap->pVm,pOut,pNode->xKey.iKey);` |
|     49 |  282 | `	}else{` |
|    566 |  283 | `		PH7_MemObjInitFromString(pNode->pMap->pVm,pOut,0);` |
|    847 |  284 | `		PH7_MemObjStringAppend(pOut,(const char *)SyBlobData(&pNode->xKey.sKey),` |
|    281 |  285 | `			SyBlobLength(&pNode->xKey.sKey));` |
|      - |  286 | `	}` |
|    660 |  287 | `}` |
|      - |  288 | `/*` |
|      - |  289 | ` * Shared key comparison for ksort()/krsort() under SORT_REGULAR: php compares` |
|      - |  290 | ` * two array KEYS exactly the way it compares two VALUES, so materialise them` |
|      - |  291 | `` * and hand them to the standard comparison — the same one sort()/`<=>` use.`` |
|      - |  292 | ` *` |
|      - |  293 | ` * The hand-rolled version this replaces got the mixed int/string case right but` |
|      - |  294 | ` * compared two STRING keys BYTEWISE, so two numeric strings sorted by their` |
|      - |  295 | ` * bytes: ksort(['10.0'=>1,'9.0'=>2]) answered ['10.0','9.0'] where php answers` |
|      - |  296 | ` * ['9.0','10.0'], ksort(['1e3'=>1,'20'=>2]) put 1e3 (1000) first, and keys that` |
|      - |  297 | ` * compare EQUAL ('1.0', '01', 1) lost php's stable order.` |
|      - |  298 | ` */` |
|    228 |  299 | `static sxi32 HashmapKeyNodeCmp(ph7_hashmap_node *pA,ph7_hashmap_node *pB)` |
|      2 |  300 | `{` |
|      - |  301 | `	ph7_value sA,sB;` |
|      - |  302 | `	sxi32 rc;` |
|    230 |  303 | `	if( pA->iType == HASHMAP_INT_NODE && pB->iType == HASHMAP_INT_NODE ){` |
|      - |  304 | `		/* Two integer keys: the common case, and no allocation needed */` |
|     23 |  305 | `		return pA->xKey.iKey < pB->xKey.iKey ? -1 : (pA->xKey.iKey > pB->xKey.iKey ? 1 : 0);` |
|      - |  306 | `	}` |
|    208 |  307 | `	HashmapNodeKeyToValue(pA,&sA);` |
|    208 |  308 | `	HashmapNodeKeyToValue(pB,&sB);` |
|    208 |  309 | `	rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|    208 |  310 | `	PH7_MemObjRelease(&sA);` |
|    208 |  311 | `	PH7_MemObjRelease(&sB);` |
|    208 |  312 | `	return rc;` |
|    116 |  313 | `}` |
|      - |  314 | `/*` |
|      - |  315 | ` * Compare two node KEYS under php's sort_flags. base 0 = SORT_REGULAR keeps the` |
|      - |  316 | ` * php-8 mixed int/string key semantics (HashmapKeyNodeCmp); explicit flags route` |
|      - |  317 | ` * the materialised keys through HashmapScalarFlagCmp.` |
|      - |  318 | ` */` |
|    122 |  319 | `static sxi32 HashmapFlagKeyCmp(ph7_hashmap_node *pA,ph7_hashmap_node *pB,sxi32 iFlags)` |
|      3 |  320 | `{` |
|      - |  321 | `	ph7_value sA,sB;` |
|    125 |  322 | `	int base = iFlags & ~8;      /* strip SORT_FLAG_CASE */` |
|    125 |  323 | `	int bFold = (iFlags & 8) != 0;` |
|      - |  324 | `	sxi32 rc;` |
|    125 |  325 | `	if( base == 0 ){` |
|    ! 0 |  326 | `		return HashmapKeyNodeCmp(pA,pB);` |
|      - |  327 | `	}` |
|    125 |  328 | `	HashmapNodeKeyToValue(pA,&sA);` |
|    125 |  329 | `	HashmapNodeKeyToValue(pB,&sB);` |
|    125 |  330 | `	rc = HashmapScalarFlagCmp(&sA,&sB,base,bFold);` |
|    125 |  331 | `	PH7_MemObjRelease(&sA);` |
|    125 |  332 | `	PH7_MemObjRelease(&sB);` |
|    125 |  333 | `	return rc;` |
|     64 |  334 | `}` |
|      - |  335 | `/*` |
|      - |  336 | ` * Node comparison callback: Compare nodes by keys only.` |
|      - |  337 | ` * used-by: [ksort()]` |
|      - |  338 | ` */` |
|    284 |  339 | `static sxi32 HashmapCmpCallback2(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|      4 |  340 | `{` |
|    288 |  341 | `	if( pCmpData == 0 ){` |
|    170 |  342 | `		return HashmapKeyNodeCmp(pA,pB);` |
|      - |  343 | `	}` |
|    119 |  344 | `	return HashmapFlagKeyCmp(pA,pB,SX_PTR_TO_INT(pCmpData));` |
|    146 |  345 | `}` |
|      - |  346 | `/*` |
|      - |  347 | ` * Node comparison callback.` |
|      - |  348 | ` * Used by: [rsort(),arsort()];` |
|      - |  349 | ` */` |
|    187 |  350 | `static sxi32 HashmapCmpCallback3(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|      4 |  351 | `{` |
|    191 |  352 | `	if( pCmpData == 0 ){` |
|      - |  353 | `		/* SORT_REGULAR fast path, reversed */` |
|    131 |  354 | `		return -HashmapNodeCmp(pA,pB,FALSE);` |
|      - |  355 | `	}` |
|     61 |  356 | `	return -HashmapFlagValueCmp(pA,pB,SX_PTR_TO_INT(pCmpData));` |
|    100 |  357 | `}` |
|      - |  358 | `/*` |
|      - |  359 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|      - |  360 | ` * used-by: [usort(),uasort()]` |
|      - |  361 | ` */` |
|    198 |  362 | `static sxi32 HashmapCmpCallback4(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|      4 |  363 | `{` |
|      - |  364 | `	ph7_value sResult,*pCallback;` |
|      - |  365 | `	ph7_value *pV1,*pV2;` |
|      - |  366 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|      - |  367 | `	sxi32 rc;` |
|      - |  368 | `	/* Point to the desired callback */` |
|    202 |  369 | `	pCallback = (ph7_value *)pCmpData;` |
|    202 |  370 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|      - |  371 | `		/* A previous comparison already raised: stop invoking the callback so` |
|      - |  372 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|     14 |  373 | `		return 0;` |
|      - |  374 | `	}` |
|      - |  375 | `	/* initialize the result value */` |
|    190 |  376 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|      - |  377 | `	/* Extract nodes values */` |
|    190 |  378 | `	pV1 = HashmapExtractNodeValue(pA);` |
|    190 |  379 | `	pV2 = HashmapExtractNodeValue(pB);` |
|    190 |  380 | `	apArg[0] = pV1;` |
|    190 |  381 | `	apArg[1] = pV2;` |
|      - |  382 | `	/* Invoke the callback */` |
|    190 |  383 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|    190 |  384 | `	if( rc == PH7_EXCEPTION ){` |
|      - |  385 | `		/* The comparator raised: flag it so the sort driver aborts and` |
|      - |  386 | `		 * propagates, and order this pair arbitrarily for the rest of the run. */` |
|     10 |  387 | `		pA->pMap->pVm->iCmpCallbackExc = 1;` |
|     10 |  388 | `		rc = 0;` |
|    186 |  389 | `	}else if( rc != SXRET_OK ){` |
|      - |  390 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|    ! 0 |  391 | `		rc = -1; /* Set a dummy result */` |
|    ! 0 |  392 | `	}else{` |
|      - |  393 | `		/* Extract callback result */` |
|    182 |  394 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|      - |  395 | `			/* Perform an int cast */` |
|    ! 0 |  396 | `			PH7_MemObjToInteger(&sResult);` |
|    ! 0 |  397 | `		}` |
|    182 |  398 | `		rc = (sxi32)sResult.x.iVal;` |
|      - |  399 | `	}` |
|    190 |  400 | `	PH7_MemObjRelease(&sResult);` |
|      - |  401 | `	/* Callback result */` |
|    190 |  402 | `	return rc;` |
|    103 |  403 | `}` |
|      - |  404 | `/*` |
|      - |  405 | ` * Node comparison callback: Compare nodes by keys only.` |
|      - |  406 | ` * used-by: [krsort()]` |
|      - |  407 | ` */` |
|     66 |  408 | `static sxi32 HashmapCmpCallback5(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|      2 |  409 | `{` |
|     68 |  410 | `	if( pCmpData == 0 ){` |
|     61 |  411 | `		return -HashmapKeyNodeCmp(pA,pB); /* Reverse result */` |
|      - |  412 | `	}` |
|      8 |  413 | `	return -HashmapFlagKeyCmp(pA,pB,SX_PTR_TO_INT(pCmpData));` |
|     35 |  414 | `}` |
|      - |  415 | `/*` |
|      - |  416 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|      - |  417 | ` * used-by: [uksort()]` |
|      - |  418 | ` */` |
|      6 |  419 | `static sxi32 HashmapCmpCallback6(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|      1 |  420 | `{` |
|      - |  421 | `	ph7_value sResult,*pCallback;` |
|      - |  422 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|      - |  423 | `	ph7_value sK1,sK2;` |
|      - |  424 | `	sxi32 rc;` |
|      - |  425 | `	/* Point to the desired callback */` |
|      7 |  426 | `	pCallback = (ph7_value *)pCmpData;` |
|      7 |  427 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|      - |  428 | `		/* A previous comparison already raised: stop invoking the callback so` |
|      - |  429 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|    ! 0 |  430 | `		return 0;` |
|      - |  431 | `	}` |
|      - |  432 | `	/* initialize the result value */` |
|      7 |  433 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|      7 |  434 | `	PH7_MemObjInit(pA->pMap->pVm,&sK1);` |
|      7 |  435 | `	PH7_MemObjInit(pA->pMap->pVm,&sK2);` |
|      - |  436 | `	/* Extract nodes keys */` |
|      7 |  437 | `	PH7_HashmapExtractNodeKey(pA,&sK1);` |
|      7 |  438 | `	PH7_HashmapExtractNodeKey(pB,&sK2);` |
|      7 |  439 | `	apArg[0] = &sK1;` |
|      7 |  440 | `	apArg[1] = &sK2;` |
|      - |  441 | `	/* Mark keys as constants */` |
|      7 |  442 | `	sK1.nIdx = SXU32_HIGH;` |
|      7 |  443 | `	sK2.nIdx = SXU32_HIGH;` |
|      - |  444 | `	/* Invoke the callback */` |
|      7 |  445 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|      7 |  446 | `	if( rc == PH7_EXCEPTION ){` |
|      - |  447 | `		/* The comparator raised: flag it so the sort driver aborts and` |
|      - |  448 | `		 * propagates, and order this pair arbitrarily for the rest of the run. */` |
|    ! 0 |  449 | `		pA->pMap->pVm->iCmpCallbackExc = 1;` |
|    ! 0 |  450 | `		rc = 0;` |
|      7 |  451 | `	}else if( rc != SXRET_OK ){` |
|      - |  452 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|    ! 0 |  453 | `		rc = -1; /* Set a dummy result */` |
|    ! 0 |  454 | `	}else{` |
|      - |  455 | `		/* Extract callback result */` |
|      7 |  456 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|      - |  457 | `			/* Perform an int cast */` |
|    ! 0 |  458 | `			PH7_MemObjToInteger(&sResult);` |
|    ! 0 |  459 | `		}` |
|      7 |  460 | `		rc = (sxi32)sResult.x.iVal;` |
|      - |  461 | `	}` |
|      7 |  462 | `	PH7_MemObjRelease(&sResult);` |
|      7 |  463 | `	PH7_MemObjRelease(&sK1);` |
|      7 |  464 | `	PH7_MemObjRelease(&sK2);` |
|      - |  465 | `	/* Callback result */` |
|      7 |  466 | `	return rc;` |
|      4 |  467 | `}` |
|      - |  468 | `/*` |
|      - |  469 | ` * Node comparison callback: Random node comparison.` |
|      - |  470 | ` * used-by: [shuffle()]` |
|      - |  471 | ` */` |
|     20 |  472 | `PH7_PRIVATE sxi32 HashmapCmpCallback7(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|      1 |  473 | `{` |
|      - |  474 | `	sxu32 n;` |
|     10 |  475 | `	SXUNUSED(pB); /* cc warning */` |
|     10 |  476 | `	SXUNUSED(pCmpData);` |
|      - |  477 | `	/* Grab a random number from the MT19937 generator so shuffle()/array_rand()` |
|      - |  478 | `	 * respond to srand()/mt_srand() (reproducible under a seed), like php. This` |
|      - |  479 | `	 * is a random-comparator merge sort, not php's Fisher-Yates, so the ordering` |
|      - |  480 | `	 * is deterministic-under-seed but not value-parity with php. */` |
|     21 |  481 | `	n = PH7_VmMtRand(pA->pMap->pVm);` |
|      - |  482 | `	/* if the random number is odd then the first node 'pA' is greater then` |
|      - |  483 | `	 * the second node 'pB'. Otherwise the reverse is assumed.` |
|      - |  484 | `	 */` |
|     21 |  485 | `	return n&1 ? 1 : -1;` |
|      1 |  486 | `}` |
|      - |  487 | `/*` |
|      - |  488 | ` * Rehash all nodes keys after a merge-sort have been applied.` |
|      - |  489 | ` * Used by [sort(),usort() and rsort()].` |
|      - |  490 | ` */` |
|   1280 |  491 | `PH7_PRIVATE void HashmapSortRehash(ph7_hashmap *pMap)` |
|      5 |  492 | `{` |
|      - |  493 | `	ph7_hashmap_node *p,*pLast;` |
|      - |  494 | `	sxu32 i;` |
|      - |  495 | `	/* Rehash all entries */` |
|   1285 |  496 | `	pLast = p = pMap->pFirst;` |
|   1285 |  497 | `	pMap->iNextIdx = 0;` |
|   1285 |  498 | `	pMap->bIntKeySeen = 0; /* Reset the automatic index */` |
|   1285 |  499 | `	i = 0;` |
|   9761 |  500 | `	for( ;; ){` |
|  19527 |  501 | `		if( i >= pMap->nEntry ){` |
|   1285 |  502 | `			pMap->pLast = pLast; /* Fix the last link broken by the merge-sort */` |
|   1285 |  503 | `			break;` |
|      - |  504 | `		}` |
|  18247 |  505 | `		if( p->iType == HASHMAP_BLOB_NODE ){` |
|      - |  506 | `			/* Do not maintain index association as requested by the PHP specification */` |
|     11 |  507 | `			SyBlobRelease(&p->xKey.sKey);` |
|      - |  508 | `			/* Change key type */` |
|     11 |  509 | `			p->iType = HASHMAP_INT_NODE;` |
|      5 |  510 | `		}` |
|  18247 |  511 | `		HashmapRehashIntNode(p);` |
|      - |  512 | `		/* Point to the next entry */` |
|  18247 |  513 | `		i++;` |
|  18247 |  514 | `		pLast = p;` |
|  18247 |  515 | `		p = p->pPrev; /* Reverse link */` |
|      5 |  516 | `	}` |
|   1285 |  517 | `}` |
|      - |  518 | `/*` |
|      - |  519 | ` * Array functions implementation.` |
|      - |  520 | ` * Status:` |
|      - |  521 | ` *  Stable.` |
|      - |  522 | ` */` |
|      - |  523 | `/*` |
|      - |  524 | ` * Reset / report the comparator-throw flag around a FLAG sort. The string sort` |
|      - |  525 | ` * flags coerce their operands user-visibly (HashmapScalarFlagCmp), and a` |
|      - |  526 | ` * not-stringable object raises php's Error there; the comparator can only flag` |
|      - |  527 | ` * it, so every flag-sort driver clears the flag before its merge sort and` |
|      - |  528 | ` * answers PH7_EXCEPTION after it. php's array is sorted after the throw too, so` |
|      - |  529 | ` * the rehash still runs.` |
|      - |  530 | ` */` |
|   1474 |  531 | `static sxi32 HashmapFlagSortStatus(ph7_context *pCtx)` |
|      5 |  532 | `{` |
|   1479 |  533 | `	if( pCtx->pVm->iCmpCallbackExc ){` |
|     25 |  534 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|     25 |  535 | `		pCtx->nThrowRc = PH7_EXCEPTION;` |
|     25 |  536 | `		return PH7_EXCEPTION;` |
|      - |  537 | `	}` |
|   1455 |  538 | `	return PH7_OK;` |
|    742 |  539 | `}` |
|      - |  540 | `/*` |
|      - |  541 | ` * bool sort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|      - |  542 | ` * Sort an array.` |
|      - |  543 | ` * Parameters` |
|      - |  544 | ` *  $array` |
|      - |  545 | ` *   The input array.` |
|      - |  546 | ` * $sort_flags` |
|      - |  547 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|      - |  548 | ` *  Sorting type flags:` |
|      - |  549 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|      - |  550 | ` *   SORT_NUMERIC - compare items numerically` |
|      - |  551 | ` *   SORT_STRING - compare items as strings` |
|      - |  552 | ` * Return` |
|      - |  553 | ` *  TRUE on success or FALSE on failure.` |
|      - |  554 | ` *` |
|      - |  555 | ` */` |
|   1220 |  556 | `PH7_PRIVATE int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  557 | `{` |
|      - |  558 | `	ph7_hashmap *pMap;` |
|      - |  559 | `	/* Make sure we are dealing with a valid hashmap */` |
|   1225 |  560 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - |  561 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  562 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  563 | `		return PH7_OK;` |
|      - |  564 | `	}` |
|      - |  565 | `	/* Point to the internal representation of the input hashmap */` |
|   1225 |  566 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|   1225 |  567 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|   1225 |  568 | `	if( pMap->nEntry > 1 ){` |
|   1215 |  569 | `		sxi32 iCmpFlags = 0;` |
|   1215 |  570 | `		if( nArg > 1 ){` |
|      - |  571 | `			/* Extract comparison flags */` |
|     58 |  572 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     27 |  573 | `		}` |
|      - |  574 | `		/* Do the merge sort */` |
|   1215 |  575 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|   1215 |  576 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|      - |  577 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|   1215 |  578 | `		HashmapSortRehash(pMap);` |
|    616 |  579 | `	}else if( pMap->nEntry == 1 ){` |
|      - |  580 | `		/* php reindexes even a single-element array: a string key becomes 0 */` |
|      7 |  581 | `		HashmapSortRehash(pMap);` |
|      3 |  582 | `	}` |
|      - |  583 | `	{` |
|   1225 |  584 | `		sxi32 rcCmp = HashmapFlagSortStatus(pCtx);` |
|   1225 |  585 | `		if( rcCmp != PH7_OK ){` |
|     13 |  586 | `			return rcCmp;` |
|      - |  587 | `		}` |
|      - |  588 | `	}` |
|      - |  589 | `	/* All done,return TRUE */` |
|   1213 |  590 | `	ph7_result_bool(pCtx,1);` |
|   1213 |  591 | `	return PH7_OK;` |
|    615 |  592 | `}` |
|      - |  593 | `/*` |
|      - |  594 | ` * bool asort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|      - |  595 | ` *  Sort an array and maintain index association.` |
|      - |  596 | ` * Parameters` |
|      - |  597 | ` *  $array` |
|      - |  598 | ` *   The input array.` |
|      - |  599 | ` * $sort_flags` |
|      - |  600 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|      - |  601 | ` *  Sorting type flags:` |
|      - |  602 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|      - |  603 | ` *   SORT_NUMERIC - compare items numerically` |
|      - |  604 | ` *   SORT_STRING - compare items as strings` |
|      - |  605 | ` * Return` |
|      - |  606 | ` *  TRUE on success or FALSE on failure.` |
|      - |  607 | ` */` |
|     40 |  608 | `PH7_PRIVATE int ph7_hashmap_asort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  609 | `{` |
|      - |  610 | `	ph7_hashmap *pMap;` |
|      - |  611 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|     45 |  612 | `	if( nArg < 1 ){` |
|    ! 0 |  613 | `		return PH7_VmThrowException(pCtx,` |
|      - |  614 | `			"ArgumentCountError",` |
|      - |  615 | `			"asort() expects at least 1 argument, 0 given"` |
|      - |  616 | `			);` |
|      - |  617 | `	}` |
|      - |  618 | `	/* PHP 8: TypeError if first argument is not an array */` |
|     45 |  619 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     12 |  620 | `		return PH7_VmThrowException(pCtx,` |
|      - |  621 | `			"TypeError",` |
|      - |  622 | `			"asort(): Argument #1 ($array) must be of type array, %s given",` |
|      3 |  623 | `			ph7_type_name(apArg[0])` |
|      - |  624 | `			);` |
|      - |  625 | `	}` |
|      - |  626 | `	/* Point to the internal representation of the input hashmap */` |
|     37 |  627 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|     37 |  628 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     37 |  629 | `	if( pMap->nEntry > 1 ){` |
|     31 |  630 | `		sxi32 iCmpFlags = 0;` |
|     31 |  631 | `		if( nArg > 1 ){` |
|      - |  632 | `			/* Extract comparison flags */` |
|     15 |  633 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|      6 |  634 | `		}` |
|      - |  635 | `		/* Do the merge sort */` |
|     31 |  636 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|     31 |  637 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|      - |  638 | `		/* Fix the last link broken by the merge */` |
|     71 |  639 | `		while(pMap->pLast->pPrev){` |
|     43 |  640 | `			pMap->pLast = pMap->pLast->pPrev;` |
|      3 |  641 | `		}` |
|     14 |  642 | `	}` |
|      - |  643 | `	{` |
|     37 |  644 | `		sxi32 rcCmp = HashmapFlagSortStatus(pCtx);` |
|     37 |  645 | `		if( rcCmp != PH7_OK ){` |
|      5 |  646 | `			return rcCmp;` |
|      - |  647 | `		}` |
|      - |  648 | `	}` |
|      - |  649 | `	/* All done,return TRUE */` |
|     32 |  650 | `	ph7_result_bool(pCtx,1);` |
|     32 |  651 | `	return PH7_OK;` |
|     25 |  652 | `}` |
|      - |  653 | `/*` |
|      - |  654 | ` * bool natsort(array &$array)` |
|      - |  655 | ` * bool natcasesort(array &$array)` |
|      - |  656 | ` *  Sort an array with php's "natural order" algorithm, maintaining index` |
|      - |  657 | ` *  association: exactly asort() under SORT_NATURAL (plus SORT_FLAG_CASE for the` |
|      - |  658 | ` *  case-insensitive twin), which is how php implements them too.` |
|      - |  659 | ` *` |
|      - |  660 | `` *  They used to be PRELUDE wrappers over `uasort($array, 'strnatcmp')`, and that`` |
|      - |  661 | ` *  is a different function: uasort hands each element to a userland callback, so` |
|      - |  662 | `` *  every element had to satisfy strnatcmp's `string` ZPP row. php's natsort`` |
|      - |  663 | ` *  COERCES each element the way any string comparison does, so` |
|      - |  664 | `` *  `natsort([10, "9", null])` — nothing exotic, just a mixed array — was a`` |
|      - |  665 | ` *  TypeError in PHL and a sorted array in php, and an object with no` |
|      - |  666 | ` *  __toString() answered strnatcmp's ZPP TypeError instead of php's coercion` |
|      - |  667 | ` *  Error. Routing through the flag comparator picks up HashmapFlagStringify,` |
|      - |  668 | ` *  which already renders arrays with php's "Array to string conversion" warning` |
|      - |  669 | ` *  and raises the coercion Error once per sort.` |
|      - |  670 | ` */` |
|     38 |  671 | `PH7_PRIVATE int ph7_hashmap_natsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  672 | `{` |
|     41 |  673 | `	const char *zName = ph7_function_name(pCtx);` |
|      - |  674 | `	/* natcasesort() is the SORT_FLAG_CASE twin; match the whole name, not a byte. */` |
|     48 |  675 | `	int bFold = zName && SyStrlen(zName) == sizeof("natcasesort")-1` |
|     57 |  676 | `		&& SyMemcmp(zName,"natcasesort",sizeof("natcasesort")-1) == 0;` |
|      - |  677 | `	ph7_hashmap *pMap;` |
|     41 |  678 | `	if( nArg < 1 ){` |
|    ! 0 |  679 | `		return PH7_VmThrowException(pCtx,` |
|      - |  680 | `			"ArgumentCountError",` |
|    ! 0 |  681 | `			"%s() expects exactly 1 argument, 0 given",zName` |
|      - |  682 | `			);` |
|      - |  683 | `	}` |
|     41 |  684 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      7 |  685 | `		return PH7_VmThrowException(pCtx,` |
|      - |  686 | `			"TypeError",` |
|      - |  687 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|      2 |  688 | `			zName,ph7_type_name(apArg[0])` |
|      - |  689 | `			);` |
|      - |  690 | `	}` |
|     37 |  691 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|     37 |  692 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     37 |  693 | `	if( pMap->nEntry > 1 ){` |
|      - |  694 | `		/* SORT_NATURAL (6), optionally \| SORT_FLAG_CASE (8) — the same iFlags` |
|      - |  695 | `		 * word asort() forwards, so this is asort($a, SORT_NATURAL) exactly. */` |
|     33 |  696 | `		sxi32 iCmpFlags = bFold ? (6\|8) : 6;` |
|     33 |  697 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|     33 |  698 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|     79 |  699 | `		while(pMap->pLast->pPrev){` |
|     49 |  700 | `			pMap->pLast = pMap->pLast->pPrev;` |
|      3 |  701 | `		}` |
|     15 |  702 | `	}` |
|      - |  703 | `	{` |
|     37 |  704 | `		sxi32 rcCmp = HashmapFlagSortStatus(pCtx);` |
|     37 |  705 | `		if( rcCmp != PH7_OK ){` |
|      5 |  706 | `			return rcCmp;` |
|      - |  707 | `		}` |
|      - |  708 | `	}` |
|     33 |  709 | `	ph7_result_bool(pCtx,1);` |
|     33 |  710 | `	return PH7_OK;` |
|     22 |  711 | `}` |
|      - |  712 | `/*` |
|      - |  713 | ` * bool arsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|      - |  714 | ` *  Sort an array in reverse order and maintain index association.` |
|      - |  715 | ` * Parameters` |
|      - |  716 | ` *  $array` |
|      - |  717 | ` *   The input array.` |
|      - |  718 | ` * $sort_flags` |
|      - |  719 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|      - |  720 | ` *  Sorting type flags:` |
|      - |  721 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|      - |  722 | ` *   SORT_NUMERIC - compare items numerically` |
|      - |  723 | ` *   SORT_STRING - compare items as strings` |
|      - |  724 | ` * Return` |
|      - |  725 | ` *  TRUE on success or FALSE on failure.` |
|      - |  726 | ` */` |
|     34 |  727 | `PH7_PRIVATE int ph7_hashmap_arsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  728 | `{` |
|      - |  729 | `	ph7_hashmap *pMap;` |
|      - |  730 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|     39 |  731 | `	if( nArg < 1 ){` |
|    ! 0 |  732 | `		return PH7_VmThrowException(pCtx,` |
|      - |  733 | `			"ArgumentCountError",` |
|      - |  734 | `			"arsort() expects at least 1 argument, 0 given"` |
|      - |  735 | `			);` |
|      - |  736 | `	}` |
|      - |  737 | `	/* PHP 8: TypeError if first argument is not an array */` |
|     39 |  738 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     12 |  739 | `		return PH7_VmThrowException(pCtx,` |
|      - |  740 | `			"TypeError",` |
|      - |  741 | `			"arsort(): Argument #1 ($array) must be of type array, %s given",` |
|      3 |  742 | `			ph7_type_name(apArg[0])` |
|      - |  743 | `			);` |
|      - |  744 | `	}` |
|      - |  745 | `	/* Point to the internal representation of the input hashmap */` |
|     31 |  746 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|     31 |  747 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     31 |  748 | `	if( pMap->nEntry > 1 ){` |
|     27 |  749 | `		sxi32 iCmpFlags = 0;` |
|     27 |  750 | `		if( nArg > 1 ){` |
|      - |  751 | `			/* Extract comparison flags */` |
|     13 |  752 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|      5 |  753 | `		}` |
|      - |  754 | `		/* Do the merge sort */` |
|     27 |  755 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|     27 |  756 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|      - |  757 | `		/* Fix the last link broken by the merge */` |
|     45 |  758 | `		while(pMap->pLast->pPrev){` |
|     20 |  759 | `			pMap->pLast = pMap->pLast->pPrev;` |
|      2 |  760 | `		}` |
|     12 |  761 | `	}` |
|      - |  762 | `	{` |
|     31 |  763 | `		sxi32 rcCmp = HashmapFlagSortStatus(pCtx);` |
|     31 |  764 | `		if( rcCmp != PH7_OK ){` |
|      3 |  765 | `			return rcCmp;` |
|      - |  766 | `		}` |
|      - |  767 | `	}` |
|      - |  768 | `	/* All done,return TRUE */` |
|     28 |  769 | `	ph7_result_bool(pCtx,1);` |
|     28 |  770 | `	return PH7_OK;` |
|     22 |  771 | `}` |
|      - |  772 | `/*` |
|      - |  773 | ` * bool ksort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|      - |  774 | ` *  Sort an array by key.` |
|      - |  775 | ` * Parameters` |
|      - |  776 | ` *  $array` |
|      - |  777 | ` *   The input array.` |
|      - |  778 | ` * $sort_flags` |
|      - |  779 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|      - |  780 | ` *  Sorting type flags:` |
|      - |  781 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|      - |  782 | ` *   SORT_NUMERIC - compare items numerically` |
|      - |  783 | ` *   SORT_STRING - compare items as strings` |
|      - |  784 | ` * Return` |
|      - |  785 | ` *  TRUE on success or FALSE on failure.` |
|      - |  786 | ` */` |
|    104 |  787 | `PH7_PRIVATE int ph7_hashmap_ksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  788 | `{` |
|      - |  789 | `	ph7_hashmap *pMap;` |
|      - |  790 | `	/* Make sure we are dealing with a valid hashmap */` |
|    108 |  791 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - |  792 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  793 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  794 | `		return PH7_OK;` |
|      - |  795 | `	}` |
|      - |  796 | `	/* Point to the internal representation of the input hashmap */` |
|    108 |  797 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|    108 |  798 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|    108 |  799 | `	if( pMap->nEntry > 1 ){` |
|    108 |  800 | `		sxi32 iCmpFlags = 0;` |
|    108 |  801 | `		if( nArg > 1 ){` |
|      - |  802 | `			/* Extract comparison flags */` |
|     55 |  803 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     26 |  804 | `		}` |
|      - |  805 | `		/* Do the merge sort */` |
|    108 |  806 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|    108 |  807 | `		HashmapMergeSort(pMap,HashmapCmpCallback2,SX_INT_TO_PTR(iCmpFlags));` |
|      - |  808 | `		/* Fix the last link broken by the merge */` |
|    214 |  809 | `		while(pMap->pLast->pPrev){` |
|    108 |  810 | `			pMap->pLast = pMap->pLast->pPrev;` |
|      2 |  811 | `		}` |
|     52 |  812 | `	}` |
|      - |  813 | `	{` |
|    108 |  814 | `		sxi32 rcCmp = HashmapFlagSortStatus(pCtx);` |
|    108 |  815 | `		if( rcCmp != PH7_OK ){` |
|    ! 0 |  816 | `			return rcCmp;` |
|      - |  817 | `		}` |
|      - |  818 | `	}` |
|      - |  819 | `	/* All done,return TRUE */` |
|    108 |  820 | `	ph7_result_bool(pCtx,1);` |
|    108 |  821 | `	return PH7_OK;` |
|     56 |  822 | `}` |
|      - |  823 | `/*` |
|      - |  824 | ` * bool krsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|      - |  825 | ` *  Sort an array by key in reverse order.` |
|      - |  826 | ` * Parameters` |
|      - |  827 | ` *  $array` |
|      - |  828 | ` *   The input array.` |
|      - |  829 | ` * $sort_flags` |
|      - |  830 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|      - |  831 | ` *  Sorting type flags:` |
|      - |  832 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|      - |  833 | ` *   SORT_NUMERIC - compare items numerically` |
|      - |  834 | ` *   SORT_STRING - compare items as strings` |
|      - |  835 | ` * Return` |
|      - |  836 | ` *  TRUE on success or FALSE on failure.` |
|      - |  837 | ` */` |
|     30 |  838 | `PH7_PRIVATE int ph7_hashmap_krsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  839 | `{` |
|      - |  840 | `	ph7_hashmap *pMap;` |
|      - |  841 | `	/* Make sure we are dealing with a valid hashmap */` |
|     32 |  842 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - |  843 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  844 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  845 | `		return PH7_OK;` |
|      - |  846 | `	}` |
|      - |  847 | `	/* Point to the internal representation of the input hashmap */` |
|     32 |  848 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|     32 |  849 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     32 |  850 | `	if( pMap->nEntry > 1 ){` |
|     32 |  851 | `		sxi32 iCmpFlags = 0;` |
|     32 |  852 | `		if( nArg > 1 ){` |
|      - |  853 | `			/* Extract comparison flags */` |
|      6 |  854 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|      2 |  855 | `		}` |
|      - |  856 | `		/* Do the merge sort */` |
|     32 |  857 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|     32 |  858 | `		HashmapMergeSort(pMap,HashmapCmpCallback5,SX_INT_TO_PTR(iCmpFlags));` |
|      - |  859 | `		/* Fix the last link broken by the merge */` |
|     54 |  860 | `		while(pMap->pLast->pPrev){` |
|     23 |  861 | `			pMap->pLast = pMap->pLast->pPrev;` |
|      1 |  862 | `		}` |
|     15 |  863 | `	}` |
|      - |  864 | `	{` |
|     32 |  865 | `		sxi32 rcCmp = HashmapFlagSortStatus(pCtx);` |
|     32 |  866 | `		if( rcCmp != PH7_OK ){` |
|    ! 0 |  867 | `			return rcCmp;` |
|      - |  868 | `		}` |
|      - |  869 | `	}` |
|      - |  870 | `	/* All done,return TRUE */` |
|     32 |  871 | `	ph7_result_bool(pCtx,1);` |
|     32 |  872 | `	return PH7_OK;` |
|     17 |  873 | `}` |
|      - |  874 | `/*` |
|      - |  875 | ` * bool rsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|      - |  876 | ` * Sort an array in reverse order.` |
|      - |  877 | ` * Parameters` |
|      - |  878 | ` *  $array` |
|      - |  879 | ` *   The input array.` |
|      - |  880 | ` * $sort_flags` |
|      - |  881 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|      - |  882 | ` *  Sorting type flags:` |
|      - |  883 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|      - |  884 | ` *   SORT_NUMERIC - compare items numerically` |
|      - |  885 | ` *   SORT_STRING - compare items as strings` |
|      - |  886 | ` * Return` |
|      - |  887 | ` *  TRUE on success or FALSE on failure.` |
|      - |  888 | ` */` |
|     24 |  889 | `PH7_PRIVATE int ph7_hashmap_rsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  890 | `{` |
|      - |  891 | `	ph7_hashmap *pMap;` |
|      - |  892 | `	/* Make sure we are dealing with a valid hashmap */` |
|     28 |  893 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - |  894 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  895 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  896 | `		return PH7_OK;` |
|      - |  897 | `	}` |
|      - |  898 | `	/* Point to the internal representation of the input hashmap */` |
|     28 |  899 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|     28 |  900 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     28 |  901 | `	if( pMap->nEntry > 1 ){` |
|     26 |  902 | `		sxi32 iCmpFlags = 0;` |
|     26 |  903 | `		if( nArg > 1 ){` |
|      - |  904 | `			/* Extract comparison flags */` |
|     11 |  905 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|      4 |  906 | `		}` |
|      - |  907 | `		/* Do the merge sort */` |
|     26 |  908 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|     26 |  909 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|      - |  910 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|     26 |  911 | `		HashmapSortRehash(pMap);` |
|     14 |  912 | `	}else if( pMap->nEntry == 1 ){` |
|      - |  913 | `		/* php reindexes even a single-element array: a string key becomes 0 */` |
|      3 |  914 | `		HashmapSortRehash(pMap);` |
|      1 |  915 | `	}` |
|      - |  916 | `	{` |
|     28 |  917 | `		sxi32 rcCmp = HashmapFlagSortStatus(pCtx);` |
|     28 |  918 | `		if( rcCmp != PH7_OK ){` |
|      3 |  919 | `			return rcCmp;` |
|      - |  920 | `		}` |
|      - |  921 | `	}` |
|      - |  922 | `	/* All done,return TRUE */` |
|     25 |  923 | `	ph7_result_bool(pCtx,1);` |
|     25 |  924 | `	return PH7_OK;` |
|     16 |  925 | `}` |
|      - |  926 | `/*` |
|      - |  927 | ` * bool usort(array &$array,callable $cmp_function)` |
|      - |  928 | ` *  Sort an array by values using a user-defined comparison function.` |
|      - |  929 | ` * Parameters` |
|      - |  930 | ` *  $array` |
|      - |  931 | ` *   The input array.` |
|      - |  932 | ` * $cmp_function` |
|      - |  933 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|      - |  934 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|      - |  935 | ` *  to, or greater than the second.` |
|      - |  936 | ` *    int callback ( mixed $a, mixed $b )` |
|      - |  937 | ` * Return` |
|      - |  938 | ` *  TRUE on success or FALSE on failure.` |
|      - |  939 | ` */` |
|     42 |  940 | `PH7_PRIVATE int ph7_hashmap_usort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  941 | `{` |
|      - |  942 | `	ph7_hashmap *pMap;` |
|      - |  943 | `	/* Make sure we are dealing with a valid hashmap */` |
|     46 |  944 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - |  945 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  946 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  947 | `		return PH7_OK;` |
|      - |  948 | `	}` |
|     46 |  949 | `	if( nArg > 1 ){` |
|      - |  950 | `		/* php rejects an uncallable comparator with a TypeError, BEFORE touching the` |
|      - |  951 | `		 * array. PH7 handed it to the dispatcher, which answers NULL in silence -- so` |
|      - |  952 | `		 * usort() with a bogus comparator reordered the array anyway, by nothing. */` |
|     46 |  953 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",0);` |
|     46 |  954 | `		if( rcCb != PH7_OK ){` |
|      7 |  955 | `			return rcCb;` |
|      - |  956 | `		}` |
|     18 |  957 | `	}` |
|      - |  958 | `	/* Point to the internal representation of the input hashmap */` |
|     40 |  959 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|     40 |  960 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     40 |  961 | `	if( pMap->nEntry > 1 ){` |
|     38 |  962 | `		ph7_value *pCallback = 0;` |
|      - |  963 | `		ProcNodeCmp xCmp;` |
|     38 |  964 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|     38 |  965 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|      - |  966 | `			/* Point to the desired callback */` |
|     38 |  967 | `			pCallback = apArg[1];` |
|     21 |  968 | `		}else{` |
|      - |  969 | `			/* Use the default comparison function */` |
|    ! 0 |  970 | `			xCmp = HashmapCmpCallback1;` |
|      - |  971 | `		}` |
|      - |  972 | `		/* Do the merge sort */` |
|     38 |  973 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|     38 |  974 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|      - |  975 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|     38 |  976 | `		HashmapSortRehash(pMap);` |
|     38 |  977 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|      - |  978 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|     10 |  979 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|     10 |  980 | `			return PH7_EXCEPTION;` |
|      4 |  981 | `		}` |
|     16 |  982 | `	}else if( pMap->nEntry == 1 ){` |
|      - |  983 | `		/* php reindexes even a single-element array: a string key becomes 0 */` |
|      3 |  984 | `		HashmapSortRehash(pMap);` |
|      1 |  985 | `	}` |
|      - |  986 | `	/* All done,return TRUE */` |
|     32 |  987 | `	ph7_result_bool(pCtx,1);` |
|     32 |  988 | `	return PH7_OK;` |
|     25 |  989 | `}` |
|      - |  990 | `/*` |
|      - |  991 | ` * bool uasort(array &$array,callable $cmp_function)` |
|      - |  992 | ` *  Sort an array by values using a user-defined comparison function` |
|      - |  993 | ` *  and maintain index association.` |
|      - |  994 | ` * Parameters` |
|      - |  995 | ` *  $array` |
|      - |  996 | ` *   The input array.` |
|      - |  997 | ` * $cmp_function` |
|      - |  998 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|      - |  999 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|      - | 1000 | ` *  to, or greater than the second.` |
|      - | 1001 | ` *    int callback ( mixed $a, mixed $b )` |
|      - | 1002 | ` * Return` |
|      - | 1003 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1004 | ` */` |
|     10 | 1005 | `PH7_PRIVATE int ph7_hashmap_uasort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1006 | `{` |
|      - | 1007 | `	ph7_hashmap *pMap;` |
|      - | 1008 | `	/* Make sure we are dealing with a valid hashmap */` |
|     11 | 1009 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 1010 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1011 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1012 | `		return PH7_OK;` |
|      - | 1013 | `	}` |
|     11 | 1014 | `	if( nArg > 1 ){` |
|      - | 1015 | `		/* php rejects an uncallable comparator with a TypeError, BEFORE touching the` |
|      - | 1016 | `		 * array. PH7 handed it to the dispatcher, which answers NULL in silence -- so` |
|      - | 1017 | `		 * usort() with a bogus comparator reordered the array anyway, by nothing. */` |
|     11 | 1018 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",0);` |
|     11 | 1019 | `		if( rcCb != PH7_OK ){` |
|      5 | 1020 | `			return rcCb;` |
|      - | 1021 | `		}` |
|      3 | 1022 | `	}` |
|      - | 1023 | `	/* Point to the internal representation of the input hashmap */` |
|      7 | 1024 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      7 | 1025 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      7 | 1026 | `	if( pMap->nEntry > 1 ){` |
|      7 | 1027 | `		ph7_value *pCallback = 0;` |
|      - | 1028 | `		ProcNodeCmp xCmp;` |
|      7 | 1029 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|      7 | 1030 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|      - | 1031 | `			/* Point to the desired callback */` |
|      7 | 1032 | `			pCallback = apArg[1];` |
|      4 | 1033 | `		}else{` |
|      - | 1034 | `			/* Use the default comparison function */` |
|    ! 0 | 1035 | `			xCmp = HashmapCmpCallback1;` |
|      - | 1036 | `		}` |
|      - | 1037 | `		/* Do the merge sort */` |
|      7 | 1038 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|      7 | 1039 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|      - | 1040 | `		/* Fix the last link broken by the merge */` |
|     17 | 1041 | `		while(pMap->pLast->pPrev){` |
|     11 | 1042 | `			pMap->pLast = pMap->pLast->pPrev;` |
|      1 | 1043 | `		}` |
|      7 | 1044 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|      - | 1045 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|    ! 0 | 1046 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|    ! 0 | 1047 | `			return PH7_EXCEPTION;` |
|      - | 1048 | `		}` |
|      3 | 1049 | `	}` |
|      - | 1050 | `	/* All done,return TRUE */` |
|      7 | 1051 | `	ph7_result_bool(pCtx,1);` |
|      7 | 1052 | `	return PH7_OK;` |
|      6 | 1053 | `}` |
|      - | 1054 | `/*` |
|      - | 1055 | ` * bool uksort(array &$array,callable $cmp_function)` |
|      - | 1056 | ` *  Sort an array by keys using a user-defined comparison` |
|      - | 1057 | ` *  function and maintain index association.` |
|      - | 1058 | ` * Parameters` |
|      - | 1059 | ` *  $array` |
|      - | 1060 | ` *   The input array.` |
|      - | 1061 | ` * $cmp_function` |
|      - | 1062 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|      - | 1063 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|      - | 1064 | ` *  to, or greater than the second.` |
|      - | 1065 | ` *    int callback ( mixed $a, mixed $b )` |
|      - | 1066 | ` * Return` |
|      - | 1067 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1068 | ` */` |
|      4 | 1069 | `PH7_PRIVATE int ph7_hashmap_uksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1070 | `{` |
|      - | 1071 | `	ph7_hashmap *pMap;` |
|      - | 1072 | `	/* Make sure we are dealing with a valid hashmap */` |
|      5 | 1073 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 1074 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1075 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1076 | `		return PH7_OK;` |
|      - | 1077 | `	}` |
|      5 | 1078 | `	if( nArg > 1 ){` |
|      - | 1079 | `		/* php rejects an uncallable comparator with a TypeError, BEFORE touching the` |
|      - | 1080 | `		 * array. PH7 handed it to the dispatcher, which answers NULL in silence -- so` |
|      - | 1081 | `		 * usort() with a bogus comparator reordered the array anyway, by nothing. */` |
|      5 | 1082 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",0);` |
|      5 | 1083 | `		if( rcCb != PH7_OK ){` |
|      3 | 1084 | `			return rcCb;` |
|      - | 1085 | `		}` |
|      1 | 1086 | `	}` |
|      - | 1087 | `	/* Point to the internal representation of the input hashmap */` |
|      3 | 1088 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      3 | 1089 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      3 | 1090 | `	if( pMap->nEntry > 1 ){` |
|      3 | 1091 | `		ph7_value *pCallback = 0;` |
|      - | 1092 | `		ProcNodeCmp xCmp;` |
|      3 | 1093 | `		xCmp = HashmapCmpCallback6; /* User-defined function as the comparison callback */` |
|      3 | 1094 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|      - | 1095 | `			/* Point to the desired callback */` |
|      3 | 1096 | `			pCallback = apArg[1];` |
|      2 | 1097 | `		}else{` |
|      - | 1098 | `			/* Use the default comparison function */` |
|    ! 0 | 1099 | `			xCmp = HashmapCmpCallback2;` |
|      - | 1100 | `		}` |
|      - | 1101 | `		/* Do the merge sort */` |
|      3 | 1102 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|      3 | 1103 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|      - | 1104 | `		/* Fix the last link broken by the merge */` |
|      3 | 1105 | `		while(pMap->pLast->pPrev){` |
|    ! 0 | 1106 | `			pMap->pLast = pMap->pLast->pPrev;` |
|    ! 0 | 1107 | `		}` |
|      3 | 1108 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|      - | 1109 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|    ! 0 | 1110 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|    ! 0 | 1111 | `			return PH7_EXCEPTION;` |
|      - | 1112 | `		}` |
|      1 | 1113 | `	}` |
|      - | 1114 | `	/* All done,return TRUE */` |
|      3 | 1115 | `	ph7_result_bool(pCtx,1);` |
|      3 | 1116 | `	return PH7_OK;` |
|      3 | 1117 | `}` |
|      - | 1118 |  |
