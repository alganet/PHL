# src/sx/sxstr.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 82/98 lines (83.67%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits | Line | Source |
| ------: | ---: | :--- |
|       - |    1 | `/**` |
|       - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|       - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |    5 | ` */` |
|       - |    6 | `#include "sxtypes.h"` |
|       - |    7 | `#include "sxmacros.h"` |
|       - |    8 | `#include "sxstr.h"` |
|       - |    9 |  |
| 2711104 |   10 | `PH7_PRIVATE sxu32 SyStrlen(const char *zSrc)` |
|       2 |   11 |  |
| 2711106 |   12 | `	register const char *zIn = zSrc;` |
|       - |   13 | `#if defined(UNTRUST)` |
|       - |   14 | `	if( zIn == 0 ){` |
|       - |   15 | `		return 0;` |
|       - |   16 | `	}` |
|       - |   17 | `#endif` |
| 4506444 |   18 | `	for(;;){` |
| 9010067 |   19 | `		if( !zIn[0] ){ break; } zIn++;` |
| 8444782 |   20 | `		if( !zIn[0] ){ break; } zIn++;` |
| 7676981 |   21 | `		if( !zIn[0] ){ break; } zIn++;` |
| 6813724 |   22 | `		if( !zIn[0] ){ break; } zIn++;` |
|       2 |   23 | `	}` |
| 2711106 |   24 | `	return (sxu32)(zIn - zSrc);` |
|       2 |   25 |  |
|     174 |   26 | `PH7_PRIVATE sxi32 SyByteFind(const char *zStr,sxu32 nLen,sxi32 c,sxu32 *pPos)` |
|       1 |   27 |  |
|     175 |   28 | `	const char *zIn = zStr;` |
|       - |   29 | `	const char *zEnd;` |
|       - |   30 |  |
|     175 |   31 | `	zEnd = &zIn[nLen];` |
|     232 |   32 | `	for(;;){` |
|     465 |   33 | `		if( zIn >= zEnd ){ break; }if( zIn[0] == c ){ if( pPos ){ *pPos = (sxu32)(zIn - zStr); } return SXRET_OK; } zIn++;` |
|     423 |   34 | `		if( zIn >= zEnd ){ break; }if( zIn[0] == c ){ if( pPos ){ *pPos = (sxu32)(zIn - zStr); } return SXRET_OK; } zIn++;` |
|     353 |   35 | `		if( zIn >= zEnd ){ break; }if( zIn[0] == c ){ if( pPos ){ *pPos = (sxu32)(zIn - zStr); } return SXRET_OK; } zIn++;` |
|     329 |   36 | `		if( zIn >= zEnd ){ break; }if( zIn[0] == c ){ if( pPos ){ *pPos = (sxu32)(zIn - zStr); } return SXRET_OK; } zIn++;` |
|       1 |   37 | `	}` |
|      97 |   38 | `	return SXERR_NOTFOUND;` |
|      88 |   39 |  |
|       - |   40 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      20 |   41 | `PH7_PRIVATE sxi32 SyByteFind2(const char *zStr,sxu32 nLen,sxi32 c,sxu32 *pPos)` |
|       1 |   42 |  |
|      21 |   43 | `	const char *zIn = zStr;` |
|       - |   44 | `	const char *zEnd;` |
|       - |   45 |  |
|      21 |   46 | `	zEnd = &zIn[nLen - 1];` |
|      18 |   47 | `	for( ;; ){` |
|      37 |   48 | `		if( zEnd < zIn ){ break; } if( zEnd[0] == c ){ if( pPos ){ *pPos =  (sxu32)(zEnd - zIn);} return SXRET_OK; } zEnd--;` |
|      31 |   49 | `		if( zEnd < zIn ){ break; } if( zEnd[0] == c ){ if( pPos ){ *pPos =  (sxu32)(zEnd - zIn);} return SXRET_OK; } zEnd--;` |
|      29 |   50 | `		if( zEnd < zIn ){ break; } if( zEnd[0] == c ){ if( pPos ){ *pPos =  (sxu32)(zEnd - zIn);} return SXRET_OK; } zEnd--;` |
|      27 |   51 | `		if( zEnd < zIn ){ break; } if( zEnd[0] == c ){ if( pPos ){ *pPos =  (sxu32)(zEnd - zIn);} return SXRET_OK; } zEnd--;` |
|       1 |   52 | `	}` |
|       7 |   53 | `	return SXERR_NOTFOUND;` |
|      11 |   54 |  |
|       - |   55 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     ! 0 |   56 | `PH7_PRIVATE sxi32 SyByteListFind(const char *zSrc,sxu32 nLen,const char *zList,sxu32 *pFirstPos)` |
|     ! 0 |   57 |  |
|     ! 0 |   58 | `	const char *zIn = zSrc;` |
|       - |   59 | `	const char *zPtr;` |
|       - |   60 | `	const char *zEnd;` |
|       - |   61 | `	sxi32 c;` |
|     ! 0 |   62 | `	zEnd = &zSrc[nLen];` |
|     ! 0 |   63 | `	for(;;){` |
|     ! 0 |   64 | `		if( zIn >= zEnd ){ break; }	for(zPtr = zList ; (c = zPtr[0]) != 0 ; zPtr++ ){ if( zIn[0] == c ){ if( pFirstPos ){ *pFirstPos = (sxu32)(zIn - zSrc); } return SXRET_OK; } } zIn++;` |
|     ! 0 |   65 | `		if( zIn >= zEnd ){ break; }	for(zPtr = zList ; (c = zPtr[0]) != 0 ; zPtr++ ){ if( zIn[0] == c ){ if( pFirstPos ){ *pFirstPos = (sxu32)(zIn - zSrc); } return SXRET_OK; } } zIn++;` |
|     ! 0 |   66 | `		if( zIn >= zEnd ){ break; }	for(zPtr = zList ; (c = zPtr[0]) != 0 ; zPtr++ ){ if( zIn[0] == c ){ if( pFirstPos ){ *pFirstPos = (sxu32)(zIn - zSrc); } return SXRET_OK; } } zIn++;` |
|     ! 0 |   67 | `		if( zIn >= zEnd ){ break; }	for(zPtr = zList ; (c = zPtr[0]) != 0 ; zPtr++ ){ if( zIn[0] == c ){ if( pFirstPos ){ *pFirstPos = (sxu32)(zIn - zSrc); } return SXRET_OK; } } zIn++;` |
|     ! 0 |   68 | `	}` |
|     ! 0 |   69 | `	return SXERR_NOTFOUND;` |
|     ! 0 |   70 |  |
|       - |   71 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|   12544 |   72 | `PH7_PRIVATE sxi32 SyStrncmp(const char *zLeft,const char *zRight,sxu32 nLen)` |
|       2 |   73 |  |
|   12546 |   74 | `	const unsigned char *zP = (const unsigned char *)zLeft;` |
|   12546 |   75 | `	const unsigned char *zQ = (const unsigned char *)zRight;` |
|       - |   76 |  |
|   12546 |   77 | `	if( SX_EMPTY_STR(zP) \|\| SX_EMPTY_STR(zQ)  ){` |
|       9 |   78 | `			return SX_EMPTY_STR(zP) ? (SX_EMPTY_STR(zQ) ? 0 : -1) :1;` |
|       - |   79 | `	}` |
|   12538 |   80 | `	if( nLen <= 0 ){` |
|     ! 0 |   81 | `		return 0;` |
|       - |   82 | `	}` |
|   12516 |   83 | `	for(;;){` |
|   25034 |   84 | `		if( nLen <= 0 ){ return 0; } if( zP[0] == 0 \|\| zQ[0] == 0 \|\| zP[0] != zQ[0] ){ break; } zP++; zQ++; nLen--;` |
|   12528 |   85 | `		if( nLen <= 0 ){ return 0; } if( zP[0] == 0 \|\| zQ[0] == 0 \|\| zP[0] != zQ[0] ){ break; } zP++; zQ++; nLen--;` |
|   12518 |   86 | `		if( nLen <= 0 ){ return 0; } if( zP[0] == 0 \|\| zQ[0] == 0 \|\| zP[0] != zQ[0] ){ break; } zP++; zQ++; nLen--;` |
|   12508 |   87 | `		if( nLen <= 0 ){ return 0; } if( zP[0] == 0 \|\| zQ[0] == 0 \|\| zP[0] != zQ[0] ){ break; } zP++; zQ++; nLen--;` |
|       2 |   88 | `	}` |
|   12508 |   89 | `	return (sxi32)(zP[0] - zQ[0]);` |
|    6274 |   90 |  |
|       - |   91 | `#endif` |
|  159604 |   92 | `PH7_PRIVATE sxi32 SyStrnicmp(const char *zLeft, const char *zRight,sxu32 SLen)` |
|       2 |   93 |  |
|  159606 |   94 | `  	register unsigned char *p = (unsigned char *)zLeft;` |
|  159606 |   95 | `	register unsigned char *q = (unsigned char *)zRight;` |
|       - |   96 |  |
|  159606 |   97 | `	if( SX_EMPTY_STR(p) \|\| SX_EMPTY_STR(q) ){` |
|     ! 0 |   98 | `		return SX_EMPTY_STR(p)? SX_EMPTY_STR(q) ? 0 : -1 :1;` |
|       - |   99 | `	}` |
|  104336 |  100 | `	for(;;){` |
|  208674 |  101 | `		if( !SLen ){ return 0; }if( !*p \|\| !*q \|\| SyCharToLower(*p) != SyCharToLower(*q) ){ break; }p++;q++;--SLen;` |
|   89218 |  102 | `		if( !SLen ){ return 0; }if( !*p \|\| !*q \|\| SyCharToLower(*p) != SyCharToLower(*q) ){ break; }p++;q++;--SLen;` |
|   60254 |  103 | `		if( !SLen ){ return 0; }if( !*p \|\| !*q \|\| SyCharToLower(*p) != SyCharToLower(*q) ){ break; }p++;q++;--SLen;` |
|   59930 |  104 | `		if( !SLen ){ return 0; }if( !*p \|\| !*q \|\| SyCharToLower(*p) != SyCharToLower(*q) ){ break; }p++;q++;--SLen;` |
|       - |  105 |  |
|       2 |  106 | `	}` |
|  104596 |  107 | `	return (sxi32)(SyCharToLower(p[0]) - SyCharToLower(q[0]));` |
|   79804 |  108 |  |
|    7426 |  109 | `PH7_PRIVATE sxi32 SyStrnmicmp(const void *pLeft, const void *pRight,sxu32 SLen)` |
|       2 |  110 |  |
|    7428 |  111 | `	return SyStrnicmp((const char *)pLeft,(const char *)pRight,SLen);` |
|       2 |  112 |  |
|  825556 |  113 | `PH7_PRIVATE sxu32 Systrcpy(char *zDest,sxu32 nDestLen,const char *zSrc,sxu32 nLen)` |
|       2 |  114 |  |
|  825558 |  115 | `	unsigned char *zBuf = (unsigned char *)zDest;` |
|  825558 |  116 | `	unsigned char *zIn = (unsigned char *)zSrc;` |
|       - |  117 | `	unsigned char *zEnd;` |
|       - |  118 | `#if defined(UNTRUST)` |
|       - |  119 | `	if( zSrc == (const char *)zDest ){` |
|       - |  120 | `			return 0;` |
|       - |  121 | `	}` |
|       - |  122 | `#endif` |
|  825558 |  123 | `	if( nLen <= 0 ){` |
|     ! 0 |  124 | `		nLen = SyStrlen(zSrc);` |
|     ! 0 |  125 | `	}` |
|  825558 |  126 | `	zEnd = &zBuf[nDestLen - 1]; /* reserve a room for the null terminator */` |
| 1436345 |  127 | `	for(;;){` |
| 2872632 |  128 | `		if( zBuf >= zEnd \|\| nLen == 0 ){ break;} zBuf[0] = zIn[0]; zIn++; zBuf++; nLen--;` |
| 2640934 |  129 | `		if( zBuf >= zEnd \|\| nLen == 0 ){ break;} zBuf[0] = zIn[0]; zIn++; zBuf++; nLen--;` |
| 2442510 |  130 | `		if( zBuf >= zEnd \|\| nLen == 0 ){ break;} zBuf[0] = zIn[0]; zIn++; zBuf++; nLen--;` |
| 2253524 |  131 | `		if( zBuf >= zEnd \|\| nLen == 0 ){ break;} zBuf[0] = zIn[0]; zIn++; zBuf++; nLen--;` |
|       2 |  132 | `	}` |
|  825558 |  133 | `	zBuf[0] = 0;` |
|  825558 |  134 | `	return (sxu32)(zBuf-(unsigned char *)zDest);` |
|       2 |  135 |  |
|       - |  136 |  |
