# src/ph7/lex.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 813/871 lines (93.34%)

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
| 174153004 |   19 | `static sxi32 TokenizePHP(SyStream *pStream,SyToken *pToken,void *pUserData,void *pCtxData)` |
|         5 |   20 | `{` |
|         - |   21 | `	SyString *pStr;` |
|         - |   22 | `	sxi32 rc;` |
|         - |   23 | `	/* Ignore leading white spaces */` |
| 257375715 |   24 | `	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){` |
|         - |   25 | `		/* Advance the stream cursor */` |
|  83222711 |   26 | `		if( pStream->zText[0] == '\n' ){` |
|         - |   27 | `			/* Update line counter */` |
|     55143 |   28 | `			pStream->nLine++;` |
|     27569 |   29 | `		}` |
|  83222711 |   30 | `		pStream->zText++;` |
|         5 |   31 | `	}` |
| 174153009 |   32 | `	if( pStream->zText >= pStream->zEnd ){` |
|         - |   33 | `		/* End of input reached */` |
|       ! 0 |   34 | `		return SXERR_EOF;` |
|         - |   35 | `	}` |
|         - |   36 | `	/* Record token starting position and line */` |
| 174153009 |   37 | `	pToken->nLine = pStream->nLine;` |
| 174153009 |   38 | `	pToken->pUserData = 0;` |
| 174153009 |   39 | `	pStr = &pToken->sData;` |
| 174153009 |   40 | `	SyStringInitFromBuf(pStr,pStream->zText,0);` |
| 203176853 |   41 | `	if( pStream->zText[0] >= 0xc0 \|\| SyisAlpha(pStream->zText[0]) \|\| pStream->zText[0] == '_' ){` |
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
|  58047693 |   52 | `		if( pStream->zText[0] < 0xc0 ){` |
|  58047677 |   53 | `			pStream->zText++;` |
|  29023836 |   54 | `		}` |
|  54417722 |   55 | `		for(;;){` |
| 108835449 |   56 | `			zIn = pStream->zText;` |
| 108835449 |   57 | `			if( zIn[0] >= 0xc0 ){` |
|        49 |   58 | `				zIn++;` |
|         - |   59 | `				/* UTF-8 stream */` |
|       109 |   60 | `				while( zIn < pStream->zEnd && ((zIn[0] & 0xc0) == 0x80) ){` |
|        61 |   61 | `					zIn++;` |
|         1 |   62 | `				}` |
|        24 |   63 | `			}` |
|         - |   64 | `			/* Skip alphanumeric stream */` |
| 433261965 |   65 | `			while( zIn < pStream->zEnd && zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_') ){` |
| 270008799 |   66 | `				zIn++;` |
|         5 |   67 | `			}` |
| 108835449 |   68 | `			if( zIn == pStream->zText ){` |
|         - |   69 | `				/* Not an UTF-8 or alphanumeric stream */` |
|  58047693 |   70 | `				break;` |
|         - |   71 | `			}` |
|         - |   72 | `			/* Synchronize pointers */` |
|  50787761 |   73 | `			pStream->zText = zIn;` |
|         5 |   74 | `		}` |
|         - |   75 | `		/* Record token length */` |
|  58047693 |   76 | `		pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  58047693 |   77 | `		nKeyword = KeywordCode(pStr->zString,(int)pStr->nByte);` |
|         - |   78 | `		/* PHP 7.4: 'fn' is a keyword reserved for arrow functions.` |
|         - |   79 | `		 * The auto-generated perfect hash above doesn't know about it,` |
|         - |   80 | `		 * so intercept the 'fn' identifier here.` |
|         - |   81 | `		 */` |
|  58047688 |   82 | `		if( nKeyword == PH7_TK_ID && pStr->nByte == 2` |
|  20026778 |   83 | `			&& pStr->zString[0] == 'f' && pStr->zString[1] == 'n' ){` |
|       837 |   84 | `			nKeyword = PH7_TKWRD_FN;` |
|       416 |   85 | `		}` |
|  58047693 |   86 | `		if( nKeyword != PH7_TK_ID ){` |
|  19347167 |   87 | `			if( nKeyword &` |
|         - |   88 | `				(PH7_TKWRD_NEW\|PH7_TKWRD_CLONE\|PH7_TKWRD_AND\|PH7_TKWRD_XOR\|PH7_TKWRD_OR\|PH7_TKWRD_INSTANCEOF) ){` |
|         - |   89 | `					/* Alpha stream operators [i.e: new,clone,and,instanceof,or,xor],save the operator instance for later processing */` |
|    923653 |   90 | `					pToken->pUserData = (void *)PH7_ExprExtractOperator(pStr,0);` |
|         - |   91 | `					/* Mark as an operator */` |
|    923653 |   92 | `					pToken->nType = PH7_TK_ID\|PH7_TK_OP;` |
|    461829 |   93 | `			}else{` |
|         - |   94 | `				/* We are dealing with a keyword [i.e: while,foreach,class...],save the keyword ID */` |
|  18423519 |   95 | `				pToken->nType = PH7_TK_KEYWORD;` |
|  18423519 |   96 | `				pToken->pUserData = SX_INT_TO_PTR(nKeyword);` |
|         - |   97 | `			}` |
|   9673586 |   98 | `		}else{` |
|         - |   99 | `			/* A simple identifier */` |
|  38700531 |  100 | `			pToken->nType = PH7_TK_ID;` |
|         - |  101 | `		}` |
|  29023849 |  102 | `	}else{` |
|         - |  103 | `		sxi32 c;` |
|         - |  104 | `		/* Non-alpha stream */` |
| 116105321 |  105 | `		if( pStream->zText[0] == '#' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '[' ){` |
|      7933 |  106 | `			sxu32 nDepth = 1;` |
|         - |  107 | `			/* PHP 8 attribute group '#[ ... ]': skip the whole balanced group as` |
|         - |  108 | `			 * trivia (attributes are not stored yet). Brackets inside string` |
|         - |  109 | `			 * literals and comments must not affect the depth count. An` |
|         - |  110 | `			 * unterminated group is silently consumed up to EOF, consistent` |
|         - |  111 | `			 * with unterminated block comments below.` |
|         - |  112 | `			 */` |
|         - |  113 | `			const unsigned char *zGroupStart;` |
|      7933 |  114 | `			pStream->zText += 2;` |
|      7933 |  115 | `			zGroupStart = pStream->zText;` |
|    647471 |  116 | `			while( pStream->zText < pStream->zEnd && nDepth > 0 ){` |
|    639543 |  117 | `				sxi32 d = pStream->zText[0];` |
|    639543 |  118 | `				if( d == '[' ){` |
|        11 |  119 | `					nDepth++;` |
|    639538 |  120 | `				}else if( d == ']' ){` |
|      7943 |  121 | `					nDepth--;` |
|    635564 |  122 | `				}else if( d == '\'' \|\| d == '"' ){` |
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
|    631573 |  145 | `				}else if( d == '#' \|\| (d == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '/') ){` |
|         - |  146 | `					/* Inline comment inside the group */` |
|       ! 0 |  147 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] != '\n' ){` |
|       ! 0 |  148 | `						pStream->zText++;` |
|       ! 0 |  149 | `					}` |
|       ! 0 |  150 | `					continue; /* Let the outer loop count the newline */` |
|    631551 |  151 | `				}else if( d == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '*' ){` |
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
|    631551 |  165 | `				}else if( d == '\n' ){` |
|         7 |  166 | `					pStream->nLine++;` |
|         3 |  167 | `				}` |
|    639543 |  168 | `				pStream->zText++;` |
|         5 |  169 | `			}` |
|      7933 |  170 | `			if( pUserData && pStream->pSet ){` |
|         - |  171 | `				/* Record the group's inner span (between #[ and its balanced ])` |
|         - |  172 | `				 * in the trivia sidecar, keyed like doc-comments. */` |
|         - |  173 | `				ph7_trivia sTrivia;` |
|      7933 |  174 | `				const unsigned char *zGroupEnd = pStream->zText;` |
|      7933 |  175 | `				if( nDepth == 0 && zGroupEnd > zGroupStart ){` |
|      7933 |  176 | `					zGroupEnd--; /* Exclude the closing ']' */` |
|      3964 |  177 | `				}` |
|      7933 |  178 | `				sTrivia.nTokIdx = SySetUsed(pStream->pSet);` |
|      7933 |  179 | `				sTrivia.iKind = PH7_TRIVIA_ATTR;` |
|      7933 |  180 | `				SyStringInitFromBuf(&sTrivia.sText,(const char *)zGroupStart,(sxu32)(zGroupEnd - zGroupStart));` |
|      7933 |  181 | `				sTrivia.nLine = pToken->nLine;` |
|      7933 |  182 | `				SySetPut((SySet *)pUserData,(const void *)&sTrivia);` |
|      3964 |  183 | `			}` |
|         - |  184 | `			/* Tell the upper-layer to ignore this token */` |
|      7933 |  185 | `			return SXERR_CONTINUE;` |
| 116207988 |  186 | `		}else if( pStream->zText[0] == '#' \|\|` |
| 116097382 |  187 | `			( pStream->zText[0] == '/' &&  &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '/') ){` |
|      7283 |  188 | `				pStream->zText++;` |
|         - |  189 | `				/* Inline comments */` |
|    312539 |  190 | `				while( pStream->zText < pStream->zEnd && pStream->zText[0] != '\n' ){` |
|    305261 |  191 | `					pStream->zText++;` |
|         5 |  192 | `				}` |
|         - |  193 | `				/* Tell the upper-layer to ignore this token */` |
|      7283 |  194 | `				return SXERR_CONTINUE;` |
| 116090115 |  195 | `		}else if( pStream->zText[0] == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '*' ){` |
|         - |  196 | `			/* A doc-comment starts with slash-star-star followed by more` |
|         - |  197 | `			 * content (slash-star-star-slash is the empty comment, not a` |
|         - |  198 | `			 * docblock). Its full span, delimiters included, goes to the` |
|         - |  199 | `			 * trivia sidecar when the caller supplied one — keyed by the` |
|         - |  200 | `			 * index the NEXT real token receives — and never enters the` |
|         - |  201 | `			 * token stream. */` |
|    209939 |  202 | `			const unsigned char *zDocStart = pStream->zText;` |
|    209951 |  203 | `			int bDoc = ( &pStream->zText[2] < pStream->zEnd && pStream->zText[2] == '*'` |
|    314913 |  204 | `			 && ( &pStream->zText[3] >= pStream->zEnd \|\| pStream->zText[3] != '/' ) );` |
|    209939 |  205 | `			pStream->zText += 2;` |
|         - |  206 | `			/* Block comment */` |
|  16166417 |  207 | `			while( pStream->zText < pStream->zEnd ){` |
|  16166417 |  208 | `				if( pStream->zText[0] == '*' ){` |
|    287863 |  209 | `					if( &pStream->zText[1] >= pStream->zEnd \|\| pStream->zText[1] == '/'  ){` |
|    104972 |  210 | `						break;` |
|         - |  211 | `					}` |
|     38962 |  212 | `				}` |
|  15956483 |  213 | `				if( pStream->zText[0] == '\n' ){` |
|       257 |  214 | `					pStream->nLine++;` |
|       126 |  215 | `				}` |
|  15956483 |  216 | `				pStream->zText++;` |
|         5 |  217 | `			}` |
|    209939 |  218 | `			pStream->zText += 2;` |
|    209939 |  219 | `			if( bDoc && pUserData && pStream->pSet ){` |
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
|    209939 |  232 | `			return SXERR_CONTINUE;` |
| 115880181 |  233 | `		}else if( SyisDigit(pStream->zText[0]) ){` |
|   3769153 |  234 | `			pStream->zText++;` |
|         - |  235 | `			/* PHP 7.4: handle underscore separator immediately following the first digit.` |
|         - |  236 | `			 * Check pStream->zText < pStream->zEnd BEFORE forming pStream->zText + 1 so` |
|         - |  237 | `			 * we never compute a pointer past one-past-end. */` |
|   3769148 |  238 | `			if( pStream->zText < pStream->zEnd` |
|   3769148 |  239 | `				&& pStream->zText[0] == '_'` |
|   1884654 |  240 | `				&& pStream->zText + 1 < pStream->zEnd` |
|       160 |  241 | `				&& pStream->zText[1] < 0xc0` |
|       165 |  242 | `				&& SyisDigit(pStream->zText[1]) ){` |
|       151 |  243 | `				pStream->zText++; /* swallow underscore between two digits */` |
|        75 |  244 | `			}` |
|         - |  245 | `			/* Decimal digit stream (PHP 7.4: underscore separator allowed between two digits) */` |
|   4750149 |  246 | `			while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|    981001 |  247 | `				pStream->zText++;` |
|    980996 |  248 | `				if( pStream->zText < pStream->zEnd` |
|    980996 |  249 | `					&& pStream->zText[0] == '_'` |
|    490584 |  250 | `					&& pStream->zText + 1 < pStream->zEnd` |
|       172 |  251 | `					&& pStream->zText[1] < 0xc0` |
|       177 |  252 | `					&& SyisDigit(pStream->zText[1]) ){` |
|       173 |  253 | `					pStream->zText++; /* swallow underscore between two digits */` |
|        86 |  254 | `				}` |
|         5 |  255 | `			}` |
|         - |  256 | `			/* Mark the token as integer until we encounter a real number */` |
|   3769153 |  257 | `			pToken->nType = PH7_TK_INTEGER;` |
|   3769153 |  258 | `			if( pStream->zText < pStream->zEnd ){` |
|   3769153 |  259 | `				c = pStream->zText[0];` |
|   3769153 |  260 | `				if( c == '.' ){` |
|         - |  261 | `					/* Real number (PHP 7.4: underscore separator allowed between two digits) */` |
|      8635 |  262 | `					pStream->zText++;` |
|     18721 |  263 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|     10091 |  264 | `						pStream->zText++;` |
|     10086 |  265 | `						if( pStream->zText < pStream->zEnd` |
|     10086 |  266 | `							&& pStream->zText[0] == '_'` |
|      5049 |  267 | `							&& pStream->zText + 1 < pStream->zEnd` |
|        12 |  268 | `							&& pStream->zText[1] < 0xc0` |
|        17 |  269 | `							&& SyisDigit(pStream->zText[1]) ){` |
|        13 |  270 | `							pStream->zText++;` |
|         6 |  271 | `						}` |
|         5 |  272 | `					}` |
|      8635 |  273 | `					if( pStream->zText < pStream->zEnd ){` |
|      8635 |  274 | `						c = pStream->zText[0];` |
|      8635 |  275 | `						if( c=='e' \|\| c=='E' ){` |
|        59 |  276 | `							pStream->zText++;` |
|        59 |  277 | `							if( pStream->zText < pStream->zEnd ){` |
|        59 |  278 | `								c = pStream->zText[0];` |
|        58 |  279 | `								if( (c =='+' \|\| c=='-') && &pStream->zText[1] < pStream->zEnd  &&` |
|        31 |  280 | `									pStream->zText[1] < 0xc0 && SyisDigit(pStream->zText[1]) ){` |
|        31 |  281 | `										pStream->zText++;` |
|        15 |  282 | `								}` |
|       171 |  283 | `								while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|       113 |  284 | `									pStream->zText++;` |
|       112 |  285 | `									if( pStream->zText < pStream->zEnd` |
|       112 |  286 | `										&& pStream->zText[0] == '_'` |
|        60 |  287 | `										&& pStream->zText + 1 < pStream->zEnd` |
|         8 |  288 | `										&& pStream->zText[1] < 0xc0` |
|         9 |  289 | `										&& SyisDigit(pStream->zText[1]) ){` |
|         9 |  290 | `										pStream->zText++;` |
|         4 |  291 | `									}` |
|         1 |  292 | `								}` |
|        29 |  293 | `							}` |
|        29 |  294 | `						}` |
|      4315 |  295 | `					}` |
|      8635 |  296 | `					pToken->nType = PH7_TK_REAL;` |
|   3764838 |  297 | `				}else if( c=='e' \|\| c=='E' ){` |
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
|   3760469 |  319 | `				}else if( c == 'x' \|\| c == 'X' ){` |
|         - |  320 | `					/* Hex digit stream (PHP 7.4: underscore separator allowed between two digits) */` |
|    104913 |  321 | `					pStream->zText++;` |
|    454775 |  322 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisHex(pStream->zText[0]) ){` |
|    349867 |  323 | `						pStream->zText++;` |
|    349862 |  324 | `						if( pStream->zText < pStream->zEnd` |
|    349862 |  325 | `							&& pStream->zText[0] == '_'` |
|    174955 |  326 | `							&& pStream->zText + 1 < pStream->zEnd` |
|        48 |  327 | `							&& pStream->zText[1] < 0xc0` |
|        53 |  328 | `							&& SyisHex(pStream->zText[1]) ){` |
|        49 |  329 | `							pStream->zText++;` |
|        24 |  330 | `						}` |
|         5 |  331 | `					}` |
|   3707961 |  332 | `				}else if(c  == 'b' \|\| c == 'B' ){` |
|         - |  333 | `					/* Binary digit stream (PHP 7.4: underscore separator allowed between two digits) */` |
|       286 |  334 | `					pStream->zText++;` |
|      3101 |  335 | `					while( pStream->zText < pStream->zEnd && (pStream->zText[0] == '0' \|\| pStream->zText[0] == '1') ){` |
|      1783 |  336 | `						pStream->zText++;` |
|      1782 |  337 | `						if( pStream->zText < pStream->zEnd` |
|      1782 |  338 | `							&& pStream->zText[0] == '_'` |
|       960 |  339 | `							&& pStream->zText + 1 < pStream->zEnd` |
|       139 |  340 | `							&& (pStream->zText[1] == '0' \|\| pStream->zText[1] == '1') ){` |
|       139 |  341 | `							pStream->zText++;` |
|        69 |  342 | `						}` |
|         1 |  343 | `					}` |
|   3655362 |  344 | `				}else if( c == 'o' \|\| c == 'O' ){` |
|         - |  345 | `					/* PHP 8.1 explicit octal 0o/0O (underscore separator allowed between two digits) */` |
|        17 |  346 | `					pStream->zText++;` |
|        89 |  347 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] >= '0' && pStream->zText[0] <= '7' ){` |
|        73 |  348 | `						pStream->zText++;` |
|        72 |  349 | `						if( pStream->zText < pStream->zEnd` |
|        72 |  350 | `							&& pStream->zText[0] == '_'` |
|        37 |  351 | `							&& pStream->zText + 1 < pStream->zEnd` |
|         3 |  352 | `							&& pStream->zText[1] >= '0' && pStream->zText[1] <= '7' ){` |
|         3 |  353 | `							pStream->zText++;` |
|         1 |  354 | `						}` |
|         1 |  355 | `					}` |
|         8 |  356 | `				}` |
|   1884574 |  357 | `			}` |
|         - |  358 | `			/* PHP 7.4: absorb a trailing malformed underscore run into the` |
|         - |  359 | `			 * numeric token so the compile phase can emit a PHP-compatible` |
|         - |  360 | `			 * "syntax error, unexpected identifier" parse error. Valid` |
|         - |  361 | `			 * separators were already consumed by the per-loop peek logic` |
|         - |  362 | `			 * above, so an underscore here is always misplaced. */` |
|   3769153 |  363 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '_' ){` |
|        18 |  364 | `				pStream->zText++;` |
|        44 |  365 | `				while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0` |
|        49 |  366 | `					&& (SyisAlphaNum(pStream->zText[0]) \|\| pStream->zText[0] == '_') ){` |
|        20 |  367 | `					pStream->zText++;` |
|         4 |  368 | `				}` |
|         7 |  369 | `			}` |
|         - |  370 | `			/* Record token length */` |
|   3769153 |  371 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|   3769153 |  372 | `			return SXRET_OK;` |
|         - |  373 | `		}` |
| 112111033 |  374 | `		c = pStream->zText[0];` |
| 112111033 |  375 | `		pStream->zText++; /* Advance the stream cursor */` |
|         - |  376 | `		/* Assume we are dealing with an operator*/` |
| 112111033 |  377 | `		pToken->nType = PH7_TK_OP;` |
| 112111033 |  378 | `		switch(c){` |
|  23150651 |  379 | `		case '$': pToken->nType = PH7_TK_DOLLAR; break;` |
|   6880023 |  380 | `		case '{': pToken->nType = PH7_TK_OCB;    break;` |
|   6880009 |  381 | `		case '}': pToken->nType = PH7_TK_CCB;    break;` |
|  14404377 |  382 | `		case '(': pToken->nType = PH7_TK_LPAREN; break;` |
|   3193055 |  383 | `		case '[': pToken->nType \|= PH7_TK_OSB;   break; /* Bitwise operation here,since the square bracket token '['` |
|         - |  384 | `														 * is a potential operator [i.e: subscripting] */` |
|   3193061 |  385 | `		case ']': pToken->nType = PH7_TK_CSB;    break;` |
|   7202174 |  386 | `		case ')': {` |
|  14404353 |  387 | `			SySet *pTokSet = pStream->pSet;` |
|         - |  388 | `			/* Assemble type cast operators [i.e: (int),(float),(bool)...] */` |
|  14404353 |  389 | `			if( pTokSet->nUsed >= 2 ){` |
|         - |  390 | `				SyToken *pTmp;` |
|         - |  391 | `				/* Peek the last recongnized token */` |
|  14404351 |  392 | `				pTmp = (SyToken *)SySetPeek(pTokSet);` |
|  14404351 |  393 | `				if( pTmp->nType & PH7_TK_KEYWORD ){` |
|   1041187 |  394 | `					sxi32 nID = SX_PTR_TO_INT(pTmp->pUserData);` |
|   1041187 |  395 | `					if( (sxu32)nID & (PH7_TKWRD_ARRAY\|PH7_TKWRD_INT\|PH7_TKWRD_FLOAT\|PH7_TKWRD_STRING\|PH7_TKWRD_OBJECT\|PH7_TKWRD_BOOL\|PH7_TKWRD_UNSET) ){` |
|    928305 |  396 | `						pTmp = (SyToken *)SySetAt(pTokSet,pTokSet->nUsed - 2);` |
|    928305 |  397 | `						if( pTmp->nType & PH7_TK_LPAREN ){` |
|         - |  398 | `							/* Merge the three tokens '(' 'TYPE' ')' into a single one */` |
|    606013 |  399 | `							const char * zTypeCast = "(int)";` |
|    606013 |  400 | `							if( nID & PH7_TKWRD_FLOAT ){` |
|     31083 |  401 | `								zTypeCast = "(float)";` |
|    590474 |  402 | `							}else if( nID & PH7_TKWRD_BOOL ){` |
|      3923 |  403 | `								zTypeCast = "(bool)";` |
|    572976 |  404 | `							}else if( nID & PH7_TKWRD_STRING ){` |
|    252413 |  405 | `								zTypeCast = "(string)";` |
|    444813 |  406 | `							}else if( nID & PH7_TKWRD_ARRAY ){` |
|        29 |  407 | `								zTypeCast = "(array)";` |
|    318595 |  408 | `							}else if( nID & PH7_TKWRD_OBJECT ){` |
|        35 |  409 | `								zTypeCast = "(object)";` |
|    318564 |  410 | `							}else if( nID & PH7_TKWRD_UNSET ){` |
|         3 |  411 | `								zTypeCast = "(unset)";` |
|         1 |  412 | `							}` |
|         - |  413 | `							/* Reflect the change */` |
|    606013 |  414 | `							pToken->nType = PH7_TK_OP;` |
|    606013 |  415 | `							SyStringInitFromBuf(&pToken->sData,zTypeCast,SyStrlen(zTypeCast));` |
|         - |  416 | `							/* Save the instance associated with the type cast operator */` |
|    606013 |  417 | `							pToken->pUserData = (void *)PH7_ExprExtractOperator(&pToken->sData,0);` |
|         - |  418 | `							/* Remove the two previous tokens */` |
|    606013 |  419 | `							pTokSet->nUsed -= 2;` |
|    606013 |  420 | `							return SXRET_OK;` |
|         - |  421 | `						}` |
|    161146 |  422 | `					}` |
|    217587 |  423 | `				}` |
|   6899169 |  424 | `			}` |
|  13798345 |  425 | `			pToken->nType = PH7_TK_RPAREN;` |
|  13798345 |  426 | `			break;` |
|         - |  427 | `				  }` |
|   2646489 |  428 | `		case '\'':{` |
|         - |  429 | `			/* Single quoted string */` |
|   5292983 |  430 | `			pStr->zString++;` |
|  60378723 |  431 | `			while( pStream->zText < pStream->zEnd ){` |
|  60378723 |  432 | `				if( pStream->zText[0] == '\''  ){` |
|   5292997 |  433 | `					if( pStream->zText[-1] != '\\' ){` |
|   5258009 |  434 | `						break;` |
|       ! 0 |  435 | `					}else{` |
|     34993 |  436 | `						const unsigned char *zPtr = &pStream->zText[-2];` |
|     34993 |  437 | `						sxi32 i = 1;` |
|     69987 |  438 | `						while( zPtr > pStream->zInput && zPtr[0] == '\\' ){` |
|     34999 |  439 | `							zPtr--;` |
|     34999 |  440 | `							i++;` |
|         5 |  441 | `						}` |
|     34993 |  442 | `						if((i&1)==0){` |
|     34979 |  443 | `							break;` |
|         - |  444 | `						}` |
|         - |  445 | `					}` |
|         7 |  446 | `				}` |
|  55085745 |  447 | `				if( pStream->zText[0] == '\n' ){` |
|        67 |  448 | `					pStream->nLine++;` |
|        33 |  449 | `				}` |
|  55085745 |  450 | `				pStream->zText++;` |
|         5 |  451 | `			}` |
|         - |  452 | `			/* Record token length and type */` |
|   5292983 |  453 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|   5292983 |  454 | `			pToken->nType = PH7_TK_SSTR;` |
|         - |  455 | `			/* Jump the trailing single quote */` |
|   5292983 |  456 | `			pStream->zText++;` |
|   5292983 |  457 | `			return SXRET_OK;` |
|         - |  458 | `				  }` |
|     63067 |  459 | `		case '"':{` |
|         - |  460 | `			sxi32 iNest;` |
|         - |  461 | `			/* Double quoted string */` |
|    126139 |  462 | `			pStr->zString++;` |
|   1748465 |  463 | `			while( pStream->zText < pStream->zEnd ){` |
|   1748465 |  464 | `				if( pStream->zText[0] == '{' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '$'){` |
|       115 |  465 | `					iNest = 1;` |
|       115 |  466 | `					pStream->zText++;` |
|         - |  467 | `					/* TICKET 1433-40: Hnadle braces'{}' in double quoted string where everything is allowed */` |
|      1129 |  468 | `					while(pStream->zText < pStream->zEnd ){` |
|      1129 |  469 | `						if( pStream->zText[0] == '{' ){` |
|         3 |  470 | `							iNest++;` |
|      1128 |  471 | `						}else if (pStream->zText[0] == '}' ){` |
|       117 |  472 | `							iNest--;` |
|       117 |  473 | `							if( iNest <= 0 ){` |
|       115 |  474 | `								pStream->zText++;` |
|       115 |  475 | `								break;` |
|         1 |  476 | `							}` |
|      1014 |  477 | `						}else if( pStream->zText[0] == '\n' ){` |
|       ! 0 |  478 | `							pStream->nLine++;` |
|       ! 0 |  479 | `						}` |
|      1017 |  480 | `						pStream->zText++;` |
|         3 |  481 | `					}` |
|       115 |  482 | `					if( pStream->zText >= pStream->zEnd ){` |
|       ! 0 |  483 | `						break;` |
|         - |  484 | `					}` |
|        56 |  485 | `				}` |
|   1748465 |  486 | `				if( pStream->zText[0] == '"' ){` |
|    126433 |  487 | `					if( pStream->zText[-1] != '\\' ){` |
|    126131 |  488 | `						break;` |
|       ! 0 |  489 | `					}else{` |
|       307 |  490 | `						const unsigned char *zPtr = &pStream->zText[-2];` |
|       307 |  491 | `						sxi32 i = 1;` |
|       363 |  492 | `						while( zPtr > pStream->zInput && zPtr[0] == '\\' ){` |
|        59 |  493 | `							zPtr--;` |
|        59 |  494 | `							i++;` |
|         3 |  495 | `						}` |
|       307 |  496 | `						if((i&1)==0){` |
|         9 |  497 | `							break;` |
|         - |  498 | `						}` |
|         - |  499 | `					}` |
|       147 |  500 | `				}` |
|   1622331 |  501 | `				if( pStream->zText[0] == '\n' ){` |
|        29 |  502 | `					pStream->nLine++;` |
|        14 |  503 | `				}` |
|   1622331 |  504 | `				pStream->zText++;` |
|         5 |  505 | `			}` |
|         - |  506 | `			/* Record token length and type */` |
|    126139 |  507 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|    126139 |  508 | `			pToken->nType = PH7_TK_DSTR;` |
|         - |  509 | `			/* Jump the trailing quote */` |
|    126139 |  510 | `			pStream->zText++;` |
|    126139 |  511 | `			return SXRET_OK;` |
|         - |  512 | `				  }` |
|         2 |  513 | ``		case '`':{`` |
|         - |  514 | `			/* Backtick quoted string */` |
|         6 |  515 | `			pStr->zString++;` |
|        46 |  516 | `			while( pStream->zText < pStream->zEnd ){` |
|        46 |  517 | ``				if( pStream->zText[0] == '`' && pStream->zText[-1] != '\\' ){`` |
|         6 |  518 | `					break;` |
|         - |  519 | `				}` |
|        42 |  520 | `				if( pStream->zText[0] == '\n' ){` |
|       ! 0 |  521 | `					pStream->nLine++;` |
|       ! 0 |  522 | `				}` |
|        42 |  523 | `				pStream->zText++;` |
|         2 |  524 | `			}` |
|         - |  525 | `			/* Record token length and type */` |
|         6 |  526 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|         6 |  527 | `			pToken->nType = PH7_TK_BSTR;` |
|         - |  528 | `			/* Jump the trailing backtick */` |
|         6 |  529 | `			pStream->zText++;` |
|         6 |  530 | `			return SXRET_OK;` |
|         - |  531 | `				  }` |
|      8737 |  532 | `		case '\\': pToken->nType = PH7_TK_NSSEP;  break;` |
|    486221 |  533 | `		case ':':` |
|    972447 |  534 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == ':' ){` |
|         - |  535 | `				/* Current operator: '::' */` |
|    404773 |  536 | `				pStream->zText++;` |
|    202389 |  537 | `			}else{` |
|    567679 |  538 | `				pToken->nType = PH7_TK_COLON; /* Single colon */` |
|         - |  539 | `			}` |
|    972447 |  540 | `			break;` |
|   3845043 |  541 | `		case ',': pToken->nType \|= PH7_TK_COMMA;  break; /* Comma is also an operator */` |
|  10923221 |  542 | `		case ';': pToken->nType = PH7_TK_SEMI;    break;` |
|         - |  543 | `			/* Handle combined operators [i.e: +=,===,!=== ...] */` |
|   3912648 |  544 | `		case '=':` |
|   7825301 |  545 | `			pToken->nType \|= PH7_TK_EQUAL;` |
|   7825301 |  546 | `			if( pStream->zText < pStream->zEnd ){` |
|   7825301 |  547 | `				if( pStream->zText[0] == '=' ){` |
|   1356635 |  548 | `					pToken->nType &= ~PH7_TK_EQUAL;` |
|         - |  549 | `					/* Current operator: == */` |
|   1356635 |  550 | `					pStream->zText++;` |
|   1356635 |  551 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  552 | `						/* Current operator: === */` |
|   1305805 |  553 | `						pStream->zText++;` |
|    652905 |  554 | `					}` |
|   7146986 |  555 | `				}else if( pStream->zText[0] == '>' ){` |
|         - |  556 | `					/* Array operator: => */` |
|    527277 |  557 | `					pToken->nType = PH7_TK_ARRAY_OP;` |
|    527277 |  558 | `					pStream->zText++;` |
|    263641 |  559 | `				}else{` |
|         - |  560 | `					/* TICKET 1433-0010: Reference operator '=&' */` |
|   5941399 |  561 | `					const unsigned char *zCur = pStream->zText;` |
|   5941399 |  562 | `					sxu32 nLine = 0;` |
|  11882629 |  563 | `					while( zCur < pStream->zEnd && zCur[0] < 0xc0 && SyisSpace(zCur[0]) ){` |
|   5941235 |  564 | `						if( zCur[0] == '\n' ){` |
|         5 |  565 | `							nLine++;` |
|         2 |  566 | `						}` |
|   5941235 |  567 | `						zCur++;` |
|         5 |  568 | `					}` |
|   5941399 |  569 | `					if( zCur < pStream->zEnd && zCur[0] == '&' ){` |
|         - |  570 | `						/* Current operator: =& */` |
|        65 |  571 | `						pToken->nType &= ~PH7_TK_EQUAL;` |
|        65 |  572 | `						SyStringInitFromBuf(pStr,"=&",sizeof("=&")-1);` |
|         - |  573 | `						/* Update token stream */` |
|        65 |  574 | `						pStream->zText = &zCur[1];` |
|        65 |  575 | `						pStream->nLine += nLine;` |
|        31 |  576 | `					}` |
|         - |  577 | `				}` |
|   3912648 |  578 | `			}` |
|   7825301 |  579 | `			break;` |
|    437147 |  580 | `		case '!':` |
|    874299 |  581 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  582 | `				/* Current operator: != */` |
|    427251 |  583 | `				pStream->zText++;` |
|    427251 |  584 | `				if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  585 | `					/* Current operator: !== */` |
|    411689 |  586 | `					pStream->zText++;` |
|    205842 |  587 | `				}` |
|    213623 |  588 | `			}` |
|    874299 |  589 | `			break;` |
|    336182 |  590 | `		case '&':` |
|    672369 |  591 | `			pToken->nType \|= PH7_TK_AMPER;` |
|    672369 |  592 | `			if( pStream->zText < pStream->zEnd ){` |
|    672369 |  593 | `				if( pStream->zText[0] == '&' ){` |
|    443063 |  594 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|         - |  595 | `					/* Current operator: && */` |
|    443063 |  596 | `					pStream->zText++;` |
|    450840 |  597 | `				}else if( pStream->zText[0] == '=' ){` |
|         7 |  598 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|         - |  599 | `					/* Current operator: &= */` |
|         7 |  600 | `					pStream->zText++;` |
|         3 |  601 | `				}` |
|    336182 |  602 | `			}` |
|    672369 |  603 | `			break;` |
|    196246 |  604 | `		case '\|':` |
|    392497 |  605 | `			if( pStream->zText < pStream->zEnd ){` |
|    392497 |  606 | `				if( pStream->zText[0] == '\|' ){` |
|         - |  607 | `					/* Current operator: \|\| */` |
|    318481 |  608 | `					pStream->zText++;` |
|    233259 |  609 | `				}else if( pStream->zText[0] == '=' ){` |
|         - |  610 | `					/* Current operator: \|= */` |
|     58241 |  611 | `					pStream->zText++;` |
|     44903 |  612 | `				}else if( pStream->zText[0] == '>' ){` |
|         - |  613 | `					/* Current operator: \|> (PHP 8.5 pipe) */` |
|        27 |  614 | `					pStream->zText++;` |
|        13 |  615 | `				}` |
|    196246 |  616 | `			}` |
|    392497 |  617 | `			break;` |
|    223834 |  618 | `		case '+':` |
|    447673 |  619 | `			if( pStream->zText < pStream->zEnd ){` |
|    447671 |  620 | `				if( pStream->zText[0] == '+' ){` |
|         - |  621 | `					/* Current operator: ++ */` |
|    179039 |  622 | `					pStream->zText++;` |
|    358154 |  623 | `				}else if( pStream->zText[0] == '=' ){` |
|         - |  624 | `					/* Current operator: += */` |
|     54427 |  625 | `					pStream->zText++;` |
|     27211 |  626 | `				}` |
|    223833 |  627 | `			}` |
|    447673 |  628 | `			break;` |
|   2649267 |  629 | `		case '-':` |
|   5298539 |  630 | `			if( pStream->zText < pStream->zEnd ){` |
|   5298539 |  631 | `				if( pStream->zText[0] == '-' ){` |
|         - |  632 | `					/* Current operator: -- */` |
|     31103 |  633 | `					pStream->zText++;` |
|   5282990 |  634 | `				}else if( pStream->zText[0] == '=' ){` |
|         - |  635 | `					/* Current operator: -= */` |
|        14 |  636 | `					pStream->zText++;` |
|   5267435 |  637 | `				}else if( pStream->zText[0] == '>' ){` |
|         - |  638 | `					/* Current operator: -> */` |
|   5025861 |  639 | `					pStream->zText++;` |
|   2512928 |  640 | `				}` |
|   2649267 |  641 | `			}` |
|   5298539 |  642 | `			break;` |
|     19622 |  643 | `		case '*':` |
|     39249 |  644 | `			if( pStream->zText < pStream->zEnd ){` |
|     39249 |  645 | `				if( pStream->zText[0] == '*' ){` |
|         - |  646 | `					/* Current operator: ** or **= */` |
|       137 |  647 | `					pStream->zText++;` |
|       137 |  648 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  649 | `						/* Current operator: **= */` |
|        23 |  650 | `						pStream->zText++;` |
|        12 |  651 | `					}` |
|     39181 |  652 | `				}else if( pStream->zText[0] == '=' ){` |
|         - |  653 | `					/* Current operator: *= */` |
|        27 |  654 | `					pStream->zText++;` |
|        12 |  655 | `				}` |
|     19622 |  656 | `			}` |
|     39249 |  657 | `			break;` |
|      1992 |  658 | `		case '/':` |
|      3989 |  659 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  660 | `				/* Current operator: /= */` |
|         9 |  661 | `				pStream->zText++;` |
|         4 |  662 | `			}` |
|      3989 |  663 | `			break;` |
|     17518 |  664 | `		case '%':` |
|     35041 |  665 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  666 | `				/* Current operator: %= */` |
|         9 |  667 | `				pStream->zText++;` |
|         4 |  668 | `			}` |
|     35041 |  669 | `			break;` |
|        11 |  670 | `		case '^':` |
|        23 |  671 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  672 | `				/* Current operator: ^= */` |
|         9 |  673 | `				pStream->zText++;` |
|         4 |  674 | `			}` |
|        23 |  675 | `			break;` |
|    998379 |  676 | `		case '.':` |
|   1996763 |  677 | `			if( pStream->zText + 1 < pStream->zEnd && pStream->zText[0] == '.' && pStream->zText[1] == '.' ){` |
|         - |  678 | `				/* Ellipsis: ... */` |
|     27633 |  679 | `				pStream->zText += 2;` |
|     27633 |  680 | `				pToken->nType = PH7_TK_ELLIPSIS;` |
|   1982949 |  681 | `			}else if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  682 | `				/* Current operator: .= */` |
|    337899 |  683 | `				pStream->zText++;` |
|    168947 |  684 | `			}` |
|   1996763 |  685 | `			break;` |
|    194441 |  686 | `		case '<':` |
|    388887 |  687 | `			if( pStream->zText < pStream->zEnd ){` |
|    388887 |  688 | `				if( pStream->zText[0] == '<' ){` |
|         - |  689 | `					/* Current operator: << */` |
|       149 |  690 | `					pStream->zText++;` |
|       149 |  691 | `					if( pStream->zText < pStream->zEnd ){` |
|       149 |  692 | `						if( pStream->zText[0] == '=' ){` |
|         - |  693 | `							/* Current operator: <<= */` |
|         9 |  694 | `							pStream->zText++;` |
|       145 |  695 | `						}else if( pStream->zText[0] == '<' ){` |
|         - |  696 | `							/* Current Token: <<<  */` |
|       127 |  697 | `							pStream->zText++;` |
|         - |  698 | `							/* This may be the beginning of a Heredoc/Nowdoc string,try to delimit it */` |
|       127 |  699 | `							rc = LexExtractHeredoc(&(*pStream),&(*pToken));` |
|       127 |  700 | `							if( rc == SXRET_OK ){` |
|         - |  701 | `								/* Here/Now doc successfuly extracted */` |
|       127 |  702 | `								return SXRET_OK;` |
|         - |  703 | `							}` |
|       ! 0 |  704 | `						}` |
|        12 |  705 | `					}` |
|    388754 |  706 | `				}else if( pStream->zText[0] == '>' ){` |
|         - |  707 | `					/* Current operator: <> */` |
|         5 |  708 | `					pStream->zText++;` |
|    388741 |  709 | `				}else if( pStream->zText[0] == '=' ){` |
|         - |  710 | `					/* Current operator: <= or <=> */` |
|     66133 |  711 | `					pStream->zText++;` |
|     66133 |  712 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '>' ){` |
|         - |  713 | `						/* Current operator: <=> */` |
|     27249 |  714 | `						pStream->zText++;` |
|     13622 |  715 | `					}` |
|     33064 |  716 | `				}` |
|    194380 |  717 | `			}` |
|    388765 |  718 | `			break;` |
|    153496 |  719 | `		case '>':` |
|    306997 |  720 | `			if( pStream->zText < pStream->zEnd ){` |
|    306997 |  721 | `				if( pStream->zText[0] == '>' ){` |
|         - |  722 | `					/* Current operator: >> */` |
|     19435 |  723 | `					pStream->zText++;` |
|     19435 |  724 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  725 | `						/* Current operator: >>= */` |
|        11 |  726 | `						pStream->zText++;` |
|        10 |  727 | `					}` |
|    297282 |  728 | `				}else if( pStream->zText[0] == '=' ){` |
|         - |  729 | `					/* Current operator: >= */` |
|    104921 |  730 | `					pStream->zText++;` |
|     52458 |  731 | `				}` |
|    153496 |  732 | `			}` |
|    306997 |  733 | `			break;` |
|    269798 |  734 | `		case '?':` |
|    539601 |  735 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '?' ){` |
|         - |  736 | `				/* Null coalescing operator: ?? */` |
|     58497 |  737 | `				pStream->zText++;` |
|     58497 |  738 | `				if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  739 | `					/* Null coalescing assignment operator (PHP 7.4) */` |
|       101 |  740 | `					pStream->zText++;` |
|        48 |  741 | `				}` |
|    510355 |  742 | `			}else if( (pStream->zEnd - pStream->zText) >= 2` |
|    481109 |  743 | `				&& pStream->zText[0] == '-' && pStream->zText[1] == '>' ){` |
|         - |  744 | `				/* Nullsafe object operator (PHP 8.0): ?-> */` |
|       119 |  745 | `				pStream->zText += 2;` |
|        57 |  746 | `			}` |
|    539596 |  747 | `			break;` |
|      7914 |  748 | `		default:` |
|     15828 |  749 | `			break;` |
|         - |  750 | `		}` |
| 106085787 |  751 | `		if( pStr->nByte <= 0 ){` |
|         - |  752 | `			/* Record token length */` |
| 106085725 |  753 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  53042860 |  754 | `		}` |
| 106085787 |  755 | `		if( pToken->nType & PH7_TK_OP ){` |
|         - |  756 | `			const ph7_expr_op *pOp;` |
|         - |  757 | `			/* Check if the extracted token is an operator */` |
|  25724829 |  758 | `			pOp = PH7_ExprExtractOperator(pStr,(SyToken *)SySetPeek(pStream->pSet));` |
|  25724829 |  759 | `			if( pOp == 0 ){` |
|         - |  760 | `				/* Not an operator */` |
|       ! 0 |  761 | `				pToken->nType &= ~PH7_TK_OP;` |
|       ! 0 |  762 | `				if( pToken->nType <= 0 ){` |
|       ! 0 |  763 | `					pToken->nType = PH7_TK_OTHER;` |
|       ! 0 |  764 | `				}` |
|       ! 0 |  765 | `			}else{` |
|         - |  766 | `				/* Save the instance associated with this operator for later processing */` |
|  25724829 |  767 | `				pToken->pUserData = (void *)pOp;` |
|         - |  768 | `			}` |
|  12862412 |  769 | `		}` |
|         - |  770 | `	}` |
|         - |  771 | `	/* Tell the upper-layer to save the extracted token for later processing */` |
| 164133475 |  772 | `	return SXRET_OK;` |
|  87076507 |  773 | `}` |
|         - |  774 | `/* SPDX-SnippetBegin */` |
|         - |  775 | `/* SPDX-SnippetCopyrightText: SQLite mkkeywordhash.c (D. Richard Hipp and the SQLite authors <https://sqlite.org/>); adapted for the PH7 engine by Chems mrad */` |
|         - |  776 | `/* SPDX-License-Identifier: blessing */` |
|         - |  777 | `/***** This file contains automatically generated code ******` |
|         - |  778 | `**` |
|         - |  779 | `** The code in this file has been automatically generated by` |
|         - |  780 | `**` |
|         - |  781 | `**     $Header: /sqlite/sqlite/tool/mkkeywordhash.c` |
|         - |  782 | `**` |
|         - |  783 | `** Sligthly modified by Chems mrad <chm@symisc.net> for the PH7 engine.` |
|         - |  784 | `**` |
|         - |  785 | `** The code in this file implements a function that determines whether` |
|         - |  786 | `** or not a given identifier is really a PHP keyword.  The same thing` |
|         - |  787 | `** might be implemented more directly using a hand-written hash table.` |
|         - |  788 | `** But by using this automatically generated code, the size of the code` |
|         - |  789 | `** is substantially reduced.  This is important for embedded applications` |
|         - |  790 | `** on platforms with limited memory.` |
|         - |  791 | `*/` |
|         - |  792 | `/* Hash score: 103 */` |
|  58047693 |  793 | `static sxu32 KeywordCode(const char *z, int n){` |
|         - |  794 | `  /* zText[] encodes 532 bytes of keywords in 333 bytes */` |
|         - |  795 | `  /*   extendswitchprintegerequire_oncenddeclareturnamespacechobject      */` |
|         - |  796 | `  /*   hrowbooleandefaultrycaselfinalistaticlonewconstringlobaluse        */` |
|         - |  797 | `  /*   lseifloatvarrayANDIEchoUSECHOabstractclasscontinuendifunction      */` |
|         - |  798 | `  /*   diendwhilevaldoexitgotoimplementsinclude_oncemptyinstanceof        */` |
|         - |  799 | `  /*   interfacendforeachissetparentprivateprotectedpublicatchunset       */` |
|         - |  800 | `  /*   xorARRAYASArrayEXITUNSETXORbreak                                   */` |
|         - |  801 | `  static const char zText[332] = {` |
|         - |  802 | `    'e','x','t','e','n','d','s','w','i','t','c','h','p','r','i','n','t','e',` |
|         - |  803 | `    'g','e','r','e','q','u','i','r','e','_','o','n','c','e','n','d','d','e',` |
|         - |  804 | `    'c','l','a','r','e','t','u','r','n','a','m','e','s','p','a','c','e','c',` |
|         - |  805 | `    'h','o','b','j','e','c','t','h','r','o','w','b','o','o','l','e','a','n',` |
|         - |  806 | `    'd','e','f','a','u','l','t','r','y','c','a','s','e','l','f','i','n','a',` |
|         - |  807 | `    'l','i','s','t','a','t','i','c','l','o','n','e','w','c','o','n','s','t',` |
|         - |  808 | `    'r','i','n','g','l','o','b','a','l','u','s','e','l','s','e','i','f','l',` |
|         - |  809 | `    'o','a','t','v','a','r','r','a','y','A','N','D','I','E','c','h','o','U',` |
|         - |  810 | `    'S','E','C','H','O','a','b','s','t','r','a','c','t','c','l','a','s','s',` |
|         - |  811 | `    'c','o','n','t','i','n','u','e','n','d','i','f','u','n','c','t','i','o',` |
|         - |  812 | `    'n','d','i','e','n','d','w','h','i','l','e','v','a','l','d','o','e','x',` |
|         - |  813 | `    'i','t','g','o','t','o','i','m','p','l','e','m','e','n','t','s','i','n',` |
|         - |  814 | `    'c','l','u','d','e','_','o','n','c','e','m','p','t','y','i','n','s','t',` |
|         - |  815 | `    'a','n','c','e','o','f','i','n','t','e','r','f','a','c','e','n','d','f',` |
|         - |  816 | `    'o','r','e','a','c','h','i','s','s','e','t','p','a','r','e','n','t','p',` |
|         - |  817 | `    'r','i','v','a','t','e','p','r','o','t','e','c','t','e','d','p','u','b',` |
|         - |  818 | `    'l','i','c','a','t','c','h','u','n','s','e','t','x','o','r','A','R','R',` |
|         - |  819 | `    'A','Y','A','S','A','r','r','a','y','E','X','I','T','U','N','S','E','T',` |
|         - |  820 | `    'X','O','R','b','r','e','a','k'` |
|         - |  821 | `  };` |
|         - |  822 | `  static const unsigned char aHash[151] = {` |
|         - |  823 | `       0,   0,   4,  83,   0,  61,  39,  12,   0,  33,  77,   0,  48,` |
|         - |  824 | `       0,   2,  65,  67,   0,   0,   0,  47,   0,   0,  40,   0,  15,` |
|         - |  825 | `      74,   0,  51,   0,  76,   0,   0,  20,   0,   0,   0,  50,   0,` |
|         - |  826 | `      80,  34,   0,  36,   0,   0,  64,  16,   0,   0,  17,   0,   1,` |
|         - |  827 | `      19,  84,  66,   0,  43,  45,  78,   0,   0,  53,  56,   0,   0,` |
|         - |  828 | `       0,  23,  49,   0,   0,  13,  31,  54,   7,   0,   0,  25,   0,` |
|         - |  829 | `      72,  14,   0,  71,   0,  38,   6,   0,   0,   0,  73,   0,   0,` |
|         - |  830 | `       3,   0,  41,   5,  52,  57,  32,   0,  60,  63,   0,  69,  82,` |
|         - |  831 | `      30,   0,  79,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,` |
|         - |  832 | `       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  81,   0,   0,` |
|         - |  833 | `      62,   0,  11,   0,   0,  58,   0,   0,   0,   0,  59,  75,   0,` |
|         - |  834 | `       0,   0,   0,   0,   0,  35,  27,   0` |
|         - |  835 | `  };` |
|         - |  836 | `  static const unsigned char aNext[84] = {` |
|         - |  837 | `       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,` |
|         - |  838 | `       0,   0,   8,   0,   0,   0,  10,   0,   0,   0,   0,   0,   0,` |
|         - |  839 | `       0,   0,   0,   0,  28,   0,   0,   0,   0,   0,   0,   0,   0,` |
|         - |  840 | `       0,   0,   0,   0,   0,  44,   0,  18,   0,   0,   0,   0,   0,` |
|         - |  841 | `       0,  46,   0,  29,   0,   0,   0,  22,   0,   0,   0,   0,  26,` |
|         - |  842 | `       0,  21,  24,   0,   0,  68,   0,   0,   9,  37,   0,   0,   0,` |
|         - |  843 | `      42,   0,   0,   0,  70,  55` |
|         - |  844 | `  };` |
|         - |  845 | `  static const unsigned char aLen[84] = {` |
|         - |  846 | `       7,   9,   6,   5,   7,  12,   7,   2,  10,   7,   6,   9,   4,` |
|         - |  847 | `       6,   5,   7,   4,   3,   7,   3,   4,   4,   5,   4,   6,   5,` |
|         - |  848 | `       2,   3,   5,   6,   6,   3,   6,   4,   2,   5,   3,   5,   3,` |
|         - |  849 | `       3,   4,   3,   4,   8,   5,   2,   8,   5,   8,   3,   8,   5,` |
|         - |  850 | `       4,   2,   4,   4,  10,  12,   7,   5,  10,   9,   3,   6,  10,` |
|         - |  851 | `       3,   7,   2,   5,   6,   7,   9,   6,   5,   5,   3,   5,   2,` |
|         - |  852 | `       5,   4,   5,   3,   2,   5` |
|         - |  853 | `  };` |
|         - |  854 | `  static const sxu16 aOffset[84] = {` |
|         - |  855 | `       0,   3,   6,  12,  14,  20,  20,  21,  31,  34,  39,  44,  52,` |
|         - |  856 | `      55,  60,  65,  65,  70,  72,  78,  81,  83,  86,  90,  92,  97,` |
|         - |  857 | `     100, 100, 103, 106, 111, 117, 119, 119, 123, 124, 129, 130, 135,` |
|         - |  858 | `     137, 139, 143, 145, 149, 157, 159, 162, 169, 173, 181, 183, 186,` |
|         - |  859 | `     190, 194, 196, 200, 204, 214, 214, 225, 230, 240, 240, 248, 248,` |
|         - |  860 | `     251, 251, 252, 258, 263, 269, 276, 285, 290, 295, 300, 303, 308,` |
|         - |  861 | `     310, 315, 319, 324, 325, 327` |
|         - |  862 | `  };` |
|         - |  863 | `  static const sxu32 aCode[84] = {` |
|         - |  864 | `    PH7_TKWRD_EXTENDS,   PH7_TKWRD_ENDSWITCH,   PH7_TKWRD_SWITCH,    PH7_TKWRD_PRINT,   PH7_TKWRD_INT,` |
|         - |  865 | `    PH7_TKWRD_REQONCE,   PH7_TKWRD_REQUIRE,     PH7_TK_ID /* 'eq' PH7-ism removed */, PH7_TKWRD_ENDDEC, PH7_TKWRD_DECLARE,` |
|         - |  866 | `    PH7_TKWRD_RETURN,    PH7_TKWRD_NAMESPACE,   PH7_TKWRD_ECHO,      PH7_TKWRD_OBJECT,    PH7_TKWRD_THROW,` |
|         - |  867 | `    PH7_TKWRD_BOOL,      PH7_TKWRD_BOOL,        PH7_TKWRD_AND,       PH7_TKWRD_DEFAULT,   PH7_TKWRD_TRY,` |
|         - |  868 | `    PH7_TKWRD_CASE,      PH7_TKWRD_SELF,        PH7_TKWRD_FINAL,     PH7_TKWRD_LIST,      PH7_TKWRD_STATIC,` |
|         - |  869 | `    PH7_TKWRD_CLONE,     PH7_TK_ID /* 'ne' PH7-ism removed */, PH7_TKWRD_NEW,  PH7_TKWRD_CONST,     PH7_TKWRD_STRING,` |
|         - |  870 | `    PH7_TKWRD_GLOBAL,    PH7_TKWRD_USE,         PH7_TKWRD_ELIF,      PH7_TKWRD_ELSE,      PH7_TKWRD_IF,` |
|         - |  871 | `    PH7_TKWRD_FLOAT,     PH7_TKWRD_VAR,         PH7_TKWRD_ARRAY,     PH7_TKWRD_AND,       PH7_TKWRD_DIE,` |
|         - |  872 | `    PH7_TKWRD_ECHO,      PH7_TKWRD_USE,         PH7_TKWRD_ECHO,      PH7_TKWRD_ABSTRACT,  PH7_TKWRD_CLASS,` |
|         - |  873 | `    PH7_TKWRD_AS,        PH7_TKWRD_CONTINUE,    PH7_TKWRD_ENDIF,     PH7_TKWRD_FUNCTION,  PH7_TKWRD_DIE,` |
|         - |  874 | `    PH7_TKWRD_ENDWHILE,  PH7_TKWRD_WHILE,       PH7_TKWRD_EVAL,      PH7_TKWRD_DO,        PH7_TKWRD_EXIT,` |
|         - |  875 | `    PH7_TKWRD_GOTO,      PH7_TKWRD_IMPLEMENTS,  PH7_TKWRD_INCONCE,   PH7_TKWRD_INCLUDE,   PH7_TKWRD_EMPTY,` |
|         - |  876 | `    PH7_TKWRD_INSTANCEOF,PH7_TKWRD_INTERFACE,   PH7_TKWRD_INT,       PH7_TKWRD_ENDFOR,    PH7_TKWRD_END4EACH,` |
|         - |  877 | `    PH7_TKWRD_FOR,       PH7_TKWRD_FOREACH,     PH7_TKWRD_OR,        PH7_TKWRD_ISSET,     PH7_TKWRD_PARENT,` |
|         - |  878 | `    PH7_TKWRD_PRIVATE,   PH7_TKWRD_PROTECTED,   PH7_TKWRD_PUBLIC,    PH7_TKWRD_CATCH,     PH7_TKWRD_UNSET,` |
|         - |  879 | `    PH7_TKWRD_XOR,       PH7_TKWRD_ARRAY,       PH7_TKWRD_AS,        PH7_TKWRD_ARRAY,     PH7_TKWRD_EXIT,` |
|         - |  880 | `    PH7_TKWRD_UNSET,     PH7_TKWRD_XOR,         PH7_TKWRD_OR,        PH7_TKWRD_BREAK` |
|         - |  881 | `  };` |
|         - |  882 | `  int h, i;` |
|  58047693 |  883 | `  if( n<2 ) return PH7_TK_ID;` |
|  50787739 |  884 | `  h = (((int)z[0]*4) ^ ((int)z[n-1]*3) ^ n) % 151;` |
|  78348573 |  885 | `  for(i=((int)aHash[h])-1; i>=0; i=((int)aNext[i])-1){` |
|  46898721 |  886 | `    if( (int)aLen[i]==n && SyMemcmp(&zText[aOffset[i]],z,n)==0 ){` |
|         - |  887 | `       /* PH7_TKWRD_EXTENDS */` |
|         - |  888 | `       /* PH7_TKWRD_ENDSWITCH */` |
|         - |  889 | `       /* PH7_TKWRD_SWITCH */` |
|         - |  890 | `       /* PH7_TKWRD_PRINT */` |
|         - |  891 | `       /* PH7_TKWRD_INT */` |
|         - |  892 | `       /* PH7_TKWRD_REQONCE */` |
|         - |  893 | `       /* PH7_TKWRD_REQUIRE */` |
|         - |  894 | `       /* PH7_TK_ID */` |
|         - |  895 | `       /* PH7_TKWRD_ENDDEC */` |
|         - |  896 | `       /* PH7_TKWRD_DECLARE */` |
|         - |  897 | `       /* PH7_TKWRD_RETURN */` |
|         - |  898 | `       /* PH7_TKWRD_NAMESPACE */` |
|         - |  899 | `       /* PH7_TKWRD_ECHO */` |
|         - |  900 | `       /* PH7_TKWRD_OBJECT */` |
|         - |  901 | `       /* PH7_TKWRD_THROW */` |
|         - |  902 | `       /* PH7_TKWRD_BOOL */` |
|         - |  903 | `       /* PH7_TKWRD_BOOL */` |
|         - |  904 | `       /* PH7_TKWRD_AND */` |
|         - |  905 | `       /* PH7_TKWRD_DEFAULT */` |
|         - |  906 | `       /* PH7_TKWRD_TRY */` |
|         - |  907 | `       /* PH7_TKWRD_CASE */` |
|         - |  908 | `       /* PH7_TKWRD_SELF */` |
|         - |  909 | `       /* PH7_TKWRD_FINAL */` |
|         - |  910 | `       /* PH7_TKWRD_LIST */` |
|         - |  911 | `       /* PH7_TKWRD_STATIC */` |
|         - |  912 | `       /* PH7_TKWRD_CLONE */` |
|         - |  913 | `       /* PH7_TK_ID */` |
|         - |  914 | `       /* PH7_TKWRD_NEW */` |
|         - |  915 | `       /* PH7_TKWRD_CONST */` |
|         - |  916 | `       /* PH7_TKWRD_STRING */` |
|         - |  917 | `       /* PH7_TKWRD_GLOBAL */` |
|         - |  918 | `       /* PH7_TKWRD_USE */` |
|         - |  919 | `       /* PH7_TKWRD_ELIF */` |
|         - |  920 | `       /* PH7_TKWRD_ELSE */` |
|         - |  921 | `       /* PH7_TKWRD_IF */` |
|         - |  922 | `       /* PH7_TKWRD_FLOAT */` |
|         - |  923 | `       /* PH7_TKWRD_VAR */` |
|         - |  924 | `       /* PH7_TKWRD_ARRAY */` |
|         - |  925 | `       /* PH7_TKWRD_AND */` |
|         - |  926 | `       /* PH7_TKWRD_DIE */` |
|         - |  927 | `       /* PH7_TKWRD_ECHO */` |
|         - |  928 | `       /* PH7_TKWRD_USE */` |
|         - |  929 | `       /* PH7_TKWRD_ECHO */` |
|         - |  930 | `       /* PH7_TKWRD_ABSTRACT */` |
|         - |  931 | `       /* PH7_TKWRD_CLASS */` |
|         - |  932 | `       /* PH7_TKWRD_AS */` |
|         - |  933 | `       /* PH7_TKWRD_CONTINUE */` |
|         - |  934 | `       /* PH7_TKWRD_ENDIF */` |
|         - |  935 | `       /* PH7_TKWRD_FUNCTION */` |
|         - |  936 | `       /* PH7_TKWRD_DIE */` |
|         - |  937 | `       /* PH7_TKWRD_ENDWHILE */` |
|         - |  938 | `       /* PH7_TKWRD_WHILE */` |
|         - |  939 | `       /* PH7_TKWRD_EVAL */` |
|         - |  940 | `       /* PH7_TKWRD_DO */` |
|         - |  941 | `       /* PH7_TKWRD_EXIT */` |
|         - |  942 | `       /* PH7_TKWRD_GOTO */` |
|         - |  943 | `       /* PH7_TKWRD_IMPLEMENTS */` |
|         - |  944 | `       /* PH7_TKWRD_INCONCE */` |
|         - |  945 | `       /* PH7_TKWRD_INCLUDE */` |
|         - |  946 | `       /* PH7_TKWRD_EMPTY */` |
|         - |  947 | `       /* PH7_TKWRD_INSTANCEOF */` |
|         - |  948 | `       /* PH7_TKWRD_INTERFACE */` |
|         - |  949 | `       /* PH7_TKWRD_INT */` |
|         - |  950 | `       /* PH7_TKWRD_ENDFOR */` |
|         - |  951 | `       /* PH7_TKWRD_END4EACH */` |
|         - |  952 | `       /* PH7_TKWRD_FOR */` |
|         - |  953 | `       /* PH7_TKWRD_FOREACH */` |
|         - |  954 | `       /* PH7_TKWRD_OR */` |
|         - |  955 | `       /* PH7_TKWRD_ISSET */` |
|         - |  956 | `       /* PH7_TKWRD_PARENT */` |
|         - |  957 | `       /* PH7_TKWRD_PRIVATE */` |
|         - |  958 | `       /* PH7_TKWRD_PROTECTED */` |
|         - |  959 | `       /* PH7_TKWRD_PUBLIC */` |
|         - |  960 | `       /* PH7_TKWRD_CATCH */` |
|         - |  961 | `       /* PH7_TKWRD_UNSET */` |
|         - |  962 | `       /* PH7_TKWRD_XOR */` |
|         - |  963 | `       /* PH7_TKWRD_ARRAY */` |
|         - |  964 | `       /* PH7_TKWRD_AS */` |
|         - |  965 | `       /* PH7_TKWRD_ARRAY */` |
|         - |  966 | `       /* PH7_TKWRD_EXIT */` |
|         - |  967 | `       /* PH7_TKWRD_UNSET */` |
|         - |  968 | `       /* PH7_TKWRD_XOR */` |
|         - |  969 | `       /* PH7_TKWRD_OR */` |
|         - |  970 | `       /* PH7_TKWRD_BREAK */` |
|  19337887 |  971 | `      return aCode[i];` |
|         - |  972 | `    }` |
|  13780420 |  973 | `  }` |
|         - |  974 | `  /* Linear fallback for keywords not in the auto-generated hash table */` |
|  31449857 |  975 | `  if( n==5 && SyMemcmp(z,"trait",5)==0 ) return PH7_TKWRD_TRAIT;` |
|  31442003 |  976 | `  if( n==9 && SyMemcmp(z,"insteadof",9)==0 ) return PH7_TKWRD_INSTEADOF;` |
|  31441999 |  977 | `  if( n==7 && SyMemcmp(z,"finally",7)==0 ) return PH7_TKWRD_FINALLY;` |
|  31441827 |  978 | `  if( n==5 && SyMemcmp(z,"yield",5)==0 ) return PH7_TKWRD_YIELD;` |
|  31425909 |  979 | `  if( n==5 && SyMemcmp(z,"match",5)==0 ) return PH7_TKWRD_MATCH;` |
|  31425831 |  980 | `  return PH7_TK_ID;` |
|  29023849 |  981 | `}` |
|         - |  982 | `/* --- End of Automatically generated code --- */` |
|         - |  983 | `/* SPDX-SnippetEnd */` |
|         - |  984 | `/*` |
|         - |  985 | ` * Extract a heredoc/nowdoc text from a raw PHP input.` |
|         - |  986 | ` * According to the PHP language reference manual:` |
|         - |  987 | ` *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier` |
|         - |  988 | ` *  is provided, then a newline. The string itself follows, and then the same identifier again` |
|         - |  989 | ` *  to close the quotation.` |
|         - |  990 | ` *  The closing identifier must begin in the first column of the line. Also, the identifier must` |
|         - |  991 | ` *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric` |
|         - |  992 | ` *  characters and underscores, and must start with a non-digit character or underscore.` |
|         - |  993 | ` *  Heredoc text behaves just like a double-quoted string, without the double quotes.` |
|         - |  994 | ` *  This means that quotes in a heredoc do not need to be escaped, but the escape codes listed` |
|         - |  995 | ` *  above can still be used. Variables are expanded, but the same care must be taken when expressing` |
|         - |  996 | ` *  complex variables inside a heredoc as with strings.` |
|         - |  997 | ` *  Nowdocs are to single-quoted strings what heredocs are to double-quoted strings.` |
|         - |  998 | ` *  A nowdoc is specified similarly to a heredoc, but no parsing is done inside a nowdoc.` |
|         - |  999 | ` *  The construct is ideal for embedding PHP code or other large blocks of text without the need` |
|         - | 1000 | ` *  for escaping. It shares some features in common with the SGML <![CDATA[ ]]> construct, in that` |
|         - | 1001 | ` *  it declares a block of text which is not for parsing.` |
|         - | 1002 | ` *  A nowdoc is identified with the same <<< sequence used for heredocs, but the identifier which follows` |
|         - | 1003 | ` *  is enclosed in single quotes, e.g. <<<'EOT'. All the rules for heredoc identifiers also apply to nowdoc` |
|         - | 1004 | ` *  identifiers, especially those regarding the appearance of the closing identifier.` |
|         - | 1005 | ` * Symisc Extension:` |
|         - | 1006 | ` * The closing delimiter can now start with a digit or undersocre or it can be an UTF-8 stream.` |
|         - | 1007 | ` * Example:` |
|         - | 1008 | ` *  <<<123` |
|         - | 1009 | ` *    HEREDOC Here` |
|         - | 1010 | ` * 123` |
|         - | 1011 | ` *  or` |
|         - | 1012 | ` *  <<<___` |
|         - | 1013 | ` *   HEREDOC Here` |
|         - | 1014 | ` *  ___` |
|         - | 1015 | ` */` |
|       122 | 1016 | `static sxi32 LexExtractHeredoc(SyStream *pStream,SyToken *pToken)` |
|         5 | 1017 | `{` |
|       127 | 1018 | `	const unsigned char *zIn  = pStream->zText;` |
|       127 | 1019 | `	const unsigned char *zEnd = pStream->zEnd;` |
|         - | 1020 | `	const unsigned char *zPtr;` |
|       127 | 1021 | `	sxu8 bNowDoc = FALSE;` |
|         - | 1022 | `	SyString sDelim;` |
|         - | 1023 | `	SyString sStr;` |
|         - | 1024 | `	/* Jump leading white spaces */` |
|       139 | 1025 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|        13 | 1026 | `		zIn++;` |
|         1 | 1027 | `	}` |
|       127 | 1028 | `	if( zIn >= zEnd ){` |
|         - | 1029 | `		/* A simple symbol,return immediately */` |
|       ! 0 | 1030 | `		return SXERR_CONTINUE;` |
|         - | 1031 | `	}` |
|       127 | 1032 | `	if( zIn[0] == '\'' \|\| zIn[0] == '"' ){` |
|         - | 1033 | `		/* Make sure we are dealing with a nowdoc */` |
|        54 | 1034 | `		bNowDoc =  zIn[0] == '\'' ? TRUE : FALSE;` |
|        54 | 1035 | `		zIn++;` |
|        25 | 1036 | `	}` |
|       127 | 1037 | `	if( zIn[0] < 0xc0 && !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|         - | 1038 | `		/* Invalid delimiter,return immediately */` |
|       ! 0 | 1039 | `		return SXERR_CONTINUE;` |
|         - | 1040 | `	}` |
|         - | 1041 | `	/* Isolate the identifier */` |
|       127 | 1042 | `	sDelim.zString = (const char *)zIn;` |
|       130 | 1043 | `	for(;;){` |
|       265 | 1044 | `		zPtr = zIn;` |
|         - | 1045 | `		/* Skip alphanumeric stream */` |
|       831 | 1046 | `		while( zPtr < zEnd && zPtr[0] < 0xc0 && (SyisAlphaNum(zPtr[0]) \|\| zPtr[0] == '_') ){` |
|       441 | 1047 | `			zPtr++;` |
|         5 | 1048 | `		}` |
|       265 | 1049 | `		if( zPtr < zEnd && zPtr[0] >= 0xc0 ){` |
|        19 | 1050 | `			zPtr++;` |
|         - | 1051 | `			/* UTF-8 stream */` |
|        37 | 1052 | `			while( zPtr < zEnd && ((zPtr[0] & 0xc0) == 0x80) ){` |
|        19 | 1053 | `				zPtr++;` |
|         1 | 1054 | `			}` |
|         9 | 1055 | `		}` |
|       265 | 1056 | `		if( zPtr == zIn ){` |
|         - | 1057 | `			/* Not an UTF-8 or alphanumeric stream */` |
|       127 | 1058 | `			break;` |
|         - | 1059 | `		}` |
|         - | 1060 | `		/* Synchronize pointers */` |
|       143 | 1061 | `		zIn = zPtr;` |
|         5 | 1062 | `	}` |
|         - | 1063 | `	/* Get the identifier length */` |
|       127 | 1064 | `	sDelim.nByte = (sxu32)((const char *)zIn-sDelim.zString);` |
|       127 | 1065 | `	if( zIn[0] == '"' \|\| (bNowDoc && zIn[0] == '\'') ){` |
|         - | 1066 | `		/* Jump the trailing single quote */` |
|        54 | 1067 | `		zIn++;` |
|        25 | 1068 | `	}` |
|         - | 1069 | `	/* Jump trailing white spaces */` |
|       127 | 1070 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|       ! 0 | 1071 | `		zIn++;` |
|       ! 0 | 1072 | `	}` |
|       127 | 1073 | `	if( sDelim.nByte <= 0 \|\| zIn >= zEnd \|\| zIn[0] != '\n' ){` |
|         - | 1074 | `		/* Invalid syntax */` |
|       ! 0 | 1075 | `		return SXERR_CONTINUE;` |
|         - | 1076 | `	}` |
|       127 | 1077 | `	pStream->nLine++; /* Increment line counter */` |
|       127 | 1078 | `	zIn++;` |
|         - | 1079 | `	/* Isolate the delimited string */` |
|       127 | 1080 | `	sStr.zString = (const char *)zIn;` |
|         - | 1081 | `	/* PHP 7.3 flexible heredoc/nowdoc: the closing marker may be preceded` |
|         - | 1082 | `	 * by whitespace (spaces/tabs), and may be followed by any non-identifier` |
|         - | 1083 | `	 * character. The indent count is recorded in pToken->pUserData and the` |
|         - | 1084 | `	 * compile phase strips it from each body line. */` |
|         - | 1085 | `	{` |
|       127 | 1086 | `		const unsigned char *zMarkerLine = zIn; /* Start of marker's line (set on match) */` |
|       127 | 1087 | `		sxu32 nIndent = 0;` |
|       271 | 1088 | `		for(;;){` |
|       337 | 1089 | `			const unsigned char *zLineStart = zIn;` |
|         - | 1090 | `			/* Skip leading space/tab on this line */` |
|       893 | 1091 | `			while( zIn < zEnd && (zIn[0] == ' ' \|\| zIn[0] == '\t') ){` |
|       393 | 1092 | `				zIn++;` |
|         3 | 1093 | `			}` |
|       332 | 1094 | `			if( (sxu32)(zEnd - zIn) >= sDelim.nByte` |
|       336 | 1095 | `				&& SyMemcmp((const void *)sDelim.zString,(const void *)zIn,sDelim.nByte) == 0 ){` |
|         - | 1096 | `				int bIdentCont;` |
|       125 | 1097 | `				zPtr = &zIn[sDelim.nByte];` |
|         - | 1098 | `				/* Disambiguate: next byte must not continue an identifier.` |
|         - | 1099 | `				 * A leading byte >= 0xc0 starts a multi-byte UTF-8 sequence,` |
|         - | 1100 | `				 * which PHP identifiers may contain, so treat it as ident. */` |
|       125 | 1101 | `				if( zPtr >= zEnd ){` |
|       ! 0 | 1102 | `					bIdentCont = 0;` |
|       125 | 1103 | `				}else if( zPtr[0] >= 0xc0 ){` |
|       ! 0 | 1104 | `					bIdentCont = 1;` |
|       ! 0 | 1105 | `				}else{` |
|       125 | 1106 | `					bIdentCont = (SyisAlphaNum(zPtr[0]) \|\| zPtr[0] == '_');` |
|         - | 1107 | `				}` |
|       125 | 1108 | `				if( !bIdentCont ){` |
|         - | 1109 | `					/* Closing marker found */` |
|       125 | 1110 | `					nIndent = (sxu32)(zIn - zLineStart);` |
|       125 | 1111 | `					zMarkerLine = zLineStart;` |
|       125 | 1112 | `					pStream->zText = zPtr; /* Cursor right after identifier */` |
|       125 | 1113 | `					break;` |
|         - | 1114 | `				}` |
|       ! 0 | 1115 | `			}` |
|         - | 1116 | `			/* Not the closing marker on this line; walk to next newline */` |
|      4533 | 1117 | `			while( zIn < zEnd && zIn[0] != '\n' ){` |
|      4321 | 1118 | `				zIn++;` |
|         5 | 1119 | `			}` |
|       217 | 1120 | `			if( zIn >= zEnd ){` |
|         - | 1121 | `				/* End of input without finding the closing marker */` |
|         3 | 1122 | `				pStream->zText = pStream->zEnd;` |
|         3 | 1123 | `				zMarkerLine = zIn;` |
|         3 | 1124 | `				break;` |
|         - | 1125 | `			}` |
|       215 | 1126 | `			pStream->nLine++;` |
|       215 | 1127 | `			zIn++;` |
|         5 | 1128 | `		}` |
|         - | 1129 | `		/* Body runs from sStr.zString up to just before the marker line */` |
|       127 | 1130 | `		sStr.nByte = (sxu32)((const char *)zMarkerLine - sStr.zString);` |
|       127 | 1131 | `		pToken->nType = bNowDoc ? PH7_TK_NOWDOC : PH7_TK_HEREDOC;` |
|       127 | 1132 | `		SyStringDupPtr(&pToken->sData,&sStr);` |
|         - | 1133 | `		/* Strip exactly one line terminator that precedes the marker's line. */` |
|       122 | 1134 | `		if( pToken->sData.nByte > 0` |
|       123 | 1135 | `			&& pToken->sData.zString[pToken->sData.nByte - 1] == '\n' ){` |
|       117 | 1136 | `			pToken->sData.nByte--;` |
|       112 | 1137 | `			if( pToken->sData.nByte > 0` |
|       117 | 1138 | `				&& pToken->sData.zString[pToken->sData.nByte - 1] == '\r' ){` |
|       ! 0 | 1139 | `				pToken->sData.nByte--;` |
|       ! 0 | 1140 | `			}` |
|        56 | 1141 | `		}` |
|       127 | 1142 | `		pToken->pUserData = SX_INT_TO_PTR(nIndent);` |
|         - | 1143 | `	}` |
|         - | 1144 | `	/* All done */` |
|       127 | 1145 | `	return SXRET_OK;` |
|        66 | 1146 | `}` |
|         - | 1147 | `/*` |
|         - | 1148 | ` * Tokenize a raw PHP input.` |
|         - | 1149 | ` * This is the public tokenizer called by most code generator routines.` |
|         - | 1150 | ` */` |
|     87106 | 1151 | `PH7_PRIVATE sxi32 PH7_TokenizePHP(const char *zInput,sxu32 nLen,sxu32 nLineStart,SySet *pOut,SySet *pTrivia)` |
|         5 | 1152 | `{` |
|         - | 1153 | `	SyLex sLexer;` |
|         - | 1154 | `	sxi32 rc;` |
|         - | 1155 | `	/* Defense-in-depth cap for internal tokenizer calls that bypass ph7_compile() */` |
|     87111 | 1156 | `	if( nLen > PH7_MAX_INPUT_SIZE ){` |
|       ! 0 | 1157 | `		return SXERR_LIMIT;` |
|         - | 1158 | `	}` |
|         - | 1159 | `	/* Initialize the lexer. pTrivia (may be NULL = discard) rides as the` |
|         - | 1160 | `	 * tokenizer callback's user data: doc-comments (and later attribute` |
|         - | 1161 | `	 * groups) are recorded there instead of entering the token stream. */` |
|     87111 | 1162 | `	rc = SyLexInit(&sLexer,&(*pOut),TokenizePHP,pTrivia);` |
|     87111 | 1163 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 1164 | `		return rc;` |
|         - | 1165 | `	}` |
|     87111 | 1166 | `	sLexer.sStream.nLine = nLineStart;` |
|         - | 1167 | `	/* Tokenize input */` |
|     87111 | 1168 | `	rc = SyLexTokenizeInput(&sLexer,zInput,nLen,0,0,0);` |
|         - | 1169 | `	/* Release the lexer */` |
|     87111 | 1170 | `	SyLexRelease(&sLexer);` |
|         - | 1171 | `	/* Tokenization result */` |
|     87111 | 1172 | `	return rc;` |
|     43558 | 1173 | `}` |
|         - | 1174 | `/*` |
|         - | 1175 | ` * High level public tokenizer.` |
|         - | 1176 | ` *  Tokenize the input into PHP tokens and raw tokens [i.e: HTML,XML,Raw text...].` |
|         - | 1177 | ` * According to the PHP language reference manual` |
|         - | 1178 | ` *   When PHP parses a file, it looks for opening and closing tags, which tell PHP` |
|         - | 1179 | ` *   to start and stop interpreting the code between them. Parsing in this manner allows` |
|         - | 1180 | ` *   PHP to be embedded in all sorts of different documents, as everything outside of a pair` |
|         - | 1181 | ` *   of opening and closing tags is ignored by the PHP parser. Most of the time you will see` |
|         - | 1182 | ` *   PHP embedded in HTML documents, as in this example.` |
|         - | 1183 | ` *   <?php echo 'While this is going to be parsed.'; ?>` |
|         - | 1184 | ` *   <p>This will also be ignored.</p>` |
|         - | 1185 | ` *   You can also use more advanced structures:` |
|         - | 1186 | ` *   Example #1 Advanced escaping` |
|         - | 1187 | ` * <?php` |
|         - | 1188 | ` * if ($expression) {` |
|         - | 1189 | ` *   ?>` |
|         - | 1190 | ` *   <strong>This is true.</strong>` |
|         - | 1191 | ` *   <?php` |
|         - | 1192 | ` * } else {` |
|         - | 1193 | ` *   ?>` |
|         - | 1194 | ` *   <strong>This is false.</strong>` |
|         - | 1195 | ` *   <?php` |
|         - | 1196 | ` * }` |
|         - | 1197 | ` * ?>` |
|         - | 1198 | ` * This works as expected, because when PHP hits the ?> closing tags, it simply starts outputting` |
|         - | 1199 | ` * whatever it finds (except for an immediately following newline - see instruction separation ) until it hits` |
|         - | 1200 | ` * another opening tag. The example given here is contrived, of course, but for outputting large blocks of text` |
|         - | 1201 | ` * dropping out of PHP parsing mode is generally more efficient than sending all of the text through echo() or print().` |
|         - | 1202 | ` * There are four different pairs of opening and closing tags which can be used in PHP. Three of those, <?php ?>` |
|         - | 1203 | ` * <script language="php"> </script>  and <? ?> are always available. The other two are short tags and ASP style` |
|         - | 1204 | ` * tags, and can be turned on and off from the php.ini configuration file. As such, while some people find short tags` |
|         - | 1205 | ` * and ASP style tags convenient, they are less portable, and generally not recommended.` |
|         - | 1206 | ` * Note:` |
|         - | 1207 | ` * Also note that if you are embedding PHP within XML or XHTML you will need to use the <?php ?> tags to remain` |
|         - | 1208 | ` * compliant with standards.` |
|         - | 1209 | ` * Example #2 PHP Opening and Closing Tags` |
|         - | 1210 | ` * 1.  <?php echo 'if you want to serve XHTML or XML documents, do it like this'; ?>` |
|         - | 1211 | ` * 2.  <script language="php">` |
|         - | 1212 | ` *       echo 'some editors (like FrontPage) don\'t` |
|         - | 1213 | ` *             like processing instructions';` |
|         - | 1214 | ` *   </script>` |
|         - | 1215 | ` *` |
|         - | 1216 | ` * 3.  <? echo 'this is the simplest, an SGML processing instruction'; ?>` |
|         - | 1217 | ` *   <?= expression ?> This is a shortcut for "<? echo expression ?>"` |
|         - | 1218 | ` */` |
|     13706 | 1219 | `PH7_PRIVATE sxi32 PH7_TokenizeRawText(const char *zInput,sxu32 nLen,SySet *pOut,sxu32 nBaseLine)` |
|         5 | 1220 | `{` |
|     13711 | 1221 | `	const char *zEnd = &zInput[nLen];` |
|     13711 | 1222 | `	const char *zIn  = zInput;` |
|         - | 1223 | `	const char *zCur,*zCurEnd;` |
|     13711 | 1224 | `	SyString sCtag = { 0, 0 };     /* Closing tag */` |
|         - | 1225 | `	SyToken sToken;` |
|         - | 1226 | `	SyString sDoc;` |
|         - | 1227 | `	sxu32 nLine;` |
|         - | 1228 | `	sxi32 iNest;` |
|         - | 1229 | `	sxi32 rc;` |
|         - | 1230 | `	/* Tokenize the input into PHP tokens and raw tokens. nBaseLine is normally 1,` |
|         - | 1231 | `	 * but 2 when a "#!" shebang line was stripped so error lines still match php. */` |
|     13711 | 1232 | `	nLine = nBaseLine;` |
|     13711 | 1233 | `	zCur = zCurEnd   = 0; /* Prevent compiler warning */` |
|     13711 | 1234 | `	sToken.pUserData = 0;` |
|     13711 | 1235 | `	iNest = 0;` |
|     13711 | 1236 | `	sDoc.nByte = 0;` |
|     13711 | 1237 | `	sDoc.zString = ""; /* cc warning */` |
|     13704 | 1238 | `	for(;;){` |
|     27243 | 1239 | `		if( zIn >= zEnd ){` |
|         - | 1240 | `			/* End of input reached */` |
|     13525 | 1241 | `			break;` |
|         - | 1242 | `		}` |
|     13723 | 1243 | `		sToken.nLine = nLine;` |
|     13723 | 1244 | `		zCur = zIn;` |
|     13723 | 1245 | `		zCurEnd = 0;` |
|     14019 | 1246 | `		while( zIn < zEnd ){` |
|     13833 | 1247 | `			 if( zIn[0] == '<' ){` |
|     13537 | 1248 | `				const char *zTmp = zIn; /* End of raw input marker */` |
|     13537 | 1249 | `				zIn++;` |
|     13537 | 1250 | `				if( zIn < zEnd ){` |
|     13537 | 1251 | `					if( zIn[0] == '?' ){` |
|     13537 | 1252 | `						zIn++;` |
|     13537 | 1253 | `						if( (sxu32)(zEnd - zIn) >= sizeof("php")-1 &&  SyStrnicmp(zIn,"php",sizeof("php")-1) == 0 ){` |
|         - | 1254 | `							/* opening tag: <?php */` |
|     13535 | 1255 | `							zIn += sizeof("php")-1;` |
|      6765 | 1256 | `						}` |
|         - | 1257 | `						/* Look for the closing tag '?>' */` |
|     13537 | 1258 | `						SyStringInitFromBuf(&sCtag,"?>",sizeof("?>")-1);` |
|     13537 | 1259 | `						zCurEnd = zTmp;` |
|     13537 | 1260 | `						break;` |
|         - | 1261 | `					}` |
|       ! 0 | 1262 | `				}` |
|       ! 0 | 1263 | `			}else{` |
|       301 | 1264 | `				if( zIn[0] == '\n' ){` |
|         8 | 1265 | `					nLine++;` |
|         3 | 1266 | `				}` |
|       301 | 1267 | `				zIn++;` |
|         - | 1268 | `			 }` |
|         5 | 1269 | `		} /* While(zIn < zEnd) */` |
|     13723 | 1270 | `		if( zCurEnd == 0 ){` |
|        19 | 1271 | `			zCurEnd = zIn;` |
|         8 | 1272 | `		}` |
|         - | 1273 | `		/* Save the raw token */` |
|     13723 | 1274 | `		SyStringInitFromBuf(&sToken.sData,zCur,zCurEnd - zCur);` |
|     13723 | 1275 | `		sToken.nType = PH7_TOKEN_RAW;` |
|     13723 | 1276 | `		rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|     13723 | 1277 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1278 | `			return rc;` |
|         - | 1279 | `		}` |
|     13723 | 1280 | `		if( zIn >= zEnd ){` |
|        19 | 1281 | `			break;` |
|         - | 1282 | `		}` |
|         - | 1283 | `		/* Ignore leading white space */` |
|     28975 | 1284 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|     15273 | 1285 | `			if( zIn[0] == '\n' ){` |
|     14799 | 1286 | `				nLine++;` |
|      7397 | 1287 | `			}` |
|     15273 | 1288 | `			zIn++;` |
|         5 | 1289 | `		}` |
|         - | 1290 | `		/* Delimit the PHP chunk */` |
|     13707 | 1291 | `		sToken.nLine = nLine;` |
|     13707 | 1292 | `		zCur = zIn;` |
|   1550119 | 1293 | `		while( (sxu32)(zEnd - zIn) >= sCtag.nByte ){` |
|         - | 1294 | `			const char *zPtr;` |
|   1543855 | 1295 | `			if( SyMemcmp(zIn,sCtag.zString,sCtag.nByte) == 0 && iNest < 1 ){` |
|      7273 | 1296 | `				break;` |
|         - | 1297 | `			}` |
|         - | 1298 | `			/* Line comment ('#' or '//', but not the '#[' attribute opener): php` |
|         - | 1299 | `			 * ends it at a newline OR at the closing tag, so a '?>' inside a line` |
|         - | 1300 | `			 * comment DOES close the PHP block. Skipping the comment here also` |
|         - | 1301 | `			 * stops the string skip below from treating a quote inside the` |
|         - | 1302 | `			 * comment as a string. Only outside a heredoc body (iNest < 1). */` |
|   1540427 | 1303 | `			if( iNest < 1 &&` |
|   1532610 | 1304 | `				( (zIn[0] == '#' && !(zIn+1 < zEnd && zIn[1] == '[')) \|\|` |
|   1532638 | 1305 | `				  (zIn[0] == '/' && zIn+1 < zEnd && zIn[1] == '/') ) ){` |
|      7685 | 1306 | `				zIn += (zIn[0] == '#') ? 1 : 2;` |
|    305671 | 1307 | `				while( zIn < zEnd && zIn[0] != '\n' ){` |
|    297988 | 1308 | `					if( (sxu32)(zEnd - zIn) >= sCtag.nByte` |
|    297990 | 1309 | `						&& SyMemcmp(zIn,sCtag.zString,sCtag.nByte) == 0 ){` |
|         3 | 1310 | `						break; /* the closing tag terminates the line comment */` |
|         - | 1311 | `					}` |
|    297991 | 1312 | `					zIn++;` |
|         5 | 1313 | `				}` |
|      7283 | 1314 | `				continue;` |
|         - | 1315 | `			}` |
|         - | 1316 | `			/* Block comment: spans everything, including '?>', up to its close. */` |
|   1529139 | 1317 | `			if( iNest < 1 && zIn[0] == '/' && zIn+1 < zEnd && zIn[1] == '*' ){` |
|       311 | 1318 | `				zIn += 2;` |
|     32825 | 1319 | `				while( (sxu32)(zEnd-zIn) >= sizeof("*/") - 1 ){` |
|     32825 | 1320 | `					if( zIn[0] == '*' && zIn[1] == '/' ){` |
|       311 | 1321 | `						zIn += 2;` |
|       311 | 1322 | `						break;` |
|         - | 1323 | `					}` |
|     32519 | 1324 | `					if( zIn[0] == '\n' ){` |
|       257 | 1325 | `						nLine++;` |
|       126 | 1326 | `					}` |
|     32519 | 1327 | `					zIn++;` |
|         5 | 1328 | `				}` |
|       311 | 1329 | `				continue;` |
|         - | 1330 | `			}` |
|         - | 1331 | `			/* Skip over a single/double-quoted or backtick string literal so a` |
|         - | 1332 | `			 * '?>' sequence inside it is not mistaken for the closing tag. Only` |
|         - | 1333 | `			 * outside a heredoc body (iNest < 1); heredocs are delimited by the` |
|         - | 1334 | `			 * label-matching logic above. Escapes (\" \' \\ and a line-continuing` |
|         - | 1335 | `			 * backslash-newline) are honoured. Same-quote nesting inside "{$...}"` |
|         - | 1336 | `			 * interpolation is not tracked, but that can only end the skip early` |
|         - | 1337 | `			 * on a string that has no '?>' anyway, which stays a PHP chunk either` |
|         - | 1338 | `			 * way — it never mis-splits code that works today. */` |
|   1528833 | 1339 | ``			if( iNest < 1 && (zIn[0] == '\'' \|\| zIn[0] == '"' \|\| zIn[0] == '`') ){`` |
|     46255 | 1340 | `				int qch = zIn[0];` |
|     46255 | 1341 | `				zIn++;` |
|    334785 | 1342 | `				while( zIn < zEnd ){` |
|    334785 | 1343 | `					if( zIn[0] == '\\' && zIn + 1 < zEnd ){` |
|     18191 | 1344 | `						if( zIn[1] == '\n' ){ nLine++; }` |
|     18191 | 1345 | `						zIn += 2;` |
|     18191 | 1346 | `						continue;` |
|         - | 1347 | `					}` |
|    316599 | 1348 | `					if( zIn[0] == qch ){ zIn++; break; }` |
|    270349 | 1349 | `					if( zIn[0] == '\n' ){ nLine++; }` |
|    270349 | 1350 | `					zIn++;` |
|         5 | 1351 | `				}` |
|     46255 | 1352 | `				continue;` |
|         - | 1353 | `			}` |
|   1482583 | 1354 | `			if( zIn[0] == '\n' ){` |
|     62333 | 1355 | `				nLine++;` |
|     62333 | 1356 | `				if( iNest > 0 ){` |
|       337 | 1357 | `					zIn++;` |
|       727 | 1358 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|       393 | 1359 | `						zIn++;` |
|         3 | 1360 | `					}` |
|       337 | 1361 | `					zPtr = zIn;` |
|      1679 | 1362 | `					while( zIn < zEnd ){` |
|      1679 | 1363 | `						if( (unsigned char)zIn[0] >= 0xc0 ){` |
|         - | 1364 | `							/* UTF-8 stream */` |
|        19 | 1365 | `							zIn++;` |
|        37 | 1366 | `							SX_JMP_UTF8(zIn,zEnd);` |
|      1666 | 1367 | `						}else if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       171 | 1368 | `							break;` |
|       ! 0 | 1369 | `						}else{` |
|      1329 | 1370 | `							zIn++;` |
|         - | 1371 | `						}` |
|         5 | 1372 | `					}` |
|       337 | 1373 | `					if( (sxu32)(zIn - zPtr) == sDoc.nByte && SyMemcmp(sDoc.zString,zPtr,sDoc.nByte) == 0 ){` |
|       125 | 1374 | `						iNest = 0;` |
|        60 | 1375 | `					}` |
|       337 | 1376 | `					continue;` |
|         5 | 1377 | `				}` |
|   1451253 | 1378 | `			}else if ( (sxu32)(zEnd - zIn) >= sizeof("<<<") && zIn[0] == '<' && zIn[1] == '<' && zIn[2] == '<' && iNest < 1){` |
|       127 | 1379 | `				zIn += sizeof("<<<")-1;` |
|       139 | 1380 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|        13 | 1381 | `					zIn++;` |
|         1 | 1382 | `				}` |
|       127 | 1383 | `				if( zIn[0] == '"' \|\| zIn[0] == '\'' ){` |
|        54 | 1384 | `					zIn++;` |
|        25 | 1385 | `				}` |
|       127 | 1386 | `				zPtr = zIn;` |
|       581 | 1387 | `				while( zIn < zEnd ){` |
|       581 | 1388 | `					if( (unsigned char)zIn[0] >= 0xc0 ){` |
|         - | 1389 | `						/* UTF-8 stream */` |
|        19 | 1390 | `						zIn++;` |
|        37 | 1391 | `						SX_JMP_UTF8(zIn,zEnd);` |
|       568 | 1392 | `					}else if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|        66 | 1393 | `						break;` |
|       ! 0 | 1394 | `					}else{` |
|       441 | 1395 | `						zIn++;` |
|         - | 1396 | `					}` |
|         5 | 1397 | `				}` |
|       127 | 1398 | `				SyStringInitFromBuf(&sDoc,zPtr,zIn-zPtr);` |
|       127 | 1399 | `				SyStringFullTrim(&sDoc);` |
|       127 | 1400 | `				if( sDoc.nByte > 0 ){` |
|       127 | 1401 | `					iNest++;` |
|        61 | 1402 | `				}` |
|       127 | 1403 | `				continue;` |
|         - | 1404 | `			}` |
|   1482129 | 1405 | `			zIn++;` |
|         - | 1406 |  |
|   1482129 | 1407 | `			if ( zIn >= zEnd )` |
|       ! 0 | 1408 | `				break;` |
|         5 | 1409 | `		}` |
|     13537 | 1410 | `		if( (sxu32)(zEnd - zIn) < sCtag.nByte ){` |
|      6269 | 1411 | `			zIn = zEnd;` |
|      3132 | 1412 | `		}` |
|     13537 | 1413 | `		if( zCur < zIn ){` |
|         - | 1414 | `			/* Save the PHP chunk for later processing */` |
|     10313 | 1415 | `			sToken.nType = PH7_TOKEN_PHP;` |
|     10313 | 1416 | `			SyStringInitFromBuf(&sToken.sData,zCur,zIn-zCur);` |
|     20431 | 1417 | `			SyStringRightTrim(&sToken.sData); /* Trim trailing white spaces */` |
|     10313 | 1418 | `			rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|     10313 | 1419 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 1420 | `				return rc;` |
|         - | 1421 | `			}` |
|      5154 | 1422 | `		}` |
|     13537 | 1423 | `		if( zIn < zEnd ){` |
|         - | 1424 | `			/* Jump the trailing closing tag */` |
|      7273 | 1425 | `			zIn += sCtag.nByte;` |
|         - | 1426 | `			/* php's lexer swallows exactly ONE newline immediately after the` |
|         - | 1427 | `			 * closing tag ("?>\n" emits nothing) */` |
|      7273 | 1428 | `			if( zIn < zEnd && zIn[0] == '\r' && zIn + 1 < zEnd && zIn[1] == '\n' ){` |
|       ! 0 | 1429 | `				zIn += 2;` |
|       ! 0 | 1430 | `				nLine++;` |
|      7273 | 1431 | `			}else if( zIn < zEnd && zIn[0] == '\n' ){` |
|        66 | 1432 | `				zIn++;` |
|        66 | 1433 | `				nLine++;` |
|        31 | 1434 | `			}` |
|      3634 | 1435 | `		}` |
|         5 | 1436 | `	} /* For(;;) */` |
|         - | 1437 |  |
|     13541 | 1438 | ` 	return SXRET_OK;` |
|      6773 | 1439 | `}` |
|         - | 1440 |  |
