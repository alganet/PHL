# src/sx/sxutils.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 329/398 lines (82.66%)

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
|     440 |   12 | `PH7_PRIVATE sxi32 SyStrIsNumeric(const char *zSrc,sxu32 nLen,sxu8 *pReal,const char  **pzTail)` |
|       5 |   13 | `{` |
|       - |   14 | `	const char *zCur,*zEnd;` |
|       - |   15 | `#ifdef UNTRUST` |
|       - |   16 | `	if( SX_EMPTY_STR(zSrc) ){` |
|       - |   17 | `		return SXERR_EMPTY;` |
|       - |   18 | `	}` |
|       - |   19 | `#endif` |
|     445 |   20 | `	zEnd = &zSrc[nLen];` |
|       - |   21 | `	/* Jump leading white spaces */` |
|     451 |   22 | `	while( zSrc < zEnd && (unsigned char)zSrc[0] < 0xc0  && SyisSpace(zSrc[0]) ){` |
|       7 |   23 | `		zSrc++;` |
|       1 |   24 | `	}` |
|     445 |   25 | `	if( zSrc < zEnd && (zSrc[0] == '+' \|\| zSrc[0] == '-') ){` |
|      38 |   26 | `		zSrc++;` |
|      18 |   27 | `	}` |
|     445 |   28 | `	zCur = zSrc;` |
|     445 |   29 | `	if( pReal ){` |
|     439 |   30 | `		*pReal = FALSE;` |
|     217 |   31 | `	}` |
|     223 |   32 | `	for(;;){` |
|     451 |   33 | `		if( zSrc >= zEnd \|\| (unsigned char)zSrc[0] >= 0xc0 \|\| !SyisDigit(zSrc[0]) ){` |
|      38 |   34 | `			break;` |
|       - |   35 | `		}` |
|     383 |   36 | `		zSrc++;` |
|     383 |   37 | `		if( zSrc >= zEnd \|\| (unsigned char)zSrc[0] >= 0xc0 \|\| !SyisDigit(zSrc[0]) ){` |
|     180 |   38 | `			break;` |
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
|     445 |   50 | `	if( zSrc < zEnd && zSrc > zCur ){` |
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
|     445 |   86 | `	if( pzTail ){` |
|       - |   87 | `		/* Point to the non numeric part */` |
|      37 |   88 | `		*pzTail = zSrc;` |
|      17 |   89 | `	}` |
|     445 |   90 | `	return zSrc > zCur ? SXRET_OK /* String prefix is numeric */ : SXERR_INVALID /* Not a digit stream */;` |
|       5 |   91 | `}` |
|       - |   92 | `#define SXINT32_MIN_STR		"2147483648"` |
|       - |   93 | `#define SXINT32_MAX_STR		"2147483647"` |
|    1839 |   94 | `PH7_PRIVATE sxi32 SyStrToInt32(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)` |
|       5 |   95 | `{` |
|    1844 |   96 | `	int isNeg = FALSE;` |
|       - |   97 | `	const char *zEnd;` |
|    1844 |   98 | `	sxi32 nVal = 0;` |
|       - |   99 | `	sxi16 i;` |
|       - |  100 | `#if defined(UNTRUST)` |
|       - |  101 | `	if( SX_EMPTY_STR(zSrc) ){` |
|       - |  102 | `		if( pOutVal ){` |
|       - |  103 | `			*(sxi32 *)pOutVal = 0;` |
|       - |  104 | `		}` |
|       - |  105 | `		return SXERR_EMPTY;` |
|       - |  106 | `	}` |
|       - |  107 | `#endif` |
|    1844 |  108 | `	zEnd = &zSrc[nLen];` |
|    1852 |  109 | `	while(zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|       8 |  110 | `		zSrc++;` |
|     ! 0 |  111 | `	}` |
|    1844 |  112 | `	if( zSrc < zEnd && ( zSrc[0] == '-' \|\| zSrc[0] == '+' ) ){` |
|     ! 0 |  113 | `		isNeg = (zSrc[0] == '-') ? TRUE :FALSE;` |
|     ! 0 |  114 | `		zSrc++;` |
|     ! 0 |  115 | `	}` |
|       - |  116 | `	/* Skip leading zero */` |
|    2441 |  117 | `	while(zSrc < zEnd && zSrc[0] == '0' ){` |
|     601 |  118 | `		zSrc++;` |
|       4 |  119 | `	}` |
|    1844 |  120 | `	i = 10;` |
|    1844 |  121 | `	if( (sxu32)(zEnd-zSrc) >= 10 ){` |
|       - |  122 | `		/* Handle overflow */` |
|      28 |  123 | `		i = SyMemcmp(zSrc,(isNeg == TRUE) ? SXINT32_MIN_STR : SXINT32_MAX_STR,nLen) <= 0 ? 10 : 9;` |
|      14 |  124 | `	}` |
|    1050 |  125 | `	for(;;){` |
|    2124 |  126 | `		if(zSrc >= zEnd \|\| !i \|\| !SyisDigit(zSrc[0])){ break; } nVal = nVal * 10 + ( zSrc[0] - '0' ) ; --i ; zSrc++;` |
|    1245 |  127 | `		if(zSrc >= zEnd \|\| !i \|\| !SyisDigit(zSrc[0])){` |
|     337 |  128 | `			break;` |
|       - |  129 | `		}` |
|     563 |  130 | `		nVal = nVal * 10 + ( zSrc[0] - '0' );` |
|     563 |  131 | `		--i;` |
|     563 |  132 | `		zSrc++;` |
|     563 |  133 | `		if(zSrc >= zEnd \|\| !i \|\| !SyisDigit(zSrc[0])){` |
|     ! 0 |  134 | `			break;` |
|       - |  135 | `		}` |
|     563 |  136 | `		nVal = nVal * 10 + ( zSrc[0] - '0' );` |
|     563 |  137 | `		--i;` |
|     563 |  138 | `		zSrc++;` |
|     563 |  139 | `		if(zSrc >= zEnd \|\| !i \|\| !SyisDigit(zSrc[0])){` |
|     136 |  140 | `			break;` |
|       - |  141 | `		}` |
|     291 |  142 | `		nVal = nVal * 10 + ( zSrc[0] - '0' );` |
|     291 |  143 | `		--i;` |
|     291 |  144 | `		zSrc++;` |
|     291 |  145 | `		if(zSrc >= zEnd \|\| !i \|\| !SyisDigit(zSrc[0])){` |
|       6 |  146 | `			break;` |
|       - |  147 | `		}` |
|     280 |  148 | `		nVal = nVal * 10 + ( zSrc[0] - '0' );` |
|     280 |  149 | `		--i;` |
|     280 |  150 | `		zSrc++;` |
|     ! 0 |  151 | `	}` |
|       - |  152 | `	/* Skip trailing spaces */` |
|    2118 |  153 | `	while(zSrc < zEnd && SyisSpace(zSrc[0])){` |
|     274 |  154 | `		zSrc++;` |
|     ! 0 |  155 | `	}` |
|    1844 |  156 | `	if( zRest ){` |
|     ! 0 |  157 | `		*zRest = (char *)zSrc;` |
|     ! 0 |  158 | `	}` |
|    1844 |  159 | `	if( pOutVal ){` |
|    1834 |  160 | `		if( isNeg == TRUE && nVal != 0 ){` |
|     ! 0 |  161 | `			nVal = -nVal;` |
|     ! 0 |  162 | `		}` |
|    1834 |  163 | `		*(sxi32 *)pOutVal = nVal;` |
|     905 |  164 | `	}` |
|    1844 |  165 | `	return (zSrc >= zEnd) ? SXRET_OK : SXERR_SYNTAX;` |
|       5 |  166 | `}` |
|  474415 |  167 | `PH7_PRIVATE sxi32 SyStrToInt64(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)` |
|       5 |  168 | `{` |
|  474420 |  169 | `	return SyStrToInt64Ex(zSrc,nLen,pOutVal,zRest,0);` |
|       5 |  170 | `}` |
|       - |  171 | `/*` |
|       - |  172 | ` * SyStrToInt64 plus the one fact its saturating reader threw away: whether the` |
|       - |  173 | ` * digit run ran PAST the int64 range. *pOverflow comes back 1 for the positive` |
|       - |  174 | ` * side, -1 for the negative one and 0 when the value fits. The integer handed` |
|       - |  175 | ` * back is still the saturated one, which is what php's (int) cast wants -- but a` |
|       - |  176 | ` * caller converting a numeric STRING to a NUMBER needs to know, because php's` |
|       - |  177 | ` * answer there is a float, not PHP_INT_MAX.` |
|       - |  178 | ` */` |
| 1651804 |  179 | `PH7_PRIVATE sxi32 SyStrToInt64Ex(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest,int *pOverflow)` |
|       5 |  180 | `{` |
| 1651809 |  181 | `	int isNeg = FALSE;` |
|       - |  182 | `	const char *zEnd;` |
|       - |  183 | `	sxi64 nVal;` |
|       - |  184 | `	/* Magnitude accumulated unsigned so overflow can be detected and the result` |
|       - |  185 | `	 * saturated (PHP casts an out-of-range numeric string to PHP_INT_MAX/MIN)` |
|       - |  186 | `	 * rather than the digits being dropped. cutoff is the largest magnitude that` |
|       - |  187 | `	 * fits: PHP_INT_MAX for a positive value, \|PHP_INT_MIN\| == 2^63 for a` |
|       - |  188 | `	 * negative one. */` |
|       - |  189 | `	sxu64 uVal, cutoff;` |
| 1651809 |  190 | `	int bOverflow = FALSE;` |
| 1651809 |  191 | `	if( pOverflow ){` |
|    2485 |  192 | `		*pOverflow = 0;` |
|    1232 |  193 | `	}` |
|       - |  194 | `#if defined(UNTRUST)` |
|       - |  195 | `	if( SX_EMPTY_STR(zSrc) ){` |
|       - |  196 | `		if( pOutVal ){` |
|       - |  197 | `			*(sxi32 *)pOutVal = 0;` |
|       - |  198 | `		}` |
|       - |  199 | `		return SXERR_EMPTY;` |
|       - |  200 | `	}` |
|       - |  201 | `#endif` |
| 1651809 |  202 | `	zEnd = &zSrc[nLen];` |
| 1651895 |  203 | `	while(zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|      89 |  204 | `		zSrc++;` |
|       3 |  205 | `	}` |
| 1651809 |  206 | `	if( zSrc < zEnd && ( zSrc[0] == '-' \|\| zSrc[0] == '+' ) ){` |
|     259 |  207 | `		isNeg = (zSrc[0] == '-') ? TRUE :FALSE;` |
|     259 |  208 | `		zSrc++;` |
|     128 |  209 | `	}` |
|       - |  210 | `	/* Skip leading zero */` |
| 1653889 |  211 | `	while(zSrc < zEnd && zSrc[0] == '0' ){` |
|    2085 |  212 | `		zSrc++;` |
|       5 |  213 | `	}` |
| 1651809 |  214 | `	cutoff = isNeg ? ((sxu64)SXI64_HIGH + 1) : (sxu64)SXI64_HIGH;` |
| 1651809 |  215 | `	uVal = 0;` |
| 5152411 |  216 | `	while( zSrc < zEnd && (unsigned char)zSrc[0] < 0xc0 && SyisDigit(zSrc[0]) ){` |
| 3500607 |  217 | `		int d = zSrc[0] - '0';` |
| 3500607 |  218 | `		if( uVal > cutoff / 10 \|\| (uVal == cutoff / 10 && (sxu64)d > cutoff % 10) ){` |
|   12909 |  219 | `			bOverflow = TRUE;` |
|    6456 |  220 | `		}else{` |
| 3487701 |  221 | `			uVal = uVal * 10 + (sxu64)d;` |
|       - |  222 | `		}` |
| 3500607 |  223 | `		zSrc++;` |
|       5 |  224 | `	}` |
| 1651809 |  225 | `	if( bOverflow ){` |
|     599 |  226 | `		uVal = cutoff;` |
|     599 |  227 | `		if( pOverflow ){` |
|     564 |  228 | `			*pOverflow = isNeg ? -1 : 1;` |
|     281 |  229 | `		}` |
|     298 |  230 | `	}` |
|       - |  231 | `	/* Skip trailing spaces */` |
| 1651883 |  232 | `	while(zSrc < zEnd && SyisSpace(zSrc[0])){` |
|      77 |  233 | `		zSrc++;` |
|       3 |  234 | `	}` |
| 1651809 |  235 | `	if( zRest ){` |
|     ! 0 |  236 | `		*zRest = (char *)zSrc;` |
|     ! 0 |  237 | `	}` |
| 1651809 |  238 | `	if( pOutVal ){` |
| 1651809 |  239 | `		if( isNeg ){` |
|       - |  240 | `			/* uVal <= 2^63; the cap value 2^63 is PHP_INT_MIN and has no positive` |
|       - |  241 | `			 * sxi64 representation, so materialize it directly to dodge UB. */` |
|     233 |  242 | `			nVal = ( uVal > (sxu64)SXI64_HIGH ) ? (-SXI64_HIGH - 1) : -(sxi64)uVal;` |
|     118 |  243 | `		}else{` |
| 1651579 |  244 | `			nVal = (sxi64)uVal;` |
|       - |  245 | `		}` |
| 1651809 |  246 | `		*(sxi64 *)pOutVal = nVal;` |
|  825567 |  247 | `	}` |
| 1651809 |  248 | `	return (zSrc >= zEnd) ? SXRET_OK : SXERR_SYNTAX;` |
|       5 |  249 | `}` |
|   12374 |  250 | `PH7_PRIVATE sxi32 SyHexToint(sxi32 c)` |
|       5 |  251 | `{` |
|   12379 |  252 | `	switch(c){` |
|    3342 |  253 | `	case '0': return 0;` |
|    1000 |  254 | `	case '1': return 1;` |
|     709 |  255 | `	case '2': return 2;` |
|     497 |  256 | `	case '3': return 3;` |
|     345 |  257 | `	case '4': return 4;` |
|     245 |  258 | `	case '5': return 5;` |
|     191 |  259 | `	case '6': return 6;` |
|     233 |  260 | `	case '7': return 7;` |
|     638 |  261 | `	case '8': return 8;` |
|     631 |  262 | `	case '9': return 9;` |
|     566 |  263 | `	case 'A': case 'a': return 10;` |
|     399 |  264 | `	case 'B': case 'b': return 11;` |
|     629 |  265 | `	case 'C': case 'c': return 12;` |
|     349 |  266 | `	case 'D': case 'd': return 13;` |
|     614 |  267 | `	case 'E': case 'e': return 14;` |
|    2034 |  268 | `	case 'F': case 'f': return 15;` |
|       - |  269 | `	}` |
|      25 |  270 | `	return -1;` |
|    5923 |  271 | `}` |
|     315 |  272 | `PH7_PRIVATE sxi32 SyHexStrToInt64(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)` |
|       4 |  273 | `{` |
|       - |  274 | `	const char *zIn,*zEnd;` |
|     319 |  275 | `	int isNeg = FALSE;` |
|     319 |  276 | `	sxi64 nVal = 0;` |
|       - |  277 | `#if defined(UNTRUST)` |
|       - |  278 | `	if( SX_EMPTY_STR(zSrc) ){` |
|       - |  279 | `		if( pOutVal ){` |
|       - |  280 | `			*(sxi32 *)pOutVal = 0;` |
|       - |  281 | `		}` |
|       - |  282 | `		return SXERR_EMPTY;` |
|       - |  283 | `	}` |
|       - |  284 | `#endif` |
|     319 |  285 | `	zEnd = &zSrc[nLen];` |
|     319 |  286 | `	while( zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|     ! 0 |  287 | `		zSrc++;` |
|     ! 0 |  288 | `	}` |
|     319 |  289 | `	if( zSrc < zEnd && ( *zSrc == '-' \|\| *zSrc == '+' ) ){` |
|     ! 0 |  290 | `		isNeg = (zSrc[0] == '-') ? TRUE :FALSE;` |
|     ! 0 |  291 | `		zSrc++;` |
|     ! 0 |  292 | `	}` |
|     319 |  293 | `	if( zSrc < &zEnd[-2] && zSrc[0] == '0' && (zSrc[1] == 'x' \|\| zSrc[1] == 'X') ){` |
|       - |  294 | `		/* Bypass hex prefix */` |
|     319 |  295 | `		zSrc += sizeof(char) * 2;` |
|     157 |  296 | `	}` |
|       - |  297 | `	/* Skip leading zero */` |
|     371 |  298 | `	while(zSrc < zEnd && zSrc[0] == '0' ){` |
|      54 |  299 | `		zSrc++;` |
|       2 |  300 | `	}` |
|     319 |  301 | `	zIn = zSrc;` |
|     246 |  302 | `	for(;;){` |
|     499 |  303 | `		if(zSrc >= zEnd \|\| !SyisHex(zSrc[0]) \|\| (int)(zSrc-zIn) > 15){` |
|      56 |  304 | `			break;` |
|       - |  305 | `		}` |
|     392 |  306 | `		nVal = nVal * 16 + SyHexToint(zSrc[0]);` |
|     392 |  307 | `		zSrc++;` |
|     392 |  308 | `		if(zSrc >= zEnd \|\| !SyisHex(zSrc[0]) \|\| (int)(zSrc-zIn) > 15){` |
|      14 |  309 | `			break;` |
|       - |  310 | `		}` |
|     368 |  311 | `		nVal = nVal * 16 + SyHexToint(zSrc[0]);` |
|     368 |  312 | `		zSrc++;` |
|     368 |  313 | `		if(zSrc >= zEnd \|\| !SyisHex(zSrc[0]) \|\| (int)(zSrc-zIn) > 15){` |
|      81 |  314 | `			break;` |
|       - |  315 | `		}` |
|     212 |  316 | `		nVal = nVal * 16 + SyHexToint(zSrc[0]);` |
|     212 |  317 | `		zSrc++;` |
|     212 |  318 | `		if(zSrc >= zEnd \|\| !SyisHex(zSrc[0]) \|\| (int)(zSrc-zIn) > 15){` |
|      15 |  319 | `			break;` |
|       - |  320 | `		}` |
|     184 |  321 | `		nVal = nVal * 16 + SyHexToint(zSrc[0]);` |
|     184 |  322 | `		zSrc++;` |
|       4 |  323 | `	}` |
|     319 |  324 | `	while( zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|     ! 0 |  325 | `		zSrc++;` |
|     ! 0 |  326 | `	}` |
|     319 |  327 | `	if( zRest ){` |
|     ! 0 |  328 | `		*zRest = zSrc;` |
|     ! 0 |  329 | `	}` |
|     319 |  330 | `	if( pOutVal ){` |
|     319 |  331 | `		if( isNeg == TRUE && nVal != 0 ){` |
|     ! 0 |  332 | `			nVal = -nVal;` |
|     ! 0 |  333 | `		}` |
|     319 |  334 | `		*(sxi64 *)pOutVal = nVal;` |
|     157 |  335 | `	}` |
|     319 |  336 | `	return zSrc >= zEnd ? SXRET_OK : SXERR_SYNTAX;` |
|       4 |  337 | `}` |
|    6930 |  338 | `PH7_PRIVATE sxi32 SyOctalStrToInt64(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)` |
|       5 |  339 | `{` |
|       - |  340 | `	const char *zIn,*zEnd;` |
|    6935 |  341 | `	int isNeg = FALSE;` |
|    6935 |  342 | `	sxi64 nVal = 0;` |
|       - |  343 | `	int c;` |
|       - |  344 | `#if defined(UNTRUST)` |
|       - |  345 | `	if( SX_EMPTY_STR(zSrc) ){` |
|       - |  346 | `		if( pOutVal ){` |
|       - |  347 | `			*(sxi32 *)pOutVal = 0;` |
|       - |  348 | `		}` |
|       - |  349 | `		return SXERR_EMPTY;` |
|       - |  350 | `	}` |
|       - |  351 | `#endif` |
|    6935 |  352 | `	zEnd = &zSrc[nLen];` |
|    6935 |  353 | `	while(zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|     ! 0 |  354 | `		zSrc++;` |
|     ! 0 |  355 | `	}` |
|    6935 |  356 | `	if( zSrc < zEnd && ( zSrc[0] == '-' \|\| zSrc[0] == '+' ) ){` |
|     ! 0 |  357 | `		isNeg = (zSrc[0] == '-') ? TRUE :FALSE;` |
|     ! 0 |  358 | `		zSrc++;` |
|     ! 0 |  359 | `	}` |
|       - |  360 | `	/* Skip leading zero */` |
|   13861 |  361 | `	while(zSrc < zEnd && zSrc[0] == '0' ){` |
|    6931 |  362 | `		zSrc++;` |
|       5 |  363 | `	}` |
|    6935 |  364 | `	zIn = zSrc;` |
|    3491 |  365 | `	for(;;){` |
|    6999 |  366 | `		if(zSrc >= zEnd \|\| !SyisDigit(zSrc[0])){ break; } if( (c=zSrc[0]-'0') > 7 \|\| (int)(zSrc-zIn) > 20){ break;} nVal = nVal * 8 +  c; zSrc++;` |
|    6995 |  367 | `		if(zSrc >= zEnd \|\| !SyisDigit(zSrc[0])){ break; } if( (c=zSrc[0]-'0') > 7 \|\| (int)(zSrc-zIn) > 20){ break;} nVal = nVal * 8 +  c; zSrc++;` |
|    6975 |  368 | `		if(zSrc >= zEnd \|\| !SyisDigit(zSrc[0])){ break; } if( (c=zSrc[0]-'0') > 7 \|\| (int)(zSrc-zIn) > 20){ break;} nVal = nVal * 8 +  c; zSrc++;` |
|    6911 |  369 | `		if(zSrc >= zEnd \|\| !SyisDigit(zSrc[0])){ break; } if( (c=zSrc[0]-'0') > 7 \|\| (int)(zSrc-zIn) > 20){ break;} nVal = nVal * 8 +  c; zSrc++;` |
|       2 |  370 | `	}` |
|       - |  371 | `	/* Skip trailing spaces */` |
|    6935 |  372 | `	while(zSrc < zEnd && SyisSpace(zSrc[0])){` |
|     ! 0 |  373 | `		zSrc++;` |
|     ! 0 |  374 | `	}` |
|    6935 |  375 | `	if( zRest ){` |
|     ! 0 |  376 | `		*zRest = zSrc;` |
|     ! 0 |  377 | `	}` |
|    6935 |  378 | `	if( pOutVal ){` |
|    6935 |  379 | `		if( isNeg == TRUE && nVal != 0 ){` |
|     ! 0 |  380 | `			nVal = -nVal;` |
|     ! 0 |  381 | `		}` |
|    6935 |  382 | `		*(sxi64 *)pOutVal = nVal;` |
|    3459 |  383 | `	}` |
|    6935 |  384 | `	return (zSrc >= zEnd) ? SXRET_OK : SXERR_SYNTAX;` |
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
|   17131 |  438 | `PH7_PRIVATE sxi32 SyStrToReal(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)` |
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
|   17136 |  453 | `	const char *zEnd = &zSrc[nLen];` |
|       - |  454 | `	const char *zNum;` |
|   17136 |  455 | `	const char *zExpStart = 0;` |
|   17136 |  456 | `	sxreal Val = 0.0;` |
|   17136 |  457 | `	sxu32 nCopy = 0;` |
|   17136 |  458 | `	int bDigit = 0;` |
|       - |  459 | `#ifdef UNTRUST` |
|       - |  460 | `	if( SX_EMPTY_STR(zSrc)  ){` |
|       - |  461 | `		if( pOutVal ){` |
|       - |  462 | `			*(sxreal *)pOutVal = 0.0;` |
|       - |  463 | `		}` |
|       - |  464 | `		return SXERR_EMPTY;` |
|       - |  465 | `	}` |
|       - |  466 | `#endif` |
|       - |  467 | `	/* Skip leading spaces */` |
|   17178 |  468 | `	while( zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|      44 |  469 | `		zSrc++;` |
|       2 |  470 | `	}` |
|   17136 |  471 | `	zNum = zSrc;` |
|       - |  472 | `	/* Sign (if exists) */` |
|   17136 |  473 | `	if( zSrc < zEnd && (zSrc[0] == '-' \|\| zSrc[0] == '+' ) ){` |
|     193 |  474 | `		zSrc++;` |
|      95 |  475 | `	}` |
|       - |  476 | `	/* Integer part */` |
|   59627 |  477 | `	while( zSrc < zEnd && SyisDigit(zSrc[0]) ){` |
|   42496 |  478 | `		bDigit = 1;` |
|   42496 |  479 | `		zSrc++;` |
|       5 |  480 | `	}` |
|       - |  481 | `	/* Fractional part */` |
|   17136 |  482 | `	if( zSrc < zEnd && ( zSrc[0] == '.' \|\| zSrc[0] == ',' ) ){` |
|   15990 |  483 | `		zSrc++;` |
|   35753 |  484 | `		while( zSrc < zEnd && SyisDigit(zSrc[0]) ){` |
|   19768 |  485 | `			bDigit = 1;` |
|   19768 |  486 | `			zSrc++;` |
|       5 |  487 | `		}` |
|    7983 |  488 | `	}` |
|       - |  489 | `	/* Exponent — consumed only when it carries at least one digit, like` |
|       - |  490 | `	 * strtod, so "1e+x" leaves the "e+" unconsumed. */` |
|   17136 |  491 | `	if( bDigit && zSrc < zEnd && ( zSrc[0] == 'e' \|\| zSrc[0] == 'E' ) ){` |
|     565 |  492 | `		const char *zExp = &zSrc[1];` |
|     565 |  493 | `		if( zExp < zEnd && (zExp[0] == '-' \|\| zExp[0] == '+') ){` |
|     184 |  494 | `			zExp++;` |
|      90 |  495 | `		}` |
|     565 |  496 | `		if( zExp < zEnd && SyisDigit(zExp[0]) ){` |
|     565 |  497 | `			zExpStart = zSrc;` |
|     565 |  498 | `			zSrc = zExp;` |
|    1551 |  499 | `			while( zSrc < zEnd && SyisDigit(zSrc[0]) ){` |
|     991 |  500 | `				zSrc++;` |
|       5 |  501 | `			}` |
|     280 |  502 | `		}` |
|     280 |  503 | `	}` |
|   17136 |  504 | `	if( bDigit ){` |
|   17102 |  505 | `		sxu32 i, nSpan = (sxu32)((zExpStart ? zExpStart : zSrc) - zNum);` |
|   17102 |  506 | `		sxu32 nExp = zExpStart ? (sxu32)(zSrc - zExpStart) : 0;` |
|   17102 |  507 | `		char *zDup = zBuf;` |
|   17102 |  508 | `		sxu32 nDup = sizeof(zBuf);` |
|   17102 |  509 | `		if( nSpan + nExp >= sizeof(zBuf) ){` |
|       3 |  510 | `			char *zHeap = (char *)malloc(nSpan + nExp + 1);` |
|       3 |  511 | `			if( zHeap ){` |
|       3 |  512 | `				zDup = zHeap;` |
|       3 |  513 | `				nDup = nSpan + nExp + 1;` |
|       1 |  514 | `			}` |
|       1 |  515 | `		}` |
|       - |  516 | `		{` |
|   17102 |  517 | `			sxu32 nMantMax = nDup - 1 - (nExp < nDup - 1 ? nExp : 0);` |
|   95531 |  518 | `			for( i = 0 ; i < nSpan && nCopy < nMantMax ; i++ ){` |
|   78434 |  519 | `				zDup[nCopy++] = (zNum[i] == ',') ? '.' : zNum[i];` |
|   39191 |  520 | `			}` |
|       - |  521 | `			/* The exponent rides behind even a truncated mantissa: dropping` |
|       - |  522 | `			 * it would collapse "0.<hundreds of zeros>1e300" to 0.0. */` |
|   18828 |  523 | `			for( i = 0 ; i < nExp && nCopy < nDup - 1 ; i++ ){` |
|    1731 |  524 | `				zDup[nCopy++] = zExpStart[i];` |
|     868 |  525 | `			}` |
|       - |  526 | `		}` |
|   17102 |  527 | `		zDup[nCopy] = 0;` |
|   17102 |  528 | `		Val = (sxreal)strtod(zDup,0);` |
|   17102 |  529 | `		if( zDup != zBuf ){` |
|       3 |  530 | `			free(zDup);` |
|       1 |  531 | `		}` |
|    8539 |  532 | `	}` |
|       - |  533 | `	/* Jump trailing spaces */` |
|   17178 |  534 | `	while( zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|      44 |  535 | `		zSrc++;` |
|       2 |  536 | `	}` |
|   17136 |  537 | `	if( zRest ){` |
|     ! 0 |  538 | `		*zRest = zSrc;` |
|     ! 0 |  539 | `	}` |
|   17136 |  540 | `	if( pOutVal ){` |
|   17136 |  541 | `		*(sxreal *)pOutVal = Val;` |
|    8556 |  542 | `	}` |
|   17136 |  543 | `	return zSrc >= zEnd ? SXRET_OK : SXERR_SYNTAX;` |
|       5 |  544 | `}` |
|       - |  545 |  |
