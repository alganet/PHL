# src/ph7/hashmap_sort.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 611/668 lines (91.47%)

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
|  68138 |   37 | `static ph7_hashmap_node * HashmapNodeMerge(ph7_hashmap_node *pA,ph7_hashmap_node *pB,ProcNodeCmp xCmp,void *pCmpData)` |
|      5 |   38 | `{` |
|      - |   39 | `	ph7_hashmap_node result,*pTail;` |
|      - |   40 | `    /* Prevent compiler warning */` |
|  68143 |   41 | `	result.pNext = result.pPrev = 0;` |
|  68143 |   42 | `	pTail = &result;` |
| 162718 |   43 | `	while( pA && pB ){` |
|  94580 |   44 | `		if( xCmp(pA,pB,pCmpData) <= 0 ){` |
|  73965 |   45 | `			pTail->pPrev = pA;` |
|  73965 |   46 | `			pA->pNext = pTail;` |
|  73965 |   47 | `			pTail = pA;` |
|  73965 |   48 | `			pA = pA->pPrev;` |
|  37057 |   49 | `		}else{` |
|  20620 |   50 | `			pTail->pPrev = pB;` |
|  20620 |   51 | `			pB->pNext = pTail;` |
|  20620 |   52 | `			pTail = pB;` |
|  20620 |   53 | `			pB = pB->pPrev;` |
|      - |   54 | `		}` |
|      5 |   55 | `	}` |
|  68143 |   56 | `	if( pA ){` |
|   5474 |   57 | `		pTail->pPrev = pA;` |
|   5474 |   58 | `		pA->pNext = pTail;` |
|  65321 |   59 | `	}else if( pB ){` |
|  62264 |   60 | `		pTail->pPrev = pB;` |
|  62264 |   61 | `		pB->pNext = pTail;` |
|  31222 |   62 | `	}else{` |
|    415 |   63 | `		pTail->pPrev = pTail->pNext = 0;` |
|      - |   64 | `	}` |
|  68143 |   65 | `	return result.pPrev;` |
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
|   1638 |   79 | `PH7_PRIVATE sxi32 HashmapMergeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData)` |
|      5 |   80 | `{` |
|      - |   81 | `	ph7_hashmap_node *a[N_SORT_BUCKET], *p,*pIn;` |
|      - |   82 | `	sxu32 i;` |
|   1643 |   83 | `	SyZero(a,sizeof(a));` |
|      - |   84 | `	/* Point to the first inserted entry */` |
|   1643 |   85 | `	pIn = pMap->pFirst;` |
|  22009 |   86 | `	while( pIn ){` |
|  20371 |   87 | `		p = pIn;` |
|  20371 |   88 | `		pIn = p->pPrev;` |
|  20371 |   89 | `		p->pPrev = 0;` |
|  37731 |   90 | `		for(i=0; i<N_SORT_BUCKET-1; i++){` |
|  37731 |   91 | `			if( a[i]==0 ){` |
|  20371 |   92 | `				a[i] = p;` |
|  20371 |   93 | `				break;` |
|    ! 0 |   94 | `			}else{` |
|  17365 |   95 | `				p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|  17365 |   96 | `				a[i] = 0;` |
|      - |   97 | `			}` |
|   8685 |   98 | `		}` |
|  20371 |   99 | `		if( i==N_SORT_BUCKET-1 ){` |
|      - |  100 | `			/* To get here, there need to be 2^(N_SORT_BUCKET) elements in he input list.` |
|      - |  101 | `			 * But that is impossible.` |
|      - |  102 | `			 */` |
|    ! 0 |  103 | `			a[i] = HashmapNodeMerge(a[i], p,xCmp,pCmpData);` |
|    ! 0 |  104 | `		}` |
|      5 |  105 | `	}` |
|   1643 |  106 | `	p = a[0];` |
|  52421 |  107 | `	for(i=1; i<N_SORT_BUCKET; i++){` |
|      - |  108 | `		/* Higher-index buckets hold EARLIER-inserted (and larger) runs, so the` |
|      - |  109 | `		 * bucket must be the LEFT operand: HashmapNodeMerge favors its left arg on` |
|      - |  110 | `		 * a tie (cmp <= 0), and php's sorts are stable (PHP 8.0+) — equal elements` |
|      - |  111 | `		 * keep their original order. Passing p (the later elements) on the left` |
|      - |  112 | `		 * reversed equal runs (e.g. usort of five tie-keyed items moved the last to` |
|      - |  113 | `		 * the front). */` |
|  50783 |  114 | `		p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|  25394 |  115 | `	}` |
|   1643 |  116 | `	p->pNext = 0;` |
|      - |  117 | `	/* Reflect the change */` |
|   1643 |  118 | `	pMap->pFirst = p;` |
|      - |  119 | `	/* Reset the loop cursor */` |
|   1643 |  120 | `	pMap->pCur = pMap->pFirst;` |
|   1643 |  121 | `	return SXRET_OK;` |
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
|    370 |  140 | `static void HashmapFlagStringify(ph7_value *pVal)` |
|      3 |  141 | `{` |
|    373 |  142 | `	ph7_vm *pVm = pVal->pVm;` |
|    373 |  143 | `	if( !PH7_MemObjIsNotStringable(pVal) ){` |
|    335 |  144 | `		if( PH7_MemObjToStringUV(pVal) == SXRET_OK ){` |
|    335 |  145 | `			return;` |
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
|    188 |  156 | `}` |
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
|    932 |  169 | `static sxi32 HashmapScalarFlagCmp(ph7_value *pA,ph7_value *pB,int base,int bFold)` |
|      5 |  170 | `{` |
|      - |  171 | `	sxi32 rc;` |
|    937 |  172 | `	if( base == 1 ){` |
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
|    709 |  195 | `		if( (pA->iFlags & MEMOBJ_STRING) == 0 ){ HashmapFlagStringify(pA); }` |
|    709 |  196 | `		if( (pB->iFlags & MEMOBJ_STRING) == 0 ){ HashmapFlagStringify(pB); }` |
|    709 |  197 | `		zA = (const char *)SyBlobData(&pA->sBlob);` |
|    709 |  198 | `		zB = (const char *)SyBlobData(&pB->sBlob);` |
|    709 |  199 | `		nA = SyBlobLength(&pA->sBlob);` |
|    709 |  200 | `		nB = SyBlobLength(&pB->sBlob);` |
|    709 |  201 | `		if( base == 6 ){` |
|    189 |  202 | `			rc = PH7_StrNatCmp(zA,(int)nA,zB,(int)nB,bFold);` |
|     96 |  203 | `		}else{` |
|      - |  204 | `			/* Lexicographic comparison (binary-safe), case-folded on request. */` |
|    522 |  205 | `			nMin = nA < nB ? nA : nB;` |
|    522 |  206 | `			rc = 0;` |
|    724 |  207 | `			for( i = 0 ; i < nMin ; ++i ){` |
|    610 |  208 | `				int ca = (unsigned char)zA[i];` |
|    610 |  209 | `				int cb = (unsigned char)zB[i];` |
|    610 |  210 | `				if( bFold ){ ca = SyToLower(ca); cb = SyToLower(cb); }` |
|    610 |  211 | `				if( ca != cb ){ rc = ca < cb ? -1 : 1; break; }` |
|    104 |  212 | `			}` |
|    522 |  213 | `			if( rc == 0 ){` |
|    117 |  214 | `				if( nA < nB ) rc = -1;` |
|     77 |  215 | `				else if( nA > nB ) rc = 1;` |
|     57 |  216 | `			}` |
|      - |  217 | `		}` |
|      - |  218 | `	}` |
|    937 |  219 | `	return rc;` |
|      5 |  220 | `}` |
|      - |  221 | `/*` |
|      - |  222 | ` * Are two live values equal under an array_unique() sort_flags? A non-mutating` |
|      - |  223 | ` * wrapper (works on private copies): base 0 = SORT_REGULAR loose comparison,` |
|      - |  224 | ` * explicit flags route through HashmapScalarFlagCmp. Used by array_unique.` |
|      - |  225 | ` */` |
|    374 |  226 | `PH7_PRIVATE int HashmapValueFlagEqual(ph7_vm *pVm,ph7_value *pA,ph7_value *pB,int base,int bFold)` |
|      3 |  227 | `{` |
|      - |  228 | `	ph7_value sA,sB;` |
|      - |  229 | `	sxi32 rc;` |
|    377 |  230 | `	PH7_MemObjInit(pVm,&sA);` |
|    377 |  231 | `	PH7_MemObjInit(pVm,&sB);` |
|    377 |  232 | `	PH7_MemObjStore(pA,&sA);` |
|    377 |  233 | `	PH7_MemObjStore(pB,&sB);` |
|    377 |  234 | `	if( base == 0 ){` |
|     14 |  235 | `		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0); /* SORT_REGULAR: loose comparison */` |
|      8 |  236 | `	}else{` |
|    365 |  237 | `		rc = HashmapScalarFlagCmp(&sA,&sB,base,bFold);` |
|      - |  238 | `	}` |
|    377 |  239 | `	PH7_MemObjRelease(&sA);` |
|    377 |  240 | `	PH7_MemObjRelease(&sB);` |
|    377 |  241 | `	return rc == 0;` |
|      3 |  242 | `}` |
|      - |  243 | `/*` |
|      - |  244 | ` * Compare two node VALUES under php's sort_flags (base 0 = SORT_REGULAR uses the` |
|      - |  245 | ` * standard value comparison; explicit flags route through HashmapScalarFlagCmp).` |
|      - |  246 | ` */` |
|    504 |  247 | `static sxi32 HashmapFlagValueCmp(ph7_hashmap_node *pA,ph7_hashmap_node *pB,sxi32 iFlags)` |
|      5 |  248 | `{` |
|      - |  249 | `	ph7_value sA,sB;` |
|    509 |  250 | `	int base = iFlags & ~8;      /* strip SORT_FLAG_CASE */` |
|    509 |  251 | `	int bFold = (iFlags & 8) != 0;` |
|      - |  252 | `	sxi32 rc;` |
|    509 |  253 | `	if( base == 0 ){` |
|      - |  254 | `		/* SORT_REGULAR */` |
|     65 |  255 | `		return HashmapNodeCmp(pA,pB,FALSE);` |
|      - |  256 | `	}` |
|    447 |  257 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|    447 |  258 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|    447 |  259 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|    447 |  260 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|    447 |  261 | `	rc = HashmapScalarFlagCmp(&sA,&sB,base,bFold);` |
|    447 |  262 | `	PH7_MemObjRelease(&sA);` |
|    447 |  263 | `	PH7_MemObjRelease(&sB);` |
|    447 |  264 | `	return rc;` |
|    257 |  265 | `}` |
|  93804 |  266 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|      5 |  267 | `{` |
|  93809 |  268 | `	if( pCmpData == 0 ){` |
|      - |  269 | `		/* SORT_REGULAR fast path */` |
|  93441 |  270 | `		return HashmapNodeCmp(pA,pB,FALSE);` |
|      - |  271 | `	}` |
|    372 |  272 | `	return HashmapFlagValueCmp(pA,pB,SX_PTR_TO_INT(pCmpData));` |
|  46754 |  273 | `}` |
|      - |  274 | `/*` |
|      - |  275 | ` * Materialise a node's KEY as a scalar ph7_value (int key -> integer, string key` |
|      - |  276 | ` * -> string) for a flag-aware key comparison.` |
|      - |  277 | ` */` |
|    664 |  278 | `static void HashmapNodeKeyToValue(ph7_hashmap_node *pNode,ph7_value *pOut)` |
|      3 |  279 | `{` |
|    667 |  280 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|    108 |  281 | `		PH7_MemObjInitFromInt(pNode->pMap->pVm,pOut,pNode->xKey.iKey);` |
|     55 |  282 | `	}else{` |
|    561 |  283 | `		PH7_MemObjInitFromString(pNode->pMap->pVm,pOut,0);` |
|    840 |  284 | `		PH7_MemObjStringAppend(pOut,(const char *)SyBlobData(&pNode->xKey.sKey),` |
|    279 |  285 | `			SyBlobLength(&pNode->xKey.sKey));` |
|      - |  286 | `	}` |
|    667 |  287 | `}` |
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
|    226 |  299 | `static sxi32 HashmapKeyNodeCmp(ph7_hashmap_node *pA,ph7_hashmap_node *pB)` |
|      1 |  300 | `{` |
|      - |  301 | `	ph7_value sA,sB;` |
|      - |  302 | `	sxi32 rc;` |
|    227 |  303 | `	if( pA->iType == HASHMAP_INT_NODE && pB->iType == HASHMAP_INT_NODE ){` |
|      - |  304 | `		/* Two integer keys: the common case, and no allocation needed */` |
|     23 |  305 | `		return pA->xKey.iKey < pB->xKey.iKey ? -1 : (pA->xKey.iKey > pB->xKey.iKey ? 1 : 0);` |
|      - |  306 | `	}` |
|    205 |  307 | `	HashmapNodeKeyToValue(pA,&sA);` |
|    205 |  308 | `	HashmapNodeKeyToValue(pB,&sB);` |
|    205 |  309 | `	rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|    205 |  310 | `	PH7_MemObjRelease(&sA);` |
|    205 |  311 | `	PH7_MemObjRelease(&sB);` |
|    205 |  312 | `	return rc;` |
|    114 |  313 | `}` |
|      - |  314 | `/*` |
|      - |  315 | ` * Compare two node KEYS under php's sort_flags. base 0 = SORT_REGULAR keeps the` |
|      - |  316 | ` * php-8 mixed int/string key semantics (HashmapKeyNodeCmp); explicit flags route` |
|      - |  317 | ` * the materialised keys through HashmapScalarFlagCmp.` |
|      - |  318 | ` */` |
|    128 |  319 | `static sxi32 HashmapFlagKeyCmp(ph7_hashmap_node *pA,ph7_hashmap_node *pB,sxi32 iFlags)` |
|      3 |  320 | `{` |
|      - |  321 | `	ph7_value sA,sB;` |
|    131 |  322 | `	int base = iFlags & ~8;      /* strip SORT_FLAG_CASE */` |
|    131 |  323 | `	int bFold = (iFlags & 8) != 0;` |
|      - |  324 | `	sxi32 rc;` |
|    131 |  325 | `	if( base == 0 ){` |
|    ! 0 |  326 | `		return HashmapKeyNodeCmp(pA,pB);` |
|      - |  327 | `	}` |
|    131 |  328 | `	HashmapNodeKeyToValue(pA,&sA);` |
|    131 |  329 | `	HashmapNodeKeyToValue(pB,&sB);` |
|    131 |  330 | `	rc = HashmapScalarFlagCmp(&sA,&sB,base,bFold);` |
|    131 |  331 | `	PH7_MemObjRelease(&sA);` |
|    131 |  332 | `	PH7_MemObjRelease(&sB);` |
|    131 |  333 | `	return rc;` |
|     67 |  334 | `}` |
|      - |  335 | `/*` |
|      - |  336 | ` * Node comparison callback: Compare nodes by keys only.` |
|      - |  337 | ` * used-by: [ksort()]` |
|      - |  338 | ` */` |
|    288 |  339 | `static sxi32 HashmapCmpCallback2(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|      3 |  340 | `{` |
|    291 |  341 | `	if( pCmpData == 0 ){` |
|    167 |  342 | `		return HashmapKeyNodeCmp(pA,pB);` |
|      - |  343 | `	}` |
|    125 |  344 | `	return HashmapFlagKeyCmp(pA,pB,SX_PTR_TO_INT(pCmpData));` |
|    147 |  345 | `}` |
|      - |  346 | `/*` |
|      - |  347 | ` * Node comparison callback.` |
|      - |  348 | ` * Used by: [rsort(),arsort()];` |
|      - |  349 | ` */` |
|    187 |  350 | `static sxi32 HashmapCmpCallback3(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|      3 |  351 | `{` |
|    190 |  352 | `	if( pCmpData == 0 ){` |
|      - |  353 | `		/* SORT_REGULAR fast path, reversed */` |
|    131 |  354 | `		return -HashmapNodeCmp(pA,pB,FALSE);` |
|      - |  355 | `	}` |
|     61 |  356 | `	return -HashmapFlagValueCmp(pA,pB,SX_PTR_TO_INT(pCmpData));` |
|     99 |  357 | `}` |
|      - |  358 | `/*` |
|      - |  359 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|      - |  360 | ` * used-by: [usort(),uasort()]` |
|      - |  361 | ` */` |
|    202 |  362 | `static sxi32 HashmapCmpCallback4(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|      4 |  363 | `{` |
|      - |  364 | `	ph7_value sResult,*pCallback;` |
|      - |  365 | `	ph7_value *pV1,*pV2;` |
|      - |  366 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|      - |  367 | `	sxi32 rc;` |
|      - |  368 | `	/* Point to the desired callback */` |
|    206 |  369 | `	pCallback = (ph7_value *)pCmpData;` |
|    206 |  370 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|      - |  371 | `		/* A previous comparison already raised: stop invoking the callback so` |
|      - |  372 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|     14 |  373 | `		return 0;` |
|      - |  374 | `	}` |
|      - |  375 | `	/* initialize the result value */` |
|    194 |  376 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|      - |  377 | `	/* Extract nodes values */` |
|    194 |  378 | `	pV1 = HashmapExtractNodeValue(pA);` |
|    194 |  379 | `	pV2 = HashmapExtractNodeValue(pB);` |
|    194 |  380 | `	apArg[0] = pV1;` |
|    194 |  381 | `	apArg[1] = pV2;` |
|      - |  382 | `	/* Invoke the callback */` |
|    194 |  383 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|    194 |  384 | `	if( rc == PH7_EXCEPTION ){` |
|      - |  385 | `		/* The comparator raised: flag it so the sort driver aborts and` |
|      - |  386 | `		 * propagates, and order this pair arbitrarily for the rest of the run. */` |
|     10 |  387 | `		pA->pMap->pVm->iCmpCallbackExc = 1;` |
|     10 |  388 | `		rc = 0;` |
|    189 |  389 | `	}else if( rc != SXRET_OK ){` |
|      - |  390 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|    ! 0 |  391 | `		rc = -1; /* Set a dummy result */` |
|    ! 0 |  392 | `	}else{` |
|      - |  393 | `		/* Extract callback result */` |
|    185 |  394 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|      - |  395 | `			/* Perform an int cast */` |
|    ! 0 |  396 | `			PH7_MemObjToInteger(&sResult);` |
|    ! 0 |  397 | `		}` |
|    185 |  398 | `		rc = (sxi32)sResult.x.iVal;` |
|      - |  399 | `	}` |
|    194 |  400 | `	PH7_MemObjRelease(&sResult);` |
|      - |  401 | `	/* Callback result */` |
|    194 |  402 | `	return rc;` |
|    105 |  403 | `}` |
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
|      8 |  419 | `static sxi32 HashmapCmpCallback6(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|      1 |  420 | `{` |
|      - |  421 | `	ph7_value sResult,*pCallback;` |
|      - |  422 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|      - |  423 | `	ph7_value sK1,sK2;` |
|      - |  424 | `	sxi32 rc;` |
|      - |  425 | `	/* Point to the desired callback */` |
|      9 |  426 | `	pCallback = (ph7_value *)pCmpData;` |
|      9 |  427 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|      - |  428 | `		/* A previous comparison already raised: stop invoking the callback so` |
|      - |  429 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|    ! 0 |  430 | `		return 0;` |
|      - |  431 | `	}` |
|      - |  432 | `	/* initialize the result value */` |
|      9 |  433 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|      9 |  434 | `	PH7_MemObjInit(pA->pMap->pVm,&sK1);` |
|      9 |  435 | `	PH7_MemObjInit(pA->pMap->pVm,&sK2);` |
|      - |  436 | `	/* Extract nodes keys */` |
|      9 |  437 | `	PH7_HashmapExtractNodeKey(pA,&sK1);` |
|      9 |  438 | `	PH7_HashmapExtractNodeKey(pB,&sK2);` |
|      9 |  439 | `	apArg[0] = &sK1;` |
|      9 |  440 | `	apArg[1] = &sK2;` |
|      - |  441 | `	/* Mark keys as constants */` |
|      9 |  442 | `	sK1.nIdx = SXU32_HIGH;` |
|      9 |  443 | `	sK2.nIdx = SXU32_HIGH;` |
|      - |  444 | `	/* Invoke the callback */` |
|      9 |  445 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|      9 |  446 | `	if( rc == PH7_EXCEPTION ){` |
|      - |  447 | `		/* The comparator raised: flag it so the sort driver aborts and` |
|      - |  448 | `		 * propagates, and order this pair arbitrarily for the rest of the run. */` |
|    ! 0 |  449 | `		pA->pMap->pVm->iCmpCallbackExc = 1;` |
|    ! 0 |  450 | `		rc = 0;` |
|      9 |  451 | `	}else if( rc != SXRET_OK ){` |
|      - |  452 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|    ! 0 |  453 | `		rc = -1; /* Set a dummy result */` |
|    ! 0 |  454 | `	}else{` |
|      - |  455 | `		/* Extract callback result */` |
|      9 |  456 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|      - |  457 | `			/* Perform an int cast */` |
|    ! 0 |  458 | `			PH7_MemObjToInteger(&sResult);` |
|    ! 0 |  459 | `		}` |
|      9 |  460 | `		rc = (sxi32)sResult.x.iVal;` |
|      - |  461 | `	}` |
|      9 |  462 | `	PH7_MemObjRelease(&sResult);` |
|      9 |  463 | `	PH7_MemObjRelease(&sK1);` |
|      9 |  464 | `	PH7_MemObjRelease(&sK2);` |
|      - |  465 | `	/* Callback result */` |
|      9 |  466 | `	return rc;` |
|      5 |  467 | `}` |
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
|   1418 |  491 | `PH7_PRIVATE void HashmapSortRehash(ph7_hashmap *pMap)` |
|      5 |  492 | `{` |
|      - |  493 | `	ph7_hashmap_node *p,*pLast;` |
|      - |  494 | `	sxu32 i;` |
|      - |  495 | `	/* Rehash all entries */` |
|   1423 |  496 | `	pLast = p = pMap->pFirst;` |
|   1423 |  497 | `	pMap->iNextIdx = 0;` |
|   1423 |  498 | `	pMap->bIntKeySeen = 0; /* Reset the automatic index */` |
|   1423 |  499 | `	i = 0;` |
|  10533 |  500 | `	for( ;; ){` |
|  21071 |  501 | `		if( i >= pMap->nEntry ){` |
|   1423 |  502 | `			pMap->pLast = pLast; /* Fix the last link broken by the merge-sort */` |
|   1423 |  503 | `			break;` |
|      - |  504 | `		}` |
|  19653 |  505 | `		if( p->iType == HASHMAP_BLOB_NODE ){` |
|      - |  506 | `			/* Do not maintain index association as requested by the PHP specification */` |
|     19 |  507 | `			SyBlobRelease(&p->xKey.sKey);` |
|      - |  508 | `			/* Change key type */` |
|     19 |  509 | `			p->iType = HASHMAP_INT_NODE;` |
|      9 |  510 | `		}` |
|  19653 |  511 | `		HashmapRehashIntNode(p);` |
|      - |  512 | `		/* Point to the next entry */` |
|  19653 |  513 | `		i++;` |
|  19653 |  514 | `		pLast = p;` |
|  19653 |  515 | `		p = p->pPrev; /* Reverse link */` |
|      5 |  516 | `	}` |
|   1423 |  517 | `}` |
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
|   1660 |  531 | `static sxi32 HashmapFlagSortStatus(ph7_context *pCtx)` |
|      5 |  532 | `{` |
|   1665 |  533 | `	if( pCtx->pVm->iCmpCallbackExc ){` |
|     25 |  534 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|     25 |  535 | `		pCtx->nThrowRc = PH7_EXCEPTION;` |
|     25 |  536 | `		return PH7_EXCEPTION;` |
|      - |  537 | `	}` |
|   1641 |  538 | `	return PH7_OK;` |
|    835 |  539 | `}` |
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
|   1374 |  556 | `PH7_PRIVATE int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  557 | `{` |
|      - |  558 | `	ph7_hashmap *pMap;` |
|      - |  559 | `	/* Make sure we are dealing with a valid hashmap */` |
|   1379 |  560 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - |  561 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  562 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  563 | `		return PH7_OK;` |
|      - |  564 | `	}` |
|      - |  565 | `	/* Point to the internal representation of the input hashmap */` |
|   1379 |  566 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|   1379 |  567 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|   1379 |  568 | `	if( pMap->nEntry > 1 ){` |
|   1343 |  569 | `		sxi32 iCmpFlags = 0;` |
|   1343 |  570 | `		if( nArg > 1 ){` |
|      - |  571 | `			/* Extract comparison flags */` |
|     58 |  572 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     27 |  573 | `		}` |
|      - |  574 | `		/* Do the merge sort */` |
|   1343 |  575 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|   1343 |  576 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|      - |  577 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|   1343 |  578 | `		HashmapSortRehash(pMap);` |
|    706 |  579 | `	}else if( pMap->nEntry == 1 ){` |
|      - |  580 | `		/* php reindexes even a single-element array: a string key becomes 0 */` |
|     15 |  581 | `		HashmapSortRehash(pMap);` |
|      7 |  582 | `	}` |
|      - |  583 | `	{` |
|   1379 |  584 | `		sxi32 rcCmp = HashmapFlagSortStatus(pCtx);` |
|   1379 |  585 | `		if( rcCmp != PH7_OK ){` |
|     13 |  586 | `			return rcCmp;` |
|      - |  587 | `		}` |
|      - |  588 | `	}` |
|      - |  589 | `	/* All done,return TRUE */` |
|   1367 |  590 | `	ph7_result_bool(pCtx,1);` |
|   1367 |  591 | `	return PH7_OK;` |
|    692 |  592 | `}` |
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
|     50 |  608 | `PH7_PRIVATE int ph7_hashmap_asort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  609 | `{` |
|      - |  610 | `	ph7_hashmap *pMap;` |
|      - |  611 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|     54 |  612 | `	if( nArg < 1 ){` |
|    ! 0 |  613 | `		return PH7_VmThrowException(pCtx,` |
|      - |  614 | `			"ArgumentCountError",` |
|      - |  615 | `			"asort() expects at least 1 argument, 0 given"` |
|      - |  616 | `			);` |
|      - |  617 | `	}` |
|      - |  618 | `	/* PHP 8: TypeError if first argument is not an array */` |
|     54 |  619 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|    ! 0 |  620 | `		return PH7_VmThrowException(pCtx,` |
|      - |  621 | `			"TypeError",` |
|      - |  622 | `			"asort(): Argument #1 ($array) must be of type array, %s given",` |
|    ! 0 |  623 | `			ph7_type_name(apArg[0])` |
|      - |  624 | `			);` |
|      - |  625 | `	}` |
|      - |  626 | `	/* Point to the internal representation of the input hashmap */` |
|     54 |  627 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|     54 |  628 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     54 |  629 | `	if( pMap->nEntry > 1 ){` |
|     48 |  630 | `		sxi32 iCmpFlags = 0;` |
|     48 |  631 | `		if( nArg > 1 ){` |
|      - |  632 | `			/* Extract comparison flags */` |
|     30 |  633 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     13 |  634 | `		}` |
|      - |  635 | `		/* Do the merge sort */` |
|     48 |  636 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|     48 |  637 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|      - |  638 | `		/* Fix the last link broken by the merge */` |
|    112 |  639 | `		while(pMap->pLast->pPrev){` |
|     68 |  640 | `			pMap->pLast = pMap->pLast->pPrev;` |
|      4 |  641 | `		}` |
|     22 |  642 | `	}` |
|      - |  643 | `	{` |
|     54 |  644 | `		sxi32 rcCmp = HashmapFlagSortStatus(pCtx);` |
|     54 |  645 | `		if( rcCmp != PH7_OK ){` |
|      5 |  646 | `			return rcCmp;` |
|      - |  647 | `		}` |
|      - |  648 | `	}` |
|      - |  649 | `	/* All done,return TRUE */` |
|     50 |  650 | `	ph7_result_bool(pCtx,1);` |
|     50 |  651 | `	return PH7_OK;` |
|     29 |  652 | `}` |
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
|     26 |  671 | `PH7_PRIVATE int ph7_hashmap_natsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  672 | `{` |
|     29 |  673 | `	const char *zName = ph7_function_name(pCtx);` |
|      - |  674 | `	/* natcasesort() is the SORT_FLAG_CASE twin; match the whole name, not a byte. */` |
|     34 |  675 | `	int bFold = zName && SyStrlen(zName) == sizeof("natcasesort")-1` |
|     39 |  676 | `		&& SyMemcmp(zName,"natcasesort",sizeof("natcasesort")-1) == 0;` |
|      - |  677 | `	ph7_hashmap *pMap;` |
|     29 |  678 | `	if( nArg < 1 ){` |
|    ! 0 |  679 | `		return PH7_VmThrowException(pCtx,` |
|      - |  680 | `			"ArgumentCountError",` |
|    ! 0 |  681 | `			"%s() expects exactly 1 argument, 0 given",zName` |
|      - |  682 | `			);` |
|      - |  683 | `	}` |
|     29 |  684 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|    ! 0 |  685 | `		return PH7_VmThrowException(pCtx,` |
|      - |  686 | `			"TypeError",` |
|      - |  687 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|    ! 0 |  688 | `			zName,ph7_type_name(apArg[0])` |
|      - |  689 | `			);` |
|      - |  690 | `	}` |
|     29 |  691 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|     29 |  692 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     29 |  693 | `	if( pMap->nEntry > 1 ){` |
|      - |  694 | `		/* SORT_NATURAL (6), optionally \| SORT_FLAG_CASE (8) — the same iFlags` |
|      - |  695 | `		 * word asort() forwards, so this is asort($a, SORT_NATURAL) exactly. */` |
|     25 |  696 | `		sxi32 iCmpFlags = bFold ? (6\|8) : 6;` |
|     25 |  697 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|     25 |  698 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|     57 |  699 | `		while(pMap->pLast->pPrev){` |
|     35 |  700 | `			pMap->pLast = pMap->pLast->pPrev;` |
|      3 |  701 | `		}` |
|     11 |  702 | `	}` |
|      - |  703 | `	{` |
|     29 |  704 | `		sxi32 rcCmp = HashmapFlagSortStatus(pCtx);` |
|     29 |  705 | `		if( rcCmp != PH7_OK ){` |
|      5 |  706 | `			return rcCmp;` |
|      - |  707 | `		}` |
|      - |  708 | `	}` |
|     25 |  709 | `	ph7_result_bool(pCtx,1);` |
|     25 |  710 | `	return PH7_OK;` |
|     16 |  711 | `}` |
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
|     28 |  727 | `PH7_PRIVATE int ph7_hashmap_arsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  728 | `{` |
|      - |  729 | `	ph7_hashmap *pMap;` |
|      - |  730 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|     31 |  731 | `	if( nArg < 1 ){` |
|    ! 0 |  732 | `		return PH7_VmThrowException(pCtx,` |
|      - |  733 | `			"ArgumentCountError",` |
|      - |  734 | `			"arsort() expects at least 1 argument, 0 given"` |
|      - |  735 | `			);` |
|      - |  736 | `	}` |
|      - |  737 | `	/* PHP 8: TypeError if first argument is not an array */` |
|     31 |  738 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|    ! 0 |  739 | `		return PH7_VmThrowException(pCtx,` |
|      - |  740 | `			"TypeError",` |
|      - |  741 | `			"arsort(): Argument #1 ($array) must be of type array, %s given",` |
|    ! 0 |  742 | `			ph7_type_name(apArg[0])` |
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
|     17 |  771 | `}` |
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
|      3 |  788 | `{` |
|      - |  789 | `	ph7_hashmap *pMap;` |
|      - |  790 | `	/* Make sure we are dealing with a valid hashmap */` |
|    107 |  791 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - |  792 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  793 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  794 | `		return PH7_OK;` |
|      - |  795 | `	}` |
|      - |  796 | `	/* Point to the internal representation of the input hashmap */` |
|    107 |  797 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|    107 |  798 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|    107 |  799 | `	if( pMap->nEntry > 1 ){` |
|    107 |  800 | `		sxi32 iCmpFlags = 0;` |
|    107 |  801 | `		if( nArg > 1 ){` |
|      - |  802 | `			/* Extract comparison flags */` |
|     57 |  803 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     27 |  804 | `		}` |
|      - |  805 | `		/* Do the merge sort */` |
|    107 |  806 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|    107 |  807 | `		HashmapMergeSort(pMap,HashmapCmpCallback2,SX_INT_TO_PTR(iCmpFlags));` |
|      - |  808 | `		/* Fix the last link broken by the merge */` |
|    215 |  809 | `		while(pMap->pLast->pPrev){` |
|    110 |  810 | `			pMap->pLast = pMap->pLast->pPrev;` |
|      2 |  811 | `		}` |
|     52 |  812 | `	}` |
|      - |  813 | `	{` |
|    107 |  814 | `		sxi32 rcCmp = HashmapFlagSortStatus(pCtx);` |
|    107 |  815 | `		if( rcCmp != PH7_OK ){` |
|    ! 0 |  816 | `			return rcCmp;` |
|      - |  817 | `		}` |
|      - |  818 | `	}` |
|      - |  819 | `	/* All done,return TRUE */` |
|    107 |  820 | `	ph7_result_bool(pCtx,1);` |
|    107 |  821 | `	return PH7_OK;` |
|     55 |  822 | `}` |
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
|      3 |  890 | `{` |
|      - |  891 | `	ph7_hashmap *pMap;` |
|      - |  892 | `	/* Make sure we are dealing with a valid hashmap */` |
|     27 |  893 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - |  894 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  895 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  896 | `		return PH7_OK;` |
|      - |  897 | `	}` |
|      - |  898 | `	/* Point to the internal representation of the input hashmap */` |
|     27 |  899 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|     27 |  900 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     27 |  901 | `	if( pMap->nEntry > 1 ){` |
|     25 |  902 | `		sxi32 iCmpFlags = 0;` |
|     25 |  903 | `		if( nArg > 1 ){` |
|      - |  904 | `			/* Extract comparison flags */` |
|     11 |  905 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|      4 |  906 | `		}` |
|      - |  907 | `		/* Do the merge sort */` |
|     25 |  908 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|     25 |  909 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|      - |  910 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|     25 |  911 | `		HashmapSortRehash(pMap);` |
|     14 |  912 | `	}else if( pMap->nEntry == 1 ){` |
|      - |  913 | `		/* php reindexes even a single-element array: a string key becomes 0 */` |
|      3 |  914 | `		HashmapSortRehash(pMap);` |
|      1 |  915 | `	}` |
|      - |  916 | `	{` |
|     27 |  917 | `		sxi32 rcCmp = HashmapFlagSortStatus(pCtx);` |
|     27 |  918 | `		if( rcCmp != PH7_OK ){` |
|      3 |  919 | `			return rcCmp;` |
|      - |  920 | `		}` |
|      - |  921 | `	}` |
|      - |  922 | `	/* All done,return TRUE */` |
|     25 |  923 | `	ph7_result_bool(pCtx,1);` |
|     25 |  924 | `	return PH7_OK;` |
|     15 |  925 | `}` |
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
|     44 |  940 | `PH7_PRIVATE int ph7_hashmap_usort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  941 | `{` |
|      - |  942 | `	ph7_hashmap *pMap;` |
|      - |  943 | `	/* Make sure we are dealing with a valid hashmap */` |
|     48 |  944 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - |  945 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  946 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  947 | `		return PH7_OK;` |
|      - |  948 | `	}` |
|     48 |  949 | `	if( nArg > 1 ){` |
|      - |  950 | `		/* php rejects an uncallable comparator with a TypeError, BEFORE touching the` |
|      - |  951 | `		 * array. PH7 handed it to the dispatcher, which answers NULL in silence -- so` |
|      - |  952 | `		 * usort() with a bogus comparator reordered the array anyway, by nothing. */` |
|     48 |  953 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",0);` |
|     48 |  954 | `		if( rcCb != PH7_OK ){` |
|      7 |  955 | `			return rcCb;` |
|      - |  956 | `		}` |
|     19 |  957 | `	}` |
|      - |  958 | `	/* Point to the internal representation of the input hashmap */` |
|     42 |  959 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|     42 |  960 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     42 |  961 | `	if( pMap->nEntry > 1 ){` |
|     40 |  962 | `		ph7_value *pCallback = 0;` |
|      - |  963 | `		ProcNodeCmp xCmp;` |
|     40 |  964 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|     40 |  965 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|      - |  966 | `			/* Point to the desired callback */` |
|     40 |  967 | `			pCallback = apArg[1];` |
|     22 |  968 | `		}else{` |
|      - |  969 | `			/* Use the default comparison function */` |
|    ! 0 |  970 | `			xCmp = HashmapCmpCallback1;` |
|      - |  971 | `		}` |
|      - |  972 | `		/* Do the merge sort */` |
|     40 |  973 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|     40 |  974 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|      - |  975 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|     40 |  976 | `		HashmapSortRehash(pMap);` |
|     40 |  977 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|      - |  978 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|     10 |  979 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|     10 |  980 | `			return PH7_EXCEPTION;` |
|      3 |  981 | `		}` |
|     17 |  982 | `	}else if( pMap->nEntry == 1 ){` |
|      - |  983 | `		/* php reindexes even a single-element array: a string key becomes 0 */` |
|      3 |  984 | `		HashmapSortRehash(pMap);` |
|      1 |  985 | `	}` |
|      - |  986 | `	/* All done,return TRUE */` |
|     33 |  987 | `	ph7_result_bool(pCtx,1);` |
|     33 |  988 | `	return PH7_OK;` |
|     26 |  989 | `}` |
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
|     12 | 1005 | `PH7_PRIVATE int ph7_hashmap_uasort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1006 | `{` |
|      - | 1007 | `	ph7_hashmap *pMap;` |
|      - | 1008 | `	/* Make sure we are dealing with a valid hashmap */` |
|     13 | 1009 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 1010 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1011 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1012 | `		return PH7_OK;` |
|      - | 1013 | `	}` |
|     13 | 1014 | `	if( nArg > 1 ){` |
|      - | 1015 | `		/* php rejects an uncallable comparator with a TypeError, BEFORE touching the` |
|      - | 1016 | `		 * array. PH7 handed it to the dispatcher, which answers NULL in silence -- so` |
|      - | 1017 | `		 * usort() with a bogus comparator reordered the array anyway, by nothing. */` |
|     13 | 1018 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",0);` |
|     13 | 1019 | `		if( rcCb != PH7_OK ){` |
|      5 | 1020 | `			return rcCb;` |
|      - | 1021 | `		}` |
|      4 | 1022 | `	}` |
|      - | 1023 | `	/* Point to the internal representation of the input hashmap */` |
|      9 | 1024 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      9 | 1025 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      9 | 1026 | `	if( pMap->nEntry > 1 ){` |
|      9 | 1027 | `		ph7_value *pCallback = 0;` |
|      - | 1028 | `		ProcNodeCmp xCmp;` |
|      9 | 1029 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|      9 | 1030 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|      - | 1031 | `			/* Point to the desired callback */` |
|      9 | 1032 | `			pCallback = apArg[1];` |
|      5 | 1033 | `		}else{` |
|      - | 1034 | `			/* Use the default comparison function */` |
|    ! 0 | 1035 | `			xCmp = HashmapCmpCallback1;` |
|      - | 1036 | `		}` |
|      - | 1037 | `		/* Do the merge sort */` |
|      9 | 1038 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|      9 | 1039 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|      - | 1040 | `		/* Fix the last link broken by the merge */` |
|     21 | 1041 | `		while(pMap->pLast->pPrev){` |
|     13 | 1042 | `			pMap->pLast = pMap->pLast->pPrev;` |
|      1 | 1043 | `		}` |
|      9 | 1044 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|      - | 1045 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|    ! 0 | 1046 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|    ! 0 | 1047 | `			return PH7_EXCEPTION;` |
|      - | 1048 | `		}` |
|      4 | 1049 | `	}` |
|      - | 1050 | `	/* All done,return TRUE */` |
|      9 | 1051 | `	ph7_result_bool(pCtx,1);` |
|      9 | 1052 | `	return PH7_OK;` |
|      7 | 1053 | `}` |
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
|      6 | 1069 | `PH7_PRIVATE int ph7_hashmap_uksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1070 | `{` |
|      - | 1071 | `	ph7_hashmap *pMap;` |
|      - | 1072 | `	/* Make sure we are dealing with a valid hashmap */` |
|      7 | 1073 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 1074 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1075 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1076 | `		return PH7_OK;` |
|      - | 1077 | `	}` |
|      7 | 1078 | `	if( nArg > 1 ){` |
|      - | 1079 | `		/* php rejects an uncallable comparator with a TypeError, BEFORE touching the` |
|      - | 1080 | `		 * array. PH7 handed it to the dispatcher, which answers NULL in silence -- so` |
|      - | 1081 | `		 * usort() with a bogus comparator reordered the array anyway, by nothing. */` |
|      7 | 1082 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",0);` |
|      7 | 1083 | `		if( rcCb != PH7_OK ){` |
|      3 | 1084 | `			return rcCb;` |
|      - | 1085 | `		}` |
|      2 | 1086 | `	}` |
|      - | 1087 | `	/* Point to the internal representation of the input hashmap */` |
|      5 | 1088 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      5 | 1089 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      5 | 1090 | `	if( pMap->nEntry > 1 ){` |
|      5 | 1091 | `		ph7_value *pCallback = 0;` |
|      - | 1092 | `		ProcNodeCmp xCmp;` |
|      5 | 1093 | `		xCmp = HashmapCmpCallback6; /* User-defined function as the comparison callback */` |
|      5 | 1094 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|      - | 1095 | `			/* Point to the desired callback */` |
|      5 | 1096 | `			pCallback = apArg[1];` |
|      3 | 1097 | `		}else{` |
|      - | 1098 | `			/* Use the default comparison function */` |
|    ! 0 | 1099 | `			xCmp = HashmapCmpCallback2;` |
|      - | 1100 | `		}` |
|      - | 1101 | `		/* Do the merge sort */` |
|      5 | 1102 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|      5 | 1103 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|      - | 1104 | `		/* Fix the last link broken by the merge */` |
|      7 | 1105 | `		while(pMap->pLast->pPrev){` |
|      3 | 1106 | `			pMap->pLast = pMap->pLast->pPrev;` |
|      1 | 1107 | `		}` |
|      5 | 1108 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|      - | 1109 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|    ! 0 | 1110 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|    ! 0 | 1111 | `			return PH7_EXCEPTION;` |
|      - | 1112 | `		}` |
|      2 | 1113 | `	}` |
|      - | 1114 | `	/* All done,return TRUE */` |
|      5 | 1115 | `	ph7_result_bool(pCtx,1);` |
|      5 | 1116 | `	return PH7_OK;` |
|      4 | 1117 | `}` |
|      - | 1118 | `/*` |
|      - | 1119 | ` * bool array_multisort(array &$array, mixed $array1_sort_order = SORT_ASC,` |
|      - | 1120 | ` *                      mixed $array1_sort_flags = SORT_REGULAR, mixed &...$rest)` |
|      - | 1121 | ` *  Sort multiple arrays at once: the argument list is a little language read` |
|      - | 1122 | ` *  left to right — an ARRAY opens a column, and each column may be followed by` |
|      - | 1123 | ` *  at most one sort ORDER (SORT_ASC/SORT_DESC) and at most one sort FLAGS` |
|      - | 1124 | ` *  value, in either order. Rows are compared column by column, a tie in one` |
|      - | 1125 | ` *  column falling through to the next; the resulting permutation is applied to` |
|      - | 1126 | ` *  EVERY column. String keys are kept, numeric keys are renumbered, and the` |
|      - | 1127 | ` *  sort is stable (php 8's own guarantee). php's error shapes, pinned by` |
|      - | 1128 | `` *  probe: a non-int non-array is `Argument #N must be an array or a sort`` |
|      - | 1129 | `` *  flag`; a misplaced or repeated order/flags is the same text with`` |
|      - | 1130 | `` *  `... that has not already been specified`; an int that is no flag at all is`` |
|      - | 1131 | `` *  the ValueError `must be a valid sort flag`; mismatched lengths are`` |
|      - | 1132 | `` *  `Array sizes are inconsistent` with no function prefix; and only`` |
|      - | 1133 | `` *  Argument #1 carries its `($array)` name. php declares the whole list`` |
|      - | 1134 | ` *  prefer-ref (VmBuiltinPrefersRef), so a literal sorts a temporary silently.` |
|      - | 1135 | ` */` |
|      - | 1136 | `/* One column of the multisort: the caller's array plus its sort spec. */` |
|      - | 1137 | `typedef struct MultisortCol MultisortCol;` |
|      - | 1138 | `struct MultisortCol` |
|      - | 1139 | `{` |
|      - | 1140 | `	ph7_value *pArr;        /* The caller's argument slot */` |
|      - | 1141 | `	ph7_hashmap *pMap;      /* Its hashmap */` |
|      - | 1142 | `	ph7_hashmap_node **apNode; /* Nodes in ORIGINAL iteration order */` |
|      - | 1143 | `	sxi32 iFlags;           /* SORT_* comparison flags */` |
|      - | 1144 | `	int iDir;               /* +1 SORT_ASC, -1 SORT_DESC */` |
|      - | 1145 | `	int bOrderSeen;         /* An order argument already attached */` |
|      - | 1146 | `	int bFlagsSeen;         /* A flags argument already attached */` |
|      - | 1147 | `};` |
|      - | 1148 | `/* Compare two ROWS, column by column with each column's own direction/flags. */` |
|     68 | 1149 | `static sxi32 MultisortRowCmp(MultisortCol *aCol,sxu32 nCol,sxu32 iA,sxu32 iB)` |
|      3 | 1150 | `{` |
|      - | 1151 | `	sxu32 c;` |
|     81 | 1152 | `	for( c = 0 ; c < nCol ; c++ ){` |
|     81 | 1153 | `		sxi32 rc = HashmapFlagValueCmp(aCol[c].apNode[iA],aCol[c].apNode[iB],aCol[c].iFlags);` |
|     81 | 1154 | `		if( rc != 0 ){` |
|     71 | 1155 | `			return aCol[c].iDir < 0 ? -rc : rc;` |
|      - | 1156 | `		}` |
|      6 | 1157 | `	}` |
|    ! 0 | 1158 | `	return 0;` |
|     37 | 1159 | `}` |
|      - | 1160 | `/*` |
|      - | 1161 | ` * Stable bottom-up merge sort over the row-index permutation. Iterative on` |
|      - | 1162 | ` * purpose — the recursive shape would put O(log n) frames on the native stack` |
|      - | 1163 | ` * (the §7 embedder C-stack family).` |
|      - | 1164 | ` */` |
|     24 | 1165 | `static void MultisortSortIdx(MultisortCol *aCol,sxu32 nCol,sxu32 *aIdx,sxu32 *aTmp,sxu32 n)` |
|      3 | 1166 | `{` |
|      - | 1167 | `	sxu32 nWidth,iLo;` |
|     71 | 1168 | `	for( nWidth = 1 ; nWidth < n ; nWidth *= 2 ){` |
|    111 | 1169 | `		for( iLo = 0 ; iLo < n ; iLo += 2 * nWidth ){` |
|     67 | 1170 | `			sxu32 iMid = iLo + nWidth;` |
|     67 | 1171 | `			sxu32 iHi = iLo + 2 * nWidth;` |
|      - | 1172 | `			sxu32 i,j,k;` |
|     67 | 1173 | `			if( iMid > n ){ iMid = n; }` |
|     67 | 1174 | `			if( iHi > n ){ iHi = n; }` |
|     67 | 1175 | `			i = iLo; j = iMid; k = iLo;` |
|    135 | 1176 | `			while( i < iMid && j < iHi ){` |
|      - | 1177 | `				/* <= keeps the run stable: on a full tie the left row wins */` |
|     71 | 1178 | `				if( MultisortRowCmp(aCol,nCol,aIdx[i],aIdx[j]) <= 0 ){` |
|     33 | 1179 | `					aTmp[k++] = aIdx[i++];` |
|     18 | 1180 | `				}else{` |
|     41 | 1181 | `					aTmp[k++] = aIdx[j++];` |
|      - | 1182 | `				}` |
|      3 | 1183 | `			}` |
|    121 | 1184 | `			while( i < iMid ){ aTmp[k++] = aIdx[i++]; }` |
|     81 | 1185 | `			while( j < iHi ){ aTmp[k++] = aIdx[j++]; }` |
|     35 | 1186 | `		}` |
|      - | 1187 | `		/* aTmp holds the merged runs for this width; swap roles by copying` |
|      - | 1188 | `		 * back — n is bounded by the array count, one memcpy per doubling. */` |
|     47 | 1189 | `		SyMemcpy(aTmp,aIdx,(sxu32)(n * sizeof(sxu32)));` |
|     25 | 1190 | `	}` |
|     27 | 1191 | `}` |
|     50 | 1192 | `PH7_PRIVATE int ph7_hashmap_multisort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 1193 | `{` |
|      - | 1194 | `	MultisortCol *aCol;` |
|      - | 1195 | `	sxu32 *aIdx,*aTmp;` |
|     53 | 1196 | `	sxu32 nCol = 0,nRow,i;` |
|      - | 1197 | `	sxi32 rcStatus;` |
|      - | 1198 | `	int iArg;` |
|      - | 1199 |  |
|     53 | 1200 | `	if( nArg < 1 ){` |
|    ! 0 | 1201 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1202 | `			"ArgumentCountError",` |
|      - | 1203 | `			"array_multisort() expects at least 1 argument, %d given",` |
|    ! 0 | 1204 | `			nArg` |
|      - | 1205 | `			);` |
|      - | 1206 | `	}` |
|     78 | 1207 | `	aCol = (MultisortCol *)ph7_context_alloc_chunk(pCtx,` |
|     50 | 1208 | `		(unsigned int)(sizeof(MultisortCol) * (sxu32)nArg),TRUE,TRUE);` |
|     53 | 1209 | `	if( aCol == 0 ){` |
|    ! 0 | 1210 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1211 | `	}` |
|      - | 1212 | `	/* Read the argument list's little language, php's own state machine. */` |
|    139 | 1213 | `	for( iArg = 0 ; iArg < nArg ; iArg++ ){` |
|    111 | 1214 | `		ph7_value *pArg = apArg[iArg];` |
|      - | 1215 | `		/* Only Argument #1 carries its parameter name in php's messages. */` |
|    111 | 1216 | `		const char *zName = (iArg == 0) ? " ($array)" : "";` |
|    111 | 1217 | `		if( ph7_value_is_array(pArg) ){` |
|     67 | 1218 | `			MultisortCol *pCol = &aCol[nCol++];` |
|     67 | 1219 | `			pCol->pArr = pArg;` |
|     67 | 1220 | `			pCol->pMap = (ph7_hashmap *)pArg->x.pOther;` |
|     67 | 1221 | `			pCol->apNode = 0;` |
|     67 | 1222 | `			pCol->iFlags = 0; /* SORT_REGULAR */` |
|     67 | 1223 | `			pCol->iDir = 1;   /* SORT_ASC */` |
|     67 | 1224 | `			pCol->bOrderSeen = pCol->bFlagsSeen = 0;` |
|     67 | 1225 | `			continue;` |
|      - | 1226 | `		}` |
|     47 | 1227 | `		if( ph7_value_is_float(pArg) \|\| (pArg->iFlags & MEMOBJ_INT) == 0 ){` |
|      - | 1228 | `			/* A REAL int only, float asked FIRST (ph7_type_name's rule: an` |
|      - | 1229 | `			 * integer-valued real caches an int and would pass a bare flag` |
|      - | 1230 | `			 * test). php coerces nothing here: "4", 4.0 and true are all` |
|      - | 1231 | `			 * refused. */` |
|     17 | 1232 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1233 | `				"array_multisort(): Argument #%d%s must be an array or a sort flag",` |
|      5 | 1234 | `				iArg + 1,zName);` |
|      - | 1235 | `		}` |
|      - | 1236 | `		{` |
|     37 | 1237 | `			sxi64 iVal = ph7_value_to_int64(pArg);` |
|      - | 1238 | `			/* php masks SORT_FLAG_CASE off for the ORDER match too, but reads` |
|      - | 1239 | `			 * the direction from the UNMASKED value — so SORT_DESC\|SORT_FLAG_CASE` |
|      - | 1240 | `			 * (11) consumes the order slot and quirkily sorts ASCENDING. */` |
|     37 | 1241 | `			if( (iVal & ~(sxi64)8) == 3 /* SORT_DESC */ \|\| (iVal & ~(sxi64)8) == 4 /* SORT_ASC */ ){` |
|     23 | 1242 | `				if( nCol < 1 \|\| aCol[nCol - 1].bOrderSeen ){` |
|     11 | 1243 | `					return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1244 | `						"array_multisort(): Argument #%d%s must be an array or a sort flag that has not already been specified",` |
|      3 | 1245 | `						iArg + 1,zName);` |
|      - | 1246 | `				}` |
|     17 | 1247 | `				aCol[nCol - 1].iDir = (iVal == 3) ? -1 : 1;` |
|     17 | 1248 | `				aCol[nCol - 1].bOrderSeen = 1;` |
|     24 | 1249 | `			}else if( (iVal & ~(sxi64)8 /* SORT_FLAG_CASE */) == 0 /* SORT_REGULAR */` |
|     13 | 1250 | `			       \|\| (iVal & ~(sxi64)8) == 1 /* SORT_NUMERIC */` |
|     11 | 1251 | `			       \|\| (iVal & ~(sxi64)8) == 2 /* SORT_STRING */` |
|      8 | 1252 | `			       \|\| (iVal & ~(sxi64)8) == 5 /* SORT_LOCALE_STRING */` |
|      7 | 1253 | `			       \|\| (iVal & ~(sxi64)8) == 6 /* SORT_NATURAL */ ){` |
|     14 | 1254 | `				if( nCol < 1 \|\| aCol[nCol - 1].bFlagsSeen ){` |
|      7 | 1255 | `					return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1256 | `						"array_multisort(): Argument #%d%s must be an array or a sort flag that has not already been specified",` |
|      2 | 1257 | `						iArg + 1,zName);` |
|      - | 1258 | `				}` |
|     10 | 1259 | `				aCol[nCol - 1].iFlags = (sxi32)iVal;` |
|     10 | 1260 | `				aCol[nCol - 1].bFlagsSeen = 1;` |
|      6 | 1261 | `			}else{` |
|      4 | 1262 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1263 | `					"array_multisort(): Argument #%d%s must be a valid sort flag",` |
|      1 | 1264 | `					iArg + 1,zName);` |
|      - | 1265 | `			}` |
|      - | 1266 | `		}` |
|     14 | 1267 | `	}` |
|      - | 1268 | `	/* Every column must hold the same number of rows; php's message carries no` |
|      - | 1269 | `	 * function prefix. */` |
|     31 | 1270 | `	nRow = aCol[0].pMap->nEntry;` |
|     45 | 1271 | `	for( i = 1 ; i < nCol ; i++ ){` |
|     18 | 1272 | `		if( aCol[i].pMap->nEntry != nRow ){` |
|      3 | 1273 | `			return PH7_VmThrowException(pCtx,"ValueError","Array sizes are inconsistent");` |
|      - | 1274 | `		}` |
|      8 | 1275 | `	}` |
|     29 | 1276 | `	if( nRow > 0 ){` |
|      - | 1277 | `		/* Collect each column's nodes in original order. */` |
|     63 | 1278 | `		for( i = 0 ; i < nCol ; i++ ){` |
|     39 | 1279 | `			ph7_hashmap_node *pNode = aCol[i].pMap->pFirst;` |
|      - | 1280 | `			sxu32 r;` |
|     57 | 1281 | `			aCol[i].apNode = (ph7_hashmap_node **)ph7_context_alloc_chunk(pCtx,` |
|     18 | 1282 | `				(unsigned int)(sizeof(ph7_hashmap_node *) * nRow),FALSE,TRUE);` |
|     39 | 1283 | `			if( aCol[i].apNode == 0 ){` |
|    ! 0 | 1284 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 1285 | `			}` |
|    147 | 1286 | `			for( r = 0 ; r < nRow && pNode ; r++ ){` |
|    111 | 1287 | `				aCol[i].apNode[r] = pNode;` |
|    111 | 1288 | `				pNode = pNode->pPrev; /* Reverse link */` |
|     57 | 1289 | `			}` |
|     21 | 1290 | `		}` |
|     39 | 1291 | `		aIdx = (sxu32 *)ph7_context_alloc_chunk(pCtx,` |
|     12 | 1292 | `			(unsigned int)(sizeof(sxu32) * nRow * 2),FALSE,TRUE);` |
|     27 | 1293 | `		if( aIdx == 0 ){` |
|    ! 0 | 1294 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 1295 | `		}` |
|     27 | 1296 | `		aTmp = &aIdx[nRow];` |
|     99 | 1297 | `		for( i = 0 ; i < nRow ; i++ ){` |
|     75 | 1298 | `			aIdx[i] = i;` |
|     39 | 1299 | `		}` |
|      - | 1300 | `		/* The string flags coerce user-visibly and can only FLAG a throw` |
|      - | 1301 | `		 * (iCmpCallbackExc); clear it, sort, and report after — the flag-sort` |
|      - | 1302 | `		 * drivers' shared pattern. */` |
|     27 | 1303 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|     27 | 1304 | `		MultisortSortIdx(aCol,nCol,aIdx,aTmp,nRow);` |
|      - | 1305 | `		/* Apply the permutation to every column: rebuild in sorted order,` |
|      - | 1306 | `		 * keeping string keys and renumbering int keys, then hand the fresh` |
|      - | 1307 | `		 * array back through the by-ref slot (a literal has none and the` |
|      - | 1308 | `		 * result is silently dropped — php's prefer-ref). */` |
|     63 | 1309 | `		for( i = 0 ; i < nCol ; i++ ){` |
|     39 | 1310 | `			ph7_value *pNew = ph7_context_new_array(pCtx);` |
|      - | 1311 | `			sxu32 r;` |
|     39 | 1312 | `			if( pNew == 0 ){` |
|    ! 0 | 1313 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 1314 | `			}` |
|    147 | 1315 | `			for( r = 0 ; r < nRow ; r++ ){` |
|    111 | 1316 | `				ph7_hashmap_node *pNode = aCol[i].apNode[aIdx[r]];` |
|    165 | 1317 | `				HashmapInsertNode((ph7_hashmap *)pNew->x.pOther,pNode,` |
|    108 | 1318 | `					pNode->iType == HASHMAP_BLOB_NODE ? TRUE : FALSE);` |
|     57 | 1319 | `			}` |
|     39 | 1320 | `			PH7_VmStoreArgByRef(pCtx->pVm,aCol[i].pArr,pNew);` |
|     21 | 1321 | `		}` |
|     27 | 1322 | `		rcStatus = HashmapFlagSortStatus(pCtx);` |
|     27 | 1323 | `		if( rcStatus != PH7_OK ){` |
|    ! 0 | 1324 | `			return rcStatus;` |
|      - | 1325 | `		}` |
|     12 | 1326 | `	}` |
|     29 | 1327 | `	ph7_result_bool(pCtx,1);` |
|     29 | 1328 | `	return PH7_OK;` |
|     28 | 1329 | `}` |
|      - | 1330 |  |
