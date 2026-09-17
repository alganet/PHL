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
|  52180 |   37 | `static ph7_hashmap_node * HashmapNodeMerge(ph7_hashmap_node *pA,ph7_hashmap_node *pB,ProcNodeCmp xCmp,void *pCmpData)` |
|      5 |   38 | `{` |
|      - |   39 | `	ph7_hashmap_node result,*pTail;` |
|      - |   40 | `    /* Prevent compiler warning */` |
|  52185 |   41 | `	result.pNext = result.pPrev = 0;` |
|  52185 |   42 | `	pTail = &result;` |
| 130270 |   43 | `	while( pA && pB ){` |
|  78090 |   44 | `		if( xCmp(pA,pB,pCmpData) <= 0 ){` |
|  61247 |   45 | `			pTail->pPrev = pA;` |
|  61247 |   46 | `			pA->pNext = pTail;` |
|  61247 |   47 | `			pTail = pA;` |
|  61247 |   48 | `			pA = pA->pPrev;` |
|  30769 |   49 | `		}else{` |
|  16848 |   50 | `			pTail->pPrev = pB;` |
|  16848 |   51 | `			pB->pNext = pTail;` |
|  16848 |   52 | `			pTail = pB;` |
|  16848 |   53 | `			pB = pB->pPrev;` |
|      - |   54 | `		}` |
|      5 |   55 | `	}` |
|  52185 |   56 | `	if( pA ){` |
|   4321 |   57 | `		pTail->pPrev = pA;` |
|   4321 |   58 | `		pA->pNext = pTail;` |
|  49899 |   59 | `	}else if( pB ){` |
|  47541 |   60 | `		pTail->pPrev = pB;` |
|  47541 |   61 | `		pB->pNext = pTail;` |
|  23901 |   62 | `	}else{` |
|    333 |   63 | `		pTail->pPrev = pTail->pNext = 0;` |
|      - |   64 | `	}` |
|  52185 |   65 | `	return result.pPrev;` |
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
|   1216 |   79 | `PH7_PRIVATE sxi32 HashmapMergeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData)` |
|      5 |   80 | `{` |
|      - |   81 | `	ph7_hashmap_node *a[N_SORT_BUCKET], *p,*pIn;` |
|      - |   82 | `	sxu32 i;` |
|   1221 |   83 | `	SyZero(a,sizeof(a));` |
|      - |   84 | `	/* Point to the first inserted entry */` |
|   1221 |   85 | `	pIn = pMap->pFirst;` |
|  18049 |   86 | `	while( pIn ){` |
|  16833 |   87 | `		p = pIn;` |
|  16833 |   88 | `		pIn = p->pPrev;` |
|  16833 |   89 | `		p->pPrev = 0;` |
|  31317 |   90 | `		for(i=0; i<N_SORT_BUCKET-1; i++){` |
|  31317 |   91 | `			if( a[i]==0 ){` |
|  16833 |   92 | `				a[i] = p;` |
|  16833 |   93 | `				break;` |
|    ! 0 |   94 | `			}else{` |
|  14489 |   95 | `				p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|  14489 |   96 | `				a[i] = 0;` |
|      - |   97 | `			}` |
|   7247 |   98 | `		}` |
|  16833 |   99 | `		if( i==N_SORT_BUCKET-1 ){` |
|      - |  100 | `			/* To get here, there need to be 2^(N_SORT_BUCKET) elements in he input list.` |
|      - |  101 | `			 * But that is impossible.` |
|      - |  102 | `			 */` |
|    ! 0 |  103 | `			a[i] = HashmapNodeMerge(a[i], p,xCmp,pCmpData);` |
|    ! 0 |  104 | `		}` |
|      5 |  105 | `	}` |
|   1221 |  106 | `	p = a[0];` |
|  38917 |  107 | `	for(i=1; i<N_SORT_BUCKET; i++){` |
|      - |  108 | `		/* Higher-index buckets hold EARLIER-inserted (and larger) runs, so the` |
|      - |  109 | `		 * bucket must be the LEFT operand: HashmapNodeMerge favors its left arg on` |
|      - |  110 | `		 * a tie (cmp <= 0), and php's sorts are stable (PHP 8.0+) — equal elements` |
|      - |  111 | `		 * keep their original order. Passing p (the later elements) on the left` |
|      - |  112 | `		 * reversed equal runs (e.g. usort of five tie-keyed items moved the last to` |
|      - |  113 | `		 * the front). */` |
|  37701 |  114 | `		p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|  18853 |  115 | `	}` |
|   1221 |  116 | `	p->pNext = 0;` |
|      - |  117 | `	/* Reflect the change */` |
|   1221 |  118 | `	pMap->pFirst = p;` |
|      - |  119 | `	/* Reset the loop cursor */` |
|   1221 |  120 | `	pMap->pCur = pMap->pFirst;` |
|   1221 |  121 | `	return SXRET_OK;` |
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
|  77710 |  219 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|      5 |  220 | `{` |
|  77715 |  221 | `	if( pCmpData == 0 ){` |
|      - |  222 | `		/* SORT_REGULAR fast path */` |
|  77627 |  223 | `		return HashmapNodeCmp(pA,pB,FALSE);` |
|      - |  224 | `	}` |
|     89 |  225 | `	return HashmapFlagValueCmp(pA,pB,SX_PTR_TO_INT(pCmpData));` |
|  38737 |  226 | `}` |
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
|     19 |  472 | `PH7_PRIVATE sxi32 HashmapCmpCallback7(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|      1 |  473 | `{` |
|      - |  474 | `	sxu32 n;` |
|      9 |  475 | `	SXUNUSED(pB); /* cc warning */` |
|      9 |  476 | `	SXUNUSED(pCmpData);` |
|      - |  477 | `	/* Grab a random number from the MT19937 generator so shuffle()/array_rand()` |
|      - |  478 | `	 * respond to srand()/mt_srand() (reproducible under a seed), like php. This` |
|      - |  479 | `	 * is a random-comparator merge sort, not php's Fisher-Yates, so the ordering` |
|      - |  480 | `	 * is deterministic-under-seed but not value-parity with php. */` |
|     20 |  481 | `	n = PH7_VmMtRand(pA->pMap->pVm);` |
|      - |  482 | `	/* if the random number is odd then the first node 'pA' is greater then` |
|      - |  483 | `	 * the second node 'pB'. Otherwise the reverse is assumed.` |
|      - |  484 | `	 */` |
|     20 |  485 | `	return n&1 ? 1 : -1;` |
|      1 |  486 | `}` |
|      - |  487 | `/*` |
|      - |  488 | ` * Rehash all nodes keys after a merge-sort have been applied.` |
|      - |  489 | ` * Used by [sort(),usort() and rsort()].` |
|      - |  490 | ` */` |
|   1140 |  491 | `PH7_PRIVATE void HashmapSortRehash(ph7_hashmap *pMap)` |
|      5 |  492 | `{` |
|      - |  493 | `	ph7_hashmap_node *p,*pLast;` |
|      - |  494 | `	sxu32 i;` |
|      - |  495 | `	/* Rehash all entries */` |
|   1145 |  496 | `	pLast = p = pMap->pFirst;` |
|   1145 |  497 | `	pMap->iNextIdx = 0;` |
|   1145 |  498 | `	pMap->bIntKeySeen = 0; /* Reset the automatic index */` |
|   1145 |  499 | `	i = 0;` |
|   8846 |  500 | `	for( ;; ){` |
|  17697 |  501 | `		if( i >= pMap->nEntry ){` |
|   1145 |  502 | `			pMap->pLast = pLast; /* Fix the last link broken by the merge-sort */` |
|   1145 |  503 | `			break;` |
|      - |  504 | `		}` |
|  16557 |  505 | `		if( p->iType == HASHMAP_BLOB_NODE ){` |
|      - |  506 | `			/* Do not maintain index association as requested by the PHP specification */` |
|     11 |  507 | `			SyBlobRelease(&p->xKey.sKey);` |
|      - |  508 | `			/* Change key type */` |
|     11 |  509 | `			p->iType = HASHMAP_INT_NODE;` |
|      5 |  510 | `		}` |
|  16557 |  511 | `		HashmapRehashIntNode(p);` |
|      - |  512 | `		/* Point to the next entry */` |
|  16557 |  513 | `		i++;` |
|  16557 |  514 | `		pLast = p;` |
|  16557 |  515 | `		p = p->pPrev; /* Reverse link */` |
|      5 |  516 | `	}` |
|   1145 |  517 | `}` |
|      - |  518 | `/*` |
|      - |  519 | ` * Array functions implementation.` |
|      - |  520 | ` * Status:` |
|      - |  521 | ` *  Stable.` |
|      - |  522 | ` */` |
|      - |  523 | `/*` |
|      - |  524 | ` * bool sort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|      - |  525 | ` * Sort an array.` |
|      - |  526 | ` * Parameters` |
|      - |  527 | ` *  $array` |
|      - |  528 | ` *   The input array.` |
|      - |  529 | ` * $sort_flags` |
|      - |  530 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|      - |  531 | ` *  Sorting type flags:` |
|      - |  532 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|      - |  533 | ` *   SORT_NUMERIC - compare items numerically` |
|      - |  534 | ` *   SORT_STRING - compare items as strings` |
|      - |  535 | ` * Return` |
|      - |  536 | ` *  TRUE on success or FALSE on failure.` |
|      - |  537 | ` *` |
|      - |  538 | ` */` |
|   1104 |  539 | `PH7_PRIVATE int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  540 | `{` |
|      - |  541 | `	ph7_hashmap *pMap;` |
|      - |  542 | `	/* Make sure we are dealing with a valid hashmap */` |
|   1109 |  543 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - |  544 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  545 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  546 | `		return PH7_OK;` |
|      - |  547 | `	}` |
|      - |  548 | `	/* Point to the internal representation of the input hashmap */` |
|   1109 |  549 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|   1109 |  550 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|   1109 |  551 | `	if( pMap->nEntry > 1 ){` |
|   1103 |  552 | `		sxi32 iCmpFlags = 0;` |
|   1103 |  553 | `		if( nArg > 1 ){` |
|      - |  554 | `			/* Extract comparison flags */` |
|     15 |  555 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|      7 |  556 | `		}` |
|      - |  557 | `		/* Do the merge sort */` |
|   1103 |  558 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|      - |  559 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|   1103 |  560 | `		HashmapSortRehash(pMap);` |
|    556 |  561 | `	}else if( pMap->nEntry == 1 ){` |
|      - |  562 | `		/* php reindexes even a single-element array: a string key becomes 0 */` |
|      5 |  563 | `		HashmapSortRehash(pMap);` |
|      2 |  564 | `	}` |
|      - |  565 | `	/* All done,return TRUE */` |
|   1109 |  566 | `	ph7_result_bool(pCtx,1);` |
|   1109 |  567 | `	return PH7_OK;` |
|    557 |  568 | `}` |
|      - |  569 | `/*` |
|      - |  570 | ` * bool asort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|      - |  571 | ` *  Sort an array and maintain index association.` |
|      - |  572 | ` * Parameters` |
|      - |  573 | ` *  $array` |
|      - |  574 | ` *   The input array.` |
|      - |  575 | ` * $sort_flags` |
|      - |  576 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|      - |  577 | ` *  Sorting type flags:` |
|      - |  578 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|      - |  579 | ` *   SORT_NUMERIC - compare items numerically` |
|      - |  580 | ` *   SORT_STRING - compare items as strings` |
|      - |  581 | ` * Return` |
|      - |  582 | ` *  TRUE on success or FALSE on failure.` |
|      - |  583 | ` */` |
|     34 |  584 | `PH7_PRIVATE int ph7_hashmap_asort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  585 | `{` |
|      - |  586 | `	ph7_hashmap *pMap;` |
|      - |  587 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|     38 |  588 | `	if( nArg < 1 ){` |
|    ! 0 |  589 | `		return PH7_VmThrowException(pCtx,` |
|      - |  590 | `			"ArgumentCountError",` |
|      - |  591 | `			"asort() expects at least 1 argument, 0 given"` |
|      - |  592 | `			);` |
|      - |  593 | `	}` |
|      - |  594 | `	/* PHP 8: TypeError if first argument is not an array */` |
|     38 |  595 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     12 |  596 | `		return PH7_VmThrowException(pCtx,` |
|      - |  597 | `			"TypeError",` |
|      - |  598 | `			"asort(): Argument #1 ($array) must be of type array, %s given",` |
|      3 |  599 | `			ph7_type_name(apArg[0])` |
|      - |  600 | `			);` |
|      - |  601 | `	}` |
|      - |  602 | `	/* Point to the internal representation of the input hashmap */` |
|     29 |  603 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|     29 |  604 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     29 |  605 | `	if( pMap->nEntry > 1 ){` |
|     23 |  606 | `		sxi32 iCmpFlags = 0;` |
|     23 |  607 | `		if( nArg > 1 ){` |
|      - |  608 | `			/* Extract comparison flags */` |
|      7 |  609 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|      3 |  610 | `		}` |
|      - |  611 | `		/* Do the merge sort */` |
|     23 |  612 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|      - |  613 | `		/* Fix the last link broken by the merge */` |
|     55 |  614 | `		while(pMap->pLast->pPrev){` |
|     33 |  615 | `			pMap->pLast = pMap->pLast->pPrev;` |
|      1 |  616 | `		}` |
|     11 |  617 | `	}` |
|      - |  618 | `	/* All done,return TRUE */` |
|     29 |  619 | `	ph7_result_bool(pCtx,1);` |
|     29 |  620 | `	return PH7_OK;` |
|     21 |  621 | `}` |
|      - |  622 | `/*` |
|      - |  623 | ` * bool arsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|      - |  624 | ` *  Sort an array in reverse order and maintain index association.` |
|      - |  625 | ` * Parameters` |
|      - |  626 | ` *  $array` |
|      - |  627 | ` *   The input array.` |
|      - |  628 | ` * $sort_flags` |
|      - |  629 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|      - |  630 | ` *  Sorting type flags:` |
|      - |  631 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|      - |  632 | ` *   SORT_NUMERIC - compare items numerically` |
|      - |  633 | ` *   SORT_STRING - compare items as strings` |
|      - |  634 | ` * Return` |
|      - |  635 | ` *  TRUE on success or FALSE on failure.` |
|      - |  636 | ` */` |
|     30 |  637 | `PH7_PRIVATE int ph7_hashmap_arsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  638 | `{` |
|      - |  639 | `	ph7_hashmap *pMap;` |
|      - |  640 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|     34 |  641 | `	if( nArg < 1 ){` |
|    ! 0 |  642 | `		return PH7_VmThrowException(pCtx,` |
|      - |  643 | `			"ArgumentCountError",` |
|      - |  644 | `			"arsort() expects at least 1 argument, 0 given"` |
|      - |  645 | `			);` |
|      - |  646 | `	}` |
|      - |  647 | `	/* PHP 8: TypeError if first argument is not an array */` |
|     34 |  648 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     12 |  649 | `		return PH7_VmThrowException(pCtx,` |
|      - |  650 | `			"TypeError",` |
|      - |  651 | `			"arsort(): Argument #1 ($array) must be of type array, %s given",` |
|      3 |  652 | `			ph7_type_name(apArg[0])` |
|      - |  653 | `			);` |
|      - |  654 | `	}` |
|      - |  655 | `	/* Point to the internal representation of the input hashmap */` |
|     25 |  656 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|     25 |  657 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     25 |  658 | `	if( pMap->nEntry > 1 ){` |
|     21 |  659 | `		sxi32 iCmpFlags = 0;` |
|     21 |  660 | `		if( nArg > 1 ){` |
|      - |  661 | `			/* Extract comparison flags */` |
|      7 |  662 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|      3 |  663 | `		}` |
|      - |  664 | `		/* Do the merge sort */` |
|     21 |  665 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|      - |  666 | `		/* Fix the last link broken by the merge */` |
|     37 |  667 | `		while(pMap->pLast->pPrev){` |
|     17 |  668 | `			pMap->pLast = pMap->pLast->pPrev;` |
|      1 |  669 | `		}` |
|     10 |  670 | `	}` |
|      - |  671 | `	/* All done,return TRUE */` |
|     25 |  672 | `	ph7_result_bool(pCtx,1);` |
|     25 |  673 | `	return PH7_OK;` |
|     19 |  674 | `}` |
|      - |  675 | `/*` |
|      - |  676 | ` * bool ksort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|      - |  677 | ` *  Sort an array by key.` |
|      - |  678 | ` * Parameters` |
|      - |  679 | ` *  $array` |
|      - |  680 | ` *   The input array.` |
|      - |  681 | ` * $sort_flags` |
|      - |  682 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|      - |  683 | ` *  Sorting type flags:` |
|      - |  684 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|      - |  685 | ` *   SORT_NUMERIC - compare items numerically` |
|      - |  686 | ` *   SORT_STRING - compare items as strings` |
|      - |  687 | ` * Return` |
|      - |  688 | ` *  TRUE on success or FALSE on failure.` |
|      - |  689 | ` */` |
|     20 |  690 | `PH7_PRIVATE int ph7_hashmap_ksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  691 | `{` |
|      - |  692 | `	ph7_hashmap *pMap;` |
|      - |  693 | `	/* Make sure we are dealing with a valid hashmap */` |
|     22 |  694 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - |  695 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  696 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  697 | `		return PH7_OK;` |
|      - |  698 | `	}` |
|      - |  699 | `	/* Point to the internal representation of the input hashmap */` |
|     22 |  700 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|     22 |  701 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     22 |  702 | `	if( pMap->nEntry > 1 ){` |
|     22 |  703 | `		sxi32 iCmpFlags = 0;` |
|     22 |  704 | `		if( nArg > 1 ){` |
|      - |  705 | `			/* Extract comparison flags */` |
|      5 |  706 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|      2 |  707 | `		}` |
|      - |  708 | `		/* Do the merge sort */` |
|     22 |  709 | `		HashmapMergeSort(pMap,HashmapCmpCallback2,SX_INT_TO_PTR(iCmpFlags));` |
|      - |  710 | `		/* Fix the last link broken by the merge */` |
|     56 |  711 | `		while(pMap->pLast->pPrev){` |
|     35 |  712 | `			pMap->pLast = pMap->pLast->pPrev;` |
|      1 |  713 | `		}` |
|     10 |  714 | `	}` |
|      - |  715 | `	/* All done,return TRUE */` |
|     22 |  716 | `	ph7_result_bool(pCtx,1);` |
|     22 |  717 | `	return PH7_OK;` |
|     12 |  718 | `}` |
|      - |  719 | `/*` |
|      - |  720 | ` * bool krsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|      - |  721 | ` *  Sort an array by key in reverse order.` |
|      - |  722 | ` * Parameters` |
|      - |  723 | ` *  $array` |
|      - |  724 | ` *   The input array.` |
|      - |  725 | ` * $sort_flags` |
|      - |  726 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|      - |  727 | ` *  Sorting type flags:` |
|      - |  728 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|      - |  729 | ` *   SORT_NUMERIC - compare items numerically` |
|      - |  730 | ` *   SORT_STRING - compare items as strings` |
|      - |  731 | ` * Return` |
|      - |  732 | ` *  TRUE on success or FALSE on failure.` |
|      - |  733 | ` */` |
|      6 |  734 | `PH7_PRIVATE int ph7_hashmap_krsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  735 | `{` |
|      - |  736 | `	ph7_hashmap *pMap;` |
|      - |  737 | `	/* Make sure we are dealing with a valid hashmap */` |
|      7 |  738 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - |  739 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  740 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  741 | `		return PH7_OK;` |
|      - |  742 | `	}` |
|      - |  743 | `	/* Point to the internal representation of the input hashmap */` |
|      7 |  744 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      7 |  745 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      7 |  746 | `	if( pMap->nEntry > 1 ){` |
|      7 |  747 | `		sxi32 iCmpFlags = 0;` |
|      7 |  748 | `		if( nArg > 1 ){` |
|      - |  749 | `			/* Extract comparison flags */` |
|      3 |  750 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|      1 |  751 | `		}` |
|      - |  752 | `		/* Do the merge sort */` |
|      7 |  753 | `		HashmapMergeSort(pMap,HashmapCmpCallback5,SX_INT_TO_PTR(iCmpFlags));` |
|      - |  754 | `		/* Fix the last link broken by the merge */` |
|     23 |  755 | `		while(pMap->pLast->pPrev){` |
|     17 |  756 | `			pMap->pLast = pMap->pLast->pPrev;` |
|      1 |  757 | `		}` |
|      3 |  758 | `	}` |
|      - |  759 | `	/* All done,return TRUE */` |
|      7 |  760 | `	ph7_result_bool(pCtx,1);` |
|      7 |  761 | `	return PH7_OK;` |
|      4 |  762 | `}` |
|      - |  763 | `/*` |
|      - |  764 | ` * bool rsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|      - |  765 | ` * Sort an array in reverse order.` |
|      - |  766 | ` * Parameters` |
|      - |  767 | ` *  $array` |
|      - |  768 | ` *   The input array.` |
|      - |  769 | ` * $sort_flags` |
|      - |  770 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|      - |  771 | ` *  Sorting type flags:` |
|      - |  772 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|      - |  773 | ` *   SORT_NUMERIC - compare items numerically` |
|      - |  774 | ` *   SORT_STRING - compare items as strings` |
|      - |  775 | ` * Return` |
|      - |  776 | ` *  TRUE on success or FALSE on failure.` |
|      - |  777 | ` */` |
|      8 |  778 | `PH7_PRIVATE int ph7_hashmap_rsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  779 | `{` |
|      - |  780 | `	ph7_hashmap *pMap;` |
|      - |  781 | `	/* Make sure we are dealing with a valid hashmap */` |
|      9 |  782 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - |  783 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  784 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  785 | `		return PH7_OK;` |
|      - |  786 | `	}` |
|      - |  787 | `	/* Point to the internal representation of the input hashmap */` |
|      9 |  788 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      9 |  789 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      9 |  790 | `	if( pMap->nEntry > 1 ){` |
|      7 |  791 | `		sxi32 iCmpFlags = 0;` |
|      7 |  792 | `		if( nArg > 1 ){` |
|      - |  793 | `			/* Extract comparison flags */` |
|      5 |  794 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|      2 |  795 | `		}` |
|      - |  796 | `		/* Do the merge sort */` |
|      7 |  797 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|      - |  798 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|      7 |  799 | `		HashmapSortRehash(pMap);` |
|      6 |  800 | `	}else if( pMap->nEntry == 1 ){` |
|      - |  801 | `		/* php reindexes even a single-element array: a string key becomes 0 */` |
|      3 |  802 | `		HashmapSortRehash(pMap);` |
|      1 |  803 | `	}` |
|      - |  804 | `	/* All done,return TRUE */` |
|      9 |  805 | `	ph7_result_bool(pCtx,1);` |
|      9 |  806 | `	return PH7_OK;` |
|      5 |  807 | `}` |
|      - |  808 | `/*` |
|      - |  809 | ` * bool usort(array &$array,callable $cmp_function)` |
|      - |  810 | ` *  Sort an array by values using a user-defined comparison function.` |
|      - |  811 | ` * Parameters` |
|      - |  812 | ` *  $array` |
|      - |  813 | ` *   The input array.` |
|      - |  814 | ` * $cmp_function` |
|      - |  815 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|      - |  816 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|      - |  817 | ` *  to, or greater than the second.` |
|      - |  818 | ` *    int callback ( mixed $a, mixed $b )` |
|      - |  819 | ` * Return` |
|      - |  820 | ` *  TRUE on success or FALSE on failure.` |
|      - |  821 | ` */` |
|     28 |  822 | `PH7_PRIVATE int ph7_hashmap_usort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  823 | `{` |
|      - |  824 | `	ph7_hashmap *pMap;` |
|      - |  825 | `	/* Make sure we are dealing with a valid hashmap */` |
|     31 |  826 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - |  827 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  828 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  829 | `		return PH7_OK;` |
|      - |  830 | `	}` |
|     31 |  831 | `	if( nArg > 1 ){` |
|      - |  832 | `		/* php rejects an uncallable comparator with a TypeError, BEFORE touching the` |
|      - |  833 | `		 * array. PH7 handed it to the dispatcher, which answers NULL in silence -- so` |
|      - |  834 | `		 * usort() with a bogus comparator reordered the array anyway, by nothing. */` |
|     31 |  835 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",0);` |
|     31 |  836 | `		if( rcCb != PH7_OK ){` |
|      3 |  837 | `			return rcCb;` |
|      - |  838 | `		}` |
|     13 |  839 | `	}` |
|      - |  840 | `	/* Point to the internal representation of the input hashmap */` |
|     29 |  841 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|     29 |  842 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     29 |  843 | `	if( pMap->nEntry > 1 ){` |
|     27 |  844 | `		ph7_value *pCallback = 0;` |
|      - |  845 | `		ProcNodeCmp xCmp;` |
|     27 |  846 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|     27 |  847 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|      - |  848 | `			/* Point to the desired callback */` |
|     27 |  849 | `			pCallback = apArg[1];` |
|     15 |  850 | `		}else{` |
|      - |  851 | `			/* Use the default comparison function */` |
|    ! 0 |  852 | `			xCmp = HashmapCmpCallback1;` |
|      - |  853 | `		}` |
|      - |  854 | `		/* Do the merge sort */` |
|     27 |  855 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|     27 |  856 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|      - |  857 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|     27 |  858 | `		HashmapSortRehash(pMap);` |
|     27 |  859 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|      - |  860 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|     10 |  861 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|     10 |  862 | `			return PH7_EXCEPTION;` |
|      2 |  863 | `		}` |
|     11 |  864 | `	}else if( pMap->nEntry == 1 ){` |
|      - |  865 | `		/* php reindexes even a single-element array: a string key becomes 0 */` |
|      3 |  866 | `		HashmapSortRehash(pMap);` |
|      1 |  867 | `	}` |
|      - |  868 | `	/* All done,return TRUE */` |
|     20 |  869 | `	ph7_result_bool(pCtx,1);` |
|     20 |  870 | `	return PH7_OK;` |
|     17 |  871 | `}` |
|      - |  872 | `/*` |
|      - |  873 | ` * bool uasort(array &$array,callable $cmp_function)` |
|      - |  874 | ` *  Sort an array by values using a user-defined comparison function` |
|      - |  875 | ` *  and maintain index association.` |
|      - |  876 | ` * Parameters` |
|      - |  877 | ` *  $array` |
|      - |  878 | ` *   The input array.` |
|      - |  879 | ` * $cmp_function` |
|      - |  880 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|      - |  881 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|      - |  882 | ` *  to, or greater than the second.` |
|      - |  883 | ` *    int callback ( mixed $a, mixed $b )` |
|      - |  884 | ` * Return` |
|      - |  885 | ` *  TRUE on success or FALSE on failure.` |
|      - |  886 | ` */` |
|     14 |  887 | `PH7_PRIVATE int ph7_hashmap_uasort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  888 | `{` |
|      - |  889 | `	ph7_hashmap *pMap;` |
|      - |  890 | `	/* Make sure we are dealing with a valid hashmap */` |
|     15 |  891 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - |  892 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  893 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  894 | `		return PH7_OK;` |
|      - |  895 | `	}` |
|     15 |  896 | `	if( nArg > 1 ){` |
|      - |  897 | `		/* php rejects an uncallable comparator with a TypeError, BEFORE touching the` |
|      - |  898 | `		 * array. PH7 handed it to the dispatcher, which answers NULL in silence -- so` |
|      - |  899 | `		 * usort() with a bogus comparator reordered the array anyway, by nothing. */` |
|     15 |  900 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",0);` |
|     15 |  901 | `		if( rcCb != PH7_OK ){` |
|      3 |  902 | `			return rcCb;` |
|      - |  903 | `		}` |
|      6 |  904 | `	}` |
|      - |  905 | `	/* Point to the internal representation of the input hashmap */` |
|     13 |  906 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|     13 |  907 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     13 |  908 | `	if( pMap->nEntry > 1 ){` |
|     13 |  909 | `		ph7_value *pCallback = 0;` |
|      - |  910 | `		ProcNodeCmp xCmp;` |
|     13 |  911 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|     13 |  912 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|      - |  913 | `			/* Point to the desired callback */` |
|     13 |  914 | `			pCallback = apArg[1];` |
|      7 |  915 | `		}else{` |
|      - |  916 | `			/* Use the default comparison function */` |
|    ! 0 |  917 | `			xCmp = HashmapCmpCallback1;` |
|      - |  918 | `		}` |
|      - |  919 | `		/* Do the merge sort */` |
|     13 |  920 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|     13 |  921 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|      - |  922 | `		/* Fix the last link broken by the merge */` |
|     31 |  923 | `		while(pMap->pLast->pPrev){` |
|     19 |  924 | `			pMap->pLast = pMap->pLast->pPrev;` |
|      1 |  925 | `		}` |
|     13 |  926 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|      - |  927 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|    ! 0 |  928 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|    ! 0 |  929 | `			return PH7_EXCEPTION;` |
|      - |  930 | `		}` |
|      6 |  931 | `	}` |
|      - |  932 | `	/* All done,return TRUE */` |
|     13 |  933 | `	ph7_result_bool(pCtx,1);` |
|     13 |  934 | `	return PH7_OK;` |
|      8 |  935 | `}` |
|      - |  936 | `/*` |
|      - |  937 | ` * bool uksort(array &$array,callable $cmp_function)` |
|      - |  938 | ` *  Sort an array by keys using a user-defined comparison` |
|      - |  939 | ` *  function and maintain index association.` |
|      - |  940 | ` * Parameters` |
|      - |  941 | ` *  $array` |
|      - |  942 | ` *   The input array.` |
|      - |  943 | ` * $cmp_function` |
|      - |  944 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|      - |  945 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|      - |  946 | ` *  to, or greater than the second.` |
|      - |  947 | ` *    int callback ( mixed $a, mixed $b )` |
|      - |  948 | ` * Return` |
|      - |  949 | ` *  TRUE on success or FALSE on failure.` |
|      - |  950 | ` */` |
|      4 |  951 | `PH7_PRIVATE int ph7_hashmap_uksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  952 | `{` |
|      - |  953 | `	ph7_hashmap *pMap;` |
|      - |  954 | `	/* Make sure we are dealing with a valid hashmap */` |
|      5 |  955 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - |  956 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  957 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  958 | `		return PH7_OK;` |
|      - |  959 | `	}` |
|      5 |  960 | `	if( nArg > 1 ){` |
|      - |  961 | `		/* php rejects an uncallable comparator with a TypeError, BEFORE touching the` |
|      - |  962 | `		 * array. PH7 handed it to the dispatcher, which answers NULL in silence -- so` |
|      - |  963 | `		 * usort() with a bogus comparator reordered the array anyway, by nothing. */` |
|      5 |  964 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",0);` |
|      5 |  965 | `		if( rcCb != PH7_OK ){` |
|      3 |  966 | `			return rcCb;` |
|      - |  967 | `		}` |
|      1 |  968 | `	}` |
|      - |  969 | `	/* Point to the internal representation of the input hashmap */` |
|      3 |  970 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      3 |  971 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      3 |  972 | `	if( pMap->nEntry > 1 ){` |
|      3 |  973 | `		ph7_value *pCallback = 0;` |
|      - |  974 | `		ProcNodeCmp xCmp;` |
|      3 |  975 | `		xCmp = HashmapCmpCallback6; /* User-defined function as the comparison callback */` |
|      3 |  976 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|      - |  977 | `			/* Point to the desired callback */` |
|      3 |  978 | `			pCallback = apArg[1];` |
|      2 |  979 | `		}else{` |
|      - |  980 | `			/* Use the default comparison function */` |
|    ! 0 |  981 | `			xCmp = HashmapCmpCallback2;` |
|      - |  982 | `		}` |
|      - |  983 | `		/* Do the merge sort */` |
|      3 |  984 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|      3 |  985 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|      - |  986 | `		/* Fix the last link broken by the merge */` |
|      3 |  987 | `		while(pMap->pLast->pPrev){` |
|    ! 0 |  988 | `			pMap->pLast = pMap->pLast->pPrev;` |
|    ! 0 |  989 | `		}` |
|      3 |  990 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|      - |  991 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|    ! 0 |  992 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|    ! 0 |  993 | `			return PH7_EXCEPTION;` |
|      - |  994 | `		}` |
|      1 |  995 | `	}` |
|      - |  996 | `	/* All done,return TRUE */` |
|      3 |  997 | `	ph7_result_bool(pCtx,1);` |
|      3 |  998 | `	return PH7_OK;` |
|      3 |  999 | `}` |
|      - | 1000 |  |
