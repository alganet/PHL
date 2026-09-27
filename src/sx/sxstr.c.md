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
| 146650082 |   10 | `PH7_PRIVATE sxu32 SyStrlen(const char *zSrc)` |
|         5 |   11 | `{` |
| 146650087 |   12 | `	register const char *zIn = zSrc;` |
|         - |   13 | `#if defined(UNTRUST)` |
|         - |   14 | `	if( zIn == 0 ){` |
|         - |   15 | `		return 0;` |
|         - |   16 | `	}` |
|         - |   17 | `#endif` |
| 270708207 |   18 | `	for(;;){` |
| 541191134 |   19 | `		if( !zIn[0] ){ break; } zIn++;` |
| 505484838 |   20 | `		if( !zIn[0] ){ break; } zIn++;` |
| 466446812 |   21 | `		if( !zIn[0] ){ break; } zIn++;` |
| 432282554 |   22 | `		if( !zIn[0] ){ break; } zIn++;` |
|         5 |   23 | `	}` |
| 146650087 |   24 | `	return (sxu32)(zIn - zSrc);` |
|         5 |   25 | `}` |
|      2408 |   26 | `PH7_PRIVATE sxi32 SyByteFind(const char *zStr,sxu32 nLen,sxi32 c,sxu32 *pPos)` |
|         5 |   27 | `{` |
|      2413 |   28 | `	const char *zIn = zStr;` |
|         - |   29 | `	const char *zEnd;` |
|         - |   30 |  |
|      2413 |   31 | `	zEnd = &zIn[nLen];` |
|      4044 |   32 | `	for(;;){` |
|      7684 |   33 | `		if( zIn >= zEnd ){ break; }if( zIn[0] == c ){ if( pPos ){ *pPos = (sxu32)(zIn - zStr); } return SXRET_OK; } zIn++;` |
|      7401 |   34 | `		if( zIn >= zEnd ){ break; }if( zIn[0] == c ){ if( pPos ){ *pPos = (sxu32)(zIn - zStr); } return SXRET_OK; } zIn++;` |
|      5966 |   35 | `		if( zIn >= zEnd ){ break; }if( zIn[0] == c ){ if( pPos ){ *pPos = (sxu32)(zIn - zStr); } return SXRET_OK; } zIn++;` |
|      5643 |   36 | `		if( zIn >= zEnd ){ break; }if( zIn[0] == c ){ if( pPos ){ *pPos = (sxu32)(zIn - zStr); } return SXRET_OK; } zIn++;` |
|         5 |   37 | `	}` |
|      2157 |   38 | `	return SXERR_NOTFOUND;` |
|      1218 |   39 | `}` |
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
|  46860805 |   72 | `PH7_PRIVATE sxi32 SyStrncmp(const char *zLeft,const char *zRight,sxu32 nLen)` |
|         5 |   73 | `{` |
|  46860810 |   74 | `	const unsigned char *zP = (const unsigned char *)zLeft;` |
|  46860810 |   75 | `	const unsigned char *zQ = (const unsigned char *)zRight;` |
|         - |   76 |  |
|         - |   77 | `	/* Comparing ZERO bytes is always equal, whatever the operands -- this test has` |
|         - |   78 | `	 * to come before the empty-string shortcut below, which used to run first and` |
|         - |   79 | `	 * so answered -1/1 for a zero-length compare against an empty string. That is` |
|         - |   80 | `	 * what php's strncmp("", "a", 0) exposed: it must be 0. */` |
|  46860810 |   81 | `	if( nLen <= 0 ){` |
|        17 |   82 | `		return 0;` |
|         - |   83 | `	}` |
|  46860794 |   84 | `	if( SX_EMPTY_STR(zP) \|\| SX_EMPTY_STR(zQ)  ){` |
|       269 |   85 | `			return SX_EMPTY_STR(zP) ? (SX_EMPTY_STR(zQ) ? 0 : -1) :1;` |
|         - |   86 | `	}` |
|  23580366 |   87 | `	for(;;){` |
|  47132811 |   88 | `		if( nLen <= 0 ){ return 0; } if( zP[0] == 0 \|\| zQ[0] == 0 \|\| zP[0] != zQ[0] ){ break; } zP++; zQ++; nLen--;` |
|   1241455 |   89 | `		if( nLen <= 0 ){ return 0; } if( zP[0] == 0 \|\| zQ[0] == 0 \|\| zP[0] != zQ[0] ){ break; } zP++; zQ++; nLen--;` |
|    662276 |   90 | `		if( nLen <= 0 ){ return 0; } if( zP[0] == 0 \|\| zQ[0] == 0 \|\| zP[0] != zQ[0] ){ break; } zP++; zQ++; nLen--;` |
|    586914 |   91 | `		if( nLen <= 0 ){ return 0; } if( zP[0] == 0 \|\| zQ[0] == 0 \|\| zP[0] != zQ[0] ){ break; } zP++; zQ++; nLen--;` |
|         5 |   92 | `	}` |
|  46800239 |   93 | `	return (sxi32)(zP[0] - zQ[0]);` |
|  23444331 |   94 | `}` |
|  48539088 |   95 | `PH7_PRIVATE sxi32 SyStrnicmp(const char *zLeft, const char *zRight,sxu32 SLen)` |
|         5 |   96 | `{` |
|  48539093 |   97 | `  	register unsigned char *p = (unsigned char *)zLeft;` |
|  48539093 |   98 | `	register unsigned char *q = (unsigned char *)zRight;` |
|         - |   99 |  |
|         - |  100 | `	/* Zero bytes compared is always equal -- see the note in SyStrncmp. */` |
|  48539093 |  101 | `	if( !SLen ){` |
|       ! 0 |  102 | `		return 0;` |
|         - |  103 | `	}` |
|  48539093 |  104 | `	if( SX_EMPTY_STR(p) \|\| SX_EMPTY_STR(q) ){` |
|       ! 0 |  105 | `		return SX_EMPTY_STR(p)? SX_EMPTY_STR(q) ? 0 : -1 :1;` |
|         - |  106 | `	}` |
| 131863954 |  107 | `	for(;;){` |
| 263710183 |  108 | `		if( !SLen ){ return 0; }if( !*p \|\| !*q \|\| SyCharToLower(*p) != SyCharToLower(*q) ){ break; }p++;q++;--SLen;` |
| 249387414 |  109 | `		if( !SLen ){ return 0; }if( !*p \|\| !*q \|\| SyCharToLower(*p) != SyCharToLower(*q) ){ break; }p++;q++;--SLen;` |
| 234200136 |  110 | `		if( !SLen ){ return 0; }if( !*p \|\| !*q \|\| SyCharToLower(*p) != SyCharToLower(*q) ){ break; }p++;q++;--SLen;` |
| 226643832 |  111 | `		if( !SLen ){ return 0; }if( !*p \|\| !*q \|\| SyCharToLower(*p) != SyCharToLower(*q) ){ break; }p++;q++;--SLen;` |
|         - |  112 |  |
|         5 |  113 | `	}` |
|   6189477 |  114 | `	return (sxi32)(SyCharToLower(p[0]) - SyCharToLower(q[0]));` |
|  24274209 |  115 | `}` |
|  41762199 |  116 | `PH7_PRIVATE sxi32 SyStrnmicmp(const void *pLeft, const void *pRight,sxu32 SLen)` |
|         5 |  117 | `{` |
|  41762204 |  118 | `	return SyStrnicmp((const char *)pLeft,(const char *)pRight,SLen);` |
|         5 |  119 | `}` |
|  19117552 |  120 | `PH7_PRIVATE sxu32 Systrcpy(char *zDest,sxu32 nDestLen,const char *zSrc,sxu32 nLen)` |
|         5 |  121 | `{` |
|  19117557 |  122 | `	unsigned char *zBuf = (unsigned char *)zDest;` |
|  19117557 |  123 | `	unsigned char *zIn = (unsigned char *)zSrc;` |
|         - |  124 | `	unsigned char *zEnd;` |
|         - |  125 | `#if defined(UNTRUST)` |
|         - |  126 | `	if( zSrc == (const char *)zDest ){` |
|         - |  127 | `			return 0;` |
|         - |  128 | `	}` |
|         - |  129 | `#endif` |
|  19117557 |  130 | `	if( nLen <= 0 ){` |
|        21 |  131 | `		nLen = SyStrlen(zSrc);` |
|         5 |  132 | `	}` |
|  19117557 |  133 | `	zEnd = &zBuf[nDestLen - 1]; /* reserve a room for the null terminator */` |
|  47839165 |  134 | `	for(;;){` |
|  95673135 |  135 | `		if( zBuf >= zEnd \|\| nLen == 0 ){ break;} zBuf[0] = zIn[0]; zIn++; zBuf++; nLen--;` |
|  90997121 |  136 | `		if( zBuf >= zEnd \|\| nLen == 0 ){ break;} zBuf[0] = zIn[0]; zIn++; zBuf++; nLen--;` |
|  86029390 |  137 | `		if( zBuf >= zEnd \|\| nLen == 0 ){ break;} zBuf[0] = zIn[0]; zIn++; zBuf++; nLen--;` |
|  81378678 |  138 | `		if( zBuf >= zEnd \|\| nLen == 0 ){ break;} zBuf[0] = zIn[0]; zIn++; zBuf++; nLen--;` |
|         5 |  139 | `	}` |
|  19117557 |  140 | `	zBuf[0] = 0;` |
|  19117557 |  141 | `	return (sxu32)(zBuf-(unsigned char *)zDest);` |
|         5 |  142 | `}` |
|         - |  143 |  |
