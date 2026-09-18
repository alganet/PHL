# src/sx/sxstr.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 93/100 lines (93.00%)

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
| 22780364 |   10 | `PH7_PRIVATE sxu32 SyStrlen(const char *zSrc)` |
|        5 |   11 | `{` |
| 22780369 |   12 | `	register const char *zIn = zSrc;` |
|        - |   13 | `#if defined(UNTRUST)` |
|        - |   14 | `	if( zIn == 0 ){` |
|        - |   15 | `		return 0;` |
|        - |   16 | `	}` |
|        - |   17 | `#endif` |
| 36554405 |   18 | `	for(;;){` |
| 73078792 |   19 | `		if( !zIn[0] ){ break; } zIn++;` |
| 67086172 |   20 | `		if( !zIn[0] ){ break; } zIn++;` |
| 60790160 |   21 | `		if( !zIn[0] ){ break; } zIn++;` |
| 55350236 |   22 | `		if( !zIn[0] ){ break; } zIn++;` |
|        5 |   23 | `	}` |
| 22780369 |   24 | `	return (sxu32)(zIn - zSrc);` |
|        5 |   25 | `}` |
|      316 |   26 | `PH7_PRIVATE sxi32 SyByteFind(const char *zStr,sxu32 nLen,sxi32 c,sxu32 *pPos)` |
|        1 |   27 | `{` |
|      317 |   28 | `	const char *zIn = zStr;` |
|        - |   29 | `	const char *zEnd;` |
|        - |   30 |  |
|      317 |   31 | `	zEnd = &zIn[nLen];` |
|      327 |   32 | `	for(;;){` |
|      655 |   33 | `		if( zIn >= zEnd ){ break; }if( zIn[0] == c ){ if( pPos ){ *pPos = (sxu32)(zIn - zStr); } return SXRET_OK; } zIn++;` |
|      567 |   34 | `		if( zIn >= zEnd ){ break; }if( zIn[0] == c ){ if( pPos ){ *pPos = (sxu32)(zIn - zStr); } return SXRET_OK; } zIn++;` |
|      507 |   35 | `		if( zIn >= zEnd ){ break; }if( zIn[0] == c ){ if( pPos ){ *pPos = (sxu32)(zIn - zStr); } return SXRET_OK; } zIn++;` |
|      395 |   36 | `		if( zIn >= zEnd ){ break; }if( zIn[0] == c ){ if( pPos ){ *pPos = (sxu32)(zIn - zStr); } return SXRET_OK; } zIn++;` |
|        1 |   37 | `	}` |
|      143 |   38 | `	return SXERR_NOTFOUND;` |
|      159 |   39 | `}` |
|        - |   40 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|       20 |   41 | `PH7_PRIVATE sxi32 SyByteFind2(const char *zStr,sxu32 nLen,sxi32 c,sxu32 *pPos)` |
|        1 |   42 | `{` |
|       21 |   43 | `	const char *zIn = zStr;` |
|        - |   44 | `	const char *zEnd;` |
|        - |   45 |  |
|       21 |   46 | `	zEnd = &zIn[nLen - 1];` |
|       20 |   47 | `	for( ;; ){` |
|       41 |   48 | `		if( zEnd < zIn ){ break; } if( zEnd[0] == c ){ if( pPos ){ *pPos =  (sxu32)(zEnd - zIn);} return SXRET_OK; } zEnd--;` |
|       37 |   49 | `		if( zEnd < zIn ){ break; } if( zEnd[0] == c ){ if( pPos ){ *pPos =  (sxu32)(zEnd - zIn);} return SXRET_OK; } zEnd--;` |
|       33 |   50 | `		if( zEnd < zIn ){ break; } if( zEnd[0] == c ){ if( pPos ){ *pPos =  (sxu32)(zEnd - zIn);} return SXRET_OK; } zEnd--;` |
|       31 |   51 | `		if( zEnd < zIn ){ break; } if( zEnd[0] == c ){ if( pPos ){ *pPos =  (sxu32)(zEnd - zIn);} return SXRET_OK; } zEnd--;` |
|        1 |   52 | `	}` |
|        9 |   53 | `	return SXERR_NOTFOUND;` |
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
|  1030933 |   72 | `PH7_PRIVATE sxi32 SyStrncmp(const char *zLeft,const char *zRight,sxu32 nLen)` |
|        5 |   73 | `{` |
|  1030938 |   74 | `	const unsigned char *zP = (const unsigned char *)zLeft;` |
|  1030938 |   75 | `	const unsigned char *zQ = (const unsigned char *)zRight;` |
|        - |   76 |  |
|        - |   77 | `	/* Comparing ZERO bytes is always equal, whatever the operands -- this test has` |
|        - |   78 | `	 * to come before the empty-string shortcut below, which used to run first and` |
|        - |   79 | `	 * so answered -1/1 for a zero-length compare against an empty string. That is` |
|        - |   80 | `	 * what php's strncmp("", "a", 0) exposed: it must be 0. */` |
|  1030938 |   81 | `	if( nLen <= 0 ){` |
|       17 |   82 | `		return 0;` |
|        - |   83 | `	}` |
|  1030922 |   84 | `	if( SX_EMPTY_STR(zP) \|\| SX_EMPTY_STR(zQ)  ){` |
|      ! 0 |   85 | `			return SX_EMPTY_STR(zP) ? (SX_EMPTY_STR(zQ) ? 0 : -1) :1;` |
|        - |   86 | `	}` |
|   535319 |   87 | `	for(;;){` |
|  1069890 |   88 | `		if( nLen <= 0 ){ return 0; } if( zP[0] == 0 \|\| zQ[0] == 0 \|\| zP[0] != zQ[0] ){ break; } zP++; zQ++; nLen--;` |
|    43749 |   89 | `		if( nLen <= 0 ){ return 0; } if( zP[0] == 0 \|\| zQ[0] == 0 \|\| zP[0] != zQ[0] ){ break; } zP++; zQ++; nLen--;` |
|    42689 |   90 | `		if( nLen <= 0 ){ return 0; } if( zP[0] == 0 \|\| zQ[0] == 0 \|\| zP[0] != zQ[0] ){ break; } zP++; zQ++; nLen--;` |
|    42641 |   91 | `		if( nLen <= 0 ){ return 0; } if( zP[0] == 0 \|\| zQ[0] == 0 \|\| zP[0] != zQ[0] ){ break; } zP++; zQ++; nLen--;` |
|        5 |   92 | `	}` |
|  1023984 |   93 | `	return (sxi32)(zP[0] - zQ[0]);` |
|   515848 |   94 | `}` |
| 14997792 |   95 | `PH7_PRIVATE sxi32 SyStrnicmp(const char *zLeft, const char *zRight,sxu32 SLen)` |
|        5 |   96 | `{` |
| 14997797 |   97 | `  	register unsigned char *p = (unsigned char *)zLeft;` |
| 14997797 |   98 | `	register unsigned char *q = (unsigned char *)zRight;` |
|        - |   99 |  |
|        - |  100 | `	/* Zero bytes compared is always equal -- see the note in SyStrncmp. */` |
| 14997797 |  101 | `	if( !SLen ){` |
|      ! 0 |  102 | `		return 0;` |
|        - |  103 | `	}` |
| 14997797 |  104 | `	if( SX_EMPTY_STR(p) \|\| SX_EMPTY_STR(q) ){` |
|      ! 0 |  105 | `		return SX_EMPTY_STR(p)? SX_EMPTY_STR(q) ? 0 : -1 :1;` |
|        - |  106 | `	}` |
| 11994425 |  107 | `	for(;;){` |
| 23988855 |  108 | `		if( !SLen ){ return 0; }if( !*p \|\| !*q \|\| SyCharToLower(*p) != SyCharToLower(*q) ){ break; }p++;q++;--SLen;` |
| 14180183 |  109 | `		if( !SLen ){ return 0; }if( !*p \|\| !*q \|\| SyCharToLower(*p) != SyCharToLower(*q) ){ break; }p++;q++;--SLen;` |
| 12262423 |  110 | `		if( !SLen ){ return 0; }if( !*p \|\| !*q \|\| SyCharToLower(*p) != SyCharToLower(*q) ){ break; }p++;q++;--SLen;` |
| 10864643 |  111 | `		if( !SLen ){ return 0; }if( !*p \|\| !*q \|\| SyCharToLower(*p) != SyCharToLower(*q) ){ break; }p++;q++;--SLen;` |
|        - |  112 |  |
|        5 |  113 | `	}` |
|  8037183 |  114 | `	return (sxi32)(SyCharToLower(p[0]) - SyCharToLower(q[0]));` |
|  7498901 |  115 | `}` |
|  3241258 |  116 | `PH7_PRIVATE sxi32 SyStrnmicmp(const void *pLeft, const void *pRight,sxu32 SLen)` |
|        5 |  117 | `{` |
|  3241263 |  118 | `	return SyStrnicmp((const char *)pLeft,(const char *)pRight,SLen);` |
|        5 |  119 | `}` |
|  9818120 |  120 | `PH7_PRIVATE sxu32 Systrcpy(char *zDest,sxu32 nDestLen,const char *zSrc,sxu32 nLen)` |
|        5 |  121 | `{` |
|  9818125 |  122 | `	unsigned char *zBuf = (unsigned char *)zDest;` |
|  9818125 |  123 | `	unsigned char *zIn = (unsigned char *)zSrc;` |
|        - |  124 | `	unsigned char *zEnd;` |
|        - |  125 | `#if defined(UNTRUST)` |
|        - |  126 | `	if( zSrc == (const char *)zDest ){` |
|        - |  127 | `			return 0;` |
|        - |  128 | `	}` |
|        - |  129 | `#endif` |
|  9818125 |  130 | `	if( nLen <= 0 ){` |
|      ! 0 |  131 | `		nLen = SyStrlen(zSrc);` |
|      ! 0 |  132 | `	}` |
|  9818125 |  133 | `	zEnd = &zBuf[nDestLen - 1]; /* reserve a room for the null terminator */` |
| 14924549 |  134 | `	for(;;){` |
| 29846782 |  135 | `		if( zBuf >= zEnd \|\| nLen == 0 ){ break;} zBuf[0] = zIn[0]; zIn++; zBuf++; nLen--;` |
| 27199408 |  136 | `		if( zBuf >= zEnd \|\| nLen == 0 ){ break;} zBuf[0] = zIn[0]; zIn++; zBuf++; nLen--;` |
| 24495599 |  137 | `		if( zBuf >= zEnd \|\| nLen == 0 ){ break;} zBuf[0] = zIn[0]; zIn++; zBuf++; nLen--;` |
| 22214877 |  138 | `		if( zBuf >= zEnd \|\| nLen == 0 ){ break;} zBuf[0] = zIn[0]; zIn++; zBuf++; nLen--;` |
|        5 |  139 | `	}` |
|  9818125 |  140 | `	zBuf[0] = 0;` |
|  9818125 |  141 | `	return (sxu32)(zBuf-(unsigned char *)zDest);` |
|        5 |  142 | `}` |
|        - |  143 |  |
