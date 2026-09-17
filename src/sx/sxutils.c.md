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
|      82 |   12 | `PH7_PRIVATE sxi32 SyStrIsNumeric(const char *zSrc,sxu32 nLen,sxu8 *pReal,const char  **pzTail)` |
|       5 |   13 | `{` |
|       - |   14 | `	const char *zCur,*zEnd;` |
|       - |   15 | `#ifdef UNTRUST` |
|       - |   16 | `	if( SX_EMPTY_STR(zSrc) ){` |
|       - |   17 | `		return SXERR_EMPTY;` |
|       - |   18 | `	}` |
|       - |   19 | `#endif` |
|      87 |   20 | `	zEnd = &zSrc[nLen];` |
|       - |   21 | `	/* Jump leading white spaces */` |
|      91 |   22 | `	while( zSrc < zEnd && (unsigned char)zSrc[0] < 0xc0  && SyisSpace(zSrc[0]) ){` |
|       5 |   23 | `		zSrc++;` |
|       1 |   24 | `	}` |
|      87 |   25 | `	if( zSrc < zEnd && (zSrc[0] == '+' \|\| zSrc[0] == '-') ){` |
|       7 |   26 | `		zSrc++;` |
|       3 |   27 | `	}` |
|      87 |   28 | `	zCur = zSrc;` |
|      87 |   29 | `	if( pReal ){` |
|      63 |   30 | `		*pReal = FALSE;` |
|      29 |   31 | `	}` |
|      41 |   32 | `	for(;;){` |
|      87 |   33 | `		if( zSrc >= zEnd \|\| (unsigned char)zSrc[0] >= 0xc0 \|\| !SyisDigit(zSrc[0]) ){` |
|      24 |   34 | `			break;` |
|       - |   35 | `		}` |
|      45 |   36 | `		zSrc++;` |
|      45 |   37 | `		if( zSrc >= zEnd \|\| (unsigned char)zSrc[0] >= 0xc0 \|\| !SyisDigit(zSrc[0]) ){` |
|      21 |   38 | `			break;` |
|       - |   39 | `		}` |
|       7 |   40 | `		zSrc++;` |
|       7 |   41 | `		if( zSrc >= zEnd \|\| (unsigned char)zSrc[0] >= 0xc0 \|\| !SyisDigit(zSrc[0]) ){` |
|       3 |   42 | `			break;` |
|       - |   43 | `		}` |
|       3 |   44 | `		zSrc++;` |
|       3 |   45 | `		if( zSrc >= zEnd \|\| (unsigned char)zSrc[0] >= 0xc0 \|\| !SyisDigit(zSrc[0]) ){` |
|       2 |   46 | `			break;` |
|       - |   47 | `		}` |
|     ! 0 |   48 | `		zSrc++;` |
|     ! 0 |   49 | `	};` |
|      87 |   50 | `	if( zSrc < zEnd && zSrc > zCur ){` |
|      19 |   51 | `		int c = zSrc[0];` |
|      19 |   52 | `		if( c == '.' ){` |
|      17 |   53 | `			zSrc++;` |
|      17 |   54 | `			if( pReal ){` |
|      17 |   55 | `				*pReal = TRUE;` |
|       8 |   56 | `			}` |
|      17 |   57 | `			if( pzTail ){` |
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
|       4 |   70 | `			}` |
|      11 |   71 | `		}else if( c == 'e' \|\| c == 'E' ){` |
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
|       9 |   85 | `	}` |
|      87 |   86 | `	if( pzTail ){` |
|       - |   87 | `		/* Point to the non numeric part */` |
|      34 |   88 | `		*pzTail = zSrc;` |
|      15 |   89 | `	}` |
|      87 |   90 | `	return zSrc > zCur ? SXRET_OK /* String prefix is numeric */ : SXERR_INVALID /* Not a digit stream */;` |
|       5 |   91 | `}` |
|       - |   92 | `#define SXINT32_MIN_STR		"2147483648"` |
|       - |   93 | `#define SXINT32_MAX_STR		"2147483647"` |
|       2 |   94 | `PH7_PRIVATE sxi32 SyStrToInt32(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)` |
|     ! 0 |   95 | `{` |
|       2 |   96 | `	int isNeg = FALSE;` |
|       - |   97 | `	const char *zEnd;` |
|       2 |   98 | `	sxi32 nVal = 0;` |
|       - |   99 | `	sxi16 i;` |
|       - |  100 | `#if defined(UNTRUST)` |
|       - |  101 | `	if( SX_EMPTY_STR(zSrc) ){` |
|       - |  102 | `		if( pOutVal ){` |
|       - |  103 | `			*(sxi32 *)pOutVal = 0;` |
|       - |  104 | `		}` |
|       - |  105 | `		return SXERR_EMPTY;` |
|       - |  106 | `	}` |
|       - |  107 | `#endif` |
|       2 |  108 | `	zEnd = &zSrc[nLen];` |
|       2 |  109 | `	while(zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|     ! 0 |  110 | `		zSrc++;` |
|     ! 0 |  111 | `	}` |
|       2 |  112 | `	if( zSrc < zEnd && ( zSrc[0] == '-' \|\| zSrc[0] == '+' ) ){` |
|     ! 0 |  113 | `		isNeg = (zSrc[0] == '-') ? TRUE :FALSE;` |
|     ! 0 |  114 | `		zSrc++;` |
|     ! 0 |  115 | `	}` |
|       - |  116 | `	/* Skip leading zero */` |
|       2 |  117 | `	while(zSrc < zEnd && zSrc[0] == '0' ){` |
|     ! 0 |  118 | `		zSrc++;` |
|     ! 0 |  119 | `	}` |
|       2 |  120 | `	i = 10;` |
|       2 |  121 | `	if( (sxu32)(zEnd-zSrc) >= 10 ){` |
|       - |  122 | `		/* Handle overflow */` |
|     ! 0 |  123 | `		i = SyMemcmp(zSrc,(isNeg == TRUE) ? SXINT32_MIN_STR : SXINT32_MAX_STR,nLen) <= 0 ? 10 : 9;` |
|     ! 0 |  124 | `	}` |
|       2 |  125 | `	for(;;){` |
|       4 |  126 | `		if(zSrc >= zEnd \|\| !i \|\| !SyisDigit(zSrc[0])){ break; } nVal = nVal * 10 + ( zSrc[0] - '0' ) ; --i ; zSrc++;` |
|       2 |  127 | `		if(zSrc >= zEnd \|\| !i \|\| !SyisDigit(zSrc[0])){` |
|     ! 0 |  128 | `			break;` |
|       - |  129 | `		}` |
|       2 |  130 | `		nVal = nVal * 10 + ( zSrc[0] - '0' );` |
|       2 |  131 | `		--i;` |
|       2 |  132 | `		zSrc++;` |
|       2 |  133 | `		if(zSrc >= zEnd \|\| !i \|\| !SyisDigit(zSrc[0])){` |
|     ! 0 |  134 | `			break;` |
|       - |  135 | `		}` |
|       2 |  136 | `		nVal = nVal * 10 + ( zSrc[0] - '0' );` |
|       2 |  137 | `		--i;` |
|       2 |  138 | `		zSrc++;` |
|       2 |  139 | `		if(zSrc >= zEnd \|\| !i \|\| !SyisDigit(zSrc[0])){` |
|     ! 0 |  140 | `			break;` |
|       - |  141 | `		}` |
|       2 |  142 | `		nVal = nVal * 10 + ( zSrc[0] - '0' );` |
|       2 |  143 | `		--i;` |
|       2 |  144 | `		zSrc++;` |
|       2 |  145 | `		if(zSrc >= zEnd \|\| !i \|\| !SyisDigit(zSrc[0])){` |
|     ! 0 |  146 | `			break;` |
|       - |  147 | `		}` |
|       2 |  148 | `		nVal = nVal * 10 + ( zSrc[0] - '0' );` |
|       2 |  149 | `		--i;` |
|       2 |  150 | `		zSrc++;` |
|     ! 0 |  151 | `	}` |
|       - |  152 | `	/* Skip trailing spaces */` |
|       2 |  153 | `	while(zSrc < zEnd && SyisSpace(zSrc[0])){` |
|     ! 0 |  154 | `		zSrc++;` |
|     ! 0 |  155 | `	}` |
|       2 |  156 | `	if( zRest ){` |
|     ! 0 |  157 | `		*zRest = (char *)zSrc;` |
|     ! 0 |  158 | `	}` |
|       2 |  159 | `	if( pOutVal ){` |
|       2 |  160 | `		if( isNeg == TRUE && nVal != 0 ){` |
|     ! 0 |  161 | `			nVal = -nVal;` |
|     ! 0 |  162 | `		}` |
|       2 |  163 | `		*(sxi32 *)pOutVal = nVal;` |
|       1 |  164 | `	}` |
|       2 |  165 | `	return (zSrc >= zEnd) ? SXRET_OK : SXERR_SYNTAX;` |
|     ! 0 |  166 | `}` |
| 2322337 |  167 | `PH7_PRIVATE sxi32 SyStrToInt64(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)` |
|       5 |  168 | `{` |
| 2322342 |  169 | `	int isNeg = FALSE;` |
|       - |  170 | `	const char *zEnd;` |
|       - |  171 | `	sxi64 nVal;` |
|       - |  172 | `	/* Magnitude accumulated unsigned so overflow can be detected and the result` |
|       - |  173 | `	 * saturated (PHP casts an out-of-range numeric string to PHP_INT_MAX/MIN)` |
|       - |  174 | `	 * rather than the digits being dropped. cutoff is the largest magnitude that` |
|       - |  175 | `	 * fits: PHP_INT_MAX for a positive value, \|PHP_INT_MIN\| == 2^63 for a` |
|       - |  176 | `	 * negative one. */` |
|       - |  177 | `	sxu64 uVal, cutoff;` |
| 2322342 |  178 | `	int bOverflow = FALSE;` |
|       - |  179 | `#if defined(UNTRUST)` |
|       - |  180 | `	if( SX_EMPTY_STR(zSrc) ){` |
|       - |  181 | `		if( pOutVal ){` |
|       - |  182 | `			*(sxi32 *)pOutVal = 0;` |
|       - |  183 | `		}` |
|       - |  184 | `		return SXERR_EMPTY;` |
|       - |  185 | `	}` |
|       - |  186 | `#endif` |
| 2322342 |  187 | `	zEnd = &zSrc[nLen];` |
| 2322364 |  188 | `	while(zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|      24 |  189 | `		zSrc++;` |
|       2 |  190 | `	}` |
| 2322342 |  191 | `	if( zSrc < zEnd && ( zSrc[0] == '-' \|\| zSrc[0] == '+' ) ){` |
|      33 |  192 | `		isNeg = (zSrc[0] == '-') ? TRUE :FALSE;` |
|      33 |  193 | `		zSrc++;` |
|      15 |  194 | `	}` |
|       - |  195 | `	/* Skip leading zero */` |
| 2323710 |  196 | `	while(zSrc < zEnd && zSrc[0] == '0' ){` |
|    1369 |  197 | `		zSrc++;` |
|       1 |  198 | `	}` |
| 2322342 |  199 | `	cutoff = isNeg ? ((sxu64)SXI64_HIGH + 1) : (sxu64)SXI64_HIGH;` |
| 2322342 |  200 | `	uVal = 0;` |
| 5613203 |  201 | `	while( zSrc < zEnd && (unsigned char)zSrc[0] < 0xc0 && SyisDigit(zSrc[0]) ){` |
| 3290866 |  202 | `		int d = zSrc[0] - '0';` |
| 3290866 |  203 | `		if( uVal > cutoff / 10 \|\| (uVal == cutoff / 10 && (sxu64)d > cutoff % 10) ){` |
|      33 |  204 | `			bOverflow = TRUE;` |
|      17 |  205 | `		}else{` |
| 3290834 |  206 | `			uVal = uVal * 10 + (sxu64)d;` |
|       - |  207 | `		}` |
| 3290866 |  208 | `		zSrc++;` |
|       5 |  209 | `	}` |
| 2322342 |  210 | `	if( bOverflow ){` |
|      25 |  211 | `		uVal = cutoff;` |
|      12 |  212 | `	}` |
|       - |  213 | `	/* Skip trailing spaces */` |
| 2322364 |  214 | `	while(zSrc < zEnd && SyisSpace(zSrc[0])){` |
|      24 |  215 | `		zSrc++;` |
|       2 |  216 | `	}` |
| 2322342 |  217 | `	if( zRest ){` |
|     ! 0 |  218 | `		*zRest = (char *)zSrc;` |
|     ! 0 |  219 | `	}` |
| 2322342 |  220 | `	if( pOutVal ){` |
| 2322342 |  221 | `		if( isNeg ){` |
|       - |  222 | `			/* uVal <= 2^63; the cap value 2^63 is PHP_INT_MIN and has no positive` |
|       - |  223 | `			 * sxi64 representation, so materialize it directly to dodge UB. */` |
|      33 |  224 | `			nVal = ( uVal > (sxu64)SXI64_HIGH ) ? (-SXI64_HIGH - 1) : -(sxi64)uVal;` |
|      18 |  225 | `		}else{` |
| 2322312 |  226 | `			nVal = (sxi64)uVal;` |
|       - |  227 | `		}` |
| 2322342 |  228 | `		*(sxi64 *)pOutVal = nVal;` |
| 1161168 |  229 | `	}` |
| 2322342 |  230 | `	return (zSrc >= zEnd) ? SXRET_OK : SXERR_SYNTAX;` |
|       5 |  231 | `}` |
|  323946 |  232 | `PH7_PRIVATE sxi32 SyHexToint(sxi32 c)` |
|       5 |  233 | `{` |
|  323951 |  234 | `	switch(c){` |
|  132379 |  235 | `	case '0': return 0;` |
|   11729 |  236 | `	case '1': return 1;` |
|   77741 |  237 | `	case '2': return 2;` |
|    7851 |  238 | `	case '3': return 3;` |
|    3933 |  239 | `	case '4': return 4;` |
|   11679 |  240 | `	case '5': return 5;` |
|    7805 |  241 | `	case '6': return 6;` |
|    3911 |  242 | `	case '7': return 7;` |
|   19497 |  243 | `	case '8': return 8;` |
|   11753 |  244 | `	case '9': return 9;` |
|   11755 |  245 | `	case 'A': case 'a': return 10;` |
|    3921 |  246 | `	case 'B': case 'b': return 11;` |
|    4009 |  247 | `	case 'C': case 'c': return 12;` |
|    3915 |  248 | `	case 'D': case 'd': return 13;` |
|    3985 |  249 | `	case 'E': case 'e': return 14;` |
|    8161 |  250 | `	case 'F': case 'f': return 15;` |
|       - |  251 | `	}` |
|       3 |  252 | `	return -1;` |
|  161978 |  253 | `}` |
|  104958 |  254 | `PH7_PRIVATE sxi32 SyHexStrToInt64(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)` |
|       5 |  255 | `{` |
|       - |  256 | `	const char *zIn,*zEnd;` |
|  104963 |  257 | `	int isNeg = FALSE;` |
|  104963 |  258 | `	sxi64 nVal = 0;` |
|       - |  259 | `#if defined(UNTRUST)` |
|       - |  260 | `	if( SX_EMPTY_STR(zSrc) ){` |
|       - |  261 | `		if( pOutVal ){` |
|       - |  262 | `			*(sxi32 *)pOutVal = 0;` |
|       - |  263 | `		}` |
|       - |  264 | `		return SXERR_EMPTY;` |
|       - |  265 | `	}` |
|       - |  266 | `#endif` |
|  104963 |  267 | `	zEnd = &zSrc[nLen];` |
|  104963 |  268 | `	while( zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|     ! 0 |  269 | `		zSrc++;` |
|     ! 0 |  270 | `	}` |
|  104963 |  271 | `	if( zSrc < zEnd && ( *zSrc == '-' \|\| *zSrc == '+' ) ){` |
|     ! 0 |  272 | `		isNeg = (zSrc[0] == '-') ? TRUE :FALSE;` |
|     ! 0 |  273 | `		zSrc++;` |
|     ! 0 |  274 | `	}` |
|  104963 |  275 | `	if( zSrc < &zEnd[-2] && zSrc[0] == '0' && (zSrc[1] == 'x' \|\| zSrc[1] == 'X') ){` |
|       - |  276 | `		/* Bypass hex prefix */` |
|  104963 |  277 | `		zSrc += sizeof(char) * 2;` |
|   52479 |  278 | `	}` |
|       - |  279 | `	/* Skip leading zero */` |
|  132159 |  280 | `	while(zSrc < zEnd && zSrc[0] == '0' ){` |
|   27201 |  281 | `		zSrc++;` |
|       5 |  282 | `	}` |
|  104963 |  283 | `	zIn = zSrc;` |
|   87476 |  284 | `	for(;;){` |
|  174957 |  285 | `		if(zSrc >= zEnd \|\| !SyisHex(zSrc[0]) \|\| (int)(zSrc-zIn) > 15){` |
|   36930 |  286 | `			break;` |
|       - |  287 | `		}` |
|  101107 |  288 | `		nVal = nVal * 16 + SyHexToint(zSrc[0]);` |
|  101107 |  289 | `		zSrc++;` |
|  101107 |  290 | `		if(zSrc >= zEnd \|\| !SyisHex(zSrc[0]) \|\| (int)(zSrc-zIn) > 15){` |
|    9721 |  291 | `			break;` |
|       - |  292 | `		}` |
|   81675 |  293 | `		nVal = nVal * 16 + SyHexToint(zSrc[0]);` |
|   81675 |  294 | `		zSrc++;` |
|   81675 |  295 | `		if(zSrc >= zEnd \|\| !SyisHex(zSrc[0]) \|\| (int)(zSrc-zIn) > 15){` |
|    5843 |  296 | `			break;` |
|       - |  297 | `		}` |
|   69999 |  298 | `		nVal = nVal * 16 + SyHexToint(zSrc[0]);` |
|   69999 |  299 | `		zSrc++;` |
|   69999 |  300 | `		if(zSrc >= zEnd \|\| !SyisHex(zSrc[0]) \|\| (int)(zSrc-zIn) > 15){` |
|     ! 0 |  301 | `			break;` |
|       - |  302 | `		}` |
|   69999 |  303 | `		nVal = nVal * 16 + SyHexToint(zSrc[0]);` |
|   69999 |  304 | `		zSrc++;` |
|       5 |  305 | `	}` |
|  104963 |  306 | `	while( zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|     ! 0 |  307 | `		zSrc++;` |
|     ! 0 |  308 | `	}` |
|  104963 |  309 | `	if( zRest ){` |
|     ! 0 |  310 | `		*zRest = zSrc;` |
|     ! 0 |  311 | `	}` |
|  104963 |  312 | `	if( pOutVal ){` |
|  104963 |  313 | `		if( isNeg == TRUE && nVal != 0 ){` |
|     ! 0 |  314 | `			nVal = -nVal;` |
|     ! 0 |  315 | `		}` |
|  104963 |  316 | `		*(sxi64 *)pOutVal = nVal;` |
|   52479 |  317 | `	}` |
|  104963 |  318 | `	return zSrc >= zEnd ? SXRET_OK : SXERR_SYNTAX;` |
|       5 |  319 | `}` |
|    3978 |  320 | `PH7_PRIVATE sxi32 SyOctalStrToInt64(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)` |
|       5 |  321 | `{` |
|       - |  322 | `	const char *zIn,*zEnd;` |
|    3983 |  323 | `	int isNeg = FALSE;` |
|    3983 |  324 | `	sxi64 nVal = 0;` |
|       - |  325 | `	int c;` |
|       - |  326 | `#if defined(UNTRUST)` |
|       - |  327 | `	if( SX_EMPTY_STR(zSrc) ){` |
|       - |  328 | `		if( pOutVal ){` |
|       - |  329 | `			*(sxi32 *)pOutVal = 0;` |
|       - |  330 | `		}` |
|       - |  331 | `		return SXERR_EMPTY;` |
|       - |  332 | `	}` |
|       - |  333 | `#endif` |
|    3983 |  334 | `	zEnd = &zSrc[nLen];` |
|    3983 |  335 | `	while(zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|     ! 0 |  336 | `		zSrc++;` |
|     ! 0 |  337 | `	}` |
|    3983 |  338 | `	if( zSrc < zEnd && ( zSrc[0] == '-' \|\| zSrc[0] == '+' ) ){` |
|     ! 0 |  339 | `		isNeg = (zSrc[0] == '-') ? TRUE :FALSE;` |
|     ! 0 |  340 | `		zSrc++;` |
|     ! 0 |  341 | `	}` |
|       - |  342 | `	/* Skip leading zero */` |
|    7951 |  343 | `	while(zSrc < zEnd && zSrc[0] == '0' ){` |
|    3973 |  344 | `		zSrc++;` |
|       5 |  345 | `	}` |
|    3983 |  346 | `	zIn = zSrc;` |
|    1999 |  347 | `	for(;;){` |
|    4003 |  348 | `		if(zSrc >= zEnd \|\| !SyisDigit(zSrc[0])){ break; } if( (c=zSrc[0]-'0') > 7 \|\| (int)(zSrc-zIn) > 20){ break;} nVal = nVal * 8 +  c; zSrc++;` |
|    3999 |  349 | `		if(zSrc >= zEnd \|\| !SyisDigit(zSrc[0])){ break; } if( (c=zSrc[0]-'0') > 7 \|\| (int)(zSrc-zIn) > 20){ break;} nVal = nVal * 8 +  c; zSrc++;` |
|    3987 |  350 | `		if(zSrc >= zEnd \|\| !SyisDigit(zSrc[0])){ break; } if( (c=zSrc[0]-'0') > 7 \|\| (int)(zSrc-zIn) > 20){ break;} nVal = nVal * 8 +  c; zSrc++;` |
|    3959 |  351 | `		if(zSrc >= zEnd \|\| !SyisDigit(zSrc[0])){ break; } if( (c=zSrc[0]-'0') > 7 \|\| (int)(zSrc-zIn) > 20){ break;} nVal = nVal * 8 +  c; zSrc++;` |
|       1 |  352 | `	}` |
|       - |  353 | `	/* Skip trailing spaces */` |
|    3983 |  354 | `	while(zSrc < zEnd && SyisSpace(zSrc[0])){` |
|     ! 0 |  355 | `		zSrc++;` |
|     ! 0 |  356 | `	}` |
|    3983 |  357 | `	if( zRest ){` |
|     ! 0 |  358 | `		*zRest = zSrc;` |
|     ! 0 |  359 | `	}` |
|    3983 |  360 | `	if( pOutVal ){` |
|    3983 |  361 | `		if( isNeg == TRUE && nVal != 0 ){` |
|     ! 0 |  362 | `			nVal = -nVal;` |
|     ! 0 |  363 | `		}` |
|    3983 |  364 | `		*(sxi64 *)pOutVal = nVal;` |
|    1989 |  365 | `	}` |
|    3983 |  366 | `	return (zSrc >= zEnd) ? SXRET_OK : SXERR_SYNTAX;` |
|       5 |  367 | `}` |
|     284 |  368 | `PH7_PRIVATE sxi32 SyBinaryStrToInt64(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)` |
|       1 |  369 | `{` |
|       - |  370 | `	const char *zIn,*zEnd;` |
|     285 |  371 | `	int isNeg = FALSE;` |
|     285 |  372 | `	sxi64 nVal = 0;` |
|       - |  373 | `	int c;` |
|       - |  374 | `#if defined(UNTRUST)` |
|       - |  375 | `	if( SX_EMPTY_STR(zSrc) ){` |
|       - |  376 | `		if( pOutVal ){` |
|       - |  377 | `			*(sxi32 *)pOutVal = 0;` |
|       - |  378 | `		}` |
|       - |  379 | `		return SXERR_EMPTY;` |
|       - |  380 | `	}` |
|       - |  381 | `#endif` |
|     285 |  382 | `	zEnd = &zSrc[nLen];` |
|     285 |  383 | `	while(zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|     ! 0 |  384 | `		zSrc++;` |
|     ! 0 |  385 | `	}` |
|     285 |  386 | `	if( zSrc < zEnd && ( zSrc[0] == '-' \|\| zSrc[0] == '+' ) ){` |
|     ! 0 |  387 | `		isNeg = (zSrc[0] == '-') ? TRUE :FALSE;` |
|     ! 0 |  388 | `		zSrc++;` |
|     ! 0 |  389 | `	}` |
|     285 |  390 | `	if( zSrc < &zEnd[-2] && zSrc[0] == '0' && (zSrc[1] == 'b' \|\| zSrc[1] == 'B') ){` |
|       - |  391 | `		/* Bypass binary prefix */` |
|     285 |  392 | `		zSrc += sizeof(char) * 2;` |
|     142 |  393 | `	}` |
|       - |  394 | `	/* Skip leading zero */` |
|     333 |  395 | `	while(zSrc < zEnd && zSrc[0] == '0' ){` |
|      49 |  396 | `		zSrc++;` |
|       1 |  397 | `	}` |
|     285 |  398 | `	zIn = zSrc;` |
|     314 |  399 | `	for(;;){` |
|     629 |  400 | `		if(zSrc >= zEnd \|\| (zSrc[0] != '1' && zSrc[0] != '0') \|\| (int)(zSrc-zIn) > 62){ break; } c = zSrc[0] - '0'; nVal = (nVal << 1) + c; zSrc++;` |
|     461 |  401 | `		if(zSrc >= zEnd \|\| (zSrc[0] != '1' && zSrc[0] != '0') \|\| (int)(zSrc-zIn) > 62){ break; } c = zSrc[0] - '0'; nVal = (nVal << 1) + c; zSrc++;` |
|     429 |  402 | `		if(zSrc >= zEnd \|\| (zSrc[0] != '1' && zSrc[0] != '0') \|\| (int)(zSrc-zIn) > 62){ break; } c = zSrc[0] - '0'; nVal = (nVal << 1) + c; zSrc++;` |
|     383 |  403 | `		if(zSrc >= zEnd \|\| (zSrc[0] != '1' && zSrc[0] != '0') \|\| (int)(zSrc-zIn) > 62){ break; } c = zSrc[0] - '0'; nVal = (nVal << 1) + c; zSrc++;` |
|       1 |  404 | `	}` |
|       - |  405 | `	/* Skip trailing spaces */` |
|     285 |  406 | `	while(zSrc < zEnd && SyisSpace(zSrc[0])){` |
|     ! 0 |  407 | `		zSrc++;` |
|     ! 0 |  408 | `	}` |
|     285 |  409 | `	if( zRest ){` |
|     ! 0 |  410 | `		*zRest = zSrc;` |
|     ! 0 |  411 | `	}` |
|     285 |  412 | `	if( pOutVal ){` |
|     285 |  413 | `		if( isNeg == TRUE && nVal != 0 ){` |
|     ! 0 |  414 | `			nVal = -nVal;` |
|     ! 0 |  415 | `		}` |
|     285 |  416 | `		*(sxi64 *)pOutVal = nVal;` |
|     142 |  417 | `	}` |
|     285 |  418 | `	return (zSrc >= zEnd) ? SXRET_OK : SXERR_SYNTAX;` |
|       1 |  419 | `}` |
|    8848 |  420 | `PH7_PRIVATE sxi32 SyStrToReal(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)` |
|       5 |  421 | `{` |
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
|    8853 |  435 | `	const char *zEnd = &zSrc[nLen];` |
|       - |  436 | `	const char *zNum;` |
|    8853 |  437 | `	const char *zExpStart = 0;` |
|    8853 |  438 | `	sxreal Val = 0.0;` |
|    8853 |  439 | `	sxu32 nCopy = 0;` |
|    8853 |  440 | `	int bDigit = 0;` |
|       - |  441 | `#ifdef UNTRUST` |
|       - |  442 | `	if( SX_EMPTY_STR(zSrc)  ){` |
|       - |  443 | `		if( pOutVal ){` |
|       - |  444 | `			*(sxreal *)pOutVal = 0.0;` |
|       - |  445 | `		}` |
|       - |  446 | `		return SXERR_EMPTY;` |
|       - |  447 | `	}` |
|       - |  448 | `#endif` |
|       - |  449 | `	/* Skip leading spaces */` |
|    8857 |  450 | `	while( zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|       5 |  451 | `		zSrc++;` |
|       1 |  452 | `	}` |
|    8853 |  453 | `	zNum = zSrc;` |
|       - |  454 | `	/* Sign (if exists) */` |
|    8853 |  455 | `	if( zSrc < zEnd && (zSrc[0] == '-' \|\| zSrc[0] == '+' ) ){` |
|      14 |  456 | `		zSrc++;` |
|       6 |  457 | `	}` |
|       - |  458 | `	/* Integer part */` |
|   18739 |  459 | `	while( zSrc < zEnd && SyisDigit(zSrc[0]) ){` |
|    9891 |  460 | `		bDigit = 1;` |
|    9891 |  461 | `		zSrc++;` |
|       5 |  462 | `	}` |
|       - |  463 | `	/* Fractional part */` |
|    8853 |  464 | `	if( zSrc < zEnd && ( zSrc[0] == '.' \|\| zSrc[0] == ',' ) ){` |
|    8701 |  465 | `		zSrc++;` |
|   19979 |  466 | `		while( zSrc < zEnd && SyisDigit(zSrc[0]) ){` |
|   11283 |  467 | `			bDigit = 1;` |
|   11283 |  468 | `			zSrc++;` |
|       5 |  469 | `		}` |
|    4348 |  470 | `	}` |
|       - |  471 | `	/* Exponent — consumed only when it carries at least one digit, like` |
|       - |  472 | `	 * strtod, so "1e+x" leaves the "e+" unconsumed. */` |
|    8853 |  473 | `	if( bDigit && zSrc < zEnd && ( zSrc[0] == 'e' \|\| zSrc[0] == 'E' ) ){` |
|     193 |  474 | `		const char *zExp = &zSrc[1];` |
|     193 |  475 | `		if( zExp < zEnd && (zExp[0] == '-' \|\| zExp[0] == '+') ){` |
|      72 |  476 | `			zExp++;` |
|      35 |  477 | `		}` |
|     193 |  478 | `		if( zExp < zEnd && SyisDigit(zExp[0]) ){` |
|     193 |  479 | `			zExpStart = zSrc;` |
|     193 |  480 | `			zSrc = zExp;` |
|     579 |  481 | `			while( zSrc < zEnd && SyisDigit(zSrc[0]) ){` |
|     389 |  482 | `				zSrc++;` |
|       3 |  483 | `			}` |
|      95 |  484 | `		}` |
|      95 |  485 | `	}` |
|    8853 |  486 | `	if( bDigit ){` |
|    8853 |  487 | `		sxu32 i, nSpan = (sxu32)((zExpStart ? zExpStart : zSrc) - zNum);` |
|    8853 |  488 | `		sxu32 nExp = zExpStart ? (sxu32)(zSrc - zExpStart) : 0;` |
|    8853 |  489 | `		char *zDup = zBuf;` |
|    8853 |  490 | `		sxu32 nDup = sizeof(zBuf);` |
|    8853 |  491 | `		if( nSpan + nExp >= sizeof(zBuf) ){` |
|       3 |  492 | `			char *zHeap = (char *)malloc(nSpan + nExp + 1);` |
|       3 |  493 | `			if( zHeap ){` |
|       3 |  494 | `				zDup = zHeap;` |
|       3 |  495 | `				nDup = nSpan + nExp + 1;` |
|       1 |  496 | `			}` |
|       1 |  497 | `		}` |
|       - |  498 | `		{` |
|    8853 |  499 | `			sxu32 nMantMax = nDup - 1 - (nExp < nDup - 1 ? nExp : 0);` |
|   38725 |  500 | `			for( i = 0 ; i < nSpan && nCopy < nMantMax ; i++ ){` |
|   29877 |  501 | `				zDup[nCopy++] = (zNum[i] == ',') ? '.' : zNum[i];` |
|   14941 |  502 | `			}` |
|       - |  503 | `			/* The exponent rides behind even a truncated mantissa: dropping` |
|       - |  504 | `			 * it would collapse "0.<hundreds of zeros>1e300" to 0.0. */` |
|    9499 |  505 | `			for( i = 0 ; i < nExp && nCopy < nDup - 1 ; i++ ){` |
|     649 |  506 | `				zDup[nCopy++] = zExpStart[i];` |
|     326 |  507 | `			}` |
|       - |  508 | `		}` |
|    8853 |  509 | `		zDup[nCopy] = 0;` |
|    8853 |  510 | `		Val = (sxreal)strtod(zDup,0);` |
|    8853 |  511 | `		if( zDup != zBuf ){` |
|       3 |  512 | `			free(zDup);` |
|       1 |  513 | `		}` |
|    4424 |  514 | `	}` |
|       - |  515 | `	/* Jump trailing spaces */` |
|    8853 |  516 | `	while( zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|     ! 0 |  517 | `		zSrc++;` |
|     ! 0 |  518 | `	}` |
|    8853 |  519 | `	if( zRest ){` |
|     ! 0 |  520 | `		*zRest = zSrc;` |
|     ! 0 |  521 | `	}` |
|    8853 |  522 | `	if( pOutVal ){` |
|    8853 |  523 | `		*(sxreal *)pOutVal = Val;` |
|    4424 |  524 | `	}` |
|    8853 |  525 | `	return zSrc >= zEnd ? SXRET_OK : SXERR_SYNTAX;` |
|       5 |  526 | `}` |
|       - |  527 |  |
