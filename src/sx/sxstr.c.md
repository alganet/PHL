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
|  6196133 |   10 | `PH7_PRIVATE sxu32 SyStrlen(const char *zSrc)` |
|        5 |   11 |  |
|  6196138 |   12 | `	register const char *zIn = zSrc;` |
|        - |   13 | `#if defined(UNTRUST)` |
|        - |   14 | `	if( zIn == 0 ){` |
|        - |   15 | `		return 0;` |
|        - |   16 | `	}` |
|        - |   17 | `#endif` |
| 10792856 |   18 | `	for(;;){` |
| 21581467 |   19 | `		if( !zIn[0] ){ break; } zIn++;` |
| 20192096 |   20 | `		if( !zIn[0] ){ break; } zIn++;` |
| 18602583 |   21 | `		if( !zIn[0] ){ break; } zIn++;` |
| 16602560 |   22 | `		if( !zIn[0] ){ break; } zIn++;` |
|        5 |   23 | `	}` |
|  6196138 |   24 | `	return (sxu32)(zIn - zSrc);` |
|        5 |   25 |  |
|      414 |   26 | `PH7_PRIVATE sxi32 SyByteFind(const char *zStr,sxu32 nLen,sxi32 c,sxu32 *pPos)` |
|        1 |   27 |  |
|      415 |   28 | `	const char *zIn = zStr;` |
|        - |   29 | `	const char *zEnd;` |
|        - |   30 |  |
|      415 |   31 | `	zEnd = &zIn[nLen];` |
|      489 |   32 | `	for(;;){` |
|      979 |   33 | `		if( zIn >= zEnd ){ break; }if( zIn[0] == c ){ if( pPos ){ *pPos = (sxu32)(zIn - zStr); } return SXRET_OK; } zIn++;` |
|      865 |   34 | `		if( zIn >= zEnd ){ break; }if( zIn[0] == c ){ if( pPos ){ *pPos = (sxu32)(zIn - zStr); } return SXRET_OK; } zIn++;` |
|      761 |   35 | `		if( zIn >= zEnd ){ break; }if( zIn[0] == c ){ if( pPos ){ *pPos = (sxu32)(zIn - zStr); } return SXRET_OK; } zIn++;` |
|      641 |   36 | `		if( zIn >= zEnd ){ break; }if( zIn[0] == c ){ if( pPos ){ *pPos = (sxu32)(zIn - zStr); } return SXRET_OK; } zIn++;` |
|        1 |   37 | `	}` |
|      191 |   38 | `	return SXERR_NOTFOUND;` |
|      208 |   39 |  |
|        - |   40 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|       20 |   41 | `PH7_PRIVATE sxi32 SyByteFind2(const char *zStr,sxu32 nLen,sxi32 c,sxu32 *pPos)` |
|        1 |   42 |  |
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
|       11 |   54 |  |
|        - |   55 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|       96 |   56 | `PH7_PRIVATE sxi32 SyByteListFind(const char *zSrc,sxu32 nLen,const char *zList,sxu32 *pFirstPos)` |
|      ! 0 |   57 |  |
|       96 |   58 | `	const char *zIn = zSrc;` |
|        - |   59 | `	const char *zPtr;` |
|        - |   60 | `	const char *zEnd;` |
|        - |   61 | `	sxi32 c;` |
|       96 |   62 | `	zEnd = &zSrc[nLen];` |
|      253 |   63 | `	for(;;){` |
|     1514 |   64 | `		if( zIn >= zEnd ){ break; }	for(zPtr = zList ; (c = zPtr[0]) != 0 ; zPtr++ ){ if( zIn[0] == c ){ if( pFirstPos ){ *pFirstPos = (sxu32)(zIn - zSrc); } return SXRET_OK; } } zIn++;` |
|     1456 |   65 | `		if( zIn >= zEnd ){ break; }	for(zPtr = zList ; (c = zPtr[0]) != 0 ; zPtr++ ){ if( zIn[0] == c ){ if( pFirstPos ){ *pFirstPos = (sxu32)(zIn - zSrc); } return SXRET_OK; } } zIn++;` |
|     1376 |   66 | `		if( zIn >= zEnd ){ break; }	for(zPtr = zList ; (c = zPtr[0]) != 0 ; zPtr++ ){ if( zIn[0] == c ){ if( pFirstPos ){ *pFirstPos = (sxu32)(zIn - zSrc); } return SXRET_OK; } } zIn++;` |
|     1270 |   67 | `		if( zIn >= zEnd ){ break; }	for(zPtr = zList ; (c = zPtr[0]) != 0 ; zPtr++ ){ if( zIn[0] == c ){ if( pFirstPos ){ *pFirstPos = (sxu32)(zIn - zSrc); } return SXRET_OK; } } zIn++;` |
|      ! 0 |   68 | `	}` |
|       24 |   69 | `	return SXERR_NOTFOUND;` |
|       48 |   70 |  |
|        - |   71 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|    23758 |   72 | `PH7_PRIVATE sxi32 SyStrncmp(const char *zLeft,const char *zRight,sxu32 nLen)` |
|        5 |   73 |  |
|    23763 |   74 | `	const unsigned char *zP = (const unsigned char *)zLeft;` |
|    23763 |   75 | `	const unsigned char *zQ = (const unsigned char *)zRight;` |
|        - |   76 |  |
|    23763 |   77 | `	if( SX_EMPTY_STR(zP) \|\| SX_EMPTY_STR(zQ)  ){` |
|        9 |   78 | `			return SX_EMPTY_STR(zP) ? (SX_EMPTY_STR(zQ) ? 0 : -1) :1;` |
|        - |   79 | `	}` |
|    23755 |   80 | `	if( nLen <= 0 ){` |
|      ! 0 |   81 | `		return 0;` |
|        - |   82 | `	}` |
|    23715 |   83 | `	for(;;){` |
|    47435 |   84 | `		if( nLen <= 0 ){ return 0; } if( zP[0] == 0 \|\| zQ[0] == 0 \|\| zP[0] != zQ[0] ){ break; } zP++; zQ++; nLen--;` |
|    23725 |   85 | `		if( nLen <= 0 ){ return 0; } if( zP[0] == 0 \|\| zQ[0] == 0 \|\| zP[0] != zQ[0] ){ break; } zP++; zQ++; nLen--;` |
|    23705 |   86 | `		if( nLen <= 0 ){ return 0; } if( zP[0] == 0 \|\| zQ[0] == 0 \|\| zP[0] != zQ[0] ){ break; } zP++; zQ++; nLen--;` |
|    23695 |   87 | `		if( nLen <= 0 ){ return 0; } if( zP[0] == 0 \|\| zQ[0] == 0 \|\| zP[0] != zQ[0] ){ break; } zP++; zQ++; nLen--;` |
|        5 |   88 | `	}` |
|    23715 |   89 | `	return (sxi32)(zP[0] - zQ[0]);` |
|    11884 |   90 |  |
|        - |   91 | `#endif` |
|   824284 |   92 | `PH7_PRIVATE sxi32 SyStrnicmp(const char *zLeft, const char *zRight,sxu32 SLen)` |
|        5 |   93 |  |
|   824289 |   94 | `  	register unsigned char *p = (unsigned char *)zLeft;` |
|   824289 |   95 | `	register unsigned char *q = (unsigned char *)zRight;` |
|        - |   96 |  |
|   824289 |   97 | `	if( SX_EMPTY_STR(p) \|\| SX_EMPTY_STR(q) ){` |
|      ! 0 |   98 | `		return SX_EMPTY_STR(p)? SX_EMPTY_STR(q) ? 0 : -1 :1;` |
|        - |   99 | `	}` |
|   718904 |  100 | `	for(;;){` |
|  1437813 |  101 | `		if( !SLen ){ return 0; }if( !*p \|\| !*q \|\| SyCharToLower(*p) != SyCharToLower(*q) ){ break; }p++;q++;--SLen;` |
|   905199 |  102 | `		if( !SLen ){ return 0; }if( !*p \|\| !*q \|\| SyCharToLower(*p) != SyCharToLower(*q) ){ break; }p++;q++;--SLen;` |
|   774493 |  103 | `		if( !SLen ){ return 0; }if( !*p \|\| !*q \|\| SyCharToLower(*p) != SyCharToLower(*q) ){ break; }p++;q++;--SLen;` |
|   667715 |  104 | `		if( !SLen ){ return 0; }if( !*p \|\| !*q \|\| SyCharToLower(*p) != SyCharToLower(*q) ){ break; }p++;q++;--SLen;` |
|        - |  105 |  |
|        5 |  106 | `	}` |
|   447089 |  107 | `	return (sxi32)(SyCharToLower(p[0]) - SyCharToLower(q[0]));` |
|   412147 |  108 |  |
|   211766 |  109 | `PH7_PRIVATE sxi32 SyStrnmicmp(const void *pLeft, const void *pRight,sxu32 SLen)` |
|        5 |  110 |  |
|   211771 |  111 | `	return SyStrnicmp((const char *)pLeft,(const char *)pRight,SLen);` |
|        5 |  112 |  |
|  3056710 |  113 | `PH7_PRIVATE sxu32 Systrcpy(char *zDest,sxu32 nDestLen,const char *zSrc,sxu32 nLen)` |
|        5 |  114 |  |
|  3056715 |  115 | `	unsigned char *zBuf = (unsigned char *)zDest;` |
|  3056715 |  116 | `	unsigned char *zIn = (unsigned char *)zSrc;` |
|        - |  117 | `	unsigned char *zEnd;` |
|        - |  118 | `#if defined(UNTRUST)` |
|        - |  119 | `	if( zSrc == (const char *)zDest ){` |
|        - |  120 | `			return 0;` |
|        - |  121 | `	}` |
|        - |  122 | `#endif` |
|  3056715 |  123 | `	if( nLen <= 0 ){` |
|      ! 0 |  124 | `		nLen = SyStrlen(zSrc);` |
|      ! 0 |  125 | `	}` |
|  3056715 |  126 | `	zEnd = &zBuf[nDestLen - 1]; /* reserve a room for the null terminator */` |
|  5381076 |  127 | `	for(;;){` |
| 10761919 |  128 | `		if( zBuf >= zEnd \|\| nLen == 0 ){ break;} zBuf[0] = zIn[0]; zIn++; zBuf++; nLen--;` |
|  9968526 |  129 | `		if( zBuf >= zEnd \|\| nLen == 0 ){ break;} zBuf[0] = zIn[0]; zIn++; zBuf++; nLen--;` |
|  9198916 |  130 | `		if( zBuf >= zEnd \|\| nLen == 0 ){ break;} zBuf[0] = zIn[0]; zIn++; zBuf++; nLen--;` |
|  8446562 |  131 | `		if( zBuf >= zEnd \|\| nLen == 0 ){ break;} zBuf[0] = zIn[0]; zIn++; zBuf++; nLen--;` |
|        5 |  132 | `	}` |
|  3056715 |  133 | `	zBuf[0] = 0;` |
|  3056715 |  134 | `	return (sxu32)(zBuf-(unsigned char *)zDest);` |
|        5 |  135 |  |
|        - |  136 |  |
