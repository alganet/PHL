# src/ph7/lex.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 723/758 lines (95.38%)

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
|  8648694 |   19 | `static sxi32 TokenizePHP(SyStream *pStream,SyToken *pToken,void *pUserData,void *pCtxData)` |
|        2 |   20 |  |
|        - |   21 | `	SyString *pStr;` |
|        - |   22 | `	sxi32 rc;` |
|        - |   23 | `	/* Ignore leading white spaces */` |
| 12983574 |   24 | `	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){` |
|        - |   25 | `		/* Advance the stream cursor */` |
|  4334880 |   26 | `		if( pStream->zText[0] == '\n' ){` |
|        - |   27 | `			/* Update line counter */` |
|    33664 |   28 | `			pStream->nLine++;` |
|    16831 |   29 | `		}` |
|  4334880 |   30 | `		pStream->zText++;` |
|        2 |   31 | `	}` |
|  8648696 |   32 | `	if( pStream->zText >= pStream->zEnd ){` |
|        - |   33 | `		/* End of input reached */` |
|      ! 0 |   34 | `		return SXERR_EOF;` |
|        - |   35 | `	}` |
|        - |   36 | `	/* Record token starting position and line */` |
|  8648696 |   37 | `	pToken->nLine = pStream->nLine;` |
|  8648696 |   38 | `	pToken->pUserData = 0;` |
|  8648696 |   39 | `	pStr = &pToken->sData;` |
|  8648696 |   40 | `	SyStringInitFromBuf(pStr,pStream->zText,0);` |
| 10240067 |   41 | `	if( pStream->zText[0] >= 0xc0 \|\| SyisAlpha(pStream->zText[0]) \|\| pStream->zText[0] == '_' ){` |
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
|  3182744 |   52 | `		if( pStream->zText[0] < 0xc0 ){` |
|  3182728 |   53 | `			pStream->zText++;` |
|  1591363 |   54 | `		}` |
|  3131054 |   55 | `		for(;;){` |
|  6262110 |   56 | `			zIn = pStream->zText;` |
|  6262110 |   57 | `			if( zIn[0] >= 0xc0 ){` |
|       49 |   58 | `				zIn++;` |
|        - |   59 | `				/* UTF-8 stream */` |
|      109 |   60 | `				while( zIn < pStream->zEnd && ((zIn[0] & 0xc0) == 0x80) ){` |
|       61 |   61 | `					zIn++;` |
|        1 |   62 | `				}` |
|       24 |   63 | `			}` |
|        - |   64 | `			/* Skip alphanumeric stream */` |
| 25926242 |   65 | `			while( zIn < pStream->zEnd && zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_') ){` |
| 16533080 |   66 | `				zIn++;` |
|        2 |   67 | `			}` |
|  6262110 |   68 | `			if( zIn == pStream->zText ){` |
|        - |   69 | `				/* Not an UTF-8 or alphanumeric stream */` |
|  3182744 |   70 | `				break;` |
|        - |   71 | `			}` |
|        - |   72 | `			/* Synchronize pointers */` |
|  3079368 |   73 | `			pStream->zText = zIn;` |
|        2 |   74 | `		}` |
|        - |   75 | `		/* Record token length */` |
|  3182744 |   76 | `		pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  3182744 |   77 | `		nKeyword = KeywordCode(pStr->zString,(int)pStr->nByte);` |
|        - |   78 | `		/* PHP 7.4: 'fn' is a keyword reserved for arrow functions.` |
|        - |   79 | `		 * The auto-generated perfect hash above doesn't know about it,` |
|        - |   80 | `		 * so intercept the 'fn' identifier here.` |
|        - |   81 | `		 */` |
|  3183226 |   82 | `		if( nKeyword == PH7_TK_ID && pStr->nByte == 2` |
|  1035062 |   83 | `			&& pStr->zString[0] == 'f' && pStr->zString[1] == 'n' ){` |
|      362 |   84 | `			nKeyword = PH7_TKWRD_FN;` |
|      180 |   85 | `		}` |
|  3182744 |   86 | `		if( nKeyword != PH7_TK_ID ){` |
|  1120774 |   87 | `			if( nKeyword &` |
|        - |   88 | `				(PH7_TKWRD_NEW\|PH7_TKWRD_CLONE\|PH7_TKWRD_AND\|PH7_TKWRD_XOR\|PH7_TKWRD_OR\|PH7_TKWRD_INSTANCEOF\|PH7_TKWRD_SEQ\|PH7_TKWRD_SNE) ){` |
|        - |   89 | `					/* Alpha stream operators [i.e: new,clone,and,instanceof,eq,ne,or,xor],save the operator instance for later processing */` |
|    16078 |   90 | `					pToken->pUserData = (void *)PH7_ExprExtractOperator(pStr,0);` |
|        - |   91 | `					/* Mark as an operator */` |
|    16078 |   92 | `					pToken->nType = PH7_TK_ID\|PH7_TK_OP;` |
|     8040 |   93 | `			}else{` |
|        - |   94 | `				/* We are dealing with a keyword [i.e: while,foreach,class...],save the keyword ID */` |
|  1104698 |   95 | `				pToken->nType = PH7_TK_KEYWORD;` |
|  1104698 |   96 | `				pToken->pUserData = SX_INT_TO_PTR(nKeyword);` |
|        - |   97 | `			}` |
|   560388 |   98 | `		}else{` |
|        - |   99 | `			/* A simple identifier */` |
|  2061972 |  100 | `			pToken->nType = PH7_TK_ID;` |
|        - |  101 | `		}` |
|  1591373 |  102 | `	}else{` |
|        - |  103 | `		sxi32 c;` |
|        - |  104 | `		/* Non-alpha stream */` |
|  5503936 |  105 | `		if( pStream->zText[0] == '#' \|\|` |
|  5465952 |  106 | `			( pStream->zText[0] == '/' &&  &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '/') ){` |
|     4008 |  107 | `				pStream->zText++;` |
|        - |  108 | `				/* Inline comments */` |
|   146910 |  109 | `				while( pStream->zText < pStream->zEnd && pStream->zText[0] != '\n' ){` |
|   142904 |  110 | `					pStream->zText++;` |
|        2 |  111 | `				}` |
|        - |  112 | `				/* Tell the upper-layer to ignore this token */` |
|     4008 |  113 | `				return SXERR_CONTINUE;` |
|  5461948 |  114 | `		}else if( pStream->zText[0] == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '*' ){` |
|    71894 |  115 | `			pStream->zText += 2;` |
|        - |  116 | `			/* Block comment */` |
|  2039090 |  117 | `			while( pStream->zText < pStream->zEnd ){` |
|  2039090 |  118 | `				if( pStream->zText[0] == '*' ){` |
|    71920 |  119 | `					if( &pStream->zText[1] >= pStream->zEnd \|\| pStream->zText[1] == '/'  ){` |
|    35948 |  120 | `						break;` |
|        - |  121 | `					}` |
|       13 |  122 | `				}` |
|  1967198 |  123 | `				if( pStream->zText[0] == '\n' ){` |
|       28 |  124 | `					pStream->nLine++;` |
|       13 |  125 | `				}` |
|  1967198 |  126 | `				pStream->zText++;` |
|        2 |  127 | `			}` |
|    71894 |  128 | `			pStream->zText += 2;` |
|        - |  129 | `			/* Tell the upper-layer to ignore this token */` |
|    71894 |  130 | `			return SXERR_CONTINUE;` |
|  5390056 |  131 | `		}else if( SyisDigit(pStream->zText[0]) ){` |
|   105748 |  132 | `			pStream->zText++;` |
|        - |  133 | `			/* PHP 7.4: handle underscore separator immediately following the first digit.` |
|        - |  134 | `			 * Check pStream->zText < pStream->zEnd BEFORE forming pStream->zText + 1 so` |
|        - |  135 | `			 * we never compute a pointer past one-past-end. */` |
|   105826 |  136 | `			if( pStream->zText < pStream->zEnd` |
|   105746 |  137 | `				&& pStream->zText[0] == '_'` |
|    52953 |  138 | `				&& pStream->zText + 1 < pStream->zEnd` |
|      160 |  139 | `				&& pStream->zText[1] < 0xc0` |
|      162 |  140 | `				&& SyisDigit(pStream->zText[1]) ){` |
|      151 |  141 | `				pStream->zText++; /* swallow underscore between two digits */` |
|       75 |  142 | `			}` |
|        - |  143 | `			/* Decimal digit stream (PHP 7.4: underscore separator allowed between two digits) */` |
|   116044 |  144 | `			while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|    10298 |  145 | `				pStream->zText++;` |
|    10382 |  146 | `				if( pStream->zText < pStream->zEnd` |
|    10296 |  147 | `					&& pStream->zText[0] == '_'` |
|     5234 |  148 | `					&& pStream->zText + 1 < pStream->zEnd` |
|      172 |  149 | `					&& pStream->zText[1] < 0xc0` |
|      174 |  150 | `					&& SyisDigit(pStream->zText[1]) ){` |
|      173 |  151 | `					pStream->zText++; /* swallow underscore between two digits */` |
|       86 |  152 | `				}` |
|        2 |  153 | `			}` |
|        - |  154 | `			/* Mark the token as integer until we encounter a real number */` |
|   105748 |  155 | `			pToken->nType = PH7_TK_INTEGER;` |
|   105748 |  156 | `			if( pStream->zText < pStream->zEnd ){` |
|   105748 |  157 | `				c = pStream->zText[0];` |
|   105748 |  158 | `				if( c == '.' ){` |
|        - |  159 | `					/* Real number (PHP 7.4: underscore separator allowed between two digits) */` |
|      484 |  160 | `					pStream->zText++;` |
|     1836 |  161 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|     1354 |  162 | `						pStream->zText++;` |
|     1358 |  163 | `						if( pStream->zText < pStream->zEnd` |
|     1352 |  164 | `							&& pStream->zText[0] == '_'` |
|      682 |  165 | `							&& pStream->zText + 1 < pStream->zEnd` |
|       12 |  166 | `							&& pStream->zText[1] < 0xc0` |
|       14 |  167 | `							&& SyisDigit(pStream->zText[1]) ){` |
|       13 |  168 | `							pStream->zText++;` |
|        6 |  169 | `						}` |
|        2 |  170 | `					}` |
|      484 |  171 | `					if( pStream->zText < pStream->zEnd ){` |
|      484 |  172 | `						c = pStream->zText[0];` |
|      484 |  173 | `						if( c=='e' \|\| c=='E' ){` |
|       29 |  174 | `							pStream->zText++;` |
|       29 |  175 | `							if( pStream->zText < pStream->zEnd ){` |
|       29 |  176 | `								c = pStream->zText[0];` |
|       35 |  177 | `								if( (c =='+' \|\| c=='-') && &pStream->zText[1] < pStream->zEnd  &&` |
|       15 |  178 | `									pStream->zText[1] < 0xc0 && SyisDigit(pStream->zText[1]) ){` |
|       15 |  179 | `										pStream->zText++;` |
|        7 |  180 | `								}` |
|       69 |  181 | `								while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|       41 |  182 | `									pStream->zText++;` |
|       44 |  183 | `									if( pStream->zText < pStream->zEnd` |
|       40 |  184 | `										&& pStream->zText[0] == '_'` |
|       24 |  185 | `										&& pStream->zText + 1 < pStream->zEnd` |
|        8 |  186 | `										&& pStream->zText[1] < 0xc0` |
|        9 |  187 | `										&& SyisDigit(pStream->zText[1]) ){` |
|        9 |  188 | `										pStream->zText++;` |
|        4 |  189 | `									}` |
|        1 |  190 | `								}` |
|       14 |  191 | `							}` |
|       14 |  192 | `						}` |
|      241 |  193 | `					}` |
|      484 |  194 | `					pToken->nType = PH7_TK_REAL;` |
|   105507 |  195 | `				}else if( c=='e' \|\| c=='E' ){` |
|       14 |  196 | `					SXUNUSED(pUserData); /* Prevent compiler warning */` |
|       14 |  197 | `					SXUNUSED(pCtxData);` |
|       29 |  198 | `					pStream->zText++;` |
|       29 |  199 | `					if( pStream->zText < pStream->zEnd ){` |
|       29 |  200 | `						c = pStream->zText[0];` |
|       31 |  201 | `						if( (c =='+' \|\| c=='-') && &pStream->zText[1] < pStream->zEnd  &&` |
|        7 |  202 | `							pStream->zText[1] < 0xc0 && SyisDigit(pStream->zText[1]) ){` |
|        7 |  203 | `								pStream->zText++;` |
|        3 |  204 | `						}` |
|       67 |  205 | `						while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|       39 |  206 | `							pStream->zText++;` |
|       40 |  207 | `							if( pStream->zText < pStream->zEnd` |
|       38 |  208 | `								&& pStream->zText[0] == '_'` |
|       21 |  209 | `								&& pStream->zText + 1 < pStream->zEnd` |
|        4 |  210 | `								&& pStream->zText[1] < 0xc0` |
|        5 |  211 | `								&& SyisDigit(pStream->zText[1]) ){` |
|        5 |  212 | `								pStream->zText++;` |
|        2 |  213 | `							}` |
|        1 |  214 | `						}` |
|       14 |  215 | `					}` |
|       29 |  216 | `					pToken->nType = PH7_TK_REAL;` |
|   105252 |  217 | `				}else if( c == 'x' \|\| c == 'X' ){` |
|        - |  218 | `					/* Hex digit stream (PHP 7.4: underscore separator allowed between two digits) */` |
|       74 |  219 | `					pStream->zText++;` |
|      370 |  220 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisHex(pStream->zText[0]) ){` |
|      297 |  221 | `						pStream->zText++;` |
|      320 |  222 | `						if( pStream->zText < pStream->zEnd` |
|      296 |  223 | `							&& pStream->zText[0] == '_'` |
|      172 |  224 | `							&& pStream->zText + 1 < pStream->zEnd` |
|       48 |  225 | `							&& pStream->zText[1] < 0xc0` |
|       49 |  226 | `							&& SyisHex(pStream->zText[1]) ){` |
|       49 |  227 | `							pStream->zText++;` |
|       24 |  228 | `						}` |
|        1 |  229 | `					}` |
|   105202 |  230 | `				}else if(c  == 'b' \|\| c == 'B' ){` |
|        - |  231 | `					/* Binary digit stream (PHP 7.4: underscore separator allowed between two digits) */` |
|      280 |  232 | `					pStream->zText++;` |
|     2702 |  233 | `					while( pStream->zText < pStream->zEnd && (pStream->zText[0] == '0' \|\| pStream->zText[0] == '1') ){` |
|     1523 |  234 | `						pStream->zText++;` |
|     1583 |  235 | `						if( pStream->zText < pStream->zEnd` |
|     1522 |  236 | `							&& pStream->zText[0] == '_'` |
|      830 |  237 | `							&& pStream->zText + 1 < pStream->zEnd` |
|      139 |  238 | `							&& (pStream->zText[1] == '0' \|\| pStream->zText[1] == '1') ){` |
|      139 |  239 | `							pStream->zText++;` |
|       69 |  240 | `						}` |
|        1 |  241 | `					}` |
|      139 |  242 | `				}` |
|    52873 |  243 | `			}` |
|        - |  244 | `			/* PHP 7.4: absorb a trailing malformed underscore run into the` |
|        - |  245 | `			 * numeric token so the compile phase can emit a PHP-compatible` |
|        - |  246 | `			 * "syntax error, unexpected identifier" parse error. Valid` |
|        - |  247 | `			 * separators were already consumed by the per-loop peek logic` |
|        - |  248 | `			 * above, so an underscore here is always misplaced. */` |
|   105748 |  249 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '_' ){` |
|       15 |  250 | `				pStream->zText++;` |
|       44 |  251 | `				while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0` |
|       46 |  252 | `					&& (SyisAlphaNum(pStream->zText[0]) \|\| pStream->zText[0] == '_') ){` |
|       17 |  253 | `					pStream->zText++;` |
|        1 |  254 | `				}` |
|        7 |  255 | `			}` |
|        - |  256 | `			/* Record token length */` |
|   105748 |  257 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|   105748 |  258 | `			return SXRET_OK;` |
|        - |  259 | `		}` |
|  5284310 |  260 | `		c = pStream->zText[0];` |
|  5284310 |  261 | `		pStream->zText++; /* Advance the stream cursor */` |
|        - |  262 | `		/* Assume we are dealing with an operator*/` |
|  5284310 |  263 | `		pToken->nType = PH7_TK_OP;` |
|  5284310 |  264 | `		switch(c){` |
|  1092524 |  265 | `		case '$': pToken->nType = PH7_TK_DOLLAR; break;` |
|   416276 |  266 | `		case '{': pToken->nType = PH7_TK_OCB;    break;` |
|   416262 |  267 | `		case '}': pToken->nType = PH7_TK_CCB;    break;` |
|   825088 |  268 | `		case '(': pToken->nType = PH7_TK_LPAREN; break;` |
|    77148 |  269 | `		case '[': pToken->nType \|= PH7_TK_OSB;   break; /* Bitwise operation here,since the square bracket token '['` |
|        - |  270 | `														 * is a potential operator [i.e: subscripting] */` |
|    77154 |  271 | `		case ']': pToken->nType = PH7_TK_CSB;    break;` |
|   412536 |  272 | `		case ')': {` |
|   825074 |  273 | `			SySet *pTokSet = pStream->pSet;` |
|        - |  274 | `			/* Assemble type cast operators [i.e: (int),(float),(bool)...] */` |
|   825074 |  275 | `			if( pTokSet->nUsed >= 2 ){` |
|        - |  276 | `				SyToken *pTmp;` |
|        - |  277 | `				/* Peek the last recongnized token */` |
|   825072 |  278 | `				pTmp = (SyToken *)SySetPeek(pTokSet);` |
|   825072 |  279 | `				if( pTmp->nType & PH7_TK_KEYWORD ){` |
|    15518 |  280 | `					sxi32 nID = SX_PTR_TO_INT(pTmp->pUserData);` |
|    15518 |  281 | `					if( (sxu32)nID & (PH7_TKWRD_ARRAY\|PH7_TKWRD_INT\|PH7_TKWRD_FLOAT\|PH7_TKWRD_STRING\|PH7_TKWRD_OBJECT\|PH7_TKWRD_BOOL\|PH7_TKWRD_UNSET) ){` |
|    15266 |  282 | `						pTmp = (SyToken *)SySetAt(pTokSet,pTokSet->nUsed - 2);` |
|    15266 |  283 | `						if( pTmp->nType & PH7_TK_LPAREN ){` |
|        - |  284 | `							/* Merge the three tokens '(' 'TYPE' ')' into a single one */` |
|    15182 |  285 | `							const char * zTypeCast = "(int)";` |
|    15182 |  286 | `							if( nID & PH7_TKWRD_FLOAT ){` |
|     3002 |  287 | `								zTypeCast = "(float)";` |
|    13682 |  288 | `							}else if( nID & PH7_TKWRD_BOOL ){` |
|     3002 |  289 | `								zTypeCast = "(bool)";` |
|    10682 |  290 | `							}else if( nID & PH7_TKWRD_STRING ){` |
|     5994 |  291 | `								zTypeCast = "(string)";` |
|     6186 |  292 | `							}else if( nID & PH7_TKWRD_ARRAY ){` |
|       21 |  293 | `								zTypeCast = "(array)";` |
|     3180 |  294 | `							}else if( nID & PH7_TKWRD_OBJECT ){` |
|       17 |  295 | `								zTypeCast = "(object)";` |
|     3162 |  296 | `							}else if( nID & PH7_TKWRD_UNSET ){` |
|        7 |  297 | `								zTypeCast = "(unset)";` |
|        3 |  298 | `							}` |
|        - |  299 | `							/* Reflect the change */` |
|    15182 |  300 | `							pToken->nType = PH7_TK_OP;` |
|    15182 |  301 | `							SyStringInitFromBuf(&pToken->sData,zTypeCast,SyStrlen(zTypeCast));` |
|        - |  302 | `							/* Save the instance associated with the type cast operator */` |
|    15182 |  303 | `							pToken->pUserData = (void *)PH7_ExprExtractOperator(&pToken->sData,0);` |
|        - |  304 | `							/* Remove the two previous tokens */` |
|    15182 |  305 | `							pTokSet->nUsed -= 2;` |
|    15182 |  306 | `							return SXRET_OK;` |
|        - |  307 | `						}` |
|       42 |  308 | `					}` |
|      168 |  309 | `				}` |
|   404945 |  310 | `			}` |
|   809894 |  311 | `			pToken->nType = PH7_TK_RPAREN;` |
|   809894 |  312 | `			break;` |
|        - |  313 | `				  }` |
|    38063 |  314 | `		case '\'':{` |
|        - |  315 | `			/* Single quoted string */` |
|    76128 |  316 | `			pStr->zString++;` |
|   778774 |  317 | `			while( pStream->zText < pStream->zEnd ){` |
|   778774 |  318 | `				if( pStream->zText[0] == '\''  ){` |
|    76138 |  319 | `					if( pStream->zText[-1] != '\\' ){` |
|    76114 |  320 | `						break;` |
|      ! 0 |  321 | `					}else{` |
|       25 |  322 | `						const unsigned char *zPtr = &pStream->zText[-2];` |
|       25 |  323 | `						sxi32 i = 1;` |
|       43 |  324 | `						while( zPtr > pStream->zInput && zPtr[0] == '\\' ){` |
|       19 |  325 | `							zPtr--;` |
|       19 |  326 | `							i++;` |
|        1 |  327 | `						}` |
|       25 |  328 | `						if((i&1)==0){` |
|       15 |  329 | `							break;` |
|        - |  330 | `						}` |
|        - |  331 | `					}` |
|        5 |  332 | `				}` |
|   702648 |  333 | `				if( pStream->zText[0] == '\n' ){` |
|       67 |  334 | `					pStream->nLine++;` |
|       33 |  335 | `				}` |
|   702648 |  336 | `				pStream->zText++;` |
|        2 |  337 | `			}` |
|        - |  338 | `			/* Record token length and type */` |
|    76128 |  339 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|    76128 |  340 | `			pToken->nType = PH7_TK_SSTR;` |
|        - |  341 | `			/* Jump the trailing single quote */` |
|    76128 |  342 | `			pStream->zText++;` |
|    76128 |  343 | `			return SXRET_OK;` |
|        - |  344 | `				  }` |
|     9354 |  345 | `		case '"':{` |
|        - |  346 | `			sxi32 iNest;` |
|        - |  347 | `			/* Double quoted string */` |
|    18710 |  348 | `			pStr->zString++;` |
|   171288 |  349 | `			while( pStream->zText < pStream->zEnd ){` |
|   171288 |  350 | `				if( pStream->zText[0] == '{' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '$'){` |
|      106 |  351 | `					iNest = 1;` |
|      106 |  352 | `					pStream->zText++;` |
|        - |  353 | `					/* TICKET 1433-40: Hnadle braces'{}' in double quoted string where everything is allowed */` |
|     1118 |  354 | `					while(pStream->zText < pStream->zEnd ){` |
|     1118 |  355 | `						if( pStream->zText[0] == '{' ){` |
|        9 |  356 | `							iNest++;` |
|     1114 |  357 | `						}else if (pStream->zText[0] == '}' ){` |
|      114 |  358 | `							iNest--;` |
|      114 |  359 | `							if( iNest <= 0 ){` |
|      106 |  360 | `								pStream->zText++;` |
|      106 |  361 | `								break;` |
|        1 |  362 | `							}` |
|     1002 |  363 | `						}else if( pStream->zText[0] == '\n' ){` |
|      ! 0 |  364 | `							pStream->nLine++;` |
|      ! 0 |  365 | `						}` |
|     1014 |  366 | `						pStream->zText++;` |
|        2 |  367 | `					}` |
|      106 |  368 | `					if( pStream->zText >= pStream->zEnd ){` |
|      ! 0 |  369 | `						break;` |
|        - |  370 | `					}` |
|       52 |  371 | `				}` |
|   171288 |  372 | `				if( pStream->zText[0] == '"' ){` |
|    18818 |  373 | `					if( pStream->zText[-1] != '\\' ){` |
|    18706 |  374 | `						break;` |
|      ! 0 |  375 | `					}else{` |
|      114 |  376 | `						const unsigned char *zPtr = &pStream->zText[-2];` |
|      114 |  377 | `						sxi32 i = 1;` |
|      166 |  378 | `						while( zPtr > pStream->zInput && zPtr[0] == '\\' ){` |
|       54 |  379 | `							zPtr--;` |
|       54 |  380 | `							i++;` |
|        2 |  381 | `						}` |
|      114 |  382 | `						if((i&1)==0){` |
|        5 |  383 | `							break;` |
|        - |  384 | `						}` |
|        - |  385 | `					}` |
|       54 |  386 | `				}` |
|   152580 |  387 | `				if( pStream->zText[0] == '\n' ){` |
|        7 |  388 | `					pStream->nLine++;` |
|        3 |  389 | `				}` |
|   152580 |  390 | `				pStream->zText++;` |
|        2 |  391 | `			}` |
|        - |  392 | `			/* Record token length and type */` |
|    18710 |  393 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|    18710 |  394 | `			pToken->nType = PH7_TK_DSTR;` |
|        - |  395 | `			/* Jump the trailing quote */` |
|    18710 |  396 | `			pStream->zText++;` |
|    18710 |  397 | `			return SXRET_OK;` |
|        - |  398 | `				  }` |
|        2 |  399 | ``		case '`':{`` |
|        - |  400 | `			/* Backtick quoted string */` |
|        5 |  401 | `			pStr->zString++;` |
|       45 |  402 | `			while( pStream->zText < pStream->zEnd ){` |
|       45 |  403 | ``				if( pStream->zText[0] == '`' && pStream->zText[-1] != '\\' ){`` |
|        5 |  404 | `					break;` |
|        - |  405 | `				}` |
|       41 |  406 | `				if( pStream->zText[0] == '\n' ){` |
|      ! 0 |  407 | `					pStream->nLine++;` |
|      ! 0 |  408 | `				}` |
|       41 |  409 | `				pStream->zText++;` |
|        1 |  410 | `			}` |
|        - |  411 | `			/* Record token length and type */` |
|        5 |  412 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|        5 |  413 | `			pToken->nType = PH7_TK_BSTR;` |
|        - |  414 | `			/* Jump the trailing backtick */` |
|        5 |  415 | `			pStream->zText++;` |
|        5 |  416 | `			return SXRET_OK;` |
|        - |  417 | `				  }` |
|      220 |  418 | `		case '\\': pToken->nType = PH7_TK_NSSEP;  break;` |
|     1545 |  419 | `		case ':':` |
|     3092 |  420 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == ':' ){` |
|        - |  421 | `				/* Current operator: '::' */` |
|      242 |  422 | `				pStream->zText++;` |
|      122 |  423 | `			}else{` |
|     2852 |  424 | `				pToken->nType = PH7_TK_COLON; /* Single colon */` |
|        - |  425 | `			}` |
|     3092 |  426 | `			break;` |
|    87978 |  427 | `		case ',': pToken->nType \|= PH7_TK_COMMA;  break; /* Comma is also an operator */` |
|   627830 |  428 | `		case ';': pToken->nType = PH7_TK_SEMI;    break;` |
|        - |  429 | `			/* Handle combined operators [i.e: +=,===,!=== ...] */` |
|   173651 |  430 | `		case '=':` |
|   347304 |  431 | `			pToken->nType \|= PH7_TK_EQUAL;` |
|   347304 |  432 | `			if( pStream->zText < pStream->zEnd ){` |
|   347304 |  433 | `				if( pStream->zText[0] == '=' ){` |
|    19518 |  434 | `					pToken->nType &= ~PH7_TK_EQUAL;` |
|        - |  435 | `					/* Current operator: == */` |
|    19518 |  436 | `					pStream->zText++;` |
|    19518 |  437 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  438 | `						/* Current operator: === */` |
|     4244 |  439 | `						pStream->zText++;` |
|     2123 |  440 | `					}` |
|   337546 |  441 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  442 | `					/* Array operator: => */` |
|     4678 |  443 | `					pToken->nType = PH7_TK_ARRAY_OP;` |
|     4678 |  444 | `					pStream->zText++;` |
|     2340 |  445 | `				}else{` |
|        - |  446 | `					/* TICKET 1433-0010: Reference operator '=&' */` |
|   323112 |  447 | `					const unsigned char *zCur = pStream->zText;` |
|   323112 |  448 | `					sxu32 nLine = 0;` |
|   646170 |  449 | `					while( zCur < pStream->zEnd && zCur[0] < 0xc0 && SyisSpace(zCur[0]) ){` |
|   323060 |  450 | `						if( zCur[0] == '\n' ){` |
|        5 |  451 | `							nLine++;` |
|        2 |  452 | `						}` |
|   323060 |  453 | `						zCur++;` |
|        2 |  454 | `					}` |
|   323112 |  455 | `					if( zCur < pStream->zEnd && zCur[0] == '&' ){` |
|        - |  456 | `						/* Current operator: =& */` |
|       50 |  457 | `						pToken->nType &= ~PH7_TK_EQUAL;` |
|       50 |  458 | `						SyStringInitFromBuf(pStr,"=&",sizeof("=&")-1);` |
|        - |  459 | `						/* Update token stream */` |
|       50 |  460 | `						pStream->zText = &zCur[1];` |
|       50 |  461 | `						pStream->nLine += nLine;` |
|       24 |  462 | `					}` |
|        - |  463 | `				}` |
|   173651 |  464 | `			}` |
|   347304 |  465 | `			break;` |
|    21321 |  466 | `		case '!':` |
|    42644 |  467 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  468 | `				/* Current operator: != */` |
|    18142 |  469 | `				pStream->zText++;` |
|    18142 |  470 | `				if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  471 | `					/* Current operator: !== */` |
|    15118 |  472 | `					pStream->zText++;` |
|     7558 |  473 | `				}` |
|     9070 |  474 | `			}` |
|    42644 |  475 | `			break;` |
|    12241 |  476 | `		case '&':` |
|    24484 |  477 | `			pToken->nType \|= PH7_TK_AMPER;` |
|    24484 |  478 | `			if( pStream->zText < pStream->zEnd ){` |
|    24484 |  479 | `				if( pStream->zText[0] == '&' ){` |
|     9392 |  480 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|        - |  481 | `					/* Current operator: && */` |
|     9392 |  482 | `					pStream->zText++;` |
|    19789 |  483 | `				}else if( pStream->zText[0] == '=' ){` |
|        7 |  484 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|        - |  485 | `					/* Current operator: &= */` |
|        7 |  486 | `					pStream->zText++;` |
|        3 |  487 | `				}` |
|    12241 |  488 | `			}` |
|    24484 |  489 | `			break;` |
|     1638 |  490 | `		case '\|':` |
|     3278 |  491 | `			if( pStream->zText < pStream->zEnd ){` |
|     3278 |  492 | `				if( pStream->zText[0] == '\|' ){` |
|        - |  493 | `					/* Current operator: \|\| */` |
|     3142 |  494 | `					pStream->zText++;` |
|     1708 |  495 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  496 | `					/* Current operator: \|= */` |
|        7 |  497 | `					pStream->zText++;` |
|        3 |  498 | `				}` |
|     1638 |  499 | `			}` |
|     3278 |  500 | `			break;` |
|     7905 |  501 | `		case '+':` |
|    15812 |  502 | `			if( pStream->zText < pStream->zEnd ){` |
|    15810 |  503 | `				if( pStream->zText[0] == '+' ){` |
|        - |  504 | `					/* Current operator: ++ */` |
|    12296 |  505 | `					pStream->zText++;` |
|     9663 |  506 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  507 | `					/* Current operator: += */` |
|       48 |  508 | `					pStream->zText++;` |
|       23 |  509 | `				}` |
|     7904 |  510 | `			}` |
|    15812 |  511 | `			break;` |
|    81845 |  512 | `		case '-':` |
|   163692 |  513 | `			if( pStream->zText < pStream->zEnd ){` |
|   163692 |  514 | `				if( pStream->zText[0] == '-' ){` |
|        - |  515 | `					/* Current operator: -- */` |
|        5 |  516 | `					pStream->zText++;` |
|   163690 |  517 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  518 | `					/* Current operator: -= */` |
|       10 |  519 | `					pStream->zText++;` |
|   163684 |  520 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  521 | `					/* Current operator: -> */` |
|   163136 |  522 | `					pStream->zText++;` |
|    81567 |  523 | `				}` |
|    81845 |  524 | `			}` |
|   163692 |  525 | `			break;` |
|      168 |  526 | `		case '*':` |
|      338 |  527 | `			if( pStream->zText < pStream->zEnd ){` |
|      338 |  528 | `				if( pStream->zText[0] == '*' ){` |
|        - |  529 | `					/* Current operator: ** or **= */` |
|      133 |  530 | `					pStream->zText++;` |
|      133 |  531 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  532 | `						/* Current operator: **= */` |
|       23 |  533 | `						pStream->zText++;` |
|       12 |  534 | `					}` |
|      272 |  535 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  536 | `					/* Current operator: *= */` |
|       20 |  537 | `					pStream->zText++;` |
|        9 |  538 | `				}` |
|      168 |  539 | `			}` |
|      338 |  540 | `			break;` |
|       33 |  541 | `		case '/':` |
|       68 |  542 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  543 | `				/* Current operator: /= */` |
|        5 |  544 | `				pStream->zText++;` |
|        2 |  545 | `			}` |
|       68 |  546 | `			break;` |
|       26 |  547 | `		case '%':` |
|       54 |  548 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  549 | `				/* Current operator: %= */` |
|        3 |  550 | `				pStream->zText++;` |
|        1 |  551 | `			}` |
|       54 |  552 | `			break;` |
|       11 |  553 | `		case '^':` |
|       23 |  554 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  555 | `				/* Current operator: ^= */` |
|        9 |  556 | `				pStream->zText++;` |
|        4 |  557 | `			}` |
|       23 |  558 | `			break;` |
|    41364 |  559 | `		case '.':` |
|    82730 |  560 | `			if( pStream->zText + 1 < pStream->zEnd && pStream->zText[0] == '.' && pStream->zText[1] == '.' ){` |
|        - |  561 | `				/* Ellipsis: ... */` |
|       60 |  562 | `				pStream->zText += 2;` |
|       60 |  563 | `				pToken->nType = PH7_TK_ELLIPSIS;` |
|    82701 |  564 | `			}else if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  565 | `				/* Current operator: .= */` |
|     3064 |  566 | `				pStream->zText++;` |
|     1531 |  567 | `			}` |
|    82730 |  568 | `			break;` |
|    25708 |  569 | `		case '<':` |
|    51418 |  570 | `			if( pStream->zText < pStream->zEnd ){` |
|    51418 |  571 | `				if( pStream->zText[0] == '<' ){` |
|        - |  572 | `					/* Current operator: << */` |
|      134 |  573 | `					pStream->zText++;` |
|      134 |  574 | `					if( pStream->zText < pStream->zEnd ){` |
|      134 |  575 | `						if( pStream->zText[0] == '=' ){` |
|        - |  576 | `							/* Current operator: <<= */` |
|        9 |  577 | `							pStream->zText++;` |
|      130 |  578 | `						}else if( pStream->zText[0] == '<' ){` |
|        - |  579 | `							/* Current Token: <<<  */` |
|      112 |  580 | `							pStream->zText++;` |
|        - |  581 | `							/* This may be the beginning of a Heredoc/Nowdoc string,try to delimit it */` |
|      112 |  582 | `							rc = LexExtractHeredoc(&(*pStream),&(*pToken));` |
|      112 |  583 | `							if( rc == SXRET_OK ){` |
|        - |  584 | `								/* Here/Now doc successfuly extracted */` |
|      112 |  585 | `								return SXRET_OK;` |
|        - |  586 | `							}` |
|      ! 0 |  587 | `						}` |
|       12 |  588 | `					}` |
|    51297 |  589 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  590 | `					/* Current operator: <> */` |
|        5 |  591 | `					pStream->zText++;` |
|    51284 |  592 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  593 | `					/* Current operator: <= or <=> */` |
|      100 |  594 | `					pStream->zText++;` |
|      100 |  595 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '>' ){` |
|        - |  596 | `						/* Current operator: <=> */` |
|       51 |  597 | `						pStream->zText++;` |
|       25 |  598 | `					}` |
|       49 |  599 | `				}` |
|    25653 |  600 | `			}` |
|    51308 |  601 | `			break;` |
|     3108 |  602 | `		case '>':` |
|     6218 |  603 | `			if( pStream->zText < pStream->zEnd ){` |
|     6218 |  604 | `				if( pStream->zText[0] == '>' ){` |
|        - |  605 | `					/* Current operator: >> */` |
|       21 |  606 | `					pStream->zText++;` |
|       21 |  607 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  608 | `						/* Current operator: >>= */` |
|       11 |  609 | `						pStream->zText++;` |
|        6 |  610 | `					}` |
|     6208 |  611 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  612 | `					/* Current operator: >= */` |
|       84 |  613 | `					pStream->zText++;` |
|       41 |  614 | `				}` |
|     3108 |  615 | `			}` |
|     6218 |  616 | `			break;` |
|     1294 |  617 | `		case '?':` |
|     2590 |  618 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '?' ){` |
|        - |  619 | `				/* Null coalescing operator: ?? */` |
|      136 |  620 | `				pStream->zText++;` |
|      136 |  621 | `				if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  622 | `					/* Null coalescing assignment operator (PHP 7.4) */` |
|       54 |  623 | `					pStream->zText++;` |
|       26 |  624 | `				}` |
|     2577 |  625 | `			}else if( (pStream->zEnd - pStream->zText) >= 2` |
|     2456 |  626 | `				&& pStream->zText[0] == '-' && pStream->zText[1] == '>' ){` |
|        - |  627 | `				/* Nullsafe object operator (PHP 8.0): ?-> */` |
|      110 |  628 | `				pStream->zText += 2;` |
|       54 |  629 | `			}` |
|     2588 |  630 | `			break;` |
|      110 |  631 | `		default:` |
|      220 |  632 | `			break;` |
|        - |  633 | `		}` |
|  5174182 |  634 | `		if( pStr->nByte <= 0 ){` |
|        - |  635 | `			/* Record token length */` |
|  5174134 |  636 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  2587066 |  637 | `		}` |
|  5174182 |  638 | `		if( pToken->nType & PH7_TK_OP ){` |
|        - |  639 | `			const ph7_expr_op *pOp;` |
|        - |  640 | `			/* Check if the extracted token is an operator */` |
|   901366 |  641 | `			pOp = PH7_ExprExtractOperator(pStr,(SyToken *)SySetPeek(pStream->pSet));` |
|   901366 |  642 | `			if( pOp == 0 ){` |
|        - |  643 | `				/* Not an operator */` |
|      ! 0 |  644 | `				pToken->nType &= ~PH7_TK_OP;` |
|      ! 0 |  645 | `				if( pToken->nType <= 0 ){` |
|      ! 0 |  646 | `					pToken->nType = PH7_TK_OTHER;` |
|      ! 0 |  647 | `				}` |
|      ! 0 |  648 | `			}else{` |
|        - |  649 | `				/* Save the instance associated with this operator for later processing */` |
|   901366 |  650 | `				pToken->pUserData = (void *)pOp;` |
|        - |  651 | `			}` |
|   450682 |  652 | `		}` |
|        - |  653 | `	}` |
|        - |  654 | `	/* Tell the upper-layer to save the extracted token for later processing */` |
|  8356924 |  655 | `	return SXRET_OK;` |
|  4324349 |  656 |  |
|        - |  657 | `/***** This file contains automatically generated code ******` |
|        - |  658 | `**` |
|        - |  659 | `** The code in this file has been automatically generated by` |
|        - |  660 | `**` |
|        - |  661 | `**     $Header: /sqlite/sqlite/tool/mkkeywordhash.c` |
|        - |  662 | `**` |
|        - |  663 | `** Sligthly modified by Chems mrad <chm@symisc.net> for the PH7 engine.` |
|        - |  664 | `**` |
|        - |  665 | `** The code in this file implements a function that determines whether` |
|        - |  666 | `** or not a given identifier is really a PHP keyword.  The same thing` |
|        - |  667 | `** might be implemented more directly using a hand-written hash table.` |
|        - |  668 | `** But by using this automatically generated code, the size of the code` |
|        - |  669 | `** is substantially reduced.  This is important for embedded applications` |
|        - |  670 | `** on platforms with limited memory.` |
|        - |  671 | `*/` |
|        - |  672 | `/* Hash score: 103 */` |
|  3182744 |  673 | `static sxu32 KeywordCode(const char *z, int n){` |
|        - |  674 | `  /* zText[] encodes 532 bytes of keywords in 333 bytes */` |
|        - |  675 | `  /*   extendswitchprintegerequire_oncenddeclareturnamespacechobject      */` |
|        - |  676 | `  /*   hrowbooleandefaultrycaselfinalistaticlonewconstringlobaluse        */` |
|        - |  677 | `  /*   lseifloatvarrayANDIEchoUSECHOabstractclasscontinuendifunction      */` |
|        - |  678 | `  /*   diendwhilevaldoexitgotoimplementsinclude_oncemptyinstanceof        */` |
|        - |  679 | `  /*   interfacendforeachissetparentprivateprotectedpublicatchunset       */` |
|        - |  680 | `  /*   xorARRAYASArrayEXITUNSETXORbreak                                   */` |
|        - |  681 | `  static const char zText[332] = {` |
|        - |  682 | `    'e','x','t','e','n','d','s','w','i','t','c','h','p','r','i','n','t','e',` |
|        - |  683 | `    'g','e','r','e','q','u','i','r','e','_','o','n','c','e','n','d','d','e',` |
|        - |  684 | `    'c','l','a','r','e','t','u','r','n','a','m','e','s','p','a','c','e','c',` |
|        - |  685 | `    'h','o','b','j','e','c','t','h','r','o','w','b','o','o','l','e','a','n',` |
|        - |  686 | `    'd','e','f','a','u','l','t','r','y','c','a','s','e','l','f','i','n','a',` |
|        - |  687 | `    'l','i','s','t','a','t','i','c','l','o','n','e','w','c','o','n','s','t',` |
|        - |  688 | `    'r','i','n','g','l','o','b','a','l','u','s','e','l','s','e','i','f','l',` |
|        - |  689 | `    'o','a','t','v','a','r','r','a','y','A','N','D','I','E','c','h','o','U',` |
|        - |  690 | `    'S','E','C','H','O','a','b','s','t','r','a','c','t','c','l','a','s','s',` |
|        - |  691 | `    'c','o','n','t','i','n','u','e','n','d','i','f','u','n','c','t','i','o',` |
|        - |  692 | `    'n','d','i','e','n','d','w','h','i','l','e','v','a','l','d','o','e','x',` |
|        - |  693 | `    'i','t','g','o','t','o','i','m','p','l','e','m','e','n','t','s','i','n',` |
|        - |  694 | `    'c','l','u','d','e','_','o','n','c','e','m','p','t','y','i','n','s','t',` |
|        - |  695 | `    'a','n','c','e','o','f','i','n','t','e','r','f','a','c','e','n','d','f',` |
|        - |  696 | `    'o','r','e','a','c','h','i','s','s','e','t','p','a','r','e','n','t','p',` |
|        - |  697 | `    'r','i','v','a','t','e','p','r','o','t','e','c','t','e','d','p','u','b',` |
|        - |  698 | `    'l','i','c','a','t','c','h','u','n','s','e','t','x','o','r','A','R','R',` |
|        - |  699 | `    'A','Y','A','S','A','r','r','a','y','E','X','I','T','U','N','S','E','T',` |
|        - |  700 | `    'X','O','R','b','r','e','a','k'` |
|        - |  701 | `  };` |
|        - |  702 | `  static const unsigned char aHash[151] = {` |
|        - |  703 |  |
|        - |  704 |  |
|        - |  705 | `      74,   0,  51,   0,  76,   0,   0,  20,   0,   0,   0,  50,   0,` |
|        - |  706 | `      80,  34,   0,  36,   0,   0,  64,  16,   0,   0,  17,   0,   1,` |
|        - |  707 | `      19,  84,  66,   0,  43,  45,  78,   0,   0,  53,  56,   0,   0,` |
|        - |  708 |  |
|        - |  709 | `      72,  14,   0,  71,   0,  38,   6,   0,   0,   0,  73,   0,   0,` |
|        - |  710 | `       3,   0,  41,   5,  52,  57,  32,   0,  60,  63,   0,  69,  82,` |
|        - |  711 | `      30,   0,  79,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,` |
|        - |  712 |  |
|        - |  713 | `      62,   0,  11,   0,   0,  58,   0,   0,   0,   0,  59,  75,   0,` |
|        - |  714 |  |
|        - |  715 | `  };` |
|        - |  716 | `  static const unsigned char aNext[84] = {` |
|        - |  717 |  |
|        - |  718 |  |
|        - |  719 |  |
|        - |  720 |  |
|        - |  721 |  |
|        - |  722 |  |
|        - |  723 | `      42,   0,   0,   0,  70,  55` |
|        - |  724 | `  };` |
|        - |  725 | `  static const unsigned char aLen[84] = {` |
|        - |  726 | `       7,   9,   6,   5,   7,  12,   7,   2,  10,   7,   6,   9,   4,` |
|        - |  727 | `       6,   5,   7,   4,   3,   7,   3,   4,   4,   5,   4,   6,   5,` |
|        - |  728 | `       2,   3,   5,   6,   6,   3,   6,   4,   2,   5,   3,   5,   3,` |
|        - |  729 | `       3,   4,   3,   4,   8,   5,   2,   8,   5,   8,   3,   8,   5,` |
|        - |  730 | `       4,   2,   4,   4,  10,  12,   7,   5,  10,   9,   3,   6,  10,` |
|        - |  731 | `       3,   7,   2,   5,   6,   7,   9,   6,   5,   5,   3,   5,   2,` |
|        - |  732 | `       5,   4,   5,   3,   2,   5` |
|        - |  733 | `  };` |
|        - |  734 | `  static const sxu16 aOffset[84] = {` |
|        - |  735 |  |
|        - |  736 | `      55,  60,  65,  65,  70,  72,  78,  81,  83,  86,  90,  92,  97,` |
|        - |  737 | `     100, 100, 103, 106, 111, 117, 119, 119, 123, 124, 129, 130, 135,` |
|        - |  738 | `     137, 139, 143, 145, 149, 157, 159, 162, 169, 173, 181, 183, 186,` |
|        - |  739 | `     190, 194, 196, 200, 204, 214, 214, 225, 230, 240, 240, 248, 248,` |
|        - |  740 | `     251, 251, 252, 258, 263, 269, 276, 285, 290, 295, 300, 303, 308,` |
|        - |  741 | `     310, 315, 319, 324, 325, 327` |
|        - |  742 | `  };` |
|        - |  743 | `  static const sxu32 aCode[84] = {` |
|        - |  744 | `    PH7_TKWRD_EXTENDS,   PH7_TKWRD_ENDSWITCH,   PH7_TKWRD_SWITCH,    PH7_TKWRD_PRINT,   PH7_TKWRD_INT,` |
|        - |  745 | `    PH7_TKWRD_REQONCE,   PH7_TKWRD_REQUIRE,     PH7_TKWRD_SEQ,       PH7_TKWRD_ENDDEC,    PH7_TKWRD_DECLARE,` |
|        - |  746 | `    PH7_TKWRD_RETURN,    PH7_TKWRD_NAMESPACE,   PH7_TKWRD_ECHO,      PH7_TKWRD_OBJECT,    PH7_TKWRD_THROW,` |
|        - |  747 | `    PH7_TKWRD_BOOL,      PH7_TKWRD_BOOL,        PH7_TKWRD_AND,       PH7_TKWRD_DEFAULT,   PH7_TKWRD_TRY,` |
|        - |  748 | `    PH7_TKWRD_CASE,      PH7_TKWRD_SELF,        PH7_TKWRD_FINAL,     PH7_TKWRD_LIST,      PH7_TKWRD_STATIC,` |
|        - |  749 | `    PH7_TKWRD_CLONE,     PH7_TKWRD_SNE,         PH7_TKWRD_NEW,       PH7_TKWRD_CONST,     PH7_TKWRD_STRING,` |
|        - |  750 | `    PH7_TKWRD_GLOBAL,    PH7_TKWRD_USE,         PH7_TKWRD_ELIF,      PH7_TKWRD_ELSE,      PH7_TKWRD_IF,` |
|        - |  751 | `    PH7_TKWRD_FLOAT,     PH7_TKWRD_VAR,         PH7_TKWRD_ARRAY,     PH7_TKWRD_AND,       PH7_TKWRD_DIE,` |
|        - |  752 | `    PH7_TKWRD_ECHO,      PH7_TKWRD_USE,         PH7_TKWRD_ECHO,      PH7_TKWRD_ABSTRACT,  PH7_TKWRD_CLASS,` |
|        - |  753 | `    PH7_TKWRD_AS,        PH7_TKWRD_CONTINUE,    PH7_TKWRD_ENDIF,     PH7_TKWRD_FUNCTION,  PH7_TKWRD_DIE,` |
|        - |  754 | `    PH7_TKWRD_ENDWHILE,  PH7_TKWRD_WHILE,       PH7_TKWRD_EVAL,      PH7_TKWRD_DO,        PH7_TKWRD_EXIT,` |
|        - |  755 | `    PH7_TKWRD_GOTO,      PH7_TKWRD_IMPLEMENTS,  PH7_TKWRD_INCONCE,   PH7_TKWRD_INCLUDE,   PH7_TKWRD_EMPTY,` |
|        - |  756 | `    PH7_TKWRD_INSTANCEOF,PH7_TKWRD_INTERFACE,   PH7_TKWRD_INT,       PH7_TKWRD_ENDFOR,    PH7_TKWRD_END4EACH,` |
|        - |  757 | `    PH7_TKWRD_FOR,       PH7_TKWRD_FOREACH,     PH7_TKWRD_OR,        PH7_TKWRD_ISSET,     PH7_TKWRD_PARENT,` |
|        - |  758 | `    PH7_TKWRD_PRIVATE,   PH7_TKWRD_PROTECTED,   PH7_TKWRD_PUBLIC,    PH7_TKWRD_CATCH,     PH7_TKWRD_UNSET,` |
|        - |  759 | `    PH7_TKWRD_XOR,       PH7_TKWRD_ARRAY,       PH7_TKWRD_AS,        PH7_TKWRD_ARRAY,     PH7_TKWRD_EXIT,` |
|        - |  760 | `    PH7_TKWRD_UNSET,     PH7_TKWRD_XOR,         PH7_TKWRD_OR,        PH7_TKWRD_BREAK` |
|        - |  761 | `  };` |
|        - |  762 | `  int h, i;` |
|  3182744 |  763 | `  if( n<2 ) return PH7_TK_ID;` |
|  3079346 |  764 | `  h = (((int)z[0]*4) ^ ((int)z[n-1]*3) ^ n) % 151;` |
|  4711170 |  765 | `  for(i=((int)aHash[h])-1; i>=0; i=((int)aNext[i])-1){` |
|  2752044 |  766 | `    if( (int)aLen[i]==n && SyMemcmp(&zText[aOffset[i]],z,n)==0 ){` |
|        - |  767 | `       /* PH7_TKWRD_EXTENDS */` |
|        - |  768 | `       /* PH7_TKWRD_ENDSWITCH */` |
|        - |  769 | `       /* PH7_TKWRD_SWITCH */` |
|        - |  770 | `       /* PH7_TKWRD_PRINT */` |
|        - |  771 | `       /* PH7_TKWRD_INT */` |
|        - |  772 | `       /* PH7_TKWRD_REQONCE */` |
|        - |  773 | `       /* PH7_TKWRD_REQUIRE */` |
|        - |  774 | `       /* PH7_TKWRD_SEQ */` |
|        - |  775 | `       /* PH7_TKWRD_ENDDEC */` |
|        - |  776 | `       /* PH7_TKWRD_DECLARE */` |
|        - |  777 | `       /* PH7_TKWRD_RETURN */` |
|        - |  778 | `       /* PH7_TKWRD_NAMESPACE */` |
|        - |  779 | `       /* PH7_TKWRD_ECHO */` |
|        - |  780 | `       /* PH7_TKWRD_OBJECT */` |
|        - |  781 | `       /* PH7_TKWRD_THROW */` |
|        - |  782 | `       /* PH7_TKWRD_BOOL */` |
|        - |  783 | `       /* PH7_TKWRD_BOOL */` |
|        - |  784 | `       /* PH7_TKWRD_AND */` |
|        - |  785 | `       /* PH7_TKWRD_DEFAULT */` |
|        - |  786 | `       /* PH7_TKWRD_TRY */` |
|        - |  787 | `       /* PH7_TKWRD_CASE */` |
|        - |  788 | `       /* PH7_TKWRD_SELF */` |
|        - |  789 | `       /* PH7_TKWRD_FINAL */` |
|        - |  790 | `       /* PH7_TKWRD_LIST */` |
|        - |  791 | `       /* PH7_TKWRD_STATIC */` |
|        - |  792 | `       /* PH7_TKWRD_CLONE */` |
|        - |  793 | `       /* PH7_TKWRD_SNE */` |
|        - |  794 | `       /* PH7_TKWRD_NEW */` |
|        - |  795 | `       /* PH7_TKWRD_CONST */` |
|        - |  796 | `       /* PH7_TKWRD_STRING */` |
|        - |  797 | `       /* PH7_TKWRD_GLOBAL */` |
|        - |  798 | `       /* PH7_TKWRD_USE */` |
|        - |  799 | `       /* PH7_TKWRD_ELIF */` |
|        - |  800 | `       /* PH7_TKWRD_ELSE */` |
|        - |  801 | `       /* PH7_TKWRD_IF */` |
|        - |  802 | `       /* PH7_TKWRD_FLOAT */` |
|        - |  803 | `       /* PH7_TKWRD_VAR */` |
|        - |  804 | `       /* PH7_TKWRD_ARRAY */` |
|        - |  805 | `       /* PH7_TKWRD_AND */` |
|        - |  806 | `       /* PH7_TKWRD_DIE */` |
|        - |  807 | `       /* PH7_TKWRD_ECHO */` |
|        - |  808 | `       /* PH7_TKWRD_USE */` |
|        - |  809 | `       /* PH7_TKWRD_ECHO */` |
|        - |  810 | `       /* PH7_TKWRD_ABSTRACT */` |
|        - |  811 | `       /* PH7_TKWRD_CLASS */` |
|        - |  812 | `       /* PH7_TKWRD_AS */` |
|        - |  813 | `       /* PH7_TKWRD_CONTINUE */` |
|        - |  814 | `       /* PH7_TKWRD_ENDIF */` |
|        - |  815 | `       /* PH7_TKWRD_FUNCTION */` |
|        - |  816 | `       /* PH7_TKWRD_DIE */` |
|        - |  817 | `       /* PH7_TKWRD_ENDWHILE */` |
|        - |  818 | `       /* PH7_TKWRD_WHILE */` |
|        - |  819 | `       /* PH7_TKWRD_EVAL */` |
|        - |  820 | `       /* PH7_TKWRD_DO */` |
|        - |  821 | `       /* PH7_TKWRD_EXIT */` |
|        - |  822 | `       /* PH7_TKWRD_GOTO */` |
|        - |  823 | `       /* PH7_TKWRD_IMPLEMENTS */` |
|        - |  824 | `       /* PH7_TKWRD_INCONCE */` |
|        - |  825 | `       /* PH7_TKWRD_INCLUDE */` |
|        - |  826 | `       /* PH7_TKWRD_EMPTY */` |
|        - |  827 | `       /* PH7_TKWRD_INSTANCEOF */` |
|        - |  828 | `       /* PH7_TKWRD_INTERFACE */` |
|        - |  829 | `       /* PH7_TKWRD_INT */` |
|        - |  830 | `       /* PH7_TKWRD_ENDFOR */` |
|        - |  831 | `       /* PH7_TKWRD_END4EACH */` |
|        - |  832 | `       /* PH7_TKWRD_FOR */` |
|        - |  833 | `       /* PH7_TKWRD_FOREACH */` |
|        - |  834 | `       /* PH7_TKWRD_OR */` |
|        - |  835 | `       /* PH7_TKWRD_ISSET */` |
|        - |  836 | `       /* PH7_TKWRD_PARENT */` |
|        - |  837 | `       /* PH7_TKWRD_PRIVATE */` |
|        - |  838 | `       /* PH7_TKWRD_PROTECTED */` |
|        - |  839 | `       /* PH7_TKWRD_PUBLIC */` |
|        - |  840 | `       /* PH7_TKWRD_CATCH */` |
|        - |  841 | `       /* PH7_TKWRD_UNSET */` |
|        - |  842 | `       /* PH7_TKWRD_XOR */` |
|        - |  843 | `       /* PH7_TKWRD_ARRAY */` |
|        - |  844 | `       /* PH7_TKWRD_AS */` |
|        - |  845 | `       /* PH7_TKWRD_ARRAY */` |
|        - |  846 | `       /* PH7_TKWRD_EXIT */` |
|        - |  847 | `       /* PH7_TKWRD_UNSET */` |
|        - |  848 | `       /* PH7_TKWRD_XOR */` |
|        - |  849 | `       /* PH7_TKWRD_OR */` |
|        - |  850 | `       /* PH7_TKWRD_BREAK */` |
|  1120220 |  851 | `      return aCode[i];` |
|        - |  852 | `    }` |
|   815912 |  853 | `  }` |
|        - |  854 | `  /* Linear fallback for keywords not in the auto-generated hash table */` |
|  1959128 |  855 | `  if( n==5 && SyMemcmp(z,"trait",5)==0 ) return PH7_TKWRD_TRAIT;` |
|  1959072 |  856 | `  if( n==9 && SyMemcmp(z,"insteadof",9)==0 ) return PH7_TKWRD_INSTEADOF;` |
|  1959068 |  857 | `  if( n==7 && SyMemcmp(z,"finally",7)==0 ) return PH7_TKWRD_FINALLY;` |
|  1959038 |  858 | `  if( n==5 && SyMemcmp(z,"yield",5)==0 ) return PH7_TKWRD_YIELD;` |
|  1959004 |  859 | `  if( n==5 && SyMemcmp(z,"match",5)==0 ) return PH7_TKWRD_MATCH;` |
|  1958934 |  860 | `  return PH7_TK_ID;` |
|  1591373 |  861 |  |
|        - |  862 | `/* --- End of Automatically generated code --- */` |
|        - |  863 | `/*` |
|        - |  864 | ` * Extract a heredoc/nowdoc text from a raw PHP input.` |
|        - |  865 | ` * According to the PHP language reference manual:` |
|        - |  866 | ` *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier` |
|        - |  867 | ` *  is provided, then a newline. The string itself follows, and then the same identifier again` |
|        - |  868 | ` *  to close the quotation.` |
|        - |  869 | ` *  The closing identifier must begin in the first column of the line. Also, the identifier must` |
|        - |  870 | ` *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric` |
|        - |  871 | ` *  characters and underscores, and must start with a non-digit character or underscore.` |
|        - |  872 | ` *  Heredoc text behaves just like a double-quoted string, without the double quotes.` |
|        - |  873 | ` *  This means that quotes in a heredoc do not need to be escaped, but the escape codes listed` |
|        - |  874 | ` *  above can still be used. Variables are expanded, but the same care must be taken when expressing` |
|        - |  875 | ` *  complex variables inside a heredoc as with strings.` |
|        - |  876 | ` *  Nowdocs are to single-quoted strings what heredocs are to double-quoted strings.` |
|        - |  877 | ` *  A nowdoc is specified similarly to a heredoc, but no parsing is done inside a nowdoc.` |
|        - |  878 | ` *  The construct is ideal for embedding PHP code or other large blocks of text without the need` |
|        - |  879 | ` *  for escaping. It shares some features in common with the SGML <![CDATA[ ]]> construct, in that` |
|        - |  880 | ` *  it declares a block of text which is not for parsing.` |
|        - |  881 | ` *  A nowdoc is identified with the same <<< sequence used for heredocs, but the identifier which follows` |
|        - |  882 | ` *  is enclosed in single quotes, e.g. <<<'EOT'. All the rules for heredoc identifiers also apply to nowdoc` |
|        - |  883 | ` *  identifiers, especially those regarding the appearance of the closing identifier.` |
|        - |  884 | ` * Symisc Extension:` |
|        - |  885 | ` * The closing delimiter can now start with a digit or undersocre or it can be an UTF-8 stream.` |
|        - |  886 | ` * Example:` |
|        - |  887 | ` *  <<<123` |
|        - |  888 | ` *    HEREDOC Here` |
|        - |  889 | ` * 123` |
|        - |  890 | ` *  or` |
|        - |  891 | ` *  <<<___` |
|        - |  892 | ` *   HEREDOC Here` |
|        - |  893 | ` *  ___` |
|        - |  894 | ` */` |
|      110 |  895 | `static sxi32 LexExtractHeredoc(SyStream *pStream,SyToken *pToken)` |
|        2 |  896 |  |
|      112 |  897 | `	const unsigned char *zIn  = pStream->zText;` |
|      112 |  898 | `	const unsigned char *zEnd = pStream->zEnd;` |
|        - |  899 | `	const unsigned char *zPtr;` |
|      112 |  900 | `	sxu8 bNowDoc = FALSE;` |
|        - |  901 | `	SyString sDelim;` |
|        - |  902 | `	SyString sStr;` |
|        - |  903 | `	/* Jump leading white spaces */` |
|      124 |  904 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|       13 |  905 | `		zIn++;` |
|        1 |  906 | `	}` |
|      112 |  907 | `	if( zIn >= zEnd ){` |
|        - |  908 | `		/* A simple symbol,return immediately */` |
|      ! 0 |  909 | `		return SXERR_CONTINUE;` |
|        - |  910 | `	}` |
|      112 |  911 | `	if( zIn[0] == '\'' \|\| zIn[0] == '"' ){` |
|        - |  912 | `		/* Make sure we are dealing with a nowdoc */` |
|       44 |  913 | `		bNowDoc =  zIn[0] == '\'' ? TRUE : FALSE;` |
|       44 |  914 | `		zIn++;` |
|       21 |  915 | `	}` |
|      112 |  916 | `	if( zIn[0] < 0xc0 && !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|        - |  917 | `		/* Invalid delimiter,return immediately */` |
|      ! 0 |  918 | `		return SXERR_CONTINUE;` |
|        - |  919 | `	}` |
|        - |  920 | `	/* Isolate the identifier */` |
|      112 |  921 | `	sDelim.zString = (const char *)zIn;` |
|      118 |  922 | `	for(;;){` |
|      238 |  923 | `		zPtr = zIn;` |
|        - |  924 | `		/* Skip alphanumeric stream */` |
|      756 |  925 | `		while( zPtr < zEnd && zPtr[0] < 0xc0 && (SyisAlphaNum(zPtr[0]) \|\| zPtr[0] == '_') ){` |
|      402 |  926 | `			zPtr++;` |
|        2 |  927 | `		}` |
|      238 |  928 | `		if( zPtr < zEnd && zPtr[0] >= 0xc0 ){` |
|       19 |  929 | `			zPtr++;` |
|        - |  930 | `			/* UTF-8 stream */` |
|       37 |  931 | `			while( zPtr < zEnd && ((zPtr[0] & 0xc0) == 0x80) ){` |
|       19 |  932 | `				zPtr++;` |
|        1 |  933 | `			}` |
|        9 |  934 | `		}` |
|      238 |  935 | `		if( zPtr == zIn ){` |
|        - |  936 | `			/* Not an UTF-8 or alphanumeric stream */` |
|      112 |  937 | `			break;` |
|        - |  938 | `		}` |
|        - |  939 | `		/* Synchronize pointers */` |
|      128 |  940 | `		zIn = zPtr;` |
|        2 |  941 | `	}` |
|        - |  942 | `	/* Get the identifier length */` |
|      112 |  943 | `	sDelim.nByte = (sxu32)((const char *)zIn-sDelim.zString);` |
|      112 |  944 | `	if( zIn[0] == '"' \|\| (bNowDoc && zIn[0] == '\'') ){` |
|        - |  945 | `		/* Jump the trailing single quote */` |
|       44 |  946 | `		zIn++;` |
|       21 |  947 | `	}` |
|        - |  948 | `	/* Jump trailing white spaces */` |
|      112 |  949 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|      ! 0 |  950 | `		zIn++;` |
|      ! 0 |  951 | `	}` |
|      112 |  952 | `	if( sDelim.nByte <= 0 \|\| zIn >= zEnd \|\| zIn[0] != '\n' ){` |
|        - |  953 | `		/* Invalid syntax */` |
|      ! 0 |  954 | `		return SXERR_CONTINUE;` |
|        - |  955 | `	}` |
|      112 |  956 | `	pStream->nLine++; /* Increment line counter */` |
|      112 |  957 | `	zIn++;` |
|        - |  958 | `	/* Isolate the delimited string */` |
|      112 |  959 | `	sStr.zString = (const char *)zIn;` |
|        - |  960 | `	/* PHP 7.3 flexible heredoc/nowdoc: the closing marker may be preceded` |
|        - |  961 | `	 * by whitespace (spaces/tabs), and may be followed by any non-identifier` |
|        - |  962 | `	 * character. The indent count is recorded in pToken->pUserData and the` |
|        - |  963 | `	 * compile phase strips it from each body line. */` |
|        - |  964 | `	{` |
|      112 |  965 | `		const unsigned char *zMarkerLine = zIn; /* Start of marker's line (set on match) */` |
|      112 |  966 | `		sxu32 nIndent = 0;` |
|      225 |  967 | `		for(;;){` |
|      282 |  968 | `			const unsigned char *zLineStart = zIn;` |
|        - |  969 | `			/* Skip leading space/tab on this line */` |
|      806 |  970 | `			while( zIn < zEnd && (zIn[0] == ' ' \|\| zIn[0] == '\t') ){` |
|      386 |  971 | `				zIn++;` |
|        2 |  972 | `			}` |
|      280 |  973 | `			if( (sxu32)(zEnd - zIn) >= sDelim.nByte` |
|      281 |  974 | `				&& SyMemcmp((const void *)sDelim.zString,(const void *)zIn,sDelim.nByte) == 0 ){` |
|        - |  975 | `				int bIdentCont;` |
|      110 |  976 | `				zPtr = &zIn[sDelim.nByte];` |
|        - |  977 | `				/* Disambiguate: next byte must not continue an identifier.` |
|        - |  978 | `				 * A leading byte >= 0xc0 starts a multi-byte UTF-8 sequence,` |
|        - |  979 | `				 * which PHP identifiers may contain, so treat it as ident. */` |
|      110 |  980 | `				if( zPtr >= zEnd ){` |
|      ! 0 |  981 | `					bIdentCont = 0;` |
|      110 |  982 | `				}else if( zPtr[0] >= 0xc0 ){` |
|      ! 0 |  983 | `					bIdentCont = 1;` |
|      ! 0 |  984 | `				}else{` |
|      110 |  985 | `					bIdentCont = (SyisAlphaNum(zPtr[0]) \|\| zPtr[0] == '_');` |
|        - |  986 | `				}` |
|      110 |  987 | `				if( !bIdentCont ){` |
|        - |  988 | `					/* Closing marker found */` |
|      110 |  989 | `					nIndent = (sxu32)(zIn - zLineStart);` |
|      110 |  990 | `					zMarkerLine = zLineStart;` |
|      110 |  991 | `					pStream->zText = zPtr; /* Cursor right after identifier */` |
|      110 |  992 | `					break;` |
|        - |  993 | `				}` |
|      ! 0 |  994 | `			}` |
|        - |  995 | `			/* Not the closing marker on this line; walk to next newline */` |
|     2824 |  996 | `			while( zIn < zEnd && zIn[0] != '\n' ){` |
|     2652 |  997 | `				zIn++;` |
|        2 |  998 | `			}` |
|      174 |  999 | `			if( zIn >= zEnd ){` |
|        - | 1000 | `				/* End of input without finding the closing marker */` |
|        3 | 1001 | `				pStream->zText = pStream->zEnd;` |
|        3 | 1002 | `				zMarkerLine = zIn;` |
|        3 | 1003 | `				break;` |
|        - | 1004 | `			}` |
|      172 | 1005 | `			pStream->nLine++;` |
|      172 | 1006 | `			zIn++;` |
|        2 | 1007 | `		}` |
|        - | 1008 | `		/* Body runs from sStr.zString up to just before the marker line */` |
|      112 | 1009 | `		sStr.nByte = (sxu32)((const char *)zMarkerLine - sStr.zString);` |
|      112 | 1010 | `		pToken->nType = bNowDoc ? PH7_TK_NOWDOC : PH7_TK_HEREDOC;` |
|      112 | 1011 | `		SyStringDupPtr(&pToken->sData,&sStr);` |
|        - | 1012 | `		/* Strip exactly one line terminator that precedes the marker's line. */` |
|      110 | 1013 | `		if( pToken->sData.nByte > 0` |
|      108 | 1014 | `			&& pToken->sData.zString[pToken->sData.nByte - 1] == '\n' ){` |
|      102 | 1015 | `			pToken->sData.nByte--;` |
|      100 | 1016 | `			if( pToken->sData.nByte > 0` |
|      102 | 1017 | `				&& pToken->sData.zString[pToken->sData.nByte - 1] == '\r' ){` |
|      ! 0 | 1018 | `				pToken->sData.nByte--;` |
|      ! 0 | 1019 | `			}` |
|       50 | 1020 | `		}` |
|      112 | 1021 | `		pToken->pUserData = SX_INT_TO_PTR(nIndent);` |
|        - | 1022 | `	}` |
|        - | 1023 | `	/* All done */` |
|      112 | 1024 | `	return SXRET_OK;` |
|       57 | 1025 |  |
|        - | 1026 | `/*` |
|        - | 1027 | ` * Tokenize a raw PHP input.` |
|        - | 1028 | ` * This is the public tokenizer called by most code generator routines.` |
|        - | 1029 | ` */` |
|    14776 | 1030 | `PH7_PRIVATE sxi32 PH7_TokenizePHP(const char *zInput,sxu32 nLen,sxu32 nLineStart,SySet *pOut)` |
|        2 | 1031 |  |
|        - | 1032 | `	SyLex sLexer;` |
|        - | 1033 | `	sxi32 rc;` |
|        - | 1034 | `	/* Initialize the lexer */` |
|    14778 | 1035 | `	rc = SyLexInit(&sLexer,&(*pOut),TokenizePHP,0);` |
|    14778 | 1036 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1037 | `		return rc;` |
|        - | 1038 | `	}` |
|    14778 | 1039 | `	sLexer.sStream.nLine = nLineStart;` |
|        - | 1040 | `	/* Tokenize input */` |
|    14778 | 1041 | `	rc = SyLexTokenizeInput(&sLexer,zInput,nLen,0,0,0);` |
|        - | 1042 | `	/* Release the lexer */` |
|    14778 | 1043 | `	SyLexRelease(&sLexer);` |
|        - | 1044 | `	/* Tokenization result */` |
|    14778 | 1045 | `	return rc;` |
|     7390 | 1046 |  |
|        - | 1047 | `/*` |
|        - | 1048 | ` * High level public tokenizer.` |
|        - | 1049 | ` *  Tokenize the input into PHP tokens and raw tokens [i.e: HTML,XML,Raw text...].` |
|        - | 1050 | ` * According to the PHP language reference manual` |
|        - | 1051 | ` *   When PHP parses a file, it looks for opening and closing tags, which tell PHP` |
|        - | 1052 | ` *   to start and stop interpreting the code between them. Parsing in this manner allows` |
|        - | 1053 | ` *   PHP to be embedded in all sorts of different documents, as everything outside of a pair` |
|        - | 1054 | ` *   of opening and closing tags is ignored by the PHP parser. Most of the time you will see` |
|        - | 1055 | ` *   PHP embedded in HTML documents, as in this example.` |
|        - | 1056 | ` *   <?php echo 'While this is going to be parsed.'; ?>` |
|        - | 1057 | ` *   <p>This will also be ignored.</p>` |
|        - | 1058 | ` *   You can also use more advanced structures:` |
|        - | 1059 | ` *   Example #1 Advanced escaping` |
|        - | 1060 | ` * <?php` |
|        - | 1061 | ` * if ($expression) {` |
|        - | 1062 | ` *   ?>` |
|        - | 1063 | ` *   <strong>This is true.</strong>` |
|        - | 1064 | ` *   <?php` |
|        - | 1065 | ` * } else {` |
|        - | 1066 | ` *   ?>` |
|        - | 1067 | ` *   <strong>This is false.</strong>` |
|        - | 1068 | ` *   <?php` |
|        - | 1069 | ` * }` |
|        - | 1070 | ` * ?>` |
|        - | 1071 | ` * This works as expected, because when PHP hits the ?> closing tags, it simply starts outputting` |
|        - | 1072 | ` * whatever it finds (except for an immediately following newline - see instruction separation ) until it hits` |
|        - | 1073 | ` * another opening tag. The example given here is contrived, of course, but for outputting large blocks of text` |
|        - | 1074 | ` * dropping out of PHP parsing mode is generally more efficient than sending all of the text through echo() or print().` |
|        - | 1075 | ` * There are four different pairs of opening and closing tags which can be used in PHP. Three of those, <?php ?>` |
|        - | 1076 | ` * <script language="php"> </script>  and <? ?> are always available. The other two are short tags and ASP style` |
|        - | 1077 | ` * tags, and can be turned on and off from the php.ini configuration file. As such, while some people find short tags` |
|        - | 1078 | ` * and ASP style tags convenient, they are less portable, and generally not recommended.` |
|        - | 1079 | ` * Note:` |
|        - | 1080 | ` * Also note that if you are embedding PHP within XML or XHTML you will need to use the <?php ?> tags to remain` |
|        - | 1081 | ` * compliant with standards.` |
|        - | 1082 | ` * Example #2 PHP Opening and Closing Tags` |
|        - | 1083 | ` * 1.  <?php echo 'if you want to serve XHTML or XML documents, do it like this'; ?>` |
|        - | 1084 | ` * 2.  <script language="php">` |
|        - | 1085 | ` *       echo 'some editors (like FrontPage) don\'t` |
|        - | 1086 | ` *             like processing instructions';` |
|        - | 1087 | ` *   </script>` |
|        - | 1088 | ` *` |
|        - | 1089 | ` * 3.  <? echo 'this is the simplest, an SGML processing instruction'; ?>` |
|        - | 1090 | ` *   <?= expression ?> This is a shortcut for "<? echo expression ?>"` |
|        - | 1091 | ` */` |
|    12260 | 1092 | `PH7_PRIVATE sxi32 PH7_TokenizeRawText(const char *zInput,sxu32 nLen,SySet *pOut)` |
|        2 | 1093 |  |
|    12262 | 1094 | `	const char *zEnd = &zInput[nLen];` |
|    12262 | 1095 | `	const char *zIn  = zInput;` |
|        - | 1096 | `	const char *zCur,*zCurEnd;` |
|    12262 | 1097 | `	SyString sCtag = { 0, 0 };     /* Closing tag */` |
|        - | 1098 | `	SyToken sToken;` |
|        - | 1099 | `	SyString sDoc;` |
|        - | 1100 | `	sxu32 nLine;` |
|        - | 1101 | `	sxi32 iNest;` |
|        - | 1102 | `	sxi32 rc;` |
|        - | 1103 | `	/* Tokenize the input into PHP tokens and raw tokens */` |
|    12262 | 1104 | `	nLine = 1;` |
|    12262 | 1105 | `	zCur = zCurEnd   = 0; /* Prevent compiler warning */` |
|    12262 | 1106 | `	sToken.pUserData = 0;` |
|    12262 | 1107 | `	iNest = 0;` |
|    12262 | 1108 | `	sDoc.nByte = 0;` |
|    12262 | 1109 | `	sDoc.zString = ""; /* cc warning */` |
|    12262 | 1110 | `	for(;;){` |
|    24526 | 1111 | `		if( zIn >= zEnd ){` |
|        - | 1112 | `			/* End of input reached */` |
|    12258 | 1113 | `			break;` |
|        - | 1114 | `		}` |
|    12270 | 1115 | `		sToken.nLine = nLine;` |
|    12270 | 1116 | `		zCur = zIn;` |
|    12270 | 1117 | `		zCurEnd = 0;` |
|    12278 | 1118 | `		while( zIn < zEnd ){` |
|    12274 | 1119 | `			 if( zIn[0] == '<' ){` |
|    12266 | 1120 | `				const char *zTmp = zIn; /* End of raw input marker */` |
|    12266 | 1121 | `				zIn++;` |
|    12266 | 1122 | `				if( zIn < zEnd ){` |
|    12266 | 1123 | `					if( zIn[0] == '?' ){` |
|    12266 | 1124 | `						zIn++;` |
|    12266 | 1125 | `						if( (sxu32)(zEnd - zIn) >= sizeof("php")-1 &&  SyStrnicmp(zIn,"php",sizeof("php")-1) == 0 ){` |
|        - | 1126 | `							/* opening tag: <?php */` |
|    12264 | 1127 | `							zIn += sizeof("php")-1;` |
|     6131 | 1128 | `						}` |
|        - | 1129 | `						/* Look for the closing tag '?>' */` |
|    12266 | 1130 | `						SyStringInitFromBuf(&sCtag,"?>",sizeof("?>")-1);` |
|    12266 | 1131 | `						zCurEnd = zTmp;` |
|    12266 | 1132 | `						break;` |
|        - | 1133 | `					}` |
|      ! 0 | 1134 | `				}` |
|      ! 0 | 1135 | `			}else{` |
|       10 | 1136 | `				if( zIn[0] == '\n' ){` |
|       10 | 1137 | `					nLine++;` |
|        4 | 1138 | `				}` |
|       10 | 1139 | `				zIn++;` |
|        - | 1140 | `			 }` |
|        2 | 1141 | `		} /* While(zIn < zEnd) */` |
|    12270 | 1142 | `		if( zCurEnd == 0 ){` |
|        5 | 1143 | `			zCurEnd = zIn;` |
|        2 | 1144 | `		}` |
|        - | 1145 | `		/* Save the raw token */` |
|    12270 | 1146 | `		SyStringInitFromBuf(&sToken.sData,zCur,zCurEnd - zCur);` |
|    12270 | 1147 | `		sToken.nType = PH7_TOKEN_RAW;` |
|    12270 | 1148 | `		rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|    12270 | 1149 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1150 | `			return rc;` |
|        - | 1151 | `		}` |
|    12270 | 1152 | `		if( zIn >= zEnd ){` |
|        5 | 1153 | `			break;` |
|        - | 1154 | `		}` |
|        - | 1155 | `		/* Ignore leading white space */` |
|    26532 | 1156 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    14268 | 1157 | `			if( zIn[0] == '\n' ){` |
|    13076 | 1158 | `				nLine++;` |
|     6537 | 1159 | `			}` |
|    14268 | 1160 | `			zIn++;` |
|        2 | 1161 | `		}` |
|        - | 1162 | `		/* Delimit the PHP chunk */` |
|    12266 | 1163 | `		sToken.nLine = nLine;` |
|    12266 | 1164 | `		zCur = zIn;` |
|  1151474 | 1165 | `		while( (sxu32)(zEnd - zIn) >= sCtag.nByte ){` |
|        - | 1166 | `			const char *zPtr;` |
|  1146076 | 1167 | `			if( SyMemcmp(zIn,sCtag.zString,sCtag.nByte) == 0 && iNest < 1 ){` |
|     6868 | 1168 | `				break;` |
|        - | 1169 | `			}` |
|   571676 | 1170 | `			for(;;){` |
|  1143354 | 1171 | `				if( zIn[0] != '/' \|\| (zIn[1] != '*' && zIn[1] != '/') /* && sCtag.nByte >= 2 */ ){` |
|   569606 | 1172 | `					break;` |
|        - | 1173 | `				}` |
|     4146 | 1174 | `				zIn += 2;` |
|     4146 | 1175 | `				if( zIn[-1] == '/' ){` |
|        - | 1176 | `					/* Inline comment */` |
|   145418 | 1177 | `					while( zIn < zEnd && zIn[0] != '\n' ){` |
|   141358 | 1178 | `						zIn++;` |
|        2 | 1179 | `					}` |
|     4062 | 1180 | `					if( zIn >= zEnd ){` |
|      ! 0 | 1181 | `						zIn--;` |
|      ! 0 | 1182 | `					}` |
|     2032 | 1183 | `				}else{` |
|        - | 1184 | `					/* Block comment */` |
|     4530 | 1185 | `					while( (sxu32)(zEnd-zIn) >= sizeof("*/") - 1 ){` |
|     4530 | 1186 | `						if( zIn[0] == '*' && zIn[1] == '/' ){` |
|       86 | 1187 | `							zIn += 2;` |
|       86 | 1188 | `							break;` |
|        - | 1189 | `						}` |
|     4446 | 1190 | `						if( zIn[0] == '\n' ){` |
|       28 | 1191 | `							nLine++;` |
|       13 | 1192 | `						}` |
|     4446 | 1193 | `						zIn++;` |
|        2 | 1194 | `					}` |
|        - | 1195 | `				}` |
|        2 | 1196 | `			}` |
|  1139210 | 1197 | `			if( zIn[0] == '\n' ){` |
|    39784 | 1198 | `				nLine++;` |
|    39784 | 1199 | `				if( iNest > 0 ){` |
|      282 | 1200 | `					zIn++;` |
|      666 | 1201 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|      386 | 1202 | `						zIn++;` |
|        2 | 1203 | `					}` |
|      282 | 1204 | `					zPtr = zIn;` |
|     1440 | 1205 | `					while( zIn < zEnd ){` |
|     1440 | 1206 | `						if( (unsigned char)zIn[0] >= 0xc0 ){` |
|        - | 1207 | `							/* UTF-8 stream */` |
|       19 | 1208 | `							zIn++;` |
|       37 | 1209 | `							SX_JMP_UTF8(zIn,zEnd);` |
|     1430 | 1210 | `						}else if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|      142 | 1211 | `							break;` |
|      ! 0 | 1212 | `						}else{` |
|     1142 | 1213 | `							zIn++;` |
|        - | 1214 | `						}` |
|        2 | 1215 | `					}` |
|      282 | 1216 | `					if( (sxu32)(zIn - zPtr) == sDoc.nByte && SyMemcmp(sDoc.zString,zPtr,sDoc.nByte) == 0 ){` |
|      110 | 1217 | `						iNest = 0;` |
|       54 | 1218 | `					}` |
|      282 | 1219 | `					continue;` |
|        2 | 1220 | `				}` |
|  1119179 | 1221 | `			}else if ( (sxu32)(zEnd - zIn) >= sizeof("<<<") && zIn[0] == '<' && zIn[1] == '<' && zIn[2] == '<' && iNest < 1){` |
|      112 | 1222 | `				zIn += sizeof("<<<")-1;` |
|      124 | 1223 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|       13 | 1224 | `					zIn++;` |
|        1 | 1225 | `				}` |
|      112 | 1226 | `				if( zIn[0] == '"' \|\| zIn[0] == '\'' ){` |
|       44 | 1227 | `					zIn++;` |
|       21 | 1228 | `				}` |
|      112 | 1229 | `				zPtr = zIn;` |
|      530 | 1230 | `				while( zIn < zEnd ){` |
|      530 | 1231 | `					if( (unsigned char)zIn[0] >= 0xc0 ){` |
|        - | 1232 | `						/* UTF-8 stream */` |
|       19 | 1233 | `						zIn++;` |
|       37 | 1234 | `						SX_JMP_UTF8(zIn,zEnd);` |
|      520 | 1235 | `					}else if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       57 | 1236 | `						break;` |
|      ! 0 | 1237 | `					}else{` |
|      402 | 1238 | `						zIn++;` |
|        - | 1239 | `					}` |
|        2 | 1240 | `				}` |
|      112 | 1241 | `				SyStringInitFromBuf(&sDoc,zPtr,zIn-zPtr);` |
|      112 | 1242 | `				SyStringFullTrim(&sDoc);` |
|      112 | 1243 | `				if( sDoc.nByte > 0 ){` |
|      112 | 1244 | `					iNest++;` |
|       55 | 1245 | `				}` |
|      112 | 1246 | `				continue;` |
|        - | 1247 | `			}` |
|  1138820 | 1248 | `			zIn++;` |
|        - | 1249 |  |
|  1138820 | 1250 | `			if ( zIn >= zEnd )` |
|      ! 0 | 1251 | `				break;` |
|        2 | 1252 | `		}` |
|    12266 | 1253 | `		if( (sxu32)(zEnd - zIn) < sCtag.nByte ){` |
|     5400 | 1254 | `			zIn = zEnd;` |
|     2699 | 1255 | `		}` |
|    12266 | 1256 | `		if( zCur < zIn ){` |
|        - | 1257 | `			/* Save the PHP chunk for later processing */` |
|     9766 | 1258 | `			sToken.nType = PH7_TOKEN_PHP;` |
|     9766 | 1259 | `			SyStringInitFromBuf(&sToken.sData,zCur,zIn-zCur);` |
|    19464 | 1260 | `			SyStringRightTrim(&sToken.sData); /* Trim trailing white spaces */` |
|     9766 | 1261 | `			rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|     9766 | 1262 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1263 | `				return rc;` |
|        - | 1264 | `			}` |
|     4882 | 1265 | `		}` |
|    12266 | 1266 | `		if( zIn < zEnd ){` |
|        - | 1267 | `			/* Jump the trailing closing tag */` |
|     6868 | 1268 | `			zIn += sCtag.nByte;` |
|     3433 | 1269 | `		}` |
|        2 | 1270 | `	} /* For(;;) */` |
|        - | 1271 |  |
|    12262 | 1272 | ` 	return SXRET_OK;` |
|     6132 | 1273 |  |
|        - | 1274 |  |
