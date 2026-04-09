# src/ph7/lex.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 643/678 lines (94.84%)

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
|  6812340 |   19 | `static sxi32 TokenizePHP(SyStream *pStream,SyToken *pToken,void *pUserData,void *pCtxData)` |
|        2 |   20 |  |
|        - |   21 | `	SyString *pStr;` |
|        - |   22 | `	sxi32 rc;` |
|        - |   23 | `	/* Ignore leading white spaces */` |
| 10262196 |   24 | `	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){` |
|        - |   25 | `		/* Advance the stream cursor */` |
|  3449856 |   26 | `		if( pStream->zText[0] == '\n' ){` |
|        - |   27 | `			/* Update line counter */` |
|    29240 |   28 | `			pStream->nLine++;` |
|    14619 |   29 | `		}` |
|  3449856 |   30 | `		pStream->zText++;` |
|        2 |   31 | `	}` |
|  6812342 |   32 | `	if( pStream->zText >= pStream->zEnd ){` |
|        - |   33 | `		/* End of input reached */` |
|      ! 0 |   34 | `		return SXERR_EOF;` |
|        - |   35 | `	}` |
|        - |   36 | `	/* Record token starting position and line */` |
|  6812342 |   37 | `	pToken->nLine = pStream->nLine;` |
|  6812342 |   38 | `	pToken->pUserData = 0;` |
|  6812342 |   39 | `	pStr = &pToken->sData;` |
|  6812342 |   40 | `	SyStringInitFromBuf(pStr,pStream->zText,0);` |
|  8043018 |   41 | `	if( pStream->zText[0] >= 0xc0 \|\| SyisAlpha(pStream->zText[0]) \|\| pStream->zText[0] == '_' ){` |
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
|  2461354 |   52 | `		if( pStream->zText[0] < 0xc0 ){` |
|  2461338 |   53 | `			pStream->zText++;` |
|  1230668 |   54 | `		}` |
|  2416908 |   55 | `		for(;;){` |
|  4833818 |   56 | `			zIn = pStream->zText;` |
|  4833818 |   57 | `			if( zIn[0] >= 0xc0 ){` |
|       49 |   58 | `				zIn++;` |
|        - |   59 | `				/* UTF-8 stream */` |
|      109 |   60 | `				while( zIn < pStream->zEnd && ((zIn[0] & 0xc0) == 0x80) ){` |
|       61 |   61 | `					zIn++;` |
|        1 |   62 | `				}` |
|       24 |   63 | `			}` |
|        - |   64 | `			/* Skip alphanumeric stream */` |
| 19840650 |   65 | `			while( zIn < pStream->zEnd && zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_') ){` |
| 12589926 |   66 | `				zIn++;` |
|        2 |   67 | `			}` |
|  4833818 |   68 | `			if( zIn == pStream->zText ){` |
|        - |   69 | `				/* Not an UTF-8 or alphanumeric stream */` |
|  2461354 |   70 | `				break;` |
|        - |   71 | `			}` |
|        - |   72 | `			/* Synchronize pointers */` |
|  2372466 |   73 | `			pStream->zText = zIn;` |
|        2 |   74 | `		}` |
|        - |   75 | `		/* Record token length */` |
|  2461354 |   76 | `		pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  2461354 |   77 | `		nKeyword = KeywordCode(pStr->zString,(int)pStr->nByte);` |
|  2461354 |   78 | `		if( nKeyword != PH7_TK_ID ){` |
|   841040 |   79 | `			if( nKeyword &` |
|        - |   80 | `				(PH7_TKWRD_NEW\|PH7_TKWRD_CLONE\|PH7_TKWRD_AND\|PH7_TKWRD_XOR\|PH7_TKWRD_OR\|PH7_TKWRD_INSTANCEOF\|PH7_TKWRD_SEQ\|PH7_TKWRD_SNE) ){` |
|        - |   81 | `					/* Alpha stream operators [i.e: new,clone,and,instanceof,eq,ne,or,xor],save the operator instance for later processing */` |
|    13796 |   82 | `					pToken->pUserData = (void *)PH7_ExprExtractOperator(pStr,0);` |
|        - |   83 | `					/* Mark as an operator */` |
|    13796 |   84 | `					pToken->nType = PH7_TK_ID\|PH7_TK_OP;` |
|     6899 |   85 | `			}else{` |
|        - |   86 | `				/* We are dealing with a keyword [i.e: while,foreach,class...],save the keyword ID */` |
|   827246 |   87 | `				pToken->nType = PH7_TK_KEYWORD;` |
|   827246 |   88 | `				pToken->pUserData = SX_INT_TO_PTR(nKeyword);` |
|        - |   89 | `			}` |
|   420521 |   90 | `		}else{` |
|        - |   91 | `			/* A simple identifier */` |
|  1620316 |   92 | `			pToken->nType = PH7_TK_ID;` |
|        - |   93 | `		}` |
|  1230678 |   94 | `	}else{` |
|        - |   95 | `		sxi32 c;` |
|        - |   96 | `		/* Non-alpha stream */` |
|  4384461 |   97 | `		if( pStream->zText[0] == '#' \|\|` |
|  4350988 |   98 | `			( pStream->zText[0] == '/' &&  &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '/') ){` |
|     3730 |   99 | `				pStream->zText++;` |
|        - |  100 | `				/* Inline comments */` |
|   134446 |  101 | `				while( pStream->zText < pStream->zEnd && pStream->zText[0] != '\n' ){` |
|   130718 |  102 | `					pStream->zText++;` |
|        2 |  103 | `				}` |
|        - |  104 | `				/* Tell the upper-layer to ignore this token */` |
|     3730 |  105 | `				return SXERR_CONTINUE;` |
|  4347262 |  106 | `		}else if( pStream->zText[0] == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '*' ){` |
|    63156 |  107 | `			pStream->zText += 2;` |
|        - |  108 | `			/* Block comment */` |
|  1791540 |  109 | `			while( pStream->zText < pStream->zEnd ){` |
|  1791540 |  110 | `				if( pStream->zText[0] == '*' ){` |
|    63182 |  111 | `					if( &pStream->zText[1] >= pStream->zEnd \|\| pStream->zText[1] == '/'  ){` |
|    31579 |  112 | `						break;` |
|        - |  113 | `					}` |
|       13 |  114 | `				}` |
|  1728386 |  115 | `				if( pStream->zText[0] == '\n' ){` |
|       28 |  116 | `					pStream->nLine++;` |
|       13 |  117 | `				}` |
|  1728386 |  118 | `				pStream->zText++;` |
|        2 |  119 | `			}` |
|    63156 |  120 | `			pStream->zText += 2;` |
|        - |  121 | `			/* Tell the upper-layer to ignore this token */` |
|    63156 |  122 | `			return SXERR_CONTINUE;` |
|  4284108 |  123 | `		}else if( SyisDigit(pStream->zText[0]) ){` |
|    87034 |  124 | `			pStream->zText++;` |
|        - |  125 | `			/* Decimal digit stream */` |
|    95228 |  126 | `			while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|     8196 |  127 | `				pStream->zText++;` |
|        2 |  128 | `			}` |
|        - |  129 | `			/* Mark the token as integer until we encounter a real number */` |
|    87034 |  130 | `			pToken->nType = PH7_TK_INTEGER;` |
|    87034 |  131 | `			if( pStream->zText < pStream->zEnd ){` |
|    87034 |  132 | `				c = pStream->zText[0];` |
|    87034 |  133 | `				if( c == '.' ){` |
|        - |  134 | `					/* Real number */` |
|      402 |  135 | `					pStream->zText++;` |
|     1596 |  136 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|     1196 |  137 | `						pStream->zText++;` |
|        2 |  138 | `					}` |
|      402 |  139 | `					if( pStream->zText < pStream->zEnd ){` |
|      402 |  140 | `						c = pStream->zText[0];` |
|      402 |  141 | `						if( c=='e' \|\| c=='E' ){` |
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
|      200 |  154 | `					}` |
|      402 |  155 | `					pToken->nType = PH7_TK_REAL;` |
|    86834 |  156 | `				}else if( c=='e' \|\| c=='E' ){` |
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
|    86627 |  171 | `				}else if( c == 'x' \|\| c == 'X' ){` |
|        - |  172 | `					/* Hex digit stream */` |
|       26 |  173 | `					pStream->zText++;` |
|       74 |  174 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisHex(pStream->zText[0]) ){` |
|       49 |  175 | `						pStream->zText++;` |
|        1 |  176 | `					}` |
|    86608 |  177 | `				}else if(c  == 'b' \|\| c == 'B' ){` |
|        - |  178 | `					/* Binary digit stream */` |
|      233 |  179 | `					pStream->zText++;` |
|     1319 |  180 | `					while( pStream->zText < pStream->zEnd && (pStream->zText[0] == '0' \|\| pStream->zText[0] == '1') ){` |
|      971 |  181 | `						pStream->zText++;` |
|        1 |  182 | `					}` |
|      116 |  183 | `				}` |
|    43516 |  184 | `			}` |
|        - |  185 | `			/* Record token length */` |
|    87034 |  186 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|    87034 |  187 | `			return SXRET_OK;` |
|        - |  188 | `		}` |
|  4197076 |  189 | `		c = pStream->zText[0];` |
|  4197076 |  190 | `		pStream->zText++; /* Advance the stream cursor */` |
|        - |  191 | `		/* Assume we are dealing with an operator*/` |
|  4197076 |  192 | `		pToken->nType = PH7_TK_OP;` |
|  4197076 |  193 | `		switch(c){` |
|   880202 |  194 | `		case '$': pToken->nType = PH7_TK_DOLLAR; break;` |
|   333614 |  195 | `		case '{': pToken->nType = PH7_TK_OCB;    break;` |
|   333600 |  196 | `		case '}': pToken->nType = PH7_TK_CCB;    break;` |
|   664032 |  197 | `		case '(': pToken->nType = PH7_TK_LPAREN; break;` |
|    67738 |  198 | `		case '[': pToken->nType \|= PH7_TK_OSB;   break; /* Bitwise operation here,since the square bracket token '['` |
|        - |  199 | `														 * is a potential operator [i.e: subscripting] */` |
|    67744 |  200 | `		case ']': pToken->nType = PH7_TK_CSB;    break;` |
|   332009 |  201 | `		case ')': {` |
|   664020 |  202 | `			SySet *pTokSet = pStream->pSet;` |
|        - |  203 | `			/* Assemble type cast operators [i.e: (int),(float),(bool)...] */` |
|   664020 |  204 | `			if( pTokSet->nUsed >= 2 ){` |
|        - |  205 | `				SyToken *pTmp;` |
|        - |  206 | `				/* Peek the last recongnized token */` |
|   664018 |  207 | `				pTmp = (SyToken *)SySetPeek(pTokSet);` |
|   664018 |  208 | `				if( pTmp->nType & PH7_TK_KEYWORD ){` |
|    13458 |  209 | `					sxi32 nID = SX_PTR_TO_INT(pTmp->pUserData);` |
|    13458 |  210 | `					if( (sxu32)nID & (PH7_TKWRD_ARRAY\|PH7_TKWRD_INT\|PH7_TKWRD_FLOAT\|PH7_TKWRD_STRING\|PH7_TKWRD_OBJECT\|PH7_TKWRD_BOOL\|PH7_TKWRD_UNSET) ){` |
|    13358 |  211 | `						pTmp = (SyToken *)SySetAt(pTokSet,pTokSet->nUsed - 2);` |
|    13358 |  212 | `						if( pTmp->nType & PH7_TK_LPAREN ){` |
|        - |  213 | `							/* Merge the three tokens '(' 'TYPE' ')' into a single one */` |
|    13274 |  214 | `							const char * zTypeCast = "(int)";` |
|    13274 |  215 | `							if( nID & PH7_TKWRD_FLOAT ){` |
|     2636 |  216 | `								zTypeCast = "(float)";` |
|    11957 |  217 | `							}else if( nID & PH7_TKWRD_BOOL ){` |
|     2638 |  218 | `								zTypeCast = "(bool)";` |
|     9322 |  219 | `							}else if( nID & PH7_TKWRD_STRING ){` |
|     5266 |  220 | `								zTypeCast = "(string)";` |
|     5372 |  221 | `							}else if( nID & PH7_TKWRD_ARRAY ){` |
|       21 |  222 | `								zTypeCast = "(array)";` |
|     2730 |  223 | `							}else if( nID & PH7_TKWRD_OBJECT ){` |
|       17 |  224 | `								zTypeCast = "(object)";` |
|     2712 |  225 | `							}else if( nID & PH7_TKWRD_UNSET ){` |
|        7 |  226 | `								zTypeCast = "(unset)";` |
|        3 |  227 | `							}` |
|        - |  228 | `							/* Reflect the change */` |
|    13274 |  229 | `							pToken->nType = PH7_TK_OP;` |
|    13274 |  230 | `							SyStringInitFromBuf(&pToken->sData,zTypeCast,SyStrlen(zTypeCast));` |
|        - |  231 | `							/* Save the instance associated with the type cast operator */` |
|    13274 |  232 | `							pToken->pUserData = (void *)PH7_ExprExtractOperator(&pToken->sData,0);` |
|        - |  233 | `							/* Remove the two previous tokens */` |
|    13274 |  234 | `							pTokSet->nUsed -= 2;` |
|    13274 |  235 | `							return SXRET_OK;` |
|        - |  236 | `						}` |
|       42 |  237 | `					}` |
|       92 |  238 | `				}` |
|   325372 |  239 | `			}` |
|   650748 |  240 | `			pToken->nType = PH7_TK_RPAREN;` |
|   650748 |  241 | `			break;` |
|        - |  242 | `				  }` |
|    28376 |  243 | `		case '\'':{` |
|        - |  244 | `			/* Single quoted string */` |
|    56754 |  245 | `			pStr->zString++;` |
|   714908 |  246 | `			while( pStream->zText < pStream->zEnd ){` |
|   714908 |  247 | `				if( pStream->zText[0] == '\''  ){` |
|    56764 |  248 | `					if( pStream->zText[-1] != '\\' ){` |
|    56740 |  249 | `						break;` |
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
|   658156 |  262 | `				if( pStream->zText[0] == '\n' ){` |
|       67 |  263 | `					pStream->nLine++;` |
|       33 |  264 | `				}` |
|   658156 |  265 | `				pStream->zText++;` |
|        2 |  266 | `			}` |
|        - |  267 | `			/* Record token length and type */` |
|    56754 |  268 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|    56754 |  269 | `			pToken->nType = PH7_TK_SSTR;` |
|        - |  270 | `			/* Jump the trailing single quote */` |
|    56754 |  271 | `			pStream->zText++;` |
|    56754 |  272 | `			return SXRET_OK;` |
|        - |  273 | `				  }` |
|     7702 |  274 | `		case '"':{` |
|        - |  275 | `			sxi32 iNest;` |
|        - |  276 | `			/* Double quoted string */` |
|    15406 |  277 | `			pStr->zString++;` |
|   154568 |  278 | `			while( pStream->zText < pStream->zEnd ){` |
|   154568 |  279 | `				if( pStream->zText[0] == '{' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '$'){` |
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
|   154568 |  301 | `				if( pStream->zText[0] == '"' ){` |
|    15506 |  302 | `					if( pStream->zText[-1] != '\\' ){` |
|    15402 |  303 | `						break;` |
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
|   139164 |  316 | `				if( pStream->zText[0] == '\n' ){` |
|        7 |  317 | `					pStream->nLine++;` |
|        3 |  318 | `				}` |
|   139164 |  319 | `				pStream->zText++;` |
|        2 |  320 | `			}` |
|        - |  321 | `			/* Record token length and type */` |
|    15406 |  322 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|    15406 |  323 | `			pToken->nType = PH7_TK_DSTR;` |
|        - |  324 | `			/* Jump the trailing quote */` |
|    15406 |  325 | `			pStream->zText++;` |
|    15406 |  326 | `			return SXRET_OK;` |
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
|      166 |  347 | `		case '\\': pToken->nType = PH7_TK_NSSEP;  break;` |
|     1191 |  348 | `		case ':':` |
|     2384 |  349 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == ':' ){` |
|        - |  350 | `				/* Current operator: '::' */` |
|      210 |  351 | `				pStream->zText++;` |
|      106 |  352 | `			}else{` |
|     2176 |  353 | `				pToken->nType = PH7_TK_COLON; /* Single colon */` |
|        - |  354 | `			}` |
|     2384 |  355 | `			break;` |
|    71318 |  356 | `		case ',': pToken->nType \|= PH7_TK_COMMA;  break; /* Comma is also an operator */` |
|   477372 |  357 | `		case ';': pToken->nType = PH7_TK_SEMI;    break;` |
|        - |  358 | `			/* Handle combined operators [i.e: +=,===,!=== ...] */` |
|   137931 |  359 | `		case '=':` |
|   275864 |  360 | `			pToken->nType \|= PH7_TK_EQUAL;` |
|   275864 |  361 | `			if( pStream->zText < pStream->zEnd ){` |
|   275864 |  362 | `				if( pStream->zText[0] == '=' ){` |
|    17214 |  363 | `					pToken->nType &= ~PH7_TK_EQUAL;` |
|        - |  364 | `					/* Current operator: == */` |
|    17214 |  365 | `					pStream->zText++;` |
|    17214 |  366 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  367 | `						/* Current operator: === */` |
|     3766 |  368 | `						pStream->zText++;` |
|     1884 |  369 | `					}` |
|   267258 |  370 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  371 | `					/* Array operator: => */` |
|     3984 |  372 | `					pToken->nType = PH7_TK_ARRAY_OP;` |
|     3984 |  373 | `					pStream->zText++;` |
|     1993 |  374 | `				}else{` |
|        - |  375 | `					/* TICKET 1433-0010: Reference operator '=&' */` |
|   254670 |  376 | `					const unsigned char *zCur = pStream->zText;` |
|   254670 |  377 | `					sxu32 nLine = 0;` |
|   509316 |  378 | `					while( zCur < pStream->zEnd && zCur[0] < 0xc0 && SyisSpace(zCur[0]) ){` |
|   254648 |  379 | `						if( zCur[0] == '\n' ){` |
|        5 |  380 | `							nLine++;` |
|        2 |  381 | `						}` |
|   254648 |  382 | `						zCur++;` |
|        2 |  383 | `					}` |
|   254670 |  384 | `					if( zCur < pStream->zEnd && zCur[0] == '&' ){` |
|        - |  385 | `						/* Current operator: =& */` |
|       46 |  386 | `						pToken->nType &= ~PH7_TK_EQUAL;` |
|       46 |  387 | `						SyStringInitFromBuf(pStr,"=&",sizeof("=&")-1);` |
|        - |  388 | `						/* Update token stream */` |
|       46 |  389 | `						pStream->zText = &zCur[1];` |
|       46 |  390 | `						pStream->nLine += nLine;` |
|       22 |  391 | `					}` |
|        - |  392 | `				}` |
|   137931 |  393 | `			}` |
|   275864 |  394 | `			break;` |
|    18745 |  395 | `		case '!':` |
|    37492 |  396 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  397 | `				/* Current operator: != */` |
|    15950 |  398 | `				pStream->zText++;` |
|    15950 |  399 | `				if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  400 | `					/* Current operator: !== */` |
|    13290 |  401 | `					pStream->zText++;` |
|     6644 |  402 | `				}` |
|     7974 |  403 | `			}` |
|    37492 |  404 | `			break;` |
|    10776 |  405 | `		case '&':` |
|    21554 |  406 | `			pToken->nType \|= PH7_TK_AMPER;` |
|    21554 |  407 | `			if( pStream->zText < pStream->zEnd ){` |
|    21554 |  408 | `				if( pStream->zText[0] == '&' ){` |
|     8286 |  409 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|        - |  410 | `					/* Current operator: && */` |
|     8286 |  411 | `					pStream->zText++;` |
|    17412 |  412 | `				}else if( pStream->zText[0] == '=' ){` |
|        7 |  413 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|        - |  414 | `					/* Current operator: &= */` |
|        7 |  415 | `					pStream->zText++;` |
|        3 |  416 | `				}` |
|    10776 |  417 | `			}` |
|    21554 |  418 | `			break;` |
|     1402 |  419 | `		case '\|':` |
|     2806 |  420 | `			if( pStream->zText < pStream->zEnd ){` |
|     2806 |  421 | `				if( pStream->zText[0] == '\|' ){` |
|        - |  422 | `					/* Current operator: \|\| */` |
|     2762 |  423 | `					pStream->zText++;` |
|     1426 |  424 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  425 | `					/* Current operator: \|= */` |
|        7 |  426 | `					pStream->zText++;` |
|        3 |  427 | `				}` |
|     1402 |  428 | `			}` |
|     2806 |  429 | `			break;` |
|     6908 |  430 | `		case '+':` |
|    13818 |  431 | `			if( pStream->zText < pStream->zEnd ){` |
|    13816 |  432 | `				if( pStream->zText[0] == '+' ){` |
|        - |  433 | `					/* Current operator: ++ */` |
|    10744 |  434 | `					pStream->zText++;` |
|     8445 |  435 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  436 | `					/* Current operator: += */` |
|       40 |  437 | `					pStream->zText++;` |
|       19 |  438 | `				}` |
|     6907 |  439 | `			}` |
|    13818 |  440 | `			break;` |
|    50583 |  441 | `		case '-':` |
|   101168 |  442 | `			if( pStream->zText < pStream->zEnd ){` |
|   101168 |  443 | `				if( pStream->zText[0] == '-' ){` |
|        - |  444 | `					/* Current operator: -- */` |
|        5 |  445 | `					pStream->zText++;` |
|   101166 |  446 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  447 | `					/* Current operator: -= */` |
|        5 |  448 | `					pStream->zText++;` |
|   101162 |  449 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  450 | `					/* Current operator: -> */` |
|   100684 |  451 | `					pStream->zText++;` |
|    50341 |  452 | `				}` |
|    50583 |  453 | `			}` |
|   101168 |  454 | `			break;` |
|       76 |  455 | `		case '*':` |
|      154 |  456 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  457 | `				/* Current operator: *= */` |
|       15 |  458 | `				pStream->zText++;` |
|        7 |  459 | `			}` |
|      154 |  460 | `			break;` |
|       30 |  461 | `		case '/':` |
|       62 |  462 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  463 | `				/* Current operator: /= */` |
|        3 |  464 | `				pStream->zText++;` |
|        1 |  465 | `			}` |
|       62 |  466 | `			break;` |
|       24 |  467 | `		case '%':` |
|       50 |  468 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  469 | `				/* Current operator: %= */` |
|        3 |  470 | `				pStream->zText++;` |
|        1 |  471 | `			}` |
|       50 |  472 | `			break;` |
|       11 |  473 | `		case '^':` |
|       23 |  474 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  475 | `				/* Current operator: ^= */` |
|        9 |  476 | `				pStream->zText++;` |
|        4 |  477 | `			}` |
|       23 |  478 | `			break;` |
|    28516 |  479 | `		case '.':` |
|    57034 |  480 | `			if( pStream->zText + 1 < pStream->zEnd && pStream->zText[0] == '.' && pStream->zText[1] == '.' ){` |
|        - |  481 | `				/* Ellipsis: ... */` |
|       42 |  482 | `				pStream->zText += 2;` |
|       42 |  483 | `				pToken->nType = PH7_TK_ELLIPSIS;` |
|    57014 |  484 | `			}else if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  485 | `				/* Current operator: .= */` |
|     2698 |  486 | `				pStream->zText++;` |
|     1348 |  487 | `			}` |
|    57034 |  488 | `			break;` |
|    22562 |  489 | `		case '<':` |
|    45126 |  490 | `			if( pStream->zText < pStream->zEnd ){` |
|    45126 |  491 | `				if( pStream->zText[0] == '<' ){` |
|        - |  492 | `					/* Current operator: << */` |
|       80 |  493 | `					pStream->zText++;` |
|       80 |  494 | `					if( pStream->zText < pStream->zEnd ){` |
|       80 |  495 | `						if( pStream->zText[0] == '=' ){` |
|        - |  496 | `							/* Current operator: <<= */` |
|        9 |  497 | `							pStream->zText++;` |
|       76 |  498 | `						}else if( pStream->zText[0] == '<' ){` |
|        - |  499 | `							/* Current Token: <<<  */` |
|       58 |  500 | `							pStream->zText++;` |
|        - |  501 | `							/* This may be the beginning of a Heredoc/Nowdoc string,try to delimit it */` |
|       58 |  502 | `							rc = LexExtractHeredoc(&(*pStream),&(*pToken));` |
|       58 |  503 | `							if( rc == SXRET_OK ){` |
|        - |  504 | `								/* Here/Now doc successfuly extracted */` |
|       58 |  505 | `								return SXRET_OK;` |
|        - |  506 | `							}` |
|      ! 0 |  507 | `						}` |
|       12 |  508 | `					}` |
|    45059 |  509 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  510 | `					/* Current operator: <> */` |
|        5 |  511 | `					pStream->zText++;` |
|    45046 |  512 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  513 | `					/* Current operator: <= or <=> */` |
|       86 |  514 | `					pStream->zText++;` |
|       86 |  515 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '>' ){` |
|        - |  516 | `						/* Current operator: <=> */` |
|       51 |  517 | `						pStream->zText++;` |
|       25 |  518 | `					}` |
|       42 |  519 | `				}` |
|    22534 |  520 | `			}` |
|    45070 |  521 | `			break;` |
|     2733 |  522 | `		case '>':` |
|     5468 |  523 | `			if( pStream->zText < pStream->zEnd ){` |
|     5468 |  524 | `				if( pStream->zText[0] == '>' ){` |
|        - |  525 | `					/* Current operator: >> */` |
|       21 |  526 | `					pStream->zText++;` |
|       21 |  527 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  528 | `						/* Current operator: >>= */` |
|       11 |  529 | `						pStream->zText++;` |
|        6 |  530 | `					}` |
|     5458 |  531 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  532 | `					/* Current operator: >= */` |
|       80 |  533 | `					pStream->zText++;` |
|       39 |  534 | `				}` |
|     2733 |  535 | `			}` |
|     5468 |  536 | `			break;` |
|      971 |  537 | `		case '?':` |
|     1944 |  538 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '?' ){` |
|        - |  539 | `				/* Null coalescing operator: ?? */` |
|       32 |  540 | `				pStream->zText++;` |
|       15 |  541 | `			}` |
|     1942 |  542 | `			break;` |
|      105 |  543 | `		default:` |
|      210 |  544 | `			break;` |
|        - |  545 | `		}` |
|  4111588 |  546 | `		if( pStr->nByte <= 0 ){` |
|        - |  547 | `			/* Record token length */` |
|  4111544 |  548 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  2055771 |  549 | `		}` |
|  4111588 |  550 | `		if( pToken->nType & PH7_TK_OP ){` |
|        - |  551 | `			const ph7_expr_op *pOp;` |
|        - |  552 | `			/* Check if the extracted token is an operator */` |
|   697930 |  553 | `			pOp = PH7_ExprExtractOperator(pStr,(SyToken *)SySetPeek(pStream->pSet));` |
|   697930 |  554 | `			if( pOp == 0 ){` |
|        - |  555 | `				/* Not an operator */` |
|      ! 0 |  556 | `				pToken->nType &= ~PH7_TK_OP;` |
|      ! 0 |  557 | `				if( pToken->nType <= 0 ){` |
|      ! 0 |  558 | `					pToken->nType = PH7_TK_OTHER;` |
|      ! 0 |  559 | `				}` |
|      ! 0 |  560 | `			}else{` |
|        - |  561 | `				/* Save the instance associated with this operator for later processing */` |
|   697930 |  562 | `				pToken->pUserData = (void *)pOp;` |
|        - |  563 | `			}` |
|   348964 |  564 | `		}` |
|        - |  565 | `	}` |
|        - |  566 | `	/* Tell the upper-layer to save the extracted token for later processing */` |
|  6572940 |  567 | `	return SXRET_OK;` |
|  3406172 |  568 |  |
|        - |  569 | `/***** This file contains automatically generated code ******` |
|        - |  570 | `**` |
|        - |  571 | `** The code in this file has been automatically generated by` |
|        - |  572 | `**` |
|        - |  573 | `**     $Header: /sqlite/sqlite/tool/mkkeywordhash.c` |
|        - |  574 | `**` |
|        - |  575 | `** Sligthly modified by Chems mrad <chm@symisc.net> for the PH7 engine.` |
|        - |  576 | `**` |
|        - |  577 | `** The code in this file implements a function that determines whether` |
|        - |  578 | `** or not a given identifier is really a PHP keyword.  The same thing` |
|        - |  579 | `** might be implemented more directly using a hand-written hash table.` |
|        - |  580 | `** But by using this automatically generated code, the size of the code` |
|        - |  581 | `** is substantially reduced.  This is important for embedded applications` |
|        - |  582 | `** on platforms with limited memory.` |
|        - |  583 | `*/` |
|        - |  584 | `/* Hash score: 103 */` |
|  2461354 |  585 | `static sxu32 KeywordCode(const char *z, int n){` |
|        - |  586 | `  /* zText[] encodes 532 bytes of keywords in 333 bytes */` |
|        - |  587 | `  /*   extendswitchprintegerequire_oncenddeclareturnamespacechobject      */` |
|        - |  588 | `  /*   hrowbooleandefaultrycaselfinalistaticlonewconstringlobaluse        */` |
|        - |  589 | `  /*   lseifloatvarrayANDIEchoUSECHOabstractclasscontinuendifunction      */` |
|        - |  590 | `  /*   diendwhilevaldoexitgotoimplementsinclude_oncemptyinstanceof        */` |
|        - |  591 | `  /*   interfacendforeachissetparentprivateprotectedpublicatchunset       */` |
|        - |  592 | `  /*   xorARRAYASArrayEXITUNSETXORbreak                                   */` |
|        - |  593 | `  static const char zText[332] = {` |
|        - |  594 | `    'e','x','t','e','n','d','s','w','i','t','c','h','p','r','i','n','t','e',` |
|        - |  595 | `    'g','e','r','e','q','u','i','r','e','_','o','n','c','e','n','d','d','e',` |
|        - |  596 | `    'c','l','a','r','e','t','u','r','n','a','m','e','s','p','a','c','e','c',` |
|        - |  597 | `    'h','o','b','j','e','c','t','h','r','o','w','b','o','o','l','e','a','n',` |
|        - |  598 | `    'd','e','f','a','u','l','t','r','y','c','a','s','e','l','f','i','n','a',` |
|        - |  599 | `    'l','i','s','t','a','t','i','c','l','o','n','e','w','c','o','n','s','t',` |
|        - |  600 | `    'r','i','n','g','l','o','b','a','l','u','s','e','l','s','e','i','f','l',` |
|        - |  601 | `    'o','a','t','v','a','r','r','a','y','A','N','D','I','E','c','h','o','U',` |
|        - |  602 | `    'S','E','C','H','O','a','b','s','t','r','a','c','t','c','l','a','s','s',` |
|        - |  603 | `    'c','o','n','t','i','n','u','e','n','d','i','f','u','n','c','t','i','o',` |
|        - |  604 | `    'n','d','i','e','n','d','w','h','i','l','e','v','a','l','d','o','e','x',` |
|        - |  605 | `    'i','t','g','o','t','o','i','m','p','l','e','m','e','n','t','s','i','n',` |
|        - |  606 | `    'c','l','u','d','e','_','o','n','c','e','m','p','t','y','i','n','s','t',` |
|        - |  607 | `    'a','n','c','e','o','f','i','n','t','e','r','f','a','c','e','n','d','f',` |
|        - |  608 | `    'o','r','e','a','c','h','i','s','s','e','t','p','a','r','e','n','t','p',` |
|        - |  609 | `    'r','i','v','a','t','e','p','r','o','t','e','c','t','e','d','p','u','b',` |
|        - |  610 | `    'l','i','c','a','t','c','h','u','n','s','e','t','x','o','r','A','R','R',` |
|        - |  611 | `    'A','Y','A','S','A','r','r','a','y','E','X','I','T','U','N','S','E','T',` |
|        - |  612 | `    'X','O','R','b','r','e','a','k'` |
|        - |  613 | `  };` |
|        - |  614 | `  static const unsigned char aHash[151] = {` |
|        - |  615 |  |
|        - |  616 |  |
|        - |  617 | `      74,   0,  51,   0,  76,   0,   0,  20,   0,   0,   0,  50,   0,` |
|        - |  618 | `      80,  34,   0,  36,   0,   0,  64,  16,   0,   0,  17,   0,   1,` |
|        - |  619 | `      19,  84,  66,   0,  43,  45,  78,   0,   0,  53,  56,   0,   0,` |
|        - |  620 |  |
|        - |  621 | `      72,  14,   0,  71,   0,  38,   6,   0,   0,   0,  73,   0,   0,` |
|        - |  622 | `       3,   0,  41,   5,  52,  57,  32,   0,  60,  63,   0,  69,  82,` |
|        - |  623 | `      30,   0,  79,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,` |
|        - |  624 |  |
|        - |  625 | `      62,   0,  11,   0,   0,  58,   0,   0,   0,   0,  59,  75,   0,` |
|        - |  626 |  |
|        - |  627 | `  };` |
|        - |  628 | `  static const unsigned char aNext[84] = {` |
|        - |  629 |  |
|        - |  630 |  |
|        - |  631 |  |
|        - |  632 |  |
|        - |  633 |  |
|        - |  634 |  |
|        - |  635 | `      42,   0,   0,   0,  70,  55` |
|        - |  636 | `  };` |
|        - |  637 | `  static const unsigned char aLen[84] = {` |
|        - |  638 | `       7,   9,   6,   5,   7,  12,   7,   2,  10,   7,   6,   9,   4,` |
|        - |  639 | `       6,   5,   7,   4,   3,   7,   3,   4,   4,   5,   4,   6,   5,` |
|        - |  640 | `       2,   3,   5,   6,   6,   3,   6,   4,   2,   5,   3,   5,   3,` |
|        - |  641 | `       3,   4,   3,   4,   8,   5,   2,   8,   5,   8,   3,   8,   5,` |
|        - |  642 | `       4,   2,   4,   4,  10,  12,   7,   5,  10,   9,   3,   6,  10,` |
|        - |  643 | `       3,   7,   2,   5,   6,   7,   9,   6,   5,   5,   3,   5,   2,` |
|        - |  644 | `       5,   4,   5,   3,   2,   5` |
|        - |  645 | `  };` |
|        - |  646 | `  static const sxu16 aOffset[84] = {` |
|        - |  647 |  |
|        - |  648 | `      55,  60,  65,  65,  70,  72,  78,  81,  83,  86,  90,  92,  97,` |
|        - |  649 | `     100, 100, 103, 106, 111, 117, 119, 119, 123, 124, 129, 130, 135,` |
|        - |  650 | `     137, 139, 143, 145, 149, 157, 159, 162, 169, 173, 181, 183, 186,` |
|        - |  651 | `     190, 194, 196, 200, 204, 214, 214, 225, 230, 240, 240, 248, 248,` |
|        - |  652 | `     251, 251, 252, 258, 263, 269, 276, 285, 290, 295, 300, 303, 308,` |
|        - |  653 | `     310, 315, 319, 324, 325, 327` |
|        - |  654 | `  };` |
|        - |  655 | `  static const sxu32 aCode[84] = {` |
|        - |  656 | `    PH7_TKWRD_EXTENDS,   PH7_TKWRD_ENDSWITCH,   PH7_TKWRD_SWITCH,    PH7_TKWRD_PRINT,   PH7_TKWRD_INT,` |
|        - |  657 | `    PH7_TKWRD_REQONCE,   PH7_TKWRD_REQUIRE,     PH7_TKWRD_SEQ,       PH7_TKWRD_ENDDEC,    PH7_TKWRD_DECLARE,` |
|        - |  658 | `    PH7_TKWRD_RETURN,    PH7_TKWRD_NAMESPACE,   PH7_TKWRD_ECHO,      PH7_TKWRD_OBJECT,    PH7_TKWRD_THROW,` |
|        - |  659 | `    PH7_TKWRD_BOOL,      PH7_TKWRD_BOOL,        PH7_TKWRD_AND,       PH7_TKWRD_DEFAULT,   PH7_TKWRD_TRY,` |
|        - |  660 | `    PH7_TKWRD_CASE,      PH7_TKWRD_SELF,        PH7_TKWRD_FINAL,     PH7_TKWRD_LIST,      PH7_TKWRD_STATIC,` |
|        - |  661 | `    PH7_TKWRD_CLONE,     PH7_TKWRD_SNE,         PH7_TKWRD_NEW,       PH7_TKWRD_CONST,     PH7_TKWRD_STRING,` |
|        - |  662 | `    PH7_TKWRD_GLOBAL,    PH7_TKWRD_USE,         PH7_TKWRD_ELIF,      PH7_TKWRD_ELSE,      PH7_TKWRD_IF,` |
|        - |  663 | `    PH7_TKWRD_FLOAT,     PH7_TKWRD_VAR,         PH7_TKWRD_ARRAY,     PH7_TKWRD_AND,       PH7_TKWRD_DIE,` |
|        - |  664 | `    PH7_TKWRD_ECHO,      PH7_TKWRD_USE,         PH7_TKWRD_ECHO,      PH7_TKWRD_ABSTRACT,  PH7_TKWRD_CLASS,` |
|        - |  665 | `    PH7_TKWRD_AS,        PH7_TKWRD_CONTINUE,    PH7_TKWRD_ENDIF,     PH7_TKWRD_FUNCTION,  PH7_TKWRD_DIE,` |
|        - |  666 | `    PH7_TKWRD_ENDWHILE,  PH7_TKWRD_WHILE,       PH7_TKWRD_EVAL,      PH7_TKWRD_DO,        PH7_TKWRD_EXIT,` |
|        - |  667 | `    PH7_TKWRD_GOTO,      PH7_TKWRD_IMPLEMENTS,  PH7_TKWRD_INCONCE,   PH7_TKWRD_INCLUDE,   PH7_TKWRD_EMPTY,` |
|        - |  668 | `    PH7_TKWRD_INSTANCEOF,PH7_TKWRD_INTERFACE,   PH7_TKWRD_INT,       PH7_TKWRD_ENDFOR,    PH7_TKWRD_END4EACH,` |
|        - |  669 | `    PH7_TKWRD_FOR,       PH7_TKWRD_FOREACH,     PH7_TKWRD_OR,        PH7_TKWRD_ISSET,     PH7_TKWRD_PARENT,` |
|        - |  670 | `    PH7_TKWRD_PRIVATE,   PH7_TKWRD_PROTECTED,   PH7_TKWRD_PUBLIC,    PH7_TKWRD_CATCH,     PH7_TKWRD_UNSET,` |
|        - |  671 | `    PH7_TKWRD_XOR,       PH7_TKWRD_ARRAY,       PH7_TKWRD_AS,        PH7_TKWRD_ARRAY,     PH7_TKWRD_EXIT,` |
|        - |  672 | `    PH7_TKWRD_UNSET,     PH7_TKWRD_XOR,         PH7_TKWRD_OR,        PH7_TKWRD_BREAK` |
|        - |  673 | `  };` |
|        - |  674 | `  int h, i;` |
|  2461354 |  675 | `  if( n<2 ) return PH7_TK_ID;` |
|  2372444 |  676 | `  h = (((int)z[0]*4) ^ ((int)z[n-1]*3) ^ n) % 151;` |
|  3631742 |  677 | `  for(i=((int)aHash[h])-1; i>=0; i=((int)aNext[i])-1){` |
|  2100218 |  678 | `    if( (int)aLen[i]==n && SyMemcmp(&zText[aOffset[i]],z,n)==0 ){` |
|        - |  679 | `       /* PH7_TKWRD_EXTENDS */` |
|        - |  680 | `       /* PH7_TKWRD_ENDSWITCH */` |
|        - |  681 | `       /* PH7_TKWRD_SWITCH */` |
|        - |  682 | `       /* PH7_TKWRD_PRINT */` |
|        - |  683 | `       /* PH7_TKWRD_INT */` |
|        - |  684 | `       /* PH7_TKWRD_REQONCE */` |
|        - |  685 | `       /* PH7_TKWRD_REQUIRE */` |
|        - |  686 | `       /* PH7_TKWRD_SEQ */` |
|        - |  687 | `       /* PH7_TKWRD_ENDDEC */` |
|        - |  688 | `       /* PH7_TKWRD_DECLARE */` |
|        - |  689 | `       /* PH7_TKWRD_RETURN */` |
|        - |  690 | `       /* PH7_TKWRD_NAMESPACE */` |
|        - |  691 | `       /* PH7_TKWRD_ECHO */` |
|        - |  692 | `       /* PH7_TKWRD_OBJECT */` |
|        - |  693 | `       /* PH7_TKWRD_THROW */` |
|        - |  694 | `       /* PH7_TKWRD_BOOL */` |
|        - |  695 | `       /* PH7_TKWRD_BOOL */` |
|        - |  696 | `       /* PH7_TKWRD_AND */` |
|        - |  697 | `       /* PH7_TKWRD_DEFAULT */` |
|        - |  698 | `       /* PH7_TKWRD_TRY */` |
|        - |  699 | `       /* PH7_TKWRD_CASE */` |
|        - |  700 | `       /* PH7_TKWRD_SELF */` |
|        - |  701 | `       /* PH7_TKWRD_FINAL */` |
|        - |  702 | `       /* PH7_TKWRD_LIST */` |
|        - |  703 | `       /* PH7_TKWRD_STATIC */` |
|        - |  704 | `       /* PH7_TKWRD_CLONE */` |
|        - |  705 | `       /* PH7_TKWRD_SNE */` |
|        - |  706 | `       /* PH7_TKWRD_NEW */` |
|        - |  707 | `       /* PH7_TKWRD_CONST */` |
|        - |  708 | `       /* PH7_TKWRD_STRING */` |
|        - |  709 | `       /* PH7_TKWRD_GLOBAL */` |
|        - |  710 | `       /* PH7_TKWRD_USE */` |
|        - |  711 | `       /* PH7_TKWRD_ELIF */` |
|        - |  712 | `       /* PH7_TKWRD_ELSE */` |
|        - |  713 | `       /* PH7_TKWRD_IF */` |
|        - |  714 | `       /* PH7_TKWRD_FLOAT */` |
|        - |  715 | `       /* PH7_TKWRD_VAR */` |
|        - |  716 | `       /* PH7_TKWRD_ARRAY */` |
|        - |  717 | `       /* PH7_TKWRD_AND */` |
|        - |  718 | `       /* PH7_TKWRD_DIE */` |
|        - |  719 | `       /* PH7_TKWRD_ECHO */` |
|        - |  720 | `       /* PH7_TKWRD_USE */` |
|        - |  721 | `       /* PH7_TKWRD_ECHO */` |
|        - |  722 | `       /* PH7_TKWRD_ABSTRACT */` |
|        - |  723 | `       /* PH7_TKWRD_CLASS */` |
|        - |  724 | `       /* PH7_TKWRD_AS */` |
|        - |  725 | `       /* PH7_TKWRD_CONTINUE */` |
|        - |  726 | `       /* PH7_TKWRD_ENDIF */` |
|        - |  727 | `       /* PH7_TKWRD_FUNCTION */` |
|        - |  728 | `       /* PH7_TKWRD_DIE */` |
|        - |  729 | `       /* PH7_TKWRD_ENDWHILE */` |
|        - |  730 | `       /* PH7_TKWRD_WHILE */` |
|        - |  731 | `       /* PH7_TKWRD_EVAL */` |
|        - |  732 | `       /* PH7_TKWRD_DO */` |
|        - |  733 | `       /* PH7_TKWRD_EXIT */` |
|        - |  734 | `       /* PH7_TKWRD_GOTO */` |
|        - |  735 | `       /* PH7_TKWRD_IMPLEMENTS */` |
|        - |  736 | `       /* PH7_TKWRD_INCONCE */` |
|        - |  737 | `       /* PH7_TKWRD_INCLUDE */` |
|        - |  738 | `       /* PH7_TKWRD_EMPTY */` |
|        - |  739 | `       /* PH7_TKWRD_INSTANCEOF */` |
|        - |  740 | `       /* PH7_TKWRD_INTERFACE */` |
|        - |  741 | `       /* PH7_TKWRD_INT */` |
|        - |  742 | `       /* PH7_TKWRD_ENDFOR */` |
|        - |  743 | `       /* PH7_TKWRD_END4EACH */` |
|        - |  744 | `       /* PH7_TKWRD_FOR */` |
|        - |  745 | `       /* PH7_TKWRD_FOREACH */` |
|        - |  746 | `       /* PH7_TKWRD_OR */` |
|        - |  747 | `       /* PH7_TKWRD_ISSET */` |
|        - |  748 | `       /* PH7_TKWRD_PARENT */` |
|        - |  749 | `       /* PH7_TKWRD_PRIVATE */` |
|        - |  750 | `       /* PH7_TKWRD_PROTECTED */` |
|        - |  751 | `       /* PH7_TKWRD_PUBLIC */` |
|        - |  752 | `       /* PH7_TKWRD_CATCH */` |
|        - |  753 | `       /* PH7_TKWRD_UNSET */` |
|        - |  754 | `       /* PH7_TKWRD_XOR */` |
|        - |  755 | `       /* PH7_TKWRD_ARRAY */` |
|        - |  756 | `       /* PH7_TKWRD_AS */` |
|        - |  757 | `       /* PH7_TKWRD_ARRAY */` |
|        - |  758 | `       /* PH7_TKWRD_EXIT */` |
|        - |  759 | `       /* PH7_TKWRD_UNSET */` |
|        - |  760 | `       /* PH7_TKWRD_XOR */` |
|        - |  761 | `       /* PH7_TKWRD_OR */` |
|        - |  762 | `       /* PH7_TKWRD_BREAK */` |
|   840920 |  763 | `      return aCode[i];` |
|        - |  764 | `    }` |
|   629649 |  765 | `  }` |
|        - |  766 | `  /* Linear fallback for keywords not in the auto-generated hash table */` |
|  1531526 |  767 | `  if( n==5 && SyMemcmp(z,"trait",5)==0 ) return PH7_TKWRD_TRAIT;` |
|  1531472 |  768 | `  if( n==9 && SyMemcmp(z,"insteadof",9)==0 ) return PH7_TKWRD_INSTEADOF;` |
|  1531468 |  769 | `  if( n==7 && SyMemcmp(z,"finally",7)==0 ) return PH7_TKWRD_FINALLY;` |
|  1531438 |  770 | `  if( n==5 && SyMemcmp(z,"yield",5)==0 ) return PH7_TKWRD_YIELD;` |
|  1531406 |  771 | `  return PH7_TK_ID;` |
|  1230678 |  772 |  |
|        - |  773 | `/* --- End of Automatically generated code --- */` |
|        - |  774 | `/*` |
|        - |  775 | ` * Extract a heredoc/nowdoc text from a raw PHP input.` |
|        - |  776 | ` * According to the PHP language reference manual:` |
|        - |  777 | ` *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier` |
|        - |  778 | ` *  is provided, then a newline. The string itself follows, and then the same identifier again` |
|        - |  779 | ` *  to close the quotation.` |
|        - |  780 | ` *  The closing identifier must begin in the first column of the line. Also, the identifier must` |
|        - |  781 | ` *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric` |
|        - |  782 | ` *  characters and underscores, and must start with a non-digit character or underscore.` |
|        - |  783 | ` *  Heredoc text behaves just like a double-quoted string, without the double quotes.` |
|        - |  784 | ` *  This means that quotes in a heredoc do not need to be escaped, but the escape codes listed` |
|        - |  785 | ` *  above can still be used. Variables are expanded, but the same care must be taken when expressing` |
|        - |  786 | ` *  complex variables inside a heredoc as with strings.` |
|        - |  787 | ` *  Nowdocs are to single-quoted strings what heredocs are to double-quoted strings.` |
|        - |  788 | ` *  A nowdoc is specified similarly to a heredoc, but no parsing is done inside a nowdoc.` |
|        - |  789 | ` *  The construct is ideal for embedding PHP code or other large blocks of text without the need` |
|        - |  790 | ` *  for escaping. It shares some features in common with the SGML <![CDATA[ ]]> construct, in that` |
|        - |  791 | ` *  it declares a block of text which is not for parsing.` |
|        - |  792 | ` *  A nowdoc is identified with the same <<< sequence used for heredocs, but the identifier which follows` |
|        - |  793 | ` *  is enclosed in single quotes, e.g. <<<'EOT'. All the rules for heredoc identifiers also apply to nowdoc` |
|        - |  794 | ` *  identifiers, especially those regarding the appearance of the closing identifier.` |
|        - |  795 | ` * Symisc Extension:` |
|        - |  796 | ` * The closing delimiter can now start with a digit or undersocre or it can be an UTF-8 stream.` |
|        - |  797 | ` * Example:` |
|        - |  798 | ` *  <<<123` |
|        - |  799 | ` *    HEREDOC Here` |
|        - |  800 | ` * 123` |
|        - |  801 | ` *  or` |
|        - |  802 | ` *  <<<___` |
|        - |  803 | ` *   HEREDOC Here` |
|        - |  804 | ` *  ___` |
|        - |  805 | ` */` |
|       56 |  806 | `static sxi32 LexExtractHeredoc(SyStream *pStream,SyToken *pToken)` |
|        2 |  807 |  |
|       58 |  808 | `	const unsigned char *zIn  = pStream->zText;` |
|       58 |  809 | `	const unsigned char *zEnd = pStream->zEnd;` |
|        - |  810 | `	const unsigned char *zPtr;` |
|       58 |  811 | `	sxu8 bNowDoc = FALSE;` |
|        - |  812 | `	SyString sDelim;` |
|        - |  813 | `	SyString sStr;` |
|        - |  814 | `	/* Jump leading white spaces */` |
|       70 |  815 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|       13 |  816 | `		zIn++;` |
|        1 |  817 | `	}` |
|       58 |  818 | `	if( zIn >= zEnd ){` |
|        - |  819 | `		/* A simple symbol,return immediately */` |
|      ! 0 |  820 | `		return SXERR_CONTINUE;` |
|        - |  821 | `	}` |
|       58 |  822 | `	if( zIn[0] == '\'' \|\| zIn[0] == '"' ){` |
|        - |  823 | `		/* Make sure we are dealing with a nowdoc */` |
|       29 |  824 | `		bNowDoc =  zIn[0] == '\'' ? TRUE : FALSE;` |
|       29 |  825 | `		zIn++;` |
|       14 |  826 | `	}` |
|       58 |  827 | `	if( zIn[0] < 0xc0 && !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|        - |  828 | `		/* Invalid delimiter,return immediately */` |
|      ! 0 |  829 | `		return SXERR_CONTINUE;` |
|        - |  830 | `	}` |
|        - |  831 | `	/* Isolate the identifier */` |
|       58 |  832 | `	sDelim.zString = (const char *)zIn;` |
|       64 |  833 | `	for(;;){` |
|      130 |  834 | `		zPtr = zIn;` |
|        - |  835 | `		/* Skip alphanumeric stream */` |
|      424 |  836 | `		while( zPtr < zEnd && zPtr[0] < 0xc0 && (SyisAlphaNum(zPtr[0]) \|\| zPtr[0] == '_') ){` |
|      232 |  837 | `			zPtr++;` |
|        2 |  838 | `		}` |
|      130 |  839 | `		if( zPtr < zEnd && zPtr[0] >= 0xc0 ){` |
|       19 |  840 | `			zPtr++;` |
|        - |  841 | `			/* UTF-8 stream */` |
|       37 |  842 | `			while( zPtr < zEnd && ((zPtr[0] & 0xc0) == 0x80) ){` |
|       19 |  843 | `				zPtr++;` |
|        1 |  844 | `			}` |
|        9 |  845 | `		}` |
|      130 |  846 | `		if( zPtr == zIn ){` |
|        - |  847 | `			/* Not an UTF-8 or alphanumeric stream */` |
|       58 |  848 | `			break;` |
|        - |  849 | `		}` |
|        - |  850 | `		/* Synchronize pointers */` |
|       74 |  851 | `		zIn = zPtr;` |
|        2 |  852 | `	}` |
|        - |  853 | `	/* Get the identifier length */` |
|       58 |  854 | `	sDelim.nByte = (sxu32)((const char *)zIn-sDelim.zString);` |
|       58 |  855 | `	if( zIn[0] == '"' \|\| (bNowDoc && zIn[0] == '\'') ){` |
|        - |  856 | `		/* Jump the trailing single quote */` |
|       29 |  857 | `		zIn++;` |
|       14 |  858 | `	}` |
|        - |  859 | `	/* Jump trailing white spaces */` |
|       58 |  860 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|      ! 0 |  861 | `		zIn++;` |
|      ! 0 |  862 | `	}` |
|       58 |  863 | `	if( sDelim.nByte <= 0 \|\| zIn >= zEnd \|\| zIn[0] != '\n' ){` |
|        - |  864 | `		/* Invalid syntax */` |
|      ! 0 |  865 | `		return SXERR_CONTINUE;` |
|        - |  866 | `	}` |
|       58 |  867 | `	pStream->nLine++; /* Increment line counter */` |
|       58 |  868 | `	zIn++;` |
|        - |  869 | `	/* Isolate the delimited string */` |
|       58 |  870 | `	sStr.zString = (const char *)zIn;` |
|        - |  871 | `	/* Go and found the closing delimiter */` |
|       75 |  872 | `	for(;;){` |
|        - |  873 | `		/* Synchronize with the next line */` |
|     3018 |  874 | `		while( zIn < zEnd && zIn[0] != '\n' ){` |
|     2868 |  875 | `			zIn++;` |
|        2 |  876 | `		}` |
|      152 |  877 | `		if( zIn >= zEnd ){` |
|        - |  878 | `			/* End of the input reached, break immediately */` |
|       12 |  879 | `			pStream->zText = pStream->zEnd;` |
|       12 |  880 | `			break;` |
|        - |  881 | `		}` |
|      142 |  882 | `		pStream->nLine++; /* Increment line counter */` |
|      142 |  883 | `		zIn++;` |
|      142 |  884 | `		if( (sxu32)(zEnd - zIn) >= sDelim.nByte && SyMemcmp((const void *)sDelim.zString,(const void *)zIn,sDelim.nByte) == 0 ){` |
|       50 |  885 | `			zPtr = &zIn[sDelim.nByte];` |
|       62 |  886 | `			while( zPtr < zEnd && zPtr[0] < 0xc0 && SyisSpace(zPtr[0]) && zPtr[0] != '\n' ){` |
|       13 |  887 | `				zPtr++;` |
|        1 |  888 | `			}` |
|       50 |  889 | `			if( zPtr >= zEnd ){` |
|        - |  890 | `				/* End of input */` |
|      ! 0 |  891 | `				pStream->zText = zPtr;` |
|      ! 0 |  892 | `				break;` |
|        - |  893 | `			}` |
|       50 |  894 | `			if( zPtr[0] == ';' ){` |
|       50 |  895 | `				const unsigned char *zCur = zPtr;` |
|       50 |  896 | `				zPtr++;` |
|       52 |  897 | `				while( zPtr < zEnd && zPtr[0] < 0xc0 && SyisSpace(zPtr[0]) && zPtr[0] != '\n' ){` |
|        3 |  898 | `					zPtr++;` |
|        1 |  899 | `				}` |
|       50 |  900 | `				if( zPtr >= zEnd \|\| zPtr[0] == '\n' ){` |
|        - |  901 | `					/* Closing delimiter found,break immediately */` |
|       48 |  902 | `					pStream->zText = zCur; /* Keep the semi-colon */` |
|       48 |  903 | `					break;` |
|        1 |  904 | `				}` |
|        1 |  905 | `			}else if( zPtr[0] == '\n' ){` |
|        - |  906 | `				/* Closing delimiter found,break immediately */` |
|      ! 0 |  907 | `				pStream->zText = zPtr; /* Synchronize with the stream cursor */` |
|      ! 0 |  908 | `				break;` |
|        - |  909 | `			}` |
|        - |  910 | `			/* Synchronize pointers and continue searching */` |
|        3 |  911 | `			zIn = zPtr;` |
|        1 |  912 | `		}` |
|        2 |  913 | `	} /* For(;;) */` |
|        - |  914 | `	/* Get the delimited string length */` |
|       58 |  915 | `	sStr.nByte = (sxu32)((const char *)zIn-sStr.zString);` |
|        - |  916 | `	/* Record token type and length */` |
|       58 |  917 | `	pToken->nType = bNowDoc ? PH7_TK_NOWDOC : PH7_TK_HEREDOC;` |
|       58 |  918 | `	SyStringDupPtr(&pToken->sData,&sStr);` |
|        - |  919 | `	/* Remove trailing white spaces */` |
|      104 |  920 | `	SyStringRightTrim(&pToken->sData);` |
|        - |  921 | `	/* All done */` |
|       58 |  922 | `	return SXRET_OK;` |
|       30 |  923 |  |
|        - |  924 | `/*` |
|        - |  925 | ` * Tokenize a raw PHP input.` |
|        - |  926 | ` * This is the public tokenizer called by most code generator routines.` |
|        - |  927 | ` */` |
|    13136 |  928 | `PH7_PRIVATE sxi32 PH7_TokenizePHP(const char *zInput,sxu32 nLen,sxu32 nLineStart,SySet *pOut)` |
|        2 |  929 |  |
|        - |  930 | `	SyLex sLexer;` |
|        - |  931 | `	sxi32 rc;` |
|        - |  932 | `	/* Initialize the lexer */` |
|    13138 |  933 | `	rc = SyLexInit(&sLexer,&(*pOut),TokenizePHP,0);` |
|    13138 |  934 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  935 | `		return rc;` |
|        - |  936 | `	}` |
|    13138 |  937 | `	sLexer.sStream.nLine = nLineStart;` |
|        - |  938 | `	/* Tokenize input */` |
|    13138 |  939 | `	rc = SyLexTokenizeInput(&sLexer,zInput,nLen,0,0,0);` |
|        - |  940 | `	/* Release the lexer */` |
|    13138 |  941 | `	SyLexRelease(&sLexer);` |
|        - |  942 | `	/* Tokenization result */` |
|    13138 |  943 | `	return rc;` |
|     6570 |  944 |  |
|        - |  945 | `/*` |
|        - |  946 | ` * High level public tokenizer.` |
|        - |  947 | ` *  Tokenize the input into PHP tokens and raw tokens [i.e: HTML,XML,Raw text...].` |
|        - |  948 | ` * According to the PHP language reference manual` |
|        - |  949 | ` *   When PHP parses a file, it looks for opening and closing tags, which tell PHP` |
|        - |  950 | ` *   to start and stop interpreting the code between them. Parsing in this manner allows` |
|        - |  951 | ` *   PHP to be embedded in all sorts of different documents, as everything outside of a pair` |
|        - |  952 | ` *   of opening and closing tags is ignored by the PHP parser. Most of the time you will see` |
|        - |  953 | ` *   PHP embedded in HTML documents, as in this example.` |
|        - |  954 | ` *   <?php echo 'While this is going to be parsed.'; ?>` |
|        - |  955 | ` *   <p>This will also be ignored.</p>` |
|        - |  956 | ` *   You can also use more advanced structures:` |
|        - |  957 | ` *   Example #1 Advanced escaping` |
|        - |  958 | ` * <?php` |
|        - |  959 | ` * if ($expression) {` |
|        - |  960 | ` *   ?>` |
|        - |  961 | ` *   <strong>This is true.</strong>` |
|        - |  962 | ` *   <?php` |
|        - |  963 | ` * } else {` |
|        - |  964 | ` *   ?>` |
|        - |  965 | ` *   <strong>This is false.</strong>` |
|        - |  966 | ` *   <?php` |
|        - |  967 | ` * }` |
|        - |  968 | ` * ?>` |
|        - |  969 | ` * This works as expected, because when PHP hits the ?> closing tags, it simply starts outputting` |
|        - |  970 | ` * whatever it finds (except for an immediately following newline - see instruction separation ) until it hits` |
|        - |  971 | ` * another opening tag. The example given here is contrived, of course, but for outputting large blocks of text` |
|        - |  972 | ` * dropping out of PHP parsing mode is generally more efficient than sending all of the text through echo() or print().` |
|        - |  973 | ` * There are four different pairs of opening and closing tags which can be used in PHP. Three of those, <?php ?>` |
|        - |  974 | ` * <script language="php"> </script>  and <? ?> are always available. The other two are short tags and ASP style` |
|        - |  975 | ` * tags, and can be turned on and off from the php.ini configuration file. As such, while some people find short tags` |
|        - |  976 | ` * and ASP style tags convenient, they are less portable, and generally not recommended.` |
|        - |  977 | ` * Note:` |
|        - |  978 | ` * Also note that if you are embedding PHP within XML or XHTML you will need to use the <?php ?> tags to remain` |
|        - |  979 | ` * compliant with standards.` |
|        - |  980 | ` * Example #2 PHP Opening and Closing Tags` |
|        - |  981 | ` * 1.  <?php echo 'if you want to serve XHTML or XML documents, do it like this'; ?>` |
|        - |  982 | ` * 2.  <script language="php">` |
|        - |  983 | ` *       echo 'some editors (like FrontPage) don\'t` |
|        - |  984 | ` *             like processing instructions';` |
|        - |  985 | ` *   </script>` |
|        - |  986 | ` *` |
|        - |  987 | ` * 3.  <? echo 'this is the simplest, an SGML processing instruction'; ?>` |
|        - |  988 | ` *   <?= expression ?> This is a shortcut for "<? echo expression ?>"` |
|        - |  989 | ` */` |
|    10838 |  990 | `PH7_PRIVATE sxi32 PH7_TokenizeRawText(const char *zInput,sxu32 nLen,SySet *pOut)` |
|        2 |  991 |  |
|    10840 |  992 | `	const char *zEnd = &zInput[nLen];` |
|    10840 |  993 | `	const char *zIn  = zInput;` |
|        - |  994 | `	const char *zCur,*zCurEnd;` |
|    10840 |  995 | `	SyString sCtag = { 0, 0 };     /* Closing tag */` |
|        - |  996 | `	SyToken sToken;` |
|        - |  997 | `	SyString sDoc;` |
|        - |  998 | `	sxu32 nLine;` |
|        - |  999 | `	sxi32 iNest;` |
|        - | 1000 | `	sxi32 rc;` |
|        - | 1001 | `	/* Tokenize the input into PHP tokens and raw tokens */` |
|    10840 | 1002 | `	nLine = 1;` |
|    10840 | 1003 | `	zCur = zCurEnd   = 0; /* Prevent compiler warning */` |
|    10840 | 1004 | `	sToken.pUserData = 0;` |
|    10840 | 1005 | `	iNest = 0;` |
|    10840 | 1006 | `	sDoc.nByte = 0;` |
|    10840 | 1007 | `	sDoc.zString = ""; /* cc warning */` |
|    10840 | 1008 | `	for(;;){` |
|    21682 | 1009 | `		if( zIn >= zEnd ){` |
|        - | 1010 | `			/* End of input reached */` |
|    10836 | 1011 | `			break;` |
|        - | 1012 | `		}` |
|    10848 | 1013 | `		sToken.nLine = nLine;` |
|    10848 | 1014 | `		zCur = zIn;` |
|    10848 | 1015 | `		zCurEnd = 0;` |
|    10856 | 1016 | `		while( zIn < zEnd ){` |
|    10852 | 1017 | `			 if( zIn[0] == '<' ){` |
|    10844 | 1018 | `				const char *zTmp = zIn; /* End of raw input marker */` |
|    10844 | 1019 | `				zIn++;` |
|    10844 | 1020 | `				if( zIn < zEnd ){` |
|    10844 | 1021 | `					if( zIn[0] == '?' ){` |
|    10844 | 1022 | `						zIn++;` |
|    10844 | 1023 | `						if( (sxu32)(zEnd - zIn) >= sizeof("php")-1 &&  SyStrnicmp(zIn,"php",sizeof("php")-1) == 0 ){` |
|        - | 1024 | `							/* opening tag: <?php */` |
|    10842 | 1025 | `							zIn += sizeof("php")-1;` |
|     5420 | 1026 | `						}` |
|        - | 1027 | `						/* Look for the closing tag '?>' */` |
|    10844 | 1028 | `						SyStringInitFromBuf(&sCtag,"?>",sizeof("?>")-1);` |
|    10844 | 1029 | `						zCurEnd = zTmp;` |
|    10844 | 1030 | `						break;` |
|        - | 1031 | `					}` |
|      ! 0 | 1032 | `				}` |
|      ! 0 | 1033 | `			}else{` |
|       10 | 1034 | `				if( zIn[0] == '\n' ){` |
|       10 | 1035 | `					nLine++;` |
|        4 | 1036 | `				}` |
|       10 | 1037 | `				zIn++;` |
|        - | 1038 | `			 }` |
|        2 | 1039 | `		} /* While(zIn < zEnd) */` |
|    10848 | 1040 | `		if( zCurEnd == 0 ){` |
|        5 | 1041 | `			zCurEnd = zIn;` |
|        2 | 1042 | `		}` |
|        - | 1043 | `		/* Save the raw token */` |
|    10848 | 1044 | `		SyStringInitFromBuf(&sToken.sData,zCur,zCurEnd - zCur);` |
|    10848 | 1045 | `		sToken.nType = PH7_TOKEN_RAW;` |
|    10848 | 1046 | `		rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|    10848 | 1047 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1048 | `			return rc;` |
|        - | 1049 | `		}` |
|    10848 | 1050 | `		if( zIn >= zEnd ){` |
|        5 | 1051 | `			break;` |
|        - | 1052 | `		}` |
|        - | 1053 | `		/* Ignore leading white space */` |
|    23540 | 1054 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    12698 | 1055 | `			if( zIn[0] == '\n' ){` |
|    11502 | 1056 | `				nLine++;` |
|     5750 | 1057 | `			}` |
|    12698 | 1058 | `			zIn++;` |
|        2 | 1059 | `		}` |
|        - | 1060 | `		/* Delimit the PHP chunk */` |
|    10844 | 1061 | `		sToken.nLine = nLine;` |
|    10844 | 1062 | `		zCur = zIn;` |
|   996922 | 1063 | `		while( (sxu32)(zEnd - zIn) >= sCtag.nByte ){` |
|        - | 1064 | `			const char *zPtr;` |
|   992242 | 1065 | `			if( SyMemcmp(zIn,sCtag.zString,sCtag.nByte) == 0 && iNest < 1 ){` |
|     6164 | 1066 | `				break;` |
|        - | 1067 | `			}` |
|   494977 | 1068 | `			for(;;){` |
|   989956 | 1069 | `				if( zIn[0] != '/' \|\| (zIn[1] != '*' && zIn[1] != '/') /* && sCtag.nByte >= 2 */ ){` |
|   493041 | 1070 | `					break;` |
|        - | 1071 | `				}` |
|     3878 | 1072 | `				zIn += 2;` |
|     3878 | 1073 | `				if( zIn[-1] == '/' ){` |
|        - | 1074 | `					/* Inline comment */` |
|   133532 | 1075 | `					while( zIn < zEnd && zIn[0] != '\n' ){` |
|   129738 | 1076 | `						zIn++;` |
|        2 | 1077 | `					}` |
|     3796 | 1078 | `					if( zIn >= zEnd ){` |
|      ! 0 | 1079 | `						zIn--;` |
|      ! 0 | 1080 | `					}` |
|     1899 | 1081 | `				}else{` |
|        - | 1082 | `					/* Block comment */` |
|     4500 | 1083 | `					while( (sxu32)(zEnd-zIn) >= sizeof("*/") - 1 ){` |
|     4500 | 1084 | `						if( zIn[0] == '*' && zIn[1] == '/' ){` |
|       84 | 1085 | `							zIn += 2;` |
|       84 | 1086 | `							break;` |
|        - | 1087 | `						}` |
|     4418 | 1088 | `						if( zIn[0] == '\n' ){` |
|       28 | 1089 | `							nLine++;` |
|       13 | 1090 | `						}` |
|     4418 | 1091 | `						zIn++;` |
|        2 | 1092 | `					}` |
|        - | 1093 | `				}` |
|        2 | 1094 | `			}` |
|   986080 | 1095 | `			if( zIn[0] == '\n' ){` |
|    34568 | 1096 | `				nLine++;` |
|    34568 | 1097 | `				if( iNest > 0 ){` |
|      156 | 1098 | `					zIn++;` |
|      156 | 1099 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|      ! 0 | 1100 | `						zIn++;` |
|      ! 0 | 1101 | `					}` |
|      156 | 1102 | `					zPtr = zIn;` |
|      864 | 1103 | `					while( zIn < zEnd ){` |
|      864 | 1104 | `						if( (unsigned char)zIn[0] >= 0xc0 ){` |
|        - | 1105 | `							/* UTF-8 stream */` |
|       19 | 1106 | `							zIn++;` |
|       37 | 1107 | `							SX_JMP_UTF8(zIn,zEnd);` |
|      854 | 1108 | `						}else if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       79 | 1109 | `							break;` |
|      ! 0 | 1110 | `						}else{` |
|      692 | 1111 | `							zIn++;` |
|        - | 1112 | `						}` |
|        2 | 1113 | `					}` |
|      156 | 1114 | `					if( (sxu32)(zIn - zPtr) == sDoc.nByte && SyMemcmp(sDoc.zString,zPtr,sDoc.nByte) == 0 ){` |
|       60 | 1115 | `						iNest = 0;` |
|       29 | 1116 | `					}` |
|      156 | 1117 | `					continue;` |
|        2 | 1118 | `				}` |
|   968720 | 1119 | `			}else if ( (sxu32)(zEnd - zIn) >= sizeof("<<<") && zIn[0] == '<' && zIn[1] == '<' && zIn[2] == '<' && iNest < 1){` |
|       62 | 1120 | `				zIn += sizeof("<<<")-1;` |
|       74 | 1121 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|       13 | 1122 | `					zIn++;` |
|        1 | 1123 | `				}` |
|       62 | 1124 | `				if( zIn[0] == '"' \|\| zIn[0] == '\'' ){` |
|       32 | 1125 | `					zIn++;` |
|       15 | 1126 | `				}` |
|       62 | 1127 | `				zPtr = zIn;` |
|      330 | 1128 | `				while( zIn < zEnd ){` |
|      330 | 1129 | `					if( (unsigned char)zIn[0] >= 0xc0 ){` |
|        - | 1130 | `						/* UTF-8 stream */` |
|       19 | 1131 | `						zIn++;` |
|       37 | 1132 | `						SX_JMP_UTF8(zIn,zEnd);` |
|      320 | 1133 | `					}else if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       32 | 1134 | `						break;` |
|      ! 0 | 1135 | `					}else{` |
|      252 | 1136 | `						zIn++;` |
|        - | 1137 | `					}` |
|        2 | 1138 | `				}` |
|       62 | 1139 | `				SyStringInitFromBuf(&sDoc,zPtr,zIn-zPtr);` |
|       62 | 1140 | `				SyStringFullTrim(&sDoc);` |
|       62 | 1141 | `				if( sDoc.nByte > 0 ){` |
|       62 | 1142 | `					iNest++;` |
|       30 | 1143 | `				}` |
|       62 | 1144 | `				continue;` |
|        - | 1145 | `			}` |
|   985866 | 1146 | `			zIn++;` |
|        - | 1147 |  |
|   985866 | 1148 | `			if ( zIn >= zEnd )` |
|      ! 0 | 1149 | `				break;` |
|        2 | 1150 | `		}` |
|    10844 | 1151 | `		if( (sxu32)(zEnd - zIn) < sCtag.nByte ){` |
|     4682 | 1152 | `			zIn = zEnd;` |
|     2340 | 1153 | `		}` |
|    10844 | 1154 | `		if( zCur < zIn ){` |
|        - | 1155 | `			/* Save the PHP chunk for later processing */` |
|     8772 | 1156 | `			sToken.nType = PH7_TOKEN_PHP;` |
|     8772 | 1157 | `			SyStringInitFromBuf(&sToken.sData,zCur,zIn-zCur);` |
|    17518 | 1158 | `			SyStringRightTrim(&sToken.sData); /* Trim trailing white spaces */` |
|     8772 | 1159 | `			rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|     8772 | 1160 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1161 | `				return rc;` |
|        - | 1162 | `			}` |
|     4385 | 1163 | `		}` |
|    10844 | 1164 | `		if( zIn < zEnd ){` |
|        - | 1165 | `			/* Jump the trailing closing tag */` |
|     6164 | 1166 | `			zIn += sCtag.nByte;` |
|     3081 | 1167 | `		}` |
|        2 | 1168 | `	} /* For(;;) */` |
|        - | 1169 |  |
|    10840 | 1170 | ` 	return SXRET_OK;` |
|     5421 | 1171 |  |
|        - | 1172 |  |
