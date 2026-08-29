# src/sx/sxstr.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 92/98 lines (93.88%)

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
|        - |    8 | `#include "sxstr.h"` |
|        - |    9 |  |
| 11065167 |   10 | `PH7_PRIVATE sxu32 SyStrlen(const char *zSrc)` |
|        5 |   11 | `{` |
| 11065172 |   12 | `	register const char *zIn = zSrc;` |
|        - |   13 | `#if defined(UNTRUST)` |
|        - |   14 | `	if( zIn == 0 ){` |
|        - |   15 | `		return 0;` |
|        - |   16 | `	}` |
|        - |   17 | `#endif` |
| 18257898 |   18 | `	for(;;){` |
| 36511332 |   19 | `		if( !zIn[0] ){ break; } zIn++;` |
| 33957253 |   20 | `		if( !zIn[0] ){ break; } zIn++;` |
| 30753962 |   21 | `		if( !zIn[0] ){ break; } zIn++;` |
| 27730998 |   22 | `		if( !zIn[0] ){ break; } zIn++;` |
|        5 |   23 | `	}` |
| 11065172 |   24 | `	return (sxu32)(zIn - zSrc);` |
|        5 |   25 | `}` |
|      460 |   26 | `PH7_PRIVATE sxi32 SyByteFind(const char *zStr,sxu32 nLen,sxi32 c,sxu32 *pPos)` |
|        1 |   27 | `{` |
|      461 |   28 | `	const char *zIn = zStr;` |
|        - |   29 | `	const char *zEnd;` |
|        - |   30 |  |
|      461 |   31 | `	zEnd = &zIn[nLen];` |
|      537 |   32 | `	for(;;){` |
|     1075 |   33 | `		if( zIn >= zEnd ){ break; }if( zIn[0] == c ){ if( pPos ){ *pPos = (sxu32)(zIn - zStr); } return SXRET_OK; } zIn++;` |
|      949 |   34 | `		if( zIn >= zEnd ){ break; }if( zIn[0] == c ){ if( pPos ){ *pPos = (sxu32)(zIn - zStr); } return SXRET_OK; } zIn++;` |
|      837 |   35 | `		if( zIn >= zEnd ){ break; }if( zIn[0] == c ){ if( pPos ){ *pPos = (sxu32)(zIn - zStr); } return SXRET_OK; } zIn++;` |
|      703 |   36 | `		if( zIn >= zEnd ){ break; }if( zIn[0] == c ){ if( pPos ){ *pPos = (sxu32)(zIn - zStr); } return SXRET_OK; } zIn++;` |
|        1 |   37 | `	}` |
|      215 |   38 | `	return SXERR_NOTFOUND;` |
|      231 |   39 | `}` |
|        - |   40 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|       20 |   41 | `PH7_PRIVATE sxi32 SyByteFind2(const char *zStr,sxu32 nLen,sxi32 c,sxu32 *pPos)` |
|        1 |   42 | `{` |
|       21 |   43 | `	const char *zIn = zStr;` |
|        - |   44 | `	const char *zEnd;` |
|        - |   45 |  |
|       21 |   46 | `	zEnd = &zIn[nLen - 1];` |
|       18 |   47 | `	for( ;; ){` |
|       37 |   48 | `		if( zEnd < zIn ){ break; } if( zEnd[0] == c ){ if( pPos ){ *pPos =  (sxu32)(zEnd - zIn);} return SXRET_OK; } zEnd--;` |
|       31 |   49 | `		if( zEnd < zIn ){ break; } if( zEnd[0] == c ){ if( pPos ){ *pPos =  (sxu32)(zEnd - zIn);} return SXRET_OK; } zEnd--;` |
|       29 |   50 | `		if( zEnd < zIn ){ break; } if( zEnd[0] == c ){ if( pPos ){ *pPos =  (sxu32)(zEnd - zIn);} return SXRET_OK; } zEnd--;` |
|       27 |   51 | `		if( zEnd < zIn ){ break; } if( zEnd[0] == c ){ if( pPos ){ *pPos =  (sxu32)(zEnd - zIn);} return SXRET_OK; } zEnd--;` |
|        1 |   52 | `	}` |
|        7 |   53 | `	return SXERR_NOTFOUND;` |
|       11 |   54 | `}` |
|        - |   55 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      120 |   56 | `PH7_PRIVATE sxi32 SyByteListFind(const char *zSrc,sxu32 nLen,const char *zList,sxu32 *pFirstPos)` |
|      ! 0 |   57 | `{` |
|      120 |   58 | `	const char *zIn = zSrc;` |
|        - |   59 | `	const char *zPtr;` |
|        - |   60 | `	const char *zEnd;` |
|        - |   61 | `	sxi32 c;` |
|      120 |   62 | `	zEnd = &zSrc[nLen];` |
|      314 |   63 | `	for(;;){` |
|     1880 |   64 | `		if( zIn >= zEnd ){ break; }	for(zPtr = zList ; (c = zPtr[0]) != 0 ; zPtr++ ){ if( zIn[0] == c ){ if( pFirstPos ){ *pFirstPos = (sxu32)(zIn - zSrc); } return SXRET_OK; } } zIn++;` |
|     1810 |   65 | `		if( zIn >= zEnd ){ break; }	for(zPtr = zList ; (c = zPtr[0]) != 0 ; zPtr++ ){ if( zIn[0] == c ){ if( pFirstPos ){ *pFirstPos = (sxu32)(zIn - zSrc); } return SXRET_OK; } } zIn++;` |
|     1708 |   66 | `		if( zIn >= zEnd ){ break; }	for(zPtr = zList ; (c = zPtr[0]) != 0 ; zPtr++ ){ if( zIn[0] == c ){ if( pFirstPos ){ *pFirstPos = (sxu32)(zIn - zSrc); } return SXRET_OK; } } zIn++;` |
|     1574 |   67 | `		if( zIn >= zEnd ){ break; }	for(zPtr = zList ; (c = zPtr[0]) != 0 ; zPtr++ ){ if( zIn[0] == c ){ if( pFirstPos ){ *pFirstPos = (sxu32)(zIn - zSrc); } return SXRET_OK; } } zIn++;` |
|      ! 0 |   68 | `	}` |
|       32 |   69 | `	return SXERR_NOTFOUND;` |
|       60 |   70 | `}` |
|        - |   71 | `/* used by hashmap.c's key sorting — must stay in the tiny build */` |
|    30018 |   72 | `PH7_PRIVATE sxi32 SyStrncmp(const char *zLeft,const char *zRight,sxu32 nLen)` |
|        5 |   73 | `{` |
|    30023 |   74 | `	const unsigned char *zP = (const unsigned char *)zLeft;` |
|    30023 |   75 | `	const unsigned char *zQ = (const unsigned char *)zRight;` |
|        - |   76 |  |
|    30023 |   77 | `	if( SX_EMPTY_STR(zP) \|\| SX_EMPTY_STR(zQ)  ){` |
|        9 |   78 | `			return SX_EMPTY_STR(zP) ? (SX_EMPTY_STR(zQ) ? 0 : -1) :1;` |
|        - |   79 | `	}` |
|    30015 |   80 | `	if( nLen <= 0 ){` |
|      ! 0 |   81 | `		return 0;` |
|        - |   82 | `	}` |
|    29962 |   83 | `	for(;;){` |
|    59929 |   84 | `		if( nLen <= 0 ){ return 0; } if( zP[0] == 0 \|\| zQ[0] == 0 \|\| zP[0] != zQ[0] ){ break; } zP++; zQ++; nLen--;` |
|    29961 |   85 | `		if( nLen <= 0 ){ return 0; } if( zP[0] == 0 \|\| zQ[0] == 0 \|\| zP[0] != zQ[0] ){ break; } zP++; zQ++; nLen--;` |
|    29939 |   86 | `		if( nLen <= 0 ){ return 0; } if( zP[0] == 0 \|\| zQ[0] == 0 \|\| zP[0] != zQ[0] ){ break; } zP++; zQ++; nLen--;` |
|    29927 |   87 | `		if( nLen <= 0 ){ return 0; } if( zP[0] == 0 \|\| zQ[0] == 0 \|\| zP[0] != zQ[0] ){ break; } zP++; zQ++; nLen--;` |
|        5 |   88 | `	}` |
|    29965 |   89 | `	return (sxi32)(zP[0] - zQ[0]);` |
|    15014 |   90 | `}` |
| 11126124 |   91 | `PH7_PRIVATE sxi32 SyStrnicmp(const char *zLeft, const char *zRight,sxu32 SLen)` |
|        5 |   92 | `{` |
| 11126129 |   93 | `  	register unsigned char *p = (unsigned char *)zLeft;` |
| 11126129 |   94 | `	register unsigned char *q = (unsigned char *)zRight;` |
|        - |   95 |  |
| 11126129 |   96 | `	if( SX_EMPTY_STR(p) \|\| SX_EMPTY_STR(q) ){` |
|      ! 0 |   97 | `		return SX_EMPTY_STR(p)? SX_EMPTY_STR(q) ? 0 : -1 :1;` |
|        - |   98 | `	}` |
|  7609413 |   99 | `	for(;;){` |
| 15218831 |  100 | `		if( !SLen ){ return 0; }if( !*p \|\| !*q \|\| SyCharToLower(*p) != SyCharToLower(*q) ){ break; }p++;q++;--SLen;` |
|  7206249 |  101 | `		if( !SLen ){ return 0; }if( !*p \|\| !*q \|\| SyCharToLower(*p) != SyCharToLower(*q) ){ break; }p++;q++;--SLen;` |
|  5351157 |  102 | `		if( !SLen ){ return 0; }if( !*p \|\| !*q \|\| SyCharToLower(*p) != SyCharToLower(*q) ){ break; }p++;q++;--SLen;` |
|  4957057 |  103 | `		if( !SLen ){ return 0; }if( !*p \|\| !*q \|\| SyCharToLower(*p) != SyCharToLower(*q) ){ break; }p++;q++;--SLen;` |
|        - |  104 |  |
|        5 |  105 | `	}` |
|  7385659 |  106 | `	return (sxi32)(SyCharToLower(p[0]) - SyCharToLower(q[0]));` |
|  5563067 |  107 | `}` |
|   681050 |  108 | `PH7_PRIVATE sxi32 SyStrnmicmp(const void *pLeft, const void *pRight,sxu32 SLen)` |
|        5 |  109 | `{` |
|   681055 |  110 | `	return SyStrnicmp((const char *)pLeft,(const char *)pRight,SLen);` |
|        5 |  111 | `}` |
|  7554956 |  112 | `PH7_PRIVATE sxu32 Systrcpy(char *zDest,sxu32 nDestLen,const char *zSrc,sxu32 nLen)` |
|        5 |  113 | `{` |
|  7554961 |  114 | `	unsigned char *zBuf = (unsigned char *)zDest;` |
|  7554961 |  115 | `	unsigned char *zIn = (unsigned char *)zSrc;` |
|        - |  116 | `	unsigned char *zEnd;` |
|        - |  117 | `#if defined(UNTRUST)` |
|        - |  118 | `	if( zSrc == (const char *)zDest ){` |
|        - |  119 | `			return 0;` |
|        - |  120 | `	}` |
|        - |  121 | `#endif` |
|  7554961 |  122 | `	if( nLen <= 0 ){` |
|      ! 0 |  123 | `		nLen = SyStrlen(zSrc);` |
|      ! 0 |  124 | `	}` |
|  7554961 |  125 | `	zEnd = &zBuf[nDestLen - 1]; /* reserve a room for the null terminator */` |
| 11744821 |  126 | `	for(;;){` |
| 23489380 |  127 | `		if( zBuf >= zEnd \|\| nLen == 0 ){ break;} zBuf[0] = zIn[0]; zIn++; zBuf++; nLen--;` |
| 21402978 |  128 | `		if( zBuf >= zEnd \|\| nLen == 0 ){ break;} zBuf[0] = zIn[0]; zIn++; zBuf++; nLen--;` |
| 19325786 |  129 | `		if( zBuf >= zEnd \|\| nLen == 0 ){ break;} zBuf[0] = zIn[0]; zIn++; zBuf++; nLen--;` |
| 17551328 |  130 | `		if( zBuf >= zEnd \|\| nLen == 0 ){ break;} zBuf[0] = zIn[0]; zIn++; zBuf++; nLen--;` |
|        5 |  131 | `	}` |
|  7554961 |  132 | `	zBuf[0] = 0;` |
|  7554961 |  133 | `	return (sxu32)(zBuf-(unsigned char *)zDest);` |
|        5 |  134 | `}` |
|        - |  135 |  |
