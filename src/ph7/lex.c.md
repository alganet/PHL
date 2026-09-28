# src/ph7/lex.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 808/861 lines (93.84%)

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
| 22368800 |   38 | `static sxi32 TokenizePHP(SyStream *pStream,SyToken *pToken,void *pUserData,void *pCtxData)` |
|        5 |   39 | `{` |
|        - |   40 | `	SyString *pStr;` |
|        - |   41 | `	sxi32 rc;` |
|        - |   42 | `	/* Ignore leading white spaces */` |
| 35926047 |   43 | `	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){` |
|        - |   44 | `		/* Advance the stream cursor */` |
| 13557247 |   45 | `		if( pStream->zText[0] == '\n' ){` |
|        - |   46 | `			/* Update line counter */` |
|   143809 |   47 | `			pStream->nLine++;` |
|    71902 |   48 | `		}` |
| 13557247 |   49 | `		pStream->zText++;` |
|        5 |   50 | `	}` |
| 22368805 |   51 | `	if( pStream->zText >= pStream->zEnd ){` |
|        - |   52 | `		/* End of input reached */` |
|        3 |   53 | `		return SXERR_EOF;` |
|        - |   54 | `	}` |
|        - |   55 | `	/* Record token starting position and line */` |
| 22368803 |   56 | `	pToken->nLine = pStream->nLine;` |
| 22368803 |   57 | `	pToken->pUserData = 0;` |
| 22368803 |   58 | `	pStr = &pToken->sData;` |
| 22368803 |   59 | `	SyStringInitFromBuf(pStr,pStream->zText,0);` |
| 25706315 |   60 | `	if( LEX_LABEL_START(pStream->zText[0]) ){` |
|        - |   61 | `		const unsigned char *zIn;` |
|        - |   62 | `		sxu32 nKeyword;` |
|        - |   63 | `		/* Isolate the LABEL. php's class is a flat byte set — [a-zA-Z_\x80-\xff] then` |
|        - |   64 | `		 * [a-zA-Z0-9_\x80-\xff]* — so no UTF-8 decoding is involved: a multibyte name` |
|        - |   65 | `		 * is consumed because every one of its bytes is >= 0x80. (This replaces the` |
|        - |   66 | `		 * xPP lead-byte-plus-continuations dance, which required a lead >= 0xc0 and so` |
|        - |   67 | `		 * stopped one byte class short of php's own rule; see LEX_LABEL_START.) */` |
|  6675029 |   68 | `		zIn = &pStream->zText[1];` |
| 39325933 |   69 | `		while( zIn < pStream->zEnd && LEX_LABEL_BYTE(zIn[0]) ){` |
| 29313397 |   70 | `			zIn++;` |
|        5 |   71 | `		}` |
|  6675029 |   72 | `		pStream->zText = zIn;` |
|        - |   73 | `		/* Record token length */` |
|  6675029 |   74 | `		pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  6675029 |   75 | `		nKeyword = KeywordCodeCI(pStr->zString,(int)pStr->nByte);` |
|  6675029 |   76 | `		if( nKeyword != PH7_TK_ID ){` |
|  2096101 |   77 | `			if( nKeyword &` |
|        - |   78 | `				(PH7_TKWRD_NEW\|PH7_TKWRD_CLONE\|PH7_TKWRD_AND\|PH7_TKWRD_XOR\|PH7_TKWRD_OR\|PH7_TKWRD_INSTANCEOF) ){` |
|        - |   79 | `					/* Alpha stream operators [i.e: new,clone,and,instanceof,or,xor],save the operator instance for later processing */` |
|   103067 |   80 | `					pToken->pUserData = (void *)PH7_ExprExtractOperator(pStr,0);` |
|        - |   81 | `					/* Mark as an operator */` |
|   103067 |   82 | `					pToken->nType = PH7_TK_ID\|PH7_TK_OP;` |
|    51536 |   83 | `			}else{` |
|        - |   84 | `				/* We are dealing with a keyword [i.e: while,foreach,class...],save the keyword ID */` |
|  1993039 |   85 | `				pToken->nType = PH7_TK_KEYWORD;` |
|  1993039 |   86 | `				pToken->pUserData = SX_INT_TO_PTR(nKeyword);` |
|        - |   87 | `			}` |
|  1048053 |   88 | `		}else{` |
|        - |   89 | `			/* A simple identifier */` |
|  4578933 |   90 | `			pToken->nType = PH7_TK_ID;` |
|        - |   91 | `		}` |
|  3337517 |   92 | `	}else{` |
|        - |   93 | `		sxi32 c;` |
|        - |   94 | `		/* Non-alpha stream */` |
| 15693779 |   95 | `		if( pStream->zText[0] == '#' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '[' ){` |
|      625 |   96 | `			sxu32 nDepth = 1;` |
|        - |   97 | `			/* PHP 8 attribute group '#[ ... ]': skip the whole balanced group as` |
|        - |   98 | `			 * trivia (attributes are not stored yet). Brackets inside string` |
|        - |   99 | `			 * literals and comments must not affect the depth count. An` |
|        - |  100 | `			 * unterminated group is silently consumed up to EOF, consistent` |
|        - |  101 | `			 * with unterminated block comments below.` |
|        - |  102 | `			 */` |
|        - |  103 | `			const unsigned char *zGroupStart;` |
|      625 |  104 | `			pStream->zText += 2;` |
|      625 |  105 | `			zGroupStart = pStream->zText;` |
|    10007 |  106 | `			while( pStream->zText < pStream->zEnd && nDepth > 0 ){` |
|     9387 |  107 | `				sxi32 d = pStream->zText[0];` |
|     9387 |  108 | `				if( d == '[' ){` |
|       29 |  109 | `					nDepth++;` |
|     9373 |  110 | `				}else if( d == ']' ){` |
|      653 |  111 | `					nDepth--;` |
|     9035 |  112 | `				}else if( d == '\'' \|\| d == '"' ){` |
|        - |  113 | `					/* String literal: scan for the matching unescaped quote */` |
|       97 |  114 | `					pStream->zText++;` |
|      485 |  115 | `					while( pStream->zText < pStream->zEnd ){` |
|      485 |  116 | `						if( pStream->zText[0] == '\\' && &pStream->zText[1] < pStream->zEnd ){` |
|       23 |  117 | `							if( pStream->zText[1] == '\n' ){` |
|      ! 0 |  118 | `								pStream->nLine++;` |
|      ! 0 |  119 | `							}` |
|       23 |  120 | `							pStream->zText += 2;` |
|       23 |  121 | `							continue;` |
|        - |  122 | `						}` |
|      463 |  123 | `						if( pStream->zText[0] == d ){` |
|       97 |  124 | `							break;` |
|        - |  125 | `						}` |
|      369 |  126 | `						if( pStream->zText[0] == '\n' ){` |
|      ! 0 |  127 | `							pStream->nLine++;` |
|      ! 0 |  128 | `						}` |
|      369 |  129 | `						pStream->zText++;` |
|        3 |  130 | `					}` |
|       97 |  131 | `					if( pStream->zText >= pStream->zEnd ){` |
|      ! 0 |  132 | `						break; /* Unterminated string literal */` |
|        3 |  133 | `					}` |
|        - |  134 | `					/* Fall through: consume the closing quote below */` |
|     8664 |  135 | `				}else if( d == '#' \|\| (d == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '/') ){` |
|        - |  136 | `					/* Inline comment inside the group */` |
|      ! 0 |  137 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] != '\n' ){` |
|      ! 0 |  138 | `						pStream->zText++;` |
|      ! 0 |  139 | `					}` |
|      ! 0 |  140 | `					continue; /* Let the outer loop count the newline */` |
|     8617 |  141 | `				}else if( d == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '*' ){` |
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
|     8617 |  155 | `				}else if( d == '\n' ){` |
|        7 |  156 | `					pStream->nLine++;` |
|        3 |  157 | `				}` |
|     9387 |  158 | `				pStream->zText++;` |
|        5 |  159 | `			}` |
|      625 |  160 | `			if( pUserData && pStream->pSet ){` |
|        - |  161 | `				/* Record the group's inner span (between #[ and its balanced ])` |
|        - |  162 | `				 * in the trivia sidecar, keyed like doc-comments. */` |
|        - |  163 | `				ph7_trivia sTrivia;` |
|      625 |  164 | `				const unsigned char *zGroupEnd = pStream->zText;` |
|      625 |  165 | `				if( nDepth == 0 && zGroupEnd > zGroupStart ){` |
|      625 |  166 | `					zGroupEnd--; /* Exclude the closing ']' */` |
|      310 |  167 | `				}` |
|      625 |  168 | `				sTrivia.nTokIdx = SySetUsed(pStream->pSet);` |
|      625 |  169 | `				sTrivia.iKind = PH7_TRIVIA_ATTR;` |
|      625 |  170 | `				SyStringInitFromBuf(&sTrivia.sText,(const char *)zGroupStart,(sxu32)(zGroupEnd - zGroupStart));` |
|      625 |  171 | `				sTrivia.nLine = pToken->nLine;` |
|      625 |  172 | `				SySetPut((SySet *)pUserData,(const void *)&sTrivia);` |
|      310 |  173 | `			}` |
|        - |  174 | `			/* Tell the upper-layer to ignore this token */` |
|      625 |  175 | `			return SXERR_CONTINUE;` |
| 15820496 |  176 | `		}else if( pStream->zText[0] == '#' \|\|` |
| 15693148 |  177 | `			( pStream->zText[0] == '/' &&  &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '/') ){` |
|    14907 |  178 | `				pStream->zText++;` |
|        - |  179 | `				/* Inline comments */` |
|   803765 |  180 | `				while( pStream->zText < pStream->zEnd && pStream->zText[0] != '\n' ){` |
|   788863 |  181 | `					pStream->zText++;` |
|        5 |  182 | `				}` |
|        - |  183 | `				/* Tell the upper-layer to ignore this token */` |
|    14907 |  184 | `				return SXERR_CONTINUE;` |
| 15678257 |  185 | `		}else if( pStream->zText[0] == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '*' ){` |
|        - |  186 | `			/* A doc-comment starts with slash-star-star followed by more` |
|        - |  187 | `			 * content (slash-star-star-slash is the empty comment, not a` |
|        - |  188 | `			 * docblock). Its full span, delimiters included, goes to the` |
|        - |  189 | `			 * trivia sidecar when the caller supplied one — keyed by the` |
|        - |  190 | `			 * index the NEXT real token receives — and never enters the` |
|        - |  191 | `			 * token stream. */` |
|   233083 |  192 | `			const unsigned char *zDocStart = pStream->zText;` |
|   233176 |  193 | `			int bDoc = ( &pStream->zText[2] < pStream->zEnd && pStream->zText[2] == '*'` |
|   349710 |  194 | `			 && ( &pStream->zText[3] >= pStream->zEnd \|\| pStream->zText[3] != '/' ) );` |
|   233083 |  195 | `			pStream->zText += 2;` |
|        - |  196 | `			/* Block comment */` |
| 33539147 |  197 | `			while( pStream->zText < pStream->zEnd ){` |
| 33539147 |  198 | `				if( pStream->zText[0] == '*' ){` |
|   335873 |  199 | `					if( &pStream->zText[1] >= pStream->zEnd \|\| pStream->zText[1] == '/'  ){` |
|   116544 |  200 | `						break;` |
|        - |  201 | `					}` |
|    51395 |  202 | `				}` |
| 33306069 |  203 | `				if( pStream->zText[0] == '\n' ){` |
|     5153 |  204 | `					pStream->nLine++;` |
|     2574 |  205 | `				}` |
| 33306069 |  206 | `				pStream->zText++;` |
|        5 |  207 | `			}` |
|   233083 |  208 | `			pStream->zText += 2;` |
|   233083 |  209 | `			if( bDoc && pUserData && pStream->pSet ){` |
|        - |  210 | `				ph7_trivia sTrivia;` |
|      191 |  211 | `				const unsigned char *zDocEnd = pStream->zText;` |
|      191 |  212 | `				if( zDocEnd > pStream->zEnd ){` |
|      ! 0 |  213 | `					zDocEnd = pStream->zEnd; /* Unterminated comment at EOF */` |
|      ! 0 |  214 | `				}` |
|      191 |  215 | `				sTrivia.nTokIdx = SySetUsed(pStream->pSet);` |
|      191 |  216 | `				sTrivia.iKind = PH7_TRIVIA_DOC;` |
|      191 |  217 | `				SyStringInitFromBuf(&sTrivia.sText,(const char *)zDocStart,(sxu32)(zDocEnd - zDocStart));` |
|      191 |  218 | `				sTrivia.nLine = pToken->nLine;` |
|      191 |  219 | `				SySetPut((SySet *)pUserData,(const void *)&sTrivia);` |
|       93 |  220 | `			}` |
|        - |  221 | `			/* Tell the upper-layer to ignore this token */` |
|   233083 |  222 | `			return SXERR_CONTINUE;` |
| 15445179 |  223 | `		}else if( SyisDigit(pStream->zText[0]) ){` |
|   707625 |  224 | `			pStream->zText++;` |
|        - |  225 | `			/* PHP 7.4: handle underscore separator immediately following the first digit.` |
|        - |  226 | `			 * Check pStream->zText < pStream->zEnd BEFORE forming pStream->zText + 1 so` |
|        - |  227 | `			 * we never compute a pointer past one-past-end. */` |
|   707620 |  228 | `			if( pStream->zText < pStream->zEnd` |
|   707620 |  229 | `				&& pStream->zText[0] == '_'` |
|   353892 |  230 | `				&& pStream->zText + 1 < pStream->zEnd` |
|      164 |  231 | `				&& pStream->zText[1] < 0xc0` |
|      169 |  232 | `				&& SyisDigit(pStream->zText[1]) ){` |
|      156 |  233 | `				pStream->zText++; /* swallow underscore between two digits */` |
|       77 |  234 | `			}` |
|        - |  235 | `			/* Decimal digit stream (PHP 7.4: underscore separator allowed between two digits) */` |
|   978959 |  236 | `			while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|   271339 |  237 | `				pStream->zText++;` |
|   271334 |  238 | `				if( pStream->zText < pStream->zEnd` |
|   271334 |  239 | `					&& pStream->zText[0] == '_'` |
|   135753 |  240 | `					&& pStream->zText + 1 < pStream->zEnd` |
|      172 |  241 | `					&& pStream->zText[1] < 0xc0` |
|      177 |  242 | `					&& SyisDigit(pStream->zText[1]) ){` |
|      173 |  243 | `					pStream->zText++; /* swallow underscore between two digits */` |
|       86 |  244 | `				}` |
|        5 |  245 | `			}` |
|        - |  246 | `			/* Mark the token as integer until we encounter a real number */` |
|   707625 |  247 | `			pToken->nType = PH7_TK_INTEGER;` |
|   707625 |  248 | `			if( pStream->zText < pStream->zEnd ){` |
|   707625 |  249 | `				c = pStream->zText[0];` |
|   707625 |  250 | `				if( c == '.' ){` |
|        - |  251 | `					/* Real number (PHP 7.4: underscore separator allowed between two digits) */` |
|    13411 |  252 | `					pStream->zText++;` |
|    28797 |  253 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|    15391 |  254 | `						pStream->zText++;` |
|    15386 |  255 | `						if( pStream->zText < pStream->zEnd` |
|    15386 |  256 | `							&& pStream->zText[0] == '_'` |
|     7699 |  257 | `							&& pStream->zText + 1 < pStream->zEnd` |
|       12 |  258 | `							&& pStream->zText[1] < 0xc0` |
|       17 |  259 | `							&& SyisDigit(pStream->zText[1]) ){` |
|       13 |  260 | `							pStream->zText++;` |
|        6 |  261 | `						}` |
|        5 |  262 | `					}` |
|    13411 |  263 | `					if( pStream->zText < pStream->zEnd ){` |
|    13411 |  264 | `						c = pStream->zText[0];` |
|    13411 |  265 | `						if( c=='e' \|\| c=='E' ){` |
|      149 |  266 | `							pStream->zText++;` |
|      149 |  267 | `							if( pStream->zText < pStream->zEnd ){` |
|      149 |  268 | `								c = pStream->zText[0];` |
|      146 |  269 | `								if( (c =='+' \|\| c=='-') && &pStream->zText[1] < pStream->zEnd  &&` |
|      119 |  270 | `									pStream->zText[1] < 0xc0 && SyisDigit(pStream->zText[1]) ){` |
|      119 |  271 | `										pStream->zText++;` |
|       58 |  272 | `								}` |
|      429 |  273 | `								while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|      283 |  274 | `									pStream->zText++;` |
|      280 |  275 | `									if( pStream->zText < pStream->zEnd` |
|      280 |  276 | `										&& pStream->zText[0] == '_'` |
|      144 |  277 | `										&& pStream->zText + 1 < pStream->zEnd` |
|        8 |  278 | `										&& pStream->zText[1] < 0xc0` |
|       11 |  279 | `										&& SyisDigit(pStream->zText[1]) ){` |
|        9 |  280 | `										pStream->zText++;` |
|        4 |  281 | `									}` |
|        3 |  282 | `								}` |
|       73 |  283 | `							}` |
|       73 |  284 | `						}` |
|     6703 |  285 | `					}` |
|    13411 |  286 | `					pToken->nType = PH7_TK_REAL;` |
|   700922 |  287 | `				}else if( c=='e' \|\| c=='E' ){` |
|       90 |  288 | `					SXUNUSED(pUserData); /* Prevent compiler warning */` |
|       90 |  289 | `					SXUNUSED(pCtxData);` |
|      184 |  290 | `					pStream->zText++;` |
|      184 |  291 | `					if( pStream->zText < pStream->zEnd ){` |
|      184 |  292 | `						c = pStream->zText[0];` |
|      180 |  293 | `						if( (c =='+' \|\| c=='-') && &pStream->zText[1] < pStream->zEnd  &&` |
|       44 |  294 | `							pStream->zText[1] < 0xc0 && SyisDigit(pStream->zText[1]) ){` |
|       42 |  295 | `								pStream->zText++;` |
|       20 |  296 | `						}` |
|      554 |  297 | `						while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|      374 |  298 | `							pStream->zText++;` |
|      370 |  299 | `							if( pStream->zText < pStream->zEnd` |
|      370 |  300 | `								&& pStream->zText[0] == '_'` |
|      187 |  301 | `								&& pStream->zText + 1 < pStream->zEnd` |
|        4 |  302 | `								&& pStream->zText[1] < 0xc0` |
|        8 |  303 | `								&& SyisDigit(pStream->zText[1]) ){` |
|        5 |  304 | `								pStream->zText++;` |
|        2 |  305 | `							}` |
|        4 |  306 | `						}` |
|       90 |  307 | `					}` |
|      184 |  308 | `					pToken->nType = PH7_TK_REAL;` |
|        - |  309 | `				/* php only reads a base prefix when the literal so far is exactly "0"` |
|        - |  310 | `				 * AND at least one valid digit follows it. Otherwise the '0' stands` |
|        - |  311 | `				 * alone as an integer and the letter begins an IDENTIFIER, which is` |
|        - |  312 | ``				 * why php reports `0xG` as `unexpected identifier "xG"` while PHL,`` |
|        - |  313 | `				 * consuming the prefix unconditionally, reported just "G". The same` |
|        - |  314 | ``				 * gap silently ACCEPTED `0x`/`0b`/`0o` as int(0), and read `1x5` as`` |
|        - |  315 | `				 * a hex literal, both of which php rejects outright. */` |
|   694128 |  316 | `				}else if( (c == 'x' \|\| c == 'X')` |
|   347112 |  317 | `					&& pStream->zText == &((const unsigned char *)pStr->zString)[1] && pStr->zString[0] == 0x30` |
|      190 |  318 | `					&& &pStream->zText[1] < pStream->zEnd` |
|      195 |  319 | `					&& pStream->zText[1] < 0xc0 && SyisHex(pStream->zText[1]) ){` |
|        - |  320 | `					/* Hex digit stream (PHP 7.4: underscore separator allowed between two digits) */` |
|      190 |  321 | `					pStream->zText++;` |
|     1116 |  322 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisHex(pStream->zText[0]) ){` |
|      930 |  323 | `						pStream->zText++;` |
|      926 |  324 | `						if( pStream->zText < pStream->zEnd` |
|      926 |  325 | `							&& pStream->zText[0] == '_'` |
|      488 |  326 | `							&& pStream->zText + 1 < pStream->zEnd` |
|       50 |  327 | `							&& pStream->zText[1] < 0xc0` |
|       54 |  328 | `							&& SyisHex(pStream->zText[1]) ){` |
|       51 |  329 | `							pStream->zText++;` |
|       25 |  330 | `						}` |
|        4 |  331 | `					}` |
|   693945 |  332 | `				}else if( (c == 'b' \|\| c == 'B')` |
|   347068 |  333 | `					&& pStream->zText == &((const unsigned char *)pStr->zString)[1] && pStr->zString[0] == 0x30` |
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
|   693706 |  347 | `				}else if( (c == 'o' \|\| c == 'O')` |
|   346791 |  348 | `					&& pStream->zText == &((const unsigned char *)pStr->zString)[1] && pStr->zString[0] == 0x30` |
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
|   353810 |  363 | `			}` |
|        - |  364 | `			/* PHP 7.4: absorb a trailing malformed underscore run into the` |
|        - |  365 | `			 * numeric token so the compile phase can emit a PHP-compatible` |
|        - |  366 | `			 * "syntax error, unexpected identifier" parse error. Valid` |
|        - |  367 | `			 * separators were already consumed by the per-loop peek logic` |
|        - |  368 | `			 * above, so an underscore here is always misplaced. */` |
|   707625 |  369 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '_' ){` |
|       14 |  370 | `				pStream->zText++;` |
|       28 |  371 | `				while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0` |
|       31 |  372 | `					&& (SyisAlphaNum(pStream->zText[0]) \|\| pStream->zText[0] == '_') ){` |
|       10 |  373 | `					pStream->zText++;` |
|        2 |  374 | `				}` |
|        5 |  375 | `			}` |
|        - |  376 | `			/* Record token length */` |
|   707625 |  377 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|   707625 |  378 | `			return SXRET_OK;` |
|        - |  379 | `		}` |
| 14737559 |  380 | `		c = pStream->zText[0];` |
| 14737559 |  381 | `		pStream->zText++; /* Advance the stream cursor */` |
|        - |  382 | `		/* Assume we are dealing with an operator*/` |
| 14737559 |  383 | `		pToken->nType = PH7_TK_OP;` |
| 14737559 |  384 | `		switch(c){` |
|  3414633 |  385 | `		case '$': pToken->nType = PH7_TK_DOLLAR; break;` |
|   813443 |  386 | `		case '{': pToken->nType = PH7_TK_OCB;    break;` |
|   813429 |  387 | `		case '}': pToken->nType = PH7_TK_CCB;    break;` |
|  2024669 |  388 | `		case '(': pToken->nType = PH7_TK_LPAREN; break;` |
|   264459 |  389 | `		case '[': pToken->nType \|= PH7_TK_OSB;   break; /* Bitwise operation here,since the square bracket token '['` |
|        - |  390 | `														 * is a potential operator [i.e: subscripting] */` |
|   264465 |  391 | `		case ']': pToken->nType = PH7_TK_CSB;    break;` |
|  1012321 |  392 | `		case ')': {` |
|  2024647 |  393 | `			SySet *pTokSet = pStream->pSet;` |
|        - |  394 | `			/* Assemble type cast operators [i.e: (int),(float),(bool)...] */` |
|  2024647 |  395 | `			if( pTokSet->nUsed >= 2 ){` |
|        - |  396 | `				SyToken *pTmp;` |
|        - |  397 | `				/* Peek the last recongnized token */` |
|  2024645 |  398 | `				pTmp = (SyToken *)SySetPeek(pTokSet);` |
|  2024645 |  399 | `				if( pTmp->nType & PH7_TK_KEYWORD ){` |
|   191543 |  400 | `					sxi32 nID = SX_PTR_TO_INT(pTmp->pUserData);` |
|   191543 |  401 | `					if( (sxu32)nID & (PH7_TKWRD_ARRAY\|PH7_TKWRD_INT\|PH7_TKWRD_FLOAT\|PH7_TKWRD_STRING\|PH7_TKWRD_OBJECT\|PH7_TKWRD_BOOL\|PH7_TKWRD_UNSET) ){` |
|   190843 |  402 | `						pTmp = (SyToken *)SySetAt(pTokSet,pTokSet->nUsed - 2);` |
|   190843 |  403 | `						if( pTmp->nType & PH7_TK_LPAREN ){` |
|        - |  404 | `							/* Merge the three tokens '(' 'TYPE' ')' into a single one */` |
|   133351 |  405 | `							const char * zTypeCast = "(int)";` |
|   133351 |  406 | `							if( nID & PH7_TKWRD_FLOAT ){` |
|    23005 |  407 | `								zTypeCast = "(float)";` |
|   121851 |  408 | `							}else if( nID & PH7_TKWRD_BOOL ){` |
|       81 |  409 | `								zTypeCast = "(bool)";` |
|   110313 |  410 | `							}else if( nID & PH7_TKWRD_STRING ){` |
|    69429 |  411 | `								zTypeCast = "(string)";` |
|    75563 |  412 | `							}else if( nID & PH7_TKWRD_ARRAY ){` |
|      221 |  413 | `								zTypeCast = "(array)";` |
|    40743 |  414 | `							}else if( nID & PH7_TKWRD_OBJECT ){` |
|       57 |  415 | `								zTypeCast = "(object)";` |
|    40608 |  416 | `							}else if( nID & PH7_TKWRD_UNSET ){` |
|        3 |  417 | `								zTypeCast = "(unset)";` |
|        1 |  418 | `							}` |
|        - |  419 | `							/* Reflect the change */` |
|   133351 |  420 | `							pToken->nType = PH7_TK_OP;` |
|   133351 |  421 | `							SyStringInitFromBuf(&pToken->sData,zTypeCast,SyStrlen(zTypeCast));` |
|        - |  422 | `							/* Save the instance associated with the type cast operator */` |
|   133351 |  423 | `							pToken->pUserData = (void *)PH7_ExprExtractOperator(&pToken->sData,0);` |
|        - |  424 | `							/* Remove the two previous tokens */` |
|   133351 |  425 | `							pTokSet->nUsed -= 2;` |
|   133351 |  426 | `							return SXRET_OK;` |
|        - |  427 | `						}` |
|    28746 |  428 | `					}` |
|    29096 |  429 | `				}` |
|   945647 |  430 | `			}` |
|        - |  431 | ``			/* ...and php 8.5's `(void)`, which is a cast TOKEN there too. `void` is`` |
|        - |  432 | ``			 * not a reserved word (`const void = 5;` is legal in both engines), so`` |
|        - |  433 | `			 * the merge keys on the identifier's text -- and it happens wherever` |
|        - |  434 | ``			 * the three tokens meet, which is php's own behaviour: `foo(void)` is`` |
|        - |  435 | ``			 * `foo` followed by a stray cast, not a call passing a constant. */`` |
|  1891301 |  436 | `			if( pTokSet->nUsed >= 2 ){` |
|  1891299 |  437 | `				SyToken *pTmp = (SyToken *)SySetPeek(pTokSet);` |
|  1891294 |  438 | `				if( (pTmp->nType & PH7_TK_ID)` |
|  1371336 |  439 | `				 && pTmp->sData.nByte == sizeof("void")-1` |
|   479015 |  440 | `				 && SyStrnicmp(pTmp->sData.zString,"void",sizeof("void")-1) == 0 ){` |
|       33 |  441 | `					pTmp = (SyToken *)SySetAt(pTokSet,pTokSet->nUsed - 2);` |
|       33 |  442 | `					if( pTmp->nType & PH7_TK_LPAREN ){` |
|       33 |  443 | `						pToken->nType = PH7_TK_VOID_CAST;` |
|       33 |  444 | `						SyStringInitFromBuf(&pToken->sData,"(void)",sizeof("(void)")-1);` |
|       33 |  445 | `						pToken->pUserData = 0;` |
|       33 |  446 | `						pTokSet->nUsed -= 2;` |
|       33 |  447 | `						return SXRET_OK;` |
|        - |  448 | `					}` |
|      ! 0 |  449 | `				}` |
|   945631 |  450 | `			}` |
|  1891269 |  451 | `			pToken->nType = PH7_TK_RPAREN;` |
|  1891269 |  452 | `			break;` |
|        - |  453 | `				  }` |
|   251028 |  454 | `		case '\'':{` |
|        - |  455 | `			/* Single quoted string */` |
|   502061 |  456 | `			pStr->zString++;` |
|  7734743 |  457 | `			while( pStream->zText < pStream->zEnd ){` |
|  7734743 |  458 | `				if( pStream->zText[0] == '\''  ){` |
|   502097 |  459 | `					if( pStream->zText[-1] != '\\' ){` |
|   501903 |  460 | `						break;` |
|      ! 0 |  461 | `					}else{` |
|      199 |  462 | `						const unsigned char *zPtr = &pStream->zText[-2];` |
|      199 |  463 | `						sxi32 i = 1;` |
|      377 |  464 | `						while( zPtr > pStream->zInput && zPtr[0] == '\\' ){` |
|      183 |  465 | `							zPtr--;` |
|      183 |  466 | `							i++;` |
|        5 |  467 | `						}` |
|      199 |  468 | `						if((i&1)==0){` |
|      163 |  469 | `							break;` |
|        - |  470 | `						}` |
|        - |  471 | `					}` |
|       18 |  472 | `				}` |
|  7232687 |  473 | `				if( pStream->zText[0] == '\n' ){` |
|      115 |  474 | `					pStream->nLine++;` |
|       57 |  475 | `				}` |
|  7232687 |  476 | `				pStream->zText++;` |
|        5 |  477 | `			}` |
|        - |  478 | `			/* Record token length and type */` |
|   502061 |  479 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|   502061 |  480 | `			pToken->nType = PH7_TK_SSTR;` |
|        - |  481 | `			/* Jump the trailing single quote */` |
|   502061 |  482 | `			pStream->zText++;` |
|   502061 |  483 | `			return SXRET_OK;` |
|        - |  484 | `				  }` |
|    31412 |  485 | `		case '"':{` |
|        - |  486 | `			sxi32 iNest;` |
|        - |  487 | `			/* Double quoted string */` |
|    62829 |  488 | `			pStr->zString++;` |
|   514733 |  489 | `			while( pStream->zText < pStream->zEnd ){` |
|   514733 |  490 | `				if( pStream->zText[0] == '{' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '$'){` |
|      214 |  491 | `					iNest = 1;` |
|      214 |  492 | `					pStream->zText++;` |
|        - |  493 | `					/* TICKET 1433-40: Hnadle braces'{}' in double quoted string where everything is allowed */` |
|     1976 |  494 | `					while(pStream->zText < pStream->zEnd ){` |
|     1976 |  495 | `						if( pStream->zText[0] == '{' ){` |
|        3 |  496 | `							iNest++;` |
|     1975 |  497 | `						}else if (pStream->zText[0] == '}' ){` |
|      216 |  498 | `							iNest--;` |
|      216 |  499 | `							if( iNest <= 0 ){` |
|      214 |  500 | `								pStream->zText++;` |
|      214 |  501 | `								break;` |
|        1 |  502 | `							}` |
|     1763 |  503 | `						}else if( pStream->zText[0] == '\n' ){` |
|      ! 0 |  504 | `							pStream->nLine++;` |
|      ! 0 |  505 | `						}` |
|     1766 |  506 | `						pStream->zText++;` |
|        4 |  507 | `					}` |
|      214 |  508 | `					if( pStream->zText >= pStream->zEnd ){` |
|      ! 0 |  509 | `						break;` |
|        - |  510 | `					}` |
|      105 |  511 | `				}` |
|   514733 |  512 | `				if( pStream->zText[0] == '"' ){` |
|    63369 |  513 | `					if( pStream->zText[-1] != '\\' ){` |
|    62795 |  514 | `						break;` |
|      ! 0 |  515 | `					}else{` |
|      579 |  516 | `						const unsigned char *zPtr = &pStream->zText[-2];` |
|      579 |  517 | `						sxi32 i = 1;` |
|      665 |  518 | `						while( zPtr > pStream->zInput && zPtr[0] == '\\' ){` |
|       89 |  519 | `							zPtr--;` |
|       89 |  520 | `							i++;` |
|        3 |  521 | `						}` |
|      579 |  522 | `						if((i&1)==0){` |
|       35 |  523 | `							break;` |
|        - |  524 | `						}` |
|        - |  525 | `					}` |
|      270 |  526 | `				}` |
|   451909 |  527 | `				if( pStream->zText[0] == '\n' ){` |
|       49 |  528 | `					pStream->nLine++;` |
|       24 |  529 | `				}` |
|   451909 |  530 | `				pStream->zText++;` |
|        5 |  531 | `			}` |
|        - |  532 | `			/* Record token length and type */` |
|    62829 |  533 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|    62829 |  534 | `			pToken->nType = PH7_TK_DSTR;` |
|        - |  535 | `			/* Jump the trailing quote */` |
|    62829 |  536 | `			pStream->zText++;` |
|    62829 |  537 | `			return SXRET_OK;` |
|        - |  538 | `				  }` |
|        1 |  539 | ``		case '`':{`` |
|        - |  540 | `			/* Backtick quoted string */` |
|        3 |  541 | `			pStr->zString++;` |
|       21 |  542 | `			while( pStream->zText < pStream->zEnd ){` |
|       21 |  543 | ``				if( pStream->zText[0] == '`' && pStream->zText[-1] != '\\' ){`` |
|        3 |  544 | `					break;` |
|        - |  545 | `				}` |
|       19 |  546 | `				if( pStream->zText[0] == '\n' ){` |
|      ! 0 |  547 | `					pStream->nLine++;` |
|      ! 0 |  548 | `				}` |
|       19 |  549 | `				pStream->zText++;` |
|        1 |  550 | `			}` |
|        - |  551 | `			/* Record token length and type */` |
|        3 |  552 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|        3 |  553 | `			pToken->nType = PH7_TK_BSTR;` |
|        - |  554 | `			/* Jump the trailing backtick */` |
|        3 |  555 | `			pStream->zText++;` |
|        3 |  556 | `			return SXRET_OK;` |
|        - |  557 | `				  }` |
|     2291 |  558 | `		case '\\': pToken->nType = PH7_TK_NSSEP;  break;` |
|    28321 |  559 | `		case ':':` |
|    56647 |  560 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == ':' ){` |
|        - |  561 | `				/* Current operator: '::' */` |
|     3657 |  562 | `				pStream->zText++;` |
|     1831 |  563 | `			}else{` |
|    52995 |  564 | `				pToken->nType = PH7_TK_COLON; /* Single colon */` |
|        - |  565 | `			}` |
|    56647 |  566 | `			break;` |
|   663841 |  567 | `		case ',': pToken->nType \|= PH7_TK_COMMA;  break; /* Comma is also an operator */` |
|  1417751 |  568 | `		case ';': pToken->nType = PH7_TK_SEMI;    break;` |
|        - |  569 | `			/* Handle combined operators [i.e: +=,===,!=== ...] */` |
|   592714 |  570 | `		case '=':` |
|  1185433 |  571 | `			pToken->nType \|= PH7_TK_EQUAL;` |
|  1185433 |  572 | `			if( pStream->zText < pStream->zEnd ){` |
|  1185433 |  573 | `				if( pStream->zText[0] == '=' ){` |
|   244921 |  574 | `					pToken->nType &= ~PH7_TK_EQUAL;` |
|        - |  575 | `					/* Current operator: == */` |
|   244921 |  576 | `					pStream->zText++;` |
|   244921 |  577 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  578 | `						/* Current operator: === */` |
|   186963 |  579 | `						pStream->zText++;` |
|    93484 |  580 | `					}` |
|  1062975 |  581 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  582 | `					/* Array operator: => */` |
|    43789 |  583 | `					pToken->nType = PH7_TK_ARRAY_OP;` |
|    43789 |  584 | `					pStream->zText++;` |
|    21897 |  585 | `				}else{` |
|        - |  586 | `					/* TICKET 1433-0010: Reference operator '=&' */` |
|   896733 |  587 | `					const unsigned char *zCur = pStream->zText;` |
|   896733 |  588 | `					sxu32 nLine = 0;` |
|  1793085 |  589 | `					while( zCur < pStream->zEnd && zCur[0] < 0xc0 && SyisSpace(zCur[0]) ){` |
|   896357 |  590 | `						if( zCur[0] == '\n' ){` |
|       10 |  591 | `							nLine++;` |
|        4 |  592 | `						}` |
|   896357 |  593 | `						zCur++;` |
|        5 |  594 | `					}` |
|   896733 |  595 | `					if( zCur < pStream->zEnd && zCur[0] == '&' ){` |
|        - |  596 | `						/* Current operator: =& */` |
|      269 |  597 | `						pToken->nType &= ~PH7_TK_EQUAL;` |
|      269 |  598 | `						SyStringInitFromBuf(pStr,"=&",sizeof("=&")-1);` |
|        - |  599 | `						/* Update token stream */` |
|      269 |  600 | `						pStream->zText = &zCur[1];` |
|      269 |  601 | `						pStream->nLine += nLine;` |
|      132 |  602 | `					}` |
|        - |  603 | `				}` |
|   592714 |  604 | `			}` |
|  1185433 |  605 | `			break;` |
|    95387 |  606 | `		case '!':` |
|   190779 |  607 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  608 | `				/* Current operator: != */` |
|   109679 |  609 | `				pStream->zText++;` |
|   109679 |  610 | `				if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  611 | `					/* Current operator: !== */` |
|    86683 |  612 | `					pStream->zText++;` |
|    43339 |  613 | `				}` |
|    54837 |  614 | `			}` |
|   190779 |  615 | `			break;` |
|   104578 |  616 | `		case '&':` |
|   209161 |  617 | `			pToken->nType \|= PH7_TK_AMPER;` |
|   209161 |  618 | `			if( pStream->zText < pStream->zEnd ){` |
|   209161 |  619 | `				if( pStream->zText[0] == '&' ){` |
|    98245 |  620 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|        - |  621 | `					/* Current operator: && */` |
|    98245 |  622 | `					pStream->zText++;` |
|   160041 |  623 | `				}else if( pStream->zText[0] == '=' ){` |
|      407 |  624 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|        - |  625 | `					/* Current operator: &= */` |
|      407 |  626 | `					pStream->zText++;` |
|      203 |  627 | `				}` |
|   104578 |  628 | `			}` |
|   209161 |  629 | `			break;` |
|    41072 |  630 | `		case '\|':` |
|    82149 |  631 | `			if( pStream->zText < pStream->zEnd ){` |
|    82149 |  632 | `				if( pStream->zText[0] == '\|' ){` |
|        - |  633 | `					/* Current operator: \|\| */` |
|    69165 |  634 | `					pStream->zText++;` |
|    47569 |  635 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  636 | `					/* Current operator: \|= */` |
|      415 |  637 | `					pStream->zText++;` |
|    12782 |  638 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  639 | `					/* Current operator: \|> (PHP 8.5 pipe) */` |
|       27 |  640 | `					pStream->zText++;` |
|       13 |  641 | `				}` |
|    41072 |  642 | `			}` |
|    82149 |  643 | `			break;` |
|    47438 |  644 | `		case '+':` |
|    94881 |  645 | `			if( pStream->zText < pStream->zEnd ){` |
|    94881 |  646 | `				if( pStream->zText[0] == '+' ){` |
|        - |  647 | `					/* Current operator: ++ */` |
|    29593 |  648 | `					pStream->zText++;` |
|    80087 |  649 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  650 | `					/* Current operator: += */` |
|     6337 |  651 | `					pStream->zText++;` |
|     3166 |  652 | `				}` |
|    47438 |  653 | `			}` |
|    94881 |  654 | `			break;` |
|    75520 |  655 | `		case '-':` |
|   151045 |  656 | `			if( pStream->zText < pStream->zEnd ){` |
|   151045 |  657 | `				if( pStream->zText[0] == '-' ){` |
|        - |  658 | `					/* Current operator: -- */` |
|    23041 |  659 | `					pStream->zText++;` |
|   139527 |  660 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  661 | `					/* Current operator: -= */` |
|      412 |  662 | `					pStream->zText++;` |
|   127804 |  663 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  664 | `					/* Current operator: -> */` |
|    43489 |  665 | `					pStream->zText++;` |
|    21742 |  666 | `				}` |
|    75520 |  667 | `			}` |
|   151045 |  668 | `			break;` |
|     3976 |  669 | `		case '*':` |
|     7957 |  670 | `			if( pStream->zText < pStream->zEnd ){` |
|     7957 |  671 | `				if( pStream->zText[0] == '*' ){` |
|        - |  672 | `					/* Current operator: ** or **= */` |
|     1034 |  673 | `					pStream->zText++;` |
|     1034 |  674 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  675 | `						/* Current operator: **= */` |
|      431 |  676 | `						pStream->zText++;` |
|      217 |  677 | `					}` |
|     7441 |  678 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  679 | `					/* Current operator: *= */` |
|      441 |  680 | `					pStream->zText++;` |
|      219 |  681 | `				}` |
|     3976 |  682 | `			}` |
|     7957 |  683 | `			break;` |
|     3350 |  684 | `		case '/':` |
|     6705 |  685 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  686 | `				/* Current operator: /= */` |
|      423 |  687 | `				pStream->zText++;` |
|      211 |  688 | `			}` |
|     6705 |  689 | `			break;` |
|    11990 |  690 | `		case '%':` |
|    23985 |  691 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  692 | `				/* Current operator: %= */` |
|      409 |  693 | `				pStream->zText++;` |
|      204 |  694 | `			}` |
|    23985 |  695 | `			break;` |
|      421 |  696 | `		case '^':` |
|      844 |  697 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  698 | `				/* Current operator: ^= */` |
|      407 |  699 | `				pStream->zText++;` |
|      203 |  700 | `			}` |
|      844 |  701 | `			break;` |
|   101583 |  702 | `		case '.':` |
|   203171 |  703 | `			if( pStream->zText + 1 < pStream->zEnd && pStream->zText[0] == '.' && pStream->zText[1] == '.' ){` |
|        - |  704 | `				/* Ellipsis: ... */` |
|     6625 |  705 | `				pStream->zText += 2;` |
|     6625 |  706 | `				pToken->nType = PH7_TK_ELLIPSIS;` |
|   199861 |  707 | `			}else if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  708 | `				/* Current operator: .= */` |
|    12307 |  709 | `				pStream->zText++;` |
|     6151 |  710 | `			}` |
|   203171 |  711 | `			break;` |
|    41238 |  712 | `		case '<':` |
|    82481 |  713 | `			if( pStream->zText < pStream->zEnd ){` |
|    82481 |  714 | `				if( pStream->zText[0] == '<' ){` |
|        - |  715 | `					/* Current operator: << */` |
|     1034 |  716 | `					pStream->zText++;` |
|     1034 |  717 | `					if( pStream->zText < pStream->zEnd ){` |
|     1034 |  718 | `						if( pStream->zText[0] == '=' ){` |
|        - |  719 | `							/* Current operator: <<= */` |
|      419 |  720 | `							pStream->zText++;` |
|      825 |  721 | `						}else if( pStream->zText[0] == '<' ){` |
|        - |  722 | `							/* Current Token: <<<  */` |
|      138 |  723 | `							pStream->zText++;` |
|        - |  724 | `							/* This may be the beginning of a Heredoc/Nowdoc string,try to delimit it */` |
|      138 |  725 | `							rc = LexExtractHeredoc(&(*pStream),&(*pToken));` |
|      138 |  726 | `							if( rc == SXRET_OK ){` |
|        - |  727 | `								/* Here/Now doc successfuly extracted */` |
|      138 |  728 | `								return SXRET_OK;` |
|        - |  729 | `							}` |
|      ! 0 |  730 | `						}` |
|      450 |  731 | `					}` |
|    81899 |  732 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  733 | `					/* Current operator: <> */` |
|        5 |  734 | `					pStream->zText++;` |
|    81449 |  735 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  736 | `					/* Current operator: <= or <=> */` |
|     6067 |  737 | `					pStream->zText++;` |
|     6067 |  738 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '>' ){` |
|        - |  739 | `						/* Current operator: <=> */` |
|      218 |  740 | `						pStream->zText++;` |
|      107 |  741 | `					}` |
|     3031 |  742 | `				}` |
|    41171 |  743 | `			}` |
|    82347 |  744 | `			break;` |
|    46549 |  745 | `		case '>':` |
|    93103 |  746 | `			if( pStream->zText < pStream->zEnd ){` |
|    93103 |  747 | `				if( pStream->zText[0] == '>' ){` |
|        - |  748 | `					/* Current operator: >> */` |
|    18075 |  749 | `					pStream->zText++;` |
|    18075 |  750 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  751 | `						/* Current operator: >>= */` |
|      411 |  752 | `						pStream->zText++;` |
|      210 |  753 | `					}` |
|    84068 |  754 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  755 | `					/* Current operator: >= */` |
|    17347 |  756 | `					pStream->zText++;` |
|     8671 |  757 | `				}` |
|    46549 |  758 | `			}` |
|    93103 |  759 | `			break;` |
|    22612 |  760 | `		case '?':` |
|    45229 |  761 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '?' ){` |
|        - |  762 | `				/* Null coalescing operator: ?? */` |
|      635 |  763 | `				pStream->zText++;` |
|      635 |  764 | `				if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  765 | `					/* Null coalescing assignment operator (PHP 7.4) */` |
|      193 |  766 | `					pStream->zText++;` |
|       94 |  767 | `				}` |
|    44914 |  768 | `			}else if( (pStream->zEnd - pStream->zText) >= 2` |
|    44599 |  769 | `				&& pStream->zText[0] == '-' && pStream->zText[1] == '>' ){` |
|        - |  770 | `				/* Nullsafe object operator (PHP 8.0): ?-> */` |
|      167 |  771 | `				pStream->zText += 2;` |
|       81 |  772 | `			}` |
|    45224 |  773 | `			break;` |
|    17798 |  774 | `		default:` |
|    35596 |  775 | `			break;` |
|        - |  776 | `		}` |
| 14039165 |  777 | `		if( pStr->nByte <= 0 ){` |
|        - |  778 | `			/* Record token length */` |
| 14038901 |  779 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  7019448 |  780 | `		}` |
| 14039165 |  781 | `		if( pToken->nType & PH7_TK_OP ){` |
|        - |  782 | `			const ph7_expr_op *pOp;` |
|        - |  783 | `			/* Check if the extracted token is an operator */` |
|  3293861 |  784 | `			pOp = PH7_ExprExtractOperator(pStr,(SyToken *)SySetPeek(pStream->pSet));` |
|  3293861 |  785 | `			if( pOp == 0 ){` |
|        - |  786 | `				/* Not an operator */` |
|      ! 0 |  787 | `				pToken->nType &= ~PH7_TK_OP;` |
|      ! 0 |  788 | `				if( pToken->nType <= 0 ){` |
|      ! 0 |  789 | `					pToken->nType = PH7_TK_OTHER;` |
|      ! 0 |  790 | `				}` |
|      ! 0 |  791 | `			}else{` |
|        - |  792 | `				/* Save the instance associated with this operator for later processing */` |
|  3293861 |  793 | `				pToken->pUserData = (void *)pOp;` |
|        - |  794 | `			}` |
|  1646928 |  795 | `		}` |
|        - |  796 | `	}` |
|        - |  797 | `	/* Tell the upper-layer to save the extracted token for later processing */` |
| 20714189 |  798 | `	return SXRET_OK;` |
| 11184405 |  799 | `}` |
|        - |  800 | `/* SPDX-SnippetBegin */` |
|        - |  801 | `/* SPDX-SnippetCopyrightText: SQLite mkkeywordhash.c (D. Richard Hipp and the SQLite authors <https://sqlite.org/>); adapted for the PH7 engine by Chems mrad */` |
|        - |  802 | `/* SPDX-License-Identifier: blessing */` |
|        - |  803 | `/***** This file contains automatically generated code ******` |
|        - |  804 | `**` |
|        - |  805 | `** The code in this file has been automatically generated by` |
|        - |  806 | `**` |
|        - |  807 | `**     $Header: /sqlite/sqlite/tool/mkkeywordhash.c` |
|        - |  808 | `**` |
|        - |  809 | `** Sligthly modified by Chems mrad <chm@symisc.net> for the PH7 engine.` |
|        - |  810 | `**` |
|        - |  811 | `** The code in this file implements a function that determines whether` |
|        - |  812 | `** or not a given identifier is really a PHP keyword.  The same thing` |
|        - |  813 | `** might be implemented more directly using a hand-written hash table.` |
|        - |  814 | `** But by using this automatically generated code, the size of the code` |
|        - |  815 | `** is substantially reduced.  This is important for embedded applications` |
|        - |  816 | `** on platforms with limited memory.` |
|        - |  817 | `*/` |
|        - |  818 | `/* Hash score: 103 */` |
|  5525139 |  819 | `static sxu32 KeywordCode(const char *z, int n){` |
|        - |  820 | `  /* zText[] encodes 532 bytes of keywords in 333 bytes */` |
|        - |  821 | `  /*   extendswitchprintegerequire_oncenddeclareturnamespacechobject      */` |
|        - |  822 | `  /*   hrowbooleandefaultrycaselfinalistaticlonewconstringlobaluse        */` |
|        - |  823 | `  /*   lseifloatvarrayANDIEchoUSECHOabstractclasscontinuendifunction      */` |
|        - |  824 | `  /*   diendwhilevaldoexitgotoimplementsinclude_oncemptyinstanceof        */` |
|        - |  825 | `  /*   interfacendforeachissetparentprivateprotectedpublicatchunset       */` |
|        - |  826 | `  /*   xorARRAYASArrayEXITUNSETXORbreak                                   */` |
|        - |  827 | `  static const char zText[332] = {` |
|        - |  828 | `    'e','x','t','e','n','d','s','w','i','t','c','h','p','r','i','n','t','e',` |
|        - |  829 | `    'g','e','r','e','q','u','i','r','e','_','o','n','c','e','n','d','d','e',` |
|        - |  830 | `    'c','l','a','r','e','t','u','r','n','a','m','e','s','p','a','c','e','c',` |
|        - |  831 | `    'h','o','b','j','e','c','t','h','r','o','w','b','o','o','l','e','a','n',` |
|        - |  832 | `    'd','e','f','a','u','l','t','r','y','c','a','s','e','l','f','i','n','a',` |
|        - |  833 | `    'l','i','s','t','a','t','i','c','l','o','n','e','w','c','o','n','s','t',` |
|        - |  834 | `    'r','i','n','g','l','o','b','a','l','u','s','e','l','s','e','i','f','l',` |
|        - |  835 | `    'o','a','t','v','a','r','r','a','y','A','N','D','I','E','c','h','o','U',` |
|        - |  836 | `    'S','E','C','H','O','a','b','s','t','r','a','c','t','c','l','a','s','s',` |
|        - |  837 | `    'c','o','n','t','i','n','u','e','n','d','i','f','u','n','c','t','i','o',` |
|        - |  838 | `    'n','d','i','e','n','d','w','h','i','l','e','v','a','l','d','o','e','x',` |
|        - |  839 | `    'i','t','g','o','t','o','i','m','p','l','e','m','e','n','t','s','i','n',` |
|        - |  840 | `    'c','l','u','d','e','_','o','n','c','e','m','p','t','y','i','n','s','t',` |
|        - |  841 | `    'a','n','c','e','o','f','i','n','t','e','r','f','a','c','e','n','d','f',` |
|        - |  842 | `    'o','r','e','a','c','h','i','s','s','e','t','p','a','r','e','n','t','p',` |
|        - |  843 | `    'r','i','v','a','t','e','p','r','o','t','e','c','t','e','d','p','u','b',` |
|        - |  844 | `    'l','i','c','a','t','c','h','u','n','s','e','t','x','o','r','A','R','R',` |
|        - |  845 | `    'A','Y','A','S','A','r','r','a','y','E','X','I','T','U','N','S','E','T',` |
|        - |  846 | `    'X','O','R','b','r','e','a','k'` |
|        - |  847 | `  };` |
|        - |  848 | `  static const unsigned char aHash[151] = {` |
|        - |  849 | `       0,   0,   4,  83,   0,  61,  39,  12,   0,  33,  77,   0,  48,` |
|        - |  850 | `       0,   2,  65,  67,   0,   0,   0,  47,   0,   0,  40,   0,  15,` |
|        - |  851 | `      74,   0,  51,   0,  76,   0,   0,  20,   0,   0,   0,  50,   0,` |
|        - |  852 | `      80,  34,   0,  36,   0,   0,  64,  16,   0,   0,  17,   0,   1,` |
|        - |  853 | `      19,  84,  66,   0,  43,  45,  78,   0,   0,  53,  56,   0,   0,` |
|        - |  854 | `       0,  23,  49,   0,   0,  13,  31,  54,   7,   0,   0,  25,   0,` |
|        - |  855 | `      72,  14,   0,  71,   0,  38,   6,   0,   0,   0,  73,   0,   0,` |
|        - |  856 | `       3,   0,  41,   5,  52,  57,  32,   0,  60,  63,   0,  69,  82,` |
|        - |  857 | `      30,   0,  79,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,` |
|        - |  858 | `       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  81,   0,   0,` |
|        - |  859 | `      62,   0,  11,   0,   0,  58,   0,   0,   0,   0,  59,  75,   0,` |
|        - |  860 | `       0,   0,   0,   0,   0,  35,  27,   0` |
|        - |  861 | `  };` |
|        - |  862 | `  static const unsigned char aNext[84] = {` |
|        - |  863 | `       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,` |
|        - |  864 | `       0,   0,   8,   0,   0,   0,  10,   0,   0,   0,   0,   0,   0,` |
|        - |  865 | `       0,   0,   0,   0,  28,   0,   0,   0,   0,   0,   0,   0,   0,` |
|        - |  866 | `       0,   0,   0,   0,   0,  44,   0,  18,   0,   0,   0,   0,   0,` |
|        - |  867 | `       0,  46,   0,  29,   0,   0,   0,  22,   0,   0,   0,   0,  26,` |
|        - |  868 | `       0,  21,  24,   0,   0,  68,   0,   0,   9,  37,   0,   0,   0,` |
|        - |  869 | `      42,   0,   0,   0,  70,  55` |
|        - |  870 | `  };` |
|        - |  871 | `  static const unsigned char aLen[84] = {` |
|        - |  872 | `       7,   9,   6,   5,   7,  12,   7,   2,  10,   7,   6,   9,   4,` |
|        - |  873 | `       6,   5,   7,   4,   3,   7,   3,   4,   4,   5,   4,   6,   5,` |
|        - |  874 | `       2,   3,   5,   6,   6,   3,   6,   4,   2,   5,   3,   5,   3,` |
|        - |  875 | `       3,   4,   3,   4,   8,   5,   2,   8,   5,   8,   3,   8,   5,` |
|        - |  876 | `       4,   2,   4,   4,  10,  12,   7,   5,  10,   9,   3,   6,  10,` |
|        - |  877 | `       3,   7,   2,   5,   6,   7,   9,   6,   5,   5,   3,   5,   2,` |
|        - |  878 | `       5,   4,   5,   3,   2,   5` |
|        - |  879 | `  };` |
|        - |  880 | `  static const sxu16 aOffset[84] = {` |
|        - |  881 | `       0,   3,   6,  12,  14,  20,  20,  21,  31,  34,  39,  44,  52,` |
|        - |  882 | `      55,  60,  65,  65,  70,  72,  78,  81,  83,  86,  90,  92,  97,` |
|        - |  883 | `     100, 100, 103, 106, 111, 117, 119, 119, 123, 124, 129, 130, 135,` |
|        - |  884 | `     137, 139, 143, 145, 149, 157, 159, 162, 169, 173, 181, 183, 186,` |
|        - |  885 | `     190, 194, 196, 200, 204, 214, 214, 225, 230, 240, 240, 248, 248,` |
|        - |  886 | `     251, 251, 252, 258, 263, 269, 276, 285, 290, 295, 300, 303, 308,` |
|        - |  887 | `     310, 315, 319, 324, 325, 327` |
|        - |  888 | `  };` |
|        - |  889 | `  static const sxu32 aCode[84] = {` |
|        - |  890 | `    PH7_TKWRD_EXTENDS,   PH7_TKWRD_ENDSWITCH,   PH7_TKWRD_SWITCH,    PH7_TKWRD_PRINT,   PH7_TKWRD_INT,` |
|        - |  891 | `    PH7_TKWRD_REQONCE,   PH7_TKWRD_REQUIRE,     PH7_TK_ID /* 'eq' PH7-ism removed */, PH7_TKWRD_ENDDEC, PH7_TKWRD_DECLARE,` |
|        - |  892 | `    PH7_TKWRD_RETURN,    PH7_TKWRD_NAMESPACE,   PH7_TKWRD_ECHO,      PH7_TKWRD_OBJECT,    PH7_TKWRD_THROW,` |
|        - |  893 | `    PH7_TKWRD_BOOL,      PH7_TKWRD_BOOL,        PH7_TKWRD_AND,       PH7_TKWRD_DEFAULT,   PH7_TKWRD_TRY,` |
|        - |  894 | `    PH7_TKWRD_CASE,      PH7_TKWRD_SELF,        PH7_TKWRD_FINAL,     PH7_TKWRD_LIST,      PH7_TKWRD_STATIC,` |
|        - |  895 | `    PH7_TKWRD_CLONE,     PH7_TK_ID /* 'ne' PH7-ism removed */, PH7_TKWRD_NEW,  PH7_TKWRD_CONST,     PH7_TKWRD_STRING,` |
|        - |  896 | `    PH7_TKWRD_GLOBAL,    PH7_TKWRD_USE,         PH7_TKWRD_ELIF,      PH7_TKWRD_ELSE,      PH7_TKWRD_IF,` |
|        - |  897 | `    PH7_TKWRD_FLOAT,     PH7_TKWRD_VAR,         PH7_TKWRD_ARRAY,     PH7_TKWRD_AND,       PH7_TKWRD_DIE,` |
|        - |  898 | `    PH7_TKWRD_ECHO,      PH7_TKWRD_USE,         PH7_TKWRD_ECHO,      PH7_TKWRD_ABSTRACT,  PH7_TKWRD_CLASS,` |
|        - |  899 | `    PH7_TKWRD_AS,        PH7_TKWRD_CONTINUE,    PH7_TKWRD_ENDIF,     PH7_TKWRD_FUNCTION,  PH7_TKWRD_DIE,` |
|        - |  900 | `    PH7_TKWRD_ENDWHILE,  PH7_TKWRD_WHILE,       PH7_TKWRD_EVAL,      PH7_TKWRD_DO,        PH7_TKWRD_EXIT,` |
|        - |  901 | `    PH7_TKWRD_GOTO,      PH7_TKWRD_IMPLEMENTS,  PH7_TKWRD_INCONCE,   PH7_TKWRD_INCLUDE,   PH7_TKWRD_EMPTY,` |
|        - |  902 | `    PH7_TKWRD_INSTANCEOF,PH7_TKWRD_INTERFACE,   PH7_TKWRD_INT,       PH7_TKWRD_ENDFOR,    PH7_TKWRD_END4EACH,` |
|        - |  903 | `    PH7_TKWRD_FOR,       PH7_TKWRD_FOREACH,     PH7_TKWRD_OR,        PH7_TKWRD_ISSET,     PH7_TKWRD_PARENT,` |
|        - |  904 | `    PH7_TKWRD_PRIVATE,   PH7_TKWRD_PROTECTED,   PH7_TKWRD_PUBLIC,    PH7_TKWRD_CATCH,     PH7_TKWRD_UNSET,` |
|        - |  905 | `    PH7_TKWRD_XOR,       PH7_TKWRD_ARRAY,       PH7_TKWRD_AS,        PH7_TKWRD_ARRAY,     PH7_TKWRD_EXIT,` |
|        - |  906 | `    PH7_TKWRD_UNSET,     PH7_TKWRD_XOR,         PH7_TKWRD_OR,        PH7_TKWRD_BREAK` |
|        - |  907 | `  };` |
|        - |  908 | `  int h, i;` |
|  5525139 |  909 | `  if( n<2 ) return PH7_TK_ID;` |
|        - |  910 | ``  /* Hash through UNSIGNED bytes: `char` is signed on most targets, so an`` |
|        - |  911 | `   * identifier carrying a high byte (php allows 0x80-0xFF in identifiers, and` |
|        - |  912 | ``   * every UTF-8 name has them) made the xor negative, and C's `%` keeps that`` |
|        - |  913 | `   * sign — aHash[-46] read off the front of the table. ASCII is unaffected, so` |
|        - |  914 | `   * the generated keyword buckets still resolve exactly as before. */` |
|  5525139 |  915 | `  h = (int)(((sxu32)(sxu8)z[0]*4) ^ ((sxu32)(sxu8)z[n-1]*3) ^ (sxu32)n) % 151;` |
|  8146841 |  916 | `  for(i=((int)aHash[h])-1; i>=0; i=((int)aNext[i])-1){` |
|  4709769 |  917 | `    if( (int)aLen[i]==n && SyMemcmp(&zText[aOffset[i]],z,n)==0 ){` |
|        - |  918 | `       /* PH7_TKWRD_EXTENDS */` |
|        - |  919 | `       /* PH7_TKWRD_ENDSWITCH */` |
|        - |  920 | `       /* PH7_TKWRD_SWITCH */` |
|        - |  921 | `       /* PH7_TKWRD_PRINT */` |
|        - |  922 | `       /* PH7_TKWRD_INT */` |
|        - |  923 | `       /* PH7_TKWRD_REQONCE */` |
|        - |  924 | `       /* PH7_TKWRD_REQUIRE */` |
|        - |  925 | `       /* PH7_TK_ID */` |
|        - |  926 | `       /* PH7_TKWRD_ENDDEC */` |
|        - |  927 | `       /* PH7_TKWRD_DECLARE */` |
|        - |  928 | `       /* PH7_TKWRD_RETURN */` |
|        - |  929 | `       /* PH7_TKWRD_NAMESPACE */` |
|        - |  930 | `       /* PH7_TKWRD_ECHO */` |
|        - |  931 | `       /* PH7_TKWRD_OBJECT */` |
|        - |  932 | `       /* PH7_TKWRD_THROW */` |
|        - |  933 | `       /* PH7_TKWRD_BOOL */` |
|        - |  934 | `       /* PH7_TKWRD_BOOL */` |
|        - |  935 | `       /* PH7_TKWRD_AND */` |
|        - |  936 | `       /* PH7_TKWRD_DEFAULT */` |
|        - |  937 | `       /* PH7_TKWRD_TRY */` |
|        - |  938 | `       /* PH7_TKWRD_CASE */` |
|        - |  939 | `       /* PH7_TKWRD_SELF */` |
|        - |  940 | `       /* PH7_TKWRD_FINAL */` |
|        - |  941 | `       /* PH7_TKWRD_LIST */` |
|        - |  942 | `       /* PH7_TKWRD_STATIC */` |
|        - |  943 | `       /* PH7_TKWRD_CLONE */` |
|        - |  944 | `       /* PH7_TK_ID */` |
|        - |  945 | `       /* PH7_TKWRD_NEW */` |
|        - |  946 | `       /* PH7_TKWRD_CONST */` |
|        - |  947 | `       /* PH7_TKWRD_STRING */` |
|        - |  948 | `       /* PH7_TKWRD_GLOBAL */` |
|        - |  949 | `       /* PH7_TKWRD_USE */` |
|        - |  950 | `       /* PH7_TKWRD_ELIF */` |
|        - |  951 | `       /* PH7_TKWRD_ELSE */` |
|        - |  952 | `       /* PH7_TKWRD_IF */` |
|        - |  953 | `       /* PH7_TKWRD_FLOAT */` |
|        - |  954 | `       /* PH7_TKWRD_VAR */` |
|        - |  955 | `       /* PH7_TKWRD_ARRAY */` |
|        - |  956 | `       /* PH7_TKWRD_AND */` |
|        - |  957 | `       /* PH7_TKWRD_DIE */` |
|        - |  958 | `       /* PH7_TKWRD_ECHO */` |
|        - |  959 | `       /* PH7_TKWRD_USE */` |
|        - |  960 | `       /* PH7_TKWRD_ECHO */` |
|        - |  961 | `       /* PH7_TKWRD_ABSTRACT */` |
|        - |  962 | `       /* PH7_TKWRD_CLASS */` |
|        - |  963 | `       /* PH7_TKWRD_AS */` |
|        - |  964 | `       /* PH7_TKWRD_CONTINUE */` |
|        - |  965 | `       /* PH7_TKWRD_ENDIF */` |
|        - |  966 | `       /* PH7_TKWRD_FUNCTION */` |
|        - |  967 | `       /* PH7_TKWRD_DIE */` |
|        - |  968 | `       /* PH7_TKWRD_ENDWHILE */` |
|        - |  969 | `       /* PH7_TKWRD_WHILE */` |
|        - |  970 | `       /* PH7_TKWRD_EVAL */` |
|        - |  971 | `       /* PH7_TKWRD_DO */` |
|        - |  972 | `       /* PH7_TKWRD_EXIT */` |
|        - |  973 | `       /* PH7_TKWRD_GOTO */` |
|        - |  974 | `       /* PH7_TKWRD_IMPLEMENTS */` |
|        - |  975 | `       /* PH7_TKWRD_INCONCE */` |
|        - |  976 | `       /* PH7_TKWRD_INCLUDE */` |
|        - |  977 | `       /* PH7_TKWRD_EMPTY */` |
|        - |  978 | `       /* PH7_TKWRD_INSTANCEOF */` |
|        - |  979 | `       /* PH7_TKWRD_INTERFACE */` |
|        - |  980 | `       /* PH7_TKWRD_INT */` |
|        - |  981 | `       /* PH7_TKWRD_ENDFOR */` |
|        - |  982 | `       /* PH7_TKWRD_END4EACH */` |
|        - |  983 | `       /* PH7_TKWRD_FOR */` |
|        - |  984 | `       /* PH7_TKWRD_FOREACH */` |
|        - |  985 | `       /* PH7_TKWRD_OR */` |
|        - |  986 | `       /* PH7_TKWRD_ISSET */` |
|        - |  987 | `       /* PH7_TKWRD_PARENT */` |
|        - |  988 | `       /* PH7_TKWRD_PRIVATE */` |
|        - |  989 | `       /* PH7_TKWRD_PROTECTED */` |
|        - |  990 | `       /* PH7_TKWRD_PUBLIC */` |
|        - |  991 | `       /* PH7_TKWRD_CATCH */` |
|        - |  992 | `       /* PH7_TKWRD_UNSET */` |
|        - |  993 | `       /* PH7_TKWRD_XOR */` |
|        - |  994 | `       /* PH7_TKWRD_ARRAY */` |
|        - |  995 | `       /* PH7_TKWRD_AS */` |
|        - |  996 | `       /* PH7_TKWRD_ARRAY */` |
|        - |  997 | `       /* PH7_TKWRD_EXIT */` |
|        - |  998 | `       /* PH7_TKWRD_UNSET */` |
|        - |  999 | `       /* PH7_TKWRD_XOR */` |
|        - | 1000 | `       /* PH7_TKWRD_OR */` |
|        - | 1001 | `       /* PH7_TKWRD_BREAK */` |
|  2088067 | 1002 | `      return aCode[i];` |
|        - | 1003 | `    }` |
|  1310856 | 1004 | `  }` |
|        - | 1005 | `  /* Linear fallback for keywords not in the auto-generated hash table */` |
|  3437077 | 1006 | `  if( n==5 && SyMemcmp(z,"trait",5)==0 ) return PH7_TKWRD_TRAIT;` |
|  3436867 | 1007 | `  if( n==9 && SyMemcmp(z,"insteadof",9)==0 ) return PH7_TKWRD_INSTEADOF;` |
|  3436855 | 1008 | `  if( n==7 && SyMemcmp(z,"finally",7)==0 ) return PH7_TKWRD_FINALLY;` |
|  3436551 | 1009 | `  if( n==5 && SyMemcmp(z,"yield",5)==0 ) return PH7_TKWRD_YIELD;` |
|  3436027 | 1010 | `  if( n==5 && SyMemcmp(z,"match",5)==0 ) return PH7_TKWRD_MATCH;` |
|  3435863 | 1011 | `  if( n==2 && SyMemcmp(z,"fn",2)==0 ) return PH7_TKWRD_FN;   /* PHP 7.4 arrow functions */` |
|  3428993 | 1012 | `  return PH7_TK_ID;` |
|  2762572 | 1013 | `}` |
|        - | 1014 | `/* --- End of Automatically generated code --- */` |
|        - | 1015 | `/* SPDX-SnippetEnd */` |
|        - | 1016 | `/*` |
|        - | 1017 | ` * Keyword lookup as php does it: CASE-INSENSITIVELY. 'IF', 'Function' and 'NEW'` |
|        - | 1018 | ` * are the very same tokens as 'if', 'function' and 'new', so the generated table` |
|        - | 1019 | ` * above — whose hash buckets and SyMemcmp() rows are byte-exact lower case — is` |
|        - | 1020 | ` * probed through an ASCII-folded COPY of the identifier. KeywordCode() itself` |
|        - | 1021 | ` * stays byte-exact so the generated code needs no regeneration.` |
|        - | 1022 | ` *` |
|        - | 1023 | ` * The fold is ASCII-only on purpose: libc tolower() follows LC_CTYPE (a tr_TR` |
|        - | 1024 | ` * embedder would stop recognising 'IF') where php's lexer is locale-independent,` |
|        - | 1025 | ` * and identifier bytes >= 0x80 — php allows them, and every UTF-8 name has` |
|        - | 1026 | ` * them — must pass through untouched.` |
|        - | 1027 | ` *` |
|        - | 1028 | ` * A handful of UPPER-case rows in the table ('ARRAY', 'AS', 'EXIT', 'UNSET',` |
|        - | 1029 | ` * 'XOR', 'AND', 'OR', 'ECHO', 'Echo', 'Array', 'USE') are PH7's old partial hack` |
|        - | 1030 | ` * for this same problem. Each has a lower-case twin, so folding makes them` |
|        - | 1031 | ` * unreachable-but-harmless rather than wrong.` |
|        - | 1032 | ` */` |
|  6675024 | 1033 | `static sxu32 KeywordCodeCI(const char *zRaw, int n)` |
|        5 | 1034 | `{` |
|        - | 1035 | `	/* Longest row in the table above: 'require_once'/'include_once' (12 bytes) */` |
|        - | 1036 | `	char zFold[12];` |
|        - | 1037 | `	int i;` |
|  6675029 | 1038 | `	if( n < 2 \|\| n > (int)sizeof(zFold) ){` |
|        - | 1039 | `		/* Too short or too long to be any keyword: skip the fold and the probe */` |
|  1149895 | 1040 | `		return PH7_TK_ID;` |
|        - | 1041 | `	}` |
| 35365857 | 1042 | `	for( i = 0 ; i < n ; ++i ){` |
| 29840723 | 1043 | `		unsigned char c = (unsigned char)zRaw[i];` |
| 29840723 | 1044 | `		zFold[i] = (char)((c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c);` |
| 14920364 | 1045 | `	}` |
|  5525139 | 1046 | `	return KeywordCode(zFold,n);` |
|  3337517 | 1047 | `}` |
|        - | 1048 | `/*` |
|        - | 1049 | ` * Extract a heredoc/nowdoc text from a raw PHP input.` |
|        - | 1050 | ` * According to the PHP language reference manual:` |
|        - | 1051 | ` *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier` |
|        - | 1052 | ` *  is provided, then a newline. The string itself follows, and then the same identifier again` |
|        - | 1053 | ` *  to close the quotation.` |
|        - | 1054 | ` *  The closing identifier must begin in the first column of the line. Also, the identifier must` |
|        - | 1055 | ` *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric` |
|        - | 1056 | ` *  characters and underscores, and must start with a non-digit character or underscore.` |
|        - | 1057 | ` *  Heredoc text behaves just like a double-quoted string, without the double quotes.` |
|        - | 1058 | ` *  This means that quotes in a heredoc do not need to be escaped, but the escape codes listed` |
|        - | 1059 | ` *  above can still be used. Variables are expanded, but the same care must be taken when expressing` |
|        - | 1060 | ` *  complex variables inside a heredoc as with strings.` |
|        - | 1061 | ` *  Nowdocs are to single-quoted strings what heredocs are to double-quoted strings.` |
|        - | 1062 | ` *  A nowdoc is specified similarly to a heredoc, but no parsing is done inside a nowdoc.` |
|        - | 1063 | ` *  The construct is ideal for embedding PHP code or other large blocks of text without the need` |
|        - | 1064 | ` *  for escaping. It shares some features in common with the SGML <![CDATA[ ]]> construct, in that` |
|        - | 1065 | ` *  it declares a block of text which is not for parsing.` |
|        - | 1066 | ` *  A nowdoc is identified with the same <<< sequence used for heredocs, but the identifier which follows` |
|        - | 1067 | ` *  is enclosed in single quotes, e.g. <<<'EOT'. All the rules for heredoc identifiers also apply to nowdoc` |
|        - | 1068 | ` *  identifiers, especially those regarding the appearance of the closing identifier.` |
|        - | 1069 | ` * Symisc Extension:` |
|        - | 1070 | ` * The closing delimiter can now start with a digit or undersocre or it can be an UTF-8 stream.` |
|        - | 1071 | ` * Example:` |
|        - | 1072 | ` *  <<<123` |
|        - | 1073 | ` *    HEREDOC Here` |
|        - | 1074 | ` * 123` |
|        - | 1075 | ` *  or` |
|        - | 1076 | ` *  <<<___` |
|        - | 1077 | ` *   HEREDOC Here` |
|        - | 1078 | ` *  ___` |
|        - | 1079 | ` */` |
|      134 | 1080 | `static sxi32 LexExtractHeredoc(SyStream *pStream,SyToken *pToken)` |
|        4 | 1081 | `{` |
|      138 | 1082 | `	const unsigned char *zIn  = pStream->zText;` |
|      138 | 1083 | `	const unsigned char *zEnd = pStream->zEnd;` |
|        - | 1084 | `	const unsigned char *zPtr;` |
|      138 | 1085 | `	sxu8 bNowDoc = FALSE;` |
|        - | 1086 | `	SyString sDelim;` |
|        - | 1087 | `	SyString sStr;` |
|        - | 1088 | `	/* Jump leading white spaces */` |
|      150 | 1089 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|       13 | 1090 | `		zIn++;` |
|        1 | 1091 | `	}` |
|      138 | 1092 | `	if( zIn >= zEnd ){` |
|        - | 1093 | `		/* A simple symbol,return immediately */` |
|      ! 0 | 1094 | `		return SXERR_CONTINUE;` |
|        - | 1095 | `	}` |
|      138 | 1096 | `	if( zIn[0] == '\'' \|\| zIn[0] == '"' ){` |
|        - | 1097 | `		/* Make sure we are dealing with a nowdoc */` |
|       58 | 1098 | `		bNowDoc =  zIn[0] == '\'' ? TRUE : FALSE;` |
|       58 | 1099 | `		zIn++;` |
|       27 | 1100 | `	}` |
|      138 | 1101 | `	if( !LEX_LABEL_BYTE(zIn[0]) ){` |
|        - | 1102 | `		/* Invalid delimiter,return immediately */` |
|      ! 0 | 1103 | `		return SXERR_CONTINUE;` |
|        - | 1104 | `	}` |
|        - | 1105 | `	/* Isolate the identifier (php's label bytes; see LEX_LABEL_START) */` |
|      138 | 1106 | `	sDelim.zString = (const char *)zIn;` |
|      138 | 1107 | `	zPtr = zIn;` |
|      717 | 1108 | `	while( zPtr < zEnd && LEX_LABEL_BYTE(zPtr[0]) ){` |
|      516 | 1109 | `		zPtr++;` |
|        4 | 1110 | `	}` |
|      138 | 1111 | `	zIn = zPtr;` |
|        - | 1112 | `	/* Get the identifier length */` |
|      138 | 1113 | `	sDelim.nByte = (sxu32)((const char *)zIn-sDelim.zString);` |
|      138 | 1114 | `	if( zIn[0] == '"' \|\| (bNowDoc && zIn[0] == '\'') ){` |
|        - | 1115 | `		/* Jump the trailing single quote */` |
|       58 | 1116 | `		zIn++;` |
|       27 | 1117 | `	}` |
|        - | 1118 | `	/* Jump trailing white spaces */` |
|      138 | 1119 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|      ! 0 | 1120 | `		zIn++;` |
|      ! 0 | 1121 | `	}` |
|      138 | 1122 | `	if( sDelim.nByte <= 0 \|\| zIn >= zEnd \|\| zIn[0] != '\n' ){` |
|        - | 1123 | `		/* Invalid syntax */` |
|      ! 0 | 1124 | `		return SXERR_CONTINUE;` |
|        - | 1125 | `	}` |
|      138 | 1126 | `	pStream->nLine++; /* Increment line counter */` |
|      138 | 1127 | `	zIn++;` |
|        - | 1128 | `	/* Isolate the delimited string */` |
|      138 | 1129 | `	sStr.zString = (const char *)zIn;` |
|        - | 1130 | `	/* PHP 7.3 flexible heredoc/nowdoc: the closing marker may be preceded` |
|        - | 1131 | `	 * by whitespace (spaces/tabs), and may be followed by any non-identifier` |
|        - | 1132 | `	 * character. The indent count is recorded in pToken->pUserData and the` |
|        - | 1133 | `	 * compile phase strips it from each body line. */` |
|        - | 1134 | `	{` |
|      138 | 1135 | `		const unsigned char *zMarkerLine = zIn; /* Start of marker's line (set on match) */` |
|      138 | 1136 | `		sxu32 nIndent = 0;` |
|      319 | 1137 | `		for(;;){` |
|      390 | 1138 | `			const unsigned char *zLineStart = zIn;` |
|        - | 1139 | `			/* Skip leading space/tab on this line */` |
|     1081 | 1140 | `			while( zIn < zEnd && (zIn[0] == ' ' \|\| zIn[0] == '\t') ){` |
|      502 | 1141 | `				zIn++;` |
|        4 | 1142 | `			}` |
|      386 | 1143 | `			if( (sxu32)(zEnd - zIn) >= sDelim.nByte` |
|      389 | 1144 | `				&& SyMemcmp((const void *)sDelim.zString,(const void *)zIn,sDelim.nByte) == 0 ){` |
|        - | 1145 | `				int bIdentCont;` |
|      136 | 1146 | `				zPtr = &zIn[sDelim.nByte];` |
|        - | 1147 | `				/* Disambiguate: the next byte must not continue an identifier` |
|        - | 1148 | `				 * (php's label bytes; see LEX_LABEL_START). */` |
|      202 | 1149 | `				bIdentCont = zPtr < zEnd && LEX_LABEL_BYTE(zPtr[0]);` |
|      136 | 1150 | `				if( !bIdentCont ){` |
|        - | 1151 | `					/* Closing marker found */` |
|      136 | 1152 | `					nIndent = (sxu32)(zIn - zLineStart);` |
|      136 | 1153 | `					zMarkerLine = zLineStart;` |
|      136 | 1154 | `					pStream->zText = zPtr; /* Cursor right after identifier */` |
|      136 | 1155 | `					break;` |
|        - | 1156 | `				}` |
|      ! 0 | 1157 | `			}` |
|        - | 1158 | `			/* Not the closing marker on this line; walk to next newline */` |
|     5842 | 1159 | `			while( zIn < zEnd && zIn[0] != '\n' ){` |
|     5588 | 1160 | `				zIn++;` |
|        4 | 1161 | `			}` |
|      258 | 1162 | `			if( zIn >= zEnd ){` |
|        - | 1163 | `				/* End of input without finding the closing marker */` |
|        3 | 1164 | `				pStream->zText = pStream->zEnd;` |
|        3 | 1165 | `				zMarkerLine = zIn;` |
|        3 | 1166 | `				break;` |
|        - | 1167 | `			}` |
|      256 | 1168 | `			pStream->nLine++;` |
|      256 | 1169 | `			zIn++;` |
|        4 | 1170 | `		}` |
|        - | 1171 | `		/* Body runs from sStr.zString up to just before the marker line */` |
|      138 | 1172 | `		sStr.nByte = (sxu32)((const char *)zMarkerLine - sStr.zString);` |
|      138 | 1173 | `		pToken->nType = bNowDoc ? PH7_TK_NOWDOC : PH7_TK_HEREDOC;` |
|      138 | 1174 | `		SyStringDupPtr(&pToken->sData,&sStr);` |
|        - | 1175 | `		/* Strip exactly one line terminator that precedes the marker's line. */` |
|      134 | 1176 | `		if( pToken->sData.nByte > 0` |
|      134 | 1177 | `			&& pToken->sData.zString[pToken->sData.nByte - 1] == '\n' ){` |
|      128 | 1178 | `			pToken->sData.nByte--;` |
|      124 | 1179 | `			if( pToken->sData.nByte > 0` |
|      128 | 1180 | `				&& pToken->sData.zString[pToken->sData.nByte - 1] == '\r' ){` |
|      ! 0 | 1181 | `				pToken->sData.nByte--;` |
|      ! 0 | 1182 | `			}` |
|       62 | 1183 | `		}` |
|      138 | 1184 | `		pToken->pUserData = SX_INT_TO_PTR(nIndent);` |
|        - | 1185 | `	}` |
|        - | 1186 | `	/* All done */` |
|      138 | 1187 | `	return SXRET_OK;` |
|       71 | 1188 | `}` |
|        - | 1189 | `/*` |
|        - | 1190 | ` * Tokenize a raw PHP input.` |
|        - | 1191 | ` * This is the public tokenizer called by most code generator routines.` |
|        - | 1192 | ` */` |
|    33760 | 1193 | `PH7_PRIVATE sxi32 PH7_TokenizePHP(const char *zInput,sxu32 nLen,sxu32 nLineStart,SySet *pOut,SySet *pTrivia)` |
|        5 | 1194 | `{` |
|        - | 1195 | `	SyLex sLexer;` |
|        - | 1196 | `	sxi32 rc;` |
|        - | 1197 | `	/* Defense-in-depth cap for internal tokenizer calls that bypass ph7_compile() */` |
|    33765 | 1198 | `	if( nLen > PH7_MAX_INPUT_SIZE ){` |
|      ! 0 | 1199 | `		return SXERR_LIMIT;` |
|        - | 1200 | `	}` |
|        - | 1201 | `	/* Initialize the lexer. pTrivia (may be NULL = discard) rides as the` |
|        - | 1202 | `	 * tokenizer callback's user data: doc-comments (and later attribute` |
|        - | 1203 | `	 * groups) are recorded there instead of entering the token stream. */` |
|    33765 | 1204 | `	rc = SyLexInit(&sLexer,&(*pOut),TokenizePHP,pTrivia);` |
|    33765 | 1205 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1206 | `		return rc;` |
|        - | 1207 | `	}` |
|    33765 | 1208 | `	sLexer.sStream.nLine = nLineStart;` |
|        - | 1209 | `	/* Tokenize input */` |
|    33765 | 1210 | `	rc = SyLexTokenizeInput(&sLexer,zInput,nLen,0,0,0);` |
|        - | 1211 | `	/* Release the lexer */` |
|    33765 | 1212 | `	SyLexRelease(&sLexer);` |
|        - | 1213 | `	/* Tokenization result */` |
|    33765 | 1214 | `	return rc;` |
|    16885 | 1215 | `}` |
|        - | 1216 | `/*` |
|        - | 1217 | ` * High level public tokenizer.` |
|        - | 1218 | ` *  Tokenize the input into PHP tokens and raw tokens [i.e: HTML,XML,Raw text...].` |
|        - | 1219 | ` * According to the PHP language reference manual` |
|        - | 1220 | ` *   When PHP parses a file, it looks for opening and closing tags, which tell PHP` |
|        - | 1221 | ` *   to start and stop interpreting the code between them. Parsing in this manner allows` |
|        - | 1222 | ` *   PHP to be embedded in all sorts of different documents, as everything outside of a pair` |
|        - | 1223 | ` *   of opening and closing tags is ignored by the PHP parser. Most of the time you will see` |
|        - | 1224 | ` *   PHP embedded in HTML documents, as in this example.` |
|        - | 1225 | ` *   <?php echo 'While this is going to be parsed.'; ?>` |
|        - | 1226 | ` *   <p>This will also be ignored.</p>` |
|        - | 1227 | ` *   You can also use more advanced structures:` |
|        - | 1228 | ` *   Example #1 Advanced escaping` |
|        - | 1229 | ` * <?php` |
|        - | 1230 | ` * if ($expression) {` |
|        - | 1231 | ` *   ?>` |
|        - | 1232 | ` *   <strong>This is true.</strong>` |
|        - | 1233 | ` *   <?php` |
|        - | 1234 | ` * } else {` |
|        - | 1235 | ` *   ?>` |
|        - | 1236 | ` *   <strong>This is false.</strong>` |
|        - | 1237 | ` *   <?php` |
|        - | 1238 | ` * }` |
|        - | 1239 | ` * ?>` |
|        - | 1240 | ` * This works as expected, because when PHP hits the ?> closing tags, it simply starts outputting` |
|        - | 1241 | ` * whatever it finds (except for an immediately following newline - see instruction separation ) until it hits` |
|        - | 1242 | ` * another opening tag. The example given here is contrived, of course, but for outputting large blocks of text` |
|        - | 1243 | ` * dropping out of PHP parsing mode is generally more efficient than sending all of the text through echo() or print().` |
|        - | 1244 | ` * There are four different pairs of opening and closing tags which can be used in PHP. Three of those, <?php ?>` |
|        - | 1245 | ` * <script language="php"> </script>  and <? ?> are always available. The other two are short tags and ASP style` |
|        - | 1246 | ` * tags, and can be turned on and off from the php.ini configuration file. As such, while some people find short tags` |
|        - | 1247 | ` * and ASP style tags convenient, they are less portable, and generally not recommended.` |
|        - | 1248 | ` * Note:` |
|        - | 1249 | ` * Also note that if you are embedding PHP within XML or XHTML you will need to use the <?php ?> tags to remain` |
|        - | 1250 | ` * compliant with standards.` |
|        - | 1251 | ` * Example #2 PHP Opening and Closing Tags` |
|        - | 1252 | ` * 1.  <?php echo 'if you want to serve XHTML or XML documents, do it like this'; ?>` |
|        - | 1253 | ` * 2.  <script language="php">` |
|        - | 1254 | ` *       echo 'some editors (like FrontPage) don\'t` |
|        - | 1255 | ` *             like processing instructions';` |
|        - | 1256 | ` *   </script>` |
|        - | 1257 | ` *` |
|        - | 1258 | ` * 3.  <? echo 'this is the simplest, an SGML processing instruction'; ?>` |
|        - | 1259 | ` *   <?= expression ?> This is a shortcut for "<? echo expression ?>"` |
|        - | 1260 | ` */` |
|    17140 | 1261 | `PH7_PRIVATE sxi32 PH7_TokenizeRawText(const char *zInput,sxu32 nLen,SySet *pOut,sxu32 nBaseLine)` |
|        5 | 1262 | `{` |
|    17145 | 1263 | `	const char *zEnd = &zInput[nLen];` |
|    17145 | 1264 | `	const char *zIn  = zInput;` |
|        - | 1265 | `	const char *zCur,*zCurEnd;` |
|    17145 | 1266 | `	SyString sCtag = { 0, 0 };     /* Closing tag */` |
|        - | 1267 | `	SyToken sToken;` |
|        - | 1268 | `	SyString sDoc;` |
|        - | 1269 | `	sxu32 nLine;` |
|        - | 1270 | `	sxi32 iNest;` |
|        - | 1271 | `	sxi32 rc;` |
|        - | 1272 | `	/* Tokenize the input into PHP tokens and raw tokens. nBaseLine is normally 1,` |
|        - | 1273 | `	 * but 2 when a "#!" shebang line was stripped so error lines still match php. */` |
|    17145 | 1274 | `	nLine = nBaseLine;` |
|    17145 | 1275 | `	zCur = zCurEnd   = 0; /* Prevent compiler warning */` |
|    17145 | 1276 | `	sToken.pUserData = 0;` |
|    17145 | 1277 | `	iNest = 0;` |
|    17145 | 1278 | `	sDoc.nByte = 0;` |
|    17145 | 1279 | `	sDoc.zString = ""; /* cc warning */` |
|    17122 | 1280 | `	for(;;){` |
|    33623 | 1281 | `		if( zIn >= zEnd ){` |
|        - | 1282 | `			/* End of input reached */` |
|    16475 | 1283 | `			break;` |
|        - | 1284 | `		}` |
|    17153 | 1285 | `		sToken.nLine = nLine;` |
|    17153 | 1286 | `		zCur = zIn;` |
|    17153 | 1287 | `		zCurEnd = 0;` |
|    17865 | 1288 | `		while( zIn < zEnd ){` |
|    17195 | 1289 | `			 if( zIn[0] == '<' ){` |
|    16483 | 1290 | `				const char *zTmp = zIn; /* End of raw input marker */` |
|    16483 | 1291 | `				zIn++;` |
|    16483 | 1292 | `				if( zIn < zEnd ){` |
|    16483 | 1293 | `					if( zIn[0] == '?' ){` |
|    16483 | 1294 | `						zIn++;` |
|    16483 | 1295 | `						if( (sxu32)(zEnd - zIn) >= sizeof("php")-1 &&  SyStrnicmp(zIn,"php",sizeof("php")-1) == 0 ){` |
|        - | 1296 | `							/* opening tag: <?php */` |
|    16481 | 1297 | `							zIn += sizeof("php")-1;` |
|     8238 | 1298 | `						}` |
|        - | 1299 | `						/* Look for the closing tag '?>' */` |
|    16483 | 1300 | `						SyStringInitFromBuf(&sCtag,"?>",sizeof("?>")-1);` |
|    16483 | 1301 | `						zCurEnd = zTmp;` |
|    16483 | 1302 | `						break;` |
|        - | 1303 | `					}` |
|      ! 0 | 1304 | `				}` |
|      ! 0 | 1305 | `			}else{` |
|      717 | 1306 | `				if( zIn[0] == '\n' ){` |
|        7 | 1307 | `					nLine++;` |
|        3 | 1308 | `				}` |
|      717 | 1309 | `				zIn++;` |
|        - | 1310 | `			 }` |
|        5 | 1311 | `		} /* While(zIn < zEnd) */` |
|    17153 | 1312 | `		if( zCurEnd == 0 ){` |
|       48 | 1313 | `			zCurEnd = zIn;` |
|       22 | 1314 | `		}` |
|        - | 1315 | `		/* Save the raw token */` |
|    17153 | 1316 | `		SyStringInitFromBuf(&sToken.sData,zCur,zCurEnd - zCur);` |
|    17153 | 1317 | `		sToken.nType = PH7_TOKEN_RAW;` |
|    17153 | 1318 | `		rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|    17153 | 1319 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1320 | `			return rc;` |
|        - | 1321 | `		}` |
|    17153 | 1322 | `		if( zIn >= zEnd ){` |
|       48 | 1323 | `			break;` |
|        - | 1324 | `		}` |
|        - | 1325 | `		/* Ignore leading white space */` |
|    35141 | 1326 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    18037 | 1327 | `			if( zIn[0] == '\n' ){` |
|    17789 | 1328 | `				nLine++;` |
|     8892 | 1329 | `			}` |
|    18037 | 1330 | `			zIn++;` |
|        5 | 1331 | `		}` |
|        - | 1332 | `		/* Delimit the PHP chunk */` |
|    17109 | 1333 | `		sToken.nLine = nLine;` |
|    17109 | 1334 | `		zCur = zIn;` |
|  4162409 | 1335 | `		while( (sxu32)(zEnd - zIn) >= sCtag.nByte ){` |
|        - | 1336 | `			const char *zPtr;` |
|  4154643 | 1337 | `			if( SyMemcmp(zIn,sCtag.zString,sCtag.nByte) == 0 && iNest < 1 ){` |
|     8717 | 1338 | `				break;` |
|        - | 1339 | `			}` |
|        - | 1340 | `			/* Line comment ('#' or '//', but not the '#[' attribute opener): php` |
|        - | 1341 | `			 * ends it at a newline OR at the closing tag, so a '?>' inside a line` |
|        - | 1342 | `			 * comment DOES close the PHP block. Skipping the comment here also` |
|        - | 1343 | `			 * stops the string skip below from treating a quote inside the` |
|        - | 1344 | `			 * comment as a string. Only outside a heredoc body (iNest < 1). */` |
|  4155198 | 1345 | `			if( iNest < 1 &&` |
|  4140270 | 1346 | `				( (zIn[0] == '#' && !(zIn+1 < zEnd && zIn[1] == '[')) \|\|` |
|  4141457 | 1347 | `				  (zIn[0] == '/' && zIn+1 < zEnd && zIn[1] == '/') ) ){` |
|    18539 | 1348 | `				zIn += (zIn[0] == '#') ? 1 : 2;` |
|   792503 | 1349 | `				while( zIn < zEnd && zIn[0] != '\n' ){` |
|   773966 | 1350 | `					if( (sxu32)(zEnd - zIn) >= sCtag.nByte` |
|   773968 | 1351 | `						&& SyMemcmp(zIn,sCtag.zString,sCtag.nByte) == 0 ){` |
|        3 | 1352 | `						break; /* the closing tag terminates the line comment */` |
|        - | 1353 | `					}` |
|   773969 | 1354 | `					zIn++;` |
|        5 | 1355 | `				}` |
|    14907 | 1356 | `				continue;` |
|        - | 1357 | `			}` |
|        - | 1358 | `			/* Block comment: spans everything, including '?>', up to its close. */` |
|  4130403 | 1359 | `			if( iNest < 1 && zIn[0] == '/' && zIn+1 < zEnd && zIn[1] == '*' ){` |
|     3483 | 1360 | `				zIn += 2;` |
|   545627 | 1361 | `				while( (sxu32)(zEnd-zIn) >= sizeof("*/") - 1 ){` |
|   545627 | 1362 | `					if( zIn[0] == '*' && zIn[1] == '/' ){` |
|     3483 | 1363 | `						zIn += 2;` |
|     3483 | 1364 | `						break;` |
|        - | 1365 | `					}` |
|   542149 | 1366 | `					if( zIn[0] == '\n' ){` |
|     5153 | 1367 | `						nLine++;` |
|     2574 | 1368 | `					}` |
|   542149 | 1369 | `					zIn++;` |
|        5 | 1370 | `				}` |
|     3483 | 1371 | `				continue;` |
|        - | 1372 | `			}` |
|        - | 1373 | `			/* Skip over a single/double-quoted or backtick string literal so a` |
|        - | 1374 | `			 * '?>' sequence inside it is not mistaken for the closing tag. Only` |
|        - | 1375 | `			 * outside a heredoc body (iNest < 1); heredocs are delimited by the` |
|        - | 1376 | `			 * label-matching logic above. Escapes (\" \' \\ and a line-continuing` |
|        - | 1377 | `			 * backslash-newline) are honoured. Same-quote nesting inside "{$...}"` |
|        - | 1378 | `			 * interpolation is not tracked, but that can only end the skip early` |
|        - | 1379 | `			 * on a string that has no '?>' anyway, which stays a PHP chunk either` |
|        - | 1380 | `			 * way — it never mis-splits code that works today. */` |
|  4126925 | 1381 | ``			if( iNest < 1 && (zIn[0] == '\'' \|\| zIn[0] == '"' \|\| zIn[0] == '`') ){`` |
|   116703 | 1382 | `				int qch = zIn[0];` |
|   116703 | 1383 | `				zIn++;` |
|  1035005 | 1384 | `				while( zIn < zEnd ){` |
|  1035005 | 1385 | `					if( zIn[0] == '\\' && zIn + 1 < zEnd ){` |
|    34023 | 1386 | `						if( zIn[1] == '\n' ){ nLine++; }` |
|    34023 | 1387 | `						zIn += 2;` |
|    34023 | 1388 | `						continue;` |
|        - | 1389 | `					}` |
|  1000987 | 1390 | `					if( zIn[0] == qch ){ zIn++; break; }` |
|   884289 | 1391 | `					if( zIn[0] == '\n' ){ nLine++; }` |
|   884289 | 1392 | `					zIn++;` |
|        5 | 1393 | `				}` |
|   116703 | 1394 | `				continue;` |
|        - | 1395 | `			}` |
|  4010227 | 1396 | `			if( zIn[0] == '\n' ){` |
|   152375 | 1397 | `				nLine++;` |
|   152375 | 1398 | `				if( iNest > 0 ){` |
|      386 | 1399 | `					zIn++;` |
|      880 | 1400 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|      497 | 1401 | `						zIn++;` |
|        3 | 1402 | `					}` |
|      386 | 1403 | `					zPtr = zIn;` |
|     2051 | 1404 | `					while( zIn < zEnd && LEX_LABEL_BYTE(zIn[0]) ){` |
|     1478 | 1405 | `						zIn++;` |
|        4 | 1406 | `					}` |
|      386 | 1407 | `					if( (sxu32)(zIn - zPtr) == sDoc.nByte && SyMemcmp(sDoc.zString,zPtr,sDoc.nByte) == 0 ){` |
|      134 | 1408 | `						iNest = 0;` |
|       65 | 1409 | `					}` |
|      386 | 1410 | `					continue;` |
|        5 | 1411 | `				}` |
|  3933851 | 1412 | `			}else if ( (sxu32)(zEnd - zIn) >= sizeof("<<<") && zIn[0] == '<' && zIn[1] == '<' && zIn[2] == '<' && iNest < 1){` |
|      136 | 1413 | `				zIn += sizeof("<<<")-1;` |
|      148 | 1414 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|       13 | 1415 | `					zIn++;` |
|        1 | 1416 | `				}` |
|      136 | 1417 | `				if( zIn[0] == '"' \|\| zIn[0] == '\'' ){` |
|       58 | 1418 | `					zIn++;` |
|       27 | 1419 | `				}` |
|      136 | 1420 | `				zPtr = zIn;` |
|      706 | 1421 | `				while( zIn < zEnd && LEX_LABEL_BYTE(zIn[0]) ){` |
|      508 | 1422 | `					zIn++;` |
|        4 | 1423 | `				}` |
|      136 | 1424 | `				SyStringInitFromBuf(&sDoc,zPtr,zIn-zPtr);` |
|      136 | 1425 | `				SyStringFullTrim(&sDoc);` |
|      136 | 1426 | `				if( sDoc.nByte > 0 ){` |
|      136 | 1427 | `					iNest++;` |
|       66 | 1428 | `				}` |
|      136 | 1429 | `				continue;` |
|        - | 1430 | `			}` |
|  4009713 | 1431 | `			zIn++;` |
|        - | 1432 |  |
|  4009713 | 1433 | `			if ( zIn >= zEnd )` |
|      ! 0 | 1434 | `				break;` |
|        5 | 1435 | `		}` |
|    16483 | 1436 | `		if( (sxu32)(zEnd - zIn) < sCtag.nByte ){` |
|     7771 | 1437 | `			zIn = zEnd;` |
|     3883 | 1438 | `		}` |
|    16483 | 1439 | `		if( zCur < zIn ){` |
|        - | 1440 | `			/* Save the PHP chunk for later processing */` |
|    12747 | 1441 | `			sToken.nType = PH7_TOKEN_PHP;` |
|    12747 | 1442 | `			SyStringInitFromBuf(&sToken.sData,zCur,zIn-zCur);` |
|    24839 | 1443 | `			SyStringRightTrim(&sToken.sData); /* Trim trailing white spaces */` |
|    12747 | 1444 | `			rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|    12747 | 1445 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1446 | `				return rc;` |
|        - | 1447 | `			}` |
|     6371 | 1448 | `		}` |
|    16483 | 1449 | `		if( zIn < zEnd ){` |
|        - | 1450 | `			/* Jump the trailing closing tag */` |
|     8717 | 1451 | `			zIn += sCtag.nByte;` |
|        - | 1452 | `			/* php's lexer swallows exactly ONE newline immediately after the` |
|        - | 1453 | `			 * closing tag ("?>\n" emits nothing) */` |
|     8717 | 1454 | `			if( zIn < zEnd && zIn[0] == '\r' && zIn + 1 < zEnd && zIn[1] == '\n' ){` |
|      ! 0 | 1455 | `				zIn += 2;` |
|      ! 0 | 1456 | `				nLine++;` |
|     8717 | 1457 | `			}else if( zIn < zEnd && zIn[0] == '\n' ){` |
|      111 | 1458 | `				zIn++;` |
|      111 | 1459 | `				nLine++;` |
|       53 | 1460 | `			}` |
|     4356 | 1461 | `		}` |
|        5 | 1462 | `	} /* For(;;) */` |
|        - | 1463 |  |
|    16519 | 1464 | ` 	return SXRET_OK;` |
|     8262 | 1465 | `}` |
|        - | 1466 |  |
