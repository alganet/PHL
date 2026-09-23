# src/ph7/lex.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 795/847 lines (93.86%)

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
|        - |   11 | `/*` |
|        - |   12 | ` * php's LABEL byte class, the one rule behind every identifier the engine reads —` |
|        - |   13 | ` * a variable name, a function/class name, a keyword, a constant, and the name half` |
|        - |   14 | ` * of "$name" inside a double-quoted string (compile_literal.c mirrors it):` |
|        - |   15 | ` *` |
|        - |   16 | ` *     label       [a-zA-Z_\x80-\xff][a-zA-Z0-9_\x80-\xff]*` |
|        - |   17 | ` *` |
|        - |   18 | ` * Every byte >= 0x80 is a label byte, which is what makes a UTF-8 name work without` |
|        - |   19 | ` * decoding it. PH7's scanners required a UTF-8 LEAD byte (>= 0xc0) and then walked` |
|        - |   20 | ``  * the continuation bytes, so the 0x80-0xbf half of php's class was missing: `$\x80ab` `` |
|        - |   21 | `` * was a PHL parse error where php accepts it, and `"$\x80ab"` printed as literal text`` |
|        - |   22 | ` * where php read the variable. Malformed-source-only (0x80-0xbf never LEADS a valid` |
|        - |   23 | ` * UTF-8 sequence), but it is the same class php applies everywhere.` |
|        - |   24 | ` */` |
|        - |   25 | `#define LEX_LABEL_START(c) \` |
|        - |   26 | `	( (unsigned char)(c) >= 0x80 \|\| SyisAlpha(c) \|\| (c) == '_' )` |
|        - |   27 | `#define LEX_LABEL_BYTE(c) \` |
|        - |   28 | `	( (unsigned char)(c) >= 0x80 \|\| SyisAlphaNum(c) \|\| (c) == '_' )` |
|        - |   29 | `/* Forward declaration */` |
|        - |   30 | `static sxu32 KeywordCode(const char *z, int n);` |
|        - |   31 | `static sxu32 KeywordCodeCI(const char *zRaw, int n);` |
|        - |   32 | `static sxi32 LexExtractHeredoc(SyStream *pStream,SyToken *pToken);` |
|        - |   33 | `/*` |
|        - |   34 | ` * Tokenize a raw PHP input.` |
|        - |   35 | ` * Get a single low-level token from the input file. Update the stream pointer so that` |
|        - |   36 | ` * it points to the first character beyond the extracted token.` |
|        - |   37 | ` */` |
| 17089858 |   38 | `static sxi32 TokenizePHP(SyStream *pStream,SyToken *pToken,void *pUserData,void *pCtxData)` |
|        5 |   39 | `{` |
|        - |   40 | `	SyString *pStr;` |
|        - |   41 | `	sxi32 rc;` |
|        - |   42 | `	/* Ignore leading white spaces */` |
| 27258521 |   43 | `	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){` |
|        - |   44 | `		/* Advance the stream cursor */` |
| 10168663 |   45 | `		if( pStream->zText[0] == '\n' ){` |
|        - |   46 | `			/* Update line counter */` |
|    90939 |   47 | `			pStream->nLine++;` |
|    45467 |   48 | `		}` |
| 10168663 |   49 | `		pStream->zText++;` |
|        5 |   50 | `	}` |
| 17089863 |   51 | `	if( pStream->zText >= pStream->zEnd ){` |
|        - |   52 | `		/* End of input reached */` |
|        3 |   53 | `		return SXERR_EOF;` |
|        - |   54 | `	}` |
|        - |   55 | `	/* Record token starting position and line */` |
| 17089861 |   56 | `	pToken->nLine = pStream->nLine;` |
| 17089861 |   57 | `	pToken->pUserData = 0;` |
| 17089861 |   58 | `	pStr = &pToken->sData;` |
| 17089861 |   59 | `	SyStringInitFromBuf(pStr,pStream->zText,0);` |
| 19630663 |   60 | `	if( LEX_LABEL_START(pStream->zText[0]) ){` |
|        - |   61 | `		const unsigned char *zIn;` |
|        - |   62 | `		sxu32 nKeyword;` |
|        - |   63 | `		/* Isolate the LABEL. php's class is a flat byte set — [a-zA-Z_\x80-\xff] then` |
|        - |   64 | `		 * [a-zA-Z0-9_\x80-\xff]* — so no UTF-8 decoding is involved: a multibyte name` |
|        - |   65 | `		 * is consumed because every one of its bytes is >= 0x80. (This replaces the` |
|        - |   66 | `		 * xPP lead-byte-plus-continuations dance, which required a lead >= 0xc0 and so` |
|        - |   67 | `		 * stopped one byte class short of php's own rule; see LEX_LABEL_START.) */` |
|  5081609 |   68 | `		zIn = &pStream->zText[1];` |
| 29764523 |   69 | `		while( zIn < pStream->zEnd && LEX_LABEL_BYTE(zIn[0]) ){` |
| 22142117 |   70 | `			zIn++;` |
|        5 |   71 | `		}` |
|  5081609 |   72 | `		pStream->zText = zIn;` |
|        - |   73 | `		/* Record token length */` |
|  5081609 |   74 | `		pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  5081609 |   75 | `		nKeyword = KeywordCodeCI(pStr->zString,(int)pStr->nByte);` |
|  5081609 |   76 | `		if( nKeyword != PH7_TK_ID ){` |
|  1707127 |   77 | `			if( nKeyword &` |
|        - |   78 | `				(PH7_TKWRD_NEW\|PH7_TKWRD_CLONE\|PH7_TKWRD_AND\|PH7_TKWRD_XOR\|PH7_TKWRD_OR\|PH7_TKWRD_INSTANCEOF) ){` |
|        - |   79 | `					/* Alpha stream operators [i.e: new,clone,and,instanceof,or,xor],save the operator instance for later processing */` |
|    77767 |   80 | `					pToken->pUserData = (void *)PH7_ExprExtractOperator(pStr,0);` |
|        - |   81 | `					/* Mark as an operator */` |
|    77767 |   82 | `					pToken->nType = PH7_TK_ID\|PH7_TK_OP;` |
|    38886 |   83 | `			}else{` |
|        - |   84 | `				/* We are dealing with a keyword [i.e: while,foreach,class...],save the keyword ID */` |
|  1629365 |   85 | `				pToken->nType = PH7_TK_KEYWORD;` |
|  1629365 |   86 | `				pToken->pUserData = SX_INT_TO_PTR(nKeyword);` |
|        - |   87 | `			}` |
|   853566 |   88 | `		}else{` |
|        - |   89 | `			/* A simple identifier */` |
|  3374487 |   90 | `			pToken->nType = PH7_TK_ID;` |
|        - |   91 | `		}` |
|  2540807 |   92 | `	}else{` |
|        - |   93 | `		sxi32 c;` |
|        - |   94 | `		/* Non-alpha stream */` |
| 12008257 |   95 | `		if( pStream->zText[0] == '#' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '[' ){` |
|      237 |   96 | `			sxu32 nDepth = 1;` |
|        - |   97 | `			/* PHP 8 attribute group '#[ ... ]': skip the whole balanced group as` |
|        - |   98 | `			 * trivia (attributes are not stored yet). Brackets inside string` |
|        - |   99 | `			 * literals and comments must not affect the depth count. An` |
|        - |  100 | `			 * unterminated group is silently consumed up to EOF, consistent` |
|        - |  101 | `			 * with unterminated block comments below.` |
|        - |  102 | `			 */` |
|        - |  103 | `			const unsigned char *zGroupStart;` |
|      237 |  104 | `			pStream->zText += 2;` |
|      237 |  105 | `			zGroupStart = pStream->zText;` |
|     4721 |  106 | `			while( pStream->zText < pStream->zEnd && nDepth > 0 ){` |
|     4489 |  107 | `				sxi32 d = pStream->zText[0];` |
|     4489 |  108 | `				if( d == '[' ){` |
|       29 |  109 | `					nDepth++;` |
|     4475 |  110 | `				}else if( d == ']' ){` |
|      265 |  111 | `					nDepth--;` |
|     4331 |  112 | `				}else if( d == '\'' \|\| d == '"' ){` |
|        - |  113 | `					/* String literal: scan for the matching unescaped quote */` |
|       91 |  114 | `					pStream->zText++;` |
|      457 |  115 | `					while( pStream->zText < pStream->zEnd ){` |
|      457 |  116 | `						if( pStream->zText[0] == '\\' && &pStream->zText[1] < pStream->zEnd ){` |
|       23 |  117 | `							if( pStream->zText[1] == '\n' ){` |
|      ! 0 |  118 | `								pStream->nLine++;` |
|      ! 0 |  119 | `							}` |
|       23 |  120 | `							pStream->zText += 2;` |
|       23 |  121 | `							continue;` |
|        - |  122 | `						}` |
|      435 |  123 | `						if( pStream->zText[0] == d ){` |
|       91 |  124 | `							break;` |
|        - |  125 | `						}` |
|      347 |  126 | `						if( pStream->zText[0] == '\n' ){` |
|      ! 0 |  127 | `							pStream->nLine++;` |
|      ! 0 |  128 | `						}` |
|      347 |  129 | `						pStream->zText++;` |
|        3 |  130 | `					}` |
|       91 |  131 | `					if( pStream->zText >= pStream->zEnd ){` |
|      ! 0 |  132 | `						break; /* Unterminated string literal */` |
|        3 |  133 | `					}` |
|        - |  134 | `					/* Fall through: consume the closing quote below */` |
|     4157 |  135 | `				}else if( d == '#' \|\| (d == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '/') ){` |
|        - |  136 | `					/* Inline comment inside the group */` |
|      ! 0 |  137 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] != '\n' ){` |
|      ! 0 |  138 | `						pStream->zText++;` |
|      ! 0 |  139 | `					}` |
|      ! 0 |  140 | `					continue; /* Let the outer loop count the newline */` |
|     4113 |  141 | `				}else if( d == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '*' ){` |
|        - |  142 | `					/* Block comment inside the group */` |
|      ! 0 |  143 | `					pStream->zText += 2;` |
|      ! 0 |  144 | `					while( pStream->zText < pStream->zEnd ){` |
|      ! 0 |  145 | `						if( pStream->zText[0] == '*' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '/' ){` |
|      ! 0 |  146 | `							pStream->zText += 2;` |
|      ! 0 |  147 | `							break;` |
|        - |  148 | `						}` |
|      ! 0 |  149 | `						if( pStream->zText[0] == '\n' ){` |
|      ! 0 |  150 | `							pStream->nLine++;` |
|      ! 0 |  151 | `						}` |
|      ! 0 |  152 | `						pStream->zText++;` |
|      ! 0 |  153 | `					}` |
|      ! 0 |  154 | `					continue;` |
|     4113 |  155 | `				}else if( d == '\n' ){` |
|        7 |  156 | `					pStream->nLine++;` |
|        3 |  157 | `				}` |
|     4489 |  158 | `				pStream->zText++;` |
|        5 |  159 | `			}` |
|      237 |  160 | `			if( pUserData && pStream->pSet ){` |
|        - |  161 | `				/* Record the group's inner span (between #[ and its balanced ])` |
|        - |  162 | `				 * in the trivia sidecar, keyed like doc-comments. */` |
|        - |  163 | `				ph7_trivia sTrivia;` |
|      237 |  164 | `				const unsigned char *zGroupEnd = pStream->zText;` |
|      237 |  165 | `				if( nDepth == 0 && zGroupEnd > zGroupStart ){` |
|      237 |  166 | `					zGroupEnd--; /* Exclude the closing ']' */` |
|      116 |  167 | `				}` |
|      237 |  168 | `				sTrivia.nTokIdx = SySetUsed(pStream->pSet);` |
|      237 |  169 | `				sTrivia.iKind = PH7_TRIVIA_ATTR;` |
|      237 |  170 | `				SyStringInitFromBuf(&sTrivia.sText,(const char *)zGroupStart,(sxu32)(zGroupEnd - zGroupStart));` |
|      237 |  171 | `				sTrivia.nLine = pToken->nLine;` |
|      237 |  172 | `				SySetPut((SySet *)pUserData,(const void *)&sTrivia);` |
|      116 |  173 | `			}` |
|        - |  174 | `			/* Tell the upper-layer to ignore this token */` |
|      237 |  175 | `			return SXERR_CONTINUE;` |
| 12096028 |  176 | `		}else if( pStream->zText[0] == '#' \|\|` |
| 12008014 |  177 | `			( pStream->zText[0] == '/' &&  &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '/') ){` |
|    11277 |  178 | `				pStream->zText++;` |
|        - |  179 | `				/* Inline comments */` |
|   563627 |  180 | `				while( pStream->zText < pStream->zEnd && pStream->zText[0] != '\n' ){` |
|   552355 |  181 | `					pStream->zText++;` |
|        5 |  182 | `				}` |
|        - |  183 | `				/* Tell the upper-layer to ignore this token */` |
|    11277 |  184 | `				return SXERR_CONTINUE;` |
| 11996753 |  185 | `		}else if( pStream->zText[0] == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '*' ){` |
|        - |  186 | `			/* A doc-comment starts with slash-star-star followed by more` |
|        - |  187 | `			 * content (slash-star-star-slash is the empty comment, not a` |
|        - |  188 | `			 * docblock). Its full span, delimiters included, goes to the` |
|        - |  189 | `			 * trivia sidecar when the caller supplied one — keyed by the` |
|        - |  190 | `			 * index the NEXT real token receives — and never enters the` |
|        - |  191 | `			 * token stream. */` |
|   159937 |  192 | `			const unsigned char *zDocStart = pStream->zText;` |
|   159964 |  193 | `			int bDoc = ( &pStream->zText[2] < pStream->zEnd && pStream->zText[2] == '*'` |
|   239925 |  194 | `			 && ( &pStream->zText[3] >= pStream->zEnd \|\| pStream->zText[3] != '/' ) );` |
|   159937 |  195 | `			pStream->zText += 2;` |
|        - |  196 | `			/* Block comment */` |
| 18779579 |  197 | `			while( pStream->zText < pStream->zEnd ){` |
| 18779579 |  198 | `				if( pStream->zText[0] == '*' ){` |
|   221795 |  199 | `					if( &pStream->zText[1] >= pStream->zEnd \|\| pStream->zText[1] == '/'  ){` |
|    79971 |  200 | `						break;` |
|        - |  201 | `					}` |
|    30929 |  202 | `				}` |
| 18619647 |  203 | `				if( pStream->zText[0] == '\n' ){` |
|     1137 |  204 | `					pStream->nLine++;` |
|      566 |  205 | `				}` |
| 18619647 |  206 | `				pStream->zText++;` |
|        5 |  207 | `			}` |
|   159937 |  208 | `			pStream->zText += 2;` |
|   159937 |  209 | `			if( bDoc && pUserData && pStream->pSet ){` |
|        - |  210 | `				ph7_trivia sTrivia;` |
|       59 |  211 | `				const unsigned char *zDocEnd = pStream->zText;` |
|       59 |  212 | `				if( zDocEnd > pStream->zEnd ){` |
|      ! 0 |  213 | `					zDocEnd = pStream->zEnd; /* Unterminated comment at EOF */` |
|      ! 0 |  214 | `				}` |
|       59 |  215 | `				sTrivia.nTokIdx = SySetUsed(pStream->pSet);` |
|       59 |  216 | `				sTrivia.iKind = PH7_TRIVIA_DOC;` |
|       59 |  217 | `				SyStringInitFromBuf(&sTrivia.sText,(const char *)zDocStart,(sxu32)(zDocEnd - zDocStart));` |
|       59 |  218 | `				sTrivia.nLine = pToken->nLine;` |
|       59 |  219 | `				SySetPut((SySet *)pUserData,(const void *)&sTrivia);` |
|       27 |  220 | `			}` |
|        - |  221 | `			/* Tell the upper-layer to ignore this token */` |
|   159937 |  222 | `			return SXERR_CONTINUE;` |
| 11836821 |  223 | `		}else if( SyisDigit(pStream->zText[0]) ){` |
|   657693 |  224 | `			pStream->zText++;` |
|        - |  225 | `			/* PHP 7.4: handle underscore separator immediately following the first digit.` |
|        - |  226 | `			 * Check pStream->zText < pStream->zEnd BEFORE forming pStream->zText + 1 so` |
|        - |  227 | `			 * we never compute a pointer past one-past-end. */` |
|   657688 |  228 | `			if( pStream->zText < pStream->zEnd` |
|   657688 |  229 | `				&& pStream->zText[0] == '_'` |
|   328926 |  230 | `				&& pStream->zText + 1 < pStream->zEnd` |
|      164 |  231 | `				&& pStream->zText[1] < 0xc0` |
|      169 |  232 | `				&& SyisDigit(pStream->zText[1]) ){` |
|      156 |  233 | `				pStream->zText++; /* swallow underscore between two digits */` |
|       77 |  234 | `			}` |
|        - |  235 | `			/* Decimal digit stream (PHP 7.4: underscore separator allowed between two digits) */` |
|   958325 |  236 | `			while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|   300637 |  237 | `				pStream->zText++;` |
|   300632 |  238 | `				if( pStream->zText < pStream->zEnd` |
|   300632 |  239 | `					&& pStream->zText[0] == '_'` |
|   150402 |  240 | `					&& pStream->zText + 1 < pStream->zEnd` |
|      172 |  241 | `					&& pStream->zText[1] < 0xc0` |
|      177 |  242 | `					&& SyisDigit(pStream->zText[1]) ){` |
|      173 |  243 | `					pStream->zText++; /* swallow underscore between two digits */` |
|       86 |  244 | `				}` |
|        5 |  245 | `			}` |
|        - |  246 | `			/* Mark the token as integer until we encounter a real number */` |
|   657693 |  247 | `			pToken->nType = PH7_TK_INTEGER;` |
|   657693 |  248 | `			if( pStream->zText < pStream->zEnd ){` |
|   657693 |  249 | `				c = pStream->zText[0];` |
|   657693 |  250 | `				if( c == '.' ){` |
|        - |  251 | `					/* Real number (PHP 7.4: underscore separator allowed between two digits) */` |
|    10655 |  252 | `					pStream->zText++;` |
|    22949 |  253 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|    12299 |  254 | `						pStream->zText++;` |
|    12294 |  255 | `						if( pStream->zText < pStream->zEnd` |
|    12294 |  256 | `							&& pStream->zText[0] == '_'` |
|     6153 |  257 | `							&& pStream->zText + 1 < pStream->zEnd` |
|       12 |  258 | `							&& pStream->zText[1] < 0xc0` |
|       17 |  259 | `							&& SyisDigit(pStream->zText[1]) ){` |
|       13 |  260 | `							pStream->zText++;` |
|        6 |  261 | `						}` |
|        5 |  262 | `					}` |
|    10655 |  263 | `					if( pStream->zText < pStream->zEnd ){` |
|    10655 |  264 | `						c = pStream->zText[0];` |
|    10655 |  265 | `						if( c=='e' \|\| c=='E' ){` |
|       78 |  266 | `							pStream->zText++;` |
|       78 |  267 | `							if( pStream->zText < pStream->zEnd ){` |
|       78 |  268 | `								c = pStream->zText[0];` |
|       76 |  269 | `								if( (c =='+' \|\| c=='-') && &pStream->zText[1] < pStream->zEnd  &&` |
|       48 |  270 | `									pStream->zText[1] < 0xc0 && SyisDigit(pStream->zText[1]) ){` |
|       48 |  271 | `										pStream->zText++;` |
|       23 |  272 | `								}` |
|      220 |  273 | `								while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|      144 |  274 | `									pStream->zText++;` |
|      142 |  275 | `									if( pStream->zText < pStream->zEnd` |
|      142 |  276 | `										&& pStream->zText[0] == '_'` |
|       75 |  277 | `										&& pStream->zText + 1 < pStream->zEnd` |
|        8 |  278 | `										&& pStream->zText[1] < 0xc0` |
|       10 |  279 | `										&& SyisDigit(pStream->zText[1]) ){` |
|        9 |  280 | `										pStream->zText++;` |
|        4 |  281 | `									}` |
|        2 |  282 | `								}` |
|       38 |  283 | `							}` |
|       38 |  284 | `						}` |
|     5325 |  285 | `					}` |
|    10655 |  286 | `					pToken->nType = PH7_TK_REAL;` |
|   652368 |  287 | `				}else if( c=='e' \|\| c=='E' ){` |
|       80 |  288 | `					SXUNUSED(pUserData); /* Prevent compiler warning */` |
|       80 |  289 | `					SXUNUSED(pCtxData);` |
|      163 |  290 | `					pStream->zText++;` |
|      163 |  291 | `					if( pStream->zText < pStream->zEnd ){` |
|      163 |  292 | `						c = pStream->zText[0];` |
|      160 |  293 | `						if( (c =='+' \|\| c=='-') && &pStream->zText[1] < pStream->zEnd  &&` |
|       41 |  294 | `							pStream->zText[1] < 0xc0 && SyisDigit(pStream->zText[1]) ){` |
|       40 |  295 | `								pStream->zText++;` |
|       19 |  296 | `						}` |
|      487 |  297 | `						while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|      327 |  298 | `							pStream->zText++;` |
|      324 |  299 | `							if( pStream->zText < pStream->zEnd` |
|      324 |  300 | `								&& pStream->zText[0] == '_'` |
|      164 |  301 | `								&& pStream->zText + 1 < pStream->zEnd` |
|        4 |  302 | `								&& pStream->zText[1] < 0xc0` |
|        7 |  303 | `								&& SyisDigit(pStream->zText[1]) ){` |
|        5 |  304 | `								pStream->zText++;` |
|        2 |  305 | `							}` |
|        3 |  306 | `						}` |
|       80 |  307 | `					}` |
|      163 |  308 | `					pToken->nType = PH7_TK_REAL;` |
|        - |  309 | `				/* php only reads a base prefix when the literal so far is exactly "0"` |
|        - |  310 | `				 * AND at least one valid digit follows it. Otherwise the '0' stands` |
|        - |  311 | `				 * alone as an integer and the letter begins an IDENTIFIER, which is` |
|        - |  312 | ``				 * why php reports `0xG` as `unexpected identifier "xG"` while PHL,`` |
|        - |  313 | `				 * consuming the prefix unconditionally, reported just "G". The same` |
|        - |  314 | ``				 * gap silently ACCEPTED `0x`/`0b`/`0o` as int(0), and read `1x5` as`` |
|        - |  315 | `				 * a hex literal, both of which php rejects outright. */` |
|   646961 |  316 | `				}else if( (c == 'x' \|\| c == 'X')` |
|   323490 |  317 | `					&& pStream->zText == &((const unsigned char *)pStr->zString)[1] && pStr->zString[0] == 0x30` |
|      102 |  318 | `					&& &pStream->zText[1] < pStream->zEnd` |
|      107 |  319 | `					&& pStream->zText[1] < 0xc0 && SyisHex(pStream->zText[1]) ){` |
|        - |  320 | `					/* Hex digit stream (PHP 7.4: underscore separator allowed between two digits) */` |
|      100 |  321 | `					pStream->zText++;` |
|      596 |  322 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisHex(pStream->zText[0]) ){` |
|      498 |  323 | `						pStream->zText++;` |
|      496 |  324 | `						if( pStream->zText < pStream->zEnd` |
|      496 |  325 | `							&& pStream->zText[0] == '_'` |
|      273 |  326 | `							&& pStream->zText + 1 < pStream->zEnd` |
|       50 |  327 | `							&& pStream->zText[1] < 0xc0` |
|       52 |  328 | `							&& SyisHex(pStream->zText[1]) ){` |
|       51 |  329 | `							pStream->zText++;` |
|       25 |  330 | `						}` |
|        2 |  331 | `					}` |
|   646831 |  332 | `				}else if( (c == 'b' \|\| c == 'B')` |
|   323534 |  333 | `					&& pStream->zText == &((const unsigned char *)pStr->zString)[1] && pStr->zString[0] == 0x30` |
|      288 |  334 | `					&& &pStream->zText[1] < pStream->zEnd` |
|      293 |  335 | `					&& (pStream->zText[1] == '0' \|\| pStream->zText[1] == '1') ){` |
|        - |  336 | `					/* Binary digit stream (PHP 7.4: underscore separator allowed between two digits) */` |
|      287 |  337 | `					pStream->zText++;` |
|     3115 |  338 | `					while( pStream->zText < pStream->zEnd && (pStream->zText[0] == '0' \|\| pStream->zText[0] == '1') ){` |
|     1791 |  339 | `						pStream->zText++;` |
|     1790 |  340 | `						if( pStream->zText < pStream->zEnd` |
|     1790 |  341 | `							&& pStream->zText[0] == '_'` |
|      965 |  342 | `							&& pStream->zText + 1 < pStream->zEnd` |
|      141 |  343 | `							&& (pStream->zText[1] == '0' \|\| pStream->zText[1] == '1') ){` |
|      141 |  344 | `							pStream->zText++;` |
|       70 |  345 | `						}` |
|        1 |  346 | `					}` |
|   646638 |  347 | `				}else if( (c == 'o' \|\| c == 'O')` |
|   323257 |  348 | `					&& pStream->zText == &((const unsigned char *)pStr->zString)[1] && pStr->zString[0] == 0x30` |
|       20 |  349 | `					&& &pStream->zText[1] < pStream->zEnd` |
|       25 |  350 | `					&& pStream->zText[1] >= '0' && pStream->zText[1] <= '7' ){` |
|        - |  351 | `					/* PHP 8.1 explicit octal 0o/0O (underscore separator allowed between two digits) */` |
|       21 |  352 | `					pStream->zText++;` |
|      101 |  353 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] >= '0' && pStream->zText[0] <= '7' ){` |
|       81 |  354 | `						pStream->zText++;` |
|       80 |  355 | `						if( pStream->zText < pStream->zEnd` |
|       80 |  356 | `							&& pStream->zText[0] == '_'` |
|       41 |  357 | `							&& pStream->zText + 1 < pStream->zEnd` |
|        3 |  358 | `							&& pStream->zText[1] >= '0' && pStream->zText[1] <= '7' ){` |
|        3 |  359 | `							pStream->zText++;` |
|        1 |  360 | `						}` |
|        1 |  361 | `					}` |
|       10 |  362 | `				}` |
|   328844 |  363 | `			}` |
|        - |  364 | `			/* PHP 7.4: absorb a trailing malformed underscore run into the` |
|        - |  365 | `			 * numeric token so the compile phase can emit a PHP-compatible` |
|        - |  366 | `			 * "syntax error, unexpected identifier" parse error. Valid` |
|        - |  367 | `			 * separators were already consumed by the per-loop peek logic` |
|        - |  368 | `			 * above, so an underscore here is always misplaced. */` |
|   657693 |  369 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '_' ){` |
|       14 |  370 | `				pStream->zText++;` |
|       28 |  371 | `				while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0` |
|       31 |  372 | `					&& (SyisAlphaNum(pStream->zText[0]) \|\| pStream->zText[0] == '_') ){` |
|       10 |  373 | `					pStream->zText++;` |
|        2 |  374 | `				}` |
|        5 |  375 | `			}` |
|        - |  376 | `			/* Record token length */` |
|   657693 |  377 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|   657693 |  378 | `			return SXRET_OK;` |
|        - |  379 | `		}` |
| 11179133 |  380 | `		c = pStream->zText[0];` |
| 11179133 |  381 | `		pStream->zText++; /* Advance the stream cursor */` |
|        - |  382 | `		/* Assume we are dealing with an operator*/` |
| 11179133 |  383 | `		pToken->nType = PH7_TK_OP;` |
| 11179133 |  384 | `		switch(c){` |
|  2635579 |  385 | `		case '$': pToken->nType = PH7_TK_DOLLAR; break;` |
|   620689 |  386 | `		case '{': pToken->nType = PH7_TK_OCB;    break;` |
|   620675 |  387 | `		case '}': pToken->nType = PH7_TK_CCB;    break;` |
|  1507461 |  388 | `		case '(': pToken->nType = PH7_TK_LPAREN; break;` |
|   195369 |  389 | `		case '[': pToken->nType \|= PH7_TK_OSB;   break; /* Bitwise operation here,since the square bracket token '['` |
|        - |  390 | `														 * is a potential operator [i.e: subscripting] */` |
|   195375 |  391 | `		case ']': pToken->nType = PH7_TK_CSB;    break;` |
|   753717 |  392 | `		case ')': {` |
|  1507439 |  393 | `			SySet *pTokSet = pStream->pSet;` |
|        - |  394 | `			/* Assemble type cast operators [i.e: (int),(float),(bool)...] */` |
|  1507439 |  395 | `			if( pTokSet->nUsed >= 2 ){` |
|        - |  396 | `				SyToken *pTmp;` |
|        - |  397 | `				/* Peek the last recongnized token */` |
|  1507437 |  398 | `				pTmp = (SyToken *)SySetPeek(pTokSet);` |
|  1507437 |  399 | `				if( pTmp->nType & PH7_TK_KEYWORD ){` |
|   174103 |  400 | `					sxi32 nID = SX_PTR_TO_INT(pTmp->pUserData);` |
|   174103 |  401 | `					if( (sxu32)nID & (PH7_TKWRD_ARRAY\|PH7_TKWRD_INT\|PH7_TKWRD_FLOAT\|PH7_TKWRD_STRING\|PH7_TKWRD_OBJECT\|PH7_TKWRD_BOOL\|PH7_TKWRD_UNSET) ){` |
|   173633 |  402 | `						pTmp = (SyToken *)SySetAt(pTokSet,pTokSet->nUsed - 2);` |
|   173633 |  403 | `						if( pTmp->nType & PH7_TK_LPAREN ){` |
|        - |  404 | `							/* Merge the three tokens '(' 'TYPE' ')' into a single one */` |
|   126845 |  405 | `							const char * zTypeCast = "(int)";` |
|   126845 |  406 | `							if( nID & PH7_TKWRD_FLOAT ){` |
|    18719 |  407 | `								zTypeCast = "(float)";` |
|   117488 |  408 | `							}else if( nID & PH7_TKWRD_BOOL ){` |
|       41 |  409 | `								zTypeCast = "(bool)";` |
|   108111 |  410 | `							}else if( nID & PH7_TKWRD_STRING ){` |
|    65651 |  411 | `								zTypeCast = "(string)";` |
|    75268 |  412 | `							}else if( nID & PH7_TKWRD_ARRAY ){` |
|      101 |  413 | `								zTypeCast = "(array)";` |
|    42397 |  414 | `							}else if( nID & PH7_TKWRD_OBJECT ){` |
|       52 |  415 | `								zTypeCast = "(object)";` |
|    42324 |  416 | `							}else if( nID & PH7_TKWRD_UNSET ){` |
|        3 |  417 | `								zTypeCast = "(unset)";` |
|        1 |  418 | `							}` |
|        - |  419 | `							/* Reflect the change */` |
|   126845 |  420 | `							pToken->nType = PH7_TK_OP;` |
|   126845 |  421 | `							SyStringInitFromBuf(&pToken->sData,zTypeCast,SyStrlen(zTypeCast));` |
|        - |  422 | `							/* Save the instance associated with the type cast operator */` |
|   126845 |  423 | `							pToken->pUserData = (void *)PH7_ExprExtractOperator(&pToken->sData,0);` |
|        - |  424 | `							/* Remove the two previous tokens */` |
|   126845 |  425 | `							pTokSet->nUsed -= 2;` |
|   126845 |  426 | `							return SXRET_OK;` |
|        - |  427 | `						}` |
|    23394 |  428 | `					}` |
|    23629 |  429 | `				}` |
|   690296 |  430 | `			}` |
|  1380599 |  431 | `			pToken->nType = PH7_TK_RPAREN;` |
|  1380599 |  432 | `			break;` |
|        - |  433 | `				  }` |
|   172898 |  434 | `		case '\'':{` |
|        - |  435 | `			/* Single quoted string */` |
|   345801 |  436 | `			pStr->zString++;` |
|  5536693 |  437 | `			while( pStream->zText < pStream->zEnd ){` |
|  5536693 |  438 | `				if( pStream->zText[0] == '\''  ){` |
|   345821 |  439 | `					if( pStream->zText[-1] != '\\' ){` |
|   345739 |  440 | `						break;` |
|      ! 0 |  441 | `					}else{` |
|       85 |  442 | `						const unsigned char *zPtr = &pStream->zText[-2];` |
|       85 |  443 | `						sxi32 i = 1;` |
|      167 |  444 | `						while( zPtr > pStream->zInput && zPtr[0] == '\\' ){` |
|       85 |  445 | `							zPtr--;` |
|       85 |  446 | `							i++;` |
|        3 |  447 | `						}` |
|       85 |  448 | `						if((i&1)==0){` |
|       65 |  449 | `							break;` |
|        - |  450 | `						}` |
|        - |  451 | `					}` |
|       10 |  452 | `				}` |
|  5190897 |  453 | `				if( pStream->zText[0] == '\n' ){` |
|       65 |  454 | `					pStream->nLine++;` |
|       32 |  455 | `				}` |
|  5190897 |  456 | `				pStream->zText++;` |
|        5 |  457 | `			}` |
|        - |  458 | `			/* Record token length and type */` |
|   345801 |  459 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|   345801 |  460 | `			pToken->nType = PH7_TK_SSTR;` |
|        - |  461 | `			/* Jump the trailing single quote */` |
|   345801 |  462 | `			pStream->zText++;` |
|   345801 |  463 | `			return SXRET_OK;` |
|        - |  464 | `				  }` |
|    22580 |  465 | `		case '"':{` |
|        - |  466 | `			sxi32 iNest;` |
|        - |  467 | `			/* Double quoted string */` |
|    45165 |  468 | `			pStr->zString++;` |
|   344897 |  469 | `			while( pStream->zText < pStream->zEnd ){` |
|   344897 |  470 | `				if( pStream->zText[0] == '{' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '$'){` |
|      189 |  471 | `					iNest = 1;` |
|      189 |  472 | `					pStream->zText++;` |
|        - |  473 | `					/* TICKET 1433-40: Hnadle braces'{}' in double quoted string where everything is allowed */` |
|     1613 |  474 | `					while(pStream->zText < pStream->zEnd ){` |
|     1613 |  475 | `						if( pStream->zText[0] == '{' ){` |
|        3 |  476 | `							iNest++;` |
|     1612 |  477 | `						}else if (pStream->zText[0] == '}' ){` |
|      191 |  478 | `							iNest--;` |
|      191 |  479 | `							if( iNest <= 0 ){` |
|      189 |  480 | `								pStream->zText++;` |
|      189 |  481 | `								break;` |
|        1 |  482 | `							}` |
|     1424 |  483 | `						}else if( pStream->zText[0] == '\n' ){` |
|      ! 0 |  484 | `							pStream->nLine++;` |
|      ! 0 |  485 | `						}` |
|     1427 |  486 | `						pStream->zText++;` |
|        3 |  487 | `					}` |
|      189 |  488 | `					if( pStream->zText >= pStream->zEnd ){` |
|      ! 0 |  489 | `						break;` |
|        - |  490 | `					}` |
|       93 |  491 | `				}` |
|   344897 |  492 | `				if( pStream->zText[0] == '"' ){` |
|    45577 |  493 | `					if( pStream->zText[-1] != '\\' ){` |
|    45131 |  494 | `						break;` |
|      ! 0 |  495 | `					}else{` |
|      451 |  496 | `						const unsigned char *zPtr = &pStream->zText[-2];` |
|      451 |  497 | `						sxi32 i = 1;` |
|      533 |  498 | `						while( zPtr > pStream->zInput && zPtr[0] == '\\' ){` |
|       85 |  499 | `							zPtr--;` |
|       85 |  500 | `							i++;` |
|        3 |  501 | `						}` |
|      451 |  502 | `						if((i&1)==0){` |
|       35 |  503 | `							break;` |
|        - |  504 | `						}` |
|        - |  505 | `					}` |
|      206 |  506 | `				}` |
|   299737 |  507 | `				if( pStream->zText[0] == '\n' ){` |
|       47 |  508 | `					pStream->nLine++;` |
|       23 |  509 | `				}` |
|   299737 |  510 | `				pStream->zText++;` |
|        5 |  511 | `			}` |
|        - |  512 | `			/* Record token length and type */` |
|    45165 |  513 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|    45165 |  514 | `			pToken->nType = PH7_TK_DSTR;` |
|        - |  515 | `			/* Jump the trailing quote */` |
|    45165 |  516 | `			pStream->zText++;` |
|    45165 |  517 | `			return SXRET_OK;` |
|        - |  518 | `				  }` |
|        1 |  519 | ``		case '`':{`` |
|        - |  520 | `			/* Backtick quoted string */` |
|        3 |  521 | `			pStr->zString++;` |
|       21 |  522 | `			while( pStream->zText < pStream->zEnd ){` |
|       21 |  523 | ``				if( pStream->zText[0] == '`' && pStream->zText[-1] != '\\' ){`` |
|        3 |  524 | `					break;` |
|        - |  525 | `				}` |
|       19 |  526 | `				if( pStream->zText[0] == '\n' ){` |
|      ! 0 |  527 | `					pStream->nLine++;` |
|      ! 0 |  528 | `				}` |
|       19 |  529 | `				pStream->zText++;` |
|        1 |  530 | `			}` |
|        - |  531 | `			/* Record token length and type */` |
|        3 |  532 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|        3 |  533 | `			pToken->nType = PH7_TK_BSTR;` |
|        - |  534 | `			/* Jump the trailing backtick */` |
|        3 |  535 | `			pStream->zText++;` |
|        3 |  536 | `			return SXRET_OK;` |
|        - |  537 | `				  }` |
|     1539 |  538 | `		case '\\': pToken->nType = PH7_TK_NSSEP;  break;` |
|    20405 |  539 | `		case ':':` |
|    40815 |  540 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == ':' ){` |
|        - |  541 | `				/* Current operator: '::' */` |
|     2317 |  542 | `				pStream->zText++;` |
|     1161 |  543 | `			}else{` |
|    38503 |  544 | `				pToken->nType = PH7_TK_COLON; /* Single colon */` |
|        - |  545 | `			}` |
|    40815 |  546 | `			break;` |
|   530207 |  547 | `		case ',': pToken->nType \|= PH7_TK_COMMA;  break; /* Comma is also an operator */` |
|  1095459 |  548 | `		case ';': pToken->nType = PH7_TK_SEMI;    break;` |
|        - |  549 | `			/* Handle combined operators [i.e: +=,===,!=== ...] */` |
|   468874 |  550 | `		case '=':` |
|   937753 |  551 | `			pToken->nType \|= PH7_TK_EQUAL;` |
|   937753 |  552 | `			if( pStream->zText < pStream->zEnd ){` |
|   937753 |  553 | `				if( pStream->zText[0] == '=' ){` |
|   202989 |  554 | `					pToken->nType &= ~PH7_TK_EQUAL;` |
|        - |  555 | `					/* Current operator: == */` |
|   202989 |  556 | `					pStream->zText++;` |
|   202989 |  557 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  558 | `						/* Current operator: === */` |
|   165191 |  559 | `						pStream->zText++;` |
|    82598 |  560 | `					}` |
|   836261 |  561 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  562 | `					/* Array operator: => */` |
|    31853 |  563 | `					pToken->nType = PH7_TK_ARRAY_OP;` |
|    31853 |  564 | `					pStream->zText++;` |
|    15929 |  565 | `				}else{` |
|        - |  566 | `					/* TICKET 1433-0010: Reference operator '=&' */` |
|   702921 |  567 | `					const unsigned char *zCur = pStream->zText;` |
|   702921 |  568 | `					sxu32 nLine = 0;` |
|  1405653 |  569 | `					while( zCur < pStream->zEnd && zCur[0] < 0xc0 && SyisSpace(zCur[0]) ){` |
|   702737 |  570 | `						if( zCur[0] == '\n' ){` |
|       10 |  571 | `							nLine++;` |
|        4 |  572 | `						}` |
|   702737 |  573 | `						zCur++;` |
|        5 |  574 | `					}` |
|   702921 |  575 | `					if( zCur < pStream->zEnd && zCur[0] == '&' ){` |
|        - |  576 | `						/* Current operator: =& */` |
|      207 |  577 | `						pToken->nType &= ~PH7_TK_EQUAL;` |
|      207 |  578 | `						SyStringInitFromBuf(pStr,"=&",sizeof("=&")-1);` |
|        - |  579 | `						/* Update token stream */` |
|      207 |  580 | `						pStream->zText = &zCur[1];` |
|      207 |  581 | `						pStream->nLine += nLine;` |
|      101 |  582 | `					}` |
|        - |  583 | `				}` |
|   468874 |  584 | `			}` |
|   937753 |  585 | `			break;` |
|    65774 |  586 | `		case '!':` |
|   131553 |  587 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  588 | `				/* Current operator: != */` |
|    70397 |  589 | `				pStream->zText++;` |
|    70397 |  590 | `				if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  591 | `					/* Current operator: !== */` |
|    56355 |  592 | `					pStream->zText++;` |
|    28175 |  593 | `				}` |
|    35196 |  594 | `			}` |
|   131553 |  595 | `			break;` |
|    68318 |  596 | `		case '&':` |
|   136641 |  597 | `			pToken->nType \|= PH7_TK_AMPER;` |
|   136641 |  598 | `			if( pStream->zText < pStream->zEnd ){` |
|   136641 |  599 | `				if( pStream->zText[0] == '&' ){` |
|    75231 |  600 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|        - |  601 | `					/* Current operator: && */` |
|    75231 |  602 | `					pStream->zText++;` |
|    99028 |  603 | `				}else if( pStream->zText[0] == '=' ){` |
|       15 |  604 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|        - |  605 | `					/* Current operator: &= */` |
|       15 |  606 | `					pStream->zText++;` |
|        7 |  607 | `				}` |
|    68318 |  608 | `			}` |
|   136641 |  609 | `			break;` |
|    33024 |  610 | `		case '\|':` |
|    66053 |  611 | `			if( pStream->zText < pStream->zEnd ){` |
|    66053 |  612 | `				if( pStream->zText[0] == '\|' ){` |
|        - |  613 | `					/* Current operator: \|\| */` |
|    65505 |  614 | `					pStream->zText++;` |
|    33303 |  615 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  616 | `					/* Current operator: \|= */` |
|       19 |  617 | `					pStream->zText++;` |
|      544 |  618 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  619 | `					/* Current operator: \|> (PHP 8.5 pipe) */` |
|       27 |  620 | `					pStream->zText++;` |
|       13 |  621 | `				}` |
|    33024 |  622 | `			}` |
|    66053 |  623 | `			break;` |
|    35873 |  624 | `		case '+':` |
|    71751 |  625 | `			if( pStream->zText < pStream->zEnd ){` |
|    71751 |  626 | `				if( pStream->zText[0] == '+' ){` |
|        - |  627 | `					/* Current operator: ++ */` |
|    23989 |  628 | `					pStream->zText++;` |
|    59759 |  629 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  630 | `					/* Current operator: += */` |
|     4791 |  631 | `					pStream->zText++;` |
|     2393 |  632 | `				}` |
|    35873 |  633 | `			}` |
|    71751 |  634 | `			break;` |
|    54011 |  635 | `		case '-':` |
|   108027 |  636 | `			if( pStream->zText < pStream->zEnd ){` |
|   108027 |  637 | `				if( pStream->zText[0] == '-' ){` |
|        - |  638 | `					/* Current operator: -- */` |
|    18749 |  639 | `					pStream->zText++;` |
|    98655 |  640 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  641 | `					/* Current operator: -= */` |
|       18 |  642 | `					pStream->zText++;` |
|    89275 |  643 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  644 | `					/* Current operator: -> */` |
|    27243 |  645 | `					pStream->zText++;` |
|    13619 |  646 | `				}` |
|    54011 |  647 | `			}` |
|   108027 |  648 | `			break;` |
|     2578 |  649 | `		case '*':` |
|     5161 |  650 | `			if( pStream->zText < pStream->zEnd ){` |
|     5161 |  651 | `				if( pStream->zText[0] == '*' ){` |
|        - |  652 | `					/* Current operator: ** or **= */` |
|      151 |  653 | `					pStream->zText++;` |
|      151 |  654 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  655 | `						/* Current operator: **= */` |
|       27 |  656 | `						pStream->zText++;` |
|       14 |  657 | `					}` |
|     5086 |  658 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  659 | `					/* Current operator: *= */` |
|       37 |  660 | `					pStream->zText++;` |
|       17 |  661 | `				}` |
|     2578 |  662 | `			}` |
|     5161 |  663 | `			break;` |
|     2404 |  664 | `		case '/':` |
|     4813 |  665 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  666 | `				/* Current operator: /= */` |
|       17 |  667 | `				pStream->zText++;` |
|        8 |  668 | `			}` |
|     4813 |  669 | `			break;` |
|    16414 |  670 | `		case '%':` |
|    32833 |  671 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  672 | `				/* Current operator: %= */` |
|       13 |  673 | `				pStream->zText++;` |
|        6 |  674 | `			}` |
|    32833 |  675 | `			break;` |
|       20 |  676 | `		case '^':` |
|       41 |  677 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  678 | `				/* Current operator: ^= */` |
|       15 |  679 | `				pStream->zText++;` |
|        7 |  680 | `			}` |
|       41 |  681 | `			break;` |
|    75086 |  682 | `		case '.':` |
|   150177 |  683 | `			if( pStream->zText + 1 < pStream->zEnd && pStream->zText[0] == '.' && pStream->zText[1] == '.' ){` |
|        - |  684 | `				/* Ellipsis: ... */` |
|     5475 |  685 | `				pStream->zText += 2;` |
|     5475 |  686 | `				pToken->nType = PH7_TK_ELLIPSIS;` |
|   147442 |  687 | `			}else if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  688 | `				/* Current operator: .= */` |
|     9589 |  689 | `				pStream->zText++;` |
|     4792 |  690 | `			}` |
|   150177 |  691 | `			break;` |
|    30835 |  692 | `		case '<':` |
|    61675 |  693 | `			if( pStream->zText < pStream->zEnd ){` |
|    61675 |  694 | `				if( pStream->zText[0] == '<' ){` |
|        - |  695 | `					/* Current operator: << */` |
|      223 |  696 | `					pStream->zText++;` |
|      223 |  697 | `					if( pStream->zText < pStream->zEnd ){` |
|      223 |  698 | `						if( pStream->zText[0] == '=' ){` |
|        - |  699 | `							/* Current operator: <<= */` |
|       23 |  700 | `							pStream->zText++;` |
|      212 |  701 | `						}else if( pStream->zText[0] == '<' ){` |
|        - |  702 | `							/* Current Token: <<<  */` |
|      137 |  703 | `							pStream->zText++;` |
|        - |  704 | `							/* This may be the beginning of a Heredoc/Nowdoc string,try to delimit it */` |
|      137 |  705 | `							rc = LexExtractHeredoc(&(*pStream),&(*pToken));` |
|      137 |  706 | `							if( rc == SXRET_OK ){` |
|        - |  707 | `								/* Here/Now doc successfuly extracted */` |
|      137 |  708 | `								return SXRET_OK;` |
|        - |  709 | `							}` |
|      ! 0 |  710 | `						}` |
|       44 |  711 | `					}` |
|    61500 |  712 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  713 | `					/* Current operator: <> */` |
|        5 |  714 | `					pStream->zText++;` |
|    61455 |  715 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  716 | `					/* Current operator: <= or <=> */` |
|     4897 |  717 | `					pStream->zText++;` |
|     4897 |  718 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '>' ){` |
|        - |  719 | `						/* Current operator: <=> */` |
|      150 |  720 | `						pStream->zText++;` |
|       73 |  721 | `					}` |
|     2446 |  722 | `				}` |
|    30769 |  723 | `			}` |
|    61543 |  724 | `			break;` |
|    39879 |  725 | `		case '>':` |
|    79763 |  726 | `			if( pStream->zText < pStream->zEnd ){` |
|    79763 |  727 | `				if( pStream->zText[0] == '>' ){` |
|        - |  728 | `					/* Current operator: >> */` |
|    14063 |  729 | `					pStream->zText++;` |
|    14063 |  730 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  731 | `						/* Current operator: >>= */` |
|       19 |  732 | `						pStream->zText++;` |
|       14 |  733 | `					}` |
|    72734 |  734 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  735 | `					/* Current operator: >= */` |
|    14113 |  736 | `					pStream->zText++;` |
|     7054 |  737 | `				}` |
|    39879 |  738 | `			}` |
|    79763 |  739 | `			break;` |
|    16139 |  740 | `		case '?':` |
|    32283 |  741 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '?' ){` |
|        - |  742 | `				/* Null coalescing operator: ?? */` |
|      473 |  743 | `				pStream->zText++;` |
|      473 |  744 | `				if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  745 | `					/* Null coalescing assignment operator (PHP 7.4) */` |
|      165 |  746 | `					pStream->zText++;` |
|       80 |  747 | `				}` |
|    32049 |  748 | `			}else if( (pStream->zEnd - pStream->zText) >= 2` |
|    31815 |  749 | `				&& pStream->zText[0] == '-' && pStream->zText[1] == '>' ){` |
|        - |  750 | `				/* Nullsafe object operator (PHP 8.0): ?-> */` |
|      143 |  751 | `				pStream->zText += 2;` |
|       69 |  752 | `			}` |
|    32278 |  753 | `			break;` |
|     9580 |  754 | `		default:` |
|    19160 |  755 | `			break;` |
|        - |  756 | `		}` |
| 10661203 |  757 | `		if( pStr->nByte <= 0 ){` |
|        - |  758 | `			/* Record token length */` |
| 10661001 |  759 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  5330498 |  760 | `		}` |
| 10661203 |  761 | `		if( pToken->nType & PH7_TK_OP ){` |
|        - |  762 | `			const ph7_expr_op *pOp;` |
|        - |  763 | `			/* Check if the extracted token is an operator */` |
|  2528051 |  764 | `			pOp = PH7_ExprExtractOperator(pStr,(SyToken *)SySetPeek(pStream->pSet));` |
|  2528051 |  765 | `			if( pOp == 0 ){` |
|        - |  766 | `				/* Not an operator */` |
|      ! 0 |  767 | `				pToken->nType &= ~PH7_TK_OP;` |
|      ! 0 |  768 | `				if( pToken->nType <= 0 ){` |
|      ! 0 |  769 | `					pToken->nType = PH7_TK_OTHER;` |
|      ! 0 |  770 | `				}` |
|      ! 0 |  771 | `			}else{` |
|        - |  772 | `				/* Save the instance associated with this operator for later processing */` |
|  2528051 |  773 | `				pToken->pUserData = (void *)pOp;` |
|        - |  774 | `			}` |
|  1264023 |  775 | `		}` |
|        - |  776 | `	}` |
|        - |  777 | `	/* Tell the upper-layer to save the extracted token for later processing */` |
| 15742807 |  778 | `	return SXRET_OK;` |
|  8544934 |  779 | `}` |
|        - |  780 | `/* SPDX-SnippetBegin */` |
|        - |  781 | `/* SPDX-SnippetCopyrightText: SQLite mkkeywordhash.c (D. Richard Hipp and the SQLite authors <https://sqlite.org/>); adapted for the PH7 engine by Chems mrad */` |
|        - |  782 | `/* SPDX-License-Identifier: blessing */` |
|        - |  783 | `/***** This file contains automatically generated code ******` |
|        - |  784 | `**` |
|        - |  785 | `** The code in this file has been automatically generated by` |
|        - |  786 | `**` |
|        - |  787 | `**     $Header: /sqlite/sqlite/tool/mkkeywordhash.c` |
|        - |  788 | `**` |
|        - |  789 | `** Sligthly modified by Chems mrad <chm@symisc.net> for the PH7 engine.` |
|        - |  790 | `**` |
|        - |  791 | `** The code in this file implements a function that determines whether` |
|        - |  792 | `** or not a given identifier is really a PHP keyword.  The same thing` |
|        - |  793 | `** might be implemented more directly using a hand-written hash table.` |
|        - |  794 | `** But by using this automatically generated code, the size of the code` |
|        - |  795 | `** is substantially reduced.  This is important for embedded applications` |
|        - |  796 | `** on platforms with limited memory.` |
|        - |  797 | `*/` |
|        - |  798 | `/* Hash score: 103 */` |
|  4171815 |  799 | `static sxu32 KeywordCode(const char *z, int n){` |
|        - |  800 | `  /* zText[] encodes 532 bytes of keywords in 333 bytes */` |
|        - |  801 | `  /*   extendswitchprintegerequire_oncenddeclareturnamespacechobject      */` |
|        - |  802 | `  /*   hrowbooleandefaultrycaselfinalistaticlonewconstringlobaluse        */` |
|        - |  803 | `  /*   lseifloatvarrayANDIEchoUSECHOabstractclasscontinuendifunction      */` |
|        - |  804 | `  /*   diendwhilevaldoexitgotoimplementsinclude_oncemptyinstanceof        */` |
|        - |  805 | `  /*   interfacendforeachissetparentprivateprotectedpublicatchunset       */` |
|        - |  806 | `  /*   xorARRAYASArrayEXITUNSETXORbreak                                   */` |
|        - |  807 | `  static const char zText[332] = {` |
|        - |  808 | `    'e','x','t','e','n','d','s','w','i','t','c','h','p','r','i','n','t','e',` |
|        - |  809 | `    'g','e','r','e','q','u','i','r','e','_','o','n','c','e','n','d','d','e',` |
|        - |  810 | `    'c','l','a','r','e','t','u','r','n','a','m','e','s','p','a','c','e','c',` |
|        - |  811 | `    'h','o','b','j','e','c','t','h','r','o','w','b','o','o','l','e','a','n',` |
|        - |  812 | `    'd','e','f','a','u','l','t','r','y','c','a','s','e','l','f','i','n','a',` |
|        - |  813 | `    'l','i','s','t','a','t','i','c','l','o','n','e','w','c','o','n','s','t',` |
|        - |  814 | `    'r','i','n','g','l','o','b','a','l','u','s','e','l','s','e','i','f','l',` |
|        - |  815 | `    'o','a','t','v','a','r','r','a','y','A','N','D','I','E','c','h','o','U',` |
|        - |  816 | `    'S','E','C','H','O','a','b','s','t','r','a','c','t','c','l','a','s','s',` |
|        - |  817 | `    'c','o','n','t','i','n','u','e','n','d','i','f','u','n','c','t','i','o',` |
|        - |  818 | `    'n','d','i','e','n','d','w','h','i','l','e','v','a','l','d','o','e','x',` |
|        - |  819 | `    'i','t','g','o','t','o','i','m','p','l','e','m','e','n','t','s','i','n',` |
|        - |  820 | `    'c','l','u','d','e','_','o','n','c','e','m','p','t','y','i','n','s','t',` |
|        - |  821 | `    'a','n','c','e','o','f','i','n','t','e','r','f','a','c','e','n','d','f',` |
|        - |  822 | `    'o','r','e','a','c','h','i','s','s','e','t','p','a','r','e','n','t','p',` |
|        - |  823 | `    'r','i','v','a','t','e','p','r','o','t','e','c','t','e','d','p','u','b',` |
|        - |  824 | `    'l','i','c','a','t','c','h','u','n','s','e','t','x','o','r','A','R','R',` |
|        - |  825 | `    'A','Y','A','S','A','r','r','a','y','E','X','I','T','U','N','S','E','T',` |
|        - |  826 | `    'X','O','R','b','r','e','a','k'` |
|        - |  827 | `  };` |
|        - |  828 | `  static const unsigned char aHash[151] = {` |
|        - |  829 | `       0,   0,   4,  83,   0,  61,  39,  12,   0,  33,  77,   0,  48,` |
|        - |  830 | `       0,   2,  65,  67,   0,   0,   0,  47,   0,   0,  40,   0,  15,` |
|        - |  831 | `      74,   0,  51,   0,  76,   0,   0,  20,   0,   0,   0,  50,   0,` |
|        - |  832 | `      80,  34,   0,  36,   0,   0,  64,  16,   0,   0,  17,   0,   1,` |
|        - |  833 | `      19,  84,  66,   0,  43,  45,  78,   0,   0,  53,  56,   0,   0,` |
|        - |  834 | `       0,  23,  49,   0,   0,  13,  31,  54,   7,   0,   0,  25,   0,` |
|        - |  835 | `      72,  14,   0,  71,   0,  38,   6,   0,   0,   0,  73,   0,   0,` |
|        - |  836 | `       3,   0,  41,   5,  52,  57,  32,   0,  60,  63,   0,  69,  82,` |
|        - |  837 | `      30,   0,  79,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,` |
|        - |  838 | `       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  81,   0,   0,` |
|        - |  839 | `      62,   0,  11,   0,   0,  58,   0,   0,   0,   0,  59,  75,   0,` |
|        - |  840 | `       0,   0,   0,   0,   0,  35,  27,   0` |
|        - |  841 | `  };` |
|        - |  842 | `  static const unsigned char aNext[84] = {` |
|        - |  843 | `       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,` |
|        - |  844 | `       0,   0,   8,   0,   0,   0,  10,   0,   0,   0,   0,   0,   0,` |
|        - |  845 | `       0,   0,   0,   0,  28,   0,   0,   0,   0,   0,   0,   0,   0,` |
|        - |  846 | `       0,   0,   0,   0,   0,  44,   0,  18,   0,   0,   0,   0,   0,` |
|        - |  847 | `       0,  46,   0,  29,   0,   0,   0,  22,   0,   0,   0,   0,  26,` |
|        - |  848 | `       0,  21,  24,   0,   0,  68,   0,   0,   9,  37,   0,   0,   0,` |
|        - |  849 | `      42,   0,   0,   0,  70,  55` |
|        - |  850 | `  };` |
|        - |  851 | `  static const unsigned char aLen[84] = {` |
|        - |  852 | `       7,   9,   6,   5,   7,  12,   7,   2,  10,   7,   6,   9,   4,` |
|        - |  853 | `       6,   5,   7,   4,   3,   7,   3,   4,   4,   5,   4,   6,   5,` |
|        - |  854 | `       2,   3,   5,   6,   6,   3,   6,   4,   2,   5,   3,   5,   3,` |
|        - |  855 | `       3,   4,   3,   4,   8,   5,   2,   8,   5,   8,   3,   8,   5,` |
|        - |  856 | `       4,   2,   4,   4,  10,  12,   7,   5,  10,   9,   3,   6,  10,` |
|        - |  857 | `       3,   7,   2,   5,   6,   7,   9,   6,   5,   5,   3,   5,   2,` |
|        - |  858 | `       5,   4,   5,   3,   2,   5` |
|        - |  859 | `  };` |
|        - |  860 | `  static const sxu16 aOffset[84] = {` |
|        - |  861 | `       0,   3,   6,  12,  14,  20,  20,  21,  31,  34,  39,  44,  52,` |
|        - |  862 | `      55,  60,  65,  65,  70,  72,  78,  81,  83,  86,  90,  92,  97,` |
|        - |  863 | `     100, 100, 103, 106, 111, 117, 119, 119, 123, 124, 129, 130, 135,` |
|        - |  864 | `     137, 139, 143, 145, 149, 157, 159, 162, 169, 173, 181, 183, 186,` |
|        - |  865 | `     190, 194, 196, 200, 204, 214, 214, 225, 230, 240, 240, 248, 248,` |
|        - |  866 | `     251, 251, 252, 258, 263, 269, 276, 285, 290, 295, 300, 303, 308,` |
|        - |  867 | `     310, 315, 319, 324, 325, 327` |
|        - |  868 | `  };` |
|        - |  869 | `  static const sxu32 aCode[84] = {` |
|        - |  870 | `    PH7_TKWRD_EXTENDS,   PH7_TKWRD_ENDSWITCH,   PH7_TKWRD_SWITCH,    PH7_TKWRD_PRINT,   PH7_TKWRD_INT,` |
|        - |  871 | `    PH7_TKWRD_REQONCE,   PH7_TKWRD_REQUIRE,     PH7_TK_ID /* 'eq' PH7-ism removed */, PH7_TKWRD_ENDDEC, PH7_TKWRD_DECLARE,` |
|        - |  872 | `    PH7_TKWRD_RETURN,    PH7_TKWRD_NAMESPACE,   PH7_TKWRD_ECHO,      PH7_TKWRD_OBJECT,    PH7_TKWRD_THROW,` |
|        - |  873 | `    PH7_TKWRD_BOOL,      PH7_TKWRD_BOOL,        PH7_TKWRD_AND,       PH7_TKWRD_DEFAULT,   PH7_TKWRD_TRY,` |
|        - |  874 | `    PH7_TKWRD_CASE,      PH7_TKWRD_SELF,        PH7_TKWRD_FINAL,     PH7_TKWRD_LIST,      PH7_TKWRD_STATIC,` |
|        - |  875 | `    PH7_TKWRD_CLONE,     PH7_TK_ID /* 'ne' PH7-ism removed */, PH7_TKWRD_NEW,  PH7_TKWRD_CONST,     PH7_TKWRD_STRING,` |
|        - |  876 | `    PH7_TKWRD_GLOBAL,    PH7_TKWRD_USE,         PH7_TKWRD_ELIF,      PH7_TKWRD_ELSE,      PH7_TKWRD_IF,` |
|        - |  877 | `    PH7_TKWRD_FLOAT,     PH7_TKWRD_VAR,         PH7_TKWRD_ARRAY,     PH7_TKWRD_AND,       PH7_TKWRD_DIE,` |
|        - |  878 | `    PH7_TKWRD_ECHO,      PH7_TKWRD_USE,         PH7_TKWRD_ECHO,      PH7_TKWRD_ABSTRACT,  PH7_TKWRD_CLASS,` |
|        - |  879 | `    PH7_TKWRD_AS,        PH7_TKWRD_CONTINUE,    PH7_TKWRD_ENDIF,     PH7_TKWRD_FUNCTION,  PH7_TKWRD_DIE,` |
|        - |  880 | `    PH7_TKWRD_ENDWHILE,  PH7_TKWRD_WHILE,       PH7_TKWRD_EVAL,      PH7_TKWRD_DO,        PH7_TKWRD_EXIT,` |
|        - |  881 | `    PH7_TKWRD_GOTO,      PH7_TKWRD_IMPLEMENTS,  PH7_TKWRD_INCONCE,   PH7_TKWRD_INCLUDE,   PH7_TKWRD_EMPTY,` |
|        - |  882 | `    PH7_TKWRD_INSTANCEOF,PH7_TKWRD_INTERFACE,   PH7_TKWRD_INT,       PH7_TKWRD_ENDFOR,    PH7_TKWRD_END4EACH,` |
|        - |  883 | `    PH7_TKWRD_FOR,       PH7_TKWRD_FOREACH,     PH7_TKWRD_OR,        PH7_TKWRD_ISSET,     PH7_TKWRD_PARENT,` |
|        - |  884 | `    PH7_TKWRD_PRIVATE,   PH7_TKWRD_PROTECTED,   PH7_TKWRD_PUBLIC,    PH7_TKWRD_CATCH,     PH7_TKWRD_UNSET,` |
|        - |  885 | `    PH7_TKWRD_XOR,       PH7_TKWRD_ARRAY,       PH7_TKWRD_AS,        PH7_TKWRD_ARRAY,     PH7_TKWRD_EXIT,` |
|        - |  886 | `    PH7_TKWRD_UNSET,     PH7_TKWRD_XOR,         PH7_TKWRD_OR,        PH7_TKWRD_BREAK` |
|        - |  887 | `  };` |
|        - |  888 | `  int h, i;` |
|  4171815 |  889 | `  if( n<2 ) return PH7_TK_ID;` |
|        - |  890 | ``  /* Hash through UNSIGNED bytes: `char` is signed on most targets, so an`` |
|        - |  891 | `   * identifier carrying a high byte (php allows 0x80-0xFF in identifiers, and` |
|        - |  892 | ``   * every UTF-8 name has them) made the xor negative, and C's `%` keeps that`` |
|        - |  893 | `   * sign — aHash[-46] read off the front of the table. ASCII is unaffected, so` |
|        - |  894 | `   * the generated keyword buckets still resolve exactly as before. */` |
|  4171815 |  895 | `  h = (int)(((sxu32)(sxu8)z[0]*4) ^ ((sxu32)(sxu8)z[n-1]*3) ^ (sxu32)n) % 151;` |
|  5995463 |  896 | `  for(i=((int)aHash[h])-1; i>=0; i=((int)aNext[i])-1){` |
|  3525471 |  897 | `    if( (int)aLen[i]==n && SyMemcmp(&zText[aOffset[i]],z,n)==0 ){` |
|        - |  898 | `       /* PH7_TKWRD_EXTENDS */` |
|        - |  899 | `       /* PH7_TKWRD_ENDSWITCH */` |
|        - |  900 | `       /* PH7_TKWRD_SWITCH */` |
|        - |  901 | `       /* PH7_TKWRD_PRINT */` |
|        - |  902 | `       /* PH7_TKWRD_INT */` |
|        - |  903 | `       /* PH7_TKWRD_REQONCE */` |
|        - |  904 | `       /* PH7_TKWRD_REQUIRE */` |
|        - |  905 | `       /* PH7_TK_ID */` |
|        - |  906 | `       /* PH7_TKWRD_ENDDEC */` |
|        - |  907 | `       /* PH7_TKWRD_DECLARE */` |
|        - |  908 | `       /* PH7_TKWRD_RETURN */` |
|        - |  909 | `       /* PH7_TKWRD_NAMESPACE */` |
|        - |  910 | `       /* PH7_TKWRD_ECHO */` |
|        - |  911 | `       /* PH7_TKWRD_OBJECT */` |
|        - |  912 | `       /* PH7_TKWRD_THROW */` |
|        - |  913 | `       /* PH7_TKWRD_BOOL */` |
|        - |  914 | `       /* PH7_TKWRD_BOOL */` |
|        - |  915 | `       /* PH7_TKWRD_AND */` |
|        - |  916 | `       /* PH7_TKWRD_DEFAULT */` |
|        - |  917 | `       /* PH7_TKWRD_TRY */` |
|        - |  918 | `       /* PH7_TKWRD_CASE */` |
|        - |  919 | `       /* PH7_TKWRD_SELF */` |
|        - |  920 | `       /* PH7_TKWRD_FINAL */` |
|        - |  921 | `       /* PH7_TKWRD_LIST */` |
|        - |  922 | `       /* PH7_TKWRD_STATIC */` |
|        - |  923 | `       /* PH7_TKWRD_CLONE */` |
|        - |  924 | `       /* PH7_TK_ID */` |
|        - |  925 | `       /* PH7_TKWRD_NEW */` |
|        - |  926 | `       /* PH7_TKWRD_CONST */` |
|        - |  927 | `       /* PH7_TKWRD_STRING */` |
|        - |  928 | `       /* PH7_TKWRD_GLOBAL */` |
|        - |  929 | `       /* PH7_TKWRD_USE */` |
|        - |  930 | `       /* PH7_TKWRD_ELIF */` |
|        - |  931 | `       /* PH7_TKWRD_ELSE */` |
|        - |  932 | `       /* PH7_TKWRD_IF */` |
|        - |  933 | `       /* PH7_TKWRD_FLOAT */` |
|        - |  934 | `       /* PH7_TKWRD_VAR */` |
|        - |  935 | `       /* PH7_TKWRD_ARRAY */` |
|        - |  936 | `       /* PH7_TKWRD_AND */` |
|        - |  937 | `       /* PH7_TKWRD_DIE */` |
|        - |  938 | `       /* PH7_TKWRD_ECHO */` |
|        - |  939 | `       /* PH7_TKWRD_USE */` |
|        - |  940 | `       /* PH7_TKWRD_ECHO */` |
|        - |  941 | `       /* PH7_TKWRD_ABSTRACT */` |
|        - |  942 | `       /* PH7_TKWRD_CLASS */` |
|        - |  943 | `       /* PH7_TKWRD_AS */` |
|        - |  944 | `       /* PH7_TKWRD_CONTINUE */` |
|        - |  945 | `       /* PH7_TKWRD_ENDIF */` |
|        - |  946 | `       /* PH7_TKWRD_FUNCTION */` |
|        - |  947 | `       /* PH7_TKWRD_DIE */` |
|        - |  948 | `       /* PH7_TKWRD_ENDWHILE */` |
|        - |  949 | `       /* PH7_TKWRD_WHILE */` |
|        - |  950 | `       /* PH7_TKWRD_EVAL */` |
|        - |  951 | `       /* PH7_TKWRD_DO */` |
|        - |  952 | `       /* PH7_TKWRD_EXIT */` |
|        - |  953 | `       /* PH7_TKWRD_GOTO */` |
|        - |  954 | `       /* PH7_TKWRD_IMPLEMENTS */` |
|        - |  955 | `       /* PH7_TKWRD_INCONCE */` |
|        - |  956 | `       /* PH7_TKWRD_INCLUDE */` |
|        - |  957 | `       /* PH7_TKWRD_EMPTY */` |
|        - |  958 | `       /* PH7_TKWRD_INSTANCEOF */` |
|        - |  959 | `       /* PH7_TKWRD_INTERFACE */` |
|        - |  960 | `       /* PH7_TKWRD_INT */` |
|        - |  961 | `       /* PH7_TKWRD_ENDFOR */` |
|        - |  962 | `       /* PH7_TKWRD_END4EACH */` |
|        - |  963 | `       /* PH7_TKWRD_FOR */` |
|        - |  964 | `       /* PH7_TKWRD_FOREACH */` |
|        - |  965 | `       /* PH7_TKWRD_OR */` |
|        - |  966 | `       /* PH7_TKWRD_ISSET */` |
|        - |  967 | `       /* PH7_TKWRD_PARENT */` |
|        - |  968 | `       /* PH7_TKWRD_PRIVATE */` |
|        - |  969 | `       /* PH7_TKWRD_PROTECTED */` |
|        - |  970 | `       /* PH7_TKWRD_PUBLIC */` |
|        - |  971 | `       /* PH7_TKWRD_CATCH */` |
|        - |  972 | `       /* PH7_TKWRD_UNSET */` |
|        - |  973 | `       /* PH7_TKWRD_XOR */` |
|        - |  974 | `       /* PH7_TKWRD_ARRAY */` |
|        - |  975 | `       /* PH7_TKWRD_AS */` |
|        - |  976 | `       /* PH7_TKWRD_ARRAY */` |
|        - |  977 | `       /* PH7_TKWRD_EXIT */` |
|        - |  978 | `       /* PH7_TKWRD_UNSET */` |
|        - |  979 | `       /* PH7_TKWRD_XOR */` |
|        - |  980 | `       /* PH7_TKWRD_OR */` |
|        - |  981 | `       /* PH7_TKWRD_BREAK */` |
|  1701823 |  982 | `      return aCode[i];` |
|        - |  983 | `    }` |
|   911829 |  984 | `  }` |
|        - |  985 | `  /* Linear fallback for keywords not in the auto-generated hash table */` |
|  2469997 |  986 | `  if( n==5 && SyMemcmp(z,"trait",5)==0 ) return PH7_TKWRD_TRAIT;` |
|  2469811 |  987 | `  if( n==9 && SyMemcmp(z,"insteadof",9)==0 ) return PH7_TKWRD_INSTEADOF;` |
|  2469799 |  988 | `  if( n==7 && SyMemcmp(z,"finally",7)==0 ) return PH7_TKWRD_FINALLY;` |
|  2469507 |  989 | `  if( n==5 && SyMemcmp(z,"yield",5)==0 ) return PH7_TKWRD_YIELD;` |
|  2469027 |  990 | `  if( n==5 && SyMemcmp(z,"match",5)==0 ) return PH7_TKWRD_MATCH;` |
|  2468881 |  991 | `  if( n==2 && SyMemcmp(z,"fn",2)==0 ) return PH7_TKWRD_FN;   /* PHP 7.4 arrow functions */` |
|  2464643 |  992 | `  return PH7_TK_ID;` |
|  2085910 |  993 | `}` |
|        - |  994 | `/* --- End of Automatically generated code --- */` |
|        - |  995 | `/* SPDX-SnippetEnd */` |
|        - |  996 | `/*` |
|        - |  997 | ` * Keyword lookup as php does it: CASE-INSENSITIVELY. 'IF', 'Function' and 'NEW'` |
|        - |  998 | ` * are the very same tokens as 'if', 'function' and 'new', so the generated table` |
|        - |  999 | ` * above — whose hash buckets and SyMemcmp() rows are byte-exact lower case — is` |
|        - | 1000 | ` * probed through an ASCII-folded COPY of the identifier. KeywordCode() itself` |
|        - | 1001 | ` * stays byte-exact so the generated code needs no regeneration.` |
|        - | 1002 | ` *` |
|        - | 1003 | ` * The fold is ASCII-only on purpose: libc tolower() follows LC_CTYPE (a tr_TR` |
|        - | 1004 | ` * embedder would stop recognising 'IF') where php's lexer is locale-independent,` |
|        - | 1005 | ` * and identifier bytes >= 0x80 — php allows them, and every UTF-8 name has` |
|        - | 1006 | ` * them — must pass through untouched.` |
|        - | 1007 | ` *` |
|        - | 1008 | ` * A handful of UPPER-case rows in the table ('ARRAY', 'AS', 'EXIT', 'UNSET',` |
|        - | 1009 | ` * 'XOR', 'AND', 'OR', 'ECHO', 'Echo', 'Array', 'USE') are PH7's old partial hack` |
|        - | 1010 | ` * for this same problem. Each has a lower-case twin, so folding makes them` |
|        - | 1011 | ` * unreachable-but-harmless rather than wrong.` |
|        - | 1012 | ` */` |
|  5081604 | 1013 | `static sxu32 KeywordCodeCI(const char *zRaw, int n)` |
|        5 | 1014 | `{` |
|        - | 1015 | `	/* Longest row in the table above: 'require_once'/'include_once' (12 bytes) */` |
|        - | 1016 | `	char zFold[12];` |
|        - | 1017 | `	int i;` |
|  5081609 | 1018 | `	if( n < 2 \|\| n > (int)sizeof(zFold) ){` |
|        - | 1019 | `		/* Too short or too long to be any keyword: skip the fold and the probe */` |
|   909799 | 1020 | `		return PH7_TK_ID;` |
|        - | 1021 | `	}` |
| 26497257 | 1022 | `	for( i = 0 ; i < n ; ++i ){` |
| 22325447 | 1023 | `		unsigned char c = (unsigned char)zRaw[i];` |
| 22325447 | 1024 | `		zFold[i] = (char)((c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c);` |
| 11162726 | 1025 | `	}` |
|  4171815 | 1026 | `	return KeywordCode(zFold,n);` |
|  2540807 | 1027 | `}` |
|        - | 1028 | `/*` |
|        - | 1029 | ` * Extract a heredoc/nowdoc text from a raw PHP input.` |
|        - | 1030 | ` * According to the PHP language reference manual:` |
|        - | 1031 | ` *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier` |
|        - | 1032 | ` *  is provided, then a newline. The string itself follows, and then the same identifier again` |
|        - | 1033 | ` *  to close the quotation.` |
|        - | 1034 | ` *  The closing identifier must begin in the first column of the line. Also, the identifier must` |
|        - | 1035 | ` *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric` |
|        - | 1036 | ` *  characters and underscores, and must start with a non-digit character or underscore.` |
|        - | 1037 | ` *  Heredoc text behaves just like a double-quoted string, without the double quotes.` |
|        - | 1038 | ` *  This means that quotes in a heredoc do not need to be escaped, but the escape codes listed` |
|        - | 1039 | ` *  above can still be used. Variables are expanded, but the same care must be taken when expressing` |
|        - | 1040 | ` *  complex variables inside a heredoc as with strings.` |
|        - | 1041 | ` *  Nowdocs are to single-quoted strings what heredocs are to double-quoted strings.` |
|        - | 1042 | ` *  A nowdoc is specified similarly to a heredoc, but no parsing is done inside a nowdoc.` |
|        - | 1043 | ` *  The construct is ideal for embedding PHP code or other large blocks of text without the need` |
|        - | 1044 | ` *  for escaping. It shares some features in common with the SGML <![CDATA[ ]]> construct, in that` |
|        - | 1045 | ` *  it declares a block of text which is not for parsing.` |
|        - | 1046 | ` *  A nowdoc is identified with the same <<< sequence used for heredocs, but the identifier which follows` |
|        - | 1047 | ` *  is enclosed in single quotes, e.g. <<<'EOT'. All the rules for heredoc identifiers also apply to nowdoc` |
|        - | 1048 | ` *  identifiers, especially those regarding the appearance of the closing identifier.` |
|        - | 1049 | ` * Symisc Extension:` |
|        - | 1050 | ` * The closing delimiter can now start with a digit or undersocre or it can be an UTF-8 stream.` |
|        - | 1051 | ` * Example:` |
|        - | 1052 | ` *  <<<123` |
|        - | 1053 | ` *    HEREDOC Here` |
|        - | 1054 | ` * 123` |
|        - | 1055 | ` *  or` |
|        - | 1056 | ` *  <<<___` |
|        - | 1057 | ` *   HEREDOC Here` |
|        - | 1058 | ` *  ___` |
|        - | 1059 | ` */` |
|      132 | 1060 | `static sxi32 LexExtractHeredoc(SyStream *pStream,SyToken *pToken)` |
|        5 | 1061 | `{` |
|      137 | 1062 | `	const unsigned char *zIn  = pStream->zText;` |
|      137 | 1063 | `	const unsigned char *zEnd = pStream->zEnd;` |
|        - | 1064 | `	const unsigned char *zPtr;` |
|      137 | 1065 | `	sxu8 bNowDoc = FALSE;` |
|        - | 1066 | `	SyString sDelim;` |
|        - | 1067 | `	SyString sStr;` |
|        - | 1068 | `	/* Jump leading white spaces */` |
|      149 | 1069 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|       13 | 1070 | `		zIn++;` |
|        1 | 1071 | `	}` |
|      137 | 1072 | `	if( zIn >= zEnd ){` |
|        - | 1073 | `		/* A simple symbol,return immediately */` |
|      ! 0 | 1074 | `		return SXERR_CONTINUE;` |
|        - | 1075 | `	}` |
|      137 | 1076 | `	if( zIn[0] == '\'' \|\| zIn[0] == '"' ){` |
|        - | 1077 | `		/* Make sure we are dealing with a nowdoc */` |
|       56 | 1078 | `		bNowDoc =  zIn[0] == '\'' ? TRUE : FALSE;` |
|       56 | 1079 | `		zIn++;` |
|       26 | 1080 | `	}` |
|      137 | 1081 | `	if( !LEX_LABEL_BYTE(zIn[0]) ){` |
|        - | 1082 | `		/* Invalid delimiter,return immediately */` |
|      ! 0 | 1083 | `		return SXERR_CONTINUE;` |
|        - | 1084 | `	}` |
|        - | 1085 | `	/* Isolate the identifier (php's label bytes; see LEX_LABEL_START) */` |
|      137 | 1086 | `	sDelim.zString = (const char *)zIn;` |
|      137 | 1087 | `	zPtr = zIn;` |
|      707 | 1088 | `	while( zPtr < zEnd && LEX_LABEL_BYTE(zPtr[0]) ){` |
|      509 | 1089 | `		zPtr++;` |
|        5 | 1090 | `	}` |
|      137 | 1091 | `	zIn = zPtr;` |
|        - | 1092 | `	/* Get the identifier length */` |
|      137 | 1093 | `	sDelim.nByte = (sxu32)((const char *)zIn-sDelim.zString);` |
|      137 | 1094 | `	if( zIn[0] == '"' \|\| (bNowDoc && zIn[0] == '\'') ){` |
|        - | 1095 | `		/* Jump the trailing single quote */` |
|       56 | 1096 | `		zIn++;` |
|       26 | 1097 | `	}` |
|        - | 1098 | `	/* Jump trailing white spaces */` |
|      137 | 1099 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|      ! 0 | 1100 | `		zIn++;` |
|      ! 0 | 1101 | `	}` |
|      137 | 1102 | `	if( sDelim.nByte <= 0 \|\| zIn >= zEnd \|\| zIn[0] != '\n' ){` |
|        - | 1103 | `		/* Invalid syntax */` |
|      ! 0 | 1104 | `		return SXERR_CONTINUE;` |
|        - | 1105 | `	}` |
|      137 | 1106 | `	pStream->nLine++; /* Increment line counter */` |
|      137 | 1107 | `	zIn++;` |
|        - | 1108 | `	/* Isolate the delimited string */` |
|      137 | 1109 | `	sStr.zString = (const char *)zIn;` |
|        - | 1110 | `	/* PHP 7.3 flexible heredoc/nowdoc: the closing marker may be preceded` |
|        - | 1111 | `	 * by whitespace (spaces/tabs), and may be followed by any non-identifier` |
|        - | 1112 | `	 * character. The indent count is recorded in pToken->pUserData and the` |
|        - | 1113 | `	 * compile phase strips it from each body line. */` |
|        - | 1114 | `	{` |
|      137 | 1115 | `		const unsigned char *zMarkerLine = zIn; /* Start of marker's line (set on match) */` |
|      137 | 1116 | `		sxu32 nIndent = 0;` |
|      308 | 1117 | `		for(;;){` |
|      379 | 1118 | `			const unsigned char *zLineStart = zIn;` |
|        - | 1119 | `			/* Skip leading space/tab on this line */` |
|     1064 | 1120 | `			while( zIn < zEnd && (zIn[0] == ' ' \|\| zIn[0] == '\t') ){` |
|      501 | 1121 | `				zIn++;` |
|        3 | 1122 | `			}` |
|      374 | 1123 | `			if( (sxu32)(zEnd - zIn) >= sDelim.nByte` |
|      378 | 1124 | `				&& SyMemcmp((const void *)sDelim.zString,(const void *)zIn,sDelim.nByte) == 0 ){` |
|        - | 1125 | `				int bIdentCont;` |
|      135 | 1126 | `				zPtr = &zIn[sDelim.nByte];` |
|        - | 1127 | `				/* Disambiguate: the next byte must not continue an identifier` |
|        - | 1128 | `				 * (php's label bytes; see LEX_LABEL_START). */` |
|      200 | 1129 | `				bIdentCont = zPtr < zEnd && LEX_LABEL_BYTE(zPtr[0]);` |
|      135 | 1130 | `				if( !bIdentCont ){` |
|        - | 1131 | `					/* Closing marker found */` |
|      135 | 1132 | `					nIndent = (sxu32)(zIn - zLineStart);` |
|      135 | 1133 | `					zMarkerLine = zLineStart;` |
|      135 | 1134 | `					pStream->zText = zPtr; /* Cursor right after identifier */` |
|      135 | 1135 | `					break;` |
|        - | 1136 | `				}` |
|      ! 0 | 1137 | `			}` |
|        - | 1138 | `			/* Not the closing marker on this line; walk to next newline */` |
|     5387 | 1139 | `			while( zIn < zEnd && zIn[0] != '\n' ){` |
|     5143 | 1140 | `				zIn++;` |
|        5 | 1141 | `			}` |
|      249 | 1142 | `			if( zIn >= zEnd ){` |
|        - | 1143 | `				/* End of input without finding the closing marker */` |
|        3 | 1144 | `				pStream->zText = pStream->zEnd;` |
|        3 | 1145 | `				zMarkerLine = zIn;` |
|        3 | 1146 | `				break;` |
|        - | 1147 | `			}` |
|      247 | 1148 | `			pStream->nLine++;` |
|      247 | 1149 | `			zIn++;` |
|        5 | 1150 | `		}` |
|        - | 1151 | `		/* Body runs from sStr.zString up to just before the marker line */` |
|      137 | 1152 | `		sStr.nByte = (sxu32)((const char *)zMarkerLine - sStr.zString);` |
|      137 | 1153 | `		pToken->nType = bNowDoc ? PH7_TK_NOWDOC : PH7_TK_HEREDOC;` |
|      137 | 1154 | `		SyStringDupPtr(&pToken->sData,&sStr);` |
|        - | 1155 | `		/* Strip exactly one line terminator that precedes the marker's line. */` |
|      132 | 1156 | `		if( pToken->sData.nByte > 0` |
|      133 | 1157 | `			&& pToken->sData.zString[pToken->sData.nByte - 1] == '\n' ){` |
|      127 | 1158 | `			pToken->sData.nByte--;` |
|      122 | 1159 | `			if( pToken->sData.nByte > 0` |
|      127 | 1160 | `				&& pToken->sData.zString[pToken->sData.nByte - 1] == '\r' ){` |
|      ! 0 | 1161 | `				pToken->sData.nByte--;` |
|      ! 0 | 1162 | `			}` |
|       61 | 1163 | `		}` |
|      137 | 1164 | `		pToken->pUserData = SX_INT_TO_PTR(nIndent);` |
|        - | 1165 | `	}` |
|        - | 1166 | `	/* All done */` |
|      137 | 1167 | `	return SXRET_OK;` |
|       71 | 1168 | `}` |
|        - | 1169 | `/*` |
|        - | 1170 | ` * Tokenize a raw PHP input.` |
|        - | 1171 | ` * This is the public tokenizer called by most code generator routines.` |
|        - | 1172 | ` */` |
|    19678 | 1173 | `PH7_PRIVATE sxi32 PH7_TokenizePHP(const char *zInput,sxu32 nLen,sxu32 nLineStart,SySet *pOut,SySet *pTrivia)` |
|        5 | 1174 | `{` |
|        - | 1175 | `	SyLex sLexer;` |
|        - | 1176 | `	sxi32 rc;` |
|        - | 1177 | `	/* Defense-in-depth cap for internal tokenizer calls that bypass ph7_compile() */` |
|    19683 | 1178 | `	if( nLen > PH7_MAX_INPUT_SIZE ){` |
|      ! 0 | 1179 | `		return SXERR_LIMIT;` |
|        - | 1180 | `	}` |
|        - | 1181 | `	/* Initialize the lexer. pTrivia (may be NULL = discard) rides as the` |
|        - | 1182 | `	 * tokenizer callback's user data: doc-comments (and later attribute` |
|        - | 1183 | `	 * groups) are recorded there instead of entering the token stream. */` |
|    19683 | 1184 | `	rc = SyLexInit(&sLexer,&(*pOut),TokenizePHP,pTrivia);` |
|    19683 | 1185 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1186 | `		return rc;` |
|        - | 1187 | `	}` |
|    19683 | 1188 | `	sLexer.sStream.nLine = nLineStart;` |
|        - | 1189 | `	/* Tokenize input */` |
|    19683 | 1190 | `	rc = SyLexTokenizeInput(&sLexer,zInput,nLen,0,0,0);` |
|        - | 1191 | `	/* Release the lexer */` |
|    19683 | 1192 | `	SyLexRelease(&sLexer);` |
|        - | 1193 | `	/* Tokenization result */` |
|    19683 | 1194 | `	return rc;` |
|     9844 | 1195 | `}` |
|        - | 1196 | `/*` |
|        - | 1197 | ` * High level public tokenizer.` |
|        - | 1198 | ` *  Tokenize the input into PHP tokens and raw tokens [i.e: HTML,XML,Raw text...].` |
|        - | 1199 | ` * According to the PHP language reference manual` |
|        - | 1200 | ` *   When PHP parses a file, it looks for opening and closing tags, which tell PHP` |
|        - | 1201 | ` *   to start and stop interpreting the code between them. Parsing in this manner allows` |
|        - | 1202 | ` *   PHP to be embedded in all sorts of different documents, as everything outside of a pair` |
|        - | 1203 | ` *   of opening and closing tags is ignored by the PHP parser. Most of the time you will see` |
|        - | 1204 | ` *   PHP embedded in HTML documents, as in this example.` |
|        - | 1205 | ` *   <?php echo 'While this is going to be parsed.'; ?>` |
|        - | 1206 | ` *   <p>This will also be ignored.</p>` |
|        - | 1207 | ` *   You can also use more advanced structures:` |
|        - | 1208 | ` *   Example #1 Advanced escaping` |
|        - | 1209 | ` * <?php` |
|        - | 1210 | ` * if ($expression) {` |
|        - | 1211 | ` *   ?>` |
|        - | 1212 | ` *   <strong>This is true.</strong>` |
|        - | 1213 | ` *   <?php` |
|        - | 1214 | ` * } else {` |
|        - | 1215 | ` *   ?>` |
|        - | 1216 | ` *   <strong>This is false.</strong>` |
|        - | 1217 | ` *   <?php` |
|        - | 1218 | ` * }` |
|        - | 1219 | ` * ?>` |
|        - | 1220 | ` * This works as expected, because when PHP hits the ?> closing tags, it simply starts outputting` |
|        - | 1221 | ` * whatever it finds (except for an immediately following newline - see instruction separation ) until it hits` |
|        - | 1222 | ` * another opening tag. The example given here is contrived, of course, but for outputting large blocks of text` |
|        - | 1223 | ` * dropping out of PHP parsing mode is generally more efficient than sending all of the text through echo() or print().` |
|        - | 1224 | ` * There are four different pairs of opening and closing tags which can be used in PHP. Three of those, <?php ?>` |
|        - | 1225 | ` * <script language="php"> </script>  and <? ?> are always available. The other two are short tags and ASP style` |
|        - | 1226 | ` * tags, and can be turned on and off from the php.ini configuration file. As such, while some people find short tags` |
|        - | 1227 | ` * and ASP style tags convenient, they are less portable, and generally not recommended.` |
|        - | 1228 | ` * Note:` |
|        - | 1229 | ` * Also note that if you are embedding PHP within XML or XHTML you will need to use the <?php ?> tags to remain` |
|        - | 1230 | ` * compliant with standards.` |
|        - | 1231 | ` * Example #2 PHP Opening and Closing Tags` |
|        - | 1232 | ` * 1.  <?php echo 'if you want to serve XHTML or XML documents, do it like this'; ?>` |
|        - | 1233 | ` * 2.  <script language="php">` |
|        - | 1234 | ` *       echo 'some editors (like FrontPage) don\'t` |
|        - | 1235 | ` *             like processing instructions';` |
|        - | 1236 | ` *   </script>` |
|        - | 1237 | ` *` |
|        - | 1238 | ` * 3.  <? echo 'this is the simplest, an SGML processing instruction'; ?>` |
|        - | 1239 | ` *   <?= expression ?> This is a shortcut for "<? echo expression ?>"` |
|        - | 1240 | ` */` |
|    14838 | 1241 | `PH7_PRIVATE sxi32 PH7_TokenizeRawText(const char *zInput,sxu32 nLen,SySet *pOut,sxu32 nBaseLine)` |
|        5 | 1242 | `{` |
|    14843 | 1243 | `	const char *zEnd = &zInput[nLen];` |
|    14843 | 1244 | `	const char *zIn  = zInput;` |
|        - | 1245 | `	const char *zCur,*zCurEnd;` |
|    14843 | 1246 | `	SyString sCtag = { 0, 0 };     /* Closing tag */` |
|        - | 1247 | `	SyToken sToken;` |
|        - | 1248 | `	SyString sDoc;` |
|        - | 1249 | `	sxu32 nLine;` |
|        - | 1250 | `	sxi32 iNest;` |
|        - | 1251 | `	sxi32 rc;` |
|        - | 1252 | `	/* Tokenize the input into PHP tokens and raw tokens. nBaseLine is normally 1,` |
|        - | 1253 | `	 * but 2 when a "#!" shebang line was stripped so error lines still match php. */` |
|    14843 | 1254 | `	nLine = nBaseLine;` |
|    14843 | 1255 | `	zCur = zCurEnd   = 0; /* Prevent compiler warning */` |
|    14843 | 1256 | `	sToken.pUserData = 0;` |
|    14843 | 1257 | `	iNest = 0;` |
|    14843 | 1258 | `	sDoc.nByte = 0;` |
|    14843 | 1259 | `	sDoc.zString = ""; /* cc warning */` |
|    14820 | 1260 | `	for(;;){` |
|    29407 | 1261 | `		if( zIn >= zEnd ){` |
|        - | 1262 | `			/* End of input reached */` |
|    14561 | 1263 | `			break;` |
|        - | 1264 | `		}` |
|    14851 | 1265 | `		sToken.nLine = nLine;` |
|    14851 | 1266 | `		zCur = zIn;` |
|    14851 | 1267 | `		zCurEnd = 0;` |
|    15563 | 1268 | `		while( zIn < zEnd ){` |
|    15281 | 1269 | `			 if( zIn[0] == '<' ){` |
|    14569 | 1270 | `				const char *zTmp = zIn; /* End of raw input marker */` |
|    14569 | 1271 | `				zIn++;` |
|    14569 | 1272 | `				if( zIn < zEnd ){` |
|    14569 | 1273 | `					if( zIn[0] == '?' ){` |
|    14569 | 1274 | `						zIn++;` |
|    14569 | 1275 | `						if( (sxu32)(zEnd - zIn) >= sizeof("php")-1 &&  SyStrnicmp(zIn,"php",sizeof("php")-1) == 0 ){` |
|        - | 1276 | `							/* opening tag: <?php */` |
|    14567 | 1277 | `							zIn += sizeof("php")-1;` |
|     7281 | 1278 | `						}` |
|        - | 1279 | `						/* Look for the closing tag '?>' */` |
|    14569 | 1280 | `						SyStringInitFromBuf(&sCtag,"?>",sizeof("?>")-1);` |
|    14569 | 1281 | `						zCurEnd = zTmp;` |
|    14569 | 1282 | `						break;` |
|        - | 1283 | `					}` |
|      ! 0 | 1284 | `				}` |
|      ! 0 | 1285 | `			}else{` |
|      717 | 1286 | `				if( zIn[0] == '\n' ){` |
|        7 | 1287 | `					nLine++;` |
|        3 | 1288 | `				}` |
|      717 | 1289 | `				zIn++;` |
|        - | 1290 | `			 }` |
|        5 | 1291 | `		} /* While(zIn < zEnd) */` |
|    14851 | 1292 | `		if( zCurEnd == 0 ){` |
|       49 | 1293 | `			zCurEnd = zIn;` |
|       22 | 1294 | `		}` |
|        - | 1295 | `		/* Save the raw token */` |
|    14851 | 1296 | `		SyStringInitFromBuf(&sToken.sData,zCur,zCurEnd - zCur);` |
|    14851 | 1297 | `		sToken.nType = PH7_TOKEN_RAW;` |
|    14851 | 1298 | `		rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|    14851 | 1299 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1300 | `			return rc;` |
|        - | 1301 | `		}` |
|    14851 | 1302 | `		if( zIn >= zEnd ){` |
|       49 | 1303 | `			break;` |
|        - | 1304 | `		}` |
|        - | 1305 | `		/* Ignore leading white space */` |
|    30925 | 1306 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    16123 | 1307 | `			if( zIn[0] == '\n' ){` |
|    15921 | 1308 | `				nLine++;` |
|     7958 | 1309 | `			}` |
|    16123 | 1310 | `			zIn++;` |
|        5 | 1311 | `		}` |
|        - | 1312 | `		/* Delimit the PHP chunk */` |
|    14807 | 1313 | `		sToken.nLine = nLine;` |
|    14807 | 1314 | `		zCur = zIn;` |
|  2574397 | 1315 | `		while( (sxu32)(zEnd - zIn) >= sCtag.nByte ){` |
|        - | 1316 | `			const char *zPtr;` |
|  2567537 | 1317 | `			if( SyMemcmp(zIn,sCtag.zString,sCtag.nByte) == 0 && iNest < 1 ){` |
|     7709 | 1318 | `				break;` |
|        - | 1319 | `			}` |
|        - | 1320 | `			/* Line comment ('#' or '//', but not the '#[' attribute opener): php` |
|        - | 1321 | `			 * ends it at a newline OR at the closing tag, so a '?>' inside a line` |
|        - | 1322 | `			 * comment DOES close the PHP block. Skipping the comment here also` |
|        - | 1323 | `			 * stops the string skip below from treating a quote inside the` |
|        - | 1324 | `			 * comment as a string. Only outside a heredoc body (iNest < 1). */` |
|  2566110 | 1325 | `			if( iNest < 1 &&` |
|  2554942 | 1326 | `				( (zIn[0] == '#' && !(zIn+1 < zEnd && zIn[1] == '[')) \|\|` |
|  2555342 | 1327 | `				  (zIn[0] == '/' && zIn+1 < zEnd && zIn[1] == '/') ) ){` |
|    12559 | 1328 | `				zIn += (zIn[0] == '#') ? 1 : 2;` |
|   553645 | 1329 | `				while( zIn < zEnd && zIn[0] != '\n' ){` |
|   541088 | 1330 | `					if( (sxu32)(zEnd - zIn) >= sCtag.nByte` |
|   541090 | 1331 | `						&& SyMemcmp(zIn,sCtag.zString,sCtag.nByte) == 0 ){` |
|        3 | 1332 | `						break; /* the closing tag terminates the line comment */` |
|        - | 1333 | `					}` |
|   541091 | 1334 | `					zIn++;` |
|        5 | 1335 | `				}` |
|    11277 | 1336 | `				continue;` |
|        - | 1337 | `			}` |
|        - | 1338 | `			/* Block comment: spans everything, including '?>', up to its close. */` |
|  2548323 | 1339 | `			if( iNest < 1 && zIn[0] == '/' && zIn+1 < zEnd && zIn[1] == '*' ){` |
|     1157 | 1340 | `				zIn += 2;` |
|   141609 | 1341 | `				while( (sxu32)(zEnd-zIn) >= sizeof("*/") - 1 ){` |
|   141609 | 1342 | `					if( zIn[0] == '*' && zIn[1] == '/' ){` |
|     1157 | 1343 | `						zIn += 2;` |
|     1157 | 1344 | `						break;` |
|        - | 1345 | `					}` |
|   140457 | 1346 | `					if( zIn[0] == '\n' ){` |
|     1137 | 1347 | `						nLine++;` |
|      566 | 1348 | `					}` |
|   140457 | 1349 | `					zIn++;` |
|        5 | 1350 | `				}` |
|     1157 | 1351 | `				continue;` |
|        - | 1352 | `			}` |
|        - | 1353 | `			/* Skip over a single/double-quoted or backtick string literal so a` |
|        - | 1354 | `			 * '?>' sequence inside it is not mistaken for the closing tag. Only` |
|        - | 1355 | `			 * outside a heredoc body (iNest < 1); heredocs are delimited by the` |
|        - | 1356 | `			 * label-matching logic above. Escapes (\" \' \\ and a line-continuing` |
|        - | 1357 | `			 * backslash-newline) are honoured. Same-quote nesting inside "{$...}"` |
|        - | 1358 | `			 * interpolation is not tracked, but that can only end the skip early` |
|        - | 1359 | `			 * on a string that has no '?>' anyway, which stays a PHP chunk either` |
|        - | 1360 | `			 * way — it never mis-splits code that works today. */` |
|  2547171 | 1361 | ``			if( iNest < 1 && (zIn[0] == '\'' \|\| zIn[0] == '"' \|\| zIn[0] == '`') ){`` |
|    72961 | 1362 | `				int qch = zIn[0];` |
|    72961 | 1363 | `				zIn++;` |
|   551161 | 1364 | `				while( zIn < zEnd ){` |
|   551161 | 1365 | `					if( zIn[0] == '\\' && zIn + 1 < zEnd ){` |
|    24347 | 1366 | `						if( zIn[1] == '\n' ){ nLine++; }` |
|    24347 | 1367 | `						zIn += 2;` |
|    24347 | 1368 | `						continue;` |
|        - | 1369 | `					}` |
|   526819 | 1370 | `					if( zIn[0] == qch ){ zIn++; break; }` |
|   453863 | 1371 | `					if( zIn[0] == '\n' ){ nLine++; }` |
|   453863 | 1372 | `					zIn++;` |
|        5 | 1373 | `				}` |
|    72961 | 1374 | `				continue;` |
|        - | 1375 | `			}` |
|  2474215 | 1376 | `			if( zIn[0] == '\n' ){` |
|    98581 | 1377 | `				nLine++;` |
|    98581 | 1378 | `				if( iNest > 0 ){` |
|      375 | 1379 | `					zIn++;` |
|      869 | 1380 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|      497 | 1381 | `						zIn++;` |
|        3 | 1382 | `					}` |
|      375 | 1383 | `					zPtr = zIn;` |
|     1948 | 1384 | `					while( zIn < zEnd && LEX_LABEL_BYTE(zIn[0]) ){` |
|     1393 | 1385 | `						zIn++;` |
|        5 | 1386 | `					}` |
|      375 | 1387 | `					if( (sxu32)(zIn - zPtr) == sDoc.nByte && SyMemcmp(sDoc.zString,zPtr,sDoc.nByte) == 0 ){` |
|      133 | 1388 | `						iNest = 0;` |
|       64 | 1389 | `					}` |
|      375 | 1390 | `					continue;` |
|        5 | 1391 | `				}` |
|  2424742 | 1392 | `			}else if ( (sxu32)(zEnd - zIn) >= sizeof("<<<") && zIn[0] == '<' && zIn[1] == '<' && zIn[2] == '<' && iNest < 1){` |
|      135 | 1393 | `				zIn += sizeof("<<<")-1;` |
|      147 | 1394 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|       13 | 1395 | `					zIn++;` |
|        1 | 1396 | `				}` |
|      135 | 1397 | `				if( zIn[0] == '"' \|\| zIn[0] == '\'' ){` |
|       56 | 1398 | `					zIn++;` |
|       26 | 1399 | `				}` |
|      135 | 1400 | `				zPtr = zIn;` |
|      696 | 1401 | `				while( zIn < zEnd && LEX_LABEL_BYTE(zIn[0]) ){` |
|      501 | 1402 | `					zIn++;` |
|        5 | 1403 | `				}` |
|      135 | 1404 | `				SyStringInitFromBuf(&sDoc,zPtr,zIn-zPtr);` |
|      135 | 1405 | `				SyStringFullTrim(&sDoc);` |
|      135 | 1406 | `				if( sDoc.nByte > 0 ){` |
|      135 | 1407 | `					iNest++;` |
|       65 | 1408 | `				}` |
|      135 | 1409 | `				continue;` |
|        - | 1410 | `			}` |
|  2473715 | 1411 | `			zIn++;` |
|        - | 1412 |  |
|  2473715 | 1413 | `			if ( zIn >= zEnd )` |
|      ! 0 | 1414 | `				break;` |
|        5 | 1415 | `		}` |
|    14569 | 1416 | `		if( (sxu32)(zEnd - zIn) < sCtag.nByte ){` |
|     6865 | 1417 | `			zIn = zEnd;` |
|     3430 | 1418 | `		}` |
|    14569 | 1419 | `		if( zCur < zIn ){` |
|        - | 1420 | `			/* Save the PHP chunk for later processing */` |
|    10939 | 1421 | `			sToken.nType = PH7_TOKEN_PHP;` |
|    10939 | 1422 | `			SyStringInitFromBuf(&sToken.sData,zCur,zIn-zCur);` |
|    21485 | 1423 | `			SyStringRightTrim(&sToken.sData); /* Trim trailing white spaces */` |
|    10939 | 1424 | `			rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|    10939 | 1425 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1426 | `				return rc;` |
|        - | 1427 | `			}` |
|     5467 | 1428 | `		}` |
|    14569 | 1429 | `		if( zIn < zEnd ){` |
|        - | 1430 | `			/* Jump the trailing closing tag */` |
|     7709 | 1431 | `			zIn += sCtag.nByte;` |
|        - | 1432 | `			/* php's lexer swallows exactly ONE newline immediately after the` |
|        - | 1433 | `			 * closing tag ("?>\n" emits nothing) */` |
|     7709 | 1434 | `			if( zIn < zEnd && zIn[0] == '\r' && zIn + 1 < zEnd && zIn[1] == '\n' ){` |
|      ! 0 | 1435 | `				zIn += 2;` |
|      ! 0 | 1436 | `				nLine++;` |
|     7709 | 1437 | `			}else if( zIn < zEnd && zIn[0] == '\n' ){` |
|       79 | 1438 | `				zIn++;` |
|       79 | 1439 | `				nLine++;` |
|       37 | 1440 | `			}` |
|     3852 | 1441 | `		}` |
|        5 | 1442 | `	} /* For(;;) */` |
|        - | 1443 |  |
|    14605 | 1444 | ` 	return SXRET_OK;` |
|     7305 | 1445 | `}` |
|        - | 1446 |  |
