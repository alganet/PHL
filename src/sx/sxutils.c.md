# src/sx/sxutils.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 318/398 lines (79.90%)

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
|      60 |   12 | `PH7_PRIVATE sxi32 SyStrIsNumeric(const char *zSrc,sxu32 nLen,sxu8 *pReal,const char  **pzTail)` |
|       4 |   13 | `{` |
|       - |   14 | `	const char *zCur,*zEnd;` |
|       - |   15 | `#ifdef UNTRUST` |
|       - |   16 | `	if( SX_EMPTY_STR(zSrc) ){` |
|       - |   17 | `		return SXERR_EMPTY;` |
|       - |   18 | `	}` |
|       - |   19 | `#endif` |
|      64 |   20 | `	zEnd = &zSrc[nLen];` |
|       - |   21 | `	/* Jump leading white spaces */` |
|      70 |   22 | `	while( zSrc < zEnd && (unsigned char)zSrc[0] < 0xc0  && SyisSpace(zSrc[0]) ){` |
|       7 |   23 | `		zSrc++;` |
|       1 |   24 | `	}` |
|      64 |   25 | `	if( zSrc < zEnd && (zSrc[0] == '+' \|\| zSrc[0] == '-') ){` |
|       7 |   26 | `		zSrc++;` |
|       3 |   27 | `	}` |
|      64 |   28 | `	zCur = zSrc;` |
|      64 |   29 | `	if( pReal ){` |
|      58 |   30 | `		*pReal = FALSE;` |
|      27 |   31 | `	}` |
|      30 |   32 | `	for(;;){` |
|      64 |   33 | `		if( zSrc >= zEnd \|\| (unsigned char)zSrc[0] >= 0xc0 \|\| !SyisDigit(zSrc[0]) ){` |
|       7 |   34 | `			break;` |
|       - |   35 | `		}` |
|      55 |   36 | `		zSrc++;` |
|      55 |   37 | `		if( zSrc >= zEnd \|\| (unsigned char)zSrc[0] >= 0xc0 \|\| !SyisDigit(zSrc[0]) ){` |
|      26 |   38 | `			break;` |
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
|      64 |   50 | `	if( zSrc < zEnd && zSrc > zCur ){` |
|      15 |   51 | `		int c = zSrc[0];` |
|      15 |   52 | `		if( c == '.' ){` |
|      13 |   53 | `			zSrc++;` |
|      13 |   54 | `			if( pReal ){` |
|      13 |   55 | `				*pReal = TRUE;` |
|       6 |   56 | `			}` |
|      13 |   57 | `			if( pzTail ){` |
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
|       9 |   71 | `		}else if( c == 'e' \|\| c == 'E' ){` |
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
|       7 |   85 | `	}` |
|      64 |   86 | `	if( pzTail ){` |
|       - |   87 | `		/* Point to the non numeric part */` |
|      38 |   88 | `		*pzTail = zSrc;` |
|      17 |   89 | `	}` |
|      64 |   90 | `	return zSrc > zCur ? SXRET_OK /* String prefix is numeric */ : SXERR_INVALID /* Not a digit stream */;` |
|       4 |   91 | `}` |
|       - |   92 | `#define SXINT32_MIN_STR		"2147483648"` |
|       - |   93 | `#define SXINT32_MAX_STR		"2147483647"` |
|      10 |   94 | `PH7_PRIVATE sxi32 SyStrToInt32(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)` |
|       2 |   95 | `{` |
|      12 |   96 | `	int isNeg = FALSE;` |
|       - |   97 | `	const char *zEnd;` |
|      12 |   98 | `	sxi32 nVal = 0;` |
|       - |   99 | `	sxi16 i;` |
|       - |  100 | `#if defined(UNTRUST)` |
|       - |  101 | `	if( SX_EMPTY_STR(zSrc) ){` |
|       - |  102 | `		if( pOutVal ){` |
|       - |  103 | `			*(sxi32 *)pOutVal = 0;` |
|       - |  104 | `		}` |
|       - |  105 | `		return SXERR_EMPTY;` |
|       - |  106 | `	}` |
|       - |  107 | `#endif` |
|      12 |  108 | `	zEnd = &zSrc[nLen];` |
|      12 |  109 | `	while(zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|     ! 0 |  110 | `		zSrc++;` |
|     ! 0 |  111 | `	}` |
|      12 |  112 | `	if( zSrc < zEnd && ( zSrc[0] == '-' \|\| zSrc[0] == '+' ) ){` |
|     ! 0 |  113 | `		isNeg = (zSrc[0] == '-') ? TRUE :FALSE;` |
|     ! 0 |  114 | `		zSrc++;` |
|     ! 0 |  115 | `	}` |
|       - |  116 | `	/* Skip leading zero */` |
|      12 |  117 | `	while(zSrc < zEnd && zSrc[0] == '0' ){` |
|     ! 0 |  118 | `		zSrc++;` |
|     ! 0 |  119 | `	}` |
|      12 |  120 | `	i = 10;` |
|      12 |  121 | `	if( (sxu32)(zEnd-zSrc) >= 10 ){` |
|       - |  122 | `		/* Handle overflow */` |
|     ! 0 |  123 | `		i = SyMemcmp(zSrc,(isNeg == TRUE) ? SXINT32_MIN_STR : SXINT32_MAX_STR,nLen) <= 0 ? 10 : 9;` |
|     ! 0 |  124 | `	}` |
|       6 |  125 | `	for(;;){` |
|      14 |  126 | `		if(zSrc >= zEnd \|\| !i \|\| !SyisDigit(zSrc[0])){ break; } nVal = nVal * 10 + ( zSrc[0] - '0' ) ; --i ; zSrc++;` |
|      12 |  127 | `		if(zSrc >= zEnd \|\| !i \|\| !SyisDigit(zSrc[0])){` |
|       6 |  128 | `			break;` |
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
|      12 |  153 | `	while(zSrc < zEnd && SyisSpace(zSrc[0])){` |
|     ! 0 |  154 | `		zSrc++;` |
|     ! 0 |  155 | `	}` |
|      12 |  156 | `	if( zRest ){` |
|     ! 0 |  157 | `		*zRest = (char *)zSrc;` |
|     ! 0 |  158 | `	}` |
|      12 |  159 | `	if( pOutVal ){` |
|      12 |  160 | `		if( isNeg == TRUE && nVal != 0 ){` |
|     ! 0 |  161 | `			nVal = -nVal;` |
|     ! 0 |  162 | `		}` |
|      12 |  163 | `		*(sxi32 *)pOutVal = nVal;` |
|       5 |  164 | `	}` |
|      12 |  165 | `	return (zSrc >= zEnd) ? SXRET_OK : SXERR_SYNTAX;` |
|       2 |  166 | `}` |
|  419268 |  167 | `PH7_PRIVATE sxi32 SyStrToInt64(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)` |
|       5 |  168 | `{` |
|  419273 |  169 | `	return SyStrToInt64Ex(zSrc,nLen,pOutVal,zRest,0);` |
|       5 |  170 | `}` |
|       - |  171 | `/*` |
|       - |  172 | ` * SyStrToInt64 plus the one fact its saturating reader threw away: whether the` |
|       - |  173 | ` * digit run ran PAST the int64 range. *pOverflow comes back 1 for the positive` |
|       - |  174 | ` * side, -1 for the negative one and 0 when the value fits. The integer handed` |
|       - |  175 | ` * back is still the saturated one, which is what php's (int) cast wants -- but a` |
|       - |  176 | ` * caller converting a numeric STRING to a NUMBER needs to know, because php's` |
|       - |  177 | ` * answer there is a float, not PHP_INT_MAX.` |
|       - |  178 | ` */` |
|  421504 |  179 | `PH7_PRIVATE sxi32 SyStrToInt64Ex(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest,int *pOverflow)` |
|       5 |  180 | `{` |
|  421509 |  181 | `	int isNeg = FALSE;` |
|       - |  182 | `	const char *zEnd;` |
|       - |  183 | `	sxi64 nVal;` |
|       - |  184 | `	/* Magnitude accumulated unsigned so overflow can be detected and the result` |
|       - |  185 | `	 * saturated (PHP casts an out-of-range numeric string to PHP_INT_MAX/MIN)` |
|       - |  186 | `	 * rather than the digits being dropped. cutoff is the largest magnitude that` |
|       - |  187 | `	 * fits: PHP_INT_MAX for a positive value, \|PHP_INT_MIN\| == 2^63 for a` |
|       - |  188 | `	 * negative one. */` |
|       - |  189 | `	sxu64 uVal, cutoff;` |
|  421509 |  190 | `	int bOverflow = FALSE;` |
|  421509 |  191 | `	if( pOverflow ){` |
|    1125 |  192 | `		*pOverflow = 0;` |
|     560 |  193 | `	}` |
|       - |  194 | `#if defined(UNTRUST)` |
|       - |  195 | `	if( SX_EMPTY_STR(zSrc) ){` |
|       - |  196 | `		if( pOutVal ){` |
|       - |  197 | `			*(sxi32 *)pOutVal = 0;` |
|       - |  198 | `		}` |
|       - |  199 | `		return SXERR_EMPTY;` |
|       - |  200 | `	}` |
|       - |  201 | `#endif` |
|  421509 |  202 | `	zEnd = &zSrc[nLen];` |
|  421579 |  203 | `	while(zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|      73 |  204 | `		zSrc++;` |
|       3 |  205 | `	}` |
|  421509 |  206 | `	if( zSrc < zEnd && ( zSrc[0] == '-' \|\| zSrc[0] == '+' ) ){` |
|     209 |  207 | `		isNeg = (zSrc[0] == '-') ? TRUE :FALSE;` |
|     209 |  208 | `		zSrc++;` |
|     103 |  209 | `	}` |
|       - |  210 | `	/* Skip leading zero */` |
|  421943 |  211 | `	while(zSrc < zEnd && zSrc[0] == '0' ){` |
|     439 |  212 | `		zSrc++;` |
|       5 |  213 | `	}` |
|  421509 |  214 | `	cutoff = isNeg ? ((sxu64)SXI64_HIGH + 1) : (sxu64)SXI64_HIGH;` |
|  421509 |  215 | `	uVal = 0;` |
| 1154319 |  216 | `	while( zSrc < zEnd && (unsigned char)zSrc[0] < 0xc0 && SyisDigit(zSrc[0]) ){` |
|  732815 |  217 | `		int d = zSrc[0] - '0';` |
|  732815 |  218 | `		if( uVal > cutoff / 10 \|\| (uVal == cutoff / 10 && (sxu64)d > cutoff % 10) ){` |
|   12883 |  219 | `			bOverflow = TRUE;` |
|    6442 |  220 | `		}else{` |
|  719933 |  221 | `			uVal = uVal * 10 + (sxu64)d;` |
|       - |  222 | `		}` |
|  732815 |  223 | `		zSrc++;` |
|       5 |  224 | `	}` |
|  421509 |  225 | `	if( bOverflow ){` |
|     585 |  226 | `		uVal = cutoff;` |
|     585 |  227 | `		if( pOverflow ){` |
|     557 |  228 | `			*pOverflow = isNeg ? -1 : 1;` |
|     278 |  229 | `		}` |
|     292 |  230 | `	}` |
|       - |  231 | `	/* Skip trailing spaces */` |
|  421569 |  232 | `	while(zSrc < zEnd && SyisSpace(zSrc[0])){` |
|      63 |  233 | `		zSrc++;` |
|       3 |  234 | `	}` |
|  421509 |  235 | `	if( zRest ){` |
|     ! 0 |  236 | `		*zRest = (char *)zSrc;` |
|     ! 0 |  237 | `	}` |
|  421509 |  238 | `	if( pOutVal ){` |
|  421509 |  239 | `		if( isNeg ){` |
|       - |  240 | `			/* uVal <= 2^63; the cap value 2^63 is PHP_INT_MIN and has no positive` |
|       - |  241 | `			 * sxi64 representation, so materialize it directly to dodge UB. */` |
|     187 |  242 | `			nVal = ( uVal > (sxu64)SXI64_HIGH ) ? (-SXI64_HIGH - 1) : -(sxi64)uVal;` |
|      95 |  243 | `		}else{` |
|  421325 |  244 | `			nVal = (sxi64)uVal;` |
|       - |  245 | `		}` |
|  421509 |  246 | `		*(sxi64 *)pOutVal = nVal;` |
|  210752 |  247 | `	}` |
|  421509 |  248 | `	return (zSrc >= zEnd) ? SXRET_OK : SXERR_SYNTAX;` |
|       5 |  249 | `}` |
|    6140 |  250 | `PH7_PRIVATE sxi32 SyHexToint(sxi32 c)` |
|       4 |  251 | `{` |
|    6144 |  252 | `	switch(c){` |
|     838 |  253 | `	case '0': return 0;` |
|     333 |  254 | `	case '1': return 1;` |
|     356 |  255 | `	case '2': return 2;` |
|     304 |  256 | `	case '3': return 3;` |
|     160 |  257 | `	case '4': return 4;` |
|     180 |  258 | `	case '5': return 5;` |
|     158 |  259 | `	case '6': return 6;` |
|     162 |  260 | `	case '7': return 7;` |
|     415 |  261 | `	case '8': return 8;` |
|     430 |  262 | `	case '9': return 9;` |
|     408 |  263 | `	case 'A': case 'a': return 10;` |
|     312 |  264 | `	case 'B': case 'b': return 11;` |
|     476 |  265 | `	case 'C': case 'c': return 12;` |
|     292 |  266 | `	case 'D': case 'd': return 13;` |
|     416 |  267 | `	case 'E': case 'e': return 14;` |
|     921 |  268 | `	case 'F': case 'f': return 15;` |
|       - |  269 | `	}` |
|      18 |  270 | `	return -1;` |
|    3074 |  271 | `}` |
|      94 |  272 | `PH7_PRIVATE sxi32 SyHexStrToInt64(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)` |
|       2 |  273 | `{` |
|       - |  274 | `	const char *zIn,*zEnd;` |
|      96 |  275 | `	int isNeg = FALSE;` |
|      96 |  276 | `	sxi64 nVal = 0;` |
|       - |  277 | `#if defined(UNTRUST)` |
|       - |  278 | `	if( SX_EMPTY_STR(zSrc) ){` |
|       - |  279 | `		if( pOutVal ){` |
|       - |  280 | `			*(sxi32 *)pOutVal = 0;` |
|       - |  281 | `		}` |
|       - |  282 | `		return SXERR_EMPTY;` |
|       - |  283 | `	}` |
|       - |  284 | `#endif` |
|      96 |  285 | `	zEnd = &zSrc[nLen];` |
|      96 |  286 | `	while( zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|     ! 0 |  287 | `		zSrc++;` |
|     ! 0 |  288 | `	}` |
|      96 |  289 | `	if( zSrc < zEnd && ( *zSrc == '-' \|\| *zSrc == '+' ) ){` |
|     ! 0 |  290 | `		isNeg = (zSrc[0] == '-') ? TRUE :FALSE;` |
|     ! 0 |  291 | `		zSrc++;` |
|     ! 0 |  292 | `	}` |
|      96 |  293 | `	if( zSrc < &zEnd[-2] && zSrc[0] == '0' && (zSrc[1] == 'x' \|\| zSrc[1] == 'X') ){` |
|       - |  294 | `		/* Bypass hex prefix */` |
|      96 |  295 | `		zSrc += sizeof(char) * 2;` |
|      47 |  296 | `	}` |
|       - |  297 | `	/* Skip leading zero */` |
|     104 |  298 | `	while(zSrc < zEnd && zSrc[0] == '0' ){` |
|       9 |  299 | `		zSrc++;` |
|       1 |  300 | `	}` |
|      96 |  301 | `	zIn = zSrc;` |
|      88 |  302 | `	for(;;){` |
|     178 |  303 | `		if(zSrc >= zEnd \|\| !SyisHex(zSrc[0]) \|\| (int)(zSrc-zIn) > 15){` |
|      28 |  304 | `			break;` |
|       - |  305 | `		}` |
|     124 |  306 | `		nVal = nVal * 16 + SyHexToint(zSrc[0]);` |
|     124 |  307 | `		zSrc++;` |
|     124 |  308 | `		if(zSrc >= zEnd \|\| !SyisHex(zSrc[0]) \|\| (int)(zSrc-zIn) > 15){` |
|       7 |  309 | `			break;` |
|       - |  310 | `		}` |
|     112 |  311 | `		nVal = nVal * 16 + SyHexToint(zSrc[0]);` |
|     112 |  312 | `		zSrc++;` |
|     112 |  313 | `		if(zSrc >= zEnd \|\| !SyisHex(zSrc[0]) \|\| (int)(zSrc-zIn) > 15){` |
|      16 |  314 | `			break;` |
|       - |  315 | `		}` |
|      83 |  316 | `		nVal = nVal * 16 + SyHexToint(zSrc[0]);` |
|      83 |  317 | `		zSrc++;` |
|      83 |  318 | `		if(zSrc >= zEnd \|\| !SyisHex(zSrc[0]) \|\| (int)(zSrc-zIn) > 15){` |
|     ! 0 |  319 | `			break;` |
|       - |  320 | `		}` |
|      83 |  321 | `		nVal = nVal * 16 + SyHexToint(zSrc[0]);` |
|      83 |  322 | `		zSrc++;` |
|       1 |  323 | `	}` |
|      96 |  324 | `	while( zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|     ! 0 |  325 | `		zSrc++;` |
|     ! 0 |  326 | `	}` |
|      96 |  327 | `	if( zRest ){` |
|     ! 0 |  328 | `		*zRest = zSrc;` |
|     ! 0 |  329 | `	}` |
|      96 |  330 | `	if( pOutVal ){` |
|      96 |  331 | `		if( isNeg == TRUE && nVal != 0 ){` |
|     ! 0 |  332 | `			nVal = -nVal;` |
|     ! 0 |  333 | `		}` |
|      96 |  334 | `		*(sxi64 *)pOutVal = nVal;` |
|      47 |  335 | `	}` |
|      96 |  336 | `	return zSrc >= zEnd ? SXRET_OK : SXERR_SYNTAX;` |
|       2 |  337 | `}` |
|    4766 |  338 | `PH7_PRIVATE sxi32 SyOctalStrToInt64(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)` |
|       5 |  339 | `{` |
|       - |  340 | `	const char *zIn,*zEnd;` |
|    4771 |  341 | `	int isNeg = FALSE;` |
|    4771 |  342 | `	sxi64 nVal = 0;` |
|       - |  343 | `	int c;` |
|       - |  344 | `#if defined(UNTRUST)` |
|       - |  345 | `	if( SX_EMPTY_STR(zSrc) ){` |
|       - |  346 | `		if( pOutVal ){` |
|       - |  347 | `			*(sxi32 *)pOutVal = 0;` |
|       - |  348 | `		}` |
|       - |  349 | `		return SXERR_EMPTY;` |
|       - |  350 | `	}` |
|       - |  351 | `#endif` |
|    4771 |  352 | `	zEnd = &zSrc[nLen];` |
|    4771 |  353 | `	while(zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|     ! 0 |  354 | `		zSrc++;` |
|     ! 0 |  355 | `	}` |
|    4771 |  356 | `	if( zSrc < zEnd && ( zSrc[0] == '-' \|\| zSrc[0] == '+' ) ){` |
|     ! 0 |  357 | `		isNeg = (zSrc[0] == '-') ? TRUE :FALSE;` |
|     ! 0 |  358 | `		zSrc++;` |
|     ! 0 |  359 | `	}` |
|       - |  360 | `	/* Skip leading zero */` |
|    9527 |  361 | `	while(zSrc < zEnd && zSrc[0] == '0' ){` |
|    4761 |  362 | `		zSrc++;` |
|       5 |  363 | `	}` |
|    4771 |  364 | `	zIn = zSrc;` |
|    2393 |  365 | `	for(;;){` |
|    4791 |  366 | `		if(zSrc >= zEnd \|\| !SyisDigit(zSrc[0])){ break; } if( (c=zSrc[0]-'0') > 7 \|\| (int)(zSrc-zIn) > 20){ break;} nVal = nVal * 8 +  c; zSrc++;` |
|    4787 |  367 | `		if(zSrc >= zEnd \|\| !SyisDigit(zSrc[0])){ break; } if( (c=zSrc[0]-'0') > 7 \|\| (int)(zSrc-zIn) > 20){ break;} nVal = nVal * 8 +  c; zSrc++;` |
|    4775 |  368 | `		if(zSrc >= zEnd \|\| !SyisDigit(zSrc[0])){ break; } if( (c=zSrc[0]-'0') > 7 \|\| (int)(zSrc-zIn) > 20){ break;} nVal = nVal * 8 +  c; zSrc++;` |
|    4747 |  369 | `		if(zSrc >= zEnd \|\| !SyisDigit(zSrc[0])){ break; } if( (c=zSrc[0]-'0') > 7 \|\| (int)(zSrc-zIn) > 20){ break;} nVal = nVal * 8 +  c; zSrc++;` |
|       1 |  370 | `	}` |
|       - |  371 | `	/* Skip trailing spaces */` |
|    4771 |  372 | `	while(zSrc < zEnd && SyisSpace(zSrc[0])){` |
|     ! 0 |  373 | `		zSrc++;` |
|     ! 0 |  374 | `	}` |
|    4771 |  375 | `	if( zRest ){` |
|     ! 0 |  376 | `		*zRest = zSrc;` |
|     ! 0 |  377 | `	}` |
|    4771 |  378 | `	if( pOutVal ){` |
|    4771 |  379 | `		if( isNeg == TRUE && nVal != 0 ){` |
|     ! 0 |  380 | `			nVal = -nVal;` |
|     ! 0 |  381 | `		}` |
|    4771 |  382 | `		*(sxi64 *)pOutVal = nVal;` |
|    2383 |  383 | `	}` |
|    4771 |  384 | `	return (zSrc >= zEnd) ? SXRET_OK : SXERR_SYNTAX;` |
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
|   11872 |  438 | `PH7_PRIVATE sxi32 SyStrToReal(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)` |
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
|   11877 |  453 | `	const char *zEnd = &zSrc[nLen];` |
|       - |  454 | `	const char *zNum;` |
|   11877 |  455 | `	const char *zExpStart = 0;` |
|   11877 |  456 | `	sxreal Val = 0.0;` |
|   11877 |  457 | `	sxu32 nCopy = 0;` |
|   11877 |  458 | `	int bDigit = 0;` |
|       - |  459 | `#ifdef UNTRUST` |
|       - |  460 | `	if( SX_EMPTY_STR(zSrc)  ){` |
|       - |  461 | `		if( pOutVal ){` |
|       - |  462 | `			*(sxreal *)pOutVal = 0.0;` |
|       - |  463 | `		}` |
|       - |  464 | `		return SXERR_EMPTY;` |
|       - |  465 | `	}` |
|       - |  466 | `#endif` |
|       - |  467 | `	/* Skip leading spaces */` |
|   11913 |  468 | `	while( zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|      37 |  469 | `		zSrc++;` |
|       1 |  470 | `	}` |
|   11877 |  471 | `	zNum = zSrc;` |
|       - |  472 | `	/* Sign (if exists) */` |
|   11877 |  473 | `	if( zSrc < zEnd && (zSrc[0] == '-' \|\| zSrc[0] == '+' ) ){` |
|     155 |  474 | `		zSrc++;` |
|      76 |  475 | `	}` |
|       - |  476 | `	/* Integer part */` |
|   48597 |  477 | `	while( zSrc < zEnd && SyisDigit(zSrc[0]) ){` |
|   36725 |  478 | `		bDigit = 1;` |
|   36725 |  479 | `		zSrc++;` |
|       5 |  480 | `	}` |
|       - |  481 | `	/* Fractional part */` |
|   11877 |  482 | `	if( zSrc < zEnd && ( zSrc[0] == '.' \|\| zSrc[0] == ',' ) ){` |
|   10885 |  483 | `		zSrc++;` |
|   24681 |  484 | `		while( zSrc < zEnd && SyisDigit(zSrc[0]) ){` |
|   13801 |  485 | `			bDigit = 1;` |
|   13801 |  486 | `			zSrc++;` |
|       5 |  487 | `		}` |
|    5440 |  488 | `	}` |
|       - |  489 | `	/* Exponent — consumed only when it carries at least one digit, like` |
|       - |  490 | `	 * strtod, so "1e+x" leaves the "e+" unconsumed. */` |
|   11877 |  491 | `	if( bDigit && zSrc < zEnd && ( zSrc[0] == 'e' \|\| zSrc[0] == 'E' ) ){` |
|     333 |  492 | `		const char *zExp = &zSrc[1];` |
|     333 |  493 | `		if( zExp < zEnd && (zExp[0] == '-' \|\| zExp[0] == '+') ){` |
|      96 |  494 | `			zExp++;` |
|      47 |  495 | `		}` |
|     333 |  496 | `		if( zExp < zEnd && SyisDigit(zExp[0]) ){` |
|     333 |  497 | `			zExpStart = zSrc;` |
|     333 |  498 | `			zSrc = zExp;` |
|     923 |  499 | `			while( zSrc < zEnd && SyisDigit(zSrc[0]) ){` |
|     595 |  500 | `				zSrc++;` |
|       5 |  501 | `			}` |
|     164 |  502 | `		}` |
|     164 |  503 | `	}` |
|   11877 |  504 | `	if( bDigit ){` |
|   11845 |  505 | `		sxu32 i, nSpan = (sxu32)((zExpStart ? zExpStart : zSrc) - zNum);` |
|   11845 |  506 | `		sxu32 nExp = zExpStart ? (sxu32)(zSrc - zExpStart) : 0;` |
|   11845 |  507 | `		char *zDup = zBuf;` |
|   11845 |  508 | `		sxu32 nDup = sizeof(zBuf);` |
|   11845 |  509 | `		if( nSpan + nExp >= sizeof(zBuf) ){` |
|       3 |  510 | `			char *zHeap = (char *)malloc(nSpan + nExp + 1);` |
|       3 |  511 | `			if( zHeap ){` |
|       3 |  512 | `				zDup = zHeap;` |
|       3 |  513 | `				nDup = nSpan + nExp + 1;` |
|       1 |  514 | `			}` |
|       1 |  515 | `		}` |
|       - |  516 | `		{` |
|   11845 |  517 | `			sxu32 nMantMax = nDup - 1 - (nExp < nDup - 1 ? nExp : 0);` |
|   73393 |  518 | `			for( i = 0 ; i < nSpan && nCopy < nMantMax ; i++ ){` |
|   61553 |  519 | `				zDup[nCopy++] = (zNum[i] == ',') ? '.' : zNum[i];` |
|   30779 |  520 | `			}` |
|       - |  521 | `			/* The exponent rides behind even a truncated mantissa: dropping` |
|       - |  522 | `			 * it would collapse "0.<hundreds of zeros>1e300" to 0.0. */` |
|   12857 |  523 | `			for( i = 0 ; i < nExp && nCopy < nDup - 1 ; i++ ){` |
|    1017 |  524 | `				zDup[nCopy++] = zExpStart[i];` |
|     511 |  525 | `			}` |
|       - |  526 | `		}` |
|   11845 |  527 | `		zDup[nCopy] = 0;` |
|   11845 |  528 | `		Val = (sxreal)strtod(zDup,0);` |
|   11845 |  529 | `		if( zDup != zBuf ){` |
|       3 |  530 | `			free(zDup);` |
|       1 |  531 | `		}` |
|    5920 |  532 | `	}` |
|       - |  533 | `	/* Jump trailing spaces */` |
|   11913 |  534 | `	while( zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|      37 |  535 | `		zSrc++;` |
|       1 |  536 | `	}` |
|   11877 |  537 | `	if( zRest ){` |
|     ! 0 |  538 | `		*zRest = zSrc;` |
|     ! 0 |  539 | `	}` |
|   11877 |  540 | `	if( pOutVal ){` |
|   11877 |  541 | `		*(sxreal *)pOutVal = Val;` |
|    5936 |  542 | `	}` |
|   11877 |  543 | `	return zSrc >= zEnd ? SXRET_OK : SXERR_SYNTAX;` |
|       5 |  544 | `}` |
|       - |  545 |  |
