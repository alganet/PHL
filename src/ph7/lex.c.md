# src/ph7/lex.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 896/956 lines (93.72%)

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
|        - |   34 | ` * php's cast is a SCANNER pattern, not three tokens the parser puts back together:` |
|        - |   35 | ` *` |
|        - |   36 | ` *   "(" [ \t]* ("int"\|"integer"\|"bool"\|"boolean"\|"float"\|"double"\|"real"` |
|        - |   37 | ` *              \|"string"\|"binary"\|"array"\|"object"\|"unset"\|"void") [ \t]* ")"` |
|        - |   38 | ` *` |
|        - |   39 | `` * Three facts follow from that, and PHL -- which used to MERGE `(`, a keyword and`` |
|        - |   40 | ``  * `)` after the fact -- had none of them. The name is matched as TEXT, so `double` `` |
|        - |   41 | `` * and `binary` are casts (php's two aliases this engine had no keyword for, so`` |
|        - |   42 | `` * `(double)$x` was `syntax error, unexpected variable`), while neither word is`` |
|        - |   43 | `` * reserved anywhere else (`function double() {}` still compiles). Only TABS AND`` |
|        - |   44 | ` * SPACES may sit inside, so a newline or a comment between the parentheses is NOT` |
|        - |   45 | `` * a cast in php -- `(\nint\n) $b` is a parenthesised constant followed by a stray`` |
|        - |   46 | ` * variable -- where the merge accepted any separation at all. And the match runs` |
|        - |   47 | `` * wherever the three bytes meet, so `strlen(int)` is php's stray `(int)` token`` |
|        - |   48 | ` * rather than a call passing a constant.` |
|        - |   49 | ` *` |
|        - |   50 | ` * Answers the CANONICAL spelling php reports the token by -- both aliases of a` |
|        - |   51 | ` * pair report the primary name -- or 0 when this is an ordinary parenthesis.` |
|        - |   52 | `` * `*pbAlias` says whether the source spelled one of the four NON-CANONICAL names`` |
|        - |   53 | ` * php 8.5 deprecates, which the canonical text on its own can no longer tell.` |
|        - |   54 | ` */` |
|  2466115 |   55 | `static const char * LexCastToken(const unsigned char *zIn,const unsigned char *zEnd,` |
|        - |   56 | `	const unsigned char **pzNext,int *pbAlias)` |
|        5 |   57 | `{` |
|        - |   58 | `	static const struct { const char *zName; int nName; const char *zCanon; int bAlias; } aCast[] = {` |
|        - |   59 | `		{ "int",     3, "(int)",    0 }, { "integer", 7, "(int)",    1 },` |
|        - |   60 | `		{ "bool",    4, "(bool)",   0 }, { "boolean", 7, "(bool)",   1 },` |
|        - |   61 | `		{ "float",   5, "(float)",  0 }, { "double",  6, "(float)",  1 },` |
|        - |   62 | `		{ "string",  6, "(string)", 0 }, { "binary",  6, "(string)", 1 },` |
|        - |   63 | `		{ "array",   5, "(array)",  0 }, { "object",  6, "(object)", 0 },` |
|        - |   64 | `		{ "unset",   5, "(unset)",  0 }, { "real",    4, "(real)",   0 },` |
|        - |   65 | `		{ "void",    4, "(void)",   0 }` |
|        - |   66 | `	};` |
|  2466120 |   67 | `	const unsigned char *z = zIn,*zName;` |
|        - |   68 | `	sxu32 nName;` |
|        - |   69 | `	sxu32 i;` |
|  4441879 |   70 | `	while( z < zEnd && (z[0] == ' ' \|\| z[0] == '\t') ){` |
|   744971 |   71 | `		z++;` |
|        5 |   72 | `	}` |
|  2466120 |   73 | `	zName = z;` |
|  4764190 |   74 | `	while( z < zEnd && z[0] < 0x80 && SyisAlpha(z[0]) ){` |
|  2298075 |   75 | `		z++;` |
|        5 |   76 | `	}` |
|  2466120 |   77 | `	nName = (sxu32)(z - zName);` |
|  3890381 |   78 | `	while( z < zEnd && (z[0] == ' ' \|\| z[0] == '\t') ){` |
|   193473 |   79 | `		z++;` |
|        5 |   80 | `	}` |
|  2466120 |   81 | `	if( nName < 1 \|\| z >= zEnd \|\| z[0] != ')' ){` |
|  2413469 |   82 | `		return 0;` |
|        - |   83 | `	}` |
|   227622 |   84 | `	for( i = 0 ; i < SX_ARRAYSIZE(aCast) ; ++i ){` |
|   224615 |   85 | `		if( nName == (sxu32)aCast[i].nName` |
|   150001 |   86 | `		 && SyStrnicmp((const char *)zName,aCast[i].zName,nName) == 0 ){` |
|    49654 |   87 | `			*pzNext = &z[1];` |
|    49654 |   88 | `			*pbAlias = aCast[i].bAlias;` |
|    49654 |   89 | `			return aCast[i].zCanon;` |
|        - |   90 | `		}` |
|    87235 |   91 | `	}` |
|     3007 |   92 | `	return 0;` |
|  1230798 |   93 | `}` |
|        - |   94 | `/*` |
|        - |   95 | ` * Tokenize a raw PHP input.` |
|        - |   96 | ` * Get a single low-level token from the input file. Update the stream pointer so that` |
|        - |   97 | ` * it points to the first character beyond the extracted token.` |
|        - |   98 | ` */` |
| 27273557 |   99 | `static sxi32 TokenizePHP(SyStream *pStream,SyToken *pToken,void *pUserData,void *pCtxData)` |
|        5 |  100 | `{` |
|        - |  101 | `	SyString *pStr;` |
|        - |  102 | `	sxi32 rc;` |
|        - |  103 | `	/* Ignore leading white spaces */` |
| 44330862 |  104 | `	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){` |
|        - |  105 | `		/* Advance the stream cursor */` |
| 17057305 |  106 | `		if( pStream->zText[0] == '\n' ){` |
|        - |  107 | `			/* Update line counter */` |
|   191287 |  108 | `			pStream->nLine++;` |
|    95200 |  109 | `		}` |
| 17057305 |  110 | `		pStream->zText++;` |
|        5 |  111 | `	}` |
| 27273562 |  112 | `	if( pStream->zText >= pStream->zEnd ){` |
|        - |  113 | `		/* End of input reached */` |
|      ! 0 |  114 | `		return SXERR_EOF;` |
|        - |  115 | `	}` |
|        - |  116 | `	/* Record token starting position and line */` |
| 27273562 |  117 | `	pToken->nLine = pStream->nLine;` |
| 27273562 |  118 | `	pToken->pUserData = 0;` |
| 27273562 |  119 | `	pStr = &pToken->sData;` |
| 27273562 |  120 | `	SyStringInitFromBuf(pStr,pStream->zText,0);` |
|        - |  121 | ``	/* php's BINARY-STRING PREFIX. A lone `b`/`B` welded to a quote or to `<<<` is`` |
|        - |  122 | `	 * not an identifier: php's scanner takes it as part of the string token, and` |
|        - |  123 | `	 * since php has ONE string type it marks nothing at runtime -- which is exactly` |
|        - |  124 | ``	 * why it survives in real source (`b'foo'` is `'foo'`). PHL lexed the letter as`` |
|        - |  125 | ``	 * a label, so every such literal was `syntax error, unexpected`` |
|        - |  126 | `` 	 * single-quoted string`. The prefix is ADJACENT only: `b <<<'S'` and `bb'foo'` `` |
|        - |  127 | `	 * are an identifier followed by a string, in php as here. */` |
| 27273557 |  128 | `	if( (pStream->zText[0] == 'b' \|\| pStream->zText[0] == 'B')` |
| 13672117 |  129 | `	 && &pStream->zText[1] < pStream->zEnd` |
| 13672024 |  130 | `	 && ( pStream->zText[1] == '\'' \|\| pStream->zText[1] == '"'` |
|   115283 |  131 | `	   \|\| ( pStream->zText[1] == '<' && &pStream->zText[3] < pStream->zEnd` |
|        4 |  132 | `	     && pStream->zText[2] == '<' && pStream->zText[3] == '<' ) ) ){` |
|       23 |  133 | `		pStream->zText++;                       /* the prefix is not part of the VALUE */` |
|       23 |  134 | `		SyStringInitFromBuf(pStr,pStream->zText,0);` |
|       11 |  135 | `	}` |
| 31353472 |  136 | `	if( LEX_LABEL_START(pStream->zText[0]) ){` |
|        - |  137 | `		const unsigned char *zIn;` |
|        - |  138 | `		sxu32 nKeyword;` |
|        - |  139 | `		/* Isolate the LABEL. php's class is a flat byte set — [a-zA-Z_\x80-\xff] then` |
|        - |  140 | `		 * [a-zA-Z0-9_\x80-\xff]* — so no UTF-8 decoding is involved: a multibyte name` |
|        - |  141 | `		 * is consumed because every one of its bytes is >= 0x80. (This replaces the` |
|        - |  142 | `		 * xPP lead-byte-plus-continuations dance, which required a lead >= 0xc0 and so` |
|        - |  143 | `		 * stopped one byte class short of php's own rule; see LEX_LABEL_START.) */` |
|  8146768 |  144 | `		zIn = &pStream->zText[1];` |
| 47825173 |  145 | `		while( zIn < pStream->zEnd && LEX_LABEL_BYTE(zIn[0]) ){` |
| 35611557 |  146 | `			zIn++;` |
|        5 |  147 | `		}` |
|  8146768 |  148 | `		pStream->zText = zIn;` |
|        - |  149 | `		/* Record token length */` |
|  8146768 |  150 | `		pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  8146768 |  151 | `		nKeyword = KeywordCodeCI(pStr->zString,(int)pStr->nByte);` |
|  8146768 |  152 | `		if( nKeyword != PH7_TK_ID ){` |
|  2728872 |  153 | `			if( nKeyword &` |
|        - |  154 | `				(PH7_TKWRD_NEW\|PH7_TKWRD_CLONE\|PH7_TKWRD_AND\|PH7_TKWRD_XOR\|PH7_TKWRD_OR\|PH7_TKWRD_INSTANCEOF) ){` |
|        - |  155 | `					/* Alpha stream operators [i.e: new,clone,and,instanceof,or,xor],save the operator instance for later processing */` |
|   132788 |  156 | `					pToken->pUserData = (void *)PH7_ExprExtractOperator(pStr,0);` |
|        - |  157 | `					/* Mark as an operator */` |
|   132788 |  158 | `					pToken->nType = PH7_TK_ID\|PH7_TK_OP;` |
|    66302 |  159 | `			}else{` |
|        - |  160 | `				/* We are dealing with a keyword [i.e: while,foreach,class...],save the keyword ID */` |
|  2596089 |  161 | `				pToken->nType = PH7_TK_KEYWORD;` |
|  2596089 |  162 | `				pToken->pUserData = SX_INT_TO_PTR(nKeyword);` |
|        - |  163 | `			}` |
|  1362387 |  164 | `		}else{` |
|        - |  165 | `			/* A simple identifier */` |
|  5417901 |  166 | `			pToken->nType = PH7_TK_ID;` |
|        - |  167 | `		}` |
|  4066858 |  168 | `	}else{` |
|        - |  169 | `		sxi32 c;` |
|        - |  170 | `		/* Non-alpha stream */` |
| 19126799 |  171 | `		if( pStream->zText[0] == '#' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '[' ){` |
|      689 |  172 | `			sxu32 nDepth = 1;` |
|        - |  173 | `			/* PHP 8 attribute group '#[ ... ]': skip the whole balanced group as` |
|        - |  174 | `			 * trivia (attributes are not stored yet). Brackets inside string` |
|        - |  175 | `			 * literals and comments must not affect the depth count. An` |
|        - |  176 | `			 * unterminated group is silently consumed up to EOF, consistent` |
|        - |  177 | `			 * with unterminated block comments below.` |
|        - |  178 | `			 */` |
|        - |  179 | `			const unsigned char *zGroupStart;` |
|      689 |  180 | `			pStream->zText += 2;` |
|      689 |  181 | `			zGroupStart = pStream->zText;` |
|    11793 |  182 | `			while( pStream->zText < pStream->zEnd && nDepth > 0 ){` |
|    11109 |  183 | `				sxi32 d = pStream->zText[0];` |
|    11109 |  184 | `				if( d == '[' ){` |
|       32 |  185 | `					nDepth++;` |
|    11094 |  186 | `				}else if( d == ']' ){` |
|      719 |  187 | `					nDepth--;` |
|    10722 |  188 | `				}else if( d == '\'' \|\| d == '"' ){` |
|        - |  189 | `					/* String literal: scan for the matching unescaped quote */` |
|      136 |  190 | `					pStream->zText++;` |
|      760 |  191 | `					while( pStream->zText < pStream->zEnd ){` |
|      760 |  192 | `						if( pStream->zText[0] == '\\' && &pStream->zText[1] < pStream->zEnd ){` |
|       23 |  193 | `							if( pStream->zText[1] == '\n' ){` |
|      ! 0 |  194 | `								pStream->nLine++;` |
|      ! 0 |  195 | `							}` |
|       23 |  196 | `							pStream->zText += 2;` |
|       23 |  197 | `							continue;` |
|        - |  198 | `						}` |
|      738 |  199 | `						if( pStream->zText[0] == d ){` |
|      136 |  200 | `							break;` |
|        - |  201 | `						}` |
|      606 |  202 | `						if( pStream->zText[0] == '\n' ){` |
|      ! 0 |  203 | `							pStream->nLine++;` |
|      ! 0 |  204 | `						}` |
|      606 |  205 | `						pStream->zText++;` |
|        4 |  206 | `					}` |
|      136 |  207 | `					if( pStream->zText >= pStream->zEnd ){` |
|      ! 0 |  208 | `						break; /* Unterminated string literal */` |
|        4 |  209 | `					}` |
|        - |  210 | `					/* Fall through: consume the closing quote below */` |
|    10299 |  211 | `				}else if( d == '#' \|\| (d == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '/') ){` |
|        - |  212 | `					/* Inline comment inside the group */` |
|      ! 0 |  213 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] != '\n' ){` |
|      ! 0 |  214 | `						pStream->zText++;` |
|      ! 0 |  215 | `					}` |
|      ! 0 |  216 | `					continue; /* Let the outer loop count the newline */` |
|    10233 |  217 | `				}else if( d == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '*' ){` |
|        - |  218 | `					/* Block comment inside the group */` |
|      ! 0 |  219 | `					pStream->zText += 2;` |
|      ! 0 |  220 | `					while( pStream->zText < pStream->zEnd ){` |
|      ! 0 |  221 | `						if( pStream->zText[0] == '*' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '/' ){` |
|      ! 0 |  222 | `							pStream->zText += 2;` |
|      ! 0 |  223 | `							break;` |
|        - |  224 | `						}` |
|      ! 0 |  225 | `						if( pStream->zText[0] == '\n' ){` |
|      ! 0 |  226 | `							pStream->nLine++;` |
|      ! 0 |  227 | `						}` |
|      ! 0 |  228 | `						pStream->zText++;` |
|      ! 0 |  229 | `					}` |
|      ! 0 |  230 | `					continue;` |
|    10233 |  231 | `				}else if( d == '\n' ){` |
|        7 |  232 | `					pStream->nLine++;` |
|        3 |  233 | `				}` |
|    11109 |  234 | `				pStream->zText++;` |
|        5 |  235 | `			}` |
|      689 |  236 | `			if( pUserData && pStream->pSet ){` |
|        - |  237 | `				/* Record the group's inner span (between #[ and its balanced ])` |
|        - |  238 | `				 * in the trivia sidecar, keyed like doc-comments. */` |
|        - |  239 | `				ph7_trivia sTrivia;` |
|      689 |  240 | `				const unsigned char *zGroupEnd = pStream->zText;` |
|      689 |  241 | `				if( nDepth == 0 && zGroupEnd > zGroupStart ){` |
|      689 |  242 | `					zGroupEnd--; /* Exclude the closing ']' */` |
|      342 |  243 | `				}` |
|      689 |  244 | `				sTrivia.nTokIdx = SySetUsed(pStream->pSet);` |
|      689 |  245 | `				sTrivia.iKind = PH7_TRIVIA_ATTR;` |
|      689 |  246 | `				SyStringInitFromBuf(&sTrivia.sText,(const char *)zGroupStart,(sxu32)(zGroupEnd - zGroupStart));` |
|      689 |  247 | `				sTrivia.nLine = pToken->nLine;` |
|      689 |  248 | `				SySetPut((SySet *)pUserData,(const void *)&sTrivia);` |
|      342 |  249 | `			}` |
|        - |  250 | `			/* Tell the upper-layer to ignore this token */` |
|      689 |  251 | `			return SXERR_CONTINUE;` |
| 19299690 |  252 | `		}else if( pStream->zText[0] == '#' \|\|` |
| 19126104 |  253 | `			( pStream->zText[0] == '/' &&  &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '/') ){` |
|    16325 |  254 | `				pStream->zText++;` |
|        - |  255 | `				/* Inline comments */` |
|   894001 |  256 | `				while( pStream->zText < pStream->zEnd && pStream->zText[0] != '\n' ){` |
|   877681 |  257 | `					pStream->zText++;` |
|        5 |  258 | `				}` |
|        - |  259 | `				/* Tell the upper-layer to ignore this token */` |
|    16325 |  260 | `				return SXERR_CONTINUE;` |
| 19109795 |  261 | `		}else if( pStream->zText[0] == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '*' ){` |
|        - |  262 | `			/* A doc-comment starts with slash-star-star followed by more` |
|        - |  263 | `			 * content (slash-star-star-slash is the empty comment, not a` |
|        - |  264 | `			 * docblock). Its full span, delimiters included, goes to the` |
|        - |  265 | `			 * trivia sidecar when the caller supplied one — keyed by the` |
|        - |  266 | `			 * index the NEXT real token receives — and never enters the` |
|        - |  267 | `			 * token stream. */` |
|   322447 |  268 | `			const unsigned char *zDocStart = pStream->zText;` |
|   322686 |  269 | `			int bDoc = ( &pStream->zText[2] < pStream->zEnd && pStream->zText[2] == '*'` |
|   484144 |  270 | `			 && ( &pStream->zText[3] >= pStream->zEnd \|\| pStream->zText[3] != '/' ) );` |
|   322447 |  271 | `			pStream->zText += 2;` |
|        - |  272 | `			/* Block comment */` |
| 53065363 |  273 | `			while( pStream->zText < pStream->zEnd ){` |
| 53065359 |  274 | `				if( pStream->zText[0] == '*' ){` |
|   481287 |  275 | `					if( &pStream->zText[1] >= pStream->zEnd \|\| pStream->zText[1] == '/'  ){` |
|   160982 |  276 | `						break;` |
|        - |  277 | `					}` |
|    79293 |  278 | `				}` |
| 52742921 |  279 | `				if( pStream->zText[0] == '\n' ){` |
|     7986 |  280 | `					pStream->nLine++;` |
|     3966 |  281 | `				}` |
| 52742921 |  282 | `				pStream->zText++;` |
|        5 |  283 | `			}` |
|   322442 |  284 | `			if( pStream->zText >= pStream->zEnd` |
|   322445 |  285 | `			 \|\| !(pStream->zText[0] == '*' && &pStream->zText[1] < pStream->zEnd` |
|   322438 |  286 | `			   && pStream->zText[1] == '/') ){` |
|        - |  287 | `				/* The comment never closed before the end of the input. php refuses` |
|        - |  288 | ``				 * the file (`Unterminated comment starting line N`) where this`` |
|        - |  289 | `				 * swallowed the rest of it in silence; hand the compile phase a` |
|        - |  290 | `				 * token to report it with. */` |
|        4 |  291 | `				pToken->nType = PH7_TK_OTHER\|PH7_TK_UNTERM;` |
|        4 |  292 | `				SyStringInitFromBuf(&pToken->sData,"/*",sizeof("/*")-1);` |
|        4 |  293 | `				pStream->zText = pStream->zEnd;` |
|        4 |  294 | `				return SXRET_OK;` |
|        - |  295 | `			}` |
|   322443 |  296 | `			pStream->zText += 2;` |
|   322443 |  297 | `			if( bDoc && pUserData && pStream->pSet ){` |
|        - |  298 | `				ph7_trivia sTrivia;` |
|      483 |  299 | `				const unsigned char *zDocEnd = pStream->zText;` |
|      483 |  300 | `				if( zDocEnd > pStream->zEnd ){` |
|      ! 0 |  301 | `					zDocEnd = pStream->zEnd; /* Unterminated comment at EOF */` |
|      ! 0 |  302 | `				}` |
|      483 |  303 | `				sTrivia.nTokIdx = SySetUsed(pStream->pSet);` |
|      483 |  304 | `				sTrivia.iKind = PH7_TRIVIA_DOC;` |
|      483 |  305 | `				SyStringInitFromBuf(&sTrivia.sText,(const char *)zDocStart,(sxu32)(zDocEnd - zDocStart));` |
|      483 |  306 | `				sTrivia.nLine = pToken->nLine;` |
|      483 |  307 | `				SySetPut((SySet *)pUserData,(const void *)&sTrivia);` |
|      239 |  308 | `			}` |
|        - |  309 | `			/* Tell the upper-layer to ignore this token */` |
|   322443 |  310 | `			return SXERR_CONTINUE;` |
| 18787353 |  311 | `		}else if( SyisDigit(pStream->zText[0]) ){` |
|   943396 |  312 | `			pStream->zText++;` |
|        - |  313 | `			/* PHP 7.4: handle underscore separator immediately following the first digit.` |
|        - |  314 | `			 * Check pStream->zText < pStream->zEnd BEFORE forming pStream->zText + 1 so` |
|        - |  315 | `			 * we never compute a pointer past one-past-end. */` |
|   943391 |  316 | `			if( pStream->zText < pStream->zEnd` |
|   943381 |  317 | `				&& pStream->zText[0] == '_'` |
|   471086 |  318 | `				&& pStream->zText + 1 < pStream->zEnd` |
|      168 |  319 | `				&& pStream->zText[1] < 0xc0` |
|      173 |  320 | `				&& SyisDigit(pStream->zText[1]) ){` |
|      158 |  321 | `				pStream->zText++; /* swallow underscore between two digits */` |
|       78 |  322 | `			}` |
|        - |  323 | `			/* Decimal digit stream (PHP 7.4: underscore separator allowed between two digits) */` |
|  1311315 |  324 | `			while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|   367924 |  325 | `				pStream->zText++;` |
|   367919 |  326 | `				if( pStream->zText < pStream->zEnd` |
|   367919 |  327 | `					&& pStream->zText[0] == '_'` |
|   183752 |  328 | `					&& pStream->zText + 1 < pStream->zEnd` |
|      172 |  329 | `					&& pStream->zText[1] < 0xc0` |
|      177 |  330 | `					&& SyisDigit(pStream->zText[1]) ){` |
|      173 |  331 | `					pStream->zText++; /* swallow underscore between two digits */` |
|       86 |  332 | `				}` |
|        5 |  333 | `			}` |
|        - |  334 | `			/* Mark the token as integer until we encounter a real number */` |
|   943396 |  335 | `			pToken->nType = PH7_TK_INTEGER;` |
|   943396 |  336 | `			if( pStream->zText < pStream->zEnd ){` |
|   943376 |  337 | `				c = pStream->zText[0];` |
|   943376 |  338 | `				if( c == '.' ){` |
|        - |  339 | `					/* Real number (PHP 7.4: underscore separator allowed between two digits) */` |
|    18082 |  340 | `					pStream->zText++;` |
|    38279 |  341 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|    20202 |  342 | `						pStream->zText++;` |
|    20197 |  343 | `						if( pStream->zText < pStream->zEnd` |
|    20197 |  344 | `							&& pStream->zText[0] == '_'` |
|    10093 |  345 | `							&& pStream->zText + 1 < pStream->zEnd` |
|       12 |  346 | `							&& pStream->zText[1] < 0xc0` |
|       17 |  347 | `							&& SyisDigit(pStream->zText[1]) ){` |
|       13 |  348 | `							pStream->zText++;` |
|        6 |  349 | `						}` |
|        5 |  350 | `					}` |
|    18082 |  351 | `					if( pStream->zText < pStream->zEnd ){` |
|    18082 |  352 | `						c = pStream->zText[0];` |
|    18082 |  353 | `						if( c=='e' \|\| c=='E' ){` |
|      149 |  354 | `							pStream->zText++;` |
|      149 |  355 | `							if( pStream->zText < pStream->zEnd ){` |
|      149 |  356 | `								c = pStream->zText[0];` |
|      146 |  357 | `								if( (c =='+' \|\| c=='-') && &pStream->zText[1] < pStream->zEnd  &&` |
|      119 |  358 | `									pStream->zText[1] < 0xc0 && SyisDigit(pStream->zText[1]) ){` |
|      119 |  359 | `										pStream->zText++;` |
|       58 |  360 | `								}` |
|      429 |  361 | `								while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|      283 |  362 | `									pStream->zText++;` |
|      280 |  363 | `									if( pStream->zText < pStream->zEnd` |
|      280 |  364 | `										&& pStream->zText[0] == '_'` |
|      144 |  365 | `										&& pStream->zText + 1 < pStream->zEnd` |
|        8 |  366 | `										&& pStream->zText[1] < 0xc0` |
|       11 |  367 | `										&& SyisDigit(pStream->zText[1]) ){` |
|        9 |  368 | `										pStream->zText++;` |
|        4 |  369 | `									}` |
|        3 |  370 | `								}` |
|       73 |  371 | `							}` |
|       73 |  372 | `						}` |
|     9027 |  373 | `					}` |
|    18082 |  374 | `					pToken->nType = PH7_TK_REAL;` |
|   934326 |  375 | `				}else if( c=='e' \|\| c=='E' ){` |
|       95 |  376 | `					SXUNUSED(pUserData); /* Prevent compiler warning */` |
|       95 |  377 | `					SXUNUSED(pCtxData);` |
|      195 |  378 | `					pStream->zText++;` |
|      195 |  379 | `					if( pStream->zText < pStream->zEnd ){` |
|      195 |  380 | `						c = pStream->zText[0];` |
|      190 |  381 | `						if( (c =='+' \|\| c=='-') && &pStream->zText[1] < pStream->zEnd  &&` |
|       45 |  382 | `							pStream->zText[1] < 0xc0 && SyisDigit(pStream->zText[1]) ){` |
|       43 |  383 | `								pStream->zText++;` |
|       20 |  384 | `						}` |
|      587 |  385 | `						while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|      397 |  386 | `							pStream->zText++;` |
|      392 |  387 | `							if( pStream->zText < pStream->zEnd` |
|      392 |  388 | `								&& pStream->zText[0] == '_'` |
|      198 |  389 | `								&& pStream->zText + 1 < pStream->zEnd` |
|        4 |  390 | `								&& pStream->zText[1] < 0xc0` |
|        9 |  391 | `								&& SyisDigit(pStream->zText[1]) ){` |
|        5 |  392 | `								pStream->zText++;` |
|        2 |  393 | `							}` |
|        5 |  394 | `						}` |
|       95 |  395 | `					}` |
|      195 |  396 | `					pToken->nType = PH7_TK_REAL;` |
|        - |  397 | `				/* php only reads a base prefix when the literal so far is exactly "0"` |
|        - |  398 | `				 * AND at least one valid digit follows it. Otherwise the '0' stands` |
|        - |  399 | `				 * alone as an integer and the letter begins an IDENTIFIER, which is` |
|        - |  400 | ``				 * why php reports `0xG` as `unexpected identifier "xG"` while PHL,`` |
|        - |  401 | `				 * consuming the prefix unconditionally, reported just "G". The same` |
|        - |  402 | ``				 * gap silently ACCEPTED `0x`/`0b`/`0o` as int(0), and read `1x5` as`` |
|        - |  403 | `				 * a hex literal, both of which php rejects outright. */` |
|   925204 |  404 | `				}else if( (c == 'x' \|\| c == 'X')` |
|   462059 |  405 | `					&& pStream->zText == &((const unsigned char *)pStr->zString)[1] && pStr->zString[0] == 0x30` |
|      357 |  406 | `					&& &pStream->zText[1] < pStream->zEnd` |
|      362 |  407 | `					&& pStream->zText[1] < 0xc0 && SyisHex(pStream->zText[1]) ){` |
|        - |  408 | `					/* Hex digit stream (PHP 7.4: underscore separator allowed between two digits) */` |
|      358 |  409 | `					pStream->zText++;` |
|     1758 |  410 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisHex(pStream->zText[0]) ){` |
|     1405 |  411 | `						pStream->zText++;` |
|     1400 |  412 | `						if( pStream->zText < pStream->zEnd` |
|     1400 |  413 | `							&& pStream->zText[0] == '_'` |
|      721 |  414 | `							&& pStream->zText + 1 < pStream->zEnd` |
|       50 |  415 | `							&& pStream->zText[1] < 0xc0` |
|       55 |  416 | `							&& SyisHex(pStream->zText[1]) ){` |
|       51 |  417 | `							pStream->zText++;` |
|       25 |  418 | `						}` |
|        5 |  419 | `					}` |
|   924932 |  420 | `				}else if( (c == 'b' \|\| c == 'B')` |
|   461848 |  421 | `					&& pStream->zText == &((const unsigned char *)pStr->zString)[1] && pStr->zString[0] == 0x30` |
|      288 |  422 | `					&& &pStream->zText[1] < pStream->zEnd` |
|      293 |  423 | `					&& (pStream->zText[1] == '0' \|\| pStream->zText[1] == '1') ){` |
|        - |  424 | `					/* Binary digit stream (PHP 7.4: underscore separator allowed between two digits) */` |
|      287 |  425 | `					pStream->zText++;` |
|     3115 |  426 | `					while( pStream->zText < pStream->zEnd && (pStream->zText[0] == '0' \|\| pStream->zText[0] == '1') ){` |
|     1791 |  427 | `						pStream->zText++;` |
|     1790 |  428 | `						if( pStream->zText < pStream->zEnd` |
|     1790 |  429 | `							&& pStream->zText[0] == '_'` |
|      965 |  430 | `							&& pStream->zText + 1 < pStream->zEnd` |
|      141 |  431 | `							&& (pStream->zText[1] == '0' \|\| pStream->zText[1] == '1') ){` |
|      141 |  432 | `							pStream->zText++;` |
|       70 |  433 | `						}` |
|        1 |  434 | `					}` |
|   924609 |  435 | `				}else if( (c == 'o' \|\| c == 'O')` |
|   461571 |  436 | `					&& pStream->zText == &((const unsigned char *)pStr->zString)[1] && pStr->zString[0] == 0x30` |
|       20 |  437 | `					&& &pStream->zText[1] < pStream->zEnd` |
|       25 |  438 | `					&& pStream->zText[1] >= '0' && pStream->zText[1] <= '7' ){` |
|        - |  439 | `					/* PHP 8.1 explicit octal 0o/0O (underscore separator allowed between two digits) */` |
|       21 |  440 | `					pStream->zText++;` |
|      101 |  441 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] >= '0' && pStream->zText[0] <= '7' ){` |
|       81 |  442 | `						pStream->zText++;` |
|       80 |  443 | `						if( pStream->zText < pStream->zEnd` |
|       80 |  444 | `							&& pStream->zText[0] == '_'` |
|       41 |  445 | `							&& pStream->zText + 1 < pStream->zEnd` |
|        3 |  446 | `							&& pStream->zText[1] >= '0' && pStream->zText[1] <= '7' ){` |
|        3 |  447 | `							pStream->zText++;` |
|        1 |  448 | `						}` |
|        1 |  449 | `					}` |
|       10 |  450 | `				}` |
|   471002 |  451 | `			}` |
|        - |  452 | `			/* A MISPLACED PHP 7.4 separator needs nothing here: it is where php's` |
|        - |  453 | ``			 * scanner stops the number and starts a LABEL, so `1_` is the integer`` |
|        - |  454 | ``			 * 1 followed by the identifier `_` and `0x_1f` the integer 0 followed`` |
|        - |  455 | ``			 * by `x_1f` -- which is the identifier php's parse error names. This`` |
|        - |  456 | `			 * used to ABSORB the run into the numeric token and re-report it from` |
|        - |  457 | `			 * the compile phase; that named the same thing until a leading-dot` |
|        - |  458 | ``			 * float could follow it, at which point `1_.5` complained about the`` |
|        - |  459 | ``			 * `.5` two tokens later where php complains about the `_`. */`` |
|        - |  460 | `			/* Record token length */` |
|   943396 |  461 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|   943396 |  462 | `			return SXRET_OK;` |
| 17843957 |  463 | `		}else if( pStream->zText[0] == '.' && &pStream->zText[1] < pStream->zEnd` |
|   303340 |  464 | `			&& pStream->zText[1] < 0xc0 && SyisDigit(pStream->zText[1]) ){` |
|        - |  465 | `` 			/* php's DNUM has a leading-dot form -- `({LNUM}?"."{LNUM})` -- so `.5` `` |
|        - |  466 | `			 * is a float literal and not the concatenation operator followed by a 5.` |
|        - |  467 | `			 * The scanner takes it UNCONDITIONALLY, wherever the dot stands: php` |
|        - |  468 | ``			 * reads `"x".5` as a string followed by the float `.5` and reports a`` |
|        - |  469 | `			 * parse error for it, which is why a program that means concatenation` |
|        - |  470 | ``			 * has to write the space (`"x". 5`). PHL had no such token at all, so`` |
|        - |  471 | ``			 * every `.5` in real source -- an ordinary way to write a fraction --`` |
|        - |  472 | ``			 * was `syntax error, unexpected token ";"`. */`` |
|       33 |  473 | `			pStream->zText++; /* Jump the dot */` |
|      117 |  474 | `			while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0` |
|      105 |  475 | `				&& SyisDigit(pStream->zText[0]) ){` |
|       41 |  476 | `				pStream->zText++;` |
|       38 |  477 | `				if( pStream->zText < pStream->zEnd` |
|       38 |  478 | `					&& pStream->zText[0] == '_'` |
|       20 |  479 | `					&& pStream->zText + 1 < pStream->zEnd` |
|        2 |  480 | `					&& pStream->zText[1] < 0xc0` |
|        5 |  481 | `					&& SyisDigit(pStream->zText[1]) ){` |
|        3 |  482 | `					pStream->zText++; /* swallow underscore between two digits */` |
|        1 |  483 | `				}` |
|        3 |  484 | `			}` |
|       30 |  485 | `			if( pStream->zText < pStream->zEnd` |
|       33 |  486 | `			 && (pStream->zText[0] == 'e' \|\| pStream->zText[0] == 'E') ){` |
|        - |  487 | `				/* An EXPONENT_DNUM built on this DNUM. The sign is taken only when a` |
|        - |  488 | ``				 * digit follows it, exactly as the two runs above do, so `.5e+` is`` |
|        - |  489 | ``				 * the float `.5` followed by an identifier rather than a bad float. */`` |
|        5 |  490 | `				const unsigned char *zRewind = pStream->zText;` |
|        5 |  491 | `				pStream->zText++;` |
|        4 |  492 | `				if( pStream->zText < pStream->zEnd` |
|        4 |  493 | `				 && (pStream->zText[0] == '+' \|\| pStream->zText[0] == '-')` |
|        3 |  494 | `				 && &pStream->zText[1] < pStream->zEnd` |
|        4 |  495 | `				 && pStream->zText[1] < 0xc0 && SyisDigit(pStream->zText[1]) ){` |
|        3 |  496 | `					pStream->zText++;` |
|        1 |  497 | `				}` |
|        4 |  498 | `				if( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0` |
|        5 |  499 | `				 && SyisDigit(pStream->zText[0]) ){` |
|       14 |  500 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0` |
|       13 |  501 | `						&& SyisDigit(pStream->zText[0]) ){` |
|        5 |  502 | `						pStream->zText++;` |
|        4 |  503 | `						if( pStream->zText < pStream->zEnd` |
|        4 |  504 | `							&& pStream->zText[0] == '_'` |
|        2 |  505 | `							&& pStream->zText + 1 < pStream->zEnd` |
|      ! 0 |  506 | `							&& pStream->zText[1] < 0xc0` |
|        1 |  507 | `							&& SyisDigit(pStream->zText[1]) ){` |
|      ! 0 |  508 | `							pStream->zText++; /* swallow underscore between two digits */` |
|      ! 0 |  509 | `						}` |
|        1 |  510 | `					}` |
|        3 |  511 | `				}else{` |
|      ! 0 |  512 | `					pStream->zText = zRewind;` |
|        - |  513 | `				}` |
|        2 |  514 | `			}` |
|       33 |  515 | `			pToken->nType = PH7_TK_REAL;` |
|       33 |  516 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|       33 |  517 | `			return SXRET_OK;` |
|        - |  518 | `		}` |
| 17843932 |  519 | `		c = pStream->zText[0];` |
| 17843932 |  520 | `		pStream->zText++; /* Advance the stream cursor */` |
|        - |  521 | `		/* Assume we are dealing with an operator*/` |
| 17843932 |  522 | `		pToken->nType = PH7_TK_OP;` |
| 17843932 |  523 | `		switch(c){` |
|  3824716 |  524 | `		case '$': pToken->nType = PH7_TK_DOLLAR; break;` |
|  1026230 |  525 | `		case '{': pToken->nType = PH7_TK_OCB;    break;` |
|  1026216 |  526 | `		case '}': pToken->nType = PH7_TK_CCB;    break;` |
|  1235322 |  527 | `		case '(': {` |
|        - |  528 | `			/* A type cast is recognised HERE, off the raw bytes, exactly as php's` |
|        - |  529 | `			 * scanner does it (LexCastToken above). */` |
|  2466120 |  530 | `			const unsigned char *zNext = 0;` |
|  2466120 |  531 | `			int bAlias = 0;` |
|  2466120 |  532 | `			const char *zCanon = LexCastToken(pStream->zText,pStream->zEnd,&zNext,&bAlias);` |
|  2466120 |  533 | `			if( zCanon ){` |
|    49654 |  534 | `				pStream->zText = zNext;` |
|    49654 |  535 | `				SyStringInitFromBuf(&pToken->sData,zCanon,SyStrlen(zCanon));` |
|    49654 |  536 | `				if( SyStrncmp(zCanon,"(void)",sizeof("(void)")-1) == 0 ){` |
|        - |  537 | ``					/* php 8.5's `(void)`, which converts nothing and is taken by the`` |
|        - |  538 | `					 * grammar only at the head of an expression STATEMENT. */` |
|       33 |  539 | `					pToken->nType = PH7_TK_VOID_CAST;` |
|       33 |  540 | `					pToken->pUserData = 0;` |
|       17 |  541 | `				}else{` |
|    49622 |  542 | `					pToken->nType = PH7_TK_OP;` |
|    49622 |  543 | `					if( bAlias ){` |
|        - |  544 | `						/* php 8.5 deprecates the spelling, not the cast. Carry the fact to` |
|        - |  545 | `						 * the chunk's pre-scan, which announces it the way php's scanner` |
|        - |  546 | `						 * does -- ahead of the program, and ahead of a parse error further` |
|        - |  547 | `						 * down the file. */` |
|       40 |  548 | `						pToken->nType \|= PH7_TK_ALIAS_CAST;` |
|       19 |  549 | `					}` |
|    49622 |  550 | `					pToken->pUserData = (void *)PH7_ExprExtractOperator(&pToken->sData,0);` |
|        - |  551 | `				}` |
|    49654 |  552 | `				return SXRET_OK;` |
|        - |  553 | `			}` |
|  2416471 |  554 | `			pToken->nType = PH7_TK_LPAREN;` |
|  2416471 |  555 | `			break;` |
|        - |  556 | `				  }` |
|   379189 |  557 | `		case '[': pToken->nType \|= PH7_TK_OSB;   break; /* Bitwise operation here,since the square bracket token '['` |
|        - |  558 | `														 * is a potential operator [i.e: subscripting] */` |
|   379195 |  559 | `		case ']': pToken->nType = PH7_TK_CSB;    break;` |
|  1210446 |  560 | `		case ')':` |
|  2416449 |  561 | `			pToken->nType = PH7_TK_RPAREN;` |
|  2416449 |  562 | `			break;` |
|   365388 |  563 | `		case '\'':{` |
|        - |  564 | `			/* Single quoted string */` |
|   729099 |  565 | `			pStr->zString++;` |
| 11525490 |  566 | `			while( pStream->zText < pStream->zEnd ){` |
| 11525486 |  567 | `				if( pStream->zText[0] == '\''  ){` |
|   729178 |  568 | `					if( pStream->zText[-1] != '\\' ){` |
|   728831 |  569 | `						break;` |
|      ! 0 |  570 | `					}else{` |
|      352 |  571 | `						const unsigned char *zPtr = &pStream->zText[-2];` |
|      352 |  572 | `						sxi32 i = 1;` |
|      640 |  573 | `						while( zPtr > pStream->zInput && zPtr[0] == '\\' ){` |
|      293 |  574 | `							zPtr--;` |
|      293 |  575 | `							i++;` |
|        5 |  576 | `						}` |
|      352 |  577 | `						if((i&1)==0){` |
|      269 |  578 | `							break;` |
|        - |  579 | `						}` |
|        - |  580 | `					}` |
|       41 |  581 | `				}` |
| 10796396 |  582 | `				if( pStream->zText[0] == '\n' ){` |
|      125 |  583 | `					pStream->nLine++;` |
|       62 |  584 | `				}` |
| 10796396 |  585 | `				pStream->zText++;` |
|        5 |  586 | `			}` |
|        - |  587 | `			/* Record token length and type. Running into the END OF THE INPUT` |
|        - |  588 | `			 * instead of the closing quote is marked: php refuses the file, where` |
|        - |  589 | `			 * this consumed the rest of it and ran the program. */` |
|   729099 |  590 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|   729099 |  591 | `			pToken->nType = PH7_TK_SSTR;` |
|   729099 |  592 | `			if( pStream->zText >= pStream->zEnd ){` |
|        4 |  593 | `				pToken->nType \|= PH7_TK_UNTERM;` |
|        2 |  594 | `			}` |
|        - |  595 | `			/* Jump the trailing single quote */` |
|   729099 |  596 | `			pStream->zText++;` |
|   729099 |  597 | `			return SXRET_OK;` |
|        - |  598 | `				  }` |
|    41522 |  599 | `		case '"':{` |
|        - |  600 | `			sxi32 iNest;` |
|        - |  601 | `			/* Double quoted string */` |
|    82829 |  602 | `			pStr->zString++;` |
|  1204036 |  603 | `			while( pStream->zText < pStream->zEnd ){` |
|  1204028 |  604 | `				if( pStream->zText[0] == '{' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '$'){` |
|      238 |  605 | `					iNest = 1;` |
|      238 |  606 | `					pStream->zText++;` |
|        - |  607 | `					/* TICKET 1433-40: Hnadle braces'{}' in double quoted string where everything is allowed */` |
|     2149 |  608 | `					while(pStream->zText < pStream->zEnd ){` |
|     2149 |  609 | `						if( pStream->zText[0] == '{' ){` |
|        3 |  610 | `							iNest++;` |
|     2148 |  611 | `						}else if (pStream->zText[0] == '}' ){` |
|      240 |  612 | `							iNest--;` |
|      240 |  613 | `							if( iNest <= 0 ){` |
|      238 |  614 | `								pStream->zText++;` |
|      238 |  615 | `								break;` |
|        1 |  616 | `							}` |
|     1913 |  617 | `						}else if( pStream->zText[0] == '\n' ){` |
|      ! 0 |  618 | `							pStream->nLine++;` |
|      ! 0 |  619 | `						}` |
|     1916 |  620 | `						pStream->zText++;` |
|        5 |  621 | `					}` |
|      238 |  622 | `					if( pStream->zText >= pStream->zEnd ){` |
|      ! 0 |  623 | `						break;` |
|        - |  624 | `					}` |
|      116 |  625 | `				}` |
|  1204028 |  626 | `				if( pStream->zText[0] == '"' ){` |
|    84003 |  627 | `					if( pStream->zText[-1] != '\\' ){` |
|    82785 |  628 | `						break;` |
|      ! 0 |  629 | `					}else{` |
|     1223 |  630 | `						const unsigned char *zPtr = &pStream->zText[-2];` |
|     1223 |  631 | `						sxi32 i = 1;` |
|     1311 |  632 | `						while( zPtr > pStream->zInput && zPtr[0] == '\\' ){` |
|       91 |  633 | `							zPtr--;` |
|       91 |  634 | `							i++;` |
|        3 |  635 | `						}` |
|     1223 |  636 | `						if((i&1)==0){` |
|       38 |  637 | `							break;` |
|        - |  638 | `						}` |
|        - |  639 | `					}` |
|      591 |  640 | `				}` |
|  1121212 |  641 | `				if( pStream->zText[0] == '\n' ){` |
|       77 |  642 | `					pStream->nLine++;` |
|       37 |  643 | `				}` |
|  1121212 |  644 | `				pStream->zText++;` |
|        5 |  645 | `			}` |
|        - |  646 | `			/* Record token length and type (see the single-quoted branch above for` |
|        - |  647 | `			 * the unterminated mark). */` |
|    82829 |  648 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|    82829 |  649 | `			pToken->nType = PH7_TK_DSTR;` |
|    82829 |  650 | `			if( pStream->zText >= pStream->zEnd ){` |
|        8 |  651 | `				pToken->nType \|= PH7_TK_UNTERM;` |
|        4 |  652 | `			}` |
|        - |  653 | `			/* Jump the trailing quote */` |
|    82829 |  654 | `			pStream->zText++;` |
|    82829 |  655 | `			return SXRET_OK;` |
|        - |  656 | `				  }` |
|        1 |  657 | ``		case '`':{`` |
|        - |  658 | `			/* Backtick quoted string */` |
|        3 |  659 | `			pStr->zString++;` |
|       21 |  660 | `			while( pStream->zText < pStream->zEnd ){` |
|       21 |  661 | ``				if( pStream->zText[0] == '`' && pStream->zText[-1] != '\\' ){`` |
|        3 |  662 | `					break;` |
|        - |  663 | `				}` |
|       19 |  664 | `				if( pStream->zText[0] == '\n' ){` |
|      ! 0 |  665 | `					pStream->nLine++;` |
|      ! 0 |  666 | `				}` |
|       19 |  667 | `				pStream->zText++;` |
|        1 |  668 | `			}` |
|        - |  669 | `			/* Record token length and type */` |
|        3 |  670 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|        3 |  671 | `			pToken->nType = PH7_TK_BSTR;` |
|        - |  672 | `			/* Jump the trailing backtick */` |
|        3 |  673 | `			pStream->zText++;` |
|        3 |  674 | `			return SXRET_OK;` |
|        - |  675 | `				  }` |
|     1447 |  676 | `		case '\\':{` |
|        - |  677 | `` 			/* php's scanner never hands its parser a `\` from INSIDE a name: `A\B` `` |
|        - |  678 | ``			 * and `\A\B` are one token each (T_NAME_QUALIFIED, T_NAME_FULLY_QUALIFIED,`` |
|        - |  679 | ``			 * matched as `{LABEL}("\\"{LABEL})+` so the backslash binds only to a`` |
|        - |  680 | ``			 * label glued to it), and a `\` reaches the grammar on its own`` |
|        - |  681 | `			 * (T_NS_SEPARATOR) only when no label follows -- which the grammar` |
|        - |  682 | ``			 * takes in exactly one place, before the `{` of a group `use`. The name`` |
|        - |  683 | `			 * walkers here read a separator TOKEN between segments and none of them` |
|        - |  684 | ``			 * looked at the bytes around it, so `A\ B`, `A \ B`, `A \B` and a`` |
|        - |  685 | ``			 * trailing `A\;` all compiled as the name `A\B`. The byte is classified`` |
|        - |  686 | `			 * once, here:` |
|        - |  687 | `			 *  - glued to a label on its right: a name lexeme, PH7_TK_NSSEP, which` |
|        - |  688 | `			 *    the walkers read as before;` |
|        - |  689 | `			 *  - nothing glued on its right: php's bare separator, an ordinary` |
|        - |  690 | `			 *    token no walker takes and the group-use door recognizes by text;` |
|        - |  691 | `			 *  - a name HEAD (glued right, not left) straight after a plain` |
|        - |  692 | `			 *    identifier or a variable's name: a fully-qualified name where php's` |
|        - |  693 | `			 *    grammar has no room for a second name, which its parse error names` |
|        - |  694 | `			 *    whole -- so the whole name becomes this one token, marked for the` |
|        - |  695 | ``			 *    noun. After a keyword (`echo \A`, `new \A`, `extends \A`) a name`` |
|        - |  696 | ``			 *    may START, and `readonly`, an identifier to this lexer, is a`` |
|        - |  697 | ``			 *    keyword there too (`public readonly \A $p`). */`` |
|     2894 |  698 | `			const unsigned char *zNext = pStream->zText;` |
|     2894 |  699 | `			if( zNext >= pStream->zEnd \|\| !LEX_LABEL_START(zNext[0]) ){` |
|       38 |  700 | `				pToken->nType = PH7_TK_OTHER;` |
|       38 |  701 | `				break;` |
|        - |  702 | `			}` |
|     2860 |  703 | `			pToken->nType = PH7_TK_NSSEP;` |
|     2855 |  704 | `			if( (const unsigned char *)pStr->zString > pStream->zInput` |
|     2623 |  705 | `			 && LEX_LABEL_BYTE(((const unsigned char *)pStr->zString)[-1]) ){` |
|      548 |  706 | `				break; /* a segment separator */` |
|        - |  707 | `			}` |
|        - |  708 | `			{` |
|     1773 |  709 | `				SyToken *pPrev = (SyToken *)SySetPeek(pStream->pSet);` |
|     1768 |  710 | `				if( pPrev && (pPrev->nType & PH7_TK_ID) && (pPrev->nType & PH7_TK_OP) == 0` |
|       70 |  711 | `				 && !( pPrev->sData.nByte == sizeof("readonly")-1` |
|        9 |  712 | `					&& SyStrnicmp(pPrev->sData.zString,"readonly",sizeof("readonly")-1) == 0 ) ){` |
|        - |  713 | `					/* Take php's whole token: "\\"{LABEL}("\\"{LABEL})* */` |
|        3 |  714 | `					for(;;){` |
|        7 |  715 | `						zNext++;` |
|       10 |  716 | `						while( zNext < pStream->zEnd && LEX_LABEL_BYTE(zNext[0]) ){` |
|      ! 0 |  717 | `							zNext++;` |
|      ! 0 |  718 | `						}` |
|        7 |  719 | `						if( &zNext[1] < pStream->zEnd && zNext[0] == '\\' && LEX_LABEL_START(zNext[1]) ){` |
|      ! 0 |  720 | `							zNext++;` |
|      ! 0 |  721 | `							continue;` |
|        - |  722 | `						}` |
|        7 |  723 | `						break;` |
|      ! 0 |  724 | `					}` |
|        7 |  725 | `					pStream->zText = zNext;` |
|        7 |  726 | `					pToken->nType = PH7_TK_OTHER\|PH7_TK_FQNAME;` |
|        3 |  727 | `				}` |
|        - |  728 | `			}` |
|     1773 |  729 | `			break;` |
|        - |  730 | `		}` |
|   105806 |  731 | `		case ':':` |
|   211300 |  732 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == ':' ){` |
|        - |  733 | `				/* Current operator: '::' */` |
|     4706 |  734 | `				pStream->zText++;` |
|     2347 |  735 | `			}else{` |
|   206599 |  736 | `				pToken->nType = PH7_TK_COLON; /* Single colon */` |
|        - |  737 | `			}` |
|   211300 |  738 | `			break;` |
|   768503 |  739 | `		case ',': pToken->nType \|= PH7_TK_COMMA;  break; /* Comma is also an operator */` |
|  1641968 |  740 | `		case ';': pToken->nType = PH7_TK_SEMI;    break;` |
|        - |  741 | `			/* Handle combined operators [i.e: +=,===,!=== ...] */` |
|   643548 |  742 | `		case '=':` |
|  1284998 |  743 | `			pToken->nType \|= PH7_TK_EQUAL;` |
|  1284998 |  744 | `			if( pStream->zText < pStream->zEnd ){` |
|  1284998 |  745 | `				if( pStream->zText[0] == '=' ){` |
|   354117 |  746 | `					pToken->nType &= ~PH7_TK_EQUAL;` |
|        - |  747 | `					/* Current operator: == */` |
|   354117 |  748 | `					pStream->zText++;` |
|   354117 |  749 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  750 | `						/* Current operator: === */` |
|   274231 |  751 | `						pStream->zText++;` |
|   136909 |  752 | `					}` |
|  1107678 |  753 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  754 | `					/* Array operator: => */` |
|    37365 |  755 | `					pToken->nType = PH7_TK_ARRAY_OP;` |
|    37365 |  756 | `					pStream->zText++;` |
|    18545 |  757 | `				}else{` |
|        - |  758 | `					/* TICKET 1433-0010: Reference operator '=&' */` |
|   893526 |  759 | `					const unsigned char *zCur = pStream->zText;` |
|   893526 |  760 | `					sxu32 nLine = 0;` |
|  1786407 |  761 | `					while( zCur < pStream->zEnd && zCur[0] < 0xc0 && SyisSpace(zCur[0]) ){` |
|   892886 |  762 | `						if( zCur[0] == '\n' ){` |
|       10 |  763 | `							nLine++;` |
|        4 |  764 | `						}` |
|   892886 |  765 | `						zCur++;` |
|        5 |  766 | `					}` |
|   893526 |  767 | `					if( zCur < pStream->zEnd && zCur[0] == '&' ){` |
|        - |  768 | `						/* Current operator: =& */` |
|      471 |  769 | `						pToken->nType &= ~PH7_TK_EQUAL;` |
|      471 |  770 | `						SyStringInitFromBuf(pStr,"=&",sizeof("=&")-1);` |
|        - |  771 | `						/* Update token stream */` |
|      471 |  772 | `						pStream->zText = &zCur[1];` |
|      471 |  773 | `						pStream->nLine += nLine;` |
|      233 |  774 | `					}` |
|        - |  775 | `				}` |
|   641445 |  776 | `			}` |
|  1284998 |  777 | `			break;` |
|   116221 |  778 | `		case '!':` |
|   232122 |  779 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  780 | `				/* Current operator: != */` |
|   159608 |  781 | `				pStream->zText++;` |
|   159608 |  782 | `				if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  783 | `					/* Current operator: !== */` |
|   127868 |  784 | `					pStream->zText++;` |
|    63842 |  785 | `				}` |
|    79690 |  786 | `			}` |
|   232122 |  787 | `			break;` |
|   132574 |  788 | `		case '&':` |
|   264773 |  789 | `			pToken->nType \|= PH7_TK_AMPER;` |
|   264773 |  790 | `			if( pStream->zText < pStream->zEnd ){` |
|   264773 |  791 | `				if( pStream->zText[0] == '&' ){` |
|   135552 |  792 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|        - |  793 | `					/* Current operator: && */` |
|   135552 |  794 | `					pStream->zText++;` |
|   196903 |  795 | `				}else if( pStream->zText[0] == '=' ){` |
|      407 |  796 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|        - |  797 | `					/* Current operator: &= */` |
|      407 |  798 | `					pStream->zText++;` |
|      203 |  799 | `				}` |
|   132194 |  800 | `			}` |
|   264773 |  801 | `			break;` |
|    76476 |  802 | `		case '\|':` |
|   152747 |  803 | `			if( pStream->zText < pStream->zEnd ){` |
|   152747 |  804 | `				if( pStream->zText[0] == '\|' ){` |
|        - |  805 | `					/* Current operator: \|\| */` |
|    95618 |  806 | `					pStream->zText++;` |
|   104874 |  807 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  808 | `					/* Current operator: \|= */` |
|      415 |  809 | `					pStream->zText++;` |
|    56927 |  810 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  811 | `					/* Current operator: \|> (PHP 8.5 pipe) */` |
|       27 |  812 | `					pStream->zText++;` |
|       13 |  813 | `				}` |
|    76266 |  814 | `			}` |
|   152747 |  815 | `			break;` |
|    57313 |  816 | `		case '+':` |
|   114466 |  817 | `			if( pStream->zText < pStream->zEnd ){` |
|   114466 |  818 | `				if( pStream->zText[0] == '+' ){` |
|        - |  819 | `					/* Current operator: ++ */` |
|    40778 |  820 | `					pStream->zText++;` |
|    94050 |  821 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  822 | `					/* Current operator: += */` |
|     8556 |  823 | `					pStream->zText++;` |
|     4269 |  824 | `				}` |
|    57148 |  825 | `			}` |
|   114466 |  826 | `			break;` |
|    81773 |  827 | `		case '-':` |
|   163319 |  828 | `			if( pStream->zText < pStream->zEnd ){` |
|   163319 |  829 | `				if( pStream->zText[0] == '-' ){` |
|        - |  830 | `					/* Current operator: -- */` |
|    31786 |  831 | `					pStream->zText++;` |
|   147406 |  832 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  833 | `					/* Current operator: -= */` |
|      412 |  834 | `					pStream->zText++;` |
|   131333 |  835 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  836 | `					/* Current operator: -> */` |
|    31972 |  837 | `					pStream->zText++;` |
|    15963 |  838 | `				}` |
|    81541 |  839 | `			}` |
|   163319 |  840 | `			break;` |
|     5104 |  841 | `		case '*':` |
|    10199 |  842 | `			if( pStream->zText < pStream->zEnd ){` |
|    10199 |  843 | `				if( pStream->zText[0] == '*' ){` |
|        - |  844 | `					/* Current operator: ** or **= */` |
|     1034 |  845 | `					pStream->zText++;` |
|     1034 |  846 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  847 | `						/* Current operator: **= */` |
|      431 |  848 | `						pStream->zText++;` |
|      217 |  849 | `					}` |
|     9683 |  850 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  851 | `					/* Current operator: *= */` |
|      451 |  852 | `					pStream->zText++;` |
|      224 |  853 | `				}` |
|     5090 |  854 | `			}` |
|    10199 |  855 | `			break;` |
|     4450 |  856 | `		case '/':` |
|     8894 |  857 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  858 | `				/* Current operator: /= */` |
|      423 |  859 | `				pStream->zText++;` |
|      211 |  860 | `			}` |
|     8894 |  861 | `			break;` |
|    16402 |  862 | `		case '%':` |
|    32765 |  863 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  864 | `				/* Current operator: %= */` |
|      409 |  865 | `				pStream->zText++;` |
|      204 |  866 | `			}` |
|    32765 |  867 | `			break;` |
|      425 |  868 | `		case '^':` |
|      852 |  869 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  870 | `				/* Current operator: ^= */` |
|      407 |  871 | `				pStream->zText++;` |
|      203 |  872 | `			}` |
|      852 |  873 | `			break;` |
|   151882 |  874 | `		case '.':` |
|   303310 |  875 | `			if( pStream->zText + 1 < pStream->zEnd && pStream->zText[0] == '.' && pStream->zText[1] == '.' ){` |
|        - |  876 | `				/* Ellipsis: ... */` |
|     9004 |  877 | `				pStream->zText += 2;` |
|     9004 |  878 | `				pToken->nType = PH7_TK_ELLIPSIS;` |
|   298804 |  879 | `			}else if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  880 | `				/* Current operator: .= */` |
|    16891 |  881 | `				pStream->zText++;` |
|     8430 |  882 | `			}` |
|   303310 |  883 | `			break;` |
|    56773 |  884 | `		case '<':` |
|   113391 |  885 | `			if( pStream->zText < pStream->zEnd ){` |
|   113391 |  886 | `				if( pStream->zText[0] == '<' ){` |
|        - |  887 | `					/* Current operator: << */` |
|     1067 |  888 | `					pStream->zText++;` |
|     1067 |  889 | `					if( pStream->zText < pStream->zEnd ){` |
|     1067 |  890 | `						if( pStream->zText[0] == '=' ){` |
|        - |  891 | `							/* Current operator: <<= */` |
|      419 |  892 | `							pStream->zText++;` |
|      858 |  893 | `						}else if( pStream->zText[0] == '<' ){` |
|        - |  894 | `							/* Current Token: <<<  */` |
|      164 |  895 | `							pStream->zText++;` |
|        - |  896 | `							/* This may be the beginning of a Heredoc/Nowdoc string,try to delimit it */` |
|      164 |  897 | `							rc = LexExtractHeredoc(&(*pStream),&(*pToken));` |
|      164 |  898 | `							if( rc == SXRET_OK ){` |
|        - |  899 | `								/* Here/Now doc successfuly extracted */` |
|      164 |  900 | `								return SXRET_OK;` |
|        - |  901 | `							}` |
|      ! 0 |  902 | `						}` |
|      453 |  903 | `					}` |
|   112779 |  904 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  905 | `					/* Current operator: <> */` |
|        5 |  906 | `					pStream->zText++;` |
|   112326 |  907 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  908 | `					/* Current operator: <= or <=> */` |
|     8300 |  909 | `					pStream->zText++;` |
|     8300 |  910 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '>' ){` |
|        - |  911 | `						/* Current operator: <=> */` |
|      259 |  912 | `						pStream->zText++;` |
|      127 |  913 | `					}` |
|     4142 |  914 | `				}` |
|    56533 |  915 | `			}` |
|   113231 |  916 | `			break;` |
|    56238 |  917 | `		case '>':` |
|   112320 |  918 | `			if( pStream->zText < pStream->zEnd ){` |
|   112320 |  919 | `				if( pStream->zText[0] == '>' ){` |
|        - |  920 | `					/* Current operator: >> */` |
|    24634 |  921 | `					pStream->zText++;` |
|    24634 |  922 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  923 | `						/* Current operator: >>= */` |
|      411 |  924 | `						pStream->zText++;` |
|      210 |  925 | `					}` |
|    99989 |  926 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  927 | `					/* Current operator: >= */` |
|    23921 |  928 | `					pStream->zText++;` |
|    11941 |  929 | `				}` |
|    56077 |  930 | `			}` |
|   112320 |  931 | `			break;` |
|    22766 |  932 | `		case '?':` |
|    45476 |  933 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '?' ){` |
|        - |  934 | `				/* Null coalescing operator: ?? */` |
|      757 |  935 | `				pStream->zText++;` |
|      757 |  936 | `				if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  937 | `					/* Null coalescing assignment operator (PHP 7.4) */` |
|      201 |  938 | `					pStream->zText++;` |
|       98 |  939 | `				}` |
|    45100 |  940 | `			}else if( (pStream->zEnd - pStream->zText) >= 2` |
|    44724 |  941 | `				&& pStream->zText[0] == '-' && pStream->zText[1] == '>' ){` |
|        - |  942 | `				/* Nullsafe object operator (PHP 8.0): ?-> */` |
|      221 |  943 | `				pStream->zText += 2;` |
|      108 |  944 | `			}` |
|    45471 |  945 | `			break;` |
|    24901 |  946 | `		default:` |
|    49717 |  947 | `			break;` |
|        - |  948 | `		}` |
| 16982203 |  949 | `		if( pStr->nByte <= 0 ){` |
|        - |  950 | `			/* Record token length */` |
| 16981737 |  951 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  8476808 |  952 | `		}` |
| 16982203 |  953 | `		if( pToken->nType & PH7_TK_OP ){` |
|        - |  954 | `			const ph7_expr_op *pOp;` |
|        - |  955 | `			/* Check if the extracted token is an operator */` |
|  3995151 |  956 | `			pOp = PH7_ExprExtractOperator(pStr,(SyToken *)SySetPeek(pStream->pSet));` |
|  3995151 |  957 | `			if( pOp == 0 ){` |
|        - |  958 | `				/* Not an operator */` |
|      ! 0 |  959 | `				pToken->nType &= ~PH7_TK_OP;` |
|      ! 0 |  960 | `				if( pToken->nType <= 0 ){` |
|      ! 0 |  961 | `					pToken->nType = PH7_TK_OTHER;` |
|      ! 0 |  962 | `				}` |
|      ! 0 |  963 | `			}else{` |
|        - |  964 | `				/* Save the instance associated with this operator for later processing */` |
|  3995151 |  965 | `				pToken->pUserData = (void *)pOp;` |
|        - |  966 | `			}` |
|  1994294 |  967 | `		}` |
|        - |  968 | `	}` |
|        - |  969 | `	/* Tell the upper-layer to save the extracted token for later processing */` |
| 25128966 |  970 | `	return SXRET_OK;` |
| 13614280 |  971 | `}` |
|        - |  972 | `/* SPDX-SnippetBegin */` |
|        - |  973 | `/* SPDX-SnippetCopyrightText: SQLite mkkeywordhash.c (D. Richard Hipp and the SQLite authors <https://sqlite.org/>); adapted for the PH7 engine by Chems mrad */` |
|        - |  974 | `/* SPDX-License-Identifier: blessing */` |
|        - |  975 | `/***** This file contains automatically generated code ******` |
|        - |  976 | `**` |
|        - |  977 | `** The code in this file has been automatically generated by` |
|        - |  978 | `**` |
|        - |  979 | `**     $Header: /sqlite/sqlite/tool/mkkeywordhash.c` |
|        - |  980 | `**` |
|        - |  981 | `** Sligthly modified by Chems mrad <chm@symisc.net> for the PH7 engine.` |
|        - |  982 | `**` |
|        - |  983 | `** The code in this file implements a function that determines whether` |
|        - |  984 | `** or not a given identifier is really a PHP keyword.  The same thing` |
|        - |  985 | `** might be implemented more directly using a hand-written hash table.` |
|        - |  986 | `** But by using this automatically generated code, the size of the code` |
|        - |  987 | `** is substantially reduced.  This is important for embedded applications` |
|        - |  988 | `** on platforms with limited memory.` |
|        - |  989 | `*/` |
|        - |  990 | `/* Hash score: 103 */` |
|  7070717 |  991 | `static sxu32 KeywordCode(const char *z, int n){` |
|        - |  992 | `  /* zText[] encodes 532 bytes of keywords in 333 bytes */` |
|        - |  993 | `  /*   extendswitchprintegerequire_oncenddeclareturnamespacechobject      */` |
|        - |  994 | `  /*   hrowbooleandefaultrycaselfinalistaticlonewconstringlobaluse        */` |
|        - |  995 | `  /*   lseifloatvarrayANDIEchoUSECHOabstractclasscontinuendifunction      */` |
|        - |  996 | `  /*   diendwhilevaldoexitgotoimplementsinclude_oncemptyinstanceof        */` |
|        - |  997 | `  /*   interfacendforeachissetparentprivateprotectedpublicatchunset       */` |
|        - |  998 | `  /*   xorARRAYASArrayEXITUNSETXORbreak                                   */` |
|        - |  999 | `  static const char zText[332] = {` |
|        - | 1000 | `    'e','x','t','e','n','d','s','w','i','t','c','h','p','r','i','n','t','e',` |
|        - | 1001 | `    'g','e','r','e','q','u','i','r','e','_','o','n','c','e','n','d','d','e',` |
|        - | 1002 | `    'c','l','a','r','e','t','u','r','n','a','m','e','s','p','a','c','e','c',` |
|        - | 1003 | `    'h','o','b','j','e','c','t','h','r','o','w','b','o','o','l','e','a','n',` |
|        - | 1004 | `    'd','e','f','a','u','l','t','r','y','c','a','s','e','l','f','i','n','a',` |
|        - | 1005 | `    'l','i','s','t','a','t','i','c','l','o','n','e','w','c','o','n','s','t',` |
|        - | 1006 | `    'r','i','n','g','l','o','b','a','l','u','s','e','l','s','e','i','f','l',` |
|        - | 1007 | `    'o','a','t','v','a','r','r','a','y','A','N','D','I','E','c','h','o','U',` |
|        - | 1008 | `    'S','E','C','H','O','a','b','s','t','r','a','c','t','c','l','a','s','s',` |
|        - | 1009 | `    'c','o','n','t','i','n','u','e','n','d','i','f','u','n','c','t','i','o',` |
|        - | 1010 | `    'n','d','i','e','n','d','w','h','i','l','e','v','a','l','d','o','e','x',` |
|        - | 1011 | `    'i','t','g','o','t','o','i','m','p','l','e','m','e','n','t','s','i','n',` |
|        - | 1012 | `    'c','l','u','d','e','_','o','n','c','e','m','p','t','y','i','n','s','t',` |
|        - | 1013 | `    'a','n','c','e','o','f','i','n','t','e','r','f','a','c','e','n','d','f',` |
|        - | 1014 | `    'o','r','e','a','c','h','i','s','s','e','t','p','a','r','e','n','t','p',` |
|        - | 1015 | `    'r','i','v','a','t','e','p','r','o','t','e','c','t','e','d','p','u','b',` |
|        - | 1016 | `    'l','i','c','a','t','c','h','u','n','s','e','t','x','o','r','A','R','R',` |
|        - | 1017 | `    'A','Y','A','S','A','r','r','a','y','E','X','I','T','U','N','S','E','T',` |
|        - | 1018 | `    'X','O','R','b','r','e','a','k'` |
|        - | 1019 | `  };` |
|        - | 1020 | `  static const unsigned char aHash[151] = {` |
|        - | 1021 | `       0,   0,   4,  83,   0,  61,  39,  12,   0,  33,  77,   0,  48,` |
|        - | 1022 | `       0,   2,  65,  67,   0,   0,   0,  47,   0,   0,  40,   0,  15,` |
|        - | 1023 | `      74,   0,  51,   0,  76,   0,   0,  20,   0,   0,   0,  50,   0,` |
|        - | 1024 | `      80,  34,   0,  36,   0,   0,  64,  16,   0,   0,  17,   0,   1,` |
|        - | 1025 | `      19,  84,  66,   0,  43,  45,  78,   0,   0,  53,  56,   0,   0,` |
|        - | 1026 | `       0,  23,  49,   0,   0,  13,  31,  54,   7,   0,   0,  25,   0,` |
|        - | 1027 | `      72,  14,   0,  71,   0,  38,   6,   0,   0,   0,  73,   0,   0,` |
|        - | 1028 | `       3,   0,  41,   5,  52,  57,  32,   0,  60,  63,   0,  69,  82,` |
|        - | 1029 | `      30,   0,  79,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,` |
|        - | 1030 | `       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  81,   0,   0,` |
|        - | 1031 | `      62,   0,  11,   0,   0,  58,   0,   0,   0,   0,  59,  75,   0,` |
|        - | 1032 | `       0,   0,   0,   0,   0,  35,  27,   0` |
|        - | 1033 | `  };` |
|        - | 1034 | `  static const unsigned char aNext[84] = {` |
|        - | 1035 | `       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,` |
|        - | 1036 | `       0,   0,   8,   0,   0,   0,  10,   0,   0,   0,   0,   0,   0,` |
|        - | 1037 | `       0,   0,   0,   0,  28,   0,   0,   0,   0,   0,   0,   0,   0,` |
|        - | 1038 | `       0,   0,   0,   0,   0,  44,   0,  18,   0,   0,   0,   0,   0,` |
|        - | 1039 | `       0,  46,   0,  29,   0,   0,   0,  22,   0,   0,   0,   0,  26,` |
|        - | 1040 | `       0,  21,  24,   0,   0,  68,   0,   0,   9,  37,   0,   0,   0,` |
|        - | 1041 | `      42,   0,   0,   0,  70,  55` |
|        - | 1042 | `  };` |
|        - | 1043 | `  static const unsigned char aLen[84] = {` |
|        - | 1044 | `       7,   9,   6,   5,   7,  12,   7,   2,  10,   7,   6,   9,   4,` |
|        - | 1045 | `       6,   5,   7,   4,   3,   7,   3,   4,   4,   5,   4,   6,   5,` |
|        - | 1046 | `       2,   3,   5,   6,   6,   3,   6,   4,   2,   5,   3,   5,   3,` |
|        - | 1047 | `       3,   4,   3,   4,   8,   5,   2,   8,   5,   8,   3,   8,   5,` |
|        - | 1048 | `       4,   2,   4,   4,  10,  12,   7,   5,  10,   9,   3,   6,  10,` |
|        - | 1049 | `       3,   7,   2,   5,   6,   7,   9,   6,   5,   5,   3,   5,   2,` |
|        - | 1050 | `       5,   4,   5,   3,   2,   5` |
|        - | 1051 | `  };` |
|        - | 1052 | `  static const sxu16 aOffset[84] = {` |
|        - | 1053 | `       0,   3,   6,  12,  14,  20,  20,  21,  31,  34,  39,  44,  52,` |
|        - | 1054 | `      55,  60,  65,  65,  70,  72,  78,  81,  83,  86,  90,  92,  97,` |
|        - | 1055 | `     100, 100, 103, 106, 111, 117, 119, 119, 123, 124, 129, 130, 135,` |
|        - | 1056 | `     137, 139, 143, 145, 149, 157, 159, 162, 169, 173, 181, 183, 186,` |
|        - | 1057 | `     190, 194, 196, 200, 204, 214, 214, 225, 230, 240, 240, 248, 248,` |
|        - | 1058 | `     251, 251, 252, 258, 263, 269, 276, 285, 290, 295, 300, 303, 308,` |
|        - | 1059 | `     310, 315, 319, 324, 325, 327` |
|        - | 1060 | `  };` |
|        - | 1061 | `  static const sxu32 aCode[84] = {` |
|        - | 1062 | `    PH7_TKWRD_EXTENDS,   PH7_TKWRD_ENDSWITCH,   PH7_TKWRD_SWITCH,    PH7_TKWRD_PRINT,   PH7_TKWRD_INT,` |
|        - | 1063 | `    PH7_TKWRD_REQONCE,   PH7_TKWRD_REQUIRE,     PH7_TK_ID /* 'eq' PH7-ism removed */, PH7_TKWRD_ENDDEC, PH7_TKWRD_DECLARE,` |
|        - | 1064 | `    PH7_TKWRD_RETURN,    PH7_TKWRD_NAMESPACE,   PH7_TKWRD_ECHO,      PH7_TKWRD_OBJECT,    PH7_TKWRD_THROW,` |
|        - | 1065 | `    PH7_TKWRD_BOOL,      PH7_TKWRD_BOOL,        PH7_TKWRD_AND,       PH7_TKWRD_DEFAULT,   PH7_TKWRD_TRY,` |
|        - | 1066 | `    PH7_TKWRD_CASE,      PH7_TKWRD_SELF,        PH7_TKWRD_FINAL,     PH7_TKWRD_LIST,      PH7_TKWRD_STATIC,` |
|        - | 1067 | `    PH7_TKWRD_CLONE,     PH7_TK_ID /* 'ne' PH7-ism removed */, PH7_TKWRD_NEW,  PH7_TKWRD_CONST,     PH7_TKWRD_STRING,` |
|        - | 1068 | `    PH7_TKWRD_GLOBAL,    PH7_TKWRD_USE,         PH7_TKWRD_ELIF,      PH7_TKWRD_ELSE,      PH7_TKWRD_IF,` |
|        - | 1069 | `    PH7_TKWRD_FLOAT,     PH7_TKWRD_VAR,         PH7_TKWRD_ARRAY,     PH7_TKWRD_AND,       PH7_TKWRD_DIE,` |
|        - | 1070 | `    PH7_TKWRD_ECHO,      PH7_TKWRD_USE,         PH7_TKWRD_ECHO,      PH7_TKWRD_ABSTRACT,  PH7_TKWRD_CLASS,` |
|        - | 1071 | `    PH7_TKWRD_AS,        PH7_TKWRD_CONTINUE,    PH7_TKWRD_ENDIF,     PH7_TKWRD_FUNCTION,  PH7_TKWRD_DIE,` |
|        - | 1072 | `    PH7_TKWRD_ENDWHILE,  PH7_TKWRD_WHILE,       PH7_TKWRD_EVAL,      PH7_TKWRD_DO,        PH7_TKWRD_EXIT,` |
|        - | 1073 | `    PH7_TKWRD_GOTO,      PH7_TKWRD_IMPLEMENTS,  PH7_TKWRD_INCONCE,   PH7_TKWRD_INCLUDE,   PH7_TKWRD_EMPTY,` |
|        - | 1074 | `    PH7_TKWRD_INSTANCEOF,PH7_TKWRD_INTERFACE,   PH7_TKWRD_INT,       PH7_TKWRD_ENDFOR,    PH7_TKWRD_END4EACH,` |
|        - | 1075 | `    PH7_TKWRD_FOR,       PH7_TKWRD_FOREACH,     PH7_TKWRD_OR,        PH7_TKWRD_ISSET,     PH7_TKWRD_PARENT,` |
|        - | 1076 | `    PH7_TKWRD_PRIVATE,   PH7_TKWRD_PROTECTED,   PH7_TKWRD_PUBLIC,    PH7_TKWRD_CATCH,     PH7_TKWRD_UNSET,` |
|        - | 1077 | `    PH7_TKWRD_XOR,       PH7_TKWRD_ARRAY,       PH7_TKWRD_AS,        PH7_TKWRD_ARRAY,     PH7_TKWRD_EXIT,` |
|        - | 1078 | `    PH7_TKWRD_UNSET,     PH7_TKWRD_XOR,         PH7_TKWRD_OR,        PH7_TKWRD_BREAK` |
|        - | 1079 | `  };` |
|        - | 1080 | `  int h, i;` |
|  7070717 | 1081 | `  if( n<2 ) return PH7_TK_ID;` |
|        - | 1082 | ``  /* Hash through UNSIGNED bytes: `char` is signed on most targets, so an`` |
|        - | 1083 | `   * identifier carrying a high byte (php allows 0x80-0xFF in identifiers, and` |
|        - | 1084 | ``   * every UTF-8 name has them) made the xor negative, and C's `%` keeps that`` |
|        - | 1085 | `   * sign — aHash[-46] read off the front of the table. ASCII is unaffected, so` |
|        - | 1086 | `   * the generated keyword buckets still resolve exactly as before. */` |
|  7070717 | 1087 | `  h = (int)(((sxu32)(sxu8)z[0]*4) ^ ((sxu32)(sxu8)z[n-1]*3) ^ (sxu32)n) % 151;` |
| 10349712 | 1088 | `  for(i=((int)aHash[h])-1; i>=0; i=((int)aNext[i])-1){` |
|  5996591 | 1089 | `    if( (int)aLen[i]==n && SyMemcmp(&zText[aOffset[i]],z,n)==0 ){` |
|        - | 1090 | `       /* PH7_TKWRD_EXTENDS */` |
|        - | 1091 | `       /* PH7_TKWRD_ENDSWITCH */` |
|        - | 1092 | `       /* PH7_TKWRD_SWITCH */` |
|        - | 1093 | `       /* PH7_TKWRD_PRINT */` |
|        - | 1094 | `       /* PH7_TKWRD_INT */` |
|        - | 1095 | `       /* PH7_TKWRD_REQONCE */` |
|        - | 1096 | `       /* PH7_TKWRD_REQUIRE */` |
|        - | 1097 | `       /* PH7_TK_ID */` |
|        - | 1098 | `       /* PH7_TKWRD_ENDDEC */` |
|        - | 1099 | `       /* PH7_TKWRD_DECLARE */` |
|        - | 1100 | `       /* PH7_TKWRD_RETURN */` |
|        - | 1101 | `       /* PH7_TKWRD_NAMESPACE */` |
|        - | 1102 | `       /* PH7_TKWRD_ECHO */` |
|        - | 1103 | `       /* PH7_TKWRD_OBJECT */` |
|        - | 1104 | `       /* PH7_TKWRD_THROW */` |
|        - | 1105 | `       /* PH7_TKWRD_BOOL */` |
|        - | 1106 | `       /* PH7_TKWRD_BOOL */` |
|        - | 1107 | `       /* PH7_TKWRD_AND */` |
|        - | 1108 | `       /* PH7_TKWRD_DEFAULT */` |
|        - | 1109 | `       /* PH7_TKWRD_TRY */` |
|        - | 1110 | `       /* PH7_TKWRD_CASE */` |
|        - | 1111 | `       /* PH7_TKWRD_SELF */` |
|        - | 1112 | `       /* PH7_TKWRD_FINAL */` |
|        - | 1113 | `       /* PH7_TKWRD_LIST */` |
|        - | 1114 | `       /* PH7_TKWRD_STATIC */` |
|        - | 1115 | `       /* PH7_TKWRD_CLONE */` |
|        - | 1116 | `       /* PH7_TK_ID */` |
|        - | 1117 | `       /* PH7_TKWRD_NEW */` |
|        - | 1118 | `       /* PH7_TKWRD_CONST */` |
|        - | 1119 | `       /* PH7_TKWRD_STRING */` |
|        - | 1120 | `       /* PH7_TKWRD_GLOBAL */` |
|        - | 1121 | `       /* PH7_TKWRD_USE */` |
|        - | 1122 | `       /* PH7_TKWRD_ELIF */` |
|        - | 1123 | `       /* PH7_TKWRD_ELSE */` |
|        - | 1124 | `       /* PH7_TKWRD_IF */` |
|        - | 1125 | `       /* PH7_TKWRD_FLOAT */` |
|        - | 1126 | `       /* PH7_TKWRD_VAR */` |
|        - | 1127 | `       /* PH7_TKWRD_ARRAY */` |
|        - | 1128 | `       /* PH7_TKWRD_AND */` |
|        - | 1129 | `       /* PH7_TKWRD_DIE */` |
|        - | 1130 | `       /* PH7_TKWRD_ECHO */` |
|        - | 1131 | `       /* PH7_TKWRD_USE */` |
|        - | 1132 | `       /* PH7_TKWRD_ECHO */` |
|        - | 1133 | `       /* PH7_TKWRD_ABSTRACT */` |
|        - | 1134 | `       /* PH7_TKWRD_CLASS */` |
|        - | 1135 | `       /* PH7_TKWRD_AS */` |
|        - | 1136 | `       /* PH7_TKWRD_CONTINUE */` |
|        - | 1137 | `       /* PH7_TKWRD_ENDIF */` |
|        - | 1138 | `       /* PH7_TKWRD_FUNCTION */` |
|        - | 1139 | `       /* PH7_TKWRD_DIE */` |
|        - | 1140 | `       /* PH7_TKWRD_ENDWHILE */` |
|        - | 1141 | `       /* PH7_TKWRD_WHILE */` |
|        - | 1142 | `       /* PH7_TKWRD_EVAL */` |
|        - | 1143 | `       /* PH7_TKWRD_DO */` |
|        - | 1144 | `       /* PH7_TKWRD_EXIT */` |
|        - | 1145 | `       /* PH7_TKWRD_GOTO */` |
|        - | 1146 | `       /* PH7_TKWRD_IMPLEMENTS */` |
|        - | 1147 | `       /* PH7_TKWRD_INCONCE */` |
|        - | 1148 | `       /* PH7_TKWRD_INCLUDE */` |
|        - | 1149 | `       /* PH7_TKWRD_EMPTY */` |
|        - | 1150 | `       /* PH7_TKWRD_INSTANCEOF */` |
|        - | 1151 | `       /* PH7_TKWRD_INTERFACE */` |
|        - | 1152 | `       /* PH7_TKWRD_INT */` |
|        - | 1153 | `       /* PH7_TKWRD_ENDFOR */` |
|        - | 1154 | `       /* PH7_TKWRD_END4EACH */` |
|        - | 1155 | `       /* PH7_TKWRD_FOR */` |
|        - | 1156 | `       /* PH7_TKWRD_FOREACH */` |
|        - | 1157 | `       /* PH7_TKWRD_OR */` |
|        - | 1158 | `       /* PH7_TKWRD_ISSET */` |
|        - | 1159 | `       /* PH7_TKWRD_PARENT */` |
|        - | 1160 | `       /* PH7_TKWRD_PRIVATE */` |
|        - | 1161 | `       /* PH7_TKWRD_PROTECTED */` |
|        - | 1162 | `       /* PH7_TKWRD_PUBLIC */` |
|        - | 1163 | `       /* PH7_TKWRD_CATCH */` |
|        - | 1164 | `       /* PH7_TKWRD_UNSET */` |
|        - | 1165 | `       /* PH7_TKWRD_XOR */` |
|        - | 1166 | `       /* PH7_TKWRD_ARRAY */` |
|        - | 1167 | `       /* PH7_TKWRD_AS */` |
|        - | 1168 | `       /* PH7_TKWRD_ARRAY */` |
|        - | 1169 | `       /* PH7_TKWRD_EXIT */` |
|        - | 1170 | `       /* PH7_TKWRD_UNSET */` |
|        - | 1171 | `       /* PH7_TKWRD_XOR */` |
|        - | 1172 | `       /* PH7_TKWRD_OR */` |
|        - | 1173 | `       /* PH7_TKWRD_BREAK */` |
|  2717596 | 1174 | `      return aCode[i];` |
|        - | 1175 | `    }` |
|  1636746 | 1176 | `  }` |
|        - | 1177 | `  /* Linear fallback for keywords not in the auto-generated hash table */` |
|  4353126 | 1178 | `  if( n==5 && SyMemcmp(z,"trait",5)==0 ) return PH7_TKWRD_TRAIT;` |
|  4352768 | 1179 | `  if( n==9 && SyMemcmp(z,"insteadof",9)==0 ) return PH7_TKWRD_INSTEADOF;` |
|  4352754 | 1180 | `  if( n==7 && SyMemcmp(z,"finally",7)==0 ) return PH7_TKWRD_FINALLY;` |
|  4352430 | 1181 | `  if( n==5 && SyMemcmp(z,"yield",5)==0 ) return PH7_TKWRD_YIELD;` |
|  4351780 | 1182 | `  if( n==5 && SyMemcmp(z,"match",5)==0 ) return PH7_TKWRD_MATCH;` |
|  4351576 | 1183 | `  if( n==2 && SyMemcmp(z,"fn",2)==0 ) return PH7_TKWRD_FN;   /* PHP 7.4 arrow functions */` |
|  4341800 | 1184 | `  return PH7_TK_ID;` |
|  3529854 | 1185 | `}` |
|        - | 1186 | `/* --- End of Automatically generated code --- */` |
|        - | 1187 | `/* SPDX-SnippetEnd */` |
|        - | 1188 | `/*` |
|        - | 1189 | ` * Keyword lookup as php does it: CASE-INSENSITIVELY. 'IF', 'Function' and 'NEW'` |
|        - | 1190 | ` * are the very same tokens as 'if', 'function' and 'new', so the generated table` |
|        - | 1191 | ` * above — whose hash buckets and SyMemcmp() rows are byte-exact lower case — is` |
|        - | 1192 | ` * probed through an ASCII-folded COPY of the identifier. KeywordCode() itself` |
|        - | 1193 | ` * stays byte-exact so the generated code needs no regeneration.` |
|        - | 1194 | ` *` |
|        - | 1195 | ` * The fold is ASCII-only on purpose: libc tolower() follows LC_CTYPE (a tr_TR` |
|        - | 1196 | ` * embedder would stop recognising 'IF') where php's lexer is locale-independent,` |
|        - | 1197 | ` * and identifier bytes >= 0x80 — php allows them, and every UTF-8 name has` |
|        - | 1198 | ` * them — must pass through untouched.` |
|        - | 1199 | ` *` |
|        - | 1200 | ` * A handful of UPPER-case rows in the table ('ARRAY', 'AS', 'EXIT', 'UNSET',` |
|        - | 1201 | ` * 'XOR', 'AND', 'OR', 'ECHO', 'Echo', 'Array', 'USE') are PH7's old partial hack` |
|        - | 1202 | ` * for this same problem. Each has a lower-case twin, so folding makes them` |
|        - | 1203 | ` * unreachable-but-harmless rather than wrong.` |
|        - | 1204 | ` */` |
|  8146763 | 1205 | `static sxu32 KeywordCodeCI(const char *zRaw, int n)` |
|        5 | 1206 | `{` |
|        - | 1207 | `	/* Longest row in the table above: 'require_once'/'include_once' (12 bytes) */` |
|        - | 1208 | `	char zFold[12];` |
|        - | 1209 | `	int i;` |
|  8146768 | 1210 | `	if( n < 2 \|\| n > (int)sizeof(zFold) ){` |
|        - | 1211 | `		/* Too short or too long to be any keyword: skip the fold and the probe */` |
|  1076056 | 1212 | `		return PH7_TK_ID;` |
|        - | 1213 | `	}` |
| 45014847 | 1214 | `	for( i = 0 ; i < n ; ++i ){` |
| 37944135 | 1215 | `		unsigned char c = (unsigned char)zRaw[i];` |
| 37944135 | 1216 | `		zFold[i] = (char)((c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c);` |
| 18942509 | 1217 | `	}` |
|  7070717 | 1218 | `	return KeywordCode(zFold,n);` |
|  4066858 | 1219 | `}` |
|        - | 1220 | `/*` |
|        - | 1221 | ` * Extract a heredoc/nowdoc text from a raw PHP input.` |
|        - | 1222 | ` * According to the PHP language reference manual:` |
|        - | 1223 | ` *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier` |
|        - | 1224 | ` *  is provided, then a newline. The string itself follows, and then the same identifier again` |
|        - | 1225 | ` *  to close the quotation.` |
|        - | 1226 | ` *  The closing identifier must begin in the first column of the line. Also, the identifier must` |
|        - | 1227 | ` *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric` |
|        - | 1228 | ` *  characters and underscores, and must start with a non-digit character or underscore.` |
|        - | 1229 | ` *  Heredoc text behaves just like a double-quoted string, without the double quotes.` |
|        - | 1230 | ` *  This means that quotes in a heredoc do not need to be escaped, but the escape codes listed` |
|        - | 1231 | ` *  above can still be used. Variables are expanded, but the same care must be taken when expressing` |
|        - | 1232 | ` *  complex variables inside a heredoc as with strings.` |
|        - | 1233 | ` *  Nowdocs are to single-quoted strings what heredocs are to double-quoted strings.` |
|        - | 1234 | ` *  A nowdoc is specified similarly to a heredoc, but no parsing is done inside a nowdoc.` |
|        - | 1235 | ` *  The construct is ideal for embedding PHP code or other large blocks of text without the need` |
|        - | 1236 | ` *  for escaping. It shares some features in common with the SGML <![CDATA[ ]]> construct, in that` |
|        - | 1237 | ` *  it declares a block of text which is not for parsing.` |
|        - | 1238 | ` *  A nowdoc is identified with the same <<< sequence used for heredocs, but the identifier which follows` |
|        - | 1239 | ` *  is enclosed in single quotes, e.g. <<<'EOT'. All the rules for heredoc identifiers also apply to nowdoc` |
|        - | 1240 | ` *  identifiers, especially those regarding the appearance of the closing identifier.` |
|        - | 1241 | ` * Symisc Extension:` |
|        - | 1242 | ` * The closing delimiter can now start with a digit or undersocre or it can be an UTF-8 stream.` |
|        - | 1243 | ` * Example:` |
|        - | 1244 | ` *  <<<123` |
|        - | 1245 | ` *    HEREDOC Here` |
|        - | 1246 | ` * 123` |
|        - | 1247 | ` *  or` |
|        - | 1248 | ` *  <<<___` |
|        - | 1249 | ` *   HEREDOC Here` |
|        - | 1250 | ` *  ___` |
|        - | 1251 | ` */` |
|      160 | 1252 | `static sxi32 LexExtractHeredoc(SyStream *pStream,SyToken *pToken)` |
|        4 | 1253 | `{` |
|      164 | 1254 | `	const unsigned char *zIn  = pStream->zText;` |
|      164 | 1255 | `	const unsigned char *zEnd = pStream->zEnd;` |
|        - | 1256 | `	const unsigned char *zPtr;` |
|      164 | 1257 | `	sxu8 bNowDoc = FALSE;` |
|      164 | 1258 | `	sxu8 bUnterm = FALSE;` |
|        - | 1259 | `	SyString sDelim;` |
|        - | 1260 | `	SyString sStr;` |
|        - | 1261 | `	/* Jump leading white spaces */` |
|      182 | 1262 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|       20 | 1263 | `		zIn++;` |
|        2 | 1264 | `	}` |
|      164 | 1265 | `	if( zIn >= zEnd ){` |
|        - | 1266 | `		/* A simple symbol,return immediately */` |
|      ! 0 | 1267 | `		return SXERR_CONTINUE;` |
|        - | 1268 | `	}` |
|      164 | 1269 | `	if( zIn[0] == '\'' \|\| zIn[0] == '"' ){` |
|        - | 1270 | `		/* Make sure we are dealing with a nowdoc */` |
|       70 | 1271 | `		bNowDoc =  zIn[0] == '\'' ? TRUE : FALSE;` |
|       70 | 1272 | `		zIn++;` |
|       33 | 1273 | `	}` |
|      164 | 1274 | `	if( !LEX_LABEL_BYTE(zIn[0]) ){` |
|        - | 1275 | `		/* Invalid delimiter,return immediately */` |
|      ! 0 | 1276 | `		return SXERR_CONTINUE;` |
|        - | 1277 | `	}` |
|        - | 1278 | `	/* Isolate the identifier (php's label bytes; see LEX_LABEL_START) */` |
|      164 | 1279 | `	sDelim.zString = (const char *)zIn;` |
|      164 | 1280 | `	zPtr = zIn;` |
|      836 | 1281 | `	while( zPtr < zEnd && LEX_LABEL_BYTE(zPtr[0]) ){` |
|      596 | 1282 | `		zPtr++;` |
|        4 | 1283 | `	}` |
|      164 | 1284 | `	zIn = zPtr;` |
|        - | 1285 | `	/* Get the identifier length */` |
|      164 | 1286 | `	sDelim.nByte = (sxu32)((const char *)zIn-sDelim.zString);` |
|      164 | 1287 | `	if( zIn[0] == '"' \|\| (bNowDoc && zIn[0] == '\'') ){` |
|        - | 1288 | `		/* Jump the trailing single quote */` |
|       70 | 1289 | `		zIn++;` |
|       33 | 1290 | `	}` |
|        - | 1291 | `	/* Jump trailing white spaces */` |
|      164 | 1292 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|      ! 0 | 1293 | `		zIn++;` |
|      ! 0 | 1294 | `	}` |
|      164 | 1295 | `	if( sDelim.nByte > 0 && zIn >= zEnd ){` |
|        - | 1296 | ``		/* `<<<EOT` and then the end of the input: php has a heredoc with nothing in`` |
|        - | 1297 | `		 * it and no closing marker, and refuses the file for that. Form the token so` |
|        - | 1298 | ``		 * the compile phase can say so; without this the `<<<` fell through to the`` |
|        - | 1299 | `		 * operator table and named itself instead. */` |
|        2 | 1300 | `		SyStringInitFromBuf(&sStr,(const char *)zIn,0);` |
|        2 | 1301 | `		pStream->zText = zEnd;` |
|        2 | 1302 | `		pToken->nType = (bNowDoc ? PH7_TK_NOWDOC : PH7_TK_HEREDOC)\|PH7_TK_UNTERM;` |
|        2 | 1303 | `		SyStringDupPtr(&pToken->sData,&sStr);` |
|        2 | 1304 | `		pToken->pUserData = SX_INT_TO_PTR(0);` |
|        2 | 1305 | `		return SXRET_OK;` |
|        - | 1306 | `	}` |
|      162 | 1307 | `	if( sDelim.nByte <= 0 \|\| zIn >= zEnd \|\| zIn[0] != '\n' ){` |
|        - | 1308 | `		/* Invalid syntax */` |
|      ! 0 | 1309 | `		return SXERR_CONTINUE;` |
|        - | 1310 | `	}` |
|      162 | 1311 | `	pStream->nLine++; /* Increment line counter */` |
|      162 | 1312 | `	zIn++;` |
|        - | 1313 | `	/* Isolate the delimited string */` |
|      162 | 1314 | `	sStr.zString = (const char *)zIn;` |
|        - | 1315 | `	/* PHP 7.3 flexible heredoc/nowdoc: the closing marker may be preceded` |
|        - | 1316 | `	 * by whitespace (spaces/tabs), and may be followed by any non-identifier` |
|        - | 1317 | `	 * character. The indent count is recorded in pToken->pUserData and the` |
|        - | 1318 | `	 * compile phase strips it from each body line. */` |
|        - | 1319 | `	{` |
|      162 | 1320 | `		const unsigned char *zMarkerLine = zIn; /* Start of marker's line (set on match) */` |
|      162 | 1321 | `		sxu32 nIndent = 0;` |
|      407 | 1322 | `		for(;;){` |
|      490 | 1323 | `			const unsigned char *zLineStart = zIn;` |
|        - | 1324 | `			/* Skip leading space/tab on this line */` |
|     1287 | 1325 | `			while( zIn < zEnd && (zIn[0] == ' ' \|\| zIn[0] == '\t') ){` |
|      558 | 1326 | `				zIn++;` |
|        4 | 1327 | `			}` |
|      486 | 1328 | `			if( (sxu32)(zEnd - zIn) >= sDelim.nByte` |
|      487 | 1329 | `				&& SyMemcmp((const void *)sDelim.zString,(const void *)zIn,sDelim.nByte) == 0 ){` |
|        - | 1330 | `				int bIdentCont;` |
|      154 | 1331 | `				zPtr = &zIn[sDelim.nByte];` |
|        - | 1332 | `				/* Disambiguate: the next byte must not continue an identifier` |
|        - | 1333 | `				 * (php's label bytes; see LEX_LABEL_START). */` |
|      229 | 1334 | `				bIdentCont = zPtr < zEnd && LEX_LABEL_BYTE(zPtr[0]);` |
|      154 | 1335 | `				if( !bIdentCont ){` |
|        - | 1336 | `					/* Closing marker found */` |
|      154 | 1337 | `					nIndent = (sxu32)(zIn - zLineStart);` |
|      154 | 1338 | `					zMarkerLine = zLineStart;` |
|      154 | 1339 | `					pStream->zText = zPtr; /* Cursor right after identifier */` |
|      154 | 1340 | `					break;` |
|        - | 1341 | `				}` |
|      ! 0 | 1342 | `			}` |
|        - | 1343 | `			/* Not the closing marker on this line; walk to next newline */` |
|     7748 | 1344 | `			while( zIn < zEnd && zIn[0] != '\n' ){` |
|     7412 | 1345 | `				zIn++;` |
|        4 | 1346 | `			}` |
|      340 | 1347 | `			if( zIn >= zEnd ){` |
|        - | 1348 | `				/* End of input without finding the closing marker: php refuses the` |
|        - | 1349 | `				 * file, where this took the rest of it as the body. */` |
|        9 | 1350 | `				pStream->zText = pStream->zEnd;` |
|        9 | 1351 | `				zMarkerLine = zIn;` |
|        9 | 1352 | `				bUnterm = TRUE;` |
|        9 | 1353 | `				break;` |
|        - | 1354 | `			}` |
|      332 | 1355 | `			pStream->nLine++;` |
|      332 | 1356 | `			zIn++;` |
|        4 | 1357 | `		}` |
|        - | 1358 | `		/* Body runs from sStr.zString up to just before the marker line */` |
|      162 | 1359 | `		sStr.nByte = (sxu32)((const char *)zMarkerLine - sStr.zString);` |
|      162 | 1360 | `		pToken->nType = bNowDoc ? PH7_TK_NOWDOC : PH7_TK_HEREDOC;` |
|      162 | 1361 | `		if( bUnterm ){` |
|        9 | 1362 | `			pToken->nType \|= PH7_TK_UNTERM;` |
|        4 | 1363 | `		}` |
|      162 | 1364 | `		SyStringDupPtr(&pToken->sData,&sStr);` |
|        - | 1365 | `		/* Strip exactly one line terminator that precedes the marker's line. */` |
|      158 | 1366 | `		if( pToken->sData.nByte > 0` |
|      158 | 1367 | `			&& pToken->sData.zString[pToken->sData.nByte - 1] == '\n' ){` |
|      146 | 1368 | `			pToken->sData.nByte--;` |
|      142 | 1369 | `			if( pToken->sData.nByte > 0` |
|      146 | 1370 | `				&& pToken->sData.zString[pToken->sData.nByte - 1] == '\r' ){` |
|      ! 0 | 1371 | `				pToken->sData.nByte--;` |
|      ! 0 | 1372 | `			}` |
|       71 | 1373 | `		}` |
|      162 | 1374 | `		pToken->pUserData = SX_INT_TO_PTR(nIndent);` |
|        - | 1375 | `	}` |
|        - | 1376 | `	/* All done */` |
|      162 | 1377 | `	return SXRET_OK;` |
|       84 | 1378 | `}` |
|        - | 1379 | `/*` |
|        - | 1380 | ` * Tokenize a raw PHP input.` |
|        - | 1381 | ` * This is the public tokenizer called by most code generator routines.` |
|        - | 1382 | ` */` |
|    40624 | 1383 | `PH7_PRIVATE sxi32 PH7_TokenizePHP(const char *zInput,sxu32 nLen,sxu32 nLineStart,SySet *pOut,SySet *pTrivia)` |
|        5 | 1384 | `{` |
|        - | 1385 | `	SyLex sLexer;` |
|        - | 1386 | `	sxi32 rc;` |
|        - | 1387 | `	/* Defense-in-depth cap for internal tokenizer calls that bypass ph7_compile() */` |
|    40629 | 1388 | `	if( nLen > PH7_MAX_INPUT_SIZE ){` |
|      ! 0 | 1389 | `		return SXERR_LIMIT;` |
|        - | 1390 | `	}` |
|        - | 1391 | `	/* Initialize the lexer. pTrivia (may be NULL = discard) rides as the` |
|        - | 1392 | `	 * tokenizer callback's user data: doc-comments (and later attribute` |
|        - | 1393 | `	 * groups) are recorded there instead of entering the token stream. */` |
|    40629 | 1394 | `	rc = SyLexInit(&sLexer,&(*pOut),TokenizePHP,pTrivia);` |
|    40629 | 1395 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1396 | `		return rc;` |
|        - | 1397 | `	}` |
|    40629 | 1398 | `	sLexer.sStream.nLine = nLineStart;` |
|        - | 1399 | `	/* Tokenize input */` |
|    40629 | 1400 | `	rc = SyLexTokenizeInput(&sLexer,zInput,nLen,0,0,0);` |
|        - | 1401 | `	/* Release the lexer */` |
|    40629 | 1402 | `	SyLexRelease(&sLexer);` |
|        - | 1403 | `	/* Tokenization result */` |
|    40629 | 1404 | `	return rc;` |
|    20288 | 1405 | `}` |
|        - | 1406 | `/*` |
|        - | 1407 | ` * High level public tokenizer.` |
|        - | 1408 | ` *  Tokenize the input into PHP tokens and raw tokens [i.e: HTML,XML,Raw text...].` |
|        - | 1409 | ` * According to the PHP language reference manual` |
|        - | 1410 | ` *   When PHP parses a file, it looks for opening and closing tags, which tell PHP` |
|        - | 1411 | ` *   to start and stop interpreting the code between them. Parsing in this manner allows` |
|        - | 1412 | ` *   PHP to be embedded in all sorts of different documents, as everything outside of a pair` |
|        - | 1413 | ` *   of opening and closing tags is ignored by the PHP parser. Most of the time you will see` |
|        - | 1414 | ` *   PHP embedded in HTML documents, as in this example.` |
|        - | 1415 | ` *   <?php echo 'While this is going to be parsed.'; ?>` |
|        - | 1416 | ` *   <p>This will also be ignored.</p>` |
|        - | 1417 | ` *   You can also use more advanced structures:` |
|        - | 1418 | ` *   Example #1 Advanced escaping` |
|        - | 1419 | ` * <?php` |
|        - | 1420 | ` * if ($expression) {` |
|        - | 1421 | ` *   ?>` |
|        - | 1422 | ` *   <strong>This is true.</strong>` |
|        - | 1423 | ` *   <?php` |
|        - | 1424 | ` * } else {` |
|        - | 1425 | ` *   ?>` |
|        - | 1426 | ` *   <strong>This is false.</strong>` |
|        - | 1427 | ` *   <?php` |
|        - | 1428 | ` * }` |
|        - | 1429 | ` * ?>` |
|        - | 1430 | ` * This works as expected, because when PHP hits the ?> closing tags, it simply starts outputting` |
|        - | 1431 | ` * whatever it finds (except for an immediately following newline - see instruction separation ) until it hits` |
|        - | 1432 | ` * another opening tag. The example given here is contrived, of course, but for outputting large blocks of text` |
|        - | 1433 | ` * dropping out of PHP parsing mode is generally more efficient than sending all of the text through echo() or print().` |
|        - | 1434 | ` * There are four different pairs of opening and closing tags which can be used in PHP. Three of those, <?php ?>` |
|        - | 1435 | ` * <script language="php"> </script>  and <? ?> are always available. The other two are short tags and ASP style` |
|        - | 1436 | ` * tags, and can be turned on and off from the php.ini configuration file. As such, while some people find short tags` |
|        - | 1437 | ` * and ASP style tags convenient, they are less portable, and generally not recommended.` |
|        - | 1438 | ` * Note:` |
|        - | 1439 | ` * Also note that if you are embedding PHP within XML or XHTML you will need to use the <?php ?> tags to remain` |
|        - | 1440 | ` * compliant with standards.` |
|        - | 1441 | ` * Example #2 PHP Opening and Closing Tags` |
|        - | 1442 | ` * 1.  <?php echo 'if you want to serve XHTML or XML documents, do it like this'; ?>` |
|        - | 1443 | ` * 2.  <script language="php">` |
|        - | 1444 | ` *       echo 'some editors (like FrontPage) don\'t` |
|        - | 1445 | ` *             like processing instructions';` |
|        - | 1446 | ` *   </script>` |
|        - | 1447 | ` *` |
|        - | 1448 | ` * 3.  <? echo 'this is the simplest, an SGML processing instruction'; ?>` |
|        - | 1449 | ` *   <?= expression ?> This is a shortcut for "<? echo expression ?>"` |
|        - | 1450 | ` */` |
|    29499 | 1451 | `PH7_PRIVATE sxi32 PH7_TokenizeRawText(const char *zInput,sxu32 nLen,SySet *pOut,sxu32 nBaseLine)` |
|        5 | 1452 | `{` |
|    29504 | 1453 | `	const char *zEnd = &zInput[nLen];` |
|    29504 | 1454 | `	const char *zIn  = zInput;` |
|        - | 1455 | `	const char *zCur,*zCurEnd;` |
|    29504 | 1456 | `	SyString sCtag = { 0, 0 };     /* Closing tag */` |
|        - | 1457 | `	SyToken sToken;` |
|        - | 1458 | `	SyString sDoc;` |
|        - | 1459 | `	sxu32 nLine;` |
|        - | 1460 | `	sxi32 iNest;` |
|        - | 1461 | `	sxi32 rc;` |
|        - | 1462 | `	/* Tokenize the input into PHP tokens and raw tokens. nBaseLine is normally 1,` |
|        - | 1463 | `	 * but 2 when a "#!" shebang line was stripped so error lines still match php. */` |
|    29504 | 1464 | `	nLine = nBaseLine;` |
|    29504 | 1465 | `	zCur = zCurEnd   = 0; /* Prevent compiler warning */` |
|    29504 | 1466 | `	sToken.pUserData = 0;` |
|    29504 | 1467 | `	iNest = 0;` |
|    29504 | 1468 | `	sDoc.nByte = 0;` |
|    29504 | 1469 | `	sDoc.zString = ""; /* cc warning */` |
|    29490 | 1470 | `	for(;;){` |
|    58339 | 1471 | `		if( zIn >= zEnd ){` |
|        - | 1472 | `			/* End of input reached */` |
|    28784 | 1473 | `			break;` |
|        - | 1474 | `		}` |
|    29560 | 1475 | `		sToken.nLine = nLine;` |
|    29560 | 1476 | `		zCur = zIn;` |
|    29560 | 1477 | `		zCurEnd = 0;` |
|    31910 | 1478 | `		while( zIn < zEnd ){` |
|    31190 | 1479 | `			 if( zIn[0] == '<' ){` |
|    28840 | 1480 | `				const char *zTmp = zIn; /* End of raw input marker */` |
|    28840 | 1481 | `				zIn++;` |
|    28840 | 1482 | `				if( zIn < zEnd ){` |
|    28840 | 1483 | `					if( zIn[0] == '?' ){` |
|    28840 | 1484 | `						zIn++;` |
|    28840 | 1485 | `						if( (sxu32)(zEnd - zIn) >= sizeof("php")-1 &&  SyStrnicmp(zIn,"php",sizeof("php")-1) == 0 ){` |
|        - | 1486 | `							/* opening tag: <?php */` |
|    28838 | 1487 | `							zIn += sizeof("php")-1;` |
|    14411 | 1488 | `						}` |
|        - | 1489 | `						/* Look for the closing tag '?>' */` |
|    28840 | 1490 | `						SyStringInitFromBuf(&sCtag,"?>",sizeof("?>")-1);` |
|    28840 | 1491 | `						zCurEnd = zTmp;` |
|    28840 | 1492 | `						break;` |
|        - | 1493 | `					}` |
|      ! 0 | 1494 | `				}` |
|      ! 0 | 1495 | `			}else{` |
|     2355 | 1496 | `				if( zIn[0] == '\n' ){` |
|       21 | 1497 | `					nLine++;` |
|        9 | 1498 | `				}` |
|     2355 | 1499 | `				zIn++;` |
|        - | 1500 | `			 }` |
|        5 | 1501 | `		} /* While(zIn < zEnd) */` |
|    29560 | 1502 | `		if( zCurEnd == 0 ){` |
|       57 | 1503 | `			zCurEnd = zIn;` |
|       26 | 1504 | `		}` |
|        - | 1505 | `		/* Save the raw token */` |
|    29560 | 1506 | `		SyStringInitFromBuf(&sToken.sData,zCur,zCurEnd - zCur);` |
|    29560 | 1507 | `		sToken.nType = PH7_TOKEN_RAW;` |
|    29560 | 1508 | `		rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|    29560 | 1509 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1510 | `			return rc;` |
|        - | 1511 | `		}` |
|    29560 | 1512 | `		if( zIn >= zEnd ){` |
|       57 | 1513 | `			break;` |
|        - | 1514 | `		}` |
|        - | 1515 | `		/* Ignore leading white space */` |
|    59923 | 1516 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    30420 | 1517 | `			if( zIn[0] == '\n' ){` |
|    20019 | 1518 | `				nLine++;` |
|    10002 | 1519 | `			}` |
|    30420 | 1520 | `			zIn++;` |
|        5 | 1521 | `		}` |
|        - | 1522 | `		/* Delimit the PHP chunk */` |
|    29508 | 1523 | `		sToken.nLine = nLine;` |
|    29508 | 1524 | `		zCur = zIn;` |
|  5799299 | 1525 | `		while( (sxu32)(zEnd - zIn) >= sCtag.nByte ){` |
|        - | 1526 | `			const char *zPtr;` |
|  5780085 | 1527 | `			if( SyMemcmp(zIn,sCtag.zString,sCtag.nByte) == 0 && iNest < 1 ){` |
|     9626 | 1528 | `				break;` |
|        - | 1529 | `			}` |
|        - | 1530 | `			/* Line comment ('#' or '//', but not the '#[' attribute opener): php` |
|        - | 1531 | `			 * ends it at a newline OR at the closing tag, so a '?>' inside a line` |
|        - | 1532 | `			 * comment DOES close the PHP block. Skipping the comment here also` |
|        - | 1533 | `			 * stops the string skip below from treating a quote inside the` |
|        - | 1534 | `			 * comment as a string. Only outside a heredoc body (iNest < 1). */` |
|  5781781 | 1535 | `			if( iNest < 1 &&` |
|  5763127 | 1536 | `				( (zIn[0] == '#' && !(zIn+1 < zEnd && zIn[1] == '[')) \|\|` |
|  5765625 | 1537 | `				  (zIn[0] == '/' && zIn+1 < zEnd && zIn[1] == '/') ) ){` |
|    22639 | 1538 | `				zIn += (zIn[0] == '#') ? 1 : 2;` |
|   882507 | 1539 | `				while( zIn < zEnd && zIn[0] != '\n' ){` |
|   859872 | 1540 | `					if( (sxu32)(zEnd - zIn) >= sCtag.nByte` |
|   859874 | 1541 | `						&& SyMemcmp(zIn,sCtag.zString,sCtag.nByte) == 0 ){` |
|        6 | 1542 | `						break; /* the closing tag terminates the line comment */` |
|        - | 1543 | `					}` |
|   859873 | 1544 | `					zIn++;` |
|        5 | 1545 | `				}` |
|    16301 | 1546 | `				continue;` |
|        - | 1547 | `			}` |
|        - | 1548 | `			/* Block comment: spans everything, including '?>', up to its close. */` |
|  5753500 | 1549 | `			if( iNest < 1 && zIn[0] == '/' && zIn+1 < zEnd && zIn[1] == '*' ){` |
|     5425 | 1550 | `				zIn += 2;` |
|   851465 | 1551 | `				while( (sxu32)(zEnd-zIn) >= sizeof("*/") - 1 ){` |
|   851461 | 1552 | `					if( zIn[0] == '*' && zIn[1] == '/' ){` |
|     5421 | 1553 | `						zIn += 2;` |
|     5421 | 1554 | `						break;` |
|        - | 1555 | `					}` |
|   846045 | 1556 | `					if( zIn[0] == '\n' ){` |
|     7950 | 1557 | `						nLine++;` |
|     3950 | 1558 | `					}` |
|   846045 | 1559 | `					zIn++;` |
|        5 | 1560 | `				}` |
|     5425 | 1561 | `				continue;` |
|        - | 1562 | `			}` |
|        - | 1563 | `			/* Skip over a single/double-quoted or backtick string literal so a` |
|        - | 1564 | `			 * '?>' sequence inside it is not mistaken for the closing tag. Only` |
|        - | 1565 | `			 * outside a heredoc body (iNest < 1); heredocs are delimited by the` |
|        - | 1566 | `			 * label-matching logic above. Escapes (\" \' \\ and a line-continuing` |
|        - | 1567 | `			 * backslash-newline) are honoured. Same-quote nesting inside "{$...}"` |
|        - | 1568 | `			 * interpolation is not tracked, but that can only end the skip early` |
|        - | 1569 | `			 * on a string that has no '?>' anyway, which stays a PHP chunk either` |
|        - | 1570 | `			 * way — it never mis-splits code that works today. */` |
|  5748080 | 1571 | ``			if( iNest < 1 && (zIn[0] == '\'' \|\| zIn[0] == '"' \|\| zIn[0] == '`') ){`` |
|   159247 | 1572 | `				int qch = zIn[0];` |
|   159247 | 1573 | `				zIn++;` |
|  1501621 | 1574 | `				while( zIn < zEnd ){` |
|  1501612 | 1575 | `					if( zIn[0] == '\\' && zIn + 1 < zEnd ){` |
|    46677 | 1576 | `						if( zIn[1] == '\n' ){ nLine++; }` |
|    46677 | 1577 | `						zIn += 2;` |
|    46677 | 1578 | `						continue;` |
|        - | 1579 | `					}` |
|  1454940 | 1580 | `					if( zIn[0] == qch ){ zIn++; break; }` |
|  1295707 | 1581 | `					if( zIn[0] == '\n' ){ nLine++; }` |
|  1295707 | 1582 | `					zIn++;` |
|        5 | 1583 | `				}` |
|   159247 | 1584 | `				continue;` |
|        - | 1585 | `			}` |
|  5588838 | 1586 | `			if( zIn[0] == '\n' ){` |
|   198422 | 1587 | `				nLine++;` |
|   198422 | 1588 | `				if( iNest > 0 ){` |
|      490 | 1589 | `					zIn++;` |
|     1044 | 1590 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|      558 | 1591 | `						zIn++;` |
|        4 | 1592 | `					}` |
|      490 | 1593 | `					zPtr = zIn;` |
|     2587 | 1594 | `					while( zIn < zEnd && LEX_LABEL_BYTE(zIn[0]) ){` |
|     1858 | 1595 | `						zIn++;` |
|        4 | 1596 | `					}` |
|      490 | 1597 | `					if( (sxu32)(zIn - zPtr) == sDoc.nByte && SyMemcmp(sDoc.zString,zPtr,sDoc.nByte) == 0 ){` |
|      154 | 1598 | `						iNest = 0;` |
|       75 | 1599 | `					}` |
|      490 | 1600 | `					continue;` |
|        5 | 1601 | `				}` |
|  5488990 | 1602 | `			}else if ( (sxu32)(zEnd - zIn) >= sizeof("<<<") && zIn[0] == '<' && zIn[1] == '<' && zIn[2] == '<' && iNest < 1){` |
|      164 | 1603 | `				zIn += sizeof("<<<")-1;` |
|      182 | 1604 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|       20 | 1605 | `					zIn++;` |
|        2 | 1606 | `				}` |
|      164 | 1607 | `				if( zIn[0] == '"' \|\| zIn[0] == '\'' ){` |
|       70 | 1608 | `					zIn++;` |
|       33 | 1609 | `				}` |
|      164 | 1610 | `				zPtr = zIn;` |
|      836 | 1611 | `				while( zIn < zEnd && LEX_LABEL_BYTE(zIn[0]) ){` |
|      596 | 1612 | `					zIn++;` |
|        4 | 1613 | `				}` |
|      164 | 1614 | `				SyStringInitFromBuf(&sDoc,zPtr,zIn-zPtr);` |
|      164 | 1615 | `				SyStringFullTrim(&sDoc);` |
|      164 | 1616 | `				if( sDoc.nByte > 0 ){` |
|      164 | 1617 | `					iNest++;` |
|       80 | 1618 | `				}` |
|      164 | 1619 | `				continue;` |
|        - | 1620 | `			}` |
|  5588192 | 1621 | `			zIn++;` |
|        - | 1622 |  |
|  5588192 | 1623 | `			if ( zIn >= zEnd )` |
|      ! 0 | 1624 | `				break;` |
|        5 | 1625 | `		}` |
|    28840 | 1626 | `		if( (sxu32)(zEnd - zIn) < sCtag.nByte ){` |
|    19219 | 1627 | `			zIn = zEnd;` |
|     9605 | 1628 | `		}` |
|    28840 | 1629 | `		if( zCur < zIn ){` |
|        - | 1630 | `			/* Save the PHP chunk for later processing. pUserData records whether the` |
|        - | 1631 | ``			 * chunk was CLOSED by a `?>`: php reads that tag as the terminator of`` |
|        - | 1632 | ``			 * whatever statement was open (`<?php echo 1 ?>` is legal), and only a`` |
|        - | 1633 | `			 * chunk that ran into the end of the FILE leaves one unfinished. */` |
|    24798 | 1634 | `			sToken.nType = PH7_TOKEN_PHP;` |
|    24798 | 1635 | `			sToken.pUserData = SX_INT_TO_PTR(zIn < zEnd ? 1 : 0);` |
|    24798 | 1636 | `			SyStringInitFromBuf(&sToken.sData,zCur,zIn-zCur);` |
|    38510 | 1637 | `			SyStringRightTrim(&sToken.sData); /* Trim trailing white spaces */` |
|    24798 | 1638 | `			rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|    24798 | 1639 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1640 | `				return rc;` |
|        - | 1641 | `			}` |
|    12391 | 1642 | `		}` |
|    28840 | 1643 | `		if( zIn < zEnd ){` |
|        - | 1644 | `			/* Jump the trailing closing tag */` |
|     9626 | 1645 | `			zIn += sCtag.nByte;` |
|        - | 1646 | `			/* php's lexer swallows exactly ONE newline immediately after the` |
|        - | 1647 | `			 * closing tag ("?>\n" emits nothing) */` |
|     9626 | 1648 | `			if( zIn < zEnd && zIn[0] == '\r' && zIn + 1 < zEnd && zIn[1] == '\n' ){` |
|        5 | 1649 | `				zIn += 2;` |
|        5 | 1650 | `				nLine++;` |
|     9624 | 1651 | `			}else if( zIn < zEnd && zIn[0] == '\n' ){` |
|      137 | 1652 | `				zIn++;` |
|      137 | 1653 | `				nLine++;` |
|       66 | 1654 | `			}` |
|     4807 | 1655 | `		}` |
|        5 | 1656 | `	} /* For(;;) */` |
|        - | 1657 |  |
|    28836 | 1658 | ` 	return SXRET_OK;` |
|    14415 | 1659 | `}` |
|        - | 1660 |  |
