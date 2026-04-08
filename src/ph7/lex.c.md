# src/ph7/lex.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 640/675 lines (94.81%)

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
|  7353744 |   19 | `static sxi32 TokenizePHP(SyStream *pStream,SyToken *pToken,void *pUserData,void *pCtxData)` |
|        2 |   20 |  |
|        - |   21 | `	SyString *pStr;` |
|        - |   22 | `	sxi32 rc;` |
|        - |   23 | `	/* Ignore leading white spaces */` |
| 11062886 |   24 | `	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){` |
|        - |   25 | `		/* Advance the stream cursor */` |
|  3709142 |   26 | `		if( pStream->zText[0] == '\n' ){` |
|        - |   27 | `			/* Update line counter */` |
|    28290 |   28 | `			pStream->nLine++;` |
|    14144 |   29 | `		}` |
|  3709142 |   30 | `		pStream->zText++;` |
|        2 |   31 | `	}` |
|  7353746 |   32 | `	if( pStream->zText >= pStream->zEnd ){` |
|        - |   33 | `		/* End of input reached */` |
|      ! 0 |   34 | `		return SXERR_EOF;` |
|        - |   35 | `	}` |
|        - |   36 | `	/* Record token starting position and line */` |
|  7353746 |   37 | `	pToken->nLine = pStream->nLine;` |
|  7353746 |   38 | `	pToken->pUserData = 0;` |
|  7353746 |   39 | `	pStr = &pToken->sData;` |
|  7353746 |   40 | `	SyStringInitFromBuf(pStr,pStream->zText,0);` |
|  8678044 |   41 | `	if( pStream->zText[0] >= 0xc0 \|\| SyisAlpha(pStream->zText[0]) \|\| pStream->zText[0] == '_' ){` |
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
|  2648598 |   52 | `		if( pStream->zText[0] < 0xc0 ){` |
|  2648582 |   53 | `			pStream->zText++;` |
|  1324290 |   54 | `		}` |
|  2600867 |   55 | `		for(;;){` |
|  5201736 |   56 | `			zIn = pStream->zText;` |
|  5201736 |   57 | `			if( zIn[0] >= 0xc0 ){` |
|       49 |   58 | `				zIn++;` |
|        - |   59 | `				/* UTF-8 stream */` |
|      109 |   60 | `				while( zIn < pStream->zEnd && ((zIn[0] & 0xc0) == 0x80) ){` |
|       61 |   61 | `					zIn++;` |
|        1 |   62 | `				}` |
|       24 |   63 | `			}` |
|        - |   64 | `			/* Skip alphanumeric stream */` |
| 21272423 |   65 | `			while( zIn < pStream->zEnd && zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_') ){` |
| 13469822 |   66 | `				zIn++;` |
|        2 |   67 | `			}` |
|  5201736 |   68 | `			if( zIn == pStream->zText ){` |
|        - |   69 | `				/* Not an UTF-8 or alphanumeric stream */` |
|  2648598 |   70 | `				break;` |
|        - |   71 | `			}` |
|        - |   72 | `			/* Synchronize pointers */` |
|  2553140 |   73 | `			pStream->zText = zIn;` |
|        2 |   74 | `		}` |
|        - |   75 | `		/* Record token length */` |
|  2648598 |   76 | `		pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  2648598 |   77 | `		nKeyword = KeywordCode(pStr->zString,(int)pStr->nByte);` |
|  2648598 |   78 | `		if( nKeyword != PH7_TK_ID ){` |
|   900970 |   79 | `			if( nKeyword &` |
|        - |   80 | `				(PH7_TKWRD_NEW\|PH7_TKWRD_CLONE\|PH7_TKWRD_AND\|PH7_TKWRD_XOR\|PH7_TKWRD_OR\|PH7_TKWRD_INSTANCEOF\|PH7_TKWRD_SEQ\|PH7_TKWRD_SNE) ){` |
|        - |   81 | `					/* Alpha stream operators [i.e: new,clone,and,instanceof,eq,ne,or,xor],save the operator instance for later processing */` |
|    14896 |   82 | `					pToken->pUserData = (void *)PH7_ExprExtractOperator(pStr,0);` |
|        - |   83 | `					/* Mark as an operator */` |
|    14896 |   84 | `					pToken->nType = PH7_TK_ID\|PH7_TK_OP;` |
|     7449 |   85 | `			}else{` |
|        - |   86 | `				/* We are dealing with a keyword [i.e: while,foreach,class...],save the keyword ID */` |
|   886076 |   87 | `				pToken->nType = PH7_TK_KEYWORD;` |
|   886076 |   88 | `				pToken->pUserData = SX_INT_TO_PTR(nKeyword);` |
|        - |   89 | `			}` |
|   450486 |   90 | `		}else{` |
|        - |   91 | `			/* A simple identifier */` |
|  1747630 |   92 | `			pToken->nType = PH7_TK_ID;` |
|        - |   93 | `		}` |
|  1324300 |   94 | `	}else{` |
|        - |   95 | `		sxi32 c;` |
|        - |   96 | `		/* Non-alpha stream */` |
|  4741420 |   97 | `		if( pStream->zText[0] == '#' \|\|` |
|  4705148 |   98 | `			( pStream->zText[0] == '/' &&  &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '/') ){` |
|     3712 |   99 | `				pStream->zText++;` |
|        - |  100 | `				/* Inline comments */` |
|   133960 |  101 | `				while( pStream->zText < pStream->zEnd && pStream->zText[0] != '\n' ){` |
|   130250 |  102 | `					pStream->zText++;` |
|        2 |  103 | `				}` |
|        - |  104 | `				/* Tell the upper-layer to ignore this token */` |
|     3712 |  105 | `				return SXERR_CONTINUE;` |
|  4701440 |  106 | `		}else if( pStream->zText[0] == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '*' ){` |
|    68772 |  107 | `			pStream->zText += 2;` |
|        - |  108 | `			/* Block comment */` |
|  1950660 |  109 | `			while( pStream->zText < pStream->zEnd ){` |
|  1950660 |  110 | `				if( pStream->zText[0] == '*' ){` |
|    68798 |  111 | `					if( &pStream->zText[1] >= pStream->zEnd \|\| pStream->zText[1] == '/'  ){` |
|    34387 |  112 | `						break;` |
|        - |  113 | `					}` |
|       13 |  114 | `				}` |
|  1881890 |  115 | `				if( pStream->zText[0] == '\n' ){` |
|       28 |  116 | `					pStream->nLine++;` |
|       13 |  117 | `				}` |
|  1881890 |  118 | `				pStream->zText++;` |
|        2 |  119 | `			}` |
|    68772 |  120 | `			pStream->zText += 2;` |
|        - |  121 | `			/* Tell the upper-layer to ignore this token */` |
|    68772 |  122 | `			return SXERR_CONTINUE;` |
|  4632670 |  123 | `		}else if( SyisDigit(pStream->zText[0]) ){` |
|    93806 |  124 | `			pStream->zText++;` |
|        - |  125 | `			/* Decimal digit stream */` |
|   102398 |  126 | `			while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|     8594 |  127 | `				pStream->zText++;` |
|        2 |  128 | `			}` |
|        - |  129 | `			/* Mark the token as integer until we encounter a real number */` |
|    93806 |  130 | `			pToken->nType = PH7_TK_INTEGER;` |
|    93806 |  131 | `			if( pStream->zText < pStream->zEnd ){` |
|    93806 |  132 | `				c = pStream->zText[0];` |
|    93806 |  133 | `				if( c == '.' ){` |
|        - |  134 | `					/* Real number */` |
|      390 |  135 | `					pStream->zText++;` |
|     1572 |  136 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|     1184 |  137 | `						pStream->zText++;` |
|        2 |  138 | `					}` |
|      390 |  139 | `					if( pStream->zText < pStream->zEnd ){` |
|      390 |  140 | `						c = pStream->zText[0];` |
|      390 |  141 | `						if( c=='e' \|\| c=='E' ){` |
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
|      194 |  154 | `					}` |
|      390 |  155 | `					pToken->nType = PH7_TK_REAL;` |
|    93612 |  156 | `				}else if( c=='e' \|\| c=='E' ){` |
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
|    93411 |  171 | `				}else if( c == 'x' \|\| c == 'X' ){` |
|        - |  172 | `					/* Hex digit stream */` |
|       26 |  173 | `					pStream->zText++;` |
|       74 |  174 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisHex(pStream->zText[0]) ){` |
|       49 |  175 | `						pStream->zText++;` |
|        1 |  176 | `					}` |
|    93392 |  177 | `				}else if(c  == 'b' \|\| c == 'B' ){` |
|        - |  178 | `					/* Binary digit stream */` |
|      233 |  179 | `					pStream->zText++;` |
|     1319 |  180 | `					while( pStream->zText < pStream->zEnd && (pStream->zText[0] == '0' \|\| pStream->zText[0] == '1') ){` |
|      971 |  181 | `						pStream->zText++;` |
|        1 |  182 | `					}` |
|      116 |  183 | `				}` |
|    46902 |  184 | `			}` |
|        - |  185 | `			/* Record token length */` |
|    93806 |  186 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|    93806 |  187 | `			return SXRET_OK;` |
|        - |  188 | `		}` |
|  4538866 |  189 | `		c = pStream->zText[0];` |
|  4538866 |  190 | `		pStream->zText++; /* Advance the stream cursor */` |
|        - |  191 | `		/* Assume we are dealing with an operator*/` |
|  4538866 |  192 | `		pToken->nType = PH7_TK_OP;` |
|  4538866 |  193 | `		switch(c){` |
|   955422 |  194 | `		case '$': pToken->nType = PH7_TK_DOLLAR; break;` |
|   356888 |  195 | `		case '{': pToken->nType = PH7_TK_OCB;    break;` |
|   356874 |  196 | `		case '}': pToken->nType = PH7_TK_CCB;    break;` |
|   720608 |  197 | `		case '(': pToken->nType = PH7_TK_LPAREN; break;` |
|    73480 |  198 | `		case '[': pToken->nType \|= PH7_TK_OSB;   break; /* Bitwise operation here,since the square bracket token '['` |
|        - |  199 | `														 * is a potential operator [i.e: subscripting] */` |
|    73486 |  200 | `		case ']': pToken->nType = PH7_TK_CSB;    break;` |
|   360297 |  201 | `		case ')': {` |
|   720596 |  202 | `			SySet *pTokSet = pStream->pSet;` |
|        - |  203 | `			/* Assemble type cast operators [i.e: (int),(float),(bool)...] */` |
|   720596 |  204 | `			if( pTokSet->nUsed >= 2 ){` |
|        - |  205 | `				SyToken *pTmp;` |
|        - |  206 | `				/* Peek the last recongnized token */` |
|   720594 |  207 | `				pTmp = (SyToken *)SySetPeek(pTokSet);` |
|   720594 |  208 | `				if( pTmp->nType & PH7_TK_KEYWORD ){` |
|    14626 |  209 | `					sxi32 nID = SX_PTR_TO_INT(pTmp->pUserData);` |
|    14626 |  210 | `					if( (sxu32)nID & (PH7_TKWRD_ARRAY\|PH7_TKWRD_INT\|PH7_TKWRD_FLOAT\|PH7_TKWRD_STRING\|PH7_TKWRD_OBJECT\|PH7_TKWRD_BOOL\|PH7_TKWRD_UNSET) ){` |
|    14528 |  211 | `						pTmp = (SyToken *)SySetAt(pTokSet,pTokSet->nUsed - 2);` |
|    14528 |  212 | `						if( pTmp->nType & PH7_TK_LPAREN ){` |
|        - |  213 | `							/* Merge the three tokens '(' 'TYPE' ')' into a single one */` |
|    14444 |  214 | `							const char * zTypeCast = "(int)";` |
|    14444 |  215 | `							if( nID & PH7_TKWRD_FLOAT ){` |
|     2870 |  216 | `								zTypeCast = "(float)";` |
|    13010 |  217 | `							}else if( nID & PH7_TKWRD_BOOL ){` |
|     2872 |  218 | `								zTypeCast = "(bool)";` |
|    10141 |  219 | `							}else if( nID & PH7_TKWRD_STRING ){` |
|     5734 |  220 | `								zTypeCast = "(string)";` |
|     5840 |  221 | `							}else if( nID & PH7_TKWRD_ARRAY ){` |
|       21 |  222 | `								zTypeCast = "(array)";` |
|     2964 |  223 | `							}else if( nID & PH7_TKWRD_OBJECT ){` |
|       17 |  224 | `								zTypeCast = "(object)";` |
|     2946 |  225 | `							}else if( nID & PH7_TKWRD_UNSET ){` |
|        7 |  226 | `								zTypeCast = "(unset)";` |
|        3 |  227 | `							}` |
|        - |  228 | `							/* Reflect the change */` |
|    14444 |  229 | `							pToken->nType = PH7_TK_OP;` |
|    14444 |  230 | `							SyStringInitFromBuf(&pToken->sData,zTypeCast,SyStrlen(zTypeCast));` |
|        - |  231 | `							/* Save the instance associated with the type cast operator */` |
|    14444 |  232 | `							pToken->pUserData = (void *)PH7_ExprExtractOperator(&pToken->sData,0);` |
|        - |  233 | `							/* Remove the two previous tokens */` |
|    14444 |  234 | `							pTokSet->nUsed -= 2;` |
|    14444 |  235 | `							return SXRET_OK;` |
|        - |  236 | `						}` |
|       42 |  237 | `					}` |
|       91 |  238 | `				}` |
|   353075 |  239 | `			}` |
|   706154 |  240 | `			pToken->nType = PH7_TK_RPAREN;` |
|   706154 |  241 | `			break;` |
|        - |  242 | `				  }` |
|    30463 |  243 | `		case '\'':{` |
|        - |  244 | `			/* Single quoted string */` |
|    60928 |  245 | `			pStr->zString++;` |
|   771286 |  246 | `			while( pStream->zText < pStream->zEnd ){` |
|   771286 |  247 | `				if( pStream->zText[0] == '\''  ){` |
|    60938 |  248 | `					if( pStream->zText[-1] != '\\' ){` |
|    60914 |  249 | `						break;` |
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
|   710360 |  262 | `				if( pStream->zText[0] == '\n' ){` |
|       67 |  263 | `					pStream->nLine++;` |
|       33 |  264 | `				}` |
|   710360 |  265 | `				pStream->zText++;` |
|        2 |  266 | `			}` |
|        - |  267 | `			/* Record token length and type */` |
|    60928 |  268 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|    60928 |  269 | `			pToken->nType = PH7_TK_SSTR;` |
|        - |  270 | `			/* Jump the trailing single quote */` |
|    60928 |  271 | `			pStream->zText++;` |
|    60928 |  272 | `			return SXRET_OK;` |
|        - |  273 | `				  }` |
|     7431 |  274 | `		case '"':{` |
|        - |  275 | `			sxi32 iNest;` |
|        - |  276 | `			/* Double quoted string */` |
|    14864 |  277 | `			pStr->zString++;` |
|   151194 |  278 | `			while( pStream->zText < pStream->zEnd ){` |
|   151194 |  279 | `				if( pStream->zText[0] == '{' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '$'){` |
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
|   151194 |  301 | `				if( pStream->zText[0] == '"' ){` |
|    14964 |  302 | `					if( pStream->zText[-1] != '\\' ){` |
|    14860 |  303 | `						break;` |
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
|   136332 |  316 | `				if( pStream->zText[0] == '\n' ){` |
|        7 |  317 | `					pStream->nLine++;` |
|        3 |  318 | `				}` |
|   136332 |  319 | `				pStream->zText++;` |
|        2 |  320 | `			}` |
|        - |  321 | `			/* Record token length and type */` |
|    14864 |  322 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|    14864 |  323 | `			pToken->nType = PH7_TK_DSTR;` |
|        - |  324 | `			/* Jump the trailing quote */` |
|    14864 |  325 | `			pStream->zText++;` |
|    14864 |  326 | `			return SXRET_OK;` |
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
|      115 |  347 | `		case '\\': pToken->nType = PH7_TK_NSSEP;  break;` |
|     1128 |  348 | `		case ':':` |
|     2258 |  349 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == ':' ){` |
|        - |  350 | `				/* Current operator: '::' */` |
|      118 |  351 | `				pStream->zText++;` |
|       60 |  352 | `			}else{` |
|     2142 |  353 | `				pToken->nType = PH7_TK_COLON; /* Single colon */` |
|        - |  354 | `			}` |
|     2258 |  355 | `			break;` |
|    76408 |  356 | `		case ',': pToken->nType \|= PH7_TK_COMMA;  break; /* Comma is also an operator */` |
|   516912 |  357 | `		case ';': pToken->nType = PH7_TK_SEMI;    break;` |
|        - |  358 | `			/* Handle combined operators [i.e: +=,===,!=== ...] */` |
|   149682 |  359 | `		case '=':` |
|   299366 |  360 | `			pToken->nType \|= PH7_TK_EQUAL;` |
|   299366 |  361 | `			if( pStream->zText < pStream->zEnd ){` |
|   299366 |  362 | `				if( pStream->zText[0] == '=' ){` |
|    18610 |  363 | `					pToken->nType &= ~PH7_TK_EQUAL;` |
|        - |  364 | `					/* Current operator: == */` |
|    18610 |  365 | `					pStream->zText++;` |
|    18610 |  366 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  367 | `						/* Current operator: === */` |
|     3992 |  368 | `						pStream->zText++;` |
|     1997 |  369 | `					}` |
|   290062 |  370 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  371 | `					/* Array operator: => */` |
|     4216 |  372 | `					pToken->nType = PH7_TK_ARRAY_OP;` |
|     4216 |  373 | `					pStream->zText++;` |
|     2109 |  374 | `				}else{` |
|        - |  375 | `					/* TICKET 1433-0010: Reference operator '=&' */` |
|   276544 |  376 | `					const unsigned char *zCur = pStream->zText;` |
|   276544 |  377 | `					sxu32 nLine = 0;` |
|   553064 |  378 | `					while( zCur < pStream->zEnd && zCur[0] < 0xc0 && SyisSpace(zCur[0]) ){` |
|   276522 |  379 | `						if( zCur[0] == '\n' ){` |
|        5 |  380 | `							nLine++;` |
|        2 |  381 | `						}` |
|   276522 |  382 | `						zCur++;` |
|        2 |  383 | `					}` |
|   276544 |  384 | `					if( zCur < pStream->zEnd && zCur[0] == '&' ){` |
|        - |  385 | `						/* Current operator: =& */` |
|       46 |  386 | `						pToken->nType &= ~PH7_TK_EQUAL;` |
|       46 |  387 | `						SyStringInitFromBuf(pStr,"=&",sizeof("=&")-1);` |
|        - |  388 | `						/* Update token stream */` |
|       46 |  389 | `						pStream->zText = &zCur[1];` |
|       46 |  390 | `						pStream->nLine += nLine;` |
|       22 |  391 | `					}` |
|        - |  392 | `				}` |
|   149682 |  393 | `			}` |
|   299366 |  394 | `			break;` |
|    20382 |  395 | `		case '!':` |
|    40766 |  396 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  397 | `				/* Current operator: != */` |
|    17352 |  398 | `				pStream->zText++;` |
|    17352 |  399 | `				if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  400 | `					/* Current operator: !== */` |
|    14458 |  401 | `					pStream->zText++;` |
|     7228 |  402 | `				}` |
|     8675 |  403 | `			}` |
|    40766 |  404 | `			break;` |
|    11713 |  405 | `		case '&':` |
|    23428 |  406 | `			pToken->nType \|= PH7_TK_AMPER;` |
|    23428 |  407 | `			if( pStream->zText < pStream->zEnd ){` |
|    23428 |  408 | `				if( pStream->zText[0] == '&' ){` |
|     8986 |  409 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|        - |  410 | `					/* Current operator: && */` |
|     8986 |  411 | `					pStream->zText++;` |
|    18936 |  412 | `				}else if( pStream->zText[0] == '=' ){` |
|        7 |  413 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|        - |  414 | `					/* Current operator: &= */` |
|        7 |  415 | `					pStream->zText++;` |
|        3 |  416 | `				}` |
|    11713 |  417 | `			}` |
|    23428 |  418 | `			break;` |
|     1507 |  419 | `		case '\|':` |
|     3016 |  420 | `			if( pStream->zText < pStream->zEnd ){` |
|     3016 |  421 | `				if( pStream->zText[0] == '\|' ){` |
|        - |  422 | `					/* Current operator: \|\| */` |
|     2996 |  423 | `					pStream->zText++;` |
|     1518 |  424 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  425 | `					/* Current operator: \|= */` |
|        7 |  426 | `					pStream->zText++;` |
|        3 |  427 | `				}` |
|     1507 |  428 | `			}` |
|     3016 |  429 | `			break;` |
|     7487 |  430 | `		case '+':` |
|    14976 |  431 | `			if( pStream->zText < pStream->zEnd ){` |
|    14974 |  432 | `				if( pStream->zText[0] == '+' ){` |
|        - |  433 | `					/* Current operator: ++ */` |
|    11680 |  434 | `					pStream->zText++;` |
|     9135 |  435 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  436 | `					/* Current operator: += */` |
|       40 |  437 | `					pStream->zText++;` |
|       19 |  438 | `				}` |
|     7486 |  439 | `			}` |
|    14976 |  440 | `			break;` |
|    54984 |  441 | `		case '-':` |
|   109970 |  442 | `			if( pStream->zText < pStream->zEnd ){` |
|   109970 |  443 | `				if( pStream->zText[0] == '-' ){` |
|        - |  444 | `					/* Current operator: -- */` |
|        5 |  445 | `					pStream->zText++;` |
|   109968 |  446 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  447 | `					/* Current operator: -= */` |
|        5 |  448 | `					pStream->zText++;` |
|   109964 |  449 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  450 | `					/* Current operator: -> */` |
|   109500 |  451 | `					pStream->zText++;` |
|    54749 |  452 | `				}` |
|    54984 |  453 | `			}` |
|   109970 |  454 | `			break;` |
|       75 |  455 | `		case '*':` |
|      152 |  456 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  457 | `				/* Current operator: *= */` |
|       15 |  458 | `				pStream->zText++;` |
|        7 |  459 | `			}` |
|      152 |  460 | `			break;` |
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
|    30571 |  479 | `		case '.':` |
|    61144 |  480 | `			if( pStream->zText + 1 < pStream->zEnd && pStream->zText[0] == '.' && pStream->zText[1] == '.' ){` |
|        - |  481 | `				/* Ellipsis: ... */` |
|       37 |  482 | `				pStream->zText += 2;` |
|       37 |  483 | `				pToken->nType = PH7_TK_ELLIPSIS;` |
|    61126 |  484 | `			}else if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  485 | `				/* Current operator: .= */` |
|     2932 |  486 | `				pStream->zText++;` |
|     1465 |  487 | `			}` |
|    61144 |  488 | `			break;` |
|    24526 |  489 | `		case '<':` |
|    49054 |  490 | `			if( pStream->zText < pStream->zEnd ){` |
|    49054 |  491 | `				if( pStream->zText[0] == '<' ){` |
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
|    48987 |  509 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  510 | `					/* Current operator: <> */` |
|        5 |  511 | `					pStream->zText++;` |
|    48974 |  512 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  513 | `					/* Current operator: <= */` |
|       36 |  514 | `					pStream->zText++;` |
|       17 |  515 | `				}` |
|    24498 |  516 | `			}` |
|    48998 |  517 | `			break;` |
|     2967 |  518 | `		case '>':` |
|     5936 |  519 | `			if( pStream->zText < pStream->zEnd ){` |
|     5936 |  520 | `				if( pStream->zText[0] == '>' ){` |
|        - |  521 | `					/* Current operator: >> */` |
|       21 |  522 | `					pStream->zText++;` |
|       21 |  523 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  524 | `						/* Current operator: >>= */` |
|       11 |  525 | `						pStream->zText++;` |
|        6 |  526 | `					}` |
|     5926 |  527 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  528 | `					/* Current operator: >= */` |
|       80 |  529 | `					pStream->zText++;` |
|       39 |  530 | `				}` |
|     2967 |  531 | `			}` |
|     5936 |  532 | `			break;` |
|      957 |  533 | `		case '?':` |
|     1916 |  534 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '?' ){` |
|        - |  535 | `				/* Null coalescing operator: ?? */` |
|       31 |  536 | `				pStream->zText++;` |
|       15 |  537 | `			}` |
|     1914 |  538 | `			break;` |
|      107 |  539 | `		default:` |
|      214 |  540 | `			break;` |
|        - |  541 | `		}` |
|  4448576 |  542 | `		if( pStr->nByte <= 0 ){` |
|        - |  543 | `			/* Record token length */` |
|  4448532 |  544 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  2224265 |  545 | `		}` |
|  4448576 |  546 | `		if( pToken->nType & PH7_TK_OP ){` |
|        - |  547 | `			const ph7_expr_op *pOp;` |
|        - |  548 | `			/* Check if the extracted token is an operator */` |
|   755742 |  549 | `			pOp = PH7_ExprExtractOperator(pStr,(SyToken *)SySetPeek(pStream->pSet));` |
|   755742 |  550 | `			if( pOp == 0 ){` |
|        - |  551 | `				/* Not an operator */` |
|      ! 0 |  552 | `				pToken->nType &= ~PH7_TK_OP;` |
|      ! 0 |  553 | `				if( pToken->nType <= 0 ){` |
|      ! 0 |  554 | `					pToken->nType = PH7_TK_OTHER;` |
|      ! 0 |  555 | `				}` |
|      ! 0 |  556 | `			}else{` |
|        - |  557 | `				/* Save the instance associated with this operator for later processing */` |
|   755742 |  558 | `				pToken->pUserData = (void *)pOp;` |
|        - |  559 | `			}` |
|   377870 |  560 | `		}` |
|        - |  561 | `	}` |
|        - |  562 | `	/* Tell the upper-layer to save the extracted token for later processing */` |
|  7097172 |  563 | `	return SXRET_OK;` |
|  3676874 |  564 |  |
|        - |  565 | `/***** This file contains automatically generated code ******` |
|        - |  566 | `**` |
|        - |  567 | `** The code in this file has been automatically generated by` |
|        - |  568 | `**` |
|        - |  569 | `**     $Header: /sqlite/sqlite/tool/mkkeywordhash.c` |
|        - |  570 | `**` |
|        - |  571 | `** Sligthly modified by Chems mrad <chm@symisc.net> for the PH7 engine.` |
|        - |  572 | `**` |
|        - |  573 | `** The code in this file implements a function that determines whether` |
|        - |  574 | `** or not a given identifier is really a PHP keyword.  The same thing` |
|        - |  575 | `** might be implemented more directly using a hand-written hash table.` |
|        - |  576 | `** But by using this automatically generated code, the size of the code` |
|        - |  577 | `** is substantially reduced.  This is important for embedded applications` |
|        - |  578 | `** on platforms with limited memory.` |
|        - |  579 | `*/` |
|        - |  580 | `/* Hash score: 103 */` |
|  2648598 |  581 | `static sxu32 KeywordCode(const char *z, int n){` |
|        - |  582 | `  /* zText[] encodes 532 bytes of keywords in 333 bytes */` |
|        - |  583 | `  /*   extendswitchprintegerequire_oncenddeclareturnamespacechobject      */` |
|        - |  584 | `  /*   hrowbooleandefaultrycaselfinalistaticlonewconstringlobaluse        */` |
|        - |  585 | `  /*   lseifloatvarrayANDIEchoUSECHOabstractclasscontinuendifunction      */` |
|        - |  586 | `  /*   diendwhilevaldoexitgotoimplementsinclude_oncemptyinstanceof        */` |
|        - |  587 | `  /*   interfacendforeachissetparentprivateprotectedpublicatchunset       */` |
|        - |  588 | `  /*   xorARRAYASArrayEXITUNSETXORbreak                                   */` |
|        - |  589 | `  static const char zText[332] = {` |
|        - |  590 | `    'e','x','t','e','n','d','s','w','i','t','c','h','p','r','i','n','t','e',` |
|        - |  591 | `    'g','e','r','e','q','u','i','r','e','_','o','n','c','e','n','d','d','e',` |
|        - |  592 | `    'c','l','a','r','e','t','u','r','n','a','m','e','s','p','a','c','e','c',` |
|        - |  593 | `    'h','o','b','j','e','c','t','h','r','o','w','b','o','o','l','e','a','n',` |
|        - |  594 | `    'd','e','f','a','u','l','t','r','y','c','a','s','e','l','f','i','n','a',` |
|        - |  595 | `    'l','i','s','t','a','t','i','c','l','o','n','e','w','c','o','n','s','t',` |
|        - |  596 | `    'r','i','n','g','l','o','b','a','l','u','s','e','l','s','e','i','f','l',` |
|        - |  597 | `    'o','a','t','v','a','r','r','a','y','A','N','D','I','E','c','h','o','U',` |
|        - |  598 | `    'S','E','C','H','O','a','b','s','t','r','a','c','t','c','l','a','s','s',` |
|        - |  599 | `    'c','o','n','t','i','n','u','e','n','d','i','f','u','n','c','t','i','o',` |
|        - |  600 | `    'n','d','i','e','n','d','w','h','i','l','e','v','a','l','d','o','e','x',` |
|        - |  601 | `    'i','t','g','o','t','o','i','m','p','l','e','m','e','n','t','s','i','n',` |
|        - |  602 | `    'c','l','u','d','e','_','o','n','c','e','m','p','t','y','i','n','s','t',` |
|        - |  603 | `    'a','n','c','e','o','f','i','n','t','e','r','f','a','c','e','n','d','f',` |
|        - |  604 | `    'o','r','e','a','c','h','i','s','s','e','t','p','a','r','e','n','t','p',` |
|        - |  605 | `    'r','i','v','a','t','e','p','r','o','t','e','c','t','e','d','p','u','b',` |
|        - |  606 | `    'l','i','c','a','t','c','h','u','n','s','e','t','x','o','r','A','R','R',` |
|        - |  607 | `    'A','Y','A','S','A','r','r','a','y','E','X','I','T','U','N','S','E','T',` |
|        - |  608 | `    'X','O','R','b','r','e','a','k'` |
|        - |  609 | `  };` |
|        - |  610 | `  static const unsigned char aHash[151] = {` |
|        - |  611 |  |
|        - |  612 |  |
|        - |  613 | `      74,   0,  51,   0,  76,   0,   0,  20,   0,   0,   0,  50,   0,` |
|        - |  614 | `      80,  34,   0,  36,   0,   0,  64,  16,   0,   0,  17,   0,   1,` |
|        - |  615 | `      19,  84,  66,   0,  43,  45,  78,   0,   0,  53,  56,   0,   0,` |
|        - |  616 |  |
|        - |  617 | `      72,  14,   0,  71,   0,  38,   6,   0,   0,   0,  73,   0,   0,` |
|        - |  618 | `       3,   0,  41,   5,  52,  57,  32,   0,  60,  63,   0,  69,  82,` |
|        - |  619 | `      30,   0,  79,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,` |
|        - |  620 |  |
|        - |  621 | `      62,   0,  11,   0,   0,  58,   0,   0,   0,   0,  59,  75,   0,` |
|        - |  622 |  |
|        - |  623 | `  };` |
|        - |  624 | `  static const unsigned char aNext[84] = {` |
|        - |  625 |  |
|        - |  626 |  |
|        - |  627 |  |
|        - |  628 |  |
|        - |  629 |  |
|        - |  630 |  |
|        - |  631 | `      42,   0,   0,   0,  70,  55` |
|        - |  632 | `  };` |
|        - |  633 | `  static const unsigned char aLen[84] = {` |
|        - |  634 | `       7,   9,   6,   5,   7,  12,   7,   2,  10,   7,   6,   9,   4,` |
|        - |  635 | `       6,   5,   7,   4,   3,   7,   3,   4,   4,   5,   4,   6,   5,` |
|        - |  636 | `       2,   3,   5,   6,   6,   3,   6,   4,   2,   5,   3,   5,   3,` |
|        - |  637 | `       3,   4,   3,   4,   8,   5,   2,   8,   5,   8,   3,   8,   5,` |
|        - |  638 | `       4,   2,   4,   4,  10,  12,   7,   5,  10,   9,   3,   6,  10,` |
|        - |  639 | `       3,   7,   2,   5,   6,   7,   9,   6,   5,   5,   3,   5,   2,` |
|        - |  640 | `       5,   4,   5,   3,   2,   5` |
|        - |  641 | `  };` |
|        - |  642 | `  static const sxu16 aOffset[84] = {` |
|        - |  643 |  |
|        - |  644 | `      55,  60,  65,  65,  70,  72,  78,  81,  83,  86,  90,  92,  97,` |
|        - |  645 | `     100, 100, 103, 106, 111, 117, 119, 119, 123, 124, 129, 130, 135,` |
|        - |  646 | `     137, 139, 143, 145, 149, 157, 159, 162, 169, 173, 181, 183, 186,` |
|        - |  647 | `     190, 194, 196, 200, 204, 214, 214, 225, 230, 240, 240, 248, 248,` |
|        - |  648 | `     251, 251, 252, 258, 263, 269, 276, 285, 290, 295, 300, 303, 308,` |
|        - |  649 | `     310, 315, 319, 324, 325, 327` |
|        - |  650 | `  };` |
|        - |  651 | `  static const sxu32 aCode[84] = {` |
|        - |  652 | `    PH7_TKWRD_EXTENDS,   PH7_TKWRD_ENDSWITCH,   PH7_TKWRD_SWITCH,    PH7_TKWRD_PRINT,   PH7_TKWRD_INT,` |
|        - |  653 | `    PH7_TKWRD_REQONCE,   PH7_TKWRD_REQUIRE,     PH7_TKWRD_SEQ,       PH7_TKWRD_ENDDEC,    PH7_TKWRD_DECLARE,` |
|        - |  654 | `    PH7_TKWRD_RETURN,    PH7_TKWRD_NAMESPACE,   PH7_TKWRD_ECHO,      PH7_TKWRD_OBJECT,    PH7_TKWRD_THROW,` |
|        - |  655 | `    PH7_TKWRD_BOOL,      PH7_TKWRD_BOOL,        PH7_TKWRD_AND,       PH7_TKWRD_DEFAULT,   PH7_TKWRD_TRY,` |
|        - |  656 | `    PH7_TKWRD_CASE,      PH7_TKWRD_SELF,        PH7_TKWRD_FINAL,     PH7_TKWRD_LIST,      PH7_TKWRD_STATIC,` |
|        - |  657 | `    PH7_TKWRD_CLONE,     PH7_TKWRD_SNE,         PH7_TKWRD_NEW,       PH7_TKWRD_CONST,     PH7_TKWRD_STRING,` |
|        - |  658 | `    PH7_TKWRD_GLOBAL,    PH7_TKWRD_USE,         PH7_TKWRD_ELIF,      PH7_TKWRD_ELSE,      PH7_TKWRD_IF,` |
|        - |  659 | `    PH7_TKWRD_FLOAT,     PH7_TKWRD_VAR,         PH7_TKWRD_ARRAY,     PH7_TKWRD_AND,       PH7_TKWRD_DIE,` |
|        - |  660 | `    PH7_TKWRD_ECHO,      PH7_TKWRD_USE,         PH7_TKWRD_ECHO,      PH7_TKWRD_ABSTRACT,  PH7_TKWRD_CLASS,` |
|        - |  661 | `    PH7_TKWRD_AS,        PH7_TKWRD_CONTINUE,    PH7_TKWRD_ENDIF,     PH7_TKWRD_FUNCTION,  PH7_TKWRD_DIE,` |
|        - |  662 | `    PH7_TKWRD_ENDWHILE,  PH7_TKWRD_WHILE,       PH7_TKWRD_EVAL,      PH7_TKWRD_DO,        PH7_TKWRD_EXIT,` |
|        - |  663 | `    PH7_TKWRD_GOTO,      PH7_TKWRD_IMPLEMENTS,  PH7_TKWRD_INCONCE,   PH7_TKWRD_INCLUDE,   PH7_TKWRD_EMPTY,` |
|        - |  664 | `    PH7_TKWRD_INSTANCEOF,PH7_TKWRD_INTERFACE,   PH7_TKWRD_INT,       PH7_TKWRD_ENDFOR,    PH7_TKWRD_END4EACH,` |
|        - |  665 | `    PH7_TKWRD_FOR,       PH7_TKWRD_FOREACH,     PH7_TKWRD_OR,        PH7_TKWRD_ISSET,     PH7_TKWRD_PARENT,` |
|        - |  666 | `    PH7_TKWRD_PRIVATE,   PH7_TKWRD_PROTECTED,   PH7_TKWRD_PUBLIC,    PH7_TKWRD_CATCH,     PH7_TKWRD_UNSET,` |
|        - |  667 | `    PH7_TKWRD_XOR,       PH7_TKWRD_ARRAY,       PH7_TKWRD_AS,        PH7_TKWRD_ARRAY,     PH7_TKWRD_EXIT,` |
|        - |  668 | `    PH7_TKWRD_UNSET,     PH7_TKWRD_XOR,         PH7_TKWRD_OR,        PH7_TKWRD_BREAK` |
|        - |  669 | `  };` |
|        - |  670 | `  int h, i;` |
|  2648598 |  671 | `  if( n<2 ) return PH7_TK_ID;` |
|  2553118 |  672 | `  h = (((int)z[0]*4) ^ ((int)z[n-1]*3) ^ n) % 151;` |
|  3910466 |  673 | `  for(i=((int)aHash[h])-1; i>=0; i=((int)aNext[i])-1){` |
|  2258204 |  674 | `    if( (int)aLen[i]==n && SyMemcmp(&zText[aOffset[i]],z,n)==0 ){` |
|        - |  675 | `       /* PH7_TKWRD_EXTENDS */` |
|        - |  676 | `       /* PH7_TKWRD_ENDSWITCH */` |
|        - |  677 | `       /* PH7_TKWRD_SWITCH */` |
|        - |  678 | `       /* PH7_TKWRD_PRINT */` |
|        - |  679 | `       /* PH7_TKWRD_INT */` |
|        - |  680 | `       /* PH7_TKWRD_REQONCE */` |
|        - |  681 | `       /* PH7_TKWRD_REQUIRE */` |
|        - |  682 | `       /* PH7_TKWRD_SEQ */` |
|        - |  683 | `       /* PH7_TKWRD_ENDDEC */` |
|        - |  684 | `       /* PH7_TKWRD_DECLARE */` |
|        - |  685 | `       /* PH7_TKWRD_RETURN */` |
|        - |  686 | `       /* PH7_TKWRD_NAMESPACE */` |
|        - |  687 | `       /* PH7_TKWRD_ECHO */` |
|        - |  688 | `       /* PH7_TKWRD_OBJECT */` |
|        - |  689 | `       /* PH7_TKWRD_THROW */` |
|        - |  690 | `       /* PH7_TKWRD_BOOL */` |
|        - |  691 | `       /* PH7_TKWRD_BOOL */` |
|        - |  692 | `       /* PH7_TKWRD_AND */` |
|        - |  693 | `       /* PH7_TKWRD_DEFAULT */` |
|        - |  694 | `       /* PH7_TKWRD_TRY */` |
|        - |  695 | `       /* PH7_TKWRD_CASE */` |
|        - |  696 | `       /* PH7_TKWRD_SELF */` |
|        - |  697 | `       /* PH7_TKWRD_FINAL */` |
|        - |  698 | `       /* PH7_TKWRD_LIST */` |
|        - |  699 | `       /* PH7_TKWRD_STATIC */` |
|        - |  700 | `       /* PH7_TKWRD_CLONE */` |
|        - |  701 | `       /* PH7_TKWRD_SNE */` |
|        - |  702 | `       /* PH7_TKWRD_NEW */` |
|        - |  703 | `       /* PH7_TKWRD_CONST */` |
|        - |  704 | `       /* PH7_TKWRD_STRING */` |
|        - |  705 | `       /* PH7_TKWRD_GLOBAL */` |
|        - |  706 | `       /* PH7_TKWRD_USE */` |
|        - |  707 | `       /* PH7_TKWRD_ELIF */` |
|        - |  708 | `       /* PH7_TKWRD_ELSE */` |
|        - |  709 | `       /* PH7_TKWRD_IF */` |
|        - |  710 | `       /* PH7_TKWRD_FLOAT */` |
|        - |  711 | `       /* PH7_TKWRD_VAR */` |
|        - |  712 | `       /* PH7_TKWRD_ARRAY */` |
|        - |  713 | `       /* PH7_TKWRD_AND */` |
|        - |  714 | `       /* PH7_TKWRD_DIE */` |
|        - |  715 | `       /* PH7_TKWRD_ECHO */` |
|        - |  716 | `       /* PH7_TKWRD_USE */` |
|        - |  717 | `       /* PH7_TKWRD_ECHO */` |
|        - |  718 | `       /* PH7_TKWRD_ABSTRACT */` |
|        - |  719 | `       /* PH7_TKWRD_CLASS */` |
|        - |  720 | `       /* PH7_TKWRD_AS */` |
|        - |  721 | `       /* PH7_TKWRD_CONTINUE */` |
|        - |  722 | `       /* PH7_TKWRD_ENDIF */` |
|        - |  723 | `       /* PH7_TKWRD_FUNCTION */` |
|        - |  724 | `       /* PH7_TKWRD_DIE */` |
|        - |  725 | `       /* PH7_TKWRD_ENDWHILE */` |
|        - |  726 | `       /* PH7_TKWRD_WHILE */` |
|        - |  727 | `       /* PH7_TKWRD_EVAL */` |
|        - |  728 | `       /* PH7_TKWRD_DO */` |
|        - |  729 | `       /* PH7_TKWRD_EXIT */` |
|        - |  730 | `       /* PH7_TKWRD_GOTO */` |
|        - |  731 | `       /* PH7_TKWRD_IMPLEMENTS */` |
|        - |  732 | `       /* PH7_TKWRD_INCONCE */` |
|        - |  733 | `       /* PH7_TKWRD_INCLUDE */` |
|        - |  734 | `       /* PH7_TKWRD_EMPTY */` |
|        - |  735 | `       /* PH7_TKWRD_INSTANCEOF */` |
|        - |  736 | `       /* PH7_TKWRD_INTERFACE */` |
|        - |  737 | `       /* PH7_TKWRD_INT */` |
|        - |  738 | `       /* PH7_TKWRD_ENDFOR */` |
|        - |  739 | `       /* PH7_TKWRD_END4EACH */` |
|        - |  740 | `       /* PH7_TKWRD_FOR */` |
|        - |  741 | `       /* PH7_TKWRD_FOREACH */` |
|        - |  742 | `       /* PH7_TKWRD_OR */` |
|        - |  743 | `       /* PH7_TKWRD_ISSET */` |
|        - |  744 | `       /* PH7_TKWRD_PARENT */` |
|        - |  745 | `       /* PH7_TKWRD_PRIVATE */` |
|        - |  746 | `       /* PH7_TKWRD_PROTECTED */` |
|        - |  747 | `       /* PH7_TKWRD_PUBLIC */` |
|        - |  748 | `       /* PH7_TKWRD_CATCH */` |
|        - |  749 | `       /* PH7_TKWRD_UNSET */` |
|        - |  750 | `       /* PH7_TKWRD_XOR */` |
|        - |  751 | `       /* PH7_TKWRD_ARRAY */` |
|        - |  752 | `       /* PH7_TKWRD_AS */` |
|        - |  753 | `       /* PH7_TKWRD_ARRAY */` |
|        - |  754 | `       /* PH7_TKWRD_EXIT */` |
|        - |  755 | `       /* PH7_TKWRD_UNSET */` |
|        - |  756 | `       /* PH7_TKWRD_XOR */` |
|        - |  757 | `       /* PH7_TKWRD_OR */` |
|        - |  758 | `       /* PH7_TKWRD_BREAK */` |
|   900856 |  759 | `      return aCode[i];` |
|        - |  760 | `    }` |
|   678674 |  761 | `  }` |
|        - |  762 | `  /* Linear fallback for keywords not in the auto-generated hash table */` |
|  1652264 |  763 | `  if( n==5 && SyMemcmp(z,"trait",5)==0 ) return PH7_TKWRD_TRAIT;` |
|  1652212 |  764 | `  if( n==9 && SyMemcmp(z,"insteadof",9)==0 ) return PH7_TKWRD_INSTEADOF;` |
|  1652208 |  765 | `  if( n==7 && SyMemcmp(z,"finally",7)==0 ) return PH7_TKWRD_FINALLY;` |
|  1652182 |  766 | `  if( n==5 && SyMemcmp(z,"yield",5)==0 ) return PH7_TKWRD_YIELD;` |
|  1652150 |  767 | `  return PH7_TK_ID;` |
|  1324300 |  768 |  |
|        - |  769 | `/* --- End of Automatically generated code --- */` |
|        - |  770 | `/*` |
|        - |  771 | ` * Extract a heredoc/nowdoc text from a raw PHP input.` |
|        - |  772 | ` * According to the PHP language reference manual:` |
|        - |  773 | ` *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier` |
|        - |  774 | ` *  is provided, then a newline. The string itself follows, and then the same identifier again` |
|        - |  775 | ` *  to close the quotation.` |
|        - |  776 | ` *  The closing identifier must begin in the first column of the line. Also, the identifier must` |
|        - |  777 | ` *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric` |
|        - |  778 | ` *  characters and underscores, and must start with a non-digit character or underscore.` |
|        - |  779 | ` *  Heredoc text behaves just like a double-quoted string, without the double quotes.` |
|        - |  780 | ` *  This means that quotes in a heredoc do not need to be escaped, but the escape codes listed` |
|        - |  781 | ` *  above can still be used. Variables are expanded, but the same care must be taken when expressing` |
|        - |  782 | ` *  complex variables inside a heredoc as with strings.` |
|        - |  783 | ` *  Nowdocs are to single-quoted strings what heredocs are to double-quoted strings.` |
|        - |  784 | ` *  A nowdoc is specified similarly to a heredoc, but no parsing is done inside a nowdoc.` |
|        - |  785 | ` *  The construct is ideal for embedding PHP code or other large blocks of text without the need` |
|        - |  786 | ` *  for escaping. It shares some features in common with the SGML <![CDATA[ ]]> construct, in that` |
|        - |  787 | ` *  it declares a block of text which is not for parsing.` |
|        - |  788 | ` *  A nowdoc is identified with the same <<< sequence used for heredocs, but the identifier which follows` |
|        - |  789 | ` *  is enclosed in single quotes, e.g. <<<'EOT'. All the rules for heredoc identifiers also apply to nowdoc` |
|        - |  790 | ` *  identifiers, especially those regarding the appearance of the closing identifier.` |
|        - |  791 | ` * Symisc Extension:` |
|        - |  792 | ` * The closing delimiter can now start with a digit or undersocre or it can be an UTF-8 stream.` |
|        - |  793 | ` * Example:` |
|        - |  794 | ` *  <<<123` |
|        - |  795 | ` *    HEREDOC Here` |
|        - |  796 | ` * 123` |
|        - |  797 | ` *  or` |
|        - |  798 | ` *  <<<___` |
|        - |  799 | ` *   HEREDOC Here` |
|        - |  800 | ` *  ___` |
|        - |  801 | ` */` |
|       56 |  802 | `static sxi32 LexExtractHeredoc(SyStream *pStream,SyToken *pToken)` |
|        2 |  803 |  |
|       58 |  804 | `	const unsigned char *zIn  = pStream->zText;` |
|       58 |  805 | `	const unsigned char *zEnd = pStream->zEnd;` |
|        - |  806 | `	const unsigned char *zPtr;` |
|       58 |  807 | `	sxu8 bNowDoc = FALSE;` |
|        - |  808 | `	SyString sDelim;` |
|        - |  809 | `	SyString sStr;` |
|        - |  810 | `	/* Jump leading white spaces */` |
|       70 |  811 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|       13 |  812 | `		zIn++;` |
|        1 |  813 | `	}` |
|       58 |  814 | `	if( zIn >= zEnd ){` |
|        - |  815 | `		/* A simple symbol,return immediately */` |
|      ! 0 |  816 | `		return SXERR_CONTINUE;` |
|        - |  817 | `	}` |
|       58 |  818 | `	if( zIn[0] == '\'' \|\| zIn[0] == '"' ){` |
|        - |  819 | `		/* Make sure we are dealing with a nowdoc */` |
|       29 |  820 | `		bNowDoc =  zIn[0] == '\'' ? TRUE : FALSE;` |
|       29 |  821 | `		zIn++;` |
|       14 |  822 | `	}` |
|       58 |  823 | `	if( zIn[0] < 0xc0 && !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|        - |  824 | `		/* Invalid delimiter,return immediately */` |
|      ! 0 |  825 | `		return SXERR_CONTINUE;` |
|        - |  826 | `	}` |
|        - |  827 | `	/* Isolate the identifier */` |
|       58 |  828 | `	sDelim.zString = (const char *)zIn;` |
|       64 |  829 | `	for(;;){` |
|      130 |  830 | `		zPtr = zIn;` |
|        - |  831 | `		/* Skip alphanumeric stream */` |
|      424 |  832 | `		while( zPtr < zEnd && zPtr[0] < 0xc0 && (SyisAlphaNum(zPtr[0]) \|\| zPtr[0] == '_') ){` |
|      232 |  833 | `			zPtr++;` |
|        2 |  834 | `		}` |
|      130 |  835 | `		if( zPtr < zEnd && zPtr[0] >= 0xc0 ){` |
|       19 |  836 | `			zPtr++;` |
|        - |  837 | `			/* UTF-8 stream */` |
|       37 |  838 | `			while( zPtr < zEnd && ((zPtr[0] & 0xc0) == 0x80) ){` |
|       19 |  839 | `				zPtr++;` |
|        1 |  840 | `			}` |
|        9 |  841 | `		}` |
|      130 |  842 | `		if( zPtr == zIn ){` |
|        - |  843 | `			/* Not an UTF-8 or alphanumeric stream */` |
|       58 |  844 | `			break;` |
|        - |  845 | `		}` |
|        - |  846 | `		/* Synchronize pointers */` |
|       74 |  847 | `		zIn = zPtr;` |
|        2 |  848 | `	}` |
|        - |  849 | `	/* Get the identifier length */` |
|       58 |  850 | `	sDelim.nByte = (sxu32)((const char *)zIn-sDelim.zString);` |
|       58 |  851 | `	if( zIn[0] == '"' \|\| (bNowDoc && zIn[0] == '\'') ){` |
|        - |  852 | `		/* Jump the trailing single quote */` |
|       29 |  853 | `		zIn++;` |
|       14 |  854 | `	}` |
|        - |  855 | `	/* Jump trailing white spaces */` |
|       58 |  856 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|      ! 0 |  857 | `		zIn++;` |
|      ! 0 |  858 | `	}` |
|       58 |  859 | `	if( sDelim.nByte <= 0 \|\| zIn >= zEnd \|\| zIn[0] != '\n' ){` |
|        - |  860 | `		/* Invalid syntax */` |
|      ! 0 |  861 | `		return SXERR_CONTINUE;` |
|        - |  862 | `	}` |
|       58 |  863 | `	pStream->nLine++; /* Increment line counter */` |
|       58 |  864 | `	zIn++;` |
|        - |  865 | `	/* Isolate the delimited string */` |
|       58 |  866 | `	sStr.zString = (const char *)zIn;` |
|        - |  867 | `	/* Go and found the closing delimiter */` |
|       75 |  868 | `	for(;;){` |
|        - |  869 | `		/* Synchronize with the next line */` |
|     3018 |  870 | `		while( zIn < zEnd && zIn[0] != '\n' ){` |
|     2868 |  871 | `			zIn++;` |
|        2 |  872 | `		}` |
|      152 |  873 | `		if( zIn >= zEnd ){` |
|        - |  874 | `			/* End of the input reached, break immediately */` |
|       12 |  875 | `			pStream->zText = pStream->zEnd;` |
|       12 |  876 | `			break;` |
|        - |  877 | `		}` |
|      142 |  878 | `		pStream->nLine++; /* Increment line counter */` |
|      142 |  879 | `		zIn++;` |
|      142 |  880 | `		if( (sxu32)(zEnd - zIn) >= sDelim.nByte && SyMemcmp((const void *)sDelim.zString,(const void *)zIn,sDelim.nByte) == 0 ){` |
|       50 |  881 | `			zPtr = &zIn[sDelim.nByte];` |
|       62 |  882 | `			while( zPtr < zEnd && zPtr[0] < 0xc0 && SyisSpace(zPtr[0]) && zPtr[0] != '\n' ){` |
|       13 |  883 | `				zPtr++;` |
|        1 |  884 | `			}` |
|       50 |  885 | `			if( zPtr >= zEnd ){` |
|        - |  886 | `				/* End of input */` |
|      ! 0 |  887 | `				pStream->zText = zPtr;` |
|      ! 0 |  888 | `				break;` |
|        - |  889 | `			}` |
|       50 |  890 | `			if( zPtr[0] == ';' ){` |
|       50 |  891 | `				const unsigned char *zCur = zPtr;` |
|       50 |  892 | `				zPtr++;` |
|       52 |  893 | `				while( zPtr < zEnd && zPtr[0] < 0xc0 && SyisSpace(zPtr[0]) && zPtr[0] != '\n' ){` |
|        3 |  894 | `					zPtr++;` |
|        1 |  895 | `				}` |
|       50 |  896 | `				if( zPtr >= zEnd \|\| zPtr[0] == '\n' ){` |
|        - |  897 | `					/* Closing delimiter found,break immediately */` |
|       48 |  898 | `					pStream->zText = zCur; /* Keep the semi-colon */` |
|       48 |  899 | `					break;` |
|        1 |  900 | `				}` |
|        1 |  901 | `			}else if( zPtr[0] == '\n' ){` |
|        - |  902 | `				/* Closing delimiter found,break immediately */` |
|      ! 0 |  903 | `				pStream->zText = zPtr; /* Synchronize with the stream cursor */` |
|      ! 0 |  904 | `				break;` |
|        - |  905 | `			}` |
|        - |  906 | `			/* Synchronize pointers and continue searching */` |
|        3 |  907 | `			zIn = zPtr;` |
|        1 |  908 | `		}` |
|        2 |  909 | `	} /* For(;;) */` |
|        - |  910 | `	/* Get the delimited string length */` |
|       58 |  911 | `	sStr.nByte = (sxu32)((const char *)zIn-sStr.zString);` |
|        - |  912 | `	/* Record token type and length */` |
|       58 |  913 | `	pToken->nType = bNowDoc ? PH7_TK_NOWDOC : PH7_TK_HEREDOC;` |
|       58 |  914 | `	SyStringDupPtr(&pToken->sData,&sStr);` |
|        - |  915 | `	/* Remove trailing white spaces */` |
|      104 |  916 | `	SyStringRightTrim(&pToken->sData);` |
|        - |  917 | `	/* All done */` |
|       58 |  918 | `	return SXRET_OK;` |
|       30 |  919 |  |
|        - |  920 | `/*` |
|        - |  921 | ` * Tokenize a raw PHP input.` |
|        - |  922 | ` * This is the public tokenizer called by most code generator routines.` |
|        - |  923 | ` */` |
|    13120 |  924 | `PH7_PRIVATE sxi32 PH7_TokenizePHP(const char *zInput,sxu32 nLen,sxu32 nLineStart,SySet *pOut)` |
|        2 |  925 |  |
|        - |  926 | `	SyLex sLexer;` |
|        - |  927 | `	sxi32 rc;` |
|        - |  928 | `	/* Initialize the lexer */` |
|    13122 |  929 | `	rc = SyLexInit(&sLexer,&(*pOut),TokenizePHP,0);` |
|    13122 |  930 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  931 | `		return rc;` |
|        - |  932 | `	}` |
|    13122 |  933 | `	sLexer.sStream.nLine = nLineStart;` |
|        - |  934 | `	/* Tokenize input */` |
|    13122 |  935 | `	rc = SyLexTokenizeInput(&sLexer,zInput,nLen,0,0,0);` |
|        - |  936 | `	/* Release the lexer */` |
|    13122 |  937 | `	SyLexRelease(&sLexer);` |
|        - |  938 | `	/* Tokenization result */` |
|    13122 |  939 | `	return rc;` |
|     6562 |  940 |  |
|        - |  941 | `/*` |
|        - |  942 | ` * High level public tokenizer.` |
|        - |  943 | ` *  Tokenize the input into PHP tokens and raw tokens [i.e: HTML,XML,Raw text...].` |
|        - |  944 | ` * According to the PHP language reference manual` |
|        - |  945 | ` *   When PHP parses a file, it looks for opening and closing tags, which tell PHP` |
|        - |  946 | ` *   to start and stop interpreting the code between them. Parsing in this manner allows` |
|        - |  947 | ` *   PHP to be embedded in all sorts of different documents, as everything outside of a pair` |
|        - |  948 | ` *   of opening and closing tags is ignored by the PHP parser. Most of the time you will see` |
|        - |  949 | ` *   PHP embedded in HTML documents, as in this example.` |
|        - |  950 | ` *   <?php echo 'While this is going to be parsed.'; ?>` |
|        - |  951 | ` *   <p>This will also be ignored.</p>` |
|        - |  952 | ` *   You can also use more advanced structures:` |
|        - |  953 | ` *   Example #1 Advanced escaping` |
|        - |  954 | ` * <?php` |
|        - |  955 | ` * if ($expression) {` |
|        - |  956 | ` *   ?>` |
|        - |  957 | ` *   <strong>This is true.</strong>` |
|        - |  958 | ` *   <?php` |
|        - |  959 | ` * } else {` |
|        - |  960 | ` *   ?>` |
|        - |  961 | ` *   <strong>This is false.</strong>` |
|        - |  962 | ` *   <?php` |
|        - |  963 | ` * }` |
|        - |  964 | ` * ?>` |
|        - |  965 | ` * This works as expected, because when PHP hits the ?> closing tags, it simply starts outputting` |
|        - |  966 | ` * whatever it finds (except for an immediately following newline - see instruction separation ) until it hits` |
|        - |  967 | ` * another opening tag. The example given here is contrived, of course, but for outputting large blocks of text` |
|        - |  968 | ` * dropping out of PHP parsing mode is generally more efficient than sending all of the text through echo() or print().` |
|        - |  969 | ` * There are four different pairs of opening and closing tags which can be used in PHP. Three of those, <?php ?>` |
|        - |  970 | ` * <script language="php"> </script>  and <? ?> are always available. The other two are short tags and ASP style` |
|        - |  971 | ` * tags, and can be turned on and off from the php.ini configuration file. As such, while some people find short tags` |
|        - |  972 | ` * and ASP style tags convenient, they are less portable, and generally not recommended.` |
|        - |  973 | ` * Note:` |
|        - |  974 | ` * Also note that if you are embedding PHP within XML or XHTML you will need to use the <?php ?> tags to remain` |
|        - |  975 | ` * compliant with standards.` |
|        - |  976 | ` * Example #2 PHP Opening and Closing Tags` |
|        - |  977 | ` * 1.  <?php echo 'if you want to serve XHTML or XML documents, do it like this'; ?>` |
|        - |  978 | ` * 2.  <script language="php">` |
|        - |  979 | ` *       echo 'some editors (like FrontPage) don\'t` |
|        - |  980 | ` *             like processing instructions';` |
|        - |  981 | ` *   </script>` |
|        - |  982 | ` *` |
|        - |  983 | ` * 3.  <? echo 'this is the simplest, an SGML processing instruction'; ?>` |
|        - |  984 | ` *   <?= expression ?> This is a shortcut for "<? echo expression ?>"` |
|        - |  985 | ` */` |
|    10562 |  986 | `PH7_PRIVATE sxi32 PH7_TokenizeRawText(const char *zInput,sxu32 nLen,SySet *pOut)` |
|        2 |  987 |  |
|    10564 |  988 | `	const char *zEnd = &zInput[nLen];` |
|    10564 |  989 | `	const char *zIn  = zInput;` |
|        - |  990 | `	const char *zCur,*zCurEnd;` |
|    10564 |  991 | `	SyString sCtag = { 0, 0 };     /* Closing tag */` |
|        - |  992 | `	SyToken sToken;` |
|        - |  993 | `	SyString sDoc;` |
|        - |  994 | `	sxu32 nLine;` |
|        - |  995 | `	sxi32 iNest;` |
|        - |  996 | `	sxi32 rc;` |
|        - |  997 | `	/* Tokenize the input into PHP tokens and raw tokens */` |
|    10564 |  998 | `	nLine = 1;` |
|    10564 |  999 | `	zCur = zCurEnd   = 0; /* Prevent compiler warning */` |
|    10564 | 1000 | `	sToken.pUserData = 0;` |
|    10564 | 1001 | `	iNest = 0;` |
|    10564 | 1002 | `	sDoc.nByte = 0;` |
|    10564 | 1003 | `	sDoc.zString = ""; /* cc warning */` |
|    10564 | 1004 | `	for(;;){` |
|    21130 | 1005 | `		if( zIn >= zEnd ){` |
|        - | 1006 | `			/* End of input reached */` |
|    10560 | 1007 | `			break;` |
|        - | 1008 | `		}` |
|    10572 | 1009 | `		sToken.nLine = nLine;` |
|    10572 | 1010 | `		zCur = zIn;` |
|    10572 | 1011 | `		zCurEnd = 0;` |
|    10580 | 1012 | `		while( zIn < zEnd ){` |
|    10576 | 1013 | `			 if( zIn[0] == '<' ){` |
|    10568 | 1014 | `				const char *zTmp = zIn; /* End of raw input marker */` |
|    10568 | 1015 | `				zIn++;` |
|    10568 | 1016 | `				if( zIn < zEnd ){` |
|    10568 | 1017 | `					if( zIn[0] == '?' ){` |
|    10568 | 1018 | `						zIn++;` |
|    10568 | 1019 | `						if( (sxu32)(zEnd - zIn) >= sizeof("php")-1 &&  SyStrnicmp(zIn,"php",sizeof("php")-1) == 0 ){` |
|        - | 1020 | `							/* opening tag: <?php */` |
|    10566 | 1021 | `							zIn += sizeof("php")-1;` |
|     5282 | 1022 | `						}` |
|        - | 1023 | `						/* Look for the closing tag '?>' */` |
|    10568 | 1024 | `						SyStringInitFromBuf(&sCtag,"?>",sizeof("?>")-1);` |
|    10568 | 1025 | `						zCurEnd = zTmp;` |
|    10568 | 1026 | `						break;` |
|        - | 1027 | `					}` |
|      ! 0 | 1028 | `				}` |
|      ! 0 | 1029 | `			}else{` |
|       10 | 1030 | `				if( zIn[0] == '\n' ){` |
|       10 | 1031 | `					nLine++;` |
|        4 | 1032 | `				}` |
|       10 | 1033 | `				zIn++;` |
|        - | 1034 | `			 }` |
|        2 | 1035 | `		} /* While(zIn < zEnd) */` |
|    10572 | 1036 | `		if( zCurEnd == 0 ){` |
|        5 | 1037 | `			zCurEnd = zIn;` |
|        2 | 1038 | `		}` |
|        - | 1039 | `		/* Save the raw token */` |
|    10572 | 1040 | `		SyStringInitFromBuf(&sToken.sData,zCur,zCurEnd - zCur);` |
|    10572 | 1041 | `		sToken.nType = PH7_TOKEN_RAW;` |
|    10572 | 1042 | `		rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|    10572 | 1043 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1044 | `			return rc;` |
|        - | 1045 | `		}` |
|    10572 | 1046 | `		if( zIn >= zEnd ){` |
|        5 | 1047 | `			break;` |
|        - | 1048 | `		}` |
|        - | 1049 | `		/* Ignore leading white space */` |
|    22938 | 1050 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    12372 | 1051 | `			if( zIn[0] == '\n' ){` |
|    11178 | 1052 | `				nLine++;` |
|     5588 | 1053 | `			}` |
|    12372 | 1054 | `			zIn++;` |
|        2 | 1055 | `		}` |
|        - | 1056 | `		/* Delimit the PHP chunk */` |
|    10568 | 1057 | `		sToken.nLine = nLine;` |
|    10568 | 1058 | `		zCur = zIn;` |
|   968350 | 1059 | `		while( (sxu32)(zEnd - zIn) >= sCtag.nByte ){` |
|        - | 1060 | `			const char *zPtr;` |
|   963806 | 1061 | `			if( SyMemcmp(zIn,sCtag.zString,sCtag.nByte) == 0 && iNest < 1 ){` |
|     6024 | 1062 | `				break;` |
|        - | 1063 | `			}` |
|   480820 | 1064 | `			for(;;){` |
|   961642 | 1065 | `				if( zIn[0] != '/' \|\| (zIn[1] != '*' && zIn[1] != '/') /* && sCtag.nByte >= 2 */ ){` |
|   478893 | 1066 | `					break;` |
|        - | 1067 | `				}` |
|     3860 | 1068 | `				zIn += 2;` |
|     3860 | 1069 | `				if( zIn[-1] == '/' ){` |
|        - | 1070 | `					/* Inline comment */` |
|   133064 | 1071 | `					while( zIn < zEnd && zIn[0] != '\n' ){` |
|   129288 | 1072 | `						zIn++;` |
|        2 | 1073 | `					}` |
|     3778 | 1074 | `					if( zIn >= zEnd ){` |
|      ! 0 | 1075 | `						zIn--;` |
|      ! 0 | 1076 | `					}` |
|     1890 | 1077 | `				}else{` |
|        - | 1078 | `					/* Block comment */` |
|     4500 | 1079 | `					while( (sxu32)(zEnd-zIn) >= sizeof("*/") - 1 ){` |
|     4500 | 1080 | `						if( zIn[0] == '*' && zIn[1] == '/' ){` |
|       84 | 1081 | `							zIn += 2;` |
|       84 | 1082 | `							break;` |
|        - | 1083 | `						}` |
|     4418 | 1084 | `						if( zIn[0] == '\n' ){` |
|       28 | 1085 | `							nLine++;` |
|       13 | 1086 | `						}` |
|     4418 | 1087 | `						zIn++;` |
|        2 | 1088 | `					}` |
|        - | 1089 | `				}` |
|        2 | 1090 | `			}` |
|   957784 | 1091 | `			if( zIn[0] == '\n' ){` |
|    33480 | 1092 | `				nLine++;` |
|    33480 | 1093 | `				if( iNest > 0 ){` |
|      156 | 1094 | `					zIn++;` |
|      156 | 1095 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|      ! 0 | 1096 | `						zIn++;` |
|      ! 0 | 1097 | `					}` |
|      156 | 1098 | `					zPtr = zIn;` |
|      864 | 1099 | `					while( zIn < zEnd ){` |
|      864 | 1100 | `						if( (unsigned char)zIn[0] >= 0xc0 ){` |
|        - | 1101 | `							/* UTF-8 stream */` |
|       19 | 1102 | `							zIn++;` |
|       37 | 1103 | `							SX_JMP_UTF8(zIn,zEnd);` |
|      854 | 1104 | `						}else if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       79 | 1105 | `							break;` |
|      ! 0 | 1106 | `						}else{` |
|      692 | 1107 | `							zIn++;` |
|        - | 1108 | `						}` |
|        2 | 1109 | `					}` |
|      156 | 1110 | `					if( (sxu32)(zIn - zPtr) == sDoc.nByte && SyMemcmp(sDoc.zString,zPtr,sDoc.nByte) == 0 ){` |
|       60 | 1111 | `						iNest = 0;` |
|       29 | 1112 | `					}` |
|      156 | 1113 | `					continue;` |
|        2 | 1114 | `				}` |
|   940968 | 1115 | `			}else if ( (sxu32)(zEnd - zIn) >= sizeof("<<<") && zIn[0] == '<' && zIn[1] == '<' && zIn[2] == '<' && iNest < 1){` |
|       62 | 1116 | `				zIn += sizeof("<<<")-1;` |
|       74 | 1117 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|       13 | 1118 | `					zIn++;` |
|        1 | 1119 | `				}` |
|       62 | 1120 | `				if( zIn[0] == '"' \|\| zIn[0] == '\'' ){` |
|       32 | 1121 | `					zIn++;` |
|       15 | 1122 | `				}` |
|       62 | 1123 | `				zPtr = zIn;` |
|      330 | 1124 | `				while( zIn < zEnd ){` |
|      330 | 1125 | `					if( (unsigned char)zIn[0] >= 0xc0 ){` |
|        - | 1126 | `						/* UTF-8 stream */` |
|       19 | 1127 | `						zIn++;` |
|       37 | 1128 | `						SX_JMP_UTF8(zIn,zEnd);` |
|      320 | 1129 | `					}else if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       32 | 1130 | `						break;` |
|      ! 0 | 1131 | `					}else{` |
|      252 | 1132 | `						zIn++;` |
|        - | 1133 | `					}` |
|        2 | 1134 | `				}` |
|       62 | 1135 | `				SyStringInitFromBuf(&sDoc,zPtr,zIn-zPtr);` |
|       62 | 1136 | `				SyStringFullTrim(&sDoc);` |
|       62 | 1137 | `				if( sDoc.nByte > 0 ){` |
|       62 | 1138 | `					iNest++;` |
|       30 | 1139 | `				}` |
|       62 | 1140 | `				continue;` |
|        - | 1141 | `			}` |
|   957570 | 1142 | `			zIn++;` |
|        - | 1143 |  |
|   957570 | 1144 | `			if ( zIn >= zEnd )` |
|      ! 0 | 1145 | `				break;` |
|        2 | 1146 | `		}` |
|    10568 | 1147 | `		if( (sxu32)(zEnd - zIn) < sCtag.nByte ){` |
|     4546 | 1148 | `			zIn = zEnd;` |
|     2272 | 1149 | `		}` |
|    10568 | 1150 | `		if( zCur < zIn ){` |
|        - | 1151 | `			/* Save the PHP chunk for later processing */` |
|     8582 | 1152 | `			sToken.nType = PH7_TOKEN_PHP;` |
|     8582 | 1153 | `			SyStringInitFromBuf(&sToken.sData,zCur,zIn-zCur);` |
|    17138 | 1154 | `			SyStringRightTrim(&sToken.sData); /* Trim trailing white spaces */` |
|     8582 | 1155 | `			rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|     8582 | 1156 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1157 | `				return rc;` |
|        - | 1158 | `			}` |
|     4290 | 1159 | `		}` |
|    10568 | 1160 | `		if( zIn < zEnd ){` |
|        - | 1161 | `			/* Jump the trailing closing tag */` |
|     6024 | 1162 | `			zIn += sCtag.nByte;` |
|     3011 | 1163 | `		}` |
|        2 | 1164 | `	} /* For(;;) */` |
|        - | 1165 |  |
|    10564 | 1166 | ` 	return SXRET_OK;` |
|     5283 | 1167 |  |
|        - | 1168 |  |
