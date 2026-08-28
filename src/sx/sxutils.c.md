# src/sx/sxutils.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 303/388 lines (78.09%)

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
|       - |    8 | `#include "sxutils.h"` |
|       - |    9 | `#include "sxstr.h"` |
|       - |   10 | `#include <stdlib.h> /* strtod — SyStrToReal must be correctly rounded (see its comment) */` |
|       - |   11 |  |
|     292 |   12 | `PH7_PRIVATE sxi32 SyStrIsNumeric(const char *zSrc,sxu32 nLen,sxu8 *pReal,const char  **pzTail)` |
|       5 |   13 | `{` |
|       - |   14 | `	const char *zCur,*zEnd;` |
|       - |   15 | `#ifdef UNTRUST` |
|       - |   16 | `	if( SX_EMPTY_STR(zSrc) ){` |
|       - |   17 | `		return SXERR_EMPTY;` |
|       - |   18 | `	}` |
|       - |   19 | `#endif` |
|     297 |   20 | `	zEnd = &zSrc[nLen];` |
|       - |   21 | `	/* Jump leading white spaces */` |
|     317 |   22 | `	while( zSrc < zEnd && (unsigned char)zSrc[0] < 0xc0  && SyisSpace(zSrc[0]) ){` |
|      22 |   23 | `		zSrc++;` |
|       2 |   24 | `	}` |
|     297 |   25 | `	if( zSrc < zEnd && (zSrc[0] == '+' \|\| zSrc[0] == '-') ){` |
|      11 |   26 | `		zSrc++;` |
|       5 |   27 | `	}` |
|     297 |   28 | `	zCur = zSrc;` |
|     297 |   29 | `	if( pReal ){` |
|     273 |   30 | `		*pReal = FALSE;` |
|     134 |   31 | `	}` |
|     146 |   32 | `	for(;;){` |
|     297 |   33 | `		if( zSrc >= zEnd \|\| (unsigned char)zSrc[0] >= 0xc0 \|\| !SyisDigit(zSrc[0]) ){` |
|      49 |   34 | `			break;` |
|       - |   35 | `		}` |
|     205 |   36 | `		zSrc++;` |
|     205 |   37 | `		if( zSrc >= zEnd \|\| (unsigned char)zSrc[0] >= 0xc0 \|\| !SyisDigit(zSrc[0]) ){` |
|      76 |   38 | `			break;` |
|       - |   39 | `		}` |
|      56 |   40 | `		zSrc++;` |
|      56 |   41 | `		if( zSrc >= zEnd \|\| (unsigned char)zSrc[0] >= 0xc0 \|\| !SyisDigit(zSrc[0]) ){` |
|      22 |   42 | `			break;` |
|       - |   43 | `		}` |
|      15 |   44 | `		zSrc++;` |
|      15 |   45 | `		if( zSrc >= zEnd \|\| (unsigned char)zSrc[0] >= 0xc0 \|\| !SyisDigit(zSrc[0]) ){` |
|       8 |   46 | `			break;` |
|       - |   47 | `		}` |
|     ! 0 |   48 | `		zSrc++;` |
|     ! 0 |   49 | `	};` |
|     297 |   50 | `	if( zSrc < zEnd && zSrc > zCur ){` |
|      44 |   51 | `		int c = zSrc[0];` |
|      44 |   52 | `		if( c == '.' ){` |
|      29 |   53 | `			zSrc++;` |
|      29 |   54 | `			if( pReal ){` |
|      29 |   55 | `				*pReal = TRUE;` |
|      14 |   56 | `			}` |
|      29 |   57 | `			if( pzTail ){` |
|      25 |   58 | `				while( zSrc < zEnd && (unsigned char)zSrc[0] < 0xc0 && SyisDigit(zSrc[0]) ){` |
|      15 |   59 | `					zSrc++;` |
|       1 |   60 | `				}` |
|      11 |   61 | `				if( zSrc < zEnd && (zSrc[0] == 'e' \|\| zSrc[0] == 'E') ){` |
|     ! 0 |   62 | `					zSrc++;` |
|     ! 0 |   63 | `					if( zSrc < zEnd && (zSrc[0] == '+' \|\| zSrc[0] == '-') ){` |
|     ! 0 |   64 | `						zSrc++;` |
|     ! 0 |   65 | `					}` |
|     ! 0 |   66 | `					while( zSrc < zEnd && (unsigned char)zSrc[0] < 0xc0 && SyisDigit(zSrc[0]) ){` |
|     ! 0 |   67 | `						zSrc++;` |
|     ! 0 |   68 | `					}` |
|     ! 0 |   69 | `				}` |
|       6 |   70 | `			}` |
|      30 |   71 | `		}else if( c == 'e' \|\| c == 'E' ){` |
|     ! 0 |   72 | `			zSrc++;` |
|     ! 0 |   73 | `			if( pReal ){` |
|     ! 0 |   74 | `				*pReal = TRUE;` |
|     ! 0 |   75 | `			}` |
|     ! 0 |   76 | `			if( pzTail ){` |
|     ! 0 |   77 | `				if( zSrc < zEnd && (zSrc[0] == '+' \|\| zSrc[0] == '-') ){` |
|     ! 0 |   78 | `					zSrc++;` |
|     ! 0 |   79 | `				}` |
|     ! 0 |   80 | `				while( zSrc < zEnd && (unsigned char)zSrc[0] < 0xc0 && SyisDigit(zSrc[0]) ){` |
|     ! 0 |   81 | `					zSrc++;` |
|     ! 0 |   82 | `				}` |
|     ! 0 |   83 | `			}` |
|     ! 0 |   84 | `		}` |
|      21 |   85 | `	}` |
|     297 |   86 | `	if( pzTail ){` |
|       - |   87 | `		/* Point to the non numeric part */` |
|      84 |   88 | `		*pzTail = zSrc;` |
|      41 |   89 | `	}` |
|     297 |   90 | `	return zSrc > zCur ? SXRET_OK /* String prefix is numeric */ : SXERR_INVALID /* Not a digit stream */;` |
|       5 |   91 | `}` |
|       - |   92 | `#define SXINT32_MIN_STR		"2147483648"` |
|       - |   93 | `#define SXINT32_MAX_STR		"2147483647"` |
|      10 |   94 | `PH7_PRIVATE sxi32 SyStrToInt32(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)` |
|       1 |   95 | `{` |
|      11 |   96 | `	int isNeg = FALSE;` |
|       - |   97 | `	const char *zEnd;` |
|      11 |   98 | `	sxi32 nVal = 0;` |
|       - |   99 | `	sxi16 i;` |
|       - |  100 | `#if defined(UNTRUST)` |
|       - |  101 | `	if( SX_EMPTY_STR(zSrc) ){` |
|       - |  102 | `		if( pOutVal ){` |
|       - |  103 | `			*(sxi32 *)pOutVal = 0;` |
|       - |  104 | `		}` |
|       - |  105 | `		return SXERR_EMPTY;` |
|       - |  106 | `	}` |
|       - |  107 | `#endif` |
|      11 |  108 | `	zEnd = &zSrc[nLen];` |
|      11 |  109 | `	while(zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|     ! 0 |  110 | `		zSrc++;` |
|     ! 0 |  111 | `	}` |
|      11 |  112 | `	if( zSrc < zEnd && ( zSrc[0] == '-' \|\| zSrc[0] == '+' ) ){` |
|     ! 0 |  113 | `		isNeg = (zSrc[0] == '-') ? TRUE :FALSE;` |
|     ! 0 |  114 | `		zSrc++;` |
|     ! 0 |  115 | `	}` |
|       - |  116 | `	/* Skip leading zero */` |
|      11 |  117 | `	while(zSrc < zEnd && zSrc[0] == '0' ){` |
|     ! 0 |  118 | `		zSrc++;` |
|     ! 0 |  119 | `	}` |
|      11 |  120 | `	i = 10;` |
|      11 |  121 | `	if( (sxu32)(zEnd-zSrc) >= 10 ){` |
|       - |  122 | `		/* Handle overflow */` |
|     ! 0 |  123 | `		i = SyMemcmp(zSrc,(isNeg == TRUE) ? SXINT32_MIN_STR : SXINT32_MAX_STR,nLen) <= 0 ? 10 : 9;` |
|     ! 0 |  124 | `	}` |
|       5 |  125 | `	for(;;){` |
|      11 |  126 | `		if(zSrc >= zEnd \|\| !i \|\| !SyisDigit(zSrc[0])){ break; } nVal = nVal * 10 + ( zSrc[0] - '0' ) ; --i ; zSrc++;` |
|      11 |  127 | `		if(zSrc >= zEnd \|\| !i \|\| !SyisDigit(zSrc[0])){` |
|     ! 0 |  128 | `			break;` |
|       - |  129 | `		}` |
|      11 |  130 | `		nVal = nVal * 10 + ( zSrc[0] - '0' );` |
|      11 |  131 | `		--i;` |
|      11 |  132 | `		zSrc++;` |
|      11 |  133 | `		if(zSrc >= zEnd \|\| !i \|\| !SyisDigit(zSrc[0])){` |
|       3 |  134 | `			break;` |
|       - |  135 | `		}` |
|       7 |  136 | `		nVal = nVal * 10 + ( zSrc[0] - '0' );` |
|       7 |  137 | `		--i;` |
|       7 |  138 | `		zSrc++;` |
|       7 |  139 | `		if(zSrc >= zEnd \|\| !i \|\| !SyisDigit(zSrc[0])){` |
|       2 |  140 | `			break;` |
|       - |  141 | `		}` |
|       5 |  142 | `		nVal = nVal * 10 + ( zSrc[0] - '0' );` |
|       5 |  143 | `		--i;` |
|       5 |  144 | `		zSrc++;` |
|       5 |  145 | `		if(zSrc >= zEnd \|\| !i \|\| !SyisDigit(zSrc[0])){` |
|       3 |  146 | `			break;` |
|       - |  147 | `		}` |
|     ! 0 |  148 | `		nVal = nVal * 10 + ( zSrc[0] - '0' );` |
|     ! 0 |  149 | `		--i;` |
|     ! 0 |  150 | `		zSrc++;` |
|     ! 0 |  151 | `	}` |
|       - |  152 | `	/* Skip trailing spaces */` |
|      11 |  153 | `	while(zSrc < zEnd && SyisSpace(zSrc[0])){` |
|     ! 0 |  154 | `		zSrc++;` |
|     ! 0 |  155 | `	}` |
|      11 |  156 | `	if( zRest ){` |
|     ! 0 |  157 | `		*zRest = (char *)zSrc;` |
|     ! 0 |  158 | `	}` |
|      11 |  159 | `	if( pOutVal ){` |
|      11 |  160 | `		if( isNeg == TRUE && nVal != 0 ){` |
|     ! 0 |  161 | `			nVal = -nVal;` |
|     ! 0 |  162 | `		}` |
|      11 |  163 | `		*(sxi32 *)pOutVal = nVal;` |
|       5 |  164 | `	}` |
|      11 |  165 | `	return (zSrc >= zEnd) ? SXRET_OK : SXERR_SYNTAX;` |
|       1 |  166 | `}` |
| 1566428 |  167 | `PH7_PRIVATE sxi32 SyStrToInt64(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)` |
|       5 |  168 | `{` |
| 1566433 |  169 | `	int isNeg = FALSE;` |
|       - |  170 | `	const char *zEnd;` |
|       - |  171 | `	sxi64 nVal;` |
|       - |  172 | `	/* Magnitude accumulated unsigned so overflow can be detected and the result` |
|       - |  173 | `	 * saturated (PHP casts an out-of-range numeric string to PHP_INT_MAX/MIN)` |
|       - |  174 | `	 * rather than the digits being dropped. cutoff is the largest magnitude that` |
|       - |  175 | `	 * fits: PHP_INT_MAX for a positive value, \|PHP_INT_MIN\| == 2^63 for a` |
|       - |  176 | `	 * negative one. */` |
|       - |  177 | `	sxu64 uVal, cutoff;` |
| 1566433 |  178 | `	int bOverflow = FALSE;` |
|       - |  179 | `#if defined(UNTRUST)` |
|       - |  180 | `	if( SX_EMPTY_STR(zSrc) ){` |
|       - |  181 | `		if( pOutVal ){` |
|       - |  182 | `			*(sxi32 *)pOutVal = 0;` |
|       - |  183 | `		}` |
|       - |  184 | `		return SXERR_EMPTY;` |
|       - |  185 | `	}` |
|       - |  186 | `#endif` |
| 1566433 |  187 | `	zEnd = &zSrc[nLen];` |
| 1566457 |  188 | `	while(zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|      26 |  189 | `		zSrc++;` |
|       2 |  190 | `	}` |
| 1566433 |  191 | `	if( zSrc < zEnd && ( zSrc[0] == '-' \|\| zSrc[0] == '+' ) ){` |
|      21 |  192 | `		isNeg = (zSrc[0] == '-') ? TRUE :FALSE;` |
|      21 |  193 | `		zSrc++;` |
|      10 |  194 | `	}` |
|       - |  195 | `	/* Skip leading zero */` |
| 1566433 |  196 | `	while(zSrc < zEnd && zSrc[0] == '0' ){` |
|     ! 0 |  197 | `		zSrc++;` |
|     ! 0 |  198 | `	}` |
| 1566433 |  199 | `	cutoff = isNeg ? ((sxu64)SXI64_HIGH + 1) : (sxu64)SXI64_HIGH;` |
| 1566433 |  200 | `	uVal = 0;` |
| 3729589 |  201 | `	while( zSrc < zEnd && (unsigned char)zSrc[0] < 0xc0 && SyisDigit(zSrc[0]) ){` |
| 2163161 |  202 | `		int d = zSrc[0] - '0';` |
| 2163161 |  203 | `		if( uVal > cutoff / 10 \|\| (uVal == cutoff / 10 && (sxu64)d > cutoff % 10) ){` |
|      33 |  204 | `			bOverflow = TRUE;` |
|      17 |  205 | `		}else{` |
| 2163129 |  206 | `			uVal = uVal * 10 + (sxu64)d;` |
|       - |  207 | `		}` |
| 2163161 |  208 | `		zSrc++;` |
|       5 |  209 | `	}` |
| 1566433 |  210 | `	if( bOverflow ){` |
|      25 |  211 | `		uVal = cutoff;` |
|      12 |  212 | `	}` |
|       - |  213 | `	/* Skip trailing spaces */` |
| 1566457 |  214 | `	while(zSrc < zEnd && SyisSpace(zSrc[0])){` |
|      26 |  215 | `		zSrc++;` |
|       2 |  216 | `	}` |
| 1566433 |  217 | `	if( zRest ){` |
|     ! 0 |  218 | `		*zRest = (char *)zSrc;` |
|     ! 0 |  219 | `	}` |
| 1566433 |  220 | `	if( pOutVal ){` |
| 1566433 |  221 | `		if( isNeg ){` |
|       - |  222 | `			/* uVal <= 2^63; the cap value 2^63 is PHP_INT_MIN and has no positive` |
|       - |  223 | `			 * sxi64 representation, so materialize it directly to dodge UB. */` |
|      21 |  224 | `			nVal = ( uVal > (sxu64)SXI64_HIGH ) ? (-SXI64_HIGH - 1) : -(sxi64)uVal;` |
|      11 |  225 | `		}else{` |
| 1566413 |  226 | `			nVal = (sxi64)uVal;` |
|       - |  227 | `		}` |
| 1566433 |  228 | `		*(sxi64 *)pOutVal = nVal;` |
|  783214 |  229 | `	}` |
| 1566433 |  230 | `	return (zSrc >= zEnd) ? SXRET_OK : SXERR_SYNTAX;` |
|       5 |  231 | `}` |
|    1650 |  232 | `PH7_PRIVATE sxi32 SyHexToint(sxi32 c)` |
|       2 |  233 | `{` |
|    1652 |  234 | `	switch(c){` |
|     584 |  235 | `	case '0': return 0;` |
|      82 |  236 | `	case '1': return 1;` |
|      43 |  237 | `	case '2': return 2;` |
|      63 |  238 | `	case '3': return 3;` |
|      63 |  239 | `	case '4': return 4;` |
|      35 |  240 | `	case '5': return 5;` |
|      35 |  241 | `	case '6': return 6;` |
|      23 |  242 | `	case '7': return 7;` |
|      49 |  243 | `	case '8': return 8;` |
|      59 |  244 | `	case '9': return 9;` |
|      79 |  245 | `	case 'A': case 'a': return 10;` |
|      27 |  246 | `	case 'B': case 'b': return 11;` |
|      71 |  247 | `	case 'C': case 'c': return 12;` |
|      25 |  248 | `	case 'D': case 'd': return 13;` |
|      53 |  249 | `	case 'E': case 'e': return 14;` |
|     375 |  250 | `	case 'F': case 'f': return 15;` |
|       - |  251 | `	}` |
|       3 |  252 | `	return -1;` |
|     827 |  253 | `}` |
|      70 |  254 | `PH7_PRIVATE sxi32 SyHexStrToInt64(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)` |
|       1 |  255 | `{` |
|       - |  256 | `	const char *zIn,*zEnd;` |
|      71 |  257 | `	int isNeg = FALSE;` |
|      71 |  258 | `	sxi64 nVal = 0;` |
|       - |  259 | `#if defined(UNTRUST)` |
|       - |  260 | `	if( SX_EMPTY_STR(zSrc) ){` |
|       - |  261 | `		if( pOutVal ){` |
|       - |  262 | `			*(sxi32 *)pOutVal = 0;` |
|       - |  263 | `		}` |
|       - |  264 | `		return SXERR_EMPTY;` |
|       - |  265 | `	}` |
|       - |  266 | `#endif` |
|      71 |  267 | `	zEnd = &zSrc[nLen];` |
|      71 |  268 | `	while( zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|     ! 0 |  269 | `		zSrc++;` |
|     ! 0 |  270 | `	}` |
|      71 |  271 | `	if( zSrc < zEnd && ( *zSrc == '-' \|\| *zSrc == '+' ) ){` |
|     ! 0 |  272 | `		isNeg = (zSrc[0] == '-') ? TRUE :FALSE;` |
|     ! 0 |  273 | `		zSrc++;` |
|     ! 0 |  274 | `	}` |
|      71 |  275 | `	if( zSrc < &zEnd[-2] && zSrc[0] == '0' && (zSrc[1] == 'x' \|\| zSrc[1] == 'X') ){` |
|       - |  276 | `		/* Bypass hex prefix */` |
|      71 |  277 | `		zSrc += sizeof(char) * 2;` |
|      35 |  278 | `	}` |
|       - |  279 | `	/* Skip leading zero */` |
|      79 |  280 | `	while(zSrc < zEnd && zSrc[0] == '0' ){` |
|       9 |  281 | `		zSrc++;` |
|       1 |  282 | `	}` |
|      71 |  283 | `	zIn = zSrc;` |
|      71 |  284 | `	for(;;){` |
|     143 |  285 | `		if(zSrc >= zEnd \|\| !SyisHex(zSrc[0]) \|\| (int)(zSrc-zIn) > 15){` |
|      26 |  286 | `			break;` |
|       - |  287 | `		}` |
|      93 |  288 | `		nVal = nVal * 16 + SyHexToint(zSrc[0]);` |
|      93 |  289 | `		zSrc++;` |
|      93 |  290 | `		if(zSrc >= zEnd \|\| !SyisHex(zSrc[0]) \|\| (int)(zSrc-zIn) > 15){` |
|       5 |  291 | `			break;` |
|       - |  292 | `		}` |
|      85 |  293 | `		nVal = nVal * 16 + SyHexToint(zSrc[0]);` |
|      85 |  294 | `		zSrc++;` |
|      85 |  295 | `		if(zSrc >= zEnd \|\| !SyisHex(zSrc[0]) \|\| (int)(zSrc-zIn) > 15){` |
|       7 |  296 | `			break;` |
|       - |  297 | `		}` |
|      73 |  298 | `		nVal = nVal * 16 + SyHexToint(zSrc[0]);` |
|      73 |  299 | `		zSrc++;` |
|      73 |  300 | `		if(zSrc >= zEnd \|\| !SyisHex(zSrc[0]) \|\| (int)(zSrc-zIn) > 15){` |
|     ! 0 |  301 | `			break;` |
|       - |  302 | `		}` |
|      73 |  303 | `		nVal = nVal * 16 + SyHexToint(zSrc[0]);` |
|      73 |  304 | `		zSrc++;` |
|       1 |  305 | `	}` |
|      71 |  306 | `	while( zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|     ! 0 |  307 | `		zSrc++;` |
|     ! 0 |  308 | `	}` |
|      71 |  309 | `	if( zRest ){` |
|     ! 0 |  310 | `		*zRest = zSrc;` |
|     ! 0 |  311 | `	}` |
|      71 |  312 | `	if( pOutVal ){` |
|      71 |  313 | `		if( isNeg == TRUE && nVal != 0 ){` |
|     ! 0 |  314 | `			nVal = -nVal;` |
|     ! 0 |  315 | `		}` |
|      71 |  316 | `		*(sxi64 *)pOutVal = nVal;` |
|      35 |  317 | `	}` |
|      71 |  318 | `	return zSrc >= zEnd ? SXRET_OK : SXERR_SYNTAX;` |
|       1 |  319 | `}` |
|     108 |  320 | `PH7_PRIVATE sxi32 SyOctalStrToInt64(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)` |
|       1 |  321 | `{` |
|       - |  322 | `	const char *zIn,*zEnd;` |
|     109 |  323 | `	int isNeg = FALSE;` |
|     109 |  324 | `	sxi64 nVal = 0;` |
|       - |  325 | `	int c;` |
|       - |  326 | `#if defined(UNTRUST)` |
|       - |  327 | `	if( SX_EMPTY_STR(zSrc) ){` |
|       - |  328 | `		if( pOutVal ){` |
|       - |  329 | `			*(sxi32 *)pOutVal = 0;` |
|       - |  330 | `		}` |
|       - |  331 | `		return SXERR_EMPTY;` |
|       - |  332 | `	}` |
|       - |  333 | `#endif` |
|     109 |  334 | `	zEnd = &zSrc[nLen];` |
|     109 |  335 | `	while(zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|     ! 0 |  336 | `		zSrc++;` |
|     ! 0 |  337 | `	}` |
|     109 |  338 | `	if( zSrc < zEnd && ( zSrc[0] == '-' \|\| zSrc[0] == '+' ) ){` |
|     ! 0 |  339 | `		isNeg = (zSrc[0] == '-') ? TRUE :FALSE;` |
|     ! 0 |  340 | `		zSrc++;` |
|     ! 0 |  341 | `	}` |
|       - |  342 | `	/* Skip leading zero */` |
|     247 |  343 | `	while(zSrc < zEnd && zSrc[0] == '0' ){` |
|     139 |  344 | `		zSrc++;` |
|       1 |  345 | `	}` |
|     109 |  346 | `	zIn = zSrc;` |
|      59 |  347 | `	for(;;){` |
|     119 |  348 | `		if(zSrc >= zEnd \|\| !SyisDigit(zSrc[0])){ break; } if( (c=zSrc[0]-'0') > 7 \|\| (int)(zSrc-zIn) > 20){ break;} nVal = nVal * 8 +  c; zSrc++;` |
|      93 |  349 | `		if(zSrc >= zEnd \|\| !SyisDigit(zSrc[0])){ break; } if( (c=zSrc[0]-'0') > 7 \|\| (int)(zSrc-zIn) > 20){ break;} nVal = nVal * 8 +  c; zSrc++;` |
|      63 |  350 | `		if(zSrc >= zEnd \|\| !SyisDigit(zSrc[0])){ break; } if( (c=zSrc[0]-'0') > 7 \|\| (int)(zSrc-zIn) > 20){ break;} nVal = nVal * 8 +  c; zSrc++;` |
|      49 |  351 | `		if(zSrc >= zEnd \|\| !SyisDigit(zSrc[0])){ break; } if( (c=zSrc[0]-'0') > 7 \|\| (int)(zSrc-zIn) > 20){ break;} nVal = nVal * 8 +  c; zSrc++;` |
|       1 |  352 | `	}` |
|       - |  353 | `	/* Skip trailing spaces */` |
|     109 |  354 | `	while(zSrc < zEnd && SyisSpace(zSrc[0])){` |
|     ! 0 |  355 | `		zSrc++;` |
|     ! 0 |  356 | `	}` |
|     109 |  357 | `	if( zRest ){` |
|     ! 0 |  358 | `		*zRest = zSrc;` |
|     ! 0 |  359 | `	}` |
|     109 |  360 | `	if( pOutVal ){` |
|     109 |  361 | `		if( isNeg == TRUE && nVal != 0 ){` |
|     ! 0 |  362 | `			nVal = -nVal;` |
|     ! 0 |  363 | `		}` |
|     109 |  364 | `		*(sxi64 *)pOutVal = nVal;` |
|      54 |  365 | `	}` |
|     109 |  366 | `	return (zSrc >= zEnd) ? SXRET_OK : SXERR_SYNTAX;` |
|       1 |  367 | `}` |
|     278 |  368 | `PH7_PRIVATE sxi32 SyBinaryStrToInt64(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)` |
|       1 |  369 | `{` |
|       - |  370 | `	const char *zIn,*zEnd;` |
|     279 |  371 | `	int isNeg = FALSE;` |
|     279 |  372 | `	sxi64 nVal = 0;` |
|       - |  373 | `	int c;` |
|       - |  374 | `#if defined(UNTRUST)` |
|       - |  375 | `	if( SX_EMPTY_STR(zSrc) ){` |
|       - |  376 | `		if( pOutVal ){` |
|       - |  377 | `			*(sxi32 *)pOutVal = 0;` |
|       - |  378 | `		}` |
|       - |  379 | `		return SXERR_EMPTY;` |
|       - |  380 | `	}` |
|       - |  381 | `#endif` |
|     279 |  382 | `	zEnd = &zSrc[nLen];` |
|     279 |  383 | `	while(zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|     ! 0 |  384 | `		zSrc++;` |
|     ! 0 |  385 | `	}` |
|     279 |  386 | `	if( zSrc < zEnd && ( zSrc[0] == '-' \|\| zSrc[0] == '+' ) ){` |
|     ! 0 |  387 | `		isNeg = (zSrc[0] == '-') ? TRUE :FALSE;` |
|     ! 0 |  388 | `		zSrc++;` |
|     ! 0 |  389 | `	}` |
|     279 |  390 | `	if( zSrc < &zEnd[-2] && zSrc[0] == '0' && (zSrc[1] == 'b' \|\| zSrc[1] == 'B') ){` |
|       - |  391 | `		/* Bypass binary prefix */` |
|     279 |  392 | `		zSrc += sizeof(char) * 2;` |
|     139 |  393 | `	}` |
|       - |  394 | `	/* Skip leading zero */` |
|     327 |  395 | `	while(zSrc < zEnd && zSrc[0] == '0' ){` |
|      49 |  396 | `		zSrc++;` |
|       1 |  397 | `	}` |
|     279 |  398 | `	zIn = zSrc;` |
|     311 |  399 | `	for(;;){` |
|     623 |  400 | `		if(zSrc >= zEnd \|\| (zSrc[0] != '1' && zSrc[0] != '0') \|\| (int)(zSrc-zIn) > 62){ break; } c = zSrc[0] - '0'; nVal = (nVal << 1) + c; zSrc++;` |
|     455 |  401 | `		if(zSrc >= zEnd \|\| (zSrc[0] != '1' && zSrc[0] != '0') \|\| (int)(zSrc-zIn) > 62){ break; } c = zSrc[0] - '0'; nVal = (nVal << 1) + c; zSrc++;` |
|     423 |  402 | `		if(zSrc >= zEnd \|\| (zSrc[0] != '1' && zSrc[0] != '0') \|\| (int)(zSrc-zIn) > 62){ break; } c = zSrc[0] - '0'; nVal = (nVal << 1) + c; zSrc++;` |
|     381 |  403 | `		if(zSrc >= zEnd \|\| (zSrc[0] != '1' && zSrc[0] != '0') \|\| (int)(zSrc-zIn) > 62){ break; } c = zSrc[0] - '0'; nVal = (nVal << 1) + c; zSrc++;` |
|       1 |  404 | `	}` |
|       - |  405 | `	/* Skip trailing spaces */` |
|     279 |  406 | `	while(zSrc < zEnd && SyisSpace(zSrc[0])){` |
|     ! 0 |  407 | `		zSrc++;` |
|     ! 0 |  408 | `	}` |
|     279 |  409 | `	if( zRest ){` |
|     ! 0 |  410 | `		*zRest = zSrc;` |
|     ! 0 |  411 | `	}` |
|     279 |  412 | `	if( pOutVal ){` |
|     279 |  413 | `		if( isNeg == TRUE && nVal != 0 ){` |
|     ! 0 |  414 | `			nVal = -nVal;` |
|     ! 0 |  415 | `		}` |
|     279 |  416 | `		*(sxi64 *)pOutVal = nVal;` |
|     139 |  417 | `	}` |
|     279 |  418 | `	return (zSrc >= zEnd) ? SXRET_OK : SXERR_SYNTAX;` |
|       1 |  419 | `}` |
|    1026 |  420 | `PH7_PRIVATE sxi32 SyStrToReal(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)` |
|       3 |  421 | `{` |
|       - |  422 | `	/* Correctly-rounded conversion via libc strtod (the byte-exact-floats` |
|       - |  423 | `	 * rule): the old hand-rolled accumulator kept only 15 significant` |
|       - |  424 | `	 * digits, clamped exponents to +/-30x (so 1e400 silently became 1e308` |
|       - |  425 | `	 * and 5e-324 became 5e-307) and drifted the low mantissa bits, making` |
|       - |  426 | `	 * float literals and string->float casts differ from php. The accepted` |
|       - |  427 | `	 * numeric-prefix grammar is kept from the old parser, including the ','` |
|       - |  428 | `	 * decimal separator: [ws][sign]D*[(.\|,)D*][(e\|E)[sign]D+][ws]. The` |
|       - |  429 | `	 * prefix is copied (',' -> '.') into a stack buffer for strtod — or a` |
|       - |  430 | `	 * heap copy for the rare number longer than the buffer (e.g. hundreds of` |
|       - |  431 | `	 * leading fractional zeros before the significant digits), falling back` |
|       - |  432 | `	 * to a truncated-mantissa-plus-exponent copy only if that allocation` |
|       - |  433 | `	 * fails. Everything is always consumed from the input. */` |
|       - |  434 | `	char zBuf[512];` |
|    1029 |  435 | `	const char *zEnd = &zSrc[nLen];` |
|       - |  436 | `	const char *zNum;` |
|    1029 |  437 | `	const char *zExpStart = 0;` |
|    1029 |  438 | `	sxreal Val = 0.0;` |
|    1029 |  439 | `	sxu32 nCopy = 0;` |
|    1029 |  440 | `	int bDigit = 0;` |
|       - |  441 | `#ifdef UNTRUST` |
|       - |  442 | `	if( SX_EMPTY_STR(zSrc)  ){` |
|       - |  443 | `		if( pOutVal ){` |
|       - |  444 | `			*(sxreal *)pOutVal = 0.0;` |
|       - |  445 | `		}` |
|       - |  446 | `		return SXERR_EMPTY;` |
|       - |  447 | `	}` |
|       - |  448 | `#endif` |
|       - |  449 | `	/* Skip leading spaces */` |
|    1033 |  450 | `	while( zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|       5 |  451 | `		zSrc++;` |
|       1 |  452 | `	}` |
|    1029 |  453 | `	zNum = zSrc;` |
|       - |  454 | `	/* Sign (if exists) */` |
|    1029 |  455 | `	if( zSrc < zEnd && (zSrc[0] == '-' \|\| zSrc[0] == '+' ) ){` |
|      12 |  456 | `		zSrc++;` |
|       5 |  457 | `	}` |
|       - |  458 | `	/* Integer part */` |
|    3059 |  459 | `	while( zSrc < zEnd && SyisDigit(zSrc[0]) ){` |
|    2033 |  460 | `		bDigit = 1;` |
|    2033 |  461 | `		zSrc++;` |
|       3 |  462 | `	}` |
|       - |  463 | `	/* Fractional part */` |
|    1029 |  464 | `	if( zSrc < zEnd && ( zSrc[0] == '.' \|\| zSrc[0] == ',' ) ){` |
|     885 |  465 | `		zSrc++;` |
|    4335 |  466 | `		while( zSrc < zEnd && SyisDigit(zSrc[0]) ){` |
|    3453 |  467 | `			bDigit = 1;` |
|    3453 |  468 | `			zSrc++;` |
|       3 |  469 | `		}` |
|     441 |  470 | `	}` |
|       - |  471 | `	/* Exponent — consumed only when it carries at least one digit, like` |
|       - |  472 | `	 * strtod, so "1e+x" leaves the "e+" unconsumed. */` |
|    1029 |  473 | `	if( bDigit && zSrc < zEnd && ( zSrc[0] == 'e' \|\| zSrc[0] == 'E' ) ){` |
|     182 |  474 | `		const char *zExp = &zSrc[1];` |
|     182 |  475 | `		if( zExp < zEnd && (zExp[0] == '-' \|\| zExp[0] == '+') ){` |
|      65 |  476 | `			zExp++;` |
|      32 |  477 | `		}` |
|     182 |  478 | `		if( zExp < zEnd && SyisDigit(zExp[0]) ){` |
|     182 |  479 | `			zExpStart = zSrc;` |
|     182 |  480 | `			zSrc = zExp;` |
|     554 |  481 | `			while( zSrc < zEnd && SyisDigit(zSrc[0]) ){` |
|     374 |  482 | `				zSrc++;` |
|       2 |  483 | `			}` |
|      90 |  484 | `		}` |
|      90 |  485 | `	}` |
|    1029 |  486 | `	if( bDigit ){` |
|    1029 |  487 | `		sxu32 i, nSpan = (sxu32)((zExpStart ? zExpStart : zSrc) - zNum);` |
|    1029 |  488 | `		sxu32 nExp = zExpStart ? (sxu32)(zSrc - zExpStart) : 0;` |
|    1029 |  489 | `		char *zDup = zBuf;` |
|    1029 |  490 | `		sxu32 nDup = sizeof(zBuf);` |
|    1029 |  491 | `		if( nSpan + nExp >= sizeof(zBuf) ){` |
|       3 |  492 | `			char *zHeap = (char *)malloc(nSpan + nExp + 1);` |
|       3 |  493 | `			if( zHeap ){` |
|       3 |  494 | `				zDup = zHeap;` |
|       3 |  495 | `				nDup = nSpan + nExp + 1;` |
|       1 |  496 | `			}` |
|       1 |  497 | `		}` |
|       - |  498 | `		{` |
|    1029 |  499 | `			sxu32 nMantMax = nDup - 1 - (nExp < nDup - 1 ? nExp : 0);` |
|    7401 |  500 | `			for( i = 0 ; i < nSpan && nCopy < nMantMax ; i++ ){` |
|    6375 |  501 | `				zDup[nCopy++] = (zNum[i] == ',') ? '.' : zNum[i];` |
|    3189 |  502 | `			}` |
|       - |  503 | `			/* The exponent rides behind even a truncated mantissa: dropping` |
|       - |  504 | `			 * it would collapse "0.<hundreds of zeros>1e300" to 0.0. */` |
|    1645 |  505 | `			for( i = 0 ; i < nExp && nCopy < nDup - 1 ; i++ ){` |
|     618 |  506 | `				zDup[nCopy++] = zExpStart[i];` |
|     310 |  507 | `			}` |
|       - |  508 | `		}` |
|    1029 |  509 | `		zDup[nCopy] = 0;` |
|    1029 |  510 | `		Val = (sxreal)strtod(zDup,0);` |
|    1029 |  511 | `		if( zDup != zBuf ){` |
|       3 |  512 | `			free(zDup);` |
|       1 |  513 | `		}` |
|     513 |  514 | `	}` |
|       - |  515 | `	/* Jump trailing spaces */` |
|    1029 |  516 | `	while( zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|     ! 0 |  517 | `		zSrc++;` |
|     ! 0 |  518 | `	}` |
|    1029 |  519 | `	if( zRest ){` |
|     ! 0 |  520 | `		*zRest = zSrc;` |
|     ! 0 |  521 | `	}` |
|    1029 |  522 | `	if( pOutVal ){` |
|    1029 |  523 | `		*(sxreal *)pOutVal = Val;` |
|     513 |  524 | `	}` |
|    1029 |  525 | `	return zSrc >= zEnd ? SXRET_OK : SXERR_SYNTAX;` |
|       3 |  526 | `}` |
|       - |  527 |  |
