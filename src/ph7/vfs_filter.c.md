# src/ph7/vfs_filter.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1198/1336 lines (89.67%)

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
|  1912 |   34 | `PH7_PRIVATE phl_bucket * PH7_FilterBucketNew(ph7_vm *pVm,const void *pData,sxu32 nLen)` |
|     4 |   35 | `{` |
|     - |   36 | `	phl_bucket *pBucket;` |
|  1916 |   37 | `	if( pVm == 0 ){` |
|   ! 0 |   38 | `		return 0;` |
|     - |   39 | `	}` |
|  1916 |   40 | `	pBucket = (phl_bucket *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_bucket));` |
|  1916 |   41 | `	if( pBucket == 0 ){` |
|   ! 0 |   42 | `		return 0;` |
|     - |   43 | `	}` |
|  1916 |   44 | `	SyZero(pBucket,sizeof(phl_bucket));` |
|  1916 |   45 | `	SyBlobInit(&pBucket->sData,&pVm->sAllocator);` |
|  1916 |   46 | `	if( nLen > 0 && pData != 0 ){` |
|  1916 |   47 | `		if( SyBlobAppend(&pBucket->sData,pData,nLen) != SXRET_OK ){` |
|   ! 0 |   48 | `			SyBlobRelease(&pBucket->sData);` |
|   ! 0 |   49 | `			SyMemBackendFree(&pVm->sAllocator,pBucket);` |
|   ! 0 |   50 | `			return 0;` |
|     - |   51 | `		}` |
|   956 |   52 | `	}` |
|  1916 |   53 | `	return pBucket;` |
|   960 |   54 | `}` |
|  1978 |   55 | `PH7_PRIVATE void PH7_FilterBucketAppend(phl_brigade *pBrig,phl_bucket *pBucket)` |
|     4 |   56 | `{` |
|  1982 |   57 | `	if( pBucket == 0 ){` |
|   ! 0 |   58 | `		return;` |
|     - |   59 | `	}` |
|  1982 |   60 | `	pBucket->pNext = 0;` |
|  1982 |   61 | `	if( pBrig->pTail ){` |
|   ! 0 |   62 | `		pBrig->pTail->pNext = pBucket;` |
|   ! 0 |   63 | `	}else{` |
|  1982 |   64 | `		pBrig->pHead = pBucket;` |
|     - |   65 | `	}` |
|  1982 |   66 | `	pBrig->pTail = pBucket;` |
|   993 |   67 | `}` |
|  1912 |   68 | `PH7_PRIVATE void PH7_FilterBucketFree(ph7_vm *pVm,phl_bucket *pBucket)` |
|     4 |   69 | `{` |
|  1916 |   70 | `	if( pBucket == 0 ){` |
|   ! 0 |   71 | `		return;` |
|     - |   72 | `	}` |
|  1916 |   73 | `	SyBlobRelease(&pBucket->sData);` |
|  1916 |   74 | `	SyMemBackendFree(&pVm->sAllocator,pBucket);` |
|   960 |   75 | `}` |
|     - |   76 | `/* Unlink and answer the first bucket of a brigade, or 0 when it is empty. */` |
|  9246 |   77 | `PH7_PRIVATE phl_bucket * PH7_FilterBucketPop(phl_brigade *pBrig)` |
|     4 |   78 | `{` |
|  9250 |   79 | `	phl_bucket *pBucket = pBrig->pHead;` |
|  9250 |   80 | `	if( pBucket == 0 ){` |
|  7270 |   81 | `		return 0;` |
|     - |   82 | `	}` |
|  1984 |   83 | `	pBrig->pHead = pBucket->pNext;` |
|  1984 |   84 | `	if( pBrig->pHead == 0 ){` |
|  1982 |   85 | `		pBrig->pTail = 0;` |
|   989 |   86 | `	}` |
|  1984 |   87 | `	pBucket->pNext = 0;` |
|  1984 |   88 | `	return pBucket;` |
|  4627 |   89 | `}` |
|  4360 |   90 | `PH7_PRIVATE void PH7_FilterBrigadeRelease(ph7_vm *pVm,phl_brigade *pBrig)` |
|     4 |   91 | `{` |
|     - |   92 | `	phl_bucket *pBucket;` |
|  4368 |   93 | `	while( (pBucket = PH7_FilterBucketPop(pBrig)) != 0 ){` |
|     5 |   94 | `		PH7_FilterBucketFree(pVm,pBucket);` |
|     1 |   95 | `	}` |
|  4364 |   96 | `}` |
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
|   128 |  118 | `static int StringFilterRun(phl_stream_filter *pFilter,phl_brigade *pIn,phl_brigade *pOut,` |
|     - |  119 | `	int iFlags,int iMode)` |
|     3 |  120 | `{` |
|     - |  121 | `	phl_bucket *pBucket;` |
|    64 |  122 | `	SXUNUSED(iFlags);` |
|    64 |  123 | `	SXUNUSED(pFilter);` |
|   199 |  124 | `	while( (pBucket = PH7_FilterBucketPop(pIn)) != 0 ){` |
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
|   131 |  148 | `	return PHL_PSFS_PASS_ON;` |
|     3 |  149 | `}` |
|    42 |  150 | `static int Rot13Filter(phl_stream_filter *pF,phl_brigade *pIn,phl_brigade *pOut,int iFlags)` |
|     1 |  151 | `{` |
|    43 |  152 | `	return StringFilterRun(pF,pIn,pOut,iFlags,PHL_STRF_ROT13);` |
|     1 |  153 | `}` |
|    70 |  154 | `static int ToUpperFilter(phl_stream_filter *pF,phl_brigade *pIn,phl_brigade *pOut,int iFlags)` |
|     2 |  155 | `{` |
|    72 |  156 | `	return StringFilterRun(pF,pIn,pOut,iFlags,PHL_STRF_TOUPPER);` |
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
|    26 |  878 | `static const phl_filter_ops * FilterFindOpsExact(const char *zName,int nName)` |
|     1 |  879 | `{` |
|     - |  880 | `	sxu32 n;` |
|   175 |  881 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltinFilters) ; ++n ){` |
|   151 |  882 | `		const char *zCur = aBuiltinFilters[n].zName;` |
|   151 |  883 | `		if( (int)SyStrlen(zCur) == nName && SyMemcmp(zCur,zName,(sxu32)nName) == 0 ){` |
|     3 |  884 | `			return &aBuiltinFilters[n];` |
|     - |  885 | `		}` |
|    75 |  886 | `	}` |
|    25 |  887 | `	return 0;` |
|    14 |  888 | `}` |
|   244 |  889 | `static const phl_filter_ops * FilterFindOps(const char *zName,int nName)` |
|     4 |  890 | `{` |
|     - |  891 | `	char zWild[128];` |
|     - |  892 | `	sxu32 n;` |
|     - |  893 | `	int nTry;` |
|  1398 |  894 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltinFilters) ; ++n ){` |
|  1238 |  895 | `		const char *zCur = aBuiltinFilters[n].zName;` |
|  1238 |  896 | `		if( (int)SyStrlen(zCur) == nName && SyMemcmp(zCur,zName,(sxu32)nName) == 0 ){` |
|    87 |  897 | `			return &aBuiltinFilters[n];` |
|     - |  898 | `		}` |
|   579 |  899 | `	}` |
|   162 |  900 | `	nTry = nName;` |
|   101 |  901 | `	for(;;){` |
|     - |  902 | `		/* Strip back to (and including) the last period still inside the prefix. */` |
|  2288 |  903 | `		while( nTry > 0 && zName[nTry-1] != '.' ){` |
|  2086 |  904 | `			nTry--;` |
|     2 |  905 | `		}` |
|   204 |  906 | `		if( nTry < 1 ){` |
|    39 |  907 | `			break;` |
|     - |  908 | `		}` |
|   166 |  909 | `		if( nTry + 1 < (int)sizeof(zWild) ){` |
|   166 |  910 | `			SyMemcpy(zName,zWild,(sxu32)nTry);` |
|   166 |  911 | `			zWild[nTry] = '*';` |
|   802 |  912 | `			for( n = 0 ; n < SX_ARRAYSIZE(aBuiltinFilters) ; ++n ){` |
|   760 |  913 | `				const char *zCur = aBuiltinFilters[n].zName;` |
|   760 |  914 | `				if( (int)SyStrlen(zCur) == nTry + 1 && SyMemcmp(zCur,zWild,(sxu32)(nTry+1)) == 0 ){` |
|   124 |  915 | `					return &aBuiltinFilters[n];` |
|     - |  916 | `				}` |
|   320 |  917 | `			}` |
|    21 |  918 | `		}` |
|    44 |  919 | `		nTry--; /* step past the period we just matched on */` |
|     2 |  920 | `	}` |
|    39 |  921 | `	return 0;` |
|   126 |  922 | `}` |
|     - |  923 | `/* --------------------------------------------------------------------------` |
|     - |  924 | ` * Filter instances.` |
|     - |  925 | ` * -------------------------------------------------------------------------- */` |
|    10 |  926 | `PH7_PRIVATE phl_stream_filter * PH7_StreamFilterFromValue(ph7_value *pVal)` |
|     1 |  927 | `{` |
|     - |  928 | `	phl_stream_filter *pFilter;` |
|    11 |  929 | `	if( pVal == 0 \|\| !ph7_value_is_resource(pVal) ){` |
|   ! 0 |  930 | `		return 0;` |
|     - |  931 | `	}` |
|    11 |  932 | `	pFilter = (phl_stream_filter *)ph7_value_to_resource(pVal);` |
|    11 |  933 | `	if( pFilter == 0 \|\| pFilter->base.iMagic != STREAM_FILTER_MAGIC ){` |
|     5 |  934 | `		return 0;` |
|     - |  935 | `	}` |
|     7 |  936 | `	return pFilter;` |
|     6 |  937 | `}` |
|     - |  938 | `/* Allocate one filter, chained on the VM registry so it goes back at reset. */` |
|   230 |  939 | `static phl_stream_filter * FilterNew(ph7_vm *pVm,const phl_filter_ops *pOps,` |
|     - |  940 | `	const char *zName,int nName)` |
|     4 |  941 | `{` |
|     - |  942 | `	phl_stream_filter *pFilter;` |
|   234 |  943 | `	pFilter = (phl_stream_filter *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_stream_filter));` |
|   234 |  944 | `	if( pFilter == 0 ){` |
|   ! 0 |  945 | `		return 0;` |
|     - |  946 | `	}` |
|   234 |  947 | `	SyZero(pFilter,sizeof(phl_stream_filter));` |
|   234 |  948 | `	pFilter->base.iMagic = STREAM_FILTER_MAGIC;` |
|   234 |  949 | `	pFilter->pVm = pVm;` |
|   234 |  950 | `	pFilter->pOps = pOps;` |
|   234 |  951 | `	SyBlobInit(&pFilter->sName,&pVm->sAllocator);` |
|   234 |  952 | `	SyBlobInit(&pFilter->sCarry,&pVm->sAllocator);` |
|   234 |  953 | `	if( nName > 0 ){` |
|   234 |  954 | `		SyBlobAppend(&pFilter->sName,zName,(sxu32)nName);` |
|   115 |  955 | `	}` |
|   234 |  956 | `	pFilter->pRegNext = (phl_stream_filter *)pVm->pStreamFilter;` |
|   234 |  957 | `	pVm->pStreamFilter = (void *)pFilter;` |
|   234 |  958 | `	return pFilter;` |
|   119 |  959 | `}` |
|     - |  960 | `/*` |
|     - |  961 | ` * Release one filter's own resources. The instance itself stays allocated until` |
|     - |  962 | ` * the VM resets — a ph7_value the script still holds names this pointer, and a` |
|     - |  963 | ` * probe of it has to stay in bounds — so the magic becomes the CLOSED one,` |
|     - |  964 | `` * which is what makes `is_resource($f)` false after stream_filter_remove()`` |
|     - |  965 | ` * exactly as php reports it.` |
|     - |  966 | ` */` |
|   230 |  967 | `static void FilterDispose(phl_stream_filter *pFilter)` |
|     4 |  968 | `{` |
|   234 |  969 | `	if( pFilter->pOps && pFilter->pOps->xClose ){` |
|   160 |  970 | `		pFilter->pOps->xClose(pFilter);` |
|    79 |  971 | `	}` |
|   234 |  972 | `	SyBlobRelease(&pFilter->sCarry);` |
|   234 |  973 | `	pFilter->pDev = 0;` |
|   234 |  974 | `	pFilter->pNext = 0;` |
|   234 |  975 | `	pFilter->base.iMagic = IO_PRIVATE_CLOSED_MAGIC;` |
|   234 |  976 | `}` |
|     - |  977 | `/* --------------------------------------------------------------------------` |
|     - |  978 | ` * Running a chain.` |
|     - |  979 | ` * -------------------------------------------------------------------------- */` |
|     - |  980 | `/* One filter's turn. Built-in ops run their routine; the userland half hooks in` |
|     - |  981 | ` * here when it lands. */` |
|  1472 |  982 | `static int FilterInvoke(phl_stream_filter *pFilter,phl_brigade *pIn,phl_brigade *pOut,int iFlags)` |
|     4 |  983 | `{` |
|  1476 |  984 | `	if( pFilter->pOps == 0 \|\| pFilter->pOps->xFilter == 0 ){` |
|   ! 0 |  985 | `		return PHL_PSFS_ERR_FATAL;` |
|     - |  986 | `	}` |
|  1476 |  987 | `	if( iFlags & PHL_PSFS_FLAG_FLUSH_CLOSE ){` |
|   196 |  988 | `		if( pFilter->bClosed ){` |
|     - |  989 | `			/* A filter gets exactly ONE closing call: the device's end already` |
|     - |  990 | `			 * made it, and running a buffering codec's tail a second time (a` |
|     - |  991 | `			 * stream_filter_remove() after the last read, say) would emit that` |
|     - |  992 | `			 * tail twice. Whatever arrives now simply passes through. */` |
|     - |  993 | `			phl_bucket *pBucket;` |
|   ! 0 |  994 | `			while( (pBucket = PH7_FilterBucketPop(pIn)) != 0 ){` |
|   ! 0 |  995 | `				PH7_FilterBucketAppend(pOut,pBucket);` |
|   ! 0 |  996 | `			}` |
|   ! 0 |  997 | `			return PHL_PSFS_PASS_ON;` |
|     - |  998 | `		}` |
|   196 |  999 | `		pFilter->bClosed = 1;` |
|    96 | 1000 | `	}` |
|     - | 1001 | `	{` |
|  1476 | 1002 | `		int rc = pFilter->pOps->xFilter(pFilter,pIn,pOut,iFlags);` |
|  1476 | 1003 | `		if( rc == PHL_PSFS_ERR_FATAL ){` |
|     - | 1004 | `			/* Marked, not skipped: php runs a filter that has already refused` |
|     - | 1005 | `			 * once again on the next write and reports the refusal again — what` |
|     - | 1006 | `			 * it does NOT do is run it a last time at close. */` |
|     9 | 1007 | `			pFilter->bDead = 1;` |
|     4 | 1008 | `		}` |
|  1476 | 1009 | `		return rc;` |
|     - | 1010 | `	}` |
|   740 | 1011 | `}` |
|     - | 1012 | `/*` |
|     - | 1013 | ` * iFlags describes the call for the HEAD of the chain and iRestFlags for` |
|     - | 1014 | ` * everything behind it, because the two are not always the same: the device's` |
|     - | 1015 | ` * end of file closes every filter on the stream, but flushing ONE filter — what` |
|     - | 1016 | ` * stream_filter_remove() does — closes only that one and hands its tail to the` |
|     - | 1017 | ` * others as ordinary data. Closing them too would make a codec below emit its` |
|     - | 1018 | `` * own tail early: removing an upstream `string.toupper` from a chain ending in`` |
|     - | 1019 | `` * `convert.base64-encode` padded the base64 there and then, where php leaves it`` |
|     - | 1020 | ` * mid-group.` |
|     - | 1021 | ` */` |
|  1450 | 1022 | `PH7_PRIVATE int PH7_FilterChainProcess(phl_stream_filter *pHead,` |
|     - | 1023 | `	const void *pData,sxu32 nLen,int iFlags,int iRestFlags,SyBlob *pOut,int *pbUnread)` |
|     4 | 1024 | `{` |
|  1454 | 1025 | `	ph7_vm *pVm = pHead->pVm;` |
|     - | 1026 | `	phl_brigade sA,sB;` |
|     - | 1027 | `	phl_brigade *pIn,*pOutBrig,*pSwap;` |
|     - | 1028 | `	phl_stream_filter *pFilter;` |
|     - | 1029 | `	phl_bucket *pBucket;` |
|  1454 | 1030 | `	int iStatus = PHL_PSFS_PASS_ON;` |
|  1454 | 1031 | `	SyZero(&sA,sizeof(sA));` |
|  1454 | 1032 | `	SyZero(&sB,sizeof(sB));` |
|  1454 | 1033 | `	if( nLen > 0 ){` |
|  1272 | 1034 | `		pBucket = PH7_FilterBucketNew(pVm,pData,nLen);` |
|  1272 | 1035 | `		if( pBucket == 0 ){` |
|   ! 0 | 1036 | `			return PHL_PSFS_ERR_FATAL;` |
|     - | 1037 | `		}` |
|  1272 | 1038 | `		PH7_FilterBucketAppend(&sA,pBucket);` |
|   634 | 1039 | `	}` |
|  1454 | 1040 | `	pIn = &sA;` |
|  1454 | 1041 | `	pOutBrig = &sB;` |
|  2914 | 1042 | `	for( pFilter = pHead ; pFilter ; pFilter = pFilter->pNext ){` |
|  1476 | 1043 | `		iStatus = FilterInvoke(pFilter,pIn,pOutBrig,pFilter == pHead ? iFlags : iRestFlags);` |
|  1476 | 1044 | `		if( iStatus != PHL_PSFS_PASS_ON ){` |
|    13 | 1045 | `			break;` |
|     - | 1046 | `		}` |
|     - | 1047 | `		/* Whatever the filter left behind is dropped: php warns about it from` |
|     - | 1048 | `		 * the reader ("Unprocessed filter buckets remaining on input brigade")` |
|     - | 1049 | `		 * and hands the read back as a failure, which is the ERR_FATAL path. */` |
|  1464 | 1050 | `		PH7_FilterBrigadeRelease(pVm,pIn);` |
|     - | 1051 | `		/* This filter's output is the next one's input. */` |
|  1464 | 1052 | `		pSwap = pIn;` |
|  1464 | 1053 | `		pIn = pOutBrig;` |
|  1464 | 1054 | `		pOutBrig = pSwap;` |
|   734 | 1055 | `	}` |
|  1454 | 1056 | `	if( iStatus != PHL_PSFS_PASS_ON && pIn->pHead != 0 ){` |
|     - | 1057 | `		/* A filter that gave up on its input without taking it: php says so and` |
|     - | 1058 | `		 * the READ answers FALSE rather than an end of file. A filter that` |
|     - | 1059 | `		 * consumed everything and then refused is the quiet shape. */` |
|     5 | 1060 | `		if( pbUnread ){` |
|   ! 0 | 1061 | `			*pbUnread = 1;` |
|   ! 0 | 1062 | `		}` |
|     5 | 1063 | `		PH7_VmThrowError(pVm,pVm->pCalleeName,PH7_CTX_WARNING,` |
|     - | 1064 | `			"Unprocessed filter buckets remaining on input brigade");` |
|     2 | 1065 | `	}` |
|  1454 | 1066 | `	if( iStatus == PHL_PSFS_PASS_ON && pOut ){` |
|  2140 | 1067 | `		while( (pBucket = PH7_FilterBucketPop(pIn)) != 0 ){` |
|   702 | 1068 | `			if( SyBlobLength(&pBucket->sData) > 0 ){` |
|   702 | 1069 | `				SyBlobAppend(pOut,SyBlobData(&pBucket->sData),SyBlobLength(&pBucket->sData));` |
|   349 | 1070 | `			}` |
|   702 | 1071 | `			PH7_FilterBucketFree(pVm,pBucket);` |
|     4 | 1072 | `		}` |
|   719 | 1073 | `	}` |
|  1454 | 1074 | `	PH7_FilterBrigadeRelease(pVm,&sA);` |
|  1454 | 1075 | `	PH7_FilterBrigadeRelease(pVm,&sB);` |
|  1454 | 1076 | `	return iStatus;` |
|   729 | 1077 | `}` |
|     - | 1078 | `/* --------------------------------------------------------------------------` |
|     - | 1079 | ` * Attaching, removing and releasing.` |
|     - | 1080 | ` * -------------------------------------------------------------------------- */` |
|     - | 1081 | `/* The chain head slot of a handle for one direction. */` |
| 44058 | 1082 | `static phl_stream_filter ** FilterChainSlot(io_private *pDev,int iChain)` |
|     5 | 1083 | `{` |
| 44063 | 1084 | `	if( iChain == PHL_STREAM_FILTER_WRITE ){` |
| 21972 | 1085 | `		return (phl_stream_filter **)&pDev->pWriteFilters;` |
|     - | 1086 | `	}` |
| 22096 | 1087 | `	return (phl_stream_filter **)&pDev->pReadFilters;` |
| 22007 | 1088 | `}` |
|     - | 1089 | `/* Unlink a filter from the chain it sits on. */` |
|     6 | 1090 | `static void FilterUnlink(phl_stream_filter *pFilter)` |
|     1 | 1091 | `{` |
|     - | 1092 | `	phl_stream_filter **ppSlot,*pCur;` |
|     7 | 1093 | `	if( pFilter->pDev == 0 ){` |
|   ! 0 | 1094 | `		return;` |
|     - | 1095 | `	}` |
|     7 | 1096 | `	ppSlot = FilterChainSlot(pFilter->pDev,pFilter->iChain);` |
|     7 | 1097 | `	pCur = *ppSlot;` |
|     7 | 1098 | `	if( pCur == pFilter ){` |
|     7 | 1099 | `		*ppSlot = pFilter->pNext;` |
|     7 | 1100 | `		return;` |
|     - | 1101 | `	}` |
|   ! 0 | 1102 | `	while( pCur ){` |
|   ! 0 | 1103 | `		if( pCur->pNext == pFilter ){` |
|   ! 0 | 1104 | `			pCur->pNext = pFilter->pNext;` |
|   ! 0 | 1105 | `			return;` |
|     - | 1106 | `		}` |
|   ! 0 | 1107 | `		pCur = pCur->pNext;` |
|   ! 0 | 1108 | `	}` |
|     4 | 1109 | `}` |
|     - | 1110 | `/*` |
|     - | 1111 | ` * The last call a filter ever gets. A write filter's tail has to reach the` |
|     - | 1112 | ` * device, and a read filter's has to reach the reader, so a flush is a chain` |
|     - | 1113 | ` * run from THIS filter down with no input and the closing flag.` |
|     - | 1114 | ` */` |
|    42 | 1115 | `static void FilterFlushTail(phl_stream_filter *pFilter,int iRestFlags)` |
|     3 | 1116 | `{` |
|    45 | 1117 | `	io_private *pDev = pFilter->pDev;` |
|     - | 1118 | `	phl_stream_filter *pCur;` |
|     - | 1119 | `	SyBlob sOut;` |
|    45 | 1120 | `	if( pDev == 0 ){` |
|   ! 0 | 1121 | `		return;` |
|     - | 1122 | `	}` |
|    97 | 1123 | `	for( pCur = pFilter ; pCur ; pCur = pCur->pNext ){` |
|    57 | 1124 | `		if( pCur->bDead ){` |
|     - | 1125 | `			/* A chain that already refused its input is finished: php does not` |
|     - | 1126 | `			 * run it again at close, and running it here would report the same` |
|     - | 1127 | `			 * refusal a second time from fclose(). */` |
|     3 | 1128 | `			return;` |
|     - | 1129 | `		}` |
|    29 | 1130 | `	}` |
|    43 | 1131 | `	SyBlobInit(&sOut,&pFilter->pVm->sAllocator);` |
|    40 | 1132 | `	if( PH7_FilterChainProcess(pFilter,0,0,PHL_PSFS_FLAG_FLUSH_CLOSE,iRestFlags,&sOut,0)` |
|    43 | 1133 | `	    == PHL_PSFS_PASS_ON && SyBlobLength(&sOut) > 0 ){` |
|    16 | 1134 | `		if( pFilter->iChain == PHL_STREAM_FILTER_WRITE ){` |
|    16 | 1135 | `			if( pDev->pStream && pDev->pStream->xWrite ){` |
|    23 | 1136 | `				pDev->pStream->xWrite(pDev->pHandle,SyBlobData(&sOut),` |
|    14 | 1137 | `					(ph7_int64)SyBlobLength(&sOut));` |
|     7 | 1138 | `			}` |
|     9 | 1139 | `		}else{` |
|   ! 0 | 1140 | `			SyBlobAppend(&pDev->sFilt,SyBlobData(&sOut),SyBlobLength(&sOut));` |
|     - | 1141 | `		}` |
|     7 | 1142 | `	}` |
|    43 | 1143 | `	SyBlobRelease(&sOut);` |
|    24 | 1144 | `}` |
| 21403 | 1145 | `PH7_PRIVATE void PH7_StreamFilterReleaseChains(io_private *pDev)` |
|     5 | 1146 | `{` |
|     - | 1147 | `	int i;` |
| 64214 | 1148 | `	for( i = 0 ; i < 2 ; i++ ){` |
| 42811 | 1149 | `		int iChain = i == 0 ? PHL_STREAM_FILTER_WRITE : PHL_STREAM_FILTER_READ;` |
| 42811 | 1150 | `		phl_stream_filter **ppSlot = FilterChainSlot(pDev,iChain);` |
| 42811 | 1151 | `		phl_stream_filter *pFilter = *ppSlot;` |
|     - | 1152 | `		/* The WRITE chain is flushed first and as a whole: the head's tail has` |
|     - | 1153 | `		 * to travel through the filters below it before anything reaches the` |
|     - | 1154 | `		 * device. */` |
| 42811 | 1155 | `		if( iChain == PHL_STREAM_FILTER_WRITE && pFilter ){` |
|    39 | 1156 | `			FilterFlushTail(pFilter,PHL_PSFS_FLAG_FLUSH_CLOSE);` |
|    18 | 1157 | `		}` |
| 43023 | 1158 | `		while( pFilter ){` |
|   216 | 1159 | `			phl_stream_filter *pNext = pFilter->pNext;` |
|   216 | 1160 | `			FilterDispose(pFilter);` |
|   216 | 1161 | `			pFilter = pNext;` |
|     4 | 1162 | `		}` |
| 42811 | 1163 | `		*ppSlot = 0;` |
| 21381 | 1164 | `	}` |
| 21408 | 1165 | `}` |
|   514 | 1166 | `PH7_PRIVATE void PH7_StreamFilterRewound(io_private *pDev)` |
|     4 | 1167 | `{` |
|     - | 1168 | `	int i;` |
|  1546 | 1169 | `	for( i = 0 ; i < 2 ; i++ ){` |
|  1546 | 1170 | `		phl_stream_filter *pFilter = *FilterChainSlot(pDev,` |
|   514 | 1171 | `			i == 0 ? PHL_STREAM_FILTER_READ : PHL_STREAM_FILTER_WRITE);` |
|  1060 | 1172 | `		while( pFilter ){` |
|     - | 1173 | `			/* The stream moved, so the end it had reached is not the end any` |
|     - | 1174 | `			 * more: a chain closed at the old one must be able to run — and to` |
|     - | 1175 | `			 * emit its tail — again. */` |
|    29 | 1176 | `			pFilter->bClosed = 0;` |
|    29 | 1177 | `			pFilter = pFilter->pNext;` |
|     1 | 1178 | `		}` |
|   518 | 1179 | `	}` |
|   518 | 1180 | `}` |
|    16 | 1181 | `PH7_PRIVATE void PH7_StreamFilterVmReset(ph7_vm *pVm)` |
|   ! 0 | 1182 | `{` |
|     - | 1183 | `	phl_stream_filter *pFilter;` |
|    16 | 1184 | `	if( pVm == 0 ){` |
|   ! 0 | 1185 | `		return;` |
|     - | 1186 | `	}` |
|    16 | 1187 | `	pFilter = (phl_stream_filter *)pVm->pStreamFilter;` |
|    16 | 1188 | `	while( pFilter ){` |
|   ! 0 | 1189 | `		phl_stream_filter *pNext = pFilter->pRegNext;` |
|   ! 0 | 1190 | `		if( pFilter->base.iMagic == STREAM_FILTER_MAGIC ){` |
|     - | 1191 | `			/* The std handles outlive a reset (the -S server reuses one VM), so` |
|     - | 1192 | `			 * a filter that was never removed has to leave their chain before` |
|     - | 1193 | `			 * its memory goes back — otherwise the next request's first write` |
|     - | 1194 | `			 * walks a freed one. */` |
|   ! 0 | 1195 | `			io_private *pDev = pFilter->pDev;` |
|   ! 0 | 1196 | `			FilterUnlink(pFilter);` |
|   ! 0 | 1197 | `			if( pDev ){` |
|   ! 0 | 1198 | `				SyBlobReset(&pDev->sFilt);` |
|   ! 0 | 1199 | `				pDev->nFiltOfft = 0;` |
|   ! 0 | 1200 | `				pDev->bFiltDone = 0;` |
|   ! 0 | 1201 | `			}` |
|   ! 0 | 1202 | `			FilterDispose(pFilter);` |
|   ! 0 | 1203 | `		}` |
|   ! 0 | 1204 | `		SyBlobRelease(&pFilter->sName);` |
|   ! 0 | 1205 | `		pFilter->base.iMagic = 0;` |
|   ! 0 | 1206 | `		SyMemBackendFree(&pVm->sAllocator,pFilter);` |
|   ! 0 | 1207 | `		pFilter = pNext;` |
|   ! 0 | 1208 | `	}` |
|    16 | 1209 | `	pVm->pStreamFilter = 0;` |
|     - | 1210 | `	{` |
|    16 | 1211 | `		phl_ufilter_reg *pReg = (phl_ufilter_reg *)pVm->pUserFilters;` |
|    16 | 1212 | `		while( pReg ){` |
|   ! 0 | 1213 | `			phl_ufilter_reg *pNext = pReg->pNext;` |
|   ! 0 | 1214 | `			SyBlobRelease(&pReg->sName);` |
|   ! 0 | 1215 | `			SyBlobRelease(&pReg->sClass);` |
|   ! 0 | 1216 | `			SyMemBackendFree(&pVm->sAllocator,pReg);` |
|   ! 0 | 1217 | `			pReg = pNext;` |
|   ! 0 | 1218 | `		}` |
|    16 | 1219 | `		pVm->pUserFilters = 0;` |
|     - | 1220 | `	}` |
|    16 | 1221 | `	pVm->pFilterCall = 0;` |
|     8 | 1222 | `}` |
|     - | 1223 | `/* php's own two diagnostics, worded from the builtin that is running — which is` |
|     - | 1224 | `` * `stream_filter_append` on one path and the READER (file_get_contents, fopen)`` |
|     - | 1225 | ` * on the php://filter one. */` |
|    26 | 1226 | `static void FilterWarn(ph7_vm *pVm,const char *zFmt,int nName,const char *zName)` |
|     2 | 1227 | `{` |
|     - | 1228 | `	char zMsg[160];` |
|    28 | 1229 | `	SyBufferFormat(zMsg,sizeof(zMsg),zFmt,nName,zName);` |
|    28 | 1230 | `	PH7_VmThrowError(pVm,pVm->pCalleeName,PH7_CTX_WARNING,zMsg);` |
|    28 | 1231 | `}` |
|   244 | 1232 | `PH7_PRIVATE phl_stream_filter * PH7_StreamFilterAttach(ph7_vm *pVm,io_private *pDev,` |
|     - | 1233 | `	const char *zName,int nName,int iChain,int bPrepend,ph7_value *pParams,` |
|     - | 1234 | `	ph7_value *pStreamVal)` |
|     4 | 1235 | `{` |
|     - | 1236 | `	const phl_filter_ops *pOps;` |
|   248 | 1237 | `	phl_ufilter_reg *pReg = 0;` |
|     - | 1238 | `	phl_stream_filter *pFilter;` |
|   248 | 1239 | `	pOps = FilterFindOps(zName,nName);` |
|   248 | 1240 | `	if( pOps == 0 ){` |
|     - | 1241 | `		/* Nothing built in answers to it; a script may have registered one. */` |
|    39 | 1242 | `		pReg = UserFilterFind(pVm,zName,nName);` |
|    39 | 1243 | `		if( pReg == 0 ){` |
|    13 | 1244 | `			FilterWarn(pVm,"Unable to locate filter \"%.*s\"",nName,zName);` |
|    13 | 1245 | `			return 0;` |
|     - | 1246 | `		}` |
|    27 | 1247 | `		pFilter = UserFilterCreate(pVm,pReg,zName,nName,pParams,pStreamVal);` |
|    27 | 1248 | `		if( pFilter == 0 ){` |
|     5 | 1249 | `			FilterWarn(pVm,"Unable to create or locate filter \"%.*s\"",nName,zName);` |
|     5 | 1250 | `			return 0;` |
|     - | 1251 | `		}` |
|    23 | 1252 | `		goto attach;` |
|     - | 1253 | `	}` |
|   210 | 1254 | `	pFilter = FilterNew(pVm,pOps,zName,nName);` |
|   210 | 1255 | `	if( pFilter == 0 ){` |
|   ! 0 | 1256 | `		PH7_VmThrowError(pVm,pVm->pCalleeName,PH7_CTX_ERR,"PH7 is running out of memory");` |
|   ! 0 | 1257 | `		return 0;` |
|     - | 1258 | `	}` |
|   210 | 1259 | `	if( pOps->xCreate && pOps->xCreate(pFilter,pParams) != PH7_OK ){` |
|    12 | 1260 | `		FilterDispose(pFilter);` |
|    12 | 1261 | `		FilterWarn(pVm,"Unable to create or locate filter \"%.*s\"",nName,zName);` |
|    12 | 1262 | `		return 0;` |
|     - | 1263 | `	}` |
|    98 | 1264 | `attach:` |
|   222 | 1265 | `	pFilter->pDev = pDev;` |
|   222 | 1266 | `	pFilter->iChain = iChain;` |
|   222 | 1267 | `	if( bPrepend ){` |
|     3 | 1268 | `		phl_stream_filter **ppSlot = FilterChainSlot(pDev,iChain);` |
|     3 | 1269 | `		pFilter->pNext = *ppSlot;` |
|     3 | 1270 | `		*ppSlot = pFilter;` |
|     2 | 1271 | `	}else{` |
|   220 | 1272 | `		phl_stream_filter **ppSlot = FilterChainSlot(pDev,iChain);` |
|   220 | 1273 | `		phl_stream_filter *pCur = *ppSlot;` |
|   220 | 1274 | `		if( pCur == 0 ){` |
|   206 | 1275 | `			*ppSlot = pFilter;` |
|   105 | 1276 | `		}else{` |
|    22 | 1277 | `			while( pCur->pNext ){` |
|     7 | 1278 | `				pCur = pCur->pNext;` |
|     1 | 1279 | `			}` |
|    16 | 1280 | `			pCur->pNext = pFilter;` |
|     - | 1281 | `		}` |
|     - | 1282 | `	}` |
|   222 | 1283 | `	return pFilter;` |
|   126 | 1284 | `}` |
|     - | 1285 | `/* --------------------------------------------------------------------------` |
|     - | 1286 | ` * The builtins.` |
|     - | 1287 | ` * -------------------------------------------------------------------------- */` |
|     - | 1288 | `/*` |
|     - | 1289 | ` * resource\|false stream_filter_append(resource $stream, string $filter_name,` |
|     - | 1290 | ` *                                     int $mode = 0, mixed $params = null)` |
|     - | 1291 | ` * resource\|false stream_filter_prepend(...)` |
|     - | 1292 | ` *` |
|     - | 1293 | ` * php's $mode of 0 is not "no chain": it means "whichever chains this handle's` |
|     - | 1294 | `` * MODE makes sense for", so a stream opened `r+` gets the filter on BOTH — two`` |
|     - | 1295 | ` * separate instances, since a filter carries state and one cannot serve two` |
|     - | 1296 | ` * directions. The resource answered is the LAST one created, which is why` |
|     - | 1297 | `` * removing what `stream_filter_append($h,'…')` gave back on an `r+` handle`` |
|     - | 1298 | ` * leaves the READ half of it still filtering.` |
|     - | 1299 | ` */` |
|   196 | 1300 | `static int StreamFilterAddCommon(ph7_context *pCtx,int nArg,ph7_value **apArg,int bPrepend)` |
|     3 | 1301 | `{` |
|   199 | 1302 | `	phl_stream_filter *pFilter = 0;` |
|     - | 1303 | `	ph7_value *pParams;` |
|     - | 1304 | `	io_private *pDev;` |
|     - | 1305 | `	const char *zName;` |
|     - | 1306 | `	int nName,iChain,rc;` |
|   199 | 1307 | `	pDev = PH7_StreamHandleArg(pCtx,apArg[0],1,"stream",&rc);` |
|   199 | 1308 | `	if( pDev == 0 ){` |
|   ! 0 | 1309 | `		return rc;` |
|     - | 1310 | `	}` |
|   199 | 1311 | `	zName = ph7_value_to_string(apArg[1],&nName);` |
|   199 | 1312 | `	iChain = nArg > 2 ? (int)ph7_value_to_int(apArg[2]) : 0;` |
|   199 | 1313 | `	pParams = nArg > 3 ? apArg[3] : 0;` |
|   199 | 1314 | `	if( iChain == 0 ){` |
|     - | 1315 | `		/* php reads the mode the handle was OPENED with. */` |
|    27 | 1316 | `		const char *zMode = pDev->zMode;` |
|     - | 1317 | `		sxu32 nDummy;` |
|    27 | 1318 | `		int bPlus = SyByteFind(zMode,SyStrlen(zMode),'+',&nDummy) == SXRET_OK;` |
|    27 | 1319 | `		switch( zMode[0] ){` |
|    10 | 1320 | `		case 'r':` |
|    22 | 1321 | `			iChain = bPlus ? PHL_STREAM_FILTER_ALL : PHL_STREAM_FILTER_READ;` |
|    22 | 1322 | `			break;` |
|     2 | 1323 | `		case 'w':` |
|     - | 1324 | `		case 'a':` |
|     - | 1325 | `		case 'x':` |
|     - | 1326 | `		case 'c':` |
|     5 | 1327 | `			iChain = bPlus ? PHL_STREAM_FILTER_ALL : PHL_STREAM_FILTER_WRITE;` |
|     4 | 1328 | `			break;` |
|   ! 0 | 1329 | `		default:` |
|   ! 0 | 1330 | `			break;` |
|     - | 1331 | `		}` |
|    12 | 1332 | `	}` |
|   199 | 1333 | `	if( iChain & PHL_STREAM_FILTER_READ ){` |
|   237 | 1334 | `		pFilter = PH7_StreamFilterAttach(pCtx->pVm,pDev,zName,nName,PHL_STREAM_FILTER_READ,` |
|    78 | 1335 | `			bPrepend,pParams,apArg[0]);` |
|   159 | 1336 | `		if( pFilter == 0 ){` |
|    22 | 1337 | `			ph7_result_bool(pCtx,0);` |
|    22 | 1338 | `			return PH7_OK;` |
|     - | 1339 | `		}` |
|    68 | 1340 | `	}` |
|   179 | 1341 | `	if( iChain & PHL_STREAM_FILTER_WRITE ){` |
|    69 | 1342 | `		pFilter = PH7_StreamFilterAttach(pCtx->pVm,pDev,zName,nName,PHL_STREAM_FILTER_WRITE,` |
|    22 | 1343 | `			bPrepend,pParams,apArg[0]);` |
|    47 | 1344 | `		if( pFilter == 0 ){` |
|     3 | 1345 | `			ph7_result_bool(pCtx,0);` |
|     3 | 1346 | `			return PH7_OK;` |
|     - | 1347 | `		}` |
|    21 | 1348 | `	}` |
|   177 | 1349 | `	if( pFilter == 0 ){` |
|     - | 1350 | `		/* A mode this engine could not place the filter on. */` |
|   ! 0 | 1351 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1352 | `		return PH7_OK;` |
|     - | 1353 | `	}` |
|   177 | 1354 | `	ph7_result_resource(pCtx,pFilter);` |
|   177 | 1355 | `	return PH7_OK;` |
|   101 | 1356 | `}` |
|   194 | 1357 | `PH7_PRIVATE int PH7_builtin_stream_filter_append(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 | 1358 | `{` |
|   197 | 1359 | `	return StreamFilterAddCommon(pCtx,nArg,apArg,0);` |
|     3 | 1360 | `}` |
|     2 | 1361 | `PH7_PRIVATE int PH7_builtin_stream_filter_prepend(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1362 | `{` |
|     3 | 1363 | `	return StreamFilterAddCommon(pCtx,nArg,apArg,1);` |
|     1 | 1364 | `}` |
|     - | 1365 | `/*` |
|     - | 1366 | ` * bool stream_filter_remove(resource $stream_filter)` |
|     - | 1367 | ` *` |
|     - | 1368 | ` * php FLUSHES the filter on the way out — a write filter's tail still reaches` |
|     - | 1369 | ` * the device and a read filter's still reaches the reader — and then the` |
|     - | 1370 | ` * resource is dead: passing it again is a TypeError, not FALSE.` |
|     - | 1371 | ` */` |
|    12 | 1372 | `PH7_PRIVATE int PH7_builtin_stream_filter_remove(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1373 | `{` |
|     - | 1374 | `	char zGiven[64];` |
|     - | 1375 | `	phl_stream_filter *pFilter;` |
|     6 | 1376 | `	SXUNUSED(nArg);` |
|    13 | 1377 | `	if( !ph7_value_is_resource(apArg[0]) ){` |
|     - | 1378 | `		/* php's ZPP runs first: a string is not "the wrong resource", it is not` |
|     - | 1379 | `		 * a resource at all, and the two diagnostics are different. */` |
|     4 | 1380 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 1381 | `			"%s(): Argument #1 ($stream_filter) must be of type resource, %s given",` |
|     1 | 1382 | `			ph7_function_name(pCtx),VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));` |
|     - | 1383 | `	}` |
|    11 | 1384 | `	pFilter = PH7_StreamFilterFromValue(apArg[0]);` |
|    11 | 1385 | `	if( pFilter == 0 ){` |
|     7 | 1386 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 1387 | `			"%s(): supplied resource is not a valid stream filter resource",` |
|     2 | 1388 | `			ph7_function_name(pCtx));` |
|     - | 1389 | `	}` |
|     - | 1390 | `	/* Only THIS filter closes; what it emits travels through the rest of the` |
|     - | 1391 | `	 * chain as ordinary data, because those filters stay on the stream. */` |
|     7 | 1392 | `	FilterFlushTail(pFilter,PHL_PSFS_FLAG_NORMAL);` |
|     7 | 1393 | `	FilterUnlink(pFilter);` |
|     7 | 1394 | `	FilterDispose(pFilter);` |
|     7 | 1395 | `	ph7_result_bool(pCtx,1);` |
|     7 | 1396 | `	return PH7_OK;` |
|     7 | 1397 | `}` |
|     - | 1398 | `/*` |
|     - | 1399 | ` * array stream_get_filters(void)` |
|     - | 1400 | ` *  The filter names this build can create, in php's own registration order.` |
|     - | 1401 | ` */` |
|    10 | 1402 | `PH7_PRIVATE int PH7_builtin_stream_get_filters(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 | 1403 | `{` |
|     - | 1404 | `	ph7_value *pArray,*pValue;` |
|     - | 1405 | `	sxu32 n;` |
|     5 | 1406 | `	SXUNUSED(nArg);` |
|     5 | 1407 | `	SXUNUSED(apArg);` |
|    13 | 1408 | `	pArray = ph7_context_new_array(pCtx);` |
|    13 | 1409 | `	pValue = ph7_context_new_scalar(pCtx);` |
|    13 | 1410 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|   ! 0 | 1411 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|   ! 0 | 1412 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1413 | `		return PH7_OK;` |
|     - | 1414 | `	}` |
|    73 | 1415 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltinFilters) ; ++n ){` |
|    63 | 1416 | `		ph7_value_string(pValue,aBuiltinFilters[n].zName,-1);` |
|    63 | 1417 | `		ph7_array_add_elem(pArray,0,pValue);` |
|    63 | 1418 | `		ph7_value_reset_string_cursor(pValue);` |
|    33 | 1419 | `	}` |
|     - | 1420 | `	{` |
|     - | 1421 | `		/* And whatever the script registered, newest last — php lists them` |
|     - | 1422 | `		 * beside its own. */` |
|     - | 1423 | `		phl_ufilter_reg *pReg;` |
|     - | 1424 | `		SySet aName;` |
|     - | 1425 | `		sxu32 i;` |
|    13 | 1426 | `		SySetInit(&aName,&pCtx->pVm->sAllocator,sizeof(phl_ufilter_reg *));` |
|    15 | 1427 | `		for( pReg = (phl_ufilter_reg *)pCtx->pVm->pUserFilters ; pReg ; pReg = pReg->pNext ){` |
|     3 | 1428 | `			SySetPut(&aName,(const void *)&pReg);` |
|     2 | 1429 | `		}` |
|    15 | 1430 | `		for( i = SySetUsed(&aName) ; i > 0 ; --i ){` |
|     3 | 1431 | `			phl_ufilter_reg **ppReg = (phl_ufilter_reg **)SySetAt(&aName,i-1);` |
|     4 | 1432 | `			ph7_value_string(pValue,(const char *)SyBlobData(&(*ppReg)->sName),` |
|     2 | 1433 | `				(int)SyBlobLength(&(*ppReg)->sName));` |
|     3 | 1434 | `			ph7_array_add_elem(pArray,0,pValue);` |
|     3 | 1435 | `			ph7_value_reset_string_cursor(pValue);` |
|     2 | 1436 | `		}` |
|    13 | 1437 | `		SySetRelease(&aName);` |
|     - | 1438 | `	}` |
|    13 | 1439 | `	ph7_result_value(pCtx,pArray);` |
|    13 | 1440 | `	return PH7_OK;` |
|     8 | 1441 | `}` |
|     - | 1442 | `/*` |
|     - | 1443 | ` * ---------------------------------------------------------------------------` |
|     - | 1444 | ` * php://filter/…/resource=… — the URL form of the same chain.` |
|     - | 1445 | ` *` |
|     - | 1446 | `` * The path after `filter/` is a list of `/`-separated segments: `read=a\|b` and`` |
|     - | 1447 | `` * `write=a\|b` name one chain each, and a bare `a\|b` names both (as far as the`` |
|     - | 1448 | ` * OPEN MODE allows — a read filter on a write-only handle is dropped). What php` |
|     - | 1449 | ` * does with the RESOURCE is worth spelling out, because it is not a clean split:` |
|     - | 1450 | `` * it looks for `/resource=` and truncates the list there, and when the path`` |
|     - | 1451 | `` * BEGINS with `resource=` — no slash before it — it takes the resource and`` |
|     - | 1452 | ` * leaves the list alone, so every segment of the resource path is then tried as` |
|     - | 1453 | `` * a filter name too. `php://filter/resource=/tmp/x` really does warn about`` |
|     - | 1454 | `` * `resource=`, `tmp` and `x` and then open the file.`` |
|     - | 1455 | ` * ---------------------------------------------------------------------------` |
|     - | 1456 | ` */` |
|    42 | 1457 | `static void FilterUrlOne(ph7_vm *pVm,io_private *pDev,const char *zList,int nList,int iChains)` |
|     2 | 1458 | `{` |
|    44 | 1459 | `	int i = 0;` |
|    88 | 1460 | `	while( i < nList ){` |
|    46 | 1461 | `		int j = i;` |
|   656 | 1462 | `		while( j < nList && zList[j] != '\|' ){` |
|   612 | 1463 | `			j++;` |
|     2 | 1464 | `		}` |
|    46 | 1465 | `		if( j > i ){` |
|    46 | 1466 | `			int bOk = 1;` |
|    46 | 1467 | `			if( iChains & PHL_STREAM_FILTER_READ ){` |
|    59 | 1468 | `				bOk = PH7_StreamFilterAttach(pVm,pDev,&zList[i],j-i,` |
|    38 | 1469 | `					PHL_STREAM_FILTER_READ,0,0,0) != 0;` |
|    19 | 1470 | `			}` |
|    46 | 1471 | `			if( bOk && (iChains & PHL_STREAM_FILTER_WRITE) ){` |
|    10 | 1472 | `				bOk = PH7_StreamFilterAttach(pVm,pDev,&zList[i],j-i,` |
|     6 | 1473 | `					PHL_STREAM_FILTER_WRITE,0,0,0) != 0;` |
|     3 | 1474 | `			}` |
|    46 | 1475 | `			if( !bOk ){` |
|     - | 1476 | `				/* The URL form says it TWICE: once about the name and once about` |
|     - | 1477 | `				 * the chain it could not be put on. The open still succeeds —` |
|     - | 1478 | `				 * php opens the resource with the filters it could make. */` |
|     - | 1479 | `				char zMsg[160];` |
|     7 | 1480 | `				SyBufferFormat(zMsg,sizeof(zMsg),"Unable to create filter (%.*s)",` |
|     2 | 1481 | `					j-i,&zList[i]);` |
|     5 | 1482 | `				PH7_VmThrowError(pVm,pVm->pCalleeName,PH7_CTX_WARNING,zMsg);` |
|     2 | 1483 | `			}` |
|    22 | 1484 | `		}` |
|    46 | 1485 | `		i = j + 1;` |
|     2 | 1486 | `	}` |
|    44 | 1487 | `}` |
|    44 | 1488 | `PH7_PRIVATE int PH7_StreamFilterParseUrl(ph7_vm *pVm,const char *zSpec,int nSpec,` |
|     - | 1489 | `	io_private *pDev,int iChains)` |
|     2 | 1490 | `{` |
|    46 | 1491 | `	int i = 0;` |
|    92 | 1492 | `	while( i < nSpec ){` |
|    48 | 1493 | `		int j = i,iWant = iChains;` |
|     - | 1494 | `		const char *zList;` |
|     - | 1495 | `		int nName;` |
|   906 | 1496 | `		while( j < nSpec && zSpec[j] != '/' ){` |
|   860 | 1497 | `			j++;` |
|     2 | 1498 | `		}` |
|    48 | 1499 | `		zList = &zSpec[i];` |
|    48 | 1500 | `		nName = j - i;` |
|    48 | 1501 | `		if( nName >= 5 && SyMemcmp(zList,"read=",5) == 0 ){` |
|    36 | 1502 | `			iWant = iChains & PHL_STREAM_FILTER_READ;` |
|    36 | 1503 | `			zList += 5;` |
|    36 | 1504 | `			nName -= 5;` |
|    30 | 1505 | `		}else if( nName >= 6 && SyMemcmp(zList,"write=",6) == 0 ){` |
|     9 | 1506 | `			iWant = iChains & PHL_STREAM_FILTER_WRITE;` |
|     9 | 1507 | `			zList += 6;` |
|     9 | 1508 | `			nName -= 6;` |
|     4 | 1509 | `		}` |
|    48 | 1510 | `		if( nName > 0 && iWant != 0 ){` |
|    44 | 1511 | `			FilterUrlOne(pVm,pDev,zList,nName,iWant);` |
|    21 | 1512 | `		}` |
|    48 | 1513 | `		i = j + 1;` |
|     2 | 1514 | `	}` |
|    46 | 1515 | `	return PH7_OK;` |
|     2 | 1516 | `}` |
|     - | 1517 |  |
|     - | 1518 | `/*` |
|     - | 1519 | ` * ---------------------------------------------------------------------------` |
|     - | 1520 | ` * Userland filters: stream_filter_register(), php_user_filter and the buckets.` |
|     - | 1521 | ` *` |
|     - | 1522 | ` * A userland filter is a CLASS, not a function: php instantiates it once per` |
|     - | 1523 | ` * attachment, tells it what name it was created under and what params it was` |
|     - | 1524 | ` * given, and then calls filter($in,$out,&$consumed,$closing) with two BRIGADE` |
|     - | 1525 | `` * handles. The script walks `$in` with stream_bucket_make_writeable(), which`` |
|     - | 1526 | ` * hands over one bucket at a time as a StreamBucket object, and appends what it` |
|     - | 1527 | `` * made to `$out`. What it RETURNS is the chain's answer: PSFS_PASS_ON,`` |
|     - | 1528 | ` * PSFS_FEED_ME or PSFS_ERR_FATAL.` |
|     - | 1529 | ` *` |
|     - | 1530 | ``  * The bucket the script sees is a VALUE — its bytes live in the object's `data` `` |
|     - | 1531 | ` * property, which the script may replace outright — so the C bucket ends at` |
|     - | 1532 | ` * make_writeable and stream_bucket_append() builds a new one from whatever the` |
|     - | 1533 | `` * object holds when it is appended. `$bucket->bucket` is the handle php shows`` |
|     - | 1534 | ` * there; it is a token owned by the call, and it goes back with it.` |
|     - | 1535 | ` * ---------------------------------------------------------------------------` |
|     - | 1536 | ` */` |
|     - | 1537 | ``/* The `bucket` handle a StreamBucket carries. It names nothing the engine reads`` |
|     - | 1538 | ` * back — the bytes are in the object — and exists because php shows one. */` |
|     - | 1539 | `typedef struct phl_bucket_tok phl_bucket_tok;` |
|     - | 1540 | `struct phl_bucket_tok` |
|     - | 1541 | `{` |
|     - | 1542 | `	io_private base;           /* resource header (base.iMagic == STREAM_BUCKET_MAGIC) */` |
|     - | 1543 | `	phl_bucket_tok *pNext;` |
|     - | 1544 | `};` |
|     - | 1545 | `/* The registration behind a name, php's own lookup: the exact name, then` |
|     - | 1546 | `` * progressively shorter `prefix.*` wildcards. */`` |
|    38 | 1547 | `static phl_ufilter_reg * UserFilterFind(ph7_vm *pVm,const char *zName,int nName)` |
|     1 | 1548 | `{` |
|     - | 1549 | `	phl_ufilter_reg *pReg;` |
|     - | 1550 | `	char zWild[128];` |
|     - | 1551 | `	int nTry;` |
|    63 | 1552 | `	for( pReg = (phl_ufilter_reg *)pVm->pUserFilters ; pReg ; pReg = pReg->pNext ){` |
|    48 | 1553 | `		if( (int)SyBlobLength(&pReg->sName) == nName` |
|    38 | 1554 | `		 && SyMemcmp(SyBlobData(&pReg->sName),zName,(sxu32)nName) == 0 ){` |
|    25 | 1555 | `			return pReg;` |
|     - | 1556 | `		}` |
|    13 | 1557 | `	}` |
|    15 | 1558 | `	nTry = nName;` |
|    13 | 1559 | `	for(;;){` |
|   151 | 1560 | `		while( nTry > 0 && zName[nTry-1] != '.' ){` |
|   125 | 1561 | `			nTry--;` |
|     1 | 1562 | `		}` |
|    27 | 1563 | `		if( nTry < 1 ){` |
|    13 | 1564 | `			break;` |
|     - | 1565 | `		}` |
|    15 | 1566 | `		if( nTry + 1 < (int)sizeof(zWild) ){` |
|    15 | 1567 | `			SyMemcpy(zName,zWild,(sxu32)nTry);` |
|    15 | 1568 | `			zWild[nTry] = '*';` |
|    35 | 1569 | `			for( pReg = (phl_ufilter_reg *)pVm->pUserFilters ; pReg ; pReg = pReg->pNext ){` |
|    22 | 1570 | `				if( (int)SyBlobLength(&pReg->sName) == nTry + 1` |
|    14 | 1571 | `				 && SyMemcmp(SyBlobData(&pReg->sName),zWild,(sxu32)(nTry+1)) == 0 ){` |
|     3 | 1572 | `					return pReg;` |
|     - | 1573 | `				}` |
|    11 | 1574 | `			}` |
|     6 | 1575 | `		}` |
|    13 | 1576 | `		nTry--;` |
|     1 | 1577 | `	}` |
|    13 | 1578 | `	return 0;` |
|    20 | 1579 | `}` |
|     - | 1580 | `/* Call one of the three methods on the filter's instance. */` |
|    86 | 1581 | `static int UserFilterCall(phl_stream_filter *pFilter,const char *zMethod,int nArg,` |
|     - | 1582 | `	ph7_value **apArg,ph7_value *pResult)` |
|     1 | 1583 | `{` |
|    87 | 1584 | `	ph7_class_instance *pObj = (ph7_class_instance *)pFilter->pObj;` |
|     - | 1585 | `	ph7_class_method *pMeth;` |
|    87 | 1586 | `	if( pObj == 0 ){` |
|   ! 0 | 1587 | `		return -1;` |
|     - | 1588 | `	}` |
|    87 | 1589 | `	pMeth = PH7_ClassExtractMethod(pObj->pClass,zMethod,(sxu32)SyStrlen(zMethod));` |
|    87 | 1590 | `	if( pMeth == 0 ){` |
|     - | 1591 | `		/* php requires nothing of the class but the name: a class that does not` |
|     - | 1592 | `		 * extend php_user_filter and declares none of the three is registered` |
|     - | 1593 | `		 * and attached without complaint, and only the missing filter() is ever` |
|     - | 1594 | `		 * noticed — at the READ. */` |
|   ! 0 | 1595 | `		return 1;` |
|     - | 1596 | `	}` |
|    87 | 1597 | `	if( PH7_VmCallClassMethod(pFilter->pVm,pObj,pMeth,pResult,nArg,apArg) != SXRET_OK ){` |
|   ! 0 | 1598 | `		return -1;` |
|     - | 1599 | `	}` |
|    87 | 1600 | `	return 0;` |
|    44 | 1601 | `}` |
|    24 | 1602 | `static void UserFilterClose(phl_stream_filter *pFilter)` |
|     1 | 1603 | `{` |
|     - | 1604 | `	ph7_value sRet;` |
|    25 | 1605 | `	if( pFilter->pObj ){` |
|    23 | 1606 | `		PH7_MemObjInit(pFilter->pVm,&sRet);` |
|    23 | 1607 | `		UserFilterCall(pFilter,"onClose",0,0,&sRet);` |
|    23 | 1608 | `		PH7_MemObjRelease(&sRet);` |
|     - | 1609 | `		/* The instance was created here and is held by nothing else. */` |
|    23 | 1610 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pFilter->pObj);` |
|    23 | 1611 | `		pFilter->pObj = 0;` |
|    11 | 1612 | `	}` |
|    25 | 1613 | `	if( pFilter->pStreamRes ){` |
|    25 | 1614 | `		ph7_release_value(pFilter->pVm,pFilter->pStreamRes);` |
|    25 | 1615 | `		pFilter->pStreamRes = 0;` |
|    12 | 1616 | `	}` |
|    25 | 1617 | `	pFilter->sIn.pBrig = 0;` |
|    25 | 1618 | `	pFilter->sOut.pBrig = 0;` |
|    25 | 1619 | `}` |
|     - | 1620 | `/* Build a brigade handle for one filter() call. */` |
|    80 | 1621 | `static void UserBrigadeInit(phl_brigade_res *pRes,ph7_vm *pVm,phl_brigade *pBrig)` |
|     1 | 1622 | `{` |
|    81 | 1623 | `	pRes->base.iMagic = STREAM_BRIGADE_MAGIC;` |
|    81 | 1624 | `	pRes->pVm = pVm;` |
|    81 | 1625 | `	pRes->pBrig = pBrig;` |
|    81 | 1626 | `}` |
|     - | 1627 | `/* The brigade behind the handle goes away with the call; the handle itself` |
|     - | 1628 | ` * stays in bounds, so a script that kept one simply finds it empty. */` |
|    80 | 1629 | `static void UserBrigadeDetach(phl_brigade_res *pRes)` |
|     1 | 1630 | `{` |
|    81 | 1631 | `	pRes->pBrig = 0;` |
|    81 | 1632 | `}` |
|    74 | 1633 | `static phl_brigade_res * UserBrigadeFromValue(ph7_value *pVal)` |
|     1 | 1634 | `{` |
|     - | 1635 | `	phl_brigade_res *pRes;` |
|    75 | 1636 | `	if( pVal == 0 \|\| !ph7_value_is_resource(pVal) ){` |
|   ! 0 | 1637 | `		return 0;` |
|     - | 1638 | `	}` |
|    75 | 1639 | `	pRes = (phl_brigade_res *)ph7_value_to_resource(pVal);` |
|    75 | 1640 | `	if( pRes == 0 \|\| pRes->base.iMagic != STREAM_BRIGADE_MAGIC ){` |
|   ! 0 | 1641 | `		return 0;` |
|     - | 1642 | `	}` |
|    75 | 1643 | `	return pRes;` |
|    38 | 1644 | `}` |
|     - | 1645 | `/* One StreamBucket object around a run of bytes, with the token php shows. */` |
|    24 | 1646 | `static ph7_class_instance * UserBucketObject(ph7_vm *pVm,const char *zData,int nData)` |
|     1 | 1647 | `{` |
|     - | 1648 | `	ph7_class *pClass;` |
|     - | 1649 | `	ph7_class_instance *pObj;` |
|     - | 1650 | `	phl_bucket_tok *pTok;` |
|     - | 1651 | `	ph7_value *pSlot;` |
|    25 | 1652 | `	pClass = PH7_VmExtractClass(pVm,"StreamBucket",sizeof("StreamBucket")-1,FALSE,0);` |
|    25 | 1653 | `	pObj = pClass ? PH7_NewClassInstance(pVm,pClass) : 0;` |
|    25 | 1654 | `	if( pObj == 0 ){` |
|   ! 0 | 1655 | `		return 0;` |
|     - | 1656 | `	}` |
|    25 | 1657 | `	pTok = (phl_bucket_tok *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_bucket_tok));` |
|    25 | 1658 | `	if( pTok ){` |
|    25 | 1659 | `		SyZero(pTok,sizeof(*pTok));` |
|    25 | 1660 | `		pTok->base.iMagic = STREAM_BUCKET_MAGIC;` |
|    25 | 1661 | `		pSlot = PH7_NativeAttr(pObj,"bucket");` |
|    25 | 1662 | `		if( pSlot ){` |
|    25 | 1663 | `			PH7_MemObjRelease(pSlot);` |
|    25 | 1664 | `			pSlot->x.pOther = (void *)pTok;` |
|    25 | 1665 | `			pSlot->iFlags = MEMOBJ_RES;` |
|    12 | 1666 | `		}` |
|    12 | 1667 | `	}` |
|    25 | 1668 | `	PH7_NativeSetAttrStr(pVm,pObj,"data",zData,nData);` |
|    25 | 1669 | `	PH7_NativeSetAttrInt(pVm,pObj,"datalen",(sxi64)nData);` |
|    25 | 1670 | `	PH7_NativeSetAttrInt(pVm,pObj,"dataLength",(sxi64)nData);` |
|    25 | 1671 | `	return pObj;` |
|    13 | 1672 | `}` |
|     - | 1673 | `/* The token goes back with the object that owns it — which is what keeps` |
|     - | 1674 | `` * `$bucket->bucket` in bounds for as long as the script holds the bucket. */`` |
|    24 | 1675 | `static void UserBucketRelease(ph7_vm *pVm,ph7_class_instance *pObj)` |
|     1 | 1676 | `{` |
|    25 | 1677 | `	ph7_value *pSlot = PH7_NativeAttr(pObj,"bucket");` |
|    25 | 1678 | `	if( pSlot && (pSlot->iFlags & MEMOBJ_RES) && pSlot->x.pOther ){` |
|    23 | 1679 | `		phl_bucket_tok *pTok = (phl_bucket_tok *)pSlot->x.pOther;` |
|    23 | 1680 | `		if( pTok->base.iMagic == STREAM_BUCKET_MAGIC ){` |
|    23 | 1681 | `			pTok->base.iMagic = 0;` |
|    23 | 1682 | `			SyMemBackendFree(&pVm->sAllocator,pTok);` |
|    11 | 1683 | `		}` |
|    23 | 1684 | `		pSlot->x.pOther = 0;` |
|    23 | 1685 | `		pSlot->iFlags = MEMOBJ_NULL;` |
|    11 | 1686 | `	}` |
|    25 | 1687 | `}` |
|     - | 1688 | `/*` |
|     - | 1689 | ` * The filter() call itself. php hands over four arguments — the two brigades,` |
|     - | 1690 | ` * a by-reference $consumed that arrives as NULL, and whether this is the last` |
|     - | 1691 | ` * call — and reads the answer as one of the PSFS_* codes.` |
|     - | 1692 | ` */` |
|    40 | 1693 | `static int UserFilterRun(phl_stream_filter *pFilter,phl_brigade *pIn,phl_brigade *pOut,int iFlags)` |
|     1 | 1694 | `{` |
|    41 | 1695 | `	ph7_vm *pVm = pFilter->pVm;` |
|     - | 1696 | `	ph7_value *apArg[4];` |
|     - | 1697 | `	ph7_value sRet;` |
|     - | 1698 | `	void *pSavedCall;` |
|    41 | 1699 | `	sxu32 nConsumedIdx = SXU32_HIGH;` |
|     - | 1700 | `	int i,rc,iStatus;` |
|    41 | 1701 | `	if( pFilter->pObj == 0 ){` |
|   ! 0 | 1702 | `		return PHL_PSFS_ERR_FATAL;` |
|     - | 1703 | `	}` |
|    41 | 1704 | `	UserBrigadeInit(&pFilter->sIn,pVm,pIn);` |
|    41 | 1705 | `	UserBrigadeInit(&pFilter->sOut,pVm,pOut);` |
|   201 | 1706 | `	for( i = 0 ; i < 4 ; i++ ){` |
|   161 | 1707 | `		apArg[i] = ph7_new_scalar(pVm);` |
|    81 | 1708 | `	}` |
|    41 | 1709 | `	if( apArg[0] == 0 \|\| apArg[1] == 0 \|\| apArg[2] == 0 \|\| apArg[3] == 0 ){` |
|   ! 0 | 1710 | `		for( i = 0 ; i < 4 ; i++ ){` |
|   ! 0 | 1711 | `			if( apArg[i] ){` |
|   ! 0 | 1712 | `				ph7_release_value(pVm,apArg[i]);` |
|   ! 0 | 1713 | `			}` |
|   ! 0 | 1714 | `		}` |
|   ! 0 | 1715 | `		UserBrigadeDetach(&pFilter->sIn);` |
|   ! 0 | 1716 | `		UserBrigadeDetach(&pFilter->sOut);` |
|   ! 0 | 1717 | `		return PHL_PSFS_ERR_FATAL;` |
|     - | 1718 | `	}` |
|    41 | 1719 | `	ph7_value_resource(apArg[0],(void *)&pFilter->sIn);` |
|    41 | 1720 | `	ph7_value_resource(apArg[1],(void *)&pFilter->sOut);` |
|     - | 1721 | `	/* php's $consumed is BY REFERENCE and arrives NULL, not 0. A by-ref` |
|     - | 1722 | `	 * parameter binds to a caller SLOT, and the engine building the argument` |
|     - | 1723 | `	 * has none to offer — so one is reserved here, exactly as a variable would` |
|     - | 1724 | `	 * have, and the filter writes into it for real. */` |
|     - | 1725 | `	{` |
|    41 | 1726 | `		ph7_value *pSlot = VmReserveMemObj(pVm,&nConsumedIdx);` |
|    41 | 1727 | `		if( pSlot == 0 ){` |
|   ! 0 | 1728 | `			for( i = 0 ; i < 4 ; i++ ){` |
|   ! 0 | 1729 | `				ph7_release_value(pVm,apArg[i]);` |
|   ! 0 | 1730 | `			}` |
|   ! 0 | 1731 | `			UserBrigadeDetach(&pFilter->sIn);` |
|   ! 0 | 1732 | `			UserBrigadeDetach(&pFilter->sOut);` |
|   ! 0 | 1733 | `			return PHL_PSFS_ERR_FATAL;` |
|     - | 1734 | `		}` |
|    41 | 1735 | `		PH7_MemObjInit(pVm,pSlot);` |
|    41 | 1736 | `		pSlot->nIdx = nConsumedIdx;` |
|    41 | 1737 | `		ph7_value_null(apArg[2]);` |
|    41 | 1738 | `		apArg[2]->nIdx = nConsumedIdx;` |
|     - | 1739 | `	}` |
|    41 | 1740 | `	ph7_value_bool(apArg[3],(iFlags & PHL_PSFS_FLAG_FLUSH_CLOSE) != 0);` |
|     - | 1741 | ``	/* php sets `$this->stream` for the duration of the call and for no longer:`` |
|     - | 1742 | `	 * onCreate() sees nothing there. */` |
|    41 | 1743 | `	if( pFilter->pStreamRes ){` |
|    41 | 1744 | `		ph7_value *pSlot = PH7_NativeAttr((ph7_class_instance *)pFilter->pObj,"stream");` |
|    41 | 1745 | `		if( pSlot ){` |
|    41 | 1746 | `			PH7_MemObjStore(pFilter->pStreamRes,pSlot);` |
|    20 | 1747 | `		}` |
|    20 | 1748 | `	}` |
|    41 | 1749 | `	pSavedCall = pVm->pFilterCall;` |
|    41 | 1750 | `	pVm->pFilterCall = (void *)&pFilter->sOut;` |
|    41 | 1751 | `	PH7_MemObjInit(pVm,&sRet);` |
|    41 | 1752 | `	rc = UserFilterCall(pFilter,"filter",4,apArg,&sRet);` |
|    41 | 1753 | `	pVm->pFilterCall = pSavedCall;` |
|    41 | 1754 | `	iStatus = rc == 0 ? (int)ph7_value_to_int(&sRet) : PHL_PSFS_ERR_FATAL;` |
|    41 | 1755 | `	PH7_MemObjRelease(&sRet);` |
|   201 | 1756 | `	for( i = 0 ; i < 4 ; i++ ){` |
|   161 | 1757 | `		ph7_release_value(pVm,apArg[i]);` |
|    81 | 1758 | `	}` |
|    41 | 1759 | `	if( nConsumedIdx != SXU32_HIGH ){` |
|    41 | 1760 | `		PH7_VmReleaseUnheldSlot(pVm,nConsumedIdx);` |
|    20 | 1761 | `	}` |
|     - | 1762 | ``	/* `stream` is set for the DURATION of the call, so onClose() finds nothing`` |
|     - | 1763 | `	 * there — which is what php shows. */` |
|     - | 1764 | `	{` |
|    41 | 1765 | `		ph7_value *pSlot = PH7_NativeAttr((ph7_class_instance *)pFilter->pObj,"stream");` |
|    41 | 1766 | `		if( pSlot ){` |
|    41 | 1767 | `			PH7_MemObjRelease(pSlot);` |
|    20 | 1768 | `		}` |
|     - | 1769 | `	}` |
|    41 | 1770 | `	UserBrigadeDetach(&pFilter->sIn);` |
|    41 | 1771 | `	UserBrigadeDetach(&pFilter->sOut);` |
|    41 | 1772 | `	if( iStatus != PHL_PSFS_PASS_ON && iStatus != PHL_PSFS_FEED_ME ){` |
|     5 | 1773 | `		return PHL_PSFS_ERR_FATAL;` |
|     - | 1774 | `	}` |
|    37 | 1775 | `	return iStatus;` |
|    21 | 1776 | `}` |
|     - | 1777 | `static const phl_filter_ops sUserFilterOps = { "", 0, UserFilterRun, UserFilterClose };` |
|     - | 1778 | `/*` |
|     - | 1779 | ` * Create the instance behind one attachment. php refuses when the class is not` |
|     - | 1780 | ` * defined and when onCreate() answers FALSE, and says so twice on the second` |
|     - | 1781 | ` * one — once about the class, once about the filter.` |
|     - | 1782 | ` */` |
|    26 | 1783 | `static phl_stream_filter * UserFilterCreate(ph7_vm *pVm,phl_ufilter_reg *pReg,` |
|     - | 1784 | `	const char *zName,int nName,ph7_value *pParams,ph7_value *pStream)` |
|     1 | 1785 | `{` |
|     - | 1786 | `	phl_stream_filter *pFilter;` |
|     - | 1787 | `	ph7_class *pClass;` |
|     - | 1788 | `	ph7_class_instance *pObj;` |
|     - | 1789 | `	ph7_value sRet;` |
|    27 | 1790 | `	int nClass = (int)SyBlobLength(&pReg->sClass);` |
|    27 | 1791 | `	const char *zClass = (const char *)SyBlobData(&pReg->sClass);` |
|    27 | 1792 | `	pClass = PH7_VmExtractClass(pVm,zClass,(sxu32)nClass,FALSE,0);` |
|    27 | 1793 | `	if( pClass == 0 ){` |
|     - | 1794 | `		char zMsg[192];` |
|     4 | 1795 | `		SyBufferFormat(zMsg,sizeof(zMsg),` |
|     - | 1796 | `			"User-filter \"%.*s\" requires class \"%.*s\", but that class is not defined",` |
|     1 | 1797 | `			nName,zName,nClass,zClass);` |
|     3 | 1798 | `		PH7_VmThrowError(pVm,pVm->pCalleeName,PH7_CTX_WARNING,zMsg);` |
|     3 | 1799 | `		return 0;` |
|     - | 1800 | `	}` |
|    25 | 1801 | `	pObj = PH7_NewClassInstance(pVm,pClass);` |
|    25 | 1802 | `	if( pObj == 0 ){` |
|   ! 0 | 1803 | `		return 0;` |
|     - | 1804 | `	}` |
|    25 | 1805 | `	pFilter = FilterNew(pVm,&sUserFilterOps,zName,nName);` |
|    25 | 1806 | `	if( pFilter == 0 ){` |
|   ! 0 | 1807 | `		PH7_ClassInstanceUnref(pObj);` |
|   ! 0 | 1808 | `		return 0;` |
|     - | 1809 | `	}` |
|    25 | 1810 | `	pFilter->pObj = (void *)pObj;` |
|     - | 1811 | `	/* The name it was created UNDER, which a wildcard registration needs: a` |
|     - | 1812 | ``	 * `my.*` filter asked for as `my.thing` is told `my.thing`. */`` |
|    25 | 1813 | `	PH7_NativeSetAttrStr(pVm,pObj,"filtername",zName,nName);` |
|     - | 1814 | `	{` |
|    25 | 1815 | `		ph7_value *pSlot = PH7_NativeAttr(pObj,"params");` |
|    25 | 1816 | `		if( pSlot ){` |
|    25 | 1817 | `			if( pParams ){` |
|     3 | 1818 | `				PH7_MemObjStore(pParams,pSlot);` |
|     2 | 1819 | `			}else{` |
|    23 | 1820 | `				PH7_MemObjRelease(pSlot);` |
|     - | 1821 | `			}` |
|    12 | 1822 | `		}` |
|     - | 1823 | `	}` |
|    25 | 1824 | `	if( pStream ){` |
|    25 | 1825 | `		pFilter->pStreamRes = ph7_new_scalar(pVm);` |
|    25 | 1826 | `		if( pFilter->pStreamRes ){` |
|    25 | 1827 | `			PH7_MemObjStore(pStream,pFilter->pStreamRes);` |
|    12 | 1828 | `		}` |
|    12 | 1829 | `	}` |
|    25 | 1830 | `	PH7_MemObjInit(pVm,&sRet);` |
|     - | 1831 | `	/* A class with no onCreate() of its own simply has nothing to refuse with. */` |
|    25 | 1832 | `	if( UserFilterCall(pFilter,"onCreate",0,0,&sRet) == 0 && !ph7_value_to_bool(&sRet) ){` |
|     3 | 1833 | `		PH7_MemObjRelease(&sRet);` |
|     - | 1834 | `		/* php does not call onClose() for a filter onCreate() refused, so the` |
|     - | 1835 | `		 * instance goes back here rather than through the close path. */` |
|     3 | 1836 | `		PH7_ClassInstanceUnref(pObj);` |
|     3 | 1837 | `		pFilter->pObj = 0;` |
|     3 | 1838 | `		FilterDispose(pFilter);` |
|     3 | 1839 | `		return 0;` |
|     - | 1840 | `	}` |
|    23 | 1841 | `	PH7_MemObjRelease(&sRet);` |
|    23 | 1842 | `	return pFilter;` |
|    14 | 1843 | `}` |
|     - | 1844 | `/*` |
|     - | 1845 | ` * bool stream_filter_register(string $filter_name, string $class)` |
|     - | 1846 | ` *  php refuses an empty name or class outright, and answers FALSE for a name` |
|     - | 1847 | ` *  that is already taken rather than replacing it.` |
|     - | 1848 | ` */` |
|    30 | 1849 | `PH7_PRIVATE int PH7_builtin_stream_filter_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1850 | `{` |
|    31 | 1851 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 1852 | `	phl_ufilter_reg *pReg;` |
|     - | 1853 | `	const char *zName,*zClass;` |
|     - | 1854 | `	int nName,nClass;` |
|    15 | 1855 | `	SXUNUSED(nArg);` |
|    31 | 1856 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|    31 | 1857 | `	zClass = ph7_value_to_string(apArg[1],&nClass);` |
|    31 | 1858 | `	if( nName < 1 ){` |
|     4 | 1859 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 1860 | `			"%s(): Argument #1 ($filter_name) must be a non-empty string",` |
|     1 | 1861 | `			ph7_function_name(pCtx));` |
|     - | 1862 | `	}` |
|    29 | 1863 | `	if( nClass < 1 ){` |
|     4 | 1864 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 1865 | `			"%s(): Argument #2 ($class) must be a non-empty string",` |
|     1 | 1866 | `			ph7_function_name(pCtx));` |
|     - | 1867 | `	}` |
|    27 | 1868 | `	if( FilterFindOpsExact(zName,nName) != 0 ){` |
|     - | 1869 | `		/* A name one of the built-ins answers to is taken. */` |
|     3 | 1870 | `		ph7_result_bool(pCtx,0);` |
|     3 | 1871 | `		return PH7_OK;` |
|     - | 1872 | `	}` |
|   135 | 1873 | `	for( pReg = (phl_ufilter_reg *)pVm->pUserFilters ; pReg ; pReg = pReg->pNext ){` |
|   112 | 1874 | `		if( (int)SyBlobLength(&pReg->sName) == nName` |
|    69 | 1875 | `		 && SyMemcmp(SyBlobData(&pReg->sName),zName,(sxu32)nName) == 0 ){` |
|     3 | 1876 | `			ph7_result_bool(pCtx,0);` |
|     3 | 1877 | `			return PH7_OK;` |
|     - | 1878 | `		}` |
|    56 | 1879 | `	}` |
|    23 | 1880 | `	pReg = (phl_ufilter_reg *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_ufilter_reg));` |
|    23 | 1881 | `	if( pReg == 0 ){` |
|   ! 0 | 1882 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 1883 | `	}` |
|    23 | 1884 | `	SyZero(pReg,sizeof(*pReg));` |
|    23 | 1885 | `	SyBlobInit(&pReg->sName,&pVm->sAllocator);` |
|    23 | 1886 | `	SyBlobInit(&pReg->sClass,&pVm->sAllocator);` |
|    23 | 1887 | `	SyBlobAppend(&pReg->sName,zName,(sxu32)nName);` |
|    23 | 1888 | `	SyBlobAppend(&pReg->sClass,zClass,(sxu32)nClass);` |
|    23 | 1889 | `	pReg->pNext = (phl_ufilter_reg *)pVm->pUserFilters;` |
|    23 | 1890 | `	pVm->pUserFilters = (void *)pReg;` |
|    23 | 1891 | `	ph7_result_bool(pCtx,1);` |
|    23 | 1892 | `	return PH7_OK;` |
|    16 | 1893 | `}` |
|     - | 1894 | `/*` |
|     - | 1895 | ` * ?StreamBucket stream_bucket_make_writeable(resource $brigade)` |
|     - | 1896 | ` *  Take the next bucket off the brigade, as an object the script owns.` |
|     - | 1897 | ` */` |
|    54 | 1898 | `PH7_PRIVATE int PH7_builtin_stream_bucket_make_writeable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1899 | `{` |
|     - | 1900 | `	char zGiven[64];` |
|     - | 1901 | `	phl_brigade_res *pRes;` |
|     - | 1902 | `	phl_bucket *pBucket;` |
|     - | 1903 | `	ph7_class_instance *pObj;` |
|    27 | 1904 | `	SXUNUSED(nArg);` |
|    55 | 1905 | `	pRes = UserBrigadeFromValue(apArg[0]);` |
|    55 | 1906 | `	if( pRes == 0 ){` |
|   ! 0 | 1907 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 1908 | `			"%s(): Argument #1 ($brigade) must be of type resource, %s given",` |
|   ! 0 | 1909 | `			ph7_function_name(pCtx),VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));` |
|     - | 1910 | `	}` |
|    55 | 1911 | `	pBucket = pRes->pBrig ? PH7_FilterBucketPop(pRes->pBrig) : 0;` |
|    55 | 1912 | `	if( pBucket == 0 ){` |
|    37 | 1913 | `		ph7_result_null(pCtx);` |
|    37 | 1914 | `		return PH7_OK;` |
|     - | 1915 | `	}` |
|    28 | 1916 | `	pObj = UserBucketObject(pRes->pVm,(const char *)SyBlobData(&pBucket->sData),` |
|    18 | 1917 | `		(int)SyBlobLength(&pBucket->sData));` |
|    19 | 1918 | `	PH7_FilterBucketFree(pRes->pVm,pBucket);` |
|    19 | 1919 | `	if( pObj == 0 ){` |
|   ! 0 | 1920 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 1921 | `	}` |
|    19 | 1922 | `	PH7_NativeResultObject(pCtx,pObj);` |
|    19 | 1923 | `	return PH7_OK;` |
|    28 | 1924 | `}` |
|     - | 1925 | `/* The two that put one back, differing only in WHICH end. */` |
|    20 | 1926 | `static int UserBucketPut(ph7_context *pCtx,ph7_value **apArg,int bPrepend)` |
|     1 | 1927 | `{` |
|     - | 1928 | `	char zGiven[64];` |
|     - | 1929 | `	phl_brigade_res *pRes;` |
|     - | 1930 | `	ph7_class_instance *pObj;` |
|     - | 1931 | `	phl_bucket *pBucket;` |
|    21 | 1932 | `	const char *zData = "";` |
|    21 | 1933 | `	int nData = 0;` |
|    21 | 1934 | `	pRes = UserBrigadeFromValue(apArg[0]);` |
|    21 | 1935 | `	if( pRes == 0 ){` |
|   ! 0 | 1936 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 1937 | `			"%s(): Argument #1 ($brigade) must be of type resource, %s given",` |
|   ! 0 | 1938 | `			ph7_function_name(pCtx),VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));` |
|     - | 1939 | `	}` |
|    21 | 1940 | `	if( !ph7_value_is_object(apArg[1]) ){` |
|   ! 0 | 1941 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 1942 | `			"%s(): Argument #2 ($bucket) must be of type object, %s given",` |
|   ! 0 | 1943 | `			ph7_function_name(pCtx),VmValueGivenName(apArg[1],zGiven,sizeof(zGiven)));` |
|     - | 1944 | `	}` |
|    21 | 1945 | `	pObj = (ph7_class_instance *)apArg[1]->x.pOther;` |
|     - | 1946 | `	/* The bytes are whatever the object holds NOW: a filter that replaced` |
|     - | 1947 | ``	 * `$bucket->data` outright is the ordinary way to write one. */`` |
|    21 | 1948 | `	PH7_NativeAttrStr(pObj,"data",&zData,&nData);` |
|    21 | 1949 | `	if( pRes->pBrig == 0 ){` |
|     - | 1950 | `		/* A handle kept past the call it belonged to: there is nothing to put` |
|     - | 1951 | `		 * it back into. */` |
|   ! 0 | 1952 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1953 | `		return PH7_OK;` |
|     - | 1954 | `	}` |
|    21 | 1955 | `	pBucket = PH7_FilterBucketNew(pRes->pVm,zData,(sxu32)nData);` |
|    21 | 1956 | `	if( pBucket == 0 ){` |
|   ! 0 | 1957 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 1958 | `	}` |
|    21 | 1959 | `	if( bPrepend ){` |
|     3 | 1960 | `		pBucket->pNext = pRes->pBrig->pHead;` |
|     3 | 1961 | `		pRes->pBrig->pHead = pBucket;` |
|     3 | 1962 | `		if( pRes->pBrig->pTail == 0 ){` |
|   ! 0 | 1963 | `			pRes->pBrig->pTail = pBucket;` |
|   ! 0 | 1964 | `		}` |
|     2 | 1965 | `	}else{` |
|    19 | 1966 | `		PH7_FilterBucketAppend(pRes->pBrig,pBucket);` |
|     - | 1967 | `	}` |
|    21 | 1968 | `	ph7_result_null(pCtx);` |
|    21 | 1969 | `	return PH7_OK;` |
|    11 | 1970 | `}` |
|    18 | 1971 | `PH7_PRIVATE int PH7_builtin_stream_bucket_append(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1972 | `{` |
|     9 | 1973 | `	SXUNUSED(nArg);` |
|    19 | 1974 | `	return UserBucketPut(pCtx,apArg,0);` |
|     1 | 1975 | `}` |
|     2 | 1976 | `PH7_PRIVATE int PH7_builtin_stream_bucket_prepend(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1977 | `{` |
|     1 | 1978 | `	SXUNUSED(nArg);` |
|     3 | 1979 | `	return UserBucketPut(pCtx,apArg,1);` |
|     1 | 1980 | `}` |
|     - | 1981 | `/*` |
|     - | 1982 | ` * StreamBucket stream_bucket_new(resource $stream, string $buffer)` |
|     - | 1983 | ` *  A bucket of the filter's own making — the only way to emit a TAIL, since the` |
|     - | 1984 | ` *  closing call arrives with an empty brigade.` |
|     - | 1985 | ` */` |
|     6 | 1986 | `PH7_PRIVATE int PH7_builtin_stream_bucket_new(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1987 | `{` |
|     - | 1988 | `	ph7_class_instance *pObj;` |
|     - | 1989 | `	const char *zData;` |
|     - | 1990 | `	int nData;` |
|     3 | 1991 | `	SXUNUSED(nArg);` |
|     7 | 1992 | `	zData = ph7_value_to_string(apArg[1],&nData);` |
|     7 | 1993 | `	pObj = UserBucketObject(pCtx->pVm,zData,nData);` |
|     7 | 1994 | `	if( pObj == 0 ){` |
|   ! 0 | 1995 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 1996 | `	}` |
|     7 | 1997 | `	PH7_NativeResultObject(pCtx,pObj);` |
|     7 | 1998 | `	return PH7_OK;` |
|     4 | 1999 | `}` |
|     - | 2000 | `/*` |
|     - | 2001 | ` * php_user_filter and StreamBucket. The three methods are the ones a filter` |
|     - | 2002 | ` * OVERRIDES; their bodies here are php's own do-nothing defaults, and a class` |
|     - | 2003 | ` * that overrides none of them is a filter that refuses every read — which is` |
|     - | 2004 | ` * what php answers too.` |
|     - | 2005 | ` */` |
|     2 | 2006 | `static int vm_builtin_user_filter_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2007 | `{` |
|     1 | 2008 | `	SXUNUSED(nArg);` |
|     1 | 2009 | `	SXUNUSED(apArg);` |
|     3 | 2010 | `	ph7_result_int(pCtx,PHL_PSFS_ERR_FATAL);` |
|     3 | 2011 | `	return PH7_OK;` |
|     1 | 2012 | `}` |
|    18 | 2013 | `static int vm_builtin_user_filter_onCreate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2014 | `{` |
|     9 | 2015 | `	SXUNUSED(nArg);` |
|     9 | 2016 | `	SXUNUSED(apArg);` |
|    19 | 2017 | `	ph7_result_bool(pCtx,1);` |
|    19 | 2018 | `	return PH7_OK;` |
|     1 | 2019 | `}` |
|    18 | 2020 | `static int vm_builtin_user_filter_onClose(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2021 | `{` |
|     9 | 2022 | `	SXUNUSED(nArg);` |
|     9 | 2023 | `	SXUNUSED(apArg);` |
|    19 | 2024 | `	ph7_result_null(pCtx);` |
|    19 | 2025 | `	return PH7_OK;` |
|     1 | 2026 | `}` |
|  6721 | 2027 | `PH7_PRIVATE sxi32 PH7_VmInstallStreamFilter(ph7_vm *pVm)` |
|     5 | 2028 | `{` |
|     - | 2029 | `	static const PH7_NativeMethodDef aFilterMethod[] = {` |
|     - | 2030 | `		{ "filter", PH7_MOD_PUBLIC, "$in, $out, &$consumed, bool $closing", "@int",` |
|     - | 2031 | `		  vm_builtin_user_filter_filter },` |
|     - | 2032 | `		{ "onCreate", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_user_filter_onCreate },` |
|     - | 2033 | `		{ "onClose", PH7_MOD_PUBLIC, "", "@void", vm_builtin_user_filter_onClose },` |
|     - | 2034 | `	};` |
|     - | 2035 | `	static const PH7_NativePropDef aFilterProp[] = {` |
|     - | 2036 | `		{ "filtername", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, "string" },` |
|     - | 2037 | `		{ "params", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },` |
|     - | 2038 | `		{ "stream", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - | 2039 | `	};` |
|     - | 2040 | `	static const PH7_NativePropDef aBucketProp[] = {` |
|     - | 2041 | `		{ "bucket", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - | 2042 | `		/* php declares the three TYPED and without a default, so a bucket the` |
|     - | 2043 | `		 * stream layer has not filled yet carries them UNINITIALIZED: absent from` |
|     - | 2044 | `		 * the (array) cast, get_object_vars() and json_encode(), printed as` |
|     - | 2045 | ``		 * `uninitialized(string)` by var_dump and uncounted in its header, and a`` |
|     - | 2046 | `		 * read before the first write is php's "must not be accessed before` |
|     - | 2047 | `		 * initialization" Error rather than an empty string. */` |
|     - | 2048 | `		{ "data", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|     - | 2049 | `		{ "datalen", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "int" },` |
|     - | 2050 | `		{ "dataLength", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "int" },` |
|     - | 2051 | `	};` |
|     - | 2052 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - | 2053 | `		{ "php_user_filter", 0, 0, 0,` |
|     - | 2054 | `		  aFilterMethod, SX_ARRAYSIZE(aFilterMethod), 0, 0,` |
|     - | 2055 | `		  aFilterProp, SX_ARRAYSIZE(aFilterProp), 0, 0, 0 },` |
|     - | 2056 | `		{ "StreamBucket", 0, 0, PH7_CLASS_FINAL,` |
|     - | 2057 | `		  0, 0, 0, 0,` |
|     - | 2058 | `		  aBucketProp, SX_ARRAYSIZE(aBucketProp), UserBucketRelease, 0, 0 },` |
|     - | 2059 | `	};` |
|  6726 | 2060 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|     5 | 2061 | `}` |
|     - | 2062 | `#endif /* PH7_DISABLE_DISK_IO */` |
|     - | 2063 |  |
