# src/ph7/vfs_filter.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1239/1378 lines (89.91%)

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
|  1918 |   34 | `PH7_PRIVATE phl_bucket * PH7_FilterBucketNew(ph7_vm *pVm,const void *pData,sxu32 nLen)` |
|     4 |   35 | `{` |
|     - |   36 | `	phl_bucket *pBucket;` |
|  1922 |   37 | `	if( pVm == 0 ){` |
|   ! 0 |   38 | `		return 0;` |
|     - |   39 | `	}` |
|  1922 |   40 | `	pBucket = (phl_bucket *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_bucket));` |
|  1922 |   41 | `	if( pBucket == 0 ){` |
|   ! 0 |   42 | `		return 0;` |
|     - |   43 | `	}` |
|  1922 |   44 | `	SyZero(pBucket,sizeof(phl_bucket));` |
|  1922 |   45 | `	SyBlobInit(&pBucket->sData,&pVm->sAllocator);` |
|  1922 |   46 | `	if( nLen > 0 && pData != 0 ){` |
|  1922 |   47 | `		if( SyBlobAppend(&pBucket->sData,pData,nLen) != SXRET_OK ){` |
|   ! 0 |   48 | `			SyBlobRelease(&pBucket->sData);` |
|   ! 0 |   49 | `			SyMemBackendFree(&pVm->sAllocator,pBucket);` |
|   ! 0 |   50 | `			return 0;` |
|     - |   51 | `		}` |
|   959 |   52 | `	}` |
|  1922 |   53 | `	return pBucket;` |
|   963 |   54 | `}` |
|  1986 |   55 | `PH7_PRIVATE void PH7_FilterBucketAppend(phl_brigade *pBrig,phl_bucket *pBucket)` |
|     4 |   56 | `{` |
|  1990 |   57 | `	if( pBucket == 0 ){` |
|   ! 0 |   58 | `		return;` |
|     - |   59 | `	}` |
|  1990 |   60 | `	pBucket->pNext = 0;` |
|  1990 |   61 | `	if( pBrig->pTail ){` |
|   ! 0 |   62 | `		pBrig->pTail->pNext = pBucket;` |
|   ! 0 |   63 | `	}else{` |
|  1990 |   64 | `		pBrig->pHead = pBucket;` |
|     - |   65 | `	}` |
|  1990 |   66 | `	pBrig->pTail = pBucket;` |
|   997 |   67 | `}` |
|  1918 |   68 | `PH7_PRIVATE void PH7_FilterBucketFree(ph7_vm *pVm,phl_bucket *pBucket)` |
|     4 |   69 | `{` |
|  1922 |   70 | `	if( pBucket == 0 ){` |
|   ! 0 |   71 | `		return;` |
|     - |   72 | `	}` |
|  1922 |   73 | `	SyBlobRelease(&pBucket->sData);` |
|  1922 |   74 | `	SyMemBackendFree(&pVm->sAllocator,pBucket);` |
|   963 |   75 | `}` |
|     - |   76 | `/* Unlink and answer the first bucket of a brigade, or 0 when it is empty. */` |
| 59304 |   77 | `PH7_PRIVATE phl_bucket * PH7_FilterBucketPop(phl_brigade *pBrig)` |
|     4 |   78 | `{` |
| 59308 |   79 | `	phl_bucket *pBucket = pBrig->pHead;` |
| 59308 |   80 | `	if( pBucket == 0 ){` |
| 57320 |   81 | `		return 0;` |
|     - |   82 | `	}` |
|  1992 |   83 | `	pBrig->pHead = pBucket->pNext;` |
|  1992 |   84 | `	if( pBrig->pHead == 0 ){` |
|  1990 |   85 | `		pBrig->pTail = 0;` |
|   993 |   86 | `	}` |
|  1992 |   87 | `	pBucket->pNext = 0;` |
|  1992 |   88 | `	return pBucket;` |
| 29656 |   89 | `}` |
| 34390 |   90 | `PH7_PRIVATE void PH7_FilterBrigadeRelease(ph7_vm *pVm,phl_brigade *pBrig)` |
|     4 |   91 | `{` |
|     - |   92 | `	phl_bucket *pBucket;` |
| 34398 |   93 | `	while( (pBucket = PH7_FilterBucketPop(pBrig)) != 0 ){` |
|     5 |   94 | `		PH7_FilterBucketFree(pVm,pBucket);` |
|     1 |   95 | `	}` |
| 34394 |   96 | `}` |
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
| 10134 |  118 | `static int StringFilterRun(phl_stream_filter *pFilter,phl_brigade *pIn,phl_brigade *pOut,` |
|     - |  119 | `	int iFlags,int iMode)` |
|     4 |  120 | `{` |
|     - |  121 | `	phl_bucket *pBucket;` |
|  5067 |  122 | `	SXUNUSED(iFlags);` |
|  5067 |  123 | `	SXUNUSED(pFilter);` |
| 10208 |  124 | `	while( (pBucket = PH7_FilterBucketPop(pIn)) != 0 ){` |
|    74 |  125 | `		unsigned char *zData = (unsigned char *)SyBlobData(&pBucket->sData);` |
|    74 |  126 | `		sxu32 n,nLen = SyBlobLength(&pBucket->sData);` |
|   682 |  127 | `		for( n = 0 ; n < nLen ; n++ ){` |
|   612 |  128 | `			int c = zData[n];` |
|   612 |  129 | `			if( iMode == PHL_STRF_TOUPPER ){` |
|   229 |  130 | `				if( c >= 'a' && c <= 'z' ){` |
|   215 |  131 | `					c -= 32;` |
|   109 |  132 | `				}` |
|   497 |  133 | `			}else if( iMode == PHL_STRF_TOLOWER ){` |
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
|   612 |  144 | `			zData[n] = (unsigned char)c;` |
|   308 |  145 | `		}` |
|    74 |  146 | `		PH7_FilterBucketAppend(pOut,pBucket);` |
|     4 |  147 | `	}` |
| 10138 |  148 | `	return PHL_PSFS_PASS_ON;` |
|     4 |  149 | `}` |
|    42 |  150 | `static int Rot13Filter(phl_stream_filter *pF,phl_brigade *pIn,phl_brigade *pOut,int iFlags)` |
|     1 |  151 | `{` |
|    43 |  152 | `	return StringFilterRun(pF,pIn,pOut,iFlags,PHL_STRF_ROT13);` |
|     1 |  153 | `}` |
| 10076 |  154 | `static int ToUpperFilter(phl_stream_filter *pF,phl_brigade *pIn,phl_brigade *pOut,int iFlags)` |
|     3 |  155 | `{` |
| 10079 |  156 | `	return StringFilterRun(pF,pIn,pOut,iFlags,PHL_STRF_TOUPPER);` |
|     3 |  157 | `}` |
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
|  2419 |  608 | `	while( (pBucket = PH7_FilterBucketPop(pIn)) != 0 ){` |
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
|    37 |  747 | `	while( (pBucket = PH7_FilterBucketPop(pIn)) != 0 ){` |
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
|     - |  851 | ` * php's own list carries one more this build has no engine for —` |
|     - |  852 | ``  * `convert.iconv.*` — and one it has no explicable behaviour for: `consumed` `` |
|     - |  853 | ` * passes every byte through (a userland filter placed after it receives them` |
|     - |  854 | `` * all) and yet php answers "" to `fgets()`, to `fread($h,100)` and to`` |
|     - |  855 | `` * `stream_get_contents()` while answering `fread($h,3)` correctly. It exists`` |
|     - |  856 | ` * for php://input's own bookkeeping; a name whose answer depends on WHICH` |
|     - |  857 | ` * reader asked is not one to reproduce, so it is left out rather than guessed.` |
|     - |  858 | ` */` |
|     - |  859 | `static const phl_filter_ops aBuiltinFilters[] = {` |
|     - |  860 | `#ifdef PH7_ENABLE_ZLIB` |
|     - |  861 | `	/* php registers this one FIRST, which is where stream_get_filters() lists` |
|     - |  862 | `	 * it. Its body is ext/zlib's own (builtin_zlib.c). */` |
|     - |  863 | `	{ "zlib.*",         PH7_ZlibFilterCreate, PH7_ZlibFilterRun, PH7_ZlibFilterClose },` |
|     - |  864 | `#endif` |
|     - |  865 | `	{ "string.rot13",   0, Rot13Filter,   0 },` |
|     - |  866 | `	{ "string.toupper", 0, ToUpperFilter, 0 },` |
|     - |  867 | `	{ "string.tolower", 0, ToLowerFilter, 0 },` |
|     - |  868 | `	{ "convert.*",      ConvFilterCreate, ConvFilter, ConvFilterClose },` |
|     - |  869 | `	{ "dechunk",        DechunkCreate,    DechunkFilter, DechunkClose },` |
|     - |  870 | `};` |
|     - |  871 | `/*` |
|     - |  872 | ` * Locate the ops behind a filter NAME. php tries the exact name first, then` |
|     - |  873 | `` * replaces everything after each trailing `.` with `*` and tries again, so`` |
|     - |  874 | `` * `convert.iconv.utf-8/utf-16` finds `convert.iconv.*` and then `convert.*`.`` |
|     - |  875 | `` * The comparison is case SENSITIVE: php answers `Unable to locate filter` for`` |
|     - |  876 | `` * `STRING.ROT13`.`` |
|     - |  877 | ` */` |
|    28 |  878 | `static const phl_filter_ops * FilterFindOpsExact(const char *zName,int nName)` |
|     2 |  879 | `{` |
|     - |  880 | `	sxu32 n;` |
|   190 |  881 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltinFilters) ; ++n ){` |
|   164 |  882 | `		const char *zCur = aBuiltinFilters[n].zName;` |
|   164 |  883 | `		if( (int)SyStrlen(zCur) == nName && SyMemcmp(zCur,zName,(sxu32)nName) == 0 ){` |
|     3 |  884 | `			return &aBuiltinFilters[n];` |
|     - |  885 | `		}` |
|    82 |  886 | `	}` |
|    28 |  887 | `	return 0;` |
|    16 |  888 | `}` |
| 10250 |  889 | `static const phl_filter_ops * FilterFindOps(const char *zName,int nName)` |
|     4 |  890 | `{` |
|     - |  891 | `	char zWild[128];` |
|     - |  892 | `	sxu32 n;` |
|     - |  893 | `	int nTry;` |
| 31424 |  894 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltinFilters) ; ++n ){` |
| 31262 |  895 | `		const char *zCur = aBuiltinFilters[n].zName;` |
| 31262 |  896 | `		if( (int)SyStrlen(zCur) == nName && SyMemcmp(zCur,zName,(sxu32)nName) == 0 ){` |
| 10092 |  897 | `			return &aBuiltinFilters[n];` |
|     - |  898 | `		}` |
| 10589 |  899 | `	}` |
|   165 |  900 | `	nTry = nName;` |
|   103 |  901 | `	for(;;){` |
|     - |  902 | `		/* Strip back to (and including) the last period still inside the prefix. */` |
|  2315 |  903 | `		while( nTry > 0 && zName[nTry-1] != '.' ){` |
|  2109 |  904 | `			nTry--;` |
|     3 |  905 | `		}` |
|   209 |  906 | `		if( nTry < 1 ){` |
|    42 |  907 | `			break;` |
|     - |  908 | `		}` |
|   169 |  909 | `		if( nTry + 1 < (int)sizeof(zWild) ){` |
|   169 |  910 | `			SyMemcpy(zName,zWild,(sxu32)nTry);` |
|   169 |  911 | `			zWild[nTry] = '*';` |
|   817 |  912 | `			for( n = 0 ; n < SX_ARRAYSIZE(aBuiltinFilters) ; ++n ){` |
|   773 |  913 | `				const char *zCur = aBuiltinFilters[n].zName;` |
|   773 |  914 | `				if( (int)SyStrlen(zCur) == nTry + 1 && SyMemcmp(zCur,zWild,(sxu32)(nTry+1)) == 0 ){` |
|   124 |  915 | `					return &aBuiltinFilters[n];` |
|     - |  916 | `				}` |
|   327 |  917 | `			}` |
|    22 |  918 | `		}` |
|    47 |  919 | `		nTry--; /* step past the period we just matched on */` |
|     3 |  920 | `	}` |
|    42 |  921 | `	return 0;` |
|  5129 |  922 | `}` |
|     - |  923 | `/* --------------------------------------------------------------------------` |
|     - |  924 | ` * Filter instances.` |
|     - |  925 | ` * -------------------------------------------------------------------------- */` |
| 10016 |  926 | `PH7_PRIVATE phl_stream_filter * PH7_StreamFilterFromValue(ph7_value *pVal)` |
|     2 |  927 | `{` |
|     - |  928 | `	phl_stream_filter *pFilter;` |
| 10018 |  929 | `	if( pVal == 0 \|\| !ph7_value_is_resource(pVal) ){` |
|   ! 0 |  930 | `		return 0;` |
|     - |  931 | `	}` |
| 10018 |  932 | `	pFilter = (phl_stream_filter *)ph7_value_to_resource(pVal);` |
| 10018 |  933 | `	if( pFilter == 0 \|\| pFilter->base.iMagic != STREAM_FILTER_MAGIC ){` |
|     5 |  934 | `		return 0;` |
|     - |  935 | `	}` |
| 10014 |  936 | `	return pFilter;` |
|  5010 |  937 | `}` |
|     - |  938 | `/* Allocate one filter, chained on the VM registry so it goes back at reset. */` |
| 10236 |  939 | `static phl_stream_filter * FilterNew(ph7_vm *pVm,const phl_filter_ops *pOps,` |
|     - |  940 | `	const char *zName,int nName)` |
|     4 |  941 | `{` |
|     - |  942 | `	phl_stream_filter *pFilter;` |
| 10240 |  943 | `	pFilter = (phl_stream_filter *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_stream_filter));` |
| 10240 |  944 | `	if( pFilter == 0 ){` |
|   ! 0 |  945 | `		return 0;` |
|     - |  946 | `	}` |
| 10240 |  947 | `	SyZero(pFilter,sizeof(phl_stream_filter));` |
| 10240 |  948 | `	pFilter->base.iHead = IO_PRIVATE_HEAD_MAGIC;` |
| 10240 |  949 | `	pFilter->base.iMagic = STREAM_FILTER_MAGIC;` |
|     - |  950 | `	/* The two brigade handles are part of this allocation, so a value naming one` |
|     - |  951 | `	 * of them keeps the whole filter alive; they are counted from here rather` |
|     - |  952 | `	 * than from the call that hands them out. */` |
| 10240 |  953 | `	pFilter->sIn.base.iHead = IO_PRIVATE_HEAD_MAGIC;` |
| 10240 |  954 | `	pFilter->sOut.base.iHead = IO_PRIVATE_HEAD_MAGIC;` |
| 10240 |  955 | `	pFilter->sIn.pOwner = pFilter;` |
| 10240 |  956 | `	pFilter->sOut.pOwner = pFilter;` |
| 10240 |  957 | `	pFilter->pVm = pVm;` |
| 10240 |  958 | `	pFilter->pOps = pOps;` |
| 10240 |  959 | `	SyBlobInit(&pFilter->sName,&pVm->sAllocator);` |
| 10240 |  960 | `	SyBlobInit(&pFilter->sCarry,&pVm->sAllocator);` |
| 10240 |  961 | `	if( nName > 0 ){` |
| 10240 |  962 | `		SyBlobAppend(&pFilter->sName,zName,(sxu32)nName);` |
|  5118 |  963 | `	}` |
| 10240 |  964 | `	pFilter->pRegNext = (phl_stream_filter *)pVm->pStreamFilter;` |
| 10240 |  965 | `	pVm->pStreamFilter = (void *)pFilter;` |
| 10240 |  966 | `	return pFilter;` |
|  5122 |  967 | `}` |
|     - |  968 | `/*` |
|     - |  969 | ` * Release one filter's own resources. The instance itself stays allocated while` |
|     - |  970 | ` * a ph7_value still names it — a probe of that value has to stay in bounds — so` |
|     - |  971 | `` * the magic becomes the CLOSED one, which is what makes `is_resource($f)` false`` |
|     - |  972 | ` * after stream_filter_remove() exactly as php reports it. Once the last value` |
|     - |  973 | ` * lets go, FilterMaybeFree() hands the memory back.` |
|     - |  974 | ` */` |
| 10236 |  975 | `static void FilterDispose(phl_stream_filter *pFilter)` |
|     4 |  976 | `{` |
| 10240 |  977 | `	if( pFilter->pOps && pFilter->pOps->xClose ){` |
|   163 |  978 | `		pFilter->pOps->xClose(pFilter);` |
|    80 |  979 | `	}` |
| 10240 |  980 | `	SyBlobRelease(&pFilter->sCarry);` |
| 10240 |  981 | `	pFilter->pDev = 0;` |
| 10240 |  982 | `	pFilter->pNext = 0;` |
| 10240 |  983 | `	pFilter->base.iMagic = STREAM_FILTER_CLOSED_MAGIC;` |
| 10240 |  984 | `}` |
|     - |  985 | `/*` |
|     - |  986 | ` * A disposed filter's memory goes back the moment nothing can still reach it:` |
|     - |  987 | ` * it has left the chain that owned it, no value names its own handle, and no` |
|     - |  988 | ` * value names either brigade handle living inside the same allocation. php has` |
|     - |  989 | ` * no separate step here — its filter resource is refcounted and the last holder` |
|     - |  990 | ` * frees it — where this engine used to park every removed filter on the VM` |
|     - |  991 | ` * registry until reset, so a loop of append/remove pairs grew without bound.` |
|     - |  992 | ` */` |
| 20476 |  993 | `static void FilterMaybeFree(phl_stream_filter *pFilter)` |
|     4 |  994 | `{` |
|     - |  995 | `	phl_stream_filter **ppSlot;` |
| 20480 |  996 | `	ph7_vm *pVm = pFilter->pVm;` |
| 20480 |  997 | `	if( pFilter->base.iMagic != STREAM_FILTER_CLOSED_MAGIC \|\| pFilter->pDev != 0 ){` |
|     - |  998 | `		/* Still live, or still on a chain: the chain is an owner of its own. */` |
|   125 |  999 | `		return;` |
|     - | 1000 | `	}` |
| 20354 | 1001 | `	if( pFilter->base.nValRef > 0` |
| 15289 | 1002 | `	 \|\| pFilter->sIn.base.nValRef > 0 \|\| pFilter->sOut.base.nValRef > 0 ){` |
| 10146 | 1003 | `		return;` |
|     - | 1004 | `	}` |
|     - | 1005 | `	/* Off the registry first: the reset walk must never meet a freed link. */` |
| 20217 | 1006 | `	for( ppSlot = (phl_stream_filter **)&pVm->pStreamFilter ; *ppSlot ;` |
| 10004 | 1007 | `	     ppSlot = &(*ppSlot)->pRegNext ){` |
| 20217 | 1008 | `		if( *ppSlot == pFilter ){` |
| 10215 | 1009 | `			*ppSlot = pFilter->pRegNext;` |
| 10215 | 1010 | `			break;` |
|     - | 1011 | `		}` |
|  5003 | 1012 | `	}` |
| 10215 | 1013 | `	SyBlobRelease(&pFilter->sName);` |
| 10215 | 1014 | `	pFilter->base.iHead = 0;` |
| 10215 | 1015 | `	pFilter->base.iMagic = 0;` |
| 10215 | 1016 | `	pFilter->sIn.base.iHead = pFilter->sOut.base.iHead = 0;` |
| 10215 | 1017 | `	pFilter->sIn.base.iMagic = pFilter->sOut.base.iMagic = 0;` |
| 10215 | 1018 | `	SyMemBackendFree(&pVm->sAllocator,pFilter);` |
| 10242 | 1019 | `}` |
|     - | 1020 | `/*` |
|     - | 1021 | ` * The value doors call in here through PH7_StreamValueUnref() when the last` |
|     - | 1022 | ` * ph7_value naming a filter -- or one of its brigade handles -- goes away.` |
|     - | 1023 | ` */` |
| 10242 | 1024 | `PH7_PRIVATE void PH7_StreamFilterValueGone(void *pResource)` |
|     3 | 1025 | `{` |
| 10245 | 1026 | `	io_private *pHead = (io_private *)pResource;` |
| 10245 | 1027 | `	if( pHead->iMagic == STREAM_BRIGADE_MAGIC ){` |
|    84 | 1028 | `		phl_brigade_res *pRes = (phl_brigade_res *)pHead;` |
|    84 | 1029 | `		if( pRes->pOwner ){` |
|    84 | 1030 | `			FilterMaybeFree(pRes->pOwner);` |
|    41 | 1031 | `		}` |
|    84 | 1032 | `		return;` |
|     - | 1033 | `	}` |
| 10163 | 1034 | `	FilterMaybeFree((phl_stream_filter *)pHead);` |
|  5124 | 1035 | `}` |
|     - | 1036 | `/* --------------------------------------------------------------------------` |
|     - | 1037 | ` * Running a chain.` |
|     - | 1038 | ` * -------------------------------------------------------------------------- */` |
|     - | 1039 | `/* One filter's turn. Built-in ops run their routine; the userland half hooks in` |
|     - | 1040 | ` * here when it lands. */` |
| 11482 | 1041 | `static int FilterInvoke(phl_stream_filter *pFilter,phl_brigade *pIn,phl_brigade *pOut,int iFlags)` |
|     4 | 1042 | `{` |
| 11486 | 1043 | `	if( pFilter->pOps == 0 \|\| pFilter->pOps->xFilter == 0 ){` |
|   ! 0 | 1044 | `		return PHL_PSFS_ERR_FATAL;` |
|     - | 1045 | `	}` |
| 11486 | 1046 | `	if( iFlags & PHL_PSFS_FLAG_FLUSH_CLOSE ){` |
| 10202 | 1047 | `		if( pFilter->bClosed ){` |
|     - | 1048 | `			/* A filter gets exactly ONE closing call: the device's end already` |
|     - | 1049 | `			 * made it, and running a buffering codec's tail a second time (a` |
|     - | 1050 | `			 * stream_filter_remove() after the last read, say) would emit that` |
|     - | 1051 | `			 * tail twice. Whatever arrives now simply passes through. */` |
|     - | 1052 | `			phl_bucket *pBucket;` |
|   ! 0 | 1053 | `			while( (pBucket = PH7_FilterBucketPop(pIn)) != 0 ){` |
|   ! 0 | 1054 | `				PH7_FilterBucketAppend(pOut,pBucket);` |
|   ! 0 | 1055 | `			}` |
|   ! 0 | 1056 | `			return PHL_PSFS_PASS_ON;` |
|     - | 1057 | `		}` |
| 10202 | 1058 | `		pFilter->bClosed = 1;` |
|  5099 | 1059 | `	}` |
|     - | 1060 | `	{` |
| 11486 | 1061 | `		int rc = pFilter->pOps->xFilter(pFilter,pIn,pOut,iFlags);` |
| 11486 | 1062 | `		if( rc == PHL_PSFS_ERR_FATAL ){` |
|     - | 1063 | `			/* Marked, not skipped: php runs a filter that has already refused` |
|     - | 1064 | `			 * once again on the next write and reports the refusal again — what` |
|     - | 1065 | `			 * it does NOT do is run it a last time at close. */` |
|     9 | 1066 | `			pFilter->bDead = 1;` |
|     4 | 1067 | `		}` |
| 11486 | 1068 | `		return rc;` |
|     - | 1069 | `	}` |
|  5745 | 1070 | `}` |
|     - | 1071 | `/*` |
|     - | 1072 | ` * iFlags describes the call for the HEAD of the chain and iRestFlags for` |
|     - | 1073 | ` * everything behind it, because the two are not always the same: the device's` |
|     - | 1074 | ` * end of file closes every filter on the stream, but flushing ONE filter — what` |
|     - | 1075 | ` * stream_filter_remove() does — closes only that one and hands its tail to the` |
|     - | 1076 | ` * others as ordinary data. Closing them too would make a codec below emit its` |
|     - | 1077 | `` * own tail early: removing an upstream `string.toupper` from a chain ending in`` |
|     - | 1078 | `` * `convert.base64-encode` padded the base64 there and then, where php leaves it`` |
|     - | 1079 | ` * mid-group.` |
|     - | 1080 | ` */` |
| 11460 | 1081 | `PH7_PRIVATE int PH7_FilterChainProcess(phl_stream_filter *pHead,` |
|     - | 1082 | `	const void *pData,sxu32 nLen,int iFlags,int iRestFlags,SyBlob *pOut,int *pbUnread)` |
|     4 | 1083 | `{` |
| 11464 | 1084 | `	ph7_vm *pVm = pHead->pVm;` |
|     - | 1085 | `	phl_brigade sA,sB;` |
|     - | 1086 | `	phl_brigade *pIn,*pOutBrig,*pSwap;` |
|     - | 1087 | `	phl_stream_filter *pFilter;` |
|     - | 1088 | `	phl_bucket *pBucket;` |
| 11464 | 1089 | `	int iStatus = PHL_PSFS_PASS_ON;` |
| 11464 | 1090 | `	SyZero(&sA,sizeof(sA));` |
| 11464 | 1091 | `	SyZero(&sB,sizeof(sB));` |
| 11464 | 1092 | `	if( nLen > 0 ){` |
|  1276 | 1093 | `		pBucket = PH7_FilterBucketNew(pVm,pData,nLen);` |
|  1276 | 1094 | `		if( pBucket == 0 ){` |
|   ! 0 | 1095 | `			return PHL_PSFS_ERR_FATAL;` |
|     - | 1096 | `		}` |
|  1276 | 1097 | `		PH7_FilterBucketAppend(&sA,pBucket);` |
|   636 | 1098 | `	}` |
| 11464 | 1099 | `	pIn = &sA;` |
| 11464 | 1100 | `	pOutBrig = &sB;` |
| 22934 | 1101 | `	for( pFilter = pHead ; pFilter ; pFilter = pFilter->pNext ){` |
| 11486 | 1102 | `		iStatus = FilterInvoke(pFilter,pIn,pOutBrig,pFilter == pHead ? iFlags : iRestFlags);` |
| 11486 | 1103 | `		if( iStatus != PHL_PSFS_PASS_ON ){` |
|    13 | 1104 | `			break;` |
|     - | 1105 | `		}` |
|     - | 1106 | `		/* Whatever the filter left behind is dropped: php warns about it from` |
|     - | 1107 | `		 * the reader ("Unprocessed filter buckets remaining on input brigade")` |
|     - | 1108 | `		 * and hands the read back as a failure, which is the ERR_FATAL path. */` |
| 11474 | 1109 | `		PH7_FilterBrigadeRelease(pVm,pIn);` |
|     - | 1110 | `		/* This filter's output is the next one's input. */` |
| 11474 | 1111 | `		pSwap = pIn;` |
| 11474 | 1112 | `		pIn = pOutBrig;` |
| 11474 | 1113 | `		pOutBrig = pSwap;` |
|  5739 | 1114 | `	}` |
| 11464 | 1115 | `	if( iStatus != PHL_PSFS_PASS_ON && pIn->pHead != 0 ){` |
|     - | 1116 | `		/* A filter that gave up on its input without taking it: php says so and` |
|     - | 1117 | `		 * the READ answers FALSE rather than an end of file. A filter that` |
|     - | 1118 | `		 * consumed everything and then refused is the quiet shape. */` |
|     5 | 1119 | `		if( pbUnread ){` |
|   ! 0 | 1120 | `			*pbUnread = 1;` |
|   ! 0 | 1121 | `		}` |
|     5 | 1122 | `		PH7_VmThrowError(pVm,pVm->pCalleeName,PH7_CTX_WARNING,` |
|     - | 1123 | `			"Unprocessed filter buckets remaining on input brigade");` |
|     2 | 1124 | `	}` |
| 11464 | 1125 | `	if( iStatus == PHL_PSFS_PASS_ON && pOut ){` |
| 12154 | 1126 | `		while( (pBucket = PH7_FilterBucketPop(pIn)) != 0 ){` |
|   706 | 1127 | `			if( SyBlobLength(&pBucket->sData) > 0 ){` |
|   706 | 1128 | `				SyBlobAppend(pOut,SyBlobData(&pBucket->sData),SyBlobLength(&pBucket->sData));` |
|   351 | 1129 | `			}` |
|   706 | 1130 | `			PH7_FilterBucketFree(pVm,pBucket);` |
|     4 | 1131 | `		}` |
|  5724 | 1132 | `	}` |
| 11464 | 1133 | `	PH7_FilterBrigadeRelease(pVm,&sA);` |
| 11464 | 1134 | `	PH7_FilterBrigadeRelease(pVm,&sB);` |
| 11464 | 1135 | `	return iStatus;` |
|  5734 | 1136 | `}` |
|     - | 1137 | `/* --------------------------------------------------------------------------` |
|     - | 1138 | ` * Attaching, removing and releasing.` |
|     - | 1139 | ` * -------------------------------------------------------------------------- */` |
|     - | 1140 | `/* The chain head slot of a handle for one direction. */` |
| 75194 | 1141 | `static phl_stream_filter ** FilterChainSlot(io_private *pDev,int iChain)` |
|     5 | 1142 | `{` |
| 75199 | 1143 | `	if( iChain == PHL_STREAM_FILTER_WRITE ){` |
| 47546 | 1144 | `		return (phl_stream_filter **)&pDev->pWriteFilters;` |
|     - | 1145 | `	}` |
| 27658 | 1146 | `	return (phl_stream_filter **)&pDev->pReadFilters;` |
| 37415 | 1147 | `}` |
|     - | 1148 | `/* Unlink a filter from the chain it sits on. */` |
| 10012 | 1149 | `static void FilterUnlink(phl_stream_filter *pFilter)` |
|     2 | 1150 | `{` |
|     - | 1151 | `	phl_stream_filter **ppSlot,*pCur;` |
| 10014 | 1152 | `	if( pFilter->pDev == 0 ){` |
|   ! 0 | 1153 | `		return;` |
|     - | 1154 | `	}` |
| 10014 | 1155 | `	ppSlot = FilterChainSlot(pFilter->pDev,pFilter->iChain);` |
| 10014 | 1156 | `	pCur = *ppSlot;` |
| 10014 | 1157 | `	if( pCur == pFilter ){` |
| 10014 | 1158 | `		*ppSlot = pFilter->pNext;` |
| 10014 | 1159 | `		return;` |
|     - | 1160 | `	}` |
|   ! 0 | 1161 | `	while( pCur ){` |
|   ! 0 | 1162 | `		if( pCur->pNext == pFilter ){` |
|   ! 0 | 1163 | `			pCur->pNext = pFilter->pNext;` |
|   ! 0 | 1164 | `			return;` |
|     - | 1165 | `		}` |
|   ! 0 | 1166 | `		pCur = pCur->pNext;` |
|   ! 0 | 1167 | `	}` |
|  5008 | 1168 | `}` |
|     - | 1169 | `/*` |
|     - | 1170 | ` * The last call a filter ever gets. A write filter's tail has to reach the` |
|     - | 1171 | ` * device, and a read filter's has to reach the reader, so a flush is a chain` |
|     - | 1172 | ` * run from THIS filter down with no input and the closing flag.` |
|     - | 1173 | ` */` |
| 10048 | 1174 | `static void FilterFlushTail(phl_stream_filter *pFilter,int iRestFlags)` |
|     4 | 1175 | `{` |
| 10052 | 1176 | `	io_private *pDev = pFilter->pDev;` |
|     - | 1177 | `	phl_stream_filter *pCur;` |
|     - | 1178 | `	SyBlob sOut;` |
| 10052 | 1179 | `	if( pDev == 0 ){` |
|   ! 0 | 1180 | `		return;` |
|     - | 1181 | `	}` |
| 20110 | 1182 | `	for( pCur = pFilter ; pCur ; pCur = pCur->pNext ){` |
| 10064 | 1183 | `		if( pCur->bDead ){` |
|     - | 1184 | `			/* A chain that already refused its input is finished: php does not` |
|     - | 1185 | `			 * run it again at close, and running it here would report the same` |
|     - | 1186 | `			 * refusal a second time from fclose(). */` |
|     3 | 1187 | `			return;` |
|     - | 1188 | `		}` |
|  5033 | 1189 | `	}` |
| 10050 | 1190 | `	SyBlobInit(&sOut,&pFilter->pVm->sAllocator);` |
| 10046 | 1191 | `	if( PH7_FilterChainProcess(pFilter,0,0,PHL_PSFS_FLAG_FLUSH_CLOSE,iRestFlags,&sOut,0)` |
| 10050 | 1192 | `	    == PHL_PSFS_PASS_ON && SyBlobLength(&sOut) > 0 ){` |
|    16 | 1193 | `		if( pFilter->iChain == PHL_STREAM_FILTER_WRITE ){` |
|    16 | 1194 | `			if( pDev->pStream && pDev->pStream->xWrite ){` |
|    23 | 1195 | `				pDev->pStream->xWrite(pDev->pHandle,SyBlobData(&sOut),` |
|    14 | 1196 | `					(ph7_int64)SyBlobLength(&sOut));` |
|     7 | 1197 | `			}` |
|     9 | 1198 | `		}else{` |
|   ! 0 | 1199 | `			SyBlobAppend(&pDev->sFilt,SyBlobData(&sOut),SyBlobLength(&sOut));` |
|     - | 1200 | `		}` |
|     7 | 1201 | `	}` |
| 10050 | 1202 | `	SyBlobRelease(&sOut);` |
|  5028 | 1203 | `}` |
| 26781 | 1204 | `PH7_PRIVATE void PH7_StreamFilterReleaseChains(io_private *pDev)` |
|     5 | 1205 | `{` |
|     - | 1206 | `	int i;` |
| 80348 | 1207 | `	for( i = 0 ; i < 2 ; i++ ){` |
| 53567 | 1208 | `		int iChain = i == 0 ? PHL_STREAM_FILTER_WRITE : PHL_STREAM_FILTER_READ;` |
| 53567 | 1209 | `		phl_stream_filter **ppSlot = FilterChainSlot(pDev,iChain);` |
| 53567 | 1210 | `		phl_stream_filter *pFilter = *ppSlot;` |
|     - | 1211 | `		/* The WRITE chain is flushed first and as a whole: the head's tail has` |
|     - | 1212 | `		 * to travel through the filters below it before anything reaches the` |
|     - | 1213 | `		 * device. */` |
| 53567 | 1214 | `		if( iChain == PHL_STREAM_FILTER_WRITE && pFilter ){` |
|    39 | 1215 | `			FilterFlushTail(pFilter,PHL_PSFS_FLAG_FLUSH_CLOSE);` |
|    18 | 1216 | `		}` |
|     - | 1217 | `		/* Emptied before the walk, not after: a filter the walk hands back must` |
|     - | 1218 | `		 * not still be reachable from the handle it was attached to. */` |
| 53567 | 1219 | `		*ppSlot = 0;` |
| 53779 | 1220 | `		while( pFilter ){` |
|   215 | 1221 | `			phl_stream_filter *pNext = pFilter->pNext;` |
|   215 | 1222 | `			FilterDispose(pFilter);` |
|   215 | 1223 | `			FilterMaybeFree(pFilter);` |
|   215 | 1224 | `			pFilter = pNext;` |
|     3 | 1225 | `		}` |
| 26599 | 1226 | `	}` |
| 26786 | 1227 | `}` |
|   698 | 1228 | `PH7_PRIVATE void PH7_StreamFilterRewound(io_private *pDev)` |
|     5 | 1229 | `{` |
|     - | 1230 | `	int i;` |
|  2099 | 1231 | `	for( i = 0 ; i < 2 ; i++ ){` |
|  2099 | 1232 | `		phl_stream_filter *pFilter = *FilterChainSlot(pDev,` |
|   698 | 1233 | `			i == 0 ? PHL_STREAM_FILTER_READ : PHL_STREAM_FILTER_WRITE);` |
|  1429 | 1234 | `		while( pFilter ){` |
|     - | 1235 | `			/* The stream moved, so the end it had reached is not the end any` |
|     - | 1236 | `			 * more: a chain closed at the old one must be able to run — and to` |
|     - | 1237 | `			 * emit its tail — again. */` |
|    29 | 1238 | `			pFilter->bClosed = 0;` |
|    29 | 1239 | `			pFilter = pFilter->pNext;` |
|     1 | 1240 | `		}` |
|   703 | 1241 | `	}` |
|   703 | 1242 | `}` |
|    16 | 1243 | `PH7_PRIVATE void PH7_StreamFilterVmReset(ph7_vm *pVm)` |
|   ! 0 | 1244 | `{` |
|     - | 1245 | `	phl_stream_filter *pFilter;` |
|    16 | 1246 | `	if( pVm == 0 ){` |
|   ! 0 | 1247 | `		return;` |
|     - | 1248 | `	}` |
|    16 | 1249 | `	pFilter = (phl_stream_filter *)pVm->pStreamFilter;` |
|    16 | 1250 | `	while( pFilter ){` |
|   ! 0 | 1251 | `		phl_stream_filter *pNext = pFilter->pRegNext;` |
|   ! 0 | 1252 | `		if( pFilter->base.iMagic == STREAM_FILTER_MAGIC ){` |
|     - | 1253 | `			/* The std handles outlive a reset (the -S server reuses one VM), so` |
|     - | 1254 | `			 * a filter that was never removed has to leave their chain before` |
|     - | 1255 | `			 * its memory goes back — otherwise the next request's first write` |
|     - | 1256 | `			 * walks a freed one. */` |
|   ! 0 | 1257 | `			io_private *pDev = pFilter->pDev;` |
|   ! 0 | 1258 | `			FilterUnlink(pFilter);` |
|   ! 0 | 1259 | `			if( pDev ){` |
|   ! 0 | 1260 | `				SyBlobReset(&pDev->sFilt);` |
|   ! 0 | 1261 | `				pDev->nFiltOfft = 0;` |
|   ! 0 | 1262 | `				pDev->bFiltDone = 0;` |
|   ! 0 | 1263 | `			}` |
|   ! 0 | 1264 | `			FilterDispose(pFilter);` |
|   ! 0 | 1265 | `		}` |
|   ! 0 | 1266 | `		SyBlobRelease(&pFilter->sName);` |
|   ! 0 | 1267 | `		pFilter->base.iHead = 0;` |
|   ! 0 | 1268 | `		pFilter->base.iMagic = 0;` |
|   ! 0 | 1269 | `		SyMemBackendFree(&pVm->sAllocator,pFilter);` |
|   ! 0 | 1270 | `		pFilter = pNext;` |
|   ! 0 | 1271 | `	}` |
|    16 | 1272 | `	pVm->pStreamFilter = 0;` |
|     - | 1273 | `	{` |
|    16 | 1274 | `		phl_ufilter_reg *pReg = (phl_ufilter_reg *)pVm->pUserFilters;` |
|    16 | 1275 | `		while( pReg ){` |
|   ! 0 | 1276 | `			phl_ufilter_reg *pNext = pReg->pNext;` |
|   ! 0 | 1277 | `			SyBlobRelease(&pReg->sName);` |
|   ! 0 | 1278 | `			SyBlobRelease(&pReg->sClass);` |
|   ! 0 | 1279 | `			SyMemBackendFree(&pVm->sAllocator,pReg);` |
|   ! 0 | 1280 | `			pReg = pNext;` |
|   ! 0 | 1281 | `		}` |
|    16 | 1282 | `		pVm->pUserFilters = 0;` |
|     - | 1283 | `	}` |
|    16 | 1284 | `	pVm->pFilterCall = 0;` |
|     8 | 1285 | `}` |
|     - | 1286 | `/* php's own two diagnostics, worded from the builtin that is running — which is` |
|     - | 1287 | `` * `stream_filter_append` on one path and the READER (file_get_contents, fopen)`` |
|     - | 1288 | ` * on the php://filter one. */` |
|    26 | 1289 | `static void FilterWarn(ph7_vm *pVm,const char *zFmt,int nName,const char *zName)` |
|     2 | 1290 | `{` |
|     - | 1291 | `	char zMsg[160];` |
|    28 | 1292 | `	SyBufferFormat(zMsg,sizeof(zMsg),zFmt,nName,zName);` |
|    28 | 1293 | `	PH7_VmThrowError(pVm,pVm->pCalleeName,PH7_CTX_WARNING,zMsg);` |
|    28 | 1294 | `}` |
| 10250 | 1295 | `PH7_PRIVATE phl_stream_filter * PH7_StreamFilterAttach(ph7_vm *pVm,io_private *pDev,` |
|     - | 1296 | `	const char *zName,int nName,int iChain,int bPrepend,ph7_value *pParams,` |
|     - | 1297 | `	ph7_value *pStreamVal)` |
|     4 | 1298 | `{` |
|     - | 1299 | `	const phl_filter_ops *pOps;` |
| 10254 | 1300 | `	phl_ufilter_reg *pReg = 0;` |
|     - | 1301 | `	phl_stream_filter *pFilter;` |
| 10254 | 1302 | `	pOps = FilterFindOps(zName,nName);` |
| 10254 | 1303 | `	if( pOps == 0 ){` |
|     - | 1304 | `		/* Nothing built in answers to it; a script may have registered one. */` |
|    42 | 1305 | `		pReg = UserFilterFind(pVm,zName,nName);` |
|    42 | 1306 | `		if( pReg == 0 ){` |
|    13 | 1307 | `			FilterWarn(pVm,"Unable to locate filter \"%.*s\"",nName,zName);` |
|    13 | 1308 | `			return 0;` |
|     - | 1309 | `		}` |
|    30 | 1310 | `		pFilter = UserFilterCreate(pVm,pReg,zName,nName,pParams,pStreamVal);` |
|    30 | 1311 | `		if( pFilter == 0 ){` |
|     5 | 1312 | `			FilterWarn(pVm,"Unable to create or locate filter \"%.*s\"",nName,zName);` |
|     5 | 1313 | `			return 0;` |
|     - | 1314 | `		}` |
|    26 | 1315 | `		goto attach;` |
|     - | 1316 | `	}` |
| 10214 | 1317 | `	pFilter = FilterNew(pVm,pOps,zName,nName);` |
| 10214 | 1318 | `	if( pFilter == 0 ){` |
|   ! 0 | 1319 | `		PH7_VmThrowError(pVm,pVm->pCalleeName,PH7_CTX_ERR,"PH7 is running out of memory");` |
|   ! 0 | 1320 | `		return 0;` |
|     - | 1321 | `	}` |
| 10214 | 1322 | `	if( pOps->xCreate && pOps->xCreate(pFilter,pParams) != PH7_OK ){` |
|    12 | 1323 | `		FilterDispose(pFilter);` |
|    12 | 1324 | `		FilterMaybeFree(pFilter);` |
|    12 | 1325 | `		FilterWarn(pVm,"Unable to create or locate filter \"%.*s\"",nName,zName);` |
|    12 | 1326 | `		return 0;` |
|     - | 1327 | `	}` |
|  5100 | 1328 | `attach:` |
| 10228 | 1329 | `	pFilter->pDev = pDev;` |
| 10228 | 1330 | `	pFilter->iChain = iChain;` |
| 10228 | 1331 | `	if( bPrepend ){` |
|     3 | 1332 | `		phl_stream_filter **ppSlot = FilterChainSlot(pDev,iChain);` |
|     3 | 1333 | `		pFilter->pNext = *ppSlot;` |
|     3 | 1334 | `		*ppSlot = pFilter;` |
|     2 | 1335 | `	}else{` |
| 10226 | 1336 | `		phl_stream_filter **ppSlot = FilterChainSlot(pDev,iChain);` |
| 10226 | 1337 | `		phl_stream_filter *pCur = *ppSlot;` |
| 10226 | 1338 | `		if( pCur == 0 ){` |
| 10212 | 1339 | `			*ppSlot = pFilter;` |
|  5108 | 1340 | `		}else{` |
|    22 | 1341 | `			while( pCur->pNext ){` |
|     7 | 1342 | `				pCur = pCur->pNext;` |
|     1 | 1343 | `			}` |
|    16 | 1344 | `			pCur->pNext = pFilter;` |
|     - | 1345 | `		}` |
|     - | 1346 | `	}` |
| 10228 | 1347 | `	return pFilter;` |
|  5129 | 1348 | `}` |
|     - | 1349 | `/* --------------------------------------------------------------------------` |
|     - | 1350 | ` * The builtins.` |
|     - | 1351 | ` * -------------------------------------------------------------------------- */` |
|     - | 1352 | `/*` |
|     - | 1353 | ` * resource\|false stream_filter_append(resource $stream, string $filter_name,` |
|     - | 1354 | ` *                                     int $mode = 0, mixed $params = null)` |
|     - | 1355 | ` * resource\|false stream_filter_prepend(...)` |
|     - | 1356 | ` *` |
|     - | 1357 | ` * php's $mode of 0 is not "no chain": it means "whichever chains this handle's` |
|     - | 1358 | `` * MODE makes sense for", so a stream opened `r+` gets the filter on BOTH — two`` |
|     - | 1359 | ` * separate instances, since a filter carries state and one cannot serve two` |
|     - | 1360 | ` * directions. The resource answered is the LAST one created, which is why` |
|     - | 1361 | `` * removing what `stream_filter_append($h,'…')` gave back on an `r+` handle`` |
|     - | 1362 | ` * leaves the READ half of it still filtering.` |
|     - | 1363 | ` */` |
| 10202 | 1364 | `static int StreamFilterAddCommon(ph7_context *pCtx,int nArg,ph7_value **apArg,int bPrepend)` |
|     4 | 1365 | `{` |
| 10206 | 1366 | `	phl_stream_filter *pFilter = 0;` |
|     - | 1367 | `	ph7_value *pParams;` |
|     - | 1368 | `	io_private *pDev;` |
|     - | 1369 | `	const char *zName;` |
|     - | 1370 | `	int nName,iChain,rc;` |
| 10206 | 1371 | `	pDev = PH7_StreamHandleArg(pCtx,apArg[0],1,"stream",&rc);` |
| 10206 | 1372 | `	if( pDev == 0 ){` |
|   ! 0 | 1373 | `		return rc;` |
|     - | 1374 | `	}` |
| 10206 | 1375 | `	zName = ph7_value_to_string(apArg[1],&nName);` |
| 10206 | 1376 | `	iChain = nArg > 2 ? (int)ph7_value_to_int(apArg[2]) : 0;` |
| 10206 | 1377 | `	pParams = nArg > 3 ? apArg[3] : 0;` |
| 10206 | 1378 | `	if( iChain == 0 ){` |
|     - | 1379 | `		/* php reads the mode the handle was OPENED with. */` |
|    27 | 1380 | `		const char *zMode = pDev->zMode;` |
|     - | 1381 | `		sxu32 nDummy;` |
|    27 | 1382 | `		int bPlus = SyByteFind(zMode,SyStrlen(zMode),'+',&nDummy) == SXRET_OK;` |
|    27 | 1383 | `		switch( zMode[0] ){` |
|    10 | 1384 | `		case 'r':` |
|    22 | 1385 | `			iChain = bPlus ? PHL_STREAM_FILTER_ALL : PHL_STREAM_FILTER_READ;` |
|    22 | 1386 | `			break;` |
|     2 | 1387 | `		case 'w':` |
|     - | 1388 | `		case 'a':` |
|     - | 1389 | `		case 'x':` |
|     - | 1390 | `		case 'c':` |
|     5 | 1391 | `			iChain = bPlus ? PHL_STREAM_FILTER_ALL : PHL_STREAM_FILTER_WRITE;` |
|     4 | 1392 | `			break;` |
|   ! 0 | 1393 | `		default:` |
|   ! 0 | 1394 | `			break;` |
|     - | 1395 | `		}` |
|    12 | 1396 | `	}` |
| 10206 | 1397 | `	if( iChain & PHL_STREAM_FILTER_READ ){` |
|   237 | 1398 | `		pFilter = PH7_StreamFilterAttach(pCtx->pVm,pDev,zName,nName,PHL_STREAM_FILTER_READ,` |
|    78 | 1399 | `			bPrepend,pParams,apArg[0]);` |
|   159 | 1400 | `		if( pFilter == 0 ){` |
|    22 | 1401 | `			ph7_result_bool(pCtx,0);` |
|    22 | 1402 | `			return PH7_OK;` |
|     - | 1403 | `		}` |
|    68 | 1404 | `	}` |
| 10186 | 1405 | `	if( iChain & PHL_STREAM_FILTER_WRITE ){` |
| 15079 | 1406 | `		pFilter = PH7_StreamFilterAttach(pCtx->pVm,pDev,zName,nName,PHL_STREAM_FILTER_WRITE,` |
|  5025 | 1407 | `			bPrepend,pParams,apArg[0]);` |
| 10054 | 1408 | `		if( pFilter == 0 ){` |
|     3 | 1409 | `			ph7_result_bool(pCtx,0);` |
|     3 | 1410 | `			return PH7_OK;` |
|     - | 1411 | `		}` |
|  5024 | 1412 | `	}` |
| 10184 | 1413 | `	if( pFilter == 0 ){` |
|     - | 1414 | `		/* A mode this engine could not place the filter on. */` |
|   ! 0 | 1415 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1416 | `		return PH7_OK;` |
|     - | 1417 | `	}` |
| 10184 | 1418 | `	ph7_result_resource(pCtx,pFilter);` |
| 10184 | 1419 | `	return PH7_OK;` |
|  5105 | 1420 | `}` |
| 10200 | 1421 | `PH7_PRIVATE int PH7_builtin_stream_filter_append(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     4 | 1422 | `{` |
| 10204 | 1423 | `	return StreamFilterAddCommon(pCtx,nArg,apArg,0);` |
|     4 | 1424 | `}` |
|     2 | 1425 | `PH7_PRIVATE int PH7_builtin_stream_filter_prepend(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1426 | `{` |
|     3 | 1427 | `	return StreamFilterAddCommon(pCtx,nArg,apArg,1);` |
|     1 | 1428 | `}` |
|     - | 1429 | `/*` |
|     - | 1430 | ` * bool stream_filter_remove(resource $stream_filter)` |
|     - | 1431 | ` *` |
|     - | 1432 | ` * php FLUSHES the filter on the way out — a write filter's tail still reaches` |
|     - | 1433 | ` * the device and a read filter's still reaches the reader — and then the` |
|     - | 1434 | ` * resource is dead: passing it again is a TypeError, not FALSE.` |
|     - | 1435 | ` */` |
| 10018 | 1436 | `PH7_PRIVATE int PH7_builtin_stream_filter_remove(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 1437 | `{` |
|     - | 1438 | `	char zGiven[64];` |
|     - | 1439 | `	phl_stream_filter *pFilter;` |
|  5009 | 1440 | `	SXUNUSED(nArg);` |
| 10020 | 1441 | `	if( !ph7_value_is_resource(apArg[0]) ){` |
|     - | 1442 | `		/* php's ZPP runs first: a string is not "the wrong resource", it is not` |
|     - | 1443 | `		 * a resource at all, and the two diagnostics are different. */` |
|     4 | 1444 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 1445 | `			"%s(): Argument #1 ($stream_filter) must be of type resource, %s given",` |
|     1 | 1446 | `			ph7_function_name(pCtx),VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));` |
|     - | 1447 | `	}` |
| 10018 | 1448 | `	pFilter = PH7_StreamFilterFromValue(apArg[0]);` |
| 10018 | 1449 | `	if( pFilter == 0 ){` |
|     7 | 1450 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 1451 | `			"%s(): supplied resource is not a valid stream filter resource",` |
|     2 | 1452 | `			ph7_function_name(pCtx));` |
|     - | 1453 | `	}` |
|     - | 1454 | `	/* Only THIS filter closes; what it emits travels through the rest of the` |
|     - | 1455 | `	 * chain as ordinary data, because those filters stay on the stream. */` |
| 10014 | 1456 | `	FilterFlushTail(pFilter,PHL_PSFS_FLAG_NORMAL);` |
| 10014 | 1457 | `	FilterUnlink(pFilter);` |
| 10014 | 1458 | `	FilterDispose(pFilter);` |
| 10014 | 1459 | `	FilterMaybeFree(pFilter);` |
| 10014 | 1460 | `	ph7_result_bool(pCtx,1);` |
| 10014 | 1461 | `	return PH7_OK;` |
|  5011 | 1462 | `}` |
|     - | 1463 | `/*` |
|     - | 1464 | ` * array stream_get_filters(void)` |
|     - | 1465 | ` *  The filter names this build can create, in php's own registration order.` |
|     - | 1466 | ` */` |
|    10 | 1467 | `PH7_PRIVATE int PH7_builtin_stream_get_filters(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 | 1468 | `{` |
|     - | 1469 | `	ph7_value *pArray,*pValue;` |
|     - | 1470 | `	sxu32 n;` |
|     5 | 1471 | `	SXUNUSED(nArg);` |
|     5 | 1472 | `	SXUNUSED(apArg);` |
|    13 | 1473 | `	pArray = ph7_context_new_array(pCtx);` |
|    13 | 1474 | `	pValue = ph7_context_new_scalar(pCtx);` |
|    13 | 1475 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|   ! 0 | 1476 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|   ! 0 | 1477 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1478 | `		return PH7_OK;` |
|     - | 1479 | `	}` |
|    73 | 1480 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltinFilters) ; ++n ){` |
|    63 | 1481 | `		ph7_value_string(pValue,aBuiltinFilters[n].zName,-1);` |
|    63 | 1482 | `		ph7_array_add_elem(pArray,0,pValue);` |
|    63 | 1483 | `		ph7_value_reset_string_cursor(pValue);` |
|    33 | 1484 | `	}` |
|     - | 1485 | `	{` |
|     - | 1486 | `		/* And whatever the script registered, newest last — php lists them` |
|     - | 1487 | `		 * beside its own. */` |
|     - | 1488 | `		phl_ufilter_reg *pReg;` |
|     - | 1489 | `		SySet aName;` |
|     - | 1490 | `		sxu32 i;` |
|    13 | 1491 | `		SySetInit(&aName,&pCtx->pVm->sAllocator,sizeof(phl_ufilter_reg *));` |
|    15 | 1492 | `		for( pReg = (phl_ufilter_reg *)pCtx->pVm->pUserFilters ; pReg ; pReg = pReg->pNext ){` |
|     3 | 1493 | `			SySetPut(&aName,(const void *)&pReg);` |
|     2 | 1494 | `		}` |
|    15 | 1495 | `		for( i = SySetUsed(&aName) ; i > 0 ; --i ){` |
|     3 | 1496 | `			phl_ufilter_reg **ppReg = (phl_ufilter_reg **)SySetAt(&aName,i-1);` |
|     4 | 1497 | `			ph7_value_string(pValue,(const char *)SyBlobData(&(*ppReg)->sName),` |
|     2 | 1498 | `				(int)SyBlobLength(&(*ppReg)->sName));` |
|     3 | 1499 | `			ph7_array_add_elem(pArray,0,pValue);` |
|     3 | 1500 | `			ph7_value_reset_string_cursor(pValue);` |
|     2 | 1501 | `		}` |
|    13 | 1502 | `		SySetRelease(&aName);` |
|     - | 1503 | `	}` |
|    13 | 1504 | `	ph7_result_value(pCtx,pArray);` |
|    13 | 1505 | `	return PH7_OK;` |
|     8 | 1506 | `}` |
|     - | 1507 | `/*` |
|     - | 1508 | ` * ---------------------------------------------------------------------------` |
|     - | 1509 | ` * php://filter/…/resource=… — the URL form of the same chain.` |
|     - | 1510 | ` *` |
|     - | 1511 | `` * The path after `filter/` is a list of `/`-separated segments: `read=a\|b` and`` |
|     - | 1512 | `` * `write=a\|b` name one chain each, and a bare `a\|b` names both (as far as the`` |
|     - | 1513 | ` * OPEN MODE allows — a read filter on a write-only handle is dropped). What php` |
|     - | 1514 | ` * does with the RESOURCE is worth spelling out, because it is not a clean split:` |
|     - | 1515 | `` * it looks for `/resource=` and truncates the list there, and when the path`` |
|     - | 1516 | `` * BEGINS with `resource=` — no slash before it — it takes the resource and`` |
|     - | 1517 | ` * leaves the list alone, so every segment of the resource path is then tried as` |
|     - | 1518 | `` * a filter name too. `php://filter/resource=/tmp/x` really does warn about`` |
|     - | 1519 | `` * `resource=`, `tmp` and `x` and then open the file.`` |
|     - | 1520 | ` * ---------------------------------------------------------------------------` |
|     - | 1521 | ` */` |
|    42 | 1522 | `static void FilterUrlOne(ph7_vm *pVm,io_private *pDev,const char *zList,int nList,int iChains)` |
|     2 | 1523 | `{` |
|    44 | 1524 | `	int i = 0;` |
|    88 | 1525 | `	while( i < nList ){` |
|    46 | 1526 | `		int j = i;` |
|   656 | 1527 | `		while( j < nList && zList[j] != '\|' ){` |
|   612 | 1528 | `			j++;` |
|     2 | 1529 | `		}` |
|    46 | 1530 | `		if( j > i ){` |
|    46 | 1531 | `			int bOk = 1;` |
|    46 | 1532 | `			if( iChains & PHL_STREAM_FILTER_READ ){` |
|    59 | 1533 | `				bOk = PH7_StreamFilterAttach(pVm,pDev,&zList[i],j-i,` |
|    38 | 1534 | `					PHL_STREAM_FILTER_READ,0,0,0) != 0;` |
|    19 | 1535 | `			}` |
|    46 | 1536 | `			if( bOk && (iChains & PHL_STREAM_FILTER_WRITE) ){` |
|    10 | 1537 | `				bOk = PH7_StreamFilterAttach(pVm,pDev,&zList[i],j-i,` |
|     6 | 1538 | `					PHL_STREAM_FILTER_WRITE,0,0,0) != 0;` |
|     3 | 1539 | `			}` |
|    46 | 1540 | `			if( !bOk ){` |
|     - | 1541 | `				/* The URL form says it TWICE: once about the name and once about` |
|     - | 1542 | `				 * the chain it could not be put on. The open still succeeds —` |
|     - | 1543 | `				 * php opens the resource with the filters it could make. */` |
|     - | 1544 | `				char zMsg[160];` |
|     7 | 1545 | `				SyBufferFormat(zMsg,sizeof(zMsg),"Unable to create filter (%.*s)",` |
|     2 | 1546 | `					j-i,&zList[i]);` |
|     5 | 1547 | `				PH7_VmThrowError(pVm,pVm->pCalleeName,PH7_CTX_WARNING,zMsg);` |
|     2 | 1548 | `			}` |
|    22 | 1549 | `		}` |
|    46 | 1550 | `		i = j + 1;` |
|     2 | 1551 | `	}` |
|    44 | 1552 | `}` |
|    44 | 1553 | `PH7_PRIVATE int PH7_StreamFilterParseUrl(ph7_vm *pVm,const char *zSpec,int nSpec,` |
|     - | 1554 | `	io_private *pDev,int iChains)` |
|     2 | 1555 | `{` |
|    46 | 1556 | `	int i = 0;` |
|    92 | 1557 | `	while( i < nSpec ){` |
|    48 | 1558 | `		int j = i,iWant = iChains;` |
|     - | 1559 | `		const char *zList;` |
|     - | 1560 | `		int nName;` |
|   906 | 1561 | `		while( j < nSpec && zSpec[j] != '/' ){` |
|   860 | 1562 | `			j++;` |
|     2 | 1563 | `		}` |
|    48 | 1564 | `		zList = &zSpec[i];` |
|    48 | 1565 | `		nName = j - i;` |
|    48 | 1566 | `		if( nName >= 5 && SyMemcmp(zList,"read=",5) == 0 ){` |
|    36 | 1567 | `			iWant = iChains & PHL_STREAM_FILTER_READ;` |
|    36 | 1568 | `			zList += 5;` |
|    36 | 1569 | `			nName -= 5;` |
|    30 | 1570 | `		}else if( nName >= 6 && SyMemcmp(zList,"write=",6) == 0 ){` |
|     9 | 1571 | `			iWant = iChains & PHL_STREAM_FILTER_WRITE;` |
|     9 | 1572 | `			zList += 6;` |
|     9 | 1573 | `			nName -= 6;` |
|     4 | 1574 | `		}` |
|    48 | 1575 | `		if( nName > 0 && iWant != 0 ){` |
|    44 | 1576 | `			FilterUrlOne(pVm,pDev,zList,nName,iWant);` |
|    21 | 1577 | `		}` |
|    48 | 1578 | `		i = j + 1;` |
|     2 | 1579 | `	}` |
|    46 | 1580 | `	return PH7_OK;` |
|     2 | 1581 | `}` |
|     - | 1582 |  |
|     - | 1583 | `/*` |
|     - | 1584 | ` * ---------------------------------------------------------------------------` |
|     - | 1585 | ` * Userland filters: stream_filter_register(), php_user_filter and the buckets.` |
|     - | 1586 | ` *` |
|     - | 1587 | ` * A userland filter is a CLASS, not a function: php instantiates it once per` |
|     - | 1588 | ` * attachment, tells it what name it was created under and what params it was` |
|     - | 1589 | ` * given, and then calls filter($in,$out,&$consumed,$closing) with two BRIGADE` |
|     - | 1590 | `` * handles. The script walks `$in` with stream_bucket_make_writeable(), which`` |
|     - | 1591 | ` * hands over one bucket at a time as a StreamBucket object, and appends what it` |
|     - | 1592 | `` * made to `$out`. What it RETURNS is the chain's answer: PSFS_PASS_ON,`` |
|     - | 1593 | ` * PSFS_FEED_ME or PSFS_ERR_FATAL.` |
|     - | 1594 | ` *` |
|     - | 1595 | ``  * The bucket the script sees is a VALUE — its bytes live in the object's `data` `` |
|     - | 1596 | ` * property, which the script may replace outright — so the C bucket ends at` |
|     - | 1597 | ` * make_writeable and stream_bucket_append() builds a new one from whatever the` |
|     - | 1598 | `` * object holds when it is appended. `$bucket->bucket` is the handle php shows`` |
|     - | 1599 | ` * there; it is a token owned by the call, and it goes back with it.` |
|     - | 1600 | ` * ---------------------------------------------------------------------------` |
|     - | 1601 | ` */` |
|     - | 1602 | ``/* The `bucket` handle a StreamBucket carries. It names nothing the engine reads`` |
|     - | 1603 | ` * back — the bytes are in the object — and exists because php shows one. */` |
|     - | 1604 | `typedef struct phl_bucket_tok phl_bucket_tok;` |
|     - | 1605 | `struct phl_bucket_tok` |
|     - | 1606 | `{` |
|     - | 1607 | `	io_private base;           /* resource header (base.iMagic == STREAM_BUCKET_MAGIC) */` |
|     - | 1608 | `	phl_bucket_tok *pNext;` |
|     - | 1609 | `};` |
|     - | 1610 | `/* The registration behind a name, php's own lookup: the exact name, then` |
|     - | 1611 | `` * progressively shorter `prefix.*` wildcards. */`` |
|    40 | 1612 | `static phl_ufilter_reg * UserFilterFind(ph7_vm *pVm,const char *zName,int nName)` |
|     2 | 1613 | `{` |
|     - | 1614 | `	phl_ufilter_reg *pReg;` |
|     - | 1615 | `	char zWild[128];` |
|     - | 1616 | `	int nTry;` |
|    66 | 1617 | `	for( pReg = (phl_ufilter_reg *)pVm->pUserFilters ; pReg ; pReg = pReg->pNext ){` |
|    50 | 1618 | `		if( (int)SyBlobLength(&pReg->sName) == nName` |
|    41 | 1619 | `		 && SyMemcmp(SyBlobData(&pReg->sName),zName,(sxu32)nName) == 0 ){` |
|    28 | 1620 | `			return pReg;` |
|     - | 1621 | `		}` |
|    13 | 1622 | `	}` |
|    15 | 1623 | `	nTry = nName;` |
|    13 | 1624 | `	for(;;){` |
|   151 | 1625 | `		while( nTry > 0 && zName[nTry-1] != '.' ){` |
|   125 | 1626 | `			nTry--;` |
|     1 | 1627 | `		}` |
|    27 | 1628 | `		if( nTry < 1 ){` |
|    13 | 1629 | `			break;` |
|     - | 1630 | `		}` |
|    15 | 1631 | `		if( nTry + 1 < (int)sizeof(zWild) ){` |
|    15 | 1632 | `			SyMemcpy(zName,zWild,(sxu32)nTry);` |
|    15 | 1633 | `			zWild[nTry] = '*';` |
|    35 | 1634 | `			for( pReg = (phl_ufilter_reg *)pVm->pUserFilters ; pReg ; pReg = pReg->pNext ){` |
|    22 | 1635 | `				if( (int)SyBlobLength(&pReg->sName) == nTry + 1` |
|    14 | 1636 | `				 && SyMemcmp(SyBlobData(&pReg->sName),zWild,(sxu32)(nTry+1)) == 0 ){` |
|     3 | 1637 | `					return pReg;` |
|     - | 1638 | `				}` |
|    11 | 1639 | `			}` |
|     6 | 1640 | `		}` |
|    13 | 1641 | `		nTry--;` |
|     1 | 1642 | `	}` |
|    13 | 1643 | `	return 0;` |
|    22 | 1644 | `}` |
|     - | 1645 | `/* Call one of the three methods on the filter's instance. */` |
|    94 | 1646 | `static int UserFilterCall(phl_stream_filter *pFilter,const char *zMethod,int nArg,` |
|     - | 1647 | `	ph7_value **apArg,ph7_value *pResult)` |
|     2 | 1648 | `{` |
|    96 | 1649 | `	ph7_class_instance *pObj = (ph7_class_instance *)pFilter->pObj;` |
|     - | 1650 | `	ph7_class_method *pMeth;` |
|    96 | 1651 | `	if( pObj == 0 ){` |
|   ! 0 | 1652 | `		return -1;` |
|     - | 1653 | `	}` |
|    96 | 1654 | `	pMeth = PH7_ClassExtractMethod(pObj->pClass,zMethod,(sxu32)SyStrlen(zMethod));` |
|    96 | 1655 | `	if( pMeth == 0 ){` |
|     - | 1656 | `		/* php requires nothing of the class but the name: a class that does not` |
|     - | 1657 | `		 * extend php_user_filter and declares none of the three is registered` |
|     - | 1658 | `		 * and attached without complaint, and only the missing filter() is ever` |
|     - | 1659 | `		 * noticed — at the READ. */` |
|   ! 0 | 1660 | `		return 1;` |
|     - | 1661 | `	}` |
|    96 | 1662 | `	if( PH7_VmCallClassMethod(pFilter->pVm,pObj,pMeth,pResult,nArg,apArg) != SXRET_OK ){` |
|   ! 0 | 1663 | `		return -1;` |
|     - | 1664 | `	}` |
|    96 | 1665 | `	return 0;` |
|    49 | 1666 | `}` |
|    26 | 1667 | `static void UserFilterClose(phl_stream_filter *pFilter)` |
|     2 | 1668 | `{` |
|     - | 1669 | `	ph7_value sRet;` |
|    28 | 1670 | `	if( pFilter->pObj ){` |
|    26 | 1671 | `		PH7_MemObjInit(pFilter->pVm,&sRet);` |
|    26 | 1672 | `		UserFilterCall(pFilter,"onClose",0,0,&sRet);` |
|    26 | 1673 | `		PH7_MemObjRelease(&sRet);` |
|     - | 1674 | `		/* The instance was created here and is held by nothing else. */` |
|    26 | 1675 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pFilter->pObj);` |
|    26 | 1676 | `		pFilter->pObj = 0;` |
|    12 | 1677 | `	}` |
|    28 | 1678 | `	if( pFilter->pStreamRes ){` |
|    28 | 1679 | `		ph7_release_value(pFilter->pVm,pFilter->pStreamRes);` |
|    28 | 1680 | `		pFilter->pStreamRes = 0;` |
|    13 | 1681 | `	}` |
|    28 | 1682 | `	pFilter->sIn.pBrig = 0;` |
|    28 | 1683 | `	pFilter->sOut.pBrig = 0;` |
|    28 | 1684 | `}` |
|     - | 1685 | `/* Build a brigade handle for one filter() call. */` |
|    88 | 1686 | `static void UserBrigadeInit(phl_brigade_res *pRes,ph7_vm *pVm,phl_brigade *pBrig)` |
|     2 | 1687 | `{` |
|    90 | 1688 | `	pRes->base.iHead = IO_PRIVATE_HEAD_MAGIC;` |
|    90 | 1689 | `	pRes->base.iMagic = STREAM_BRIGADE_MAGIC;` |
|    90 | 1690 | `	pRes->pVm = pVm;` |
|    90 | 1691 | `	pRes->pBrig = pBrig;` |
|    90 | 1692 | `}` |
|     - | 1693 | `/* The brigade behind the handle goes away with the call; the handle itself` |
|     - | 1694 | ` * stays in bounds, so a script that kept one simply finds it empty. */` |
|    88 | 1695 | `static void UserBrigadeDetach(phl_brigade_res *pRes)` |
|     2 | 1696 | `{` |
|    90 | 1697 | `	pRes->pBrig = 0;` |
|    90 | 1698 | `}` |
|    82 | 1699 | `static phl_brigade_res * UserBrigadeFromValue(ph7_value *pVal)` |
|     2 | 1700 | `{` |
|     - | 1701 | `	phl_brigade_res *pRes;` |
|    84 | 1702 | `	if( pVal == 0 \|\| !ph7_value_is_resource(pVal) ){` |
|   ! 0 | 1703 | `		return 0;` |
|     - | 1704 | `	}` |
|    84 | 1705 | `	pRes = (phl_brigade_res *)ph7_value_to_resource(pVal);` |
|    84 | 1706 | `	if( pRes == 0 \|\| pRes->base.iMagic != STREAM_BRIGADE_MAGIC ){` |
|   ! 0 | 1707 | `		return 0;` |
|     - | 1708 | `	}` |
|    84 | 1709 | `	return pRes;` |
|    43 | 1710 | `}` |
|     - | 1711 | `/* One StreamBucket object around a run of bytes, with the token php shows. */` |
|    26 | 1712 | `static ph7_class_instance * UserBucketObject(ph7_vm *pVm,const char *zData,int nData)` |
|     2 | 1713 | `{` |
|     - | 1714 | `	ph7_class *pClass;` |
|     - | 1715 | `	ph7_class_instance *pObj;` |
|     - | 1716 | `	phl_bucket_tok *pTok;` |
|     - | 1717 | `	ph7_value *pSlot;` |
|    28 | 1718 | `	pClass = PH7_VmExtractClass(pVm,"StreamBucket",sizeof("StreamBucket")-1,FALSE,0);` |
|    28 | 1719 | `	pObj = pClass ? PH7_NewClassInstance(pVm,pClass) : 0;` |
|    28 | 1720 | `	if( pObj == 0 ){` |
|   ! 0 | 1721 | `		return 0;` |
|     - | 1722 | `	}` |
|    28 | 1723 | `	pTok = (phl_bucket_tok *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_bucket_tok));` |
|    28 | 1724 | `	if( pTok ){` |
|    28 | 1725 | `		SyZero(pTok,sizeof(*pTok));` |
|    28 | 1726 | `		pTok->base.iMagic = STREAM_BUCKET_MAGIC;` |
|    28 | 1727 | `		pSlot = PH7_NativeAttr(pObj,"bucket");` |
|    28 | 1728 | `		if( pSlot ){` |
|    28 | 1729 | `			PH7_MemObjRelease(pSlot);` |
|    28 | 1730 | `			pSlot->x.pOther = (void *)pTok;` |
|    28 | 1731 | `			pSlot->iFlags = MEMOBJ_RES;` |
|    13 | 1732 | `		}` |
|    13 | 1733 | `	}` |
|    28 | 1734 | `	PH7_NativeSetAttrStr(pVm,pObj,"data",zData,nData);` |
|    28 | 1735 | `	PH7_NativeSetAttrInt(pVm,pObj,"datalen",(sxi64)nData);` |
|    28 | 1736 | `	PH7_NativeSetAttrInt(pVm,pObj,"dataLength",(sxi64)nData);` |
|    28 | 1737 | `	return pObj;` |
|    15 | 1738 | `}` |
|     - | 1739 | `/* The token goes back with the object that owns it — which is what keeps` |
|     - | 1740 | `` * `$bucket->bucket` in bounds for as long as the script holds the bucket. */`` |
|    26 | 1741 | `static void UserBucketRelease(ph7_vm *pVm,ph7_class_instance *pObj)` |
|     2 | 1742 | `{` |
|    28 | 1743 | `	ph7_value *pSlot = PH7_NativeAttr(pObj,"bucket");` |
|    28 | 1744 | `	if( pSlot && (pSlot->iFlags & MEMOBJ_RES) && pSlot->x.pOther ){` |
|    26 | 1745 | `		phl_bucket_tok *pTok = (phl_bucket_tok *)pSlot->x.pOther;` |
|    26 | 1746 | `		if( pTok->base.iMagic == STREAM_BUCKET_MAGIC ){` |
|    26 | 1747 | `			pTok->base.iMagic = 0;` |
|    26 | 1748 | `			SyMemBackendFree(&pVm->sAllocator,pTok);` |
|    12 | 1749 | `		}` |
|    26 | 1750 | `		pSlot->x.pOther = 0;` |
|    26 | 1751 | `		pSlot->iFlags = MEMOBJ_NULL;` |
|    12 | 1752 | `	}` |
|    28 | 1753 | `}` |
|     - | 1754 | `/*` |
|     - | 1755 | ` * The filter() call itself. php hands over four arguments — the two brigades,` |
|     - | 1756 | ` * a by-reference $consumed that arrives as NULL, and whether this is the last` |
|     - | 1757 | ` * call — and reads the answer as one of the PSFS_* codes.` |
|     - | 1758 | ` */` |
|    44 | 1759 | `static int UserFilterRun(phl_stream_filter *pFilter,phl_brigade *pIn,phl_brigade *pOut,int iFlags)` |
|     2 | 1760 | `{` |
|    46 | 1761 | `	ph7_vm *pVm = pFilter->pVm;` |
|     - | 1762 | `	ph7_value *apArg[4];` |
|     - | 1763 | `	ph7_value sRet;` |
|     - | 1764 | `	void *pSavedCall;` |
|    46 | 1765 | `	sxu32 nConsumedIdx = SXU32_HIGH;` |
|     - | 1766 | `	int i,rc,iStatus;` |
|    46 | 1767 | `	if( pFilter->pObj == 0 ){` |
|   ! 0 | 1768 | `		return PHL_PSFS_ERR_FATAL;` |
|     - | 1769 | `	}` |
|    46 | 1770 | `	UserBrigadeInit(&pFilter->sIn,pVm,pIn);` |
|    46 | 1771 | `	UserBrigadeInit(&pFilter->sOut,pVm,pOut);` |
|   222 | 1772 | `	for( i = 0 ; i < 4 ; i++ ){` |
|   178 | 1773 | `		apArg[i] = ph7_new_scalar(pVm);` |
|    90 | 1774 | `	}` |
|    46 | 1775 | `	if( apArg[0] == 0 \|\| apArg[1] == 0 \|\| apArg[2] == 0 \|\| apArg[3] == 0 ){` |
|   ! 0 | 1776 | `		for( i = 0 ; i < 4 ; i++ ){` |
|   ! 0 | 1777 | `			if( apArg[i] ){` |
|   ! 0 | 1778 | `				ph7_release_value(pVm,apArg[i]);` |
|   ! 0 | 1779 | `			}` |
|   ! 0 | 1780 | `		}` |
|   ! 0 | 1781 | `		UserBrigadeDetach(&pFilter->sIn);` |
|   ! 0 | 1782 | `		UserBrigadeDetach(&pFilter->sOut);` |
|   ! 0 | 1783 | `		return PHL_PSFS_ERR_FATAL;` |
|     - | 1784 | `	}` |
|    46 | 1785 | `	ph7_value_resource(apArg[0],(void *)&pFilter->sIn);` |
|    46 | 1786 | `	ph7_value_resource(apArg[1],(void *)&pFilter->sOut);` |
|     - | 1787 | `	/* php's $consumed is BY REFERENCE and arrives NULL, not 0. A by-ref` |
|     - | 1788 | `	 * parameter binds to a caller SLOT, and the engine building the argument` |
|     - | 1789 | `	 * has none to offer — so one is reserved here, exactly as a variable would` |
|     - | 1790 | `	 * have, and the filter writes into it for real. */` |
|     - | 1791 | `	{` |
|    46 | 1792 | `		ph7_value *pSlot = VmReserveMemObj(pVm,&nConsumedIdx);` |
|    46 | 1793 | `		if( pSlot == 0 ){` |
|   ! 0 | 1794 | `			for( i = 0 ; i < 4 ; i++ ){` |
|   ! 0 | 1795 | `				ph7_release_value(pVm,apArg[i]);` |
|   ! 0 | 1796 | `			}` |
|   ! 0 | 1797 | `			UserBrigadeDetach(&pFilter->sIn);` |
|   ! 0 | 1798 | `			UserBrigadeDetach(&pFilter->sOut);` |
|   ! 0 | 1799 | `			return PHL_PSFS_ERR_FATAL;` |
|     - | 1800 | `		}` |
|    46 | 1801 | `		PH7_MemObjInit(pVm,pSlot);` |
|    46 | 1802 | `		pSlot->nIdx = nConsumedIdx;` |
|    46 | 1803 | `		ph7_value_null(apArg[2]);` |
|    46 | 1804 | `		apArg[2]->nIdx = nConsumedIdx;` |
|     - | 1805 | `	}` |
|    46 | 1806 | `	ph7_value_bool(apArg[3],(iFlags & PHL_PSFS_FLAG_FLUSH_CLOSE) != 0);` |
|     - | 1807 | ``	/* php sets `$this->stream` for the duration of the call and for no longer:`` |
|     - | 1808 | `	 * onCreate() sees nothing there. */` |
|    46 | 1809 | `	if( pFilter->pStreamRes ){` |
|    46 | 1810 | `		ph7_value *pSlot = PH7_NativeAttr((ph7_class_instance *)pFilter->pObj,"stream");` |
|    46 | 1811 | `		if( pSlot ){` |
|    46 | 1812 | `			PH7_MemObjStore(pFilter->pStreamRes,pSlot);` |
|    22 | 1813 | `		}` |
|    22 | 1814 | `	}` |
|    46 | 1815 | `	pSavedCall = pVm->pFilterCall;` |
|    46 | 1816 | `	pVm->pFilterCall = (void *)&pFilter->sOut;` |
|    46 | 1817 | `	PH7_MemObjInit(pVm,&sRet);` |
|    46 | 1818 | `	rc = UserFilterCall(pFilter,"filter",4,apArg,&sRet);` |
|    46 | 1819 | `	pVm->pFilterCall = pSavedCall;` |
|    46 | 1820 | `	iStatus = rc == 0 ? (int)ph7_value_to_int(&sRet) : PHL_PSFS_ERR_FATAL;` |
|    46 | 1821 | `	PH7_MemObjRelease(&sRet);` |
|   222 | 1822 | `	for( i = 0 ; i < 4 ; i++ ){` |
|   178 | 1823 | `		ph7_release_value(pVm,apArg[i]);` |
|    90 | 1824 | `	}` |
|    46 | 1825 | `	if( nConsumedIdx != SXU32_HIGH ){` |
|    46 | 1826 | `		PH7_VmReleaseUnheldSlot(pVm,nConsumedIdx);` |
|    22 | 1827 | `	}` |
|     - | 1828 | ``	/* `stream` is set for the DURATION of the call, so onClose() finds nothing`` |
|     - | 1829 | `	 * there — which is what php shows. */` |
|     - | 1830 | `	{` |
|    46 | 1831 | `		ph7_value *pSlot = PH7_NativeAttr((ph7_class_instance *)pFilter->pObj,"stream");` |
|    46 | 1832 | `		if( pSlot ){` |
|    46 | 1833 | `			PH7_MemObjRelease(pSlot);` |
|    22 | 1834 | `		}` |
|     - | 1835 | `	}` |
|    46 | 1836 | `	UserBrigadeDetach(&pFilter->sIn);` |
|    46 | 1837 | `	UserBrigadeDetach(&pFilter->sOut);` |
|    46 | 1838 | `	if( iStatus != PHL_PSFS_PASS_ON && iStatus != PHL_PSFS_FEED_ME ){` |
|     5 | 1839 | `		return PHL_PSFS_ERR_FATAL;` |
|     - | 1840 | `	}` |
|    42 | 1841 | `	return iStatus;` |
|    24 | 1842 | `}` |
|     - | 1843 | `static const phl_filter_ops sUserFilterOps = { "", 0, UserFilterRun, UserFilterClose };` |
|     - | 1844 | `/*` |
|     - | 1845 | ` * Create the instance behind one attachment. php refuses when the class is not` |
|     - | 1846 | ` * defined and when onCreate() answers FALSE, and says so twice on the second` |
|     - | 1847 | ` * one — once about the class, once about the filter.` |
|     - | 1848 | ` */` |
|    28 | 1849 | `static phl_stream_filter * UserFilterCreate(ph7_vm *pVm,phl_ufilter_reg *pReg,` |
|     - | 1850 | `	const char *zName,int nName,ph7_value *pParams,ph7_value *pStream)` |
|     2 | 1851 | `{` |
|     - | 1852 | `	phl_stream_filter *pFilter;` |
|     - | 1853 | `	ph7_class *pClass;` |
|     - | 1854 | `	ph7_class_instance *pObj;` |
|     - | 1855 | `	ph7_value sRet;` |
|    30 | 1856 | `	int nClass = (int)SyBlobLength(&pReg->sClass);` |
|    30 | 1857 | `	const char *zClass = (const char *)SyBlobData(&pReg->sClass);` |
|    30 | 1858 | `	pClass = PH7_VmExtractClass(pVm,zClass,(sxu32)nClass,FALSE,0);` |
|    30 | 1859 | `	if( pClass == 0 ){` |
|     - | 1860 | `		char zMsg[192];` |
|     4 | 1861 | `		SyBufferFormat(zMsg,sizeof(zMsg),` |
|     - | 1862 | `			"User-filter \"%.*s\" requires class \"%.*s\", but that class is not defined",` |
|     1 | 1863 | `			nName,zName,nClass,zClass);` |
|     3 | 1864 | `		PH7_VmThrowError(pVm,pVm->pCalleeName,PH7_CTX_WARNING,zMsg);` |
|     3 | 1865 | `		return 0;` |
|     - | 1866 | `	}` |
|    28 | 1867 | `	pObj = PH7_NewClassInstance(pVm,pClass);` |
|    28 | 1868 | `	if( pObj == 0 ){` |
|   ! 0 | 1869 | `		return 0;` |
|     - | 1870 | `	}` |
|    28 | 1871 | `	pFilter = FilterNew(pVm,&sUserFilterOps,zName,nName);` |
|    28 | 1872 | `	if( pFilter == 0 ){` |
|   ! 0 | 1873 | `		PH7_ClassInstanceUnref(pObj);` |
|   ! 0 | 1874 | `		return 0;` |
|     - | 1875 | `	}` |
|    28 | 1876 | `	pFilter->pObj = (void *)pObj;` |
|     - | 1877 | `	/* The name it was created UNDER, which a wildcard registration needs: a` |
|     - | 1878 | ``	 * `my.*` filter asked for as `my.thing` is told `my.thing`. */`` |
|    28 | 1879 | `	PH7_NativeSetAttrStr(pVm,pObj,"filtername",zName,nName);` |
|     - | 1880 | `	{` |
|    28 | 1881 | `		ph7_value *pSlot = PH7_NativeAttr(pObj,"params");` |
|    28 | 1882 | `		if( pSlot ){` |
|    28 | 1883 | `			if( pParams ){` |
|     3 | 1884 | `				PH7_MemObjStore(pParams,pSlot);` |
|     2 | 1885 | `			}else{` |
|    26 | 1886 | `				PH7_MemObjRelease(pSlot);` |
|     - | 1887 | `			}` |
|    13 | 1888 | `		}` |
|     - | 1889 | `	}` |
|    28 | 1890 | `	if( pStream ){` |
|    28 | 1891 | `		pFilter->pStreamRes = ph7_new_scalar(pVm);` |
|    28 | 1892 | `		if( pFilter->pStreamRes ){` |
|    28 | 1893 | `			PH7_MemObjStore(pStream,pFilter->pStreamRes);` |
|    13 | 1894 | `		}` |
|    13 | 1895 | `	}` |
|    28 | 1896 | `	PH7_MemObjInit(pVm,&sRet);` |
|     - | 1897 | `	/* A class with no onCreate() of its own simply has nothing to refuse with. */` |
|    28 | 1898 | `	if( UserFilterCall(pFilter,"onCreate",0,0,&sRet) == 0 && !ph7_value_to_bool(&sRet) ){` |
|     3 | 1899 | `		PH7_MemObjRelease(&sRet);` |
|     - | 1900 | `		/* php does not call onClose() for a filter onCreate() refused, so the` |
|     - | 1901 | `		 * instance goes back here rather than through the close path. */` |
|     3 | 1902 | `		PH7_ClassInstanceUnref(pObj);` |
|     3 | 1903 | `		pFilter->pObj = 0;` |
|     3 | 1904 | `		FilterDispose(pFilter);` |
|     3 | 1905 | `		return 0;` |
|     - | 1906 | `	}` |
|    26 | 1907 | `	PH7_MemObjRelease(&sRet);` |
|    26 | 1908 | `	return pFilter;` |
|    16 | 1909 | `}` |
|     - | 1910 | `/*` |
|     - | 1911 | ` * bool stream_filter_register(string $filter_name, string $class)` |
|     - | 1912 | ` *  php refuses an empty name or class outright, and answers FALSE for a name` |
|     - | 1913 | ` *  that is already taken rather than replacing it.` |
|     - | 1914 | ` */` |
|    32 | 1915 | `PH7_PRIVATE int PH7_builtin_stream_filter_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 1916 | `{` |
|    34 | 1917 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 1918 | `	phl_ufilter_reg *pReg;` |
|     - | 1919 | `	const char *zName,*zClass;` |
|     - | 1920 | `	int nName,nClass;` |
|    16 | 1921 | `	SXUNUSED(nArg);` |
|    34 | 1922 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|    34 | 1923 | `	zClass = ph7_value_to_string(apArg[1],&nClass);` |
|    34 | 1924 | `	if( nName < 1 ){` |
|     4 | 1925 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 1926 | `			"%s(): Argument #1 ($filter_name) must be a non-empty string",` |
|     1 | 1927 | `			ph7_function_name(pCtx));` |
|     - | 1928 | `	}` |
|    32 | 1929 | `	if( nClass < 1 ){` |
|     4 | 1930 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 1931 | `			"%s(): Argument #2 ($class) must be a non-empty string",` |
|     1 | 1932 | `			ph7_function_name(pCtx));` |
|     - | 1933 | `	}` |
|    30 | 1934 | `	if( FilterFindOpsExact(zName,nName) != 0 ){` |
|     - | 1935 | `		/* A name one of the built-ins answers to is taken. */` |
|     3 | 1936 | `		ph7_result_bool(pCtx,0);` |
|     3 | 1937 | `		return PH7_OK;` |
|     - | 1938 | `	}` |
|   138 | 1939 | `	for( pReg = (phl_ufilter_reg *)pVm->pUserFilters ; pReg ; pReg = pReg->pNext ){` |
|   112 | 1940 | `		if( (int)SyBlobLength(&pReg->sName) == nName` |
|    69 | 1941 | `		 && SyMemcmp(SyBlobData(&pReg->sName),zName,(sxu32)nName) == 0 ){` |
|     3 | 1942 | `			ph7_result_bool(pCtx,0);` |
|     3 | 1943 | `			return PH7_OK;` |
|     - | 1944 | `		}` |
|    56 | 1945 | `	}` |
|    26 | 1946 | `	pReg = (phl_ufilter_reg *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_ufilter_reg));` |
|    26 | 1947 | `	if( pReg == 0 ){` |
|   ! 0 | 1948 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 1949 | `	}` |
|    26 | 1950 | `	SyZero(pReg,sizeof(*pReg));` |
|    26 | 1951 | `	SyBlobInit(&pReg->sName,&pVm->sAllocator);` |
|    26 | 1952 | `	SyBlobInit(&pReg->sClass,&pVm->sAllocator);` |
|    26 | 1953 | `	SyBlobAppend(&pReg->sName,zName,(sxu32)nName);` |
|    26 | 1954 | `	SyBlobAppend(&pReg->sClass,zClass,(sxu32)nClass);` |
|    26 | 1955 | `	pReg->pNext = (phl_ufilter_reg *)pVm->pUserFilters;` |
|    26 | 1956 | `	pVm->pUserFilters = (void *)pReg;` |
|    26 | 1957 | `	ph7_result_bool(pCtx,1);` |
|    26 | 1958 | `	return PH7_OK;` |
|    18 | 1959 | `}` |
|     - | 1960 | `/*` |
|     - | 1961 | ` * ?StreamBucket stream_bucket_make_writeable(resource $brigade)` |
|     - | 1962 | ` *  Take the next bucket off the brigade, as an object the script owns.` |
|     - | 1963 | ` */` |
|    60 | 1964 | `PH7_PRIVATE int PH7_builtin_stream_bucket_make_writeable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 1965 | `{` |
|     - | 1966 | `	char zGiven[64];` |
|     - | 1967 | `	phl_brigade_res *pRes;` |
|     - | 1968 | `	phl_bucket *pBucket;` |
|     - | 1969 | `	ph7_class_instance *pObj;` |
|    30 | 1970 | `	SXUNUSED(nArg);` |
|    62 | 1971 | `	pRes = UserBrigadeFromValue(apArg[0]);` |
|    62 | 1972 | `	if( pRes == 0 ){` |
|   ! 0 | 1973 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 1974 | `			"%s(): Argument #1 ($brigade) must be of type resource, %s given",` |
|   ! 0 | 1975 | `			ph7_function_name(pCtx),VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));` |
|     - | 1976 | `	}` |
|    62 | 1977 | `	pBucket = pRes->pBrig ? PH7_FilterBucketPop(pRes->pBrig) : 0;` |
|    62 | 1978 | `	if( pBucket == 0 ){` |
|    42 | 1979 | `		ph7_result_null(pCtx);` |
|    42 | 1980 | `		return PH7_OK;` |
|     - | 1981 | `	}` |
|    32 | 1982 | `	pObj = UserBucketObject(pRes->pVm,(const char *)SyBlobData(&pBucket->sData),` |
|    20 | 1983 | `		(int)SyBlobLength(&pBucket->sData));` |
|    22 | 1984 | `	PH7_FilterBucketFree(pRes->pVm,pBucket);` |
|    22 | 1985 | `	if( pObj == 0 ){` |
|   ! 0 | 1986 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 1987 | `	}` |
|    22 | 1988 | `	PH7_NativeResultObject(pCtx,pObj);` |
|    22 | 1989 | `	return PH7_OK;` |
|    32 | 1990 | `}` |
|     - | 1991 | `/* The two that put one back, differing only in WHICH end. */` |
|    22 | 1992 | `static int UserBucketPut(ph7_context *pCtx,ph7_value **apArg,int bPrepend)` |
|     2 | 1993 | `{` |
|     - | 1994 | `	char zGiven[64];` |
|     - | 1995 | `	phl_brigade_res *pRes;` |
|     - | 1996 | `	ph7_class_instance *pObj;` |
|     - | 1997 | `	phl_bucket *pBucket;` |
|    24 | 1998 | `	const char *zData = "";` |
|    24 | 1999 | `	int nData = 0;` |
|    24 | 2000 | `	pRes = UserBrigadeFromValue(apArg[0]);` |
|    24 | 2001 | `	if( pRes == 0 ){` |
|   ! 0 | 2002 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 2003 | `			"%s(): Argument #1 ($brigade) must be of type resource, %s given",` |
|   ! 0 | 2004 | `			ph7_function_name(pCtx),VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));` |
|     - | 2005 | `	}` |
|    24 | 2006 | `	if( !ph7_value_is_object(apArg[1]) ){` |
|   ! 0 | 2007 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 2008 | `			"%s(): Argument #2 ($bucket) must be of type object, %s given",` |
|   ! 0 | 2009 | `			ph7_function_name(pCtx),VmValueGivenName(apArg[1],zGiven,sizeof(zGiven)));` |
|     - | 2010 | `	}` |
|    24 | 2011 | `	pObj = (ph7_class_instance *)apArg[1]->x.pOther;` |
|     - | 2012 | `	/* The bytes are whatever the object holds NOW: a filter that replaced` |
|     - | 2013 | ``	 * `$bucket->data` outright is the ordinary way to write one. */`` |
|    24 | 2014 | `	PH7_NativeAttrStr(pObj,"data",&zData,&nData);` |
|    24 | 2015 | `	if( pRes->pBrig == 0 ){` |
|     - | 2016 | `		/* A handle kept past the call it belonged to: there is nothing to put` |
|     - | 2017 | `		 * it back into. */` |
|   ! 0 | 2018 | `		ph7_result_null(pCtx);` |
|   ! 0 | 2019 | `		return PH7_OK;` |
|     - | 2020 | `	}` |
|    24 | 2021 | `	pBucket = PH7_FilterBucketNew(pRes->pVm,zData,(sxu32)nData);` |
|    24 | 2022 | `	if( pBucket == 0 ){` |
|   ! 0 | 2023 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 2024 | `	}` |
|    24 | 2025 | `	if( bPrepend ){` |
|     3 | 2026 | `		pBucket->pNext = pRes->pBrig->pHead;` |
|     3 | 2027 | `		pRes->pBrig->pHead = pBucket;` |
|     3 | 2028 | `		if( pRes->pBrig->pTail == 0 ){` |
|   ! 0 | 2029 | `			pRes->pBrig->pTail = pBucket;` |
|   ! 0 | 2030 | `		}` |
|     2 | 2031 | `	}else{` |
|    22 | 2032 | `		PH7_FilterBucketAppend(pRes->pBrig,pBucket);` |
|     - | 2033 | `	}` |
|    24 | 2034 | `	ph7_result_null(pCtx);` |
|    24 | 2035 | `	return PH7_OK;` |
|    13 | 2036 | `}` |
|    20 | 2037 | `PH7_PRIVATE int PH7_builtin_stream_bucket_append(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 2038 | `{` |
|    10 | 2039 | `	SXUNUSED(nArg);` |
|    22 | 2040 | `	return UserBucketPut(pCtx,apArg,0);` |
|     2 | 2041 | `}` |
|     2 | 2042 | `PH7_PRIVATE int PH7_builtin_stream_bucket_prepend(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2043 | `{` |
|     1 | 2044 | `	SXUNUSED(nArg);` |
|     3 | 2045 | `	return UserBucketPut(pCtx,apArg,1);` |
|     1 | 2046 | `}` |
|     - | 2047 | `/*` |
|     - | 2048 | ` * StreamBucket stream_bucket_new(resource $stream, string $buffer)` |
|     - | 2049 | ` *  A bucket of the filter's own making — the only way to emit a TAIL, since the` |
|     - | 2050 | ` *  closing call arrives with an empty brigade.` |
|     - | 2051 | ` */` |
|     6 | 2052 | `PH7_PRIVATE int PH7_builtin_stream_bucket_new(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2053 | `{` |
|     - | 2054 | `	ph7_class_instance *pObj;` |
|     - | 2055 | `	const char *zData;` |
|     - | 2056 | `	int nData;` |
|     3 | 2057 | `	SXUNUSED(nArg);` |
|     7 | 2058 | `	zData = ph7_value_to_string(apArg[1],&nData);` |
|     7 | 2059 | `	pObj = UserBucketObject(pCtx->pVm,zData,nData);` |
|     7 | 2060 | `	if( pObj == 0 ){` |
|   ! 0 | 2061 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 2062 | `	}` |
|     7 | 2063 | `	PH7_NativeResultObject(pCtx,pObj);` |
|     7 | 2064 | `	return PH7_OK;` |
|     4 | 2065 | `}` |
|     - | 2066 | `/*` |
|     - | 2067 | ` * php_user_filter and StreamBucket. The three methods are the ones a filter` |
|     - | 2068 | ` * OVERRIDES; their bodies here are php's own do-nothing defaults, and a class` |
|     - | 2069 | ` * that overrides none of them is a filter that refuses every read — which is` |
|     - | 2070 | ` * what php answers too.` |
|     - | 2071 | ` */` |
|     2 | 2072 | `static int vm_builtin_user_filter_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2073 | `{` |
|     1 | 2074 | `	SXUNUSED(nArg);` |
|     1 | 2075 | `	SXUNUSED(apArg);` |
|     3 | 2076 | `	ph7_result_int(pCtx,PHL_PSFS_ERR_FATAL);` |
|     3 | 2077 | `	return PH7_OK;` |
|     1 | 2078 | `}` |
|    20 | 2079 | `static int vm_builtin_user_filter_onCreate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 2080 | `{` |
|    10 | 2081 | `	SXUNUSED(nArg);` |
|    10 | 2082 | `	SXUNUSED(apArg);` |
|    22 | 2083 | `	ph7_result_bool(pCtx,1);` |
|    22 | 2084 | `	return PH7_OK;` |
|     2 | 2085 | `}` |
|    20 | 2086 | `static int vm_builtin_user_filter_onClose(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 2087 | `{` |
|    10 | 2088 | `	SXUNUSED(nArg);` |
|    10 | 2089 | `	SXUNUSED(apArg);` |
|    22 | 2090 | `	ph7_result_null(pCtx);` |
|    22 | 2091 | `	return PH7_OK;` |
|     2 | 2092 | `}` |
|  8445 | 2093 | `PH7_PRIVATE sxi32 PH7_VmInstallStreamFilter(ph7_vm *pVm)` |
|     5 | 2094 | `{` |
|     - | 2095 | `	static const PH7_NativeMethodDef aFilterMethod[] = {` |
|     - | 2096 | `		{ "filter", PH7_MOD_PUBLIC, "$in, $out, &$consumed, bool $closing", "@int",` |
|     - | 2097 | `		  vm_builtin_user_filter_filter },` |
|     - | 2098 | `		{ "onCreate", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_user_filter_onCreate },` |
|     - | 2099 | `		{ "onClose", PH7_MOD_PUBLIC, "", "@void", vm_builtin_user_filter_onClose },` |
|     - | 2100 | `	};` |
|     - | 2101 | `	static const PH7_NativePropDef aFilterProp[] = {` |
|     - | 2102 | `		{ "filtername", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, "string" },` |
|     - | 2103 | `		{ "params", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, "mixed" },` |
|     - | 2104 | `		{ "stream", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - | 2105 | `	};` |
|     - | 2106 | `	static const PH7_NativePropDef aBucketProp[] = {` |
|     - | 2107 | `		{ "bucket", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - | 2108 | `		/* php declares the three TYPED and without a default, so a bucket the` |
|     - | 2109 | `		 * stream layer has not filled yet carries them UNINITIALIZED: absent from` |
|     - | 2110 | `		 * the (array) cast, get_object_vars() and json_encode(), printed as` |
|     - | 2111 | ``		 * `uninitialized(string)` by var_dump and uncounted in its header, and a`` |
|     - | 2112 | `		 * read before the first write is php's "must not be accessed before` |
|     - | 2113 | `		 * initialization" Error rather than an empty string. */` |
|     - | 2114 | `		{ "data", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|     - | 2115 | `		{ "datalen", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "int" },` |
|     - | 2116 | `		{ "dataLength", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "int" },` |
|     - | 2117 | `	};` |
|     - | 2118 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - | 2119 | `		{ "php_user_filter", 0, 0, 0,` |
|     - | 2120 | `		  aFilterMethod, SX_ARRAYSIZE(aFilterMethod), 0, 0,` |
|     - | 2121 | `		  aFilterProp, SX_ARRAYSIZE(aFilterProp), 0, 0, 0 },` |
|     - | 2122 | `		{ "StreamBucket", 0, 0, PH7_CLASS_FINAL,` |
|     - | 2123 | `		  0, 0, 0, 0,` |
|     - | 2124 | `		  aBucketProp, SX_ARRAYSIZE(aBucketProp), UserBucketRelease, 0, 0 },` |
|     - | 2125 | `	};` |
|  8450 | 2126 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|     5 | 2127 | `}` |
|     - | 2128 | `#endif /* PH7_DISABLE_DISK_IO */` |
|     - | 2129 |  |
