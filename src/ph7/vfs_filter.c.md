# src/ph7/vfs_filter.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1191/1336 lines (89.15%)

[Root index](../../index.md) | [Directory index](index.md)

|  Hits | Line | Source |
| ----: | ---: | :--- |
|     - |    1 | `/**` |
|     - |    2 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|     - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|     - |    4 | ` */` |
|     - |    5 | `#include "ph7int.h"` |
|     - |    6 | `#include <string.h>` |
|     - |    7 |  |
|     - |    8 | `#ifndef PH7_DISABLE_DISK_IO` |
|     - |    9 | `/*` |
|     - |   10 | ` * Section:` |
|     - |   11 | ` *    Stream filters — php's stream_filter_* family and the chains it runs.` |
|     - |   12 | ` * Status:` |
|     - |   13 | ` *    Stable.` |
|     - |   14 | ` *` |
|     - |   15 | ` * A php stream carries two chains of filters: everything that comes off the` |
|     - |   16 | ` * device is run through the READ chain before the script sees a byte of it, and` |
|     - |   17 | ` * everything the script writes is run through the WRITE chain before the device` |
|     - |   18 | ` * does. Neither is a byte-for-byte mapping — base64 makes four bytes out of` |
|     - |   19 | ` * three and dechunk throws whole runs away — which is why a filtered read` |
|     - |   20 | ` * cannot be served straight into the caller's buffer and why the chain speaks` |
|     - |   21 | ` * in BRIGADES: a filter takes the buckets that arrived and appends what it made` |
|     - |   22 | ` * to a second brigade, and what it ANSWERS says whether that output may go on` |
|     - |   23 | ` * (PASS_ON), whether it needs more input before it can produce any (FEED_ME) or` |
|     - |   24 | ` * whether the stream is finished (ERR_FATAL).` |
|     - |   25 | ` *` |
|     - |   26 | ` * Every chain call carries a FLAG saying which kind of call it is, and` |
|     - |   27 | ` * FLUSH_CLOSE — the one a filter gets when the device hit its end, when the` |
|     - |   28 | ` * handle is closed and when the filter is removed — is the only chance a` |
|     - |   29 | ` * buffering filter has to emit the tail it is holding.` |
|     - |   30 | ` */` |
|     - |   31 | `/* --------------------------------------------------------------------------` |
|     - |   32 | ` * Brigades.` |
|     - |   33 | ` * -------------------------------------------------------------------------- */` |
|  1882 |   34 | `PH7_PRIVATE phl_bucket * PH7_FilterBucketNew(ph7_vm *pVm,const void *pData,sxu32 nLen)` |
|     3 |   35 | `{` |
|     - |   36 | `	phl_bucket *pBucket;` |
|  1885 |   37 | `	if( pVm == 0 ){` |
|   ! 0 |   38 | `		return 0;` |
|     - |   39 | `	}` |
|  1885 |   40 | `	pBucket = (phl_bucket *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_bucket));` |
|  1885 |   41 | `	if( pBucket == 0 ){` |
|   ! 0 |   42 | `		return 0;` |
|     - |   43 | `	}` |
|  1885 |   44 | `	SyZero(pBucket,sizeof(phl_bucket));` |
|  1885 |   45 | `	SyBlobInit(&pBucket->sData,&pVm->sAllocator);` |
|  1885 |   46 | `	if( nLen > 0 && pData != 0 ){` |
|  1885 |   47 | `		if( SyBlobAppend(&pBucket->sData,pData,nLen) != SXRET_OK ){` |
|   ! 0 |   48 | `			SyBlobRelease(&pBucket->sData);` |
|   ! 0 |   49 | `			SyMemBackendFree(&pVm->sAllocator,pBucket);` |
|   ! 0 |   50 | `			return 0;` |
|     - |   51 | `		}` |
|   941 |   52 | `	}` |
|  1885 |   53 | `	return pBucket;` |
|   944 |   54 | `}` |
|  1948 |   55 | `PH7_PRIVATE void PH7_FilterBucketAppend(phl_brigade *pBrig,phl_bucket *pBucket)` |
|     3 |   56 | `{` |
|  1951 |   57 | `	if( pBucket == 0 ){` |
|   ! 0 |   58 | `		return;` |
|     - |   59 | `	}` |
|  1951 |   60 | `	pBucket->pNext = 0;` |
|  1951 |   61 | `	if( pBrig->pTail ){` |
|   ! 0 |   62 | `		pBrig->pTail->pNext = pBucket;` |
|   ! 0 |   63 | `	}else{` |
|  1951 |   64 | `		pBrig->pHead = pBucket;` |
|     - |   65 | `	}` |
|  1951 |   66 | `	pBrig->pTail = pBucket;` |
|   977 |   67 | `}` |
|  1882 |   68 | `PH7_PRIVATE void PH7_FilterBucketFree(ph7_vm *pVm,phl_bucket *pBucket)` |
|     3 |   69 | `{` |
|  1885 |   70 | `	if( pBucket == 0 ){` |
|   ! 0 |   71 | `		return;` |
|     - |   72 | `	}` |
|  1885 |   73 | `	SyBlobRelease(&pBucket->sData);` |
|  1885 |   74 | `	SyMemBackendFree(&pVm->sAllocator,pBucket);` |
|   944 |   75 | `}` |
|     - |   76 | `/* Unlink and answer the first bucket of a brigade, or 0 when it is empty. */` |
|  9094 |   77 | `static phl_bucket * FilterBucketPop(phl_brigade *pBrig)` |
|     3 |   78 | `{` |
|  9097 |   79 | `	phl_bucket *pBucket = pBrig->pHead;` |
|  9097 |   80 | `	if( pBucket == 0 ){` |
|  7147 |   81 | `		return 0;` |
|     - |   82 | `	}` |
|  1953 |   83 | `	pBrig->pHead = pBucket->pNext;` |
|  1953 |   84 | `	if( pBrig->pHead == 0 ){` |
|  1951 |   85 | `		pBrig->pTail = 0;` |
|   974 |   86 | `	}` |
|  1953 |   87 | `	pBucket->pNext = 0;` |
|  1953 |   88 | `	return pBucket;` |
|  4550 |   89 | `}` |
|  4288 |   90 | `PH7_PRIVATE void PH7_FilterBrigadeRelease(ph7_vm *pVm,phl_brigade *pBrig)` |
|     3 |   91 | `{` |
|     - |   92 | `	phl_bucket *pBucket;` |
|  4295 |   93 | `	while( (pBucket = FilterBucketPop(pBrig)) != 0 ){` |
|     5 |   94 | `		PH7_FilterBucketFree(pVm,pBucket);` |
|     1 |   95 | `	}` |
|  4291 |   96 | `}` |
|     - |   97 | `/* The stream_filter_register() registry, defined with the userland half at the` |
|     - |   98 | ` * bottom of this file; the chain, the lookup and the create are needed by the` |
|     - |   99 | ` * attach path above it. */` |
|     - |  100 | `typedef struct phl_ufilter_reg phl_ufilter_reg;` |
|     - |  101 | `struct phl_ufilter_reg` |
|     - |  102 | `{` |
|     - |  103 | `	SyBlob sName;              /* the filter name, wildcards included */` |
|     - |  104 | `	SyBlob sClass;             /* the class that serves it */` |
|     - |  105 | `	phl_ufilter_reg *pNext;` |
|     - |  106 | `};` |
|     - |  107 |  |
|     - |  108 | `static phl_ufilter_reg * UserFilterFind(ph7_vm *pVm,const char *zName,int nName);` |
|     - |  109 | `static phl_stream_filter * UserFilterCreate(ph7_vm *pVm,phl_ufilter_reg *pReg,` |
|     - |  110 | `	const char *zName,int nName,ph7_value *pParams,ph7_value *pStream);` |
|     - |  111 | `/* --------------------------------------------------------------------------` |
|     - |  112 | ` * The built-in filters.` |
|     - |  113 | ` * -------------------------------------------------------------------------- */` |
|     - |  114 | `/* php's string.* trio: one byte in, one byte out, no state at all. */` |
|     - |  115 | `#define PHL_STRF_ROT13   0` |
|     - |  116 | `#define PHL_STRF_TOUPPER 1` |
|     - |  117 | `#define PHL_STRF_TOLOWER 2` |
|   126 |  118 | `static int StringFilterRun(phl_stream_filter *pFilter,phl_brigade *pIn,phl_brigade *pOut,` |
|     - |  119 | `	int iFlags,int iMode)` |
|     3 |  120 | `{` |
|     - |  121 | `	phl_bucket *pBucket;` |
|    63 |  122 | `	SXUNUSED(iFlags);` |
|    63 |  123 | `	SXUNUSED(pFilter);` |
|   197 |  124 | `	while( (pBucket = FilterBucketPop(pIn)) != 0 ){` |
|    71 |  125 | `		unsigned char *zData = (unsigned char *)SyBlobData(&pBucket->sData);` |
|    71 |  126 | `		sxu32 n,nLen = SyBlobLength(&pBucket->sData);` |
|   649 |  127 | `		for( n = 0 ; n < nLen ; n++ ){` |
|   581 |  128 | `			int c = zData[n];` |
|   581 |  129 | `			if( iMode == PHL_STRF_TOUPPER ){` |
|   198 |  130 | `				if( c >= 'a' && c <= 'z' ){` |
|   186 |  131 | `					c -= 32;` |
|    94 |  132 | `				}` |
|   482 |  133 | `			}else if( iMode == PHL_STRF_TOLOWER ){` |
|   156 |  134 | `				if( c >= 'A' && c <= 'Z' ){` |
|    52 |  135 | `					c += 32;` |
|    25 |  136 | `				}` |
|    79 |  137 | `			}else{` |
|   229 |  138 | `				if( c >= 'a' && c <= 'z' ){` |
|   177 |  139 | `					c = 'a' + (c - 'a' + 13) % 26;` |
|   141 |  140 | `				}else if( c >= 'A' && c <= 'Z' ){` |
|    19 |  141 | `					c = 'A' + (c - 'A' + 13) % 26;` |
|     9 |  142 | `				}` |
|     - |  143 | `			}` |
|   581 |  144 | `			zData[n] = (unsigned char)c;` |
|   292 |  145 | `		}` |
|    71 |  146 | `		PH7_FilterBucketAppend(pOut,pBucket);` |
|     3 |  147 | `	}` |
|   129 |  148 | `	return PHL_PSFS_PASS_ON;` |
|     3 |  149 | `}` |
|    42 |  150 | `static int Rot13Filter(phl_stream_filter *pF,phl_brigade *pIn,phl_brigade *pOut,int iFlags)` |
|     1 |  151 | `{` |
|    43 |  152 | `	return StringFilterRun(pF,pIn,pOut,iFlags,PHL_STRF_ROT13);` |
|     1 |  153 | `}` |
|    68 |  154 | `static int ToUpperFilter(phl_stream_filter *pF,phl_brigade *pIn,phl_brigade *pOut,int iFlags)` |
|     2 |  155 | `{` |
|    70 |  156 | `	return StringFilterRun(pF,pIn,pOut,iFlags,PHL_STRF_TOUPPER);` |
|     2 |  157 | `}` |
|    16 |  158 | `static int ToLowerFilter(phl_stream_filter *pF,phl_brigade *pIn,phl_brigade *pOut,int iFlags)` |
|     2 |  159 | `{` |
|    18 |  160 | `	return StringFilterRun(pF,pIn,pOut,iFlags,PHL_STRF_TOLOWER);` |
|     2 |  161 | `}` |
|     - |  162 | `/* --------------------------------------------------------------------------` |
|     - |  163 | ` * convert.* — the CODEC filters, and the shape they share.` |
|     - |  164 | ` *` |
|     - |  165 | `` * php registers ONE factory for the whole `convert.` prefix and picks the codec`` |
|     - |  166 | ` * from the rest of the name. All four are stateful across calls: a chunk` |
|     - |  167 | `` * boundary can fall in the middle of a base64 group or of a `=XX` escape, so`` |
|     - |  168 | ` * what cannot be finished yet is carried to the next call and what is still` |
|     - |  169 | ` * carried at the closing one is what the tail is made of.` |
|     - |  170 | ` * -------------------------------------------------------------------------- */` |
|     - |  171 | `#define PHL_CONV_B64_ENCODE 0` |
|     - |  172 | `#define PHL_CONV_B64_DECODE 1` |
|     - |  173 | `#define PHL_CONV_QP_ENCODE  2` |
|     - |  174 | `#define PHL_CONV_QP_DECODE  3` |
|     - |  175 | `typedef struct phl_conv_state phl_conv_state;` |
|     - |  176 | `struct phl_conv_state` |
|     - |  177 | `{` |
|     - |  178 | `	int iKind;        /* PHL_CONV_* */` |
|     - |  179 | ``	int iLineLen;     /* `line-length`, 0 = never wrap */`` |
|     - |  180 | `	int iCol;         /* how much of the current output line is used */` |
|     - |  181 | ``	int bBinary;      /* quoted-printable's `binary`: space and tab encoded too */`` |
|     - |  182 | `	int bErr;         /* a byte sequence php refuses was seen */` |
|     - |  183 | ``	int bPadded;      /* base64-decode: the `=` arrived, the rest is ignored */`` |
|     - |  184 | `	int iMatch;       /* qp-encode: bytes of sBreak matched so far in the INPUT */` |
|     - |  185 | ``	SyBlob sBreak;    /* `line-break-chars` */`` |
|     - |  186 | `};` |
|     - |  187 | ``/* The `line-length`/`line-break-chars`/`binary` triple, read once. php refuses a`` |
|     - |  188 | ` * $params that is present and not an array — which an explicit NULL is, so` |
|     - |  189 | ``  * `stream_filter_append($h,'convert.base64-encode',STREAM_FILTER_READ,null)` `` |
|     - |  190 | ` * fails where omitting the argument works. */` |
|    96 |  191 | `static int ConvFilterCreate(phl_stream_filter *pFilter,ph7_value *pParams)` |
|     1 |  192 | `{` |
|     - |  193 | `	phl_conv_state *pState;` |
|    97 |  194 | `	const char *zName = (const char *)SyBlobData(&pFilter->sName);` |
|    97 |  195 | `	int nName = (int)SyBlobLength(&pFilter->sName);` |
|     - |  196 | `	int iKind;` |
|   144 |  197 | `	if( nName > 8 && SyMemcmp(zName,"convert.",8) == 0 ){` |
|    97 |  198 | `		const char *z = &zName[8];` |
|    97 |  199 | `		int n = nName - 8;` |
|    97 |  200 | `		if( n == (int)sizeof("base64-encode")-1 && SyMemcmp(z,"base64-encode",13) == 0 ){` |
|    39 |  201 | `			iKind = PHL_CONV_B64_ENCODE;` |
|    78 |  202 | `		}else if( n == (int)sizeof("base64-decode")-1 && SyMemcmp(z,"base64-decode",13) == 0 ){` |
|    21 |  203 | `			iKind = PHL_CONV_B64_DECODE;` |
|    49 |  204 | `		}else if( n == (int)sizeof("quoted-printable-encode")-1` |
|    38 |  205 | `		       && SyMemcmp(z,"quoted-printable-encode",23) == 0 ){` |
|    27 |  206 | `			iKind = PHL_CONV_QP_ENCODE;` |
|    26 |  207 | `		}else if( n == (int)sizeof("quoted-printable-decode")-1` |
|    12 |  208 | `		       && SyMemcmp(z,"quoted-printable-decode",23) == 0 ){` |
|    11 |  209 | `			iKind = PHL_CONV_QP_DECODE;` |
|     6 |  210 | `		}else{` |
|     3 |  211 | `			return -1; /* convert.<something this build has no codec for> */` |
|     - |  212 | `		}` |
|    48 |  213 | `	}else{` |
|   ! 0 |  214 | `		return -1;` |
|     - |  215 | `	}` |
|    95 |  216 | `	if( pParams != 0 && !ph7_value_is_array(pParams) ){` |
|     - |  217 | `		char zMsg[96];` |
|     4 |  218 | `		SyBufferFormat(zMsg,sizeof(zMsg),"Stream filter (%.*s): invalid filter parameter",` |
|     1 |  219 | `			nName,zName);` |
|     3 |  220 | `		PH7_VmThrowError(pFilter->pVm,pFilter->pVm->pCalleeName,PH7_CTX_WARNING,zMsg);` |
|     3 |  221 | `		return -1;` |
|     - |  222 | `	}` |
|    93 |  223 | `	pState = (phl_conv_state *)SyMemBackendAlloc(&pFilter->pVm->sAllocator,sizeof(phl_conv_state));` |
|    93 |  224 | `	if( pState == 0 ){` |
|   ! 0 |  225 | `		return -1;` |
|     - |  226 | `	}` |
|    93 |  227 | `	SyZero(pState,sizeof(phl_conv_state));` |
|    93 |  228 | `	pState->iKind = iKind;` |
|    93 |  229 | `	SyBlobInit(&pState->sBreak,&pFilter->pVm->sAllocator);` |
|    93 |  230 | `	SyBlobAppend(&pState->sBreak,"\r\n",2); /* php's default */` |
|    93 |  231 | `	if( pParams ){` |
|     - |  232 | `		ph7_value *pVal;` |
|     - |  233 | `		/* $params is the script's own array: every read goes through a COPY, since` |
|     - |  234 | `		 * ph7_value_to_xxx() would convert the entry in place and rewrite it. */` |
|    85 |  235 | `		pVal = ph7_array_fetch(pParams,"line-length",-1);` |
|    85 |  236 | `		if( pVal ){` |
|    43 |  237 | `			pState->iLineLen = (int)PH7_ValuePeekInt64(pVal);` |
|    21 |  238 | `		}` |
|    85 |  239 | `		pVal = ph7_array_fetch(pParams,"line-break-chars",-1);` |
|    85 |  240 | `		if( pVal ){` |
|     - |  241 | `			ph7_value sTmp;` |
|     - |  242 | `			int nLb;` |
|     - |  243 | `			const char *zLb;` |
|    33 |  244 | `			PH7_MemObjInit(pFilter->pVm,&sTmp);` |
|    33 |  245 | `			zLb = ph7_value_to_string(PH7_ValuePeek(pVal,&sTmp),&nLb);` |
|    33 |  246 | `			SyBlobReset(&pState->sBreak);` |
|    33 |  247 | `			if( nLb > 0 ){` |
|    33 |  248 | `				SyBlobAppend(&pState->sBreak,zLb,(sxu32)nLb);` |
|    16 |  249 | `			}` |
|    33 |  250 | `			PH7_MemObjRelease(&sTmp);` |
|    16 |  251 | `		}` |
|    85 |  252 | `		pVal = ph7_array_fetch(pParams,"binary",-1);` |
|    85 |  253 | `		if( pVal ){` |
|     5 |  254 | `			pState->bBinary = PH7_ValuePeekBool(pVal);` |
|     2 |  255 | `		}` |
|    42 |  256 | `	}` |
|     - |  257 | `	/* php's wrapping thresholds, which are not the same on the two encoders:` |
|     - |  258 | `	 * base64 rounds the length DOWN to a whole group of four and wraps only at` |
|     - |  259 | ``	 * four or more, and quoted-printable wants room for a `=XX` escape plus its`` |
|     - |  260 | ``	 * own soft-break `=` — so anything under four never wraps at all. */`` |
|    93 |  261 | `	if( iKind == PHL_CONV_B64_ENCODE ){` |
|    37 |  262 | `		pState->iLineLen = (pState->iLineLen / 4) * 4;` |
|    18 |  263 | `	}` |
|    93 |  264 | `	if( pState->iLineLen < 4 ){` |
|    59 |  265 | `		pState->iLineLen = 0;` |
|    29 |  266 | `	}` |
|    93 |  267 | `	pFilter->pPriv = (void *)pState;` |
|    93 |  268 | `	return PH7_OK;` |
|    49 |  269 | `}` |
|    96 |  270 | `static void ConvFilterClose(phl_stream_filter *pFilter)` |
|     1 |  271 | `{` |
|    97 |  272 | `	phl_conv_state *pState = (phl_conv_state *)pFilter->pPriv;` |
|    97 |  273 | `	if( pState ){` |
|    93 |  274 | `		SyBlobRelease(&pState->sBreak);` |
|    93 |  275 | `		SyMemBackendFree(&pFilter->pVm->sAllocator,pState);` |
|    93 |  276 | `		pFilter->pPriv = 0;` |
|    46 |  277 | `	}` |
|    97 |  278 | `}` |
|     - |  279 | `/* Emit one output byte, wrapping when the codec asked for a line length. */` |
|  2268 |  280 | `static void ConvEmit(phl_conv_state *pState,SyBlob *pOut,int c,int bSoftBreak)` |
|     1 |  281 | `{` |
|  2269 |  282 | `	if( pState->iLineLen > 0 && SyBlobLength(&pState->sBreak) > 0 ){` |
|  1797 |  283 | `		int nRoom = bSoftBreak ? pState->iLineLen - 1 : pState->iLineLen;` |
|  1797 |  284 | `		if( pState->iCol >= nRoom ){` |
|   209 |  285 | `			if( bSoftBreak ){` |
|     9 |  286 | `				char eq = '=';` |
|     9 |  287 | `				SyBlobAppend(pOut,&eq,1);` |
|     4 |  288 | `			}` |
|   209 |  289 | `			SyBlobAppend(pOut,SyBlobData(&pState->sBreak),SyBlobLength(&pState->sBreak));` |
|   209 |  290 | `			pState->iCol = 0;` |
|   104 |  291 | `		}` |
|   898 |  292 | `	}` |
|     - |  293 | `	{` |
|  2269 |  294 | `		char ch = (char)c;` |
|  2269 |  295 | `		SyBlobAppend(pOut,&ch,1);` |
|     - |  296 | `	}` |
|  2269 |  297 | `	pState->iCol++;` |
|  2269 |  298 | `}` |
|     - |  299 | `static const char zB64Alpha[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";` |
|     - |  300 | `/* -1 for a byte base64 does not use; php SKIPS those rather than refusing. */` |
|  1740 |  301 | `static int ConvB64Value(int c)` |
|     1 |  302 | `{` |
|  1741 |  303 | `	const char *z = zB64Alpha;` |
|     - |  304 | `	int i;` |
| 54793 |  305 | `	for( i = 0 ; i < 64 ; i++ ){` |
| 54607 |  306 | `		if( z[i] == c ){` |
|  1555 |  307 | `			return i;` |
|     - |  308 | `		}` |
| 26527 |  309 | `	}` |
|   187 |  310 | `	return -1;` |
|   871 |  311 | `}` |
|   496 |  312 | `static void ConvB64EncodeBytes(phl_conv_state *pState,phl_stream_filter *pFilter,` |
|     - |  313 | `	const unsigned char *zIn,sxu32 nIn,SyBlob *pOut,int bClosing)` |
|     1 |  314 | `{` |
|     - |  315 | `	sxu32 i;` |
|  1905 |  316 | `	for( i = 0 ; i < nIn ; i++ ){` |
|  1409 |  317 | `		char b = (char)zIn[i];` |
|  1409 |  318 | `		SyBlobAppend(&pFilter->sCarry,&b,1);` |
|  1409 |  319 | `		if( SyBlobLength(&pFilter->sCarry) == 3 ){` |
|   459 |  320 | `			const unsigned char *z = (const unsigned char *)SyBlobData(&pFilter->sCarry);` |
|   459 |  321 | `			ConvEmit(pState,pOut,zB64Alpha[z[0]>>2],0);` |
|   459 |  322 | `			ConvEmit(pState,pOut,zB64Alpha[((z[0]&0x03)<<4)\|(z[1]>>4)],0);` |
|   459 |  323 | `			ConvEmit(pState,pOut,zB64Alpha[((z[1]&0x0F)<<2)\|(z[2]>>6)],0);` |
|   459 |  324 | `			ConvEmit(pState,pOut,zB64Alpha[z[2]&0x3F],0);` |
|   459 |  325 | `			SyBlobReset(&pFilter->sCarry);` |
|   229 |  326 | `		}` |
|   705 |  327 | `	}` |
|   497 |  328 | `	if( bClosing && SyBlobLength(&pFilter->sCarry) > 0 ){` |
|    23 |  329 | `		const unsigned char *z = (const unsigned char *)SyBlobData(&pFilter->sCarry);` |
|    23 |  330 | `		sxu32 n = SyBlobLength(&pFilter->sCarry);` |
|    23 |  331 | `		ConvEmit(pState,pOut,zB64Alpha[z[0]>>2],0);` |
|    23 |  332 | `		if( n == 1 ){` |
|    15 |  333 | `			ConvEmit(pState,pOut,zB64Alpha[(z[0]&0x03)<<4],0);` |
|    15 |  334 | `			ConvEmit(pState,pOut,'=',0);` |
|     8 |  335 | `		}else{` |
|     9 |  336 | `			ConvEmit(pState,pOut,zB64Alpha[((z[0]&0x03)<<4)\|(z[1]>>4)],0);` |
|     9 |  337 | `			ConvEmit(pState,pOut,zB64Alpha[(z[1]&0x0F)<<2],0);` |
|     - |  338 | `		}` |
|    23 |  339 | `		ConvEmit(pState,pOut,'=',0);` |
|    23 |  340 | `		SyBlobReset(&pFilter->sCarry);` |
|    11 |  341 | `	}` |
|   497 |  342 | `}` |
|     - |  343 | `static void ConvB64DecodeTail(phl_stream_filter *pFilter,SyBlob *pOut);` |
|   656 |  344 | `static void ConvB64DecodeBytes(phl_conv_state *pState,phl_stream_filter *pFilter,` |
|     - |  345 | `	const unsigned char *zIn,sxu32 nIn,SyBlob *pOut)` |
|     1 |  346 | `{` |
|     - |  347 | `	sxu32 i;` |
|  2425 |  348 | `	for( i = 0 ; i < nIn ; i++ ){` |
|     - |  349 | `		int v;` |
|  1769 |  350 | `		if( zIn[i] == '=' ){` |
|     - |  351 | `			/* Padding. php refuses one that BEGINS a group — there is nothing for` |
|     - |  352 | ``			 * it to pad — and otherwise ends the stream. A second `=` is the rest`` |
|     - |  353 | `			 * of the same padding and never a new group. */` |
|    29 |  354 | `			if( pState->bPadded ){` |
|    13 |  355 | `				continue;` |
|     - |  356 | `			}` |
|    17 |  357 | `			if( SyBlobLength(&pFilter->sCarry) == 0 ){` |
|   ! 0 |  358 | `				pState->bErr = 1;` |
|   ! 0 |  359 | `			}else{` |
|     - |  360 | `				/* The padding CLOSES the group: what the carry holds is decoded` |
|     - |  361 | `				 * now, not at the end of the stream — php hands those bytes to` |
|     - |  362 | `				 * the reader before anything that follows can refuse. */` |
|    17 |  363 | `				ConvB64DecodeTail(pFilter,pOut);` |
|     - |  364 | `			}` |
|    17 |  365 | `			pState->bPadded = 1;` |
|    17 |  366 | `			continue;` |
|     - |  367 | `		}` |
|  1741 |  368 | `		v = ConvB64Value(zIn[i]);` |
|  1741 |  369 | `		if( pState->bPadded ){` |
|     - |  370 | `			/* php ends the stream at the padding: another alphabet byte after it` |
|     - |  371 | `			 * is an invalid sequence, not something to ignore. */` |
|   ! 0 |  372 | `			if( v >= 0 ){` |
|   ! 0 |  373 | `				pState->bErr = 1;` |
|   ! 0 |  374 | `			}` |
|   ! 0 |  375 | `			continue;` |
|     - |  376 | `		}` |
|  1741 |  377 | `		if( v < 0 ){` |
|   187 |  378 | `			continue; /* php SKIPS a byte outside the alphabet */` |
|     - |  379 | `		}` |
|     - |  380 | `		{` |
|  1555 |  381 | `			char b = (char)v;` |
|  1555 |  382 | `			SyBlobAppend(&pFilter->sCarry,&b,1);` |
|     - |  383 | `		}` |
|  1555 |  384 | `		if( SyBlobLength(&pFilter->sCarry) == 4 ){` |
|   381 |  385 | `			const unsigned char *z = (const unsigned char *)SyBlobData(&pFilter->sCarry);` |
|     - |  386 | `			char zOut[3];` |
|   381 |  387 | `			zOut[0] = (char)((z[0]<<2)\|(z[1]>>4));` |
|   381 |  388 | `			zOut[1] = (char)((z[1]<<4)\|(z[2]>>2));` |
|   381 |  389 | `			zOut[2] = (char)((z[2]<<6)\|z[3]);` |
|   381 |  390 | `			SyBlobAppend(pOut,zOut,3);` |
|   381 |  391 | `			SyBlobReset(&pFilter->sCarry);` |
|   190 |  392 | `		}` |
|   778 |  393 | `	}` |
|   657 |  394 | `}` |
|     - |  395 | `/* What is left in the carry when the padding (or the stream) arrived. */` |
|    36 |  396 | `static void ConvB64DecodeTail(phl_stream_filter *pFilter,SyBlob *pOut)` |
|     1 |  397 | `{` |
|    37 |  398 | `	const unsigned char *z = (const unsigned char *)SyBlobData(&pFilter->sCarry);` |
|    37 |  399 | `	sxu32 n = SyBlobLength(&pFilter->sCarry);` |
|     - |  400 | `	char zOut[2];` |
|    37 |  401 | `	if( n >= 2 ){` |
|    17 |  402 | `		zOut[0] = (char)((z[0]<<2)\|(z[1]>>4));` |
|    17 |  403 | `		if( n >= 3 ){` |
|     3 |  404 | `			zOut[1] = (char)((z[1]<<4)\|(z[2]>>2));` |
|     3 |  405 | `			SyBlobAppend(pOut,zOut,2);` |
|     2 |  406 | `		}else{` |
|    15 |  407 | `			SyBlobAppend(pOut,zOut,1);` |
|     - |  408 | `		}` |
|     8 |  409 | `	}` |
|    37 |  410 | `	SyBlobReset(&pFilter->sCarry);` |
|    37 |  411 | `}` |
|     - |  412 | `/* php's printable set: everything but a byte that has to be escaped. Space and` |
|     - |  413 | ` * tab are literal in TEXT mode and escaped in binary mode; the line break the` |
|     - |  414 | ` * input carries is always escaped, since the only breaks php's encoder writes` |
|     - |  415 | ` * are the SOFT ones it makes itself. */` |
|   244 |  416 | `static int ConvQpLiteral(phl_conv_state *pState,int c)` |
|     1 |  417 | `{` |
|   245 |  418 | `	if( c == '=' ){` |
|     5 |  419 | `		return 0;` |
|     - |  420 | `	}` |
|   241 |  421 | `	if( c == ' ' \|\| c == '\t' ){` |
|    19 |  422 | `		return !pState->bBinary;` |
|     - |  423 | `	}` |
|   223 |  424 | `	return c >= 33 && c <= 126;` |
|   123 |  425 | `}` |
|     - |  426 | `static void ConvQpEncodeOne(phl_conv_state *pState,int c,SyBlob *pOut);` |
|     - |  427 | `/*` |
|     - |  428 | ` * php only looks for lines once a line LENGTH is set: with no wrapping there` |
|     - |  429 | ` * are no lines, so the configured break is not recognised in the input and a` |
|     - |  430 | ` * space is just a space. With wrapping on, two more rules appear, and they do` |
|     - |  431 | ` * NOT see the same distance.` |
|     - |  432 | ` *` |
|     - |  433 | ` * The input's own line break is a HARD one — written through untouched and` |
|     - |  434 | ` * starting the column over — and php remembers a partial match ACROSS calls, so` |
|     - |  435 | ` * a break split by a chunk boundary is still one.` |
|     - |  436 | ` *` |
|     - |  437 | ` * WHITESPACE is decided with what this call holds and nothing more: a space or` |
|     - |  438 | ` * tab may be written literally only when the next byte is a non-whitespace one` |
|     - |  439 | ` * with two more bytes behind it, because trailing whitespace is exactly what` |
|     - |  440 | ` * quoted-printable must escape and php cannot see past the buffer it was given.` |
|     - |  441 | ` * That is why the same input encodes DIFFERENTLY under a small chunk size —` |
|     - |  442 | `` * `stream_set_chunk_size($h,1)` escapes every space — and matching php means`` |
|     - |  443 | ` * looking exactly as far as php does.` |
|     - |  444 | ` */` |
|    34 |  445 | `static void ConvQpEncodeBytes(phl_conv_state *pState,const unsigned char *zIn,sxu32 nIn,SyBlob *pOut)` |
|     1 |  446 | `{` |
|    35 |  447 | `	const unsigned char *zBreak = (const unsigned char *)SyBlobData(&pState->sBreak);` |
|    35 |  448 | `	int nBreak = (int)SyBlobLength(&pState->sBreak);` |
|     - |  449 | `	/* BINARY mode has no lines at all: every byte that is not printable is` |
|     - |  450 | `	 * escaped, the input's own line ending included. */` |
|    35 |  451 | `	int bLines = pState->iLineLen > 0 && nBreak > 0 && !pState->bBinary;` |
|     - |  452 | `	sxu32 i;` |
|   291 |  453 | `	for( i = 0 ; i < nIn ; i++ ){` |
|   257 |  454 | `		int c = zIn[i];` |
|   257 |  455 | `		if( bLines ){` |
|   109 |  456 | `			if( c == zBreak[pState->iMatch] ){` |
|     9 |  457 | `				pState->iMatch++;` |
|     9 |  458 | `				if( pState->iMatch >= nBreak ){` |
|     5 |  459 | `					SyBlobAppend(pOut,zBreak,(sxu32)nBreak);` |
|     5 |  460 | `					pState->iCol = 0;` |
|     5 |  461 | `					pState->iMatch = 0;` |
|     2 |  462 | `				}` |
|     9 |  463 | `				continue;` |
|     - |  464 | `			}` |
|   101 |  465 | `			if( pState->iMatch > 0 ){` |
|   ! 0 |  466 | `				int k,nMatched = pState->iMatch;` |
|   ! 0 |  467 | `				pState->iMatch = 0;` |
|   ! 0 |  468 | `				for( k = 0 ; k < nMatched ; k++ ){` |
|   ! 0 |  469 | `					ConvQpEncodeOne(pState,zBreak[k],pOut);` |
|   ! 0 |  470 | `				}` |
|   ! 0 |  471 | `				if( c == zBreak[0] ){` |
|   ! 0 |  472 | `					pState->iMatch = 1;` |
|   ! 0 |  473 | `					continue;` |
|     - |  474 | `				}` |
|   ! 0 |  475 | `			}` |
|   101 |  476 | `			if( c == ' ' \|\| c == '\t' ){` |
|     - |  477 | `				/* …and the whitespace that ENDS a line is the whole point of the` |
|     - |  478 | `				 * escape, so a space right before the break is never literal. */` |
|    12 |  479 | `				int bLiteral = (i + 2 < nIn) && zIn[i+1] != ' ' && zIn[i+1] != '\t'` |
|    13 |  480 | `					&& zIn[i+1] != zBreak[0];` |
|    11 |  481 | `				if( bLiteral ){` |
|     5 |  482 | `					ConvEmit(pState,pOut,c,1);` |
|     3 |  483 | `				}else{` |
|     7 |  484 | `					pState->bBinary = 1;   /* escape it, just this once */` |
|     7 |  485 | `					ConvQpEncodeOne(pState,c,pOut);` |
|     7 |  486 | `					pState->bBinary = 0;` |
|     - |  487 | `				}` |
|    11 |  488 | `				continue;` |
|     - |  489 | `			}` |
|    45 |  490 | `		}` |
|   239 |  491 | `		ConvQpEncodeOne(pState,c,pOut);` |
|   120 |  492 | `	}` |
|    35 |  493 | `}` |
|   244 |  494 | `static void ConvQpEncodeOne(phl_conv_state *pState,int c,SyBlob *pOut)` |
|     1 |  495 | `{` |
|     - |  496 | `	static const char zHex[] = "0123456789ABCDEF";` |
|     - |  497 | `	{` |
|   245 |  498 | `		if( ConvQpLiteral(pState,c) ){` |
|   195 |  499 | `			ConvEmit(pState,pOut,c,1);` |
|    98 |  500 | `		}else{` |
|     - |  501 | `			/* A three-byte escape is never split across a soft break. */` |
|    50 |  502 | `			if( pState->iLineLen > 0 && SyBlobLength(&pState->sBreak) > 0` |
|    13 |  503 | `			 && pState->iCol + 3 > pState->iLineLen - 1 ){` |
|     7 |  504 | `				char eq = '=';` |
|     7 |  505 | `				SyBlobAppend(pOut,&eq,1);` |
|     7 |  506 | `				SyBlobAppend(pOut,SyBlobData(&pState->sBreak),SyBlobLength(&pState->sBreak));` |
|     7 |  507 | `				pState->iCol = 0;` |
|     3 |  508 | `			}` |
|    51 |  509 | `			ConvEmit(pState,pOut,'=',0);` |
|    51 |  510 | `			ConvEmit(pState,pOut,zHex[(c>>4)&0x0F],0);` |
|    51 |  511 | `			ConvEmit(pState,pOut,zHex[c&0x0F],0);` |
|     - |  512 | `		}` |
|     - |  513 | `	}` |
|   245 |  514 | `}` |
|   116 |  515 | `static int ConvHexValue(int c)` |
|     1 |  516 | `{` |
|   117 |  517 | `	if( c >= '0' && c <= '9' ){` |
|    69 |  518 | `		return c - '0';` |
|     - |  519 | `	}` |
|    49 |  520 | `	if( c >= 'A' && c <= 'F' ){` |
|    25 |  521 | `		return c - 'A' + 10;` |
|     - |  522 | `	}` |
|    25 |  523 | `	if( c >= 'a' && c <= 'f' ){` |
|   ! 0 |  524 | `		return c - 'a' + 10;` |
|     - |  525 | `	}` |
|    25 |  526 | `	return -1;` |
|    59 |  527 | `}` |
|     - |  528 | `/*` |
|     - |  529 | `` * The decoder carries an unfinished escape: `=`, `=A`, and the `=` of a SOFT`` |
|     - |  530 | ` * line break whose ending has not arrived yet are all states a chunk boundary` |
|     - |  531 | `` * can land in. A `=` followed by anything that is not two hex digits or a line`` |
|     - |  532 | ` * ending is what php calls an invalid byte sequence.` |
|     - |  533 | ` */` |
|    12 |  534 | `static void ConvQpDecodeBytes(phl_conv_state *pState,phl_stream_filter *pFilter,` |
|     - |  535 | `	const unsigned char *zIn,sxu32 nIn,SyBlob *pOut)` |
|     1 |  536 | `{` |
|     - |  537 | `	sxu32 i;` |
|   135 |  538 | `	for( i = 0 ; i < nIn ; i++ ){` |
|   123 |  539 | `		int c = zIn[i];` |
|   123 |  540 | `		sxu32 nCarry = SyBlobLength(&pFilter->sCarry);` |
|   123 |  541 | `		if( nCarry == 0 ){` |
|    87 |  542 | `			if( c == '=' ){` |
|    23 |  543 | `				char eq = '=';` |
|    23 |  544 | `				SyBlobAppend(&pFilter->sCarry,&eq,1);` |
|    12 |  545 | `			}else{` |
|    65 |  546 | `				char ch = (char)c;` |
|    65 |  547 | `				SyBlobAppend(pOut,&ch,1);` |
|     - |  548 | `			}` |
|    87 |  549 | `			continue;` |
|     - |  550 | `		}` |
|    37 |  551 | `		if( nCarry == 1 ){` |
|    21 |  552 | `			if( c == '\r' ){` |
|     - |  553 | `				/* Wait for the LF (or for the next byte, which decides). */` |
|     3 |  554 | `				char ch = (char)c;` |
|     3 |  555 | `				SyBlobAppend(&pFilter->sCarry,&ch,1);` |
|     3 |  556 | `				continue;` |
|     - |  557 | `			}` |
|    19 |  558 | `			if( c == '\n' ){` |
|     3 |  559 | `				SyBlobReset(&pFilter->sCarry); /* soft break: both bytes vanish */` |
|     3 |  560 | `				continue;` |
|     - |  561 | `			}` |
|    17 |  562 | `			if( ConvHexValue(c) < 0 ){` |
|     3 |  563 | `				pState->bErr = 1;` |
|     3 |  564 | `				SyBlobReset(&pFilter->sCarry);` |
|     3 |  565 | `				continue;` |
|     - |  566 | `			}` |
|     - |  567 | `			{` |
|    15 |  568 | `				char ch = (char)c;` |
|    15 |  569 | `				SyBlobAppend(&pFilter->sCarry,&ch,1);` |
|     - |  570 | `			}` |
|    15 |  571 | `			continue;` |
|     - |  572 | `		}` |
|     - |  573 | `		{` |
|    17 |  574 | `			const unsigned char *z = (const unsigned char *)SyBlobData(&pFilter->sCarry);` |
|    17 |  575 | `			if( z[1] == '\r' ){` |
|     - |  576 | ``				/* `=\r` then anything: the soft break ends, and a byte that is`` |
|     - |  577 | `				 * not the LF belongs to the output. */` |
|     3 |  578 | `				SyBlobReset(&pFilter->sCarry);` |
|     3 |  579 | `				if( c != '\n' ){` |
|   ! 0 |  580 | `					char ch = (char)c;` |
|   ! 0 |  581 | `					SyBlobAppend(pOut,&ch,1);` |
|   ! 0 |  582 | `				}` |
|     3 |  583 | `				continue;` |
|     - |  584 | `			}` |
|    15 |  585 | `			if( ConvHexValue(c) < 0 ){` |
|   ! 0 |  586 | `				pState->bErr = 1;` |
|   ! 0 |  587 | `				SyBlobReset(&pFilter->sCarry);` |
|   ! 0 |  588 | `				continue;` |
|     - |  589 | `			}` |
|     - |  590 | `			{` |
|    15 |  591 | `				char ch = (char)((ConvHexValue(z[1]) << 4) \| ConvHexValue(c));` |
|    15 |  592 | `				SyBlobAppend(pOut,&ch,1);` |
|     - |  593 | `			}` |
|    15 |  594 | `			SyBlobReset(&pFilter->sCarry);` |
|     - |  595 | `		}` |
|     8 |  596 | `	}` |
|    13 |  597 | `}` |
|  1254 |  598 | `static int ConvFilter(phl_stream_filter *pFilter,phl_brigade *pIn,phl_brigade *pOut,int iFlags)` |
|     1 |  599 | `{` |
|  1255 |  600 | `	phl_conv_state *pState = (phl_conv_state *)pFilter->pPriv;` |
|  1255 |  601 | `	int bClosing = (iFlags & PHL_PSFS_FLAG_FLUSH_CLOSE) != 0;` |
|     - |  602 | `	phl_bucket *pBucket;` |
|     - |  603 | `	SyBlob sOut;` |
|  1255 |  604 | `	if( pState == 0 ){` |
|   ! 0 |  605 | `		return PHL_PSFS_ERR_FATAL;` |
|     - |  606 | `	}` |
|  1255 |  607 | `	SyBlobInit(&sOut,&pFilter->pVm->sAllocator);` |
|  2419 |  608 | `	while( (pBucket = FilterBucketPop(pIn)) != 0 ){` |
|  1165 |  609 | `		const unsigned char *z = (const unsigned char *)SyBlobData(&pBucket->sData);` |
|  1165 |  610 | `		sxu32 n = SyBlobLength(&pBucket->sData);` |
|  1165 |  611 | `		switch( pState->iKind ){` |
|   463 |  612 | `		case PHL_CONV_B64_ENCODE: ConvB64EncodeBytes(pState,pFilter,z,n,&sOut,0); break;` |
|   657 |  613 | `		case PHL_CONV_B64_DECODE: ConvB64DecodeBytes(pState,pFilter,z,n,&sOut); break;` |
|    35 |  614 | `		case PHL_CONV_QP_ENCODE:  ConvQpEncodeBytes(pState,z,n,&sOut); break;` |
|    13 |  615 | `		default:                  ConvQpDecodeBytes(pState,pFilter,z,n,&sOut); break;` |
|     - |  616 | `		}` |
|  1165 |  617 | `		PH7_FilterBucketFree(pFilter->pVm,pBucket);` |
|     1 |  618 | `	}` |
|  1255 |  619 | `	if( bClosing ){` |
|    89 |  620 | `		switch( pState->iKind ){` |
|    35 |  621 | `		case PHL_CONV_B64_ENCODE: ConvB64EncodeBytes(pState,pFilter,0,0,&sOut,1); break;` |
|    21 |  622 | `		case PHL_CONV_B64_DECODE: ConvB64DecodeTail(pFilter,&sOut); break;` |
|    17 |  623 | `		default:` |
|     - |  624 | ``			/* A dangling `=` at the end of a quoted-printable stream is dropped,`` |
|     - |  625 | `			 * and an unfinished base64 group has already been handled above. */` |
|    35 |  626 | `			SyBlobReset(&pFilter->sCarry);` |
|    34 |  627 | `			break;` |
|     - |  628 | `		}` |
|    44 |  629 | `	}` |
|  1255 |  630 | `	if( pState->bErr ){` |
|     - |  631 | `		char zMsg[96];` |
|     7 |  632 | `		SyBufferFormat(zMsg,sizeof(zMsg),"Stream filter (%.*s): invalid byte sequence",` |
|     4 |  633 | `			(int)SyBlobLength(&pFilter->sName),(const char *)SyBlobData(&pFilter->sName));` |
|     - |  634 | `		/* php names the READER in this warning — it is raised from inside the` |
|     - |  635 | `		 * read, not from the call that attached the filter. */` |
|     5 |  636 | `		PH7_VmThrowError(pFilter->pVm,pFilter->pVm->pCalleeName,PH7_CTX_WARNING,zMsg);` |
|     5 |  637 | `		SyBlobRelease(&sOut);` |
|     5 |  638 | `		return PHL_PSFS_ERR_FATAL;` |
|     - |  639 | `	}` |
|  1251 |  640 | `	if( SyBlobLength(&sOut) > 0 ){` |
|   889 |  641 | `		PH7_FilterBucketAppend(pOut,` |
|   592 |  642 | `			PH7_FilterBucketNew(pFilter->pVm,SyBlobData(&sOut),SyBlobLength(&sOut)));` |
|   296 |  643 | `	}` |
|  1251 |  644 | `	SyBlobRelease(&sOut);` |
|  1251 |  645 | `	return PHL_PSFS_PASS_ON;` |
|   628 |  646 | `}` |
|     - |  647 | `/* --------------------------------------------------------------------------` |
|     - |  648 | ` * dechunk — HTTP's chunked transfer encoding, taken apart.` |
|     - |  649 | ` *` |
|     - |  650 | ` * php is forgiving here on purpose: a body that is not chunked at all comes` |
|     - |  651 | `` * back unchanged, an extension after the size (`4;name=value`) is ignored, and`` |
|     - |  652 | ` * a truncated body answers what arrived.` |
|     - |  653 | ` * -------------------------------------------------------------------------- */` |
|     - |  654 | `#define PHL_DECHUNK_SIZE  0  /* reading the size line */` |
|     - |  655 | `#define PHL_DECHUNK_DATA  1  /* copying nRemain bytes */` |
|     - |  656 | `#define PHL_DECHUNK_CRLF  2  /* eating the ending after a chunk */` |
|     - |  657 | `#define PHL_DECHUNK_DONE  3  /* the zero-size chunk arrived */` |
|     - |  658 | `#define PHL_DECHUNK_RAW   4  /* not chunked at all: pass everything through */` |
|     - |  659 | `typedef struct phl_dechunk_state phl_dechunk_state;` |
|     - |  660 | `struct phl_dechunk_state` |
|     - |  661 | `{` |
|     - |  662 | `	int iPhase;` |
|     - |  663 | `	ph7_int64 nRemain;` |
|     - |  664 | `};` |
|    12 |  665 | `static int DechunkCreate(phl_stream_filter *pFilter,ph7_value *pParams)` |
|     1 |  666 | `{` |
|     - |  667 | `	phl_dechunk_state *pState;` |
|     6 |  668 | `	SXUNUSED(pParams);` |
|    13 |  669 | `	pState = (phl_dechunk_state *)SyMemBackendAlloc(&pFilter->pVm->sAllocator,` |
|     - |  670 | `		sizeof(phl_dechunk_state));` |
|    13 |  671 | `	if( pState == 0 ){` |
|   ! 0 |  672 | `		return -1;` |
|     - |  673 | `	}` |
|    13 |  674 | `	SyZero(pState,sizeof(phl_dechunk_state));` |
|    13 |  675 | `	pFilter->pPriv = (void *)pState;` |
|    13 |  676 | `	return PH7_OK;` |
|     7 |  677 | `}` |
|    12 |  678 | `static void DechunkClose(phl_stream_filter *pFilter)` |
|     1 |  679 | `{` |
|    13 |  680 | `	if( pFilter->pPriv ){` |
|    13 |  681 | `		SyMemBackendFree(&pFilter->pVm->sAllocator,pFilter->pPriv);` |
|    13 |  682 | `		pFilter->pPriv = 0;` |
|     6 |  683 | `	}` |
|    13 |  684 | `}` |
|     - |  685 | `/* Read the size line out of the carry. Answers 1 when one was complete. */` |
|    22 |  686 | `static int DechunkTakeSize(phl_stream_filter *pFilter,phl_dechunk_state *pState)` |
|     1 |  687 | `{` |
|    23 |  688 | `	const char *z = (const char *)SyBlobData(&pFilter->sCarry);` |
|    23 |  689 | `	sxu32 n = SyBlobLength(&pFilter->sCarry);` |
|     - |  690 | `	sxu32 i,nLine;` |
|    23 |  691 | `	ph7_int64 nSize = 0;` |
|    23 |  692 | `	if( n < 1 ){` |
|   ! 0 |  693 | `		return 0;` |
|     - |  694 | `	}` |
|    23 |  695 | `	if( ConvHexValue((unsigned char)z[0]) < 0 ){` |
|     - |  696 | `		/* A size line has to START with a hex digit. php decides "this body is` |
|     - |  697 | `		 * not chunked" on that ONE byte and passes everything from there on` |
|     - |  698 | `		 * through unchanged — which is why a body with no line ending at all` |
|     - |  699 | `		 * still comes back whole, and why the trailer of a truncated chunked` |
|     - |  700 | `		 * body does too. */` |
|     5 |  701 | `		pState->iPhase = PHL_DECHUNK_RAW;` |
|     5 |  702 | `		return 1;` |
|     - |  703 | `	}` |
|    67 |  704 | `	for( i = 0 ; i < n ; i++ ){` |
|    67 |  705 | `		if( z[i] == '\n' ){` |
|    19 |  706 | `			break;` |
|     - |  707 | `		}` |
|    25 |  708 | `	}` |
|    19 |  709 | `	if( i >= n ){` |
|   ! 0 |  710 | `		return 0; /* no ending yet */` |
|     - |  711 | `	}` |
|    19 |  712 | `	nLine = i;` |
|    37 |  713 | `	for( i = 0 ; i < nLine ; i++ ){` |
|    37 |  714 | `		int v = ConvHexValue((unsigned char)z[i]);` |
|    37 |  715 | `		if( v < 0 ){` |
|    19 |  716 | `			break;` |
|     - |  717 | `		}` |
|    19 |  718 | `		nSize = nSize * 16 + v;` |
|    10 |  719 | `	}` |
|     - |  720 | `	/* Drop the line, extension and ending included. */` |
|     - |  721 | `	{` |
|     - |  722 | `		SyBlob sRest;` |
|    19 |  723 | `		SyBlobInit(&sRest,&pFilter->pVm->sAllocator);` |
|    19 |  724 | `		if( nLine + 1 < n ){` |
|    19 |  725 | `			SyBlobAppend(&sRest,&z[nLine+1],n - (nLine+1));` |
|     9 |  726 | `		}` |
|    19 |  727 | `		SyBlobReset(&pFilter->sCarry);` |
|    19 |  728 | `		if( SyBlobLength(&sRest) > 0 ){` |
|    19 |  729 | `			SyBlobAppend(&pFilter->sCarry,SyBlobData(&sRest),SyBlobLength(&sRest));` |
|     9 |  730 | `		}` |
|    19 |  731 | `		SyBlobRelease(&sRest);` |
|     - |  732 | `	}` |
|    19 |  733 | `	pState->nRemain = nSize;` |
|    19 |  734 | `	pState->iPhase = nSize > 0 ? PHL_DECHUNK_DATA : PHL_DECHUNK_DONE;` |
|    19 |  735 | `	return 1;` |
|    12 |  736 | `}` |
|    24 |  737 | `static int DechunkFilter(phl_stream_filter *pFilter,phl_brigade *pIn,phl_brigade *pOut,int iFlags)` |
|     1 |  738 | `{` |
|    25 |  739 | `	phl_dechunk_state *pState = (phl_dechunk_state *)pFilter->pPriv;` |
|     - |  740 | `	phl_bucket *pBucket;` |
|     - |  741 | `	SyBlob sOut;` |
|    12 |  742 | `	SXUNUSED(iFlags);` |
|    25 |  743 | `	if( pState == 0 ){` |
|   ! 0 |  744 | `		return PHL_PSFS_ERR_FATAL;` |
|     - |  745 | `	}` |
|    25 |  746 | `	SyBlobInit(&sOut,&pFilter->pVm->sAllocator);` |
|    37 |  747 | `	while( (pBucket = FilterBucketPop(pIn)) != 0 ){` |
|    13 |  748 | `		if( SyBlobLength(&pBucket->sData) > 0 ){` |
|    19 |  749 | `			SyBlobAppend(&pFilter->sCarry,SyBlobData(&pBucket->sData),` |
|     6 |  750 | `				SyBlobLength(&pBucket->sData));` |
|     6 |  751 | `		}` |
|    13 |  752 | `		PH7_FilterBucketFree(pFilter->pVm,pBucket);` |
|     1 |  753 | `	}` |
|    34 |  754 | `	for(;;){` |
|    69 |  755 | `		sxu32 nHave = SyBlobLength(&pFilter->sCarry);` |
|    69 |  756 | `		if( pState->iPhase == PHL_DECHUNK_DONE ){` |
|    13 |  757 | `			SyBlobReset(&pFilter->sCarry);` |
|    13 |  758 | `			break;` |
|     - |  759 | `		}` |
|    57 |  760 | `		if( pState->iPhase == PHL_DECHUNK_RAW ){` |
|     9 |  761 | `			if( nHave > 0 ){` |
|     5 |  762 | `				SyBlobAppend(&sOut,SyBlobData(&pFilter->sCarry),nHave);` |
|     5 |  763 | `				SyBlobReset(&pFilter->sCarry);` |
|     2 |  764 | `			}` |
|     9 |  765 | `			break;` |
|     - |  766 | `		}` |
|    49 |  767 | `		if( nHave < 1 ){` |
|     5 |  768 | `			break;` |
|     - |  769 | `		}` |
|    45 |  770 | `		if( pState->iPhase == PHL_DECHUNK_SIZE ){` |
|    23 |  771 | `			if( !DechunkTakeSize(pFilter,pState) ){` |
|   ! 0 |  772 | `				break;` |
|     - |  773 | `			}` |
|    23 |  774 | `			continue;` |
|     - |  775 | `		}` |
|    23 |  776 | `		if( pState->iPhase == PHL_DECHUNK_DATA ){` |
|    13 |  777 | `			ph7_int64 nTake = (ph7_int64)nHave;` |
|    13 |  778 | `			if( nTake > pState->nRemain ){` |
|    11 |  779 | `				nTake = pState->nRemain;` |
|     5 |  780 | `			}` |
|    13 |  781 | `			SyBlobAppend(&sOut,SyBlobData(&pFilter->sCarry),(sxu32)nTake);` |
|     - |  782 | `			{` |
|     - |  783 | `				SyBlob sRest;` |
|    13 |  784 | `				SyBlobInit(&sRest,&pFilter->pVm->sAllocator);` |
|    13 |  785 | `				if( (sxu32)nTake < nHave ){` |
|    11 |  786 | `					SyBlobAppend(&sRest,` |
|    10 |  787 | `						(const char *)SyBlobData(&pFilter->sCarry) + nTake,nHave - (sxu32)nTake);` |
|     5 |  788 | `				}` |
|    13 |  789 | `				SyBlobReset(&pFilter->sCarry);` |
|    13 |  790 | `				if( SyBlobLength(&sRest) > 0 ){` |
|    11 |  791 | `					SyBlobAppend(&pFilter->sCarry,SyBlobData(&sRest),SyBlobLength(&sRest));` |
|     5 |  792 | `				}` |
|    13 |  793 | `				SyBlobRelease(&sRest);` |
|     - |  794 | `			}` |
|    13 |  795 | `			pState->nRemain -= nTake;` |
|    13 |  796 | `			if( pState->nRemain < 1 ){` |
|    11 |  797 | `				pState->iPhase = PHL_DECHUNK_CRLF;` |
|     5 |  798 | `			}` |
|    13 |  799 | `			continue;` |
|     - |  800 | `		}` |
|     - |  801 | `		/* PHL_DECHUNK_CRLF: the ending that follows a chunk's bytes has to be` |
|     - |  802 | `		 * RIGHT THERE. php does not go looking for it — anything else means the` |
|     - |  803 | `		 * body was never chunked to begin with, and the rest passes through. */` |
|     5 |  804 | `		{` |
|    11 |  805 | `			const char *z = (const char *)SyBlobData(&pFilter->sCarry);` |
|     - |  806 | `			sxu32 i;` |
|    11 |  807 | `			if( z[0] == '\r' ){` |
|    11 |  808 | `				if( nHave < 2 ){` |
|   ! 0 |  809 | `					break; /* the LF may still be coming */` |
|     - |  810 | `				}` |
|    11 |  811 | `				if( z[1] != '\n' ){` |
|   ! 0 |  812 | `					pState->iPhase = PHL_DECHUNK_RAW;` |
|   ! 0 |  813 | `					continue;` |
|     - |  814 | `				}` |
|    11 |  815 | `				i = 1;` |
|     5 |  816 | `			}else if( z[0] == '\n' ){` |
|   ! 0 |  817 | `				i = 0;` |
|   ! 0 |  818 | `			}else{` |
|   ! 0 |  819 | `				pState->iPhase = PHL_DECHUNK_RAW;` |
|   ! 0 |  820 | `				continue;` |
|     - |  821 | `			}` |
|     - |  822 | `			{` |
|     - |  823 | `				SyBlob sRest;` |
|    11 |  824 | `				SyBlobInit(&sRest,&pFilter->pVm->sAllocator);` |
|    11 |  825 | `				if( i + 1 < nHave ){` |
|    11 |  826 | `					SyBlobAppend(&sRest,&z[i+1],nHave - (i+1));` |
|     5 |  827 | `				}` |
|    11 |  828 | `				SyBlobReset(&pFilter->sCarry);` |
|    11 |  829 | `				if( SyBlobLength(&sRest) > 0 ){` |
|    11 |  830 | `					SyBlobAppend(&pFilter->sCarry,SyBlobData(&sRest),SyBlobLength(&sRest));` |
|     5 |  831 | `				}` |
|    11 |  832 | `				SyBlobRelease(&sRest);` |
|     - |  833 | `			}` |
|    11 |  834 | `			pState->iPhase = PHL_DECHUNK_SIZE;` |
|    11 |  835 | `			continue;` |
|     - |  836 | `		}` |
|   ! 0 |  837 | `	}` |
|    25 |  838 | `	if( SyBlobLength(&sOut) > 0 ){` |
|    19 |  839 | `		PH7_FilterBucketAppend(pOut,` |
|    12 |  840 | `			PH7_FilterBucketNew(pFilter->pVm,SyBlobData(&sOut),SyBlobLength(&sOut)));` |
|     6 |  841 | `	}` |
|    25 |  842 | `	SyBlobRelease(&sOut);` |
|    25 |  843 | `	return PHL_PSFS_PASS_ON;` |
|    13 |  844 | `}` |
|     - |  845 | `/*` |
|     - |  846 | ` * The registry, in php's own registration order — which is the order` |
|     - |  847 | `` * stream_get_filters() answers in. A name ending in `.*` is a FACTORY: php`` |
|     - |  848 | `` * registers `convert.*` once and lets it answer for every convert.<something>,`` |
|     - |  849 | ` * which is why the lookup below falls back to progressively shorter wildcards.` |
|     - |  850 | ` *` |
|     - |  851 | `` * php's own list carries two more this build has no engine for — `zlib.*` and`` |
|     - |  852 | ``  * `convert.iconv.*` — and one it has no explicable behaviour for: `consumed` `` |
|     - |  853 | ` * passes every byte through (a userland filter placed after it receives them` |
|     - |  854 | `` * all) and yet php answers "" to `fgets()`, to `fread($h,100)` and to`` |
|     - |  855 | `` * `stream_get_contents()` while answering `fread($h,3)` correctly. It exists`` |
|     - |  856 | ` * for php://input's own bookkeeping; a name whose answer depends on WHICH` |
|     - |  857 | ` * reader asked is not one to reproduce, so it is left out rather than guessed.` |
|     - |  858 | ` */` |
|     - |  859 | `static const phl_filter_ops aBuiltinFilters[] = {` |
|     - |  860 | `	{ "string.rot13",   0, Rot13Filter,   0 },` |
|     - |  861 | `	{ "string.toupper", 0, ToUpperFilter, 0 },` |
|     - |  862 | `	{ "string.tolower", 0, ToLowerFilter, 0 },` |
|     - |  863 | `	{ "convert.*",      ConvFilterCreate, ConvFilter, ConvFilterClose },` |
|     - |  864 | `	{ "dechunk",        DechunkCreate,    DechunkFilter, DechunkClose },` |
|     - |  865 | `};` |
|     - |  866 | `/*` |
|     - |  867 | ` * Locate the ops behind a filter NAME. php tries the exact name first, then` |
|     - |  868 | `` * replaces everything after each trailing `.` with `*` and tries again, so`` |
|     - |  869 | `` * `convert.iconv.utf-8/utf-16` finds `convert.iconv.*` and then `convert.*`.`` |
|     - |  870 | `` * The comparison is case SENSITIVE: php answers `Unable to locate filter` for`` |
|     - |  871 | `` * `STRING.ROT13`.`` |
|     - |  872 | ` */` |
|    26 |  873 | `static const phl_filter_ops * FilterFindOpsExact(const char *zName,int nName)` |
|     1 |  874 | `{` |
|     - |  875 | `	sxu32 n;` |
|   149 |  876 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltinFilters) ; ++n ){` |
|   125 |  877 | `		const char *zCur = aBuiltinFilters[n].zName;` |
|   125 |  878 | `		if( (int)SyStrlen(zCur) == nName && SyMemcmp(zCur,zName,(sxu32)nName) == 0 ){` |
|     3 |  879 | `			return &aBuiltinFilters[n];` |
|     - |  880 | `		}` |
|    62 |  881 | `	}` |
|    25 |  882 | `	return 0;` |
|    14 |  883 | `}` |
|   218 |  884 | `static const phl_filter_ops * FilterFindOps(const char *zName,int nName)` |
|     3 |  885 | `{` |
|     - |  886 | `	char zWild[128];` |
|     - |  887 | `	sxu32 n;` |
|     - |  888 | `	int nTry;` |
|   997 |  889 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltinFilters) ; ++n ){` |
|   863 |  890 | `		const char *zCur = aBuiltinFilters[n].zName;` |
|   863 |  891 | `		if( (int)SyStrlen(zCur) == nName && SyMemcmp(zCur,zName,(sxu32)nName) == 0 ){` |
|    87 |  892 | `			return &aBuiltinFilters[n];` |
|     - |  893 | `		}` |
|   391 |  894 | `	}` |
|   135 |  895 | `	nTry = nName;` |
|    87 |  896 | `	for(;;){` |
|     - |  897 | `		/* Strip back to (and including) the last period still inside the prefix. */` |
|  2075 |  898 | `		while( nTry > 0 && zName[nTry-1] != '.' ){` |
|  1901 |  899 | `			nTry--;` |
|     1 |  900 | `		}` |
|   175 |  901 | `		if( nTry < 1 ){` |
|    39 |  902 | `			break;` |
|     - |  903 | `		}` |
|   137 |  904 | `		if( nTry + 1 < (int)sizeof(zWild) ){` |
|   137 |  905 | `			SyMemcpy(zName,zWild,(sxu32)nTry);` |
|   137 |  906 | `			zWild[nTry] = '*';` |
|   625 |  907 | `			for( n = 0 ; n < SX_ARRAYSIZE(aBuiltinFilters) ; ++n ){` |
|   585 |  908 | `				const char *zCur = aBuiltinFilters[n].zName;` |
|   585 |  909 | `				if( (int)SyStrlen(zCur) == nTry + 1 && SyMemcmp(zCur,zWild,(sxu32)(nTry+1)) == 0 ){` |
|    97 |  910 | `					return &aBuiltinFilters[n];` |
|     - |  911 | `				}` |
|   245 |  912 | `			}` |
|    20 |  913 | `		}` |
|    41 |  914 | `		nTry--; /* step past the period we just matched on */` |
|     1 |  915 | `	}` |
|    39 |  916 | `	return 0;` |
|   112 |  917 | `}` |
|     - |  918 | `/* --------------------------------------------------------------------------` |
|     - |  919 | ` * Filter instances.` |
|     - |  920 | ` * -------------------------------------------------------------------------- */` |
|    10 |  921 | `PH7_PRIVATE phl_stream_filter * PH7_StreamFilterFromValue(ph7_value *pVal)` |
|     1 |  922 | `{` |
|     - |  923 | `	phl_stream_filter *pFilter;` |
|    11 |  924 | `	if( pVal == 0 \|\| !ph7_value_is_resource(pVal) ){` |
|   ! 0 |  925 | `		return 0;` |
|     - |  926 | `	}` |
|    11 |  927 | `	pFilter = (phl_stream_filter *)ph7_value_to_resource(pVal);` |
|    11 |  928 | `	if( pFilter == 0 \|\| pFilter->base.iMagic != STREAM_FILTER_MAGIC ){` |
|     5 |  929 | `		return 0;` |
|     - |  930 | `	}` |
|     7 |  931 | `	return pFilter;` |
|     6 |  932 | `}` |
|     - |  933 | `/* Allocate one filter, chained on the VM registry so it goes back at reset. */` |
|   204 |  934 | `static phl_stream_filter * FilterNew(ph7_vm *pVm,const phl_filter_ops *pOps,` |
|     - |  935 | `	const char *zName,int nName)` |
|     3 |  936 | `{` |
|     - |  937 | `	phl_stream_filter *pFilter;` |
|   207 |  938 | `	pFilter = (phl_stream_filter *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_stream_filter));` |
|   207 |  939 | `	if( pFilter == 0 ){` |
|   ! 0 |  940 | `		return 0;` |
|     - |  941 | `	}` |
|   207 |  942 | `	SyZero(pFilter,sizeof(phl_stream_filter));` |
|   207 |  943 | `	pFilter->base.iMagic = STREAM_FILTER_MAGIC;` |
|   207 |  944 | `	pFilter->pVm = pVm;` |
|   207 |  945 | `	pFilter->pOps = pOps;` |
|   207 |  946 | `	SyBlobInit(&pFilter->sName,&pVm->sAllocator);` |
|   207 |  947 | `	SyBlobInit(&pFilter->sCarry,&pVm->sAllocator);` |
|   207 |  948 | `	if( nName > 0 ){` |
|   207 |  949 | `		SyBlobAppend(&pFilter->sName,zName,(sxu32)nName);` |
|   102 |  950 | `	}` |
|   207 |  951 | `	pFilter->pRegNext = (phl_stream_filter *)pVm->pStreamFilter;` |
|   207 |  952 | `	pVm->pStreamFilter = (void *)pFilter;` |
|   207 |  953 | `	return pFilter;` |
|   105 |  954 | `}` |
|     - |  955 | `/*` |
|     - |  956 | ` * Release one filter's own resources. The instance itself stays allocated until` |
|     - |  957 | ` * the VM resets — a ph7_value the script still holds names this pointer, and a` |
|     - |  958 | ` * probe of it has to stay in bounds — so the magic becomes the CLOSED one,` |
|     - |  959 | `` * which is what makes `is_resource($f)` false after stream_filter_remove()`` |
|     - |  960 | ` * exactly as php reports it.` |
|     - |  961 | ` */` |
|   204 |  962 | `static void FilterDispose(phl_stream_filter *pFilter)` |
|     3 |  963 | `{` |
|   207 |  964 | `	if( pFilter->pOps && pFilter->pOps->xClose ){` |
|   133 |  965 | `		pFilter->pOps->xClose(pFilter);` |
|    66 |  966 | `	}` |
|   207 |  967 | `	SyBlobRelease(&pFilter->sCarry);` |
|   207 |  968 | `	pFilter->pDev = 0;` |
|   207 |  969 | `	pFilter->pNext = 0;` |
|   207 |  970 | `	pFilter->base.iMagic = IO_PRIVATE_CLOSED_MAGIC;` |
|   207 |  971 | `}` |
|     - |  972 | `/* --------------------------------------------------------------------------` |
|     - |  973 | ` * Running a chain.` |
|     - |  974 | ` * -------------------------------------------------------------------------- */` |
|     - |  975 | `/* One filter's turn. Built-in ops run their routine; the userland half hooks in` |
|     - |  976 | ` * here when it lands. */` |
|  1444 |  977 | `static int FilterInvoke(phl_stream_filter *pFilter,phl_brigade *pIn,phl_brigade *pOut,int iFlags)` |
|     3 |  978 | `{` |
|  1447 |  979 | `	if( pFilter->pOps == 0 \|\| pFilter->pOps->xFilter == 0 ){` |
|   ! 0 |  980 | `		return PHL_PSFS_ERR_FATAL;` |
|     - |  981 | `	}` |
|  1447 |  982 | `	if( iFlags & PHL_PSFS_FLAG_FLUSH_CLOSE ){` |
|   179 |  983 | `		if( pFilter->bClosed ){` |
|     - |  984 | `			/* A filter gets exactly ONE closing call: the device's end already` |
|     - |  985 | `			 * made it, and running a buffering codec's tail a second time (a` |
|     - |  986 | `			 * stream_filter_remove() after the last read, say) would emit that` |
|     - |  987 | `			 * tail twice. Whatever arrives now simply passes through. */` |
|     - |  988 | `			phl_bucket *pBucket;` |
|   ! 0 |  989 | `			while( (pBucket = FilterBucketPop(pIn)) != 0 ){` |
|   ! 0 |  990 | `				PH7_FilterBucketAppend(pOut,pBucket);` |
|   ! 0 |  991 | `			}` |
|   ! 0 |  992 | `			return PHL_PSFS_PASS_ON;` |
|     - |  993 | `		}` |
|   179 |  994 | `		pFilter->bClosed = 1;` |
|    88 |  995 | `	}` |
|     - |  996 | `	{` |
|  1447 |  997 | `		int rc = pFilter->pOps->xFilter(pFilter,pIn,pOut,iFlags);` |
|  1447 |  998 | `		if( rc == PHL_PSFS_ERR_FATAL ){` |
|     - |  999 | `			/* Marked, not skipped: php runs a filter that has already refused` |
|     - | 1000 | `			 * once again on the next write and reports the refusal again — what` |
|     - | 1001 | `			 * it does NOT do is run it a last time at close. */` |
|     9 | 1002 | `			pFilter->bDead = 1;` |
|     4 | 1003 | `		}` |
|  1447 | 1004 | `		return rc;` |
|     - | 1005 | `	}` |
|   725 | 1006 | `}` |
|     - | 1007 | `/*` |
|     - | 1008 | ` * iFlags describes the call for the HEAD of the chain and iRestFlags for` |
|     - | 1009 | ` * everything behind it, because the two are not always the same: the device's` |
|     - | 1010 | ` * end of file closes every filter on the stream, but flushing ONE filter — what` |
|     - | 1011 | ` * stream_filter_remove() does — closes only that one and hands its tail to the` |
|     - | 1012 | ` * others as ordinary data. Closing them too would make a codec below emit its` |
|     - | 1013 | `` * own tail early: removing an upstream `string.toupper` from a chain ending in`` |
|     - | 1014 | `` * `convert.base64-encode` padded the base64 there and then, where php leaves it`` |
|     - | 1015 | ` * mid-group.` |
|     - | 1016 | ` */` |
|  1428 | 1017 | `PH7_PRIVATE int PH7_FilterChainProcess(phl_stream_filter *pHead,` |
|     - | 1018 | `	const void *pData,sxu32 nLen,int iFlags,int iRestFlags,SyBlob *pOut,int *pbUnread)` |
|     3 | 1019 | `{` |
|  1431 | 1020 | `	ph7_vm *pVm = pHead->pVm;` |
|     - | 1021 | `	phl_brigade sA,sB;` |
|     - | 1022 | `	phl_brigade *pIn,*pOutBrig,*pSwap;` |
|     - | 1023 | `	phl_stream_filter *pFilter;` |
|     - | 1024 | `	phl_bucket *pBucket;` |
|  1431 | 1025 | `	int iStatus = PHL_PSFS_PASS_ON;` |
|  1431 | 1026 | `	SyZero(&sA,sizeof(sA));` |
|  1431 | 1027 | `	SyZero(&sB,sizeof(sB));` |
|  1431 | 1028 | `	if( nLen > 0 ){` |
|  1261 | 1029 | `		pBucket = PH7_FilterBucketNew(pVm,pData,nLen);` |
|  1261 | 1030 | `		if( pBucket == 0 ){` |
|   ! 0 | 1031 | `			return PHL_PSFS_ERR_FATAL;` |
|     - | 1032 | `		}` |
|  1261 | 1033 | `		PH7_FilterBucketAppend(&sA,pBucket);` |
|   629 | 1034 | `	}` |
|  1431 | 1035 | `	pIn = &sA;` |
|  1431 | 1036 | `	pOutBrig = &sB;` |
|  2863 | 1037 | `	for( pFilter = pHead ; pFilter ; pFilter = pFilter->pNext ){` |
|  1447 | 1038 | `		iStatus = FilterInvoke(pFilter,pIn,pOutBrig,pFilter == pHead ? iFlags : iRestFlags);` |
|  1447 | 1039 | `		if( iStatus != PHL_PSFS_PASS_ON ){` |
|    13 | 1040 | `			break;` |
|     - | 1041 | `		}` |
|     - | 1042 | `		/* Whatever the filter left behind is dropped: php warns about it from` |
|     - | 1043 | `		 * the reader ("Unprocessed filter buckets remaining on input brigade")` |
|     - | 1044 | `		 * and hands the read back as a failure, which is the ERR_FATAL path. */` |
|  1435 | 1045 | `		PH7_FilterBrigadeRelease(pVm,pIn);` |
|     - | 1046 | `		/* This filter's output is the next one's input. */` |
|  1435 | 1047 | `		pSwap = pIn;` |
|  1435 | 1048 | `		pIn = pOutBrig;` |
|  1435 | 1049 | `		pOutBrig = pSwap;` |
|   719 | 1050 | `	}` |
|  1431 | 1051 | `	if( iStatus != PHL_PSFS_PASS_ON && pIn->pHead != 0 ){` |
|     - | 1052 | `		/* A filter that gave up on its input without taking it: php says so and` |
|     - | 1053 | `		 * the READ answers FALSE rather than an end of file. A filter that` |
|     - | 1054 | `		 * consumed everything and then refused is the quiet shape. */` |
|     5 | 1055 | `		if( pbUnread ){` |
|   ! 0 | 1056 | `			*pbUnread = 1;` |
|   ! 0 | 1057 | `		}` |
|     5 | 1058 | `		PH7_VmThrowError(pVm,pVm->pCalleeName,PH7_CTX_WARNING,` |
|     - | 1059 | `			"Unprocessed filter buckets remaining on input brigade");` |
|     2 | 1060 | `	}` |
|  1431 | 1061 | `	if( iStatus == PHL_PSFS_PASS_ON && pOut ){` |
|  2103 | 1062 | `		while( (pBucket = FilterBucketPop(pIn)) != 0 ){` |
|   687 | 1063 | `			if( SyBlobLength(&pBucket->sData) > 0 ){` |
|   687 | 1064 | `				SyBlobAppend(pOut,SyBlobData(&pBucket->sData),SyBlobLength(&pBucket->sData));` |
|   342 | 1065 | `			}` |
|   687 | 1066 | `			PH7_FilterBucketFree(pVm,pBucket);` |
|     3 | 1067 | `		}` |
|   708 | 1068 | `	}` |
|  1431 | 1069 | `	PH7_FilterBrigadeRelease(pVm,&sA);` |
|  1431 | 1070 | `	PH7_FilterBrigadeRelease(pVm,&sB);` |
|  1431 | 1071 | `	return iStatus;` |
|   717 | 1072 | `}` |
|     - | 1073 | `/* --------------------------------------------------------------------------` |
|     - | 1074 | ` * Attaching, removing and releasing.` |
|     - | 1075 | ` * -------------------------------------------------------------------------- */` |
|     - | 1076 | `/* The chain head slot of a handle for one direction. */` |
| 30772 | 1077 | `static phl_stream_filter ** FilterChainSlot(io_private *pDev,int iChain)` |
|     5 | 1078 | `{` |
| 30777 | 1079 | `	if( iChain == PHL_STREAM_FILTER_WRITE ){` |
| 15325 | 1080 | `		return (phl_stream_filter **)&pDev->pWriteFilters;` |
|     - | 1081 | `	}` |
| 15457 | 1082 | `	return (phl_stream_filter **)&pDev->pReadFilters;` |
| 15389 | 1083 | `}` |
|     - | 1084 | `/* Unlink a filter from the chain it sits on. */` |
|     6 | 1085 | `static void FilterUnlink(phl_stream_filter *pFilter)` |
|     1 | 1086 | `{` |
|     - | 1087 | `	phl_stream_filter **ppSlot,*pCur;` |
|     7 | 1088 | `	if( pFilter->pDev == 0 ){` |
|   ! 0 | 1089 | `		return;` |
|     - | 1090 | `	}` |
|     7 | 1091 | `	ppSlot = FilterChainSlot(pFilter->pDev,pFilter->iChain);` |
|     7 | 1092 | `	pCur = *ppSlot;` |
|     7 | 1093 | `	if( pCur == pFilter ){` |
|     7 | 1094 | `		*ppSlot = pFilter->pNext;` |
|     7 | 1095 | `		return;` |
|     - | 1096 | `	}` |
|   ! 0 | 1097 | `	while( pCur ){` |
|   ! 0 | 1098 | `		if( pCur->pNext == pFilter ){` |
|   ! 0 | 1099 | `			pCur->pNext = pFilter->pNext;` |
|   ! 0 | 1100 | `			return;` |
|     - | 1101 | `		}` |
|   ! 0 | 1102 | `		pCur = pCur->pNext;` |
|   ! 0 | 1103 | `	}` |
|     4 | 1104 | `}` |
|     - | 1105 | `/*` |
|     - | 1106 | ` * The last call a filter ever gets. A write filter's tail has to reach the` |
|     - | 1107 | ` * device, and a read filter's has to reach the reader, so a flush is a chain` |
|     - | 1108 | ` * run from THIS filter down with no input and the closing flag.` |
|     - | 1109 | ` */` |
|    34 | 1110 | `static void FilterFlushTail(phl_stream_filter *pFilter,int iRestFlags)` |
|     2 | 1111 | `{` |
|    36 | 1112 | `	io_private *pDev = pFilter->pDev;` |
|     - | 1113 | `	phl_stream_filter *pCur;` |
|     - | 1114 | `	SyBlob sOut;` |
|    36 | 1115 | `	if( pDev == 0 ){` |
|   ! 0 | 1116 | `		return;` |
|     - | 1117 | `	}` |
|    74 | 1118 | `	for( pCur = pFilter ; pCur ; pCur = pCur->pNext ){` |
|    42 | 1119 | `		if( pCur->bDead ){` |
|     - | 1120 | `			/* A chain that already refused its input is finished: php does not` |
|     - | 1121 | `			 * run it again at close, and running it here would report the same` |
|     - | 1122 | `			 * refusal a second time from fclose(). */` |
|     3 | 1123 | `			return;` |
|     - | 1124 | `		}` |
|    21 | 1125 | `	}` |
|    34 | 1126 | `	SyBlobInit(&sOut,&pFilter->pVm->sAllocator);` |
|    32 | 1127 | `	if( PH7_FilterChainProcess(pFilter,0,0,PHL_PSFS_FLAG_FLUSH_CLOSE,iRestFlags,&sOut,0)` |
|    34 | 1128 | `	    == PHL_PSFS_PASS_ON && SyBlobLength(&sOut) > 0 ){` |
|     7 | 1129 | `		if( pFilter->iChain == PHL_STREAM_FILTER_WRITE ){` |
|     7 | 1130 | `			if( pDev->pStream && pDev->pStream->xWrite ){` |
|    10 | 1131 | `				pDev->pStream->xWrite(pDev->pHandle,SyBlobData(&sOut),` |
|     6 | 1132 | `					(ph7_int64)SyBlobLength(&sOut));` |
|     3 | 1133 | `			}` |
|     4 | 1134 | `		}else{` |
|   ! 0 | 1135 | `			SyBlobAppend(&pDev->sFilt,SyBlobData(&sOut),SyBlobLength(&sOut));` |
|     - | 1136 | `		}` |
|     3 | 1137 | `	}` |
|    34 | 1138 | `	SyBlobRelease(&sOut);` |
|    19 | 1139 | `}` |
| 14896 | 1140 | `PH7_PRIVATE void PH7_StreamFilterReleaseChains(io_private *pDev)` |
|     5 | 1141 | `{` |
|     - | 1142 | `	int i;` |
| 44693 | 1143 | `	for( i = 0 ; i < 2 ; i++ ){` |
| 29797 | 1144 | `		int iChain = i == 0 ? PHL_STREAM_FILTER_WRITE : PHL_STREAM_FILTER_READ;` |
| 29797 | 1145 | `		phl_stream_filter **ppSlot = FilterChainSlot(pDev,iChain);` |
| 29797 | 1146 | `		phl_stream_filter *pFilter = *ppSlot;` |
|     - | 1147 | `		/* The WRITE chain is flushed first and as a whole: the head's tail has` |
|     - | 1148 | `		 * to travel through the filters below it before anything reaches the` |
|     - | 1149 | `		 * device. */` |
| 29797 | 1150 | `		if( iChain == PHL_STREAM_FILTER_WRITE && pFilter ){` |
|    30 | 1151 | `			FilterFlushTail(pFilter,PHL_PSFS_FLAG_FLUSH_CLOSE);` |
|    14 | 1152 | `		}` |
| 29989 | 1153 | `		while( pFilter ){` |
|   195 | 1154 | `			phl_stream_filter *pNext = pFilter->pNext;` |
|   195 | 1155 | `			FilterDispose(pFilter);` |
|   195 | 1156 | `			pFilter = pNext;` |
|     3 | 1157 | `		}` |
| 29797 | 1158 | `		*ppSlot = 0;` |
| 14899 | 1159 | `	}` |
| 14901 | 1160 | `}` |
|   388 | 1161 | `PH7_PRIVATE void PH7_StreamFilterRewound(io_private *pDev)` |
|     5 | 1162 | `{` |
|     - | 1163 | `	int i;` |
|  1169 | 1164 | `	for( i = 0 ; i < 2 ; i++ ){` |
|  1169 | 1165 | `		phl_stream_filter *pFilter = *FilterChainSlot(pDev,` |
|   388 | 1166 | `			i == 0 ? PHL_STREAM_FILTER_READ : PHL_STREAM_FILTER_WRITE);` |
|   809 | 1167 | `		while( pFilter ){` |
|     - | 1168 | `			/* The stream moved, so the end it had reached is not the end any` |
|     - | 1169 | `			 * more: a chain closed at the old one must be able to run — and to` |
|     - | 1170 | `			 * emit its tail — again. */` |
|    29 | 1171 | `			pFilter->bClosed = 0;` |
|    29 | 1172 | `			pFilter = pFilter->pNext;` |
|     1 | 1173 | `		}` |
|   393 | 1174 | `	}` |
|   393 | 1175 | `}` |
|    16 | 1176 | `PH7_PRIVATE void PH7_StreamFilterVmReset(ph7_vm *pVm)` |
|   ! 0 | 1177 | `{` |
|     - | 1178 | `	phl_stream_filter *pFilter;` |
|    16 | 1179 | `	if( pVm == 0 ){` |
|   ! 0 | 1180 | `		return;` |
|     - | 1181 | `	}` |
|    16 | 1182 | `	pFilter = (phl_stream_filter *)pVm->pStreamFilter;` |
|    16 | 1183 | `	while( pFilter ){` |
|   ! 0 | 1184 | `		phl_stream_filter *pNext = pFilter->pRegNext;` |
|   ! 0 | 1185 | `		if( pFilter->base.iMagic == STREAM_FILTER_MAGIC ){` |
|     - | 1186 | `			/* The std handles outlive a reset (the -S server reuses one VM), so` |
|     - | 1187 | `			 * a filter that was never removed has to leave their chain before` |
|     - | 1188 | `			 * its memory goes back — otherwise the next request's first write` |
|     - | 1189 | `			 * walks a freed one. */` |
|   ! 0 | 1190 | `			io_private *pDev = pFilter->pDev;` |
|   ! 0 | 1191 | `			FilterUnlink(pFilter);` |
|   ! 0 | 1192 | `			if( pDev ){` |
|   ! 0 | 1193 | `				SyBlobReset(&pDev->sFilt);` |
|   ! 0 | 1194 | `				pDev->nFiltOfft = 0;` |
|   ! 0 | 1195 | `				pDev->bFiltDone = 0;` |
|   ! 0 | 1196 | `			}` |
|   ! 0 | 1197 | `			FilterDispose(pFilter);` |
|   ! 0 | 1198 | `		}` |
|   ! 0 | 1199 | `		SyBlobRelease(&pFilter->sName);` |
|   ! 0 | 1200 | `		pFilter->base.iMagic = 0;` |
|   ! 0 | 1201 | `		SyMemBackendFree(&pVm->sAllocator,pFilter);` |
|   ! 0 | 1202 | `		pFilter = pNext;` |
|   ! 0 | 1203 | `	}` |
|    16 | 1204 | `	pVm->pStreamFilter = 0;` |
|     - | 1205 | `	{` |
|    16 | 1206 | `		phl_ufilter_reg *pReg = (phl_ufilter_reg *)pVm->pUserFilters;` |
|    16 | 1207 | `		while( pReg ){` |
|   ! 0 | 1208 | `			phl_ufilter_reg *pNext = pReg->pNext;` |
|   ! 0 | 1209 | `			SyBlobRelease(&pReg->sName);` |
|   ! 0 | 1210 | `			SyBlobRelease(&pReg->sClass);` |
|   ! 0 | 1211 | `			SyMemBackendFree(&pVm->sAllocator,pReg);` |
|   ! 0 | 1212 | `			pReg = pNext;` |
|   ! 0 | 1213 | `		}` |
|    16 | 1214 | `		pVm->pUserFilters = 0;` |
|     - | 1215 | `	}` |
|    16 | 1216 | `	pVm->pFilterCall = 0;` |
|     8 | 1217 | `}` |
|     - | 1218 | `/* php's own two diagnostics, worded from the builtin that is running — which is` |
|     - | 1219 | `` * `stream_filter_append` on one path and the READER (file_get_contents, fopen)`` |
|     - | 1220 | ` * on the php://filter one. */` |
|    20 | 1221 | `static void FilterWarn(ph7_vm *pVm,const char *zFmt,int nName,const char *zName)` |
|     1 | 1222 | `{` |
|     - | 1223 | `	char zMsg[160];` |
|    21 | 1224 | `	SyBufferFormat(zMsg,sizeof(zMsg),zFmt,nName,zName);` |
|    21 | 1225 | `	PH7_VmThrowError(pVm,pVm->pCalleeName,PH7_CTX_WARNING,zMsg);` |
|    21 | 1226 | `}` |
|   218 | 1227 | `PH7_PRIVATE phl_stream_filter * PH7_StreamFilterAttach(ph7_vm *pVm,io_private *pDev,` |
|     - | 1228 | `	const char *zName,int nName,int iChain,int bPrepend,ph7_value *pParams,` |
|     - | 1229 | `	ph7_value *pStreamVal)` |
|     3 | 1230 | `{` |
|     - | 1231 | `	const phl_filter_ops *pOps;` |
|   221 | 1232 | `	phl_ufilter_reg *pReg = 0;` |
|     - | 1233 | `	phl_stream_filter *pFilter;` |
|   221 | 1234 | `	pOps = FilterFindOps(zName,nName);` |
|   221 | 1235 | `	if( pOps == 0 ){` |
|     - | 1236 | `		/* Nothing built in answers to it; a script may have registered one. */` |
|    39 | 1237 | `		pReg = UserFilterFind(pVm,zName,nName);` |
|    39 | 1238 | `		if( pReg == 0 ){` |
|    13 | 1239 | `			FilterWarn(pVm,"Unable to locate filter \"%.*s\"",nName,zName);` |
|    13 | 1240 | `			return 0;` |
|     - | 1241 | `		}` |
|    27 | 1242 | `		pFilter = UserFilterCreate(pVm,pReg,zName,nName,pParams,pStreamVal);` |
|    27 | 1243 | `		if( pFilter == 0 ){` |
|     5 | 1244 | `			FilterWarn(pVm,"Unable to create or locate filter \"%.*s\"",nName,zName);` |
|     5 | 1245 | `			return 0;` |
|     - | 1246 | `		}` |
|    23 | 1247 | `		goto attach;` |
|     - | 1248 | `	}` |
|   183 | 1249 | `	pFilter = FilterNew(pVm,pOps,zName,nName);` |
|   183 | 1250 | `	if( pFilter == 0 ){` |
|   ! 0 | 1251 | `		PH7_VmThrowError(pVm,pVm->pCalleeName,PH7_CTX_ERR,"PH7 is running out of memory");` |
|   ! 0 | 1252 | `		return 0;` |
|     - | 1253 | `	}` |
|   183 | 1254 | `	if( pOps->xCreate && pOps->xCreate(pFilter,pParams) != PH7_OK ){` |
|     5 | 1255 | `		FilterDispose(pFilter);` |
|     5 | 1256 | `		FilterWarn(pVm,"Unable to create or locate filter \"%.*s\"",nName,zName);` |
|     5 | 1257 | `		return 0;` |
|     - | 1258 | `	}` |
|    88 | 1259 | `attach:` |
|   201 | 1260 | `	pFilter->pDev = pDev;` |
|   201 | 1261 | `	pFilter->iChain = iChain;` |
|   201 | 1262 | `	if( bPrepend ){` |
|     3 | 1263 | `		phl_stream_filter **ppSlot = FilterChainSlot(pDev,iChain);` |
|     3 | 1264 | `		pFilter->pNext = *ppSlot;` |
|     3 | 1265 | `		*ppSlot = pFilter;` |
|     2 | 1266 | `	}else{` |
|   199 | 1267 | `		phl_stream_filter **ppSlot = FilterChainSlot(pDev,iChain);` |
|   199 | 1268 | `		phl_stream_filter *pCur = *ppSlot;` |
|   199 | 1269 | `		if( pCur == 0 ){` |
|   193 | 1270 | `			*ppSlot = pFilter;` |
|    98 | 1271 | `		}else{` |
|     7 | 1272 | `			while( pCur->pNext ){` |
|   ! 0 | 1273 | `				pCur = pCur->pNext;` |
|   ! 0 | 1274 | `			}` |
|     7 | 1275 | `			pCur->pNext = pFilter;` |
|     - | 1276 | `		}` |
|     - | 1277 | `	}` |
|   201 | 1278 | `	return pFilter;` |
|   112 | 1279 | `}` |
|     - | 1280 | `/* --------------------------------------------------------------------------` |
|     - | 1281 | ` * The builtins.` |
|     - | 1282 | ` * -------------------------------------------------------------------------- */` |
|     - | 1283 | `/*` |
|     - | 1284 | ` * resource\|false stream_filter_append(resource $stream, string $filter_name,` |
|     - | 1285 | ` *                                     int $mode = 0, mixed $params = null)` |
|     - | 1286 | ` * resource\|false stream_filter_prepend(...)` |
|     - | 1287 | ` *` |
|     - | 1288 | ` * php's $mode of 0 is not "no chain": it means "whichever chains this handle's` |
|     - | 1289 | `` * MODE makes sense for", so a stream opened `r+` gets the filter on BOTH — two`` |
|     - | 1290 | ` * separate instances, since a filter carries state and one cannot serve two` |
|     - | 1291 | ` * directions. The resource answered is the LAST one created, which is why` |
|     - | 1292 | `` * removing what `stream_filter_append($h,'…')` gave back on an `r+` handle`` |
|     - | 1293 | ` * leaves the READ half of it still filtering.` |
|     - | 1294 | ` */` |
|   170 | 1295 | `static int StreamFilterAddCommon(ph7_context *pCtx,int nArg,ph7_value **apArg,int bPrepend)` |
|     2 | 1296 | `{` |
|   172 | 1297 | `	phl_stream_filter *pFilter = 0;` |
|     - | 1298 | `	ph7_value *pParams;` |
|     - | 1299 | `	io_private *pDev;` |
|     - | 1300 | `	const char *zName;` |
|     - | 1301 | `	int nName,iChain,rc;` |
|   172 | 1302 | `	pDev = PH7_StreamHandleArg(pCtx,apArg[0],1,"stream",&rc);` |
|   172 | 1303 | `	if( pDev == 0 ){` |
|   ! 0 | 1304 | `		return rc;` |
|     - | 1305 | `	}` |
|   172 | 1306 | `	zName = ph7_value_to_string(apArg[1],&nName);` |
|   172 | 1307 | `	iChain = nArg > 2 ? (int)ph7_value_to_int(apArg[2]) : 0;` |
|   172 | 1308 | `	pParams = nArg > 3 ? apArg[3] : 0;` |
|   172 | 1309 | `	if( iChain == 0 ){` |
|     - | 1310 | `		/* php reads the mode the handle was OPENED with. */` |
|    22 | 1311 | `		const char *zMode = pDev->zMode;` |
|     - | 1312 | `		sxu32 nDummy;` |
|    22 | 1313 | `		int bPlus = SyByteFind(zMode,SyStrlen(zMode),'+',&nDummy) == SXRET_OK;` |
|    22 | 1314 | `		switch( zMode[0] ){` |
|    10 | 1315 | `		case 'r':` |
|    22 | 1316 | `			iChain = bPlus ? PHL_STREAM_FILTER_ALL : PHL_STREAM_FILTER_READ;` |
|    22 | 1317 | `			break;` |
|   ! 0 | 1318 | `		case 'w':` |
|     - | 1319 | `		case 'a':` |
|     - | 1320 | `		case 'x':` |
|     - | 1321 | `		case 'c':` |
|   ! 0 | 1322 | `			iChain = bPlus ? PHL_STREAM_FILTER_ALL : PHL_STREAM_FILTER_WRITE;` |
|   ! 0 | 1323 | `			break;` |
|   ! 0 | 1324 | `		default:` |
|   ! 0 | 1325 | `			break;` |
|     - | 1326 | `		}` |
|    10 | 1327 | `	}` |
|   172 | 1328 | `	if( iChain & PHL_STREAM_FILTER_READ ){` |
|   221 | 1329 | `		pFilter = PH7_StreamFilterAttach(pCtx->pVm,pDev,zName,nName,PHL_STREAM_FILTER_READ,` |
|    73 | 1330 | `			bPrepend,pParams,apArg[0]);` |
|   148 | 1331 | `		if( pFilter == 0 ){` |
|    17 | 1332 | `			ph7_result_bool(pCtx,0);` |
|    17 | 1333 | `			return PH7_OK;` |
|     - | 1334 | `		}` |
|    65 | 1335 | `	}` |
|   156 | 1336 | `	if( iChain & PHL_STREAM_FILTER_WRITE ){` |
|    44 | 1337 | `		pFilter = PH7_StreamFilterAttach(pCtx->pVm,pDev,zName,nName,PHL_STREAM_FILTER_WRITE,` |
|    14 | 1338 | `			bPrepend,pParams,apArg[0]);` |
|    30 | 1339 | `		if( pFilter == 0 ){` |
|   ! 0 | 1340 | `			ph7_result_bool(pCtx,0);` |
|   ! 0 | 1341 | `			return PH7_OK;` |
|     - | 1342 | `		}` |
|    14 | 1343 | `	}` |
|   156 | 1344 | `	if( pFilter == 0 ){` |
|     - | 1345 | `		/* A mode this engine could not place the filter on. */` |
|   ! 0 | 1346 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1347 | `		return PH7_OK;` |
|     - | 1348 | `	}` |
|   156 | 1349 | `	ph7_result_resource(pCtx,pFilter);` |
|   156 | 1350 | `	return PH7_OK;` |
|    87 | 1351 | `}` |
|   168 | 1352 | `PH7_PRIVATE int PH7_builtin_stream_filter_append(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 1353 | `{` |
|   170 | 1354 | `	return StreamFilterAddCommon(pCtx,nArg,apArg,0);` |
|     2 | 1355 | `}` |
|     2 | 1356 | `PH7_PRIVATE int PH7_builtin_stream_filter_prepend(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1357 | `{` |
|     3 | 1358 | `	return StreamFilterAddCommon(pCtx,nArg,apArg,1);` |
|     1 | 1359 | `}` |
|     - | 1360 | `/*` |
|     - | 1361 | ` * bool stream_filter_remove(resource $stream_filter)` |
|     - | 1362 | ` *` |
|     - | 1363 | ` * php FLUSHES the filter on the way out — a write filter's tail still reaches` |
|     - | 1364 | ` * the device and a read filter's still reaches the reader — and then the` |
|     - | 1365 | ` * resource is dead: passing it again is a TypeError, not FALSE.` |
|     - | 1366 | ` */` |
|    12 | 1367 | `PH7_PRIVATE int PH7_builtin_stream_filter_remove(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1368 | `{` |
|     - | 1369 | `	phl_stream_filter *pFilter;` |
|     6 | 1370 | `	SXUNUSED(nArg);` |
|    13 | 1371 | `	if( !ph7_value_is_resource(apArg[0]) ){` |
|     - | 1372 | `		/* php's ZPP runs first: a string is not "the wrong resource", it is not` |
|     - | 1373 | `		 * a resource at all, and the two diagnostics are different. */` |
|     4 | 1374 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 1375 | `			"%s(): Argument #1 ($stream_filter) must be of type resource, %s given",` |
|     1 | 1376 | `			ph7_function_name(pCtx),ph7_type_name(apArg[0]));` |
|     - | 1377 | `	}` |
|    11 | 1378 | `	pFilter = PH7_StreamFilterFromValue(apArg[0]);` |
|    11 | 1379 | `	if( pFilter == 0 ){` |
|     7 | 1380 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 1381 | `			"%s(): supplied resource is not a valid stream filter resource",` |
|     2 | 1382 | `			ph7_function_name(pCtx));` |
|     - | 1383 | `	}` |
|     - | 1384 | `	/* Only THIS filter closes; what it emits travels through the rest of the` |
|     - | 1385 | `	 * chain as ordinary data, because those filters stay on the stream. */` |
|     7 | 1386 | `	FilterFlushTail(pFilter,PHL_PSFS_FLAG_NORMAL);` |
|     7 | 1387 | `	FilterUnlink(pFilter);` |
|     7 | 1388 | `	FilterDispose(pFilter);` |
|     7 | 1389 | `	ph7_result_bool(pCtx,1);` |
|     7 | 1390 | `	return PH7_OK;` |
|     7 | 1391 | `}` |
|     - | 1392 | `/*` |
|     - | 1393 | ` * array stream_get_filters(void)` |
|     - | 1394 | ` *  The filter names this build can create, in php's own registration order.` |
|     - | 1395 | ` */` |
|     6 | 1396 | `PH7_PRIVATE int PH7_builtin_stream_get_filters(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1397 | `{` |
|     - | 1398 | `	ph7_value *pArray,*pValue;` |
|     - | 1399 | `	sxu32 n;` |
|     3 | 1400 | `	SXUNUSED(nArg);` |
|     3 | 1401 | `	SXUNUSED(apArg);` |
|     7 | 1402 | `	pArray = ph7_context_new_array(pCtx);` |
|     7 | 1403 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     7 | 1404 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|   ! 0 | 1405 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|   ! 0 | 1406 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1407 | `		return PH7_OK;` |
|     - | 1408 | `	}` |
|    37 | 1409 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltinFilters) ; ++n ){` |
|    31 | 1410 | `		ph7_value_string(pValue,aBuiltinFilters[n].zName,-1);` |
|    31 | 1411 | `		ph7_array_add_elem(pArray,0,pValue);` |
|    31 | 1412 | `		ph7_value_reset_string_cursor(pValue);` |
|    16 | 1413 | `	}` |
|     - | 1414 | `	{` |
|     - | 1415 | `		/* And whatever the script registered, newest last — php lists them` |
|     - | 1416 | `		 * beside its own. */` |
|     - | 1417 | `		phl_ufilter_reg *pReg;` |
|     - | 1418 | `		SySet aName;` |
|     - | 1419 | `		sxu32 i;` |
|     7 | 1420 | `		SySetInit(&aName,&pCtx->pVm->sAllocator,sizeof(phl_ufilter_reg *));` |
|     9 | 1421 | `		for( pReg = (phl_ufilter_reg *)pCtx->pVm->pUserFilters ; pReg ; pReg = pReg->pNext ){` |
|     3 | 1422 | `			SySetPut(&aName,(const void *)&pReg);` |
|     2 | 1423 | `		}` |
|     9 | 1424 | `		for( i = SySetUsed(&aName) ; i > 0 ; --i ){` |
|     3 | 1425 | `			phl_ufilter_reg **ppReg = (phl_ufilter_reg **)SySetAt(&aName,i-1);` |
|     4 | 1426 | `			ph7_value_string(pValue,(const char *)SyBlobData(&(*ppReg)->sName),` |
|     2 | 1427 | `				(int)SyBlobLength(&(*ppReg)->sName));` |
|     3 | 1428 | `			ph7_array_add_elem(pArray,0,pValue);` |
|     3 | 1429 | `			ph7_value_reset_string_cursor(pValue);` |
|     2 | 1430 | `		}` |
|     7 | 1431 | `		SySetRelease(&aName);` |
|     - | 1432 | `	}` |
|     7 | 1433 | `	ph7_result_value(pCtx,pArray);` |
|     7 | 1434 | `	return PH7_OK;` |
|     4 | 1435 | `}` |
|     - | 1436 | `/*` |
|     - | 1437 | ` * ---------------------------------------------------------------------------` |
|     - | 1438 | ` * php://filter/…/resource=… — the URL form of the same chain.` |
|     - | 1439 | ` *` |
|     - | 1440 | `` * The path after `filter/` is a list of `/`-separated segments: `read=a\|b` and`` |
|     - | 1441 | `` * `write=a\|b` name one chain each, and a bare `a\|b` names both (as far as the`` |
|     - | 1442 | ` * OPEN MODE allows — a read filter on a write-only handle is dropped). What php` |
|     - | 1443 | ` * does with the RESOURCE is worth spelling out, because it is not a clean split:` |
|     - | 1444 | `` * it looks for `/resource=` and truncates the list there, and when the path`` |
|     - | 1445 | `` * BEGINS with `resource=` — no slash before it — it takes the resource and`` |
|     - | 1446 | ` * leaves the list alone, so every segment of the resource path is then tried as` |
|     - | 1447 | `` * a filter name too. `php://filter/resource=/tmp/x` really does warn about`` |
|     - | 1448 | `` * `resource=`, `tmp` and `x` and then open the file.`` |
|     - | 1449 | ` * ---------------------------------------------------------------------------` |
|     - | 1450 | ` */` |
|    42 | 1451 | `static void FilterUrlOne(ph7_vm *pVm,io_private *pDev,const char *zList,int nList,int iChains)` |
|     2 | 1452 | `{` |
|    44 | 1453 | `	int i = 0;` |
|    88 | 1454 | `	while( i < nList ){` |
|    46 | 1455 | `		int j = i;` |
|   656 | 1456 | `		while( j < nList && zList[j] != '\|' ){` |
|   612 | 1457 | `			j++;` |
|     2 | 1458 | `		}` |
|    46 | 1459 | `		if( j > i ){` |
|    46 | 1460 | `			int bOk = 1;` |
|    46 | 1461 | `			if( iChains & PHL_STREAM_FILTER_READ ){` |
|    59 | 1462 | `				bOk = PH7_StreamFilterAttach(pVm,pDev,&zList[i],j-i,` |
|    38 | 1463 | `					PHL_STREAM_FILTER_READ,0,0,0) != 0;` |
|    19 | 1464 | `			}` |
|    46 | 1465 | `			if( bOk && (iChains & PHL_STREAM_FILTER_WRITE) ){` |
|    10 | 1466 | `				bOk = PH7_StreamFilterAttach(pVm,pDev,&zList[i],j-i,` |
|     6 | 1467 | `					PHL_STREAM_FILTER_WRITE,0,0,0) != 0;` |
|     3 | 1468 | `			}` |
|    46 | 1469 | `			if( !bOk ){` |
|     - | 1470 | `				/* The URL form says it TWICE: once about the name and once about` |
|     - | 1471 | `				 * the chain it could not be put on. The open still succeeds —` |
|     - | 1472 | `				 * php opens the resource with the filters it could make. */` |
|     - | 1473 | `				char zMsg[160];` |
|     7 | 1474 | `				SyBufferFormat(zMsg,sizeof(zMsg),"Unable to create filter (%.*s)",` |
|     2 | 1475 | `					j-i,&zList[i]);` |
|     5 | 1476 | `				PH7_VmThrowError(pVm,pVm->pCalleeName,PH7_CTX_WARNING,zMsg);` |
|     2 | 1477 | `			}` |
|    22 | 1478 | `		}` |
|    46 | 1479 | `		i = j + 1;` |
|     2 | 1480 | `	}` |
|    44 | 1481 | `}` |
|    44 | 1482 | `PH7_PRIVATE int PH7_StreamFilterParseUrl(ph7_vm *pVm,const char *zSpec,int nSpec,` |
|     - | 1483 | `	io_private *pDev,int iChains)` |
|     2 | 1484 | `{` |
|    46 | 1485 | `	int i = 0;` |
|    92 | 1486 | `	while( i < nSpec ){` |
|    48 | 1487 | `		int j = i,iWant = iChains;` |
|     - | 1488 | `		const char *zList;` |
|     - | 1489 | `		int nName;` |
|   906 | 1490 | `		while( j < nSpec && zSpec[j] != '/' ){` |
|   860 | 1491 | `			j++;` |
|     2 | 1492 | `		}` |
|    48 | 1493 | `		zList = &zSpec[i];` |
|    48 | 1494 | `		nName = j - i;` |
|    48 | 1495 | `		if( nName >= 5 && SyMemcmp(zList,"read=",5) == 0 ){` |
|    36 | 1496 | `			iWant = iChains & PHL_STREAM_FILTER_READ;` |
|    36 | 1497 | `			zList += 5;` |
|    36 | 1498 | `			nName -= 5;` |
|    30 | 1499 | `		}else if( nName >= 6 && SyMemcmp(zList,"write=",6) == 0 ){` |
|     9 | 1500 | `			iWant = iChains & PHL_STREAM_FILTER_WRITE;` |
|     9 | 1501 | `			zList += 6;` |
|     9 | 1502 | `			nName -= 6;` |
|     4 | 1503 | `		}` |
|    48 | 1504 | `		if( nName > 0 && iWant != 0 ){` |
|    44 | 1505 | `			FilterUrlOne(pVm,pDev,zList,nName,iWant);` |
|    21 | 1506 | `		}` |
|    48 | 1507 | `		i = j + 1;` |
|     2 | 1508 | `	}` |
|    46 | 1509 | `	return PH7_OK;` |
|     2 | 1510 | `}` |
|     - | 1511 |  |
|     - | 1512 | `/*` |
|     - | 1513 | ` * ---------------------------------------------------------------------------` |
|     - | 1514 | ` * Userland filters: stream_filter_register(), php_user_filter and the buckets.` |
|     - | 1515 | ` *` |
|     - | 1516 | ` * A userland filter is a CLASS, not a function: php instantiates it once per` |
|     - | 1517 | ` * attachment, tells it what name it was created under and what params it was` |
|     - | 1518 | ` * given, and then calls filter($in,$out,&$consumed,$closing) with two BRIGADE` |
|     - | 1519 | `` * handles. The script walks `$in` with stream_bucket_make_writeable(), which`` |
|     - | 1520 | ` * hands over one bucket at a time as a StreamBucket object, and appends what it` |
|     - | 1521 | `` * made to `$out`. What it RETURNS is the chain's answer: PSFS_PASS_ON,`` |
|     - | 1522 | ` * PSFS_FEED_ME or PSFS_ERR_FATAL.` |
|     - | 1523 | ` *` |
|     - | 1524 | ``  * The bucket the script sees is a VALUE — its bytes live in the object's `data` `` |
|     - | 1525 | ` * property, which the script may replace outright — so the C bucket ends at` |
|     - | 1526 | ` * make_writeable and stream_bucket_append() builds a new one from whatever the` |
|     - | 1527 | `` * object holds when it is appended. `$bucket->bucket` is the handle php shows`` |
|     - | 1528 | ` * there; it is a token owned by the call, and it goes back with it.` |
|     - | 1529 | ` * ---------------------------------------------------------------------------` |
|     - | 1530 | ` */` |
|     - | 1531 | ``/* The `bucket` handle a StreamBucket carries. It names nothing the engine reads`` |
|     - | 1532 | ` * back — the bytes are in the object — and exists because php shows one. */` |
|     - | 1533 | `typedef struct phl_bucket_tok phl_bucket_tok;` |
|     - | 1534 | `struct phl_bucket_tok` |
|     - | 1535 | `{` |
|     - | 1536 | `	io_private base;           /* resource header (base.iMagic == STREAM_BUCKET_MAGIC) */` |
|     - | 1537 | `	phl_bucket_tok *pNext;` |
|     - | 1538 | `};` |
|     - | 1539 | `/* The registration behind a name, php's own lookup: the exact name, then` |
|     - | 1540 | `` * progressively shorter `prefix.*` wildcards. */`` |
|    38 | 1541 | `static phl_ufilter_reg * UserFilterFind(ph7_vm *pVm,const char *zName,int nName)` |
|     1 | 1542 | `{` |
|     - | 1543 | `	phl_ufilter_reg *pReg;` |
|     - | 1544 | `	char zWild[128];` |
|     - | 1545 | `	int nTry;` |
|    63 | 1546 | `	for( pReg = (phl_ufilter_reg *)pVm->pUserFilters ; pReg ; pReg = pReg->pNext ){` |
|    48 | 1547 | `		if( (int)SyBlobLength(&pReg->sName) == nName` |
|    38 | 1548 | `		 && SyMemcmp(SyBlobData(&pReg->sName),zName,(sxu32)nName) == 0 ){` |
|    25 | 1549 | `			return pReg;` |
|     - | 1550 | `		}` |
|    13 | 1551 | `	}` |
|    15 | 1552 | `	nTry = nName;` |
|    13 | 1553 | `	for(;;){` |
|   151 | 1554 | `		while( nTry > 0 && zName[nTry-1] != '.' ){` |
|   125 | 1555 | `			nTry--;` |
|     1 | 1556 | `		}` |
|    27 | 1557 | `		if( nTry < 1 ){` |
|    13 | 1558 | `			break;` |
|     - | 1559 | `		}` |
|    15 | 1560 | `		if( nTry + 1 < (int)sizeof(zWild) ){` |
|    15 | 1561 | `			SyMemcpy(zName,zWild,(sxu32)nTry);` |
|    15 | 1562 | `			zWild[nTry] = '*';` |
|    35 | 1563 | `			for( pReg = (phl_ufilter_reg *)pVm->pUserFilters ; pReg ; pReg = pReg->pNext ){` |
|    22 | 1564 | `				if( (int)SyBlobLength(&pReg->sName) == nTry + 1` |
|    14 | 1565 | `				 && SyMemcmp(SyBlobData(&pReg->sName),zWild,(sxu32)(nTry+1)) == 0 ){` |
|     3 | 1566 | `					return pReg;` |
|     - | 1567 | `				}` |
|    11 | 1568 | `			}` |
|     6 | 1569 | `		}` |
|    13 | 1570 | `		nTry--;` |
|     1 | 1571 | `	}` |
|    13 | 1572 | `	return 0;` |
|    20 | 1573 | `}` |
|     - | 1574 | `/* Call one of the three methods on the filter's instance. */` |
|    86 | 1575 | `static int UserFilterCall(phl_stream_filter *pFilter,const char *zMethod,int nArg,` |
|     - | 1576 | `	ph7_value **apArg,ph7_value *pResult)` |
|     1 | 1577 | `{` |
|    87 | 1578 | `	ph7_class_instance *pObj = (ph7_class_instance *)pFilter->pObj;` |
|     - | 1579 | `	ph7_class_method *pMeth;` |
|    87 | 1580 | `	if( pObj == 0 ){` |
|   ! 0 | 1581 | `		return -1;` |
|     - | 1582 | `	}` |
|    87 | 1583 | `	pMeth = PH7_ClassExtractMethod(pObj->pClass,zMethod,(sxu32)SyStrlen(zMethod));` |
|    87 | 1584 | `	if( pMeth == 0 ){` |
|     - | 1585 | `		/* php requires nothing of the class but the name: a class that does not` |
|     - | 1586 | `		 * extend php_user_filter and declares none of the three is registered` |
|     - | 1587 | `		 * and attached without complaint, and only the missing filter() is ever` |
|     - | 1588 | `		 * noticed — at the READ. */` |
|   ! 0 | 1589 | `		return 1;` |
|     - | 1590 | `	}` |
|    87 | 1591 | `	if( PH7_VmCallClassMethod(pFilter->pVm,pObj,pMeth,pResult,nArg,apArg) != SXRET_OK ){` |
|   ! 0 | 1592 | `		return -1;` |
|     - | 1593 | `	}` |
|    87 | 1594 | `	return 0;` |
|    44 | 1595 | `}` |
|    24 | 1596 | `static void UserFilterClose(phl_stream_filter *pFilter)` |
|     1 | 1597 | `{` |
|     - | 1598 | `	ph7_value sRet;` |
|    25 | 1599 | `	if( pFilter->pObj ){` |
|    23 | 1600 | `		PH7_MemObjInit(pFilter->pVm,&sRet);` |
|    23 | 1601 | `		UserFilterCall(pFilter,"onClose",0,0,&sRet);` |
|    23 | 1602 | `		PH7_MemObjRelease(&sRet);` |
|     - | 1603 | `		/* The instance was created here and is held by nothing else. */` |
|    23 | 1604 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pFilter->pObj);` |
|    23 | 1605 | `		pFilter->pObj = 0;` |
|    11 | 1606 | `	}` |
|    25 | 1607 | `	if( pFilter->pStreamRes ){` |
|    25 | 1608 | `		ph7_release_value(pFilter->pVm,pFilter->pStreamRes);` |
|    25 | 1609 | `		pFilter->pStreamRes = 0;` |
|    12 | 1610 | `	}` |
|    25 | 1611 | `	pFilter->sIn.pBrig = 0;` |
|    25 | 1612 | `	pFilter->sOut.pBrig = 0;` |
|    25 | 1613 | `}` |
|     - | 1614 | `/* Build a brigade handle for one filter() call. */` |
|    80 | 1615 | `static void UserBrigadeInit(phl_brigade_res *pRes,ph7_vm *pVm,phl_brigade *pBrig)` |
|     1 | 1616 | `{` |
|    81 | 1617 | `	pRes->base.iMagic = STREAM_BRIGADE_MAGIC;` |
|    81 | 1618 | `	pRes->pVm = pVm;` |
|    81 | 1619 | `	pRes->pBrig = pBrig;` |
|    81 | 1620 | `}` |
|     - | 1621 | `/* The brigade behind the handle goes away with the call; the handle itself` |
|     - | 1622 | ` * stays in bounds, so a script that kept one simply finds it empty. */` |
|    80 | 1623 | `static void UserBrigadeDetach(phl_brigade_res *pRes)` |
|     1 | 1624 | `{` |
|    81 | 1625 | `	pRes->pBrig = 0;` |
|    81 | 1626 | `}` |
|    74 | 1627 | `static phl_brigade_res * UserBrigadeFromValue(ph7_value *pVal)` |
|     1 | 1628 | `{` |
|     - | 1629 | `	phl_brigade_res *pRes;` |
|    75 | 1630 | `	if( pVal == 0 \|\| !ph7_value_is_resource(pVal) ){` |
|   ! 0 | 1631 | `		return 0;` |
|     - | 1632 | `	}` |
|    75 | 1633 | `	pRes = (phl_brigade_res *)ph7_value_to_resource(pVal);` |
|    75 | 1634 | `	if( pRes == 0 \|\| pRes->base.iMagic != STREAM_BRIGADE_MAGIC ){` |
|   ! 0 | 1635 | `		return 0;` |
|     - | 1636 | `	}` |
|    75 | 1637 | `	return pRes;` |
|    38 | 1638 | `}` |
|     - | 1639 | `/* One StreamBucket object around a run of bytes, with the token php shows. */` |
|    24 | 1640 | `static ph7_class_instance * UserBucketObject(ph7_vm *pVm,const char *zData,int nData)` |
|     1 | 1641 | `{` |
|     - | 1642 | `	ph7_class *pClass;` |
|     - | 1643 | `	ph7_class_instance *pObj;` |
|     - | 1644 | `	phl_bucket_tok *pTok;` |
|     - | 1645 | `	ph7_value *pSlot;` |
|    25 | 1646 | `	pClass = PH7_VmExtractClass(pVm,"StreamBucket",sizeof("StreamBucket")-1,FALSE,0);` |
|    25 | 1647 | `	pObj = pClass ? PH7_NewClassInstance(pVm,pClass) : 0;` |
|    25 | 1648 | `	if( pObj == 0 ){` |
|   ! 0 | 1649 | `		return 0;` |
|     - | 1650 | `	}` |
|    25 | 1651 | `	pTok = (phl_bucket_tok *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_bucket_tok));` |
|    25 | 1652 | `	if( pTok ){` |
|    25 | 1653 | `		SyZero(pTok,sizeof(*pTok));` |
|    25 | 1654 | `		pTok->base.iMagic = STREAM_BUCKET_MAGIC;` |
|    25 | 1655 | `		pSlot = PH7_NativeAttr(pObj,"bucket");` |
|    25 | 1656 | `		if( pSlot ){` |
|    25 | 1657 | `			PH7_MemObjRelease(pSlot);` |
|    25 | 1658 | `			pSlot->x.pOther = (void *)pTok;` |
|    25 | 1659 | `			pSlot->iFlags = MEMOBJ_RES;` |
|    12 | 1660 | `		}` |
|    12 | 1661 | `	}` |
|    25 | 1662 | `	PH7_NativeSetAttrStr(pVm,pObj,"data",zData,nData);` |
|    25 | 1663 | `	PH7_NativeSetAttrInt(pVm,pObj,"datalen",(sxi64)nData);` |
|    25 | 1664 | `	PH7_NativeSetAttrInt(pVm,pObj,"dataLength",(sxi64)nData);` |
|    25 | 1665 | `	return pObj;` |
|    13 | 1666 | `}` |
|     - | 1667 | `/* The token goes back with the object that owns it — which is what keeps` |
|     - | 1668 | `` * `$bucket->bucket` in bounds for as long as the script holds the bucket. */`` |
|    22 | 1669 | `static void UserBucketRelease(ph7_vm *pVm,ph7_class_instance *pObj)` |
|     1 | 1670 | `{` |
|    23 | 1671 | `	ph7_value *pSlot = PH7_NativeAttr(pObj,"bucket");` |
|    23 | 1672 | `	if( pSlot && (pSlot->iFlags & MEMOBJ_RES) && pSlot->x.pOther ){` |
|    23 | 1673 | `		phl_bucket_tok *pTok = (phl_bucket_tok *)pSlot->x.pOther;` |
|    23 | 1674 | `		if( pTok->base.iMagic == STREAM_BUCKET_MAGIC ){` |
|    23 | 1675 | `			pTok->base.iMagic = 0;` |
|    23 | 1676 | `			SyMemBackendFree(&pVm->sAllocator,pTok);` |
|    11 | 1677 | `		}` |
|    23 | 1678 | `		pSlot->x.pOther = 0;` |
|    23 | 1679 | `		pSlot->iFlags = MEMOBJ_NULL;` |
|    11 | 1680 | `	}` |
|    23 | 1681 | `}` |
|     - | 1682 | `/*` |
|     - | 1683 | ` * The filter() call itself. php hands over four arguments — the two brigades,` |
|     - | 1684 | ` * a by-reference $consumed that arrives as NULL, and whether this is the last` |
|     - | 1685 | ` * call — and reads the answer as one of the PSFS_* codes.` |
|     - | 1686 | ` */` |
|    40 | 1687 | `static int UserFilterRun(phl_stream_filter *pFilter,phl_brigade *pIn,phl_brigade *pOut,int iFlags)` |
|     1 | 1688 | `{` |
|    41 | 1689 | `	ph7_vm *pVm = pFilter->pVm;` |
|     - | 1690 | `	ph7_value *apArg[4];` |
|     - | 1691 | `	ph7_value sRet;` |
|     - | 1692 | `	void *pSavedCall;` |
|    41 | 1693 | `	sxu32 nConsumedIdx = SXU32_HIGH;` |
|     - | 1694 | `	int i,rc,iStatus;` |
|    41 | 1695 | `	if( pFilter->pObj == 0 ){` |
|   ! 0 | 1696 | `		return PHL_PSFS_ERR_FATAL;` |
|     - | 1697 | `	}` |
|    41 | 1698 | `	UserBrigadeInit(&pFilter->sIn,pVm,pIn);` |
|    41 | 1699 | `	UserBrigadeInit(&pFilter->sOut,pVm,pOut);` |
|   201 | 1700 | `	for( i = 0 ; i < 4 ; i++ ){` |
|   161 | 1701 | `		apArg[i] = ph7_new_scalar(pVm);` |
|    81 | 1702 | `	}` |
|    41 | 1703 | `	if( apArg[0] == 0 \|\| apArg[1] == 0 \|\| apArg[2] == 0 \|\| apArg[3] == 0 ){` |
|   ! 0 | 1704 | `		for( i = 0 ; i < 4 ; i++ ){` |
|   ! 0 | 1705 | `			if( apArg[i] ){` |
|   ! 0 | 1706 | `				ph7_release_value(pVm,apArg[i]);` |
|   ! 0 | 1707 | `			}` |
|   ! 0 | 1708 | `		}` |
|   ! 0 | 1709 | `		UserBrigadeDetach(&pFilter->sIn);` |
|   ! 0 | 1710 | `		UserBrigadeDetach(&pFilter->sOut);` |
|   ! 0 | 1711 | `		return PHL_PSFS_ERR_FATAL;` |
|     - | 1712 | `	}` |
|    41 | 1713 | `	ph7_value_resource(apArg[0],(void *)&pFilter->sIn);` |
|    41 | 1714 | `	ph7_value_resource(apArg[1],(void *)&pFilter->sOut);` |
|     - | 1715 | `	/* php's $consumed is BY REFERENCE and arrives NULL, not 0. A by-ref` |
|     - | 1716 | `	 * parameter binds to a caller SLOT, and the engine building the argument` |
|     - | 1717 | `	 * has none to offer — so one is reserved here, exactly as a variable would` |
|     - | 1718 | `	 * have, and the filter writes into it for real. */` |
|     - | 1719 | `	{` |
|    41 | 1720 | `		ph7_value *pSlot = VmReserveMemObj(pVm,&nConsumedIdx);` |
|    41 | 1721 | `		if( pSlot == 0 ){` |
|   ! 0 | 1722 | `			for( i = 0 ; i < 4 ; i++ ){` |
|   ! 0 | 1723 | `				ph7_release_value(pVm,apArg[i]);` |
|   ! 0 | 1724 | `			}` |
|   ! 0 | 1725 | `			UserBrigadeDetach(&pFilter->sIn);` |
|   ! 0 | 1726 | `			UserBrigadeDetach(&pFilter->sOut);` |
|   ! 0 | 1727 | `			return PHL_PSFS_ERR_FATAL;` |
|     - | 1728 | `		}` |
|    41 | 1729 | `		PH7_MemObjInit(pVm,pSlot);` |
|    41 | 1730 | `		pSlot->nIdx = nConsumedIdx;` |
|    41 | 1731 | `		ph7_value_null(apArg[2]);` |
|    41 | 1732 | `		apArg[2]->nIdx = nConsumedIdx;` |
|     - | 1733 | `	}` |
|    41 | 1734 | `	ph7_value_bool(apArg[3],(iFlags & PHL_PSFS_FLAG_FLUSH_CLOSE) != 0);` |
|     - | 1735 | ``	/* php sets `$this->stream` for the duration of the call and for no longer:`` |
|     - | 1736 | `	 * onCreate() sees nothing there. */` |
|    41 | 1737 | `	if( pFilter->pStreamRes ){` |
|    41 | 1738 | `		ph7_value *pSlot = PH7_NativeAttr((ph7_class_instance *)pFilter->pObj,"stream");` |
|    41 | 1739 | `		if( pSlot ){` |
|    41 | 1740 | `			PH7_MemObjStore(pFilter->pStreamRes,pSlot);` |
|    20 | 1741 | `		}` |
|    20 | 1742 | `	}` |
|    41 | 1743 | `	pSavedCall = pVm->pFilterCall;` |
|    41 | 1744 | `	pVm->pFilterCall = (void *)&pFilter->sOut;` |
|    41 | 1745 | `	PH7_MemObjInit(pVm,&sRet);` |
|    41 | 1746 | `	rc = UserFilterCall(pFilter,"filter",4,apArg,&sRet);` |
|    41 | 1747 | `	pVm->pFilterCall = pSavedCall;` |
|    41 | 1748 | `	iStatus = rc == 0 ? (int)ph7_value_to_int(&sRet) : PHL_PSFS_ERR_FATAL;` |
|    41 | 1749 | `	PH7_MemObjRelease(&sRet);` |
|   201 | 1750 | `	for( i = 0 ; i < 4 ; i++ ){` |
|   161 | 1751 | `		ph7_release_value(pVm,apArg[i]);` |
|    81 | 1752 | `	}` |
|    41 | 1753 | `	if( nConsumedIdx != SXU32_HIGH ){` |
|    41 | 1754 | `		PH7_VmReleaseUnheldSlot(pVm,nConsumedIdx);` |
|    20 | 1755 | `	}` |
|     - | 1756 | ``	/* `stream` is set for the DURATION of the call, so onClose() finds nothing`` |
|     - | 1757 | `	 * there — which is what php shows. */` |
|     - | 1758 | `	{` |
|    41 | 1759 | `		ph7_value *pSlot = PH7_NativeAttr((ph7_class_instance *)pFilter->pObj,"stream");` |
|    41 | 1760 | `		if( pSlot ){` |
|    41 | 1761 | `			PH7_MemObjRelease(pSlot);` |
|    20 | 1762 | `		}` |
|     - | 1763 | `	}` |
|    41 | 1764 | `	UserBrigadeDetach(&pFilter->sIn);` |
|    41 | 1765 | `	UserBrigadeDetach(&pFilter->sOut);` |
|    41 | 1766 | `	if( iStatus != PHL_PSFS_PASS_ON && iStatus != PHL_PSFS_FEED_ME ){` |
|     5 | 1767 | `		return PHL_PSFS_ERR_FATAL;` |
|     - | 1768 | `	}` |
|    37 | 1769 | `	return iStatus;` |
|    21 | 1770 | `}` |
|     - | 1771 | `static const phl_filter_ops sUserFilterOps = { "", 0, UserFilterRun, UserFilterClose };` |
|     - | 1772 | `/*` |
|     - | 1773 | ` * Create the instance behind one attachment. php refuses when the class is not` |
|     - | 1774 | ` * defined and when onCreate() answers FALSE, and says so twice on the second` |
|     - | 1775 | ` * one — once about the class, once about the filter.` |
|     - | 1776 | ` */` |
|    26 | 1777 | `static phl_stream_filter * UserFilterCreate(ph7_vm *pVm,phl_ufilter_reg *pReg,` |
|     - | 1778 | `	const char *zName,int nName,ph7_value *pParams,ph7_value *pStream)` |
|     1 | 1779 | `{` |
|     - | 1780 | `	phl_stream_filter *pFilter;` |
|     - | 1781 | `	ph7_class *pClass;` |
|     - | 1782 | `	ph7_class_instance *pObj;` |
|     - | 1783 | `	ph7_value sRet;` |
|    27 | 1784 | `	int nClass = (int)SyBlobLength(&pReg->sClass);` |
|    27 | 1785 | `	const char *zClass = (const char *)SyBlobData(&pReg->sClass);` |
|    27 | 1786 | `	pClass = PH7_VmExtractClass(pVm,zClass,(sxu32)nClass,FALSE,0);` |
|    27 | 1787 | `	if( pClass == 0 ){` |
|     - | 1788 | `		char zMsg[192];` |
|     4 | 1789 | `		SyBufferFormat(zMsg,sizeof(zMsg),` |
|     - | 1790 | `			"User-filter \"%.*s\" requires class \"%.*s\", but that class is not defined",` |
|     1 | 1791 | `			nName,zName,nClass,zClass);` |
|     3 | 1792 | `		PH7_VmThrowError(pVm,pVm->pCalleeName,PH7_CTX_WARNING,zMsg);` |
|     3 | 1793 | `		return 0;` |
|     - | 1794 | `	}` |
|    25 | 1795 | `	pObj = PH7_NewClassInstance(pVm,pClass);` |
|    25 | 1796 | `	if( pObj == 0 ){` |
|   ! 0 | 1797 | `		return 0;` |
|     - | 1798 | `	}` |
|    25 | 1799 | `	pFilter = FilterNew(pVm,&sUserFilterOps,zName,nName);` |
|    25 | 1800 | `	if( pFilter == 0 ){` |
|   ! 0 | 1801 | `		PH7_ClassInstanceUnref(pObj);` |
|   ! 0 | 1802 | `		return 0;` |
|     - | 1803 | `	}` |
|    25 | 1804 | `	pFilter->pObj = (void *)pObj;` |
|     - | 1805 | `	/* The name it was created UNDER, which a wildcard registration needs: a` |
|     - | 1806 | ``	 * `my.*` filter asked for as `my.thing` is told `my.thing`. */`` |
|    25 | 1807 | `	PH7_NativeSetAttrStr(pVm,pObj,"filtername",zName,nName);` |
|     - | 1808 | `	{` |
|    25 | 1809 | `		ph7_value *pSlot = PH7_NativeAttr(pObj,"params");` |
|    25 | 1810 | `		if( pSlot ){` |
|    25 | 1811 | `			if( pParams ){` |
|     3 | 1812 | `				PH7_MemObjStore(pParams,pSlot);` |
|     2 | 1813 | `			}else{` |
|    23 | 1814 | `				PH7_MemObjRelease(pSlot);` |
|     - | 1815 | `			}` |
|    12 | 1816 | `		}` |
|     - | 1817 | `	}` |
|    25 | 1818 | `	if( pStream ){` |
|    25 | 1819 | `		pFilter->pStreamRes = ph7_new_scalar(pVm);` |
|    25 | 1820 | `		if( pFilter->pStreamRes ){` |
|    25 | 1821 | `			PH7_MemObjStore(pStream,pFilter->pStreamRes);` |
|    12 | 1822 | `		}` |
|    12 | 1823 | `	}` |
|    25 | 1824 | `	PH7_MemObjInit(pVm,&sRet);` |
|     - | 1825 | `	/* A class with no onCreate() of its own simply has nothing to refuse with. */` |
|    25 | 1826 | `	if( UserFilterCall(pFilter,"onCreate",0,0,&sRet) == 0 && !ph7_value_to_bool(&sRet) ){` |
|     3 | 1827 | `		PH7_MemObjRelease(&sRet);` |
|     - | 1828 | `		/* php does not call onClose() for a filter onCreate() refused, so the` |
|     - | 1829 | `		 * instance goes back here rather than through the close path. */` |
|     3 | 1830 | `		PH7_ClassInstanceUnref(pObj);` |
|     3 | 1831 | `		pFilter->pObj = 0;` |
|     3 | 1832 | `		FilterDispose(pFilter);` |
|     3 | 1833 | `		return 0;` |
|     - | 1834 | `	}` |
|    23 | 1835 | `	PH7_MemObjRelease(&sRet);` |
|    23 | 1836 | `	return pFilter;` |
|    14 | 1837 | `}` |
|     - | 1838 | `/*` |
|     - | 1839 | ` * bool stream_filter_register(string $filter_name, string $class)` |
|     - | 1840 | ` *  php refuses an empty name or class outright, and answers FALSE for a name` |
|     - | 1841 | ` *  that is already taken rather than replacing it.` |
|     - | 1842 | ` */` |
|    30 | 1843 | `PH7_PRIVATE int PH7_builtin_stream_filter_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1844 | `{` |
|    31 | 1845 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 1846 | `	phl_ufilter_reg *pReg;` |
|     - | 1847 | `	const char *zName,*zClass;` |
|     - | 1848 | `	int nName,nClass;` |
|    15 | 1849 | `	SXUNUSED(nArg);` |
|    31 | 1850 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|    31 | 1851 | `	zClass = ph7_value_to_string(apArg[1],&nClass);` |
|    31 | 1852 | `	if( nName < 1 ){` |
|     4 | 1853 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 1854 | `			"%s(): Argument #1 ($filter_name) must be a non-empty string",` |
|     1 | 1855 | `			ph7_function_name(pCtx));` |
|     - | 1856 | `	}` |
|    29 | 1857 | `	if( nClass < 1 ){` |
|     4 | 1858 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 1859 | `			"%s(): Argument #2 ($class) must be a non-empty string",` |
|     1 | 1860 | `			ph7_function_name(pCtx));` |
|     - | 1861 | `	}` |
|    27 | 1862 | `	if( FilterFindOpsExact(zName,nName) != 0 ){` |
|     - | 1863 | `		/* A name one of the built-ins answers to is taken. */` |
|     3 | 1864 | `		ph7_result_bool(pCtx,0);` |
|     3 | 1865 | `		return PH7_OK;` |
|     - | 1866 | `	}` |
|   135 | 1867 | `	for( pReg = (phl_ufilter_reg *)pVm->pUserFilters ; pReg ; pReg = pReg->pNext ){` |
|   112 | 1868 | `		if( (int)SyBlobLength(&pReg->sName) == nName` |
|    69 | 1869 | `		 && SyMemcmp(SyBlobData(&pReg->sName),zName,(sxu32)nName) == 0 ){` |
|     3 | 1870 | `			ph7_result_bool(pCtx,0);` |
|     3 | 1871 | `			return PH7_OK;` |
|     - | 1872 | `		}` |
|    56 | 1873 | `	}` |
|    23 | 1874 | `	pReg = (phl_ufilter_reg *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_ufilter_reg));` |
|    23 | 1875 | `	if( pReg == 0 ){` |
|   ! 0 | 1876 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 1877 | `	}` |
|    23 | 1878 | `	SyZero(pReg,sizeof(*pReg));` |
|    23 | 1879 | `	SyBlobInit(&pReg->sName,&pVm->sAllocator);` |
|    23 | 1880 | `	SyBlobInit(&pReg->sClass,&pVm->sAllocator);` |
|    23 | 1881 | `	SyBlobAppend(&pReg->sName,zName,(sxu32)nName);` |
|    23 | 1882 | `	SyBlobAppend(&pReg->sClass,zClass,(sxu32)nClass);` |
|    23 | 1883 | `	pReg->pNext = (phl_ufilter_reg *)pVm->pUserFilters;` |
|    23 | 1884 | `	pVm->pUserFilters = (void *)pReg;` |
|    23 | 1885 | `	ph7_result_bool(pCtx,1);` |
|    23 | 1886 | `	return PH7_OK;` |
|    16 | 1887 | `}` |
|     - | 1888 | `/*` |
|     - | 1889 | ` * ?StreamBucket stream_bucket_make_writeable(resource $brigade)` |
|     - | 1890 | ` *  Take the next bucket off the brigade, as an object the script owns.` |
|     - | 1891 | ` */` |
|    54 | 1892 | `PH7_PRIVATE int PH7_builtin_stream_bucket_make_writeable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1893 | `{` |
|     - | 1894 | `	phl_brigade_res *pRes;` |
|     - | 1895 | `	phl_bucket *pBucket;` |
|     - | 1896 | `	ph7_class_instance *pObj;` |
|    27 | 1897 | `	SXUNUSED(nArg);` |
|    55 | 1898 | `	pRes = UserBrigadeFromValue(apArg[0]);` |
|    55 | 1899 | `	if( pRes == 0 ){` |
|   ! 0 | 1900 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 1901 | `			"%s(): Argument #1 ($brigade) must be of type resource, %s given",` |
|   ! 0 | 1902 | `			ph7_function_name(pCtx),ph7_type_name(apArg[0]));` |
|     - | 1903 | `	}` |
|    55 | 1904 | `	pBucket = pRes->pBrig ? FilterBucketPop(pRes->pBrig) : 0;` |
|    55 | 1905 | `	if( pBucket == 0 ){` |
|    37 | 1906 | `		ph7_result_null(pCtx);` |
|    37 | 1907 | `		return PH7_OK;` |
|     - | 1908 | `	}` |
|    28 | 1909 | `	pObj = UserBucketObject(pRes->pVm,(const char *)SyBlobData(&pBucket->sData),` |
|    18 | 1910 | `		(int)SyBlobLength(&pBucket->sData));` |
|    19 | 1911 | `	PH7_FilterBucketFree(pRes->pVm,pBucket);` |
|    19 | 1912 | `	if( pObj == 0 ){` |
|   ! 0 | 1913 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 1914 | `	}` |
|    19 | 1915 | `	PH7_NativeResultObject(pCtx,pObj);` |
|    19 | 1916 | `	return PH7_OK;` |
|    28 | 1917 | `}` |
|     - | 1918 | `/* The two that put one back, differing only in WHICH end. */` |
|    20 | 1919 | `static int UserBucketPut(ph7_context *pCtx,ph7_value **apArg,int bPrepend)` |
|     1 | 1920 | `{` |
|     - | 1921 | `	phl_brigade_res *pRes;` |
|     - | 1922 | `	ph7_class_instance *pObj;` |
|     - | 1923 | `	phl_bucket *pBucket;` |
|    21 | 1924 | `	const char *zData = "";` |
|    21 | 1925 | `	int nData = 0;` |
|    21 | 1926 | `	pRes = UserBrigadeFromValue(apArg[0]);` |
|    21 | 1927 | `	if( pRes == 0 ){` |
|   ! 0 | 1928 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 1929 | `			"%s(): Argument #1 ($brigade) must be of type resource, %s given",` |
|   ! 0 | 1930 | `			ph7_function_name(pCtx),ph7_type_name(apArg[0]));` |
|     - | 1931 | `	}` |
|    21 | 1932 | `	if( !ph7_value_is_object(apArg[1]) ){` |
|   ! 0 | 1933 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 1934 | `			"%s(): Argument #2 ($bucket) must be of type object, %s given",` |
|   ! 0 | 1935 | `			ph7_function_name(pCtx),ph7_type_name(apArg[1]));` |
|     - | 1936 | `	}` |
|    21 | 1937 | `	pObj = (ph7_class_instance *)apArg[1]->x.pOther;` |
|     - | 1938 | `	/* The bytes are whatever the object holds NOW: a filter that replaced` |
|     - | 1939 | ``	 * `$bucket->data` outright is the ordinary way to write one. */`` |
|    21 | 1940 | `	PH7_NativeAttrStr(pObj,"data",&zData,&nData);` |
|    21 | 1941 | `	if( pRes->pBrig == 0 ){` |
|     - | 1942 | `		/* A handle kept past the call it belonged to: there is nothing to put` |
|     - | 1943 | `		 * it back into. */` |
|   ! 0 | 1944 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1945 | `		return PH7_OK;` |
|     - | 1946 | `	}` |
|    21 | 1947 | `	pBucket = PH7_FilterBucketNew(pRes->pVm,zData,(sxu32)nData);` |
|    21 | 1948 | `	if( pBucket == 0 ){` |
|   ! 0 | 1949 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 1950 | `	}` |
|    21 | 1951 | `	if( bPrepend ){` |
|     3 | 1952 | `		pBucket->pNext = pRes->pBrig->pHead;` |
|     3 | 1953 | `		pRes->pBrig->pHead = pBucket;` |
|     3 | 1954 | `		if( pRes->pBrig->pTail == 0 ){` |
|   ! 0 | 1955 | `			pRes->pBrig->pTail = pBucket;` |
|   ! 0 | 1956 | `		}` |
|     2 | 1957 | `	}else{` |
|    19 | 1958 | `		PH7_FilterBucketAppend(pRes->pBrig,pBucket);` |
|     - | 1959 | `	}` |
|    21 | 1960 | `	ph7_result_null(pCtx);` |
|    21 | 1961 | `	return PH7_OK;` |
|    11 | 1962 | `}` |
|    18 | 1963 | `PH7_PRIVATE int PH7_builtin_stream_bucket_append(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1964 | `{` |
|     9 | 1965 | `	SXUNUSED(nArg);` |
|    19 | 1966 | `	return UserBucketPut(pCtx,apArg,0);` |
|     1 | 1967 | `}` |
|     2 | 1968 | `PH7_PRIVATE int PH7_builtin_stream_bucket_prepend(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1969 | `{` |
|     1 | 1970 | `	SXUNUSED(nArg);` |
|     3 | 1971 | `	return UserBucketPut(pCtx,apArg,1);` |
|     1 | 1972 | `}` |
|     - | 1973 | `/*` |
|     - | 1974 | ` * StreamBucket stream_bucket_new(resource $stream, string $buffer)` |
|     - | 1975 | ` *  A bucket of the filter's own making — the only way to emit a TAIL, since the` |
|     - | 1976 | ` *  closing call arrives with an empty brigade.` |
|     - | 1977 | ` */` |
|     6 | 1978 | `PH7_PRIVATE int PH7_builtin_stream_bucket_new(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1979 | `{` |
|     - | 1980 | `	ph7_class_instance *pObj;` |
|     - | 1981 | `	const char *zData;` |
|     - | 1982 | `	int nData;` |
|     3 | 1983 | `	SXUNUSED(nArg);` |
|     7 | 1984 | `	zData = ph7_value_to_string(apArg[1],&nData);` |
|     7 | 1985 | `	pObj = UserBucketObject(pCtx->pVm,zData,nData);` |
|     7 | 1986 | `	if( pObj == 0 ){` |
|   ! 0 | 1987 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 1988 | `	}` |
|     7 | 1989 | `	PH7_NativeResultObject(pCtx,pObj);` |
|     7 | 1990 | `	return PH7_OK;` |
|     4 | 1991 | `}` |
|     - | 1992 | `/*` |
|     - | 1993 | ` * php_user_filter and StreamBucket. The three methods are the ones a filter` |
|     - | 1994 | ` * OVERRIDES; their bodies here are php's own do-nothing defaults, and a class` |
|     - | 1995 | ` * that overrides none of them is a filter that refuses every read — which is` |
|     - | 1996 | ` * what php answers too.` |
|     - | 1997 | ` */` |
|     2 | 1998 | `static int vm_builtin_user_filter_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1999 | `{` |
|     1 | 2000 | `	SXUNUSED(nArg);` |
|     1 | 2001 | `	SXUNUSED(apArg);` |
|     3 | 2002 | `	ph7_result_int(pCtx,PHL_PSFS_ERR_FATAL);` |
|     3 | 2003 | `	return PH7_OK;` |
|     1 | 2004 | `}` |
|    18 | 2005 | `static int vm_builtin_user_filter_onCreate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2006 | `{` |
|     9 | 2007 | `	SXUNUSED(nArg);` |
|     9 | 2008 | `	SXUNUSED(apArg);` |
|    19 | 2009 | `	ph7_result_bool(pCtx,1);` |
|    19 | 2010 | `	return PH7_OK;` |
|     1 | 2011 | `}` |
|    18 | 2012 | `static int vm_builtin_user_filter_onClose(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2013 | `{` |
|     9 | 2014 | `	SXUNUSED(nArg);` |
|     9 | 2015 | `	SXUNUSED(apArg);` |
|    19 | 2016 | `	ph7_result_null(pCtx);` |
|    19 | 2017 | `	return PH7_OK;` |
|     1 | 2018 | `}` |
|  5146 | 2019 | `PH7_PRIVATE sxi32 PH7_VmInstallStreamFilter(ph7_vm *pVm)` |
|     5 | 2020 | `{` |
|     - | 2021 | `	static const PH7_NativeMethodDef aFilterMethod[] = {` |
|     - | 2022 | `		{ "filter", PH7_MOD_PUBLIC, "$in, $out, &$consumed, bool $closing", "int",` |
|     - | 2023 | `		  vm_builtin_user_filter_filter },` |
|     - | 2024 | `		{ "onCreate", PH7_MOD_PUBLIC, "", "bool", vm_builtin_user_filter_onCreate },` |
|     - | 2025 | `		{ "onClose", PH7_MOD_PUBLIC, "", "void", vm_builtin_user_filter_onClose },` |
|     - | 2026 | `	};` |
|     - | 2027 | `	static const PH7_NativePropDef aFilterProp[] = {` |
|     - | 2028 | `		{ "filtername", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, "string" },` |
|     - | 2029 | `		{ "params", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },` |
|     - | 2030 | `		{ "stream", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - | 2031 | `	};` |
|     - | 2032 | `	static const PH7_NativePropDef aBucketProp[] = {` |
|     - | 2033 | `		{ "bucket", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - | 2034 | `		{ "data", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, "string" },` |
|     - | 2035 | `		{ "datalen", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, "int" },` |
|     - | 2036 | `		{ "dataLength", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, "int" },` |
|     - | 2037 | `	};` |
|     - | 2038 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - | 2039 | `		{ "php_user_filter", 0, 0, 0,` |
|     - | 2040 | `		  aFilterMethod, SX_ARRAYSIZE(aFilterMethod), 0, 0,` |
|     - | 2041 | `		  aFilterProp, SX_ARRAYSIZE(aFilterProp), 0, 0, 0 },` |
|     - | 2042 | `		{ "StreamBucket", 0, 0, PH7_CLASS_FINAL,` |
|     - | 2043 | `		  0, 0, 0, 0,` |
|     - | 2044 | `		  aBucketProp, SX_ARRAYSIZE(aBucketProp), UserBucketRelease, 0, 0 },` |
|     - | 2045 | `	};` |
|  5151 | 2046 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|     5 | 2047 | `}` |
|     - | 2048 | `#endif /* PH7_DISABLE_DISK_IO */` |
|     - | 2049 |  |
