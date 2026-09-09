# src/sx/sxlib.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 238/261 lines (91.19%)

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
|         - |   16 |  |
|  82395071 |   17 | `PH7_PRIVATE sxu32 SyBinHash(const void *pSrc,sxu32 nLen)` |
|         5 |   18 | `{` |
|  82395076 |   19 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|         - |   20 | `	unsigned char *zEnd;` |
|  82395076 |   21 | `	sxu32 nH = 5381;` |
|  82395076 |   22 | `	zEnd = &zIn[nLen];` |
| 139622253 |   23 | `	for(;;){` |
| 279243001 |   24 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
| 256947970 |   25 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
| 233719016 |   26 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
| 215349202 |   27 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|         5 |   28 | `	}` |
|  82395076 |   29 | `	return nH;` |
|         5 |   30 | `}` |
|   1494148 |   31 | `PH7_PRIVATE sxu32 SyStrHash(const void *pSrc,sxu32 nLen)` |
|         5 |   32 | `{` |
|   1494153 |   33 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|         - |   34 | `	unsigned char *zEnd;` |
|   1494153 |   35 | `	sxu32 nH = 5381;` |
|   1494153 |   36 | `	zEnd = &zIn[nLen];` |
|   2974052 |   37 | `	for(;;){` |
|   5948109 |   38 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + SyToLower(zIn[0]); zIn++;` |
|   5635117 |   39 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + SyToLower(zIn[0]); zIn++;` |
|   5242995 |   40 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + SyToLower(zIn[0]); zIn++;` |
|   4743837 |   41 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + SyToLower(zIn[0]); zIn++;` |
|         5 |   42 | `	}` |
|   1494153 |   43 | `	return nH;` |
|         5 |   44 | `}` |
|         - |   45 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         6 |   46 | `PH7_PRIVATE sxi32 SyBase64Encode(const char *zSrc,sxu32 nLen,ProcConsumer xConsumer,void *pUserData)` |
|         1 |   47 | `{` |
|         - |   48 | `	static const unsigned char zBase64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";` |
|         7 |   49 | `	unsigned char *zIn = (unsigned char *)zSrc;` |
|         - |   50 | `	unsigned char z64[4];` |
|         - |   51 | `	sxu32 i;` |
|         - |   52 | `	sxi32 rc;` |
|         - |   53 | `#if defined(UNTRUST)` |
|         - |   54 | `	if( SX_EMPTY_STR(zSrc) \|\| xConsumer == 0){` |
|         - |   55 | `		return SXERR_EMPTY;` |
|         - |   56 | `	}` |
|         - |   57 | `#endif` |
|         9 |   58 | `	for(i = 0; i + 2 < nLen; i += 3){` |
|         3 |   59 | `		z64[0] = zBase64[(zIn[i] >> 2) & 0x3F];` |
|         3 |   60 | `		z64[1] = zBase64[( ((zIn[i] & 0x03) << 4)   \| (zIn[i+1] >> 4)) & 0x3F];` |
|         3 |   61 | `		z64[2] = zBase64[( ((zIn[i+1] & 0x0F) << 2) \| (zIn[i + 2] >> 6) ) & 0x3F];` |
|         3 |   62 | `		z64[3] = zBase64[ zIn[i + 2] & 0x3F];` |
|         - |   63 |  |
|         3 |   64 | `		rc = xConsumer((const void *)z64,sizeof(z64),pUserData);` |
|         3 |   65 | `		if( rc != SXRET_OK ){return SXERR_ABORT;}` |
|         - |   66 |  |
|         2 |   67 | `	}` |
|         7 |   68 | `	if ( i+1 < nLen ){` |
|         3 |   69 | `		z64[0] = zBase64[(zIn[i] >> 2) & 0x3F];` |
|         3 |   70 | `		z64[1] = zBase64[( ((zIn[i] & 0x03) << 4)   \| (zIn[i+1] >> 4)) & 0x3F];` |
|         3 |   71 | `		z64[2] = zBase64[(zIn[i+1] & 0x0F) << 2 ];` |
|         3 |   72 | `		z64[3] = '=';` |
|         - |   73 |  |
|         3 |   74 | `		rc = xConsumer((const void *)z64,sizeof(z64),pUserData);` |
|         3 |   75 | `		if( rc != SXRET_OK ){return SXERR_ABORT;}` |
|         - |   76 |  |
|         6 |   77 | `	}else if( i < nLen ){` |
|         3 |   78 | `		z64[0] = zBase64[(zIn[i] >> 2) & 0x3F];` |
|         3 |   79 | `		z64[1]   = zBase64[(zIn[i] & 0x03) << 4];` |
|         3 |   80 | `		z64[2] = '=';` |
|         3 |   81 | `		z64[3] = '=';` |
|         - |   82 |  |
|         3 |   83 | `		rc = xConsumer((const void *)z64,sizeof(z64),pUserData);` |
|         3 |   84 | `		if( rc != SXRET_OK ){return SXERR_ABORT;}` |
|         1 |   85 | `	}` |
|         - |   86 |  |
|         7 |   87 | `	return SXRET_OK;` |
|         4 |   88 | `}` |
|        34 |   89 | `PH7_PRIVATE sxi32 SyBase64Decode(const char *zB64,sxu32 nLen,ProcConsumer xConsumer,void *pUserData)` |
|         2 |   90 | `{` |
|         - |   91 | `	static const sxu32 aBase64Trans[] = {` |
|         - |   92 | `	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,` |
|         - |   93 | `	0,0,0,0,0,62,0,0,0,63,52,53,54,55,56,57,58,59,60,61,0,0,0,0,0,0,0,0,1,2,3,4,` |
|         - |   94 | `	5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,0,0,0,0,0,0,26,27,` |
|         - |   95 | `	28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,0,0,` |
|         - |   96 | `	0,0,0` |
|         - |   97 | `	};` |
|         - |   98 | `	sxu32 n,w,x,y,z;` |
|         - |   99 | `	sxi32 rc;` |
|         - |  100 | `	unsigned char zOut[10];` |
|         - |  101 | `#if defined(UNTRUST)` |
|         - |  102 | `	if( SX_EMPTY_STR(zB64) \|\| xConsumer == 0 ){` |
|         - |  103 | `		return SXERR_EMPTY;` |
|         - |  104 | `	}` |
|         - |  105 | `#endif` |
|        66 |  106 | `	while(nLen > 0 && zB64[nLen - 1] == '=' ){` |
|        32 |  107 | `		nLen--;` |
|         2 |  108 | `	}` |
|      1012 |  109 | `	for( n = 0 ; n+3<nLen ; n += 4){` |
|       978 |  110 | `		w = aBase64Trans[zB64[n] & 0x7F];` |
|       978 |  111 | `		x = aBase64Trans[zB64[n+1] & 0x7F];` |
|       978 |  112 | `		y = aBase64Trans[zB64[n+2] & 0x7F];` |
|       978 |  113 | `		z = aBase64Trans[zB64[n+3] & 0x7F];` |
|       978 |  114 | `		zOut[0] = ((w<<2) & 0xFC) \| ((x>>4) & 0x03);` |
|       978 |  115 | `		zOut[1] = ((x<<4) & 0xF0) \| ((y>>2) & 0x0F);` |
|       978 |  116 | `		zOut[2] = ((y<<6) & 0xC0) \| (z & 0x3F);` |
|         - |  117 |  |
|       978 |  118 | `		rc = xConsumer((const void *)zOut,sizeof(unsigned char)*3,pUserData);` |
|       978 |  119 | `		if( rc != SXRET_OK ){ return SXERR_ABORT;}` |
|       490 |  120 | `	}` |
|        36 |  121 | `	if( n+2 < nLen ){` |
|        26 |  122 | `		w = aBase64Trans[zB64[n] & 0x7F];` |
|        26 |  123 | `		x = aBase64Trans[zB64[n+1] & 0x7F];` |
|        26 |  124 | `		y = aBase64Trans[zB64[n+2] & 0x7F];` |
|         - |  125 |  |
|        26 |  126 | `		zOut[0] = ((w<<2) & 0xFC) \| ((x>>4) & 0x03);` |
|        26 |  127 | `		zOut[1] = ((x<<4) & 0xF0) \| ((y>>2) & 0x0F);` |
|         - |  128 |  |
|        26 |  129 | `		rc = xConsumer((const void *)zOut,sizeof(unsigned char)*2,pUserData);` |
|        26 |  130 | `		if( rc != SXRET_OK ){ return SXERR_ABORT;}` |
|        23 |  131 | `	}else if( n+1 < nLen ){` |
|         5 |  132 | `		w = aBase64Trans[zB64[n] & 0x7F];` |
|         5 |  133 | `		x = aBase64Trans[zB64[n+1] & 0x7F];` |
|         - |  134 |  |
|         5 |  135 | `		zOut[0] = ((w<<2) & 0xFC) \| ((x>>4) & 0x03);` |
|         - |  136 |  |
|         5 |  137 | `		rc = xConsumer((const void *)zOut,sizeof(unsigned char)*1,pUserData);` |
|         5 |  138 | `		if( rc != SXRET_OK ){ return SXERR_ABORT;}` |
|         2 |  139 | `	}` |
|        36 |  140 | `	return SXRET_OK;` |
|        19 |  141 | `}` |
|         - |  142 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - |  143 | `#define INVALID_LEXER(LEX)	(  LEX == 0  \|\| LEX->xTokenizer == 0 )` |
|     81814 |  144 | `PH7_PRIVATE sxi32 SyLexInit(SyLex *pLex,SySet *pSet,ProcTokenizer xTokenizer,void *pUserData)` |
|         5 |  145 | `{` |
|         - |  146 | `	SyStream *pStream;` |
|         - |  147 | `#if defined (UNTRUST)` |
|         - |  148 | `	if ( pLex == 0 \|\| xTokenizer == 0 ){` |
|         - |  149 | `		return SXERR_CORRUPT;` |
|         - |  150 | `	}` |
|         - |  151 | `#endif` |
|     81819 |  152 | `	pLex->pTokenSet = 0;` |
|         - |  153 | `	/* Initialize lexer fields */` |
|     81819 |  154 | `	if( pSet ){` |
|     81819 |  155 | `		if ( SySetElemSize(pSet) != sizeof(SyToken) ){` |
|       ! 0 |  156 | `			return SXERR_INVALID;` |
|         - |  157 | `		}` |
|     81819 |  158 | `		pLex->pTokenSet = pSet;` |
|     40907 |  159 | `	}` |
|     81819 |  160 | `	pStream = &pLex->sStream;` |
|     81819 |  161 | `	pLex->xTokenizer = xTokenizer;` |
|     81819 |  162 | `	pLex->pUserData = pUserData;` |
|         - |  163 |  |
|     81819 |  164 | `	pStream->nLine = 1;` |
|     81819 |  165 | `	pStream->nIgn  = 0;` |
|     81819 |  166 | `	pStream->zText = pStream->zEnd = 0;` |
|     81819 |  167 | `	pStream->pSet  = pSet;` |
|     81819 |  168 | `	return SXRET_OK;` |
|     40912 |  169 | `}` |
|     81804 |  170 | `PH7_PRIVATE sxi32 SyLexTokenizeInput(SyLex *pLex,const char *zInput,sxu32 nLen,void *pCtxData,ProcSort xSort,ProcCmp xCmp)` |
|         5 |  171 | `{` |
|         - |  172 | `	const unsigned char *zCur;` |
|         - |  173 | `	SyStream *pStream;` |
|         - |  174 | `	SyToken sToken;` |
|         - |  175 | `	sxi32 rc;` |
|         - |  176 | `#if defined (UNTRUST)` |
|         - |  177 | `	if ( INVALID_LEXER(pLex) \|\| zInput == 0 ){` |
|         - |  178 | `		return SXERR_CORRUPT;` |
|         - |  179 | `	}` |
|         - |  180 | `#endif` |
|     81809 |  181 | `	pStream = &pLex->sStream;` |
|         - |  182 | `	/* Point to the head of the input */` |
|     81809 |  183 | `	pStream->zText = pStream->zInput = (const unsigned char *)zInput;` |
|         - |  184 | `	/* Point to the end of the input */` |
|     81809 |  185 | `	pStream->zEnd = &pStream->zInput[nLen];` |
|  79457091 |  186 | `	for(;;){` |
| 158914187 |  187 | `		if( pStream->zText >= pStream->zEnd ){` |
|         - |  188 | `			/* End of the input reached */` |
|     81775 |  189 | `			break;` |
|         - |  190 | `		}` |
| 158832417 |  191 | `		zCur = pStream->zText;` |
|         - |  192 | `		/* Call the tokenizer callback */` |
| 158832417 |  193 | `		rc = pLex->xTokenizer(pStream,&sToken,pLex->pUserData,pCtxData);` |
| 158832417 |  194 | `		if( rc != SXRET_OK && rc != SXERR_CONTINUE ){` |
|         - |  195 | `			/* Tokenizer callback request an operation abort */` |
|        36 |  196 | `			if( rc == SXERR_ABORT ){` |
|        36 |  197 | `				return SXERR_ABORT;` |
|         - |  198 | `			}` |
|       ! 0 |  199 | `			break;` |
|         - |  200 | `		}` |
| 158832383 |  201 | `		if( rc == SXERR_CONTINUE ){` |
|         - |  202 | `			/* Request to ignore this token */` |
|    205537 |  203 | `			pStream->nIgn++;` |
| 158729617 |  204 | `		}else if( pLex->pTokenSet  ){` |
|         - |  205 | `			/* Put the token in the set */` |
| 158626851 |  206 | `			rc = SySetPut(pLex->pTokenSet,(const void *)&sToken);` |
| 158626851 |  207 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  208 | `				break;` |
|         - |  209 | `			}` |
|  79313423 |  210 | `		}` |
| 158832383 |  211 | `		if( zCur >= pStream->zText ){` |
|         - |  212 | `			/* Automatic advance of the stream cursor */` |
|       ! 0 |  213 | `			pStream->zText = &zCur[1];` |
|       ! 0 |  214 | `		}` |
|         5 |  215 | `	}` |
|     81775 |  216 | `	if( xSort &&  pLex->pTokenSet ){` |
|       ! 0 |  217 | `		SyToken *aToken = (SyToken *)SySetBasePtr(pLex->pTokenSet);` |
|         - |  218 | `		/* Sort the extracted tokens */` |
|       ! 0 |  219 | `		if( xCmp == 0 ){` |
|         - |  220 | `			/* Use a default comparison function */` |
|       ! 0 |  221 | `			xCmp = SyMemcmp;` |
|       ! 0 |  222 | `		}` |
|       ! 0 |  223 | `		xSort(aToken,SySetUsed(pLex->pTokenSet),sizeof(SyToken),xCmp);` |
|       ! 0 |  224 | `	}` |
|     81775 |  225 | `	return SXRET_OK;` |
|     40907 |  226 | `}` |
|     81814 |  227 | `PH7_PRIVATE sxi32 SyLexRelease(SyLex *pLex)` |
|         5 |  228 | `{` |
|     81819 |  229 | `	sxi32 rc = SXRET_OK;` |
|         - |  230 | `#if defined (UNTRUST)` |
|         - |  231 | `	if ( INVALID_LEXER(pLex) ){` |
|         - |  232 | `		return SXERR_CORRUPT;` |
|         - |  233 | `	}` |
|         - |  234 | `#else` |
|     40907 |  235 | `	SXUNUSED(pLex); /* Prevent compiler warning */` |
|         - |  236 | `#endif` |
|     81819 |  237 | `	return rc;` |
|         5 |  238 | `}` |
|         - |  239 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - |  240 | `/* php's urlencode() keeps only alnum and -_. safe (space becomes '+'); rawurlencode()` |
|         - |  241 | ` * additionally keeps '~' and encodes space as %20 (RFC 3986). '$' is NOT safe in` |
|         - |  242 | ` * either -- php encodes it %24. */` |
|         - |  243 | `#define SAFE_URL(C)	(SyisAlphaNum(c) \|\| c == '_' \|\| c == '-' \|\| c == '.' )` |
|         - |  244 | `#define SAFE_RAW(C)	(SyisAlphaNum(c) \|\| c == '_' \|\| c == '-' \|\| c == '.' \|\| c == '~' )` |
|       110 |  245 | `static sxi32 SyUriEncodeInternal(const char *zSrc,sxu32 nLen,ProcConsumer xConsumer,void *pUserData,int bRaw)` |
|         1 |  246 | `{` |
|       111 |  247 | `	unsigned char *zIn = (unsigned char *)zSrc;` |
|       111 |  248 | `	unsigned char zHex[3] = { '%',0,0 };` |
|         - |  249 | `	unsigned char zOut[2];` |
|         - |  250 | `	unsigned char *zCur,*zEnd;` |
|         - |  251 | `	sxi32 c;` |
|         - |  252 | `	sxi32 rc;` |
|         - |  253 | `#ifdef UNTRUST` |
|         - |  254 | `	if( SX_EMPTY_STR(zSrc) \|\| xConsumer == 0 ){` |
|         - |  255 | `		return SXERR_EMPTY;` |
|         - |  256 | `	}` |
|         - |  257 | `#endif` |
|       111 |  258 | `	rc = SXRET_OK;` |
|       111 |  259 | `	zEnd = &zIn[nLen]; zCur = zIn;` |
|        67 |  260 | `	for(;;){` |
|       313 |  261 | `		if( zCur >= zEnd ){` |
|       119 |  262 | `			if( zCur != zIn ){` |
|       107 |  263 | `				rc = xConsumer(zIn,(sxu32)(zCur-zIn),pUserData);` |
|        53 |  264 | `			}` |
|       119 |  265 | `			break;` |
|         - |  266 | `		}` |
|       195 |  267 | `		c = zCur[0];` |
|       195 |  268 | `		if( bRaw ? SAFE_RAW(c) : SAFE_URL(c) ){` |
|       179 |  269 | `			zCur++; continue;` |
|         - |  270 | `		}` |
|        25 |  271 | `		if( zCur != zIn && SXRET_OK != (rc = xConsumer(zIn,(sxu32)(zCur-zIn),pUserData))){` |
|       ! 0 |  272 | `			break;` |
|         - |  273 | `		}` |
|        25 |  274 | `		if( c == ' ' && !bRaw ){` |
|         7 |  275 | `			zOut[0] = '+';` |
|         7 |  276 | `			rc = xConsumer((const void *)zOut,sizeof(unsigned char),pUserData);` |
|         4 |  277 | `		}else{` |
|        19 |  278 | `			zHex[1]	= "0123456789ABCDEF"[(c >> 4) & 0x0F];` |
|        19 |  279 | `			zHex[2] = "0123456789ABCDEF"[c & 0x0F];` |
|        19 |  280 | `			rc = xConsumer(zHex,sizeof(zHex),pUserData);` |
|         - |  281 | `		}` |
|        25 |  282 | `		if( SXRET_OK != rc ){` |
|       ! 0 |  283 | `			break;` |
|         - |  284 | `		}` |
|        25 |  285 | `		zIn = &zCur[1]; zCur = zIn ;` |
|         1 |  286 | `	}` |
|       119 |  287 | `	return rc == SXRET_OK ? SXRET_OK : SXERR_ABORT;` |
|         1 |  288 | `}` |
|        98 |  289 | `PH7_PRIVATE sxi32 SyUriEncode(const char *zSrc,sxu32 nLen,ProcConsumer xConsumer,void *pUserData)` |
|         1 |  290 | `{` |
|        99 |  291 | `	return SyUriEncodeInternal(zSrc,nLen,xConsumer,pUserData,0);` |
|         1 |  292 | `}` |
|        12 |  293 | `PH7_PRIVATE sxi32 SyUriEncodeRaw(const char *zSrc,sxu32 nLen,ProcConsumer xConsumer,void *pUserData)` |
|         1 |  294 | `{` |
|        13 |  295 | `	return SyUriEncodeInternal(zSrc,nLen,xConsumer,pUserData,1);` |
|         1 |  296 | `}` |
|         - |  297 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        28 |  298 | `static sxi32 SyAsciiToHex(sxi32 c)` |
|         1 |  299 | `{` |
|        29 |  300 | `	if( c >= 'a' && c <= 'f' ){` |
|       ! 0 |  301 | `		c += 10 - 'a';` |
|       ! 0 |  302 | `		return c;` |
|         - |  303 | `	}` |
|        29 |  304 | `	if( c >= '0' && c <= '9' ){` |
|        19 |  305 | `		c -= '0';` |
|        19 |  306 | `		return c;` |
|         - |  307 | `	}` |
|        11 |  308 | `	if( c >= 'A' && c <= 'F') {` |
|        11 |  309 | `		c += 10 - 'A';` |
|        11 |  310 | `		return c;` |
|         - |  311 | `	}` |
|       ! 0 |  312 | `	return 0;` |
|        15 |  313 | `}` |
|       110 |  314 | `PH7_PRIVATE sxi32 SyUriDecode(const char *zSrc,sxu32 nLen,ProcConsumer xConsumer,void *pUserData,int bUTF8)` |
|         1 |  315 | `{` |
|         - |  316 | `	static const sxu8 Utf8Trans[] = {` |
|         - |  317 | `		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,` |
|         - |  318 | `		0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,` |
|         - |  319 | `		0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,` |
|         - |  320 | `		0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,` |
|         - |  321 | `		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,` |
|         - |  322 | `		0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,` |
|         - |  323 | `		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,` |
|         - |  324 | `		0x00, 0x01, 0x02, 0x03, 0x00, 0x01, 0x00, 0x00` |
|         - |  325 | `	};` |
|       111 |  326 | `	const char *zIn = zSrc;` |
|         - |  327 | `	const char *zEnd;` |
|         - |  328 | `	const char *zCur;` |
|         - |  329 | `	sxu8 *zOutPtr;` |
|         - |  330 | `	sxu8 zOut[10];` |
|         - |  331 | `	sxi32 c,d;` |
|         - |  332 | `	sxi32 rc;` |
|         - |  333 | `#if defined(UNTRUST)` |
|         - |  334 | `	if( SX_EMPTY_STR(zSrc) \|\| xConsumer == 0 ){` |
|         - |  335 | `		return SXERR_EMPTY;` |
|         - |  336 | `	}` |
|         - |  337 | `#endif` |
|       111 |  338 | `	rc = SXRET_OK;` |
|       111 |  339 | `	zEnd = &zSrc[nLen];` |
|       111 |  340 | `	zCur = zIn;` |
|        62 |  341 | `	for(;;){` |
|       341 |  342 | `		while(zCur < zEnd && zCur[0] != '%' && zCur[0] != '+' ){` |
|       217 |  343 | `			zCur++;` |
|         1 |  344 | `		}` |
|       125 |  345 | `		if( zCur != zIn ){` |
|         - |  346 | `			/* Consume input */` |
|       119 |  347 | `			rc = xConsumer(zIn,(unsigned int)(zCur-zIn),pUserData);` |
|       119 |  348 | `			if( rc != SXRET_OK ){` |
|         - |  349 | `				/* User consumer routine request an operation abort */` |
|       ! 0 |  350 | `				break;` |
|         - |  351 | `			}` |
|        59 |  352 | `		}` |
|       125 |  353 | `		if( zCur >= zEnd ){` |
|       111 |  354 | `			rc = SXRET_OK;` |
|       111 |  355 | `			break;` |
|         - |  356 | `		}` |
|         - |  357 | `		/* Decode unsafe HTTP characters */` |
|        15 |  358 | `		zOutPtr = zOut;` |
|        15 |  359 | `		if( zCur[0] == '+' ){` |
|         7 |  360 | `			*zOutPtr++ = ' ';` |
|         7 |  361 | `			zCur++;` |
|         4 |  362 | `		}else{` |
|         9 |  363 | `			if( &zCur[2] >= zEnd ){` |
|       ! 0 |  364 | `				rc = SXERR_OVERFLOW;` |
|       ! 0 |  365 | `				break;` |
|         - |  366 | `			}` |
|         9 |  367 | `			c = (SyAsciiToHex(zCur[1]) <<4) \| SyAsciiToHex(zCur[2]);` |
|         9 |  368 | `			zCur += 3;` |
|         9 |  369 | `			if( c < 0x000C0 ){` |
|         5 |  370 | `				*zOutPtr++ = (sxu8)c;` |
|         3 |  371 | `			}else{` |
|         5 |  372 | `				c = Utf8Trans[c-0xC0];` |
|        11 |  373 | `				while( zCur[0] == '%' ){` |
|         7 |  374 | `					d = (SyAsciiToHex(zCur[1]) <<4) \| SyAsciiToHex(zCur[2]);` |
|         7 |  375 | `					if( (d&0xC0) != 0x80 ){` |
|       ! 0 |  376 | `						break;` |
|         - |  377 | `					}` |
|         7 |  378 | `					c = (c<<6) + (0x3f & d);` |
|         7 |  379 | `					zCur += 3;` |
|         1 |  380 | `				}` |
|         5 |  381 | `				if( bUTF8 == FALSE ){` |
|       ! 0 |  382 | `					*zOutPtr++ = (sxu8)c;` |
|       ! 0 |  383 | `				}else{` |
|         5 |  384 | `					SX_WRITE_UTF8(zOutPtr,c);` |
|         - |  385 | `				}` |
|         - |  386 | `			}` |
|         - |  387 |  |
|         - |  388 | `		}` |
|         - |  389 | `		/* Consume the decoded characters */` |
|        15 |  390 | `		rc = xConsumer((const void *)zOut,(unsigned int)(zOutPtr-zOut),pUserData);` |
|        15 |  391 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  392 | `			break;` |
|         - |  393 | `		}` |
|         - |  394 | `		/* Synchronize pointers */` |
|        15 |  395 | `		zIn = zCur;` |
|         1 |  396 | `	}` |
|       111 |  397 | `	return rc;` |
|         1 |  398 | `}` |
|         - |  399 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - |  400 | `static const char *zEngDay[] = {` |
|         - |  401 | `	"Sunday","Monday","Tuesday","Wednesday",` |
|         - |  402 | `	"Thursday","Friday","Saturday"` |
|         - |  403 | `};` |
|         - |  404 | `static const char *zEngMonth[] = {` |
|         - |  405 | `	"January","February","March","April",` |
|         - |  406 | `	"May","June","July","August",` |
|         - |  407 | `	"September","October","November","December"` |
|         - |  408 | `};` |
|        76 |  409 | `static const char * GetDay(sxi32 i)` |
|         1 |  410 | `{` |
|        77 |  411 | `	return zEngDay[ i % 7 ];` |
|         1 |  412 | `}` |
|        12 |  413 | `static const char * GetMonth(sxi32 i)` |
|         1 |  414 | `{` |
|        13 |  415 | `	return zEngMonth[ i % 12 ];` |
|         1 |  416 | `}` |
|        76 |  417 | `PH7_PRIVATE const char * SyTimeGetDay(sxi32 iDay)` |
|         1 |  418 | `{` |
|        77 |  419 | `	return GetDay(iDay);` |
|         1 |  420 | `}` |
|        12 |  421 | `PH7_PRIVATE const char * SyTimeGetMonth(sxi32 iMonth)` |
|         1 |  422 | `{` |
|        13 |  423 | `	return GetMonth(iMonth);` |
|         1 |  424 | `}` |
|         - |  425 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - |  426 |  |
