# src/sx/sxstr.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 91/93 lines (97.85%)

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
| 241261163 |   10 | `PH7_PRIVATE sxu32 SyStrlen(const char *zSrc)` |
|         5 |   11 | `{` |
|         - |   12 | `#if defined(UNTRUST)` |
|         - |   13 | `	if( zSrc == 0 ){` |
|         - |   14 | `		return 0;` |
|         - |   15 | `	}` |
|         - |   16 | `#endif` |
|         - |   17 | `	/* The C library's, not a byte loop: this is called ~111M times in a` |
|         - |   18 | `	 * nine-second run of the ecosystem gate's phpcs step. */` |
| 241261168 |   19 | `	return (sxu32)strlen(zSrc);` |
|         5 |   20 | `}` |
|   2124106 |   21 | `PH7_PRIVATE sxi32 SyByteFind(const char *zStr,sxu32 nLen,sxi32 c,sxu32 *pPos)` |
|         5 |   22 | `{` |
|   2124111 |   23 | `	const char *zIn = zStr;` |
|         - |   24 | `	const char *zEnd;` |
|         - |   25 |  |
|   2124111 |   26 | `	zEnd = &zIn[nLen];` |
|   4775106 |   27 | `	for(;;){` |
|   9559405 |   28 | `		if( zIn >= zEnd ){ break; }if( zIn[0] == c ){ if( pPos ){ *pPos = (sxu32)(zIn - zStr); } return SXRET_OK; } zIn++;` |
|   8961532 |   29 | `		if( zIn >= zEnd ){ break; }if( zIn[0] == c ){ if( pPos ){ *pPos = (sxu32)(zIn - zStr); } return SXRET_OK; } zIn++;` |
|   8503514 |   30 | `		if( zIn >= zEnd ){ break; }if( zIn[0] == c ){ if( pPos ){ *pPos = (sxu32)(zIn - zStr); } return SXRET_OK; } zIn++;` |
|   7965115 |   31 | `		if( zIn >= zEnd ){ break; }if( zIn[0] == c ){ if( pPos ){ *pPos = (sxu32)(zIn - zStr); } return SXRET_OK; } zIn++;` |
|         5 |   32 | `	}` |
|   2120588 |   33 | `	return SXERR_NOTFOUND;` |
|   1060660 |   34 | `}` |
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
|   1763307 |   67 | `PH7_PRIVATE sxi32 SyStrncmp(const char *zLeft,const char *zRight,sxu32 nLen)` |
|         5 |   68 | `{` |
|   1763312 |   69 | `	const unsigned char *zP = (const unsigned char *)zLeft;` |
|   1763312 |   70 | `	const unsigned char *zQ = (const unsigned char *)zRight;` |
|         - |   71 |  |
|         - |   72 | `	/* Comparing ZERO bytes is always equal, whatever the operands -- this test has` |
|         - |   73 | `	 * to come before the empty-string shortcut below, which used to run first and` |
|         - |   74 | `	 * so answered -1/1 for a zero-length compare against an empty string. That is` |
|         - |   75 | `	 * what php's strncmp("", "a", 0) exposed: it must be 0. */` |
|   1763312 |   76 | `	if( nLen <= 0 ){` |
|        17 |   77 | `		return 0;` |
|         - |   78 | `	}` |
|   1763296 |   79 | `	if( SX_EMPTY_STR(zP) \|\| SX_EMPTY_STR(zQ)  ){` |
|       390 |   80 | `			return SX_EMPTY_STR(zP) ? (SX_EMPTY_STR(zQ) ? 0 : -1) :1;` |
|         - |   81 | `	}` |
|   1078471 |   82 | `	for(;;){` |
|   2163582 |   83 | `		if( nLen <= 0 ){ return 0; } if( zP[0] == 0 \|\| zQ[0] == 0 \|\| zP[0] != zQ[0] ){ break; } zP++; zQ++; nLen--;` |
|    682218 |   84 | `		if( nLen <= 0 ){ return 0; } if( zP[0] == 0 \|\| zQ[0] == 0 \|\| zP[0] != zQ[0] ){ break; } zP++; zQ++; nLen--;` |
|    497641 |   85 | `		if( nLen <= 0 ){ return 0; } if( zP[0] == 0 \|\| zQ[0] == 0 \|\| zP[0] != zQ[0] ){ break; } zP++; zQ++; nLen--;` |
|    467060 |   86 | `		if( nLen <= 0 ){ return 0; } if( zP[0] == 0 \|\| zQ[0] == 0 \|\| zP[0] != zQ[0] ){ break; } zP++; zQ++; nLen--;` |
|         5 |   87 | `	}` |
|   1639071 |   88 | `	return (sxi32)(zP[0] - zQ[0]);` |
|    878522 |   89 | `}` |
| 132021133 |   90 | `PH7_PRIVATE sxi32 SyStrnicmp(const char *zLeft, const char *zRight,sxu32 SLen)` |
|         5 |   91 | `{` |
| 132021138 |   92 | `  	register unsigned char *p = (unsigned char *)zLeft;` |
| 132021138 |   93 | `	register unsigned char *q = (unsigned char *)zRight;` |
|         - |   94 |  |
|         - |   95 | `	/* Zero bytes compared is always equal -- see the note in SyStrncmp. */` |
| 132021138 |   96 | `	if( !SLen ){` |
|        42 |   97 | `		return 0;` |
|         - |   98 | `	}` |
| 132021100 |   99 | `	if( SX_EMPTY_STR(p) \|\| SX_EMPTY_STR(q) ){` |
|       410 |  100 | `		return SX_EMPTY_STR(p)? SX_EMPTY_STR(q) ? 0 : -1 :1;` |
|         - |  101 | `	}` |
| 310632066 |  102 | `	for(;;){` |
| 622123173 |  103 | `		if( !SLen ){ return 0; }if( !*p \|\| !*q \|\| SyCharToLower(*p) != SyCharToLower(*q) ){ break; }p++;q++;--SLen;` |
| 566491917 |  104 | `		if( !SLen ){ return 0; }if( !*p \|\| !*q \|\| SyCharToLower(*p) != SyCharToLower(*q) ){ break; }p++;q++;--SLen;` |
| 535185717 |  105 | `		if( !SLen ){ return 0; }if( !*p \|\| !*q \|\| SyCharToLower(*p) != SyCharToLower(*q) ){ break; }p++;q++;--SLen;` |
| 512536263 |  106 | `		if( !SLen ){ return 0; }if( !*p \|\| !*q \|\| SyCharToLower(*p) != SyCharToLower(*q) ){ break; }p++;q++;--SLen;` |
|         - |  107 |  |
|         5 |  108 | `	}` |
|  39523394 |  109 | `	return (sxi32)(SyCharToLower(p[0]) - SyCharToLower(q[0]));` |
|  65946523 |  110 | `}` |
|  78220114 |  111 | `PH7_PRIVATE sxi32 SyStrnmicmp(const void *pLeft, const void *pRight,sxu32 SLen)` |
|         5 |  112 | `{` |
|  78220119 |  113 | `	return SyStrnicmp((const char *)pLeft,(const char *)pRight,SLen);` |
|         5 |  114 | `}` |
|  48662597 |  115 | `PH7_PRIVATE sxu32 Systrcpy(char *zDest,sxu32 nDestLen,const char *zSrc,sxu32 nLen)` |
|         5 |  116 | `{` |
|  48662602 |  117 | `	unsigned char *zBuf = (unsigned char *)zDest;` |
|  48662602 |  118 | `	unsigned char *zIn = (unsigned char *)zSrc;` |
|         - |  119 | `	unsigned char *zEnd;` |
|         - |  120 | `#if defined(UNTRUST)` |
|         - |  121 | `	if( zSrc == (const char *)zDest ){` |
|         - |  122 | `			return 0;` |
|         - |  123 | `	}` |
|         - |  124 | `#endif` |
|  48662602 |  125 | `	if( nLen <= 0 ){` |
|        33 |  126 | `		nLen = SyStrlen(zSrc);` |
|         7 |  127 | `	}` |
|  48662602 |  128 | `	zEnd = &zBuf[nDestLen - 1]; /* reserve a room for the null terminator */` |
| 116630712 |  129 | `	for(;;){` |
| 235237339 |  130 | `		if( zBuf >= zEnd \|\| nLen == 0 ){ break;} zBuf[0] = zIn[0]; zIn++; zBuf++; nLen--;` |
| 223258696 |  131 | `		if( zBuf >= zEnd \|\| nLen == 0 ){ break;} zBuf[0] = zIn[0]; zIn++; zBuf++; nLen--;` |
| 210422361 |  132 | `		if( zBuf >= zEnd \|\| nLen == 0 ){ break;} zBuf[0] = zIn[0]; zIn++; zBuf++; nLen--;` |
| 198340727 |  133 | `		if( zBuf >= zEnd \|\| nLen == 0 ){ break;} zBuf[0] = zIn[0]; zIn++; zBuf++; nLen--;` |
|         5 |  134 | `	}` |
|  48662602 |  135 | `	zBuf[0] = 0;` |
|  48662602 |  136 | `	return (sxu32)(zBuf-(unsigned char *)zDest);` |
|         5 |  137 | `}` |
|         - |  138 |  |
