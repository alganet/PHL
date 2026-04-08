# src/sx/sxlib.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 228/253 lines (90.12%)

[Root index](../../index.md) | [Directory index](index.md)

|     Hits | Line | Source |
| -------: | ---: | :--- |
|        - |    1 | `/**` |
|        - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|        - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|        - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|        - |    5 | ` */` |
|        - |    6 | `#include "sxtypes.h"` |
|        - |    7 | `#include "sxmacros.h"` |
|        - |    8 | `#include "sxset.h"` |
|        - |    9 | `#include "sxmem.h"` |
|        - |   10 | `#include "sxhash.h"` |
|        - |   11 | `#include "sxlex.h"` |
|        - |   12 | `#include "sxbase64.h"` |
|        - |   13 | `#include "sxuri.h"` |
|        - |   14 | `#include "sxtime.h"` |
|        - |   15 | `#include "sxstr.h"` |
|        - |   16 |  |
| 13891662 |   17 | `PH7_PRIVATE sxu32 SyBinHash(const void *pSrc,sxu32 nLen)` |
|        2 |   18 |  |
| 13891664 |   19 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|        - |   20 | `	unsigned char *zEnd;` |
| 13891664 |   21 | `	sxu32 nH = 5381;` |
| 13891664 |   22 | `	zEnd = &zIn[nLen];` |
| 23165501 |   23 | `	for(;;){` |
| 46329596 |   24 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
| 42982394 |   25 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
| 39445536 |   26 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
| 35439864 |   27 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|        2 |   28 | `	}` |
| 13891664 |   29 | `	return nH;` |
|        2 |   30 |  |
|   103116 |   31 | `PH7_PRIVATE sxu32 SyStrHash(const void *pSrc,sxu32 nLen)` |
|        2 |   32 |  |
|   103118 |   33 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|        - |   34 | `	unsigned char *zEnd;` |
|   103118 |   35 | `	sxu32 nH = 5381;` |
|   103118 |   36 | `	zEnd = &zIn[nLen];` |
|   161151 |   37 | `	for(;;){` |
|   322304 |   38 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + SyToLower(zIn[0]); zIn++;` |
|   303648 |   39 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + SyToLower(zIn[0]); zIn++;` |
|   245810 |   40 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + SyToLower(zIn[0]); zIn++;` |
|   219598 |   41 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + SyToLower(zIn[0]); zIn++;` |
|        2 |   42 | `	}` |
|   103118 |   43 | `	return nH;` |
|        2 |   44 |  |
|        - |   45 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        6 |   46 | `PH7_PRIVATE sxi32 SyBase64Encode(const char *zSrc,sxu32 nLen,ProcConsumer xConsumer,void *pUserData)` |
|        1 |   47 |  |
|        - |   48 | `	static const unsigned char zBase64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";` |
|        7 |   49 | `	unsigned char *zIn = (unsigned char *)zSrc;` |
|        - |   50 | `	unsigned char z64[4];` |
|        - |   51 | `	sxu32 i;` |
|        - |   52 | `	sxi32 rc;` |
|        - |   53 | `#if defined(UNTRUST)` |
|        - |   54 | `	if( SX_EMPTY_STR(zSrc) \|\| xConsumer == 0){` |
|        - |   55 | `		return SXERR_EMPTY;` |
|        - |   56 | `	}` |
|        - |   57 | `#endif` |
|        9 |   58 | `	for(i = 0; i + 2 < nLen; i += 3){` |
|        3 |   59 | `		z64[0] = zBase64[(zIn[i] >> 2) & 0x3F];` |
|        3 |   60 | `		z64[1] = zBase64[( ((zIn[i] & 0x03) << 4)   \| (zIn[i+1] >> 4)) & 0x3F];` |
|        3 |   61 | `		z64[2] = zBase64[( ((zIn[i+1] & 0x0F) << 2) \| (zIn[i + 2] >> 6) ) & 0x3F];` |
|        3 |   62 | `		z64[3] = zBase64[ zIn[i + 2] & 0x3F];` |
|        - |   63 |  |
|        3 |   64 | `		rc = xConsumer((const void *)z64,sizeof(z64),pUserData);` |
|        3 |   65 | `		if( rc != SXRET_OK ){return SXERR_ABORT;}` |
|        - |   66 |  |
|        2 |   67 | `	}` |
|        7 |   68 | `	if ( i+1 < nLen ){` |
|        3 |   69 | `		z64[0] = zBase64[(zIn[i] >> 2) & 0x3F];` |
|        3 |   70 | `		z64[1] = zBase64[( ((zIn[i] & 0x03) << 4)   \| (zIn[i+1] >> 4)) & 0x3F];` |
|        3 |   71 | `		z64[2] = zBase64[(zIn[i+1] & 0x0F) << 2 ];` |
|        3 |   72 | `		z64[3] = '=';` |
|        - |   73 |  |
|        3 |   74 | `		rc = xConsumer((const void *)z64,sizeof(z64),pUserData);` |
|        3 |   75 | `		if( rc != SXRET_OK ){return SXERR_ABORT;}` |
|        - |   76 |  |
|        6 |   77 | `	}else if( i < nLen ){` |
|        3 |   78 | `		z64[0] = zBase64[(zIn[i] >> 2) & 0x3F];` |
|        3 |   79 | `		z64[1]   = zBase64[(zIn[i] & 0x03) << 4];` |
|        3 |   80 | `		z64[2] = '=';` |
|        3 |   81 | `		z64[3] = '=';` |
|        - |   82 |  |
|        3 |   83 | `		rc = xConsumer((const void *)z64,sizeof(z64),pUserData);` |
|        3 |   84 | `		if( rc != SXRET_OK ){return SXERR_ABORT;}` |
|        1 |   85 | `	}` |
|        - |   86 |  |
|        7 |   87 | `	return SXRET_OK;` |
|        4 |   88 |  |
|       32 |   89 | `PH7_PRIVATE sxi32 SyBase64Decode(const char *zB64,sxu32 nLen,ProcConsumer xConsumer,void *pUserData)` |
|        2 |   90 |  |
|        - |   91 | `	static const sxu32 aBase64Trans[] = {` |
|        - |   92 |  |
|        - |   93 |  |
|        - |   94 | `	5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,0,0,0,0,0,0,26,27,` |
|        - |   95 | `	28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,0,0,` |
|        - |   96 |  |
|        - |   97 | `	};` |
|        - |   98 | `	sxu32 n,w,x,y,z;` |
|        - |   99 | `	sxi32 rc;` |
|        - |  100 | `	unsigned char zOut[10];` |
|        - |  101 | `#if defined(UNTRUST)` |
|        - |  102 | `	if( SX_EMPTY_STR(zB64) \|\| xConsumer == 0 ){` |
|        - |  103 | `		return SXERR_EMPTY;` |
|        - |  104 | `	}` |
|        - |  105 | `#endif` |
|       64 |  106 | `	while(nLen > 0 && zB64[nLen - 1] == '=' ){` |
|       32 |  107 | `		nLen--;` |
|        2 |  108 | `	}` |
|     1002 |  109 | `	for( n = 0 ; n+3<nLen ; n += 4){` |
|      970 |  110 | `		w = aBase64Trans[zB64[n] & 0x7F];` |
|      970 |  111 | `		x = aBase64Trans[zB64[n+1] & 0x7F];` |
|      970 |  112 | `		y = aBase64Trans[zB64[n+2] & 0x7F];` |
|      970 |  113 | `		z = aBase64Trans[zB64[n+3] & 0x7F];` |
|      970 |  114 | `		zOut[0] = ((w<<2) & 0xFC) \| ((x>>4) & 0x03);` |
|      970 |  115 | `		zOut[1] = ((x<<4) & 0xF0) \| ((y>>2) & 0x0F);` |
|      970 |  116 | `		zOut[2] = ((y<<6) & 0xC0) \| (z & 0x3F);` |
|        - |  117 |  |
|      970 |  118 | `		rc = xConsumer((const void *)zOut,sizeof(unsigned char)*3,pUserData);` |
|      970 |  119 | `		if( rc != SXRET_OK ){ return SXERR_ABORT;}` |
|      486 |  120 | `	}` |
|       34 |  121 | `	if( n+2 < nLen ){` |
|       26 |  122 | `		w = aBase64Trans[zB64[n] & 0x7F];` |
|       26 |  123 | `		x = aBase64Trans[zB64[n+1] & 0x7F];` |
|       26 |  124 | `		y = aBase64Trans[zB64[n+2] & 0x7F];` |
|        - |  125 |  |
|       26 |  126 | `		zOut[0] = ((w<<2) & 0xFC) \| ((x>>4) & 0x03);` |
|       26 |  127 | `		zOut[1] = ((x<<4) & 0xF0) \| ((y>>2) & 0x0F);` |
|        - |  128 |  |
|       26 |  129 | `		rc = xConsumer((const void *)zOut,sizeof(unsigned char)*2,pUserData);` |
|       26 |  130 | `		if( rc != SXRET_OK ){ return SXERR_ABORT;}` |
|       21 |  131 | `	}else if( n+1 < nLen ){` |
|        5 |  132 | `		w = aBase64Trans[zB64[n] & 0x7F];` |
|        5 |  133 | `		x = aBase64Trans[zB64[n+1] & 0x7F];` |
|        - |  134 |  |
|        5 |  135 | `		zOut[0] = ((w<<2) & 0xFC) \| ((x>>4) & 0x03);` |
|        - |  136 |  |
|        5 |  137 | `		rc = xConsumer((const void *)zOut,sizeof(unsigned char)*1,pUserData);` |
|        5 |  138 | `		if( rc != SXRET_OK ){ return SXERR_ABORT;}` |
|        2 |  139 | `	}` |
|       34 |  140 | `	return SXRET_OK;` |
|       18 |  141 |  |
|        - |  142 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - |  143 | `#define INVALID_LEXER(LEX)	(  LEX == 0  \|\| LEX->xTokenizer == 0 )` |
|    12968 |  144 | `PH7_PRIVATE sxi32 SyLexInit(SyLex *pLex,SySet *pSet,ProcTokenizer xTokenizer,void *pUserData)` |
|        2 |  145 |  |
|        - |  146 | `	SyStream *pStream;` |
|        - |  147 | `#if defined (UNTRUST)` |
|        - |  148 | `	if ( pLex == 0 \|\| xTokenizer == 0 ){` |
|        - |  149 | `		return SXERR_CORRUPT;` |
|        - |  150 | `	}` |
|        - |  151 | `#endif` |
|    12970 |  152 | `	pLex->pTokenSet = 0;` |
|        - |  153 | `	/* Initialize lexer fields */` |
|    12970 |  154 | `	if( pSet ){` |
|    12970 |  155 | `		if ( SySetElemSize(pSet) != sizeof(SyToken) ){` |
|      ! 0 |  156 | `			return SXERR_INVALID;` |
|        - |  157 | `		}` |
|    12970 |  158 | `		pLex->pTokenSet = pSet;` |
|     6484 |  159 | `	}` |
|    12970 |  160 | `	pStream = &pLex->sStream;` |
|    12970 |  161 | `	pLex->xTokenizer = xTokenizer;` |
|    12970 |  162 | `	pLex->pUserData = pUserData;` |
|        - |  163 |  |
|    12970 |  164 | `	pStream->nLine = 1;` |
|    12970 |  165 | `	pStream->nIgn  = 0;` |
|    12970 |  166 | `	pStream->zText = pStream->zEnd = 0;` |
|    12970 |  167 | `	pStream->pSet  = pSet;` |
|    12970 |  168 | `	return SXRET_OK;` |
|     6486 |  169 |  |
|    12958 |  170 | `PH7_PRIVATE sxi32 SyLexTokenizeInput(SyLex *pLex,const char *zInput,sxu32 nLen,void *pCtxData,ProcSort xSort,ProcCmp xCmp)` |
|        2 |  171 |  |
|        - |  172 | `	const unsigned char *zCur;` |
|        - |  173 | `	SyStream *pStream;` |
|        - |  174 | `	SyToken sToken;` |
|        - |  175 | `	sxi32 rc;` |
|        - |  176 | `#if defined (UNTRUST)` |
|        - |  177 | `	if ( INVALID_LEXER(pLex) \|\| zInput == 0 ){` |
|        - |  178 | `		return SXERR_CORRUPT;` |
|        - |  179 | `	}` |
|        - |  180 | `#endif` |
|    12960 |  181 | `	pStream = &pLex->sStream;` |
|        - |  182 | `	/* Point to the head of the input */` |
|    12960 |  183 | `	pStream->zText = pStream->zInput = (const unsigned char *)zInput;` |
|        - |  184 | `	/* Point to the end of the input */` |
|    12960 |  185 | `	pStream->zEnd = &pStream->zInput[nLen];` |
|  3329456 |  186 | `	for(;;){` |
|  6658914 |  187 | `		if( pStream->zText >= pStream->zEnd ){` |
|        - |  188 | `			/* End of the input reached */` |
|    12934 |  189 | `			break;` |
|        - |  190 | `		}` |
|  6645982 |  191 | `		zCur = pStream->zText;` |
|        - |  192 | `		/* Call the tokenizer callback */` |
|  6645982 |  193 | `		rc = pLex->xTokenizer(pStream,&sToken,pLex->pUserData,pCtxData);` |
|  6645982 |  194 | `		if( rc != SXRET_OK && rc != SXERR_CONTINUE ){` |
|        - |  195 | `			/* Tokenizer callback request an operation abort */` |
|       28 |  196 | `			if( rc == SXERR_ABORT ){` |
|       28 |  197 | `				return SXERR_ABORT;` |
|        - |  198 | `			}` |
|      ! 0 |  199 | `			break;` |
|        - |  200 | `		}` |
|  6645956 |  201 | `		if( rc == SXERR_CONTINUE ){` |
|        - |  202 | `			/* Request to ignore this token */` |
|    65640 |  203 | `			pStream->nIgn++;` |
|  6613137 |  204 | `		}else if( pLex->pTokenSet  ){` |
|        - |  205 | `			/* Put the token in the set */` |
|  6580318 |  206 | `			rc = SySetPut(pLex->pTokenSet,(const void *)&sToken);` |
|  6580318 |  207 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  208 | `				break;` |
|        - |  209 | `			}` |
|  3290158 |  210 | `		}` |
|  6645956 |  211 | `		if( zCur >= pStream->zText ){` |
|        - |  212 | `			/* Automatic advance of the stream cursor */` |
|      ! 0 |  213 | `			pStream->zText = &zCur[1];` |
|      ! 0 |  214 | `		}` |
|        2 |  215 | `	}` |
|    12934 |  216 | `	if( xSort &&  pLex->pTokenSet ){` |
|      ! 0 |  217 | `		SyToken *aToken = (SyToken *)SySetBasePtr(pLex->pTokenSet);` |
|        - |  218 | `		/* Sort the extracted tokens */` |
|      ! 0 |  219 | `		if( xCmp == 0 ){` |
|        - |  220 | `			/* Use a default comparison function */` |
|      ! 0 |  221 | `			xCmp = SyMemcmp;` |
|      ! 0 |  222 | `		}` |
|      ! 0 |  223 | `		xSort(aToken,SySetUsed(pLex->pTokenSet),sizeof(SyToken),xCmp);` |
|      ! 0 |  224 | `	}` |
|    12934 |  225 | `	return SXRET_OK;` |
|     6481 |  226 |  |
|    12968 |  227 | `PH7_PRIVATE sxi32 SyLexRelease(SyLex *pLex)` |
|        2 |  228 |  |
|    12970 |  229 | `	sxi32 rc = SXRET_OK;` |
|        - |  230 | `#if defined (UNTRUST)` |
|        - |  231 | `	if ( INVALID_LEXER(pLex) ){` |
|        - |  232 | `		return SXERR_CORRUPT;` |
|        - |  233 | `	}` |
|        - |  234 | `#else` |
|     6484 |  235 | `	SXUNUSED(pLex); /* Prevent compiler warning */` |
|        - |  236 | `#endif` |
|    12970 |  237 | `	return rc;` |
|        2 |  238 |  |
|        - |  239 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  240 | `#define SAFE_HTTP(C)	(SyisAlphaNum(c) \|\| c == '_' \|\| c == '-' \|\| c == '$' \|\| c == '.' )` |
|        4 |  241 | `PH7_PRIVATE sxi32 SyUriEncode(const char *zSrc,sxu32 nLen,ProcConsumer xConsumer,void *pUserData)` |
|        1 |  242 |  |
|        5 |  243 | `	unsigned char *zIn = (unsigned char *)zSrc;` |
|        5 |  244 | `	unsigned char zHex[3] = { '%',0,0 };` |
|        - |  245 | `	unsigned char zOut[2];` |
|        - |  246 | `	unsigned char *zCur,*zEnd;` |
|        - |  247 | `	sxi32 c;` |
|        - |  248 | `	sxi32 rc;` |
|        - |  249 | `#ifdef UNTRUST` |
|        - |  250 | `	if( SX_EMPTY_STR(zSrc) \|\| xConsumer == 0 ){` |
|        - |  251 | `		return SXERR_EMPTY;` |
|        - |  252 | `	}` |
|        - |  253 | `#endif` |
|        5 |  254 | `	rc = SXRET_OK;` |
|        5 |  255 | `	zEnd = &zIn[nLen]; zCur = zIn;` |
|        4 |  256 | `	for(;;){` |
|       17 |  257 | `		if( zCur >= zEnd ){` |
|        5 |  258 | `			if( zCur != zIn ){` |
|        5 |  259 | `				rc = xConsumer(zIn,(sxu32)(zCur-zIn),pUserData);` |
|        2 |  260 | `			}` |
|        5 |  261 | `			break;` |
|        - |  262 | `		}` |
|       13 |  263 | `		c = zCur[0];` |
|       13 |  264 | `		if( SAFE_HTTP(c) ){` |
|        9 |  265 | `			zCur++; continue;` |
|        - |  266 | `		}` |
|        5 |  267 | `		if( zCur != zIn && SXRET_OK != (rc = xConsumer(zIn,(sxu32)(zCur-zIn),pUserData))){` |
|      ! 0 |  268 | `			break;` |
|        - |  269 | `		}` |
|        5 |  270 | `		if( c == ' ' ){` |
|        3 |  271 | `			zOut[0] = '+';` |
|        3 |  272 | `			rc = xConsumer((const void *)zOut,sizeof(unsigned char),pUserData);` |
|        2 |  273 | `		}else{` |
|        3 |  274 | `			zHex[1]	= "0123456789ABCDEF"[(c >> 4) & 0x0F];` |
|        3 |  275 | `			zHex[2] = "0123456789ABCDEF"[c & 0x0F];` |
|        3 |  276 | `			rc = xConsumer(zHex,sizeof(zHex),pUserData);` |
|        - |  277 | `		}` |
|        5 |  278 | `		if( SXRET_OK != rc ){` |
|      ! 0 |  279 | `			break;` |
|        - |  280 | `		}` |
|        5 |  281 | `		zIn = &zCur[1]; zCur = zIn ;` |
|        1 |  282 | `	}` |
|        5 |  283 | `	return rc == SXRET_OK ? SXRET_OK : SXERR_ABORT;` |
|        1 |  284 |  |
|        - |  285 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|       20 |  286 | `static sxi32 SyAsciiToHex(sxi32 c)` |
|        1 |  287 |  |
|       21 |  288 | `	if( c >= 'a' && c <= 'f' ){` |
|      ! 0 |  289 | `		c += 10 - 'a';` |
|      ! 0 |  290 | `		return c;` |
|        - |  291 | `	}` |
|       21 |  292 | `	if( c >= '0' && c <= '9' ){` |
|       11 |  293 | `		c -= '0';` |
|       11 |  294 | `		return c;` |
|        - |  295 | `	}` |
|       11 |  296 | `	if( c >= 'A' && c <= 'F') {` |
|       11 |  297 | `		c += 10 - 'A';` |
|       11 |  298 | `		return c;` |
|        - |  299 | `	}` |
|      ! 0 |  300 | `	return 0;` |
|       11 |  301 |  |
|       10 |  302 | `PH7_PRIVATE sxi32 SyUriDecode(const char *zSrc,sxu32 nLen,ProcConsumer xConsumer,void *pUserData,int bUTF8)` |
|        1 |  303 |  |
|        - |  304 | `	static const sxu8 Utf8Trans[] = {` |
|        - |  305 |  |
|        - |  306 |  |
|        - |  307 |  |
|        - |  308 |  |
|        - |  309 |  |
|        - |  310 |  |
|        - |  311 |  |
|        - |  312 |  |
|        - |  313 | `	};` |
|       11 |  314 | `	const char *zIn = zSrc;` |
|        - |  315 | `	const char *zEnd;` |
|        - |  316 | `	const char *zCur;` |
|        - |  317 | `	sxu8 *zOutPtr;` |
|        - |  318 | `	sxu8 zOut[10];` |
|        - |  319 | `	sxi32 c,d;` |
|        - |  320 | `	sxi32 rc;` |
|        - |  321 | `#if defined(UNTRUST)` |
|        - |  322 | `	if( SX_EMPTY_STR(zSrc) \|\| xConsumer == 0 ){` |
|        - |  323 | `		return SXERR_EMPTY;` |
|        - |  324 | `	}` |
|        - |  325 | `#endif` |
|       11 |  326 | `	rc = SXRET_OK;` |
|       11 |  327 | `	zEnd = &zSrc[nLen];` |
|       11 |  328 | `	zCur = zIn;` |
|        8 |  329 | `	for(;;){` |
|       39 |  330 | `		while(zCur < zEnd && zCur[0] != '%' && zCur[0] != '+' ){` |
|       23 |  331 | `			zCur++;` |
|        1 |  332 | `		}` |
|       17 |  333 | `		if( zCur != zIn ){` |
|        - |  334 | `			/* Consume input */` |
|       11 |  335 | `			rc = xConsumer(zIn,(unsigned int)(zCur-zIn),pUserData);` |
|       11 |  336 | `			if( rc != SXRET_OK ){` |
|        - |  337 | `				/* User consumer routine request an operation abort */` |
|      ! 0 |  338 | `				break;` |
|        - |  339 | `			}` |
|        5 |  340 | `		}` |
|       17 |  341 | `		if( zCur >= zEnd ){` |
|       11 |  342 | `			rc = SXRET_OK;` |
|       11 |  343 | `			break;` |
|        - |  344 | `		}` |
|        - |  345 | `		/* Decode unsafe HTTP characters */` |
|        7 |  346 | `		zOutPtr = zOut;` |
|        7 |  347 | `		if( zCur[0] == '+' ){` |
|        3 |  348 | `			*zOutPtr++ = ' ';` |
|        3 |  349 | `			zCur++;` |
|        2 |  350 | `		}else{` |
|        5 |  351 | `			if( &zCur[2] >= zEnd ){` |
|      ! 0 |  352 | `				rc = SXERR_OVERFLOW;` |
|      ! 0 |  353 | `				break;` |
|        - |  354 | `			}` |
|        5 |  355 | `			c = (SyAsciiToHex(zCur[1]) <<4) \| SyAsciiToHex(zCur[2]);` |
|        5 |  356 | `			zCur += 3;` |
|        5 |  357 | `			if( c < 0x000C0 ){` |
|      ! 0 |  358 | `				*zOutPtr++ = (sxu8)c;` |
|      ! 0 |  359 | `			}else{` |
|        5 |  360 | `				c = Utf8Trans[c-0xC0];` |
|       11 |  361 | `				while( zCur[0] == '%' ){` |
|        7 |  362 | `					d = (SyAsciiToHex(zCur[1]) <<4) \| SyAsciiToHex(zCur[2]);` |
|        7 |  363 | `					if( (d&0xC0) != 0x80 ){` |
|      ! 0 |  364 | `						break;` |
|        - |  365 | `					}` |
|        7 |  366 | `					c = (c<<6) + (0x3f & d);` |
|        7 |  367 | `					zCur += 3;` |
|        1 |  368 | `				}` |
|        5 |  369 | `				if( bUTF8 == FALSE ){` |
|      ! 0 |  370 | `					*zOutPtr++ = (sxu8)c;` |
|      ! 0 |  371 | `				}else{` |
|        5 |  372 | `					SX_WRITE_UTF8(zOutPtr,c);` |
|        - |  373 | `				}` |
|        - |  374 | `			}` |
|        - |  375 |  |
|        - |  376 | `		}` |
|        - |  377 | `		/* Consume the decoded characters */` |
|        7 |  378 | `		rc = xConsumer((const void *)zOut,(unsigned int)(zOutPtr-zOut),pUserData);` |
|        7 |  379 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  380 | `			break;` |
|        - |  381 | `		}` |
|        - |  382 | `		/* Synchronize pointers */` |
|        7 |  383 | `		zIn = zCur;` |
|        1 |  384 | `	}` |
|       11 |  385 | `	return rc;` |
|        1 |  386 |  |
|        - |  387 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  388 | `static const char *zEngDay[] = {` |
|        - |  389 | `	"Sunday","Monday","Tuesday","Wednesday",` |
|        - |  390 | `	"Thursday","Friday","Saturday"` |
|        - |  391 | `};` |
|        - |  392 | `static const char *zEngMonth[] = {` |
|        - |  393 | `	"January","February","March","April",` |
|        - |  394 | `	"May","June","July","August",` |
|        - |  395 | `	"September","October","November","December"` |
|        - |  396 | `};` |
|       14 |  397 | `static const char * GetDay(sxi32 i)` |
|        1 |  398 |  |
|       15 |  399 | `	return zEngDay[ i % 7 ];` |
|        1 |  400 |  |
|       12 |  401 | `static const char * GetMonth(sxi32 i)` |
|        1 |  402 |  |
|       13 |  403 | `	return zEngMonth[ i % 12 ];` |
|        1 |  404 |  |
|       14 |  405 | `PH7_PRIVATE const char * SyTimeGetDay(sxi32 iDay)` |
|        1 |  406 |  |
|       15 |  407 | `	return GetDay(iDay);` |
|        1 |  408 |  |
|       12 |  409 | `PH7_PRIVATE const char * SyTimeGetMonth(sxi32 iMonth)` |
|        1 |  410 |  |
|       13 |  411 | `	return GetMonth(iMonth);` |
|        1 |  412 |  |
|        - |  413 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - |  414 |  |
