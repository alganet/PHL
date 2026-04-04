# src/ph7/lex.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 632/667 lines (94.75%)

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
|  7133558 |   19 | `static sxi32 TokenizePHP(SyStream *pStream,SyToken *pToken,void *pUserData,void *pCtxData)` |
|        2 |   20 |  |
|        - |   21 | `	SyString *pStr;` |
|        - |   22 | `	sxi32 rc;` |
|        - |   23 | `	/* Ignore leading white spaces */` |
| 10731258 |   24 | `	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){` |
|        - |   25 | `		/* Advance the stream cursor */` |
|  3597700 |   26 | `		if( pStream->zText[0] == '\n' ){` |
|        - |   27 | `			/* Update line counter */` |
|    27228 |   28 | `			pStream->nLine++;` |
|    13613 |   29 | `		}` |
|  3597700 |   30 | `		pStream->zText++;` |
|        2 |   31 | `	}` |
|  7133560 |   32 | `	if( pStream->zText >= pStream->zEnd ){` |
|        - |   33 | `		/* End of input reached */` |
|      ! 0 |   34 | `		return SXERR_EOF;` |
|        - |   35 | `	}` |
|        - |   36 | `	/* Record token starting position and line */` |
|  7133560 |   37 | `	pToken->nLine = pStream->nLine;` |
|  7133560 |   38 | `	pToken->pUserData = 0;` |
|  7133560 |   39 | `	pStr = &pToken->sData;` |
|  7133560 |   40 | `	SyStringInitFromBuf(pStr,pStream->zText,0);` |
|  8418153 |   41 | `	if( pStream->zText[0] >= 0xc0 \|\| SyisAlpha(pStream->zText[0]) \|\| pStream->zText[0] == '_' ){` |
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
|  2569188 |   52 | `		if( pStream->zText[0] < 0xc0 ){` |
|  2569172 |   53 | `			pStream->zText++;` |
|  1284585 |   54 | `		}` |
|  2522862 |   55 | `		for(;;){` |
|  5045726 |   56 | `			zIn = pStream->zText;` |
|  5045726 |   57 | `			if( zIn[0] >= 0xc0 ){` |
|       49 |   58 | `				zIn++;` |
|        - |   59 | `				/* UTF-8 stream */` |
|      109 |   60 | `				while( zIn < pStream->zEnd && ((zIn[0] & 0xc0) == 0x80) ){` |
|       61 |   61 | `					zIn++;` |
|        1 |   62 | `				}` |
|       24 |   63 | `			}` |
|        - |   64 | `			/* Skip alphanumeric stream */` |
| 20633408 |   65 | `			while( zIn < pStream->zEnd && zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_') ){` |
| 13064822 |   66 | `				zIn++;` |
|        2 |   67 | `			}` |
|  5045726 |   68 | `			if( zIn == pStream->zText ){` |
|        - |   69 | `				/* Not an UTF-8 or alphanumeric stream */` |
|  2569188 |   70 | `				break;` |
|        - |   71 | `			}` |
|        - |   72 | `			/* Synchronize pointers */` |
|  2476540 |   73 | `			pStream->zText = zIn;` |
|        2 |   74 | `		}` |
|        - |   75 | `		/* Record token length */` |
|  2569188 |   76 | `		pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  2569188 |   77 | `		nKeyword = KeywordCode(pStr->zString,(int)pStr->nByte);` |
|  2569188 |   78 | `		if( nKeyword != PH7_TK_ID ){` |
|   873760 |   79 | `			if( nKeyword &` |
|        - |   80 | `				(PH7_TKWRD_NEW\|PH7_TKWRD_CLONE\|PH7_TKWRD_AND\|PH7_TKWRD_XOR\|PH7_TKWRD_OR\|PH7_TKWRD_INSTANCEOF\|PH7_TKWRD_SEQ\|PH7_TKWRD_SNE) ){` |
|        - |   81 | `					/* Alpha stream operators [i.e: new,clone,and,instanceof,eq,ne,or,xor],save the operator instance for later processing */` |
|    14452 |   82 | `					pToken->pUserData = (void *)PH7_ExprExtractOperator(pStr,0);` |
|        - |   83 | `					/* Mark as an operator */` |
|    14452 |   84 | `					pToken->nType = PH7_TK_ID\|PH7_TK_OP;` |
|     7227 |   85 | `			}else{` |
|        - |   86 | `				/* We are dealing with a keyword [i.e: while,foreach,class...],save the keyword ID */` |
|   859310 |   87 | `				pToken->nType = PH7_TK_KEYWORD;` |
|   859310 |   88 | `				pToken->pUserData = SX_INT_TO_PTR(nKeyword);` |
|        - |   89 | `			}` |
|   436881 |   90 | `		}else{` |
|        - |   91 | `			/* A simple identifier */` |
|  1695430 |   92 | `			pToken->nType = PH7_TK_ID;` |
|        - |   93 | `		}` |
|  1284595 |   94 | `	}else{` |
|        - |   95 | `		sxi32 c;` |
|        - |   96 | `		/* Non-alpha stream */` |
|  4599583 |   97 | `		if( pStream->zText[0] == '#' \|\|` |
|  4564372 |   98 | `			( pStream->zText[0] == '/' &&  &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '/') ){` |
|     3656 |   99 | `				pStream->zText++;` |
|        - |  100 | `				/* Inline comments */` |
|   131336 |  101 | `				while( pStream->zText < pStream->zEnd && pStream->zText[0] != '\n' ){` |
|   127682 |  102 | `					pStream->zText++;` |
|        2 |  103 | `				}` |
|        - |  104 | `				/* Tell the upper-layer to ignore this token */` |
|     3656 |  105 | `				return SXERR_CONTINUE;` |
|  4560720 |  106 | `		}else if( pStream->zText[0] == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '*' ){` |
|    66708 |  107 | `			pStream->zText += 2;` |
|        - |  108 | `			/* Block comment */` |
|  1892180 |  109 | `			while( pStream->zText < pStream->zEnd ){` |
|  1892180 |  110 | `				if( pStream->zText[0] == '*' ){` |
|    66734 |  111 | `					if( &pStream->zText[1] >= pStream->zEnd \|\| pStream->zText[1] == '/'  ){` |
|    33355 |  112 | `						break;` |
|        - |  113 | `					}` |
|       13 |  114 | `				}` |
|  1825474 |  115 | `				if( pStream->zText[0] == '\n' ){` |
|       28 |  116 | `					pStream->nLine++;` |
|       13 |  117 | `				}` |
|  1825474 |  118 | `				pStream->zText++;` |
|        2 |  119 | `			}` |
|    66708 |  120 | `			pStream->zText += 2;` |
|        - |  121 | `			/* Tell the upper-layer to ignore this token */` |
|    66708 |  122 | `			return SXERR_CONTINUE;` |
|  4494014 |  123 | `		}else if( SyisDigit(pStream->zText[0]) ){` |
|    90876 |  124 | `			pStream->zText++;` |
|        - |  125 | `			/* Decimal digit stream */` |
|    99226 |  126 | `			while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|     8352 |  127 | `				pStream->zText++;` |
|        2 |  128 | `			}` |
|        - |  129 | `			/* Mark the token as integer until we encounter a real number */` |
|    90876 |  130 | `			pToken->nType = PH7_TK_INTEGER;` |
|    90876 |  131 | `			if( pStream->zText < pStream->zEnd ){` |
|    90876 |  132 | `				c = pStream->zText[0];` |
|    90876 |  133 | `				if( c == '.' ){` |
|        - |  134 | `					/* Real number */` |
|      388 |  135 | `					pStream->zText++;` |
|     1566 |  136 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|     1180 |  137 | `						pStream->zText++;` |
|        2 |  138 | `					}` |
|      388 |  139 | `					if( pStream->zText < pStream->zEnd ){` |
|      388 |  140 | `						c = pStream->zText[0];` |
|      388 |  141 | `						if( c=='e' \|\| c=='E' ){` |
|       19 |  142 | `							pStream->zText++;` |
|       19 |  143 | `							if( pStream->zText < pStream->zEnd ){` |
|       19 |  144 | `								c = pStream->zText[0];` |
|       24 |  145 | `								if( (c =='+' \|\| c=='-') && &pStream->zText[1] < pStream->zEnd  &&` |
|       13 |  146 | `									pStream->zText[1] < 0xc0 && SyisDigit(pStream->zText[1]) ){` |
|       13 |  147 | `										pStream->zText++;` |
|        6 |  148 | `								}` |
|       39 |  149 | `								while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|       21 |  150 | `									pStream->zText++;` |
|        1 |  151 | `								}` |
|        9 |  152 | `							}` |
|        9 |  153 | `						}` |
|      193 |  154 | `					}` |
|      388 |  155 | `					pToken->nType = PH7_TK_REAL;` |
|    90683 |  156 | `				}else if( c=='e' \|\| c=='E' ){` |
|        7 |  157 | `					SXUNUSED(pUserData); /* Prevent compiler warning */` |
|        7 |  158 | `					SXUNUSED(pCtxData);` |
|       15 |  159 | `					pStream->zText++;` |
|       15 |  160 | `					if( pStream->zText < pStream->zEnd ){` |
|       15 |  161 | `						c = pStream->zText[0];` |
|       16 |  162 | `						if( (c =='+' \|\| c=='-') && &pStream->zText[1] < pStream->zEnd  &&` |
|        5 |  163 | `							pStream->zText[1] < 0xc0 && SyisDigit(pStream->zText[1]) ){` |
|        5 |  164 | `								pStream->zText++;` |
|        2 |  165 | `						}` |
|       33 |  166 | `						while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|       19 |  167 | `							pStream->zText++;` |
|        1 |  168 | `						}` |
|        7 |  169 | `					}` |
|       15 |  170 | `					pToken->nType = PH7_TK_REAL;` |
|    90483 |  171 | `				}else if( c == 'x' \|\| c == 'X' ){` |
|        - |  172 | `					/* Hex digit stream */` |
|       16 |  173 | `					pStream->zText++;` |
|       50 |  174 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisHex(pStream->zText[0]) ){` |
|       35 |  175 | `						pStream->zText++;` |
|        1 |  176 | `					}` |
|    90469 |  177 | `				}else if(c  == 'b' \|\| c == 'B' ){` |
|        - |  178 | `					/* Binary digit stream */` |
|       31 |  179 | `					pStream->zText++;` |
|      198 |  180 | `					while( pStream->zText < pStream->zEnd && (pStream->zText[0] == '0' \|\| pStream->zText[0] == '1') ){` |
|      153 |  181 | `						pStream->zText++;` |
|        1 |  182 | `					}` |
|       15 |  183 | `				}` |
|    45437 |  184 | `			}` |
|        - |  185 | `			/* Record token length */` |
|    90876 |  186 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|    90876 |  187 | `			return SXRET_OK;` |
|        - |  188 | `		}` |
|  4403140 |  189 | `		c = pStream->zText[0];` |
|  4403140 |  190 | `		pStream->zText++; /* Advance the stream cursor */` |
|        - |  191 | `		/* Assume we are dealing with an operator*/` |
|  4403140 |  192 | `		pToken->nType = PH7_TK_OP;` |
|  4403140 |  193 | `		switch(c){` |
|   926972 |  194 | `		case '$': pToken->nType = PH7_TK_DOLLAR; break;` |
|   346148 |  195 | `		case '{': pToken->nType = PH7_TK_OCB;    break;` |
|   346134 |  196 | `		case '}': pToken->nType = PH7_TK_CCB;    break;` |
|   699066 |  197 | `		case '(': pToken->nType = PH7_TK_LPAREN; break;` |
|    71282 |  198 | `		case '[': pToken->nType \|= PH7_TK_OSB;   break; /* Bitwise operation here,since the square bracket token '['` |
|        - |  199 | `														 * is a potential operator [i.e: subscripting] */` |
|    71288 |  200 | `		case ']': pToken->nType = PH7_TK_CSB;    break;` |
|   349526 |  201 | `		case ')': {` |
|   699054 |  202 | `			SySet *pTokSet = pStream->pSet;` |
|        - |  203 | `			/* Assemble type cast operators [i.e: (int),(float),(bool)...] */` |
|   699054 |  204 | `			if( pTokSet->nUsed >= 2 ){` |
|        - |  205 | `				SyToken *pTmp;` |
|        - |  206 | `				/* Peek the last recongnized token */` |
|   699052 |  207 | `				pTmp = (SyToken *)SySetPeek(pTokSet);` |
|   699052 |  208 | `				if( pTmp->nType & PH7_TK_KEYWORD ){` |
|    14170 |  209 | `					sxi32 nID = SX_PTR_TO_INT(pTmp->pUserData);` |
|    14170 |  210 | `					if( (sxu32)nID & (PH7_TKWRD_ARRAY\|PH7_TKWRD_INT\|PH7_TKWRD_FLOAT\|PH7_TKWRD_STRING\|PH7_TKWRD_OBJECT\|PH7_TKWRD_BOOL\|PH7_TKWRD_UNSET) ){` |
|    14098 |  211 | `						pTmp = (SyToken *)SySetAt(pTokSet,pTokSet->nUsed - 2);` |
|    14098 |  212 | `						if( pTmp->nType & PH7_TK_LPAREN ){` |
|        - |  213 | `							/* Merge the three tokens '(' 'TYPE' ')' into a single one */` |
|    14014 |  214 | `							const char * zTypeCast = "(int)";` |
|    14014 |  215 | `							if( nID & PH7_TKWRD_FLOAT ){` |
|     2784 |  216 | `								zTypeCast = "(float)";` |
|    12623 |  217 | `							}else if( nID & PH7_TKWRD_BOOL ){` |
|     2786 |  218 | `								zTypeCast = "(bool)";` |
|     9840 |  219 | `							}else if( nID & PH7_TKWRD_STRING ){` |
|     5562 |  220 | `								zTypeCast = "(string)";` |
|     5668 |  221 | `							}else if( nID & PH7_TKWRD_ARRAY ){` |
|       21 |  222 | `								zTypeCast = "(array)";` |
|     2878 |  223 | `							}else if( nID & PH7_TKWRD_OBJECT ){` |
|       17 |  224 | `								zTypeCast = "(object)";` |
|     2860 |  225 | `							}else if( nID & PH7_TKWRD_UNSET ){` |
|        7 |  226 | `								zTypeCast = "(unset)";` |
|        3 |  227 | `							}` |
|        - |  228 | `							/* Reflect the change */` |
|    14014 |  229 | `							pToken->nType = PH7_TK_OP;` |
|    14014 |  230 | `							SyStringInitFromBuf(&pToken->sData,zTypeCast,SyStrlen(zTypeCast));` |
|        - |  231 | `							/* Save the instance associated with the type cast operator */` |
|    14014 |  232 | `							pToken->pUserData = (void *)PH7_ExprExtractOperator(&pToken->sData,0);` |
|        - |  233 | `							/* Remove the two previous tokens */` |
|    14014 |  234 | `							pTokSet->nUsed -= 2;` |
|    14014 |  235 | `							return SXRET_OK;` |
|        - |  236 | `						}` |
|       42 |  237 | `					}` |
|       78 |  238 | `				}` |
|   342519 |  239 | `			}` |
|   685042 |  240 | `			pToken->nType = PH7_TK_RPAREN;` |
|   685042 |  241 | `			break;` |
|        - |  242 | `				  }` |
|    29661 |  243 | `		case '\'':{` |
|        - |  244 | `			/* Single quoted string */` |
|    59324 |  245 | `			pStr->zString++;` |
|   749392 |  246 | `			while( pStream->zText < pStream->zEnd ){` |
|   749392 |  247 | `				if( pStream->zText[0] == '\''  ){` |
|    59334 |  248 | `					if( pStream->zText[-1] != '\\' ){` |
|    59310 |  249 | `						break;` |
|      ! 0 |  250 | `					}else{` |
|       25 |  251 | `						const unsigned char *zPtr = &pStream->zText[-2];` |
|       25 |  252 | `						sxi32 i = 1;` |
|       43 |  253 | `						while( zPtr > pStream->zInput && zPtr[0] == '\\' ){` |
|       19 |  254 | `							zPtr--;` |
|       19 |  255 | `							i++;` |
|        1 |  256 | `						}` |
|       25 |  257 | `						if((i&1)==0){` |
|       15 |  258 | `							break;` |
|        - |  259 | `						}` |
|        - |  260 | `					}` |
|        5 |  261 | `				}` |
|   690070 |  262 | `				if( pStream->zText[0] == '\n' ){` |
|       67 |  263 | `					pStream->nLine++;` |
|       33 |  264 | `				}` |
|   690070 |  265 | `				pStream->zText++;` |
|        2 |  266 | `			}` |
|        - |  267 | `			/* Record token length and type */` |
|    59324 |  268 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|    59324 |  269 | `			pToken->nType = PH7_TK_SSTR;` |
|        - |  270 | `			/* Jump the trailing single quote */` |
|    59324 |  271 | `			pStream->zText++;` |
|    59324 |  272 | `			return SXRET_OK;` |
|        - |  273 | `				  }` |
|     7134 |  274 | `		case '"':{` |
|        - |  275 | `			sxi32 iNest;` |
|        - |  276 | `			/* Double quoted string */` |
|    14270 |  277 | `			pStr->zString++;` |
|   148192 |  278 | `			while( pStream->zText < pStream->zEnd ){` |
|   148192 |  279 | `				if( pStream->zText[0] == '{' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '$'){` |
|       71 |  280 | `					iNest = 1;` |
|       71 |  281 | `					pStream->zText++;` |
|        - |  282 | `					/* TICKET 1433-40: Hnadle braces'{}' in double quoted string where everything is allowed */` |
|      823 |  283 | `					while(pStream->zText < pStream->zEnd ){` |
|      823 |  284 | `						if( pStream->zText[0] == '{' ){` |
|        9 |  285 | `							iNest++;` |
|      819 |  286 | `						}else if (pStream->zText[0] == '}' ){` |
|       79 |  287 | `							iNest--;` |
|       79 |  288 | `							if( iNest <= 0 ){` |
|       71 |  289 | `								pStream->zText++;` |
|       71 |  290 | `								break;` |
|        1 |  291 | `							}` |
|      741 |  292 | `						}else if( pStream->zText[0] == '\n' ){` |
|      ! 0 |  293 | `							pStream->nLine++;` |
|      ! 0 |  294 | `						}` |
|      753 |  295 | `						pStream->zText++;` |
|        1 |  296 | `					}` |
|       71 |  297 | `					if( pStream->zText >= pStream->zEnd ){` |
|      ! 0 |  298 | `						break;` |
|        - |  299 | `					}` |
|       35 |  300 | `				}` |
|   148192 |  301 | `				if( pStream->zText[0] == '"' ){` |
|    14370 |  302 | `					if( pStream->zText[-1] != '\\' ){` |
|    14266 |  303 | `						break;` |
|      ! 0 |  304 | `					}else{` |
|      106 |  305 | `						const unsigned char *zPtr = &pStream->zText[-2];` |
|      106 |  306 | `						sxi32 i = 1;` |
|      158 |  307 | `						while( zPtr > pStream->zInput && zPtr[0] == '\\' ){` |
|       54 |  308 | `							zPtr--;` |
|       54 |  309 | `							i++;` |
|        2 |  310 | `						}` |
|      106 |  311 | `						if((i&1)==0){` |
|        5 |  312 | `							break;` |
|        - |  313 | `						}` |
|        - |  314 | `					}` |
|       50 |  315 | `				}` |
|   133924 |  316 | `				if( pStream->zText[0] == '\n' ){` |
|        7 |  317 | `					pStream->nLine++;` |
|        3 |  318 | `				}` |
|   133924 |  319 | `				pStream->zText++;` |
|        2 |  320 | `			}` |
|        - |  321 | `			/* Record token length and type */` |
|    14270 |  322 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|    14270 |  323 | `			pToken->nType = PH7_TK_DSTR;` |
|        - |  324 | `			/* Jump the trailing quote */` |
|    14270 |  325 | `			pStream->zText++;` |
|    14270 |  326 | `			return SXRET_OK;` |
|        - |  327 | `				  }` |
|        2 |  328 | ``		case '`':{`` |
|        - |  329 | `			/* Backtick quoted string */` |
|        5 |  330 | `			pStr->zString++;` |
|       45 |  331 | `			while( pStream->zText < pStream->zEnd ){` |
|       45 |  332 | ``				if( pStream->zText[0] == '`' && pStream->zText[-1] != '\\' ){`` |
|        5 |  333 | `					break;` |
|        - |  334 | `				}` |
|       41 |  335 | `				if( pStream->zText[0] == '\n' ){` |
|      ! 0 |  336 | `					pStream->nLine++;` |
|      ! 0 |  337 | `				}` |
|       41 |  338 | `				pStream->zText++;` |
|        1 |  339 | `			}` |
|        - |  340 | `			/* Record token length and type */` |
|        5 |  341 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|        5 |  342 | `			pToken->nType = PH7_TK_BSTR;` |
|        - |  343 | `			/* Jump the trailing backtick */` |
|        5 |  344 | `			pStream->zText++;` |
|        5 |  345 | `			return SXRET_OK;` |
|        - |  346 | `				  }` |
|      111 |  347 | `		case '\\': pToken->nType = PH7_TK_NSSEP;  break;` |
|     1062 |  348 | `		case ':':` |
|     2126 |  349 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == ':' ){` |
|        - |  350 | `				/* Current operator: '::' */` |
|      118 |  351 | `				pStream->zText++;` |
|       60 |  352 | `			}else{` |
|     2010 |  353 | `				pToken->nType = PH7_TK_COLON; /* Single colon */` |
|        - |  354 | `			}` |
|     2126 |  355 | `			break;` |
|    74260 |  356 | `		case ',': pToken->nType \|= PH7_TK_COMMA;  break; /* Comma is also an operator */` |
|   501368 |  357 | `		case ';': pToken->nType = PH7_TK_SEMI;    break;` |
|        - |  358 | `			/* Handle combined operators [i.e: +=,===,!=== ...] */` |
|   145255 |  359 | `		case '=':` |
|   290512 |  360 | `			pToken->nType \|= PH7_TK_EQUAL;` |
|   290512 |  361 | `			if( pStream->zText < pStream->zEnd ){` |
|   290512 |  362 | `				if( pStream->zText[0] == '=' ){` |
|    18058 |  363 | `					pToken->nType &= ~PH7_TK_EQUAL;` |
|        - |  364 | `					/* Current operator: == */` |
|    18058 |  365 | `					pStream->zText++;` |
|    18058 |  366 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  367 | `						/* Current operator: === */` |
|     3872 |  368 | `						pStream->zText++;` |
|     1937 |  369 | `					}` |
|   281484 |  370 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  371 | `					/* Array operator: => */` |
|     4120 |  372 | `					pToken->nType = PH7_TK_ARRAY_OP;` |
|     4120 |  373 | `					pStream->zText++;` |
|     2061 |  374 | `				}else{` |
|        - |  375 | `					/* TICKET 1433-0010: Reference operator '=&' */` |
|   268338 |  376 | `					const unsigned char *zCur = pStream->zText;` |
|   268338 |  377 | `					sxu32 nLine = 0;` |
|   536652 |  378 | `					while( zCur < pStream->zEnd && zCur[0] < 0xc0 && SyisSpace(zCur[0]) ){` |
|   268316 |  379 | `						if( zCur[0] == '\n' ){` |
|        5 |  380 | `							nLine++;` |
|        2 |  381 | `						}` |
|   268316 |  382 | `						zCur++;` |
|        2 |  383 | `					}` |
|   268338 |  384 | `					if( zCur < pStream->zEnd && zCur[0] == '&' ){` |
|        - |  385 | `						/* Current operator: =& */` |
|       46 |  386 | `						pToken->nType &= ~PH7_TK_EQUAL;` |
|       46 |  387 | `						SyStringInitFromBuf(pStr,"=&",sizeof("=&")-1);` |
|        - |  388 | `						/* Update token stream */` |
|       46 |  389 | `						pStream->zText = &zCur[1];` |
|       46 |  390 | `						pStream->nLine += nLine;` |
|       22 |  391 | `					}` |
|        - |  392 | `				}` |
|   145255 |  393 | `			}` |
|   290512 |  394 | `			break;` |
|    19779 |  395 | `		case '!':` |
|    39560 |  396 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  397 | `				/* Current operator: != */` |
|    16834 |  398 | `				pStream->zText++;` |
|    16834 |  399 | `				if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  400 | `					/* Current operator: !== */` |
|    14026 |  401 | `					pStream->zText++;` |
|     7012 |  402 | `				}` |
|     8416 |  403 | `			}` |
|    39560 |  404 | `			break;` |
|    11362 |  405 | `		case '&':` |
|    22726 |  406 | `			pToken->nType \|= PH7_TK_AMPER;` |
|    22726 |  407 | `			if( pStream->zText < pStream->zEnd ){` |
|    22726 |  408 | `				if( pStream->zText[0] == '&' ){` |
|     8728 |  409 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|        - |  410 | `					/* Current operator: && */` |
|     8728 |  411 | `					pStream->zText++;` |
|    18363 |  412 | `				}else if( pStream->zText[0] == '=' ){` |
|        5 |  413 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|        - |  414 | `					/* Current operator: &= */` |
|        5 |  415 | `					pStream->zText++;` |
|        2 |  416 | `				}` |
|    11362 |  417 | `			}` |
|    22726 |  418 | `			break;` |
|     1462 |  419 | `		case '\|':` |
|     2926 |  420 | `			if( pStream->zText < pStream->zEnd ){` |
|     2926 |  421 | `				if( pStream->zText[0] == '\|' ){` |
|        - |  422 | `					/* Current operator: \|\| */` |
|     2910 |  423 | `					pStream->zText++;` |
|     1471 |  424 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  425 | `					/* Current operator: \|= */` |
|        5 |  426 | `					pStream->zText++;` |
|        2 |  427 | `				}` |
|     1462 |  428 | `			}` |
|     2926 |  429 | `			break;` |
|     7257 |  430 | `		case '+':` |
|    14516 |  431 | `			if( pStream->zText < pStream->zEnd ){` |
|    14514 |  432 | `				if( pStream->zText[0] == '+' ){` |
|        - |  433 | `					/* Current operator: ++ */` |
|    11336 |  434 | `					pStream->zText++;` |
|     8847 |  435 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  436 | `					/* Current operator: += */` |
|       32 |  437 | `					pStream->zText++;` |
|       15 |  438 | `				}` |
|     7256 |  439 | `			}` |
|    14516 |  440 | `			break;` |
|    53339 |  441 | `		case '-':` |
|   106680 |  442 | `			if( pStream->zText < pStream->zEnd ){` |
|   106680 |  443 | `				if( pStream->zText[0] == '-' ){` |
|        - |  444 | `					/* Current operator: -- */` |
|        5 |  445 | `					pStream->zText++;` |
|   106678 |  446 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  447 | `					/* Current operator: -= */` |
|        3 |  448 | `					pStream->zText++;` |
|   106675 |  449 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  450 | `					/* Current operator: -> */` |
|   106216 |  451 | `					pStream->zText++;` |
|    53107 |  452 | `				}` |
|    53339 |  453 | `			}` |
|   106680 |  454 | `			break;` |
|       73 |  455 | `		case '*':` |
|      148 |  456 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  457 | `				/* Current operator: *= */` |
|       13 |  458 | `				pStream->zText++;` |
|        6 |  459 | `			}` |
|      148 |  460 | `			break;` |
|       29 |  461 | `		case '/':` |
|       60 |  462 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  463 | `				/* Current operator: /= */` |
|        3 |  464 | `				pStream->zText++;` |
|        1 |  465 | `			}` |
|       60 |  466 | `			break;` |
|       23 |  467 | `		case '%':` |
|       48 |  468 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  469 | `				/* Current operator: %= */` |
|        3 |  470 | `				pStream->zText++;` |
|        1 |  471 | `			}` |
|       48 |  472 | `			break;` |
|        9 |  473 | `		case '^':` |
|       19 |  474 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  475 | `				/* Current operator: ^= */` |
|        7 |  476 | `				pStream->zText++;` |
|        3 |  477 | `			}` |
|       19 |  478 | `			break;` |
|    29615 |  479 | `		case '.':` |
|    59232 |  480 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  481 | `				/* Current operator: .= */` |
|     2844 |  482 | `				pStream->zText++;` |
|     1421 |  483 | `			}` |
|    59232 |  484 | `			break;` |
|    23790 |  485 | `		case '<':` |
|    47582 |  486 | `			if( pStream->zText < pStream->zEnd ){` |
|    47582 |  487 | `				if( pStream->zText[0] == '<' ){` |
|        - |  488 | `					/* Current operator: << */` |
|       74 |  489 | `					pStream->zText++;` |
|       74 |  490 | `					if( pStream->zText < pStream->zEnd ){` |
|       74 |  491 | `						if( pStream->zText[0] == '=' ){` |
|        - |  492 | `							/* Current operator: <<= */` |
|        7 |  493 | `							pStream->zText++;` |
|       71 |  494 | `						}else if( pStream->zText[0] == '<' ){` |
|        - |  495 | `							/* Current Token: <<<  */` |
|       58 |  496 | `							pStream->zText++;` |
|        - |  497 | `							/* This may be the beginning of a Heredoc/Nowdoc string,try to delimit it */` |
|       58 |  498 | `							rc = LexExtractHeredoc(&(*pStream),&(*pToken));` |
|       58 |  499 | `							if( rc == SXRET_OK ){` |
|        - |  500 | `								/* Here/Now doc successfuly extracted */` |
|       58 |  501 | `								return SXRET_OK;` |
|        - |  502 | `							}` |
|      ! 0 |  503 | `						}` |
|        9 |  504 | `					}` |
|    47518 |  505 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  506 | `					/* Current operator: <> */` |
|        5 |  507 | `					pStream->zText++;` |
|    47508 |  508 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  509 | `					/* Current operator: <= */` |
|       34 |  510 | `					pStream->zText++;` |
|       16 |  511 | `				}` |
|    23762 |  512 | `			}` |
|    47526 |  513 | `			break;` |
|     2877 |  514 | `		case '>':` |
|     5756 |  515 | `			if( pStream->zText < pStream->zEnd ){` |
|     5756 |  516 | `				if( pStream->zText[0] == '>' ){` |
|        - |  517 | `					/* Current operator: >> */` |
|       17 |  518 | `					pStream->zText++;` |
|       17 |  519 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  520 | `						/* Current operator: >>= */` |
|        9 |  521 | `						pStream->zText++;` |
|        5 |  522 | `					}` |
|     5748 |  523 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  524 | `					/* Current operator: >= */` |
|       78 |  525 | `					pStream->zText++;` |
|       38 |  526 | `				}` |
|     2877 |  527 | `			}` |
|     5754 |  528 | `			break;` |
|     1008 |  529 | `		default:` |
|     2016 |  530 | `			break;` |
|        - |  531 | `		}` |
|  4315478 |  532 | `		if( pStr->nByte <= 0 ){` |
|        - |  533 | `			/* Record token length */` |
|  4315434 |  534 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  2157716 |  535 | `		}` |
|  4315478 |  536 | `		if( pToken->nType & PH7_TK_OP ){` |
|        - |  537 | `			const ph7_expr_op *pOp;` |
|        - |  538 | `			/* Check if the extracted token is an operator */` |
|   733238 |  539 | `			pOp = PH7_ExprExtractOperator(pStr,(SyToken *)SySetPeek(pStream->pSet));` |
|   733238 |  540 | `			if( pOp == 0 ){` |
|        - |  541 | `				/* Not an operator */` |
|      ! 0 |  542 | `				pToken->nType &= ~PH7_TK_OP;` |
|      ! 0 |  543 | `				if( pToken->nType <= 0 ){` |
|      ! 0 |  544 | `					pToken->nType = PH7_TK_OTHER;` |
|      ! 0 |  545 | `				}` |
|      ! 0 |  546 | `			}else{` |
|        - |  547 | `				/* Save the instance associated with this operator for later processing */` |
|   733238 |  548 | `				pToken->pUserData = (void *)pOp;` |
|        - |  549 | `			}` |
|   366618 |  550 | `		}` |
|        - |  551 | `	}` |
|        - |  552 | `	/* Tell the upper-layer to save the extracted token for later processing */` |
|  6884664 |  553 | `	return SXRET_OK;` |
|  3566781 |  554 |  |
|        - |  555 | `/***** This file contains automatically generated code ******` |
|        - |  556 | `**` |
|        - |  557 | `** The code in this file has been automatically generated by` |
|        - |  558 | `**` |
|        - |  559 | `**     $Header: /sqlite/sqlite/tool/mkkeywordhash.c` |
|        - |  560 | `**` |
|        - |  561 | `** Sligthly modified by Chems mrad <chm@symisc.net> for the PH7 engine.` |
|        - |  562 | `**` |
|        - |  563 | `** The code in this file implements a function that determines whether` |
|        - |  564 | `** or not a given identifier is really a PHP keyword.  The same thing` |
|        - |  565 | `** might be implemented more directly using a hand-written hash table.` |
|        - |  566 | `** But by using this automatically generated code, the size of the code` |
|        - |  567 | `** is substantially reduced.  This is important for embedded applications` |
|        - |  568 | `** on platforms with limited memory.` |
|        - |  569 | `*/` |
|        - |  570 | `/* Hash score: 103 */` |
|  2569188 |  571 | `static sxu32 KeywordCode(const char *z, int n){` |
|        - |  572 | `  /* zText[] encodes 532 bytes of keywords in 333 bytes */` |
|        - |  573 | `  /*   extendswitchprintegerequire_oncenddeclareturnamespacechobject      */` |
|        - |  574 | `  /*   hrowbooleandefaultrycaselfinalistaticlonewconstringlobaluse        */` |
|        - |  575 | `  /*   lseifloatvarrayANDIEchoUSECHOabstractclasscontinuendifunction      */` |
|        - |  576 | `  /*   diendwhilevaldoexitgotoimplementsinclude_oncemptyinstanceof        */` |
|        - |  577 | `  /*   interfacendforeachissetparentprivateprotectedpublicatchunset       */` |
|        - |  578 | `  /*   xorARRAYASArrayEXITUNSETXORbreak                                   */` |
|        - |  579 | `  static const char zText[332] = {` |
|        - |  580 | `    'e','x','t','e','n','d','s','w','i','t','c','h','p','r','i','n','t','e',` |
|        - |  581 | `    'g','e','r','e','q','u','i','r','e','_','o','n','c','e','n','d','d','e',` |
|        - |  582 | `    'c','l','a','r','e','t','u','r','n','a','m','e','s','p','a','c','e','c',` |
|        - |  583 | `    'h','o','b','j','e','c','t','h','r','o','w','b','o','o','l','e','a','n',` |
|        - |  584 | `    'd','e','f','a','u','l','t','r','y','c','a','s','e','l','f','i','n','a',` |
|        - |  585 | `    'l','i','s','t','a','t','i','c','l','o','n','e','w','c','o','n','s','t',` |
|        - |  586 | `    'r','i','n','g','l','o','b','a','l','u','s','e','l','s','e','i','f','l',` |
|        - |  587 | `    'o','a','t','v','a','r','r','a','y','A','N','D','I','E','c','h','o','U',` |
|        - |  588 | `    'S','E','C','H','O','a','b','s','t','r','a','c','t','c','l','a','s','s',` |
|        - |  589 | `    'c','o','n','t','i','n','u','e','n','d','i','f','u','n','c','t','i','o',` |
|        - |  590 | `    'n','d','i','e','n','d','w','h','i','l','e','v','a','l','d','o','e','x',` |
|        - |  591 | `    'i','t','g','o','t','o','i','m','p','l','e','m','e','n','t','s','i','n',` |
|        - |  592 | `    'c','l','u','d','e','_','o','n','c','e','m','p','t','y','i','n','s','t',` |
|        - |  593 | `    'a','n','c','e','o','f','i','n','t','e','r','f','a','c','e','n','d','f',` |
|        - |  594 | `    'o','r','e','a','c','h','i','s','s','e','t','p','a','r','e','n','t','p',` |
|        - |  595 | `    'r','i','v','a','t','e','p','r','o','t','e','c','t','e','d','p','u','b',` |
|        - |  596 | `    'l','i','c','a','t','c','h','u','n','s','e','t','x','o','r','A','R','R',` |
|        - |  597 | `    'A','Y','A','S','A','r','r','a','y','E','X','I','T','U','N','S','E','T',` |
|        - |  598 | `    'X','O','R','b','r','e','a','k'` |
|        - |  599 | `  };` |
|        - |  600 | `  static const unsigned char aHash[151] = {` |
|        - |  601 |  |
|        - |  602 |  |
|        - |  603 | `      74,   0,  51,   0,  76,   0,   0,  20,   0,   0,   0,  50,   0,` |
|        - |  604 | `      80,  34,   0,  36,   0,   0,  64,  16,   0,   0,  17,   0,   1,` |
|        - |  605 | `      19,  84,  66,   0,  43,  45,  78,   0,   0,  53,  56,   0,   0,` |
|        - |  606 |  |
|        - |  607 | `      72,  14,   0,  71,   0,  38,   6,   0,   0,   0,  73,   0,   0,` |
|        - |  608 | `       3,   0,  41,   5,  52,  57,  32,   0,  60,  63,   0,  69,  82,` |
|        - |  609 | `      30,   0,  79,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,` |
|        - |  610 |  |
|        - |  611 | `      62,   0,  11,   0,   0,  58,   0,   0,   0,   0,  59,  75,   0,` |
|        - |  612 |  |
|        - |  613 | `  };` |
|        - |  614 | `  static const unsigned char aNext[84] = {` |
|        - |  615 |  |
|        - |  616 |  |
|        - |  617 |  |
|        - |  618 |  |
|        - |  619 |  |
|        - |  620 |  |
|        - |  621 | `      42,   0,   0,   0,  70,  55` |
|        - |  622 | `  };` |
|        - |  623 | `  static const unsigned char aLen[84] = {` |
|        - |  624 | `       7,   9,   6,   5,   7,  12,   7,   2,  10,   7,   6,   9,   4,` |
|        - |  625 | `       6,   5,   7,   4,   3,   7,   3,   4,   4,   5,   4,   6,   5,` |
|        - |  626 | `       2,   3,   5,   6,   6,   3,   6,   4,   2,   5,   3,   5,   3,` |
|        - |  627 | `       3,   4,   3,   4,   8,   5,   2,   8,   5,   8,   3,   8,   5,` |
|        - |  628 | `       4,   2,   4,   4,  10,  12,   7,   5,  10,   9,   3,   6,  10,` |
|        - |  629 | `       3,   7,   2,   5,   6,   7,   9,   6,   5,   5,   3,   5,   2,` |
|        - |  630 | `       5,   4,   5,   3,   2,   5` |
|        - |  631 | `  };` |
|        - |  632 | `  static const sxu16 aOffset[84] = {` |
|        - |  633 |  |
|        - |  634 | `      55,  60,  65,  65,  70,  72,  78,  81,  83,  86,  90,  92,  97,` |
|        - |  635 | `     100, 100, 103, 106, 111, 117, 119, 119, 123, 124, 129, 130, 135,` |
|        - |  636 | `     137, 139, 143, 145, 149, 157, 159, 162, 169, 173, 181, 183, 186,` |
|        - |  637 | `     190, 194, 196, 200, 204, 214, 214, 225, 230, 240, 240, 248, 248,` |
|        - |  638 | `     251, 251, 252, 258, 263, 269, 276, 285, 290, 295, 300, 303, 308,` |
|        - |  639 | `     310, 315, 319, 324, 325, 327` |
|        - |  640 | `  };` |
|        - |  641 | `  static const sxu32 aCode[84] = {` |
|        - |  642 | `    PH7_TKWRD_EXTENDS,   PH7_TKWRD_ENDSWITCH,   PH7_TKWRD_SWITCH,    PH7_TKWRD_PRINT,   PH7_TKWRD_INT,` |
|        - |  643 | `    PH7_TKWRD_REQONCE,   PH7_TKWRD_REQUIRE,     PH7_TKWRD_SEQ,       PH7_TKWRD_ENDDEC,    PH7_TKWRD_DECLARE,` |
|        - |  644 | `    PH7_TKWRD_RETURN,    PH7_TKWRD_NAMESPACE,   PH7_TKWRD_ECHO,      PH7_TKWRD_OBJECT,    PH7_TKWRD_THROW,` |
|        - |  645 | `    PH7_TKWRD_BOOL,      PH7_TKWRD_BOOL,        PH7_TKWRD_AND,       PH7_TKWRD_DEFAULT,   PH7_TKWRD_TRY,` |
|        - |  646 | `    PH7_TKWRD_CASE,      PH7_TKWRD_SELF,        PH7_TKWRD_FINAL,     PH7_TKWRD_LIST,      PH7_TKWRD_STATIC,` |
|        - |  647 | `    PH7_TKWRD_CLONE,     PH7_TKWRD_SNE,         PH7_TKWRD_NEW,       PH7_TKWRD_CONST,     PH7_TKWRD_STRING,` |
|        - |  648 | `    PH7_TKWRD_GLOBAL,    PH7_TKWRD_USE,         PH7_TKWRD_ELIF,      PH7_TKWRD_ELSE,      PH7_TKWRD_IF,` |
|        - |  649 | `    PH7_TKWRD_FLOAT,     PH7_TKWRD_VAR,         PH7_TKWRD_ARRAY,     PH7_TKWRD_AND,       PH7_TKWRD_DIE,` |
|        - |  650 | `    PH7_TKWRD_ECHO,      PH7_TKWRD_USE,         PH7_TKWRD_ECHO,      PH7_TKWRD_ABSTRACT,  PH7_TKWRD_CLASS,` |
|        - |  651 | `    PH7_TKWRD_AS,        PH7_TKWRD_CONTINUE,    PH7_TKWRD_ENDIF,     PH7_TKWRD_FUNCTION,  PH7_TKWRD_DIE,` |
|        - |  652 | `    PH7_TKWRD_ENDWHILE,  PH7_TKWRD_WHILE,       PH7_TKWRD_EVAL,      PH7_TKWRD_DO,        PH7_TKWRD_EXIT,` |
|        - |  653 | `    PH7_TKWRD_GOTO,      PH7_TKWRD_IMPLEMENTS,  PH7_TKWRD_INCONCE,   PH7_TKWRD_INCLUDE,   PH7_TKWRD_EMPTY,` |
|        - |  654 | `    PH7_TKWRD_INSTANCEOF,PH7_TKWRD_INTERFACE,   PH7_TKWRD_INT,       PH7_TKWRD_ENDFOR,    PH7_TKWRD_END4EACH,` |
|        - |  655 | `    PH7_TKWRD_FOR,       PH7_TKWRD_FOREACH,     PH7_TKWRD_OR,        PH7_TKWRD_ISSET,     PH7_TKWRD_PARENT,` |
|        - |  656 | `    PH7_TKWRD_PRIVATE,   PH7_TKWRD_PROTECTED,   PH7_TKWRD_PUBLIC,    PH7_TKWRD_CATCH,     PH7_TKWRD_UNSET,` |
|        - |  657 | `    PH7_TKWRD_XOR,       PH7_TKWRD_ARRAY,       PH7_TKWRD_AS,        PH7_TKWRD_ARRAY,     PH7_TKWRD_EXIT,` |
|        - |  658 | `    PH7_TKWRD_UNSET,     PH7_TKWRD_XOR,         PH7_TKWRD_OR,        PH7_TKWRD_BREAK` |
|        - |  659 | `  };` |
|        - |  660 | `  int h, i;` |
|  2569188 |  661 | `  if( n<2 ) return PH7_TK_ID;` |
|  2476518 |  662 | `  h = (((int)z[0]*4) ^ ((int)z[n-1]*3) ^ n) % 151;` |
|  3793114 |  663 | `  for(i=((int)aHash[h])-1; i>=0; i=((int)aNext[i])-1){` |
|  2190242 |  664 | `    if( (int)aLen[i]==n && SyMemcmp(&zText[aOffset[i]],z,n)==0 ){` |
|        - |  665 | `       /* PH7_TKWRD_EXTENDS */` |
|        - |  666 | `       /* PH7_TKWRD_ENDSWITCH */` |
|        - |  667 | `       /* PH7_TKWRD_SWITCH */` |
|        - |  668 | `       /* PH7_TKWRD_PRINT */` |
|        - |  669 | `       /* PH7_TKWRD_INT */` |
|        - |  670 | `       /* PH7_TKWRD_REQONCE */` |
|        - |  671 | `       /* PH7_TKWRD_REQUIRE */` |
|        - |  672 | `       /* PH7_TKWRD_SEQ */` |
|        - |  673 | `       /* PH7_TKWRD_ENDDEC */` |
|        - |  674 | `       /* PH7_TKWRD_DECLARE */` |
|        - |  675 | `       /* PH7_TKWRD_RETURN */` |
|        - |  676 | `       /* PH7_TKWRD_NAMESPACE */` |
|        - |  677 | `       /* PH7_TKWRD_ECHO */` |
|        - |  678 | `       /* PH7_TKWRD_OBJECT */` |
|        - |  679 | `       /* PH7_TKWRD_THROW */` |
|        - |  680 | `       /* PH7_TKWRD_BOOL */` |
|        - |  681 | `       /* PH7_TKWRD_BOOL */` |
|        - |  682 | `       /* PH7_TKWRD_AND */` |
|        - |  683 | `       /* PH7_TKWRD_DEFAULT */` |
|        - |  684 | `       /* PH7_TKWRD_TRY */` |
|        - |  685 | `       /* PH7_TKWRD_CASE */` |
|        - |  686 | `       /* PH7_TKWRD_SELF */` |
|        - |  687 | `       /* PH7_TKWRD_FINAL */` |
|        - |  688 | `       /* PH7_TKWRD_LIST */` |
|        - |  689 | `       /* PH7_TKWRD_STATIC */` |
|        - |  690 | `       /* PH7_TKWRD_CLONE */` |
|        - |  691 | `       /* PH7_TKWRD_SNE */` |
|        - |  692 | `       /* PH7_TKWRD_NEW */` |
|        - |  693 | `       /* PH7_TKWRD_CONST */` |
|        - |  694 | `       /* PH7_TKWRD_STRING */` |
|        - |  695 | `       /* PH7_TKWRD_GLOBAL */` |
|        - |  696 | `       /* PH7_TKWRD_USE */` |
|        - |  697 | `       /* PH7_TKWRD_ELIF */` |
|        - |  698 | `       /* PH7_TKWRD_ELSE */` |
|        - |  699 | `       /* PH7_TKWRD_IF */` |
|        - |  700 | `       /* PH7_TKWRD_FLOAT */` |
|        - |  701 | `       /* PH7_TKWRD_VAR */` |
|        - |  702 | `       /* PH7_TKWRD_ARRAY */` |
|        - |  703 | `       /* PH7_TKWRD_AND */` |
|        - |  704 | `       /* PH7_TKWRD_DIE */` |
|        - |  705 | `       /* PH7_TKWRD_ECHO */` |
|        - |  706 | `       /* PH7_TKWRD_USE */` |
|        - |  707 | `       /* PH7_TKWRD_ECHO */` |
|        - |  708 | `       /* PH7_TKWRD_ABSTRACT */` |
|        - |  709 | `       /* PH7_TKWRD_CLASS */` |
|        - |  710 | `       /* PH7_TKWRD_AS */` |
|        - |  711 | `       /* PH7_TKWRD_CONTINUE */` |
|        - |  712 | `       /* PH7_TKWRD_ENDIF */` |
|        - |  713 | `       /* PH7_TKWRD_FUNCTION */` |
|        - |  714 | `       /* PH7_TKWRD_DIE */` |
|        - |  715 | `       /* PH7_TKWRD_ENDWHILE */` |
|        - |  716 | `       /* PH7_TKWRD_WHILE */` |
|        - |  717 | `       /* PH7_TKWRD_EVAL */` |
|        - |  718 | `       /* PH7_TKWRD_DO */` |
|        - |  719 | `       /* PH7_TKWRD_EXIT */` |
|        - |  720 | `       /* PH7_TKWRD_GOTO */` |
|        - |  721 | `       /* PH7_TKWRD_IMPLEMENTS */` |
|        - |  722 | `       /* PH7_TKWRD_INCONCE */` |
|        - |  723 | `       /* PH7_TKWRD_INCLUDE */` |
|        - |  724 | `       /* PH7_TKWRD_EMPTY */` |
|        - |  725 | `       /* PH7_TKWRD_INSTANCEOF */` |
|        - |  726 | `       /* PH7_TKWRD_INTERFACE */` |
|        - |  727 | `       /* PH7_TKWRD_INT */` |
|        - |  728 | `       /* PH7_TKWRD_ENDFOR */` |
|        - |  729 | `       /* PH7_TKWRD_END4EACH */` |
|        - |  730 | `       /* PH7_TKWRD_FOR */` |
|        - |  731 | `       /* PH7_TKWRD_FOREACH */` |
|        - |  732 | `       /* PH7_TKWRD_OR */` |
|        - |  733 | `       /* PH7_TKWRD_ISSET */` |
|        - |  734 | `       /* PH7_TKWRD_PARENT */` |
|        - |  735 | `       /* PH7_TKWRD_PRIVATE */` |
|        - |  736 | `       /* PH7_TKWRD_PROTECTED */` |
|        - |  737 | `       /* PH7_TKWRD_PUBLIC */` |
|        - |  738 | `       /* PH7_TKWRD_CATCH */` |
|        - |  739 | `       /* PH7_TKWRD_UNSET */` |
|        - |  740 | `       /* PH7_TKWRD_XOR */` |
|        - |  741 | `       /* PH7_TKWRD_ARRAY */` |
|        - |  742 | `       /* PH7_TKWRD_AS */` |
|        - |  743 | `       /* PH7_TKWRD_ARRAY */` |
|        - |  744 | `       /* PH7_TKWRD_EXIT */` |
|        - |  745 | `       /* PH7_TKWRD_UNSET */` |
|        - |  746 | `       /* PH7_TKWRD_XOR */` |
|        - |  747 | `       /* PH7_TKWRD_OR */` |
|        - |  748 | `       /* PH7_TKWRD_BREAK */` |
|   873646 |  749 | `      return aCode[i];` |
|        - |  750 | `    }` |
|   658298 |  751 | `  }` |
|        - |  752 | `  /* Linear fallback for keywords not in the auto-generated hash table */` |
|  1602874 |  753 | `  if( n==5 && SyMemcmp(z,"trait",5)==0 ) return PH7_TKWRD_TRAIT;` |
|  1602822 |  754 | `  if( n==9 && SyMemcmp(z,"insteadof",9)==0 ) return PH7_TKWRD_INSTEADOF;` |
|  1602818 |  755 | `  if( n==7 && SyMemcmp(z,"finally",7)==0 ) return PH7_TKWRD_FINALLY;` |
|  1602792 |  756 | `  if( n==5 && SyMemcmp(z,"yield",5)==0 ) return PH7_TKWRD_YIELD;` |
|  1602760 |  757 | `  return PH7_TK_ID;` |
|  1284595 |  758 |  |
|        - |  759 | `/* --- End of Automatically generated code --- */` |
|        - |  760 | `/*` |
|        - |  761 | ` * Extract a heredoc/nowdoc text from a raw PHP input.` |
|        - |  762 | ` * According to the PHP language reference manual:` |
|        - |  763 | ` *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier` |
|        - |  764 | ` *  is provided, then a newline. The string itself follows, and then the same identifier again` |
|        - |  765 | ` *  to close the quotation.` |
|        - |  766 | ` *  The closing identifier must begin in the first column of the line. Also, the identifier must` |
|        - |  767 | ` *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric` |
|        - |  768 | ` *  characters and underscores, and must start with a non-digit character or underscore.` |
|        - |  769 | ` *  Heredoc text behaves just like a double-quoted string, without the double quotes.` |
|        - |  770 | ` *  This means that quotes in a heredoc do not need to be escaped, but the escape codes listed` |
|        - |  771 | ` *  above can still be used. Variables are expanded, but the same care must be taken when expressing` |
|        - |  772 | ` *  complex variables inside a heredoc as with strings.` |
|        - |  773 | ` *  Nowdocs are to single-quoted strings what heredocs are to double-quoted strings.` |
|        - |  774 | ` *  A nowdoc is specified similarly to a heredoc, but no parsing is done inside a nowdoc.` |
|        - |  775 | ` *  The construct is ideal for embedding PHP code or other large blocks of text without the need` |
|        - |  776 | ` *  for escaping. It shares some features in common with the SGML <![CDATA[ ]]> construct, in that` |
|        - |  777 | ` *  it declares a block of text which is not for parsing.` |
|        - |  778 | ` *  A nowdoc is identified with the same <<< sequence used for heredocs, but the identifier which follows` |
|        - |  779 | ` *  is enclosed in single quotes, e.g. <<<'EOT'. All the rules for heredoc identifiers also apply to nowdoc` |
|        - |  780 | ` *  identifiers, especially those regarding the appearance of the closing identifier.` |
|        - |  781 | ` * Symisc Extension:` |
|        - |  782 | ` * The closing delimiter can now start with a digit or undersocre or it can be an UTF-8 stream.` |
|        - |  783 | ` * Example:` |
|        - |  784 | ` *  <<<123` |
|        - |  785 | ` *    HEREDOC Here` |
|        - |  786 | ` * 123` |
|        - |  787 | ` *  or` |
|        - |  788 | ` *  <<<___` |
|        - |  789 | ` *   HEREDOC Here` |
|        - |  790 | ` *  ___` |
|        - |  791 | ` */` |
|       56 |  792 | `static sxi32 LexExtractHeredoc(SyStream *pStream,SyToken *pToken)` |
|        2 |  793 |  |
|       58 |  794 | `	const unsigned char *zIn  = pStream->zText;` |
|       58 |  795 | `	const unsigned char *zEnd = pStream->zEnd;` |
|        - |  796 | `	const unsigned char *zPtr;` |
|       58 |  797 | `	sxu8 bNowDoc = FALSE;` |
|        - |  798 | `	SyString sDelim;` |
|        - |  799 | `	SyString sStr;` |
|        - |  800 | `	/* Jump leading white spaces */` |
|       70 |  801 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|       13 |  802 | `		zIn++;` |
|        1 |  803 | `	}` |
|       58 |  804 | `	if( zIn >= zEnd ){` |
|        - |  805 | `		/* A simple symbol,return immediately */` |
|      ! 0 |  806 | `		return SXERR_CONTINUE;` |
|        - |  807 | `	}` |
|       58 |  808 | `	if( zIn[0] == '\'' \|\| zIn[0] == '"' ){` |
|        - |  809 | `		/* Make sure we are dealing with a nowdoc */` |
|       29 |  810 | `		bNowDoc =  zIn[0] == '\'' ? TRUE : FALSE;` |
|       29 |  811 | `		zIn++;` |
|       14 |  812 | `	}` |
|       58 |  813 | `	if( zIn[0] < 0xc0 && !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|        - |  814 | `		/* Invalid delimiter,return immediately */` |
|      ! 0 |  815 | `		return SXERR_CONTINUE;` |
|        - |  816 | `	}` |
|        - |  817 | `	/* Isolate the identifier */` |
|       58 |  818 | `	sDelim.zString = (const char *)zIn;` |
|       64 |  819 | `	for(;;){` |
|      130 |  820 | `		zPtr = zIn;` |
|        - |  821 | `		/* Skip alphanumeric stream */` |
|      424 |  822 | `		while( zPtr < zEnd && zPtr[0] < 0xc0 && (SyisAlphaNum(zPtr[0]) \|\| zPtr[0] == '_') ){` |
|      232 |  823 | `			zPtr++;` |
|        2 |  824 | `		}` |
|      130 |  825 | `		if( zPtr < zEnd && zPtr[0] >= 0xc0 ){` |
|       19 |  826 | `			zPtr++;` |
|        - |  827 | `			/* UTF-8 stream */` |
|       37 |  828 | `			while( zPtr < zEnd && ((zPtr[0] & 0xc0) == 0x80) ){` |
|       19 |  829 | `				zPtr++;` |
|        1 |  830 | `			}` |
|        9 |  831 | `		}` |
|      130 |  832 | `		if( zPtr == zIn ){` |
|        - |  833 | `			/* Not an UTF-8 or alphanumeric stream */` |
|       58 |  834 | `			break;` |
|        - |  835 | `		}` |
|        - |  836 | `		/* Synchronize pointers */` |
|       74 |  837 | `		zIn = zPtr;` |
|        2 |  838 | `	}` |
|        - |  839 | `	/* Get the identifier length */` |
|       58 |  840 | `	sDelim.nByte = (sxu32)((const char *)zIn-sDelim.zString);` |
|       58 |  841 | `	if( zIn[0] == '"' \|\| (bNowDoc && zIn[0] == '\'') ){` |
|        - |  842 | `		/* Jump the trailing single quote */` |
|       29 |  843 | `		zIn++;` |
|       14 |  844 | `	}` |
|        - |  845 | `	/* Jump trailing white spaces */` |
|       58 |  846 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|      ! 0 |  847 | `		zIn++;` |
|      ! 0 |  848 | `	}` |
|       58 |  849 | `	if( sDelim.nByte <= 0 \|\| zIn >= zEnd \|\| zIn[0] != '\n' ){` |
|        - |  850 | `		/* Invalid syntax */` |
|      ! 0 |  851 | `		return SXERR_CONTINUE;` |
|        - |  852 | `	}` |
|       58 |  853 | `	pStream->nLine++; /* Increment line counter */` |
|       58 |  854 | `	zIn++;` |
|        - |  855 | `	/* Isolate the delimited string */` |
|       58 |  856 | `	sStr.zString = (const char *)zIn;` |
|        - |  857 | `	/* Go and found the closing delimiter */` |
|       75 |  858 | `	for(;;){` |
|        - |  859 | `		/* Synchronize with the next line */` |
|     3018 |  860 | `		while( zIn < zEnd && zIn[0] != '\n' ){` |
|     2868 |  861 | `			zIn++;` |
|        2 |  862 | `		}` |
|      152 |  863 | `		if( zIn >= zEnd ){` |
|        - |  864 | `			/* End of the input reached, break immediately */` |
|       12 |  865 | `			pStream->zText = pStream->zEnd;` |
|       12 |  866 | `			break;` |
|        - |  867 | `		}` |
|      142 |  868 | `		pStream->nLine++; /* Increment line counter */` |
|      142 |  869 | `		zIn++;` |
|      142 |  870 | `		if( (sxu32)(zEnd - zIn) >= sDelim.nByte && SyMemcmp((const void *)sDelim.zString,(const void *)zIn,sDelim.nByte) == 0 ){` |
|       50 |  871 | `			zPtr = &zIn[sDelim.nByte];` |
|       62 |  872 | `			while( zPtr < zEnd && zPtr[0] < 0xc0 && SyisSpace(zPtr[0]) && zPtr[0] != '\n' ){` |
|       13 |  873 | `				zPtr++;` |
|        1 |  874 | `			}` |
|       50 |  875 | `			if( zPtr >= zEnd ){` |
|        - |  876 | `				/* End of input */` |
|      ! 0 |  877 | `				pStream->zText = zPtr;` |
|      ! 0 |  878 | `				break;` |
|        - |  879 | `			}` |
|       50 |  880 | `			if( zPtr[0] == ';' ){` |
|       50 |  881 | `				const unsigned char *zCur = zPtr;` |
|       50 |  882 | `				zPtr++;` |
|       52 |  883 | `				while( zPtr < zEnd && zPtr[0] < 0xc0 && SyisSpace(zPtr[0]) && zPtr[0] != '\n' ){` |
|        3 |  884 | `					zPtr++;` |
|        1 |  885 | `				}` |
|       50 |  886 | `				if( zPtr >= zEnd \|\| zPtr[0] == '\n' ){` |
|        - |  887 | `					/* Closing delimiter found,break immediately */` |
|       48 |  888 | `					pStream->zText = zCur; /* Keep the semi-colon */` |
|       48 |  889 | `					break;` |
|        1 |  890 | `				}` |
|        1 |  891 | `			}else if( zPtr[0] == '\n' ){` |
|        - |  892 | `				/* Closing delimiter found,break immediately */` |
|      ! 0 |  893 | `				pStream->zText = zPtr; /* Synchronize with the stream cursor */` |
|      ! 0 |  894 | `				break;` |
|        - |  895 | `			}` |
|        - |  896 | `			/* Synchronize pointers and continue searching */` |
|        3 |  897 | `			zIn = zPtr;` |
|        1 |  898 | `		}` |
|        2 |  899 | `	} /* For(;;) */` |
|        - |  900 | `	/* Get the delimited string length */` |
|       58 |  901 | `	sStr.nByte = (sxu32)((const char *)zIn-sStr.zString);` |
|        - |  902 | `	/* Record token type and length */` |
|       58 |  903 | `	pToken->nType = bNowDoc ? PH7_TK_NOWDOC : PH7_TK_HEREDOC;` |
|       58 |  904 | `	SyStringDupPtr(&pToken->sData,&sStr);` |
|        - |  905 | `	/* Remove trailing white spaces */` |
|      104 |  906 | `	SyStringRightTrim(&pToken->sData);` |
|        - |  907 | `	/* All done */` |
|       58 |  908 | `	return SXRET_OK;` |
|       30 |  909 |  |
|        - |  910 | `/*` |
|        - |  911 | ` * Tokenize a raw PHP input.` |
|        - |  912 | ` * This is the public tokenizer called by most code generator routines.` |
|        - |  913 | ` */` |
|    12896 |  914 | `PH7_PRIVATE sxi32 PH7_TokenizePHP(const char *zInput,sxu32 nLen,sxu32 nLineStart,SySet *pOut)` |
|        2 |  915 |  |
|        - |  916 | `	SyLex sLexer;` |
|        - |  917 | `	sxi32 rc;` |
|        - |  918 | `	/* Initialize the lexer */` |
|    12898 |  919 | `	rc = SyLexInit(&sLexer,&(*pOut),TokenizePHP,0);` |
|    12898 |  920 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  921 | `		return rc;` |
|        - |  922 | `	}` |
|    12898 |  923 | `	sLexer.sStream.nLine = nLineStart;` |
|        - |  924 | `	/* Tokenize input */` |
|    12898 |  925 | `	rc = SyLexTokenizeInput(&sLexer,zInput,nLen,0,0,0);` |
|        - |  926 | `	/* Release the lexer */` |
|    12898 |  927 | `	SyLexRelease(&sLexer);` |
|        - |  928 | `	/* Tokenization result */` |
|    12898 |  929 | `	return rc;` |
|     6450 |  930 |  |
|        - |  931 | `/*` |
|        - |  932 | ` * High level public tokenizer.` |
|        - |  933 | ` *  Tokenize the input into PHP tokens and raw tokens [i.e: HTML,XML,Raw text...].` |
|        - |  934 | ` * According to the PHP language reference manual` |
|        - |  935 | ` *   When PHP parses a file, it looks for opening and closing tags, which tell PHP` |
|        - |  936 | ` *   to start and stop interpreting the code between them. Parsing in this manner allows` |
|        - |  937 | ` *   PHP to be embedded in all sorts of different documents, as everything outside of a pair` |
|        - |  938 | ` *   of opening and closing tags is ignored by the PHP parser. Most of the time you will see` |
|        - |  939 | ` *   PHP embedded in HTML documents, as in this example.` |
|        - |  940 | ` *   <?php echo 'While this is going to be parsed.'; ?>` |
|        - |  941 | ` *   <p>This will also be ignored.</p>` |
|        - |  942 | ` *   You can also use more advanced structures:` |
|        - |  943 | ` *   Example #1 Advanced escaping` |
|        - |  944 | ` * <?php` |
|        - |  945 | ` * if ($expression) {` |
|        - |  946 | ` *   ?>` |
|        - |  947 | ` *   <strong>This is true.</strong>` |
|        - |  948 | ` *   <?php` |
|        - |  949 | ` * } else {` |
|        - |  950 | ` *   ?>` |
|        - |  951 | ` *   <strong>This is false.</strong>` |
|        - |  952 | ` *   <?php` |
|        - |  953 | ` * }` |
|        - |  954 | ` * ?>` |
|        - |  955 | ` * This works as expected, because when PHP hits the ?> closing tags, it simply starts outputting` |
|        - |  956 | ` * whatever it finds (except for an immediately following newline - see instruction separation ) until it hits` |
|        - |  957 | ` * another opening tag. The example given here is contrived, of course, but for outputting large blocks of text` |
|        - |  958 | ` * dropping out of PHP parsing mode is generally more efficient than sending all of the text through echo() or print().` |
|        - |  959 | ` * There are four different pairs of opening and closing tags which can be used in PHP. Three of those, <?php ?>` |
|        - |  960 | ` * <script language="php"> </script>  and <? ?> are always available. The other two are short tags and ASP style` |
|        - |  961 | ` * tags, and can be turned on and off from the php.ini configuration file. As such, while some people find short tags` |
|        - |  962 | ` * and ASP style tags convenient, they are less portable, and generally not recommended.` |
|        - |  963 | ` * Note:` |
|        - |  964 | ` * Also note that if you are embedding PHP within XML or XHTML you will need to use the <?php ?> tags to remain` |
|        - |  965 | ` * compliant with standards.` |
|        - |  966 | ` * Example #2 PHP Opening and Closing Tags` |
|        - |  967 | ` * 1.  <?php echo 'if you want to serve XHTML or XML documents, do it like this'; ?>` |
|        - |  968 | ` * 2.  <script language="php">` |
|        - |  969 | ` *       echo 'some editors (like FrontPage) don\'t` |
|        - |  970 | ` *             like processing instructions';` |
|        - |  971 | ` *   </script>` |
|        - |  972 | ` *` |
|        - |  973 | ` * 3.  <? echo 'this is the simplest, an SGML processing instruction'; ?>` |
|        - |  974 | ` *   <?= expression ?> This is a shortcut for "<? echo expression ?>"` |
|        - |  975 | ` */` |
|    10404 |  976 | `PH7_PRIVATE sxi32 PH7_TokenizeRawText(const char *zInput,sxu32 nLen,SySet *pOut)` |
|        2 |  977 |  |
|    10406 |  978 | `	const char *zEnd = &zInput[nLen];` |
|    10406 |  979 | `	const char *zIn  = zInput;` |
|        - |  980 | `	const char *zCur,*zCurEnd;` |
|    10406 |  981 | `	SyString sCtag = { 0, 0 };     /* Closing tag */` |
|        - |  982 | `	SyToken sToken;` |
|        - |  983 | `	SyString sDoc;` |
|        - |  984 | `	sxu32 nLine;` |
|        - |  985 | `	sxi32 iNest;` |
|        - |  986 | `	sxi32 rc;` |
|        - |  987 | `	/* Tokenize the input into PHP tokens and raw tokens */` |
|    10406 |  988 | `	nLine = 1;` |
|    10406 |  989 | `	zCur = zCurEnd   = 0; /* Prevent compiler warning */` |
|    10406 |  990 | `	sToken.pUserData = 0;` |
|    10406 |  991 | `	iNest = 0;` |
|    10406 |  992 | `	sDoc.nByte = 0;` |
|    10406 |  993 | `	sDoc.zString = ""; /* cc warning */` |
|    10406 |  994 | `	for(;;){` |
|    20814 |  995 | `		if( zIn >= zEnd ){` |
|        - |  996 | `			/* End of input reached */` |
|    10402 |  997 | `			break;` |
|        - |  998 | `		}` |
|    10414 |  999 | `		sToken.nLine = nLine;` |
|    10414 | 1000 | `		zCur = zIn;` |
|    10414 | 1001 | `		zCurEnd = 0;` |
|    10422 | 1002 | `		while( zIn < zEnd ){` |
|    10418 | 1003 | `			 if( zIn[0] == '<' ){` |
|    10410 | 1004 | `				const char *zTmp = zIn; /* End of raw input marker */` |
|    10410 | 1005 | `				zIn++;` |
|    10410 | 1006 | `				if( zIn < zEnd ){` |
|    10410 | 1007 | `					if( zIn[0] == '?' ){` |
|    10410 | 1008 | `						zIn++;` |
|    10410 | 1009 | `						if( (sxu32)(zEnd - zIn) >= sizeof("php")-1 &&  SyStrnicmp(zIn,"php",sizeof("php")-1) == 0 ){` |
|        - | 1010 | `							/* opening tag: <?php */` |
|    10408 | 1011 | `							zIn += sizeof("php")-1;` |
|     5203 | 1012 | `						}` |
|        - | 1013 | `						/* Look for the closing tag '?>' */` |
|    10410 | 1014 | `						SyStringInitFromBuf(&sCtag,"?>",sizeof("?>")-1);` |
|    10410 | 1015 | `						zCurEnd = zTmp;` |
|    10410 | 1016 | `						break;` |
|        - | 1017 | `					}` |
|      ! 0 | 1018 | `				}` |
|      ! 0 | 1019 | `			}else{` |
|       10 | 1020 | `				if( zIn[0] == '\n' ){` |
|       10 | 1021 | `					nLine++;` |
|        4 | 1022 | `				}` |
|       10 | 1023 | `				zIn++;` |
|        - | 1024 | `			 }` |
|        2 | 1025 | `		} /* While(zIn < zEnd) */` |
|    10414 | 1026 | `		if( zCurEnd == 0 ){` |
|        5 | 1027 | `			zCurEnd = zIn;` |
|        2 | 1028 | `		}` |
|        - | 1029 | `		/* Save the raw token */` |
|    10414 | 1030 | `		SyStringInitFromBuf(&sToken.sData,zCur,zCurEnd - zCur);` |
|    10414 | 1031 | `		sToken.nType = PH7_TOKEN_RAW;` |
|    10414 | 1032 | `		rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|    10414 | 1033 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1034 | `			return rc;` |
|        - | 1035 | `		}` |
|    10414 | 1036 | `		if( zIn >= zEnd ){` |
|        5 | 1037 | `			break;` |
|        - | 1038 | `		}` |
|        - | 1039 | `		/* Ignore leading white space */` |
|    22568 | 1040 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    12160 | 1041 | `			if( zIn[0] == '\n' ){` |
|    10966 | 1042 | `				nLine++;` |
|     5482 | 1043 | `			}` |
|    12160 | 1044 | `			zIn++;` |
|        2 | 1045 | `		}` |
|        - | 1046 | `		/* Delimit the PHP chunk */` |
|    10410 | 1047 | `		sToken.nLine = nLine;` |
|    10410 | 1048 | `		zCur = zIn;` |
|   941952 | 1049 | `		while( (sxu32)(zEnd - zIn) >= sCtag.nByte ){` |
|        - | 1050 | `			const char *zPtr;` |
|   937488 | 1051 | `			if( SyMemcmp(zIn,sCtag.zString,sCtag.nByte) == 0 && iNest < 1 ){` |
|     5946 | 1052 | `				break;` |
|        - | 1053 | `			}` |
|   467672 | 1054 | `			for(;;){` |
|   935346 | 1055 | `				if( zIn[0] != '/' \|\| (zIn[1] != '*' && zIn[1] != '/') /* && sCtag.nByte >= 2 */ ){` |
|   465773 | 1056 | `					break;` |
|        - | 1057 | `				}` |
|     3804 | 1058 | `				zIn += 2;` |
|     3804 | 1059 | `				if( zIn[-1] == '/' ){` |
|        - | 1060 | `					/* Inline comment */` |
|   130496 | 1061 | `					while( zIn < zEnd && zIn[0] != '\n' ){` |
|   126776 | 1062 | `						zIn++;` |
|        2 | 1063 | `					}` |
|     3722 | 1064 | `					if( zIn >= zEnd ){` |
|      ! 0 | 1065 | `						zIn--;` |
|      ! 0 | 1066 | `					}` |
|     1862 | 1067 | `				}else{` |
|        - | 1068 | `					/* Block comment */` |
|     4500 | 1069 | `					while( (sxu32)(zEnd-zIn) >= sizeof("*/") - 1 ){` |
|     4500 | 1070 | `						if( zIn[0] == '*' && zIn[1] == '/' ){` |
|       84 | 1071 | `							zIn += 2;` |
|       84 | 1072 | `							break;` |
|        - | 1073 | `						}` |
|     4418 | 1074 | `						if( zIn[0] == '\n' ){` |
|       28 | 1075 | `							nLine++;` |
|       13 | 1076 | `						}` |
|     4418 | 1077 | `						zIn++;` |
|        2 | 1078 | `					}` |
|        - | 1079 | `				}` |
|        2 | 1080 | `			}` |
|   931544 | 1081 | `			if( zIn[0] == '\n' ){` |
|    32340 | 1082 | `				nLine++;` |
|    32340 | 1083 | `				if( iNest > 0 ){` |
|      156 | 1084 | `					zIn++;` |
|      156 | 1085 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|      ! 0 | 1086 | `						zIn++;` |
|      ! 0 | 1087 | `					}` |
|      156 | 1088 | `					zPtr = zIn;` |
|      864 | 1089 | `					while( zIn < zEnd ){` |
|      864 | 1090 | `						if( (unsigned char)zIn[0] >= 0xc0 ){` |
|        - | 1091 | `							/* UTF-8 stream */` |
|       19 | 1092 | `							zIn++;` |
|       37 | 1093 | `							SX_JMP_UTF8(zIn,zEnd);` |
|      854 | 1094 | `						}else if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       79 | 1095 | `							break;` |
|      ! 0 | 1096 | `						}else{` |
|      692 | 1097 | `							zIn++;` |
|        - | 1098 | `						}` |
|        2 | 1099 | `					}` |
|      156 | 1100 | `					if( (sxu32)(zIn - zPtr) == sDoc.nByte && SyMemcmp(sDoc.zString,zPtr,sDoc.nByte) == 0 ){` |
|       60 | 1101 | `						iNest = 0;` |
|       29 | 1102 | `					}` |
|      156 | 1103 | `					continue;` |
|        2 | 1104 | `				}` |
|   915298 | 1105 | `			}else if ( (sxu32)(zEnd - zIn) >= sizeof("<<<") && zIn[0] == '<' && zIn[1] == '<' && zIn[2] == '<' && iNest < 1){` |
|       62 | 1106 | `				zIn += sizeof("<<<")-1;` |
|       74 | 1107 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|       13 | 1108 | `					zIn++;` |
|        1 | 1109 | `				}` |
|       62 | 1110 | `				if( zIn[0] == '"' \|\| zIn[0] == '\'' ){` |
|       32 | 1111 | `					zIn++;` |
|       15 | 1112 | `				}` |
|       62 | 1113 | `				zPtr = zIn;` |
|      330 | 1114 | `				while( zIn < zEnd ){` |
|      330 | 1115 | `					if( (unsigned char)zIn[0] >= 0xc0 ){` |
|        - | 1116 | `						/* UTF-8 stream */` |
|       19 | 1117 | `						zIn++;` |
|       37 | 1118 | `						SX_JMP_UTF8(zIn,zEnd);` |
|      320 | 1119 | `					}else if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       32 | 1120 | `						break;` |
|      ! 0 | 1121 | `					}else{` |
|      252 | 1122 | `						zIn++;` |
|        - | 1123 | `					}` |
|        2 | 1124 | `				}` |
|       62 | 1125 | `				SyStringInitFromBuf(&sDoc,zPtr,zIn-zPtr);` |
|       62 | 1126 | `				SyStringFullTrim(&sDoc);` |
|       62 | 1127 | `				if( sDoc.nByte > 0 ){` |
|       62 | 1128 | `					iNest++;` |
|       30 | 1129 | `				}` |
|       62 | 1130 | `				continue;` |
|        - | 1131 | `			}` |
|   931330 | 1132 | `			zIn++;` |
|        - | 1133 |  |
|   931330 | 1134 | `			if ( zIn >= zEnd )` |
|      ! 0 | 1135 | `				break;` |
|        2 | 1136 | `		}` |
|    10410 | 1137 | `		if( (sxu32)(zEnd - zIn) < sCtag.nByte ){` |
|     4466 | 1138 | `			zIn = zEnd;` |
|     2232 | 1139 | `		}` |
|    10410 | 1140 | `		if( zCur < zIn ){` |
|        - | 1141 | `			/* Save the PHP chunk for later processing */` |
|     8484 | 1142 | `			sToken.nType = PH7_TOKEN_PHP;` |
|     8484 | 1143 | `			SyStringInitFromBuf(&sToken.sData,zCur,zIn-zCur);` |
|    16942 | 1144 | `			SyStringRightTrim(&sToken.sData); /* Trim trailing white spaces */` |
|     8484 | 1145 | `			rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|     8484 | 1146 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1147 | `				return rc;` |
|        - | 1148 | `			}` |
|     4241 | 1149 | `		}` |
|    10410 | 1150 | `		if( zIn < zEnd ){` |
|        - | 1151 | `			/* Jump the trailing closing tag */` |
|     5946 | 1152 | `			zIn += sCtag.nByte;` |
|     2972 | 1153 | `		}` |
|        2 | 1154 | `	} /* For(;;) */` |
|        - | 1155 |  |
|    10406 | 1156 | ` 	return SXRET_OK;` |
|     5204 | 1157 |  |
|        - | 1158 |  |
