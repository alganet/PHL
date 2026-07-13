# src/ph7/lex.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 758/813 lines (93.23%)

[Root index](../../index.md) | [Directory index](index.md)

|     Hits | Line | Source |
| -------: | ---: | :--- |
|        - |    1 | `/**` |
|        - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|        - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|        - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|        - |    5 | ` */` |
|        - |    6 | `#include "ph7int.h"` |
|        - |    7 | `/*` |
|        - |    8 | ` * This file implement an efficient hand-coded,thread-safe and full-reentrant` |
|        - |    9 | ` * lexical analyzer/Tokenizer for the PH7 engine.` |
|        - |   10 | ` */` |
|        - |   11 | `/* Forward declaration */` |
|        - |   12 | `static sxu32 KeywordCode(const char *z, int n);` |
|        - |   13 | `static sxi32 LexExtractHeredoc(SyStream *pStream,SyToken *pToken);` |
|        - |   14 | `/*` |
|        - |   15 | ` * Tokenize a raw PHP input.` |
|        - |   16 | ` * Get a single low-level token from the input file. Update the stream pointer so that` |
|        - |   17 | ` * it points to the first character beyond the extracted token.` |
|        - |   18 | ` */` |
| 12754384 |   19 | `static sxi32 TokenizePHP(SyStream *pStream,SyToken *pToken,void *pUserData,void *pCtxData)` |
|        5 |   20 | `{` |
|        - |   21 | `	SyString *pStr;` |
|        - |   22 | `	sxi32 rc;` |
|        - |   23 | `	/* Ignore leading white spaces */` |
| 19227159 |   24 | `	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){` |
|        - |   25 | `		/* Advance the stream cursor */` |
|  6472775 |   26 | `		if( pStream->zText[0] == '\n' ){` |
|        - |   27 | `			/* Update line counter */` |
|    44215 |   28 | `			pStream->nLine++;` |
|    22105 |   29 | `		}` |
|  6472775 |   30 | `		pStream->zText++;` |
|        5 |   31 | `	}` |
| 12754389 |   32 | `	if( pStream->zText >= pStream->zEnd ){` |
|        - |   33 | `		/* End of input reached */` |
|      ! 0 |   34 | `		return SXERR_EOF;` |
|        - |   35 | `	}` |
|        - |   36 | `	/* Record token starting position and line */` |
| 12754389 |   37 | `	pToken->nLine = pStream->nLine;` |
| 12754389 |   38 | `	pToken->pUserData = 0;` |
| 12754389 |   39 | `	pStr = &pToken->sData;` |
| 12754389 |   40 | `	SyStringInitFromBuf(pStr,pStream->zText,0);` |
| 15176920 |   41 | `	if( pStream->zText[0] >= 0xc0 \|\| SyisAlpha(pStream->zText[0]) \|\| pStream->zText[0] == '_' ){` |
|        - |   42 | `		/* The following code fragment is taken verbatim from the xPP source tree.` |
|        - |   43 | `		 * xPP is a modern embeddable macro processor with advanced features useful for` |
|        - |   44 | `		 * application seeking for a production quality,ready to use macro processor.` |
|        - |   45 | `		 * xPP is a widely used library developed and maintened by Symisc Systems.` |
|        - |   46 | `		 * You can reach the xPP home page by following this link:` |
|        - |   47 | `		 * http://xpp.symisc.net/` |
|        - |   48 | `		 */` |
|        - |   49 | `		const unsigned char *zIn;` |
|        - |   50 | `		sxu32 nKeyword;` |
|        - |   51 | `		/* Isolate UTF-8 or alphanumeric stream */` |
|  4845067 |   52 | `		if( pStream->zText[0] < 0xc0 ){` |
|  4845051 |   53 | `			pStream->zText++;` |
|  2422523 |   54 | `		}` |
|  4759104 |   55 | `		for(;;){` |
|  9518213 |   56 | `			zIn = pStream->zText;` |
|  9518213 |   57 | `			if( zIn[0] >= 0xc0 ){` |
|       49 |   58 | `				zIn++;` |
|        - |   59 | `				/* UTF-8 stream */` |
|      109 |   60 | `				while( zIn < pStream->zEnd && ((zIn[0] & 0xc0) == 0x80) ){` |
|       61 |   61 | `					zIn++;` |
|        1 |   62 | `				}` |
|       24 |   63 | `			}` |
|        - |   64 | `			/* Skip alphanumeric stream */` |
| 40923151 |   65 | `			while( zIn < pStream->zEnd && zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_') ){` |
| 26645839 |   66 | `				zIn++;` |
|        5 |   67 | `			}` |
|  9518213 |   68 | `			if( zIn == pStream->zText ){` |
|        - |   69 | `				/* Not an UTF-8 or alphanumeric stream */` |
|  4845067 |   70 | `				break;` |
|        - |   71 | `			}` |
|        - |   72 | `			/* Synchronize pointers */` |
|  4673151 |   73 | `			pStream->zText = zIn;` |
|        5 |   74 | `		}` |
|        - |   75 | `		/* Record token length */` |
|  4845067 |   76 | `		pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  4845067 |   77 | `		nKeyword = KeywordCode(pStr->zString,(int)pStr->nByte);` |
|        - |   78 | `		/* PHP 7.4: 'fn' is a keyword reserved for arrow functions.` |
|        - |   79 | `		 * The auto-generated perfect hash above doesn't know about it,` |
|        - |   80 | `		 * so intercept the 'fn' identifier here.` |
|        - |   81 | `		 */` |
|  4845062 |   82 | `		if( nKeyword == PH7_TK_ID && pStr->nByte == 2` |
|  1520706 |   83 | `			&& pStr->zString[0] == 'f' && pStr->zString[1] == 'n' ){` |
|      539 |   84 | `			nKeyword = PH7_TKWRD_FN;` |
|      267 |   85 | `		}` |
|  4845067 |   86 | `		if( nKeyword != PH7_TK_ID ){` |
|  1814669 |   87 | `			if( nKeyword &` |
|        - |   88 | `				(PH7_TKWRD_NEW\|PH7_TKWRD_CLONE\|PH7_TKWRD_AND\|PH7_TKWRD_XOR\|PH7_TKWRD_OR\|PH7_TKWRD_INSTANCEOF) ){` |
|        - |   89 | `					/* Alpha stream operators [i.e: new,clone,and,instanceof,or,xor],save the operator instance for later processing */` |
|    48307 |   90 | `					pToken->pUserData = (void *)PH7_ExprExtractOperator(pStr,0);` |
|        - |   91 | `					/* Mark as an operator */` |
|    48307 |   92 | `					pToken->nType = PH7_TK_ID\|PH7_TK_OP;` |
|    24156 |   93 | `			}else{` |
|        - |   94 | `				/* We are dealing with a keyword [i.e: while,foreach,class...],save the keyword ID */` |
|  1766367 |   95 | `				pToken->nType = PH7_TK_KEYWORD;` |
|  1766367 |   96 | `				pToken->pUserData = SX_INT_TO_PTR(nKeyword);` |
|        - |   97 | `			}` |
|   907337 |   98 | `		}else{` |
|        - |   99 | `			/* A simple identifier */` |
|  3030403 |  100 | `			pToken->nType = PH7_TK_ID;` |
|        - |  101 | `		}` |
|  2422536 |  102 | `	}else{` |
|        - |  103 | `		sxi32 c;` |
|        - |  104 | `		/* Non-alpha stream */` |
|  7909327 |  105 | `		if( pStream->zText[0] == '#' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '[' ){` |
|       73 |  106 | `			sxu32 nDepth = 1;` |
|        - |  107 | `			/* PHP 8 attribute group '#[ ... ]': skip the whole balanced group as` |
|        - |  108 | `			 * trivia (attributes are not stored yet). Brackets inside string` |
|        - |  109 | `			 * literals and comments must not affect the depth count. An` |
|        - |  110 | `			 * unterminated group is silently consumed up to EOF, consistent` |
|        - |  111 | `			 * with unterminated block comments below.` |
|        - |  112 | `			 */` |
|       73 |  113 | `			pStream->zText += 2;` |
|     1371 |  114 | `			while( pStream->zText < pStream->zEnd && nDepth > 0 ){` |
|     1303 |  115 | `				sxi32 d = pStream->zText[0];` |
|     1303 |  116 | `				if( d == '[' ){` |
|       11 |  117 | `					nDepth++;` |
|     1298 |  118 | `				}else if( d == ']' ){` |
|       83 |  119 | `					nDepth--;` |
|     1254 |  120 | `				}else if( d == '\'' \|\| d == '"' ){` |
|        - |  121 | `					/* String literal: scan for the matching unescaped quote */` |
|       13 |  122 | `					pStream->zText++;` |
|       95 |  123 | `					while( pStream->zText < pStream->zEnd ){` |
|       95 |  124 | `						if( pStream->zText[0] == '\\' && &pStream->zText[1] < pStream->zEnd ){` |
|        3 |  125 | `							if( pStream->zText[1] == '\n' ){` |
|      ! 0 |  126 | `								pStream->nLine++;` |
|      ! 0 |  127 | `							}` |
|        3 |  128 | `							pStream->zText += 2;` |
|        3 |  129 | `							continue;` |
|        - |  130 | `						}` |
|       93 |  131 | `						if( pStream->zText[0] == d ){` |
|       13 |  132 | `							break;` |
|        - |  133 | `						}` |
|       81 |  134 | `						if( pStream->zText[0] == '\n' ){` |
|      ! 0 |  135 | `							pStream->nLine++;` |
|      ! 0 |  136 | `						}` |
|       81 |  137 | `						pStream->zText++;` |
|        1 |  138 | `					}` |
|       13 |  139 | `					if( pStream->zText >= pStream->zEnd ){` |
|      ! 0 |  140 | `						break; /* Unterminated string literal */` |
|        1 |  141 | `					}` |
|        - |  142 | `					/* Fall through: consume the closing quote below */` |
|     1209 |  143 | `				}else if( d == '#' \|\| (d == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '/') ){` |
|        - |  144 | `					/* Inline comment inside the group */` |
|      ! 0 |  145 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] != '\n' ){` |
|      ! 0 |  146 | `						pStream->zText++;` |
|      ! 0 |  147 | `					}` |
|      ! 0 |  148 | `					continue; /* Let the outer loop count the newline */` |
|     1203 |  149 | `				}else if( d == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '*' ){` |
|        - |  150 | `					/* Block comment inside the group */` |
|      ! 0 |  151 | `					pStream->zText += 2;` |
|      ! 0 |  152 | `					while( pStream->zText < pStream->zEnd ){` |
|      ! 0 |  153 | `						if( pStream->zText[0] == '*' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '/' ){` |
|      ! 0 |  154 | `							pStream->zText += 2;` |
|      ! 0 |  155 | `							break;` |
|        - |  156 | `						}` |
|      ! 0 |  157 | `						if( pStream->zText[0] == '\n' ){` |
|      ! 0 |  158 | `							pStream->nLine++;` |
|      ! 0 |  159 | `						}` |
|      ! 0 |  160 | `						pStream->zText++;` |
|      ! 0 |  161 | `					}` |
|      ! 0 |  162 | `					continue;` |
|     1203 |  163 | `				}else if( d == '\n' ){` |
|        7 |  164 | `					pStream->nLine++;` |
|        3 |  165 | `				}` |
|     1303 |  166 | `				pStream->zText++;` |
|        5 |  167 | `			}` |
|        - |  168 | `			/* Tell the upper-layer to ignore this token */` |
|       73 |  169 | `			return SXERR_CONTINUE;` |
|  7958373 |  170 | `		}else if( pStream->zText[0] == '#' \|\|` |
|  7909248 |  171 | `			( pStream->zText[0] == '/' &&  &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '/') ){` |
|     5665 |  172 | `				pStream->zText++;` |
|        - |  173 | `				/* Inline comments */` |
|   226801 |  174 | `				while( pStream->zText < pStream->zEnd && pStream->zText[0] != '\n' ){` |
|   221141 |  175 | `					pStream->zText++;` |
|        5 |  176 | `				}` |
|        - |  177 | `				/* Tell the upper-layer to ignore this token */` |
|     5665 |  178 | `				return SXERR_CONTINUE;` |
|  7903599 |  179 | `		}else if( pStream->zText[0] == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '*' ){` |
|    92483 |  180 | `			pStream->zText += 2;` |
|        - |  181 | `			/* Block comment */` |
|  2859779 |  182 | `			while( pStream->zText < pStream->zEnd ){` |
|  2859779 |  183 | `				if( pStream->zText[0] == '*' ){` |
|    92561 |  184 | `					if( &pStream->zText[1] >= pStream->zEnd \|\| pStream->zText[1] == '/'  ){` |
|    46244 |  185 | `						break;` |
|        - |  186 | `					}` |
|       39 |  187 | `				}` |
|  2767301 |  188 | `				if( pStream->zText[0] == '\n' ){` |
|       77 |  189 | `					pStream->nLine++;` |
|       36 |  190 | `				}` |
|  2767301 |  191 | `				pStream->zText++;` |
|        5 |  192 | `			}` |
|    92483 |  193 | `			pStream->zText += 2;` |
|        - |  194 | `			/* Tell the upper-layer to ignore this token */` |
|    92483 |  195 | `			return SXERR_CONTINUE;` |
|  7811121 |  196 | `		}else if( SyisDigit(pStream->zText[0]) ){` |
|   136589 |  197 | `			pStream->zText++;` |
|        - |  198 | `			/* PHP 7.4: handle underscore separator immediately following the first digit.` |
|        - |  199 | `			 * Check pStream->zText < pStream->zEnd BEFORE forming pStream->zText + 1 so` |
|        - |  200 | `			 * we never compute a pointer past one-past-end. */` |
|   136584 |  201 | `			if( pStream->zText < pStream->zEnd` |
|   136584 |  202 | `				&& pStream->zText[0] == '_'` |
|    68372 |  203 | `				&& pStream->zText + 1 < pStream->zEnd` |
|      160 |  204 | `				&& pStream->zText[1] < 0xc0` |
|      165 |  205 | `				&& SyisDigit(pStream->zText[1]) ){` |
|      151 |  206 | `				pStream->zText++; /* swallow underscore between two digits */` |
|       75 |  207 | `			}` |
|        - |  208 | `			/* Decimal digit stream (PHP 7.4: underscore separator allowed between two digits) */` |
|   149555 |  209 | `			while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|    12971 |  210 | `				pStream->zText++;` |
|    12966 |  211 | `				if( pStream->zText < pStream->zEnd` |
|    12966 |  212 | `					&& pStream->zText[0] == '_'` |
|     6569 |  213 | `					&& pStream->zText + 1 < pStream->zEnd` |
|      172 |  214 | `					&& pStream->zText[1] < 0xc0` |
|      177 |  215 | `					&& SyisDigit(pStream->zText[1]) ){` |
|      173 |  216 | `					pStream->zText++; /* swallow underscore between two digits */` |
|       86 |  217 | `				}` |
|        5 |  218 | `			}` |
|        - |  219 | `			/* Mark the token as integer until we encounter a real number */` |
|   136589 |  220 | `			pToken->nType = PH7_TK_INTEGER;` |
|   136589 |  221 | `			if( pStream->zText < pStream->zEnd ){` |
|   136589 |  222 | `				c = pStream->zText[0];` |
|   136589 |  223 | `				if( c == '.' ){` |
|        - |  224 | `					/* Real number (PHP 7.4: underscore separator allowed between two digits) */` |
|      811 |  225 | `					pStream->zText++;` |
|     2999 |  226 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|     2193 |  227 | `						pStream->zText++;` |
|     2188 |  228 | `						if( pStream->zText < pStream->zEnd` |
|     2188 |  229 | `							&& pStream->zText[0] == '_'` |
|     1100 |  230 | `							&& pStream->zText + 1 < pStream->zEnd` |
|       12 |  231 | `							&& pStream->zText[1] < 0xc0` |
|       17 |  232 | `							&& SyisDigit(pStream->zText[1]) ){` |
|       13 |  233 | `							pStream->zText++;` |
|        6 |  234 | `						}` |
|        5 |  235 | `					}` |
|      811 |  236 | `					if( pStream->zText < pStream->zEnd ){` |
|      811 |  237 | `						c = pStream->zText[0];` |
|      811 |  238 | `						if( c=='e' \|\| c=='E' ){` |
|       55 |  239 | `							pStream->zText++;` |
|       55 |  240 | `							if( pStream->zText < pStream->zEnd ){` |
|       55 |  241 | `								c = pStream->zText[0];` |
|       54 |  242 | `								if( (c =='+' \|\| c=='-') && &pStream->zText[1] < pStream->zEnd  &&` |
|       27 |  243 | `									pStream->zText[1] < 0xc0 && SyisDigit(pStream->zText[1]) ){` |
|       27 |  244 | `										pStream->zText++;` |
|       13 |  245 | `								}` |
|      159 |  246 | `								while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|      105 |  247 | `									pStream->zText++;` |
|      104 |  248 | `									if( pStream->zText < pStream->zEnd` |
|      104 |  249 | `										&& pStream->zText[0] == '_'` |
|       56 |  250 | `										&& pStream->zText + 1 < pStream->zEnd` |
|        8 |  251 | `										&& pStream->zText[1] < 0xc0` |
|        9 |  252 | `										&& SyisDigit(pStream->zText[1]) ){` |
|        9 |  253 | `										pStream->zText++;` |
|        4 |  254 | `									}` |
|        1 |  255 | `								}` |
|       27 |  256 | `							}` |
|       27 |  257 | `						}` |
|      403 |  258 | `					}` |
|      811 |  259 | `					pToken->nType = PH7_TK_REAL;` |
|   136186 |  260 | `				}else if( c=='e' \|\| c=='E' ){` |
|       51 |  261 | `					SXUNUSED(pUserData); /* Prevent compiler warning */` |
|       51 |  262 | `					SXUNUSED(pCtxData);` |
|      103 |  263 | `					pStream->zText++;` |
|      103 |  264 | `					if( pStream->zText < pStream->zEnd ){` |
|      103 |  265 | `						c = pStream->zText[0];` |
|      102 |  266 | `						if( (c =='+' \|\| c=='-') && &pStream->zText[1] < pStream->zEnd  &&` |
|       31 |  267 | `							pStream->zText[1] < 0xc0 && SyisDigit(pStream->zText[1]) ){` |
|       31 |  268 | `								pStream->zText++;` |
|       15 |  269 | `						}` |
|      319 |  270 | `						while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|      217 |  271 | `							pStream->zText++;` |
|      216 |  272 | `							if( pStream->zText < pStream->zEnd` |
|      216 |  273 | `								&& pStream->zText[0] == '_'` |
|      110 |  274 | `								&& pStream->zText + 1 < pStream->zEnd` |
|        4 |  275 | `								&& pStream->zText[1] < 0xc0` |
|        5 |  276 | `								&& SyisDigit(pStream->zText[1]) ){` |
|        5 |  277 | `								pStream->zText++;` |
|        2 |  278 | `							}` |
|        1 |  279 | `						}` |
|       51 |  280 | `					}` |
|      103 |  281 | `					pToken->nType = PH7_TK_REAL;` |
|   135732 |  282 | `				}else if( c == 'x' \|\| c == 'X' ){` |
|        - |  283 | `					/* Hex digit stream (PHP 7.4: underscore separator allowed between two digits) */` |
|       75 |  284 | `					pStream->zText++;` |
|      371 |  285 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisHex(pStream->zText[0]) ){` |
|      297 |  286 | `						pStream->zText++;` |
|      296 |  287 | `						if( pStream->zText < pStream->zEnd` |
|      296 |  288 | `							&& pStream->zText[0] == '_'` |
|      172 |  289 | `							&& pStream->zText + 1 < pStream->zEnd` |
|       48 |  290 | `							&& pStream->zText[1] < 0xc0` |
|       49 |  291 | `							&& SyisHex(pStream->zText[1]) ){` |
|       49 |  292 | `							pStream->zText++;` |
|       24 |  293 | `						}` |
|        1 |  294 | `					}` |
|   135643 |  295 | `				}else if(c  == 'b' \|\| c == 'B' ){` |
|        - |  296 | `					/* Binary digit stream (PHP 7.4: underscore separator allowed between two digits) */` |
|      280 |  297 | `					pStream->zText++;` |
|     2702 |  298 | `					while( pStream->zText < pStream->zEnd && (pStream->zText[0] == '0' \|\| pStream->zText[0] == '1') ){` |
|     1523 |  299 | `						pStream->zText++;` |
|     1522 |  300 | `						if( pStream->zText < pStream->zEnd` |
|     1522 |  301 | `							&& pStream->zText[0] == '_'` |
|      830 |  302 | `							&& pStream->zText + 1 < pStream->zEnd` |
|      139 |  303 | `							&& (pStream->zText[1] == '0' \|\| pStream->zText[1] == '1') ){` |
|      139 |  304 | `							pStream->zText++;` |
|       69 |  305 | `						}` |
|        1 |  306 | `					}` |
|      139 |  307 | `				}` |
|    68292 |  308 | `			}` |
|        - |  309 | `			/* PHP 7.4: absorb a trailing malformed underscore run into the` |
|        - |  310 | `			 * numeric token so the compile phase can emit a PHP-compatible` |
|        - |  311 | `			 * "syntax error, unexpected identifier" parse error. Valid` |
|        - |  312 | `			 * separators were already consumed by the per-loop peek logic` |
|        - |  313 | `			 * above, so an underscore here is always misplaced. */` |
|   136589 |  314 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '_' ){` |
|       18 |  315 | `				pStream->zText++;` |
|       44 |  316 | `				while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0` |
|       49 |  317 | `					&& (SyisAlphaNum(pStream->zText[0]) \|\| pStream->zText[0] == '_') ){` |
|       20 |  318 | `					pStream->zText++;` |
|        4 |  319 | `				}` |
|        7 |  320 | `			}` |
|        - |  321 | `			/* Record token length */` |
|   136589 |  322 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|   136589 |  323 | `			return SXRET_OK;` |
|        - |  324 | `		}` |
|  7674537 |  325 | `		c = pStream->zText[0];` |
|  7674537 |  326 | `		pStream->zText++; /* Advance the stream cursor */` |
|        - |  327 | `		/* Assume we are dealing with an operator*/` |
|  7674537 |  328 | `		pToken->nType = PH7_TK_OP;` |
|  7674537 |  329 | `		switch(c){` |
|  1541317 |  330 | `		case '$': pToken->nType = PH7_TK_DOLLAR; break;` |
|   667105 |  331 | `		case '{': pToken->nType = PH7_TK_OCB;    break;` |
|   667091 |  332 | `		case '}': pToken->nType = PH7_TK_CCB;    break;` |
|  1180189 |  333 | `		case '(': pToken->nType = PH7_TK_LPAREN; break;` |
|   100287 |  334 | `		case '[': pToken->nType \|= PH7_TK_OSB;   break; /* Bitwise operation here,since the square bracket token '['` |
|        - |  335 | `														 * is a potential operator [i.e: subscripting] */` |
|   100293 |  336 | `		case ']': pToken->nType = PH7_TK_CSB;    break;` |
|   590084 |  337 | `		case ')': {` |
|  1180173 |  338 | `			SySet *pTokSet = pStream->pSet;` |
|        - |  339 | `			/* Assemble type cast operators [i.e: (int),(float),(bool)...] */` |
|  1180173 |  340 | `			if( pTokSet->nUsed >= 2 ){` |
|        - |  341 | `				SyToken *pTmp;` |
|        - |  342 | `				/* Peek the last recongnized token */` |
|  1180171 |  343 | `				pTmp = (SyToken *)SySetPeek(pTokSet);` |
|  1180171 |  344 | `				if( pTmp->nType & PH7_TK_KEYWORD ){` |
|     4505 |  345 | `					sxi32 nID = SX_PTR_TO_INT(pTmp->pUserData);` |
|     4505 |  346 | `					if( (sxu32)nID & (PH7_TKWRD_ARRAY\|PH7_TKWRD_INT\|PH7_TKWRD_FLOAT\|PH7_TKWRD_STRING\|PH7_TKWRD_OBJECT\|PH7_TKWRD_BOOL\|PH7_TKWRD_UNSET) ){` |
|     4241 |  347 | `						pTmp = (SyToken *)SySetAt(pTokSet,pTokSet->nUsed - 2);` |
|     4241 |  348 | `						if( pTmp->nType & PH7_TK_LPAREN ){` |
|        - |  349 | `							/* Merge the three tokens '(' 'TYPE' ')' into a single one */` |
|     4157 |  350 | `							const char * zTypeCast = "(int)";` |
|     4157 |  351 | `							if( nID & PH7_TKWRD_FLOAT ){` |
|       23 |  352 | `								zTypeCast = "(float)";` |
|     4146 |  353 | `							}else if( nID & PH7_TKWRD_BOOL ){` |
|        9 |  354 | `								zTypeCast = "(bool)";` |
|     4131 |  355 | `							}else if( nID & PH7_TKWRD_STRING ){` |
|     3879 |  356 | `								zTypeCast = "(string)";` |
|     2190 |  357 | `							}else if( nID & PH7_TKWRD_ARRAY ){` |
|       27 |  358 | `								zTypeCast = "(array)";` |
|      240 |  359 | `							}else if( nID & PH7_TKWRD_OBJECT ){` |
|       35 |  360 | `								zTypeCast = "(object)";` |
|      210 |  361 | `							}else if( nID & PH7_TKWRD_UNSET ){` |
|        3 |  362 | `								zTypeCast = "(unset)";` |
|        1 |  363 | `							}` |
|        - |  364 | `							/* Reflect the change */` |
|     4157 |  365 | `							pToken->nType = PH7_TK_OP;` |
|     4157 |  366 | `							SyStringInitFromBuf(&pToken->sData,zTypeCast,SyStrlen(zTypeCast));` |
|        - |  367 | `							/* Save the instance associated with the type cast operator */` |
|     4157 |  368 | `							pToken->pUserData = (void *)PH7_ExprExtractOperator(&pToken->sData,0);` |
|        - |  369 | `							/* Remove the two previous tokens */` |
|     4157 |  370 | `							pTokSet->nUsed -= 2;` |
|     4157 |  371 | `							return SXRET_OK;` |
|        - |  372 | `						}` |
|       42 |  373 | `					}` |
|      174 |  374 | `				}` |
|   588007 |  375 | `			}` |
|  1176021 |  376 | `			pToken->nType = PH7_TK_RPAREN;` |
|  1176021 |  377 | `			break;` |
|        - |  378 | `				  }` |
|    86223 |  379 | `		case '\'':{` |
|        - |  380 | `			/* Single quoted string */` |
|   172451 |  381 | `			pStr->zString++;` |
|  2679261 |  382 | `			while( pStream->zText < pStream->zEnd ){` |
|  2679261 |  383 | `				if( pStream->zText[0] == '\''  ){` |
|   172461 |  384 | `					if( pStream->zText[-1] != '\\' ){` |
|   172437 |  385 | `						break;` |
|      ! 0 |  386 | `					}else{` |
|       25 |  387 | `						const unsigned char *zPtr = &pStream->zText[-2];` |
|       25 |  388 | `						sxi32 i = 1;` |
|       43 |  389 | `						while( zPtr > pStream->zInput && zPtr[0] == '\\' ){` |
|       19 |  390 | `							zPtr--;` |
|       19 |  391 | `							i++;` |
|        1 |  392 | `						}` |
|       25 |  393 | `						if((i&1)==0){` |
|       15 |  394 | `							break;` |
|        - |  395 | `						}` |
|        - |  396 | `					}` |
|        5 |  397 | `				}` |
|  2506815 |  398 | `				if( pStream->zText[0] == '\n' ){` |
|       67 |  399 | `					pStream->nLine++;` |
|       33 |  400 | `				}` |
|  2506815 |  401 | `				pStream->zText++;` |
|        5 |  402 | `			}` |
|        - |  403 | `			/* Record token length and type */` |
|   172451 |  404 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|   172451 |  405 | `			pToken->nType = PH7_TK_SSTR;` |
|        - |  406 | `			/* Jump the trailing single quote */` |
|   172451 |  407 | `			pStream->zText++;` |
|   172451 |  408 | `			return SXRET_OK;` |
|        - |  409 | `				  }` |
|    12945 |  410 | `		case '"':{` |
|        - |  411 | `			sxi32 iNest;` |
|        - |  412 | `			/* Double quoted string */` |
|    25895 |  413 | `			pStr->zString++;` |
|   211307 |  414 | `			while( pStream->zText < pStream->zEnd ){` |
|   211307 |  415 | `				if( pStream->zText[0] == '{' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '$'){` |
|      117 |  416 | `					iNest = 1;` |
|      117 |  417 | `					pStream->zText++;` |
|        - |  418 | `					/* TICKET 1433-40: Hnadle braces'{}' in double quoted string where everything is allowed */` |
|     1201 |  419 | `					while(pStream->zText < pStream->zEnd ){` |
|     1201 |  420 | `						if( pStream->zText[0] == '{' ){` |
|        9 |  421 | `							iNest++;` |
|     1197 |  422 | `						}else if (pStream->zText[0] == '}' ){` |
|      125 |  423 | `							iNest--;` |
|      125 |  424 | `							if( iNest <= 0 ){` |
|      117 |  425 | `								pStream->zText++;` |
|      117 |  426 | `								break;` |
|        1 |  427 | `							}` |
|     1075 |  428 | `						}else if( pStream->zText[0] == '\n' ){` |
|      ! 0 |  429 | `							pStream->nLine++;` |
|      ! 0 |  430 | `						}` |
|     1087 |  431 | `						pStream->zText++;` |
|        3 |  432 | `					}` |
|      117 |  433 | `					if( pStream->zText >= pStream->zEnd ){` |
|      ! 0 |  434 | `						break;` |
|        - |  435 | `					}` |
|       57 |  436 | `				}` |
|   211307 |  437 | `				if( pStream->zText[0] == '"' ){` |
|    26115 |  438 | `					if( pStream->zText[-1] != '\\' ){` |
|    25889 |  439 | `						break;` |
|      ! 0 |  440 | `					}else{` |
|      231 |  441 | `						const unsigned char *zPtr = &pStream->zText[-2];` |
|      231 |  442 | `						sxi32 i = 1;` |
|      285 |  443 | `						while( zPtr > pStream->zInput && zPtr[0] == '\\' ){` |
|       57 |  444 | `							zPtr--;` |
|       57 |  445 | `							i++;` |
|        3 |  446 | `						}` |
|      231 |  447 | `						if((i&1)==0){` |
|        7 |  448 | `							break;` |
|        - |  449 | `						}` |
|        - |  450 | `					}` |
|      110 |  451 | `				}` |
|   185417 |  452 | `				if( pStream->zText[0] == '\n' ){` |
|       29 |  453 | `					pStream->nLine++;` |
|       14 |  454 | `				}` |
|   185417 |  455 | `				pStream->zText++;` |
|        5 |  456 | `			}` |
|        - |  457 | `			/* Record token length and type */` |
|    25895 |  458 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|    25895 |  459 | `			pToken->nType = PH7_TK_DSTR;` |
|        - |  460 | `			/* Jump the trailing quote */` |
|    25895 |  461 | `			pStream->zText++;` |
|    25895 |  462 | `			return SXRET_OK;` |
|        - |  463 | `				  }` |
|        2 |  464 | ``		case '`':{`` |
|        - |  465 | `			/* Backtick quoted string */` |
|        6 |  466 | `			pStr->zString++;` |
|       46 |  467 | `			while( pStream->zText < pStream->zEnd ){` |
|       46 |  468 | ``				if( pStream->zText[0] == '`' && pStream->zText[-1] != '\\' ){`` |
|        6 |  469 | `					break;` |
|        - |  470 | `				}` |
|       42 |  471 | `				if( pStream->zText[0] == '\n' ){` |
|      ! 0 |  472 | `					pStream->nLine++;` |
|      ! 0 |  473 | `				}` |
|       42 |  474 | `				pStream->zText++;` |
|        2 |  475 | `			}` |
|        - |  476 | `			/* Record token length and type */` |
|        6 |  477 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|        6 |  478 | `			pToken->nType = PH7_TK_BSTR;` |
|        - |  479 | `			/* Jump the trailing backtick */` |
|        6 |  480 | `			pStream->zText++;` |
|        6 |  481 | `			return SXRET_OK;` |
|        - |  482 | `				  }` |
|     8431 |  483 | `		case '\\': pToken->nType = PH7_TK_NSSEP;  break;` |
|     4012 |  484 | `		case ':':` |
|     8029 |  485 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == ':' ){` |
|        - |  486 | `				/* Current operator: '::' */` |
|      391 |  487 | `				pStream->zText++;` |
|      198 |  488 | `			}else{` |
|     7643 |  489 | `				pToken->nType = PH7_TK_COLON; /* Single colon */` |
|        - |  490 | `			}` |
|     8029 |  491 | `			break;` |
|   159451 |  492 | `		case ',': pToken->nType \|= PH7_TK_COMMA;  break; /* Comma is also an operator */` |
|   898269 |  493 | `		case ';': pToken->nType = PH7_TK_SEMI;    break;` |
|        - |  494 | `			/* Handle combined operators [i.e: +=,===,!=== ...] */` |
|   230371 |  495 | `		case '=':` |
|   460747 |  496 | `			pToken->nType \|= PH7_TK_EQUAL;` |
|   460747 |  497 | `			if( pStream->zText < pStream->zEnd ){` |
|   460747 |  498 | `				if( pStream->zText[0] == '=' ){` |
|    24625 |  499 | `					pToken->nType &= ~PH7_TK_EQUAL;` |
|        - |  500 | `					/* Current operator: == */` |
|    24625 |  501 | `					pStream->zText++;` |
|    24625 |  502 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  503 | `						/* Current operator: === */` |
|     5081 |  504 | `						pStream->zText++;` |
|     2543 |  505 | `					}` |
|   448437 |  506 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  507 | `					/* Array operator: => */` |
|     6291 |  508 | `					pToken->nType = PH7_TK_ARRAY_OP;` |
|     6291 |  509 | `					pStream->zText++;` |
|     3148 |  510 | `				}else{` |
|        - |  511 | `					/* TICKET 1433-0010: Reference operator '=&' */` |
|   429841 |  512 | `					const unsigned char *zCur = pStream->zText;` |
|   429841 |  513 | `					sxu32 nLine = 0;` |
|   859505 |  514 | `					while( zCur < pStream->zEnd && zCur[0] < 0xc0 && SyisSpace(zCur[0]) ){` |
|   429669 |  515 | `						if( zCur[0] == '\n' ){` |
|        5 |  516 | `							nLine++;` |
|        2 |  517 | `						}` |
|   429669 |  518 | `						zCur++;` |
|        5 |  519 | `					}` |
|   429841 |  520 | `					if( zCur < pStream->zEnd && zCur[0] == '&' ){` |
|        - |  521 | `						/* Current operator: =& */` |
|       64 |  522 | `						pToken->nType &= ~PH7_TK_EQUAL;` |
|       64 |  523 | `						SyStringInitFromBuf(pStr,"=&",sizeof("=&")-1);` |
|        - |  524 | `						/* Update token stream */` |
|       64 |  525 | `						pStream->zText = &zCur[1];` |
|       64 |  526 | `						pStream->nLine += nLine;` |
|       30 |  527 | `					}` |
|        - |  528 | `				}` |
|   230371 |  529 | `			}` |
|   460747 |  530 | `			break;` |
|    23490 |  531 | `		case '!':` |
|    46985 |  532 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  533 | `				/* Current operator: != */` |
|    15583 |  534 | `				pStream->zText++;` |
|    15583 |  535 | `				if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  536 | `					/* Current operator: !== */` |
|    11705 |  537 | `					pStream->zText++;` |
|     5850 |  538 | `				}` |
|     7789 |  539 | `			}` |
|    46985 |  540 | `			break;` |
|    15713 |  541 | `		case '&':` |
|    31431 |  542 | `			pToken->nType \|= PH7_TK_AMPER;` |
|    31431 |  543 | `			if( pStream->zText < pStream->zEnd ){` |
|    31431 |  544 | `				if( pStream->zText[0] == '&' ){` |
|    12045 |  545 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|        - |  546 | `					/* Current operator: && */` |
|    12045 |  547 | `					pStream->zText++;` |
|    25411 |  548 | `				}else if( pStream->zText[0] == '=' ){` |
|        7 |  549 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|        - |  550 | `					/* Current operator: &= */` |
|        7 |  551 | `					pStream->zText++;` |
|        3 |  552 | `				}` |
|    15713 |  553 | `			}` |
|    31431 |  554 | `			break;` |
|     2120 |  555 | `		case '\|':` |
|     4245 |  556 | `			if( pStream->zText < pStream->zEnd ){` |
|     4245 |  557 | `				if( pStream->zText[0] == '\|' ){` |
|        - |  558 | `					/* Current operator: \|\| */` |
|     4017 |  559 | `					pStream->zText++;` |
|     2239 |  560 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  561 | `					/* Current operator: \|= */` |
|        7 |  562 | `					pStream->zText++;` |
|      230 |  563 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  564 | `					/* Current operator: \|> (PHP 8.5 pipe) */` |
|       27 |  565 | `					pStream->zText++;` |
|       13 |  566 | `				}` |
|     2120 |  567 | `			}` |
|     4245 |  568 | `			break;` |
|    10126 |  569 | `		case '+':` |
|    20257 |  570 | `			if( pStream->zText < pStream->zEnd ){` |
|    20255 |  571 | `				if( pStream->zText[0] == '+' ){` |
|        - |  572 | `					/* Current operator: ++ */` |
|    15763 |  573 | `					pStream->zText++;` |
|    12376 |  574 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  575 | `					/* Current operator: += */` |
|       61 |  576 | `					pStream->zText++;` |
|       28 |  577 | `				}` |
|    10125 |  578 | `			}` |
|    20257 |  579 | `			break;` |
|    96304 |  580 | `		case '-':` |
|   192613 |  581 | `			if( pStream->zText < pStream->zEnd ){` |
|   192613 |  582 | `				if( pStream->zText[0] == '-' ){` |
|        - |  583 | `					/* Current operator: -- */` |
|       37 |  584 | `					pStream->zText++;` |
|   192596 |  585 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  586 | `					/* Current operator: -= */` |
|       10 |  587 | `					pStream->zText++;` |
|   192575 |  588 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  589 | `					/* Current operator: -> */` |
|   191785 |  590 | `					pStream->zText++;` |
|    95890 |  591 | `				}` |
|    96304 |  592 | `			}` |
|   192613 |  593 | `			break;` |
|      182 |  594 | `		case '*':` |
|      369 |  595 | `			if( pStream->zText < pStream->zEnd ){` |
|      369 |  596 | `				if( pStream->zText[0] == '*' ){` |
|        - |  597 | `					/* Current operator: ** or **= */` |
|      135 |  598 | `					pStream->zText++;` |
|      135 |  599 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  600 | `						/* Current operator: **= */` |
|       23 |  601 | `						pStream->zText++;` |
|       12 |  602 | `					}` |
|      302 |  603 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  604 | `					/* Current operator: *= */` |
|       20 |  605 | `					pStream->zText++;` |
|        9 |  606 | `				}` |
|      182 |  607 | `			}` |
|      369 |  608 | `			break;` |
|       48 |  609 | `		case '/':` |
|       98 |  610 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  611 | `				/* Current operator: /= */` |
|        7 |  612 | `				pStream->zText++;` |
|        3 |  613 | `			}` |
|       98 |  614 | `			break;` |
|       40 |  615 | `		case '%':` |
|       85 |  616 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  617 | `				/* Current operator: %= */` |
|        7 |  618 | `				pStream->zText++;` |
|        3 |  619 | `			}` |
|       85 |  620 | `			break;` |
|       11 |  621 | `		case '^':` |
|       23 |  622 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  623 | `				/* Current operator: ^= */` |
|        9 |  624 | `				pStream->zText++;` |
|        4 |  625 | `			}` |
|       23 |  626 | `			break;` |
|    63841 |  627 | `		case '.':` |
|   127687 |  628 | `			if( pStream->zText + 1 < pStream->zEnd && pStream->zText[0] == '.' && pStream->zText[1] == '.' ){` |
|        - |  629 | `				/* Ellipsis: ... */` |
|     7915 |  630 | `				pStream->zText += 2;` |
|     7915 |  631 | `				pToken->nType = PH7_TK_ELLIPSIS;` |
|   123732 |  632 | `			}else if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  633 | `				/* Current operator: .= */` |
|     3979 |  634 | `				pStream->zText++;` |
|     1987 |  635 | `			}` |
|   127687 |  636 | `			break;` |
|    32986 |  637 | `		case '<':` |
|    65977 |  638 | `			if( pStream->zText < pStream->zEnd ){` |
|    65977 |  639 | `				if( pStream->zText[0] == '<' ){` |
|        - |  640 | `					/* Current operator: << */` |
|      145 |  641 | `					pStream->zText++;` |
|      145 |  642 | `					if( pStream->zText < pStream->zEnd ){` |
|      145 |  643 | `						if( pStream->zText[0] == '=' ){` |
|        - |  644 | `							/* Current operator: <<= */` |
|        9 |  645 | `							pStream->zText++;` |
|      141 |  646 | `						}else if( pStream->zText[0] == '<' ){` |
|        - |  647 | `							/* Current Token: <<<  */` |
|      123 |  648 | `							pStream->zText++;` |
|        - |  649 | `							/* This may be the beginning of a Heredoc/Nowdoc string,try to delimit it */` |
|      123 |  650 | `							rc = LexExtractHeredoc(&(*pStream),&(*pToken));` |
|      123 |  651 | `							if( rc == SXRET_OK ){` |
|        - |  652 | `								/* Here/Now doc successfuly extracted */` |
|      123 |  653 | `								return SXRET_OK;` |
|        - |  654 | `							}` |
|      ! 0 |  655 | `						}` |
|       12 |  656 | `					}` |
|    65848 |  657 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  658 | `					/* Current operator: <> */` |
|        5 |  659 | `					pStream->zText++;` |
|    65835 |  660 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  661 | `					/* Current operator: <= or <=> */` |
|      117 |  662 | `					pStream->zText++;` |
|      117 |  663 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '>' ){` |
|        - |  664 | `						/* Current operator: <=> */` |
|       56 |  665 | `						pStream->zText++;` |
|       27 |  666 | `					}` |
|       56 |  667 | `				}` |
|    32927 |  668 | `			}` |
|    65859 |  669 | `			break;` |
|     3990 |  670 | `		case '>':` |
|     7985 |  671 | `			if( pStream->zText < pStream->zEnd ){` |
|     7985 |  672 | `				if( pStream->zText[0] == '>' ){` |
|        - |  673 | `					/* Current operator: >> */` |
|       21 |  674 | `					pStream->zText++;` |
|       21 |  675 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  676 | `						/* Current operator: >>= */` |
|       11 |  677 | `						pStream->zText++;` |
|        6 |  678 | `					}` |
|     7975 |  679 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  680 | `					/* Current operator: >= */` |
|       95 |  681 | `					pStream->zText++;` |
|       45 |  682 | `				}` |
|     3990 |  683 | `			}` |
|     7985 |  684 | `			break;` |
|     3469 |  685 | `		case '?':` |
|     6943 |  686 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '?' ){` |
|        - |  687 | `				/* Null coalescing operator: ?? */` |
|      203 |  688 | `				pStream->zText++;` |
|      203 |  689 | `				if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  690 | `					/* Null coalescing assignment operator (PHP 7.4) */` |
|       73 |  691 | `					pStream->zText++;` |
|       34 |  692 | `				}` |
|     6844 |  693 | `			}else if( (pStream->zEnd - pStream->zText) >= 2` |
|     6745 |  694 | `				&& pStream->zText[0] == '-' && pStream->zText[1] == '>' ){` |
|        - |  695 | `				/* Nullsafe object operator (PHP 8.0): ?-> */` |
|      119 |  696 | `				pStream->zText += 2;` |
|       57 |  697 | `			}` |
|     6938 |  698 | `			break;` |
|      115 |  699 | `		default:` |
|      230 |  700 | `			break;` |
|        - |  701 | `		}` |
|  7471927 |  702 | `		if( pStr->nByte <= 0 ){` |
|        - |  703 | `			/* Record token length */` |
|  7471867 |  704 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  3735931 |  705 | `		}` |
|  7471927 |  706 | `		if( pToken->nType & PH7_TK_OP ){` |
|        - |  707 | `			const ph7_expr_op *pOp;` |
|        - |  708 | `			/* Check if the extracted token is an operator */` |
|  1211417 |  709 | `			pOp = PH7_ExprExtractOperator(pStr,(SyToken *)SySetPeek(pStream->pSet));` |
|  1211417 |  710 | `			if( pOp == 0 ){` |
|        - |  711 | `				/* Not an operator */` |
|      ! 0 |  712 | `				pToken->nType &= ~PH7_TK_OP;` |
|      ! 0 |  713 | `				if( pToken->nType <= 0 ){` |
|      ! 0 |  714 | `					pToken->nType = PH7_TK_OTHER;` |
|      ! 0 |  715 | `				}` |
|      ! 0 |  716 | `			}else{` |
|        - |  717 | `				/* Save the instance associated with this operator for later processing */` |
|  1211417 |  718 | `				pToken->pUserData = (void *)pOp;` |
|        - |  719 | `			}` |
|   605706 |  720 | `		}` |
|        - |  721 | `	}` |
|        - |  722 | `	/* Tell the upper-layer to save the extracted token for later processing */` |
| 12316989 |  723 | `	return SXRET_OK;` |
|  6377197 |  724 | `}` |
|        - |  725 | `/* SPDX-SnippetBegin */` |
|        - |  726 | `/* SPDX-SnippetCopyrightText: SQLite mkkeywordhash.c (D. Richard Hipp and the SQLite authors <https://sqlite.org/>); adapted for the PH7 engine by Chems mrad */` |
|        - |  727 | `/* SPDX-License-Identifier: blessing */` |
|        - |  728 | `/***** This file contains automatically generated code ******` |
|        - |  729 | `**` |
|        - |  730 | `** The code in this file has been automatically generated by` |
|        - |  731 | `**` |
|        - |  732 | `**     $Header: /sqlite/sqlite/tool/mkkeywordhash.c` |
|        - |  733 | `**` |
|        - |  734 | `** Sligthly modified by Chems mrad <chm@symisc.net> for the PH7 engine.` |
|        - |  735 | `**` |
|        - |  736 | `** The code in this file implements a function that determines whether` |
|        - |  737 | `** or not a given identifier is really a PHP keyword.  The same thing` |
|        - |  738 | `** might be implemented more directly using a hand-written hash table.` |
|        - |  739 | `** But by using this automatically generated code, the size of the code` |
|        - |  740 | `** is substantially reduced.  This is important for embedded applications` |
|        - |  741 | `** on platforms with limited memory.` |
|        - |  742 | `*/` |
|        - |  743 | `/* Hash score: 103 */` |
|  4845067 |  744 | `static sxu32 KeywordCode(const char *z, int n){` |
|        - |  745 | `  /* zText[] encodes 532 bytes of keywords in 333 bytes */` |
|        - |  746 | `  /*   extendswitchprintegerequire_oncenddeclareturnamespacechobject      */` |
|        - |  747 | `  /*   hrowbooleandefaultrycaselfinalistaticlonewconstringlobaluse        */` |
|        - |  748 | `  /*   lseifloatvarrayANDIEchoUSECHOabstractclasscontinuendifunction      */` |
|        - |  749 | `  /*   diendwhilevaldoexitgotoimplementsinclude_oncemptyinstanceof        */` |
|        - |  750 | `  /*   interfacendforeachissetparentprivateprotectedpublicatchunset       */` |
|        - |  751 | `  /*   xorARRAYASArrayEXITUNSETXORbreak                                   */` |
|        - |  752 | `  static const char zText[332] = {` |
|        - |  753 | `    'e','x','t','e','n','d','s','w','i','t','c','h','p','r','i','n','t','e',` |
|        - |  754 | `    'g','e','r','e','q','u','i','r','e','_','o','n','c','e','n','d','d','e',` |
|        - |  755 | `    'c','l','a','r','e','t','u','r','n','a','m','e','s','p','a','c','e','c',` |
|        - |  756 | `    'h','o','b','j','e','c','t','h','r','o','w','b','o','o','l','e','a','n',` |
|        - |  757 | `    'd','e','f','a','u','l','t','r','y','c','a','s','e','l','f','i','n','a',` |
|        - |  758 | `    'l','i','s','t','a','t','i','c','l','o','n','e','w','c','o','n','s','t',` |
|        - |  759 | `    'r','i','n','g','l','o','b','a','l','u','s','e','l','s','e','i','f','l',` |
|        - |  760 | `    'o','a','t','v','a','r','r','a','y','A','N','D','I','E','c','h','o','U',` |
|        - |  761 | `    'S','E','C','H','O','a','b','s','t','r','a','c','t','c','l','a','s','s',` |
|        - |  762 | `    'c','o','n','t','i','n','u','e','n','d','i','f','u','n','c','t','i','o',` |
|        - |  763 | `    'n','d','i','e','n','d','w','h','i','l','e','v','a','l','d','o','e','x',` |
|        - |  764 | `    'i','t','g','o','t','o','i','m','p','l','e','m','e','n','t','s','i','n',` |
|        - |  765 | `    'c','l','u','d','e','_','o','n','c','e','m','p','t','y','i','n','s','t',` |
|        - |  766 | `    'a','n','c','e','o','f','i','n','t','e','r','f','a','c','e','n','d','f',` |
|        - |  767 | `    'o','r','e','a','c','h','i','s','s','e','t','p','a','r','e','n','t','p',` |
|        - |  768 | `    'r','i','v','a','t','e','p','r','o','t','e','c','t','e','d','p','u','b',` |
|        - |  769 | `    'l','i','c','a','t','c','h','u','n','s','e','t','x','o','r','A','R','R',` |
|        - |  770 | `    'A','Y','A','S','A','r','r','a','y','E','X','I','T','U','N','S','E','T',` |
|        - |  771 | `    'X','O','R','b','r','e','a','k'` |
|        - |  772 | `  };` |
|        - |  773 | `  static const unsigned char aHash[151] = {` |
|        - |  774 | `       0,   0,   4,  83,   0,  61,  39,  12,   0,  33,  77,   0,  48,` |
|        - |  775 | `       0,   2,  65,  67,   0,   0,   0,  47,   0,   0,  40,   0,  15,` |
|        - |  776 | `      74,   0,  51,   0,  76,   0,   0,  20,   0,   0,   0,  50,   0,` |
|        - |  777 | `      80,  34,   0,  36,   0,   0,  64,  16,   0,   0,  17,   0,   1,` |
|        - |  778 | `      19,  84,  66,   0,  43,  45,  78,   0,   0,  53,  56,   0,   0,` |
|        - |  779 | `       0,  23,  49,   0,   0,  13,  31,  54,   7,   0,   0,  25,   0,` |
|        - |  780 | `      72,  14,   0,  71,   0,  38,   6,   0,   0,   0,  73,   0,   0,` |
|        - |  781 | `       3,   0,  41,   5,  52,  57,  32,   0,  60,  63,   0,  69,  82,` |
|        - |  782 | `      30,   0,  79,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,` |
|        - |  783 | `       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  81,   0,   0,` |
|        - |  784 | `      62,   0,  11,   0,   0,  58,   0,   0,   0,   0,  59,  75,   0,` |
|        - |  785 | `       0,   0,   0,   0,   0,  35,  27,   0` |
|        - |  786 | `  };` |
|        - |  787 | `  static const unsigned char aNext[84] = {` |
|        - |  788 | `       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,` |
|        - |  789 | `       0,   0,   8,   0,   0,   0,  10,   0,   0,   0,   0,   0,   0,` |
|        - |  790 | `       0,   0,   0,   0,  28,   0,   0,   0,   0,   0,   0,   0,   0,` |
|        - |  791 | `       0,   0,   0,   0,   0,  44,   0,  18,   0,   0,   0,   0,   0,` |
|        - |  792 | `       0,  46,   0,  29,   0,   0,   0,  22,   0,   0,   0,   0,  26,` |
|        - |  793 | `       0,  21,  24,   0,   0,  68,   0,   0,   9,  37,   0,   0,   0,` |
|        - |  794 | `      42,   0,   0,   0,  70,  55` |
|        - |  795 | `  };` |
|        - |  796 | `  static const unsigned char aLen[84] = {` |
|        - |  797 | `       7,   9,   6,   5,   7,  12,   7,   2,  10,   7,   6,   9,   4,` |
|        - |  798 | `       6,   5,   7,   4,   3,   7,   3,   4,   4,   5,   4,   6,   5,` |
|        - |  799 | `       2,   3,   5,   6,   6,   3,   6,   4,   2,   5,   3,   5,   3,` |
|        - |  800 | `       3,   4,   3,   4,   8,   5,   2,   8,   5,   8,   3,   8,   5,` |
|        - |  801 | `       4,   2,   4,   4,  10,  12,   7,   5,  10,   9,   3,   6,  10,` |
|        - |  802 | `       3,   7,   2,   5,   6,   7,   9,   6,   5,   5,   3,   5,   2,` |
|        - |  803 | `       5,   4,   5,   3,   2,   5` |
|        - |  804 | `  };` |
|        - |  805 | `  static const sxu16 aOffset[84] = {` |
|        - |  806 | `       0,   3,   6,  12,  14,  20,  20,  21,  31,  34,  39,  44,  52,` |
|        - |  807 | `      55,  60,  65,  65,  70,  72,  78,  81,  83,  86,  90,  92,  97,` |
|        - |  808 | `     100, 100, 103, 106, 111, 117, 119, 119, 123, 124, 129, 130, 135,` |
|        - |  809 | `     137, 139, 143, 145, 149, 157, 159, 162, 169, 173, 181, 183, 186,` |
|        - |  810 | `     190, 194, 196, 200, 204, 214, 214, 225, 230, 240, 240, 248, 248,` |
|        - |  811 | `     251, 251, 252, 258, 263, 269, 276, 285, 290, 295, 300, 303, 308,` |
|        - |  812 | `     310, 315, 319, 324, 325, 327` |
|        - |  813 | `  };` |
|        - |  814 | `  static const sxu32 aCode[84] = {` |
|        - |  815 | `    PH7_TKWRD_EXTENDS,   PH7_TKWRD_ENDSWITCH,   PH7_TKWRD_SWITCH,    PH7_TKWRD_PRINT,   PH7_TKWRD_INT,` |
|        - |  816 | `    PH7_TKWRD_REQONCE,   PH7_TKWRD_REQUIRE,     PH7_TK_ID /* 'eq' PH7-ism removed */, PH7_TKWRD_ENDDEC, PH7_TKWRD_DECLARE,` |
|        - |  817 | `    PH7_TKWRD_RETURN,    PH7_TKWRD_NAMESPACE,   PH7_TKWRD_ECHO,      PH7_TKWRD_OBJECT,    PH7_TKWRD_THROW,` |
|        - |  818 | `    PH7_TKWRD_BOOL,      PH7_TKWRD_BOOL,        PH7_TKWRD_AND,       PH7_TKWRD_DEFAULT,   PH7_TKWRD_TRY,` |
|        - |  819 | `    PH7_TKWRD_CASE,      PH7_TKWRD_SELF,        PH7_TKWRD_FINAL,     PH7_TKWRD_LIST,      PH7_TKWRD_STATIC,` |
|        - |  820 | `    PH7_TKWRD_CLONE,     PH7_TK_ID /* 'ne' PH7-ism removed */, PH7_TKWRD_NEW,  PH7_TKWRD_CONST,     PH7_TKWRD_STRING,` |
|        - |  821 | `    PH7_TKWRD_GLOBAL,    PH7_TKWRD_USE,         PH7_TKWRD_ELIF,      PH7_TKWRD_ELSE,      PH7_TKWRD_IF,` |
|        - |  822 | `    PH7_TKWRD_FLOAT,     PH7_TKWRD_VAR,         PH7_TKWRD_ARRAY,     PH7_TKWRD_AND,       PH7_TKWRD_DIE,` |
|        - |  823 | `    PH7_TKWRD_ECHO,      PH7_TKWRD_USE,         PH7_TKWRD_ECHO,      PH7_TKWRD_ABSTRACT,  PH7_TKWRD_CLASS,` |
|        - |  824 | `    PH7_TKWRD_AS,        PH7_TKWRD_CONTINUE,    PH7_TKWRD_ENDIF,     PH7_TKWRD_FUNCTION,  PH7_TKWRD_DIE,` |
|        - |  825 | `    PH7_TKWRD_ENDWHILE,  PH7_TKWRD_WHILE,       PH7_TKWRD_EVAL,      PH7_TKWRD_DO,        PH7_TKWRD_EXIT,` |
|        - |  826 | `    PH7_TKWRD_GOTO,      PH7_TKWRD_IMPLEMENTS,  PH7_TKWRD_INCONCE,   PH7_TKWRD_INCLUDE,   PH7_TKWRD_EMPTY,` |
|        - |  827 | `    PH7_TKWRD_INSTANCEOF,PH7_TKWRD_INTERFACE,   PH7_TKWRD_INT,       PH7_TKWRD_ENDFOR,    PH7_TKWRD_END4EACH,` |
|        - |  828 | `    PH7_TKWRD_FOR,       PH7_TKWRD_FOREACH,     PH7_TKWRD_OR,        PH7_TKWRD_ISSET,     PH7_TKWRD_PARENT,` |
|        - |  829 | `    PH7_TKWRD_PRIVATE,   PH7_TKWRD_PROTECTED,   PH7_TKWRD_PUBLIC,    PH7_TKWRD_CATCH,     PH7_TKWRD_UNSET,` |
|        - |  830 | `    PH7_TKWRD_XOR,       PH7_TKWRD_ARRAY,       PH7_TKWRD_AS,        PH7_TKWRD_ARRAY,     PH7_TKWRD_EXIT,` |
|        - |  831 | `    PH7_TKWRD_UNSET,     PH7_TKWRD_XOR,         PH7_TKWRD_OR,        PH7_TKWRD_BREAK` |
|        - |  832 | `  };` |
|        - |  833 | `  int h, i;` |
|  4845067 |  834 | `  if( n<2 ) return PH7_TK_ID;` |
|  4673129 |  835 | `  h = (((int)z[0]*4) ^ ((int)z[n-1]*3) ^ n) % 151;` |
|  7017927 |  836 | `  for(i=((int)aHash[h])-1; i>=0; i=((int)aNext[i])-1){` |
|  4158335 |  837 | `    if( (int)aLen[i]==n && SyMemcmp(&zText[aOffset[i]],z,n)==0 ){` |
|        - |  838 | `       /* PH7_TKWRD_EXTENDS */` |
|        - |  839 | `       /* PH7_TKWRD_ENDSWITCH */` |
|        - |  840 | `       /* PH7_TKWRD_SWITCH */` |
|        - |  841 | `       /* PH7_TKWRD_PRINT */` |
|        - |  842 | `       /* PH7_TKWRD_INT */` |
|        - |  843 | `       /* PH7_TKWRD_REQONCE */` |
|        - |  844 | `       /* PH7_TKWRD_REQUIRE */` |
|        - |  845 | `       /* PH7_TK_ID */` |
|        - |  846 | `       /* PH7_TKWRD_ENDDEC */` |
|        - |  847 | `       /* PH7_TKWRD_DECLARE */` |
|        - |  848 | `       /* PH7_TKWRD_RETURN */` |
|        - |  849 | `       /* PH7_TKWRD_NAMESPACE */` |
|        - |  850 | `       /* PH7_TKWRD_ECHO */` |
|        - |  851 | `       /* PH7_TKWRD_OBJECT */` |
|        - |  852 | `       /* PH7_TKWRD_THROW */` |
|        - |  853 | `       /* PH7_TKWRD_BOOL */` |
|        - |  854 | `       /* PH7_TKWRD_BOOL */` |
|        - |  855 | `       /* PH7_TKWRD_AND */` |
|        - |  856 | `       /* PH7_TKWRD_DEFAULT */` |
|        - |  857 | `       /* PH7_TKWRD_TRY */` |
|        - |  858 | `       /* PH7_TKWRD_CASE */` |
|        - |  859 | `       /* PH7_TKWRD_SELF */` |
|        - |  860 | `       /* PH7_TKWRD_FINAL */` |
|        - |  861 | `       /* PH7_TKWRD_LIST */` |
|        - |  862 | `       /* PH7_TKWRD_STATIC */` |
|        - |  863 | `       /* PH7_TKWRD_CLONE */` |
|        - |  864 | `       /* PH7_TK_ID */` |
|        - |  865 | `       /* PH7_TKWRD_NEW */` |
|        - |  866 | `       /* PH7_TKWRD_CONST */` |
|        - |  867 | `       /* PH7_TKWRD_STRING */` |
|        - |  868 | `       /* PH7_TKWRD_GLOBAL */` |
|        - |  869 | `       /* PH7_TKWRD_USE */` |
|        - |  870 | `       /* PH7_TKWRD_ELIF */` |
|        - |  871 | `       /* PH7_TKWRD_ELSE */` |
|        - |  872 | `       /* PH7_TKWRD_IF */` |
|        - |  873 | `       /* PH7_TKWRD_FLOAT */` |
|        - |  874 | `       /* PH7_TKWRD_VAR */` |
|        - |  875 | `       /* PH7_TKWRD_ARRAY */` |
|        - |  876 | `       /* PH7_TKWRD_AND */` |
|        - |  877 | `       /* PH7_TKWRD_DIE */` |
|        - |  878 | `       /* PH7_TKWRD_ECHO */` |
|        - |  879 | `       /* PH7_TKWRD_USE */` |
|        - |  880 | `       /* PH7_TKWRD_ECHO */` |
|        - |  881 | `       /* PH7_TKWRD_ABSTRACT */` |
|        - |  882 | `       /* PH7_TKWRD_CLASS */` |
|        - |  883 | `       /* PH7_TKWRD_AS */` |
|        - |  884 | `       /* PH7_TKWRD_CONTINUE */` |
|        - |  885 | `       /* PH7_TKWRD_ENDIF */` |
|        - |  886 | `       /* PH7_TKWRD_FUNCTION */` |
|        - |  887 | `       /* PH7_TKWRD_DIE */` |
|        - |  888 | `       /* PH7_TKWRD_ENDWHILE */` |
|        - |  889 | `       /* PH7_TKWRD_WHILE */` |
|        - |  890 | `       /* PH7_TKWRD_EVAL */` |
|        - |  891 | `       /* PH7_TKWRD_DO */` |
|        - |  892 | `       /* PH7_TKWRD_EXIT */` |
|        - |  893 | `       /* PH7_TKWRD_GOTO */` |
|        - |  894 | `       /* PH7_TKWRD_IMPLEMENTS */` |
|        - |  895 | `       /* PH7_TKWRD_INCONCE */` |
|        - |  896 | `       /* PH7_TKWRD_INCLUDE */` |
|        - |  897 | `       /* PH7_TKWRD_EMPTY */` |
|        - |  898 | `       /* PH7_TKWRD_INSTANCEOF */` |
|        - |  899 | `       /* PH7_TKWRD_INTERFACE */` |
|        - |  900 | `       /* PH7_TKWRD_INT */` |
|        - |  901 | `       /* PH7_TKWRD_ENDFOR */` |
|        - |  902 | `       /* PH7_TKWRD_END4EACH */` |
|        - |  903 | `       /* PH7_TKWRD_FOR */` |
|        - |  904 | `       /* PH7_TKWRD_FOREACH */` |
|        - |  905 | `       /* PH7_TKWRD_OR */` |
|        - |  906 | `       /* PH7_TKWRD_ISSET */` |
|        - |  907 | `       /* PH7_TKWRD_PARENT */` |
|        - |  908 | `       /* PH7_TKWRD_PRIVATE */` |
|        - |  909 | `       /* PH7_TKWRD_PROTECTED */` |
|        - |  910 | `       /* PH7_TKWRD_PUBLIC */` |
|        - |  911 | `       /* PH7_TKWRD_CATCH */` |
|        - |  912 | `       /* PH7_TKWRD_UNSET */` |
|        - |  913 | `       /* PH7_TKWRD_XOR */` |
|        - |  914 | `       /* PH7_TKWRD_ARRAY */` |
|        - |  915 | `       /* PH7_TKWRD_AS */` |
|        - |  916 | `       /* PH7_TKWRD_ARRAY */` |
|        - |  917 | `       /* PH7_TKWRD_EXIT */` |
|        - |  918 | `       /* PH7_TKWRD_UNSET */` |
|        - |  919 | `       /* PH7_TKWRD_XOR */` |
|        - |  920 | `       /* PH7_TKWRD_OR */` |
|        - |  921 | `       /* PH7_TKWRD_BREAK */` |
|  1813537 |  922 | `      return aCode[i];` |
|        - |  923 | `    }` |
|  1172402 |  924 | `  }` |
|        - |  925 | `  /* Linear fallback for keywords not in the auto-generated hash table */` |
|  2859597 |  926 | `  if( n==5 && SyMemcmp(z,"trait",5)==0 ) return PH7_TKWRD_TRAIT;` |
|  2859531 |  927 | `  if( n==9 && SyMemcmp(z,"insteadof",9)==0 ) return PH7_TKWRD_INSTEADOF;` |
|  2859527 |  928 | `  if( n==7 && SyMemcmp(z,"finally",7)==0 ) return PH7_TKWRD_FINALLY;` |
|  2859355 |  929 | `  if( n==5 && SyMemcmp(z,"yield",5)==0 ) return PH7_TKWRD_YIELD;` |
|  2859023 |  930 | `  if( n==5 && SyMemcmp(z,"match",5)==0 ) return PH7_TKWRD_MATCH;` |
|  2858949 |  931 | `  return PH7_TK_ID;` |
|  2422536 |  932 | `}` |
|        - |  933 | `/* --- End of Automatically generated code --- */` |
|        - |  934 | `/* SPDX-SnippetEnd */` |
|        - |  935 | `/*` |
|        - |  936 | ` * Extract a heredoc/nowdoc text from a raw PHP input.` |
|        - |  937 | ` * According to the PHP language reference manual:` |
|        - |  938 | ` *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier` |
|        - |  939 | ` *  is provided, then a newline. The string itself follows, and then the same identifier again` |
|        - |  940 | ` *  to close the quotation.` |
|        - |  941 | ` *  The closing identifier must begin in the first column of the line. Also, the identifier must` |
|        - |  942 | ` *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric` |
|        - |  943 | ` *  characters and underscores, and must start with a non-digit character or underscore.` |
|        - |  944 | ` *  Heredoc text behaves just like a double-quoted string, without the double quotes.` |
|        - |  945 | ` *  This means that quotes in a heredoc do not need to be escaped, but the escape codes listed` |
|        - |  946 | ` *  above can still be used. Variables are expanded, but the same care must be taken when expressing` |
|        - |  947 | ` *  complex variables inside a heredoc as with strings.` |
|        - |  948 | ` *  Nowdocs are to single-quoted strings what heredocs are to double-quoted strings.` |
|        - |  949 | ` *  A nowdoc is specified similarly to a heredoc, but no parsing is done inside a nowdoc.` |
|        - |  950 | ` *  The construct is ideal for embedding PHP code or other large blocks of text without the need` |
|        - |  951 | ` *  for escaping. It shares some features in common with the SGML <![CDATA[ ]]> construct, in that` |
|        - |  952 | ` *  it declares a block of text which is not for parsing.` |
|        - |  953 | ` *  A nowdoc is identified with the same <<< sequence used for heredocs, but the identifier which follows` |
|        - |  954 | ` *  is enclosed in single quotes, e.g. <<<'EOT'. All the rules for heredoc identifiers also apply to nowdoc` |
|        - |  955 | ` *  identifiers, especially those regarding the appearance of the closing identifier.` |
|        - |  956 | ` * Symisc Extension:` |
|        - |  957 | ` * The closing delimiter can now start with a digit or undersocre or it can be an UTF-8 stream.` |
|        - |  958 | ` * Example:` |
|        - |  959 | ` *  <<<123` |
|        - |  960 | ` *    HEREDOC Here` |
|        - |  961 | ` * 123` |
|        - |  962 | ` *  or` |
|        - |  963 | ` *  <<<___` |
|        - |  964 | ` *   HEREDOC Here` |
|        - |  965 | ` *  ___` |
|        - |  966 | ` */` |
|      118 |  967 | `static sxi32 LexExtractHeredoc(SyStream *pStream,SyToken *pToken)` |
|        5 |  968 | `{` |
|      123 |  969 | `	const unsigned char *zIn  = pStream->zText;` |
|      123 |  970 | `	const unsigned char *zEnd = pStream->zEnd;` |
|        - |  971 | `	const unsigned char *zPtr;` |
|      123 |  972 | `	sxu8 bNowDoc = FALSE;` |
|        - |  973 | `	SyString sDelim;` |
|        - |  974 | `	SyString sStr;` |
|        - |  975 | `	/* Jump leading white spaces */` |
|      135 |  976 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|       13 |  977 | `		zIn++;` |
|        1 |  978 | `	}` |
|      123 |  979 | `	if( zIn >= zEnd ){` |
|        - |  980 | `		/* A simple symbol,return immediately */` |
|      ! 0 |  981 | `		return SXERR_CONTINUE;` |
|        - |  982 | `	}` |
|      123 |  983 | `	if( zIn[0] == '\'' \|\| zIn[0] == '"' ){` |
|        - |  984 | `		/* Make sure we are dealing with a nowdoc */` |
|       51 |  985 | `		bNowDoc =  zIn[0] == '\'' ? TRUE : FALSE;` |
|       51 |  986 | `		zIn++;` |
|       24 |  987 | `	}` |
|      123 |  988 | `	if( zIn[0] < 0xc0 && !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|        - |  989 | `		/* Invalid delimiter,return immediately */` |
|      ! 0 |  990 | `		return SXERR_CONTINUE;` |
|        - |  991 | `	}` |
|        - |  992 | `	/* Isolate the identifier */` |
|      123 |  993 | `	sDelim.zString = (const char *)zIn;` |
|      126 |  994 | `	for(;;){` |
|      257 |  995 | `		zPtr = zIn;` |
|        - |  996 | `		/* Skip alphanumeric stream */` |
|      807 |  997 | `		while( zPtr < zEnd && zPtr[0] < 0xc0 && (SyisAlphaNum(zPtr[0]) \|\| zPtr[0] == '_') ){` |
|      429 |  998 | `			zPtr++;` |
|        5 |  999 | `		}` |
|      257 | 1000 | `		if( zPtr < zEnd && zPtr[0] >= 0xc0 ){` |
|       19 | 1001 | `			zPtr++;` |
|        - | 1002 | `			/* UTF-8 stream */` |
|       37 | 1003 | `			while( zPtr < zEnd && ((zPtr[0] & 0xc0) == 0x80) ){` |
|       19 | 1004 | `				zPtr++;` |
|        1 | 1005 | `			}` |
|        9 | 1006 | `		}` |
|      257 | 1007 | `		if( zPtr == zIn ){` |
|        - | 1008 | `			/* Not an UTF-8 or alphanumeric stream */` |
|      123 | 1009 | `			break;` |
|        - | 1010 | `		}` |
|        - | 1011 | `		/* Synchronize pointers */` |
|      139 | 1012 | `		zIn = zPtr;` |
|        5 | 1013 | `	}` |
|        - | 1014 | `	/* Get the identifier length */` |
|      123 | 1015 | `	sDelim.nByte = (sxu32)((const char *)zIn-sDelim.zString);` |
|      123 | 1016 | `	if( zIn[0] == '"' \|\| (bNowDoc && zIn[0] == '\'') ){` |
|        - | 1017 | `		/* Jump the trailing single quote */` |
|       51 | 1018 | `		zIn++;` |
|       24 | 1019 | `	}` |
|        - | 1020 | `	/* Jump trailing white spaces */` |
|      123 | 1021 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|      ! 0 | 1022 | `		zIn++;` |
|      ! 0 | 1023 | `	}` |
|      123 | 1024 | `	if( sDelim.nByte <= 0 \|\| zIn >= zEnd \|\| zIn[0] != '\n' ){` |
|        - | 1025 | `		/* Invalid syntax */` |
|      ! 0 | 1026 | `		return SXERR_CONTINUE;` |
|        - | 1027 | `	}` |
|      123 | 1028 | `	pStream->nLine++; /* Increment line counter */` |
|      123 | 1029 | `	zIn++;` |
|        - | 1030 | `	/* Isolate the delimited string */` |
|      123 | 1031 | `	sStr.zString = (const char *)zIn;` |
|        - | 1032 | `	/* PHP 7.3 flexible heredoc/nowdoc: the closing marker may be preceded` |
|        - | 1033 | `	 * by whitespace (spaces/tabs), and may be followed by any non-identifier` |
|        - | 1034 | `	 * character. The indent count is recorded in pToken->pUserData and the` |
|        - | 1035 | `	 * compile phase strips it from each body line. */` |
|        - | 1036 | `	{` |
|      123 | 1037 | `		const unsigned char *zMarkerLine = zIn; /* Start of marker's line (set on match) */` |
|      123 | 1038 | `		sxu32 nIndent = 0;` |
|      265 | 1039 | `		for(;;){` |
|      329 | 1040 | `			const unsigned char *zLineStart = zIn;` |
|        - | 1041 | `			/* Skip leading space/tab on this line */` |
|      881 | 1042 | `			while( zIn < zEnd && (zIn[0] == ' ' \|\| zIn[0] == '\t') ){` |
|      394 | 1043 | `				zIn++;` |
|        4 | 1044 | `			}` |
|      324 | 1045 | `			if( (sxu32)(zEnd - zIn) >= sDelim.nByte` |
|      328 | 1046 | `				&& SyMemcmp((const void *)sDelim.zString,(const void *)zIn,sDelim.nByte) == 0 ){` |
|        - | 1047 | `				int bIdentCont;` |
|      121 | 1048 | `				zPtr = &zIn[sDelim.nByte];` |
|        - | 1049 | `				/* Disambiguate: next byte must not continue an identifier.` |
|        - | 1050 | `				 * A leading byte >= 0xc0 starts a multi-byte UTF-8 sequence,` |
|        - | 1051 | `				 * which PHP identifiers may contain, so treat it as ident. */` |
|      121 | 1052 | `				if( zPtr >= zEnd ){` |
|      ! 0 | 1053 | `					bIdentCont = 0;` |
|      121 | 1054 | `				}else if( zPtr[0] >= 0xc0 ){` |
|      ! 0 | 1055 | `					bIdentCont = 1;` |
|      ! 0 | 1056 | `				}else{` |
|      121 | 1057 | `					bIdentCont = (SyisAlphaNum(zPtr[0]) \|\| zPtr[0] == '_');` |
|        - | 1058 | `				}` |
|      121 | 1059 | `				if( !bIdentCont ){` |
|        - | 1060 | `					/* Closing marker found */` |
|      121 | 1061 | `					nIndent = (sxu32)(zIn - zLineStart);` |
|      121 | 1062 | `					zMarkerLine = zLineStart;` |
|      121 | 1063 | `					pStream->zText = zPtr; /* Cursor right after identifier */` |
|      121 | 1064 | `					break;` |
|        - | 1065 | `				}` |
|      ! 0 | 1066 | `			}` |
|        - | 1067 | `			/* Not the closing marker on this line; walk to next newline */` |
|     4481 | 1068 | `			while( zIn < zEnd && zIn[0] != '\n' ){` |
|     4273 | 1069 | `				zIn++;` |
|        5 | 1070 | `			}` |
|      213 | 1071 | `			if( zIn >= zEnd ){` |
|        - | 1072 | `				/* End of input without finding the closing marker */` |
|        3 | 1073 | `				pStream->zText = pStream->zEnd;` |
|        3 | 1074 | `				zMarkerLine = zIn;` |
|        3 | 1075 | `				break;` |
|        - | 1076 | `			}` |
|      211 | 1077 | `			pStream->nLine++;` |
|      211 | 1078 | `			zIn++;` |
|        5 | 1079 | `		}` |
|        - | 1080 | `		/* Body runs from sStr.zString up to just before the marker line */` |
|      123 | 1081 | `		sStr.nByte = (sxu32)((const char *)zMarkerLine - sStr.zString);` |
|      123 | 1082 | `		pToken->nType = bNowDoc ? PH7_TK_NOWDOC : PH7_TK_HEREDOC;` |
|      123 | 1083 | `		SyStringDupPtr(&pToken->sData,&sStr);` |
|        - | 1084 | `		/* Strip exactly one line terminator that precedes the marker's line. */` |
|      118 | 1085 | `		if( pToken->sData.nByte > 0` |
|      119 | 1086 | `			&& pToken->sData.zString[pToken->sData.nByte - 1] == '\n' ){` |
|      113 | 1087 | `			pToken->sData.nByte--;` |
|      108 | 1088 | `			if( pToken->sData.nByte > 0` |
|      113 | 1089 | `				&& pToken->sData.zString[pToken->sData.nByte - 1] == '\r' ){` |
|      ! 0 | 1090 | `				pToken->sData.nByte--;` |
|      ! 0 | 1091 | `			}` |
|       54 | 1092 | `		}` |
|      123 | 1093 | `		pToken->pUserData = SX_INT_TO_PTR(nIndent);` |
|        - | 1094 | `	}` |
|        - | 1095 | `	/* All done */` |
|      123 | 1096 | `	return SXRET_OK;` |
|       64 | 1097 | `}` |
|        - | 1098 | `/*` |
|        - | 1099 | ` * Tokenize a raw PHP input.` |
|        - | 1100 | ` * This is the public tokenizer called by most code generator routines.` |
|        - | 1101 | ` */` |
|    20586 | 1102 | `PH7_PRIVATE sxi32 PH7_TokenizePHP(const char *zInput,sxu32 nLen,sxu32 nLineStart,SySet *pOut)` |
|        5 | 1103 | `{` |
|        - | 1104 | `	SyLex sLexer;` |
|        - | 1105 | `	sxi32 rc;` |
|        - | 1106 | `	/* Defense-in-depth cap for internal tokenizer calls that bypass ph7_compile() */` |
|    20591 | 1107 | `	if( nLen > PH7_MAX_INPUT_SIZE ){` |
|      ! 0 | 1108 | `		return SXERR_LIMIT;` |
|        - | 1109 | `	}` |
|        - | 1110 | `	/* Initialize the lexer */` |
|    20591 | 1111 | `	rc = SyLexInit(&sLexer,&(*pOut),TokenizePHP,0);` |
|    20591 | 1112 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1113 | `		return rc;` |
|        - | 1114 | `	}` |
|    20591 | 1115 | `	sLexer.sStream.nLine = nLineStart;` |
|        - | 1116 | `	/* Tokenize input */` |
|    20591 | 1117 | `	rc = SyLexTokenizeInput(&sLexer,zInput,nLen,0,0,0);` |
|        - | 1118 | `	/* Release the lexer */` |
|    20591 | 1119 | `	SyLexRelease(&sLexer);` |
|        - | 1120 | `	/* Tokenization result */` |
|    20591 | 1121 | `	return rc;` |
|    10298 | 1122 | `}` |
|        - | 1123 | `/*` |
|        - | 1124 | ` * High level public tokenizer.` |
|        - | 1125 | ` *  Tokenize the input into PHP tokens and raw tokens [i.e: HTML,XML,Raw text...].` |
|        - | 1126 | ` * According to the PHP language reference manual` |
|        - | 1127 | ` *   When PHP parses a file, it looks for opening and closing tags, which tell PHP` |
|        - | 1128 | ` *   to start and stop interpreting the code between them. Parsing in this manner allows` |
|        - | 1129 | ` *   PHP to be embedded in all sorts of different documents, as everything outside of a pair` |
|        - | 1130 | ` *   of opening and closing tags is ignored by the PHP parser. Most of the time you will see` |
|        - | 1131 | ` *   PHP embedded in HTML documents, as in this example.` |
|        - | 1132 | ` *   <?php echo 'While this is going to be parsed.'; ?>` |
|        - | 1133 | ` *   <p>This will also be ignored.</p>` |
|        - | 1134 | ` *   You can also use more advanced structures:` |
|        - | 1135 | ` *   Example #1 Advanced escaping` |
|        - | 1136 | ` * <?php` |
|        - | 1137 | ` * if ($expression) {` |
|        - | 1138 | ` *   ?>` |
|        - | 1139 | ` *   <strong>This is true.</strong>` |
|        - | 1140 | ` *   <?php` |
|        - | 1141 | ` * } else {` |
|        - | 1142 | ` *   ?>` |
|        - | 1143 | ` *   <strong>This is false.</strong>` |
|        - | 1144 | ` *   <?php` |
|        - | 1145 | ` * }` |
|        - | 1146 | ` * ?>` |
|        - | 1147 | ` * This works as expected, because when PHP hits the ?> closing tags, it simply starts outputting` |
|        - | 1148 | ` * whatever it finds (except for an immediately following newline - see instruction separation ) until it hits` |
|        - | 1149 | ` * another opening tag. The example given here is contrived, of course, but for outputting large blocks of text` |
|        - | 1150 | ` * dropping out of PHP parsing mode is generally more efficient than sending all of the text through echo() or print().` |
|        - | 1151 | ` * There are four different pairs of opening and closing tags which can be used in PHP. Three of those, <?php ?>` |
|        - | 1152 | ` * <script language="php"> </script>  and <? ?> are always available. The other two are short tags and ASP style` |
|        - | 1153 | ` * tags, and can be turned on and off from the php.ini configuration file. As such, while some people find short tags` |
|        - | 1154 | ` * and ASP style tags convenient, they are less portable, and generally not recommended.` |
|        - | 1155 | ` * Note:` |
|        - | 1156 | ` * Also note that if you are embedding PHP within XML or XHTML you will need to use the <?php ?> tags to remain` |
|        - | 1157 | ` * compliant with standards.` |
|        - | 1158 | ` * Example #2 PHP Opening and Closing Tags` |
|        - | 1159 | ` * 1.  <?php echo 'if you want to serve XHTML or XML documents, do it like this'; ?>` |
|        - | 1160 | ` * 2.  <script language="php">` |
|        - | 1161 | ` *       echo 'some editors (like FrontPage) don\'t` |
|        - | 1162 | ` *             like processing instructions';` |
|        - | 1163 | ` *   </script>` |
|        - | 1164 | ` *` |
|        - | 1165 | ` * 3.  <? echo 'this is the simplest, an SGML processing instruction'; ?>` |
|        - | 1166 | ` *   <?= expression ?> This is a shortcut for "<? echo expression ?>"` |
|        - | 1167 | ` */` |
|    13370 | 1168 | `PH7_PRIVATE sxi32 PH7_TokenizeRawText(const char *zInput,sxu32 nLen,SySet *pOut)` |
|        5 | 1169 | `{` |
|    13375 | 1170 | `	const char *zEnd = &zInput[nLen];` |
|    13375 | 1171 | `	const char *zIn  = zInput;` |
|        - | 1172 | `	const char *zCur,*zCurEnd;` |
|    13375 | 1173 | `	SyString sCtag = { 0, 0 };     /* Closing tag */` |
|        - | 1174 | `	SyToken sToken;` |
|        - | 1175 | `	SyString sDoc;` |
|        - | 1176 | `	sxu32 nLine;` |
|        - | 1177 | `	sxi32 iNest;` |
|        - | 1178 | `	sxi32 rc;` |
|        - | 1179 | `	/* Tokenize the input into PHP tokens and raw tokens */` |
|    13375 | 1180 | `	nLine = 1;` |
|    13375 | 1181 | `	zCur = zCurEnd   = 0; /* Prevent compiler warning */` |
|    13375 | 1182 | `	sToken.pUserData = 0;` |
|    13375 | 1183 | `	iNest = 0;` |
|    13375 | 1184 | `	sDoc.nByte = 0;` |
|    13375 | 1185 | `	sDoc.zString = ""; /* cc warning */` |
|    13372 | 1186 | `	for(;;){` |
|    26749 | 1187 | `		if( zIn >= zEnd ){` |
|        - | 1188 | `			/* End of input reached */` |
|    13325 | 1189 | `			break;` |
|        - | 1190 | `		}` |
|    13429 | 1191 | `		sToken.nLine = nLine;` |
|    13429 | 1192 | `		zCur = zIn;` |
|    13429 | 1193 | `		zCurEnd = 0;` |
|    13483 | 1194 | `		while( zIn < zEnd ){` |
|    13433 | 1195 | `			 if( zIn[0] == '<' ){` |
|    13379 | 1196 | `				const char *zTmp = zIn; /* End of raw input marker */` |
|    13379 | 1197 | `				zIn++;` |
|    13379 | 1198 | `				if( zIn < zEnd ){` |
|    13379 | 1199 | `					if( zIn[0] == '?' ){` |
|    13379 | 1200 | `						zIn++;` |
|    13379 | 1201 | `						if( (sxu32)(zEnd - zIn) >= sizeof("php")-1 &&  SyStrnicmp(zIn,"php",sizeof("php")-1) == 0 ){` |
|        - | 1202 | `							/* opening tag: <?php */` |
|    13377 | 1203 | `							zIn += sizeof("php")-1;` |
|     6686 | 1204 | `						}` |
|        - | 1205 | `						/* Look for the closing tag '?>' */` |
|    13379 | 1206 | `						SyStringInitFromBuf(&sCtag,"?>",sizeof("?>")-1);` |
|    13379 | 1207 | `						zCurEnd = zTmp;` |
|    13379 | 1208 | `						break;` |
|        - | 1209 | `					}` |
|      ! 0 | 1210 | `				}` |
|      ! 0 | 1211 | `			}else{` |
|       59 | 1212 | `				if( zIn[0] == '\n' ){` |
|       59 | 1213 | `					nLine++;` |
|       27 | 1214 | `				}` |
|       59 | 1215 | `				zIn++;` |
|        - | 1216 | `			 }` |
|        5 | 1217 | `		} /* While(zIn < zEnd) */` |
|    13429 | 1218 | `		if( zCurEnd == 0 ){` |
|       54 | 1219 | `			zCurEnd = zIn;` |
|       25 | 1220 | `		}` |
|        - | 1221 | `		/* Save the raw token */` |
|    13429 | 1222 | `		SyStringInitFromBuf(&sToken.sData,zCur,zCurEnd - zCur);` |
|    13429 | 1223 | `		sToken.nType = PH7_TOKEN_RAW;` |
|    13429 | 1224 | `		rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|    13429 | 1225 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1226 | `			return rc;` |
|        - | 1227 | `		}` |
|    13429 | 1228 | `		if( zIn >= zEnd ){` |
|       54 | 1229 | `			break;` |
|        - | 1230 | `		}` |
|        - | 1231 | `		/* Ignore leading white space */` |
|    28529 | 1232 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    15155 | 1233 | `			if( zIn[0] == '\n' ){` |
|    14279 | 1234 | `				nLine++;` |
|     7137 | 1235 | `			}` |
|    15155 | 1236 | `			zIn++;` |
|        5 | 1237 | `		}` |
|        - | 1238 | `		/* Delimit the PHP chunk */` |
|    13379 | 1239 | `		sToken.nLine = nLine;` |
|    13379 | 1240 | `		zCur = zIn;` |
|  1502073 | 1241 | `		while( (sxu32)(zEnd - zIn) >= sCtag.nByte ){` |
|        - | 1242 | `			const char *zPtr;` |
|  1496161 | 1243 | `			if( SyMemcmp(zIn,sCtag.zString,sCtag.nByte) == 0 && iNest < 1 ){` |
|     7465 | 1244 | `				break;` |
|        - | 1245 | `			}` |
|   747316 | 1246 | `			for(;;){` |
|  1494637 | 1247 | `				if( zIn[0] != '/' \|\| (zIn[1] != '*' && zIn[1] != '/') /* && sCtag.nByte >= 2 */ ){` |
|   744353 | 1248 | `					break;` |
|        - | 1249 | `				}` |
|     5941 | 1250 | `				zIn += 2;` |
|     5941 | 1251 | `				if( zIn[-1] == '/' ){` |
|        - | 1252 | `					/* Inline comment */` |
|   224043 | 1253 | `					while( zIn < zEnd && zIn[0] != '\n' ){` |
|   218329 | 1254 | `						zIn++;` |
|        5 | 1255 | `					}` |
|     5719 | 1256 | `					if( zIn >= zEnd ){` |
|      ! 0 | 1257 | `						zIn--;` |
|      ! 0 | 1258 | `					}` |
|     2862 | 1259 | `				}else{` |
|        - | 1260 | `					/* Block comment */` |
|    15219 | 1261 | `					while( (sxu32)(zEnd-zIn) >= sizeof("*/") - 1 ){` |
|    15219 | 1262 | `						if( zIn[0] == '*' && zIn[1] == '/' ){` |
|      227 | 1263 | `							zIn += 2;` |
|      227 | 1264 | `							break;` |
|        - | 1265 | `						}` |
|    14997 | 1266 | `						if( zIn[0] == '\n' ){` |
|       77 | 1267 | `							nLine++;` |
|       36 | 1268 | `						}` |
|    14997 | 1269 | `						zIn++;` |
|        5 | 1270 | `					}` |
|        - | 1271 | `				}` |
|        5 | 1272 | `			}` |
|  1488701 | 1273 | `			if( zIn[0] == '\n' ){` |
|    51269 | 1274 | `				nLine++;` |
|    51269 | 1275 | `				if( iNest > 0 ){` |
|      329 | 1276 | `					zIn++;` |
|      719 | 1277 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|      394 | 1278 | `						zIn++;` |
|        4 | 1279 | `					}` |
|      329 | 1280 | `					zPtr = zIn;` |
|     1645 | 1281 | `					while( zIn < zEnd ){` |
|     1645 | 1282 | `						if( (unsigned char)zIn[0] >= 0xc0 ){` |
|        - | 1283 | `							/* UTF-8 stream */` |
|       19 | 1284 | `							zIn++;` |
|       37 | 1285 | `							SX_JMP_UTF8(zIn,zEnd);` |
|     1632 | 1286 | `						}else if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|      167 | 1287 | `							break;` |
|      ! 0 | 1288 | `						}else{` |
|     1303 | 1289 | `							zIn++;` |
|        - | 1290 | `						}` |
|        5 | 1291 | `					}` |
|      329 | 1292 | `					if( (sxu32)(zIn - zPtr) == sDoc.nByte && SyMemcmp(sDoc.zString,zPtr,sDoc.nByte) == 0 ){` |
|      121 | 1293 | `						iNest = 0;` |
|       58 | 1294 | `					}` |
|      329 | 1295 | `					continue;` |
|        5 | 1296 | `				}` |
|  1462907 | 1297 | `			}else if ( (sxu32)(zEnd - zIn) >= sizeof("<<<") && zIn[0] == '<' && zIn[1] == '<' && zIn[2] == '<' && iNest < 1){` |
|      123 | 1298 | `				zIn += sizeof("<<<")-1;` |
|      135 | 1299 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|       13 | 1300 | `					zIn++;` |
|        1 | 1301 | `				}` |
|      123 | 1302 | `				if( zIn[0] == '"' \|\| zIn[0] == '\'' ){` |
|       51 | 1303 | `					zIn++;` |
|       24 | 1304 | `				}` |
|      123 | 1305 | `				zPtr = zIn;` |
|      565 | 1306 | `				while( zIn < zEnd ){` |
|      565 | 1307 | `					if( (unsigned char)zIn[0] >= 0xc0 ){` |
|        - | 1308 | `						/* UTF-8 stream */` |
|       19 | 1309 | `						zIn++;` |
|       37 | 1310 | `						SX_JMP_UTF8(zIn,zEnd);` |
|      552 | 1311 | `					}else if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       64 | 1312 | `						break;` |
|      ! 0 | 1313 | `					}else{` |
|      429 | 1314 | `						zIn++;` |
|        - | 1315 | `					}` |
|        5 | 1316 | `				}` |
|      123 | 1317 | `				SyStringInitFromBuf(&sDoc,zPtr,zIn-zPtr);` |
|      123 | 1318 | `				SyStringFullTrim(&sDoc);` |
|      123 | 1319 | `				if( sDoc.nByte > 0 ){` |
|      123 | 1320 | `					iNest++;` |
|       59 | 1321 | `				}` |
|      123 | 1322 | `				continue;` |
|        - | 1323 | `			}` |
|  1488259 | 1324 | `			zIn++;` |
|        - | 1325 |  |
|  1488259 | 1326 | `			if ( zIn >= zEnd )` |
|        3 | 1327 | `				break;` |
|        5 | 1328 | `		}` |
|    13379 | 1329 | `		if( (sxu32)(zEnd - zIn) < sCtag.nByte ){` |
|     5919 | 1330 | `			zIn = zEnd;` |
|     2957 | 1331 | `		}` |
|    13379 | 1332 | `		if( zCur < zIn ){` |
|        - | 1333 | `			/* Save the PHP chunk for later processing */` |
|    10361 | 1334 | `			sToken.nType = PH7_TOKEN_PHP;` |
|    10361 | 1335 | `			SyStringInitFromBuf(&sToken.sData,zCur,zIn-zCur);` |
|    20615 | 1336 | `			SyStringRightTrim(&sToken.sData); /* Trim trailing white spaces */` |
|    10361 | 1337 | `			rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|    10361 | 1338 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1339 | `				return rc;` |
|        - | 1340 | `			}` |
|     5178 | 1341 | `		}` |
|    13379 | 1342 | `		if( zIn < zEnd ){` |
|        - | 1343 | `			/* Jump the trailing closing tag */` |
|     7465 | 1344 | `			zIn += sCtag.nByte;` |
|     3730 | 1345 | `		}` |
|        5 | 1346 | `	} /* For(;;) */` |
|        - | 1347 |  |
|    13375 | 1348 | ` 	return SXRET_OK;` |
|     6690 | 1349 | `}` |
|        - | 1350 |  |
