# src/ph7/lex.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 630/665 lines (94.74%)

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
|  5431108 |   19 | `static sxi32 TokenizePHP(SyStream *pStream,SyToken *pToken,void *pUserData,void *pCtxData)` |
|        2 |   20 |  |
|        - |   21 | `	SyString *pStr;` |
|        - |   22 | `	sxi32 rc;` |
|        - |   23 | `	/* Ignore leading white spaces */` |
|  8185372 |   24 | `	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){` |
|        - |   25 | `		/* Advance the stream cursor */` |
|  2754264 |   26 | `		if( pStream->zText[0] == '\n' ){` |
|        - |   27 | `			/* Update line counter */` |
|    25244 |   28 | `			pStream->nLine++;` |
|    12621 |   29 | `		}` |
|  2754264 |   30 | `		pStream->zText++;` |
|        2 |   31 | `	}` |
|  5431110 |   32 | `	if( pStream->zText >= pStream->zEnd ){` |
|        - |   33 | `		/* End of input reached */` |
|      ! 0 |   34 | `		return SXERR_EOF;` |
|        - |   35 | `	}` |
|        - |   36 | `	/* Record token starting position and line */` |
|  5431110 |   37 | `	pToken->nLine = pStream->nLine;` |
|  5431110 |   38 | `	pToken->pUserData = 0;` |
|  5431110 |   39 | `	pStr = &pToken->sData;` |
|  5431110 |   40 | `	SyStringInitFromBuf(pStr,pStream->zText,0);` |
|  6379112 |   41 | `	if( pStream->zText[0] >= 0xc0 \|\| SyisAlpha(pStream->zText[0]) \|\| pStream->zText[0] == '_' ){` |
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
|  1896006 |   52 | `		if( pStream->zText[0] < 0xc0 ){` |
|  1895990 |   53 | `			pStream->zText++;` |
|   947994 |   54 | `		}` |
|  1855637 |   55 | `		for(;;){` |
|  3711276 |   56 | `			zIn = pStream->zText;` |
|  3711276 |   57 | `			if( zIn[0] >= 0xc0 ){` |
|       49 |   58 | `				zIn++;` |
|        - |   59 | `				/* UTF-8 stream */` |
|      109 |   60 | `				while( zIn < pStream->zEnd && ((zIn[0] & 0xc0) == 0x80) ){` |
|       61 |   61 | `					zIn++;` |
|        1 |   62 | `				}` |
|       24 |   63 | `			}` |
|        - |   64 | `			/* Skip alphanumeric stream */` |
| 14744987 |   65 | `			while( zIn < pStream->zEnd && zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_') ){` |
|  9178076 |   66 | `				zIn++;` |
|        2 |   67 | `			}` |
|  3711276 |   68 | `			if( zIn == pStream->zText ){` |
|        - |   69 | `				/* Not an UTF-8 or alphanumeric stream */` |
|  1896006 |   70 | `				break;` |
|        - |   71 | `			}` |
|        - |   72 | `			/* Synchronize pointers */` |
|  1815272 |   73 | `			pStream->zText = zIn;` |
|        2 |   74 | `		}` |
|        - |   75 | `		/* Record token length */` |
|  1896006 |   76 | `		pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  1896006 |   77 | `		nKeyword = KeywordCode(pStr->zString,(int)pStr->nByte);` |
|  1896006 |   78 | `		if( nKeyword != PH7_TK_ID ){` |
|   606392 |   79 | `			if( nKeyword &` |
|        - |   80 | `				(PH7_TKWRD_NEW\|PH7_TKWRD_CLONE\|PH7_TKWRD_AND\|PH7_TKWRD_XOR\|PH7_TKWRD_OR\|PH7_TKWRD_INSTANCEOF\|PH7_TKWRD_SEQ\|PH7_TKWRD_SNE) ){` |
|        - |   81 | `					/* Alpha stream operators [i.e: new,clone,and,instanceof,eq,ne,or,xor],save the operator instance for later processing */` |
|    12574 |   82 | `					pToken->pUserData = (void *)PH7_ExprExtractOperator(pStr,0);` |
|        - |   83 | `					/* Mark as an operator */` |
|    12574 |   84 | `					pToken->nType = PH7_TK_ID\|PH7_TK_OP;` |
|     6288 |   85 | `			}else{` |
|        - |   86 | `				/* We are dealing with a keyword [i.e: while,foreach,class...],save the keyword ID */` |
|   593820 |   87 | `				pToken->nType = PH7_TK_KEYWORD;` |
|   593820 |   88 | `				pToken->pUserData = SX_INT_TO_PTR(nKeyword);` |
|        - |   89 | `			}` |
|   303197 |   90 | `		}else{` |
|        - |   91 | `			/* A simple identifier */` |
|  1289616 |   92 | `			pToken->nType = PH7_TK_ID;` |
|        - |   93 | `		}` |
|   948004 |   94 | `	}else{` |
|        - |   95 | `		sxi32 c;` |
|        - |   96 | `		/* Non-alpha stream */` |
|  3565943 |   97 | `		if( pStream->zText[0] == '#' \|\|` |
|  3535104 |   98 | `			( pStream->zText[0] == '/' &&  &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '/') ){` |
|     3650 |   99 | `				pStream->zText++;` |
|        - |  100 | `				/* Inline comments */` |
|   130898 |  101 | `				while( pStream->zText < pStream->zEnd && pStream->zText[0] != '\n' ){` |
|   127250 |  102 | `					pStream->zText++;` |
|        2 |  103 | `				}` |
|        - |  104 | `				/* Tell the upper-layer to ignore this token */` |
|     3650 |  105 | `				return SXERR_CONTINUE;` |
|  3531458 |  106 | `		}else if( pStream->zText[0] == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '*' ){` |
|    57970 |  107 | `			pStream->zText += 2;` |
|        - |  108 | `			/* Block comment */` |
|  1644560 |  109 | `			while( pStream->zText < pStream->zEnd ){` |
|  1644560 |  110 | `				if( pStream->zText[0] == '*' ){` |
|    57996 |  111 | `					if( &pStream->zText[1] >= pStream->zEnd \|\| pStream->zText[1] == '/'  ){` |
|    28986 |  112 | `						break;` |
|        - |  113 | `					}` |
|       13 |  114 | `				}` |
|  1586592 |  115 | `				if( pStream->zText[0] == '\n' ){` |
|       28 |  116 | `					pStream->nLine++;` |
|       13 |  117 | `				}` |
|  1586592 |  118 | `				pStream->zText++;` |
|        2 |  119 | `			}` |
|    57970 |  120 | `			pStream->zText += 2;` |
|        - |  121 | `			/* Tell the upper-layer to ignore this token */` |
|    57970 |  122 | `			return SXERR_CONTINUE;` |
|  3473490 |  123 | `		}else if( SyisDigit(pStream->zText[0]) ){` |
|    79318 |  124 | `			pStream->zText++;` |
|        - |  125 | `			/* Decimal digit stream */` |
|    86562 |  126 | `			while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|     7246 |  127 | `				pStream->zText++;` |
|        2 |  128 | `			}` |
|        - |  129 | `			/* Mark the token as integer until we encounter a real number */` |
|    79318 |  130 | `			pToken->nType = PH7_TK_INTEGER;` |
|    79318 |  131 | `			if( pStream->zText < pStream->zEnd ){` |
|    79318 |  132 | `				c = pStream->zText[0];` |
|    79318 |  133 | `				if( c == '.' ){` |
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
|    79125 |  156 | `				}else if( c=='e' \|\| c=='E' ){` |
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
|    78925 |  171 | `				}else if( c == 'x' \|\| c == 'X' ){` |
|        - |  172 | `					/* Hex digit stream */` |
|       16 |  173 | `					pStream->zText++;` |
|       50 |  174 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisHex(pStream->zText[0]) ){` |
|       35 |  175 | `						pStream->zText++;` |
|        1 |  176 | `					}` |
|    78911 |  177 | `				}else if(c  == 'b' \|\| c == 'B' ){` |
|        - |  178 | `					/* Binary digit stream */` |
|       31 |  179 | `					pStream->zText++;` |
|      198 |  180 | `					while( pStream->zText < pStream->zEnd && (pStream->zText[0] == '0' \|\| pStream->zText[0] == '1') ){` |
|      153 |  181 | `						pStream->zText++;` |
|        1 |  182 | `					}` |
|       15 |  183 | `				}` |
|    39658 |  184 | `			}` |
|        - |  185 | `			/* Record token length */` |
|    79318 |  186 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|    79318 |  187 | `			return SXRET_OK;` |
|        - |  188 | `		}` |
|  3394174 |  189 | `		c = pStream->zText[0];` |
|  3394174 |  190 | `		pStream->zText++; /* Advance the stream cursor */` |
|        - |  191 | `		/* Assume we are dealing with an operator*/` |
|  3394174 |  192 | `		pToken->nType = PH7_TK_OP;` |
|  3394174 |  193 | `		switch(c){` |
|   732234 |  194 | `		case '$': pToken->nType = PH7_TK_DOLLAR; break;` |
|   247928 |  195 | `		case '{': pToken->nType = PH7_TK_OCB;    break;` |
|   247914 |  196 | `		case '}': pToken->nType = PH7_TK_CCB;    break;` |
|   514704 |  197 | `		case '(': pToken->nType = PH7_TK_LPAREN; break;` |
|    61854 |  198 | `		case '[': pToken->nType \|= PH7_TK_OSB;   break; /* Bitwise operation here,since the square bracket token '['` |
|        - |  199 | `														 * is a potential operator [i.e: subscripting] */` |
|    61860 |  200 | `		case ']': pToken->nType = PH7_TK_CSB;    break;` |
|   257345 |  201 | `		case ')': {` |
|   514692 |  202 | `			SySet *pTokSet = pStream->pSet;` |
|        - |  203 | `			/* Assemble type cast operators [i.e: (int),(float),(bool)...] */` |
|   514692 |  204 | `			if( pTokSet->nUsed >= 2 ){` |
|        - |  205 | `				SyToken *pTmp;` |
|        - |  206 | `				/* Peek the last recongnized token */` |
|   514690 |  207 | `				pTmp = (SyToken *)SySetPeek(pTokSet);` |
|   514690 |  208 | `				if( pTmp->nType & PH7_TK_KEYWORD ){` |
|    12344 |  209 | `					sxi32 nID = SX_PTR_TO_INT(pTmp->pUserData);` |
|    12344 |  210 | `					if( (sxu32)nID & (PH7_TKWRD_ARRAY\|PH7_TKWRD_INT\|PH7_TKWRD_FLOAT\|PH7_TKWRD_STRING\|PH7_TKWRD_OBJECT\|PH7_TKWRD_BOOL\|PH7_TKWRD_UNSET) ){` |
|    12278 |  211 | `						pTmp = (SyToken *)SySetAt(pTokSet,pTokSet->nUsed - 2);` |
|    12278 |  212 | `						if( pTmp->nType & PH7_TK_LPAREN ){` |
|        - |  213 | `							/* Merge the three tokens '(' 'TYPE' ')' into a single one */` |
|    12194 |  214 | `							const char * zTypeCast = "(int)";` |
|    12194 |  215 | `							if( nID & PH7_TKWRD_FLOAT ){` |
|     2420 |  216 | `								zTypeCast = "(float)";` |
|    10985 |  217 | `							}else if( nID & PH7_TKWRD_BOOL ){` |
|     2422 |  218 | `								zTypeCast = "(bool)";` |
|     8566 |  219 | `							}else if( nID & PH7_TKWRD_STRING ){` |
|     4834 |  220 | `								zTypeCast = "(string)";` |
|     4940 |  221 | `							}else if( nID & PH7_TKWRD_ARRAY ){` |
|       21 |  222 | `								zTypeCast = "(array)";` |
|     2514 |  223 | `							}else if( nID & PH7_TKWRD_OBJECT ){` |
|       17 |  224 | `								zTypeCast = "(object)";` |
|     2496 |  225 | `							}else if( nID & PH7_TKWRD_UNSET ){` |
|        7 |  226 | `								zTypeCast = "(unset)";` |
|        3 |  227 | `							}` |
|        - |  228 | `							/* Reflect the change */` |
|    12194 |  229 | `							pToken->nType = PH7_TK_OP;` |
|    12194 |  230 | `							SyStringInitFromBuf(&pToken->sData,zTypeCast,SyStrlen(zTypeCast));` |
|        - |  231 | `							/* Save the instance associated with the type cast operator */` |
|    12194 |  232 | `							pToken->pUserData = (void *)PH7_ExprExtractOperator(&pToken->sData,0);` |
|        - |  233 | `							/* Remove the two previous tokens */` |
|    12194 |  234 | `							pTokSet->nUsed -= 2;` |
|    12194 |  235 | `							return SXRET_OK;` |
|        - |  236 | `						}` |
|       42 |  237 | `					}` |
|       75 |  238 | `				}` |
|   251248 |  239 | `			}` |
|   502500 |  240 | `			pToken->nType = PH7_TK_RPAREN;` |
|   502500 |  241 | `			break;` |
|        - |  242 | `				  }` |
|    26049 |  243 | `		case '\'':{` |
|        - |  244 | `			/* Single quoted string */` |
|    52100 |  245 | `			pStr->zString++;` |
|   651848 |  246 | `			while( pStream->zText < pStream->zEnd ){` |
|   651848 |  247 | `				if( pStream->zText[0] == '\''  ){` |
|    52110 |  248 | `					if( pStream->zText[-1] != '\\' ){` |
|    52086 |  249 | `						break;` |
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
|   599750 |  262 | `				if( pStream->zText[0] == '\n' ){` |
|        7 |  263 | `					pStream->nLine++;` |
|        3 |  264 | `				}` |
|   599750 |  265 | `				pStream->zText++;` |
|        2 |  266 | `			}` |
|        - |  267 | `			/* Record token length and type */` |
|    52100 |  268 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|    52100 |  269 | `			pToken->nType = PH7_TK_SSTR;` |
|        - |  270 | `			/* Jump the trailing single quote */` |
|    52100 |  271 | `			pStream->zText++;` |
|    52100 |  272 | `			return SXRET_OK;` |
|        - |  273 | `				  }` |
|     6716 |  274 | `		case '"':{` |
|        - |  275 | `			sxi32 iNest;` |
|        - |  276 | `			/* Double quoted string */` |
|    13434 |  277 | `			pStr->zString++;` |
|   142080 |  278 | `			while( pStream->zText < pStream->zEnd ){` |
|   142080 |  279 | `				if( pStream->zText[0] == '{' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '$'){` |
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
|   142080 |  301 | `				if( pStream->zText[0] == '"' ){` |
|    13534 |  302 | `					if( pStream->zText[-1] != '\\' ){` |
|    13430 |  303 | `						break;` |
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
|   128648 |  316 | `				if( pStream->zText[0] == '\n' ){` |
|        7 |  317 | `					pStream->nLine++;` |
|        3 |  318 | `				}` |
|   128648 |  319 | `				pStream->zText++;` |
|        2 |  320 | `			}` |
|        - |  321 | `			/* Record token length and type */` |
|    13434 |  322 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|    13434 |  323 | `			pToken->nType = PH7_TK_DSTR;` |
|        - |  324 | `			/* Jump the trailing quote */` |
|    13434 |  325 | `			pStream->zText++;` |
|    13434 |  326 | `			return SXRET_OK;` |
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
|      103 |  347 | `		case '\\': pToken->nType = PH7_TK_NSSEP;  break;` |
|     1022 |  348 | `		case ':':` |
|     2046 |  349 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == ':' ){` |
|        - |  350 | `				/* Current operator: '::' */` |
|       90 |  351 | `				pStream->zText++;` |
|       46 |  352 | `			}else{` |
|     1958 |  353 | `				pToken->nType = PH7_TK_COLON; /* Single colon */` |
|        - |  354 | `			}` |
|     2046 |  355 | `			break;` |
|    53222 |  356 | `		case ',': pToken->nType \|= PH7_TK_COMMA;  break; /* Comma is also an operator */` |
|   383976 |  357 | `		case ';': pToken->nType = PH7_TK_SEMI;    break;` |
|        - |  358 | `			/* Handle combined operators [i.e: +=,===,!=== ...] */` |
|   122966 |  359 | `		case '=':` |
|   245934 |  360 | `			pToken->nType \|= PH7_TK_EQUAL;` |
|   245934 |  361 | `			if( pStream->zText < pStream->zEnd ){` |
|   245934 |  362 | `				if( pStream->zText[0] == '=' ){` |
|    15842 |  363 | `					pToken->nType &= ~PH7_TK_EQUAL;` |
|        - |  364 | `					/* Current operator: == */` |
|    15842 |  365 | `					pStream->zText++;` |
|    15842 |  366 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  367 | `						/* Current operator: === */` |
|     3498 |  368 | `						pStream->zText++;` |
|     1750 |  369 | `					}` |
|   238014 |  370 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  371 | `					/* Array operator: => */` |
|     3722 |  372 | `					pToken->nType = PH7_TK_ARRAY_OP;` |
|     3722 |  373 | `					pStream->zText++;` |
|     1862 |  374 | `				}else{` |
|        - |  375 | `					/* TICKET 1433-0010: Reference operator '=&' */` |
|   226374 |  376 | `					const unsigned char *zCur = pStream->zText;` |
|   226374 |  377 | `					sxu32 nLine = 0;` |
|   452724 |  378 | `					while( zCur < pStream->zEnd && zCur[0] < 0xc0 && SyisSpace(zCur[0]) ){` |
|   226352 |  379 | `						if( zCur[0] == '\n' ){` |
|        5 |  380 | `							nLine++;` |
|        2 |  381 | `						}` |
|   226352 |  382 | `						zCur++;` |
|        2 |  383 | `					}` |
|   226374 |  384 | `					if( zCur < pStream->zEnd && zCur[0] == '&' ){` |
|        - |  385 | `						/* Current operator: =& */` |
|       44 |  386 | `						pToken->nType &= ~PH7_TK_EQUAL;` |
|       44 |  387 | `						SyStringInitFromBuf(pStr,"=&",sizeof("=&")-1);` |
|        - |  388 | `						/* Update token stream */` |
|       44 |  389 | `						pStream->zText = &zCur[1];` |
|       44 |  390 | `						pStream->nLine += nLine;` |
|       21 |  391 | `					}` |
|        - |  392 | `				}` |
|   122966 |  393 | `			}` |
|   245934 |  394 | `			break;` |
|    17212 |  395 | `		case '!':` |
|    34426 |  396 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  397 | `				/* Current operator: != */` |
|    14638 |  398 | `				pStream->zText++;` |
|    14638 |  399 | `				if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  400 | `					/* Current operator: !== */` |
|    12194 |  401 | `					pStream->zText++;` |
|     6096 |  402 | `				}` |
|     7318 |  403 | `			}` |
|    34426 |  404 | `			break;` |
|     9880 |  405 | `		case '&':` |
|    19762 |  406 | `			pToken->nType \|= PH7_TK_AMPER;` |
|    19762 |  407 | `			if( pStream->zText < pStream->zEnd ){` |
|    19762 |  408 | `				if( pStream->zText[0] == '&' ){` |
|     7598 |  409 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|        - |  410 | `					/* Current operator: && */` |
|     7598 |  411 | `					pStream->zText++;` |
|    15964 |  412 | `				}else if( pStream->zText[0] == '=' ){` |
|        5 |  413 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|        - |  414 | `					/* Current operator: &= */` |
|        5 |  415 | `					pStream->zText++;` |
|        2 |  416 | `				}` |
|     9880 |  417 | `			}` |
|    19762 |  418 | `			break;` |
|     1280 |  419 | `		case '\|':` |
|     2562 |  420 | `			if( pStream->zText < pStream->zEnd ){` |
|     2562 |  421 | `				if( pStream->zText[0] == '\|' ){` |
|        - |  422 | `					/* Current operator: \|\| */` |
|     2546 |  423 | `					pStream->zText++;` |
|     1289 |  424 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  425 | `					/* Current operator: \|= */` |
|        5 |  426 | `					pStream->zText++;` |
|        2 |  427 | `				}` |
|     1280 |  428 | `			}` |
|     2562 |  429 | `			break;` |
|     6324 |  430 | `		case '+':` |
|    12650 |  431 | `			if( pStream->zText < pStream->zEnd ){` |
|    12648 |  432 | `				if( pStream->zText[0] == '+' ){` |
|        - |  433 | `					/* Current operator: ++ */` |
|     9856 |  434 | `					pStream->zText++;` |
|     7721 |  435 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  436 | `					/* Current operator: += */` |
|       30 |  437 | `					pStream->zText++;` |
|       14 |  438 | `				}` |
|     6323 |  439 | `			}` |
|    12650 |  440 | `			break;` |
|    46313 |  441 | `		case '-':` |
|    92628 |  442 | `			if( pStream->zText < pStream->zEnd ){` |
|    92628 |  443 | `				if( pStream->zText[0] == '-' ){` |
|        - |  444 | `					/* Current operator: -- */` |
|        5 |  445 | `					pStream->zText++;` |
|    92626 |  446 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  447 | `					/* Current operator: -= */` |
|        3 |  448 | `					pStream->zText++;` |
|    92623 |  449 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  450 | `					/* Current operator: -> */` |
|    92166 |  451 | `					pStream->zText++;` |
|    46082 |  452 | `				}` |
|    46313 |  453 | `			}` |
|    92628 |  454 | `			break;` |
|       70 |  455 | `		case '*':` |
|      142 |  456 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  457 | `				/* Current operator: *= */` |
|       11 |  458 | `				pStream->zText++;` |
|        5 |  459 | `			}` |
|      142 |  460 | `			break;` |
|       29 |  461 | `		case '/':` |
|       60 |  462 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  463 | `				/* Current operator: /= */` |
|        3 |  464 | `				pStream->zText++;` |
|        1 |  465 | `			}` |
|       60 |  466 | `			break;` |
|       14 |  467 | `		case '%':` |
|       30 |  468 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  469 | `				/* Current operator: %= */` |
|        3 |  470 | `				pStream->zText++;` |
|        1 |  471 | `			}` |
|       30 |  472 | `			break;` |
|        9 |  473 | `		case '^':` |
|       19 |  474 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  475 | `				/* Current operator: ^= */` |
|        7 |  476 | `				pStream->zText++;` |
|        3 |  477 | `			}` |
|       19 |  478 | `			break;` |
|    25789 |  479 | `		case '.':` |
|    51580 |  480 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  481 | `				/* Current operator: .= */` |
|     2458 |  482 | `				pStream->zText++;` |
|     1228 |  483 | `			}` |
|    51580 |  484 | `			break;` |
|    20687 |  485 | `		case '<':` |
|    41376 |  486 | `			if( pStream->zText < pStream->zEnd ){` |
|    41376 |  487 | `				if( pStream->zText[0] == '<' ){` |
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
|    41312 |  505 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  506 | `					/* Current operator: <> */` |
|        5 |  507 | `					pStream->zText++;` |
|    41302 |  508 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  509 | `					/* Current operator: <= */` |
|       27 |  510 | `					pStream->zText++;` |
|       13 |  511 | `				}` |
|    20659 |  512 | `			}` |
|    41320 |  513 | `			break;` |
|     2512 |  514 | `		case '>':` |
|     5026 |  515 | `			if( pStream->zText < pStream->zEnd ){` |
|     5026 |  516 | `				if( pStream->zText[0] == '>' ){` |
|        - |  517 | `					/* Current operator: >> */` |
|       17 |  518 | `					pStream->zText++;` |
|       17 |  519 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  520 | `						/* Current operator: >>= */` |
|        9 |  521 | `						pStream->zText++;` |
|        5 |  522 | `					}` |
|     5018 |  523 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  524 | `					/* Current operator: >= */` |
|       76 |  525 | `					pStream->zText++;` |
|       37 |  526 | `				}` |
|     2512 |  527 | `			}` |
|     5024 |  528 | `			break;` |
|      978 |  529 | `		default:` |
|     1956 |  530 | `			break;` |
|        - |  531 | `		}` |
|  3316392 |  532 | `		if( pStr->nByte <= 0 ){` |
|        - |  533 | `			/* Record token length */` |
|  3316350 |  534 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  1658174 |  535 | `		}` |
|  3316392 |  536 | `		if( pToken->nType & PH7_TK_OP ){` |
|        - |  537 | `			const ph7_expr_op *pOp;` |
|        - |  538 | `			/* Check if the extracted token is an operator */` |
|   619512 |  539 | `			pOp = PH7_ExprExtractOperator(pStr,(SyToken *)SySetPeek(pStream->pSet));` |
|   619512 |  540 | `			if( pOp == 0 ){` |
|        - |  541 | `				/* Not an operator */` |
|      ! 0 |  542 | `				pToken->nType &= ~PH7_TK_OP;` |
|      ! 0 |  543 | `				if( pToken->nType <= 0 ){` |
|      ! 0 |  544 | `					pToken->nType = PH7_TK_OTHER;` |
|      ! 0 |  545 | `				}` |
|      ! 0 |  546 | `			}else{` |
|        - |  547 | `				/* Save the instance associated with this operator for later processing */` |
|   619512 |  548 | `				pToken->pUserData = (void *)pOp;` |
|        - |  549 | `			}` |
|   309755 |  550 | `		}` |
|        - |  551 | `	}` |
|        - |  552 | `	/* Tell the upper-layer to save the extracted token for later processing */` |
|  5212396 |  553 | `	return SXRET_OK;` |
|  2715556 |  554 |  |
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
|  1896006 |  571 | `static sxu32 KeywordCode(const char *z, int n){` |
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
|  1896006 |  661 | `  if( n<2 ) return PH7_TK_ID;` |
|  1815250 |  662 | `  h = (((int)z[0]*4) ^ ((int)z[n-1]*3) ^ n) % 151;` |
|  2769434 |  663 | `  for(i=((int)aHash[h])-1; i>=0; i=((int)aNext[i])-1){` |
|  1560528 |  664 | `    if( (int)aLen[i]==n && SyMemcmp(&zText[aOffset[i]],z,n)==0 ){` |
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
|   606344 |  749 | `      return aCode[i];` |
|        - |  750 | `    }` |
|   477092 |  751 | `  }` |
|        - |  752 | `  /* Linear fallback for keywords not in the auto-generated hash table */` |
|  1208908 |  753 | `  if( n==5 && SyMemcmp(z,"trait",5)==0 ) return PH7_TKWRD_TRAIT;` |
|  1208864 |  754 | `  if( n==9 && SyMemcmp(z,"insteadof",9)==0 ) return PH7_TKWRD_INSTEADOF;` |
|  1208860 |  755 | `  return PH7_TK_ID;` |
|   948004 |  756 |  |
|        - |  757 | `/* --- End of Automatically generated code --- */` |
|        - |  758 | `/*` |
|        - |  759 | ` * Extract a heredoc/nowdoc text from a raw PHP input.` |
|        - |  760 | ` * According to the PHP language reference manual:` |
|        - |  761 | ` *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier` |
|        - |  762 | ` *  is provided, then a newline. The string itself follows, and then the same identifier again` |
|        - |  763 | ` *  to close the quotation.` |
|        - |  764 | ` *  The closing identifier must begin in the first column of the line. Also, the identifier must` |
|        - |  765 | ` *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric` |
|        - |  766 | ` *  characters and underscores, and must start with a non-digit character or underscore.` |
|        - |  767 | ` *  Heredoc text behaves just like a double-quoted string, without the double quotes.` |
|        - |  768 | ` *  This means that quotes in a heredoc do not need to be escaped, but the escape codes listed` |
|        - |  769 | ` *  above can still be used. Variables are expanded, but the same care must be taken when expressing` |
|        - |  770 | ` *  complex variables inside a heredoc as with strings.` |
|        - |  771 | ` *  Nowdocs are to single-quoted strings what heredocs are to double-quoted strings.` |
|        - |  772 | ` *  A nowdoc is specified similarly to a heredoc, but no parsing is done inside a nowdoc.` |
|        - |  773 | ` *  The construct is ideal for embedding PHP code or other large blocks of text without the need` |
|        - |  774 | ` *  for escaping. It shares some features in common with the SGML <![CDATA[ ]]> construct, in that` |
|        - |  775 | ` *  it declares a block of text which is not for parsing.` |
|        - |  776 | ` *  A nowdoc is identified with the same <<< sequence used for heredocs, but the identifier which follows` |
|        - |  777 | ` *  is enclosed in single quotes, e.g. <<<'EOT'. All the rules for heredoc identifiers also apply to nowdoc` |
|        - |  778 | ` *  identifiers, especially those regarding the appearance of the closing identifier.` |
|        - |  779 | ` * Symisc Extension:` |
|        - |  780 | ` * The closing delimiter can now start with a digit or undersocre or it can be an UTF-8 stream.` |
|        - |  781 | ` * Example:` |
|        - |  782 | ` *  <<<123` |
|        - |  783 | ` *    HEREDOC Here` |
|        - |  784 | ` * 123` |
|        - |  785 | ` *  or` |
|        - |  786 | ` *  <<<___` |
|        - |  787 | ` *   HEREDOC Here` |
|        - |  788 | ` *  ___` |
|        - |  789 | ` */` |
|       56 |  790 | `static sxi32 LexExtractHeredoc(SyStream *pStream,SyToken *pToken)` |
|        2 |  791 |  |
|       58 |  792 | `	const unsigned char *zIn  = pStream->zText;` |
|       58 |  793 | `	const unsigned char *zEnd = pStream->zEnd;` |
|        - |  794 | `	const unsigned char *zPtr;` |
|       58 |  795 | `	sxu8 bNowDoc = FALSE;` |
|        - |  796 | `	SyString sDelim;` |
|        - |  797 | `	SyString sStr;` |
|        - |  798 | `	/* Jump leading white spaces */` |
|       70 |  799 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|       13 |  800 | `		zIn++;` |
|        1 |  801 | `	}` |
|       58 |  802 | `	if( zIn >= zEnd ){` |
|        - |  803 | `		/* A simple symbol,return immediately */` |
|      ! 0 |  804 | `		return SXERR_CONTINUE;` |
|        - |  805 | `	}` |
|       58 |  806 | `	if( zIn[0] == '\'' \|\| zIn[0] == '"' ){` |
|        - |  807 | `		/* Make sure we are dealing with a nowdoc */` |
|       29 |  808 | `		bNowDoc =  zIn[0] == '\'' ? TRUE : FALSE;` |
|       29 |  809 | `		zIn++;` |
|       14 |  810 | `	}` |
|       58 |  811 | `	if( zIn[0] < 0xc0 && !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|        - |  812 | `		/* Invalid delimiter,return immediately */` |
|      ! 0 |  813 | `		return SXERR_CONTINUE;` |
|        - |  814 | `	}` |
|        - |  815 | `	/* Isolate the identifier */` |
|       58 |  816 | `	sDelim.zString = (const char *)zIn;` |
|       64 |  817 | `	for(;;){` |
|      130 |  818 | `		zPtr = zIn;` |
|        - |  819 | `		/* Skip alphanumeric stream */` |
|      424 |  820 | `		while( zPtr < zEnd && zPtr[0] < 0xc0 && (SyisAlphaNum(zPtr[0]) \|\| zPtr[0] == '_') ){` |
|      232 |  821 | `			zPtr++;` |
|        2 |  822 | `		}` |
|      130 |  823 | `		if( zPtr < zEnd && zPtr[0] >= 0xc0 ){` |
|       19 |  824 | `			zPtr++;` |
|        - |  825 | `			/* UTF-8 stream */` |
|       37 |  826 | `			while( zPtr < zEnd && ((zPtr[0] & 0xc0) == 0x80) ){` |
|       19 |  827 | `				zPtr++;` |
|        1 |  828 | `			}` |
|        9 |  829 | `		}` |
|      130 |  830 | `		if( zPtr == zIn ){` |
|        - |  831 | `			/* Not an UTF-8 or alphanumeric stream */` |
|       58 |  832 | `			break;` |
|        - |  833 | `		}` |
|        - |  834 | `		/* Synchronize pointers */` |
|       74 |  835 | `		zIn = zPtr;` |
|        2 |  836 | `	}` |
|        - |  837 | `	/* Get the identifier length */` |
|       58 |  838 | `	sDelim.nByte = (sxu32)((const char *)zIn-sDelim.zString);` |
|       58 |  839 | `	if( zIn[0] == '"' \|\| (bNowDoc && zIn[0] == '\'') ){` |
|        - |  840 | `		/* Jump the trailing single quote */` |
|       29 |  841 | `		zIn++;` |
|       14 |  842 | `	}` |
|        - |  843 | `	/* Jump trailing white spaces */` |
|       58 |  844 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|      ! 0 |  845 | `		zIn++;` |
|      ! 0 |  846 | `	}` |
|       58 |  847 | `	if( sDelim.nByte <= 0 \|\| zIn >= zEnd \|\| zIn[0] != '\n' ){` |
|        - |  848 | `		/* Invalid syntax */` |
|      ! 0 |  849 | `		return SXERR_CONTINUE;` |
|        - |  850 | `	}` |
|       58 |  851 | `	pStream->nLine++; /* Increment line counter */` |
|       58 |  852 | `	zIn++;` |
|        - |  853 | `	/* Isolate the delimited string */` |
|       58 |  854 | `	sStr.zString = (const char *)zIn;` |
|        - |  855 | `	/* Go and found the closing delimiter */` |
|       75 |  856 | `	for(;;){` |
|        - |  857 | `		/* Synchronize with the next line */` |
|     3018 |  858 | `		while( zIn < zEnd && zIn[0] != '\n' ){` |
|     2868 |  859 | `			zIn++;` |
|        2 |  860 | `		}` |
|      152 |  861 | `		if( zIn >= zEnd ){` |
|        - |  862 | `			/* End of the input reached, break immediately */` |
|       12 |  863 | `			pStream->zText = pStream->zEnd;` |
|       12 |  864 | `			break;` |
|        - |  865 | `		}` |
|      142 |  866 | `		pStream->nLine++; /* Increment line counter */` |
|      142 |  867 | `		zIn++;` |
|      142 |  868 | `		if( (sxu32)(zEnd - zIn) >= sDelim.nByte && SyMemcmp((const void *)sDelim.zString,(const void *)zIn,sDelim.nByte) == 0 ){` |
|       50 |  869 | `			zPtr = &zIn[sDelim.nByte];` |
|       62 |  870 | `			while( zPtr < zEnd && zPtr[0] < 0xc0 && SyisSpace(zPtr[0]) && zPtr[0] != '\n' ){` |
|       13 |  871 | `				zPtr++;` |
|        1 |  872 | `			}` |
|       50 |  873 | `			if( zPtr >= zEnd ){` |
|        - |  874 | `				/* End of input */` |
|      ! 0 |  875 | `				pStream->zText = zPtr;` |
|      ! 0 |  876 | `				break;` |
|        - |  877 | `			}` |
|       50 |  878 | `			if( zPtr[0] == ';' ){` |
|       50 |  879 | `				const unsigned char *zCur = zPtr;` |
|       50 |  880 | `				zPtr++;` |
|       52 |  881 | `				while( zPtr < zEnd && zPtr[0] < 0xc0 && SyisSpace(zPtr[0]) && zPtr[0] != '\n' ){` |
|        3 |  882 | `					zPtr++;` |
|        1 |  883 | `				}` |
|       50 |  884 | `				if( zPtr >= zEnd \|\| zPtr[0] == '\n' ){` |
|        - |  885 | `					/* Closing delimiter found,break immediately */` |
|       48 |  886 | `					pStream->zText = zCur; /* Keep the semi-colon */` |
|       48 |  887 | `					break;` |
|        1 |  888 | `				}` |
|        1 |  889 | `			}else if( zPtr[0] == '\n' ){` |
|        - |  890 | `				/* Closing delimiter found,break immediately */` |
|      ! 0 |  891 | `				pStream->zText = zPtr; /* Synchronize with the stream cursor */` |
|      ! 0 |  892 | `				break;` |
|        - |  893 | `			}` |
|        - |  894 | `			/* Synchronize pointers and continue searching */` |
|        3 |  895 | `			zIn = zPtr;` |
|        1 |  896 | `		}` |
|        2 |  897 | `	} /* For(;;) */` |
|        - |  898 | `	/* Get the delimited string length */` |
|       58 |  899 | `	sStr.nByte = (sxu32)((const char *)zIn-sStr.zString);` |
|        - |  900 | `	/* Record token type and length */` |
|       58 |  901 | `	pToken->nType = bNowDoc ? PH7_TK_NOWDOC : PH7_TK_HEREDOC;` |
|       58 |  902 | `	SyStringDupPtr(&pToken->sData,&sStr);` |
|        - |  903 | `	/* Remove trailing white spaces */` |
|      104 |  904 | `	SyStringRightTrim(&pToken->sData);` |
|        - |  905 | `	/* All done */` |
|       58 |  906 | `	return SXRET_OK;` |
|       30 |  907 |  |
|        - |  908 | `/*` |
|        - |  909 | ` * Tokenize a raw PHP input.` |
|        - |  910 | ` * This is the public tokenizer called by most code generator routines.` |
|        - |  911 | ` */` |
|    12094 |  912 | `PH7_PRIVATE sxi32 PH7_TokenizePHP(const char *zInput,sxu32 nLen,sxu32 nLineStart,SySet *pOut)` |
|        2 |  913 |  |
|        - |  914 | `	SyLex sLexer;` |
|        - |  915 | `	sxi32 rc;` |
|        - |  916 | `	/* Initialize the lexer */` |
|    12096 |  917 | `	rc = SyLexInit(&sLexer,&(*pOut),TokenizePHP,0);` |
|    12096 |  918 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  919 | `		return rc;` |
|        - |  920 | `	}` |
|    12096 |  921 | `	sLexer.sStream.nLine = nLineStart;` |
|        - |  922 | `	/* Tokenize input */` |
|    12096 |  923 | `	rc = SyLexTokenizeInput(&sLexer,zInput,nLen,0,0,0);` |
|        - |  924 | `	/* Release the lexer */` |
|    12096 |  925 | `	SyLexRelease(&sLexer);` |
|        - |  926 | `	/* Tokenization result */` |
|    12096 |  927 | `	return rc;` |
|     6049 |  928 |  |
|        - |  929 | `/*` |
|        - |  930 | ` * High level public tokenizer.` |
|        - |  931 | ` *  Tokenize the input into PHP tokens and raw tokens [i.e: HTML,XML,Raw text...].` |
|        - |  932 | ` * According to the PHP language reference manual` |
|        - |  933 | ` *   When PHP parses a file, it looks for opening and closing tags, which tell PHP` |
|        - |  934 | ` *   to start and stop interpreting the code between them. Parsing in this manner allows` |
|        - |  935 | ` *   PHP to be embedded in all sorts of different documents, as everything outside of a pair` |
|        - |  936 | ` *   of opening and closing tags is ignored by the PHP parser. Most of the time you will see` |
|        - |  937 | ` *   PHP embedded in HTML documents, as in this example.` |
|        - |  938 | ` *   <?php echo 'While this is going to be parsed.'; ?>` |
|        - |  939 | ` *   <p>This will also be ignored.</p>` |
|        - |  940 | ` *   You can also use more advanced structures:` |
|        - |  941 | ` *   Example #1 Advanced escaping` |
|        - |  942 | ` * <?php` |
|        - |  943 | ` * if ($expression) {` |
|        - |  944 | ` *   ?>` |
|        - |  945 | ` *   <strong>This is true.</strong>` |
|        - |  946 | ` *   <?php` |
|        - |  947 | ` * } else {` |
|        - |  948 | ` *   ?>` |
|        - |  949 | ` *   <strong>This is false.</strong>` |
|        - |  950 | ` *   <?php` |
|        - |  951 | ` * }` |
|        - |  952 | ` * ?>` |
|        - |  953 | ` * This works as expected, because when PHP hits the ?> closing tags, it simply starts outputting` |
|        - |  954 | ` * whatever it finds (except for an immediately following newline - see instruction separation ) until it hits` |
|        - |  955 | ` * another opening tag. The example given here is contrived, of course, but for outputting large blocks of text` |
|        - |  956 | ` * dropping out of PHP parsing mode is generally more efficient than sending all of the text through echo() or print().` |
|        - |  957 | ` * There are four different pairs of opening and closing tags which can be used in PHP. Three of those, <?php ?>` |
|        - |  958 | ` * <script language="php"> </script>  and <? ?> are always available. The other two are short tags and ASP style` |
|        - |  959 | ` * tags, and can be turned on and off from the php.ini configuration file. As such, while some people find short tags` |
|        - |  960 | ` * and ASP style tags convenient, they are less portable, and generally not recommended.` |
|        - |  961 | ` * Note:` |
|        - |  962 | ` * Also note that if you are embedding PHP within XML or XHTML you will need to use the <?php ?> tags to remain` |
|        - |  963 | ` * compliant with standards.` |
|        - |  964 | ` * Example #2 PHP Opening and Closing Tags` |
|        - |  965 | ` * 1.  <?php echo 'if you want to serve XHTML or XML documents, do it like this'; ?>` |
|        - |  966 | ` * 2.  <script language="php">` |
|        - |  967 | ` *       echo 'some editors (like FrontPage) don\'t` |
|        - |  968 | ` *             like processing instructions';` |
|        - |  969 | ` *   </script>` |
|        - |  970 | ` *` |
|        - |  971 | ` * 3.  <? echo 'this is the simplest, an SGML processing instruction'; ?>` |
|        - |  972 | ` *   <?= expression ?> This is a shortcut for "<? echo expression ?>"` |
|        - |  973 | ` */` |
|     9938 |  974 | `PH7_PRIVATE sxi32 PH7_TokenizeRawText(const char *zInput,sxu32 nLen,SySet *pOut)` |
|        2 |  975 |  |
|     9940 |  976 | `	const char *zEnd = &zInput[nLen];` |
|     9940 |  977 | `	const char *zIn  = zInput;` |
|        - |  978 | `	const char *zCur,*zCurEnd;` |
|     9940 |  979 | `	SyString sCtag = { 0, 0 };     /* Closing tag */` |
|        - |  980 | `	SyToken sToken;` |
|        - |  981 | `	SyString sDoc;` |
|        - |  982 | `	sxu32 nLine;` |
|        - |  983 | `	sxi32 iNest;` |
|        - |  984 | `	sxi32 rc;` |
|        - |  985 | `	/* Tokenize the input into PHP tokens and raw tokens */` |
|     9940 |  986 | `	nLine = 1;` |
|     9940 |  987 | `	zCur = zCurEnd   = 0; /* Prevent compiler warning */` |
|     9940 |  988 | `	sToken.pUserData = 0;` |
|     9940 |  989 | `	iNest = 0;` |
|     9940 |  990 | `	sDoc.nByte = 0;` |
|     9940 |  991 | `	sDoc.zString = ""; /* cc warning */` |
|     9940 |  992 | `	for(;;){` |
|    19882 |  993 | `		if( zIn >= zEnd ){` |
|        - |  994 | `			/* End of input reached */` |
|     9936 |  995 | `			break;` |
|        - |  996 | `		}` |
|     9948 |  997 | `		sToken.nLine = nLine;` |
|     9948 |  998 | `		zCur = zIn;` |
|     9948 |  999 | `		zCurEnd = 0;` |
|     9956 | 1000 | `		while( zIn < zEnd ){` |
|     9952 | 1001 | `			 if( zIn[0] == '<' ){` |
|     9944 | 1002 | `				const char *zTmp = zIn; /* End of raw input marker */` |
|     9944 | 1003 | `				zIn++;` |
|     9944 | 1004 | `				if( zIn < zEnd ){` |
|     9944 | 1005 | `					if( zIn[0] == '?' ){` |
|     9944 | 1006 | `						zIn++;` |
|     9944 | 1007 | `						if( (sxu32)(zEnd - zIn) >= sizeof("php")-1 &&  SyStrnicmp(zIn,"php",sizeof("php")-1) == 0 ){` |
|        - | 1008 | `							/* opening tag: <?php */` |
|     9942 | 1009 | `							zIn += sizeof("php")-1;` |
|     4970 | 1010 | `						}` |
|        - | 1011 | `						/* Look for the closing tag '?>' */` |
|     9944 | 1012 | `						SyStringInitFromBuf(&sCtag,"?>",sizeof("?>")-1);` |
|     9944 | 1013 | `						zCurEnd = zTmp;` |
|     9944 | 1014 | `						break;` |
|        - | 1015 | `					}` |
|      ! 0 | 1016 | `				}` |
|      ! 0 | 1017 | `			}else{` |
|       10 | 1018 | `				if( zIn[0] == '\n' ){` |
|       10 | 1019 | `					nLine++;` |
|        4 | 1020 | `				}` |
|       10 | 1021 | `				zIn++;` |
|        - | 1022 | `			 }` |
|        2 | 1023 | `		} /* While(zIn < zEnd) */` |
|     9948 | 1024 | `		if( zCurEnd == 0 ){` |
|        5 | 1025 | `			zCurEnd = zIn;` |
|        2 | 1026 | `		}` |
|        - | 1027 | `		/* Save the raw token */` |
|     9948 | 1028 | `		SyStringInitFromBuf(&sToken.sData,zCur,zCurEnd - zCur);` |
|     9948 | 1029 | `		sToken.nType = PH7_TOKEN_RAW;` |
|     9948 | 1030 | `		rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|     9948 | 1031 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1032 | `			return rc;` |
|        - | 1033 | `		}` |
|     9948 | 1034 | `		if( zIn >= zEnd ){` |
|        5 | 1035 | `			break;` |
|        - | 1036 | `		}` |
|        - | 1037 | `		/* Ignore leading white space */` |
|    21618 | 1038 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    11676 | 1039 | `			if( zIn[0] == '\n' ){` |
|    10524 | 1040 | `				nLine++;` |
|     5261 | 1041 | `			}` |
|    11676 | 1042 | `			zIn++;` |
|        2 | 1043 | `		}` |
|        - | 1044 | `		/* Delimit the PHP chunk */` |
|     9944 | 1045 | `		sToken.nLine = nLine;` |
|     9944 | 1046 | `		zCur = zIn;` |
|   879382 | 1047 | `		while( (sxu32)(zEnd - zIn) >= sCtag.nByte ){` |
|        - | 1048 | `			const char *zPtr;` |
|   875130 | 1049 | `			if( SyMemcmp(zIn,sCtag.zString,sCtag.nByte) == 0 && iNest < 1 ){` |
|     5692 | 1050 | `				break;` |
|        - | 1051 | `			}` |
|   436605 | 1052 | `			for(;;){` |
|   873212 | 1053 | `				if( zIn[0] != '/' \|\| (zIn[1] != '*' && zIn[1] != '/') /* && sCtag.nByte >= 2 */ ){` |
|   434721 | 1054 | `					break;` |
|        - | 1055 | `				}` |
|     3774 | 1056 | `				zIn += 2;` |
|     3774 | 1057 | `				if( zIn[-1] == '/' ){` |
|        - | 1058 | `					/* Inline comment */` |
|   128886 | 1059 | `					while( zIn < zEnd && zIn[0] != '\n' ){` |
|   125194 | 1060 | `						zIn++;` |
|        2 | 1061 | `					}` |
|     3694 | 1062 | `					if( zIn >= zEnd ){` |
|      ! 0 | 1063 | `						zIn--;` |
|      ! 0 | 1064 | `					}` |
|     1848 | 1065 | `				}else{` |
|        - | 1066 | `					/* Block comment */` |
|     4400 | 1067 | `					while( (sxu32)(zEnd-zIn) >= sizeof("*/") - 1 ){` |
|     4400 | 1068 | `						if( zIn[0] == '*' && zIn[1] == '/' ){` |
|       82 | 1069 | `							zIn += 2;` |
|       82 | 1070 | `							break;` |
|        - | 1071 | `						}` |
|     4320 | 1072 | `						if( zIn[0] == '\n' ){` |
|       28 | 1073 | `							nLine++;` |
|       13 | 1074 | `						}` |
|     4320 | 1075 | `						zIn++;` |
|        2 | 1076 | `					}` |
|        - | 1077 | `				}` |
|        2 | 1078 | `			}` |
|   869440 | 1079 | `			if( zIn[0] == '\n' ){` |
|    30076 | 1080 | `				nLine++;` |
|    30076 | 1081 | `				if( iNest > 0 ){` |
|      156 | 1082 | `					zIn++;` |
|      156 | 1083 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|      ! 0 | 1084 | `						zIn++;` |
|      ! 0 | 1085 | `					}` |
|      156 | 1086 | `					zPtr = zIn;` |
|      864 | 1087 | `					while( zIn < zEnd ){` |
|      864 | 1088 | `						if( (unsigned char)zIn[0] >= 0xc0 ){` |
|        - | 1089 | `							/* UTF-8 stream */` |
|       19 | 1090 | `							zIn++;` |
|       37 | 1091 | `							SX_JMP_UTF8(zIn,zEnd);` |
|      854 | 1092 | `						}else if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       79 | 1093 | `							break;` |
|      ! 0 | 1094 | `						}else{` |
|      692 | 1095 | `							zIn++;` |
|        - | 1096 | `						}` |
|        2 | 1097 | `					}` |
|      156 | 1098 | `					if( (sxu32)(zIn - zPtr) == sDoc.nByte && SyMemcmp(sDoc.zString,zPtr,sDoc.nByte) == 0 ){` |
|       60 | 1099 | `						iNest = 0;` |
|       29 | 1100 | `					}` |
|      156 | 1101 | `					continue;` |
|        2 | 1102 | `				}` |
|   854326 | 1103 | `			}else if ( (sxu32)(zEnd - zIn) >= sizeof("<<<") && zIn[0] == '<' && zIn[1] == '<' && zIn[2] == '<' && iNest < 1){` |
|       62 | 1104 | `				zIn += sizeof("<<<")-1;` |
|       74 | 1105 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|       13 | 1106 | `					zIn++;` |
|        1 | 1107 | `				}` |
|       62 | 1108 | `				if( zIn[0] == '"' \|\| zIn[0] == '\'' ){` |
|       32 | 1109 | `					zIn++;` |
|       15 | 1110 | `				}` |
|       62 | 1111 | `				zPtr = zIn;` |
|      330 | 1112 | `				while( zIn < zEnd ){` |
|      330 | 1113 | `					if( (unsigned char)zIn[0] >= 0xc0 ){` |
|        - | 1114 | `						/* UTF-8 stream */` |
|       19 | 1115 | `						zIn++;` |
|       37 | 1116 | `						SX_JMP_UTF8(zIn,zEnd);` |
|      320 | 1117 | `					}else if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       32 | 1118 | `						break;` |
|      ! 0 | 1119 | `					}else{` |
|      252 | 1120 | `						zIn++;` |
|        - | 1121 | `					}` |
|        2 | 1122 | `				}` |
|       62 | 1123 | `				SyStringInitFromBuf(&sDoc,zPtr,zIn-zPtr);` |
|       62 | 1124 | `				SyStringFullTrim(&sDoc);` |
|       62 | 1125 | `				if( sDoc.nByte > 0 ){` |
|       62 | 1126 | `					iNest++;` |
|       30 | 1127 | `				}` |
|       62 | 1128 | `				continue;` |
|        - | 1129 | `			}` |
|   869226 | 1130 | `			zIn++;` |
|        - | 1131 |  |
|   869226 | 1132 | `			if ( zIn >= zEnd )` |
|      ! 0 | 1133 | `				break;` |
|        2 | 1134 | `		}` |
|     9944 | 1135 | `		if( (sxu32)(zEnd - zIn) < sCtag.nByte ){` |
|     4254 | 1136 | `			zIn = zEnd;` |
|     2126 | 1137 | `		}` |
|     9944 | 1138 | `		if( zCur < zIn ){` |
|        - | 1139 | `			/* Save the PHP chunk for later processing */` |
|     8144 | 1140 | `			sToken.nType = PH7_TOKEN_PHP;` |
|     8144 | 1141 | `			SyStringInitFromBuf(&sToken.sData,zCur,zIn-zCur);` |
|    16266 | 1142 | `			SyStringRightTrim(&sToken.sData); /* Trim trailing white spaces */` |
|     8144 | 1143 | `			rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|     8144 | 1144 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1145 | `				return rc;` |
|        - | 1146 | `			}` |
|     4071 | 1147 | `		}` |
|     9944 | 1148 | `		if( zIn < zEnd ){` |
|        - | 1149 | `			/* Jump the trailing closing tag */` |
|     5692 | 1150 | `			zIn += sCtag.nByte;` |
|     2845 | 1151 | `		}` |
|        2 | 1152 | `	} /* For(;;) */` |
|        - | 1153 |  |
|     9940 | 1154 | ` 	return SXRET_OK;` |
|     4971 | 1155 |  |
|        - | 1156 |  |
