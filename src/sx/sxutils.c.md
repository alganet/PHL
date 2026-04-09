# src/sx/sxutils.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 316/445 lines (71.01%)

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
| 336746 |   11 | `PH7_PRIVATE sxi32 SyStrIsNumeric(const char *zSrc,sxu32 nLen,sxu8 *pReal,const char  **pzTail)` |
|      2 |   12 |  |
|      - |   13 | `	const char *zCur,*zEnd;` |
|      - |   14 | `#ifdef UNTRUST` |
|      - |   15 | `	if( SX_EMPTY_STR(zSrc) ){` |
|      - |   16 | `		return SXERR_EMPTY;` |
|      - |   17 | `	}` |
|      - |   18 | `#endif` |
| 336748 |   19 | `	zEnd = &zSrc[nLen];` |
|      - |   20 | `	/* Jump leading white spaces */` |
| 336752 |   21 | `	while( zSrc < zEnd && (unsigned char)zSrc[0] < 0xc0  && SyisSpace(zSrc[0]) ){` |
|      5 |   22 | `		zSrc++;` |
|      1 |   23 | `	}` |
| 336748 |   24 | `	if( zSrc < zEnd && (zSrc[0] == '+' \|\| zSrc[0] == '-') ){` |
|     82 |   25 | `		zSrc++;` |
|     40 |   26 | `	}` |
| 336748 |   27 | `	zCur = zSrc;` |
| 336748 |   28 | `	if( pReal ){` |
|    100 |   29 | `		*pReal = FALSE;` |
|     49 |   30 | `	}` |
| 168412 |   31 | `	for(;;){` |
| 336748 |   32 | `		if( zSrc >= zEnd \|\| (unsigned char)zSrc[0] >= 0xc0 \|\| !SyisDigit(zSrc[0]) ){` |
| 168364 |   33 | `			break;` |
|      - |   34 | `		}` |
|    101 |   35 | `		zSrc++;` |
|    101 |   36 | `		if( zSrc >= zEnd \|\| (unsigned char)zSrc[0] >= 0xc0 \|\| !SyisDigit(zSrc[0]) ){` |
|     34 |   37 | `			break;` |
|      - |   38 | `		}` |
|     35 |   39 | `		zSrc++;` |
|     35 |   40 | `		if( zSrc >= zEnd \|\| (unsigned char)zSrc[0] >= 0xc0 \|\| !SyisDigit(zSrc[0]) ){` |
|     10 |   41 | `			break;` |
|      - |   42 | `		}` |
|     17 |   43 | `		zSrc++;` |
|     17 |   44 | `		if( zSrc >= zEnd \|\| (unsigned char)zSrc[0] >= 0xc0 \|\| !SyisDigit(zSrc[0]) ){` |
|      9 |   45 | `			break;` |
|      - |   46 | `		}` |
|    ! 0 |   47 | `		zSrc++;` |
|    ! 0 |   48 | `	};` |
| 336748 |   49 | `	if( zSrc < zEnd && zSrc > zCur ){` |
|     23 |   50 | `		int c = zSrc[0];` |
|     23 |   51 | `		if( c == '.' ){` |
|     21 |   52 | `			zSrc++;` |
|     21 |   53 | `			if( pReal ){` |
|     13 |   54 | `				*pReal = TRUE;` |
|      6 |   55 | `			}` |
|     21 |   56 | `			if( pzTail ){` |
|    ! 0 |   57 | `				while( zSrc < zEnd && (unsigned char)zSrc[0] < 0xc0 && SyisDigit(zSrc[0]) ){` |
|    ! 0 |   58 | `					zSrc++;` |
|    ! 0 |   59 | `				}` |
|    ! 0 |   60 | `				if( zSrc < zEnd && (zSrc[0] == 'e' \|\| zSrc[0] == 'E') ){` |
|    ! 0 |   61 | `					zSrc++;` |
|    ! 0 |   62 | `					if( zSrc < zEnd && (zSrc[0] == '+' \|\| zSrc[0] == '-') ){` |
|    ! 0 |   63 | `						zSrc++;` |
|    ! 0 |   64 | `					}` |
|    ! 0 |   65 | `					while( zSrc < zEnd && (unsigned char)zSrc[0] < 0xc0 && SyisDigit(zSrc[0]) ){` |
|    ! 0 |   66 | `						zSrc++;` |
|    ! 0 |   67 | `					}` |
|    ! 0 |   68 | `				}` |
|      1 |   69 | `			}` |
|     13 |   70 | `		}else if( c == 'e' \|\| c == 'E' ){` |
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
|     11 |   84 | `	}` |
| 336748 |   85 | `	if( pzTail ){` |
|      - |   86 | `		/* Point to the non numeric part */` |
|    ! 0 |   87 | `		*pzTail = zSrc;` |
|    ! 0 |   88 | `	}` |
| 336748 |   89 | `	return zSrc > zCur ? SXRET_OK /* String prefix is numeric */ : SXERR_INVALID /* Not a digit stream */;` |
|      2 |   90 |  |
|      - |   91 | `#define SXINT32_MIN_STR		"2147483648"` |
|      - |   92 | `#define SXINT32_MAX_STR		"2147483647"` |
|      - |   93 | `#define SXINT64_MIN_STR		"9223372036854775808"` |
|      - |   94 | `#define SXINT64_MAX_STR		"9223372036854775807"` |
|     10 |   95 | `PH7_PRIVATE sxi32 SyStrToInt32(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)` |
|      1 |   96 |  |
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
|      1 |  167 |  |
|  53285 |  168 | `PH7_PRIVATE sxi32 SyStrToInt64(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)` |
|      2 |  169 |  |
|  53287 |  170 | `	int isNeg = FALSE;` |
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
|  53287 |  182 | `	zEnd = &zSrc[nLen];` |
|  53287 |  183 | `	while(zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|    ! 0 |  184 | `		zSrc++;` |
|    ! 0 |  185 | `	}` |
|  53287 |  186 | `	if( zSrc < zEnd && ( zSrc[0] == '-' \|\| zSrc[0] == '+' ) ){` |
|    ! 0 |  187 | `		isNeg = (zSrc[0] == '-') ? TRUE :FALSE;` |
|    ! 0 |  188 | `		zSrc++;` |
|    ! 0 |  189 | `	}` |
|      - |  190 | `	/* Skip leading zero */` |
|  53287 |  191 | `	while(zSrc < zEnd && zSrc[0] == '0' ){` |
|    ! 0 |  192 | `		zSrc++;` |
|    ! 0 |  193 | `	}` |
|  53287 |  194 | `	i = 19;` |
|  53287 |  195 | `	if( (sxu32)(zEnd-zSrc) >= 19 ){` |
|      5 |  196 | `		i = SyMemcmp(zSrc,isNeg ? SXINT64_MIN_STR : SXINT64_MAX_STR,19) <= 0 ? 19 : 18 ;` |
|      2 |  197 | `	}` |
|  53287 |  198 | `	nVal = 0;` |
|  26780 |  199 | `	for(;;){` |
|  53563 |  200 | `		if(zSrc >= zEnd \|\| !i \|\| !SyisDigit(zSrc[0])){ break; } nVal = nVal * 10 + ( zSrc[0] - '0' ) ; --i ; zSrc++;` |
|  53471 |  201 | `		if(zSrc >= zEnd \|\| !i \|\| !SyisDigit(zSrc[0])){ break; } nVal = nVal * 10 + ( zSrc[0] - '0' ) ; --i ; zSrc++;` |
|   7225 |  202 | `		if(zSrc >= zEnd \|\| !i \|\| !SyisDigit(zSrc[0])){ break; } nVal = nVal * 10 + ( zSrc[0] - '0' ) ; --i ; zSrc++;` |
|    794 |  203 | `		if(zSrc >= zEnd \|\| !i \|\| !SyisDigit(zSrc[0])){ break; } nVal = nVal * 10 + ( zSrc[0] - '0' ) ; --i ; zSrc++;` |
|      1 |  204 | `	}` |
|      - |  205 | `	/* Skip trailing spaces */` |
|  53287 |  206 | `	while(zSrc < zEnd && SyisSpace(zSrc[0])){` |
|    ! 0 |  207 | `		zSrc++;` |
|    ! 0 |  208 | `	}` |
|  53287 |  209 | `	if( zRest ){` |
|    ! 0 |  210 | `		*zRest = (char *)zSrc;` |
|    ! 0 |  211 | `	}` |
|  53287 |  212 | `	if( pOutVal ){` |
|  53287 |  213 | `		if( isNeg == TRUE && nVal != 0 ){` |
|    ! 0 |  214 | `			nVal = -nVal;` |
|    ! 0 |  215 | `		}` |
|  53287 |  216 | `		*(sxi64 *)pOutVal = nVal;` |
|  26642 |  217 | `	}` |
|  53287 |  218 | `	return (zSrc >= zEnd) ? SXRET_OK : SXERR_SYNTAX;` |
|      2 |  219 |  |
|    844 |  220 | `PH7_PRIVATE sxi32 SyHexToint(sxi32 c)` |
|      1 |  221 |  |
|    845 |  222 | `	switch(c){` |
|    403 |  223 | `	case '0': return 0;` |
|     41 |  224 | `	case '1': return 1;` |
|     19 |  225 | `	case '2': return 2;` |
|     23 |  226 | `	case '3': return 3;` |
|     47 |  227 | `	case '4': return 4;` |
|     27 |  228 | `	case '5': return 5;` |
|     27 |  229 | `	case '6': return 6;` |
|      9 |  230 | `	case '7': return 7;` |
|     19 |  231 | `	case '8': return 8;` |
|     17 |  232 | `	case '9': return 9;` |
|     35 |  233 | `	case 'A': case 'a': return 10;` |
|     19 |  234 | `	case 'B': case 'b': return 11;` |
|     29 |  235 | `	case 'C': case 'c': return 12;` |
|     11 |  236 | `	case 'D': case 'd': return 13;` |
|     21 |  237 | `	case 'E': case 'e': return 14;` |
|    113 |  238 | `	case 'F': case 'f': return 15;` |
|      - |  239 | `	}` |
|    ! 0 |  240 | `	return -1;` |
|    423 |  241 |  |
|     46 |  242 | `PH7_PRIVATE sxi32 SyHexStrToInt64(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)` |
|      1 |  243 |  |
|      - |  244 | `	const char *zIn,*zEnd;` |
|     47 |  245 | `	int isNeg = FALSE;` |
|     47 |  246 | `	sxi64 nVal = 0;` |
|      - |  247 | `#if defined(UNTRUST)` |
|      - |  248 | `	if( SX_EMPTY_STR(zSrc) ){` |
|      - |  249 | `		if( pOutVal ){` |
|      - |  250 | `			*(sxi32 *)pOutVal = 0;` |
|      - |  251 | `		}` |
|      - |  252 | `		return SXERR_EMPTY;` |
|      - |  253 | `	}` |
|      - |  254 | `#endif` |
|     47 |  255 | `	zEnd = &zSrc[nLen];` |
|     47 |  256 | `	while( zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|    ! 0 |  257 | `		zSrc++;` |
|    ! 0 |  258 | `	}` |
|     47 |  259 | `	if( zSrc < zEnd && ( *zSrc == '-' \|\| *zSrc == '+' ) ){` |
|    ! 0 |  260 | `		isNeg = (zSrc[0] == '-') ? TRUE :FALSE;` |
|    ! 0 |  261 | `		zSrc++;` |
|    ! 0 |  262 | `	}` |
|     47 |  263 | `	if( zSrc < &zEnd[-2] && zSrc[0] == '0' && (zSrc[1] == 'x' \|\| zSrc[1] == 'X') ){` |
|      - |  264 | `		/* Bypass hex prefix */` |
|     25 |  265 | `		zSrc += sizeof(char) * 2;` |
|     12 |  266 | `	}` |
|      - |  267 | `	/* Skip leading zero */` |
|     53 |  268 | `	while(zSrc < zEnd && zSrc[0] == '0' ){` |
|      7 |  269 | `		zSrc++;` |
|      1 |  270 | `	}` |
|     47 |  271 | `	zIn = zSrc;` |
|     31 |  272 | `	for(;;){` |
|     63 |  273 | `		if(zSrc >= zEnd \|\| !SyisHex(zSrc[0]) \|\| (int)(zSrc-zIn) > 15){` |
|      6 |  274 | `			break;` |
|      - |  275 | `		}` |
|     53 |  276 | `		nVal = nVal * 16 + SyHexToint(zSrc[0]);` |
|     53 |  277 | `		zSrc++;` |
|     53 |  278 | `		if(zSrc >= zEnd \|\| !SyisHex(zSrc[0]) \|\| (int)(zSrc-zIn) > 15){` |
|      9 |  279 | `			break;` |
|      - |  280 | `		}` |
|     37 |  281 | `		nVal = nVal * 16 + SyHexToint(zSrc[0]);` |
|     37 |  282 | `		zSrc++;` |
|     37 |  283 | `		if(zSrc >= zEnd \|\| !SyisHex(zSrc[0]) \|\| (int)(zSrc-zIn) > 15){` |
|     11 |  284 | `			break;` |
|      - |  285 | `		}` |
|     17 |  286 | `		nVal = nVal * 16 + SyHexToint(zSrc[0]);` |
|     17 |  287 | `		zSrc++;` |
|     17 |  288 | `		if(zSrc >= zEnd \|\| !SyisHex(zSrc[0]) \|\| (int)(zSrc-zIn) > 15){` |
|    ! 0 |  289 | `			break;` |
|      - |  290 | `		}` |
|     17 |  291 | `		nVal = nVal * 16 + SyHexToint(zSrc[0]);` |
|     17 |  292 | `		zSrc++;` |
|      1 |  293 | `	}` |
|     47 |  294 | `	while( zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|    ! 0 |  295 | `		zSrc++;` |
|    ! 0 |  296 | `	}` |
|     47 |  297 | `	if( zRest ){` |
|    ! 0 |  298 | `		*zRest = zSrc;` |
|    ! 0 |  299 | `	}` |
|     47 |  300 | `	if( pOutVal ){` |
|     47 |  301 | `		if( isNeg == TRUE && nVal != 0 ){` |
|    ! 0 |  302 | `			nVal = -nVal;` |
|    ! 0 |  303 | `		}` |
|     47 |  304 | `		*(sxi64 *)pOutVal = nVal;` |
|     23 |  305 | `	}` |
|     47 |  306 | `	return zSrc >= zEnd ? SXRET_OK : SXERR_SYNTAX;` |
|      1 |  307 |  |
|     41 |  308 | `PH7_PRIVATE sxi32 SyOctalStrToInt64(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)` |
|      1 |  309 |  |
|      - |  310 | `	const char *zIn,*zEnd;` |
|     42 |  311 | `	int isNeg = FALSE;` |
|     42 |  312 | `	sxi64 nVal = 0;` |
|      - |  313 | `	int c;` |
|      - |  314 | `#if defined(UNTRUST)` |
|      - |  315 | `	if( SX_EMPTY_STR(zSrc) ){` |
|      - |  316 | `		if( pOutVal ){` |
|      - |  317 | `			*(sxi32 *)pOutVal = 0;` |
|      - |  318 | `		}` |
|      - |  319 | `		return SXERR_EMPTY;` |
|      - |  320 | `	}` |
|      - |  321 | `#endif` |
|     42 |  322 | `	zEnd = &zSrc[nLen];` |
|     42 |  323 | `	while(zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|    ! 0 |  324 | `		zSrc++;` |
|    ! 0 |  325 | `	}` |
|     42 |  326 | `	if( zSrc < zEnd && ( zSrc[0] == '-' \|\| zSrc[0] == '+' ) ){` |
|    ! 0 |  327 | `		isNeg = (zSrc[0] == '-') ? TRUE :FALSE;` |
|    ! 0 |  328 | `		zSrc++;` |
|    ! 0 |  329 | `	}` |
|      - |  330 | `	/* Skip leading zero */` |
|     77 |  331 | `	while(zSrc < zEnd && zSrc[0] == '0' ){` |
|     36 |  332 | `		zSrc++;` |
|      1 |  333 | `	}` |
|     42 |  334 | `	zIn = zSrc;` |
|     21 |  335 | `	for(;;){` |
|     42 |  336 | `		if(zSrc >= zEnd \|\| !SyisDigit(zSrc[0])){ break; } if( (c=zSrc[0]-'0') > 7 \|\| (int)(zSrc-zIn) > 20){ break;} nVal = nVal * 8 +  c; zSrc++;` |
|     38 |  337 | `		if(zSrc >= zEnd \|\| !SyisDigit(zSrc[0])){ break; } if( (c=zSrc[0]-'0') > 7 \|\| (int)(zSrc-zIn) > 20){ break;} nVal = nVal * 8 +  c; zSrc++;` |
|     33 |  338 | `		if(zSrc >= zEnd \|\| !SyisDigit(zSrc[0])){ break; } if( (c=zSrc[0]-'0') > 7 \|\| (int)(zSrc-zIn) > 20){ break;} nVal = nVal * 8 +  c; zSrc++;` |
|     15 |  339 | `		if(zSrc >= zEnd \|\| !SyisDigit(zSrc[0])){ break; } if( (c=zSrc[0]-'0') > 7 \|\| (int)(zSrc-zIn) > 20){ break;} nVal = nVal * 8 +  c; zSrc++;` |
|    ! 0 |  340 | `	}` |
|      - |  341 | `	/* Skip trailing spaces */` |
|     42 |  342 | `	while(zSrc < zEnd && SyisSpace(zSrc[0])){` |
|    ! 0 |  343 | `		zSrc++;` |
|    ! 0 |  344 | `	}` |
|     42 |  345 | `	if( zRest ){` |
|    ! 0 |  346 | `		*zRest = zSrc;` |
|    ! 0 |  347 | `	}` |
|     42 |  348 | `	if( pOutVal ){` |
|     42 |  349 | `		if( isNeg == TRUE && nVal != 0 ){` |
|    ! 0 |  350 | `			nVal = -nVal;` |
|    ! 0 |  351 | `		}` |
|     42 |  352 | `		*(sxi64 *)pOutVal = nVal;` |
|     21 |  353 | `	}` |
|     42 |  354 | `	return (zSrc >= zEnd) ? SXRET_OK : SXERR_SYNTAX;` |
|      1 |  355 |  |
|    242 |  356 | `PH7_PRIVATE sxi32 SyBinaryStrToInt64(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)` |
|      1 |  357 |  |
|      - |  358 | `	const char *zIn,*zEnd;` |
|    243 |  359 | `	int isNeg = FALSE;` |
|    243 |  360 | `	sxi64 nVal = 0;` |
|      - |  361 | `	int c;` |
|      - |  362 | `#if defined(UNTRUST)` |
|      - |  363 | `	if( SX_EMPTY_STR(zSrc) ){` |
|      - |  364 | `		if( pOutVal ){` |
|      - |  365 | `			*(sxi32 *)pOutVal = 0;` |
|      - |  366 | `		}` |
|      - |  367 | `		return SXERR_EMPTY;` |
|      - |  368 | `	}` |
|      - |  369 | `#endif` |
|    243 |  370 | `	zEnd = &zSrc[nLen];` |
|    243 |  371 | `	while(zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|    ! 0 |  372 | `		zSrc++;` |
|    ! 0 |  373 | `	}` |
|    243 |  374 | `	if( zSrc < zEnd && ( zSrc[0] == '-' \|\| zSrc[0] == '+' ) ){` |
|    ! 0 |  375 | `		isNeg = (zSrc[0] == '-') ? TRUE :FALSE;` |
|    ! 0 |  376 | `		zSrc++;` |
|    ! 0 |  377 | `	}` |
|    243 |  378 | `	if( zSrc < &zEnd[-2] && zSrc[0] == '0' && (zSrc[1] == 'b' \|\| zSrc[1] == 'B') ){` |
|      - |  379 | `		/* Bypass binary prefix */` |
|    233 |  380 | `		zSrc += sizeof(char) * 2;` |
|    116 |  381 | `	}` |
|      - |  382 | `	/* Skip leading zero */` |
|    291 |  383 | `	while(zSrc < zEnd && zSrc[0] == '0' ){` |
|     49 |  384 | `		zSrc++;` |
|      1 |  385 | `	}` |
|    243 |  386 | `	zIn = zSrc;` |
|    217 |  387 | `	for(;;){` |
|    435 |  388 | `		if(zSrc >= zEnd \|\| (zSrc[0] != '1' && zSrc[0] != '0') \|\| (int)(zSrc-zIn) > 62){ break; } c = zSrc[0] - '0'; nVal = (nVal << 1) + c; zSrc++;` |
|    289 |  389 | `		if(zSrc >= zEnd \|\| (zSrc[0] != '1' && zSrc[0] != '0') \|\| (int)(zSrc-zIn) > 62){ break; } c = zSrc[0] - '0'; nVal = (nVal << 1) + c; zSrc++;` |
|    257 |  390 | `		if(zSrc >= zEnd \|\| (zSrc[0] != '1' && zSrc[0] != '0') \|\| (int)(zSrc-zIn) > 62){ break; } c = zSrc[0] - '0'; nVal = (nVal << 1) + c; zSrc++;` |
|    223 |  391 | `		if(zSrc >= zEnd \|\| (zSrc[0] != '1' && zSrc[0] != '0') \|\| (int)(zSrc-zIn) > 62){ break; } c = zSrc[0] - '0'; nVal = (nVal << 1) + c; zSrc++;` |
|      1 |  392 | `	}` |
|      - |  393 | `	/* Skip trailing spaces */` |
|    243 |  394 | `	while(zSrc < zEnd && SyisSpace(zSrc[0])){` |
|    ! 0 |  395 | `		zSrc++;` |
|    ! 0 |  396 | `	}` |
|    243 |  397 | `	if( zRest ){` |
|    ! 0 |  398 | `		*zRest = zSrc;` |
|    ! 0 |  399 | `	}` |
|    243 |  400 | `	if( pOutVal ){` |
|    243 |  401 | `		if( isNeg == TRUE && nVal != 0 ){` |
|    ! 0 |  402 | `			nVal = -nVal;` |
|    ! 0 |  403 | `		}` |
|    243 |  404 | `		*(sxi64 *)pOutVal = nVal;` |
|    121 |  405 | `	}` |
|    243 |  406 | `	return (zSrc >= zEnd) ? SXRET_OK : SXERR_SYNTAX;` |
|      1 |  407 |  |
|    446 |  408 | `PH7_PRIVATE sxi32 SyStrToReal(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)` |
|      2 |  409 |  |
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
|    448 |  424 | `	sxu8 neg = FALSE;` |
|    448 |  425 | `	sxreal Val = 0.0;` |
|      - |  426 | `	const char *zEnd;` |
|      - |  427 | `	sxi32 Lim,exp;` |
|    448 |  428 | `	sxreal *p = 0;` |
|      - |  429 | `#ifdef UNTRUST` |
|      - |  430 | `	if( SX_EMPTY_STR(zSrc)  ){` |
|      - |  431 | `		if( pOutVal ){` |
|      - |  432 | `			*(sxreal *)pOutVal = 0.0;` |
|      - |  433 | `		}` |
|      - |  434 | `		return SXERR_EMPTY;` |
|      - |  435 | `	}` |
|      - |  436 | `#endif` |
|      - |  437 | `	/* Define local limits and end pointer used by the parsing loops */` |
|    448 |  438 | `	zEnd = &zSrc[nLen];` |
|    448 |  439 | `	Lim = SXDBL_DIG;` |
|      - |  440 | `	/* Skip leading spaces */` |
|    452 |  441 | `	while( zSrc < zEnd && SyisSpace(zSrc[0]) ) zSrc++;` |
|      - |  442 |  |
|      - |  443 | `	/* Sign (if exists) */` |
|    448 |  444 | `	if( zSrc < zEnd && (zSrc[0] == '-' \|\| zSrc[0] == '+' ) ){` |
|      5 |  445 | `		neg =  zSrc[0] == '-' ? TRUE : FALSE ;` |
|      5 |  446 | `		zSrc++;` |
|      2 |  447 | `	}` |
|      - |  448 |  |
|      - |  449 | `	/* Integer part */` |
|    223 |  450 | `	for(;;){` |
|    448 |  451 | `		if(zSrc >= zEnd \|\| !Lim \|\| !SyisDigit(zSrc[0])){` |
|    ! 0 |  452 | `			break;` |
|      - |  453 | `		}` |
|    448 |  454 | `		Val = Val * 10.0 + (zSrc[0] - '0');` |
|    448 |  455 | `		zSrc++;` |
|    448 |  456 | `		--Lim;` |
|    448 |  457 | `		if(zSrc >= zEnd \|\| !Lim \|\| !SyisDigit(zSrc[0])){` |
|    221 |  458 | `			break;` |
|      - |  459 | `		}` |
|      9 |  460 | `		Val = Val * 10.0 + (zSrc[0] - '0');` |
|      9 |  461 | `		zSrc++;` |
|      9 |  462 | `		--Lim;` |
|      9 |  463 | `		if(zSrc >= zEnd \|\| !Lim \|\| !SyisDigit(zSrc[0])){` |
|      3 |  464 | `			break;` |
|      - |  465 | `		}` |
|      5 |  466 | `		Val = Val * 10.0 + (zSrc[0] - '0');` |
|      5 |  467 | `		zSrc++;` |
|      5 |  468 | `		--Lim;` |
|      5 |  469 | `		if(zSrc >= zEnd \|\| !Lim \|\| !SyisDigit(zSrc[0])){` |
|      3 |  470 | `			break;` |
|      - |  471 | `		}` |
|    ! 0 |  472 | `		Val = Val * 10.0 + (zSrc[0] - '0');` |
|    ! 0 |  473 | `		zSrc++;` |
|    ! 0 |  474 | `		--Lim;` |
|    ! 0 |  475 | `	}` |
|    448 |  476 | `	Lim = SXDBL_DIG ;` |
|    223 |  477 | `	for(;;){` |
|    448 |  478 | `		if(zSrc >= zEnd \|\| !Lim \|\| !SyisDigit(zSrc[0])){` |
|    225 |  479 | `			break;` |
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
|    448 |  503 | `	if( zSrc < zEnd && ( zSrc[0] == '.' \|\| zSrc[0] == ',' ) ){` |
|    428 |  504 | `		sxreal dec = 1.0;` |
|    428 |  505 | `		zSrc++;` |
|    294 |  506 | `		for(;;){` |
|    590 |  507 | `			if(zSrc >= zEnd \|\| !Lim \|\| !SyisDigit(zSrc[0])){` |
|     24 |  508 | `				break;` |
|      - |  509 | `			}` |
|    544 |  510 | `			Val = Val * 10.0 + (zSrc[0] - '0');` |
|    544 |  511 | `			dec *= 10.0;` |
|    544 |  512 | `			zSrc++;` |
|    544 |  513 | `			--Lim;` |
|    544 |  514 | `			if(zSrc >= zEnd \|\| !Lim \|\| !SyisDigit(zSrc[0])){` |
|    123 |  515 | `				break;` |
|      - |  516 | `			}` |
|    302 |  517 | `			Val = Val * 10.0 + (zSrc[0] - '0');` |
|    302 |  518 | `			dec *= 10.0;` |
|    302 |  519 | `			zSrc++;` |
|    302 |  520 | `			--Lim;` |
|    302 |  521 | `			if(zSrc >= zEnd \|\| !Lim \|\| !SyisDigit(zSrc[0])){` |
|     33 |  522 | `				break;` |
|      - |  523 | `			}` |
|    239 |  524 | `			Val = Val * 10.0 + (zSrc[0] - '0');` |
|    239 |  525 | `			dec *= 10.0;` |
|    239 |  526 | `			zSrc++;` |
|    239 |  527 | `			--Lim;` |
|    239 |  528 | `			if(zSrc >= zEnd \|\| !Lim \|\| !SyisDigit(zSrc[0])){` |
|     39 |  529 | `				break;` |
|      - |  530 | `			}` |
|    163 |  531 | `			Val = Val * 10.0 + (zSrc[0] - '0');` |
|    163 |  532 | `			dec *= 10.0;` |
|    163 |  533 | `			zSrc++;` |
|    163 |  534 | `			--Lim;` |
|      1 |  535 | `		}` |
|    428 |  536 | `		Val /= dec;` |
|    213 |  537 | `	}` |
|    448 |  538 | `	if( neg == TRUE && Val != 0.0 ) {` |
|      5 |  539 | `		Val = -Val ;` |
|      2 |  540 | `	}` |
|    448 |  541 | `	if( Lim <= 0 ){` |
|      - |  542 | `		/* jump overflow digit */` |
|     69 |  543 | `		while( zSrc < zEnd ){` |
|     41 |  544 | `			if( zSrc[0] == 'e' \|\| zSrc[0] == 'E' ){` |
|    ! 0 |  545 | `				break;` |
|      - |  546 | `			}` |
|     41 |  547 | `			zSrc++;` |
|      1 |  548 | `		}` |
|     14 |  549 | `	}` |
|    448 |  550 | `	neg = FALSE;` |
|    448 |  551 | `	if( zSrc < zEnd && ( zSrc[0] == 'e' \|\| zSrc[0] == 'E' ) ){` |
|     37 |  552 | `		zSrc++;` |
|     37 |  553 | `		if( zSrc < zEnd && ( zSrc[0] == '-' \|\| zSrc[0] == '+') ){` |
|     17 |  554 | `			neg = zSrc[0] == '-' ? TRUE : FALSE ;` |
|     17 |  555 | `			zSrc++;` |
|      8 |  556 | `		}` |
|     37 |  557 | `		exp = 0;` |
|     79 |  558 | `		while( zSrc < zEnd && SyisDigit(zSrc[0]) && exp < SXDBL_MAX_EXP ){` |
|     43 |  559 | `			exp = exp * 10 + (zSrc[0] - '0');` |
|     43 |  560 | `			zSrc++;` |
|      1 |  561 | `		}` |
|     37 |  562 | `		if( neg  ){` |
|     13 |  563 | `			if( exp > SXDBL_MIN_EXP_PLUS ) exp = SXDBL_MIN_EXP_PLUS ;` |
|     31 |  564 | `		}else if ( exp > SXDBL_MAX_EXP ){` |
|    ! 0 |  565 | `			exp = SXDBL_MAX_EXP;` |
|    ! 0 |  566 | `		}` |
|    129 |  567 | `		for( p = (sxreal *)aTab ; exp ; exp >>= 1 , p++ ){` |
|     93 |  568 | `			if( exp & 01 ){` |
|     55 |  569 | `				if( neg ){` |
|     19 |  570 | `					Val /= *p ;` |
|     10 |  571 | `				}else{` |
|     37 |  572 | `					Val *= *p;` |
|      - |  573 | `				}` |
|     27 |  574 | `			}` |
|     47 |  575 | `		}` |
|     18 |  576 | `	}` |
|    448 |  577 | `	while( zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|    ! 0 |  578 | `		zSrc++;` |
|    ! 0 |  579 | `	}` |
|    448 |  580 | `	if( zRest ){` |
|    ! 0 |  581 | `		*zRest = zSrc;` |
|    ! 0 |  582 | `	}` |
|    448 |  583 | `	if( pOutVal ){` |
|    448 |  584 | `		*(sxreal *)pOutVal = Val;` |
|    223 |  585 | `	}` |
|    448 |  586 | `	return zSrc >= zEnd ? SXRET_OK : SXERR_SYNTAX;` |
|      2 |  587 |  |
|      - |  588 |  |
