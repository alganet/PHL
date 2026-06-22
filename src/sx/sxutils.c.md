# src/sx/sxutils.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 336/445 lines (75.51%)

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
| 427774 |   11 | `PH7_PRIVATE sxi32 SyStrIsNumeric(const char *zSrc,sxu32 nLen,sxu8 *pReal,const char  **pzTail)` |
|      5 |   12 |  |
|      - |   13 | `	const char *zCur,*zEnd;` |
|      - |   14 | `#ifdef UNTRUST` |
|      - |   15 | `	if( SX_EMPTY_STR(zSrc) ){` |
|      - |   16 | `		return SXERR_EMPTY;` |
|      - |   17 | `	}` |
|      - |   18 | `#endif` |
| 427779 |   19 | `	zEnd = &zSrc[nLen];` |
|      - |   20 | `	/* Jump leading white spaces */` |
| 427795 |   21 | `	while( zSrc < zEnd && (unsigned char)zSrc[0] < 0xc0  && SyisSpace(zSrc[0]) ){` |
|     17 |   22 | `		zSrc++;` |
|      1 |   23 | `	}` |
| 427779 |   24 | `	if( zSrc < zEnd && (zSrc[0] == '+' \|\| zSrc[0] == '-') ){` |
|     89 |   25 | `		zSrc++;` |
|     42 |   26 | `	}` |
| 427779 |   27 | `	zCur = zSrc;` |
| 427779 |   28 | `	if( pReal ){` |
|    243 |   29 | `		*pReal = FALSE;` |
|    119 |   30 | `	}` |
| 213846 |   31 | `	for(;;){` |
| 427779 |   32 | `		if( zSrc >= zEnd \|\| (unsigned char)zSrc[0] >= 0xc0 \|\| !SyisDigit(zSrc[0]) ){` |
| 213757 |   33 | `			break;` |
|      - |   34 | `		}` |
|    191 |   35 | `		zSrc++;` |
|    191 |   36 | `		if( zSrc >= zEnd \|\| (unsigned char)zSrc[0] >= 0xc0 \|\| !SyisDigit(zSrc[0]) ){` |
|     71 |   37 | `			break;` |
|      - |   38 | `		}` |
|     53 |   39 | `		zSrc++;` |
|     53 |   40 | `		if( zSrc >= zEnd \|\| (unsigned char)zSrc[0] >= 0xc0 \|\| !SyisDigit(zSrc[0]) ){` |
|     20 |   41 | `			break;` |
|      - |   42 | `		}` |
|     17 |   43 | `		zSrc++;` |
|     17 |   44 | `		if( zSrc >= zEnd \|\| (unsigned char)zSrc[0] >= 0xc0 \|\| !SyisDigit(zSrc[0]) ){` |
|      9 |   45 | `			break;` |
|      - |   46 | `		}` |
|    ! 0 |   47 | `		zSrc++;` |
|    ! 0 |   48 | `	};` |
| 427779 |   49 | `	if( zSrc < zEnd && zSrc > zCur ){` |
|     59 |   50 | `		int c = zSrc[0];` |
|     59 |   51 | `		if( c == '.' ){` |
|     40 |   52 | `			zSrc++;` |
|     40 |   53 | `			if( pReal ){` |
|     32 |   54 | `				*pReal = TRUE;` |
|     15 |   55 | `			}` |
|     40 |   56 | `			if( pzTail ){` |
|     38 |   57 | `				while( zSrc < zEnd && (unsigned char)zSrc[0] < 0xc0 && SyisDigit(zSrc[0]) ){` |
|     24 |   58 | `					zSrc++;` |
|      2 |   59 | `				}` |
|     16 |   60 | `				if( zSrc < zEnd && (zSrc[0] == 'e' \|\| zSrc[0] == 'E') ){` |
|    ! 0 |   61 | `					zSrc++;` |
|    ! 0 |   62 | `					if( zSrc < zEnd && (zSrc[0] == '+' \|\| zSrc[0] == '-') ){` |
|    ! 0 |   63 | `						zSrc++;` |
|    ! 0 |   64 | `					}` |
|    ! 0 |   65 | `					while( zSrc < zEnd && (unsigned char)zSrc[0] < 0xc0 && SyisDigit(zSrc[0]) ){` |
|    ! 0 |   66 | `						zSrc++;` |
|    ! 0 |   67 | `					}` |
|    ! 0 |   68 | `				}` |
|      9 |   69 | `			}` |
|     40 |   70 | `		}else if( c == 'e' \|\| c == 'E' ){` |
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
|     28 |   84 | `	}` |
| 427779 |   85 | `	if( pzTail ){` |
|      - |   86 | `		/* Point to the non numeric part */` |
|    111 |   87 | `		*pzTail = zSrc;` |
|     53 |   88 | `	}` |
| 427779 |   89 | `	return zSrc > zCur ? SXRET_OK /* String prefix is numeric */ : SXERR_INVALID /* Not a digit stream */;` |
|      5 |   90 |  |
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
|  67742 |  168 | `PH7_PRIVATE sxi32 SyStrToInt64(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)` |
|      5 |  169 |  |
|  67747 |  170 | `	int isNeg = FALSE;` |
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
|  67747 |  182 | `	zEnd = &zSrc[nLen];` |
|  67755 |  183 | `	while(zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|      9 |  184 | `		zSrc++;` |
|      1 |  185 | `	}` |
|  67747 |  186 | `	if( zSrc < zEnd && ( zSrc[0] == '-' \|\| zSrc[0] == '+' ) ){` |
|      3 |  187 | `		isNeg = (zSrc[0] == '-') ? TRUE :FALSE;` |
|      3 |  188 | `		zSrc++;` |
|      1 |  189 | `	}` |
|      - |  190 | `	/* Skip leading zero */` |
|  67747 |  191 | `	while(zSrc < zEnd && zSrc[0] == '0' ){` |
|    ! 0 |  192 | `		zSrc++;` |
|    ! 0 |  193 | `	}` |
|  67747 |  194 | `	i = 19;` |
|  67747 |  195 | `	if( (sxu32)(zEnd-zSrc) >= 19 ){` |
|     11 |  196 | `		i = SyMemcmp(zSrc,isNeg ? SXINT64_MIN_STR : SXINT64_MAX_STR,19) <= 0 ? 19 : 18 ;` |
|      5 |  197 | `	}` |
|  67747 |  198 | `	nVal = 0;` |
|  34102 |  199 | `	for(;;){` |
|  68209 |  200 | `		if(zSrc >= zEnd \|\| !i \|\| !SyisDigit(zSrc[0])){ break; } nVal = nVal * 10 + ( zSrc[0] - '0' ) ; --i ; zSrc++;` |
|  68029 |  201 | `		if(zSrc >= zEnd \|\| !i \|\| !SyisDigit(zSrc[0])){ break; } nVal = nVal * 10 + ( zSrc[0] - '0' ) ; --i ; zSrc++;` |
|   9223 |  202 | `		if(zSrc >= zEnd \|\| !i \|\| !SyisDigit(zSrc[0])){ break; } nVal = nVal * 10 + ( zSrc[0] - '0' ) ; --i ; zSrc++;` |
|   1069 |  203 | `		if(zSrc >= zEnd \|\| !i \|\| !SyisDigit(zSrc[0])){ break; } nVal = nVal * 10 + ( zSrc[0] - '0' ) ; --i ; zSrc++;` |
|      1 |  204 | `	}` |
|      - |  205 | `	/* Skip trailing spaces */` |
|  67755 |  206 | `	while(zSrc < zEnd && SyisSpace(zSrc[0])){` |
|      9 |  207 | `		zSrc++;` |
|      1 |  208 | `	}` |
|  67747 |  209 | `	if( zRest ){` |
|    ! 0 |  210 | `		*zRest = (char *)zSrc;` |
|    ! 0 |  211 | `	}` |
|  67747 |  212 | `	if( pOutVal ){` |
|  67747 |  213 | `		if( isNeg == TRUE && nVal != 0 ){` |
|      3 |  214 | `			nVal = -nVal;` |
|      1 |  215 | `		}` |
|  67747 |  216 | `		*(sxi64 *)pOutVal = nVal;` |
|  33871 |  217 | `	}` |
|  67747 |  218 | `	return (zSrc >= zEnd) ? SXRET_OK : SXERR_SYNTAX;` |
|      5 |  219 |  |
|   1240 |  220 | `PH7_PRIVATE sxi32 SyHexToint(sxi32 c)` |
|      1 |  221 |  |
|   1241 |  222 | `	switch(c){` |
|    495 |  223 | `	case '0': return 0;` |
|     61 |  224 | `	case '1': return 1;` |
|     25 |  225 | `	case '2': return 2;` |
|     49 |  226 | `	case '3': return 3;` |
|     53 |  227 | `	case '4': return 4;` |
|     27 |  228 | `	case '5': return 5;` |
|     29 |  229 | `	case '6': return 6;` |
|     11 |  230 | `	case '7': return 7;` |
|     19 |  231 | `	case '8': return 8;` |
|     21 |  232 | `	case '9': return 9;` |
|     65 |  233 | `	case 'A': case 'a': return 10;` |
|     21 |  234 | `	case 'B': case 'b': return 11;` |
|     57 |  235 | `	case 'C': case 'c': return 12;` |
|     23 |  236 | `	case 'D': case 'd': return 13;` |
|     31 |  237 | `	case 'E': case 'e': return 14;` |
|    269 |  238 | `	case 'F': case 'f': return 15;` |
|      - |  239 | `	}` |
|    ! 0 |  240 | `	return -1;` |
|    621 |  241 |  |
|     92 |  242 | `PH7_PRIVATE sxi32 SyHexStrToInt64(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)` |
|      1 |  243 |  |
|      - |  244 | `	const char *zIn,*zEnd;` |
|     93 |  245 | `	int isNeg = FALSE;` |
|     93 |  246 | `	sxi64 nVal = 0;` |
|      - |  247 | `#if defined(UNTRUST)` |
|      - |  248 | `	if( SX_EMPTY_STR(zSrc) ){` |
|      - |  249 | `		if( pOutVal ){` |
|      - |  250 | `			*(sxi32 *)pOutVal = 0;` |
|      - |  251 | `		}` |
|      - |  252 | `		return SXERR_EMPTY;` |
|      - |  253 | `	}` |
|      - |  254 | `#endif` |
|     93 |  255 | `	zEnd = &zSrc[nLen];` |
|     93 |  256 | `	while( zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|    ! 0 |  257 | `		zSrc++;` |
|    ! 0 |  258 | `	}` |
|     93 |  259 | `	if( zSrc < zEnd && ( *zSrc == '-' \|\| *zSrc == '+' ) ){` |
|    ! 0 |  260 | `		isNeg = (zSrc[0] == '-') ? TRUE :FALSE;` |
|    ! 0 |  261 | `		zSrc++;` |
|    ! 0 |  262 | `	}` |
|     93 |  263 | `	if( zSrc < &zEnd[-2] && zSrc[0] == '0' && (zSrc[1] == 'x' \|\| zSrc[1] == 'X') ){` |
|      - |  264 | `		/* Bypass hex prefix */` |
|     71 |  265 | `		zSrc += sizeof(char) * 2;` |
|     35 |  266 | `	}` |
|      - |  267 | `	/* Skip leading zero */` |
|    103 |  268 | `	while(zSrc < zEnd && zSrc[0] == '0' ){` |
|     11 |  269 | `		zSrc++;` |
|      1 |  270 | `	}` |
|     93 |  271 | `	zIn = zSrc;` |
|     84 |  272 | `	for(;;){` |
|    169 |  273 | `		if(zSrc >= zEnd \|\| !SyisHex(zSrc[0]) \|\| (int)(zSrc-zIn) > 15){` |
|     28 |  274 | `			break;` |
|      - |  275 | `		}` |
|    115 |  276 | `		nVal = nVal * 16 + SyHexToint(zSrc[0]);` |
|    115 |  277 | `		zSrc++;` |
|    115 |  278 | `		if(zSrc >= zEnd \|\| !SyisHex(zSrc[0]) \|\| (int)(zSrc-zIn) > 15){` |
|      9 |  279 | `			break;` |
|      - |  280 | `		}` |
|     99 |  281 | `		nVal = nVal * 16 + SyHexToint(zSrc[0]);` |
|     99 |  282 | `		zSrc++;` |
|     99 |  283 | `		if(zSrc >= zEnd \|\| !SyisHex(zSrc[0]) \|\| (int)(zSrc-zIn) > 15){` |
|     12 |  284 | `			break;` |
|      - |  285 | `		}` |
|     77 |  286 | `		nVal = nVal * 16 + SyHexToint(zSrc[0]);` |
|     77 |  287 | `		zSrc++;` |
|     77 |  288 | `		if(zSrc >= zEnd \|\| !SyisHex(zSrc[0]) \|\| (int)(zSrc-zIn) > 15){` |
|    ! 0 |  289 | `			break;` |
|      - |  290 | `		}` |
|     77 |  291 | `		nVal = nVal * 16 + SyHexToint(zSrc[0]);` |
|     77 |  292 | `		zSrc++;` |
|      1 |  293 | `	}` |
|     93 |  294 | `	while( zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|    ! 0 |  295 | `		zSrc++;` |
|    ! 0 |  296 | `	}` |
|     93 |  297 | `	if( zRest ){` |
|    ! 0 |  298 | `		*zRest = zSrc;` |
|    ! 0 |  299 | `	}` |
|     93 |  300 | `	if( pOutVal ){` |
|     93 |  301 | `		if( isNeg == TRUE && nVal != 0 ){` |
|    ! 0 |  302 | `			nVal = -nVal;` |
|    ! 0 |  303 | `		}` |
|     93 |  304 | `		*(sxi64 *)pOutVal = nVal;` |
|     46 |  305 | `	}` |
|     93 |  306 | `	return zSrc >= zEnd ? SXRET_OK : SXERR_SYNTAX;` |
|      1 |  307 |  |
|     66 |  308 | `PH7_PRIVATE sxi32 SyOctalStrToInt64(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)` |
|      1 |  309 |  |
|      - |  310 | `	const char *zIn,*zEnd;` |
|     67 |  311 | `	int isNeg = FALSE;` |
|     67 |  312 | `	sxi64 nVal = 0;` |
|      - |  313 | `	int c;` |
|      - |  314 | `#if defined(UNTRUST)` |
|      - |  315 | `	if( SX_EMPTY_STR(zSrc) ){` |
|      - |  316 | `		if( pOutVal ){` |
|      - |  317 | `			*(sxi32 *)pOutVal = 0;` |
|      - |  318 | `		}` |
|      - |  319 | `		return SXERR_EMPTY;` |
|      - |  320 | `	}` |
|      - |  321 | `#endif` |
|     67 |  322 | `	zEnd = &zSrc[nLen];` |
|     67 |  323 | `	while(zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|    ! 0 |  324 | `		zSrc++;` |
|    ! 0 |  325 | `	}` |
|     67 |  326 | `	if( zSrc < zEnd && ( zSrc[0] == '-' \|\| zSrc[0] == '+' ) ){` |
|    ! 0 |  327 | `		isNeg = (zSrc[0] == '-') ? TRUE :FALSE;` |
|    ! 0 |  328 | `		zSrc++;` |
|    ! 0 |  329 | `	}` |
|      - |  330 | `	/* Skip leading zero */` |
|    127 |  331 | `	while(zSrc < zEnd && zSrc[0] == '0' ){` |
|     61 |  332 | `		zSrc++;` |
|      1 |  333 | `	}` |
|     67 |  334 | `	zIn = zSrc;` |
|     33 |  335 | `	for(;;){` |
|     67 |  336 | `		if(zSrc >= zEnd \|\| !SyisDigit(zSrc[0])){ break; } if( (c=zSrc[0]-'0') > 7 \|\| (int)(zSrc-zIn) > 20){ break;} nVal = nVal * 8 +  c; zSrc++;` |
|     67 |  337 | `		if(zSrc >= zEnd \|\| !SyisDigit(zSrc[0])){ break; } if( (c=zSrc[0]-'0') > 7 \|\| (int)(zSrc-zIn) > 20){ break;} nVal = nVal * 8 +  c; zSrc++;` |
|     59 |  338 | `		if(zSrc >= zEnd \|\| !SyisDigit(zSrc[0])){ break; } if( (c=zSrc[0]-'0') > 7 \|\| (int)(zSrc-zIn) > 20){ break;} nVal = nVal * 8 +  c; zSrc++;` |
|     41 |  339 | `		if(zSrc >= zEnd \|\| !SyisDigit(zSrc[0])){ break; } if( (c=zSrc[0]-'0') > 7 \|\| (int)(zSrc-zIn) > 20){ break;} nVal = nVal * 8 +  c; zSrc++;` |
|    ! 0 |  340 | `	}` |
|      - |  341 | `	/* Skip trailing spaces */` |
|     67 |  342 | `	while(zSrc < zEnd && SyisSpace(zSrc[0])){` |
|    ! 0 |  343 | `		zSrc++;` |
|    ! 0 |  344 | `	}` |
|     67 |  345 | `	if( zRest ){` |
|    ! 0 |  346 | `		*zRest = zSrc;` |
|    ! 0 |  347 | `	}` |
|     67 |  348 | `	if( pOutVal ){` |
|     67 |  349 | `		if( isNeg == TRUE && nVal != 0 ){` |
|    ! 0 |  350 | `			nVal = -nVal;` |
|    ! 0 |  351 | `		}` |
|     67 |  352 | `		*(sxi64 *)pOutVal = nVal;` |
|     33 |  353 | `	}` |
|     67 |  354 | `	return (zSrc >= zEnd) ? SXRET_OK : SXERR_SYNTAX;` |
|      1 |  355 |  |
|    286 |  356 | `PH7_PRIVATE sxi32 SyBinaryStrToInt64(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)` |
|      1 |  357 |  |
|      - |  358 | `	const char *zIn,*zEnd;` |
|    287 |  359 | `	int isNeg = FALSE;` |
|    287 |  360 | `	sxi64 nVal = 0;` |
|      - |  361 | `	int c;` |
|      - |  362 | `#if defined(UNTRUST)` |
|      - |  363 | `	if( SX_EMPTY_STR(zSrc) ){` |
|      - |  364 | `		if( pOutVal ){` |
|      - |  365 | `			*(sxi32 *)pOutVal = 0;` |
|      - |  366 | `		}` |
|      - |  367 | `		return SXERR_EMPTY;` |
|      - |  368 | `	}` |
|      - |  369 | `#endif` |
|    287 |  370 | `	zEnd = &zSrc[nLen];` |
|    287 |  371 | `	while(zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|    ! 0 |  372 | `		zSrc++;` |
|    ! 0 |  373 | `	}` |
|    287 |  374 | `	if( zSrc < zEnd && ( zSrc[0] == '-' \|\| zSrc[0] == '+' ) ){` |
|    ! 0 |  375 | `		isNeg = (zSrc[0] == '-') ? TRUE :FALSE;` |
|    ! 0 |  376 | `		zSrc++;` |
|    ! 0 |  377 | `	}` |
|    287 |  378 | `	if( zSrc < &zEnd[-2] && zSrc[0] == '0' && (zSrc[1] == 'b' \|\| zSrc[1] == 'B') ){` |
|      - |  379 | `		/* Bypass binary prefix */` |
|    277 |  380 | `		zSrc += sizeof(char) * 2;` |
|    138 |  381 | `	}` |
|      - |  382 | `	/* Skip leading zero */` |
|    335 |  383 | `	while(zSrc < zEnd && zSrc[0] == '0' ){` |
|     49 |  384 | `		zSrc++;` |
|      1 |  385 | `	}` |
|    287 |  386 | `	zIn = zSrc;` |
|    304 |  387 | `	for(;;){` |
|    609 |  388 | `		if(zSrc >= zEnd \|\| (zSrc[0] != '1' && zSrc[0] != '0') \|\| (int)(zSrc-zIn) > 62){ break; } c = zSrc[0] - '0'; nVal = (nVal << 1) + c; zSrc++;` |
|    433 |  389 | `		if(zSrc >= zEnd \|\| (zSrc[0] != '1' && zSrc[0] != '0') \|\| (int)(zSrc-zIn) > 62){ break; } c = zSrc[0] - '0'; nVal = (nVal << 1) + c; zSrc++;` |
|    401 |  390 | `		if(zSrc >= zEnd \|\| (zSrc[0] != '1' && zSrc[0] != '0') \|\| (int)(zSrc-zIn) > 62){ break; } c = zSrc[0] - '0'; nVal = (nVal << 1) + c; zSrc++;` |
|    357 |  391 | `		if(zSrc >= zEnd \|\| (zSrc[0] != '1' && zSrc[0] != '0') \|\| (int)(zSrc-zIn) > 62){ break; } c = zSrc[0] - '0'; nVal = (nVal << 1) + c; zSrc++;` |
|      1 |  392 | `	}` |
|      - |  393 | `	/* Skip trailing spaces */` |
|    287 |  394 | `	while(zSrc < zEnd && SyisSpace(zSrc[0])){` |
|    ! 0 |  395 | `		zSrc++;` |
|    ! 0 |  396 | `	}` |
|    287 |  397 | `	if( zRest ){` |
|    ! 0 |  398 | `		*zRest = zSrc;` |
|    ! 0 |  399 | `	}` |
|    287 |  400 | `	if( pOutVal ){` |
|    287 |  401 | `		if( isNeg == TRUE && nVal != 0 ){` |
|    ! 0 |  402 | `			nVal = -nVal;` |
|    ! 0 |  403 | `		}` |
|    287 |  404 | `		*(sxi64 *)pOutVal = nVal;` |
|    143 |  405 | `	}` |
|    287 |  406 | `	return (zSrc >= zEnd) ? SXRET_OK : SXERR_SYNTAX;` |
|      1 |  407 |  |
|    604 |  408 | `PH7_PRIVATE sxi32 SyStrToReal(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)` |
|      5 |  409 |  |
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
|    609 |  424 | `	sxu8 neg = FALSE;` |
|    609 |  425 | `	sxreal Val = 0.0;` |
|      - |  426 | `	const char *zEnd;` |
|      - |  427 | `	sxi32 Lim,exp;` |
|    609 |  428 | `	sxreal *p = 0;` |
|      - |  429 | `#ifdef UNTRUST` |
|      - |  430 | `	if( SX_EMPTY_STR(zSrc)  ){` |
|      - |  431 | `		if( pOutVal ){` |
|      - |  432 | `			*(sxreal *)pOutVal = 0.0;` |
|      - |  433 | `		}` |
|      - |  434 | `		return SXERR_EMPTY;` |
|      - |  435 | `	}` |
|      - |  436 | `#endif` |
|      - |  437 | `	/* Define local limits and end pointer used by the parsing loops */` |
|    609 |  438 | `	zEnd = &zSrc[nLen];` |
|    609 |  439 | `	Lim = SXDBL_DIG;` |
|      - |  440 | `	/* Skip leading spaces */` |
|    613 |  441 | `	while( zSrc < zEnd && SyisSpace(zSrc[0]) ) zSrc++;` |
|      - |  442 |  |
|      - |  443 | `	/* Sign (if exists) */` |
|    609 |  444 | `	if( zSrc < zEnd && (zSrc[0] == '-' \|\| zSrc[0] == '+' ) ){` |
|      7 |  445 | `		neg =  zSrc[0] == '-' ? TRUE : FALSE ;` |
|      7 |  446 | `		zSrc++;` |
|      3 |  447 | `	}` |
|      - |  448 |  |
|      - |  449 | `	/* Integer part */` |
|    318 |  450 | `	for(;;){` |
|    641 |  451 | `		if(zSrc >= zEnd \|\| !Lim \|\| !SyisDigit(zSrc[0])){` |
|     15 |  452 | `			break;` |
|      - |  453 | `		}` |
|    613 |  454 | `		Val = Val * 10.0 + (zSrc[0] - '0');` |
|    613 |  455 | `		zSrc++;` |
|    613 |  456 | `		--Lim;` |
|    613 |  457 | `		if(zSrc >= zEnd \|\| !Lim \|\| !SyisDigit(zSrc[0])){` |
|    285 |  458 | `			break;` |
|      - |  459 | `		}` |
|     49 |  460 | `		Val = Val * 10.0 + (zSrc[0] - '0');` |
|     49 |  461 | `		zSrc++;` |
|     49 |  462 | `		--Lim;` |
|     49 |  463 | `		if(zSrc >= zEnd \|\| !Lim \|\| !SyisDigit(zSrc[0])){` |
|      7 |  464 | `			break;` |
|      - |  465 | `		}` |
|     37 |  466 | `		Val = Val * 10.0 + (zSrc[0] - '0');` |
|     37 |  467 | `		zSrc++;` |
|     37 |  468 | `		--Lim;` |
|     37 |  469 | `		if(zSrc >= zEnd \|\| !Lim \|\| !SyisDigit(zSrc[0])){` |
|      3 |  470 | `			break;` |
|      - |  471 | `		}` |
|     33 |  472 | `		Val = Val * 10.0 + (zSrc[0] - '0');` |
|     33 |  473 | `		zSrc++;` |
|     33 |  474 | `		--Lim;` |
|      1 |  475 | `	}` |
|    609 |  476 | `	Lim = SXDBL_DIG ;` |
|    302 |  477 | `	for(;;){` |
|    609 |  478 | `		if(zSrc >= zEnd \|\| !Lim \|\| !SyisDigit(zSrc[0])){` |
|    307 |  479 | `			break;` |
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
|    609 |  503 | `	if( zSrc < zEnd && ( zSrc[0] == '.' \|\| zSrc[0] == ',' ) ){` |
|    573 |  504 | `		sxreal dec = 1.0;` |
|    573 |  505 | `		zSrc++;` |
|    374 |  506 | `		for(;;){` |
|    753 |  507 | `			if(zSrc >= zEnd \|\| !Lim \|\| !SyisDigit(zSrc[0])){` |
|     25 |  508 | `				break;` |
|      - |  509 | `			}` |
|    705 |  510 | `			Val = Val * 10.0 + (zSrc[0] - '0');` |
|    705 |  511 | `			dec *= 10.0;` |
|    705 |  512 | `			zSrc++;` |
|    705 |  513 | `			--Lim;` |
|    705 |  514 | `			if(zSrc >= zEnd \|\| !Lim \|\| !SyisDigit(zSrc[0])){` |
|    181 |  515 | `				break;` |
|      - |  516 | `			}` |
|    349 |  517 | `			Val = Val * 10.0 + (zSrc[0] - '0');` |
|    349 |  518 | `			dec *= 10.0;` |
|    349 |  519 | `			zSrc++;` |
|    349 |  520 | `			--Lim;` |
|    349 |  521 | `			if(zSrc >= zEnd \|\| !Lim \|\| !SyisDigit(zSrc[0])){` |
|     47 |  522 | `				break;` |
|      - |  523 | `			}` |
|    259 |  524 | `			Val = Val * 10.0 + (zSrc[0] - '0');` |
|    259 |  525 | `			dec *= 10.0;` |
|    259 |  526 | `			zSrc++;` |
|    259 |  527 | `			--Lim;` |
|    259 |  528 | `			if(zSrc >= zEnd \|\| !Lim \|\| !SyisDigit(zSrc[0])){` |
|     40 |  529 | `				break;` |
|      - |  530 | `			}` |
|    181 |  531 | `			Val = Val * 10.0 + (zSrc[0] - '0');` |
|    181 |  532 | `			dec *= 10.0;` |
|    181 |  533 | `			zSrc++;` |
|    181 |  534 | `			--Lim;` |
|      1 |  535 | `		}` |
|    573 |  536 | `		Val /= dec;` |
|    284 |  537 | `	}` |
|    609 |  538 | `	if( neg == TRUE && Val != 0.0 ) {` |
|      7 |  539 | `		Val = -Val ;` |
|      3 |  540 | `	}` |
|    609 |  541 | `	if( Lim <= 0 ){` |
|      - |  542 | `		/* jump overflow digit */` |
|     69 |  543 | `		while( zSrc < zEnd ){` |
|     41 |  544 | `			if( zSrc[0] == 'e' \|\| zSrc[0] == 'E' ){` |
|    ! 0 |  545 | `				break;` |
|      - |  546 | `			}` |
|     41 |  547 | `			zSrc++;` |
|      1 |  548 | `		}` |
|     14 |  549 | `	}` |
|    609 |  550 | `	neg = FALSE;` |
|    609 |  551 | `	if( zSrc < zEnd && ( zSrc[0] == 'e' \|\| zSrc[0] == 'E' ) ){` |
|     61 |  552 | `		zSrc++;` |
|     61 |  553 | `		if( zSrc < zEnd && ( zSrc[0] == '-' \|\| zSrc[0] == '+') ){` |
|     21 |  554 | `			neg = zSrc[0] == '-' ? TRUE : FALSE ;` |
|     21 |  555 | `			zSrc++;` |
|     10 |  556 | `		}` |
|     61 |  557 | `		exp = 0;` |
|    143 |  558 | `		while( zSrc < zEnd && SyisDigit(zSrc[0]) && exp < SXDBL_MAX_EXP ){` |
|     83 |  559 | `			exp = exp * 10 + (zSrc[0] - '0');` |
|     83 |  560 | `			zSrc++;` |
|      1 |  561 | `		}` |
|     61 |  562 | `		if( neg  ){` |
|     15 |  563 | `			if( exp > SXDBL_MIN_EXP_PLUS ) exp = SXDBL_MIN_EXP_PLUS ;` |
|     54 |  564 | `		}else if ( exp > SXDBL_MAX_EXP ){` |
|    ! 0 |  565 | `			exp = SXDBL_MAX_EXP;` |
|    ! 0 |  566 | `		}` |
|    239 |  567 | `		for( p = (sxreal *)aTab ; exp ; exp >>= 1 , p++ ){` |
|    179 |  568 | `			if( exp & 01 ){` |
|    103 |  569 | `				if( neg ){` |
|     23 |  570 | `					Val /= *p ;` |
|     12 |  571 | `				}else{` |
|     81 |  572 | `					Val *= *p;` |
|      - |  573 | `				}` |
|     51 |  574 | `			}` |
|     90 |  575 | `		}` |
|     30 |  576 | `	}` |
|    609 |  577 | `	while( zSrc < zEnd && SyisSpace(zSrc[0]) ){` |
|    ! 0 |  578 | `		zSrc++;` |
|    ! 0 |  579 | `	}` |
|    609 |  580 | `	if( zRest ){` |
|    ! 0 |  581 | `		*zRest = zSrc;` |
|    ! 0 |  582 | `	}` |
|    609 |  583 | `	if( pOutVal ){` |
|    609 |  584 | `		*(sxreal *)pOutVal = Val;` |
|    302 |  585 | `	}` |
|    609 |  586 | `	return zSrc >= zEnd ? SXRET_OK : SXERR_SYNTAX;` |
|      5 |  587 |  |
|      - |  588 |  |
