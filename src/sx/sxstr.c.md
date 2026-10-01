# src/sx/sxstr.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 90/93 lines (96.77%)

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
| 132933929 |   10 | `PH7_PRIVATE sxu32 SyStrlen(const char *zSrc)` |
|         5 |   11 | `{` |
|         - |   12 | `#if defined(UNTRUST)` |
|         - |   13 | `	if( zSrc == 0 ){` |
|         - |   14 | `		return 0;` |
|         - |   15 | `	}` |
|         - |   16 | `#endif` |
|         - |   17 | `	/* The C library's, not a byte loop: this is called ~111M times in a` |
|         - |   18 | `	 * nine-second run of the ecosystem gate's phpcs step. */` |
| 132933934 |   19 | `	return (sxu32)strlen(zSrc);` |
|         5 |   20 | `}` |
|      5500 |   21 | `PH7_PRIVATE sxi32 SyByteFind(const char *zStr,sxu32 nLen,sxi32 c,sxu32 *pPos)` |
|         5 |   22 | `{` |
|      5505 |   23 | `	const char *zIn = zStr;` |
|         - |   24 | `	const char *zEnd;` |
|         - |   25 |  |
|      5505 |   26 | `	zEnd = &zIn[nLen];` |
|     13340 |   27 | `	for(;;){` |
|     23401 |   28 | `		if( zIn >= zEnd ){ break; }if( zIn[0] == c ){ if( pPos ){ *pPos = (sxu32)(zIn - zStr); } return SXRET_OK; } zIn++;` |
|     22665 |   29 | `		if( zIn >= zEnd ){ break; }if( zIn[0] == c ){ if( pPos ){ *pPos = (sxu32)(zIn - zStr); } return SXRET_OK; } zIn++;` |
|     19903 |   30 | `		if( zIn >= zEnd ){ break; }if( zIn[0] == c ){ if( pPos ){ *pPos = (sxu32)(zIn - zStr); } return SXRET_OK; } zIn++;` |
|     19140 |   31 | `		if( zIn >= zEnd ){ break; }if( zIn[0] == c ){ if( pPos ){ *pPos = (sxu32)(zIn - zStr); } return SXRET_OK; } zIn++;` |
|         5 |   32 | `	}` |
|      4229 |   33 | `	return SXERR_NOTFOUND;` |
|      2756 |   34 | `}` |
|         - |   35 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        60 |   36 | `PH7_PRIVATE sxi32 SyByteFind2(const char *zStr,sxu32 nLen,sxi32 c,sxu32 *pPos)` |
|         1 |   37 | `{` |
|        61 |   38 | `	const char *zIn = zStr;` |
|         - |   39 | `	const char *zEnd;` |
|         - |   40 |  |
|        61 |   41 | `	zEnd = &zIn[nLen - 1];` |
|        44 |   42 | `	for( ;; ){` |
|        89 |   43 | `		if( zEnd < zIn ){ break; } if( zEnd[0] == c ){ if( pPos ){ *pPos =  (sxu32)(zEnd - zIn);} return SXRET_OK; } zEnd--;` |
|        73 |   44 | `		if( zEnd < zIn ){ break; } if( zEnd[0] == c ){ if( pPos ){ *pPos =  (sxu32)(zEnd - zIn);} return SXRET_OK; } zEnd--;` |
|        51 |   45 | `		if( zEnd < zIn ){ break; } if( zEnd[0] == c ){ if( pPos ){ *pPos =  (sxu32)(zEnd - zIn);} return SXRET_OK; } zEnd--;` |
|        45 |   46 | `		if( zEnd < zIn ){ break; } if( zEnd[0] == c ){ if( pPos ){ *pPos =  (sxu32)(zEnd - zIn);} return SXRET_OK; } zEnd--;` |
|         1 |   47 | `	}` |
|        11 |   48 | `	return SXERR_NOTFOUND;` |
|        31 |   49 | `}` |
|         - |   50 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|       178 |   51 | `PH7_PRIVATE sxi32 SyByteListFind(const char *zSrc,sxu32 nLen,const char *zList,sxu32 *pFirstPos)` |
|       ! 0 |   52 | `{` |
|       178 |   53 | `	const char *zIn = zSrc;` |
|         - |   54 | `	const char *zPtr;` |
|         - |   55 | `	const char *zEnd;` |
|         - |   56 | `	sxi32 c;` |
|       178 |   57 | `	zEnd = &zSrc[nLen];` |
|       481 |   58 | `	for(;;){` |
|      2874 |   59 | `		if( zIn >= zEnd ){ break; }	for(zPtr = zList ; (c = zPtr[0]) != 0 ; zPtr++ ){ if( zIn[0] == c ){ if( pFirstPos ){ *pFirstPos = (sxu32)(zIn - zSrc); } return SXRET_OK; } } zIn++;` |
|      2764 |   60 | `		if( zIn >= zEnd ){ break; }	for(zPtr = zList ; (c = zPtr[0]) != 0 ; zPtr++ ){ if( zIn[0] == c ){ if( pFirstPos ){ *pFirstPos = (sxu32)(zIn - zSrc); } return SXRET_OK; } } zIn++;` |
|      2616 |   61 | `		if( zIn >= zEnd ){ break; }	for(zPtr = zList ; (c = zPtr[0]) != 0 ; zPtr++ ){ if( zIn[0] == c ){ if( pFirstPos ){ *pFirstPos = (sxu32)(zIn - zSrc); } return SXRET_OK; } } zIn++;` |
|      2424 |   62 | `		if( zIn >= zEnd ){ break; }	for(zPtr = zList ; (c = zPtr[0]) != 0 ; zPtr++ ){ if( zIn[0] == c ){ if( pFirstPos ){ *pFirstPos = (sxu32)(zIn - zSrc); } return SXRET_OK; } } zIn++;` |
|       ! 0 |   63 | `	}` |
|        46 |   64 | `	return SXERR_NOTFOUND;` |
|        89 |   65 | `}` |
|         - |   66 | `/* used by hashmap.c's key sorting — must stay in the tiny build */` |
|   1003073 |   67 | `PH7_PRIVATE sxi32 SyStrncmp(const char *zLeft,const char *zRight,sxu32 nLen)` |
|         5 |   68 | `{` |
|   1003078 |   69 | `	const unsigned char *zP = (const unsigned char *)zLeft;` |
|   1003078 |   70 | `	const unsigned char *zQ = (const unsigned char *)zRight;` |
|         - |   71 |  |
|         - |   72 | `	/* Comparing ZERO bytes is always equal, whatever the operands -- this test has` |
|         - |   73 | `	 * to come before the empty-string shortcut below, which used to run first and` |
|         - |   74 | `	 * so answered -1/1 for a zero-length compare against an empty string. That is` |
|         - |   75 | `	 * what php's strncmp("", "a", 0) exposed: it must be 0. */` |
|   1003078 |   76 | `	if( nLen <= 0 ){` |
|        17 |   77 | `		return 0;` |
|         - |   78 | `	}` |
|   1003062 |   79 | `	if( SX_EMPTY_STR(zP) \|\| SX_EMPTY_STR(zQ)  ){` |
|       398 |   80 | `			return SX_EMPTY_STR(zP) ? (SX_EMPTY_STR(zQ) ? 0 : -1) :1;` |
|         - |   81 | `	}` |
|    563892 |   82 | `	for(;;){` |
|   1133761 |   83 | `		if( nLen <= 0 ){ return 0; } if( zP[0] == 0 \|\| zQ[0] == 0 \|\| zP[0] != zQ[0] ){ break; } zP++; zQ++; nLen--;` |
|    216920 |   84 | `		if( nLen <= 0 ){ return 0; } if( zP[0] == 0 \|\| zQ[0] == 0 \|\| zP[0] != zQ[0] ){ break; } zP++; zQ++; nLen--;` |
|    162541 |   85 | `		if( nLen <= 0 ){ return 0; } if( zP[0] == 0 \|\| zQ[0] == 0 \|\| zP[0] != zQ[0] ){ break; } zP++; zQ++; nLen--;` |
|    150082 |   86 | `		if( nLen <= 0 ){ return 0; } if( zP[0] == 0 \|\| zQ[0] == 0 \|\| zP[0] != zQ[0] ){ break; } zP++; zQ++; nLen--;` |
|         5 |   87 | `	}` |
|    972557 |   88 | `	return (sxi32)(zP[0] - zQ[0]);` |
|    498727 |   89 | `}` |
|  64342192 |   90 | `PH7_PRIVATE sxi32 SyStrnicmp(const char *zLeft, const char *zRight,sxu32 SLen)` |
|         5 |   91 | `{` |
|  64342197 |   92 | `  	register unsigned char *p = (unsigned char *)zLeft;` |
|  64342197 |   93 | `	register unsigned char *q = (unsigned char *)zRight;` |
|         - |   94 |  |
|         - |   95 | `	/* Zero bytes compared is always equal -- see the note in SyStrncmp. */` |
|  64342197 |   96 | `	if( !SLen ){` |
|       ! 0 |   97 | `		return 0;` |
|         - |   98 | `	}` |
|  64342197 |   99 | `	if( SX_EMPTY_STR(p) \|\| SX_EMPTY_STR(q) ){` |
|       410 |  100 | `		return SX_EMPTY_STR(p)? SX_EMPTY_STR(q) ? 0 : -1 :1;` |
|         - |  101 | `	}` |
| 193357409 |  102 | `	for(;;){` |
| 387309140 |  103 | `		if( !SLen ){ return 0; }if( !*p \|\| !*q \|\| SyCharToLower(*p) != SyCharToLower(*q) ){ break; }p++;q++;--SLen;` |
| 370007120 |  104 | `		if( !SLen ){ return 0; }if( !*p \|\| !*q \|\| SyCharToLower(*p) != SyCharToLower(*q) ){ break; }p++;q++;--SLen;` |
| 351275304 |  105 | `		if( !SLen ){ return 0; }if( !*p \|\| !*q \|\| SyCharToLower(*p) != SyCharToLower(*q) ){ break; }p++;q++;--SLen;` |
| 338797131 |  106 | `		if( !SLen ){ return 0; }if( !*p \|\| !*q \|\| SyCharToLower(*p) != SyCharToLower(*q) ){ break; }p++;q++;--SLen;` |
|         - |  107 |  |
|         5 |  108 | `	}` |
|   5956825 |  109 | `	return (sxi32)(SyCharToLower(p[0]) - SyCharToLower(q[0]));` |
|  32125294 |  110 | `}` |
|  57508127 |  111 | `PH7_PRIVATE sxi32 SyStrnmicmp(const void *pLeft, const void *pRight,sxu32 SLen)` |
|         5 |  112 | `{` |
|  57508132 |  113 | `	return SyStrnicmp((const char *)pLeft,(const char *)pRight,SLen);` |
|         5 |  114 | `}` |
|  35096348 |  115 | `PH7_PRIVATE sxu32 Systrcpy(char *zDest,sxu32 nDestLen,const char *zSrc,sxu32 nLen)` |
|         5 |  116 | `{` |
|  35096353 |  117 | `	unsigned char *zBuf = (unsigned char *)zDest;` |
|  35096353 |  118 | `	unsigned char *zIn = (unsigned char *)zSrc;` |
|         - |  119 | `	unsigned char *zEnd;` |
|         - |  120 | `#if defined(UNTRUST)` |
|         - |  121 | `	if( zSrc == (const char *)zDest ){` |
|         - |  122 | `			return 0;` |
|         - |  123 | `	}` |
|         - |  124 | `#endif` |
|  35096353 |  125 | `	if( nLen <= 0 ){` |
|        33 |  126 | `		nLen = SyStrlen(zSrc);` |
|         7 |  127 | `	}` |
|  35096353 |  128 | `	zEnd = &zBuf[nDestLen - 1]; /* reserve a room for the null terminator */` |
|  84005414 |  129 | `	for(;;){` |
| 169582320 |  130 | `		if( zBuf >= zEnd \|\| nLen == 0 ){ break;} zBuf[0] = zIn[0]; zIn++; zBuf++; nLen--;` |
| 160887797 |  131 | `		if( zBuf >= zEnd \|\| nLen == 0 ){ break;} zBuf[0] = zIn[0]; zIn++; zBuf++; nLen--;` |
| 151613196 |  132 | `		if( zBuf >= zEnd \|\| nLen == 0 ){ break;} zBuf[0] = zIn[0]; zIn++; zBuf++; nLen--;` |
| 142884108 |  133 | `		if( zBuf >= zEnd \|\| nLen == 0 ){ break;} zBuf[0] = zIn[0]; zIn++; zBuf++; nLen--;` |
|         5 |  134 | `	}` |
|  35096353 |  135 | `	zBuf[0] = 0;` |
|  35096353 |  136 | `	return (sxu32)(zBuf-(unsigned char *)zDest);` |
|         5 |  137 | `}` |
|         - |  138 |  |
