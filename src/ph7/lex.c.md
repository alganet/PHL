# src/ph7/lex.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 713/748 lines (95.32%)

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
|  7357940 |   19 | `static sxi32 TokenizePHP(SyStream *pStream,SyToken *pToken,void *pUserData,void *pCtxData)` |
|        2 |   20 |  |
|        - |   21 | `	SyString *pStr;` |
|        - |   22 | `	sxi32 rc;` |
|        - |   23 | `	/* Ignore leading white spaces */` |
| 11084334 |   24 | `	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){` |
|        - |   25 | `		/* Advance the stream cursor */` |
|  3726394 |   26 | `		if( pStream->zText[0] == '\n' ){` |
|        - |   27 | `			/* Update line counter */` |
|    31556 |   28 | `			pStream->nLine++;` |
|    15777 |   29 | `		}` |
|  3726394 |   30 | `		pStream->zText++;` |
|        2 |   31 | `	}` |
|  7357942 |   32 | `	if( pStream->zText >= pStream->zEnd ){` |
|        - |   33 | `		/* End of input reached */` |
|      ! 0 |   34 | `		return SXERR_EOF;` |
|        - |   35 | `	}` |
|        - |   36 | `	/* Record token starting position and line */` |
|  7357942 |   37 | `	pToken->nLine = pStream->nLine;` |
|  7357942 |   38 | `	pToken->pUserData = 0;` |
|  7357942 |   39 | `	pStr = &pToken->sData;` |
|  7357942 |   40 | `	SyStringInitFromBuf(pStr,pStream->zText,0);` |
|  8687355 |   41 | `	if( pStream->zText[0] >= 0xc0 \|\| SyisAlpha(pStream->zText[0]) \|\| pStream->zText[0] == '_' ){` |
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
|  2658828 |   52 | `		if( pStream->zText[0] < 0xc0 ){` |
|  2658812 |   53 | `			pStream->zText++;` |
|  1329405 |   54 | `		}` |
|  2610159 |   55 | `		for(;;){` |
|  5220320 |   56 | `			zIn = pStream->zText;` |
|  5220320 |   57 | `			if( zIn[0] >= 0xc0 ){` |
|       49 |   58 | `				zIn++;` |
|        - |   59 | `				/* UTF-8 stream */` |
|      109 |   60 | `				while( zIn < pStream->zEnd && ((zIn[0] & 0xc0) == 0x80) ){` |
|       61 |   61 | `					zIn++;` |
|        1 |   62 | `				}` |
|       24 |   63 | `			}` |
|        - |   64 | `			/* Skip alphanumeric stream */` |
| 21419051 |   65 | `			while( zIn < pStream->zEnd && zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_') ){` |
| 13588574 |   66 | `				zIn++;` |
|        2 |   67 | `			}` |
|  5220320 |   68 | `			if( zIn == pStream->zText ){` |
|        - |   69 | `				/* Not an UTF-8 or alphanumeric stream */` |
|  2658828 |   70 | `				break;` |
|        - |   71 | `			}` |
|        - |   72 | `			/* Synchronize pointers */` |
|  2561494 |   73 | `			pStream->zText = zIn;` |
|        2 |   74 | `		}` |
|        - |   75 | `		/* Record token length */` |
|  2658828 |   76 | `		pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  2658828 |   77 | `		nKeyword = KeywordCode(pStr->zString,(int)pStr->nByte);` |
|        - |   78 | `		/* PHP 7.4: 'fn' is a keyword reserved for arrow functions.` |
|        - |   79 | `		 * The auto-generated perfect hash above doesn't know about it,` |
|        - |   80 | `		 * so intercept the 'fn' identifier here.` |
|        - |   81 | `		 */` |
|  2659308 |   82 | `		if( nKeyword == PH7_TK_ID && pStr->nByte == 2` |
|   878918 |   83 | `			&& pStr->zString[0] == 'f' && pStr->zString[1] == 'n' ){` |
|      358 |   84 | `			nKeyword = PH7_TKWRD_FN;` |
|      178 |   85 | `		}` |
|  2658828 |   86 | `		if( nKeyword != PH7_TK_ID ){` |
|   908752 |   87 | `			if( nKeyword &` |
|        - |   88 | `				(PH7_TKWRD_NEW\|PH7_TKWRD_CLONE\|PH7_TKWRD_AND\|PH7_TKWRD_XOR\|PH7_TKWRD_OR\|PH7_TKWRD_INSTANCEOF\|PH7_TKWRD_SEQ\|PH7_TKWRD_SNE) ){` |
|        - |   89 | `					/* Alpha stream operators [i.e: new,clone,and,instanceof,eq,ne,or,xor],save the operator instance for later processing */` |
|    14984 |   90 | `					pToken->pUserData = (void *)PH7_ExprExtractOperator(pStr,0);` |
|        - |   91 | `					/* Mark as an operator */` |
|    14984 |   92 | `					pToken->nType = PH7_TK_ID\|PH7_TK_OP;` |
|     7493 |   93 | `			}else{` |
|        - |   94 | `				/* We are dealing with a keyword [i.e: while,foreach,class...],save the keyword ID */` |
|   893770 |   95 | `				pToken->nType = PH7_TK_KEYWORD;` |
|   893770 |   96 | `				pToken->pUserData = SX_INT_TO_PTR(nKeyword);` |
|        - |   97 | `			}` |
|   454377 |   98 | `		}else{` |
|        - |   99 | `			/* A simple identifier */` |
|  1750078 |  100 | `			pToken->nType = PH7_TK_ID;` |
|        - |  101 | `		}` |
|  1329415 |  102 | `	}else{` |
|        - |  103 | `		sxi32 c;` |
|        - |  104 | `		/* Non-alpha stream */` |
|  4735167 |  105 | `		if( pStream->zText[0] == '#' \|\|` |
|  4699114 |  106 | `			( pStream->zText[0] == '/' &&  &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '/') ){` |
|     3846 |  107 | `				pStream->zText++;` |
|        - |  108 | `				/* Inline comments */` |
|   139962 |  109 | `				while( pStream->zText < pStream->zEnd && pStream->zText[0] != '\n' ){` |
|   136118 |  110 | `					pStream->zText++;` |
|        2 |  111 | `				}` |
|        - |  112 | `				/* Tell the upper-layer to ignore this token */` |
|     3846 |  113 | `				return SXERR_CONTINUE;` |
|  4695272 |  114 | `		}else if( pStream->zText[0] == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '*' ){` |
|    68196 |  115 | `			pStream->zText += 2;` |
|        - |  116 | `			/* Block comment */` |
|  1934340 |  117 | `			while( pStream->zText < pStream->zEnd ){` |
|  1934340 |  118 | `				if( pStream->zText[0] == '*' ){` |
|    68222 |  119 | `					if( &pStream->zText[1] >= pStream->zEnd \|\| pStream->zText[1] == '/'  ){` |
|    34099 |  120 | `						break;` |
|        - |  121 | `					}` |
|       13 |  122 | `				}` |
|  1866146 |  123 | `				if( pStream->zText[0] == '\n' ){` |
|       28 |  124 | `					pStream->nLine++;` |
|       13 |  125 | `				}` |
|  1866146 |  126 | `				pStream->zText++;` |
|        2 |  127 | `			}` |
|    68196 |  128 | `			pStream->zText += 2;` |
|        - |  129 | `			/* Tell the upper-layer to ignore this token */` |
|    68196 |  130 | `			return SXERR_CONTINUE;` |
|  4627078 |  131 | `		}else if( SyisDigit(pStream->zText[0]) ){` |
|    94388 |  132 | `			pStream->zText++;` |
|        - |  133 | `			/* PHP 7.4: handle underscore separator immediately following the first digit.` |
|        - |  134 | `			 * Check pStream->zText < pStream->zEnd BEFORE forming pStream->zText + 1 so` |
|        - |  135 | `			 * we never compute a pointer past one-past-end. */` |
|    94466 |  136 | `			if( pStream->zText < pStream->zEnd` |
|    94386 |  137 | `				&& pStream->zText[0] == '_'` |
|    47273 |  138 | `				&& pStream->zText + 1 < pStream->zEnd` |
|      160 |  139 | `				&& pStream->zText[1] < 0xc0` |
|      162 |  140 | `				&& SyisDigit(pStream->zText[1]) ){` |
|      151 |  141 | `				pStream->zText++; /* swallow underscore between two digits */` |
|       75 |  142 | `			}` |
|        - |  143 | `			/* Decimal digit stream (PHP 7.4: underscore separator allowed between two digits) */` |
|   104212 |  144 | `			while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|     9826 |  145 | `				pStream->zText++;` |
|     9910 |  146 | `				if( pStream->zText < pStream->zEnd` |
|     9824 |  147 | `					&& pStream->zText[0] == '_'` |
|     4998 |  148 | `					&& pStream->zText + 1 < pStream->zEnd` |
|      172 |  149 | `					&& pStream->zText[1] < 0xc0` |
|      174 |  150 | `					&& SyisDigit(pStream->zText[1]) ){` |
|      173 |  151 | `					pStream->zText++; /* swallow underscore between two digits */` |
|       86 |  152 | `				}` |
|        2 |  153 | `			}` |
|        - |  154 | `			/* Mark the token as integer until we encounter a real number */` |
|    94388 |  155 | `			pToken->nType = PH7_TK_INTEGER;` |
|    94388 |  156 | `			if( pStream->zText < pStream->zEnd ){` |
|    94388 |  157 | `				c = pStream->zText[0];` |
|    94388 |  158 | `				if( c == '.' ){` |
|        - |  159 | `					/* Real number (PHP 7.4: underscore separator allowed between two digits) */` |
|      460 |  160 | `					pStream->zText++;` |
|     1782 |  161 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|     1324 |  162 | `						pStream->zText++;` |
|     1328 |  163 | `						if( pStream->zText < pStream->zEnd` |
|     1322 |  164 | `							&& pStream->zText[0] == '_'` |
|      667 |  165 | `							&& pStream->zText + 1 < pStream->zEnd` |
|       12 |  166 | `							&& pStream->zText[1] < 0xc0` |
|       14 |  167 | `							&& SyisDigit(pStream->zText[1]) ){` |
|       13 |  168 | `							pStream->zText++;` |
|        6 |  169 | `						}` |
|        2 |  170 | `					}` |
|      460 |  171 | `					if( pStream->zText < pStream->zEnd ){` |
|      460 |  172 | `						c = pStream->zText[0];` |
|      460 |  173 | `						if( c=='e' \|\| c=='E' ){` |
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
|      229 |  193 | `					}` |
|      460 |  194 | `					pToken->nType = PH7_TK_REAL;` |
|    94159 |  195 | `				}else if( c=='e' \|\| c=='E' ){` |
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
|    93916 |  217 | `				}else if( c == 'x' \|\| c == 'X' ){` |
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
|    93866 |  230 | `				}else if(c  == 'b' \|\| c == 'B' ){` |
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
|    47193 |  243 | `			}` |
|        - |  244 | `			/* PHP 7.4: absorb a trailing malformed underscore run into the` |
|        - |  245 | `			 * numeric token so the compile phase can emit a PHP-compatible` |
|        - |  246 | `			 * "syntax error, unexpected identifier" parse error. Valid` |
|        - |  247 | `			 * separators were already consumed by the per-loop peek logic` |
|        - |  248 | `			 * above, so an underscore here is always misplaced. */` |
|    94388 |  249 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '_' ){` |
|       15 |  250 | `				pStream->zText++;` |
|       44 |  251 | `				while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0` |
|       46 |  252 | `					&& (SyisAlphaNum(pStream->zText[0]) \|\| pStream->zText[0] == '_') ){` |
|       17 |  253 | `					pStream->zText++;` |
|        1 |  254 | `				}` |
|        7 |  255 | `			}` |
|        - |  256 | `			/* Record token length */` |
|    94388 |  257 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|    94388 |  258 | `			return SXRET_OK;` |
|        - |  259 | `		}` |
|  4532692 |  260 | `		c = pStream->zText[0];` |
|  4532692 |  261 | `		pStream->zText++; /* Advance the stream cursor */` |
|        - |  262 | `		/* Assume we are dealing with an operator*/` |
|  4532692 |  263 | `		pToken->nType = PH7_TK_OP;` |
|  4532692 |  264 | `		switch(c){` |
|   950774 |  265 | `		case '$': pToken->nType = PH7_TK_DOLLAR; break;` |
|   360408 |  266 | `		case '{': pToken->nType = PH7_TK_OCB;    break;` |
|   360394 |  267 | `		case '}': pToken->nType = PH7_TK_CCB;    break;` |
|   716750 |  268 | `		case '(': pToken->nType = PH7_TK_LPAREN; break;` |
|    73170 |  269 | `		case '[': pToken->nType \|= PH7_TK_OSB;   break; /* Bitwise operation here,since the square bracket token '['` |
|        - |  270 | `														 * is a potential operator [i.e: subscripting] */` |
|    73176 |  271 | `		case ']': pToken->nType = PH7_TK_CSB;    break;` |
|   358367 |  272 | `		case ')': {` |
|   716736 |  273 | `			SySet *pTokSet = pStream->pSet;` |
|        - |  274 | `			/* Assemble type cast operators [i.e: (int),(float),(bool)...] */` |
|   716736 |  275 | `			if( pTokSet->nUsed >= 2 ){` |
|        - |  276 | `				SyToken *pTmp;` |
|        - |  277 | `				/* Peek the last recongnized token */` |
|   716734 |  278 | `				pTmp = (SyToken *)SySetPeek(pTokSet);` |
|   716734 |  279 | `				if( pTmp->nType & PH7_TK_KEYWORD ){` |
|    14732 |  280 | `					sxi32 nID = SX_PTR_TO_INT(pTmp->pUserData);` |
|    14732 |  281 | `					if( (sxu32)nID & (PH7_TKWRD_ARRAY\|PH7_TKWRD_INT\|PH7_TKWRD_FLOAT\|PH7_TKWRD_STRING\|PH7_TKWRD_OBJECT\|PH7_TKWRD_BOOL\|PH7_TKWRD_UNSET) ){` |
|    14492 |  282 | `						pTmp = (SyToken *)SySetAt(pTokSet,pTokSet->nUsed - 2);` |
|    14492 |  283 | `						if( pTmp->nType & PH7_TK_LPAREN ){` |
|        - |  284 | `							/* Merge the three tokens '(' 'TYPE' ')' into a single one */` |
|    14408 |  285 | `							const char * zTypeCast = "(int)";` |
|    14408 |  286 | `							if( nID & PH7_TKWRD_FLOAT ){` |
|     2846 |  287 | `								zTypeCast = "(float)";` |
|    12986 |  288 | `							}else if( nID & PH7_TKWRD_BOOL ){` |
|     2848 |  289 | `								zTypeCast = "(bool)";` |
|    10141 |  290 | `							}else if( nID & PH7_TKWRD_STRING ){` |
|     5686 |  291 | `								zTypeCast = "(string)";` |
|     5876 |  292 | `							}else if( nID & PH7_TKWRD_ARRAY ){` |
|       21 |  293 | `								zTypeCast = "(array)";` |
|     3024 |  294 | `							}else if( nID & PH7_TKWRD_OBJECT ){` |
|       17 |  295 | `								zTypeCast = "(object)";` |
|     3006 |  296 | `							}else if( nID & PH7_TKWRD_UNSET ){` |
|        7 |  297 | `								zTypeCast = "(unset)";` |
|        3 |  298 | `							}` |
|        - |  299 | `							/* Reflect the change */` |
|    14408 |  300 | `							pToken->nType = PH7_TK_OP;` |
|    14408 |  301 | `							SyStringInitFromBuf(&pToken->sData,zTypeCast,SyStrlen(zTypeCast));` |
|        - |  302 | `							/* Save the instance associated with the type cast operator */` |
|    14408 |  303 | `							pToken->pUserData = (void *)PH7_ExprExtractOperator(&pToken->sData,0);` |
|        - |  304 | `							/* Remove the two previous tokens */` |
|    14408 |  305 | `							pTokSet->nUsed -= 2;` |
|    14408 |  306 | `							return SXRET_OK;` |
|        - |  307 | `						}` |
|       42 |  308 | `					}` |
|      162 |  309 | `				}` |
|   351163 |  310 | `			}` |
|   702330 |  311 | `			pToken->nType = PH7_TK_RPAREN;` |
|   702330 |  312 | `			break;` |
|        - |  313 | `				  }` |
|    30455 |  314 | `		case '\'':{` |
|        - |  315 | `			/* Single quoted string */` |
|    60912 |  316 | `			pStr->zString++;` |
|   767308 |  317 | `			while( pStream->zText < pStream->zEnd ){` |
|   767308 |  318 | `				if( pStream->zText[0] == '\''  ){` |
|    60922 |  319 | `					if( pStream->zText[-1] != '\\' ){` |
|    60898 |  320 | `						break;` |
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
|   706398 |  333 | `				if( pStream->zText[0] == '\n' ){` |
|       67 |  334 | `					pStream->nLine++;` |
|       33 |  335 | `				}` |
|   706398 |  336 | `				pStream->zText++;` |
|        2 |  337 | `			}` |
|        - |  338 | `			/* Record token length and type */` |
|    60912 |  339 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|    60912 |  340 | `			pToken->nType = PH7_TK_SSTR;` |
|        - |  341 | `			/* Jump the trailing single quote */` |
|    60912 |  342 | `			pStream->zText++;` |
|    60912 |  343 | `			return SXRET_OK;` |
|        - |  344 | `				  }` |
|     8308 |  345 | `		case '"':{` |
|        - |  346 | `			sxi32 iNest;` |
|        - |  347 | `			/* Double quoted string */` |
|    16618 |  348 | `			pStr->zString++;` |
|   160194 |  349 | `			while( pStream->zText < pStream->zEnd ){` |
|   160194 |  350 | `				if( pStream->zText[0] == '{' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '$'){` |
|       99 |  351 | `					iNest = 1;` |
|       99 |  352 | `					pStream->zText++;` |
|        - |  353 | `					/* TICKET 1433-40: Hnadle braces'{}' in double quoted string where everything is allowed */` |
|     1061 |  354 | `					while(pStream->zText < pStream->zEnd ){` |
|     1061 |  355 | `						if( pStream->zText[0] == '{' ){` |
|        9 |  356 | `							iNest++;` |
|     1057 |  357 | `						}else if (pStream->zText[0] == '}' ){` |
|      107 |  358 | `							iNest--;` |
|      107 |  359 | `							if( iNest <= 0 ){` |
|       99 |  360 | `								pStream->zText++;` |
|       99 |  361 | `								break;` |
|        1 |  362 | `							}` |
|      951 |  363 | `						}else if( pStream->zText[0] == '\n' ){` |
|      ! 0 |  364 | `							pStream->nLine++;` |
|      ! 0 |  365 | `						}` |
|      963 |  366 | `						pStream->zText++;` |
|        1 |  367 | `					}` |
|       99 |  368 | `					if( pStream->zText >= pStream->zEnd ){` |
|      ! 0 |  369 | `						break;` |
|        - |  370 | `					}` |
|       49 |  371 | `				}` |
|   160194 |  372 | `				if( pStream->zText[0] == '"' ){` |
|    16718 |  373 | `					if( pStream->zText[-1] != '\\' ){` |
|    16614 |  374 | `						break;` |
|      ! 0 |  375 | `					}else{` |
|      106 |  376 | `						const unsigned char *zPtr = &pStream->zText[-2];` |
|      106 |  377 | `						sxi32 i = 1;` |
|      158 |  378 | `						while( zPtr > pStream->zInput && zPtr[0] == '\\' ){` |
|       54 |  379 | `							zPtr--;` |
|       54 |  380 | `							i++;` |
|        2 |  381 | `						}` |
|      106 |  382 | `						if((i&1)==0){` |
|        5 |  383 | `							break;` |
|        - |  384 | `						}` |
|        - |  385 | `					}` |
|       50 |  386 | `				}` |
|   143578 |  387 | `				if( pStream->zText[0] == '\n' ){` |
|        7 |  388 | `					pStream->nLine++;` |
|        3 |  389 | `				}` |
|   143578 |  390 | `				pStream->zText++;` |
|        2 |  391 | `			}` |
|        - |  392 | `			/* Record token length and type */` |
|    16618 |  393 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|    16618 |  394 | `			pToken->nType = PH7_TK_DSTR;` |
|        - |  395 | `			/* Jump the trailing quote */` |
|    16618 |  396 | `			pStream->zText++;` |
|    16618 |  397 | `			return SXRET_OK;` |
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
|      174 |  418 | `		case '\\': pToken->nType = PH7_TK_NSSEP;  break;` |
|     1356 |  419 | `		case ':':` |
|     2714 |  420 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == ':' ){` |
|        - |  421 | `				/* Current operator: '::' */` |
|      236 |  422 | `				pStream->zText++;` |
|      119 |  423 | `			}else{` |
|     2480 |  424 | `				pToken->nType = PH7_TK_COLON; /* Single colon */` |
|        - |  425 | `			}` |
|     2714 |  426 | `			break;` |
|    77212 |  427 | `		case ',': pToken->nType \|= PH7_TK_COMMA;  break; /* Comma is also an operator */` |
|   515502 |  428 | `		case ';': pToken->nType = PH7_TK_SEMI;    break;` |
|        - |  429 | `			/* Handle combined operators [i.e: +=,===,!=== ...] */` |
|   149076 |  430 | `		case '=':` |
|   298154 |  431 | `			pToken->nType \|= PH7_TK_EQUAL;` |
|   298154 |  432 | `			if( pStream->zText < pStream->zEnd ){` |
|   298154 |  433 | `				if( pStream->zText[0] == '=' ){` |
|    18556 |  434 | `					pToken->nType &= ~PH7_TK_EQUAL;` |
|        - |  435 | `					/* Current operator: == */` |
|    18556 |  436 | `					pStream->zText++;` |
|    18556 |  437 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  438 | `						/* Current operator: === */` |
|     4054 |  439 | `						pStream->zText++;` |
|     2028 |  440 | `					}` |
|   288877 |  441 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  442 | `					/* Array operator: => */` |
|     4482 |  443 | `					pToken->nType = PH7_TK_ARRAY_OP;` |
|     4482 |  444 | `					pStream->zText++;` |
|     2242 |  445 | `				}else{` |
|        - |  446 | `					/* TICKET 1433-0010: Reference operator '=&' */` |
|   275120 |  447 | `					const unsigned char *zCur = pStream->zText;` |
|   275120 |  448 | `					sxu32 nLine = 0;` |
|   550216 |  449 | `					while( zCur < pStream->zEnd && zCur[0] < 0xc0 && SyisSpace(zCur[0]) ){` |
|   275098 |  450 | `						if( zCur[0] == '\n' ){` |
|        5 |  451 | `							nLine++;` |
|        2 |  452 | `						}` |
|   275098 |  453 | `						zCur++;` |
|        2 |  454 | `					}` |
|   275120 |  455 | `					if( zCur < pStream->zEnd && zCur[0] == '&' ){` |
|        - |  456 | `						/* Current operator: =& */` |
|       48 |  457 | `						pToken->nType &= ~PH7_TK_EQUAL;` |
|       48 |  458 | `						SyStringInitFromBuf(pStr,"=&",sizeof("=&")-1);` |
|        - |  459 | `						/* Update token stream */` |
|       48 |  460 | `						pStream->zText = &zCur[1];` |
|       48 |  461 | `						pStream->nLine += nLine;` |
|       23 |  462 | `					}` |
|        - |  463 | `				}` |
|   149076 |  464 | `			}` |
|   298154 |  465 | `			break;` |
|    20218 |  466 | `		case '!':` |
|    40438 |  467 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  468 | `				/* Current operator: != */` |
|    17214 |  469 | `				pStream->zText++;` |
|    17214 |  470 | `				if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  471 | `					/* Current operator: !== */` |
|    14344 |  472 | `					pStream->zText++;` |
|     7171 |  473 | `				}` |
|     8606 |  474 | `			}` |
|    40438 |  475 | `			break;` |
|    11618 |  476 | `		case '&':` |
|    23238 |  477 | `			pToken->nType \|= PH7_TK_AMPER;` |
|    23238 |  478 | `			if( pStream->zText < pStream->zEnd ){` |
|    23238 |  479 | `				if( pStream->zText[0] == '&' ){` |
|     8918 |  480 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|        - |  481 | `					/* Current operator: && */` |
|     8918 |  482 | `					pStream->zText++;` |
|    18780 |  483 | `				}else if( pStream->zText[0] == '=' ){` |
|        7 |  484 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|        - |  485 | `					/* Current operator: &= */` |
|        7 |  486 | `					pStream->zText++;` |
|        3 |  487 | `				}` |
|    11618 |  488 | `			}` |
|    23238 |  489 | `			break;` |
|     1549 |  490 | `		case '\|':` |
|     3100 |  491 | `			if( pStream->zText < pStream->zEnd ){` |
|     3100 |  492 | `				if( pStream->zText[0] == '\|' ){` |
|        - |  493 | `					/* Current operator: \|\| */` |
|     2972 |  494 | `					pStream->zText++;` |
|     1615 |  495 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  496 | `					/* Current operator: \|= */` |
|        7 |  497 | `					pStream->zText++;` |
|        3 |  498 | `				}` |
|     1549 |  499 | `			}` |
|     3100 |  500 | `			break;` |
|     7474 |  501 | `		case '+':` |
|    14950 |  502 | `			if( pStream->zText < pStream->zEnd ){` |
|    14948 |  503 | `				if( pStream->zText[0] == '+' ){` |
|        - |  504 | `					/* Current operator: ++ */` |
|    11612 |  505 | `					pStream->zText++;` |
|     9143 |  506 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  507 | `					/* Current operator: += */` |
|       44 |  508 | `					pStream->zText++;` |
|       21 |  509 | `				}` |
|     7473 |  510 | `			}` |
|    14950 |  511 | `			break;` |
|    54803 |  512 | `		case '-':` |
|   109608 |  513 | `			if( pStream->zText < pStream->zEnd ){` |
|   109608 |  514 | `				if( pStream->zText[0] == '-' ){` |
|        - |  515 | `					/* Current operator: -- */` |
|        5 |  516 | `					pStream->zText++;` |
|   109606 |  517 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  518 | `					/* Current operator: -= */` |
|       10 |  519 | `					pStream->zText++;` |
|   109600 |  520 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  521 | `					/* Current operator: -> */` |
|   109112 |  522 | `					pStream->zText++;` |
|    54555 |  523 | `				}` |
|    54803 |  524 | `			}` |
|   109608 |  525 | `			break;` |
|       96 |  526 | `		case '*':` |
|      194 |  527 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  528 | `				/* Current operator: *= */` |
|       20 |  529 | `				pStream->zText++;` |
|        9 |  530 | `			}` |
|      194 |  531 | `			break;` |
|       32 |  532 | `		case '/':` |
|       66 |  533 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  534 | `				/* Current operator: /= */` |
|        5 |  535 | `				pStream->zText++;` |
|        2 |  536 | `			}` |
|       66 |  537 | `			break;` |
|       25 |  538 | `		case '%':` |
|       52 |  539 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  540 | `				/* Current operator: %= */` |
|        3 |  541 | `				pStream->zText++;` |
|        1 |  542 | `			}` |
|       52 |  543 | `			break;` |
|       11 |  544 | `		case '^':` |
|       23 |  545 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  546 | `				/* Current operator: ^= */` |
|        9 |  547 | `				pStream->zText++;` |
|        4 |  548 | `			}` |
|       23 |  549 | `			break;` |
|    30688 |  550 | `		case '.':` |
|    61378 |  551 | `			if( pStream->zText + 1 < pStream->zEnd && pStream->zText[0] == '.' && pStream->zText[1] == '.' ){` |
|        - |  552 | `				/* Ellipsis: ... */` |
|       56 |  553 | `				pStream->zText += 2;` |
|       56 |  554 | `				pToken->nType = PH7_TK_ELLIPSIS;` |
|    61351 |  555 | `			}else if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  556 | `				/* Current operator: .= */` |
|     2910 |  557 | `				pStream->zText++;` |
|     1454 |  558 | `			}` |
|    61378 |  559 | `			break;` |
|    24389 |  560 | `		case '<':` |
|    48780 |  561 | `			if( pStream->zText < pStream->zEnd ){` |
|    48780 |  562 | `				if( pStream->zText[0] == '<' ){` |
|        - |  563 | `					/* Current operator: << */` |
|      134 |  564 | `					pStream->zText++;` |
|      134 |  565 | `					if( pStream->zText < pStream->zEnd ){` |
|      134 |  566 | `						if( pStream->zText[0] == '=' ){` |
|        - |  567 | `							/* Current operator: <<= */` |
|        9 |  568 | `							pStream->zText++;` |
|      130 |  569 | `						}else if( pStream->zText[0] == '<' ){` |
|        - |  570 | `							/* Current Token: <<<  */` |
|      112 |  571 | `							pStream->zText++;` |
|        - |  572 | `							/* This may be the beginning of a Heredoc/Nowdoc string,try to delimit it */` |
|      112 |  573 | `							rc = LexExtractHeredoc(&(*pStream),&(*pToken));` |
|      112 |  574 | `							if( rc == SXRET_OK ){` |
|        - |  575 | `								/* Here/Now doc successfuly extracted */` |
|      112 |  576 | `								return SXRET_OK;` |
|        - |  577 | `							}` |
|      ! 0 |  578 | `						}` |
|       12 |  579 | `					}` |
|    48659 |  580 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  581 | `					/* Current operator: <> */` |
|        5 |  582 | `					pStream->zText++;` |
|    48646 |  583 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  584 | `					/* Current operator: <= or <=> */` |
|       94 |  585 | `					pStream->zText++;` |
|       94 |  586 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '>' ){` |
|        - |  587 | `						/* Current operator: <=> */` |
|       51 |  588 | `						pStream->zText++;` |
|       25 |  589 | `					}` |
|       46 |  590 | `				}` |
|    24334 |  591 | `			}` |
|    48670 |  592 | `			break;` |
|     2947 |  593 | `		case '>':` |
|     5896 |  594 | `			if( pStream->zText < pStream->zEnd ){` |
|     5896 |  595 | `				if( pStream->zText[0] == '>' ){` |
|        - |  596 | `					/* Current operator: >> */` |
|       21 |  597 | `					pStream->zText++;` |
|       21 |  598 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  599 | `						/* Current operator: >>= */` |
|       11 |  600 | `						pStream->zText++;` |
|        6 |  601 | `					}` |
|     5886 |  602 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  603 | `					/* Current operator: >= */` |
|       80 |  604 | `					pStream->zText++;` |
|       39 |  605 | `				}` |
|     2947 |  606 | `			}` |
|     5896 |  607 | `			break;` |
|     1055 |  608 | `		case '?':` |
|     2112 |  609 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '?' ){` |
|        - |  610 | `				/* Null coalescing operator: ?? */` |
|       84 |  611 | `				pStream->zText++;` |
|       84 |  612 | `				if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  613 | `					/* Null coalescing assignment operator (PHP 7.4) */` |
|       54 |  614 | `					pStream->zText++;` |
|       26 |  615 | `				}` |
|       41 |  616 | `			}` |
|     2110 |  617 | `			break;` |
|      105 |  618 | `		default:` |
|      210 |  619 | `			break;` |
|        - |  620 | `		}` |
|  4440646 |  621 | `		if( pStr->nByte <= 0 ){` |
|        - |  622 | `			/* Record token length */` |
|  4440600 |  623 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  2220299 |  624 | `		}` |
|  4440646 |  625 | `		if( pToken->nType & PH7_TK_OP ){` |
|        - |  626 | `			const ph7_expr_op *pOp;` |
|        - |  627 | `			/* Check if the extracted token is an operator */` |
|   754142 |  628 | `			pOp = PH7_ExprExtractOperator(pStr,(SyToken *)SySetPeek(pStream->pSet));` |
|   754142 |  629 | `			if( pOp == 0 ){` |
|        - |  630 | `				/* Not an operator */` |
|      ! 0 |  631 | `				pToken->nType &= ~PH7_TK_OP;` |
|      ! 0 |  632 | `				if( pToken->nType <= 0 ){` |
|      ! 0 |  633 | `					pToken->nType = PH7_TK_OTHER;` |
|      ! 0 |  634 | `				}` |
|      ! 0 |  635 | `			}else{` |
|        - |  636 | `				/* Save the instance associated with this operator for later processing */` |
|   754142 |  637 | `				pToken->pUserData = (void *)pOp;` |
|        - |  638 | `			}` |
|   377070 |  639 | `		}` |
|        - |  640 | `	}` |
|        - |  641 | `	/* Tell the upper-layer to save the extracted token for later processing */` |
|  7099472 |  642 | `	return SXRET_OK;` |
|  3678972 |  643 |  |
|        - |  644 | `/***** This file contains automatically generated code ******` |
|        - |  645 | `**` |
|        - |  646 | `** The code in this file has been automatically generated by` |
|        - |  647 | `**` |
|        - |  648 | `**     $Header: /sqlite/sqlite/tool/mkkeywordhash.c` |
|        - |  649 | `**` |
|        - |  650 | `** Sligthly modified by Chems mrad <chm@symisc.net> for the PH7 engine.` |
|        - |  651 | `**` |
|        - |  652 | `** The code in this file implements a function that determines whether` |
|        - |  653 | `** or not a given identifier is really a PHP keyword.  The same thing` |
|        - |  654 | `** might be implemented more directly using a hand-written hash table.` |
|        - |  655 | `** But by using this automatically generated code, the size of the code` |
|        - |  656 | `** is substantially reduced.  This is important for embedded applications` |
|        - |  657 | `** on platforms with limited memory.` |
|        - |  658 | `*/` |
|        - |  659 | `/* Hash score: 103 */` |
|  2658828 |  660 | `static sxu32 KeywordCode(const char *z, int n){` |
|        - |  661 | `  /* zText[] encodes 532 bytes of keywords in 333 bytes */` |
|        - |  662 | `  /*   extendswitchprintegerequire_oncenddeclareturnamespacechobject      */` |
|        - |  663 | `  /*   hrowbooleandefaultrycaselfinalistaticlonewconstringlobaluse        */` |
|        - |  664 | `  /*   lseifloatvarrayANDIEchoUSECHOabstractclasscontinuendifunction      */` |
|        - |  665 | `  /*   diendwhilevaldoexitgotoimplementsinclude_oncemptyinstanceof        */` |
|        - |  666 | `  /*   interfacendforeachissetparentprivateprotectedpublicatchunset       */` |
|        - |  667 | `  /*   xorARRAYASArrayEXITUNSETXORbreak                                   */` |
|        - |  668 | `  static const char zText[332] = {` |
|        - |  669 | `    'e','x','t','e','n','d','s','w','i','t','c','h','p','r','i','n','t','e',` |
|        - |  670 | `    'g','e','r','e','q','u','i','r','e','_','o','n','c','e','n','d','d','e',` |
|        - |  671 | `    'c','l','a','r','e','t','u','r','n','a','m','e','s','p','a','c','e','c',` |
|        - |  672 | `    'h','o','b','j','e','c','t','h','r','o','w','b','o','o','l','e','a','n',` |
|        - |  673 | `    'd','e','f','a','u','l','t','r','y','c','a','s','e','l','f','i','n','a',` |
|        - |  674 | `    'l','i','s','t','a','t','i','c','l','o','n','e','w','c','o','n','s','t',` |
|        - |  675 | `    'r','i','n','g','l','o','b','a','l','u','s','e','l','s','e','i','f','l',` |
|        - |  676 | `    'o','a','t','v','a','r','r','a','y','A','N','D','I','E','c','h','o','U',` |
|        - |  677 | `    'S','E','C','H','O','a','b','s','t','r','a','c','t','c','l','a','s','s',` |
|        - |  678 | `    'c','o','n','t','i','n','u','e','n','d','i','f','u','n','c','t','i','o',` |
|        - |  679 | `    'n','d','i','e','n','d','w','h','i','l','e','v','a','l','d','o','e','x',` |
|        - |  680 | `    'i','t','g','o','t','o','i','m','p','l','e','m','e','n','t','s','i','n',` |
|        - |  681 | `    'c','l','u','d','e','_','o','n','c','e','m','p','t','y','i','n','s','t',` |
|        - |  682 | `    'a','n','c','e','o','f','i','n','t','e','r','f','a','c','e','n','d','f',` |
|        - |  683 | `    'o','r','e','a','c','h','i','s','s','e','t','p','a','r','e','n','t','p',` |
|        - |  684 | `    'r','i','v','a','t','e','p','r','o','t','e','c','t','e','d','p','u','b',` |
|        - |  685 | `    'l','i','c','a','t','c','h','u','n','s','e','t','x','o','r','A','R','R',` |
|        - |  686 | `    'A','Y','A','S','A','r','r','a','y','E','X','I','T','U','N','S','E','T',` |
|        - |  687 | `    'X','O','R','b','r','e','a','k'` |
|        - |  688 | `  };` |
|        - |  689 | `  static const unsigned char aHash[151] = {` |
|        - |  690 |  |
|        - |  691 |  |
|        - |  692 | `      74,   0,  51,   0,  76,   0,   0,  20,   0,   0,   0,  50,   0,` |
|        - |  693 | `      80,  34,   0,  36,   0,   0,  64,  16,   0,   0,  17,   0,   1,` |
|        - |  694 | `      19,  84,  66,   0,  43,  45,  78,   0,   0,  53,  56,   0,   0,` |
|        - |  695 |  |
|        - |  696 | `      72,  14,   0,  71,   0,  38,   6,   0,   0,   0,  73,   0,   0,` |
|        - |  697 | `       3,   0,  41,   5,  52,  57,  32,   0,  60,  63,   0,  69,  82,` |
|        - |  698 | `      30,   0,  79,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,` |
|        - |  699 |  |
|        - |  700 | `      62,   0,  11,   0,   0,  58,   0,   0,   0,   0,  59,  75,   0,` |
|        - |  701 |  |
|        - |  702 | `  };` |
|        - |  703 | `  static const unsigned char aNext[84] = {` |
|        - |  704 |  |
|        - |  705 |  |
|        - |  706 |  |
|        - |  707 |  |
|        - |  708 |  |
|        - |  709 |  |
|        - |  710 | `      42,   0,   0,   0,  70,  55` |
|        - |  711 | `  };` |
|        - |  712 | `  static const unsigned char aLen[84] = {` |
|        - |  713 | `       7,   9,   6,   5,   7,  12,   7,   2,  10,   7,   6,   9,   4,` |
|        - |  714 | `       6,   5,   7,   4,   3,   7,   3,   4,   4,   5,   4,   6,   5,` |
|        - |  715 | `       2,   3,   5,   6,   6,   3,   6,   4,   2,   5,   3,   5,   3,` |
|        - |  716 | `       3,   4,   3,   4,   8,   5,   2,   8,   5,   8,   3,   8,   5,` |
|        - |  717 | `       4,   2,   4,   4,  10,  12,   7,   5,  10,   9,   3,   6,  10,` |
|        - |  718 | `       3,   7,   2,   5,   6,   7,   9,   6,   5,   5,   3,   5,   2,` |
|        - |  719 | `       5,   4,   5,   3,   2,   5` |
|        - |  720 | `  };` |
|        - |  721 | `  static const sxu16 aOffset[84] = {` |
|        - |  722 |  |
|        - |  723 | `      55,  60,  65,  65,  70,  72,  78,  81,  83,  86,  90,  92,  97,` |
|        - |  724 | `     100, 100, 103, 106, 111, 117, 119, 119, 123, 124, 129, 130, 135,` |
|        - |  725 | `     137, 139, 143, 145, 149, 157, 159, 162, 169, 173, 181, 183, 186,` |
|        - |  726 | `     190, 194, 196, 200, 204, 214, 214, 225, 230, 240, 240, 248, 248,` |
|        - |  727 | `     251, 251, 252, 258, 263, 269, 276, 285, 290, 295, 300, 303, 308,` |
|        - |  728 | `     310, 315, 319, 324, 325, 327` |
|        - |  729 | `  };` |
|        - |  730 | `  static const sxu32 aCode[84] = {` |
|        - |  731 | `    PH7_TKWRD_EXTENDS,   PH7_TKWRD_ENDSWITCH,   PH7_TKWRD_SWITCH,    PH7_TKWRD_PRINT,   PH7_TKWRD_INT,` |
|        - |  732 | `    PH7_TKWRD_REQONCE,   PH7_TKWRD_REQUIRE,     PH7_TKWRD_SEQ,       PH7_TKWRD_ENDDEC,    PH7_TKWRD_DECLARE,` |
|        - |  733 | `    PH7_TKWRD_RETURN,    PH7_TKWRD_NAMESPACE,   PH7_TKWRD_ECHO,      PH7_TKWRD_OBJECT,    PH7_TKWRD_THROW,` |
|        - |  734 | `    PH7_TKWRD_BOOL,      PH7_TKWRD_BOOL,        PH7_TKWRD_AND,       PH7_TKWRD_DEFAULT,   PH7_TKWRD_TRY,` |
|        - |  735 | `    PH7_TKWRD_CASE,      PH7_TKWRD_SELF,        PH7_TKWRD_FINAL,     PH7_TKWRD_LIST,      PH7_TKWRD_STATIC,` |
|        - |  736 | `    PH7_TKWRD_CLONE,     PH7_TKWRD_SNE,         PH7_TKWRD_NEW,       PH7_TKWRD_CONST,     PH7_TKWRD_STRING,` |
|        - |  737 | `    PH7_TKWRD_GLOBAL,    PH7_TKWRD_USE,         PH7_TKWRD_ELIF,      PH7_TKWRD_ELSE,      PH7_TKWRD_IF,` |
|        - |  738 | `    PH7_TKWRD_FLOAT,     PH7_TKWRD_VAR,         PH7_TKWRD_ARRAY,     PH7_TKWRD_AND,       PH7_TKWRD_DIE,` |
|        - |  739 | `    PH7_TKWRD_ECHO,      PH7_TKWRD_USE,         PH7_TKWRD_ECHO,      PH7_TKWRD_ABSTRACT,  PH7_TKWRD_CLASS,` |
|        - |  740 | `    PH7_TKWRD_AS,        PH7_TKWRD_CONTINUE,    PH7_TKWRD_ENDIF,     PH7_TKWRD_FUNCTION,  PH7_TKWRD_DIE,` |
|        - |  741 | `    PH7_TKWRD_ENDWHILE,  PH7_TKWRD_WHILE,       PH7_TKWRD_EVAL,      PH7_TKWRD_DO,        PH7_TKWRD_EXIT,` |
|        - |  742 | `    PH7_TKWRD_GOTO,      PH7_TKWRD_IMPLEMENTS,  PH7_TKWRD_INCONCE,   PH7_TKWRD_INCLUDE,   PH7_TKWRD_EMPTY,` |
|        - |  743 | `    PH7_TKWRD_INSTANCEOF,PH7_TKWRD_INTERFACE,   PH7_TKWRD_INT,       PH7_TKWRD_ENDFOR,    PH7_TKWRD_END4EACH,` |
|        - |  744 | `    PH7_TKWRD_FOR,       PH7_TKWRD_FOREACH,     PH7_TKWRD_OR,        PH7_TKWRD_ISSET,     PH7_TKWRD_PARENT,` |
|        - |  745 | `    PH7_TKWRD_PRIVATE,   PH7_TKWRD_PROTECTED,   PH7_TKWRD_PUBLIC,    PH7_TKWRD_CATCH,     PH7_TKWRD_UNSET,` |
|        - |  746 | `    PH7_TKWRD_XOR,       PH7_TKWRD_ARRAY,       PH7_TKWRD_AS,        PH7_TKWRD_ARRAY,     PH7_TKWRD_EXIT,` |
|        - |  747 | `    PH7_TKWRD_UNSET,     PH7_TKWRD_XOR,         PH7_TKWRD_OR,        PH7_TKWRD_BREAK` |
|        - |  748 | `  };` |
|        - |  749 | `  int h, i;` |
|  2658828 |  750 | `  if( n<2 ) return PH7_TK_ID;` |
|  2561472 |  751 | `  h = (((int)z[0]*4) ^ ((int)z[n-1]*3) ^ n) % 151;` |
|  3921256 |  752 | `  for(i=((int)aHash[h])-1; i>=0; i=((int)aNext[i])-1){` |
|  2267988 |  753 | `    if( (int)aLen[i]==n && SyMemcmp(&zText[aOffset[i]],z,n)==0 ){` |
|        - |  754 | `       /* PH7_TKWRD_EXTENDS */` |
|        - |  755 | `       /* PH7_TKWRD_ENDSWITCH */` |
|        - |  756 | `       /* PH7_TKWRD_SWITCH */` |
|        - |  757 | `       /* PH7_TKWRD_PRINT */` |
|        - |  758 | `       /* PH7_TKWRD_INT */` |
|        - |  759 | `       /* PH7_TKWRD_REQONCE */` |
|        - |  760 | `       /* PH7_TKWRD_REQUIRE */` |
|        - |  761 | `       /* PH7_TKWRD_SEQ */` |
|        - |  762 | `       /* PH7_TKWRD_ENDDEC */` |
|        - |  763 | `       /* PH7_TKWRD_DECLARE */` |
|        - |  764 | `       /* PH7_TKWRD_RETURN */` |
|        - |  765 | `       /* PH7_TKWRD_NAMESPACE */` |
|        - |  766 | `       /* PH7_TKWRD_ECHO */` |
|        - |  767 | `       /* PH7_TKWRD_OBJECT */` |
|        - |  768 | `       /* PH7_TKWRD_THROW */` |
|        - |  769 | `       /* PH7_TKWRD_BOOL */` |
|        - |  770 | `       /* PH7_TKWRD_BOOL */` |
|        - |  771 | `       /* PH7_TKWRD_AND */` |
|        - |  772 | `       /* PH7_TKWRD_DEFAULT */` |
|        - |  773 | `       /* PH7_TKWRD_TRY */` |
|        - |  774 | `       /* PH7_TKWRD_CASE */` |
|        - |  775 | `       /* PH7_TKWRD_SELF */` |
|        - |  776 | `       /* PH7_TKWRD_FINAL */` |
|        - |  777 | `       /* PH7_TKWRD_LIST */` |
|        - |  778 | `       /* PH7_TKWRD_STATIC */` |
|        - |  779 | `       /* PH7_TKWRD_CLONE */` |
|        - |  780 | `       /* PH7_TKWRD_SNE */` |
|        - |  781 | `       /* PH7_TKWRD_NEW */` |
|        - |  782 | `       /* PH7_TKWRD_CONST */` |
|        - |  783 | `       /* PH7_TKWRD_STRING */` |
|        - |  784 | `       /* PH7_TKWRD_GLOBAL */` |
|        - |  785 | `       /* PH7_TKWRD_USE */` |
|        - |  786 | `       /* PH7_TKWRD_ELIF */` |
|        - |  787 | `       /* PH7_TKWRD_ELSE */` |
|        - |  788 | `       /* PH7_TKWRD_IF */` |
|        - |  789 | `       /* PH7_TKWRD_FLOAT */` |
|        - |  790 | `       /* PH7_TKWRD_VAR */` |
|        - |  791 | `       /* PH7_TKWRD_ARRAY */` |
|        - |  792 | `       /* PH7_TKWRD_AND */` |
|        - |  793 | `       /* PH7_TKWRD_DIE */` |
|        - |  794 | `       /* PH7_TKWRD_ECHO */` |
|        - |  795 | `       /* PH7_TKWRD_USE */` |
|        - |  796 | `       /* PH7_TKWRD_ECHO */` |
|        - |  797 | `       /* PH7_TKWRD_ABSTRACT */` |
|        - |  798 | `       /* PH7_TKWRD_CLASS */` |
|        - |  799 | `       /* PH7_TKWRD_AS */` |
|        - |  800 | `       /* PH7_TKWRD_CONTINUE */` |
|        - |  801 | `       /* PH7_TKWRD_ENDIF */` |
|        - |  802 | `       /* PH7_TKWRD_FUNCTION */` |
|        - |  803 | `       /* PH7_TKWRD_DIE */` |
|        - |  804 | `       /* PH7_TKWRD_ENDWHILE */` |
|        - |  805 | `       /* PH7_TKWRD_WHILE */` |
|        - |  806 | `       /* PH7_TKWRD_EVAL */` |
|        - |  807 | `       /* PH7_TKWRD_DO */` |
|        - |  808 | `       /* PH7_TKWRD_EXIT */` |
|        - |  809 | `       /* PH7_TKWRD_GOTO */` |
|        - |  810 | `       /* PH7_TKWRD_IMPLEMENTS */` |
|        - |  811 | `       /* PH7_TKWRD_INCONCE */` |
|        - |  812 | `       /* PH7_TKWRD_INCLUDE */` |
|        - |  813 | `       /* PH7_TKWRD_EMPTY */` |
|        - |  814 | `       /* PH7_TKWRD_INSTANCEOF */` |
|        - |  815 | `       /* PH7_TKWRD_INTERFACE */` |
|        - |  816 | `       /* PH7_TKWRD_INT */` |
|        - |  817 | `       /* PH7_TKWRD_ENDFOR */` |
|        - |  818 | `       /* PH7_TKWRD_END4EACH */` |
|        - |  819 | `       /* PH7_TKWRD_FOR */` |
|        - |  820 | `       /* PH7_TKWRD_FOREACH */` |
|        - |  821 | `       /* PH7_TKWRD_OR */` |
|        - |  822 | `       /* PH7_TKWRD_ISSET */` |
|        - |  823 | `       /* PH7_TKWRD_PARENT */` |
|        - |  824 | `       /* PH7_TKWRD_PRIVATE */` |
|        - |  825 | `       /* PH7_TKWRD_PROTECTED */` |
|        - |  826 | `       /* PH7_TKWRD_PUBLIC */` |
|        - |  827 | `       /* PH7_TKWRD_CATCH */` |
|        - |  828 | `       /* PH7_TKWRD_UNSET */` |
|        - |  829 | `       /* PH7_TKWRD_XOR */` |
|        - |  830 | `       /* PH7_TKWRD_ARRAY */` |
|        - |  831 | `       /* PH7_TKWRD_AS */` |
|        - |  832 | `       /* PH7_TKWRD_ARRAY */` |
|        - |  833 | `       /* PH7_TKWRD_EXIT */` |
|        - |  834 | `       /* PH7_TKWRD_UNSET */` |
|        - |  835 | `       /* PH7_TKWRD_XOR */` |
|        - |  836 | `       /* PH7_TKWRD_OR */` |
|        - |  837 | `       /* PH7_TKWRD_BREAK */` |
|   908204 |  838 | `      return aCode[i];` |
|        - |  839 | `    }` |
|   679892 |  840 | `  }` |
|        - |  841 | `  /* Linear fallback for keywords not in the auto-generated hash table */` |
|  1653270 |  842 | `  if( n==5 && SyMemcmp(z,"trait",5)==0 ) return PH7_TKWRD_TRAIT;` |
|  1653214 |  843 | `  if( n==9 && SyMemcmp(z,"insteadof",9)==0 ) return PH7_TKWRD_INSTEADOF;` |
|  1653210 |  844 | `  if( n==7 && SyMemcmp(z,"finally",7)==0 ) return PH7_TKWRD_FINALLY;` |
|  1653180 |  845 | `  if( n==5 && SyMemcmp(z,"yield",5)==0 ) return PH7_TKWRD_YIELD;` |
|  1653146 |  846 | `  if( n==5 && SyMemcmp(z,"match",5)==0 ) return PH7_TKWRD_MATCH;` |
|  1653078 |  847 | `  return PH7_TK_ID;` |
|  1329415 |  848 |  |
|        - |  849 | `/* --- End of Automatically generated code --- */` |
|        - |  850 | `/*` |
|        - |  851 | ` * Extract a heredoc/nowdoc text from a raw PHP input.` |
|        - |  852 | ` * According to the PHP language reference manual:` |
|        - |  853 | ` *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier` |
|        - |  854 | ` *  is provided, then a newline. The string itself follows, and then the same identifier again` |
|        - |  855 | ` *  to close the quotation.` |
|        - |  856 | ` *  The closing identifier must begin in the first column of the line. Also, the identifier must` |
|        - |  857 | ` *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric` |
|        - |  858 | ` *  characters and underscores, and must start with a non-digit character or underscore.` |
|        - |  859 | ` *  Heredoc text behaves just like a double-quoted string, without the double quotes.` |
|        - |  860 | ` *  This means that quotes in a heredoc do not need to be escaped, but the escape codes listed` |
|        - |  861 | ` *  above can still be used. Variables are expanded, but the same care must be taken when expressing` |
|        - |  862 | ` *  complex variables inside a heredoc as with strings.` |
|        - |  863 | ` *  Nowdocs are to single-quoted strings what heredocs are to double-quoted strings.` |
|        - |  864 | ` *  A nowdoc is specified similarly to a heredoc, but no parsing is done inside a nowdoc.` |
|        - |  865 | ` *  The construct is ideal for embedding PHP code or other large blocks of text without the need` |
|        - |  866 | ` *  for escaping. It shares some features in common with the SGML <![CDATA[ ]]> construct, in that` |
|        - |  867 | ` *  it declares a block of text which is not for parsing.` |
|        - |  868 | ` *  A nowdoc is identified with the same <<< sequence used for heredocs, but the identifier which follows` |
|        - |  869 | ` *  is enclosed in single quotes, e.g. <<<'EOT'. All the rules for heredoc identifiers also apply to nowdoc` |
|        - |  870 | ` *  identifiers, especially those regarding the appearance of the closing identifier.` |
|        - |  871 | ` * Symisc Extension:` |
|        - |  872 | ` * The closing delimiter can now start with a digit or undersocre or it can be an UTF-8 stream.` |
|        - |  873 | ` * Example:` |
|        - |  874 | ` *  <<<123` |
|        - |  875 | ` *    HEREDOC Here` |
|        - |  876 | ` * 123` |
|        - |  877 | ` *  or` |
|        - |  878 | ` *  <<<___` |
|        - |  879 | ` *   HEREDOC Here` |
|        - |  880 | ` *  ___` |
|        - |  881 | ` */` |
|      110 |  882 | `static sxi32 LexExtractHeredoc(SyStream *pStream,SyToken *pToken)` |
|        2 |  883 |  |
|      112 |  884 | `	const unsigned char *zIn  = pStream->zText;` |
|      112 |  885 | `	const unsigned char *zEnd = pStream->zEnd;` |
|        - |  886 | `	const unsigned char *zPtr;` |
|      112 |  887 | `	sxu8 bNowDoc = FALSE;` |
|        - |  888 | `	SyString sDelim;` |
|        - |  889 | `	SyString sStr;` |
|        - |  890 | `	/* Jump leading white spaces */` |
|      124 |  891 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|       13 |  892 | `		zIn++;` |
|        1 |  893 | `	}` |
|      112 |  894 | `	if( zIn >= zEnd ){` |
|        - |  895 | `		/* A simple symbol,return immediately */` |
|      ! 0 |  896 | `		return SXERR_CONTINUE;` |
|        - |  897 | `	}` |
|      112 |  898 | `	if( zIn[0] == '\'' \|\| zIn[0] == '"' ){` |
|        - |  899 | `		/* Make sure we are dealing with a nowdoc */` |
|       44 |  900 | `		bNowDoc =  zIn[0] == '\'' ? TRUE : FALSE;` |
|       44 |  901 | `		zIn++;` |
|       21 |  902 | `	}` |
|      112 |  903 | `	if( zIn[0] < 0xc0 && !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|        - |  904 | `		/* Invalid delimiter,return immediately */` |
|      ! 0 |  905 | `		return SXERR_CONTINUE;` |
|        - |  906 | `	}` |
|        - |  907 | `	/* Isolate the identifier */` |
|      112 |  908 | `	sDelim.zString = (const char *)zIn;` |
|      118 |  909 | `	for(;;){` |
|      238 |  910 | `		zPtr = zIn;` |
|        - |  911 | `		/* Skip alphanumeric stream */` |
|      756 |  912 | `		while( zPtr < zEnd && zPtr[0] < 0xc0 && (SyisAlphaNum(zPtr[0]) \|\| zPtr[0] == '_') ){` |
|      402 |  913 | `			zPtr++;` |
|        2 |  914 | `		}` |
|      238 |  915 | `		if( zPtr < zEnd && zPtr[0] >= 0xc0 ){` |
|       19 |  916 | `			zPtr++;` |
|        - |  917 | `			/* UTF-8 stream */` |
|       37 |  918 | `			while( zPtr < zEnd && ((zPtr[0] & 0xc0) == 0x80) ){` |
|       19 |  919 | `				zPtr++;` |
|        1 |  920 | `			}` |
|        9 |  921 | `		}` |
|      238 |  922 | `		if( zPtr == zIn ){` |
|        - |  923 | `			/* Not an UTF-8 or alphanumeric stream */` |
|      112 |  924 | `			break;` |
|        - |  925 | `		}` |
|        - |  926 | `		/* Synchronize pointers */` |
|      128 |  927 | `		zIn = zPtr;` |
|        2 |  928 | `	}` |
|        - |  929 | `	/* Get the identifier length */` |
|      112 |  930 | `	sDelim.nByte = (sxu32)((const char *)zIn-sDelim.zString);` |
|      112 |  931 | `	if( zIn[0] == '"' \|\| (bNowDoc && zIn[0] == '\'') ){` |
|        - |  932 | `		/* Jump the trailing single quote */` |
|       44 |  933 | `		zIn++;` |
|       21 |  934 | `	}` |
|        - |  935 | `	/* Jump trailing white spaces */` |
|      112 |  936 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|      ! 0 |  937 | `		zIn++;` |
|      ! 0 |  938 | `	}` |
|      112 |  939 | `	if( sDelim.nByte <= 0 \|\| zIn >= zEnd \|\| zIn[0] != '\n' ){` |
|        - |  940 | `		/* Invalid syntax */` |
|      ! 0 |  941 | `		return SXERR_CONTINUE;` |
|        - |  942 | `	}` |
|      112 |  943 | `	pStream->nLine++; /* Increment line counter */` |
|      112 |  944 | `	zIn++;` |
|        - |  945 | `	/* Isolate the delimited string */` |
|      112 |  946 | `	sStr.zString = (const char *)zIn;` |
|        - |  947 | `	/* PHP 7.3 flexible heredoc/nowdoc: the closing marker may be preceded` |
|        - |  948 | `	 * by whitespace (spaces/tabs), and may be followed by any non-identifier` |
|        - |  949 | `	 * character. The indent count is recorded in pToken->pUserData and the` |
|        - |  950 | `	 * compile phase strips it from each body line. */` |
|        - |  951 | `	{` |
|      112 |  952 | `		const unsigned char *zMarkerLine = zIn; /* Start of marker's line (set on match) */` |
|      112 |  953 | `		sxu32 nIndent = 0;` |
|      225 |  954 | `		for(;;){` |
|      282 |  955 | `			const unsigned char *zLineStart = zIn;` |
|        - |  956 | `			/* Skip leading space/tab on this line */` |
|      806 |  957 | `			while( zIn < zEnd && (zIn[0] == ' ' \|\| zIn[0] == '\t') ){` |
|      386 |  958 | `				zIn++;` |
|        2 |  959 | `			}` |
|      280 |  960 | `			if( (sxu32)(zEnd - zIn) >= sDelim.nByte` |
|      281 |  961 | `				&& SyMemcmp((const void *)sDelim.zString,(const void *)zIn,sDelim.nByte) == 0 ){` |
|        - |  962 | `				int bIdentCont;` |
|      110 |  963 | `				zPtr = &zIn[sDelim.nByte];` |
|        - |  964 | `				/* Disambiguate: next byte must not continue an identifier.` |
|        - |  965 | `				 * A leading byte >= 0xc0 starts a multi-byte UTF-8 sequence,` |
|        - |  966 | `				 * which PHP identifiers may contain, so treat it as ident. */` |
|      110 |  967 | `				if( zPtr >= zEnd ){` |
|      ! 0 |  968 | `					bIdentCont = 0;` |
|      110 |  969 | `				}else if( zPtr[0] >= 0xc0 ){` |
|      ! 0 |  970 | `					bIdentCont = 1;` |
|      ! 0 |  971 | `				}else{` |
|      110 |  972 | `					bIdentCont = (SyisAlphaNum(zPtr[0]) \|\| zPtr[0] == '_');` |
|        - |  973 | `				}` |
|      110 |  974 | `				if( !bIdentCont ){` |
|        - |  975 | `					/* Closing marker found */` |
|      110 |  976 | `					nIndent = (sxu32)(zIn - zLineStart);` |
|      110 |  977 | `					zMarkerLine = zLineStart;` |
|      110 |  978 | `					pStream->zText = zPtr; /* Cursor right after identifier */` |
|      110 |  979 | `					break;` |
|        - |  980 | `				}` |
|      ! 0 |  981 | `			}` |
|        - |  982 | `			/* Not the closing marker on this line; walk to next newline */` |
|     2824 |  983 | `			while( zIn < zEnd && zIn[0] != '\n' ){` |
|     2652 |  984 | `				zIn++;` |
|        2 |  985 | `			}` |
|      174 |  986 | `			if( zIn >= zEnd ){` |
|        - |  987 | `				/* End of input without finding the closing marker */` |
|        3 |  988 | `				pStream->zText = pStream->zEnd;` |
|        3 |  989 | `				zMarkerLine = zIn;` |
|        3 |  990 | `				break;` |
|        - |  991 | `			}` |
|      172 |  992 | `			pStream->nLine++;` |
|      172 |  993 | `			zIn++;` |
|        2 |  994 | `		}` |
|        - |  995 | `		/* Body runs from sStr.zString up to just before the marker line */` |
|      112 |  996 | `		sStr.nByte = (sxu32)((const char *)zMarkerLine - sStr.zString);` |
|      112 |  997 | `		pToken->nType = bNowDoc ? PH7_TK_NOWDOC : PH7_TK_HEREDOC;` |
|      112 |  998 | `		SyStringDupPtr(&pToken->sData,&sStr);` |
|        - |  999 | `		/* Strip exactly one line terminator that precedes the marker's line. */` |
|      110 | 1000 | `		if( pToken->sData.nByte > 0` |
|      108 | 1001 | `			&& pToken->sData.zString[pToken->sData.nByte - 1] == '\n' ){` |
|      102 | 1002 | `			pToken->sData.nByte--;` |
|      100 | 1003 | `			if( pToken->sData.nByte > 0` |
|      102 | 1004 | `				&& pToken->sData.zString[pToken->sData.nByte - 1] == '\r' ){` |
|      ! 0 | 1005 | `				pToken->sData.nByte--;` |
|      ! 0 | 1006 | `			}` |
|       50 | 1007 | `		}` |
|      112 | 1008 | `		pToken->pUserData = SX_INT_TO_PTR(nIndent);` |
|        - | 1009 | `	}` |
|        - | 1010 | `	/* All done */` |
|      112 | 1011 | `	return SXRET_OK;` |
|       57 | 1012 |  |
|        - | 1013 | `/*` |
|        - | 1014 | ` * Tokenize a raw PHP input.` |
|        - | 1015 | ` * This is the public tokenizer called by most code generator routines.` |
|        - | 1016 | ` */` |
|    14060 | 1017 | `PH7_PRIVATE sxi32 PH7_TokenizePHP(const char *zInput,sxu32 nLen,sxu32 nLineStart,SySet *pOut)` |
|        2 | 1018 |  |
|        - | 1019 | `	SyLex sLexer;` |
|        - | 1020 | `	sxi32 rc;` |
|        - | 1021 | `	/* Initialize the lexer */` |
|    14062 | 1022 | `	rc = SyLexInit(&sLexer,&(*pOut),TokenizePHP,0);` |
|    14062 | 1023 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1024 | `		return rc;` |
|        - | 1025 | `	}` |
|    14062 | 1026 | `	sLexer.sStream.nLine = nLineStart;` |
|        - | 1027 | `	/* Tokenize input */` |
|    14062 | 1028 | `	rc = SyLexTokenizeInput(&sLexer,zInput,nLen,0,0,0);` |
|        - | 1029 | `	/* Release the lexer */` |
|    14062 | 1030 | `	SyLexRelease(&sLexer);` |
|        - | 1031 | `	/* Tokenization result */` |
|    14062 | 1032 | `	return rc;` |
|     7032 | 1033 |  |
|        - | 1034 | `/*` |
|        - | 1035 | ` * High level public tokenizer.` |
|        - | 1036 | ` *  Tokenize the input into PHP tokens and raw tokens [i.e: HTML,XML,Raw text...].` |
|        - | 1037 | ` * According to the PHP language reference manual` |
|        - | 1038 | ` *   When PHP parses a file, it looks for opening and closing tags, which tell PHP` |
|        - | 1039 | ` *   to start and stop interpreting the code between them. Parsing in this manner allows` |
|        - | 1040 | ` *   PHP to be embedded in all sorts of different documents, as everything outside of a pair` |
|        - | 1041 | ` *   of opening and closing tags is ignored by the PHP parser. Most of the time you will see` |
|        - | 1042 | ` *   PHP embedded in HTML documents, as in this example.` |
|        - | 1043 | ` *   <?php echo 'While this is going to be parsed.'; ?>` |
|        - | 1044 | ` *   <p>This will also be ignored.</p>` |
|        - | 1045 | ` *   You can also use more advanced structures:` |
|        - | 1046 | ` *   Example #1 Advanced escaping` |
|        - | 1047 | ` * <?php` |
|        - | 1048 | ` * if ($expression) {` |
|        - | 1049 | ` *   ?>` |
|        - | 1050 | ` *   <strong>This is true.</strong>` |
|        - | 1051 | ` *   <?php` |
|        - | 1052 | ` * } else {` |
|        - | 1053 | ` *   ?>` |
|        - | 1054 | ` *   <strong>This is false.</strong>` |
|        - | 1055 | ` *   <?php` |
|        - | 1056 | ` * }` |
|        - | 1057 | ` * ?>` |
|        - | 1058 | ` * This works as expected, because when PHP hits the ?> closing tags, it simply starts outputting` |
|        - | 1059 | ` * whatever it finds (except for an immediately following newline - see instruction separation ) until it hits` |
|        - | 1060 | ` * another opening tag. The example given here is contrived, of course, but for outputting large blocks of text` |
|        - | 1061 | ` * dropping out of PHP parsing mode is generally more efficient than sending all of the text through echo() or print().` |
|        - | 1062 | ` * There are four different pairs of opening and closing tags which can be used in PHP. Three of those, <?php ?>` |
|        - | 1063 | ` * <script language="php"> </script>  and <? ?> are always available. The other two are short tags and ASP style` |
|        - | 1064 | ` * tags, and can be turned on and off from the php.ini configuration file. As such, while some people find short tags` |
|        - | 1065 | ` * and ASP style tags convenient, they are less portable, and generally not recommended.` |
|        - | 1066 | ` * Note:` |
|        - | 1067 | ` * Also note that if you are embedding PHP within XML or XHTML you will need to use the <?php ?> tags to remain` |
|        - | 1068 | ` * compliant with standards.` |
|        - | 1069 | ` * Example #2 PHP Opening and Closing Tags` |
|        - | 1070 | ` * 1.  <?php echo 'if you want to serve XHTML or XML documents, do it like this'; ?>` |
|        - | 1071 | ` * 2.  <script language="php">` |
|        - | 1072 | ` *       echo 'some editors (like FrontPage) don\'t` |
|        - | 1073 | ` *             like processing instructions';` |
|        - | 1074 | ` *   </script>` |
|        - | 1075 | ` *` |
|        - | 1076 | ` * 3.  <? echo 'this is the simplest, an SGML processing instruction'; ?>` |
|        - | 1077 | ` *   <?= expression ?> This is a shortcut for "<? echo expression ?>"` |
|        - | 1078 | ` */` |
|    11514 | 1079 | `PH7_PRIVATE sxi32 PH7_TokenizeRawText(const char *zInput,sxu32 nLen,SySet *pOut)` |
|        2 | 1080 |  |
|    11516 | 1081 | `	const char *zEnd = &zInput[nLen];` |
|    11516 | 1082 | `	const char *zIn  = zInput;` |
|        - | 1083 | `	const char *zCur,*zCurEnd;` |
|    11516 | 1084 | `	SyString sCtag = { 0, 0 };     /* Closing tag */` |
|        - | 1085 | `	SyToken sToken;` |
|        - | 1086 | `	SyString sDoc;` |
|        - | 1087 | `	sxu32 nLine;` |
|        - | 1088 | `	sxi32 iNest;` |
|        - | 1089 | `	sxi32 rc;` |
|        - | 1090 | `	/* Tokenize the input into PHP tokens and raw tokens */` |
|    11516 | 1091 | `	nLine = 1;` |
|    11516 | 1092 | `	zCur = zCurEnd   = 0; /* Prevent compiler warning */` |
|    11516 | 1093 | `	sToken.pUserData = 0;` |
|    11516 | 1094 | `	iNest = 0;` |
|    11516 | 1095 | `	sDoc.nByte = 0;` |
|    11516 | 1096 | `	sDoc.zString = ""; /* cc warning */` |
|    11516 | 1097 | `	for(;;){` |
|    23034 | 1098 | `		if( zIn >= zEnd ){` |
|        - | 1099 | `			/* End of input reached */` |
|    11512 | 1100 | `			break;` |
|        - | 1101 | `		}` |
|    11524 | 1102 | `		sToken.nLine = nLine;` |
|    11524 | 1103 | `		zCur = zIn;` |
|    11524 | 1104 | `		zCurEnd = 0;` |
|    11532 | 1105 | `		while( zIn < zEnd ){` |
|    11528 | 1106 | `			 if( zIn[0] == '<' ){` |
|    11520 | 1107 | `				const char *zTmp = zIn; /* End of raw input marker */` |
|    11520 | 1108 | `				zIn++;` |
|    11520 | 1109 | `				if( zIn < zEnd ){` |
|    11520 | 1110 | `					if( zIn[0] == '?' ){` |
|    11520 | 1111 | `						zIn++;` |
|    11520 | 1112 | `						if( (sxu32)(zEnd - zIn) >= sizeof("php")-1 &&  SyStrnicmp(zIn,"php",sizeof("php")-1) == 0 ){` |
|        - | 1113 | `							/* opening tag: <?php */` |
|    11518 | 1114 | `							zIn += sizeof("php")-1;` |
|     5758 | 1115 | `						}` |
|        - | 1116 | `						/* Look for the closing tag '?>' */` |
|    11520 | 1117 | `						SyStringInitFromBuf(&sCtag,"?>",sizeof("?>")-1);` |
|    11520 | 1118 | `						zCurEnd = zTmp;` |
|    11520 | 1119 | `						break;` |
|        - | 1120 | `					}` |
|      ! 0 | 1121 | `				}` |
|      ! 0 | 1122 | `			}else{` |
|       10 | 1123 | `				if( zIn[0] == '\n' ){` |
|       10 | 1124 | `					nLine++;` |
|        4 | 1125 | `				}` |
|       10 | 1126 | `				zIn++;` |
|        - | 1127 | `			 }` |
|        2 | 1128 | `		} /* While(zIn < zEnd) */` |
|    11524 | 1129 | `		if( zCurEnd == 0 ){` |
|        5 | 1130 | `			zCurEnd = zIn;` |
|        2 | 1131 | `		}` |
|        - | 1132 | `		/* Save the raw token */` |
|    11524 | 1133 | `		SyStringInitFromBuf(&sToken.sData,zCur,zCurEnd - zCur);` |
|    11524 | 1134 | `		sToken.nType = PH7_TOKEN_RAW;` |
|    11524 | 1135 | `		rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|    11524 | 1136 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1137 | `			return rc;` |
|        - | 1138 | `		}` |
|    11524 | 1139 | `		if( zIn >= zEnd ){` |
|        5 | 1140 | `			break;` |
|        - | 1141 | `		}` |
|        - | 1142 | `		/* Ignore leading white space */` |
|    24928 | 1143 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    13410 | 1144 | `			if( zIn[0] == '\n' ){` |
|    12218 | 1145 | `				nLine++;` |
|     6108 | 1146 | `			}` |
|    13410 | 1147 | `			zIn++;` |
|        2 | 1148 | `		}` |
|        - | 1149 | `		/* Delimit the PHP chunk */` |
|    11520 | 1150 | `		sToken.nLine = nLine;` |
|    11520 | 1151 | `		zCur = zIn;` |
|  1062866 | 1152 | `		while( (sxu32)(zEnd - zIn) >= sCtag.nByte ){` |
|        - | 1153 | `			const char *zPtr;` |
|  1057804 | 1154 | `			if( SyMemcmp(zIn,sCtag.zString,sCtag.nByte) == 0 && iNest < 1 ){` |
|     6458 | 1155 | `				break;` |
|        - | 1156 | `			}` |
|   527663 | 1157 | `			for(;;){` |
|  1055328 | 1158 | `				if( zIn[0] != '/' \|\| (zIn[1] != '*' && zIn[1] != '/') /* && sCtag.nByte >= 2 */ ){` |
|   525675 | 1159 | `					break;` |
|        - | 1160 | `				}` |
|     3982 | 1161 | `				zIn += 2;` |
|     3982 | 1162 | `				if( zIn[-1] == '/' ){` |
|        - | 1163 | `					/* Inline comment */` |
|   138632 | 1164 | `					while( zIn < zEnd && zIn[0] != '\n' ){` |
|   134734 | 1165 | `						zIn++;` |
|        2 | 1166 | `					}` |
|     3900 | 1167 | `					if( zIn >= zEnd ){` |
|      ! 0 | 1168 | `						zIn--;` |
|      ! 0 | 1169 | `					}` |
|     1951 | 1170 | `				}else{` |
|        - | 1171 | `					/* Block comment */` |
|     4500 | 1172 | `					while( (sxu32)(zEnd-zIn) >= sizeof("*/") - 1 ){` |
|     4500 | 1173 | `						if( zIn[0] == '*' && zIn[1] == '/' ){` |
|       84 | 1174 | `							zIn += 2;` |
|       84 | 1175 | `							break;` |
|        - | 1176 | `						}` |
|     4418 | 1177 | `						if( zIn[0] == '\n' ){` |
|       28 | 1178 | `							nLine++;` |
|       13 | 1179 | `						}` |
|     4418 | 1180 | `						zIn++;` |
|        2 | 1181 | `					}` |
|        - | 1182 | `				}` |
|        2 | 1183 | `			}` |
|  1051348 | 1184 | `			if( zIn[0] == '\n' ){` |
|    37266 | 1185 | `				nLine++;` |
|    37266 | 1186 | `				if( iNest > 0 ){` |
|      282 | 1187 | `					zIn++;` |
|      666 | 1188 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|      386 | 1189 | `						zIn++;` |
|        2 | 1190 | `					}` |
|      282 | 1191 | `					zPtr = zIn;` |
|     1440 | 1192 | `					while( zIn < zEnd ){` |
|     1440 | 1193 | `						if( (unsigned char)zIn[0] >= 0xc0 ){` |
|        - | 1194 | `							/* UTF-8 stream */` |
|       19 | 1195 | `							zIn++;` |
|       37 | 1196 | `							SX_JMP_UTF8(zIn,zEnd);` |
|     1430 | 1197 | `						}else if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|      142 | 1198 | `							break;` |
|      ! 0 | 1199 | `						}else{` |
|     1142 | 1200 | `							zIn++;` |
|        - | 1201 | `						}` |
|        2 | 1202 | `					}` |
|      282 | 1203 | `					if( (sxu32)(zIn - zPtr) == sDoc.nByte && SyMemcmp(sDoc.zString,zPtr,sDoc.nByte) == 0 ){` |
|      110 | 1204 | `						iNest = 0;` |
|       54 | 1205 | `					}` |
|      282 | 1206 | `					continue;` |
|        2 | 1207 | `				}` |
|  1032576 | 1208 | `			}else if ( (sxu32)(zEnd - zIn) >= sizeof("<<<") && zIn[0] == '<' && zIn[1] == '<' && zIn[2] == '<' && iNest < 1){` |
|      112 | 1209 | `				zIn += sizeof("<<<")-1;` |
|      124 | 1210 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|       13 | 1211 | `					zIn++;` |
|        1 | 1212 | `				}` |
|      112 | 1213 | `				if( zIn[0] == '"' \|\| zIn[0] == '\'' ){` |
|       44 | 1214 | `					zIn++;` |
|       21 | 1215 | `				}` |
|      112 | 1216 | `				zPtr = zIn;` |
|      530 | 1217 | `				while( zIn < zEnd ){` |
|      530 | 1218 | `					if( (unsigned char)zIn[0] >= 0xc0 ){` |
|        - | 1219 | `						/* UTF-8 stream */` |
|       19 | 1220 | `						zIn++;` |
|       37 | 1221 | `						SX_JMP_UTF8(zIn,zEnd);` |
|      520 | 1222 | `					}else if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       57 | 1223 | `						break;` |
|      ! 0 | 1224 | `					}else{` |
|      402 | 1225 | `						zIn++;` |
|        - | 1226 | `					}` |
|        2 | 1227 | `				}` |
|      112 | 1228 | `				SyStringInitFromBuf(&sDoc,zPtr,zIn-zPtr);` |
|      112 | 1229 | `				SyStringFullTrim(&sDoc);` |
|      112 | 1230 | `				if( sDoc.nByte > 0 ){` |
|      112 | 1231 | `					iNest++;` |
|       55 | 1232 | `				}` |
|      112 | 1233 | `				continue;` |
|        - | 1234 | `			}` |
|  1050958 | 1235 | `			zIn++;` |
|        - | 1236 |  |
|  1050958 | 1237 | `			if ( zIn >= zEnd )` |
|      ! 0 | 1238 | `				break;` |
|        2 | 1239 | `		}` |
|    11520 | 1240 | `		if( (sxu32)(zEnd - zIn) < sCtag.nByte ){` |
|     5064 | 1241 | `			zIn = zEnd;` |
|     2531 | 1242 | `		}` |
|    11520 | 1243 | `		if( zCur < zIn ){` |
|        - | 1244 | `			/* Save the PHP chunk for later processing */` |
|     9264 | 1245 | `			sToken.nType = PH7_TOKEN_PHP;` |
|     9264 | 1246 | `			SyStringInitFromBuf(&sToken.sData,zCur,zIn-zCur);` |
|    18460 | 1247 | `			SyStringRightTrim(&sToken.sData); /* Trim trailing white spaces */` |
|     9264 | 1248 | `			rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|     9264 | 1249 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1250 | `				return rc;` |
|        - | 1251 | `			}` |
|     4631 | 1252 | `		}` |
|    11520 | 1253 | `		if( zIn < zEnd ){` |
|        - | 1254 | `			/* Jump the trailing closing tag */` |
|     6458 | 1255 | `			zIn += sCtag.nByte;` |
|     3228 | 1256 | `		}` |
|        2 | 1257 | `	} /* For(;;) */` |
|        - | 1258 |  |
|    11516 | 1259 | ` 	return SXRET_OK;` |
|     5759 | 1260 |  |
|        - | 1261 |  |
