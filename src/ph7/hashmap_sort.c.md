# src/ph7/hashmap_sort.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 463/508 lines (91.14%)

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
|  52176 |   37 | `static ph7_hashmap_node * HashmapNodeMerge(ph7_hashmap_node *pA,ph7_hashmap_node *pB,ProcNodeCmp xCmp,void *pCmpData)` |
|      5 |   38 | `{` |
|      - |   39 | `	ph7_hashmap_node result,*pTail;` |
|      - |   40 | `    /* Prevent compiler warning */` |
|  52181 |   41 | `	result.pNext = result.pPrev = 0;` |
|  52181 |   42 | `	pTail = &result;` |
| 129974 |   43 | `	while( pA && pB ){` |
|  77798 |   44 | `		if( xCmp(pA,pB,pCmpData) <= 0 ){` |
|  61053 |   45 | `			pTail->pPrev = pA;` |
|  61053 |   46 | `			pA->pNext = pTail;` |
|  61053 |   47 | `			pTail = pA;` |
|  61053 |   48 | `			pA = pA->pPrev;` |
|  30672 |   49 | `		}else{` |
|  16750 |   50 | `			pTail->pPrev = pB;` |
|  16750 |   51 | `			pB->pNext = pTail;` |
|  16750 |   52 | `			pTail = pB;` |
|  16750 |   53 | `			pB = pB->pPrev;` |
|      - |   54 | `		}` |
|      5 |   55 | `	}` |
|  52181 |   56 | `	if( pA ){` |
|   4324 |   57 | `		pTail->pPrev = pA;` |
|   4324 |   58 | `		pA->pNext = pTail;` |
|  49887 |   59 | `	}else if( pB ){` |
|  47550 |   60 | `		pTail->pPrev = pB;` |
|  47550 |   61 | `		pB->pNext = pTail;` |
|  23912 |   62 | `	}else{` |
|    317 |   63 | `		pTail->pPrev = pTail->pNext = 0;` |
|      - |   64 | `	}` |
|  52181 |   65 | `	return result.pPrev;` |
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
|   1218 |   79 | `PH7_PRIVATE sxi32 HashmapMergeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData)` |
|      5 |   80 | `{` |
|      - |   81 | `	ph7_hashmap_node *a[N_SORT_BUCKET], *p,*pIn;` |
|      - |   82 | `	sxu32 i;` |
|   1223 |   83 | `	SyZero(a,sizeof(a));` |
|      - |   84 | `	/* Point to the first inserted entry */` |
|   1223 |   85 | `	pIn = pMap->pFirst;` |
|  17993 |   86 | `	while( pIn ){` |
|  16775 |   87 | `		p = pIn;` |
|  16775 |   88 | `		pIn = p->pPrev;` |
|  16775 |   89 | `		p->pPrev = 0;` |
|  31193 |   90 | `		for(i=0; i<N_SORT_BUCKET-1; i++){` |
|  31193 |   91 | `			if( a[i]==0 ){` |
|  16775 |   92 | `				a[i] = p;` |
|  16775 |   93 | `				break;` |
|    ! 0 |   94 | `			}else{` |
|  14423 |   95 | `				p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|  14423 |   96 | `				a[i] = 0;` |
|      - |   97 | `			}` |
|   7214 |   98 | `		}` |
|  16775 |   99 | `		if( i==N_SORT_BUCKET-1 ){` |
|      - |  100 | `			/* To get here, there need to be 2^(N_SORT_BUCKET) elements in he input list.` |
|      - |  101 | `			 * But that is impossible.` |
|      - |  102 | `			 */` |
|    ! 0 |  103 | `			a[i] = HashmapNodeMerge(a[i], p,xCmp,pCmpData);` |
|    ! 0 |  104 | `		}` |
|      5 |  105 | `	}` |
|   1223 |  106 | `	p = a[0];` |
|  38981 |  107 | `	for(i=1; i<N_SORT_BUCKET; i++){` |
|      - |  108 | `		/* Higher-index buckets hold EARLIER-inserted (and larger) runs, so the` |
|      - |  109 | `		 * bucket must be the LEFT operand: HashmapNodeMerge favors its left arg on` |
|      - |  110 | `		 * a tie (cmp <= 0), and php's sorts are stable (PHP 8.0+) — equal elements` |
|      - |  111 | `		 * keep their original order. Passing p (the later elements) on the left` |
|      - |  112 | `		 * reversed equal runs (e.g. usort of five tie-keyed items moved the last to` |
|      - |  113 | `		 * the front). */` |
|  37763 |  114 | `		p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|  18884 |  115 | `	}` |
|   1223 |  116 | `	p->pNext = 0;` |
|      - |  117 | `	/* Reflect the change */` |
|   1223 |  118 | `	pMap->pFirst = p;` |
|      - |  119 | `	/* Reset the loop cursor */` |
|   1223 |  120 | `	pMap->pCur = pMap->pFirst;` |
|   1223 |  121 | `	return SXRET_OK;` |
|      5 |  122 | `}` |
|      - |  123 | `/* SPDX-SnippetEnd */` |
|      - |  124 | `/*` |
|      - |  125 | ` * Node comparison callback.` |
|      - |  126 | ` * used-by: [sort(),asort(),...]` |
|      - |  127 | ` */` |
|      - |  128 | `/*` |
|      - |  129 | ` * Compare two scalar values under an EXPLICIT php sort base type (never 0 —` |
|      - |  130 | ` * SORT_REGULAR is handled by the callers, which differ for keys vs values):` |
|      - |  131 | ` *   SORT_NUMERIC 1 · SORT_STRING 2 · SORT_LOCALE_STRING 5 · SORT_NATURAL 6,` |
|      - |  132 | ` * with bFold applying SORT_FLAG_CASE. PHL has no locale tables, so` |
|      - |  133 | ` * SORT_LOCALE_STRING behaves like SORT_STRING. Mutates both operands (numeric or` |
|      - |  134 | ` * string cast); the caller owns and releases them.` |
|      - |  135 | ` */` |
|    250 |  136 | `static sxi32 HashmapScalarFlagCmp(ph7_value *pA,ph7_value *pB,int base,int bFold)` |
|      1 |  137 | `{` |
|      - |  138 | `	sxi32 rc;` |
|    251 |  139 | `	if( base == 1 ){` |
|      - |  140 | `		/* SORT_NUMERIC */` |
|     59 |  141 | `		PH7_MemObjToNumeric(pA);` |
|     59 |  142 | `		PH7_MemObjToNumeric(pB);` |
|     59 |  143 | `		rc = PH7_MemObjCmp(pA,pB,FALSE,0);` |
|     30 |  144 | `	}else{` |
|      - |  145 | `		/* SORT_STRING (2) / SORT_LOCALE_STRING (5) / SORT_NATURAL (6) */` |
|      - |  146 | `		const char *zA,*zB;` |
|      - |  147 | `		sxu32 nA,nB,nMin,i;` |
|    193 |  148 | `		if( (pA->iFlags & MEMOBJ_STRING) == 0 ){ PH7_MemObjToString(pA); }` |
|    193 |  149 | `		if( (pB->iFlags & MEMOBJ_STRING) == 0 ){ PH7_MemObjToString(pB); }` |
|    193 |  150 | `		zA = (const char *)SyBlobData(&pA->sBlob);` |
|    193 |  151 | `		zB = (const char *)SyBlobData(&pB->sBlob);` |
|    193 |  152 | `		nA = SyBlobLength(&pA->sBlob);` |
|    193 |  153 | `		nB = SyBlobLength(&pB->sBlob);` |
|    193 |  154 | `		if( base == 6 ){` |
|     29 |  155 | `			rc = PH7_StrNatCmp(zA,(int)nA,zB,(int)nB,bFold);` |
|     15 |  156 | `		}else{` |
|      - |  157 | `			/* Lexicographic comparison (binary-safe), case-folded on request. */` |
|    165 |  158 | `			nMin = nA < nB ? nA : nB;` |
|    165 |  159 | `			rc = 0;` |
|    241 |  160 | `			for( i = 0 ; i < nMin ; ++i ){` |
|    187 |  161 | `				int ca = (unsigned char)zA[i];` |
|    187 |  162 | `				int cb = (unsigned char)zB[i];` |
|    187 |  163 | `				if( bFold ){ ca = SyToLower(ca); cb = SyToLower(cb); }` |
|    187 |  164 | `				if( ca != cb ){ rc = ca < cb ? -1 : 1; break; }` |
|     39 |  165 | `			}` |
|    165 |  166 | `			if( rc == 0 ){` |
|     55 |  167 | `				if( nA < nB ) rc = -1;` |
|     43 |  168 | `				else if( nA > nB ) rc = 1;` |
|     27 |  169 | `			}` |
|      - |  170 | `		}` |
|      - |  171 | `	}` |
|    251 |  172 | `	return rc;` |
|      1 |  173 | `}` |
|      - |  174 | `/*` |
|      - |  175 | ` * Are two live values equal under an array_unique() sort_flags? A non-mutating` |
|      - |  176 | ` * wrapper (works on private copies): base 0 = SORT_REGULAR loose comparison,` |
|      - |  177 | ` * explicit flags route through HashmapScalarFlagCmp. Used by array_unique.` |
|      - |  178 | ` */` |
|    116 |  179 | `PH7_PRIVATE int HashmapValueFlagEqual(ph7_vm *pVm,ph7_value *pA,ph7_value *pB,int base,int bFold)` |
|      1 |  180 | `{` |
|      - |  181 | `	ph7_value sA,sB;` |
|      - |  182 | `	sxi32 rc;` |
|    117 |  183 | `	PH7_MemObjInit(pVm,&sA);` |
|    117 |  184 | `	PH7_MemObjInit(pVm,&sB);` |
|    117 |  185 | `	PH7_MemObjStore(pA,&sA);` |
|    117 |  186 | `	PH7_MemObjStore(pB,&sB);` |
|    117 |  187 | `	if( base == 0 ){` |
|     11 |  188 | `		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0); /* SORT_REGULAR: loose comparison */` |
|      6 |  189 | `	}else{` |
|    107 |  190 | `		rc = HashmapScalarFlagCmp(&sA,&sB,base,bFold);` |
|      - |  191 | `	}` |
|    117 |  192 | `	PH7_MemObjRelease(&sA);` |
|    117 |  193 | `	PH7_MemObjRelease(&sB);` |
|    117 |  194 | `	return rc == 0;` |
|      1 |  195 | `}` |
|      - |  196 | `/*` |
|      - |  197 | ` * Compare two node VALUES under php's sort_flags (base 0 = SORT_REGULAR uses the` |
|      - |  198 | ` * standard value comparison; explicit flags route through HashmapScalarFlagCmp).` |
|      - |  199 | ` */` |
|    126 |  200 | `static sxi32 HashmapFlagValueCmp(ph7_hashmap_node *pA,ph7_hashmap_node *pB,sxi32 iFlags)` |
|      1 |  201 | `{` |
|      - |  202 | `	ph7_value sA,sB;` |
|    127 |  203 | `	int base = iFlags & ~8;      /* strip SORT_FLAG_CASE */` |
|    127 |  204 | `	int bFold = (iFlags & 8) != 0;` |
|      - |  205 | `	sxi32 rc;` |
|    127 |  206 | `	if( base == 0 ){` |
|      - |  207 | `		/* SORT_REGULAR */` |
|    ! 0 |  208 | `		return HashmapNodeCmp(pA,pB,FALSE);` |
|      - |  209 | `	}` |
|    127 |  210 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|    127 |  211 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|    127 |  212 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|    127 |  213 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|    127 |  214 | `	rc = HashmapScalarFlagCmp(&sA,&sB,base,bFold);` |
|    127 |  215 | `	PH7_MemObjRelease(&sA);` |
|    127 |  216 | `	PH7_MemObjRelease(&sB);` |
|    127 |  217 | `	return rc;` |
|     64 |  218 | `}` |
|  77417 |  219 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|      5 |  220 | `{` |
|  77422 |  221 | `	if( pCmpData == 0 ){` |
|      - |  222 | `		/* SORT_REGULAR fast path */` |
|  77334 |  223 | `		return HashmapNodeCmp(pA,pB,FALSE);` |
|      - |  224 | `	}` |
|     89 |  225 | `	return HashmapFlagValueCmp(pA,pB,SX_PTR_TO_INT(pCmpData));` |
|  38602 |  226 | `}` |
|      - |  227 | `/*` |
|      - |  228 | ` * Shared key comparison for ksort()/krsort(): php 8 semantics. Two string` |
|      - |  229 | ` * keys compare bytewise. Mixed int/string keys: a NUMERIC string compares` |
|      - |  230 | ` * numerically with the int key; a non-numeric one makes the int key compare` |
|      - |  231 | ` * AS A STRING ("5" < "b", so int keys land before alphabetic ones — pre-fix` |
|      - |  232 | ` * PHL cast "b" to 0 and sorted string keys first).` |
|      - |  233 | ` */` |
|      - |  234 | `/* True lexicographic compare (memcmp on the common prefix, length breaks` |
|      - |  235 | ` * ties) — SyBlobCmp compares LENGTH first, which is fine for equality but` |
|      - |  236 | ` * wrong for ordering ("c" would sort before "a.y"). */` |
|     44 |  237 | `static sxi32 HashmapLexCmp(const char *zA,sxu32 nA,const char *zB,sxu32 nB)` |
|      2 |  238 | `{` |
|     46 |  239 | `	sxu32 nMin = nA < nB ? nA : nB;` |
|     46 |  240 | `	sxi32 rc = nMin ? SyMemcmp(zA,zB,nMin) : 0;` |
|     46 |  241 | `	if( rc == 0 ){` |
|    ! 0 |  242 | `		rc = (sxi32)nA - (sxi32)nB;` |
|    ! 0 |  243 | `	}` |
|     46 |  244 | `	return rc;` |
|      2 |  245 | `}` |
|     66 |  246 | `static sxi32 HashmapKeyNodeCmp(ph7_hashmap_node *pA,ph7_hashmap_node *pB)` |
|      2 |  247 | `{` |
|      - |  248 | `	sxi32 rc;` |
|     68 |  249 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|      - |  250 | `		/* Perform a string comparison */` |
|     44 |  251 | `		rc = HashmapLexCmp((const char *)SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey),` |
|     28 |  252 | `			(const char *)SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|     16 |  253 | `	}else{` |
|      - |  254 | `		SyString sStr;` |
|     39 |  255 | `		sxi64 iA = 0,iB = 0;` |
|     39 |  256 | `		int bNum = 1;` |
|     39 |  257 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|     13 |  258 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|     13 |  259 | `			if( sStr.nByte < 1 \|\| SyStrIsNumeric(sStr.zString,sStr.nByte,0,0) != SXRET_OK ){` |
|     13 |  260 | `				bNum = 0;` |
|      7 |  261 | `			}else{` |
|    ! 0 |  262 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|      - |  263 | `			}` |
|      7 |  264 | `		}else{` |
|     27 |  265 | `			iA = pA->xKey.iKey;` |
|      - |  266 | `		}` |
|     39 |  267 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|      5 |  268 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|      5 |  269 | `			if( sStr.nByte < 1 \|\| SyStrIsNumeric(sStr.zString,sStr.nByte,0,0) != SXRET_OK ){` |
|      5 |  270 | `				bNum = 0;` |
|      3 |  271 | `			}else{` |
|    ! 0 |  272 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|      - |  273 | `			}` |
|      3 |  274 | `		}else{` |
|     35 |  275 | `			iB = pB->xKey.iKey;` |
|      - |  276 | `		}` |
|     39 |  277 | `		if( bNum ){` |
|     23 |  278 | `			rc = iA < iB ? -1 : (iA > iB ? 1 : 0);` |
|     12 |  279 | `		}else{` |
|      - |  280 | `			/* Render the int key and compare bytewise like php */` |
|      - |  281 | `			char zNumA[24],zNumB[24];` |
|      - |  282 | `			SyString sA,sB;` |
|     17 |  283 | `			if( pA->iType != HASHMAP_BLOB_NODE ){` |
|      5 |  284 | `				sxu32 n = SyBufferFormat(zNumA,sizeof(zNumA),"%qd",pA->xKey.iKey);` |
|      5 |  285 | `				SyStringInitFromBuf(&sA,zNumA,n);` |
|      3 |  286 | `			}else{` |
|     13 |  287 | `				SyStringInitFromBuf(&sA,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|      - |  288 | `			}` |
|     17 |  289 | `			if( pB->iType != HASHMAP_BLOB_NODE ){` |
|     13 |  290 | `				sxu32 n = SyBufferFormat(zNumB,sizeof(zNumB),"%qd",pB->xKey.iKey);` |
|     13 |  291 | `				SyStringInitFromBuf(&sB,zNumB,n);` |
|      7 |  292 | `			}else{` |
|      5 |  293 | `				SyStringInitFromBuf(&sB,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|      - |  294 | `			}` |
|     17 |  295 | `			rc = HashmapLexCmp(sA.zString,sA.nByte,sB.zString,sB.nByte);` |
|      - |  296 | `		}` |
|      - |  297 | `	}` |
|     68 |  298 | `	return rc;` |
|      2 |  299 | `}` |
|      - |  300 | `/*` |
|      - |  301 | ` * Materialise a node's KEY as a scalar ph7_value (int key -> integer, string key` |
|      - |  302 | ` * -> string) for a flag-aware key comparison.` |
|      - |  303 | ` */` |
|     36 |  304 | `static void HashmapNodeKeyToValue(ph7_hashmap_node *pNode,ph7_value *pOut)` |
|      1 |  305 | `{` |
|     37 |  306 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|     21 |  307 | `		PH7_MemObjInitFromInt(pNode->pMap->pVm,pOut,pNode->xKey.iKey);` |
|     11 |  308 | `	}else{` |
|     17 |  309 | `		PH7_MemObjInitFromString(pNode->pMap->pVm,pOut,0);` |
|     25 |  310 | `		PH7_MemObjStringAppend(pOut,(const char *)SyBlobData(&pNode->xKey.sKey),` |
|      8 |  311 | `			SyBlobLength(&pNode->xKey.sKey));` |
|      - |  312 | `	}` |
|     37 |  313 | `}` |
|      - |  314 | `/*` |
|      - |  315 | ` * Compare two node KEYS under php's sort_flags. base 0 = SORT_REGULAR keeps the` |
|      - |  316 | ` * php-8 mixed int/string key semantics (HashmapKeyNodeCmp); explicit flags route` |
|      - |  317 | ` * the materialised keys through HashmapScalarFlagCmp.` |
|      - |  318 | ` */` |
|     18 |  319 | `static sxi32 HashmapFlagKeyCmp(ph7_hashmap_node *pA,ph7_hashmap_node *pB,sxi32 iFlags)` |
|      1 |  320 | `{` |
|      - |  321 | `	ph7_value sA,sB;` |
|     19 |  322 | `	int base = iFlags & ~8;      /* strip SORT_FLAG_CASE */` |
|     19 |  323 | `	int bFold = (iFlags & 8) != 0;` |
|      - |  324 | `	sxi32 rc;` |
|     19 |  325 | `	if( base == 0 ){` |
|    ! 0 |  326 | `		return HashmapKeyNodeCmp(pA,pB);` |
|      - |  327 | `	}` |
|     19 |  328 | `	HashmapNodeKeyToValue(pA,&sA);` |
|     19 |  329 | `	HashmapNodeKeyToValue(pB,&sB);` |
|     19 |  330 | `	rc = HashmapScalarFlagCmp(&sA,&sB,base,bFold);` |
|     19 |  331 | `	PH7_MemObjRelease(&sA);` |
|     19 |  332 | `	PH7_MemObjRelease(&sB);` |
|     19 |  333 | `	return rc;` |
|     10 |  334 | `}` |
|      - |  335 | `/*` |
|      - |  336 | ` * Node comparison callback: Compare nodes by keys only.` |
|      - |  337 | ` * used-by: [ksort()]` |
|      - |  338 | ` */` |
|     66 |  339 | `static sxi32 HashmapCmpCallback2(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|      2 |  340 | `{` |
|     68 |  341 | `	if( pCmpData == 0 ){` |
|     54 |  342 | `		return HashmapKeyNodeCmp(pA,pB);` |
|      - |  343 | `	}` |
|     15 |  344 | `	return HashmapFlagKeyCmp(pA,pB,SX_PTR_TO_INT(pCmpData));` |
|     35 |  345 | `}` |
|      - |  346 | `/*` |
|      - |  347 | ` * Node comparison callback.` |
|      - |  348 | ` * Used by: [rsort(),arsort()];` |
|      - |  349 | ` */` |
|     96 |  350 | `static sxi32 HashmapCmpCallback3(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|      1 |  351 | `{` |
|     97 |  352 | `	if( pCmpData == 0 ){` |
|      - |  353 | `		/* SORT_REGULAR fast path, reversed */` |
|     59 |  354 | `		return -HashmapNodeCmp(pA,pB,FALSE);` |
|      - |  355 | `	}` |
|     39 |  356 | `	return -HashmapFlagValueCmp(pA,pB,SX_PTR_TO_INT(pCmpData));` |
|     49 |  357 | `}` |
|      - |  358 | `/*` |
|      - |  359 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|      - |  360 | ` * used-by: [usort(),uasort()]` |
|      - |  361 | ` */` |
|    170 |  362 | `static sxi32 HashmapCmpCallback4(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|      3 |  363 | `{` |
|      - |  364 | `	ph7_value sResult,*pCallback;` |
|      - |  365 | `	ph7_value *pV1,*pV2;` |
|      - |  366 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|      - |  367 | `	sxi32 rc;` |
|      - |  368 | `	/* Point to the desired callback */` |
|    173 |  369 | `	pCallback = (ph7_value *)pCmpData;` |
|    173 |  370 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|      - |  371 | `		/* A previous comparison already raised: stop invoking the callback so` |
|      - |  372 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|     14 |  373 | `		return 0;` |
|      - |  374 | `	}` |
|      - |  375 | `	/* initialize the result value */` |
|    161 |  376 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|      - |  377 | `	/* Extract nodes values */` |
|    161 |  378 | `	pV1 = HashmapExtractNodeValue(pA);` |
|    161 |  379 | `	pV2 = HashmapExtractNodeValue(pB);` |
|    161 |  380 | `	apArg[0] = pV1;` |
|    161 |  381 | `	apArg[1] = pV2;` |
|      - |  382 | `	/* Invoke the callback */` |
|    161 |  383 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|    161 |  384 | `	if( rc == PH7_EXCEPTION ){` |
|      - |  385 | `		/* The comparator raised: flag it so the sort driver aborts and` |
|      - |  386 | `		 * propagates, and order this pair arbitrarily for the rest of the run. */` |
|     10 |  387 | `		pA->pMap->pVm->iCmpCallbackExc = 1;` |
|     10 |  388 | `		rc = 0;` |
|    156 |  389 | `	}else if( rc != SXRET_OK ){` |
|      - |  390 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|    ! 0 |  391 | `		rc = -1; /* Set a dummy result */` |
|    ! 0 |  392 | `	}else{` |
|      - |  393 | `		/* Extract callback result */` |
|    152 |  394 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|      - |  395 | `			/* Perform an int cast */` |
|    ! 0 |  396 | `			PH7_MemObjToInteger(&sResult);` |
|    ! 0 |  397 | `		}` |
|    152 |  398 | `		rc = (sxi32)sResult.x.iVal;` |
|      - |  399 | `	}` |
|    161 |  400 | `	PH7_MemObjRelease(&sResult);` |
|      - |  401 | `	/* Callback result */` |
|    161 |  402 | `	return rc;` |
|     88 |  403 | `}` |
|      - |  404 | `/*` |
|      - |  405 | ` * Node comparison callback: Compare nodes by keys only.` |
|      - |  406 | ` * used-by: [krsort()]` |
|      - |  407 | ` */` |
|     18 |  408 | `static sxi32 HashmapCmpCallback5(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|      1 |  409 | `{` |
|     19 |  410 | `	if( pCmpData == 0 ){` |
|     15 |  411 | `		return -HashmapKeyNodeCmp(pA,pB); /* Reverse result */` |
|      - |  412 | `	}` |
|      5 |  413 | `	return -HashmapFlagKeyCmp(pA,pB,SX_PTR_TO_INT(pCmpData));` |
|     10 |  414 | `}` |
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
|      8 |  475 | `	SXUNUSED(pB); /* cc warning */` |
|      8 |  476 | `	SXUNUSED(pCmpData);` |
|      - |  477 | `	/* Grab a random number */` |
|     21 |  478 | `	n = PH7_VmRandomNum(pA->pMap->pVm);` |
|      - |  479 | `	/* if the random number is odd then the first node 'pA' is greater then` |
|      - |  480 | `	 * the second node 'pB'. Otherwise the reverse is assumed.` |
|      - |  481 | `	 */` |
|     21 |  482 | `	return n&1 ? 1 : -1;` |
|      1 |  483 | `}` |
|      - |  484 | `/*` |
|      - |  485 | ` * Rehash all nodes keys after a merge-sort have been applied.` |
|      - |  486 | ` * Used by [sort(),usort() and rsort()].` |
|      - |  487 | ` */` |
|   1142 |  488 | `PH7_PRIVATE void HashmapSortRehash(ph7_hashmap *pMap)` |
|      5 |  489 | `{` |
|      - |  490 | `	ph7_hashmap_node *p,*pLast;` |
|      - |  491 | `	sxu32 i;` |
|      - |  492 | `	/* Rehash all entries */` |
|   1147 |  493 | `	pLast = p = pMap->pFirst;` |
|   1147 |  494 | `	pMap->iNextIdx = 0;` |
|   1147 |  495 | `	pMap->bIntKeySeen = 0; /* Reset the automatic index */` |
|   1147 |  496 | `	i = 0;` |
|   8818 |  497 | `	for( ;; ){` |
|  17641 |  498 | `		if( i >= pMap->nEntry ){` |
|   1147 |  499 | `			pMap->pLast = pLast; /* Fix the last link broken by the merge-sort */` |
|   1147 |  500 | `			break;` |
|      - |  501 | `		}` |
|  16499 |  502 | `		if( p->iType == HASHMAP_BLOB_NODE ){` |
|      - |  503 | `			/* Do not maintain index association as requested by the PHP specification */` |
|     11 |  504 | `			SyBlobRelease(&p->xKey.sKey);` |
|      - |  505 | `			/* Change key type */` |
|     11 |  506 | `			p->iType = HASHMAP_INT_NODE;` |
|      5 |  507 | `		}` |
|  16499 |  508 | `		HashmapRehashIntNode(p);` |
|      - |  509 | `		/* Point to the next entry */` |
|  16499 |  510 | `		i++;` |
|  16499 |  511 | `		pLast = p;` |
|  16499 |  512 | `		p = p->pPrev; /* Reverse link */` |
|      5 |  513 | `	}` |
|   1147 |  514 | `}` |
|      - |  515 | `/*` |
|      - |  516 | ` * Array functions implementation.` |
|      - |  517 | ` * Status:` |
|      - |  518 | ` *  Stable.` |
|      - |  519 | ` */` |
|      - |  520 | `/*` |
|      - |  521 | ` * bool sort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|      - |  522 | ` * Sort an array.` |
|      - |  523 | ` * Parameters` |
|      - |  524 | ` *  $array` |
|      - |  525 | ` *   The input array.` |
|      - |  526 | ` * $sort_flags` |
|      - |  527 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|      - |  528 | ` *  Sorting type flags:` |
|      - |  529 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|      - |  530 | ` *   SORT_NUMERIC - compare items numerically` |
|      - |  531 | ` *   SORT_STRING - compare items as strings` |
|      - |  532 | ` * Return` |
|      - |  533 | ` *  TRUE on success or FALSE on failure.` |
|      - |  534 | ` *` |
|      - |  535 | ` */` |
|   1106 |  536 | `PH7_PRIVATE int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  537 | `{` |
|      - |  538 | `	ph7_hashmap *pMap;` |
|      - |  539 | `	/* Make sure we are dealing with a valid hashmap */` |
|   1111 |  540 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - |  541 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  542 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  543 | `		return PH7_OK;` |
|      - |  544 | `	}` |
|      - |  545 | `	/* Point to the internal representation of the input hashmap */` |
|   1111 |  546 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|   1111 |  547 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|   1111 |  548 | `	if( pMap->nEntry > 1 ){` |
|   1105 |  549 | `		sxi32 iCmpFlags = 0;` |
|   1105 |  550 | `		if( nArg > 1 ){` |
|      - |  551 | `			/* Extract comparison flags */` |
|     15 |  552 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|      7 |  553 | `		}` |
|      - |  554 | `		/* Do the merge sort */` |
|   1105 |  555 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|      - |  556 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|   1105 |  557 | `		HashmapSortRehash(pMap);` |
|    557 |  558 | `	}else if( pMap->nEntry == 1 ){` |
|      - |  559 | `		/* php reindexes even a single-element array: a string key becomes 0 */` |
|      5 |  560 | `		HashmapSortRehash(pMap);` |
|      2 |  561 | `	}` |
|      - |  562 | `	/* All done,return TRUE */` |
|   1111 |  563 | `	ph7_result_bool(pCtx,1);` |
|   1111 |  564 | `	return PH7_OK;` |
|    558 |  565 | `}` |
|      - |  566 | `/*` |
|      - |  567 | ` * bool asort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|      - |  568 | ` *  Sort an array and maintain index association.` |
|      - |  569 | ` * Parameters` |
|      - |  570 | ` *  $array` |
|      - |  571 | ` *   The input array.` |
|      - |  572 | ` * $sort_flags` |
|      - |  573 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|      - |  574 | ` *  Sorting type flags:` |
|      - |  575 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|      - |  576 | ` *   SORT_NUMERIC - compare items numerically` |
|      - |  577 | ` *   SORT_STRING - compare items as strings` |
|      - |  578 | ` * Return` |
|      - |  579 | ` *  TRUE on success or FALSE on failure.` |
|      - |  580 | ` */` |
|     36 |  581 | `PH7_PRIVATE int ph7_hashmap_asort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  582 | `{` |
|      - |  583 | `	ph7_hashmap *pMap;` |
|      - |  584 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|     41 |  585 | `	if( nArg < 1 ){` |
|    ! 0 |  586 | `		return PH7_VmThrowException(pCtx,` |
|      - |  587 | `			"ArgumentCountError",` |
|      - |  588 | `			"asort() expects at least 1 argument, 0 given"` |
|      - |  589 | `			);` |
|      - |  590 | `	}` |
|      - |  591 | `	/* PHP 8: TypeError if first argument is not an array */` |
|     41 |  592 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     16 |  593 | `		return PH7_VmThrowException(pCtx,` |
|      - |  594 | `			"TypeError",` |
|      - |  595 | `			"asort(): Argument #1 ($array) must be of type array, %s given",` |
|      4 |  596 | `			ph7_type_name(apArg[0])` |
|      - |  597 | `			);` |
|      - |  598 | `	}` |
|      - |  599 | `	/* Point to the internal representation of the input hashmap */` |
|     29 |  600 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|     29 |  601 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     29 |  602 | `	if( pMap->nEntry > 1 ){` |
|     23 |  603 | `		sxi32 iCmpFlags = 0;` |
|     23 |  604 | `		if( nArg > 1 ){` |
|      - |  605 | `			/* Extract comparison flags */` |
|      7 |  606 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|      3 |  607 | `		}` |
|      - |  608 | `		/* Do the merge sort */` |
|     23 |  609 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|      - |  610 | `		/* Fix the last link broken by the merge */` |
|     55 |  611 | `		while(pMap->pLast->pPrev){` |
|     33 |  612 | `			pMap->pLast = pMap->pLast->pPrev;` |
|      1 |  613 | `		}` |
|     11 |  614 | `	}` |
|      - |  615 | `	/* All done,return TRUE */` |
|     29 |  616 | `	ph7_result_bool(pCtx,1);` |
|     29 |  617 | `	return PH7_OK;` |
|     23 |  618 | `}` |
|      - |  619 | `/*` |
|      - |  620 | ` * bool arsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|      - |  621 | ` *  Sort an array in reverse order and maintain index association.` |
|      - |  622 | ` * Parameters` |
|      - |  623 | ` *  $array` |
|      - |  624 | ` *   The input array.` |
|      - |  625 | ` * $sort_flags` |
|      - |  626 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|      - |  627 | ` *  Sorting type flags:` |
|      - |  628 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|      - |  629 | ` *   SORT_NUMERIC - compare items numerically` |
|      - |  630 | ` *   SORT_STRING - compare items as strings` |
|      - |  631 | ` * Return` |
|      - |  632 | ` *  TRUE on success or FALSE on failure.` |
|      - |  633 | ` */` |
|     32 |  634 | `PH7_PRIVATE int ph7_hashmap_arsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  635 | `{` |
|      - |  636 | `	ph7_hashmap *pMap;` |
|      - |  637 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|     37 |  638 | `	if( nArg < 1 ){` |
|    ! 0 |  639 | `		return PH7_VmThrowException(pCtx,` |
|      - |  640 | `			"ArgumentCountError",` |
|      - |  641 | `			"arsort() expects at least 1 argument, 0 given"` |
|      - |  642 | `			);` |
|      - |  643 | `	}` |
|      - |  644 | `	/* PHP 8: TypeError if first argument is not an array */` |
|     37 |  645 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     16 |  646 | `		return PH7_VmThrowException(pCtx,` |
|      - |  647 | `			"TypeError",` |
|      - |  648 | `			"arsort(): Argument #1 ($array) must be of type array, %s given",` |
|      4 |  649 | `			ph7_type_name(apArg[0])` |
|      - |  650 | `			);` |
|      - |  651 | `	}` |
|      - |  652 | `	/* Point to the internal representation of the input hashmap */` |
|     25 |  653 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|     25 |  654 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     25 |  655 | `	if( pMap->nEntry > 1 ){` |
|     21 |  656 | `		sxi32 iCmpFlags = 0;` |
|     21 |  657 | `		if( nArg > 1 ){` |
|      - |  658 | `			/* Extract comparison flags */` |
|      7 |  659 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|      3 |  660 | `		}` |
|      - |  661 | `		/* Do the merge sort */` |
|     21 |  662 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|      - |  663 | `		/* Fix the last link broken by the merge */` |
|     37 |  664 | `		while(pMap->pLast->pPrev){` |
|     17 |  665 | `			pMap->pLast = pMap->pLast->pPrev;` |
|      1 |  666 | `		}` |
|     10 |  667 | `	}` |
|      - |  668 | `	/* All done,return TRUE */` |
|     25 |  669 | `	ph7_result_bool(pCtx,1);` |
|     25 |  670 | `	return PH7_OK;` |
|     21 |  671 | `}` |
|      - |  672 | `/*` |
|      - |  673 | ` * bool ksort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|      - |  674 | ` *  Sort an array by key.` |
|      - |  675 | ` * Parameters` |
|      - |  676 | ` *  $array` |
|      - |  677 | ` *   The input array.` |
|      - |  678 | ` * $sort_flags` |
|      - |  679 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|      - |  680 | ` *  Sorting type flags:` |
|      - |  681 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|      - |  682 | ` *   SORT_NUMERIC - compare items numerically` |
|      - |  683 | ` *   SORT_STRING - compare items as strings` |
|      - |  684 | ` * Return` |
|      - |  685 | ` *  TRUE on success or FALSE on failure.` |
|      - |  686 | ` */` |
|     20 |  687 | `PH7_PRIVATE int ph7_hashmap_ksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  688 | `{` |
|      - |  689 | `	ph7_hashmap *pMap;` |
|      - |  690 | `	/* Make sure we are dealing with a valid hashmap */` |
|     22 |  691 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - |  692 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  693 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  694 | `		return PH7_OK;` |
|      - |  695 | `	}` |
|      - |  696 | `	/* Point to the internal representation of the input hashmap */` |
|     22 |  697 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|     22 |  698 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     22 |  699 | `	if( pMap->nEntry > 1 ){` |
|     22 |  700 | `		sxi32 iCmpFlags = 0;` |
|     22 |  701 | `		if( nArg > 1 ){` |
|      - |  702 | `			/* Extract comparison flags */` |
|      5 |  703 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|      2 |  704 | `		}` |
|      - |  705 | `		/* Do the merge sort */` |
|     22 |  706 | `		HashmapMergeSort(pMap,HashmapCmpCallback2,SX_INT_TO_PTR(iCmpFlags));` |
|      - |  707 | `		/* Fix the last link broken by the merge */` |
|     56 |  708 | `		while(pMap->pLast->pPrev){` |
|     35 |  709 | `			pMap->pLast = pMap->pLast->pPrev;` |
|      1 |  710 | `		}` |
|     10 |  711 | `	}` |
|      - |  712 | `	/* All done,return TRUE */` |
|     22 |  713 | `	ph7_result_bool(pCtx,1);` |
|     22 |  714 | `	return PH7_OK;` |
|     12 |  715 | `}` |
|      - |  716 | `/*` |
|      - |  717 | ` * bool krsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|      - |  718 | ` *  Sort an array by key in reverse order.` |
|      - |  719 | ` * Parameters` |
|      - |  720 | ` *  $array` |
|      - |  721 | ` *   The input array.` |
|      - |  722 | ` * $sort_flags` |
|      - |  723 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|      - |  724 | ` *  Sorting type flags:` |
|      - |  725 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|      - |  726 | ` *   SORT_NUMERIC - compare items numerically` |
|      - |  727 | ` *   SORT_STRING - compare items as strings` |
|      - |  728 | ` * Return` |
|      - |  729 | ` *  TRUE on success or FALSE on failure.` |
|      - |  730 | ` */` |
|      6 |  731 | `PH7_PRIVATE int ph7_hashmap_krsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  732 | `{` |
|      - |  733 | `	ph7_hashmap *pMap;` |
|      - |  734 | `	/* Make sure we are dealing with a valid hashmap */` |
|      7 |  735 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - |  736 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  737 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  738 | `		return PH7_OK;` |
|      - |  739 | `	}` |
|      - |  740 | `	/* Point to the internal representation of the input hashmap */` |
|      7 |  741 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      7 |  742 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      7 |  743 | `	if( pMap->nEntry > 1 ){` |
|      7 |  744 | `		sxi32 iCmpFlags = 0;` |
|      7 |  745 | `		if( nArg > 1 ){` |
|      - |  746 | `			/* Extract comparison flags */` |
|      3 |  747 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|      1 |  748 | `		}` |
|      - |  749 | `		/* Do the merge sort */` |
|      7 |  750 | `		HashmapMergeSort(pMap,HashmapCmpCallback5,SX_INT_TO_PTR(iCmpFlags));` |
|      - |  751 | `		/* Fix the last link broken by the merge */` |
|     23 |  752 | `		while(pMap->pLast->pPrev){` |
|     17 |  753 | `			pMap->pLast = pMap->pLast->pPrev;` |
|      1 |  754 | `		}` |
|      3 |  755 | `	}` |
|      - |  756 | `	/* All done,return TRUE */` |
|      7 |  757 | `	ph7_result_bool(pCtx,1);` |
|      7 |  758 | `	return PH7_OK;` |
|      4 |  759 | `}` |
|      - |  760 | `/*` |
|      - |  761 | ` * bool rsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|      - |  762 | ` * Sort an array in reverse order.` |
|      - |  763 | ` * Parameters` |
|      - |  764 | ` *  $array` |
|      - |  765 | ` *   The input array.` |
|      - |  766 | ` * $sort_flags` |
|      - |  767 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|      - |  768 | ` *  Sorting type flags:` |
|      - |  769 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|      - |  770 | ` *   SORT_NUMERIC - compare items numerically` |
|      - |  771 | ` *   SORT_STRING - compare items as strings` |
|      - |  772 | ` * Return` |
|      - |  773 | ` *  TRUE on success or FALSE on failure.` |
|      - |  774 | ` */` |
|      8 |  775 | `PH7_PRIVATE int ph7_hashmap_rsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  776 | `{` |
|      - |  777 | `	ph7_hashmap *pMap;` |
|      - |  778 | `	/* Make sure we are dealing with a valid hashmap */` |
|      9 |  779 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - |  780 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  781 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  782 | `		return PH7_OK;` |
|      - |  783 | `	}` |
|      - |  784 | `	/* Point to the internal representation of the input hashmap */` |
|      9 |  785 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      9 |  786 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      9 |  787 | `	if( pMap->nEntry > 1 ){` |
|      7 |  788 | `		sxi32 iCmpFlags = 0;` |
|      7 |  789 | `		if( nArg > 1 ){` |
|      - |  790 | `			/* Extract comparison flags */` |
|      5 |  791 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|      2 |  792 | `		}` |
|      - |  793 | `		/* Do the merge sort */` |
|      7 |  794 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|      - |  795 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|      7 |  796 | `		HashmapSortRehash(pMap);` |
|      6 |  797 | `	}else if( pMap->nEntry == 1 ){` |
|      - |  798 | `		/* php reindexes even a single-element array: a string key becomes 0 */` |
|      3 |  799 | `		HashmapSortRehash(pMap);` |
|      1 |  800 | `	}` |
|      - |  801 | `	/* All done,return TRUE */` |
|      9 |  802 | `	ph7_result_bool(pCtx,1);` |
|      9 |  803 | `	return PH7_OK;` |
|      5 |  804 | `}` |
|      - |  805 | `/*` |
|      - |  806 | ` * bool usort(array &$array,callable $cmp_function)` |
|      - |  807 | ` *  Sort an array by values using a user-defined comparison function.` |
|      - |  808 | ` * Parameters` |
|      - |  809 | ` *  $array` |
|      - |  810 | ` *   The input array.` |
|      - |  811 | ` * $cmp_function` |
|      - |  812 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|      - |  813 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|      - |  814 | ` *  to, or greater than the second.` |
|      - |  815 | ` *    int callback ( mixed $a, mixed $b )` |
|      - |  816 | ` * Return` |
|      - |  817 | ` *  TRUE on success or FALSE on failure.` |
|      - |  818 | ` */` |
|     28 |  819 | `PH7_PRIVATE int ph7_hashmap_usort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  820 | `{` |
|      - |  821 | `	ph7_hashmap *pMap;` |
|      - |  822 | `	/* Make sure we are dealing with a valid hashmap */` |
|     31 |  823 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - |  824 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  825 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  826 | `		return PH7_OK;` |
|      - |  827 | `	}` |
|     31 |  828 | `	if( nArg > 1 ){` |
|      - |  829 | `		/* php rejects an uncallable comparator with a TypeError, BEFORE touching the` |
|      - |  830 | `		 * array. PH7 handed it to the dispatcher, which answers NULL in silence -- so` |
|      - |  831 | `		 * usort() with a bogus comparator reordered the array anyway, by nothing. */` |
|     31 |  832 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",0);` |
|     31 |  833 | `		if( rcCb != PH7_OK ){` |
|      3 |  834 | `			return rcCb;` |
|      - |  835 | `		}` |
|     13 |  836 | `	}` |
|      - |  837 | `	/* Point to the internal representation of the input hashmap */` |
|     29 |  838 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|     29 |  839 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     29 |  840 | `	if( pMap->nEntry > 1 ){` |
|     27 |  841 | `		ph7_value *pCallback = 0;` |
|      - |  842 | `		ProcNodeCmp xCmp;` |
|     27 |  843 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|     27 |  844 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|      - |  845 | `			/* Point to the desired callback */` |
|     27 |  846 | `			pCallback = apArg[1];` |
|     15 |  847 | `		}else{` |
|      - |  848 | `			/* Use the default comparison function */` |
|    ! 0 |  849 | `			xCmp = HashmapCmpCallback1;` |
|      - |  850 | `		}` |
|      - |  851 | `		/* Do the merge sort */` |
|     27 |  852 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|     27 |  853 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|      - |  854 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|     27 |  855 | `		HashmapSortRehash(pMap);` |
|     27 |  856 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|      - |  857 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|     10 |  858 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|     10 |  859 | `			return PH7_EXCEPTION;` |
|      2 |  860 | `		}` |
|     11 |  861 | `	}else if( pMap->nEntry == 1 ){` |
|      - |  862 | `		/* php reindexes even a single-element array: a string key becomes 0 */` |
|      3 |  863 | `		HashmapSortRehash(pMap);` |
|      1 |  864 | `	}` |
|      - |  865 | `	/* All done,return TRUE */` |
|     20 |  866 | `	ph7_result_bool(pCtx,1);` |
|     20 |  867 | `	return PH7_OK;` |
|     17 |  868 | `}` |
|      - |  869 | `/*` |
|      - |  870 | ` * bool uasort(array &$array,callable $cmp_function)` |
|      - |  871 | ` *  Sort an array by values using a user-defined comparison function` |
|      - |  872 | ` *  and maintain index association.` |
|      - |  873 | ` * Parameters` |
|      - |  874 | ` *  $array` |
|      - |  875 | ` *   The input array.` |
|      - |  876 | ` * $cmp_function` |
|      - |  877 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|      - |  878 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|      - |  879 | ` *  to, or greater than the second.` |
|      - |  880 | ` *    int callback ( mixed $a, mixed $b )` |
|      - |  881 | ` * Return` |
|      - |  882 | ` *  TRUE on success or FALSE on failure.` |
|      - |  883 | ` */` |
|     14 |  884 | `PH7_PRIVATE int ph7_hashmap_uasort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  885 | `{` |
|      - |  886 | `	ph7_hashmap *pMap;` |
|      - |  887 | `	/* Make sure we are dealing with a valid hashmap */` |
|     15 |  888 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - |  889 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  890 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  891 | `		return PH7_OK;` |
|      - |  892 | `	}` |
|     15 |  893 | `	if( nArg > 1 ){` |
|      - |  894 | `		/* php rejects an uncallable comparator with a TypeError, BEFORE touching the` |
|      - |  895 | `		 * array. PH7 handed it to the dispatcher, which answers NULL in silence -- so` |
|      - |  896 | `		 * usort() with a bogus comparator reordered the array anyway, by nothing. */` |
|     15 |  897 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",0);` |
|     15 |  898 | `		if( rcCb != PH7_OK ){` |
|      3 |  899 | `			return rcCb;` |
|      - |  900 | `		}` |
|      6 |  901 | `	}` |
|      - |  902 | `	/* Point to the internal representation of the input hashmap */` |
|     13 |  903 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|     13 |  904 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     13 |  905 | `	if( pMap->nEntry > 1 ){` |
|     13 |  906 | `		ph7_value *pCallback = 0;` |
|      - |  907 | `		ProcNodeCmp xCmp;` |
|     13 |  908 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|     13 |  909 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|      - |  910 | `			/* Point to the desired callback */` |
|     13 |  911 | `			pCallback = apArg[1];` |
|      7 |  912 | `		}else{` |
|      - |  913 | `			/* Use the default comparison function */` |
|    ! 0 |  914 | `			xCmp = HashmapCmpCallback1;` |
|      - |  915 | `		}` |
|      - |  916 | `		/* Do the merge sort */` |
|     13 |  917 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|     13 |  918 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|      - |  919 | `		/* Fix the last link broken by the merge */` |
|     31 |  920 | `		while(pMap->pLast->pPrev){` |
|     19 |  921 | `			pMap->pLast = pMap->pLast->pPrev;` |
|      1 |  922 | `		}` |
|     13 |  923 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|      - |  924 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|    ! 0 |  925 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|    ! 0 |  926 | `			return PH7_EXCEPTION;` |
|      - |  927 | `		}` |
|      6 |  928 | `	}` |
|      - |  929 | `	/* All done,return TRUE */` |
|     13 |  930 | `	ph7_result_bool(pCtx,1);` |
|     13 |  931 | `	return PH7_OK;` |
|      8 |  932 | `}` |
|      - |  933 | `/*` |
|      - |  934 | ` * bool uksort(array &$array,callable $cmp_function)` |
|      - |  935 | ` *  Sort an array by keys using a user-defined comparison` |
|      - |  936 | ` *  function and maintain index association.` |
|      - |  937 | ` * Parameters` |
|      - |  938 | ` *  $array` |
|      - |  939 | ` *   The input array.` |
|      - |  940 | ` * $cmp_function` |
|      - |  941 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|      - |  942 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|      - |  943 | ` *  to, or greater than the second.` |
|      - |  944 | ` *    int callback ( mixed $a, mixed $b )` |
|      - |  945 | ` * Return` |
|      - |  946 | ` *  TRUE on success or FALSE on failure.` |
|      - |  947 | ` */` |
|      4 |  948 | `PH7_PRIVATE int ph7_hashmap_uksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  949 | `{` |
|      - |  950 | `	ph7_hashmap *pMap;` |
|      - |  951 | `	/* Make sure we are dealing with a valid hashmap */` |
|      5 |  952 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - |  953 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  954 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  955 | `		return PH7_OK;` |
|      - |  956 | `	}` |
|      5 |  957 | `	if( nArg > 1 ){` |
|      - |  958 | `		/* php rejects an uncallable comparator with a TypeError, BEFORE touching the` |
|      - |  959 | `		 * array. PH7 handed it to the dispatcher, which answers NULL in silence -- so` |
|      - |  960 | `		 * usort() with a bogus comparator reordered the array anyway, by nothing. */` |
|      5 |  961 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",0);` |
|      5 |  962 | `		if( rcCb != PH7_OK ){` |
|      3 |  963 | `			return rcCb;` |
|      - |  964 | `		}` |
|      1 |  965 | `	}` |
|      - |  966 | `	/* Point to the internal representation of the input hashmap */` |
|      3 |  967 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      3 |  968 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      3 |  969 | `	if( pMap->nEntry > 1 ){` |
|      3 |  970 | `		ph7_value *pCallback = 0;` |
|      - |  971 | `		ProcNodeCmp xCmp;` |
|      3 |  972 | `		xCmp = HashmapCmpCallback6; /* User-defined function as the comparison callback */` |
|      3 |  973 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|      - |  974 | `			/* Point to the desired callback */` |
|      3 |  975 | `			pCallback = apArg[1];` |
|      2 |  976 | `		}else{` |
|      - |  977 | `			/* Use the default comparison function */` |
|    ! 0 |  978 | `			xCmp = HashmapCmpCallback2;` |
|      - |  979 | `		}` |
|      - |  980 | `		/* Do the merge sort */` |
|      3 |  981 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|      3 |  982 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|      - |  983 | `		/* Fix the last link broken by the merge */` |
|      3 |  984 | `		while(pMap->pLast->pPrev){` |
|    ! 0 |  985 | `			pMap->pLast = pMap->pLast->pPrev;` |
|    ! 0 |  986 | `		}` |
|      3 |  987 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|      - |  988 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|    ! 0 |  989 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|    ! 0 |  990 | `			return PH7_EXCEPTION;` |
|      - |  991 | `		}` |
|      1 |  992 | `	}` |
|      - |  993 | `	/* All done,return TRUE */` |
|      3 |  994 | `	ph7_result_bool(pCtx,1);` |
|      3 |  995 | `	return PH7_OK;` |
|      3 |  996 | `}` |
|      - |  997 |  |
