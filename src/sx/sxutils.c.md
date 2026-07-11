# src/sx/sxutils.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 303/384 lines (78.91%)

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
|      - |   10 | `#include <stdlib.h> /* strtod — SyStrToReal must be correctly rounded (see its comment) */` |
|      - |   11 |  |
| 457542 |   12 | `PH7_PRIVATE sxi32 SyStrIsNumeric(const char *zSrc,sxu32 nLen,sxu8 *pReal,const char  **pzTail)` |
|      5 |   13 | `{` |
|      - |   14 | `	const char *zCur,*zEnd;` |
|      - |   15 | `#ifdef UNTRUST` |
|      - |   16 | `	if( SX_EMPTY_STR(zSrc) ){` |
|      - |   17 | `		return SXERR_EMPTY;` |
|      - |   18 | `	}` |
|      - |   19 | `#endif` |
| 457547 |   20 | `	zEnd = &zSrc[nLen];` |
|      - |   21 | `	/* Jump leading white spaces */` |
| 457563 |   22 | `	while( zSrc < zEnd && (unsigned char)zSrc[0] < 0xc0  && SyisSpace(zSrc[0]) ){` |
|     17 |   23 | `		zSrc++;` |
|      1 |   24 | `	}` |
| 457547 |   25 | `	if( zSrc < zEnd && (zSrc[0] == '+' \|\| zSrc[0] == '-') ){` |
|     89 |   26 | `		zSrc++;` |
|     42 |   27 | `	}` |
| 457547 |   28 | `	zCur = zSrc;` |
| 457547 |   29 | `	if( pReal ){` |
|    230 |   30 | `		*pReal = FALSE;` |
|    113 |   31 | `	}` |
| 228784 |   32 | `	for(;;){` |
| 457547 |   33 | `		if( zSrc >= zEnd \|\| (unsigned char)zSrc[0] >= 0xc0 \|\| !SyisDigit(zSrc[0]) ){` |
| 228691 |   34 | `			break;` |
|      - |   35 | `		}` |
|    197 |   36 | `		zSrc++;` |
|    197 |   37 | `		if( zSrc >= zEnd \|\| (unsigned char)zSrc[0] >= 0xc0 \|\| !SyisDigit(zSrc[0]) ){` |
|     79 |   38 | `			break;` |
|      - |   39 | `		}` |
|     41 |   40 | `		zSrc++;` |
|     41 |   41 | `		if( zSrc >= zEnd \|\| (unsigned char)zSrc[0] >= 0xc0 \|\| !SyisDigit(zSrc[0]) ){` |
|     13 |   42 | `			break;` |
|      - |   43 | `		}` |
|     17 |   44 | `		zSrc++;` |
|     17 |   45 | `		if( zSrc >= zEnd \|\| (unsigned char)zSrc[0] >= 0xc0 \|\| !SyisDigit(zSrc[0]) ){` |
|      9 |   46 | `			break;` |
|      - |   47 | `		}` |
|    ! 0 |   48 | `		zSrc++;` |
|    ! 0 |   49 | `	};` |
| 457547 |   50 | `	if( zSrc < zEnd && zSrc > zCur ){` |
|     61 |   51 | `		int c = zSrc[0];` |
|     61 |   52 | `		if( c == '.' ){` |
|     41 |   53 | `			zSrc++;` |
|     41 |   54 | `			if( pReal ){` |
|     29 |   55 | `				*pReal = TRUE;` |
|     14 |   56 | `			}` |
|     41 |   57 | `			if( pzTail ){` |
|     25 |   58 | `				while( zSrc < zEnd && (unsigned char)zSrc[0] < 0xc0 && SyisDigit(zSrc[0]) ){` |
|     15 |   59 | `					zSrc++;` |
|      1 |   60 | `				}` |
|     11 |   61 | `				if( zSrc < zEnd && (zSrc[0] == 'e' \|\| zSrc[0] == 'E') ){` |
|    ! 0 |   62 | `					zSrc++;` |
|    ! 0 |   63 | `					if( zSrc < zEnd && (zSrc[0] == '+' \|\| zSrc[0] == '-') ){` |
|    ! 0 |   64 | `						zSrc++;` |
|    ! 0 |   65 | `					}` |
|    ! 0 |   66 | `					while( zSrc < zEnd && (unsigned char)zSrc[0] < 0xc0 && SyisDigit(zSrc[0]) ){` |
|    ! 0 |   67 | `						zSrc++;` |
|    ! 0 |   68 | `					}` |
|    ! 0 |   69 | `				}` |
|      6 |   70 | `			}` |
|     41 |   71 | `		}else if( c == 'e' \|\| c == 'E' ){` |
|      5 |   72 | `			zSrc++;` |
|      5 |   73 | `			if( pReal ){` |
|    ! 0 |   74 | `				*pReal = TRUE;` |
|    ! 0 |   75 | `			}` |
|      5 |   76 | `			if( pzTail ){` |
|    ! 0 |   77 | `				if( zSrc < zEnd && (zSrc[0] == '+' \|\| zSrc[0] == '-') ){` |
|    ! 0 |   78 | `					zSrc++;` |
|    ! 0 |   79 | `				}` |
|    ! 0 |   80 | `				while( zSrc < zEnd && (unsigned char)zSrc[0] < 0xc0 && SyisDigit(zSrc[0]) ){` |
|    ! 0 |   81 | `					zSrc++;` |
|    ! 0 |   82 | `				}` |
|    ! 0 |   83 | `			}` |
|      2 |   84 | `		}` |
|     30 |   85 | `	}` |
| 457547 |   86 | `	if( pzTail ){` |
|      - |   87 | `		/* Point to the non numeric part */` |
|     84 |   88 | `		*pzTail = zSrc;` |
|     41 |   89 | `	}` |
| 457547 |   90 | `	return zSrc > zCur ? SXRET_OK /* String prefix is numeric */ : SXERR_INVALID /* Not a digit stream */;` |
|      5 |   91 | `}` |
|      - |   92 | `#define SXINT32_MIN_STR		"2147483648"` |
|      - |   93 | `#define SXINT32_MAX_STR		"2147483647"` |
|      - |   94 | `#define SXINT64_MIN_STR		"9223372036854775808"` |
|      - |   95 | `#define SXINT64_MAX_STR		"9223372036854775807"` |
|     10 |   96 | `PH7_PRIVATE sxi32 SyStrToInt32(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)` |
|      1 |   97 | `{` |
|     11 |   98 | `	int isNeg = FALSE;` |
|      - |   99 | `	const char *zEnd;` |
|     11 |  100 | `	sxi32 nVal = 0;` |
|      - |  101 | `	sxi16 i;` |
|      - |  102 | `#if defined(UNTRUST)` |
|      - |  103 | `	if( SX_EMPTY_STR(zSrc) ){` |
|      - |  104 | `		if( pOutVal ){` |
|      - |  105 | `			*(sxi32 *)pOutVal = 0;` |
|      - |  106 | `		}` |
|      - |  107 | `		return SXERR_EMPTY;` |
|      - |  108 | `	}` |
|      - |  109 | `#endif` |
|     11 |  110 | `	zEnd = &zSrc[nLen];` |
|     11 |  111 | `	while(zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|    ! 0 |  112 | `		zSrc++;` |
|    ! 0 |  113 | `	}` |
|     11 |  114 | `	if( zSrc < zEnd && ( zSrc[0] == '-' \|\| zSrc[0] == '+' ) ){` |
|    ! 0 |  115 | `		isNeg = (zSrc[0] == '-') ? TRUE :FALSE;` |
|    ! 0 |  116 | `		zSrc++;` |
|    ! 0 |  117 | `	}` |
|      - |  118 | `	/* Skip leading zero */` |
|     11 |  119 | `	while(zSrc < zEnd && zSrc[0] == '0' ){` |
|    ! 0 |  120 | `		zSrc++;` |
|    ! 0 |  121 | `	}` |
|     11 |  122 | `	i = 10;` |
|     11 |  123 | `	if( (sxu32)(zEnd-zSrc) >= 10 ){` |
|      - |  124 | `		/* Handle overflow */` |
|    ! 0 |  125 | `		i = SyMemcmp(zSrc,(isNeg == TRUE) ? SXINT32_MIN_STR : SXINT32_MAX_STR,nLen) <= 0 ? 10 : 9;` |
|    ! 0 |  126 | `	}` |
|      5 |  127 | `	for(;;){` |
|     11 |  128 | `		if(zSrc >= zEnd \|\| !i \|\| !SyisDigit(zSrc[0])){ break; } nVal = nVal * 10 + ( zSrc[0] - '0' ) ; --i ; zSrc++;` |
|     11 |  129 | `		if(zSrc >= zEnd \|\| !i \|\| !SyisDigit(zSrc[0])){` |
|    ! 0 |  130 | `			break;` |
|      - |  131 | `		}` |
|     11 |  132 | `		nVal = nVal * 10 + ( zSrc[0] - '0' );` |
|     11 |  133 | `		--i;` |
|     11 |  134 | `		zSrc++;` |
|     11 |  135 | `		if(zSrc >= zEnd \|\| !i \|\| !SyisDigit(zSrc[0])){` |
|      3 |  136 | `			break;` |
|      - |  137 | `		}` |
|      7 |  138 | `		nVal = nVal * 10 + ( zSrc[0] - '0' );` |
|      7 |  139 | `		--i;` |
|      7 |  140 | `		zSrc++;` |
|      7 |  141 | `		if(zSrc >= zEnd \|\| !i \|\| !SyisDigit(zSrc[0])){` |
|      2 |  142 | `			break;` |
|      - |  143 | `		}` |
|      5 |  144 | `		nVal = nVal * 10 + ( zSrc[0] - '0' );` |
|      5 |  145 | `		--i;` |
|      5 |  146 | `		zSrc++;` |
|      5 |  147 | `		if(zSrc >= zEnd \|\| !i \|\| !SyisDigit(zSrc[0])){` |
|      3 |  148 | `			break;` |
|      - |  149 | `		}` |
|    ! 0 |  150 | `		nVal = nVal * 10 + ( zSrc[0] - '0' );` |
|    ! 0 |  151 | `		--i;` |
|    ! 0 |  152 | `		zSrc++;` |
|    ! 0 |  153 | `	}` |
|      - |  154 | `	/* Skip trailing spaces */` |
|     11 |  155 | `	while(zSrc < zEnd && SyisSpace(zSrc[0])){` |
|    ! 0 |  156 | `		zSrc++;` |
|    ! 0 |  157 | `	}` |
|     11 |  158 | `	if( zRest ){` |
|    ! 0 |  159 | `		*zRest = (char *)zSrc;` |
|    ! 0 |  160 | `	}` |
|     11 |  161 | `	if( pOutVal ){` |
|     11 |  162 | `		if( isNeg == TRUE && nVal != 0 ){` |
|    ! 0 |  163 | `			nVal = -nVal;` |
|    ! 0 |  164 | `		}` |
|     11 |  165 | `		*(sxi32 *)pOutVal = nVal;` |
|      5 |  166 | `	}` |
|     11 |  167 | `	return (zSrc >= zEnd) ? SXRET_OK : SXERR_SYNTAX;` |
|      1 |  168 | `}` |
|  79740 |  169 | `PH7_PRIVATE sxi32 SyStrToInt64(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)` |
|      5 |  170 | `{` |
|  79745 |  171 | `	int isNeg = FALSE;` |
|      - |  172 | `	const char *zEnd;` |
|      - |  173 | `	sxi64 nVal;` |
|      - |  174 | `	sxi16 i;` |
|      - |  175 | `#if defined(UNTRUST)` |
|      - |  176 | `	if( SX_EMPTY_STR(zSrc) ){` |
|      - |  177 | `		if( pOutVal ){` |
|      - |  178 | `			*(sxi32 *)pOutVal = 0;` |
|      - |  179 | `		}` |
|      - |  180 | `		return SXERR_EMPTY;` |
|      - |  181 | `	}` |
|      - |  182 | `#endif` |
|  79745 |  183 | `	zEnd = &zSrc[nLen];` |
|  79757 |  184 | `	while(zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|     14 |  185 | `		zSrc++;` |
|      2 |  186 | `	}` |
|  79745 |  187 | `	if( zSrc < zEnd && ( zSrc[0] == '-' \|\| zSrc[0] == '+' ) ){` |
|      3 |  188 | `		isNeg = (zSrc[0] == '-') ? TRUE :FALSE;` |
|      3 |  189 | `		zSrc++;` |
|      1 |  190 | `	}` |
|      - |  191 | `	/* Skip leading zero */` |
|  79745 |  192 | `	while(zSrc < zEnd && zSrc[0] == '0' ){` |
|    ! 0 |  193 | `		zSrc++;` |
|    ! 0 |  194 | `	}` |
|  79745 |  195 | `	i = 19;` |
|  79745 |  196 | `	if( (sxu32)(zEnd-zSrc) >= 19 ){` |
|     11 |  197 | `		i = SyMemcmp(zSrc,isNeg ? SXINT64_MIN_STR : SXINT64_MAX_STR,19) <= 0 ? 19 : 18 ;` |
|      5 |  198 | `	}` |
|  79745 |  199 | `	nVal = 0;` |
|  40120 |  200 | `	for(;;){` |
|  80245 |  201 | `		if(zSrc >= zEnd \|\| !i \|\| !SyisDigit(zSrc[0])){ break; } nVal = nVal * 10 + ( zSrc[0] - '0' ) ; --i ; zSrc++;` |
|  80049 |  202 | `		if(zSrc >= zEnd \|\| !i \|\| !SyisDigit(zSrc[0])){ break; } nVal = nVal * 10 + ( zSrc[0] - '0' ) ; --i ; zSrc++;` |
|  10681 |  203 | `		if(zSrc >= zEnd \|\| !i \|\| !SyisDigit(zSrc[0])){ break; } nVal = nVal * 10 + ( zSrc[0] - '0' ) ; --i ; zSrc++;` |
|   1123 |  204 | `		if(zSrc >= zEnd \|\| !i \|\| !SyisDigit(zSrc[0])){ break; } nVal = nVal * 10 + ( zSrc[0] - '0' ) ; --i ; zSrc++;` |
|      2 |  205 | `	}` |
|      - |  206 | `	/* Skip trailing spaces */` |
|  79757 |  207 | `	while(zSrc < zEnd && SyisSpace(zSrc[0])){` |
|     14 |  208 | `		zSrc++;` |
|      2 |  209 | `	}` |
|  79745 |  210 | `	if( zRest ){` |
|    ! 0 |  211 | `		*zRest = (char *)zSrc;` |
|    ! 0 |  212 | `	}` |
|  79745 |  213 | `	if( pOutVal ){` |
|  79745 |  214 | `		if( isNeg == TRUE && nVal != 0 ){` |
|      3 |  215 | `			nVal = -nVal;` |
|      1 |  216 | `		}` |
|  79745 |  217 | `		*(sxi64 *)pOutVal = nVal;` |
|  39870 |  218 | `	}` |
|  79745 |  219 | `	return (zSrc >= zEnd) ? SXRET_OK : SXERR_SYNTAX;` |
|      5 |  220 | `}` |
|   1604 |  221 | `PH7_PRIVATE sxi32 SyHexToint(sxi32 c)` |
|      2 |  222 | `{` |
|   1606 |  223 | `	switch(c){` |
|    554 |  224 | `	case '0': return 0;` |
|     90 |  225 | `	case '1': return 1;` |
|     49 |  226 | `	case '2': return 2;` |
|     67 |  227 | `	case '3': return 3;` |
|     67 |  228 | `	case '4': return 4;` |
|     43 |  229 | `	case '5': return 5;` |
|     41 |  230 | `	case '6': return 6;` |
|     21 |  231 | `	case '7': return 7;` |
|     49 |  232 | `	case '8': return 8;` |
|     63 |  233 | `	case '9': return 9;` |
|     87 |  234 | `	case 'A': case 'a': return 10;` |
|     31 |  235 | `	case 'B': case 'b': return 11;` |
|     73 |  236 | `	case 'C': case 'c': return 12;` |
|     31 |  237 | `	case 'D': case 'd': return 13;` |
|     61 |  238 | `	case 'E': case 'e': return 14;` |
|    293 |  239 | `	case 'F': case 'f': return 15;` |
|      - |  240 | `	}` |
|      3 |  241 | `	return -1;` |
|    804 |  242 | `}` |
|     90 |  243 | `PH7_PRIVATE sxi32 SyHexStrToInt64(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)` |
|      1 |  244 | `{` |
|      - |  245 | `	const char *zIn,*zEnd;` |
|     91 |  246 | `	int isNeg = FALSE;` |
|     91 |  247 | `	sxi64 nVal = 0;` |
|      - |  248 | `#if defined(UNTRUST)` |
|      - |  249 | `	if( SX_EMPTY_STR(zSrc) ){` |
|      - |  250 | `		if( pOutVal ){` |
|      - |  251 | `			*(sxi32 *)pOutVal = 0;` |
|      - |  252 | `		}` |
|      - |  253 | `		return SXERR_EMPTY;` |
|      - |  254 | `	}` |
|      - |  255 | `#endif` |
|     91 |  256 | `	zEnd = &zSrc[nLen];` |
|     91 |  257 | `	while( zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|    ! 0 |  258 | `		zSrc++;` |
|    ! 0 |  259 | `	}` |
|     91 |  260 | `	if( zSrc < zEnd && ( *zSrc == '-' \|\| *zSrc == '+' ) ){` |
|    ! 0 |  261 | `		isNeg = (zSrc[0] == '-') ? TRUE :FALSE;` |
|    ! 0 |  262 | `		zSrc++;` |
|    ! 0 |  263 | `	}` |
|     91 |  264 | `	if( zSrc < &zEnd[-2] && zSrc[0] == '0' && (zSrc[1] == 'x' \|\| zSrc[1] == 'X') ){` |
|      - |  265 | `		/* Bypass hex prefix */` |
|     71 |  266 | `		zSrc += sizeof(char) * 2;` |
|     35 |  267 | `	}` |
|      - |  268 | `	/* Skip leading zero */` |
|    101 |  269 | `	while(zSrc < zEnd && zSrc[0] == '0' ){` |
|     11 |  270 | `		zSrc++;` |
|      1 |  271 | `	}` |
|     91 |  272 | `	zIn = zSrc;` |
|     85 |  273 | `	for(;;){` |
|    171 |  274 | `		if(zSrc >= zEnd \|\| !SyisHex(zSrc[0]) \|\| (int)(zSrc-zIn) > 15){` |
|     27 |  275 | `			break;` |
|      - |  276 | `		}` |
|    119 |  277 | `		nVal = nVal * 16 + SyHexToint(zSrc[0]);` |
|    119 |  278 | `		zSrc++;` |
|    119 |  279 | `		if(zSrc >= zEnd \|\| !SyisHex(zSrc[0]) \|\| (int)(zSrc-zIn) > 15){` |
|      8 |  280 | `			break;` |
|      - |  281 | `		}` |
|    105 |  282 | `		nVal = nVal * 16 + SyHexToint(zSrc[0]);` |
|    105 |  283 | `		zSrc++;` |
|    105 |  284 | `		if(zSrc >= zEnd \|\| !SyisHex(zSrc[0]) \|\| (int)(zSrc-zIn) > 15){` |
|     12 |  285 | `			break;` |
|      - |  286 | `		}` |
|     83 |  287 | `		nVal = nVal * 16 + SyHexToint(zSrc[0]);` |
|     83 |  288 | `		zSrc++;` |
|     83 |  289 | `		if(zSrc >= zEnd \|\| !SyisHex(zSrc[0]) \|\| (int)(zSrc-zIn) > 15){` |
|      2 |  290 | `			break;` |
|      - |  291 | `		}` |
|     81 |  292 | `		nVal = nVal * 16 + SyHexToint(zSrc[0]);` |
|     81 |  293 | `		zSrc++;` |
|      1 |  294 | `	}` |
|     91 |  295 | `	while( zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|    ! 0 |  296 | `		zSrc++;` |
|    ! 0 |  297 | `	}` |
|     91 |  298 | `	if( zRest ){` |
|    ! 0 |  299 | `		*zRest = zSrc;` |
|    ! 0 |  300 | `	}` |
|     91 |  301 | `	if( pOutVal ){` |
|     91 |  302 | `		if( isNeg == TRUE && nVal != 0 ){` |
|    ! 0 |  303 | `			nVal = -nVal;` |
|    ! 0 |  304 | `		}` |
|     91 |  305 | `		*(sxi64 *)pOutVal = nVal;` |
|     45 |  306 | `	}` |
|     91 |  307 | `	return zSrc >= zEnd ? SXRET_OK : SXERR_SYNTAX;` |
|      1 |  308 | `}` |
|     68 |  309 | `PH7_PRIVATE sxi32 SyOctalStrToInt64(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)` |
|      1 |  310 | `{` |
|      - |  311 | `	const char *zIn,*zEnd;` |
|     69 |  312 | `	int isNeg = FALSE;` |
|     69 |  313 | `	sxi64 nVal = 0;` |
|      - |  314 | `	int c;` |
|      - |  315 | `#if defined(UNTRUST)` |
|      - |  316 | `	if( SX_EMPTY_STR(zSrc) ){` |
|      - |  317 | `		if( pOutVal ){` |
|      - |  318 | `			*(sxi32 *)pOutVal = 0;` |
|      - |  319 | `		}` |
|      - |  320 | `		return SXERR_EMPTY;` |
|      - |  321 | `	}` |
|      - |  322 | `#endif` |
|     69 |  323 | `	zEnd = &zSrc[nLen];` |
|     69 |  324 | `	while(zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|    ! 0 |  325 | `		zSrc++;` |
|    ! 0 |  326 | `	}` |
|     69 |  327 | `	if( zSrc < zEnd && ( zSrc[0] == '-' \|\| zSrc[0] == '+' ) ){` |
|    ! 0 |  328 | `		isNeg = (zSrc[0] == '-') ? TRUE :FALSE;` |
|    ! 0 |  329 | `		zSrc++;` |
|    ! 0 |  330 | `	}` |
|      - |  331 | `	/* Skip leading zero */` |
|    129 |  332 | `	while(zSrc < zEnd && zSrc[0] == '0' ){` |
|     61 |  333 | `		zSrc++;` |
|      1 |  334 | `	}` |
|     69 |  335 | `	zIn = zSrc;` |
|     34 |  336 | `	for(;;){` |
|     69 |  337 | `		if(zSrc >= zEnd \|\| !SyisDigit(zSrc[0])){ break; } if( (c=zSrc[0]-'0') > 7 \|\| (int)(zSrc-zIn) > 20){ break;} nVal = nVal * 8 +  c; zSrc++;` |
|     69 |  338 | `		if(zSrc >= zEnd \|\| !SyisDigit(zSrc[0])){ break; } if( (c=zSrc[0]-'0') > 7 \|\| (int)(zSrc-zIn) > 20){ break;} nVal = nVal * 8 +  c; zSrc++;` |
|     61 |  339 | `		if(zSrc >= zEnd \|\| !SyisDigit(zSrc[0])){ break; } if( (c=zSrc[0]-'0') > 7 \|\| (int)(zSrc-zIn) > 20){ break;} nVal = nVal * 8 +  c; zSrc++;` |
|     43 |  340 | `		if(zSrc >= zEnd \|\| !SyisDigit(zSrc[0])){ break; } if( (c=zSrc[0]-'0') > 7 \|\| (int)(zSrc-zIn) > 20){ break;} nVal = nVal * 8 +  c; zSrc++;` |
|    ! 0 |  341 | `	}` |
|      - |  342 | `	/* Skip trailing spaces */` |
|     69 |  343 | `	while(zSrc < zEnd && SyisSpace(zSrc[0])){` |
|    ! 0 |  344 | `		zSrc++;` |
|    ! 0 |  345 | `	}` |
|     69 |  346 | `	if( zRest ){` |
|    ! 0 |  347 | `		*zRest = zSrc;` |
|    ! 0 |  348 | `	}` |
|     69 |  349 | `	if( pOutVal ){` |
|     69 |  350 | `		if( isNeg == TRUE && nVal != 0 ){` |
|    ! 0 |  351 | `			nVal = -nVal;` |
|    ! 0 |  352 | `		}` |
|     69 |  353 | `		*(sxi64 *)pOutVal = nVal;` |
|     34 |  354 | `	}` |
|     69 |  355 | `	return (zSrc >= zEnd) ? SXRET_OK : SXERR_SYNTAX;` |
|      1 |  356 | `}` |
|    288 |  357 | `PH7_PRIVATE sxi32 SyBinaryStrToInt64(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)` |
|      1 |  358 | `{` |
|      - |  359 | `	const char *zIn,*zEnd;` |
|    289 |  360 | `	int isNeg = FALSE;` |
|    289 |  361 | `	sxi64 nVal = 0;` |
|      - |  362 | `	int c;` |
|      - |  363 | `#if defined(UNTRUST)` |
|      - |  364 | `	if( SX_EMPTY_STR(zSrc) ){` |
|      - |  365 | `		if( pOutVal ){` |
|      - |  366 | `			*(sxi32 *)pOutVal = 0;` |
|      - |  367 | `		}` |
|      - |  368 | `		return SXERR_EMPTY;` |
|      - |  369 | `	}` |
|      - |  370 | `#endif` |
|    289 |  371 | `	zEnd = &zSrc[nLen];` |
|    289 |  372 | `	while(zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|    ! 0 |  373 | `		zSrc++;` |
|    ! 0 |  374 | `	}` |
|    289 |  375 | `	if( zSrc < zEnd && ( zSrc[0] == '-' \|\| zSrc[0] == '+' ) ){` |
|    ! 0 |  376 | `		isNeg = (zSrc[0] == '-') ? TRUE :FALSE;` |
|    ! 0 |  377 | `		zSrc++;` |
|    ! 0 |  378 | `	}` |
|    289 |  379 | `	if( zSrc < &zEnd[-2] && zSrc[0] == '0' && (zSrc[1] == 'b' \|\| zSrc[1] == 'B') ){` |
|      - |  380 | `		/* Bypass binary prefix */` |
|    277 |  381 | `		zSrc += sizeof(char) * 2;` |
|    138 |  382 | `	}` |
|      - |  383 | `	/* Skip leading zero */` |
|    337 |  384 | `	while(zSrc < zEnd && zSrc[0] == '0' ){` |
|     49 |  385 | `		zSrc++;` |
|      1 |  386 | `	}` |
|    289 |  387 | `	zIn = zSrc;` |
|    304 |  388 | `	for(;;){` |
|    609 |  389 | `		if(zSrc >= zEnd \|\| (zSrc[0] != '1' && zSrc[0] != '0') \|\| (int)(zSrc-zIn) > 62){ break; } c = zSrc[0] - '0'; nVal = (nVal << 1) + c; zSrc++;` |
|    435 |  390 | `		if(zSrc >= zEnd \|\| (zSrc[0] != '1' && zSrc[0] != '0') \|\| (int)(zSrc-zIn) > 62){ break; } c = zSrc[0] - '0'; nVal = (nVal << 1) + c; zSrc++;` |
|    401 |  391 | `		if(zSrc >= zEnd \|\| (zSrc[0] != '1' && zSrc[0] != '0') \|\| (int)(zSrc-zIn) > 62){ break; } c = zSrc[0] - '0'; nVal = (nVal << 1) + c; zSrc++;` |
|    357 |  392 | `		if(zSrc >= zEnd \|\| (zSrc[0] != '1' && zSrc[0] != '0') \|\| (int)(zSrc-zIn) > 62){ break; } c = zSrc[0] - '0'; nVal = (nVal << 1) + c; zSrc++;` |
|      1 |  393 | `	}` |
|      - |  394 | `	/* Skip trailing spaces */` |
|    289 |  395 | `	while(zSrc < zEnd && SyisSpace(zSrc[0])){` |
|    ! 0 |  396 | `		zSrc++;` |
|    ! 0 |  397 | `	}` |
|    289 |  398 | `	if( zRest ){` |
|    ! 0 |  399 | `		*zRest = zSrc;` |
|    ! 0 |  400 | `	}` |
|    289 |  401 | `	if( pOutVal ){` |
|    289 |  402 | `		if( isNeg == TRUE && nVal != 0 ){` |
|    ! 0 |  403 | `			nVal = -nVal;` |
|    ! 0 |  404 | `		}` |
|    289 |  405 | `		*(sxi64 *)pOutVal = nVal;` |
|    144 |  406 | `	}` |
|    289 |  407 | `	return (zSrc >= zEnd) ? SXRET_OK : SXERR_SYNTAX;` |
|      1 |  408 | `}` |
|    966 |  409 | `PH7_PRIVATE sxi32 SyStrToReal(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)` |
|      5 |  410 | `{` |
|      - |  411 | `	/* Correctly-rounded conversion via libc strtod (the byte-exact-floats` |
|      - |  412 | `	 * rule): the old hand-rolled accumulator kept only 15 significant` |
|      - |  413 | `	 * digits, clamped exponents to +/-30x (so 1e400 silently became 1e308` |
|      - |  414 | `	 * and 5e-324 became 5e-307) and drifted the low mantissa bits, making` |
|      - |  415 | `	 * float literals and string->float casts differ from php. The accepted` |
|      - |  416 | `	 * numeric-prefix grammar is kept from the old parser, including the ','` |
|      - |  417 | `	 * decimal separator: [ws][sign]D*[(.\|,)D*][(e\|E)[sign]D+][ws]. The` |
|      - |  418 | `	 * prefix is copied (',' -> '.') into a stack buffer for strtod — or a` |
|      - |  419 | `	 * heap copy for the rare number longer than the buffer (e.g. hundreds of` |
|      - |  420 | `	 * leading fractional zeros before the significant digits), falling back` |
|      - |  421 | `	 * to a truncated-mantissa-plus-exponent copy only if that allocation` |
|      - |  422 | `	 * fails. Everything is always consumed from the input. */` |
|      - |  423 | `	char zBuf[512];` |
|    971 |  424 | `	const char *zEnd = &zSrc[nLen];` |
|      - |  425 | `	const char *zNum;` |
|    971 |  426 | `	const char *zExpStart = 0;` |
|    971 |  427 | `	sxreal Val = 0.0;` |
|    971 |  428 | `	sxu32 nCopy = 0;` |
|    971 |  429 | `	int bDigit = 0;` |
|      - |  430 | `#ifdef UNTRUST` |
|      - |  431 | `	if( SX_EMPTY_STR(zSrc)  ){` |
|      - |  432 | `		if( pOutVal ){` |
|      - |  433 | `			*(sxreal *)pOutVal = 0.0;` |
|      - |  434 | `		}` |
|      - |  435 | `		return SXERR_EMPTY;` |
|      - |  436 | `	}` |
|      - |  437 | `#endif` |
|      - |  438 | `	/* Skip leading spaces */` |
|    975 |  439 | `	while( zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|      5 |  440 | `		zSrc++;` |
|      1 |  441 | `	}` |
|    971 |  442 | `	zNum = zSrc;` |
|      - |  443 | `	/* Sign (if exists) */` |
|    971 |  444 | `	if( zSrc < zEnd && (zSrc[0] == '-' \|\| zSrc[0] == '+' ) ){` |
|     10 |  445 | `		zSrc++;` |
|      4 |  446 | `	}` |
|      - |  447 | `	/* Integer part */` |
|   2231 |  448 | `	while( zSrc < zEnd && SyisDigit(zSrc[0]) ){` |
|   1265 |  449 | `		bDigit = 1;` |
|   1265 |  450 | `		zSrc++;` |
|      5 |  451 | `	}` |
|      - |  452 | `	/* Fractional part */` |
|    971 |  453 | `	if( zSrc < zEnd && ( zSrc[0] == '.' \|\| zSrc[0] == ',' ) ){` |
|    853 |  454 | `		zSrc++;` |
|   4093 |  455 | `		while( zSrc < zEnd && SyisDigit(zSrc[0]) ){` |
|   3245 |  456 | `			bDigit = 1;` |
|   3245 |  457 | `			zSrc++;` |
|      5 |  458 | `		}` |
|    424 |  459 | `	}` |
|      - |  460 | `	/* Exponent — consumed only when it carries at least one digit, like` |
|      - |  461 | `	 * strtod, so "1e+x" leaves the "e+" unconsumed. */` |
|    971 |  462 | `	if( bDigit && zSrc < zEnd && ( zSrc[0] == 'e' \|\| zSrc[0] == 'E' ) ){` |
|    168 |  463 | `		const char *zExp = &zSrc[1];` |
|    168 |  464 | `		if( zExp < zEnd && (zExp[0] == '-' \|\| zExp[0] == '+') ){` |
|     61 |  465 | `			zExp++;` |
|     30 |  466 | `		}` |
|    168 |  467 | `		if( zExp < zEnd && SyisDigit(zExp[0]) ){` |
|    168 |  468 | `			zExpStart = zSrc;` |
|    168 |  469 | `			zSrc = zExp;` |
|    512 |  470 | `			while( zSrc < zEnd && SyisDigit(zSrc[0]) ){` |
|    346 |  471 | `				zSrc++;` |
|      2 |  472 | `			}` |
|     83 |  473 | `		}` |
|     83 |  474 | `	}` |
|    971 |  475 | `	if( bDigit ){` |
|    971 |  476 | `		sxu32 i, nSpan = (sxu32)((zExpStart ? zExpStart : zSrc) - zNum);` |
|    971 |  477 | `		sxu32 nExp = zExpStart ? (sxu32)(zSrc - zExpStart) : 0;` |
|    971 |  478 | `		char *zDup = zBuf;` |
|    971 |  479 | `		sxu32 nDup = sizeof(zBuf);` |
|    971 |  480 | `		if( nSpan + nExp >= sizeof(zBuf) ){` |
|      3 |  481 | `			char *zHeap = (char *)malloc(nSpan + nExp + 1);` |
|      3 |  482 | `			if( zHeap ){` |
|      3 |  483 | `				zDup = zHeap;` |
|      3 |  484 | `				nDup = nSpan + nExp + 1;` |
|      1 |  485 | `			}` |
|      1 |  486 | `		}` |
|      - |  487 | `		{` |
|    971 |  488 | `			sxu32 nMantMax = nDup - 1 - (nExp < nDup - 1 ? nExp : 0);` |
|   6327 |  489 | `			for( i = 0 ; i < nSpan && nCopy < nMantMax ; i++ ){` |
|   5361 |  490 | `				zDup[nCopy++] = (zNum[i] == ',') ? '.' : zNum[i];` |
|   2683 |  491 | `			}` |
|      - |  492 | `			/* The exponent rides behind even a truncated mantissa: dropping` |
|      - |  493 | `			 * it would collapse "0.<hundreds of zeros>1e300" to 0.0. */` |
|   1541 |  494 | `			for( i = 0 ; i < nExp && nCopy < nDup - 1 ; i++ ){` |
|    572 |  495 | `				zDup[nCopy++] = zExpStart[i];` |
|    287 |  496 | `			}` |
|      - |  497 | `		}` |
|    971 |  498 | `		zDup[nCopy] = 0;` |
|    971 |  499 | `		Val = (sxreal)strtod(zDup,0);` |
|    971 |  500 | `		if( zDup != zBuf ){` |
|      3 |  501 | `			free(zDup);` |
|      1 |  502 | `		}` |
|    483 |  503 | `	}` |
|      - |  504 | `	/* Jump trailing spaces */` |
|    971 |  505 | `	while( zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|    ! 0 |  506 | `		zSrc++;` |
|    ! 0 |  507 | `	}` |
|    971 |  508 | `	if( zRest ){` |
|    ! 0 |  509 | `		*zRest = zSrc;` |
|    ! 0 |  510 | `	}` |
|    971 |  511 | `	if( pOutVal ){` |
|    971 |  512 | `		*(sxreal *)pOutVal = Val;` |
|    483 |  513 | `	}` |
|    971 |  514 | `	return zSrc >= zEnd ? SXRET_OK : SXERR_SYNTAX;` |
|      5 |  515 | `}` |
|      - |  516 |  |
