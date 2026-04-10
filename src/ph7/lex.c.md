# src/ph7/lex.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 701/736 lines (95.24%)

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
|  6916164 |   19 | `static sxi32 TokenizePHP(SyStream *pStream,SyToken *pToken,void *pUserData,void *pCtxData)` |
|        2 |   20 |  |
|        - |   21 | `	SyString *pStr;` |
|        - |   22 | `	sxi32 rc;` |
|        - |   23 | `	/* Ignore leading white spaces */` |
| 10418752 |   24 | `	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){` |
|        - |   25 | `		/* Advance the stream cursor */` |
|  3502588 |   26 | `		if( pStream->zText[0] == '\n' ){` |
|        - |   27 | `			/* Update line counter */` |
|    29796 |   28 | `			pStream->nLine++;` |
|    14897 |   29 | `		}` |
|  3502588 |   30 | `		pStream->zText++;` |
|        2 |   31 | `	}` |
|  6916166 |   32 | `	if( pStream->zText >= pStream->zEnd ){` |
|        - |   33 | `		/* End of input reached */` |
|      ! 0 |   34 | `		return SXERR_EOF;` |
|        - |   35 | `	}` |
|        - |   36 | `	/* Record token starting position and line */` |
|  6916166 |   37 | `	pToken->nLine = pStream->nLine;` |
|  6916166 |   38 | `	pToken->pUserData = 0;` |
|  6916166 |   39 | `	pStr = &pToken->sData;` |
|  6916166 |   40 | `	SyStringInitFromBuf(pStr,pStream->zText,0);` |
|  8165470 |   41 | `	if( pStream->zText[0] >= 0xc0 \|\| SyisAlpha(pStream->zText[0]) \|\| pStream->zText[0] == '_' ){` |
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
|  2498610 |   52 | `		if( pStream->zText[0] < 0xc0 ){` |
|  2498594 |   53 | `			pStream->zText++;` |
|  1249296 |   54 | `		}` |
|  2453471 |   55 | `		for(;;){` |
|  4906944 |   56 | `			zIn = pStream->zText;` |
|  4906944 |   57 | `			if( zIn[0] >= 0xc0 ){` |
|       49 |   58 | `				zIn++;` |
|        - |   59 | `				/* UTF-8 stream */` |
|      109 |   60 | `				while( zIn < pStream->zEnd && ((zIn[0] & 0xc0) == 0x80) ){` |
|       61 |   61 | `					zIn++;` |
|        1 |   62 | `				}` |
|       24 |   63 | `			}` |
|        - |   64 | `			/* Skip alphanumeric stream */` |
| 20139495 |   65 | `			while( zIn < pStream->zEnd && zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_') ){` |
| 12779082 |   66 | `				zIn++;` |
|        2 |   67 | `			}` |
|  4906944 |   68 | `			if( zIn == pStream->zText ){` |
|        - |   69 | `				/* Not an UTF-8 or alphanumeric stream */` |
|  2498610 |   70 | `				break;` |
|        - |   71 | `			}` |
|        - |   72 | `			/* Synchronize pointers */` |
|  2408336 |   73 | `			pStream->zText = zIn;` |
|        2 |   74 | `		}` |
|        - |   75 | `		/* Record token length */` |
|  2498610 |   76 | `		pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  2498610 |   77 | `		nKeyword = KeywordCode(pStr->zString,(int)pStr->nByte);` |
|  2498610 |   78 | `		if( nKeyword != PH7_TK_ID ){` |
|   853872 |   79 | `			if( nKeyword &` |
|        - |   80 | `				(PH7_TKWRD_NEW\|PH7_TKWRD_CLONE\|PH7_TKWRD_AND\|PH7_TKWRD_XOR\|PH7_TKWRD_OR\|PH7_TKWRD_INSTANCEOF\|PH7_TKWRD_SEQ\|PH7_TKWRD_SNE) ){` |
|        - |   81 | `					/* Alpha stream operators [i.e: new,clone,and,instanceof,eq,ne,or,xor],save the operator instance for later processing */` |
|    13996 |   82 | `					pToken->pUserData = (void *)PH7_ExprExtractOperator(pStr,0);` |
|        - |   83 | `					/* Mark as an operator */` |
|    13996 |   84 | `					pToken->nType = PH7_TK_ID\|PH7_TK_OP;` |
|     6999 |   85 | `			}else{` |
|        - |   86 | `				/* We are dealing with a keyword [i.e: while,foreach,class...],save the keyword ID */` |
|   839878 |   87 | `				pToken->nType = PH7_TK_KEYWORD;` |
|   839878 |   88 | `				pToken->pUserData = SX_INT_TO_PTR(nKeyword);` |
|        - |   89 | `			}` |
|   426937 |   90 | `		}else{` |
|        - |   91 | `			/* A simple identifier */` |
|  1644740 |   92 | `			pToken->nType = PH7_TK_ID;` |
|        - |   93 | `		}` |
|  1249306 |   94 | `	}else{` |
|        - |   95 | `		sxi32 c;` |
|        - |   96 | `		/* Non-alpha stream */` |
|  4451537 |   97 | `		if( pStream->zText[0] == '#' \|\|` |
|  4417556 |   98 | `			( pStream->zText[0] == '/' &&  &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '/') ){` |
|     3782 |   99 | `				pStream->zText++;` |
|        - |  100 | `				/* Inline comments */` |
|   137184 |  101 | `				while( pStream->zText < pStream->zEnd && pStream->zText[0] != '\n' ){` |
|   133404 |  102 | `					pStream->zText++;` |
|        2 |  103 | `				}` |
|        - |  104 | `				/* Tell the upper-layer to ignore this token */` |
|     3782 |  105 | `				return SXERR_CONTINUE;` |
|  4413778 |  106 | `		}else if( pStream->zText[0] == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '*' ){` |
|    64116 |  107 | `			pStream->zText += 2;` |
|        - |  108 | `			/* Block comment */` |
|  1818740 |  109 | `			while( pStream->zText < pStream->zEnd ){` |
|  1818740 |  110 | `				if( pStream->zText[0] == '*' ){` |
|    64142 |  111 | `					if( &pStream->zText[1] >= pStream->zEnd \|\| pStream->zText[1] == '/'  ){` |
|    32059 |  112 | `						break;` |
|        - |  113 | `					}` |
|       13 |  114 | `				}` |
|  1754626 |  115 | `				if( pStream->zText[0] == '\n' ){` |
|       28 |  116 | `					pStream->nLine++;` |
|       13 |  117 | `				}` |
|  1754626 |  118 | `				pStream->zText++;` |
|        2 |  119 | `			}` |
|    64116 |  120 | `			pStream->zText += 2;` |
|        - |  121 | `			/* Tell the upper-layer to ignore this token */` |
|    64116 |  122 | `			return SXERR_CONTINUE;` |
|  4349664 |  123 | `		}else if( SyisDigit(pStream->zText[0]) ){` |
|    88634 |  124 | `			pStream->zText++;` |
|        - |  125 | `			/* PHP 7.4: handle underscore separator immediately following the first digit.` |
|        - |  126 | `			 * Check pStream->zText < pStream->zEnd BEFORE forming pStream->zText + 1 so` |
|        - |  127 | `			 * we never compute a pointer past one-past-end. */` |
|    88712 |  128 | `			if( pStream->zText < pStream->zEnd` |
|    88632 |  129 | `				&& pStream->zText[0] == '_'` |
|    44396 |  130 | `				&& pStream->zText + 1 < pStream->zEnd` |
|      160 |  131 | `				&& pStream->zText[1] < 0xc0` |
|      162 |  132 | `				&& SyisDigit(pStream->zText[1]) ){` |
|      151 |  133 | `				pStream->zText++; /* swallow underscore between two digits */` |
|       75 |  134 | `			}` |
|        - |  135 | `			/* Decimal digit stream (PHP 7.4: underscore separator allowed between two digits) */` |
|    97972 |  136 | `			while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|     9340 |  137 | `				pStream->zText++;` |
|     9424 |  138 | `				if( pStream->zText < pStream->zEnd` |
|     9338 |  139 | `					&& pStream->zText[0] == '_'` |
|     4755 |  140 | `					&& pStream->zText + 1 < pStream->zEnd` |
|      172 |  141 | `					&& pStream->zText[1] < 0xc0` |
|      174 |  142 | `					&& SyisDigit(pStream->zText[1]) ){` |
|      173 |  143 | `					pStream->zText++; /* swallow underscore between two digits */` |
|       86 |  144 | `				}` |
|        2 |  145 | `			}` |
|        - |  146 | `			/* Mark the token as integer until we encounter a real number */` |
|    88634 |  147 | `			pToken->nType = PH7_TK_INTEGER;` |
|    88634 |  148 | `			if( pStream->zText < pStream->zEnd ){` |
|    88634 |  149 | `				c = pStream->zText[0];` |
|    88634 |  150 | `				if( c == '.' ){` |
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
|    88411 |  187 | `				}else if( c=='e' \|\| c=='E' ){` |
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
|    88174 |  209 | `				}else if( c == 'x' \|\| c == 'X' ){` |
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
|    88124 |  222 | `				}else if(c  == 'b' \|\| c == 'B' ){` |
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
|    44316 |  235 | `			}` |
|        - |  236 | `			/* PHP 7.4: absorb a trailing malformed underscore run into the` |
|        - |  237 | `			 * numeric token so the compile phase can emit a PHP-compatible` |
|        - |  238 | `			 * "syntax error, unexpected identifier" parse error. Valid` |
|        - |  239 | `			 * separators were already consumed by the per-loop peek logic` |
|        - |  240 | `			 * above, so an underscore here is always misplaced. */` |
|    88634 |  241 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '_' ){` |
|       15 |  242 | `				pStream->zText++;` |
|       44 |  243 | `				while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0` |
|       46 |  244 | `					&& (SyisAlphaNum(pStream->zText[0]) \|\| pStream->zText[0] == '_') ){` |
|       17 |  245 | `					pStream->zText++;` |
|        1 |  246 | `				}` |
|        7 |  247 | `			}` |
|        - |  248 | `			/* Record token length */` |
|    88634 |  249 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|    88634 |  250 | `			return SXRET_OK;` |
|        - |  251 | `		}` |
|  4261032 |  252 | `		c = pStream->zText[0];` |
|  4261032 |  253 | `		pStream->zText++; /* Advance the stream cursor */` |
|        - |  254 | `		/* Assume we are dealing with an operator*/` |
|  4261032 |  255 | `		pToken->nType = PH7_TK_OP;` |
|  4261032 |  256 | `		switch(c){` |
|   893536 |  257 | `		case '$': pToken->nType = PH7_TK_DOLLAR; break;` |
|   338644 |  258 | `		case '{': pToken->nType = PH7_TK_OCB;    break;` |
|   338630 |  259 | `		case '}': pToken->nType = PH7_TK_CCB;    break;` |
|   674030 |  260 | `		case '(': pToken->nType = PH7_TK_LPAREN; break;` |
|    68854 |  261 | `		case '[': pToken->nType \|= PH7_TK_OSB;   break; /* Bitwise operation here,since the square bracket token '['` |
|        - |  262 | `														 * is a potential operator [i.e: subscripting] */` |
|    68860 |  263 | `		case ']': pToken->nType = PH7_TK_CSB;    break;` |
|   337008 |  264 | `		case ')': {` |
|   674018 |  265 | `			SySet *pTokSet = pStream->pSet;` |
|        - |  266 | `			/* Assemble type cast operators [i.e: (int),(float),(bool)...] */` |
|   674018 |  267 | `			if( pTokSet->nUsed >= 2 ){` |
|        - |  268 | `				SyToken *pTmp;` |
|        - |  269 | `				/* Peek the last recongnized token */` |
|   674016 |  270 | `				pTmp = (SyToken *)SySetPeek(pTokSet);` |
|   674016 |  271 | `				if( pTmp->nType & PH7_TK_KEYWORD ){` |
|    13742 |  272 | `					sxi32 nID = SX_PTR_TO_INT(pTmp->pUserData);` |
|    13742 |  273 | `					if( (sxu32)nID & (PH7_TKWRD_ARRAY\|PH7_TKWRD_INT\|PH7_TKWRD_FLOAT\|PH7_TKWRD_STRING\|PH7_TKWRD_OBJECT\|PH7_TKWRD_BOOL\|PH7_TKWRD_UNSET) ){` |
|    13642 |  274 | `						pTmp = (SyToken *)SySetAt(pTokSet,pTokSet->nUsed - 2);` |
|    13642 |  275 | `						if( pTmp->nType & PH7_TK_LPAREN ){` |
|        - |  276 | `							/* Merge the three tokens '(' 'TYPE' ')' into a single one */` |
|    13558 |  277 | `							const char * zTypeCast = "(int)";` |
|    13558 |  278 | `							if( nID & PH7_TKWRD_FLOAT ){` |
|     2676 |  279 | `								zTypeCast = "(float)";` |
|    12221 |  280 | `							}else if( nID & PH7_TKWRD_BOOL ){` |
|     2678 |  281 | `								zTypeCast = "(bool)";` |
|     9546 |  282 | `							}else if( nID & PH7_TKWRD_STRING ){` |
|     5346 |  283 | `								zTypeCast = "(string)";` |
|     5536 |  284 | `							}else if( nID & PH7_TKWRD_ARRAY ){` |
|       21 |  285 | `								zTypeCast = "(array)";` |
|     2854 |  286 | `							}else if( nID & PH7_TKWRD_OBJECT ){` |
|       17 |  287 | `								zTypeCast = "(object)";` |
|     2836 |  288 | `							}else if( nID & PH7_TKWRD_UNSET ){` |
|        7 |  289 | `								zTypeCast = "(unset)";` |
|        3 |  290 | `							}` |
|        - |  291 | `							/* Reflect the change */` |
|    13558 |  292 | `							pToken->nType = PH7_TK_OP;` |
|    13558 |  293 | `							SyStringInitFromBuf(&pToken->sData,zTypeCast,SyStrlen(zTypeCast));` |
|        - |  294 | `							/* Save the instance associated with the type cast operator */` |
|    13558 |  295 | `							pToken->pUserData = (void *)PH7_ExprExtractOperator(&pToken->sData,0);` |
|        - |  296 | `							/* Remove the two previous tokens */` |
|    13558 |  297 | `							pTokSet->nUsed -= 2;` |
|    13558 |  298 | `							return SXRET_OK;` |
|        - |  299 | `						}` |
|       42 |  300 | `					}` |
|       92 |  301 | `				}` |
|   330229 |  302 | `			}` |
|   660462 |  303 | `			pToken->nType = PH7_TK_RPAREN;` |
|   660462 |  304 | `			break;` |
|        - |  305 | `				  }` |
|    28815 |  306 | `		case '\'':{` |
|        - |  307 | `			/* Single quoted string */` |
|    57632 |  308 | `			pStr->zString++;` |
|   725150 |  309 | `			while( pStream->zText < pStream->zEnd ){` |
|   725150 |  310 | `				if( pStream->zText[0] == '\''  ){` |
|    57642 |  311 | `					if( pStream->zText[-1] != '\\' ){` |
|    57618 |  312 | `						break;` |
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
|   667520 |  325 | `				if( pStream->zText[0] == '\n' ){` |
|       67 |  326 | `					pStream->nLine++;` |
|       33 |  327 | `				}` |
|   667520 |  328 | `				pStream->zText++;` |
|        2 |  329 | `			}` |
|        - |  330 | `			/* Record token length and type */` |
|    57632 |  331 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|    57632 |  332 | `			pToken->nType = PH7_TK_SSTR;` |
|        - |  333 | `			/* Jump the trailing single quote */` |
|    57632 |  334 | `			pStream->zText++;` |
|    57632 |  335 | `			return SXRET_OK;` |
|        - |  336 | `				  }` |
|     7856 |  337 | `		case '"':{` |
|        - |  338 | `			sxi32 iNest;` |
|        - |  339 | `			/* Double quoted string */` |
|    15714 |  340 | `			pStr->zString++;` |
|   155970 |  341 | `			while( pStream->zText < pStream->zEnd ){` |
|   155970 |  342 | `				if( pStream->zText[0] == '{' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '$'){` |
|       79 |  343 | `					iNest = 1;` |
|       79 |  344 | `					pStream->zText++;` |
|        - |  345 | `					/* TICKET 1433-40: Hnadle braces'{}' in double quoted string where everything is allowed */` |
|      915 |  346 | `					while(pStream->zText < pStream->zEnd ){` |
|      915 |  347 | `						if( pStream->zText[0] == '{' ){` |
|        9 |  348 | `							iNest++;` |
|      911 |  349 | `						}else if (pStream->zText[0] == '}' ){` |
|       87 |  350 | `							iNest--;` |
|       87 |  351 | `							if( iNest <= 0 ){` |
|       79 |  352 | `								pStream->zText++;` |
|       79 |  353 | `								break;` |
|        1 |  354 | `							}` |
|      825 |  355 | `						}else if( pStream->zText[0] == '\n' ){` |
|      ! 0 |  356 | `							pStream->nLine++;` |
|      ! 0 |  357 | `						}` |
|      837 |  358 | `						pStream->zText++;` |
|        1 |  359 | `					}` |
|       79 |  360 | `					if( pStream->zText >= pStream->zEnd ){` |
|      ! 0 |  361 | `						break;` |
|        - |  362 | `					}` |
|       39 |  363 | `				}` |
|   155970 |  364 | `				if( pStream->zText[0] == '"' ){` |
|    15814 |  365 | `					if( pStream->zText[-1] != '\\' ){` |
|    15710 |  366 | `						break;` |
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
|   140258 |  379 | `				if( pStream->zText[0] == '\n' ){` |
|        7 |  380 | `					pStream->nLine++;` |
|        3 |  381 | `				}` |
|   140258 |  382 | `				pStream->zText++;` |
|        2 |  383 | `			}` |
|        - |  384 | `			/* Record token length and type */` |
|    15714 |  385 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|    15714 |  386 | `			pToken->nType = PH7_TK_DSTR;` |
|        - |  387 | `			/* Jump the trailing quote */` |
|    15714 |  388 | `			pStream->zText++;` |
|    15714 |  389 | `			return SXRET_OK;` |
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
|     1200 |  411 | `		case ':':` |
|     2402 |  412 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == ':' ){` |
|        - |  413 | `				/* Current operator: '::' */` |
|      210 |  414 | `				pStream->zText++;` |
|      106 |  415 | `			}else{` |
|     2194 |  416 | `				pToken->nType = PH7_TK_COLON; /* Single colon */` |
|        - |  417 | `			}` |
|     2402 |  418 | `			break;` |
|    72314 |  419 | `		case ',': pToken->nType \|= PH7_TK_COMMA;  break; /* Comma is also an operator */` |
|   484718 |  420 | `		case ';': pToken->nType = PH7_TK_SEMI;    break;` |
|        - |  421 | `			/* Handle combined operators [i.e: +=,===,!=== ...] */` |
|   140044 |  422 | `		case '=':` |
|   280090 |  423 | `			pToken->nType \|= PH7_TK_EQUAL;` |
|   280090 |  424 | `			if( pStream->zText < pStream->zEnd ){` |
|   280090 |  425 | `				if( pStream->zText[0] == '=' ){` |
|    17520 |  426 | `					pToken->nType &= ~PH7_TK_EQUAL;` |
|        - |  427 | `					/* Current operator: == */` |
|    17520 |  428 | `					pStream->zText++;` |
|    17520 |  429 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  430 | `						/* Current operator: === */` |
|     3868 |  431 | `						pStream->zText++;` |
|     1935 |  432 | `					}` |
|   271331 |  433 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  434 | `					/* Array operator: => */` |
|     4048 |  435 | `					pToken->nType = PH7_TK_ARRAY_OP;` |
|     4048 |  436 | `					pStream->zText++;` |
|     2025 |  437 | `				}else{` |
|        - |  438 | `					/* TICKET 1433-0010: Reference operator '=&' */` |
|   258526 |  439 | `					const unsigned char *zCur = pStream->zText;` |
|   258526 |  440 | `					sxu32 nLine = 0;` |
|   517028 |  441 | `					while( zCur < pStream->zEnd && zCur[0] < 0xc0 && SyisSpace(zCur[0]) ){` |
|   258504 |  442 | `						if( zCur[0] == '\n' ){` |
|        5 |  443 | `							nLine++;` |
|        2 |  444 | `						}` |
|   258504 |  445 | `						zCur++;` |
|        2 |  446 | `					}` |
|   258526 |  447 | `					if( zCur < pStream->zEnd && zCur[0] == '&' ){` |
|        - |  448 | `						/* Current operator: =& */` |
|       48 |  449 | `						pToken->nType &= ~PH7_TK_EQUAL;` |
|       48 |  450 | `						SyStringInitFromBuf(pStr,"=&",sizeof("=&")-1);` |
|        - |  451 | `						/* Update token stream */` |
|       48 |  452 | `						pStream->zText = &zCur[1];` |
|       48 |  453 | `						pStream->nLine += nLine;` |
|       23 |  454 | `					}` |
|        - |  455 | `				}` |
|   140044 |  456 | `			}` |
|   280090 |  457 | `			break;` |
|    19026 |  458 | `		case '!':` |
|    38054 |  459 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  460 | `				/* Current operator: != */` |
|    16192 |  461 | `				pStream->zText++;` |
|    16192 |  462 | `				if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  463 | `					/* Current operator: !== */` |
|    13492 |  464 | `					pStream->zText++;` |
|     6745 |  465 | `				}` |
|     8095 |  466 | `			}` |
|    38054 |  467 | `			break;` |
|    10937 |  468 | `		case '&':` |
|    21876 |  469 | `			pToken->nType \|= PH7_TK_AMPER;` |
|    21876 |  470 | `			if( pStream->zText < pStream->zEnd ){` |
|    21876 |  471 | `				if( pStream->zText[0] == '&' ){` |
|     8408 |  472 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|        - |  473 | `					/* Current operator: && */` |
|     8408 |  474 | `					pStream->zText++;` |
|    17673 |  475 | `				}else if( pStream->zText[0] == '=' ){` |
|        7 |  476 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|        - |  477 | `					/* Current operator: &= */` |
|        7 |  478 | `					pStream->zText++;` |
|        3 |  479 | `				}` |
|    10937 |  480 | `			}` |
|    21876 |  481 | `			break;` |
|     1422 |  482 | `		case '\|':` |
|     2846 |  483 | `			if( pStream->zText < pStream->zEnd ){` |
|     2846 |  484 | `				if( pStream->zText[0] == '\|' ){` |
|        - |  485 | `					/* Current operator: \|\| */` |
|     2802 |  486 | `					pStream->zText++;` |
|     1446 |  487 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  488 | `					/* Current operator: \|= */` |
|        7 |  489 | `					pStream->zText++;` |
|        3 |  490 | `				}` |
|     1422 |  491 | `			}` |
|     2846 |  492 | `			break;` |
|     7021 |  493 | `		case '+':` |
|    14044 |  494 | `			if( pStream->zText < pStream->zEnd ){` |
|    14042 |  495 | `				if( pStream->zText[0] == '+' ){` |
|        - |  496 | `					/* Current operator: ++ */` |
|    10916 |  497 | `					pStream->zText++;` |
|     8585 |  498 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  499 | `					/* Current operator: += */` |
|       42 |  500 | `					pStream->zText++;` |
|       20 |  501 | `				}` |
|     7020 |  502 | `			}` |
|    14044 |  503 | `			break;` |
|    51346 |  504 | `		case '-':` |
|   102694 |  505 | `			if( pStream->zText < pStream->zEnd ){` |
|   102694 |  506 | `				if( pStream->zText[0] == '-' ){` |
|        - |  507 | `					/* Current operator: -- */` |
|        5 |  508 | `					pStream->zText++;` |
|   102692 |  509 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  510 | `					/* Current operator: -= */` |
|        7 |  511 | `					pStream->zText++;` |
|   102687 |  512 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  513 | `					/* Current operator: -> */` |
|   102204 |  514 | `					pStream->zText++;` |
|    51101 |  515 | `				}` |
|    51346 |  516 | `			}` |
|   102694 |  517 | `			break;` |
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
|    29032 |  542 | `		case '.':` |
|    58066 |  543 | `			if( pStream->zText + 1 < pStream->zEnd && pStream->zText[0] == '.' && pStream->zText[1] == '.' ){` |
|        - |  544 | `				/* Ellipsis: ... */` |
|       42 |  545 | `				pStream->zText += 2;` |
|       42 |  546 | `				pToken->nType = PH7_TK_ELLIPSIS;` |
|    58046 |  547 | `			}else if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  548 | `				/* Current operator: .= */` |
|     2738 |  549 | `				pStream->zText++;` |
|     1368 |  550 | `			}` |
|    58066 |  551 | `			break;` |
|    22910 |  552 | `		case '<':` |
|    45822 |  553 | `			if( pStream->zText < pStream->zEnd ){` |
|    45822 |  554 | `				if( pStream->zText[0] == '<' ){` |
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
|    45755 |  572 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  573 | `					/* Current operator: <> */` |
|        5 |  574 | `					pStream->zText++;` |
|    45742 |  575 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  576 | `					/* Current operator: <= or <=> */` |
|       88 |  577 | `					pStream->zText++;` |
|       88 |  578 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '>' ){` |
|        - |  579 | `						/* Current operator: <=> */` |
|       51 |  580 | `						pStream->zText++;` |
|       25 |  581 | `					}` |
|       43 |  582 | `				}` |
|    22882 |  583 | `			}` |
|    45766 |  584 | `			break;` |
|     2775 |  585 | `		case '>':` |
|     5552 |  586 | `			if( pStream->zText < pStream->zEnd ){` |
|     5552 |  587 | `				if( pStream->zText[0] == '>' ){` |
|        - |  588 | `					/* Current operator: >> */` |
|       21 |  589 | `					pStream->zText++;` |
|       21 |  590 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  591 | `						/* Current operator: >>= */` |
|       11 |  592 | `						pStream->zText++;` |
|        6 |  593 | `					}` |
|     5542 |  594 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  595 | `					/* Current operator: >= */` |
|       80 |  596 | `					pStream->zText++;` |
|       39 |  597 | `				}` |
|     2775 |  598 | `			}` |
|     5552 |  599 | `			break;` |
|     1001 |  600 | `		case '?':` |
|     2004 |  601 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '?' ){` |
|        - |  602 | `				/* Null coalescing operator: ?? */` |
|       84 |  603 | `				pStream->zText++;` |
|       84 |  604 | `				if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  605 | `					/* Null coalescing assignment operator (PHP 7.4) */` |
|       54 |  606 | `					pStream->zText++;` |
|       26 |  607 | `				}` |
|       41 |  608 | `			}` |
|     2002 |  609 | `			break;` |
|      105 |  610 | `		default:` |
|      210 |  611 | `			break;` |
|        - |  612 | `		}` |
|  4174074 |  613 | `		if( pStr->nByte <= 0 ){` |
|        - |  614 | `			/* Record token length */` |
|  4174028 |  615 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  2087013 |  616 | `		}` |
|  4174074 |  617 | `		if( pToken->nType & PH7_TK_OP ){` |
|        - |  618 | `			const ph7_expr_op *pOp;` |
|        - |  619 | `			/* Check if the extracted token is an operator */` |
|   708766 |  620 | `			pOp = PH7_ExprExtractOperator(pStr,(SyToken *)SySetPeek(pStream->pSet));` |
|   708766 |  621 | `			if( pOp == 0 ){` |
|        - |  622 | `				/* Not an operator */` |
|      ! 0 |  623 | `				pToken->nType &= ~PH7_TK_OP;` |
|      ! 0 |  624 | `				if( pToken->nType <= 0 ){` |
|      ! 0 |  625 | `					pToken->nType = PH7_TK_OTHER;` |
|      ! 0 |  626 | `				}` |
|      ! 0 |  627 | `			}else{` |
|        - |  628 | `				/* Save the instance associated with this operator for later processing */` |
|   708766 |  629 | `				pToken->pUserData = (void *)pOp;` |
|        - |  630 | `			}` |
|   354382 |  631 | `		}` |
|        - |  632 | `	}` |
|        - |  633 | `	/* Tell the upper-layer to save the extracted token for later processing */` |
|  6672682 |  634 | `	return SXRET_OK;` |
|  3458084 |  635 |  |
|        - |  636 | `/***** This file contains automatically generated code ******` |
|        - |  637 | `**` |
|        - |  638 | `** The code in this file has been automatically generated by` |
|        - |  639 | `**` |
|        - |  640 | `**     $Header: /sqlite/sqlite/tool/mkkeywordhash.c` |
|        - |  641 | `**` |
|        - |  642 | `** Sligthly modified by Chems mrad <chm@symisc.net> for the PH7 engine.` |
|        - |  643 | `**` |
|        - |  644 | `** The code in this file implements a function that determines whether` |
|        - |  645 | `** or not a given identifier is really a PHP keyword.  The same thing` |
|        - |  646 | `** might be implemented more directly using a hand-written hash table.` |
|        - |  647 | `** But by using this automatically generated code, the size of the code` |
|        - |  648 | `** is substantially reduced.  This is important for embedded applications` |
|        - |  649 | `** on platforms with limited memory.` |
|        - |  650 | `*/` |
|        - |  651 | `/* Hash score: 103 */` |
|  2498610 |  652 | `static sxu32 KeywordCode(const char *z, int n){` |
|        - |  653 | `  /* zText[] encodes 532 bytes of keywords in 333 bytes */` |
|        - |  654 | `  /*   extendswitchprintegerequire_oncenddeclareturnamespacechobject      */` |
|        - |  655 | `  /*   hrowbooleandefaultrycaselfinalistaticlonewconstringlobaluse        */` |
|        - |  656 | `  /*   lseifloatvarrayANDIEchoUSECHOabstractclasscontinuendifunction      */` |
|        - |  657 | `  /*   diendwhilevaldoexitgotoimplementsinclude_oncemptyinstanceof        */` |
|        - |  658 | `  /*   interfacendforeachissetparentprivateprotectedpublicatchunset       */` |
|        - |  659 | `  /*   xorARRAYASArrayEXITUNSETXORbreak                                   */` |
|        - |  660 | `  static const char zText[332] = {` |
|        - |  661 | `    'e','x','t','e','n','d','s','w','i','t','c','h','p','r','i','n','t','e',` |
|        - |  662 | `    'g','e','r','e','q','u','i','r','e','_','o','n','c','e','n','d','d','e',` |
|        - |  663 | `    'c','l','a','r','e','t','u','r','n','a','m','e','s','p','a','c','e','c',` |
|        - |  664 | `    'h','o','b','j','e','c','t','h','r','o','w','b','o','o','l','e','a','n',` |
|        - |  665 | `    'd','e','f','a','u','l','t','r','y','c','a','s','e','l','f','i','n','a',` |
|        - |  666 | `    'l','i','s','t','a','t','i','c','l','o','n','e','w','c','o','n','s','t',` |
|        - |  667 | `    'r','i','n','g','l','o','b','a','l','u','s','e','l','s','e','i','f','l',` |
|        - |  668 | `    'o','a','t','v','a','r','r','a','y','A','N','D','I','E','c','h','o','U',` |
|        - |  669 | `    'S','E','C','H','O','a','b','s','t','r','a','c','t','c','l','a','s','s',` |
|        - |  670 | `    'c','o','n','t','i','n','u','e','n','d','i','f','u','n','c','t','i','o',` |
|        - |  671 | `    'n','d','i','e','n','d','w','h','i','l','e','v','a','l','d','o','e','x',` |
|        - |  672 | `    'i','t','g','o','t','o','i','m','p','l','e','m','e','n','t','s','i','n',` |
|        - |  673 | `    'c','l','u','d','e','_','o','n','c','e','m','p','t','y','i','n','s','t',` |
|        - |  674 | `    'a','n','c','e','o','f','i','n','t','e','r','f','a','c','e','n','d','f',` |
|        - |  675 | `    'o','r','e','a','c','h','i','s','s','e','t','p','a','r','e','n','t','p',` |
|        - |  676 | `    'r','i','v','a','t','e','p','r','o','t','e','c','t','e','d','p','u','b',` |
|        - |  677 | `    'l','i','c','a','t','c','h','u','n','s','e','t','x','o','r','A','R','R',` |
|        - |  678 | `    'A','Y','A','S','A','r','r','a','y','E','X','I','T','U','N','S','E','T',` |
|        - |  679 | `    'X','O','R','b','r','e','a','k'` |
|        - |  680 | `  };` |
|        - |  681 | `  static const unsigned char aHash[151] = {` |
|        - |  682 |  |
|        - |  683 |  |
|        - |  684 | `      74,   0,  51,   0,  76,   0,   0,  20,   0,   0,   0,  50,   0,` |
|        - |  685 | `      80,  34,   0,  36,   0,   0,  64,  16,   0,   0,  17,   0,   1,` |
|        - |  686 | `      19,  84,  66,   0,  43,  45,  78,   0,   0,  53,  56,   0,   0,` |
|        - |  687 |  |
|        - |  688 | `      72,  14,   0,  71,   0,  38,   6,   0,   0,   0,  73,   0,   0,` |
|        - |  689 | `       3,   0,  41,   5,  52,  57,  32,   0,  60,  63,   0,  69,  82,` |
|        - |  690 | `      30,   0,  79,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,` |
|        - |  691 |  |
|        - |  692 | `      62,   0,  11,   0,   0,  58,   0,   0,   0,   0,  59,  75,   0,` |
|        - |  693 |  |
|        - |  694 | `  };` |
|        - |  695 | `  static const unsigned char aNext[84] = {` |
|        - |  696 |  |
|        - |  697 |  |
|        - |  698 |  |
|        - |  699 |  |
|        - |  700 |  |
|        - |  701 |  |
|        - |  702 | `      42,   0,   0,   0,  70,  55` |
|        - |  703 | `  };` |
|        - |  704 | `  static const unsigned char aLen[84] = {` |
|        - |  705 | `       7,   9,   6,   5,   7,  12,   7,   2,  10,   7,   6,   9,   4,` |
|        - |  706 | `       6,   5,   7,   4,   3,   7,   3,   4,   4,   5,   4,   6,   5,` |
|        - |  707 | `       2,   3,   5,   6,   6,   3,   6,   4,   2,   5,   3,   5,   3,` |
|        - |  708 | `       3,   4,   3,   4,   8,   5,   2,   8,   5,   8,   3,   8,   5,` |
|        - |  709 | `       4,   2,   4,   4,  10,  12,   7,   5,  10,   9,   3,   6,  10,` |
|        - |  710 | `       3,   7,   2,   5,   6,   7,   9,   6,   5,   5,   3,   5,   2,` |
|        - |  711 | `       5,   4,   5,   3,   2,   5` |
|        - |  712 | `  };` |
|        - |  713 | `  static const sxu16 aOffset[84] = {` |
|        - |  714 |  |
|        - |  715 | `      55,  60,  65,  65,  70,  72,  78,  81,  83,  86,  90,  92,  97,` |
|        - |  716 | `     100, 100, 103, 106, 111, 117, 119, 119, 123, 124, 129, 130, 135,` |
|        - |  717 | `     137, 139, 143, 145, 149, 157, 159, 162, 169, 173, 181, 183, 186,` |
|        - |  718 | `     190, 194, 196, 200, 204, 214, 214, 225, 230, 240, 240, 248, 248,` |
|        - |  719 | `     251, 251, 252, 258, 263, 269, 276, 285, 290, 295, 300, 303, 308,` |
|        - |  720 | `     310, 315, 319, 324, 325, 327` |
|        - |  721 | `  };` |
|        - |  722 | `  static const sxu32 aCode[84] = {` |
|        - |  723 | `    PH7_TKWRD_EXTENDS,   PH7_TKWRD_ENDSWITCH,   PH7_TKWRD_SWITCH,    PH7_TKWRD_PRINT,   PH7_TKWRD_INT,` |
|        - |  724 | `    PH7_TKWRD_REQONCE,   PH7_TKWRD_REQUIRE,     PH7_TKWRD_SEQ,       PH7_TKWRD_ENDDEC,    PH7_TKWRD_DECLARE,` |
|        - |  725 | `    PH7_TKWRD_RETURN,    PH7_TKWRD_NAMESPACE,   PH7_TKWRD_ECHO,      PH7_TKWRD_OBJECT,    PH7_TKWRD_THROW,` |
|        - |  726 | `    PH7_TKWRD_BOOL,      PH7_TKWRD_BOOL,        PH7_TKWRD_AND,       PH7_TKWRD_DEFAULT,   PH7_TKWRD_TRY,` |
|        - |  727 | `    PH7_TKWRD_CASE,      PH7_TKWRD_SELF,        PH7_TKWRD_FINAL,     PH7_TKWRD_LIST,      PH7_TKWRD_STATIC,` |
|        - |  728 | `    PH7_TKWRD_CLONE,     PH7_TKWRD_SNE,         PH7_TKWRD_NEW,       PH7_TKWRD_CONST,     PH7_TKWRD_STRING,` |
|        - |  729 | `    PH7_TKWRD_GLOBAL,    PH7_TKWRD_USE,         PH7_TKWRD_ELIF,      PH7_TKWRD_ELSE,      PH7_TKWRD_IF,` |
|        - |  730 | `    PH7_TKWRD_FLOAT,     PH7_TKWRD_VAR,         PH7_TKWRD_ARRAY,     PH7_TKWRD_AND,       PH7_TKWRD_DIE,` |
|        - |  731 | `    PH7_TKWRD_ECHO,      PH7_TKWRD_USE,         PH7_TKWRD_ECHO,      PH7_TKWRD_ABSTRACT,  PH7_TKWRD_CLASS,` |
|        - |  732 | `    PH7_TKWRD_AS,        PH7_TKWRD_CONTINUE,    PH7_TKWRD_ENDIF,     PH7_TKWRD_FUNCTION,  PH7_TKWRD_DIE,` |
|        - |  733 | `    PH7_TKWRD_ENDWHILE,  PH7_TKWRD_WHILE,       PH7_TKWRD_EVAL,      PH7_TKWRD_DO,        PH7_TKWRD_EXIT,` |
|        - |  734 | `    PH7_TKWRD_GOTO,      PH7_TKWRD_IMPLEMENTS,  PH7_TKWRD_INCONCE,   PH7_TKWRD_INCLUDE,   PH7_TKWRD_EMPTY,` |
|        - |  735 | `    PH7_TKWRD_INSTANCEOF,PH7_TKWRD_INTERFACE,   PH7_TKWRD_INT,       PH7_TKWRD_ENDFOR,    PH7_TKWRD_END4EACH,` |
|        - |  736 | `    PH7_TKWRD_FOR,       PH7_TKWRD_FOREACH,     PH7_TKWRD_OR,        PH7_TKWRD_ISSET,     PH7_TKWRD_PARENT,` |
|        - |  737 | `    PH7_TKWRD_PRIVATE,   PH7_TKWRD_PROTECTED,   PH7_TKWRD_PUBLIC,    PH7_TKWRD_CATCH,     PH7_TKWRD_UNSET,` |
|        - |  738 | `    PH7_TKWRD_XOR,       PH7_TKWRD_ARRAY,       PH7_TKWRD_AS,        PH7_TKWRD_ARRAY,     PH7_TKWRD_EXIT,` |
|        - |  739 | `    PH7_TKWRD_UNSET,     PH7_TKWRD_XOR,         PH7_TKWRD_OR,        PH7_TKWRD_BREAK` |
|        - |  740 | `  };` |
|        - |  741 | `  int h, i;` |
|  2498610 |  742 | `  if( n<2 ) return PH7_TK_ID;` |
|  2408314 |  743 | `  h = (((int)z[0]*4) ^ ((int)z[n-1]*3) ^ n) % 151;` |
|  3686620 |  744 | `  for(i=((int)aHash[h])-1; i>=0; i=((int)aNext[i])-1){` |
|  2132058 |  745 | `    if( (int)aLen[i]==n && SyMemcmp(&zText[aOffset[i]],z,n)==0 ){` |
|        - |  746 | `       /* PH7_TKWRD_EXTENDS */` |
|        - |  747 | `       /* PH7_TKWRD_ENDSWITCH */` |
|        - |  748 | `       /* PH7_TKWRD_SWITCH */` |
|        - |  749 | `       /* PH7_TKWRD_PRINT */` |
|        - |  750 | `       /* PH7_TKWRD_INT */` |
|        - |  751 | `       /* PH7_TKWRD_REQONCE */` |
|        - |  752 | `       /* PH7_TKWRD_REQUIRE */` |
|        - |  753 | `       /* PH7_TKWRD_SEQ */` |
|        - |  754 | `       /* PH7_TKWRD_ENDDEC */` |
|        - |  755 | `       /* PH7_TKWRD_DECLARE */` |
|        - |  756 | `       /* PH7_TKWRD_RETURN */` |
|        - |  757 | `       /* PH7_TKWRD_NAMESPACE */` |
|        - |  758 | `       /* PH7_TKWRD_ECHO */` |
|        - |  759 | `       /* PH7_TKWRD_OBJECT */` |
|        - |  760 | `       /* PH7_TKWRD_THROW */` |
|        - |  761 | `       /* PH7_TKWRD_BOOL */` |
|        - |  762 | `       /* PH7_TKWRD_BOOL */` |
|        - |  763 | `       /* PH7_TKWRD_AND */` |
|        - |  764 | `       /* PH7_TKWRD_DEFAULT */` |
|        - |  765 | `       /* PH7_TKWRD_TRY */` |
|        - |  766 | `       /* PH7_TKWRD_CASE */` |
|        - |  767 | `       /* PH7_TKWRD_SELF */` |
|        - |  768 | `       /* PH7_TKWRD_FINAL */` |
|        - |  769 | `       /* PH7_TKWRD_LIST */` |
|        - |  770 | `       /* PH7_TKWRD_STATIC */` |
|        - |  771 | `       /* PH7_TKWRD_CLONE */` |
|        - |  772 | `       /* PH7_TKWRD_SNE */` |
|        - |  773 | `       /* PH7_TKWRD_NEW */` |
|        - |  774 | `       /* PH7_TKWRD_CONST */` |
|        - |  775 | `       /* PH7_TKWRD_STRING */` |
|        - |  776 | `       /* PH7_TKWRD_GLOBAL */` |
|        - |  777 | `       /* PH7_TKWRD_USE */` |
|        - |  778 | `       /* PH7_TKWRD_ELIF */` |
|        - |  779 | `       /* PH7_TKWRD_ELSE */` |
|        - |  780 | `       /* PH7_TKWRD_IF */` |
|        - |  781 | `       /* PH7_TKWRD_FLOAT */` |
|        - |  782 | `       /* PH7_TKWRD_VAR */` |
|        - |  783 | `       /* PH7_TKWRD_ARRAY */` |
|        - |  784 | `       /* PH7_TKWRD_AND */` |
|        - |  785 | `       /* PH7_TKWRD_DIE */` |
|        - |  786 | `       /* PH7_TKWRD_ECHO */` |
|        - |  787 | `       /* PH7_TKWRD_USE */` |
|        - |  788 | `       /* PH7_TKWRD_ECHO */` |
|        - |  789 | `       /* PH7_TKWRD_ABSTRACT */` |
|        - |  790 | `       /* PH7_TKWRD_CLASS */` |
|        - |  791 | `       /* PH7_TKWRD_AS */` |
|        - |  792 | `       /* PH7_TKWRD_CONTINUE */` |
|        - |  793 | `       /* PH7_TKWRD_ENDIF */` |
|        - |  794 | `       /* PH7_TKWRD_FUNCTION */` |
|        - |  795 | `       /* PH7_TKWRD_DIE */` |
|        - |  796 | `       /* PH7_TKWRD_ENDWHILE */` |
|        - |  797 | `       /* PH7_TKWRD_WHILE */` |
|        - |  798 | `       /* PH7_TKWRD_EVAL */` |
|        - |  799 | `       /* PH7_TKWRD_DO */` |
|        - |  800 | `       /* PH7_TKWRD_EXIT */` |
|        - |  801 | `       /* PH7_TKWRD_GOTO */` |
|        - |  802 | `       /* PH7_TKWRD_IMPLEMENTS */` |
|        - |  803 | `       /* PH7_TKWRD_INCONCE */` |
|        - |  804 | `       /* PH7_TKWRD_INCLUDE */` |
|        - |  805 | `       /* PH7_TKWRD_EMPTY */` |
|        - |  806 | `       /* PH7_TKWRD_INSTANCEOF */` |
|        - |  807 | `       /* PH7_TKWRD_INTERFACE */` |
|        - |  808 | `       /* PH7_TKWRD_INT */` |
|        - |  809 | `       /* PH7_TKWRD_ENDFOR */` |
|        - |  810 | `       /* PH7_TKWRD_END4EACH */` |
|        - |  811 | `       /* PH7_TKWRD_FOR */` |
|        - |  812 | `       /* PH7_TKWRD_FOREACH */` |
|        - |  813 | `       /* PH7_TKWRD_OR */` |
|        - |  814 | `       /* PH7_TKWRD_ISSET */` |
|        - |  815 | `       /* PH7_TKWRD_PARENT */` |
|        - |  816 | `       /* PH7_TKWRD_PRIVATE */` |
|        - |  817 | `       /* PH7_TKWRD_PROTECTED */` |
|        - |  818 | `       /* PH7_TKWRD_PUBLIC */` |
|        - |  819 | `       /* PH7_TKWRD_CATCH */` |
|        - |  820 | `       /* PH7_TKWRD_UNSET */` |
|        - |  821 | `       /* PH7_TKWRD_XOR */` |
|        - |  822 | `       /* PH7_TKWRD_ARRAY */` |
|        - |  823 | `       /* PH7_TKWRD_AS */` |
|        - |  824 | `       /* PH7_TKWRD_ARRAY */` |
|        - |  825 | `       /* PH7_TKWRD_EXIT */` |
|        - |  826 | `       /* PH7_TKWRD_UNSET */` |
|        - |  827 | `       /* PH7_TKWRD_XOR */` |
|        - |  828 | `       /* PH7_TKWRD_OR */` |
|        - |  829 | `       /* PH7_TKWRD_BREAK */` |
|   853752 |  830 | `      return aCode[i];` |
|        - |  831 | `    }` |
|   639153 |  832 | `  }` |
|        - |  833 | `  /* Linear fallback for keywords not in the auto-generated hash table */` |
|  1554564 |  834 | `  if( n==5 && SyMemcmp(z,"trait",5)==0 ) return PH7_TKWRD_TRAIT;` |
|  1554510 |  835 | `  if( n==9 && SyMemcmp(z,"insteadof",9)==0 ) return PH7_TKWRD_INSTEADOF;` |
|  1554506 |  836 | `  if( n==7 && SyMemcmp(z,"finally",7)==0 ) return PH7_TKWRD_FINALLY;` |
|  1554476 |  837 | `  if( n==5 && SyMemcmp(z,"yield",5)==0 ) return PH7_TKWRD_YIELD;` |
|  1554444 |  838 | `  return PH7_TK_ID;` |
|  1249306 |  839 |  |
|        - |  840 | `/* --- End of Automatically generated code --- */` |
|        - |  841 | `/*` |
|        - |  842 | ` * Extract a heredoc/nowdoc text from a raw PHP input.` |
|        - |  843 | ` * According to the PHP language reference manual:` |
|        - |  844 | ` *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier` |
|        - |  845 | ` *  is provided, then a newline. The string itself follows, and then the same identifier again` |
|        - |  846 | ` *  to close the quotation.` |
|        - |  847 | ` *  The closing identifier must begin in the first column of the line. Also, the identifier must` |
|        - |  848 | ` *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric` |
|        - |  849 | ` *  characters and underscores, and must start with a non-digit character or underscore.` |
|        - |  850 | ` *  Heredoc text behaves just like a double-quoted string, without the double quotes.` |
|        - |  851 | ` *  This means that quotes in a heredoc do not need to be escaped, but the escape codes listed` |
|        - |  852 | ` *  above can still be used. Variables are expanded, but the same care must be taken when expressing` |
|        - |  853 | ` *  complex variables inside a heredoc as with strings.` |
|        - |  854 | ` *  Nowdocs are to single-quoted strings what heredocs are to double-quoted strings.` |
|        - |  855 | ` *  A nowdoc is specified similarly to a heredoc, but no parsing is done inside a nowdoc.` |
|        - |  856 | ` *  The construct is ideal for embedding PHP code or other large blocks of text without the need` |
|        - |  857 | ` *  for escaping. It shares some features in common with the SGML <![CDATA[ ]]> construct, in that` |
|        - |  858 | ` *  it declares a block of text which is not for parsing.` |
|        - |  859 | ` *  A nowdoc is identified with the same <<< sequence used for heredocs, but the identifier which follows` |
|        - |  860 | ` *  is enclosed in single quotes, e.g. <<<'EOT'. All the rules for heredoc identifiers also apply to nowdoc` |
|        - |  861 | ` *  identifiers, especially those regarding the appearance of the closing identifier.` |
|        - |  862 | ` * Symisc Extension:` |
|        - |  863 | ` * The closing delimiter can now start with a digit or undersocre or it can be an UTF-8 stream.` |
|        - |  864 | ` * Example:` |
|        - |  865 | ` *  <<<123` |
|        - |  866 | ` *    HEREDOC Here` |
|        - |  867 | ` * 123` |
|        - |  868 | ` *  or` |
|        - |  869 | ` *  <<<___` |
|        - |  870 | ` *   HEREDOC Here` |
|        - |  871 | ` *  ___` |
|        - |  872 | ` */` |
|       56 |  873 | `static sxi32 LexExtractHeredoc(SyStream *pStream,SyToken *pToken)` |
|        2 |  874 |  |
|       58 |  875 | `	const unsigned char *zIn  = pStream->zText;` |
|       58 |  876 | `	const unsigned char *zEnd = pStream->zEnd;` |
|        - |  877 | `	const unsigned char *zPtr;` |
|       58 |  878 | `	sxu8 bNowDoc = FALSE;` |
|        - |  879 | `	SyString sDelim;` |
|        - |  880 | `	SyString sStr;` |
|        - |  881 | `	/* Jump leading white spaces */` |
|       70 |  882 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|       13 |  883 | `		zIn++;` |
|        1 |  884 | `	}` |
|       58 |  885 | `	if( zIn >= zEnd ){` |
|        - |  886 | `		/* A simple symbol,return immediately */` |
|      ! 0 |  887 | `		return SXERR_CONTINUE;` |
|        - |  888 | `	}` |
|       58 |  889 | `	if( zIn[0] == '\'' \|\| zIn[0] == '"' ){` |
|        - |  890 | `		/* Make sure we are dealing with a nowdoc */` |
|       29 |  891 | `		bNowDoc =  zIn[0] == '\'' ? TRUE : FALSE;` |
|       29 |  892 | `		zIn++;` |
|       14 |  893 | `	}` |
|       58 |  894 | `	if( zIn[0] < 0xc0 && !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|        - |  895 | `		/* Invalid delimiter,return immediately */` |
|      ! 0 |  896 | `		return SXERR_CONTINUE;` |
|        - |  897 | `	}` |
|        - |  898 | `	/* Isolate the identifier */` |
|       58 |  899 | `	sDelim.zString = (const char *)zIn;` |
|       64 |  900 | `	for(;;){` |
|      130 |  901 | `		zPtr = zIn;` |
|        - |  902 | `		/* Skip alphanumeric stream */` |
|      424 |  903 | `		while( zPtr < zEnd && zPtr[0] < 0xc0 && (SyisAlphaNum(zPtr[0]) \|\| zPtr[0] == '_') ){` |
|      232 |  904 | `			zPtr++;` |
|        2 |  905 | `		}` |
|      130 |  906 | `		if( zPtr < zEnd && zPtr[0] >= 0xc0 ){` |
|       19 |  907 | `			zPtr++;` |
|        - |  908 | `			/* UTF-8 stream */` |
|       37 |  909 | `			while( zPtr < zEnd && ((zPtr[0] & 0xc0) == 0x80) ){` |
|       19 |  910 | `				zPtr++;` |
|        1 |  911 | `			}` |
|        9 |  912 | `		}` |
|      130 |  913 | `		if( zPtr == zIn ){` |
|        - |  914 | `			/* Not an UTF-8 or alphanumeric stream */` |
|       58 |  915 | `			break;` |
|        - |  916 | `		}` |
|        - |  917 | `		/* Synchronize pointers */` |
|       74 |  918 | `		zIn = zPtr;` |
|        2 |  919 | `	}` |
|        - |  920 | `	/* Get the identifier length */` |
|       58 |  921 | `	sDelim.nByte = (sxu32)((const char *)zIn-sDelim.zString);` |
|       58 |  922 | `	if( zIn[0] == '"' \|\| (bNowDoc && zIn[0] == '\'') ){` |
|        - |  923 | `		/* Jump the trailing single quote */` |
|       29 |  924 | `		zIn++;` |
|       14 |  925 | `	}` |
|        - |  926 | `	/* Jump trailing white spaces */` |
|       58 |  927 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|      ! 0 |  928 | `		zIn++;` |
|      ! 0 |  929 | `	}` |
|       58 |  930 | `	if( sDelim.nByte <= 0 \|\| zIn >= zEnd \|\| zIn[0] != '\n' ){` |
|        - |  931 | `		/* Invalid syntax */` |
|      ! 0 |  932 | `		return SXERR_CONTINUE;` |
|        - |  933 | `	}` |
|       58 |  934 | `	pStream->nLine++; /* Increment line counter */` |
|       58 |  935 | `	zIn++;` |
|        - |  936 | `	/* Isolate the delimited string */` |
|       58 |  937 | `	sStr.zString = (const char *)zIn;` |
|        - |  938 | `	/* Go and found the closing delimiter */` |
|       75 |  939 | `	for(;;){` |
|        - |  940 | `		/* Synchronize with the next line */` |
|     3018 |  941 | `		while( zIn < zEnd && zIn[0] != '\n' ){` |
|     2868 |  942 | `			zIn++;` |
|        2 |  943 | `		}` |
|      152 |  944 | `		if( zIn >= zEnd ){` |
|        - |  945 | `			/* End of the input reached, break immediately */` |
|       12 |  946 | `			pStream->zText = pStream->zEnd;` |
|       12 |  947 | `			break;` |
|        - |  948 | `		}` |
|      142 |  949 | `		pStream->nLine++; /* Increment line counter */` |
|      142 |  950 | `		zIn++;` |
|      142 |  951 | `		if( (sxu32)(zEnd - zIn) >= sDelim.nByte && SyMemcmp((const void *)sDelim.zString,(const void *)zIn,sDelim.nByte) == 0 ){` |
|       50 |  952 | `			zPtr = &zIn[sDelim.nByte];` |
|       62 |  953 | `			while( zPtr < zEnd && zPtr[0] < 0xc0 && SyisSpace(zPtr[0]) && zPtr[0] != '\n' ){` |
|       13 |  954 | `				zPtr++;` |
|        1 |  955 | `			}` |
|       50 |  956 | `			if( zPtr >= zEnd ){` |
|        - |  957 | `				/* End of input */` |
|      ! 0 |  958 | `				pStream->zText = zPtr;` |
|      ! 0 |  959 | `				break;` |
|        - |  960 | `			}` |
|       50 |  961 | `			if( zPtr[0] == ';' ){` |
|       50 |  962 | `				const unsigned char *zCur = zPtr;` |
|       50 |  963 | `				zPtr++;` |
|       52 |  964 | `				while( zPtr < zEnd && zPtr[0] < 0xc0 && SyisSpace(zPtr[0]) && zPtr[0] != '\n' ){` |
|        3 |  965 | `					zPtr++;` |
|        1 |  966 | `				}` |
|       50 |  967 | `				if( zPtr >= zEnd \|\| zPtr[0] == '\n' ){` |
|        - |  968 | `					/* Closing delimiter found,break immediately */` |
|       48 |  969 | `					pStream->zText = zCur; /* Keep the semi-colon */` |
|       48 |  970 | `					break;` |
|        1 |  971 | `				}` |
|        1 |  972 | `			}else if( zPtr[0] == '\n' ){` |
|        - |  973 | `				/* Closing delimiter found,break immediately */` |
|      ! 0 |  974 | `				pStream->zText = zPtr; /* Synchronize with the stream cursor */` |
|      ! 0 |  975 | `				break;` |
|        - |  976 | `			}` |
|        - |  977 | `			/* Synchronize pointers and continue searching */` |
|        3 |  978 | `			zIn = zPtr;` |
|        1 |  979 | `		}` |
|        2 |  980 | `	} /* For(;;) */` |
|        - |  981 | `	/* Get the delimited string length */` |
|       58 |  982 | `	sStr.nByte = (sxu32)((const char *)zIn-sStr.zString);` |
|        - |  983 | `	/* Record token type and length */` |
|       58 |  984 | `	pToken->nType = bNowDoc ? PH7_TK_NOWDOC : PH7_TK_HEREDOC;` |
|       58 |  985 | `	SyStringDupPtr(&pToken->sData,&sStr);` |
|        - |  986 | `	/* Remove trailing white spaces */` |
|      104 |  987 | `	SyStringRightTrim(&pToken->sData);` |
|        - |  988 | `	/* All done */` |
|       58 |  989 | `	return SXRET_OK;` |
|       30 |  990 |  |
|        - |  991 | `/*` |
|        - |  992 | ` * Tokenize a raw PHP input.` |
|        - |  993 | ` * This is the public tokenizer called by most code generator routines.` |
|        - |  994 | ` */` |
|    13272 |  995 | `PH7_PRIVATE sxi32 PH7_TokenizePHP(const char *zInput,sxu32 nLen,sxu32 nLineStart,SySet *pOut)` |
|        2 |  996 |  |
|        - |  997 | `	SyLex sLexer;` |
|        - |  998 | `	sxi32 rc;` |
|        - |  999 | `	/* Initialize the lexer */` |
|    13274 | 1000 | `	rc = SyLexInit(&sLexer,&(*pOut),TokenizePHP,0);` |
|    13274 | 1001 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1002 | `		return rc;` |
|        - | 1003 | `	}` |
|    13274 | 1004 | `	sLexer.sStream.nLine = nLineStart;` |
|        - | 1005 | `	/* Tokenize input */` |
|    13274 | 1006 | `	rc = SyLexTokenizeInput(&sLexer,zInput,nLen,0,0,0);` |
|        - | 1007 | `	/* Release the lexer */` |
|    13274 | 1008 | `	SyLexRelease(&sLexer);` |
|        - | 1009 | `	/* Tokenization result */` |
|    13274 | 1010 | `	return rc;` |
|     6638 | 1011 |  |
|        - | 1012 | `/*` |
|        - | 1013 | ` * High level public tokenizer.` |
|        - | 1014 | ` *  Tokenize the input into PHP tokens and raw tokens [i.e: HTML,XML,Raw text...].` |
|        - | 1015 | ` * According to the PHP language reference manual` |
|        - | 1016 | ` *   When PHP parses a file, it looks for opening and closing tags, which tell PHP` |
|        - | 1017 | ` *   to start and stop interpreting the code between them. Parsing in this manner allows` |
|        - | 1018 | ` *   PHP to be embedded in all sorts of different documents, as everything outside of a pair` |
|        - | 1019 | ` *   of opening and closing tags is ignored by the PHP parser. Most of the time you will see` |
|        - | 1020 | ` *   PHP embedded in HTML documents, as in this example.` |
|        - | 1021 | ` *   <?php echo 'While this is going to be parsed.'; ?>` |
|        - | 1022 | ` *   <p>This will also be ignored.</p>` |
|        - | 1023 | ` *   You can also use more advanced structures:` |
|        - | 1024 | ` *   Example #1 Advanced escaping` |
|        - | 1025 | ` * <?php` |
|        - | 1026 | ` * if ($expression) {` |
|        - | 1027 | ` *   ?>` |
|        - | 1028 | ` *   <strong>This is true.</strong>` |
|        - | 1029 | ` *   <?php` |
|        - | 1030 | ` * } else {` |
|        - | 1031 | ` *   ?>` |
|        - | 1032 | ` *   <strong>This is false.</strong>` |
|        - | 1033 | ` *   <?php` |
|        - | 1034 | ` * }` |
|        - | 1035 | ` * ?>` |
|        - | 1036 | ` * This works as expected, because when PHP hits the ?> closing tags, it simply starts outputting` |
|        - | 1037 | ` * whatever it finds (except for an immediately following newline - see instruction separation ) until it hits` |
|        - | 1038 | ` * another opening tag. The example given here is contrived, of course, but for outputting large blocks of text` |
|        - | 1039 | ` * dropping out of PHP parsing mode is generally more efficient than sending all of the text through echo() or print().` |
|        - | 1040 | ` * There are four different pairs of opening and closing tags which can be used in PHP. Three of those, <?php ?>` |
|        - | 1041 | ` * <script language="php"> </script>  and <? ?> are always available. The other two are short tags and ASP style` |
|        - | 1042 | ` * tags, and can be turned on and off from the php.ini configuration file. As such, while some people find short tags` |
|        - | 1043 | ` * and ASP style tags convenient, they are less portable, and generally not recommended.` |
|        - | 1044 | ` * Note:` |
|        - | 1045 | ` * Also note that if you are embedding PHP within XML or XHTML you will need to use the <?php ?> tags to remain` |
|        - | 1046 | ` * compliant with standards.` |
|        - | 1047 | ` * Example #2 PHP Opening and Closing Tags` |
|        - | 1048 | ` * 1.  <?php echo 'if you want to serve XHTML or XML documents, do it like this'; ?>` |
|        - | 1049 | ` * 2.  <script language="php">` |
|        - | 1050 | ` *       echo 'some editors (like FrontPage) don\'t` |
|        - | 1051 | ` *             like processing instructions';` |
|        - | 1052 | ` *   </script>` |
|        - | 1053 | ` *` |
|        - | 1054 | ` * 3.  <? echo 'this is the simplest, an SGML processing instruction'; ?>` |
|        - | 1055 | ` *   <?= expression ?> This is a shortcut for "<? echo expression ?>"` |
|        - | 1056 | ` */` |
|    10942 | 1057 | `PH7_PRIVATE sxi32 PH7_TokenizeRawText(const char *zInput,sxu32 nLen,SySet *pOut)` |
|        2 | 1058 |  |
|    10944 | 1059 | `	const char *zEnd = &zInput[nLen];` |
|    10944 | 1060 | `	const char *zIn  = zInput;` |
|        - | 1061 | `	const char *zCur,*zCurEnd;` |
|    10944 | 1062 | `	SyString sCtag = { 0, 0 };     /* Closing tag */` |
|        - | 1063 | `	SyToken sToken;` |
|        - | 1064 | `	SyString sDoc;` |
|        - | 1065 | `	sxu32 nLine;` |
|        - | 1066 | `	sxi32 iNest;` |
|        - | 1067 | `	sxi32 rc;` |
|        - | 1068 | `	/* Tokenize the input into PHP tokens and raw tokens */` |
|    10944 | 1069 | `	nLine = 1;` |
|    10944 | 1070 | `	zCur = zCurEnd   = 0; /* Prevent compiler warning */` |
|    10944 | 1071 | `	sToken.pUserData = 0;` |
|    10944 | 1072 | `	iNest = 0;` |
|    10944 | 1073 | `	sDoc.nByte = 0;` |
|    10944 | 1074 | `	sDoc.zString = ""; /* cc warning */` |
|    10944 | 1075 | `	for(;;){` |
|    21890 | 1076 | `		if( zIn >= zEnd ){` |
|        - | 1077 | `			/* End of input reached */` |
|    10940 | 1078 | `			break;` |
|        - | 1079 | `		}` |
|    10952 | 1080 | `		sToken.nLine = nLine;` |
|    10952 | 1081 | `		zCur = zIn;` |
|    10952 | 1082 | `		zCurEnd = 0;` |
|    10960 | 1083 | `		while( zIn < zEnd ){` |
|    10956 | 1084 | `			 if( zIn[0] == '<' ){` |
|    10948 | 1085 | `				const char *zTmp = zIn; /* End of raw input marker */` |
|    10948 | 1086 | `				zIn++;` |
|    10948 | 1087 | `				if( zIn < zEnd ){` |
|    10948 | 1088 | `					if( zIn[0] == '?' ){` |
|    10948 | 1089 | `						zIn++;` |
|    10948 | 1090 | `						if( (sxu32)(zEnd - zIn) >= sizeof("php")-1 &&  SyStrnicmp(zIn,"php",sizeof("php")-1) == 0 ){` |
|        - | 1091 | `							/* opening tag: <?php */` |
|    10946 | 1092 | `							zIn += sizeof("php")-1;` |
|     5472 | 1093 | `						}` |
|        - | 1094 | `						/* Look for the closing tag '?>' */` |
|    10948 | 1095 | `						SyStringInitFromBuf(&sCtag,"?>",sizeof("?>")-1);` |
|    10948 | 1096 | `						zCurEnd = zTmp;` |
|    10948 | 1097 | `						break;` |
|        - | 1098 | `					}` |
|      ! 0 | 1099 | `				}` |
|      ! 0 | 1100 | `			}else{` |
|       10 | 1101 | `				if( zIn[0] == '\n' ){` |
|       10 | 1102 | `					nLine++;` |
|        4 | 1103 | `				}` |
|       10 | 1104 | `				zIn++;` |
|        - | 1105 | `			 }` |
|        2 | 1106 | `		} /* While(zIn < zEnd) */` |
|    10952 | 1107 | `		if( zCurEnd == 0 ){` |
|        5 | 1108 | `			zCurEnd = zIn;` |
|        2 | 1109 | `		}` |
|        - | 1110 | `		/* Save the raw token */` |
|    10952 | 1111 | `		SyStringInitFromBuf(&sToken.sData,zCur,zCurEnd - zCur);` |
|    10952 | 1112 | `		sToken.nType = PH7_TOKEN_RAW;` |
|    10952 | 1113 | `		rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|    10952 | 1114 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1115 | `			return rc;` |
|        - | 1116 | `		}` |
|    10952 | 1117 | `		if( zIn >= zEnd ){` |
|        5 | 1118 | `			break;` |
|        - | 1119 | `		}` |
|        - | 1120 | `		/* Ignore leading white space */` |
|    23778 | 1121 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    12832 | 1122 | `			if( zIn[0] == '\n' ){` |
|    11636 | 1123 | `				nLine++;` |
|     5817 | 1124 | `			}` |
|    12832 | 1125 | `			zIn++;` |
|        2 | 1126 | `		}` |
|        - | 1127 | `		/* Delimit the PHP chunk */` |
|    10948 | 1128 | `		sToken.nLine = nLine;` |
|    10948 | 1129 | `		zCur = zIn;` |
|  1011192 | 1130 | `		while( (sxu32)(zEnd - zIn) >= sCtag.nByte ){` |
|        - | 1131 | `			const char *zPtr;` |
|  1006460 | 1132 | `			if( SyMemcmp(zIn,sCtag.zString,sCtag.nByte) == 0 && iNest < 1 ){` |
|     6216 | 1133 | `				break;` |
|        - | 1134 | `			}` |
|   502086 | 1135 | `			for(;;){` |
|  1004174 | 1136 | `				if( zIn[0] != '/' \|\| (zIn[1] != '*' && zIn[1] != '/') /* && sCtag.nByte >= 2 */ ){` |
|   500124 | 1137 | `					break;` |
|        - | 1138 | `				}` |
|     3930 | 1139 | `				zIn += 2;` |
|     3930 | 1140 | `				if( zIn[-1] == '/' ){` |
|        - | 1141 | `					/* Inline comment */` |
|   136218 | 1142 | `					while( zIn < zEnd && zIn[0] != '\n' ){` |
|   132372 | 1143 | `						zIn++;` |
|        2 | 1144 | `					}` |
|     3848 | 1145 | `					if( zIn >= zEnd ){` |
|      ! 0 | 1146 | `						zIn--;` |
|      ! 0 | 1147 | `					}` |
|     1925 | 1148 | `				}else{` |
|        - | 1149 | `					/* Block comment */` |
|     4500 | 1150 | `					while( (sxu32)(zEnd-zIn) >= sizeof("*/") - 1 ){` |
|     4500 | 1151 | `						if( zIn[0] == '*' && zIn[1] == '/' ){` |
|       84 | 1152 | `							zIn += 2;` |
|       84 | 1153 | `							break;` |
|        - | 1154 | `						}` |
|     4418 | 1155 | `						if( zIn[0] == '\n' ){` |
|       28 | 1156 | `							nLine++;` |
|       13 | 1157 | `						}` |
|     4418 | 1158 | `						zIn++;` |
|        2 | 1159 | `					}` |
|        - | 1160 | `				}` |
|        2 | 1161 | `			}` |
|  1000246 | 1162 | `			if( zIn[0] == '\n' ){` |
|    35176 | 1163 | `				nLine++;` |
|    35176 | 1164 | `				if( iNest > 0 ){` |
|      156 | 1165 | `					zIn++;` |
|      156 | 1166 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|      ! 0 | 1167 | `						zIn++;` |
|      ! 0 | 1168 | `					}` |
|      156 | 1169 | `					zPtr = zIn;` |
|      864 | 1170 | `					while( zIn < zEnd ){` |
|      864 | 1171 | `						if( (unsigned char)zIn[0] >= 0xc0 ){` |
|        - | 1172 | `							/* UTF-8 stream */` |
|       19 | 1173 | `							zIn++;` |
|       37 | 1174 | `							SX_JMP_UTF8(zIn,zEnd);` |
|      854 | 1175 | `						}else if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       79 | 1176 | `							break;` |
|      ! 0 | 1177 | `						}else{` |
|      692 | 1178 | `							zIn++;` |
|        - | 1179 | `						}` |
|        2 | 1180 | `					}` |
|      156 | 1181 | `					if( (sxu32)(zIn - zPtr) == sDoc.nByte && SyMemcmp(sDoc.zString,zPtr,sDoc.nByte) == 0 ){` |
|       60 | 1182 | `						iNest = 0;` |
|       29 | 1183 | `					}` |
|      156 | 1184 | `					continue;` |
|        2 | 1185 | `				}` |
|   982582 | 1186 | `			}else if ( (sxu32)(zEnd - zIn) >= sizeof("<<<") && zIn[0] == '<' && zIn[1] == '<' && zIn[2] == '<' && iNest < 1){` |
|       62 | 1187 | `				zIn += sizeof("<<<")-1;` |
|       74 | 1188 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|       13 | 1189 | `					zIn++;` |
|        1 | 1190 | `				}` |
|       62 | 1191 | `				if( zIn[0] == '"' \|\| zIn[0] == '\'' ){` |
|       32 | 1192 | `					zIn++;` |
|       15 | 1193 | `				}` |
|       62 | 1194 | `				zPtr = zIn;` |
|      330 | 1195 | `				while( zIn < zEnd ){` |
|      330 | 1196 | `					if( (unsigned char)zIn[0] >= 0xc0 ){` |
|        - | 1197 | `						/* UTF-8 stream */` |
|       19 | 1198 | `						zIn++;` |
|       37 | 1199 | `						SX_JMP_UTF8(zIn,zEnd);` |
|      320 | 1200 | `					}else if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       32 | 1201 | `						break;` |
|      ! 0 | 1202 | `					}else{` |
|      252 | 1203 | `						zIn++;` |
|        - | 1204 | `					}` |
|        2 | 1205 | `				}` |
|       62 | 1206 | `				SyStringInitFromBuf(&sDoc,zPtr,zIn-zPtr);` |
|       62 | 1207 | `				SyStringFullTrim(&sDoc);` |
|       62 | 1208 | `				if( sDoc.nByte > 0 ){` |
|       62 | 1209 | `					iNest++;` |
|       30 | 1210 | `				}` |
|       62 | 1211 | `				continue;` |
|        - | 1212 | `			}` |
|  1000032 | 1213 | `			zIn++;` |
|        - | 1214 |  |
|  1000032 | 1215 | `			if ( zIn >= zEnd )` |
|      ! 0 | 1216 | `				break;` |
|        2 | 1217 | `		}` |
|    10948 | 1218 | `		if( (sxu32)(zEnd - zIn) < sCtag.nByte ){` |
|     4734 | 1219 | `			zIn = zEnd;` |
|     2366 | 1220 | `		}` |
|    10948 | 1221 | `		if( zCur < zIn ){` |
|        - | 1222 | `			/* Save the PHP chunk for later processing */` |
|     8826 | 1223 | `			sToken.nType = PH7_TOKEN_PHP;` |
|     8826 | 1224 | `			SyStringInitFromBuf(&sToken.sData,zCur,zIn-zCur);` |
|    17626 | 1225 | `			SyStringRightTrim(&sToken.sData); /* Trim trailing white spaces */` |
|     8826 | 1226 | `			rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|     8826 | 1227 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1228 | `				return rc;` |
|        - | 1229 | `			}` |
|     4412 | 1230 | `		}` |
|    10948 | 1231 | `		if( zIn < zEnd ){` |
|        - | 1232 | `			/* Jump the trailing closing tag */` |
|     6216 | 1233 | `			zIn += sCtag.nByte;` |
|     3107 | 1234 | `		}` |
|        2 | 1235 | `	} /* For(;;) */` |
|        - | 1236 |  |
|    10944 | 1237 | ` 	return SXRET_OK;` |
|     5473 | 1238 |  |
|        - | 1239 |  |
