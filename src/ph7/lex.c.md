# src/ph7/lex.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 756/811 lines (93.22%)

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
| 12077476 |   19 | `static sxi32 TokenizePHP(SyStream *pStream,SyToken *pToken,void *pUserData,void *pCtxData)` |
|        5 |   20 | `{` |
|        - |   21 | `	SyString *pStr;` |
|        - |   22 | `	sxi32 rc;` |
|        - |   23 | `	/* Ignore leading white spaces */` |
| 18210159 |   24 | `	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){` |
|        - |   25 | `		/* Advance the stream cursor */` |
|  6132683 |   26 | `		if( pStream->zText[0] == '\n' ){` |
|        - |   27 | `			/* Update line counter */` |
|    42643 |   28 | `			pStream->nLine++;` |
|    21319 |   29 | `		}` |
|  6132683 |   30 | `		pStream->zText++;` |
|        5 |   31 | `	}` |
| 12077481 |   32 | `	if( pStream->zText >= pStream->zEnd ){` |
|        - |   33 | `		/* End of input reached */` |
|      ! 0 |   34 | `		return SXERR_EOF;` |
|        - |   35 | `	}` |
|        - |   36 | `	/* Record token starting position and line */` |
| 12077481 |   37 | `	pToken->nLine = pStream->nLine;` |
| 12077481 |   38 | `	pToken->pUserData = 0;` |
| 12077481 |   39 | `	pStr = &pToken->sData;` |
| 12077481 |   40 | `	SyStringInitFromBuf(pStr,pStream->zText,0);` |
| 14385726 |   41 | `	if( pStream->zText[0] >= 0xc0 \|\| SyisAlpha(pStream->zText[0]) \|\| pStream->zText[0] == '_' ){` |
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
|  4616495 |   52 | `		if( pStream->zText[0] < 0xc0 ){` |
|  4616479 |   53 | `			pStream->zText++;` |
|  2308237 |   54 | `		}` |
|  4552570 |   55 | `		for(;;){` |
|  9105145 |   56 | `			zIn = pStream->zText;` |
|  9105145 |   57 | `			if( zIn[0] >= 0xc0 ){` |
|       49 |   58 | `				zIn++;` |
|        - |   59 | `				/* UTF-8 stream */` |
|      109 |   60 | `				while( zIn < pStream->zEnd && ((zIn[0] & 0xc0) == 0x80) ){` |
|       61 |   61 | `					zIn++;` |
|        1 |   62 | `				}` |
|       24 |   63 | `			}` |
|        - |   64 | `			/* Skip alphanumeric stream */` |
| 39339577 |   65 | `			while( zIn < pStream->zEnd && zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_') ){` |
| 25681867 |   66 | `				zIn++;` |
|        5 |   67 | `			}` |
|  9105145 |   68 | `			if( zIn == pStream->zText ){` |
|        - |   69 | `				/* Not an UTF-8 or alphanumeric stream */` |
|  4616495 |   70 | `				break;` |
|        - |   71 | `			}` |
|        - |   72 | `			/* Synchronize pointers */` |
|  4488655 |   73 | `			pStream->zText = zIn;` |
|        5 |   74 | `		}` |
|        - |   75 | `		/* Record token length */` |
|  4616495 |   76 | `		pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  4616495 |   77 | `		nKeyword = KeywordCode(pStr->zString,(int)pStr->nByte);` |
|        - |   78 | `		/* PHP 7.4: 'fn' is a keyword reserved for arrow functions.` |
|        - |   79 | `		 * The auto-generated perfect hash above doesn't know about it,` |
|        - |   80 | `		 * so intercept the 'fn' identifier here.` |
|        - |   81 | `		 */` |
|  4616490 |   82 | `		if( nKeyword == PH7_TK_ID && pStr->nByte == 2` |
|  1468247 |   83 | `			&& pStr->zString[0] == 'f' && pStr->zString[1] == 'n' ){` |
|      467 |   84 | `			nKeyword = PH7_TKWRD_FN;` |
|      231 |   85 | `		}` |
|  4616495 |   86 | `		if( nKeyword != PH7_TK_ID ){` |
|  1690749 |   87 | `			if( nKeyword &` |
|        - |   88 | `				(PH7_TKWRD_NEW\|PH7_TKWRD_CLONE\|PH7_TKWRD_AND\|PH7_TKWRD_XOR\|PH7_TKWRD_OR\|PH7_TKWRD_INSTANCEOF) ){` |
|        - |   89 | `					/* Alpha stream operators [i.e: new,clone,and,instanceof,or,xor],save the operator instance for later processing */` |
|    25021 |   90 | `					pToken->pUserData = (void *)PH7_ExprExtractOperator(pStr,0);` |
|        - |   91 | `					/* Mark as an operator */` |
|    25021 |   92 | `					pToken->nType = PH7_TK_ID\|PH7_TK_OP;` |
|    12513 |   93 | `			}else{` |
|        - |   94 | `				/* We are dealing with a keyword [i.e: while,foreach,class...],save the keyword ID */` |
|  1665733 |   95 | `				pToken->nType = PH7_TK_KEYWORD;` |
|  1665733 |   96 | `				pToken->pUserData = SX_INT_TO_PTR(nKeyword);` |
|        - |   97 | `			}` |
|   845377 |   98 | `		}else{` |
|        - |   99 | `			/* A simple identifier */` |
|  2925751 |  100 | `			pToken->nType = PH7_TK_ID;` |
|        - |  101 | `		}` |
|  2308250 |  102 | `	}else{` |
|        - |  103 | `		sxi32 c;` |
|        - |  104 | `		/* Non-alpha stream */` |
|  7460991 |  105 | `		if( pStream->zText[0] == '#' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '[' ){` |
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
|  7509618 |  170 | `		}else if( pStream->zText[0] == '#' \|\|` |
|  7460912 |  171 | `			( pStream->zText[0] == '/' &&  &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '/') ){` |
|     5285 |  172 | `				pStream->zText++;` |
|        - |  173 | `				/* Inline comments */` |
|   208449 |  174 | `				while( pStream->zText < pStream->zEnd && pStream->zText[0] != '\n' ){` |
|   203169 |  175 | `					pStream->zText++;` |
|        5 |  176 | `				}` |
|        - |  177 | `				/* Tell the upper-layer to ignore this token */` |
|     5285 |  178 | `				return SXERR_CONTINUE;` |
|  7455643 |  179 | `		}else if( pStream->zText[0] == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '*' ){` |
|    92041 |  180 | `			pStream->zText += 2;` |
|        - |  181 | `			/* Block comment */` |
|  2845489 |  182 | `			while( pStream->zText < pStream->zEnd ){` |
|  2845489 |  183 | `				if( pStream->zText[0] == '*' ){` |
|    92115 |  184 | `					if( &pStream->zText[1] >= pStream->zEnd \|\| pStream->zText[1] == '/'  ){` |
|    46023 |  185 | `						break;` |
|        - |  186 | `					}` |
|       37 |  187 | `				}` |
|  2753453 |  188 | `				if( pStream->zText[0] == '\n' ){` |
|       73 |  189 | `					pStream->nLine++;` |
|       34 |  190 | `				}` |
|  2753453 |  191 | `				pStream->zText++;` |
|        5 |  192 | `			}` |
|    92041 |  193 | `			pStream->zText += 2;` |
|        - |  194 | `			/* Tell the upper-layer to ignore this token */` |
|    92041 |  195 | `			return SXERR_CONTINUE;` |
|  7363607 |  196 | `		}else if( SyisDigit(pStream->zText[0]) ){` |
|   134919 |  197 | `			pStream->zText++;` |
|        - |  198 | `			/* PHP 7.4: handle underscore separator immediately following the first digit.` |
|        - |  199 | `			 * Check pStream->zText < pStream->zEnd BEFORE forming pStream->zText + 1 so` |
|        - |  200 | `			 * we never compute a pointer past one-past-end. */` |
|   134914 |  201 | `			if( pStream->zText < pStream->zEnd` |
|   134914 |  202 | `				&& pStream->zText[0] == '_'` |
|    67537 |  203 | `				&& pStream->zText + 1 < pStream->zEnd` |
|      160 |  204 | `				&& pStream->zText[1] < 0xc0` |
|      165 |  205 | `				&& SyisDigit(pStream->zText[1]) ){` |
|      151 |  206 | `				pStream->zText++; /* swallow underscore between two digits */` |
|       75 |  207 | `			}` |
|        - |  208 | `			/* Decimal digit stream (PHP 7.4: underscore separator allowed between two digits) */` |
|   147387 |  209 | `			while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|    12473 |  210 | `				pStream->zText++;` |
|    12468 |  211 | `				if( pStream->zText < pStream->zEnd` |
|    12468 |  212 | `					&& pStream->zText[0] == '_'` |
|     6320 |  213 | `					&& pStream->zText + 1 < pStream->zEnd` |
|      172 |  214 | `					&& pStream->zText[1] < 0xc0` |
|      177 |  215 | `					&& SyisDigit(pStream->zText[1]) ){` |
|      173 |  216 | `					pStream->zText++; /* swallow underscore between two digits */` |
|       86 |  217 | `				}` |
|        5 |  218 | `			}` |
|        - |  219 | `			/* Mark the token as integer until we encounter a real number */` |
|   134919 |  220 | `			pToken->nType = PH7_TK_INTEGER;` |
|   134919 |  221 | `			if( pStream->zText < pStream->zEnd ){` |
|   134919 |  222 | `				c = pStream->zText[0];` |
|   134919 |  223 | `				if( c == '.' ){` |
|        - |  224 | `					/* Real number (PHP 7.4: underscore separator allowed between two digits) */` |
|      665 |  225 | `					pStream->zText++;` |
|     2325 |  226 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|     1665 |  227 | `						pStream->zText++;` |
|     1660 |  228 | `						if( pStream->zText < pStream->zEnd` |
|     1660 |  229 | `							&& pStream->zText[0] == '_'` |
|      836 |  230 | `							&& pStream->zText + 1 < pStream->zEnd` |
|       12 |  231 | `							&& pStream->zText[1] < 0xc0` |
|       17 |  232 | `							&& SyisDigit(pStream->zText[1]) ){` |
|       13 |  233 | `							pStream->zText++;` |
|        6 |  234 | `						}` |
|        5 |  235 | `					}` |
|      665 |  236 | `					if( pStream->zText < pStream->zEnd ){` |
|      665 |  237 | `						c = pStream->zText[0];` |
|      665 |  238 | `						if( c=='e' \|\| c=='E' ){` |
|       37 |  239 | `							pStream->zText++;` |
|       37 |  240 | `							if( pStream->zText < pStream->zEnd ){` |
|       37 |  241 | `								c = pStream->zText[0];` |
|       36 |  242 | `								if( (c =='+' \|\| c=='-') && &pStream->zText[1] < pStream->zEnd  &&` |
|       17 |  243 | `									pStream->zText[1] < 0xc0 && SyisDigit(pStream->zText[1]) ){` |
|       17 |  244 | `										pStream->zText++;` |
|        8 |  245 | `								}` |
|       95 |  246 | `								while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|       59 |  247 | `									pStream->zText++;` |
|       58 |  248 | `									if( pStream->zText < pStream->zEnd` |
|       58 |  249 | `										&& pStream->zText[0] == '_'` |
|       33 |  250 | `										&& pStream->zText + 1 < pStream->zEnd` |
|        8 |  251 | `										&& pStream->zText[1] < 0xc0` |
|        9 |  252 | `										&& SyisDigit(pStream->zText[1]) ){` |
|        9 |  253 | `										pStream->zText++;` |
|        4 |  254 | `									}` |
|        1 |  255 | `								}` |
|       18 |  256 | `							}` |
|       18 |  257 | `						}` |
|      330 |  258 | `					}` |
|      665 |  259 | `					pToken->nType = PH7_TK_REAL;` |
|   134589 |  260 | `				}else if( c=='e' \|\| c=='E' ){` |
|       29 |  261 | `					SXUNUSED(pUserData); /* Prevent compiler warning */` |
|       29 |  262 | `					SXUNUSED(pCtxData);` |
|       59 |  263 | `					pStream->zText++;` |
|       59 |  264 | `					if( pStream->zText < pStream->zEnd ){` |
|       59 |  265 | `						c = pStream->zText[0];` |
|       58 |  266 | `						if( (c =='+' \|\| c=='-') && &pStream->zText[1] < pStream->zEnd  &&` |
|       17 |  267 | `							pStream->zText[1] < 0xc0 && SyisDigit(pStream->zText[1]) ){` |
|       17 |  268 | `								pStream->zText++;` |
|        8 |  269 | `						}` |
|      167 |  270 | `						while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|      109 |  271 | `							pStream->zText++;` |
|      108 |  272 | `							if( pStream->zText < pStream->zEnd` |
|      108 |  273 | `								&& pStream->zText[0] == '_'` |
|       56 |  274 | `								&& pStream->zText + 1 < pStream->zEnd` |
|        4 |  275 | `								&& pStream->zText[1] < 0xc0` |
|        5 |  276 | `								&& SyisDigit(pStream->zText[1]) ){` |
|        5 |  277 | `								pStream->zText++;` |
|        2 |  278 | `							}` |
|        1 |  279 | `						}` |
|       29 |  280 | `					}` |
|       59 |  281 | `					pToken->nType = PH7_TK_REAL;` |
|   134230 |  282 | `				}else if( c == 'x' \|\| c == 'X' ){` |
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
|   134163 |  295 | `				}else if(c  == 'b' \|\| c == 'B' ){` |
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
|    67457 |  308 | `			}` |
|        - |  309 | `			/* PHP 7.4: absorb a trailing malformed underscore run into the` |
|        - |  310 | `			 * numeric token so the compile phase can emit a PHP-compatible` |
|        - |  311 | `			 * "syntax error, unexpected identifier" parse error. Valid` |
|        - |  312 | `			 * separators were already consumed by the per-loop peek logic` |
|        - |  313 | `			 * above, so an underscore here is always misplaced. */` |
|   134919 |  314 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '_' ){` |
|       18 |  315 | `				pStream->zText++;` |
|       44 |  316 | `				while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0` |
|       49 |  317 | `					&& (SyisAlphaNum(pStream->zText[0]) \|\| pStream->zText[0] == '_') ){` |
|       20 |  318 | `					pStream->zText++;` |
|        4 |  319 | `				}` |
|        7 |  320 | `			}` |
|        - |  321 | `			/* Record token length */` |
|   134919 |  322 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|   134919 |  323 | `			return SXRET_OK;` |
|        - |  324 | `		}` |
|  7228693 |  325 | `		c = pStream->zText[0];` |
|  7228693 |  326 | `		pStream->zText++; /* Advance the stream cursor */` |
|        - |  327 | `		/* Assume we are dealing with an operator*/` |
|  7228693 |  328 | `		pToken->nType = PH7_TK_OP;` |
|  7228693 |  329 | `		switch(c){` |
|  1483491 |  330 | `		case '$': pToken->nType = PH7_TK_DOLLAR; break;` |
|   621573 |  331 | `		case '{': pToken->nType = PH7_TK_OCB;    break;` |
|   621559 |  332 | `		case '}': pToken->nType = PH7_TK_CCB;    break;` |
|  1104761 |  333 | `		case '(': pToken->nType = PH7_TK_LPAREN; break;` |
|    99583 |  334 | `		case '[': pToken->nType \|= PH7_TK_OSB;   break; /* Bitwise operation here,since the square bracket token '['` |
|        - |  335 | `														 * is a potential operator [i.e: subscripting] */` |
|    99589 |  336 | `		case ']': pToken->nType = PH7_TK_CSB;    break;` |
|   552370 |  337 | `		case ')': {` |
|  1104745 |  338 | `			SySet *pTokSet = pStream->pSet;` |
|        - |  339 | `			/* Assemble type cast operators [i.e: (int),(float),(bool)...] */` |
|  1104745 |  340 | `			if( pTokSet->nUsed >= 2 ){` |
|        - |  341 | `				SyToken *pTmp;` |
|        - |  342 | `				/* Peek the last recongnized token */` |
|  1104743 |  343 | `				pTmp = (SyToken *)SySetPeek(pTokSet);` |
|  1104743 |  344 | `				if( pTmp->nType & PH7_TK_KEYWORD ){` |
|     4453 |  345 | `					sxi32 nID = SX_PTR_TO_INT(pTmp->pUserData);` |
|     4453 |  346 | `					if( (sxu32)nID & (PH7_TKWRD_ARRAY\|PH7_TKWRD_INT\|PH7_TKWRD_FLOAT\|PH7_TKWRD_STRING\|PH7_TKWRD_OBJECT\|PH7_TKWRD_BOOL\|PH7_TKWRD_UNSET) ){` |
|     4197 |  347 | `						pTmp = (SyToken *)SySetAt(pTokSet,pTokSet->nUsed - 2);` |
|     4197 |  348 | `						if( pTmp->nType & PH7_TK_LPAREN ){` |
|        - |  349 | `							/* Merge the three tokens '(' 'TYPE' ')' into a single one */` |
|     4113 |  350 | `							const char * zTypeCast = "(int)";` |
|     4113 |  351 | `							if( nID & PH7_TKWRD_FLOAT ){` |
|       13 |  352 | `								zTypeCast = "(float)";` |
|     4107 |  353 | `							}else if( nID & PH7_TKWRD_BOOL ){` |
|        9 |  354 | `								zTypeCast = "(bool)";` |
|     4097 |  355 | `							}else if( nID & PH7_TKWRD_STRING ){` |
|     3859 |  356 | `								zTypeCast = "(string)";` |
|     2166 |  357 | `							}else if( nID & PH7_TKWRD_ARRAY ){` |
|       27 |  358 | `								zTypeCast = "(array)";` |
|      226 |  359 | `							}else if( nID & PH7_TKWRD_OBJECT ){` |
|       35 |  360 | `								zTypeCast = "(object)";` |
|      196 |  361 | `							}else if( nID & PH7_TKWRD_UNSET ){` |
|        3 |  362 | `								zTypeCast = "(unset)";` |
|        1 |  363 | `							}` |
|        - |  364 | `							/* Reflect the change */` |
|     4113 |  365 | `							pToken->nType = PH7_TK_OP;` |
|     4113 |  366 | `							SyStringInitFromBuf(&pToken->sData,zTypeCast,SyStrlen(zTypeCast));` |
|        - |  367 | `							/* Save the instance associated with the type cast operator */` |
|     4113 |  368 | `							pToken->pUserData = (void *)PH7_ExprExtractOperator(&pToken->sData,0);` |
|        - |  369 | `							/* Remove the two previous tokens */` |
|     4113 |  370 | `							pTokSet->nUsed -= 2;` |
|     4113 |  371 | `							return SXRET_OK;` |
|        - |  372 | `						}` |
|       42 |  373 | `					}` |
|      170 |  374 | `				}` |
|   550315 |  375 | `			}` |
|  1100637 |  376 | `			pToken->nType = PH7_TK_RPAREN;` |
|  1100637 |  377 | `			break;` |
|        - |  378 | `				  }` |
|    53526 |  379 | `		case '\'':{` |
|        - |  380 | `			/* Single quoted string */` |
|   107057 |  381 | `			pStr->zString++;` |
|  1210575 |  382 | `			while( pStream->zText < pStream->zEnd ){` |
|  1210575 |  383 | `				if( pStream->zText[0] == '\''  ){` |
|   107067 |  384 | `					if( pStream->zText[-1] != '\\' ){` |
|   107043 |  385 | `						break;` |
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
|  1103523 |  398 | `				if( pStream->zText[0] == '\n' ){` |
|       67 |  399 | `					pStream->nLine++;` |
|       33 |  400 | `				}` |
|  1103523 |  401 | `				pStream->zText++;` |
|        5 |  402 | `			}` |
|        - |  403 | `			/* Record token length and type */` |
|   107057 |  404 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|   107057 |  405 | `			pToken->nType = PH7_TK_SSTR;` |
|        - |  406 | `			/* Jump the trailing single quote */` |
|   107057 |  407 | `			pStream->zText++;` |
|   107057 |  408 | `			return SXRET_OK;` |
|        - |  409 | `				  }` |
|    12389 |  410 | `		case '"':{` |
|        - |  411 | `			sxi32 iNest;` |
|        - |  412 | `			/* Double quoted string */` |
|    24783 |  413 | `			pStr->zString++;` |
|   207511 |  414 | `			while( pStream->zText < pStream->zEnd ){` |
|   207511 |  415 | `				if( pStream->zText[0] == '{' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '$'){` |
|      115 |  416 | `					iNest = 1;` |
|      115 |  417 | `					pStream->zText++;` |
|        - |  418 | `					/* TICKET 1433-40: Hnadle braces'{}' in double quoted string where everything is allowed */` |
|     1183 |  419 | `					while(pStream->zText < pStream->zEnd ){` |
|     1183 |  420 | `						if( pStream->zText[0] == '{' ){` |
|        9 |  421 | `							iNest++;` |
|     1179 |  422 | `						}else if (pStream->zText[0] == '}' ){` |
|      123 |  423 | `							iNest--;` |
|      123 |  424 | `							if( iNest <= 0 ){` |
|      115 |  425 | `								pStream->zText++;` |
|      115 |  426 | `								break;` |
|        1 |  427 | `							}` |
|     1059 |  428 | `						}else if( pStream->zText[0] == '\n' ){` |
|      ! 0 |  429 | `							pStream->nLine++;` |
|      ! 0 |  430 | `						}` |
|     1071 |  431 | `						pStream->zText++;` |
|        3 |  432 | `					}` |
|      115 |  433 | `					if( pStream->zText >= pStream->zEnd ){` |
|      ! 0 |  434 | `						break;` |
|        - |  435 | `					}` |
|       56 |  436 | `				}` |
|   207511 |  437 | `				if( pStream->zText[0] == '"' ){` |
|    25003 |  438 | `					if( pStream->zText[-1] != '\\' ){` |
|    24777 |  439 | `						break;` |
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
|   182733 |  452 | `				if( pStream->zText[0] == '\n' ){` |
|       29 |  453 | `					pStream->nLine++;` |
|       14 |  454 | `				}` |
|   182733 |  455 | `				pStream->zText++;` |
|        5 |  456 | `			}` |
|        - |  457 | `			/* Record token length and type */` |
|    24783 |  458 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|    24783 |  459 | `			pToken->nType = PH7_TK_DSTR;` |
|        - |  460 | `			/* Jump the trailing quote */` |
|    24783 |  461 | `			pStream->zText++;` |
|    24783 |  462 | `			return SXRET_OK;` |
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
|     8055 |  483 | `		case '\\': pToken->nType = PH7_TK_NSSEP;  break;` |
|     2051 |  484 | `		case ':':` |
|     4107 |  485 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == ':' ){` |
|        - |  486 | `				/* Current operator: '::' */` |
|      383 |  487 | `				pStream->zText++;` |
|      194 |  488 | `			}else{` |
|     3729 |  489 | `				pToken->nType = PH7_TK_COLON; /* Single colon */` |
|        - |  490 | `			}` |
|     4107 |  491 | `			break;` |
|   156727 |  492 | `		case ',': pToken->nType \|= PH7_TK_COMMA;  break; /* Comma is also an operator */` |
|   851303 |  493 | `		case ';': pToken->nType = PH7_TK_SEMI;    break;` |
|        - |  494 | `			/* Handle combined operators [i.e: +=,===,!=== ...] */` |
|   225500 |  495 | `		case '=':` |
|   451005 |  496 | `			pToken->nType \|= PH7_TK_EQUAL;` |
|   451005 |  497 | `			if( pStream->zText < pStream->zEnd ){` |
|   451005 |  498 | `				if( pStream->zText[0] == '=' ){` |
|    24705 |  499 | `					pToken->nType &= ~PH7_TK_EQUAL;` |
|        - |  500 | `					/* Current operator: == */` |
|    24705 |  501 | `					pStream->zText++;` |
|    24705 |  502 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  503 | `						/* Current operator: === */` |
|     5241 |  504 | `						pStream->zText++;` |
|     2623 |  505 | `					}` |
|   438655 |  506 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  507 | `					/* Array operator: => */` |
|     6169 |  508 | `					pToken->nType = PH7_TK_ARRAY_OP;` |
|     6169 |  509 | `					pStream->zText++;` |
|     3087 |  510 | `				}else{` |
|        - |  511 | `					/* TICKET 1433-0010: Reference operator '=&' */` |
|   420141 |  512 | `					const unsigned char *zCur = pStream->zText;` |
|   420141 |  513 | `					sxu32 nLine = 0;` |
|   840107 |  514 | `					while( zCur < pStream->zEnd && zCur[0] < 0xc0 && SyisSpace(zCur[0]) ){` |
|   419971 |  515 | `						if( zCur[0] == '\n' ){` |
|        5 |  516 | `							nLine++;` |
|        2 |  517 | `						}` |
|   419971 |  518 | `						zCur++;` |
|        5 |  519 | `					}` |
|   420141 |  520 | `					if( zCur < pStream->zEnd && zCur[0] == '&' ){` |
|        - |  521 | `						/* Current operator: =& */` |
|       59 |  522 | `						pToken->nType &= ~PH7_TK_EQUAL;` |
|       59 |  523 | `						SyStringInitFromBuf(pStr,"=&",sizeof("=&")-1);` |
|        - |  524 | `						/* Update token stream */` |
|       59 |  525 | `						pStream->zText = &zCur[1];` |
|       59 |  526 | `						pStream->nLine += nLine;` |
|       28 |  527 | `					}` |
|        - |  528 | `				}` |
|   225500 |  529 | `			}` |
|   451005 |  530 | `			break;` |
|    27215 |  531 | `		case '!':` |
|    54435 |  532 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  533 | `				/* Current operator: != */` |
|    23163 |  534 | `				pStream->zText++;` |
|    23163 |  535 | `				if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  536 | `					/* Current operator: !== */` |
|    19303 |  537 | `					pStream->zText++;` |
|     9649 |  538 | `				}` |
|    11579 |  539 | `			}` |
|    54435 |  540 | `			break;` |
|    15630 |  541 | `		case '&':` |
|    31265 |  542 | `			pToken->nType \|= PH7_TK_AMPER;` |
|    31265 |  543 | `			if( pStream->zText < pStream->zEnd ){` |
|    31265 |  544 | `				if( pStream->zText[0] == '&' ){` |
|    11971 |  545 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|        - |  546 | `					/* Current operator: && */` |
|    11971 |  547 | `					pStream->zText++;` |
|    25282 |  548 | `				}else if( pStream->zText[0] == '=' ){` |
|        7 |  549 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|        - |  550 | `					/* Current operator: &= */` |
|        7 |  551 | `					pStream->zText++;` |
|        3 |  552 | `				}` |
|    15630 |  553 | `			}` |
|    31265 |  554 | `			break;` |
|     2098 |  555 | `		case '\|':` |
|     4201 |  556 | `			if( pStream->zText < pStream->zEnd ){` |
|     4201 |  557 | `				if( pStream->zText[0] == '\|' ){` |
|        - |  558 | `					/* Current operator: \|\| */` |
|     3999 |  559 | `					pStream->zText++;` |
|     2204 |  560 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  561 | `					/* Current operator: \|= */` |
|        7 |  562 | `					pStream->zText++;` |
|        3 |  563 | `				}` |
|     2098 |  564 | `			}` |
|     4201 |  565 | `			break;` |
|    10069 |  566 | `		case '+':` |
|    20143 |  567 | `			if( pStream->zText < pStream->zEnd ){` |
|    20141 |  568 | `				if( pStream->zText[0] == '+' ){` |
|        - |  569 | `					/* Current operator: ++ */` |
|    15685 |  570 | `					pStream->zText++;` |
|    12301 |  571 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  572 | `					/* Current operator: += */` |
|       59 |  573 | `					pStream->zText++;` |
|       27 |  574 | `				}` |
|    10068 |  575 | `			}` |
|    20143 |  576 | `			break;` |
|    95575 |  577 | `		case '-':` |
|   191155 |  578 | `			if( pStream->zText < pStream->zEnd ){` |
|   191155 |  579 | `				if( pStream->zText[0] == '-' ){` |
|        - |  580 | `					/* Current operator: -- */` |
|       37 |  581 | `					pStream->zText++;` |
|   191138 |  582 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  583 | `					/* Current operator: -= */` |
|       10 |  584 | `					pStream->zText++;` |
|   191117 |  585 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  586 | `					/* Current operator: -> */` |
|   190431 |  587 | `					pStream->zText++;` |
|    95213 |  588 | `				}` |
|    95575 |  589 | `			}` |
|   191155 |  590 | `			break;` |
|      180 |  591 | `		case '*':` |
|      365 |  592 | `			if( pStream->zText < pStream->zEnd ){` |
|      365 |  593 | `				if( pStream->zText[0] == '*' ){` |
|        - |  594 | `					/* Current operator: ** or **= */` |
|      135 |  595 | `					pStream->zText++;` |
|      135 |  596 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  597 | `						/* Current operator: **= */` |
|       23 |  598 | `						pStream->zText++;` |
|       12 |  599 | `					}` |
|      298 |  600 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  601 | `					/* Current operator: *= */` |
|       20 |  602 | `					pStream->zText++;` |
|        9 |  603 | `				}` |
|      180 |  604 | `			}` |
|      365 |  605 | `			break;` |
|       40 |  606 | `		case '/':` |
|       82 |  607 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  608 | `				/* Current operator: /= */` |
|        5 |  609 | `				pStream->zText++;` |
|        2 |  610 | `			}` |
|       82 |  611 | `			break;` |
|       34 |  612 | `		case '%':` |
|       73 |  613 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  614 | `				/* Current operator: %= */` |
|        3 |  615 | `				pStream->zText++;` |
|        1 |  616 | `			}` |
|       73 |  617 | `			break;` |
|       11 |  618 | `		case '^':` |
|       23 |  619 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  620 | `				/* Current operator: ^= */` |
|        9 |  621 | `				pStream->zText++;` |
|        4 |  622 | `			}` |
|       23 |  623 | `			break;` |
|    55929 |  624 | `		case '.':` |
|   111863 |  625 | `			if( pStream->zText + 1 < pStream->zEnd && pStream->zText[0] == '.' && pStream->zText[1] == '.' ){` |
|        - |  626 | `				/* Ellipsis: ... */` |
|     7863 |  627 | `				pStream->zText += 2;` |
|     7863 |  628 | `				pToken->nType = PH7_TK_ELLIPSIS;` |
|   107934 |  629 | `			}else if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  630 | `				/* Current operator: .= */` |
|     3961 |  631 | `				pStream->zText++;` |
|     1978 |  632 | `			}` |
|   111863 |  633 | `			break;` |
|    32827 |  634 | `		case '<':` |
|    65659 |  635 | `			if( pStream->zText < pStream->zEnd ){` |
|    65659 |  636 | `				if( pStream->zText[0] == '<' ){` |
|        - |  637 | `					/* Current operator: << */` |
|      145 |  638 | `					pStream->zText++;` |
|      145 |  639 | `					if( pStream->zText < pStream->zEnd ){` |
|      145 |  640 | `						if( pStream->zText[0] == '=' ){` |
|        - |  641 | `							/* Current operator: <<= */` |
|        9 |  642 | `							pStream->zText++;` |
|      141 |  643 | `						}else if( pStream->zText[0] == '<' ){` |
|        - |  644 | `							/* Current Token: <<<  */` |
|      123 |  645 | `							pStream->zText++;` |
|        - |  646 | `							/* This may be the beginning of a Heredoc/Nowdoc string,try to delimit it */` |
|      123 |  647 | `							rc = LexExtractHeredoc(&(*pStream),&(*pToken));` |
|      123 |  648 | `							if( rc == SXRET_OK ){` |
|        - |  649 | `								/* Here/Now doc successfuly extracted */` |
|      123 |  650 | `								return SXRET_OK;` |
|        - |  651 | `							}` |
|      ! 0 |  652 | `						}` |
|       12 |  653 | `					}` |
|    65530 |  654 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  655 | `					/* Current operator: <> */` |
|        5 |  656 | `					pStream->zText++;` |
|    65517 |  657 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  658 | `					/* Current operator: <= or <=> */` |
|      113 |  659 | `					pStream->zText++;` |
|      113 |  660 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '>' ){` |
|        - |  661 | `						/* Current operator: <=> */` |
|       54 |  662 | `						pStream->zText++;` |
|       26 |  663 | `					}` |
|       54 |  664 | `				}` |
|    32768 |  665 | `			}` |
|    65541 |  666 | `			break;` |
|     3963 |  667 | `		case '>':` |
|     7931 |  668 | `			if( pStream->zText < pStream->zEnd ){` |
|     7931 |  669 | `				if( pStream->zText[0] == '>' ){` |
|        - |  670 | `					/* Current operator: >> */` |
|       21 |  671 | `					pStream->zText++;` |
|       21 |  672 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  673 | `						/* Current operator: >>= */` |
|       11 |  674 | `						pStream->zText++;` |
|        6 |  675 | `					}` |
|     7921 |  676 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  677 | `					/* Current operator: >= */` |
|       91 |  678 | `					pStream->zText++;` |
|       43 |  679 | `				}` |
|     3963 |  680 | `			}` |
|     7931 |  681 | `			break;` |
|     1522 |  682 | `		case '?':` |
|     3049 |  683 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '?' ){` |
|        - |  684 | `				/* Null coalescing operator: ?? */` |
|      197 |  685 | `				pStream->zText++;` |
|      197 |  686 | `				if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  687 | `					/* Null coalescing assignment operator (PHP 7.4) */` |
|       73 |  688 | `					pStream->zText++;` |
|       34 |  689 | `				}` |
|     2953 |  690 | `			}else if( (pStream->zEnd - pStream->zText) >= 2` |
|     2857 |  691 | `				&& pStream->zText[0] == '-' && pStream->zText[1] == '>' ){` |
|        - |  692 | `				/* Nullsafe object operator (PHP 8.0): ?-> */` |
|      117 |  693 | `				pStream->zText += 2;` |
|       56 |  694 | `			}` |
|     3044 |  695 | `			break;` |
|      115 |  696 | `		default:` |
|      230 |  697 | `			break;` |
|        - |  698 | `		}` |
|  7092633 |  699 | `		if( pStr->nByte <= 0 ){` |
|        - |  700 | `			/* Record token length */` |
|  7092577 |  701 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  3546286 |  702 | `		}` |
|  7092633 |  703 | `		if( pToken->nType & PH7_TK_OP ){` |
|        - |  704 | `			const ph7_expr_op *pOp;` |
|        - |  705 | `			/* Check if the extracted token is an operator */` |
|  1183959 |  706 | `			pOp = PH7_ExprExtractOperator(pStr,(SyToken *)SySetPeek(pStream->pSet));` |
|  1183959 |  707 | `			if( pOp == 0 ){` |
|        - |  708 | `				/* Not an operator */` |
|      ! 0 |  709 | `				pToken->nType &= ~PH7_TK_OP;` |
|      ! 0 |  710 | `				if( pToken->nType <= 0 ){` |
|      ! 0 |  711 | `					pToken->nType = PH7_TK_OTHER;` |
|      ! 0 |  712 | `				}` |
|      ! 0 |  713 | `			}else{` |
|        - |  714 | `				/* Save the instance associated with this operator for later processing */` |
|  1183959 |  715 | `				pToken->pUserData = (void *)pOp;` |
|        - |  716 | `			}` |
|   591977 |  717 | `		}` |
|        - |  718 | `	}` |
|        - |  719 | `	/* Tell the upper-layer to save the extracted token for later processing */` |
| 11709123 |  720 | `	return SXRET_OK;` |
|  6038743 |  721 | `}` |
|        - |  722 | `/* SPDX-SnippetBegin */` |
|        - |  723 | `/* SPDX-SnippetCopyrightText: SQLite mkkeywordhash.c (D. Richard Hipp and the SQLite authors <https://sqlite.org/>); adapted for the PH7 engine by Chems mrad */` |
|        - |  724 | `/* SPDX-License-Identifier: blessing */` |
|        - |  725 | `/***** This file contains automatically generated code ******` |
|        - |  726 | `**` |
|        - |  727 | `** The code in this file has been automatically generated by` |
|        - |  728 | `**` |
|        - |  729 | `**     $Header: /sqlite/sqlite/tool/mkkeywordhash.c` |
|        - |  730 | `**` |
|        - |  731 | `** Sligthly modified by Chems mrad <chm@symisc.net> for the PH7 engine.` |
|        - |  732 | `**` |
|        - |  733 | `** The code in this file implements a function that determines whether` |
|        - |  734 | `** or not a given identifier is really a PHP keyword.  The same thing` |
|        - |  735 | `** might be implemented more directly using a hand-written hash table.` |
|        - |  736 | `** But by using this automatically generated code, the size of the code` |
|        - |  737 | `** is substantially reduced.  This is important for embedded applications` |
|        - |  738 | `** on platforms with limited memory.` |
|        - |  739 | `*/` |
|        - |  740 | `/* Hash score: 103 */` |
|  4616495 |  741 | `static sxu32 KeywordCode(const char *z, int n){` |
|        - |  742 | `  /* zText[] encodes 532 bytes of keywords in 333 bytes */` |
|        - |  743 | `  /*   extendswitchprintegerequire_oncenddeclareturnamespacechobject      */` |
|        - |  744 | `  /*   hrowbooleandefaultrycaselfinalistaticlonewconstringlobaluse        */` |
|        - |  745 | `  /*   lseifloatvarrayANDIEchoUSECHOabstractclasscontinuendifunction      */` |
|        - |  746 | `  /*   diendwhilevaldoexitgotoimplementsinclude_oncemptyinstanceof        */` |
|        - |  747 | `  /*   interfacendforeachissetparentprivateprotectedpublicatchunset       */` |
|        - |  748 | `  /*   xorARRAYASArrayEXITUNSETXORbreak                                   */` |
|        - |  749 | `  static const char zText[332] = {` |
|        - |  750 | `    'e','x','t','e','n','d','s','w','i','t','c','h','p','r','i','n','t','e',` |
|        - |  751 | `    'g','e','r','e','q','u','i','r','e','_','o','n','c','e','n','d','d','e',` |
|        - |  752 | `    'c','l','a','r','e','t','u','r','n','a','m','e','s','p','a','c','e','c',` |
|        - |  753 | `    'h','o','b','j','e','c','t','h','r','o','w','b','o','o','l','e','a','n',` |
|        - |  754 | `    'd','e','f','a','u','l','t','r','y','c','a','s','e','l','f','i','n','a',` |
|        - |  755 | `    'l','i','s','t','a','t','i','c','l','o','n','e','w','c','o','n','s','t',` |
|        - |  756 | `    'r','i','n','g','l','o','b','a','l','u','s','e','l','s','e','i','f','l',` |
|        - |  757 | `    'o','a','t','v','a','r','r','a','y','A','N','D','I','E','c','h','o','U',` |
|        - |  758 | `    'S','E','C','H','O','a','b','s','t','r','a','c','t','c','l','a','s','s',` |
|        - |  759 | `    'c','o','n','t','i','n','u','e','n','d','i','f','u','n','c','t','i','o',` |
|        - |  760 | `    'n','d','i','e','n','d','w','h','i','l','e','v','a','l','d','o','e','x',` |
|        - |  761 | `    'i','t','g','o','t','o','i','m','p','l','e','m','e','n','t','s','i','n',` |
|        - |  762 | `    'c','l','u','d','e','_','o','n','c','e','m','p','t','y','i','n','s','t',` |
|        - |  763 | `    'a','n','c','e','o','f','i','n','t','e','r','f','a','c','e','n','d','f',` |
|        - |  764 | `    'o','r','e','a','c','h','i','s','s','e','t','p','a','r','e','n','t','p',` |
|        - |  765 | `    'r','i','v','a','t','e','p','r','o','t','e','c','t','e','d','p','u','b',` |
|        - |  766 | `    'l','i','c','a','t','c','h','u','n','s','e','t','x','o','r','A','R','R',` |
|        - |  767 | `    'A','Y','A','S','A','r','r','a','y','E','X','I','T','U','N','S','E','T',` |
|        - |  768 | `    'X','O','R','b','r','e','a','k'` |
|        - |  769 | `  };` |
|        - |  770 | `  static const unsigned char aHash[151] = {` |
|        - |  771 |  |
|        - |  772 |  |
|        - |  773 | `      74,   0,  51,   0,  76,   0,   0,  20,   0,   0,   0,  50,   0,` |
|        - |  774 | `      80,  34,   0,  36,   0,   0,  64,  16,   0,   0,  17,   0,   1,` |
|        - |  775 | `      19,  84,  66,   0,  43,  45,  78,   0,   0,  53,  56,   0,   0,` |
|        - |  776 |  |
|        - |  777 | `      72,  14,   0,  71,   0,  38,   6,   0,   0,   0,  73,   0,   0,` |
|        - |  778 | `       3,   0,  41,   5,  52,  57,  32,   0,  60,  63,   0,  69,  82,` |
|        - |  779 | `      30,   0,  79,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,` |
|        - |  780 |  |
|        - |  781 | `      62,   0,  11,   0,   0,  58,   0,   0,   0,   0,  59,  75,   0,` |
|        - |  782 |  |
|        - |  783 | `  };` |
|        - |  784 | `  static const unsigned char aNext[84] = {` |
|        - |  785 |  |
|        - |  786 |  |
|        - |  787 |  |
|        - |  788 |  |
|        - |  789 |  |
|        - |  790 |  |
|        - |  791 | `      42,   0,   0,   0,  70,  55` |
|        - |  792 | `  };` |
|        - |  793 | `  static const unsigned char aLen[84] = {` |
|        - |  794 | `       7,   9,   6,   5,   7,  12,   7,   2,  10,   7,   6,   9,   4,` |
|        - |  795 | `       6,   5,   7,   4,   3,   7,   3,   4,   4,   5,   4,   6,   5,` |
|        - |  796 | `       2,   3,   5,   6,   6,   3,   6,   4,   2,   5,   3,   5,   3,` |
|        - |  797 | `       3,   4,   3,   4,   8,   5,   2,   8,   5,   8,   3,   8,   5,` |
|        - |  798 | `       4,   2,   4,   4,  10,  12,   7,   5,  10,   9,   3,   6,  10,` |
|        - |  799 | `       3,   7,   2,   5,   6,   7,   9,   6,   5,   5,   3,   5,   2,` |
|        - |  800 | `       5,   4,   5,   3,   2,   5` |
|        - |  801 | `  };` |
|        - |  802 | `  static const sxu16 aOffset[84] = {` |
|        - |  803 |  |
|        - |  804 | `      55,  60,  65,  65,  70,  72,  78,  81,  83,  86,  90,  92,  97,` |
|        - |  805 | `     100, 100, 103, 106, 111, 117, 119, 119, 123, 124, 129, 130, 135,` |
|        - |  806 | `     137, 139, 143, 145, 149, 157, 159, 162, 169, 173, 181, 183, 186,` |
|        - |  807 | `     190, 194, 196, 200, 204, 214, 214, 225, 230, 240, 240, 248, 248,` |
|        - |  808 | `     251, 251, 252, 258, 263, 269, 276, 285, 290, 295, 300, 303, 308,` |
|        - |  809 | `     310, 315, 319, 324, 325, 327` |
|        - |  810 | `  };` |
|        - |  811 | `  static const sxu32 aCode[84] = {` |
|        - |  812 | `    PH7_TKWRD_EXTENDS,   PH7_TKWRD_ENDSWITCH,   PH7_TKWRD_SWITCH,    PH7_TKWRD_PRINT,   PH7_TKWRD_INT,` |
|        - |  813 | `    PH7_TKWRD_REQONCE,   PH7_TKWRD_REQUIRE,     PH7_TK_ID /* 'eq' PH7-ism removed */, PH7_TKWRD_ENDDEC, PH7_TKWRD_DECLARE,` |
|        - |  814 | `    PH7_TKWRD_RETURN,    PH7_TKWRD_NAMESPACE,   PH7_TKWRD_ECHO,      PH7_TKWRD_OBJECT,    PH7_TKWRD_THROW,` |
|        - |  815 | `    PH7_TKWRD_BOOL,      PH7_TKWRD_BOOL,        PH7_TKWRD_AND,       PH7_TKWRD_DEFAULT,   PH7_TKWRD_TRY,` |
|        - |  816 | `    PH7_TKWRD_CASE,      PH7_TKWRD_SELF,        PH7_TKWRD_FINAL,     PH7_TKWRD_LIST,      PH7_TKWRD_STATIC,` |
|        - |  817 | `    PH7_TKWRD_CLONE,     PH7_TK_ID /* 'ne' PH7-ism removed */, PH7_TKWRD_NEW,  PH7_TKWRD_CONST,     PH7_TKWRD_STRING,` |
|        - |  818 | `    PH7_TKWRD_GLOBAL,    PH7_TKWRD_USE,         PH7_TKWRD_ELIF,      PH7_TKWRD_ELSE,      PH7_TKWRD_IF,` |
|        - |  819 | `    PH7_TKWRD_FLOAT,     PH7_TKWRD_VAR,         PH7_TKWRD_ARRAY,     PH7_TKWRD_AND,       PH7_TKWRD_DIE,` |
|        - |  820 | `    PH7_TKWRD_ECHO,      PH7_TKWRD_USE,         PH7_TKWRD_ECHO,      PH7_TKWRD_ABSTRACT,  PH7_TKWRD_CLASS,` |
|        - |  821 | `    PH7_TKWRD_AS,        PH7_TKWRD_CONTINUE,    PH7_TKWRD_ENDIF,     PH7_TKWRD_FUNCTION,  PH7_TKWRD_DIE,` |
|        - |  822 | `    PH7_TKWRD_ENDWHILE,  PH7_TKWRD_WHILE,       PH7_TKWRD_EVAL,      PH7_TKWRD_DO,        PH7_TKWRD_EXIT,` |
|        - |  823 | `    PH7_TKWRD_GOTO,      PH7_TKWRD_IMPLEMENTS,  PH7_TKWRD_INCONCE,   PH7_TKWRD_INCLUDE,   PH7_TKWRD_EMPTY,` |
|        - |  824 | `    PH7_TKWRD_INSTANCEOF,PH7_TKWRD_INTERFACE,   PH7_TKWRD_INT,       PH7_TKWRD_ENDFOR,    PH7_TKWRD_END4EACH,` |
|        - |  825 | `    PH7_TKWRD_FOR,       PH7_TKWRD_FOREACH,     PH7_TKWRD_OR,        PH7_TKWRD_ISSET,     PH7_TKWRD_PARENT,` |
|        - |  826 | `    PH7_TKWRD_PRIVATE,   PH7_TKWRD_PROTECTED,   PH7_TKWRD_PUBLIC,    PH7_TKWRD_CATCH,     PH7_TKWRD_UNSET,` |
|        - |  827 | `    PH7_TKWRD_XOR,       PH7_TKWRD_ARRAY,       PH7_TKWRD_AS,        PH7_TKWRD_ARRAY,     PH7_TKWRD_EXIT,` |
|        - |  828 | `    PH7_TKWRD_UNSET,     PH7_TKWRD_XOR,         PH7_TKWRD_OR,        PH7_TKWRD_BREAK` |
|        - |  829 | `  };` |
|        - |  830 | `  int h, i;` |
|  4616495 |  831 | `  if( n<2 ) return PH7_TK_ID;` |
|  4488633 |  832 | `  h = (((int)z[0]*4) ^ ((int)z[n-1]*3) ^ n) % 151;` |
|  6733413 |  833 | `  for(i=((int)aHash[h])-1; i>=0; i=((int)aNext[i])-1){` |
|  3934555 |  834 | `    if( (int)aLen[i]==n && SyMemcmp(&zText[aOffset[i]],z,n)==0 ){` |
|        - |  835 | `       /* PH7_TKWRD_EXTENDS */` |
|        - |  836 | `       /* PH7_TKWRD_ENDSWITCH */` |
|        - |  837 | `       /* PH7_TKWRD_SWITCH */` |
|        - |  838 | `       /* PH7_TKWRD_PRINT */` |
|        - |  839 | `       /* PH7_TKWRD_INT */` |
|        - |  840 | `       /* PH7_TKWRD_REQONCE */` |
|        - |  841 | `       /* PH7_TKWRD_REQUIRE */` |
|        - |  842 | `       /* PH7_TK_ID */` |
|        - |  843 | `       /* PH7_TKWRD_ENDDEC */` |
|        - |  844 | `       /* PH7_TKWRD_DECLARE */` |
|        - |  845 | `       /* PH7_TKWRD_RETURN */` |
|        - |  846 | `       /* PH7_TKWRD_NAMESPACE */` |
|        - |  847 | `       /* PH7_TKWRD_ECHO */` |
|        - |  848 | `       /* PH7_TKWRD_OBJECT */` |
|        - |  849 | `       /* PH7_TKWRD_THROW */` |
|        - |  850 | `       /* PH7_TKWRD_BOOL */` |
|        - |  851 | `       /* PH7_TKWRD_BOOL */` |
|        - |  852 | `       /* PH7_TKWRD_AND */` |
|        - |  853 | `       /* PH7_TKWRD_DEFAULT */` |
|        - |  854 | `       /* PH7_TKWRD_TRY */` |
|        - |  855 | `       /* PH7_TKWRD_CASE */` |
|        - |  856 | `       /* PH7_TKWRD_SELF */` |
|        - |  857 | `       /* PH7_TKWRD_FINAL */` |
|        - |  858 | `       /* PH7_TKWRD_LIST */` |
|        - |  859 | `       /* PH7_TKWRD_STATIC */` |
|        - |  860 | `       /* PH7_TKWRD_CLONE */` |
|        - |  861 | `       /* PH7_TK_ID */` |
|        - |  862 | `       /* PH7_TKWRD_NEW */` |
|        - |  863 | `       /* PH7_TKWRD_CONST */` |
|        - |  864 | `       /* PH7_TKWRD_STRING */` |
|        - |  865 | `       /* PH7_TKWRD_GLOBAL */` |
|        - |  866 | `       /* PH7_TKWRD_USE */` |
|        - |  867 | `       /* PH7_TKWRD_ELIF */` |
|        - |  868 | `       /* PH7_TKWRD_ELSE */` |
|        - |  869 | `       /* PH7_TKWRD_IF */` |
|        - |  870 | `       /* PH7_TKWRD_FLOAT */` |
|        - |  871 | `       /* PH7_TKWRD_VAR */` |
|        - |  872 | `       /* PH7_TKWRD_ARRAY */` |
|        - |  873 | `       /* PH7_TKWRD_AND */` |
|        - |  874 | `       /* PH7_TKWRD_DIE */` |
|        - |  875 | `       /* PH7_TKWRD_ECHO */` |
|        - |  876 | `       /* PH7_TKWRD_USE */` |
|        - |  877 | `       /* PH7_TKWRD_ECHO */` |
|        - |  878 | `       /* PH7_TKWRD_ABSTRACT */` |
|        - |  879 | `       /* PH7_TKWRD_CLASS */` |
|        - |  880 | `       /* PH7_TKWRD_AS */` |
|        - |  881 | `       /* PH7_TKWRD_CONTINUE */` |
|        - |  882 | `       /* PH7_TKWRD_ENDIF */` |
|        - |  883 | `       /* PH7_TKWRD_FUNCTION */` |
|        - |  884 | `       /* PH7_TKWRD_DIE */` |
|        - |  885 | `       /* PH7_TKWRD_ENDWHILE */` |
|        - |  886 | `       /* PH7_TKWRD_WHILE */` |
|        - |  887 | `       /* PH7_TKWRD_EVAL */` |
|        - |  888 | `       /* PH7_TKWRD_DO */` |
|        - |  889 | `       /* PH7_TKWRD_EXIT */` |
|        - |  890 | `       /* PH7_TKWRD_GOTO */` |
|        - |  891 | `       /* PH7_TKWRD_IMPLEMENTS */` |
|        - |  892 | `       /* PH7_TKWRD_INCONCE */` |
|        - |  893 | `       /* PH7_TKWRD_INCLUDE */` |
|        - |  894 | `       /* PH7_TKWRD_EMPTY */` |
|        - |  895 | `       /* PH7_TKWRD_INSTANCEOF */` |
|        - |  896 | `       /* PH7_TKWRD_INTERFACE */` |
|        - |  897 | `       /* PH7_TKWRD_INT */` |
|        - |  898 | `       /* PH7_TKWRD_ENDFOR */` |
|        - |  899 | `       /* PH7_TKWRD_END4EACH */` |
|        - |  900 | `       /* PH7_TKWRD_FOR */` |
|        - |  901 | `       /* PH7_TKWRD_FOREACH */` |
|        - |  902 | `       /* PH7_TKWRD_OR */` |
|        - |  903 | `       /* PH7_TKWRD_ISSET */` |
|        - |  904 | `       /* PH7_TKWRD_PARENT */` |
|        - |  905 | `       /* PH7_TKWRD_PRIVATE */` |
|        - |  906 | `       /* PH7_TKWRD_PROTECTED */` |
|        - |  907 | `       /* PH7_TKWRD_PUBLIC */` |
|        - |  908 | `       /* PH7_TKWRD_CATCH */` |
|        - |  909 | `       /* PH7_TKWRD_UNSET */` |
|        - |  910 | `       /* PH7_TKWRD_XOR */` |
|        - |  911 | `       /* PH7_TKWRD_ARRAY */` |
|        - |  912 | `       /* PH7_TKWRD_AS */` |
|        - |  913 | `       /* PH7_TKWRD_ARRAY */` |
|        - |  914 | `       /* PH7_TKWRD_EXIT */` |
|        - |  915 | `       /* PH7_TKWRD_UNSET */` |
|        - |  916 | `       /* PH7_TKWRD_XOR */` |
|        - |  917 | `       /* PH7_TKWRD_OR */` |
|        - |  918 | `       /* PH7_TKWRD_BREAK */` |
|  1689775 |  919 | `      return aCode[i];` |
|        - |  920 | `    }` |
|  1122393 |  921 | `  }` |
|        - |  922 | `  /* Linear fallback for keywords not in the auto-generated hash table */` |
|  2798863 |  923 | `  if( n==5 && SyMemcmp(z,"trait",5)==0 ) return PH7_TKWRD_TRAIT;` |
|  2798797 |  924 | `  if( n==9 && SyMemcmp(z,"insteadof",9)==0 ) return PH7_TKWRD_INSTEADOF;` |
|  2798793 |  925 | `  if( n==7 && SyMemcmp(z,"finally",7)==0 ) return PH7_TKWRD_FINALLY;` |
|  2798645 |  926 | `  if( n==5 && SyMemcmp(z,"yield",5)==0 ) return PH7_TKWRD_YIELD;` |
|  2798375 |  927 | `  if( n==5 && SyMemcmp(z,"match",5)==0 ) return PH7_TKWRD_MATCH;` |
|  2798301 |  928 | `  return PH7_TK_ID;` |
|  2308250 |  929 | `}` |
|        - |  930 | `/* --- End of Automatically generated code --- */` |
|        - |  931 | `/* SPDX-SnippetEnd */` |
|        - |  932 | `/*` |
|        - |  933 | ` * Extract a heredoc/nowdoc text from a raw PHP input.` |
|        - |  934 | ` * According to the PHP language reference manual:` |
|        - |  935 | ` *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier` |
|        - |  936 | ` *  is provided, then a newline. The string itself follows, and then the same identifier again` |
|        - |  937 | ` *  to close the quotation.` |
|        - |  938 | ` *  The closing identifier must begin in the first column of the line. Also, the identifier must` |
|        - |  939 | ` *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric` |
|        - |  940 | ` *  characters and underscores, and must start with a non-digit character or underscore.` |
|        - |  941 | ` *  Heredoc text behaves just like a double-quoted string, without the double quotes.` |
|        - |  942 | ` *  This means that quotes in a heredoc do not need to be escaped, but the escape codes listed` |
|        - |  943 | ` *  above can still be used. Variables are expanded, but the same care must be taken when expressing` |
|        - |  944 | ` *  complex variables inside a heredoc as with strings.` |
|        - |  945 | ` *  Nowdocs are to single-quoted strings what heredocs are to double-quoted strings.` |
|        - |  946 | ` *  A nowdoc is specified similarly to a heredoc, but no parsing is done inside a nowdoc.` |
|        - |  947 | ` *  The construct is ideal for embedding PHP code or other large blocks of text without the need` |
|        - |  948 | ` *  for escaping. It shares some features in common with the SGML <![CDATA[ ]]> construct, in that` |
|        - |  949 | ` *  it declares a block of text which is not for parsing.` |
|        - |  950 | ` *  A nowdoc is identified with the same <<< sequence used for heredocs, but the identifier which follows` |
|        - |  951 | ` *  is enclosed in single quotes, e.g. <<<'EOT'. All the rules for heredoc identifiers also apply to nowdoc` |
|        - |  952 | ` *  identifiers, especially those regarding the appearance of the closing identifier.` |
|        - |  953 | ` * Symisc Extension:` |
|        - |  954 | ` * The closing delimiter can now start with a digit or undersocre or it can be an UTF-8 stream.` |
|        - |  955 | ` * Example:` |
|        - |  956 | ` *  <<<123` |
|        - |  957 | ` *    HEREDOC Here` |
|        - |  958 | ` * 123` |
|        - |  959 | ` *  or` |
|        - |  960 | ` *  <<<___` |
|        - |  961 | ` *   HEREDOC Here` |
|        - |  962 | ` *  ___` |
|        - |  963 | ` */` |
|      118 |  964 | `static sxi32 LexExtractHeredoc(SyStream *pStream,SyToken *pToken)` |
|        5 |  965 | `{` |
|      123 |  966 | `	const unsigned char *zIn  = pStream->zText;` |
|      123 |  967 | `	const unsigned char *zEnd = pStream->zEnd;` |
|        - |  968 | `	const unsigned char *zPtr;` |
|      123 |  969 | `	sxu8 bNowDoc = FALSE;` |
|        - |  970 | `	SyString sDelim;` |
|        - |  971 | `	SyString sStr;` |
|        - |  972 | `	/* Jump leading white spaces */` |
|      135 |  973 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|       13 |  974 | `		zIn++;` |
|        1 |  975 | `	}` |
|      123 |  976 | `	if( zIn >= zEnd ){` |
|        - |  977 | `		/* A simple symbol,return immediately */` |
|      ! 0 |  978 | `		return SXERR_CONTINUE;` |
|        - |  979 | `	}` |
|      123 |  980 | `	if( zIn[0] == '\'' \|\| zIn[0] == '"' ){` |
|        - |  981 | `		/* Make sure we are dealing with a nowdoc */` |
|       52 |  982 | `		bNowDoc =  zIn[0] == '\'' ? TRUE : FALSE;` |
|       52 |  983 | `		zIn++;` |
|       24 |  984 | `	}` |
|      123 |  985 | `	if( zIn[0] < 0xc0 && !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|        - |  986 | `		/* Invalid delimiter,return immediately */` |
|      ! 0 |  987 | `		return SXERR_CONTINUE;` |
|        - |  988 | `	}` |
|        - |  989 | `	/* Isolate the identifier */` |
|      123 |  990 | `	sDelim.zString = (const char *)zIn;` |
|      126 |  991 | `	for(;;){` |
|      257 |  992 | `		zPtr = zIn;` |
|        - |  993 | `		/* Skip alphanumeric stream */` |
|      807 |  994 | `		while( zPtr < zEnd && zPtr[0] < 0xc0 && (SyisAlphaNum(zPtr[0]) \|\| zPtr[0] == '_') ){` |
|      429 |  995 | `			zPtr++;` |
|        5 |  996 | `		}` |
|      257 |  997 | `		if( zPtr < zEnd && zPtr[0] >= 0xc0 ){` |
|       19 |  998 | `			zPtr++;` |
|        - |  999 | `			/* UTF-8 stream */` |
|       37 | 1000 | `			while( zPtr < zEnd && ((zPtr[0] & 0xc0) == 0x80) ){` |
|       19 | 1001 | `				zPtr++;` |
|        1 | 1002 | `			}` |
|        9 | 1003 | `		}` |
|      257 | 1004 | `		if( zPtr == zIn ){` |
|        - | 1005 | `			/* Not an UTF-8 or alphanumeric stream */` |
|      123 | 1006 | `			break;` |
|        - | 1007 | `		}` |
|        - | 1008 | `		/* Synchronize pointers */` |
|      139 | 1009 | `		zIn = zPtr;` |
|        5 | 1010 | `	}` |
|        - | 1011 | `	/* Get the identifier length */` |
|      123 | 1012 | `	sDelim.nByte = (sxu32)((const char *)zIn-sDelim.zString);` |
|      123 | 1013 | `	if( zIn[0] == '"' \|\| (bNowDoc && zIn[0] == '\'') ){` |
|        - | 1014 | `		/* Jump the trailing single quote */` |
|       52 | 1015 | `		zIn++;` |
|       24 | 1016 | `	}` |
|        - | 1017 | `	/* Jump trailing white spaces */` |
|      123 | 1018 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|      ! 0 | 1019 | `		zIn++;` |
|      ! 0 | 1020 | `	}` |
|      123 | 1021 | `	if( sDelim.nByte <= 0 \|\| zIn >= zEnd \|\| zIn[0] != '\n' ){` |
|        - | 1022 | `		/* Invalid syntax */` |
|      ! 0 | 1023 | `		return SXERR_CONTINUE;` |
|        - | 1024 | `	}` |
|      123 | 1025 | `	pStream->nLine++; /* Increment line counter */` |
|      123 | 1026 | `	zIn++;` |
|        - | 1027 | `	/* Isolate the delimited string */` |
|      123 | 1028 | `	sStr.zString = (const char *)zIn;` |
|        - | 1029 | `	/* PHP 7.3 flexible heredoc/nowdoc: the closing marker may be preceded` |
|        - | 1030 | `	 * by whitespace (spaces/tabs), and may be followed by any non-identifier` |
|        - | 1031 | `	 * character. The indent count is recorded in pToken->pUserData and the` |
|        - | 1032 | `	 * compile phase strips it from each body line. */` |
|        - | 1033 | `	{` |
|      123 | 1034 | `		const unsigned char *zMarkerLine = zIn; /* Start of marker's line (set on match) */` |
|      123 | 1035 | `		sxu32 nIndent = 0;` |
|      265 | 1036 | `		for(;;){` |
|      329 | 1037 | `			const unsigned char *zLineStart = zIn;` |
|        - | 1038 | `			/* Skip leading space/tab on this line */` |
|      881 | 1039 | `			while( zIn < zEnd && (zIn[0] == ' ' \|\| zIn[0] == '\t') ){` |
|      394 | 1040 | `				zIn++;` |
|        4 | 1041 | `			}` |
|      324 | 1042 | `			if( (sxu32)(zEnd - zIn) >= sDelim.nByte` |
|      328 | 1043 | `				&& SyMemcmp((const void *)sDelim.zString,(const void *)zIn,sDelim.nByte) == 0 ){` |
|        - | 1044 | `				int bIdentCont;` |
|      121 | 1045 | `				zPtr = &zIn[sDelim.nByte];` |
|        - | 1046 | `				/* Disambiguate: next byte must not continue an identifier.` |
|        - | 1047 | `				 * A leading byte >= 0xc0 starts a multi-byte UTF-8 sequence,` |
|        - | 1048 | `				 * which PHP identifiers may contain, so treat it as ident. */` |
|      121 | 1049 | `				if( zPtr >= zEnd ){` |
|      ! 0 | 1050 | `					bIdentCont = 0;` |
|      121 | 1051 | `				}else if( zPtr[0] >= 0xc0 ){` |
|      ! 0 | 1052 | `					bIdentCont = 1;` |
|      ! 0 | 1053 | `				}else{` |
|      121 | 1054 | `					bIdentCont = (SyisAlphaNum(zPtr[0]) \|\| zPtr[0] == '_');` |
|        - | 1055 | `				}` |
|      121 | 1056 | `				if( !bIdentCont ){` |
|        - | 1057 | `					/* Closing marker found */` |
|      121 | 1058 | `					nIndent = (sxu32)(zIn - zLineStart);` |
|      121 | 1059 | `					zMarkerLine = zLineStart;` |
|      121 | 1060 | `					pStream->zText = zPtr; /* Cursor right after identifier */` |
|      121 | 1061 | `					break;` |
|        - | 1062 | `				}` |
|      ! 0 | 1063 | `			}` |
|        - | 1064 | `			/* Not the closing marker on this line; walk to next newline */` |
|     4481 | 1065 | `			while( zIn < zEnd && zIn[0] != '\n' ){` |
|     4273 | 1066 | `				zIn++;` |
|        5 | 1067 | `			}` |
|      213 | 1068 | `			if( zIn >= zEnd ){` |
|        - | 1069 | `				/* End of input without finding the closing marker */` |
|        3 | 1070 | `				pStream->zText = pStream->zEnd;` |
|        3 | 1071 | `				zMarkerLine = zIn;` |
|        3 | 1072 | `				break;` |
|        - | 1073 | `			}` |
|      211 | 1074 | `			pStream->nLine++;` |
|      211 | 1075 | `			zIn++;` |
|        5 | 1076 | `		}` |
|        - | 1077 | `		/* Body runs from sStr.zString up to just before the marker line */` |
|      123 | 1078 | `		sStr.nByte = (sxu32)((const char *)zMarkerLine - sStr.zString);` |
|      123 | 1079 | `		pToken->nType = bNowDoc ? PH7_TK_NOWDOC : PH7_TK_HEREDOC;` |
|      123 | 1080 | `		SyStringDupPtr(&pToken->sData,&sStr);` |
|        - | 1081 | `		/* Strip exactly one line terminator that precedes the marker's line. */` |
|      118 | 1082 | `		if( pToken->sData.nByte > 0` |
|      119 | 1083 | `			&& pToken->sData.zString[pToken->sData.nByte - 1] == '\n' ){` |
|      113 | 1084 | `			pToken->sData.nByte--;` |
|      108 | 1085 | `			if( pToken->sData.nByte > 0` |
|      113 | 1086 | `				&& pToken->sData.zString[pToken->sData.nByte - 1] == '\r' ){` |
|      ! 0 | 1087 | `				pToken->sData.nByte--;` |
|      ! 0 | 1088 | `			}` |
|       54 | 1089 | `		}` |
|      123 | 1090 | `		pToken->pUserData = SX_INT_TO_PTR(nIndent);` |
|        - | 1091 | `	}` |
|        - | 1092 | `	/* All done */` |
|      123 | 1093 | `	return SXRET_OK;` |
|       64 | 1094 | `}` |
|        - | 1095 | `/*` |
|        - | 1096 | ` * Tokenize a raw PHP input.` |
|        - | 1097 | ` * This is the public tokenizer called by most code generator routines.` |
|        - | 1098 | ` */` |
|    20762 | 1099 | `PH7_PRIVATE sxi32 PH7_TokenizePHP(const char *zInput,sxu32 nLen,sxu32 nLineStart,SySet *pOut)` |
|        5 | 1100 | `{` |
|        - | 1101 | `	SyLex sLexer;` |
|        - | 1102 | `	sxi32 rc;` |
|        - | 1103 | `	/* Defense-in-depth cap for internal tokenizer calls that bypass ph7_compile() */` |
|    20767 | 1104 | `	if( nLen > PH7_MAX_INPUT_SIZE ){` |
|      ! 0 | 1105 | `		return SXERR_LIMIT;` |
|        - | 1106 | `	}` |
|        - | 1107 | `	/* Initialize the lexer */` |
|    20767 | 1108 | `	rc = SyLexInit(&sLexer,&(*pOut),TokenizePHP,0);` |
|    20767 | 1109 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1110 | `		return rc;` |
|        - | 1111 | `	}` |
|    20767 | 1112 | `	sLexer.sStream.nLine = nLineStart;` |
|        - | 1113 | `	/* Tokenize input */` |
|    20767 | 1114 | `	rc = SyLexTokenizeInput(&sLexer,zInput,nLen,0,0,0);` |
|        - | 1115 | `	/* Release the lexer */` |
|    20767 | 1116 | `	SyLexRelease(&sLexer);` |
|        - | 1117 | `	/* Tokenization result */` |
|    20767 | 1118 | `	return rc;` |
|    10386 | 1119 | `}` |
|        - | 1120 | `/*` |
|        - | 1121 | ` * High level public tokenizer.` |
|        - | 1122 | ` *  Tokenize the input into PHP tokens and raw tokens [i.e: HTML,XML,Raw text...].` |
|        - | 1123 | ` * According to the PHP language reference manual` |
|        - | 1124 | ` *   When PHP parses a file, it looks for opening and closing tags, which tell PHP` |
|        - | 1125 | ` *   to start and stop interpreting the code between them. Parsing in this manner allows` |
|        - | 1126 | ` *   PHP to be embedded in all sorts of different documents, as everything outside of a pair` |
|        - | 1127 | ` *   of opening and closing tags is ignored by the PHP parser. Most of the time you will see` |
|        - | 1128 | ` *   PHP embedded in HTML documents, as in this example.` |
|        - | 1129 | ` *   <?php echo 'While this is going to be parsed.'; ?>` |
|        - | 1130 | ` *   <p>This will also be ignored.</p>` |
|        - | 1131 | ` *   You can also use more advanced structures:` |
|        - | 1132 | ` *   Example #1 Advanced escaping` |
|        - | 1133 | ` * <?php` |
|        - | 1134 | ` * if ($expression) {` |
|        - | 1135 | ` *   ?>` |
|        - | 1136 | ` *   <strong>This is true.</strong>` |
|        - | 1137 | ` *   <?php` |
|        - | 1138 | ` * } else {` |
|        - | 1139 | ` *   ?>` |
|        - | 1140 | ` *   <strong>This is false.</strong>` |
|        - | 1141 | ` *   <?php` |
|        - | 1142 | ` * }` |
|        - | 1143 | ` * ?>` |
|        - | 1144 | ` * This works as expected, because when PHP hits the ?> closing tags, it simply starts outputting` |
|        - | 1145 | ` * whatever it finds (except for an immediately following newline - see instruction separation ) until it hits` |
|        - | 1146 | ` * another opening tag. The example given here is contrived, of course, but for outputting large blocks of text` |
|        - | 1147 | ` * dropping out of PHP parsing mode is generally more efficient than sending all of the text through echo() or print().` |
|        - | 1148 | ` * There are four different pairs of opening and closing tags which can be used in PHP. Three of those, <?php ?>` |
|        - | 1149 | ` * <script language="php"> </script>  and <? ?> are always available. The other two are short tags and ASP style` |
|        - | 1150 | ` * tags, and can be turned on and off from the php.ini configuration file. As such, while some people find short tags` |
|        - | 1151 | ` * and ASP style tags convenient, they are less portable, and generally not recommended.` |
|        - | 1152 | ` * Note:` |
|        - | 1153 | ` * Also note that if you are embedding PHP within XML or XHTML you will need to use the <?php ?> tags to remain` |
|        - | 1154 | ` * compliant with standards.` |
|        - | 1155 | ` * Example #2 PHP Opening and Closing Tags` |
|        - | 1156 | ` * 1.  <?php echo 'if you want to serve XHTML or XML documents, do it like this'; ?>` |
|        - | 1157 | ` * 2.  <script language="php">` |
|        - | 1158 | ` *       echo 'some editors (like FrontPage) don\'t` |
|        - | 1159 | ` *             like processing instructions';` |
|        - | 1160 | ` *   </script>` |
|        - | 1161 | ` *` |
|        - | 1162 | ` * 3.  <? echo 'this is the simplest, an SGML processing instruction'; ?>` |
|        - | 1163 | ` *   <?= expression ?> This is a shortcut for "<? echo expression ?>"` |
|        - | 1164 | ` */` |
|    13656 | 1165 | `PH7_PRIVATE sxi32 PH7_TokenizeRawText(const char *zInput,sxu32 nLen,SySet *pOut)` |
|        5 | 1166 | `{` |
|    13661 | 1167 | `	const char *zEnd = &zInput[nLen];` |
|    13661 | 1168 | `	const char *zIn  = zInput;` |
|        - | 1169 | `	const char *zCur,*zCurEnd;` |
|    13661 | 1170 | `	SyString sCtag = { 0, 0 };     /* Closing tag */` |
|        - | 1171 | `	SyToken sToken;` |
|        - | 1172 | `	SyString sDoc;` |
|        - | 1173 | `	sxu32 nLine;` |
|        - | 1174 | `	sxi32 iNest;` |
|        - | 1175 | `	sxi32 rc;` |
|        - | 1176 | `	/* Tokenize the input into PHP tokens and raw tokens */` |
|    13661 | 1177 | `	nLine = 1;` |
|    13661 | 1178 | `	zCur = zCurEnd   = 0; /* Prevent compiler warning */` |
|    13661 | 1179 | `	sToken.pUserData = 0;` |
|    13661 | 1180 | `	iNest = 0;` |
|    13661 | 1181 | `	sDoc.nByte = 0;` |
|    13661 | 1182 | `	sDoc.zString = ""; /* cc warning */` |
|    13658 | 1183 | `	for(;;){` |
|    27321 | 1184 | `		if( zIn >= zEnd ){` |
|        - | 1185 | `			/* End of input reached */` |
|    13611 | 1186 | `			break;` |
|        - | 1187 | `		}` |
|    13715 | 1188 | `		sToken.nLine = nLine;` |
|    13715 | 1189 | `		zCur = zIn;` |
|    13715 | 1190 | `		zCurEnd = 0;` |
|    13769 | 1191 | `		while( zIn < zEnd ){` |
|    13719 | 1192 | `			 if( zIn[0] == '<' ){` |
|    13665 | 1193 | `				const char *zTmp = zIn; /* End of raw input marker */` |
|    13665 | 1194 | `				zIn++;` |
|    13665 | 1195 | `				if( zIn < zEnd ){` |
|    13665 | 1196 | `					if( zIn[0] == '?' ){` |
|    13665 | 1197 | `						zIn++;` |
|    13665 | 1198 | `						if( (sxu32)(zEnd - zIn) >= sizeof("php")-1 &&  SyStrnicmp(zIn,"php",sizeof("php")-1) == 0 ){` |
|        - | 1199 | `							/* opening tag: <?php */` |
|    13663 | 1200 | `							zIn += sizeof("php")-1;` |
|     6829 | 1201 | `						}` |
|        - | 1202 | `						/* Look for the closing tag '?>' */` |
|    13665 | 1203 | `						SyStringInitFromBuf(&sCtag,"?>",sizeof("?>")-1);` |
|    13665 | 1204 | `						zCurEnd = zTmp;` |
|    13665 | 1205 | `						break;` |
|        - | 1206 | `					}` |
|      ! 0 | 1207 | `				}` |
|      ! 0 | 1208 | `			}else{` |
|       59 | 1209 | `				if( zIn[0] == '\n' ){` |
|       59 | 1210 | `					nLine++;` |
|       27 | 1211 | `				}` |
|       59 | 1212 | `				zIn++;` |
|        - | 1213 | `			 }` |
|        5 | 1214 | `		} /* While(zIn < zEnd) */` |
|    13715 | 1215 | `		if( zCurEnd == 0 ){` |
|       54 | 1216 | `			zCurEnd = zIn;` |
|       25 | 1217 | `		}` |
|        - | 1218 | `		/* Save the raw token */` |
|    13715 | 1219 | `		SyStringInitFromBuf(&sToken.sData,zCur,zCurEnd - zCur);` |
|    13715 | 1220 | `		sToken.nType = PH7_TOKEN_RAW;` |
|    13715 | 1221 | `		rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|    13715 | 1222 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1223 | `			return rc;` |
|        - | 1224 | `		}` |
|    13715 | 1225 | `		if( zIn >= zEnd ){` |
|       54 | 1226 | `			break;` |
|        - | 1227 | `		}` |
|        - | 1228 | `		/* Ignore leading white space */` |
|    29259 | 1229 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    15599 | 1230 | `			if( zIn[0] == '\n' ){` |
|    14467 | 1231 | `				nLine++;` |
|     7231 | 1232 | `			}` |
|    15599 | 1233 | `			zIn++;` |
|        5 | 1234 | `		}` |
|        - | 1235 | `		/* Delimit the PHP chunk */` |
|    13665 | 1236 | `		sToken.nLine = nLine;` |
|    13665 | 1237 | `		zCur = zIn;` |
|  1464197 | 1238 | `		while( (sxu32)(zEnd - zIn) >= sCtag.nByte ){` |
|        - | 1239 | `			const char *zPtr;` |
|  1458191 | 1240 | `			if( SyMemcmp(zIn,sCtag.zString,sCtag.nByte) == 0 && iNest < 1 ){` |
|     7657 | 1241 | `				break;` |
|        - | 1242 | `			}` |
|   728040 | 1243 | `			for(;;){` |
|  1456085 | 1244 | `				if( zIn[0] != '/' \|\| (zIn[1] != '*' && zIn[1] != '/') /* && sCtag.nByte >= 2 */ ){` |
|   725272 | 1245 | `					break;` |
|        - | 1246 | `				}` |
|     5551 | 1247 | `				zIn += 2;` |
|     5551 | 1248 | `				if( zIn[-1] == '/' ){` |
|        - | 1249 | `					/* Inline comment */` |
|   206071 | 1250 | `					while( zIn < zEnd && zIn[0] != '\n' ){` |
|   200737 | 1251 | `						zIn++;` |
|        5 | 1252 | `					}` |
|     5339 | 1253 | `					if( zIn >= zEnd ){` |
|      ! 0 | 1254 | `						zIn--;` |
|      ! 0 | 1255 | `					}` |
|     2672 | 1256 | `				}else{` |
|        - | 1257 | `					/* Block comment */` |
|    14249 | 1258 | `					while( (sxu32)(zEnd-zIn) >= sizeof("*/") - 1 ){` |
|    14249 | 1259 | `						if( zIn[0] == '*' && zIn[1] == '/' ){` |
|      217 | 1260 | `							zIn += 2;` |
|      217 | 1261 | `							break;` |
|        - | 1262 | `						}` |
|    14037 | 1263 | `						if( zIn[0] == '\n' ){` |
|       73 | 1264 | `							nLine++;` |
|       34 | 1265 | `						}` |
|    14037 | 1266 | `						zIn++;` |
|        5 | 1267 | `					}` |
|        - | 1268 | `				}` |
|        5 | 1269 | `			}` |
|  1450539 | 1270 | `			if( zIn[0] == '\n' ){` |
|    49635 | 1271 | `				nLine++;` |
|    49635 | 1272 | `				if( iNest > 0 ){` |
|      329 | 1273 | `					zIn++;` |
|      719 | 1274 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|      394 | 1275 | `						zIn++;` |
|        4 | 1276 | `					}` |
|      329 | 1277 | `					zPtr = zIn;` |
|     1645 | 1278 | `					while( zIn < zEnd ){` |
|     1645 | 1279 | `						if( (unsigned char)zIn[0] >= 0xc0 ){` |
|        - | 1280 | `							/* UTF-8 stream */` |
|       19 | 1281 | `							zIn++;` |
|       37 | 1282 | `							SX_JMP_UTF8(zIn,zEnd);` |
|     1632 | 1283 | `						}else if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|      167 | 1284 | `							break;` |
|      ! 0 | 1285 | `						}else{` |
|     1303 | 1286 | `							zIn++;` |
|        - | 1287 | `						}` |
|        5 | 1288 | `					}` |
|      329 | 1289 | `					if( (sxu32)(zIn - zPtr) == sDoc.nByte && SyMemcmp(sDoc.zString,zPtr,sDoc.nByte) == 0 ){` |
|      121 | 1290 | `						iNest = 0;` |
|       58 | 1291 | `					}` |
|      329 | 1292 | `					continue;` |
|        5 | 1293 | `				}` |
|  1425562 | 1294 | `			}else if ( (sxu32)(zEnd - zIn) >= sizeof("<<<") && zIn[0] == '<' && zIn[1] == '<' && zIn[2] == '<' && iNest < 1){` |
|      123 | 1295 | `				zIn += sizeof("<<<")-1;` |
|      135 | 1296 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|       13 | 1297 | `					zIn++;` |
|        1 | 1298 | `				}` |
|      123 | 1299 | `				if( zIn[0] == '"' \|\| zIn[0] == '\'' ){` |
|       52 | 1300 | `					zIn++;` |
|       24 | 1301 | `				}` |
|      123 | 1302 | `				zPtr = zIn;` |
|      565 | 1303 | `				while( zIn < zEnd ){` |
|      565 | 1304 | `					if( (unsigned char)zIn[0] >= 0xc0 ){` |
|        - | 1305 | `						/* UTF-8 stream */` |
|       19 | 1306 | `						zIn++;` |
|       37 | 1307 | `						SX_JMP_UTF8(zIn,zEnd);` |
|      552 | 1308 | `					}else if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       64 | 1309 | `						break;` |
|      ! 0 | 1310 | `					}else{` |
|      429 | 1311 | `						zIn++;` |
|        - | 1312 | `					}` |
|        5 | 1313 | `				}` |
|      123 | 1314 | `				SyStringInitFromBuf(&sDoc,zPtr,zIn-zPtr);` |
|      123 | 1315 | `				SyStringFullTrim(&sDoc);` |
|      123 | 1316 | `				if( sDoc.nByte > 0 ){` |
|      123 | 1317 | `					iNest++;` |
|       59 | 1318 | `				}` |
|      123 | 1319 | `				continue;` |
|        - | 1320 | `			}` |
|  1450097 | 1321 | `			zIn++;` |
|        - | 1322 |  |
|  1450097 | 1323 | `			if ( zIn >= zEnd )` |
|        3 | 1324 | `				break;` |
|        5 | 1325 | `		}` |
|    13665 | 1326 | `		if( (sxu32)(zEnd - zIn) < sCtag.nByte ){` |
|     6013 | 1327 | `			zIn = zEnd;` |
|     3004 | 1328 | `		}` |
|    13665 | 1329 | `		if( zCur < zIn ){` |
|        - | 1330 | `			/* Save the PHP chunk for later processing */` |
|    10627 | 1331 | `			sToken.nType = PH7_TOKEN_PHP;` |
|    10627 | 1332 | `			SyStringInitFromBuf(&sToken.sData,zCur,zIn-zCur);` |
|    21161 | 1333 | `			SyStringRightTrim(&sToken.sData); /* Trim trailing white spaces */` |
|    10627 | 1334 | `			rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|    10627 | 1335 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1336 | `				return rc;` |
|        - | 1337 | `			}` |
|     5311 | 1338 | `		}` |
|    13665 | 1339 | `		if( zIn < zEnd ){` |
|        - | 1340 | `			/* Jump the trailing closing tag */` |
|     7657 | 1341 | `			zIn += sCtag.nByte;` |
|     3826 | 1342 | `		}` |
|        5 | 1343 | `	} /* For(;;) */` |
|        - | 1344 |  |
|    13661 | 1345 | ` 	return SXRET_OK;` |
|     6833 | 1346 | `}` |
|        - | 1347 |  |
