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
| 10163038 |   19 | `static sxi32 TokenizePHP(SyStream *pStream,SyToken *pToken,void *pUserData,void *pCtxData)` |
|        5 |   20 |  |
|        - |   21 | `	SyString *pStr;` |
|        - |   22 | `	sxi32 rc;` |
|        - |   23 | `	/* Ignore leading white spaces */` |
| 15303999 |   24 | `	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){` |
|        - |   25 | `		/* Advance the stream cursor */` |
|  5140961 |   26 | `		if( pStream->zText[0] == '\n' ){` |
|        - |   27 | `			/* Update line counter */` |
|    37329 |   28 | `			pStream->nLine++;` |
|    18662 |   29 | `		}` |
|  5140961 |   30 | `		pStream->zText++;` |
|        5 |   31 | `	}` |
| 10163043 |   32 | `	if( pStream->zText >= pStream->zEnd ){` |
|        - |   33 | `		/* End of input reached */` |
|      ! 0 |   34 | `		return SXERR_EOF;` |
|        - |   35 | `	}` |
|        - |   36 | `	/* Record token starting position and line */` |
| 10163043 |   37 | `	pToken->nLine = pStream->nLine;` |
| 10163043 |   38 | `	pToken->pUserData = 0;` |
| 10163043 |   39 | `	pStr = &pToken->sData;` |
| 10163043 |   40 | `	SyStringInitFromBuf(pStr,pStream->zText,0);` |
| 12101839 |   41 | `	if( pStream->zText[0] >= 0xc0 \|\| SyisAlpha(pStream->zText[0]) \|\| pStream->zText[0] == '_' ){` |
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
|  3877597 |   52 | `		if( pStream->zText[0] < 0xc0 ){` |
|  3877581 |   53 | `			pStream->zText++;` |
|  1938788 |   54 | `		}` |
|  3819997 |   55 | `		for(;;){` |
|  7639999 |   56 | `			zIn = pStream->zText;` |
|  7639999 |   57 | `			if( zIn[0] >= 0xc0 ){` |
|       49 |   58 | `				zIn++;` |
|        - |   59 | `				/* UTF-8 stream */` |
|      109 |   60 | `				while( zIn < pStream->zEnd && ((zIn[0] & 0xc0) == 0x80) ){` |
|       61 |   61 | `					zIn++;` |
|        1 |   62 | `				}` |
|       24 |   63 | `			}` |
|        - |   64 | `			/* Skip alphanumeric stream */` |
| 32807892 |   65 | `			while( zIn < pStream->zEnd && zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_') ){` |
| 21347901 |   66 | `				zIn++;` |
|        5 |   67 | `			}` |
|  7639999 |   68 | `			if( zIn == pStream->zText ){` |
|        - |   69 | `				/* Not an UTF-8 or alphanumeric stream */` |
|  3877597 |   70 | `				break;` |
|        - |   71 | `			}` |
|        - |   72 | `			/* Synchronize pointers */` |
|  3762407 |   73 | `			pStream->zText = zIn;` |
|        5 |   74 | `		}` |
|        - |   75 | `		/* Record token length */` |
|  3877597 |   76 | `		pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  3877597 |   77 | `		nKeyword = KeywordCode(pStr->zString,(int)pStr->nByte);` |
|        - |   78 | `		/* PHP 7.4: 'fn' is a keyword reserved for arrow functions.` |
|        - |   79 | `		 * The auto-generated perfect hash above doesn't know about it,` |
|        - |   80 | `		 * so intercept the 'fn' identifier here.` |
|        - |   81 | `		 */` |
|  3878126 |   82 | `		if( nKeyword == PH7_TK_ID && pStr->nByte == 2` |
|  1228295 |   83 | `			&& pStr->zString[0] == 'f' && pStr->zString[1] == 'n' ){` |
|      395 |   84 | `			nKeyword = PH7_TKWRD_FN;` |
|      195 |   85 | `		}` |
|  3877597 |   86 | `		if( nKeyword != PH7_TK_ID ){` |
|  1430101 |   87 | `			if( nKeyword &` |
|        - |   88 | `				(PH7_TKWRD_NEW\|PH7_TKWRD_CLONE\|PH7_TKWRD_AND\|PH7_TKWRD_XOR\|PH7_TKWRD_OR\|PH7_TKWRD_INSTANCEOF\|PH7_TKWRD_SEQ\|PH7_TKWRD_SNE) ){` |
|        - |   89 | `					/* Alpha stream operators [i.e: new,clone,and,instanceof,eq,ne,or,xor],save the operator instance for later processing */` |
|    17979 |   90 | `					pToken->pUserData = (void *)PH7_ExprExtractOperator(pStr,0);` |
|        - |   91 | `					/* Mark as an operator */` |
|    17979 |   92 | `					pToken->nType = PH7_TK_ID\|PH7_TK_OP;` |
|     8992 |   93 | `			}else{` |
|        - |   94 | `				/* We are dealing with a keyword [i.e: while,foreach,class...],save the keyword ID */` |
|  1412127 |   95 | `				pToken->nType = PH7_TK_KEYWORD;` |
|  1412127 |   96 | `				pToken->pUserData = SX_INT_TO_PTR(nKeyword);` |
|        - |   97 | `			}` |
|   715053 |   98 | `		}else{` |
|        - |   99 | `			/* A simple identifier */` |
|  2447501 |  100 | `			pToken->nType = PH7_TK_ID;` |
|        - |  101 | `		}` |
|  1938801 |  102 | `	}else{` |
|        - |  103 | `		sxi32 c;` |
|        - |  104 | `		/* Non-alpha stream */` |
|  6285451 |  105 | `		if( pStream->zText[0] == '#' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '[' ){` |
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
|  6328866 |  170 | `		}else if( pStream->zText[0] == '#' \|\|` |
|  6285372 |  171 | `			( pStream->zText[0] == '/' &&  &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '/') ){` |
|     4417 |  172 | `				pStream->zText++;` |
|        - |  173 | `				/* Inline comments */` |
|   166103 |  174 | `				while( pStream->zText < pStream->zEnd && pStream->zText[0] != '\n' ){` |
|   161691 |  175 | `					pStream->zText++;` |
|        5 |  176 | `				}` |
|        - |  177 | `				/* Tell the upper-layer to ignore this token */` |
|     4417 |  178 | `				return SXERR_CONTINUE;` |
|  6280971 |  179 | `		}else if( pStream->zText[0] == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '*' ){` |
|    82495 |  180 | `			pStream->zText += 2;` |
|        - |  181 | `			/* Block comment */` |
|  2498541 |  182 | `			while( pStream->zText < pStream->zEnd ){` |
|  2498541 |  183 | `				if( pStream->zText[0] == '*' ){` |
|    82549 |  184 | `					if( &pStream->zText[1] >= pStream->zEnd \|\| pStream->zText[1] == '/'  ){` |
|    41250 |  185 | `						break;` |
|        - |  186 | `					}` |
|       27 |  187 | `				}` |
|  2416051 |  188 | `				if( pStream->zText[0] == '\n' ){` |
|       55 |  189 | `					pStream->nLine++;` |
|       25 |  190 | `				}` |
|  2416051 |  191 | `				pStream->zText++;` |
|        5 |  192 | `			}` |
|    82495 |  193 | `			pStream->zText += 2;` |
|        - |  194 | `			/* Tell the upper-layer to ignore this token */` |
|    82495 |  195 | `			return SXERR_CONTINUE;` |
|  6198481 |  196 | `		}else if( SyisDigit(pStream->zText[0]) ){` |
|   116565 |  197 | `			pStream->zText++;` |
|        - |  198 | `			/* PHP 7.4: handle underscore separator immediately following the first digit.` |
|        - |  199 | `			 * Check pStream->zText < pStream->zEnd BEFORE forming pStream->zText + 1 so` |
|        - |  200 | `			 * we never compute a pointer past one-past-end. */` |
|   116640 |  201 | `			if( pStream->zText < pStream->zEnd` |
|   116560 |  202 | `				&& pStream->zText[0] == '_'` |
|    58360 |  203 | `				&& pStream->zText + 1 < pStream->zEnd` |
|      160 |  204 | `				&& pStream->zText[1] < 0xc0` |
|      165 |  205 | `				&& SyisDigit(pStream->zText[1]) ){` |
|      151 |  206 | `				pStream->zText++; /* swallow underscore between two digits */` |
|       75 |  207 | `			}` |
|        - |  208 | `			/* Decimal digit stream (PHP 7.4: underscore separator allowed between two digits) */` |
|   127723 |  209 | `			while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|    11163 |  210 | `				pStream->zText++;` |
|    11244 |  211 | `				if( pStream->zText < pStream->zEnd` |
|    11158 |  212 | `					&& pStream->zText[0] == '_'` |
|     5665 |  213 | `					&& pStream->zText + 1 < pStream->zEnd` |
|      172 |  214 | `					&& pStream->zText[1] < 0xc0` |
|      177 |  215 | `					&& SyisDigit(pStream->zText[1]) ){` |
|      173 |  216 | `					pStream->zText++; /* swallow underscore between two digits */` |
|       86 |  217 | `				}` |
|        5 |  218 | `			}` |
|        - |  219 | `			/* Mark the token as integer until we encounter a real number */` |
|   116565 |  220 | `			pToken->nType = PH7_TK_INTEGER;` |
|   116565 |  221 | `			if( pStream->zText < pStream->zEnd ){` |
|   116565 |  222 | `				c = pStream->zText[0];` |
|   116565 |  223 | `				if( c == '.' ){` |
|        - |  224 | `					/* Real number (PHP 7.4: underscore separator allowed between two digits) */` |
|      531 |  225 | `					pStream->zText++;` |
|     1929 |  226 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|     1403 |  227 | `						pStream->zText++;` |
|     1404 |  228 | `						if( pStream->zText < pStream->zEnd` |
|     1398 |  229 | `							&& pStream->zText[0] == '_'` |
|      705 |  230 | `							&& pStream->zText + 1 < pStream->zEnd` |
|       12 |  231 | `							&& pStream->zText[1] < 0xc0` |
|       17 |  232 | `							&& SyisDigit(pStream->zText[1]) ){` |
|       13 |  233 | `							pStream->zText++;` |
|        6 |  234 | `						}` |
|        5 |  235 | `					}` |
|      531 |  236 | `					if( pStream->zText < pStream->zEnd ){` |
|      531 |  237 | `						c = pStream->zText[0];` |
|      531 |  238 | `						if( c=='e' \|\| c=='E' ){` |
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
|      263 |  258 | `					}` |
|      531 |  259 | `					pToken->nType = PH7_TK_REAL;` |
|   116302 |  260 | `				}else if( c=='e' \|\| c=='E' ){` |
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
|   116025 |  282 | `				}else if( c == 'x' \|\| c == 'X' ){` |
|        - |  283 | `					/* Hex digit stream (PHP 7.4: underscore separator allowed between two digits) */` |
|       75 |  284 | `					pStream->zText++;` |
|      371 |  285 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisHex(pStream->zText[0]) ){` |
|      297 |  286 | `						pStream->zText++;` |
|      320 |  287 | `						if( pStream->zText < pStream->zEnd` |
|      296 |  288 | `							&& pStream->zText[0] == '_'` |
|      172 |  289 | `							&& pStream->zText + 1 < pStream->zEnd` |
|       48 |  290 | `							&& pStream->zText[1] < 0xc0` |
|       49 |  291 | `							&& SyisHex(pStream->zText[1]) ){` |
|       49 |  292 | `							pStream->zText++;` |
|       24 |  293 | `						}` |
|        1 |  294 | `					}` |
|   115973 |  295 | `				}else if(c  == 'b' \|\| c == 'B' ){` |
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
|    58280 |  308 | `			}` |
|        - |  309 | `			/* PHP 7.4: absorb a trailing malformed underscore run into the` |
|        - |  310 | `			 * numeric token so the compile phase can emit a PHP-compatible` |
|        - |  311 | `			 * "syntax error, unexpected identifier" parse error. Valid` |
|        - |  312 | `			 * separators were already consumed by the per-loop peek logic` |
|        - |  313 | `			 * above, so an underscore here is always misplaced. */` |
|   116565 |  314 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '_' ){` |
|       18 |  315 | `				pStream->zText++;` |
|       44 |  316 | `				while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0` |
|       49 |  317 | `					&& (SyisAlphaNum(pStream->zText[0]) \|\| pStream->zText[0] == '_') ){` |
|       20 |  318 | `					pStream->zText++;` |
|        4 |  319 | `				}` |
|        7 |  320 | `			}` |
|        - |  321 | `			/* Record token length */` |
|   116565 |  322 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|   116565 |  323 | `			return SXRET_OK;` |
|        - |  324 | `		}` |
|  6081921 |  325 | `		c = pStream->zText[0];` |
|  6081921 |  326 | `		pStream->zText++; /* Advance the stream cursor */` |
|        - |  327 | `		/* Assume we are dealing with an operator*/` |
|  6081921 |  328 | `		pToken->nType = PH7_TK_OP;` |
|  6081921 |  329 | `		switch(c){` |
|  1226779 |  330 | `		case '$': pToken->nType = PH7_TK_DOLLAR; break;` |
|   524987 |  331 | `		case '{': pToken->nType = PH7_TK_OCB;    break;` |
|   524973 |  332 | `		case '}': pToken->nType = PH7_TK_CCB;    break;` |
|   941871 |  333 | `		case '(': pToken->nType = PH7_TK_LPAREN; break;` |
|    85639 |  334 | `		case '[': pToken->nType \|= PH7_TK_OSB;   break; /* Bitwise operation here,since the square bracket token '['` |
|        - |  335 | `														 * is a potential operator [i.e: subscripting] */` |
|    85645 |  336 | `		case ']': pToken->nType = PH7_TK_CSB;    break;` |
|   470925 |  337 | `		case ')': {` |
|   941855 |  338 | `			SySet *pTokSet = pStream->pSet;` |
|        - |  339 | `			/* Assemble type cast operators [i.e: (int),(float),(bool)...] */` |
|   941855 |  340 | `			if( pTokSet->nUsed >= 2 ){` |
|        - |  341 | `				SyToken *pTmp;` |
|        - |  342 | `				/* Peek the last recongnized token */` |
|   941853 |  343 | `				pTmp = (SyToken *)SySetPeek(pTokSet);` |
|   941853 |  344 | `				if( pTmp->nType & PH7_TK_KEYWORD ){` |
|    17077 |  345 | `					sxi32 nID = SX_PTR_TO_INT(pTmp->pUserData);` |
|    17077 |  346 | `					if( (sxu32)nID & (PH7_TKWRD_ARRAY\|PH7_TKWRD_INT\|PH7_TKWRD_FLOAT\|PH7_TKWRD_STRING\|PH7_TKWRD_OBJECT\|PH7_TKWRD_BOOL\|PH7_TKWRD_UNSET) ){` |
|    16825 |  347 | `						pTmp = (SyToken *)SySetAt(pTokSet,pTokSet->nUsed - 2);` |
|    16825 |  348 | `						if( pTmp->nType & PH7_TK_LPAREN ){` |
|        - |  349 | `							/* Merge the three tokens '(' 'TYPE' ')' into a single one */` |
|    16741 |  350 | `							const char * zTypeCast = "(int)";` |
|    16741 |  351 | `							if( nID & PH7_TKWRD_FLOAT ){` |
|     3313 |  352 | `								zTypeCast = "(float)";` |
|    15087 |  353 | `							}else if( nID & PH7_TKWRD_BOOL ){` |
|     3309 |  354 | `								zTypeCast = "(bool)";` |
|    11781 |  355 | `							}else if( nID & PH7_TKWRD_STRING ){` |
|     6625 |  356 | `								zTypeCast = "(string)";` |
|     6819 |  357 | `							}else if( nID & PH7_TKWRD_ARRAY ){` |
|       21 |  358 | `								zTypeCast = "(array)";` |
|     3499 |  359 | `							}else if( nID & PH7_TKWRD_OBJECT ){` |
|       17 |  360 | `								zTypeCast = "(object)";` |
|     3481 |  361 | `							}else if( nID & PH7_TKWRD_UNSET ){` |
|        7 |  362 | `								zTypeCast = "(unset)";` |
|        3 |  363 | `							}` |
|        - |  364 | `							/* Reflect the change */` |
|    16741 |  365 | `							pToken->nType = PH7_TK_OP;` |
|    16741 |  366 | `							SyStringInitFromBuf(&pToken->sData,zTypeCast,SyStrlen(zTypeCast));` |
|        - |  367 | `							/* Save the instance associated with the type cast operator */` |
|    16741 |  368 | `							pToken->pUserData = (void *)PH7_ExprExtractOperator(&pToken->sData,0);` |
|        - |  369 | `							/* Remove the two previous tokens */` |
|    16741 |  370 | `							pTokSet->nUsed -= 2;` |
|    16741 |  371 | `							return SXRET_OK;` |
|        - |  372 | `						}` |
|       42 |  373 | `					}` |
|      168 |  374 | `				}` |
|   462556 |  375 | `			}` |
|   925119 |  376 | `			pToken->nType = PH7_TK_RPAREN;` |
|   925119 |  377 | `			break;` |
|        - |  378 | `				  }` |
|    41789 |  379 | `		case '\'':{` |
|        - |  380 | `			/* Single quoted string */` |
|    83583 |  381 | `			pStr->zString++;` |
|   855537 |  382 | `			while( pStream->zText < pStream->zEnd ){` |
|   855537 |  383 | `				if( pStream->zText[0] == '\''  ){` |
|    83593 |  384 | `					if( pStream->zText[-1] != '\\' ){` |
|    83569 |  385 | `						break;` |
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
|   771959 |  398 | `				if( pStream->zText[0] == '\n' ){` |
|       67 |  399 | `					pStream->nLine++;` |
|       33 |  400 | `				}` |
|   771959 |  401 | `				pStream->zText++;` |
|        5 |  402 | `			}` |
|        - |  403 | `			/* Record token length and type */` |
|    83583 |  404 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|    83583 |  405 | `			pToken->nType = PH7_TK_SSTR;` |
|        - |  406 | `			/* Jump the trailing single quote */` |
|    83583 |  407 | `			pStream->zText++;` |
|    83583 |  408 | `			return SXRET_OK;` |
|        - |  409 | `				  }` |
|    10602 |  410 | `		case '"':{` |
|        - |  411 | `			sxi32 iNest;` |
|        - |  412 | `			/* Double quoted string */` |
|    21209 |  413 | `			pStr->zString++;` |
|   188571 |  414 | `			while( pStream->zText < pStream->zEnd ){` |
|   188571 |  415 | `				if( pStream->zText[0] == '{' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '$'){` |
|      113 |  416 | `					iNest = 1;` |
|      113 |  417 | `					pStream->zText++;` |
|        - |  418 | `					/* TICKET 1433-40: Hnadle braces'{}' in double quoted string where everything is allowed */` |
|     1177 |  419 | `					while(pStream->zText < pStream->zEnd ){` |
|     1177 |  420 | `						if( pStream->zText[0] == '{' ){` |
|        9 |  421 | `							iNest++;` |
|     1173 |  422 | `						}else if (pStream->zText[0] == '}' ){` |
|      121 |  423 | `							iNest--;` |
|      121 |  424 | `							if( iNest <= 0 ){` |
|      113 |  425 | `								pStream->zText++;` |
|      113 |  426 | `								break;` |
|        1 |  427 | `							}` |
|     1055 |  428 | `						}else if( pStream->zText[0] == '\n' ){` |
|      ! 0 |  429 | `							pStream->nLine++;` |
|      ! 0 |  430 | `						}` |
|     1067 |  431 | `						pStream->zText++;` |
|        3 |  432 | `					}` |
|      113 |  433 | `					if( pStream->zText >= pStream->zEnd ){` |
|      ! 0 |  434 | `						break;` |
|        - |  435 | `					}` |
|       55 |  436 | `				}` |
|   188571 |  437 | `				if( pStream->zText[0] == '"' ){` |
|    21401 |  438 | `					if( pStream->zText[-1] != '\\' ){` |
|    21205 |  439 | `						break;` |
|      ! 0 |  440 | `					}else{` |
|      201 |  441 | `						const unsigned char *zPtr = &pStream->zText[-2];` |
|      201 |  442 | `						sxi32 i = 1;` |
|      253 |  443 | `						while( zPtr > pStream->zInput && zPtr[0] == '\\' ){` |
|       55 |  444 | `							zPtr--;` |
|       55 |  445 | `							i++;` |
|        3 |  446 | `						}` |
|      201 |  447 | `						if((i&1)==0){` |
|        5 |  448 | `							break;` |
|        - |  449 | `						}` |
|        - |  450 | `					}` |
|       96 |  451 | `				}` |
|   167367 |  452 | `				if( pStream->zText[0] == '\n' ){` |
|        7 |  453 | `					pStream->nLine++;` |
|        3 |  454 | `				}` |
|   167367 |  455 | `				pStream->zText++;` |
|        5 |  456 | `			}` |
|        - |  457 | `			/* Record token length and type */` |
|    21209 |  458 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|    21209 |  459 | `			pToken->nType = PH7_TK_DSTR;` |
|        - |  460 | `			/* Jump the trailing quote */` |
|    21209 |  461 | `			pStream->zText++;` |
|    21209 |  462 | `			return SXRET_OK;` |
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
|      239 |  483 | `		case '\\': pToken->nType = PH7_TK_NSSEP;  break;` |
|     1824 |  484 | `		case ':':` |
|     3653 |  485 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == ':' ){` |
|        - |  486 | `				/* Current operator: '::' */` |
|      297 |  487 | `				pStream->zText++;` |
|      151 |  488 | `			}else{` |
|     3361 |  489 | `				pToken->nType = PH7_TK_COLON; /* Single colon */` |
|        - |  490 | `			}` |
|     3653 |  491 | `			break;` |
|   100961 |  492 | `		case ',': pToken->nType \|= PH7_TK_COMMA;  break; /* Comma is also an operator */` |
|   724565 |  493 | `		case ';': pToken->nType = PH7_TK_SEMI;    break;` |
|        - |  494 | `			/* Handle combined operators [i.e: +=,===,!=== ...] */` |
|   191324 |  495 | `		case '=':` |
|   382653 |  496 | `			pToken->nType \|= PH7_TK_EQUAL;` |
|   382653 |  497 | `			if( pStream->zText < pStream->zEnd ){` |
|   382653 |  498 | `				if( pStream->zText[0] == '=' ){` |
|    21465 |  499 | `					pToken->nType &= ~PH7_TK_EQUAL;` |
|        - |  500 | `					/* Current operator: == */` |
|    21465 |  501 | `					pStream->zText++;` |
|    21465 |  502 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  503 | `						/* Current operator: === */` |
|     4657 |  504 | `						pStream->zText++;` |
|     2331 |  505 | `					}` |
|   371923 |  506 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  507 | `					/* Array operator: => */` |
|     5267 |  508 | `					pToken->nType = PH7_TK_ARRAY_OP;` |
|     5267 |  509 | `					pStream->zText++;` |
|     2636 |  510 | `				}else{` |
|        - |  511 | `					/* TICKET 1433-0010: Reference operator '=&' */` |
|   355931 |  512 | `					const unsigned char *zCur = pStream->zText;` |
|   355931 |  513 | `					sxu32 nLine = 0;` |
|   711719 |  514 | `					while( zCur < pStream->zEnd && zCur[0] < 0xc0 && SyisSpace(zCur[0]) ){` |
|   355793 |  515 | `						if( zCur[0] == '\n' ){` |
|        5 |  516 | `							nLine++;` |
|        2 |  517 | `						}` |
|   355793 |  518 | `						zCur++;` |
|        5 |  519 | `					}` |
|   355931 |  520 | `					if( zCur < pStream->zEnd && zCur[0] == '&' ){` |
|        - |  521 | `						/* Current operator: =& */` |
|       55 |  522 | `						pToken->nType &= ~PH7_TK_EQUAL;` |
|       55 |  523 | `						SyStringInitFromBuf(pStr,"=&",sizeof("=&")-1);` |
|        - |  524 | `						/* Update token stream */` |
|       55 |  525 | `						pStream->zText = &zCur[1];` |
|       55 |  526 | `						pStream->nLine += nLine;` |
|       26 |  527 | `					}` |
|        - |  528 | `				}` |
|   191324 |  529 | `			}` |
|   382653 |  530 | `			break;` |
|    23482 |  531 | `		case '!':` |
|    46969 |  532 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  533 | `				/* Current operator: != */` |
|    19975 |  534 | `				pStream->zText++;` |
|    19975 |  535 | `				if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  536 | `					/* Current operator: !== */` |
|    16647 |  537 | `					pStream->zText++;` |
|     8321 |  538 | `				}` |
|     9985 |  539 | `			}` |
|    46969 |  540 | `			break;` |
|    13473 |  541 | `		case '&':` |
|    26951 |  542 | `			pToken->nType \|= PH7_TK_AMPER;` |
|    26951 |  543 | `			if( pStream->zText < pStream->zEnd ){` |
|    26951 |  544 | `				if( pStream->zText[0] == '&' ){` |
|    10333 |  545 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|        - |  546 | `					/* Current operator: && */` |
|    10333 |  547 | `					pStream->zText++;` |
|    21787 |  548 | `				}else if( pStream->zText[0] == '=' ){` |
|        7 |  549 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|        - |  550 | `					/* Current operator: &= */` |
|        7 |  551 | `					pStream->zText++;` |
|        3 |  552 | `				}` |
|    13473 |  553 | `			}` |
|    26951 |  554 | `			break;` |
|     1802 |  555 | `		case '\|':` |
|     3609 |  556 | `			if( pStream->zText < pStream->zEnd ){` |
|     3609 |  557 | `				if( pStream->zText[0] == '\|' ){` |
|        - |  558 | `					/* Current operator: \|\| */` |
|     3467 |  559 | `					pStream->zText++;` |
|     1878 |  560 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  561 | `					/* Current operator: \|= */` |
|        7 |  562 | `					pStream->zText++;` |
|        3 |  563 | `				}` |
|     1802 |  564 | `			}` |
|     3609 |  565 | `			break;` |
|     8696 |  566 | `		case '+':` |
|    17397 |  567 | `			if( pStream->zText < pStream->zEnd ){` |
|    17395 |  568 | `				if( pStream->zText[0] == '+' ){` |
|        - |  569 | `					/* Current operator: ++ */` |
|    13545 |  570 | `					pStream->zText++;` |
|    10625 |  571 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  572 | `					/* Current operator: += */` |
|       51 |  573 | `					pStream->zText++;` |
|       23 |  574 | `				}` |
|     8695 |  575 | `			}` |
|    17397 |  576 | `			break;` |
|    90274 |  577 | `		case '-':` |
|   180553 |  578 | `			if( pStream->zText < pStream->zEnd ){` |
|   180553 |  579 | `				if( pStream->zText[0] == '-' ){` |
|        - |  580 | `					/* Current operator: -- */` |
|       37 |  581 | `					pStream->zText++;` |
|   180536 |  582 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  583 | `					/* Current operator: -= */` |
|       10 |  584 | `					pStream->zText++;` |
|   180515 |  585 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  586 | `					/* Current operator: -> */` |
|   179953 |  587 | `					pStream->zText++;` |
|    89974 |  588 | `				}` |
|    90274 |  589 | `			}` |
|   180553 |  590 | `			break;` |
|      174 |  591 | `		case '*':` |
|      353 |  592 | `			if( pStream->zText < pStream->zEnd ){` |
|      353 |  593 | `				if( pStream->zText[0] == '*' ){` |
|        - |  594 | `					/* Current operator: ** or **= */` |
|      135 |  595 | `					pStream->zText++;` |
|      135 |  596 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  597 | `						/* Current operator: **= */` |
|       23 |  598 | `						pStream->zText++;` |
|       12 |  599 | `					}` |
|      286 |  600 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  601 | `					/* Current operator: *= */` |
|       20 |  602 | `					pStream->zText++;` |
|        9 |  603 | `				}` |
|      174 |  604 | `			}` |
|      353 |  605 | `			break;` |
|       35 |  606 | `		case '/':` |
|       72 |  607 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  608 | `				/* Current operator: /= */` |
|        5 |  609 | `				pStream->zText++;` |
|        2 |  610 | `			}` |
|       72 |  611 | `			break;` |
|       30 |  612 | `		case '%':` |
|       65 |  613 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  614 | `				/* Current operator: %= */` |
|        3 |  615 | `				pStream->zText++;` |
|        1 |  616 | `			}` |
|       65 |  617 | `			break;` |
|       11 |  618 | `		case '^':` |
|       23 |  619 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  620 | `				/* Current operator: ^= */` |
|        9 |  621 | `				pStream->zText++;` |
|        4 |  622 | `			}` |
|       23 |  623 | `			break;` |
|    45388 |  624 | `		case '.':` |
|    90781 |  625 | `			if( pStream->zText + 1 < pStream->zEnd && pStream->zText[0] == '.' && pStream->zText[1] == '.' ){` |
|        - |  626 | `				/* Ellipsis: ... */` |
|      133 |  627 | `				pStream->zText += 2;` |
|      133 |  628 | `				pToken->nType = PH7_TK_ELLIPSIS;` |
|    90717 |  629 | `			}else if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  630 | `				/* Current operator: .= */` |
|     3423 |  631 | `				pStream->zText++;` |
|     1709 |  632 | `			}` |
|    90781 |  633 | `			break;` |
|    28307 |  634 | `		case '<':` |
|    56619 |  635 | `			if( pStream->zText < pStream->zEnd ){` |
|    56619 |  636 | `				if( pStream->zText[0] == '<' ){` |
|        - |  637 | `					/* Current operator: << */` |
|      141 |  638 | `					pStream->zText++;` |
|      141 |  639 | `					if( pStream->zText < pStream->zEnd ){` |
|      141 |  640 | `						if( pStream->zText[0] == '=' ){` |
|        - |  641 | `							/* Current operator: <<= */` |
|        9 |  642 | `							pStream->zText++;` |
|      137 |  643 | `						}else if( pStream->zText[0] == '<' ){` |
|        - |  644 | `							/* Current Token: <<<  */` |
|      119 |  645 | `							pStream->zText++;` |
|        - |  646 | `							/* This may be the beginning of a Heredoc/Nowdoc string,try to delimit it */` |
|      119 |  647 | `							rc = LexExtractHeredoc(&(*pStream),&(*pToken));` |
|      119 |  648 | `							if( rc == SXRET_OK ){` |
|        - |  649 | `								/* Here/Now doc successfuly extracted */` |
|      119 |  650 | `								return SXRET_OK;` |
|        - |  651 | `							}` |
|      ! 0 |  652 | `						}` |
|       12 |  653 | `					}` |
|    56494 |  654 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  655 | `					/* Current operator: <> */` |
|        5 |  656 | `					pStream->zText++;` |
|    56481 |  657 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  658 | `					/* Current operator: <= or <=> */` |
|      103 |  659 | `					pStream->zText++;` |
|      103 |  660 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '>' ){` |
|        - |  661 | `						/* Current operator: <=> */` |
|       51 |  662 | `						pStream->zText++;` |
|       25 |  663 | `					}` |
|       49 |  664 | `				}` |
|    28250 |  665 | `			}` |
|    56505 |  666 | `			break;` |
|     3429 |  667 | `		case '>':` |
|     6863 |  668 | `			if( pStream->zText < pStream->zEnd ){` |
|     6863 |  669 | `				if( pStream->zText[0] == '>' ){` |
|        - |  670 | `					/* Current operator: >> */` |
|       21 |  671 | `					pStream->zText++;` |
|       21 |  672 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  673 | `						/* Current operator: >>= */` |
|       11 |  674 | `						pStream->zText++;` |
|        6 |  675 | `					}` |
|     6853 |  676 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  677 | `					/* Current operator: >= */` |
|       89 |  678 | `					pStream->zText++;` |
|       42 |  679 | `				}` |
|     3429 |  680 | `			}` |
|     6863 |  681 | `			break;` |
|     1470 |  682 | `		case '?':` |
|     2945 |  683 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '?' ){` |
|        - |  684 | `				/* Null coalescing operator: ?? */` |
|      191 |  685 | `				pStream->zText++;` |
|      191 |  686 | `				if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  687 | `					/* Null coalescing assignment operator (PHP 7.4) */` |
|       66 |  688 | `					pStream->zText++;` |
|       31 |  689 | `				}` |
|     2906 |  690 | `			}else if( (pStream->zEnd - pStream->zText) >= 2` |
|     2759 |  691 | `				&& pStream->zText[0] == '-' && pStream->zText[1] == '>' ){` |
|        - |  692 | `				/* Nullsafe object operator (PHP 8.0): ?-> */` |
|      113 |  693 | `				pStream->zText += 2;` |
|       54 |  694 | `			}` |
|     2940 |  695 | `			break;` |
|      114 |  696 | `		default:` |
|      228 |  697 | `			break;` |
|        - |  698 | `		}` |
|  5960285 |  699 | `		if( pStr->nByte <= 0 ){` |
|        - |  700 | `			/* Record token length */` |
|  5960233 |  701 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  2980114 |  702 | `		}` |
|  5960285 |  703 | `		if( pToken->nType & PH7_TK_OP ){` |
|        - |  704 | `			const ph7_expr_op *pOp;` |
|        - |  705 | `			/* Check if the extracted token is an operator */` |
|   997401 |  706 | `			pOp = PH7_ExprExtractOperator(pStr,(SyToken *)SySetPeek(pStream->pSet));` |
|   997401 |  707 | `			if( pOp == 0 ){` |
|        - |  708 | `				/* Not an operator */` |
|      ! 0 |  709 | `				pToken->nType &= ~PH7_TK_OP;` |
|      ! 0 |  710 | `				if( pToken->nType <= 0 ){` |
|      ! 0 |  711 | `					pToken->nType = PH7_TK_OTHER;` |
|      ! 0 |  712 | `				}` |
|      ! 0 |  713 | `			}else{` |
|        - |  714 | `				/* Save the instance associated with this operator for later processing */` |
|   997401 |  715 | `				pToken->pUserData = (void *)pOp;` |
|        - |  716 | `			}` |
|   498698 |  717 | `		}` |
|        - |  718 | `	}` |
|        - |  719 | `	/* Tell the upper-layer to save the extracted token for later processing */` |
|  9837877 |  720 | `	return SXRET_OK;` |
|  5081524 |  721 |  |
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
|  3877597 |  741 | `static sxu32 KeywordCode(const char *z, int n){` |
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
|        - |  813 | `    PH7_TKWRD_REQONCE,   PH7_TKWRD_REQUIRE,     PH7_TKWRD_SEQ,       PH7_TKWRD_ENDDEC,    PH7_TKWRD_DECLARE,` |
|        - |  814 | `    PH7_TKWRD_RETURN,    PH7_TKWRD_NAMESPACE,   PH7_TKWRD_ECHO,      PH7_TKWRD_OBJECT,    PH7_TKWRD_THROW,` |
|        - |  815 | `    PH7_TKWRD_BOOL,      PH7_TKWRD_BOOL,        PH7_TKWRD_AND,       PH7_TKWRD_DEFAULT,   PH7_TKWRD_TRY,` |
|        - |  816 | `    PH7_TKWRD_CASE,      PH7_TKWRD_SELF,        PH7_TKWRD_FINAL,     PH7_TKWRD_LIST,      PH7_TKWRD_STATIC,` |
|        - |  817 | `    PH7_TKWRD_CLONE,     PH7_TKWRD_SNE,         PH7_TKWRD_NEW,       PH7_TKWRD_CONST,     PH7_TKWRD_STRING,` |
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
|  3877597 |  831 | `  if( n<2 ) return PH7_TK_ID;` |
|  3762385 |  832 | `  h = (((int)z[0]*4) ^ ((int)z[n-1]*3) ^ n) % 151;` |
|  5626861 |  833 | `  for(i=((int)aHash[h])-1; i>=0; i=((int)aNext[i])-1){` |
|  3293935 |  834 | `    if( (int)aLen[i]==n && SyMemcmp(&zText[aOffset[i]],z,n)==0 ){` |
|        - |  835 | `       /* PH7_TKWRD_EXTENDS */` |
|        - |  836 | `       /* PH7_TKWRD_ENDSWITCH */` |
|        - |  837 | `       /* PH7_TKWRD_SWITCH */` |
|        - |  838 | `       /* PH7_TKWRD_PRINT */` |
|        - |  839 | `       /* PH7_TKWRD_INT */` |
|        - |  840 | `       /* PH7_TKWRD_REQONCE */` |
|        - |  841 | `       /* PH7_TKWRD_REQUIRE */` |
|        - |  842 | `       /* PH7_TKWRD_SEQ */` |
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
|        - |  861 | `       /* PH7_TKWRD_SNE */` |
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
|  1429459 |  919 | `      return aCode[i];` |
|        - |  920 | `    }` |
|   932241 |  921 | `  }` |
|        - |  922 | `  /* Linear fallback for keywords not in the auto-generated hash table */` |
|  2332931 |  923 | `  if( n==5 && SyMemcmp(z,"trait",5)==0 ) return PH7_TKWRD_TRAIT;` |
|  2332873 |  924 | `  if( n==9 && SyMemcmp(z,"insteadof",9)==0 ) return PH7_TKWRD_INSTEADOF;` |
|  2332869 |  925 | `  if( n==7 && SyMemcmp(z,"finally",7)==0 ) return PH7_TKWRD_FINALLY;` |
|  2332821 |  926 | `  if( n==5 && SyMemcmp(z,"yield",5)==0 ) return PH7_TKWRD_YIELD;` |
|  2332749 |  927 | `  if( n==5 && SyMemcmp(z,"match",5)==0 ) return PH7_TKWRD_MATCH;` |
|  2332679 |  928 | `  return PH7_TK_ID;` |
|  1938801 |  929 |  |
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
|      114 |  964 | `static sxi32 LexExtractHeredoc(SyStream *pStream,SyToken *pToken)` |
|        5 |  965 |  |
|      119 |  966 | `	const unsigned char *zIn  = pStream->zText;` |
|      119 |  967 | `	const unsigned char *zEnd = pStream->zEnd;` |
|        - |  968 | `	const unsigned char *zPtr;` |
|      119 |  969 | `	sxu8 bNowDoc = FALSE;` |
|        - |  970 | `	SyString sDelim;` |
|        - |  971 | `	SyString sStr;` |
|        - |  972 | `	/* Jump leading white spaces */` |
|      131 |  973 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|       13 |  974 | `		zIn++;` |
|        1 |  975 | `	}` |
|      119 |  976 | `	if( zIn >= zEnd ){` |
|        - |  977 | `		/* A simple symbol,return immediately */` |
|      ! 0 |  978 | `		return SXERR_CONTINUE;` |
|        - |  979 | `	}` |
|      119 |  980 | `	if( zIn[0] == '\'' \|\| zIn[0] == '"' ){` |
|        - |  981 | `		/* Make sure we are dealing with a nowdoc */` |
|       50 |  982 | `		bNowDoc =  zIn[0] == '\'' ? TRUE : FALSE;` |
|       50 |  983 | `		zIn++;` |
|       23 |  984 | `	}` |
|      119 |  985 | `	if( zIn[0] < 0xc0 && !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|        - |  986 | `		/* Invalid delimiter,return immediately */` |
|      ! 0 |  987 | `		return SXERR_CONTINUE;` |
|        - |  988 | `	}` |
|        - |  989 | `	/* Isolate the identifier */` |
|      119 |  990 | `	sDelim.zString = (const char *)zIn;` |
|      122 |  991 | `	for(;;){` |
|      249 |  992 | `		zPtr = zIn;` |
|        - |  993 | `		/* Skip alphanumeric stream */` |
|      783 |  994 | `		while( zPtr < zEnd && zPtr[0] < 0xc0 && (SyisAlphaNum(zPtr[0]) \|\| zPtr[0] == '_') ){` |
|      417 |  995 | `			zPtr++;` |
|        5 |  996 | `		}` |
|      249 |  997 | `		if( zPtr < zEnd && zPtr[0] >= 0xc0 ){` |
|       19 |  998 | `			zPtr++;` |
|        - |  999 | `			/* UTF-8 stream */` |
|       37 | 1000 | `			while( zPtr < zEnd && ((zPtr[0] & 0xc0) == 0x80) ){` |
|       19 | 1001 | `				zPtr++;` |
|        1 | 1002 | `			}` |
|        9 | 1003 | `		}` |
|      249 | 1004 | `		if( zPtr == zIn ){` |
|        - | 1005 | `			/* Not an UTF-8 or alphanumeric stream */` |
|      119 | 1006 | `			break;` |
|        - | 1007 | `		}` |
|        - | 1008 | `		/* Synchronize pointers */` |
|      135 | 1009 | `		zIn = zPtr;` |
|        5 | 1010 | `	}` |
|        - | 1011 | `	/* Get the identifier length */` |
|      119 | 1012 | `	sDelim.nByte = (sxu32)((const char *)zIn-sDelim.zString);` |
|      119 | 1013 | `	if( zIn[0] == '"' \|\| (bNowDoc && zIn[0] == '\'') ){` |
|        - | 1014 | `		/* Jump the trailing single quote */` |
|       50 | 1015 | `		zIn++;` |
|       23 | 1016 | `	}` |
|        - | 1017 | `	/* Jump trailing white spaces */` |
|      119 | 1018 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|      ! 0 | 1019 | `		zIn++;` |
|      ! 0 | 1020 | `	}` |
|      119 | 1021 | `	if( sDelim.nByte <= 0 \|\| zIn >= zEnd \|\| zIn[0] != '\n' ){` |
|        - | 1022 | `		/* Invalid syntax */` |
|      ! 0 | 1023 | `		return SXERR_CONTINUE;` |
|        - | 1024 | `	}` |
|      119 | 1025 | `	pStream->nLine++; /* Increment line counter */` |
|      119 | 1026 | `	zIn++;` |
|        - | 1027 | `	/* Isolate the delimited string */` |
|      119 | 1028 | `	sStr.zString = (const char *)zIn;` |
|        - | 1029 | `	/* PHP 7.3 flexible heredoc/nowdoc: the closing marker may be preceded` |
|        - | 1030 | `	 * by whitespace (spaces/tabs), and may be followed by any non-identifier` |
|        - | 1031 | `	 * character. The indent count is recorded in pToken->pUserData and the` |
|        - | 1032 | `	 * compile phase strips it from each body line. */` |
|        - | 1033 | `	{` |
|      119 | 1034 | `		const unsigned char *zMarkerLine = zIn; /* Start of marker's line (set on match) */` |
|      119 | 1035 | `		sxu32 nIndent = 0;` |
|      259 | 1036 | `		for(;;){` |
|      321 | 1037 | `			const unsigned char *zLineStart = zIn;` |
|        - | 1038 | `			/* Skip leading space/tab on this line */` |
|      869 | 1039 | `			while( zIn < zEnd && (zIn[0] == ' ' \|\| zIn[0] == '\t') ){` |
|      395 | 1040 | `				zIn++;` |
|        5 | 1041 | `			}` |
|      316 | 1042 | `			if( (sxu32)(zEnd - zIn) >= sDelim.nByte` |
|      320 | 1043 | `				&& SyMemcmp((const void *)sDelim.zString,(const void *)zIn,sDelim.nByte) == 0 ){` |
|        - | 1044 | `				int bIdentCont;` |
|      117 | 1045 | `				zPtr = &zIn[sDelim.nByte];` |
|        - | 1046 | `				/* Disambiguate: next byte must not continue an identifier.` |
|        - | 1047 | `				 * A leading byte >= 0xc0 starts a multi-byte UTF-8 sequence,` |
|        - | 1048 | `				 * which PHP identifiers may contain, so treat it as ident. */` |
|      117 | 1049 | `				if( zPtr >= zEnd ){` |
|      ! 0 | 1050 | `					bIdentCont = 0;` |
|      117 | 1051 | `				}else if( zPtr[0] >= 0xc0 ){` |
|      ! 0 | 1052 | `					bIdentCont = 1;` |
|      ! 0 | 1053 | `				}else{` |
|      117 | 1054 | `					bIdentCont = (SyisAlphaNum(zPtr[0]) \|\| zPtr[0] == '_');` |
|        - | 1055 | `				}` |
|      117 | 1056 | `				if( !bIdentCont ){` |
|        - | 1057 | `					/* Closing marker found */` |
|      117 | 1058 | `					nIndent = (sxu32)(zIn - zLineStart);` |
|      117 | 1059 | `					zMarkerLine = zLineStart;` |
|      117 | 1060 | `					pStream->zText = zPtr; /* Cursor right after identifier */` |
|      117 | 1061 | `					break;` |
|        - | 1062 | `				}` |
|      ! 0 | 1063 | `			}` |
|        - | 1064 | `			/* Not the closing marker on this line; walk to next newline */` |
|     4295 | 1065 | `			while( zIn < zEnd && zIn[0] != '\n' ){` |
|     4091 | 1066 | `				zIn++;` |
|        5 | 1067 | `			}` |
|      209 | 1068 | `			if( zIn >= zEnd ){` |
|        - | 1069 | `				/* End of input without finding the closing marker */` |
|        3 | 1070 | `				pStream->zText = pStream->zEnd;` |
|        3 | 1071 | `				zMarkerLine = zIn;` |
|        3 | 1072 | `				break;` |
|        - | 1073 | `			}` |
|      207 | 1074 | `			pStream->nLine++;` |
|      207 | 1075 | `			zIn++;` |
|        5 | 1076 | `		}` |
|        - | 1077 | `		/* Body runs from sStr.zString up to just before the marker line */` |
|      119 | 1078 | `		sStr.nByte = (sxu32)((const char *)zMarkerLine - sStr.zString);` |
|      119 | 1079 | `		pToken->nType = bNowDoc ? PH7_TK_NOWDOC : PH7_TK_HEREDOC;` |
|      119 | 1080 | `		SyStringDupPtr(&pToken->sData,&sStr);` |
|        - | 1081 | `		/* Strip exactly one line terminator that precedes the marker's line. */` |
|      114 | 1082 | `		if( pToken->sData.nByte > 0` |
|      115 | 1083 | `			&& pToken->sData.zString[pToken->sData.nByte - 1] == '\n' ){` |
|      109 | 1084 | `			pToken->sData.nByte--;` |
|      104 | 1085 | `			if( pToken->sData.nByte > 0` |
|      109 | 1086 | `				&& pToken->sData.zString[pToken->sData.nByte - 1] == '\r' ){` |
|      ! 0 | 1087 | `				pToken->sData.nByte--;` |
|      ! 0 | 1088 | `			}` |
|       52 | 1089 | `		}` |
|      119 | 1090 | `		pToken->pUserData = SX_INT_TO_PTR(nIndent);` |
|        - | 1091 | `	}` |
|        - | 1092 | `	/* All done */` |
|      119 | 1093 | `	return SXRET_OK;` |
|       62 | 1094 |  |
|        - | 1095 | `/*` |
|        - | 1096 | ` * Tokenize a raw PHP input.` |
|        - | 1097 | ` * This is the public tokenizer called by most code generator routines.` |
|        - | 1098 | ` */` |
|    15770 | 1099 | `PH7_PRIVATE sxi32 PH7_TokenizePHP(const char *zInput,sxu32 nLen,sxu32 nLineStart,SySet *pOut)` |
|        5 | 1100 |  |
|        - | 1101 | `	SyLex sLexer;` |
|        - | 1102 | `	sxi32 rc;` |
|        - | 1103 | `	/* Initialize the lexer */` |
|    15775 | 1104 | `	rc = SyLexInit(&sLexer,&(*pOut),TokenizePHP,0);` |
|    15775 | 1105 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1106 | `		return rc;` |
|        - | 1107 | `	}` |
|    15775 | 1108 | `	sLexer.sStream.nLine = nLineStart;` |
|        - | 1109 | `	/* Tokenize input */` |
|    15775 | 1110 | `	rc = SyLexTokenizeInput(&sLexer,zInput,nLen,0,0,0);` |
|        - | 1111 | `	/* Release the lexer */` |
|    15775 | 1112 | `	SyLexRelease(&sLexer);` |
|        - | 1113 | `	/* Tokenization result */` |
|    15775 | 1114 | `	return rc;` |
|     7890 | 1115 |  |
|        - | 1116 | `/*` |
|        - | 1117 | ` * High level public tokenizer.` |
|        - | 1118 | ` *  Tokenize the input into PHP tokens and raw tokens [i.e: HTML,XML,Raw text...].` |
|        - | 1119 | ` * According to the PHP language reference manual` |
|        - | 1120 | ` *   When PHP parses a file, it looks for opening and closing tags, which tell PHP` |
|        - | 1121 | ` *   to start and stop interpreting the code between them. Parsing in this manner allows` |
|        - | 1122 | ` *   PHP to be embedded in all sorts of different documents, as everything outside of a pair` |
|        - | 1123 | ` *   of opening and closing tags is ignored by the PHP parser. Most of the time you will see` |
|        - | 1124 | ` *   PHP embedded in HTML documents, as in this example.` |
|        - | 1125 | ` *   <?php echo 'While this is going to be parsed.'; ?>` |
|        - | 1126 | ` *   <p>This will also be ignored.</p>` |
|        - | 1127 | ` *   You can also use more advanced structures:` |
|        - | 1128 | ` *   Example #1 Advanced escaping` |
|        - | 1129 | ` * <?php` |
|        - | 1130 | ` * if ($expression) {` |
|        - | 1131 | ` *   ?>` |
|        - | 1132 | ` *   <strong>This is true.</strong>` |
|        - | 1133 | ` *   <?php` |
|        - | 1134 | ` * } else {` |
|        - | 1135 | ` *   ?>` |
|        - | 1136 | ` *   <strong>This is false.</strong>` |
|        - | 1137 | ` *   <?php` |
|        - | 1138 | ` * }` |
|        - | 1139 | ` * ?>` |
|        - | 1140 | ` * This works as expected, because when PHP hits the ?> closing tags, it simply starts outputting` |
|        - | 1141 | ` * whatever it finds (except for an immediately following newline - see instruction separation ) until it hits` |
|        - | 1142 | ` * another opening tag. The example given here is contrived, of course, but for outputting large blocks of text` |
|        - | 1143 | ` * dropping out of PHP parsing mode is generally more efficient than sending all of the text through echo() or print().` |
|        - | 1144 | ` * There are four different pairs of opening and closing tags which can be used in PHP. Three of those, <?php ?>` |
|        - | 1145 | ` * <script language="php"> </script>  and <? ?> are always available. The other two are short tags and ASP style` |
|        - | 1146 | ` * tags, and can be turned on and off from the php.ini configuration file. As such, while some people find short tags` |
|        - | 1147 | ` * and ASP style tags convenient, they are less portable, and generally not recommended.` |
|        - | 1148 | ` * Note:` |
|        - | 1149 | ` * Also note that if you are embedding PHP within XML or XHTML you will need to use the <?php ?> tags to remain` |
|        - | 1150 | ` * compliant with standards.` |
|        - | 1151 | ` * Example #2 PHP Opening and Closing Tags` |
|        - | 1152 | ` * 1.  <?php echo 'if you want to serve XHTML or XML documents, do it like this'; ?>` |
|        - | 1153 | ` * 2.  <script language="php">` |
|        - | 1154 | ` *       echo 'some editors (like FrontPage) don\'t` |
|        - | 1155 | ` *             like processing instructions';` |
|        - | 1156 | ` *   </script>` |
|        - | 1157 | ` *` |
|        - | 1158 | ` * 3.  <? echo 'this is the simplest, an SGML processing instruction'; ?>` |
|        - | 1159 | ` *   <?= expression ?> This is a shortcut for "<? echo expression ?>"` |
|        - | 1160 | ` */` |
|    13062 | 1161 | `PH7_PRIVATE sxi32 PH7_TokenizeRawText(const char *zInput,sxu32 nLen,SySet *pOut)` |
|        5 | 1162 |  |
|    13067 | 1163 | `	const char *zEnd = &zInput[nLen];` |
|    13067 | 1164 | `	const char *zIn  = zInput;` |
|        - | 1165 | `	const char *zCur,*zCurEnd;` |
|    13067 | 1166 | `	SyString sCtag = { 0, 0 };     /* Closing tag */` |
|        - | 1167 | `	SyToken sToken;` |
|        - | 1168 | `	SyString sDoc;` |
|        - | 1169 | `	sxu32 nLine;` |
|        - | 1170 | `	sxi32 iNest;` |
|        - | 1171 | `	sxi32 rc;` |
|        - | 1172 | `	/* Tokenize the input into PHP tokens and raw tokens */` |
|    13067 | 1173 | `	nLine = 1;` |
|    13067 | 1174 | `	zCur = zCurEnd   = 0; /* Prevent compiler warning */` |
|    13067 | 1175 | `	sToken.pUserData = 0;` |
|    13067 | 1176 | `	iNest = 0;` |
|    13067 | 1177 | `	sDoc.nByte = 0;` |
|    13067 | 1178 | `	sDoc.zString = ""; /* cc warning */` |
|    13064 | 1179 | `	for(;;){` |
|    26133 | 1180 | `		if( zIn >= zEnd ){` |
|        - | 1181 | `			/* End of input reached */` |
|    13017 | 1182 | `			break;` |
|        - | 1183 | `		}` |
|    13121 | 1184 | `		sToken.nLine = nLine;` |
|    13121 | 1185 | `		zCur = zIn;` |
|    13121 | 1186 | `		zCurEnd = 0;` |
|    13175 | 1187 | `		while( zIn < zEnd ){` |
|    13125 | 1188 | `			 if( zIn[0] == '<' ){` |
|    13071 | 1189 | `				const char *zTmp = zIn; /* End of raw input marker */` |
|    13071 | 1190 | `				zIn++;` |
|    13071 | 1191 | `				if( zIn < zEnd ){` |
|    13071 | 1192 | `					if( zIn[0] == '?' ){` |
|    13071 | 1193 | `						zIn++;` |
|    13071 | 1194 | `						if( (sxu32)(zEnd - zIn) >= sizeof("php")-1 &&  SyStrnicmp(zIn,"php",sizeof("php")-1) == 0 ){` |
|        - | 1195 | `							/* opening tag: <?php */` |
|    13069 | 1196 | `							zIn += sizeof("php")-1;` |
|     6532 | 1197 | `						}` |
|        - | 1198 | `						/* Look for the closing tag '?>' */` |
|    13071 | 1199 | `						SyStringInitFromBuf(&sCtag,"?>",sizeof("?>")-1);` |
|    13071 | 1200 | `						zCurEnd = zTmp;` |
|    13071 | 1201 | `						break;` |
|        - | 1202 | `					}` |
|      ! 0 | 1203 | `				}` |
|      ! 0 | 1204 | `			}else{` |
|       59 | 1205 | `				if( zIn[0] == '\n' ){` |
|       59 | 1206 | `					nLine++;` |
|       27 | 1207 | `				}` |
|       59 | 1208 | `				zIn++;` |
|        - | 1209 | `			 }` |
|        5 | 1210 | `		} /* While(zIn < zEnd) */` |
|    13121 | 1211 | `		if( zCurEnd == 0 ){` |
|       54 | 1212 | `			zCurEnd = zIn;` |
|       25 | 1213 | `		}` |
|        - | 1214 | `		/* Save the raw token */` |
|    13121 | 1215 | `		SyStringInitFromBuf(&sToken.sData,zCur,zCurEnd - zCur);` |
|    13121 | 1216 | `		sToken.nType = PH7_TOKEN_RAW;` |
|    13121 | 1217 | `		rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|    13121 | 1218 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1219 | `			return rc;` |
|        - | 1220 | `		}` |
|    13121 | 1221 | `		if( zIn >= zEnd ){` |
|       54 | 1222 | `			break;` |
|        - | 1223 | `		}` |
|        - | 1224 | `		/* Ignore leading white space */` |
|    28139 | 1225 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    15073 | 1226 | `			if( zIn[0] == '\n' ){` |
|    13829 | 1227 | `				nLine++;` |
|     6912 | 1228 | `			}` |
|    15073 | 1229 | `			zIn++;` |
|        5 | 1230 | `		}` |
|        - | 1231 | `		/* Delimit the PHP chunk */` |
|    13071 | 1232 | `		sToken.nLine = nLine;` |
|    13071 | 1233 | `		zCur = zIn;` |
|  1287553 | 1234 | `		while( (sxu32)(zEnd - zIn) >= sCtag.nByte ){` |
|        - | 1235 | `			const char *zPtr;` |
|  1281825 | 1236 | `			if( SyMemcmp(zIn,sCtag.zString,sCtag.nByte) == 0 && iNest < 1 ){` |
|     7341 | 1237 | `				break;` |
|        - | 1238 | `			}` |
|   639519 | 1239 | `			for(;;){` |
|  1279043 | 1240 | `				if( zIn[0] != '/' \|\| (zIn[1] != '*' && zIn[1] != '/') /* && sCtag.nByte >= 2 */ ){` |
|   637247 | 1241 | `					break;` |
|        - | 1242 | `				}` |
|     4559 | 1243 | `				zIn += 2;` |
|     4559 | 1244 | `				if( zIn[-1] == '/' ){` |
|        - | 1245 | `					/* Inline comment */` |
|   164289 | 1246 | `					while( zIn < zEnd && zIn[0] != '\n' ){` |
|   159825 | 1247 | `						zIn++;` |
|        5 | 1248 | `					}` |
|     4469 | 1249 | `					if( zIn >= zEnd ){` |
|      ! 0 | 1250 | `						zIn--;` |
|      ! 0 | 1251 | `					}` |
|     2237 | 1252 | `				}else{` |
|        - | 1253 | `					/* Block comment */` |
|     6765 | 1254 | `					while( (sxu32)(zEnd-zIn) >= sizeof("*/") - 1 ){` |
|     6765 | 1255 | `						if( zIn[0] == '*' && zIn[1] == '/' ){` |
|       95 | 1256 | `							zIn += 2;` |
|       95 | 1257 | `							break;` |
|        - | 1258 | `						}` |
|     6675 | 1259 | `						if( zIn[0] == '\n' ){` |
|       55 | 1260 | `							nLine++;` |
|       25 | 1261 | `						}` |
|     6675 | 1262 | `						zIn++;` |
|        5 | 1263 | `					}` |
|        - | 1264 | `				}` |
|        5 | 1265 | `			}` |
|  1274489 | 1266 | `			if( zIn[0] == '\n' ){` |
|    43879 | 1267 | `				nLine++;` |
|    43879 | 1268 | `				if( iNest > 0 ){` |
|      321 | 1269 | `					zIn++;` |
|      711 | 1270 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|      395 | 1271 | `						zIn++;` |
|        5 | 1272 | `					}` |
|      321 | 1273 | `					zPtr = zIn;` |
|     1599 | 1274 | `					while( zIn < zEnd ){` |
|     1599 | 1275 | `						if( (unsigned char)zIn[0] >= 0xc0 ){` |
|        - | 1276 | `							/* UTF-8 stream */` |
|       19 | 1277 | `							zIn++;` |
|       37 | 1278 | `							SX_JMP_UTF8(zIn,zEnd);` |
|     1586 | 1279 | `						}else if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|      163 | 1280 | `							break;` |
|      ! 0 | 1281 | `						}else{` |
|     1265 | 1282 | `							zIn++;` |
|        - | 1283 | `						}` |
|        5 | 1284 | `					}` |
|      321 | 1285 | `					if( (sxu32)(zIn - zPtr) == sDoc.nByte && SyMemcmp(sDoc.zString,zPtr,sDoc.nByte) == 0 ){` |
|      117 | 1286 | `						iNest = 0;` |
|       56 | 1287 | `					}` |
|      321 | 1288 | `					continue;` |
|        5 | 1289 | `				}` |
|  1252394 | 1290 | `			}else if ( (sxu32)(zEnd - zIn) >= sizeof("<<<") && zIn[0] == '<' && zIn[1] == '<' && zIn[2] == '<' && iNest < 1){` |
|      119 | 1291 | `				zIn += sizeof("<<<")-1;` |
|      131 | 1292 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|       13 | 1293 | `					zIn++;` |
|        1 | 1294 | `				}` |
|      119 | 1295 | `				if( zIn[0] == '"' \|\| zIn[0] == '\'' ){` |
|       50 | 1296 | `					zIn++;` |
|       23 | 1297 | `				}` |
|      119 | 1298 | `				zPtr = zIn;` |
|      549 | 1299 | `				while( zIn < zEnd ){` |
|      549 | 1300 | `					if( (unsigned char)zIn[0] >= 0xc0 ){` |
|        - | 1301 | `						/* UTF-8 stream */` |
|       19 | 1302 | `						zIn++;` |
|       37 | 1303 | `						SX_JMP_UTF8(zIn,zEnd);` |
|      536 | 1304 | `					}else if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       62 | 1305 | `						break;` |
|      ! 0 | 1306 | `					}else{` |
|      417 | 1307 | `						zIn++;` |
|        - | 1308 | `					}` |
|        5 | 1309 | `				}` |
|      119 | 1310 | `				SyStringInitFromBuf(&sDoc,zPtr,zIn-zPtr);` |
|      119 | 1311 | `				SyStringFullTrim(&sDoc);` |
|      119 | 1312 | `				if( sDoc.nByte > 0 ){` |
|      119 | 1313 | `					iNest++;` |
|       57 | 1314 | `				}` |
|      119 | 1315 | `				continue;` |
|        - | 1316 | `			}` |
|  1274059 | 1317 | `			zIn++;` |
|        - | 1318 |  |
|  1274059 | 1319 | `			if ( zIn >= zEnd )` |
|        3 | 1320 | `				break;` |
|        5 | 1321 | `		}` |
|    13071 | 1322 | `		if( (sxu32)(zEnd - zIn) < sCtag.nByte ){` |
|     5735 | 1323 | `			zIn = zEnd;` |
|     2865 | 1324 | `		}` |
|    13071 | 1325 | `		if( zCur < zIn ){` |
|        - | 1326 | `			/* Save the PHP chunk for later processing */` |
|    10287 | 1327 | `			sToken.nType = PH7_TOKEN_PHP;` |
|    10287 | 1328 | `			SyStringInitFromBuf(&sToken.sData,zCur,zIn-zCur);` |
|    20499 | 1329 | `			SyStringRightTrim(&sToken.sData); /* Trim trailing white spaces */` |
|    10287 | 1330 | `			rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|    10287 | 1331 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1332 | `				return rc;` |
|        - | 1333 | `			}` |
|     5141 | 1334 | `		}` |
|    13071 | 1335 | `		if( zIn < zEnd ){` |
|        - | 1336 | `			/* Jump the trailing closing tag */` |
|     7341 | 1337 | `			zIn += sCtag.nByte;` |
|     3668 | 1338 | `		}` |
|        5 | 1339 | `	} /* For(;;) */` |
|        - | 1340 |  |
|    13067 | 1341 | ` 	return SXRET_OK;` |
|     6536 | 1342 |  |
|        - | 1343 |  |
