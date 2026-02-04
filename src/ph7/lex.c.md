# src/ph7/lex.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 628/663 lines (94.72%)

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
| 10735300 |   19 | `static sxi32 TokenizePHP(SyStream *pStream,SyToken *pToken,void *pUserData,void *pCtxData)` |
|        1 |   20 |  |
|        - |   21 | `	SyString *pStr;` |
|        - |   22 | `	sxi32 rc;` |
|        - |   23 | `	/* Ignore leading white spaces */` |
| 15984155 |   24 | `	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){` |
|        - |   25 | `		/* Advance the stream cursor */` |
|  5248855 |   26 | `		if( pStream->zText[0] == '\n' ){` |
|        - |   27 | `			/* Update line counter */` |
|    21571 |   28 | `			pStream->nLine++;` |
|    10785 |   29 | `		}` |
|  5248855 |   30 | `		pStream->zText++;` |
|        1 |   31 | `	}` |
| 10735301 |   32 | `	if( pStream->zText >= pStream->zEnd ){` |
|        - |   33 | `		/* End of input reached */` |
|      ! 0 |   34 | `		return SXERR_EOF;` |
|        - |   35 | `	}` |
|        - |   36 | `	/* Record token starting position and line */` |
| 10735301 |   37 | `	pToken->nLine = pStream->nLine;` |
| 10735301 |   38 | `	pToken->pUserData = 0;` |
| 10735301 |   39 | `	pStr = &pToken->sData;` |
| 10735301 |   40 | `	SyStringInitFromBuf(pStr,pStream->zText,0);` |
| 12611573 |   41 | `	if( pStream->zText[0] >= 0xc0 \|\| SyisAlpha(pStream->zText[0]) \|\| pStream->zText[0] == '_' ){` |
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
|  3752545 |   52 | `		if( pStream->zText[0] < 0xc0 ){` |
|  3752533 |   53 | `			pStream->zText++;` |
|  1876266 |   54 | `		}` |
|  3675792 |   55 | `		for(;;){` |
|  7351585 |   56 | `			zIn = pStream->zText;` |
|  7351585 |   57 | `			if( zIn[0] >= 0xc0 ){` |
|       37 |   58 | `				zIn++;` |
|        - |   59 | `				/* UTF-8 stream */` |
|       81 |   60 | `				while( zIn < pStream->zEnd && ((zIn[0] & 0xc0) == 0x80) ){` |
|       45 |   61 | `					zIn++;` |
|        1 |   62 | `				}` |
|       18 |   63 | `			}` |
|        - |   64 | `			/* Skip alphanumeric stream */` |
| 29070785 |   65 | `			while( zIn < pStream->zEnd && zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_') ){` |
| 18043409 |   66 | `				zIn++;` |
|        1 |   67 | `			}` |
|  7351585 |   68 | `			if( zIn == pStream->zText ){` |
|        - |   69 | `				/* Not an UTF-8 or alphanumeric stream */` |
|  3752545 |   70 | `				break;` |
|        - |   71 | `			}` |
|        - |   72 | `			/* Synchronize pointers */` |
|  3599041 |   73 | `			pStream->zText = zIn;` |
|        1 |   74 | `		}` |
|        - |   75 | `		/* Record token length */` |
|  3752545 |   76 | `		pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  3752545 |   77 | `		nKeyword = KeywordCode(pStr->zString,(int)pStr->nByte);` |
|  3752545 |   78 | `		if( nKeyword != PH7_TK_ID ){` |
|  1190787 |   79 | `			if( nKeyword &` |
|        - |   80 | `				(PH7_TKWRD_NEW\|PH7_TKWRD_CLONE\|PH7_TKWRD_AND\|PH7_TKWRD_XOR\|PH7_TKWRD_OR\|PH7_TKWRD_INSTANCEOF\|PH7_TKWRD_SEQ\|PH7_TKWRD_SNE) ){` |
|        - |   81 | `					/* Alpha stream operators [i.e: new,clone,and,instanceof,eq,ne,or,xor],save the operator instance for later processing */` |
|    11079 |   82 | `					pToken->pUserData = (void *)PH7_ExprExtractOperator(pStr,0);` |
|        - |   83 | `					/* Mark as an operator */` |
|    11079 |   84 | `					pToken->nType = PH7_TK_ID\|PH7_TK_OP;` |
|     5540 |   85 | `			}else{` |
|        - |   86 | `				/* We are dealing with a keyword [i.e: while,foreach,class...],save the keyword ID */` |
|  1179709 |   87 | `				pToken->nType = PH7_TK_KEYWORD;` |
|  1179709 |   88 | `				pToken->pUserData = SX_INT_TO_PTR(nKeyword);` |
|        - |   89 | `			}` |
|   595394 |   90 | `		}else{` |
|        - |   91 | `			/* A simple identifier */` |
|  2561759 |   92 | `			pToken->nType = PH7_TK_ID;` |
|        - |   93 | `		}` |
|  1876273 |   94 | `	}else{` |
|        - |   95 | `		sxi32 c;` |
|        - |   96 | `		/* Non-alpha stream */` |
|  7051369 |   97 | `		if( pStream->zText[0] == '#' \|\|` |
|  6982756 |   98 | `			( pStream->zText[0] == '/' &&  &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '/') ){` |
|     3743 |   99 | `				pStream->zText++;` |
|        - |  100 | `				/* Inline comments */` |
|   128629 |  101 | `				while( pStream->zText < pStream->zEnd && pStream->zText[0] != '\n' ){` |
|   124887 |  102 | `					pStream->zText++;` |
|        1 |  103 | `				}` |
|        - |  104 | `				/* Tell the upper-layer to ignore this token */` |
|     3743 |  105 | `				return SXERR_CONTINUE;` |
|  6979015 |  106 | `		}else if( pStream->zText[0] == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '*' ){` |
|   133423 |  107 | `			pStream->zText += 2;` |
|        - |  108 | `			/* Block comment */` |
|  3716099 |  109 | `			while( pStream->zText < pStream->zEnd ){` |
|  3716099 |  110 | `				if( pStream->zText[0] == '*' ){` |
|   133429 |  111 | `					if( &pStream->zText[1] >= pStream->zEnd \|\| pStream->zText[1] == '/'  ){` |
|    66712 |  112 | `						break;` |
|        - |  113 | `					}` |
|        3 |  114 | `				}` |
|  3582677 |  115 | `				if( pStream->zText[0] == '\n' ){` |
|        7 |  116 | `					pStream->nLine++;` |
|        3 |  117 | `				}` |
|  3582677 |  118 | `				pStream->zText++;` |
|        1 |  119 | `			}` |
|   133423 |  120 | `			pStream->zText += 2;` |
|        - |  121 | `			/* Tell the upper-layer to ignore this token */` |
|   133423 |  122 | `			return SXERR_CONTINUE;` |
|  6845593 |  123 | `		}else if( SyisDigit(pStream->zText[0]) ){` |
|   169995 |  124 | `			pStream->zText++;` |
|        - |  125 | `			/* Decimal digit stream */` |
|   182761 |  126 | `			while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|    12767 |  127 | `				pStream->zText++;` |
|        1 |  128 | `			}` |
|        - |  129 | `			/* Mark the token as integer until we encounter a real number */` |
|   169995 |  130 | `			pToken->nType = PH7_TK_INTEGER;` |
|   169995 |  131 | `			if( pStream->zText < pStream->zEnd ){` |
|   169991 |  132 | `				c = pStream->zText[0];` |
|   169991 |  133 | `				if( c == '.' ){` |
|        - |  134 | `					/* Real number */` |
|      303 |  135 | `					pStream->zText++;` |
|     1295 |  136 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|      993 |  137 | `						pStream->zText++;` |
|        1 |  138 | `					}` |
|      303 |  139 | `					if( pStream->zText < pStream->zEnd ){` |
|      303 |  140 | `						c = pStream->zText[0];` |
|      303 |  141 | `						if( c=='e' \|\| c=='E' ){` |
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
|      151 |  154 | `					}` |
|      303 |  155 | `					pToken->nType = PH7_TK_REAL;` |
|   169840 |  156 | `				}else if( c=='e' \|\| c=='E' ){` |
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
|   169682 |  171 | `				}else if( c == 'x' \|\| c == 'X' ){` |
|        - |  172 | `					/* Hex digit stream */` |
|       15 |  173 | `					pStream->zText++;` |
|       49 |  174 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisHex(pStream->zText[0]) ){` |
|       35 |  175 | `						pStream->zText++;` |
|        1 |  176 | `					}` |
|   169668 |  177 | `				}else if(c  == 'b' \|\| c == 'B' ){` |
|        - |  178 | `					/* Binary digit stream */` |
|       31 |  179 | `					pStream->zText++;` |
|      198 |  180 | `					while( pStream->zText < pStream->zEnd && (pStream->zText[0] == '0' \|\| pStream->zText[0] == '1') ){` |
|      153 |  181 | `						pStream->zText++;` |
|        1 |  182 | `					}` |
|       15 |  183 | `				}` |
|    84995 |  184 | `			}` |
|        - |  185 | `			/* Record token length */` |
|   169995 |  186 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|   169995 |  187 | `			return SXRET_OK;` |
|        - |  188 | `		}` |
|  6675599 |  189 | `		c = pStream->zText[0];` |
|  6675599 |  190 | `		pStream->zText++; /* Advance the stream cursor */` |
|        - |  191 | `		/* Assume we are dealing with an operator*/` |
|  6675599 |  192 | `		pToken->nType = PH7_TK_OP;` |
|  6675599 |  193 | `		switch(c){` |
|  1467379 |  194 | `		case '$': pToken->nType = PH7_TK_DOLLAR; break;` |
|   489089 |  195 | `		case '{': pToken->nType = PH7_TK_OCB;    break;` |
|   489075 |  196 | `		case '}': pToken->nType = PH7_TK_CCB;    break;` |
|  1001769 |  197 | `		case '(': pToken->nType = PH7_TK_LPAREN; break;` |
|   123477 |  198 | `		case '[': pToken->nType \|= PH7_TK_OSB;   break; /* Bitwise operation here,since the square bracket token '['` |
|        - |  199 | `														 * is a potential operator [i.e: subscripting] */` |
|   123483 |  200 | `		case ']': pToken->nType = PH7_TK_CSB;    break;` |
|   500877 |  201 | `		case ')': {` |
|  1001755 |  202 | `			SySet *pTokSet = pStream->pSet;` |
|        - |  203 | `			/* Assemble type cast operators [i.e: (int),(float),(bool)...] */` |
|  1001755 |  204 | `			if( pTokSet->nUsed >= 2 ){` |
|        - |  205 | `				SyToken *pTmp;` |
|        - |  206 | `				/* Peek the last recongnized token */` |
|  1001753 |  207 | `				pTmp = (SyToken *)SySetPeek(pTokSet);` |
|  1001753 |  208 | `				if( pTmp->nType & PH7_TK_KEYWORD ){` |
|    32277 |  209 | `					sxi32 nID = SX_PTR_TO_INT(pTmp->pUserData);` |
|    32277 |  210 | `					if( (sxu32)nID & (PH7_TKWRD_ARRAY\|PH7_TKWRD_INT\|PH7_TKWRD_FLOAT\|PH7_TKWRD_STRING\|PH7_TKWRD_OBJECT\|PH7_TKWRD_BOOL\|PH7_TKWRD_UNSET) ){` |
|    32233 |  211 | `						pTmp = (SyToken *)SySetAt(pTokSet,pTokSet->nUsed - 2);` |
|    32233 |  212 | `						if( pTmp->nType & PH7_TK_LPAREN ){` |
|        - |  213 | `							/* Merge the three tokens '(' 'TYPE' ')' into a single one */` |
|    32129 |  214 | `							const char * zTypeCast = "(int)";` |
|    32129 |  215 | `							if( nID & PH7_TKWRD_FLOAT ){` |
|     5341 |  216 | `								zTypeCast = "(float)";` |
|    29459 |  217 | `							}else if( nID & PH7_TKWRD_BOOL ){` |
|     5345 |  218 | `								zTypeCast = "(bool)";` |
|    24117 |  219 | `							}else if( nID & PH7_TKWRD_STRING ){` |
|    16013 |  220 | `								zTypeCast = "(string)";` |
|    13439 |  221 | `							}else if( nID & PH7_TKWRD_ARRAY ){` |
|       21 |  222 | `								zTypeCast = "(array)";` |
|     5423 |  223 | `							}else if( nID & PH7_TKWRD_OBJECT ){` |
|       17 |  224 | `								zTypeCast = "(object)";` |
|     5405 |  225 | `							}else if( nID & PH7_TKWRD_UNSET ){` |
|        7 |  226 | `								zTypeCast = "(unset)";` |
|        3 |  227 | `							}` |
|        - |  228 | `							/* Reflect the change */` |
|    32129 |  229 | `							pToken->nType = PH7_TK_OP;` |
|    32129 |  230 | `							SyStringInitFromBuf(&pToken->sData,zTypeCast,SyStrlen(zTypeCast));` |
|        - |  231 | `							/* Save the instance associated with the type cast operator */` |
|    32129 |  232 | `							pToken->pUserData = (void *)PH7_ExprExtractOperator(&pToken->sData,0);` |
|        - |  233 | `							/* Remove the two previous tokens */` |
|    32129 |  234 | `							pTokSet->nUsed -= 2;` |
|    32129 |  235 | `							return SXRET_OK;` |
|        - |  236 | `						}` |
|       52 |  237 | `					}` |
|       74 |  238 | `				}` |
|   484812 |  239 | `			}` |
|   969627 |  240 | `			pToken->nType = PH7_TK_RPAREN;` |
|   969627 |  241 | `			break;` |
|        - |  242 | `				  }` |
|    35508 |  243 | `		case '\'':{` |
|        - |  244 | `			/* Single quoted string */` |
|    71017 |  245 | `			pStr->zString++;` |
|   341111 |  246 | `			while( pStream->zText < pStream->zEnd ){` |
|   341111 |  247 | `				if( pStream->zText[0] == '\''  ){` |
|    71027 |  248 | `					if( pStream->zText[-1] != '\\' ){` |
|    71003 |  249 | `						break;` |
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
|   270095 |  262 | `				if( pStream->zText[0] == '\n' ){` |
|        7 |  263 | `					pStream->nLine++;` |
|        3 |  264 | `				}` |
|   270095 |  265 | `				pStream->zText++;` |
|        1 |  266 | `			}` |
|        - |  267 | `			/* Record token length and type */` |
|    71017 |  268 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|    71017 |  269 | `			pToken->nType = PH7_TK_SSTR;` |
|        - |  270 | `			/* Jump the trailing single quote */` |
|    71017 |  271 | `			pStream->zText++;` |
|    71017 |  272 | `			return SXRET_OK;` |
|        - |  273 | `				  }` |
|     6047 |  274 | `		case '"':{` |
|        - |  275 | `			sxi32 iNest;` |
|        - |  276 | `			/* Double quoted string */` |
|    12095 |  277 | `			pStr->zString++;` |
|   131791 |  278 | `			while( pStream->zText < pStream->zEnd ){` |
|   131791 |  279 | `				if( pStream->zText[0] == '{' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '$'){` |
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
|   131791 |  301 | `				if( pStream->zText[0] == '"' ){` |
|    12189 |  302 | `					if( pStream->zText[-1] != '\\' ){` |
|    12091 |  303 | `						break;` |
|      ! 0 |  304 | `					}else{` |
|       99 |  305 | `						const unsigned char *zPtr = &pStream->zText[-2];` |
|       99 |  306 | `						sxi32 i = 1;` |
|      151 |  307 | `						while( zPtr > pStream->zInput && zPtr[0] == '\\' ){` |
|       53 |  308 | `							zPtr--;` |
|       53 |  309 | `							i++;` |
|        1 |  310 | `						}` |
|       99 |  311 | `						if((i&1)==0){` |
|        5 |  312 | `							break;` |
|        - |  313 | `						}` |
|        - |  314 | `					}` |
|       47 |  315 | `				}` |
|   119697 |  316 | `				if( pStream->zText[0] == '\n' ){` |
|        7 |  317 | `					pStream->nLine++;` |
|        3 |  318 | `				}` |
|   119697 |  319 | `				pStream->zText++;` |
|        1 |  320 | `			}` |
|        - |  321 | `			/* Record token length and type */` |
|    12095 |  322 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|    12095 |  323 | `			pToken->nType = PH7_TK_DSTR;` |
|        - |  324 | `			/* Jump the trailing quote */` |
|    12095 |  325 | `			pStream->zText++;` |
|    12095 |  326 | `			return SXRET_OK;` |
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
|       29 |  347 | `		case '\\': pToken->nType = PH7_TK_NSSEP;  break;` |
|      891 |  348 | `		case ':':` |
|     1783 |  349 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == ':' ){` |
|        - |  350 | `				/* Current operator: '::' */` |
|       55 |  351 | `				pStream->zText++;` |
|       28 |  352 | `			}else{` |
|     1729 |  353 | `				pToken->nType = PH7_TK_COLON; /* Single colon */` |
|        - |  354 | `			}` |
|     1783 |  355 | `			break;` |
|    89743 |  356 | `		case ',': pToken->nType \|= PH7_TK_COMMA;  break; /* Comma is also an operator */` |
|   785005 |  357 | `		case ';': pToken->nType = PH7_TK_SEMI;    break;` |
|        - |  358 | `			/* Handle combined operators [i.e: +=,===,!=== ...] */` |
|   251815 |  359 | `		case '=':` |
|   503631 |  360 | `			pToken->nType \|= PH7_TK_EQUAL;` |
|   503631 |  361 | `			if( pStream->zText < pStream->zEnd ){` |
|   503631 |  362 | `				if( pStream->zText[0] == '=' ){` |
|    33243 |  363 | `					pToken->nType &= ~PH7_TK_EQUAL;` |
|        - |  364 | `					/* Current operator: == */` |
|    33243 |  365 | `					pStream->zText++;` |
|    33243 |  366 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  367 | `						/* Current operator: === */` |
|     6307 |  368 | `						pStream->zText++;` |
|     3154 |  369 | `					}` |
|   487010 |  370 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  371 | `					/* Array operator: => */` |
|     5959 |  372 | `					pToken->nType = PH7_TK_ARRAY_OP;` |
|     5959 |  373 | `					pStream->zText++;` |
|     2980 |  374 | `				}else{` |
|        - |  375 | `					/* TICKET 1433-0010: Reference operator '=&' */` |
|   464431 |  376 | `					const unsigned char *zCur = pStream->zText;` |
|   464431 |  377 | `					sxu32 nLine = 0;` |
|   928841 |  378 | `					while( zCur < pStream->zEnd && zCur[0] < 0xc0 && SyisSpace(zCur[0]) ){` |
|   464411 |  379 | `						if( zCur[0] == '\n' ){` |
|        5 |  380 | `							nLine++;` |
|        2 |  381 | `						}` |
|   464411 |  382 | `						zCur++;` |
|        1 |  383 | `					}` |
|   464431 |  384 | `					if( zCur < pStream->zEnd && zCur[0] == '&' ){` |
|        - |  385 | `						/* Current operator: =& */` |
|       45 |  386 | `						pToken->nType &= ~PH7_TK_EQUAL;` |
|       45 |  387 | `						SyStringInitFromBuf(pStr,"=&",sizeof("=&")-1);` |
|        - |  388 | `						/* Update token stream */` |
|       45 |  389 | `						pStream->zText = &zCur[1];` |
|       45 |  390 | `						pStream->nLine += nLine;` |
|       22 |  391 | `					}` |
|        - |  392 | `				}` |
|   251815 |  393 | `			}` |
|   503631 |  394 | `			break;` |
|    34939 |  395 | `		case '!':` |
|    69879 |  396 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  397 | `				/* Current operator: != */` |
|    32119 |  398 | `				pStream->zText++;` |
|    32119 |  399 | `				if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  400 | `					/* Current operator: !== */` |
|    26781 |  401 | `					pStream->zText++;` |
|    13390 |  402 | `				}` |
|    16059 |  403 | `			}` |
|    69879 |  404 | `			break;` |
|    18846 |  405 | `		case '&':` |
|    37693 |  406 | `			pToken->nType \|= PH7_TK_AMPER;` |
|    37693 |  407 | `			if( pStream->zText < pStream->zEnd ){` |
|    37693 |  408 | `				if( pStream->zText[0] == '&' ){` |
|    10941 |  409 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|        - |  410 | `					/* Current operator: && */` |
|    10941 |  411 | `					pStream->zText++;` |
|    32223 |  412 | `				}else if( pStream->zText[0] == '=' ){` |
|        5 |  413 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|        - |  414 | `					/* Current operator: &= */` |
|        5 |  415 | `					pStream->zText++;` |
|        2 |  416 | `				}` |
|    18846 |  417 | `			}` |
|    37693 |  418 | `			break;` |
|     2721 |  419 | `		case '\|':` |
|     5443 |  420 | `			if( pStream->zText < pStream->zEnd ){` |
|     5443 |  421 | `				if( pStream->zText[0] == '\|' ){` |
|        - |  422 | `					/* Current operator: \|\| */` |
|     5427 |  423 | `					pStream->zText++;` |
|     2730 |  424 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  425 | `					/* Current operator: \|= */` |
|        5 |  426 | `					pStream->zText++;` |
|        2 |  427 | `				}` |
|     2721 |  428 | `			}` |
|     5443 |  429 | `			break;` |
|    10907 |  430 | `		case '+':` |
|    21815 |  431 | `			if( pStream->zText < pStream->zEnd ){` |
|    21813 |  432 | `				if( pStream->zText[0] == '+' ){` |
|        - |  433 | `					/* Current operator: ++ */` |
|    21467 |  434 | `					pStream->zText++;` |
|    11080 |  435 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  436 | `					/* Current operator: += */` |
|       33 |  437 | `					pStream->zText++;` |
|       16 |  438 | `				}` |
|    10906 |  439 | `			}` |
|    21815 |  440 | `			break;` |
|   101700 |  441 | `		case '-':` |
|   203401 |  442 | `			if( pStream->zText < pStream->zEnd ){` |
|   203401 |  443 | `				if( pStream->zText[0] == '-' ){` |
|        - |  444 | `					/* Current operator: -- */` |
|        5 |  445 | `					pStream->zText++;` |
|   203399 |  446 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  447 | `					/* Current operator: -= */` |
|        3 |  448 | `					pStream->zText++;` |
|   203396 |  449 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  450 | `					/* Current operator: -> */` |
|   203049 |  451 | `					pStream->zText++;` |
|   101524 |  452 | `				}` |
|   101700 |  453 | `			}` |
|   203401 |  454 | `			break;` |
|       72 |  455 | `		case '*':` |
|      145 |  456 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  457 | `				/* Current operator: *= */` |
|       13 |  458 | `				pStream->zText++;` |
|        6 |  459 | `			}` |
|      145 |  460 | `			break;` |
|       30 |  461 | `		case '/':` |
|       61 |  462 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  463 | `				/* Current operator: /= */` |
|        3 |  464 | `				pStream->zText++;` |
|        1 |  465 | `			}` |
|       61 |  466 | `			break;` |
|       16 |  467 | `		case '%':` |
|       33 |  468 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  469 | `				/* Current operator: %= */` |
|        3 |  470 | `				pStream->zText++;` |
|        1 |  471 | `			}` |
|       33 |  472 | `			break;` |
|        9 |  473 | `		case '^':` |
|       19 |  474 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  475 | `				/* Current operator: ^= */` |
|        7 |  476 | `				pStream->zText++;` |
|        3 |  477 | `			}` |
|       19 |  478 | `			break;` |
|    37125 |  479 | `		case '.':` |
|    74251 |  480 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  481 | `				/* Current operator: .= */` |
|     5365 |  482 | `				pStream->zText++;` |
|     2682 |  483 | `			}` |
|    74251 |  484 | `			break;` |
|    45500 |  485 | `		case '<':` |
|    91001 |  486 | `			if( pStream->zText < pStream->zEnd ){` |
|    91001 |  487 | `				if( pStream->zText[0] == '<' ){` |
|        - |  488 | `					/* Current operator: << */` |
|       77 |  489 | `					pStream->zText++;` |
|       77 |  490 | `					if( pStream->zText < pStream->zEnd ){` |
|       77 |  491 | `						if( pStream->zText[0] == '=' ){` |
|        - |  492 | `							/* Current operator: <<= */` |
|        7 |  493 | `							pStream->zText++;` |
|       74 |  494 | `						}else if( pStream->zText[0] == '<' ){` |
|        - |  495 | `							/* Current Token: <<<  */` |
|       61 |  496 | `							pStream->zText++;` |
|        - |  497 | `							/* This may be the beginning of a Heredoc/Nowdoc string,try to delimit it */` |
|       61 |  498 | `							rc = LexExtractHeredoc(&(*pStream),&(*pToken));` |
|       61 |  499 | `							if( rc == SXRET_OK ){` |
|        - |  500 | `								/* Here/Now doc successfuly extracted */` |
|       61 |  501 | `								return SXRET_OK;` |
|        - |  502 | `							}` |
|      ! 0 |  503 | `						}` |
|        9 |  504 | `					}` |
|    90933 |  505 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  506 | `					/* Current operator: <> */` |
|        5 |  507 | `					pStream->zText++;` |
|    90923 |  508 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  509 | `					/* Current operator: <= */` |
|       27 |  510 | `					pStream->zText++;` |
|       13 |  511 | `				}` |
|    45470 |  512 | `			}` |
|    90941 |  513 | `			break;` |
|     5412 |  514 | `		case '>':` |
|    10825 |  515 | `			if( pStream->zText < pStream->zEnd ){` |
|    10825 |  516 | `				if( pStream->zText[0] == '>' ){` |
|        - |  517 | `					/* Current operator: >> */` |
|       17 |  518 | `					pStream->zText++;` |
|       17 |  519 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  520 | `						/* Current operator: >>= */` |
|        9 |  521 | `						pStream->zText++;` |
|        5 |  522 | `					}` |
|    10817 |  523 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  524 | `					/* Current operator: >= */` |
|       39 |  525 | `					pStream->zText++;` |
|       19 |  526 | `				}` |
|     5412 |  527 | `			}` |
|    10824 |  528 | `			break;` |
|      862 |  529 | `		default:` |
|     1724 |  530 | `			break;` |
|        - |  531 | `		}` |
|  6560297 |  532 | `		if( pStr->nByte <= 0 ){` |
|        - |  533 | `			/* Record token length */` |
|  6560253 |  534 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  3280126 |  535 | `		}` |
|  6560297 |  536 | `		if( pToken->nType & PH7_TK_OP ){` |
|        - |  537 | `			const ph7_expr_op *pOp;` |
|        - |  538 | `			/* Check if the extracted token is an operator */` |
|  1227163 |  539 | `			pOp = PH7_ExprExtractOperator(pStr,(SyToken *)SySetPeek(pStream->pSet));` |
|  1227163 |  540 | `			if( pOp == 0 ){` |
|        - |  541 | `				/* Not an operator */` |
|      ! 0 |  542 | `				pToken->nType &= ~PH7_TK_OP;` |
|      ! 0 |  543 | `				if( pToken->nType <= 0 ){` |
|      ! 0 |  544 | `					pToken->nType = PH7_TK_OTHER;` |
|      ! 0 |  545 | `				}` |
|      ! 0 |  546 | `			}else{` |
|        - |  547 | `				/* Save the instance associated with this operator for later processing */` |
|  1227163 |  548 | `				pToken->pUserData = (void *)pOp;` |
|        - |  549 | `			}` |
|   613581 |  550 | `		}` |
|        - |  551 | `	}` |
|        - |  552 | `	/* Tell the upper-layer to save the extracted token for later processing */` |
| 10312841 |  553 | `	return SXRET_OK;` |
|  5367651 |  554 |  |
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
|  3752545 |  571 | `static sxu32 KeywordCode(const char *z, int n){` |
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
|        - |  601 | `       0,   0,   4,  83,   0,  61,  39,  12,   0,  33,  77,   0,  48,` |
|        - |  602 | `       0,   2,  65,  67,   0,   0,   0,  47,   0,   0,  40,   0,  15,` |
|        - |  603 | `      74,   0,  51,   0,  76,   0,   0,  20,   0,   0,   0,  50,   0,` |
|        - |  604 | `      80,  34,   0,  36,   0,   0,  64,  16,   0,   0,  17,   0,   1,` |
|        - |  605 | `      19,  84,  66,   0,  43,  45,  78,   0,   0,  53,  56,   0,   0,` |
|        - |  606 | `       0,  23,  49,   0,   0,  13,  31,  54,   7,   0,   0,  25,   0,` |
|        - |  607 | `      72,  14,   0,  71,   0,  38,   6,   0,   0,   0,  73,   0,   0,` |
|        - |  608 | `       3,   0,  41,   5,  52,  57,  32,   0,  60,  63,   0,  69,  82,` |
|        - |  609 | `      30,   0,  79,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,` |
|        - |  610 | `       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  81,   0,   0,` |
|        - |  611 | `      62,   0,  11,   0,   0,  58,   0,   0,   0,   0,  59,  75,   0,` |
|        - |  612 | `       0,   0,   0,   0,   0,  35,  27,   0` |
|        - |  613 | `  };` |
|        - |  614 | `  static const unsigned char aNext[84] = {` |
|        - |  615 | `       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,` |
|        - |  616 | `       0,   0,   8,   0,   0,   0,  10,   0,   0,   0,   0,   0,   0,` |
|        - |  617 | `       0,   0,   0,   0,  28,   0,   0,   0,   0,   0,   0,   0,   0,` |
|        - |  618 | `       0,   0,   0,   0,   0,  44,   0,  18,   0,   0,   0,   0,   0,` |
|        - |  619 | `       0,  46,   0,  29,   0,   0,   0,  22,   0,   0,   0,   0,  26,` |
|        - |  620 | `       0,  21,  24,   0,   0,  68,   0,   0,   9,  37,   0,   0,   0,` |
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
|        - |  633 | `       0,   3,   6,  12,  14,  20,  20,  21,  31,  34,  39,  44,  52,` |
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
|  3752545 |  661 | `  if( n<2 ) return PH7_TK_ID;` |
|  3599025 |  662 | `  h = (((int)z[0]*4) ^ ((int)z[n-1]*3) ^ n) % 151;` |
|  5468361 |  663 | `  for(i=((int)aHash[h])-1; i>=0; i=((int)aNext[i])-1){` |
|  3060123 |  664 | `    if( (int)aLen[i]==n && SyMemcmp(&zText[aOffset[i]],z,n)==0 ){` |
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
|  1190787 |  749 | `      return aCode[i];` |
|        - |  750 | `    }` |
|   934667 |  751 | `  }` |
|  2408239 |  752 | `  return PH7_TK_ID;` |
|  1876273 |  753 |  |
|        - |  754 | `/* --- End of Automatically generated code --- */` |
|        - |  755 | `/*` |
|        - |  756 | ` * Extract a heredoc/nowdoc text from a raw PHP input.` |
|        - |  757 | ` * According to the PHP language reference manual:` |
|        - |  758 | ` *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier` |
|        - |  759 | ` *  is provided, then a newline. The string itself follows, and then the same identifier again` |
|        - |  760 | ` *  to close the quotation.` |
|        - |  761 | ` *  The closing identifier must begin in the first column of the line. Also, the identifier must` |
|        - |  762 | ` *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric` |
|        - |  763 | ` *  characters and underscores, and must start with a non-digit character or underscore.` |
|        - |  764 | ` *  Heredoc text behaves just like a double-quoted string, without the double quotes.` |
|        - |  765 | ` *  This means that quotes in a heredoc do not need to be escaped, but the escape codes listed` |
|        - |  766 | ` *  above can still be used. Variables are expanded, but the same care must be taken when expressing` |
|        - |  767 | ` *  complex variables inside a heredoc as with strings.` |
|        - |  768 | ` *  Nowdocs are to single-quoted strings what heredocs are to double-quoted strings.` |
|        - |  769 | ` *  A nowdoc is specified similarly to a heredoc, but no parsing is done inside a nowdoc.` |
|        - |  770 | ` *  The construct is ideal for embedding PHP code or other large blocks of text without the need` |
|        - |  771 | ` *  for escaping. It shares some features in common with the SGML <![CDATA[ ]]> construct, in that` |
|        - |  772 | ` *  it declares a block of text which is not for parsing.` |
|        - |  773 | ` *  A nowdoc is identified with the same <<< sequence used for heredocs, but the identifier which follows` |
|        - |  774 | ` *  is enclosed in single quotes, e.g. <<<'EOT'. All the rules for heredoc identifiers also apply to nowdoc` |
|        - |  775 | ` *  identifiers, especially those regarding the appearance of the closing identifier.` |
|        - |  776 | ` * Symisc Extension:` |
|        - |  777 | ` * The closing delimiter can now start with a digit or undersocre or it can be an UTF-8 stream.` |
|        - |  778 | ` * Example:` |
|        - |  779 | ` *  <<<123` |
|        - |  780 | ` *    HEREDOC Here` |
|        - |  781 | ` * 123` |
|        - |  782 | ` *  or` |
|        - |  783 | ` *  <<<___` |
|        - |  784 | ` *   HEREDOC Here` |
|        - |  785 | ` *  ___` |
|        - |  786 | ` */` |
|       60 |  787 | `static sxi32 LexExtractHeredoc(SyStream *pStream,SyToken *pToken)` |
|        1 |  788 |  |
|       61 |  789 | `	const unsigned char *zIn  = pStream->zText;` |
|       61 |  790 | `	const unsigned char *zEnd = pStream->zEnd;` |
|        - |  791 | `	const unsigned char *zPtr;` |
|       61 |  792 | `	sxu8 bNowDoc = FALSE;` |
|        - |  793 | `	SyString sDelim;` |
|        - |  794 | `	SyString sStr;` |
|        - |  795 | `	/* Jump leading white spaces */` |
|       73 |  796 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|       13 |  797 | `		zIn++;` |
|        1 |  798 | `	}` |
|       61 |  799 | `	if( zIn >= zEnd ){` |
|        - |  800 | `		/* A simple symbol,return immediately */` |
|      ! 0 |  801 | `		return SXERR_CONTINUE;` |
|        - |  802 | `	}` |
|       61 |  803 | `	if( zIn[0] == '\'' \|\| zIn[0] == '"' ){` |
|        - |  804 | `		/* Make sure we are dealing with a nowdoc */` |
|       29 |  805 | `		bNowDoc =  zIn[0] == '\'' ? TRUE : FALSE;` |
|       29 |  806 | `		zIn++;` |
|       14 |  807 | `	}` |
|       61 |  808 | `	if( zIn[0] < 0xc0 && !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|        - |  809 | `		/* Invalid delimiter,return immediately */` |
|      ! 0 |  810 | `		return SXERR_CONTINUE;` |
|        - |  811 | `	}` |
|        - |  812 | `	/* Isolate the identifier */` |
|       61 |  813 | `	sDelim.zString = (const char *)zIn;` |
|       68 |  814 | `	for(;;){` |
|      137 |  815 | `		zPtr = zIn;` |
|        - |  816 | `		/* Skip alphanumeric stream */` |
|      447 |  817 | `		while( zPtr < zEnd && zPtr[0] < 0xc0 && (SyisAlphaNum(zPtr[0]) \|\| zPtr[0] == '_') ){` |
|      243 |  818 | `			zPtr++;` |
|        1 |  819 | `		}` |
|      137 |  820 | `		if( zPtr < zEnd && zPtr[0] >= 0xc0 ){` |
|       19 |  821 | `			zPtr++;` |
|        - |  822 | `			/* UTF-8 stream */` |
|       37 |  823 | `			while( zPtr < zEnd && ((zPtr[0] & 0xc0) == 0x80) ){` |
|       19 |  824 | `				zPtr++;` |
|        1 |  825 | `			}` |
|        9 |  826 | `		}` |
|      137 |  827 | `		if( zPtr == zIn ){` |
|        - |  828 | `			/* Not an UTF-8 or alphanumeric stream */` |
|       61 |  829 | `			break;` |
|        - |  830 | `		}` |
|        - |  831 | `		/* Synchronize pointers */` |
|       77 |  832 | `		zIn = zPtr;` |
|        1 |  833 | `	}` |
|        - |  834 | `	/* Get the identifier length */` |
|       61 |  835 | `	sDelim.nByte = (sxu32)((const char *)zIn-sDelim.zString);` |
|       61 |  836 | `	if( zIn[0] == '"' \|\| (bNowDoc && zIn[0] == '\'') ){` |
|        - |  837 | `		/* Jump the trailing single quote */` |
|       29 |  838 | `		zIn++;` |
|       14 |  839 | `	}` |
|        - |  840 | `	/* Jump trailing white spaces */` |
|       61 |  841 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|      ! 0 |  842 | `		zIn++;` |
|      ! 0 |  843 | `	}` |
|       61 |  844 | `	if( sDelim.nByte <= 0 \|\| zIn >= zEnd \|\| zIn[0] != '\n' ){` |
|        - |  845 | `		/* Invalid syntax */` |
|      ! 0 |  846 | `		return SXERR_CONTINUE;` |
|        - |  847 | `	}` |
|       61 |  848 | `	pStream->nLine++; /* Increment line counter */` |
|       61 |  849 | `	zIn++;` |
|        - |  850 | `	/* Isolate the delimited string */` |
|       61 |  851 | `	sStr.zString = (const char *)zIn;` |
|        - |  852 | `	/* Go and found the closing delimiter */` |
|       78 |  853 | `	for(;;){` |
|        - |  854 | `		/* Synchronize with the next line */` |
|     3133 |  855 | `		while( zIn < zEnd && zIn[0] != '\n' ){` |
|     2977 |  856 | `			zIn++;` |
|        1 |  857 | `		}` |
|      157 |  858 | `		if( zIn >= zEnd ){` |
|        - |  859 | `			/* End of the input reached, break immediately */` |
|       11 |  860 | `			pStream->zText = pStream->zEnd;` |
|       11 |  861 | `			break;` |
|        - |  862 | `		}` |
|      147 |  863 | `		pStream->nLine++; /* Increment line counter */` |
|      147 |  864 | `		zIn++;` |
|      147 |  865 | `		if( (sxu32)(zEnd - zIn) >= sDelim.nByte && SyMemcmp((const void *)sDelim.zString,(const void *)zIn,sDelim.nByte) == 0 ){` |
|       53 |  866 | `			zPtr = &zIn[sDelim.nByte];` |
|       65 |  867 | `			while( zPtr < zEnd && zPtr[0] < 0xc0 && SyisSpace(zPtr[0]) && zPtr[0] != '\n' ){` |
|       13 |  868 | `				zPtr++;` |
|        1 |  869 | `			}` |
|       53 |  870 | `			if( zPtr >= zEnd ){` |
|        - |  871 | `				/* End of input */` |
|      ! 0 |  872 | `				pStream->zText = zPtr;` |
|      ! 0 |  873 | `				break;` |
|        - |  874 | `			}` |
|       53 |  875 | `			if( zPtr[0] == ';' ){` |
|       53 |  876 | `				const unsigned char *zCur = zPtr;` |
|       53 |  877 | `				zPtr++;` |
|       55 |  878 | `				while( zPtr < zEnd && zPtr[0] < 0xc0 && SyisSpace(zPtr[0]) && zPtr[0] != '\n' ){` |
|        3 |  879 | `					zPtr++;` |
|        1 |  880 | `				}` |
|       53 |  881 | `				if( zPtr >= zEnd \|\| zPtr[0] == '\n' ){` |
|        - |  882 | `					/* Closing delimiter found,break immediately */` |
|       51 |  883 | `					pStream->zText = zCur; /* Keep the semi-colon */` |
|       51 |  884 | `					break;` |
|        1 |  885 | `				}` |
|        1 |  886 | `			}else if( zPtr[0] == '\n' ){` |
|        - |  887 | `				/* Closing delimiter found,break immediately */` |
|      ! 0 |  888 | `				pStream->zText = zPtr; /* Synchronize with the stream cursor */` |
|      ! 0 |  889 | `				break;` |
|        - |  890 | `			}` |
|        - |  891 | `			/* Synchronize pointers and continue searching */` |
|        3 |  892 | `			zIn = zPtr;` |
|        1 |  893 | `		}` |
|        1 |  894 | `	} /* For(;;) */` |
|        - |  895 | `	/* Get the delimited string length */` |
|       61 |  896 | `	sStr.nByte = (sxu32)((const char *)zIn-sStr.zString);` |
|        - |  897 | `	/* Record token type and length */` |
|       61 |  898 | `	pToken->nType = bNowDoc ? PH7_TK_NOWDOC : PH7_TK_HEREDOC;` |
|       61 |  899 | `	SyStringDupPtr(&pToken->sData,&sStr);` |
|        - |  900 | `	/* Remove trailing white spaces */` |
|      111 |  901 | `	SyStringRightTrim(&pToken->sData);` |
|        - |  902 | `	/* All done */` |
|       61 |  903 | `	return SXRET_OK;` |
|       31 |  904 |  |
|        - |  905 | `/*` |
|        - |  906 | ` * Tokenize a raw PHP input.` |
|        - |  907 | ` * This is the public tokenizer called by most code generator routines.` |
|        - |  908 | ` */` |
|    11988 |  909 | `PH7_PRIVATE sxi32 PH7_TokenizePHP(const char *zInput,sxu32 nLen,sxu32 nLineStart,SySet *pOut)` |
|        1 |  910 |  |
|        - |  911 | `	SyLex sLexer;` |
|        - |  912 | `	sxi32 rc;` |
|        - |  913 | `	/* Initialize the lexer */` |
|    11989 |  914 | `	rc = SyLexInit(&sLexer,&(*pOut),TokenizePHP,0);` |
|    11989 |  915 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  916 | `		return rc;` |
|        - |  917 | `	}` |
|    11989 |  918 | `	sLexer.sStream.nLine = nLineStart;` |
|        - |  919 | `	/* Tokenize input */` |
|    11989 |  920 | `	rc = SyLexTokenizeInput(&sLexer,zInput,nLen,0,0,0);` |
|        - |  921 | `	/* Release the lexer */` |
|    11989 |  922 | `	SyLexRelease(&sLexer);` |
|        - |  923 | `	/* Tokenization result */` |
|    11989 |  924 | `	return rc;` |
|     5995 |  925 |  |
|        - |  926 | `/*` |
|        - |  927 | ` * High level public tokenizer.` |
|        - |  928 | ` *  Tokenize the input into PHP tokens and raw tokens [i.e: HTML,XML,Raw text...].` |
|        - |  929 | ` * According to the PHP language reference manual` |
|        - |  930 | ` *   When PHP parses a file, it looks for opening and closing tags, which tell PHP` |
|        - |  931 | ` *   to start and stop interpreting the code between them. Parsing in this manner allows` |
|        - |  932 | ` *   PHP to be embedded in all sorts of different documents, as everything outside of a pair` |
|        - |  933 | ` *   of opening and closing tags is ignored by the PHP parser. Most of the time you will see` |
|        - |  934 | ` *   PHP embedded in HTML documents, as in this example.` |
|        - |  935 | ` *   <?php echo 'While this is going to be parsed.'; ?>` |
|        - |  936 | ` *   <p>This will also be ignored.</p>` |
|        - |  937 | ` *   You can also use more advanced structures:` |
|        - |  938 | ` *   Example #1 Advanced escaping` |
|        - |  939 | ` * <?php` |
|        - |  940 | ` * if ($expression) {` |
|        - |  941 | ` *   ?>` |
|        - |  942 | ` *   <strong>This is true.</strong>` |
|        - |  943 | ` *   <?php` |
|        - |  944 | ` * } else {` |
|        - |  945 | ` *   ?>` |
|        - |  946 | ` *   <strong>This is false.</strong>` |
|        - |  947 | ` *   <?php` |
|        - |  948 | ` * }` |
|        - |  949 | ` * ?>` |
|        - |  950 | ` * This works as expected, because when PHP hits the ?> closing tags, it simply starts outputting` |
|        - |  951 | ` * whatever it finds (except for an immediately following newline - see instruction separation ) until it hits` |
|        - |  952 | ` * another opening tag. The example given here is contrived, of course, but for outputting large blocks of text` |
|        - |  953 | ` * dropping out of PHP parsing mode is generally more efficient than sending all of the text through echo() or print().` |
|        - |  954 | ` * There are four different pairs of opening and closing tags which can be used in PHP. Three of those, <?php ?>` |
|        - |  955 | ` * <script language="php"> </script>  and <? ?> are always available. The other two are short tags and ASP style` |
|        - |  956 | ` * tags, and can be turned on and off from the php.ini configuration file. As such, while some people find short tags` |
|        - |  957 | ` * and ASP style tags convenient, they are less portable, and generally not recommended.` |
|        - |  958 | ` * Note:` |
|        - |  959 | ` * Also note that if you are embedding PHP within XML or XHTML you will need to use the <?php ?> tags to remain` |
|        - |  960 | ` * compliant with standards.` |
|        - |  961 | ` * Example #2 PHP Opening and Closing Tags` |
|        - |  962 | ` * 1.  <?php echo 'if you want to serve XHTML or XML documents, do it like this'; ?>` |
|        - |  963 | ` * 2.  <script language="php">` |
|        - |  964 | ` *       echo 'some editors (like FrontPage) don\'t` |
|        - |  965 | ` *             like processing instructions';` |
|        - |  966 | ` *   </script>` |
|        - |  967 | ` *` |
|        - |  968 | ` * 3.  <? echo 'this is the simplest, an SGML processing instruction'; ?>` |
|        - |  969 | ` *   <?= expression ?> This is a shortcut for "<? echo expression ?>"` |
|        - |  970 | ` */` |
|     5340 |  971 | `PH7_PRIVATE sxi32 PH7_TokenizeRawText(const char *zInput,sxu32 nLen,SySet *pOut)` |
|        1 |  972 |  |
|     5341 |  973 | `	const char *zEnd = &zInput[nLen];` |
|     5341 |  974 | `	const char *zIn  = zInput;` |
|        - |  975 | `	const char *zCur,*zCurEnd;` |
|     5341 |  976 | `	SyString sCtag = { 0, 0 };     /* Closing tag */` |
|        - |  977 | `	SyToken sToken;` |
|        - |  978 | `	SyString sDoc;` |
|        - |  979 | `	sxu32 nLine;` |
|        - |  980 | `	sxi32 iNest;` |
|        - |  981 | `	sxi32 rc;` |
|        - |  982 | `	/* Tokenize the input into PHP tokens and raw tokens */` |
|     5341 |  983 | `	nLine = 1;` |
|     5341 |  984 | `	zCur = zCurEnd   = 0; /* Prevent compiler warning */` |
|     5341 |  985 | `	sToken.pUserData = 0;` |
|     5341 |  986 | `	iNest = 0;` |
|     5341 |  987 | `	sDoc.nByte = 0;` |
|     5341 |  988 | `	sDoc.zString = ""; /* cc warning */` |
|     5342 |  989 | `	for(;;){` |
|    10685 |  990 | `		if( zIn >= zEnd ){` |
|        - |  991 | `			/* End of input reached */` |
|     4851 |  992 | `			break;` |
|        - |  993 | `		}` |
|     5835 |  994 | `		sToken.nLine = nLine;` |
|     5835 |  995 | `		zCur = zIn;` |
|     5835 |  996 | `		zCurEnd = 0;` |
|     6335 |  997 | `		while( zIn < zEnd ){` |
|     5845 |  998 | `			 if( zIn[0] == '<' ){` |
|     5345 |  999 | `				const char *zTmp = zIn; /* End of raw input marker */` |
|     5345 | 1000 | `				zIn++;` |
|     5345 | 1001 | `				if( zIn < zEnd ){` |
|     5345 | 1002 | `					if( zIn[0] == '?' ){` |
|     5345 | 1003 | `						zIn++;` |
|     5345 | 1004 | `						if( (sxu32)(zEnd - zIn) >= sizeof("php")-1 &&  SyStrnicmp(zIn,"php",sizeof("php")-1) == 0 ){` |
|        - | 1005 | `							/* opening tag: <?php */` |
|     5343 | 1006 | `							zIn += sizeof("php")-1;` |
|     2671 | 1007 | `						}` |
|        - | 1008 | `						/* Look for the closing tag '?>' */` |
|     5345 | 1009 | `						SyStringInitFromBuf(&sCtag,"?>",sizeof("?>")-1);` |
|     5345 | 1010 | `						zCurEnd = zTmp;` |
|     5345 | 1011 | `						break;` |
|        - | 1012 | `					}` |
|      ! 0 | 1013 | `				}` |
|      ! 0 | 1014 | `			}else{` |
|      501 | 1015 | `				if( zIn[0] == '\n' ){` |
|      497 | 1016 | `					nLine++;` |
|      248 | 1017 | `				}` |
|      501 | 1018 | `				zIn++;` |
|        - | 1019 | `			 }` |
|        1 | 1020 | `		} /* While(zIn < zEnd) */` |
|     5835 | 1021 | `		if( zCurEnd == 0 ){` |
|      491 | 1022 | `			zCurEnd = zIn;` |
|      245 | 1023 | `		}` |
|        - | 1024 | `		/* Save the raw token */` |
|     5835 | 1025 | `		SyStringInitFromBuf(&sToken.sData,zCur,zCurEnd - zCur);` |
|     5835 | 1026 | `		sToken.nType = PH7_TOKEN_RAW;` |
|     5835 | 1027 | `		rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|     5835 | 1028 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1029 | `			return rc;` |
|        - | 1030 | `		}` |
|     5835 | 1031 | `		if( zIn >= zEnd ){` |
|      491 | 1032 | `			break;` |
|        - | 1033 | `		}` |
|        - | 1034 | `		/* Ignore leading white space */` |
|    10707 | 1035 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|     5363 | 1036 | `			if( zIn[0] == '\n' ){` |
|     4109 | 1037 | `				nLine++;` |
|     2054 | 1038 | `			}` |
|     5363 | 1039 | `			zIn++;` |
|        1 | 1040 | `		}` |
|        - | 1041 | `		/* Delimit the PHP chunk */` |
|     5345 | 1042 | `		sToken.nLine = nLine;` |
|     5345 | 1043 | `		zCur = zIn;` |
|   685621 | 1044 | `		while( (sxu32)(zEnd - zIn) >= sCtag.nByte ){` |
|        - | 1045 | `			const char *zPtr;` |
|   685577 | 1046 | `			if( SyMemcmp(zIn,sCtag.zString,sCtag.nByte) == 0 && iNest < 1 ){` |
|     5301 | 1047 | `				break;` |
|        - | 1048 | `			}` |
|   342067 | 1049 | `			for(;;){` |
|   684135 | 1050 | `				if( zIn[0] != '/' \|\| (zIn[1] != '*' && zIn[1] != '/') /* && sCtag.nByte >= 2 */ ){` |
|   340139 | 1051 | `					break;` |
|        - | 1052 | `				}` |
|     3859 | 1053 | `				zIn += 2;` |
|     3859 | 1054 | `				if( zIn[-1] == '/' ){` |
|        - | 1055 | `					/* Inline comment */` |
|   126523 | 1056 | `					while( zIn < zEnd && zIn[0] != '\n' ){` |
|   122737 | 1057 | `						zIn++;` |
|        1 | 1058 | `					}` |
|     3787 | 1059 | `					if( zIn >= zEnd ){` |
|      ! 0 | 1060 | `						zIn--;` |
|      ! 0 | 1061 | `					}` |
|     1894 | 1062 | `				}else{` |
|        - | 1063 | `					/* Block comment */` |
|     3635 | 1064 | `					while( (sxu32)(zEnd-zIn) >= sizeof("*/") - 1 ){` |
|     3635 | 1065 | `						if( zIn[0] == '*' && zIn[1] == '/' ){` |
|       73 | 1066 | `							zIn += 2;` |
|       73 | 1067 | `							break;` |
|        - | 1068 | `						}` |
|     3563 | 1069 | `						if( zIn[0] == '\n' ){` |
|        7 | 1070 | `							nLine++;` |
|        3 | 1071 | `						}` |
|     3563 | 1072 | `						zIn++;` |
|        1 | 1073 | `					}` |
|        - | 1074 | `				}` |
|        1 | 1075 | `			}` |
|   680277 | 1076 | `			if( zIn[0] == '\n' ){` |
|    25885 | 1077 | `				nLine++;` |
|    25885 | 1078 | `				if( iNest > 0 ){` |
|      165 | 1079 | `					zIn++;` |
|      165 | 1080 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|      ! 0 | 1081 | `						zIn++;` |
|      ! 0 | 1082 | `					}` |
|      165 | 1083 | `					zPtr = zIn;` |
|      907 | 1084 | `					while( zIn < zEnd ){` |
|      907 | 1085 | `						if( (unsigned char)zIn[0] >= 0xc0 ){` |
|        - | 1086 | `							/* UTF-8 stream */` |
|       19 | 1087 | `							zIn++;` |
|       37 | 1088 | `							SX_JMP_UTF8(zIn,zEnd);` |
|      898 | 1089 | `						}else if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       83 | 1090 | `							break;` |
|      ! 0 | 1091 | `						}else{` |
|      725 | 1092 | `							zIn++;` |
|        - | 1093 | `						}` |
|        1 | 1094 | `					}` |
|      165 | 1095 | `					if( (sxu32)(zIn - zPtr) == sDoc.nByte && SyMemcmp(sDoc.zString,zPtr,sDoc.nByte) == 0 ){` |
|       63 | 1096 | `						iNest = 0;` |
|       31 | 1097 | `					}` |
|      165 | 1098 | `					continue;` |
|        1 | 1099 | `				}` |
|   667253 | 1100 | `			}else if ( (sxu32)(zEnd - zIn) >= sizeof("<<<") && zIn[0] == '<' && zIn[1] == '<' && zIn[2] == '<' && iNest < 1){` |
|       65 | 1101 | `				zIn += sizeof("<<<")-1;` |
|       77 | 1102 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|       13 | 1103 | `					zIn++;` |
|        1 | 1104 | `				}` |
|       65 | 1105 | `				if( zIn[0] == '"' \|\| zIn[0] == '\'' ){` |
|       31 | 1106 | `					zIn++;` |
|       15 | 1107 | `				}` |
|       65 | 1108 | `				zPtr = zIn;` |
|      345 | 1109 | `				while( zIn < zEnd ){` |
|      345 | 1110 | `					if( (unsigned char)zIn[0] >= 0xc0 ){` |
|        - | 1111 | `						/* UTF-8 stream */` |
|       19 | 1112 | `						zIn++;` |
|       37 | 1113 | `						SX_JMP_UTF8(zIn,zEnd);` |
|      336 | 1114 | `					}else if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       33 | 1115 | `						break;` |
|      ! 0 | 1116 | `					}else{` |
|      263 | 1117 | `						zIn++;` |
|        - | 1118 | `					}` |
|        1 | 1119 | `				}` |
|       65 | 1120 | `				SyStringInitFromBuf(&sDoc,zPtr,zIn-zPtr);` |
|       65 | 1121 | `				SyStringFullTrim(&sDoc);` |
|       65 | 1122 | `				if( sDoc.nByte > 0 ){` |
|       65 | 1123 | `					iNest++;` |
|       32 | 1124 | `				}` |
|       65 | 1125 | `				continue;` |
|        - | 1126 | `			}` |
|   680049 | 1127 | `			zIn++;` |
|        - | 1128 |  |
|   680049 | 1129 | `			if ( zIn >= zEnd )` |
|      ! 0 | 1130 | `				break;` |
|        1 | 1131 | `		}` |
|     5345 | 1132 | `		if( (sxu32)(zEnd - zIn) < sCtag.nByte ){` |
|       45 | 1133 | `			zIn = zEnd;` |
|       22 | 1134 | `		}` |
|     5345 | 1135 | `		if( zCur < zIn ){` |
|        - | 1136 | `			/* Save the PHP chunk for later processing */` |
|     5343 | 1137 | `			sToken.nType = PH7_TOKEN_PHP;` |
|     5343 | 1138 | `			SyStringInitFromBuf(&sToken.sData,zCur,zIn-zCur);` |
|    10677 | 1139 | `			SyStringRightTrim(&sToken.sData); /* Trim trailing white spaces */` |
|     5343 | 1140 | `			rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|     5343 | 1141 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1142 | `				return rc;` |
|        - | 1143 | `			}` |
|     2671 | 1144 | `		}` |
|     5345 | 1145 | `		if( zIn < zEnd ){` |
|        - | 1146 | `			/* Jump the trailing closing tag */` |
|     5301 | 1147 | `			zIn += sCtag.nByte;` |
|     2650 | 1148 | `		}` |
|        1 | 1149 | `	} /* For(;;) */` |
|        - | 1150 |  |
|     5341 | 1151 | ` 	return SXRET_OK;` |
|     2671 | 1152 |  |
|        - | 1153 |  |
