# src/sx/sxlib.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 216/230 lines (93.91%)

[Root index](../../index.md) | [Directory index](index.md)

|      Hits | Line | Source |
| --------: | ---: | :--- |
|         - |    1 | `/**` |
|         - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|         - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|         - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|         - |    5 | ` */` |
|         - |    6 | `#include "sxtypes.h"` |
|         - |    7 | `#include "sxmacros.h"` |
|         - |    8 | `#include "sxset.h"` |
|         - |    9 | `#include "sxmem.h"` |
|         - |   10 | `#include "sxhash.h"` |
|         - |   11 | `#include "sxlex.h"` |
|         - |   12 | `#include "sxbase64.h"` |
|         - |   13 | `#include "sxuri.h"` |
|         - |   14 | `#include "sxtime.h"` |
|         - |   15 | `#include "sxstr.h"` |
|         - |   16 | `#include "sxutils.h" /* SyHexToint(), used by SyUriDecode() */` |
|         - |   17 |  |
|  93973097 |   18 | `PH7_PRIVATE sxu32 SyBinHash(const void *pSrc,sxu32 nLen)` |
|         5 |   19 | `{` |
|  93973102 |   20 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|         - |   21 | `	unsigned char *zEnd;` |
|  93973102 |   22 | `	sxu32 nH = 5381;` |
|  93973102 |   23 | `	zEnd = &zIn[nLen];` |
| 146131686 |   24 | `	for(;;){` |
| 295511560 |   25 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
| 256384736 |   26 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
| 236265381 |   27 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
| 220483558 |   28 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|         5 |   29 | `	}` |
|  93973102 |   30 | `	return nH;` |
|         5 |   31 | `}` |
| 160348100 |   32 | `PH7_PRIVATE sxu32 SyStrHash(const void *pSrc,sxu32 nLen)` |
|         5 |   33 | `{` |
| 160348105 |   34 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|         - |   35 | `	unsigned char *zEnd;` |
| 160348105 |   36 | `	sxu32 nH = 5381;` |
| 160348105 |   37 | `	zEnd = &zIn[nLen];` |
| 455653601 |   38 | `	for(;;){` |
| 912687759 |   39 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + SyToLower(zIn[0]); zIn++;` |
| 874781759 |   40 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + SyToLower(zIn[0]); zIn++;` |
| 837034910 |   41 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + SyToLower(zIn[0]); zIn++;` |
| 799218840 |   42 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + SyToLower(zIn[0]); zIn++;` |
|         5 |   43 | `	}` |
| 160348105 |   44 | `	return nH;` |
|         5 |   45 | `}` |
|         - |   46 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        24 |   47 | `PH7_PRIVATE sxi32 SyBase64Encode(const char *zSrc,sxu32 nLen,ProcConsumer xConsumer,void *pUserData)` |
|         3 |   48 | `{` |
|         - |   49 | `	static const unsigned char zBase64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";` |
|        27 |   50 | `	unsigned char *zIn = (unsigned char *)zSrc;` |
|         - |   51 | `	unsigned char z64[4];` |
|         - |   52 | `	sxu32 i;` |
|         - |   53 | `	sxi32 rc;` |
|         - |   54 | `#if defined(UNTRUST)` |
|         - |   55 | `	if( SX_EMPTY_STR(zSrc) \|\| xConsumer == 0){` |
|         - |   56 | `		return SXERR_EMPTY;` |
|         - |   57 | `	}` |
|         - |   58 | `#endif` |
|        97 |   59 | `	for(i = 0; i + 2 < nLen; i += 3){` |
|        73 |   60 | `		z64[0] = zBase64[(zIn[i] >> 2) & 0x3F];` |
|        73 |   61 | `		z64[1] = zBase64[( ((zIn[i] & 0x03) << 4)   \| (zIn[i+1] >> 4)) & 0x3F];` |
|        73 |   62 | `		z64[2] = zBase64[( ((zIn[i+1] & 0x0F) << 2) \| (zIn[i + 2] >> 6) ) & 0x3F];` |
|        73 |   63 | `		z64[3] = zBase64[ zIn[i + 2] & 0x3F];` |
|         - |   64 |  |
|        73 |   65 | `		rc = xConsumer((const void *)z64,sizeof(z64),pUserData);` |
|        73 |   66 | `		if( rc != SXRET_OK ){return SXERR_ABORT;}` |
|         - |   67 |  |
|        38 |   68 | `	}` |
|        27 |   69 | `	if ( i+1 < nLen ){` |
|         6 |   70 | `		z64[0] = zBase64[(zIn[i] >> 2) & 0x3F];` |
|         6 |   71 | `		z64[1] = zBase64[( ((zIn[i] & 0x03) << 4)   \| (zIn[i+1] >> 4)) & 0x3F];` |
|         6 |   72 | `		z64[2] = zBase64[(zIn[i+1] & 0x0F) << 2 ];` |
|         6 |   73 | `		z64[3] = '=';` |
|         - |   74 |  |
|         6 |   75 | `		rc = xConsumer((const void *)z64,sizeof(z64),pUserData);` |
|         6 |   76 | `		if( rc != SXRET_OK ){return SXERR_ABORT;}` |
|         - |   77 |  |
|        25 |   78 | `	}else if( i < nLen ){` |
|        16 |   79 | `		z64[0] = zBase64[(zIn[i] >> 2) & 0x3F];` |
|        16 |   80 | `		z64[1]   = zBase64[(zIn[i] & 0x03) << 4];` |
|        16 |   81 | `		z64[2] = '=';` |
|        16 |   82 | `		z64[3] = '=';` |
|         - |   83 |  |
|        16 |   84 | `		rc = xConsumer((const void *)z64,sizeof(z64),pUserData);` |
|        16 |   85 | `		if( rc != SXRET_OK ){return SXERR_ABORT;}` |
|         7 |   86 | `	}` |
|         - |   87 |  |
|        27 |   88 | `	return SXRET_OK;` |
|        15 |   89 | `}` |
|        12 |   90 | `PH7_PRIVATE sxi32 SyBase64Decode(const char *zB64,sxu32 nLen,ProcConsumer xConsumer,void *pUserData)` |
|         3 |   91 | `{` |
|         - |   92 | `	static const sxu32 aBase64Trans[] = {` |
|         - |   93 | `	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,` |
|         - |   94 | `	0,0,0,0,0,62,0,0,0,63,52,53,54,55,56,57,58,59,60,61,0,0,0,0,0,0,0,0,1,2,3,4,` |
|         - |   95 | `	5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,0,0,0,0,0,0,26,27,` |
|         - |   96 | `	28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,0,0,` |
|         - |   97 | `	0,0,0` |
|         - |   98 | `	};` |
|         - |   99 | `	sxu32 n,w,x,y,z;` |
|         - |  100 | `	sxi32 rc;` |
|         - |  101 | `	unsigned char zOut[10];` |
|         - |  102 | `#if defined(UNTRUST)` |
|         - |  103 | `	if( SX_EMPTY_STR(zB64) \|\| xConsumer == 0 ){` |
|         - |  104 | `		return SXERR_EMPTY;` |
|         - |  105 | `	}` |
|         - |  106 | `#endif` |
|        25 |  107 | `	while(nLen > 0 && zB64[nLen - 1] == '=' ){` |
|        13 |  108 | `		nLen--;` |
|         3 |  109 | `	}` |
|        39 |  110 | `	for( n = 0 ; n+3<nLen ; n += 4){` |
|        27 |  111 | `		w = aBase64Trans[zB64[n] & 0x7F];` |
|        27 |  112 | `		x = aBase64Trans[zB64[n+1] & 0x7F];` |
|        27 |  113 | `		y = aBase64Trans[zB64[n+2] & 0x7F];` |
|        27 |  114 | `		z = aBase64Trans[zB64[n+3] & 0x7F];` |
|        27 |  115 | `		zOut[0] = ((w<<2) & 0xFC) \| ((x>>4) & 0x03);` |
|        27 |  116 | `		zOut[1] = ((x<<4) & 0xF0) \| ((y>>2) & 0x0F);` |
|        27 |  117 | `		zOut[2] = ((y<<6) & 0xC0) \| (z & 0x3F);` |
|         - |  118 |  |
|        27 |  119 | `		rc = xConsumer((const void *)zOut,sizeof(unsigned char)*3,pUserData);` |
|        27 |  120 | `		if( rc != SXRET_OK ){ return SXERR_ABORT;}` |
|        15 |  121 | `	}` |
|        15 |  122 | `	if( n+2 < nLen ){` |
|        11 |  123 | `		w = aBase64Trans[zB64[n] & 0x7F];` |
|        11 |  124 | `		x = aBase64Trans[zB64[n+1] & 0x7F];` |
|        11 |  125 | `		y = aBase64Trans[zB64[n+2] & 0x7F];` |
|         - |  126 |  |
|        11 |  127 | `		zOut[0] = ((w<<2) & 0xFC) \| ((x>>4) & 0x03);` |
|        11 |  128 | `		zOut[1] = ((x<<4) & 0xF0) \| ((y>>2) & 0x0F);` |
|         - |  129 |  |
|        11 |  130 | `		rc = xConsumer((const void *)zOut,sizeof(unsigned char)*2,pUserData);` |
|        11 |  131 | `		if( rc != SXRET_OK ){ return SXERR_ABORT;}` |
|        10 |  132 | `	}else if( n+1 < nLen ){` |
|         3 |  133 | `		w = aBase64Trans[zB64[n] & 0x7F];` |
|         3 |  134 | `		x = aBase64Trans[zB64[n+1] & 0x7F];` |
|         - |  135 |  |
|         3 |  136 | `		zOut[0] = ((w<<2) & 0xFC) \| ((x>>4) & 0x03);` |
|         - |  137 |  |
|         3 |  138 | `		rc = xConsumer((const void *)zOut,sizeof(unsigned char)*1,pUserData);` |
|         3 |  139 | `		if( rc != SXRET_OK ){ return SXERR_ABORT;}` |
|         1 |  140 | `	}` |
|        15 |  141 | `	return SXRET_OK;` |
|         9 |  142 | `}` |
|         - |  143 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - |  144 | `#define INVALID_LEXER(LEX)	(  LEX == 0  \|\| LEX->xTokenizer == 0 )` |
|     40911 |  145 | `PH7_PRIVATE sxi32 SyLexInit(SyLex *pLex,SySet *pSet,ProcTokenizer xTokenizer,void *pUserData)` |
|         5 |  146 | `{` |
|         - |  147 | `	SyStream *pStream;` |
|         - |  148 | `#if defined (UNTRUST)` |
|         - |  149 | `	if ( pLex == 0 \|\| xTokenizer == 0 ){` |
|         - |  150 | `		return SXERR_CORRUPT;` |
|         - |  151 | `	}` |
|         - |  152 | `#endif` |
|     40916 |  153 | `	pLex->pTokenSet = 0;` |
|         - |  154 | `	/* Initialize lexer fields */` |
|     40916 |  155 | `	if( pSet ){` |
|     40916 |  156 | `		if ( SySetElemSize(pSet) != sizeof(SyToken) ){` |
|       ! 0 |  157 | `			return SXERR_INVALID;` |
|         - |  158 | `		}` |
|     40916 |  159 | `		pLex->pTokenSet = pSet;` |
|     20430 |  160 | `	}` |
|     40916 |  161 | `	pStream = &pLex->sStream;` |
|     40916 |  162 | `	pLex->xTokenizer = xTokenizer;` |
|     40916 |  163 | `	pLex->pUserData = pUserData;` |
|         - |  164 |  |
|     40916 |  165 | `	pStream->nLine = 1;` |
|     40916 |  166 | `	pStream->nIgn  = 0;` |
|     40916 |  167 | `	pStream->zText = pStream->zEnd = 0;` |
|     40916 |  168 | `	pStream->pSet  = pSet;` |
|     40916 |  169 | `	return SXRET_OK;` |
|     20435 |  170 | `}` |
|     40911 |  171 | `PH7_PRIVATE sxi32 SyLexTokenizeInput(SyLex *pLex,const char *zInput,sxu32 nLen,void *pCtxData,ProcSort xSort,ProcCmp xCmp)` |
|         5 |  172 | `{` |
|         - |  173 | `	const unsigned char *zCur;` |
|         - |  174 | `	SyStream *pStream;` |
|         - |  175 | `	SyToken sToken;` |
|         - |  176 | `	sxi32 rc;` |
|         - |  177 | `#if defined (UNTRUST)` |
|         - |  178 | `	if ( INVALID_LEXER(pLex) \|\| zInput == 0 ){` |
|         - |  179 | `		return SXERR_CORRUPT;` |
|         - |  180 | `	}` |
|         - |  181 | `#endif` |
|     40916 |  182 | `	pStream = &pLex->sStream;` |
|         - |  183 | `	/* Point to the head of the input */` |
|     40916 |  184 | `	pStream->zText = pStream->zInput = (const unsigned char *)zInput;` |
|         - |  185 | `	/* Point to the end of the input */` |
|     40916 |  186 | `	pStream->zEnd = &pStream->zInput[nLen];` |
|  13643109 |  187 | `	for(;;){` |
|  27325585 |  188 | `		if( pStream->zText >= pStream->zEnd ){` |
|         - |  189 | `			/* End of the input reached */` |
|     40842 |  190 | `			break;` |
|         - |  191 | `		}` |
|  27284748 |  192 | `		zCur = pStream->zText;` |
|         - |  193 | `		/* Call the tokenizer callback */` |
|  27284748 |  194 | `		rc = pLex->xTokenizer(pStream,&sToken,pLex->pUserData,pCtxData);` |
|  27284748 |  195 | `		if( rc != SXRET_OK && rc != SXERR_CONTINUE ){` |
|         - |  196 | `			/* Tokenizer callback request an operation abort */` |
|        77 |  197 | `			if( rc == SXERR_ABORT ){` |
|        52 |  198 | `				return SXERR_ABORT;` |
|         - |  199 | `			}` |
|        26 |  200 | `			break;` |
|         - |  201 | `		}` |
|  27284674 |  202 | `		if( rc == SXERR_CONTINUE ){` |
|         - |  203 | `			/* Request to ignore this token */` |
|    339510 |  204 | `			pStream->nIgn++;` |
|  27114711 |  205 | `		}else if( pLex->pTokenSet  ){` |
|         - |  206 | `			/* Put the token in the set */` |
|  26945169 |  207 | `			rc = SySetPut(pLex->pTokenSet,(const void *)&sToken);` |
|  26945169 |  208 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  209 | `				break;` |
|         - |  210 | `			}` |
|  13453137 |  211 | `		}` |
|  27284674 |  212 | `		if( zCur >= pStream->zText ){` |
|         - |  213 | `			/* Automatic advance of the stream cursor */` |
|       ! 0 |  214 | `			pStream->zText = &zCur[1];` |
|       ! 0 |  215 | `		}` |
|         5 |  216 | `	}` |
|     40866 |  217 | `	if( xSort &&  pLex->pTokenSet ){` |
|       ! 0 |  218 | `		SyToken *aToken = (SyToken *)SySetBasePtr(pLex->pTokenSet);` |
|         - |  219 | `		/* Sort the extracted tokens */` |
|       ! 0 |  220 | `		if( xCmp == 0 ){` |
|         - |  221 | `			/* Use a default comparison function */` |
|       ! 0 |  222 | `			xCmp = SyMemcmp;` |
|       ! 0 |  223 | `		}` |
|       ! 0 |  224 | `		xSort(aToken,SySetUsed(pLex->pTokenSet),sizeof(SyToken),xCmp);` |
|       ! 0 |  225 | `	}` |
|     40866 |  226 | `	return SXRET_OK;` |
|     20435 |  227 | `}` |
|     40911 |  228 | `PH7_PRIVATE sxi32 SyLexRelease(SyLex *pLex)` |
|         5 |  229 | `{` |
|     40916 |  230 | `	sxi32 rc = SXRET_OK;` |
|         - |  231 | `#if defined (UNTRUST)` |
|         - |  232 | `	if ( INVALID_LEXER(pLex) ){` |
|         - |  233 | `		return SXERR_CORRUPT;` |
|         - |  234 | `	}` |
|         - |  235 | `#else` |
|     20430 |  236 | `	SXUNUSED(pLex); /* Prevent compiler warning */` |
|         - |  237 | `#endif` |
|     40916 |  238 | `	return rc;` |
|         5 |  239 | `}` |
|         - |  240 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - |  241 | `/* php's urlencode() keeps only alnum and -_. safe (space becomes '+'); rawurlencode()` |
|         - |  242 | ` * additionally keeps '~' and encodes space as %20 (RFC 3986). '$' is NOT safe in` |
|         - |  243 | ` * either -- php encodes it %24. */` |
|         - |  244 | `#define SAFE_URL(C)	(SyisAlphaNum(c) \|\| c == '_' \|\| c == '-' \|\| c == '.' )` |
|         - |  245 | `#define SAFE_RAW(C)	(SyisAlphaNum(c) \|\| c == '_' \|\| c == '-' \|\| c == '.' \|\| c == '~' )` |
|      1036 |  246 | `static sxi32 SyUriEncodeInternal(const char *zSrc,sxu32 nLen,ProcConsumer xConsumer,void *pUserData,int bRaw)` |
|         5 |  247 | `{` |
|      1041 |  248 | `	unsigned char *zIn = (unsigned char *)zSrc;` |
|      1041 |  249 | `	unsigned char zHex[3] = { '%',0,0 };` |
|         - |  250 | `	unsigned char zOut[2];` |
|         - |  251 | `	unsigned char *zCur,*zEnd;` |
|         - |  252 | `	sxi32 c;` |
|         - |  253 | `	sxi32 rc;` |
|         - |  254 | `#ifdef UNTRUST` |
|         - |  255 | `	if( SX_EMPTY_STR(zSrc) \|\| xConsumer == 0 ){` |
|         - |  256 | `		return SXERR_EMPTY;` |
|         - |  257 | `	}` |
|         - |  258 | `#endif` |
|      1041 |  259 | `	rc = SXRET_OK;` |
|      1041 |  260 | `	zEnd = &zIn[nLen]; zCur = zIn;` |
|       783 |  261 | `	for(;;){` |
|      6007 |  262 | `		if( zCur >= zEnd ){` |
|      1301 |  263 | `			if( zCur != zIn ){` |
|      1035 |  264 | `				rc = xConsumer(zIn,(sxu32)(zCur-zIn),pUserData);` |
|       516 |  265 | `			}` |
|      1301 |  266 | `			break;` |
|         - |  267 | `		}` |
|      4711 |  268 | `		c = zCur[0];` |
|      4711 |  269 | `		if( bRaw ? SAFE_RAW(c) : SAFE_URL(c) ){` |
|      4311 |  270 | `			zCur++; continue;` |
|         - |  271 | `		}` |
|       663 |  272 | `		if( zCur != zIn && SXRET_OK != (rc = xConsumer(zIn,(sxu32)(zCur-zIn),pUserData))){` |
|       ! 0 |  273 | `			break;` |
|         - |  274 | `		}` |
|       663 |  275 | `		if( c == ' ' && !bRaw ){` |
|        10 |  276 | `			zOut[0] = '+';` |
|        10 |  277 | `			rc = xConsumer((const void *)zOut,sizeof(unsigned char),pUserData);` |
|         6 |  278 | `		}else{` |
|       655 |  279 | `			zHex[1]	= "0123456789ABCDEF"[(c >> 4) & 0x0F];` |
|       655 |  280 | `			zHex[2] = "0123456789ABCDEF"[c & 0x0F];` |
|       655 |  281 | `			rc = xConsumer(zHex,sizeof(zHex),pUserData);` |
|         - |  282 | `		}` |
|       663 |  283 | `		if( SXRET_OK != rc ){` |
|       ! 0 |  284 | `			break;` |
|         - |  285 | `		}` |
|       663 |  286 | `		zIn = &zCur[1]; zCur = zIn ;` |
|         3 |  287 | `	}` |
|      1301 |  288 | `	return rc == SXRET_OK ? SXRET_OK : SXERR_ABORT;` |
|         5 |  289 | `}` |
|       788 |  290 | `PH7_PRIVATE sxi32 SyUriEncode(const char *zSrc,sxu32 nLen,ProcConsumer xConsumer,void *pUserData)` |
|         2 |  291 | `{` |
|       790 |  292 | `	return SyUriEncodeInternal(zSrc,nLen,xConsumer,pUserData,0);` |
|         2 |  293 | `}` |
|       248 |  294 | `PH7_PRIVATE sxi32 SyUriEncodeRaw(const char *zSrc,sxu32 nLen,ProcConsumer xConsumer,void *pUserData)` |
|         5 |  295 | `{` |
|       253 |  296 | `	return SyUriEncodeInternal(zSrc,nLen,xConsumer,pUserData,1);` |
|         5 |  297 | `}` |
|         - |  298 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - |  299 | `/*` |
|         - |  300 | ` * php's php_url_decode()/php_raw_url_decode(): a BYTE-exact walk -- "%" followed by` |
|         - |  301 | ` * two hex digits becomes that byte, "+" becomes a space when bPlus is set, and` |
|         - |  302 | ` * anything else (a truncated "%4"/"%", a "%zz" that is not hex, a raw high byte)` |
|         - |  303 | ` * is copied through untouched.` |
|         - |  304 | ` *` |
|         - |  305 | ` * The routine this replaced tried to be clever about UTF-8: it folded a %XX byte` |
|         - |  306 | ` * >= 0xC0 and its continuation bytes into a codepoint and re-encoded it, dropped` |
|         - |  307 | ` * a truncated escape entirely, and read a non-hex digit as 0. That round-tripped` |
|         - |  308 | ` * VALID UTF-8 and silently corrupted everything else -- "%FF" decoded to NUL,` |
|         - |  309 | ` * "%C3" to \x03, "abc%" to "abc", "a%zzb" to "a\0b" -- and it reached $_GET,` |
|         - |  310 | ` * $_POST, cookies and parse_str() as well as urldecode() itself.` |
|         - |  311 | ` */` |
|      4466 |  312 | `PH7_PRIVATE sxi32 SyUriDecode(const char *zSrc,sxu32 nLen,ProcConsumer xConsumer,void *pUserData,int bPlus)` |
|         2 |  313 | `{` |
|         - |  314 | `	const char *zIn,*zCur,*zEnd;` |
|      4468 |  315 | `	sxi32 rc = SXRET_OK;` |
|         - |  316 | `#if defined(UNTRUST)` |
|         - |  317 | `	if( SX_EMPTY_STR(zSrc) \|\| xConsumer == 0 ){` |
|         - |  318 | `		return SXERR_EMPTY;` |
|         - |  319 | `	}` |
|         - |  320 | `#endif` |
|      4468 |  321 | `	zIn = zCur = zSrc;` |
|      4468 |  322 | `	zEnd = &zSrc[nLen];` |
|     21110 |  323 | `	while( zCur < zEnd ){` |
|         - |  324 | `		unsigned char zByte[1];` |
|         - |  325 | `		int nSkip;` |
|     16642 |  326 | `		if( zCur[0] == '%' && &zCur[2] < zEnd` |
|       793 |  327 | `			&& SyHexToint(zCur[1]) >= 0 && SyHexToint(zCur[2]) >= 0 ){` |
|       766 |  328 | `			zByte[0] = (unsigned char)((SyHexToint(zCur[1]) << 4) \| SyHexToint(zCur[2]));` |
|       766 |  329 | `			nSkip = 3;` |
|     16197 |  330 | `		}else if( bPlus && zCur[0] == '+' ){` |
|        20 |  331 | `			zByte[0] = ' ';` |
|        20 |  332 | `			nSkip = 1;` |
|        11 |  333 | `		}else{` |
|         - |  334 | `			/* Verbatim: batched with its neighbours and flushed below. */` |
|     15862 |  335 | `			zCur++;` |
|     15862 |  336 | `			continue;` |
|         - |  337 | `		}` |
|       784 |  338 | `		if( zCur > zIn ){` |
|        82 |  339 | `			rc = xConsumer(zIn,(unsigned int)(zCur-zIn),pUserData);` |
|        82 |  340 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `				return rc;` |
|         - |  342 | `			}` |
|        45 |  343 | `		}` |
|       784 |  344 | `		rc = xConsumer((const void *)zByte,sizeof(zByte),pUserData);` |
|       784 |  345 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  346 | `			return rc;` |
|         - |  347 | `		}` |
|       784 |  348 | `		zCur += nSkip;` |
|       784 |  349 | `		zIn = zCur;` |
|         2 |  350 | `	}` |
|      4468 |  351 | `	if( zCur > zIn ){` |
|      4406 |  352 | `		rc = xConsumer(zIn,(unsigned int)(zCur-zIn),pUserData);` |
|      2203 |  353 | `	}` |
|      4468 |  354 | `	return rc;` |
|      2235 |  355 | `}` |
|         - |  356 |  |
|         - |  357 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - |  358 | `static const char *zEngDay[] = {` |
|         - |  359 | `	"Sunday","Monday","Tuesday","Wednesday",` |
|         - |  360 | `	"Thursday","Friday","Saturday"` |
|         - |  361 | `};` |
|         - |  362 | `static const char *zEngMonth[] = {` |
|         - |  363 | `	"January","February","March","April",` |
|         - |  364 | `	"May","June","July","August",` |
|         - |  365 | `	"September","October","November","December"` |
|         - |  366 | `};` |
|       292 |  367 | `static const char * GetDay(sxi32 i)` |
|         3 |  368 | `{` |
|       295 |  369 | `	return zEngDay[ i % 7 ];` |
|         3 |  370 | `}` |
|       132 |  371 | `static const char * GetMonth(sxi32 i)` |
|         3 |  372 | `{` |
|       135 |  373 | `	return zEngMonth[ i % 12 ];` |
|         3 |  374 | `}` |
|       292 |  375 | `PH7_PRIVATE const char * SyTimeGetDay(sxi32 iDay)` |
|         3 |  376 | `{` |
|       295 |  377 | `	return GetDay(iDay);` |
|         3 |  378 | `}` |
|       132 |  379 | `PH7_PRIVATE const char * SyTimeGetMonth(sxi32 iMonth)` |
|         3 |  380 | `{` |
|       135 |  381 | `	return GetMonth(iMonth);` |
|         3 |  382 | `}` |
|         - |  383 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - |  384 |  |
