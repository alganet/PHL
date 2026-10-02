# src/sx/sxutils.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 335/398 lines (84.17%)

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
|     442 |   12 | `PH7_PRIVATE sxi32 SyStrIsNumeric(const char *zSrc,sxu32 nLen,sxu8 *pReal,const char  **pzTail)` |
|       5 |   13 | `{` |
|       - |   14 | `	const char *zCur,*zEnd;` |
|       - |   15 | `#ifdef UNTRUST` |
|       - |   16 | `	if( SX_EMPTY_STR(zSrc) ){` |
|       - |   17 | `		return SXERR_EMPTY;` |
|       - |   18 | `	}` |
|       - |   19 | `#endif` |
|     447 |   20 | `	zEnd = &zSrc[nLen];` |
|       - |   21 | `	/* Jump leading white spaces */` |
|     453 |   22 | `	while( zSrc < zEnd && (unsigned char)zSrc[0] < 0xc0  && SyisSpace(zSrc[0]) ){` |
|       7 |   23 | `		zSrc++;` |
|       1 |   24 | `	}` |
|     447 |   25 | `	if( zSrc < zEnd && (zSrc[0] == '+' \|\| zSrc[0] == '-') ){` |
|      38 |   26 | `		zSrc++;` |
|      18 |   27 | `	}` |
|     447 |   28 | `	zCur = zSrc;` |
|     447 |   29 | `	if( pReal ){` |
|     441 |   30 | `		*pReal = FALSE;` |
|     218 |   31 | `	}` |
|     224 |   32 | `	for(;;){` |
|     453 |   33 | `		if( zSrc >= zEnd \|\| (unsigned char)zSrc[0] >= 0xc0 \|\| !SyisDigit(zSrc[0]) ){` |
|      38 |   34 | `			break;` |
|       - |   35 | `		}` |
|     385 |   36 | `		zSrc++;` |
|     385 |   37 | `		if( zSrc >= zEnd \|\| (unsigned char)zSrc[0] >= 0xc0 \|\| !SyisDigit(zSrc[0]) ){` |
|     181 |   38 | `			break;` |
|       - |   39 | `		}` |
|      31 |   40 | `		zSrc++;` |
|      31 |   41 | `		if( zSrc >= zEnd \|\| (unsigned char)zSrc[0] >= 0xc0 \|\| !SyisDigit(zSrc[0]) ){` |
|       5 |   42 | `			break;` |
|       - |   43 | `		}` |
|      24 |   44 | `		zSrc++;` |
|      24 |   45 | `		if( zSrc >= zEnd \|\| (unsigned char)zSrc[0] >= 0xc0 \|\| !SyisDigit(zSrc[0]) ){` |
|       9 |   46 | `			break;` |
|       - |   47 | `		}` |
|       8 |   48 | `		zSrc++;` |
|       2 |   49 | `	};` |
|     447 |   50 | `	if( zSrc < zEnd && zSrc > zCur ){` |
|      18 |   51 | `		int c = zSrc[0];` |
|      18 |   52 | `		if( c == '.' ){` |
|      16 |   53 | `			zSrc++;` |
|      16 |   54 | `			if( pReal ){` |
|      16 |   55 | `				*pReal = TRUE;` |
|       7 |   56 | `			}` |
|      16 |   57 | `			if( pzTail ){` |
|      17 |   58 | `				while( zSrc < zEnd && (unsigned char)zSrc[0] < 0xc0 && SyisDigit(zSrc[0]) ){` |
|      11 |   59 | `					zSrc++;` |
|       1 |   60 | `				}` |
|       7 |   61 | `				if( zSrc < zEnd && (zSrc[0] == 'e' \|\| zSrc[0] == 'E') ){` |
|     ! 0 |   62 | `					zSrc++;` |
|     ! 0 |   63 | `					if( zSrc < zEnd && (zSrc[0] == '+' \|\| zSrc[0] == '-') ){` |
|     ! 0 |   64 | `						zSrc++;` |
|     ! 0 |   65 | `					}` |
|     ! 0 |   66 | `					while( zSrc < zEnd && (unsigned char)zSrc[0] < 0xc0 && SyisDigit(zSrc[0]) ){` |
|     ! 0 |   67 | `						zSrc++;` |
|     ! 0 |   68 | `					}` |
|     ! 0 |   69 | `				}` |
|       5 |   70 | `			}` |
|      10 |   71 | `		}else if( c == 'e' \|\| c == 'E' ){` |
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
|       8 |   85 | `	}` |
|     447 |   86 | `	if( pzTail ){` |
|       - |   87 | `		/* Point to the non numeric part */` |
|      37 |   88 | `		*pzTail = zSrc;` |
|      17 |   89 | `	}` |
|     447 |   90 | `	return zSrc > zCur ? SXRET_OK /* String prefix is numeric */ : SXERR_INVALID /* Not a digit stream */;` |
|       5 |   91 | `}` |
|       - |   92 | `#define SXINT32_MIN_STR		"2147483648"` |
|       - |   93 | `#define SXINT32_MAX_STR		"2147483647"` |
|    2413 |   94 | `PH7_PRIVATE sxi32 SyStrToInt32(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)` |
|       5 |   95 | `{` |
|    2418 |   96 | `	int isNeg = FALSE;` |
|       - |   97 | `	const char *zEnd;` |
|    2418 |   98 | `	sxi32 nVal = 0;` |
|       - |   99 | `	sxi16 i;` |
|       - |  100 | `#if defined(UNTRUST)` |
|       - |  101 | `	if( SX_EMPTY_STR(zSrc) ){` |
|       - |  102 | `		if( pOutVal ){` |
|       - |  103 | `			*(sxi32 *)pOutVal = 0;` |
|       - |  104 | `		}` |
|       - |  105 | `		return SXERR_EMPTY;` |
|       - |  106 | `	}` |
|       - |  107 | `#endif` |
|    2418 |  108 | `	zEnd = &zSrc[nLen];` |
|    2426 |  109 | `	while(zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|       8 |  110 | `		zSrc++;` |
|     ! 0 |  111 | `	}` |
|    2418 |  112 | `	if( zSrc < zEnd && ( zSrc[0] == '-' \|\| zSrc[0] == '+' ) ){` |
|      17 |  113 | `		isNeg = (zSrc[0] == '-') ? TRUE :FALSE;` |
|      17 |  114 | `		zSrc++;` |
|       7 |  115 | `	}` |
|       - |  116 | `	/* Skip leading zero */` |
|    3015 |  117 | `	while(zSrc < zEnd && zSrc[0] == '0' ){` |
|     601 |  118 | `		zSrc++;` |
|       4 |  119 | `	}` |
|    2418 |  120 | `	i = 10;` |
|    2418 |  121 | `	if( (sxu32)(zEnd-zSrc) >= 10 ){` |
|       - |  122 | `		/* Handle overflow */` |
|      28 |  123 | `		i = SyMemcmp(zSrc,(isNeg == TRUE) ? SXINT32_MIN_STR : SXINT32_MAX_STR,nLen) <= 0 ? 10 : 9;` |
|      14 |  124 | `	}` |
|    1540 |  125 | `	for(;;){` |
|    3104 |  126 | `		if(zSrc >= zEnd \|\| !i \|\| !SyisDigit(zSrc[0])){ break; } nVal = nVal * 10 + ( zSrc[0] - '0' ) ; --i ; zSrc++;` |
|    1815 |  127 | `		if(zSrc >= zEnd \|\| !i \|\| !SyisDigit(zSrc[0])){` |
|     383 |  128 | `			break;` |
|       - |  129 | `		}` |
|    1043 |  130 | `		nVal = nVal * 10 + ( zSrc[0] - '0' );` |
|    1043 |  131 | `		--i;` |
|    1043 |  132 | `		zSrc++;` |
|    1043 |  133 | `		if(zSrc >= zEnd \|\| !i \|\| !SyisDigit(zSrc[0])){` |
|     ! 0 |  134 | `			break;` |
|       - |  135 | `		}` |
|    1043 |  136 | `		nVal = nVal * 10 + ( zSrc[0] - '0' );` |
|    1043 |  137 | `		--i;` |
|    1043 |  138 | `		zSrc++;` |
|    1043 |  139 | `		if(zSrc >= zEnd \|\| !i \|\| !SyisDigit(zSrc[0])){` |
|     149 |  140 | `			break;` |
|       - |  141 | `		}` |
|     745 |  142 | `		nVal = nVal * 10 + ( zSrc[0] - '0' );` |
|     745 |  143 | `		--i;` |
|     745 |  144 | `		zSrc++;` |
|     745 |  145 | `		if(zSrc >= zEnd \|\| !i \|\| !SyisDigit(zSrc[0])){` |
|      31 |  146 | `			break;` |
|       - |  147 | `		}` |
|     689 |  148 | `		nVal = nVal * 10 + ( zSrc[0] - '0' );` |
|     689 |  149 | `		--i;` |
|     689 |  150 | `		zSrc++;` |
|       3 |  151 | `	}` |
|       - |  152 | `	/* Skip trailing spaces */` |
|    2718 |  153 | `	while(zSrc < zEnd && SyisSpace(zSrc[0])){` |
|     300 |  154 | `		zSrc++;` |
|     ! 0 |  155 | `	}` |
|    2418 |  156 | `	if( zRest ){` |
|     ! 0 |  157 | `		*zRest = (char *)zSrc;` |
|     ! 0 |  158 | `	}` |
|    2418 |  159 | `	if( pOutVal ){` |
|    2388 |  160 | `		if( isNeg == TRUE && nVal != 0 ){` |
|      17 |  161 | `			nVal = -nVal;` |
|       7 |  162 | `		}` |
|    2388 |  163 | `		*(sxi32 *)pOutVal = nVal;` |
|    1182 |  164 | `	}` |
|    2418 |  165 | `	return (zSrc >= zEnd) ? SXRET_OK : SXERR_SYNTAX;` |
|       5 |  166 | `}` |
|  592799 |  167 | `PH7_PRIVATE sxi32 SyStrToInt64(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)` |
|       5 |  168 | `{` |
|  592804 |  169 | `	return SyStrToInt64Ex(zSrc,nLen,pOutVal,zRest,0);` |
|       5 |  170 | `}` |
|       - |  171 | `/*` |
|       - |  172 | ` * SyStrToInt64 plus the one fact its saturating reader threw away: whether the` |
|       - |  173 | ` * digit run ran PAST the int64 range. *pOverflow comes back 1 for the positive` |
|       - |  174 | ` * side, -1 for the negative one and 0 when the value fits. The integer handed` |
|       - |  175 | ` * back is still the saturated one, which is what php's (int) cast wants -- but a` |
|       - |  176 | ` * caller converting a numeric STRING to a NUMBER needs to know, because php's` |
|       - |  177 | ` * answer there is a float, not PHP_INT_MAX.` |
|       - |  178 | ` */` |
| 1771509 |  179 | `PH7_PRIVATE sxi32 SyStrToInt64Ex(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest,int *pOverflow)` |
|       5 |  180 | `{` |
| 1771514 |  181 | `	int isNeg = FALSE;` |
|       - |  182 | `	const char *zEnd;` |
|       - |  183 | `	sxi64 nVal;` |
|       - |  184 | `	/* Magnitude accumulated unsigned so overflow can be detected and the result` |
|       - |  185 | `	 * saturated (PHP casts an out-of-range numeric string to PHP_INT_MAX/MIN)` |
|       - |  186 | `	 * rather than the digits being dropped. cutoff is the largest magnitude that` |
|       - |  187 | `	 * fits: PHP_INT_MAX for a positive value, \|PHP_INT_MIN\| == 2^63 for a` |
|       - |  188 | `	 * negative one. */` |
|       - |  189 | `	sxu64 uVal, cutoff;` |
| 1771514 |  190 | `	int bOverflow = FALSE;` |
| 1771514 |  191 | `	if( pOverflow ){` |
|    2465 |  192 | `		*pOverflow = 0;` |
|    1222 |  193 | `	}` |
|       - |  194 | `#if defined(UNTRUST)` |
|       - |  195 | `	if( SX_EMPTY_STR(zSrc) ){` |
|       - |  196 | `		if( pOutVal ){` |
|       - |  197 | `			*(sxi32 *)pOutVal = 0;` |
|       - |  198 | `		}` |
|       - |  199 | `		return SXERR_EMPTY;` |
|       - |  200 | `	}` |
|       - |  201 | `#endif` |
| 1771514 |  202 | `	zEnd = &zSrc[nLen];` |
| 1771600 |  203 | `	while(zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|      89 |  204 | `		zSrc++;` |
|       3 |  205 | `	}` |
| 1771514 |  206 | `	if( zSrc < zEnd && ( zSrc[0] == '-' \|\| zSrc[0] == '+' ) ){` |
|     262 |  207 | `		isNeg = (zSrc[0] == '-') ? TRUE :FALSE;` |
|     262 |  208 | `		zSrc++;` |
|     129 |  209 | `	}` |
|       - |  210 | `	/* Skip leading zero */` |
| 1774376 |  211 | `	while(zSrc < zEnd && zSrc[0] == '0' ){` |
|    2867 |  212 | `		zSrc++;` |
|       5 |  213 | `	}` |
| 1771514 |  214 | `	cutoff = isNeg ? ((sxu64)SXI64_HIGH + 1) : (sxu64)SXI64_HIGH;` |
| 1771514 |  215 | `	uVal = 0;` |
| 5440169 |  216 | `	while( zSrc < zEnd && (unsigned char)zSrc[0] < 0xc0 && SyisDigit(zSrc[0]) ){` |
| 3668660 |  217 | `		int d = zSrc[0] - '0';` |
| 3668660 |  218 | `		if( uVal > cutoff / 10 \|\| (uVal == cutoff / 10 && (sxu64)d > cutoff % 10) ){` |
|   12909 |  219 | `			bOverflow = TRUE;` |
|    6456 |  220 | `		}else{` |
| 3655754 |  221 | `			uVal = uVal * 10 + (sxu64)d;` |
|       - |  222 | `		}` |
| 3668660 |  223 | `		zSrc++;` |
|       5 |  224 | `	}` |
| 1771514 |  225 | `	if( bOverflow ){` |
|     599 |  226 | `		uVal = cutoff;` |
|     599 |  227 | `		if( pOverflow ){` |
|     564 |  228 | `			*pOverflow = isNeg ? -1 : 1;` |
|     281 |  229 | `		}` |
|     298 |  230 | `	}` |
|       - |  231 | `	/* Skip trailing spaces */` |
| 1771588 |  232 | `	while(zSrc < zEnd && SyisSpace(zSrc[0])){` |
|      77 |  233 | `		zSrc++;` |
|       3 |  234 | `	}` |
| 1771514 |  235 | `	if( zRest ){` |
|     ! 0 |  236 | `		*zRest = (char *)zSrc;` |
|     ! 0 |  237 | `	}` |
| 1771514 |  238 | `	if( pOutVal ){` |
| 1771514 |  239 | `		if( isNeg ){` |
|       - |  240 | `			/* uVal <= 2^63; the cap value 2^63 is PHP_INT_MIN and has no positive` |
|       - |  241 | `			 * sxi64 representation, so materialize it directly to dodge UB. */` |
|     236 |  242 | `			nVal = ( uVal > (sxu64)SXI64_HIGH ) ? (-SXI64_HIGH - 1) : -(sxi64)uVal;` |
|     120 |  243 | `		}else{` |
| 1771282 |  244 | `			nVal = (sxi64)uVal;` |
|       - |  245 | `		}` |
| 1771514 |  246 | `		*(sxi64 *)pOutVal = nVal;` |
|  885397 |  247 | `	}` |
| 1771514 |  248 | `	return (zSrc >= zEnd) ? SXRET_OK : SXERR_SYNTAX;` |
|       5 |  249 | `}` |
|   12838 |  250 | `PH7_PRIVATE sxi32 SyHexToint(sxi32 c)` |
|       5 |  251 | `{` |
|   12843 |  252 | `	switch(c){` |
|    3402 |  253 | `	case '0': return 0;` |
|    1086 |  254 | `	case '1': return 1;` |
|     740 |  255 | `	case '2': return 2;` |
|     513 |  256 | `	case '3': return 3;` |
|     361 |  257 | `	case '4': return 4;` |
|     275 |  258 | `	case '5': return 5;` |
|     228 |  259 | `	case '6': return 6;` |
|     240 |  260 | `	case '7': return 7;` |
|     644 |  261 | `	case '8': return 8;` |
|     632 |  262 | `	case '9': return 9;` |
|     606 |  263 | `	case 'A': case 'a': return 10;` |
|     434 |  264 | `	case 'B': case 'b': return 11;` |
|     643 |  265 | `	case 'C': case 'c': return 12;` |
|     353 |  266 | `	case 'D': case 'd': return 13;` |
|     633 |  267 | `	case 'E': case 'e': return 14;` |
|    2098 |  268 | `	case 'F': case 'f': return 15;` |
|       - |  269 | `	}` |
|      25 |  270 | `	return -1;` |
|    6155 |  271 | `}` |
|     343 |  272 | `PH7_PRIVATE sxi32 SyHexStrToInt64(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)` |
|       5 |  273 | `{` |
|       - |  274 | `	const char *zIn,*zEnd;` |
|     348 |  275 | `	int isNeg = FALSE;` |
|     348 |  276 | `	sxi64 nVal = 0;` |
|       - |  277 | `#if defined(UNTRUST)` |
|       - |  278 | `	if( SX_EMPTY_STR(zSrc) ){` |
|       - |  279 | `		if( pOutVal ){` |
|       - |  280 | `			*(sxi32 *)pOutVal = 0;` |
|       - |  281 | `		}` |
|       - |  282 | `		return SXERR_EMPTY;` |
|       - |  283 | `	}` |
|       - |  284 | `#endif` |
|     348 |  285 | `	zEnd = &zSrc[nLen];` |
|     348 |  286 | `	while( zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|     ! 0 |  287 | `		zSrc++;` |
|     ! 0 |  288 | `	}` |
|     348 |  289 | `	if( zSrc < zEnd && ( *zSrc == '-' \|\| *zSrc == '+' ) ){` |
|     ! 0 |  290 | `		isNeg = (zSrc[0] == '-') ? TRUE :FALSE;` |
|     ! 0 |  291 | `		zSrc++;` |
|     ! 0 |  292 | `	}` |
|     348 |  293 | `	if( zSrc < &zEnd[-2] && zSrc[0] == '0' && (zSrc[1] == 'x' \|\| zSrc[1] == 'X') ){` |
|       - |  294 | `		/* Bypass hex prefix */` |
|     348 |  295 | `		zSrc += sizeof(char) * 2;` |
|     171 |  296 | `	}` |
|       - |  297 | `	/* Skip leading zero */` |
|     408 |  298 | `	while(zSrc < zEnd && zSrc[0] == '0' ){` |
|      63 |  299 | `		zSrc++;` |
|       3 |  300 | `	}` |
|     348 |  301 | `	zIn = zSrc;` |
|     270 |  302 | `	for(;;){` |
|     548 |  303 | `		if(zSrc >= zEnd \|\| !SyisHex(zSrc[0]) \|\| (int)(zSrc-zIn) > 15){` |
|      67 |  304 | `			break;` |
|       - |  305 | `		}` |
|     421 |  306 | `		nVal = nVal * 16 + SyHexToint(zSrc[0]);` |
|     421 |  307 | `		zSrc++;` |
|     421 |  308 | `		if(zSrc >= zEnd \|\| !SyisHex(zSrc[0]) \|\| (int)(zSrc-zIn) > 15){` |
|      14 |  309 | `			break;` |
|       - |  310 | `		}` |
|     397 |  311 | `		nVal = nVal * 16 + SyHexToint(zSrc[0]);` |
|     397 |  312 | `		zSrc++;` |
|     397 |  313 | `		if(zSrc >= zEnd \|\| !SyisHex(zSrc[0]) \|\| (int)(zSrc-zIn) > 15){` |
|      87 |  314 | `			break;` |
|       - |  315 | `		}` |
|     233 |  316 | `		nVal = nVal * 16 + SyHexToint(zSrc[0]);` |
|     233 |  317 | `		zSrc++;` |
|     233 |  318 | `		if(zSrc >= zEnd \|\| !SyisHex(zSrc[0]) \|\| (int)(zSrc-zIn) > 15){` |
|      15 |  319 | `			break;` |
|       - |  320 | `		}` |
|     205 |  321 | `		nVal = nVal * 16 + SyHexToint(zSrc[0]);` |
|     205 |  322 | `		zSrc++;` |
|       5 |  323 | `	}` |
|     348 |  324 | `	while( zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|     ! 0 |  325 | `		zSrc++;` |
|     ! 0 |  326 | `	}` |
|     348 |  327 | `	if( zRest ){` |
|     ! 0 |  328 | `		*zRest = zSrc;` |
|     ! 0 |  329 | `	}` |
|     348 |  330 | `	if( pOutVal ){` |
|     348 |  331 | `		if( isNeg == TRUE && nVal != 0 ){` |
|     ! 0 |  332 | `			nVal = -nVal;` |
|     ! 0 |  333 | `		}` |
|     348 |  334 | `		*(sxi64 *)pOutVal = nVal;` |
|     171 |  335 | `	}` |
|     348 |  336 | `	return zSrc >= zEnd ? SXRET_OK : SXERR_SYNTAX;` |
|       5 |  337 | `}` |
|    8153 |  338 | `PH7_PRIVATE sxi32 SyOctalStrToInt64(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)` |
|       5 |  339 | `{` |
|       - |  340 | `	const char *zIn,*zEnd;` |
|    8158 |  341 | `	int isNeg = FALSE;` |
|    8158 |  342 | `	sxi64 nVal = 0;` |
|       - |  343 | `	int c;` |
|       - |  344 | `#if defined(UNTRUST)` |
|       - |  345 | `	if( SX_EMPTY_STR(zSrc) ){` |
|       - |  346 | `		if( pOutVal ){` |
|       - |  347 | `			*(sxi32 *)pOutVal = 0;` |
|       - |  348 | `		}` |
|       - |  349 | `		return SXERR_EMPTY;` |
|       - |  350 | `	}` |
|       - |  351 | `#endif` |
|    8158 |  352 | `	zEnd = &zSrc[nLen];` |
|    8158 |  353 | `	while(zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|     ! 0 |  354 | `		zSrc++;` |
|     ! 0 |  355 | `	}` |
|    8158 |  356 | `	if( zSrc < zEnd && ( zSrc[0] == '-' \|\| zSrc[0] == '+' ) ){` |
|     ! 0 |  357 | `		isNeg = (zSrc[0] == '-') ? TRUE :FALSE;` |
|     ! 0 |  358 | `		zSrc++;` |
|     ! 0 |  359 | `	}` |
|       - |  360 | `	/* Skip leading zero */` |
|   16307 |  361 | `	while(zSrc < zEnd && zSrc[0] == '0' ){` |
|    8154 |  362 | `		zSrc++;` |
|       5 |  363 | `	}` |
|    8158 |  364 | `	zIn = zSrc;` |
|    4102 |  365 | `	for(;;){` |
|    8222 |  366 | `		if(zSrc >= zEnd \|\| !SyisDigit(zSrc[0])){ break; } if( (c=zSrc[0]-'0') > 7 \|\| (int)(zSrc-zIn) > 20){ break;} nVal = nVal * 8 +  c; zSrc++;` |
|    8218 |  367 | `		if(zSrc >= zEnd \|\| !SyisDigit(zSrc[0])){ break; } if( (c=zSrc[0]-'0') > 7 \|\| (int)(zSrc-zIn) > 20){ break;} nVal = nVal * 8 +  c; zSrc++;` |
|    8198 |  368 | `		if(zSrc >= zEnd \|\| !SyisDigit(zSrc[0])){ break; } if( (c=zSrc[0]-'0') > 7 \|\| (int)(zSrc-zIn) > 20){ break;} nVal = nVal * 8 +  c; zSrc++;` |
|    8134 |  369 | `		if(zSrc >= zEnd \|\| !SyisDigit(zSrc[0])){ break; } if( (c=zSrc[0]-'0') > 7 \|\| (int)(zSrc-zIn) > 20){ break;} nVal = nVal * 8 +  c; zSrc++;` |
|       2 |  370 | `	}` |
|       - |  371 | `	/* Skip trailing spaces */` |
|    8158 |  372 | `	while(zSrc < zEnd && SyisSpace(zSrc[0])){` |
|     ! 0 |  373 | `		zSrc++;` |
|     ! 0 |  374 | `	}` |
|    8158 |  375 | `	if( zRest ){` |
|     ! 0 |  376 | `		*zRest = zSrc;` |
|     ! 0 |  377 | `	}` |
|    8158 |  378 | `	if( pOutVal ){` |
|    8158 |  379 | `		if( isNeg == TRUE && nVal != 0 ){` |
|     ! 0 |  380 | `			nVal = -nVal;` |
|     ! 0 |  381 | `		}` |
|    8158 |  382 | `		*(sxi64 *)pOutVal = nVal;` |
|    4070 |  383 | `	}` |
|    8158 |  384 | `	return (zSrc >= zEnd) ? SXRET_OK : SXERR_SYNTAX;` |
|       5 |  385 | `}` |
|     284 |  386 | `PH7_PRIVATE sxi32 SyBinaryStrToInt64(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)` |
|       1 |  387 | `{` |
|       - |  388 | `	const char *zIn,*zEnd;` |
|     285 |  389 | `	int isNeg = FALSE;` |
|     285 |  390 | `	sxi64 nVal = 0;` |
|       - |  391 | `	int c;` |
|       - |  392 | `#if defined(UNTRUST)` |
|       - |  393 | `	if( SX_EMPTY_STR(zSrc) ){` |
|       - |  394 | `		if( pOutVal ){` |
|       - |  395 | `			*(sxi32 *)pOutVal = 0;` |
|       - |  396 | `		}` |
|       - |  397 | `		return SXERR_EMPTY;` |
|       - |  398 | `	}` |
|       - |  399 | `#endif` |
|     285 |  400 | `	zEnd = &zSrc[nLen];` |
|     285 |  401 | `	while(zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|     ! 0 |  402 | `		zSrc++;` |
|     ! 0 |  403 | `	}` |
|     285 |  404 | `	if( zSrc < zEnd && ( zSrc[0] == '-' \|\| zSrc[0] == '+' ) ){` |
|     ! 0 |  405 | `		isNeg = (zSrc[0] == '-') ? TRUE :FALSE;` |
|     ! 0 |  406 | `		zSrc++;` |
|     ! 0 |  407 | `	}` |
|     285 |  408 | `	if( zSrc < &zEnd[-2] && zSrc[0] == '0' && (zSrc[1] == 'b' \|\| zSrc[1] == 'B') ){` |
|       - |  409 | `		/* Bypass binary prefix */` |
|     285 |  410 | `		zSrc += sizeof(char) * 2;` |
|     142 |  411 | `	}` |
|       - |  412 | `	/* Skip leading zero */` |
|     333 |  413 | `	while(zSrc < zEnd && zSrc[0] == '0' ){` |
|      49 |  414 | `		zSrc++;` |
|       1 |  415 | `	}` |
|     285 |  416 | `	zIn = zSrc;` |
|     314 |  417 | `	for(;;){` |
|     629 |  418 | `		if(zSrc >= zEnd \|\| (zSrc[0] != '1' && zSrc[0] != '0') \|\| (int)(zSrc-zIn) > 62){ break; } c = zSrc[0] - '0'; nVal = (nVal << 1) + c; zSrc++;` |
|     461 |  419 | `		if(zSrc >= zEnd \|\| (zSrc[0] != '1' && zSrc[0] != '0') \|\| (int)(zSrc-zIn) > 62){ break; } c = zSrc[0] - '0'; nVal = (nVal << 1) + c; zSrc++;` |
|     429 |  420 | `		if(zSrc >= zEnd \|\| (zSrc[0] != '1' && zSrc[0] != '0') \|\| (int)(zSrc-zIn) > 62){ break; } c = zSrc[0] - '0'; nVal = (nVal << 1) + c; zSrc++;` |
|     383 |  421 | `		if(zSrc >= zEnd \|\| (zSrc[0] != '1' && zSrc[0] != '0') \|\| (int)(zSrc-zIn) > 62){ break; } c = zSrc[0] - '0'; nVal = (nVal << 1) + c; zSrc++;` |
|       1 |  422 | `	}` |
|       - |  423 | `	/* Skip trailing spaces */` |
|     285 |  424 | `	while(zSrc < zEnd && SyisSpace(zSrc[0])){` |
|     ! 0 |  425 | `		zSrc++;` |
|     ! 0 |  426 | `	}` |
|     285 |  427 | `	if( zRest ){` |
|     ! 0 |  428 | `		*zRest = zSrc;` |
|     ! 0 |  429 | `	}` |
|     285 |  430 | `	if( pOutVal ){` |
|     285 |  431 | `		if( isNeg == TRUE && nVal != 0 ){` |
|     ! 0 |  432 | `			nVal = -nVal;` |
|     ! 0 |  433 | `		}` |
|     285 |  434 | `		*(sxi64 *)pOutVal = nVal;` |
|     142 |  435 | `	}` |
|     285 |  436 | `	return (zSrc >= zEnd) ? SXRET_OK : SXERR_SYNTAX;` |
|       1 |  437 | `}` |
|   19768 |  438 | `PH7_PRIVATE sxi32 SyStrToReal(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)` |
|       5 |  439 | `{` |
|       - |  440 | `	/* Correctly-rounded conversion via libc strtod (the byte-exact-floats` |
|       - |  441 | `	 * rule): the old hand-rolled accumulator kept only 15 significant` |
|       - |  442 | `	 * digits, clamped exponents to +/-30x (so 1e400 silently became 1e308` |
|       - |  443 | `	 * and 5e-324 became 5e-307) and drifted the low mantissa bits, making` |
|       - |  444 | `	 * float literals and string->float casts differ from php. The accepted` |
|       - |  445 | `	 * numeric-prefix grammar is kept from the old parser, including the ','` |
|       - |  446 | `	 * decimal separator: [ws][sign]D*[(.\|,)D*][(e\|E)[sign]D+][ws]. The` |
|       - |  447 | `	 * prefix is copied (',' -> '.') into a stack buffer for strtod — or a` |
|       - |  448 | `	 * heap copy for the rare number longer than the buffer (e.g. hundreds of` |
|       - |  449 | `	 * leading fractional zeros before the significant digits), falling back` |
|       - |  450 | `	 * to a truncated-mantissa-plus-exponent copy only if that allocation` |
|       - |  451 | `	 * fails. Everything is always consumed from the input. */` |
|       - |  452 | `	char zBuf[512];` |
|   19773 |  453 | `	const char *zEnd = &zSrc[nLen];` |
|       - |  454 | `	const char *zNum;` |
|   19773 |  455 | `	const char *zExpStart = 0;` |
|   19773 |  456 | `	sxreal Val = 0.0;` |
|   19773 |  457 | `	sxu32 nCopy = 0;` |
|   19773 |  458 | `	int bDigit = 0;` |
|       - |  459 | `#ifdef UNTRUST` |
|       - |  460 | `	if( SX_EMPTY_STR(zSrc)  ){` |
|       - |  461 | `		if( pOutVal ){` |
|       - |  462 | `			*(sxreal *)pOutVal = 0.0;` |
|       - |  463 | `		}` |
|       - |  464 | `		return SXERR_EMPTY;` |
|       - |  465 | `	}` |
|       - |  466 | `#endif` |
|       - |  467 | `	/* Skip leading spaces */` |
|   19815 |  468 | `	while( zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|      44 |  469 | `		zSrc++;` |
|       2 |  470 | `	}` |
|   19773 |  471 | `	zNum = zSrc;` |
|       - |  472 | `	/* Sign (if exists) */` |
|   19773 |  473 | `	if( zSrc < zEnd && (zSrc[0] == '-' \|\| zSrc[0] == '+' ) ){` |
|     193 |  474 | `		zSrc++;` |
|      95 |  475 | `	}` |
|       - |  476 | `	/* Integer part */` |
|   65077 |  477 | `	while( zSrc < zEnd && SyisDigit(zSrc[0]) ){` |
|   45309 |  478 | `		bDigit = 1;` |
|   45309 |  479 | `		zSrc++;` |
|       5 |  480 | `	}` |
|       - |  481 | `	/* Fractional part */` |
|   19773 |  482 | `	if( zSrc < zEnd && ( zSrc[0] == '.' \|\| zSrc[0] == ',' ) ){` |
|   18597 |  483 | `		zSrc++;` |
|   41259 |  484 | `		while( zSrc < zEnd && SyisDigit(zSrc[0]) ){` |
|   22667 |  485 | `			bDigit = 1;` |
|   22667 |  486 | `			zSrc++;` |
|       5 |  487 | `		}` |
|    9287 |  488 | `	}` |
|       - |  489 | `	/* Exponent — consumed only when it carries at least one digit, like` |
|       - |  490 | `	 * strtod, so "1e+x" leaves the "e+" unconsumed. */` |
|   19773 |  491 | `	if( bDigit && zSrc < zEnd && ( zSrc[0] == 'e' \|\| zSrc[0] == 'E' ) ){` |
|     563 |  492 | `		const char *zExp = &zSrc[1];` |
|     563 |  493 | `		if( zExp < zEnd && (zExp[0] == '-' \|\| zExp[0] == '+') ){` |
|     184 |  494 | `			zExp++;` |
|      90 |  495 | `		}` |
|     563 |  496 | `		if( zExp < zEnd && SyisDigit(zExp[0]) ){` |
|     563 |  497 | `			zExpStart = zSrc;` |
|     563 |  498 | `			zSrc = zExp;` |
|    1559 |  499 | `			while( zSrc < zEnd && SyisDigit(zSrc[0]) ){` |
|    1001 |  500 | `				zSrc++;` |
|       5 |  501 | `			}` |
|     279 |  502 | `		}` |
|     279 |  503 | `	}` |
|   19773 |  504 | `	if( bDigit ){` |
|   19745 |  505 | `		sxu32 i, nSpan = (sxu32)((zExpStart ? zExpStart : zSrc) - zNum);` |
|   19745 |  506 | `		sxu32 nExp = zExpStart ? (sxu32)(zSrc - zExpStart) : 0;` |
|   19745 |  507 | `		char *zDup = zBuf;` |
|   19745 |  508 | `		sxu32 nDup = sizeof(zBuf);` |
|   19745 |  509 | `		if( nSpan + nExp >= sizeof(zBuf) ){` |
|       3 |  510 | `			char *zHeap = (char *)malloc(nSpan + nExp + 1);` |
|       3 |  511 | `			if( zHeap ){` |
|       3 |  512 | `				zDup = zHeap;` |
|       3 |  513 | `				nDup = nSpan + nExp + 1;` |
|       1 |  514 | `			}` |
|       1 |  515 | `		}` |
|       - |  516 | `		{` |
|   19745 |  517 | `			sxu32 nMantMax = nDup - 1 - (nExp < nDup - 1 ? nExp : 0);` |
|  106493 |  518 | `			for( i = 0 ; i < nSpan && nCopy < nMantMax ; i++ ){` |
|   86753 |  519 | `				zDup[nCopy++] = (zNum[i] == ',') ? '.' : zNum[i];` |
|   43352 |  520 | `			}` |
|       - |  521 | `			/* The exponent rides behind even a truncated mantissa: dropping` |
|       - |  522 | `			 * it would collapse "0.<hundreds of zeros>1e300" to 0.0. */` |
|   21479 |  523 | `			for( i = 0 ; i < nExp && nCopy < nDup - 1 ; i++ ){` |
|    1739 |  524 | `				zDup[nCopy++] = zExpStart[i];` |
|     872 |  525 | `			}` |
|       - |  526 | `		}` |
|   19745 |  527 | `		zDup[nCopy] = 0;` |
|   19745 |  528 | `		Val = (sxreal)strtod(zDup,0);` |
|   19745 |  529 | `		if( zDup != zBuf ){` |
|       3 |  530 | `			free(zDup);` |
|       1 |  531 | `		}` |
|    9861 |  532 | `	}` |
|       - |  533 | `	/* Jump trailing spaces */` |
|   19815 |  534 | `	while( zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|      44 |  535 | `		zSrc++;` |
|       2 |  536 | `	}` |
|   19773 |  537 | `	if( zRest ){` |
|     ! 0 |  538 | `		*zRest = zSrc;` |
|     ! 0 |  539 | `	}` |
|   19773 |  540 | `	if( pOutVal ){` |
|   19773 |  541 | `		*(sxreal *)pOutVal = Val;` |
|    9875 |  542 | `	}` |
|   19773 |  543 | `	return zSrc >= zEnd ? SXRET_OK : SXERR_SYNTAX;` |
|       5 |  544 | `}` |
|       - |  545 |  |
