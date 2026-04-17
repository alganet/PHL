# src/ph7/lex.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 716/751 lines (95.34%)

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
|  8479128 |   19 | `static sxi32 TokenizePHP(SyStream *pStream,SyToken *pToken,void *pUserData,void *pCtxData)` |
|        2 |   20 |  |
|        - |   21 | `	SyString *pStr;` |
|        - |   22 | `	sxi32 rc;` |
|        - |   23 | `	/* Ignore leading white spaces */` |
| 12727698 |   24 | `	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){` |
|        - |   25 | `		/* Advance the stream cursor */` |
|  4248570 |   26 | `		if( pStream->zText[0] == '\n' ){` |
|        - |   27 | `			/* Update line counter */` |
|    32586 |   28 | `			pStream->nLine++;` |
|    16292 |   29 | `		}` |
|  4248570 |   30 | `		pStream->zText++;` |
|        2 |   31 | `	}` |
|  8479130 |   32 | `	if( pStream->zText >= pStream->zEnd ){` |
|        - |   33 | `		/* End of input reached */` |
|      ! 0 |   34 | `		return SXERR_EOF;` |
|        - |   35 | `	}` |
|        - |   36 | `	/* Record token starting position and line */` |
|  8479130 |   37 | `	pToken->nLine = pStream->nLine;` |
|  8479130 |   38 | `	pToken->pUserData = 0;` |
|  8479130 |   39 | `	pStr = &pToken->sData;` |
|  8479130 |   40 | `	SyStringInitFromBuf(pStr,pStream->zText,0);` |
| 10039991 |   41 | `	if( pStream->zText[0] >= 0xc0 \|\| SyisAlpha(pStream->zText[0]) \|\| pStream->zText[0] == '_' ){` |
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
|  3121724 |   52 | `		if( pStream->zText[0] < 0xc0 ){` |
|  3121708 |   53 | `			pStream->zText++;` |
|  1560853 |   54 | `		}` |
|  3071259 |   55 | `		for(;;){` |
|  6142520 |   56 | `			zIn = pStream->zText;` |
|  6142520 |   57 | `			if( zIn[0] >= 0xc0 ){` |
|       49 |   58 | `				zIn++;` |
|        - |   59 | `				/* UTF-8 stream */` |
|      109 |   60 | `				while( zIn < pStream->zEnd && ((zIn[0] & 0xc0) == 0x80) ){` |
|       61 |   61 | `					zIn++;` |
|        1 |   62 | `				}` |
|       24 |   63 | `			}` |
|        - |   64 | `			/* Skip alphanumeric stream */` |
| 25430911 |   65 | `			while( zIn < pStream->zEnd && zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_') ){` |
| 16217134 |   66 | `				zIn++;` |
|        2 |   67 | `			}` |
|  6142520 |   68 | `			if( zIn == pStream->zText ){` |
|        - |   69 | `				/* Not an UTF-8 or alphanumeric stream */` |
|  3121724 |   70 | `				break;` |
|        - |   71 | `			}` |
|        - |   72 | `			/* Synchronize pointers */` |
|  3020798 |   73 | `			pStream->zText = zIn;` |
|        2 |   74 | `		}` |
|        - |   75 | `		/* Record token length */` |
|  3121724 |   76 | `		pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  3121724 |   77 | `		nKeyword = KeywordCode(pStr->zString,(int)pStr->nByte);` |
|        - |   78 | `		/* PHP 7.4: 'fn' is a keyword reserved for arrow functions.` |
|        - |   79 | `		 * The auto-generated perfect hash above doesn't know about it,` |
|        - |   80 | `		 * so intercept the 'fn' identifier here.` |
|        - |   81 | `		 */` |
|  3122205 |   82 | `		if( nKeyword == PH7_TK_ID && pStr->nByte == 2` |
|  1015281 |   83 | `			&& pStr->zString[0] == 'f' && pStr->zString[1] == 'n' ){` |
|      360 |   84 | `			nKeyword = PH7_TKWRD_FN;` |
|      179 |   85 | `		}` |
|  3121724 |   86 | `		if( nKeyword != PH7_TK_ID ){` |
|  1099172 |   87 | `			if( nKeyword &` |
|        - |   88 | `				(PH7_TKWRD_NEW\|PH7_TKWRD_CLONE\|PH7_TKWRD_AND\|PH7_TKWRD_XOR\|PH7_TKWRD_OR\|PH7_TKWRD_INSTANCEOF\|PH7_TKWRD_SEQ\|PH7_TKWRD_SNE) ){` |
|        - |   89 | `					/* Alpha stream operators [i.e: new,clone,and,instanceof,eq,ne,or,xor],save the operator instance for later processing */` |
|    15730 |   90 | `					pToken->pUserData = (void *)PH7_ExprExtractOperator(pStr,0);` |
|        - |   91 | `					/* Mark as an operator */` |
|    15730 |   92 | `					pToken->nType = PH7_TK_ID\|PH7_TK_OP;` |
|     7866 |   93 | `			}else{` |
|        - |   94 | `				/* We are dealing with a keyword [i.e: while,foreach,class...],save the keyword ID */` |
|  1083444 |   95 | `				pToken->nType = PH7_TK_KEYWORD;` |
|  1083444 |   96 | `				pToken->pUserData = SX_INT_TO_PTR(nKeyword);` |
|        - |   97 | `			}` |
|   549587 |   98 | `		}else{` |
|        - |   99 | `			/* A simple identifier */` |
|  2022554 |  100 | `			pToken->nType = PH7_TK_ID;` |
|        - |  101 | `		}` |
|  1560863 |  102 | `	}else{` |
|        - |  103 | `		sxi32 c;` |
|        - |  104 | `		/* Non-alpha stream */` |
|  5394644 |  105 | `		if( pStream->zText[0] == '#' \|\|` |
|  5357406 |  106 | `			( pStream->zText[0] == '/' &&  &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '/') ){` |
|     3862 |  107 | `				pStream->zText++;` |
|        - |  108 | `				/* Inline comments */` |
|   140914 |  109 | `				while( pStream->zText < pStream->zEnd && pStream->zText[0] != '\n' ){` |
|   137054 |  110 | `					pStream->zText++;` |
|        2 |  111 | `				}` |
|        - |  112 | `				/* Tell the upper-layer to ignore this token */` |
|     3862 |  113 | `				return SXERR_CONTINUE;` |
|  5353548 |  114 | `		}else if( pStream->zText[0] == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '*' ){` |
|    70550 |  115 | `			pStream->zText += 2;` |
|        - |  116 | `			/* Block comment */` |
|  2001010 |  117 | `			while( pStream->zText < pStream->zEnd ){` |
|  2001010 |  118 | `				if( pStream->zText[0] == '*' ){` |
|    70576 |  119 | `					if( &pStream->zText[1] >= pStream->zEnd \|\| pStream->zText[1] == '/'  ){` |
|    35276 |  120 | `						break;` |
|        - |  121 | `					}` |
|       13 |  122 | `				}` |
|  1930462 |  123 | `				if( pStream->zText[0] == '\n' ){` |
|       28 |  124 | `					pStream->nLine++;` |
|       13 |  125 | `				}` |
|  1930462 |  126 | `				pStream->zText++;` |
|        2 |  127 | `			}` |
|    70550 |  128 | `			pStream->zText += 2;` |
|        - |  129 | `			/* Tell the upper-layer to ignore this token */` |
|    70550 |  130 | `			return SXERR_CONTINUE;` |
|  5283000 |  131 | `		}else if( SyisDigit(pStream->zText[0]) ){` |
|   103394 |  132 | `			pStream->zText++;` |
|        - |  133 | `			/* PHP 7.4: handle underscore separator immediately following the first digit.` |
|        - |  134 | `			 * Check pStream->zText < pStream->zEnd BEFORE forming pStream->zText + 1 so` |
|        - |  135 | `			 * we never compute a pointer past one-past-end. */` |
|   103472 |  136 | `			if( pStream->zText < pStream->zEnd` |
|   103392 |  137 | `				&& pStream->zText[0] == '_'` |
|    51776 |  138 | `				&& pStream->zText + 1 < pStream->zEnd` |
|      160 |  139 | `				&& pStream->zText[1] < 0xc0` |
|      162 |  140 | `				&& SyisDigit(pStream->zText[1]) ){` |
|      151 |  141 | `				pStream->zText++; /* swallow underscore between two digits */` |
|       75 |  142 | `			}` |
|        - |  143 | `			/* Decimal digit stream (PHP 7.4: underscore separator allowed between two digits) */` |
|   113444 |  144 | `			while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|    10052 |  145 | `				pStream->zText++;` |
|    10136 |  146 | `				if( pStream->zText < pStream->zEnd` |
|    10050 |  147 | `					&& pStream->zText[0] == '_'` |
|     5111 |  148 | `					&& pStream->zText + 1 < pStream->zEnd` |
|      172 |  149 | `					&& pStream->zText[1] < 0xc0` |
|      174 |  150 | `					&& SyisDigit(pStream->zText[1]) ){` |
|      173 |  151 | `					pStream->zText++; /* swallow underscore between two digits */` |
|       86 |  152 | `				}` |
|        2 |  153 | `			}` |
|        - |  154 | `			/* Mark the token as integer until we encounter a real number */` |
|   103394 |  155 | `			pToken->nType = PH7_TK_INTEGER;` |
|   103394 |  156 | `			if( pStream->zText < pStream->zEnd ){` |
|   103394 |  157 | `				c = pStream->zText[0];` |
|   103394 |  158 | `				if( c == '.' ){` |
|        - |  159 | `					/* Real number (PHP 7.4: underscore separator allowed between two digits) */` |
|      464 |  160 | `					pStream->zText++;` |
|     1790 |  161 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|     1328 |  162 | `						pStream->zText++;` |
|     1332 |  163 | `						if( pStream->zText < pStream->zEnd` |
|     1326 |  164 | `							&& pStream->zText[0] == '_'` |
|      669 |  165 | `							&& pStream->zText + 1 < pStream->zEnd` |
|       12 |  166 | `							&& pStream->zText[1] < 0xc0` |
|       14 |  167 | `							&& SyisDigit(pStream->zText[1]) ){` |
|       13 |  168 | `							pStream->zText++;` |
|        6 |  169 | `						}` |
|        2 |  170 | `					}` |
|      464 |  171 | `					if( pStream->zText < pStream->zEnd ){` |
|      464 |  172 | `						c = pStream->zText[0];` |
|      464 |  173 | `						if( c=='e' \|\| c=='E' ){` |
|       29 |  174 | `							pStream->zText++;` |
|       29 |  175 | `							if( pStream->zText < pStream->zEnd ){` |
|       29 |  176 | `								c = pStream->zText[0];` |
|       35 |  177 | `								if( (c =='+' \|\| c=='-') && &pStream->zText[1] < pStream->zEnd  &&` |
|       15 |  178 | `									pStream->zText[1] < 0xc0 && SyisDigit(pStream->zText[1]) ){` |
|       15 |  179 | `										pStream->zText++;` |
|        7 |  180 | `								}` |
|       69 |  181 | `								while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|       41 |  182 | `									pStream->zText++;` |
|       44 |  183 | `									if( pStream->zText < pStream->zEnd` |
|       40 |  184 | `										&& pStream->zText[0] == '_'` |
|       24 |  185 | `										&& pStream->zText + 1 < pStream->zEnd` |
|        8 |  186 | `										&& pStream->zText[1] < 0xc0` |
|        9 |  187 | `										&& SyisDigit(pStream->zText[1]) ){` |
|        9 |  188 | `										pStream->zText++;` |
|        4 |  189 | `									}` |
|        1 |  190 | `								}` |
|       14 |  191 | `							}` |
|       14 |  192 | `						}` |
|      231 |  193 | `					}` |
|      464 |  194 | `					pToken->nType = PH7_TK_REAL;` |
|   103163 |  195 | `				}else if( c=='e' \|\| c=='E' ){` |
|       14 |  196 | `					SXUNUSED(pUserData); /* Prevent compiler warning */` |
|       14 |  197 | `					SXUNUSED(pCtxData);` |
|       29 |  198 | `					pStream->zText++;` |
|       29 |  199 | `					if( pStream->zText < pStream->zEnd ){` |
|       29 |  200 | `						c = pStream->zText[0];` |
|       31 |  201 | `						if( (c =='+' \|\| c=='-') && &pStream->zText[1] < pStream->zEnd  &&` |
|        7 |  202 | `							pStream->zText[1] < 0xc0 && SyisDigit(pStream->zText[1]) ){` |
|        7 |  203 | `								pStream->zText++;` |
|        3 |  204 | `						}` |
|       67 |  205 | `						while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|       39 |  206 | `							pStream->zText++;` |
|       40 |  207 | `							if( pStream->zText < pStream->zEnd` |
|       38 |  208 | `								&& pStream->zText[0] == '_'` |
|       21 |  209 | `								&& pStream->zText + 1 < pStream->zEnd` |
|        4 |  210 | `								&& pStream->zText[1] < 0xc0` |
|        5 |  211 | `								&& SyisDigit(pStream->zText[1]) ){` |
|        5 |  212 | `								pStream->zText++;` |
|        2 |  213 | `							}` |
|        1 |  214 | `						}` |
|       14 |  215 | `					}` |
|       29 |  216 | `					pToken->nType = PH7_TK_REAL;` |
|   102918 |  217 | `				}else if( c == 'x' \|\| c == 'X' ){` |
|        - |  218 | `					/* Hex digit stream (PHP 7.4: underscore separator allowed between two digits) */` |
|       74 |  219 | `					pStream->zText++;` |
|      370 |  220 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisHex(pStream->zText[0]) ){` |
|      297 |  221 | `						pStream->zText++;` |
|      320 |  222 | `						if( pStream->zText < pStream->zEnd` |
|      296 |  223 | `							&& pStream->zText[0] == '_'` |
|      172 |  224 | `							&& pStream->zText + 1 < pStream->zEnd` |
|       48 |  225 | `							&& pStream->zText[1] < 0xc0` |
|       49 |  226 | `							&& SyisHex(pStream->zText[1]) ){` |
|       49 |  227 | `							pStream->zText++;` |
|       24 |  228 | `						}` |
|        1 |  229 | `					}` |
|   102868 |  230 | `				}else if(c  == 'b' \|\| c == 'B' ){` |
|        - |  231 | `					/* Binary digit stream (PHP 7.4: underscore separator allowed between two digits) */` |
|      280 |  232 | `					pStream->zText++;` |
|     2702 |  233 | `					while( pStream->zText < pStream->zEnd && (pStream->zText[0] == '0' \|\| pStream->zText[0] == '1') ){` |
|     1523 |  234 | `						pStream->zText++;` |
|     1583 |  235 | `						if( pStream->zText < pStream->zEnd` |
|     1522 |  236 | `							&& pStream->zText[0] == '_'` |
|      830 |  237 | `							&& pStream->zText + 1 < pStream->zEnd` |
|      139 |  238 | `							&& (pStream->zText[1] == '0' \|\| pStream->zText[1] == '1') ){` |
|      139 |  239 | `							pStream->zText++;` |
|       69 |  240 | `						}` |
|        1 |  241 | `					}` |
|      139 |  242 | `				}` |
|    51696 |  243 | `			}` |
|        - |  244 | `			/* PHP 7.4: absorb a trailing malformed underscore run into the` |
|        - |  245 | `			 * numeric token so the compile phase can emit a PHP-compatible` |
|        - |  246 | `			 * "syntax error, unexpected identifier" parse error. Valid` |
|        - |  247 | `			 * separators were already consumed by the per-loop peek logic` |
|        - |  248 | `			 * above, so an underscore here is always misplaced. */` |
|   103394 |  249 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '_' ){` |
|       15 |  250 | `				pStream->zText++;` |
|       44 |  251 | `				while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0` |
|       46 |  252 | `					&& (SyisAlphaNum(pStream->zText[0]) \|\| pStream->zText[0] == '_') ){` |
|       17 |  253 | `					pStream->zText++;` |
|        1 |  254 | `				}` |
|        7 |  255 | `			}` |
|        - |  256 | `			/* Record token length */` |
|   103394 |  257 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|   103394 |  258 | `			return SXRET_OK;` |
|        - |  259 | `		}` |
|  5179608 |  260 | `		c = pStream->zText[0];` |
|  5179608 |  261 | `		pStream->zText++; /* Advance the stream cursor */` |
|        - |  262 | `		/* Assume we are dealing with an operator*/` |
|  5179608 |  263 | `		pToken->nType = PH7_TK_OP;` |
|  5179608 |  264 | `		switch(c){` |
|  1071710 |  265 | `		case '$': pToken->nType = PH7_TK_DOLLAR; break;` |
|   408262 |  266 | `		case '{': pToken->nType = PH7_TK_OCB;    break;` |
|   408248 |  267 | `		case '}': pToken->nType = PH7_TK_CCB;    break;` |
|   809014 |  268 | `		case '(': pToken->nType = PH7_TK_LPAREN; break;` |
|    75636 |  269 | `		case '[': pToken->nType \|= PH7_TK_OSB;   break; /* Bitwise operation here,since the square bracket token '['` |
|        - |  270 | `														 * is a potential operator [i.e: subscripting] */` |
|    75642 |  271 | `		case ']': pToken->nType = PH7_TK_CSB;    break;` |
|   404499 |  272 | `		case ')': {` |
|   809000 |  273 | `			SySet *pTokSet = pStream->pSet;` |
|        - |  274 | `			/* Assemble type cast operators [i.e: (int),(float),(bool)...] */` |
|   809000 |  275 | `			if( pTokSet->nUsed >= 2 ){` |
|        - |  276 | `				SyToken *pTmp;` |
|        - |  277 | `				/* Peek the last recongnized token */` |
|   808998 |  278 | `				pTmp = (SyToken *)SySetPeek(pTokSet);` |
|   808998 |  279 | `				if( pTmp->nType & PH7_TK_KEYWORD ){` |
|    15222 |  280 | `					sxi32 nID = SX_PTR_TO_INT(pTmp->pUserData);` |
|    15222 |  281 | `					if( (sxu32)nID & (PH7_TKWRD_ARRAY\|PH7_TKWRD_INT\|PH7_TKWRD_FLOAT\|PH7_TKWRD_STRING\|PH7_TKWRD_OBJECT\|PH7_TKWRD_BOOL\|PH7_TKWRD_UNSET) ){` |
|    14982 |  282 | `						pTmp = (SyToken *)SySetAt(pTokSet,pTokSet->nUsed - 2);` |
|    14982 |  283 | `						if( pTmp->nType & PH7_TK_LPAREN ){` |
|        - |  284 | `							/* Merge the three tokens '(' 'TYPE' ')' into a single one */` |
|    14898 |  285 | `							const char * zTypeCast = "(int)";` |
|    14898 |  286 | `							if( nID & PH7_TKWRD_FLOAT ){` |
|     2944 |  287 | `								zTypeCast = "(float)";` |
|    13427 |  288 | `							}else if( nID & PH7_TKWRD_BOOL ){` |
|     2946 |  289 | `								zTypeCast = "(bool)";` |
|    10484 |  290 | `							}else if( nID & PH7_TKWRD_STRING ){` |
|     5882 |  291 | `								zTypeCast = "(string)";` |
|     6072 |  292 | `							}else if( nID & PH7_TKWRD_ARRAY ){` |
|       21 |  293 | `								zTypeCast = "(array)";` |
|     3122 |  294 | `							}else if( nID & PH7_TKWRD_OBJECT ){` |
|       17 |  295 | `								zTypeCast = "(object)";` |
|     3104 |  296 | `							}else if( nID & PH7_TKWRD_UNSET ){` |
|        7 |  297 | `								zTypeCast = "(unset)";` |
|        3 |  298 | `							}` |
|        - |  299 | `							/* Reflect the change */` |
|    14898 |  300 | `							pToken->nType = PH7_TK_OP;` |
|    14898 |  301 | `							SyStringInitFromBuf(&pToken->sData,zTypeCast,SyStrlen(zTypeCast));` |
|        - |  302 | `							/* Save the instance associated with the type cast operator */` |
|    14898 |  303 | `							pToken->pUserData = (void *)PH7_ExprExtractOperator(&pToken->sData,0);` |
|        - |  304 | `							/* Remove the two previous tokens */` |
|    14898 |  305 | `							pTokSet->nUsed -= 2;` |
|    14898 |  306 | `							return SXRET_OK;` |
|        - |  307 | `						}` |
|       42 |  308 | `					}` |
|      162 |  309 | `				}` |
|   397050 |  310 | `			}` |
|   794104 |  311 | `			pToken->nType = PH7_TK_RPAREN;` |
|   794104 |  312 | `			break;` |
|        - |  313 | `				  }` |
|    37235 |  314 | `		case '\'':{` |
|        - |  315 | `			/* Single quoted string */` |
|    74472 |  316 | `			pStr->zString++;` |
|   762176 |  317 | `			while( pStream->zText < pStream->zEnd ){` |
|   762176 |  318 | `				if( pStream->zText[0] == '\''  ){` |
|    74482 |  319 | `					if( pStream->zText[-1] != '\\' ){` |
|    74458 |  320 | `						break;` |
|      ! 0 |  321 | `					}else{` |
|       25 |  322 | `						const unsigned char *zPtr = &pStream->zText[-2];` |
|       25 |  323 | `						sxi32 i = 1;` |
|       43 |  324 | `						while( zPtr > pStream->zInput && zPtr[0] == '\\' ){` |
|       19 |  325 | `							zPtr--;` |
|       19 |  326 | `							i++;` |
|        1 |  327 | `						}` |
|       25 |  328 | `						if((i&1)==0){` |
|       15 |  329 | `							break;` |
|        - |  330 | `						}` |
|        - |  331 | `					}` |
|        5 |  332 | `				}` |
|   687706 |  333 | `				if( pStream->zText[0] == '\n' ){` |
|       67 |  334 | `					pStream->nLine++;` |
|       33 |  335 | `				}` |
|   687706 |  336 | `				pStream->zText++;` |
|        2 |  337 | `			}` |
|        - |  338 | `			/* Record token length and type */` |
|    74472 |  339 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|    74472 |  340 | `			pToken->nType = PH7_TK_SSTR;` |
|        - |  341 | `			/* Jump the trailing single quote */` |
|    74472 |  342 | `			pStream->zText++;` |
|    74472 |  343 | `			return SXRET_OK;` |
|        - |  344 | `				  }` |
|     8608 |  345 | `		case '"':{` |
|        - |  346 | `			sxi32 iNest;` |
|        - |  347 | `			/* Double quoted string */` |
|    17218 |  348 | `			pStr->zString++;` |
|   163100 |  349 | `			while( pStream->zText < pStream->zEnd ){` |
|   163100 |  350 | `				if( pStream->zText[0] == '{' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '$'){` |
|       99 |  351 | `					iNest = 1;` |
|       99 |  352 | `					pStream->zText++;` |
|        - |  353 | `					/* TICKET 1433-40: Hnadle braces'{}' in double quoted string where everything is allowed */` |
|     1061 |  354 | `					while(pStream->zText < pStream->zEnd ){` |
|     1061 |  355 | `						if( pStream->zText[0] == '{' ){` |
|        9 |  356 | `							iNest++;` |
|     1057 |  357 | `						}else if (pStream->zText[0] == '}' ){` |
|      107 |  358 | `							iNest--;` |
|      107 |  359 | `							if( iNest <= 0 ){` |
|       99 |  360 | `								pStream->zText++;` |
|       99 |  361 | `								break;` |
|        1 |  362 | `							}` |
|      951 |  363 | `						}else if( pStream->zText[0] == '\n' ){` |
|      ! 0 |  364 | `							pStream->nLine++;` |
|      ! 0 |  365 | `						}` |
|      963 |  366 | `						pStream->zText++;` |
|        1 |  367 | `					}` |
|       99 |  368 | `					if( pStream->zText >= pStream->zEnd ){` |
|      ! 0 |  369 | `						break;` |
|        - |  370 | `					}` |
|       49 |  371 | `				}` |
|   163100 |  372 | `				if( pStream->zText[0] == '"' ){` |
|    17318 |  373 | `					if( pStream->zText[-1] != '\\' ){` |
|    17214 |  374 | `						break;` |
|      ! 0 |  375 | `					}else{` |
|      106 |  376 | `						const unsigned char *zPtr = &pStream->zText[-2];` |
|      106 |  377 | `						sxi32 i = 1;` |
|      158 |  378 | `						while( zPtr > pStream->zInput && zPtr[0] == '\\' ){` |
|       54 |  379 | `							zPtr--;` |
|       54 |  380 | `							i++;` |
|        2 |  381 | `						}` |
|      106 |  382 | `						if((i&1)==0){` |
|        5 |  383 | `							break;` |
|        - |  384 | `						}` |
|        - |  385 | `					}` |
|       50 |  386 | `				}` |
|   145884 |  387 | `				if( pStream->zText[0] == '\n' ){` |
|        7 |  388 | `					pStream->nLine++;` |
|        3 |  389 | `				}` |
|   145884 |  390 | `				pStream->zText++;` |
|        2 |  391 | `			}` |
|        - |  392 | `			/* Record token length and type */` |
|    17218 |  393 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|    17218 |  394 | `			pToken->nType = PH7_TK_DSTR;` |
|        - |  395 | `			/* Jump the trailing quote */` |
|    17218 |  396 | `			pStream->zText++;` |
|    17218 |  397 | `			return SXRET_OK;` |
|        - |  398 | `				  }` |
|        2 |  399 | ``		case '`':{`` |
|        - |  400 | `			/* Backtick quoted string */` |
|        5 |  401 | `			pStr->zString++;` |
|       45 |  402 | `			while( pStream->zText < pStream->zEnd ){` |
|       45 |  403 | ``				if( pStream->zText[0] == '`' && pStream->zText[-1] != '\\' ){`` |
|        5 |  404 | `					break;` |
|        - |  405 | `				}` |
|       41 |  406 | `				if( pStream->zText[0] == '\n' ){` |
|      ! 0 |  407 | `					pStream->nLine++;` |
|      ! 0 |  408 | `				}` |
|       41 |  409 | `				pStream->zText++;` |
|        1 |  410 | `			}` |
|        - |  411 | `			/* Record token length and type */` |
|        5 |  412 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|        5 |  413 | `			pToken->nType = PH7_TK_BSTR;` |
|        - |  414 | `			/* Jump the trailing backtick */` |
|        5 |  415 | `			pStream->zText++;` |
|        5 |  416 | `			return SXRET_OK;` |
|        - |  417 | `				  }` |
|      220 |  418 | `		case '\\': pToken->nType = PH7_TK_NSSEP;  break;` |
|     1416 |  419 | `		case ':':` |
|     2834 |  420 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == ':' ){` |
|        - |  421 | `				/* Current operator: '::' */` |
|      242 |  422 | `				pStream->zText++;` |
|      122 |  423 | `			}else{` |
|     2594 |  424 | `				pToken->nType = PH7_TK_COLON; /* Single colon */` |
|        - |  425 | `			}` |
|     2834 |  426 | `			break;` |
|    85800 |  427 | `		case ',': pToken->nType \|= PH7_TK_COMMA;  break; /* Comma is also an operator */` |
|   615486 |  428 | `		case ';': pToken->nType = PH7_TK_SEMI;    break;` |
|        - |  429 | `			/* Handle combined operators [i.e: +=,===,!=== ...] */` |
|   170380 |  430 | `		case '=':` |
|   340762 |  431 | `			pToken->nType \|= PH7_TK_EQUAL;` |
|   340762 |  432 | `			if( pStream->zText < pStream->zEnd ){` |
|   340762 |  433 | `				if( pStream->zText[0] == '=' ){` |
|    19172 |  434 | `					pToken->nType &= ~PH7_TK_EQUAL;` |
|        - |  435 | `					/* Current operator: == */` |
|    19172 |  436 | `					pStream->zText++;` |
|    19172 |  437 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  438 | `						/* Current operator: === */` |
|     4180 |  439 | `						pStream->zText++;` |
|     2091 |  440 | `					}` |
|   331177 |  441 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  442 | `					/* Array operator: => */` |
|     4586 |  443 | `					pToken->nType = PH7_TK_ARRAY_OP;` |
|     4586 |  444 | `					pStream->zText++;` |
|     2294 |  445 | `				}else{` |
|        - |  446 | `					/* TICKET 1433-0010: Reference operator '=&' */` |
|   317008 |  447 | `					const unsigned char *zCur = pStream->zText;` |
|   317008 |  448 | `					sxu32 nLine = 0;` |
|   633960 |  449 | `					while( zCur < pStream->zEnd && zCur[0] < 0xc0 && SyisSpace(zCur[0]) ){` |
|   316954 |  450 | `						if( zCur[0] == '\n' ){` |
|        5 |  451 | `							nLine++;` |
|        2 |  452 | `						}` |
|   316954 |  453 | `						zCur++;` |
|        2 |  454 | `					}` |
|   317008 |  455 | `					if( zCur < pStream->zEnd && zCur[0] == '&' ){` |
|        - |  456 | `						/* Current operator: =& */` |
|       50 |  457 | `						pToken->nType &= ~PH7_TK_EQUAL;` |
|       50 |  458 | `						SyStringInitFromBuf(pStr,"=&",sizeof("=&")-1);` |
|        - |  459 | `						/* Update token stream */` |
|       50 |  460 | `						pStream->zText = &zCur[1];` |
|       50 |  461 | `						pStream->nLine += nLine;` |
|       24 |  462 | `					}` |
|        - |  463 | `				}` |
|   170380 |  464 | `			}` |
|   340762 |  465 | `			break;` |
|    20904 |  466 | `		case '!':` |
|    41810 |  467 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  468 | `				/* Current operator: != */` |
|    17802 |  469 | `				pStream->zText++;` |
|    17802 |  470 | `				if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  471 | `					/* Current operator: !== */` |
|    14834 |  472 | `					pStream->zText++;` |
|     7416 |  473 | `				}` |
|     8900 |  474 | `			}` |
|    41810 |  475 | `			break;` |
|    12010 |  476 | `		case '&':` |
|    24022 |  477 | `			pToken->nType \|= PH7_TK_AMPER;` |
|    24022 |  478 | `			if( pStream->zText < pStream->zEnd ){` |
|    24022 |  479 | `				if( pStream->zText[0] == '&' ){` |
|     9212 |  480 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|        - |  481 | `					/* Current operator: && */` |
|     9212 |  482 | `					pStream->zText++;` |
|    19417 |  483 | `				}else if( pStream->zText[0] == '=' ){` |
|        7 |  484 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|        - |  485 | `					/* Current operator: &= */` |
|        7 |  486 | `					pStream->zText++;` |
|        3 |  487 | `				}` |
|    12010 |  488 | `			}` |
|    24022 |  489 | `			break;` |
|     1604 |  490 | `		case '\|':` |
|     3210 |  491 | `			if( pStream->zText < pStream->zEnd ){` |
|     3210 |  492 | `				if( pStream->zText[0] == '\|' ){` |
|        - |  493 | `					/* Current operator: \|\| */` |
|     3074 |  494 | `					pStream->zText++;` |
|     1674 |  495 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  496 | `					/* Current operator: \|= */` |
|        7 |  497 | `					pStream->zText++;` |
|        3 |  498 | `				}` |
|     1604 |  499 | `			}` |
|     3210 |  500 | `			break;` |
|     7725 |  501 | `		case '+':` |
|    15452 |  502 | `			if( pStream->zText < pStream->zEnd ){` |
|    15450 |  503 | `				if( pStream->zText[0] == '+' ){` |
|        - |  504 | `					/* Current operator: ++ */` |
|    12004 |  505 | `					pStream->zText++;` |
|     9449 |  506 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  507 | `					/* Current operator: += */` |
|       48 |  508 | `					pStream->zText++;` |
|       23 |  509 | `				}` |
|     7724 |  510 | `			}` |
|    15452 |  511 | `			break;` |
|    80272 |  512 | `		case '-':` |
|   160546 |  513 | `			if( pStream->zText < pStream->zEnd ){` |
|   160546 |  514 | `				if( pStream->zText[0] == '-' ){` |
|        - |  515 | `					/* Current operator: -- */` |
|        5 |  516 | `					pStream->zText++;` |
|   160544 |  517 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  518 | `					/* Current operator: -= */` |
|       10 |  519 | `					pStream->zText++;` |
|   160538 |  520 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  521 | `					/* Current operator: -> */` |
|   160048 |  522 | `					pStream->zText++;` |
|    80023 |  523 | `				}` |
|    80272 |  524 | `			}` |
|   160546 |  525 | `			break;` |
|       96 |  526 | `		case '*':` |
|      194 |  527 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  528 | `				/* Current operator: *= */` |
|       20 |  529 | `				pStream->zText++;` |
|        9 |  530 | `			}` |
|      194 |  531 | `			break;` |
|       32 |  532 | `		case '/':` |
|       66 |  533 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  534 | `				/* Current operator: /= */` |
|        5 |  535 | `				pStream->zText++;` |
|        2 |  536 | `			}` |
|       66 |  537 | `			break;` |
|       25 |  538 | `		case '%':` |
|       52 |  539 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  540 | `				/* Current operator: %= */` |
|        3 |  541 | `				pStream->zText++;` |
|        1 |  542 | `			}` |
|       52 |  543 | `			break;` |
|       11 |  544 | `		case '^':` |
|       23 |  545 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  546 | `				/* Current operator: ^= */` |
|        9 |  547 | `				pStream->zText++;` |
|        4 |  548 | `			}` |
|       23 |  549 | `			break;` |
|    40433 |  550 | `		case '.':` |
|    80868 |  551 | `			if( pStream->zText + 1 < pStream->zEnd && pStream->zText[0] == '.' && pStream->zText[1] == '.' ){` |
|        - |  552 | `				/* Ellipsis: ... */` |
|       58 |  553 | `				pStream->zText += 2;` |
|       58 |  554 | `				pToken->nType = PH7_TK_ELLIPSIS;` |
|    80840 |  555 | `			}else if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  556 | `				/* Current operator: .= */` |
|     3008 |  557 | `				pStream->zText++;` |
|     1503 |  558 | `			}` |
|    80868 |  559 | `			break;` |
|    25222 |  560 | `		case '<':` |
|    50446 |  561 | `			if( pStream->zText < pStream->zEnd ){` |
|    50446 |  562 | `				if( pStream->zText[0] == '<' ){` |
|        - |  563 | `					/* Current operator: << */` |
|      134 |  564 | `					pStream->zText++;` |
|      134 |  565 | `					if( pStream->zText < pStream->zEnd ){` |
|      134 |  566 | `						if( pStream->zText[0] == '=' ){` |
|        - |  567 | `							/* Current operator: <<= */` |
|        9 |  568 | `							pStream->zText++;` |
|      130 |  569 | `						}else if( pStream->zText[0] == '<' ){` |
|        - |  570 | `							/* Current Token: <<<  */` |
|      112 |  571 | `							pStream->zText++;` |
|        - |  572 | `							/* This may be the beginning of a Heredoc/Nowdoc string,try to delimit it */` |
|      112 |  573 | `							rc = LexExtractHeredoc(&(*pStream),&(*pToken));` |
|      112 |  574 | `							if( rc == SXRET_OK ){` |
|        - |  575 | `								/* Here/Now doc successfuly extracted */` |
|      112 |  576 | `								return SXRET_OK;` |
|        - |  577 | `							}` |
|      ! 0 |  578 | `						}` |
|       12 |  579 | `					}` |
|    50325 |  580 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  581 | `					/* Current operator: <> */` |
|        5 |  582 | `					pStream->zText++;` |
|    50312 |  583 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  584 | `					/* Current operator: <= or <=> */` |
|       94 |  585 | `					pStream->zText++;` |
|       94 |  586 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '>' ){` |
|        - |  587 | `						/* Current operator: <=> */` |
|       51 |  588 | `						pStream->zText++;` |
|       25 |  589 | `					}` |
|       46 |  590 | `				}` |
|    25167 |  591 | `			}` |
|    50336 |  592 | `			break;` |
|     3047 |  593 | `		case '>':` |
|     6096 |  594 | `			if( pStream->zText < pStream->zEnd ){` |
|     6096 |  595 | `				if( pStream->zText[0] == '>' ){` |
|        - |  596 | `					/* Current operator: >> */` |
|       21 |  597 | `					pStream->zText++;` |
|       21 |  598 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  599 | `						/* Current operator: >>= */` |
|       11 |  600 | `						pStream->zText++;` |
|        6 |  601 | `					}` |
|     6086 |  602 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  603 | `					/* Current operator: >= */` |
|       80 |  604 | `					pStream->zText++;` |
|       39 |  605 | `				}` |
|     3047 |  606 | `			}` |
|     6096 |  607 | `			break;` |
|     1177 |  608 | `		case '?':` |
|     2356 |  609 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '?' ){` |
|        - |  610 | `				/* Null coalescing operator: ?? */` |
|      136 |  611 | `				pStream->zText++;` |
|      136 |  612 | `				if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  613 | `					/* Null coalescing assignment operator (PHP 7.4) */` |
|       54 |  614 | `					pStream->zText++;` |
|       26 |  615 | `				}` |
|     2343 |  616 | `			}else if( (pStream->zEnd - pStream->zText) >= 2` |
|     2222 |  617 | `				&& pStream->zText[0] == '-' && pStream->zText[1] == '>' ){` |
|        - |  618 | `				/* Nullsafe object operator (PHP 8.0): ?-> */` |
|      110 |  619 | `				pStream->zText += 2;` |
|       54 |  620 | `			}` |
|     2354 |  621 | `			break;` |
|      105 |  622 | `		default:` |
|      210 |  623 | `			break;` |
|        - |  624 | `		}` |
|  5072912 |  625 | `		if( pStr->nByte <= 0 ){` |
|        - |  626 | `			/* Record token length */` |
|  5072864 |  627 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  2536431 |  628 | `		}` |
|  5072912 |  629 | `		if( pToken->nType & PH7_TK_OP ){` |
|        - |  630 | `			const ph7_expr_op *pOp;` |
|        - |  631 | `			/* Check if the extracted token is an operator */` |
|   883010 |  632 | `			pOp = PH7_ExprExtractOperator(pStr,(SyToken *)SySetPeek(pStream->pSet));` |
|   883010 |  633 | `			if( pOp == 0 ){` |
|        - |  634 | `				/* Not an operator */` |
|      ! 0 |  635 | `				pToken->nType &= ~PH7_TK_OP;` |
|      ! 0 |  636 | `				if( pToken->nType <= 0 ){` |
|      ! 0 |  637 | `					pToken->nType = PH7_TK_OTHER;` |
|      ! 0 |  638 | `				}` |
|      ! 0 |  639 | `			}else{` |
|        - |  640 | `				/* Save the instance associated with this operator for later processing */` |
|   883010 |  641 | `				pToken->pUserData = (void *)pOp;` |
|        - |  642 | `			}` |
|   441504 |  643 | `		}` |
|        - |  644 | `	}` |
|        - |  645 | `	/* Tell the upper-layer to save the extracted token for later processing */` |
|  8194634 |  646 | `	return SXRET_OK;` |
|  4239566 |  647 |  |
|        - |  648 | `/***** This file contains automatically generated code ******` |
|        - |  649 | `**` |
|        - |  650 | `** The code in this file has been automatically generated by` |
|        - |  651 | `**` |
|        - |  652 | `**     $Header: /sqlite/sqlite/tool/mkkeywordhash.c` |
|        - |  653 | `**` |
|        - |  654 | `** Sligthly modified by Chems mrad <chm@symisc.net> for the PH7 engine.` |
|        - |  655 | `**` |
|        - |  656 | `** The code in this file implements a function that determines whether` |
|        - |  657 | `** or not a given identifier is really a PHP keyword.  The same thing` |
|        - |  658 | `** might be implemented more directly using a hand-written hash table.` |
|        - |  659 | `** But by using this automatically generated code, the size of the code` |
|        - |  660 | `** is substantially reduced.  This is important for embedded applications` |
|        - |  661 | `** on platforms with limited memory.` |
|        - |  662 | `*/` |
|        - |  663 | `/* Hash score: 103 */` |
|  3121724 |  664 | `static sxu32 KeywordCode(const char *z, int n){` |
|        - |  665 | `  /* zText[] encodes 532 bytes of keywords in 333 bytes */` |
|        - |  666 | `  /*   extendswitchprintegerequire_oncenddeclareturnamespacechobject      */` |
|        - |  667 | `  /*   hrowbooleandefaultrycaselfinalistaticlonewconstringlobaluse        */` |
|        - |  668 | `  /*   lseifloatvarrayANDIEchoUSECHOabstractclasscontinuendifunction      */` |
|        - |  669 | `  /*   diendwhilevaldoexitgotoimplementsinclude_oncemptyinstanceof        */` |
|        - |  670 | `  /*   interfacendforeachissetparentprivateprotectedpublicatchunset       */` |
|        - |  671 | `  /*   xorARRAYASArrayEXITUNSETXORbreak                                   */` |
|        - |  672 | `  static const char zText[332] = {` |
|        - |  673 | `    'e','x','t','e','n','d','s','w','i','t','c','h','p','r','i','n','t','e',` |
|        - |  674 | `    'g','e','r','e','q','u','i','r','e','_','o','n','c','e','n','d','d','e',` |
|        - |  675 | `    'c','l','a','r','e','t','u','r','n','a','m','e','s','p','a','c','e','c',` |
|        - |  676 | `    'h','o','b','j','e','c','t','h','r','o','w','b','o','o','l','e','a','n',` |
|        - |  677 | `    'd','e','f','a','u','l','t','r','y','c','a','s','e','l','f','i','n','a',` |
|        - |  678 | `    'l','i','s','t','a','t','i','c','l','o','n','e','w','c','o','n','s','t',` |
|        - |  679 | `    'r','i','n','g','l','o','b','a','l','u','s','e','l','s','e','i','f','l',` |
|        - |  680 | `    'o','a','t','v','a','r','r','a','y','A','N','D','I','E','c','h','o','U',` |
|        - |  681 | `    'S','E','C','H','O','a','b','s','t','r','a','c','t','c','l','a','s','s',` |
|        - |  682 | `    'c','o','n','t','i','n','u','e','n','d','i','f','u','n','c','t','i','o',` |
|        - |  683 | `    'n','d','i','e','n','d','w','h','i','l','e','v','a','l','d','o','e','x',` |
|        - |  684 | `    'i','t','g','o','t','o','i','m','p','l','e','m','e','n','t','s','i','n',` |
|        - |  685 | `    'c','l','u','d','e','_','o','n','c','e','m','p','t','y','i','n','s','t',` |
|        - |  686 | `    'a','n','c','e','o','f','i','n','t','e','r','f','a','c','e','n','d','f',` |
|        - |  687 | `    'o','r','e','a','c','h','i','s','s','e','t','p','a','r','e','n','t','p',` |
|        - |  688 | `    'r','i','v','a','t','e','p','r','o','t','e','c','t','e','d','p','u','b',` |
|        - |  689 | `    'l','i','c','a','t','c','h','u','n','s','e','t','x','o','r','A','R','R',` |
|        - |  690 | `    'A','Y','A','S','A','r','r','a','y','E','X','I','T','U','N','S','E','T',` |
|        - |  691 | `    'X','O','R','b','r','e','a','k'` |
|        - |  692 | `  };` |
|        - |  693 | `  static const unsigned char aHash[151] = {` |
|        - |  694 |  |
|        - |  695 |  |
|        - |  696 | `      74,   0,  51,   0,  76,   0,   0,  20,   0,   0,   0,  50,   0,` |
|        - |  697 | `      80,  34,   0,  36,   0,   0,  64,  16,   0,   0,  17,   0,   1,` |
|        - |  698 | `      19,  84,  66,   0,  43,  45,  78,   0,   0,  53,  56,   0,   0,` |
|        - |  699 |  |
|        - |  700 | `      72,  14,   0,  71,   0,  38,   6,   0,   0,   0,  73,   0,   0,` |
|        - |  701 | `       3,   0,  41,   5,  52,  57,  32,   0,  60,  63,   0,  69,  82,` |
|        - |  702 | `      30,   0,  79,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,` |
|        - |  703 |  |
|        - |  704 | `      62,   0,  11,   0,   0,  58,   0,   0,   0,   0,  59,  75,   0,` |
|        - |  705 |  |
|        - |  706 | `  };` |
|        - |  707 | `  static const unsigned char aNext[84] = {` |
|        - |  708 |  |
|        - |  709 |  |
|        - |  710 |  |
|        - |  711 |  |
|        - |  712 |  |
|        - |  713 |  |
|        - |  714 | `      42,   0,   0,   0,  70,  55` |
|        - |  715 | `  };` |
|        - |  716 | `  static const unsigned char aLen[84] = {` |
|        - |  717 | `       7,   9,   6,   5,   7,  12,   7,   2,  10,   7,   6,   9,   4,` |
|        - |  718 | `       6,   5,   7,   4,   3,   7,   3,   4,   4,   5,   4,   6,   5,` |
|        - |  719 | `       2,   3,   5,   6,   6,   3,   6,   4,   2,   5,   3,   5,   3,` |
|        - |  720 | `       3,   4,   3,   4,   8,   5,   2,   8,   5,   8,   3,   8,   5,` |
|        - |  721 | `       4,   2,   4,   4,  10,  12,   7,   5,  10,   9,   3,   6,  10,` |
|        - |  722 | `       3,   7,   2,   5,   6,   7,   9,   6,   5,   5,   3,   5,   2,` |
|        - |  723 | `       5,   4,   5,   3,   2,   5` |
|        - |  724 | `  };` |
|        - |  725 | `  static const sxu16 aOffset[84] = {` |
|        - |  726 |  |
|        - |  727 | `      55,  60,  65,  65,  70,  72,  78,  81,  83,  86,  90,  92,  97,` |
|        - |  728 | `     100, 100, 103, 106, 111, 117, 119, 119, 123, 124, 129, 130, 135,` |
|        - |  729 | `     137, 139, 143, 145, 149, 157, 159, 162, 169, 173, 181, 183, 186,` |
|        - |  730 | `     190, 194, 196, 200, 204, 214, 214, 225, 230, 240, 240, 248, 248,` |
|        - |  731 | `     251, 251, 252, 258, 263, 269, 276, 285, 290, 295, 300, 303, 308,` |
|        - |  732 | `     310, 315, 319, 324, 325, 327` |
|        - |  733 | `  };` |
|        - |  734 | `  static const sxu32 aCode[84] = {` |
|        - |  735 | `    PH7_TKWRD_EXTENDS,   PH7_TKWRD_ENDSWITCH,   PH7_TKWRD_SWITCH,    PH7_TKWRD_PRINT,   PH7_TKWRD_INT,` |
|        - |  736 | `    PH7_TKWRD_REQONCE,   PH7_TKWRD_REQUIRE,     PH7_TKWRD_SEQ,       PH7_TKWRD_ENDDEC,    PH7_TKWRD_DECLARE,` |
|        - |  737 | `    PH7_TKWRD_RETURN,    PH7_TKWRD_NAMESPACE,   PH7_TKWRD_ECHO,      PH7_TKWRD_OBJECT,    PH7_TKWRD_THROW,` |
|        - |  738 | `    PH7_TKWRD_BOOL,      PH7_TKWRD_BOOL,        PH7_TKWRD_AND,       PH7_TKWRD_DEFAULT,   PH7_TKWRD_TRY,` |
|        - |  739 | `    PH7_TKWRD_CASE,      PH7_TKWRD_SELF,        PH7_TKWRD_FINAL,     PH7_TKWRD_LIST,      PH7_TKWRD_STATIC,` |
|        - |  740 | `    PH7_TKWRD_CLONE,     PH7_TKWRD_SNE,         PH7_TKWRD_NEW,       PH7_TKWRD_CONST,     PH7_TKWRD_STRING,` |
|        - |  741 | `    PH7_TKWRD_GLOBAL,    PH7_TKWRD_USE,         PH7_TKWRD_ELIF,      PH7_TKWRD_ELSE,      PH7_TKWRD_IF,` |
|        - |  742 | `    PH7_TKWRD_FLOAT,     PH7_TKWRD_VAR,         PH7_TKWRD_ARRAY,     PH7_TKWRD_AND,       PH7_TKWRD_DIE,` |
|        - |  743 | `    PH7_TKWRD_ECHO,      PH7_TKWRD_USE,         PH7_TKWRD_ECHO,      PH7_TKWRD_ABSTRACT,  PH7_TKWRD_CLASS,` |
|        - |  744 | `    PH7_TKWRD_AS,        PH7_TKWRD_CONTINUE,    PH7_TKWRD_ENDIF,     PH7_TKWRD_FUNCTION,  PH7_TKWRD_DIE,` |
|        - |  745 | `    PH7_TKWRD_ENDWHILE,  PH7_TKWRD_WHILE,       PH7_TKWRD_EVAL,      PH7_TKWRD_DO,        PH7_TKWRD_EXIT,` |
|        - |  746 | `    PH7_TKWRD_GOTO,      PH7_TKWRD_IMPLEMENTS,  PH7_TKWRD_INCONCE,   PH7_TKWRD_INCLUDE,   PH7_TKWRD_EMPTY,` |
|        - |  747 | `    PH7_TKWRD_INSTANCEOF,PH7_TKWRD_INTERFACE,   PH7_TKWRD_INT,       PH7_TKWRD_ENDFOR,    PH7_TKWRD_END4EACH,` |
|        - |  748 | `    PH7_TKWRD_FOR,       PH7_TKWRD_FOREACH,     PH7_TKWRD_OR,        PH7_TKWRD_ISSET,     PH7_TKWRD_PARENT,` |
|        - |  749 | `    PH7_TKWRD_PRIVATE,   PH7_TKWRD_PROTECTED,   PH7_TKWRD_PUBLIC,    PH7_TKWRD_CATCH,     PH7_TKWRD_UNSET,` |
|        - |  750 | `    PH7_TKWRD_XOR,       PH7_TKWRD_ARRAY,       PH7_TKWRD_AS,        PH7_TKWRD_ARRAY,     PH7_TKWRD_EXIT,` |
|        - |  751 | `    PH7_TKWRD_UNSET,     PH7_TKWRD_XOR,         PH7_TKWRD_OR,        PH7_TKWRD_BREAK` |
|        - |  752 | `  };` |
|        - |  753 | `  int h, i;` |
|  3121724 |  754 | `  if( n<2 ) return PH7_TK_ID;` |
|  3020776 |  755 | `  h = (((int)z[0]*4) ^ ((int)z[n-1]*3) ^ n) % 151;` |
|  4621778 |  756 | `  for(i=((int)aHash[h])-1; i>=0; i=((int)aNext[i])-1){` |
|  2699622 |  757 | `    if( (int)aLen[i]==n && SyMemcmp(&zText[aOffset[i]],z,n)==0 ){` |
|        - |  758 | `       /* PH7_TKWRD_EXTENDS */` |
|        - |  759 | `       /* PH7_TKWRD_ENDSWITCH */` |
|        - |  760 | `       /* PH7_TKWRD_SWITCH */` |
|        - |  761 | `       /* PH7_TKWRD_PRINT */` |
|        - |  762 | `       /* PH7_TKWRD_INT */` |
|        - |  763 | `       /* PH7_TKWRD_REQONCE */` |
|        - |  764 | `       /* PH7_TKWRD_REQUIRE */` |
|        - |  765 | `       /* PH7_TKWRD_SEQ */` |
|        - |  766 | `       /* PH7_TKWRD_ENDDEC */` |
|        - |  767 | `       /* PH7_TKWRD_DECLARE */` |
|        - |  768 | `       /* PH7_TKWRD_RETURN */` |
|        - |  769 | `       /* PH7_TKWRD_NAMESPACE */` |
|        - |  770 | `       /* PH7_TKWRD_ECHO */` |
|        - |  771 | `       /* PH7_TKWRD_OBJECT */` |
|        - |  772 | `       /* PH7_TKWRD_THROW */` |
|        - |  773 | `       /* PH7_TKWRD_BOOL */` |
|        - |  774 | `       /* PH7_TKWRD_BOOL */` |
|        - |  775 | `       /* PH7_TKWRD_AND */` |
|        - |  776 | `       /* PH7_TKWRD_DEFAULT */` |
|        - |  777 | `       /* PH7_TKWRD_TRY */` |
|        - |  778 | `       /* PH7_TKWRD_CASE */` |
|        - |  779 | `       /* PH7_TKWRD_SELF */` |
|        - |  780 | `       /* PH7_TKWRD_FINAL */` |
|        - |  781 | `       /* PH7_TKWRD_LIST */` |
|        - |  782 | `       /* PH7_TKWRD_STATIC */` |
|        - |  783 | `       /* PH7_TKWRD_CLONE */` |
|        - |  784 | `       /* PH7_TKWRD_SNE */` |
|        - |  785 | `       /* PH7_TKWRD_NEW */` |
|        - |  786 | `       /* PH7_TKWRD_CONST */` |
|        - |  787 | `       /* PH7_TKWRD_STRING */` |
|        - |  788 | `       /* PH7_TKWRD_GLOBAL */` |
|        - |  789 | `       /* PH7_TKWRD_USE */` |
|        - |  790 | `       /* PH7_TKWRD_ELIF */` |
|        - |  791 | `       /* PH7_TKWRD_ELSE */` |
|        - |  792 | `       /* PH7_TKWRD_IF */` |
|        - |  793 | `       /* PH7_TKWRD_FLOAT */` |
|        - |  794 | `       /* PH7_TKWRD_VAR */` |
|        - |  795 | `       /* PH7_TKWRD_ARRAY */` |
|        - |  796 | `       /* PH7_TKWRD_AND */` |
|        - |  797 | `       /* PH7_TKWRD_DIE */` |
|        - |  798 | `       /* PH7_TKWRD_ECHO */` |
|        - |  799 | `       /* PH7_TKWRD_USE */` |
|        - |  800 | `       /* PH7_TKWRD_ECHO */` |
|        - |  801 | `       /* PH7_TKWRD_ABSTRACT */` |
|        - |  802 | `       /* PH7_TKWRD_CLASS */` |
|        - |  803 | `       /* PH7_TKWRD_AS */` |
|        - |  804 | `       /* PH7_TKWRD_CONTINUE */` |
|        - |  805 | `       /* PH7_TKWRD_ENDIF */` |
|        - |  806 | `       /* PH7_TKWRD_FUNCTION */` |
|        - |  807 | `       /* PH7_TKWRD_DIE */` |
|        - |  808 | `       /* PH7_TKWRD_ENDWHILE */` |
|        - |  809 | `       /* PH7_TKWRD_WHILE */` |
|        - |  810 | `       /* PH7_TKWRD_EVAL */` |
|        - |  811 | `       /* PH7_TKWRD_DO */` |
|        - |  812 | `       /* PH7_TKWRD_EXIT */` |
|        - |  813 | `       /* PH7_TKWRD_GOTO */` |
|        - |  814 | `       /* PH7_TKWRD_IMPLEMENTS */` |
|        - |  815 | `       /* PH7_TKWRD_INCONCE */` |
|        - |  816 | `       /* PH7_TKWRD_INCLUDE */` |
|        - |  817 | `       /* PH7_TKWRD_EMPTY */` |
|        - |  818 | `       /* PH7_TKWRD_INSTANCEOF */` |
|        - |  819 | `       /* PH7_TKWRD_INTERFACE */` |
|        - |  820 | `       /* PH7_TKWRD_INT */` |
|        - |  821 | `       /* PH7_TKWRD_ENDFOR */` |
|        - |  822 | `       /* PH7_TKWRD_END4EACH */` |
|        - |  823 | `       /* PH7_TKWRD_FOR */` |
|        - |  824 | `       /* PH7_TKWRD_FOREACH */` |
|        - |  825 | `       /* PH7_TKWRD_OR */` |
|        - |  826 | `       /* PH7_TKWRD_ISSET */` |
|        - |  827 | `       /* PH7_TKWRD_PARENT */` |
|        - |  828 | `       /* PH7_TKWRD_PRIVATE */` |
|        - |  829 | `       /* PH7_TKWRD_PROTECTED */` |
|        - |  830 | `       /* PH7_TKWRD_PUBLIC */` |
|        - |  831 | `       /* PH7_TKWRD_CATCH */` |
|        - |  832 | `       /* PH7_TKWRD_UNSET */` |
|        - |  833 | `       /* PH7_TKWRD_XOR */` |
|        - |  834 | `       /* PH7_TKWRD_ARRAY */` |
|        - |  835 | `       /* PH7_TKWRD_AS */` |
|        - |  836 | `       /* PH7_TKWRD_ARRAY */` |
|        - |  837 | `       /* PH7_TKWRD_EXIT */` |
|        - |  838 | `       /* PH7_TKWRD_UNSET */` |
|        - |  839 | `       /* PH7_TKWRD_XOR */` |
|        - |  840 | `       /* PH7_TKWRD_OR */` |
|        - |  841 | `       /* PH7_TKWRD_BREAK */` |
|  1098620 |  842 | `      return aCode[i];` |
|        - |  843 | `    }` |
|   800501 |  844 | `  }` |
|        - |  845 | `  /* Linear fallback for keywords not in the auto-generated hash table */` |
|  1922158 |  846 | `  if( n==5 && SyMemcmp(z,"trait",5)==0 ) return PH7_TKWRD_TRAIT;` |
|  1922102 |  847 | `  if( n==9 && SyMemcmp(z,"insteadof",9)==0 ) return PH7_TKWRD_INSTEADOF;` |
|  1922098 |  848 | `  if( n==7 && SyMemcmp(z,"finally",7)==0 ) return PH7_TKWRD_FINALLY;` |
|  1922068 |  849 | `  if( n==5 && SyMemcmp(z,"yield",5)==0 ) return PH7_TKWRD_YIELD;` |
|  1922034 |  850 | `  if( n==5 && SyMemcmp(z,"match",5)==0 ) return PH7_TKWRD_MATCH;` |
|  1921964 |  851 | `  return PH7_TK_ID;` |
|  1560863 |  852 |  |
|        - |  853 | `/* --- End of Automatically generated code --- */` |
|        - |  854 | `/*` |
|        - |  855 | ` * Extract a heredoc/nowdoc text from a raw PHP input.` |
|        - |  856 | ` * According to the PHP language reference manual:` |
|        - |  857 | ` *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier` |
|        - |  858 | ` *  is provided, then a newline. The string itself follows, and then the same identifier again` |
|        - |  859 | ` *  to close the quotation.` |
|        - |  860 | ` *  The closing identifier must begin in the first column of the line. Also, the identifier must` |
|        - |  861 | ` *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric` |
|        - |  862 | ` *  characters and underscores, and must start with a non-digit character or underscore.` |
|        - |  863 | ` *  Heredoc text behaves just like a double-quoted string, without the double quotes.` |
|        - |  864 | ` *  This means that quotes in a heredoc do not need to be escaped, but the escape codes listed` |
|        - |  865 | ` *  above can still be used. Variables are expanded, but the same care must be taken when expressing` |
|        - |  866 | ` *  complex variables inside a heredoc as with strings.` |
|        - |  867 | ` *  Nowdocs are to single-quoted strings what heredocs are to double-quoted strings.` |
|        - |  868 | ` *  A nowdoc is specified similarly to a heredoc, but no parsing is done inside a nowdoc.` |
|        - |  869 | ` *  The construct is ideal for embedding PHP code or other large blocks of text without the need` |
|        - |  870 | ` *  for escaping. It shares some features in common with the SGML <![CDATA[ ]]> construct, in that` |
|        - |  871 | ` *  it declares a block of text which is not for parsing.` |
|        - |  872 | ` *  A nowdoc is identified with the same <<< sequence used for heredocs, but the identifier which follows` |
|        - |  873 | ` *  is enclosed in single quotes, e.g. <<<'EOT'. All the rules for heredoc identifiers also apply to nowdoc` |
|        - |  874 | ` *  identifiers, especially those regarding the appearance of the closing identifier.` |
|        - |  875 | ` * Symisc Extension:` |
|        - |  876 | ` * The closing delimiter can now start with a digit or undersocre or it can be an UTF-8 stream.` |
|        - |  877 | ` * Example:` |
|        - |  878 | ` *  <<<123` |
|        - |  879 | ` *    HEREDOC Here` |
|        - |  880 | ` * 123` |
|        - |  881 | ` *  or` |
|        - |  882 | ` *  <<<___` |
|        - |  883 | ` *   HEREDOC Here` |
|        - |  884 | ` *  ___` |
|        - |  885 | ` */` |
|      110 |  886 | `static sxi32 LexExtractHeredoc(SyStream *pStream,SyToken *pToken)` |
|        2 |  887 |  |
|      112 |  888 | `	const unsigned char *zIn  = pStream->zText;` |
|      112 |  889 | `	const unsigned char *zEnd = pStream->zEnd;` |
|        - |  890 | `	const unsigned char *zPtr;` |
|      112 |  891 | `	sxu8 bNowDoc = FALSE;` |
|        - |  892 | `	SyString sDelim;` |
|        - |  893 | `	SyString sStr;` |
|        - |  894 | `	/* Jump leading white spaces */` |
|      124 |  895 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|       13 |  896 | `		zIn++;` |
|        1 |  897 | `	}` |
|      112 |  898 | `	if( zIn >= zEnd ){` |
|        - |  899 | `		/* A simple symbol,return immediately */` |
|      ! 0 |  900 | `		return SXERR_CONTINUE;` |
|        - |  901 | `	}` |
|      112 |  902 | `	if( zIn[0] == '\'' \|\| zIn[0] == '"' ){` |
|        - |  903 | `		/* Make sure we are dealing with a nowdoc */` |
|       44 |  904 | `		bNowDoc =  zIn[0] == '\'' ? TRUE : FALSE;` |
|       44 |  905 | `		zIn++;` |
|       21 |  906 | `	}` |
|      112 |  907 | `	if( zIn[0] < 0xc0 && !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|        - |  908 | `		/* Invalid delimiter,return immediately */` |
|      ! 0 |  909 | `		return SXERR_CONTINUE;` |
|        - |  910 | `	}` |
|        - |  911 | `	/* Isolate the identifier */` |
|      112 |  912 | `	sDelim.zString = (const char *)zIn;` |
|      118 |  913 | `	for(;;){` |
|      238 |  914 | `		zPtr = zIn;` |
|        - |  915 | `		/* Skip alphanumeric stream */` |
|      756 |  916 | `		while( zPtr < zEnd && zPtr[0] < 0xc0 && (SyisAlphaNum(zPtr[0]) \|\| zPtr[0] == '_') ){` |
|      402 |  917 | `			zPtr++;` |
|        2 |  918 | `		}` |
|      238 |  919 | `		if( zPtr < zEnd && zPtr[0] >= 0xc0 ){` |
|       19 |  920 | `			zPtr++;` |
|        - |  921 | `			/* UTF-8 stream */` |
|       37 |  922 | `			while( zPtr < zEnd && ((zPtr[0] & 0xc0) == 0x80) ){` |
|       19 |  923 | `				zPtr++;` |
|        1 |  924 | `			}` |
|        9 |  925 | `		}` |
|      238 |  926 | `		if( zPtr == zIn ){` |
|        - |  927 | `			/* Not an UTF-8 or alphanumeric stream */` |
|      112 |  928 | `			break;` |
|        - |  929 | `		}` |
|        - |  930 | `		/* Synchronize pointers */` |
|      128 |  931 | `		zIn = zPtr;` |
|        2 |  932 | `	}` |
|        - |  933 | `	/* Get the identifier length */` |
|      112 |  934 | `	sDelim.nByte = (sxu32)((const char *)zIn-sDelim.zString);` |
|      112 |  935 | `	if( zIn[0] == '"' \|\| (bNowDoc && zIn[0] == '\'') ){` |
|        - |  936 | `		/* Jump the trailing single quote */` |
|       44 |  937 | `		zIn++;` |
|       21 |  938 | `	}` |
|        - |  939 | `	/* Jump trailing white spaces */` |
|      112 |  940 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|      ! 0 |  941 | `		zIn++;` |
|      ! 0 |  942 | `	}` |
|      112 |  943 | `	if( sDelim.nByte <= 0 \|\| zIn >= zEnd \|\| zIn[0] != '\n' ){` |
|        - |  944 | `		/* Invalid syntax */` |
|      ! 0 |  945 | `		return SXERR_CONTINUE;` |
|        - |  946 | `	}` |
|      112 |  947 | `	pStream->nLine++; /* Increment line counter */` |
|      112 |  948 | `	zIn++;` |
|        - |  949 | `	/* Isolate the delimited string */` |
|      112 |  950 | `	sStr.zString = (const char *)zIn;` |
|        - |  951 | `	/* PHP 7.3 flexible heredoc/nowdoc: the closing marker may be preceded` |
|        - |  952 | `	 * by whitespace (spaces/tabs), and may be followed by any non-identifier` |
|        - |  953 | `	 * character. The indent count is recorded in pToken->pUserData and the` |
|        - |  954 | `	 * compile phase strips it from each body line. */` |
|        - |  955 | `	{` |
|      112 |  956 | `		const unsigned char *zMarkerLine = zIn; /* Start of marker's line (set on match) */` |
|      112 |  957 | `		sxu32 nIndent = 0;` |
|      225 |  958 | `		for(;;){` |
|      282 |  959 | `			const unsigned char *zLineStart = zIn;` |
|        - |  960 | `			/* Skip leading space/tab on this line */` |
|      806 |  961 | `			while( zIn < zEnd && (zIn[0] == ' ' \|\| zIn[0] == '\t') ){` |
|      386 |  962 | `				zIn++;` |
|        2 |  963 | `			}` |
|      280 |  964 | `			if( (sxu32)(zEnd - zIn) >= sDelim.nByte` |
|      281 |  965 | `				&& SyMemcmp((const void *)sDelim.zString,(const void *)zIn,sDelim.nByte) == 0 ){` |
|        - |  966 | `				int bIdentCont;` |
|      110 |  967 | `				zPtr = &zIn[sDelim.nByte];` |
|        - |  968 | `				/* Disambiguate: next byte must not continue an identifier.` |
|        - |  969 | `				 * A leading byte >= 0xc0 starts a multi-byte UTF-8 sequence,` |
|        - |  970 | `				 * which PHP identifiers may contain, so treat it as ident. */` |
|      110 |  971 | `				if( zPtr >= zEnd ){` |
|      ! 0 |  972 | `					bIdentCont = 0;` |
|      110 |  973 | `				}else if( zPtr[0] >= 0xc0 ){` |
|      ! 0 |  974 | `					bIdentCont = 1;` |
|      ! 0 |  975 | `				}else{` |
|      110 |  976 | `					bIdentCont = (SyisAlphaNum(zPtr[0]) \|\| zPtr[0] == '_');` |
|        - |  977 | `				}` |
|      110 |  978 | `				if( !bIdentCont ){` |
|        - |  979 | `					/* Closing marker found */` |
|      110 |  980 | `					nIndent = (sxu32)(zIn - zLineStart);` |
|      110 |  981 | `					zMarkerLine = zLineStart;` |
|      110 |  982 | `					pStream->zText = zPtr; /* Cursor right after identifier */` |
|      110 |  983 | `					break;` |
|        - |  984 | `				}` |
|      ! 0 |  985 | `			}` |
|        - |  986 | `			/* Not the closing marker on this line; walk to next newline */` |
|     2824 |  987 | `			while( zIn < zEnd && zIn[0] != '\n' ){` |
|     2652 |  988 | `				zIn++;` |
|        2 |  989 | `			}` |
|      174 |  990 | `			if( zIn >= zEnd ){` |
|        - |  991 | `				/* End of input without finding the closing marker */` |
|        3 |  992 | `				pStream->zText = pStream->zEnd;` |
|        3 |  993 | `				zMarkerLine = zIn;` |
|        3 |  994 | `				break;` |
|        - |  995 | `			}` |
|      172 |  996 | `			pStream->nLine++;` |
|      172 |  997 | `			zIn++;` |
|        2 |  998 | `		}` |
|        - |  999 | `		/* Body runs from sStr.zString up to just before the marker line */` |
|      112 | 1000 | `		sStr.nByte = (sxu32)((const char *)zMarkerLine - sStr.zString);` |
|      112 | 1001 | `		pToken->nType = bNowDoc ? PH7_TK_NOWDOC : PH7_TK_HEREDOC;` |
|      112 | 1002 | `		SyStringDupPtr(&pToken->sData,&sStr);` |
|        - | 1003 | `		/* Strip exactly one line terminator that precedes the marker's line. */` |
|      110 | 1004 | `		if( pToken->sData.nByte > 0` |
|      108 | 1005 | `			&& pToken->sData.zString[pToken->sData.nByte - 1] == '\n' ){` |
|      102 | 1006 | `			pToken->sData.nByte--;` |
|      100 | 1007 | `			if( pToken->sData.nByte > 0` |
|      102 | 1008 | `				&& pToken->sData.zString[pToken->sData.nByte - 1] == '\r' ){` |
|      ! 0 | 1009 | `				pToken->sData.nByte--;` |
|      ! 0 | 1010 | `			}` |
|       50 | 1011 | `		}` |
|      112 | 1012 | `		pToken->pUserData = SX_INT_TO_PTR(nIndent);` |
|        - | 1013 | `	}` |
|        - | 1014 | `	/* All done */` |
|      112 | 1015 | `	return SXRET_OK;` |
|       57 | 1016 |  |
|        - | 1017 | `/*` |
|        - | 1018 | ` * Tokenize a raw PHP input.` |
|        - | 1019 | ` * This is the public tokenizer called by most code generator routines.` |
|        - | 1020 | ` */` |
|    14456 | 1021 | `PH7_PRIVATE sxi32 PH7_TokenizePHP(const char *zInput,sxu32 nLen,sxu32 nLineStart,SySet *pOut)` |
|        2 | 1022 |  |
|        - | 1023 | `	SyLex sLexer;` |
|        - | 1024 | `	sxi32 rc;` |
|        - | 1025 | `	/* Initialize the lexer */` |
|    14458 | 1026 | `	rc = SyLexInit(&sLexer,&(*pOut),TokenizePHP,0);` |
|    14458 | 1027 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1028 | `		return rc;` |
|        - | 1029 | `	}` |
|    14458 | 1030 | `	sLexer.sStream.nLine = nLineStart;` |
|        - | 1031 | `	/* Tokenize input */` |
|    14458 | 1032 | `	rc = SyLexTokenizeInput(&sLexer,zInput,nLen,0,0,0);` |
|        - | 1033 | `	/* Release the lexer */` |
|    14458 | 1034 | `	SyLexRelease(&sLexer);` |
|        - | 1035 | `	/* Tokenization result */` |
|    14458 | 1036 | `	return rc;` |
|     7230 | 1037 |  |
|        - | 1038 | `/*` |
|        - | 1039 | ` * High level public tokenizer.` |
|        - | 1040 | ` *  Tokenize the input into PHP tokens and raw tokens [i.e: HTML,XML,Raw text...].` |
|        - | 1041 | ` * According to the PHP language reference manual` |
|        - | 1042 | ` *   When PHP parses a file, it looks for opening and closing tags, which tell PHP` |
|        - | 1043 | ` *   to start and stop interpreting the code between them. Parsing in this manner allows` |
|        - | 1044 | ` *   PHP to be embedded in all sorts of different documents, as everything outside of a pair` |
|        - | 1045 | ` *   of opening and closing tags is ignored by the PHP parser. Most of the time you will see` |
|        - | 1046 | ` *   PHP embedded in HTML documents, as in this example.` |
|        - | 1047 | ` *   <?php echo 'While this is going to be parsed.'; ?>` |
|        - | 1048 | ` *   <p>This will also be ignored.</p>` |
|        - | 1049 | ` *   You can also use more advanced structures:` |
|        - | 1050 | ` *   Example #1 Advanced escaping` |
|        - | 1051 | ` * <?php` |
|        - | 1052 | ` * if ($expression) {` |
|        - | 1053 | ` *   ?>` |
|        - | 1054 | ` *   <strong>This is true.</strong>` |
|        - | 1055 | ` *   <?php` |
|        - | 1056 | ` * } else {` |
|        - | 1057 | ` *   ?>` |
|        - | 1058 | ` *   <strong>This is false.</strong>` |
|        - | 1059 | ` *   <?php` |
|        - | 1060 | ` * }` |
|        - | 1061 | ` * ?>` |
|        - | 1062 | ` * This works as expected, because when PHP hits the ?> closing tags, it simply starts outputting` |
|        - | 1063 | ` * whatever it finds (except for an immediately following newline - see instruction separation ) until it hits` |
|        - | 1064 | ` * another opening tag. The example given here is contrived, of course, but for outputting large blocks of text` |
|        - | 1065 | ` * dropping out of PHP parsing mode is generally more efficient than sending all of the text through echo() or print().` |
|        - | 1066 | ` * There are four different pairs of opening and closing tags which can be used in PHP. Three of those, <?php ?>` |
|        - | 1067 | ` * <script language="php"> </script>  and <? ?> are always available. The other two are short tags and ASP style` |
|        - | 1068 | ` * tags, and can be turned on and off from the php.ini configuration file. As such, while some people find short tags` |
|        - | 1069 | ` * and ASP style tags convenient, they are less portable, and generally not recommended.` |
|        - | 1070 | ` * Note:` |
|        - | 1071 | ` * Also note that if you are embedding PHP within XML or XHTML you will need to use the <?php ?> tags to remain` |
|        - | 1072 | ` * compliant with standards.` |
|        - | 1073 | ` * Example #2 PHP Opening and Closing Tags` |
|        - | 1074 | ` * 1.  <?php echo 'if you want to serve XHTML or XML documents, do it like this'; ?>` |
|        - | 1075 | ` * 2.  <script language="php">` |
|        - | 1076 | ` *       echo 'some editors (like FrontPage) don\'t` |
|        - | 1077 | ` *             like processing instructions';` |
|        - | 1078 | ` *   </script>` |
|        - | 1079 | ` *` |
|        - | 1080 | ` * 3.  <? echo 'this is the simplest, an SGML processing instruction'; ?>` |
|        - | 1081 | ` *   <?= expression ?> This is a shortcut for "<? echo expression ?>"` |
|        - | 1082 | ` */` |
|    11858 | 1083 | `PH7_PRIVATE sxi32 PH7_TokenizeRawText(const char *zInput,sxu32 nLen,SySet *pOut)` |
|        2 | 1084 |  |
|    11860 | 1085 | `	const char *zEnd = &zInput[nLen];` |
|    11860 | 1086 | `	const char *zIn  = zInput;` |
|        - | 1087 | `	const char *zCur,*zCurEnd;` |
|    11860 | 1088 | `	SyString sCtag = { 0, 0 };     /* Closing tag */` |
|        - | 1089 | `	SyToken sToken;` |
|        - | 1090 | `	SyString sDoc;` |
|        - | 1091 | `	sxu32 nLine;` |
|        - | 1092 | `	sxi32 iNest;` |
|        - | 1093 | `	sxi32 rc;` |
|        - | 1094 | `	/* Tokenize the input into PHP tokens and raw tokens */` |
|    11860 | 1095 | `	nLine = 1;` |
|    11860 | 1096 | `	zCur = zCurEnd   = 0; /* Prevent compiler warning */` |
|    11860 | 1097 | `	sToken.pUserData = 0;` |
|    11860 | 1098 | `	iNest = 0;` |
|    11860 | 1099 | `	sDoc.nByte = 0;` |
|    11860 | 1100 | `	sDoc.zString = ""; /* cc warning */` |
|    11860 | 1101 | `	for(;;){` |
|    23722 | 1102 | `		if( zIn >= zEnd ){` |
|        - | 1103 | `			/* End of input reached */` |
|    11856 | 1104 | `			break;` |
|        - | 1105 | `		}` |
|    11868 | 1106 | `		sToken.nLine = nLine;` |
|    11868 | 1107 | `		zCur = zIn;` |
|    11868 | 1108 | `		zCurEnd = 0;` |
|    11876 | 1109 | `		while( zIn < zEnd ){` |
|    11872 | 1110 | `			 if( zIn[0] == '<' ){` |
|    11864 | 1111 | `				const char *zTmp = zIn; /* End of raw input marker */` |
|    11864 | 1112 | `				zIn++;` |
|    11864 | 1113 | `				if( zIn < zEnd ){` |
|    11864 | 1114 | `					if( zIn[0] == '?' ){` |
|    11864 | 1115 | `						zIn++;` |
|    11864 | 1116 | `						if( (sxu32)(zEnd - zIn) >= sizeof("php")-1 &&  SyStrnicmp(zIn,"php",sizeof("php")-1) == 0 ){` |
|        - | 1117 | `							/* opening tag: <?php */` |
|    11862 | 1118 | `							zIn += sizeof("php")-1;` |
|     5930 | 1119 | `						}` |
|        - | 1120 | `						/* Look for the closing tag '?>' */` |
|    11864 | 1121 | `						SyStringInitFromBuf(&sCtag,"?>",sizeof("?>")-1);` |
|    11864 | 1122 | `						zCurEnd = zTmp;` |
|    11864 | 1123 | `						break;` |
|        - | 1124 | `					}` |
|      ! 0 | 1125 | `				}` |
|      ! 0 | 1126 | `			}else{` |
|       10 | 1127 | `				if( zIn[0] == '\n' ){` |
|       10 | 1128 | `					nLine++;` |
|        4 | 1129 | `				}` |
|       10 | 1130 | `				zIn++;` |
|        - | 1131 | `			 }` |
|        2 | 1132 | `		} /* While(zIn < zEnd) */` |
|    11868 | 1133 | `		if( zCurEnd == 0 ){` |
|        5 | 1134 | `			zCurEnd = zIn;` |
|        2 | 1135 | `		}` |
|        - | 1136 | `		/* Save the raw token */` |
|    11868 | 1137 | `		SyStringInitFromBuf(&sToken.sData,zCur,zCurEnd - zCur);` |
|    11868 | 1138 | `		sToken.nType = PH7_TOKEN_RAW;` |
|    11868 | 1139 | `		rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|    11868 | 1140 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1141 | `			return rc;` |
|        - | 1142 | `		}` |
|    11868 | 1143 | `		if( zIn >= zEnd ){` |
|        5 | 1144 | `			break;` |
|        - | 1145 | `		}` |
|        - | 1146 | `		/* Ignore leading white space */` |
|    25616 | 1147 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    13754 | 1148 | `			if( zIn[0] == '\n' ){` |
|    12562 | 1149 | `				nLine++;` |
|     6280 | 1150 | `			}` |
|    13754 | 1151 | `			zIn++;` |
|        2 | 1152 | `		}` |
|        - | 1153 | `		/* Delimit the PHP chunk */` |
|    11864 | 1154 | `		sToken.nLine = nLine;` |
|    11864 | 1155 | `		zCur = zIn;` |
|  1102952 | 1156 | `		while( (sxu32)(zEnd - zIn) >= sCtag.nByte ){` |
|        - | 1157 | `			const char *zPtr;` |
|  1097736 | 1158 | `			if( SyMemcmp(zIn,sCtag.zString,sCtag.nByte) == 0 && iNest < 1 ){` |
|     6648 | 1159 | `				break;` |
|        - | 1160 | `			}` |
|   547543 | 1161 | `			for(;;){` |
|  1095088 | 1162 | `				if( zIn[0] != '/' \|\| (zIn[1] != '*' && zIn[1] != '/') /* && sCtag.nByte >= 2 */ ){` |
|   545546 | 1163 | `					break;` |
|        - | 1164 | `				}` |
|     4000 | 1165 | `				zIn += 2;` |
|     4000 | 1166 | `				if( zIn[-1] == '/' ){` |
|        - | 1167 | `					/* Inline comment */` |
|   139568 | 1168 | `					while( zIn < zEnd && zIn[0] != '\n' ){` |
|   135654 | 1169 | `						zIn++;` |
|        2 | 1170 | `					}` |
|     3916 | 1171 | `					if( zIn >= zEnd ){` |
|      ! 0 | 1172 | `						zIn--;` |
|      ! 0 | 1173 | `					}` |
|     1959 | 1174 | `				}else{` |
|        - | 1175 | `					/* Block comment */` |
|     4530 | 1176 | `					while( (sxu32)(zEnd-zIn) >= sizeof("*/") - 1 ){` |
|     4530 | 1177 | `						if( zIn[0] == '*' && zIn[1] == '/' ){` |
|       86 | 1178 | `							zIn += 2;` |
|       86 | 1179 | `							break;` |
|        - | 1180 | `						}` |
|     4446 | 1181 | `						if( zIn[0] == '\n' ){` |
|       28 | 1182 | `							nLine++;` |
|       13 | 1183 | `						}` |
|     4446 | 1184 | `						zIn++;` |
|        2 | 1185 | `					}` |
|        - | 1186 | `				}` |
|        2 | 1187 | `			}` |
|  1091090 | 1188 | `			if( zIn[0] == '\n' ){` |
|    38486 | 1189 | `				nLine++;` |
|    38486 | 1190 | `				if( iNest > 0 ){` |
|      282 | 1191 | `					zIn++;` |
|      666 | 1192 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|      386 | 1193 | `						zIn++;` |
|        2 | 1194 | `					}` |
|      282 | 1195 | `					zPtr = zIn;` |
|     1440 | 1196 | `					while( zIn < zEnd ){` |
|     1440 | 1197 | `						if( (unsigned char)zIn[0] >= 0xc0 ){` |
|        - | 1198 | `							/* UTF-8 stream */` |
|       19 | 1199 | `							zIn++;` |
|       37 | 1200 | `							SX_JMP_UTF8(zIn,zEnd);` |
|     1430 | 1201 | `						}else if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|      142 | 1202 | `							break;` |
|      ! 0 | 1203 | `						}else{` |
|     1142 | 1204 | `							zIn++;` |
|        - | 1205 | `						}` |
|        2 | 1206 | `					}` |
|      282 | 1207 | `					if( (sxu32)(zIn - zPtr) == sDoc.nByte && SyMemcmp(sDoc.zString,zPtr,sDoc.nByte) == 0 ){` |
|      110 | 1208 | `						iNest = 0;` |
|       54 | 1209 | `					}` |
|      282 | 1210 | `					continue;` |
|        2 | 1211 | `				}` |
|  1071708 | 1212 | `			}else if ( (sxu32)(zEnd - zIn) >= sizeof("<<<") && zIn[0] == '<' && zIn[1] == '<' && zIn[2] == '<' && iNest < 1){` |
|      112 | 1213 | `				zIn += sizeof("<<<")-1;` |
|      124 | 1214 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|       13 | 1215 | `					zIn++;` |
|        1 | 1216 | `				}` |
|      112 | 1217 | `				if( zIn[0] == '"' \|\| zIn[0] == '\'' ){` |
|       44 | 1218 | `					zIn++;` |
|       21 | 1219 | `				}` |
|      112 | 1220 | `				zPtr = zIn;` |
|      530 | 1221 | `				while( zIn < zEnd ){` |
|      530 | 1222 | `					if( (unsigned char)zIn[0] >= 0xc0 ){` |
|        - | 1223 | `						/* UTF-8 stream */` |
|       19 | 1224 | `						zIn++;` |
|       37 | 1225 | `						SX_JMP_UTF8(zIn,zEnd);` |
|      520 | 1226 | `					}else if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       57 | 1227 | `						break;` |
|      ! 0 | 1228 | `					}else{` |
|      402 | 1229 | `						zIn++;` |
|        - | 1230 | `					}` |
|        2 | 1231 | `				}` |
|      112 | 1232 | `				SyStringInitFromBuf(&sDoc,zPtr,zIn-zPtr);` |
|      112 | 1233 | `				SyStringFullTrim(&sDoc);` |
|      112 | 1234 | `				if( sDoc.nByte > 0 ){` |
|      112 | 1235 | `					iNest++;` |
|       55 | 1236 | `				}` |
|      112 | 1237 | `				continue;` |
|        - | 1238 | `			}` |
|  1090700 | 1239 | `			zIn++;` |
|        - | 1240 |  |
|  1090700 | 1241 | `			if ( zIn >= zEnd )` |
|      ! 0 | 1242 | `				break;` |
|        2 | 1243 | `		}` |
|    11864 | 1244 | `		if( (sxu32)(zEnd - zIn) < sCtag.nByte ){` |
|     5218 | 1245 | `			zIn = zEnd;` |
|     2608 | 1246 | `		}` |
|    11864 | 1247 | `		if( zCur < zIn ){` |
|        - | 1248 | `			/* Save the PHP chunk for later processing */` |
|     9520 | 1249 | `			sToken.nType = PH7_TOKEN_PHP;` |
|     9520 | 1250 | `			SyStringInitFromBuf(&sToken.sData,zCur,zIn-zCur);` |
|    18972 | 1251 | `			SyStringRightTrim(&sToken.sData); /* Trim trailing white spaces */` |
|     9520 | 1252 | `			rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|     9520 | 1253 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1254 | `				return rc;` |
|        - | 1255 | `			}` |
|     4759 | 1256 | `		}` |
|    11864 | 1257 | `		if( zIn < zEnd ){` |
|        - | 1258 | `			/* Jump the trailing closing tag */` |
|     6648 | 1259 | `			zIn += sCtag.nByte;` |
|     3323 | 1260 | `		}` |
|        2 | 1261 | `	} /* For(;;) */` |
|        - | 1262 |  |
|    11860 | 1263 | ` 	return SXRET_OK;` |
|     5931 | 1264 |  |
|        - | 1265 |  |
