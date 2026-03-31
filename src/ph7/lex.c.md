# src/ph7/lex.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 631/666 lines (94.74%)

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
|  5732458 |   19 | `static sxi32 TokenizePHP(SyStream *pStream,SyToken *pToken,void *pUserData,void *pCtxData)` |
|        2 |   20 |  |
|        - |   21 | `	SyString *pStr;` |
|        - |   22 | `	sxi32 rc;` |
|        - |   23 | `	/* Ignore leading white spaces */` |
|  8639832 |   24 | `	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){` |
|        - |   25 | `		/* Advance the stream cursor */` |
|  2907374 |   26 | `		if( pStream->zText[0] == '\n' ){` |
|        - |   27 | `			/* Update line counter */` |
|    25788 |   28 | `			pStream->nLine++;` |
|    12893 |   29 | `		}` |
|  2907374 |   30 | `		pStream->zText++;` |
|        2 |   31 | `	}` |
|  5732460 |   32 | `	if( pStream->zText >= pStream->zEnd ){` |
|        - |   33 | `		/* End of input reached */` |
|      ! 0 |   34 | `		return SXERR_EOF;` |
|        - |   35 | `	}` |
|        - |   36 | `	/* Record token starting position and line */` |
|  5732460 |   37 | `	pToken->nLine = pStream->nLine;` |
|  5732460 |   38 | `	pToken->pUserData = 0;` |
|  5732460 |   39 | `	pStr = &pToken->sData;` |
|  5732460 |   40 | `	SyStringInitFromBuf(pStr,pStream->zText,0);` |
|  6733636 |   41 | `	if( pStream->zText[0] >= 0xc0 \|\| SyisAlpha(pStream->zText[0]) \|\| pStream->zText[0] == '_' ){` |
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
|  2002354 |   52 | `		if( pStream->zText[0] < 0xc0 ){` |
|  2002338 |   53 | `			pStream->zText++;` |
|  1001168 |   54 | `		}` |
|  1959798 |   55 | `		for(;;){` |
|  3919598 |   56 | `			zIn = pStream->zText;` |
|  3919598 |   57 | `			if( zIn[0] >= 0xc0 ){` |
|       49 |   58 | `				zIn++;` |
|        - |   59 | `				/* UTF-8 stream */` |
|      109 |   60 | `				while( zIn < pStream->zEnd && ((zIn[0] & 0xc0) == 0x80) ){` |
|       61 |   61 | `					zIn++;` |
|        1 |   62 | `				}` |
|       24 |   63 | `			}` |
|        - |   64 | `			/* Skip alphanumeric stream */` |
| 15572052 |   65 | `			while( zIn < pStream->zEnd && zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_') ){` |
|  9692658 |   66 | `				zIn++;` |
|        2 |   67 | `			}` |
|  3919598 |   68 | `			if( zIn == pStream->zText ){` |
|        - |   69 | `				/* Not an UTF-8 or alphanumeric stream */` |
|  2002354 |   70 | `				break;` |
|        - |   71 | `			}` |
|        - |   72 | `			/* Synchronize pointers */` |
|  1917246 |   73 | `			pStream->zText = zIn;` |
|        2 |   74 | `		}` |
|        - |   75 | `		/* Record token length */` |
|  2002354 |   76 | `		pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  2002354 |   77 | `		nKeyword = KeywordCode(pStr->zString,(int)pStr->nByte);` |
|  2002354 |   78 | `		if( nKeyword != PH7_TK_ID ){` |
|   640524 |   79 | `			if( nKeyword &` |
|        - |   80 | `				(PH7_TKWRD_NEW\|PH7_TKWRD_CLONE\|PH7_TKWRD_AND\|PH7_TKWRD_XOR\|PH7_TKWRD_OR\|PH7_TKWRD_INSTANCEOF\|PH7_TKWRD_SEQ\|PH7_TKWRD_SNE) ){` |
|        - |   81 | `					/* Alpha stream operators [i.e: new,clone,and,instanceof,eq,ne,or,xor],save the operator instance for later processing */` |
|    13294 |   82 | `					pToken->pUserData = (void *)PH7_ExprExtractOperator(pStr,0);` |
|        - |   83 | `					/* Mark as an operator */` |
|    13294 |   84 | `					pToken->nType = PH7_TK_ID\|PH7_TK_OP;` |
|     6648 |   85 | `			}else{` |
|        - |   86 | `				/* We are dealing with a keyword [i.e: while,foreach,class...],save the keyword ID */` |
|   627232 |   87 | `				pToken->nType = PH7_TK_KEYWORD;` |
|   627232 |   88 | `				pToken->pUserData = SX_INT_TO_PTR(nKeyword);` |
|        - |   89 | `			}` |
|   320263 |   90 | `		}else{` |
|        - |   91 | `			/* A simple identifier */` |
|  1361832 |   92 | `			pToken->nType = PH7_TK_ID;` |
|        - |   93 | `		}` |
|  1001178 |   94 | `	}else{` |
|        - |   95 | `		sxi32 c;` |
|        - |   96 | `		/* Non-alpha stream */` |
|  3762601 |   97 | `		if( pStream->zText[0] == '#' \|\|` |
|  3730106 |   98 | `			( pStream->zText[0] == '/' &&  &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '/') ){` |
|     3650 |   99 | `				pStream->zText++;` |
|        - |  100 | `				/* Inline comments */` |
|   130898 |  101 | `				while( pStream->zText < pStream->zEnd && pStream->zText[0] != '\n' ){` |
|   127250 |  102 | `					pStream->zText++;` |
|        2 |  103 | `				}` |
|        - |  104 | `				/* Tell the upper-layer to ignore this token */` |
|     3650 |  105 | `				return SXERR_CONTINUE;` |
|  3726460 |  106 | `		}else if( pStream->zText[0] == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '*' ){` |
|    61282 |  107 | `			pStream->zText += 2;` |
|        - |  108 | `			/* Block comment */` |
|  1738400 |  109 | `			while( pStream->zText < pStream->zEnd ){` |
|  1738400 |  110 | `				if( pStream->zText[0] == '*' ){` |
|    61308 |  111 | `					if( &pStream->zText[1] >= pStream->zEnd \|\| pStream->zText[1] == '/'  ){` |
|    30642 |  112 | `						break;` |
|        - |  113 | `					}` |
|       13 |  114 | `				}` |
|  1677120 |  115 | `				if( pStream->zText[0] == '\n' ){` |
|       28 |  116 | `					pStream->nLine++;` |
|       13 |  117 | `				}` |
|  1677120 |  118 | `				pStream->zText++;` |
|        2 |  119 | `			}` |
|    61282 |  120 | `			pStream->zText += 2;` |
|        - |  121 | `			/* Tell the upper-layer to ignore this token */` |
|    61282 |  122 | `			return SXERR_CONTINUE;` |
|  3665180 |  123 | `		}else if( SyisDigit(pStream->zText[0]) ){` |
|    83534 |  124 | `			pStream->zText++;` |
|        - |  125 | `			/* Decimal digit stream */` |
|    91072 |  126 | `			while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|     7540 |  127 | `				pStream->zText++;` |
|        2 |  128 | `			}` |
|        - |  129 | `			/* Mark the token as integer until we encounter a real number */` |
|    83534 |  130 | `			pToken->nType = PH7_TK_INTEGER;` |
|    83534 |  131 | `			if( pStream->zText < pStream->zEnd ){` |
|    83534 |  132 | `				c = pStream->zText[0];` |
|    83534 |  133 | `				if( c == '.' ){` |
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
|    83341 |  156 | `				}else if( c=='e' \|\| c=='E' ){` |
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
|    83141 |  171 | `				}else if( c == 'x' \|\| c == 'X' ){` |
|        - |  172 | `					/* Hex digit stream */` |
|       16 |  173 | `					pStream->zText++;` |
|       50 |  174 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisHex(pStream->zText[0]) ){` |
|       35 |  175 | `						pStream->zText++;` |
|        1 |  176 | `					}` |
|    83127 |  177 | `				}else if(c  == 'b' \|\| c == 'B' ){` |
|        - |  178 | `					/* Binary digit stream */` |
|       31 |  179 | `					pStream->zText++;` |
|      198 |  180 | `					while( pStream->zText < pStream->zEnd && (pStream->zText[0] == '0' \|\| pStream->zText[0] == '1') ){` |
|      153 |  181 | `						pStream->zText++;` |
|        1 |  182 | `					}` |
|       15 |  183 | `				}` |
|    41766 |  184 | `			}` |
|        - |  185 | `			/* Record token length */` |
|    83534 |  186 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|    83534 |  187 | `			return SXRET_OK;` |
|        - |  188 | `		}` |
|  3581648 |  189 | `		c = pStream->zText[0];` |
|  3581648 |  190 | `		pStream->zText++; /* Advance the stream cursor */` |
|        - |  191 | `		/* Assume we are dealing with an operator*/` |
|  3581648 |  192 | `		pToken->nType = PH7_TK_OP;` |
|  3581648 |  193 | `		switch(c){` |
|   772968 |  194 | `		case '$': pToken->nType = PH7_TK_DOLLAR; break;` |
|   262112 |  195 | `		case '{': pToken->nType = PH7_TK_OCB;    break;` |
|   262098 |  196 | `		case '}': pToken->nType = PH7_TK_CCB;    break;` |
|   543254 |  197 | `		case '(': pToken->nType = PH7_TK_LPAREN; break;` |
|    65324 |  198 | `		case '[': pToken->nType \|= PH7_TK_OSB;   break; /* Bitwise operation here,since the square bracket token '['` |
|        - |  199 | `														 * is a potential operator [i.e: subscripting] */` |
|    65330 |  200 | `		case ']': pToken->nType = PH7_TK_CSB;    break;` |
|   271620 |  201 | `		case ')': {` |
|   543242 |  202 | `			SySet *pTokSet = pStream->pSet;` |
|        - |  203 | `			/* Assemble type cast operators [i.e: (int),(float),(bool)...] */` |
|   543242 |  204 | `			if( pTokSet->nUsed >= 2 ){` |
|        - |  205 | `				SyToken *pTmp;` |
|        - |  206 | `				/* Peek the last recongnized token */` |
|   543240 |  207 | `				pTmp = (SyToken *)SySetPeek(pTokSet);` |
|   543240 |  208 | `				if( pTmp->nType & PH7_TK_KEYWORD ){` |
|    13036 |  209 | `					sxi32 nID = SX_PTR_TO_INT(pTmp->pUserData);` |
|    13036 |  210 | `					if( (sxu32)nID & (PH7_TKWRD_ARRAY\|PH7_TKWRD_INT\|PH7_TKWRD_FLOAT\|PH7_TKWRD_STRING\|PH7_TKWRD_OBJECT\|PH7_TKWRD_BOOL\|PH7_TKWRD_UNSET) ){` |
|    12968 |  211 | `						pTmp = (SyToken *)SySetAt(pTokSet,pTokSet->nUsed - 2);` |
|    12968 |  212 | `						if( pTmp->nType & PH7_TK_LPAREN ){` |
|        - |  213 | `							/* Merge the three tokens '(' 'TYPE' ')' into a single one */` |
|    12884 |  214 | `							const char * zTypeCast = "(int)";` |
|    12884 |  215 | `							if( nID & PH7_TKWRD_FLOAT ){` |
|     2558 |  216 | `								zTypeCast = "(float)";` |
|    11606 |  217 | `							}else if( nID & PH7_TKWRD_BOOL ){` |
|     2560 |  218 | `								zTypeCast = "(bool)";` |
|     9049 |  219 | `							}else if( nID & PH7_TKWRD_STRING ){` |
|     5110 |  220 | `								zTypeCast = "(string)";` |
|     5216 |  221 | `							}else if( nID & PH7_TKWRD_ARRAY ){` |
|       21 |  222 | `								zTypeCast = "(array)";` |
|     2652 |  223 | `							}else if( nID & PH7_TKWRD_OBJECT ){` |
|       17 |  224 | `								zTypeCast = "(object)";` |
|     2634 |  225 | `							}else if( nID & PH7_TKWRD_UNSET ){` |
|        7 |  226 | `								zTypeCast = "(unset)";` |
|        3 |  227 | `							}` |
|        - |  228 | `							/* Reflect the change */` |
|    12884 |  229 | `							pToken->nType = PH7_TK_OP;` |
|    12884 |  230 | `							SyStringInitFromBuf(&pToken->sData,zTypeCast,SyStrlen(zTypeCast));` |
|        - |  231 | `							/* Save the instance associated with the type cast operator */` |
|    12884 |  232 | `							pToken->pUserData = (void *)PH7_ExprExtractOperator(&pToken->sData,0);` |
|        - |  233 | `							/* Remove the two previous tokens */` |
|    12884 |  234 | `							pTokSet->nUsed -= 2;` |
|    12884 |  235 | `							return SXRET_OK;` |
|        - |  236 | `						}` |
|       42 |  237 | `					}` |
|       76 |  238 | `				}` |
|   265178 |  239 | `			}` |
|   530360 |  240 | `			pToken->nType = PH7_TK_RPAREN;` |
|   530360 |  241 | `			break;` |
|        - |  242 | `				  }` |
|    27298 |  243 | `		case '\'':{` |
|        - |  244 | `			/* Single quoted string */` |
|    54598 |  245 | `			pStr->zString++;` |
|   685368 |  246 | `			while( pStream->zText < pStream->zEnd ){` |
|   685368 |  247 | `				if( pStream->zText[0] == '\''  ){` |
|    54608 |  248 | `					if( pStream->zText[-1] != '\\' ){` |
|    54584 |  249 | `						break;` |
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
|   630772 |  262 | `				if( pStream->zText[0] == '\n' ){` |
|        7 |  263 | `					pStream->nLine++;` |
|        3 |  264 | `				}` |
|   630772 |  265 | `				pStream->zText++;` |
|        2 |  266 | `			}` |
|        - |  267 | `			/* Record token length and type */` |
|    54598 |  268 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|    54598 |  269 | `			pToken->nType = PH7_TK_SSTR;` |
|        - |  270 | `			/* Jump the trailing single quote */` |
|    54598 |  271 | `			pStream->zText++;` |
|    54598 |  272 | `			return SXRET_OK;` |
|        - |  273 | `				  }` |
|     6792 |  274 | `		case '"':{` |
|        - |  275 | `			sxi32 iNest;` |
|        - |  276 | `			/* Double quoted string */` |
|    13586 |  277 | `			pStr->zString++;` |
|   143322 |  278 | `			while( pStream->zText < pStream->zEnd ){` |
|   143322 |  279 | `				if( pStream->zText[0] == '{' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '$'){` |
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
|   143322 |  301 | `				if( pStream->zText[0] == '"' ){` |
|    13686 |  302 | `					if( pStream->zText[-1] != '\\' ){` |
|    13582 |  303 | `						break;` |
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
|   129738 |  316 | `				if( pStream->zText[0] == '\n' ){` |
|        7 |  317 | `					pStream->nLine++;` |
|        3 |  318 | `				}` |
|   129738 |  319 | `				pStream->zText++;` |
|        2 |  320 | `			}` |
|        - |  321 | `			/* Record token length and type */` |
|    13586 |  322 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|    13586 |  323 | `			pToken->nType = PH7_TK_DSTR;` |
|        - |  324 | `			/* Jump the trailing quote */` |
|    13586 |  325 | `			pStream->zText++;` |
|    13586 |  326 | `			return SXRET_OK;` |
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
|     1023 |  348 | `		case ':':` |
|     2048 |  349 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == ':' ){` |
|        - |  350 | `				/* Current operator: '::' */` |
|       92 |  351 | `				pStream->zText++;` |
|       47 |  352 | `			}else{` |
|     1958 |  353 | `				pToken->nType = PH7_TK_COLON; /* Single colon */` |
|        - |  354 | `			}` |
|     2048 |  355 | `			break;` |
|    55770 |  356 | `		case ',': pToken->nType \|= PH7_TK_COMMA;  break; /* Comma is also an operator */` |
|   404990 |  357 | `		case ';': pToken->nType = PH7_TK_SEMI;    break;` |
|        - |  358 | `			/* Handle combined operators [i.e: +=,===,!=== ...] */` |
|   129764 |  359 | `		case '=':` |
|   259530 |  360 | `			pToken->nType \|= PH7_TK_EQUAL;` |
|   259530 |  361 | `			if( pStream->zText < pStream->zEnd ){` |
|   259530 |  362 | `				if( pStream->zText[0] == '=' ){` |
|    16674 |  363 | `					pToken->nType &= ~PH7_TK_EQUAL;` |
|        - |  364 | `					/* Current operator: == */` |
|    16674 |  365 | `					pStream->zText++;` |
|    16674 |  366 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  367 | `						/* Current operator: === */` |
|     3636 |  368 | `						pStream->zText++;` |
|     1819 |  369 | `					}` |
|   251194 |  370 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  371 | `					/* Array operator: => */` |
|     3872 |  372 | `					pToken->nType = PH7_TK_ARRAY_OP;` |
|     3872 |  373 | `					pStream->zText++;` |
|     1937 |  374 | `				}else{` |
|        - |  375 | `					/* TICKET 1433-0010: Reference operator '=&' */` |
|   238988 |  376 | `					const unsigned char *zCur = pStream->zText;` |
|   238988 |  377 | `					sxu32 nLine = 0;` |
|   477952 |  378 | `					while( zCur < pStream->zEnd && zCur[0] < 0xc0 && SyisSpace(zCur[0]) ){` |
|   238966 |  379 | `						if( zCur[0] == '\n' ){` |
|        5 |  380 | `							nLine++;` |
|        2 |  381 | `						}` |
|   238966 |  382 | `						zCur++;` |
|        2 |  383 | `					}` |
|   238988 |  384 | `					if( zCur < pStream->zEnd && zCur[0] == '&' ){` |
|        - |  385 | `						/* Current operator: =& */` |
|       44 |  386 | `						pToken->nType &= ~PH7_TK_EQUAL;` |
|       44 |  387 | `						SyStringInitFromBuf(pStr,"=&",sizeof("=&")-1);` |
|        - |  388 | `						/* Update token stream */` |
|       44 |  389 | `						pStream->zText = &zCur[1];` |
|       44 |  390 | `						pStream->nLine += nLine;` |
|       21 |  391 | `					}` |
|        - |  392 | `				}` |
|   129764 |  393 | `			}` |
|   259530 |  394 | `			break;` |
|    18178 |  395 | `		case '!':` |
|    36358 |  396 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  397 | `				/* Current operator: != */` |
|    15466 |  398 | `				pStream->zText++;` |
|    15466 |  399 | `				if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  400 | `					/* Current operator: !== */` |
|    12884 |  401 | `					pStream->zText++;` |
|     6441 |  402 | `				}` |
|     7732 |  403 | `			}` |
|    36358 |  404 | `			break;` |
|    10437 |  405 | `		case '&':` |
|    20876 |  406 | `			pToken->nType \|= PH7_TK_AMPER;` |
|    20876 |  407 | `			if( pStream->zText < pStream->zEnd ){` |
|    20876 |  408 | `				if( pStream->zText[0] == '&' ){` |
|     8012 |  409 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|        - |  410 | `					/* Current operator: && */` |
|     8012 |  411 | `					pStream->zText++;` |
|    16871 |  412 | `				}else if( pStream->zText[0] == '=' ){` |
|        5 |  413 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|        - |  414 | `					/* Current operator: &= */` |
|        5 |  415 | `					pStream->zText++;` |
|        2 |  416 | `				}` |
|    10437 |  417 | `			}` |
|    20876 |  418 | `			break;` |
|     1349 |  419 | `		case '\|':` |
|     2700 |  420 | `			if( pStream->zText < pStream->zEnd ){` |
|     2700 |  421 | `				if( pStream->zText[0] == '\|' ){` |
|        - |  422 | `					/* Current operator: \|\| */` |
|     2684 |  423 | `					pStream->zText++;` |
|     1358 |  424 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  425 | `					/* Current operator: \|= */` |
|        5 |  426 | `					pStream->zText++;` |
|        2 |  427 | `				}` |
|     1349 |  428 | `			}` |
|     2700 |  429 | `			break;` |
|     6675 |  430 | `		case '+':` |
|    13352 |  431 | `			if( pStream->zText < pStream->zEnd ){` |
|    13350 |  432 | `				if( pStream->zText[0] == '+' ){` |
|        - |  433 | `					/* Current operator: ++ */` |
|    10420 |  434 | `					pStream->zText++;` |
|     8141 |  435 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  436 | `					/* Current operator: += */` |
|       30 |  437 | `					pStream->zText++;` |
|       14 |  438 | `				}` |
|     6674 |  439 | `			}` |
|    13352 |  440 | `			break;` |
|    48978 |  441 | `		case '-':` |
|    97958 |  442 | `			if( pStream->zText < pStream->zEnd ){` |
|    97958 |  443 | `				if( pStream->zText[0] == '-' ){` |
|        - |  444 | `					/* Current operator: -- */` |
|        5 |  445 | `					pStream->zText++;` |
|    97956 |  446 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  447 | `					/* Current operator: -= */` |
|        3 |  448 | `					pStream->zText++;` |
|    97953 |  449 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  450 | `					/* Current operator: -> */` |
|    97494 |  451 | `					pStream->zText++;` |
|    48746 |  452 | `				}` |
|    48978 |  453 | `			}` |
|    97958 |  454 | `			break;` |
|       71 |  455 | `		case '*':` |
|      144 |  456 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  457 | `				/* Current operator: *= */` |
|       11 |  458 | `				pStream->zText++;` |
|        5 |  459 | `			}` |
|      144 |  460 | `			break;` |
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
|    27115 |  479 | `		case '.':` |
|    54232 |  480 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  481 | `				/* Current operator: .= */` |
|     2596 |  482 | `				pStream->zText++;` |
|     1297 |  483 | `			}` |
|    54232 |  484 | `			break;` |
|    21866 |  485 | `		case '<':` |
|    43734 |  486 | `			if( pStream->zText < pStream->zEnd ){` |
|    43734 |  487 | `				if( pStream->zText[0] == '<' ){` |
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
|    43670 |  505 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  506 | `					/* Current operator: <> */` |
|        5 |  507 | `					pStream->zText++;` |
|    43660 |  508 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  509 | `					/* Current operator: <= */` |
|       30 |  510 | `					pStream->zText++;` |
|       14 |  511 | `				}` |
|    21838 |  512 | `			}` |
|    43678 |  513 | `			break;` |
|     2651 |  514 | `		case '>':` |
|     5304 |  515 | `			if( pStream->zText < pStream->zEnd ){` |
|     5304 |  516 | `				if( pStream->zText[0] == '>' ){` |
|        - |  517 | `					/* Current operator: >> */` |
|       17 |  518 | `					pStream->zText++;` |
|       17 |  519 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  520 | `						/* Current operator: >>= */` |
|        9 |  521 | `						pStream->zText++;` |
|        5 |  522 | `					}` |
|     5296 |  523 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  524 | `					/* Current operator: >= */` |
|       78 |  525 | `					pStream->zText++;` |
|       38 |  526 | `				}` |
|     2651 |  527 | `			}` |
|     5302 |  528 | `			break;` |
|      982 |  529 | `		default:` |
|     1964 |  530 | `			break;` |
|        - |  531 | `		}` |
|  3500526 |  532 | `		if( pStr->nByte <= 0 ){` |
|        - |  533 | `			/* Record token length */` |
|  3500484 |  534 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  1750241 |  535 | `		}` |
|  3500526 |  536 | `		if( pToken->nType & PH7_TK_OP ){` |
|        - |  537 | `			const ph7_expr_op *pOp;` |
|        - |  538 | `			/* Check if the extracted token is an operator */` |
|   653492 |  539 | `			pOp = PH7_ExprExtractOperator(pStr,(SyToken *)SySetPeek(pStream->pSet));` |
|   653492 |  540 | `			if( pOp == 0 ){` |
|        - |  541 | `				/* Not an operator */` |
|      ! 0 |  542 | `				pToken->nType &= ~PH7_TK_OP;` |
|      ! 0 |  543 | `				if( pToken->nType <= 0 ){` |
|      ! 0 |  544 | `					pToken->nType = PH7_TK_OTHER;` |
|      ! 0 |  545 | `				}` |
|      ! 0 |  546 | `			}else{` |
|        - |  547 | `				/* Save the instance associated with this operator for later processing */` |
|   653492 |  548 | `				pToken->pUserData = (void *)pOp;` |
|        - |  549 | `			}` |
|   326745 |  550 | `		}` |
|        - |  551 | `	}` |
|        - |  552 | `	/* Tell the upper-layer to save the extracted token for later processing */` |
|  5502878 |  553 | `	return SXRET_OK;` |
|  2866231 |  554 |  |
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
|  2002354 |  571 | `static sxu32 KeywordCode(const char *z, int n){` |
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
|  2002354 |  661 | `  if( n<2 ) return PH7_TK_ID;` |
|  1917224 |  662 | `  h = (((int)z[0]*4) ^ ((int)z[n-1]*3) ^ n) % 151;` |
|  2925436 |  663 | `  for(i=((int)aHash[h])-1; i>=0; i=((int)aNext[i])-1){` |
|  1648654 |  664 | `    if( (int)aLen[i]==n && SyMemcmp(&zText[aOffset[i]],z,n)==0 ){` |
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
|   640442 |  749 | `      return aCode[i];` |
|        - |  750 | `    }` |
|   504106 |  751 | `  }` |
|        - |  752 | `  /* Linear fallback for keywords not in the auto-generated hash table */` |
|  1276784 |  753 | `  if( n==5 && SyMemcmp(z,"trait",5)==0 ) return PH7_TKWRD_TRAIT;` |
|  1276732 |  754 | `  if( n==9 && SyMemcmp(z,"insteadof",9)==0 ) return PH7_TKWRD_INSTEADOF;` |
|  1276728 |  755 | `  if( n==7 && SyMemcmp(z,"finally",7)==0 ) return PH7_TKWRD_FINALLY;` |
|  1276702 |  756 | `  return PH7_TK_ID;` |
|  1001178 |  757 |  |
|        - |  758 | `/* --- End of Automatically generated code --- */` |
|        - |  759 | `/*` |
|        - |  760 | ` * Extract a heredoc/nowdoc text from a raw PHP input.` |
|        - |  761 | ` * According to the PHP language reference manual:` |
|        - |  762 | ` *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier` |
|        - |  763 | ` *  is provided, then a newline. The string itself follows, and then the same identifier again` |
|        - |  764 | ` *  to close the quotation.` |
|        - |  765 | ` *  The closing identifier must begin in the first column of the line. Also, the identifier must` |
|        - |  766 | ` *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric` |
|        - |  767 | ` *  characters and underscores, and must start with a non-digit character or underscore.` |
|        - |  768 | ` *  Heredoc text behaves just like a double-quoted string, without the double quotes.` |
|        - |  769 | ` *  This means that quotes in a heredoc do not need to be escaped, but the escape codes listed` |
|        - |  770 | ` *  above can still be used. Variables are expanded, but the same care must be taken when expressing` |
|        - |  771 | ` *  complex variables inside a heredoc as with strings.` |
|        - |  772 | ` *  Nowdocs are to single-quoted strings what heredocs are to double-quoted strings.` |
|        - |  773 | ` *  A nowdoc is specified similarly to a heredoc, but no parsing is done inside a nowdoc.` |
|        - |  774 | ` *  The construct is ideal for embedding PHP code or other large blocks of text without the need` |
|        - |  775 | ` *  for escaping. It shares some features in common with the SGML <![CDATA[ ]]> construct, in that` |
|        - |  776 | ` *  it declares a block of text which is not for parsing.` |
|        - |  777 | ` *  A nowdoc is identified with the same <<< sequence used for heredocs, but the identifier which follows` |
|        - |  778 | ` *  is enclosed in single quotes, e.g. <<<'EOT'. All the rules for heredoc identifiers also apply to nowdoc` |
|        - |  779 | ` *  identifiers, especially those regarding the appearance of the closing identifier.` |
|        - |  780 | ` * Symisc Extension:` |
|        - |  781 | ` * The closing delimiter can now start with a digit or undersocre or it can be an UTF-8 stream.` |
|        - |  782 | ` * Example:` |
|        - |  783 | ` *  <<<123` |
|        - |  784 | ` *    HEREDOC Here` |
|        - |  785 | ` * 123` |
|        - |  786 | ` *  or` |
|        - |  787 | ` *  <<<___` |
|        - |  788 | ` *   HEREDOC Here` |
|        - |  789 | ` *  ___` |
|        - |  790 | ` */` |
|       56 |  791 | `static sxi32 LexExtractHeredoc(SyStream *pStream,SyToken *pToken)` |
|        2 |  792 |  |
|       58 |  793 | `	const unsigned char *zIn  = pStream->zText;` |
|       58 |  794 | `	const unsigned char *zEnd = pStream->zEnd;` |
|        - |  795 | `	const unsigned char *zPtr;` |
|       58 |  796 | `	sxu8 bNowDoc = FALSE;` |
|        - |  797 | `	SyString sDelim;` |
|        - |  798 | `	SyString sStr;` |
|        - |  799 | `	/* Jump leading white spaces */` |
|       70 |  800 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|       13 |  801 | `		zIn++;` |
|        1 |  802 | `	}` |
|       58 |  803 | `	if( zIn >= zEnd ){` |
|        - |  804 | `		/* A simple symbol,return immediately */` |
|      ! 0 |  805 | `		return SXERR_CONTINUE;` |
|        - |  806 | `	}` |
|       58 |  807 | `	if( zIn[0] == '\'' \|\| zIn[0] == '"' ){` |
|        - |  808 | `		/* Make sure we are dealing with a nowdoc */` |
|       29 |  809 | `		bNowDoc =  zIn[0] == '\'' ? TRUE : FALSE;` |
|       29 |  810 | `		zIn++;` |
|       14 |  811 | `	}` |
|       58 |  812 | `	if( zIn[0] < 0xc0 && !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|        - |  813 | `		/* Invalid delimiter,return immediately */` |
|      ! 0 |  814 | `		return SXERR_CONTINUE;` |
|        - |  815 | `	}` |
|        - |  816 | `	/* Isolate the identifier */` |
|       58 |  817 | `	sDelim.zString = (const char *)zIn;` |
|       64 |  818 | `	for(;;){` |
|      130 |  819 | `		zPtr = zIn;` |
|        - |  820 | `		/* Skip alphanumeric stream */` |
|      424 |  821 | `		while( zPtr < zEnd && zPtr[0] < 0xc0 && (SyisAlphaNum(zPtr[0]) \|\| zPtr[0] == '_') ){` |
|      232 |  822 | `			zPtr++;` |
|        2 |  823 | `		}` |
|      130 |  824 | `		if( zPtr < zEnd && zPtr[0] >= 0xc0 ){` |
|       19 |  825 | `			zPtr++;` |
|        - |  826 | `			/* UTF-8 stream */` |
|       37 |  827 | `			while( zPtr < zEnd && ((zPtr[0] & 0xc0) == 0x80) ){` |
|       19 |  828 | `				zPtr++;` |
|        1 |  829 | `			}` |
|        9 |  830 | `		}` |
|      130 |  831 | `		if( zPtr == zIn ){` |
|        - |  832 | `			/* Not an UTF-8 or alphanumeric stream */` |
|       58 |  833 | `			break;` |
|        - |  834 | `		}` |
|        - |  835 | `		/* Synchronize pointers */` |
|       74 |  836 | `		zIn = zPtr;` |
|        2 |  837 | `	}` |
|        - |  838 | `	/* Get the identifier length */` |
|       58 |  839 | `	sDelim.nByte = (sxu32)((const char *)zIn-sDelim.zString);` |
|       58 |  840 | `	if( zIn[0] == '"' \|\| (bNowDoc && zIn[0] == '\'') ){` |
|        - |  841 | `		/* Jump the trailing single quote */` |
|       29 |  842 | `		zIn++;` |
|       14 |  843 | `	}` |
|        - |  844 | `	/* Jump trailing white spaces */` |
|       58 |  845 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|      ! 0 |  846 | `		zIn++;` |
|      ! 0 |  847 | `	}` |
|       58 |  848 | `	if( sDelim.nByte <= 0 \|\| zIn >= zEnd \|\| zIn[0] != '\n' ){` |
|        - |  849 | `		/* Invalid syntax */` |
|      ! 0 |  850 | `		return SXERR_CONTINUE;` |
|        - |  851 | `	}` |
|       58 |  852 | `	pStream->nLine++; /* Increment line counter */` |
|       58 |  853 | `	zIn++;` |
|        - |  854 | `	/* Isolate the delimited string */` |
|       58 |  855 | `	sStr.zString = (const char *)zIn;` |
|        - |  856 | `	/* Go and found the closing delimiter */` |
|       75 |  857 | `	for(;;){` |
|        - |  858 | `		/* Synchronize with the next line */` |
|     3018 |  859 | `		while( zIn < zEnd && zIn[0] != '\n' ){` |
|     2868 |  860 | `			zIn++;` |
|        2 |  861 | `		}` |
|      152 |  862 | `		if( zIn >= zEnd ){` |
|        - |  863 | `			/* End of the input reached, break immediately */` |
|       12 |  864 | `			pStream->zText = pStream->zEnd;` |
|       12 |  865 | `			break;` |
|        - |  866 | `		}` |
|      142 |  867 | `		pStream->nLine++; /* Increment line counter */` |
|      142 |  868 | `		zIn++;` |
|      142 |  869 | `		if( (sxu32)(zEnd - zIn) >= sDelim.nByte && SyMemcmp((const void *)sDelim.zString,(const void *)zIn,sDelim.nByte) == 0 ){` |
|       50 |  870 | `			zPtr = &zIn[sDelim.nByte];` |
|       62 |  871 | `			while( zPtr < zEnd && zPtr[0] < 0xc0 && SyisSpace(zPtr[0]) && zPtr[0] != '\n' ){` |
|       13 |  872 | `				zPtr++;` |
|        1 |  873 | `			}` |
|       50 |  874 | `			if( zPtr >= zEnd ){` |
|        - |  875 | `				/* End of input */` |
|      ! 0 |  876 | `				pStream->zText = zPtr;` |
|      ! 0 |  877 | `				break;` |
|        - |  878 | `			}` |
|       50 |  879 | `			if( zPtr[0] == ';' ){` |
|       50 |  880 | `				const unsigned char *zCur = zPtr;` |
|       50 |  881 | `				zPtr++;` |
|       52 |  882 | `				while( zPtr < zEnd && zPtr[0] < 0xc0 && SyisSpace(zPtr[0]) && zPtr[0] != '\n' ){` |
|        3 |  883 | `					zPtr++;` |
|        1 |  884 | `				}` |
|       50 |  885 | `				if( zPtr >= zEnd \|\| zPtr[0] == '\n' ){` |
|        - |  886 | `					/* Closing delimiter found,break immediately */` |
|       48 |  887 | `					pStream->zText = zCur; /* Keep the semi-colon */` |
|       48 |  888 | `					break;` |
|        1 |  889 | `				}` |
|        1 |  890 | `			}else if( zPtr[0] == '\n' ){` |
|        - |  891 | `				/* Closing delimiter found,break immediately */` |
|      ! 0 |  892 | `				pStream->zText = zPtr; /* Synchronize with the stream cursor */` |
|      ! 0 |  893 | `				break;` |
|        - |  894 | `			}` |
|        - |  895 | `			/* Synchronize pointers and continue searching */` |
|        3 |  896 | `			zIn = zPtr;` |
|        1 |  897 | `		}` |
|        2 |  898 | `	} /* For(;;) */` |
|        - |  899 | `	/* Get the delimited string length */` |
|       58 |  900 | `	sStr.nByte = (sxu32)((const char *)zIn-sStr.zString);` |
|        - |  901 | `	/* Record token type and length */` |
|       58 |  902 | `	pToken->nType = bNowDoc ? PH7_TK_NOWDOC : PH7_TK_HEREDOC;` |
|       58 |  903 | `	SyStringDupPtr(&pToken->sData,&sStr);` |
|        - |  904 | `	/* Remove trailing white spaces */` |
|      104 |  905 | `	SyStringRightTrim(&pToken->sData);` |
|        - |  906 | `	/* All done */` |
|       58 |  907 | `	return SXRET_OK;` |
|       30 |  908 |  |
|        - |  909 | `/*` |
|        - |  910 | ` * Tokenize a raw PHP input.` |
|        - |  911 | ` * This is the public tokenizer called by most code generator routines.` |
|        - |  912 | ` */` |
|    12348 |  913 | `PH7_PRIVATE sxi32 PH7_TokenizePHP(const char *zInput,sxu32 nLen,sxu32 nLineStart,SySet *pOut)` |
|        2 |  914 |  |
|        - |  915 | `	SyLex sLexer;` |
|        - |  916 | `	sxi32 rc;` |
|        - |  917 | `	/* Initialize the lexer */` |
|    12350 |  918 | `	rc = SyLexInit(&sLexer,&(*pOut),TokenizePHP,0);` |
|    12350 |  919 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  920 | `		return rc;` |
|        - |  921 | `	}` |
|    12350 |  922 | `	sLexer.sStream.nLine = nLineStart;` |
|        - |  923 | `	/* Tokenize input */` |
|    12350 |  924 | `	rc = SyLexTokenizeInput(&sLexer,zInput,nLen,0,0,0);` |
|        - |  925 | `	/* Release the lexer */` |
|    12350 |  926 | `	SyLexRelease(&sLexer);` |
|        - |  927 | `	/* Tokenization result */` |
|    12350 |  928 | `	return rc;` |
|     6176 |  929 |  |
|        - |  930 | `/*` |
|        - |  931 | ` * High level public tokenizer.` |
|        - |  932 | ` *  Tokenize the input into PHP tokens and raw tokens [i.e: HTML,XML,Raw text...].` |
|        - |  933 | ` * According to the PHP language reference manual` |
|        - |  934 | ` *   When PHP parses a file, it looks for opening and closing tags, which tell PHP` |
|        - |  935 | ` *   to start and stop interpreting the code between them. Parsing in this manner allows` |
|        - |  936 | ` *   PHP to be embedded in all sorts of different documents, as everything outside of a pair` |
|        - |  937 | ` *   of opening and closing tags is ignored by the PHP parser. Most of the time you will see` |
|        - |  938 | ` *   PHP embedded in HTML documents, as in this example.` |
|        - |  939 | ` *   <?php echo 'While this is going to be parsed.'; ?>` |
|        - |  940 | ` *   <p>This will also be ignored.</p>` |
|        - |  941 | ` *   You can also use more advanced structures:` |
|        - |  942 | ` *   Example #1 Advanced escaping` |
|        - |  943 | ` * <?php` |
|        - |  944 | ` * if ($expression) {` |
|        - |  945 | ` *   ?>` |
|        - |  946 | ` *   <strong>This is true.</strong>` |
|        - |  947 | ` *   <?php` |
|        - |  948 | ` * } else {` |
|        - |  949 | ` *   ?>` |
|        - |  950 | ` *   <strong>This is false.</strong>` |
|        - |  951 | ` *   <?php` |
|        - |  952 | ` * }` |
|        - |  953 | ` * ?>` |
|        - |  954 | ` * This works as expected, because when PHP hits the ?> closing tags, it simply starts outputting` |
|        - |  955 | ` * whatever it finds (except for an immediately following newline - see instruction separation ) until it hits` |
|        - |  956 | ` * another opening tag. The example given here is contrived, of course, but for outputting large blocks of text` |
|        - |  957 | ` * dropping out of PHP parsing mode is generally more efficient than sending all of the text through echo() or print().` |
|        - |  958 | ` * There are four different pairs of opening and closing tags which can be used in PHP. Three of those, <?php ?>` |
|        - |  959 | ` * <script language="php"> </script>  and <? ?> are always available. The other two are short tags and ASP style` |
|        - |  960 | ` * tags, and can be turned on and off from the php.ini configuration file. As such, while some people find short tags` |
|        - |  961 | ` * and ASP style tags convenient, they are less portable, and generally not recommended.` |
|        - |  962 | ` * Note:` |
|        - |  963 | ` * Also note that if you are embedding PHP within XML or XHTML you will need to use the <?php ?> tags to remain` |
|        - |  964 | ` * compliant with standards.` |
|        - |  965 | ` * Example #2 PHP Opening and Closing Tags` |
|        - |  966 | ` * 1.  <?php echo 'if you want to serve XHTML or XML documents, do it like this'; ?>` |
|        - |  967 | ` * 2.  <script language="php">` |
|        - |  968 | ` *       echo 'some editors (like FrontPage) don\'t` |
|        - |  969 | ` *             like processing instructions';` |
|        - |  970 | ` *   </script>` |
|        - |  971 | ` *` |
|        - |  972 | ` * 3.  <? echo 'this is the simplest, an SGML processing instruction'; ?>` |
|        - |  973 | ` *   <?= expression ?> This is a shortcut for "<? echo expression ?>"` |
|        - |  974 | ` */` |
|    10076 |  975 | `PH7_PRIVATE sxi32 PH7_TokenizeRawText(const char *zInput,sxu32 nLen,SySet *pOut)` |
|        2 |  976 |  |
|    10078 |  977 | `	const char *zEnd = &zInput[nLen];` |
|    10078 |  978 | `	const char *zIn  = zInput;` |
|        - |  979 | `	const char *zCur,*zCurEnd;` |
|    10078 |  980 | `	SyString sCtag = { 0, 0 };     /* Closing tag */` |
|        - |  981 | `	SyToken sToken;` |
|        - |  982 | `	SyString sDoc;` |
|        - |  983 | `	sxu32 nLine;` |
|        - |  984 | `	sxi32 iNest;` |
|        - |  985 | `	sxi32 rc;` |
|        - |  986 | `	/* Tokenize the input into PHP tokens and raw tokens */` |
|    10078 |  987 | `	nLine = 1;` |
|    10078 |  988 | `	zCur = zCurEnd   = 0; /* Prevent compiler warning */` |
|    10078 |  989 | `	sToken.pUserData = 0;` |
|    10078 |  990 | `	iNest = 0;` |
|    10078 |  991 | `	sDoc.nByte = 0;` |
|    10078 |  992 | `	sDoc.zString = ""; /* cc warning */` |
|    10078 |  993 | `	for(;;){` |
|    20158 |  994 | `		if( zIn >= zEnd ){` |
|        - |  995 | `			/* End of input reached */` |
|    10074 |  996 | `			break;` |
|        - |  997 | `		}` |
|    10086 |  998 | `		sToken.nLine = nLine;` |
|    10086 |  999 | `		zCur = zIn;` |
|    10086 | 1000 | `		zCurEnd = 0;` |
|    10094 | 1001 | `		while( zIn < zEnd ){` |
|    10090 | 1002 | `			 if( zIn[0] == '<' ){` |
|    10082 | 1003 | `				const char *zTmp = zIn; /* End of raw input marker */` |
|    10082 | 1004 | `				zIn++;` |
|    10082 | 1005 | `				if( zIn < zEnd ){` |
|    10082 | 1006 | `					if( zIn[0] == '?' ){` |
|    10082 | 1007 | `						zIn++;` |
|    10082 | 1008 | `						if( (sxu32)(zEnd - zIn) >= sizeof("php")-1 &&  SyStrnicmp(zIn,"php",sizeof("php")-1) == 0 ){` |
|        - | 1009 | `							/* opening tag: <?php */` |
|    10080 | 1010 | `							zIn += sizeof("php")-1;` |
|     5039 | 1011 | `						}` |
|        - | 1012 | `						/* Look for the closing tag '?>' */` |
|    10082 | 1013 | `						SyStringInitFromBuf(&sCtag,"?>",sizeof("?>")-1);` |
|    10082 | 1014 | `						zCurEnd = zTmp;` |
|    10082 | 1015 | `						break;` |
|        - | 1016 | `					}` |
|      ! 0 | 1017 | `				}` |
|      ! 0 | 1018 | `			}else{` |
|       10 | 1019 | `				if( zIn[0] == '\n' ){` |
|       10 | 1020 | `					nLine++;` |
|        4 | 1021 | `				}` |
|       10 | 1022 | `				zIn++;` |
|        - | 1023 | `			 }` |
|        2 | 1024 | `		} /* While(zIn < zEnd) */` |
|    10086 | 1025 | `		if( zCurEnd == 0 ){` |
|        5 | 1026 | `			zCurEnd = zIn;` |
|        2 | 1027 | `		}` |
|        - | 1028 | `		/* Save the raw token */` |
|    10086 | 1029 | `		SyStringInitFromBuf(&sToken.sData,zCur,zCurEnd - zCur);` |
|    10086 | 1030 | `		sToken.nType = PH7_TOKEN_RAW;` |
|    10086 | 1031 | `		rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|    10086 | 1032 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1033 | `			return rc;` |
|        - | 1034 | `		}` |
|    10086 | 1035 | `		if( zIn >= zEnd ){` |
|        5 | 1036 | `			break;` |
|        - | 1037 | `		}` |
|        - | 1038 | `		/* Ignore leading white space */` |
|    21894 | 1039 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    11814 | 1040 | `			if( zIn[0] == '\n' ){` |
|    10662 | 1041 | `				nLine++;` |
|     5330 | 1042 | `			}` |
|    11814 | 1043 | `			zIn++;` |
|        2 | 1044 | `		}` |
|        - | 1045 | `		/* Delimit the PHP chunk */` |
|    10082 | 1046 | `		sToken.nLine = nLine;` |
|    10082 | 1047 | `		zCur = zIn;` |
|   894056 | 1048 | `		while( (sxu32)(zEnd - zIn) >= sCtag.nByte ){` |
|        - | 1049 | `			const char *zPtr;` |
|   889734 | 1050 | `			if( SyMemcmp(zIn,sCtag.zString,sCtag.nByte) == 0 && iNest < 1 ){` |
|     5760 | 1051 | `				break;` |
|        - | 1052 | `			}` |
|   443873 | 1053 | `			for(;;){` |
|   887748 | 1054 | `				if( zIn[0] != '/' \|\| (zIn[1] != '*' && zIn[1] != '/') /* && sCtag.nByte >= 2 */ ){` |
|   441989 | 1055 | `					break;` |
|        - | 1056 | `				}` |
|     3774 | 1057 | `				zIn += 2;` |
|     3774 | 1058 | `				if( zIn[-1] == '/' ){` |
|        - | 1059 | `					/* Inline comment */` |
|   128886 | 1060 | `					while( zIn < zEnd && zIn[0] != '\n' ){` |
|   125194 | 1061 | `						zIn++;` |
|        2 | 1062 | `					}` |
|     3694 | 1063 | `					if( zIn >= zEnd ){` |
|      ! 0 | 1064 | `						zIn--;` |
|      ! 0 | 1065 | `					}` |
|     1848 | 1066 | `				}else{` |
|        - | 1067 | `					/* Block comment */` |
|     4400 | 1068 | `					while( (sxu32)(zEnd-zIn) >= sizeof("*/") - 1 ){` |
|     4400 | 1069 | `						if( zIn[0] == '*' && zIn[1] == '/' ){` |
|       82 | 1070 | `							zIn += 2;` |
|       82 | 1071 | `							break;` |
|        - | 1072 | `						}` |
|     4320 | 1073 | `						if( zIn[0] == '\n' ){` |
|       28 | 1074 | `							nLine++;` |
|       13 | 1075 | `						}` |
|     4320 | 1076 | `						zIn++;` |
|        2 | 1077 | `					}` |
|        - | 1078 | `				}` |
|        2 | 1079 | `			}` |
|   883976 | 1080 | `			if( zIn[0] == '\n' ){` |
|    30688 | 1081 | `				nLine++;` |
|    30688 | 1082 | `				if( iNest > 0 ){` |
|      156 | 1083 | `					zIn++;` |
|      156 | 1084 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|      ! 0 | 1085 | `						zIn++;` |
|      ! 0 | 1086 | `					}` |
|      156 | 1087 | `					zPtr = zIn;` |
|      864 | 1088 | `					while( zIn < zEnd ){` |
|      864 | 1089 | `						if( (unsigned char)zIn[0] >= 0xc0 ){` |
|        - | 1090 | `							/* UTF-8 stream */` |
|       19 | 1091 | `							zIn++;` |
|       37 | 1092 | `							SX_JMP_UTF8(zIn,zEnd);` |
|      854 | 1093 | `						}else if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       79 | 1094 | `							break;` |
|      ! 0 | 1095 | `						}else{` |
|      692 | 1096 | `							zIn++;` |
|        - | 1097 | `						}` |
|        2 | 1098 | `					}` |
|      156 | 1099 | `					if( (sxu32)(zIn - zPtr) == sDoc.nByte && SyMemcmp(sDoc.zString,zPtr,sDoc.nByte) == 0 ){` |
|       60 | 1100 | `						iNest = 0;` |
|       29 | 1101 | `					}` |
|      156 | 1102 | `					continue;` |
|        2 | 1103 | `				}` |
|   868556 | 1104 | `			}else if ( (sxu32)(zEnd - zIn) >= sizeof("<<<") && zIn[0] == '<' && zIn[1] == '<' && zIn[2] == '<' && iNest < 1){` |
|       62 | 1105 | `				zIn += sizeof("<<<")-1;` |
|       74 | 1106 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|       13 | 1107 | `					zIn++;` |
|        1 | 1108 | `				}` |
|       62 | 1109 | `				if( zIn[0] == '"' \|\| zIn[0] == '\'' ){` |
|       32 | 1110 | `					zIn++;` |
|       15 | 1111 | `				}` |
|       62 | 1112 | `				zPtr = zIn;` |
|      330 | 1113 | `				while( zIn < zEnd ){` |
|      330 | 1114 | `					if( (unsigned char)zIn[0] >= 0xc0 ){` |
|        - | 1115 | `						/* UTF-8 stream */` |
|       19 | 1116 | `						zIn++;` |
|       37 | 1117 | `						SX_JMP_UTF8(zIn,zEnd);` |
|      320 | 1118 | `					}else if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       32 | 1119 | `						break;` |
|      ! 0 | 1120 | `					}else{` |
|      252 | 1121 | `						zIn++;` |
|        - | 1122 | `					}` |
|        2 | 1123 | `				}` |
|       62 | 1124 | `				SyStringInitFromBuf(&sDoc,zPtr,zIn-zPtr);` |
|       62 | 1125 | `				SyStringFullTrim(&sDoc);` |
|       62 | 1126 | `				if( sDoc.nByte > 0 ){` |
|       62 | 1127 | `					iNest++;` |
|       30 | 1128 | `				}` |
|       62 | 1129 | `				continue;` |
|        - | 1130 | `			}` |
|   883762 | 1131 | `			zIn++;` |
|        - | 1132 |  |
|   883762 | 1133 | `			if ( zIn >= zEnd )` |
|      ! 0 | 1134 | `				break;` |
|        2 | 1135 | `		}` |
|    10082 | 1136 | `		if( (sxu32)(zEnd - zIn) < sCtag.nByte ){` |
|     4324 | 1137 | `			zIn = zEnd;` |
|     2161 | 1138 | `		}` |
|    10082 | 1139 | `		if( zCur < zIn ){` |
|        - | 1140 | `			/* Save the PHP chunk for later processing */` |
|     8212 | 1141 | `			sToken.nType = PH7_TOKEN_PHP;` |
|     8212 | 1142 | `			SyStringInitFromBuf(&sToken.sData,zCur,zIn-zCur);` |
|    16402 | 1143 | `			SyStringRightTrim(&sToken.sData); /* Trim trailing white spaces */` |
|     8212 | 1144 | `			rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|     8212 | 1145 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1146 | `				return rc;` |
|        - | 1147 | `			}` |
|     4105 | 1148 | `		}` |
|    10082 | 1149 | `		if( zIn < zEnd ){` |
|        - | 1150 | `			/* Jump the trailing closing tag */` |
|     5760 | 1151 | `			zIn += sCtag.nByte;` |
|     2879 | 1152 | `		}` |
|        2 | 1153 | `	} /* For(;;) */` |
|        - | 1154 |  |
|    10078 | 1155 | ` 	return SXRET_OK;` |
|     5040 | 1156 |  |
|        - | 1157 |  |
