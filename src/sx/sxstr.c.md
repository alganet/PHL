# src/sx/sxstr.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 96/100 lines (96.00%)

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
|         - |    8 | `#include "sxstr.h"` |
|         - |    9 |  |
| 131972463 |   10 | `PH7_PRIVATE sxu32 SyStrlen(const char *zSrc)` |
|         5 |   11 | `{` |
| 131972468 |   12 | `	register const char *zIn = zSrc;` |
|         - |   13 | `#if defined(UNTRUST)` |
|         - |   14 | `	if( zIn == 0 ){` |
|         - |   15 | `		return 0;` |
|         - |   16 | `	}` |
|         - |   17 | `#endif` |
| 237975328 |   18 | `	for(;;){` |
| 475726398 |   19 | `		if( !zIn[0] ){ break; } zIn++;` |
| 443313225 |   20 | `		if( !zIn[0] ){ break; } zIn++;` |
| 408173114 |   21 | `		if( !zIn[0] ){ break; } zIn++;` |
| 377431969 |   22 | `		if( !zIn[0] ){ break; } zIn++;` |
|         5 |   23 | `	}` |
| 131972468 |   24 | `	return (sxu32)(zIn - zSrc);` |
|         5 |   25 | `}` |
|      2253 |   26 | `PH7_PRIVATE sxi32 SyByteFind(const char *zStr,sxu32 nLen,sxi32 c,sxu32 *pPos)` |
|         5 |   27 | `{` |
|      2258 |   28 | `	const char *zIn = zStr;` |
|         - |   29 | `	const char *zEnd;` |
|         - |   30 |  |
|      2258 |   31 | `	zEnd = &zIn[nLen];` |
|      3278 |   32 | `	for(;;){` |
|      6504 |   33 | `		if( zIn >= zEnd ){ break; }if( zIn[0] == c ){ if( pPos ){ *pPos = (sxu32)(zIn - zStr); } return SXRET_OK; } zIn++;` |
|      6262 |   34 | `		if( zIn >= zEnd ){ break; }if( zIn[0] == c ){ if( pPos ){ *pPos = (sxu32)(zIn - zStr); } return SXRET_OK; } zIn++;` |
|      4859 |   35 | `		if( zIn >= zEnd ){ break; }if( zIn[0] == c ){ if( pPos ){ *pPos = (sxu32)(zIn - zStr); } return SXRET_OK; } zIn++;` |
|      4591 |   36 | `		if( zIn >= zEnd ){ break; }if( zIn[0] == c ){ if( pPos ){ *pPos = (sxu32)(zIn - zStr); } return SXRET_OK; } zIn++;` |
|         5 |   37 | `	}` |
|      2012 |   38 | `	return SXERR_NOTFOUND;` |
|      1141 |   39 | `}` |
|         - |   40 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        56 |   41 | `PH7_PRIVATE sxi32 SyByteFind2(const char *zStr,sxu32 nLen,sxi32 c,sxu32 *pPos)` |
|         1 |   42 | `{` |
|        57 |   43 | `	const char *zIn = zStr;` |
|         - |   44 | `	const char *zEnd;` |
|         - |   45 |  |
|        57 |   46 | `	zEnd = &zIn[nLen - 1];` |
|        38 |   47 | `	for( ;; ){` |
|        77 |   48 | `		if( zEnd < zIn ){ break; } if( zEnd[0] == c ){ if( pPos ){ *pPos =  (sxu32)(zEnd - zIn);} return SXRET_OK; } zEnd--;` |
|        65 |   49 | `		if( zEnd < zIn ){ break; } if( zEnd[0] == c ){ if( pPos ){ *pPos =  (sxu32)(zEnd - zIn);} return SXRET_OK; } zEnd--;` |
|        43 |   50 | `		if( zEnd < zIn ){ break; } if( zEnd[0] == c ){ if( pPos ){ *pPos =  (sxu32)(zEnd - zIn);} return SXRET_OK; } zEnd--;` |
|        37 |   51 | `		if( zEnd < zIn ){ break; } if( zEnd[0] == c ){ if( pPos ){ *pPos =  (sxu32)(zEnd - zIn);} return SXRET_OK; } zEnd--;` |
|         1 |   52 | `	}` |
|        11 |   53 | `	return SXERR_NOTFOUND;` |
|        29 |   54 | `}` |
|         - |   55 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|       178 |   56 | `PH7_PRIVATE sxi32 SyByteListFind(const char *zSrc,sxu32 nLen,const char *zList,sxu32 *pFirstPos)` |
|       ! 0 |   57 | `{` |
|       178 |   58 | `	const char *zIn = zSrc;` |
|         - |   59 | `	const char *zPtr;` |
|         - |   60 | `	const char *zEnd;` |
|         - |   61 | `	sxi32 c;` |
|       178 |   62 | `	zEnd = &zSrc[nLen];` |
|       481 |   63 | `	for(;;){` |
|      2874 |   64 | `		if( zIn >= zEnd ){ break; }	for(zPtr = zList ; (c = zPtr[0]) != 0 ; zPtr++ ){ if( zIn[0] == c ){ if( pFirstPos ){ *pFirstPos = (sxu32)(zIn - zSrc); } return SXRET_OK; } } zIn++;` |
|      2764 |   65 | `		if( zIn >= zEnd ){ break; }	for(zPtr = zList ; (c = zPtr[0]) != 0 ; zPtr++ ){ if( zIn[0] == c ){ if( pFirstPos ){ *pFirstPos = (sxu32)(zIn - zSrc); } return SXRET_OK; } } zIn++;` |
|      2616 |   66 | `		if( zIn >= zEnd ){ break; }	for(zPtr = zList ; (c = zPtr[0]) != 0 ; zPtr++ ){ if( zIn[0] == c ){ if( pFirstPos ){ *pFirstPos = (sxu32)(zIn - zSrc); } return SXRET_OK; } } zIn++;` |
|      2424 |   67 | `		if( zIn >= zEnd ){ break; }	for(zPtr = zList ; (c = zPtr[0]) != 0 ; zPtr++ ){ if( zIn[0] == c ){ if( pFirstPos ){ *pFirstPos = (sxu32)(zIn - zSrc); } return SXRET_OK; } } zIn++;` |
|       ! 0 |   68 | `	}` |
|        46 |   69 | `	return SXERR_NOTFOUND;` |
|        89 |   70 | `}` |
|         - |   71 | `/* used by hashmap.c's key sorting — must stay in the tiny build */` |
|  45221660 |   72 | `PH7_PRIVATE sxi32 SyStrncmp(const char *zLeft,const char *zRight,sxu32 nLen)` |
|         5 |   73 | `{` |
|  45221665 |   74 | `	const unsigned char *zP = (const unsigned char *)zLeft;` |
|  45221665 |   75 | `	const unsigned char *zQ = (const unsigned char *)zRight;` |
|         - |   76 |  |
|         - |   77 | `	/* Comparing ZERO bytes is always equal, whatever the operands -- this test has` |
|         - |   78 | `	 * to come before the empty-string shortcut below, which used to run first and` |
|         - |   79 | `	 * so answered -1/1 for a zero-length compare against an empty string. That is` |
|         - |   80 | `	 * what php's strncmp("", "a", 0) exposed: it must be 0. */` |
|  45221665 |   81 | `	if( nLen <= 0 ){` |
|        17 |   82 | `		return 0;` |
|         - |   83 | `	}` |
|  45221649 |   84 | `	if( SX_EMPTY_STR(zP) \|\| SX_EMPTY_STR(zQ)  ){` |
|       269 |   85 | `			return SX_EMPTY_STR(zP) ? (SX_EMPTY_STR(zQ) ? 0 : -1) :1;` |
|         - |   86 | `	}` |
|  22746606 |   87 | `	for(;;){` |
|  45465090 |   88 | `		if( nLen <= 0 ){ return 0; } if( zP[0] == 0 \|\| zQ[0] == 0 \|\| zP[0] != zQ[0] ){ break; } zP++; zQ++; nLen--;` |
|   1151943 |   89 | `		if( nLen <= 0 ){ return 0; } if( zP[0] == 0 \|\| zQ[0] == 0 \|\| zP[0] != zQ[0] ){ break; } zP++; zQ++; nLen--;` |
|    617437 |   90 | `		if( nLen <= 0 ){ return 0; } if( zP[0] == 0 \|\| zQ[0] == 0 \|\| zP[0] != zQ[0] ){ break; } zP++; zQ++; nLen--;` |
|    547667 |   91 | `		if( nLen <= 0 ){ return 0; } if( zP[0] == 0 \|\| zQ[0] == 0 \|\| zP[0] != zQ[0] ){ break; } zP++; zQ++; nLen--;` |
|         5 |   92 | `	}` |
|  45169307 |   93 | `	return (sxi32)(zP[0] - zQ[0]);` |
|  22624860 |   94 | `}` |
|  42266921 |   95 | `PH7_PRIVATE sxi32 SyStrnicmp(const char *zLeft, const char *zRight,sxu32 SLen)` |
|         5 |   96 | `{` |
|  42266926 |   97 | `  	register unsigned char *p = (unsigned char *)zLeft;` |
|  42266926 |   98 | `	register unsigned char *q = (unsigned char *)zRight;` |
|         - |   99 |  |
|         - |  100 | `	/* Zero bytes compared is always equal -- see the note in SyStrncmp. */` |
|  42266926 |  101 | `	if( !SLen ){` |
|       ! 0 |  102 | `		return 0;` |
|         - |  103 | `	}` |
|  42266926 |  104 | `	if( SX_EMPTY_STR(p) \|\| SX_EMPTY_STR(q) ){` |
|       ! 0 |  105 | `		return SX_EMPTY_STR(p)? SX_EMPTY_STR(q) ? 0 : -1 :1;` |
|         - |  106 | `	}` |
| 107340248 |  107 | `	for(;;){` |
| 214662670 |  108 | `		if( !SLen ){ return 0; }if( !*p \|\| !*q \|\| SyCharToLower(*p) != SyCharToLower(*q) ){ break; }p++;q++;--SLen;` |
| 201971414 |  109 | `		if( !SLen ){ return 0; }if( !*p \|\| !*q \|\| SyCharToLower(*p) != SyCharToLower(*q) ){ break; }p++;q++;--SLen;` |
| 188084170 |  110 | `		if( !SLen ){ return 0; }if( !*p \|\| !*q \|\| SyCharToLower(*p) != SyCharToLower(*q) ){ break; }p++;q++;--SLen;` |
| 182220895 |  111 | `		if( !SLen ){ return 0; }if( !*p \|\| !*q \|\| SyCharToLower(*p) != SyCharToLower(*q) ){ break; }p++;q++;--SLen;` |
|         - |  112 |  |
|         5 |  113 | `	}` |
|   5860707 |  114 | `	return (sxi32)(SyCharToLower(p[0]) - SyCharToLower(q[0]));` |
|  21138154 |  115 | `}` |
|  35840387 |  116 | `PH7_PRIVATE sxi32 SyStrnmicmp(const void *pLeft, const void *pRight,sxu32 SLen)` |
|         5 |  117 | `{` |
|  35840392 |  118 | `	return SyStrnicmp((const char *)pLeft,(const char *)pRight,SLen);` |
|         5 |  119 | `}` |
|  12997690 |  120 | `PH7_PRIVATE sxu32 Systrcpy(char *zDest,sxu32 nDestLen,const char *zSrc,sxu32 nLen)` |
|         5 |  121 | `{` |
|  12997695 |  122 | `	unsigned char *zBuf = (unsigned char *)zDest;` |
|  12997695 |  123 | `	unsigned char *zIn = (unsigned char *)zSrc;` |
|         - |  124 | `	unsigned char *zEnd;` |
|         - |  125 | `#if defined(UNTRUST)` |
|         - |  126 | `	if( zSrc == (const char *)zDest ){` |
|         - |  127 | `			return 0;` |
|         - |  128 | `	}` |
|         - |  129 | `#endif` |
|  12997695 |  130 | `	if( nLen <= 0 ){` |
|        11 |  131 | `		nLen = SyStrlen(zSrc);` |
|         5 |  132 | `	}` |
|  12997695 |  133 | `	zEnd = &zBuf[nDestLen - 1]; /* reserve a room for the null terminator */` |
|  30615682 |  134 | `	for(;;){` |
|  61226347 |  135 | `		if( zBuf >= zEnd \|\| nLen == 0 ){ break;} zBuf[0] = zIn[0]; zIn++; zBuf++; nLen--;` |
|  57888899 |  136 | `		if( zBuf >= zEnd \|\| nLen == 0 ){ break;} zBuf[0] = zIn[0]; zIn++; zBuf++; nLen--;` |
|  54513814 |  137 | `		if( zBuf >= zEnd \|\| nLen == 0 ){ break;} zBuf[0] = zIn[0]; zIn++; zBuf++; nLen--;` |
|  51469465 |  138 | `		if( zBuf >= zEnd \|\| nLen == 0 ){ break;} zBuf[0] = zIn[0]; zIn++; zBuf++; nLen--;` |
|         5 |  139 | `	}` |
|  12997695 |  140 | `	zBuf[0] = 0;` |
|  12997695 |  141 | `	return (sxu32)(zBuf-(unsigned char *)zDest);` |
|         5 |  142 | `}` |
|         - |  143 |  |
