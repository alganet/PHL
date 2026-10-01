# src/ph7/lex.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 870/925 lines (94.05%)

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
|        - |   52 | ` */` |
|  2073354 |   53 | `static const char * LexCastToken(const unsigned char *zIn,const unsigned char *zEnd,` |
|        - |   54 | `	const unsigned char **pzNext)` |
|        5 |   55 | `{` |
|        - |   56 | `	static const struct { const char *zName; int nName; const char *zCanon; } aCast[] = {` |
|        - |   57 | `		{ "int",     3, "(int)"    }, { "integer", 7, "(int)"    },` |
|        - |   58 | `		{ "bool",    4, "(bool)"   }, { "boolean", 7, "(bool)"   },` |
|        - |   59 | `		{ "float",   5, "(float)"  }, { "double",  6, "(float)"  },` |
|        - |   60 | `		{ "string",  6, "(string)" }, { "binary",  6, "(string)" },` |
|        - |   61 | `		{ "array",   5, "(array)"  }, { "object",  6, "(object)" },` |
|        - |   62 | `		{ "unset",   5, "(unset)"  }, { "real",    4, "(real)"   },` |
|        - |   63 | `		{ "void",    4, "(void)"   }` |
|        - |   64 | `	};` |
|  2073359 |   65 | `	const unsigned char *z = zIn,*zName;` |
|        - |   66 | `	sxu32 nName;` |
|        - |   67 | `	sxu32 i;` |
|  3733166 |   68 | `	while( z < zEnd && (z[0] == ' ' \|\| z[0] == '\t') ){` |
|   625072 |   69 | `		z++;` |
|        5 |   70 | `	}` |
|  2073359 |   71 | `	zName = z;` |
|  4009336 |   72 | `	while( z < zEnd && z[0] < 0x80 && SyisAlpha(z[0]) ){` |
|  1935982 |   73 | `		z++;` |
|        5 |   74 | `	}` |
|  2073359 |   75 | `	nName = (sxu32)(z - zName);` |
|  3273253 |   76 | `	while( z < zEnd && (z[0] == ' ' \|\| z[0] == '\t') ){` |
|   165159 |   77 | `		z++;` |
|        5 |   78 | `	}` |
|  2073359 |   79 | `	if( nName < 1 \|\| z >= zEnd \|\| z[0] != ')' ){` |
|  2028158 |   80 | `		return 0;` |
|        - |   81 | `	}` |
|   199720 |   82 | `	for( i = 0 ; i < SX_ARRAYSIZE(aCast) ; ++i ){` |
|   196741 |   83 | `		if( nName == (sxu32)aCast[i].nName` |
|   131057 |   84 | `		 && SyStrnicmp((const char *)zName,aCast[i].zName,nName) == 0 ){` |
|    42232 |   85 | `			*pzNext = &z[1];` |
|    42232 |   86 | `			return aCast[i].zCanon;` |
|        - |   87 | `		}` |
|    77025 |   88 | `	}` |
|     2979 |   89 | `	return 0;` |
|  1034745 |   90 | `}` |
|        - |   91 | `/*` |
|        - |   92 | ` * Tokenize a raw PHP input.` |
|        - |   93 | ` * Get a single low-level token from the input file. Update the stream pointer so that` |
|        - |   94 | ` * it points to the first character beyond the extracted token.` |
|        - |   95 | ` */` |
| 22797213 |   96 | `static sxi32 TokenizePHP(SyStream *pStream,SyToken *pToken,void *pUserData,void *pCtxData)` |
|        5 |   97 | `{` |
|        - |   98 | `	SyString *pStr;` |
|        - |   99 | `	sxi32 rc;` |
|        - |  100 | `	/* Ignore leading white spaces */` |
| 37128505 |  101 | `	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){` |
|        - |  102 | `		/* Advance the stream cursor */` |
| 14331292 |  103 | `		if( pStream->zText[0] == '\n' ){` |
|        - |  104 | `			/* Update line counter */` |
|   175420 |  105 | `			pStream->nLine++;` |
|    87284 |  106 | `		}` |
| 14331292 |  107 | `		pStream->zText++;` |
|        5 |  108 | `	}` |
| 22797218 |  109 | `	if( pStream->zText >= pStream->zEnd ){` |
|        - |  110 | `		/* End of input reached */` |
|      ! 0 |  111 | `		return SXERR_EOF;` |
|        - |  112 | `	}` |
|        - |  113 | `	/* Record token starting position and line */` |
| 22797218 |  114 | `	pToken->nLine = pStream->nLine;` |
| 22797218 |  115 | `	pToken->pUserData = 0;` |
| 22797218 |  116 | `	pStr = &pToken->sData;` |
| 22797218 |  117 | `	SyStringInitFromBuf(pStr,pStream->zText,0);` |
|        - |  118 | ``	/* php's BINARY-STRING PREFIX. A lone `b`/`B` welded to a quote or to `<<<` is`` |
|        - |  119 | `	 * not an identifier: php's scanner takes it as part of the string token, and` |
|        - |  120 | `	 * since php has ONE string type it marks nothing at runtime -- which is exactly` |
|        - |  121 | ``	 * why it survives in real source (`b'foo'` is `'foo'`). PHL lexed the letter as`` |
|        - |  122 | ``	 * a label, so every such literal was `syntax error, unexpected`` |
|        - |  123 | `` 	 * single-quoted string`. The prefix is ADJACENT only: `b <<<'S'` and `bb'foo'` `` |
|        - |  124 | `	 * are an identifier followed by a string, in php as here. */` |
| 22797213 |  125 | `	if( (pStream->zText[0] == 'b' \|\| pStream->zText[0] == 'B')` |
| 11429393 |  126 | `	 && &pStream->zText[1] < pStream->zEnd` |
| 11429304 |  127 | `	 && ( pStream->zText[1] == '\'' \|\| pStream->zText[1] == '"'` |
|    98831 |  128 | `	   \|\| ( pStream->zText[1] == '<' && &pStream->zText[3] < pStream->zEnd` |
|        4 |  129 | `	     && pStream->zText[2] == '<' && pStream->zText[3] == '<' ) ) ){` |
|       23 |  130 | `		pStream->zText++;                       /* the prefix is not part of the VALUE */` |
|       23 |  131 | `		SyStringInitFromBuf(pStr,pStream->zText,0);` |
|       11 |  132 | `	}` |
| 26213006 |  133 | `	if( LEX_LABEL_START(pStream->zText[0]) ){` |
|        - |  134 | `		const unsigned char *zIn;` |
|        - |  135 | `		sxu32 nKeyword;` |
|        - |  136 | `		/* Isolate the LABEL. php's class is a flat byte set — [a-zA-Z_\x80-\xff] then` |
|        - |  137 | `		 * [a-zA-Z0-9_\x80-\xff]* — so no UTF-8 decoding is involved: a multibyte name` |
|        - |  138 | `		 * is consumed because every one of its bytes is >= 0x80. (This replaces the` |
|        - |  139 | `		 * xPP lead-byte-plus-continuations dance, which required a lead >= 0xc0 and so` |
|        - |  140 | `		 * stopped one byte class short of php's own rule; see LEX_LABEL_START.) */` |
|  6820719 |  141 | `		zIn = &pStream->zText[1];` |
| 40009044 |  142 | `		while( zIn < pStream->zEnd && LEX_LABEL_BYTE(zIn[0]) ){` |
| 29783404 |  143 | `			zIn++;` |
|        5 |  144 | `		}` |
|  6820719 |  145 | `		pStream->zText = zIn;` |
|        - |  146 | `		/* Record token length */` |
|  6820719 |  147 | `		pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  6820719 |  148 | `		nKeyword = KeywordCodeCI(pStr->zString,(int)pStr->nByte);` |
|  6820719 |  149 | `		if( nKeyword != PH7_TK_ID ){` |
|  2315419 |  150 | `			if( nKeyword &` |
|        - |  151 | `				(PH7_TKWRD_NEW\|PH7_TKWRD_CLONE\|PH7_TKWRD_AND\|PH7_TKWRD_XOR\|PH7_TKWRD_OR\|PH7_TKWRD_INSTANCEOF) ){` |
|        - |  152 | `					/* Alpha stream operators [i.e: new,clone,and,instanceof,or,xor],save the operator instance for later processing */` |
|   114282 |  153 | `					pToken->pUserData = (void *)PH7_ExprExtractOperator(pStr,0);` |
|        - |  154 | `					/* Mark as an operator */` |
|   114282 |  155 | `					pToken->nType = PH7_TK_ID\|PH7_TK_OP;` |
|    57064 |  156 | `			}else{` |
|        - |  157 | `				/* We are dealing with a keyword [i.e: while,foreach,class...],save the keyword ID */` |
|  2201142 |  158 | `				pToken->nType = PH7_TK_KEYWORD;` |
|  2201142 |  159 | `				pToken->pUserData = SX_INT_TO_PTR(nKeyword);` |
|        - |  160 | `			}` |
|  1155998 |  161 | `		}else{` |
|        - |  162 | `			/* A simple identifier */` |
|  4505305 |  163 | `			pToken->nType = PH7_TK_ID;` |
|        - |  164 | `		}` |
|  3404931 |  165 | `	}else{` |
|        - |  166 | `		sxi32 c;` |
|        - |  167 | `		/* Non-alpha stream */` |
| 15976504 |  168 | `		if( pStream->zText[0] == '#' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '[' ){` |
|      689 |  169 | `			sxu32 nDepth = 1;` |
|        - |  170 | `			/* PHP 8 attribute group '#[ ... ]': skip the whole balanced group as` |
|        - |  171 | `			 * trivia (attributes are not stored yet). Brackets inside string` |
|        - |  172 | `			 * literals and comments must not affect the depth count. An` |
|        - |  173 | `			 * unterminated group is silently consumed up to EOF, consistent` |
|        - |  174 | `			 * with unterminated block comments below.` |
|        - |  175 | `			 */` |
|        - |  176 | `			const unsigned char *zGroupStart;` |
|      689 |  177 | `			pStream->zText += 2;` |
|      689 |  178 | `			zGroupStart = pStream->zText;` |
|    11793 |  179 | `			while( pStream->zText < pStream->zEnd && nDepth > 0 ){` |
|    11109 |  180 | `				sxi32 d = pStream->zText[0];` |
|    11109 |  181 | `				if( d == '[' ){` |
|       32 |  182 | `					nDepth++;` |
|    11094 |  183 | `				}else if( d == ']' ){` |
|      719 |  184 | `					nDepth--;` |
|    10722 |  185 | `				}else if( d == '\'' \|\| d == '"' ){` |
|        - |  186 | `					/* String literal: scan for the matching unescaped quote */` |
|      135 |  187 | `					pStream->zText++;` |
|      759 |  188 | `					while( pStream->zText < pStream->zEnd ){` |
|      759 |  189 | `						if( pStream->zText[0] == '\\' && &pStream->zText[1] < pStream->zEnd ){` |
|       23 |  190 | `							if( pStream->zText[1] == '\n' ){` |
|      ! 0 |  191 | `								pStream->nLine++;` |
|      ! 0 |  192 | `							}` |
|       23 |  193 | `							pStream->zText += 2;` |
|       23 |  194 | `							continue;` |
|        - |  195 | `						}` |
|      737 |  196 | `						if( pStream->zText[0] == d ){` |
|      135 |  197 | `							break;` |
|        - |  198 | `						}` |
|      605 |  199 | `						if( pStream->zText[0] == '\n' ){` |
|      ! 0 |  200 | `							pStream->nLine++;` |
|      ! 0 |  201 | `						}` |
|      605 |  202 | `						pStream->zText++;` |
|        3 |  203 | `					}` |
|      135 |  204 | `					if( pStream->zText >= pStream->zEnd ){` |
|      ! 0 |  205 | `						break; /* Unterminated string literal */` |
|        3 |  206 | `					}` |
|        - |  207 | `					/* Fall through: consume the closing quote below */` |
|    10299 |  208 | `				}else if( d == '#' \|\| (d == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '/') ){` |
|        - |  209 | `					/* Inline comment inside the group */` |
|      ! 0 |  210 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] != '\n' ){` |
|      ! 0 |  211 | `						pStream->zText++;` |
|      ! 0 |  212 | `					}` |
|      ! 0 |  213 | `					continue; /* Let the outer loop count the newline */` |
|    10233 |  214 | `				}else if( d == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '*' ){` |
|        - |  215 | `					/* Block comment inside the group */` |
|      ! 0 |  216 | `					pStream->zText += 2;` |
|      ! 0 |  217 | `					while( pStream->zText < pStream->zEnd ){` |
|      ! 0 |  218 | `						if( pStream->zText[0] == '*' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '/' ){` |
|      ! 0 |  219 | `							pStream->zText += 2;` |
|      ! 0 |  220 | `							break;` |
|        - |  221 | `						}` |
|      ! 0 |  222 | `						if( pStream->zText[0] == '\n' ){` |
|      ! 0 |  223 | `							pStream->nLine++;` |
|      ! 0 |  224 | `						}` |
|      ! 0 |  225 | `						pStream->zText++;` |
|      ! 0 |  226 | `					}` |
|      ! 0 |  227 | `					continue;` |
|    10233 |  228 | `				}else if( d == '\n' ){` |
|        7 |  229 | `					pStream->nLine++;` |
|        3 |  230 | `				}` |
|    11109 |  231 | `				pStream->zText++;` |
|        5 |  232 | `			}` |
|      689 |  233 | `			if( pUserData && pStream->pSet ){` |
|        - |  234 | `				/* Record the group's inner span (between #[ and its balanced ])` |
|        - |  235 | `				 * in the trivia sidecar, keyed like doc-comments. */` |
|        - |  236 | `				ph7_trivia sTrivia;` |
|      689 |  237 | `				const unsigned char *zGroupEnd = pStream->zText;` |
|      689 |  238 | `				if( nDepth == 0 && zGroupEnd > zGroupStart ){` |
|      689 |  239 | `					zGroupEnd--; /* Exclude the closing ']' */` |
|      342 |  240 | `				}` |
|      689 |  241 | `				sTrivia.nTokIdx = SySetUsed(pStream->pSet);` |
|      689 |  242 | `				sTrivia.iKind = PH7_TRIVIA_ATTR;` |
|      689 |  243 | `				SyStringInitFromBuf(&sTrivia.sText,(const char *)zGroupStart,(sxu32)(zGroupEnd - zGroupStart));` |
|      689 |  244 | `				sTrivia.nLine = pToken->nLine;` |
|      689 |  245 | `				SySetPut((SySet *)pUserData,(const void *)&sTrivia);` |
|      342 |  246 | `			}` |
|        - |  247 | `			/* Tell the upper-layer to ignore this token */` |
|      689 |  248 | `			return SXERR_CONTINUE;` |
| 16123950 |  249 | `		}else if( pStream->zText[0] == '#' \|\|` |
| 15975809 |  250 | `			( pStream->zText[0] == '/' &&  &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '/') ){` |
|    15377 |  251 | `				pStream->zText++;` |
|        - |  252 | `				/* Inline comments */` |
|   829165 |  253 | `				while( pStream->zText < pStream->zEnd && pStream->zText[0] != '\n' ){` |
|   813793 |  254 | `					pStream->zText++;` |
|        5 |  255 | `				}` |
|        - |  256 | `				/* Tell the upper-layer to ignore this token */` |
|    15377 |  257 | `				return SXERR_CONTINUE;` |
| 15960448 |  258 | `		}else if( pStream->zText[0] == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '*' ){` |
|        - |  259 | `			/* A doc-comment starts with slash-star-star followed by more` |
|        - |  260 | `			 * content (slash-star-star-slash is the empty comment, not a` |
|        - |  261 | `			 * docblock). Its full span, delimiters included, goes to the` |
|        - |  262 | `			 * trivia sidecar when the caller supplied one — keyed by the` |
|        - |  263 | `			 * index the NEXT real token receives — and never enters the` |
|        - |  264 | `			 * token stream. */` |
|   273624 |  265 | `			const unsigned char *zDocStart = pStream->zText;` |
|   273789 |  266 | `			int bDoc = ( &pStream->zText[2] < pStream->zEnd && pStream->zText[2] == '*'` |
|   410793 |  267 | `			 && ( &pStream->zText[3] >= pStream->zEnd \|\| pStream->zText[3] != '/' ) );` |
|   273624 |  268 | `			pStream->zText += 2;` |
|        - |  269 | `			/* Block comment */` |
| 42272784 |  270 | `			while( pStream->zText < pStream->zEnd ){` |
| 42272780 |  271 | `				if( pStream->zText[0] == '*' ){` |
|   401555 |  272 | `					if( &pStream->zText[1] >= pStream->zEnd \|\| pStream->zText[1] == '/'  ){` |
|   136613 |  273 | `						break;` |
|        - |  274 | `					}` |
|    63863 |  275 | `				}` |
| 41999165 |  276 | `				if( pStream->zText[0] == '\n' ){` |
|     6800 |  277 | `					pStream->nLine++;` |
|     3374 |  278 | `				}` |
| 41999165 |  279 | `				pStream->zText++;` |
|        5 |  280 | `			}` |
|   273619 |  281 | `			if( pStream->zText >= pStream->zEnd` |
|   273622 |  282 | `			 \|\| !(pStream->zText[0] == '*' && &pStream->zText[1] < pStream->zEnd` |
|   273615 |  283 | `			   && pStream->zText[1] == '/') ){` |
|        - |  284 | `				/* The comment never closed before the end of the input. php refuses` |
|        - |  285 | ``				 * the file (`Unterminated comment starting line N`) where this`` |
|        - |  286 | `				 * swallowed the rest of it in silence; hand the compile phase a` |
|        - |  287 | `				 * token to report it with. */` |
|        4 |  288 | `				pToken->nType = PH7_TK_OTHER\|PH7_TK_UNTERM;` |
|        4 |  289 | `				SyStringInitFromBuf(&pToken->sData,"/*",sizeof("/*")-1);` |
|        4 |  290 | `				pStream->zText = pStream->zEnd;` |
|        4 |  291 | `				return SXRET_OK;` |
|        - |  292 | `			}` |
|   273620 |  293 | `			pStream->zText += 2;` |
|   273620 |  294 | `			if( bDoc && pUserData && pStream->pSet ){` |
|        - |  295 | `				ph7_trivia sTrivia;` |
|      335 |  296 | `				const unsigned char *zDocEnd = pStream->zText;` |
|      335 |  297 | `				if( zDocEnd > pStream->zEnd ){` |
|      ! 0 |  298 | `					zDocEnd = pStream->zEnd; /* Unterminated comment at EOF */` |
|      ! 0 |  299 | `				}` |
|      335 |  300 | `				sTrivia.nTokIdx = SySetUsed(pStream->pSet);` |
|      335 |  301 | `				sTrivia.iKind = PH7_TRIVIA_DOC;` |
|      335 |  302 | `				SyStringInitFromBuf(&sTrivia.sText,(const char *)zDocStart,(sxu32)(zDocEnd - zDocStart));` |
|      335 |  303 | `				sTrivia.nLine = pToken->nLine;` |
|      335 |  304 | `				SySetPut((SySet *)pUserData,(const void *)&sTrivia);` |
|      165 |  305 | `			}` |
|        - |  306 | `			/* Tell the upper-layer to ignore this token */` |
|   273620 |  307 | `			return SXERR_CONTINUE;` |
| 15686829 |  308 | `		}else if( SyisDigit(pStream->zText[0]) ){` |
|   772859 |  309 | `			pStream->zText++;` |
|        - |  310 | `			/* PHP 7.4: handle underscore separator immediately following the first digit.` |
|        - |  311 | `			 * Check pStream->zText < pStream->zEnd BEFORE forming pStream->zText + 1 so` |
|        - |  312 | `			 * we never compute a pointer past one-past-end. */` |
|   772854 |  313 | `			if( pStream->zText < pStream->zEnd` |
|   772844 |  314 | `				&& pStream->zText[0] == '_'` |
|   385956 |  315 | `				&& pStream->zText + 1 < pStream->zEnd` |
|      168 |  316 | `				&& pStream->zText[1] < 0xc0` |
|      173 |  317 | `				&& SyisDigit(pStream->zText[1]) ){` |
|      158 |  318 | `				pStream->zText++; /* swallow underscore between two digits */` |
|       78 |  319 | `			}` |
|        - |  320 | `			/* Decimal digit stream (PHP 7.4: underscore separator allowed between two digits) */` |
|  1088268 |  321 | `			while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|   315414 |  322 | `				pStream->zText++;` |
|   315409 |  323 | `				if( pStream->zText < pStream->zEnd` |
|   315409 |  324 | `					&& pStream->zText[0] == '_'` |
|   157540 |  325 | `					&& pStream->zText + 1 < pStream->zEnd` |
|      172 |  326 | `					&& pStream->zText[1] < 0xc0` |
|      177 |  327 | `					&& SyisDigit(pStream->zText[1]) ){` |
|      173 |  328 | `					pStream->zText++; /* swallow underscore between two digits */` |
|       86 |  329 | `				}` |
|        5 |  330 | `			}` |
|        - |  331 | `			/* Mark the token as integer until we encounter a real number */` |
|   772859 |  332 | `			pToken->nType = PH7_TK_INTEGER;` |
|   772859 |  333 | `			if( pStream->zText < pStream->zEnd ){` |
|   772839 |  334 | `				c = pStream->zText[0];` |
|   772839 |  335 | `				if( c == '.' ){` |
|        - |  336 | `					/* Real number (PHP 7.4: underscore separator allowed between two digits) */` |
|    15542 |  337 | `					pStream->zText++;` |
|    33059 |  338 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|    17522 |  339 | `						pStream->zText++;` |
|    17517 |  340 | `						if( pStream->zText < pStream->zEnd` |
|    17517 |  341 | `							&& pStream->zText[0] == '_'` |
|     8755 |  342 | `							&& pStream->zText + 1 < pStream->zEnd` |
|       12 |  343 | `							&& pStream->zText[1] < 0xc0` |
|       17 |  344 | `							&& SyisDigit(pStream->zText[1]) ){` |
|       13 |  345 | `							pStream->zText++;` |
|        6 |  346 | `						}` |
|        5 |  347 | `					}` |
|    15542 |  348 | `					if( pStream->zText < pStream->zEnd ){` |
|    15542 |  349 | `						c = pStream->zText[0];` |
|    15542 |  350 | `						if( c=='e' \|\| c=='E' ){` |
|      149 |  351 | `							pStream->zText++;` |
|      149 |  352 | `							if( pStream->zText < pStream->zEnd ){` |
|      149 |  353 | `								c = pStream->zText[0];` |
|      146 |  354 | `								if( (c =='+' \|\| c=='-') && &pStream->zText[1] < pStream->zEnd  &&` |
|      119 |  355 | `									pStream->zText[1] < 0xc0 && SyisDigit(pStream->zText[1]) ){` |
|      119 |  356 | `										pStream->zText++;` |
|       58 |  357 | `								}` |
|      429 |  358 | `								while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|      283 |  359 | `									pStream->zText++;` |
|      280 |  360 | `									if( pStream->zText < pStream->zEnd` |
|      280 |  361 | `										&& pStream->zText[0] == '_'` |
|      144 |  362 | `										&& pStream->zText + 1 < pStream->zEnd` |
|        8 |  363 | `										&& pStream->zText[1] < 0xc0` |
|       11 |  364 | `										&& SyisDigit(pStream->zText[1]) ){` |
|        9 |  365 | `										pStream->zText++;` |
|        4 |  366 | `									}` |
|        3 |  367 | `								}` |
|       73 |  368 | `							}` |
|       73 |  369 | `						}` |
|     7759 |  370 | `					}` |
|    15542 |  371 | `					pToken->nType = PH7_TK_REAL;` |
|   765061 |  372 | `				}else if( c=='e' \|\| c=='E' ){` |
|       92 |  373 | `					SXUNUSED(pUserData); /* Prevent compiler warning */` |
|       92 |  374 | `					SXUNUSED(pCtxData);` |
|      189 |  375 | `					pStream->zText++;` |
|      189 |  376 | `					if( pStream->zText < pStream->zEnd ){` |
|      189 |  377 | `						c = pStream->zText[0];` |
|      184 |  378 | `						if( (c =='+' \|\| c=='-') && &pStream->zText[1] < pStream->zEnd  &&` |
|       45 |  379 | `							pStream->zText[1] < 0xc0 && SyisDigit(pStream->zText[1]) ){` |
|       43 |  380 | `								pStream->zText++;` |
|       20 |  381 | `						}` |
|      563 |  382 | `						while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|      379 |  383 | `							pStream->zText++;` |
|      374 |  384 | `							if( pStream->zText < pStream->zEnd` |
|      374 |  385 | `								&& pStream->zText[0] == '_'` |
|      189 |  386 | `								&& pStream->zText + 1 < pStream->zEnd` |
|        4 |  387 | `								&& pStream->zText[1] < 0xc0` |
|        9 |  388 | `								&& SyisDigit(pStream->zText[1]) ){` |
|        5 |  389 | `								pStream->zText++;` |
|        2 |  390 | `							}` |
|        5 |  391 | `						}` |
|       92 |  392 | `					}` |
|      189 |  393 | `					pToken->nType = PH7_TK_REAL;` |
|        - |  394 | `				/* php only reads a base prefix when the literal so far is exactly "0"` |
|        - |  395 | `				 * AND at least one valid digit follows it. Otherwise the '0' stands` |
|        - |  396 | `				 * alone as an integer and the letter begins an IDENTIFIER, which is` |
|        - |  397 | ``				 * why php reports `0xG` as `unexpected identifier "xG"` while PHL,`` |
|        - |  398 | `				 * consuming the prefix unconditionally, reported just "G". The same` |
|        - |  399 | ``				 * gap silently ACCEPTED `0x`/`0b`/`0o` as int(0), and read `1x5` as`` |
|        - |  400 | `				 * a hex literal, both of which php rejects outright. */` |
|   757210 |  401 | `				}else if( (c == 'x' \|\| c == 'X')` |
|   378186 |  402 | `					&& pStream->zText == &((const unsigned char *)pStr->zString)[1] && pStr->zString[0] == 0x30` |
|      329 |  403 | `					&& &pStream->zText[1] < pStream->zEnd` |
|      334 |  404 | `					&& pStream->zText[1] < 0xc0 && SyisHex(pStream->zText[1]) ){` |
|        - |  405 | `					/* Hex digit stream (PHP 7.4: underscore separator allowed between two digits) */` |
|      329 |  406 | `					pStream->zText++;` |
|     1625 |  407 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisHex(pStream->zText[0]) ){` |
|     1300 |  408 | `						pStream->zText++;` |
|     1296 |  409 | `						if( pStream->zText < pStream->zEnd` |
|     1296 |  410 | `							&& pStream->zText[0] == '_'` |
|      669 |  411 | `							&& pStream->zText + 1 < pStream->zEnd` |
|       50 |  412 | `							&& pStream->zText[1] < 0xc0` |
|       54 |  413 | `							&& SyisHex(pStream->zText[1]) ){` |
|       51 |  414 | `							pStream->zText++;` |
|       25 |  415 | `						}` |
|        4 |  416 | `					}` |
|   756954 |  417 | `				}else if( (c == 'b' \|\| c == 'B')` |
|   378003 |  418 | `					&& pStream->zText == &((const unsigned char *)pStr->zString)[1] && pStr->zString[0] == 0x30` |
|      288 |  419 | `					&& &pStream->zText[1] < pStream->zEnd` |
|      293 |  420 | `					&& (pStream->zText[1] == '0' \|\| pStream->zText[1] == '1') ){` |
|        - |  421 | `					/* Binary digit stream (PHP 7.4: underscore separator allowed between two digits) */` |
|      287 |  422 | `					pStream->zText++;` |
|     3115 |  423 | `					while( pStream->zText < pStream->zEnd && (pStream->zText[0] == '0' \|\| pStream->zText[0] == '1') ){` |
|     1791 |  424 | `						pStream->zText++;` |
|     1790 |  425 | `						if( pStream->zText < pStream->zEnd` |
|     1790 |  426 | `							&& pStream->zText[0] == '_'` |
|      965 |  427 | `							&& pStream->zText + 1 < pStream->zEnd` |
|      141 |  428 | `							&& (pStream->zText[1] == '0' \|\| pStream->zText[1] == '1') ){` |
|      141 |  429 | `							pStream->zText++;` |
|       70 |  430 | `						}` |
|        1 |  431 | `					}` |
|   756646 |  432 | `				}else if( (c == 'o' \|\| c == 'O')` |
|   377726 |  433 | `					&& pStream->zText == &((const unsigned char *)pStr->zString)[1] && pStr->zString[0] == 0x30` |
|       20 |  434 | `					&& &pStream->zText[1] < pStream->zEnd` |
|       25 |  435 | `					&& pStream->zText[1] >= '0' && pStream->zText[1] <= '7' ){` |
|        - |  436 | `					/* PHP 8.1 explicit octal 0o/0O (underscore separator allowed between two digits) */` |
|       21 |  437 | `					pStream->zText++;` |
|      101 |  438 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] >= '0' && pStream->zText[0] <= '7' ){` |
|       81 |  439 | `						pStream->zText++;` |
|       80 |  440 | `						if( pStream->zText < pStream->zEnd` |
|       80 |  441 | `							&& pStream->zText[0] == '_'` |
|       41 |  442 | `							&& pStream->zText + 1 < pStream->zEnd` |
|        3 |  443 | `							&& pStream->zText[1] >= '0' && pStream->zText[1] <= '7' ){` |
|        3 |  444 | `							pStream->zText++;` |
|        1 |  445 | `						}` |
|        1 |  446 | `					}` |
|       10 |  447 | `				}` |
|   385872 |  448 | `			}` |
|        - |  449 | `			/* A MISPLACED PHP 7.4 separator needs nothing here: it is where php's` |
|        - |  450 | ``			 * scanner stops the number and starts a LABEL, so `1_` is the integer`` |
|        - |  451 | ``			 * 1 followed by the identifier `_` and `0x_1f` the integer 0 followed`` |
|        - |  452 | ``			 * by `x_1f` -- which is the identifier php's parse error names. This`` |
|        - |  453 | `			 * used to ABSORB the run into the numeric token and re-report it from` |
|        - |  454 | `			 * the compile phase; that named the same thing until a leading-dot` |
|        - |  455 | ``			 * float could follow it, at which point `1_.5` complained about the`` |
|        - |  456 | ``			 * `.5` two tokens later where php complains about the `_`. */`` |
|        - |  457 | `			/* Record token length */` |
|   772859 |  458 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|   772859 |  459 | `			return SXRET_OK;` |
| 14913970 |  460 | `		}else if( pStream->zText[0] == '.' && &pStream->zText[1] < pStream->zEnd` |
|   238547 |  461 | `			&& pStream->zText[1] < 0xc0 && SyisDigit(pStream->zText[1]) ){` |
|        - |  462 | `` 			/* php's DNUM has a leading-dot form -- `({LNUM}?"."{LNUM})` -- so `.5` `` |
|        - |  463 | `			 * is a float literal and not the concatenation operator followed by a 5.` |
|        - |  464 | `			 * The scanner takes it UNCONDITIONALLY, wherever the dot stands: php` |
|        - |  465 | ``			 * reads `"x".5` as a string followed by the float `.5` and reports a`` |
|        - |  466 | `			 * parse error for it, which is why a program that means concatenation` |
|        - |  467 | ``			 * has to write the space (`"x". 5`). PHL had no such token at all, so`` |
|        - |  468 | ``			 * every `.5` in real source -- an ordinary way to write a fraction --`` |
|        - |  469 | ``			 * was `syntax error, unexpected token ";"`. */`` |
|       33 |  470 | `			pStream->zText++; /* Jump the dot */` |
|      117 |  471 | `			while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0` |
|      105 |  472 | `				&& SyisDigit(pStream->zText[0]) ){` |
|       41 |  473 | `				pStream->zText++;` |
|       38 |  474 | `				if( pStream->zText < pStream->zEnd` |
|       38 |  475 | `					&& pStream->zText[0] == '_'` |
|       20 |  476 | `					&& pStream->zText + 1 < pStream->zEnd` |
|        2 |  477 | `					&& pStream->zText[1] < 0xc0` |
|        5 |  478 | `					&& SyisDigit(pStream->zText[1]) ){` |
|        3 |  479 | `					pStream->zText++; /* swallow underscore between two digits */` |
|        1 |  480 | `				}` |
|        3 |  481 | `			}` |
|       30 |  482 | `			if( pStream->zText < pStream->zEnd` |
|       33 |  483 | `			 && (pStream->zText[0] == 'e' \|\| pStream->zText[0] == 'E') ){` |
|        - |  484 | `				/* An EXPONENT_DNUM built on this DNUM. The sign is taken only when a` |
|        - |  485 | ``				 * digit follows it, exactly as the two runs above do, so `.5e+` is`` |
|        - |  486 | ``				 * the float `.5` followed by an identifier rather than a bad float. */`` |
|        5 |  487 | `				const unsigned char *zRewind = pStream->zText;` |
|        5 |  488 | `				pStream->zText++;` |
|        4 |  489 | `				if( pStream->zText < pStream->zEnd` |
|        4 |  490 | `				 && (pStream->zText[0] == '+' \|\| pStream->zText[0] == '-')` |
|        3 |  491 | `				 && &pStream->zText[1] < pStream->zEnd` |
|        4 |  492 | `				 && pStream->zText[1] < 0xc0 && SyisDigit(pStream->zText[1]) ){` |
|        3 |  493 | `					pStream->zText++;` |
|        1 |  494 | `				}` |
|        4 |  495 | `				if( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0` |
|        5 |  496 | `				 && SyisDigit(pStream->zText[0]) ){` |
|       14 |  497 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0` |
|       13 |  498 | `						&& SyisDigit(pStream->zText[0]) ){` |
|        5 |  499 | `						pStream->zText++;` |
|        4 |  500 | `						if( pStream->zText < pStream->zEnd` |
|        4 |  501 | `							&& pStream->zText[0] == '_'` |
|        2 |  502 | `							&& pStream->zText + 1 < pStream->zEnd` |
|      ! 0 |  503 | `							&& pStream->zText[1] < 0xc0` |
|        1 |  504 | `							&& SyisDigit(pStream->zText[1]) ){` |
|      ! 0 |  505 | `							pStream->zText++; /* swallow underscore between two digits */` |
|      ! 0 |  506 | `						}` |
|        1 |  507 | `					}` |
|        3 |  508 | `				}else{` |
|      ! 0 |  509 | `					pStream->zText = zRewind;` |
|        - |  510 | `				}` |
|        2 |  511 | `			}` |
|       33 |  512 | `			pToken->nType = PH7_TK_REAL;` |
|       33 |  513 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|       33 |  514 | `			return SXRET_OK;` |
|        - |  515 | `		}` |
| 14913945 |  516 | `		c = pStream->zText[0];` |
| 14913945 |  517 | `		pStream->zText++; /* Advance the stream cursor */` |
|        - |  518 | `		/* Assume we are dealing with an operator*/` |
| 14913945 |  519 | `		pToken->nType = PH7_TK_OP;` |
| 14913945 |  520 | `		switch(c){` |
|  3171024 |  521 | `		case '$': pToken->nType = PH7_TK_DOLLAR; break;` |
|   867119 |  522 | `		case '{': pToken->nType = PH7_TK_OCB;    break;` |
|   867105 |  523 | `		case '}': pToken->nType = PH7_TK_CCB;    break;` |
|  1038614 |  524 | `		case '(': {` |
|        - |  525 | `			/* A type cast is recognised HERE, off the raw bytes, exactly as php's` |
|        - |  526 | `			 * scanner does it (LexCastToken above). */` |
|  2073359 |  527 | `			const unsigned char *zNext = 0;` |
|  2073359 |  528 | `			const char *zCanon = LexCastToken(pStream->zText,pStream->zEnd,&zNext);` |
|  2073359 |  529 | `			if( zCanon ){` |
|    42232 |  530 | `				pStream->zText = zNext;` |
|    42232 |  531 | `				SyStringInitFromBuf(&pToken->sData,zCanon,SyStrlen(zCanon));` |
|    42232 |  532 | `				if( SyStrncmp(zCanon,"(void)",sizeof("(void)")-1) == 0 ){` |
|        - |  533 | ``					/* php 8.5's `(void)`, which converts nothing and is taken by the`` |
|        - |  534 | `					 * grammar only at the head of an expression STATEMENT. */` |
|       33 |  535 | `					pToken->nType = PH7_TK_VOID_CAST;` |
|       33 |  536 | `					pToken->pUserData = 0;` |
|       17 |  537 | `				}else{` |
|    42200 |  538 | `					pToken->nType = PH7_TK_OP;` |
|    42200 |  539 | `					pToken->pUserData = (void *)PH7_ExprExtractOperator(&pToken->sData,0);` |
|        - |  540 | `				}` |
|    42232 |  541 | `				return SXRET_OK;` |
|        - |  542 | `			}` |
|  2031132 |  543 | `			pToken->nType = PH7_TK_LPAREN;` |
|  2031132 |  544 | `			break;` |
|        - |  545 | `				  }` |
|   283731 |  546 | `		case '[': pToken->nType \|= PH7_TK_OSB;   break; /* Bitwise operation here,since the square bracket token '['` |
|        - |  547 | `														 * is a potential operator [i.e: subscripting] */` |
|   283737 |  548 | `		case ']': pToken->nType = PH7_TK_CSB;    break;` |
|  1017455 |  549 | `		case ')':` |
|  2031110 |  550 | `			pToken->nType = PH7_TK_RPAREN;` |
|  2031110 |  551 | `			break;` |
|   312738 |  552 | `		case '\'':{` |
|        - |  553 | `			/* Single quoted string */` |
|   623969 |  554 | `			pStr->zString++;` |
|  9750783 |  555 | `			while( pStream->zText < pStream->zEnd ){` |
|  9750779 |  556 | `				if( pStream->zText[0] == '\''  ){` |
|   624044 |  557 | `					if( pStream->zText[-1] != '\\' ){` |
|   623727 |  558 | `						break;` |
|      ! 0 |  559 | `					}else{` |
|      322 |  560 | `						const unsigned char *zPtr = &pStream->zText[-2];` |
|      322 |  561 | `						sxi32 i = 1;` |
|      584 |  562 | `						while( zPtr > pStream->zInput && zPtr[0] == '\\' ){` |
|      267 |  563 | `							zPtr--;` |
|      267 |  564 | `							i++;` |
|        5 |  565 | `						}` |
|      322 |  566 | `						if((i&1)==0){` |
|      243 |  567 | `							break;` |
|        - |  568 | `						}` |
|        - |  569 | `					}` |
|       39 |  570 | `				}` |
|  9126819 |  571 | `				if( pStream->zText[0] == '\n' ){` |
|      125 |  572 | `					pStream->nLine++;` |
|       62 |  573 | `				}` |
|  9126819 |  574 | `				pStream->zText++;` |
|        5 |  575 | `			}` |
|        - |  576 | `			/* Record token length and type. Running into the END OF THE INPUT` |
|        - |  577 | `			 * instead of the closing quote is marked: php refuses the file, where` |
|        - |  578 | `			 * this consumed the rest of it and ran the program. */` |
|   623969 |  579 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|   623969 |  580 | `			pToken->nType = PH7_TK_SSTR;` |
|   623969 |  581 | `			if( pStream->zText >= pStream->zEnd ){` |
|        4 |  582 | `				pToken->nType \|= PH7_TK_UNTERM;` |
|        2 |  583 | `			}` |
|        - |  584 | `			/* Jump the trailing single quote */` |
|   623969 |  585 | `			pStream->zText++;` |
|   623969 |  586 | `			return SXRET_OK;` |
|        - |  587 | `				  }` |
|    38293 |  588 | `		case '"':{` |
|        - |  589 | `			sxi32 iNest;` |
|        - |  590 | `			/* Double quoted string */` |
|    76375 |  591 | `			pStr->zString++;` |
|  1057063 |  592 | `			while( pStream->zText < pStream->zEnd ){` |
|  1057055 |  593 | `				if( pStream->zText[0] == '{' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '$'){` |
|      232 |  594 | `					iNest = 1;` |
|      232 |  595 | `					pStream->zText++;` |
|        - |  596 | `					/* TICKET 1433-40: Hnadle braces'{}' in double quoted string where everything is allowed */` |
|     2107 |  597 | `					while(pStream->zText < pStream->zEnd ){` |
|     2107 |  598 | `						if( pStream->zText[0] == '{' ){` |
|        3 |  599 | `							iNest++;` |
|     2106 |  600 | `						}else if (pStream->zText[0] == '}' ){` |
|      234 |  601 | `							iNest--;` |
|      234 |  602 | `							if( iNest <= 0 ){` |
|      232 |  603 | `								pStream->zText++;` |
|      232 |  604 | `								break;` |
|        1 |  605 | `							}` |
|     1877 |  606 | `						}else if( pStream->zText[0] == '\n' ){` |
|      ! 0 |  607 | `							pStream->nLine++;` |
|      ! 0 |  608 | `						}` |
|     1880 |  609 | `						pStream->zText++;` |
|        5 |  610 | `					}` |
|      232 |  611 | `					if( pStream->zText >= pStream->zEnd ){` |
|      ! 0 |  612 | `						break;` |
|        - |  613 | `					}` |
|      113 |  614 | `				}` |
|  1057055 |  615 | `				if( pStream->zText[0] == '"' ){` |
|    77281 |  616 | `					if( pStream->zText[-1] != '\\' ){` |
|    76333 |  617 | `						break;` |
|      ! 0 |  618 | `					}else{` |
|      953 |  619 | `						const unsigned char *zPtr = &pStream->zText[-2];` |
|      953 |  620 | `						sxi32 i = 1;` |
|     1039 |  621 | `						while( zPtr > pStream->zInput && zPtr[0] == '\\' ){` |
|       90 |  622 | `							zPtr--;` |
|       90 |  623 | `							i++;` |
|        4 |  624 | `						}` |
|      953 |  625 | `						if((i&1)==0){` |
|       35 |  626 | `							break;` |
|        - |  627 | `						}` |
|        - |  628 | `					}` |
|      457 |  629 | `				}` |
|   980693 |  630 | `				if( pStream->zText[0] == '\n' ){` |
|       76 |  631 | `					pStream->nLine++;` |
|       37 |  632 | `				}` |
|   980693 |  633 | `				pStream->zText++;` |
|        5 |  634 | `			}` |
|        - |  635 | `			/* Record token length and type (see the single-quoted branch above for` |
|        - |  636 | `			 * the unterminated mark). */` |
|    76375 |  637 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|    76375 |  638 | `			pToken->nType = PH7_TK_DSTR;` |
|    76375 |  639 | `			if( pStream->zText >= pStream->zEnd ){` |
|        8 |  640 | `				pToken->nType \|= PH7_TK_UNTERM;` |
|        4 |  641 | `			}` |
|        - |  642 | `			/* Jump the trailing quote */` |
|    76375 |  643 | `			pStream->zText++;` |
|    76375 |  644 | `			return SXRET_OK;` |
|        - |  645 | `				  }` |
|        1 |  646 | ``		case '`':{`` |
|        - |  647 | `			/* Backtick quoted string */` |
|        3 |  648 | `			pStr->zString++;` |
|       21 |  649 | `			while( pStream->zText < pStream->zEnd ){` |
|       21 |  650 | ``				if( pStream->zText[0] == '`' && pStream->zText[-1] != '\\' ){`` |
|        3 |  651 | `					break;` |
|        - |  652 | `				}` |
|       19 |  653 | `				if( pStream->zText[0] == '\n' ){` |
|      ! 0 |  654 | `					pStream->nLine++;` |
|      ! 0 |  655 | `				}` |
|       19 |  656 | `				pStream->zText++;` |
|        1 |  657 | `			}` |
|        - |  658 | `			/* Record token length and type */` |
|        3 |  659 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|        3 |  660 | `			pToken->nType = PH7_TK_BSTR;` |
|        - |  661 | `			/* Jump the trailing backtick */` |
|        3 |  662 | `			pStream->zText++;` |
|        3 |  663 | `			return SXRET_OK;` |
|        - |  664 | `				  }` |
|     2780 |  665 | `		case '\\': pToken->nType = PH7_TK_NSSEP;  break;` |
|    87093 |  666 | `		case ':':` |
|   173933 |  667 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == ':' ){` |
|        - |  668 | `				/* Current operator: '::' */` |
|     4520 |  669 | `				pStream->zText++;` |
|     2254 |  670 | `			}else{` |
|   169418 |  671 | `				pToken->nType = PH7_TK_COLON; /* Single colon */` |
|        - |  672 | `			}` |
|   173933 |  673 | `			break;` |
|   642764 |  674 | `		case ',': pToken->nType \|= PH7_TK_COMMA;  break; /* Comma is also an operator */` |
|  1393157 |  675 | `		case ';': pToken->nType = PH7_TK_SEMI;    break;` |
|        - |  676 | `			/* Handle combined operators [i.e: +=,===,!=== ...] */` |
|   541694 |  677 | `		case '=':` |
|  1081631 |  678 | `			pToken->nType \|= PH7_TK_EQUAL;` |
|  1081631 |  679 | `			if( pStream->zText < pStream->zEnd ){` |
|  1081631 |  680 | `				if( pStream->zText[0] == '=' ){` |
|   300519 |  681 | `					pToken->nType &= ~PH7_TK_EQUAL;` |
|        - |  682 | `					/* Current operator: == */` |
|   300519 |  683 | `					pStream->zText++;` |
|   300519 |  684 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  685 | `						/* Current operator: === */` |
|   232707 |  686 | `						pStream->zText++;` |
|   116181 |  687 | `					}` |
|   931154 |  688 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  689 | `					/* Array operator: => */` |
|    32986 |  690 | `					pToken->nType = PH7_TK_ARRAY_OP;` |
|    32986 |  691 | `					pStream->zText++;` |
|    16358 |  692 | `				}else{` |
|        - |  693 | `					/* TICKET 1433-0010: Reference operator '=&' */` |
|   748136 |  694 | `					const unsigned char *zCur = pStream->zText;` |
|   748136 |  695 | `					sxu32 nLine = 0;` |
|  1495695 |  696 | `					while( zCur < pStream->zEnd && zCur[0] < 0xc0 && SyisSpace(zCur[0]) ){` |
|   747564 |  697 | `						if( zCur[0] == '\n' ){` |
|       10 |  698 | `							nLine++;` |
|        4 |  699 | `						}` |
|   747564 |  700 | `						zCur++;` |
|        5 |  701 | `					}` |
|   748136 |  702 | `					if( zCur < pStream->zEnd && zCur[0] == '&' ){` |
|        - |  703 | `						/* Current operator: =& */` |
|      433 |  704 | `						pToken->nType &= ~PH7_TK_EQUAL;` |
|      433 |  705 | `						SyStringInitFromBuf(pStr,"=&",sizeof("=&")-1);` |
|        - |  706 | `						/* Update token stream */` |
|      433 |  707 | `						pStream->zText = &zCur[1];` |
|      433 |  708 | `						pStream->nLine += nLine;` |
|      214 |  709 | `					}` |
|        - |  710 | `				}` |
|   539932 |  711 | `			}` |
|  1081631 |  712 | `			break;` |
|    98498 |  713 | `		case '!':` |
|   196734 |  714 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  715 | `				/* Current operator: != */` |
|   135238 |  716 | `				pStream->zText++;` |
|   135238 |  717 | `				if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  718 | `					/* Current operator: !== */` |
|   108318 |  719 | `					pStream->zText++;` |
|    54083 |  720 | `				}` |
|    67525 |  721 | `			}` |
|   196734 |  722 | `			break;` |
|   112543 |  723 | `		case '&':` |
|   224777 |  724 | `			pToken->nType \|= PH7_TK_AMPER;` |
|   224777 |  725 | `			if( pStream->zText < pStream->zEnd ){` |
|   224777 |  726 | `				if( pStream->zText[0] == '&' ){` |
|   115016 |  727 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|        - |  728 | `					/* Current operator: && */` |
|   115016 |  729 | `					pStream->zText++;` |
|   167192 |  730 | `				}else if( pStream->zText[0] == '=' ){` |
|      407 |  731 | `					pToken->nType &= ~PH7_TK_AMPER;` |
|        - |  732 | `					/* Current operator: &= */` |
|      407 |  733 | `					pStream->zText++;` |
|      203 |  734 | `				}` |
|   112229 |  735 | `			}` |
|   224777 |  736 | `			break;` |
|    64917 |  737 | `		case '\|':` |
|   129667 |  738 | `			if( pStream->zText < pStream->zEnd ){` |
|   129667 |  739 | `				if( pStream->zText[0] == '\|' ){` |
|        - |  740 | `					/* Current operator: \|\| */` |
|    81040 |  741 | `					pStream->zText++;` |
|    89095 |  742 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  743 | `					/* Current operator: \|= */` |
|      415 |  744 | `					pStream->zText++;` |
|    48425 |  745 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  746 | `					/* Current operator: \|> (PHP 8.5 pipe) */` |
|       27 |  747 | `					pStream->zText++;` |
|       13 |  748 | `				}` |
|    64745 |  749 | `			}` |
|   129667 |  750 | `			break;` |
|    48771 |  751 | `		case '+':` |
|    97412 |  752 | `			if( pStream->zText < pStream->zEnd ){` |
|    97412 |  753 | `				if( pStream->zText[0] == '+' ){` |
|        - |  754 | `					/* Current operator: ++ */` |
|    34616 |  755 | `					pStream->zText++;` |
|    80083 |  756 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  757 | `					/* Current operator: += */` |
|     7344 |  758 | `					pStream->zText++;` |
|     3664 |  759 | `				}` |
|    48636 |  760 | `			}` |
|    97412 |  761 | `			break;` |
|    71703 |  762 | `		case '-':` |
|   143214 |  763 | `			if( pStream->zText < pStream->zEnd ){` |
|   143214 |  764 | `				if( pStream->zText[0] == '-' ){` |
|        - |  765 | `					/* Current operator: -- */` |
|    26970 |  766 | `					pStream->zText++;` |
|   129713 |  767 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  768 | `					/* Current operator: -= */` |
|      412 |  769 | `					pStream->zText++;` |
|   116044 |  770 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  771 | `					/* Current operator: -> */` |
|    31230 |  772 | `					pStream->zText++;` |
|    15592 |  773 | `				}` |
|    71506 |  774 | `			}` |
|   143214 |  775 | `			break;` |
|     4496 |  776 | `		case '*':` |
|     8985 |  777 | `			if( pStream->zText < pStream->zEnd ){` |
|     8985 |  778 | `				if( pStream->zText[0] == '*' ){` |
|        - |  779 | `					/* Current operator: ** or **= */` |
|     1034 |  780 | `					pStream->zText++;` |
|     1034 |  781 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  782 | `						/* Current operator: **= */` |
|      431 |  783 | `						pStream->zText++;` |
|      217 |  784 | `					}` |
|     8469 |  785 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  786 | `					/* Current operator: *= */` |
|      451 |  787 | `					pStream->zText++;` |
|      224 |  788 | `				}` |
|     4484 |  789 | `			}` |
|     8985 |  790 | `			break;` |
|     3846 |  791 | `		case '/':` |
|     7688 |  792 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  793 | `				/* Current operator: /= */` |
|      423 |  794 | `				pStream->zText++;` |
|      211 |  795 | `			}` |
|     7688 |  796 | `			break;` |
|    13984 |  797 | `		case '%':` |
|    27937 |  798 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  799 | `				/* Current operator: %= */` |
|      409 |  800 | `				pStream->zText++;` |
|      204 |  801 | `			}` |
|    27937 |  802 | `			break;` |
|      425 |  803 | `		case '^':` |
|      852 |  804 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  805 | `				/* Current operator: ^= */` |
|      407 |  806 | `				pStream->zText++;` |
|      203 |  807 | `			}` |
|      852 |  808 | `			break;` |
|   119436 |  809 | `		case '.':` |
|   238517 |  810 | `			if( pStream->zText + 1 < pStream->zEnd && pStream->zText[0] == '.' && pStream->zText[1] == '.' ){` |
|        - |  811 | `				/* Ellipsis: ... */` |
|     7750 |  812 | `				pStream->zText += 2;` |
|     7750 |  813 | `				pToken->nType = PH7_TK_ELLIPSIS;` |
|   234639 |  814 | `			}else if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  815 | `				/* Current operator: .= */` |
|    14357 |  816 | `				pStream->zText++;` |
|     7165 |  817 | `			}` |
|   238517 |  818 | `			break;` |
|    48250 |  819 | `		case '<':` |
|    96376 |  820 | `			if( pStream->zText < pStream->zEnd ){` |
|    96376 |  821 | `				if( pStream->zText[0] == '<' ){` |
|        - |  822 | `					/* Current operator: << */` |
|     1064 |  823 | `					pStream->zText++;` |
|     1064 |  824 | `					if( pStream->zText < pStream->zEnd ){` |
|     1064 |  825 | `						if( pStream->zText[0] == '=' ){` |
|        - |  826 | `							/* Current operator: <<= */` |
|      419 |  827 | `							pStream->zText++;` |
|      855 |  828 | `						}else if( pStream->zText[0] == '<' ){` |
|        - |  829 | `							/* Current Token: <<<  */` |
|      161 |  830 | `							pStream->zText++;` |
|        - |  831 | `							/* This may be the beginning of a Heredoc/Nowdoc string,try to delimit it */` |
|      161 |  832 | `							rc = LexExtractHeredoc(&(*pStream),&(*pToken));` |
|      161 |  833 | `							if( rc == SXRET_OK ){` |
|        - |  834 | `								/* Here/Now doc successfuly extracted */` |
|      161 |  835 | `								return SXRET_OK;` |
|        - |  836 | `							}` |
|      ! 0 |  837 | `						}` |
|      453 |  838 | `					}` |
|    95768 |  839 | `				}else if( pStream->zText[0] == '>' ){` |
|        - |  840 | `					/* Current operator: <> */` |
|        5 |  841 | `					pStream->zText++;` |
|    95315 |  842 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  843 | `					/* Current operator: <= or <=> */` |
|     7060 |  844 | `					pStream->zText++;` |
|     7060 |  845 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '>' ){` |
|        - |  846 | `						/* Current operator: <=> */` |
|      222 |  847 | `						pStream->zText++;` |
|      109 |  848 | `					}` |
|     3523 |  849 | `				}` |
|    48043 |  850 | `			}` |
|    96220 |  851 | `			break;` |
|    47793 |  852 | `		case '>':` |
|    95458 |  853 | `			if( pStream->zText < pStream->zEnd ){` |
|    95458 |  854 | `				if( pStream->zText[0] == '>' ){` |
|        - |  855 | `					/* Current operator: >> */` |
|    21022 |  856 | `					pStream->zText++;` |
|    21022 |  857 | `					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  858 | `						/* Current operator: >>= */` |
|      411 |  859 | `						pStream->zText++;` |
|      210 |  860 | `					}` |
|    84936 |  861 | `				}else if( pStream->zText[0] == '=' ){` |
|        - |  862 | `					/* Current operator: >= */` |
|    20307 |  863 | `					pStream->zText++;` |
|    10137 |  864 | `				}` |
|    47660 |  865 | `			}` |
|    95458 |  866 | `			break;` |
|    16271 |  867 | `		case '?':` |
|    32505 |  868 | `			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '?' ){` |
|        - |  869 | `				/* Null coalescing operator: ?? */` |
|      741 |  870 | `				pStream->zText++;` |
|      741 |  871 | `				if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){` |
|        - |  872 | `					/* Null coalescing assignment operator (PHP 7.4) */` |
|      199 |  873 | `					pStream->zText++;` |
|       97 |  874 | `				}` |
|    32137 |  875 | `			}else if( (pStream->zEnd - pStream->zText) >= 2` |
|    31769 |  876 | `				&& pStream->zText[0] == '-' && pStream->zText[1] == '>' ){` |
|        - |  877 | `				/* Nullsafe object operator (PHP 8.0): ?-> */` |
|      187 |  878 | `				pStream->zText += 2;` |
|       91 |  879 | `			}` |
|    32500 |  880 | `			break;` |
|    21113 |  881 | `		default:` |
|    42154 |  882 | `			break;` |
|        - |  883 | `		}` |
| 14171226 |  884 | `		if( pStr->nByte <= 0 ){` |
|        - |  885 | `			/* Record token length */` |
| 14170798 |  886 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  7073658 |  887 | `		}` |
| 14171226 |  888 | `		if( pToken->nType & PH7_TK_OP ){` |
|        - |  889 | `			const ph7_expr_op *pOp;` |
|        - |  890 | `			/* Check if the extracted token is an operator */` |
|  3313963 |  891 | `			pOp = PH7_ExprExtractOperator(pStr,(SyToken *)SySetPeek(pStream->pSet));` |
|  3313963 |  892 | `			if( pOp == 0 ){` |
|        - |  893 | `				/* Not an operator */` |
|      ! 0 |  894 | `				pToken->nType &= ~PH7_TK_OP;` |
|      ! 0 |  895 | `				if( pToken->nType <= 0 ){` |
|      ! 0 |  896 | `					pToken->nType = PH7_TK_OTHER;` |
|      ! 0 |  897 | `				}` |
|      ! 0 |  898 | `			}else{` |
|        - |  899 | `				/* Save the instance associated with this operator for later processing */` |
|  3313963 |  900 | `				pToken->pUserData = (void *)pOp;` |
|        - |  901 | `			}` |
|  1654256 |  902 | `		}` |
|        - |  903 | `	}` |
|        - |  904 | `	/* Tell the upper-layer to save the extracted token for later processing */` |
| 20991940 |  905 | `	return SXRET_OK;` |
| 11379799 |  906 | `}` |
|        - |  907 | `/* SPDX-SnippetBegin */` |
|        - |  908 | `/* SPDX-SnippetCopyrightText: SQLite mkkeywordhash.c (D. Richard Hipp and the SQLite authors <https://sqlite.org/>); adapted for the PH7 engine by Chems mrad */` |
|        - |  909 | `/* SPDX-License-Identifier: blessing */` |
|        - |  910 | `/***** This file contains automatically generated code ******` |
|        - |  911 | `**` |
|        - |  912 | `** The code in this file has been automatically generated by` |
|        - |  913 | `**` |
|        - |  914 | `**     $Header: /sqlite/sqlite/tool/mkkeywordhash.c` |
|        - |  915 | `**` |
|        - |  916 | `** Sligthly modified by Chems mrad <chm@symisc.net> for the PH7 engine.` |
|        - |  917 | `**` |
|        - |  918 | `** The code in this file implements a function that determines whether` |
|        - |  919 | `** or not a given identifier is really a PHP keyword.  The same thing` |
|        - |  920 | `** might be implemented more directly using a hand-written hash table.` |
|        - |  921 | `** But by using this automatically generated code, the size of the code` |
|        - |  922 | `** is substantially reduced.  This is important for embedded applications` |
|        - |  923 | `** on platforms with limited memory.` |
|        - |  924 | `*/` |
|        - |  925 | `/* Hash score: 103 */` |
|  5895391 |  926 | `static sxu32 KeywordCode(const char *z, int n){` |
|        - |  927 | `  /* zText[] encodes 532 bytes of keywords in 333 bytes */` |
|        - |  928 | `  /*   extendswitchprintegerequire_oncenddeclareturnamespacechobject      */` |
|        - |  929 | `  /*   hrowbooleandefaultrycaselfinalistaticlonewconstringlobaluse        */` |
|        - |  930 | `  /*   lseifloatvarrayANDIEchoUSECHOabstractclasscontinuendifunction      */` |
|        - |  931 | `  /*   diendwhilevaldoexitgotoimplementsinclude_oncemptyinstanceof        */` |
|        - |  932 | `  /*   interfacendforeachissetparentprivateprotectedpublicatchunset       */` |
|        - |  933 | `  /*   xorARRAYASArrayEXITUNSETXORbreak                                   */` |
|        - |  934 | `  static const char zText[332] = {` |
|        - |  935 | `    'e','x','t','e','n','d','s','w','i','t','c','h','p','r','i','n','t','e',` |
|        - |  936 | `    'g','e','r','e','q','u','i','r','e','_','o','n','c','e','n','d','d','e',` |
|        - |  937 | `    'c','l','a','r','e','t','u','r','n','a','m','e','s','p','a','c','e','c',` |
|        - |  938 | `    'h','o','b','j','e','c','t','h','r','o','w','b','o','o','l','e','a','n',` |
|        - |  939 | `    'd','e','f','a','u','l','t','r','y','c','a','s','e','l','f','i','n','a',` |
|        - |  940 | `    'l','i','s','t','a','t','i','c','l','o','n','e','w','c','o','n','s','t',` |
|        - |  941 | `    'r','i','n','g','l','o','b','a','l','u','s','e','l','s','e','i','f','l',` |
|        - |  942 | `    'o','a','t','v','a','r','r','a','y','A','N','D','I','E','c','h','o','U',` |
|        - |  943 | `    'S','E','C','H','O','a','b','s','t','r','a','c','t','c','l','a','s','s',` |
|        - |  944 | `    'c','o','n','t','i','n','u','e','n','d','i','f','u','n','c','t','i','o',` |
|        - |  945 | `    'n','d','i','e','n','d','w','h','i','l','e','v','a','l','d','o','e','x',` |
|        - |  946 | `    'i','t','g','o','t','o','i','m','p','l','e','m','e','n','t','s','i','n',` |
|        - |  947 | `    'c','l','u','d','e','_','o','n','c','e','m','p','t','y','i','n','s','t',` |
|        - |  948 | `    'a','n','c','e','o','f','i','n','t','e','r','f','a','c','e','n','d','f',` |
|        - |  949 | `    'o','r','e','a','c','h','i','s','s','e','t','p','a','r','e','n','t','p',` |
|        - |  950 | `    'r','i','v','a','t','e','p','r','o','t','e','c','t','e','d','p','u','b',` |
|        - |  951 | `    'l','i','c','a','t','c','h','u','n','s','e','t','x','o','r','A','R','R',` |
|        - |  952 | `    'A','Y','A','S','A','r','r','a','y','E','X','I','T','U','N','S','E','T',` |
|        - |  953 | `    'X','O','R','b','r','e','a','k'` |
|        - |  954 | `  };` |
|        - |  955 | `  static const unsigned char aHash[151] = {` |
|        - |  956 | `       0,   0,   4,  83,   0,  61,  39,  12,   0,  33,  77,   0,  48,` |
|        - |  957 | `       0,   2,  65,  67,   0,   0,   0,  47,   0,   0,  40,   0,  15,` |
|        - |  958 | `      74,   0,  51,   0,  76,   0,   0,  20,   0,   0,   0,  50,   0,` |
|        - |  959 | `      80,  34,   0,  36,   0,   0,  64,  16,   0,   0,  17,   0,   1,` |
|        - |  960 | `      19,  84,  66,   0,  43,  45,  78,   0,   0,  53,  56,   0,   0,` |
|        - |  961 | `       0,  23,  49,   0,   0,  13,  31,  54,   7,   0,   0,  25,   0,` |
|        - |  962 | `      72,  14,   0,  71,   0,  38,   6,   0,   0,   0,  73,   0,   0,` |
|        - |  963 | `       3,   0,  41,   5,  52,  57,  32,   0,  60,  63,   0,  69,  82,` |
|        - |  964 | `      30,   0,  79,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,` |
|        - |  965 | `       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  81,   0,   0,` |
|        - |  966 | `      62,   0,  11,   0,   0,  58,   0,   0,   0,   0,  59,  75,   0,` |
|        - |  967 | `       0,   0,   0,   0,   0,  35,  27,   0` |
|        - |  968 | `  };` |
|        - |  969 | `  static const unsigned char aNext[84] = {` |
|        - |  970 | `       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,` |
|        - |  971 | `       0,   0,   8,   0,   0,   0,  10,   0,   0,   0,   0,   0,   0,` |
|        - |  972 | `       0,   0,   0,   0,  28,   0,   0,   0,   0,   0,   0,   0,   0,` |
|        - |  973 | `       0,   0,   0,   0,   0,  44,   0,  18,   0,   0,   0,   0,   0,` |
|        - |  974 | `       0,  46,   0,  29,   0,   0,   0,  22,   0,   0,   0,   0,  26,` |
|        - |  975 | `       0,  21,  24,   0,   0,  68,   0,   0,   9,  37,   0,   0,   0,` |
|        - |  976 | `      42,   0,   0,   0,  70,  55` |
|        - |  977 | `  };` |
|        - |  978 | `  static const unsigned char aLen[84] = {` |
|        - |  979 | `       7,   9,   6,   5,   7,  12,   7,   2,  10,   7,   6,   9,   4,` |
|        - |  980 | `       6,   5,   7,   4,   3,   7,   3,   4,   4,   5,   4,   6,   5,` |
|        - |  981 | `       2,   3,   5,   6,   6,   3,   6,   4,   2,   5,   3,   5,   3,` |
|        - |  982 | `       3,   4,   3,   4,   8,   5,   2,   8,   5,   8,   3,   8,   5,` |
|        - |  983 | `       4,   2,   4,   4,  10,  12,   7,   5,  10,   9,   3,   6,  10,` |
|        - |  984 | `       3,   7,   2,   5,   6,   7,   9,   6,   5,   5,   3,   5,   2,` |
|        - |  985 | `       5,   4,   5,   3,   2,   5` |
|        - |  986 | `  };` |
|        - |  987 | `  static const sxu16 aOffset[84] = {` |
|        - |  988 | `       0,   3,   6,  12,  14,  20,  20,  21,  31,  34,  39,  44,  52,` |
|        - |  989 | `      55,  60,  65,  65,  70,  72,  78,  81,  83,  86,  90,  92,  97,` |
|        - |  990 | `     100, 100, 103, 106, 111, 117, 119, 119, 123, 124, 129, 130, 135,` |
|        - |  991 | `     137, 139, 143, 145, 149, 157, 159, 162, 169, 173, 181, 183, 186,` |
|        - |  992 | `     190, 194, 196, 200, 204, 214, 214, 225, 230, 240, 240, 248, 248,` |
|        - |  993 | `     251, 251, 252, 258, 263, 269, 276, 285, 290, 295, 300, 303, 308,` |
|        - |  994 | `     310, 315, 319, 324, 325, 327` |
|        - |  995 | `  };` |
|        - |  996 | `  static const sxu32 aCode[84] = {` |
|        - |  997 | `    PH7_TKWRD_EXTENDS,   PH7_TKWRD_ENDSWITCH,   PH7_TKWRD_SWITCH,    PH7_TKWRD_PRINT,   PH7_TKWRD_INT,` |
|        - |  998 | `    PH7_TKWRD_REQONCE,   PH7_TKWRD_REQUIRE,     PH7_TK_ID /* 'eq' PH7-ism removed */, PH7_TKWRD_ENDDEC, PH7_TKWRD_DECLARE,` |
|        - |  999 | `    PH7_TKWRD_RETURN,    PH7_TKWRD_NAMESPACE,   PH7_TKWRD_ECHO,      PH7_TKWRD_OBJECT,    PH7_TKWRD_THROW,` |
|        - | 1000 | `    PH7_TKWRD_BOOL,      PH7_TKWRD_BOOL,        PH7_TKWRD_AND,       PH7_TKWRD_DEFAULT,   PH7_TKWRD_TRY,` |
|        - | 1001 | `    PH7_TKWRD_CASE,      PH7_TKWRD_SELF,        PH7_TKWRD_FINAL,     PH7_TKWRD_LIST,      PH7_TKWRD_STATIC,` |
|        - | 1002 | `    PH7_TKWRD_CLONE,     PH7_TK_ID /* 'ne' PH7-ism removed */, PH7_TKWRD_NEW,  PH7_TKWRD_CONST,     PH7_TKWRD_STRING,` |
|        - | 1003 | `    PH7_TKWRD_GLOBAL,    PH7_TKWRD_USE,         PH7_TKWRD_ELIF,      PH7_TKWRD_ELSE,      PH7_TKWRD_IF,` |
|        - | 1004 | `    PH7_TKWRD_FLOAT,     PH7_TKWRD_VAR,         PH7_TKWRD_ARRAY,     PH7_TKWRD_AND,       PH7_TKWRD_DIE,` |
|        - | 1005 | `    PH7_TKWRD_ECHO,      PH7_TKWRD_USE,         PH7_TKWRD_ECHO,      PH7_TKWRD_ABSTRACT,  PH7_TKWRD_CLASS,` |
|        - | 1006 | `    PH7_TKWRD_AS,        PH7_TKWRD_CONTINUE,    PH7_TKWRD_ENDIF,     PH7_TKWRD_FUNCTION,  PH7_TKWRD_DIE,` |
|        - | 1007 | `    PH7_TKWRD_ENDWHILE,  PH7_TKWRD_WHILE,       PH7_TKWRD_EVAL,      PH7_TKWRD_DO,        PH7_TKWRD_EXIT,` |
|        - | 1008 | `    PH7_TKWRD_GOTO,      PH7_TKWRD_IMPLEMENTS,  PH7_TKWRD_INCONCE,   PH7_TKWRD_INCLUDE,   PH7_TKWRD_EMPTY,` |
|        - | 1009 | `    PH7_TKWRD_INSTANCEOF,PH7_TKWRD_INTERFACE,   PH7_TKWRD_INT,       PH7_TKWRD_ENDFOR,    PH7_TKWRD_END4EACH,` |
|        - | 1010 | `    PH7_TKWRD_FOR,       PH7_TKWRD_FOREACH,     PH7_TKWRD_OR,        PH7_TKWRD_ISSET,     PH7_TKWRD_PARENT,` |
|        - | 1011 | `    PH7_TKWRD_PRIVATE,   PH7_TKWRD_PROTECTED,   PH7_TKWRD_PUBLIC,    PH7_TKWRD_CATCH,     PH7_TKWRD_UNSET,` |
|        - | 1012 | `    PH7_TKWRD_XOR,       PH7_TKWRD_ARRAY,       PH7_TKWRD_AS,        PH7_TKWRD_ARRAY,     PH7_TKWRD_EXIT,` |
|        - | 1013 | `    PH7_TKWRD_UNSET,     PH7_TKWRD_XOR,         PH7_TKWRD_OR,        PH7_TKWRD_BREAK` |
|        - | 1014 | `  };` |
|        - | 1015 | `  int h, i;` |
|  5895391 | 1016 | `  if( n<2 ) return PH7_TK_ID;` |
|        - | 1017 | ``  /* Hash through UNSIGNED bytes: `char` is signed on most targets, so an`` |
|        - | 1018 | `   * identifier carrying a high byte (php allows 0x80-0xFF in identifiers, and` |
|        - | 1019 | ``   * every UTF-8 name has them) made the xor negative, and C's `%` keeps that`` |
|        - | 1020 | `   * sign — aHash[-46] read off the front of the table. ASCII is unaffected, so` |
|        - | 1021 | `   * the generated keyword buckets still resolve exactly as before. */` |
|  5895391 | 1022 | `  h = (int)(((sxu32)(sxu8)z[0]*4) ^ ((sxu32)(sxu8)z[n-1]*3) ^ (sxu32)n) % 151;` |
|  8644711 | 1023 | `  for(i=((int)aHash[h])-1; i>=0; i=((int)aNext[i])-1){` |
|  5053913 | 1024 | `    if( (int)aLen[i]==n && SyMemcmp(&zText[aOffset[i]],z,n)==0 ){` |
|        - | 1025 | `       /* PH7_TKWRD_EXTENDS */` |
|        - | 1026 | `       /* PH7_TKWRD_ENDSWITCH */` |
|        - | 1027 | `       /* PH7_TKWRD_SWITCH */` |
|        - | 1028 | `       /* PH7_TKWRD_PRINT */` |
|        - | 1029 | `       /* PH7_TKWRD_INT */` |
|        - | 1030 | `       /* PH7_TKWRD_REQONCE */` |
|        - | 1031 | `       /* PH7_TKWRD_REQUIRE */` |
|        - | 1032 | `       /* PH7_TK_ID */` |
|        - | 1033 | `       /* PH7_TKWRD_ENDDEC */` |
|        - | 1034 | `       /* PH7_TKWRD_DECLARE */` |
|        - | 1035 | `       /* PH7_TKWRD_RETURN */` |
|        - | 1036 | `       /* PH7_TKWRD_NAMESPACE */` |
|        - | 1037 | `       /* PH7_TKWRD_ECHO */` |
|        - | 1038 | `       /* PH7_TKWRD_OBJECT */` |
|        - | 1039 | `       /* PH7_TKWRD_THROW */` |
|        - | 1040 | `       /* PH7_TKWRD_BOOL */` |
|        - | 1041 | `       /* PH7_TKWRD_BOOL */` |
|        - | 1042 | `       /* PH7_TKWRD_AND */` |
|        - | 1043 | `       /* PH7_TKWRD_DEFAULT */` |
|        - | 1044 | `       /* PH7_TKWRD_TRY */` |
|        - | 1045 | `       /* PH7_TKWRD_CASE */` |
|        - | 1046 | `       /* PH7_TKWRD_SELF */` |
|        - | 1047 | `       /* PH7_TKWRD_FINAL */` |
|        - | 1048 | `       /* PH7_TKWRD_LIST */` |
|        - | 1049 | `       /* PH7_TKWRD_STATIC */` |
|        - | 1050 | `       /* PH7_TKWRD_CLONE */` |
|        - | 1051 | `       /* PH7_TK_ID */` |
|        - | 1052 | `       /* PH7_TKWRD_NEW */` |
|        - | 1053 | `       /* PH7_TKWRD_CONST */` |
|        - | 1054 | `       /* PH7_TKWRD_STRING */` |
|        - | 1055 | `       /* PH7_TKWRD_GLOBAL */` |
|        - | 1056 | `       /* PH7_TKWRD_USE */` |
|        - | 1057 | `       /* PH7_TKWRD_ELIF */` |
|        - | 1058 | `       /* PH7_TKWRD_ELSE */` |
|        - | 1059 | `       /* PH7_TKWRD_IF */` |
|        - | 1060 | `       /* PH7_TKWRD_FLOAT */` |
|        - | 1061 | `       /* PH7_TKWRD_VAR */` |
|        - | 1062 | `       /* PH7_TKWRD_ARRAY */` |
|        - | 1063 | `       /* PH7_TKWRD_AND */` |
|        - | 1064 | `       /* PH7_TKWRD_DIE */` |
|        - | 1065 | `       /* PH7_TKWRD_ECHO */` |
|        - | 1066 | `       /* PH7_TKWRD_USE */` |
|        - | 1067 | `       /* PH7_TKWRD_ECHO */` |
|        - | 1068 | `       /* PH7_TKWRD_ABSTRACT */` |
|        - | 1069 | `       /* PH7_TKWRD_CLASS */` |
|        - | 1070 | `       /* PH7_TKWRD_AS */` |
|        - | 1071 | `       /* PH7_TKWRD_CONTINUE */` |
|        - | 1072 | `       /* PH7_TKWRD_ENDIF */` |
|        - | 1073 | `       /* PH7_TKWRD_FUNCTION */` |
|        - | 1074 | `       /* PH7_TKWRD_DIE */` |
|        - | 1075 | `       /* PH7_TKWRD_ENDWHILE */` |
|        - | 1076 | `       /* PH7_TKWRD_WHILE */` |
|        - | 1077 | `       /* PH7_TKWRD_EVAL */` |
|        - | 1078 | `       /* PH7_TKWRD_DO */` |
|        - | 1079 | `       /* PH7_TKWRD_EXIT */` |
|        - | 1080 | `       /* PH7_TKWRD_GOTO */` |
|        - | 1081 | `       /* PH7_TKWRD_IMPLEMENTS */` |
|        - | 1082 | `       /* PH7_TKWRD_INCONCE */` |
|        - | 1083 | `       /* PH7_TKWRD_INCLUDE */` |
|        - | 1084 | `       /* PH7_TKWRD_EMPTY */` |
|        - | 1085 | `       /* PH7_TKWRD_INSTANCEOF */` |
|        - | 1086 | `       /* PH7_TKWRD_INTERFACE */` |
|        - | 1087 | `       /* PH7_TKWRD_INT */` |
|        - | 1088 | `       /* PH7_TKWRD_ENDFOR */` |
|        - | 1089 | `       /* PH7_TKWRD_END4EACH */` |
|        - | 1090 | `       /* PH7_TKWRD_FOR */` |
|        - | 1091 | `       /* PH7_TKWRD_FOREACH */` |
|        - | 1092 | `       /* PH7_TKWRD_OR */` |
|        - | 1093 | `       /* PH7_TKWRD_ISSET */` |
|        - | 1094 | `       /* PH7_TKWRD_PARENT */` |
|        - | 1095 | `       /* PH7_TKWRD_PRIVATE */` |
|        - | 1096 | `       /* PH7_TKWRD_PROTECTED */` |
|        - | 1097 | `       /* PH7_TKWRD_PUBLIC */` |
|        - | 1098 | `       /* PH7_TKWRD_CATCH */` |
|        - | 1099 | `       /* PH7_TKWRD_UNSET */` |
|        - | 1100 | `       /* PH7_TKWRD_XOR */` |
|        - | 1101 | `       /* PH7_TKWRD_ARRAY */` |
|        - | 1102 | `       /* PH7_TKWRD_AS */` |
|        - | 1103 | `       /* PH7_TKWRD_ARRAY */` |
|        - | 1104 | `       /* PH7_TKWRD_EXIT */` |
|        - | 1105 | `       /* PH7_TKWRD_UNSET */` |
|        - | 1106 | `       /* PH7_TKWRD_XOR */` |
|        - | 1107 | `       /* PH7_TKWRD_OR */` |
|        - | 1108 | `       /* PH7_TKWRD_BREAK */` |
|  2304593 | 1109 | `      return aCode[i];` |
|        - | 1110 | `    }` |
|  1372359 | 1111 | `  }` |
|        - | 1112 | `  /* Linear fallback for keywords not in the auto-generated hash table */` |
|  3590803 | 1113 | `  if( n==5 && SyMemcmp(z,"trait",5)==0 ) return PH7_TKWRD_TRAIT;` |
|  3590469 | 1114 | `  if( n==9 && SyMemcmp(z,"insteadof",9)==0 ) return PH7_TKWRD_INSTEADOF;` |
|  3590455 | 1115 | `  if( n==7 && SyMemcmp(z,"finally",7)==0 ) return PH7_TKWRD_FINALLY;` |
|  3590137 | 1116 | `  if( n==5 && SyMemcmp(z,"yield",5)==0 ) return PH7_TKWRD_YIELD;` |
|  3589519 | 1117 | `  if( n==5 && SyMemcmp(z,"match",5)==0 ) return PH7_TKWRD_MATCH;` |
|  3589333 | 1118 | `  if( n==2 && SyMemcmp(z,"fn",2)==0 ) return PH7_TKWRD_FN;   /* PHP 7.4 arrow functions */` |
|  3579927 | 1119 | `  return PH7_TK_ID;` |
|  2943170 | 1120 | `}` |
|        - | 1121 | `/* --- End of Automatically generated code --- */` |
|        - | 1122 | `/* SPDX-SnippetEnd */` |
|        - | 1123 | `/*` |
|        - | 1124 | ` * Keyword lookup as php does it: CASE-INSENSITIVELY. 'IF', 'Function' and 'NEW'` |
|        - | 1125 | ` * are the very same tokens as 'if', 'function' and 'new', so the generated table` |
|        - | 1126 | ` * above — whose hash buckets and SyMemcmp() rows are byte-exact lower case — is` |
|        - | 1127 | ` * probed through an ASCII-folded COPY of the identifier. KeywordCode() itself` |
|        - | 1128 | ` * stays byte-exact so the generated code needs no regeneration.` |
|        - | 1129 | ` *` |
|        - | 1130 | ` * The fold is ASCII-only on purpose: libc tolower() follows LC_CTYPE (a tr_TR` |
|        - | 1131 | ` * embedder would stop recognising 'IF') where php's lexer is locale-independent,` |
|        - | 1132 | ` * and identifier bytes >= 0x80 — php allows them, and every UTF-8 name has` |
|        - | 1133 | ` * them — must pass through untouched.` |
|        - | 1134 | ` *` |
|        - | 1135 | ` * A handful of UPPER-case rows in the table ('ARRAY', 'AS', 'EXIT', 'UNSET',` |
|        - | 1136 | ` * 'XOR', 'AND', 'OR', 'ECHO', 'Echo', 'Array', 'USE') are PH7's old partial hack` |
|        - | 1137 | ` * for this same problem. Each has a lower-case twin, so folding makes them` |
|        - | 1138 | ` * unreachable-but-harmless rather than wrong.` |
|        - | 1139 | ` */` |
|  6820714 | 1140 | `static sxu32 KeywordCodeCI(const char *zRaw, int n)` |
|        5 | 1141 | `{` |
|        - | 1142 | `	/* Longest row in the table above: 'require_once'/'include_once' (12 bytes) */` |
|        - | 1143 | `	char zFold[12];` |
|        - | 1144 | `	int i;` |
|  6820719 | 1145 | `	if( n < 2 \|\| n > (int)sizeof(zFold) ){` |
|        - | 1146 | `		/* Too short or too long to be any keyword: skip the fold and the probe */` |
|   925333 | 1147 | `		return PH7_TK_ID;` |
|        - | 1148 | `	}` |
| 37513098 | 1149 | `	for( i = 0 ; i < n ; ++i ){` |
| 31617712 | 1150 | `		unsigned char c = (unsigned char)zRaw[i];` |
| 31617712 | 1151 | `		zFold[i] = (char)((c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c);` |
| 15784597 | 1152 | `	}` |
|  5895391 | 1153 | `	return KeywordCode(zFold,n);` |
|  3404931 | 1154 | `}` |
|        - | 1155 | `/*` |
|        - | 1156 | ` * Extract a heredoc/nowdoc text from a raw PHP input.` |
|        - | 1157 | ` * According to the PHP language reference manual:` |
|        - | 1158 | ` *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier` |
|        - | 1159 | ` *  is provided, then a newline. The string itself follows, and then the same identifier again` |
|        - | 1160 | ` *  to close the quotation.` |
|        - | 1161 | ` *  The closing identifier must begin in the first column of the line. Also, the identifier must` |
|        - | 1162 | ` *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric` |
|        - | 1163 | ` *  characters and underscores, and must start with a non-digit character or underscore.` |
|        - | 1164 | ` *  Heredoc text behaves just like a double-quoted string, without the double quotes.` |
|        - | 1165 | ` *  This means that quotes in a heredoc do not need to be escaped, but the escape codes listed` |
|        - | 1166 | ` *  above can still be used. Variables are expanded, but the same care must be taken when expressing` |
|        - | 1167 | ` *  complex variables inside a heredoc as with strings.` |
|        - | 1168 | ` *  Nowdocs are to single-quoted strings what heredocs are to double-quoted strings.` |
|        - | 1169 | ` *  A nowdoc is specified similarly to a heredoc, but no parsing is done inside a nowdoc.` |
|        - | 1170 | ` *  The construct is ideal for embedding PHP code or other large blocks of text without the need` |
|        - | 1171 | ` *  for escaping. It shares some features in common with the SGML <![CDATA[ ]]> construct, in that` |
|        - | 1172 | ` *  it declares a block of text which is not for parsing.` |
|        - | 1173 | ` *  A nowdoc is identified with the same <<< sequence used for heredocs, but the identifier which follows` |
|        - | 1174 | ` *  is enclosed in single quotes, e.g. <<<'EOT'. All the rules for heredoc identifiers also apply to nowdoc` |
|        - | 1175 | ` *  identifiers, especially those regarding the appearance of the closing identifier.` |
|        - | 1176 | ` * Symisc Extension:` |
|        - | 1177 | ` * The closing delimiter can now start with a digit or undersocre or it can be an UTF-8 stream.` |
|        - | 1178 | ` * Example:` |
|        - | 1179 | ` *  <<<123` |
|        - | 1180 | ` *    HEREDOC Here` |
|        - | 1181 | ` * 123` |
|        - | 1182 | ` *  or` |
|        - | 1183 | ` *  <<<___` |
|        - | 1184 | ` *   HEREDOC Here` |
|        - | 1185 | ` *  ___` |
|        - | 1186 | ` */` |
|      156 | 1187 | `static sxi32 LexExtractHeredoc(SyStream *pStream,SyToken *pToken)` |
|        5 | 1188 | `{` |
|      161 | 1189 | `	const unsigned char *zIn  = pStream->zText;` |
|      161 | 1190 | `	const unsigned char *zEnd = pStream->zEnd;` |
|        - | 1191 | `	const unsigned char *zPtr;` |
|      161 | 1192 | `	sxu8 bNowDoc = FALSE;` |
|      161 | 1193 | `	sxu8 bUnterm = FALSE;` |
|        - | 1194 | `	SyString sDelim;` |
|        - | 1195 | `	SyString sStr;` |
|        - | 1196 | `	/* Jump leading white spaces */` |
|      179 | 1197 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|       20 | 1198 | `		zIn++;` |
|        2 | 1199 | `	}` |
|      161 | 1200 | `	if( zIn >= zEnd ){` |
|        - | 1201 | `		/* A simple symbol,return immediately */` |
|      ! 0 | 1202 | `		return SXERR_CONTINUE;` |
|        - | 1203 | `	}` |
|      161 | 1204 | `	if( zIn[0] == '\'' \|\| zIn[0] == '"' ){` |
|        - | 1205 | `		/* Make sure we are dealing with a nowdoc */` |
|       67 | 1206 | `		bNowDoc =  zIn[0] == '\'' ? TRUE : FALSE;` |
|       67 | 1207 | `		zIn++;` |
|       31 | 1208 | `	}` |
|      161 | 1209 | `	if( !LEX_LABEL_BYTE(zIn[0]) ){` |
|        - | 1210 | `		/* Invalid delimiter,return immediately */` |
|      ! 0 | 1211 | `		return SXERR_CONTINUE;` |
|        - | 1212 | `	}` |
|        - | 1213 | `	/* Isolate the identifier (php's label bytes; see LEX_LABEL_START) */` |
|      161 | 1214 | `	sDelim.zString = (const char *)zIn;` |
|      161 | 1215 | `	zPtr = zIn;` |
|      817 | 1216 | `	while( zPtr < zEnd && LEX_LABEL_BYTE(zPtr[0]) ){` |
|      583 | 1217 | `		zPtr++;` |
|        5 | 1218 | `	}` |
|      161 | 1219 | `	zIn = zPtr;` |
|        - | 1220 | `	/* Get the identifier length */` |
|      161 | 1221 | `	sDelim.nByte = (sxu32)((const char *)zIn-sDelim.zString);` |
|      161 | 1222 | `	if( zIn[0] == '"' \|\| (bNowDoc && zIn[0] == '\'') ){` |
|        - | 1223 | `		/* Jump the trailing single quote */` |
|       67 | 1224 | `		zIn++;` |
|       31 | 1225 | `	}` |
|        - | 1226 | `	/* Jump trailing white spaces */` |
|      161 | 1227 | `	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|      ! 0 | 1228 | `		zIn++;` |
|      ! 0 | 1229 | `	}` |
|      161 | 1230 | `	if( sDelim.nByte > 0 && zIn >= zEnd ){` |
|        - | 1231 | ``		/* `<<<EOT` and then the end of the input: php has a heredoc with nothing in`` |
|        - | 1232 | `		 * it and no closing marker, and refuses the file for that. Form the token so` |
|        - | 1233 | ``		 * the compile phase can say so; without this the `<<<` fell through to the`` |
|        - | 1234 | `		 * operator table and named itself instead. */` |
|        2 | 1235 | `		SyStringInitFromBuf(&sStr,(const char *)zIn,0);` |
|        2 | 1236 | `		pStream->zText = zEnd;` |
|        2 | 1237 | `		pToken->nType = (bNowDoc ? PH7_TK_NOWDOC : PH7_TK_HEREDOC)\|PH7_TK_UNTERM;` |
|        2 | 1238 | `		SyStringDupPtr(&pToken->sData,&sStr);` |
|        2 | 1239 | `		pToken->pUserData = SX_INT_TO_PTR(0);` |
|        2 | 1240 | `		return SXRET_OK;` |
|        - | 1241 | `	}` |
|      159 | 1242 | `	if( sDelim.nByte <= 0 \|\| zIn >= zEnd \|\| zIn[0] != '\n' ){` |
|        - | 1243 | `		/* Invalid syntax */` |
|      ! 0 | 1244 | `		return SXERR_CONTINUE;` |
|        - | 1245 | `	}` |
|      159 | 1246 | `	pStream->nLine++; /* Increment line counter */` |
|      159 | 1247 | `	zIn++;` |
|        - | 1248 | `	/* Isolate the delimited string */` |
|      159 | 1249 | `	sStr.zString = (const char *)zIn;` |
|        - | 1250 | `	/* PHP 7.3 flexible heredoc/nowdoc: the closing marker may be preceded` |
|        - | 1251 | `	 * by whitespace (spaces/tabs), and may be followed by any non-identifier` |
|        - | 1252 | `	 * character. The indent count is recorded in pToken->pUserData and the` |
|        - | 1253 | `	 * compile phase strips it from each body line. */` |
|        - | 1254 | `	{` |
|      159 | 1255 | `		const unsigned char *zMarkerLine = zIn; /* Start of marker's line (set on match) */` |
|      159 | 1256 | `		sxu32 nIndent = 0;` |
|      343 | 1257 | `		for(;;){` |
|      425 | 1258 | `			const unsigned char *zLineStart = zIn;` |
|        - | 1259 | `			/* Skip leading space/tab on this line */` |
|     1133 | 1260 | `			while( zIn < zEnd && (zIn[0] == ' ' \|\| zIn[0] == '\t') ){` |
|      503 | 1261 | `				zIn++;` |
|        5 | 1262 | `			}` |
|      420 | 1263 | `			if( (sxu32)(zEnd - zIn) >= sDelim.nByte` |
|      422 | 1264 | `				&& SyMemcmp((const void *)sDelim.zString,(const void *)zIn,sDelim.nByte) == 0 ){` |
|        - | 1265 | `				int bIdentCont;` |
|      151 | 1266 | `				zPtr = &zIn[sDelim.nByte];` |
|        - | 1267 | `				/* Disambiguate: the next byte must not continue an identifier` |
|        - | 1268 | `				 * (php's label bytes; see LEX_LABEL_START). */` |
|      224 | 1269 | `				bIdentCont = zPtr < zEnd && LEX_LABEL_BYTE(zPtr[0]);` |
|      151 | 1270 | `				if( !bIdentCont ){` |
|        - | 1271 | `					/* Closing marker found */` |
|      151 | 1272 | `					nIndent = (sxu32)(zIn - zLineStart);` |
|      151 | 1273 | `					zMarkerLine = zLineStart;` |
|      151 | 1274 | `					pStream->zText = zPtr; /* Cursor right after identifier */` |
|      151 | 1275 | `					break;` |
|        - | 1276 | `				}` |
|      ! 0 | 1277 | `			}` |
|        - | 1278 | `			/* Not the closing marker on this line; walk to next newline */` |
|     5963 | 1279 | `			while( zIn < zEnd && zIn[0] != '\n' ){` |
|     5689 | 1280 | `				zIn++;` |
|        5 | 1281 | `			}` |
|      279 | 1282 | `			if( zIn >= zEnd ){` |
|        - | 1283 | `				/* End of input without finding the closing marker: php refuses the` |
|        - | 1284 | `				 * file, where this took the rest of it as the body. */` |
|        9 | 1285 | `				pStream->zText = pStream->zEnd;` |
|        9 | 1286 | `				zMarkerLine = zIn;` |
|        9 | 1287 | `				bUnterm = TRUE;` |
|        9 | 1288 | `				break;` |
|        - | 1289 | `			}` |
|      271 | 1290 | `			pStream->nLine++;` |
|      271 | 1291 | `			zIn++;` |
|        5 | 1292 | `		}` |
|        - | 1293 | `		/* Body runs from sStr.zString up to just before the marker line */` |
|      159 | 1294 | `		sStr.nByte = (sxu32)((const char *)zMarkerLine - sStr.zString);` |
|      159 | 1295 | `		pToken->nType = bNowDoc ? PH7_TK_NOWDOC : PH7_TK_HEREDOC;` |
|      159 | 1296 | `		if( bUnterm ){` |
|        9 | 1297 | `			pToken->nType \|= PH7_TK_UNTERM;` |
|        4 | 1298 | `		}` |
|      159 | 1299 | `		SyStringDupPtr(&pToken->sData,&sStr);` |
|        - | 1300 | `		/* Strip exactly one line terminator that precedes the marker's line. */` |
|      154 | 1301 | `		if( pToken->sData.nByte > 0` |
|      155 | 1302 | `			&& pToken->sData.zString[pToken->sData.nByte - 1] == '\n' ){` |
|      143 | 1303 | `			pToken->sData.nByte--;` |
|      138 | 1304 | `			if( pToken->sData.nByte > 0` |
|      143 | 1305 | `				&& pToken->sData.zString[pToken->sData.nByte - 1] == '\r' ){` |
|      ! 0 | 1306 | `				pToken->sData.nByte--;` |
|      ! 0 | 1307 | `			}` |
|       69 | 1308 | `		}` |
|      159 | 1309 | `		pToken->pUserData = SX_INT_TO_PTR(nIndent);` |
|        - | 1310 | `	}` |
|        - | 1311 | `	/* All done */` |
|      159 | 1312 | `	return SXRET_OK;` |
|       83 | 1313 | `}` |
|        - | 1314 | `/*` |
|        - | 1315 | ` * Tokenize a raw PHP input.` |
|        - | 1316 | ` * This is the public tokenizer called by most code generator routines.` |
|        - | 1317 | ` */` |
|    37558 | 1318 | `PH7_PRIVATE sxi32 PH7_TokenizePHP(const char *zInput,sxu32 nLen,sxu32 nLineStart,SySet *pOut,SySet *pTrivia)` |
|        5 | 1319 | `{` |
|        - | 1320 | `	SyLex sLexer;` |
|        - | 1321 | `	sxi32 rc;` |
|        - | 1322 | `	/* Defense-in-depth cap for internal tokenizer calls that bypass ph7_compile() */` |
|    37563 | 1323 | `	if( nLen > PH7_MAX_INPUT_SIZE ){` |
|      ! 0 | 1324 | `		return SXERR_LIMIT;` |
|        - | 1325 | `	}` |
|        - | 1326 | `	/* Initialize the lexer. pTrivia (may be NULL = discard) rides as the` |
|        - | 1327 | `	 * tokenizer callback's user data: doc-comments (and later attribute` |
|        - | 1328 | `	 * groups) are recorded there instead of entering the token stream. */` |
|    37563 | 1329 | `	rc = SyLexInit(&sLexer,&(*pOut),TokenizePHP,pTrivia);` |
|    37563 | 1330 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1331 | `		return rc;` |
|        - | 1332 | `	}` |
|    37563 | 1333 | `	sLexer.sStream.nLine = nLineStart;` |
|        - | 1334 | `	/* Tokenize input */` |
|    37563 | 1335 | `	rc = SyLexTokenizeInput(&sLexer,zInput,nLen,0,0,0);` |
|        - | 1336 | `	/* Release the lexer */` |
|    37563 | 1337 | `	SyLexRelease(&sLexer);` |
|        - | 1338 | `	/* Tokenization result */` |
|    37563 | 1339 | `	return rc;` |
|    18757 | 1340 | `}` |
|        - | 1341 | `/*` |
|        - | 1342 | ` * High level public tokenizer.` |
|        - | 1343 | ` *  Tokenize the input into PHP tokens and raw tokens [i.e: HTML,XML,Raw text...].` |
|        - | 1344 | ` * According to the PHP language reference manual` |
|        - | 1345 | ` *   When PHP parses a file, it looks for opening and closing tags, which tell PHP` |
|        - | 1346 | ` *   to start and stop interpreting the code between them. Parsing in this manner allows` |
|        - | 1347 | ` *   PHP to be embedded in all sorts of different documents, as everything outside of a pair` |
|        - | 1348 | ` *   of opening and closing tags is ignored by the PHP parser. Most of the time you will see` |
|        - | 1349 | ` *   PHP embedded in HTML documents, as in this example.` |
|        - | 1350 | ` *   <?php echo 'While this is going to be parsed.'; ?>` |
|        - | 1351 | ` *   <p>This will also be ignored.</p>` |
|        - | 1352 | ` *   You can also use more advanced structures:` |
|        - | 1353 | ` *   Example #1 Advanced escaping` |
|        - | 1354 | ` * <?php` |
|        - | 1355 | ` * if ($expression) {` |
|        - | 1356 | ` *   ?>` |
|        - | 1357 | ` *   <strong>This is true.</strong>` |
|        - | 1358 | ` *   <?php` |
|        - | 1359 | ` * } else {` |
|        - | 1360 | ` *   ?>` |
|        - | 1361 | ` *   <strong>This is false.</strong>` |
|        - | 1362 | ` *   <?php` |
|        - | 1363 | ` * }` |
|        - | 1364 | ` * ?>` |
|        - | 1365 | ` * This works as expected, because when PHP hits the ?> closing tags, it simply starts outputting` |
|        - | 1366 | ` * whatever it finds (except for an immediately following newline - see instruction separation ) until it hits` |
|        - | 1367 | ` * another opening tag. The example given here is contrived, of course, but for outputting large blocks of text` |
|        - | 1368 | ` * dropping out of PHP parsing mode is generally more efficient than sending all of the text through echo() or print().` |
|        - | 1369 | ` * There are four different pairs of opening and closing tags which can be used in PHP. Three of those, <?php ?>` |
|        - | 1370 | ` * <script language="php"> </script>  and <? ?> are always available. The other two are short tags and ASP style` |
|        - | 1371 | ` * tags, and can be turned on and off from the php.ini configuration file. As such, while some people find short tags` |
|        - | 1372 | ` * and ASP style tags convenient, they are less portable, and generally not recommended.` |
|        - | 1373 | ` * Note:` |
|        - | 1374 | ` * Also note that if you are embedding PHP within XML or XHTML you will need to use the <?php ?> tags to remain` |
|        - | 1375 | ` * compliant with standards.` |
|        - | 1376 | ` * Example #2 PHP Opening and Closing Tags` |
|        - | 1377 | ` * 1.  <?php echo 'if you want to serve XHTML or XML documents, do it like this'; ?>` |
|        - | 1378 | ` * 2.  <script language="php">` |
|        - | 1379 | ` *       echo 'some editors (like FrontPage) don\'t` |
|        - | 1380 | ` *             like processing instructions';` |
|        - | 1381 | ` *   </script>` |
|        - | 1382 | ` *` |
|        - | 1383 | ` * 3.  <? echo 'this is the simplest, an SGML processing instruction'; ?>` |
|        - | 1384 | ` *   <?= expression ?> This is a shortcut for "<? echo expression ?>"` |
|        - | 1385 | ` */` |
|    28649 | 1386 | `PH7_PRIVATE sxi32 PH7_TokenizeRawText(const char *zInput,sxu32 nLen,SySet *pOut,sxu32 nBaseLine)` |
|        5 | 1387 | `{` |
|    28654 | 1388 | `	const char *zEnd = &zInput[nLen];` |
|    28654 | 1389 | `	const char *zIn  = zInput;` |
|        - | 1390 | `	const char *zCur,*zCurEnd;` |
|    28654 | 1391 | `	SyString sCtag = { 0, 0 };     /* Closing tag */` |
|        - | 1392 | `	SyToken sToken;` |
|        - | 1393 | `	SyString sDoc;` |
|        - | 1394 | `	sxu32 nLine;` |
|        - | 1395 | `	sxi32 iNest;` |
|        - | 1396 | `	sxi32 rc;` |
|        - | 1397 | `	/* Tokenize the input into PHP tokens and raw tokens. nBaseLine is normally 1,` |
|        - | 1398 | `	 * but 2 when a "#!" shebang line was stripped so error lines still match php. */` |
|    28654 | 1399 | `	nLine = nBaseLine;` |
|    28654 | 1400 | `	zCur = zCurEnd   = 0; /* Prevent compiler warning */` |
|    28654 | 1401 | `	sToken.pUserData = 0;` |
|    28654 | 1402 | `	iNest = 0;` |
|    28654 | 1403 | `	sDoc.nByte = 0;` |
|    28654 | 1404 | `	sDoc.zString = ""; /* cc warning */` |
|    28642 | 1405 | `	for(;;){` |
|    56639 | 1406 | `		if( zIn >= zEnd ){` |
|        - | 1407 | `			/* End of input reached */` |
|    27934 | 1408 | `			break;` |
|        - | 1409 | `		}` |
|    28710 | 1410 | `		sToken.nLine = nLine;` |
|    28710 | 1411 | `		zCur = zIn;` |
|    28710 | 1412 | `		zCurEnd = 0;` |
|    31060 | 1413 | `		while( zIn < zEnd ){` |
|    30340 | 1414 | `			 if( zIn[0] == '<' ){` |
|    27990 | 1415 | `				const char *zTmp = zIn; /* End of raw input marker */` |
|    27990 | 1416 | `				zIn++;` |
|    27990 | 1417 | `				if( zIn < zEnd ){` |
|    27990 | 1418 | `					if( zIn[0] == '?' ){` |
|    27990 | 1419 | `						zIn++;` |
|    27990 | 1420 | `						if( (sxu32)(zEnd - zIn) >= sizeof("php")-1 &&  SyStrnicmp(zIn,"php",sizeof("php")-1) == 0 ){` |
|        - | 1421 | `							/* opening tag: <?php */` |
|    27988 | 1422 | `							zIn += sizeof("php")-1;` |
|    13987 | 1423 | `						}` |
|        - | 1424 | `						/* Look for the closing tag '?>' */` |
|    27990 | 1425 | `						SyStringInitFromBuf(&sCtag,"?>",sizeof("?>")-1);` |
|    27990 | 1426 | `						zCurEnd = zTmp;` |
|    27990 | 1427 | `						break;` |
|        - | 1428 | `					}` |
|      ! 0 | 1429 | `				}` |
|      ! 0 | 1430 | `			}else{` |
|     2355 | 1431 | `				if( zIn[0] == '\n' ){` |
|       20 | 1432 | `					nLine++;` |
|        9 | 1433 | `				}` |
|     2355 | 1434 | `				zIn++;` |
|        - | 1435 | `			 }` |
|        5 | 1436 | `		} /* While(zIn < zEnd) */` |
|    28710 | 1437 | `		if( zCurEnd == 0 ){` |
|       57 | 1438 | `			zCurEnd = zIn;` |
|       26 | 1439 | `		}` |
|        - | 1440 | `		/* Save the raw token */` |
|    28710 | 1441 | `		SyStringInitFromBuf(&sToken.sData,zCur,zCurEnd - zCur);` |
|    28710 | 1442 | `		sToken.nType = PH7_TOKEN_RAW;` |
|    28710 | 1443 | `		rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|    28710 | 1444 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1445 | `			return rc;` |
|        - | 1446 | `		}` |
|    28710 | 1447 | `		if( zIn >= zEnd ){` |
|       57 | 1448 | `			break;` |
|        - | 1449 | `		}` |
|        - | 1450 | `		/* Ignore leading white space */` |
|    58223 | 1451 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    29570 | 1452 | `			if( zIn[0] == '\n' ){` |
|    19249 | 1453 | `				nLine++;` |
|     9618 | 1454 | `			}` |
|    29570 | 1455 | `			zIn++;` |
|        5 | 1456 | `		}` |
|        - | 1457 | `		/* Delimit the PHP chunk */` |
|    28658 | 1458 | `		sToken.nLine = nLine;` |
|    28658 | 1459 | `		zCur = zIn;` |
|  5394199 | 1460 | `		while( (sxu32)(zEnd - zIn) >= sCtag.nByte ){` |
|        - | 1461 | `			const char *zPtr;` |
|  5375528 | 1462 | `			if( SyMemcmp(zIn,sCtag.zString,sCtag.nByte) == 0 && iNest < 1 ){` |
|     9319 | 1463 | `				break;` |
|        - | 1464 | `			}` |
|        - | 1465 | `			/* Line comment ('#' or '//', but not the '#[' attribute opener): php` |
|        - | 1466 | `			 * ends it at a newline OR at the closing tag, so a '?>' inside a line` |
|        - | 1467 | `			 * comment DOES close the PHP block. Skipping the comment here also` |
|        - | 1468 | `			 * stops the string skip below from treating a quote inside the` |
|        - | 1469 | `			 * comment as a string. Only outside a heredoc body (iNest < 1). */` |
|  5376727 | 1470 | `			if( iNest < 1 &&` |
|  5360409 | 1471 | `				( (zIn[0] == '#' && !(zIn+1 < zEnd && zIn[1] == '[')) \|\|` |
|  5362576 | 1472 | `				  (zIn[0] == '/' && zIn+1 < zEnd && zIn[1] == '/') ) ){` |
|    21032 | 1473 | `				zIn += (zIn[0] == '#') ? 1 : 2;` |
|   817992 | 1474 | `				while( zIn < zEnd && zIn[0] != '\n' ){` |
|   796964 | 1475 | `					if( (sxu32)(zEnd - zIn) >= sCtag.nByte` |
|   796965 | 1476 | `						&& SyMemcmp(zIn,sCtag.zString,sCtag.nByte) == 0 ){` |
|        6 | 1477 | `						break; /* the closing tag terminates the line comment */` |
|        - | 1478 | `					}` |
|   796965 | 1479 | `					zIn++;` |
|        5 | 1480 | `				}` |
|    15354 | 1481 | `				continue;` |
|        - | 1482 | `			}` |
|        - | 1483 | `			/* Block comment: spans everything, including '?>', up to its close. */` |
|  5350197 | 1484 | `			if( iNest < 1 && zIn[0] == '/' && zIn+1 < zEnd && zIn[1] == '*' ){` |
|     4762 | 1485 | `				zIn += 2;` |
|   733006 | 1486 | `				while( (sxu32)(zEnd-zIn) >= sizeof("*/") - 1 ){` |
|   733002 | 1487 | `					if( zIn[0] == '*' && zIn[1] == '/' ){` |
|     4758 | 1488 | `						zIn += 2;` |
|     4758 | 1489 | `						break;` |
|        - | 1490 | `					}` |
|   728249 | 1491 | `					if( zIn[0] == '\n' ){` |
|     6764 | 1492 | `						nLine++;` |
|     3358 | 1493 | `					}` |
|   728249 | 1494 | `					zIn++;` |
|        5 | 1495 | `				}` |
|     4762 | 1496 | `				continue;` |
|        - | 1497 | `			}` |
|        - | 1498 | `			/* Skip over a single/double-quoted or backtick string literal so a` |
|        - | 1499 | `			 * '?>' sequence inside it is not mistaken for the closing tag. Only` |
|        - | 1500 | `			 * outside a heredoc body (iNest < 1); heredocs are delimited by the` |
|        - | 1501 | `			 * label-matching logic above. Escapes (\" \' \\ and a line-continuing` |
|        - | 1502 | `			 * backslash-newline) are honoured. Same-quote nesting inside "{$...}"` |
|        - | 1503 | `			 * interpolation is not tracked, but that can only end the skip early` |
|        - | 1504 | `			 * on a string that has no '?>' anyway, which stays a PHP chunk either` |
|        - | 1505 | `			 * way — it never mis-splits code that works today. */` |
|  5345440 | 1506 | ``			if( iNest < 1 && (zIn[0] == '\'' \|\| zIn[0] == '"' \|\| zIn[0] == '`') ){`` |
|   148221 | 1507 | `				int qch = zIn[0];` |
|   148221 | 1508 | `				zIn++;` |
|  1368696 | 1509 | `				while( zIn < zEnd ){` |
|  1368688 | 1510 | `					if( zIn[0] == '\\' && zIn + 1 < zEnd ){` |
|    42254 | 1511 | `						if( zIn[1] == '\n' ){ nLine++; }` |
|    42254 | 1512 | `						zIn += 2;` |
|    42254 | 1513 | `						continue;` |
|        - | 1514 | `					}` |
|  1326439 | 1515 | `					if( zIn[0] == qch ){ zIn++; break; }` |
|  1178231 | 1516 | `					if( zIn[0] == '\n' ){ nLine++; }` |
|  1178231 | 1517 | `					zIn++;` |
|        5 | 1518 | `				}` |
|   148221 | 1519 | `				continue;` |
|        - | 1520 | `			}` |
|  5197224 | 1521 | `			if( zIn[0] == '\n' ){` |
|   182485 | 1522 | `				nLine++;` |
|   182485 | 1523 | `				if( iNest > 0 ){` |
|      425 | 1524 | `					zIn++;` |
|      923 | 1525 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|      503 | 1526 | `						zIn++;` |
|        5 | 1527 | `					}` |
|      425 | 1528 | `					zPtr = zIn;` |
|     2213 | 1529 | `					while( zIn < zEnd && LEX_LABEL_BYTE(zIn[0]) ){` |
|     1583 | 1530 | `						zIn++;` |
|        5 | 1531 | `					}` |
|      425 | 1532 | `					if( (sxu32)(zIn - zPtr) == sDoc.nByte && SyMemcmp(sDoc.zString,zPtr,sDoc.nByte) == 0 ){` |
|      151 | 1533 | `						iNest = 0;` |
|       73 | 1534 | `					}` |
|      425 | 1535 | `					continue;` |
|        5 | 1536 | `				}` |
|  5105396 | 1537 | `			}else if ( (sxu32)(zEnd - zIn) >= sizeof("<<<") && zIn[0] == '<' && zIn[1] == '<' && zIn[2] == '<' && iNest < 1){` |
|      161 | 1538 | `				zIn += sizeof("<<<")-1;` |
|      179 | 1539 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){` |
|       20 | 1540 | `					zIn++;` |
|        2 | 1541 | `				}` |
|      161 | 1542 | `				if( zIn[0] == '"' \|\| zIn[0] == '\'' ){` |
|       67 | 1543 | `					zIn++;` |
|       31 | 1544 | `				}` |
|      161 | 1545 | `				zPtr = zIn;` |
|      817 | 1546 | `				while( zIn < zEnd && LEX_LABEL_BYTE(zIn[0]) ){` |
|      583 | 1547 | `					zIn++;` |
|        5 | 1548 | `				}` |
|      161 | 1549 | `				SyStringInitFromBuf(&sDoc,zPtr,zIn-zPtr);` |
|      161 | 1550 | `				SyStringFullTrim(&sDoc);` |
|      161 | 1551 | `				if( sDoc.nByte > 0 ){` |
|      161 | 1552 | `					iNest++;` |
|       78 | 1553 | `				}` |
|      161 | 1554 | `				continue;` |
|        - | 1555 | `			}` |
|  5196648 | 1556 | `			zIn++;` |
|        - | 1557 |  |
|  5196648 | 1558 | `			if ( zIn >= zEnd )` |
|      ! 0 | 1559 | `				break;` |
|        5 | 1560 | `		}` |
|    27990 | 1561 | `		if( (sxu32)(zEnd - zIn) < sCtag.nByte ){` |
|    18676 | 1562 | `			zIn = zEnd;` |
|     9334 | 1563 | `		}` |
|    27990 | 1564 | `		if( zCur < zIn ){` |
|        - | 1565 | `			/* Save the PHP chunk for later processing. pUserData records whether the` |
|        - | 1566 | ``			 * chunk was CLOSED by a `?>`: php reads that tag as the terminator of`` |
|        - | 1567 | ``			 * whatever statement was open (`<?php echo 1 ?>` is legal), and only a`` |
|        - | 1568 | `			 * chunk that ran into the end of the FILE leaves one unfinished. */` |
|    24014 | 1569 | `			sToken.nType = PH7_TOKEN_PHP;` |
|    24014 | 1570 | `			sToken.pUserData = SX_INT_TO_PTR(zIn < zEnd ? 1 : 0);` |
|    24014 | 1571 | `			SyStringInitFromBuf(&sToken.sData,zCur,zIn-zCur);` |
|    37044 | 1572 | `			SyStringRightTrim(&sToken.sData); /* Trim trailing white spaces */` |
|    24014 | 1573 | `			rc = SySetPut(&(*pOut),(const void *)&sToken);` |
|    24014 | 1574 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1575 | `				return rc;` |
|        - | 1576 | `			}` |
|    12000 | 1577 | `		}` |
|    27990 | 1578 | `		if( zIn < zEnd ){` |
|        - | 1579 | `			/* Jump the trailing closing tag */` |
|     9319 | 1580 | `			zIn += sCtag.nByte;` |
|        - | 1581 | `			/* php's lexer swallows exactly ONE newline immediately after the` |
|        - | 1582 | `			 * closing tag ("?>\n" emits nothing) */` |
|     9319 | 1583 | `			if( zIn < zEnd && zIn[0] == '\r' && zIn + 1 < zEnd && zIn[1] == '\n' ){` |
|        5 | 1584 | `				zIn += 2;` |
|        5 | 1585 | `				nLine++;` |
|     9317 | 1586 | `			}else if( zIn < zEnd && zIn[0] == '\n' ){` |
|      137 | 1587 | `				zIn++;` |
|      137 | 1588 | `				nLine++;` |
|       66 | 1589 | `			}` |
|     4654 | 1590 | `		}` |
|        5 | 1591 | `	} /* For(;;) */` |
|        - | 1592 |  |
|    27986 | 1593 | ` 	return SXRET_OK;` |
|    13991 | 1594 | `}` |
|        - | 1595 |  |
