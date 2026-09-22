# src/sx/sxstr.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 93/100 lines (93.00%)

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
|  61960686 |   10 | `PH7_PRIVATE sxu32 SyStrlen(const char *zSrc)` |
|         5 |   11 | `{` |
|  61960691 |   12 | `	register const char *zIn = zSrc;` |
|         - |   13 | `#if defined(UNTRUST)` |
|         - |   14 | `	if( zIn == 0 ){` |
|         - |   15 | `		return 0;` |
|         - |   16 | `	}` |
|         - |   17 | `#endif` |
|  86550822 |   18 | `	for(;;){` |
| 173039240 |   19 | `		if( !zIn[0] ){ break; } zIn++;` |
| 148678558 |   20 | `		if( !zIn[0] ){ break; } zIn++;` |
| 132319453 |   21 | `		if( !zIn[0] ){ break; } zIn++;` |
| 123043215 |   22 | `		if( !zIn[0] ){ break; } zIn++;` |
|         5 |   23 | `	}` |
|  61960691 |   24 | `	return (sxu32)(zIn - zSrc);` |
|         5 |   25 | `}` |
|       350 |   26 | `PH7_PRIVATE sxi32 SyByteFind(const char *zStr,sxu32 nLen,sxi32 c,sxu32 *pPos)` |
|         4 |   27 | `{` |
|       354 |   28 | `	const char *zIn = zStr;` |
|         - |   29 | `	const char *zEnd;` |
|         - |   30 |  |
|       354 |   31 | `	zEnd = &zIn[nLen];` |
|       353 |   32 | `	for(;;){` |
|       710 |   33 | `		if( zIn >= zEnd ){ break; }if( zIn[0] == c ){ if( pPos ){ *pPos = (sxu32)(zIn - zStr); } return SXRET_OK; } zIn++;` |
|       618 |   34 | `		if( zIn >= zEnd ){ break; }if( zIn[0] == c ){ if( pPos ){ *pPos = (sxu32)(zIn - zStr); } return SXRET_OK; } zIn++;` |
|       546 |   35 | `		if( zIn >= zEnd ){ break; }if( zIn[0] == c ){ if( pPos ){ *pPos = (sxu32)(zIn - zStr); } return SXRET_OK; } zIn++;` |
|       426 |   36 | `		if( zIn >= zEnd ){ break; }if( zIn[0] == c ){ if( pPos ){ *pPos = (sxu32)(zIn - zStr); } return SXRET_OK; } zIn++;` |
|         3 |   37 | `	}` |
|       176 |   38 | `	return SXERR_NOTFOUND;` |
|       179 |   39 | `}` |
|         - |   40 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        20 |   41 | `PH7_PRIVATE sxi32 SyByteFind2(const char *zStr,sxu32 nLen,sxi32 c,sxu32 *pPos)` |
|         1 |   42 | `{` |
|        21 |   43 | `	const char *zIn = zStr;` |
|         - |   44 | `	const char *zEnd;` |
|         - |   45 |  |
|        21 |   46 | `	zEnd = &zIn[nLen - 1];` |
|        20 |   47 | `	for( ;; ){` |
|        41 |   48 | `		if( zEnd < zIn ){ break; } if( zEnd[0] == c ){ if( pPos ){ *pPos =  (sxu32)(zEnd - zIn);} return SXRET_OK; } zEnd--;` |
|        37 |   49 | `		if( zEnd < zIn ){ break; } if( zEnd[0] == c ){ if( pPos ){ *pPos =  (sxu32)(zEnd - zIn);} return SXRET_OK; } zEnd--;` |
|        33 |   50 | `		if( zEnd < zIn ){ break; } if( zEnd[0] == c ){ if( pPos ){ *pPos =  (sxu32)(zEnd - zIn);} return SXRET_OK; } zEnd--;` |
|        31 |   51 | `		if( zEnd < zIn ){ break; } if( zEnd[0] == c ){ if( pPos ){ *pPos =  (sxu32)(zEnd - zIn);} return SXRET_OK; } zEnd--;` |
|         1 |   52 | `	}` |
|         9 |   53 | `	return SXERR_NOTFOUND;` |
|        11 |   54 | `}` |
|         - |   55 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|       120 |   56 | `PH7_PRIVATE sxi32 SyByteListFind(const char *zSrc,sxu32 nLen,const char *zList,sxu32 *pFirstPos)` |
|       ! 0 |   57 | `{` |
|       120 |   58 | `	const char *zIn = zSrc;` |
|         - |   59 | `	const char *zPtr;` |
|         - |   60 | `	const char *zEnd;` |
|         - |   61 | `	sxi32 c;` |
|       120 |   62 | `	zEnd = &zSrc[nLen];` |
|       314 |   63 | `	for(;;){` |
|      1880 |   64 | `		if( zIn >= zEnd ){ break; }	for(zPtr = zList ; (c = zPtr[0]) != 0 ; zPtr++ ){ if( zIn[0] == c ){ if( pFirstPos ){ *pFirstPos = (sxu32)(zIn - zSrc); } return SXRET_OK; } } zIn++;` |
|      1810 |   65 | `		if( zIn >= zEnd ){ break; }	for(zPtr = zList ; (c = zPtr[0]) != 0 ; zPtr++ ){ if( zIn[0] == c ){ if( pFirstPos ){ *pFirstPos = (sxu32)(zIn - zSrc); } return SXRET_OK; } } zIn++;` |
|      1708 |   66 | `		if( zIn >= zEnd ){ break; }	for(zPtr = zList ; (c = zPtr[0]) != 0 ; zPtr++ ){ if( zIn[0] == c ){ if( pFirstPos ){ *pFirstPos = (sxu32)(zIn - zSrc); } return SXRET_OK; } } zIn++;` |
|      1574 |   67 | `		if( zIn >= zEnd ){ break; }	for(zPtr = zList ; (c = zPtr[0]) != 0 ; zPtr++ ){ if( zIn[0] == c ){ if( pFirstPos ){ *pFirstPos = (sxu32)(zIn - zSrc); } return SXRET_OK; } } zIn++;` |
|       ! 0 |   68 | `	}` |
|        32 |   69 | `	return SXERR_NOTFOUND;` |
|        60 |   70 | `}` |
|         - |   71 | `/* used by hashmap.c's key sorting — must stay in the tiny build */` |
|   4216250 |   72 | `PH7_PRIVATE sxi32 SyStrncmp(const char *zLeft,const char *zRight,sxu32 nLen)` |
|         5 |   73 | `{` |
|   4216255 |   74 | `	const unsigned char *zP = (const unsigned char *)zLeft;` |
|   4216255 |   75 | `	const unsigned char *zQ = (const unsigned char *)zRight;` |
|         - |   76 |  |
|         - |   77 | `	/* Comparing ZERO bytes is always equal, whatever the operands -- this test has` |
|         - |   78 | `	 * to come before the empty-string shortcut below, which used to run first and` |
|         - |   79 | `	 * so answered -1/1 for a zero-length compare against an empty string. That is` |
|         - |   80 | `	 * what php's strncmp("", "a", 0) exposed: it must be 0. */` |
|   4216255 |   81 | `	if( nLen <= 0 ){` |
|        17 |   82 | `		return 0;` |
|         - |   83 | `	}` |
|   4216239 |   84 | `	if( SX_EMPTY_STR(zP) \|\| SX_EMPTY_STR(zQ)  ){` |
|       ! 0 |   85 | `			return SX_EMPTY_STR(zP) ? (SX_EMPTY_STR(zQ) ? 0 : -1) :1;` |
|         - |   86 | `	}` |
|   2177633 |   87 | `	for(;;){` |
|   4349323 |   88 | `		if( nLen <= 0 ){ return 0; } if( zP[0] == 0 \|\| zQ[0] == 0 \|\| zP[0] != zQ[0] ){ break; } zP++; zQ++; nLen--;` |
|    814395 |   89 | `		if( nLen <= 0 ){ return 0; } if( zP[0] == 0 \|\| zQ[0] == 0 \|\| zP[0] != zQ[0] ){ break; } zP++; zQ++; nLen--;` |
|    389597 |   90 | `		if( nLen <= 0 ){ return 0; } if( zP[0] == 0 \|\| zQ[0] == 0 \|\| zP[0] != zQ[0] ){ break; } zP++; zQ++; nLen--;` |
|    389529 |   91 | `		if( nLen <= 0 ){ return 0; } if( zP[0] == 0 \|\| zQ[0] == 0 \|\| zP[0] != zQ[0] ){ break; } zP++; zQ++; nLen--;` |
|         5 |   92 | `	}` |
|   4171225 |   93 | `	return (sxi32)(zP[0] - zQ[0]);` |
|   2111104 |   94 | `}` |
|  45529807 |   95 | `PH7_PRIVATE sxi32 SyStrnicmp(const char *zLeft, const char *zRight,sxu32 SLen)` |
|         5 |   96 | `{` |
|  45529812 |   97 | `  	register unsigned char *p = (unsigned char *)zLeft;` |
|  45529812 |   98 | `	register unsigned char *q = (unsigned char *)zRight;` |
|         - |   99 |  |
|         - |  100 | `	/* Zero bytes compared is always equal -- see the note in SyStrncmp. */` |
|  45529812 |  101 | `	if( !SLen ){` |
|       ! 0 |  102 | `		return 0;` |
|         - |  103 | `	}` |
|  45529812 |  104 | `	if( SX_EMPTY_STR(p) \|\| SX_EMPTY_STR(q) ){` |
|       ! 0 |  105 | `		return SX_EMPTY_STR(p)? SX_EMPTY_STR(q) ? 0 : -1 :1;` |
|         - |  106 | `	}` |
|  66764719 |  107 | `	for(;;){` |
| 133523792 |  108 | `		if( !SLen ){ return 0; }if( !*p \|\| !*q \|\| SyCharToLower(*p) != SyCharToLower(*q) ){ break; }p++;q++;--SLen;` |
| 114577631 |  109 | `		if( !SLen ){ return 0; }if( !*p \|\| !*q \|\| SyCharToLower(*p) != SyCharToLower(*q) ){ break; }p++;q++;--SLen;` |
| 100751920 |  110 | `		if( !SLen ){ return 0; }if( !*p \|\| !*q \|\| SyCharToLower(*p) != SyCharToLower(*q) ){ break; }p++;q++;--SLen;` |
|  95666432 |  111 | `		if( !SLen ){ return 0; }if( !*p \|\| !*q \|\| SyCharToLower(*p) != SyCharToLower(*q) ){ break; }p++;q++;--SLen;` |
|         - |  112 |  |
|         5 |  113 | `	}` |
|  14197795 |  114 | `	return (sxi32)(SyCharToLower(p[0]) - SyCharToLower(q[0]));` |
|  22765850 |  115 | `}` |
|  26366045 |  116 | `PH7_PRIVATE sxi32 SyStrnmicmp(const void *pLeft, const void *pRight,sxu32 SLen)` |
|         5 |  117 | `{` |
|  26366050 |  118 | `	return SyStrnicmp((const char *)pLeft,(const char *)pRight,SLen);` |
|         5 |  119 | `}` |
|  13302764 |  120 | `PH7_PRIVATE sxu32 Systrcpy(char *zDest,sxu32 nDestLen,const char *zSrc,sxu32 nLen)` |
|         5 |  121 | `{` |
|  13302769 |  122 | `	unsigned char *zBuf = (unsigned char *)zDest;` |
|  13302769 |  123 | `	unsigned char *zIn = (unsigned char *)zSrc;` |
|         - |  124 | `	unsigned char *zEnd;` |
|         - |  125 | `#if defined(UNTRUST)` |
|         - |  126 | `	if( zSrc == (const char *)zDest ){` |
|         - |  127 | `			return 0;` |
|         - |  128 | `	}` |
|         - |  129 | `#endif` |
|  13302769 |  130 | `	if( nLen <= 0 ){` |
|       ! 0 |  131 | `		nLen = SyStrlen(zSrc);` |
|       ! 0 |  132 | `	}` |
|  13302769 |  133 | `	zEnd = &zBuf[nDestLen - 1]; /* reserve a room for the null terminator */` |
|  21199277 |  134 | `	for(;;){` |
|  42396099 |  135 | `		if( zBuf >= zEnd \|\| nLen == 0 ){ break;} zBuf[0] = zIn[0]; zIn++; zBuf++; nLen--;` |
|  38920269 |  136 | `		if( zBuf >= zEnd \|\| nLen == 0 ){ break;} zBuf[0] = zIn[0]; zIn++; zBuf++; nLen--;` |
|  35218716 |  137 | `		if( zBuf >= zEnd \|\| nLen == 0 ){ break;} zBuf[0] = zIn[0]; zIn++; zBuf++; nLen--;` |
|  32122854 |  138 | `		if( zBuf >= zEnd \|\| nLen == 0 ){ break;} zBuf[0] = zIn[0]; zIn++; zBuf++; nLen--;` |
|         5 |  139 | `	}` |
|  13302769 |  140 | `	zBuf[0] = 0;` |
|  13302769 |  141 | `	return (sxu32)(zBuf-(unsigned char *)zDest);` |
|         5 |  142 | `}` |
|         - |  143 |  |
