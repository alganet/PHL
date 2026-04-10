# src/ph7/lex.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 712/747 lines (95.31%)

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
|  6989992 |   19 | `static sxi32 TokenizePHP(SyStream *pStream,SyToken *pToken,void *pUserData,void *pCtxData)` |
|        2 |   20 |  |
|        - |   21 | `	SyString *pStr;` |
|        - |   22 | `	sxi32 rc;` |
|        - |   23 | `	/* Ignore leading white spaces */` |
| 10530208 |   24 | `	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){` |
|        - |   25 | `		/* Advance the stream cursor */` |
|  3540216 |   26 | `		if( pStream->zText[0] == '\n' ){` |
|        - |   27 | `			/* Update line counter */` |
|    30158 |   28 | `			pStream->nLine++;` |
|    15078 |   29 | `		}` |
|  3540216 |   30 | `		pStream->zText++;` |
|        2 |   31 | `	}` |
|  6989994 |   32 | `	if( pStream->zText >= pStream->zEnd ){` |
|        - |   33 | `		/* End of input reached */` |
|      ! 0 |   34 | `		return SXERR_EOF;` |
|        - |   35 | `	}` |
|        - |   36 | `	/* Record token starting position and line */` |
|  6989994 |   37 | `	pToken->nLine = pStream->nLine;` |
|  6989994 |   38 | `	pToken->pUserData = 0;` |
|  6989994 |   39 | `	pStr = &pToken->sData;` |
|  6989994 |   40 | `	SyStringInitFromBuf(pStr,pStream->zText,0);` |
|  8252586 |   41 | `	if( pStream->zText[0] >= 0xc0 \|\| SyisAlpha(pStream->zText[0]) \|\| pStream->zText[0] == '_' ){` |
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
|  2525186 |   52 | `		if( pStream->zText[0] < 0xc0 ){` |
|  2525170 |   53 | `			pStream->zText++;` |
|  1262584 |   54 | `		}` |
|  2479382 |   55 | `		for(;;){` |
|  4958766 |   56 | `			zIn = pStream->zText;` |
|  4958766 |   57 | `			if( zIn[0] >= 0xc0 ){` |
|       49 |   58 | `				zIn++;` |
|        - |   59 | `				/* UTF-8 stream */` |
|      109 |   60 | `				while( zIn < pStream->zEnd && ((zIn[0] & 0xc0) == 0x80) ){` |
|       61 |   61 | `					zIn++;` |
|        1 |   62 | `				}` |
|       24 |   63 | `			}` |
|        - |   64 | `			/* Skip alphanumeric stream */` |
| 20350234 |   65 | `			while( zIn < pStream->zEnd && zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_') ){` |
| 12912088 |   66 | `				zIn++;` |
|        2 |   67 | `			}` |
|  4958766 |   68 | `			if( zIn == pStream->zText ){` |
|        - |   69 | `				/* Not an UTF-8 or alphanumeric stream */` |
|  2525186 |   70 | `				break;` |
|        - |   71 | `			}` |
|        - |   72 | `			/* Synchronize pointers */` |
|  2433582 |   73 | `			pStream->zText = zIn;` |
|        2 |   74 | `		}` |
|        - |   75 | `		/* Record token length */` |
|  2525186 |   76 | `		pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  2525186 |   77 | `		nKeyword = KeywordCode(pStr->zString,(int)pStr->nByte);` |
|        - |   78 | `		/* PHP 7.4: 'fn' is a keyword reserved for arrow functions.` |
|        - |   79 | `		 * The auto-generated perfect hash above doesn't know about it,` |
|        - |   80 | `		 * so intercept the 'fn' identifier here.` |
|        - |   81 | `		 */` |
|  2525666 |   82 | `		if( nKeyword == PH7_TK_ID && pStr->nByte == 2` |
|   834689 |   83 | `			&& pStr->zString[0] == 'f' && pStr->zString[1] == 'n' ){` |
|      358 |   84 | `			nKeyword = PH7_TKWRD_FN;` |
|      178 |   85 | `		}` |
|  2525186 |   86 | `		if( nKeyword != PH7_TK_ID ){` |
|   863194 |   87 | `			if( nKeyword &` |
|        - |   88 | `				(PH7_TKWRD_NEW\|PH7_TKWRD_CLONE\|PH7_TKWRD_AND\|PH7_TKWRD_XOR\|PH7_TKWRD_OR\|PH7_TKWRD_INSTANCEOF\|PH7_TKWRD_SEQ\|PH7_TKWRD_SNE) ){` |
|        - |   89 | `					/* Alpha stream operators [i.e: new,clone,and,instanceof,eq,ne,or,xor],save the operator instance for later processing */` |
|    14142 |   90 | `					pToken->pUserData = (void *)PH7_ExprExtractOperator(pStr,0);` |
|        - |   91 | `					/* Mark as an operator */` |
|    14142 |   92 | `					pToken->nType = PH7_TK_ID\|PH7_TK_OP;` |
|     7072 |   93 | `			}else{` |
|        - |   94 | `				/* We are dealing with a keyword [i.e: while,foreach,class...],save the keyword ID */` |
|   849054 |   95 | `				pToken->nType = PH7_TK_KEYWORD;` |
|   849054 |   96 | `				pToken->pUserData = SX_INT_TO_PTR(nKeyword);` |
|        - |   97 | `			}` |
|   431598 |   98 | `		}else{` |
|        - |   99 | `			/* A simple identifier */` |
|  1661994 |  100 | `			pToken->nType = PH7_TK_ID;` |
|        - |  101 | `		}` |
|  1262594 |  102 | `	}else{` |
|        - |  103 | `		sxi32 c;` |
|        - |  104 | `		/* Non-alpha stream */` |
|  4499146 |  105 | `		if( pStream->zText[0] == '#' \|\|` |
|  4464808 |  106 | `			( pStream->zText[0] == '/' &&  &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '/') ){` |
|     3824 |  107 | `				pStream->zText++;` |
|        - |  108 | `				/* Inline comments */` |
|   139200 |  109 | `				while( pStream->zText < pStream->zEnd && pStream->zText[0] != '\n' ){` |
|   135378 |  110 | `					pStream->zText++;` |
|        2 |  111 | `				}` |
|        - |  112 | `				/* Tell the upper-layer to ignore this token */` |
|     3824 |  113 | `				return SXERR_CONTINUE;` |
|  4460988 |  114 | `		}else if( pStream->zText[0] == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '*' ){` |
|    64788 |  115 | `			pStream->zText += 2;` |
|        - |  116 | `			/* Block comment */` |
|  1837780 |  117 | `			while( pStream->zText < pStream->zEnd ){` |
|  1837780 |  118 | `				if( pStream->zText[0] == '*' ){` |
|    64814 |  119 | `					if( &pStream->zText[1] >= pStream->zEnd \|\| pStream->zText[1] == '/'  ){` |
|    32395 |  120 | `						break;` |
|        - |  121 | `					}` |
|       13 |  122 | `				}` |
|  1772994 |  123 | `				if( pStream->zText[0] == '\n' ){` |
|       28 |  124 | `					pStream->nLine++;` |
|       13 |  125 | `				}` |
|  1772994 |  126 | `				pStream->zText++;` |
|        2 |  127 | `			}` |
|    64788 |  128 | `			pStream->zText += 2;` |
|        - |  129 | `			/* Tell the upper-layer to ignore this token */` |
|    64788 |  130 | `			return SXERR_CONTINUE;` |
|  4396202 |  131 | `		}else if( SyisDigit(pStream->zText[0]) ){` |
|    89632 |  132 | `			pStream->zText++;` |
|        - |  133 | `			/* PHP 7.4: handle underscore separator immediately following the first digit.` |
|        - |  134 | `			 * Check pStream->zText < pStream->zEnd BEFORE forming pStream->zText + 1 so` |
|        - |  135 | `			 * we never compute a pointer past one-past-end. */` |
|    89710 |  136 | `			if( pStream->zText < pStream->zEnd` |
|    89630 |  137 | `				&& pStream->zText[0] == '_'` |
|    44895 |  138 | `				&& pStream->zText + 1 < pStream->zEnd` |
|      160 |  139 | `				&& pStream->zText[1] < 0xc0` |
|      162 |  140 | `				&& SyisDigit(pStream->zText[1]) ){` |
|      151 |  141 | `				pStream->zText++; /* swallow underscore between two digits */` |
|       75 |  142 | `			}` |
|        - |  143 | `			/* Decimal digit stream (PHP 7.4: underscore separator allowed between two digits) */` |
|    99066 |  144 | `			while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|     9436 |  145 | `				pStream->zText++;` |
|     9520 |  146 | `				if( pStream->zText < pStream->zEnd` |
|     9434 |  147 | `					&& pStream->zText[0] == '_'` |
|     4803 |  148 | `					&& pStream->zText + 1 < pStream->zEnd` |
|      172 |  149 | `					&& pStream->zText[1] < 0xc0` |
|      174 |  150 | `					&& SyisDigit(pStream->zText[1]) ){` |
|      173 |  151 | `					pStream->zText++; /* swallow underscore between two digits */` |
|       86 |  152 | `				}` |
|        2 |  153 | `			}` |
|        - |  154 | `			/* Mark the token as integer until we encounter a real number */` |
|    89632 |  155 | `			pToken->nType = PH7_TK_INTEGER;` |
|    89632 |  156 | `			if( pStream->zText < pStream->zEnd ){` |
|    89632 |  157 | `				c = pStream->zText[0];` |
|    89632 |  158 | `				if( c == '.' ){` |
|        - |  159 | `					/* Real number (PHP 7.4: underscore separator allowed between two digits) */` |
|      448 |  160 | `					pStream->zText++;` |
|     1758 |  161 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|     1312 |  162 | `						pStream->zText++;` |
|     1316 |  163 | `						if( pStream->zText < pStream->zEnd` |
|     1310 |  164 | `							&& pStream->zText[0] == '_'` |
|      661 |  165 | `							&& pStream->zText + 1 < pStream->zEnd` |
|       12 |  166 | `							&& pStream->zText[1] < 0xc0` |
|       14 |  167 | `							&& SyisDigit(pStream->zText[1]) ){` |
|       13 |  168 | `							pStream->zText++;` |
|        6 |  169 | `						}` |
|        2 |  170 | `					}` |
|      448 |  171 | `					if( pStream->zText < pStream->zEnd ){` |
|      448 |  172 | `						c = pStream->zText[0];` |
|      448 |  173 | `						if( c=='e' \|\| c=='E' ){` |
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
|      223 |  193 | `					}` |
|      448 |  194 | `					pToken->nType = PH7_TK_REAL;` |
|    89409 |  195 | `				}else if( c=='e' \|\| c=='E' ){` |
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
|    89172 |  217 | `				}else if( c == 'x' \|\| c == 'X' ){` |
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
|    89122 |  230 | `				}else if(c  == 'b' \|\| c == 'B' ){` |
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
|    44815 |  243 | `			}` |
|        - |  244 | `			/* PHP 7.4: absorb a trailing malformed underscore run into the` |
|        - |  245 | `			 * numeric token so the compile phase can emit a PHP-compatible` |
|        - |  246 | `			 * "syntax error, unexpected identifier" parse error. Valid` |
|        - |  247 | `			 * separators were already consumed by the per-loop peek logic` |
|        - |  248 | `			 * above, so an underscore here is always misplaced. */` |
|    89632 |  249 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '_' ){` |
|       15 |  250 | `				pStream->zText++;` |
|       44 |  251 | `				while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0` |
|       46 |  252 | `					&& (SyisAlphaNum(pStream->zText[0]) \|\| pStream->zText[0] == '_') ){` |
|       17 |  253 | `					pStream->zText++;` |
|        1 |  254 | `				}` |
|        7 |  255 | `			}` |
|        - |  256 | `			/* Record token length */` |
|    89632 |  257 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|    89632 |  258 | `			return SXRET_OK;` |
|        - |  259 | `		}` |
|  4306572 |  260 | `		c = pStream->zText[0];` |
|  4306572 |  261 | `		pStream->zText++; /* Advance the stream cursor */` |
|        - |  262 | `		/* Assume we are dealing with an operator*/` |
|  4306572 |  263 | `		pToken->nType = PH7_TK_OP;` |
|  4306572 |  264 | `		switch(c){` |
|   903292 |  265 | `		case '$': pToken->nType = PH7_TK_DOLLAR; break;` |
|   342172 |  266 | `		case '{': pToken->nType = PH7_TK_OCB;    break;` |
|   342158 |  267 | `		case '}': pToken->nType = PH7_TK_CCB;    break;` |
|   681180 |  268 | `		case '(': pToken->nType = PH7_TK_LPAREN; break;` |
|    69576 |  269 | `		case '[': pToken->nType \|= PH7_TK_OSB;   break; /* Bitwise operation here,since the square bracket token '['` |
|        - |  270 | `														 * is a potential operator [i.e: subscripting] */` |
|    69582 |  271 | `		case ']': pToken->nType = PH7_TK_CSB;    break;` |
|   340582 |  272 | `		case ')': {` |
|   681166 |  273 | `			SySet *pTokSet = pStream->pSet;` |
|        - |  274 | `			/* Assemble type cast operators [i.e: (int),(float),(bool)...] */` |
|   681166 |  275 | `			if( pTokSet->nUsed >= 2 ){` |
|        - |  276 | `				SyToken *pTmp;` |
|        - |  277 | `				/* Peek the last recongnized token */` |
|   681164 |  278 | `				pTmp = (SyToken *)SySetPeek(pTokSet);` |
|   681164 |  279 | `				if( pTmp->nType & PH7_TK_KEYWORD ){` |
|    14020 |  280 | `					sxi32 nID = SX_PTR_TO_INT(pTmp->pUserData);` |
|    14020 |  281 | `					if( (sxu32)nID & (PH7_TKWRD_ARRAY\|PH7_TKWRD_INT\|PH7_TKWRD_FLOAT\|PH7_TKWRD_STRING\|PH7_TKWRD_OBJECT\|PH7_TKWRD_BOOL\|PH7_TKWRD_UNSET) ){` |
|    13782 |  282 | `						pTmp = (SyToken *)SySetAt(pTokSet,pTokSet->nUsed - 2);` |
|    13782 |  283 | `						if( pTmp->nType & PH7_TK_LPAREN ){` |
|        - |  284 | `							/* Merge the three tokens '(' 'TYPE' ')' into a single one */` |
|    13698 |  285 | `							const char * zTypeCast = "(int)";` |
|    13698 |  286 | `							if( nID & PH7_TKWRD_FLOAT ){` |
|     2704 |  287 | `								zTypeCast = "(float)";` |
|    12347 |  288 | `							}else if( nID & PH7_TKWRD_BOOL ){` |
|     2706 |  289 | `								zTypeCast = "(bool)";` |
|     9644 |  290 | `							}else if( nID & PH7_TKWRD_STRING ){` |
|     5402 |  291 | `								zTypeCast = "(string)";` |
|     5592 |  292 | `							}else if( nID & PH7_TKWRD_ARRAY ){` |
|       21 |  293 | `								zTypeCast = "(array)";` |
|     2882 |  294 | `							}else if( nID & PH7_TKWRD_OBJECT ){` |
|       17 |  295 | `								zTypeCast = "(object)";` |
|     2864 |  296 | `							}else if( nID & PH7_TKWRD_UNSET ){` |
|        7 |  297 | `								zTypeCast = "(unset)";` |
|        3 |  298 | `							}` |
|        - |  299 | `							/* Reflect the change */` |
|    13698 |  300 | `							pToken->nType = PH7_TK_OP;` |
|    13698 |  301 | `							SyStringInitFromBuf(&pToken->sData,zTypeCast,SyStrlen(zTypeCast));` |
|        - |  302 | `							/* Save the instance associated with the type cast operator */` |
|    13698 |  303 | `							pToken->pUserData = (void *)PH7_ExprExtractOperator(&pToken->sData,0);` |
|        - |  304 | `							/* Remove the two previous tokens */` |
|    13698 |  305 | `							pTokSet->nUsed -= 2;` |
|    13698 |  306 | `							return SXRET_OK;` |
|        - |  307 | `						}` |
|       42 |  308 | `					}` |
|      161 |  309 | `				}` |
|   333733 |  310 | `			}` |
|   667470 |  311 | `			pToken->nType = PH7_TK_RPAREN;` |
|   667470 |  312 | `			break;` |
|        - |  313 | `				  }` |
|    29072 |  314 | `		case '\'':{` |
|        - |  315 | `			/* Single quoted string */` |
|    58146 |  316 | `			pStr->zString++;` |
|   731918 |  317 | `			while( pStream->zText < pStream->zEnd ){` |
|   731918 |  318 | `				if( pStream->zText[0] == '\''  ){` |
|    58156 |  319 | `					if( pStream->zText[-1] != '\\' ){` |
|    58132 |  320 | `						break;` |
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
|   673774 |  333 | `				if( pStream->zText[0] == '\n' ){` |
|       67 |  334 | `					pStream->nLine++;` |
|       33 |  335 | `				}` |
|   673774 |  336 | `				pStream->zText++;` |
|        2 |  337 | `			}` |
|        - |  338 | `			/* Record token length and type */` |
|    58146 |  339 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|    58146 |  340 | `			pToken->nType = PH7_TK_SSTR;` |
|        - |  341 | `			/* Jump the trailing single quote */` |
|    58146 |  342 | `			pStream->zText++;` |
|    58146 |  343 | `			return SXRET_OK;` |
|        - |  344 | `				  }` |
|     7950 |  345 | `		case '"':{` |
|        - |  346 | `			sxi32 iNest;` |
|        - |  347 | `			/* Double quoted string */` |
|    15902 |  348 | `			pStr->zString++;` |
|   156894 |  349 | `			while( pStream->zText < pStream->zEnd ){` |
|   156894 |  350 | `				if( pStream->zText[0] == '{' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '$'){` |
|       83 |  351 | `					iNest = 1;` |
|       83 |  352 | `					pStream->zText++;` |
|        - |  353 | `					/* TICKET 1433-40: Hnadle braces'{}' in double quoted string where everything is allowed */` |
|      953 |  354 | `					while(pStream->zText < pStream->zEnd ){` |
|      953 |  355 | `						if( pStream->zText[0] == '{' ){` |
|        9 |  356 | `							iNest++;` |
|      949 |  357 | `						}else if (pStream->zText[0] == '}' ){` |
|       91 |  358 | `							iNest--;` |
|       91 |  359 | `							if( iNest <= 0 ){` |
|       83 |  360 | `								pStream->zText++;` |
|       83 |  361 | `								break;` |
|        1 |  362 | `							}` |
|      859 |  363 | `						}else if( pStream->zText[0] == '\n' ){` |
|      ! 0 |  364 | `							pStream->nLine++;` |
|      ! 0 |  365 | `						}` |
|      871 |  366 | `						pStream->zText++;` |
|        1 |  367 | `					}` |
|       83 |  368 | `					if( pStream->zText >= pStream->zEnd ){` |
|      ! 0 |  369 | `						break;` |
|        - |  370 | `					}` |
|       41 |  371 | `				}` |
|   156894 |  372 | `				if( pStream->zText[0] == '"' ){` |
|    16002 |  373 | `					if( pStream->zText[-1] != '\\' ){` |
|    15898 |  374 | `						break;` |
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
|   140994 |  387 | `				if( pStream->zText[0] == '\n' ){` |
|        7 |  388 | `					pStream->nLine++;` |
|        3 |  389 | `				}` |
|   140994 |  390 | `				pStream->zText++;` |
|        2 |  391 | `			}` |
|        - |  392 | `			/* Record token length and type */` |
|    15902 |  393 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|    15902 |  394 | `			pToken->nType = PH7_TK_DSTR;` |
|        - |  395 | `			/* Jump the trailing quote */` |
|    15902 |  396 | `			pStream->zText++;` |
|    15902 |  397 | `			return SXRET_OK;` |
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
|      166 |  418 | `		case '\\': pToken->nType = PH7_TK_NSSEP;  break;` |
|     1206 |  419 | `		case ':':` |
|     2414 |  420 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == ':' ){` |
|        - |  421 | `				/* Current operator: '::' */` |
|      210 |  422 | `				pStream->zText++;` |
|      106 |  423 | `			}else{` |
|     2206 |  424 | `				pToken->nType = PH7_TK_COLON; /* Single colon */` |
|        - |  425 | `			}` |
|     2414 |  426 | `			break;` |
|    73170 |  427 | `		case ',': pToken->nType \|= PH7_TK_COMMA;  break; /* Comma is also an operator */` |
|   489946 |  428 | `		case ';': pToken->nType = PH7_TK_SEMI;    break;` |
|        - |  429 | `			/* Handle combined operators [i.e: +=,===,!=== ...] */` |
|   141596 |  430 | `		case '=':` |
|   283194 |  431 | `			pToken->nType \|= PH7_TK_EQUAL;` |
|   283194 |  432 | `			if( pStream->zText < pStream->zEnd ){` |
|   283194 |  433 | `				if( pStream->zText[0] == '=' ){` |
|    17694 |  434 | `					pToken->nType &= ~PH7_TK_EQUAL;` |
|        - |  435 | `					/* Current operator: == */` |
|    17694 |  436 | `					pStream->zText++;` |
|    17694 |  437 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  438 | `						/* Current operator: === */` |
|     3902 |  439 | `						pStream->zText++;` |
|     1952 |  440 | `					}` |
|   274348 |  441 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  442 | `					/* Array operator: => */` |
|     4160 |  443 | `					pToken->nType = PH7_TK_ARRAY_OP;` |
|     4160 |  444 | `					pStream->zText++;` |
|     2081 |  445 | `				}else{` |
|        - |  446 | `					/* TICKET 1433-0010: Reference operator '=&' */` |
|   261344 |  447 | `					const unsigned char *zCur = pStream->zText;` |
|   261344 |  448 | `					sxu32 nLine = 0;` |
|   522664 |  449 | `					while( zCur < pStream->zEnd && zCur[0] < 0xc0 && SyisSpace(zCur[0]) ){` |
|   261322 |  450 | `						if( zCur[0] == '\n' ){` |
|        5 |  451 | `							nLine++;` |
|        2 |  452 | `						}` |
|   261322 |  453 | `						zCur++;` |
|        2 |  454 | `					}` |
|   261344 |  455 | `					if( zCur < pStream->zEnd && zCur[0] == '&' ){` |
|        - |  456 | `						/* Current operator: =& */` |
|       48 |  457 | `						pToken->nType &= ~PH7_TK_EQUAL;` |
|       48 |  458 | `						SyStringInitFromBuf(pStr,"=&",sizeof("=&")-1);` |
|        - |  459 | `						/* Update token stream */` |
|       48 |  460 | `						pStream->zText = &zCur[1];` |
|       48 |  461 | `						pStream->nLine += nLine;` |
|       23 |  462 | `					}` |
|        - |  463 | `				}` |
|   141596 |  464 | `			}` |
|   283194 |  465 | `			break;` |
|    19222 |  466 | `		case '!':` |
|    38446 |  467 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  468 | `				/* Current operator: != */` |
|    16360 |  469 | `				pStream->zText++;` |
|    16360 |  470 | `				if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  471 | `					/* Current operator: !== */` |
|    13632 |  472 | `					pStream->zText++;` |
|     6815 |  473 | `				}` |
|     8179 |  474 | `			}` |
|    38446 |  475 | `			break;` |
|    11049 |  476 | `		case '&':` |
|    22100 |  477 | `			pToken->nType \|= PH7_TK_AMPER;` |
|    22100 |  478 | `			if( pStream->zText < pStream->zEnd ){` |
|    22100 |  479 | `				if( pStream->zText[0] == '&' ){` |
|     8492 |  480 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|        - |  481 | `					/* Current operator: && */` |
|     8492 |  482 | `					pStream->zText++;` |
|    17855 |  483 | `				}else if( pStream->zText[0] == '=' ){` |
|        7 |  484 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|        - |  485 | `					/* Current operator: &= */` |
|        7 |  486 | `					pStream->zText++;` |
|        3 |  487 | `				}` |
|    11049 |  488 | `			}` |
|    22100 |  489 | `			break;` |
|     1436 |  490 | `		case '\|':` |
|     2874 |  491 | `			if( pStream->zText < pStream->zEnd ){` |
|     2874 |  492 | `				if( pStream->zText[0] == '\|' ){` |
|        - |  493 | `					/* Current operator: \|\| */` |
|     2830 |  494 | `					pStream->zText++;` |
|     1460 |  495 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  496 | `					/* Current operator: \|= */` |
|        7 |  497 | `					pStream->zText++;` |
|        3 |  498 | `				}` |
|     1436 |  499 | `			}` |
|     2874 |  500 | `			break;` |
|     7108 |  501 | `		case '+':` |
|    14218 |  502 | `			if( pStream->zText < pStream->zEnd ){` |
|    14216 |  503 | `				if( pStream->zText[0] == '+' ){` |
|        - |  504 | `					/* Current operator: ++ */` |
|    11036 |  505 | `					pStream->zText++;` |
|     8699 |  506 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  507 | `					/* Current operator: += */` |
|       42 |  508 | `					pStream->zText++;` |
|       20 |  509 | `				}` |
|     7107 |  510 | `			}` |
|    14218 |  511 | `			break;` |
|    51882 |  512 | `		case '-':` |
|   103766 |  513 | `			if( pStream->zText < pStream->zEnd ){` |
|   103766 |  514 | `				if( pStream->zText[0] == '-' ){` |
|        - |  515 | `					/* Current operator: -- */` |
|        5 |  516 | `					pStream->zText++;` |
|   103764 |  517 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  518 | `					/* Current operator: -= */` |
|        7 |  519 | `					pStream->zText++;` |
|   103759 |  520 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  521 | `					/* Current operator: -> */` |
|   103276 |  522 | `					pStream->zText++;` |
|    51637 |  523 | `				}` |
|    51882 |  524 | `			}` |
|   103766 |  525 | `			break;` |
|       91 |  526 | `		case '*':` |
|      184 |  527 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  528 | `				/* Current operator: *= */` |
|       17 |  529 | `				pStream->zText++;` |
|        8 |  530 | `			}` |
|      184 |  531 | `			break;` |
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
|    29320 |  550 | `		case '.':` |
|    58642 |  551 | `			if( pStream->zText + 1 < pStream->zEnd && pStream->zText[0] == '.' && pStream->zText[1] == '.' ){` |
|        - |  552 | `				/* Ellipsis: ... */` |
|       44 |  553 | `				pStream->zText += 2;` |
|       44 |  554 | `				pToken->nType = PH7_TK_ELLIPSIS;` |
|    58621 |  555 | `			}else if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  556 | `				/* Current operator: .= */` |
|     2766 |  557 | `				pStream->zText++;` |
|     1382 |  558 | `			}` |
|    58642 |  559 | `			break;` |
|    23177 |  560 | `		case '<':` |
|    46356 |  561 | `			if( pStream->zText < pStream->zEnd ){` |
|    46356 |  562 | `				if( pStream->zText[0] == '<' ){` |
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
|    46235 |  580 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  581 | `					/* Current operator: <> */` |
|        5 |  582 | `					pStream->zText++;` |
|    46222 |  583 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  584 | `					/* Current operator: <= or <=> */` |
|       92 |  585 | `					pStream->zText++;` |
|       92 |  586 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '>' ){` |
|        - |  587 | `						/* Current operator: <=> */` |
|       51 |  588 | `						pStream->zText++;` |
|       25 |  589 | `					}` |
|       45 |  590 | `				}` |
|    23122 |  591 | `			}` |
|    46246 |  592 | `			break;` |
|     2803 |  593 | `		case '>':` |
|     5608 |  594 | `			if( pStream->zText < pStream->zEnd ){` |
|     5608 |  595 | `				if( pStream->zText[0] == '>' ){` |
|        - |  596 | `					/* Current operator: >> */` |
|       21 |  597 | `					pStream->zText++;` |
|       21 |  598 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  599 | `						/* Current operator: >>= */` |
|       11 |  600 | `						pStream->zText++;` |
|        6 |  601 | `					}` |
|     5598 |  602 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  603 | `					/* Current operator: >= */` |
|       80 |  604 | `					pStream->zText++;` |
|       39 |  605 | `				}` |
|     2803 |  606 | `			}` |
|     5608 |  607 | `			break;` |
|     1004 |  608 | `		case '?':` |
|     2010 |  609 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '?' ){` |
|        - |  610 | `				/* Null coalescing operator: ?? */` |
|       84 |  611 | `				pStream->zText++;` |
|       84 |  612 | `				if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  613 | `					/* Null coalescing assignment operator (PHP 7.4) */` |
|       54 |  614 | `					pStream->zText++;` |
|       26 |  615 | `				}` |
|       41 |  616 | `			}` |
|     2008 |  617 | `			break;` |
|      105 |  618 | `		default:` |
|      210 |  619 | `			break;` |
|        - |  620 | `		}` |
|  4218718 |  621 | `		if( pStr->nByte <= 0 ){` |
|        - |  622 | `			/* Record token length */` |
|  4218672 |  623 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  2109335 |  624 | `		}` |
|  4218718 |  625 | `		if( pToken->nType & PH7_TK_OP ){` |
|        - |  626 | `			const ph7_expr_op *pOp;` |
|        - |  627 | `			/* Check if the extracted token is an operator */` |
|   716364 |  628 | `			pOp = PH7_ExprExtractOperator(pStr,(SyToken *)SySetPeek(pStream->pSet));` |
|   716364 |  629 | `			if( pOp == 0 ){` |
|        - |  630 | `				/* Not an operator */` |
|      ! 0 |  631 | `				pToken->nType &= ~PH7_TK_OP;` |
|      ! 0 |  632 | `				if( pToken->nType <= 0 ){` |
|      ! 0 |  633 | `					pToken->nType = PH7_TK_OTHER;` |
|      ! 0 |  634 | `				}` |
|      ! 0 |  635 | `			}else{` |
|        - |  636 | `				/* Save the instance associated with this operator for later processing */` |
|   716364 |  637 | `				pToken->pUserData = (void *)pOp;` |
|        - |  638 | `			}` |
|   358181 |  639 | `		}` |
|        - |  640 | `	}` |
|        - |  641 | `	/* Tell the upper-layer to save the extracted token for later processing */` |
|  6743902 |  642 | `	return SXRET_OK;` |
|  3494998 |  643 |  |
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
|  2525186 |  660 | `static sxu32 KeywordCode(const char *z, int n){` |
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
|  2525186 |  750 | `  if( n<2 ) return PH7_TK_ID;` |
|  2433560 |  751 | `  h = (((int)z[0]*4) ^ ((int)z[n-1]*3) ^ n) % 151;` |
|  3725366 |  752 | `  for(i=((int)aHash[h])-1; i>=0; i=((int)aNext[i])-1){` |
|  2154524 |  753 | `    if( (int)aLen[i]==n && SyMemcmp(&zText[aOffset[i]],z,n)==0 ){` |
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
|   862718 |  838 | `      return aCode[i];` |
|        - |  839 | `    }` |
|   645903 |  840 | `  }` |
|        - |  841 | `  /* Linear fallback for keywords not in the auto-generated hash table */` |
|  1570844 |  842 | `  if( n==5 && SyMemcmp(z,"trait",5)==0 ) return PH7_TKWRD_TRAIT;` |
|  1570790 |  843 | `  if( n==9 && SyMemcmp(z,"insteadof",9)==0 ) return PH7_TKWRD_INSTEADOF;` |
|  1570786 |  844 | `  if( n==7 && SyMemcmp(z,"finally",7)==0 ) return PH7_TKWRD_FINALLY;` |
|  1570756 |  845 | `  if( n==5 && SyMemcmp(z,"yield",5)==0 ) return PH7_TKWRD_YIELD;` |
|  1570724 |  846 | `  return PH7_TK_ID;` |
|  1262594 |  847 |  |
|        - |  848 | `/* --- End of Automatically generated code --- */` |
|        - |  849 | `/*` |
|        - |  850 | ` * Extract a heredoc/nowdoc text from a raw PHP input.` |
|        - |  851 | ` * According to the PHP language reference manual:` |
|        - |  852 | ` *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier` |
|        - |  853 | ` *  is provided, then a newline. The string itself follows, and then the same identifier again` |
|        - |  854 | ` *  to close the quotation.` |
|        - |  855 | ` *  The closing identifier must begin in the first column of the line. Also, the identifier must` |
|        - |  856 | ` *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric` |
|        - |  857 | ` *  characters and underscores, and must start with a non-digit character or underscore.` |
|        - |  858 | ` *  Heredoc text behaves just like a double-quoted string, without the double quotes.` |
|        - |  859 | ` *  This means that quotes in a heredoc do not need to be escaped, but the escape codes listed` |
|        - |  860 | ` *  above can still be used. Variables are expanded, but the same care must be taken when expressing` |
|        - |  861 | ` *  complex variables inside a heredoc as with strings.` |
|        - |  862 | ` *  Nowdocs are to single-quoted strings what heredocs are to double-quoted strings.` |
|        - |  863 | ` *  A nowdoc is specified similarly to a heredoc, but no parsing is done inside a nowdoc.` |
|        - |  864 | ` *  The construct is ideal for embedding PHP code or other large blocks of text without the need` |
|        - |  865 | ` *  for escaping. It shares some features in common with the SGML <![CDATA[ ]]> construct, in that` |
|        - |  866 | ` *  it declares a block of text which is not for parsing.` |
|        - |  867 | ` *  A nowdoc is identified with the same <<< sequence used for heredocs, but the identifier which follows` |
|        - |  868 | ` *  is enclosed in single quotes, e.g. <<<'EOT'. All the rules for heredoc identifiers also apply to nowdoc` |
|        - |  869 | ` *  identifiers, especially those regarding the appearance of the closing identifier.` |
|        - |  870 | ` * Symisc Extension:` |
|        - |  871 | ` * The closing delimiter can now start with a digit or undersocre or it can be an UTF-8 stream.` |
|        - |  872 | ` * Example:` |
|        - |  873 | ` *  <<<123` |
|        - |  874 | ` *    HEREDOC Here` |
|        - |  875 | ` * 123` |
|        - |  876 | ` *  or` |
|        - |  877 | ` *  <<<___` |
|        - |  878 | ` *   HEREDOC Here` |
|        - |  879 | ` *  ___` |
|        - |  880 | ` */` |
|      110 |  881 | `static sxi32 LexExtractHeredoc(SyStream *pStream,SyToken *pToken)` |
|        2 |  882 |  |
|      112 |  883 | `	const unsigned char *zIn  = pStream->zText;` |
|      112 |  884 | `	const unsigned char *zEnd = pStream->zEnd;` |
|        - |  885 | `	const unsigned char *zPtr;` |
|      112 |  886 | `	sxu8 bNowDoc = FALSE;` |
|        - |  887 | `	SyString sDelim;` |
|        - |  888 | `	SyString sStr;` |
|        - |  889 | `	/* Jump leading white spaces */` |
|      124 |  890 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|       13 |  891 | `		zIn++;` |
|        1 |  892 | `	}` |
|      112 |  893 | `	if( zIn >= zEnd ){` |
|        - |  894 | `		/* A simple symbol,return immediately */` |
|      ! 0 |  895 | `		return SXERR_CONTINUE;` |
|        - |  896 | `	}` |
|      112 |  897 | `	if( zIn[0] == '\'' \|\| zIn[0] == '"' ){` |
|        - |  898 | `		/* Make sure we are dealing with a nowdoc */` |
|       44 |  899 | `		bNowDoc =  zIn[0] == '\'' ? TRUE : FALSE;` |
|       44 |  900 | `		zIn++;` |
|       21 |  901 | `	}` |
|      112 |  902 | `	if( zIn[0] < 0xc0 && !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|        - |  903 | `		/* Invalid delimiter,return immediately */` |
|      ! 0 |  904 | `		return SXERR_CONTINUE;` |
|        - |  905 | `	}` |
|        - |  906 | `	/* Isolate the identifier */` |
|      112 |  907 | `	sDelim.zString = (const char *)zIn;` |
|      118 |  908 | `	for(;;){` |
|      238 |  909 | `		zPtr = zIn;` |
|        - |  910 | `		/* Skip alphanumeric stream */` |
|      756 |  911 | `		while( zPtr < zEnd && zPtr[0] < 0xc0 && (SyisAlphaNum(zPtr[0]) \|\| zPtr[0] == '_') ){` |
|      402 |  912 | `			zPtr++;` |
|        2 |  913 | `		}` |
|      238 |  914 | `		if( zPtr < zEnd && zPtr[0] >= 0xc0 ){` |
|       19 |  915 | `			zPtr++;` |
|        - |  916 | `			/* UTF-8 stream */` |
|       37 |  917 | `			while( zPtr < zEnd && ((zPtr[0] & 0xc0) == 0x80) ){` |
|       19 |  918 | `				zPtr++;` |
|        1 |  919 | `			}` |
|        9 |  920 | `		}` |
|      238 |  921 | `		if( zPtr == zIn ){` |
|        - |  922 | `			/* Not an UTF-8 or alphanumeric stream */` |
|      112 |  923 | `			break;` |
|        - |  924 | `		}` |
|        - |  925 | `		/* Synchronize pointers */` |
|      128 |  926 | `		zIn = zPtr;` |
|        2 |  927 | `	}` |
|        - |  928 | `	/* Get the identifier length */` |
|      112 |  929 | `	sDelim.nByte = (sxu32)((const char *)zIn-sDelim.zString);` |
|      112 |  930 | `	if( zIn[0] == '"' \|\| (bNowDoc && zIn[0] == '\'') ){` |
|        - |  931 | `		/* Jump the trailing single quote */` |
|       44 |  932 | `		zIn++;` |
|       21 |  933 | `	}` |
|        - |  934 | `	/* Jump trailing white spaces */` |
|      112 |  935 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|      ! 0 |  936 | `		zIn++;` |
|      ! 0 |  937 | `	}` |
|      112 |  938 | `	if( sDelim.nByte <= 0 \|\| zIn >= zEnd \|\| zIn[0] != '\n' ){` |
|        - |  939 | `		/* Invalid syntax */` |
|      ! 0 |  940 | `		return SXERR_CONTINUE;` |
|        - |  941 | `	}` |
|      112 |  942 | `	pStream->nLine++; /* Increment line counter */` |
|      112 |  943 | `	zIn++;` |
|        - |  944 | `	/* Isolate the delimited string */` |
|      112 |  945 | `	sStr.zString = (const char *)zIn;` |
|        - |  946 | `	/* PHP 7.3 flexible heredoc/nowdoc: the closing marker may be preceded` |
|        - |  947 | `	 * by whitespace (spaces/tabs), and may be followed by any non-identifier` |
|        - |  948 | `	 * character. The indent count is recorded in pToken->pUserData and the` |
|        - |  949 | `	 * compile phase strips it from each body line. */` |
|        - |  950 | `	{` |
|      112 |  951 | `		const unsigned char *zMarkerLine = zIn; /* Start of marker's line (set on match) */` |
|      112 |  952 | `		sxu32 nIndent = 0;` |
|      225 |  953 | `		for(;;){` |
|      282 |  954 | `			const unsigned char *zLineStart = zIn;` |
|        - |  955 | `			/* Skip leading space/tab on this line */` |
|      806 |  956 | `			while( zIn < zEnd && (zIn[0] == ' ' \|\| zIn[0] == '\t') ){` |
|      386 |  957 | `				zIn++;` |
|        2 |  958 | `			}` |
|      280 |  959 | `			if( (sxu32)(zEnd - zIn) >= sDelim.nByte` |
|      281 |  960 | `				&& SyMemcmp((const void *)sDelim.zString,(const void *)zIn,sDelim.nByte) == 0 ){` |
|        - |  961 | `				int bIdentCont;` |
|      110 |  962 | `				zPtr = &zIn[sDelim.nByte];` |
|        - |  963 | `				/* Disambiguate: next byte must not continue an identifier.` |
|        - |  964 | `				 * A leading byte >= 0xc0 starts a multi-byte UTF-8 sequence,` |
|        - |  965 | `				 * which PHP identifiers may contain, so treat it as ident. */` |
|      110 |  966 | `				if( zPtr >= zEnd ){` |
|      ! 0 |  967 | `					bIdentCont = 0;` |
|      110 |  968 | `				}else if( zPtr[0] >= 0xc0 ){` |
|      ! 0 |  969 | `					bIdentCont = 1;` |
|      ! 0 |  970 | `				}else{` |
|      110 |  971 | `					bIdentCont = (SyisAlphaNum(zPtr[0]) \|\| zPtr[0] == '_');` |
|        - |  972 | `				}` |
|      110 |  973 | `				if( !bIdentCont ){` |
|        - |  974 | `					/* Closing marker found */` |
|      110 |  975 | `					nIndent = (sxu32)(zIn - zLineStart);` |
|      110 |  976 | `					zMarkerLine = zLineStart;` |
|      110 |  977 | `					pStream->zText = zPtr; /* Cursor right after identifier */` |
|      110 |  978 | `					break;` |
|        - |  979 | `				}` |
|      ! 0 |  980 | `			}` |
|        - |  981 | `			/* Not the closing marker on this line; walk to next newline */` |
|     2824 |  982 | `			while( zIn < zEnd && zIn[0] != '\n' ){` |
|     2652 |  983 | `				zIn++;` |
|        2 |  984 | `			}` |
|      174 |  985 | `			if( zIn >= zEnd ){` |
|        - |  986 | `				/* End of input without finding the closing marker */` |
|        3 |  987 | `				pStream->zText = pStream->zEnd;` |
|        3 |  988 | `				zMarkerLine = zIn;` |
|        3 |  989 | `				break;` |
|        - |  990 | `			}` |
|      172 |  991 | `			pStream->nLine++;` |
|      172 |  992 | `			zIn++;` |
|        2 |  993 | `		}` |
|        - |  994 | `		/* Body runs from sStr.zString up to just before the marker line */` |
|      112 |  995 | `		sStr.nByte = (sxu32)((const char *)zMarkerLine - sStr.zString);` |
|      112 |  996 | `		pToken->nType = bNowDoc ? PH7_TK_NOWDOC : PH7_TK_HEREDOC;` |
|      112 |  997 | `		SyStringDupPtr(&pToken->sData,&sStr);` |
|        - |  998 | `		/* Strip exactly one line terminator that precedes the marker's line. */` |
|      110 |  999 | `		if( pToken->sData.nByte > 0` |
|      108 | 1000 | `			&& pToken->sData.zString[pToken->sData.nByte - 1] == '\n' ){` |
|      102 | 1001 | `			pToken->sData.nByte--;` |
|      100 | 1002 | `			if( pToken->sData.nByte > 0` |
|      102 | 1003 | `				&& pToken->sData.zString[pToken->sData.nByte - 1] == '\r' ){` |
|      ! 0 | 1004 | `				pToken->sData.nByte--;` |
|      ! 0 | 1005 | `			}` |
|       50 | 1006 | `		}` |
|      112 | 1007 | `		pToken->pUserData = SX_INT_TO_PTR(nIndent);` |
|        - | 1008 | `	}` |
|        - | 1009 | `	/* All done */` |
|      112 | 1010 | `	return SXRET_OK;` |
|       57 | 1011 |  |
|        - | 1012 | `/*` |
|        - | 1013 | ` * Tokenize a raw PHP input.` |
|        - | 1014 | ` * This is the public tokenizer called by most code generator routines.` |
|        - | 1015 | ` */` |
|    13510 | 1016 | `PH7_PRIVATE sxi32 PH7_TokenizePHP(const char *zInput,sxu32 nLen,sxu32 nLineStart,SySet *pOut)` |
|        2 | 1017 |  |
|        - | 1018 | `	SyLex sLexer;` |
|        - | 1019 | `	sxi32 rc;` |
|        - | 1020 | `	/* Initialize the lexer */` |
|    13512 | 1021 | `	rc = SyLexInit(&sLexer,&(*pOut),TokenizePHP,0);` |
|    13512 | 1022 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1023 | `		return rc;` |
|        - | 1024 | `	}` |
|    13512 | 1025 | `	sLexer.sStream.nLine = nLineStart;` |
|        - | 1026 | `	/* Tokenize input */` |
|    13512 | 1027 | `	rc = SyLexTokenizeInput(&sLexer,zInput,nLen,0,0,0);` |
|        - | 1028 | `	/* Release the lexer */` |
|    13512 | 1029 | `	SyLexRelease(&sLexer);` |
|        - | 1030 | `	/* Tokenization result */` |
|    13512 | 1031 | `	return rc;` |
|     6757 | 1032 |  |
|        - | 1033 | `/*` |
|        - | 1034 | ` * High level public tokenizer.` |
|        - | 1035 | ` *  Tokenize the input into PHP tokens and raw tokens [i.e: HTML,XML,Raw text...].` |
|        - | 1036 | ` * According to the PHP language reference manual` |
|        - | 1037 | ` *   When PHP parses a file, it looks for opening and closing tags, which tell PHP` |
|        - | 1038 | ` *   to start and stop interpreting the code between them. Parsing in this manner allows` |
|        - | 1039 | ` *   PHP to be embedded in all sorts of different documents, as everything outside of a pair` |
|        - | 1040 | ` *   of opening and closing tags is ignored by the PHP parser. Most of the time you will see` |
|        - | 1041 | ` *   PHP embedded in HTML documents, as in this example.` |
|        - | 1042 | ` *   <?php echo 'While this is going to be parsed.'; ?>` |
|        - | 1043 | ` *   <p>This will also be ignored.</p>` |
|        - | 1044 | ` *   You can also use more advanced structures:` |
|        - | 1045 | ` *   Example #1 Advanced escaping` |
|        - | 1046 | ` * <?php` |
|        - | 1047 | ` * if ($expression) {` |
|        - | 1048 | ` *   ?>` |
|        - | 1049 | ` *   <strong>This is true.</strong>` |
|        - | 1050 | ` *   <?php` |
|        - | 1051 | ` * } else {` |
|        - | 1052 | ` *   ?>` |
|        - | 1053 | ` *   <strong>This is false.</strong>` |
|        - | 1054 | ` *   <?php` |
|        - | 1055 | ` * }` |
|        - | 1056 | ` * ?>` |
|        - | 1057 | ` * This works as expected, because when PHP hits the ?> closing tags, it simply starts outputting` |
|        - | 1058 | ` * whatever it finds (except for an immediately following newline - see instruction separation ) until it hits` |
|        - | 1059 | ` * another opening tag. The example given here is contrived, of course, but for outputting large blocks of text` |
|        - | 1060 | ` * dropping out of PHP parsing mode is generally more efficient than sending all of the text through echo() or print().` |
|        - | 1061 | ` * There are four different pairs of opening and closing tags which can be used in PHP. Three of those, <?php ?>` |
|        - | 1062 | ` * <script language="php"> </script>  and <? ?> are always available. The other two are short tags and ASP style` |
|        - | 1063 | ` * tags, and can be turned on and off from the php.ini configuration file. As such, while some people find short tags` |
|        - | 1064 | ` * and ASP style tags convenient, they are less portable, and generally not recommended.` |
|        - | 1065 | ` * Note:` |
|        - | 1066 | ` * Also note that if you are embedding PHP within XML or XHTML you will need to use the <?php ?> tags to remain` |
|        - | 1067 | ` * compliant with standards.` |
|        - | 1068 | ` * Example #2 PHP Opening and Closing Tags` |
|        - | 1069 | ` * 1.  <?php echo 'if you want to serve XHTML or XML documents, do it like this'; ?>` |
|        - | 1070 | ` * 2.  <script language="php">` |
|        - | 1071 | ` *       echo 'some editors (like FrontPage) don\'t` |
|        - | 1072 | ` *             like processing instructions';` |
|        - | 1073 | ` *   </script>` |
|        - | 1074 | ` *` |
|        - | 1075 | ` * 3.  <? echo 'this is the simplest, an SGML processing instruction'; ?>` |
|        - | 1076 | ` *   <?= expression ?> This is a shortcut for "<? echo expression ?>"` |
|        - | 1077 | ` */` |
|    11120 | 1078 | `PH7_PRIVATE sxi32 PH7_TokenizeRawText(const char *zInput,sxu32 nLen,SySet *pOut)` |
|        2 | 1079 |  |
|    11122 | 1080 | `	const char *zEnd = &zInput[nLen];` |
|    11122 | 1081 | `	const char *zIn  = zInput;` |
|        - | 1082 | `	const char *zCur,*zCurEnd;` |
|    11122 | 1083 | `	SyString sCtag = { 0, 0 };     /* Closing tag */` |
|        - | 1084 | `	SyToken sToken;` |
|        - | 1085 | `	SyString sDoc;` |
|        - | 1086 | `	sxu32 nLine;` |
|        - | 1087 | `	sxi32 iNest;` |
|        - | 1088 | `	sxi32 rc;` |
|        - | 1089 | `	/* Tokenize the input into PHP tokens and raw tokens */` |
|    11122 | 1090 | `	nLine = 1;` |
|    11122 | 1091 | `	zCur = zCurEnd   = 0; /* Prevent compiler warning */` |
|    11122 | 1092 | `	sToken.pUserData = 0;` |
|    11122 | 1093 | `	iNest = 0;` |
|    11122 | 1094 | `	sDoc.nByte = 0;` |
|    11122 | 1095 | `	sDoc.zString = ""; /* cc warning */` |
|    11122 | 1096 | `	for(;;){` |
|    22246 | 1097 | `		if( zIn >= zEnd ){` |
|        - | 1098 | `			/* End of input reached */` |
|    11118 | 1099 | `			break;` |
|        - | 1100 | `		}` |
|    11130 | 1101 | `		sToken.nLine = nLine;` |
|    11130 | 1102 | `		zCur = zIn;` |
|    11130 | 1103 | `		zCurEnd = 0;` |
|    11138 | 1104 | `		while( zIn < zEnd ){` |
|    11134 | 1105 | `			 if( zIn[0] == '<' ){` |
|    11126 | 1106 | `				const char *zTmp = zIn; /* End of raw input marker */` |
|    11126 | 1107 | `				zIn++;` |
|    11126 | 1108 | `				if( zIn < zEnd ){` |
|    11126 | 1109 | `					if( zIn[0] == '?' ){` |
|    11126 | 1110 | `						zIn++;` |
|    11126 | 1111 | `						if( (sxu32)(zEnd - zIn) >= sizeof("php")-1 &&  SyStrnicmp(zIn,"php",sizeof("php")-1) == 0 ){` |
|        - | 1112 | `							/* opening tag: <?php */` |
|    11124 | 1113 | `							zIn += sizeof("php")-1;` |
|     5561 | 1114 | `						}` |
|        - | 1115 | `						/* Look for the closing tag '?>' */` |
|    11126 | 1116 | `						SyStringInitFromBuf(&sCtag,"?>",sizeof("?>")-1);` |
|    11126 | 1117 | `						zCurEnd = zTmp;` |
|    11126 | 1118 | `						break;` |
|        - | 1119 | `					}` |
|      ! 0 | 1120 | `				}` |
|      ! 0 | 1121 | `			}else{` |
|       10 | 1122 | `				if( zIn[0] == '\n' ){` |
|       10 | 1123 | `					nLine++;` |
|        4 | 1124 | `				}` |
|       10 | 1125 | `				zIn++;` |
|        - | 1126 | `			 }` |
|        2 | 1127 | `		} /* While(zIn < zEnd) */` |
|    11130 | 1128 | `		if( zCurEnd == 0 ){` |
|        5 | 1129 | `			zCurEnd = zIn;` |
|        2 | 1130 | `		}` |
|        - | 1131 | `		/* Save the raw token */` |
|    11130 | 1132 | `		SyStringInitFromBuf(&sToken.sData,zCur,zCurEnd - zCur);` |
|    11130 | 1133 | `		sToken.nType = PH7_TOKEN_RAW;` |
|    11130 | 1134 | `		rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|    11130 | 1135 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1136 | `			return rc;` |
|        - | 1137 | `		}` |
|    11130 | 1138 | `		if( zIn >= zEnd ){` |
|        5 | 1139 | `			break;` |
|        - | 1140 | `		}` |
|        - | 1141 | `		/* Ignore leading white space */` |
|    24140 | 1142 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    13016 | 1143 | `			if( zIn[0] == '\n' ){` |
|    11822 | 1144 | `				nLine++;` |
|     5910 | 1145 | `			}` |
|    13016 | 1146 | `			zIn++;` |
|        2 | 1147 | `		}` |
|        - | 1148 | `		/* Delimit the PHP chunk */` |
|    11126 | 1149 | `		sToken.nLine = nLine;` |
|    11126 | 1150 | `		zCur = zIn;` |
|  1021198 | 1151 | `		while( (sxu32)(zEnd - zIn) >= sCtag.nByte ){` |
|        - | 1152 | `			const char *zPtr;` |
|  1016342 | 1153 | `			if( SyMemcmp(zIn,sCtag.zString,sCtag.nByte) == 0 && iNest < 1 ){` |
|     6270 | 1154 | `				break;` |
|        - | 1155 | `			}` |
|   507015 | 1156 | `			for(;;){` |
|  1014032 | 1157 | `				if( zIn[0] != '/' \|\| (zIn[1] != '*' && zIn[1] != '/') /* && sCtag.nByte >= 2 */ ){` |
|   505038 | 1158 | `					break;` |
|        - | 1159 | `				}` |
|     3960 | 1160 | `				zIn += 2;` |
|     3960 | 1161 | `				if( zIn[-1] == '/' ){` |
|        - | 1162 | `					/* Inline comment */` |
|   137892 | 1163 | `					while( zIn < zEnd && zIn[0] != '\n' ){` |
|   134016 | 1164 | `						zIn++;` |
|        2 | 1165 | `					}` |
|     3878 | 1166 | `					if( zIn >= zEnd ){` |
|      ! 0 | 1167 | `						zIn--;` |
|      ! 0 | 1168 | `					}` |
|     1940 | 1169 | `				}else{` |
|        - | 1170 | `					/* Block comment */` |
|     4500 | 1171 | `					while( (sxu32)(zEnd-zIn) >= sizeof("*/") - 1 ){` |
|     4500 | 1172 | `						if( zIn[0] == '*' && zIn[1] == '/' ){` |
|       84 | 1173 | `							zIn += 2;` |
|       84 | 1174 | `							break;` |
|        - | 1175 | `						}` |
|     4418 | 1176 | `						if( zIn[0] == '\n' ){` |
|       28 | 1177 | `							nLine++;` |
|       13 | 1178 | `						}` |
|     4418 | 1179 | `						zIn++;` |
|        2 | 1180 | `					}` |
|        - | 1181 | `				}` |
|        2 | 1182 | `			}` |
|  1010074 | 1183 | `			if( zIn[0] == '\n' ){` |
|    35678 | 1184 | `				nLine++;` |
|    35678 | 1185 | `				if( iNest > 0 ){` |
|      282 | 1186 | `					zIn++;` |
|      666 | 1187 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|      386 | 1188 | `						zIn++;` |
|        2 | 1189 | `					}` |
|      282 | 1190 | `					zPtr = zIn;` |
|     1440 | 1191 | `					while( zIn < zEnd ){` |
|     1440 | 1192 | `						if( (unsigned char)zIn[0] >= 0xc0 ){` |
|        - | 1193 | `							/* UTF-8 stream */` |
|       19 | 1194 | `							zIn++;` |
|       37 | 1195 | `							SX_JMP_UTF8(zIn,zEnd);` |
|     1430 | 1196 | `						}else if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|      142 | 1197 | `							break;` |
|      ! 0 | 1198 | `						}else{` |
|     1142 | 1199 | `							zIn++;` |
|        - | 1200 | `						}` |
|        2 | 1201 | `					}` |
|      282 | 1202 | `					if( (sxu32)(zIn - zPtr) == sDoc.nByte && SyMemcmp(sDoc.zString,zPtr,sDoc.nByte) == 0 ){` |
|      110 | 1203 | `						iNest = 0;` |
|       54 | 1204 | `					}` |
|      282 | 1205 | `					continue;` |
|        2 | 1206 | `				}` |
|   992096 | 1207 | `			}else if ( (sxu32)(zEnd - zIn) >= sizeof("<<<") && zIn[0] == '<' && zIn[1] == '<' && zIn[2] == '<' && iNest < 1){` |
|      112 | 1208 | `				zIn += sizeof("<<<")-1;` |
|      124 | 1209 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|       13 | 1210 | `					zIn++;` |
|        1 | 1211 | `				}` |
|      112 | 1212 | `				if( zIn[0] == '"' \|\| zIn[0] == '\'' ){` |
|       44 | 1213 | `					zIn++;` |
|       21 | 1214 | `				}` |
|      112 | 1215 | `				zPtr = zIn;` |
|      530 | 1216 | `				while( zIn < zEnd ){` |
|      530 | 1217 | `					if( (unsigned char)zIn[0] >= 0xc0 ){` |
|        - | 1218 | `						/* UTF-8 stream */` |
|       19 | 1219 | `						zIn++;` |
|       37 | 1220 | `						SX_JMP_UTF8(zIn,zEnd);` |
|      520 | 1221 | `					}else if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       57 | 1222 | `						break;` |
|      ! 0 | 1223 | `					}else{` |
|      402 | 1224 | `						zIn++;` |
|        - | 1225 | `					}` |
|        2 | 1226 | `				}` |
|      112 | 1227 | `				SyStringInitFromBuf(&sDoc,zPtr,zIn-zPtr);` |
|      112 | 1228 | `				SyStringFullTrim(&sDoc);` |
|      112 | 1229 | `				if( sDoc.nByte > 0 ){` |
|      112 | 1230 | `					iNest++;` |
|       55 | 1231 | `				}` |
|      112 | 1232 | `				continue;` |
|        - | 1233 | `			}` |
|  1009684 | 1234 | `			zIn++;` |
|        - | 1235 |  |
|  1009684 | 1236 | `			if ( zIn >= zEnd )` |
|      ! 0 | 1237 | `				break;` |
|        2 | 1238 | `		}` |
|    11126 | 1239 | `		if( (sxu32)(zEnd - zIn) < sCtag.nByte ){` |
|     4858 | 1240 | `			zIn = zEnd;` |
|     2428 | 1241 | `		}` |
|    11126 | 1242 | `		if( zCur < zIn ){` |
|        - | 1243 | `			/* Save the PHP chunk for later processing */` |
|     8982 | 1244 | `			sToken.nType = PH7_TOKEN_PHP;` |
|     8982 | 1245 | `			SyStringInitFromBuf(&sToken.sData,zCur,zIn-zCur);` |
|    17904 | 1246 | `			SyStringRightTrim(&sToken.sData); /* Trim trailing white spaces */` |
|     8982 | 1247 | `			rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|     8982 | 1248 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1249 | `				return rc;` |
|        - | 1250 | `			}` |
|     4490 | 1251 | `		}` |
|    11126 | 1252 | `		if( zIn < zEnd ){` |
|        - | 1253 | `			/* Jump the trailing closing tag */` |
|     6270 | 1254 | `			zIn += sCtag.nByte;` |
|     3134 | 1255 | `		}` |
|        2 | 1256 | `	} /* For(;;) */` |
|        - | 1257 |  |
|    11122 | 1258 | ` 	return SXRET_OK;` |
|     5562 | 1259 |  |
|        - | 1260 |  |
