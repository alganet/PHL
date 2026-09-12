# src/ph7/lex.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 800/857 lines (93.35%)

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
| 170732006 |   19 | `static sxi32 TokenizePHP(SyStream *pStream,SyToken *pToken,void *pUserData,void *pCtxData)` |
|         5 |   20 | `{` |
|         - |   21 | `	SyString *pStr;` |
|         - |   22 | `	sxi32 rc;` |
|         - |   23 | `	/* Ignore leading white spaces */` |
| 252137697 |   24 | `	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){` |
|         - |   25 | `		/* Advance the stream cursor */` |
|  81405691 |   26 | `		if( pStream->zText[0] == '\n' ){` |
|         - |   27 | `			/* Update line counter */` |
|     54847 |   28 | `			pStream->nLine++;` |
|     27421 |   29 | `		}` |
|  81405691 |   30 | `		pStream->zText++;` |
|         5 |   31 | `	}` |
| 170732011 |   32 | `	if( pStream->zText >= pStream->zEnd ){` |
|         - |   33 | `		/* End of input reached */` |
|       ! 0 |   34 | `		return SXERR_EOF;` |
|         - |   35 | `	}` |
|         - |   36 | `	/* Record token starting position and line */` |
| 170732011 |   37 | `	pToken->nLine = pStream->nLine;` |
| 170732011 |   38 | `	pToken->pUserData = 0;` |
| 170732011 |   39 | `	pStr = &pToken->sData;` |
| 170732011 |   40 | `	SyStringInitFromBuf(pStr,pStream->zText,0);` |
| 199156745 |   41 | `	if( pStream->zText[0] >= 0xc0 \|\| SyisAlpha(pStream->zText[0]) \|\| pStream->zText[0] == '_' ){` |
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
|  56849473 |   52 | `		if( pStream->zText[0] < 0xc0 ){` |
|  56849457 |   53 | `			pStream->zText++;` |
|  28424726 |   54 | `		}` |
|  53268604 |   55 | `		for(;;){` |
| 106537213 |   56 | `			zIn = pStream->zText;` |
| 106537213 |   57 | `			if( zIn[0] >= 0xc0 ){` |
|        49 |   58 | `				zIn++;` |
|         - |   59 | `				/* UTF-8 stream */` |
|       109 |   60 | `				while( zIn < pStream->zEnd && ((zIn[0] & 0xc0) == 0x80) ){` |
|        61 |   61 | `					zIn++;` |
|         1 |   62 | `				}` |
|        24 |   63 | `			}` |
|         - |   64 | `			/* Skip alphanumeric stream */` |
| 424982965 |   65 | `			while( zIn < pStream->zEnd && zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_') ){` |
| 265177153 |   66 | `				zIn++;` |
|         5 |   67 | `			}` |
| 106537213 |   68 | `			if( zIn == pStream->zText ){` |
|         - |   69 | `				/* Not an UTF-8 or alphanumeric stream */` |
|  56849473 |   70 | `				break;` |
|         - |   71 | `			}` |
|         - |   72 | `			/* Synchronize pointers */` |
|  49687745 |   73 | `			pStream->zText = zIn;` |
|         5 |   74 | `		}` |
|         - |   75 | `		/* Record token length */` |
|  56849473 |   76 | `		pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  56849473 |   77 | `		nKeyword = KeywordCode(pStr->zString,(int)pStr->nByte);` |
|         - |   78 | `		/* PHP 7.4: 'fn' is a keyword reserved for arrow functions.` |
|         - |   79 | `		 * The auto-generated perfect hash above doesn't know about it,` |
|         - |   80 | `		 * so intercept the 'fn' identifier here.` |
|         - |   81 | `		 */` |
|  56849468 |   82 | `		if( nKeyword == PH7_TK_ID && pStr->nByte == 2` |
|  19588444 |   83 | `			&& pStr->zString[0] == 'f' && pStr->zString[1] == 'n' ){` |
|       833 |   84 | `			nKeyword = PH7_TKWRD_FN;` |
|       414 |   85 | `		}` |
|  56849473 |   86 | `		if( nKeyword != PH7_TK_ID ){` |
|  18937475 |   87 | `			if( nKeyword &` |
|         - |   88 | `				(PH7_TKWRD_NEW\|PH7_TKWRD_CLONE\|PH7_TKWRD_AND\|PH7_TKWRD_XOR\|PH7_TKWRD_OR\|PH7_TKWRD_INSTANCEOF) ){` |
|         - |   89 | `					/* Alpha stream operators [i.e: new,clone,and,instanceof,or,xor],save the operator instance for later processing */` |
|    912191 |   90 | `					pToken->pUserData = (void *)PH7_ExprExtractOperator(pStr,0);` |
|         - |   91 | `					/* Mark as an operator */` |
|    912191 |   92 | `					pToken->nType = PH7_TK_ID\|PH7_TK_OP;` |
|    456098 |   93 | `			}else{` |
|         - |   94 | `				/* We are dealing with a keyword [i.e: while,foreach,class...],save the keyword ID */` |
|  18025289 |   95 | `				pToken->nType = PH7_TK_KEYWORD;` |
|  18025289 |   96 | `				pToken->pUserData = SX_INT_TO_PTR(nKeyword);` |
|         - |   97 | `			}` |
|   9468740 |   98 | `		}else{` |
|         - |   99 | `			/* A simple identifier */` |
|  37912003 |  100 | `			pToken->nType = PH7_TK_ID;` |
|         - |  101 | `		}` |
|  28424739 |  102 | `	}else{` |
|         - |  103 | `		sxi32 c;` |
|         - |  104 | `		/* Non-alpha stream */` |
| 113882543 |  105 | `		if( pStream->zText[0] == '#' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '[' ){` |
|      7857 |  106 | `			sxu32 nDepth = 1;` |
|         - |  107 | `			/* PHP 8 attribute group '#[ ... ]': skip the whole balanced group as` |
|         - |  108 | `			 * trivia (attributes are not stored yet). Brackets inside string` |
|         - |  109 | `			 * literals and comments must not affect the depth count. An` |
|         - |  110 | `			 * unterminated group is silently consumed up to EOF, consistent` |
|         - |  111 | `			 * with unterminated block comments below.` |
|         - |  112 | `			 */` |
|         - |  113 | `			const unsigned char *zGroupStart;` |
|      7857 |  114 | `			pStream->zText += 2;` |
|      7857 |  115 | `			zGroupStart = pStream->zText;` |
|    641973 |  116 | `			while( pStream->zText < pStream->zEnd && nDepth > 0 ){` |
|    634121 |  117 | `				sxi32 d = pStream->zText[0];` |
|    634121 |  118 | `				if( d == '[' ){` |
|        11 |  119 | `					nDepth++;` |
|    634116 |  120 | `				}else if( d == ']' ){` |
|      7867 |  121 | `					nDepth--;` |
|    630180 |  122 | `				}else if( d == '\'' \|\| d == '"' ){` |
|         - |  123 | `					/* String literal: scan for the matching unescaped quote */` |
|        41 |  124 | `					pStream->zText++;` |
|       277 |  125 | `					while( pStream->zText < pStream->zEnd ){` |
|       277 |  126 | `						if( pStream->zText[0] == '\\' && &pStream->zText[1] < pStream->zEnd ){` |
|         3 |  127 | `							if( pStream->zText[1] == '\n' ){` |
|       ! 0 |  128 | `								pStream->nLine++;` |
|       ! 0 |  129 | `							}` |
|         3 |  130 | `							pStream->zText += 2;` |
|         3 |  131 | `							continue;` |
|         - |  132 | `						}` |
|       275 |  133 | `						if( pStream->zText[0] == d ){` |
|        41 |  134 | `							break;` |
|         - |  135 | `						}` |
|       235 |  136 | `						if( pStream->zText[0] == '\n' ){` |
|       ! 0 |  137 | `							pStream->nLine++;` |
|       ! 0 |  138 | `						}` |
|       235 |  139 | `						pStream->zText++;` |
|         1 |  140 | `					}` |
|        41 |  141 | `					if( pStream->zText >= pStream->zEnd ){` |
|       ! 0 |  142 | `						break; /* Unterminated string literal */` |
|         1 |  143 | `					}` |
|         - |  144 | `					/* Fall through: consume the closing quote below */` |
|    626229 |  145 | `				}else if( d == '#' \|\| (d == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '/') ){` |
|         - |  146 | `					/* Inline comment inside the group */` |
|       ! 0 |  147 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] != '\n' ){` |
|       ! 0 |  148 | `						pStream->zText++;` |
|       ! 0 |  149 | `					}` |
|       ! 0 |  150 | `					continue; /* Let the outer loop count the newline */` |
|    626209 |  151 | `				}else if( d == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '*' ){` |
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
|    626209 |  165 | `				}else if( d == '\n' ){` |
|         7 |  166 | `					pStream->nLine++;` |
|         3 |  167 | `				}` |
|    634121 |  168 | `				pStream->zText++;` |
|         5 |  169 | `			}` |
|      7857 |  170 | `			if( pUserData && pStream->pSet ){` |
|         - |  171 | `				/* Record the group's inner span (between #[ and its balanced ])` |
|         - |  172 | `				 * in the trivia sidecar, keyed like doc-comments. */` |
|         - |  173 | `				ph7_trivia sTrivia;` |
|      7857 |  174 | `				const unsigned char *zGroupEnd = pStream->zText;` |
|      7857 |  175 | `				if( nDepth == 0 && zGroupEnd > zGroupStart ){` |
|      7857 |  176 | `					zGroupEnd--; /* Exclude the closing ']' */` |
|      3926 |  177 | `				}` |
|      7857 |  178 | `				sTrivia.nTokIdx = SySetUsed(pStream->pSet);` |
|      7857 |  179 | `				sTrivia.iKind = PH7_TRIVIA_ATTR;` |
|      7857 |  180 | `				SyStringInitFromBuf(&sTrivia.sText,(const char *)zGroupStart,(sxu32)(zGroupEnd - zGroupStart));` |
|      7857 |  181 | `				sTrivia.nLine = pToken->nLine;` |
|      7857 |  182 | `				SySetPut((SySet *)pUserData,(const void *)&sTrivia);` |
|      3926 |  183 | `			}` |
|         - |  184 | `			/* Tell the upper-layer to ignore this token */` |
|      7857 |  185 | `			return SXERR_CONTINUE;` |
| 113984382 |  186 | `		}else if( pStream->zText[0] == '#' \|\|` |
| 113874680 |  187 | `			( pStream->zText[0] == '/' &&  &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '/') ){` |
|      7237 |  188 | `				pStream->zText++;` |
|         - |  189 | `				/* Inline comments */` |
|    310435 |  190 | `				while( pStream->zText < pStream->zEnd && pStream->zText[0] != '\n' ){` |
|    303203 |  191 | `					pStream->zText++;` |
|         5 |  192 | `				}` |
|         - |  193 | `				/* Tell the upper-layer to ignore this token */` |
|      7237 |  194 | `				return SXERR_CONTINUE;` |
| 113867459 |  195 | `		}else if( pStream->zText[0] == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '*' ){` |
|         - |  196 | `			/* A doc-comment starts with slash-star-star followed by more` |
|         - |  197 | `			 * content (slash-star-star-slash is the empty comment, not a` |
|         - |  198 | `			 * docblock). Its full span, delimiters included, goes to the` |
|         - |  199 | `			 * trivia sidecar when the caller supplied one — keyed by the` |
|         - |  200 | `			 * index the NEXT real token receives — and never enters the` |
|         - |  201 | `			 * token stream. */` |
|    208209 |  202 | `			const unsigned char *zDocStart = pStream->zText;` |
|    208221 |  203 | `			int bDoc = ( &pStream->zText[2] < pStream->zEnd && pStream->zText[2] == '*'` |
|    312318 |  204 | `			 && ( &pStream->zText[3] >= pStream->zEnd \|\| pStream->zText[3] != '/' ) );` |
|    208209 |  205 | `			pStream->zText += 2;` |
|         - |  206 | `			/* Block comment */` |
|  16033297 |  207 | `			while( pStream->zText < pStream->zEnd ){` |
|  16033297 |  208 | `				if( pStream->zText[0] == '*' ){` |
|    285493 |  209 | `					if( &pStream->zText[1] >= pStream->zEnd \|\| pStream->zText[1] == '/'  ){` |
|    104107 |  210 | `						break;` |
|         - |  211 | `					}` |
|     38642 |  212 | `				}` |
|  15825093 |  213 | `				if( pStream->zText[0] == '\n' ){` |
|       257 |  214 | `					pStream->nLine++;` |
|       126 |  215 | `				}` |
|  15825093 |  216 | `				pStream->zText++;` |
|         5 |  217 | `			}` |
|    208209 |  218 | `			pStream->zText += 2;` |
|    208209 |  219 | `			if( bDoc && pUserData && pStream->pSet ){` |
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
|    208209 |  232 | `			return SXERR_CONTINUE;` |
| 113659255 |  233 | `		}else if( SyisDigit(pStream->zText[0]) ){` |
|   3703595 |  234 | `			pStream->zText++;` |
|         - |  235 | `			/* PHP 7.4: handle underscore separator immediately following the first digit.` |
|         - |  236 | `			 * Check pStream->zText < pStream->zEnd BEFORE forming pStream->zText + 1 so` |
|         - |  237 | `			 * we never compute a pointer past one-past-end. */` |
|   3703590 |  238 | `			if( pStream->zText < pStream->zEnd` |
|   3703590 |  239 | `				&& pStream->zText[0] == '_'` |
|   1851875 |  240 | `				&& pStream->zText + 1 < pStream->zEnd` |
|       160 |  241 | `				&& pStream->zText[1] < 0xc0` |
|       165 |  242 | `				&& SyisDigit(pStream->zText[1]) ){` |
|       151 |  243 | `				pStream->zText++; /* swallow underscore between two digits */` |
|        75 |  244 | `			}` |
|         - |  245 | `			/* Decimal digit stream (PHP 7.4: underscore separator allowed between two digits) */` |
|   4668953 |  246 | `			while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|    965363 |  247 | `				pStream->zText++;` |
|    965358 |  248 | `				if( pStream->zText < pStream->zEnd` |
|    965358 |  249 | `					&& pStream->zText[0] == '_'` |
|    482765 |  250 | `					&& pStream->zText + 1 < pStream->zEnd` |
|       172 |  251 | `					&& pStream->zText[1] < 0xc0` |
|       177 |  252 | `					&& SyisDigit(pStream->zText[1]) ){` |
|       173 |  253 | `					pStream->zText++; /* swallow underscore between two digits */` |
|        86 |  254 | `				}` |
|         5 |  255 | `			}` |
|         - |  256 | `			/* Mark the token as integer until we encounter a real number */` |
|   3703595 |  257 | `			pToken->nType = PH7_TK_INTEGER;` |
|   3703595 |  258 | `			if( pStream->zText < pStream->zEnd ){` |
|   3703595 |  259 | `				c = pStream->zText[0];` |
|   3703595 |  260 | `				if( c == '.' ){` |
|         - |  261 | `					/* Real number (PHP 7.4: underscore separator allowed between two digits) */` |
|      8571 |  262 | `					pStream->zText++;` |
|     18593 |  263 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|     10027 |  264 | `						pStream->zText++;` |
|     10022 |  265 | `						if( pStream->zText < pStream->zEnd` |
|     10022 |  266 | `							&& pStream->zText[0] == '_'` |
|      5017 |  267 | `							&& pStream->zText + 1 < pStream->zEnd` |
|        12 |  268 | `							&& pStream->zText[1] < 0xc0` |
|        17 |  269 | `							&& SyisDigit(pStream->zText[1]) ){` |
|        13 |  270 | `							pStream->zText++;` |
|         6 |  271 | `						}` |
|         5 |  272 | `					}` |
|      8571 |  273 | `					if( pStream->zText < pStream->zEnd ){` |
|      8571 |  274 | `						c = pStream->zText[0];` |
|      8571 |  275 | `						if( c=='e' \|\| c=='E' ){` |
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
|      4283 |  295 | `					}` |
|      8571 |  296 | `					pToken->nType = PH7_TK_REAL;` |
|   3699312 |  297 | `				}else if( c=='e' \|\| c=='E' ){` |
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
|   3694975 |  319 | `				}else if( c == 'x' \|\| c == 'X' ){` |
|         - |  320 | `					/* Hex digit stream (PHP 7.4: underscore separator allowed between two digits) */` |
|    104049 |  321 | `					pStream->zText++;` |
|    451031 |  322 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisHex(pStream->zText[0]) ){` |
|    346987 |  323 | `						pStream->zText++;` |
|    346982 |  324 | `						if( pStream->zText < pStream->zEnd` |
|    346982 |  325 | `							&& pStream->zText[0] == '_'` |
|    173515 |  326 | `							&& pStream->zText + 1 < pStream->zEnd` |
|        48 |  327 | `							&& pStream->zText[1] < 0xc0` |
|        53 |  328 | `							&& SyisHex(pStream->zText[1]) ){` |
|        49 |  329 | `							pStream->zText++;` |
|        24 |  330 | `						}` |
|         5 |  331 | `					}` |
|   3642899 |  332 | `				}else if(c  == 'b' \|\| c == 'B' ){` |
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
|   3590732 |  344 | `				}else if( c == 'o' \|\| c == 'O' ){` |
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
|   1851795 |  357 | `			}` |
|         - |  358 | `			/* PHP 7.4: absorb a trailing malformed underscore run into the` |
|         - |  359 | `			 * numeric token so the compile phase can emit a PHP-compatible` |
|         - |  360 | `			 * "syntax error, unexpected identifier" parse error. Valid` |
|         - |  361 | `			 * separators were already consumed by the per-loop peek logic` |
|         - |  362 | `			 * above, so an underscore here is always misplaced. */` |
|   3703595 |  363 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '_' ){` |
|        18 |  364 | `				pStream->zText++;` |
|        44 |  365 | `				while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0` |
|        49 |  366 | `					&& (SyisAlphaNum(pStream->zText[0]) \|\| pStream->zText[0] == '_') ){` |
|        20 |  367 | `					pStream->zText++;` |
|         4 |  368 | `				}` |
|         7 |  369 | `			}` |
|         - |  370 | `			/* Record token length */` |
|   3703595 |  371 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|   3703595 |  372 | `			return SXRET_OK;` |
|         - |  373 | `		}` |
| 109955665 |  374 | `		c = pStream->zText[0];` |
| 109955665 |  375 | `		pStream->zText++; /* Advance the stream cursor */` |
|         - |  376 | `		/* Assume we are dealing with an operator*/` |
| 109955665 |  377 | `		pToken->nType = PH7_TK_OP;` |
| 109955665 |  378 | `		switch(c){` |
|  22651995 |  379 | `		case '$': pToken->nType = PH7_TK_DOLLAR; break;` |
|   6754029 |  380 | `		case '{': pToken->nType = PH7_TK_OCB;    break;` |
|   6754015 |  381 | `		case '}': pToken->nType = PH7_TK_CCB;    break;` |
|  14158625 |  382 | `		case '(': pToken->nType = PH7_TK_LPAREN; break;` |
|   3128267 |  383 | `		case '[': pToken->nType \|= PH7_TK_OSB;   break; /* Bitwise operation here,since the square bracket token '['` |
|         - |  384 | `														 * is a potential operator [i.e: subscripting] */` |
|   3128273 |  385 | `		case ']': pToken->nType = PH7_TK_CSB;    break;` |
|   7079298 |  386 | `		case ')': {` |
|  14158601 |  387 | `			SySet *pTokSet = pStream->pSet;` |
|         - |  388 | `			/* Assemble type cast operators [i.e: (int),(float),(bool)...] */` |
|  14158601 |  389 | `			if( pTokSet->nUsed >= 2 ){` |
|         - |  390 | `				SyToken *pTmp;` |
|         - |  391 | `				/* Peek the last recongnized token */` |
|  14158599 |  392 | `				pTmp = (SyToken *)SySetPeek(pTokSet);` |
|  14158599 |  393 | `				if( pTmp->nType & PH7_TK_KEYWORD ){` |
|   1028753 |  394 | `					sxi32 nID = SX_PTR_TO_INT(pTmp->pUserData);` |
|   1028753 |  395 | `					if( (sxu32)nID & (PH7_TKWRD_ARRAY\|PH7_TKWRD_INT\|PH7_TKWRD_FLOAT\|PH7_TKWRD_STRING\|PH7_TKWRD_OBJECT\|PH7_TKWRD_BOOL\|PH7_TKWRD_UNSET) ){` |
|    916807 |  396 | `						pTmp = (SyToken *)SySetAt(pTokSet,pTokSet->nUsed - 2);` |
|    916807 |  397 | `						if( pTmp->nType & PH7_TK_LPAREN ){` |
|         - |  398 | `							/* Merge the three tokens '(' 'TYPE' ')' into a single one */` |
|    597171 |  399 | `							const char * zTypeCast = "(int)";` |
|    597171 |  400 | `							if( nID & PH7_TKWRD_FLOAT ){` |
|     30827 |  401 | `								zTypeCast = "(float)";` |
|    581760 |  402 | `							}else if( nID & PH7_TKWRD_BOOL ){` |
|      3891 |  403 | `								zTypeCast = "(bool)";` |
|    564406 |  404 | `							}else if( nID & PH7_TKWRD_STRING ){` |
|    246483 |  405 | `								zTypeCast = "(string)";` |
|    439224 |  406 | `							}else if( nID & PH7_TKWRD_ARRAY ){` |
|        29 |  407 | `								zTypeCast = "(array)";` |
|    315971 |  408 | `							}else if( nID & PH7_TKWRD_OBJECT ){` |
|        35 |  409 | `								zTypeCast = "(object)";` |
|    315940 |  410 | `							}else if( nID & PH7_TKWRD_UNSET ){` |
|         3 |  411 | `								zTypeCast = "(unset)";` |
|         1 |  412 | `							}` |
|         - |  413 | `							/* Reflect the change */` |
|    597171 |  414 | `							pToken->nType = PH7_TK_OP;` |
|    597171 |  415 | `							SyStringInitFromBuf(&pToken->sData,zTypeCast,SyStrlen(zTypeCast));` |
|         - |  416 | `							/* Save the instance associated with the type cast operator */` |
|    597171 |  417 | `							pToken->pUserData = (void *)PH7_ExprExtractOperator(&pToken->sData,0);` |
|         - |  418 | `							/* Remove the two previous tokens */` |
|    597171 |  419 | `							pTokSet->nUsed -= 2;` |
|    597171 |  420 | `							return SXRET_OK;` |
|         - |  421 | `						}` |
|    159818 |  422 | `					}` |
|    215791 |  423 | `				}` |
|   6780714 |  424 | `			}` |
|  13561435 |  425 | `			pToken->nType = PH7_TK_RPAREN;` |
|  13561435 |  426 | `			break;` |
|         - |  427 | `				  }` |
|   2607333 |  428 | `		case '\'':{` |
|         - |  429 | `			/* Single quoted string */` |
|   5214671 |  430 | `			pStr->zString++;` |
|  59711031 |  431 | `			while( pStream->zText < pStream->zEnd ){` |
|  59711031 |  432 | `				if( pStream->zText[0] == '\''  ){` |
|   5214685 |  433 | `					if( pStream->zText[-1] != '\\' ){` |
|   5179985 |  434 | `						break;` |
|       ! 0 |  435 | `					}else{` |
|     34705 |  436 | `						const unsigned char *zPtr = &pStream->zText[-2];` |
|     34705 |  437 | `						sxi32 i = 1;` |
|     69411 |  438 | `						while( zPtr > pStream->zInput && zPtr[0] == '\\' ){` |
|     34711 |  439 | `							zPtr--;` |
|     34711 |  440 | `							i++;` |
|         5 |  441 | `						}` |
|     34705 |  442 | `						if((i&1)==0){` |
|     34691 |  443 | `							break;` |
|         - |  444 | `						}` |
|         - |  445 | `					}` |
|         7 |  446 | `				}` |
|  54496365 |  447 | `				if( pStream->zText[0] == '\n' ){` |
|        67 |  448 | `					pStream->nLine++;` |
|        33 |  449 | `				}` |
|  54496365 |  450 | `				pStream->zText++;` |
|         5 |  451 | `			}` |
|         - |  452 | `			/* Record token length and type */` |
|   5214671 |  453 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|   5214671 |  454 | `			pToken->nType = PH7_TK_SSTR;` |
|         - |  455 | `			/* Jump the trailing single quote */` |
|   5214671 |  456 | `			pStream->zText++;` |
|   5214671 |  457 | `			return SXRET_OK;` |
|         - |  458 | `				  }` |
|     60689 |  459 | `		case '"':{` |
|         - |  460 | `			sxi32 iNest;` |
|         - |  461 | `			/* Double quoted string */` |
|    121383 |  462 | `			pStr->zString++;` |
|   1723461 |  463 | `			while( pStream->zText < pStream->zEnd ){` |
|   1723461 |  464 | `				if( pStream->zText[0] == '{' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '$'){` |
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
|   1723461 |  486 | `				if( pStream->zText[0] == '"' ){` |
|    121661 |  487 | `					if( pStream->zText[-1] != '\\' ){` |
|    121375 |  488 | `						break;` |
|       ! 0 |  489 | `					}else{` |
|       291 |  490 | `						const unsigned char *zPtr = &pStream->zText[-2];` |
|       291 |  491 | `						sxi32 i = 1;` |
|       331 |  492 | `						while( zPtr > pStream->zInput && zPtr[0] == '\\' ){` |
|        42 |  493 | `							zPtr--;` |
|        42 |  494 | `							i++;` |
|         2 |  495 | `						}` |
|       291 |  496 | `						if((i&1)==0){` |
|         9 |  497 | `							break;` |
|         - |  498 | `						}` |
|         - |  499 | `					}` |
|       139 |  500 | `				}` |
|   1602083 |  501 | `				if( pStream->zText[0] == '\n' ){` |
|        29 |  502 | `					pStream->nLine++;` |
|        14 |  503 | `				}` |
|   1602083 |  504 | `				pStream->zText++;` |
|         5 |  505 | `			}` |
|         - |  506 | `			/* Record token length and type */` |
|    121383 |  507 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|    121383 |  508 | `			pToken->nType = PH7_TK_DSTR;` |
|         - |  509 | `			/* Jump the trailing quote */` |
|    121383 |  510 | `			pStream->zText++;` |
|    121383 |  511 | `			return SXRET_OK;` |
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
|      8669 |  532 | `		case '\\': pToken->nType = PH7_TK_NSSEP;  break;` |
|    468740 |  533 | `		case ':':` |
|    937485 |  534 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == ':' ){` |
|         - |  535 | `				/* Current operator: '::' */` |
|    401411 |  536 | `				pStream->zText++;` |
|    200708 |  537 | `			}else{` |
|    536079 |  538 | `				pToken->nType = PH7_TK_COLON; /* Single colon */` |
|         - |  539 | `			}` |
|    937485 |  540 | `			break;` |
|   3778823 |  541 | `		case ',': pToken->nType \|= PH7_TK_COMMA;  break; /* Comma is also an operator */` |
|  10706219 |  542 | `		case ';': pToken->nType = PH7_TK_SEMI;    break;` |
|         - |  543 | `			/* Handle combined operators [i.e: +=,===,!=== ...] */` |
|   3826522 |  544 | `		case '=':` |
|   7653049 |  545 | `			pToken->nType \|= PH7_TK_EQUAL;` |
|   7653049 |  546 | `			if( pStream->zText < pStream->zEnd ){` |
|   7653049 |  547 | `				if( pStream->zText[0] == '=' ){` |
|   1310805 |  548 | `					pToken->nType &= ~PH7_TK_EQUAL;` |
|         - |  549 | `					/* Current operator: == */` |
|   1310805 |  550 | `					pStream->zText++;` |
|   1310805 |  551 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  552 | `						/* Current operator: === */` |
|   1260391 |  553 | `						pStream->zText++;` |
|    630198 |  554 | `					}` |
|   6997649 |  555 | `				}else if( pStream->zText[0] == '>' ){` |
|         - |  556 | `					/* Array operator: => */` |
|    522953 |  557 | `					pToken->nType = PH7_TK_ARRAY_OP;` |
|    522953 |  558 | `					pStream->zText++;` |
|    261479 |  559 | `				}else{` |
|         - |  560 | `					/* TICKET 1433-0010: Reference operator '=&' */` |
|   5819301 |  561 | `					const unsigned char *zCur = pStream->zText;` |
|   5819301 |  562 | `					sxu32 nLine = 0;` |
|  11638433 |  563 | `					while( zCur < pStream->zEnd && zCur[0] < 0xc0 && SyisSpace(zCur[0]) ){` |
|   5819137 |  564 | `						if( zCur[0] == '\n' ){` |
|         5 |  565 | `							nLine++;` |
|         2 |  566 | `						}` |
|   5819137 |  567 | `						zCur++;` |
|         5 |  568 | `					}` |
|   5819301 |  569 | `					if( zCur < pStream->zEnd && zCur[0] == '&' ){` |
|         - |  570 | `						/* Current operator: =& */` |
|        66 |  571 | `						pToken->nType &= ~PH7_TK_EQUAL;` |
|        66 |  572 | `						SyStringInitFromBuf(pStr,"=&",sizeof("=&")-1);` |
|         - |  573 | `						/* Update token stream */` |
|        66 |  574 | `						pStream->zText = &zCur[1];` |
|        66 |  575 | `						pStream->nLine += nLine;` |
|        31 |  576 | `					}` |
|         - |  577 | `				}` |
|   3826522 |  578 | `			}` |
|   7653049 |  579 | `			break;` |
|    433547 |  580 | `		case '!':` |
|    867099 |  581 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  582 | `				/* Current operator: != */` |
|    423731 |  583 | `				pStream->zText++;` |
|    423731 |  584 | `				if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  585 | `					/* Current operator: !== */` |
|    408297 |  586 | `					pStream->zText++;` |
|    204146 |  587 | `				}` |
|    211863 |  588 | `			}` |
|    867099 |  589 | `			break;` |
|    333414 |  590 | `		case '&':` |
|    666833 |  591 | `			pToken->nType \|= PH7_TK_AMPER;` |
|    666833 |  592 | `			if( pStream->zText < pStream->zEnd ){` |
|    666833 |  593 | `				if( pStream->zText[0] == '&' ){` |
|    439415 |  594 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|         - |  595 | `					/* Current operator: && */` |
|    439415 |  596 | `					pStream->zText++;` |
|    447128 |  597 | `				}else if( pStream->zText[0] == '=' ){` |
|         7 |  598 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|         - |  599 | `					/* Current operator: &= */` |
|         7 |  600 | `					pStream->zText++;` |
|         3 |  601 | `				}` |
|    333414 |  602 | `			}` |
|    666833 |  603 | `			break;` |
|    188856 |  604 | `		case '\|':` |
|    377717 |  605 | `			if( pStream->zText < pStream->zEnd ){` |
|    377717 |  606 | `				if( pStream->zText[0] == '\|' ){` |
|         - |  607 | `					/* Current operator: \|\| */` |
|    304309 |  608 | `					pStream->zText++;` |
|    225565 |  609 | `				}else if( pStream->zText[0] == '=' ){` |
|         - |  610 | `					/* Current operator: \|= */` |
|     57761 |  611 | `					pStream->zText++;` |
|     44535 |  612 | `				}else if( pStream->zText[0] == '>' ){` |
|         - |  613 | `					/* Current operator: \|> (PHP 8.5 pipe) */` |
|        27 |  614 | `					pStream->zText++;` |
|        13 |  615 | `				}` |
|    188856 |  616 | `			}` |
|    377717 |  617 | `			break;` |
|    218142 |  618 | `		case '+':` |
|    436289 |  619 | `			if( pStream->zText < pStream->zEnd ){` |
|    436287 |  620 | `				if( pStream->zText[0] == '+' ){` |
|         - |  621 | `					/* Current operator: ++ */` |
|    177567 |  622 | `					pStream->zText++;` |
|    347506 |  623 | `				}else if( pStream->zText[0] == '=' ){` |
|         - |  624 | `					/* Current operator: += */` |
|     46279 |  625 | `					pStream->zText++;` |
|     23137 |  626 | `				}` |
|    218141 |  627 | `			}` |
|    436289 |  628 | `			break;` |
|   2592776 |  629 | `		case '-':` |
|   5185557 |  630 | `			if( pStream->zText < pStream->zEnd ){` |
|   5185557 |  631 | `				if( pStream->zText[0] == '-' ){` |
|         - |  632 | `					/* Current operator: -- */` |
|     30847 |  633 | `					pStream->zText++;` |
|   5170136 |  634 | `				}else if( pStream->zText[0] == '=' ){` |
|         - |  635 | `					/* Current operator: -= */` |
|        14 |  636 | `					pStream->zText++;` |
|   5154709 |  637 | `				}else if( pStream->zText[0] == '>' ){` |
|         - |  638 | `					/* Current operator: -> */` |
|   4922819 |  639 | `					pStream->zText++;` |
|   2461407 |  640 | `				}` |
|   2592776 |  641 | `			}` |
|   5185557 |  642 | `			break;` |
|     19461 |  643 | `		case '*':` |
|     38927 |  644 | `			if( pStream->zText < pStream->zEnd ){` |
|     38927 |  645 | `				if( pStream->zText[0] == '*' ){` |
|         - |  646 | `					/* Current operator: ** or **= */` |
|       137 |  647 | `					pStream->zText++;` |
|       137 |  648 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  649 | `						/* Current operator: **= */` |
|        23 |  650 | `						pStream->zText++;` |
|        12 |  651 | `					}` |
|     38859 |  652 | `				}else if( pStream->zText[0] == '=' ){` |
|         - |  653 | `					/* Current operator: *= */` |
|        27 |  654 | `					pStream->zText++;` |
|        12 |  655 | `				}` |
|     19461 |  656 | `			}` |
|     38927 |  657 | `			break;` |
|      1976 |  658 | `		case '/':` |
|      3957 |  659 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  660 | `				/* Current operator: /= */` |
|         9 |  661 | `				pStream->zText++;` |
|         4 |  662 | `			}` |
|      3957 |  663 | `			break;` |
|     17374 |  664 | `		case '%':` |
|     34753 |  665 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  666 | `				/* Current operator: %= */` |
|         9 |  667 | `				pStream->zText++;` |
|         4 |  668 | `			}` |
|     34753 |  669 | `			break;` |
|        11 |  670 | `		case '^':` |
|        23 |  671 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  672 | `				/* Current operator: ^= */` |
|         9 |  673 | `				pStream->zText++;` |
|         4 |  674 | `			}` |
|        23 |  675 | `			break;` |
|    982459 |  676 | `		case '.':` |
|   1964923 |  677 | `			if( pStream->zText + 1 < pStream->zEnd && pStream->zText[0] == '.' && pStream->zText[1] == '.' ){` |
|         - |  678 | `				/* Ellipsis: ... */` |
|     27407 |  679 | `				pStream->zText += 2;` |
|     27407 |  680 | `				pToken->nType = PH7_TK_ELLIPSIS;` |
|   1951222 |  681 | `			}else if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  682 | `				/* Current operator: .= */` |
|    335113 |  683 | `				pStream->zText++;` |
|    167554 |  684 | `			}` |
|   1964923 |  685 | `			break;` |
|    190914 |  686 | `		case '<':` |
|    381833 |  687 | `			if( pStream->zText < pStream->zEnd ){` |
|    381833 |  688 | `				if( pStream->zText[0] == '<' ){` |
|         - |  689 | `					/* Current operator: << */` |
|       144 |  690 | `					pStream->zText++;` |
|       144 |  691 | `					if( pStream->zText < pStream->zEnd ){` |
|       144 |  692 | `						if( pStream->zText[0] == '=' ){` |
|         - |  693 | `							/* Current operator: <<= */` |
|         9 |  694 | `							pStream->zText++;` |
|       140 |  695 | `						}else if( pStream->zText[0] == '<' ){` |
|         - |  696 | `							/* Current Token: <<<  */` |
|       122 |  697 | `							pStream->zText++;` |
|         - |  698 | `							/* This may be the beginning of a Heredoc/Nowdoc string,try to delimit it */` |
|       122 |  699 | `							rc = LexExtractHeredoc(&(*pStream),&(*pToken));` |
|       122 |  700 | `							if( rc == SXRET_OK ){` |
|         - |  701 | `								/* Here/Now doc successfuly extracted */` |
|       122 |  702 | `								return SXRET_OK;` |
|         - |  703 | `							}` |
|       ! 0 |  704 | `						}` |
|        12 |  705 | `					}` |
|    381704 |  706 | `				}else if( pStream->zText[0] == '>' ){` |
|         - |  707 | `					/* Current operator: <> */` |
|         5 |  708 | `					pStream->zText++;` |
|    381691 |  709 | `				}else if( pStream->zText[0] == '=' ){` |
|         - |  710 | `					/* Current operator: <= or <=> */` |
|     65589 |  711 | `					pStream->zText++;` |
|     65589 |  712 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '>' ){` |
|         - |  713 | `						/* Current operator: <=> */` |
|     27025 |  714 | `						pStream->zText++;` |
|     13510 |  715 | `					}` |
|     32792 |  716 | `				}` |
|    190855 |  717 | `			}` |
|    381715 |  718 | `			break;` |
|    152232 |  719 | `		case '>':` |
|    304469 |  720 | `			if( pStream->zText < pStream->zEnd ){` |
|    304469 |  721 | `				if( pStream->zText[0] == '>' ){` |
|         - |  722 | `					/* Current operator: >> */` |
|     19275 |  723 | `					pStream->zText++;` |
|     19275 |  724 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  725 | `						/* Current operator: >>= */` |
|        11 |  726 | `						pStream->zText++;` |
|        10 |  727 | `					}` |
|    294834 |  728 | `				}else if( pStream->zText[0] == '=' ){` |
|         - |  729 | `					/* Current operator: >= */` |
|    104057 |  730 | `					pStream->zText++;` |
|     52026 |  731 | `				}` |
|    152232 |  732 | `			}` |
|    304469 |  733 | `			break;` |
|    261810 |  734 | `		case '?':` |
|    523625 |  735 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '?' ){` |
|         - |  736 | `				/* Null coalescing operator: ?? */` |
|     58015 |  737 | `				pStream->zText++;` |
|     58015 |  738 | `				if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  739 | `					/* Null coalescing assignment operator (PHP 7.4) */` |
|       101 |  740 | `					pStream->zText++;` |
|        48 |  741 | `				}` |
|    494620 |  742 | `			}else if( (pStream->zEnd - pStream->zText) >= 2` |
|    465615 |  743 | `				&& pStream->zText[0] == '-' && pStream->zText[1] == '>' ){` |
|         - |  744 | `				/* Nullsafe object operator (PHP 8.0): ?-> */` |
|       119 |  745 | `				pStream->zText += 2;` |
|        57 |  746 | `			}` |
|    523620 |  747 | `			break;` |
|      7839 |  748 | `		default:` |
|     15678 |  749 | `			break;` |
|         - |  750 | `		}` |
| 104022333 |  751 | `		if( pStr->nByte <= 0 ){` |
|         - |  752 | `			/* Record token length */` |
| 104022271 |  753 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  52011133 |  754 | `		}` |
| 104022333 |  755 | `		if( pToken->nType & PH7_TK_OP ){` |
|         - |  756 | `			const ph7_expr_op *pOp;` |
|         - |  757 | `			/* Check if the extracted token is an operator */` |
|  25212689 |  758 | `			pOp = PH7_ExprExtractOperator(pStr,(SyToken *)SySetPeek(pStream->pSet));` |
|  25212689 |  759 | `			if( pOp == 0 ){` |
|         - |  760 | `				/* Not an operator */` |
|       ! 0 |  761 | `				pToken->nType &= ~PH7_TK_OP;` |
|       ! 0 |  762 | `				if( pToken->nType <= 0 ){` |
|       ! 0 |  763 | `					pToken->nType = PH7_TK_OTHER;` |
|       ! 0 |  764 | `				}` |
|       ! 0 |  765 | `			}else{` |
|         - |  766 | `				/* Save the instance associated with this operator for later processing */` |
|  25212689 |  767 | `				pToken->pUserData = (void *)pOp;` |
|         - |  768 | `			}` |
|  12606342 |  769 | `		}` |
|         - |  770 | `	}` |
|         - |  771 | `	/* Tell the upper-layer to save the extracted token for later processing */` |
| 160871801 |  772 | `	return SXRET_OK;` |
|  85366008 |  773 | `}` |
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
|  56849473 |  793 | `static sxu32 KeywordCode(const char *z, int n){` |
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
|  56849473 |  883 | `  if( n<2 ) return PH7_TK_ID;` |
|  49687723 |  884 | `  h = (((int)z[0]*4) ^ ((int)z[n-1]*3) ^ n) % 151;` |
|  76724941 |  885 | `  for(i=((int)aHash[h])-1; i>=0; i=((int)aNext[i])-1){` |
|  45965481 |  886 | `    if( (int)aLen[i]==n && SyMemcmp(&zText[aOffset[i]],z,n)==0 ){` |
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
|  18928263 |  971 | `      return aCode[i];` |
|         - |  972 | `    }` |
|  13518612 |  973 | `  }` |
|         - |  974 | `  /* Linear fallback for keywords not in the auto-generated hash table */` |
|  30759465 |  975 | `  if( n==5 && SyMemcmp(z,"trait",5)==0 ) return PH7_TKWRD_TRAIT;` |
|  30751675 |  976 | `  if( n==9 && SyMemcmp(z,"insteadof",9)==0 ) return PH7_TKWRD_INSTEADOF;` |
|  30751671 |  977 | `  if( n==7 && SyMemcmp(z,"finally",7)==0 ) return PH7_TKWRD_FINALLY;` |
|  30751499 |  978 | `  if( n==5 && SyMemcmp(z,"yield",5)==0 ) return PH7_TKWRD_YIELD;` |
|  30735709 |  979 | `  if( n==5 && SyMemcmp(z,"match",5)==0 ) return PH7_TKWRD_MATCH;` |
|  30735631 |  980 | `  return PH7_TK_ID;` |
|  28424739 |  981 | `}` |
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
|       118 | 1016 | `static sxi32 LexExtractHeredoc(SyStream *pStream,SyToken *pToken)` |
|         4 | 1017 | `{` |
|       122 | 1018 | `	const unsigned char *zIn  = pStream->zText;` |
|       122 | 1019 | `	const unsigned char *zEnd = pStream->zEnd;` |
|         - | 1020 | `	const unsigned char *zPtr;` |
|       122 | 1021 | `	sxu8 bNowDoc = FALSE;` |
|         - | 1022 | `	SyString sDelim;` |
|         - | 1023 | `	SyString sStr;` |
|         - | 1024 | `	/* Jump leading white spaces */` |
|       134 | 1025 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|        13 | 1026 | `		zIn++;` |
|         1 | 1027 | `	}` |
|       122 | 1028 | `	if( zIn >= zEnd ){` |
|         - | 1029 | `		/* A simple symbol,return immediately */` |
|       ! 0 | 1030 | `		return SXERR_CONTINUE;` |
|         - | 1031 | `	}` |
|       122 | 1032 | `	if( zIn[0] == '\'' \|\| zIn[0] == '"' ){` |
|         - | 1033 | `		/* Make sure we are dealing with a nowdoc */` |
|        52 | 1034 | `		bNowDoc =  zIn[0] == '\'' ? TRUE : FALSE;` |
|        52 | 1035 | `		zIn++;` |
|        24 | 1036 | `	}` |
|       122 | 1037 | `	if( zIn[0] < 0xc0 && !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|         - | 1038 | `		/* Invalid delimiter,return immediately */` |
|       ! 0 | 1039 | `		return SXERR_CONTINUE;` |
|         - | 1040 | `	}` |
|         - | 1041 | `	/* Isolate the identifier */` |
|       122 | 1042 | `	sDelim.zString = (const char *)zIn;` |
|       126 | 1043 | `	for(;;){` |
|       256 | 1044 | `		zPtr = zIn;` |
|         - | 1045 | `		/* Skip alphanumeric stream */` |
|       806 | 1046 | `		while( zPtr < zEnd && zPtr[0] < 0xc0 && (SyisAlphaNum(zPtr[0]) \|\| zPtr[0] == '_') ){` |
|       428 | 1047 | `			zPtr++;` |
|         4 | 1048 | `		}` |
|       256 | 1049 | `		if( zPtr < zEnd && zPtr[0] >= 0xc0 ){` |
|        19 | 1050 | `			zPtr++;` |
|         - | 1051 | `			/* UTF-8 stream */` |
|        37 | 1052 | `			while( zPtr < zEnd && ((zPtr[0] & 0xc0) == 0x80) ){` |
|        19 | 1053 | `				zPtr++;` |
|         1 | 1054 | `			}` |
|         9 | 1055 | `		}` |
|       256 | 1056 | `		if( zPtr == zIn ){` |
|         - | 1057 | `			/* Not an UTF-8 or alphanumeric stream */` |
|       122 | 1058 | `			break;` |
|         - | 1059 | `		}` |
|         - | 1060 | `		/* Synchronize pointers */` |
|       138 | 1061 | `		zIn = zPtr;` |
|         4 | 1062 | `	}` |
|         - | 1063 | `	/* Get the identifier length */` |
|       122 | 1064 | `	sDelim.nByte = (sxu32)((const char *)zIn-sDelim.zString);` |
|       122 | 1065 | `	if( zIn[0] == '"' \|\| (bNowDoc && zIn[0] == '\'') ){` |
|         - | 1066 | `		/* Jump the trailing single quote */` |
|        52 | 1067 | `		zIn++;` |
|        24 | 1068 | `	}` |
|         - | 1069 | `	/* Jump trailing white spaces */` |
|       122 | 1070 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|       ! 0 | 1071 | `		zIn++;` |
|       ! 0 | 1072 | `	}` |
|       122 | 1073 | `	if( sDelim.nByte <= 0 \|\| zIn >= zEnd \|\| zIn[0] != '\n' ){` |
|         - | 1074 | `		/* Invalid syntax */` |
|       ! 0 | 1075 | `		return SXERR_CONTINUE;` |
|         - | 1076 | `	}` |
|       122 | 1077 | `	pStream->nLine++; /* Increment line counter */` |
|       122 | 1078 | `	zIn++;` |
|         - | 1079 | `	/* Isolate the delimited string */` |
|       122 | 1080 | `	sStr.zString = (const char *)zIn;` |
|         - | 1081 | `	/* PHP 7.3 flexible heredoc/nowdoc: the closing marker may be preceded` |
|         - | 1082 | `	 * by whitespace (spaces/tabs), and may be followed by any non-identifier` |
|         - | 1083 | `	 * character. The indent count is recorded in pToken->pUserData and the` |
|         - | 1084 | `	 * compile phase strips it from each body line. */` |
|         - | 1085 | `	{` |
|       122 | 1086 | `		const unsigned char *zMarkerLine = zIn; /* Start of marker's line (set on match) */` |
|       122 | 1087 | `		sxu32 nIndent = 0;` |
|       265 | 1088 | `		for(;;){` |
|       328 | 1089 | `			const unsigned char *zLineStart = zIn;` |
|         - | 1090 | `			/* Skip leading space/tab on this line */` |
|       880 | 1091 | `			while( zIn < zEnd && (zIn[0] == ' ' \|\| zIn[0] == '\t') ){` |
|       393 | 1092 | `				zIn++;` |
|         3 | 1093 | `			}` |
|       324 | 1094 | `			if( (sxu32)(zEnd - zIn) >= sDelim.nByte` |
|       327 | 1095 | `				&& SyMemcmp((const void *)sDelim.zString,(const void *)zIn,sDelim.nByte) == 0 ){` |
|         - | 1096 | `				int bIdentCont;` |
|       120 | 1097 | `				zPtr = &zIn[sDelim.nByte];` |
|         - | 1098 | `				/* Disambiguate: next byte must not continue an identifier.` |
|         - | 1099 | `				 * A leading byte >= 0xc0 starts a multi-byte UTF-8 sequence,` |
|         - | 1100 | `				 * which PHP identifiers may contain, so treat it as ident. */` |
|       120 | 1101 | `				if( zPtr >= zEnd ){` |
|       ! 0 | 1102 | `					bIdentCont = 0;` |
|       120 | 1103 | `				}else if( zPtr[0] >= 0xc0 ){` |
|       ! 0 | 1104 | `					bIdentCont = 1;` |
|       ! 0 | 1105 | `				}else{` |
|       120 | 1106 | `					bIdentCont = (SyisAlphaNum(zPtr[0]) \|\| zPtr[0] == '_');` |
|         - | 1107 | `				}` |
|       120 | 1108 | `				if( !bIdentCont ){` |
|         - | 1109 | `					/* Closing marker found */` |
|       120 | 1110 | `					nIndent = (sxu32)(zIn - zLineStart);` |
|       120 | 1111 | `					zMarkerLine = zLineStart;` |
|       120 | 1112 | `					pStream->zText = zPtr; /* Cursor right after identifier */` |
|       120 | 1113 | `					break;` |
|         - | 1114 | `				}` |
|       ! 0 | 1115 | `			}` |
|         - | 1116 | `			/* Not the closing marker on this line; walk to next newline */` |
|      4480 | 1117 | `			while( zIn < zEnd && zIn[0] != '\n' ){` |
|      4272 | 1118 | `				zIn++;` |
|         4 | 1119 | `			}` |
|       212 | 1120 | `			if( zIn >= zEnd ){` |
|         - | 1121 | `				/* End of input without finding the closing marker */` |
|         3 | 1122 | `				pStream->zText = pStream->zEnd;` |
|         3 | 1123 | `				zMarkerLine = zIn;` |
|         3 | 1124 | `				break;` |
|         - | 1125 | `			}` |
|       210 | 1126 | `			pStream->nLine++;` |
|       210 | 1127 | `			zIn++;` |
|         4 | 1128 | `		}` |
|         - | 1129 | `		/* Body runs from sStr.zString up to just before the marker line */` |
|       122 | 1130 | `		sStr.nByte = (sxu32)((const char *)zMarkerLine - sStr.zString);` |
|       122 | 1131 | `		pToken->nType = bNowDoc ? PH7_TK_NOWDOC : PH7_TK_HEREDOC;` |
|       122 | 1132 | `		SyStringDupPtr(&pToken->sData,&sStr);` |
|         - | 1133 | `		/* Strip exactly one line terminator that precedes the marker's line. */` |
|       118 | 1134 | `		if( pToken->sData.nByte > 0` |
|       118 | 1135 | `			&& pToken->sData.zString[pToken->sData.nByte - 1] == '\n' ){` |
|       112 | 1136 | `			pToken->sData.nByte--;` |
|       108 | 1137 | `			if( pToken->sData.nByte > 0` |
|       112 | 1138 | `				&& pToken->sData.zString[pToken->sData.nByte - 1] == '\r' ){` |
|       ! 0 | 1139 | `				pToken->sData.nByte--;` |
|       ! 0 | 1140 | `			}` |
|        54 | 1141 | `		}` |
|       122 | 1142 | `		pToken->pUserData = SX_INT_TO_PTR(nIndent);` |
|         - | 1143 | `	}` |
|         - | 1144 | `	/* All done */` |
|       122 | 1145 | `	return SXRET_OK;` |
|        63 | 1146 | `}` |
|         - | 1147 | `/*` |
|         - | 1148 | ` * Tokenize a raw PHP input.` |
|         - | 1149 | ` * This is the public tokenizer called by most code generator routines.` |
|         - | 1150 | ` */` |
|     82588 | 1151 | `PH7_PRIVATE sxi32 PH7_TokenizePHP(const char *zInput,sxu32 nLen,sxu32 nLineStart,SySet *pOut,SySet *pTrivia)` |
|         5 | 1152 | `{` |
|         - | 1153 | `	SyLex sLexer;` |
|         - | 1154 | `	sxi32 rc;` |
|         - | 1155 | `	/* Defense-in-depth cap for internal tokenizer calls that bypass ph7_compile() */` |
|     82593 | 1156 | `	if( nLen > PH7_MAX_INPUT_SIZE ){` |
|       ! 0 | 1157 | `		return SXERR_LIMIT;` |
|         - | 1158 | `	}` |
|         - | 1159 | `	/* Initialize the lexer. pTrivia (may be NULL = discard) rides as the` |
|         - | 1160 | `	 * tokenizer callback's user data: doc-comments (and later attribute` |
|         - | 1161 | `	 * groups) are recorded there instead of entering the token stream. */` |
|     82593 | 1162 | `	rc = SyLexInit(&sLexer,&(*pOut),TokenizePHP,pTrivia);` |
|     82593 | 1163 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 1164 | `		return rc;` |
|         - | 1165 | `	}` |
|     82593 | 1166 | `	sLexer.sStream.nLine = nLineStart;` |
|         - | 1167 | `	/* Tokenize input */` |
|     82593 | 1168 | `	rc = SyLexTokenizeInput(&sLexer,zInput,nLen,0,0,0);` |
|         - | 1169 | `	/* Release the lexer */` |
|     82593 | 1170 | `	SyLexRelease(&sLexer);` |
|         - | 1171 | `	/* Tokenization result */` |
|     82593 | 1172 | `	return rc;` |
|     41299 | 1173 | `}` |
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
|     13490 | 1219 | `PH7_PRIVATE sxi32 PH7_TokenizeRawText(const char *zInput,sxu32 nLen,SySet *pOut,sxu32 nBaseLine)` |
|         5 | 1220 | `{` |
|     13495 | 1221 | `	const char *zEnd = &zInput[nLen];` |
|     13495 | 1222 | `	const char *zIn  = zInput;` |
|         - | 1223 | `	const char *zCur,*zCurEnd;` |
|     13495 | 1224 | `	SyString sCtag = { 0, 0 };     /* Closing tag */` |
|         - | 1225 | `	SyToken sToken;` |
|         - | 1226 | `	SyString sDoc;` |
|         - | 1227 | `	sxu32 nLine;` |
|         - | 1228 | `	sxi32 iNest;` |
|         - | 1229 | `	sxi32 rc;` |
|         - | 1230 | `	/* Tokenize the input into PHP tokens and raw tokens. nBaseLine is normally 1,` |
|         - | 1231 | `	 * but 2 when a "#!" shebang line was stripped so error lines still match php. */` |
|     13495 | 1232 | `	nLine = nBaseLine;` |
|     13495 | 1233 | `	zCur = zCurEnd   = 0; /* Prevent compiler warning */` |
|     13495 | 1234 | `	sToken.pUserData = 0;` |
|     13495 | 1235 | `	iNest = 0;` |
|     13495 | 1236 | `	sDoc.nByte = 0;` |
|     13495 | 1237 | `	sDoc.zString = ""; /* cc warning */` |
|     13489 | 1238 | `	for(;;){` |
|     26983 | 1239 | `		if( zIn >= zEnd ){` |
|         - | 1240 | `			/* End of input reached */` |
|     13483 | 1241 | `			break;` |
|         - | 1242 | `		}` |
|     13505 | 1243 | `		sToken.nLine = nLine;` |
|     13505 | 1244 | `		zCur = zIn;` |
|     13505 | 1245 | `		zCurEnd = 0;` |
|     13745 | 1246 | `		while( zIn < zEnd ){` |
|     13733 | 1247 | `			 if( zIn[0] == '<' ){` |
|     13493 | 1248 | `				const char *zTmp = zIn; /* End of raw input marker */` |
|     13493 | 1249 | `				zIn++;` |
|     13493 | 1250 | `				if( zIn < zEnd ){` |
|     13493 | 1251 | `					if( zIn[0] == '?' ){` |
|     13493 | 1252 | `						zIn++;` |
|     13493 | 1253 | `						if( (sxu32)(zEnd - zIn) >= sizeof("php")-1 &&  SyStrnicmp(zIn,"php",sizeof("php")-1) == 0 ){` |
|         - | 1254 | `							/* opening tag: <?php */` |
|     13491 | 1255 | `							zIn += sizeof("php")-1;` |
|      6743 | 1256 | `						}` |
|         - | 1257 | `						/* Look for the closing tag '?>' */` |
|     13493 | 1258 | `						SyStringInitFromBuf(&sCtag,"?>",sizeof("?>")-1);` |
|     13493 | 1259 | `						zCurEnd = zTmp;` |
|     13493 | 1260 | `						break;` |
|         - | 1261 | `					}` |
|       ! 0 | 1262 | `				}` |
|       ! 0 | 1263 | `			}else{` |
|       243 | 1264 | `				if( zIn[0] == '\n' ){` |
|         8 | 1265 | `					nLine++;` |
|         3 | 1266 | `				}` |
|       243 | 1267 | `				zIn++;` |
|         - | 1268 | `			 }` |
|         3 | 1269 | `		} /* While(zIn < zEnd) */` |
|     13505 | 1270 | `		if( zCurEnd == 0 ){` |
|        14 | 1271 | `			zCurEnd = zIn;` |
|         6 | 1272 | `		}` |
|         - | 1273 | `		/* Save the raw token */` |
|     13505 | 1274 | `		SyStringInitFromBuf(&sToken.sData,zCur,zCurEnd - zCur);` |
|     13505 | 1275 | `		sToken.nType = PH7_TOKEN_RAW;` |
|     13505 | 1276 | `		rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|     13505 | 1277 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1278 | `			return rc;` |
|         - | 1279 | `		}` |
|     13505 | 1280 | `		if( zIn >= zEnd ){` |
|        14 | 1281 | `			break;` |
|         - | 1282 | `		}` |
|         - | 1283 | `		/* Ignore leading white space */` |
|     28717 | 1284 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|     15229 | 1285 | `			if( zIn[0] == '\n' ){` |
|     14763 | 1286 | `				nLine++;` |
|      7379 | 1287 | `			}` |
|     15229 | 1288 | `			zIn++;` |
|         5 | 1289 | `		}` |
|         - | 1290 | `		/* Delimit the PHP chunk */` |
|     13493 | 1291 | `		sToken.nLine = nLine;` |
|     13493 | 1292 | `		zCur = zIn;` |
|   1877815 | 1293 | `		while( (sxu32)(zEnd - zIn) >= sCtag.nByte ){` |
|         - | 1294 | `			const char *zPtr;` |
|   1871583 | 1295 | `			if( SyMemcmp(zIn,sCtag.zString,sCtag.nByte) == 0 && iNest < 1 ){` |
|      7255 | 1296 | `				break;` |
|         - | 1297 | `			}` |
|    935978 | 1298 | `			for(;;){` |
|   1871961 | 1299 | `				if( zIn[0] != '/' \|\| (zIn[1] != '*' && zIn[1] != '/') /* && sCtag.nByte >= 2 */ ){` |
|    932169 | 1300 | `					break;` |
|         - | 1301 | `				}` |
|      7633 | 1302 | `				zIn += 2;` |
|      7633 | 1303 | `				if( zIn[-1] == '/' ){` |
|         - | 1304 | `					/* Inline comment */` |
|    307133 | 1305 | `					while( zIn < zEnd && zIn[0] != '\n' ){` |
|    299811 | 1306 | `						zIn++;` |
|         5 | 1307 | `					}` |
|      7327 | 1308 | `					if( zIn >= zEnd ){` |
|         3 | 1309 | `						zIn--;` |
|         1 | 1310 | `					}` |
|      3666 | 1311 | `				}else{` |
|         - | 1312 | `					/* Block comment */` |
|     33571 | 1313 | `					while( (sxu32)(zEnd-zIn) >= sizeof("*/") - 1 ){` |
|     33569 | 1314 | `						if( zIn[0] == '*' && zIn[1] == '/' ){` |
|       309 | 1315 | `							zIn += 2;` |
|       309 | 1316 | `							break;` |
|         - | 1317 | `						}` |
|     33265 | 1318 | `						if( zIn[0] == '\n' ){` |
|       271 | 1319 | `							nLine++;` |
|       133 | 1320 | `						}` |
|     33265 | 1321 | `						zIn++;` |
|         5 | 1322 | `					}` |
|         - | 1323 | `				}` |
|         5 | 1324 | `			}` |
|   1864333 | 1325 | `			if( zIn[0] == '\n' ){` |
|     62097 | 1326 | `				nLine++;` |
|     62097 | 1327 | `				if( iNest > 0 ){` |
|       328 | 1328 | `					zIn++;` |
|       718 | 1329 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|       393 | 1330 | `						zIn++;` |
|         3 | 1331 | `					}` |
|       328 | 1332 | `					zPtr = zIn;` |
|      1644 | 1333 | `					while( zIn < zEnd ){` |
|      1644 | 1334 | `						if( (unsigned char)zIn[0] >= 0xc0 ){` |
|         - | 1335 | `							/* UTF-8 stream */` |
|        19 | 1336 | `							zIn++;` |
|        37 | 1337 | `							SX_JMP_UTF8(zIn,zEnd);` |
|      1632 | 1338 | `						}else if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       166 | 1339 | `							break;` |
|       ! 0 | 1340 | `						}else{` |
|      1302 | 1341 | `							zIn++;` |
|         - | 1342 | `						}` |
|         4 | 1343 | `					}` |
|       328 | 1344 | `					if( (sxu32)(zIn - zPtr) == sDoc.nByte && SyMemcmp(sDoc.zString,zPtr,sDoc.nByte) == 0 ){` |
|       120 | 1345 | `						iNest = 0;` |
|        58 | 1346 | `					}` |
|       328 | 1347 | `					continue;` |
|         5 | 1348 | `				}` |
|   1833125 | 1349 | `			}else if ( (sxu32)(zEnd - zIn) >= sizeof("<<<") && zIn[0] == '<' && zIn[1] == '<' && zIn[2] == '<' && iNest < 1){` |
|       122 | 1350 | `				zIn += sizeof("<<<")-1;` |
|       134 | 1351 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|        13 | 1352 | `					zIn++;` |
|         1 | 1353 | `				}` |
|       122 | 1354 | `				if( zIn[0] == '"' \|\| zIn[0] == '\'' ){` |
|        52 | 1355 | `					zIn++;` |
|        24 | 1356 | `				}` |
|       122 | 1357 | `				zPtr = zIn;` |
|       564 | 1358 | `				while( zIn < zEnd ){` |
|       564 | 1359 | `					if( (unsigned char)zIn[0] >= 0xc0 ){` |
|         - | 1360 | `						/* UTF-8 stream */` |
|        19 | 1361 | `						zIn++;` |
|        37 | 1362 | `						SX_JMP_UTF8(zIn,zEnd);` |
|       552 | 1363 | `					}else if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|        63 | 1364 | `						break;` |
|       ! 0 | 1365 | `					}else{` |
|       428 | 1366 | `						zIn++;` |
|         - | 1367 | `					}` |
|         4 | 1368 | `				}` |
|       122 | 1369 | `				SyStringInitFromBuf(&sDoc,zPtr,zIn-zPtr);` |
|       122 | 1370 | `				SyStringFullTrim(&sDoc);` |
|       122 | 1371 | `				if( sDoc.nByte > 0 ){` |
|       122 | 1372 | `					iNest++;` |
|        59 | 1373 | `				}` |
|       122 | 1374 | `				continue;` |
|         - | 1375 | `			}` |
|   1863891 | 1376 | `			zIn++;` |
|         - | 1377 |  |
|   1863891 | 1378 | `			if ( zIn >= zEnd )` |
|         7 | 1379 | `				break;` |
|         5 | 1380 | `		}` |
|     13493 | 1381 | `		if( (sxu32)(zEnd - zIn) < sCtag.nByte ){` |
|      6243 | 1382 | `			zIn = zEnd;` |
|      3119 | 1383 | `		}` |
|     13493 | 1384 | `		if( zCur < zIn ){` |
|         - | 1385 | `			/* Save the PHP chunk for later processing */` |
|     10269 | 1386 | `			sToken.nType = PH7_TOKEN_PHP;` |
|     10269 | 1387 | `			SyStringInitFromBuf(&sToken.sData,zCur,zIn-zCur);` |
|     20357 | 1388 | `			SyStringRightTrim(&sToken.sData); /* Trim trailing white spaces */` |
|     10269 | 1389 | `			rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|     10269 | 1390 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 1391 | `				return rc;` |
|         - | 1392 | `			}` |
|      5132 | 1393 | `		}` |
|     13493 | 1394 | `		if( zIn < zEnd ){` |
|         - | 1395 | `			/* Jump the trailing closing tag */` |
|      7255 | 1396 | `			zIn += sCtag.nByte;` |
|         - | 1397 | `			/* php's lexer swallows exactly ONE newline immediately after the` |
|         - | 1398 | `			 * closing tag ("?>\n" emits nothing) */` |
|      7255 | 1399 | `			if( zIn < zEnd && zIn[0] == '\r' && zIn + 1 < zEnd && zIn[1] == '\n' ){` |
|       ! 0 | 1400 | `				zIn += 2;` |
|       ! 0 | 1401 | `				nLine++;` |
|      7255 | 1402 | `			}else if( zIn < zEnd && zIn[0] == '\n' ){` |
|        62 | 1403 | `				zIn++;` |
|        62 | 1404 | `				nLine++;` |
|        29 | 1405 | `			}` |
|      3625 | 1406 | `		}` |
|         5 | 1407 | `	} /* For(;;) */` |
|         - | 1408 |  |
|     13495 | 1409 | ` 	return SXRET_OK;` |
|      6750 | 1410 | `}` |
|         - | 1411 |  |
