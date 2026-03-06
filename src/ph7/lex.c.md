# src/ph7/lex.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 628/663 lines (94.72%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits | Line | Source |
| ------: | ---: | :--- |
|       - |    1 | `/**` |
|       - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|       - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |    5 | ` */` |
|       - |    6 | `#include "ph7int.h"` |
|       - |    7 | `/*` |
|       - |    8 | ` * This file implement an efficient hand-coded,thread-safe and full-reentrant` |
|       - |    9 | ` * lexical analyzer/Tokenizer for the PH7 engine.` |
|       - |   10 | ` */` |
|       - |   11 | `/* Forward declaration */` |
|       - |   12 | `static sxu32 KeywordCode(const char *z, int n);` |
|       - |   13 | `static sxi32 LexExtractHeredoc(SyStream *pStream,SyToken *pToken);` |
|       - |   14 | `/*` |
|       - |   15 | ` * Tokenize a raw PHP input.` |
|       - |   16 | ` * Get a single low-level token from the input file. Update the stream pointer so that` |
|       - |   17 | ` * it points to the first character beyond the extracted token.` |
|       - |   18 | ` */` |
| 3446296 |   19 | `static sxi32 TokenizePHP(SyStream *pStream,SyToken *pToken,void *pUserData,void *pCtxData)` |
|       2 |   20 |  |
|       - |   21 | `	SyString *pStr;` |
|       - |   22 | `	sxi32 rc;` |
|       - |   23 | `	/* Ignore leading white spaces */` |
| 5168520 |   24 | `	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){` |
|       - |   25 | `		/* Advance the stream cursor */` |
| 1722224 |   26 | `		if( pStream->zText[0] == '\n' ){` |
|       - |   27 | `			/* Update line counter */` |
|   23396 |   28 | `			pStream->nLine++;` |
|   11697 |   29 | `		}` |
| 1722224 |   30 | `		pStream->zText++;` |
|       2 |   31 | `	}` |
| 3446298 |   32 | `	if( pStream->zText >= pStream->zEnd ){` |
|       - |   33 | `		/* End of input reached */` |
|     ! 0 |   34 | `		return SXERR_EOF;` |
|       - |   35 | `	}` |
|       - |   36 | `	/* Record token starting position and line */` |
| 3446298 |   37 | `	pToken->nLine = pStream->nLine;` |
| 3446298 |   38 | `	pToken->pUserData = 0;` |
| 3446298 |   39 | `	pStr = &pToken->sData;` |
| 3446298 |   40 | `	SyStringInitFromBuf(pStr,pStream->zText,0);` |
| 4049183 |   41 | `	if( pStream->zText[0] >= 0xc0 \|\| SyisAlpha(pStream->zText[0]) \|\| pStream->zText[0] == '_' ){` |
|       - |   42 | `		/* The following code fragment is taken verbatim from the xPP source tree.` |
|       - |   43 | `		 * xPP is a modern embeddable macro processor with advanced features useful for` |
|       - |   44 | `		 * application seeking for a production quality,ready to use macro processor.` |
|       - |   45 | `		 * xPP is a widely used library developed and maintened by Symisc Systems.` |
|       - |   46 | `		 * You can reach the xPP home page by following this link:` |
|       - |   47 | `		 * http://xpp.symisc.net/` |
|       - |   48 | `		 */` |
|       - |   49 | `		const unsigned char *zIn;` |
|       - |   50 | `		sxu32 nKeyword;` |
|       - |   51 | `		/* Isolate UTF-8 or alphanumeric stream */` |
| 1205772 |   52 | `		if( pStream->zText[0] < 0xc0 ){` |
| 1205756 |   53 | `			pStream->zText++;` |
|  602877 |   54 | `		}` |
| 1180339 |   55 | `		for(;;){` |
| 2360680 |   56 | `			zIn = pStream->zText;` |
| 2360680 |   57 | `			if( zIn[0] >= 0xc0 ){` |
|      49 |   58 | `				zIn++;` |
|       - |   59 | `				/* UTF-8 stream */` |
|     109 |   60 | `				while( zIn < pStream->zEnd && ((zIn[0] & 0xc0) == 0x80) ){` |
|      61 |   61 | `					zIn++;` |
|       1 |   62 | `				}` |
|      24 |   63 | `			}` |
|       - |   64 | `			/* Skip alphanumeric stream */` |
| 9379095 |   65 | `			while( zIn < pStream->zEnd && zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_') ){` |
| 5838078 |   66 | `				zIn++;` |
|       2 |   67 | `			}` |
| 2360680 |   68 | `			if( zIn == pStream->zText ){` |
|       - |   69 | `				/* Not an UTF-8 or alphanumeric stream */` |
| 1205772 |   70 | `				break;` |
|       - |   71 | `			}` |
|       - |   72 | `			/* Synchronize pointers */` |
| 1154910 |   73 | `			pStream->zText = zIn;` |
|       2 |   74 | `		}` |
|       - |   75 | `		/* Record token length */` |
| 1205772 |   76 | `		pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
| 1205772 |   77 | `		nKeyword = KeywordCode(pStr->zString,(int)pStr->nByte);` |
| 1205772 |   78 | `		if( nKeyword != PH7_TK_ID ){` |
|  388882 |   79 | `			if( nKeyword &` |
|       - |   80 | `				(PH7_TKWRD_NEW\|PH7_TKWRD_CLONE\|PH7_TKWRD_AND\|PH7_TKWRD_XOR\|PH7_TKWRD_OR\|PH7_TKWRD_INSTANCEOF\|PH7_TKWRD_SEQ\|PH7_TKWRD_SNE) ){` |
|       - |   81 | `					/* Alpha stream operators [i.e: new,clone,and,instanceof,eq,ne,or,xor],save the operator instance for later processing */` |
|    3652 |   82 | `					pToken->pUserData = (void *)PH7_ExprExtractOperator(pStr,0);` |
|       - |   83 | `					/* Mark as an operator */` |
|    3652 |   84 | `					pToken->nType = PH7_TK_ID\|PH7_TK_OP;` |
|    1827 |   85 | `			}else{` |
|       - |   86 | `				/* We are dealing with a keyword [i.e: while,foreach,class...],save the keyword ID */` |
|  385232 |   87 | `				pToken->nType = PH7_TK_KEYWORD;` |
|  385232 |   88 | `				pToken->pUserData = SX_INT_TO_PTR(nKeyword);` |
|       - |   89 | `			}` |
|  194442 |   90 | `		}else{` |
|       - |   91 | `			/* A simple identifier */` |
|  816892 |   92 | `			pToken->nType = PH7_TK_ID;` |
|       - |   93 | `		}` |
|  602887 |   94 | `	}else{` |
|       - |   95 | `		sxi32 c;` |
|       - |   96 | `		/* Non-alpha stream */` |
| 2262667 |   97 | `		if( pStream->zText[0] == '#' \|\|` |
| 2240526 |   98 | `			( pStream->zText[0] == '/' &&  &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '/') ){` |
|    3692 |   99 | `				pStream->zText++;` |
|       - |  100 | `				/* Inline comments */` |
|  131584 |  101 | `				while( pStream->zText < pStream->zEnd && pStream->zText[0] != '\n' ){` |
|  127894 |  102 | `					pStream->zText++;` |
|       2 |  103 | `				}` |
|       - |  104 | `				/* Tell the upper-layer to ignore this token */` |
|    3692 |  105 | `				return SXERR_CONTINUE;` |
| 2236838 |  106 | `		}else if( pStream->zText[0] == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '*' ){` |
|   40532 |  107 | `			pStream->zText += 2;` |
|       - |  108 | `			/* Block comment */` |
| 1130528 |  109 | `			while( pStream->zText < pStream->zEnd ){` |
| 1130528 |  110 | `				if( pStream->zText[0] == '*' ){` |
|   40558 |  111 | `					if( &pStream->zText[1] >= pStream->zEnd \|\| pStream->zText[1] == '/'  ){` |
|   20267 |  112 | `						break;` |
|       - |  113 | `					}` |
|      13 |  114 | `				}` |
| 1089998 |  115 | `				if( pStream->zText[0] == '\n' ){` |
|      28 |  116 | `					pStream->nLine++;` |
|      13 |  117 | `				}` |
| 1089998 |  118 | `				pStream->zText++;` |
|       2 |  119 | `			}` |
|   40532 |  120 | `			pStream->zText += 2;` |
|       - |  121 | `			/* Tell the upper-layer to ignore this token */` |
|   40532 |  122 | `			return SXERR_CONTINUE;` |
| 2196308 |  123 | `		}else if( SyisDigit(pStream->zText[0]) ){` |
|   55552 |  124 | `			pStream->zText++;` |
|       - |  125 | `			/* Decimal digit stream */` |
|   60966 |  126 | `			while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|    5416 |  127 | `				pStream->zText++;` |
|       2 |  128 | `			}` |
|       - |  129 | `			/* Mark the token as integer until we encounter a real number */` |
|   55552 |  130 | `			pToken->nType = PH7_TK_INTEGER;` |
|   55552 |  131 | `			if( pStream->zText < pStream->zEnd ){` |
|   55548 |  132 | `				c = pStream->zText[0];` |
|   55548 |  133 | `				if( c == '.' ){` |
|       - |  134 | `					/* Real number */` |
|     381 |  135 | `					pStream->zText++;` |
|    1519 |  136 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|    1139 |  137 | `						pStream->zText++;` |
|       1 |  138 | `					}` |
|     381 |  139 | `					if( pStream->zText < pStream->zEnd ){` |
|     381 |  140 | `						c = pStream->zText[0];` |
|     381 |  141 | `						if( c=='e' \|\| c=='E' ){` |
|      19 |  142 | `							pStream->zText++;` |
|      19 |  143 | `							if( pStream->zText < pStream->zEnd ){` |
|      19 |  144 | `								c = pStream->zText[0];` |
|      24 |  145 | `								if( (c =='+' \|\| c=='-') && &pStream->zText[1] < pStream->zEnd  &&` |
|      13 |  146 | `									pStream->zText[1] < 0xc0 && SyisDigit(pStream->zText[1]) ){` |
|      13 |  147 | `										pStream->zText++;` |
|       6 |  148 | `								}` |
|      39 |  149 | `								while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|      21 |  150 | `									pStream->zText++;` |
|       1 |  151 | `								}` |
|       9 |  152 | `							}` |
|       9 |  153 | `						}` |
|     190 |  154 | `					}` |
|     381 |  155 | `					pToken->nType = PH7_TK_REAL;` |
|   55358 |  156 | `				}else if( c=='e' \|\| c=='E' ){` |
|       7 |  157 | `					SXUNUSED(pUserData); /* Prevent compiler warning */` |
|       7 |  158 | `					SXUNUSED(pCtxData);` |
|      15 |  159 | `					pStream->zText++;` |
|      15 |  160 | `					if( pStream->zText < pStream->zEnd ){` |
|      15 |  161 | `						c = pStream->zText[0];` |
|      16 |  162 | `						if( (c =='+' \|\| c=='-') && &pStream->zText[1] < pStream->zEnd  &&` |
|       5 |  163 | `							pStream->zText[1] < 0xc0 && SyisDigit(pStream->zText[1]) ){` |
|       5 |  164 | `								pStream->zText++;` |
|       2 |  165 | `						}` |
|      33 |  166 | `						while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|      19 |  167 | `							pStream->zText++;` |
|       1 |  168 | `						}` |
|       7 |  169 | `					}` |
|      15 |  170 | `					pToken->nType = PH7_TK_REAL;` |
|   55161 |  171 | `				}else if( c == 'x' \|\| c == 'X' ){` |
|       - |  172 | `					/* Hex digit stream */` |
|      16 |  173 | `					pStream->zText++;` |
|      50 |  174 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisHex(pStream->zText[0]) ){` |
|      35 |  175 | `						pStream->zText++;` |
|       1 |  176 | `					}` |
|   55147 |  177 | `				}else if(c  == 'b' \|\| c == 'B' ){` |
|       - |  178 | `					/* Binary digit stream */` |
|      31 |  179 | `					pStream->zText++;` |
|     198 |  180 | `					while( pStream->zText < pStream->zEnd && (pStream->zText[0] == '0' \|\| pStream->zText[0] == '1') ){` |
|     153 |  181 | `						pStream->zText++;` |
|       1 |  182 | `					}` |
|      15 |  183 | `				}` |
|   27773 |  184 | `			}` |
|       - |  185 | `			/* Record token length */` |
|   55552 |  186 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|   55552 |  187 | `			return SXRET_OK;` |
|       - |  188 | `		}` |
| 2140758 |  189 | `		c = pStream->zText[0];` |
| 2140758 |  190 | `		pStream->zText++; /* Advance the stream cursor */` |
|       - |  191 | `		/* Assume we are dealing with an operator*/` |
| 2140758 |  192 | `		pToken->nType = PH7_TK_OP;` |
| 2140758 |  193 | `		switch(c){` |
|  462800 |  194 | `		case '$': pToken->nType = PH7_TK_DOLLAR; break;` |
|  157742 |  195 | `		case '{': pToken->nType = PH7_TK_OCB;    break;` |
|  157728 |  196 | `		case '}': pToken->nType = PH7_TK_CCB;    break;` |
|  317374 |  197 | `		case '(': pToken->nType = PH7_TK_LPAREN; break;` |
|   38334 |  198 | `		case '[': pToken->nType \|= PH7_TK_OSB;   break; /* Bitwise operation here,since the square bracket token '['` |
|       - |  199 | `														 * is a potential operator [i.e: subscripting] */` |
|   38340 |  200 | `		case ']': pToken->nType = PH7_TK_CSB;    break;` |
|  158680 |  201 | `		case ')': {` |
|  317362 |  202 | `			SySet *pTokSet = pStream->pSet;` |
|       - |  203 | `			/* Assemble type cast operators [i.e: (int),(float),(bool)...] */` |
|  317362 |  204 | `			if( pTokSet->nUsed >= 2 ){` |
|       - |  205 | `				SyToken *pTmp;` |
|       - |  206 | `				/* Peek the last recongnized token */` |
|  317360 |  207 | `				pTmp = (SyToken *)SySetPeek(pTokSet);` |
|  317360 |  208 | `				if( pTmp->nType & PH7_TK_KEYWORD ){` |
|    9984 |  209 | `					sxi32 nID = SX_PTR_TO_INT(pTmp->pUserData);` |
|    9984 |  210 | `					if( (sxu32)nID & (PH7_TKWRD_ARRAY\|PH7_TKWRD_INT\|PH7_TKWRD_FLOAT\|PH7_TKWRD_STRING\|PH7_TKWRD_OBJECT\|PH7_TKWRD_BOOL\|PH7_TKWRD_UNSET) ){` |
|    9928 |  211 | `						pTmp = (SyToken *)SySetAt(pTokSet,pTokSet->nUsed - 2);` |
|    9928 |  212 | `						if( pTmp->nType & PH7_TK_LPAREN ){` |
|       - |  213 | `							/* Merge the three tokens '(' 'TYPE' ')' into a single one */` |
|    9842 |  214 | `							const char * zTypeCast = "(int)";` |
|    9842 |  215 | `							if( nID & PH7_TKWRD_FLOAT ){` |
|    1626 |  216 | `								zTypeCast = "(float)";` |
|    9030 |  217 | `							}else if( nID & PH7_TKWRD_BOOL ){` |
|    1628 |  218 | `								zTypeCast = "(bool)";` |
|    7405 |  219 | `							}else if( nID & PH7_TKWRD_STRING ){` |
|    4864 |  220 | `								zTypeCast = "(string)";` |
|    4161 |  221 | `							}else if( nID & PH7_TKWRD_ARRAY ){` |
|      21 |  222 | `								zTypeCast = "(array)";` |
|    1720 |  223 | `							}else if( nID & PH7_TKWRD_OBJECT ){` |
|      17 |  224 | `								zTypeCast = "(object)";` |
|    1702 |  225 | `							}else if( nID & PH7_TKWRD_UNSET ){` |
|       7 |  226 | `								zTypeCast = "(unset)";` |
|       3 |  227 | `							}` |
|       - |  228 | `							/* Reflect the change */` |
|    9842 |  229 | `							pToken->nType = PH7_TK_OP;` |
|    9842 |  230 | `							SyStringInitFromBuf(&pToken->sData,zTypeCast,SyStrlen(zTypeCast));` |
|       - |  231 | `							/* Save the instance associated with the type cast operator */` |
|    9842 |  232 | `							pToken->pUserData = (void *)PH7_ExprExtractOperator(&pToken->sData,0);` |
|       - |  233 | `							/* Remove the two previous tokens */` |
|    9842 |  234 | `							pTokSet->nUsed -= 2;` |
|    9842 |  235 | `							return SXRET_OK;` |
|       - |  236 | `						}` |
|      43 |  237 | `					}` |
|      71 |  238 | `				}` |
|  153759 |  239 | `			}` |
|  307522 |  240 | `			pToken->nType = PH7_TK_RPAREN;` |
|  307522 |  241 | `			break;` |
|       - |  242 | `				  }` |
|   13668 |  243 | `		case '\'':{` |
|       - |  244 | `			/* Single quoted string */` |
|   27338 |  245 | `			pStr->zString++;` |
|  151286 |  246 | `			while( pStream->zText < pStream->zEnd ){` |
|  151286 |  247 | `				if( pStream->zText[0] == '\''  ){` |
|   27348 |  248 | `					if( pStream->zText[-1] != '\\' ){` |
|   27324 |  249 | `						break;` |
|     ! 0 |  250 | `					}else{` |
|      25 |  251 | `						const unsigned char *zPtr = &pStream->zText[-2];` |
|      25 |  252 | `						sxi32 i = 1;` |
|      43 |  253 | `						while( zPtr > pStream->zInput && zPtr[0] == '\\' ){` |
|      19 |  254 | `							zPtr--;` |
|      19 |  255 | `							i++;` |
|       1 |  256 | `						}` |
|      25 |  257 | `						if((i&1)==0){` |
|      15 |  258 | `							break;` |
|       - |  259 | `						}` |
|       - |  260 | `					}` |
|       5 |  261 | `				}` |
|  123950 |  262 | `				if( pStream->zText[0] == '\n' ){` |
|       7 |  263 | `					pStream->nLine++;` |
|       3 |  264 | `				}` |
|  123950 |  265 | `				pStream->zText++;` |
|       2 |  266 | `			}` |
|       - |  267 | `			/* Record token length and type */` |
|   27338 |  268 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|   27338 |  269 | `			pToken->nType = PH7_TK_SSTR;` |
|       - |  270 | `			/* Jump the trailing single quote */` |
|   27338 |  271 | `			pStream->zText++;` |
|   27338 |  272 | `			return SXRET_OK;` |
|       - |  273 | `				  }` |
|    6172 |  274 | `		case '"':{` |
|       - |  275 | `			sxi32 iNest;` |
|       - |  276 | `			/* Double quoted string */` |
|   12346 |  277 | `			pStr->zString++;` |
|  138422 |  278 | `			while( pStream->zText < pStream->zEnd ){` |
|  138422 |  279 | `				if( pStream->zText[0] == '{' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '$'){` |
|      71 |  280 | `					iNest = 1;` |
|      71 |  281 | `					pStream->zText++;` |
|       - |  282 | `					/* TICKET 1433-40: Hnadle braces'{}' in double quoted string where everything is allowed */` |
|     823 |  283 | `					while(pStream->zText < pStream->zEnd ){` |
|     823 |  284 | `						if( pStream->zText[0] == '{' ){` |
|       9 |  285 | `							iNest++;` |
|     819 |  286 | `						}else if (pStream->zText[0] == '}' ){` |
|      79 |  287 | `							iNest--;` |
|      79 |  288 | `							if( iNest <= 0 ){` |
|      71 |  289 | `								pStream->zText++;` |
|      71 |  290 | `								break;` |
|       1 |  291 | `							}` |
|     741 |  292 | `						}else if( pStream->zText[0] == '\n' ){` |
|     ! 0 |  293 | `							pStream->nLine++;` |
|     ! 0 |  294 | `						}` |
|     753 |  295 | `						pStream->zText++;` |
|       1 |  296 | `					}` |
|      71 |  297 | `					if( pStream->zText >= pStream->zEnd ){` |
|     ! 0 |  298 | `						break;` |
|       - |  299 | `					}` |
|      35 |  300 | `				}` |
|  138422 |  301 | `				if( pStream->zText[0] == '"' ){` |
|   12446 |  302 | `					if( pStream->zText[-1] != '\\' ){` |
|   12342 |  303 | `						break;` |
|     ! 0 |  304 | `					}else{` |
|     106 |  305 | `						const unsigned char *zPtr = &pStream->zText[-2];` |
|     106 |  306 | `						sxi32 i = 1;` |
|     158 |  307 | `						while( zPtr > pStream->zInput && zPtr[0] == '\\' ){` |
|      54 |  308 | `							zPtr--;` |
|      54 |  309 | `							i++;` |
|       2 |  310 | `						}` |
|     106 |  311 | `						if((i&1)==0){` |
|       5 |  312 | `							break;` |
|       - |  313 | `						}` |
|       - |  314 | `					}` |
|      50 |  315 | `				}` |
|  126078 |  316 | `				if( pStream->zText[0] == '\n' ){` |
|       7 |  317 | `					pStream->nLine++;` |
|       3 |  318 | `				}` |
|  126078 |  319 | `				pStream->zText++;` |
|       2 |  320 | `			}` |
|       - |  321 | `			/* Record token length and type */` |
|   12346 |  322 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|   12346 |  323 | `			pToken->nType = PH7_TK_DSTR;` |
|       - |  324 | `			/* Jump the trailing quote */` |
|   12346 |  325 | `			pStream->zText++;` |
|   12346 |  326 | `			return SXRET_OK;` |
|       - |  327 | `				  }` |
|       2 |  328 | ``		case '`':{`` |
|       - |  329 | `			/* Backtick quoted string */` |
|       5 |  330 | `			pStr->zString++;` |
|      45 |  331 | `			while( pStream->zText < pStream->zEnd ){` |
|      45 |  332 | ``				if( pStream->zText[0] == '`' && pStream->zText[-1] != '\\' ){`` |
|       5 |  333 | `					break;` |
|       - |  334 | `				}` |
|      41 |  335 | `				if( pStream->zText[0] == '\n' ){` |
|     ! 0 |  336 | `					pStream->nLine++;` |
|     ! 0 |  337 | `				}` |
|      41 |  338 | `				pStream->zText++;` |
|       1 |  339 | `			}` |
|       - |  340 | `			/* Record token length and type */` |
|       5 |  341 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|       5 |  342 | `			pToken->nType = PH7_TK_BSTR;` |
|       - |  343 | `			/* Jump the trailing backtick */` |
|       5 |  344 | `			pStream->zText++;` |
|       5 |  345 | `			return SXRET_OK;` |
|       - |  346 | `				  }` |
|      29 |  347 | `		case '\\': pToken->nType = PH7_TK_NSSEP;  break;` |
|     918 |  348 | `		case ':':` |
|    1838 |  349 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == ':' ){` |
|       - |  350 | `				/* Current operator: '::' */` |
|      56 |  351 | `				pStream->zText++;` |
|      29 |  352 | `			}else{` |
|    1784 |  353 | `				pToken->nType = PH7_TK_COLON; /* Single colon */` |
|       - |  354 | `			}` |
|    1838 |  355 | `			break;` |
|   33820 |  356 | `		case ',': pToken->nType \|= PH7_TK_COMMA;  break; /* Comma is also an operator */` |
|  252630 |  357 | `		case ';': pToken->nType = PH7_TK_SEMI;    break;` |
|       - |  358 | `			/* Handle combined operators [i.e: +=,===,!=== ...] */` |
|   79481 |  359 | `		case '=':` |
|  158964 |  360 | `			pToken->nType \|= PH7_TK_EQUAL;` |
|  158964 |  361 | `			if( pStream->zText < pStream->zEnd ){` |
|  158964 |  362 | `				if( pStream->zText[0] == '=' ){` |
|   11094 |  363 | `					pToken->nType &= ~PH7_TK_EQUAL;` |
|       - |  364 | `					/* Current operator: == */` |
|   11094 |  365 | `					pStream->zText++;` |
|   11094 |  366 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|       - |  367 | `						/* Current operator: === */` |
|    2718 |  368 | `						pStream->zText++;` |
|    1360 |  369 | `					}` |
|  153418 |  370 | `				}else if( pStream->zText[0] == '>' ){` |
|       - |  371 | `					/* Array operator: => */` |
|    2484 |  372 | `					pToken->nType = PH7_TK_ARRAY_OP;` |
|    2484 |  373 | `					pStream->zText++;` |
|    1243 |  374 | `				}else{` |
|       - |  375 | `					/* TICKET 1433-0010: Reference operator '=&' */` |
|  145390 |  376 | `					const unsigned char *zCur = pStream->zText;` |
|  145390 |  377 | `					sxu32 nLine = 0;` |
|  290758 |  378 | `					while( zCur < pStream->zEnd && zCur[0] < 0xc0 && SyisSpace(zCur[0]) ){` |
|  145370 |  379 | `						if( zCur[0] == '\n' ){` |
|       5 |  380 | `							nLine++;` |
|       2 |  381 | `						}` |
|  145370 |  382 | `						zCur++;` |
|       2 |  383 | `					}` |
|  145390 |  384 | `					if( zCur < pStream->zEnd && zCur[0] == '&' ){` |
|       - |  385 | `						/* Current operator: =& */` |
|      42 |  386 | `						pToken->nType &= ~PH7_TK_EQUAL;` |
|      42 |  387 | `						SyStringInitFromBuf(pStr,"=&",sizeof("=&")-1);` |
|       - |  388 | `						/* Update token stream */` |
|      42 |  389 | `						pStream->zText = &zCur[1];` |
|      42 |  390 | `						pStream->nLine += nLine;` |
|      20 |  391 | `					}` |
|       - |  392 | `				}` |
|   79481 |  393 | `			}` |
|  158964 |  394 | `			break;` |
|   10844 |  395 | `		case '!':` |
|   21690 |  396 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|       - |  397 | `				/* Current operator: != */` |
|    9866 |  398 | `				pStream->zText++;` |
|    9866 |  399 | `				if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|       - |  400 | `					/* Current operator: !== */` |
|    8220 |  401 | `					pStream->zText++;` |
|    4109 |  402 | `				}` |
|    4932 |  403 | `			}` |
|   21690 |  404 | `			break;` |
|    5881 |  405 | `		case '&':` |
|   11764 |  406 | `			pToken->nType \|= PH7_TK_AMPER;` |
|   11764 |  407 | `			if( pStream->zText < pStream->zEnd ){` |
|   11764 |  408 | `				if( pStream->zText[0] == '&' ){` |
|    3594 |  409 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|       - |  410 | `					/* Current operator: && */` |
|    3594 |  411 | `					pStream->zText++;` |
|    9968 |  412 | `				}else if( pStream->zText[0] == '=' ){` |
|       5 |  413 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|       - |  414 | `					/* Current operator: &= */` |
|       5 |  415 | `					pStream->zText++;` |
|       2 |  416 | `				}` |
|    5881 |  417 | `			}` |
|   11764 |  418 | `			break;` |
|     881 |  419 | `		case '\|':` |
|    1764 |  420 | `			if( pStream->zText < pStream->zEnd ){` |
|    1764 |  421 | `				if( pStream->zText[0] == '\|' ){` |
|       - |  422 | `					/* Current operator: \|\| */` |
|    1748 |  423 | `					pStream->zText++;` |
|     890 |  424 | `				}else if( pStream->zText[0] == '=' ){` |
|       - |  425 | `					/* Current operator: \|= */` |
|       5 |  426 | `					pStream->zText++;` |
|       2 |  427 | `				}` |
|     881 |  428 | `			}` |
|    1764 |  429 | `			break;` |
|    3514 |  430 | `		case '+':` |
|    7030 |  431 | `			if( pStream->zText < pStream->zEnd ){` |
|    7028 |  432 | `				if( pStream->zText[0] == '+' ){` |
|       - |  433 | `					/* Current operator: ++ */` |
|    6666 |  434 | `					pStream->zText++;` |
|    3696 |  435 | `				}else if( pStream->zText[0] == '=' ){` |
|       - |  436 | `					/* Current operator: += */` |
|      27 |  437 | `					pStream->zText++;` |
|      13 |  438 | `				}` |
|    3513 |  439 | `			}` |
|    7030 |  440 | `			break;` |
|   31123 |  441 | `		case '-':` |
|   62248 |  442 | `			if( pStream->zText < pStream->zEnd ){` |
|   62248 |  443 | `				if( pStream->zText[0] == '-' ){` |
|       - |  444 | `					/* Current operator: -- */` |
|       5 |  445 | `					pStream->zText++;` |
|   62246 |  446 | `				}else if( pStream->zText[0] == '=' ){` |
|       - |  447 | `					/* Current operator: -= */` |
|       3 |  448 | `					pStream->zText++;` |
|   62243 |  449 | `				}else if( pStream->zText[0] == '>' ){` |
|       - |  450 | `					/* Current operator: -> */` |
|   61840 |  451 | `					pStream->zText++;` |
|   30919 |  452 | `				}` |
|   31123 |  453 | `			}` |
|   62248 |  454 | `			break;` |
|      69 |  455 | `		case '*':` |
|     140 |  456 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|       - |  457 | `				/* Current operator: *= */` |
|      11 |  458 | `				pStream->zText++;` |
|       5 |  459 | `			}` |
|     140 |  460 | `			break;` |
|      29 |  461 | `		case '/':` |
|      60 |  462 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|       - |  463 | `				/* Current operator: /= */` |
|       3 |  464 | `				pStream->zText++;` |
|       1 |  465 | `			}` |
|      60 |  466 | `			break;` |
|      14 |  467 | `		case '%':` |
|      30 |  468 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|       - |  469 | `				/* Current operator: %= */` |
|       3 |  470 | `				pStream->zText++;` |
|       1 |  471 | `			}` |
|      30 |  472 | `			break;` |
|       9 |  473 | `		case '^':` |
|      19 |  474 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|       - |  475 | `				/* Current operator: ^= */` |
|       7 |  476 | `				pStream->zText++;` |
|       3 |  477 | `			}` |
|      19 |  478 | `			break;` |
|   13167 |  479 | `		case '.':` |
|   26336 |  480 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|       - |  481 | `				/* Current operator: .= */` |
|    1658 |  482 | `				pStream->zText++;` |
|     828 |  483 | `			}` |
|   26336 |  484 | `			break;` |
|   13939 |  485 | `		case '<':` |
|   27880 |  486 | `			if( pStream->zText < pStream->zEnd ){` |
|   27880 |  487 | `				if( pStream->zText[0] == '<' ){` |
|       - |  488 | `					/* Current operator: << */` |
|      74 |  489 | `					pStream->zText++;` |
|      74 |  490 | `					if( pStream->zText < pStream->zEnd ){` |
|      74 |  491 | `						if( pStream->zText[0] == '=' ){` |
|       - |  492 | `							/* Current operator: <<= */` |
|       7 |  493 | `							pStream->zText++;` |
|      71 |  494 | `						}else if( pStream->zText[0] == '<' ){` |
|       - |  495 | `							/* Current Token: <<<  */` |
|      58 |  496 | `							pStream->zText++;` |
|       - |  497 | `							/* This may be the beginning of a Heredoc/Nowdoc string,try to delimit it */` |
|      58 |  498 | `							rc = LexExtractHeredoc(&(*pStream),&(*pToken));` |
|      58 |  499 | `							if( rc == SXRET_OK ){` |
|       - |  500 | `								/* Here/Now doc successfuly extracted */` |
|      58 |  501 | `								return SXRET_OK;` |
|       - |  502 | `							}` |
|     ! 0 |  503 | `						}` |
|       9 |  504 | `					}` |
|   27816 |  505 | `				}else if( pStream->zText[0] == '>' ){` |
|       - |  506 | `					/* Current operator: <> */` |
|       5 |  507 | `					pStream->zText++;` |
|   27806 |  508 | `				}else if( pStream->zText[0] == '=' ){` |
|       - |  509 | `					/* Current operator: <= */` |
|      27 |  510 | `					pStream->zText++;` |
|      13 |  511 | `				}` |
|   13911 |  512 | `			}` |
|   27824 |  513 | `			break;` |
|    1716 |  514 | `		case '>':` |
|    3434 |  515 | `			if( pStream->zText < pStream->zEnd ){` |
|    3434 |  516 | `				if( pStream->zText[0] == '>' ){` |
|       - |  517 | `					/* Current operator: >> */` |
|      17 |  518 | `					pStream->zText++;` |
|      17 |  519 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|       - |  520 | `						/* Current operator: >>= */` |
|       9 |  521 | `						pStream->zText++;` |
|       5 |  522 | `					}` |
|    3426 |  523 | `				}else if( pStream->zText[0] == '=' ){` |
|       - |  524 | `					/* Current operator: >= */` |
|      72 |  525 | `					pStream->zText++;` |
|      35 |  526 | `				}` |
|    1716 |  527 | `			}` |
|    3432 |  528 | `			break;` |
|     881 |  529 | `		default:` |
|    1762 |  530 | `			break;` |
|       - |  531 | `		}` |
| 2091178 |  532 | `		if( pStr->nByte <= 0 ){` |
|       - |  533 | `			/* Record token length */` |
| 2091138 |  534 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
| 1045568 |  535 | `		}` |
| 2091178 |  536 | `		if( pToken->nType & PH7_TK_OP ){` |
|       - |  537 | `			const ph7_expr_op *pOp;` |
|       - |  538 | `			/* Check if the extracted token is an operator */` |
|  392764 |  539 | `			pOp = PH7_ExprExtractOperator(pStr,(SyToken *)SySetPeek(pStream->pSet));` |
|  392764 |  540 | `			if( pOp == 0 ){` |
|       - |  541 | `				/* Not an operator */` |
|     ! 0 |  542 | `				pToken->nType &= ~PH7_TK_OP;` |
|     ! 0 |  543 | `				if( pToken->nType <= 0 ){` |
|     ! 0 |  544 | `					pToken->nType = PH7_TK_OTHER;` |
|     ! 0 |  545 | `				}` |
|     ! 0 |  546 | `			}else{` |
|       - |  547 | `				/* Save the instance associated with this operator for later processing */` |
|  392764 |  548 | `				pToken->pUserData = (void *)pOp;` |
|       - |  549 | `			}` |
|  196381 |  550 | `		}` |
|       - |  551 | `	}` |
|       - |  552 | `	/* Tell the upper-layer to save the extracted token for later processing */` |
| 3296948 |  553 | `	return SXRET_OK;` |
| 1723150 |  554 |  |
|       - |  555 | `/***** This file contains automatically generated code ******` |
|       - |  556 | `**` |
|       - |  557 | `** The code in this file has been automatically generated by` |
|       - |  558 | `**` |
|       - |  559 | `**     $Header: /sqlite/sqlite/tool/mkkeywordhash.c` |
|       - |  560 | `**` |
|       - |  561 | `** Sligthly modified by Chems mrad <chm@symisc.net> for the PH7 engine.` |
|       - |  562 | `**` |
|       - |  563 | `** The code in this file implements a function that determines whether` |
|       - |  564 | `** or not a given identifier is really a PHP keyword.  The same thing` |
|       - |  565 | `** might be implemented more directly using a hand-written hash table.` |
|       - |  566 | `** But by using this automatically generated code, the size of the code` |
|       - |  567 | `** is substantially reduced.  This is important for embedded applications` |
|       - |  568 | `** on platforms with limited memory.` |
|       - |  569 | `*/` |
|       - |  570 | `/* Hash score: 103 */` |
| 1205772 |  571 | `static sxu32 KeywordCode(const char *z, int n){` |
|       - |  572 | `  /* zText[] encodes 532 bytes of keywords in 333 bytes */` |
|       - |  573 | `  /*   extendswitchprintegerequire_oncenddeclareturnamespacechobject      */` |
|       - |  574 | `  /*   hrowbooleandefaultrycaselfinalistaticlonewconstringlobaluse        */` |
|       - |  575 | `  /*   lseifloatvarrayANDIEchoUSECHOabstractclasscontinuendifunction      */` |
|       - |  576 | `  /*   diendwhilevaldoexitgotoimplementsinclude_oncemptyinstanceof        */` |
|       - |  577 | `  /*   interfacendforeachissetparentprivateprotectedpublicatchunset       */` |
|       - |  578 | `  /*   xorARRAYASArrayEXITUNSETXORbreak                                   */` |
|       - |  579 | `  static const char zText[332] = {` |
|       - |  580 | `    'e','x','t','e','n','d','s','w','i','t','c','h','p','r','i','n','t','e',` |
|       - |  581 | `    'g','e','r','e','q','u','i','r','e','_','o','n','c','e','n','d','d','e',` |
|       - |  582 | `    'c','l','a','r','e','t','u','r','n','a','m','e','s','p','a','c','e','c',` |
|       - |  583 | `    'h','o','b','j','e','c','t','h','r','o','w','b','o','o','l','e','a','n',` |
|       - |  584 | `    'd','e','f','a','u','l','t','r','y','c','a','s','e','l','f','i','n','a',` |
|       - |  585 | `    'l','i','s','t','a','t','i','c','l','o','n','e','w','c','o','n','s','t',` |
|       - |  586 | `    'r','i','n','g','l','o','b','a','l','u','s','e','l','s','e','i','f','l',` |
|       - |  587 | `    'o','a','t','v','a','r','r','a','y','A','N','D','I','E','c','h','o','U',` |
|       - |  588 | `    'S','E','C','H','O','a','b','s','t','r','a','c','t','c','l','a','s','s',` |
|       - |  589 | `    'c','o','n','t','i','n','u','e','n','d','i','f','u','n','c','t','i','o',` |
|       - |  590 | `    'n','d','i','e','n','d','w','h','i','l','e','v','a','l','d','o','e','x',` |
|       - |  591 | `    'i','t','g','o','t','o','i','m','p','l','e','m','e','n','t','s','i','n',` |
|       - |  592 | `    'c','l','u','d','e','_','o','n','c','e','m','p','t','y','i','n','s','t',` |
|       - |  593 | `    'a','n','c','e','o','f','i','n','t','e','r','f','a','c','e','n','d','f',` |
|       - |  594 | `    'o','r','e','a','c','h','i','s','s','e','t','p','a','r','e','n','t','p',` |
|       - |  595 | `    'r','i','v','a','t','e','p','r','o','t','e','c','t','e','d','p','u','b',` |
|       - |  596 | `    'l','i','c','a','t','c','h','u','n','s','e','t','x','o','r','A','R','R',` |
|       - |  597 | `    'A','Y','A','S','A','r','r','a','y','E','X','I','T','U','N','S','E','T',` |
|       - |  598 | `    'X','O','R','b','r','e','a','k'` |
|       - |  599 | `  };` |
|       - |  600 | `  static const unsigned char aHash[151] = {` |
|       - |  601 |  |
|       - |  602 |  |
|       - |  603 | `      74,   0,  51,   0,  76,   0,   0,  20,   0,   0,   0,  50,   0,` |
|       - |  604 | `      80,  34,   0,  36,   0,   0,  64,  16,   0,   0,  17,   0,   1,` |
|       - |  605 | `      19,  84,  66,   0,  43,  45,  78,   0,   0,  53,  56,   0,   0,` |
|       - |  606 |  |
|       - |  607 | `      72,  14,   0,  71,   0,  38,   6,   0,   0,   0,  73,   0,   0,` |
|       - |  608 | `       3,   0,  41,   5,  52,  57,  32,   0,  60,  63,   0,  69,  82,` |
|       - |  609 | `      30,   0,  79,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,` |
|       - |  610 |  |
|       - |  611 | `      62,   0,  11,   0,   0,  58,   0,   0,   0,   0,  59,  75,   0,` |
|       - |  612 |  |
|       - |  613 | `  };` |
|       - |  614 | `  static const unsigned char aNext[84] = {` |
|       - |  615 |  |
|       - |  616 |  |
|       - |  617 |  |
|       - |  618 |  |
|       - |  619 |  |
|       - |  620 |  |
|       - |  621 | `      42,   0,   0,   0,  70,  55` |
|       - |  622 | `  };` |
|       - |  623 | `  static const unsigned char aLen[84] = {` |
|       - |  624 | `       7,   9,   6,   5,   7,  12,   7,   2,  10,   7,   6,   9,   4,` |
|       - |  625 | `       6,   5,   7,   4,   3,   7,   3,   4,   4,   5,   4,   6,   5,` |
|       - |  626 | `       2,   3,   5,   6,   6,   3,   6,   4,   2,   5,   3,   5,   3,` |
|       - |  627 | `       3,   4,   3,   4,   8,   5,   2,   8,   5,   8,   3,   8,   5,` |
|       - |  628 | `       4,   2,   4,   4,  10,  12,   7,   5,  10,   9,   3,   6,  10,` |
|       - |  629 | `       3,   7,   2,   5,   6,   7,   9,   6,   5,   5,   3,   5,   2,` |
|       - |  630 | `       5,   4,   5,   3,   2,   5` |
|       - |  631 | `  };` |
|       - |  632 | `  static const sxu16 aOffset[84] = {` |
|       - |  633 |  |
|       - |  634 | `      55,  60,  65,  65,  70,  72,  78,  81,  83,  86,  90,  92,  97,` |
|       - |  635 | `     100, 100, 103, 106, 111, 117, 119, 119, 123, 124, 129, 130, 135,` |
|       - |  636 | `     137, 139, 143, 145, 149, 157, 159, 162, 169, 173, 181, 183, 186,` |
|       - |  637 | `     190, 194, 196, 200, 204, 214, 214, 225, 230, 240, 240, 248, 248,` |
|       - |  638 | `     251, 251, 252, 258, 263, 269, 276, 285, 290, 295, 300, 303, 308,` |
|       - |  639 | `     310, 315, 319, 324, 325, 327` |
|       - |  640 | `  };` |
|       - |  641 | `  static const sxu32 aCode[84] = {` |
|       - |  642 | `    PH7_TKWRD_EXTENDS,   PH7_TKWRD_ENDSWITCH,   PH7_TKWRD_SWITCH,    PH7_TKWRD_PRINT,   PH7_TKWRD_INT,` |
|       - |  643 | `    PH7_TKWRD_REQONCE,   PH7_TKWRD_REQUIRE,     PH7_TKWRD_SEQ,       PH7_TKWRD_ENDDEC,    PH7_TKWRD_DECLARE,` |
|       - |  644 | `    PH7_TKWRD_RETURN,    PH7_TKWRD_NAMESPACE,   PH7_TKWRD_ECHO,      PH7_TKWRD_OBJECT,    PH7_TKWRD_THROW,` |
|       - |  645 | `    PH7_TKWRD_BOOL,      PH7_TKWRD_BOOL,        PH7_TKWRD_AND,       PH7_TKWRD_DEFAULT,   PH7_TKWRD_TRY,` |
|       - |  646 | `    PH7_TKWRD_CASE,      PH7_TKWRD_SELF,        PH7_TKWRD_FINAL,     PH7_TKWRD_LIST,      PH7_TKWRD_STATIC,` |
|       - |  647 | `    PH7_TKWRD_CLONE,     PH7_TKWRD_SNE,         PH7_TKWRD_NEW,       PH7_TKWRD_CONST,     PH7_TKWRD_STRING,` |
|       - |  648 | `    PH7_TKWRD_GLOBAL,    PH7_TKWRD_USE,         PH7_TKWRD_ELIF,      PH7_TKWRD_ELSE,      PH7_TKWRD_IF,` |
|       - |  649 | `    PH7_TKWRD_FLOAT,     PH7_TKWRD_VAR,         PH7_TKWRD_ARRAY,     PH7_TKWRD_AND,       PH7_TKWRD_DIE,` |
|       - |  650 | `    PH7_TKWRD_ECHO,      PH7_TKWRD_USE,         PH7_TKWRD_ECHO,      PH7_TKWRD_ABSTRACT,  PH7_TKWRD_CLASS,` |
|       - |  651 | `    PH7_TKWRD_AS,        PH7_TKWRD_CONTINUE,    PH7_TKWRD_ENDIF,     PH7_TKWRD_FUNCTION,  PH7_TKWRD_DIE,` |
|       - |  652 | `    PH7_TKWRD_ENDWHILE,  PH7_TKWRD_WHILE,       PH7_TKWRD_EVAL,      PH7_TKWRD_DO,        PH7_TKWRD_EXIT,` |
|       - |  653 | `    PH7_TKWRD_GOTO,      PH7_TKWRD_IMPLEMENTS,  PH7_TKWRD_INCONCE,   PH7_TKWRD_INCLUDE,   PH7_TKWRD_EMPTY,` |
|       - |  654 | `    PH7_TKWRD_INSTANCEOF,PH7_TKWRD_INTERFACE,   PH7_TKWRD_INT,       PH7_TKWRD_ENDFOR,    PH7_TKWRD_END4EACH,` |
|       - |  655 | `    PH7_TKWRD_FOR,       PH7_TKWRD_FOREACH,     PH7_TKWRD_OR,        PH7_TKWRD_ISSET,     PH7_TKWRD_PARENT,` |
|       - |  656 | `    PH7_TKWRD_PRIVATE,   PH7_TKWRD_PROTECTED,   PH7_TKWRD_PUBLIC,    PH7_TKWRD_CATCH,     PH7_TKWRD_UNSET,` |
|       - |  657 | `    PH7_TKWRD_XOR,       PH7_TKWRD_ARRAY,       PH7_TKWRD_AS,        PH7_TKWRD_ARRAY,     PH7_TKWRD_EXIT,` |
|       - |  658 | `    PH7_TKWRD_UNSET,     PH7_TKWRD_XOR,         PH7_TKWRD_OR,        PH7_TKWRD_BREAK` |
|       - |  659 | `  };` |
|       - |  660 | `  int h, i;` |
| 1205772 |  661 | `  if( n<2 ) return PH7_TK_ID;` |
| 1154888 |  662 | `  h = (((int)z[0]*4) ^ ((int)z[n-1]*3) ^ n) % 151;` |
| 1750674 |  663 | `  for(i=((int)aHash[h])-1; i>=0; i=((int)aNext[i])-1){` |
|  984668 |  664 | `    if( (int)aLen[i]==n && SyMemcmp(&zText[aOffset[i]],z,n)==0 ){` |
|       - |  665 | `       /* PH7_TKWRD_EXTENDS */` |
|       - |  666 | `       /* PH7_TKWRD_ENDSWITCH */` |
|       - |  667 | `       /* PH7_TKWRD_SWITCH */` |
|       - |  668 | `       /* PH7_TKWRD_PRINT */` |
|       - |  669 | `       /* PH7_TKWRD_INT */` |
|       - |  670 | `       /* PH7_TKWRD_REQONCE */` |
|       - |  671 | `       /* PH7_TKWRD_REQUIRE */` |
|       - |  672 | `       /* PH7_TKWRD_SEQ */` |
|       - |  673 | `       /* PH7_TKWRD_ENDDEC */` |
|       - |  674 | `       /* PH7_TKWRD_DECLARE */` |
|       - |  675 | `       /* PH7_TKWRD_RETURN */` |
|       - |  676 | `       /* PH7_TKWRD_NAMESPACE */` |
|       - |  677 | `       /* PH7_TKWRD_ECHO */` |
|       - |  678 | `       /* PH7_TKWRD_OBJECT */` |
|       - |  679 | `       /* PH7_TKWRD_THROW */` |
|       - |  680 | `       /* PH7_TKWRD_BOOL */` |
|       - |  681 | `       /* PH7_TKWRD_BOOL */` |
|       - |  682 | `       /* PH7_TKWRD_AND */` |
|       - |  683 | `       /* PH7_TKWRD_DEFAULT */` |
|       - |  684 | `       /* PH7_TKWRD_TRY */` |
|       - |  685 | `       /* PH7_TKWRD_CASE */` |
|       - |  686 | `       /* PH7_TKWRD_SELF */` |
|       - |  687 | `       /* PH7_TKWRD_FINAL */` |
|       - |  688 | `       /* PH7_TKWRD_LIST */` |
|       - |  689 | `       /* PH7_TKWRD_STATIC */` |
|       - |  690 | `       /* PH7_TKWRD_CLONE */` |
|       - |  691 | `       /* PH7_TKWRD_SNE */` |
|       - |  692 | `       /* PH7_TKWRD_NEW */` |
|       - |  693 | `       /* PH7_TKWRD_CONST */` |
|       - |  694 | `       /* PH7_TKWRD_STRING */` |
|       - |  695 | `       /* PH7_TKWRD_GLOBAL */` |
|       - |  696 | `       /* PH7_TKWRD_USE */` |
|       - |  697 | `       /* PH7_TKWRD_ELIF */` |
|       - |  698 | `       /* PH7_TKWRD_ELSE */` |
|       - |  699 | `       /* PH7_TKWRD_IF */` |
|       - |  700 | `       /* PH7_TKWRD_FLOAT */` |
|       - |  701 | `       /* PH7_TKWRD_VAR */` |
|       - |  702 | `       /* PH7_TKWRD_ARRAY */` |
|       - |  703 | `       /* PH7_TKWRD_AND */` |
|       - |  704 | `       /* PH7_TKWRD_DIE */` |
|       - |  705 | `       /* PH7_TKWRD_ECHO */` |
|       - |  706 | `       /* PH7_TKWRD_USE */` |
|       - |  707 | `       /* PH7_TKWRD_ECHO */` |
|       - |  708 | `       /* PH7_TKWRD_ABSTRACT */` |
|       - |  709 | `       /* PH7_TKWRD_CLASS */` |
|       - |  710 | `       /* PH7_TKWRD_AS */` |
|       - |  711 | `       /* PH7_TKWRD_CONTINUE */` |
|       - |  712 | `       /* PH7_TKWRD_ENDIF */` |
|       - |  713 | `       /* PH7_TKWRD_FUNCTION */` |
|       - |  714 | `       /* PH7_TKWRD_DIE */` |
|       - |  715 | `       /* PH7_TKWRD_ENDWHILE */` |
|       - |  716 | `       /* PH7_TKWRD_WHILE */` |
|       - |  717 | `       /* PH7_TKWRD_EVAL */` |
|       - |  718 | `       /* PH7_TKWRD_DO */` |
|       - |  719 | `       /* PH7_TKWRD_EXIT */` |
|       - |  720 | `       /* PH7_TKWRD_GOTO */` |
|       - |  721 | `       /* PH7_TKWRD_IMPLEMENTS */` |
|       - |  722 | `       /* PH7_TKWRD_INCONCE */` |
|       - |  723 | `       /* PH7_TKWRD_INCLUDE */` |
|       - |  724 | `       /* PH7_TKWRD_EMPTY */` |
|       - |  725 | `       /* PH7_TKWRD_INSTANCEOF */` |
|       - |  726 | `       /* PH7_TKWRD_INTERFACE */` |
|       - |  727 | `       /* PH7_TKWRD_INT */` |
|       - |  728 | `       /* PH7_TKWRD_ENDFOR */` |
|       - |  729 | `       /* PH7_TKWRD_END4EACH */` |
|       - |  730 | `       /* PH7_TKWRD_FOR */` |
|       - |  731 | `       /* PH7_TKWRD_FOREACH */` |
|       - |  732 | `       /* PH7_TKWRD_OR */` |
|       - |  733 | `       /* PH7_TKWRD_ISSET */` |
|       - |  734 | `       /* PH7_TKWRD_PARENT */` |
|       - |  735 | `       /* PH7_TKWRD_PRIVATE */` |
|       - |  736 | `       /* PH7_TKWRD_PROTECTED */` |
|       - |  737 | `       /* PH7_TKWRD_PUBLIC */` |
|       - |  738 | `       /* PH7_TKWRD_CATCH */` |
|       - |  739 | `       /* PH7_TKWRD_UNSET */` |
|       - |  740 | `       /* PH7_TKWRD_XOR */` |
|       - |  741 | `       /* PH7_TKWRD_ARRAY */` |
|       - |  742 | `       /* PH7_TKWRD_AS */` |
|       - |  743 | `       /* PH7_TKWRD_ARRAY */` |
|       - |  744 | `       /* PH7_TKWRD_EXIT */` |
|       - |  745 | `       /* PH7_TKWRD_UNSET */` |
|       - |  746 | `       /* PH7_TKWRD_XOR */` |
|       - |  747 | `       /* PH7_TKWRD_OR */` |
|       - |  748 | `       /* PH7_TKWRD_BREAK */` |
|  388882 |  749 | `      return aCode[i];` |
|       - |  750 | `    }` |
|  297895 |  751 | `  }` |
|  766008 |  752 | `  return PH7_TK_ID;` |
|  602887 |  753 |  |
|       - |  754 | `/* --- End of Automatically generated code --- */` |
|       - |  755 | `/*` |
|       - |  756 | ` * Extract a heredoc/nowdoc text from a raw PHP input.` |
|       - |  757 | ` * According to the PHP language reference manual:` |
|       - |  758 | ` *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier` |
|       - |  759 | ` *  is provided, then a newline. The string itself follows, and then the same identifier again` |
|       - |  760 | ` *  to close the quotation.` |
|       - |  761 | ` *  The closing identifier must begin in the first column of the line. Also, the identifier must` |
|       - |  762 | ` *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric` |
|       - |  763 | ` *  characters and underscores, and must start with a non-digit character or underscore.` |
|       - |  764 | ` *  Heredoc text behaves just like a double-quoted string, without the double quotes.` |
|       - |  765 | ` *  This means that quotes in a heredoc do not need to be escaped, but the escape codes listed` |
|       - |  766 | ` *  above can still be used. Variables are expanded, but the same care must be taken when expressing` |
|       - |  767 | ` *  complex variables inside a heredoc as with strings.` |
|       - |  768 | ` *  Nowdocs are to single-quoted strings what heredocs are to double-quoted strings.` |
|       - |  769 | ` *  A nowdoc is specified similarly to a heredoc, but no parsing is done inside a nowdoc.` |
|       - |  770 | ` *  The construct is ideal for embedding PHP code or other large blocks of text without the need` |
|       - |  771 | ` *  for escaping. It shares some features in common with the SGML <![CDATA[ ]]> construct, in that` |
|       - |  772 | ` *  it declares a block of text which is not for parsing.` |
|       - |  773 | ` *  A nowdoc is identified with the same <<< sequence used for heredocs, but the identifier which follows` |
|       - |  774 | ` *  is enclosed in single quotes, e.g. <<<'EOT'. All the rules for heredoc identifiers also apply to nowdoc` |
|       - |  775 | ` *  identifiers, especially those regarding the appearance of the closing identifier.` |
|       - |  776 | ` * Symisc Extension:` |
|       - |  777 | ` * The closing delimiter can now start with a digit or undersocre or it can be an UTF-8 stream.` |
|       - |  778 | ` * Example:` |
|       - |  779 | ` *  <<<123` |
|       - |  780 | ` *    HEREDOC Here` |
|       - |  781 | ` * 123` |
|       - |  782 | ` *  or` |
|       - |  783 | ` *  <<<___` |
|       - |  784 | ` *   HEREDOC Here` |
|       - |  785 | ` *  ___` |
|       - |  786 | ` */` |
|      56 |  787 | `static sxi32 LexExtractHeredoc(SyStream *pStream,SyToken *pToken)` |
|       2 |  788 |  |
|      58 |  789 | `	const unsigned char *zIn  = pStream->zText;` |
|      58 |  790 | `	const unsigned char *zEnd = pStream->zEnd;` |
|       - |  791 | `	const unsigned char *zPtr;` |
|      58 |  792 | `	sxu8 bNowDoc = FALSE;` |
|       - |  793 | `	SyString sDelim;` |
|       - |  794 | `	SyString sStr;` |
|       - |  795 | `	/* Jump leading white spaces */` |
|      70 |  796 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|      13 |  797 | `		zIn++;` |
|       1 |  798 | `	}` |
|      58 |  799 | `	if( zIn >= zEnd ){` |
|       - |  800 | `		/* A simple symbol,return immediately */` |
|     ! 0 |  801 | `		return SXERR_CONTINUE;` |
|       - |  802 | `	}` |
|      58 |  803 | `	if( zIn[0] == '\'' \|\| zIn[0] == '"' ){` |
|       - |  804 | `		/* Make sure we are dealing with a nowdoc */` |
|      29 |  805 | `		bNowDoc =  zIn[0] == '\'' ? TRUE : FALSE;` |
|      29 |  806 | `		zIn++;` |
|      14 |  807 | `	}` |
|      58 |  808 | `	if( zIn[0] < 0xc0 && !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       - |  809 | `		/* Invalid delimiter,return immediately */` |
|     ! 0 |  810 | `		return SXERR_CONTINUE;` |
|       - |  811 | `	}` |
|       - |  812 | `	/* Isolate the identifier */` |
|      58 |  813 | `	sDelim.zString = (const char *)zIn;` |
|      64 |  814 | `	for(;;){` |
|     130 |  815 | `		zPtr = zIn;` |
|       - |  816 | `		/* Skip alphanumeric stream */` |
|     424 |  817 | `		while( zPtr < zEnd && zPtr[0] < 0xc0 && (SyisAlphaNum(zPtr[0]) \|\| zPtr[0] == '_') ){` |
|     232 |  818 | `			zPtr++;` |
|       2 |  819 | `		}` |
|     130 |  820 | `		if( zPtr < zEnd && zPtr[0] >= 0xc0 ){` |
|      19 |  821 | `			zPtr++;` |
|       - |  822 | `			/* UTF-8 stream */` |
|      37 |  823 | `			while( zPtr < zEnd && ((zPtr[0] & 0xc0) == 0x80) ){` |
|      19 |  824 | `				zPtr++;` |
|       1 |  825 | `			}` |
|       9 |  826 | `		}` |
|     130 |  827 | `		if( zPtr == zIn ){` |
|       - |  828 | `			/* Not an UTF-8 or alphanumeric stream */` |
|      58 |  829 | `			break;` |
|       - |  830 | `		}` |
|       - |  831 | `		/* Synchronize pointers */` |
|      74 |  832 | `		zIn = zPtr;` |
|       2 |  833 | `	}` |
|       - |  834 | `	/* Get the identifier length */` |
|      58 |  835 | `	sDelim.nByte = (sxu32)((const char *)zIn-sDelim.zString);` |
|      58 |  836 | `	if( zIn[0] == '"' \|\| (bNowDoc && zIn[0] == '\'') ){` |
|       - |  837 | `		/* Jump the trailing single quote */` |
|      29 |  838 | `		zIn++;` |
|      14 |  839 | `	}` |
|       - |  840 | `	/* Jump trailing white spaces */` |
|      58 |  841 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|     ! 0 |  842 | `		zIn++;` |
|     ! 0 |  843 | `	}` |
|      58 |  844 | `	if( sDelim.nByte <= 0 \|\| zIn >= zEnd \|\| zIn[0] != '\n' ){` |
|       - |  845 | `		/* Invalid syntax */` |
|     ! 0 |  846 | `		return SXERR_CONTINUE;` |
|       - |  847 | `	}` |
|      58 |  848 | `	pStream->nLine++; /* Increment line counter */` |
|      58 |  849 | `	zIn++;` |
|       - |  850 | `	/* Isolate the delimited string */` |
|      58 |  851 | `	sStr.zString = (const char *)zIn;` |
|       - |  852 | `	/* Go and found the closing delimiter */` |
|      75 |  853 | `	for(;;){` |
|       - |  854 | `		/* Synchronize with the next line */` |
|    3018 |  855 | `		while( zIn < zEnd && zIn[0] != '\n' ){` |
|    2868 |  856 | `			zIn++;` |
|       2 |  857 | `		}` |
|     152 |  858 | `		if( zIn >= zEnd ){` |
|       - |  859 | `			/* End of the input reached, break immediately */` |
|      12 |  860 | `			pStream->zText = pStream->zEnd;` |
|      12 |  861 | `			break;` |
|       - |  862 | `		}` |
|     142 |  863 | `		pStream->nLine++; /* Increment line counter */` |
|     142 |  864 | `		zIn++;` |
|     142 |  865 | `		if( (sxu32)(zEnd - zIn) >= sDelim.nByte && SyMemcmp((const void *)sDelim.zString,(const void *)zIn,sDelim.nByte) == 0 ){` |
|      50 |  866 | `			zPtr = &zIn[sDelim.nByte];` |
|      62 |  867 | `			while( zPtr < zEnd && zPtr[0] < 0xc0 && SyisSpace(zPtr[0]) && zPtr[0] != '\n' ){` |
|      13 |  868 | `				zPtr++;` |
|       1 |  869 | `			}` |
|      50 |  870 | `			if( zPtr >= zEnd ){` |
|       - |  871 | `				/* End of input */` |
|     ! 0 |  872 | `				pStream->zText = zPtr;` |
|     ! 0 |  873 | `				break;` |
|       - |  874 | `			}` |
|      50 |  875 | `			if( zPtr[0] == ';' ){` |
|      50 |  876 | `				const unsigned char *zCur = zPtr;` |
|      50 |  877 | `				zPtr++;` |
|      52 |  878 | `				while( zPtr < zEnd && zPtr[0] < 0xc0 && SyisSpace(zPtr[0]) && zPtr[0] != '\n' ){` |
|       3 |  879 | `					zPtr++;` |
|       1 |  880 | `				}` |
|      50 |  881 | `				if( zPtr >= zEnd \|\| zPtr[0] == '\n' ){` |
|       - |  882 | `					/* Closing delimiter found,break immediately */` |
|      48 |  883 | `					pStream->zText = zCur; /* Keep the semi-colon */` |
|      48 |  884 | `					break;` |
|       1 |  885 | `				}` |
|       1 |  886 | `			}else if( zPtr[0] == '\n' ){` |
|       - |  887 | `				/* Closing delimiter found,break immediately */` |
|     ! 0 |  888 | `				pStream->zText = zPtr; /* Synchronize with the stream cursor */` |
|     ! 0 |  889 | `				break;` |
|       - |  890 | `			}` |
|       - |  891 | `			/* Synchronize pointers and continue searching */` |
|       3 |  892 | `			zIn = zPtr;` |
|       1 |  893 | `		}` |
|       2 |  894 | `	} /* For(;;) */` |
|       - |  895 | `	/* Get the delimited string length */` |
|      58 |  896 | `	sStr.nByte = (sxu32)((const char *)zIn-sStr.zString);` |
|       - |  897 | `	/* Record token type and length */` |
|      58 |  898 | `	pToken->nType = bNowDoc ? PH7_TK_NOWDOC : PH7_TK_HEREDOC;` |
|      58 |  899 | `	SyStringDupPtr(&pToken->sData,&sStr);` |
|       - |  900 | `	/* Remove trailing white spaces */` |
|     104 |  901 | `	SyStringRightTrim(&pToken->sData);` |
|       - |  902 | `	/* All done */` |
|      58 |  903 | `	return SXRET_OK;` |
|      30 |  904 |  |
|       - |  905 | `/*` |
|       - |  906 | ` * Tokenize a raw PHP input.` |
|       - |  907 | ` * This is the public tokenizer called by most code generator routines.` |
|       - |  908 | ` */` |
|   10164 |  909 | `PH7_PRIVATE sxi32 PH7_TokenizePHP(const char *zInput,sxu32 nLen,sxu32 nLineStart,SySet *pOut)` |
|       2 |  910 |  |
|       - |  911 | `	SyLex sLexer;` |
|       - |  912 | `	sxi32 rc;` |
|       - |  913 | `	/* Initialize the lexer */` |
|   10166 |  914 | `	rc = SyLexInit(&sLexer,&(*pOut),TokenizePHP,0);` |
|   10166 |  915 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  916 | `		return rc;` |
|       - |  917 | `	}` |
|   10166 |  918 | `	sLexer.sStream.nLine = nLineStart;` |
|       - |  919 | `	/* Tokenize input */` |
|   10166 |  920 | `	rc = SyLexTokenizeInput(&sLexer,zInput,nLen,0,0,0);` |
|       - |  921 | `	/* Release the lexer */` |
|   10166 |  922 | `	SyLexRelease(&sLexer);` |
|       - |  923 | `	/* Tokenization result */` |
|   10166 |  924 | `	return rc;` |
|    5084 |  925 |  |
|       - |  926 | `/*` |
|       - |  927 | ` * High level public tokenizer.` |
|       - |  928 | ` *  Tokenize the input into PHP tokens and raw tokens [i.e: HTML,XML,Raw text...].` |
|       - |  929 | ` * According to the PHP language reference manual` |
|       - |  930 | ` *   When PHP parses a file, it looks for opening and closing tags, which tell PHP` |
|       - |  931 | ` *   to start and stop interpreting the code between them. Parsing in this manner allows` |
|       - |  932 | ` *   PHP to be embedded in all sorts of different documents, as everything outside of a pair` |
|       - |  933 | ` *   of opening and closing tags is ignored by the PHP parser. Most of the time you will see` |
|       - |  934 | ` *   PHP embedded in HTML documents, as in this example.` |
|       - |  935 | ` *   <?php echo 'While this is going to be parsed.'; ?>` |
|       - |  936 | ` *   <p>This will also be ignored.</p>` |
|       - |  937 | ` *   You can also use more advanced structures:` |
|       - |  938 | ` *   Example #1 Advanced escaping` |
|       - |  939 | ` * <?php` |
|       - |  940 | ` * if ($expression) {` |
|       - |  941 | ` *   ?>` |
|       - |  942 | ` *   <strong>This is true.</strong>` |
|       - |  943 | ` *   <?php` |
|       - |  944 | ` * } else {` |
|       - |  945 | ` *   ?>` |
|       - |  946 | ` *   <strong>This is false.</strong>` |
|       - |  947 | ` *   <?php` |
|       - |  948 | ` * }` |
|       - |  949 | ` * ?>` |
|       - |  950 | ` * This works as expected, because when PHP hits the ?> closing tags, it simply starts outputting` |
|       - |  951 | ` * whatever it finds (except for an immediately following newline - see instruction separation ) until it hits` |
|       - |  952 | ` * another opening tag. The example given here is contrived, of course, but for outputting large blocks of text` |
|       - |  953 | ` * dropping out of PHP parsing mode is generally more efficient than sending all of the text through echo() or print().` |
|       - |  954 | ` * There are four different pairs of opening and closing tags which can be used in PHP. Three of those, <?php ?>` |
|       - |  955 | ` * <script language="php"> </script>  and <? ?> are always available. The other two are short tags and ASP style` |
|       - |  956 | ` * tags, and can be turned on and off from the php.ini configuration file. As such, while some people find short tags` |
|       - |  957 | ` * and ASP style tags convenient, they are less portable, and generally not recommended.` |
|       - |  958 | ` * Note:` |
|       - |  959 | ` * Also note that if you are embedding PHP within XML or XHTML you will need to use the <?php ?> tags to remain` |
|       - |  960 | ` * compliant with standards.` |
|       - |  961 | ` * Example #2 PHP Opening and Closing Tags` |
|       - |  962 | ` * 1.  <?php echo 'if you want to serve XHTML or XML documents, do it like this'; ?>` |
|       - |  963 | ` * 2.  <script language="php">` |
|       - |  964 | ` *       echo 'some editors (like FrontPage) don\'t` |
|       - |  965 | ` *             like processing instructions';` |
|       - |  966 | ` *   </script>` |
|       - |  967 | ` *` |
|       - |  968 | ` * 3.  <? echo 'this is the simplest, an SGML processing instruction'; ?>` |
|       - |  969 | ` *   <?= expression ?> This is a shortcut for "<? echo expression ?>"` |
|       - |  970 | ` */` |
|    8572 |  971 | `PH7_PRIVATE sxi32 PH7_TokenizeRawText(const char *zInput,sxu32 nLen,SySet *pOut)` |
|       2 |  972 |  |
|    8574 |  973 | `	const char *zEnd = &zInput[nLen];` |
|    8574 |  974 | `	const char *zIn  = zInput;` |
|       - |  975 | `	const char *zCur,*zCurEnd;` |
|    8574 |  976 | `	SyString sCtag = { 0, 0 };     /* Closing tag */` |
|       - |  977 | `	SyToken sToken;` |
|       - |  978 | `	SyString sDoc;` |
|       - |  979 | `	sxu32 nLine;` |
|       - |  980 | `	sxi32 iNest;` |
|       - |  981 | `	sxi32 rc;` |
|       - |  982 | `	/* Tokenize the input into PHP tokens and raw tokens */` |
|    8574 |  983 | `	nLine = 1;` |
|    8574 |  984 | `	zCur = zCurEnd   = 0; /* Prevent compiler warning */` |
|    8574 |  985 | `	sToken.pUserData = 0;` |
|    8574 |  986 | `	iNest = 0;` |
|    8574 |  987 | `	sDoc.nByte = 0;` |
|    8574 |  988 | `	sDoc.zString = ""; /* cc warning */` |
|    8574 |  989 | `	for(;;){` |
|   17150 |  990 | `		if( zIn >= zEnd ){` |
|       - |  991 | `			/* End of input reached */` |
|    8570 |  992 | `			break;` |
|       - |  993 | `		}` |
|    8582 |  994 | `		sToken.nLine = nLine;` |
|    8582 |  995 | `		zCur = zIn;` |
|    8582 |  996 | `		zCurEnd = 0;` |
|    8590 |  997 | `		while( zIn < zEnd ){` |
|    8586 |  998 | `			 if( zIn[0] == '<' ){` |
|    8578 |  999 | `				const char *zTmp = zIn; /* End of raw input marker */` |
|    8578 | 1000 | `				zIn++;` |
|    8578 | 1001 | `				if( zIn < zEnd ){` |
|    8578 | 1002 | `					if( zIn[0] == '?' ){` |
|    8578 | 1003 | `						zIn++;` |
|    8578 | 1004 | `						if( (sxu32)(zEnd - zIn) >= sizeof("php")-1 &&  SyStrnicmp(zIn,"php",sizeof("php")-1) == 0 ){` |
|       - | 1005 | `							/* opening tag: <?php */` |
|    8576 | 1006 | `							zIn += sizeof("php")-1;` |
|    4287 | 1007 | `						}` |
|       - | 1008 | `						/* Look for the closing tag '?>' */` |
|    8578 | 1009 | `						SyStringInitFromBuf(&sCtag,"?>",sizeof("?>")-1);` |
|    8578 | 1010 | `						zCurEnd = zTmp;` |
|    8578 | 1011 | `						break;` |
|       - | 1012 | `					}` |
|     ! 0 | 1013 | `				}` |
|     ! 0 | 1014 | `			}else{` |
|      10 | 1015 | `				if( zIn[0] == '\n' ){` |
|      10 | 1016 | `					nLine++;` |
|       4 | 1017 | `				}` |
|      10 | 1018 | `				zIn++;` |
|       - | 1019 | `			 }` |
|       2 | 1020 | `		} /* While(zIn < zEnd) */` |
|    8582 | 1021 | `		if( zCurEnd == 0 ){` |
|       5 | 1022 | `			zCurEnd = zIn;` |
|       2 | 1023 | `		}` |
|       - | 1024 | `		/* Save the raw token */` |
|    8582 | 1025 | `		SyStringInitFromBuf(&sToken.sData,zCur,zCurEnd - zCur);` |
|    8582 | 1026 | `		sToken.nType = PH7_TOKEN_RAW;` |
|    8582 | 1027 | `		rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|    8582 | 1028 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1029 | `			return rc;` |
|       - | 1030 | `		}` |
|    8582 | 1031 | `		if( zIn >= zEnd ){` |
|       5 | 1032 | `			break;` |
|       - | 1033 | `		}` |
|       - | 1034 | `		/* Ignore leading white space */` |
|   18626 | 1035 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|   10050 | 1036 | `			if( zIn[0] == '\n' ){` |
|    8880 | 1037 | `				nLine++;` |
|    4439 | 1038 | `			}` |
|   10050 | 1039 | `			zIn++;` |
|       2 | 1040 | `		}` |
|       - | 1041 | `		/* Delimit the PHP chunk */` |
|    8578 | 1042 | `		sToken.nLine = nLine;` |
|    8578 | 1043 | `		zCur = zIn;` |
|  811108 | 1044 | `		while( (sxu32)(zEnd - zIn) >= sCtag.nByte ){` |
|       - | 1045 | `			const char *zPtr;` |
|  807568 | 1046 | `			if( SyMemcmp(zIn,sCtag.zString,sCtag.nByte) == 0 && iNest < 1 ){` |
|    5038 | 1047 | `				break;` |
|       - | 1048 | `			}` |
|  403172 | 1049 | `			for(;;){` |
|  806346 | 1050 | `				if( zIn[0] != '/' \|\| (zIn[1] != '*' && zIn[1] != '/') /* && sCtag.nByte >= 2 */ ){` |
|  401267 | 1051 | `					break;` |
|       - | 1052 | `				}` |
|    3816 | 1053 | `				zIn += 2;` |
|    3816 | 1054 | `				if( zIn[-1] == '/' ){` |
|       - | 1055 | `					/* Inline comment */` |
|  129530 | 1056 | `					while( zIn < zEnd && zIn[0] != '\n' ){` |
|  125796 | 1057 | `						zIn++;` |
|       2 | 1058 | `					}` |
|    3736 | 1059 | `					if( zIn >= zEnd ){` |
|     ! 0 | 1060 | `						zIn--;` |
|     ! 0 | 1061 | `					}` |
|    1869 | 1062 | `				}else{` |
|       - | 1063 | `					/* Block comment */` |
|    4400 | 1064 | `					while( (sxu32)(zEnd-zIn) >= sizeof("*/") - 1 ){` |
|    4400 | 1065 | `						if( zIn[0] == '*' && zIn[1] == '/' ){` |
|      82 | 1066 | `							zIn += 2;` |
|      82 | 1067 | `							break;` |
|       - | 1068 | `						}` |
|    4320 | 1069 | `						if( zIn[0] == '\n' ){` |
|      28 | 1070 | `							nLine++;` |
|      13 | 1071 | `						}` |
|    4320 | 1072 | `						zIn++;` |
|       2 | 1073 | `					}` |
|       - | 1074 | `				}` |
|       2 | 1075 | `			}` |
|  802532 | 1076 | `			if( zIn[0] == '\n' ){` |
|   27540 | 1077 | `				nLine++;` |
|   27540 | 1078 | `				if( iNest > 0 ){` |
|     156 | 1079 | `					zIn++;` |
|     156 | 1080 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|     ! 0 | 1081 | `						zIn++;` |
|     ! 0 | 1082 | `					}` |
|     156 | 1083 | `					zPtr = zIn;` |
|     864 | 1084 | `					while( zIn < zEnd ){` |
|     864 | 1085 | `						if( (unsigned char)zIn[0] >= 0xc0 ){` |
|       - | 1086 | `							/* UTF-8 stream */` |
|      19 | 1087 | `							zIn++;` |
|      37 | 1088 | `							SX_JMP_UTF8(zIn,zEnd);` |
|     854 | 1089 | `						}else if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|      79 | 1090 | `							break;` |
|     ! 0 | 1091 | `						}else{` |
|     692 | 1092 | `							zIn++;` |
|       - | 1093 | `						}` |
|       2 | 1094 | `					}` |
|     156 | 1095 | `					if( (sxu32)(zIn - zPtr) == sDoc.nByte && SyMemcmp(sDoc.zString,zPtr,sDoc.nByte) == 0 ){` |
|      60 | 1096 | `						iNest = 0;` |
|      29 | 1097 | `					}` |
|     156 | 1098 | `					continue;` |
|       2 | 1099 | `				}` |
|  788686 | 1100 | `			}else if ( (sxu32)(zEnd - zIn) >= sizeof("<<<") && zIn[0] == '<' && zIn[1] == '<' && zIn[2] == '<' && iNest < 1){` |
|      62 | 1101 | `				zIn += sizeof("<<<")-1;` |
|      74 | 1102 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|      13 | 1103 | `					zIn++;` |
|       1 | 1104 | `				}` |
|      62 | 1105 | `				if( zIn[0] == '"' \|\| zIn[0] == '\'' ){` |
|      32 | 1106 | `					zIn++;` |
|      15 | 1107 | `				}` |
|      62 | 1108 | `				zPtr = zIn;` |
|     330 | 1109 | `				while( zIn < zEnd ){` |
|     330 | 1110 | `					if( (unsigned char)zIn[0] >= 0xc0 ){` |
|       - | 1111 | `						/* UTF-8 stream */` |
|      19 | 1112 | `						zIn++;` |
|      37 | 1113 | `						SX_JMP_UTF8(zIn,zEnd);` |
|     320 | 1114 | `					}else if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|      32 | 1115 | `						break;` |
|     ! 0 | 1116 | `					}else{` |
|     252 | 1117 | `						zIn++;` |
|       - | 1118 | `					}` |
|       2 | 1119 | `				}` |
|      62 | 1120 | `				SyStringInitFromBuf(&sDoc,zPtr,zIn-zPtr);` |
|      62 | 1121 | `				SyStringFullTrim(&sDoc);` |
|      62 | 1122 | `				if( sDoc.nByte > 0 ){` |
|      62 | 1123 | `					iNest++;` |
|      30 | 1124 | `				}` |
|      62 | 1125 | `				continue;` |
|       - | 1126 | `			}` |
|  802318 | 1127 | `			zIn++;` |
|       - | 1128 |  |
|  802318 | 1129 | `			if ( zIn >= zEnd )` |
|     ! 0 | 1130 | `				break;` |
|       2 | 1131 | `		}` |
|    8578 | 1132 | `		if( (sxu32)(zEnd - zIn) < sCtag.nByte ){` |
|    3542 | 1133 | `			zIn = zEnd;` |
|    1770 | 1134 | `		}` |
|    8578 | 1135 | `		if( zCur < zIn ){` |
|       - | 1136 | `			/* Save the PHP chunk for later processing */` |
|    7128 | 1137 | `			sToken.nType = PH7_TOKEN_PHP;` |
|    7128 | 1138 | `			SyStringInitFromBuf(&sToken.sData,zCur,zIn-zCur);` |
|   14248 | 1139 | `			SyStringRightTrim(&sToken.sData); /* Trim trailing white spaces */` |
|    7128 | 1140 | `			rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|    7128 | 1141 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 1142 | `				return rc;` |
|       - | 1143 | `			}` |
|    3563 | 1144 | `		}` |
|    8578 | 1145 | `		if( zIn < zEnd ){` |
|       - | 1146 | `			/* Jump the trailing closing tag */` |
|    5038 | 1147 | `			zIn += sCtag.nByte;` |
|    2518 | 1148 | `		}` |
|       2 | 1149 | `	} /* For(;;) */` |
|       - | 1150 |  |
|    8574 | 1151 | ` 	return SXRET_OK;` |
|    4288 | 1152 |  |
|       - | 1153 |  |
