# src/ph7/lex.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 823/880 lines (93.52%)

[Root index](../../index.md) | [Directory index](index.md)

|      Hits | Line | Source |
| --------: | ---: | :--- |
|         - |    1 | `/**` |
|         - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|         - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|         - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|         - |    5 | ` */` |
|         - |    6 | `#include "ph7int.h"` |
|         - |    7 | `/*` |
|         - |    8 | ` * This file implement an efficient hand-coded,thread-safe and full-reentrant` |
|         - |    9 | ` * lexical analyzer/Tokenizer for the PH7 engine.` |
|         - |   10 | ` */` |
|         - |   11 | `/* Forward declaration */` |
|         - |   12 | `static sxu32 KeywordCode(const char *z, int n);` |
|         - |   13 | `static sxi32 LexExtractHeredoc(SyStream *pStream,SyToken *pToken);` |
|         - |   14 | `/*` |
|         - |   15 | ` * Tokenize a raw PHP input.` |
|         - |   16 | ` * Get a single low-level token from the input file. Update the stream pointer so that` |
|         - |   17 | ` * it points to the first character beyond the extracted token.` |
|         - |   18 | ` */` |
| 186335434 |   19 | `static sxi32 TokenizePHP(SyStream *pStream,SyToken *pToken,void *pUserData,void *pCtxData)` |
|         5 |   20 | `{` |
|         - |   21 | `	SyString *pStr;` |
|         - |   22 | `	sxi32 rc;` |
|         - |   23 | `	/* Ignore leading white spaces */` |
| 276367327 |   24 | `	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){` |
|         - |   25 | `		/* Advance the stream cursor */` |
|  90031893 |   26 | `		if( pStream->zText[0] == '\n' ){` |
|         - |   27 | `			/* Update line counter */` |
|     54699 |   28 | `			pStream->nLine++;` |
|     27347 |   29 | `		}` |
|  90031893 |   30 | `		pStream->zText++;` |
|         5 |   31 | `	}` |
| 186335439 |   32 | `	if( pStream->zText >= pStream->zEnd ){` |
|         - |   33 | `		/* End of input reached */` |
|         3 |   34 | `		return SXERR_EOF;` |
|         - |   35 | `	}` |
|         - |   36 | `	/* Record token starting position and line */` |
| 186335437 |   37 | `	pToken->nLine = pStream->nLine;` |
| 186335437 |   38 | `	pToken->pUserData = 0;` |
| 186335437 |   39 | `	pStr = &pToken->sData;` |
| 186335437 |   40 | `	SyStringInitFromBuf(pStr,pStream->zText,0);` |
| 217668580 |   41 | `	if( pStream->zText[0] >= 0xc0 \|\| SyisAlpha(pStream->zText[0]) \|\| pStream->zText[0] == '_' ){` |
|         - |   42 | `		/* The following code fragment is taken verbatim from the xPP source tree.` |
|         - |   43 | `		 * xPP is a modern embeddable macro processor with advanced features useful for` |
|         - |   44 | `		 * application seeking for a production quality,ready to use macro processor.` |
|         - |   45 | `		 * xPP is a widely used library developed and maintened by Symisc Systems.` |
|         - |   46 | `		 * You can reach the xPP home page by following this link:` |
|         - |   47 | `		 * http://xpp.symisc.net/` |
|         - |   48 | `		 */` |
|         - |   49 | `		const unsigned char *zIn;` |
|         - |   50 | `		sxu32 nKeyword;` |
|         - |   51 | `		/* Isolate UTF-8 or alphanumeric stream */` |
|  62666291 |   52 | `		if( pStream->zText[0] < 0xc0 ){` |
|  62666269 |   53 | `			pStream->zText++;` |
|  31333132 |   54 | `		}` |
|  58942635 |   55 | `		for(;;){` |
| 117885275 |   56 | `			zIn = pStream->zText;` |
| 117885275 |   57 | `			if( zIn[0] >= 0xc0 ){` |
|        81 |   58 | `				zIn++;` |
|         - |   59 | `				/* UTF-8 stream */` |
|       173 |   60 | `				while( zIn < pStream->zEnd && ((zIn[0] & 0xc0) == 0x80) ){` |
|        93 |   61 | `					zIn++;` |
|         1 |   62 | `				}` |
|        40 |   63 | `			}` |
|         - |   64 | `			/* Skip alphanumeric stream */` |
| 473574388 |   65 | `			while( zIn < pStream->zEnd && zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_') ){` |
| 296746483 |   66 | `				zIn++;` |
|         5 |   67 | `			}` |
| 117885275 |   68 | `			if( zIn == pStream->zText ){` |
|         - |   69 | `				/* Not an UTF-8 or alphanumeric stream */` |
|  62666291 |   70 | `				break;` |
|         - |   71 | `			}` |
|         - |   72 | `			/* Synchronize pointers */` |
|  55218989 |   73 | `			pStream->zText = zIn;` |
|         5 |   74 | `		}` |
|         - |   75 | `		/* Record token length */` |
|  62666291 |   76 | `		pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  62666291 |   77 | `		nKeyword = KeywordCode(pStr->zString,(int)pStr->nByte);` |
|         - |   78 | `		/* PHP 7.4: 'fn' is a keyword reserved for arrow functions.` |
|         - |   79 | `		 * The auto-generated perfect hash above doesn't know about it,` |
|         - |   80 | `		 * so intercept the 'fn' identifier here.` |
|         - |   81 | `		 */` |
|  62666286 |   82 | `		if( nKeyword == PH7_TK_ID && pStr->nByte == 2` |
|  21643392 |   83 | `			&& pStr->zString[0] == 'f' && pStr->zString[1] == 'n' ){` |
|       661 |   84 | `			nKeyword = PH7_TKWRD_FN;` |
|       328 |   85 | `		}` |
|  62666291 |   86 | `		if( nKeyword != PH7_TK_ID ){` |
|  20747969 |   87 | `			if( nKeyword &` |
|         - |   88 | `				(PH7_TKWRD_NEW\|PH7_TKWRD_CLONE\|PH7_TKWRD_AND\|PH7_TKWRD_XOR\|PH7_TKWRD_OR\|PH7_TKWRD_INSTANCEOF) ){` |
|         - |   89 | `					/* Alpha stream operators [i.e: new,clone,and,instanceof,or,xor],save the operator instance for later processing */` |
|   1001429 |   90 | `					pToken->pUserData = (void *)PH7_ExprExtractOperator(pStr,0);` |
|         - |   91 | `					/* Mark as an operator */` |
|   1001429 |   92 | `					pToken->nType = PH7_TK_ID\|PH7_TK_OP;` |
|    500717 |   93 | `			}else{` |
|         - |   94 | `				/* We are dealing with a keyword [i.e: while,foreach,class...],save the keyword ID */` |
|  19746545 |   95 | `				pToken->nType = PH7_TK_KEYWORD;` |
|  19746545 |   96 | `				pToken->pUserData = SX_INT_TO_PTR(nKeyword);` |
|         - |   97 | `			}` |
|  10373987 |   98 | `		}else{` |
|         - |   99 | `			/* A simple identifier */` |
|  41918327 |  100 | `			pToken->nType = PH7_TK_ID;` |
|         - |  101 | `		}` |
|  31333148 |  102 | `	}else{` |
|         - |  103 | `		sxi32 c;` |
|         - |  104 | `		/* Non-alpha stream */` |
| 123669151 |  105 | `		if( pStream->zText[0] == '#' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '[' ){` |
|      7931 |  106 | `			sxu32 nDepth = 1;` |
|         - |  107 | `			/* PHP 8 attribute group '#[ ... ]': skip the whole balanced group as` |
|         - |  108 | `			 * trivia (attributes are not stored yet). Brackets inside string` |
|         - |  109 | `			 * literals and comments must not affect the depth count. An` |
|         - |  110 | `			 * unterminated group is silently consumed up to EOF, consistent` |
|         - |  111 | `			 * with unterminated block comments below.` |
|         - |  112 | `			 */` |
|         - |  113 | `			const unsigned char *zGroupStart;` |
|      7931 |  114 | `			pStream->zText += 2;` |
|      7931 |  115 | `			zGroupStart = pStream->zText;` |
|    647423 |  116 | `			while( pStream->zText < pStream->zEnd && nDepth > 0 ){` |
|    639497 |  117 | `				sxi32 d = pStream->zText[0];` |
|    639497 |  118 | `				if( d == '[' ){` |
|        11 |  119 | `					nDepth++;` |
|    639492 |  120 | `				}else if( d == ']' ){` |
|      7941 |  121 | `					nDepth--;` |
|    635519 |  122 | `				}else if( d == '\'' \|\| d == '"' ){` |
|         - |  123 | `					/* String literal: scan for the matching unescaped quote */` |
|        46 |  124 | `					pStream->zText++;` |
|       296 |  125 | `					while( pStream->zText < pStream->zEnd ){` |
|       296 |  126 | `						if( pStream->zText[0] == '\\' && &pStream->zText[1] < pStream->zEnd ){` |
|         3 |  127 | `							if( pStream->zText[1] == '\n' ){` |
|       ! 0 |  128 | `								pStream->nLine++;` |
|       ! 0 |  129 | `							}` |
|         3 |  130 | `							pStream->zText += 2;` |
|         3 |  131 | `							continue;` |
|         - |  132 | `						}` |
|       294 |  133 | `						if( pStream->zText[0] == d ){` |
|        46 |  134 | `							break;` |
|         - |  135 | `						}` |
|       250 |  136 | `						if( pStream->zText[0] == '\n' ){` |
|       ! 0 |  137 | `							pStream->nLine++;` |
|       ! 0 |  138 | `						}` |
|       250 |  139 | `						pStream->zText++;` |
|         2 |  140 | `					}` |
|        46 |  141 | `					if( pStream->zText >= pStream->zEnd ){` |
|       ! 0 |  142 | `						break; /* Unterminated string literal */` |
|         2 |  143 | `					}` |
|         - |  144 | `					/* Fall through: consume the closing quote below */` |
|    631529 |  145 | `				}else if( d == '#' \|\| (d == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '/') ){` |
|         - |  146 | `					/* Inline comment inside the group */` |
|       ! 0 |  147 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] != '\n' ){` |
|       ! 0 |  148 | `						pStream->zText++;` |
|       ! 0 |  149 | `					}` |
|       ! 0 |  150 | `					continue; /* Let the outer loop count the newline */` |
|    631507 |  151 | `				}else if( d == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '*' ){` |
|         - |  152 | `					/* Block comment inside the group */` |
|       ! 0 |  153 | `					pStream->zText += 2;` |
|       ! 0 |  154 | `					while( pStream->zText < pStream->zEnd ){` |
|       ! 0 |  155 | `						if( pStream->zText[0] == '*' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '/' ){` |
|       ! 0 |  156 | `							pStream->zText += 2;` |
|       ! 0 |  157 | `							break;` |
|         - |  158 | `						}` |
|       ! 0 |  159 | `						if( pStream->zText[0] == '\n' ){` |
|       ! 0 |  160 | `							pStream->nLine++;` |
|       ! 0 |  161 | `						}` |
|       ! 0 |  162 | `						pStream->zText++;` |
|       ! 0 |  163 | `					}` |
|       ! 0 |  164 | `					continue;` |
|    631507 |  165 | `				}else if( d == '\n' ){` |
|         7 |  166 | `					pStream->nLine++;` |
|         3 |  167 | `				}` |
|    639497 |  168 | `				pStream->zText++;` |
|         5 |  169 | `			}` |
|      7931 |  170 | `			if( pUserData && pStream->pSet ){` |
|         - |  171 | `				/* Record the group's inner span (between #[ and its balanced ])` |
|         - |  172 | `				 * in the trivia sidecar, keyed like doc-comments. */` |
|         - |  173 | `				ph7_trivia sTrivia;` |
|      7931 |  174 | `				const unsigned char *zGroupEnd = pStream->zText;` |
|      7931 |  175 | `				if( nDepth == 0 && zGroupEnd > zGroupStart ){` |
|      7931 |  176 | `					zGroupEnd--; /* Exclude the closing ']' */` |
|      3963 |  177 | `				}` |
|      7931 |  178 | `				sTrivia.nTokIdx = SySetUsed(pStream->pSet);` |
|      7931 |  179 | `				sTrivia.iKind = PH7_TRIVIA_ATTR;` |
|      7931 |  180 | `				SyStringInitFromBuf(&sTrivia.sText,(const char *)zGroupStart,(sxu32)(zGroupEnd - zGroupStart));` |
|      7931 |  181 | `				sTrivia.nLine = pToken->nLine;` |
|      7931 |  182 | `				SySetPut((SySet *)pUserData,(const void *)&sTrivia);` |
|      3963 |  183 | `			}` |
|         - |  184 | `			/* Tell the upper-layer to ignore this token */` |
|      7931 |  185 | `			return SXERR_CONTINUE;` |
| 123775920 |  186 | `		}else if( pStream->zText[0] == '#' \|\|` |
| 123661214 |  187 | `			( pStream->zText[0] == '/' &&  &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '/') ){` |
|      7753 |  188 | `				pStream->zText++;` |
|         - |  189 | `				/* Inline comments */` |
|    351881 |  190 | `				while( pStream->zText < pStream->zEnd && pStream->zText[0] != '\n' ){` |
|    344133 |  191 | `					pStream->zText++;` |
|         5 |  192 | `				}` |
|         - |  193 | `				/* Tell the upper-layer to ignore this token */` |
|      7753 |  194 | `				return SXERR_CONTINUE;` |
| 123653477 |  195 | `		}else if( pStream->zText[0] == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '*' ){` |
|         - |  196 | `			/* A doc-comment starts with slash-star-star followed by more` |
|         - |  197 | `			 * content (slash-star-star-slash is the empty comment, not a` |
|         - |  198 | `			 * docblock). Its full span, delimiters included, goes to the` |
|         - |  199 | `			 * trivia sidecar when the caller supplied one — keyed by the` |
|         - |  200 | `			 * index the NEXT real token receives — and never enters the` |
|         - |  201 | `			 * token stream. */` |
|    217671 |  202 | `			const unsigned char *zDocStart = pStream->zText;` |
|    217683 |  203 | `			int bDoc = ( &pStream->zText[2] < pStream->zEnd && pStream->zText[2] == '*'` |
|    326511 |  204 | `			 && ( &pStream->zText[3] >= pStream->zEnd \|\| pStream->zText[3] != '/' ) );` |
|    217671 |  205 | `			pStream->zText += 2;` |
|         - |  206 | `			/* Block comment */` |
|  17197135 |  207 | `			while( pStream->zText < pStream->zEnd ){` |
|  17197135 |  208 | `				if( pStream->zText[0] == '*' ){` |
|    303359 |  209 | `					if( &pStream->zText[1] >= pStream->zEnd \|\| pStream->zText[1] == '/'  ){` |
|    108838 |  210 | `						break;` |
|         - |  211 | `					}` |
|     42844 |  212 | `				}` |
|  16979469 |  213 | `				if( pStream->zText[0] == '\n' ){` |
|       257 |  214 | `					pStream->nLine++;` |
|       126 |  215 | `				}` |
|  16979469 |  216 | `				pStream->zText++;` |
|         5 |  217 | `			}` |
|    217671 |  218 | `			pStream->zText += 2;` |
|    217671 |  219 | `			if( bDoc && pUserData && pStream->pSet ){` |
|         - |  220 | `				ph7_trivia sTrivia;` |
|        29 |  221 | `				const unsigned char *zDocEnd = pStream->zText;` |
|        29 |  222 | `				if( zDocEnd > pStream->zEnd ){` |
|       ! 0 |  223 | `					zDocEnd = pStream->zEnd; /* Unterminated comment at EOF */` |
|       ! 0 |  224 | `				}` |
|        29 |  225 | `				sTrivia.nTokIdx = SySetUsed(pStream->pSet);` |
|        29 |  226 | `				sTrivia.iKind = PH7_TRIVIA_DOC;` |
|        29 |  227 | `				SyStringInitFromBuf(&sTrivia.sText,(const char *)zDocStart,(sxu32)(zDocEnd - zDocStart));` |
|        29 |  228 | `				sTrivia.nLine = pToken->nLine;` |
|        29 |  229 | `				SySetPut((SySet *)pUserData,(const void *)&sTrivia);` |
|        12 |  230 | `			}` |
|         - |  231 | `			/* Tell the upper-layer to ignore this token */` |
|    217671 |  232 | `			return SXERR_CONTINUE;` |
| 123435811 |  233 | `		}else if( SyisDigit(pStream->zText[0]) ){` |
|   3827715 |  234 | `			pStream->zText++;` |
|         - |  235 | `			/* PHP 7.4: handle underscore separator immediately following the first digit.` |
|         - |  236 | `			 * Check pStream->zText < pStream->zEnd BEFORE forming pStream->zText + 1 so` |
|         - |  237 | `			 * we never compute a pointer past one-past-end. */` |
|   3827710 |  238 | `			if( pStream->zText < pStream->zEnd` |
|   3827710 |  239 | `				&& pStream->zText[0] == '_'` |
|   1913936 |  240 | `				&& pStream->zText + 1 < pStream->zEnd` |
|       162 |  241 | `				&& pStream->zText[1] < 0xc0` |
|       167 |  242 | `				&& SyisDigit(pStream->zText[1]) ){` |
|       153 |  243 | `				pStream->zText++; /* swallow underscore between two digits */` |
|        76 |  244 | `			}` |
|         - |  245 | `			/* Decimal digit stream (PHP 7.4: underscore separator allowed between two digits) */` |
|   4808749 |  246 | `			while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|    981039 |  247 | `				pStream->zText++;` |
|    981034 |  248 | `				if( pStream->zText < pStream->zEnd` |
|    981034 |  249 | `					&& pStream->zText[0] == '_'` |
|    490603 |  250 | `					&& pStream->zText + 1 < pStream->zEnd` |
|       172 |  251 | `					&& pStream->zText[1] < 0xc0` |
|       177 |  252 | `					&& SyisDigit(pStream->zText[1]) ){` |
|       173 |  253 | `					pStream->zText++; /* swallow underscore between two digits */` |
|        86 |  254 | `				}` |
|         5 |  255 | `			}` |
|         - |  256 | `			/* Mark the token as integer until we encounter a real number */` |
|   3827715 |  257 | `			pToken->nType = PH7_TK_INTEGER;` |
|   3827715 |  258 | `			if( pStream->zText < pStream->zEnd ){` |
|   3827715 |  259 | `				c = pStream->zText[0];` |
|   3827715 |  260 | `				if( c == '.' ){` |
|         - |  261 | `					/* Real number (PHP 7.4: underscore separator allowed between two digits) */` |
|      8631 |  262 | `					pStream->zText++;` |
|     18711 |  263 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|     10085 |  264 | `						pStream->zText++;` |
|     10080 |  265 | `						if( pStream->zText < pStream->zEnd` |
|     10080 |  266 | `							&& pStream->zText[0] == '_'` |
|      5046 |  267 | `							&& pStream->zText + 1 < pStream->zEnd` |
|        12 |  268 | `							&& pStream->zText[1] < 0xc0` |
|        17 |  269 | `							&& SyisDigit(pStream->zText[1]) ){` |
|        13 |  270 | `							pStream->zText++;` |
|         6 |  271 | `						}` |
|         5 |  272 | `					}` |
|      8631 |  273 | `					if( pStream->zText < pStream->zEnd ){` |
|      8631 |  274 | `						c = pStream->zText[0];` |
|      8631 |  275 | `						if( c=='e' \|\| c=='E' ){` |
|        63 |  276 | `							pStream->zText++;` |
|        63 |  277 | `							if( pStream->zText < pStream->zEnd ){` |
|        63 |  278 | `								c = pStream->zText[0];` |
|        62 |  279 | `								if( (c =='+' \|\| c=='-') && &pStream->zText[1] < pStream->zEnd  &&` |
|        33 |  280 | `									pStream->zText[1] < 0xc0 && SyisDigit(pStream->zText[1]) ){` |
|        33 |  281 | `										pStream->zText++;` |
|        16 |  282 | `								}` |
|       179 |  283 | `								while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|       117 |  284 | `									pStream->zText++;` |
|       116 |  285 | `									if( pStream->zText < pStream->zEnd` |
|       116 |  286 | `										&& pStream->zText[0] == '_'` |
|        62 |  287 | `										&& pStream->zText + 1 < pStream->zEnd` |
|         8 |  288 | `										&& pStream->zText[1] < 0xc0` |
|         9 |  289 | `										&& SyisDigit(pStream->zText[1]) ){` |
|         9 |  290 | `										pStream->zText++;` |
|         4 |  291 | `									}` |
|         1 |  292 | `								}` |
|        31 |  293 | `							}` |
|        31 |  294 | `						}` |
|      4313 |  295 | `					}` |
|      8631 |  296 | `					pToken->nType = PH7_TK_REAL;` |
|   3823402 |  297 | `				}else if( c=='e' \|\| c=='E' ){` |
|        54 |  298 | `					SXUNUSED(pUserData); /* Prevent compiler warning */` |
|        54 |  299 | `					SXUNUSED(pCtxData);` |
|       110 |  300 | `					pStream->zText++;` |
|       110 |  301 | `					if( pStream->zText < pStream->zEnd ){` |
|       110 |  302 | `						c = pStream->zText[0];` |
|       108 |  303 | `						if( (c =='+' \|\| c=='-') && &pStream->zText[1] < pStream->zEnd  &&` |
|        36 |  304 | `							pStream->zText[1] < 0xc0 && SyisDigit(pStream->zText[1]) ){` |
|        36 |  305 | `								pStream->zText++;` |
|        17 |  306 | `						}` |
|       338 |  307 | `						while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|       230 |  308 | `							pStream->zText++;` |
|       228 |  309 | `							if( pStream->zText < pStream->zEnd` |
|       228 |  310 | `								&& pStream->zText[0] == '_'` |
|       116 |  311 | `								&& pStream->zText + 1 < pStream->zEnd` |
|         4 |  312 | `								&& pStream->zText[1] < 0xc0` |
|         6 |  313 | `								&& SyisDigit(pStream->zText[1]) ){` |
|         5 |  314 | `								pStream->zText++;` |
|         2 |  315 | `							}` |
|         2 |  316 | `						}` |
|        54 |  317 | `					}` |
|       110 |  318 | `					pToken->nType = PH7_TK_REAL;` |
|         - |  319 | `				/* php only reads a base prefix when the literal so far is exactly "0"` |
|         - |  320 | `				 * AND at least one valid digit follows it. Otherwise the '0' stands` |
|         - |  321 | `				 * alone as an integer and the letter begins an IDENTIFIER, which is` |
|         - |  322 | ``				 * why php reports `0xG` as `unexpected identifier "xG"` while PHL,`` |
|         - |  323 | `				 * consuming the prefix unconditionally, reported just "G". The same` |
|         - |  324 | ``				 * gap silently ACCEPTED `0x`/`0b`/`0o` as int(0), and read `1x5` as`` |
|         - |  325 | `				 * a hex literal, both of which php rejects outright. */` |
|   3819032 |  326 | `				}else if( (c == 'x' \|\| c == 'X')` |
|   1961945 |  327 | `					&& pStream->zText == &((const unsigned char *)pStr->zString)[1] && pStr->zString[0] == 0x30` |
|    104914 |  328 | `					&& &pStream->zText[1] < pStream->zEnd` |
|    104919 |  329 | `					&& pStream->zText[1] < 0xc0 && SyisHex(pStream->zText[1]) ){` |
|         - |  330 | `					/* Hex digit stream (PHP 7.4: underscore separator allowed between two digits) */` |
|    104915 |  331 | `					pStream->zText++;` |
|    454787 |  332 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisHex(pStream->zText[0]) ){` |
|    349877 |  333 | `						pStream->zText++;` |
|    349872 |  334 | `						if( pStream->zText < pStream->zEnd` |
|    349872 |  335 | `							&& pStream->zText[0] == '_'` |
|    174961 |  336 | `							&& pStream->zText + 1 < pStream->zEnd` |
|        50 |  337 | `							&& pStream->zText[1] < 0xc0` |
|        55 |  338 | `							&& SyisHex(pStream->zText[1]) ){` |
|        51 |  339 | `							pStream->zText++;` |
|        25 |  340 | `						}` |
|         5 |  341 | `					}` |
|   3766526 |  342 | `				}else if( (c == 'b' \|\| c == 'B')` |
|   1857177 |  343 | `					&& pStream->zText == &((const unsigned char *)pStr->zString)[1] && pStr->zString[0] == 0x30` |
|       288 |  344 | `					&& &pStream->zText[1] < pStream->zEnd` |
|       293 |  345 | `					&& (pStream->zText[1] == '0' \|\| pStream->zText[1] == '1') ){` |
|         - |  346 | `					/* Binary digit stream (PHP 7.4: underscore separator allowed between two digits) */` |
|       287 |  347 | `					pStream->zText++;` |
|      3115 |  348 | `					while( pStream->zText < pStream->zEnd && (pStream->zText[0] == '0' \|\| pStream->zText[0] == '1') ){` |
|      1791 |  349 | `						pStream->zText++;` |
|      1790 |  350 | `						if( pStream->zText < pStream->zEnd` |
|      1790 |  351 | `							&& pStream->zText[0] == '_'` |
|       965 |  352 | `							&& pStream->zText + 1 < pStream->zEnd` |
|       141 |  353 | `							&& (pStream->zText[1] == '0' \|\| pStream->zText[1] == '1') ){` |
|       141 |  354 | `							pStream->zText++;` |
|        70 |  355 | `						}` |
|         1 |  356 | `					}` |
|   3713924 |  357 | `				}else if( (c == 'o' \|\| c == 'O')` |
|   1856900 |  358 | `					&& pStream->zText == &((const unsigned char *)pStr->zString)[1] && pStr->zString[0] == 0x30` |
|        20 |  359 | `					&& &pStream->zText[1] < pStream->zEnd` |
|        25 |  360 | `					&& pStream->zText[1] >= '0' && pStream->zText[1] <= '7' ){` |
|         - |  361 | `					/* PHP 8.1 explicit octal 0o/0O (underscore separator allowed between two digits) */` |
|        21 |  362 | `					pStream->zText++;` |
|       101 |  363 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] >= '0' && pStream->zText[0] <= '7' ){` |
|        81 |  364 | `						pStream->zText++;` |
|        80 |  365 | `						if( pStream->zText < pStream->zEnd` |
|        80 |  366 | `							&& pStream->zText[0] == '_'` |
|        41 |  367 | `							&& pStream->zText + 1 < pStream->zEnd` |
|         3 |  368 | `							&& pStream->zText[1] >= '0' && pStream->zText[1] <= '7' ){` |
|         3 |  369 | `							pStream->zText++;` |
|         1 |  370 | `						}` |
|         1 |  371 | `					}` |
|        10 |  372 | `				}` |
|   1913855 |  373 | `			}` |
|         - |  374 | `			/* PHP 7.4: absorb a trailing malformed underscore run into the` |
|         - |  375 | `			 * numeric token so the compile phase can emit a PHP-compatible` |
|         - |  376 | `			 * "syntax error, unexpected identifier" parse error. Valid` |
|         - |  377 | `			 * separators were already consumed by the per-loop peek logic` |
|         - |  378 | `			 * above, so an underscore here is always misplaced. */` |
|   3827715 |  379 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '_' ){` |
|        14 |  380 | `				pStream->zText++;` |
|        28 |  381 | `				while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0` |
|        31 |  382 | `					&& (SyisAlphaNum(pStream->zText[0]) \|\| pStream->zText[0] == '_') ){` |
|        10 |  383 | `					pStream->zText++;` |
|         2 |  384 | `				}` |
|         5 |  385 | `			}` |
|         - |  386 | `			/* Record token length */` |
|   3827715 |  387 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|   3827715 |  388 | `			return SXRET_OK;` |
|         - |  389 | `		}` |
| 119608101 |  390 | `		c = pStream->zText[0];` |
| 119608101 |  391 | `		pStream->zText++; /* Advance the stream cursor */` |
|         - |  392 | `		/* Assume we are dealing with an operator*/` |
| 119608101 |  393 | `		pToken->nType = PH7_TK_OP;` |
| 119608101 |  394 | `		switch(c){` |
|  24713933 |  395 | `		case '$': pToken->nType = PH7_TK_DOLLAR; break;` |
|   7345919 |  396 | `		case '{': pToken->nType = PH7_TK_OCB;    break;` |
|   7345905 |  397 | `		case '}': pToken->nType = PH7_TK_CCB;    break;` |
|  15479349 |  398 | `		case '(': pToken->nType = PH7_TK_LPAREN; break;` |
|   3270997 |  399 | `		case '[': pToken->nType \|= PH7_TK_OSB;   break; /* Bitwise operation here,since the square bracket token '['` |
|         - |  400 | `														 * is a potential operator [i.e: subscripting] */` |
|   3271003 |  401 | `		case ']': pToken->nType = PH7_TK_CSB;    break;` |
|   7739664 |  402 | `		case ')': {` |
|  15479333 |  403 | `			SySet *pTokSet = pStream->pSet;` |
|         - |  404 | `			/* Assemble type cast operators [i.e: (int),(float),(bool)...] */` |
|  15479333 |  405 | `			if( pTokSet->nUsed >= 2 ){` |
|         - |  406 | `				SyToken *pTmp;` |
|         - |  407 | `				/* Peek the last recongnized token */` |
|  15479331 |  408 | `				pTmp = (SyToken *)SySetPeek(pTokSet);` |
|  15479331 |  409 | `				if( pTmp->nType & PH7_TK_KEYWORD ){` |
|   1200237 |  410 | `					sxi32 nID = SX_PTR_TO_INT(pTmp->pUserData);` |
|   1200237 |  411 | `					if( (sxu32)nID & (PH7_TKWRD_ARRAY\|PH7_TKWRD_INT\|PH7_TKWRD_FLOAT\|PH7_TKWRD_STRING\|PH7_TKWRD_OBJECT\|PH7_TKWRD_BOOL\|PH7_TKWRD_UNSET) ){` |
|   1083599 |  412 | `						pTmp = (SyToken *)SySetAt(pTokSet,pTokSet->nUsed - 2);` |
|   1083599 |  413 | `						if( pTmp->nType & PH7_TK_LPAREN ){` |
|         - |  414 | `							/* Merge the three tokens '(' 'TYPE' ')' into a single one */` |
|    761307 |  415 | `							const char * zTypeCast = "(int)";` |
|    761307 |  416 | `							if( nID & PH7_TKWRD_FLOAT ){` |
|     31087 |  417 | `								zTypeCast = "(float)";` |
|    745766 |  418 | `							}else if( nID & PH7_TKWRD_BOOL ){` |
|     23335 |  419 | `								zTypeCast = "(bool)";` |
|    718560 |  420 | `							}else if( nID & PH7_TKWRD_STRING ){` |
|    376637 |  421 | `								zTypeCast = "(string)";` |
|    518579 |  422 | `							}else if( nID & PH7_TKWRD_ARRAY ){` |
|        36 |  423 | `								zTypeCast = "(array)";` |
|    330246 |  424 | `							}else if( nID & PH7_TKWRD_OBJECT ){` |
|        38 |  425 | `								zTypeCast = "(object)";` |
|    330211 |  426 | `							}else if( nID & PH7_TKWRD_UNSET ){` |
|         3 |  427 | `								zTypeCast = "(unset)";` |
|         1 |  428 | `							}` |
|         - |  429 | `							/* Reflect the change */` |
|    761307 |  430 | `							pToken->nType = PH7_TK_OP;` |
|    761307 |  431 | `							SyStringInitFromBuf(&pToken->sData,zTypeCast,SyStrlen(zTypeCast));` |
|         - |  432 | `							/* Save the instance associated with the type cast operator */` |
|    761307 |  433 | `							pToken->pUserData = (void *)PH7_ExprExtractOperator(&pToken->sData,0);` |
|         - |  434 | `							/* Remove the two previous tokens */` |
|    761307 |  435 | `							pTokSet->nUsed -= 2;` |
|    761307 |  436 | `							return SXRET_OK;` |
|         - |  437 | `						}` |
|    161146 |  438 | `					}` |
|    219465 |  439 | `				}` |
|   7359012 |  440 | `			}` |
|  14718031 |  441 | `			pToken->nType = PH7_TK_RPAREN;` |
|  14718031 |  442 | `			break;` |
|         - |  443 | `				  }` |
|   2782239 |  444 | `		case '\'':{` |
|         - |  445 | `			/* Single quoted string */` |
|   5564483 |  446 | `			pStr->zString++;` |
|  63400211 |  447 | `			while( pStream->zText < pStream->zEnd ){` |
|  63400211 |  448 | `				if( pStream->zText[0] == '\''  ){` |
|   5564497 |  449 | `					if( pStream->zText[-1] != '\\' ){` |
|   5529507 |  450 | `						break;` |
|       ! 0 |  451 | `					}else{` |
|     34995 |  452 | `						const unsigned char *zPtr = &pStream->zText[-2];` |
|     34995 |  453 | `						sxi32 i = 1;` |
|     69991 |  454 | `						while( zPtr > pStream->zInput && zPtr[0] == '\\' ){` |
|     35001 |  455 | `							zPtr--;` |
|     35001 |  456 | `							i++;` |
|         5 |  457 | `						}` |
|     34995 |  458 | `						if((i&1)==0){` |
|     34981 |  459 | `							break;` |
|         - |  460 | `						}` |
|         - |  461 | `					}` |
|         7 |  462 | `				}` |
|  57835733 |  463 | `				if( pStream->zText[0] == '\n' ){` |
|        63 |  464 | `					pStream->nLine++;` |
|        31 |  465 | `				}` |
|  57835733 |  466 | `				pStream->zText++;` |
|         5 |  467 | `			}` |
|         - |  468 | `			/* Record token length and type */` |
|   5564483 |  469 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|   5564483 |  470 | `			pToken->nType = PH7_TK_SSTR;` |
|         - |  471 | `			/* Jump the trailing single quote */` |
|   5564483 |  472 | `			pStream->zText++;` |
|   5564483 |  473 | `			return SXRET_OK;` |
|         - |  474 | `				  }` |
|     62663 |  475 | `		case '"':{` |
|         - |  476 | `			sxi32 iNest;` |
|         - |  477 | `			/* Double quoted string */` |
|    125331 |  478 | `			pStr->zString++;` |
|   1732139 |  479 | `			while( pStream->zText < pStream->zEnd ){` |
|   1732139 |  480 | `				if( pStream->zText[0] == '{' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '$'){` |
|       148 |  481 | `					iNest = 1;` |
|       148 |  482 | `					pStream->zText++;` |
|         - |  483 | `					/* TICKET 1433-40: Hnadle braces'{}' in double quoted string where everything is allowed */` |
|      1348 |  484 | `					while(pStream->zText < pStream->zEnd ){` |
|      1348 |  485 | `						if( pStream->zText[0] == '{' ){` |
|         3 |  486 | `							iNest++;` |
|      1347 |  487 | `						}else if (pStream->zText[0] == '}' ){` |
|       150 |  488 | `							iNest--;` |
|       150 |  489 | `							if( iNest <= 0 ){` |
|       148 |  490 | `								pStream->zText++;` |
|       148 |  491 | `								break;` |
|         1 |  492 | `							}` |
|      1199 |  493 | `						}else if( pStream->zText[0] == '\n' ){` |
|       ! 0 |  494 | `							pStream->nLine++;` |
|       ! 0 |  495 | `						}` |
|      1202 |  496 | `						pStream->zText++;` |
|         2 |  497 | `					}` |
|       148 |  498 | `					if( pStream->zText >= pStream->zEnd ){` |
|       ! 0 |  499 | `						break;` |
|         - |  500 | `					}` |
|        73 |  501 | `				}` |
|   1732139 |  502 | `				if( pStream->zText[0] == '"' ){` |
|    125621 |  503 | `					if( pStream->zText[-1] != '\\' ){` |
|    125323 |  504 | `						break;` |
|       ! 0 |  505 | `					}else{` |
|       303 |  506 | `						const unsigned char *zPtr = &pStream->zText[-2];` |
|       303 |  507 | `						sxi32 i = 1;` |
|       359 |  508 | `						while( zPtr > pStream->zInput && zPtr[0] == '\\' ){` |
|        59 |  509 | `							zPtr--;` |
|        59 |  510 | `							i++;` |
|         3 |  511 | `						}` |
|       303 |  512 | `						if((i&1)==0){` |
|         9 |  513 | `							break;` |
|         - |  514 | `						}` |
|         - |  515 | `					}` |
|       145 |  516 | `				}` |
|   1606813 |  517 | `				if( pStream->zText[0] == '\n' ){` |
|        47 |  518 | `					pStream->nLine++;` |
|        23 |  519 | `				}` |
|   1606813 |  520 | `				pStream->zText++;` |
|         5 |  521 | `			}` |
|         - |  522 | `			/* Record token length and type */` |
|    125331 |  523 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|    125331 |  524 | `			pToken->nType = PH7_TK_DSTR;` |
|         - |  525 | `			/* Jump the trailing quote */` |
|    125331 |  526 | `			pStream->zText++;` |
|    125331 |  527 | `			return SXRET_OK;` |
|         - |  528 | `				  }` |
|         1 |  529 | ``		case '`':{`` |
|         - |  530 | `			/* Backtick quoted string */` |
|         3 |  531 | `			pStr->zString++;` |
|        21 |  532 | `			while( pStream->zText < pStream->zEnd ){` |
|        21 |  533 | ``				if( pStream->zText[0] == '`' && pStream->zText[-1] != '\\' ){`` |
|         3 |  534 | `					break;` |
|         - |  535 | `				}` |
|        19 |  536 | `				if( pStream->zText[0] == '\n' ){` |
|       ! 0 |  537 | `					pStream->nLine++;` |
|       ! 0 |  538 | `				}` |
|        19 |  539 | `				pStream->zText++;` |
|         1 |  540 | `			}` |
|         - |  541 | `			/* Record token length and type */` |
|         3 |  542 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|         3 |  543 | `			pToken->nType = PH7_TK_BSTR;` |
|         - |  544 | `			/* Jump the trailing backtick */` |
|         3 |  545 | `			pStream->zText++;` |
|         3 |  546 | `			return SXRET_OK;` |
|         - |  547 | `				  }` |
|      8795 |  548 | `		case '\\': pToken->nType = PH7_TK_NSSEP;  break;` |
|    548303 |  549 | `		case ':':` |
|   1096611 |  550 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == ':' ){` |
|         - |  551 | `				/* Current operator: '::' */` |
|    420347 |  552 | `				pStream->zText++;` |
|    210176 |  553 | `			}else{` |
|    676269 |  554 | `				pToken->nType = PH7_TK_COLON; /* Single colon */` |
|         - |  555 | `			}` |
|   1096611 |  556 | `			break;` |
|   4369279 |  557 | `		case ',': pToken->nType \|= PH7_TK_COMMA;  break; /* Comma is also an operator */` |
|  11613499 |  558 | `		case ';': pToken->nType = PH7_TK_SEMI;    break;` |
|         - |  559 | `			/* Handle combined operators [i.e: +=,===,!=== ...] */` |
|   4124176 |  560 | `		case '=':` |
|   8248357 |  561 | `			pToken->nType \|= PH7_TK_EQUAL;` |
|   8248357 |  562 | `			if( pStream->zText < pStream->zEnd ){` |
|   8248357 |  563 | `				if( pStream->zText[0] == '=' ){` |
|   1476919 |  564 | `					pToken->nType &= ~PH7_TK_EQUAL;` |
|         - |  565 | `					/* Current operator: == */` |
|   1476919 |  566 | `					pStream->zText++;` |
|   1476919 |  567 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  568 | `						/* Current operator: === */` |
|   1426095 |  569 | `						pStream->zText++;` |
|    713050 |  570 | `					}` |
|   7509900 |  571 | `				}else if( pStream->zText[0] == '>' ){` |
|         - |  572 | `					/* Array operator: => */` |
|    554589 |  573 | `					pToken->nType = PH7_TK_ARRAY_OP;` |
|    554589 |  574 | `					pStream->zText++;` |
|    277297 |  575 | `				}else{` |
|         - |  576 | `					/* TICKET 1433-0010: Reference operator '=&' */` |
|   6216859 |  577 | `					const unsigned char *zCur = pStream->zText;` |
|   6216859 |  578 | `					sxu32 nLine = 0;` |
|  12433547 |  579 | `					while( zCur < pStream->zEnd && zCur[0] < 0xc0 && SyisSpace(zCur[0]) ){` |
|   6216693 |  580 | `						if( zCur[0] == '\n' ){` |
|         5 |  581 | `							nLine++;` |
|         2 |  582 | `						}` |
|   6216693 |  583 | `						zCur++;` |
|         5 |  584 | `					}` |
|   6216859 |  585 | `					if( zCur < pStream->zEnd && zCur[0] == '&' ){` |
|         - |  586 | `						/* Current operator: =& */` |
|        75 |  587 | `						pToken->nType &= ~PH7_TK_EQUAL;` |
|        75 |  588 | `						SyStringInitFromBuf(pStr,"=&",sizeof("=&")-1);` |
|         - |  589 | `						/* Update token stream */` |
|        75 |  590 | `						pStream->zText = &zCur[1];` |
|        75 |  591 | `						pStream->nLine += nLine;` |
|        36 |  592 | `					}` |
|         - |  593 | `				}` |
|   4124176 |  594 | `			}` |
|   8248357 |  595 | `			break;` |
|    452635 |  596 | `		case '!':` |
|    905275 |  597 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  598 | `				/* Current operator: != */` |
|    435039 |  599 | `				pStream->zText++;` |
|    435039 |  600 | `				if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  601 | `					/* Current operator: !== */` |
|    419481 |  602 | `					pStream->zText++;` |
|    209738 |  603 | `				}` |
|    217517 |  604 | `			}` |
|    905275 |  605 | `			break;` |
|    340067 |  606 | `		case '&':` |
|    680139 |  607 | `			pToken->nType \|= PH7_TK_AMPER;` |
|    680139 |  608 | `			if( pStream->zText < pStream->zEnd ){` |
|    680139 |  609 | `				if( pStream->zText[0] == '&' ){` |
|    450801 |  610 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|         - |  611 | `					/* Current operator: && */` |
|    450801 |  612 | `					pStream->zText++;` |
|    454741 |  613 | `				}else if( pStream->zText[0] == '=' ){` |
|         7 |  614 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|         - |  615 | `					/* Current operator: &= */` |
|         7 |  616 | `					pStream->zText++;` |
|         3 |  617 | `				}` |
|    340067 |  618 | `			}` |
|    680139 |  619 | `			break;` |
|    200115 |  620 | `		case '\|':` |
|    400235 |  621 | `			if( pStream->zText < pStream->zEnd ){` |
|    400235 |  622 | `				if( pStream->zText[0] == '\|' ){` |
|         - |  623 | `					/* Current operator: \|\| */` |
|    326219 |  624 | `					pStream->zText++;` |
|    237128 |  625 | `				}else if( pStream->zText[0] == '=' ){` |
|         - |  626 | `					/* Current operator: \|= */` |
|     58241 |  627 | `					pStream->zText++;` |
|     44903 |  628 | `				}else if( pStream->zText[0] == '>' ){` |
|         - |  629 | `					/* Current operator: \|> (PHP 8.5 pipe) */` |
|        27 |  630 | `					pStream->zText++;` |
|        13 |  631 | `				}` |
|    200115 |  632 | `			}` |
|    400235 |  633 | `			break;` |
|    227698 |  634 | `		case '+':` |
|    455401 |  635 | `			if( pStream->zText < pStream->zEnd ){` |
|    455401 |  636 | `				if( pStream->zText[0] == '+' ){` |
|         - |  637 | `					/* Current operator: ++ */` |
|    186757 |  638 | `					pStream->zText++;` |
|    362025 |  639 | `				}else if( pStream->zText[0] == '=' ){` |
|         - |  640 | `					/* Current operator: += */` |
|     54429 |  641 | `					pStream->zText++;` |
|     27212 |  642 | `				}` |
|    227698 |  643 | `			}` |
|    455401 |  644 | `			break;` |
|   2923306 |  645 | `		case '-':` |
|   5846617 |  646 | `			if( pStream->zText < pStream->zEnd ){` |
|   5846617 |  647 | `				if( pStream->zText[0] == '-' ){` |
|         - |  648 | `					/* Current operator: -- */` |
|     31095 |  649 | `					pStream->zText++;` |
|   5831072 |  650 | `				}else if( pStream->zText[0] == '=' ){` |
|         - |  651 | `					/* Current operator: -= */` |
|        14 |  652 | `					pStream->zText++;` |
|   5815521 |  653 | `				}else if( pStream->zText[0] == '>' ){` |
|         - |  654 | `					/* Current operator: -> */` |
|   5573949 |  655 | `					pStream->zText++;` |
|   2786972 |  656 | `				}` |
|   2923306 |  657 | `			}` |
|   5846617 |  658 | `			break;` |
|     19626 |  659 | `		case '*':` |
|     39257 |  660 | `			if( pStream->zText < pStream->zEnd ){` |
|     39257 |  661 | `				if( pStream->zText[0] == '*' ){` |
|         - |  662 | `					/* Current operator: ** or **= */` |
|       137 |  663 | `					pStream->zText++;` |
|       137 |  664 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  665 | `						/* Current operator: **= */` |
|        23 |  666 | `						pStream->zText++;` |
|        12 |  667 | `					}` |
|     39189 |  668 | `				}else if( pStream->zText[0] == '=' ){` |
|         - |  669 | `					/* Current operator: *= */` |
|        27 |  670 | `					pStream->zText++;` |
|        12 |  671 | `				}` |
|     19626 |  672 | `			}` |
|     39257 |  673 | `			break;` |
|      1991 |  674 | `		case '/':` |
|      3987 |  675 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  676 | `				/* Current operator: /= */` |
|         9 |  677 | `				pStream->zText++;` |
|         4 |  678 | `			}` |
|      3987 |  679 | `			break;` |
|     17519 |  680 | `		case '%':` |
|     35043 |  681 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  682 | `				/* Current operator: %= */` |
|         9 |  683 | `				pStream->zText++;` |
|         4 |  684 | `			}` |
|     35043 |  685 | `			break;` |
|        11 |  686 | `		case '^':` |
|        23 |  687 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  688 | `				/* Current operator: ^= */` |
|         9 |  689 | `				pStream->zText++;` |
|         4 |  690 | `			}` |
|        23 |  691 | `			break;` |
|    998066 |  692 | `		case '.':` |
|   1996137 |  693 | `			if( pStream->zText + 1 < pStream->zEnd && pStream->zText[0] == '.' && pStream->zText[1] == '.' ){` |
|         - |  694 | `				/* Ellipsis: ... */` |
|     27645 |  695 | `				pStream->zText += 2;` |
|     27645 |  696 | `				pToken->nType = PH7_TK_ELLIPSIS;` |
|   1982317 |  697 | `			}else if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  698 | `				/* Current operator: .= */` |
|    337915 |  699 | `				pStream->zText++;` |
|    168955 |  700 | `			}` |
|   1996137 |  701 | `			break;` |
|    200275 |  702 | `		case '<':` |
|    400555 |  703 | `			if( pStream->zText < pStream->zEnd ){` |
|    400555 |  704 | `				if( pStream->zText[0] == '<' ){` |
|         - |  705 | `					/* Current operator: << */` |
|       151 |  706 | `					pStream->zText++;` |
|       151 |  707 | `					if( pStream->zText < pStream->zEnd ){` |
|       151 |  708 | `						if( pStream->zText[0] == '=' ){` |
|         - |  709 | `							/* Current operator: <<= */` |
|         9 |  710 | `							pStream->zText++;` |
|       147 |  711 | `						}else if( pStream->zText[0] == '<' ){` |
|         - |  712 | `							/* Current Token: <<<  */` |
|       129 |  713 | `							pStream->zText++;` |
|         - |  714 | `							/* This may be the beginning of a Heredoc/Nowdoc string,try to delimit it */` |
|       129 |  715 | `							rc = LexExtractHeredoc(&(*pStream),&(*pToken));` |
|       129 |  716 | `							if( rc == SXRET_OK ){` |
|         - |  717 | `								/* Here/Now doc successfuly extracted */` |
|       129 |  718 | `								return SXRET_OK;` |
|         - |  719 | `							}` |
|       ! 0 |  720 | `						}` |
|        12 |  721 | `					}` |
|    400420 |  722 | `				}else if( pStream->zText[0] == '>' ){` |
|         - |  723 | `					/* Current operator: <> */` |
|         5 |  724 | `					pStream->zText++;` |
|    400407 |  725 | `				}else if( pStream->zText[0] == '=' ){` |
|         - |  726 | `					/* Current operator: <= or <=> */` |
|     66155 |  727 | `					pStream->zText++;` |
|     66155 |  728 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '>' ){` |
|         - |  729 | `						/* Current operator: <=> */` |
|     27267 |  730 | `						pStream->zText++;` |
|     13631 |  731 | `					}` |
|     33075 |  732 | `				}` |
|    200213 |  733 | `			}` |
|    400431 |  734 | `			break;` |
|    157374 |  735 | `		case '>':` |
|    314753 |  736 | `			if( pStream->zText < pStream->zEnd ){` |
|    314753 |  737 | `				if( pStream->zText[0] == '>' ){` |
|         - |  738 | `					/* Current operator: >> */` |
|     19435 |  739 | `					pStream->zText++;` |
|     19435 |  740 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  741 | `						/* Current operator: >>= */` |
|        11 |  742 | `						pStream->zText++;` |
|        10 |  743 | `					}` |
|    305038 |  744 | `				}else if( pStream->zText[0] == '=' ){` |
|         - |  745 | `					/* Current operator: >= */` |
|    104911 |  746 | `					pStream->zText++;` |
|     52453 |  747 | `				}` |
|    157374 |  748 | `			}` |
|    314753 |  749 | `			break;` |
|    291099 |  750 | `		case '?':` |
|    582203 |  751 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '?' ){` |
|         - |  752 | `				/* Null coalescing operator: ?? */` |
|     58517 |  753 | `				pStream->zText++;` |
|     58517 |  754 | `				if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  755 | `					/* Null coalescing assignment operator (PHP 7.4) */` |
|       101 |  756 | `					pStream->zText++;` |
|        48 |  757 | `				}` |
|    552947 |  758 | `			}else if( (pStream->zEnd - pStream->zText) >= 2` |
|    523691 |  759 | `				&& pStream->zText[0] == '-' && pStream->zText[1] == '>' ){` |
|         - |  760 | `				/* Nullsafe object operator (PHP 8.0): ?-> */` |
|       119 |  761 | `				pStream->zText += 2;` |
|        57 |  762 | `			}` |
|    582198 |  763 | `			break;` |
|      7903 |  764 | `		default:` |
|     15806 |  765 | `			break;` |
|         - |  766 | `		}` |
| 113156869 |  767 | `		if( pStr->nByte <= 0 ){` |
|         - |  768 | `			/* Record token length */` |
| 113156797 |  769 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  56578396 |  770 | `		}` |
| 113156869 |  771 | `		if( pToken->nType & PH7_TK_OP ){` |
|         - |  772 | `			const ph7_expr_op *pOp;` |
|         - |  773 | `			/* Check if the extracted token is an operator */` |
|  27401987 |  774 | `			pOp = PH7_ExprExtractOperator(pStr,(SyToken *)SySetPeek(pStream->pSet));` |
|  27401987 |  775 | `			if( pOp == 0 ){` |
|         - |  776 | `				/* Not an operator */` |
|       ! 0 |  777 | `				pToken->nType &= ~PH7_TK_OP;` |
|       ! 0 |  778 | `				if( pToken->nType <= 0 ){` |
|       ! 0 |  779 | `					pToken->nType = PH7_TK_OTHER;` |
|       ! 0 |  780 | `				}` |
|       ! 0 |  781 | `			}else{` |
|         - |  782 | `				/* Save the instance associated with this operator for later processing */` |
|  27401987 |  783 | `				pToken->pUserData = (void *)pOp;` |
|         - |  784 | `			}` |
|  13700991 |  785 | `		}` |
|         - |  786 | `	}` |
|         - |  787 | `	/* Tell the upper-layer to save the extracted token for later processing */` |
| 175823155 |  788 | `	return SXRET_OK;` |
|  93167722 |  789 | `}` |
|         - |  790 | `/* SPDX-SnippetBegin */` |
|         - |  791 | `/* SPDX-SnippetCopyrightText: SQLite mkkeywordhash.c (D. Richard Hipp and the SQLite authors <https://sqlite.org/>); adapted for the PH7 engine by Chems mrad */` |
|         - |  792 | `/* SPDX-License-Identifier: blessing */` |
|         - |  793 | `/***** This file contains automatically generated code ******` |
|         - |  794 | `**` |
|         - |  795 | `** The code in this file has been automatically generated by` |
|         - |  796 | `**` |
|         - |  797 | `**     $Header: /sqlite/sqlite/tool/mkkeywordhash.c` |
|         - |  798 | `**` |
|         - |  799 | `** Sligthly modified by Chems mrad <chm@symisc.net> for the PH7 engine.` |
|         - |  800 | `**` |
|         - |  801 | `** The code in this file implements a function that determines whether` |
|         - |  802 | `** or not a given identifier is really a PHP keyword.  The same thing` |
|         - |  803 | `** might be implemented more directly using a hand-written hash table.` |
|         - |  804 | `** But by using this automatically generated code, the size of the code` |
|         - |  805 | `** is substantially reduced.  This is important for embedded applications` |
|         - |  806 | `** on platforms with limited memory.` |
|         - |  807 | `*/` |
|         - |  808 | `/* Hash score: 103 */` |
|  62666291 |  809 | `static sxu32 KeywordCode(const char *z, int n){` |
|         - |  810 | `  /* zText[] encodes 532 bytes of keywords in 333 bytes */` |
|         - |  811 | `  /*   extendswitchprintegerequire_oncenddeclareturnamespacechobject      */` |
|         - |  812 | `  /*   hrowbooleandefaultrycaselfinalistaticlonewconstringlobaluse        */` |
|         - |  813 | `  /*   lseifloatvarrayANDIEchoUSECHOabstractclasscontinuendifunction      */` |
|         - |  814 | `  /*   diendwhilevaldoexitgotoimplementsinclude_oncemptyinstanceof        */` |
|         - |  815 | `  /*   interfacendforeachissetparentprivateprotectedpublicatchunset       */` |
|         - |  816 | `  /*   xorARRAYASArrayEXITUNSETXORbreak                                   */` |
|         - |  817 | `  static const char zText[332] = {` |
|         - |  818 | `    'e','x','t','e','n','d','s','w','i','t','c','h','p','r','i','n','t','e',` |
|         - |  819 | `    'g','e','r','e','q','u','i','r','e','_','o','n','c','e','n','d','d','e',` |
|         - |  820 | `    'c','l','a','r','e','t','u','r','n','a','m','e','s','p','a','c','e','c',` |
|         - |  821 | `    'h','o','b','j','e','c','t','h','r','o','w','b','o','o','l','e','a','n',` |
|         - |  822 | `    'd','e','f','a','u','l','t','r','y','c','a','s','e','l','f','i','n','a',` |
|         - |  823 | `    'l','i','s','t','a','t','i','c','l','o','n','e','w','c','o','n','s','t',` |
|         - |  824 | `    'r','i','n','g','l','o','b','a','l','u','s','e','l','s','e','i','f','l',` |
|         - |  825 | `    'o','a','t','v','a','r','r','a','y','A','N','D','I','E','c','h','o','U',` |
|         - |  826 | `    'S','E','C','H','O','a','b','s','t','r','a','c','t','c','l','a','s','s',` |
|         - |  827 | `    'c','o','n','t','i','n','u','e','n','d','i','f','u','n','c','t','i','o',` |
|         - |  828 | `    'n','d','i','e','n','d','w','h','i','l','e','v','a','l','d','o','e','x',` |
|         - |  829 | `    'i','t','g','o','t','o','i','m','p','l','e','m','e','n','t','s','i','n',` |
|         - |  830 | `    'c','l','u','d','e','_','o','n','c','e','m','p','t','y','i','n','s','t',` |
|         - |  831 | `    'a','n','c','e','o','f','i','n','t','e','r','f','a','c','e','n','d','f',` |
|         - |  832 | `    'o','r','e','a','c','h','i','s','s','e','t','p','a','r','e','n','t','p',` |
|         - |  833 | `    'r','i','v','a','t','e','p','r','o','t','e','c','t','e','d','p','u','b',` |
|         - |  834 | `    'l','i','c','a','t','c','h','u','n','s','e','t','x','o','r','A','R','R',` |
|         - |  835 | `    'A','Y','A','S','A','r','r','a','y','E','X','I','T','U','N','S','E','T',` |
|         - |  836 | `    'X','O','R','b','r','e','a','k'` |
|         - |  837 | `  };` |
|         - |  838 | `  static const unsigned char aHash[151] = {` |
|         - |  839 | `       0,   0,   4,  83,   0,  61,  39,  12,   0,  33,  77,   0,  48,` |
|         - |  840 | `       0,   2,  65,  67,   0,   0,   0,  47,   0,   0,  40,   0,  15,` |
|         - |  841 | `      74,   0,  51,   0,  76,   0,   0,  20,   0,   0,   0,  50,   0,` |
|         - |  842 | `      80,  34,   0,  36,   0,   0,  64,  16,   0,   0,  17,   0,   1,` |
|         - |  843 | `      19,  84,  66,   0,  43,  45,  78,   0,   0,  53,  56,   0,   0,` |
|         - |  844 | `       0,  23,  49,   0,   0,  13,  31,  54,   7,   0,   0,  25,   0,` |
|         - |  845 | `      72,  14,   0,  71,   0,  38,   6,   0,   0,   0,  73,   0,   0,` |
|         - |  846 | `       3,   0,  41,   5,  52,  57,  32,   0,  60,  63,   0,  69,  82,` |
|         - |  847 | `      30,   0,  79,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,` |
|         - |  848 | `       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  81,   0,   0,` |
|         - |  849 | `      62,   0,  11,   0,   0,  58,   0,   0,   0,   0,  59,  75,   0,` |
|         - |  850 | `       0,   0,   0,   0,   0,  35,  27,   0` |
|         - |  851 | `  };` |
|         - |  852 | `  static const unsigned char aNext[84] = {` |
|         - |  853 | `       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,` |
|         - |  854 | `       0,   0,   8,   0,   0,   0,  10,   0,   0,   0,   0,   0,   0,` |
|         - |  855 | `       0,   0,   0,   0,  28,   0,   0,   0,   0,   0,   0,   0,   0,` |
|         - |  856 | `       0,   0,   0,   0,   0,  44,   0,  18,   0,   0,   0,   0,   0,` |
|         - |  857 | `       0,  46,   0,  29,   0,   0,   0,  22,   0,   0,   0,   0,  26,` |
|         - |  858 | `       0,  21,  24,   0,   0,  68,   0,   0,   9,  37,   0,   0,   0,` |
|         - |  859 | `      42,   0,   0,   0,  70,  55` |
|         - |  860 | `  };` |
|         - |  861 | `  static const unsigned char aLen[84] = {` |
|         - |  862 | `       7,   9,   6,   5,   7,  12,   7,   2,  10,   7,   6,   9,   4,` |
|         - |  863 | `       6,   5,   7,   4,   3,   7,   3,   4,   4,   5,   4,   6,   5,` |
|         - |  864 | `       2,   3,   5,   6,   6,   3,   6,   4,   2,   5,   3,   5,   3,` |
|         - |  865 | `       3,   4,   3,   4,   8,   5,   2,   8,   5,   8,   3,   8,   5,` |
|         - |  866 | `       4,   2,   4,   4,  10,  12,   7,   5,  10,   9,   3,   6,  10,` |
|         - |  867 | `       3,   7,   2,   5,   6,   7,   9,   6,   5,   5,   3,   5,   2,` |
|         - |  868 | `       5,   4,   5,   3,   2,   5` |
|         - |  869 | `  };` |
|         - |  870 | `  static const sxu16 aOffset[84] = {` |
|         - |  871 | `       0,   3,   6,  12,  14,  20,  20,  21,  31,  34,  39,  44,  52,` |
|         - |  872 | `      55,  60,  65,  65,  70,  72,  78,  81,  83,  86,  90,  92,  97,` |
|         - |  873 | `     100, 100, 103, 106, 111, 117, 119, 119, 123, 124, 129, 130, 135,` |
|         - |  874 | `     137, 139, 143, 145, 149, 157, 159, 162, 169, 173, 181, 183, 186,` |
|         - |  875 | `     190, 194, 196, 200, 204, 214, 214, 225, 230, 240, 240, 248, 248,` |
|         - |  876 | `     251, 251, 252, 258, 263, 269, 276, 285, 290, 295, 300, 303, 308,` |
|         - |  877 | `     310, 315, 319, 324, 325, 327` |
|         - |  878 | `  };` |
|         - |  879 | `  static const sxu32 aCode[84] = {` |
|         - |  880 | `    PH7_TKWRD_EXTENDS,   PH7_TKWRD_ENDSWITCH,   PH7_TKWRD_SWITCH,    PH7_TKWRD_PRINT,   PH7_TKWRD_INT,` |
|         - |  881 | `    PH7_TKWRD_REQONCE,   PH7_TKWRD_REQUIRE,     PH7_TK_ID /* 'eq' PH7-ism removed */, PH7_TKWRD_ENDDEC, PH7_TKWRD_DECLARE,` |
|         - |  882 | `    PH7_TKWRD_RETURN,    PH7_TKWRD_NAMESPACE,   PH7_TKWRD_ECHO,      PH7_TKWRD_OBJECT,    PH7_TKWRD_THROW,` |
|         - |  883 | `    PH7_TKWRD_BOOL,      PH7_TKWRD_BOOL,        PH7_TKWRD_AND,       PH7_TKWRD_DEFAULT,   PH7_TKWRD_TRY,` |
|         - |  884 | `    PH7_TKWRD_CASE,      PH7_TKWRD_SELF,        PH7_TKWRD_FINAL,     PH7_TKWRD_LIST,      PH7_TKWRD_STATIC,` |
|         - |  885 | `    PH7_TKWRD_CLONE,     PH7_TK_ID /* 'ne' PH7-ism removed */, PH7_TKWRD_NEW,  PH7_TKWRD_CONST,     PH7_TKWRD_STRING,` |
|         - |  886 | `    PH7_TKWRD_GLOBAL,    PH7_TKWRD_USE,         PH7_TKWRD_ELIF,      PH7_TKWRD_ELSE,      PH7_TKWRD_IF,` |
|         - |  887 | `    PH7_TKWRD_FLOAT,     PH7_TKWRD_VAR,         PH7_TKWRD_ARRAY,     PH7_TKWRD_AND,       PH7_TKWRD_DIE,` |
|         - |  888 | `    PH7_TKWRD_ECHO,      PH7_TKWRD_USE,         PH7_TKWRD_ECHO,      PH7_TKWRD_ABSTRACT,  PH7_TKWRD_CLASS,` |
|         - |  889 | `    PH7_TKWRD_AS,        PH7_TKWRD_CONTINUE,    PH7_TKWRD_ENDIF,     PH7_TKWRD_FUNCTION,  PH7_TKWRD_DIE,` |
|         - |  890 | `    PH7_TKWRD_ENDWHILE,  PH7_TKWRD_WHILE,       PH7_TKWRD_EVAL,      PH7_TKWRD_DO,        PH7_TKWRD_EXIT,` |
|         - |  891 | `    PH7_TKWRD_GOTO,      PH7_TKWRD_IMPLEMENTS,  PH7_TKWRD_INCONCE,   PH7_TKWRD_INCLUDE,   PH7_TKWRD_EMPTY,` |
|         - |  892 | `    PH7_TKWRD_INSTANCEOF,PH7_TKWRD_INTERFACE,   PH7_TKWRD_INT,       PH7_TKWRD_ENDFOR,    PH7_TKWRD_END4EACH,` |
|         - |  893 | `    PH7_TKWRD_FOR,       PH7_TKWRD_FOREACH,     PH7_TKWRD_OR,        PH7_TKWRD_ISSET,     PH7_TKWRD_PARENT,` |
|         - |  894 | `    PH7_TKWRD_PRIVATE,   PH7_TKWRD_PROTECTED,   PH7_TKWRD_PUBLIC,    PH7_TKWRD_CATCH,     PH7_TKWRD_UNSET,` |
|         - |  895 | `    PH7_TKWRD_XOR,       PH7_TKWRD_ARRAY,       PH7_TKWRD_AS,        PH7_TKWRD_ARRAY,     PH7_TKWRD_EXIT,` |
|         - |  896 | `    PH7_TKWRD_UNSET,     PH7_TKWRD_XOR,         PH7_TKWRD_OR,        PH7_TKWRD_BREAK` |
|         - |  897 | `  };` |
|         - |  898 | `  int h, i;` |
|  62666291 |  899 | `  if( n<2 ) return PH7_TK_ID;` |
|         - |  900 | ``  /* Hash through UNSIGNED bytes: `char` is signed on most targets, so an`` |
|         - |  901 | `   * identifier carrying a high byte (php allows 0x80-0xFF in identifiers, and` |
|         - |  902 | ``   * every UTF-8 name has them) made the xor negative, and C's `%` keeps that`` |
|         - |  903 | `   * sign — aHash[-46] read off the front of the table. ASCII is unaffected, so` |
|         - |  904 | `   * the generated keyword buckets still resolve exactly as before. */` |
|  55218945 |  905 | `  h = (int)(((sxu32)(sxu8)z[0]*4) ^ ((sxu32)(sxu8)z[n-1]*3) ^ (sxu32)n) % 151;` |
|  85081751 |  906 | `  for(i=((int)aHash[h])-1; i>=0; i=((int)aNext[i])-1){` |
|  50601657 |  907 | `    if( (int)aLen[i]==n && SyMemcmp(&zText[aOffset[i]],z,n)==0 ){` |
|         - |  908 | `       /* PH7_TKWRD_EXTENDS */` |
|         - |  909 | `       /* PH7_TKWRD_ENDSWITCH */` |
|         - |  910 | `       /* PH7_TKWRD_SWITCH */` |
|         - |  911 | `       /* PH7_TKWRD_PRINT */` |
|         - |  912 | `       /* PH7_TKWRD_INT */` |
|         - |  913 | `       /* PH7_TKWRD_REQONCE */` |
|         - |  914 | `       /* PH7_TKWRD_REQUIRE */` |
|         - |  915 | `       /* PH7_TK_ID */` |
|         - |  916 | `       /* PH7_TKWRD_ENDDEC */` |
|         - |  917 | `       /* PH7_TKWRD_DECLARE */` |
|         - |  918 | `       /* PH7_TKWRD_RETURN */` |
|         - |  919 | `       /* PH7_TKWRD_NAMESPACE */` |
|         - |  920 | `       /* PH7_TKWRD_ECHO */` |
|         - |  921 | `       /* PH7_TKWRD_OBJECT */` |
|         - |  922 | `       /* PH7_TKWRD_THROW */` |
|         - |  923 | `       /* PH7_TKWRD_BOOL */` |
|         - |  924 | `       /* PH7_TKWRD_BOOL */` |
|         - |  925 | `       /* PH7_TKWRD_AND */` |
|         - |  926 | `       /* PH7_TKWRD_DEFAULT */` |
|         - |  927 | `       /* PH7_TKWRD_TRY */` |
|         - |  928 | `       /* PH7_TKWRD_CASE */` |
|         - |  929 | `       /* PH7_TKWRD_SELF */` |
|         - |  930 | `       /* PH7_TKWRD_FINAL */` |
|         - |  931 | `       /* PH7_TKWRD_LIST */` |
|         - |  932 | `       /* PH7_TKWRD_STATIC */` |
|         - |  933 | `       /* PH7_TKWRD_CLONE */` |
|         - |  934 | `       /* PH7_TK_ID */` |
|         - |  935 | `       /* PH7_TKWRD_NEW */` |
|         - |  936 | `       /* PH7_TKWRD_CONST */` |
|         - |  937 | `       /* PH7_TKWRD_STRING */` |
|         - |  938 | `       /* PH7_TKWRD_GLOBAL */` |
|         - |  939 | `       /* PH7_TKWRD_USE */` |
|         - |  940 | `       /* PH7_TKWRD_ELIF */` |
|         - |  941 | `       /* PH7_TKWRD_ELSE */` |
|         - |  942 | `       /* PH7_TKWRD_IF */` |
|         - |  943 | `       /* PH7_TKWRD_FLOAT */` |
|         - |  944 | `       /* PH7_TKWRD_VAR */` |
|         - |  945 | `       /* PH7_TKWRD_ARRAY */` |
|         - |  946 | `       /* PH7_TKWRD_AND */` |
|         - |  947 | `       /* PH7_TKWRD_DIE */` |
|         - |  948 | `       /* PH7_TKWRD_ECHO */` |
|         - |  949 | `       /* PH7_TKWRD_USE */` |
|         - |  950 | `       /* PH7_TKWRD_ECHO */` |
|         - |  951 | `       /* PH7_TKWRD_ABSTRACT */` |
|         - |  952 | `       /* PH7_TKWRD_CLASS */` |
|         - |  953 | `       /* PH7_TKWRD_AS */` |
|         - |  954 | `       /* PH7_TKWRD_CONTINUE */` |
|         - |  955 | `       /* PH7_TKWRD_ENDIF */` |
|         - |  956 | `       /* PH7_TKWRD_FUNCTION */` |
|         - |  957 | `       /* PH7_TKWRD_DIE */` |
|         - |  958 | `       /* PH7_TKWRD_ENDWHILE */` |
|         - |  959 | `       /* PH7_TKWRD_WHILE */` |
|         - |  960 | `       /* PH7_TKWRD_EVAL */` |
|         - |  961 | `       /* PH7_TKWRD_DO */` |
|         - |  962 | `       /* PH7_TKWRD_EXIT */` |
|         - |  963 | `       /* PH7_TKWRD_GOTO */` |
|         - |  964 | `       /* PH7_TKWRD_IMPLEMENTS */` |
|         - |  965 | `       /* PH7_TKWRD_INCONCE */` |
|         - |  966 | `       /* PH7_TKWRD_INCLUDE */` |
|         - |  967 | `       /* PH7_TKWRD_EMPTY */` |
|         - |  968 | `       /* PH7_TKWRD_INSTANCEOF */` |
|         - |  969 | `       /* PH7_TKWRD_INTERFACE */` |
|         - |  970 | `       /* PH7_TKWRD_INT */` |
|         - |  971 | `       /* PH7_TKWRD_ENDFOR */` |
|         - |  972 | `       /* PH7_TKWRD_END4EACH */` |
|         - |  973 | `       /* PH7_TKWRD_FOR */` |
|         - |  974 | `       /* PH7_TKWRD_FOREACH */` |
|         - |  975 | `       /* PH7_TKWRD_OR */` |
|         - |  976 | `       /* PH7_TKWRD_ISSET */` |
|         - |  977 | `       /* PH7_TKWRD_PARENT */` |
|         - |  978 | `       /* PH7_TKWRD_PRIVATE */` |
|         - |  979 | `       /* PH7_TKWRD_PROTECTED */` |
|         - |  980 | `       /* PH7_TKWRD_PUBLIC */` |
|         - |  981 | `       /* PH7_TKWRD_CATCH */` |
|         - |  982 | `       /* PH7_TKWRD_UNSET */` |
|         - |  983 | `       /* PH7_TKWRD_XOR */` |
|         - |  984 | `       /* PH7_TKWRD_ARRAY */` |
|         - |  985 | `       /* PH7_TKWRD_AS */` |
|         - |  986 | `       /* PH7_TKWRD_ARRAY */` |
|         - |  987 | `       /* PH7_TKWRD_EXIT */` |
|         - |  988 | `       /* PH7_TKWRD_UNSET */` |
|         - |  989 | `       /* PH7_TKWRD_XOR */` |
|         - |  990 | `       /* PH7_TKWRD_OR */` |
|         - |  991 | `       /* PH7_TKWRD_BREAK */` |
|  20738851 |  992 | `      return aCode[i];` |
|         - |  993 | `    }` |
|  14931408 |  994 | `  }` |
|         - |  995 | `  /* Linear fallback for keywords not in the auto-generated hash table */` |
|  34480099 |  996 | `  if( n==5 && SyMemcmp(z,"trait",5)==0 ) return PH7_TKWRD_TRAIT;` |
|  34472233 |  997 | `  if( n==9 && SyMemcmp(z,"insteadof",9)==0 ) return PH7_TKWRD_INSTEADOF;` |
|  34472229 |  998 | `  if( n==7 && SyMemcmp(z,"finally",7)==0 ) return PH7_TKWRD_FINALLY;` |
|  34472057 |  999 | `  if( n==5 && SyMemcmp(z,"yield",5)==0 ) return PH7_TKWRD_YIELD;` |
|  34456137 | 1000 | `  if( n==5 && SyMemcmp(z,"match",5)==0 ) return PH7_TKWRD_MATCH;` |
|  34456059 | 1001 | `  return PH7_TK_ID;` |
|  31333148 | 1002 | `}` |
|         - | 1003 | `/* --- End of Automatically generated code --- */` |
|         - | 1004 | `/* SPDX-SnippetEnd */` |
|         - | 1005 | `/*` |
|         - | 1006 | ` * Extract a heredoc/nowdoc text from a raw PHP input.` |
|         - | 1007 | ` * According to the PHP language reference manual:` |
|         - | 1008 | ` *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier` |
|         - | 1009 | ` *  is provided, then a newline. The string itself follows, and then the same identifier again` |
|         - | 1010 | ` *  to close the quotation.` |
|         - | 1011 | ` *  The closing identifier must begin in the first column of the line. Also, the identifier must` |
|         - | 1012 | ` *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric` |
|         - | 1013 | ` *  characters and underscores, and must start with a non-digit character or underscore.` |
|         - | 1014 | ` *  Heredoc text behaves just like a double-quoted string, without the double quotes.` |
|         - | 1015 | ` *  This means that quotes in a heredoc do not need to be escaped, but the escape codes listed` |
|         - | 1016 | ` *  above can still be used. Variables are expanded, but the same care must be taken when expressing` |
|         - | 1017 | ` *  complex variables inside a heredoc as with strings.` |
|         - | 1018 | ` *  Nowdocs are to single-quoted strings what heredocs are to double-quoted strings.` |
|         - | 1019 | ` *  A nowdoc is specified similarly to a heredoc, but no parsing is done inside a nowdoc.` |
|         - | 1020 | ` *  The construct is ideal for embedding PHP code or other large blocks of text without the need` |
|         - | 1021 | ` *  for escaping. It shares some features in common with the SGML <![CDATA[ ]]> construct, in that` |
|         - | 1022 | ` *  it declares a block of text which is not for parsing.` |
|         - | 1023 | ` *  A nowdoc is identified with the same <<< sequence used for heredocs, but the identifier which follows` |
|         - | 1024 | ` *  is enclosed in single quotes, e.g. <<<'EOT'. All the rules for heredoc identifiers also apply to nowdoc` |
|         - | 1025 | ` *  identifiers, especially those regarding the appearance of the closing identifier.` |
|         - | 1026 | ` * Symisc Extension:` |
|         - | 1027 | ` * The closing delimiter can now start with a digit or undersocre or it can be an UTF-8 stream.` |
|         - | 1028 | ` * Example:` |
|         - | 1029 | ` *  <<<123` |
|         - | 1030 | ` *    HEREDOC Here` |
|         - | 1031 | ` * 123` |
|         - | 1032 | ` *  or` |
|         - | 1033 | ` *  <<<___` |
|         - | 1034 | ` *   HEREDOC Here` |
|         - | 1035 | ` *  ___` |
|         - | 1036 | ` */` |
|       124 | 1037 | `static sxi32 LexExtractHeredoc(SyStream *pStream,SyToken *pToken)` |
|         5 | 1038 | `{` |
|       129 | 1039 | `	const unsigned char *zIn  = pStream->zText;` |
|       129 | 1040 | `	const unsigned char *zEnd = pStream->zEnd;` |
|         - | 1041 | `	const unsigned char *zPtr;` |
|       129 | 1042 | `	sxu8 bNowDoc = FALSE;` |
|         - | 1043 | `	SyString sDelim;` |
|         - | 1044 | `	SyString sStr;` |
|         - | 1045 | `	/* Jump leading white spaces */` |
|       141 | 1046 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|        13 | 1047 | `		zIn++;` |
|         1 | 1048 | `	}` |
|       129 | 1049 | `	if( zIn >= zEnd ){` |
|         - | 1050 | `		/* A simple symbol,return immediately */` |
|       ! 0 | 1051 | `		return SXERR_CONTINUE;` |
|         - | 1052 | `	}` |
|       129 | 1053 | `	if( zIn[0] == '\'' \|\| zIn[0] == '"' ){` |
|         - | 1054 | `		/* Make sure we are dealing with a nowdoc */` |
|        55 | 1055 | `		bNowDoc =  zIn[0] == '\'' ? TRUE : FALSE;` |
|        55 | 1056 | `		zIn++;` |
|        26 | 1057 | `	}` |
|       129 | 1058 | `	if( zIn[0] < 0xc0 && !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|         - | 1059 | `		/* Invalid delimiter,return immediately */` |
|       ! 0 | 1060 | `		return SXERR_CONTINUE;` |
|         - | 1061 | `	}` |
|         - | 1062 | `	/* Isolate the identifier */` |
|       129 | 1063 | `	sDelim.zString = (const char *)zIn;` |
|       132 | 1064 | `	for(;;){` |
|       269 | 1065 | `		zPtr = zIn;` |
|         - | 1066 | `		/* Skip alphanumeric stream */` |
|       843 | 1067 | `		while( zPtr < zEnd && zPtr[0] < 0xc0 && (SyisAlphaNum(zPtr[0]) \|\| zPtr[0] == '_') ){` |
|       447 | 1068 | `			zPtr++;` |
|         5 | 1069 | `		}` |
|       269 | 1070 | `		if( zPtr < zEnd && zPtr[0] >= 0xc0 ){` |
|        19 | 1071 | `			zPtr++;` |
|         - | 1072 | `			/* UTF-8 stream */` |
|        37 | 1073 | `			while( zPtr < zEnd && ((zPtr[0] & 0xc0) == 0x80) ){` |
|        19 | 1074 | `				zPtr++;` |
|         1 | 1075 | `			}` |
|         9 | 1076 | `		}` |
|       269 | 1077 | `		if( zPtr == zIn ){` |
|         - | 1078 | `			/* Not an UTF-8 or alphanumeric stream */` |
|       129 | 1079 | `			break;` |
|         - | 1080 | `		}` |
|         - | 1081 | `		/* Synchronize pointers */` |
|       145 | 1082 | `		zIn = zPtr;` |
|         5 | 1083 | `	}` |
|         - | 1084 | `	/* Get the identifier length */` |
|       129 | 1085 | `	sDelim.nByte = (sxu32)((const char *)zIn-sDelim.zString);` |
|       129 | 1086 | `	if( zIn[0] == '"' \|\| (bNowDoc && zIn[0] == '\'') ){` |
|         - | 1087 | `		/* Jump the trailing single quote */` |
|        55 | 1088 | `		zIn++;` |
|        26 | 1089 | `	}` |
|         - | 1090 | `	/* Jump trailing white spaces */` |
|       129 | 1091 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|       ! 0 | 1092 | `		zIn++;` |
|       ! 0 | 1093 | `	}` |
|       129 | 1094 | `	if( sDelim.nByte <= 0 \|\| zIn >= zEnd \|\| zIn[0] != '\n' ){` |
|         - | 1095 | `		/* Invalid syntax */` |
|       ! 0 | 1096 | `		return SXERR_CONTINUE;` |
|         - | 1097 | `	}` |
|       129 | 1098 | `	pStream->nLine++; /* Increment line counter */` |
|       129 | 1099 | `	zIn++;` |
|         - | 1100 | `	/* Isolate the delimited string */` |
|       129 | 1101 | `	sStr.zString = (const char *)zIn;` |
|         - | 1102 | `	/* PHP 7.3 flexible heredoc/nowdoc: the closing marker may be preceded` |
|         - | 1103 | `	 * by whitespace (spaces/tabs), and may be followed by any non-identifier` |
|         - | 1104 | `	 * character. The indent count is recorded in pToken->pUserData and the` |
|         - | 1105 | `	 * compile phase strips it from each body line. */` |
|         - | 1106 | `	{` |
|       129 | 1107 | `		const unsigned char *zMarkerLine = zIn; /* Start of marker's line (set on match) */` |
|       129 | 1108 | `		sxu32 nIndent = 0;` |
|       296 | 1109 | `		for(;;){` |
|       363 | 1110 | `			const unsigned char *zLineStart = zIn;` |
|         - | 1111 | `			/* Skip leading space/tab on this line */` |
|      1020 | 1112 | `			while( zIn < zEnd && (zIn[0] == ' ' \|\| zIn[0] == '\t') ){` |
|       483 | 1113 | `				zIn++;` |
|         5 | 1114 | `			}` |
|       358 | 1115 | `			if( (sxu32)(zEnd - zIn) >= sDelim.nByte` |
|       362 | 1116 | `				&& SyMemcmp((const void *)sDelim.zString,(const void *)zIn,sDelim.nByte) == 0 ){` |
|         - | 1117 | `				int bIdentCont;` |
|       127 | 1118 | `				zPtr = &zIn[sDelim.nByte];` |
|         - | 1119 | `				/* Disambiguate: next byte must not continue an identifier.` |
|         - | 1120 | `				 * A leading byte >= 0xc0 starts a multi-byte UTF-8 sequence,` |
|         - | 1121 | `				 * which PHP identifiers may contain, so treat it as ident. */` |
|       127 | 1122 | `				if( zPtr >= zEnd ){` |
|       ! 0 | 1123 | `					bIdentCont = 0;` |
|       127 | 1124 | `				}else if( zPtr[0] >= 0xc0 ){` |
|       ! 0 | 1125 | `					bIdentCont = 1;` |
|       ! 0 | 1126 | `				}else{` |
|       127 | 1127 | `					bIdentCont = (SyisAlphaNum(zPtr[0]) \|\| zPtr[0] == '_');` |
|         - | 1128 | `				}` |
|       127 | 1129 | `				if( !bIdentCont ){` |
|         - | 1130 | `					/* Closing marker found */` |
|       127 | 1131 | `					nIndent = (sxu32)(zIn - zLineStart);` |
|       127 | 1132 | `					zMarkerLine = zLineStart;` |
|       127 | 1133 | `					pStream->zText = zPtr; /* Cursor right after identifier */` |
|       127 | 1134 | `					break;` |
|         - | 1135 | `				}` |
|       ! 0 | 1136 | `			}` |
|         - | 1137 | `			/* Not the closing marker on this line; walk to next newline */` |
|      5249 | 1138 | `			while( zIn < zEnd && zIn[0] != '\n' ){` |
|      5013 | 1139 | `				zIn++;` |
|         5 | 1140 | `			}` |
|       241 | 1141 | `			if( zIn >= zEnd ){` |
|         - | 1142 | `				/* End of input without finding the closing marker */` |
|         3 | 1143 | `				pStream->zText = pStream->zEnd;` |
|         3 | 1144 | `				zMarkerLine = zIn;` |
|         3 | 1145 | `				break;` |
|         - | 1146 | `			}` |
|       239 | 1147 | `			pStream->nLine++;` |
|       239 | 1148 | `			zIn++;` |
|         5 | 1149 | `		}` |
|         - | 1150 | `		/* Body runs from sStr.zString up to just before the marker line */` |
|       129 | 1151 | `		sStr.nByte = (sxu32)((const char *)zMarkerLine - sStr.zString);` |
|       129 | 1152 | `		pToken->nType = bNowDoc ? PH7_TK_NOWDOC : PH7_TK_HEREDOC;` |
|       129 | 1153 | `		SyStringDupPtr(&pToken->sData,&sStr);` |
|         - | 1154 | `		/* Strip exactly one line terminator that precedes the marker's line. */` |
|       124 | 1155 | `		if( pToken->sData.nByte > 0` |
|       125 | 1156 | `			&& pToken->sData.zString[pToken->sData.nByte - 1] == '\n' ){` |
|       119 | 1157 | `			pToken->sData.nByte--;` |
|       114 | 1158 | `			if( pToken->sData.nByte > 0` |
|       119 | 1159 | `				&& pToken->sData.zString[pToken->sData.nByte - 1] == '\r' ){` |
|       ! 0 | 1160 | `				pToken->sData.nByte--;` |
|       ! 0 | 1161 | `			}` |
|        57 | 1162 | `		}` |
|       129 | 1163 | `		pToken->pUserData = SX_INT_TO_PTR(nIndent);` |
|         - | 1164 | `	}` |
|         - | 1165 | `	/* All done */` |
|       129 | 1166 | `	return SXRET_OK;` |
|        67 | 1167 | `}` |
|         - | 1168 | `/*` |
|         - | 1169 | ` * Tokenize a raw PHP input.` |
|         - | 1170 | ` * This is the public tokenizer called by most code generator routines.` |
|         - | 1171 | ` */` |
|    105636 | 1172 | `PH7_PRIVATE sxi32 PH7_TokenizePHP(const char *zInput,sxu32 nLen,sxu32 nLineStart,SySet *pOut,SySet *pTrivia)` |
|         5 | 1173 | `{` |
|         - | 1174 | `	SyLex sLexer;` |
|         - | 1175 | `	sxi32 rc;` |
|         - | 1176 | `	/* Defense-in-depth cap for internal tokenizer calls that bypass ph7_compile() */` |
|    105641 | 1177 | `	if( nLen > PH7_MAX_INPUT_SIZE ){` |
|       ! 0 | 1178 | `		return SXERR_LIMIT;` |
|         - | 1179 | `	}` |
|         - | 1180 | `	/* Initialize the lexer. pTrivia (may be NULL = discard) rides as the` |
|         - | 1181 | `	 * tokenizer callback's user data: doc-comments (and later attribute` |
|         - | 1182 | `	 * groups) are recorded there instead of entering the token stream. */` |
|    105641 | 1183 | `	rc = SyLexInit(&sLexer,&(*pOut),TokenizePHP,pTrivia);` |
|    105641 | 1184 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 1185 | `		return rc;` |
|         - | 1186 | `	}` |
|    105641 | 1187 | `	sLexer.sStream.nLine = nLineStart;` |
|         - | 1188 | `	/* Tokenize input */` |
|    105641 | 1189 | `	rc = SyLexTokenizeInput(&sLexer,zInput,nLen,0,0,0);` |
|         - | 1190 | `	/* Release the lexer */` |
|    105641 | 1191 | `	SyLexRelease(&sLexer);` |
|         - | 1192 | `	/* Tokenization result */` |
|    105641 | 1193 | `	return rc;` |
|     52823 | 1194 | `}` |
|         - | 1195 | `/*` |
|         - | 1196 | ` * High level public tokenizer.` |
|         - | 1197 | ` *  Tokenize the input into PHP tokens and raw tokens [i.e: HTML,XML,Raw text...].` |
|         - | 1198 | ` * According to the PHP language reference manual` |
|         - | 1199 | ` *   When PHP parses a file, it looks for opening and closing tags, which tell PHP` |
|         - | 1200 | ` *   to start and stop interpreting the code between them. Parsing in this manner allows` |
|         - | 1201 | ` *   PHP to be embedded in all sorts of different documents, as everything outside of a pair` |
|         - | 1202 | ` *   of opening and closing tags is ignored by the PHP parser. Most of the time you will see` |
|         - | 1203 | ` *   PHP embedded in HTML documents, as in this example.` |
|         - | 1204 | ` *   <?php echo 'While this is going to be parsed.'; ?>` |
|         - | 1205 | ` *   <p>This will also be ignored.</p>` |
|         - | 1206 | ` *   You can also use more advanced structures:` |
|         - | 1207 | ` *   Example #1 Advanced escaping` |
|         - | 1208 | ` * <?php` |
|         - | 1209 | ` * if ($expression) {` |
|         - | 1210 | ` *   ?>` |
|         - | 1211 | ` *   <strong>This is true.</strong>` |
|         - | 1212 | ` *   <?php` |
|         - | 1213 | ` * } else {` |
|         - | 1214 | ` *   ?>` |
|         - | 1215 | ` *   <strong>This is false.</strong>` |
|         - | 1216 | ` *   <?php` |
|         - | 1217 | ` * }` |
|         - | 1218 | ` * ?>` |
|         - | 1219 | ` * This works as expected, because when PHP hits the ?> closing tags, it simply starts outputting` |
|         - | 1220 | ` * whatever it finds (except for an immediately following newline - see instruction separation ) until it hits` |
|         - | 1221 | ` * another opening tag. The example given here is contrived, of course, but for outputting large blocks of text` |
|         - | 1222 | ` * dropping out of PHP parsing mode is generally more efficient than sending all of the text through echo() or print().` |
|         - | 1223 | ` * There are four different pairs of opening and closing tags which can be used in PHP. Three of those, <?php ?>` |
|         - | 1224 | ` * <script language="php"> </script>  and <? ?> are always available. The other two are short tags and ASP style` |
|         - | 1225 | ` * tags, and can be turned on and off from the php.ini configuration file. As such, while some people find short tags` |
|         - | 1226 | ` * and ASP style tags convenient, they are less portable, and generally not recommended.` |
|         - | 1227 | ` * Note:` |
|         - | 1228 | ` * Also note that if you are embedding PHP within XML or XHTML you will need to use the <?php ?> tags to remain` |
|         - | 1229 | ` * compliant with standards.` |
|         - | 1230 | ` * Example #2 PHP Opening and Closing Tags` |
|         - | 1231 | ` * 1.  <?php echo 'if you want to serve XHTML or XML documents, do it like this'; ?>` |
|         - | 1232 | ` * 2.  <script language="php">` |
|         - | 1233 | ` *       echo 'some editors (like FrontPage) don\'t` |
|         - | 1234 | ` *             like processing instructions';` |
|         - | 1235 | ` *   </script>` |
|         - | 1236 | ` *` |
|         - | 1237 | ` * 3.  <? echo 'this is the simplest, an SGML processing instruction'; ?>` |
|         - | 1238 | ` *   <?= expression ?> This is a shortcut for "<? echo expression ?>"` |
|         - | 1239 | ` */` |
|     13076 | 1240 | `PH7_PRIVATE sxi32 PH7_TokenizeRawText(const char *zInput,sxu32 nLen,SySet *pOut,sxu32 nBaseLine)` |
|         5 | 1241 | `{` |
|     13081 | 1242 | `	const char *zEnd = &zInput[nLen];` |
|     13081 | 1243 | `	const char *zIn  = zInput;` |
|         - | 1244 | `	const char *zCur,*zCurEnd;` |
|     13081 | 1245 | `	SyString sCtag = { 0, 0 };     /* Closing tag */` |
|         - | 1246 | `	SyToken sToken;` |
|         - | 1247 | `	SyString sDoc;` |
|         - | 1248 | `	sxu32 nLine;` |
|         - | 1249 | `	sxi32 iNest;` |
|         - | 1250 | `	sxi32 rc;` |
|         - | 1251 | `	/* Tokenize the input into PHP tokens and raw tokens. nBaseLine is normally 1,` |
|         - | 1252 | `	 * but 2 when a "#!" shebang line was stripped so error lines still match php. */` |
|     13081 | 1253 | `	nLine = nBaseLine;` |
|     13081 | 1254 | `	zCur = zCurEnd   = 0; /* Prevent compiler warning */` |
|     13081 | 1255 | `	sToken.pUserData = 0;` |
|     13081 | 1256 | `	iNest = 0;` |
|     13081 | 1257 | `	sDoc.nByte = 0;` |
|     13081 | 1258 | `	sDoc.zString = ""; /* cc warning */` |
|     13070 | 1259 | `	for(;;){` |
|     25977 | 1260 | `		if( zIn >= zEnd ){` |
|         - | 1261 | `			/* End of input reached */` |
|     12893 | 1262 | `			break;` |
|         - | 1263 | `		}` |
|     13089 | 1264 | `		sToken.nLine = nLine;` |
|     13089 | 1265 | `		zCur = zIn;` |
|     13089 | 1266 | `		zCurEnd = 0;` |
|     13429 | 1267 | `		while( zIn < zEnd ){` |
|     13241 | 1268 | `			 if( zIn[0] == '<' ){` |
|     12901 | 1269 | `				const char *zTmp = zIn; /* End of raw input marker */` |
|     12901 | 1270 | `				zIn++;` |
|     12901 | 1271 | `				if( zIn < zEnd ){` |
|     12901 | 1272 | `					if( zIn[0] == '?' ){` |
|     12901 | 1273 | `						zIn++;` |
|     12901 | 1274 | `						if( (sxu32)(zEnd - zIn) >= sizeof("php")-1 &&  SyStrnicmp(zIn,"php",sizeof("php")-1) == 0 ){` |
|         - | 1275 | `							/* opening tag: <?php */` |
|     12899 | 1276 | `							zIn += sizeof("php")-1;` |
|      6447 | 1277 | `						}` |
|         - | 1278 | `						/* Look for the closing tag '?>' */` |
|     12901 | 1279 | `						SyStringInitFromBuf(&sCtag,"?>",sizeof("?>")-1);` |
|     12901 | 1280 | `						zCurEnd = zTmp;` |
|     12901 | 1281 | `						break;` |
|         - | 1282 | `					}` |
|       ! 0 | 1283 | `				}` |
|       ! 0 | 1284 | `			}else{` |
|       344 | 1285 | `				if( zIn[0] == '\n' ){` |
|         7 | 1286 | `					nLine++;` |
|         3 | 1287 | `				}` |
|       344 | 1288 | `				zIn++;` |
|         - | 1289 | `			 }` |
|         4 | 1290 | `		} /* While(zIn < zEnd) */` |
|     13089 | 1291 | `		if( zCurEnd == 0 ){` |
|        24 | 1292 | `			zCurEnd = zIn;` |
|        10 | 1293 | `		}` |
|         - | 1294 | `		/* Save the raw token */` |
|     13089 | 1295 | `		SyStringInitFromBuf(&sToken.sData,zCur,zCurEnd - zCur);` |
|     13089 | 1296 | `		sToken.nType = PH7_TOKEN_RAW;` |
|     13089 | 1297 | `		rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|     13089 | 1298 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1299 | `			return rc;` |
|         - | 1300 | `		}` |
|     13089 | 1301 | `		if( zIn >= zEnd ){` |
|        24 | 1302 | `			break;` |
|         - | 1303 | `		}` |
|         - | 1304 | `		/* Ignore leading white space */` |
|     27525 | 1305 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|     14461 | 1306 | `			if( zIn[0] == '\n' ){` |
|     14247 | 1307 | `				nLine++;` |
|      7121 | 1308 | `			}` |
|     14461 | 1309 | `			zIn++;` |
|         5 | 1310 | `		}` |
|         - | 1311 | `		/* Delimit the PHP chunk */` |
|     13069 | 1312 | `		sToken.nLine = nLine;` |
|     13069 | 1313 | `		zCur = zIn;` |
|   1536697 | 1314 | `		while( (sxu32)(zEnd - zIn) >= sCtag.nByte ){` |
|         - | 1315 | `			const char *zPtr;` |
|   1530603 | 1316 | `			if( SyMemcmp(zIn,sCtag.zString,sCtag.nByte) == 0 && iNest < 1 ){` |
|      6807 | 1317 | `				break;` |
|         - | 1318 | `			}` |
|         - | 1319 | `			/* Line comment ('#' or '//', but not the '#[' attribute opener): php` |
|         - | 1320 | `			 * ends it at a newline OR at the closing tag, so a '?>' inside a line` |
|         - | 1321 | `			 * comment DOES close the PHP block. Skipping the comment here also` |
|         - | 1322 | `			 * stops the string skip below from treating a quote inside the` |
|         - | 1323 | `			 * comment as a string. Only outside a heredoc body (iNest < 1). */` |
|   1527859 | 1324 | `			if( iNest < 1 &&` |
|   1519106 | 1325 | `				( (zIn[0] == '#' && !(zIn+1 < zEnd && zIn[1] == '[')) \|\|` |
|   1519119 | 1326 | `				  (zIn[0] == '/' && zIn+1 < zEnd && zIn[1] == '/') ) ){` |
|      8121 | 1327 | `				zIn += (zIn[0] == '#') ? 1 : 2;` |
|    344509 | 1328 | `				while( zIn < zEnd && zIn[0] != '\n' ){` |
|    336390 | 1329 | `					if( (sxu32)(zEnd - zIn) >= sCtag.nByte` |
|    336392 | 1330 | `						&& SyMemcmp(zIn,sCtag.zString,sCtag.nByte) == 0 ){` |
|         3 | 1331 | `						break; /* the closing tag terminates the line comment */` |
|         - | 1332 | `					}` |
|    336393 | 1333 | `					zIn++;` |
|         5 | 1334 | `				}` |
|      7753 | 1335 | `				continue;` |
|         - | 1336 | `			}` |
|         - | 1337 | `			/* Block comment: spans everything, including '?>', up to its close. */` |
|   1515885 | 1338 | `			if( iNest < 1 && zIn[0] == '/' && zIn+1 < zEnd && zIn[1] == '*' ){` |
|       279 | 1339 | `				zIn += 2;` |
|     30931 | 1340 | `				while( (sxu32)(zEnd-zIn) >= sizeof("*/") - 1 ){` |
|     30931 | 1341 | `					if( zIn[0] == '*' && zIn[1] == '/' ){` |
|       279 | 1342 | `						zIn += 2;` |
|       279 | 1343 | `						break;` |
|         - | 1344 | `					}` |
|     30657 | 1345 | `					if( zIn[0] == '\n' ){` |
|       257 | 1346 | `						nLine++;` |
|       126 | 1347 | `					}` |
|     30657 | 1348 | `					zIn++;` |
|         5 | 1349 | `				}` |
|       279 | 1350 | `				continue;` |
|         - | 1351 | `			}` |
|         - | 1352 | `			/* Skip over a single/double-quoted or backtick string literal so a` |
|         - | 1353 | `			 * '?>' sequence inside it is not mistaken for the closing tag. Only` |
|         - | 1354 | `			 * outside a heredoc body (iNest < 1); heredocs are delimited by the` |
|         - | 1355 | `			 * label-matching logic above. Escapes (\" \' \\ and a line-continuing` |
|         - | 1356 | `			 * backslash-newline) are honoured. Same-quote nesting inside "{$...}"` |
|         - | 1357 | `			 * interpolation is not tracked, but that can only end the skip early` |
|         - | 1358 | `			 * on a string that has no '?>' anyway, which stays a PHP chunk either` |
|         - | 1359 | `			 * way — it never mis-splits code that works today. */` |
|   1515611 | 1360 | ``			if( iNest < 1 && (zIn[0] == '\'' \|\| zIn[0] == '"' \|\| zIn[0] == '`') ){`` |
|     45195 | 1361 | `				int qch = zIn[0];` |
|     45195 | 1362 | `				zIn++;` |
|    316673 | 1363 | `				while( zIn < zEnd ){` |
|    316673 | 1364 | `					if( zIn[0] == '\\' && zIn + 1 < zEnd ){` |
|     17549 | 1365 | `						if( zIn[1] == '\n' ){ nLine++; }` |
|     17549 | 1366 | `						zIn += 2;` |
|     17549 | 1367 | `						continue;` |
|         - | 1368 | `					}` |
|    299129 | 1369 | `					if( zIn[0] == qch ){ zIn++; break; }` |
|    253939 | 1370 | `					if( zIn[0] == '\n' ){ nLine++; }` |
|    253939 | 1371 | `					zIn++;` |
|         5 | 1372 | `				}` |
|     45195 | 1373 | `				continue;` |
|         - | 1374 | `			}` |
|   1470421 | 1375 | `			if( zIn[0] == '\n' ){` |
|     61697 | 1376 | `				nLine++;` |
|     61697 | 1377 | `				if( iNest > 0 ){` |
|       363 | 1378 | `					zIn++;` |
|       841 | 1379 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|       483 | 1380 | `						zIn++;` |
|         5 | 1381 | `					}` |
|       363 | 1382 | `					zPtr = zIn;` |
|      1711 | 1383 | `					while( zIn < zEnd ){` |
|      1711 | 1384 | `						if( (unsigned char)zIn[0] >= 0xc0 ){` |
|         - | 1385 | `							/* UTF-8 stream */` |
|        19 | 1386 | `							zIn++;` |
|        37 | 1387 | `							SX_JMP_UTF8(zIn,zEnd);` |
|      1698 | 1388 | `						}else if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       184 | 1389 | `							break;` |
|       ! 0 | 1390 | `						}else{` |
|      1335 | 1391 | `							zIn++;` |
|         - | 1392 | `						}` |
|         5 | 1393 | `					}` |
|       363 | 1394 | `					if( (sxu32)(zIn - zPtr) == sDoc.nByte && SyMemcmp(sDoc.zString,zPtr,sDoc.nByte) == 0 ){` |
|       127 | 1395 | `						iNest = 0;` |
|        61 | 1396 | `					}` |
|       363 | 1397 | `					continue;` |
|         5 | 1398 | `				}` |
|   1439396 | 1399 | `			}else if ( (sxu32)(zEnd - zIn) >= sizeof("<<<") && zIn[0] == '<' && zIn[1] == '<' && zIn[2] == '<' && iNest < 1){` |
|       129 | 1400 | `				zIn += sizeof("<<<")-1;` |
|       141 | 1401 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|        13 | 1402 | `					zIn++;` |
|         1 | 1403 | `				}` |
|       129 | 1404 | `				if( zIn[0] == '"' \|\| zIn[0] == '\'' ){` |
|        55 | 1405 | `					zIn++;` |
|        26 | 1406 | `				}` |
|       129 | 1407 | `				zPtr = zIn;` |
|       589 | 1408 | `				while( zIn < zEnd ){` |
|       589 | 1409 | `					if( (unsigned char)zIn[0] >= 0xc0 ){` |
|         - | 1410 | `						/* UTF-8 stream */` |
|        19 | 1411 | `						zIn++;` |
|        37 | 1412 | `						SX_JMP_UTF8(zIn,zEnd);` |
|       576 | 1413 | `					}else if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|        67 | 1414 | `						break;` |
|       ! 0 | 1415 | `					}else{` |
|       447 | 1416 | `						zIn++;` |
|         - | 1417 | `					}` |
|         5 | 1418 | `				}` |
|       129 | 1419 | `				SyStringInitFromBuf(&sDoc,zPtr,zIn-zPtr);` |
|       129 | 1420 | `				SyStringFullTrim(&sDoc);` |
|       129 | 1421 | `				if( sDoc.nByte > 0 ){` |
|       129 | 1422 | `					iNest++;` |
|        62 | 1423 | `				}` |
|       129 | 1424 | `				continue;` |
|         - | 1425 | `			}` |
|   1469939 | 1426 | `			zIn++;` |
|         - | 1427 |  |
|   1469939 | 1428 | `			if ( zIn >= zEnd )` |
|       ! 0 | 1429 | `				break;` |
|         5 | 1430 | `		}` |
|     12901 | 1431 | `		if( (sxu32)(zEnd - zIn) < sCtag.nByte ){` |
|      6099 | 1432 | `			zIn = zEnd;` |
|      3047 | 1433 | `		}` |
|     12901 | 1434 | `		if( zCur < zIn ){` |
|         - | 1435 | `			/* Save the PHP chunk for later processing */` |
|      9747 | 1436 | `			sToken.nType = PH7_TOKEN_PHP;` |
|      9747 | 1437 | `			SyStringInitFromBuf(&sToken.sData,zCur,zIn-zCur);` |
|     19251 | 1438 | `			SyStringRightTrim(&sToken.sData); /* Trim trailing white spaces */` |
|      9747 | 1439 | `			rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|      9747 | 1440 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 1441 | `				return rc;` |
|         - | 1442 | `			}` |
|      4871 | 1443 | `		}` |
|     12901 | 1444 | `		if( zIn < zEnd ){` |
|         - | 1445 | `			/* Jump the trailing closing tag */` |
|      6807 | 1446 | `			zIn += sCtag.nByte;` |
|         - | 1447 | `			/* php's lexer swallows exactly ONE newline immediately after the` |
|         - | 1448 | `			 * closing tag ("?>\n" emits nothing) */` |
|      6807 | 1449 | `			if( zIn < zEnd && zIn[0] == '\r' && zIn + 1 < zEnd && zIn[1] == '\n' ){` |
|       ! 0 | 1450 | `				zIn += 2;` |
|       ! 0 | 1451 | `				nLine++;` |
|      6807 | 1452 | `			}else if( zIn < zEnd && zIn[0] == '\n' ){` |
|        60 | 1453 | `				zIn++;` |
|        60 | 1454 | `				nLine++;` |
|        28 | 1455 | `			}` |
|      3401 | 1456 | `		}` |
|         5 | 1457 | `	} /* For(;;) */` |
|         - | 1458 |  |
|     12913 | 1459 | ` 	return SXRET_OK;` |
|      6459 | 1460 | `}` |
|         - | 1461 |  |
