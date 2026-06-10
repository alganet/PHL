# src/ph7/lex.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 755/809 lines (93.33%)

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
|  9381816 |   19 | `static sxi32 TokenizePHP(SyStream *pStream,SyToken *pToken,void *pUserData,void *pCtxData)` |
|        2 |   20 |  |
|        - |   21 | `	SyString *pStr;` |
|        - |   22 | `	sxi32 rc;` |
|        - |   23 | `	/* Ignore leading white spaces */` |
| 14049518 |   24 | `	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){` |
|        - |   25 | `		/* Advance the stream cursor */` |
|  4667702 |   26 | `		if( pStream->zText[0] == '\n' ){` |
|        - |   27 | `			/* Update line counter */` |
|    35336 |   28 | `			pStream->nLine++;` |
|    17667 |   29 | `		}` |
|  4667702 |   30 | `		pStream->zText++;` |
|        2 |   31 | `	}` |
|  9381818 |   32 | `	if( pStream->zText >= pStream->zEnd ){` |
|        - |   33 | `		/* End of input reached */` |
|      ! 0 |   34 | `		return SXERR_EOF;` |
|        - |   35 | `	}` |
|        - |   36 | `	/* Record token starting position and line */` |
|  9381818 |   37 | `	pToken->nLine = pStream->nLine;` |
|  9381818 |   38 | `	pToken->pUserData = 0;` |
|  9381818 |   39 | `	pStr = &pToken->sData;` |
|  9381818 |   40 | `	SyStringInitFromBuf(pStr,pStream->zText,0);` |
| 11135459 |   41 | `	if( pStream->zText[0] >= 0xc0 \|\| SyisAlpha(pStream->zText[0]) \|\| pStream->zText[0] == '_' ){` |
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
|  3507284 |   52 | `		if( pStream->zText[0] < 0xc0 ){` |
|  3507268 |   53 | `			pStream->zText++;` |
|  1753633 |   54 | `		}` |
|  3452672 |   55 | `		for(;;){` |
|  6905346 |   56 | `			zIn = pStream->zText;` |
|  6905346 |   57 | `			if( zIn[0] >= 0xc0 ){` |
|       49 |   58 | `				zIn++;` |
|        - |   59 | `				/* UTF-8 stream */` |
|      109 |   60 | `				while( zIn < pStream->zEnd && ((zIn[0] & 0xc0) == 0x80) ){` |
|       61 |   61 | `					zIn++;` |
|        1 |   62 | `				}` |
|       24 |   63 | `			}` |
|        - |   64 | `			/* Skip alphanumeric stream */` |
| 28817938 |   65 | `			while( zIn < pStream->zEnd && zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_') ){` |
| 18459922 |   66 | `				zIn++;` |
|        2 |   67 | `			}` |
|  6905346 |   68 | `			if( zIn == pStream->zText ){` |
|        - |   69 | `				/* Not an UTF-8 or alphanumeric stream */` |
|  3507284 |   70 | `				break;` |
|        - |   71 | `			}` |
|        - |   72 | `			/* Synchronize pointers */` |
|  3398064 |   73 | `			pStream->zText = zIn;` |
|        2 |   74 | `		}` |
|        - |   75 | `		/* Record token length */` |
|  3507284 |   76 | `		pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  3507284 |   77 | `		nKeyword = KeywordCode(pStr->zString,(int)pStr->nByte);` |
|        - |   78 | `		/* PHP 7.4: 'fn' is a keyword reserved for arrow functions.` |
|        - |   79 | `		 * The auto-generated perfect hash above doesn't know about it,` |
|        - |   80 | `		 * so intercept the 'fn' identifier here.` |
|        - |   81 | `		 */` |
|  3507766 |   82 | `		if( nKeyword == PH7_TK_ID && pStr->nByte == 2` |
|  1123658 |   83 | `			&& pStr->zString[0] == 'f' && pStr->zString[1] == 'n' ){` |
|      362 |   84 | `			nKeyword = PH7_TKWRD_FN;` |
|      180 |   85 | `		}` |
|  3507284 |   86 | `		if( nKeyword != PH7_TK_ID ){` |
|  1268454 |   87 | `			if( nKeyword &` |
|        - |   88 | `				(PH7_TKWRD_NEW\|PH7_TKWRD_CLONE\|PH7_TKWRD_AND\|PH7_TKWRD_XOR\|PH7_TKWRD_OR\|PH7_TKWRD_INSTANCEOF\|PH7_TKWRD_SEQ\|PH7_TKWRD_SNE) ){` |
|        - |   89 | `					/* Alpha stream operators [i.e: new,clone,and,instanceof,eq,ne,or,xor],save the operator instance for later processing */` |
|    16968 |   90 | `					pToken->pUserData = (void *)PH7_ExprExtractOperator(pStr,0);` |
|        - |   91 | `					/* Mark as an operator */` |
|    16968 |   92 | `					pToken->nType = PH7_TK_ID\|PH7_TK_OP;` |
|     8485 |   93 | `			}else{` |
|        - |   94 | `				/* We are dealing with a keyword [i.e: while,foreach,class...],save the keyword ID */` |
|  1251488 |   95 | `				pToken->nType = PH7_TK_KEYWORD;` |
|  1251488 |   96 | `				pToken->pUserData = SX_INT_TO_PTR(nKeyword);` |
|        - |   97 | `			}` |
|   634228 |   98 | `		}else{` |
|        - |   99 | `			/* A simple identifier */` |
|  2238832 |  100 | `			pToken->nType = PH7_TK_ID;` |
|        - |  101 | `		}` |
|  1753643 |  102 | `	}else{` |
|        - |  103 | `		sxi32 c;` |
|        - |  104 | `		/* Non-alpha stream */` |
|  5874536 |  105 | `		if( pStream->zText[0] == '#' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '[' ){` |
|       70 |  106 | `			sxu32 nDepth = 1;` |
|        - |  107 | `			/* PHP 8 attribute group '#[ ... ]': skip the whole balanced group as` |
|        - |  108 | `			 * trivia (attributes are not stored yet). Brackets inside string` |
|        - |  109 | `			 * literals and comments must not affect the depth count. An` |
|        - |  110 | `			 * unterminated group is silently consumed up to EOF, consistent` |
|        - |  111 | `			 * with unterminated block comments below.` |
|        - |  112 | `			 */` |
|       70 |  113 | `			pStream->zText += 2;` |
|     1368 |  114 | `			while( pStream->zText < pStream->zEnd && nDepth > 0 ){` |
|     1300 |  115 | `				sxi32 d = pStream->zText[0];` |
|     1300 |  116 | `				if( d == '[' ){` |
|       11 |  117 | `					nDepth++;` |
|     1295 |  118 | `				}else if( d == ']' ){` |
|       80 |  119 | `					nDepth--;` |
|     1251 |  120 | `				}else if( d == '\'' \|\| d == '"' ){` |
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
|     1206 |  143 | `				}else if( d == '#' \|\| (d == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '/') ){` |
|        - |  144 | `					/* Inline comment inside the group */` |
|      ! 0 |  145 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] != '\n' ){` |
|      ! 0 |  146 | `						pStream->zText++;` |
|      ! 0 |  147 | `					}` |
|      ! 0 |  148 | `					continue; /* Let the outer loop count the newline */` |
|     1200 |  149 | `				}else if( d == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '*' ){` |
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
|     1200 |  163 | `				}else if( d == '\n' ){` |
|        7 |  164 | `					pStream->nLine++;` |
|        3 |  165 | `				}` |
|     1300 |  166 | `				pStream->zText++;` |
|        2 |  167 | `			}` |
|        - |  168 | `			/* Tell the upper-layer to ignore this token */` |
|       70 |  169 | `			return SXERR_CONTINUE;` |
|  5914212 |  170 | `		}else if( pStream->zText[0] == '#' \|\|` |
|  5874460 |  171 | `			( pStream->zText[0] == '/' &&  &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '/') ){` |
|     4126 |  172 | `				pStream->zText++;` |
|        - |  173 | `				/* Inline comments */` |
|   152582 |  174 | `				while( pStream->zText < pStream->zEnd && pStream->zText[0] != '\n' ){` |
|   148458 |  175 | `					pStream->zText++;` |
|        2 |  176 | `				}` |
|        - |  177 | `				/* Tell the upper-layer to ignore this token */` |
|     4126 |  178 | `				return SXERR_CONTINUE;` |
|  5870344 |  179 | `		}else if( pStream->zText[0] == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '*' ){` |
|    75302 |  180 | `			pStream->zText += 2;` |
|        - |  181 | `			/* Block comment */` |
|  2135650 |  182 | `			while( pStream->zText < pStream->zEnd ){` |
|  2135650 |  183 | `				if( pStream->zText[0] == '*' ){` |
|    75328 |  184 | `					if( &pStream->zText[1] >= pStream->zEnd \|\| pStream->zText[1] == '/'  ){` |
|    37652 |  185 | `						break;` |
|        - |  186 | `					}` |
|       13 |  187 | `				}` |
|  2060350 |  188 | `				if( pStream->zText[0] == '\n' ){` |
|       28 |  189 | `					pStream->nLine++;` |
|       13 |  190 | `				}` |
|  2060350 |  191 | `				pStream->zText++;` |
|        2 |  192 | `			}` |
|    75302 |  193 | `			pStream->zText += 2;` |
|        - |  194 | `			/* Tell the upper-layer to ignore this token */` |
|    75302 |  195 | `			return SXERR_CONTINUE;` |
|  5795044 |  196 | `		}else if( SyisDigit(pStream->zText[0]) ){` |
|   110820 |  197 | `			pStream->zText++;` |
|        - |  198 | `			/* PHP 7.4: handle underscore separator immediately following the first digit.` |
|        - |  199 | `			 * Check pStream->zText < pStream->zEnd BEFORE forming pStream->zText + 1 so` |
|        - |  200 | `			 * we never compute a pointer past one-past-end. */` |
|   110898 |  201 | `			if( pStream->zText < pStream->zEnd` |
|   110818 |  202 | `				&& pStream->zText[0] == '_'` |
|    55489 |  203 | `				&& pStream->zText + 1 < pStream->zEnd` |
|      160 |  204 | `				&& pStream->zText[1] < 0xc0` |
|      162 |  205 | `				&& SyisDigit(pStream->zText[1]) ){` |
|      151 |  206 | `				pStream->zText++; /* swallow underscore between two digits */` |
|       75 |  207 | `			}` |
|        - |  208 | `			/* Decimal digit stream (PHP 7.4: underscore separator allowed between two digits) */` |
|   121492 |  209 | `			while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|    10674 |  210 | `				pStream->zText++;` |
|    10758 |  211 | `				if( pStream->zText < pStream->zEnd` |
|    10672 |  212 | `					&& pStream->zText[0] == '_'` |
|     5422 |  213 | `					&& pStream->zText + 1 < pStream->zEnd` |
|      172 |  214 | `					&& pStream->zText[1] < 0xc0` |
|      174 |  215 | `					&& SyisDigit(pStream->zText[1]) ){` |
|      173 |  216 | `					pStream->zText++; /* swallow underscore between two digits */` |
|       86 |  217 | `				}` |
|        2 |  218 | `			}` |
|        - |  219 | `			/* Mark the token as integer until we encounter a real number */` |
|   110820 |  220 | `			pToken->nType = PH7_TK_INTEGER;` |
|   110820 |  221 | `			if( pStream->zText < pStream->zEnd ){` |
|   110820 |  222 | `				c = pStream->zText[0];` |
|   110820 |  223 | `				if( c == '.' ){` |
|        - |  224 | `					/* Real number (PHP 7.4: underscore separator allowed between two digits) */` |
|      526 |  225 | `					pStream->zText++;` |
|     1922 |  226 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|     1398 |  227 | `						pStream->zText++;` |
|     1402 |  228 | `						if( pStream->zText < pStream->zEnd` |
|     1396 |  229 | `							&& pStream->zText[0] == '_'` |
|      704 |  230 | `							&& pStream->zText + 1 < pStream->zEnd` |
|       12 |  231 | `							&& pStream->zText[1] < 0xc0` |
|       14 |  232 | `							&& SyisDigit(pStream->zText[1]) ){` |
|       13 |  233 | `							pStream->zText++;` |
|        6 |  234 | `						}` |
|        2 |  235 | `					}` |
|      526 |  236 | `					if( pStream->zText < pStream->zEnd ){` |
|      526 |  237 | `						c = pStream->zText[0];` |
|      526 |  238 | `						if( c=='e' \|\| c=='E' ){` |
|       29 |  239 | `							pStream->zText++;` |
|       29 |  240 | `							if( pStream->zText < pStream->zEnd ){` |
|       29 |  241 | `								c = pStream->zText[0];` |
|       35 |  242 | `								if( (c =='+' \|\| c=='-') && &pStream->zText[1] < pStream->zEnd  &&` |
|       15 |  243 | `									pStream->zText[1] < 0xc0 && SyisDigit(pStream->zText[1]) ){` |
|       15 |  244 | `										pStream->zText++;` |
|        7 |  245 | `								}` |
|       69 |  246 | `								while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|       41 |  247 | `									pStream->zText++;` |
|       44 |  248 | `									if( pStream->zText < pStream->zEnd` |
|       40 |  249 | `										&& pStream->zText[0] == '_'` |
|       24 |  250 | `										&& pStream->zText + 1 < pStream->zEnd` |
|        8 |  251 | `										&& pStream->zText[1] < 0xc0` |
|        9 |  252 | `										&& SyisDigit(pStream->zText[1]) ){` |
|        9 |  253 | `										pStream->zText++;` |
|        4 |  254 | `									}` |
|        1 |  255 | `								}` |
|       14 |  256 | `							}` |
|       14 |  257 | `						}` |
|      262 |  258 | `					}` |
|      526 |  259 | `					pToken->nType = PH7_TK_REAL;` |
|   110558 |  260 | `				}else if( c=='e' \|\| c=='E' ){` |
|       14 |  261 | `					SXUNUSED(pUserData); /* Prevent compiler warning */` |
|       14 |  262 | `					SXUNUSED(pCtxData);` |
|       29 |  263 | `					pStream->zText++;` |
|       29 |  264 | `					if( pStream->zText < pStream->zEnd ){` |
|       29 |  265 | `						c = pStream->zText[0];` |
|       31 |  266 | `						if( (c =='+' \|\| c=='-') && &pStream->zText[1] < pStream->zEnd  &&` |
|        7 |  267 | `							pStream->zText[1] < 0xc0 && SyisDigit(pStream->zText[1]) ){` |
|        7 |  268 | `								pStream->zText++;` |
|        3 |  269 | `						}` |
|       67 |  270 | `						while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|       39 |  271 | `							pStream->zText++;` |
|       40 |  272 | `							if( pStream->zText < pStream->zEnd` |
|       38 |  273 | `								&& pStream->zText[0] == '_'` |
|       21 |  274 | `								&& pStream->zText + 1 < pStream->zEnd` |
|        4 |  275 | `								&& pStream->zText[1] < 0xc0` |
|        5 |  276 | `								&& SyisDigit(pStream->zText[1]) ){` |
|        5 |  277 | `								pStream->zText++;` |
|        2 |  278 | `							}` |
|        1 |  279 | `						}` |
|       14 |  280 | `					}` |
|       29 |  281 | `					pToken->nType = PH7_TK_REAL;` |
|   110282 |  282 | `				}else if( c == 'x' \|\| c == 'X' ){` |
|        - |  283 | `					/* Hex digit stream (PHP 7.4: underscore separator allowed between two digits) */` |
|       74 |  284 | `					pStream->zText++;` |
|      370 |  285 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisHex(pStream->zText[0]) ){` |
|      297 |  286 | `						pStream->zText++;` |
|      320 |  287 | `						if( pStream->zText < pStream->zEnd` |
|      296 |  288 | `							&& pStream->zText[0] == '_'` |
|      172 |  289 | `							&& pStream->zText + 1 < pStream->zEnd` |
|       48 |  290 | `							&& pStream->zText[1] < 0xc0` |
|       49 |  291 | `							&& SyisHex(pStream->zText[1]) ){` |
|       49 |  292 | `							pStream->zText++;` |
|       24 |  293 | `						}` |
|        1 |  294 | `					}` |
|   110232 |  295 | `				}else if(c  == 'b' \|\| c == 'B' ){` |
|        - |  296 | `					/* Binary digit stream (PHP 7.4: underscore separator allowed between two digits) */` |
|      280 |  297 | `					pStream->zText++;` |
|     2702 |  298 | `					while( pStream->zText < pStream->zEnd && (pStream->zText[0] == '0' \|\| pStream->zText[0] == '1') ){` |
|     1523 |  299 | `						pStream->zText++;` |
|     1583 |  300 | `						if( pStream->zText < pStream->zEnd` |
|     1522 |  301 | `							&& pStream->zText[0] == '_'` |
|      830 |  302 | `							&& pStream->zText + 1 < pStream->zEnd` |
|      139 |  303 | `							&& (pStream->zText[1] == '0' \|\| pStream->zText[1] == '1') ){` |
|      139 |  304 | `							pStream->zText++;` |
|       69 |  305 | `						}` |
|        1 |  306 | `					}` |
|      139 |  307 | `				}` |
|    55409 |  308 | `			}` |
|        - |  309 | `			/* PHP 7.4: absorb a trailing malformed underscore run into the` |
|        - |  310 | `			 * numeric token so the compile phase can emit a PHP-compatible` |
|        - |  311 | `			 * "syntax error, unexpected identifier" parse error. Valid` |
|        - |  312 | `			 * separators were already consumed by the per-loop peek logic` |
|        - |  313 | `			 * above, so an underscore here is always misplaced. */` |
|   110820 |  314 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '_' ){` |
|       15 |  315 | `				pStream->zText++;` |
|       44 |  316 | `				while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0` |
|       46 |  317 | `					&& (SyisAlphaNum(pStream->zText[0]) \|\| pStream->zText[0] == '_') ){` |
|       17 |  318 | `					pStream->zText++;` |
|        1 |  319 | `				}` |
|        7 |  320 | `			}` |
|        - |  321 | `			/* Record token length */` |
|   110820 |  322 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|   110820 |  323 | `			return SXRET_OK;` |
|        - |  324 | `		}` |
|  5684226 |  325 | `		c = pStream->zText[0];` |
|  5684226 |  326 | `		pStream->zText++; /* Advance the stream cursor */` |
|        - |  327 | `		/* Assume we are dealing with an operator*/` |
|  5684226 |  328 | `		pToken->nType = PH7_TK_OP;` |
|  5684226 |  329 | `		switch(c){` |
|  1166714 |  330 | `		case '$': pToken->nType = PH7_TK_DOLLAR; break;` |
|   455048 |  331 | `		case '{': pToken->nType = PH7_TK_OCB;    break;` |
|   455034 |  332 | `		case '}': pToken->nType = PH7_TK_CCB;    break;` |
|   892238 |  333 | `		case '(': pToken->nType = PH7_TK_LPAREN; break;` |
|    81346 |  334 | `		case '[': pToken->nType \|= PH7_TK_OSB;   break; /* Bitwise operation here,since the square bracket token '['` |
|        - |  335 | `														 * is a potential operator [i.e: subscripting] */` |
|    81352 |  336 | `		case ']': pToken->nType = PH7_TK_CSB;    break;` |
|   446111 |  337 | `		case ')': {` |
|   892224 |  338 | `			SySet *pTokSet = pStream->pSet;` |
|        - |  339 | `			/* Assemble type cast operators [i.e: (int),(float),(bool)...] */` |
|   892224 |  340 | `			if( pTokSet->nUsed >= 2 ){` |
|        - |  341 | `				SyToken *pTmp;` |
|        - |  342 | `				/* Peek the last recongnized token */` |
|   892222 |  343 | `				pTmp = (SyToken *)SySetPeek(pTokSet);` |
|   892222 |  344 | `				if( pTmp->nType & PH7_TK_KEYWORD ){` |
|    16238 |  345 | `					sxi32 nID = SX_PTR_TO_INT(pTmp->pUserData);` |
|    16238 |  346 | `					if( (sxu32)nID & (PH7_TKWRD_ARRAY\|PH7_TKWRD_INT\|PH7_TKWRD_FLOAT\|PH7_TKWRD_STRING\|PH7_TKWRD_OBJECT\|PH7_TKWRD_BOOL\|PH7_TKWRD_UNSET) ){` |
|    15986 |  347 | `						pTmp = (SyToken *)SySetAt(pTokSet,pTokSet->nUsed - 2);` |
|    15986 |  348 | `						if( pTmp->nType & PH7_TK_LPAREN ){` |
|        - |  349 | `							/* Merge the three tokens '(' 'TYPE' ')' into a single one */` |
|    15902 |  350 | `							const char * zTypeCast = "(int)";` |
|    15902 |  351 | `							if( nID & PH7_TKWRD_FLOAT ){` |
|     3144 |  352 | `								zTypeCast = "(float)";` |
|    14331 |  353 | `							}else if( nID & PH7_TKWRD_BOOL ){` |
|     3144 |  354 | `								zTypeCast = "(bool)";` |
|    11189 |  355 | `							}else if( nID & PH7_TKWRD_STRING ){` |
|     6284 |  356 | `								zTypeCast = "(string)";` |
|     6477 |  357 | `							}else if( nID & PH7_TKWRD_ARRAY ){` |
|       21 |  358 | `								zTypeCast = "(array)";` |
|     3326 |  359 | `							}else if( nID & PH7_TKWRD_OBJECT ){` |
|       17 |  360 | `								zTypeCast = "(object)";` |
|     3308 |  361 | `							}else if( nID & PH7_TKWRD_UNSET ){` |
|        7 |  362 | `								zTypeCast = "(unset)";` |
|        3 |  363 | `							}` |
|        - |  364 | `							/* Reflect the change */` |
|    15902 |  365 | `							pToken->nType = PH7_TK_OP;` |
|    15902 |  366 | `							SyStringInitFromBuf(&pToken->sData,zTypeCast,SyStrlen(zTypeCast));` |
|        - |  367 | `							/* Save the instance associated with the type cast operator */` |
|    15902 |  368 | `							pToken->pUserData = (void *)PH7_ExprExtractOperator(&pToken->sData,0);` |
|        - |  369 | `							/* Remove the two previous tokens */` |
|    15902 |  370 | `							pTokSet->nUsed -= 2;` |
|    15902 |  371 | `							return SXRET_OK;` |
|        - |  372 | `						}` |
|       42 |  373 | `					}` |
|      168 |  374 | `				}` |
|   438160 |  375 | `			}` |
|   876324 |  376 | `			pToken->nType = PH7_TK_RPAREN;` |
|   876324 |  377 | `			break;` |
|        - |  378 | `				  }` |
|    39723 |  379 | `		case '\'':{` |
|        - |  380 | `			/* Single quoted string */` |
|    79448 |  381 | `			pStr->zString++;` |
|   812738 |  382 | `			while( pStream->zText < pStream->zEnd ){` |
|   812738 |  383 | `				if( pStream->zText[0] == '\''  ){` |
|    79458 |  384 | `					if( pStream->zText[-1] != '\\' ){` |
|    79434 |  385 | `						break;` |
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
|   733292 |  398 | `				if( pStream->zText[0] == '\n' ){` |
|       67 |  399 | `					pStream->nLine++;` |
|       33 |  400 | `				}` |
|   733292 |  401 | `				pStream->zText++;` |
|        2 |  402 | `			}` |
|        - |  403 | `			/* Record token length and type */` |
|    79448 |  404 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|    79448 |  405 | `			pToken->nType = PH7_TK_SSTR;` |
|        - |  406 | `			/* Jump the trailing single quote */` |
|    79448 |  407 | `			pStream->zText++;` |
|    79448 |  408 | `			return SXRET_OK;` |
|        - |  409 | `				  }` |
|     9966 |  410 | `		case '"':{` |
|        - |  411 | `			sxi32 iNest;` |
|        - |  412 | `			/* Double quoted string */` |
|    19934 |  413 | `			pStr->zString++;` |
|   179872 |  414 | `			while( pStream->zText < pStream->zEnd ){` |
|   179872 |  415 | `				if( pStream->zText[0] == '{' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '$'){` |
|      108 |  416 | `					iNest = 1;` |
|      108 |  417 | `					pStream->zText++;` |
|        - |  418 | `					/* TICKET 1433-40: Hnadle braces'{}' in double quoted string where everything is allowed */` |
|     1138 |  419 | `					while(pStream->zText < pStream->zEnd ){` |
|     1138 |  420 | `						if( pStream->zText[0] == '{' ){` |
|        9 |  421 | `							iNest++;` |
|     1134 |  422 | `						}else if (pStream->zText[0] == '}' ){` |
|      116 |  423 | `							iNest--;` |
|      116 |  424 | `							if( iNest <= 0 ){` |
|      108 |  425 | `								pStream->zText++;` |
|      108 |  426 | `								break;` |
|        1 |  427 | `							}` |
|     1020 |  428 | `						}else if( pStream->zText[0] == '\n' ){` |
|      ! 0 |  429 | `							pStream->nLine++;` |
|      ! 0 |  430 | `						}` |
|     1032 |  431 | `						pStream->zText++;` |
|        2 |  432 | `					}` |
|      108 |  433 | `					if( pStream->zText >= pStream->zEnd ){` |
|      ! 0 |  434 | `						break;` |
|        - |  435 | `					}` |
|       53 |  436 | `				}` |
|   179872 |  437 | `				if( pStream->zText[0] == '"' ){` |
|    20066 |  438 | `					if( pStream->zText[-1] != '\\' ){` |
|    19930 |  439 | `						break;` |
|      ! 0 |  440 | `					}else{` |
|      138 |  441 | `						const unsigned char *zPtr = &pStream->zText[-2];` |
|      138 |  442 | `						sxi32 i = 1;` |
|      190 |  443 | `						while( zPtr > pStream->zInput && zPtr[0] == '\\' ){` |
|       54 |  444 | `							zPtr--;` |
|       54 |  445 | `							i++;` |
|        2 |  446 | `						}` |
|      138 |  447 | `						if((i&1)==0){` |
|        5 |  448 | `							break;` |
|        - |  449 | `						}` |
|        - |  450 | `					}` |
|       66 |  451 | `				}` |
|   159940 |  452 | `				if( pStream->zText[0] == '\n' ){` |
|        7 |  453 | `					pStream->nLine++;` |
|        3 |  454 | `				}` |
|   159940 |  455 | `				pStream->zText++;` |
|        2 |  456 | `			}` |
|        - |  457 | `			/* Record token length and type */` |
|    19934 |  458 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|    19934 |  459 | `			pToken->nType = PH7_TK_DSTR;` |
|        - |  460 | `			/* Jump the trailing quote */` |
|    19934 |  461 | `			pStream->zText++;` |
|    19934 |  462 | `			return SXRET_OK;` |
|        - |  463 | `				  }` |
|        2 |  464 | ``		case '`':{`` |
|        - |  465 | `			/* Backtick quoted string */` |
|        5 |  466 | `			pStr->zString++;` |
|       45 |  467 | `			while( pStream->zText < pStream->zEnd ){` |
|       45 |  468 | ``				if( pStream->zText[0] == '`' && pStream->zText[-1] != '\\' ){`` |
|        5 |  469 | `					break;` |
|        - |  470 | `				}` |
|       41 |  471 | `				if( pStream->zText[0] == '\n' ){` |
|      ! 0 |  472 | `					pStream->nLine++;` |
|      ! 0 |  473 | `				}` |
|       41 |  474 | `				pStream->zText++;` |
|        1 |  475 | `			}` |
|        - |  476 | `			/* Record token length and type */` |
|        5 |  477 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|        5 |  478 | `			pToken->nType = PH7_TK_BSTR;` |
|        - |  479 | `			/* Jump the trailing backtick */` |
|        5 |  480 | `			pStream->zText++;` |
|        5 |  481 | `			return SXRET_OK;` |
|        - |  482 | `				  }` |
|      222 |  483 | `		case '\\': pToken->nType = PH7_TK_NSSEP;  break;` |
|     1674 |  484 | `		case ':':` |
|     3350 |  485 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == ':' ){` |
|        - |  486 | `				/* Current operator: '::' */` |
|      244 |  487 | `				pStream->zText++;` |
|      123 |  488 | `			}else{` |
|     3108 |  489 | `				pToken->nType = PH7_TK_COLON; /* Single colon */` |
|        - |  490 | `			}` |
|     3350 |  491 | `			break;` |
|    95770 |  492 | `		case ',': pToken->nType \|= PH7_TK_COMMA;  break; /* Comma is also an operator */` |
|   685784 |  493 | `		case ';': pToken->nType = PH7_TK_SEMI;    break;` |
|        - |  494 | `			/* Handle combined operators [i.e: +=,===,!=== ...] */` |
|   181909 |  495 | `		case '=':` |
|   363820 |  496 | `			pToken->nType \|= PH7_TK_EQUAL;` |
|   363820 |  497 | `			if( pStream->zText < pStream->zEnd ){` |
|   363820 |  498 | `				if( pStream->zText[0] == '=' ){` |
|    20392 |  499 | `					pToken->nType &= ~PH7_TK_EQUAL;` |
|        - |  500 | `					/* Current operator: == */` |
|    20392 |  501 | `					pStream->zText++;` |
|    20392 |  502 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  503 | `						/* Current operator: === */` |
|     4408 |  504 | `						pStream->zText++;` |
|     2205 |  505 | `					}` |
|   353625 |  506 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  507 | `					/* Array operator: => */` |
|     4962 |  508 | `					pToken->nType = PH7_TK_ARRAY_OP;` |
|     4962 |  509 | `					pStream->zText++;` |
|     2482 |  510 | `				}else{` |
|        - |  511 | `					/* TICKET 1433-0010: Reference operator '=&' */` |
|   338470 |  512 | `					const unsigned char *zCur = pStream->zText;` |
|   338470 |  513 | `					sxu32 nLine = 0;` |
|   676850 |  514 | `					while( zCur < pStream->zEnd && zCur[0] < 0xc0 && SyisSpace(zCur[0]) ){` |
|   338382 |  515 | `						if( zCur[0] == '\n' ){` |
|        5 |  516 | `							nLine++;` |
|        2 |  517 | `						}` |
|   338382 |  518 | `						zCur++;` |
|        2 |  519 | `					}` |
|   338470 |  520 | `					if( zCur < pStream->zEnd && zCur[0] == '&' ){` |
|        - |  521 | `						/* Current operator: =& */` |
|       50 |  522 | `						pToken->nType &= ~PH7_TK_EQUAL;` |
|       50 |  523 | `						SyStringInitFromBuf(pStr,"=&",sizeof("=&")-1);` |
|        - |  524 | `						/* Update token stream */` |
|       50 |  525 | `						pStream->zText = &zCur[1];` |
|       50 |  526 | `						pStream->nLine += nLine;` |
|       24 |  527 | `					}` |
|        - |  528 | `				}` |
|   181909 |  529 | `			}` |
|   363820 |  530 | `			break;` |
|    22318 |  531 | `		case '!':` |
|    44638 |  532 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  533 | `				/* Current operator: != */` |
|    18996 |  534 | `				pStream->zText++;` |
|    18996 |  535 | `				if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  536 | `					/* Current operator: !== */` |
|    15830 |  537 | `					pStream->zText++;` |
|     7914 |  538 | `				}` |
|     9497 |  539 | `			}` |
|    44638 |  540 | `			break;` |
|    12815 |  541 | `		case '&':` |
|    25632 |  542 | `			pToken->nType \|= PH7_TK_AMPER;` |
|    25632 |  543 | `			if( pStream->zText < pStream->zEnd ){` |
|    25632 |  544 | `				if( pStream->zText[0] == '&' ){` |
|     9822 |  545 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|        - |  546 | `					/* Current operator: && */` |
|     9822 |  547 | `					pStream->zText++;` |
|    20722 |  548 | `				}else if( pStream->zText[0] == '=' ){` |
|        7 |  549 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|        - |  550 | `					/* Current operator: &= */` |
|        7 |  551 | `					pStream->zText++;` |
|        3 |  552 | `				}` |
|    12815 |  553 | `			}` |
|    25632 |  554 | `			break;` |
|     1709 |  555 | `		case '\|':` |
|     3420 |  556 | `			if( pStream->zText < pStream->zEnd ){` |
|     3420 |  557 | `				if( pStream->zText[0] == '\|' ){` |
|        - |  558 | `					/* Current operator: \|\| */` |
|     3284 |  559 | `					pStream->zText++;` |
|     1779 |  560 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  561 | `					/* Current operator: \|= */` |
|        7 |  562 | `					pStream->zText++;` |
|        3 |  563 | `				}` |
|     1709 |  564 | `			}` |
|     3420 |  565 | `			break;` |
|     8271 |  566 | `		case '+':` |
|    16544 |  567 | `			if( pStream->zText < pStream->zEnd ){` |
|    16542 |  568 | `				if( pStream->zText[0] == '+' ){` |
|        - |  569 | `					/* Current operator: ++ */` |
|    12872 |  570 | `					pStream->zText++;` |
|    10107 |  571 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  572 | `					/* Current operator: += */` |
|       48 |  573 | `					pStream->zText++;` |
|       23 |  574 | `				}` |
|     8270 |  575 | `			}` |
|    16544 |  576 | `			break;` |
|    85780 |  577 | `		case '-':` |
|   171562 |  578 | `			if( pStream->zText < pStream->zEnd ){` |
|   171562 |  579 | `				if( pStream->zText[0] == '-' ){` |
|        - |  580 | `					/* Current operator: -- */` |
|       29 |  581 | `					pStream->zText++;` |
|   171548 |  582 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  583 | `					/* Current operator: -= */` |
|       10 |  584 | `					pStream->zText++;` |
|   171530 |  585 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  586 | `					/* Current operator: -> */` |
|   170980 |  587 | `					pStream->zText++;` |
|    85489 |  588 | `				}` |
|    85780 |  589 | `			}` |
|   171562 |  590 | `			break;` |
|      170 |  591 | `		case '*':` |
|      342 |  592 | `			if( pStream->zText < pStream->zEnd ){` |
|      342 |  593 | `				if( pStream->zText[0] == '*' ){` |
|        - |  594 | `					/* Current operator: ** or **= */` |
|      135 |  595 | `					pStream->zText++;` |
|      135 |  596 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  597 | `						/* Current operator: **= */` |
|       23 |  598 | `						pStream->zText++;` |
|       12 |  599 | `					}` |
|      275 |  600 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  601 | `					/* Current operator: *= */` |
|       20 |  602 | `					pStream->zText++;` |
|        9 |  603 | `				}` |
|      170 |  604 | `			}` |
|      342 |  605 | `			break;` |
|       35 |  606 | `		case '/':` |
|       72 |  607 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  608 | `				/* Current operator: /= */` |
|        5 |  609 | `				pStream->zText++;` |
|        2 |  610 | `			}` |
|       72 |  611 | `			break;` |
|       26 |  612 | `		case '%':` |
|       54 |  613 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  614 | `				/* Current operator: %= */` |
|        3 |  615 | `				pStream->zText++;` |
|        1 |  616 | `			}` |
|       54 |  617 | `			break;` |
|       11 |  618 | `		case '^':` |
|       23 |  619 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  620 | `				/* Current operator: ^= */` |
|        9 |  621 | `				pStream->zText++;` |
|        4 |  622 | `			}` |
|       23 |  623 | `			break;` |
|    43193 |  624 | `		case '.':` |
|    86388 |  625 | `			if( pStream->zText + 1 < pStream->zEnd && pStream->zText[0] == '.' && pStream->zText[1] == '.' ){` |
|        - |  626 | `				/* Ellipsis: ... */` |
|      120 |  627 | `				pStream->zText += 2;` |
|      120 |  628 | `				pToken->nType = PH7_TK_ELLIPSIS;` |
|    86329 |  629 | `			}else if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  630 | `				/* Current operator: .= */` |
|     3206 |  631 | `				pStream->zText++;` |
|     1602 |  632 | `			}` |
|    86388 |  633 | `			break;` |
|    26916 |  634 | `		case '<':` |
|    53834 |  635 | `			if( pStream->zText < pStream->zEnd ){` |
|    53834 |  636 | `				if( pStream->zText[0] == '<' ){` |
|        - |  637 | `					/* Current operator: << */` |
|      134 |  638 | `					pStream->zText++;` |
|      134 |  639 | `					if( pStream->zText < pStream->zEnd ){` |
|      134 |  640 | `						if( pStream->zText[0] == '=' ){` |
|        - |  641 | `							/* Current operator: <<= */` |
|        9 |  642 | `							pStream->zText++;` |
|      130 |  643 | `						}else if( pStream->zText[0] == '<' ){` |
|        - |  644 | `							/* Current Token: <<<  */` |
|      112 |  645 | `							pStream->zText++;` |
|        - |  646 | `							/* This may be the beginning of a Heredoc/Nowdoc string,try to delimit it */` |
|      112 |  647 | `							rc = LexExtractHeredoc(&(*pStream),&(*pToken));` |
|      112 |  648 | `							if( rc == SXRET_OK ){` |
|        - |  649 | `								/* Here/Now doc successfuly extracted */` |
|      112 |  650 | `								return SXRET_OK;` |
|        - |  651 | `							}` |
|      ! 0 |  652 | `						}` |
|       12 |  653 | `					}` |
|    53713 |  654 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  655 | `					/* Current operator: <> */` |
|        5 |  656 | `					pStream->zText++;` |
|    53700 |  657 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  658 | `					/* Current operator: <= or <=> */` |
|      100 |  659 | `					pStream->zText++;` |
|      100 |  660 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '>' ){` |
|        - |  661 | `						/* Current operator: <=> */` |
|       51 |  662 | `						pStream->zText++;` |
|       25 |  663 | `					}` |
|       49 |  664 | `				}` |
|    26861 |  665 | `			}` |
|    53724 |  666 | `			break;` |
|     3252 |  667 | `		case '>':` |
|     6506 |  668 | `			if( pStream->zText < pStream->zEnd ){` |
|     6506 |  669 | `				if( pStream->zText[0] == '>' ){` |
|        - |  670 | `					/* Current operator: >> */` |
|       21 |  671 | `					pStream->zText++;` |
|       21 |  672 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  673 | `						/* Current operator: >>= */` |
|       11 |  674 | `						pStream->zText++;` |
|        6 |  675 | `					}` |
|     6496 |  676 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  677 | `					/* Current operator: >= */` |
|       84 |  678 | `					pStream->zText++;` |
|       41 |  679 | `				}` |
|     3252 |  680 | `			}` |
|     6506 |  681 | `			break;` |
|     1374 |  682 | `		case '?':` |
|     2750 |  683 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '?' ){` |
|        - |  684 | `				/* Null coalescing operator: ?? */` |
|      184 |  685 | `				pStream->zText++;` |
|      184 |  686 | `				if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  687 | `					/* Null coalescing assignment operator (PHP 7.4) */` |
|       64 |  688 | `					pStream->zText++;` |
|       31 |  689 | `				}` |
|     2713 |  690 | `			}else if( (pStream->zEnd - pStream->zText) >= 2` |
|     2568 |  691 | `				&& pStream->zText[0] == '-' && pStream->zText[1] == '>' ){` |
|        - |  692 | `				/* Nullsafe object operator (PHP 8.0): ?-> */` |
|      110 |  693 | `				pStream->zText += 2;` |
|       54 |  694 | `			}` |
|     2748 |  695 | `			break;` |
|      112 |  696 | `		default:` |
|      224 |  697 | `			break;` |
|        - |  698 | `		}` |
|  5568834 |  699 | `		if( pStr->nByte <= 0 ){` |
|        - |  700 | `			/* Record token length */` |
|  5568786 |  701 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  2784392 |  702 | `		}` |
|  5568834 |  703 | `		if( pToken->nType & PH7_TK_OP ){` |
|        - |  704 | `			const ph7_expr_op *pOp;` |
|        - |  705 | `			/* Check if the extracted token is an operator */` |
|   947950 |  706 | `			pOp = PH7_ExprExtractOperator(pStr,(SyToken *)SySetPeek(pStream->pSet));` |
|   947950 |  707 | `			if( pOp == 0 ){` |
|        - |  708 | `				/* Not an operator */` |
|      ! 0 |  709 | `				pToken->nType &= ~PH7_TK_OP;` |
|      ! 0 |  710 | `				if( pToken->nType <= 0 ){` |
|      ! 0 |  711 | `					pToken->nType = PH7_TK_OTHER;` |
|      ! 0 |  712 | `				}` |
|      ! 0 |  713 | `			}else{` |
|        - |  714 | `				/* Save the instance associated with this operator for later processing */` |
|   947950 |  715 | `				pToken->pUserData = (void *)pOp;` |
|        - |  716 | `			}` |
|   473974 |  717 | `		}` |
|        - |  718 | `	}` |
|        - |  719 | `	/* Tell the upper-layer to save the extracted token for later processing */` |
|  9076116 |  720 | `	return SXRET_OK;` |
|  4690910 |  721 |  |
|        - |  722 | `/***** This file contains automatically generated code ******` |
|        - |  723 | `**` |
|        - |  724 | `** The code in this file has been automatically generated by` |
|        - |  725 | `**` |
|        - |  726 | `**     $Header: /sqlite/sqlite/tool/mkkeywordhash.c` |
|        - |  727 | `**` |
|        - |  728 | `** Sligthly modified by Chems mrad <chm@symisc.net> for the PH7 engine.` |
|        - |  729 | `**` |
|        - |  730 | `** The code in this file implements a function that determines whether` |
|        - |  731 | `** or not a given identifier is really a PHP keyword.  The same thing` |
|        - |  732 | `** might be implemented more directly using a hand-written hash table.` |
|        - |  733 | `** But by using this automatically generated code, the size of the code` |
|        - |  734 | `** is substantially reduced.  This is important for embedded applications` |
|        - |  735 | `** on platforms with limited memory.` |
|        - |  736 | `*/` |
|        - |  737 | `/* Hash score: 103 */` |
|  3507284 |  738 | `static sxu32 KeywordCode(const char *z, int n){` |
|        - |  739 | `  /* zText[] encodes 532 bytes of keywords in 333 bytes */` |
|        - |  740 | `  /*   extendswitchprintegerequire_oncenddeclareturnamespacechobject      */` |
|        - |  741 | `  /*   hrowbooleandefaultrycaselfinalistaticlonewconstringlobaluse        */` |
|        - |  742 | `  /*   lseifloatvarrayANDIEchoUSECHOabstractclasscontinuendifunction      */` |
|        - |  743 | `  /*   diendwhilevaldoexitgotoimplementsinclude_oncemptyinstanceof        */` |
|        - |  744 | `  /*   interfacendforeachissetparentprivateprotectedpublicatchunset       */` |
|        - |  745 | `  /*   xorARRAYASArrayEXITUNSETXORbreak                                   */` |
|        - |  746 | `  static const char zText[332] = {` |
|        - |  747 | `    'e','x','t','e','n','d','s','w','i','t','c','h','p','r','i','n','t','e',` |
|        - |  748 | `    'g','e','r','e','q','u','i','r','e','_','o','n','c','e','n','d','d','e',` |
|        - |  749 | `    'c','l','a','r','e','t','u','r','n','a','m','e','s','p','a','c','e','c',` |
|        - |  750 | `    'h','o','b','j','e','c','t','h','r','o','w','b','o','o','l','e','a','n',` |
|        - |  751 | `    'd','e','f','a','u','l','t','r','y','c','a','s','e','l','f','i','n','a',` |
|        - |  752 | `    'l','i','s','t','a','t','i','c','l','o','n','e','w','c','o','n','s','t',` |
|        - |  753 | `    'r','i','n','g','l','o','b','a','l','u','s','e','l','s','e','i','f','l',` |
|        - |  754 | `    'o','a','t','v','a','r','r','a','y','A','N','D','I','E','c','h','o','U',` |
|        - |  755 | `    'S','E','C','H','O','a','b','s','t','r','a','c','t','c','l','a','s','s',` |
|        - |  756 | `    'c','o','n','t','i','n','u','e','n','d','i','f','u','n','c','t','i','o',` |
|        - |  757 | `    'n','d','i','e','n','d','w','h','i','l','e','v','a','l','d','o','e','x',` |
|        - |  758 | `    'i','t','g','o','t','o','i','m','p','l','e','m','e','n','t','s','i','n',` |
|        - |  759 | `    'c','l','u','d','e','_','o','n','c','e','m','p','t','y','i','n','s','t',` |
|        - |  760 | `    'a','n','c','e','o','f','i','n','t','e','r','f','a','c','e','n','d','f',` |
|        - |  761 | `    'o','r','e','a','c','h','i','s','s','e','t','p','a','r','e','n','t','p',` |
|        - |  762 | `    'r','i','v','a','t','e','p','r','o','t','e','c','t','e','d','p','u','b',` |
|        - |  763 | `    'l','i','c','a','t','c','h','u','n','s','e','t','x','o','r','A','R','R',` |
|        - |  764 | `    'A','Y','A','S','A','r','r','a','y','E','X','I','T','U','N','S','E','T',` |
|        - |  765 | `    'X','O','R','b','r','e','a','k'` |
|        - |  766 | `  };` |
|        - |  767 | `  static const unsigned char aHash[151] = {` |
|        - |  768 |  |
|        - |  769 |  |
|        - |  770 | `      74,   0,  51,   0,  76,   0,   0,  20,   0,   0,   0,  50,   0,` |
|        - |  771 | `      80,  34,   0,  36,   0,   0,  64,  16,   0,   0,  17,   0,   1,` |
|        - |  772 | `      19,  84,  66,   0,  43,  45,  78,   0,   0,  53,  56,   0,   0,` |
|        - |  773 |  |
|        - |  774 | `      72,  14,   0,  71,   0,  38,   6,   0,   0,   0,  73,   0,   0,` |
|        - |  775 | `       3,   0,  41,   5,  52,  57,  32,   0,  60,  63,   0,  69,  82,` |
|        - |  776 | `      30,   0,  79,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,` |
|        - |  777 |  |
|        - |  778 | `      62,   0,  11,   0,   0,  58,   0,   0,   0,   0,  59,  75,   0,` |
|        - |  779 |  |
|        - |  780 | `  };` |
|        - |  781 | `  static const unsigned char aNext[84] = {` |
|        - |  782 |  |
|        - |  783 |  |
|        - |  784 |  |
|        - |  785 |  |
|        - |  786 |  |
|        - |  787 |  |
|        - |  788 | `      42,   0,   0,   0,  70,  55` |
|        - |  789 | `  };` |
|        - |  790 | `  static const unsigned char aLen[84] = {` |
|        - |  791 | `       7,   9,   6,   5,   7,  12,   7,   2,  10,   7,   6,   9,   4,` |
|        - |  792 | `       6,   5,   7,   4,   3,   7,   3,   4,   4,   5,   4,   6,   5,` |
|        - |  793 | `       2,   3,   5,   6,   6,   3,   6,   4,   2,   5,   3,   5,   3,` |
|        - |  794 | `       3,   4,   3,   4,   8,   5,   2,   8,   5,   8,   3,   8,   5,` |
|        - |  795 | `       4,   2,   4,   4,  10,  12,   7,   5,  10,   9,   3,   6,  10,` |
|        - |  796 | `       3,   7,   2,   5,   6,   7,   9,   6,   5,   5,   3,   5,   2,` |
|        - |  797 | `       5,   4,   5,   3,   2,   5` |
|        - |  798 | `  };` |
|        - |  799 | `  static const sxu16 aOffset[84] = {` |
|        - |  800 |  |
|        - |  801 | `      55,  60,  65,  65,  70,  72,  78,  81,  83,  86,  90,  92,  97,` |
|        - |  802 | `     100, 100, 103, 106, 111, 117, 119, 119, 123, 124, 129, 130, 135,` |
|        - |  803 | `     137, 139, 143, 145, 149, 157, 159, 162, 169, 173, 181, 183, 186,` |
|        - |  804 | `     190, 194, 196, 200, 204, 214, 214, 225, 230, 240, 240, 248, 248,` |
|        - |  805 | `     251, 251, 252, 258, 263, 269, 276, 285, 290, 295, 300, 303, 308,` |
|        - |  806 | `     310, 315, 319, 324, 325, 327` |
|        - |  807 | `  };` |
|        - |  808 | `  static const sxu32 aCode[84] = {` |
|        - |  809 | `    PH7_TKWRD_EXTENDS,   PH7_TKWRD_ENDSWITCH,   PH7_TKWRD_SWITCH,    PH7_TKWRD_PRINT,   PH7_TKWRD_INT,` |
|        - |  810 | `    PH7_TKWRD_REQONCE,   PH7_TKWRD_REQUIRE,     PH7_TKWRD_SEQ,       PH7_TKWRD_ENDDEC,    PH7_TKWRD_DECLARE,` |
|        - |  811 | `    PH7_TKWRD_RETURN,    PH7_TKWRD_NAMESPACE,   PH7_TKWRD_ECHO,      PH7_TKWRD_OBJECT,    PH7_TKWRD_THROW,` |
|        - |  812 | `    PH7_TKWRD_BOOL,      PH7_TKWRD_BOOL,        PH7_TKWRD_AND,       PH7_TKWRD_DEFAULT,   PH7_TKWRD_TRY,` |
|        - |  813 | `    PH7_TKWRD_CASE,      PH7_TKWRD_SELF,        PH7_TKWRD_FINAL,     PH7_TKWRD_LIST,      PH7_TKWRD_STATIC,` |
|        - |  814 | `    PH7_TKWRD_CLONE,     PH7_TKWRD_SNE,         PH7_TKWRD_NEW,       PH7_TKWRD_CONST,     PH7_TKWRD_STRING,` |
|        - |  815 | `    PH7_TKWRD_GLOBAL,    PH7_TKWRD_USE,         PH7_TKWRD_ELIF,      PH7_TKWRD_ELSE,      PH7_TKWRD_IF,` |
|        - |  816 | `    PH7_TKWRD_FLOAT,     PH7_TKWRD_VAR,         PH7_TKWRD_ARRAY,     PH7_TKWRD_AND,       PH7_TKWRD_DIE,` |
|        - |  817 | `    PH7_TKWRD_ECHO,      PH7_TKWRD_USE,         PH7_TKWRD_ECHO,      PH7_TKWRD_ABSTRACT,  PH7_TKWRD_CLASS,` |
|        - |  818 | `    PH7_TKWRD_AS,        PH7_TKWRD_CONTINUE,    PH7_TKWRD_ENDIF,     PH7_TKWRD_FUNCTION,  PH7_TKWRD_DIE,` |
|        - |  819 | `    PH7_TKWRD_ENDWHILE,  PH7_TKWRD_WHILE,       PH7_TKWRD_EVAL,      PH7_TKWRD_DO,        PH7_TKWRD_EXIT,` |
|        - |  820 | `    PH7_TKWRD_GOTO,      PH7_TKWRD_IMPLEMENTS,  PH7_TKWRD_INCONCE,   PH7_TKWRD_INCLUDE,   PH7_TKWRD_EMPTY,` |
|        - |  821 | `    PH7_TKWRD_INSTANCEOF,PH7_TKWRD_INTERFACE,   PH7_TKWRD_INT,       PH7_TKWRD_ENDFOR,    PH7_TKWRD_END4EACH,` |
|        - |  822 | `    PH7_TKWRD_FOR,       PH7_TKWRD_FOREACH,     PH7_TKWRD_OR,        PH7_TKWRD_ISSET,     PH7_TKWRD_PARENT,` |
|        - |  823 | `    PH7_TKWRD_PRIVATE,   PH7_TKWRD_PROTECTED,   PH7_TKWRD_PUBLIC,    PH7_TKWRD_CATCH,     PH7_TKWRD_UNSET,` |
|        - |  824 | `    PH7_TKWRD_XOR,       PH7_TKWRD_ARRAY,       PH7_TKWRD_AS,        PH7_TKWRD_ARRAY,     PH7_TKWRD_EXIT,` |
|        - |  825 | `    PH7_TKWRD_UNSET,     PH7_TKWRD_XOR,         PH7_TKWRD_OR,        PH7_TKWRD_BREAK` |
|        - |  826 | `  };` |
|        - |  827 | `  int h, i;` |
|  3507284 |  828 | `  if( n<2 ) return PH7_TK_ID;` |
|  3398042 |  829 | `  h = (((int)z[0]*4) ^ ((int)z[n-1]*3) ^ n) % 151;` |
|  5151256 |  830 | `  for(i=((int)aHash[h])-1; i>=0; i=((int)aNext[i])-1){` |
|  3021112 |  831 | `    if( (int)aLen[i]==n && SyMemcmp(&zText[aOffset[i]],z,n)==0 ){` |
|        - |  832 | `       /* PH7_TKWRD_EXTENDS */` |
|        - |  833 | `       /* PH7_TKWRD_ENDSWITCH */` |
|        - |  834 | `       /* PH7_TKWRD_SWITCH */` |
|        - |  835 | `       /* PH7_TKWRD_PRINT */` |
|        - |  836 | `       /* PH7_TKWRD_INT */` |
|        - |  837 | `       /* PH7_TKWRD_REQONCE */` |
|        - |  838 | `       /* PH7_TKWRD_REQUIRE */` |
|        - |  839 | `       /* PH7_TKWRD_SEQ */` |
|        - |  840 | `       /* PH7_TKWRD_ENDDEC */` |
|        - |  841 | `       /* PH7_TKWRD_DECLARE */` |
|        - |  842 | `       /* PH7_TKWRD_RETURN */` |
|        - |  843 | `       /* PH7_TKWRD_NAMESPACE */` |
|        - |  844 | `       /* PH7_TKWRD_ECHO */` |
|        - |  845 | `       /* PH7_TKWRD_OBJECT */` |
|        - |  846 | `       /* PH7_TKWRD_THROW */` |
|        - |  847 | `       /* PH7_TKWRD_BOOL */` |
|        - |  848 | `       /* PH7_TKWRD_BOOL */` |
|        - |  849 | `       /* PH7_TKWRD_AND */` |
|        - |  850 | `       /* PH7_TKWRD_DEFAULT */` |
|        - |  851 | `       /* PH7_TKWRD_TRY */` |
|        - |  852 | `       /* PH7_TKWRD_CASE */` |
|        - |  853 | `       /* PH7_TKWRD_SELF */` |
|        - |  854 | `       /* PH7_TKWRD_FINAL */` |
|        - |  855 | `       /* PH7_TKWRD_LIST */` |
|        - |  856 | `       /* PH7_TKWRD_STATIC */` |
|        - |  857 | `       /* PH7_TKWRD_CLONE */` |
|        - |  858 | `       /* PH7_TKWRD_SNE */` |
|        - |  859 | `       /* PH7_TKWRD_NEW */` |
|        - |  860 | `       /* PH7_TKWRD_CONST */` |
|        - |  861 | `       /* PH7_TKWRD_STRING */` |
|        - |  862 | `       /* PH7_TKWRD_GLOBAL */` |
|        - |  863 | `       /* PH7_TKWRD_USE */` |
|        - |  864 | `       /* PH7_TKWRD_ELIF */` |
|        - |  865 | `       /* PH7_TKWRD_ELSE */` |
|        - |  866 | `       /* PH7_TKWRD_IF */` |
|        - |  867 | `       /* PH7_TKWRD_FLOAT */` |
|        - |  868 | `       /* PH7_TKWRD_VAR */` |
|        - |  869 | `       /* PH7_TKWRD_ARRAY */` |
|        - |  870 | `       /* PH7_TKWRD_AND */` |
|        - |  871 | `       /* PH7_TKWRD_DIE */` |
|        - |  872 | `       /* PH7_TKWRD_ECHO */` |
|        - |  873 | `       /* PH7_TKWRD_USE */` |
|        - |  874 | `       /* PH7_TKWRD_ECHO */` |
|        - |  875 | `       /* PH7_TKWRD_ABSTRACT */` |
|        - |  876 | `       /* PH7_TKWRD_CLASS */` |
|        - |  877 | `       /* PH7_TKWRD_AS */` |
|        - |  878 | `       /* PH7_TKWRD_CONTINUE */` |
|        - |  879 | `       /* PH7_TKWRD_ENDIF */` |
|        - |  880 | `       /* PH7_TKWRD_FUNCTION */` |
|        - |  881 | `       /* PH7_TKWRD_DIE */` |
|        - |  882 | `       /* PH7_TKWRD_ENDWHILE */` |
|        - |  883 | `       /* PH7_TKWRD_WHILE */` |
|        - |  884 | `       /* PH7_TKWRD_EVAL */` |
|        - |  885 | `       /* PH7_TKWRD_DO */` |
|        - |  886 | `       /* PH7_TKWRD_EXIT */` |
|        - |  887 | `       /* PH7_TKWRD_GOTO */` |
|        - |  888 | `       /* PH7_TKWRD_IMPLEMENTS */` |
|        - |  889 | `       /* PH7_TKWRD_INCONCE */` |
|        - |  890 | `       /* PH7_TKWRD_INCLUDE */` |
|        - |  891 | `       /* PH7_TKWRD_EMPTY */` |
|        - |  892 | `       /* PH7_TKWRD_INSTANCEOF */` |
|        - |  893 | `       /* PH7_TKWRD_INTERFACE */` |
|        - |  894 | `       /* PH7_TKWRD_INT */` |
|        - |  895 | `       /* PH7_TKWRD_ENDFOR */` |
|        - |  896 | `       /* PH7_TKWRD_END4EACH */` |
|        - |  897 | `       /* PH7_TKWRD_FOR */` |
|        - |  898 | `       /* PH7_TKWRD_FOREACH */` |
|        - |  899 | `       /* PH7_TKWRD_OR */` |
|        - |  900 | `       /* PH7_TKWRD_ISSET */` |
|        - |  901 | `       /* PH7_TKWRD_PARENT */` |
|        - |  902 | `       /* PH7_TKWRD_PRIVATE */` |
|        - |  903 | `       /* PH7_TKWRD_PROTECTED */` |
|        - |  904 | `       /* PH7_TKWRD_PUBLIC */` |
|        - |  905 | `       /* PH7_TKWRD_CATCH */` |
|        - |  906 | `       /* PH7_TKWRD_UNSET */` |
|        - |  907 | `       /* PH7_TKWRD_XOR */` |
|        - |  908 | `       /* PH7_TKWRD_ARRAY */` |
|        - |  909 | `       /* PH7_TKWRD_AS */` |
|        - |  910 | `       /* PH7_TKWRD_ARRAY */` |
|        - |  911 | `       /* PH7_TKWRD_EXIT */` |
|        - |  912 | `       /* PH7_TKWRD_UNSET */` |
|        - |  913 | `       /* PH7_TKWRD_XOR */` |
|        - |  914 | `       /* PH7_TKWRD_OR */` |
|        - |  915 | `       /* PH7_TKWRD_BREAK */` |
|  1267898 |  916 | `      return aCode[i];` |
|        - |  917 | `    }` |
|   876607 |  918 | `  }` |
|        - |  919 | `  /* Linear fallback for keywords not in the auto-generated hash table */` |
|  2130146 |  920 | `  if( n==5 && SyMemcmp(z,"trait",5)==0 ) return PH7_TKWRD_TRAIT;` |
|  2130088 |  921 | `  if( n==9 && SyMemcmp(z,"insteadof",9)==0 ) return PH7_TKWRD_INSTEADOF;` |
|  2130084 |  922 | `  if( n==7 && SyMemcmp(z,"finally",7)==0 ) return PH7_TKWRD_FINALLY;` |
|  2130054 |  923 | `  if( n==5 && SyMemcmp(z,"yield",5)==0 ) return PH7_TKWRD_YIELD;` |
|  2130020 |  924 | `  if( n==5 && SyMemcmp(z,"match",5)==0 ) return PH7_TKWRD_MATCH;` |
|  2129950 |  925 | `  return PH7_TK_ID;` |
|  1753643 |  926 |  |
|        - |  927 | `/* --- End of Automatically generated code --- */` |
|        - |  928 | `/*` |
|        - |  929 | ` * Extract a heredoc/nowdoc text from a raw PHP input.` |
|        - |  930 | ` * According to the PHP language reference manual:` |
|        - |  931 | ` *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier` |
|        - |  932 | ` *  is provided, then a newline. The string itself follows, and then the same identifier again` |
|        - |  933 | ` *  to close the quotation.` |
|        - |  934 | ` *  The closing identifier must begin in the first column of the line. Also, the identifier must` |
|        - |  935 | ` *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric` |
|        - |  936 | ` *  characters and underscores, and must start with a non-digit character or underscore.` |
|        - |  937 | ` *  Heredoc text behaves just like a double-quoted string, without the double quotes.` |
|        - |  938 | ` *  This means that quotes in a heredoc do not need to be escaped, but the escape codes listed` |
|        - |  939 | ` *  above can still be used. Variables are expanded, but the same care must be taken when expressing` |
|        - |  940 | ` *  complex variables inside a heredoc as with strings.` |
|        - |  941 | ` *  Nowdocs are to single-quoted strings what heredocs are to double-quoted strings.` |
|        - |  942 | ` *  A nowdoc is specified similarly to a heredoc, but no parsing is done inside a nowdoc.` |
|        - |  943 | ` *  The construct is ideal for embedding PHP code or other large blocks of text without the need` |
|        - |  944 | ` *  for escaping. It shares some features in common with the SGML <![CDATA[ ]]> construct, in that` |
|        - |  945 | ` *  it declares a block of text which is not for parsing.` |
|        - |  946 | ` *  A nowdoc is identified with the same <<< sequence used for heredocs, but the identifier which follows` |
|        - |  947 | ` *  is enclosed in single quotes, e.g. <<<'EOT'. All the rules for heredoc identifiers also apply to nowdoc` |
|        - |  948 | ` *  identifiers, especially those regarding the appearance of the closing identifier.` |
|        - |  949 | ` * Symisc Extension:` |
|        - |  950 | ` * The closing delimiter can now start with a digit or undersocre or it can be an UTF-8 stream.` |
|        - |  951 | ` * Example:` |
|        - |  952 | ` *  <<<123` |
|        - |  953 | ` *    HEREDOC Here` |
|        - |  954 | ` * 123` |
|        - |  955 | ` *  or` |
|        - |  956 | ` *  <<<___` |
|        - |  957 | ` *   HEREDOC Here` |
|        - |  958 | ` *  ___` |
|        - |  959 | ` */` |
|      110 |  960 | `static sxi32 LexExtractHeredoc(SyStream *pStream,SyToken *pToken)` |
|        2 |  961 |  |
|      112 |  962 | `	const unsigned char *zIn  = pStream->zText;` |
|      112 |  963 | `	const unsigned char *zEnd = pStream->zEnd;` |
|        - |  964 | `	const unsigned char *zPtr;` |
|      112 |  965 | `	sxu8 bNowDoc = FALSE;` |
|        - |  966 | `	SyString sDelim;` |
|        - |  967 | `	SyString sStr;` |
|        - |  968 | `	/* Jump leading white spaces */` |
|      124 |  969 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|       13 |  970 | `		zIn++;` |
|        1 |  971 | `	}` |
|      112 |  972 | `	if( zIn >= zEnd ){` |
|        - |  973 | `		/* A simple symbol,return immediately */` |
|      ! 0 |  974 | `		return SXERR_CONTINUE;` |
|        - |  975 | `	}` |
|      112 |  976 | `	if( zIn[0] == '\'' \|\| zIn[0] == '"' ){` |
|        - |  977 | `		/* Make sure we are dealing with a nowdoc */` |
|       44 |  978 | `		bNowDoc =  zIn[0] == '\'' ? TRUE : FALSE;` |
|       44 |  979 | `		zIn++;` |
|       21 |  980 | `	}` |
|      112 |  981 | `	if( zIn[0] < 0xc0 && !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|        - |  982 | `		/* Invalid delimiter,return immediately */` |
|      ! 0 |  983 | `		return SXERR_CONTINUE;` |
|        - |  984 | `	}` |
|        - |  985 | `	/* Isolate the identifier */` |
|      112 |  986 | `	sDelim.zString = (const char *)zIn;` |
|      118 |  987 | `	for(;;){` |
|      238 |  988 | `		zPtr = zIn;` |
|        - |  989 | `		/* Skip alphanumeric stream */` |
|      756 |  990 | `		while( zPtr < zEnd && zPtr[0] < 0xc0 && (SyisAlphaNum(zPtr[0]) \|\| zPtr[0] == '_') ){` |
|      402 |  991 | `			zPtr++;` |
|        2 |  992 | `		}` |
|      238 |  993 | `		if( zPtr < zEnd && zPtr[0] >= 0xc0 ){` |
|       19 |  994 | `			zPtr++;` |
|        - |  995 | `			/* UTF-8 stream */` |
|       37 |  996 | `			while( zPtr < zEnd && ((zPtr[0] & 0xc0) == 0x80) ){` |
|       19 |  997 | `				zPtr++;` |
|        1 |  998 | `			}` |
|        9 |  999 | `		}` |
|      238 | 1000 | `		if( zPtr == zIn ){` |
|        - | 1001 | `			/* Not an UTF-8 or alphanumeric stream */` |
|      112 | 1002 | `			break;` |
|        - | 1003 | `		}` |
|        - | 1004 | `		/* Synchronize pointers */` |
|      128 | 1005 | `		zIn = zPtr;` |
|        2 | 1006 | `	}` |
|        - | 1007 | `	/* Get the identifier length */` |
|      112 | 1008 | `	sDelim.nByte = (sxu32)((const char *)zIn-sDelim.zString);` |
|      112 | 1009 | `	if( zIn[0] == '"' \|\| (bNowDoc && zIn[0] == '\'') ){` |
|        - | 1010 | `		/* Jump the trailing single quote */` |
|       44 | 1011 | `		zIn++;` |
|       21 | 1012 | `	}` |
|        - | 1013 | `	/* Jump trailing white spaces */` |
|      112 | 1014 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|      ! 0 | 1015 | `		zIn++;` |
|      ! 0 | 1016 | `	}` |
|      112 | 1017 | `	if( sDelim.nByte <= 0 \|\| zIn >= zEnd \|\| zIn[0] != '\n' ){` |
|        - | 1018 | `		/* Invalid syntax */` |
|      ! 0 | 1019 | `		return SXERR_CONTINUE;` |
|        - | 1020 | `	}` |
|      112 | 1021 | `	pStream->nLine++; /* Increment line counter */` |
|      112 | 1022 | `	zIn++;` |
|        - | 1023 | `	/* Isolate the delimited string */` |
|      112 | 1024 | `	sStr.zString = (const char *)zIn;` |
|        - | 1025 | `	/* PHP 7.3 flexible heredoc/nowdoc: the closing marker may be preceded` |
|        - | 1026 | `	 * by whitespace (spaces/tabs), and may be followed by any non-identifier` |
|        - | 1027 | `	 * character. The indent count is recorded in pToken->pUserData and the` |
|        - | 1028 | `	 * compile phase strips it from each body line. */` |
|        - | 1029 | `	{` |
|      112 | 1030 | `		const unsigned char *zMarkerLine = zIn; /* Start of marker's line (set on match) */` |
|      112 | 1031 | `		sxu32 nIndent = 0;` |
|      225 | 1032 | `		for(;;){` |
|      282 | 1033 | `			const unsigned char *zLineStart = zIn;` |
|        - | 1034 | `			/* Skip leading space/tab on this line */` |
|      806 | 1035 | `			while( zIn < zEnd && (zIn[0] == ' ' \|\| zIn[0] == '\t') ){` |
|      386 | 1036 | `				zIn++;` |
|        2 | 1037 | `			}` |
|      280 | 1038 | `			if( (sxu32)(zEnd - zIn) >= sDelim.nByte` |
|      281 | 1039 | `				&& SyMemcmp((const void *)sDelim.zString,(const void *)zIn,sDelim.nByte) == 0 ){` |
|        - | 1040 | `				int bIdentCont;` |
|      110 | 1041 | `				zPtr = &zIn[sDelim.nByte];` |
|        - | 1042 | `				/* Disambiguate: next byte must not continue an identifier.` |
|        - | 1043 | `				 * A leading byte >= 0xc0 starts a multi-byte UTF-8 sequence,` |
|        - | 1044 | `				 * which PHP identifiers may contain, so treat it as ident. */` |
|      110 | 1045 | `				if( zPtr >= zEnd ){` |
|      ! 0 | 1046 | `					bIdentCont = 0;` |
|      110 | 1047 | `				}else if( zPtr[0] >= 0xc0 ){` |
|      ! 0 | 1048 | `					bIdentCont = 1;` |
|      ! 0 | 1049 | `				}else{` |
|      110 | 1050 | `					bIdentCont = (SyisAlphaNum(zPtr[0]) \|\| zPtr[0] == '_');` |
|        - | 1051 | `				}` |
|      110 | 1052 | `				if( !bIdentCont ){` |
|        - | 1053 | `					/* Closing marker found */` |
|      110 | 1054 | `					nIndent = (sxu32)(zIn - zLineStart);` |
|      110 | 1055 | `					zMarkerLine = zLineStart;` |
|      110 | 1056 | `					pStream->zText = zPtr; /* Cursor right after identifier */` |
|      110 | 1057 | `					break;` |
|        - | 1058 | `				}` |
|      ! 0 | 1059 | `			}` |
|        - | 1060 | `			/* Not the closing marker on this line; walk to next newline */` |
|     2824 | 1061 | `			while( zIn < zEnd && zIn[0] != '\n' ){` |
|     2652 | 1062 | `				zIn++;` |
|        2 | 1063 | `			}` |
|      174 | 1064 | `			if( zIn >= zEnd ){` |
|        - | 1065 | `				/* End of input without finding the closing marker */` |
|        3 | 1066 | `				pStream->zText = pStream->zEnd;` |
|        3 | 1067 | `				zMarkerLine = zIn;` |
|        3 | 1068 | `				break;` |
|        - | 1069 | `			}` |
|      172 | 1070 | `			pStream->nLine++;` |
|      172 | 1071 | `			zIn++;` |
|        2 | 1072 | `		}` |
|        - | 1073 | `		/* Body runs from sStr.zString up to just before the marker line */` |
|      112 | 1074 | `		sStr.nByte = (sxu32)((const char *)zMarkerLine - sStr.zString);` |
|      112 | 1075 | `		pToken->nType = bNowDoc ? PH7_TK_NOWDOC : PH7_TK_HEREDOC;` |
|      112 | 1076 | `		SyStringDupPtr(&pToken->sData,&sStr);` |
|        - | 1077 | `		/* Strip exactly one line terminator that precedes the marker's line. */` |
|      110 | 1078 | `		if( pToken->sData.nByte > 0` |
|      108 | 1079 | `			&& pToken->sData.zString[pToken->sData.nByte - 1] == '\n' ){` |
|      102 | 1080 | `			pToken->sData.nByte--;` |
|      100 | 1081 | `			if( pToken->sData.nByte > 0` |
|      102 | 1082 | `				&& pToken->sData.zString[pToken->sData.nByte - 1] == '\r' ){` |
|      ! 0 | 1083 | `				pToken->sData.nByte--;` |
|      ! 0 | 1084 | `			}` |
|       50 | 1085 | `		}` |
|      112 | 1086 | `		pToken->pUserData = SX_INT_TO_PTR(nIndent);` |
|        - | 1087 | `	}` |
|        - | 1088 | `	/* All done */` |
|      112 | 1089 | `	return SXRET_OK;` |
|       57 | 1090 |  |
|        - | 1091 | `/*` |
|        - | 1092 | ` * Tokenize a raw PHP input.` |
|        - | 1093 | ` * This is the public tokenizer called by most code generator routines.` |
|        - | 1094 | ` */` |
|    15222 | 1095 | `PH7_PRIVATE sxi32 PH7_TokenizePHP(const char *zInput,sxu32 nLen,sxu32 nLineStart,SySet *pOut)` |
|        2 | 1096 |  |
|        - | 1097 | `	SyLex sLexer;` |
|        - | 1098 | `	sxi32 rc;` |
|        - | 1099 | `	/* Initialize the lexer */` |
|    15224 | 1100 | `	rc = SyLexInit(&sLexer,&(*pOut),TokenizePHP,0);` |
|    15224 | 1101 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1102 | `		return rc;` |
|        - | 1103 | `	}` |
|    15224 | 1104 | `	sLexer.sStream.nLine = nLineStart;` |
|        - | 1105 | `	/* Tokenize input */` |
|    15224 | 1106 | `	rc = SyLexTokenizeInput(&sLexer,zInput,nLen,0,0,0);` |
|        - | 1107 | `	/* Release the lexer */` |
|    15224 | 1108 | `	SyLexRelease(&sLexer);` |
|        - | 1109 | `	/* Tokenization result */` |
|    15224 | 1110 | `	return rc;` |
|     7613 | 1111 |  |
|        - | 1112 | `/*` |
|        - | 1113 | ` * High level public tokenizer.` |
|        - | 1114 | ` *  Tokenize the input into PHP tokens and raw tokens [i.e: HTML,XML,Raw text...].` |
|        - | 1115 | ` * According to the PHP language reference manual` |
|        - | 1116 | ` *   When PHP parses a file, it looks for opening and closing tags, which tell PHP` |
|        - | 1117 | ` *   to start and stop interpreting the code between them. Parsing in this manner allows` |
|        - | 1118 | ` *   PHP to be embedded in all sorts of different documents, as everything outside of a pair` |
|        - | 1119 | ` *   of opening and closing tags is ignored by the PHP parser. Most of the time you will see` |
|        - | 1120 | ` *   PHP embedded in HTML documents, as in this example.` |
|        - | 1121 | ` *   <?php echo 'While this is going to be parsed.'; ?>` |
|        - | 1122 | ` *   <p>This will also be ignored.</p>` |
|        - | 1123 | ` *   You can also use more advanced structures:` |
|        - | 1124 | ` *   Example #1 Advanced escaping` |
|        - | 1125 | ` * <?php` |
|        - | 1126 | ` * if ($expression) {` |
|        - | 1127 | ` *   ?>` |
|        - | 1128 | ` *   <strong>This is true.</strong>` |
|        - | 1129 | ` *   <?php` |
|        - | 1130 | ` * } else {` |
|        - | 1131 | ` *   ?>` |
|        - | 1132 | ` *   <strong>This is false.</strong>` |
|        - | 1133 | ` *   <?php` |
|        - | 1134 | ` * }` |
|        - | 1135 | ` * ?>` |
|        - | 1136 | ` * This works as expected, because when PHP hits the ?> closing tags, it simply starts outputting` |
|        - | 1137 | ` * whatever it finds (except for an immediately following newline - see instruction separation ) until it hits` |
|        - | 1138 | ` * another opening tag. The example given here is contrived, of course, but for outputting large blocks of text` |
|        - | 1139 | ` * dropping out of PHP parsing mode is generally more efficient than sending all of the text through echo() or print().` |
|        - | 1140 | ` * There are four different pairs of opening and closing tags which can be used in PHP. Three of those, <?php ?>` |
|        - | 1141 | ` * <script language="php"> </script>  and <? ?> are always available. The other two are short tags and ASP style` |
|        - | 1142 | ` * tags, and can be turned on and off from the php.ini configuration file. As such, while some people find short tags` |
|        - | 1143 | ` * and ASP style tags convenient, they are less portable, and generally not recommended.` |
|        - | 1144 | ` * Note:` |
|        - | 1145 | ` * Also note that if you are embedding PHP within XML or XHTML you will need to use the <?php ?> tags to remain` |
|        - | 1146 | ` * compliant with standards.` |
|        - | 1147 | ` * Example #2 PHP Opening and Closing Tags` |
|        - | 1148 | ` * 1.  <?php echo 'if you want to serve XHTML or XML documents, do it like this'; ?>` |
|        - | 1149 | ` * 2.  <script language="php">` |
|        - | 1150 | ` *       echo 'some editors (like FrontPage) don\'t` |
|        - | 1151 | ` *             like processing instructions';` |
|        - | 1152 | ` *   </script>` |
|        - | 1153 | ` *` |
|        - | 1154 | ` * 3.  <? echo 'this is the simplest, an SGML processing instruction'; ?>` |
|        - | 1155 | ` *   <?= expression ?> This is a shortcut for "<? echo expression ?>"` |
|        - | 1156 | ` */` |
|    12568 | 1157 | `PH7_PRIVATE sxi32 PH7_TokenizeRawText(const char *zInput,sxu32 nLen,SySet *pOut)` |
|        2 | 1158 |  |
|    12570 | 1159 | `	const char *zEnd = &zInput[nLen];` |
|    12570 | 1160 | `	const char *zIn  = zInput;` |
|        - | 1161 | `	const char *zCur,*zCurEnd;` |
|    12570 | 1162 | `	SyString sCtag = { 0, 0 };     /* Closing tag */` |
|        - | 1163 | `	SyToken sToken;` |
|        - | 1164 | `	SyString sDoc;` |
|        - | 1165 | `	sxu32 nLine;` |
|        - | 1166 | `	sxi32 iNest;` |
|        - | 1167 | `	sxi32 rc;` |
|        - | 1168 | `	/* Tokenize the input into PHP tokens and raw tokens */` |
|    12570 | 1169 | `	nLine = 1;` |
|    12570 | 1170 | `	zCur = zCurEnd   = 0; /* Prevent compiler warning */` |
|    12570 | 1171 | `	sToken.pUserData = 0;` |
|    12570 | 1172 | `	iNest = 0;` |
|    12570 | 1173 | `	sDoc.nByte = 0;` |
|    12570 | 1174 | `	sDoc.zString = ""; /* cc warning */` |
|    12570 | 1175 | `	for(;;){` |
|    25142 | 1176 | `		if( zIn >= zEnd ){` |
|        - | 1177 | `			/* End of input reached */` |
|    12538 | 1178 | `			break;` |
|        - | 1179 | `		}` |
|    12606 | 1180 | `		sToken.nLine = nLine;` |
|    12606 | 1181 | `		zCur = zIn;` |
|    12606 | 1182 | `		zCurEnd = 0;` |
|    12642 | 1183 | `		while( zIn < zEnd ){` |
|    12610 | 1184 | `			 if( zIn[0] == '<' ){` |
|    12574 | 1185 | `				const char *zTmp = zIn; /* End of raw input marker */` |
|    12574 | 1186 | `				zIn++;` |
|    12574 | 1187 | `				if( zIn < zEnd ){` |
|    12574 | 1188 | `					if( zIn[0] == '?' ){` |
|    12574 | 1189 | `						zIn++;` |
|    12574 | 1190 | `						if( (sxu32)(zEnd - zIn) >= sizeof("php")-1 &&  SyStrnicmp(zIn,"php",sizeof("php")-1) == 0 ){` |
|        - | 1191 | `							/* opening tag: <?php */` |
|    12572 | 1192 | `							zIn += sizeof("php")-1;` |
|     6285 | 1193 | `						}` |
|        - | 1194 | `						/* Look for the closing tag '?>' */` |
|    12574 | 1195 | `						SyStringInitFromBuf(&sCtag,"?>",sizeof("?>")-1);` |
|    12574 | 1196 | `						zCurEnd = zTmp;` |
|    12574 | 1197 | `						break;` |
|        - | 1198 | `					}` |
|      ! 0 | 1199 | `				}` |
|      ! 0 | 1200 | `			}else{` |
|       38 | 1201 | `				if( zIn[0] == '\n' ){` |
|       38 | 1202 | `					nLine++;` |
|       18 | 1203 | `				}` |
|       38 | 1204 | `				zIn++;` |
|        - | 1205 | `			 }` |
|        2 | 1206 | `		} /* While(zIn < zEnd) */` |
|    12606 | 1207 | `		if( zCurEnd == 0 ){` |
|       34 | 1208 | `			zCurEnd = zIn;` |
|       16 | 1209 | `		}` |
|        - | 1210 | `		/* Save the raw token */` |
|    12606 | 1211 | `		SyStringInitFromBuf(&sToken.sData,zCur,zCurEnd - zCur);` |
|    12606 | 1212 | `		sToken.nType = PH7_TOKEN_RAW;` |
|    12606 | 1213 | `		rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|    12606 | 1214 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1215 | `			return rc;` |
|        - | 1216 | `		}` |
|    12606 | 1217 | `		if( zIn >= zEnd ){` |
|       34 | 1218 | `			break;` |
|        - | 1219 | `		}` |
|        - | 1220 | `		/* Ignore leading white space */` |
|    27148 | 1221 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    14576 | 1222 | `			if( zIn[0] == '\n' ){` |
|    13380 | 1223 | `				nLine++;` |
|     6689 | 1224 | `			}` |
|    14576 | 1225 | `			zIn++;` |
|        2 | 1226 | `		}` |
|        - | 1227 | `		/* Delimit the PHP chunk */` |
|    12574 | 1228 | `		sToken.nLine = nLine;` |
|    12574 | 1229 | `		zCur = zIn;` |
|  1211056 | 1230 | `		while( (sxu32)(zEnd - zIn) >= sCtag.nByte ){` |
|        - | 1231 | `			const char *zPtr;` |
|  1205536 | 1232 | `			if( SyMemcmp(zIn,sCtag.zString,sCtag.nByte) == 0 && iNest < 1 ){` |
|     7052 | 1233 | `				break;` |
|        - | 1234 | `			}` |
|   601370 | 1235 | `			for(;;){` |
|  1202742 | 1236 | `				if( zIn[0] != '/' \|\| (zIn[1] != '*' && zIn[1] != '/') /* && sCtag.nByte >= 2 */ ){` |
|   599244 | 1237 | `					break;` |
|        - | 1238 | `				}` |
|     4258 | 1239 | `				zIn += 2;` |
|     4258 | 1240 | `				if( zIn[-1] == '/' ){` |
|        - | 1241 | `					/* Inline comment */` |
|   150814 | 1242 | `					while( zIn < zEnd && zIn[0] != '\n' ){` |
|   146642 | 1243 | `						zIn++;` |
|        2 | 1244 | `					}` |
|     4174 | 1245 | `					if( zIn >= zEnd ){` |
|      ! 0 | 1246 | `						zIn--;` |
|      ! 0 | 1247 | `					}` |
|     2088 | 1248 | `				}else{` |
|        - | 1249 | `					/* Block comment */` |
|     4530 | 1250 | `					while( (sxu32)(zEnd-zIn) >= sizeof("*/") - 1 ){` |
|     4530 | 1251 | `						if( zIn[0] == '*' && zIn[1] == '/' ){` |
|       86 | 1252 | `							zIn += 2;` |
|       86 | 1253 | `							break;` |
|        - | 1254 | `						}` |
|     4446 | 1255 | `						if( zIn[0] == '\n' ){` |
|       28 | 1256 | `							nLine++;` |
|       13 | 1257 | `						}` |
|     4446 | 1258 | `						zIn++;` |
|        2 | 1259 | `					}` |
|        - | 1260 | `				}` |
|        2 | 1261 | `			}` |
|  1198486 | 1262 | `			if( zIn[0] == '\n' ){` |
|    41620 | 1263 | `				nLine++;` |
|    41620 | 1264 | `				if( iNest > 0 ){` |
|      282 | 1265 | `					zIn++;` |
|      666 | 1266 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|      386 | 1267 | `						zIn++;` |
|        2 | 1268 | `					}` |
|      282 | 1269 | `					zPtr = zIn;` |
|     1440 | 1270 | `					while( zIn < zEnd ){` |
|     1440 | 1271 | `						if( (unsigned char)zIn[0] >= 0xc0 ){` |
|        - | 1272 | `							/* UTF-8 stream */` |
|       19 | 1273 | `							zIn++;` |
|       37 | 1274 | `							SX_JMP_UTF8(zIn,zEnd);` |
|     1430 | 1275 | `						}else if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|      142 | 1276 | `							break;` |
|      ! 0 | 1277 | `						}else{` |
|     1142 | 1278 | `							zIn++;` |
|        - | 1279 | `						}` |
|        2 | 1280 | `					}` |
|      282 | 1281 | `					if( (sxu32)(zIn - zPtr) == sDoc.nByte && SyMemcmp(sDoc.zString,zPtr,sDoc.nByte) == 0 ){` |
|      110 | 1282 | `						iNest = 0;` |
|       54 | 1283 | `					}` |
|      282 | 1284 | `					continue;` |
|        2 | 1285 | `				}` |
|  1177537 | 1286 | `			}else if ( (sxu32)(zEnd - zIn) >= sizeof("<<<") && zIn[0] == '<' && zIn[1] == '<' && zIn[2] == '<' && iNest < 1){` |
|      112 | 1287 | `				zIn += sizeof("<<<")-1;` |
|      124 | 1288 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|       13 | 1289 | `					zIn++;` |
|        1 | 1290 | `				}` |
|      112 | 1291 | `				if( zIn[0] == '"' \|\| zIn[0] == '\'' ){` |
|       44 | 1292 | `					zIn++;` |
|       21 | 1293 | `				}` |
|      112 | 1294 | `				zPtr = zIn;` |
|      530 | 1295 | `				while( zIn < zEnd ){` |
|      530 | 1296 | `					if( (unsigned char)zIn[0] >= 0xc0 ){` |
|        - | 1297 | `						/* UTF-8 stream */` |
|       19 | 1298 | `						zIn++;` |
|       37 | 1299 | `						SX_JMP_UTF8(zIn,zEnd);` |
|      520 | 1300 | `					}else if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       57 | 1301 | `						break;` |
|      ! 0 | 1302 | `					}else{` |
|      402 | 1303 | `						zIn++;` |
|        - | 1304 | `					}` |
|        2 | 1305 | `				}` |
|      112 | 1306 | `				SyStringInitFromBuf(&sDoc,zPtr,zIn-zPtr);` |
|      112 | 1307 | `				SyStringFullTrim(&sDoc);` |
|      112 | 1308 | `				if( sDoc.nByte > 0 ){` |
|      112 | 1309 | `					iNest++;` |
|       55 | 1310 | `				}` |
|      112 | 1311 | `				continue;` |
|        - | 1312 | `			}` |
|  1198096 | 1313 | `			zIn++;` |
|        - | 1314 |  |
|  1198096 | 1315 | `			if ( zIn >= zEnd )` |
|        3 | 1316 | `				break;` |
|        2 | 1317 | `		}` |
|    12574 | 1318 | `		if( (sxu32)(zEnd - zIn) < sCtag.nByte ){` |
|     5524 | 1319 | `			zIn = zEnd;` |
|     2761 | 1320 | `		}` |
|    12574 | 1321 | `		if( zCur < zIn ){` |
|        - | 1322 | `			/* Save the PHP chunk for later processing */` |
|     9978 | 1323 | `			sToken.nType = PH7_TOKEN_PHP;` |
|     9978 | 1324 | `			SyStringInitFromBuf(&sToken.sData,zCur,zIn-zCur);` |
|    19888 | 1325 | `			SyStringRightTrim(&sToken.sData); /* Trim trailing white spaces */` |
|     9978 | 1326 | `			rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|     9978 | 1327 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1328 | `				return rc;` |
|        - | 1329 | `			}` |
|     4988 | 1330 | `		}` |
|    12574 | 1331 | `		if( zIn < zEnd ){` |
|        - | 1332 | `			/* Jump the trailing closing tag */` |
|     7052 | 1333 | `			zIn += sCtag.nByte;` |
|     3525 | 1334 | `		}` |
|        2 | 1335 | `	} /* For(;;) */` |
|        - | 1336 |  |
|    12570 | 1337 | ` 	return SXRET_OK;` |
|     6286 | 1338 |  |
|        - | 1339 |  |
