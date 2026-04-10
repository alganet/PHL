# src/ph7/lex.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 698/733 lines (95.23%)

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
|  6884806 |   19 | `static sxi32 TokenizePHP(SyStream *pStream,SyToken *pToken,void *pUserData,void *pCtxData)` |
|        2 |   20 |  |
|        - |   21 | `	SyString *pStr;` |
|        - |   22 | `	sxi32 rc;` |
|        - |   23 | `	/* Ignore leading white spaces */` |
| 10371662 |   24 | `	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){` |
|        - |   25 | `		/* Advance the stream cursor */` |
|  3486856 |   26 | `		if( pStream->zText[0] == '\n' ){` |
|        - |   27 | `			/* Update line counter */` |
|    29574 |   28 | `			pStream->nLine++;` |
|    14786 |   29 | `		}` |
|  3486856 |   30 | `		pStream->zText++;` |
|        2 |   31 | `	}` |
|  6884808 |   32 | `	if( pStream->zText >= pStream->zEnd ){` |
|        - |   33 | `		/* End of input reached */` |
|      ! 0 |   34 | `		return SXERR_EOF;` |
|        - |   35 | `	}` |
|        - |   36 | `	/* Record token starting position and line */` |
|  6884808 |   37 | `	pToken->nLine = pStream->nLine;` |
|  6884808 |   38 | `	pToken->pUserData = 0;` |
|  6884808 |   39 | `	pStr = &pToken->sData;` |
|  6884808 |   40 | `	SyStringInitFromBuf(pStr,pStream->zText,0);` |
|  8128496 |   41 | `	if( pStream->zText[0] >= 0xc0 \|\| SyisAlpha(pStream->zText[0]) \|\| pStream->zText[0] == '_' ){` |
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
|  2487378 |   52 | `		if( pStream->zText[0] < 0xc0 ){` |
|  2487362 |   53 | `			pStream->zText++;` |
|  1243680 |   54 | `		}` |
|  2442456 |   55 | `		for(;;){` |
|  4884914 |   56 | `			zIn = pStream->zText;` |
|  4884914 |   57 | `			if( zIn[0] >= 0xc0 ){` |
|       49 |   58 | `				zIn++;` |
|        - |   59 | `				/* UTF-8 stream */` |
|      109 |   60 | `				while( zIn < pStream->zEnd && ((zIn[0] & 0xc0) == 0x80) ){` |
|       61 |   61 | `					zIn++;` |
|        1 |   62 | `				}` |
|       24 |   63 | `			}` |
|        - |   64 | `			/* Skip alphanumeric stream */` |
| 20049590 |   65 | `			while( zIn < pStream->zEnd && zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_') ){` |
| 12722222 |   66 | `				zIn++;` |
|        2 |   67 | `			}` |
|  4884914 |   68 | `			if( zIn == pStream->zText ){` |
|        - |   69 | `				/* Not an UTF-8 or alphanumeric stream */` |
|  2487378 |   70 | `				break;` |
|        - |   71 | `			}` |
|        - |   72 | `			/* Synchronize pointers */` |
|  2397538 |   73 | `			pStream->zText = zIn;` |
|        2 |   74 | `		}` |
|        - |   75 | `		/* Record token length */` |
|  2487378 |   76 | `		pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  2487378 |   77 | `		nKeyword = KeywordCode(pStr->zString,(int)pStr->nByte);` |
|  2487378 |   78 | `		if( nKeyword != PH7_TK_ID ){` |
|   850100 |   79 | `			if( nKeyword &` |
|        - |   80 | `				(PH7_TKWRD_NEW\|PH7_TKWRD_CLONE\|PH7_TKWRD_AND\|PH7_TKWRD_XOR\|PH7_TKWRD_OR\|PH7_TKWRD_INSTANCEOF\|PH7_TKWRD_SEQ\|PH7_TKWRD_SNE) ){` |
|        - |   81 | `					/* Alpha stream operators [i.e: new,clone,and,instanceof,eq,ne,or,xor],save the operator instance for later processing */` |
|    13936 |   82 | `					pToken->pUserData = (void *)PH7_ExprExtractOperator(pStr,0);` |
|        - |   83 | `					/* Mark as an operator */` |
|    13936 |   84 | `					pToken->nType = PH7_TK_ID\|PH7_TK_OP;` |
|     6969 |   85 | `			}else{` |
|        - |   86 | `				/* We are dealing with a keyword [i.e: while,foreach,class...],save the keyword ID */` |
|   836166 |   87 | `				pToken->nType = PH7_TK_KEYWORD;` |
|   836166 |   88 | `				pToken->pUserData = SX_INT_TO_PTR(nKeyword);` |
|        - |   89 | `			}` |
|   425051 |   90 | `		}else{` |
|        - |   91 | `			/* A simple identifier */` |
|  1637280 |   92 | `			pToken->nType = PH7_TK_ID;` |
|        - |   93 | `		}` |
|  1243690 |   94 | `	}else{` |
|        - |   95 | `		sxi32 c;` |
|        - |   96 | `		/* Non-alpha stream */` |
|  4431249 |   97 | `		if( pStream->zText[0] == '#' \|\|` |
|  4397430 |   98 | `			( pStream->zText[0] == '/' &&  &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '/') ){` |
|     3746 |   99 | `				pStream->zText++;` |
|        - |  100 | `				/* Inline comments */` |
|   135500 |  101 | `				while( pStream->zText < pStream->zEnd && pStream->zText[0] != '\n' ){` |
|   131756 |  102 | `					pStream->zText++;` |
|        2 |  103 | `				}` |
|        - |  104 | `				/* Tell the upper-layer to ignore this token */` |
|     3746 |  105 | `				return SXERR_CONTINUE;` |
|  4393688 |  106 | `		}else if( pStream->zText[0] == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '*' ){` |
|    63828 |  107 | `			pStream->zText += 2;` |
|        - |  108 | `			/* Block comment */` |
|  1810580 |  109 | `			while( pStream->zText < pStream->zEnd ){` |
|  1810580 |  110 | `				if( pStream->zText[0] == '*' ){` |
|    63854 |  111 | `					if( &pStream->zText[1] >= pStream->zEnd \|\| pStream->zText[1] == '/'  ){` |
|    31915 |  112 | `						break;` |
|        - |  113 | `					}` |
|       13 |  114 | `				}` |
|  1746754 |  115 | `				if( pStream->zText[0] == '\n' ){` |
|       28 |  116 | `					pStream->nLine++;` |
|       13 |  117 | `				}` |
|  1746754 |  118 | `				pStream->zText++;` |
|        2 |  119 | `			}` |
|    63828 |  120 | `			pStream->zText += 2;` |
|        - |  121 | `			/* Tell the upper-layer to ignore this token */` |
|    63828 |  122 | `			return SXERR_CONTINUE;` |
|  4329862 |  123 | `		}else if( SyisDigit(pStream->zText[0]) ){` |
|    88262 |  124 | `			pStream->zText++;` |
|        - |  125 | `			/* PHP 7.4: handle underscore separator immediately following the first digit.` |
|        - |  126 | `			 * Check pStream->zText < pStream->zEnd BEFORE forming pStream->zText + 1 so` |
|        - |  127 | `			 * we never compute a pointer past one-past-end. */` |
|    88340 |  128 | `			if( pStream->zText < pStream->zEnd` |
|    88260 |  129 | `				&& pStream->zText[0] == '_'` |
|    44210 |  130 | `				&& pStream->zText + 1 < pStream->zEnd` |
|      160 |  131 | `				&& pStream->zText[1] < 0xc0` |
|      162 |  132 | `				&& SyisDigit(pStream->zText[1]) ){` |
|      151 |  133 | `				pStream->zText++; /* swallow underscore between two digits */` |
|       75 |  134 | `			}` |
|        - |  135 | `			/* Decimal digit stream (PHP 7.4: underscore separator allowed between two digits) */` |
|    97570 |  136 | `			while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|     9310 |  137 | `				pStream->zText++;` |
|     9394 |  138 | `				if( pStream->zText < pStream->zEnd` |
|     9308 |  139 | `					&& pStream->zText[0] == '_'` |
|     4740 |  140 | `					&& pStream->zText + 1 < pStream->zEnd` |
|      172 |  141 | `					&& pStream->zText[1] < 0xc0` |
|      174 |  142 | `					&& SyisDigit(pStream->zText[1]) ){` |
|      173 |  143 | `					pStream->zText++; /* swallow underscore between two digits */` |
|       86 |  144 | `				}` |
|        2 |  145 | `			}` |
|        - |  146 | `			/* Mark the token as integer until we encounter a real number */` |
|    88262 |  147 | `			pToken->nType = PH7_TK_INTEGER;` |
|    88262 |  148 | `			if( pStream->zText < pStream->zEnd ){` |
|    88262 |  149 | `				c = pStream->zText[0];` |
|    88262 |  150 | `				if( c == '.' ){` |
|        - |  151 | `					/* Real number (PHP 7.4: underscore separator allowed between two digits) */` |
|      448 |  152 | `					pStream->zText++;` |
|     1758 |  153 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|     1312 |  154 | `						pStream->zText++;` |
|     1316 |  155 | `						if( pStream->zText < pStream->zEnd` |
|     1310 |  156 | `							&& pStream->zText[0] == '_'` |
|      661 |  157 | `							&& pStream->zText + 1 < pStream->zEnd` |
|       12 |  158 | `							&& pStream->zText[1] < 0xc0` |
|       14 |  159 | `							&& SyisDigit(pStream->zText[1]) ){` |
|       13 |  160 | `							pStream->zText++;` |
|        6 |  161 | `						}` |
|        2 |  162 | `					}` |
|      448 |  163 | `					if( pStream->zText < pStream->zEnd ){` |
|      448 |  164 | `						c = pStream->zText[0];` |
|      448 |  165 | `						if( c=='e' \|\| c=='E' ){` |
|       29 |  166 | `							pStream->zText++;` |
|       29 |  167 | `							if( pStream->zText < pStream->zEnd ){` |
|       29 |  168 | `								c = pStream->zText[0];` |
|       35 |  169 | `								if( (c =='+' \|\| c=='-') && &pStream->zText[1] < pStream->zEnd  &&` |
|       15 |  170 | `									pStream->zText[1] < 0xc0 && SyisDigit(pStream->zText[1]) ){` |
|       15 |  171 | `										pStream->zText++;` |
|        7 |  172 | `								}` |
|       69 |  173 | `								while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|       41 |  174 | `									pStream->zText++;` |
|       44 |  175 | `									if( pStream->zText < pStream->zEnd` |
|       40 |  176 | `										&& pStream->zText[0] == '_'` |
|       24 |  177 | `										&& pStream->zText + 1 < pStream->zEnd` |
|        8 |  178 | `										&& pStream->zText[1] < 0xc0` |
|        9 |  179 | `										&& SyisDigit(pStream->zText[1]) ){` |
|        9 |  180 | `										pStream->zText++;` |
|        4 |  181 | `									}` |
|        1 |  182 | `								}` |
|       14 |  183 | `							}` |
|       14 |  184 | `						}` |
|      223 |  185 | `					}` |
|      448 |  186 | `					pToken->nType = PH7_TK_REAL;` |
|    88039 |  187 | `				}else if( c=='e' \|\| c=='E' ){` |
|       14 |  188 | `					SXUNUSED(pUserData); /* Prevent compiler warning */` |
|       14 |  189 | `					SXUNUSED(pCtxData);` |
|       29 |  190 | `					pStream->zText++;` |
|       29 |  191 | `					if( pStream->zText < pStream->zEnd ){` |
|       29 |  192 | `						c = pStream->zText[0];` |
|       31 |  193 | `						if( (c =='+' \|\| c=='-') && &pStream->zText[1] < pStream->zEnd  &&` |
|        7 |  194 | `							pStream->zText[1] < 0xc0 && SyisDigit(pStream->zText[1]) ){` |
|        7 |  195 | `								pStream->zText++;` |
|        3 |  196 | `						}` |
|       67 |  197 | `						while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|       39 |  198 | `							pStream->zText++;` |
|       40 |  199 | `							if( pStream->zText < pStream->zEnd` |
|       38 |  200 | `								&& pStream->zText[0] == '_'` |
|       21 |  201 | `								&& pStream->zText + 1 < pStream->zEnd` |
|        4 |  202 | `								&& pStream->zText[1] < 0xc0` |
|        5 |  203 | `								&& SyisDigit(pStream->zText[1]) ){` |
|        5 |  204 | `								pStream->zText++;` |
|        2 |  205 | `							}` |
|        1 |  206 | `						}` |
|       14 |  207 | `					}` |
|       29 |  208 | `					pToken->nType = PH7_TK_REAL;` |
|    87802 |  209 | `				}else if( c == 'x' \|\| c == 'X' ){` |
|        - |  210 | `					/* Hex digit stream (PHP 7.4: underscore separator allowed between two digits) */` |
|       74 |  211 | `					pStream->zText++;` |
|      370 |  212 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisHex(pStream->zText[0]) ){` |
|      297 |  213 | `						pStream->zText++;` |
|      320 |  214 | `						if( pStream->zText < pStream->zEnd` |
|      296 |  215 | `							&& pStream->zText[0] == '_'` |
|      172 |  216 | `							&& pStream->zText + 1 < pStream->zEnd` |
|       48 |  217 | `							&& pStream->zText[1] < 0xc0` |
|       49 |  218 | `							&& SyisHex(pStream->zText[1]) ){` |
|       49 |  219 | `							pStream->zText++;` |
|       24 |  220 | `						}` |
|        1 |  221 | `					}` |
|    87752 |  222 | `				}else if(c  == 'b' \|\| c == 'B' ){` |
|        - |  223 | `					/* Binary digit stream (PHP 7.4: underscore separator allowed between two digits) */` |
|      280 |  224 | `					pStream->zText++;` |
|     2702 |  225 | `					while( pStream->zText < pStream->zEnd && (pStream->zText[0] == '0' \|\| pStream->zText[0] == '1') ){` |
|     1523 |  226 | `						pStream->zText++;` |
|     1583 |  227 | `						if( pStream->zText < pStream->zEnd` |
|     1522 |  228 | `							&& pStream->zText[0] == '_'` |
|      830 |  229 | `							&& pStream->zText + 1 < pStream->zEnd` |
|      139 |  230 | `							&& (pStream->zText[1] == '0' \|\| pStream->zText[1] == '1') ){` |
|      139 |  231 | `							pStream->zText++;` |
|       69 |  232 | `						}` |
|        1 |  233 | `					}` |
|      139 |  234 | `				}` |
|    44130 |  235 | `			}` |
|        - |  236 | `			/* PHP 7.4: absorb a trailing malformed underscore run into the` |
|        - |  237 | `			 * numeric token so the compile phase can emit a PHP-compatible` |
|        - |  238 | `			 * "syntax error, unexpected identifier" parse error. Valid` |
|        - |  239 | `			 * separators were already consumed by the per-loop peek logic` |
|        - |  240 | `			 * above, so an underscore here is always misplaced. */` |
|    88262 |  241 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '_' ){` |
|       15 |  242 | `				pStream->zText++;` |
|       44 |  243 | `				while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0` |
|       46 |  244 | `					&& (SyisAlphaNum(pStream->zText[0]) \|\| pStream->zText[0] == '_') ){` |
|       17 |  245 | `					pStream->zText++;` |
|        1 |  246 | `				}` |
|        7 |  247 | `			}` |
|        - |  248 | `			/* Record token length */` |
|    88262 |  249 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|    88262 |  250 | `			return SXRET_OK;` |
|        - |  251 | `		}` |
|  4241602 |  252 | `		c = pStream->zText[0];` |
|  4241602 |  253 | `		pStream->zText++; /* Advance the stream cursor */` |
|        - |  254 | `		/* Assume we are dealing with an operator*/` |
|  4241602 |  255 | `		pToken->nType = PH7_TK_OP;` |
|  4241602 |  256 | `		switch(c){` |
|   889410 |  257 | `		case '$': pToken->nType = PH7_TK_DOLLAR; break;` |
|   337142 |  258 | `		case '{': pToken->nType = PH7_TK_OCB;    break;` |
|   337128 |  259 | `		case '}': pToken->nType = PH7_TK_CCB;    break;` |
|   671082 |  260 | `		case '(': pToken->nType = PH7_TK_LPAREN; break;` |
|    68462 |  261 | `		case '[': pToken->nType \|= PH7_TK_OSB;   break; /* Bitwise operation here,since the square bracket token '['` |
|        - |  262 | `														 * is a potential operator [i.e: subscripting] */` |
|    68468 |  263 | `		case ']': pToken->nType = PH7_TK_CSB;    break;` |
|   335534 |  264 | `		case ')': {` |
|   671070 |  265 | `			SySet *pTokSet = pStream->pSet;` |
|        - |  266 | `			/* Assemble type cast operators [i.e: (int),(float),(bool)...] */` |
|   671070 |  267 | `			if( pTokSet->nUsed >= 2 ){` |
|        - |  268 | `				SyToken *pTmp;` |
|        - |  269 | `				/* Peek the last recongnized token */` |
|   671068 |  270 | `				pTmp = (SyToken *)SySetPeek(pTokSet);` |
|   671068 |  271 | `				if( pTmp->nType & PH7_TK_KEYWORD ){` |
|    13682 |  272 | `					sxi32 nID = SX_PTR_TO_INT(pTmp->pUserData);` |
|    13682 |  273 | `					if( (sxu32)nID & (PH7_TKWRD_ARRAY\|PH7_TKWRD_INT\|PH7_TKWRD_FLOAT\|PH7_TKWRD_STRING\|PH7_TKWRD_OBJECT\|PH7_TKWRD_BOOL\|PH7_TKWRD_UNSET) ){` |
|    13582 |  274 | `						pTmp = (SyToken *)SySetAt(pTokSet,pTokSet->nUsed - 2);` |
|    13582 |  275 | `						if( pTmp->nType & PH7_TK_LPAREN ){` |
|        - |  276 | `							/* Merge the three tokens '(' 'TYPE' ')' into a single one */` |
|    13498 |  277 | `							const char * zTypeCast = "(int)";` |
|    13498 |  278 | `							if( nID & PH7_TKWRD_FLOAT ){` |
|     2664 |  279 | `								zTypeCast = "(float)";` |
|    12167 |  280 | `							}else if( nID & PH7_TKWRD_BOOL ){` |
|     2666 |  281 | `								zTypeCast = "(bool)";` |
|     9504 |  282 | `							}else if( nID & PH7_TKWRD_STRING ){` |
|     5322 |  283 | `								zTypeCast = "(string)";` |
|     5512 |  284 | `							}else if( nID & PH7_TKWRD_ARRAY ){` |
|       21 |  285 | `								zTypeCast = "(array)";` |
|     2842 |  286 | `							}else if( nID & PH7_TKWRD_OBJECT ){` |
|       17 |  287 | `								zTypeCast = "(object)";` |
|     2824 |  288 | `							}else if( nID & PH7_TKWRD_UNSET ){` |
|        7 |  289 | `								zTypeCast = "(unset)";` |
|        3 |  290 | `							}` |
|        - |  291 | `							/* Reflect the change */` |
|    13498 |  292 | `							pToken->nType = PH7_TK_OP;` |
|    13498 |  293 | `							SyStringInitFromBuf(&pToken->sData,zTypeCast,SyStrlen(zTypeCast));` |
|        - |  294 | `							/* Save the instance associated with the type cast operator */` |
|    13498 |  295 | `							pToken->pUserData = (void *)PH7_ExprExtractOperator(&pToken->sData,0);` |
|        - |  296 | `							/* Remove the two previous tokens */` |
|    13498 |  297 | `							pTokSet->nUsed -= 2;` |
|    13498 |  298 | `							return SXRET_OK;` |
|        - |  299 | `						}` |
|       42 |  300 | `					}` |
|       92 |  301 | `				}` |
|   328785 |  302 | `			}` |
|   657574 |  303 | `			pToken->nType = PH7_TK_RPAREN;` |
|   657574 |  304 | `			break;` |
|        - |  305 | `				  }` |
|    28634 |  306 | `		case '\'':{` |
|        - |  307 | `			/* Single quoted string */` |
|    57270 |  308 | `			pStr->zString++;` |
|   721726 |  309 | `			while( pStream->zText < pStream->zEnd ){` |
|   721726 |  310 | `				if( pStream->zText[0] == '\''  ){` |
|    57280 |  311 | `					if( pStream->zText[-1] != '\\' ){` |
|    57256 |  312 | `						break;` |
|      ! 0 |  313 | `					}else{` |
|       25 |  314 | `						const unsigned char *zPtr = &pStream->zText[-2];` |
|       25 |  315 | `						sxi32 i = 1;` |
|       43 |  316 | `						while( zPtr > pStream->zInput && zPtr[0] == '\\' ){` |
|       19 |  317 | `							zPtr--;` |
|       19 |  318 | `							i++;` |
|        1 |  319 | `						}` |
|       25 |  320 | `						if((i&1)==0){` |
|       15 |  321 | `							break;` |
|        - |  322 | `						}` |
|        - |  323 | `					}` |
|        5 |  324 | `				}` |
|   664458 |  325 | `				if( pStream->zText[0] == '\n' ){` |
|       67 |  326 | `					pStream->nLine++;` |
|       33 |  327 | `				}` |
|   664458 |  328 | `				pStream->zText++;` |
|        2 |  329 | `			}` |
|        - |  330 | `			/* Record token length and type */` |
|    57270 |  331 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|    57270 |  332 | `			pToken->nType = PH7_TK_SSTR;` |
|        - |  333 | `			/* Jump the trailing single quote */` |
|    57270 |  334 | `			pStream->zText++;` |
|    57270 |  335 | `			return SXRET_OK;` |
|        - |  336 | `				  }` |
|     7823 |  337 | `		case '"':{` |
|        - |  338 | `			sxi32 iNest;` |
|        - |  339 | `			/* Double quoted string */` |
|    15648 |  340 | `			pStr->zString++;` |
|   155348 |  341 | `			while( pStream->zText < pStream->zEnd ){` |
|   155348 |  342 | `				if( pStream->zText[0] == '{' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '$'){` |
|       71 |  343 | `					iNest = 1;` |
|       71 |  344 | `					pStream->zText++;` |
|        - |  345 | `					/* TICKET 1433-40: Hnadle braces'{}' in double quoted string where everything is allowed */` |
|      823 |  346 | `					while(pStream->zText < pStream->zEnd ){` |
|      823 |  347 | `						if( pStream->zText[0] == '{' ){` |
|        9 |  348 | `							iNest++;` |
|      819 |  349 | `						}else if (pStream->zText[0] == '}' ){` |
|       79 |  350 | `							iNest--;` |
|       79 |  351 | `							if( iNest <= 0 ){` |
|       71 |  352 | `								pStream->zText++;` |
|       71 |  353 | `								break;` |
|        1 |  354 | `							}` |
|      741 |  355 | `						}else if( pStream->zText[0] == '\n' ){` |
|      ! 0 |  356 | `							pStream->nLine++;` |
|      ! 0 |  357 | `						}` |
|      753 |  358 | `						pStream->zText++;` |
|        1 |  359 | `					}` |
|       71 |  360 | `					if( pStream->zText >= pStream->zEnd ){` |
|      ! 0 |  361 | `						break;` |
|        - |  362 | `					}` |
|       35 |  363 | `				}` |
|   155348 |  364 | `				if( pStream->zText[0] == '"' ){` |
|    15748 |  365 | `					if( pStream->zText[-1] != '\\' ){` |
|    15644 |  366 | `						break;` |
|      ! 0 |  367 | `					}else{` |
|      106 |  368 | `						const unsigned char *zPtr = &pStream->zText[-2];` |
|      106 |  369 | `						sxi32 i = 1;` |
|      158 |  370 | `						while( zPtr > pStream->zInput && zPtr[0] == '\\' ){` |
|       54 |  371 | `							zPtr--;` |
|       54 |  372 | `							i++;` |
|        2 |  373 | `						}` |
|      106 |  374 | `						if((i&1)==0){` |
|        5 |  375 | `							break;` |
|        - |  376 | `						}` |
|        - |  377 | `					}` |
|       50 |  378 | `				}` |
|   139702 |  379 | `				if( pStream->zText[0] == '\n' ){` |
|        7 |  380 | `					pStream->nLine++;` |
|        3 |  381 | `				}` |
|   139702 |  382 | `				pStream->zText++;` |
|        2 |  383 | `			}` |
|        - |  384 | `			/* Record token length and type */` |
|    15648 |  385 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|    15648 |  386 | `			pToken->nType = PH7_TK_DSTR;` |
|        - |  387 | `			/* Jump the trailing quote */` |
|    15648 |  388 | `			pStream->zText++;` |
|    15648 |  389 | `			return SXRET_OK;` |
|        - |  390 | `				  }` |
|        2 |  391 | ``		case '`':{`` |
|        - |  392 | `			/* Backtick quoted string */` |
|        5 |  393 | `			pStr->zString++;` |
|       45 |  394 | `			while( pStream->zText < pStream->zEnd ){` |
|       45 |  395 | ``				if( pStream->zText[0] == '`' && pStream->zText[-1] != '\\' ){`` |
|        5 |  396 | `					break;` |
|        - |  397 | `				}` |
|       41 |  398 | `				if( pStream->zText[0] == '\n' ){` |
|      ! 0 |  399 | `					pStream->nLine++;` |
|      ! 0 |  400 | `				}` |
|       41 |  401 | `				pStream->zText++;` |
|        1 |  402 | `			}` |
|        - |  403 | `			/* Record token length and type */` |
|        5 |  404 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|        5 |  405 | `			pToken->nType = PH7_TK_BSTR;` |
|        - |  406 | `			/* Jump the trailing backtick */` |
|        5 |  407 | `			pStream->zText++;` |
|        5 |  408 | `			return SXRET_OK;` |
|        - |  409 | `				  }` |
|      166 |  410 | `		case '\\': pToken->nType = PH7_TK_NSSEP;  break;` |
|     1196 |  411 | `		case ':':` |
|     2394 |  412 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == ':' ){` |
|        - |  413 | `				/* Current operator: '::' */` |
|      210 |  414 | `				pStream->zText++;` |
|      106 |  415 | `			}else{` |
|     2186 |  416 | `				pToken->nType = PH7_TK_COLON; /* Single colon */` |
|        - |  417 | `			}` |
|     2394 |  418 | `			break;` |
|    71982 |  419 | `		case ',': pToken->nType \|= PH7_TK_COMMA;  break; /* Comma is also an operator */` |
|   482502 |  420 | `		case ';': pToken->nType = PH7_TK_SEMI;    break;` |
|        - |  421 | `			/* Handle combined operators [i.e: +=,===,!=== ...] */` |
|   139400 |  422 | `		case '=':` |
|   278802 |  423 | `			pToken->nType \|= PH7_TK_EQUAL;` |
|   278802 |  424 | `			if( pStream->zText < pStream->zEnd ){` |
|   278802 |  425 | `				if( pStream->zText[0] == '=' ){` |
|    17442 |  426 | `					pToken->nType &= ~PH7_TK_EQUAL;` |
|        - |  427 | `					/* Current operator: == */` |
|    17442 |  428 | `					pStream->zText++;` |
|    17442 |  429 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  430 | `						/* Current operator: === */` |
|     3850 |  431 | `						pStream->zText++;` |
|     1926 |  432 | `					}` |
|   270082 |  433 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  434 | `					/* Array operator: => */` |
|     4024 |  435 | `					pToken->nType = PH7_TK_ARRAY_OP;` |
|     4024 |  436 | `					pStream->zText++;` |
|     2013 |  437 | `				}else{` |
|        - |  438 | `					/* TICKET 1433-0010: Reference operator '=&' */` |
|   257340 |  439 | `					const unsigned char *zCur = pStream->zText;` |
|   257340 |  440 | `					sxu32 nLine = 0;` |
|   514656 |  441 | `					while( zCur < pStream->zEnd && zCur[0] < 0xc0 && SyisSpace(zCur[0]) ){` |
|   257318 |  442 | `						if( zCur[0] == '\n' ){` |
|        5 |  443 | `							nLine++;` |
|        2 |  444 | `						}` |
|   257318 |  445 | `						zCur++;` |
|        2 |  446 | `					}` |
|   257340 |  447 | `					if( zCur < pStream->zEnd && zCur[0] == '&' ){` |
|        - |  448 | `						/* Current operator: =& */` |
|       46 |  449 | `						pToken->nType &= ~PH7_TK_EQUAL;` |
|       46 |  450 | `						SyStringInitFromBuf(pStr,"=&",sizeof("=&")-1);` |
|        - |  451 | `						/* Update token stream */` |
|       46 |  452 | `						pStream->zText = &zCur[1];` |
|       46 |  453 | `						pStream->nLine += nLine;` |
|       22 |  454 | `					}` |
|        - |  455 | `				}` |
|   139400 |  456 | `			}` |
|   278802 |  457 | `			break;` |
|    18942 |  458 | `		case '!':` |
|    37886 |  459 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  460 | `				/* Current operator: != */` |
|    16120 |  461 | `				pStream->zText++;` |
|    16120 |  462 | `				if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  463 | `					/* Current operator: !== */` |
|    13432 |  464 | `					pStream->zText++;` |
|     6715 |  465 | `				}` |
|     8059 |  466 | `			}` |
|    37886 |  467 | `			break;` |
|    10889 |  468 | `		case '&':` |
|    21780 |  469 | `			pToken->nType \|= PH7_TK_AMPER;` |
|    21780 |  470 | `			if( pStream->zText < pStream->zEnd ){` |
|    21780 |  471 | `				if( pStream->zText[0] == '&' ){` |
|     8372 |  472 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|        - |  473 | `					/* Current operator: && */` |
|     8372 |  474 | `					pStream->zText++;` |
|    17595 |  475 | `				}else if( pStream->zText[0] == '=' ){` |
|        7 |  476 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|        - |  477 | `					/* Current operator: &= */` |
|        7 |  478 | `					pStream->zText++;` |
|        3 |  479 | `				}` |
|    10889 |  480 | `			}` |
|    21780 |  481 | `			break;` |
|     1416 |  482 | `		case '\|':` |
|     2834 |  483 | `			if( pStream->zText < pStream->zEnd ){` |
|     2834 |  484 | `				if( pStream->zText[0] == '\|' ){` |
|        - |  485 | `					/* Current operator: \|\| */` |
|     2790 |  486 | `					pStream->zText++;` |
|     1440 |  487 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  488 | `					/* Current operator: \|= */` |
|        7 |  489 | `					pStream->zText++;` |
|        3 |  490 | `				}` |
|     1416 |  491 | `			}` |
|     2834 |  492 | `			break;` |
|     6991 |  493 | `		case '+':` |
|    13984 |  494 | `			if( pStream->zText < pStream->zEnd ){` |
|    13982 |  495 | `				if( pStream->zText[0] == '+' ){` |
|        - |  496 | `					/* Current operator: ++ */` |
|    10868 |  497 | `					pStream->zText++;` |
|     8549 |  498 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  499 | `					/* Current operator: += */` |
|       42 |  500 | `					pStream->zText++;` |
|       20 |  501 | `				}` |
|     6990 |  502 | `			}` |
|    13984 |  503 | `			break;` |
|    51118 |  504 | `		case '-':` |
|   102238 |  505 | `			if( pStream->zText < pStream->zEnd ){` |
|   102238 |  506 | `				if( pStream->zText[0] == '-' ){` |
|        - |  507 | `					/* Current operator: -- */` |
|        5 |  508 | `					pStream->zText++;` |
|   102236 |  509 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  510 | `					/* Current operator: -= */` |
|        7 |  511 | `					pStream->zText++;` |
|   102231 |  512 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  513 | `					/* Current operator: -> */` |
|   101748 |  514 | `					pStream->zText++;` |
|    50873 |  515 | `				}` |
|    51118 |  516 | `			}` |
|   102238 |  517 | `			break;` |
|       80 |  518 | `		case '*':` |
|      162 |  519 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  520 | `				/* Current operator: *= */` |
|       17 |  521 | `				pStream->zText++;` |
|        8 |  522 | `			}` |
|      162 |  523 | `			break;` |
|       32 |  524 | `		case '/':` |
|       66 |  525 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  526 | `				/* Current operator: /= */` |
|        5 |  527 | `				pStream->zText++;` |
|        2 |  528 | `			}` |
|       66 |  529 | `			break;` |
|       25 |  530 | `		case '%':` |
|       52 |  531 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  532 | `				/* Current operator: %= */` |
|        3 |  533 | `				pStream->zText++;` |
|        1 |  534 | `			}` |
|       52 |  535 | `			break;` |
|       11 |  536 | `		case '^':` |
|       23 |  537 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  538 | `				/* Current operator: ^= */` |
|        9 |  539 | `				pStream->zText++;` |
|        4 |  540 | `			}` |
|       23 |  541 | `			break;` |
|    28898 |  542 | `		case '.':` |
|    57798 |  543 | `			if( pStream->zText + 1 < pStream->zEnd && pStream->zText[0] == '.' && pStream->zText[1] == '.' ){` |
|        - |  544 | `				/* Ellipsis: ... */` |
|       42 |  545 | `				pStream->zText += 2;` |
|       42 |  546 | `				pToken->nType = PH7_TK_ELLIPSIS;` |
|    57778 |  547 | `			}else if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  548 | `				/* Current operator: .= */` |
|     2726 |  549 | `				pStream->zText++;` |
|     1362 |  550 | `			}` |
|    57798 |  551 | `			break;` |
|    22808 |  552 | `		case '<':` |
|    45618 |  553 | `			if( pStream->zText < pStream->zEnd ){` |
|    45618 |  554 | `				if( pStream->zText[0] == '<' ){` |
|        - |  555 | `					/* Current operator: << */` |
|       80 |  556 | `					pStream->zText++;` |
|       80 |  557 | `					if( pStream->zText < pStream->zEnd ){` |
|       80 |  558 | `						if( pStream->zText[0] == '=' ){` |
|        - |  559 | `							/* Current operator: <<= */` |
|        9 |  560 | `							pStream->zText++;` |
|       76 |  561 | `						}else if( pStream->zText[0] == '<' ){` |
|        - |  562 | `							/* Current Token: <<<  */` |
|       58 |  563 | `							pStream->zText++;` |
|        - |  564 | `							/* This may be the beginning of a Heredoc/Nowdoc string,try to delimit it */` |
|       58 |  565 | `							rc = LexExtractHeredoc(&(*pStream),&(*pToken));` |
|       58 |  566 | `							if( rc == SXRET_OK ){` |
|        - |  567 | `								/* Here/Now doc successfuly extracted */` |
|       58 |  568 | `								return SXRET_OK;` |
|        - |  569 | `							}` |
|      ! 0 |  570 | `						}` |
|       12 |  571 | `					}` |
|    45551 |  572 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  573 | `					/* Current operator: <> */` |
|        5 |  574 | `					pStream->zText++;` |
|    45538 |  575 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  576 | `					/* Current operator: <= or <=> */` |
|       88 |  577 | `					pStream->zText++;` |
|       88 |  578 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '>' ){` |
|        - |  579 | `						/* Current operator: <=> */` |
|       51 |  580 | `						pStream->zText++;` |
|       25 |  581 | `					}` |
|       43 |  582 | `				}` |
|    22780 |  583 | `			}` |
|    45562 |  584 | `			break;` |
|     2763 |  585 | `		case '>':` |
|     5528 |  586 | `			if( pStream->zText < pStream->zEnd ){` |
|     5528 |  587 | `				if( pStream->zText[0] == '>' ){` |
|        - |  588 | `					/* Current operator: >> */` |
|       21 |  589 | `					pStream->zText++;` |
|       21 |  590 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  591 | `						/* Current operator: >>= */` |
|       11 |  592 | `						pStream->zText++;` |
|        6 |  593 | `					}` |
|     5518 |  594 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  595 | `					/* Current operator: >= */` |
|       80 |  596 | `					pStream->zText++;` |
|       39 |  597 | `				}` |
|     2763 |  598 | `			}` |
|     5528 |  599 | `			break;` |
|      971 |  600 | `		case '?':` |
|     1944 |  601 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '?' ){` |
|        - |  602 | `				/* Null coalescing operator: ?? */` |
|       32 |  603 | `				pStream->zText++;` |
|       15 |  604 | `			}` |
|     1942 |  605 | `			break;` |
|      105 |  606 | `		default:` |
|      210 |  607 | `			break;` |
|        - |  608 | `		}` |
|  4155132 |  609 | `		if( pStr->nByte <= 0 ){` |
|        - |  610 | `			/* Record token length */` |
|  4155088 |  611 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  2077543 |  612 | `		}` |
|  4155132 |  613 | `		if( pToken->nType & PH7_TK_OP ){` |
|        - |  614 | `			const ph7_expr_op *pOp;` |
|        - |  615 | `			/* Check if the extracted token is an operator */` |
|   705430 |  616 | `			pOp = PH7_ExprExtractOperator(pStr,(SyToken *)SySetPeek(pStream->pSet));` |
|   705430 |  617 | `			if( pOp == 0 ){` |
|        - |  618 | `				/* Not an operator */` |
|      ! 0 |  619 | `				pToken->nType &= ~PH7_TK_OP;` |
|      ! 0 |  620 | `				if( pToken->nType <= 0 ){` |
|      ! 0 |  621 | `					pToken->nType = PH7_TK_OTHER;` |
|      ! 0 |  622 | `				}` |
|      ! 0 |  623 | `			}else{` |
|        - |  624 | `				/* Save the instance associated with this operator for later processing */` |
|   705430 |  625 | `				pToken->pUserData = (void *)pOp;` |
|        - |  626 | `			}` |
|   352714 |  627 | `		}` |
|        - |  628 | `	}` |
|        - |  629 | `	/* Tell the upper-layer to save the extracted token for later processing */` |
|  6642508 |  630 | `	return SXRET_OK;` |
|  3442405 |  631 |  |
|        - |  632 | `/***** This file contains automatically generated code ******` |
|        - |  633 | `**` |
|        - |  634 | `** The code in this file has been automatically generated by` |
|        - |  635 | `**` |
|        - |  636 | `**     $Header: /sqlite/sqlite/tool/mkkeywordhash.c` |
|        - |  637 | `**` |
|        - |  638 | `** Sligthly modified by Chems mrad <chm@symisc.net> for the PH7 engine.` |
|        - |  639 | `**` |
|        - |  640 | `** The code in this file implements a function that determines whether` |
|        - |  641 | `** or not a given identifier is really a PHP keyword.  The same thing` |
|        - |  642 | `** might be implemented more directly using a hand-written hash table.` |
|        - |  643 | `** But by using this automatically generated code, the size of the code` |
|        - |  644 | `** is substantially reduced.  This is important for embedded applications` |
|        - |  645 | `** on platforms with limited memory.` |
|        - |  646 | `*/` |
|        - |  647 | `/* Hash score: 103 */` |
|  2487378 |  648 | `static sxu32 KeywordCode(const char *z, int n){` |
|        - |  649 | `  /* zText[] encodes 532 bytes of keywords in 333 bytes */` |
|        - |  650 | `  /*   extendswitchprintegerequire_oncenddeclareturnamespacechobject      */` |
|        - |  651 | `  /*   hrowbooleandefaultrycaselfinalistaticlonewconstringlobaluse        */` |
|        - |  652 | `  /*   lseifloatvarrayANDIEchoUSECHOabstractclasscontinuendifunction      */` |
|        - |  653 | `  /*   diendwhilevaldoexitgotoimplementsinclude_oncemptyinstanceof        */` |
|        - |  654 | `  /*   interfacendforeachissetparentprivateprotectedpublicatchunset       */` |
|        - |  655 | `  /*   xorARRAYASArrayEXITUNSETXORbreak                                   */` |
|        - |  656 | `  static const char zText[332] = {` |
|        - |  657 | `    'e','x','t','e','n','d','s','w','i','t','c','h','p','r','i','n','t','e',` |
|        - |  658 | `    'g','e','r','e','q','u','i','r','e','_','o','n','c','e','n','d','d','e',` |
|        - |  659 | `    'c','l','a','r','e','t','u','r','n','a','m','e','s','p','a','c','e','c',` |
|        - |  660 | `    'h','o','b','j','e','c','t','h','r','o','w','b','o','o','l','e','a','n',` |
|        - |  661 | `    'd','e','f','a','u','l','t','r','y','c','a','s','e','l','f','i','n','a',` |
|        - |  662 | `    'l','i','s','t','a','t','i','c','l','o','n','e','w','c','o','n','s','t',` |
|        - |  663 | `    'r','i','n','g','l','o','b','a','l','u','s','e','l','s','e','i','f','l',` |
|        - |  664 | `    'o','a','t','v','a','r','r','a','y','A','N','D','I','E','c','h','o','U',` |
|        - |  665 | `    'S','E','C','H','O','a','b','s','t','r','a','c','t','c','l','a','s','s',` |
|        - |  666 | `    'c','o','n','t','i','n','u','e','n','d','i','f','u','n','c','t','i','o',` |
|        - |  667 | `    'n','d','i','e','n','d','w','h','i','l','e','v','a','l','d','o','e','x',` |
|        - |  668 | `    'i','t','g','o','t','o','i','m','p','l','e','m','e','n','t','s','i','n',` |
|        - |  669 | `    'c','l','u','d','e','_','o','n','c','e','m','p','t','y','i','n','s','t',` |
|        - |  670 | `    'a','n','c','e','o','f','i','n','t','e','r','f','a','c','e','n','d','f',` |
|        - |  671 | `    'o','r','e','a','c','h','i','s','s','e','t','p','a','r','e','n','t','p',` |
|        - |  672 | `    'r','i','v','a','t','e','p','r','o','t','e','c','t','e','d','p','u','b',` |
|        - |  673 | `    'l','i','c','a','t','c','h','u','n','s','e','t','x','o','r','A','R','R',` |
|        - |  674 | `    'A','Y','A','S','A','r','r','a','y','E','X','I','T','U','N','S','E','T',` |
|        - |  675 | `    'X','O','R','b','r','e','a','k'` |
|        - |  676 | `  };` |
|        - |  677 | `  static const unsigned char aHash[151] = {` |
|        - |  678 |  |
|        - |  679 |  |
|        - |  680 | `      74,   0,  51,   0,  76,   0,   0,  20,   0,   0,   0,  50,   0,` |
|        - |  681 | `      80,  34,   0,  36,   0,   0,  64,  16,   0,   0,  17,   0,   1,` |
|        - |  682 | `      19,  84,  66,   0,  43,  45,  78,   0,   0,  53,  56,   0,   0,` |
|        - |  683 |  |
|        - |  684 | `      72,  14,   0,  71,   0,  38,   6,   0,   0,   0,  73,   0,   0,` |
|        - |  685 | `       3,   0,  41,   5,  52,  57,  32,   0,  60,  63,   0,  69,  82,` |
|        - |  686 | `      30,   0,  79,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,` |
|        - |  687 |  |
|        - |  688 | `      62,   0,  11,   0,   0,  58,   0,   0,   0,   0,  59,  75,   0,` |
|        - |  689 |  |
|        - |  690 | `  };` |
|        - |  691 | `  static const unsigned char aNext[84] = {` |
|        - |  692 |  |
|        - |  693 |  |
|        - |  694 |  |
|        - |  695 |  |
|        - |  696 |  |
|        - |  697 |  |
|        - |  698 | `      42,   0,   0,   0,  70,  55` |
|        - |  699 | `  };` |
|        - |  700 | `  static const unsigned char aLen[84] = {` |
|        - |  701 | `       7,   9,   6,   5,   7,  12,   7,   2,  10,   7,   6,   9,   4,` |
|        - |  702 | `       6,   5,   7,   4,   3,   7,   3,   4,   4,   5,   4,   6,   5,` |
|        - |  703 | `       2,   3,   5,   6,   6,   3,   6,   4,   2,   5,   3,   5,   3,` |
|        - |  704 | `       3,   4,   3,   4,   8,   5,   2,   8,   5,   8,   3,   8,   5,` |
|        - |  705 | `       4,   2,   4,   4,  10,  12,   7,   5,  10,   9,   3,   6,  10,` |
|        - |  706 | `       3,   7,   2,   5,   6,   7,   9,   6,   5,   5,   3,   5,   2,` |
|        - |  707 | `       5,   4,   5,   3,   2,   5` |
|        - |  708 | `  };` |
|        - |  709 | `  static const sxu16 aOffset[84] = {` |
|        - |  710 |  |
|        - |  711 | `      55,  60,  65,  65,  70,  72,  78,  81,  83,  86,  90,  92,  97,` |
|        - |  712 | `     100, 100, 103, 106, 111, 117, 119, 119, 123, 124, 129, 130, 135,` |
|        - |  713 | `     137, 139, 143, 145, 149, 157, 159, 162, 169, 173, 181, 183, 186,` |
|        - |  714 | `     190, 194, 196, 200, 204, 214, 214, 225, 230, 240, 240, 248, 248,` |
|        - |  715 | `     251, 251, 252, 258, 263, 269, 276, 285, 290, 295, 300, 303, 308,` |
|        - |  716 | `     310, 315, 319, 324, 325, 327` |
|        - |  717 | `  };` |
|        - |  718 | `  static const sxu32 aCode[84] = {` |
|        - |  719 | `    PH7_TKWRD_EXTENDS,   PH7_TKWRD_ENDSWITCH,   PH7_TKWRD_SWITCH,    PH7_TKWRD_PRINT,   PH7_TKWRD_INT,` |
|        - |  720 | `    PH7_TKWRD_REQONCE,   PH7_TKWRD_REQUIRE,     PH7_TKWRD_SEQ,       PH7_TKWRD_ENDDEC,    PH7_TKWRD_DECLARE,` |
|        - |  721 | `    PH7_TKWRD_RETURN,    PH7_TKWRD_NAMESPACE,   PH7_TKWRD_ECHO,      PH7_TKWRD_OBJECT,    PH7_TKWRD_THROW,` |
|        - |  722 | `    PH7_TKWRD_BOOL,      PH7_TKWRD_BOOL,        PH7_TKWRD_AND,       PH7_TKWRD_DEFAULT,   PH7_TKWRD_TRY,` |
|        - |  723 | `    PH7_TKWRD_CASE,      PH7_TKWRD_SELF,        PH7_TKWRD_FINAL,     PH7_TKWRD_LIST,      PH7_TKWRD_STATIC,` |
|        - |  724 | `    PH7_TKWRD_CLONE,     PH7_TKWRD_SNE,         PH7_TKWRD_NEW,       PH7_TKWRD_CONST,     PH7_TKWRD_STRING,` |
|        - |  725 | `    PH7_TKWRD_GLOBAL,    PH7_TKWRD_USE,         PH7_TKWRD_ELIF,      PH7_TKWRD_ELSE,      PH7_TKWRD_IF,` |
|        - |  726 | `    PH7_TKWRD_FLOAT,     PH7_TKWRD_VAR,         PH7_TKWRD_ARRAY,     PH7_TKWRD_AND,       PH7_TKWRD_DIE,` |
|        - |  727 | `    PH7_TKWRD_ECHO,      PH7_TKWRD_USE,         PH7_TKWRD_ECHO,      PH7_TKWRD_ABSTRACT,  PH7_TKWRD_CLASS,` |
|        - |  728 | `    PH7_TKWRD_AS,        PH7_TKWRD_CONTINUE,    PH7_TKWRD_ENDIF,     PH7_TKWRD_FUNCTION,  PH7_TKWRD_DIE,` |
|        - |  729 | `    PH7_TKWRD_ENDWHILE,  PH7_TKWRD_WHILE,       PH7_TKWRD_EVAL,      PH7_TKWRD_DO,        PH7_TKWRD_EXIT,` |
|        - |  730 | `    PH7_TKWRD_GOTO,      PH7_TKWRD_IMPLEMENTS,  PH7_TKWRD_INCONCE,   PH7_TKWRD_INCLUDE,   PH7_TKWRD_EMPTY,` |
|        - |  731 | `    PH7_TKWRD_INSTANCEOF,PH7_TKWRD_INTERFACE,   PH7_TKWRD_INT,       PH7_TKWRD_ENDFOR,    PH7_TKWRD_END4EACH,` |
|        - |  732 | `    PH7_TKWRD_FOR,       PH7_TKWRD_FOREACH,     PH7_TKWRD_OR,        PH7_TKWRD_ISSET,     PH7_TKWRD_PARENT,` |
|        - |  733 | `    PH7_TKWRD_PRIVATE,   PH7_TKWRD_PROTECTED,   PH7_TKWRD_PUBLIC,    PH7_TKWRD_CATCH,     PH7_TKWRD_UNSET,` |
|        - |  734 | `    PH7_TKWRD_XOR,       PH7_TKWRD_ARRAY,       PH7_TKWRD_AS,        PH7_TKWRD_ARRAY,     PH7_TKWRD_EXIT,` |
|        - |  735 | `    PH7_TKWRD_UNSET,     PH7_TKWRD_XOR,         PH7_TKWRD_OR,        PH7_TKWRD_BREAK` |
|        - |  736 | `  };` |
|        - |  737 | `  int h, i;` |
|  2487378 |  738 | `  if( n<2 ) return PH7_TK_ID;` |
|  2397516 |  739 | `  h = (((int)z[0]*4) ^ ((int)z[n-1]*3) ^ n) % 151;` |
|  3670066 |  740 | `  for(i=((int)aHash[h])-1; i>=0; i=((int)aNext[i])-1){` |
|  2122530 |  741 | `    if( (int)aLen[i]==n && SyMemcmp(&zText[aOffset[i]],z,n)==0 ){` |
|        - |  742 | `       /* PH7_TKWRD_EXTENDS */` |
|        - |  743 | `       /* PH7_TKWRD_ENDSWITCH */` |
|        - |  744 | `       /* PH7_TKWRD_SWITCH */` |
|        - |  745 | `       /* PH7_TKWRD_PRINT */` |
|        - |  746 | `       /* PH7_TKWRD_INT */` |
|        - |  747 | `       /* PH7_TKWRD_REQONCE */` |
|        - |  748 | `       /* PH7_TKWRD_REQUIRE */` |
|        - |  749 | `       /* PH7_TKWRD_SEQ */` |
|        - |  750 | `       /* PH7_TKWRD_ENDDEC */` |
|        - |  751 | `       /* PH7_TKWRD_DECLARE */` |
|        - |  752 | `       /* PH7_TKWRD_RETURN */` |
|        - |  753 | `       /* PH7_TKWRD_NAMESPACE */` |
|        - |  754 | `       /* PH7_TKWRD_ECHO */` |
|        - |  755 | `       /* PH7_TKWRD_OBJECT */` |
|        - |  756 | `       /* PH7_TKWRD_THROW */` |
|        - |  757 | `       /* PH7_TKWRD_BOOL */` |
|        - |  758 | `       /* PH7_TKWRD_BOOL */` |
|        - |  759 | `       /* PH7_TKWRD_AND */` |
|        - |  760 | `       /* PH7_TKWRD_DEFAULT */` |
|        - |  761 | `       /* PH7_TKWRD_TRY */` |
|        - |  762 | `       /* PH7_TKWRD_CASE */` |
|        - |  763 | `       /* PH7_TKWRD_SELF */` |
|        - |  764 | `       /* PH7_TKWRD_FINAL */` |
|        - |  765 | `       /* PH7_TKWRD_LIST */` |
|        - |  766 | `       /* PH7_TKWRD_STATIC */` |
|        - |  767 | `       /* PH7_TKWRD_CLONE */` |
|        - |  768 | `       /* PH7_TKWRD_SNE */` |
|        - |  769 | `       /* PH7_TKWRD_NEW */` |
|        - |  770 | `       /* PH7_TKWRD_CONST */` |
|        - |  771 | `       /* PH7_TKWRD_STRING */` |
|        - |  772 | `       /* PH7_TKWRD_GLOBAL */` |
|        - |  773 | `       /* PH7_TKWRD_USE */` |
|        - |  774 | `       /* PH7_TKWRD_ELIF */` |
|        - |  775 | `       /* PH7_TKWRD_ELSE */` |
|        - |  776 | `       /* PH7_TKWRD_IF */` |
|        - |  777 | `       /* PH7_TKWRD_FLOAT */` |
|        - |  778 | `       /* PH7_TKWRD_VAR */` |
|        - |  779 | `       /* PH7_TKWRD_ARRAY */` |
|        - |  780 | `       /* PH7_TKWRD_AND */` |
|        - |  781 | `       /* PH7_TKWRD_DIE */` |
|        - |  782 | `       /* PH7_TKWRD_ECHO */` |
|        - |  783 | `       /* PH7_TKWRD_USE */` |
|        - |  784 | `       /* PH7_TKWRD_ECHO */` |
|        - |  785 | `       /* PH7_TKWRD_ABSTRACT */` |
|        - |  786 | `       /* PH7_TKWRD_CLASS */` |
|        - |  787 | `       /* PH7_TKWRD_AS */` |
|        - |  788 | `       /* PH7_TKWRD_CONTINUE */` |
|        - |  789 | `       /* PH7_TKWRD_ENDIF */` |
|        - |  790 | `       /* PH7_TKWRD_FUNCTION */` |
|        - |  791 | `       /* PH7_TKWRD_DIE */` |
|        - |  792 | `       /* PH7_TKWRD_ENDWHILE */` |
|        - |  793 | `       /* PH7_TKWRD_WHILE */` |
|        - |  794 | `       /* PH7_TKWRD_EVAL */` |
|        - |  795 | `       /* PH7_TKWRD_DO */` |
|        - |  796 | `       /* PH7_TKWRD_EXIT */` |
|        - |  797 | `       /* PH7_TKWRD_GOTO */` |
|        - |  798 | `       /* PH7_TKWRD_IMPLEMENTS */` |
|        - |  799 | `       /* PH7_TKWRD_INCONCE */` |
|        - |  800 | `       /* PH7_TKWRD_INCLUDE */` |
|        - |  801 | `       /* PH7_TKWRD_EMPTY */` |
|        - |  802 | `       /* PH7_TKWRD_INSTANCEOF */` |
|        - |  803 | `       /* PH7_TKWRD_INTERFACE */` |
|        - |  804 | `       /* PH7_TKWRD_INT */` |
|        - |  805 | `       /* PH7_TKWRD_ENDFOR */` |
|        - |  806 | `       /* PH7_TKWRD_END4EACH */` |
|        - |  807 | `       /* PH7_TKWRD_FOR */` |
|        - |  808 | `       /* PH7_TKWRD_FOREACH */` |
|        - |  809 | `       /* PH7_TKWRD_OR */` |
|        - |  810 | `       /* PH7_TKWRD_ISSET */` |
|        - |  811 | `       /* PH7_TKWRD_PARENT */` |
|        - |  812 | `       /* PH7_TKWRD_PRIVATE */` |
|        - |  813 | `       /* PH7_TKWRD_PROTECTED */` |
|        - |  814 | `       /* PH7_TKWRD_PUBLIC */` |
|        - |  815 | `       /* PH7_TKWRD_CATCH */` |
|        - |  816 | `       /* PH7_TKWRD_UNSET */` |
|        - |  817 | `       /* PH7_TKWRD_XOR */` |
|        - |  818 | `       /* PH7_TKWRD_ARRAY */` |
|        - |  819 | `       /* PH7_TKWRD_AS */` |
|        - |  820 | `       /* PH7_TKWRD_ARRAY */` |
|        - |  821 | `       /* PH7_TKWRD_EXIT */` |
|        - |  822 | `       /* PH7_TKWRD_UNSET */` |
|        - |  823 | `       /* PH7_TKWRD_XOR */` |
|        - |  824 | `       /* PH7_TKWRD_OR */` |
|        - |  825 | `       /* PH7_TKWRD_BREAK */` |
|   849980 |  826 | `      return aCode[i];` |
|        - |  827 | `    }` |
|   636275 |  828 | `  }` |
|        - |  829 | `  /* Linear fallback for keywords not in the auto-generated hash table */` |
|  1547538 |  830 | `  if( n==5 && SyMemcmp(z,"trait",5)==0 ) return PH7_TKWRD_TRAIT;` |
|  1547484 |  831 | `  if( n==9 && SyMemcmp(z,"insteadof",9)==0 ) return PH7_TKWRD_INSTEADOF;` |
|  1547480 |  832 | `  if( n==7 && SyMemcmp(z,"finally",7)==0 ) return PH7_TKWRD_FINALLY;` |
|  1547450 |  833 | `  if( n==5 && SyMemcmp(z,"yield",5)==0 ) return PH7_TKWRD_YIELD;` |
|  1547418 |  834 | `  return PH7_TK_ID;` |
|  1243690 |  835 |  |
|        - |  836 | `/* --- End of Automatically generated code --- */` |
|        - |  837 | `/*` |
|        - |  838 | ` * Extract a heredoc/nowdoc text from a raw PHP input.` |
|        - |  839 | ` * According to the PHP language reference manual:` |
|        - |  840 | ` *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier` |
|        - |  841 | ` *  is provided, then a newline. The string itself follows, and then the same identifier again` |
|        - |  842 | ` *  to close the quotation.` |
|        - |  843 | ` *  The closing identifier must begin in the first column of the line. Also, the identifier must` |
|        - |  844 | ` *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric` |
|        - |  845 | ` *  characters and underscores, and must start with a non-digit character or underscore.` |
|        - |  846 | ` *  Heredoc text behaves just like a double-quoted string, without the double quotes.` |
|        - |  847 | ` *  This means that quotes in a heredoc do not need to be escaped, but the escape codes listed` |
|        - |  848 | ` *  above can still be used. Variables are expanded, but the same care must be taken when expressing` |
|        - |  849 | ` *  complex variables inside a heredoc as with strings.` |
|        - |  850 | ` *  Nowdocs are to single-quoted strings what heredocs are to double-quoted strings.` |
|        - |  851 | ` *  A nowdoc is specified similarly to a heredoc, but no parsing is done inside a nowdoc.` |
|        - |  852 | ` *  The construct is ideal for embedding PHP code or other large blocks of text without the need` |
|        - |  853 | ` *  for escaping. It shares some features in common with the SGML <![CDATA[ ]]> construct, in that` |
|        - |  854 | ` *  it declares a block of text which is not for parsing.` |
|        - |  855 | ` *  A nowdoc is identified with the same <<< sequence used for heredocs, but the identifier which follows` |
|        - |  856 | ` *  is enclosed in single quotes, e.g. <<<'EOT'. All the rules for heredoc identifiers also apply to nowdoc` |
|        - |  857 | ` *  identifiers, especially those regarding the appearance of the closing identifier.` |
|        - |  858 | ` * Symisc Extension:` |
|        - |  859 | ` * The closing delimiter can now start with a digit or undersocre or it can be an UTF-8 stream.` |
|        - |  860 | ` * Example:` |
|        - |  861 | ` *  <<<123` |
|        - |  862 | ` *    HEREDOC Here` |
|        - |  863 | ` * 123` |
|        - |  864 | ` *  or` |
|        - |  865 | ` *  <<<___` |
|        - |  866 | ` *   HEREDOC Here` |
|        - |  867 | ` *  ___` |
|        - |  868 | ` */` |
|       56 |  869 | `static sxi32 LexExtractHeredoc(SyStream *pStream,SyToken *pToken)` |
|        2 |  870 |  |
|       58 |  871 | `	const unsigned char *zIn  = pStream->zText;` |
|       58 |  872 | `	const unsigned char *zEnd = pStream->zEnd;` |
|        - |  873 | `	const unsigned char *zPtr;` |
|       58 |  874 | `	sxu8 bNowDoc = FALSE;` |
|        - |  875 | `	SyString sDelim;` |
|        - |  876 | `	SyString sStr;` |
|        - |  877 | `	/* Jump leading white spaces */` |
|       70 |  878 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|       13 |  879 | `		zIn++;` |
|        1 |  880 | `	}` |
|       58 |  881 | `	if( zIn >= zEnd ){` |
|        - |  882 | `		/* A simple symbol,return immediately */` |
|      ! 0 |  883 | `		return SXERR_CONTINUE;` |
|        - |  884 | `	}` |
|       58 |  885 | `	if( zIn[0] == '\'' \|\| zIn[0] == '"' ){` |
|        - |  886 | `		/* Make sure we are dealing with a nowdoc */` |
|       29 |  887 | `		bNowDoc =  zIn[0] == '\'' ? TRUE : FALSE;` |
|       29 |  888 | `		zIn++;` |
|       14 |  889 | `	}` |
|       58 |  890 | `	if( zIn[0] < 0xc0 && !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|        - |  891 | `		/* Invalid delimiter,return immediately */` |
|      ! 0 |  892 | `		return SXERR_CONTINUE;` |
|        - |  893 | `	}` |
|        - |  894 | `	/* Isolate the identifier */` |
|       58 |  895 | `	sDelim.zString = (const char *)zIn;` |
|       64 |  896 | `	for(;;){` |
|      130 |  897 | `		zPtr = zIn;` |
|        - |  898 | `		/* Skip alphanumeric stream */` |
|      424 |  899 | `		while( zPtr < zEnd && zPtr[0] < 0xc0 && (SyisAlphaNum(zPtr[0]) \|\| zPtr[0] == '_') ){` |
|      232 |  900 | `			zPtr++;` |
|        2 |  901 | `		}` |
|      130 |  902 | `		if( zPtr < zEnd && zPtr[0] >= 0xc0 ){` |
|       19 |  903 | `			zPtr++;` |
|        - |  904 | `			/* UTF-8 stream */` |
|       37 |  905 | `			while( zPtr < zEnd && ((zPtr[0] & 0xc0) == 0x80) ){` |
|       19 |  906 | `				zPtr++;` |
|        1 |  907 | `			}` |
|        9 |  908 | `		}` |
|      130 |  909 | `		if( zPtr == zIn ){` |
|        - |  910 | `			/* Not an UTF-8 or alphanumeric stream */` |
|       58 |  911 | `			break;` |
|        - |  912 | `		}` |
|        - |  913 | `		/* Synchronize pointers */` |
|       74 |  914 | `		zIn = zPtr;` |
|        2 |  915 | `	}` |
|        - |  916 | `	/* Get the identifier length */` |
|       58 |  917 | `	sDelim.nByte = (sxu32)((const char *)zIn-sDelim.zString);` |
|       58 |  918 | `	if( zIn[0] == '"' \|\| (bNowDoc && zIn[0] == '\'') ){` |
|        - |  919 | `		/* Jump the trailing single quote */` |
|       29 |  920 | `		zIn++;` |
|       14 |  921 | `	}` |
|        - |  922 | `	/* Jump trailing white spaces */` |
|       58 |  923 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|      ! 0 |  924 | `		zIn++;` |
|      ! 0 |  925 | `	}` |
|       58 |  926 | `	if( sDelim.nByte <= 0 \|\| zIn >= zEnd \|\| zIn[0] != '\n' ){` |
|        - |  927 | `		/* Invalid syntax */` |
|      ! 0 |  928 | `		return SXERR_CONTINUE;` |
|        - |  929 | `	}` |
|       58 |  930 | `	pStream->nLine++; /* Increment line counter */` |
|       58 |  931 | `	zIn++;` |
|        - |  932 | `	/* Isolate the delimited string */` |
|       58 |  933 | `	sStr.zString = (const char *)zIn;` |
|        - |  934 | `	/* Go and found the closing delimiter */` |
|       75 |  935 | `	for(;;){` |
|        - |  936 | `		/* Synchronize with the next line */` |
|     3018 |  937 | `		while( zIn < zEnd && zIn[0] != '\n' ){` |
|     2868 |  938 | `			zIn++;` |
|        2 |  939 | `		}` |
|      152 |  940 | `		if( zIn >= zEnd ){` |
|        - |  941 | `			/* End of the input reached, break immediately */` |
|       12 |  942 | `			pStream->zText = pStream->zEnd;` |
|       12 |  943 | `			break;` |
|        - |  944 | `		}` |
|      142 |  945 | `		pStream->nLine++; /* Increment line counter */` |
|      142 |  946 | `		zIn++;` |
|      142 |  947 | `		if( (sxu32)(zEnd - zIn) >= sDelim.nByte && SyMemcmp((const void *)sDelim.zString,(const void *)zIn,sDelim.nByte) == 0 ){` |
|       50 |  948 | `			zPtr = &zIn[sDelim.nByte];` |
|       62 |  949 | `			while( zPtr < zEnd && zPtr[0] < 0xc0 && SyisSpace(zPtr[0]) && zPtr[0] != '\n' ){` |
|       13 |  950 | `				zPtr++;` |
|        1 |  951 | `			}` |
|       50 |  952 | `			if( zPtr >= zEnd ){` |
|        - |  953 | `				/* End of input */` |
|      ! 0 |  954 | `				pStream->zText = zPtr;` |
|      ! 0 |  955 | `				break;` |
|        - |  956 | `			}` |
|       50 |  957 | `			if( zPtr[0] == ';' ){` |
|       50 |  958 | `				const unsigned char *zCur = zPtr;` |
|       50 |  959 | `				zPtr++;` |
|       52 |  960 | `				while( zPtr < zEnd && zPtr[0] < 0xc0 && SyisSpace(zPtr[0]) && zPtr[0] != '\n' ){` |
|        3 |  961 | `					zPtr++;` |
|        1 |  962 | `				}` |
|       50 |  963 | `				if( zPtr >= zEnd \|\| zPtr[0] == '\n' ){` |
|        - |  964 | `					/* Closing delimiter found,break immediately */` |
|       48 |  965 | `					pStream->zText = zCur; /* Keep the semi-colon */` |
|       48 |  966 | `					break;` |
|        1 |  967 | `				}` |
|        1 |  968 | `			}else if( zPtr[0] == '\n' ){` |
|        - |  969 | `				/* Closing delimiter found,break immediately */` |
|      ! 0 |  970 | `				pStream->zText = zPtr; /* Synchronize with the stream cursor */` |
|      ! 0 |  971 | `				break;` |
|        - |  972 | `			}` |
|        - |  973 | `			/* Synchronize pointers and continue searching */` |
|        3 |  974 | `			zIn = zPtr;` |
|        1 |  975 | `		}` |
|        2 |  976 | `	} /* For(;;) */` |
|        - |  977 | `	/* Get the delimited string length */` |
|       58 |  978 | `	sStr.nByte = (sxu32)((const char *)zIn-sStr.zString);` |
|        - |  979 | `	/* Record token type and length */` |
|       58 |  980 | `	pToken->nType = bNowDoc ? PH7_TK_NOWDOC : PH7_TK_HEREDOC;` |
|       58 |  981 | `	SyStringDupPtr(&pToken->sData,&sStr);` |
|        - |  982 | `	/* Remove trailing white spaces */` |
|      104 |  983 | `	SyStringRightTrim(&pToken->sData);` |
|        - |  984 | `	/* All done */` |
|       58 |  985 | `	return SXRET_OK;` |
|       30 |  986 |  |
|        - |  987 | `/*` |
|        - |  988 | ` * Tokenize a raw PHP input.` |
|        - |  989 | ` * This is the public tokenizer called by most code generator routines.` |
|        - |  990 | ` */` |
|    13212 |  991 | `PH7_PRIVATE sxi32 PH7_TokenizePHP(const char *zInput,sxu32 nLen,sxu32 nLineStart,SySet *pOut)` |
|        2 |  992 |  |
|        - |  993 | `	SyLex sLexer;` |
|        - |  994 | `	sxi32 rc;` |
|        - |  995 | `	/* Initialize the lexer */` |
|    13214 |  996 | `	rc = SyLexInit(&sLexer,&(*pOut),TokenizePHP,0);` |
|    13214 |  997 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  998 | `		return rc;` |
|        - |  999 | `	}` |
|    13214 | 1000 | `	sLexer.sStream.nLine = nLineStart;` |
|        - | 1001 | `	/* Tokenize input */` |
|    13214 | 1002 | `	rc = SyLexTokenizeInput(&sLexer,zInput,nLen,0,0,0);` |
|        - | 1003 | `	/* Release the lexer */` |
|    13214 | 1004 | `	SyLexRelease(&sLexer);` |
|        - | 1005 | `	/* Tokenization result */` |
|    13214 | 1006 | `	return rc;` |
|     6608 | 1007 |  |
|        - | 1008 | `/*` |
|        - | 1009 | ` * High level public tokenizer.` |
|        - | 1010 | ` *  Tokenize the input into PHP tokens and raw tokens [i.e: HTML,XML,Raw text...].` |
|        - | 1011 | ` * According to the PHP language reference manual` |
|        - | 1012 | ` *   When PHP parses a file, it looks for opening and closing tags, which tell PHP` |
|        - | 1013 | ` *   to start and stop interpreting the code between them. Parsing in this manner allows` |
|        - | 1014 | ` *   PHP to be embedded in all sorts of different documents, as everything outside of a pair` |
|        - | 1015 | ` *   of opening and closing tags is ignored by the PHP parser. Most of the time you will see` |
|        - | 1016 | ` *   PHP embedded in HTML documents, as in this example.` |
|        - | 1017 | ` *   <?php echo 'While this is going to be parsed.'; ?>` |
|        - | 1018 | ` *   <p>This will also be ignored.</p>` |
|        - | 1019 | ` *   You can also use more advanced structures:` |
|        - | 1020 | ` *   Example #1 Advanced escaping` |
|        - | 1021 | ` * <?php` |
|        - | 1022 | ` * if ($expression) {` |
|        - | 1023 | ` *   ?>` |
|        - | 1024 | ` *   <strong>This is true.</strong>` |
|        - | 1025 | ` *   <?php` |
|        - | 1026 | ` * } else {` |
|        - | 1027 | ` *   ?>` |
|        - | 1028 | ` *   <strong>This is false.</strong>` |
|        - | 1029 | ` *   <?php` |
|        - | 1030 | ` * }` |
|        - | 1031 | ` * ?>` |
|        - | 1032 | ` * This works as expected, because when PHP hits the ?> closing tags, it simply starts outputting` |
|        - | 1033 | ` * whatever it finds (except for an immediately following newline - see instruction separation ) until it hits` |
|        - | 1034 | ` * another opening tag. The example given here is contrived, of course, but for outputting large blocks of text` |
|        - | 1035 | ` * dropping out of PHP parsing mode is generally more efficient than sending all of the text through echo() or print().` |
|        - | 1036 | ` * There are four different pairs of opening and closing tags which can be used in PHP. Three of those, <?php ?>` |
|        - | 1037 | ` * <script language="php"> </script>  and <? ?> are always available. The other two are short tags and ASP style` |
|        - | 1038 | ` * tags, and can be turned on and off from the php.ini configuration file. As such, while some people find short tags` |
|        - | 1039 | ` * and ASP style tags convenient, they are less portable, and generally not recommended.` |
|        - | 1040 | ` * Note:` |
|        - | 1041 | ` * Also note that if you are embedding PHP within XML or XHTML you will need to use the <?php ?> tags to remain` |
|        - | 1042 | ` * compliant with standards.` |
|        - | 1043 | ` * Example #2 PHP Opening and Closing Tags` |
|        - | 1044 | ` * 1.  <?php echo 'if you want to serve XHTML or XML documents, do it like this'; ?>` |
|        - | 1045 | ` * 2.  <script language="php">` |
|        - | 1046 | ` *       echo 'some editors (like FrontPage) don\'t` |
|        - | 1047 | ` *             like processing instructions';` |
|        - | 1048 | ` *   </script>` |
|        - | 1049 | ` *` |
|        - | 1050 | ` * 3.  <? echo 'this is the simplest, an SGML processing instruction'; ?>` |
|        - | 1051 | ` *   <?= expression ?> This is a shortcut for "<? echo expression ?>"` |
|        - | 1052 | ` */` |
|    10926 | 1053 | `PH7_PRIVATE sxi32 PH7_TokenizeRawText(const char *zInput,sxu32 nLen,SySet *pOut)` |
|        2 | 1054 |  |
|    10928 | 1055 | `	const char *zEnd = &zInput[nLen];` |
|    10928 | 1056 | `	const char *zIn  = zInput;` |
|        - | 1057 | `	const char *zCur,*zCurEnd;` |
|    10928 | 1058 | `	SyString sCtag = { 0, 0 };     /* Closing tag */` |
|        - | 1059 | `	SyToken sToken;` |
|        - | 1060 | `	SyString sDoc;` |
|        - | 1061 | `	sxu32 nLine;` |
|        - | 1062 | `	sxi32 iNest;` |
|        - | 1063 | `	sxi32 rc;` |
|        - | 1064 | `	/* Tokenize the input into PHP tokens and raw tokens */` |
|    10928 | 1065 | `	nLine = 1;` |
|    10928 | 1066 | `	zCur = zCurEnd   = 0; /* Prevent compiler warning */` |
|    10928 | 1067 | `	sToken.pUserData = 0;` |
|    10928 | 1068 | `	iNest = 0;` |
|    10928 | 1069 | `	sDoc.nByte = 0;` |
|    10928 | 1070 | `	sDoc.zString = ""; /* cc warning */` |
|    10928 | 1071 | `	for(;;){` |
|    21858 | 1072 | `		if( zIn >= zEnd ){` |
|        - | 1073 | `			/* End of input reached */` |
|    10924 | 1074 | `			break;` |
|        - | 1075 | `		}` |
|    10936 | 1076 | `		sToken.nLine = nLine;` |
|    10936 | 1077 | `		zCur = zIn;` |
|    10936 | 1078 | `		zCurEnd = 0;` |
|    10944 | 1079 | `		while( zIn < zEnd ){` |
|    10940 | 1080 | `			 if( zIn[0] == '<' ){` |
|    10932 | 1081 | `				const char *zTmp = zIn; /* End of raw input marker */` |
|    10932 | 1082 | `				zIn++;` |
|    10932 | 1083 | `				if( zIn < zEnd ){` |
|    10932 | 1084 | `					if( zIn[0] == '?' ){` |
|    10932 | 1085 | `						zIn++;` |
|    10932 | 1086 | `						if( (sxu32)(zEnd - zIn) >= sizeof("php")-1 &&  SyStrnicmp(zIn,"php",sizeof("php")-1) == 0 ){` |
|        - | 1087 | `							/* opening tag: <?php */` |
|    10930 | 1088 | `							zIn += sizeof("php")-1;` |
|     5464 | 1089 | `						}` |
|        - | 1090 | `						/* Look for the closing tag '?>' */` |
|    10932 | 1091 | `						SyStringInitFromBuf(&sCtag,"?>",sizeof("?>")-1);` |
|    10932 | 1092 | `						zCurEnd = zTmp;` |
|    10932 | 1093 | `						break;` |
|        - | 1094 | `					}` |
|      ! 0 | 1095 | `				}` |
|      ! 0 | 1096 | `			}else{` |
|       10 | 1097 | `				if( zIn[0] == '\n' ){` |
|       10 | 1098 | `					nLine++;` |
|        4 | 1099 | `				}` |
|       10 | 1100 | `				zIn++;` |
|        - | 1101 | `			 }` |
|        2 | 1102 | `		} /* While(zIn < zEnd) */` |
|    10936 | 1103 | `		if( zCurEnd == 0 ){` |
|        5 | 1104 | `			zCurEnd = zIn;` |
|        2 | 1105 | `		}` |
|        - | 1106 | `		/* Save the raw token */` |
|    10936 | 1107 | `		SyStringInitFromBuf(&sToken.sData,zCur,zCurEnd - zCur);` |
|    10936 | 1108 | `		sToken.nType = PH7_TOKEN_RAW;` |
|    10936 | 1109 | `		rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|    10936 | 1110 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1111 | `			return rc;` |
|        - | 1112 | `		}` |
|    10936 | 1113 | `		if( zIn >= zEnd ){` |
|        5 | 1114 | `			break;` |
|        - | 1115 | `		}` |
|        - | 1116 | `		/* Ignore leading white space */` |
|    23746 | 1117 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    12816 | 1118 | `			if( zIn[0] == '\n' ){` |
|    11620 | 1119 | `				nLine++;` |
|     5809 | 1120 | `			}` |
|    12816 | 1121 | `			zIn++;` |
|        2 | 1122 | `		}` |
|        - | 1123 | `		/* Delimit the PHP chunk */` |
|    10932 | 1124 | `		sToken.nLine = nLine;` |
|    10932 | 1125 | `		zCur = zIn;` |
|  1007006 | 1126 | `		while( (sxu32)(zEnd - zIn) >= sCtag.nByte ){` |
|        - | 1127 | `			const char *zPtr;` |
|  1002282 | 1128 | `			if( SyMemcmp(zIn,sCtag.zString,sCtag.nByte) == 0 && iNest < 1 ){` |
|     6208 | 1129 | `				break;` |
|        - | 1130 | `			}` |
|   499983 | 1131 | `			for(;;){` |
|   999968 | 1132 | `				if( zIn[0] != '/' \|\| (zIn[1] != '*' && zIn[1] != '/') /* && sCtag.nByte >= 2 */ ){` |
|   498039 | 1133 | `					break;` |
|        - | 1134 | `				}` |
|     3894 | 1135 | `				zIn += 2;` |
|     3894 | 1136 | `				if( zIn[-1] == '/' ){` |
|        - | 1137 | `					/* Inline comment */` |
|   134570 | 1138 | `					while( zIn < zEnd && zIn[0] != '\n' ){` |
|   130760 | 1139 | `						zIn++;` |
|        2 | 1140 | `					}` |
|     3812 | 1141 | `					if( zIn >= zEnd ){` |
|      ! 0 | 1142 | `						zIn--;` |
|      ! 0 | 1143 | `					}` |
|     1907 | 1144 | `				}else{` |
|        - | 1145 | `					/* Block comment */` |
|     4500 | 1146 | `					while( (sxu32)(zEnd-zIn) >= sizeof("*/") - 1 ){` |
|     4500 | 1147 | `						if( zIn[0] == '*' && zIn[1] == '/' ){` |
|       84 | 1148 | `							zIn += 2;` |
|       84 | 1149 | `							break;` |
|        - | 1150 | `						}` |
|     4418 | 1151 | `						if( zIn[0] == '\n' ){` |
|       28 | 1152 | `							nLine++;` |
|       13 | 1153 | `						}` |
|     4418 | 1154 | `						zIn++;` |
|        2 | 1155 | `					}` |
|        - | 1156 | `				}` |
|        2 | 1157 | `			}` |
|   996076 | 1158 | `			if( zIn[0] == '\n' ){` |
|    34946 | 1159 | `				nLine++;` |
|    34946 | 1160 | `				if( iNest > 0 ){` |
|      156 | 1161 | `					zIn++;` |
|      156 | 1162 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|      ! 0 | 1163 | `						zIn++;` |
|      ! 0 | 1164 | `					}` |
|      156 | 1165 | `					zPtr = zIn;` |
|      864 | 1166 | `					while( zIn < zEnd ){` |
|      864 | 1167 | `						if( (unsigned char)zIn[0] >= 0xc0 ){` |
|        - | 1168 | `							/* UTF-8 stream */` |
|       19 | 1169 | `							zIn++;` |
|       37 | 1170 | `							SX_JMP_UTF8(zIn,zEnd);` |
|      854 | 1171 | `						}else if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       79 | 1172 | `							break;` |
|      ! 0 | 1173 | `						}else{` |
|      692 | 1174 | `							zIn++;` |
|        - | 1175 | `						}` |
|        2 | 1176 | `					}` |
|      156 | 1177 | `					if( (sxu32)(zIn - zPtr) == sDoc.nByte && SyMemcmp(sDoc.zString,zPtr,sDoc.nByte) == 0 ){` |
|       60 | 1178 | `						iNest = 0;` |
|       29 | 1179 | `					}` |
|      156 | 1180 | `					continue;` |
|        2 | 1181 | `				}` |
|   978527 | 1182 | `			}else if ( (sxu32)(zEnd - zIn) >= sizeof("<<<") && zIn[0] == '<' && zIn[1] == '<' && zIn[2] == '<' && iNest < 1){` |
|       62 | 1183 | `				zIn += sizeof("<<<")-1;` |
|       74 | 1184 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|       13 | 1185 | `					zIn++;` |
|        1 | 1186 | `				}` |
|       62 | 1187 | `				if( zIn[0] == '"' \|\| zIn[0] == '\'' ){` |
|       32 | 1188 | `					zIn++;` |
|       15 | 1189 | `				}` |
|       62 | 1190 | `				zPtr = zIn;` |
|      330 | 1191 | `				while( zIn < zEnd ){` |
|      330 | 1192 | `					if( (unsigned char)zIn[0] >= 0xc0 ){` |
|        - | 1193 | `						/* UTF-8 stream */` |
|       19 | 1194 | `						zIn++;` |
|       37 | 1195 | `						SX_JMP_UTF8(zIn,zEnd);` |
|      320 | 1196 | `					}else if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       32 | 1197 | `						break;` |
|      ! 0 | 1198 | `					}else{` |
|      252 | 1199 | `						zIn++;` |
|        - | 1200 | `					}` |
|        2 | 1201 | `				}` |
|       62 | 1202 | `				SyStringInitFromBuf(&sDoc,zPtr,zIn-zPtr);` |
|       62 | 1203 | `				SyStringFullTrim(&sDoc);` |
|       62 | 1204 | `				if( sDoc.nByte > 0 ){` |
|       62 | 1205 | `					iNest++;` |
|       30 | 1206 | `				}` |
|       62 | 1207 | `				continue;` |
|        - | 1208 | `			}` |
|   995862 | 1209 | `			zIn++;` |
|        - | 1210 |  |
|   995862 | 1211 | `			if ( zIn >= zEnd )` |
|      ! 0 | 1212 | `				break;` |
|        2 | 1213 | `		}` |
|    10932 | 1214 | `		if( (sxu32)(zEnd - zIn) < sCtag.nByte ){` |
|     4726 | 1215 | `			zIn = zEnd;` |
|     2362 | 1216 | `		}` |
|    10932 | 1217 | `		if( zCur < zIn ){` |
|        - | 1218 | `			/* Save the PHP chunk for later processing */` |
|     8816 | 1219 | `			sToken.nType = PH7_TOKEN_PHP;` |
|     8816 | 1220 | `			SyStringInitFromBuf(&sToken.sData,zCur,zIn-zCur);` |
|    17606 | 1221 | `			SyStringRightTrim(&sToken.sData); /* Trim trailing white spaces */` |
|     8816 | 1222 | `			rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|     8816 | 1223 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1224 | `				return rc;` |
|        - | 1225 | `			}` |
|     4407 | 1226 | `		}` |
|    10932 | 1227 | `		if( zIn < zEnd ){` |
|        - | 1228 | `			/* Jump the trailing closing tag */` |
|     6208 | 1229 | `			zIn += sCtag.nByte;` |
|     3103 | 1230 | `		}` |
|        2 | 1231 | `	} /* For(;;) */` |
|        - | 1232 |  |
|    10928 | 1233 | ` 	return SXRET_OK;` |
|     5465 | 1234 |  |
|        - | 1235 |  |
