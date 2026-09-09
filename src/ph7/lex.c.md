# src/ph7/lex.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 789/846 lines (93.26%)

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
| 162388758 |   19 | `static sxi32 TokenizePHP(SyStream *pStream,SyToken *pToken,void *pUserData,void *pCtxData)` |
|         5 |   20 | `{` |
|         - |   21 | `	SyString *pStr;` |
|         - |   22 | `	sxi32 rc;` |
|         - |   23 | `	/* Ignore leading white spaces */` |
| 239983921 |   24 | `	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){` |
|         - |   25 | `		/* Advance the stream cursor */` |
|  77595163 |   26 | `		if( pStream->zText[0] == '\n' ){` |
|         - |   27 | `			/* Update line counter */` |
|     53081 |   28 | `			pStream->nLine++;` |
|     26538 |   29 | `		}` |
|  77595163 |   30 | `		pStream->zText++;` |
|         5 |   31 | `	}` |
| 162388763 |   32 | `	if( pStream->zText >= pStream->zEnd ){` |
|         - |   33 | `		/* End of input reached */` |
|       ! 0 |   34 | `		return SXERR_EOF;` |
|         - |   35 | `	}` |
|         - |   36 | `	/* Record token starting position and line */` |
| 162388763 |   37 | `	pToken->nLine = pStream->nLine;` |
| 162388763 |   38 | `	pToken->pUserData = 0;` |
| 162388763 |   39 | `	pStr = &pToken->sData;` |
| 162388763 |   40 | `	SyStringInitFromBuf(pStr,pStream->zText,0);` |
| 189310793 |   41 | `	if( pStream->zText[0] >= 0xc0 \|\| SyisAlpha(pStream->zText[0]) \|\| pStream->zText[0] == '_' ){` |
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
|  53844065 |   52 | `		if( pStream->zText[0] < 0xc0 ){` |
|  53844049 |   53 | `			pStream->zText++;` |
|  26922022 |   54 | `		}` |
|  50375111 |   55 | `		for(;;){` |
| 100750227 |   56 | `			zIn = pStream->zText;` |
| 100750227 |   57 | `			if( zIn[0] >= 0xc0 ){` |
|        49 |   58 | `				zIn++;` |
|         - |   59 | `				/* UTF-8 stream */` |
|       109 |   60 | `				while( zIn < pStream->zEnd && ((zIn[0] & 0xc0) == 0x80) ){` |
|        61 |   61 | `					zIn++;` |
|         1 |   62 | `				}` |
|        24 |   63 | `			}` |
|         - |   64 | `			/* Skip alphanumeric stream */` |
| 400688410 |   65 | `			while( zIn < pStream->zEnd && zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_') ){` |
| 249563077 |   66 | `				zIn++;` |
|         5 |   67 | `			}` |
| 100750227 |   68 | `			if( zIn == pStream->zText ){` |
|         - |   69 | `				/* Not an UTF-8 or alphanumeric stream */` |
|  53844065 |   70 | `				break;` |
|         - |   71 | `			}` |
|         - |   72 | `			/* Synchronize pointers */` |
|  46906167 |   73 | `			pStream->zText = zIn;` |
|         5 |   74 | `		}` |
|         - |   75 | `		/* Record token length */` |
|  53844065 |   76 | `		pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  53844065 |   77 | `		nKeyword = KeywordCode(pStr->zString,(int)pStr->nByte);` |
|         - |   78 | `		/* PHP 7.4: 'fn' is a keyword reserved for arrow functions.` |
|         - |   79 | `		 * The auto-generated perfect hash above doesn't know about it,` |
|         - |   80 | `		 * so intercept the 'fn' identifier here.` |
|         - |   81 | `		 */` |
|  53844060 |   82 | `		if( nKeyword == PH7_TK_ID && pStr->nByte == 2` |
|  18601440 |   83 | `			&& pStr->zString[0] == 'f' && pStr->zString[1] == 'n' ){` |
|       809 |   84 | `			nKeyword = PH7_TKWRD_FN;` |
|       402 |   85 | `		}` |
|  53844065 |   86 | `		if( nKeyword != PH7_TK_ID ){` |
|  17875739 |   87 | `			if( nKeyword &` |
|         - |   88 | `				(PH7_TKWRD_NEW\|PH7_TKWRD_CLONE\|PH7_TKWRD_AND\|PH7_TKWRD_XOR\|PH7_TKWRD_OR\|PH7_TKWRD_INSTANCEOF) ){` |
|         - |   89 | `					/* Alpha stream operators [i.e: new,clone,and,instanceof,or,xor],save the operator instance for later processing */` |
|    884931 |   90 | `					pToken->pUserData = (void *)PH7_ExprExtractOperator(pStr,0);` |
|         - |   91 | `					/* Mark as an operator */` |
|    884931 |   92 | `					pToken->nType = PH7_TK_ID\|PH7_TK_OP;` |
|    442468 |   93 | `			}else{` |
|         - |   94 | `				/* We are dealing with a keyword [i.e: while,foreach,class...],save the keyword ID */` |
|  16990813 |   95 | `				pToken->nType = PH7_TK_KEYWORD;` |
|  16990813 |   96 | `				pToken->pUserData = SX_INT_TO_PTR(nKeyword);` |
|         - |   97 | `			}` |
|   8937872 |   98 | `		}else{` |
|         - |   99 | `			/* A simple identifier */` |
|  35968331 |  100 | `			pToken->nType = PH7_TK_ID;` |
|         - |  101 | `		}` |
|  26922035 |  102 | `	}else{` |
|         - |  103 | `		sxi32 c;` |
|         - |  104 | `		/* Non-alpha stream */` |
| 108544703 |  105 | `		if( pStream->zText[0] == '#' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '[' ){` |
|      7789 |  106 | `			sxu32 nDepth = 1;` |
|         - |  107 | `			/* PHP 8 attribute group '#[ ... ]': skip the whole balanced group as` |
|         - |  108 | `			 * trivia (attributes are not stored yet). Brackets inside string` |
|         - |  109 | `			 * literals and comments must not affect the depth count. An` |
|         - |  110 | `			 * unterminated group is silently consumed up to EOF, consistent` |
|         - |  111 | `			 * with unterminated block comments below.` |
|         - |  112 | `			 */` |
|         - |  113 | `			const unsigned char *zGroupStart;` |
|      7789 |  114 | `			pStream->zText += 2;` |
|      7789 |  115 | `			zGroupStart = pStream->zText;` |
|    636329 |  116 | `			while( pStream->zText < pStream->zEnd && nDepth > 0 ){` |
|    628545 |  117 | `				sxi32 d = pStream->zText[0];` |
|    628545 |  118 | `				if( d == '[' ){` |
|        11 |  119 | `					nDepth++;` |
|    628540 |  120 | `				}else if( d == ']' ){` |
|      7799 |  121 | `					nDepth--;` |
|    624638 |  122 | `				}else if( d == '\'' \|\| d == '"' ){` |
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
|    620721 |  145 | `				}else if( d == '#' \|\| (d == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '/') ){` |
|         - |  146 | `					/* Inline comment inside the group */` |
|       ! 0 |  147 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] != '\n' ){` |
|       ! 0 |  148 | `						pStream->zText++;` |
|       ! 0 |  149 | `					}` |
|       ! 0 |  150 | `					continue; /* Let the outer loop count the newline */` |
|    620701 |  151 | `				}else if( d == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '*' ){` |
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
|    620701 |  165 | `				}else if( d == '\n' ){` |
|         7 |  166 | `					pStream->nLine++;` |
|         3 |  167 | `				}` |
|    628545 |  168 | `				pStream->zText++;` |
|         5 |  169 | `			}` |
|      7789 |  170 | `			if( pUserData && pStream->pSet ){` |
|         - |  171 | `				/* Record the group's inner span (between #[ and its balanced ])` |
|         - |  172 | `				 * in the trivia sidecar, keyed like doc-comments. */` |
|         - |  173 | `				ph7_trivia sTrivia;` |
|      7789 |  174 | `				const unsigned char *zGroupEnd = pStream->zText;` |
|      7789 |  175 | `				if( nDepth == 0 && zGroupEnd > zGroupStart ){` |
|      7789 |  176 | `					zGroupEnd--; /* Exclude the closing ']' */` |
|      3892 |  177 | `				}` |
|      7789 |  178 | `				sTrivia.nTokIdx = SySetUsed(pStream->pSet);` |
|      7789 |  179 | `				sTrivia.iKind = PH7_TRIVIA_ATTR;` |
|      7789 |  180 | `				SyStringInitFromBuf(&sTrivia.sText,(const char *)zGroupStart,(sxu32)(zGroupEnd - zGroupStart));` |
|      7789 |  181 | `				sTrivia.nLine = pToken->nLine;` |
|      7789 |  182 | `				SySetPut((SySet *)pUserData,(const void *)&sTrivia);` |
|      3892 |  183 | `			}` |
|         - |  184 | `			/* Tell the upper-layer to ignore this token */` |
|      7789 |  185 | `			return SXERR_CONTINUE;` |
| 108641671 |  186 | `		}else if( pStream->zText[0] == '#' \|\|` |
| 108536908 |  187 | `			( pStream->zText[0] == '/' &&  &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '/') ){` |
|      6915 |  188 | `				pStream->zText++;` |
|         - |  189 | `				/* Inline comments */` |
|    294111 |  190 | `				while( pStream->zText < pStream->zEnd && pStream->zText[0] != '\n' ){` |
|    287201 |  191 | `					pStream->zText++;` |
|         5 |  192 | `				}` |
|         - |  193 | `				/* Tell the upper-layer to ignore this token */` |
|      6915 |  194 | `				return SXERR_CONTINUE;` |
| 108530009 |  195 | `		}else if( pStream->zText[0] == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '*' ){` |
|         - |  196 | `			/* A doc-comment starts with slash-star-star followed by more` |
|         - |  197 | `			 * content (slash-star-star-slash is the empty comment, not a` |
|         - |  198 | `			 * docblock). Its full span, delimiters included, goes to the` |
|         - |  199 | `			 * trivia sidecar when the caller supplied one — keyed by the` |
|         - |  200 | `			 * index the NEXT real token receives — and never enters the` |
|         - |  201 | `			 * token stream. */` |
|    198687 |  202 | `			const unsigned char *zDocStart = pStream->zText;` |
|    198699 |  203 | `			int bDoc = ( &pStream->zText[2] < pStream->zEnd && pStream->zText[2] == '*'` |
|    298035 |  204 | `			 && ( &pStream->zText[3] >= pStream->zEnd \|\| pStream->zText[3] != '/' ) );` |
|    198687 |  205 | `			pStream->zText += 2;` |
|         - |  206 | `			/* Block comment */` |
|  13637979 |  207 | `			while( pStream->zText < pStream->zEnd ){` |
|  13637979 |  208 | `				if( pStream->zText[0] == '*' ){` |
|    263751 |  209 | `					if( &pStream->zText[1] >= pStream->zEnd \|\| pStream->zText[1] == '/'  ){` |
|     99346 |  210 | `						break;` |
|         - |  211 | `					}` |
|     32532 |  212 | `				}` |
|  13439297 |  213 | `				if( pStream->zText[0] == '\n' ){` |
|       165 |  214 | `					pStream->nLine++;` |
|        80 |  215 | `				}` |
|  13439297 |  216 | `				pStream->zText++;` |
|         5 |  217 | `			}` |
|    198687 |  218 | `			pStream->zText += 2;` |
|    198687 |  219 | `			if( bDoc && pUserData && pStream->pSet ){` |
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
|    198687 |  232 | `			return SXERR_CONTINUE;` |
| 108331327 |  233 | `		}else if( SyisDigit(pStream->zText[0]) ){` |
|   3550299 |  234 | `			pStream->zText++;` |
|         - |  235 | `			/* PHP 7.4: handle underscore separator immediately following the first digit.` |
|         - |  236 | `			 * Check pStream->zText < pStream->zEnd BEFORE forming pStream->zText + 1 so` |
|         - |  237 | `			 * we never compute a pointer past one-past-end. */` |
|   3550294 |  238 | `			if( pStream->zText < pStream->zEnd` |
|   3550294 |  239 | `				&& pStream->zText[0] == '_'` |
|   1775227 |  240 | `				&& pStream->zText + 1 < pStream->zEnd` |
|       160 |  241 | `				&& pStream->zText[1] < 0xc0` |
|       165 |  242 | `				&& SyisDigit(pStream->zText[1]) ){` |
|       151 |  243 | `				pStream->zText++; /* swallow underscore between two digits */` |
|        75 |  244 | `			}` |
|         - |  245 | `			/* Decimal digit stream (PHP 7.4: underscore separator allowed between two digits) */` |
|   4385235 |  246 | `			while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|    834941 |  247 | `				pStream->zText++;` |
|    834936 |  248 | `				if( pStream->zText < pStream->zEnd` |
|    834936 |  249 | `					&& pStream->zText[0] == '_'` |
|    417554 |  250 | `					&& pStream->zText + 1 < pStream->zEnd` |
|       172 |  251 | `					&& pStream->zText[1] < 0xc0` |
|       177 |  252 | `					&& SyisDigit(pStream->zText[1]) ){` |
|       173 |  253 | `					pStream->zText++; /* swallow underscore between two digits */` |
|        86 |  254 | `				}` |
|         5 |  255 | `			}` |
|         - |  256 | `			/* Mark the token as integer until we encounter a real number */` |
|   3550299 |  257 | `			pToken->nType = PH7_TK_INTEGER;` |
|   3550299 |  258 | `			if( pStream->zText < pStream->zEnd ){` |
|   3550299 |  259 | `				c = pStream->zText[0];` |
|   3550299 |  260 | `				if( c == '.' ){` |
|         - |  261 | `					/* Real number (PHP 7.4: underscore separator allowed between two digits) */` |
|      8489 |  262 | `					pStream->zText++;` |
|     18427 |  263 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|      9943 |  264 | `						pStream->zText++;` |
|      9938 |  265 | `						if( pStream->zText < pStream->zEnd` |
|      9938 |  266 | `							&& pStream->zText[0] == '_'` |
|      4975 |  267 | `							&& pStream->zText + 1 < pStream->zEnd` |
|        12 |  268 | `							&& pStream->zText[1] < 0xc0` |
|        17 |  269 | `							&& SyisDigit(pStream->zText[1]) ){` |
|        13 |  270 | `							pStream->zText++;` |
|         6 |  271 | `						}` |
|         5 |  272 | `					}` |
|      8489 |  273 | `					if( pStream->zText < pStream->zEnd ){` |
|      8489 |  274 | `						c = pStream->zText[0];` |
|      8489 |  275 | `						if( c=='e' \|\| c=='E' ){` |
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
|      4242 |  295 | `					}` |
|      8489 |  296 | `					pToken->nType = PH7_TK_REAL;` |
|   3546057 |  297 | `				}else if( c=='e' \|\| c=='E' ){` |
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
|   3541761 |  319 | `				}else if( c == 'x' \|\| c == 'X' ){` |
|         - |  320 | `					/* Hex digit stream (PHP 7.4: underscore separator allowed between two digits) */` |
|    103129 |  321 | `					pStream->zText++;` |
|    447047 |  322 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisHex(pStream->zText[0]) ){` |
|    343923 |  323 | `						pStream->zText++;` |
|    343918 |  324 | `						if( pStream->zText < pStream->zEnd` |
|    343918 |  325 | `							&& pStream->zText[0] == '_'` |
|    171983 |  326 | `							&& pStream->zText + 1 < pStream->zEnd` |
|        48 |  327 | `							&& pStream->zText[1] < 0xc0` |
|        53 |  328 | `							&& SyisHex(pStream->zText[1]) ){` |
|        49 |  329 | `							pStream->zText++;` |
|        24 |  330 | `						}` |
|         5 |  331 | `					}` |
|   3490145 |  332 | `				}else if(c  == 'b' \|\| c == 'B' ){` |
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
|   1775147 |  345 | `			}` |
|         - |  346 | `			/* PHP 7.4: absorb a trailing malformed underscore run into the` |
|         - |  347 | `			 * numeric token so the compile phase can emit a PHP-compatible` |
|         - |  348 | `			 * "syntax error, unexpected identifier" parse error. Valid` |
|         - |  349 | `			 * separators were already consumed by the per-loop peek logic` |
|         - |  350 | `			 * above, so an underscore here is always misplaced. */` |
|   3550299 |  351 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '_' ){` |
|        18 |  352 | `				pStream->zText++;` |
|        44 |  353 | `				while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0` |
|        49 |  354 | `					&& (SyisAlphaNum(pStream->zText[0]) \|\| pStream->zText[0] == '_') ){` |
|        20 |  355 | `					pStream->zText++;` |
|         4 |  356 | `				}` |
|         7 |  357 | `			}` |
|         - |  358 | `			/* Record token length */` |
|   3550299 |  359 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|   3550299 |  360 | `			return SXRET_OK;` |
|         - |  361 | `		}` |
| 104781033 |  362 | `		c = pStream->zText[0];` |
| 104781033 |  363 | `		pStream->zText++; /* Advance the stream cursor */` |
|         - |  364 | `		/* Assume we are dealing with an operator*/` |
| 104781033 |  365 | `		pToken->nType = PH7_TK_OP;` |
| 104781033 |  366 | `		switch(c){` |
|  21687919 |  367 | `		case '$': pToken->nType = PH7_TK_DOLLAR; break;` |
|   6396359 |  368 | `		case '{': pToken->nType = PH7_TK_OCB;    break;` |
|   6396345 |  369 | `		case '}': pToken->nType = PH7_TK_CCB;    break;` |
|  13406671 |  370 | `		case '(': pToken->nType = PH7_TK_LPAREN; break;` |
|   3062247 |  371 | `		case '[': pToken->nType \|= PH7_TK_OSB;   break; /* Bitwise operation here,since the square bracket token '['` |
|         - |  372 | `														 * is a potential operator [i.e: subscripting] */` |
|   3062253 |  373 | `		case ']': pToken->nType = PH7_TK_CSB;    break;` |
|   6703321 |  374 | `		case ')': {` |
|  13406647 |  375 | `			SySet *pTokSet = pStream->pSet;` |
|         - |  376 | `			/* Assemble type cast operators [i.e: (int),(float),(bool)...] */` |
|  13406647 |  377 | `			if( pTokSet->nUsed >= 2 ){` |
|         - |  378 | `				SyToken *pTmp;` |
|         - |  379 | `				/* Peek the last recongnized token */` |
|  13406645 |  380 | `				pTmp = (SyToken *)SySetPeek(pTokSet);` |
|  13406645 |  381 | `				if( pTmp->nType & PH7_TK_KEYWORD ){` |
|    992945 |  382 | `					sxi32 nID = SX_PTR_TO_INT(pTmp->pUserData);` |
|    992945 |  383 | `					if( (sxu32)nID & (PH7_TKWRD_ARRAY\|PH7_TKWRD_INT\|PH7_TKWRD_FLOAT\|PH7_TKWRD_STRING\|PH7_TKWRD_OBJECT\|PH7_TKWRD_BOOL\|PH7_TKWRD_UNSET) ){` |
|    881989 |  384 | `						pTmp = (SyToken *)SySetAt(pTokSet,pTokSet->nUsed - 2);` |
|    881989 |  385 | `						if( pTmp->nType & PH7_TK_LPAREN ){` |
|         - |  386 | `							/* Merge the three tokens '(' 'TYPE' ')' into a single one */` |
|    565177 |  387 | `							const char * zTypeCast = "(int)";` |
|    565177 |  388 | `							if( nID & PH7_TKWRD_FLOAT ){` |
|     30555 |  389 | `								zTypeCast = "(float)";` |
|    549902 |  390 | `							}else if( nID & PH7_TKWRD_BOOL ){` |
|      3857 |  391 | `								zTypeCast = "(bool)";` |
|    532701 |  392 | `							}else if( nID & PH7_TKWRD_STRING ){` |
|    229041 |  393 | `								zTypeCast = "(string)";` |
|    416257 |  394 | `							}else if( nID & PH7_TKWRD_ARRAY ){` |
|        29 |  395 | `								zTypeCast = "(array)";` |
|    301725 |  396 | `							}else if( nID & PH7_TKWRD_OBJECT ){` |
|        35 |  397 | `								zTypeCast = "(object)";` |
|    301694 |  398 | `							}else if( nID & PH7_TKWRD_UNSET ){` |
|         3 |  399 | `								zTypeCast = "(unset)";` |
|         1 |  400 | `							}` |
|         - |  401 | `							/* Reflect the change */` |
|    565177 |  402 | `							pToken->nType = PH7_TK_OP;` |
|    565177 |  403 | `							SyStringInitFromBuf(&pToken->sData,zTypeCast,SyStrlen(zTypeCast));` |
|         - |  404 | `							/* Save the instance associated with the type cast operator */` |
|    565177 |  405 | `							pToken->pUserData = (void *)PH7_ExprExtractOperator(&pToken->sData,0);` |
|         - |  406 | `							/* Remove the two previous tokens */` |
|    565177 |  407 | `							pTokSet->nUsed -= 2;` |
|    565177 |  408 | `							return SXRET_OK;` |
|         - |  409 | `						}` |
|    158406 |  410 | `					}` |
|    213884 |  411 | `				}` |
|   6420734 |  412 | `			}` |
|  12841475 |  413 | `			pToken->nType = PH7_TK_RPAREN;` |
|  12841475 |  414 | `			break;` |
|         - |  415 | `				  }` |
|   2511581 |  416 | `		case '\'':{` |
|         - |  417 | `			/* Single quoted string */` |
|   5023167 |  418 | `			pStr->zString++;` |
|  58626741 |  419 | `			while( pStream->zText < pStream->zEnd ){` |
|  58626741 |  420 | `				if( pStream->zText[0] == '\''  ){` |
|   5023177 |  421 | `					if( pStream->zText[-1] != '\\' ){` |
|   4992617 |  422 | `						break;` |
|       ! 0 |  423 | `					}else{` |
|     30565 |  424 | `						const unsigned char *zPtr = &pStream->zText[-2];` |
|     30565 |  425 | `						sxi32 i = 1;` |
|     61119 |  426 | `						while( zPtr > pStream->zInput && zPtr[0] == '\\' ){` |
|     30559 |  427 | `							zPtr--;` |
|     30559 |  428 | `							i++;` |
|         5 |  429 | `						}` |
|     30565 |  430 | `						if((i&1)==0){` |
|     30555 |  431 | `							break;` |
|         - |  432 | `						}` |
|         - |  433 | `					}` |
|         5 |  434 | `				}` |
|  53603579 |  435 | `				if( pStream->zText[0] == '\n' ){` |
|        67 |  436 | `					pStream->nLine++;` |
|        33 |  437 | `				}` |
|  53603579 |  438 | `				pStream->zText++;` |
|         5 |  439 | `			}` |
|         - |  440 | `			/* Record token length and type */` |
|   5023167 |  441 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|   5023167 |  442 | `			pToken->nType = PH7_TK_SSTR;` |
|         - |  443 | `			/* Jump the trailing single quote */` |
|   5023167 |  444 | `			pStream->zText++;` |
|   5023167 |  445 | `			return SXRET_OK;` |
|         - |  446 | `				  }` |
|     59940 |  447 | `		case '"':{` |
|         - |  448 | `			sxi32 iNest;` |
|         - |  449 | `			/* Double quoted string */` |
|    119885 |  450 | `			pStr->zString++;` |
|   1706353 |  451 | `			while( pStream->zText < pStream->zEnd ){` |
|   1706353 |  452 | `				if( pStream->zText[0] == '{' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '$'){` |
|       115 |  453 | `					iNest = 1;` |
|       115 |  454 | `					pStream->zText++;` |
|         - |  455 | `					/* TICKET 1433-40: Hnadle braces'{}' in double quoted string where everything is allowed */` |
|      1129 |  456 | `					while(pStream->zText < pStream->zEnd ){` |
|      1129 |  457 | `						if( pStream->zText[0] == '{' ){` |
|         3 |  458 | `							iNest++;` |
|      1128 |  459 | `						}else if (pStream->zText[0] == '}' ){` |
|       117 |  460 | `							iNest--;` |
|       117 |  461 | `							if( iNest <= 0 ){` |
|       115 |  462 | `								pStream->zText++;` |
|       115 |  463 | `								break;` |
|         1 |  464 | `							}` |
|      1014 |  465 | `						}else if( pStream->zText[0] == '\n' ){` |
|       ! 0 |  466 | `							pStream->nLine++;` |
|       ! 0 |  467 | `						}` |
|      1017 |  468 | `						pStream->zText++;` |
|         3 |  469 | `					}` |
|       115 |  470 | `					if( pStream->zText >= pStream->zEnd ){` |
|       ! 0 |  471 | `						break;` |
|         - |  472 | `					}` |
|        56 |  473 | `				}` |
|   1706353 |  474 | `				if( pStream->zText[0] == '"' ){` |
|    120163 |  475 | `					if( pStream->zText[-1] != '\\' ){` |
|    119877 |  476 | `						break;` |
|       ! 0 |  477 | `					}else{` |
|       291 |  478 | `						const unsigned char *zPtr = &pStream->zText[-2];` |
|       291 |  479 | `						sxi32 i = 1;` |
|       331 |  480 | `						while( zPtr > pStream->zInput && zPtr[0] == '\\' ){` |
|        43 |  481 | `							zPtr--;` |
|        43 |  482 | `							i++;` |
|         3 |  483 | `						}` |
|       291 |  484 | `						if((i&1)==0){` |
|         9 |  485 | `							break;` |
|         - |  486 | `						}` |
|         - |  487 | `					}` |
|       139 |  488 | `				}` |
|   1586473 |  489 | `				if( pStream->zText[0] == '\n' ){` |
|        29 |  490 | `					pStream->nLine++;` |
|        14 |  491 | `				}` |
|   1586473 |  492 | `				pStream->zText++;` |
|         5 |  493 | `			}` |
|         - |  494 | `			/* Record token length and type */` |
|    119885 |  495 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|    119885 |  496 | `			pToken->nType = PH7_TK_DSTR;` |
|         - |  497 | `			/* Jump the trailing quote */` |
|    119885 |  498 | `			pStream->zText++;` |
|    119885 |  499 | `			return SXRET_OK;` |
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
|      8529 |  520 | `		case '\\': pToken->nType = PH7_TK_NSSEP;  break;` |
|    445399 |  521 | `		case ':':` |
|    890803 |  522 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == ':' ){` |
|         - |  523 | `				/* Current operator: '::' */` |
|    367259 |  524 | `				pStream->zText++;` |
|    183632 |  525 | `			}else{` |
|    523549 |  526 | `				pToken->nType = PH7_TK_COLON; /* Single colon */` |
|         - |  527 | `			}` |
|    890803 |  528 | `			break;` |
|   3635787 |  529 | `		case ',': pToken->nType \|= PH7_TK_COMMA;  break; /* Comma is also an operator */` |
|  10145253 |  530 | `		case ';': pToken->nType = PH7_TK_SEMI;    break;` |
|         - |  531 | `			/* Handle combined operators [i.e: +=,===,!=== ...] */` |
|   3649441 |  532 | `		case '=':` |
|   7298887 |  533 | `			pToken->nType \|= PH7_TK_EQUAL;` |
|   7298887 |  534 | `			if( pStream->zText < pStream->zEnd ){` |
|   7298887 |  535 | `				if( pStream->zText[0] == '=' ){` |
|   1249585 |  536 | `					pToken->nType &= ~PH7_TK_EQUAL;` |
|         - |  537 | `					/* Current operator: == */` |
|   1249585 |  538 | `					pStream->zText++;` |
|   1249585 |  539 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  540 | `						/* Current operator: === */` |
|   1199619 |  541 | `						pStream->zText++;` |
|    599812 |  542 | `					}` |
|   6674097 |  543 | `				}else if( pStream->zText[0] == '>' ){` |
|         - |  544 | `					/* Array operator: => */` |
|    518257 |  545 | `					pToken->nType = PH7_TK_ARRAY_OP;` |
|    518257 |  546 | `					pStream->zText++;` |
|    259131 |  547 | `				}else{` |
|         - |  548 | `					/* TICKET 1433-0010: Reference operator '=&' */` |
|   5531055 |  549 | `					const unsigned char *zCur = pStream->zText;` |
|   5531055 |  550 | `					sxu32 nLine = 0;` |
|  11061941 |  551 | `					while( zCur < pStream->zEnd && zCur[0] < 0xc0 && SyisSpace(zCur[0]) ){` |
|   5530891 |  552 | `						if( zCur[0] == '\n' ){` |
|         5 |  553 | `							nLine++;` |
|         2 |  554 | `						}` |
|   5530891 |  555 | `						zCur++;` |
|         5 |  556 | `					}` |
|   5531055 |  557 | `					if( zCur < pStream->zEnd && zCur[0] == '&' ){` |
|         - |  558 | `						/* Current operator: =& */` |
|        66 |  559 | `						pToken->nType &= ~PH7_TK_EQUAL;` |
|        66 |  560 | `						SyStringInitFromBuf(pStr,"=&",sizeof("=&")-1);` |
|         - |  561 | `						/* Update token stream */` |
|        66 |  562 | `						pStream->zText = &zCur[1];` |
|        66 |  563 | `						pStream->nLine += nLine;` |
|        31 |  564 | `					}` |
|         - |  565 | `				}` |
|   3649441 |  566 | `			}` |
|   7298887 |  567 | `			break;` |
|    420172 |  568 | `		case '!':` |
|    840349 |  569 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  570 | `				/* Current operator: != */` |
|    397093 |  571 | `				pStream->zText++;` |
|    397093 |  572 | `				if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  573 | `					/* Current operator: !== */` |
|    381795 |  574 | `					pStream->zText++;` |
|    190895 |  575 | `				}` |
|    198544 |  576 | `			}` |
|    840349 |  577 | `			break;` |
|    313295 |  578 | `		case '&':` |
|    626595 |  579 | `			pToken->nType \|= PH7_TK_AMPER;` |
|    626595 |  580 | `			if( pStream->zText < pStream->zEnd ){` |
|    626595 |  581 | `				if( pStream->zText[0] == '&' ){` |
|    412639 |  582 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|         - |  583 | `					/* Current operator: && */` |
|    412639 |  584 | `					pStream->zText++;` |
|    420278 |  585 | `				}else if( pStream->zText[0] == '=' ){` |
|         7 |  586 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|         - |  587 | `					/* Current operator: &= */` |
|         7 |  588 | `					pStream->zText++;` |
|         3 |  589 | `				}` |
|    313295 |  590 | `			}` |
|    626595 |  591 | `			break;` |
|    177648 |  592 | `		case '\|':` |
|    355301 |  593 | `			if( pStream->zText < pStream->zEnd ){` |
|    355301 |  594 | `				if( pStream->zText[0] == '\|' ){` |
|         - |  595 | `					/* Current operator: \|\| */` |
|    282543 |  596 | `					pStream->zText++;` |
|    214032 |  597 | `				}else if( pStream->zText[0] == '=' ){` |
|         - |  598 | `					/* Current operator: \|= */` |
|     57251 |  599 | `					pStream->zText++;` |
|     44140 |  600 | `				}else if( pStream->zText[0] == '>' ){` |
|         - |  601 | `					/* Current operator: \|> (PHP 8.5 pipe) */` |
|        27 |  602 | `					pStream->zText++;` |
|        13 |  603 | `				}` |
|    177648 |  604 | `			}` |
|    355301 |  605 | `			break;` |
|    206672 |  606 | `		case '+':` |
|    413349 |  607 | `			if( pStream->zText < pStream->zEnd ){` |
|    413347 |  608 | `				if( pStream->zText[0] == '+' ){` |
|         - |  609 | `					/* Current operator: ++ */` |
|    164551 |  610 | `					pStream->zText++;` |
|    331074 |  611 | `				}else if( pStream->zText[0] == '=' ){` |
|         - |  612 | `					/* Current operator: += */` |
|     45871 |  613 | `					pStream->zText++;` |
|     22933 |  614 | `				}` |
|    206671 |  615 | `			}` |
|    413349 |  616 | `			break;` |
|   2411430 |  617 | `		case '-':` |
|   4822865 |  618 | `			if( pStream->zText < pStream->zEnd ){` |
|   4822865 |  619 | `				if( pStream->zText[0] == '-' ){` |
|         - |  620 | `					/* Current operator: -- */` |
|     30575 |  621 | `					pStream->zText++;` |
|   4807580 |  622 | `				}else if( pStream->zText[0] == '=' ){` |
|         - |  623 | `					/* Current operator: -= */` |
|        14 |  624 | `					pStream->zText++;` |
|   4792289 |  625 | `				}else if( pStream->zText[0] == '>' ){` |
|         - |  626 | `					/* Current operator: -> */` |
|   4573891 |  627 | `					pStream->zText++;` |
|   2286943 |  628 | `				}` |
|   2411430 |  629 | `			}` |
|   4822865 |  630 | `			break;` |
|     19288 |  631 | `		case '*':` |
|     38581 |  632 | `			if( pStream->zText < pStream->zEnd ){` |
|     38581 |  633 | `				if( pStream->zText[0] == '*' ){` |
|         - |  634 | `					/* Current operator: ** or **= */` |
|       137 |  635 | `					pStream->zText++;` |
|       137 |  636 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  637 | `						/* Current operator: **= */` |
|        23 |  638 | `						pStream->zText++;` |
|        12 |  639 | `					}` |
|     38513 |  640 | `				}else if( pStream->zText[0] == '=' ){` |
|         - |  641 | `					/* Current operator: *= */` |
|        27 |  642 | `					pStream->zText++;` |
|        12 |  643 | `				}` |
|     19288 |  644 | `			}` |
|     38581 |  645 | `			break;` |
|      1959 |  646 | `		case '/':` |
|      3923 |  647 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  648 | `				/* Current operator: /= */` |
|         9 |  649 | `				pStream->zText++;` |
|         4 |  650 | `			}` |
|      3923 |  651 | `			break;` |
|     17220 |  652 | `		case '%':` |
|     34445 |  653 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  654 | `				/* Current operator: %= */` |
|         9 |  655 | `				pStream->zText++;` |
|         4 |  656 | `			}` |
|     34445 |  657 | `			break;` |
|        11 |  658 | `		case '^':` |
|        23 |  659 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  660 | `				/* Current operator: ^= */` |
|         9 |  661 | `				pStream->zText++;` |
|         4 |  662 | `			}` |
|        23 |  663 | `			break;` |
|    964210 |  664 | `		case '.':` |
|   1928425 |  665 | `			if( pStream->zText + 1 < pStream->zEnd && pStream->zText[0] == '.' && pStream->zText[1] == '.' ){` |
|         - |  666 | `				/* Ellipsis: ... */` |
|     27155 |  667 | `				pStream->zText += 2;` |
|     27155 |  668 | `				pToken->nType = PH7_TK_ELLIPSIS;` |
|   1914850 |  669 | `			}else if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  670 | `				/* Current operator: .= */` |
|    332143 |  671 | `				pStream->zText++;` |
|    166069 |  672 | `			}` |
|   1928425 |  673 | `			break;` |
|    179688 |  674 | `		case '<':` |
|    359381 |  675 | `			if( pStream->zText < pStream->zEnd ){` |
|    359381 |  676 | `				if( pStream->zText[0] == '<' ){` |
|         - |  677 | `					/* Current operator: << */` |
|       144 |  678 | `					pStream->zText++;` |
|       144 |  679 | `					if( pStream->zText < pStream->zEnd ){` |
|       144 |  680 | `						if( pStream->zText[0] == '=' ){` |
|         - |  681 | `							/* Current operator: <<= */` |
|         9 |  682 | `							pStream->zText++;` |
|       140 |  683 | `						}else if( pStream->zText[0] == '<' ){` |
|         - |  684 | `							/* Current Token: <<<  */` |
|       122 |  685 | `							pStream->zText++;` |
|         - |  686 | `							/* This may be the beginning of a Heredoc/Nowdoc string,try to delimit it */` |
|       122 |  687 | `							rc = LexExtractHeredoc(&(*pStream),&(*pToken));` |
|       122 |  688 | `							if( rc == SXRET_OK ){` |
|         - |  689 | `								/* Here/Now doc successfuly extracted */` |
|       122 |  690 | `								return SXRET_OK;` |
|         - |  691 | `							}` |
|       ! 0 |  692 | `						}` |
|        12 |  693 | `					}` |
|    359252 |  694 | `				}else if( pStream->zText[0] == '>' ){` |
|         - |  695 | `					/* Current operator: <> */` |
|         5 |  696 | `					pStream->zText++;` |
|    359239 |  697 | `				}else if( pStream->zText[0] == '=' ){` |
|         - |  698 | `					/* Current operator: <= or <=> */` |
|     65009 |  699 | `					pStream->zText++;` |
|     65009 |  700 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '>' ){` |
|         - |  701 | `						/* Current operator: <=> */` |
|     26787 |  702 | `						pStream->zText++;` |
|     13391 |  703 | `					}` |
|     32502 |  704 | `				}` |
|    179629 |  705 | `			}` |
|    359263 |  706 | `			break;` |
|    147073 |  707 | `		case '>':` |
|    294151 |  708 | `			if( pStream->zText < pStream->zEnd ){` |
|    294151 |  709 | `				if( pStream->zText[0] == '>' ){` |
|         - |  710 | `					/* Current operator: >> */` |
|     19105 |  711 | `					pStream->zText++;` |
|     19105 |  712 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  713 | `						/* Current operator: >>= */` |
|        11 |  714 | `						pStream->zText++;` |
|        10 |  715 | `					}` |
|    284601 |  716 | `				}else if( pStream->zText[0] == '=' ){` |
|         - |  717 | `					/* Current operator: >= */` |
|     99323 |  718 | `					pStream->zText++;` |
|     49659 |  719 | `				}` |
|    147073 |  720 | `			}` |
|    294151 |  721 | `			break;` |
|    255657 |  722 | `		case '?':` |
|    511319 |  723 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '?' ){` |
|         - |  724 | `				/* Null coalescing operator: ?? */` |
|     57495 |  725 | `				pStream->zText++;` |
|     57495 |  726 | `				if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|         - |  727 | `					/* Null coalescing assignment operator (PHP 7.4) */` |
|        99 |  728 | `					pStream->zText++;` |
|        47 |  729 | `				}` |
|    482574 |  730 | `			}else if( (pStream->zEnd - pStream->zText) >= 2` |
|    453829 |  731 | `				&& pStream->zText[0] == '-' && pStream->zText[1] == '>' ){` |
|         - |  732 | `				/* Nullsafe object operator (PHP 8.0): ?-> */` |
|       119 |  733 | `				pStream->zText += 2;` |
|        57 |  734 | `			}` |
|    511314 |  735 | `			break;` |
|      5848 |  736 | `		default:` |
|     11696 |  737 | `			break;` |
|         - |  738 | `		}` |
|  99072697 |  739 | `		if( pStr->nByte <= 0 ){` |
|         - |  740 | `			/* Record token length */` |
|  99072635 |  741 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  49536315 |  742 | `		}` |
|  99072697 |  743 | `		if( pToken->nType & PH7_TK_OP ){` |
|         - |  744 | `			const ph7_expr_op *pOp;` |
|         - |  745 | `			/* Check if the extracted token is an operator */` |
|  24058987 |  746 | `			pOp = PH7_ExprExtractOperator(pStr,(SyToken *)SySetPeek(pStream->pSet));` |
|  24058987 |  747 | `			if( pOp == 0 ){` |
|         - |  748 | `				/* Not an operator */` |
|       ! 0 |  749 | `				pToken->nType &= ~PH7_TK_OP;` |
|       ! 0 |  750 | `				if( pToken->nType <= 0 ){` |
|       ! 0 |  751 | `					pToken->nType = PH7_TK_OTHER;` |
|       ! 0 |  752 | `				}` |
|       ! 0 |  753 | `			}else{` |
|         - |  754 | `				/* Save the instance associated with this operator for later processing */` |
|  24058987 |  755 | `				pToken->pUserData = (void *)pOp;` |
|         - |  756 | `			}` |
|  12029491 |  757 | `		}` |
|         - |  758 | `	}` |
|         - |  759 | `	/* Tell the upper-layer to save the extracted token for later processing */` |
| 152916757 |  760 | `	return SXRET_OK;` |
|  81194384 |  761 | `}` |
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
|  53844065 |  781 | `static sxu32 KeywordCode(const char *z, int n){` |
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
|  53844065 |  871 | `  if( n<2 ) return PH7_TK_ID;` |
|  46906145 |  872 | `  h = (((int)z[0]*4) ^ ((int)z[n-1]*3) ^ n) % 151;` |
|  72447487 |  873 | `  for(i=((int)aHash[h])-1; i>=0; i=((int)aNext[i])-1){` |
|  43407975 |  874 | `    if( (int)aLen[i]==n && SyMemcmp(&zText[aOffset[i]],z,n)==0 ){` |
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
|  17866633 |  959 | `      return aCode[i];` |
|         - |  960 | `    }` |
|  12770674 |  961 | `  }` |
|         - |  962 | `  /* Linear fallback for keywords not in the auto-generated hash table */` |
|  29039517 |  963 | `  if( n==5 && SyMemcmp(z,"trait",5)==0 ) return PH7_TKWRD_TRAIT;` |
|  29031805 |  964 | `  if( n==9 && SyMemcmp(z,"insteadof",9)==0 ) return PH7_TKWRD_INSTEADOF;` |
|  29031801 |  965 | `  if( n==7 && SyMemcmp(z,"finally",7)==0 ) return PH7_TKWRD_FINALLY;` |
|  29031629 |  966 | `  if( n==5 && SyMemcmp(z,"yield",5)==0 ) return PH7_TKWRD_YIELD;` |
|  29015977 |  967 | `  if( n==5 && SyMemcmp(z,"match",5)==0 ) return PH7_TKWRD_MATCH;` |
|  29015901 |  968 | `  return PH7_TK_ID;` |
|  26922035 |  969 | `}` |
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
|         4 | 1005 | `{` |
|       122 | 1006 | `	const unsigned char *zIn  = pStream->zText;` |
|       122 | 1007 | `	const unsigned char *zEnd = pStream->zEnd;` |
|         - | 1008 | `	const unsigned char *zPtr;` |
|       122 | 1009 | `	sxu8 bNowDoc = FALSE;` |
|         - | 1010 | `	SyString sDelim;` |
|         - | 1011 | `	SyString sStr;` |
|         - | 1012 | `	/* Jump leading white spaces */` |
|       134 | 1013 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|        13 | 1014 | `		zIn++;` |
|         1 | 1015 | `	}` |
|       122 | 1016 | `	if( zIn >= zEnd ){` |
|         - | 1017 | `		/* A simple symbol,return immediately */` |
|       ! 0 | 1018 | `		return SXERR_CONTINUE;` |
|         - | 1019 | `	}` |
|       122 | 1020 | `	if( zIn[0] == '\'' \|\| zIn[0] == '"' ){` |
|         - | 1021 | `		/* Make sure we are dealing with a nowdoc */` |
|        52 | 1022 | `		bNowDoc =  zIn[0] == '\'' ? TRUE : FALSE;` |
|        52 | 1023 | `		zIn++;` |
|        24 | 1024 | `	}` |
|       122 | 1025 | `	if( zIn[0] < 0xc0 && !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|         - | 1026 | `		/* Invalid delimiter,return immediately */` |
|       ! 0 | 1027 | `		return SXERR_CONTINUE;` |
|         - | 1028 | `	}` |
|         - | 1029 | `	/* Isolate the identifier */` |
|       122 | 1030 | `	sDelim.zString = (const char *)zIn;` |
|       126 | 1031 | `	for(;;){` |
|       256 | 1032 | `		zPtr = zIn;` |
|         - | 1033 | `		/* Skip alphanumeric stream */` |
|       806 | 1034 | `		while( zPtr < zEnd && zPtr[0] < 0xc0 && (SyisAlphaNum(zPtr[0]) \|\| zPtr[0] == '_') ){` |
|       428 | 1035 | `			zPtr++;` |
|         4 | 1036 | `		}` |
|       256 | 1037 | `		if( zPtr < zEnd && zPtr[0] >= 0xc0 ){` |
|        19 | 1038 | `			zPtr++;` |
|         - | 1039 | `			/* UTF-8 stream */` |
|        37 | 1040 | `			while( zPtr < zEnd && ((zPtr[0] & 0xc0) == 0x80) ){` |
|        19 | 1041 | `				zPtr++;` |
|         1 | 1042 | `			}` |
|         9 | 1043 | `		}` |
|       256 | 1044 | `		if( zPtr == zIn ){` |
|         - | 1045 | `			/* Not an UTF-8 or alphanumeric stream */` |
|       122 | 1046 | `			break;` |
|         - | 1047 | `		}` |
|         - | 1048 | `		/* Synchronize pointers */` |
|       138 | 1049 | `		zIn = zPtr;` |
|         4 | 1050 | `	}` |
|         - | 1051 | `	/* Get the identifier length */` |
|       122 | 1052 | `	sDelim.nByte = (sxu32)((const char *)zIn-sDelim.zString);` |
|       122 | 1053 | `	if( zIn[0] == '"' \|\| (bNowDoc && zIn[0] == '\'') ){` |
|         - | 1054 | `		/* Jump the trailing single quote */` |
|        52 | 1055 | `		zIn++;` |
|        24 | 1056 | `	}` |
|         - | 1057 | `	/* Jump trailing white spaces */` |
|       122 | 1058 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|       ! 0 | 1059 | `		zIn++;` |
|       ! 0 | 1060 | `	}` |
|       122 | 1061 | `	if( sDelim.nByte <= 0 \|\| zIn >= zEnd \|\| zIn[0] != '\n' ){` |
|         - | 1062 | `		/* Invalid syntax */` |
|       ! 0 | 1063 | `		return SXERR_CONTINUE;` |
|         - | 1064 | `	}` |
|       122 | 1065 | `	pStream->nLine++; /* Increment line counter */` |
|       122 | 1066 | `	zIn++;` |
|         - | 1067 | `	/* Isolate the delimited string */` |
|       122 | 1068 | `	sStr.zString = (const char *)zIn;` |
|         - | 1069 | `	/* PHP 7.3 flexible heredoc/nowdoc: the closing marker may be preceded` |
|         - | 1070 | `	 * by whitespace (spaces/tabs), and may be followed by any non-identifier` |
|         - | 1071 | `	 * character. The indent count is recorded in pToken->pUserData and the` |
|         - | 1072 | `	 * compile phase strips it from each body line. */` |
|         - | 1073 | `	{` |
|       122 | 1074 | `		const unsigned char *zMarkerLine = zIn; /* Start of marker's line (set on match) */` |
|       122 | 1075 | `		sxu32 nIndent = 0;` |
|       265 | 1076 | `		for(;;){` |
|       328 | 1077 | `			const unsigned char *zLineStart = zIn;` |
|         - | 1078 | `			/* Skip leading space/tab on this line */` |
|       880 | 1079 | `			while( zIn < zEnd && (zIn[0] == ' ' \|\| zIn[0] == '\t') ){` |
|       393 | 1080 | `				zIn++;` |
|         3 | 1081 | `			}` |
|       324 | 1082 | `			if( (sxu32)(zEnd - zIn) >= sDelim.nByte` |
|       327 | 1083 | `				&& SyMemcmp((const void *)sDelim.zString,(const void *)zIn,sDelim.nByte) == 0 ){` |
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
|      4480 | 1105 | `			while( zIn < zEnd && zIn[0] != '\n' ){` |
|      4272 | 1106 | `				zIn++;` |
|         4 | 1107 | `			}` |
|       212 | 1108 | `			if( zIn >= zEnd ){` |
|         - | 1109 | `				/* End of input without finding the closing marker */` |
|         3 | 1110 | `				pStream->zText = pStream->zEnd;` |
|         3 | 1111 | `				zMarkerLine = zIn;` |
|         3 | 1112 | `				break;` |
|         - | 1113 | `			}` |
|       210 | 1114 | `			pStream->nLine++;` |
|       210 | 1115 | `			zIn++;` |
|         4 | 1116 | `		}` |
|         - | 1117 | `		/* Body runs from sStr.zString up to just before the marker line */` |
|       122 | 1118 | `		sStr.nByte = (sxu32)((const char *)zMarkerLine - sStr.zString);` |
|       122 | 1119 | `		pToken->nType = bNowDoc ? PH7_TK_NOWDOC : PH7_TK_HEREDOC;` |
|       122 | 1120 | `		SyStringDupPtr(&pToken->sData,&sStr);` |
|         - | 1121 | `		/* Strip exactly one line terminator that precedes the marker's line. */` |
|       118 | 1122 | `		if( pToken->sData.nByte > 0` |
|       118 | 1123 | `			&& pToken->sData.zString[pToken->sData.nByte - 1] == '\n' ){` |
|       112 | 1124 | `			pToken->sData.nByte--;` |
|       108 | 1125 | `			if( pToken->sData.nByte > 0` |
|       112 | 1126 | `				&& pToken->sData.zString[pToken->sData.nByte - 1] == '\r' ){` |
|       ! 0 | 1127 | `				pToken->sData.nByte--;` |
|       ! 0 | 1128 | `			}` |
|        54 | 1129 | `		}` |
|       122 | 1130 | `		pToken->pUserData = SX_INT_TO_PTR(nIndent);` |
|         - | 1131 | `	}` |
|         - | 1132 | `	/* All done */` |
|       122 | 1133 | `	return SXRET_OK;` |
|        63 | 1134 | `}` |
|         - | 1135 | `/*` |
|         - | 1136 | ` * Tokenize a raw PHP input.` |
|         - | 1137 | ` * This is the public tokenizer called by most code generator routines.` |
|         - | 1138 | ` */` |
|     81794 | 1139 | `PH7_PRIVATE sxi32 PH7_TokenizePHP(const char *zInput,sxu32 nLen,sxu32 nLineStart,SySet *pOut,SySet *pTrivia)` |
|         5 | 1140 | `{` |
|         - | 1141 | `	SyLex sLexer;` |
|         - | 1142 | `	sxi32 rc;` |
|         - | 1143 | `	/* Defense-in-depth cap for internal tokenizer calls that bypass ph7_compile() */` |
|     81799 | 1144 | `	if( nLen > PH7_MAX_INPUT_SIZE ){` |
|       ! 0 | 1145 | `		return SXERR_LIMIT;` |
|         - | 1146 | `	}` |
|         - | 1147 | `	/* Initialize the lexer. pTrivia (may be NULL = discard) rides as the` |
|         - | 1148 | `	 * tokenizer callback's user data: doc-comments (and later attribute` |
|         - | 1149 | `	 * groups) are recorded there instead of entering the token stream. */` |
|     81799 | 1150 | `	rc = SyLexInit(&sLexer,&(*pOut),TokenizePHP,pTrivia);` |
|     81799 | 1151 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 1152 | `		return rc;` |
|         - | 1153 | `	}` |
|     81799 | 1154 | `	sLexer.sStream.nLine = nLineStart;` |
|         - | 1155 | `	/* Tokenize input */` |
|     81799 | 1156 | `	rc = SyLexTokenizeInput(&sLexer,zInput,nLen,0,0,0);` |
|         - | 1157 | `	/* Release the lexer */` |
|     81799 | 1158 | `	SyLexRelease(&sLexer);` |
|         - | 1159 | `	/* Tokenization result */` |
|     81799 | 1160 | `	return rc;` |
|     40902 | 1161 | `}` |
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
|     13306 | 1207 | `PH7_PRIVATE sxi32 PH7_TokenizeRawText(const char *zInput,sxu32 nLen,SySet *pOut,sxu32 nBaseLine)` |
|         5 | 1208 | `{` |
|     13311 | 1209 | `	const char *zEnd = &zInput[nLen];` |
|     13311 | 1210 | `	const char *zIn  = zInput;` |
|         - | 1211 | `	const char *zCur,*zCurEnd;` |
|     13311 | 1212 | `	SyString sCtag = { 0, 0 };     /* Closing tag */` |
|         - | 1213 | `	SyToken sToken;` |
|         - | 1214 | `	SyString sDoc;` |
|         - | 1215 | `	sxu32 nLine;` |
|         - | 1216 | `	sxi32 iNest;` |
|         - | 1217 | `	sxi32 rc;` |
|         - | 1218 | `	/* Tokenize the input into PHP tokens and raw tokens. nBaseLine is normally 1,` |
|         - | 1219 | `	 * but 2 when a "#!" shebang line was stripped so error lines still match php. */` |
|     13311 | 1220 | `	nLine = nBaseLine;` |
|     13311 | 1221 | `	zCur = zCurEnd   = 0; /* Prevent compiler warning */` |
|     13311 | 1222 | `	sToken.pUserData = 0;` |
|     13311 | 1223 | `	iNest = 0;` |
|     13311 | 1224 | `	sDoc.nByte = 0;` |
|     13311 | 1225 | `	sDoc.zString = ""; /* cc warning */` |
|     13306 | 1226 | `	for(;;){` |
|     26617 | 1227 | `		if( zIn >= zEnd ){` |
|         - | 1228 | `			/* End of input reached */` |
|     13301 | 1229 | `			break;` |
|         - | 1230 | `		}` |
|     13321 | 1231 | `		sToken.nLine = nLine;` |
|     13321 | 1232 | `		zCur = zIn;` |
|     13321 | 1233 | `		zCurEnd = 0;` |
|     13513 | 1234 | `		while( zIn < zEnd ){` |
|     13503 | 1235 | `			 if( zIn[0] == '<' ){` |
|     13311 | 1236 | `				const char *zTmp = zIn; /* End of raw input marker */` |
|     13311 | 1237 | `				zIn++;` |
|     13311 | 1238 | `				if( zIn < zEnd ){` |
|     13311 | 1239 | `					if( zIn[0] == '?' ){` |
|     13311 | 1240 | `						zIn++;` |
|     13311 | 1241 | `						if( (sxu32)(zEnd - zIn) >= sizeof("php")-1 &&  SyStrnicmp(zIn,"php",sizeof("php")-1) == 0 ){` |
|         - | 1242 | `							/* opening tag: <?php */` |
|     13309 | 1243 | `							zIn += sizeof("php")-1;` |
|      6652 | 1244 | `						}` |
|         - | 1245 | `						/* Look for the closing tag '?>' */` |
|     13311 | 1246 | `						SyStringInitFromBuf(&sCtag,"?>",sizeof("?>")-1);` |
|     13311 | 1247 | `						zCurEnd = zTmp;` |
|     13311 | 1248 | `						break;` |
|         - | 1249 | `					}` |
|       ! 0 | 1250 | `				}` |
|       ! 0 | 1251 | `			}else{` |
|       196 | 1252 | `				if( zIn[0] == '\n' ){` |
|         8 | 1253 | `					nLine++;` |
|         3 | 1254 | `				}` |
|       196 | 1255 | `				zIn++;` |
|         - | 1256 | `			 }` |
|         4 | 1257 | `		} /* While(zIn < zEnd) */` |
|     13321 | 1258 | `		if( zCurEnd == 0 ){` |
|        13 | 1259 | `			zCurEnd = zIn;` |
|         5 | 1260 | `		}` |
|         - | 1261 | `		/* Save the raw token */` |
|     13321 | 1262 | `		SyStringInitFromBuf(&sToken.sData,zCur,zCurEnd - zCur);` |
|     13321 | 1263 | `		sToken.nType = PH7_TOKEN_RAW;` |
|     13321 | 1264 | `		rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|     13321 | 1265 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1266 | `			return rc;` |
|         - | 1267 | `		}` |
|     13321 | 1268 | `		if( zIn >= zEnd ){` |
|        13 | 1269 | `			break;` |
|         - | 1270 | `		}` |
|         - | 1271 | `		/* Ignore leading white space */` |
|     28355 | 1272 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|     15049 | 1273 | `			if( zIn[0] == '\n' ){` |
|     14607 | 1274 | `				nLine++;` |
|      7301 | 1275 | `			}` |
|     15049 | 1276 | `			zIn++;` |
|         5 | 1277 | `		}` |
|         - | 1278 | `		/* Delimit the PHP chunk */` |
|     13311 | 1279 | `		sToken.nLine = nLine;` |
|     13311 | 1280 | `		zCur = zIn;` |
|   1817509 | 1281 | `		while( (sxu32)(zEnd - zIn) >= sCtag.nByte ){` |
|         - | 1282 | `			const char *zPtr;` |
|   1811365 | 1283 | `			if( SyMemcmp(zIn,sCtag.zString,sCtag.nByte) == 0 && iNest < 1 ){` |
|      7163 | 1284 | `				break;` |
|         - | 1285 | `			}` |
|    905727 | 1286 | `			for(;;){` |
|   1811459 | 1287 | `				if( zIn[0] != '/' \|\| (zIn[1] != '*' && zIn[1] != '/') /* && sCtag.nByte >= 2 */ ){` |
|    902106 | 1288 | `					break;` |
|         - | 1289 | `				}` |
|      7257 | 1290 | `				zIn += 2;` |
|      7257 | 1291 | `				if( zIn[-1] == '/' ){` |
|         - | 1292 | `					/* Inline comment */` |
|    291199 | 1293 | `					while( zIn < zEnd && zIn[0] != '\n' ){` |
|    284197 | 1294 | `						zIn++;` |
|         5 | 1295 | `					}` |
|      7007 | 1296 | `					if( zIn >= zEnd ){` |
|         3 | 1297 | `						zIn--;` |
|         1 | 1298 | `					}` |
|      3506 | 1299 | `				}else{` |
|         - | 1300 | `					/* Block comment */` |
|     22491 | 1301 | `					while( (sxu32)(zEnd-zIn) >= sizeof("*/") - 1 ){` |
|     22491 | 1302 | `						if( zIn[0] == '*' && zIn[1] == '/' ){` |
|       255 | 1303 | `							zIn += 2;` |
|       255 | 1304 | `							break;` |
|         - | 1305 | `						}` |
|     22241 | 1306 | `						if( zIn[0] == '\n' ){` |
|       165 | 1307 | `							nLine++;` |
|        80 | 1308 | `						}` |
|     22241 | 1309 | `						zIn++;` |
|         5 | 1310 | `					}` |
|         - | 1311 | `				}` |
|         5 | 1312 | `			}` |
|   1804207 | 1313 | `			if( zIn[0] == '\n' ){` |
|     60255 | 1314 | `				nLine++;` |
|     60255 | 1315 | `				if( iNest > 0 ){` |
|       328 | 1316 | `					zIn++;` |
|       718 | 1317 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|       393 | 1318 | `						zIn++;` |
|         3 | 1319 | `					}` |
|       328 | 1320 | `					zPtr = zIn;` |
|      1644 | 1321 | `					while( zIn < zEnd ){` |
|      1644 | 1322 | `						if( (unsigned char)zIn[0] >= 0xc0 ){` |
|         - | 1323 | `							/* UTF-8 stream */` |
|        19 | 1324 | `							zIn++;` |
|        37 | 1325 | `							SX_JMP_UTF8(zIn,zEnd);` |
|      1632 | 1326 | `						}else if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       166 | 1327 | `							break;` |
|       ! 0 | 1328 | `						}else{` |
|      1302 | 1329 | `							zIn++;` |
|         - | 1330 | `						}` |
|         4 | 1331 | `					}` |
|       328 | 1332 | `					if( (sxu32)(zIn - zPtr) == sDoc.nByte && SyMemcmp(sDoc.zString,zPtr,sDoc.nByte) == 0 ){` |
|       120 | 1333 | `						iNest = 0;` |
|        58 | 1334 | `					}` |
|       328 | 1335 | `					continue;` |
|         5 | 1336 | `				}` |
|   1773920 | 1337 | `			}else if ( (sxu32)(zEnd - zIn) >= sizeof("<<<") && zIn[0] == '<' && zIn[1] == '<' && zIn[2] == '<' && iNest < 1){` |
|       122 | 1338 | `				zIn += sizeof("<<<")-1;` |
|       134 | 1339 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|        13 | 1340 | `					zIn++;` |
|         1 | 1341 | `				}` |
|       122 | 1342 | `				if( zIn[0] == '"' \|\| zIn[0] == '\'' ){` |
|        52 | 1343 | `					zIn++;` |
|        24 | 1344 | `				}` |
|       122 | 1345 | `				zPtr = zIn;` |
|       564 | 1346 | `				while( zIn < zEnd ){` |
|       564 | 1347 | `					if( (unsigned char)zIn[0] >= 0xc0 ){` |
|         - | 1348 | `						/* UTF-8 stream */` |
|        19 | 1349 | `						zIn++;` |
|        37 | 1350 | `						SX_JMP_UTF8(zIn,zEnd);` |
|       552 | 1351 | `					}else if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|        63 | 1352 | `						break;` |
|       ! 0 | 1353 | `					}else{` |
|       428 | 1354 | `						zIn++;` |
|         - | 1355 | `					}` |
|         4 | 1356 | `				}` |
|       122 | 1357 | `				SyStringInitFromBuf(&sDoc,zPtr,zIn-zPtr);` |
|       122 | 1358 | `				SyStringFullTrim(&sDoc);` |
|       122 | 1359 | `				if( sDoc.nByte > 0 ){` |
|       122 | 1360 | `					iNest++;` |
|        59 | 1361 | `				}` |
|       122 | 1362 | `				continue;` |
|         - | 1363 | `			}` |
|   1803765 | 1364 | `			zIn++;` |
|         - | 1365 |  |
|   1803765 | 1366 | `			if ( zIn >= zEnd )` |
|         5 | 1367 | `				break;` |
|         5 | 1368 | `		}` |
|     13311 | 1369 | `		if( (sxu32)(zEnd - zIn) < sCtag.nByte ){` |
|      6153 | 1370 | `			zIn = zEnd;` |
|      3074 | 1371 | `		}` |
|     13311 | 1372 | `		if( zCur < zIn ){` |
|         - | 1373 | `			/* Save the PHP chunk for later processing */` |
|     10141 | 1374 | `			sToken.nType = PH7_TOKEN_PHP;` |
|     10141 | 1375 | `			SyStringInitFromBuf(&sToken.sData,zCur,zIn-zCur);` |
|     20131 | 1376 | `			SyStringRightTrim(&sToken.sData); /* Trim trailing white spaces */` |
|     10141 | 1377 | `			rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|     10141 | 1378 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 1379 | `				return rc;` |
|         - | 1380 | `			}` |
|      5068 | 1381 | `		}` |
|     13311 | 1382 | `		if( zIn < zEnd ){` |
|         - | 1383 | `			/* Jump the trailing closing tag */` |
|      7163 | 1384 | `			zIn += sCtag.nByte;` |
|         - | 1385 | `			/* php's lexer swallows exactly ONE newline immediately after the` |
|         - | 1386 | `			 * closing tag ("?>\n" emits nothing) */` |
|      7163 | 1387 | `			if( zIn < zEnd && zIn[0] == '\r' && zIn + 1 < zEnd && zIn[1] == '\n' ){` |
|       ! 0 | 1388 | `				zIn += 2;` |
|       ! 0 | 1389 | `				nLine++;` |
|      7163 | 1390 | `			}else if( zIn < zEnd && zIn[0] == '\n' ){` |
|        63 | 1391 | `				zIn++;` |
|        63 | 1392 | `				nLine++;` |
|        29 | 1393 | `			}` |
|      3579 | 1394 | `		}` |
|         5 | 1395 | `	} /* For(;;) */` |
|         - | 1396 |  |
|     13311 | 1397 | ` 	return SXRET_OK;` |
|      6658 | 1398 | `}` |
|         - | 1399 |  |
