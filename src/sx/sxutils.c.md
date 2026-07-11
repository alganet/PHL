# src/sx/sxutils.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 339/445 lines (76.18%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|      - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    5 | ` */` |
|      - |    6 | `#include "sxtypes.h"` |
|      - |    7 | `#include "sxmacros.h"` |
|      - |    8 | `#include "sxutils.h"` |
|      - |    9 | `#include "sxstr.h"` |
|      - |   10 |  |
| 454308 |   11 | `PH7_PRIVATE sxi32 SyStrIsNumeric(const char *zSrc,sxu32 nLen,sxu8 *pReal,const char  **pzTail)` |
|      5 |   12 | `{` |
|      - |   13 | `	const char *zCur,*zEnd;` |
|      - |   14 | `#ifdef UNTRUST` |
|      - |   15 | `	if( SX_EMPTY_STR(zSrc) ){` |
|      - |   16 | `		return SXERR_EMPTY;` |
|      - |   17 | `	}` |
|      - |   18 | `#endif` |
| 454313 |   19 | `	zEnd = &zSrc[nLen];` |
|      - |   20 | `	/* Jump leading white spaces */` |
| 454329 |   21 | `	while( zSrc < zEnd && (unsigned char)zSrc[0] < 0xc0  && SyisSpace(zSrc[0]) ){` |
|     17 |   22 | `		zSrc++;` |
|      1 |   23 | `	}` |
| 454313 |   24 | `	if( zSrc < zEnd && (zSrc[0] == '+' \|\| zSrc[0] == '-') ){` |
|     89 |   25 | `		zSrc++;` |
|     42 |   26 | `	}` |
| 454313 |   27 | `	zCur = zSrc;` |
| 454313 |   28 | `	if( pReal ){` |
|    227 |   29 | `		*pReal = FALSE;` |
|    111 |   30 | `	}` |
| 227188 |   31 | `	for(;;){` |
| 454313 |   32 | `		if( zSrc >= zEnd \|\| (unsigned char)zSrc[0] >= 0xc0 \|\| !SyisDigit(zSrc[0]) ){` |
| 227101 |   33 | `			break;` |
|      - |   34 | `		}` |
|    185 |   35 | `		zSrc++;` |
|    185 |   36 | `		if( zSrc >= zEnd \|\| (unsigned char)zSrc[0] >= 0xc0 \|\| !SyisDigit(zSrc[0]) ){` |
|     73 |   37 | `			break;` |
|      - |   38 | `		}` |
|     41 |   39 | `		zSrc++;` |
|     41 |   40 | `		if( zSrc >= zEnd \|\| (unsigned char)zSrc[0] >= 0xc0 \|\| !SyisDigit(zSrc[0]) ){` |
|     13 |   41 | `			break;` |
|      - |   42 | `		}` |
|     17 |   43 | `		zSrc++;` |
|     17 |   44 | `		if( zSrc >= zEnd \|\| (unsigned char)zSrc[0] >= 0xc0 \|\| !SyisDigit(zSrc[0]) ){` |
|      9 |   45 | `			break;` |
|      - |   46 | `		}` |
|    ! 0 |   47 | `		zSrc++;` |
|    ! 0 |   48 | `	};` |
| 454313 |   49 | `	if( zSrc < zEnd && zSrc > zCur ){` |
|     53 |   50 | `		int c = zSrc[0];` |
|     53 |   51 | `		if( c == '.' ){` |
|     37 |   52 | `			zSrc++;` |
|     37 |   53 | `			if( pReal ){` |
|     29 |   54 | `				*pReal = TRUE;` |
|     14 |   55 | `			}` |
|     37 |   56 | `			if( pzTail ){` |
|     25 |   57 | `				while( zSrc < zEnd && (unsigned char)zSrc[0] < 0xc0 && SyisDigit(zSrc[0]) ){` |
|     15 |   58 | `					zSrc++;` |
|      1 |   59 | `				}` |
|     11 |   60 | `				if( zSrc < zEnd && (zSrc[0] == 'e' \|\| zSrc[0] == 'E') ){` |
|    ! 0 |   61 | `					zSrc++;` |
|    ! 0 |   62 | `					if( zSrc < zEnd && (zSrc[0] == '+' \|\| zSrc[0] == '-') ){` |
|    ! 0 |   63 | `						zSrc++;` |
|    ! 0 |   64 | `					}` |
|    ! 0 |   65 | `					while( zSrc < zEnd && (unsigned char)zSrc[0] < 0xc0 && SyisDigit(zSrc[0]) ){` |
|    ! 0 |   66 | `						zSrc++;` |
|    ! 0 |   67 | `					}` |
|    ! 0 |   68 | `				}` |
|      6 |   69 | `			}` |
|     35 |   70 | `		}else if( c == 'e' \|\| c == 'E' ){` |
|    ! 0 |   71 | `			zSrc++;` |
|    ! 0 |   72 | `			if( pReal ){` |
|    ! 0 |   73 | `				*pReal = TRUE;` |
|    ! 0 |   74 | `			}` |
|    ! 0 |   75 | `			if( pzTail ){` |
|    ! 0 |   76 | `				if( zSrc < zEnd && (zSrc[0] == '+' \|\| zSrc[0] == '-') ){` |
|    ! 0 |   77 | `					zSrc++;` |
|    ! 0 |   78 | `				}` |
|    ! 0 |   79 | `				while( zSrc < zEnd && (unsigned char)zSrc[0] < 0xc0 && SyisDigit(zSrc[0]) ){` |
|    ! 0 |   80 | `					zSrc++;` |
|    ! 0 |   81 | `				}` |
|    ! 0 |   82 | `			}` |
|    ! 0 |   83 | `		}` |
|     26 |   84 | `	}` |
| 454313 |   85 | `	if( pzTail ){` |
|      - |   86 | `		/* Point to the non numeric part */` |
|     84 |   87 | `		*pzTail = zSrc;` |
|     41 |   88 | `	}` |
| 454313 |   89 | `	return zSrc > zCur ? SXRET_OK /* String prefix is numeric */ : SXERR_INVALID /* Not a digit stream */;` |
|      5 |   90 | `}` |
|      - |   91 | `#define SXINT32_MIN_STR		"2147483648"` |
|      - |   92 | `#define SXINT32_MAX_STR		"2147483647"` |
|      - |   93 | `#define SXINT64_MIN_STR		"9223372036854775808"` |
|      - |   94 | `#define SXINT64_MAX_STR		"9223372036854775807"` |
|     10 |   95 | `PH7_PRIVATE sxi32 SyStrToInt32(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)` |
|      1 |   96 | `{` |
|     11 |   97 | `	int isNeg = FALSE;` |
|      - |   98 | `	const char *zEnd;` |
|     11 |   99 | `	sxi32 nVal = 0;` |
|      - |  100 | `	sxi16 i;` |
|      - |  101 | `#if defined(UNTRUST)` |
|      - |  102 | `	if( SX_EMPTY_STR(zSrc) ){` |
|      - |  103 | `		if( pOutVal ){` |
|      - |  104 | `			*(sxi32 *)pOutVal = 0;` |
|      - |  105 | `		}` |
|      - |  106 | `		return SXERR_EMPTY;` |
|      - |  107 | `	}` |
|      - |  108 | `#endif` |
|     11 |  109 | `	zEnd = &zSrc[nLen];` |
|     11 |  110 | `	while(zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|    ! 0 |  111 | `		zSrc++;` |
|    ! 0 |  112 | `	}` |
|     11 |  113 | `	if( zSrc < zEnd && ( zSrc[0] == '-' \|\| zSrc[0] == '+' ) ){` |
|    ! 0 |  114 | `		isNeg = (zSrc[0] == '-') ? TRUE :FALSE;` |
|    ! 0 |  115 | `		zSrc++;` |
|    ! 0 |  116 | `	}` |
|      - |  117 | `	/* Skip leading zero */` |
|     11 |  118 | `	while(zSrc < zEnd && zSrc[0] == '0' ){` |
|    ! 0 |  119 | `		zSrc++;` |
|    ! 0 |  120 | `	}` |
|     11 |  121 | `	i = 10;` |
|     11 |  122 | `	if( (sxu32)(zEnd-zSrc) >= 10 ){` |
|      - |  123 | `		/* Handle overflow */` |
|    ! 0 |  124 | `		i = SyMemcmp(zSrc,(isNeg == TRUE) ? SXINT32_MIN_STR : SXINT32_MAX_STR,nLen) <= 0 ? 10 : 9;` |
|    ! 0 |  125 | `	}` |
|      5 |  126 | `	for(;;){` |
|     11 |  127 | `		if(zSrc >= zEnd \|\| !i \|\| !SyisDigit(zSrc[0])){ break; } nVal = nVal * 10 + ( zSrc[0] - '0' ) ; --i ; zSrc++;` |
|     11 |  128 | `		if(zSrc >= zEnd \|\| !i \|\| !SyisDigit(zSrc[0])){` |
|    ! 0 |  129 | `			break;` |
|      - |  130 | `		}` |
|     11 |  131 | `		nVal = nVal * 10 + ( zSrc[0] - '0' );` |
|     11 |  132 | `		--i;` |
|     11 |  133 | `		zSrc++;` |
|     11 |  134 | `		if(zSrc >= zEnd \|\| !i \|\| !SyisDigit(zSrc[0])){` |
|      3 |  135 | `			break;` |
|      - |  136 | `		}` |
|      7 |  137 | `		nVal = nVal * 10 + ( zSrc[0] - '0' );` |
|      7 |  138 | `		--i;` |
|      7 |  139 | `		zSrc++;` |
|      7 |  140 | `		if(zSrc >= zEnd \|\| !i \|\| !SyisDigit(zSrc[0])){` |
|      2 |  141 | `			break;` |
|      - |  142 | `		}` |
|      5 |  143 | `		nVal = nVal * 10 + ( zSrc[0] - '0' );` |
|      5 |  144 | `		--i;` |
|      5 |  145 | `		zSrc++;` |
|      5 |  146 | `		if(zSrc >= zEnd \|\| !i \|\| !SyisDigit(zSrc[0])){` |
|      3 |  147 | `			break;` |
|      - |  148 | `		}` |
|    ! 0 |  149 | `		nVal = nVal * 10 + ( zSrc[0] - '0' );` |
|    ! 0 |  150 | `		--i;` |
|    ! 0 |  151 | `		zSrc++;` |
|    ! 0 |  152 | `	}` |
|      - |  153 | `	/* Skip trailing spaces */` |
|     11 |  154 | `	while(zSrc < zEnd && SyisSpace(zSrc[0])){` |
|    ! 0 |  155 | `		zSrc++;` |
|    ! 0 |  156 | `	}` |
|     11 |  157 | `	if( zRest ){` |
|    ! 0 |  158 | `		*zRest = (char *)zSrc;` |
|    ! 0 |  159 | `	}` |
|     11 |  160 | `	if( pOutVal ){` |
|     11 |  161 | `		if( isNeg == TRUE && nVal != 0 ){` |
|    ! 0 |  162 | `			nVal = -nVal;` |
|    ! 0 |  163 | `		}` |
|     11 |  164 | `		*(sxi32 *)pOutVal = nVal;` |
|      5 |  165 | `	}` |
|     11 |  166 | `	return (zSrc >= zEnd) ? SXRET_OK : SXERR_SYNTAX;` |
|      1 |  167 | `}` |
|  78857 |  168 | `PH7_PRIVATE sxi32 SyStrToInt64(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)` |
|      5 |  169 | `{` |
|  78862 |  170 | `	int isNeg = FALSE;` |
|      - |  171 | `	const char *zEnd;` |
|      - |  172 | `	sxi64 nVal;` |
|      - |  173 | `	sxi16 i;` |
|      - |  174 | `#if defined(UNTRUST)` |
|      - |  175 | `	if( SX_EMPTY_STR(zSrc) ){` |
|      - |  176 | `		if( pOutVal ){` |
|      - |  177 | `			*(sxi32 *)pOutVal = 0;` |
|      - |  178 | `		}` |
|      - |  179 | `		return SXERR_EMPTY;` |
|      - |  180 | `	}` |
|      - |  181 | `#endif` |
|  78862 |  182 | `	zEnd = &zSrc[nLen];` |
|  78874 |  183 | `	while(zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|     14 |  184 | `		zSrc++;` |
|      2 |  185 | `	}` |
|  78862 |  186 | `	if( zSrc < zEnd && ( zSrc[0] == '-' \|\| zSrc[0] == '+' ) ){` |
|      3 |  187 | `		isNeg = (zSrc[0] == '-') ? TRUE :FALSE;` |
|      3 |  188 | `		zSrc++;` |
|      1 |  189 | `	}` |
|      - |  190 | `	/* Skip leading zero */` |
|  78862 |  191 | `	while(zSrc < zEnd && zSrc[0] == '0' ){` |
|    ! 0 |  192 | `		zSrc++;` |
|    ! 0 |  193 | `	}` |
|  78862 |  194 | `	i = 19;` |
|  78862 |  195 | `	if( (sxu32)(zEnd-zSrc) >= 19 ){` |
|     11 |  196 | `		i = SyMemcmp(zSrc,isNeg ? SXINT64_MIN_STR : SXINT64_MAX_STR,19) <= 0 ? 19 : 18 ;` |
|      5 |  197 | `	}` |
|  78862 |  198 | `	nVal = 0;` |
|  39669 |  199 | `	for(;;){` |
|  79344 |  200 | `		if(zSrc >= zEnd \|\| !i \|\| !SyisDigit(zSrc[0])){ break; } nVal = nVal * 10 + ( zSrc[0] - '0' ) ; --i ; zSrc++;` |
|  79150 |  201 | `		if(zSrc >= zEnd \|\| !i \|\| !SyisDigit(zSrc[0])){ break; } nVal = nVal * 10 + ( zSrc[0] - '0' ) ; --i ; zSrc++;` |
|  10566 |  202 | `		if(zSrc >= zEnd \|\| !i \|\| !SyisDigit(zSrc[0])){ break; } nVal = nVal * 10 + ( zSrc[0] - '0' ) ; --i ; zSrc++;` |
|   1093 |  203 | `		if(zSrc >= zEnd \|\| !i \|\| !SyisDigit(zSrc[0])){ break; } nVal = nVal * 10 + ( zSrc[0] - '0' ) ; --i ; zSrc++;` |
|      2 |  204 | `	}` |
|      - |  205 | `	/* Skip trailing spaces */` |
|  78874 |  206 | `	while(zSrc < zEnd && SyisSpace(zSrc[0])){` |
|     14 |  207 | `		zSrc++;` |
|      2 |  208 | `	}` |
|  78862 |  209 | `	if( zRest ){` |
|    ! 0 |  210 | `		*zRest = (char *)zSrc;` |
|    ! 0 |  211 | `	}` |
|  78862 |  212 | `	if( pOutVal ){` |
|  78862 |  213 | `		if( isNeg == TRUE && nVal != 0 ){` |
|      3 |  214 | `			nVal = -nVal;` |
|      1 |  215 | `		}` |
|  78862 |  216 | `		*(sxi64 *)pOutVal = nVal;` |
|  39428 |  217 | `	}` |
|  78862 |  218 | `	return (zSrc >= zEnd) ? SXRET_OK : SXERR_SYNTAX;` |
|      5 |  219 | `}` |
|   1574 |  220 | `PH7_PRIVATE sxi32 SyHexToint(sxi32 c)` |
|      2 |  221 | `{` |
|   1576 |  222 | `	switch(c){` |
|    554 |  223 | `	case '0': return 0;` |
|     84 |  224 | `	case '1': return 1;` |
|     45 |  225 | `	case '2': return 2;` |
|     65 |  226 | `	case '3': return 3;` |
|     65 |  227 | `	case '4': return 4;` |
|     37 |  228 | `	case '5': return 5;` |
|     37 |  229 | `	case '6': return 6;` |
|     19 |  230 | `	case '7': return 7;` |
|     47 |  231 | `	case '8': return 8;` |
|     61 |  232 | `	case '9': return 9;` |
|     87 |  233 | `	case 'A': case 'a': return 10;` |
|     31 |  234 | `	case 'B': case 'b': return 11;` |
|     73 |  235 | `	case 'C': case 'c': return 12;` |
|     31 |  236 | `	case 'D': case 'd': return 13;` |
|     61 |  237 | `	case 'E': case 'e': return 14;` |
|    293 |  238 | `	case 'F': case 'f': return 15;` |
|      - |  239 | `	}` |
|      3 |  240 | `	return -1;` |
|    789 |  241 | `}` |
|     82 |  242 | `PH7_PRIVATE sxi32 SyHexStrToInt64(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)` |
|      1 |  243 | `{` |
|      - |  244 | `	const char *zIn,*zEnd;` |
|     83 |  245 | `	int isNeg = FALSE;` |
|     83 |  246 | `	sxi64 nVal = 0;` |
|      - |  247 | `#if defined(UNTRUST)` |
|      - |  248 | `	if( SX_EMPTY_STR(zSrc) ){` |
|      - |  249 | `		if( pOutVal ){` |
|      - |  250 | `			*(sxi32 *)pOutVal = 0;` |
|      - |  251 | `		}` |
|      - |  252 | `		return SXERR_EMPTY;` |
|      - |  253 | `	}` |
|      - |  254 | `#endif` |
|     83 |  255 | `	zEnd = &zSrc[nLen];` |
|     83 |  256 | `	while( zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|    ! 0 |  257 | `		zSrc++;` |
|    ! 0 |  258 | `	}` |
|     83 |  259 | `	if( zSrc < zEnd && ( *zSrc == '-' \|\| *zSrc == '+' ) ){` |
|    ! 0 |  260 | `		isNeg = (zSrc[0] == '-') ? TRUE :FALSE;` |
|    ! 0 |  261 | `		zSrc++;` |
|    ! 0 |  262 | `	}` |
|     83 |  263 | `	if( zSrc < &zEnd[-2] && zSrc[0] == '0' && (zSrc[1] == 'x' \|\| zSrc[1] == 'X') ){` |
|      - |  264 | `		/* Bypass hex prefix */` |
|     71 |  265 | `		zSrc += sizeof(char) * 2;` |
|     35 |  266 | `	}` |
|      - |  267 | `	/* Skip leading zero */` |
|     93 |  268 | `	while(zSrc < zEnd && zSrc[0] == '0' ){` |
|     11 |  269 | `		zSrc++;` |
|      1 |  270 | `	}` |
|     83 |  271 | `	zIn = zSrc;` |
|     79 |  272 | `	for(;;){` |
|    159 |  273 | `		if(zSrc >= zEnd \|\| !SyisHex(zSrc[0]) \|\| (int)(zSrc-zIn) > 15){` |
|     27 |  274 | `			break;` |
|      - |  275 | `		}` |
|    107 |  276 | `		nVal = nVal * 16 + SyHexToint(zSrc[0]);` |
|    107 |  277 | `		zSrc++;` |
|    107 |  278 | `		if(zSrc >= zEnd \|\| !SyisHex(zSrc[0]) \|\| (int)(zSrc-zIn) > 15){` |
|      6 |  279 | `			break;` |
|      - |  280 | `		}` |
|     97 |  281 | `		nVal = nVal * 16 + SyHexToint(zSrc[0]);` |
|     97 |  282 | `		zSrc++;` |
|     97 |  283 | `		if(zSrc >= zEnd \|\| !SyisHex(zSrc[0]) \|\| (int)(zSrc-zIn) > 15){` |
|     11 |  284 | `			break;` |
|      - |  285 | `		}` |
|     77 |  286 | `		nVal = nVal * 16 + SyHexToint(zSrc[0]);` |
|     77 |  287 | `		zSrc++;` |
|     77 |  288 | `		if(zSrc >= zEnd \|\| !SyisHex(zSrc[0]) \|\| (int)(zSrc-zIn) > 15){` |
|    ! 0 |  289 | `			break;` |
|      - |  290 | `		}` |
|     77 |  291 | `		nVal = nVal * 16 + SyHexToint(zSrc[0]);` |
|     77 |  292 | `		zSrc++;` |
|      1 |  293 | `	}` |
|     83 |  294 | `	while( zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|    ! 0 |  295 | `		zSrc++;` |
|    ! 0 |  296 | `	}` |
|     83 |  297 | `	if( zRest ){` |
|    ! 0 |  298 | `		*zRest = zSrc;` |
|    ! 0 |  299 | `	}` |
|     83 |  300 | `	if( pOutVal ){` |
|     83 |  301 | `		if( isNeg == TRUE && nVal != 0 ){` |
|    ! 0 |  302 | `			nVal = -nVal;` |
|    ! 0 |  303 | `		}` |
|     83 |  304 | `		*(sxi64 *)pOutVal = nVal;` |
|     41 |  305 | `	}` |
|     83 |  306 | `	return zSrc >= zEnd ? SXRET_OK : SXERR_SYNTAX;` |
|      1 |  307 | `}` |
|     61 |  308 | `PH7_PRIVATE sxi32 SyOctalStrToInt64(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)` |
|      1 |  309 | `{` |
|      - |  310 | `	const char *zIn,*zEnd;` |
|     62 |  311 | `	int isNeg = FALSE;` |
|     62 |  312 | `	sxi64 nVal = 0;` |
|      - |  313 | `	int c;` |
|      - |  314 | `#if defined(UNTRUST)` |
|      - |  315 | `	if( SX_EMPTY_STR(zSrc) ){` |
|      - |  316 | `		if( pOutVal ){` |
|      - |  317 | `			*(sxi32 *)pOutVal = 0;` |
|      - |  318 | `		}` |
|      - |  319 | `		return SXERR_EMPTY;` |
|      - |  320 | `	}` |
|      - |  321 | `#endif` |
|     62 |  322 | `	zEnd = &zSrc[nLen];` |
|     62 |  323 | `	while(zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|    ! 0 |  324 | `		zSrc++;` |
|    ! 0 |  325 | `	}` |
|     62 |  326 | `	if( zSrc < zEnd && ( zSrc[0] == '-' \|\| zSrc[0] == '+' ) ){` |
|    ! 0 |  327 | `		isNeg = (zSrc[0] == '-') ? TRUE :FALSE;` |
|    ! 0 |  328 | `		zSrc++;` |
|    ! 0 |  329 | `	}` |
|      - |  330 | `	/* Skip leading zero */` |
|    123 |  331 | `	while(zSrc < zEnd && zSrc[0] == '0' ){` |
|     62 |  332 | `		zSrc++;` |
|      1 |  333 | `	}` |
|     62 |  334 | `	zIn = zSrc;` |
|     31 |  335 | `	for(;;){` |
|     62 |  336 | `		if(zSrc >= zEnd \|\| !SyisDigit(zSrc[0])){ break; } if( (c=zSrc[0]-'0') > 7 \|\| (int)(zSrc-zIn) > 20){ break;} nVal = nVal * 8 +  c; zSrc++;` |
|     62 |  337 | `		if(zSrc >= zEnd \|\| !SyisDigit(zSrc[0])){ break; } if( (c=zSrc[0]-'0') > 7 \|\| (int)(zSrc-zIn) > 20){ break;} nVal = nVal * 8 +  c; zSrc++;` |
|     55 |  338 | `		if(zSrc >= zEnd \|\| !SyisDigit(zSrc[0])){ break; } if( (c=zSrc[0]-'0') > 7 \|\| (int)(zSrc-zIn) > 20){ break;} nVal = nVal * 8 +  c; zSrc++;` |
|     39 |  339 | `		if(zSrc >= zEnd \|\| !SyisDigit(zSrc[0])){ break; } if( (c=zSrc[0]-'0') > 7 \|\| (int)(zSrc-zIn) > 20){ break;} nVal = nVal * 8 +  c; zSrc++;` |
|    ! 0 |  340 | `	}` |
|      - |  341 | `	/* Skip trailing spaces */` |
|     62 |  342 | `	while(zSrc < zEnd && SyisSpace(zSrc[0])){` |
|    ! 0 |  343 | `		zSrc++;` |
|    ! 0 |  344 | `	}` |
|     62 |  345 | `	if( zRest ){` |
|    ! 0 |  346 | `		*zRest = zSrc;` |
|    ! 0 |  347 | `	}` |
|     62 |  348 | `	if( pOutVal ){` |
|     62 |  349 | `		if( isNeg == TRUE && nVal != 0 ){` |
|    ! 0 |  350 | `			nVal = -nVal;` |
|    ! 0 |  351 | `		}` |
|     62 |  352 | `		*(sxi64 *)pOutVal = nVal;` |
|     31 |  353 | `	}` |
|     62 |  354 | `	return (zSrc >= zEnd) ? SXRET_OK : SXERR_SYNTAX;` |
|      1 |  355 | `}` |
|    282 |  356 | `PH7_PRIVATE sxi32 SyBinaryStrToInt64(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)` |
|      1 |  357 | `{` |
|      - |  358 | `	const char *zIn,*zEnd;` |
|    283 |  359 | `	int isNeg = FALSE;` |
|    283 |  360 | `	sxi64 nVal = 0;` |
|      - |  361 | `	int c;` |
|      - |  362 | `#if defined(UNTRUST)` |
|      - |  363 | `	if( SX_EMPTY_STR(zSrc) ){` |
|      - |  364 | `		if( pOutVal ){` |
|      - |  365 | `			*(sxi32 *)pOutVal = 0;` |
|      - |  366 | `		}` |
|      - |  367 | `		return SXERR_EMPTY;` |
|      - |  368 | `	}` |
|      - |  369 | `#endif` |
|    283 |  370 | `	zEnd = &zSrc[nLen];` |
|    283 |  371 | `	while(zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|    ! 0 |  372 | `		zSrc++;` |
|    ! 0 |  373 | `	}` |
|    283 |  374 | `	if( zSrc < zEnd && ( zSrc[0] == '-' \|\| zSrc[0] == '+' ) ){` |
|    ! 0 |  375 | `		isNeg = (zSrc[0] == '-') ? TRUE :FALSE;` |
|    ! 0 |  376 | `		zSrc++;` |
|    ! 0 |  377 | `	}` |
|    283 |  378 | `	if( zSrc < &zEnd[-2] && zSrc[0] == '0' && (zSrc[1] == 'b' \|\| zSrc[1] == 'B') ){` |
|      - |  379 | `		/* Bypass binary prefix */` |
|    277 |  380 | `		zSrc += sizeof(char) * 2;` |
|    138 |  381 | `	}` |
|      - |  382 | `	/* Skip leading zero */` |
|    331 |  383 | `	while(zSrc < zEnd && zSrc[0] == '0' ){` |
|     49 |  384 | `		zSrc++;` |
|      1 |  385 | `	}` |
|    283 |  386 | `	zIn = zSrc;` |
|    300 |  387 | `	for(;;){` |
|    601 |  388 | `		if(zSrc >= zEnd \|\| (zSrc[0] != '1' && zSrc[0] != '0') \|\| (int)(zSrc-zIn) > 62){ break; } c = zSrc[0] - '0'; nVal = (nVal << 1) + c; zSrc++;` |
|    429 |  389 | `		if(zSrc >= zEnd \|\| (zSrc[0] != '1' && zSrc[0] != '0') \|\| (int)(zSrc-zIn) > 62){ break; } c = zSrc[0] - '0'; nVal = (nVal << 1) + c; zSrc++;` |
|    397 |  390 | `		if(zSrc >= zEnd \|\| (zSrc[0] != '1' && zSrc[0] != '0') \|\| (int)(zSrc-zIn) > 62){ break; } c = zSrc[0] - '0'; nVal = (nVal << 1) + c; zSrc++;` |
|    353 |  391 | `		if(zSrc >= zEnd \|\| (zSrc[0] != '1' && zSrc[0] != '0') \|\| (int)(zSrc-zIn) > 62){ break; } c = zSrc[0] - '0'; nVal = (nVal << 1) + c; zSrc++;` |
|      1 |  392 | `	}` |
|      - |  393 | `	/* Skip trailing spaces */` |
|    283 |  394 | `	while(zSrc < zEnd && SyisSpace(zSrc[0])){` |
|    ! 0 |  395 | `		zSrc++;` |
|    ! 0 |  396 | `	}` |
|    283 |  397 | `	if( zRest ){` |
|    ! 0 |  398 | `		*zRest = zSrc;` |
|    ! 0 |  399 | `	}` |
|    283 |  400 | `	if( pOutVal ){` |
|    283 |  401 | `		if( isNeg == TRUE && nVal != 0 ){` |
|    ! 0 |  402 | `			nVal = -nVal;` |
|    ! 0 |  403 | `		}` |
|    283 |  404 | `		*(sxi64 *)pOutVal = nVal;` |
|    141 |  405 | `	}` |
|    283 |  406 | `	return (zSrc >= zEnd) ? SXRET_OK : SXERR_SYNTAX;` |
|      1 |  407 | `}` |
|    776 |  408 | `PH7_PRIVATE sxi32 SyStrToReal(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)` |
|      4 |  409 | `{` |
|      - |  410 | `#define SXDBL_DIG        15` |
|      - |  411 | `#define SXDBL_MAX_EXP    308` |
|      - |  412 | `#define SXDBL_MIN_EXP_PLUS	307` |
|      - |  413 | `	static const sxreal aTab[] = {` |
|      - |  414 | `	10,` |
|      - |  415 | `	1.0e2,` |
|      - |  416 | `	1.0e4,` |
|      - |  417 | `	1.0e8,` |
|      - |  418 | `	1.0e16,` |
|      - |  419 | `	1.0e32,` |
|      - |  420 | `	1.0e64,` |
|      - |  421 | `	1.0e128,` |
|      - |  422 | `	1.0e256` |
|      - |  423 | `	};` |
|    780 |  424 | `	sxu8 neg = FALSE;` |
|    780 |  425 | `	sxreal Val = 0.0;` |
|      - |  426 | `	const char *zEnd;` |
|      - |  427 | `	sxi32 Lim,exp;` |
|    780 |  428 | `	sxreal *p = 0;` |
|      - |  429 | `#ifdef UNTRUST` |
|      - |  430 | `	if( SX_EMPTY_STR(zSrc)  ){` |
|      - |  431 | `		if( pOutVal ){` |
|      - |  432 | `			*(sxreal *)pOutVal = 0.0;` |
|      - |  433 | `		}` |
|      - |  434 | `		return SXERR_EMPTY;` |
|      - |  435 | `	}` |
|      - |  436 | `#endif` |
|      - |  437 | `	/* Define local limits and end pointer used by the parsing loops */` |
|    780 |  438 | `	zEnd = &zSrc[nLen];` |
|    780 |  439 | `	Lim = SXDBL_DIG;` |
|      - |  440 | `	/* Skip leading spaces */` |
|    784 |  441 | `	while( zSrc < zEnd && SyisSpace(zSrc[0]) ) zSrc++;` |
|      - |  442 |  |
|      - |  443 | `	/* Sign (if exists) */` |
|    780 |  444 | `	if( zSrc < zEnd && (zSrc[0] == '-' \|\| zSrc[0] == '+' ) ){` |
|     10 |  445 | `		neg =  zSrc[0] == '-' ? TRUE : FALSE ;` |
|     10 |  446 | `		zSrc++;` |
|      4 |  447 | `	}` |
|      - |  448 |  |
|      - |  449 | `	/* Integer part */` |
|    408 |  450 | `	for(;;){` |
|    820 |  451 | `		if(zSrc >= zEnd \|\| !Lim \|\| !SyisDigit(zSrc[0])){` |
|     20 |  452 | `			break;` |
|      - |  453 | `		}` |
|    784 |  454 | `		Val = Val * 10.0 + (zSrc[0] - '0');` |
|    784 |  455 | `		zSrc++;` |
|    784 |  456 | `		--Lim;` |
|    784 |  457 | `		if(zSrc >= zEnd \|\| !Lim \|\| !SyisDigit(zSrc[0])){` |
|    362 |  458 | `			break;` |
|      - |  459 | `		}` |
|     65 |  460 | `		Val = Val * 10.0 + (zSrc[0] - '0');` |
|     65 |  461 | `		zSrc++;` |
|     65 |  462 | `		--Lim;` |
|     65 |  463 | `		if(zSrc >= zEnd \|\| !Lim \|\| !SyisDigit(zSrc[0])){` |
|      8 |  464 | `			break;` |
|      - |  465 | `		}` |
|     51 |  466 | `		Val = Val * 10.0 + (zSrc[0] - '0');` |
|     51 |  467 | `		zSrc++;` |
|     51 |  468 | `		--Lim;` |
|     51 |  469 | `		if(zSrc >= zEnd \|\| !Lim \|\| !SyisDigit(zSrc[0])){` |
|      6 |  470 | `			break;` |
|      - |  471 | `		}` |
|     41 |  472 | `		Val = Val * 10.0 + (zSrc[0] - '0');` |
|     41 |  473 | `		zSrc++;` |
|     41 |  474 | `		--Lim;` |
|      1 |  475 | `	}` |
|    780 |  476 | `	Lim = SXDBL_DIG ;` |
|    388 |  477 | `	for(;;){` |
|    780 |  478 | `		if(zSrc >= zEnd \|\| !Lim \|\| !SyisDigit(zSrc[0])){` |
|    392 |  479 | `			break;` |
|      - |  480 | `		}` |
|    ! 0 |  481 | `		Val = Val * 10.0 + (zSrc[0] - '0');` |
|    ! 0 |  482 | `		zSrc++;` |
|    ! 0 |  483 | `		--Lim;` |
|    ! 0 |  484 | `		if(zSrc >= zEnd \|\| !Lim \|\| !SyisDigit(zSrc[0])){` |
|    ! 0 |  485 | `			break;` |
|      - |  486 | `		}` |
|    ! 0 |  487 | `		Val = Val * 10.0 + (zSrc[0] - '0');` |
|    ! 0 |  488 | `		zSrc++;` |
|    ! 0 |  489 | `		--Lim;` |
|    ! 0 |  490 | `		if(zSrc >= zEnd \|\| !Lim \|\| !SyisDigit(zSrc[0])){` |
|    ! 0 |  491 | `			break;` |
|      - |  492 | `		}` |
|    ! 0 |  493 | `		Val = Val * 10.0 + (zSrc[0] - '0');` |
|    ! 0 |  494 | `		zSrc++;` |
|    ! 0 |  495 | `		--Lim;` |
|    ! 0 |  496 | `		if(zSrc >= zEnd \|\| !Lim \|\| !SyisDigit(zSrc[0])){` |
|    ! 0 |  497 | `			break;` |
|      - |  498 | `		}` |
|    ! 0 |  499 | `		Val = Val * 10.0 + (zSrc[0] - '0');` |
|    ! 0 |  500 | `		zSrc++;` |
|    ! 0 |  501 | `		--Lim;` |
|    ! 0 |  502 | `	}` |
|    780 |  503 | `	if( zSrc < zEnd && ( zSrc[0] == '.' \|\| zSrc[0] == ',' ) ){` |
|    712 |  504 | `		sxreal dec = 1.0;` |
|    712 |  505 | `		zSrc++;` |
|    450 |  506 | `		for(;;){` |
|    904 |  507 | `			if(zSrc >= zEnd \|\| !Lim \|\| !SyisDigit(zSrc[0])){` |
|     31 |  508 | `				break;` |
|      - |  509 | `			}` |
|    846 |  510 | `			Val = Val * 10.0 + (zSrc[0] - '0');` |
|    846 |  511 | `			dec *= 10.0;` |
|    846 |  512 | `			zSrc++;` |
|    846 |  513 | `			--Lim;` |
|    846 |  514 | `			if(zSrc >= zEnd \|\| !Lim \|\| !SyisDigit(zSrc[0])){` |
|    230 |  515 | `				break;` |
|      - |  516 | `			}` |
|    393 |  517 | `			Val = Val * 10.0 + (zSrc[0] - '0');` |
|    393 |  518 | `			dec *= 10.0;` |
|    393 |  519 | `			zSrc++;` |
|    393 |  520 | `			--Lim;` |
|    393 |  521 | `			if(zSrc >= zEnd \|\| !Lim \|\| !SyisDigit(zSrc[0])){` |
|     52 |  522 | `				break;` |
|      - |  523 | `			}` |
|    293 |  524 | `			Val = Val * 10.0 + (zSrc[0] - '0');` |
|    293 |  525 | `			dec *= 10.0;` |
|    293 |  526 | `			zSrc++;` |
|    293 |  527 | `			--Lim;` |
|    293 |  528 | `			if(zSrc >= zEnd \|\| !Lim \|\| !SyisDigit(zSrc[0])){` |
|     51 |  529 | `				break;` |
|      - |  530 | `			}` |
|    193 |  531 | `			Val = Val * 10.0 + (zSrc[0] - '0');` |
|    193 |  532 | `			dec *= 10.0;` |
|    193 |  533 | `			zSrc++;` |
|    193 |  534 | `			--Lim;` |
|      1 |  535 | `		}` |
|    712 |  536 | `		Val /= dec;` |
|    354 |  537 | `	}` |
|    780 |  538 | `	if( neg == TRUE && Val != 0.0 ) {` |
|     10 |  539 | `		Val = -Val ;` |
|      4 |  540 | `	}` |
|    780 |  541 | `	if( Lim <= 0 ){` |
|      - |  542 | `		/* jump overflow digit */` |
|     45 |  543 | `		while( zSrc < zEnd ){` |
|     17 |  544 | `			if( zSrc[0] == 'e' \|\| zSrc[0] == 'E' ){` |
|    ! 0 |  545 | `				break;` |
|      - |  546 | `			}` |
|     17 |  547 | `			zSrc++;` |
|      1 |  548 | `		}` |
|     14 |  549 | `	}` |
|    780 |  550 | `	neg = FALSE;` |
|    780 |  551 | `	if( zSrc < zEnd && ( zSrc[0] == 'e' \|\| zSrc[0] == 'E' ) ){` |
|    102 |  552 | `		zSrc++;` |
|    102 |  553 | `		if( zSrc < zEnd && ( zSrc[0] == '-' \|\| zSrc[0] == '+') ){` |
|     33 |  554 | `			neg = zSrc[0] == '-' ? TRUE : FALSE ;` |
|     33 |  555 | `			zSrc++;` |
|     16 |  556 | `		}` |
|    102 |  557 | `		exp = 0;` |
|    274 |  558 | `		while( zSrc < zEnd && SyisDigit(zSrc[0]) && exp < SXDBL_MAX_EXP ){` |
|    174 |  559 | `			exp = exp * 10 + (zSrc[0] - '0');` |
|    174 |  560 | `			zSrc++;` |
|      2 |  561 | `		}` |
|    102 |  562 | `		if( neg  ){` |
|     27 |  563 | `			if( exp > SXDBL_MIN_EXP_PLUS ) exp = SXDBL_MIN_EXP_PLUS ;` |
|     88 |  564 | `		}else if ( exp > SXDBL_MAX_EXP ){` |
|      3 |  565 | `			exp = SXDBL_MAX_EXP;` |
|      1 |  566 | `		}` |
|    534 |  567 | `		for( p = (sxreal *)aTab ; exp ; exp >>= 1 , p++ ){` |
|    434 |  568 | `			if( exp & 01 ){` |
|    234 |  569 | `				if( neg ){` |
|     63 |  570 | `					Val /= *p ;` |
|     32 |  571 | `				}else{` |
|    172 |  572 | `					Val *= *p;` |
|      - |  573 | `				}` |
|    116 |  574 | `			}` |
|    218 |  575 | `		}` |
|     50 |  576 | `	}` |
|    780 |  577 | `	while( zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|    ! 0 |  578 | `		zSrc++;` |
|    ! 0 |  579 | `	}` |
|    780 |  580 | `	if( zRest ){` |
|    ! 0 |  581 | `		*zRest = zSrc;` |
|    ! 0 |  582 | `	}` |
|    780 |  583 | `	if( pOutVal ){` |
|    780 |  584 | `		*(sxreal *)pOutVal = Val;` |
|    388 |  585 | `	}` |
|    780 |  586 | `	return zSrc >= zEnd ? SXRET_OK : SXERR_SYNTAX;` |
|      4 |  587 | `}` |
|      - |  588 |  |
