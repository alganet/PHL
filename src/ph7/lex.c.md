# src/ph7/lex.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 814/871 lines (93.46%)

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
| 187099824 |   19 | `static sxi32 TokenizePHP(SyStream *pStream,SyToken *pToken,void *pUserData,void *pCtxData)` |
|         5 |   20 | `{` |
|         - |   21 | `	SyString *pStr;` |
|         - |   22 | `	sxi32 rc;` |
|         - |   23 | `	/* Ignore leading white spaces */` |
| 277500329 |   24 | `	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){` |
|         - |   25 | `		/* Advance the stream cursor */` |
|  90400505 |   26 | `		if( pStream->zText[0] == '\n' ){` |
|         - |   27 | `			/* Update line counter */` |
|     54249 |   28 | `			pStream->nLine++;` |
|     27122 |   29 | `		}` |
|  90400505 |   30 | `		pStream->zText++;` |
|         5 |   31 | `	}` |
| 187099829 |   32 | `	if( pStream->zText >= pStream->zEnd ){` |
|         - |   33 | `		/* End of input reached */` |
|         3 |   34 | `		return SXERR_EOF;` |
|         - |   35 | `	}` |
|         - |   36 | `	/* Record token starting position and line */` |
| 187099827 |   37 | `	pToken->nLine = pStream->nLine;` |
| 187099827 |   38 | `	pToken->pUserData = 0;` |
| 187099827 |   39 | `	pStr = &pToken->sData;` |
| 187099827 |   40 | `	SyStringInitFromBuf(pStr,pStream->zText,0);` |
| 218561717 |   41 | `	if( pStream->zText[0] >= 0xc0 \|\| SyisAlpha(pStream->zText[0]) \|\| pStream->zText[0] == '_' ){` |
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
|  62923785 |   52 | `		if( pStream->zText[0] < 0xc0 ){` |
|  62923763 |   53 | `			pStream->zText++;` |
|  31461879 |   54 | `		}` |
|  59184857 |   55 | `		for(;;){` |
| 118369719 |   56 | `			zIn = pStream->zText;` |
| 118369719 |   57 | `			if( zIn[0] >= 0xc0 ){` |
|        81 |   58 | `				zIn++;` |
|         - |   59 | `				/* UTF-8 stream */` |
|       173 |   60 | `				while( zIn < pStream->zEnd && ((zIn[0] & 0xc0) == 0x80) ){` |
|        93 |   61 | `					zIn++;` |
|         1 |   62 | `				}` |
|        40 |   63 | `			}` |
|         - |   64 | `			/* Skip alphanumeric stream */` |
| 475520762 |   65 | `			while( zIn < pStream->zEnd && zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_') ){` |
| 297966191 |   66 | `				zIn++;` |
|         5 |   67 | `			}` |
| 118369719 |   68 | `			if( zIn == pStream->zText ){` |
|         - |   69 | `				/* Not an UTF-8 or alphanumeric stream */` |
|  62923785 |   70 | `				break;` |
|         - |   71 | `			}` |
|         - |   72 | `			/* Synchronize pointers */` |
|  55445939 |   73 | `			pStream->zText = zIn;` |
|         5 |   74 | `		}` |
|         - |   75 | `		/* Record token length */` |
|  62923785 |   76 | `		pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  62923785 |   77 | `		nKeyword = KeywordCode(pStr->zString,(int)pStr->nByte);` |
|         - |   78 | `		/* PHP 7.4: 'fn' is a keyword reserved for arrow functions.` |
|         - |   79 | `		 * The auto-generated perfect hash above doesn't know about it,` |
|         - |   80 | `		 * so intercept the 'fn' identifier here.` |
|         - |   81 | `		 */` |
|  62923780 |   82 | `		if( nKeyword == PH7_TK_ID && pStr->nByte == 2` |
|  21732301 |   83 | `			&& pStr->zString[0] == 'f' && pStr->zString[1] == 'n' ){` |
|       661 |   84 | `			nKeyword = PH7_TKWRD_FN;` |
|       328 |   85 | `		}` |
|  62923785 |   86 | `		if( nKeyword != PH7_TK_ID ){` |
|  20833259 |   87 | `			if( nKeyword &` |
|         - |   88 | `				(PH7_TKWRD_NEW\|PH7_TKWRD_CLONE\|PH7_TKWRD_AND\|PH7_TKWRD_XOR\|PH7_TKWRD_OR\|PH7_TKWRD_INSTANCEOF) ){` |
|         - |   89 | `					/* Alpha stream operators [i.e: new,clone,and,instanceof,or,xor],save the operator instance for later processing */` |
|   1005541 |   90 | `					pToken->pUserData = (void *)PH7_ExprExtractOperator(pStr,0);` |
|         - |   91 | `					/* Mark as an operator */` |
|   1005541 |   92 | `					pToken->nType = PH7_TK_ID\|PH7_TK_OP;` |
|    502773 |   93 | `			}else{` |
|         - |   94 | `				/* We are dealing with a keyword [i.e: while,foreach,class...],save the keyword ID */` |
|  19827723 |   95 | `				pToken->nType = PH7_TK_KEYWORD;` |
|  19827723 |   96 | `				pToken->pUserData = SX_INT_TO_PTR(nKeyword);` |
|         - |   97 | `			}` |
|  10416632 |   98 | `		}else{` |
|         - |   99 | `			/* A simple identifier */` |
|  42090531 |  100 | `			pToken->nType = PH7_TK_ID;` |
|         - |  101 | `		}` |
|  31461895 |  102 | `	}else{` |
|         - |  103 | `		sxi32 c;` |
|         - |  104 | `		/* Non-alpha stream */` |
| 124176047 |  105 | `		if( pStream->zText[0] == '#' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '[' ){` |
|      7963 |  106 | `			sxu32 nDepth = 1;` |
|         - |  107 | `			/* PHP 8 attribute group '#[ ... ]': skip the whole balanced group as` |
|         - |  108 | `			 * trivia (attributes are not stored yet). Brackets inside string` |
|         - |  109 | `			 * literals and comments must not affect the depth count. An` |
|         - |  110 | `			 * unterminated group is silently consumed up to EOF, consistent` |
|         - |  111 | `			 * with unterminated block comments below.` |
|         - |  112 | `			 */` |
|         - |  113 | `			const unsigned char *zGroupStart;` |
|      7963 |  114 | `			pStream->zText += 2;` |
|      7963 |  115 | `			zGroupStart = pStream->zText;` |
|    650079 |  116 | `			while( pStream->zText < pStream->zEnd && nDepth > 0 ){` |
|    642121 |  117 | `				sxi32 d = pStream->zText[0];` |
|    642121 |  118 | `				if( d == '[' ){` |
|        11 |  119 | `					nDepth++;` |
|    642116 |  120 | `				}else if( d == ']' ){` |
|      7973 |  121 | `					nDepth--;` |
|    638127 |  122 | `				}else if( d == '\'' \|\| d == '"' ){` |
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
|    634121 |  145 | `				}else if( d == '#' \|\| (d == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '/') ){` |
|         - |  146 | `					/* Inline comment inside the group */` |
|       ! 0 |  147 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] != '\n' ){` |
|       ! 0 |  148 | `						pStream->zText++;` |
|       ! 0 |  149 | `					}` |
|       ! 0 |  150 | `					continue; /* Let the outer loop count the newline */` |
|    634099 |  151 | `				}else if( d == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '*' ){` |
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
|    634099 |  165 | `				}else if( d == '\n' ){` |
|         7 |  166 | `					pStream->nLine++;` |
|         3 |  167 | `				}` |
|    642121 |  168 | `				pStream->zText++;` |
|         5 |  169 | `			}` |
|      7963 |  170 | `			if( pUserData && pStream->pSet ){` |
|         - |  171 | `				/* Record the group's inner span (between #[ and its balanced ])` |
|         - |  172 | `				 * in the trivia sidecar, keyed like doc-comments. */` |
|         - |  173 | `				ph7_trivia sTrivia;` |
|      7963 |  174 | `				const unsigned char *zGroupEnd = pStream->zText;` |
|      7963 |  175 | `				if( nDepth == 0 && zGroupEnd > zGroupStart ){` |
|      7963 |  176 | `					zGroupEnd--; /* Exclude the closing ']' */` |
|      3979 |  177 | `				}` |
|      7963 |  178 | `				sTrivia.nTokIdx = SySetUsed(pStream->pSet);` |
|      7963 |  179 | `				sTrivia.iKind = PH7_TRIVIA_ATTR;` |
|      7963 |  180 | `				SyStringInitFromBuf(&sTrivia.sText,(const char *)zGroupStart,(sxu32)(zGroupEnd - zGroupStart));` |
|      7963 |  181 | `				sTrivia.nLine = pToken->nLine;` |
|      7963 |  182 | `				SySetPut((SySet *)pUserData,(const void *)&sTrivia);` |
|      3979 |  183 | `			}` |
|         - |  184 | `			/* Tell the upper-layer to ignore this token */` |
|      7963 |  185 | `			return SXERR_CONTINUE;` |
| 124283046 |  186 | `		}else if( pStream->zText[0] == '#' \|\|` |
| 124168078 |  187 | `			( pStream->zText[0] == '/' &&  &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '/') ){` |
|      7365 |  188 | `				pStream->zText++;` |
|         - |  189 | `				/* Inline comments */` |
|    323569 |  190 | `				while( pStream->zText < pStream->zEnd && pStream->zText[0] != '\n' ){` |
|    316209 |  191 | `					pStream->zText++;` |
|         5 |  192 | `				}` |
|         - |  193 | `				/* Tell the upper-layer to ignore this token */` |
|      7365 |  194 | `				return SXERR_CONTINUE;` |
| 124160729 |  195 | `		}else if( pStream->zText[0] == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '*' ){` |
|         - |  196 | `			/* A doc-comment starts with slash-star-star followed by more` |
|         - |  197 | `			 * content (slash-star-star-slash is the empty comment, not a` |
|         - |  198 | `			 * docblock). Its full span, delimiters included, goes to the` |
|         - |  199 | `			 * trivia sidecar when the caller supplied one — keyed by the` |
|         - |  200 | `			 * index the NEXT real token receives — and never enters the` |
|         - |  201 | `			 * token stream. */` |
|    218567 |  202 | `			const unsigned char *zDocStart = pStream->zText;` |
|    218579 |  203 | `			int bDoc = ( &pStream->zText[2] < pStream->zEnd && pStream->zText[2] == '*'` |
|    327855 |  204 | `			 && ( &pStream->zText[3] >= pStream->zEnd \|\| pStream->zText[3] != '/' ) );` |
|    218567 |  205 | `			pStream->zText += 2;` |
|         - |  206 | `			/* Block comment */` |
|  17267887 |  207 | `			while( pStream->zText < pStream->zEnd ){` |
|  17267887 |  208 | `				if( pStream->zText[0] == '*' ){` |
|    304607 |  209 | `					if( &pStream->zText[1] >= pStream->zEnd \|\| pStream->zText[1] == '/'  ){` |
|    109286 |  210 | `						break;` |
|         - |  211 | `					}` |
|     43020 |  212 | `				}` |
|  17049325 |  213 | `				if( pStream->zText[0] == '\n' ){` |
|       257 |  214 | `					pStream->nLine++;` |
|       126 |  215 | `				}` |
|  17049325 |  216 | `				pStream->zText++;` |
|         5 |  217 | `			}` |
|    218567 |  218 | `			pStream->zText += 2;` |
|    218567 |  219 | `			if( bDoc && pUserData && pStream->pSet ){` |
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
|    218567 |  232 | `			return SXERR_CONTINUE;` |
| 123942167 |  233 | `		}else if( SyisDigit(pStream->zText[0]) ){` |
|   3843305 |  234 | `			pStream->zText++;` |
|         - |  235 | `			/* PHP 7.4: handle underscore separator immediately following the first digit.` |
|         - |  236 | `			 * Check pStream->zText < pStream->zEnd BEFORE forming pStream->zText + 1 so` |
|         - |  237 | `			 * we never compute a pointer past one-past-end. */` |
|   3843300 |  238 | `			if( pStream->zText < pStream->zEnd` |
|   3843300 |  239 | `				&& pStream->zText[0] == '_'` |
|   1921730 |  240 | `				&& pStream->zText + 1 < pStream->zEnd` |
|       160 |  241 | `				&& pStream->zText[1] < 0xc0` |
|       165 |  242 | `				&& SyisDigit(pStream->zText[1]) ){` |
|       151 |  243 | `				pStream->zText++; /* swallow underscore between two digits */` |
|        75 |  244 | `			}` |
|         - |  245 | `			/* Decimal digit stream (PHP 7.4: underscore separator allowed between two digits) */` |
|   4828271 |  246 | `			while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|    984971 |  247 | `				pStream->zText++;` |
|    984966 |  248 | `				if( pStream->zText < pStream->zEnd` |
|    984966 |  249 | `					&& pStream->zText[0] == '_'` |
|    492569 |  250 | `					&& pStream->zText + 1 < pStream->zEnd` |
|       172 |  251 | `					&& pStream->zText[1] < 0xc0` |
|       177 |  252 | `					&& SyisDigit(pStream->zText[1]) ){` |
|       173 |  253 | `					pStream->zText++; /* swallow underscore between two digits */` |
|        86 |  254 | `				}` |
|         5 |  255 | `			}` |
|         - |  256 | `			/* Mark the token as integer until we encounter a real number */` |
|   3843305 |  257 | `			pToken->nType = PH7_TK_INTEGER;` |
|   3843305 |  258 | `			if( pStream->zText < pStream->zEnd ){` |
|   3843305 |  259 | `				c = pStream->zText[0];` |
|   3843305 |  260 | `				if( c == '.' ){` |
|         - |  261 | `					/* Real number (PHP 7.4: underscore separator allowed between two digits) */` |
|      8657 |  262 | `					pStream->zText++;` |
|     18763 |  263 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|     10111 |  264 | `						pStream->zText++;` |
|     10106 |  265 | `						if( pStream->zText < pStream->zEnd` |
|     10106 |  266 | `							&& pStream->zText[0] == '_'` |
|      5059 |  267 | `							&& pStream->zText + 1 < pStream->zEnd` |
|        12 |  268 | `							&& pStream->zText[1] < 0xc0` |
|        17 |  269 | `							&& SyisDigit(pStream->zText[1]) ){` |
|        13 |  270 | `							pStream->zText++;` |
|         6 |  271 | `						}` |
|         5 |  272 | `					}` |
|      8657 |  273 | `					if( pStream->zText < pStream->zEnd ){` |
|      8657 |  274 | `						c = pStream->zText[0];` |
|      8657 |  275 | `						if( c=='e' \|\| c=='E' ){` |
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
|      4326 |  295 | `					}` |
|      8657 |  296 | `					pToken->nType = PH7_TK_REAL;` |
|   3838979 |  297 | `				}else if( c=='e' \|\| c=='E' ){` |
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
|   3834599 |  319 | `				}else if( c == 'x' \|\| c == 'X' ){` |
|         - |  320 | `					/* Hex digit stream (PHP 7.4: underscore separator allowed between two digits) */` |
|    105345 |  321 | `					pStream->zText++;` |
|    456647 |  322 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisHex(pStream->zText[0]) ){` |
|    351307 |  323 | `						pStream->zText++;` |
|    351302 |  324 | `						if( pStream->zText < pStream->zEnd` |
|    351302 |  325 | `							&& pStream->zText[0] == '_'` |
|    175675 |  326 | `							&& pStream->zText + 1 < pStream->zEnd` |
|        48 |  327 | `							&& pStream->zText[1] < 0xc0` |
|        53 |  328 | `							&& SyisHex(pStream->zText[1]) ){` |
|        49 |  329 | `							pStream->zText++;` |
|        24 |  330 | `						}` |
|         5 |  331 | `					}` |
|   3781875 |  332 | `				}else if(c  == 'b' \|\| c == 'B' ){` |
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
|   3729060 |  344 | `				}else if( c == 'o' \|\| c == 'O' ){` |
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
|   1921650 |  357 | `			}` |
|         - |  358 | `			/* PHP 7.4: absorb a trailing malformed underscore run into the` |
|         - |  359 | `			 * numeric token so the compile phase can emit a PHP-compatible` |
|         - |  360 | `			 * "syntax error, unexpected identifier" parse error. Valid` |
|         - |  361 | `			 * separators were already consumed by the per-loop peek logic` |
|         - |  362 | `			 * above, so an underscore here is always misplaced. */` |
|   3843305 |  363 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '_' ){` |
|        18 |  364 | `				pStream->zText++;` |
|        44 |  365 | `				while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0` |
|        49 |  366 | `					&& (SyisAlphaNum(pStream->zText[0]) \|\| pStream->zText[0] == '_') ){` |
|        20 |  367 | `					pStream->zText++;` |
|         4 |  368 | `				}` |
|         7 |  369 | `			}` |
|         - |  370 | `			/* Record token length */` |
|   3843305 |  371 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|   3843305 |  372 | `			return SXRET_OK;` |
|         - |  373 | `		}` |
| 120098867 |  374 | `		c = pStream->zText[0];` |
| 120098867 |  375 | `		pStream->zText++; /* Advance the stream cursor */` |
|         - |  376 | `		/* Assume we are dealing with an operator*/` |
| 120098867 |  377 | `		pToken->nType = PH7_TK_OP;` |
| 120098867 |  378 | `		switch(c){` |
|  24815471 |  379 | `		case '$': pToken->nType = PH7_TK_DOLLAR; break;` |
|   7376091 |  380 | `		case '{': pToken->nType = PH7_TK_OCB;    break;` |
|   7376077 |  381 | `		case '}': pToken->nType = PH7_TK_CCB;    break;` |
|  15542935 |  382 | `		case '(': pToken->nType = PH7_TK_LPAREN; break;` |
|   3284441 |  383 | `		case '[': pToken->nType \|= PH7_TK_OSB;   break; /* Bitwise operation here,since the square bracket token '['` |
|         - |  384 | `														 * is a potential operator [i.e: subscripting] */` |
|   3284447 |  385 | `		case ']': pToken->nType = PH7_TK_CSB;    break;` |
|   7771455 |  386 | `		case ')': {` |
|  15542915 |  387 | `			SySet *pTokSet = pStream->pSet;` |
|         - |  388 | `			/* Assemble type cast operators [i.e: (int),(float),(bool)...] */` |
|  15542915 |  389 | `			if( pTokSet->nUsed >= 2 ){` |
|         - |  390 | `				SyToken *pTmp;` |
|         - |  391 | `				/* Peek the last recongnized token */` |
|  15542913 |  392 | `				pTmp = (SyToken *)SySetPeek(pTokSet);` |
|  15542913 |  393 | `				if( pTmp->nType & PH7_TK_KEYWORD ){` |
|   1205179 |  394 | `					sxi32 nID = SX_PTR_TO_INT(pTmp->pUserData);` |
|   1205179 |  395 | `					if( (sxu32)nID & (PH7_TKWRD_ARRAY\|PH7_TKWRD_INT\|PH7_TKWRD_FLOAT\|PH7_TKWRD_STRING\|PH7_TKWRD_OBJECT\|PH7_TKWRD_BOOL\|PH7_TKWRD_UNSET) ){` |
|   1088061 |  396 | `						pTmp = (SyToken *)SySetAt(pTokSet,pTokSet->nUsed - 2);` |
|   1088061 |  397 | `						if( pTmp->nType & PH7_TK_LPAREN ){` |
|         - |  398 | `							/* Merge the three tokens '(' 'TYPE' ')' into a single one */` |
|    764441 |  399 | `							const char * zTypeCast = "(int)";` |
|    764441 |  400 | `							if( nID & PH7_TKWRD_FLOAT ){` |
|     31215 |  401 | `								zTypeCast = "(float)";` |
|    748836 |  402 | `							}else if( nID & PH7_TKWRD_BOOL ){` |
|     23431 |  403 | `								zTypeCast = "(bool)";` |
|    721518 |  404 | `							}else if( nID & PH7_TKWRD_STRING ){` |
|    378189 |  405 | `								zTypeCast = "(string)";` |
|    520713 |  406 | `							}else if( nID & PH7_TKWRD_ARRAY ){` |
|        36 |  407 | `								zTypeCast = "(array)";` |
|    331604 |  408 | `							}else if( nID & PH7_TKWRD_OBJECT ){` |
|        38 |  409 | `								zTypeCast = "(object)";` |
|    331569 |  410 | `							}else if( nID & PH7_TKWRD_UNSET ){` |
|         3 |  411 | `								zTypeCast = "(unset)";` |
|         1 |  412 | `							}` |
|         - |  413 | `							/* Reflect the change */` |
|    764441 |  414 | `							pToken->nType = PH7_TK_OP;` |
|    764441 |  415 | `							SyStringInitFromBuf(&pToken->sData,zTypeCast,SyStrlen(zTypeCast));` |
|         - |  416 | `							/* Save the instance associated with the type cast operator */` |
|    764441 |  417 | `							pToken->pUserData = (void *)PH7_ExprExtractOperator(&pToken->sData,0);` |
|         - |  418 | `							/* Remove the two previous tokens */` |
|    764441 |  419 | `							pTokSet->nUsed -= 2;` |
|    764441 |  420 | `							return SXRET_OK;` |
|         - |  421 | `						}` |
|    161810 |  422 | `					}` |
|    220369 |  423 | `				}` |
|   7389236 |  424 | `			}` |
|  14778479 |  425 | `			pToken->nType = PH7_TK_RPAREN;` |
|  14778479 |  426 | `			break;` |
|         - |  427 | `				  }` |
|   2793695 |  428 | `		case '\'':{` |
|         - |  429 | `			/* Single quoted string */` |
|   5587395 |  430 | `			pStr->zString++;` |
|  63656811 |  431 | `			while( pStream->zText < pStream->zEnd ){` |
|  63656811 |  432 | `				if( pStream->zText[0] == '\''  ){` |
|   5587409 |  433 | `					if( pStream->zText[-1] != '\\' ){` |
|   5552275 |  434 | `						break;` |
|       ! 0 |  435 | `					}else{` |
|     35139 |  436 | `						const unsigned char *zPtr = &pStream->zText[-2];` |
|     35139 |  437 | `						sxi32 i = 1;` |
|     70279 |  438 | `						while( zPtr > pStream->zInput && zPtr[0] == '\\' ){` |
|     35145 |  439 | `							zPtr--;` |
|     35145 |  440 | `							i++;` |
|         5 |  441 | `						}` |
|     35139 |  442 | `						if((i&1)==0){` |
|     35125 |  443 | `							break;` |
|         - |  444 | `						}` |
|         - |  445 | `					}` |
|         7 |  446 | `				}` |
|  58069421 |  447 | `				if( pStream->zText[0] == '\n' ){` |
|        63 |  448 | `					pStream->nLine++;` |
|        31 |  449 | `				}` |
|  58069421 |  450 | `				pStream->zText++;` |
|         5 |  451 | `			}` |
|         - |  452 | `			/* Record token length and type */` |
|   5587395 |  453 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|   5587395 |  454 | `			pToken->nType = PH7_TK_SSTR;` |
|         - |  455 | `			/* Jump the trailing single quote */` |
|   5587395 |  456 | `			pStream->zText++;` |
|   5587395 |  457 | `			return SXRET_OK;` |
|         - |  458 | `				  }` |
|     62794 |  459 | `		case '"':{` |
|         - |  460 | `			sxi32 iNest;` |
|         - |  461 | `			/* Double quoted string */` |
|    125593 |  462 | `			pStr->zString++;` |
|   1736861 |  463 | `			while( pStream->zText < pStream->zEnd ){` |
|   1736861 |  464 | `				if( pStream->zText[0] == '{' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '$'){` |
|       148 |  465 | `					iNest = 1;` |
|       148 |  466 | `					pStream->zText++;` |
|         - |  467 | `					/* TICKET 1433-40: Hnadle braces'{}' in double quoted string where everything is allowed */` |
|      1348 |  468 | `					while(pStream->zText < pStream->zEnd ){` |
|      1348 |  469 | `						if( pStream->zText[0] == '{' ){` |
|         3 |  470 | `							iNest++;` |
|      1347 |  471 | `						}else if (pStream->zText[0] == '}' ){` |
|       150 |  472 | `							iNest--;` |
|       150 |  473 | `							if( iNest <= 0 ){` |
|       148 |  474 | `								pStream->zText++;` |
|       148 |  475 | `								break;` |
|         1 |  476 | `							}` |
|      1199 |  477 | `						}else if( pStream->zText[0] == '\n' ){` |
|       ! 0 |  478 | `							pStream->nLine++;` |
|       ! 0 |  479 | `						}` |
|      1202 |  480 | `						pStream->zText++;` |
|         2 |  481 | `					}` |
|       148 |  482 | `					if( pStream->zText >= pStream->zEnd ){` |
|       ! 0 |  483 | `						break;` |
|         - |  484 | `					}` |
|        73 |  485 | `				}` |
|   1736861 |  486 | `				if( pStream->zText[0] == '"' ){` |
|    125883 |  487 | `					if( pStream->zText[-1] != '\\' ){` |
|    125585 |  488 | `						break;` |
|       ! 0 |  489 | `					}else{` |
|       303 |  490 | `						const unsigned char *zPtr = &pStream->zText[-2];` |
|       303 |  491 | `						sxi32 i = 1;` |
|       359 |  492 | `						while( zPtr > pStream->zInput && zPtr[0] == '\\' ){` |
|        59 |  493 | `							zPtr--;` |
|        59 |  494 | `							i++;` |
|         3 |  495 | `						}` |
|       303 |  496 | `						if((i&1)==0){` |
|         9 |  497 | `							break;` |
|         - |  498 | `						}` |
|         - |  499 | `					}` |
|       145 |  500 | `				}` |
|   1611273 |  501 | `				if( pStream->zText[0] == '\n' ){` |
|        27 |  502 | `					pStream->nLine++;` |
|        13 |  503 | `				}` |
|   1611273 |  504 | `				pStream->zText++;` |
|         5 |  505 | `			}` |
|         - |  506 | `			/* Record token length and type */` |
|    125593 |  507 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|    125593 |  508 | `			pToken->nType = PH7_TK_DSTR;` |
|         - |  509 | `			/* Jump the trailing quote */` |
|    125593 |  510 | `			pStream->zText++;` |
|    125593 |  511 | `			return SXRET_OK;` |
|         - |  512 | `				  }` |
|         1 |  513 | ``		case '`':{`` |
|         - |  514 | `			/* Backtick quoted string */` |
|         3 |  515 | `			pStr->zString++;` |
|        21 |  516 | `			while( pStream->zText < pStream->zEnd ){` |
|        21 |  517 | ``				if( pStream->zText[0] == '`' && pStream->zText[-1] != '\\' ){`` |
|         3 |  518 | `					break;` |
|         - |  519 | `				}` |
|        19 |  520 | `				if( pStream->zText[0] == '\n' ){` |
|       ! 0 |  521 | `					pStream->nLine++;` |
|       ! 0 |  522 | `				}` |
|        19 |  523 | `				pStream->zText++;` |
|         1 |  524 | `			}` |
|         - |  525 | `			/* Record token length and type */` |
|         3 |  526 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|         3 |  527 | `			pToken->nType = PH7_TK_BSTR;` |
|         - |  528 | `			/* Jump the trailing backtick */` |
|         3 |  529 | `			pStream->zText++;` |
|         3 |  530 | `			return SXRET_OK;` |
|         - |  531 | `				  }` |
|      8827 |  532 | `		case '\\': pToken->nType = PH7_TK_NSSEP;  break;` |
|    550543 |  533 | `		case ':':` |
|   1101091 |  534 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == ':' ){` |
|         - |  535 | `				/* Current operator: '::' */` |
|    422075 |  536 | `				pStream->zText++;` |
|    211040 |  537 | `			}else{` |
|    679021 |  538 | `				pToken->nType = PH7_TK_COLON; /* Single colon */` |
|         - |  539 | `			}` |
|   1101091 |  540 | `			break;` |
|   4386997 |  541 | `		case ',': pToken->nType \|= PH7_TK_COMMA;  break; /* Comma is also an operator */` |
|  11661127 |  542 | `		case ';': pToken->nType = PH7_TK_SEMI;    break;` |
|         - |  543 | `			/* Handle combined operators [i.e: +=,===,!=== ...] */` |
|   4141106 |  544 | `		case '=':` |
|   8282217 |  545 | `			pToken->nType \|= PH7_TK_EQUAL;` |
|   8282217 |  546 | `			if( pStream->zText < pStream->zEnd ){` |
|   8282217 |  547 | `				if( pStream->zText[0] == '=' ){` |
|   1482999 |  548 | `					pToken->nType &= ~PH7_TK_EQUAL;` |
|         - |  549 | `					/* Current operator: == */` |
|   1482999 |  550 | `					pStream->zText++;` |
|   1482999 |  551 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  552 | `						/* Current operator: === */` |
|   1431967 |  553 | `						pStream->zText++;` |
|    715986 |  554 | `					}` |
|   7540720 |  555 | `				}else if( pStream->zText[0] == '>' ){` |
|         - |  556 | `					/* Array operator: => */` |
|    556829 |  557 | `					pToken->nType = PH7_TK_ARRAY_OP;` |
|    556829 |  558 | `					pStream->zText++;` |
|    278417 |  559 | `				}else{` |
|         - |  560 | `					/* TICKET 1433-0010: Reference operator '=&' */` |
|   6242399 |  561 | `					const unsigned char *zCur = pStream->zText;` |
|   6242399 |  562 | `					sxu32 nLine = 0;` |
|  12484627 |  563 | `					while( zCur < pStream->zEnd && zCur[0] < 0xc0 && SyisSpace(zCur[0]) ){` |
|   6242233 |  564 | `						if( zCur[0] == '\n' ){` |
|         5 |  565 | `							nLine++;` |
|         2 |  566 | `						}` |
|   6242233 |  567 | `						zCur++;` |
|         5 |  568 | `					}` |
|   6242399 |  569 | `					if( zCur < pStream->zEnd && zCur[0] == '&' ){` |
|         - |  570 | `						/* Current operator: =& */` |
|        75 |  571 | `						pToken->nType &= ~PH7_TK_EQUAL;` |
|        75 |  572 | `						SyStringInitFromBuf(pStr,"=&",sizeof("=&")-1);` |
|         - |  573 | `						/* Update token stream */` |
|        75 |  574 | `						pStream->zText = &zCur[1];` |
|        75 |  575 | `						pStream->nLine += nLine;` |
|        36 |  576 | `					}` |
|         - |  577 | `				}` |
|   4141106 |  578 | `			}` |
|   8282217 |  579 | `			break;` |
|    454499 |  580 | `		case '!':` |
|    909003 |  581 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  582 | `				/* Current operator: != */` |
|    436831 |  583 | `				pStream->zText++;` |
|    436831 |  584 | `				if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  585 | `					/* Current operator: !== */` |
|    421209 |  586 | `					pStream->zText++;` |
|    210602 |  587 | `				}` |
|    218413 |  588 | `			}` |
|    909003 |  589 | `			break;` |
|    341467 |  590 | `		case '&':` |
|    682939 |  591 | `			pToken->nType \|= PH7_TK_AMPER;` |
|    682939 |  592 | `			if( pStream->zText < pStream->zEnd ){` |
|    682939 |  593 | `				if( pStream->zText[0] == '&' ){` |
|    452657 |  594 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|         - |  595 | `					/* Current operator: && */` |
|    452657 |  596 | `					pStream->zText++;` |
|    456613 |  597 | `				}else if( pStream->zText[0] == '=' ){` |
|         7 |  598 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|         - |  599 | `					/* Current operator: &= */` |
|         7 |  600 | `					pStream->zText++;` |
|         3 |  601 | `				}` |
|    341467 |  602 | `			}` |
|    682939 |  603 | `			break;` |
|    200939 |  604 | `		case '\|':` |
|    401883 |  605 | `			if( pStream->zText < pStream->zEnd ){` |
|    401883 |  606 | `				if( pStream->zText[0] == '\|' ){` |
|         - |  607 | `					/* Current operator: \|\| */` |
|    327563 |  608 | `					pStream->zText++;` |
|    238104 |  609 | `				}else if( pStream->zText[0] == '=' ){` |
|         - |  610 | `					/* Current operator: \|= */` |
|     58481 |  611 | `					pStream->zText++;` |
|     45087 |  612 | `				}else if( pStream->zText[0] == '>' ){` |
|         - |  613 | `					/* Current operator: \|> (PHP 8.5 pipe) */` |
|        27 |  614 | `					pStream->zText++;` |
|        13 |  615 | `				}` |
|    200939 |  616 | `			}` |
|    401883 |  617 | `			break;` |
|    228633 |  618 | `		case '+':` |
|    457271 |  619 | `			if( pStream->zText < pStream->zEnd ){` |
|    457271 |  620 | `				if( pStream->zText[0] == '+' ){` |
|         - |  621 | `					/* Current operator: ++ */` |
|    187523 |  622 | `					pStream->zText++;` |
|    363512 |  623 | `				}else if( pStream->zText[0] == '=' ){` |
|         - |  624 | `					/* Current operator: += */` |
|     54653 |  625 | `					pStream->zText++;` |
|     27324 |  626 | `				}` |
|    228633 |  627 | `			}` |
|    457271 |  628 | `			break;` |
|   2935338 |  629 | `		case '-':` |
|   5870681 |  630 | `			if( pStream->zText < pStream->zEnd ){` |
|   5870681 |  631 | `				if( pStream->zText[0] == '-' ){` |
|         - |  632 | `					/* Current operator: -- */` |
|     31223 |  633 | `					pStream->zText++;` |
|   5855072 |  634 | `				}else if( pStream->zText[0] == '=' ){` |
|         - |  635 | `					/* Current operator: -= */` |
|        14 |  636 | `					pStream->zText++;` |
|   5839457 |  637 | `				}else if( pStream->zText[0] == '>' ){` |
|         - |  638 | `					/* Current operator: -> */` |
|   5596893 |  639 | `					pStream->zText++;` |
|   2798444 |  640 | `				}` |
|   2935338 |  641 | `			}` |
|   5870681 |  642 | `			break;` |
|     19705 |  643 | `		case '*':` |
|     39415 |  644 | `			if( pStream->zText < pStream->zEnd ){` |
|     39415 |  645 | `				if( pStream->zText[0] == '*' ){` |
|         - |  646 | `					/* Current operator: ** or **= */` |
|       137 |  647 | `					pStream->zText++;` |
|       137 |  648 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  649 | `						/* Current operator: **= */` |
|        23 |  650 | `						pStream->zText++;` |
|        12 |  651 | `					}` |
|     39347 |  652 | `				}else if( pStream->zText[0] == '=' ){` |
|         - |  653 | `					/* Current operator: *= */` |
|        27 |  654 | `					pStream->zText++;` |
|        12 |  655 | `				}` |
|     19705 |  656 | `			}` |
|     39415 |  657 | `			break;` |
|      1999 |  658 | `		case '/':` |
|      4003 |  659 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  660 | `				/* Current operator: /= */` |
|         9 |  661 | `				pStream->zText++;` |
|         4 |  662 | `			}` |
|      4003 |  663 | `			break;` |
|     17591 |  664 | `		case '%':` |
|     35187 |  665 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  666 | `				/* Current operator: %= */` |
|         9 |  667 | `				pStream->zText++;` |
|         4 |  668 | `			}` |
|     35187 |  669 | `			break;` |
|        11 |  670 | `		case '^':` |
|        23 |  671 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  672 | `				/* Current operator: ^= */` |
|         9 |  673 | `				pStream->zText++;` |
|         4 |  674 | `			}` |
|        23 |  675 | `			break;` |
|   1002138 |  676 | `		case '.':` |
|   2004281 |  677 | `			if( pStream->zText + 1 < pStream->zEnd && pStream->zText[0] == '.' && pStream->zText[1] == '.' ){` |
|         - |  678 | `				/* Ellipsis: ... */` |
|     27757 |  679 | `				pStream->zText += 2;` |
|     27757 |  680 | `				pToken->nType = PH7_TK_ELLIPSIS;` |
|   1990405 |  681 | `			}else if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  682 | `				/* Current operator: .= */` |
|    339307 |  683 | `				pStream->zText++;` |
|    169651 |  684 | `			}` |
|   2004281 |  685 | `			break;` |
|    201093 |  686 | `		case '<':` |
|    402191 |  687 | `			if( pStream->zText < pStream->zEnd ){` |
|    402191 |  688 | `				if( pStream->zText[0] == '<' ){` |
|         - |  689 | `					/* Current operator: << */` |
|       151 |  690 | `					pStream->zText++;` |
|       151 |  691 | `					if( pStream->zText < pStream->zEnd ){` |
|       151 |  692 | `						if( pStream->zText[0] == '=' ){` |
|         - |  693 | `							/* Current operator: <<= */` |
|         9 |  694 | `							pStream->zText++;` |
|       147 |  695 | `						}else if( pStream->zText[0] == '<' ){` |
|         - |  696 | `							/* Current Token: <<<  */` |
|       129 |  697 | `							pStream->zText++;` |
|         - |  698 | `							/* This may be the beginning of a Heredoc/Nowdoc string,try to delimit it */` |
|       129 |  699 | `							rc = LexExtractHeredoc(&(*pStream),&(*pToken));` |
|       129 |  700 | `							if( rc == SXRET_OK ){` |
|         - |  701 | `								/* Here/Now doc successfuly extracted */` |
|       129 |  702 | `								return SXRET_OK;` |
|         - |  703 | `							}` |
|       ! 0 |  704 | `						}` |
|        12 |  705 | `					}` |
|    402056 |  706 | `				}else if( pStream->zText[0] == '>' ){` |
|         - |  707 | `					/* Current operator: <> */` |
|         5 |  708 | `					pStream->zText++;` |
|    402043 |  709 | `				}else if( pStream->zText[0] == '=' ){` |
|         - |  710 | `					/* Current operator: <= or <=> */` |
|     66417 |  711 | `					pStream->zText++;` |
|     66417 |  712 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '>' ){` |
|         - |  713 | `						/* Current operator: <=> */` |
|     27369 |  714 | `						pStream->zText++;` |
|     13682 |  715 | `					}` |
|     33206 |  716 | `				}` |
|    201031 |  717 | `			}` |
|    402067 |  718 | `			break;` |
|    158022 |  719 | `		case '>':` |
|    316049 |  720 | `			if( pStream->zText < pStream->zEnd ){` |
|    316049 |  721 | `				if( pStream->zText[0] == '>' ){` |
|         - |  722 | `					/* Current operator: >> */` |
|     19515 |  723 | `					pStream->zText++;` |
|     19515 |  724 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  725 | `						/* Current operator: >>= */` |
|        11 |  726 | `						pStream->zText++;` |
|        10 |  727 | `					}` |
|    306294 |  728 | `				}else if( pStream->zText[0] == '=' ){` |
|         - |  729 | `					/* Current operator: >= */` |
|    105343 |  730 | `					pStream->zText++;` |
|     52669 |  731 | `				}` |
|    158022 |  732 | `			}` |
|    316049 |  733 | `			break;` |
|    292283 |  734 | `		case '?':` |
|    584571 |  735 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '?' ){` |
|         - |  736 | `				/* Null coalescing operator: ?? */` |
|     58757 |  737 | `				pStream->zText++;` |
|     58757 |  738 | `				if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  739 | `					/* Null coalescing assignment operator (PHP 7.4) */` |
|       101 |  740 | `					pStream->zText++;` |
|        48 |  741 | `				}` |
|    555195 |  742 | `			}else if( (pStream->zEnd - pStream->zText) >= 2` |
|    525819 |  743 | `				&& pStream->zText[0] == '-' && pStream->zText[1] == '>' ){` |
|         - |  744 | `				/* Nullsafe object operator (PHP 8.0): ?-> */` |
|       119 |  745 | `				pStream->zText += 2;` |
|        57 |  746 | `			}` |
|    584566 |  747 | `			break;` |
|      7935 |  748 | `		default:` |
|     15870 |  749 | `			break;` |
|         - |  750 | `		}` |
| 113621327 |  751 | `		if( pStr->nByte <= 0 ){` |
|         - |  752 | `			/* Record token length */` |
| 113621255 |  753 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  56810625 |  754 | `		}` |
| 113621327 |  755 | `		if( pToken->nType & PH7_TK_OP ){` |
|         - |  756 | `			const ph7_expr_op *pOp;` |
|         - |  757 | `			/* Check if the extracted token is an operator */` |
|  27514321 |  758 | `			pOp = PH7_ExprExtractOperator(pStr,(SyToken *)SySetPeek(pStream->pSet));` |
|  27514321 |  759 | `			if( pOp == 0 ){` |
|         - |  760 | `				/* Not an operator */` |
|       ! 0 |  761 | `				pToken->nType &= ~PH7_TK_OP;` |
|       ! 0 |  762 | `				if( pToken->nType <= 0 ){` |
|       ! 0 |  763 | `					pToken->nType = PH7_TK_OTHER;` |
|       ! 0 |  764 | `				}` |
|       ! 0 |  765 | `			}else{` |
|         - |  766 | `				/* Save the instance associated with this operator for later processing */` |
|  27514321 |  767 | `				pToken->pUserData = (void *)pOp;` |
|         - |  768 | `			}` |
|  13757158 |  769 | `		}` |
|         - |  770 | `	}` |
|         - |  771 | `	/* Tell the upper-layer to save the extracted token for later processing */` |
| 176545107 |  772 | `	return SXRET_OK;` |
|  93549917 |  773 | `}` |
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
|  62923785 |  793 | `static sxu32 KeywordCode(const char *z, int n){` |
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
|  62923785 |  883 | `  if( n<2 ) return PH7_TK_ID;` |
|         - |  884 | ``  /* Hash through UNSIGNED bytes: `char` is signed on most targets, so an`` |
|         - |  885 | `   * identifier carrying a high byte (php allows 0x80-0xFF in identifiers, and` |
|         - |  886 | ``   * every UTF-8 name has them) made the xor negative, and C's `%` keeps that`` |
|         - |  887 | `   * sign — aHash[-46] read off the front of the table. ASCII is unaffected, so` |
|         - |  888 | `   * the generated keyword buckets still resolve exactly as before. */` |
|  55445895 |  889 | `  h = (int)(((sxu32)(sxu8)z[0]*4) ^ ((sxu32)(sxu8)z[n-1]*3) ^ (sxu32)n) % 151;` |
|  85431479 |  890 | `  for(i=((int)aHash[h])-1; i>=0; i=((int)aNext[i])-1){` |
|  50809693 |  891 | `    if( (int)aLen[i]==n && SyMemcmp(&zText[aOffset[i]],z,n)==0 ){` |
|         - |  892 | `       /* PH7_TKWRD_EXTENDS */` |
|         - |  893 | `       /* PH7_TKWRD_ENDSWITCH */` |
|         - |  894 | `       /* PH7_TKWRD_SWITCH */` |
|         - |  895 | `       /* PH7_TKWRD_PRINT */` |
|         - |  896 | `       /* PH7_TKWRD_INT */` |
|         - |  897 | `       /* PH7_TKWRD_REQONCE */` |
|         - |  898 | `       /* PH7_TKWRD_REQUIRE */` |
|         - |  899 | `       /* PH7_TK_ID */` |
|         - |  900 | `       /* PH7_TKWRD_ENDDEC */` |
|         - |  901 | `       /* PH7_TKWRD_DECLARE */` |
|         - |  902 | `       /* PH7_TKWRD_RETURN */` |
|         - |  903 | `       /* PH7_TKWRD_NAMESPACE */` |
|         - |  904 | `       /* PH7_TKWRD_ECHO */` |
|         - |  905 | `       /* PH7_TKWRD_OBJECT */` |
|         - |  906 | `       /* PH7_TKWRD_THROW */` |
|         - |  907 | `       /* PH7_TKWRD_BOOL */` |
|         - |  908 | `       /* PH7_TKWRD_BOOL */` |
|         - |  909 | `       /* PH7_TKWRD_AND */` |
|         - |  910 | `       /* PH7_TKWRD_DEFAULT */` |
|         - |  911 | `       /* PH7_TKWRD_TRY */` |
|         - |  912 | `       /* PH7_TKWRD_CASE */` |
|         - |  913 | `       /* PH7_TKWRD_SELF */` |
|         - |  914 | `       /* PH7_TKWRD_FINAL */` |
|         - |  915 | `       /* PH7_TKWRD_LIST */` |
|         - |  916 | `       /* PH7_TKWRD_STATIC */` |
|         - |  917 | `       /* PH7_TKWRD_CLONE */` |
|         - |  918 | `       /* PH7_TK_ID */` |
|         - |  919 | `       /* PH7_TKWRD_NEW */` |
|         - |  920 | `       /* PH7_TKWRD_CONST */` |
|         - |  921 | `       /* PH7_TKWRD_STRING */` |
|         - |  922 | `       /* PH7_TKWRD_GLOBAL */` |
|         - |  923 | `       /* PH7_TKWRD_USE */` |
|         - |  924 | `       /* PH7_TKWRD_ELIF */` |
|         - |  925 | `       /* PH7_TKWRD_ELSE */` |
|         - |  926 | `       /* PH7_TKWRD_IF */` |
|         - |  927 | `       /* PH7_TKWRD_FLOAT */` |
|         - |  928 | `       /* PH7_TKWRD_VAR */` |
|         - |  929 | `       /* PH7_TKWRD_ARRAY */` |
|         - |  930 | `       /* PH7_TKWRD_AND */` |
|         - |  931 | `       /* PH7_TKWRD_DIE */` |
|         - |  932 | `       /* PH7_TKWRD_ECHO */` |
|         - |  933 | `       /* PH7_TKWRD_USE */` |
|         - |  934 | `       /* PH7_TKWRD_ECHO */` |
|         - |  935 | `       /* PH7_TKWRD_ABSTRACT */` |
|         - |  936 | `       /* PH7_TKWRD_CLASS */` |
|         - |  937 | `       /* PH7_TKWRD_AS */` |
|         - |  938 | `       /* PH7_TKWRD_CONTINUE */` |
|         - |  939 | `       /* PH7_TKWRD_ENDIF */` |
|         - |  940 | `       /* PH7_TKWRD_FUNCTION */` |
|         - |  941 | `       /* PH7_TKWRD_DIE */` |
|         - |  942 | `       /* PH7_TKWRD_ENDWHILE */` |
|         - |  943 | `       /* PH7_TKWRD_WHILE */` |
|         - |  944 | `       /* PH7_TKWRD_EVAL */` |
|         - |  945 | `       /* PH7_TKWRD_DO */` |
|         - |  946 | `       /* PH7_TKWRD_EXIT */` |
|         - |  947 | `       /* PH7_TKWRD_GOTO */` |
|         - |  948 | `       /* PH7_TKWRD_IMPLEMENTS */` |
|         - |  949 | `       /* PH7_TKWRD_INCONCE */` |
|         - |  950 | `       /* PH7_TKWRD_INCLUDE */` |
|         - |  951 | `       /* PH7_TKWRD_EMPTY */` |
|         - |  952 | `       /* PH7_TKWRD_INSTANCEOF */` |
|         - |  953 | `       /* PH7_TKWRD_INTERFACE */` |
|         - |  954 | `       /* PH7_TKWRD_INT */` |
|         - |  955 | `       /* PH7_TKWRD_ENDFOR */` |
|         - |  956 | `       /* PH7_TKWRD_END4EACH */` |
|         - |  957 | `       /* PH7_TKWRD_FOR */` |
|         - |  958 | `       /* PH7_TKWRD_FOREACH */` |
|         - |  959 | `       /* PH7_TKWRD_OR */` |
|         - |  960 | `       /* PH7_TKWRD_ISSET */` |
|         - |  961 | `       /* PH7_TKWRD_PARENT */` |
|         - |  962 | `       /* PH7_TKWRD_PRIVATE */` |
|         - |  963 | `       /* PH7_TKWRD_PROTECTED */` |
|         - |  964 | `       /* PH7_TKWRD_PUBLIC */` |
|         - |  965 | `       /* PH7_TKWRD_CATCH */` |
|         - |  966 | `       /* PH7_TKWRD_UNSET */` |
|         - |  967 | `       /* PH7_TKWRD_XOR */` |
|         - |  968 | `       /* PH7_TKWRD_ARRAY */` |
|         - |  969 | `       /* PH7_TKWRD_AS */` |
|         - |  970 | `       /* PH7_TKWRD_ARRAY */` |
|         - |  971 | `       /* PH7_TKWRD_EXIT */` |
|         - |  972 | `       /* PH7_TKWRD_UNSET */` |
|         - |  973 | `       /* PH7_TKWRD_XOR */` |
|         - |  974 | `       /* PH7_TKWRD_OR */` |
|         - |  975 | `       /* PH7_TKWRD_BREAK */` |
|  20824109 |  976 | `      return aCode[i];` |
|         - |  977 | `    }` |
|  14992797 |  978 | `  }` |
|         - |  979 | `  /* Linear fallback for keywords not in the auto-generated hash table */` |
|  34621791 |  980 | `  if( n==5 && SyMemcmp(z,"trait",5)==0 ) return PH7_TKWRD_TRAIT;` |
|  34613893 |  981 | `  if( n==9 && SyMemcmp(z,"insteadof",9)==0 ) return PH7_TKWRD_INSTEADOF;` |
|  34613889 |  982 | `  if( n==7 && SyMemcmp(z,"finally",7)==0 ) return PH7_TKWRD_FINALLY;` |
|  34613717 |  983 | `  if( n==5 && SyMemcmp(z,"yield",5)==0 ) return PH7_TKWRD_YIELD;` |
|  34597733 |  984 | `  if( n==5 && SyMemcmp(z,"match",5)==0 ) return PH7_TKWRD_MATCH;` |
|  34597655 |  985 | `  return PH7_TK_ID;` |
|  31461895 |  986 | `}` |
|         - |  987 | `/* --- End of Automatically generated code --- */` |
|         - |  988 | `/* SPDX-SnippetEnd */` |
|         - |  989 | `/*` |
|         - |  990 | ` * Extract a heredoc/nowdoc text from a raw PHP input.` |
|         - |  991 | ` * According to the PHP language reference manual:` |
|         - |  992 | ` *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier` |
|         - |  993 | ` *  is provided, then a newline. The string itself follows, and then the same identifier again` |
|         - |  994 | ` *  to close the quotation.` |
|         - |  995 | ` *  The closing identifier must begin in the first column of the line. Also, the identifier must` |
|         - |  996 | ` *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric` |
|         - |  997 | ` *  characters and underscores, and must start with a non-digit character or underscore.` |
|         - |  998 | ` *  Heredoc text behaves just like a double-quoted string, without the double quotes.` |
|         - |  999 | ` *  This means that quotes in a heredoc do not need to be escaped, but the escape codes listed` |
|         - | 1000 | ` *  above can still be used. Variables are expanded, but the same care must be taken when expressing` |
|         - | 1001 | ` *  complex variables inside a heredoc as with strings.` |
|         - | 1002 | ` *  Nowdocs are to single-quoted strings what heredocs are to double-quoted strings.` |
|         - | 1003 | ` *  A nowdoc is specified similarly to a heredoc, but no parsing is done inside a nowdoc.` |
|         - | 1004 | ` *  The construct is ideal for embedding PHP code or other large blocks of text without the need` |
|         - | 1005 | ` *  for escaping. It shares some features in common with the SGML <![CDATA[ ]]> construct, in that` |
|         - | 1006 | ` *  it declares a block of text which is not for parsing.` |
|         - | 1007 | ` *  A nowdoc is identified with the same <<< sequence used for heredocs, but the identifier which follows` |
|         - | 1008 | ` *  is enclosed in single quotes, e.g. <<<'EOT'. All the rules for heredoc identifiers also apply to nowdoc` |
|         - | 1009 | ` *  identifiers, especially those regarding the appearance of the closing identifier.` |
|         - | 1010 | ` * Symisc Extension:` |
|         - | 1011 | ` * The closing delimiter can now start with a digit or undersocre or it can be an UTF-8 stream.` |
|         - | 1012 | ` * Example:` |
|         - | 1013 | ` *  <<<123` |
|         - | 1014 | ` *    HEREDOC Here` |
|         - | 1015 | ` * 123` |
|         - | 1016 | ` *  or` |
|         - | 1017 | ` *  <<<___` |
|         - | 1018 | ` *   HEREDOC Here` |
|         - | 1019 | ` *  ___` |
|         - | 1020 | ` */` |
|       124 | 1021 | `static sxi32 LexExtractHeredoc(SyStream *pStream,SyToken *pToken)` |
|         5 | 1022 | `{` |
|       129 | 1023 | `	const unsigned char *zIn  = pStream->zText;` |
|       129 | 1024 | `	const unsigned char *zEnd = pStream->zEnd;` |
|         - | 1025 | `	const unsigned char *zPtr;` |
|       129 | 1026 | `	sxu8 bNowDoc = FALSE;` |
|         - | 1027 | `	SyString sDelim;` |
|         - | 1028 | `	SyString sStr;` |
|         - | 1029 | `	/* Jump leading white spaces */` |
|       141 | 1030 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|        13 | 1031 | `		zIn++;` |
|         1 | 1032 | `	}` |
|       129 | 1033 | `	if( zIn >= zEnd ){` |
|         - | 1034 | `		/* A simple symbol,return immediately */` |
|       ! 0 | 1035 | `		return SXERR_CONTINUE;` |
|         - | 1036 | `	}` |
|       129 | 1037 | `	if( zIn[0] == '\'' \|\| zIn[0] == '"' ){` |
|         - | 1038 | `		/* Make sure we are dealing with a nowdoc */` |
|        55 | 1039 | `		bNowDoc =  zIn[0] == '\'' ? TRUE : FALSE;` |
|        55 | 1040 | `		zIn++;` |
|        26 | 1041 | `	}` |
|       129 | 1042 | `	if( zIn[0] < 0xc0 && !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|         - | 1043 | `		/* Invalid delimiter,return immediately */` |
|       ! 0 | 1044 | `		return SXERR_CONTINUE;` |
|         - | 1045 | `	}` |
|         - | 1046 | `	/* Isolate the identifier */` |
|       129 | 1047 | `	sDelim.zString = (const char *)zIn;` |
|       132 | 1048 | `	for(;;){` |
|       269 | 1049 | `		zPtr = zIn;` |
|         - | 1050 | `		/* Skip alphanumeric stream */` |
|       843 | 1051 | `		while( zPtr < zEnd && zPtr[0] < 0xc0 && (SyisAlphaNum(zPtr[0]) \|\| zPtr[0] == '_') ){` |
|       447 | 1052 | `			zPtr++;` |
|         5 | 1053 | `		}` |
|       269 | 1054 | `		if( zPtr < zEnd && zPtr[0] >= 0xc0 ){` |
|        19 | 1055 | `			zPtr++;` |
|         - | 1056 | `			/* UTF-8 stream */` |
|        37 | 1057 | `			while( zPtr < zEnd && ((zPtr[0] & 0xc0) == 0x80) ){` |
|        19 | 1058 | `				zPtr++;` |
|         1 | 1059 | `			}` |
|         9 | 1060 | `		}` |
|       269 | 1061 | `		if( zPtr == zIn ){` |
|         - | 1062 | `			/* Not an UTF-8 or alphanumeric stream */` |
|       129 | 1063 | `			break;` |
|         - | 1064 | `		}` |
|         - | 1065 | `		/* Synchronize pointers */` |
|       145 | 1066 | `		zIn = zPtr;` |
|         5 | 1067 | `	}` |
|         - | 1068 | `	/* Get the identifier length */` |
|       129 | 1069 | `	sDelim.nByte = (sxu32)((const char *)zIn-sDelim.zString);` |
|       129 | 1070 | `	if( zIn[0] == '"' \|\| (bNowDoc && zIn[0] == '\'') ){` |
|         - | 1071 | `		/* Jump the trailing single quote */` |
|        55 | 1072 | `		zIn++;` |
|        26 | 1073 | `	}` |
|         - | 1074 | `	/* Jump trailing white spaces */` |
|       129 | 1075 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|       ! 0 | 1076 | `		zIn++;` |
|       ! 0 | 1077 | `	}` |
|       129 | 1078 | `	if( sDelim.nByte <= 0 \|\| zIn >= zEnd \|\| zIn[0] != '\n' ){` |
|         - | 1079 | `		/* Invalid syntax */` |
|       ! 0 | 1080 | `		return SXERR_CONTINUE;` |
|         - | 1081 | `	}` |
|       129 | 1082 | `	pStream->nLine++; /* Increment line counter */` |
|       129 | 1083 | `	zIn++;` |
|         - | 1084 | `	/* Isolate the delimited string */` |
|       129 | 1085 | `	sStr.zString = (const char *)zIn;` |
|         - | 1086 | `	/* PHP 7.3 flexible heredoc/nowdoc: the closing marker may be preceded` |
|         - | 1087 | `	 * by whitespace (spaces/tabs), and may be followed by any non-identifier` |
|         - | 1088 | `	 * character. The indent count is recorded in pToken->pUserData and the` |
|         - | 1089 | `	 * compile phase strips it from each body line. */` |
|         - | 1090 | `	{` |
|       129 | 1091 | `		const unsigned char *zMarkerLine = zIn; /* Start of marker's line (set on match) */` |
|       129 | 1092 | `		sxu32 nIndent = 0;` |
|       296 | 1093 | `		for(;;){` |
|       363 | 1094 | `			const unsigned char *zLineStart = zIn;` |
|         - | 1095 | `			/* Skip leading space/tab on this line */` |
|      1020 | 1096 | `			while( zIn < zEnd && (zIn[0] == ' ' \|\| zIn[0] == '\t') ){` |
|       483 | 1097 | `				zIn++;` |
|         5 | 1098 | `			}` |
|       358 | 1099 | `			if( (sxu32)(zEnd - zIn) >= sDelim.nByte` |
|       362 | 1100 | `				&& SyMemcmp((const void *)sDelim.zString,(const void *)zIn,sDelim.nByte) == 0 ){` |
|         - | 1101 | `				int bIdentCont;` |
|       127 | 1102 | `				zPtr = &zIn[sDelim.nByte];` |
|         - | 1103 | `				/* Disambiguate: next byte must not continue an identifier.` |
|         - | 1104 | `				 * A leading byte >= 0xc0 starts a multi-byte UTF-8 sequence,` |
|         - | 1105 | `				 * which PHP identifiers may contain, so treat it as ident. */` |
|       127 | 1106 | `				if( zPtr >= zEnd ){` |
|       ! 0 | 1107 | `					bIdentCont = 0;` |
|       127 | 1108 | `				}else if( zPtr[0] >= 0xc0 ){` |
|       ! 0 | 1109 | `					bIdentCont = 1;` |
|       ! 0 | 1110 | `				}else{` |
|       127 | 1111 | `					bIdentCont = (SyisAlphaNum(zPtr[0]) \|\| zPtr[0] == '_');` |
|         - | 1112 | `				}` |
|       127 | 1113 | `				if( !bIdentCont ){` |
|         - | 1114 | `					/* Closing marker found */` |
|       127 | 1115 | `					nIndent = (sxu32)(zIn - zLineStart);` |
|       127 | 1116 | `					zMarkerLine = zLineStart;` |
|       127 | 1117 | `					pStream->zText = zPtr; /* Cursor right after identifier */` |
|       127 | 1118 | `					break;` |
|         - | 1119 | `				}` |
|       ! 0 | 1120 | `			}` |
|         - | 1121 | `			/* Not the closing marker on this line; walk to next newline */` |
|      5249 | 1122 | `			while( zIn < zEnd && zIn[0] != '\n' ){` |
|      5013 | 1123 | `				zIn++;` |
|         5 | 1124 | `			}` |
|       241 | 1125 | `			if( zIn >= zEnd ){` |
|         - | 1126 | `				/* End of input without finding the closing marker */` |
|         3 | 1127 | `				pStream->zText = pStream->zEnd;` |
|         3 | 1128 | `				zMarkerLine = zIn;` |
|         3 | 1129 | `				break;` |
|         - | 1130 | `			}` |
|       239 | 1131 | `			pStream->nLine++;` |
|       239 | 1132 | `			zIn++;` |
|         5 | 1133 | `		}` |
|         - | 1134 | `		/* Body runs from sStr.zString up to just before the marker line */` |
|       129 | 1135 | `		sStr.nByte = (sxu32)((const char *)zMarkerLine - sStr.zString);` |
|       129 | 1136 | `		pToken->nType = bNowDoc ? PH7_TK_NOWDOC : PH7_TK_HEREDOC;` |
|       129 | 1137 | `		SyStringDupPtr(&pToken->sData,&sStr);` |
|         - | 1138 | `		/* Strip exactly one line terminator that precedes the marker's line. */` |
|       124 | 1139 | `		if( pToken->sData.nByte > 0` |
|       125 | 1140 | `			&& pToken->sData.zString[pToken->sData.nByte - 1] == '\n' ){` |
|       119 | 1141 | `			pToken->sData.nByte--;` |
|       114 | 1142 | `			if( pToken->sData.nByte > 0` |
|       119 | 1143 | `				&& pToken->sData.zString[pToken->sData.nByte - 1] == '\r' ){` |
|       ! 0 | 1144 | `				pToken->sData.nByte--;` |
|       ! 0 | 1145 | `			}` |
|        57 | 1146 | `		}` |
|       129 | 1147 | `		pToken->pUserData = SX_INT_TO_PTR(nIndent);` |
|         - | 1148 | `	}` |
|         - | 1149 | `	/* All done */` |
|       129 | 1150 | `	return SXRET_OK;` |
|        67 | 1151 | `}` |
|         - | 1152 | `/*` |
|         - | 1153 | ` * Tokenize a raw PHP input.` |
|         - | 1154 | ` * This is the public tokenizer called by most code generator routines.` |
|         - | 1155 | ` */` |
|    106036 | 1156 | `PH7_PRIVATE sxi32 PH7_TokenizePHP(const char *zInput,sxu32 nLen,sxu32 nLineStart,SySet *pOut,SySet *pTrivia)` |
|         5 | 1157 | `{` |
|         - | 1158 | `	SyLex sLexer;` |
|         - | 1159 | `	sxi32 rc;` |
|         - | 1160 | `	/* Defense-in-depth cap for internal tokenizer calls that bypass ph7_compile() */` |
|    106041 | 1161 | `	if( nLen > PH7_MAX_INPUT_SIZE ){` |
|       ! 0 | 1162 | `		return SXERR_LIMIT;` |
|         - | 1163 | `	}` |
|         - | 1164 | `	/* Initialize the lexer. pTrivia (may be NULL = discard) rides as the` |
|         - | 1165 | `	 * tokenizer callback's user data: doc-comments (and later attribute` |
|         - | 1166 | `	 * groups) are recorded there instead of entering the token stream. */` |
|    106041 | 1167 | `	rc = SyLexInit(&sLexer,&(*pOut),TokenizePHP,pTrivia);` |
|    106041 | 1168 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 1169 | `		return rc;` |
|         - | 1170 | `	}` |
|    106041 | 1171 | `	sLexer.sStream.nLine = nLineStart;` |
|         - | 1172 | `	/* Tokenize input */` |
|    106041 | 1173 | `	rc = SyLexTokenizeInput(&sLexer,zInput,nLen,0,0,0);` |
|         - | 1174 | `	/* Release the lexer */` |
|    106041 | 1175 | `	SyLexRelease(&sLexer);` |
|         - | 1176 | `	/* Tokenization result */` |
|    106041 | 1177 | `	return rc;` |
|     53023 | 1178 | `}` |
|         - | 1179 | `/*` |
|         - | 1180 | ` * High level public tokenizer.` |
|         - | 1181 | ` *  Tokenize the input into PHP tokens and raw tokens [i.e: HTML,XML,Raw text...].` |
|         - | 1182 | ` * According to the PHP language reference manual` |
|         - | 1183 | ` *   When PHP parses a file, it looks for opening and closing tags, which tell PHP` |
|         - | 1184 | ` *   to start and stop interpreting the code between them. Parsing in this manner allows` |
|         - | 1185 | ` *   PHP to be embedded in all sorts of different documents, as everything outside of a pair` |
|         - | 1186 | ` *   of opening and closing tags is ignored by the PHP parser. Most of the time you will see` |
|         - | 1187 | ` *   PHP embedded in HTML documents, as in this example.` |
|         - | 1188 | ` *   <?php echo 'While this is going to be parsed.'; ?>` |
|         - | 1189 | ` *   <p>This will also be ignored.</p>` |
|         - | 1190 | ` *   You can also use more advanced structures:` |
|         - | 1191 | ` *   Example #1 Advanced escaping` |
|         - | 1192 | ` * <?php` |
|         - | 1193 | ` * if ($expression) {` |
|         - | 1194 | ` *   ?>` |
|         - | 1195 | ` *   <strong>This is true.</strong>` |
|         - | 1196 | ` *   <?php` |
|         - | 1197 | ` * } else {` |
|         - | 1198 | ` *   ?>` |
|         - | 1199 | ` *   <strong>This is false.</strong>` |
|         - | 1200 | ` *   <?php` |
|         - | 1201 | ` * }` |
|         - | 1202 | ` * ?>` |
|         - | 1203 | ` * This works as expected, because when PHP hits the ?> closing tags, it simply starts outputting` |
|         - | 1204 | ` * whatever it finds (except for an immediately following newline - see instruction separation ) until it hits` |
|         - | 1205 | ` * another opening tag. The example given here is contrived, of course, but for outputting large blocks of text` |
|         - | 1206 | ` * dropping out of PHP parsing mode is generally more efficient than sending all of the text through echo() or print().` |
|         - | 1207 | ` * There are four different pairs of opening and closing tags which can be used in PHP. Three of those, <?php ?>` |
|         - | 1208 | ` * <script language="php"> </script>  and <? ?> are always available. The other two are short tags and ASP style` |
|         - | 1209 | ` * tags, and can be turned on and off from the php.ini configuration file. As such, while some people find short tags` |
|         - | 1210 | ` * and ASP style tags convenient, they are less portable, and generally not recommended.` |
|         - | 1211 | ` * Note:` |
|         - | 1212 | ` * Also note that if you are embedding PHP within XML or XHTML you will need to use the <?php ?> tags to remain` |
|         - | 1213 | ` * compliant with standards.` |
|         - | 1214 | ` * Example #2 PHP Opening and Closing Tags` |
|         - | 1215 | ` * 1.  <?php echo 'if you want to serve XHTML or XML documents, do it like this'; ?>` |
|         - | 1216 | ` * 2.  <script language="php">` |
|         - | 1217 | ` *       echo 'some editors (like FrontPage) don\'t` |
|         - | 1218 | ` *             like processing instructions';` |
|         - | 1219 | ` *   </script>` |
|         - | 1220 | ` *` |
|         - | 1221 | ` * 3.  <? echo 'this is the simplest, an SGML processing instruction'; ?>` |
|         - | 1222 | ` *   <?= expression ?> This is a shortcut for "<? echo expression ?>"` |
|         - | 1223 | ` */` |
|     13094 | 1224 | `PH7_PRIVATE sxi32 PH7_TokenizeRawText(const char *zInput,sxu32 nLen,SySet *pOut,sxu32 nBaseLine)` |
|         5 | 1225 | `{` |
|     13099 | 1226 | `	const char *zEnd = &zInput[nLen];` |
|     13099 | 1227 | `	const char *zIn  = zInput;` |
|         - | 1228 | `	const char *zCur,*zCurEnd;` |
|     13099 | 1229 | `	SyString sCtag = { 0, 0 };     /* Closing tag */` |
|         - | 1230 | `	SyToken sToken;` |
|         - | 1231 | `	SyString sDoc;` |
|         - | 1232 | `	sxu32 nLine;` |
|         - | 1233 | `	sxi32 iNest;` |
|         - | 1234 | `	sxi32 rc;` |
|         - | 1235 | `	/* Tokenize the input into PHP tokens and raw tokens. nBaseLine is normally 1,` |
|         - | 1236 | `	 * but 2 when a "#!" shebang line was stripped so error lines still match php. */` |
|     13099 | 1237 | `	nLine = nBaseLine;` |
|     13099 | 1238 | `	zCur = zCurEnd   = 0; /* Prevent compiler warning */` |
|     13099 | 1239 | `	sToken.pUserData = 0;` |
|     13099 | 1240 | `	iNest = 0;` |
|     13099 | 1241 | `	sDoc.nByte = 0;` |
|     13099 | 1242 | `	sDoc.zString = ""; /* cc warning */` |
|     13089 | 1243 | `	for(;;){` |
|     26015 | 1244 | `		if( zIn >= zEnd ){` |
|         - | 1245 | `			/* End of input reached */` |
|     12911 | 1246 | `			break;` |
|         - | 1247 | `		}` |
|     13109 | 1248 | `		sToken.nLine = nLine;` |
|     13109 | 1249 | `		zCur = zIn;` |
|     13109 | 1250 | `		zCurEnd = 0;` |
|     13449 | 1251 | `		while( zIn < zEnd ){` |
|     13261 | 1252 | `			 if( zIn[0] == '<' ){` |
|     12921 | 1253 | `				const char *zTmp = zIn; /* End of raw input marker */` |
|     12921 | 1254 | `				zIn++;` |
|     12921 | 1255 | `				if( zIn < zEnd ){` |
|     12921 | 1256 | `					if( zIn[0] == '?' ){` |
|     12921 | 1257 | `						zIn++;` |
|     12921 | 1258 | `						if( (sxu32)(zEnd - zIn) >= sizeof("php")-1 &&  SyStrnicmp(zIn,"php",sizeof("php")-1) == 0 ){` |
|         - | 1259 | `							/* opening tag: <?php */` |
|     12919 | 1260 | `							zIn += sizeof("php")-1;` |
|      6457 | 1261 | `						}` |
|         - | 1262 | `						/* Look for the closing tag '?>' */` |
|     12921 | 1263 | `						SyStringInitFromBuf(&sCtag,"?>",sizeof("?>")-1);` |
|     12921 | 1264 | `						zCurEnd = zTmp;` |
|     12921 | 1265 | `						break;` |
|         - | 1266 | `					}` |
|       ! 0 | 1267 | `				}` |
|       ! 0 | 1268 | `			}else{` |
|       344 | 1269 | `				if( zIn[0] == '\n' ){` |
|         7 | 1270 | `					nLine++;` |
|         3 | 1271 | `				}` |
|       344 | 1272 | `				zIn++;` |
|         - | 1273 | `			 }` |
|         4 | 1274 | `		} /* While(zIn < zEnd) */` |
|     13109 | 1275 | `		if( zCurEnd == 0 ){` |
|        24 | 1276 | `			zCurEnd = zIn;` |
|        10 | 1277 | `		}` |
|         - | 1278 | `		/* Save the raw token */` |
|     13109 | 1279 | `		SyStringInitFromBuf(&sToken.sData,zCur,zCurEnd - zCur);` |
|     13109 | 1280 | `		sToken.nType = PH7_TOKEN_RAW;` |
|     13109 | 1281 | `		rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|     13109 | 1282 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1283 | `			return rc;` |
|         - | 1284 | `		}` |
|     13109 | 1285 | `		if( zIn >= zEnd ){` |
|        24 | 1286 | `			break;` |
|         - | 1287 | `		}` |
|         - | 1288 | `		/* Ignore leading white space */` |
|     27565 | 1289 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|     14481 | 1290 | `			if( zIn[0] == '\n' ){` |
|     14171 | 1291 | `				nLine++;` |
|      7083 | 1292 | `			}` |
|     14481 | 1293 | `			zIn++;` |
|         5 | 1294 | `		}` |
|         - | 1295 | `		/* Delimit the PHP chunk */` |
|     13089 | 1296 | `		sToken.nLine = nLine;` |
|     13089 | 1297 | `		zCur = zIn;` |
|   1533421 | 1298 | `		while( (sxu32)(zEnd - zIn) >= sCtag.nByte ){` |
|         - | 1299 | `			const char *zPtr;` |
|   1527337 | 1300 | `			if( SyMemcmp(zIn,sCtag.zString,sCtag.nByte) == 0 && iNest < 1 ){` |
|      6837 | 1301 | `				break;` |
|         - | 1302 | `			}` |
|         - | 1303 | `			/* Line comment ('#' or '//', but not the '#[' attribute opener): php` |
|         - | 1304 | `			 * ends it at a newline OR at the closing tag, so a '?>' inside a line` |
|         - | 1305 | `			 * comment DOES close the PHP block. Skipping the comment here also` |
|         - | 1306 | `			 * stops the string skip below from treating a quote inside the` |
|         - | 1307 | `			 * comment as a string. Only outside a heredoc body (iNest < 1). */` |
|   1524369 | 1308 | `			if( iNest < 1 &&` |
|   1515810 | 1309 | `				( (zIn[0] == '#' && !(zIn+1 < zEnd && zIn[1] == '[')) \|\|` |
|   1515823 | 1310 | `				  (zIn[0] == '/' && zIn+1 < zEnd && zIn[1] == '/') ) ){` |
|      7733 | 1311 | `				zIn += (zIn[0] == '#') ? 1 : 2;` |
|    316585 | 1312 | `				while( zIn < zEnd && zIn[0] != '\n' ){` |
|    308854 | 1313 | `					if( (sxu32)(zEnd - zIn) >= sCtag.nByte` |
|    308856 | 1314 | `						&& SyMemcmp(zIn,sCtag.zString,sCtag.nByte) == 0 ){` |
|         3 | 1315 | `						break; /* the closing tag terminates the line comment */` |
|         - | 1316 | `					}` |
|    308857 | 1317 | `					zIn++;` |
|         5 | 1318 | `				}` |
|      7365 | 1319 | `				continue;` |
|         - | 1320 | `			}` |
|         - | 1321 | `			/* Block comment: spans everything, including '?>', up to its close. */` |
|   1512977 | 1322 | `			if( iNest < 1 && zIn[0] == '/' && zIn+1 < zEnd && zIn[1] == '*' ){` |
|       279 | 1323 | `				zIn += 2;` |
|     30931 | 1324 | `				while( (sxu32)(zEnd-zIn) >= sizeof("*/") - 1 ){` |
|     30931 | 1325 | `					if( zIn[0] == '*' && zIn[1] == '/' ){` |
|       279 | 1326 | `						zIn += 2;` |
|       279 | 1327 | `						break;` |
|         - | 1328 | `					}` |
|     30657 | 1329 | `					if( zIn[0] == '\n' ){` |
|       257 | 1330 | `						nLine++;` |
|       126 | 1331 | `					}` |
|     30657 | 1332 | `					zIn++;` |
|         5 | 1333 | `				}` |
|       279 | 1334 | `				continue;` |
|         - | 1335 | `			}` |
|         - | 1336 | `			/* Skip over a single/double-quoted or backtick string literal so a` |
|         - | 1337 | `			 * '?>' sequence inside it is not mistaken for the closing tag. Only` |
|         - | 1338 | `			 * outside a heredoc body (iNest < 1); heredocs are delimited by the` |
|         - | 1339 | `			 * label-matching logic above. Escapes (\" \' \\ and a line-continuing` |
|         - | 1340 | `			 * backslash-newline) are honoured. Same-quote nesting inside "{$...}"` |
|         - | 1341 | `			 * interpolation is not tracked, but that can only end the skip early` |
|         - | 1342 | `			 * on a string that has no '?>' anyway, which stays a PHP chunk either` |
|         - | 1343 | `			 * way — it never mis-splits code that works today. */` |
|   1512703 | 1344 | ``			if( iNest < 1 && (zIn[0] == '\'' \|\| zIn[0] == '"' \|\| zIn[0] == '`') ){`` |
|     45105 | 1345 | `				int qch = zIn[0];` |
|     45105 | 1346 | `				zIn++;` |
|    310981 | 1347 | `				while( zIn < zEnd ){` |
|    310981 | 1348 | `					if( zIn[0] == '\\' && zIn + 1 < zEnd ){` |
|     17491 | 1349 | `						if( zIn[1] == '\n' ){ nLine++; }` |
|     17491 | 1350 | `						zIn += 2;` |
|     17491 | 1351 | `						continue;` |
|         - | 1352 | `					}` |
|    293495 | 1353 | `					if( zIn[0] == qch ){ zIn++; break; }` |
|    248395 | 1354 | `					if( zIn[0] == '\n' ){ nLine++; }` |
|    248395 | 1355 | `					zIn++;` |
|         5 | 1356 | `				}` |
|     45105 | 1357 | `				continue;` |
|         - | 1358 | `			}` |
|   1467603 | 1359 | `			if( zIn[0] == '\n' ){` |
|     61181 | 1360 | `				nLine++;` |
|     61181 | 1361 | `				if( iNest > 0 ){` |
|       363 | 1362 | `					zIn++;` |
|       841 | 1363 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|       483 | 1364 | `						zIn++;` |
|         5 | 1365 | `					}` |
|       363 | 1366 | `					zPtr = zIn;` |
|      1711 | 1367 | `					while( zIn < zEnd ){` |
|      1711 | 1368 | `						if( (unsigned char)zIn[0] >= 0xc0 ){` |
|         - | 1369 | `							/* UTF-8 stream */` |
|        19 | 1370 | `							zIn++;` |
|        37 | 1371 | `							SX_JMP_UTF8(zIn,zEnd);` |
|      1698 | 1372 | `						}else if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       184 | 1373 | `							break;` |
|       ! 0 | 1374 | `						}else{` |
|      1335 | 1375 | `							zIn++;` |
|         - | 1376 | `						}` |
|         5 | 1377 | `					}` |
|       363 | 1378 | `					if( (sxu32)(zIn - zPtr) == sDoc.nByte && SyMemcmp(sDoc.zString,zPtr,sDoc.nByte) == 0 ){` |
|       127 | 1379 | `						iNest = 0;` |
|        61 | 1380 | `					}` |
|       363 | 1381 | `					continue;` |
|         5 | 1382 | `				}` |
|   1436836 | 1383 | `			}else if ( (sxu32)(zEnd - zIn) >= sizeof("<<<") && zIn[0] == '<' && zIn[1] == '<' && zIn[2] == '<' && iNest < 1){` |
|       129 | 1384 | `				zIn += sizeof("<<<")-1;` |
|       141 | 1385 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|        13 | 1386 | `					zIn++;` |
|         1 | 1387 | `				}` |
|       129 | 1388 | `				if( zIn[0] == '"' \|\| zIn[0] == '\'' ){` |
|        55 | 1389 | `					zIn++;` |
|        26 | 1390 | `				}` |
|       129 | 1391 | `				zPtr = zIn;` |
|       589 | 1392 | `				while( zIn < zEnd ){` |
|       589 | 1393 | `					if( (unsigned char)zIn[0] >= 0xc0 ){` |
|         - | 1394 | `						/* UTF-8 stream */` |
|        19 | 1395 | `						zIn++;` |
|        37 | 1396 | `						SX_JMP_UTF8(zIn,zEnd);` |
|       576 | 1397 | `					}else if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|        67 | 1398 | `						break;` |
|       ! 0 | 1399 | `					}else{` |
|       447 | 1400 | `						zIn++;` |
|         - | 1401 | `					}` |
|         5 | 1402 | `				}` |
|       129 | 1403 | `				SyStringInitFromBuf(&sDoc,zPtr,zIn-zPtr);` |
|       129 | 1404 | `				SyStringFullTrim(&sDoc);` |
|       129 | 1405 | `				if( sDoc.nByte > 0 ){` |
|       129 | 1406 | `					iNest++;` |
|        62 | 1407 | `				}` |
|       129 | 1408 | `				continue;` |
|         - | 1409 | `			}` |
|   1467121 | 1410 | `			zIn++;` |
|         - | 1411 |  |
|   1467121 | 1412 | `			if ( zIn >= zEnd )` |
|       ! 0 | 1413 | `				break;` |
|         5 | 1414 | `		}` |
|     12921 | 1415 | `		if( (sxu32)(zEnd - zIn) < sCtag.nByte ){` |
|      6089 | 1416 | `			zIn = zEnd;` |
|      3042 | 1417 | `		}` |
|     12921 | 1418 | `		if( zCur < zIn ){` |
|         - | 1419 | `			/* Save the PHP chunk for later processing */` |
|      9769 | 1420 | `			sToken.nType = PH7_TOKEN_PHP;` |
|      9769 | 1421 | `			SyStringInitFromBuf(&sToken.sData,zCur,zIn-zCur);` |
|     19295 | 1422 | `			SyStringRightTrim(&sToken.sData); /* Trim trailing white spaces */` |
|      9769 | 1423 | `			rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|      9769 | 1424 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 1425 | `				return rc;` |
|         - | 1426 | `			}` |
|      4882 | 1427 | `		}` |
|     12921 | 1428 | `		if( zIn < zEnd ){` |
|         - | 1429 | `			/* Jump the trailing closing tag */` |
|      6837 | 1430 | `			zIn += sCtag.nByte;` |
|         - | 1431 | `			/* php's lexer swallows exactly ONE newline immediately after the` |
|         - | 1432 | `			 * closing tag ("?>\n" emits nothing) */` |
|      6837 | 1433 | `			if( zIn < zEnd && zIn[0] == '\r' && zIn + 1 < zEnd && zIn[1] == '\n' ){` |
|       ! 0 | 1434 | `				zIn += 2;` |
|       ! 0 | 1435 | `				nLine++;` |
|      6837 | 1436 | `			}else if( zIn < zEnd && zIn[0] == '\n' ){` |
|        62 | 1437 | `				zIn++;` |
|        62 | 1438 | `				nLine++;` |
|        29 | 1439 | `			}` |
|      3416 | 1440 | `		}` |
|         5 | 1441 | `	} /* For(;;) */` |
|         - | 1442 |  |
|     12931 | 1443 | ` 	return SXRET_OK;` |
|      6468 | 1444 | `}` |
|         - | 1445 |  |
