# src/ph7/lex.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 708/743 lines (95.29%)

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
|  6957080 |   19 | `static sxi32 TokenizePHP(SyStream *pStream,SyToken *pToken,void *pUserData,void *pCtxData)` |
|        2 |   20 |  |
|        - |   21 | `	SyString *pStr;` |
|        - |   22 | `	sxi32 rc;` |
|        - |   23 | `	/* Ignore leading white spaces */` |
| 10480200 |   24 | `	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){` |
|        - |   25 | `		/* Advance the stream cursor */` |
|  3523120 |   26 | `		if( pStream->zText[0] == '\n' ){` |
|        - |   27 | `			/* Update line counter */` |
|    29934 |   28 | `			pStream->nLine++;` |
|    14966 |   29 | `		}` |
|  3523120 |   30 | `		pStream->zText++;` |
|        2 |   31 | `	}` |
|  6957082 |   32 | `	if( pStream->zText >= pStream->zEnd ){` |
|        - |   33 | `		/* End of input reached */` |
|      ! 0 |   34 | `		return SXERR_EOF;` |
|        - |   35 | `	}` |
|        - |   36 | `	/* Record token starting position and line */` |
|  6957082 |   37 | `	pToken->nLine = pStream->nLine;` |
|  6957082 |   38 | `	pToken->pUserData = 0;` |
|  6957082 |   39 | `	pStr = &pToken->sData;` |
|  6957082 |   40 | `	SyStringInitFromBuf(pStr,pStream->zText,0);` |
|  8213807 |   41 | `	if( pStream->zText[0] >= 0xc0 \|\| SyisAlpha(pStream->zText[0]) \|\| pStream->zText[0] == '_' ){` |
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
|  2513452 |   52 | `		if( pStream->zText[0] < 0xc0 ){` |
|  2513436 |   53 | `			pStream->zText++;` |
|  1256717 |   54 | `		}` |
|  2468001 |   55 | `		for(;;){` |
|  4936004 |   56 | `			zIn = pStream->zText;` |
|  4936004 |   57 | `			if( zIn[0] >= 0xc0 ){` |
|       49 |   58 | `				zIn++;` |
|        - |   59 | `				/* UTF-8 stream */` |
|      109 |   60 | `				while( zIn < pStream->zEnd && ((zIn[0] & 0xc0) == 0x80) ){` |
|       61 |   61 | `					zIn++;` |
|        1 |   62 | `				}` |
|       24 |   63 | `			}` |
|        - |   64 | `			/* Skip alphanumeric stream */` |
| 20258327 |   65 | `			while( zIn < pStream->zEnd && zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_') ){` |
| 12854324 |   66 | `				zIn++;` |
|        2 |   67 | `			}` |
|  4936004 |   68 | `			if( zIn == pStream->zText ){` |
|        - |   69 | `				/* Not an UTF-8 or alphanumeric stream */` |
|  2513452 |   70 | `				break;` |
|        - |   71 | `			}` |
|        - |   72 | `			/* Synchronize pointers */` |
|  2422554 |   73 | `			pStream->zText = zIn;` |
|        2 |   74 | `		}` |
|        - |   75 | `		/* Record token length */` |
|  2513452 |   76 | `		pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  2513452 |   77 | `		nKeyword = KeywordCode(pStr->zString,(int)pStr->nByte);` |
|  2513452 |   78 | `		if( nKeyword != PH7_TK_ID ){` |
|   858938 |   79 | `			if( nKeyword &` |
|        - |   80 | `				(PH7_TKWRD_NEW\|PH7_TKWRD_CLONE\|PH7_TKWRD_AND\|PH7_TKWRD_XOR\|PH7_TKWRD_OR\|PH7_TKWRD_INSTANCEOF\|PH7_TKWRD_SEQ\|PH7_TKWRD_SNE) ){` |
|        - |   81 | `					/* Alpha stream operators [i.e: new,clone,and,instanceof,eq,ne,or,xor],save the operator instance for later processing */` |
|    14078 |   82 | `					pToken->pUserData = (void *)PH7_ExprExtractOperator(pStr,0);` |
|        - |   83 | `					/* Mark as an operator */` |
|    14078 |   84 | `					pToken->nType = PH7_TK_ID\|PH7_TK_OP;` |
|     7040 |   85 | `			}else{` |
|        - |   86 | `				/* We are dealing with a keyword [i.e: while,foreach,class...],save the keyword ID */` |
|   844862 |   87 | `				pToken->nType = PH7_TK_KEYWORD;` |
|   844862 |   88 | `				pToken->pUserData = SX_INT_TO_PTR(nKeyword);` |
|        - |   89 | `			}` |
|   429470 |   90 | `		}else{` |
|        - |   91 | `			/* A simple identifier */` |
|  1654516 |   92 | `			pToken->nType = PH7_TK_ID;` |
|        - |   93 | `		}` |
|  1256727 |   94 | `	}else{` |
|        - |   95 | `		sxi32 c;` |
|        - |   96 | `		/* Non-alpha stream */` |
|  4477809 |   97 | `		if( pStream->zText[0] == '#' \|\|` |
|  4443630 |   98 | `			( pStream->zText[0] == '/' &&  &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '/') ){` |
|     3794 |   99 | `				pStream->zText++;` |
|        - |  100 | `				/* Inline comments */` |
|   137496 |  101 | `				while( pStream->zText < pStream->zEnd && pStream->zText[0] != '\n' ){` |
|   133704 |  102 | `					pStream->zText++;` |
|        2 |  103 | `				}` |
|        - |  104 | `				/* Tell the upper-layer to ignore this token */` |
|     3794 |  105 | `				return SXERR_CONTINUE;` |
|  4439840 |  106 | `		}else if( pStream->zText[0] == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '*' ){` |
|    64500 |  107 | `			pStream->zText += 2;` |
|        - |  108 | `			/* Block comment */` |
|  1829620 |  109 | `			while( pStream->zText < pStream->zEnd ){` |
|  1829620 |  110 | `				if( pStream->zText[0] == '*' ){` |
|    64526 |  111 | `					if( &pStream->zText[1] >= pStream->zEnd \|\| pStream->zText[1] == '/'  ){` |
|    32251 |  112 | `						break;` |
|        - |  113 | `					}` |
|       13 |  114 | `				}` |
|  1765122 |  115 | `				if( pStream->zText[0] == '\n' ){` |
|       28 |  116 | `					pStream->nLine++;` |
|       13 |  117 | `				}` |
|  1765122 |  118 | `				pStream->zText++;` |
|        2 |  119 | `			}` |
|    64500 |  120 | `			pStream->zText += 2;` |
|        - |  121 | `			/* Tell the upper-layer to ignore this token */` |
|    64500 |  122 | `			return SXERR_CONTINUE;` |
|  4375342 |  123 | `		}else if( SyisDigit(pStream->zText[0]) ){` |
|    89126 |  124 | `			pStream->zText++;` |
|        - |  125 | `			/* PHP 7.4: handle underscore separator immediately following the first digit.` |
|        - |  126 | `			 * Check pStream->zText < pStream->zEnd BEFORE forming pStream->zText + 1 so` |
|        - |  127 | `			 * we never compute a pointer past one-past-end. */` |
|    89204 |  128 | `			if( pStream->zText < pStream->zEnd` |
|    89124 |  129 | `				&& pStream->zText[0] == '_'` |
|    44642 |  130 | `				&& pStream->zText + 1 < pStream->zEnd` |
|      160 |  131 | `				&& pStream->zText[1] < 0xc0` |
|      162 |  132 | `				&& SyisDigit(pStream->zText[1]) ){` |
|      151 |  133 | `				pStream->zText++; /* swallow underscore between two digits */` |
|       75 |  134 | `			}` |
|        - |  135 | `			/* Decimal digit stream (PHP 7.4: underscore separator allowed between two digits) */` |
|    98498 |  136 | `			while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|     9374 |  137 | `				pStream->zText++;` |
|     9458 |  138 | `				if( pStream->zText < pStream->zEnd` |
|     9372 |  139 | `					&& pStream->zText[0] == '_'` |
|     4772 |  140 | `					&& pStream->zText + 1 < pStream->zEnd` |
|      172 |  141 | `					&& pStream->zText[1] < 0xc0` |
|      174 |  142 | `					&& SyisDigit(pStream->zText[1]) ){` |
|      173 |  143 | `					pStream->zText++; /* swallow underscore between two digits */` |
|       86 |  144 | `				}` |
|        2 |  145 | `			}` |
|        - |  146 | `			/* Mark the token as integer until we encounter a real number */` |
|    89126 |  147 | `			pToken->nType = PH7_TK_INTEGER;` |
|    89126 |  148 | `			if( pStream->zText < pStream->zEnd ){` |
|    89126 |  149 | `				c = pStream->zText[0];` |
|    89126 |  150 | `				if( c == '.' ){` |
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
|    88903 |  187 | `				}else if( c=='e' \|\| c=='E' ){` |
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
|    88666 |  209 | `				}else if( c == 'x' \|\| c == 'X' ){` |
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
|    88616 |  222 | `				}else if(c  == 'b' \|\| c == 'B' ){` |
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
|    44562 |  235 | `			}` |
|        - |  236 | `			/* PHP 7.4: absorb a trailing malformed underscore run into the` |
|        - |  237 | `			 * numeric token so the compile phase can emit a PHP-compatible` |
|        - |  238 | `			 * "syntax error, unexpected identifier" parse error. Valid` |
|        - |  239 | `			 * separators were already consumed by the per-loop peek logic` |
|        - |  240 | `			 * above, so an underscore here is always misplaced. */` |
|    89126 |  241 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '_' ){` |
|       15 |  242 | `				pStream->zText++;` |
|       44 |  243 | `				while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0` |
|       46 |  244 | `					&& (SyisAlphaNum(pStream->zText[0]) \|\| pStream->zText[0] == '_') ){` |
|       17 |  245 | `					pStream->zText++;` |
|        1 |  246 | `				}` |
|        7 |  247 | `			}` |
|        - |  248 | `			/* Record token length */` |
|    89126 |  249 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|    89126 |  250 | `			return SXRET_OK;` |
|        - |  251 | `		}` |
|  4286218 |  252 | `		c = pStream->zText[0];` |
|  4286218 |  253 | `		pStream->zText++; /* Advance the stream cursor */` |
|        - |  254 | `		/* Assume we are dealing with an operator*/` |
|  4286218 |  255 | `		pToken->nType = PH7_TK_OP;` |
|  4286218 |  256 | `		switch(c){` |
|   898886 |  257 | `		case '$': pToken->nType = PH7_TK_DOLLAR; break;` |
|   340654 |  258 | `		case '{': pToken->nType = PH7_TK_OCB;    break;` |
|   340640 |  259 | `		case '}': pToken->nType = PH7_TK_CCB;    break;` |
|   677984 |  260 | `		case '(': pToken->nType = PH7_TK_LPAREN; break;` |
|    69260 |  261 | `		case '[': pToken->nType \|= PH7_TK_OSB;   break; /* Bitwise operation here,since the square bracket token '['` |
|        - |  262 | `														 * is a potential operator [i.e: subscripting] */` |
|    69266 |  263 | `		case ']': pToken->nType = PH7_TK_CSB;    break;` |
|   338985 |  264 | `		case ')': {` |
|   677972 |  265 | `			SySet *pTokSet = pStream->pSet;` |
|        - |  266 | `			/* Assemble type cast operators [i.e: (int),(float),(bool)...] */` |
|   677972 |  267 | `			if( pTokSet->nUsed >= 2 ){` |
|        - |  268 | `				SyToken *pTmp;` |
|        - |  269 | `				/* Peek the last recongnized token */` |
|   677970 |  270 | `				pTmp = (SyToken *)SySetPeek(pTokSet);` |
|   677970 |  271 | `				if( pTmp->nType & PH7_TK_KEYWORD ){` |
|    13824 |  272 | `					sxi32 nID = SX_PTR_TO_INT(pTmp->pUserData);` |
|    13824 |  273 | `					if( (sxu32)nID & (PH7_TKWRD_ARRAY\|PH7_TKWRD_INT\|PH7_TKWRD_FLOAT\|PH7_TKWRD_STRING\|PH7_TKWRD_OBJECT\|PH7_TKWRD_BOOL\|PH7_TKWRD_UNSET) ){` |
|    13722 |  274 | `						pTmp = (SyToken *)SySetAt(pTokSet,pTokSet->nUsed - 2);` |
|    13722 |  275 | `						if( pTmp->nType & PH7_TK_LPAREN ){` |
|        - |  276 | `							/* Merge the three tokens '(' 'TYPE' ')' into a single one */` |
|    13638 |  277 | `							const char * zTypeCast = "(int)";` |
|    13638 |  278 | `							if( nID & PH7_TKWRD_FLOAT ){` |
|     2692 |  279 | `								zTypeCast = "(float)";` |
|    12293 |  280 | `							}else if( nID & PH7_TKWRD_BOOL ){` |
|     2694 |  281 | `								zTypeCast = "(bool)";` |
|     9602 |  282 | `							}else if( nID & PH7_TKWRD_STRING ){` |
|     5378 |  283 | `								zTypeCast = "(string)";` |
|     5568 |  284 | `							}else if( nID & PH7_TKWRD_ARRAY ){` |
|       21 |  285 | `								zTypeCast = "(array)";` |
|     2870 |  286 | `							}else if( nID & PH7_TKWRD_OBJECT ){` |
|       17 |  287 | `								zTypeCast = "(object)";` |
|     2852 |  288 | `							}else if( nID & PH7_TKWRD_UNSET ){` |
|        7 |  289 | `								zTypeCast = "(unset)";` |
|        3 |  290 | `							}` |
|        - |  291 | `							/* Reflect the change */` |
|    13638 |  292 | `							pToken->nType = PH7_TK_OP;` |
|    13638 |  293 | `							SyStringInitFromBuf(&pToken->sData,zTypeCast,SyStrlen(zTypeCast));` |
|        - |  294 | `							/* Save the instance associated with the type cast operator */` |
|    13638 |  295 | `							pToken->pUserData = (void *)PH7_ExprExtractOperator(&pToken->sData,0);` |
|        - |  296 | `							/* Remove the two previous tokens */` |
|    13638 |  297 | `							pTokSet->nUsed -= 2;` |
|    13638 |  298 | `							return SXRET_OK;` |
|        - |  299 | `						}` |
|       42 |  300 | `					}` |
|       93 |  301 | `				}` |
|   332166 |  302 | `			}` |
|   664336 |  303 | `			pToken->nType = PH7_TK_RPAREN;` |
|   664336 |  304 | `			break;` |
|        - |  305 | `				  }` |
|    28959 |  306 | `		case '\'':{` |
|        - |  307 | `			/* Single quoted string */` |
|    57920 |  308 | `			pStr->zString++;` |
|   728994 |  309 | `			while( pStream->zText < pStream->zEnd ){` |
|   728994 |  310 | `				if( pStream->zText[0] == '\''  ){` |
|    57930 |  311 | `					if( pStream->zText[-1] != '\\' ){` |
|    57906 |  312 | `						break;` |
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
|   671076 |  325 | `				if( pStream->zText[0] == '\n' ){` |
|       67 |  326 | `					pStream->nLine++;` |
|       33 |  327 | `				}` |
|   671076 |  328 | `				pStream->zText++;` |
|        2 |  329 | `			}` |
|        - |  330 | `			/* Record token length and type */` |
|    57920 |  331 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|    57920 |  332 | `			pToken->nType = PH7_TK_SSTR;` |
|        - |  333 | `			/* Jump the trailing single quote */` |
|    57920 |  334 | `			pStream->zText++;` |
|    57920 |  335 | `			return SXRET_OK;` |
|        - |  336 | `				  }` |
|     7897 |  337 | `		case '"':{` |
|        - |  338 | `			sxi32 iNest;` |
|        - |  339 | `			/* Double quoted string */` |
|    15796 |  340 | `			pStr->zString++;` |
|   156492 |  341 | `			while( pStream->zText < pStream->zEnd ){` |
|   156492 |  342 | `				if( pStream->zText[0] == '{' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '$'){` |
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
|   156492 |  364 | `				if( pStream->zText[0] == '"' ){` |
|    15896 |  365 | `					if( pStream->zText[-1] != '\\' ){` |
|    15792 |  366 | `						break;` |
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
|   140698 |  379 | `				if( pStream->zText[0] == '\n' ){` |
|        7 |  380 | `					pStream->nLine++;` |
|        3 |  381 | `				}` |
|   140698 |  382 | `				pStream->zText++;` |
|        2 |  383 | `			}` |
|        - |  384 | `			/* Record token length and type */` |
|    15796 |  385 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|    15796 |  386 | `			pToken->nType = PH7_TK_DSTR;` |
|        - |  387 | `			/* Jump the trailing quote */` |
|    15796 |  388 | `			pStream->zText++;` |
|    15796 |  389 | `			return SXRET_OK;` |
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
|     1201 |  411 | `		case ':':` |
|     2404 |  412 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == ':' ){` |
|        - |  413 | `				/* Current operator: '::' */` |
|      210 |  414 | `				pStream->zText++;` |
|      106 |  415 | `			}else{` |
|     2196 |  416 | `				pToken->nType = PH7_TK_COLON; /* Single colon */` |
|        - |  417 | `			}` |
|     2404 |  418 | `			break;` |
|    72720 |  419 | `		case ',': pToken->nType \|= PH7_TK_COMMA;  break; /* Comma is also an operator */` |
|   487612 |  420 | `		case ';': pToken->nType = PH7_TK_SEMI;    break;` |
|        - |  421 | `			/* Handle combined operators [i.e: +=,===,!=== ...] */` |
|   140879 |  422 | `		case '=':` |
|   281760 |  423 | `			pToken->nType \|= PH7_TK_EQUAL;` |
|   281760 |  424 | `			if( pStream->zText < pStream->zEnd ){` |
|   281760 |  425 | `				if( pStream->zText[0] == '=' ){` |
|    17616 |  426 | `					pToken->nType &= ~PH7_TK_EQUAL;` |
|        - |  427 | `					/* Current operator: == */` |
|    17616 |  428 | `					pStream->zText++;` |
|    17616 |  429 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  430 | `						/* Current operator: === */` |
|     3884 |  431 | `						pStream->zText++;` |
|     1943 |  432 | `					}` |
|   272953 |  433 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  434 | `					/* Array operator: => */` |
|     4064 |  435 | `					pToken->nType = PH7_TK_ARRAY_OP;` |
|     4064 |  436 | `					pStream->zText++;` |
|     2033 |  437 | `				}else{` |
|        - |  438 | `					/* TICKET 1433-0010: Reference operator '=&' */` |
|   260084 |  439 | `					const unsigned char *zCur = pStream->zText;` |
|   260084 |  440 | `					sxu32 nLine = 0;` |
|   520144 |  441 | `					while( zCur < pStream->zEnd && zCur[0] < 0xc0 && SyisSpace(zCur[0]) ){` |
|   260062 |  442 | `						if( zCur[0] == '\n' ){` |
|        5 |  443 | `							nLine++;` |
|        2 |  444 | `						}` |
|   260062 |  445 | `						zCur++;` |
|        2 |  446 | `					}` |
|   260084 |  447 | `					if( zCur < pStream->zEnd && zCur[0] == '&' ){` |
|        - |  448 | `						/* Current operator: =& */` |
|       48 |  449 | `						pToken->nType &= ~PH7_TK_EQUAL;` |
|       48 |  450 | `						SyStringInitFromBuf(pStr,"=&",sizeof("=&")-1);` |
|        - |  451 | `						/* Update token stream */` |
|       48 |  452 | `						pStream->zText = &zCur[1];` |
|       48 |  453 | `						pStream->nLine += nLine;` |
|       23 |  454 | `					}` |
|        - |  455 | `				}` |
|   140879 |  456 | `			}` |
|   281760 |  457 | `			break;` |
|    19138 |  458 | `		case '!':` |
|    38278 |  459 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  460 | `				/* Current operator: != */` |
|    16288 |  461 | `				pStream->zText++;` |
|    16288 |  462 | `				if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  463 | `					/* Current operator: !== */` |
|    13572 |  464 | `					pStream->zText++;` |
|     6785 |  465 | `				}` |
|     8143 |  466 | `			}` |
|    38278 |  467 | `			break;` |
|    11001 |  468 | `		case '&':` |
|    22004 |  469 | `			pToken->nType \|= PH7_TK_AMPER;` |
|    22004 |  470 | `			if( pStream->zText < pStream->zEnd ){` |
|    22004 |  471 | `				if( pStream->zText[0] == '&' ){` |
|     8456 |  472 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|        - |  473 | `					/* Current operator: && */` |
|     8456 |  474 | `					pStream->zText++;` |
|    17777 |  475 | `				}else if( pStream->zText[0] == '=' ){` |
|        7 |  476 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|        - |  477 | `					/* Current operator: &= */` |
|        7 |  478 | `					pStream->zText++;` |
|        3 |  479 | `				}` |
|    11001 |  480 | `			}` |
|    22004 |  481 | `			break;` |
|     1430 |  482 | `		case '\|':` |
|     2862 |  483 | `			if( pStream->zText < pStream->zEnd ){` |
|     2862 |  484 | `				if( pStream->zText[0] == '\|' ){` |
|        - |  485 | `					/* Current operator: \|\| */` |
|     2818 |  486 | `					pStream->zText++;` |
|     1454 |  487 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  488 | `					/* Current operator: \|= */` |
|        7 |  489 | `					pStream->zText++;` |
|        3 |  490 | `				}` |
|     1430 |  491 | `			}` |
|     2862 |  492 | `			break;` |
|     7061 |  493 | `		case '+':` |
|    14124 |  494 | `			if( pStream->zText < pStream->zEnd ){` |
|    14122 |  495 | `				if( pStream->zText[0] == '+' ){` |
|        - |  496 | `					/* Current operator: ++ */` |
|    10980 |  497 | `					pStream->zText++;` |
|     8633 |  498 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  499 | `					/* Current operator: += */` |
|       42 |  500 | `					pStream->zText++;` |
|       20 |  501 | `				}` |
|     7060 |  502 | `			}` |
|    14124 |  503 | `			break;` |
|    51651 |  504 | `		case '-':` |
|   103304 |  505 | `			if( pStream->zText < pStream->zEnd ){` |
|   103304 |  506 | `				if( pStream->zText[0] == '-' ){` |
|        - |  507 | `					/* Current operator: -- */` |
|        5 |  508 | `					pStream->zText++;` |
|   103302 |  509 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  510 | `					/* Current operator: -= */` |
|        7 |  511 | `					pStream->zText++;` |
|   103297 |  512 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  513 | `					/* Current operator: -> */` |
|   102814 |  514 | `					pStream->zText++;` |
|    51406 |  515 | `				}` |
|    51651 |  516 | `			}` |
|   103304 |  517 | `			break;` |
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
|    29202 |  542 | `		case '.':` |
|    58406 |  543 | `			if( pStream->zText + 1 < pStream->zEnd && pStream->zText[0] == '.' && pStream->zText[1] == '.' ){` |
|        - |  544 | `				/* Ellipsis: ... */` |
|       42 |  545 | `				pStream->zText += 2;` |
|       42 |  546 | `				pToken->nType = PH7_TK_ELLIPSIS;` |
|    58386 |  547 | `			}else if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  548 | `				/* Current operator: .= */` |
|     2754 |  549 | `				pStream->zText++;` |
|     1376 |  550 | `			}` |
|    58406 |  551 | `			break;` |
|    23072 |  552 | `		case '<':` |
|    46146 |  553 | `			if( pStream->zText < pStream->zEnd ){` |
|    46146 |  554 | `				if( pStream->zText[0] == '<' ){` |
|        - |  555 | `					/* Current operator: << */` |
|      132 |  556 | `					pStream->zText++;` |
|      132 |  557 | `					if( pStream->zText < pStream->zEnd ){` |
|      132 |  558 | `						if( pStream->zText[0] == '=' ){` |
|        - |  559 | `							/* Current operator: <<= */` |
|        9 |  560 | `							pStream->zText++;` |
|      128 |  561 | `						}else if( pStream->zText[0] == '<' ){` |
|        - |  562 | `							/* Current Token: <<<  */` |
|      110 |  563 | `							pStream->zText++;` |
|        - |  564 | `							/* This may be the beginning of a Heredoc/Nowdoc string,try to delimit it */` |
|      110 |  565 | `							rc = LexExtractHeredoc(&(*pStream),&(*pToken));` |
|      110 |  566 | `							if( rc == SXRET_OK ){` |
|        - |  567 | `								/* Here/Now doc successfuly extracted */` |
|      110 |  568 | `								return SXRET_OK;` |
|        - |  569 | `							}` |
|      ! 0 |  570 | `						}` |
|       12 |  571 | `					}` |
|    46027 |  572 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  573 | `					/* Current operator: <> */` |
|        5 |  574 | `					pStream->zText++;` |
|    46014 |  575 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  576 | `					/* Current operator: <= or <=> */` |
|       88 |  577 | `					pStream->zText++;` |
|       88 |  578 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '>' ){` |
|        - |  579 | `						/* Current operator: <=> */` |
|       51 |  580 | `						pStream->zText++;` |
|       25 |  581 | `					}` |
|       43 |  582 | `				}` |
|    23018 |  583 | `			}` |
|    46038 |  584 | `			break;` |
|     2791 |  585 | `		case '>':` |
|     5584 |  586 | `			if( pStream->zText < pStream->zEnd ){` |
|     5584 |  587 | `				if( pStream->zText[0] == '>' ){` |
|        - |  588 | `					/* Current operator: >> */` |
|       21 |  589 | `					pStream->zText++;` |
|       21 |  590 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  591 | `						/* Current operator: >>= */` |
|       11 |  592 | `						pStream->zText++;` |
|        6 |  593 | `					}` |
|     5574 |  594 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  595 | `					/* Current operator: >= */` |
|       80 |  596 | `					pStream->zText++;` |
|       39 |  597 | `				}` |
|     2791 |  598 | `			}` |
|     5584 |  599 | `			break;` |
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
|  4198758 |  613 | `		if( pStr->nByte <= 0 ){` |
|        - |  614 | `			/* Record token length */` |
|  4198712 |  615 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  2099355 |  616 | `		}` |
|  4198758 |  617 | `		if( pToken->nType & PH7_TK_OP ){` |
|        - |  618 | `			const ph7_expr_op *pOp;` |
|        - |  619 | `			/* Check if the extracted token is an operator */` |
|   712934 |  620 | `			pOp = PH7_ExprExtractOperator(pStr,(SyToken *)SySetPeek(pStream->pSet));` |
|   712934 |  621 | `			if( pOp == 0 ){` |
|        - |  622 | `				/* Not an operator */` |
|      ! 0 |  623 | `				pToken->nType &= ~PH7_TK_OP;` |
|      ! 0 |  624 | `				if( pToken->nType <= 0 ){` |
|      ! 0 |  625 | `					pToken->nType = PH7_TK_OTHER;` |
|      ! 0 |  626 | `				}` |
|      ! 0 |  627 | `			}else{` |
|        - |  628 | `				/* Save the instance associated with this operator for later processing */` |
|   712934 |  629 | `				pToken->pUserData = (void *)pOp;` |
|        - |  630 | `			}` |
|   356466 |  631 | `		}` |
|        - |  632 | `	}` |
|        - |  633 | `	/* Tell the upper-layer to save the extracted token for later processing */` |
|  6712208 |  634 | `	return SXRET_OK;` |
|  3478542 |  635 |  |
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
|  2513452 |  652 | `static sxu32 KeywordCode(const char *z, int n){` |
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
|  2513452 |  742 | `  if( n<2 ) return PH7_TK_ID;` |
|  2422532 |  743 | `  h = (((int)z[0]*4) ^ ((int)z[n-1]*3) ^ n) % 151;` |
|  3708408 |  744 | `  for(i=((int)aHash[h])-1; i>=0; i=((int)aNext[i])-1){` |
|  2144694 |  745 | `    if( (int)aLen[i]==n && SyMemcmp(&zText[aOffset[i]],z,n)==0 ){` |
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
|   858818 |  830 | `      return aCode[i];` |
|        - |  831 | `    }` |
|   642938 |  832 | `  }` |
|        - |  833 | `  /* Linear fallback for keywords not in the auto-generated hash table */` |
|  1563716 |  834 | `  if( n==5 && SyMemcmp(z,"trait",5)==0 ) return PH7_TKWRD_TRAIT;` |
|  1563662 |  835 | `  if( n==9 && SyMemcmp(z,"insteadof",9)==0 ) return PH7_TKWRD_INSTEADOF;` |
|  1563658 |  836 | `  if( n==7 && SyMemcmp(z,"finally",7)==0 ) return PH7_TKWRD_FINALLY;` |
|  1563628 |  837 | `  if( n==5 && SyMemcmp(z,"yield",5)==0 ) return PH7_TKWRD_YIELD;` |
|  1563596 |  838 | `  return PH7_TK_ID;` |
|  1256727 |  839 |  |
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
|      108 |  873 | `static sxi32 LexExtractHeredoc(SyStream *pStream,SyToken *pToken)` |
|        2 |  874 |  |
|      110 |  875 | `	const unsigned char *zIn  = pStream->zText;` |
|      110 |  876 | `	const unsigned char *zEnd = pStream->zEnd;` |
|        - |  877 | `	const unsigned char *zPtr;` |
|      110 |  878 | `	sxu8 bNowDoc = FALSE;` |
|        - |  879 | `	SyString sDelim;` |
|        - |  880 | `	SyString sStr;` |
|        - |  881 | `	/* Jump leading white spaces */` |
|      122 |  882 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|       13 |  883 | `		zIn++;` |
|        1 |  884 | `	}` |
|      110 |  885 | `	if( zIn >= zEnd ){` |
|        - |  886 | `		/* A simple symbol,return immediately */` |
|      ! 0 |  887 | `		return SXERR_CONTINUE;` |
|        - |  888 | `	}` |
|      110 |  889 | `	if( zIn[0] == '\'' \|\| zIn[0] == '"' ){` |
|        - |  890 | `		/* Make sure we are dealing with a nowdoc */` |
|       44 |  891 | `		bNowDoc =  zIn[0] == '\'' ? TRUE : FALSE;` |
|       44 |  892 | `		zIn++;` |
|       21 |  893 | `	}` |
|      110 |  894 | `	if( zIn[0] < 0xc0 && !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|        - |  895 | `		/* Invalid delimiter,return immediately */` |
|      ! 0 |  896 | `		return SXERR_CONTINUE;` |
|        - |  897 | `	}` |
|        - |  898 | `	/* Isolate the identifier */` |
|      110 |  899 | `	sDelim.zString = (const char *)zIn;` |
|      116 |  900 | `	for(;;){` |
|      234 |  901 | `		zPtr = zIn;` |
|        - |  902 | `		/* Skip alphanumeric stream */` |
|      744 |  903 | `		while( zPtr < zEnd && zPtr[0] < 0xc0 && (SyisAlphaNum(zPtr[0]) \|\| zPtr[0] == '_') ){` |
|      396 |  904 | `			zPtr++;` |
|        2 |  905 | `		}` |
|      234 |  906 | `		if( zPtr < zEnd && zPtr[0] >= 0xc0 ){` |
|       19 |  907 | `			zPtr++;` |
|        - |  908 | `			/* UTF-8 stream */` |
|       37 |  909 | `			while( zPtr < zEnd && ((zPtr[0] & 0xc0) == 0x80) ){` |
|       19 |  910 | `				zPtr++;` |
|        1 |  911 | `			}` |
|        9 |  912 | `		}` |
|      234 |  913 | `		if( zPtr == zIn ){` |
|        - |  914 | `			/* Not an UTF-8 or alphanumeric stream */` |
|      110 |  915 | `			break;` |
|        - |  916 | `		}` |
|        - |  917 | `		/* Synchronize pointers */` |
|      126 |  918 | `		zIn = zPtr;` |
|        2 |  919 | `	}` |
|        - |  920 | `	/* Get the identifier length */` |
|      110 |  921 | `	sDelim.nByte = (sxu32)((const char *)zIn-sDelim.zString);` |
|      110 |  922 | `	if( zIn[0] == '"' \|\| (bNowDoc && zIn[0] == '\'') ){` |
|        - |  923 | `		/* Jump the trailing single quote */` |
|       44 |  924 | `		zIn++;` |
|       21 |  925 | `	}` |
|        - |  926 | `	/* Jump trailing white spaces */` |
|      110 |  927 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|      ! 0 |  928 | `		zIn++;` |
|      ! 0 |  929 | `	}` |
|      110 |  930 | `	if( sDelim.nByte <= 0 \|\| zIn >= zEnd \|\| zIn[0] != '\n' ){` |
|        - |  931 | `		/* Invalid syntax */` |
|      ! 0 |  932 | `		return SXERR_CONTINUE;` |
|        - |  933 | `	}` |
|      110 |  934 | `	pStream->nLine++; /* Increment line counter */` |
|      110 |  935 | `	zIn++;` |
|        - |  936 | `	/* Isolate the delimited string */` |
|      110 |  937 | `	sStr.zString = (const char *)zIn;` |
|        - |  938 | `	/* PHP 7.3 flexible heredoc/nowdoc: the closing marker may be preceded` |
|        - |  939 | `	 * by whitespace (spaces/tabs), and may be followed by any non-identifier` |
|        - |  940 | `	 * character. The indent count is recorded in pToken->pUserData and the` |
|        - |  941 | `	 * compile phase strips it from each body line. */` |
|        - |  942 | `	{` |
|      110 |  943 | `		const unsigned char *zMarkerLine = zIn; /* Start of marker's line (set on match) */` |
|      110 |  944 | `		sxu32 nIndent = 0;` |
|      222 |  945 | `		for(;;){` |
|      278 |  946 | `			const unsigned char *zLineStart = zIn;` |
|        - |  947 | `			/* Skip leading space/tab on this line */` |
|      800 |  948 | `			while( zIn < zEnd && (zIn[0] == ' ' \|\| zIn[0] == '\t') ){` |
|      386 |  949 | `				zIn++;` |
|        2 |  950 | `			}` |
|      276 |  951 | `			if( (sxu32)(zEnd - zIn) >= sDelim.nByte` |
|      277 |  952 | `				&& SyMemcmp((const void *)sDelim.zString,(const void *)zIn,sDelim.nByte) == 0 ){` |
|        - |  953 | `				int bIdentCont;` |
|      108 |  954 | `				zPtr = &zIn[sDelim.nByte];` |
|        - |  955 | `				/* Disambiguate: next byte must not continue an identifier.` |
|        - |  956 | `				 * A leading byte >= 0xc0 starts a multi-byte UTF-8 sequence,` |
|        - |  957 | `				 * which PHP identifiers may contain, so treat it as ident. */` |
|      108 |  958 | `				if( zPtr >= zEnd ){` |
|      ! 0 |  959 | `					bIdentCont = 0;` |
|      108 |  960 | `				}else if( zPtr[0] >= 0xc0 ){` |
|      ! 0 |  961 | `					bIdentCont = 1;` |
|      ! 0 |  962 | `				}else{` |
|      108 |  963 | `					bIdentCont = (SyisAlphaNum(zPtr[0]) \|\| zPtr[0] == '_');` |
|        - |  964 | `				}` |
|      108 |  965 | `				if( !bIdentCont ){` |
|        - |  966 | `					/* Closing marker found */` |
|      108 |  967 | `					nIndent = (sxu32)(zIn - zLineStart);` |
|      108 |  968 | `					zMarkerLine = zLineStart;` |
|      108 |  969 | `					pStream->zText = zPtr; /* Cursor right after identifier */` |
|      108 |  970 | `					break;` |
|        - |  971 | `				}` |
|      ! 0 |  972 | `			}` |
|        - |  973 | `			/* Not the closing marker on this line; walk to next newline */` |
|     2784 |  974 | `			while( zIn < zEnd && zIn[0] != '\n' ){` |
|     2614 |  975 | `				zIn++;` |
|        2 |  976 | `			}` |
|      172 |  977 | `			if( zIn >= zEnd ){` |
|        - |  978 | `				/* End of input without finding the closing marker */` |
|        3 |  979 | `				pStream->zText = pStream->zEnd;` |
|        3 |  980 | `				zMarkerLine = zIn;` |
|        3 |  981 | `				break;` |
|        - |  982 | `			}` |
|      170 |  983 | `			pStream->nLine++;` |
|      170 |  984 | `			zIn++;` |
|        2 |  985 | `		}` |
|        - |  986 | `		/* Body runs from sStr.zString up to just before the marker line */` |
|      110 |  987 | `		sStr.nByte = (sxu32)((const char *)zMarkerLine - sStr.zString);` |
|      110 |  988 | `		pToken->nType = bNowDoc ? PH7_TK_NOWDOC : PH7_TK_HEREDOC;` |
|      110 |  989 | `		SyStringDupPtr(&pToken->sData,&sStr);` |
|        - |  990 | `		/* Strip exactly one line terminator that precedes the marker's line. */` |
|      108 |  991 | `		if( pToken->sData.nByte > 0` |
|      106 |  992 | `			&& pToken->sData.zString[pToken->sData.nByte - 1] == '\n' ){` |
|      100 |  993 | `			pToken->sData.nByte--;` |
|       98 |  994 | `			if( pToken->sData.nByte > 0` |
|      100 |  995 | `				&& pToken->sData.zString[pToken->sData.nByte - 1] == '\r' ){` |
|      ! 0 |  996 | `				pToken->sData.nByte--;` |
|      ! 0 |  997 | `			}` |
|       49 |  998 | `		}` |
|      110 |  999 | `		pToken->pUserData = SX_INT_TO_PTR(nIndent);` |
|        - | 1000 | `	}` |
|        - | 1001 | `	/* All done */` |
|      110 | 1002 | `	return SXRET_OK;` |
|       56 | 1003 |  |
|        - | 1004 | `/*` |
|        - | 1005 | ` * Tokenize a raw PHP input.` |
|        - | 1006 | ` * This is the public tokenizer called by most code generator routines.` |
|        - | 1007 | ` */` |
|    13382 | 1008 | `PH7_PRIVATE sxi32 PH7_TokenizePHP(const char *zInput,sxu32 nLen,sxu32 nLineStart,SySet *pOut)` |
|        2 | 1009 |  |
|        - | 1010 | `	SyLex sLexer;` |
|        - | 1011 | `	sxi32 rc;` |
|        - | 1012 | `	/* Initialize the lexer */` |
|    13384 | 1013 | `	rc = SyLexInit(&sLexer,&(*pOut),TokenizePHP,0);` |
|    13384 | 1014 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1015 | `		return rc;` |
|        - | 1016 | `	}` |
|    13384 | 1017 | `	sLexer.sStream.nLine = nLineStart;` |
|        - | 1018 | `	/* Tokenize input */` |
|    13384 | 1019 | `	rc = SyLexTokenizeInput(&sLexer,zInput,nLen,0,0,0);` |
|        - | 1020 | `	/* Release the lexer */` |
|    13384 | 1021 | `	SyLexRelease(&sLexer);` |
|        - | 1022 | `	/* Tokenization result */` |
|    13384 | 1023 | `	return rc;` |
|     6693 | 1024 |  |
|        - | 1025 | `/*` |
|        - | 1026 | ` * High level public tokenizer.` |
|        - | 1027 | ` *  Tokenize the input into PHP tokens and raw tokens [i.e: HTML,XML,Raw text...].` |
|        - | 1028 | ` * According to the PHP language reference manual` |
|        - | 1029 | ` *   When PHP parses a file, it looks for opening and closing tags, which tell PHP` |
|        - | 1030 | ` *   to start and stop interpreting the code between them. Parsing in this manner allows` |
|        - | 1031 | ` *   PHP to be embedded in all sorts of different documents, as everything outside of a pair` |
|        - | 1032 | ` *   of opening and closing tags is ignored by the PHP parser. Most of the time you will see` |
|        - | 1033 | ` *   PHP embedded in HTML documents, as in this example.` |
|        - | 1034 | ` *   <?php echo 'While this is going to be parsed.'; ?>` |
|        - | 1035 | ` *   <p>This will also be ignored.</p>` |
|        - | 1036 | ` *   You can also use more advanced structures:` |
|        - | 1037 | ` *   Example #1 Advanced escaping` |
|        - | 1038 | ` * <?php` |
|        - | 1039 | ` * if ($expression) {` |
|        - | 1040 | ` *   ?>` |
|        - | 1041 | ` *   <strong>This is true.</strong>` |
|        - | 1042 | ` *   <?php` |
|        - | 1043 | ` * } else {` |
|        - | 1044 | ` *   ?>` |
|        - | 1045 | ` *   <strong>This is false.</strong>` |
|        - | 1046 | ` *   <?php` |
|        - | 1047 | ` * }` |
|        - | 1048 | ` * ?>` |
|        - | 1049 | ` * This works as expected, because when PHP hits the ?> closing tags, it simply starts outputting` |
|        - | 1050 | ` * whatever it finds (except for an immediately following newline - see instruction separation ) until it hits` |
|        - | 1051 | ` * another opening tag. The example given here is contrived, of course, but for outputting large blocks of text` |
|        - | 1052 | ` * dropping out of PHP parsing mode is generally more efficient than sending all of the text through echo() or print().` |
|        - | 1053 | ` * There are four different pairs of opening and closing tags which can be used in PHP. Three of those, <?php ?>` |
|        - | 1054 | ` * <script language="php"> </script>  and <? ?> are always available. The other two are short tags and ASP style` |
|        - | 1055 | ` * tags, and can be turned on and off from the php.ini configuration file. As such, while some people find short tags` |
|        - | 1056 | ` * and ASP style tags convenient, they are less portable, and generally not recommended.` |
|        - | 1057 | ` * Note:` |
|        - | 1058 | ` * Also note that if you are embedding PHP within XML or XHTML you will need to use the <?php ?> tags to remain` |
|        - | 1059 | ` * compliant with standards.` |
|        - | 1060 | ` * Example #2 PHP Opening and Closing Tags` |
|        - | 1061 | ` * 1.  <?php echo 'if you want to serve XHTML or XML documents, do it like this'; ?>` |
|        - | 1062 | ` * 2.  <script language="php">` |
|        - | 1063 | ` *       echo 'some editors (like FrontPage) don\'t` |
|        - | 1064 | ` *             like processing instructions';` |
|        - | 1065 | ` *   </script>` |
|        - | 1066 | ` *` |
|        - | 1067 | ` * 3.  <? echo 'this is the simplest, an SGML processing instruction'; ?>` |
|        - | 1068 | ` *   <?= expression ?> This is a shortcut for "<? echo expression ?>"` |
|        - | 1069 | ` */` |
|    11008 | 1070 | `PH7_PRIVATE sxi32 PH7_TokenizeRawText(const char *zInput,sxu32 nLen,SySet *pOut)` |
|        2 | 1071 |  |
|    11010 | 1072 | `	const char *zEnd = &zInput[nLen];` |
|    11010 | 1073 | `	const char *zIn  = zInput;` |
|        - | 1074 | `	const char *zCur,*zCurEnd;` |
|    11010 | 1075 | `	SyString sCtag = { 0, 0 };     /* Closing tag */` |
|        - | 1076 | `	SyToken sToken;` |
|        - | 1077 | `	SyString sDoc;` |
|        - | 1078 | `	sxu32 nLine;` |
|        - | 1079 | `	sxi32 iNest;` |
|        - | 1080 | `	sxi32 rc;` |
|        - | 1081 | `	/* Tokenize the input into PHP tokens and raw tokens */` |
|    11010 | 1082 | `	nLine = 1;` |
|    11010 | 1083 | `	zCur = zCurEnd   = 0; /* Prevent compiler warning */` |
|    11010 | 1084 | `	sToken.pUserData = 0;` |
|    11010 | 1085 | `	iNest = 0;` |
|    11010 | 1086 | `	sDoc.nByte = 0;` |
|    11010 | 1087 | `	sDoc.zString = ""; /* cc warning */` |
|    11010 | 1088 | `	for(;;){` |
|    22022 | 1089 | `		if( zIn >= zEnd ){` |
|        - | 1090 | `			/* End of input reached */` |
|    11006 | 1091 | `			break;` |
|        - | 1092 | `		}` |
|    11018 | 1093 | `		sToken.nLine = nLine;` |
|    11018 | 1094 | `		zCur = zIn;` |
|    11018 | 1095 | `		zCurEnd = 0;` |
|    11026 | 1096 | `		while( zIn < zEnd ){` |
|    11022 | 1097 | `			 if( zIn[0] == '<' ){` |
|    11014 | 1098 | `				const char *zTmp = zIn; /* End of raw input marker */` |
|    11014 | 1099 | `				zIn++;` |
|    11014 | 1100 | `				if( zIn < zEnd ){` |
|    11014 | 1101 | `					if( zIn[0] == '?' ){` |
|    11014 | 1102 | `						zIn++;` |
|    11014 | 1103 | `						if( (sxu32)(zEnd - zIn) >= sizeof("php")-1 &&  SyStrnicmp(zIn,"php",sizeof("php")-1) == 0 ){` |
|        - | 1104 | `							/* opening tag: <?php */` |
|    11012 | 1105 | `							zIn += sizeof("php")-1;` |
|     5505 | 1106 | `						}` |
|        - | 1107 | `						/* Look for the closing tag '?>' */` |
|    11014 | 1108 | `						SyStringInitFromBuf(&sCtag,"?>",sizeof("?>")-1);` |
|    11014 | 1109 | `						zCurEnd = zTmp;` |
|    11014 | 1110 | `						break;` |
|        - | 1111 | `					}` |
|      ! 0 | 1112 | `				}` |
|      ! 0 | 1113 | `			}else{` |
|       10 | 1114 | `				if( zIn[0] == '\n' ){` |
|       10 | 1115 | `					nLine++;` |
|        4 | 1116 | `				}` |
|       10 | 1117 | `				zIn++;` |
|        - | 1118 | `			 }` |
|        2 | 1119 | `		} /* While(zIn < zEnd) */` |
|    11018 | 1120 | `		if( zCurEnd == 0 ){` |
|        5 | 1121 | `			zCurEnd = zIn;` |
|        2 | 1122 | `		}` |
|        - | 1123 | `		/* Save the raw token */` |
|    11018 | 1124 | `		SyStringInitFromBuf(&sToken.sData,zCur,zCurEnd - zCur);` |
|    11018 | 1125 | `		sToken.nType = PH7_TOKEN_RAW;` |
|    11018 | 1126 | `		rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|    11018 | 1127 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1128 | `			return rc;` |
|        - | 1129 | `		}` |
|    11018 | 1130 | `		if( zIn >= zEnd ){` |
|        5 | 1131 | `			break;` |
|        - | 1132 | `		}` |
|        - | 1133 | `		/* Ignore leading white space */` |
|    23916 | 1134 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    12904 | 1135 | `			if( zIn[0] == '\n' ){` |
|    11710 | 1136 | `				nLine++;` |
|     5854 | 1137 | `			}` |
|    12904 | 1138 | `			zIn++;` |
|        2 | 1139 | `		}` |
|        - | 1140 | `		/* Delimit the PHP chunk */` |
|    11014 | 1141 | `		sToken.nLine = nLine;` |
|    11014 | 1142 | `		zCur = zIn;` |
|  1013628 | 1143 | `		while( (sxu32)(zEnd - zIn) >= sCtag.nByte ){` |
|        - | 1144 | `			const char *zPtr;` |
|  1008828 | 1145 | `			if( SyMemcmp(zIn,sCtag.zString,sCtag.nByte) == 0 && iNest < 1 ){` |
|     6214 | 1146 | `				break;` |
|        - | 1147 | `			}` |
|   503271 | 1148 | `			for(;;){` |
|  1006544 | 1149 | `				if( zIn[0] != '/' \|\| (zIn[1] != '*' && zIn[1] != '/') /* && sCtag.nByte >= 2 */ ){` |
|   501309 | 1150 | `					break;` |
|        - | 1151 | `				}` |
|     3930 | 1152 | `				zIn += 2;` |
|     3930 | 1153 | `				if( zIn[-1] == '/' ){` |
|        - | 1154 | `					/* Inline comment */` |
|   136218 | 1155 | `					while( zIn < zEnd && zIn[0] != '\n' ){` |
|   132372 | 1156 | `						zIn++;` |
|        2 | 1157 | `					}` |
|     3848 | 1158 | `					if( zIn >= zEnd ){` |
|      ! 0 | 1159 | `						zIn--;` |
|      ! 0 | 1160 | `					}` |
|     1925 | 1161 | `				}else{` |
|        - | 1162 | `					/* Block comment */` |
|     4500 | 1163 | `					while( (sxu32)(zEnd-zIn) >= sizeof("*/") - 1 ){` |
|     4500 | 1164 | `						if( zIn[0] == '*' && zIn[1] == '/' ){` |
|       84 | 1165 | `							zIn += 2;` |
|       84 | 1166 | `							break;` |
|        - | 1167 | `						}` |
|     4418 | 1168 | `						if( zIn[0] == '\n' ){` |
|       28 | 1169 | `							nLine++;` |
|       13 | 1170 | `						}` |
|     4418 | 1171 | `						zIn++;` |
|        2 | 1172 | `					}` |
|        - | 1173 | `				}` |
|        2 | 1174 | `			}` |
|  1002616 | 1175 | `			if( zIn[0] == '\n' ){` |
|    35394 | 1176 | `				nLine++;` |
|    35394 | 1177 | `				if( iNest > 0 ){` |
|      278 | 1178 | `					zIn++;` |
|      662 | 1179 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|      386 | 1180 | `						zIn++;` |
|        2 | 1181 | `					}` |
|      278 | 1182 | `					zPtr = zIn;` |
|     1420 | 1183 | `					while( zIn < zEnd ){` |
|     1420 | 1184 | `						if( (unsigned char)zIn[0] >= 0xc0 ){` |
|        - | 1185 | `							/* UTF-8 stream */` |
|       19 | 1186 | `							zIn++;` |
|       37 | 1187 | `							SX_JMP_UTF8(zIn,zEnd);` |
|     1410 | 1188 | `						}else if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|      140 | 1189 | `							break;` |
|      ! 0 | 1190 | `						}else{` |
|     1126 | 1191 | `							zIn++;` |
|        - | 1192 | `						}` |
|        2 | 1193 | `					}` |
|      278 | 1194 | `					if( (sxu32)(zIn - zPtr) == sDoc.nByte && SyMemcmp(sDoc.zString,zPtr,sDoc.nByte) == 0 ){` |
|      108 | 1195 | `						iNest = 0;` |
|       53 | 1196 | `					}` |
|      278 | 1197 | `					continue;` |
|        2 | 1198 | `				}` |
|   984782 | 1199 | `			}else if ( (sxu32)(zEnd - zIn) >= sizeof("<<<") && zIn[0] == '<' && zIn[1] == '<' && zIn[2] == '<' && iNest < 1){` |
|      110 | 1200 | `				zIn += sizeof("<<<")-1;` |
|      122 | 1201 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|       13 | 1202 | `					zIn++;` |
|        1 | 1203 | `				}` |
|      110 | 1204 | `				if( zIn[0] == '"' \|\| zIn[0] == '\'' ){` |
|       44 | 1205 | `					zIn++;` |
|       21 | 1206 | `				}` |
|      110 | 1207 | `				zPtr = zIn;` |
|      522 | 1208 | `				while( zIn < zEnd ){` |
|      522 | 1209 | `					if( (unsigned char)zIn[0] >= 0xc0 ){` |
|        - | 1210 | `						/* UTF-8 stream */` |
|       19 | 1211 | `						zIn++;` |
|       37 | 1212 | `						SX_JMP_UTF8(zIn,zEnd);` |
|      512 | 1213 | `					}else if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       56 | 1214 | `						break;` |
|      ! 0 | 1215 | `					}else{` |
|      396 | 1216 | `						zIn++;` |
|        - | 1217 | `					}` |
|        2 | 1218 | `				}` |
|      110 | 1219 | `				SyStringInitFromBuf(&sDoc,zPtr,zIn-zPtr);` |
|      110 | 1220 | `				SyStringFullTrim(&sDoc);` |
|      110 | 1221 | `				if( sDoc.nByte > 0 ){` |
|      110 | 1222 | `					iNest++;` |
|       54 | 1223 | `				}` |
|      110 | 1224 | `				continue;` |
|        - | 1225 | `			}` |
|  1002232 | 1226 | `			zIn++;` |
|        - | 1227 |  |
|  1002232 | 1228 | `			if ( zIn >= zEnd )` |
|      ! 0 | 1229 | `				break;` |
|        2 | 1230 | `		}` |
|    11014 | 1231 | `		if( (sxu32)(zEnd - zIn) < sCtag.nByte ){` |
|     4802 | 1232 | `			zIn = zEnd;` |
|     2400 | 1233 | `		}` |
|    11014 | 1234 | `		if( zCur < zIn ){` |
|        - | 1235 | `			/* Save the PHP chunk for later processing */` |
|     8878 | 1236 | `			sToken.nType = PH7_TOKEN_PHP;` |
|     8878 | 1237 | `			SyStringInitFromBuf(&sToken.sData,zCur,zIn-zCur);` |
|    17696 | 1238 | `			SyStringRightTrim(&sToken.sData); /* Trim trailing white spaces */` |
|     8878 | 1239 | `			rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|     8878 | 1240 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1241 | `				return rc;` |
|        - | 1242 | `			}` |
|     4438 | 1243 | `		}` |
|    11014 | 1244 | `		if( zIn < zEnd ){` |
|        - | 1245 | `			/* Jump the trailing closing tag */` |
|     6214 | 1246 | `			zIn += sCtag.nByte;` |
|     3106 | 1247 | `		}` |
|        2 | 1248 | `	} /* For(;;) */` |
|        - | 1249 |  |
|    11010 | 1250 | ` 	return SXRET_OK;` |
|     5506 | 1251 |  |
|        - | 1252 |  |
