# src/ph7/lex.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 830/887 lines (93.57%)

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
|         - |   13 | `static sxu32 KeywordCodeCI(const char *zRaw, int n);` |
|         - |   14 | `static sxi32 LexExtractHeredoc(SyStream *pStream,SyToken *pToken);` |
|         - |   15 | `/*` |
|         - |   16 | ` * Tokenize a raw PHP input.` |
|         - |   17 | ` * Get a single low-level token from the input file. Update the stream pointer so that` |
|         - |   18 | ` * it points to the first character beyond the extracted token.` |
|         - |   19 | ` */` |
| 199641400 |   20 | `static sxi32 TokenizePHP(SyStream *pStream,SyToken *pToken,void *pUserData,void *pCtxData)` |
|         5 |   21 | `{` |
|         - |   22 | `	SyString *pStr;` |
|         - |   23 | `	sxi32 rc;` |
|         - |   24 | `	/* Ignore leading white spaces */` |
| 296112563 |   25 | `	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){` |
|         - |   26 | `		/* Advance the stream cursor */` |
|  96471163 |   27 | `		if( pStream->zText[0] == '\n' ){` |
|         - |   28 | `			/* Update line counter */` |
|     61529 |   29 | `			pStream->nLine++;` |
|     30762 |   30 | `		}` |
|  96471163 |   31 | `		pStream->zText++;` |
|         5 |   32 | `	}` |
| 199641405 |   33 | `	if( pStream->zText >= pStream->zEnd ){` |
|         - |   34 | `		/* End of input reached */` |
|         3 |   35 | `		return SXERR_EOF;` |
|         - |   36 | `	}` |
|         - |   37 | `	/* Record token starting position and line */` |
| 199641403 |   38 | `	pToken->nLine = pStream->nLine;` |
| 199641403 |   39 | `	pToken->pUserData = 0;` |
| 199641403 |   40 | `	pStr = &pToken->sData;` |
| 199641403 |   41 | `	SyStringInitFromBuf(pStr,pStream->zText,0);` |
| 233174632 |   42 | `	if( pStream->zText[0] >= 0xc0 \|\| SyisAlpha(pStream->zText[0]) \|\| pStream->zText[0] == '_' ){` |
|         - |   43 | `		/* The following code fragment is taken verbatim from the xPP source tree.` |
|         - |   44 | `		 * xPP is a modern embeddable macro processor with advanced features useful for` |
|         - |   45 | `		 * application seeking for a production quality,ready to use macro processor.` |
|         - |   46 | `		 * xPP is a widely used library developed and maintened by Symisc Systems.` |
|         - |   47 | `		 * You can reach the xPP home page by following this link:` |
|         - |   48 | `		 * http://xpp.symisc.net/` |
|         - |   49 | `		 */` |
|         - |   50 | `		const unsigned char *zIn;` |
|         - |   51 | `		sxu32 nKeyword;` |
|         - |   52 | `		/* Isolate UTF-8 or alphanumeric stream */` |
|  67066463 |   53 | `		if( pStream->zText[0] < 0xc0 ){` |
|  67066441 |   54 | `			pStream->zText++;` |
|  33533218 |   55 | `		}` |
|  63086356 |   56 | `		for(;;){` |
| 126172717 |   57 | `			zIn = pStream->zText;` |
| 126172717 |   58 | `			if( zIn[0] >= 0xc0 ){` |
|        81 |   59 | `				zIn++;` |
|         - |   60 | `				/* UTF-8 stream */` |
|       173 |   61 | `				while( zIn < pStream->zEnd && ((zIn[0] & 0xc0) == 0x80) ){` |
|        93 |   62 | `					zIn++;` |
|         1 |   63 | `				}` |
|        40 |   64 | `			}` |
|         - |   65 | `			/* Skip alphanumeric stream */` |
| 507057709 |   66 | `			while( zIn < pStream->zEnd && zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_') ){` |
| 317798641 |   67 | `				zIn++;` |
|         5 |   68 | `			}` |
| 126172717 |   69 | `			if( zIn == pStream->zText ){` |
|         - |   70 | `				/* Not an UTF-8 or alphanumeric stream */` |
|  67066463 |   71 | `				break;` |
|         - |   72 | `			}` |
|         - |   73 | `			/* Synchronize pointers */` |
|  59106259 |   74 | `			pStream->zText = zIn;` |
|         5 |   75 | `		}` |
|         - |   76 | `		/* Record token length */` |
|  67066463 |   77 | `		pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  67066463 |   78 | `		nKeyword = KeywordCodeCI(pStr->zString,(int)pStr->nByte);` |
|  67066463 |   79 | `		if( nKeyword != PH7_TK_ID ){` |
|  22195669 |   80 | `			if( nKeyword &` |
|         - |   81 | `				(PH7_TKWRD_NEW\|PH7_TKWRD_CLONE\|PH7_TKWRD_AND\|PH7_TKWRD_XOR\|PH7_TKWRD_OR\|PH7_TKWRD_INSTANCEOF) ){` |
|         - |   82 | `					/* Alpha stream operators [i.e: new,clone,and,instanceof,or,xor],save the operator instance for later processing */` |
|   1072263 |   83 | `					pToken->pUserData = (void *)PH7_ExprExtractOperator(pStr,0);` |
|         - |   84 | `					/* Mark as an operator */` |
|   1072263 |   85 | `					pToken->nType = PH7_TK_ID\|PH7_TK_OP;` |
|    536134 |   86 | `			}else{` |
|         - |   87 | `				/* We are dealing with a keyword [i.e: while,foreach,class...],save the keyword ID */` |
|  21123411 |   88 | `				pToken->nType = PH7_TK_KEYWORD;` |
|  21123411 |   89 | `				pToken->pUserData = SX_INT_TO_PTR(nKeyword);` |
|         - |   90 | `			}` |
|  11097837 |   91 | `		}else{` |
|         - |   92 | `			/* A simple identifier */` |
|  44870799 |   93 | `			pToken->nType = PH7_TK_ID;` |
|         - |   94 | `		}` |
|  33533234 |   95 | `	}else{` |
|         - |   96 | `		sxi32 c;` |
|         - |   97 | `		/* Non-alpha stream */` |
| 132574945 |   98 | `		if( pStream->zText[0] == '#' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '[' ){` |
|      8451 |   99 | `			sxu32 nDepth = 1;` |
|         - |  100 | `			/* PHP 8 attribute group '#[ ... ]': skip the whole balanced group as` |
|         - |  101 | `			 * trivia (attributes are not stored yet). Brackets inside string` |
|         - |  102 | `			 * literals and comments must not affect the depth count. An` |
|         - |  103 | `			 * unterminated group is silently consumed up to EOF, consistent` |
|         - |  104 | `			 * with unterminated block comments below.` |
|         - |  105 | `			 */` |
|         - |  106 | `			const unsigned char *zGroupStart;` |
|      8451 |  107 | `			pStream->zText += 2;` |
|      8451 |  108 | `			zGroupStart = pStream->zText;` |
|    690289 |  109 | `			while( pStream->zText < pStream->zEnd && nDepth > 0 ){` |
|    681843 |  110 | `				sxi32 d = pStream->zText[0];` |
|    681843 |  111 | `				if( d == '[' ){` |
|        11 |  112 | `					nDepth++;` |
|    681838 |  113 | `				}else if( d == ']' ){` |
|      8461 |  114 | `					nDepth--;` |
|    677605 |  115 | `				}else if( d == '\'' \|\| d == '"' ){` |
|         - |  116 | `					/* String literal: scan for the matching unescaped quote */` |
|        46 |  117 | `					pStream->zText++;` |
|       296 |  118 | `					while( pStream->zText < pStream->zEnd ){` |
|       296 |  119 | `						if( pStream->zText[0] == '\\' && &pStream->zText[1] < pStream->zEnd ){` |
|         3 |  120 | `							if( pStream->zText[1] == '\n' ){` |
|       ! 0 |  121 | `								pStream->nLine++;` |
|       ! 0 |  122 | `							}` |
|         3 |  123 | `							pStream->zText += 2;` |
|         3 |  124 | `							continue;` |
|         - |  125 | `						}` |
|       294 |  126 | `						if( pStream->zText[0] == d ){` |
|        46 |  127 | `							break;` |
|         - |  128 | `						}` |
|       250 |  129 | `						if( pStream->zText[0] == '\n' ){` |
|       ! 0 |  130 | `							pStream->nLine++;` |
|       ! 0 |  131 | `						}` |
|       250 |  132 | `						pStream->zText++;` |
|         2 |  133 | `					}` |
|        46 |  134 | `					if( pStream->zText >= pStream->zEnd ){` |
|       ! 0 |  135 | `						break; /* Unterminated string literal */` |
|         2 |  136 | `					}` |
|         - |  137 | `					/* Fall through: consume the closing quote below */` |
|    673355 |  138 | `				}else if( d == '#' \|\| (d == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '/') ){` |
|         - |  139 | `					/* Inline comment inside the group */` |
|       ! 0 |  140 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] != '\n' ){` |
|       ! 0 |  141 | `						pStream->zText++;` |
|       ! 0 |  142 | `					}` |
|       ! 0 |  143 | `					continue; /* Let the outer loop count the newline */` |
|    673333 |  144 | `				}else if( d == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '*' ){` |
|         - |  145 | `					/* Block comment inside the group */` |
|       ! 0 |  146 | `					pStream->zText += 2;` |
|       ! 0 |  147 | `					while( pStream->zText < pStream->zEnd ){` |
|       ! 0 |  148 | `						if( pStream->zText[0] == '*' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '/' ){` |
|       ! 0 |  149 | `							pStream->zText += 2;` |
|       ! 0 |  150 | `							break;` |
|         - |  151 | `						}` |
|       ! 0 |  152 | `						if( pStream->zText[0] == '\n' ){` |
|       ! 0 |  153 | `							pStream->nLine++;` |
|       ! 0 |  154 | `						}` |
|       ! 0 |  155 | `						pStream->zText++;` |
|       ! 0 |  156 | `					}` |
|       ! 0 |  157 | `					continue;` |
|    673333 |  158 | `				}else if( d == '\n' ){` |
|         7 |  159 | `					pStream->nLine++;` |
|         3 |  160 | `				}` |
|    681843 |  161 | `				pStream->zText++;` |
|         5 |  162 | `			}` |
|      8451 |  163 | `			if( pUserData && pStream->pSet ){` |
|         - |  164 | `				/* Record the group's inner span (between #[ and its balanced ])` |
|         - |  165 | `				 * in the trivia sidecar, keyed like doc-comments. */` |
|         - |  166 | `				ph7_trivia sTrivia;` |
|      8451 |  167 | `				const unsigned char *zGroupEnd = pStream->zText;` |
|      8451 |  168 | `				if( nDepth == 0 && zGroupEnd > zGroupStart ){` |
|      8451 |  169 | `					zGroupEnd--; /* Exclude the closing ']' */` |
|      4223 |  170 | `				}` |
|      8451 |  171 | `				sTrivia.nTokIdx = SySetUsed(pStream->pSet);` |
|      8451 |  172 | `				sTrivia.iKind = PH7_TRIVIA_ATTR;` |
|      8451 |  173 | `				SyStringInitFromBuf(&sTrivia.sText,(const char *)zGroupStart,(sxu32)(zGroupEnd - zGroupStart));` |
|      8451 |  174 | `				sTrivia.nLine = pToken->nLine;` |
|      8451 |  175 | `				SySetPut((SySet *)pUserData,(const void *)&sTrivia);` |
|      4223 |  176 | `			}` |
|         - |  177 | `			/* Tell the upper-layer to ignore this token */` |
|      8451 |  178 | `			return SXERR_CONTINUE;` |
| 132695408 |  179 | `		}else if( pStream->zText[0] == '#' \|\|` |
| 132566488 |  180 | `			( pStream->zText[0] == '/' &&  &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '/') ){` |
|      9031 |  181 | `				pStream->zText++;` |
|         - |  182 | `				/* Inline comments */` |
|    420503 |  183 | `				while( pStream->zText < pStream->zEnd && pStream->zText[0] != '\n' ){` |
|    411477 |  184 | `					pStream->zText++;` |
|         5 |  185 | `				}` |
|         - |  186 | `				/* Tell the upper-layer to ignore this token */` |
|      9031 |  187 | `				return SXERR_CONTINUE;` |
| 132557473 |  188 | `		}else if( pStream->zText[0] == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '*' ){` |
|         - |  189 | `			/* A doc-comment starts with slash-star-star followed by more` |
|         - |  190 | `			 * content (slash-star-star-slash is the empty comment, not a` |
|         - |  191 | `			 * docblock). Its full span, delimiters included, goes to the` |
|         - |  192 | `			 * trivia sidecar when the caller supplied one — keyed by the` |
|         - |  193 | `			 * index the NEXT real token receives — and never enters the` |
|         - |  194 | `			 * token stream. */` |
|    244557 |  195 | `			const unsigned char *zDocStart = pStream->zText;` |
|    244569 |  196 | `			int bDoc = ( &pStream->zText[2] < pStream->zEnd && pStream->zText[2] == '*'` |
|    366840 |  197 | `			 && ( &pStream->zText[3] >= pStream->zEnd \|\| pStream->zText[3] != '/' ) );` |
|    244557 |  198 | `			pStream->zText += 2;` |
|         - |  199 | `			/* Block comment */` |
|  22251501 |  200 | `			while( pStream->zText < pStream->zEnd ){` |
|  22251501 |  201 | `				if( pStream->zText[0] == '*' ){` |
|    352483 |  202 | `					if( &pStream->zText[1] >= pStream->zEnd \|\| pStream->zText[1] == '/'  ){` |
|    122281 |  203 | `						break;` |
|         - |  204 | `					}` |
|     53963 |  205 | `				}` |
|  22006949 |  206 | `				if( pStream->zText[0] == '\n' ){` |
|       259 |  207 | `					pStream->nLine++;` |
|       127 |  208 | `				}` |
|  22006949 |  209 | `				pStream->zText++;` |
|         5 |  210 | `			}` |
|    244557 |  211 | `			pStream->zText += 2;` |
|    244557 |  212 | `			if( bDoc && pUserData && pStream->pSet ){` |
|         - |  213 | `				ph7_trivia sTrivia;` |
|        29 |  214 | `				const unsigned char *zDocEnd = pStream->zText;` |
|        29 |  215 | `				if( zDocEnd > pStream->zEnd ){` |
|       ! 0 |  216 | `					zDocEnd = pStream->zEnd; /* Unterminated comment at EOF */` |
|       ! 0 |  217 | `				}` |
|        29 |  218 | `				sTrivia.nTokIdx = SySetUsed(pStream->pSet);` |
|        29 |  219 | `				sTrivia.iKind = PH7_TRIVIA_DOC;` |
|        29 |  220 | `				SyStringInitFromBuf(&sTrivia.sText,(const char *)zDocStart,(sxu32)(zDocEnd - zDocStart));` |
|        29 |  221 | `				sTrivia.nLine = pToken->nLine;` |
|        29 |  222 | `				SySetPut((SySet *)pUserData,(const void *)&sTrivia);` |
|        12 |  223 | `			}` |
|         - |  224 | `			/* Tell the upper-layer to ignore this token */` |
|    244557 |  225 | `			return SXERR_CONTINUE;` |
| 132312921 |  226 | `		}else if( SyisDigit(pStream->zText[0]) ){` |
|   4119437 |  227 | `			pStream->zText++;` |
|         - |  228 | `			/* PHP 7.4: handle underscore separator immediately following the first digit.` |
|         - |  229 | `			 * Check pStream->zText < pStream->zEnd BEFORE forming pStream->zText + 1 so` |
|         - |  230 | `			 * we never compute a pointer past one-past-end. */` |
|   4119432 |  231 | `			if( pStream->zText < pStream->zEnd` |
|   4119432 |  232 | `				&& pStream->zText[0] == '_'` |
|   2059798 |  233 | `				&& pStream->zText + 1 < pStream->zEnd` |
|       164 |  234 | `				&& pStream->zText[1] < 0xc0` |
|       169 |  235 | `				&& SyisDigit(pStream->zText[1]) ){` |
|       156 |  236 | `				pStream->zText++; /* swallow underscore between two digits */` |
|        77 |  237 | `			}` |
|         - |  238 | `			/* Decimal digit stream (PHP 7.4: underscore separator allowed between two digits) */` |
|   5164919 |  239 | `			while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|   1045487 |  240 | `				pStream->zText++;` |
|   1045482 |  241 | `				if( pStream->zText < pStream->zEnd` |
|   1045482 |  242 | `					&& pStream->zText[0] == '_'` |
|    522827 |  243 | `					&& pStream->zText + 1 < pStream->zEnd` |
|       172 |  244 | `					&& pStream->zText[1] < 0xc0` |
|       177 |  245 | `					&& SyisDigit(pStream->zText[1]) ){` |
|       173 |  246 | `					pStream->zText++; /* swallow underscore between two digits */` |
|        86 |  247 | `				}` |
|         5 |  248 | `			}` |
|         - |  249 | `			/* Mark the token as integer until we encounter a real number */` |
|   4119437 |  250 | `			pToken->nType = PH7_TK_INTEGER;` |
|   4119437 |  251 | `			if( pStream->zText < pStream->zEnd ){` |
|   4119437 |  252 | `				c = pStream->zText[0];` |
|   4119437 |  253 | `				if( c == '.' ){` |
|         - |  254 | `					/* Real number (PHP 7.4: underscore separator allowed between two digits) */` |
|      9251 |  255 | `					pStream->zText++;` |
|     19975 |  256 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|     10729 |  257 | `						pStream->zText++;` |
|     10724 |  258 | `						if( pStream->zText < pStream->zEnd` |
|     10724 |  259 | `							&& pStream->zText[0] == '_'` |
|      5368 |  260 | `							&& pStream->zText + 1 < pStream->zEnd` |
|        12 |  261 | `							&& pStream->zText[1] < 0xc0` |
|        17 |  262 | `							&& SyisDigit(pStream->zText[1]) ){` |
|        13 |  263 | `							pStream->zText++;` |
|         6 |  264 | `						}` |
|         5 |  265 | `					}` |
|      9251 |  266 | `					if( pStream->zText < pStream->zEnd ){` |
|      9251 |  267 | `						c = pStream->zText[0];` |
|      9251 |  268 | `						if( c=='e' \|\| c=='E' ){` |
|        65 |  269 | `							pStream->zText++;` |
|        65 |  270 | `							if( pStream->zText < pStream->zEnd ){` |
|        65 |  271 | `								c = pStream->zText[0];` |
|        64 |  272 | `								if( (c =='+' \|\| c=='-') && &pStream->zText[1] < pStream->zEnd  &&` |
|        35 |  273 | `									pStream->zText[1] < 0xc0 && SyisDigit(pStream->zText[1]) ){` |
|        35 |  274 | `										pStream->zText++;` |
|        17 |  275 | `								}` |
|       183 |  276 | `								while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|       119 |  277 | `									pStream->zText++;` |
|       118 |  278 | `									if( pStream->zText < pStream->zEnd` |
|       118 |  279 | `										&& pStream->zText[0] == '_'` |
|        63 |  280 | `										&& pStream->zText + 1 < pStream->zEnd` |
|         8 |  281 | `										&& pStream->zText[1] < 0xc0` |
|         9 |  282 | `										&& SyisDigit(pStream->zText[1]) ){` |
|         9 |  283 | `										pStream->zText++;` |
|         4 |  284 | `									}` |
|         1 |  285 | `								}` |
|        32 |  286 | `							}` |
|        32 |  287 | `						}` |
|      4623 |  288 | `					}` |
|      9251 |  289 | `					pToken->nType = PH7_TK_REAL;` |
|   4114814 |  290 | `				}else if( c=='e' \|\| c=='E' ){` |
|        59 |  291 | `					SXUNUSED(pUserData); /* Prevent compiler warning */` |
|        59 |  292 | `					SXUNUSED(pCtxData);` |
|       121 |  293 | `					pStream->zText++;` |
|       121 |  294 | `					if( pStream->zText < pStream->zEnd ){` |
|       121 |  295 | `						c = pStream->zText[0];` |
|       118 |  296 | `						if( (c =='+' \|\| c=='-') && &pStream->zText[1] < pStream->zEnd  &&` |
|        39 |  297 | `							pStream->zText[1] < 0xc0 && SyisDigit(pStream->zText[1]) ){` |
|        38 |  298 | `								pStream->zText++;` |
|        18 |  299 | `						}` |
|       363 |  300 | `						while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|       245 |  301 | `							pStream->zText++;` |
|       242 |  302 | `							if( pStream->zText < pStream->zEnd` |
|       242 |  303 | `								&& pStream->zText[0] == '_'` |
|       123 |  304 | `								&& pStream->zText + 1 < pStream->zEnd` |
|         4 |  305 | `								&& pStream->zText[1] < 0xc0` |
|         7 |  306 | `								&& SyisDigit(pStream->zText[1]) ){` |
|         5 |  307 | `								pStream->zText++;` |
|         2 |  308 | `							}` |
|         3 |  309 | `						}` |
|        59 |  310 | `					}` |
|       121 |  311 | `					pToken->nType = PH7_TK_REAL;` |
|         - |  312 | `				/* php only reads a base prefix when the literal so far is exactly "0"` |
|         - |  313 | `				 * AND at least one valid digit follows it. Otherwise the '0' stands` |
|         - |  314 | `				 * alone as an integer and the letter begins an IDENTIFIER, which is` |
|         - |  315 | ``				 * why php reports `0xG` as `unexpected identifier "xG"` while PHL,`` |
|         - |  316 | `				 * consuming the prefix unconditionally, reported just "G". The same` |
|         - |  317 | ``				 * gap silently ACCEPTED `0x`/`0b`/`0o` as int(0), and read `1x5` as`` |
|         - |  318 | `				 * a hex literal, both of which php rejects outright. */` |
|   4110130 |  319 | `				}else if( (c == 'x' \|\| c == 'X')` |
|   2110975 |  320 | `					&& pStream->zText == &((const unsigned char *)pStr->zString)[1] && pStr->zString[0] == 0x30` |
|    111882 |  321 | `					&& &pStream->zText[1] < pStream->zEnd` |
|    111887 |  322 | `					&& pStream->zText[1] < 0xc0 && SyisHex(pStream->zText[1]) ){` |
|         - |  323 | `					/* Hex digit stream (PHP 7.4: underscore separator allowed between two digits) */` |
|    111883 |  324 | `					pStream->zText++;` |
|    484979 |  325 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisHex(pStream->zText[0]) ){` |
|    373101 |  326 | `						pStream->zText++;` |
|    373096 |  327 | `						if( pStream->zText < pStream->zEnd` |
|    373096 |  328 | `							&& pStream->zText[0] == '_'` |
|    186573 |  329 | `							&& pStream->zText + 1 < pStream->zEnd` |
|        50 |  330 | `							&& pStream->zText[1] < 0xc0` |
|        55 |  331 | `							&& SyisHex(pStream->zText[1]) ){` |
|        51 |  332 | `							pStream->zText++;` |
|        25 |  333 | `						}` |
|         5 |  334 | `					}` |
|   4054134 |  335 | `				}else if( (c == 'b' \|\| c == 'B')` |
|   1999239 |  336 | `					&& pStream->zText == &((const unsigned char *)pStr->zString)[1] && pStr->zString[0] == 0x30` |
|       288 |  337 | `					&& &pStream->zText[1] < pStream->zEnd` |
|       293 |  338 | `					&& (pStream->zText[1] == '0' \|\| pStream->zText[1] == '1') ){` |
|         - |  339 | `					/* Binary digit stream (PHP 7.4: underscore separator allowed between two digits) */` |
|       287 |  340 | `					pStream->zText++;` |
|      3115 |  341 | `					while( pStream->zText < pStream->zEnd && (pStream->zText[0] == '0' \|\| pStream->zText[0] == '1') ){` |
|      1791 |  342 | `						pStream->zText++;` |
|      1790 |  343 | `						if( pStream->zText < pStream->zEnd` |
|      1790 |  344 | `							&& pStream->zText[0] == '_'` |
|       965 |  345 | `							&& pStream->zText + 1 < pStream->zEnd` |
|       141 |  346 | `							&& (pStream->zText[1] == '0' \|\| pStream->zText[1] == '1') ){` |
|       141 |  347 | `							pStream->zText++;` |
|        70 |  348 | `						}` |
|         1 |  349 | `					}` |
|   3998048 |  350 | `				}else if( (c == 'o' \|\| c == 'O')` |
|   1998962 |  351 | `					&& pStream->zText == &((const unsigned char *)pStr->zString)[1] && pStr->zString[0] == 0x30` |
|        20 |  352 | `					&& &pStream->zText[1] < pStream->zEnd` |
|        25 |  353 | `					&& pStream->zText[1] >= '0' && pStream->zText[1] <= '7' ){` |
|         - |  354 | `					/* PHP 8.1 explicit octal 0o/0O (underscore separator allowed between two digits) */` |
|        21 |  355 | `					pStream->zText++;` |
|       101 |  356 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] >= '0' && pStream->zText[0] <= '7' ){` |
|        81 |  357 | `						pStream->zText++;` |
|        80 |  358 | `						if( pStream->zText < pStream->zEnd` |
|        80 |  359 | `							&& pStream->zText[0] == '_'` |
|        41 |  360 | `							&& pStream->zText + 1 < pStream->zEnd` |
|         3 |  361 | `							&& pStream->zText[1] >= '0' && pStream->zText[1] <= '7' ){` |
|         3 |  362 | `							pStream->zText++;` |
|         1 |  363 | `						}` |
|         1 |  364 | `					}` |
|        10 |  365 | `				}` |
|   2059716 |  366 | `			}` |
|         - |  367 | `			/* PHP 7.4: absorb a trailing malformed underscore run into the` |
|         - |  368 | `			 * numeric token so the compile phase can emit a PHP-compatible` |
|         - |  369 | `			 * "syntax error, unexpected identifier" parse error. Valid` |
|         - |  370 | `			 * separators were already consumed by the per-loop peek logic` |
|         - |  371 | `			 * above, so an underscore here is always misplaced. */` |
|   4119437 |  372 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '_' ){` |
|        14 |  373 | `				pStream->zText++;` |
|        28 |  374 | `				while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0` |
|        31 |  375 | `					&& (SyisAlphaNum(pStream->zText[0]) \|\| pStream->zText[0] == '_') ){` |
|        10 |  376 | `					pStream->zText++;` |
|         2 |  377 | `				}` |
|         5 |  378 | `			}` |
|         - |  379 | `			/* Record token length */` |
|   4119437 |  380 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|   4119437 |  381 | `			return SXRET_OK;` |
|         - |  382 | `		}` |
| 128193489 |  383 | `		c = pStream->zText[0];` |
| 128193489 |  384 | `		pStream->zText++; /* Advance the stream cursor */` |
|         - |  385 | `		/* Assume we are dealing with an operator*/` |
| 128193489 |  386 | `		pToken->nType = PH7_TK_OP;` |
| 128193489 |  387 | `		switch(c){` |
|  26453117 |  388 | `		case '$': pToken->nType = PH7_TK_DOLLAR; break;` |
|   7851885 |  389 | `		case '{': pToken->nType = PH7_TK_OCB;    break;` |
|   7851871 |  390 | `		case '}': pToken->nType = PH7_TK_CCB;    break;` |
|  16601691 |  391 | `		case '(': pToken->nType = PH7_TK_LPAREN; break;` |
|   3497083 |  392 | `		case '[': pToken->nType \|= PH7_TK_OSB;   break; /* Bitwise operation here,since the square bracket token '['` |
|         - |  393 | `														 * is a potential operator [i.e: subscripting] */` |
|   3497089 |  394 | `		case ']': pToken->nType = PH7_TK_CSB;    break;` |
|   8300835 |  395 | `		case ')': {` |
|  16601675 |  396 | `			SySet *pTokSet = pStream->pSet;` |
|         - |  397 | `			/* Assemble type cast operators [i.e: (int),(float),(bool)...] */` |
|  16601675 |  398 | `			if( pTokSet->nUsed >= 2 ){` |
|         - |  399 | `				SyToken *pTmp;` |
|         - |  400 | `				/* Peek the last recongnized token */` |
|  16601673 |  401 | `				pTmp = (SyToken *)SySetPeek(pTokSet);` |
|  16601673 |  402 | `				if( pTmp->nType & PH7_TK_KEYWORD ){` |
|   1304883 |  403 | `					sxi32 nID = SX_PTR_TO_INT(pTmp->pUserData);` |
|   1304883 |  404 | `					if( (sxu32)nID & (PH7_TKWRD_ARRAY\|PH7_TKWRD_INT\|PH7_TKWRD_FLOAT\|PH7_TKWRD_STRING\|PH7_TKWRD_OBJECT\|PH7_TKWRD_BOOL\|PH7_TKWRD_UNSET) ){` |
|   1172169 |  405 | `						pTmp = (SyToken *)SySetAt(pTokSet,pTokSet->nUsed - 2);` |
|   1172169 |  406 | `						if( pTmp->nType & PH7_TK_LPAREN ){` |
|         - |  407 | `							/* Merge the three tokens '(' 'TYPE' ')' into a single one */` |
|    824323 |  408 | `							const char * zTypeCast = "(int)";` |
|    824323 |  409 | `							if( nID & PH7_TKWRD_FLOAT ){` |
|     33153 |  410 | `								zTypeCast = "(float)";` |
|    807749 |  411 | `							}else if( nID & PH7_TKWRD_BOOL ){` |
|     24885 |  412 | `								zTypeCast = "(bool)";` |
|    778735 |  413 | `							}else if( nID & PH7_TKWRD_STRING ){` |
|    405807 |  414 | `								zTypeCast = "(string)";` |
|    563394 |  415 | `							}else if( nID & PH7_TKWRD_ARRAY ){` |
|        48 |  416 | `								zTypeCast = "(array)";` |
|    360471 |  417 | `							}else if( nID & PH7_TKWRD_OBJECT ){` |
|        42 |  418 | `								zTypeCast = "(object)";` |
|    360429 |  419 | `							}else if( nID & PH7_TKWRD_UNSET ){` |
|         3 |  420 | `								zTypeCast = "(unset)";` |
|         1 |  421 | `							}` |
|         - |  422 | `							/* Reflect the change */` |
|    824323 |  423 | `							pToken->nType = PH7_TK_OP;` |
|    824323 |  424 | `							SyStringInitFromBuf(&pToken->sData,zTypeCast,SyStrlen(zTypeCast));` |
|         - |  425 | `							/* Save the instance associated with the type cast operator */` |
|    824323 |  426 | `							pToken->pUserData = (void *)PH7_ExprExtractOperator(&pToken->sData,0);` |
|         - |  427 | `							/* Remove the two previous tokens */` |
|    824323 |  428 | `							pTokSet->nUsed -= 2;` |
|    824323 |  429 | `							return SXRET_OK;` |
|         - |  430 | `						}` |
|    173923 |  431 | `					}` |
|    240280 |  432 | `				}` |
|   7888675 |  433 | `			}` |
|  15777357 |  434 | `			pToken->nType = PH7_TK_RPAREN;` |
|  15777357 |  435 | `			break;` |
|         - |  436 | `				  }` |
|   2994343 |  437 | `		case '\'':{` |
|         - |  438 | `			/* Single quoted string */` |
|   5988691 |  439 | `			pStr->zString++;` |
|  68749925 |  440 | `			while( pStream->zText < pStream->zEnd ){` |
|  68749925 |  441 | `				if( pStream->zText[0] == '\''  ){` |
|   5988707 |  442 | `					if( pStream->zText[-1] != '\\' ){` |
|   5951385 |  443 | `						break;` |
|       ! 0 |  444 | `					}else{` |
|     37327 |  445 | `						const unsigned char *zPtr = &pStream->zText[-2];` |
|     37327 |  446 | `						sxi32 i = 1;` |
|     74653 |  447 | `						while( zPtr > pStream->zInput && zPtr[0] == '\\' ){` |
|     37331 |  448 | `							zPtr--;` |
|     37331 |  449 | `							i++;` |
|         5 |  450 | `						}` |
|     37327 |  451 | `						if((i&1)==0){` |
|     37311 |  452 | `							break;` |
|         - |  453 | `						}` |
|         - |  454 | `					}` |
|         8 |  455 | `				}` |
|  62761239 |  456 | `				if( pStream->zText[0] == '\n' ){` |
|        63 |  457 | `					pStream->nLine++;` |
|        31 |  458 | `				}` |
|  62761239 |  459 | `				pStream->zText++;` |
|         5 |  460 | `			}` |
|         - |  461 | `			/* Record token length and type */` |
|   5988691 |  462 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|   5988691 |  463 | `			pToken->nType = PH7_TK_SSTR;` |
|         - |  464 | `			/* Jump the trailing single quote */` |
|   5988691 |  465 | `			pStream->zText++;` |
|   5988691 |  466 | `			return SXRET_OK;` |
|         - |  467 | `				  }` |
|     67191 |  468 | `		case '"':{` |
|         - |  469 | `			sxi32 iNest;` |
|         - |  470 | `			/* Double quoted string */` |
|    134387 |  471 | `			pStr->zString++;` |
|   1853695 |  472 | `			while( pStream->zText < pStream->zEnd ){` |
|   1853695 |  473 | `				if( pStream->zText[0] == '{' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '$'){` |
|       151 |  474 | `					iNest = 1;` |
|       151 |  475 | `					pStream->zText++;` |
|         - |  476 | `					/* TICKET 1433-40: Hnadle braces'{}' in double quoted string where everything is allowed */` |
|      1367 |  477 | `					while(pStream->zText < pStream->zEnd ){` |
|      1367 |  478 | `						if( pStream->zText[0] == '{' ){` |
|         3 |  479 | `							iNest++;` |
|      1366 |  480 | `						}else if (pStream->zText[0] == '}' ){` |
|       153 |  481 | `							iNest--;` |
|       153 |  482 | `							if( iNest <= 0 ){` |
|       151 |  483 | `								pStream->zText++;` |
|       151 |  484 | `								break;` |
|         1 |  485 | `							}` |
|      1216 |  486 | `						}else if( pStream->zText[0] == '\n' ){` |
|       ! 0 |  487 | `							pStream->nLine++;` |
|       ! 0 |  488 | `						}` |
|      1219 |  489 | `						pStream->zText++;` |
|         3 |  490 | `					}` |
|       151 |  491 | `					if( pStream->zText >= pStream->zEnd ){` |
|       ! 0 |  492 | `						break;` |
|         - |  493 | `					}` |
|        74 |  494 | `				}` |
|   1853695 |  495 | `				if( pStream->zText[0] == '"' ){` |
|    134697 |  496 | `					if( pStream->zText[-1] != '\\' ){` |
|    134353 |  497 | `						break;` |
|       ! 0 |  498 | `					}else{` |
|       349 |  499 | `						const unsigned char *zPtr = &pStream->zText[-2];` |
|       349 |  500 | `						sxi32 i = 1;` |
|       431 |  501 | `						while( zPtr > pStream->zInput && zPtr[0] == '\\' ){` |
|        85 |  502 | `							zPtr--;` |
|        85 |  503 | `							i++;` |
|         3 |  504 | `						}` |
|       349 |  505 | `						if((i&1)==0){` |
|        35 |  506 | `							break;` |
|         - |  507 | `						}` |
|         - |  508 | `					}` |
|       155 |  509 | `				}` |
|   1719313 |  510 | `				if( pStream->zText[0] == '\n' ){` |
|        47 |  511 | `					pStream->nLine++;` |
|        23 |  512 | `				}` |
|   1719313 |  513 | `				pStream->zText++;` |
|         5 |  514 | `			}` |
|         - |  515 | `			/* Record token length and type */` |
|    134387 |  516 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|    134387 |  517 | `			pToken->nType = PH7_TK_DSTR;` |
|         - |  518 | `			/* Jump the trailing quote */` |
|    134387 |  519 | `			pStream->zText++;` |
|    134387 |  520 | `			return SXRET_OK;` |
|         - |  521 | `				  }` |
|         1 |  522 | ``		case '`':{`` |
|         - |  523 | `			/* Backtick quoted string */` |
|         3 |  524 | `			pStr->zString++;` |
|        21 |  525 | `			while( pStream->zText < pStream->zEnd ){` |
|        21 |  526 | ``				if( pStream->zText[0] == '`' && pStream->zText[-1] != '\\' ){`` |
|         3 |  527 | `					break;` |
|         - |  528 | `				}` |
|        19 |  529 | `				if( pStream->zText[0] == '\n' ){` |
|       ! 0 |  530 | `					pStream->nLine++;` |
|       ! 0 |  531 | `				}` |
|        19 |  532 | `				pStream->zText++;` |
|         1 |  533 | `			}` |
|         - |  534 | `			/* Record token length and type */` |
|         3 |  535 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|         3 |  536 | `			pToken->nType = PH7_TK_BSTR;` |
|         - |  537 | `			/* Jump the trailing backtick */` |
|         3 |  538 | `			pStream->zText++;` |
|         3 |  539 | `			return SXRET_OK;` |
|         - |  540 | `				  }` |
|      9495 |  541 | `		case '\\': pToken->nType = PH7_TK_NSSEP;  break;` |
|    590973 |  542 | `		case ':':` |
|   1181951 |  543 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == ':' ){` |
|         - |  544 | `				/* Current operator: '::' */` |
|    448429 |  545 | `				pStream->zText++;` |
|    224217 |  546 | `			}else{` |
|    733527 |  547 | `				pToken->nType = PH7_TK_COLON; /* Single colon */` |
|         - |  548 | `			}` |
|   1181951 |  549 | `			break;` |
|   4686519 |  550 | `		case ',': pToken->nType \|= PH7_TK_COMMA;  break; /* Comma is also an operator */` |
|  12428611 |  551 | `		case ';': pToken->nType = PH7_TK_SEMI;    break;` |
|         - |  552 | `			/* Handle combined operators [i.e: +=,===,!=== ...] */` |
|   4421346 |  553 | `		case '=':` |
|   8842697 |  554 | `			pToken->nType \|= PH7_TK_EQUAL;` |
|   8842697 |  555 | `			if( pStream->zText < pStream->zEnd ){` |
|   8842697 |  556 | `				if( pStream->zText[0] == '=' ){` |
|   1604017 |  557 | `					pToken->nType &= ~PH7_TK_EQUAL;` |
|         - |  558 | `					/* Current operator: == */` |
|   1604017 |  559 | `					pStream->zText++;` |
|   1604017 |  560 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  561 | `						/* Current operator: === */` |
|   1541533 |  562 | `						pStream->zText++;` |
|    770769 |  563 | `					}` |
|   8040691 |  564 | `				}else if( pStream->zText[0] == '>' ){` |
|         - |  565 | `					/* Array operator: => */` |
|    595835 |  566 | `					pToken->nType = PH7_TK_ARRAY_OP;` |
|    595835 |  567 | `					pStream->zText++;` |
|    297920 |  568 | `				}else{` |
|         - |  569 | `					/* TICKET 1433-0010: Reference operator '=&' */` |
|   6642855 |  570 | `					const unsigned char *zCur = pStream->zText;` |
|   6642855 |  571 | `					sxu32 nLine = 0;` |
|  13285515 |  572 | `					while( zCur < pStream->zEnd && zCur[0] < 0xc0 && SyisSpace(zCur[0]) ){` |
|   6642665 |  573 | `						if( zCur[0] == '\n' ){` |
|         5 |  574 | `							nLine++;` |
|         2 |  575 | `						}` |
|   6642665 |  576 | `						zCur++;` |
|         5 |  577 | `					}` |
|   6642855 |  578 | `					if( zCur < pStream->zEnd && zCur[0] == '&' ){` |
|         - |  579 | `						/* Current operator: =& */` |
|        92 |  580 | `						pToken->nType &= ~PH7_TK_EQUAL;` |
|        92 |  581 | `						SyStringInitFromBuf(pStr,"=&",sizeof("=&")-1);` |
|         - |  582 | `						/* Update token stream */` |
|        92 |  583 | `						pStream->zText = &zCur[1];` |
|        92 |  584 | `						pStream->nLine += nLine;` |
|        44 |  585 | `					}` |
|         - |  586 | `				}` |
|   4421346 |  587 | `			}` |
|   8842697 |  588 | `			break;` |
|    490934 |  589 | `		case '!':` |
|    981873 |  590 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  591 | `				/* Current operator: != */` |
|    476391 |  592 | `				pStream->zText++;` |
|    476391 |  593 | `				if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  594 | `					/* Current operator: !== */` |
|    455661 |  595 | `					pStream->zText++;` |
|    227828 |  596 | `				}` |
|    238193 |  597 | `			}` |
|    981873 |  598 | `			break;` |
|    366815 |  599 | `		case '&':` |
|    733635 |  600 | `			pToken->nType \|= PH7_TK_AMPER;` |
|    733635 |  601 | `			if( pStream->zText < pStream->zEnd ){` |
|    733635 |  602 | `				if( pStream->zText[0] == '&' ){` |
|    484881 |  603 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|         - |  604 | `					/* Current operator: && */` |
|    484881 |  605 | `					pStream->zText++;` |
|    491197 |  606 | `				}else if( pStream->zText[0] == '=' ){` |
|         7 |  607 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|         - |  608 | `					/* Current operator: &= */` |
|         7 |  609 | `					pStream->zText++;` |
|         3 |  610 | `				}` |
|    366815 |  611 | `			}` |
|    733635 |  612 | `			break;` |
|    244484 |  613 | `		case '\|':` |
|    488973 |  614 | `			if( pStream->zText < pStream->zEnd ){` |
|    488973 |  615 | `				if( pStream->zText[0] == '\|' ){` |
|         - |  616 | `					/* Current operator: \|\| */` |
|    385137 |  617 | `					pStream->zText++;` |
|    296407 |  618 | `				}else if( pStream->zText[0] == '=' ){` |
|         - |  619 | `					/* Current operator: \|= */` |
|     62113 |  620 | `					pStream->zText++;` |
|     72787 |  621 | `				}else if( pStream->zText[0] == '>' ){` |
|         - |  622 | `					/* Current operator: \|> (PHP 8.5 pipe) */` |
|        27 |  623 | `					pStream->zText++;` |
|        13 |  624 | `				}` |
|    244484 |  625 | `			}` |
|    488973 |  626 | `			break;` |
|    242831 |  627 | `		case '+':` |
|    485667 |  628 | `			if( pStream->zText < pStream->zEnd ){` |
|    485667 |  629 | `				if( pStream->zText[0] == '+' ){` |
|         - |  630 | `					/* Current operator: ++ */` |
|    199175 |  631 | `					pStream->zText++;` |
|    386082 |  632 | `				}else if( pStream->zText[0] == '=' ){` |
|         - |  633 | `					/* Current operator: += */` |
|     58045 |  634 | `					pStream->zText++;` |
|     29020 |  635 | `				}` |
|    242831 |  636 | `			}` |
|    485667 |  637 | `			break;` |
|   3117698 |  638 | `		case '-':` |
|   6235401 |  639 | `			if( pStream->zText < pStream->zEnd ){` |
|   6235401 |  640 | `				if( pStream->zText[0] == '-' ){` |
|         - |  641 | `					/* Current operator: -- */` |
|     33159 |  642 | `					pStream->zText++;` |
|   6218824 |  643 | `				}else if( pStream->zText[0] == '=' ){` |
|         - |  644 | `					/* Current operator: -= */` |
|        14 |  645 | `					pStream->zText++;` |
|   6202241 |  646 | `				}else if( pStream->zText[0] == '>' ){` |
|         - |  647 | `					/* Current operator: -> */` |
|   5944563 |  648 | `					pStream->zText++;` |
|   2972279 |  649 | `				}` |
|   3117698 |  650 | `			}` |
|   6235401 |  651 | `			break;` |
|     20922 |  652 | `		case '*':` |
|     41849 |  653 | `			if( pStream->zText < pStream->zEnd ){` |
|     41849 |  654 | `				if( pStream->zText[0] == '*' ){` |
|         - |  655 | `					/* Current operator: ** or **= */` |
|       137 |  656 | `					pStream->zText++;` |
|       137 |  657 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  658 | `						/* Current operator: **= */` |
|        23 |  659 | `						pStream->zText++;` |
|        12 |  660 | `					}` |
|     41781 |  661 | `				}else if( pStream->zText[0] == '=' ){` |
|         - |  662 | `					/* Current operator: *= */` |
|        27 |  663 | `					pStream->zText++;` |
|        12 |  664 | `				}` |
|     20922 |  665 | `			}` |
|     41849 |  666 | `			break;` |
|      2123 |  667 | `		case '/':` |
|      4251 |  668 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  669 | `				/* Current operator: /= */` |
|         9 |  670 | `				pStream->zText++;` |
|         4 |  671 | `			}` |
|      4251 |  672 | `			break;` |
|     18680 |  673 | `		case '%':` |
|     37365 |  674 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  675 | `				/* Current operator: %= */` |
|         9 |  676 | `				pStream->zText++;` |
|         4 |  677 | `			}` |
|     37365 |  678 | `			break;` |
|        11 |  679 | `		case '^':` |
|        23 |  680 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  681 | `				/* Current operator: ^= */` |
|         9 |  682 | `				pStream->zText++;` |
|         4 |  683 | `			}` |
|        23 |  684 | `			break;` |
|   1068361 |  685 | `		case '.':` |
|   2136727 |  686 | `			if( pStream->zText + 1 < pStream->zEnd && pStream->zText[0] == '.' && pStream->zText[1] == '.' ){` |
|         - |  687 | `				/* Ellipsis: ... */` |
|     29513 |  688 | `				pStream->zText += 2;` |
|     29513 |  689 | `				pToken->nType = PH7_TK_ELLIPSIS;` |
|   2121973 |  690 | `			}else if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  691 | `				/* Current operator: .= */` |
|    360373 |  692 | `				pStream->zText++;` |
|    180184 |  693 | `			}` |
|   2136727 |  694 | `			break;` |
|    215652 |  695 | `		case '<':` |
|    431309 |  696 | `			if( pStream->zText < pStream->zEnd ){` |
|    431309 |  697 | `				if( pStream->zText[0] == '<' ){` |
|         - |  698 | `					/* Current operator: << */` |
|       151 |  699 | `					pStream->zText++;` |
|       151 |  700 | `					if( pStream->zText < pStream->zEnd ){` |
|       151 |  701 | `						if( pStream->zText[0] == '=' ){` |
|         - |  702 | `							/* Current operator: <<= */` |
|         9 |  703 | `							pStream->zText++;` |
|       147 |  704 | `						}else if( pStream->zText[0] == '<' ){` |
|         - |  705 | `							/* Current Token: <<<  */` |
|       129 |  706 | `							pStream->zText++;` |
|         - |  707 | `							/* This may be the beginning of a Heredoc/Nowdoc string,try to delimit it */` |
|       129 |  708 | `							rc = LexExtractHeredoc(&(*pStream),&(*pToken));` |
|       129 |  709 | `							if( rc == SXRET_OK ){` |
|         - |  710 | `								/* Here/Now doc successfuly extracted */` |
|       129 |  711 | `								return SXRET_OK;` |
|         - |  712 | `							}` |
|       ! 0 |  713 | `						}` |
|        12 |  714 | `					}` |
|    431174 |  715 | `				}else if( pStream->zText[0] == '>' ){` |
|         - |  716 | `					/* Current operator: <> */` |
|         5 |  717 | `					pStream->zText++;` |
|    431161 |  718 | `				}else if( pStream->zText[0] == '=' ){` |
|         - |  719 | `					/* Current operator: <= or <=> */` |
|     70545 |  720 | `					pStream->zText++;` |
|     70545 |  721 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '>' ){` |
|         - |  722 | `						/* Current operator: <=> */` |
|     29075 |  723 | `						pStream->zText++;` |
|     14535 |  724 | `					}` |
|     35270 |  725 | `				}` |
|    215590 |  726 | `			}` |
|    431185 |  727 | `			break;` |
|    167827 |  728 | `		case '>':` |
|    335659 |  729 | `			if( pStream->zText < pStream->zEnd ){` |
|    335659 |  730 | `				if( pStream->zText[0] == '>' ){` |
|         - |  731 | `					/* Current operator: >> */` |
|     20725 |  732 | `					pStream->zText++;` |
|     20725 |  733 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  734 | `						/* Current operator: >>= */` |
|        11 |  735 | `						pStream->zText++;` |
|        10 |  736 | `					}` |
|    325299 |  737 | `				}else if( pStream->zText[0] == '=' ){` |
|         - |  738 | `					/* Current operator: >= */` |
|    111881 |  739 | `					pStream->zText++;` |
|     55938 |  740 | `				}` |
|    167827 |  741 | `			}` |
|    335659 |  742 | `			break;` |
|    316565 |  743 | `		case '?':` |
|    633135 |  744 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '?' ){` |
|         - |  745 | `				/* Null coalescing operator: ?? */` |
|     62407 |  746 | `				pStream->zText++;` |
|     62407 |  747 | `				if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  748 | `					/* Null coalescing assignment operator (PHP 7.4) */` |
|       100 |  749 | `					pStream->zText++;` |
|        48 |  750 | `				}` |
|    601934 |  751 | `			}else if( (pStream->zEnd - pStream->zText) >= 2` |
|    570733 |  752 | `				&& pStream->zText[0] == '-' && pStream->zText[1] == '>' ){` |
|         - |  753 | `				/* Nullsafe object operator (PHP 8.0): ?-> */` |
|       121 |  754 | `				pStream->zText += 2;` |
|        58 |  755 | `			}` |
|    633130 |  756 | `			break;` |
|     10492 |  757 | `		default:` |
|     20984 |  758 | `			break;` |
|         - |  759 | `		}` |
| 121245977 |  760 | `		if( pStr->nByte <= 0 ){` |
|         - |  761 | `			/* Record token length */` |
| 121245889 |  762 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  60622942 |  763 | `		}` |
| 121245977 |  764 | `		if( pToken->nType & PH7_TK_OP ){` |
|         - |  765 | `			const ph7_expr_op *pOp;` |
|         - |  766 | `			/* Check if the extracted token is an operator */` |
|  29416041 |  767 | `			pOp = PH7_ExprExtractOperator(pStr,(SyToken *)SySetPeek(pStream->pSet));` |
|  29416041 |  768 | `			if( pOp == 0 ){` |
|         - |  769 | `				/* Not an operator */` |
|       ! 0 |  770 | `				pToken->nType &= ~PH7_TK_OP;` |
|       ! 0 |  771 | `				if( pToken->nType <= 0 ){` |
|       ! 0 |  772 | `					pToken->nType = PH7_TK_OTHER;` |
|       ! 0 |  773 | `				}` |
|       ! 0 |  774 | `			}else{` |
|         - |  775 | `				/* Save the instance associated with this operator for later processing */` |
|  29416041 |  776 | `				pToken->pUserData = (void *)pOp;` |
|         - |  777 | `			}` |
|  14708018 |  778 | `		}` |
|         - |  779 | `	}` |
|         - |  780 | `	/* Tell the upper-layer to save the extracted token for later processing */` |
| 188312435 |  781 | `	return SXRET_OK;` |
|  99820705 |  782 | `}` |
|         - |  783 | `/* SPDX-SnippetBegin */` |
|         - |  784 | `/* SPDX-SnippetCopyrightText: SQLite mkkeywordhash.c (D. Richard Hipp and the SQLite authors <https://sqlite.org/>); adapted for the PH7 engine by Chems mrad */` |
|         - |  785 | `/* SPDX-License-Identifier: blessing */` |
|         - |  786 | `/***** This file contains automatically generated code ******` |
|         - |  787 | `**` |
|         - |  788 | `** The code in this file has been automatically generated by` |
|         - |  789 | `**` |
|         - |  790 | `**     $Header: /sqlite/sqlite/tool/mkkeywordhash.c` |
|         - |  791 | `**` |
|         - |  792 | `** Sligthly modified by Chems mrad <chm@symisc.net> for the PH7 engine.` |
|         - |  793 | `**` |
|         - |  794 | `** The code in this file implements a function that determines whether` |
|         - |  795 | `** or not a given identifier is really a PHP keyword.  The same thing` |
|         - |  796 | `** might be implemented more directly using a hand-written hash table.` |
|         - |  797 | `** But by using this automatically generated code, the size of the code` |
|         - |  798 | `** is substantially reduced.  This is important for embedded applications` |
|         - |  799 | `** on platforms with limited memory.` |
|         - |  800 | `*/` |
|         - |  801 | `/* Hash score: 103 */` |
|  54662599 |  802 | `static sxu32 KeywordCode(const char *z, int n){` |
|         - |  803 | `  /* zText[] encodes 532 bytes of keywords in 333 bytes */` |
|         - |  804 | `  /*   extendswitchprintegerequire_oncenddeclareturnamespacechobject      */` |
|         - |  805 | `  /*   hrowbooleandefaultrycaselfinalistaticlonewconstringlobaluse        */` |
|         - |  806 | `  /*   lseifloatvarrayANDIEchoUSECHOabstractclasscontinuendifunction      */` |
|         - |  807 | `  /*   diendwhilevaldoexitgotoimplementsinclude_oncemptyinstanceof        */` |
|         - |  808 | `  /*   interfacendforeachissetparentprivateprotectedpublicatchunset       */` |
|         - |  809 | `  /*   xorARRAYASArrayEXITUNSETXORbreak                                   */` |
|         - |  810 | `  static const char zText[332] = {` |
|         - |  811 | `    'e','x','t','e','n','d','s','w','i','t','c','h','p','r','i','n','t','e',` |
|         - |  812 | `    'g','e','r','e','q','u','i','r','e','_','o','n','c','e','n','d','d','e',` |
|         - |  813 | `    'c','l','a','r','e','t','u','r','n','a','m','e','s','p','a','c','e','c',` |
|         - |  814 | `    'h','o','b','j','e','c','t','h','r','o','w','b','o','o','l','e','a','n',` |
|         - |  815 | `    'd','e','f','a','u','l','t','r','y','c','a','s','e','l','f','i','n','a',` |
|         - |  816 | `    'l','i','s','t','a','t','i','c','l','o','n','e','w','c','o','n','s','t',` |
|         - |  817 | `    'r','i','n','g','l','o','b','a','l','u','s','e','l','s','e','i','f','l',` |
|         - |  818 | `    'o','a','t','v','a','r','r','a','y','A','N','D','I','E','c','h','o','U',` |
|         - |  819 | `    'S','E','C','H','O','a','b','s','t','r','a','c','t','c','l','a','s','s',` |
|         - |  820 | `    'c','o','n','t','i','n','u','e','n','d','i','f','u','n','c','t','i','o',` |
|         - |  821 | `    'n','d','i','e','n','d','w','h','i','l','e','v','a','l','d','o','e','x',` |
|         - |  822 | `    'i','t','g','o','t','o','i','m','p','l','e','m','e','n','t','s','i','n',` |
|         - |  823 | `    'c','l','u','d','e','_','o','n','c','e','m','p','t','y','i','n','s','t',` |
|         - |  824 | `    'a','n','c','e','o','f','i','n','t','e','r','f','a','c','e','n','d','f',` |
|         - |  825 | `    'o','r','e','a','c','h','i','s','s','e','t','p','a','r','e','n','t','p',` |
|         - |  826 | `    'r','i','v','a','t','e','p','r','o','t','e','c','t','e','d','p','u','b',` |
|         - |  827 | `    'l','i','c','a','t','c','h','u','n','s','e','t','x','o','r','A','R','R',` |
|         - |  828 | `    'A','Y','A','S','A','r','r','a','y','E','X','I','T','U','N','S','E','T',` |
|         - |  829 | `    'X','O','R','b','r','e','a','k'` |
|         - |  830 | `  };` |
|         - |  831 | `  static const unsigned char aHash[151] = {` |
|         - |  832 | `       0,   0,   4,  83,   0,  61,  39,  12,   0,  33,  77,   0,  48,` |
|         - |  833 | `       0,   2,  65,  67,   0,   0,   0,  47,   0,   0,  40,   0,  15,` |
|         - |  834 | `      74,   0,  51,   0,  76,   0,   0,  20,   0,   0,   0,  50,   0,` |
|         - |  835 | `      80,  34,   0,  36,   0,   0,  64,  16,   0,   0,  17,   0,   1,` |
|         - |  836 | `      19,  84,  66,   0,  43,  45,  78,   0,   0,  53,  56,   0,   0,` |
|         - |  837 | `       0,  23,  49,   0,   0,  13,  31,  54,   7,   0,   0,  25,   0,` |
|         - |  838 | `      72,  14,   0,  71,   0,  38,   6,   0,   0,   0,  73,   0,   0,` |
|         - |  839 | `       3,   0,  41,   5,  52,  57,  32,   0,  60,  63,   0,  69,  82,` |
|         - |  840 | `      30,   0,  79,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,` |
|         - |  841 | `       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  81,   0,   0,` |
|         - |  842 | `      62,   0,  11,   0,   0,  58,   0,   0,   0,   0,  59,  75,   0,` |
|         - |  843 | `       0,   0,   0,   0,   0,  35,  27,   0` |
|         - |  844 | `  };` |
|         - |  845 | `  static const unsigned char aNext[84] = {` |
|         - |  846 | `       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,` |
|         - |  847 | `       0,   0,   8,   0,   0,   0,  10,   0,   0,   0,   0,   0,   0,` |
|         - |  848 | `       0,   0,   0,   0,  28,   0,   0,   0,   0,   0,   0,   0,   0,` |
|         - |  849 | `       0,   0,   0,   0,   0,  44,   0,  18,   0,   0,   0,   0,   0,` |
|         - |  850 | `       0,  46,   0,  29,   0,   0,   0,  22,   0,   0,   0,   0,  26,` |
|         - |  851 | `       0,  21,  24,   0,   0,  68,   0,   0,   9,  37,   0,   0,   0,` |
|         - |  852 | `      42,   0,   0,   0,  70,  55` |
|         - |  853 | `  };` |
|         - |  854 | `  static const unsigned char aLen[84] = {` |
|         - |  855 | `       7,   9,   6,   5,   7,  12,   7,   2,  10,   7,   6,   9,   4,` |
|         - |  856 | `       6,   5,   7,   4,   3,   7,   3,   4,   4,   5,   4,   6,   5,` |
|         - |  857 | `       2,   3,   5,   6,   6,   3,   6,   4,   2,   5,   3,   5,   3,` |
|         - |  858 | `       3,   4,   3,   4,   8,   5,   2,   8,   5,   8,   3,   8,   5,` |
|         - |  859 | `       4,   2,   4,   4,  10,  12,   7,   5,  10,   9,   3,   6,  10,` |
|         - |  860 | `       3,   7,   2,   5,   6,   7,   9,   6,   5,   5,   3,   5,   2,` |
|         - |  861 | `       5,   4,   5,   3,   2,   5` |
|         - |  862 | `  };` |
|         - |  863 | `  static const sxu16 aOffset[84] = {` |
|         - |  864 | `       0,   3,   6,  12,  14,  20,  20,  21,  31,  34,  39,  44,  52,` |
|         - |  865 | `      55,  60,  65,  65,  70,  72,  78,  81,  83,  86,  90,  92,  97,` |
|         - |  866 | `     100, 100, 103, 106, 111, 117, 119, 119, 123, 124, 129, 130, 135,` |
|         - |  867 | `     137, 139, 143, 145, 149, 157, 159, 162, 169, 173, 181, 183, 186,` |
|         - |  868 | `     190, 194, 196, 200, 204, 214, 214, 225, 230, 240, 240, 248, 248,` |
|         - |  869 | `     251, 251, 252, 258, 263, 269, 276, 285, 290, 295, 300, 303, 308,` |
|         - |  870 | `     310, 315, 319, 324, 325, 327` |
|         - |  871 | `  };` |
|         - |  872 | `  static const sxu32 aCode[84] = {` |
|         - |  873 | `    PH7_TKWRD_EXTENDS,   PH7_TKWRD_ENDSWITCH,   PH7_TKWRD_SWITCH,    PH7_TKWRD_PRINT,   PH7_TKWRD_INT,` |
|         - |  874 | `    PH7_TKWRD_REQONCE,   PH7_TKWRD_REQUIRE,     PH7_TK_ID /* 'eq' PH7-ism removed */, PH7_TKWRD_ENDDEC, PH7_TKWRD_DECLARE,` |
|         - |  875 | `    PH7_TKWRD_RETURN,    PH7_TKWRD_NAMESPACE,   PH7_TKWRD_ECHO,      PH7_TKWRD_OBJECT,    PH7_TKWRD_THROW,` |
|         - |  876 | `    PH7_TKWRD_BOOL,      PH7_TKWRD_BOOL,        PH7_TKWRD_AND,       PH7_TKWRD_DEFAULT,   PH7_TKWRD_TRY,` |
|         - |  877 | `    PH7_TKWRD_CASE,      PH7_TKWRD_SELF,        PH7_TKWRD_FINAL,     PH7_TKWRD_LIST,      PH7_TKWRD_STATIC,` |
|         - |  878 | `    PH7_TKWRD_CLONE,     PH7_TK_ID /* 'ne' PH7-ism removed */, PH7_TKWRD_NEW,  PH7_TKWRD_CONST,     PH7_TKWRD_STRING,` |
|         - |  879 | `    PH7_TKWRD_GLOBAL,    PH7_TKWRD_USE,         PH7_TKWRD_ELIF,      PH7_TKWRD_ELSE,      PH7_TKWRD_IF,` |
|         - |  880 | `    PH7_TKWRD_FLOAT,     PH7_TKWRD_VAR,         PH7_TKWRD_ARRAY,     PH7_TKWRD_AND,       PH7_TKWRD_DIE,` |
|         - |  881 | `    PH7_TKWRD_ECHO,      PH7_TKWRD_USE,         PH7_TKWRD_ECHO,      PH7_TKWRD_ABSTRACT,  PH7_TKWRD_CLASS,` |
|         - |  882 | `    PH7_TKWRD_AS,        PH7_TKWRD_CONTINUE,    PH7_TKWRD_ENDIF,     PH7_TKWRD_FUNCTION,  PH7_TKWRD_DIE,` |
|         - |  883 | `    PH7_TKWRD_ENDWHILE,  PH7_TKWRD_WHILE,       PH7_TKWRD_EVAL,      PH7_TKWRD_DO,        PH7_TKWRD_EXIT,` |
|         - |  884 | `    PH7_TKWRD_GOTO,      PH7_TKWRD_IMPLEMENTS,  PH7_TKWRD_INCONCE,   PH7_TKWRD_INCLUDE,   PH7_TKWRD_EMPTY,` |
|         - |  885 | `    PH7_TKWRD_INSTANCEOF,PH7_TKWRD_INTERFACE,   PH7_TKWRD_INT,       PH7_TKWRD_ENDFOR,    PH7_TKWRD_END4EACH,` |
|         - |  886 | `    PH7_TKWRD_FOR,       PH7_TKWRD_FOREACH,     PH7_TKWRD_OR,        PH7_TKWRD_ISSET,     PH7_TKWRD_PARENT,` |
|         - |  887 | `    PH7_TKWRD_PRIVATE,   PH7_TKWRD_PROTECTED,   PH7_TKWRD_PUBLIC,    PH7_TKWRD_CATCH,     PH7_TKWRD_UNSET,` |
|         - |  888 | `    PH7_TKWRD_XOR,       PH7_TKWRD_ARRAY,       PH7_TKWRD_AS,        PH7_TKWRD_ARRAY,     PH7_TKWRD_EXIT,` |
|         - |  889 | `    PH7_TKWRD_UNSET,     PH7_TKWRD_XOR,         PH7_TKWRD_OR,        PH7_TKWRD_BREAK` |
|         - |  890 | `  };` |
|         - |  891 | `  int h, i;` |
|  54662599 |  892 | `  if( n<2 ) return PH7_TK_ID;` |
|         - |  893 | ``  /* Hash through UNSIGNED bytes: `char` is signed on most targets, so an`` |
|         - |  894 | `   * identifier carrying a high byte (php allows 0x80-0xFF in identifiers, and` |
|         - |  895 | ``   * every UTF-8 name has them) made the xor negative, and C's `%` keeps that`` |
|         - |  896 | `   * sign — aHash[-46] read off the front of the table. ASCII is unaffected, so` |
|         - |  897 | `   * the generated keyword buckets still resolve exactly as before. */` |
|  54662599 |  898 | `  h = (int)(((sxu32)(sxu8)z[0]*4) ^ ((sxu32)(sxu8)z[n-1]*3) ^ (sxu32)n) % 151;` |
|  83074071 |  899 | `  for(i=((int)aHash[h])-1; i>=0; i=((int)aNext[i])-1){` |
|  50584547 |  900 | `    if( (int)aLen[i]==n && SyMemcmp(&zText[aOffset[i]],z,n)==0 ){` |
|         - |  901 | `       /* PH7_TKWRD_EXTENDS */` |
|         - |  902 | `       /* PH7_TKWRD_ENDSWITCH */` |
|         - |  903 | `       /* PH7_TKWRD_SWITCH */` |
|         - |  904 | `       /* PH7_TKWRD_PRINT */` |
|         - |  905 | `       /* PH7_TKWRD_INT */` |
|         - |  906 | `       /* PH7_TKWRD_REQONCE */` |
|         - |  907 | `       /* PH7_TKWRD_REQUIRE */` |
|         - |  908 | `       /* PH7_TK_ID */` |
|         - |  909 | `       /* PH7_TKWRD_ENDDEC */` |
|         - |  910 | `       /* PH7_TKWRD_DECLARE */` |
|         - |  911 | `       /* PH7_TKWRD_RETURN */` |
|         - |  912 | `       /* PH7_TKWRD_NAMESPACE */` |
|         - |  913 | `       /* PH7_TKWRD_ECHO */` |
|         - |  914 | `       /* PH7_TKWRD_OBJECT */` |
|         - |  915 | `       /* PH7_TKWRD_THROW */` |
|         - |  916 | `       /* PH7_TKWRD_BOOL */` |
|         - |  917 | `       /* PH7_TKWRD_BOOL */` |
|         - |  918 | `       /* PH7_TKWRD_AND */` |
|         - |  919 | `       /* PH7_TKWRD_DEFAULT */` |
|         - |  920 | `       /* PH7_TKWRD_TRY */` |
|         - |  921 | `       /* PH7_TKWRD_CASE */` |
|         - |  922 | `       /* PH7_TKWRD_SELF */` |
|         - |  923 | `       /* PH7_TKWRD_FINAL */` |
|         - |  924 | `       /* PH7_TKWRD_LIST */` |
|         - |  925 | `       /* PH7_TKWRD_STATIC */` |
|         - |  926 | `       /* PH7_TKWRD_CLONE */` |
|         - |  927 | `       /* PH7_TK_ID */` |
|         - |  928 | `       /* PH7_TKWRD_NEW */` |
|         - |  929 | `       /* PH7_TKWRD_CONST */` |
|         - |  930 | `       /* PH7_TKWRD_STRING */` |
|         - |  931 | `       /* PH7_TKWRD_GLOBAL */` |
|         - |  932 | `       /* PH7_TKWRD_USE */` |
|         - |  933 | `       /* PH7_TKWRD_ELIF */` |
|         - |  934 | `       /* PH7_TKWRD_ELSE */` |
|         - |  935 | `       /* PH7_TKWRD_IF */` |
|         - |  936 | `       /* PH7_TKWRD_FLOAT */` |
|         - |  937 | `       /* PH7_TKWRD_VAR */` |
|         - |  938 | `       /* PH7_TKWRD_ARRAY */` |
|         - |  939 | `       /* PH7_TKWRD_AND */` |
|         - |  940 | `       /* PH7_TKWRD_DIE */` |
|         - |  941 | `       /* PH7_TKWRD_ECHO */` |
|         - |  942 | `       /* PH7_TKWRD_USE */` |
|         - |  943 | `       /* PH7_TKWRD_ECHO */` |
|         - |  944 | `       /* PH7_TKWRD_ABSTRACT */` |
|         - |  945 | `       /* PH7_TKWRD_CLASS */` |
|         - |  946 | `       /* PH7_TKWRD_AS */` |
|         - |  947 | `       /* PH7_TKWRD_CONTINUE */` |
|         - |  948 | `       /* PH7_TKWRD_ENDIF */` |
|         - |  949 | `       /* PH7_TKWRD_FUNCTION */` |
|         - |  950 | `       /* PH7_TKWRD_DIE */` |
|         - |  951 | `       /* PH7_TKWRD_ENDWHILE */` |
|         - |  952 | `       /* PH7_TKWRD_WHILE */` |
|         - |  953 | `       /* PH7_TKWRD_EVAL */` |
|         - |  954 | `       /* PH7_TKWRD_DO */` |
|         - |  955 | `       /* PH7_TKWRD_EXIT */` |
|         - |  956 | `       /* PH7_TKWRD_GOTO */` |
|         - |  957 | `       /* PH7_TKWRD_IMPLEMENTS */` |
|         - |  958 | `       /* PH7_TKWRD_INCONCE */` |
|         - |  959 | `       /* PH7_TKWRD_INCLUDE */` |
|         - |  960 | `       /* PH7_TKWRD_EMPTY */` |
|         - |  961 | `       /* PH7_TKWRD_INSTANCEOF */` |
|         - |  962 | `       /* PH7_TKWRD_INTERFACE */` |
|         - |  963 | `       /* PH7_TKWRD_INT */` |
|         - |  964 | `       /* PH7_TKWRD_ENDFOR */` |
|         - |  965 | `       /* PH7_TKWRD_END4EACH */` |
|         - |  966 | `       /* PH7_TKWRD_FOR */` |
|         - |  967 | `       /* PH7_TKWRD_FOREACH */` |
|         - |  968 | `       /* PH7_TKWRD_OR */` |
|         - |  969 | `       /* PH7_TKWRD_ISSET */` |
|         - |  970 | `       /* PH7_TKWRD_PARENT */` |
|         - |  971 | `       /* PH7_TKWRD_PRIVATE */` |
|         - |  972 | `       /* PH7_TKWRD_PROTECTED */` |
|         - |  973 | `       /* PH7_TKWRD_PUBLIC */` |
|         - |  974 | `       /* PH7_TKWRD_CATCH */` |
|         - |  975 | `       /* PH7_TKWRD_UNSET */` |
|         - |  976 | `       /* PH7_TKWRD_XOR */` |
|         - |  977 | `       /* PH7_TKWRD_ARRAY */` |
|         - |  978 | `       /* PH7_TKWRD_AS */` |
|         - |  979 | `       /* PH7_TKWRD_ARRAY */` |
|         - |  980 | `       /* PH7_TKWRD_EXIT */` |
|         - |  981 | `       /* PH7_TKWRD_UNSET */` |
|         - |  982 | `       /* PH7_TKWRD_XOR */` |
|         - |  983 | `       /* PH7_TKWRD_OR */` |
|         - |  984 | `       /* PH7_TKWRD_BREAK */` |
|  22173075 |  985 | `      return aCode[i];` |
|         - |  986 | `    }` |
|  14205741 |  987 | `  }` |
|         - |  988 | `  /* Linear fallback for keywords not in the auto-generated hash table */` |
|  32489529 |  989 | `  if( n==5 && SyMemcmp(z,"trait",5)==0 ) return PH7_TKWRD_TRAIT;` |
|  32481103 |  990 | `  if( n==9 && SyMemcmp(z,"insteadof",9)==0 ) return PH7_TKWRD_INSTEADOF;` |
|  32481095 |  991 | `  if( n==7 && SyMemcmp(z,"finally",7)==0 ) return PH7_TKWRD_FINALLY;` |
|  32480865 |  992 | `  if( n==5 && SyMemcmp(z,"yield",5)==0 ) return PH7_TKWRD_YIELD;` |
|  32463881 |  993 | `  if( n==5 && SyMemcmp(z,"match",5)==0 ) return PH7_TKWRD_MATCH;` |
|  32451347 |  994 | `  if( n==2 && SyMemcmp(z,"fn",2)==0 ) return PH7_TKWRD_FN;   /* PHP 7.4 arrow functions */` |
|  32450325 |  995 | `  return PH7_TK_ID;` |
|  27331302 |  996 | `}` |
|         - |  997 | `/* --- End of Automatically generated code --- */` |
|         - |  998 | `/* SPDX-SnippetEnd */` |
|         - |  999 | `/*` |
|         - | 1000 | ` * Keyword lookup as php does it: CASE-INSENSITIVELY. 'IF', 'Function' and 'NEW'` |
|         - | 1001 | ` * are the very same tokens as 'if', 'function' and 'new', so the generated table` |
|         - | 1002 | ` * above — whose hash buckets and SyMemcmp() rows are byte-exact lower case — is` |
|         - | 1003 | ` * probed through an ASCII-folded COPY of the identifier. KeywordCode() itself` |
|         - | 1004 | ` * stays byte-exact so the generated code needs no regeneration.` |
|         - | 1005 | ` *` |
|         - | 1006 | ` * The fold is ASCII-only on purpose: libc tolower() follows LC_CTYPE (a tr_TR` |
|         - | 1007 | ` * embedder would stop recognising 'IF') where php's lexer is locale-independent,` |
|         - | 1008 | ` * and identifier bytes >= 0x80 — php allows them, and every UTF-8 name has` |
|         - | 1009 | ` * them — must pass through untouched.` |
|         - | 1010 | ` *` |
|         - | 1011 | ` * A handful of UPPER-case rows in the table ('ARRAY', 'AS', 'EXIT', 'UNSET',` |
|         - | 1012 | ` * 'XOR', 'AND', 'OR', 'ECHO', 'Echo', 'Array', 'USE') are PH7's old partial hack` |
|         - | 1013 | ` * for this same problem. Each has a lower-case twin, so folding makes them` |
|         - | 1014 | ` * unreachable-but-harmless rather than wrong.` |
|         - | 1015 | ` */` |
|  67066458 | 1016 | `static sxu32 KeywordCodeCI(const char *zRaw, int n)` |
|         5 | 1017 | `{` |
|         - | 1018 | `	/* Longest row in the table above: 'require_once'/'include_once' (12 bytes) */` |
|         - | 1019 | `	char zFold[12];` |
|         - | 1020 | `	int i;` |
|  67066463 | 1021 | `	if( n < 2 \|\| n > (int)sizeof(zFold) ){` |
|         - | 1022 | `		/* Too short or too long to be any keyword: skip the fold and the probe */` |
|  12403869 | 1023 | `		return PH7_TK_ID;` |
|         - | 1024 | `	}` |
| 356124055 | 1025 | `	for( i = 0 ; i < n ; ++i ){` |
| 301461461 | 1026 | `		unsigned char c = (unsigned char)zRaw[i];` |
| 301461461 | 1027 | `		zFold[i] = (char)((c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c);` |
| 150730733 | 1028 | `	}` |
|  54662599 | 1029 | `	return KeywordCode(zFold,n);` |
|  33533234 | 1030 | `}` |
|         - | 1031 | `/*` |
|         - | 1032 | ` * Extract a heredoc/nowdoc text from a raw PHP input.` |
|         - | 1033 | ` * According to the PHP language reference manual:` |
|         - | 1034 | ` *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier` |
|         - | 1035 | ` *  is provided, then a newline. The string itself follows, and then the same identifier again` |
|         - | 1036 | ` *  to close the quotation.` |
|         - | 1037 | ` *  The closing identifier must begin in the first column of the line. Also, the identifier must` |
|         - | 1038 | ` *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric` |
|         - | 1039 | ` *  characters and underscores, and must start with a non-digit character or underscore.` |
|         - | 1040 | ` *  Heredoc text behaves just like a double-quoted string, without the double quotes.` |
|         - | 1041 | ` *  This means that quotes in a heredoc do not need to be escaped, but the escape codes listed` |
|         - | 1042 | ` *  above can still be used. Variables are expanded, but the same care must be taken when expressing` |
|         - | 1043 | ` *  complex variables inside a heredoc as with strings.` |
|         - | 1044 | ` *  Nowdocs are to single-quoted strings what heredocs are to double-quoted strings.` |
|         - | 1045 | ` *  A nowdoc is specified similarly to a heredoc, but no parsing is done inside a nowdoc.` |
|         - | 1046 | ` *  The construct is ideal for embedding PHP code or other large blocks of text without the need` |
|         - | 1047 | ` *  for escaping. It shares some features in common with the SGML <![CDATA[ ]]> construct, in that` |
|         - | 1048 | ` *  it declares a block of text which is not for parsing.` |
|         - | 1049 | ` *  A nowdoc is identified with the same <<< sequence used for heredocs, but the identifier which follows` |
|         - | 1050 | ` *  is enclosed in single quotes, e.g. <<<'EOT'. All the rules for heredoc identifiers also apply to nowdoc` |
|         - | 1051 | ` *  identifiers, especially those regarding the appearance of the closing identifier.` |
|         - | 1052 | ` * Symisc Extension:` |
|         - | 1053 | ` * The closing delimiter can now start with a digit or undersocre or it can be an UTF-8 stream.` |
|         - | 1054 | ` * Example:` |
|         - | 1055 | ` *  <<<123` |
|         - | 1056 | ` *    HEREDOC Here` |
|         - | 1057 | ` * 123` |
|         - | 1058 | ` *  or` |
|         - | 1059 | ` *  <<<___` |
|         - | 1060 | ` *   HEREDOC Here` |
|         - | 1061 | ` *  ___` |
|         - | 1062 | ` */` |
|       124 | 1063 | `static sxi32 LexExtractHeredoc(SyStream *pStream,SyToken *pToken)` |
|         5 | 1064 | `{` |
|       129 | 1065 | `	const unsigned char *zIn  = pStream->zText;` |
|       129 | 1066 | `	const unsigned char *zEnd = pStream->zEnd;` |
|         - | 1067 | `	const unsigned char *zPtr;` |
|       129 | 1068 | `	sxu8 bNowDoc = FALSE;` |
|         - | 1069 | `	SyString sDelim;` |
|         - | 1070 | `	SyString sStr;` |
|         - | 1071 | `	/* Jump leading white spaces */` |
|       141 | 1072 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|        13 | 1073 | `		zIn++;` |
|         1 | 1074 | `	}` |
|       129 | 1075 | `	if( zIn >= zEnd ){` |
|         - | 1076 | `		/* A simple symbol,return immediately */` |
|       ! 0 | 1077 | `		return SXERR_CONTINUE;` |
|         - | 1078 | `	}` |
|       129 | 1079 | `	if( zIn[0] == '\'' \|\| zIn[0] == '"' ){` |
|         - | 1080 | `		/* Make sure we are dealing with a nowdoc */` |
|        56 | 1081 | `		bNowDoc =  zIn[0] == '\'' ? TRUE : FALSE;` |
|        56 | 1082 | `		zIn++;` |
|        26 | 1083 | `	}` |
|       129 | 1084 | `	if( zIn[0] < 0xc0 && !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|         - | 1085 | `		/* Invalid delimiter,return immediately */` |
|       ! 0 | 1086 | `		return SXERR_CONTINUE;` |
|         - | 1087 | `	}` |
|         - | 1088 | `	/* Isolate the identifier */` |
|       129 | 1089 | `	sDelim.zString = (const char *)zIn;` |
|       132 | 1090 | `	for(;;){` |
|       269 | 1091 | `		zPtr = zIn;` |
|         - | 1092 | `		/* Skip alphanumeric stream */` |
|       843 | 1093 | `		while( zPtr < zEnd && zPtr[0] < 0xc0 && (SyisAlphaNum(zPtr[0]) \|\| zPtr[0] == '_') ){` |
|       447 | 1094 | `			zPtr++;` |
|         5 | 1095 | `		}` |
|       269 | 1096 | `		if( zPtr < zEnd && zPtr[0] >= 0xc0 ){` |
|        19 | 1097 | `			zPtr++;` |
|         - | 1098 | `			/* UTF-8 stream */` |
|        37 | 1099 | `			while( zPtr < zEnd && ((zPtr[0] & 0xc0) == 0x80) ){` |
|        19 | 1100 | `				zPtr++;` |
|         1 | 1101 | `			}` |
|         9 | 1102 | `		}` |
|       269 | 1103 | `		if( zPtr == zIn ){` |
|         - | 1104 | `			/* Not an UTF-8 or alphanumeric stream */` |
|       129 | 1105 | `			break;` |
|         - | 1106 | `		}` |
|         - | 1107 | `		/* Synchronize pointers */` |
|       145 | 1108 | `		zIn = zPtr;` |
|         5 | 1109 | `	}` |
|         - | 1110 | `	/* Get the identifier length */` |
|       129 | 1111 | `	sDelim.nByte = (sxu32)((const char *)zIn-sDelim.zString);` |
|       129 | 1112 | `	if( zIn[0] == '"' \|\| (bNowDoc && zIn[0] == '\'') ){` |
|         - | 1113 | `		/* Jump the trailing single quote */` |
|        56 | 1114 | `		zIn++;` |
|        26 | 1115 | `	}` |
|         - | 1116 | `	/* Jump trailing white spaces */` |
|       129 | 1117 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|       ! 0 | 1118 | `		zIn++;` |
|       ! 0 | 1119 | `	}` |
|       129 | 1120 | `	if( sDelim.nByte <= 0 \|\| zIn >= zEnd \|\| zIn[0] != '\n' ){` |
|         - | 1121 | `		/* Invalid syntax */` |
|       ! 0 | 1122 | `		return SXERR_CONTINUE;` |
|         - | 1123 | `	}` |
|       129 | 1124 | `	pStream->nLine++; /* Increment line counter */` |
|       129 | 1125 | `	zIn++;` |
|         - | 1126 | `	/* Isolate the delimited string */` |
|       129 | 1127 | `	sStr.zString = (const char *)zIn;` |
|         - | 1128 | `	/* PHP 7.3 flexible heredoc/nowdoc: the closing marker may be preceded` |
|         - | 1129 | `	 * by whitespace (spaces/tabs), and may be followed by any non-identifier` |
|         - | 1130 | `	 * character. The indent count is recorded in pToken->pUserData and the` |
|         - | 1131 | `	 * compile phase strips it from each body line. */` |
|         - | 1132 | `	{` |
|       129 | 1133 | `		const unsigned char *zMarkerLine = zIn; /* Start of marker's line (set on match) */` |
|       129 | 1134 | `		sxu32 nIndent = 0;` |
|       296 | 1135 | `		for(;;){` |
|       363 | 1136 | `			const unsigned char *zLineStart = zIn;` |
|         - | 1137 | `			/* Skip leading space/tab on this line */` |
|      1020 | 1138 | `			while( zIn < zEnd && (zIn[0] == ' ' \|\| zIn[0] == '\t') ){` |
|       483 | 1139 | `				zIn++;` |
|         5 | 1140 | `			}` |
|       358 | 1141 | `			if( (sxu32)(zEnd - zIn) >= sDelim.nByte` |
|       362 | 1142 | `				&& SyMemcmp((const void *)sDelim.zString,(const void *)zIn,sDelim.nByte) == 0 ){` |
|         - | 1143 | `				int bIdentCont;` |
|       127 | 1144 | `				zPtr = &zIn[sDelim.nByte];` |
|         - | 1145 | `				/* Disambiguate: next byte must not continue an identifier.` |
|         - | 1146 | `				 * A leading byte >= 0xc0 starts a multi-byte UTF-8 sequence,` |
|         - | 1147 | `				 * which PHP identifiers may contain, so treat it as ident. */` |
|       127 | 1148 | `				if( zPtr >= zEnd ){` |
|       ! 0 | 1149 | `					bIdentCont = 0;` |
|       127 | 1150 | `				}else if( zPtr[0] >= 0xc0 ){` |
|       ! 0 | 1151 | `					bIdentCont = 1;` |
|       ! 0 | 1152 | `				}else{` |
|       127 | 1153 | `					bIdentCont = (SyisAlphaNum(zPtr[0]) \|\| zPtr[0] == '_');` |
|         - | 1154 | `				}` |
|       127 | 1155 | `				if( !bIdentCont ){` |
|         - | 1156 | `					/* Closing marker found */` |
|       127 | 1157 | `					nIndent = (sxu32)(zIn - zLineStart);` |
|       127 | 1158 | `					zMarkerLine = zLineStart;` |
|       127 | 1159 | `					pStream->zText = zPtr; /* Cursor right after identifier */` |
|       127 | 1160 | `					break;` |
|         - | 1161 | `				}` |
|       ! 0 | 1162 | `			}` |
|         - | 1163 | `			/* Not the closing marker on this line; walk to next newline */` |
|      5249 | 1164 | `			while( zIn < zEnd && zIn[0] != '\n' ){` |
|      5013 | 1165 | `				zIn++;` |
|         5 | 1166 | `			}` |
|       241 | 1167 | `			if( zIn >= zEnd ){` |
|         - | 1168 | `				/* End of input without finding the closing marker */` |
|         3 | 1169 | `				pStream->zText = pStream->zEnd;` |
|         3 | 1170 | `				zMarkerLine = zIn;` |
|         3 | 1171 | `				break;` |
|         - | 1172 | `			}` |
|       239 | 1173 | `			pStream->nLine++;` |
|       239 | 1174 | `			zIn++;` |
|         5 | 1175 | `		}` |
|         - | 1176 | `		/* Body runs from sStr.zString up to just before the marker line */` |
|       129 | 1177 | `		sStr.nByte = (sxu32)((const char *)zMarkerLine - sStr.zString);` |
|       129 | 1178 | `		pToken->nType = bNowDoc ? PH7_TK_NOWDOC : PH7_TK_HEREDOC;` |
|       129 | 1179 | `		SyStringDupPtr(&pToken->sData,&sStr);` |
|         - | 1180 | `		/* Strip exactly one line terminator that precedes the marker's line. */` |
|       124 | 1181 | `		if( pToken->sData.nByte > 0` |
|       125 | 1182 | `			&& pToken->sData.zString[pToken->sData.nByte - 1] == '\n' ){` |
|       119 | 1183 | `			pToken->sData.nByte--;` |
|       114 | 1184 | `			if( pToken->sData.nByte > 0` |
|       119 | 1185 | `				&& pToken->sData.zString[pToken->sData.nByte - 1] == '\r' ){` |
|       ! 0 | 1186 | `				pToken->sData.nByte--;` |
|       ! 0 | 1187 | `			}` |
|        57 | 1188 | `		}` |
|       129 | 1189 | `		pToken->pUserData = SX_INT_TO_PTR(nIndent);` |
|         - | 1190 | `	}` |
|         - | 1191 | `	/* All done */` |
|       129 | 1192 | `	return SXRET_OK;` |
|        67 | 1193 | `}` |
|         - | 1194 | `/*` |
|         - | 1195 | ` * Tokenize a raw PHP input.` |
|         - | 1196 | ` * This is the public tokenizer called by most code generator routines.` |
|         - | 1197 | ` */` |
|    112228 | 1198 | `PH7_PRIVATE sxi32 PH7_TokenizePHP(const char *zInput,sxu32 nLen,sxu32 nLineStart,SySet *pOut,SySet *pTrivia)` |
|         5 | 1199 | `{` |
|         - | 1200 | `	SyLex sLexer;` |
|         - | 1201 | `	sxi32 rc;` |
|         - | 1202 | `	/* Defense-in-depth cap for internal tokenizer calls that bypass ph7_compile() */` |
|    112233 | 1203 | `	if( nLen > PH7_MAX_INPUT_SIZE ){` |
|       ! 0 | 1204 | `		return SXERR_LIMIT;` |
|         - | 1205 | `	}` |
|         - | 1206 | `	/* Initialize the lexer. pTrivia (may be NULL = discard) rides as the` |
|         - | 1207 | `	 * tokenizer callback's user data: doc-comments (and later attribute` |
|         - | 1208 | `	 * groups) are recorded there instead of entering the token stream. */` |
|    112233 | 1209 | `	rc = SyLexInit(&sLexer,&(*pOut),TokenizePHP,pTrivia);` |
|    112233 | 1210 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 1211 | `		return rc;` |
|         - | 1212 | `	}` |
|    112233 | 1213 | `	sLexer.sStream.nLine = nLineStart;` |
|         - | 1214 | `	/* Tokenize input */` |
|    112233 | 1215 | `	rc = SyLexTokenizeInput(&sLexer,zInput,nLen,0,0,0);` |
|         - | 1216 | `	/* Release the lexer */` |
|    112233 | 1217 | `	SyLexRelease(&sLexer);` |
|         - | 1218 | `	/* Tokenization result */` |
|    112233 | 1219 | `	return rc;` |
|     56119 | 1220 | `}` |
|         - | 1221 | `/*` |
|         - | 1222 | ` * High level public tokenizer.` |
|         - | 1223 | ` *  Tokenize the input into PHP tokens and raw tokens [i.e: HTML,XML,Raw text...].` |
|         - | 1224 | ` * According to the PHP language reference manual` |
|         - | 1225 | ` *   When PHP parses a file, it looks for opening and closing tags, which tell PHP` |
|         - | 1226 | ` *   to start and stop interpreting the code between them. Parsing in this manner allows` |
|         - | 1227 | ` *   PHP to be embedded in all sorts of different documents, as everything outside of a pair` |
|         - | 1228 | ` *   of opening and closing tags is ignored by the PHP parser. Most of the time you will see` |
|         - | 1229 | ` *   PHP embedded in HTML documents, as in this example.` |
|         - | 1230 | ` *   <?php echo 'While this is going to be parsed.'; ?>` |
|         - | 1231 | ` *   <p>This will also be ignored.</p>` |
|         - | 1232 | ` *   You can also use more advanced structures:` |
|         - | 1233 | ` *   Example #1 Advanced escaping` |
|         - | 1234 | ` * <?php` |
|         - | 1235 | ` * if ($expression) {` |
|         - | 1236 | ` *   ?>` |
|         - | 1237 | ` *   <strong>This is true.</strong>` |
|         - | 1238 | ` *   <?php` |
|         - | 1239 | ` * } else {` |
|         - | 1240 | ` *   ?>` |
|         - | 1241 | ` *   <strong>This is false.</strong>` |
|         - | 1242 | ` *   <?php` |
|         - | 1243 | ` * }` |
|         - | 1244 | ` * ?>` |
|         - | 1245 | ` * This works as expected, because when PHP hits the ?> closing tags, it simply starts outputting` |
|         - | 1246 | ` * whatever it finds (except for an immediately following newline - see instruction separation ) until it hits` |
|         - | 1247 | ` * another opening tag. The example given here is contrived, of course, but for outputting large blocks of text` |
|         - | 1248 | ` * dropping out of PHP parsing mode is generally more efficient than sending all of the text through echo() or print().` |
|         - | 1249 | ` * There are four different pairs of opening and closing tags which can be used in PHP. Three of those, <?php ?>` |
|         - | 1250 | ` * <script language="php"> </script>  and <? ?> are always available. The other two are short tags and ASP style` |
|         - | 1251 | ` * tags, and can be turned on and off from the php.ini configuration file. As such, while some people find short tags` |
|         - | 1252 | ` * and ASP style tags convenient, they are less portable, and generally not recommended.` |
|         - | 1253 | ` * Note:` |
|         - | 1254 | ` * Also note that if you are embedding PHP within XML or XHTML you will need to use the <?php ?> tags to remain` |
|         - | 1255 | ` * compliant with standards.` |
|         - | 1256 | ` * Example #2 PHP Opening and Closing Tags` |
|         - | 1257 | ` * 1.  <?php echo 'if you want to serve XHTML or XML documents, do it like this'; ?>` |
|         - | 1258 | ` * 2.  <script language="php">` |
|         - | 1259 | ` *       echo 'some editors (like FrontPage) don\'t` |
|         - | 1260 | ` *             like processing instructions';` |
|         - | 1261 | ` *   </script>` |
|         - | 1262 | ` *` |
|         - | 1263 | ` * 3.  <? echo 'this is the simplest, an SGML processing instruction'; ?>` |
|         - | 1264 | ` *   <?= expression ?> This is a shortcut for "<? echo expression ?>"` |
|         - | 1265 | ` */` |
|     13482 | 1266 | `PH7_PRIVATE sxi32 PH7_TokenizeRawText(const char *zInput,sxu32 nLen,SySet *pOut,sxu32 nBaseLine)` |
|         5 | 1267 | `{` |
|     13487 | 1268 | `	const char *zEnd = &zInput[nLen];` |
|     13487 | 1269 | `	const char *zIn  = zInput;` |
|         - | 1270 | `	const char *zCur,*zCurEnd;` |
|     13487 | 1271 | `	SyString sCtag = { 0, 0 };     /* Closing tag */` |
|         - | 1272 | `	SyToken sToken;` |
|         - | 1273 | `	SyString sDoc;` |
|         - | 1274 | `	sxu32 nLine;` |
|         - | 1275 | `	sxi32 iNest;` |
|         - | 1276 | `	sxi32 rc;` |
|         - | 1277 | `	/* Tokenize the input into PHP tokens and raw tokens. nBaseLine is normally 1,` |
|         - | 1278 | `	 * but 2 when a "#!" shebang line was stripped so error lines still match php. */` |
|     13487 | 1279 | `	nLine = nBaseLine;` |
|     13487 | 1280 | `	zCur = zCurEnd   = 0; /* Prevent compiler warning */` |
|     13487 | 1281 | `	sToken.pUserData = 0;` |
|     13487 | 1282 | `	iNest = 0;` |
|     13487 | 1283 | `	sDoc.nByte = 0;` |
|     13487 | 1284 | `	sDoc.zString = ""; /* cc warning */` |
|     13474 | 1285 | `	for(;;){` |
|     26781 | 1286 | `		if( zIn >= zEnd ){` |
|         - | 1287 | `			/* End of input reached */` |
|     13291 | 1288 | `			break;` |
|         - | 1289 | `		}` |
|     13495 | 1290 | `		sToken.nLine = nLine;` |
|     13495 | 1291 | `		zCur = zIn;` |
|     13495 | 1292 | `		zCurEnd = 0;` |
|     13879 | 1293 | `		while( zIn < zEnd ){` |
|     13683 | 1294 | `			 if( zIn[0] == '<' ){` |
|     13299 | 1295 | `				const char *zTmp = zIn; /* End of raw input marker */` |
|     13299 | 1296 | `				zIn++;` |
|     13299 | 1297 | `				if( zIn < zEnd ){` |
|     13299 | 1298 | `					if( zIn[0] == '?' ){` |
|     13299 | 1299 | `						zIn++;` |
|     13299 | 1300 | `						if( (sxu32)(zEnd - zIn) >= sizeof("php")-1 &&  SyStrnicmp(zIn,"php",sizeof("php")-1) == 0 ){` |
|         - | 1301 | `							/* opening tag: <?php */` |
|     13297 | 1302 | `							zIn += sizeof("php")-1;` |
|      6646 | 1303 | `						}` |
|         - | 1304 | `						/* Look for the closing tag '?>' */` |
|     13299 | 1305 | `						SyStringInitFromBuf(&sCtag,"?>",sizeof("?>")-1);` |
|     13299 | 1306 | `						zCurEnd = zTmp;` |
|     13299 | 1307 | `						break;` |
|         - | 1308 | `					}` |
|       ! 0 | 1309 | `				}` |
|       ! 0 | 1310 | `			}else{` |
|       388 | 1311 | `				if( zIn[0] == '\n' ){` |
|         7 | 1312 | `					nLine++;` |
|         3 | 1313 | `				}` |
|       388 | 1314 | `				zIn++;` |
|         - | 1315 | `			 }` |
|         4 | 1316 | `		} /* While(zIn < zEnd) */` |
|     13495 | 1317 | `		if( zCurEnd == 0 ){` |
|        27 | 1318 | `			zCurEnd = zIn;` |
|        12 | 1319 | `		}` |
|         - | 1320 | `		/* Save the raw token */` |
|     13495 | 1321 | `		SyStringInitFromBuf(&sToken.sData,zCur,zCurEnd - zCur);` |
|     13495 | 1322 | `		sToken.nType = PH7_TOKEN_RAW;` |
|     13495 | 1323 | `		rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|     13495 | 1324 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1325 | `			return rc;` |
|         - | 1326 | `		}` |
|     13495 | 1327 | `		if( zIn >= zEnd ){` |
|        27 | 1328 | `			break;` |
|         - | 1329 | `		}` |
|         - | 1330 | `		/* Ignore leading white space */` |
|     28325 | 1331 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|     14859 | 1332 | `			if( zIn[0] == '\n' ){` |
|     14661 | 1333 | `				nLine++;` |
|      7328 | 1334 | `			}` |
|     14859 | 1335 | `			zIn++;` |
|         5 | 1336 | `		}` |
|         - | 1337 | `		/* Delimit the PHP chunk */` |
|     13471 | 1338 | `		sToken.nLine = nLine;` |
|     13471 | 1339 | `		zCur = zIn;` |
|   1710877 | 1340 | `		while( (sxu32)(zEnd - zIn) >= sCtag.nByte ){` |
|         - | 1341 | `			const char *zPtr;` |
|   1704577 | 1342 | `			if( SyMemcmp(zIn,sCtag.zString,sCtag.nByte) == 0 && iNest < 1 ){` |
|      6999 | 1343 | `				break;` |
|         - | 1344 | `			}` |
|         - | 1345 | `			/* Line comment ('#' or '//', but not the '#[' attribute opener): php` |
|         - | 1346 | `			 * ends it at a newline OR at the closing tag, so a '?>' inside a line` |
|         - | 1347 | `			 * comment DOES close the PHP block. Skipping the comment here also` |
|         - | 1348 | `			 * stops the string skip below from treating a quote inside the` |
|         - | 1349 | `			 * comment as a string. Only outside a heredoc body (iNest < 1). */` |
|   1702292 | 1350 | `			if( iNest < 1 &&` |
|   1692884 | 1351 | `				( (zIn[0] == '#' && !(zIn+1 < zEnd && zIn[1] == '[')) \|\|` |
|   1692905 | 1352 | `				  (zIn[0] == '/' && zIn+1 < zEnd && zIn[1] == '/') ) ){` |
|      9423 | 1353 | `				zIn += (zIn[0] == '#') ? 1 : 2;` |
|    411877 | 1354 | `				while( zIn < zEnd && zIn[0] != '\n' ){` |
|    402456 | 1355 | `					if( (sxu32)(zEnd - zIn) >= sCtag.nByte` |
|    402458 | 1356 | `						&& SyMemcmp(zIn,sCtag.zString,sCtag.nByte) == 0 ){` |
|         3 | 1357 | `						break; /* the closing tag terminates the line comment */` |
|         - | 1358 | `					}` |
|    402459 | 1359 | `					zIn++;` |
|         5 | 1360 | `				}` |
|      9031 | 1361 | `				continue;` |
|         - | 1362 | `			}` |
|         - | 1363 | `			/* Block comment: spans everything, including '?>', up to its close. */` |
|   1688385 | 1364 | `			if( iNest < 1 && zIn[0] == '/' && zIn+1 < zEnd && zIn[1] == '*' ){` |
|       297 | 1365 | `				zIn += 2;` |
|     32121 | 1366 | `				while( (sxu32)(zEnd-zIn) >= sizeof("*/") - 1 ){` |
|     32121 | 1367 | `					if( zIn[0] == '*' && zIn[1] == '/' ){` |
|       297 | 1368 | `						zIn += 2;` |
|       297 | 1369 | `						break;` |
|         - | 1370 | `					}` |
|     31829 | 1371 | `					if( zIn[0] == '\n' ){` |
|       259 | 1372 | `						nLine++;` |
|       127 | 1373 | `					}` |
|     31829 | 1374 | `					zIn++;` |
|         5 | 1375 | `				}` |
|       297 | 1376 | `				continue;` |
|         - | 1377 | `			}` |
|         - | 1378 | `			/* Skip over a single/double-quoted or backtick string literal so a` |
|         - | 1379 | `			 * '?>' sequence inside it is not mistaken for the closing tag. Only` |
|         - | 1380 | `			 * outside a heredoc body (iNest < 1); heredocs are delimited by the` |
|         - | 1381 | `			 * label-matching logic above. Escapes (\" \' \\ and a line-continuing` |
|         - | 1382 | `			 * backslash-newline) are honoured. Same-quote nesting inside "{$...}"` |
|         - | 1383 | `			 * interpolation is not tracked, but that can only end the skip early` |
|         - | 1384 | `			 * on a string that has no '?>' anyway, which stays a PHP chunk either` |
|         - | 1385 | `			 * way — it never mis-splits code that works today. */` |
|   1688093 | 1386 | ``			if( iNest < 1 && (zIn[0] == '\'' \|\| zIn[0] == '"' \|\| zIn[0] == '`') ){`` |
|     49467 | 1387 | `				int qch = zIn[0];` |
|     49467 | 1388 | `				zIn++;` |
|    341187 | 1389 | `				while( zIn < zEnd ){` |
|    341187 | 1390 | `					if( zIn[0] == '\\' && zIn + 1 < zEnd ){` |
|     19035 | 1391 | `						if( zIn[1] == '\n' ){ nLine++; }` |
|     19035 | 1392 | `						zIn += 2;` |
|     19035 | 1393 | `						continue;` |
|         - | 1394 | `					}` |
|    322157 | 1395 | `					if( zIn[0] == qch ){ zIn++; break; }` |
|    272695 | 1396 | `					if( zIn[0] == '\n' ){ nLine++; }` |
|    272695 | 1397 | `					zIn++;` |
|         5 | 1398 | `				}` |
|     49467 | 1399 | `				continue;` |
|         - | 1400 | `			}` |
|   1638631 | 1401 | `			if( zIn[0] == '\n' ){` |
|     68471 | 1402 | `				nLine++;` |
|     68471 | 1403 | `				if( iNest > 0 ){` |
|       363 | 1404 | `					zIn++;` |
|       841 | 1405 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|       483 | 1406 | `						zIn++;` |
|         5 | 1407 | `					}` |
|       363 | 1408 | `					zPtr = zIn;` |
|      1711 | 1409 | `					while( zIn < zEnd ){` |
|      1711 | 1410 | `						if( (unsigned char)zIn[0] >= 0xc0 ){` |
|         - | 1411 | `							/* UTF-8 stream */` |
|        19 | 1412 | `							zIn++;` |
|        37 | 1413 | `							SX_JMP_UTF8(zIn,zEnd);` |
|      1698 | 1414 | `						}else if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       184 | 1415 | `							break;` |
|       ! 0 | 1416 | `						}else{` |
|      1335 | 1417 | `							zIn++;` |
|         - | 1418 | `						}` |
|         5 | 1419 | `					}` |
|       363 | 1420 | `					if( (sxu32)(zIn - zPtr) == sDoc.nByte && SyMemcmp(sDoc.zString,zPtr,sDoc.nByte) == 0 ){` |
|       127 | 1421 | `						iNest = 0;` |
|        61 | 1422 | `					}` |
|       363 | 1423 | `					continue;` |
|         5 | 1424 | `				}` |
|   1604219 | 1425 | `			}else if ( (sxu32)(zEnd - zIn) >= sizeof("<<<") && zIn[0] == '<' && zIn[1] == '<' && zIn[2] == '<' && iNest < 1){` |
|       129 | 1426 | `				zIn += sizeof("<<<")-1;` |
|       141 | 1427 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|        13 | 1428 | `					zIn++;` |
|         1 | 1429 | `				}` |
|       129 | 1430 | `				if( zIn[0] == '"' \|\| zIn[0] == '\'' ){` |
|        56 | 1431 | `					zIn++;` |
|        26 | 1432 | `				}` |
|       129 | 1433 | `				zPtr = zIn;` |
|       589 | 1434 | `				while( zIn < zEnd ){` |
|       589 | 1435 | `					if( (unsigned char)zIn[0] >= 0xc0 ){` |
|         - | 1436 | `						/* UTF-8 stream */` |
|        19 | 1437 | `						zIn++;` |
|        37 | 1438 | `						SX_JMP_UTF8(zIn,zEnd);` |
|       576 | 1439 | `					}else if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|        67 | 1440 | `						break;` |
|       ! 0 | 1441 | `					}else{` |
|       447 | 1442 | `						zIn++;` |
|         - | 1443 | `					}` |
|         5 | 1444 | `				}` |
|       129 | 1445 | `				SyStringInitFromBuf(&sDoc,zPtr,zIn-zPtr);` |
|       129 | 1446 | `				SyStringFullTrim(&sDoc);` |
|       129 | 1447 | `				if( sDoc.nByte > 0 ){` |
|       129 | 1448 | `					iNest++;` |
|        62 | 1449 | `				}` |
|       129 | 1450 | `				continue;` |
|         - | 1451 | `			}` |
|   1638149 | 1452 | `			zIn++;` |
|         - | 1453 |  |
|   1638149 | 1454 | `			if ( zIn >= zEnd )` |
|       ! 0 | 1455 | `				break;` |
|         5 | 1456 | `		}` |
|     13299 | 1457 | `		if( (sxu32)(zEnd - zIn) < sCtag.nByte ){` |
|      6305 | 1458 | `			zIn = zEnd;` |
|      3150 | 1459 | `		}` |
|     13299 | 1460 | `		if( zCur < zIn ){` |
|         - | 1461 | `			/* Save the PHP chunk for later processing */` |
|      9975 | 1462 | `			sToken.nType = PH7_TOKEN_PHP;` |
|      9975 | 1463 | `			SyStringInitFromBuf(&sToken.sData,zCur,zIn-zCur);` |
|     19703 | 1464 | `			SyStringRightTrim(&sToken.sData); /* Trim trailing white spaces */` |
|      9975 | 1465 | `			rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|      9975 | 1466 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 1467 | `				return rc;` |
|         - | 1468 | `			}` |
|      4985 | 1469 | `		}` |
|     13299 | 1470 | `		if( zIn < zEnd ){` |
|         - | 1471 | `			/* Jump the trailing closing tag */` |
|      6999 | 1472 | `			zIn += sCtag.nByte;` |
|         - | 1473 | `			/* php's lexer swallows exactly ONE newline immediately after the` |
|         - | 1474 | `			 * closing tag ("?>\n" emits nothing) */` |
|      6999 | 1475 | `			if( zIn < zEnd && zIn[0] == '\r' && zIn + 1 < zEnd && zIn[1] == '\n' ){` |
|       ! 0 | 1476 | `				zIn += 2;` |
|       ! 0 | 1477 | `				nLine++;` |
|      6999 | 1478 | `			}else if( zIn < zEnd && zIn[0] == '\n' ){` |
|        61 | 1479 | `				zIn++;` |
|        61 | 1480 | `				nLine++;` |
|        28 | 1481 | `			}` |
|      3497 | 1482 | `		}` |
|         5 | 1483 | `	} /* For(;;) */` |
|         - | 1484 |  |
|     13315 | 1485 | ` 	return SXRET_OK;` |
|      6660 | 1486 | `}` |
|         - | 1487 |  |
