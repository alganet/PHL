# src/ph7/lex.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 786/846 lines (92.91%)

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
| 143799868 |   19 | `static sxi32 TokenizePHP(SyStream *pStream,SyToken *pToken,void *pUserData,void *pCtxData)` |
|         5 |   20 | `{` |
|         - |   21 | `	SyString *pStr;` |
|         - |   22 | `	sxi32 rc;` |
|         - |   23 | `	/* Ignore leading white spaces */` |
| 209710055 |   24 | `	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){` |
|         - |   25 | `		/* Advance the stream cursor */` |
|  65910187 |   26 | `		if( pStream->zText[0] == '\n' ){` |
|         - |   27 | `			/* Update line counter */` |
|     51689 |   28 | `			pStream->nLine++;` |
|     25842 |   29 | `		}` |
|  65910187 |   30 | `		pStream->zText++;` |
|         5 |   31 | `	}` |
| 143799873 |   32 | `	if( pStream->zText >= pStream->zEnd ){` |
|         - |   33 | `		/* End of input reached */` |
|       ! 0 |   34 | `		return SXERR_EOF;` |
|         - |   35 | `	}` |
|         - |   36 | `	/* Record token starting position and line */` |
| 143799873 |   37 | `	pToken->nLine = pStream->nLine;` |
| 143799873 |   38 | `	pToken->pUserData = 0;` |
| 143799873 |   39 | `	pStr = &pToken->sData;` |
| 143799873 |   40 | `	SyStringInitFromBuf(pStr,pStream->zText,0);` |
| 168122976 |   41 | `	if( pStream->zText[0] >= 0xc0 \|\| SyisAlpha(pStream->zText[0]) \|\| pStream->zText[0] == '_' ){` |
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
|  48646211 |   52 | `		if( pStream->zText[0] < 0xc0 ){` |
|  48646195 |   53 | `			pStream->zText++;` |
|  24323095 |   54 | `		}` |
|  45809586 |   55 | `		for(;;){` |
|  91619177 |   56 | `			zIn = pStream->zText;` |
|  91619177 |   57 | `			if( zIn[0] >= 0xc0 ){` |
|        49 |   58 | `				zIn++;` |
|         - |   59 | `				/* UTF-8 stream */` |
|       109 |   60 | `				while( zIn < pStream->zEnd && ((zIn[0] & 0xc0) == 0x80) ){` |
|        61 |   61 | `					zIn++;` |
|         1 |   62 | `				}` |
|        24 |   63 | `			}` |
|         - |   64 | `			/* Skip alphanumeric stream */` |
| 368454579 |   65 | `			while( zIn < pStream->zEnd && zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_') ){` |
| 231025821 |   66 | `				zIn++;` |
|         5 |   67 | `			}` |
|  91619177 |   68 | `			if( zIn == pStream->zText ){` |
|         - |   69 | `				/* Not an UTF-8 or alphanumeric stream */` |
|  48646211 |   70 | `				break;` |
|         - |   71 | `			}` |
|         - |   72 | `			/* Synchronize pointers */` |
|  42972971 |   73 | `			pStream->zText = zIn;` |
|         5 |   74 | `		}` |
|         - |   75 | `		/* Record token length */` |
|  48646211 |   76 | `		pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  48646211 |   77 | `		nKeyword = KeywordCode(pStr->zString,(int)pStr->nByte);` |
|         - |   78 | `		/* PHP 7.4: 'fn' is a keyword reserved for arrow functions.` |
|         - |   79 | `		 * The auto-generated perfect hash above doesn't know about it,` |
|         - |   80 | `		 * so intercept the 'fn' identifier here.` |
|         - |   81 | `		 */` |
|  48646206 |   82 | `		if( nKeyword == PH7_TK_ID && pStr->nByte == 2` |
|  16714849 |   83 | `			&& pStr->zString[0] == 'f' && pStr->zString[1] == 'n' ){` |
|       597 |   84 | `			nKeyword = PH7_TKWRD_FN;` |
|       296 |   85 | `		}` |
|  48646211 |   86 | `		if( nKeyword != PH7_TK_ID ){` |
|  16298657 |   87 | `			if( nKeyword &` |
|         - |   88 | `				(PH7_TKWRD_NEW\|PH7_TKWRD_CLONE\|PH7_TKWRD_AND\|PH7_TKWRD_XOR\|PH7_TKWRD_OR\|PH7_TKWRD_INSTANCEOF) ){` |
|         - |   89 | `					/* Alpha stream operators [i.e: new,clone,and,instanceof,or,xor],save the operator instance for later processing */` |
|    857039 |   90 | `					pToken->pUserData = (void *)PH7_ExprExtractOperator(pStr,0);` |
|         - |   91 | `					/* Mark as an operator */` |
|    857039 |   92 | `					pToken->nType = PH7_TK_ID\|PH7_TK_OP;` |
|    428522 |   93 | `			}else{` |
|         - |   94 | `				/* We are dealing with a keyword [i.e: while,foreach,class...],save the keyword ID */` |
|  15441623 |   95 | `				pToken->nType = PH7_TK_KEYWORD;` |
|  15441623 |   96 | `				pToken->pUserData = SX_INT_TO_PTR(nKeyword);` |
|         - |   97 | `			}` |
|   8149331 |   98 | `		}else{` |
|         - |   99 | `			/* A simple identifier */` |
|  32347559 |  100 | `			pToken->nType = PH7_TK_ID;` |
|         - |  101 | `		}` |
|  24323108 |  102 | `	}else{` |
|         - |  103 | `		sxi32 c;` |
|         - |  104 | `		/* Non-alpha stream */` |
|  95153667 |  105 | `		if( pStream->zText[0] == '#' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '[' ){` |
|      7989 |  106 | `			sxu32 nDepth = 1;` |
|         - |  107 | `			/* PHP 8 attribute group '#[ ... ]': skip the whole balanced group as` |
|         - |  108 | `			 * trivia (attributes are not stored yet). Brackets inside string` |
|         - |  109 | `			 * literals and comments must not affect the depth count. An` |
|         - |  110 | `			 * unterminated group is silently consumed up to EOF, consistent` |
|         - |  111 | `			 * with unterminated block comments below.` |
|         - |  112 | `			 */` |
|         - |  113 | `			const unsigned char *zGroupStart;` |
|      7989 |  114 | `			pStream->zText += 2;` |
|      7989 |  115 | `			zGroupStart = pStream->zText;` |
|    652929 |  116 | `			while( pStream->zText < pStream->zEnd && nDepth > 0 ){` |
|    644945 |  117 | `				sxi32 d = pStream->zText[0];` |
|    644945 |  118 | `				if( d == '[' ){` |
|        11 |  119 | `					nDepth++;` |
|    644940 |  120 | `				}else if( d == ']' ){` |
|      7999 |  121 | `					nDepth--;` |
|    640938 |  122 | `				}else if( d == '\'' \|\| d == '"' ){` |
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
|    636921 |  145 | `				}else if( d == '#' \|\| (d == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '/') ){` |
|         - |  146 | `					/* Inline comment inside the group */` |
|       ! 0 |  147 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] != '\n' ){` |
|       ! 0 |  148 | `						pStream->zText++;` |
|       ! 0 |  149 | `					}` |
|       ! 0 |  150 | `					continue; /* Let the outer loop count the newline */` |
|    636901 |  151 | `				}else if( d == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '*' ){` |
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
|    636901 |  165 | `				}else if( d == '\n' ){` |
|         7 |  166 | `					pStream->nLine++;` |
|         3 |  167 | `				}` |
|    644945 |  168 | `				pStream->zText++;` |
|         5 |  169 | `			}` |
|      7989 |  170 | `			if( pUserData && pStream->pSet ){` |
|         - |  171 | `				/* Record the group's inner span (between #[ and its balanced ])` |
|         - |  172 | `				 * in the trivia sidecar, keyed like doc-comments. */` |
|         - |  173 | `				ph7_trivia sTrivia;` |
|      7989 |  174 | `				const unsigned char *zGroupEnd = pStream->zText;` |
|      7989 |  175 | `				if( nDepth == 0 && zGroupEnd > zGroupStart ){` |
|      7989 |  176 | `					zGroupEnd--; /* Exclude the closing ']' */` |
|      3992 |  177 | `				}` |
|      7989 |  178 | `				sTrivia.nTokIdx = SySetUsed(pStream->pSet);` |
|      7989 |  179 | `				sTrivia.iKind = PH7_TRIVIA_ATTR;` |
|      7989 |  180 | `				SyStringInitFromBuf(&sTrivia.sText,(const char *)zGroupStart,(sxu32)(zGroupEnd - zGroupStart));` |
|      7989 |  181 | `				sTrivia.nLine = pToken->nLine;` |
|      7989 |  182 | `				SySetPut((SySet *)pUserData,(const void *)&sTrivia);` |
|      3992 |  183 | `			}` |
|         - |  184 | `			/* Tell the upper-layer to ignore this token */` |
|      7989 |  185 | `			return SXERR_CONTINUE;` |
|  95217705 |  186 | `		}else if( pStream->zText[0] == '#' \|\|` |
|  95145672 |  187 | `			( pStream->zText[0] == '/' &&  &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '/') ){` |
|      6637 |  188 | `				pStream->zText++;` |
|         - |  189 | `				/* Inline comments */` |
|    271499 |  190 | `				while( pStream->zText < pStream->zEnd && pStream->zText[0] != '\n' ){` |
|    264867 |  191 | `					pStream->zText++;` |
|         5 |  192 | `				}` |
|         - |  193 | `				/* Tell the upper-layer to ignore this token */` |
|      6637 |  194 | `				return SXERR_CONTINUE;` |
|  95139051 |  195 | `		}else if( pStream->zText[0] == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '*' ){` |
|         - |  196 | `			/* A doc-comment starts with slash-star-star followed by more` |
|         - |  197 | `			 * content (slash-star-star-slash is the empty comment, not a` |
|         - |  198 | `			 * docblock). Its full span, delimiters included, goes to the` |
|         - |  199 | `			 * trivia sidecar when the caller supplied one — keyed by the` |
|         - |  200 | `			 * index the NEXT real token receives — and never enters the` |
|         - |  201 | `			 * token stream. */` |
|    137327 |  202 | `			const unsigned char *zDocStart = pStream->zText;` |
|    137339 |  203 | `			int bDoc = ( &pStream->zText[2] < pStream->zEnd && pStream->zText[2] == '*'` |
|    205995 |  204 | `			 && ( &pStream->zText[3] >= pStream->zEnd \|\| pStream->zText[3] != '/' ) );` |
|    137327 |  205 | `			pStream->zText += 2;` |
|         - |  206 | `			/* Block comment */` |
|   6872121 |  207 | `			while( pStream->zText < pStream->zEnd ){` |
|   6872121 |  208 | `				if( pStream->zText[0] == '*' ){` |
|    161015 |  209 | `					if( &pStream->zText[1] >= pStream->zEnd \|\| pStream->zText[1] == '/'  ){` |
|     68666 |  210 | `						break;` |
|         - |  211 | `					}` |
|     11844 |  212 | `				}` |
|   6734799 |  213 | `				if( pStream->zText[0] == '\n' ){` |
|       165 |  214 | `					pStream->nLine++;` |
|        80 |  215 | `				}` |
|   6734799 |  216 | `				pStream->zText++;` |
|         5 |  217 | `			}` |
|    137327 |  218 | `			pStream->zText += 2;` |
|    137327 |  219 | `			if( bDoc && pUserData && pStream->pSet ){` |
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
|    137327 |  232 | `			return SXERR_CONTINUE;` |
|  95001729 |  233 | `		}else if( SyisDigit(pStream->zText[0]) ){` |
|   2780803 |  234 | `			pStream->zText++;` |
|         - |  235 | `			/* PHP 7.4: handle underscore separator immediately following the first digit.` |
|         - |  236 | `			 * Check pStream->zText < pStream->zEnd BEFORE forming pStream->zText + 1 so` |
|         - |  237 | `			 * we never compute a pointer past one-past-end. */` |
|   2780798 |  238 | `			if( pStream->zText < pStream->zEnd` |
|   2780798 |  239 | `				&& pStream->zText[0] == '_'` |
|   1390479 |  240 | `				&& pStream->zText + 1 < pStream->zEnd` |
|       160 |  241 | `				&& pStream->zText[1] < 0xc0` |
|       165 |  242 | `				&& SyisDigit(pStream->zText[1]) ){` |
|       151 |  243 | `				pStream->zText++; /* swallow underscore between two digits */` |
|        75 |  244 | `			}` |
|         - |  245 | `			/* Decimal digit stream (PHP 7.4: underscore separator allowed between two digits) */` |
|   3386395 |  246 | `			while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|    605597 |  247 | `				pStream->zText++;` |
|    605592 |  248 | `				if( pStream->zText < pStream->zEnd` |
|    605592 |  249 | `					&& pStream->zText[0] == '_'` |
|    302882 |  250 | `					&& pStream->zText + 1 < pStream->zEnd` |
|       172 |  251 | `					&& pStream->zText[1] < 0xc0` |
|       177 |  252 | `					&& SyisDigit(pStream->zText[1]) ){` |
|       173 |  253 | `					pStream->zText++; /* swallow underscore between two digits */` |
|        86 |  254 | `				}` |
|         5 |  255 | `			}` |
|         - |  256 | `			/* Mark the token as integer until we encounter a real number */` |
|   2780803 |  257 | `			pToken->nType = PH7_TK_INTEGER;` |
|   2780803 |  258 | `			if( pStream->zText < pStream->zEnd ){` |
|   2780803 |  259 | `				c = pStream->zText[0];` |
|   2780803 |  260 | `				if( c == '.' ){` |
|         - |  261 | `					/* Real number (PHP 7.4: underscore separator allowed between two digits) */` |
|       829 |  262 | `					pStream->zText++;` |
|      3093 |  263 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|      2269 |  264 | `						pStream->zText++;` |
|      2264 |  265 | `						if( pStream->zText < pStream->zEnd` |
|      2264 |  266 | `							&& pStream->zText[0] == '_'` |
|      1138 |  267 | `							&& pStream->zText + 1 < pStream->zEnd` |
|        12 |  268 | `							&& pStream->zText[1] < 0xc0` |
|        17 |  269 | `							&& SyisDigit(pStream->zText[1]) ){` |
|        13 |  270 | `							pStream->zText++;` |
|         6 |  271 | `						}` |
|         5 |  272 | `					}` |
|       829 |  273 | `					if( pStream->zText < pStream->zEnd ){` |
|       829 |  274 | `						c = pStream->zText[0];` |
|       829 |  275 | `						if( c=='e' \|\| c=='E' ){` |
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
|       412 |  295 | `					}` |
|       829 |  296 | `					pToken->nType = PH7_TK_REAL;` |
|   2780391 |  297 | `				}else if( c=='e' \|\| c=='E' ){` |
|        52 |  298 | `					SXUNUSED(pUserData); /* Prevent compiler warning */` |
|        52 |  299 | `					SXUNUSED(pCtxData);` |
|       105 |  300 | `					pStream->zText++;` |
|       105 |  301 | `					if( pStream->zText < pStream->zEnd ){` |
|       105 |  302 | `						c = pStream->zText[0];` |
|       104 |  303 | `						if( (c =='+' \|\| c=='-') && &pStream->zText[1] < pStream->zEnd  &&` |
|        31 |  304 | `							pStream->zText[1] < 0xc0 && SyisDigit(pStream->zText[1]) ){` |
|        31 |  305 | `								pStream->zText++;` |
|        15 |  306 | `						}` |
|       325 |  307 | `						while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|       221 |  308 | `							pStream->zText++;` |
|       220 |  309 | `							if( pStream->zText < pStream->zEnd` |
|       220 |  310 | `								&& pStream->zText[0] == '_'` |
|       112 |  311 | `								&& pStream->zText + 1 < pStream->zEnd` |
|         4 |  312 | `								&& pStream->zText[1] < 0xc0` |
|         5 |  313 | `								&& SyisDigit(pStream->zText[1]) ){` |
|         5 |  314 | `								pStream->zText++;` |
|         2 |  315 | `							}` |
|         1 |  316 | `						}` |
|        52 |  317 | `					}` |
|       105 |  318 | `					pToken->nType = PH7_TK_REAL;` |
|   2779927 |  319 | `				}else if( c == 'x' \|\| c == 'X' ){` |
|         - |  320 | `					/* Hex digit stream (PHP 7.4: underscore separator allowed between two digits) */` |
|        83 |  321 | `					pStream->zText++;` |
|       507 |  322 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisHex(pStream->zText[0]) ){` |
|       425 |  323 | `						pStream->zText++;` |
|       424 |  324 | `						if( pStream->zText < pStream->zEnd` |
|       424 |  325 | `							&& pStream->zText[0] == '_'` |
|       236 |  326 | `							&& pStream->zText + 1 < pStream->zEnd` |
|        48 |  327 | `							&& pStream->zText[1] < 0xc0` |
|        49 |  328 | `							&& SyisHex(pStream->zText[1]) ){` |
|        49 |  329 | `							pStream->zText++;` |
|        24 |  330 | `						}` |
|         1 |  331 | `					}` |
|   2779833 |  332 | `				}else if(c  == 'b' \|\| c == 'B' ){` |
|         - |  333 | `					/* Binary digit stream (PHP 7.4: underscore separator allowed between two digits) */` |
|       284 |  334 | `					pStream->zText++;` |
|      3089 |  335 | `					while( pStream->zText < pStream->zEnd && (pStream->zText[0] == '0' \|\| pStream->zText[0] == '1') ){` |
|      1777 |  336 | `						pStream->zText++;` |
|      1776 |  337 | `						if( pStream->zText < pStream->zEnd` |
|      1776 |  338 | `							&& pStream->zText[0] == '_'` |
|       957 |  339 | `							&& pStream->zText + 1 < pStream->zEnd` |
|       139 |  340 | `							&& (pStream->zText[1] == '0' \|\| pStream->zText[1] == '1') ){` |
|       139 |  341 | `							pStream->zText++;` |
|        69 |  342 | `						}` |
|         1 |  343 | `					}` |
|       141 |  344 | `				}` |
|   1390399 |  345 | `			}` |
|         - |  346 | `			/* PHP 7.4: absorb a trailing malformed underscore run into the` |
|         - |  347 | `			 * numeric token so the compile phase can emit a PHP-compatible` |
|         - |  348 | `			 * "syntax error, unexpected identifier" parse error. Valid` |
|         - |  349 | `			 * separators were already consumed by the per-loop peek logic` |
|         - |  350 | `			 * above, so an underscore here is always misplaced. */` |
|   2780803 |  351 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '_' ){` |
|        18 |  352 | `				pStream->zText++;` |
|        44 |  353 | `				while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0` |
|        49 |  354 | `					&& (SyisAlphaNum(pStream->zText[0]) \|\| pStream->zText[0] == '_') ){` |
|        20 |  355 | `					pStream->zText++;` |
|         4 |  356 | `				}` |
|         7 |  357 | `			}` |
|         - |  358 | `			/* Record token length */` |
|   2780803 |  359 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|   2780803 |  360 | `			return SXRET_OK;` |
|         - |  361 | `		}` |
|  92220931 |  362 | `		c = pStream->zText[0];` |
|  92220931 |  363 | `		pStream->zText++; /* Advance the stream cursor */` |
|         - |  364 | `		/* Assume we are dealing with an operator*/` |
|  92220931 |  365 | `		pToken->nType = PH7_TK_OP;` |
|  92220931 |  366 | `		switch(c){` |
|  18592061 |  367 | `		case '$': pToken->nType = PH7_TK_DOLLAR; break;` |
|   5776409 |  368 | `		case '{': pToken->nType = PH7_TK_OCB;    break;` |
|   5776395 |  369 | `		case '}': pToken->nType = PH7_TK_CCB;    break;` |
|  11888225 |  370 | `		case '(': pToken->nType = PH7_TK_LPAREN; break;` |
|   2797201 |  371 | `		case '[': pToken->nType \|= PH7_TK_OSB;   break; /* Bitwise operation here,since the square bracket token '['` |
|         - |  372 | `														 * is a potential operator [i.e: subscripting] */` |
|   2797207 |  373 | `		case ']': pToken->nType = PH7_TK_CSB;    break;` |
|   5944098 |  374 | `		case ')': {` |
|  11888201 |  375 | `			SySet *pTokSet = pStream->pSet;` |
|         - |  376 | `			/* Assemble type cast operators [i.e: (int),(float),(bool)...] */` |
|  11888201 |  377 | `			if( pTokSet->nUsed >= 2 ){` |
|         - |  378 | `				SyToken *pTmp;` |
|         - |  379 | `				/* Peek the last recongnized token */` |
|  11888199 |  380 | `				pTmp = (SyToken *)SySetPeek(pTokSet);` |
|  11888199 |  381 | `				if( pTmp->nType & PH7_TK_KEYWORD ){` |
|    776129 |  382 | `					sxi32 nID = SX_PTR_TO_INT(pTmp->pUserData);` |
|    776129 |  383 | `					if( (sxu32)nID & (PH7_TKWRD_ARRAY\|PH7_TKWRD_INT\|PH7_TKWRD_FLOAT\|PH7_TKWRD_STRING\|PH7_TKWRD_OBJECT\|PH7_TKWRD_BOOL\|PH7_TKWRD_UNSET) ){` |
|    662287 |  384 | `						pTmp = (SyToken *)SySetAt(pTokSet,pTokSet->nUsed - 2);` |
|    662287 |  385 | `						if( pTmp->nType & PH7_TK_LPAREN ){` |
|         - |  386 | `							/* Merge the three tokens '(' 'TYPE' ')' into a single one */` |
|    388083 |  387 | `							const char * zTypeCast = "(int)";` |
|    388083 |  388 | `							if( nID & PH7_TKWRD_FLOAT ){` |
|      3943 |  389 | `								zTypeCast = "(float)";` |
|    386114 |  390 | `							}else if( nID & PH7_TKWRD_BOOL ){` |
|      3957 |  391 | `								zTypeCast = "(bool)";` |
|    382169 |  392 | `							}else if( nID & PH7_TKWRD_STRING ){` |
|    137131 |  393 | `								zTypeCast = "(string)";` |
|    311630 |  394 | `							}else if( nID & PH7_TKWRD_ARRAY ){` |
|        29 |  395 | `								zTypeCast = "(array)";` |
|    243053 |  396 | `							}else if( nID & PH7_TKWRD_OBJECT ){` |
|        35 |  397 | `								zTypeCast = "(object)";` |
|    243022 |  398 | `							}else if( nID & PH7_TKWRD_UNSET ){` |
|         3 |  399 | `								zTypeCast = "(unset)";` |
|         1 |  400 | `							}` |
|         - |  401 | `							/* Reflect the change */` |
|    388083 |  402 | `							pToken->nType = PH7_TK_OP;` |
|    388083 |  403 | `							SyStringInitFromBuf(&pToken->sData,zTypeCast,SyStrlen(zTypeCast));` |
|         - |  404 | `							/* Save the instance associated with the type cast operator */` |
|    388083 |  405 | `							pToken->pUserData = (void *)PH7_ExprExtractOperator(&pToken->sData,0);` |
|         - |  406 | `							/* Remove the two previous tokens */` |
|    388083 |  407 | `							pTokSet->nUsed -= 2;` |
|    388083 |  408 | `							return SXRET_OK;` |
|         - |  409 | `						}` |
|    137102 |  410 | `					}` |
|    194023 |  411 | `				}` |
|   5750058 |  412 | `			}` |
|  11500123 |  413 | `			pToken->nType = PH7_TK_RPAREN;` |
|  11500123 |  414 | `			break;` |
|         - |  415 | `				  }` |
|   2209353 |  416 | `		case '\'':{` |
|         - |  417 | `			/* Single quoted string */` |
|   4418711 |  418 | `			pStr->zString++;` |
|  54974415 |  419 | `			while( pStream->zText < pStream->zEnd ){` |
|  54974415 |  420 | `				if( pStream->zText[0] == '\''  ){` |
|   4418721 |  421 | `					if( pStream->zText[-1] != '\\' ){` |
|   4387365 |  422 | `						break;` |
|       ! 0 |  423 | `					}else{` |
|     31361 |  424 | `						const unsigned char *zPtr = &pStream->zText[-2];` |
|     31361 |  425 | `						sxi32 i = 1;` |
|     62711 |  426 | `						while( zPtr > pStream->zInput && zPtr[0] == '\\' ){` |
|     31355 |  427 | `							zPtr--;` |
|     31355 |  428 | `							i++;` |
|         5 |  429 | `						}` |
|     31361 |  430 | `						if((i&1)==0){` |
|     31351 |  431 | `							break;` |
|         - |  432 | `						}` |
|         - |  433 | `					}` |
|         5 |  434 | `				}` |
|  50555709 |  435 | `				if( pStream->zText[0] == '\n' ){` |
|        67 |  436 | `					pStream->nLine++;` |
|        33 |  437 | `				}` |
|  50555709 |  438 | `				pStream->zText++;` |
|         5 |  439 | `			}` |
|         - |  440 | `			/* Record token length and type */` |
|   4418711 |  441 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|   4418711 |  442 | `			pToken->nType = PH7_TK_SSTR;` |
|         - |  443 | `			/* Jump the trailing single quote */` |
|   4418711 |  444 | `			pStream->zText++;` |
|   4418711 |  445 | `			return SXRET_OK;` |
|         - |  446 | `				  }` |
|     40897 |  447 | `		case '"':{` |
|         - |  448 | `			sxi32 iNest;` |
|         - |  449 | `			/* Double quoted string */` |
|     81799 |  450 | `			pStr->zString++;` |
|   1619363 |  451 | `			while( pStream->zText < pStream->zEnd ){` |
|   1619363 |  452 | `				if( pStream->zText[0] == '{' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '$'){` |
|       121 |  453 | `					iNest = 1;` |
|       121 |  454 | `					pStream->zText++;` |
|         - |  455 | `					/* TICKET 1433-40: Hnadle braces'{}' in double quoted string where everything is allowed */` |
|      1225 |  456 | `					while(pStream->zText < pStream->zEnd ){` |
|      1225 |  457 | `						if( pStream->zText[0] == '{' ){` |
|         9 |  458 | `							iNest++;` |
|      1221 |  459 | `						}else if (pStream->zText[0] == '}' ){` |
|       129 |  460 | `							iNest--;` |
|       129 |  461 | `							if( iNest <= 0 ){` |
|       121 |  462 | `								pStream->zText++;` |
|       121 |  463 | `								break;` |
|         1 |  464 | `							}` |
|      1095 |  465 | `						}else if( pStream->zText[0] == '\n' ){` |
|       ! 0 |  466 | `							pStream->nLine++;` |
|       ! 0 |  467 | `						}` |
|      1107 |  468 | `						pStream->zText++;` |
|         3 |  469 | `					}` |
|       121 |  470 | `					if( pStream->zText >= pStream->zEnd ){` |
|       ! 0 |  471 | `						break;` |
|         - |  472 | `					}` |
|        59 |  473 | `				}` |
|   1619363 |  474 | `				if( pStream->zText[0] == '"' ){` |
|     82077 |  475 | `					if( pStream->zText[-1] != '\\' ){` |
|     81793 |  476 | `						break;` |
|       ! 0 |  477 | `					}else{` |
|       289 |  478 | `						const unsigned char *zPtr = &pStream->zText[-2];` |
|       289 |  479 | `						sxi32 i = 1;` |
|       327 |  480 | `						while( zPtr > pStream->zInput && zPtr[0] == '\\' ){` |
|        41 |  481 | `							zPtr--;` |
|        41 |  482 | `							i++;` |
|         3 |  483 | `						}` |
|       289 |  484 | `						if((i&1)==0){` |
|         7 |  485 | `							break;` |
|         - |  486 | `						}` |
|         - |  487 | `					}` |
|       139 |  488 | `				}` |
|   1537569 |  489 | `				if( pStream->zText[0] == '\n' ){` |
|        29 |  490 | `					pStream->nLine++;` |
|        14 |  491 | `				}` |
|   1537569 |  492 | `				pStream->zText++;` |
|         5 |  493 | `			}` |
|         - |  494 | `			/* Record token length and type */` |
|     81799 |  495 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|     81799 |  496 | `			pToken->nType = PH7_TK_DSTR;` |
|         - |  497 | `			/* Jump the trailing quote */` |
|     81799 |  498 | `			pStream->zText++;` |
|     81799 |  499 | `			return SXRET_OK;` |
|         - |  500 | `				  }` |
|         2 |  501 | ``		case '`':{`` |
|         - |  502 | `			/* Backtick quoted string */` |
|         6 |  503 | `			pStr->zString++;` |
|        46 |  504 | `			while( pStream->zText < pStream->zEnd ){` |
|        46 |  505 | ``				if( pStream->zText[0] == '`' && pStream->zText[-1] != '\\' ){`` |
|         6 |  506 | `					break;` |
|         - |  507 | `				}` |
|        42 |  508 | `				if( pStream->zText[0] == '\n' ){` |
|       ! 0 |  509 | `					pStream->nLine++;` |
|       ! 0 |  510 | `				}` |
|        42 |  511 | `				pStream->zText++;` |
|         2 |  512 | `			}` |
|         - |  513 | `			/* Record token length and type */` |
|         6 |  514 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|         6 |  515 | `			pToken->nType = PH7_TK_BSTR;` |
|         - |  516 | `			/* Jump the trailing backtick */` |
|         6 |  517 | `			pStream->zText++;` |
|         6 |  518 | `			return SXRET_OK;` |
|         - |  519 | `				  }` |
|      8705 |  520 | `		case '\\': pToken->nType = PH7_TK_NSSEP;  break;` |
|    382549 |  521 | `		case ':':` |
|    765103 |  522 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == ':' ){` |
|         - |  523 | `				/* Current operator: '::' */` |
|    368999 |  524 | `				pStream->zText++;` |
|    184502 |  525 | `			}else{` |
|    396109 |  526 | `				pToken->nType = PH7_TK_COLON; /* Single colon */` |
|         - |  527 | `			}` |
|    765103 |  528 | `			break;` |
|   2815823 |  529 | `		case ',': pToken->nType \|= PH7_TK_COMMA;  break; /* Comma is also an operator */` |
|   9097321 |  530 | `		case ';': pToken->nType = PH7_TK_SEMI;    break;` |
|         - |  531 | `			/* Handle combined operators [i.e: +=,===,!=== ...] */` |
|   3108132 |  532 | `		case '=':` |
|   6216269 |  533 | `			pToken->nType \|= PH7_TK_EQUAL;` |
|   6216269 |  534 | `			if( pStream->zText < pStream->zEnd ){` |
|   6216269 |  535 | `				if( pStream->zText[0] == '=' ){` |
|   1047339 |  536 | `					pToken->nType &= ~PH7_TK_EQUAL;` |
|         - |  537 | `					/* Current operator: == */` |
|   1047339 |  538 | `					pStream->zText++;` |
|   1047339 |  539 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  540 | `						/* Current operator: === */` |
|   1027379 |  541 | `						pStream->zText++;` |
|    513692 |  542 | `					}` |
|   5692602 |  543 | `				}else if( pStream->zText[0] == '>' ){` |
|         - |  544 | `					/* Array operator: => */` |
|    406171 |  545 | `					pToken->nType = PH7_TK_ARRAY_OP;` |
|    406171 |  546 | `					pStream->zText++;` |
|    203088 |  547 | `				}else{` |
|         - |  548 | `					/* TICKET 1433-0010: Reference operator '=&' */` |
|   4762769 |  549 | `					const unsigned char *zCur = pStream->zText;` |
|   4762769 |  550 | `					sxu32 nLine = 0;` |
|   9525361 |  551 | `					while( zCur < pStream->zEnd && zCur[0] < 0xc0 && SyisSpace(zCur[0]) ){` |
|   4762597 |  552 | `						if( zCur[0] == '\n' ){` |
|         5 |  553 | `							nLine++;` |
|         2 |  554 | `						}` |
|   4762597 |  555 | `						zCur++;` |
|         5 |  556 | `					}` |
|   4762769 |  557 | `					if( zCur < pStream->zEnd && zCur[0] == '&' ){` |
|         - |  558 | `						/* Current operator: =& */` |
|        66 |  559 | `						pToken->nType &= ~PH7_TK_EQUAL;` |
|        66 |  560 | `						SyStringInitFromBuf(pStr,"=&",sizeof("=&")-1);` |
|         - |  561 | `						/* Update token stream */` |
|        66 |  562 | `						pStream->zText = &zCur[1];` |
|        66 |  563 | `						pStream->nLine += nLine;` |
|        31 |  564 | `					}` |
|         - |  565 | `				}` |
|   3108132 |  566 | `			}` |
|   6216269 |  567 | `			break;` |
|    374416 |  568 | `		case '!':` |
|    748837 |  569 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  570 | `				/* Current operator: != */` |
|    364409 |  571 | `				pStream->zText++;` |
|    364409 |  572 | `				if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  573 | `					/* Current operator: !== */` |
|    352627 |  574 | `					pStream->zText++;` |
|    176311 |  575 | `				}` |
|    182202 |  576 | `			}` |
|    748837 |  577 | `			break;` |
|    254923 |  578 | `		case '&':` |
|    509851 |  579 | `			pToken->nType \|= PH7_TK_AMPER;` |
|    509851 |  580 | `			if( pStream->zText < pStream->zEnd ){` |
|    509851 |  581 | `				if( pStream->zText[0] == '&' ){` |
|    321629 |  582 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|         - |  583 | `					/* Current operator: && */` |
|    321629 |  584 | `					pStream->zText++;` |
|    349039 |  585 | `				}else if( pStream->zText[0] == '=' ){` |
|         7 |  586 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|         - |  587 | `					/* Current operator: &= */` |
|         7 |  588 | `					pStream->zText++;` |
|         3 |  589 | `				}` |
|    254923 |  590 | `			}` |
|    509851 |  591 | `			break;` |
|    147056 |  592 | `		case '\|':` |
|    294117 |  593 | `			if( pStream->zText < pStream->zEnd ){` |
|    294117 |  594 | `				if( pStream->zText[0] == '\|' ){` |
|         - |  595 | `					/* Current operator: \|\| */` |
|    219467 |  596 | `					pStream->zText++;` |
|    184386 |  597 | `				}else if( pStream->zText[0] == '=' ){` |
|         - |  598 | `					/* Current operator: \|= */` |
|     58751 |  599 | `					pStream->zText++;` |
|     45282 |  600 | `				}else if( pStream->zText[0] == '>' ){` |
|         - |  601 | `					/* Current operator: \|> (PHP 8.5 pipe) */` |
|        27 |  602 | `					pStream->zText++;` |
|        13 |  603 | `				}` |
|    147056 |  604 | `			}` |
|    294117 |  605 | `			break;` |
|    174857 |  606 | `		case '+':` |
|    349719 |  607 | `			if( pStream->zText < pStream->zEnd ){` |
|    349717 |  608 | `				if( pStream->zText[0] == '+' ){` |
|         - |  609 | `					/* Current operator: ++ */` |
|    133599 |  610 | `					pStream->zText++;` |
|    282920 |  611 | `				}else if( pStream->zText[0] == '=' ){` |
|         - |  612 | `					/* Current operator: += */` |
|     43153 |  613 | `					pStream->zText++;` |
|     21574 |  614 | `				}` |
|    174856 |  615 | `			}` |
|    349719 |  616 | `			break;` |
|   2407854 |  617 | `		case '-':` |
|   4815713 |  618 | `			if( pStream->zText < pStream->zEnd ){` |
|   4815713 |  619 | `				if( pStream->zText[0] == '-' ){` |
|         - |  620 | `					/* Current operator: -- */` |
|     15707 |  621 | `					pStream->zText++;` |
|   4807862 |  622 | `				}else if( pStream->zText[0] == '=' ){` |
|         - |  623 | `					/* Current operator: -= */` |
|        12 |  624 | `					pStream->zText++;` |
|   4800006 |  625 | `				}else if( pStream->zText[0] == '>' ){` |
|         - |  626 | `					/* Current operator: -> */` |
|   4642501 |  627 | `					pStream->zText++;` |
|   2321248 |  628 | `				}` |
|   2407854 |  629 | `			}` |
|   4815713 |  630 | `			break;` |
|     17834 |  631 | `		case '*':` |
|     35673 |  632 | `			if( pStream->zText < pStream->zEnd ){` |
|     35673 |  633 | `				if( pStream->zText[0] == '*' ){` |
|         - |  634 | `					/* Current operator: ** or **= */` |
|       135 |  635 | `					pStream->zText++;` |
|       135 |  636 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  637 | `						/* Current operator: **= */` |
|        23 |  638 | `						pStream->zText++;` |
|        12 |  639 | `					}` |
|     35606 |  640 | `				}else if( pStream->zText[0] == '=' ){` |
|         - |  641 | `					/* Current operator: *= */` |
|        31 |  642 | `					pStream->zText++;` |
|        14 |  643 | `				}` |
|     17834 |  644 | `			}` |
|     35673 |  645 | `			break;` |
|        48 |  646 | `		case '/':` |
|        98 |  647 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  648 | `				/* Current operator: /= */` |
|         7 |  649 | `				pStream->zText++;` |
|         3 |  650 | `			}` |
|        98 |  651 | `			break;` |
|      2003 |  652 | `		case '%':` |
|      4011 |  653 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  654 | `				/* Current operator: %= */` |
|         9 |  655 | `				pStream->zText++;` |
|         4 |  656 | `			}` |
|      4011 |  657 | `			break;` |
|        11 |  658 | `		case '^':` |
|        23 |  659 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  660 | `				/* Current operator: ^= */` |
|         9 |  661 | `				pStream->zText++;` |
|         4 |  662 | `			}` |
|        23 |  663 | `			break;` |
|    838547 |  664 | `		case '.':` |
|   1677099 |  665 | `			if( pStream->zText + 1 < pStream->zEnd && pStream->zText[0] == '.' && pStream->zText[1] == '.' ){` |
|         - |  666 | `				/* Ellipsis: ... */` |
|     23939 |  667 | `				pStream->zText += 2;` |
|     23939 |  668 | `				pToken->nType = PH7_TK_ELLIPSIS;` |
|   1665132 |  669 | `			}else if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  670 | `				/* Current operator: .= */` |
|    321257 |  671 | `				pStream->zText++;` |
|    160626 |  672 | `			}` |
|   1677099 |  673 | `			break;` |
|    127603 |  674 | `		case '<':` |
|    255211 |  675 | `			if( pStream->zText < pStream->zEnd ){` |
|    255211 |  676 | `				if( pStream->zText[0] == '<' ){` |
|         - |  677 | `					/* Current operator: << */` |
|       145 |  678 | `					pStream->zText++;` |
|       145 |  679 | `					if( pStream->zText < pStream->zEnd ){` |
|       145 |  680 | `						if( pStream->zText[0] == '=' ){` |
|         - |  681 | `							/* Current operator: <<= */` |
|         9 |  682 | `							pStream->zText++;` |
|       141 |  683 | `						}else if( pStream->zText[0] == '<' ){` |
|         - |  684 | `							/* Current Token: <<<  */` |
|       123 |  685 | `							pStream->zText++;` |
|         - |  686 | `							/* This may be the beginning of a Heredoc/Nowdoc string,try to delimit it */` |
|       123 |  687 | `							rc = LexExtractHeredoc(&(*pStream),&(*pToken));` |
|       123 |  688 | `							if( rc == SXRET_OK ){` |
|         - |  689 | `								/* Here/Now doc successfuly extracted */` |
|       123 |  690 | `								return SXRET_OK;` |
|         - |  691 | `							}` |
|       ! 0 |  692 | `						}` |
|        12 |  693 | `					}` |
|    255082 |  694 | `				}else if( pStream->zText[0] == '>' ){` |
|         - |  695 | `					/* Current operator: <> */` |
|         5 |  696 | `					pStream->zText++;` |
|    255069 |  697 | `				}else if( pStream->zText[0] == '=' ){` |
|         - |  698 | `					/* Current operator: <= or <=> */` |
|     19715 |  699 | `					pStream->zText++;` |
|     19715 |  700 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '>' ){` |
|         - |  701 | `						/* Current operator: <=> */` |
|     11821 |  702 | `						pStream->zText++;` |
|      5908 |  703 | `					}` |
|      9855 |  704 | `				}` |
|    127544 |  705 | `			}` |
|    255093 |  706 | `			break;` |
|     90219 |  707 | `		case '>':` |
|    180443 |  708 | `			if( pStream->zText < pStream->zEnd ){` |
|    180443 |  709 | `				if( pStream->zText[0] == '>' ){` |
|         - |  710 | `					/* Current operator: >> */` |
|      7857 |  711 | `					pStream->zText++;` |
|      7857 |  712 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  713 | `						/* Current operator: >>= */` |
|        11 |  714 | `						pStream->zText++;` |
|        10 |  715 | `					}` |
|    176517 |  716 | `				}else if( pStream->zText[0] == '=' ){` |
|         - |  717 | `					/* Current operator: >= */` |
|     62757 |  718 | `					pStream->zText++;` |
|     31376 |  719 | `				}` |
|     90219 |  720 | `			}` |
|    180443 |  721 | `			break;` |
|    215293 |  722 | `		case '?':` |
|    430591 |  723 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '?' ){` |
|         - |  724 | `				/* Null coalescing operator: ?? */` |
|     51155 |  725 | `				pStream->zText++;` |
|     51155 |  726 | `				if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  727 | `					/* Null coalescing assignment operator (PHP 7.4) */` |
|        99 |  728 | `					pStream->zText++;` |
|        47 |  729 | `				}` |
|    405016 |  730 | `			}else if( (pStream->zEnd - pStream->zText) >= 2` |
|    379441 |  731 | `				&& pStream->zText[0] == '-' && pStream->zText[1] == '>' ){` |
|         - |  732 | `				/* Nullsafe object operator (PHP 8.0): ?-> */` |
|       119 |  733 | `				pStream->zText += 2;` |
|        57 |  734 | `			}` |
|    430586 |  735 | `			break;` |
|       117 |  736 | `		default:` |
|       234 |  737 | `			break;` |
|         - |  738 | `		}` |
|  87332231 |  739 | `		if( pStr->nByte <= 0 ){` |
|         - |  740 | `			/* Record token length */` |
|  87332169 |  741 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  43666082 |  742 | `		}` |
|  87332231 |  743 | `		if( pToken->nType & PH7_TK_OP ){` |
|         - |  744 | `			const ph7_expr_op *pOp;` |
|         - |  745 | `			/* Check if the extracted token is an operator */` |
|  21069621 |  746 | `			pOp = PH7_ExprExtractOperator(pStr,(SyToken *)SySetPeek(pStream->pSet));` |
|  21069621 |  747 | `			if( pOp == 0 ){` |
|         - |  748 | `				/* Not an operator */` |
|       ! 0 |  749 | `				pToken->nType &= ~PH7_TK_OP;` |
|       ! 0 |  750 | `				if( pToken->nType <= 0 ){` |
|       ! 0 |  751 | `					pToken->nType = PH7_TK_OTHER;` |
|       ! 0 |  752 | `				}` |
|       ! 0 |  753 | `			}else{` |
|         - |  754 | `				/* Save the instance associated with this operator for later processing */` |
|  21069621 |  755 | `				pToken->pUserData = (void *)pOp;` |
|         - |  756 | `			}` |
|  10534808 |  757 | `		}` |
|         - |  758 | `	}` |
|         - |  759 | `	/* Tell the upper-layer to save the extracted token for later processing */` |
| 135978437 |  760 | `	return SXRET_OK;` |
|  71899939 |  761 | `}` |
|         - |  762 | `/* SPDX-SnippetBegin */` |
|         - |  763 | `/* SPDX-SnippetCopyrightText: SQLite mkkeywordhash.c (D. Richard Hipp and the SQLite authors <https://sqlite.org/>); adapted for the PH7 engine by Chems mrad */` |
|         - |  764 | `/* SPDX-License-Identifier: blessing */` |
|         - |  765 | `/***** This file contains automatically generated code ******` |
|         - |  766 | `**` |
|         - |  767 | `** The code in this file has been automatically generated by` |
|         - |  768 | `**` |
|         - |  769 | `**     $Header: /sqlite/sqlite/tool/mkkeywordhash.c` |
|         - |  770 | `**` |
|         - |  771 | `** Sligthly modified by Chems mrad <chm@symisc.net> for the PH7 engine.` |
|         - |  772 | `**` |
|         - |  773 | `** The code in this file implements a function that determines whether` |
|         - |  774 | `** or not a given identifier is really a PHP keyword.  The same thing` |
|         - |  775 | `** might be implemented more directly using a hand-written hash table.` |
|         - |  776 | `** But by using this automatically generated code, the size of the code` |
|         - |  777 | `** is substantially reduced.  This is important for embedded applications` |
|         - |  778 | `** on platforms with limited memory.` |
|         - |  779 | `*/` |
|         - |  780 | `/* Hash score: 103 */` |
|  48646211 |  781 | `static sxu32 KeywordCode(const char *z, int n){` |
|         - |  782 | `  /* zText[] encodes 532 bytes of keywords in 333 bytes */` |
|         - |  783 | `  /*   extendswitchprintegerequire_oncenddeclareturnamespacechobject      */` |
|         - |  784 | `  /*   hrowbooleandefaultrycaselfinalistaticlonewconstringlobaluse        */` |
|         - |  785 | `  /*   lseifloatvarrayANDIEchoUSECHOabstractclasscontinuendifunction      */` |
|         - |  786 | `  /*   diendwhilevaldoexitgotoimplementsinclude_oncemptyinstanceof        */` |
|         - |  787 | `  /*   interfacendforeachissetparentprivateprotectedpublicatchunset       */` |
|         - |  788 | `  /*   xorARRAYASArrayEXITUNSETXORbreak                                   */` |
|         - |  789 | `  static const char zText[332] = {` |
|         - |  790 | `    'e','x','t','e','n','d','s','w','i','t','c','h','p','r','i','n','t','e',` |
|         - |  791 | `    'g','e','r','e','q','u','i','r','e','_','o','n','c','e','n','d','d','e',` |
|         - |  792 | `    'c','l','a','r','e','t','u','r','n','a','m','e','s','p','a','c','e','c',` |
|         - |  793 | `    'h','o','b','j','e','c','t','h','r','o','w','b','o','o','l','e','a','n',` |
|         - |  794 | `    'd','e','f','a','u','l','t','r','y','c','a','s','e','l','f','i','n','a',` |
|         - |  795 | `    'l','i','s','t','a','t','i','c','l','o','n','e','w','c','o','n','s','t',` |
|         - |  796 | `    'r','i','n','g','l','o','b','a','l','u','s','e','l','s','e','i','f','l',` |
|         - |  797 | `    'o','a','t','v','a','r','r','a','y','A','N','D','I','E','c','h','o','U',` |
|         - |  798 | `    'S','E','C','H','O','a','b','s','t','r','a','c','t','c','l','a','s','s',` |
|         - |  799 | `    'c','o','n','t','i','n','u','e','n','d','i','f','u','n','c','t','i','o',` |
|         - |  800 | `    'n','d','i','e','n','d','w','h','i','l','e','v','a','l','d','o','e','x',` |
|         - |  801 | `    'i','t','g','o','t','o','i','m','p','l','e','m','e','n','t','s','i','n',` |
|         - |  802 | `    'c','l','u','d','e','_','o','n','c','e','m','p','t','y','i','n','s','t',` |
|         - |  803 | `    'a','n','c','e','o','f','i','n','t','e','r','f','a','c','e','n','d','f',` |
|         - |  804 | `    'o','r','e','a','c','h','i','s','s','e','t','p','a','r','e','n','t','p',` |
|         - |  805 | `    'r','i','v','a','t','e','p','r','o','t','e','c','t','e','d','p','u','b',` |
|         - |  806 | `    'l','i','c','a','t','c','h','u','n','s','e','t','x','o','r','A','R','R',` |
|         - |  807 | `    'A','Y','A','S','A','r','r','a','y','E','X','I','T','U','N','S','E','T',` |
|         - |  808 | `    'X','O','R','b','r','e','a','k'` |
|         - |  809 | `  };` |
|         - |  810 | `  static const unsigned char aHash[151] = {` |
|         - |  811 | `       0,   0,   4,  83,   0,  61,  39,  12,   0,  33,  77,   0,  48,` |
|         - |  812 | `       0,   2,  65,  67,   0,   0,   0,  47,   0,   0,  40,   0,  15,` |
|         - |  813 | `      74,   0,  51,   0,  76,   0,   0,  20,   0,   0,   0,  50,   0,` |
|         - |  814 | `      80,  34,   0,  36,   0,   0,  64,  16,   0,   0,  17,   0,   1,` |
|         - |  815 | `      19,  84,  66,   0,  43,  45,  78,   0,   0,  53,  56,   0,   0,` |
|         - |  816 | `       0,  23,  49,   0,   0,  13,  31,  54,   7,   0,   0,  25,   0,` |
|         - |  817 | `      72,  14,   0,  71,   0,  38,   6,   0,   0,   0,  73,   0,   0,` |
|         - |  818 | `       3,   0,  41,   5,  52,  57,  32,   0,  60,  63,   0,  69,  82,` |
|         - |  819 | `      30,   0,  79,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,` |
|         - |  820 | `       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  81,   0,   0,` |
|         - |  821 | `      62,   0,  11,   0,   0,  58,   0,   0,   0,   0,  59,  75,   0,` |
|         - |  822 | `       0,   0,   0,   0,   0,  35,  27,   0` |
|         - |  823 | `  };` |
|         - |  824 | `  static const unsigned char aNext[84] = {` |
|         - |  825 | `       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,` |
|         - |  826 | `       0,   0,   8,   0,   0,   0,  10,   0,   0,   0,   0,   0,   0,` |
|         - |  827 | `       0,   0,   0,   0,  28,   0,   0,   0,   0,   0,   0,   0,   0,` |
|         - |  828 | `       0,   0,   0,   0,   0,  44,   0,  18,   0,   0,   0,   0,   0,` |
|         - |  829 | `       0,  46,   0,  29,   0,   0,   0,  22,   0,   0,   0,   0,  26,` |
|         - |  830 | `       0,  21,  24,   0,   0,  68,   0,   0,   9,  37,   0,   0,   0,` |
|         - |  831 | `      42,   0,   0,   0,  70,  55` |
|         - |  832 | `  };` |
|         - |  833 | `  static const unsigned char aLen[84] = {` |
|         - |  834 | `       7,   9,   6,   5,   7,  12,   7,   2,  10,   7,   6,   9,   4,` |
|         - |  835 | `       6,   5,   7,   4,   3,   7,   3,   4,   4,   5,   4,   6,   5,` |
|         - |  836 | `       2,   3,   5,   6,   6,   3,   6,   4,   2,   5,   3,   5,   3,` |
|         - |  837 | `       3,   4,   3,   4,   8,   5,   2,   8,   5,   8,   3,   8,   5,` |
|         - |  838 | `       4,   2,   4,   4,  10,  12,   7,   5,  10,   9,   3,   6,  10,` |
|         - |  839 | `       3,   7,   2,   5,   6,   7,   9,   6,   5,   5,   3,   5,   2,` |
|         - |  840 | `       5,   4,   5,   3,   2,   5` |
|         - |  841 | `  };` |
|         - |  842 | `  static const sxu16 aOffset[84] = {` |
|         - |  843 | `       0,   3,   6,  12,  14,  20,  20,  21,  31,  34,  39,  44,  52,` |
|         - |  844 | `      55,  60,  65,  65,  70,  72,  78,  81,  83,  86,  90,  92,  97,` |
|         - |  845 | `     100, 100, 103, 106, 111, 117, 119, 119, 123, 124, 129, 130, 135,` |
|         - |  846 | `     137, 139, 143, 145, 149, 157, 159, 162, 169, 173, 181, 183, 186,` |
|         - |  847 | `     190, 194, 196, 200, 204, 214, 214, 225, 230, 240, 240, 248, 248,` |
|         - |  848 | `     251, 251, 252, 258, 263, 269, 276, 285, 290, 295, 300, 303, 308,` |
|         - |  849 | `     310, 315, 319, 324, 325, 327` |
|         - |  850 | `  };` |
|         - |  851 | `  static const sxu32 aCode[84] = {` |
|         - |  852 | `    PH7_TKWRD_EXTENDS,   PH7_TKWRD_ENDSWITCH,   PH7_TKWRD_SWITCH,    PH7_TKWRD_PRINT,   PH7_TKWRD_INT,` |
|         - |  853 | `    PH7_TKWRD_REQONCE,   PH7_TKWRD_REQUIRE,     PH7_TK_ID /* 'eq' PH7-ism removed */, PH7_TKWRD_ENDDEC, PH7_TKWRD_DECLARE,` |
|         - |  854 | `    PH7_TKWRD_RETURN,    PH7_TKWRD_NAMESPACE,   PH7_TKWRD_ECHO,      PH7_TKWRD_OBJECT,    PH7_TKWRD_THROW,` |
|         - |  855 | `    PH7_TKWRD_BOOL,      PH7_TKWRD_BOOL,        PH7_TKWRD_AND,       PH7_TKWRD_DEFAULT,   PH7_TKWRD_TRY,` |
|         - |  856 | `    PH7_TKWRD_CASE,      PH7_TKWRD_SELF,        PH7_TKWRD_FINAL,     PH7_TKWRD_LIST,      PH7_TKWRD_STATIC,` |
|         - |  857 | `    PH7_TKWRD_CLONE,     PH7_TK_ID /* 'ne' PH7-ism removed */, PH7_TKWRD_NEW,  PH7_TKWRD_CONST,     PH7_TKWRD_STRING,` |
|         - |  858 | `    PH7_TKWRD_GLOBAL,    PH7_TKWRD_USE,         PH7_TKWRD_ELIF,      PH7_TKWRD_ELSE,      PH7_TKWRD_IF,` |
|         - |  859 | `    PH7_TKWRD_FLOAT,     PH7_TKWRD_VAR,         PH7_TKWRD_ARRAY,     PH7_TKWRD_AND,       PH7_TKWRD_DIE,` |
|         - |  860 | `    PH7_TKWRD_ECHO,      PH7_TKWRD_USE,         PH7_TKWRD_ECHO,      PH7_TKWRD_ABSTRACT,  PH7_TKWRD_CLASS,` |
|         - |  861 | `    PH7_TKWRD_AS,        PH7_TKWRD_CONTINUE,    PH7_TKWRD_ENDIF,     PH7_TKWRD_FUNCTION,  PH7_TKWRD_DIE,` |
|         - |  862 | `    PH7_TKWRD_ENDWHILE,  PH7_TKWRD_WHILE,       PH7_TKWRD_EVAL,      PH7_TKWRD_DO,        PH7_TKWRD_EXIT,` |
|         - |  863 | `    PH7_TKWRD_GOTO,      PH7_TKWRD_IMPLEMENTS,  PH7_TKWRD_INCONCE,   PH7_TKWRD_INCLUDE,   PH7_TKWRD_EMPTY,` |
|         - |  864 | `    PH7_TKWRD_INSTANCEOF,PH7_TKWRD_INTERFACE,   PH7_TKWRD_INT,       PH7_TKWRD_ENDFOR,    PH7_TKWRD_END4EACH,` |
|         - |  865 | `    PH7_TKWRD_FOR,       PH7_TKWRD_FOREACH,     PH7_TKWRD_OR,        PH7_TKWRD_ISSET,     PH7_TKWRD_PARENT,` |
|         - |  866 | `    PH7_TKWRD_PRIVATE,   PH7_TKWRD_PROTECTED,   PH7_TKWRD_PUBLIC,    PH7_TKWRD_CATCH,     PH7_TKWRD_UNSET,` |
|         - |  867 | `    PH7_TKWRD_XOR,       PH7_TKWRD_ARRAY,       PH7_TKWRD_AS,        PH7_TKWRD_ARRAY,     PH7_TKWRD_EXIT,` |
|         - |  868 | `    PH7_TKWRD_UNSET,     PH7_TKWRD_XOR,         PH7_TKWRD_OR,        PH7_TKWRD_BREAK` |
|         - |  869 | `  };` |
|         - |  870 | `  int h, i;` |
|  48646211 |  871 | `  if( n<2 ) return PH7_TK_ID;` |
|  42972949 |  872 | `  h = (((int)z[0]*4) ^ ((int)z[n-1]*3) ^ n) % 151;` |
|  66736609 |  873 | `  for(i=((int)aHash[h])-1; i>=0; i=((int)aNext[i])-1){` |
|  40037559 |  874 | `    if( (int)aLen[i]==n && SyMemcmp(&zText[aOffset[i]],z,n)==0 ){` |
|         - |  875 | `       /* PH7_TKWRD_EXTENDS */` |
|         - |  876 | `       /* PH7_TKWRD_ENDSWITCH */` |
|         - |  877 | `       /* PH7_TKWRD_SWITCH */` |
|         - |  878 | `       /* PH7_TKWRD_PRINT */` |
|         - |  879 | `       /* PH7_TKWRD_INT */` |
|         - |  880 | `       /* PH7_TKWRD_REQONCE */` |
|         - |  881 | `       /* PH7_TKWRD_REQUIRE */` |
|         - |  882 | `       /* PH7_TK_ID */` |
|         - |  883 | `       /* PH7_TKWRD_ENDDEC */` |
|         - |  884 | `       /* PH7_TKWRD_DECLARE */` |
|         - |  885 | `       /* PH7_TKWRD_RETURN */` |
|         - |  886 | `       /* PH7_TKWRD_NAMESPACE */` |
|         - |  887 | `       /* PH7_TKWRD_ECHO */` |
|         - |  888 | `       /* PH7_TKWRD_OBJECT */` |
|         - |  889 | `       /* PH7_TKWRD_THROW */` |
|         - |  890 | `       /* PH7_TKWRD_BOOL */` |
|         - |  891 | `       /* PH7_TKWRD_BOOL */` |
|         - |  892 | `       /* PH7_TKWRD_AND */` |
|         - |  893 | `       /* PH7_TKWRD_DEFAULT */` |
|         - |  894 | `       /* PH7_TKWRD_TRY */` |
|         - |  895 | `       /* PH7_TKWRD_CASE */` |
|         - |  896 | `       /* PH7_TKWRD_SELF */` |
|         - |  897 | `       /* PH7_TKWRD_FINAL */` |
|         - |  898 | `       /* PH7_TKWRD_LIST */` |
|         - |  899 | `       /* PH7_TKWRD_STATIC */` |
|         - |  900 | `       /* PH7_TKWRD_CLONE */` |
|         - |  901 | `       /* PH7_TK_ID */` |
|         - |  902 | `       /* PH7_TKWRD_NEW */` |
|         - |  903 | `       /* PH7_TKWRD_CONST */` |
|         - |  904 | `       /* PH7_TKWRD_STRING */` |
|         - |  905 | `       /* PH7_TKWRD_GLOBAL */` |
|         - |  906 | `       /* PH7_TKWRD_USE */` |
|         - |  907 | `       /* PH7_TKWRD_ELIF */` |
|         - |  908 | `       /* PH7_TKWRD_ELSE */` |
|         - |  909 | `       /* PH7_TKWRD_IF */` |
|         - |  910 | `       /* PH7_TKWRD_FLOAT */` |
|         - |  911 | `       /* PH7_TKWRD_VAR */` |
|         - |  912 | `       /* PH7_TKWRD_ARRAY */` |
|         - |  913 | `       /* PH7_TKWRD_AND */` |
|         - |  914 | `       /* PH7_TKWRD_DIE */` |
|         - |  915 | `       /* PH7_TKWRD_ECHO */` |
|         - |  916 | `       /* PH7_TKWRD_USE */` |
|         - |  917 | `       /* PH7_TKWRD_ECHO */` |
|         - |  918 | `       /* PH7_TKWRD_ABSTRACT */` |
|         - |  919 | `       /* PH7_TKWRD_CLASS */` |
|         - |  920 | `       /* PH7_TKWRD_AS */` |
|         - |  921 | `       /* PH7_TKWRD_CONTINUE */` |
|         - |  922 | `       /* PH7_TKWRD_ENDIF */` |
|         - |  923 | `       /* PH7_TKWRD_FUNCTION */` |
|         - |  924 | `       /* PH7_TKWRD_DIE */` |
|         - |  925 | `       /* PH7_TKWRD_ENDWHILE */` |
|         - |  926 | `       /* PH7_TKWRD_WHILE */` |
|         - |  927 | `       /* PH7_TKWRD_EVAL */` |
|         - |  928 | `       /* PH7_TKWRD_DO */` |
|         - |  929 | `       /* PH7_TKWRD_EXIT */` |
|         - |  930 | `       /* PH7_TKWRD_GOTO */` |
|         - |  931 | `       /* PH7_TKWRD_IMPLEMENTS */` |
|         - |  932 | `       /* PH7_TKWRD_INCONCE */` |
|         - |  933 | `       /* PH7_TKWRD_INCLUDE */` |
|         - |  934 | `       /* PH7_TKWRD_EMPTY */` |
|         - |  935 | `       /* PH7_TKWRD_INSTANCEOF */` |
|         - |  936 | `       /* PH7_TKWRD_INTERFACE */` |
|         - |  937 | `       /* PH7_TKWRD_INT */` |
|         - |  938 | `       /* PH7_TKWRD_ENDFOR */` |
|         - |  939 | `       /* PH7_TKWRD_END4EACH */` |
|         - |  940 | `       /* PH7_TKWRD_FOR */` |
|         - |  941 | `       /* PH7_TKWRD_FOREACH */` |
|         - |  942 | `       /* PH7_TKWRD_OR */` |
|         - |  943 | `       /* PH7_TKWRD_ISSET */` |
|         - |  944 | `       /* PH7_TKWRD_PARENT */` |
|         - |  945 | `       /* PH7_TKWRD_PRIVATE */` |
|         - |  946 | `       /* PH7_TKWRD_PROTECTED */` |
|         - |  947 | `       /* PH7_TKWRD_PUBLIC */` |
|         - |  948 | `       /* PH7_TKWRD_CATCH */` |
|         - |  949 | `       /* PH7_TKWRD_UNSET */` |
|         - |  950 | `       /* PH7_TKWRD_XOR */` |
|         - |  951 | `       /* PH7_TKWRD_ARRAY */` |
|         - |  952 | `       /* PH7_TKWRD_AS */` |
|         - |  953 | `       /* PH7_TKWRD_ARRAY */` |
|         - |  954 | `       /* PH7_TKWRD_EXIT */` |
|         - |  955 | `       /* PH7_TKWRD_UNSET */` |
|         - |  956 | `       /* PH7_TKWRD_XOR */` |
|         - |  957 | `       /* PH7_TKWRD_OR */` |
|         - |  958 | `       /* PH7_TKWRD_BREAK */` |
|  16273899 |  959 | `      return aCode[i];` |
|         - |  960 | `    }` |
|  11881833 |  961 | `  }` |
|         - |  962 | `  /* Linear fallback for keywords not in the auto-generated hash table */` |
|  26699055 |  963 | `  if( n==5 && SyMemcmp(z,"trait",5)==0 ) return PH7_TKWRD_TRAIT;` |
|  26691143 |  964 | `  if( n==9 && SyMemcmp(z,"insteadof",9)==0 ) return PH7_TKWRD_INSTEADOF;` |
|  26691139 |  965 | `  if( n==7 && SyMemcmp(z,"finally",7)==0 ) return PH7_TKWRD_FINALLY;` |
|  26690967 |  966 | `  if( n==5 && SyMemcmp(z,"yield",5)==0 ) return PH7_TKWRD_YIELD;` |
|  26674915 |  967 | `  if( n==5 && SyMemcmp(z,"match",5)==0 ) return PH7_TKWRD_MATCH;` |
|  26674839 |  968 | `  return PH7_TK_ID;` |
|  24323108 |  969 | `}` |
|         - |  970 | `/* --- End of Automatically generated code --- */` |
|         - |  971 | `/* SPDX-SnippetEnd */` |
|         - |  972 | `/*` |
|         - |  973 | ` * Extract a heredoc/nowdoc text from a raw PHP input.` |
|         - |  974 | ` * According to the PHP language reference manual:` |
|         - |  975 | ` *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier` |
|         - |  976 | ` *  is provided, then a newline. The string itself follows, and then the same identifier again` |
|         - |  977 | ` *  to close the quotation.` |
|         - |  978 | ` *  The closing identifier must begin in the first column of the line. Also, the identifier must` |
|         - |  979 | ` *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric` |
|         - |  980 | ` *  characters and underscores, and must start with a non-digit character or underscore.` |
|         - |  981 | ` *  Heredoc text behaves just like a double-quoted string, without the double quotes.` |
|         - |  982 | ` *  This means that quotes in a heredoc do not need to be escaped, but the escape codes listed` |
|         - |  983 | ` *  above can still be used. Variables are expanded, but the same care must be taken when expressing` |
|         - |  984 | ` *  complex variables inside a heredoc as with strings.` |
|         - |  985 | ` *  Nowdocs are to single-quoted strings what heredocs are to double-quoted strings.` |
|         - |  986 | ` *  A nowdoc is specified similarly to a heredoc, but no parsing is done inside a nowdoc.` |
|         - |  987 | ` *  The construct is ideal for embedding PHP code or other large blocks of text without the need` |
|         - |  988 | ` *  for escaping. It shares some features in common with the SGML <![CDATA[ ]]> construct, in that` |
|         - |  989 | ` *  it declares a block of text which is not for parsing.` |
|         - |  990 | ` *  A nowdoc is identified with the same <<< sequence used for heredocs, but the identifier which follows` |
|         - |  991 | ` *  is enclosed in single quotes, e.g. <<<'EOT'. All the rules for heredoc identifiers also apply to nowdoc` |
|         - |  992 | ` *  identifiers, especially those regarding the appearance of the closing identifier.` |
|         - |  993 | ` * Symisc Extension:` |
|         - |  994 | ` * The closing delimiter can now start with a digit or undersocre or it can be an UTF-8 stream.` |
|         - |  995 | ` * Example:` |
|         - |  996 | ` *  <<<123` |
|         - |  997 | ` *    HEREDOC Here` |
|         - |  998 | ` * 123` |
|         - |  999 | ` *  or` |
|         - | 1000 | ` *  <<<___` |
|         - | 1001 | ` *   HEREDOC Here` |
|         - | 1002 | ` *  ___` |
|         - | 1003 | ` */` |
|       118 | 1004 | `static sxi32 LexExtractHeredoc(SyStream *pStream,SyToken *pToken)` |
|         5 | 1005 | `{` |
|       123 | 1006 | `	const unsigned char *zIn  = pStream->zText;` |
|       123 | 1007 | `	const unsigned char *zEnd = pStream->zEnd;` |
|         - | 1008 | `	const unsigned char *zPtr;` |
|       123 | 1009 | `	sxu8 bNowDoc = FALSE;` |
|         - | 1010 | `	SyString sDelim;` |
|         - | 1011 | `	SyString sStr;` |
|         - | 1012 | `	/* Jump leading white spaces */` |
|       135 | 1013 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|        13 | 1014 | `		zIn++;` |
|         1 | 1015 | `	}` |
|       123 | 1016 | `	if( zIn >= zEnd ){` |
|         - | 1017 | `		/* A simple symbol,return immediately */` |
|       ! 0 | 1018 | `		return SXERR_CONTINUE;` |
|         - | 1019 | `	}` |
|       123 | 1020 | `	if( zIn[0] == '\'' \|\| zIn[0] == '"' ){` |
|         - | 1021 | `		/* Make sure we are dealing with a nowdoc */` |
|        51 | 1022 | `		bNowDoc =  zIn[0] == '\'' ? TRUE : FALSE;` |
|        51 | 1023 | `		zIn++;` |
|        24 | 1024 | `	}` |
|       123 | 1025 | `	if( zIn[0] < 0xc0 && !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|         - | 1026 | `		/* Invalid delimiter,return immediately */` |
|       ! 0 | 1027 | `		return SXERR_CONTINUE;` |
|         - | 1028 | `	}` |
|         - | 1029 | `	/* Isolate the identifier */` |
|       123 | 1030 | `	sDelim.zString = (const char *)zIn;` |
|       126 | 1031 | `	for(;;){` |
|       257 | 1032 | `		zPtr = zIn;` |
|         - | 1033 | `		/* Skip alphanumeric stream */` |
|       807 | 1034 | `		while( zPtr < zEnd && zPtr[0] < 0xc0 && (SyisAlphaNum(zPtr[0]) \|\| zPtr[0] == '_') ){` |
|       429 | 1035 | `			zPtr++;` |
|         5 | 1036 | `		}` |
|       257 | 1037 | `		if( zPtr < zEnd && zPtr[0] >= 0xc0 ){` |
|        19 | 1038 | `			zPtr++;` |
|         - | 1039 | `			/* UTF-8 stream */` |
|        37 | 1040 | `			while( zPtr < zEnd && ((zPtr[0] & 0xc0) == 0x80) ){` |
|        19 | 1041 | `				zPtr++;` |
|         1 | 1042 | `			}` |
|         9 | 1043 | `		}` |
|       257 | 1044 | `		if( zPtr == zIn ){` |
|         - | 1045 | `			/* Not an UTF-8 or alphanumeric stream */` |
|       123 | 1046 | `			break;` |
|         - | 1047 | `		}` |
|         - | 1048 | `		/* Synchronize pointers */` |
|       139 | 1049 | `		zIn = zPtr;` |
|         5 | 1050 | `	}` |
|         - | 1051 | `	/* Get the identifier length */` |
|       123 | 1052 | `	sDelim.nByte = (sxu32)((const char *)zIn-sDelim.zString);` |
|       123 | 1053 | `	if( zIn[0] == '"' \|\| (bNowDoc && zIn[0] == '\'') ){` |
|         - | 1054 | `		/* Jump the trailing single quote */` |
|        51 | 1055 | `		zIn++;` |
|        24 | 1056 | `	}` |
|         - | 1057 | `	/* Jump trailing white spaces */` |
|       123 | 1058 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|       ! 0 | 1059 | `		zIn++;` |
|       ! 0 | 1060 | `	}` |
|       123 | 1061 | `	if( sDelim.nByte <= 0 \|\| zIn >= zEnd \|\| zIn[0] != '\n' ){` |
|         - | 1062 | `		/* Invalid syntax */` |
|       ! 0 | 1063 | `		return SXERR_CONTINUE;` |
|         - | 1064 | `	}` |
|       123 | 1065 | `	pStream->nLine++; /* Increment line counter */` |
|       123 | 1066 | `	zIn++;` |
|         - | 1067 | `	/* Isolate the delimited string */` |
|       123 | 1068 | `	sStr.zString = (const char *)zIn;` |
|         - | 1069 | `	/* PHP 7.3 flexible heredoc/nowdoc: the closing marker may be preceded` |
|         - | 1070 | `	 * by whitespace (spaces/tabs), and may be followed by any non-identifier` |
|         - | 1071 | `	 * character. The indent count is recorded in pToken->pUserData and the` |
|         - | 1072 | `	 * compile phase strips it from each body line. */` |
|         - | 1073 | `	{` |
|       123 | 1074 | `		const unsigned char *zMarkerLine = zIn; /* Start of marker's line (set on match) */` |
|       123 | 1075 | `		sxu32 nIndent = 0;` |
|       265 | 1076 | `		for(;;){` |
|       329 | 1077 | `			const unsigned char *zLineStart = zIn;` |
|         - | 1078 | `			/* Skip leading space/tab on this line */` |
|       881 | 1079 | `			while( zIn < zEnd && (zIn[0] == ' ' \|\| zIn[0] == '\t') ){` |
|       393 | 1080 | `				zIn++;` |
|         3 | 1081 | `			}` |
|       324 | 1082 | `			if( (sxu32)(zEnd - zIn) >= sDelim.nByte` |
|       328 | 1083 | `				&& SyMemcmp((const void *)sDelim.zString,(const void *)zIn,sDelim.nByte) == 0 ){` |
|         - | 1084 | `				int bIdentCont;` |
|       120 | 1085 | `				zPtr = &zIn[sDelim.nByte];` |
|         - | 1086 | `				/* Disambiguate: next byte must not continue an identifier.` |
|         - | 1087 | `				 * A leading byte >= 0xc0 starts a multi-byte UTF-8 sequence,` |
|         - | 1088 | `				 * which PHP identifiers may contain, so treat it as ident. */` |
|       120 | 1089 | `				if( zPtr >= zEnd ){` |
|       ! 0 | 1090 | `					bIdentCont = 0;` |
|       120 | 1091 | `				}else if( zPtr[0] >= 0xc0 ){` |
|       ! 0 | 1092 | `					bIdentCont = 1;` |
|       ! 0 | 1093 | `				}else{` |
|       120 | 1094 | `					bIdentCont = (SyisAlphaNum(zPtr[0]) \|\| zPtr[0] == '_');` |
|         - | 1095 | `				}` |
|       120 | 1096 | `				if( !bIdentCont ){` |
|         - | 1097 | `					/* Closing marker found */` |
|       120 | 1098 | `					nIndent = (sxu32)(zIn - zLineStart);` |
|       120 | 1099 | `					zMarkerLine = zLineStart;` |
|       120 | 1100 | `					pStream->zText = zPtr; /* Cursor right after identifier */` |
|       120 | 1101 | `					break;` |
|         - | 1102 | `				}` |
|       ! 0 | 1103 | `			}` |
|         - | 1104 | `			/* Not the closing marker on this line; walk to next newline */` |
|      4481 | 1105 | `			while( zIn < zEnd && zIn[0] != '\n' ){` |
|      4273 | 1106 | `				zIn++;` |
|         5 | 1107 | `			}` |
|       213 | 1108 | `			if( zIn >= zEnd ){` |
|         - | 1109 | `				/* End of input without finding the closing marker */` |
|         3 | 1110 | `				pStream->zText = pStream->zEnd;` |
|         3 | 1111 | `				zMarkerLine = zIn;` |
|         3 | 1112 | `				break;` |
|         - | 1113 | `			}` |
|       211 | 1114 | `			pStream->nLine++;` |
|       211 | 1115 | `			zIn++;` |
|         5 | 1116 | `		}` |
|         - | 1117 | `		/* Body runs from sStr.zString up to just before the marker line */` |
|       123 | 1118 | `		sStr.nByte = (sxu32)((const char *)zMarkerLine - sStr.zString);` |
|       123 | 1119 | `		pToken->nType = bNowDoc ? PH7_TK_NOWDOC : PH7_TK_HEREDOC;` |
|       123 | 1120 | `		SyStringDupPtr(&pToken->sData,&sStr);` |
|         - | 1121 | `		/* Strip exactly one line terminator that precedes the marker's line. */` |
|       118 | 1122 | `		if( pToken->sData.nByte > 0` |
|       119 | 1123 | `			&& pToken->sData.zString[pToken->sData.nByte - 1] == '\n' ){` |
|       112 | 1124 | `			pToken->sData.nByte--;` |
|       108 | 1125 | `			if( pToken->sData.nByte > 0` |
|       112 | 1126 | `				&& pToken->sData.zString[pToken->sData.nByte - 1] == '\r' ){` |
|       ! 0 | 1127 | `				pToken->sData.nByte--;` |
|       ! 0 | 1128 | `			}` |
|        54 | 1129 | `		}` |
|       123 | 1130 | `		pToken->pUserData = SX_INT_TO_PTR(nIndent);` |
|         - | 1131 | `	}` |
|         - | 1132 | `	/* All done */` |
|       123 | 1133 | `	return SXRET_OK;` |
|        64 | 1134 | `}` |
|         - | 1135 | `/*` |
|         - | 1136 | ` * Tokenize a raw PHP input.` |
|         - | 1137 | ` * This is the public tokenizer called by most code generator routines.` |
|         - | 1138 | ` */` |
|     84042 | 1139 | `PH7_PRIVATE sxi32 PH7_TokenizePHP(const char *zInput,sxu32 nLen,sxu32 nLineStart,SySet *pOut,SySet *pTrivia)` |
|         5 | 1140 | `{` |
|         - | 1141 | `	SyLex sLexer;` |
|         - | 1142 | `	sxi32 rc;` |
|         - | 1143 | `	/* Defense-in-depth cap for internal tokenizer calls that bypass ph7_compile() */` |
|     84047 | 1144 | `	if( nLen > PH7_MAX_INPUT_SIZE ){` |
|       ! 0 | 1145 | `		return SXERR_LIMIT;` |
|         - | 1146 | `	}` |
|         - | 1147 | `	/* Initialize the lexer. pTrivia (may be NULL = discard) rides as the` |
|         - | 1148 | `	 * tokenizer callback's user data: doc-comments (and later attribute` |
|         - | 1149 | `	 * groups) are recorded there instead of entering the token stream. */` |
|     84047 | 1150 | `	rc = SyLexInit(&sLexer,&(*pOut),TokenizePHP,pTrivia);` |
|     84047 | 1151 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 1152 | `		return rc;` |
|         - | 1153 | `	}` |
|     84047 | 1154 | `	sLexer.sStream.nLine = nLineStart;` |
|         - | 1155 | `	/* Tokenize input */` |
|     84047 | 1156 | `	rc = SyLexTokenizeInput(&sLexer,zInput,nLen,0,0,0);` |
|         - | 1157 | `	/* Release the lexer */` |
|     84047 | 1158 | `	SyLexRelease(&sLexer);` |
|         - | 1159 | `	/* Tokenization result */` |
|     84047 | 1160 | `	return rc;` |
|     42026 | 1161 | `}` |
|         - | 1162 | `/*` |
|         - | 1163 | ` * High level public tokenizer.` |
|         - | 1164 | ` *  Tokenize the input into PHP tokens and raw tokens [i.e: HTML,XML,Raw text...].` |
|         - | 1165 | ` * According to the PHP language reference manual` |
|         - | 1166 | ` *   When PHP parses a file, it looks for opening and closing tags, which tell PHP` |
|         - | 1167 | ` *   to start and stop interpreting the code between them. Parsing in this manner allows` |
|         - | 1168 | ` *   PHP to be embedded in all sorts of different documents, as everything outside of a pair` |
|         - | 1169 | ` *   of opening and closing tags is ignored by the PHP parser. Most of the time you will see` |
|         - | 1170 | ` *   PHP embedded in HTML documents, as in this example.` |
|         - | 1171 | ` *   <?php echo 'While this is going to be parsed.'; ?>` |
|         - | 1172 | ` *   <p>This will also be ignored.</p>` |
|         - | 1173 | ` *   You can also use more advanced structures:` |
|         - | 1174 | ` *   Example #1 Advanced escaping` |
|         - | 1175 | ` * <?php` |
|         - | 1176 | ` * if ($expression) {` |
|         - | 1177 | ` *   ?>` |
|         - | 1178 | ` *   <strong>This is true.</strong>` |
|         - | 1179 | ` *   <?php` |
|         - | 1180 | ` * } else {` |
|         - | 1181 | ` *   ?>` |
|         - | 1182 | ` *   <strong>This is false.</strong>` |
|         - | 1183 | ` *   <?php` |
|         - | 1184 | ` * }` |
|         - | 1185 | ` * ?>` |
|         - | 1186 | ` * This works as expected, because when PHP hits the ?> closing tags, it simply starts outputting` |
|         - | 1187 | ` * whatever it finds (except for an immediately following newline - see instruction separation ) until it hits` |
|         - | 1188 | ` * another opening tag. The example given here is contrived, of course, but for outputting large blocks of text` |
|         - | 1189 | ` * dropping out of PHP parsing mode is generally more efficient than sending all of the text through echo() or print().` |
|         - | 1190 | ` * There are four different pairs of opening and closing tags which can be used in PHP. Three of those, <?php ?>` |
|         - | 1191 | ` * <script language="php"> </script>  and <? ?> are always available. The other two are short tags and ASP style` |
|         - | 1192 | ` * tags, and can be turned on and off from the php.ini configuration file. As such, while some people find short tags` |
|         - | 1193 | ` * and ASP style tags convenient, they are less portable, and generally not recommended.` |
|         - | 1194 | ` * Note:` |
|         - | 1195 | ` * Also note that if you are embedding PHP within XML or XHTML you will need to use the <?php ?> tags to remain` |
|         - | 1196 | ` * compliant with standards.` |
|         - | 1197 | ` * Example #2 PHP Opening and Closing Tags` |
|         - | 1198 | ` * 1.  <?php echo 'if you want to serve XHTML or XML documents, do it like this'; ?>` |
|         - | 1199 | ` * 2.  <script language="php">` |
|         - | 1200 | ` *       echo 'some editors (like FrontPage) don\'t` |
|         - | 1201 | ` *             like processing instructions';` |
|         - | 1202 | ` *   </script>` |
|         - | 1203 | ` *` |
|         - | 1204 | ` * 3.  <? echo 'this is the simplest, an SGML processing instruction'; ?>` |
|         - | 1205 | ` *   <?= expression ?> This is a shortcut for "<? echo expression ?>"` |
|         - | 1206 | ` */` |
|     13678 | 1207 | `PH7_PRIVATE sxi32 PH7_TokenizeRawText(const char *zInput,sxu32 nLen,SySet *pOut)` |
|         5 | 1208 | `{` |
|     13683 | 1209 | `	const char *zEnd = &zInput[nLen];` |
|     13683 | 1210 | `	const char *zIn  = zInput;` |
|         - | 1211 | `	const char *zCur,*zCurEnd;` |
|     13683 | 1212 | `	SyString sCtag = { 0, 0 };     /* Closing tag */` |
|         - | 1213 | `	SyToken sToken;` |
|         - | 1214 | `	SyString sDoc;` |
|         - | 1215 | `	sxu32 nLine;` |
|         - | 1216 | `	sxi32 iNest;` |
|         - | 1217 | `	sxi32 rc;` |
|         - | 1218 | `	/* Tokenize the input into PHP tokens and raw tokens */` |
|     13683 | 1219 | `	nLine = 1;` |
|     13683 | 1220 | `	zCur = zCurEnd   = 0; /* Prevent compiler warning */` |
|     13683 | 1221 | `	sToken.pUserData = 0;` |
|     13683 | 1222 | `	iNest = 0;` |
|     13683 | 1223 | `	sDoc.nByte = 0;` |
|     13683 | 1224 | `	sDoc.zString = ""; /* cc warning */` |
|     13683 | 1225 | `	for(;;){` |
|     27371 | 1226 | `		if( zIn >= zEnd ){` |
|         - | 1227 | `			/* End of input reached */` |
|     13683 | 1228 | `			break;` |
|         - | 1229 | `		}` |
|     13693 | 1230 | `		sToken.nLine = nLine;` |
|     13693 | 1231 | `		zCur = zIn;` |
|     13693 | 1232 | `		zCurEnd = 0;` |
|     13705 | 1233 | `		while( zIn < zEnd ){` |
|     13705 | 1234 | `			 if( zIn[0] == '<' ){` |
|     13693 | 1235 | `				const char *zTmp = zIn; /* End of raw input marker */` |
|     13693 | 1236 | `				zIn++;` |
|     13693 | 1237 | `				if( zIn < zEnd ){` |
|     13693 | 1238 | `					if( zIn[0] == '?' ){` |
|     13693 | 1239 | `						zIn++;` |
|     13693 | 1240 | `						if( (sxu32)(zEnd - zIn) >= sizeof("php")-1 &&  SyStrnicmp(zIn,"php",sizeof("php")-1) == 0 ){` |
|         - | 1241 | `							/* opening tag: <?php */` |
|     13691 | 1242 | `							zIn += sizeof("php")-1;` |
|      6843 | 1243 | `						}` |
|         - | 1244 | `						/* Look for the closing tag '?>' */` |
|     13693 | 1245 | `						SyStringInitFromBuf(&sCtag,"?>",sizeof("?>")-1);` |
|     13693 | 1246 | `						zCurEnd = zTmp;` |
|     13693 | 1247 | `						break;` |
|         - | 1248 | `					}` |
|       ! 0 | 1249 | `				}` |
|       ! 0 | 1250 | `			}else{` |
|        13 | 1251 | `				if( zIn[0] == '\n' ){` |
|         5 | 1252 | `					nLine++;` |
|         2 | 1253 | `				}` |
|        13 | 1254 | `				zIn++;` |
|         - | 1255 | `			 }` |
|         1 | 1256 | `		} /* While(zIn < zEnd) */` |
|     13693 | 1257 | `		if( zCurEnd == 0 ){` |
|       ! 0 | 1258 | `			zCurEnd = zIn;` |
|       ! 0 | 1259 | `		}` |
|         - | 1260 | `		/* Save the raw token */` |
|     13693 | 1261 | `		SyStringInitFromBuf(&sToken.sData,zCur,zCurEnd - zCur);` |
|     13693 | 1262 | `		sToken.nType = PH7_TOKEN_RAW;` |
|     13693 | 1263 | `		rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|     13693 | 1264 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1265 | `			return rc;` |
|         - | 1266 | `		}` |
|     13693 | 1267 | `		if( zIn >= zEnd ){` |
|       ! 0 | 1268 | `			break;` |
|         - | 1269 | `		}` |
|         - | 1270 | `		/* Ignore leading white space */` |
|     29153 | 1271 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|     15465 | 1272 | `			if( zIn[0] == '\n' ){` |
|     14617 | 1273 | `				nLine++;` |
|      7306 | 1274 | `			}` |
|     15465 | 1275 | `			zIn++;` |
|         5 | 1276 | `		}` |
|         - | 1277 | `		/* Delimit the PHP chunk */` |
|     13693 | 1278 | `		sToken.nLine = nLine;` |
|     13693 | 1279 | `		zCur = zIn;` |
|   1782387 | 1280 | `		while( (sxu32)(zEnd - zIn) >= sCtag.nByte ){` |
|         - | 1281 | `			const char *zPtr;` |
|   1776335 | 1282 | `			if( SyMemcmp(zIn,sCtag.zString,sCtag.nByte) == 0 && iNest < 1 ){` |
|      7637 | 1283 | `				break;` |
|         - | 1284 | `			}` |
|    887833 | 1285 | `			for(;;){` |
|   1775671 | 1286 | `				if( zIn[0] != '/' \|\| (zIn[1] != '*' && zIn[1] != '/') /* && sCtag.nByte >= 2 */ ){` |
|    884354 | 1287 | `					break;` |
|         - | 1288 | `				}` |
|      6973 | 1289 | `				zIn += 2;` |
|      6973 | 1290 | `				if( zIn[-1] == '/' ){` |
|         - | 1291 | `					/* Inline comment */` |
|    268233 | 1292 | `					while( zIn < zEnd && zIn[0] != '\n' ){` |
|    261527 | 1293 | `						zIn++;` |
|         5 | 1294 | `					}` |
|      6711 | 1295 | `					if( zIn >= zEnd ){` |
|         3 | 1296 | `						zIn--;` |
|         1 | 1297 | `					}` |
|      3358 | 1298 | `				}else{` |
|         - | 1299 | `					/* Block comment */` |
|     23037 | 1300 | `					while( (sxu32)(zEnd-zIn) >= sizeof("*/") - 1 ){` |
|     23037 | 1301 | `						if( zIn[0] == '*' && zIn[1] == '/' ){` |
|       267 | 1302 | `							zIn += 2;` |
|       267 | 1303 | `							break;` |
|         - | 1304 | `						}` |
|     22775 | 1305 | `						if( zIn[0] == '\n' ){` |
|       165 | 1306 | `							nLine++;` |
|        80 | 1307 | `						}` |
|     22775 | 1308 | `						zIn++;` |
|         5 | 1309 | `					}` |
|         - | 1310 | `				}` |
|         5 | 1311 | `			}` |
|   1768703 | 1312 | `			if( zIn[0] == '\n' ){` |
|     58937 | 1313 | `				nLine++;` |
|     58937 | 1314 | `				if( iNest > 0 ){` |
|       329 | 1315 | `					zIn++;` |
|       719 | 1316 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|       393 | 1317 | `						zIn++;` |
|         3 | 1318 | `					}` |
|       329 | 1319 | `					zPtr = zIn;` |
|      1645 | 1320 | `					while( zIn < zEnd ){` |
|      1645 | 1321 | `						if( (unsigned char)zIn[0] >= 0xc0 ){` |
|         - | 1322 | `							/* UTF-8 stream */` |
|        19 | 1323 | `							zIn++;` |
|        37 | 1324 | `							SX_JMP_UTF8(zIn,zEnd);` |
|      1632 | 1325 | `						}else if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       167 | 1326 | `							break;` |
|       ! 0 | 1327 | `						}else{` |
|      1303 | 1328 | `							zIn++;` |
|         - | 1329 | `						}` |
|         5 | 1330 | `					}` |
|       329 | 1331 | `					if( (sxu32)(zIn - zPtr) == sDoc.nByte && SyMemcmp(sDoc.zString,zPtr,sDoc.nByte) == 0 ){` |
|       120 | 1332 | `						iNest = 0;` |
|        58 | 1333 | `					}` |
|       329 | 1334 | `					continue;` |
|         5 | 1335 | `				}` |
|   1739075 | 1336 | `			}else if ( (sxu32)(zEnd - zIn) >= sizeof("<<<") && zIn[0] == '<' && zIn[1] == '<' && zIn[2] == '<' && iNest < 1){` |
|       123 | 1337 | `				zIn += sizeof("<<<")-1;` |
|       135 | 1338 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|        13 | 1339 | `					zIn++;` |
|         1 | 1340 | `				}` |
|       123 | 1341 | `				if( zIn[0] == '"' \|\| zIn[0] == '\'' ){` |
|        51 | 1342 | `					zIn++;` |
|        24 | 1343 | `				}` |
|       123 | 1344 | `				zPtr = zIn;` |
|       565 | 1345 | `				while( zIn < zEnd ){` |
|       565 | 1346 | `					if( (unsigned char)zIn[0] >= 0xc0 ){` |
|         - | 1347 | `						/* UTF-8 stream */` |
|        19 | 1348 | `						zIn++;` |
|        37 | 1349 | `						SX_JMP_UTF8(zIn,zEnd);` |
|       552 | 1350 | `					}else if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|        64 | 1351 | `						break;` |
|       ! 0 | 1352 | `					}else{` |
|       429 | 1353 | `						zIn++;` |
|         - | 1354 | `					}` |
|         5 | 1355 | `				}` |
|       123 | 1356 | `				SyStringInitFromBuf(&sDoc,zPtr,zIn-zPtr);` |
|       123 | 1357 | `				SyStringFullTrim(&sDoc);` |
|       123 | 1358 | `				if( sDoc.nByte > 0 ){` |
|       123 | 1359 | `					iNest++;` |
|        59 | 1360 | `				}` |
|       123 | 1361 | `				continue;` |
|         - | 1362 | `			}` |
|   1768261 | 1363 | `			zIn++;` |
|         - | 1364 |  |
|   1768261 | 1365 | `			if ( zIn >= zEnd )` |
|         5 | 1366 | `				break;` |
|         5 | 1367 | `		}` |
|     13693 | 1368 | `		if( (sxu32)(zEnd - zIn) < sCtag.nByte ){` |
|      6061 | 1369 | `			zIn = zEnd;` |
|      3028 | 1370 | `		}` |
|     13693 | 1371 | `		if( zCur < zIn ){` |
|         - | 1372 | `			/* Save the PHP chunk for later processing */` |
|     10617 | 1373 | `			sToken.nType = PH7_TOKEN_PHP;` |
|     10617 | 1374 | `			SyStringInitFromBuf(&sToken.sData,zCur,zIn-zCur);` |
|     21099 | 1375 | `			SyStringRightTrim(&sToken.sData); /* Trim trailing white spaces */` |
|     10617 | 1376 | `			rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|     10617 | 1377 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 1378 | `				return rc;` |
|         - | 1379 | `			}` |
|      5306 | 1380 | `		}` |
|     13693 | 1381 | `		if( zIn < zEnd ){` |
|         - | 1382 | `			/* Jump the trailing closing tag */` |
|      7637 | 1383 | `			zIn += sCtag.nByte;` |
|         - | 1384 | `			/* php's lexer swallows exactly ONE newline immediately after the` |
|         - | 1385 | `			 * closing tag ("?>\n" emits nothing) */` |
|      7637 | 1386 | `			if( zIn < zEnd && zIn[0] == '\r' && zIn + 1 < zEnd && zIn[1] == '\n' ){` |
|       ! 0 | 1387 | `				zIn += 2;` |
|       ! 0 | 1388 | `				nLine++;` |
|      7637 | 1389 | `			}else if( zIn < zEnd && zIn[0] == '\n' ){` |
|        64 | 1390 | `				zIn++;` |
|        64 | 1391 | `				nLine++;` |
|        30 | 1392 | `			}` |
|      3816 | 1393 | `		}` |
|         5 | 1394 | `	} /* For(;;) */` |
|         - | 1395 |  |
|     13683 | 1396 | ` 	return SXRET_OK;` |
|      6844 | 1397 | `}` |
|         - | 1398 |  |
