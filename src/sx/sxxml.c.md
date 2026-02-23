# src/sx/sxxml.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 533/774 lines (68.86%)

[Root index](../../index.md) | [Directory index](index.md)

| Hits | Line | Source |
| ---: | ---: | :--- |
|    - |    1 | `/**` |
|    - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|    - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|    - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|    - |    5 | ` */` |
|    - |    6 | `#include "sxtypes.h"` |
|    - |    7 | `#include "sxmacros.h"` |
|    - |    8 | `#include "sxset.h"` |
|    - |    9 | `#include "sxmem.h"` |
|    - |   10 | `#include "sxhashtable.h"` |
|    - |   11 | `#include "sxlex.h"` |
|    - |   12 | `#include "sxxml.h"` |
|    - |   13 | `#include "sxstr.h"` |
|    - |   14 | `#include "sxutils.h"` |
|    - |   15 |  |
|    - |   16 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|    - |   17 | `/*` |
|    - |   18 | `* Symisc XML Parser Engine (UTF-8) SAX(Event Driven) API` |
|    - |   19 | `* @author Mrad Chems Eddine <chm@symisc.net>` |
|    - |   20 | `* @started 08/03/2010 21:32 FreeBSD` |
|    - |   21 | `* @finished	07/04/2010 23:24 Win32[VS8]` |
|    - |   22 | `*/` |
|    - |   23 | `/*` |
|    - |   24 | ` * An XML raw text,CDATA,tag name is parsed out and stored` |
|    - |   25 | ` * in an instance of the following structure.` |
|    - |   26 | ` */` |
|    - |   27 | `typedef struct SyXMLRawStrNS SyXMLRawStrNS;` |
|    - |   28 | `struct SyXMLRawStrNS` |
|    - |   29 |  |
|    - |   30 | `	/* Public field [Must match the SyXMLRawStr fields ] */` |
|    - |   31 | `	const char *zString; /* Raw text [UTF-8 ENCODED EXCEPT CDATA] [NOT NULL TERMINATED] */` |
|    - |   32 | `	sxu32 nByte; /* Text length */` |
|    - |   33 | `	sxu32 nLine; /* Line number this text occurs */` |
|    - |   34 | `	/* Private fields */` |
|    - |   35 | `	SySet sNSset; /* Namespace entries */` |
|    - |   36 | `};` |
|    - |   37 | `/*` |
|    - |   38 | ` * Lexer token codes` |
|    - |   39 | ` * The following set of constants are the token value recognized` |
|    - |   40 | ` * by the lexer when processing XML input.` |
|    - |   41 | ` */` |
|    - |   42 | `#define SXML_TOK_INVALID	0xFFFF /* Invalid Token */` |
|    - |   43 | `#define SXML_TOK_COMMENT	0x01   /* Comment */` |
|    - |   44 | `#define SXML_TOK_PI	        0x02   /* Processing instruction */` |
|    - |   45 | `#define SXML_TOK_DOCTYPE	0x04   /* Doctype directive */` |
|    - |   46 | `#define SXML_TOK_RAW		0x08   /* Raw text */` |
|    - |   47 | `#define SXML_TOK_START_TAG	0x10   /* Starting tag */` |
|    - |   48 | `#define SXML_TOK_CDATA		0x20   /* CDATA */` |
|    - |   49 | `#define SXML_TOK_END_TAG	0x40   /* Ending tag */` |
|    - |   50 | `#define SXML_TOK_START_END	0x80   /* Tag */` |
|    - |   51 | `#define SXML_TOK_SPACE		0x100  /* Spaces (including new lines) */` |
|    - |   52 | `#define IS_XML_DIRTY(c) \` |
|    - |   53 | `	( c == '<' \|\| c == '$'\|\| c == '"' \|\| c == '\''\|\| c == '&'\|\| c == '(' \|\| c == ')' \|\| c == '*' \|\|\` |
|    - |   54 | `	c == '%'  \|\| c == '#' \|\| c == '\|' \|\| c == '/'\|\| c == '~' \|\| c == '{' \|\| c == '}' \|\|\` |
|    - |   55 | ``	c == '['  \|\| c == ']' \|\| c == '\\'\|\| c == ';'\|\|c == '^'  \|\| c == '`' )`` |
|    - |   56 | `/* Tokenize an entire XML input */` |
|  396 |   57 | `static sxi32 XML_Tokenize(SyStream *pStream,SyToken *pToken,void *pUserData,void *pUnused2)` |
|    1 |   58 |  |
|  397 |   59 | `	SyXMLParser *pParse = (SyXMLParser *)pUserData;` |
|    - |   60 | `	SyString *pStr;` |
|    - |   61 | `	sxi32 rc;` |
|    - |   62 | `	int c;` |
|    - |   63 | `	/* Jump leading white spaces */` |
|  421 |   64 | `	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){` |
|    - |   65 | `		/* Advance the stream cursor */` |
|   25 |   66 | `		if( pStream->zText[0] == '\n' ){` |
|    - |   67 | `			/* Increment line counter */` |
|    9 |   68 | `			pStream->nLine++;` |
|    4 |   69 | `		}` |
|   25 |   70 | `		pStream->zText++;` |
|    1 |   71 | `	}` |
|  397 |   72 | `	if( pStream->zText >= pStream->zEnd ){` |
|  ! 0 |   73 | `		SXUNUSED(pUnused2);` |
|    - |   74 | `		/* End of input reached */` |
|  ! 0 |   75 | `		return SXERR_EOF;` |
|    - |   76 | `	}` |
|    - |   77 | `	/* Record token starting position and line */` |
|  397 |   78 | `	pToken->nLine = pStream->nLine;` |
|  397 |   79 | `	pToken->pUserData = 0;` |
|  397 |   80 | `	pStr = &pToken->sData;` |
|  397 |   81 | `	SyStringInitFromBuf(pStr,pStream->zText,0);` |
|    - |   82 | `	/* Extract the current token */` |
|  397 |   83 | `	c = pStream->zText[0];` |
|  397 |   84 | `	if( c == '<' ){` |
|  369 |   85 | `		pStream->zText++;` |
|  369 |   86 | `		pStr->zString++;` |
|  369 |   87 | `		if( pStream->zText >= pStream->zEnd ){` |
|    3 |   88 | `			if( pParse->xError ){` |
|    3 |   89 | `				rc = pParse->xError("Illegal syntax,expecting valid start name character",SXML_ERROR_SYNTAX,pToken,pParse->pUserData);` |
|    3 |   90 | `				if( rc == SXERR_ABORT ){` |
|    3 |   91 | `					return SXERR_ABORT;` |
|    - |   92 | `				}` |
|  ! 0 |   93 | `			}` |
|    - |   94 | `			/* End of input reached */` |
|  ! 0 |   95 | `			return SXERR_EOF;` |
|    - |   96 | `		}` |
|  367 |   97 | `		c = pStream->zText[0];` |
|  367 |   98 | `		if( c == '?' ){` |
|    - |   99 | `			/* Processing instruction */` |
|   19 |  100 | `			pStream->zText++;` |
|   19 |  101 | `			pStr->zString++;` |
|   19 |  102 | `			pToken->nType = SXML_TOK_PI;` |
|  611 |  103 | `			while( XLEX_IN_LEN(pStream) >= sizeof("?>")-1 &&` |
|  302 |  104 | `				SyMemcmp((const void *)pStream->zText,"?>",sizeof("?>")-1) != 0 ){` |
|  291 |  105 | `					if( pStream->zText[0] == '\n' ){` |
|    - |  106 | `						/* Increment line counter */` |
|    3 |  107 | `						pStream->nLine++;` |
|    1 |  108 | `					}` |
|  291 |  109 | `					pStream->zText++;` |
|    1 |  110 | `			}` |
|    - |  111 | `			/* Record token length */` |
|   19 |  112 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|   19 |  113 | `			if( XLEX_IN_LEN(pStream) < sizeof("?>")-1 ){` |
|    7 |  114 | `				if( pParse->xError ){` |
|    7 |  115 | `					rc = pParse->xError("End of input found,but processing instruction was not found",SXML_ERROR_UNCLOSED_TOKEN,pToken,pParse->pUserData);` |
|    7 |  116 | `					if( rc == SXERR_ABORT ){` |
|    7 |  117 | `						return SXERR_ABORT;` |
|    - |  118 | `					}` |
|  ! 0 |  119 | `				}` |
|  ! 0 |  120 | `				return SXERR_EOF;` |
|    - |  121 | `			}` |
|   13 |  122 | `			pStream->zText += sizeof("?>")-1;` |
|  355 |  123 | `		}else if( c == '!' ){` |
|   25 |  124 | `			pStream->zText++;` |
|   25 |  125 | `			if( XLEX_IN_LEN(pStream) >= sizeof("--")-1 && pStream->zText[0] == '-' && pStream->zText[1] == '-' ){` |
|    - |  126 | `				/* Comment */` |
|    5 |  127 | `				pStream->zText += sizeof("--") - 1;` |
|  149 |  128 | `				while( XLEX_IN_LEN(pStream) >= sizeof("-->")-1 &&` |
|   74 |  129 | `					SyMemcmp((const void *)pStream->zText,"-->",sizeof("-->")-1) != 0 ){` |
|   71 |  130 | `						if( pStream->zText[0] == '\n' ){` |
|    - |  131 | `							/* Increment line counter */` |
|    3 |  132 | `							pStream->nLine++;` |
|    1 |  133 | `						}` |
|   71 |  134 | `						pStream->zText++;` |
|    1 |  135 | `				}` |
|    5 |  136 | `				pStream->zText += sizeof("-->")-1;` |
|    - |  137 | `				/* Tell the lexer to ignore this token */` |
|    5 |  138 | `				return SXERR_CONTINUE;` |
|    - |  139 | `			}` |
|   21 |  140 | `			if( XLEX_IN_LEN(pStream) >= sizeof("[CDATA[") - 1 && SyMemcmp((const void *)pStream->zText,"[CDATA[",sizeof("[CDATA[")-1) == 0 ){` |
|    - |  141 | `				/* CDATA */` |
|   11 |  142 | `				pStream->zText += sizeof("[CDATA[") - 1;` |
|   11 |  143 | `				pStr->zString = (const char *)pStream->zText;` |
|  349 |  144 | `				while( XLEX_IN_LEN(pStream) >= sizeof("]]>")-1 &&` |
|  172 |  145 | `					SyMemcmp((const void *)pStream->zText,"]]>",sizeof("]]>")-1) != 0 ){` |
|  167 |  146 | `						if( pStream->zText[0] == '\n' ){` |
|    - |  147 | `							/* Increment line counter */` |
|    3 |  148 | `							pStream->nLine++;` |
|    1 |  149 | `						}` |
|  167 |  150 | `						pStream->zText++;` |
|    1 |  151 | `				}` |
|    - |  152 | `				/* Record token type and length */` |
|   11 |  153 | `				pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|   11 |  154 | `				pToken->nType = SXML_TOK_CDATA;` |
|   11 |  155 | `				if( XLEX_IN_LEN(pStream) < sizeof("]]>")-1 ){` |
|    5 |  156 | `					if( pParse->xError ){` |
|    5 |  157 | `						rc = pParse->xError("End of input found,but ]]> was not found",SXML_ERROR_UNCLOSED_TOKEN,pToken,pParse->pUserData);` |
|    5 |  158 | `						if( rc == SXERR_ABORT ){` |
|    5 |  159 | `							return SXERR_ABORT;` |
|    - |  160 | `						}` |
|  ! 0 |  161 | `					}` |
|  ! 0 |  162 | `					return SXERR_EOF;` |
|    - |  163 | `				}` |
|    7 |  164 | `				pStream->zText += sizeof("]]>")-1;` |
|    7 |  165 | `				return SXRET_OK;` |
|    - |  166 | `			}` |
|   11 |  167 | `			if( XLEX_IN_LEN(pStream) >= sizeof("DOCTYPE") - 1 && SyMemcmp((const void *)pStream->zText,"DOCTYPE",sizeof("DOCTYPE")-1) == 0 ){` |
|    9 |  168 | `				SyString sDelim = { ">" , sizeof(char) }; /* Default delimiter */` |
|    9 |  169 | `				int c0 = 0;` |
|    - |  170 | `				/* DOCTYPE */` |
|    9 |  171 | `				pStream->zText += sizeof("DOCTYPE") - 1;` |
|    9 |  172 | `				pStr->zString = (const char *)pStream->zText;` |
|    - |  173 | `				/* Check for element declaration */` |
|  117 |  174 | `				while( pStream->zText < pStream->zEnd && pStream->zText[0] != '\n' ){` |
|  115 |  175 | `					if( pStream->zText[0] >= 0xc0 \|\| !SyisSpace(pStream->zText[0]) ){` |
|   95 |  176 | `						c0 = pStream->zText[0];` |
|   95 |  177 | `						if( c0 == '>' ){` |
|    7 |  178 | `							break;` |
|    - |  179 | `						}` |
|   44 |  180 | `					}` |
|  109 |  181 | `					pStream->zText++;` |
|    1 |  182 | `				}` |
|    9 |  183 | `				if( c0 == '[' ){` |
|    - |  184 | `					/* Change the delimiter */` |
|    3 |  185 | `					SyStringInitFromBuf(&sDelim,"]>",sizeof("]>")-1);` |
|    1 |  186 | `				}` |
|    9 |  187 | `				if( c0 != '>' ){` |
|    3 |  188 | `					while( XLEX_IN_LEN(pStream) >= sDelim.nByte &&` |
|  ! 0 |  189 | `						SyMemcmp((const void *)pStream->zText,sDelim.zString,sDelim.nByte) != 0 ){` |
|  ! 0 |  190 | `							if( pStream->zText[0] == '\n' ){` |
|    - |  191 | `								/* Increment line counter */` |
|  ! 0 |  192 | `								pStream->nLine++;` |
|  ! 0 |  193 | `							}` |
|  ! 0 |  194 | `							pStream->zText++;` |
|  ! 0 |  195 | `					}` |
|    1 |  196 | `				}` |
|    - |  197 | `				/* Record token type and length */` |
|    9 |  198 | `				pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|    9 |  199 | `				pToken->nType = SXML_TOK_DOCTYPE;` |
|    9 |  200 | `				if( XLEX_IN_LEN(pStream) < sDelim.nByte ){` |
|    3 |  201 | `					if( pParse->xError ){` |
|    3 |  202 | `						rc = pParse->xError("End of input found,but ]> or > was not found",SXML_ERROR_UNCLOSED_TOKEN,pToken,pParse->pUserData);` |
|    3 |  203 | `						if( rc == SXERR_ABORT ){` |
|    3 |  204 | `							return SXERR_ABORT;` |
|    - |  205 | `						}` |
|  ! 0 |  206 | `					}` |
|  ! 0 |  207 | `					return SXERR_EOF;` |
|    - |  208 | `				}` |
|    7 |  209 | `				pStream->zText += sDelim.nByte;` |
|    7 |  210 | `				return SXRET_OK;` |
|    - |  211 | `			}` |
|    2 |  212 | `			}else{` |
|    - |  213 | `			/* reuse function-scope variable 'c' declared at top of function */` |
|  325 |  214 | `			c = pStream->zText[0];` |
|  325 |  215 | `			rc = SXRET_OK;` |
|  325 |  216 | `			pToken->nType = SXML_TOK_START_TAG;` |
|  325 |  217 | `			if( c == '/' ){` |
|    - |  218 | `				/* End tag */` |
|   69 |  219 | `				pToken->nType = SXML_TOK_END_TAG;` |
|   69 |  220 | `				pStream->zText++;` |
|   69 |  221 | `				pStr->zString++;` |
|   69 |  222 | `				if( pStream->zText >= pStream->zEnd ){` |
|  ! 0 |  223 | `					if( pParse->xError ){` |
|  ! 0 |  224 | `						rc = pParse->xError("Illegal syntax,expecting valid start name character",SXML_ERROR_SYNTAX,pToken,pParse->pUserData);` |
|  ! 0 |  225 | `						if( rc == SXERR_ABORT ){` |
|  ! 0 |  226 | `							return SXERR_ABORT;` |
|    - |  227 | `						}` |
|  ! 0 |  228 | `					}` |
|  ! 0 |  229 | `					return SXERR_EOF;` |
|    - |  230 | `				}` |
|   69 |  231 | `				c = pStream->zText[0];` |
|   34 |  232 | `			}` |
|  325 |  233 | `			if( c == '>' ){` |
|    - |  234 | `				/*<>*/` |
|  ! 0 |  235 | `				if( pParse->xError ){` |
|  ! 0 |  236 | `					rc = pParse->xError("Illegal syntax,expecting valid start name character",SXML_ERROR_SYNTAX,pToken,pParse->pUserData);` |
|  ! 0 |  237 | `					if( rc == SXERR_ABORT ){` |
|  ! 0 |  238 | `						return SXERR_ABORT;` |
|    - |  239 | `					}` |
|  ! 0 |  240 | `				}` |
|    - |  241 | `				/* Ignore the token */` |
|  ! 0 |  242 | `				return SXERR_CONTINUE;` |
|    - |  243 | `			}` |
|  325 |  244 | `			if( c < 0xc0 && (SyisSpace(c) \|\| SyisDigit(c) \|\| c == '.' \|\| c == '-' \|\|IS_XML_DIRTY(c) ) ){` |
|  165 |  245 | `				if( pParse->xError ){` |
|    7 |  246 | `					rc = pParse->xError("Illegal syntax,expecting valid start name character",SXML_ERROR_SYNTAX,pToken,pParse->pUserData);` |
|    7 |  247 | `					if( rc == SXERR_ABORT ){` |
|    7 |  248 | `						return SXERR_ABORT;` |
|    - |  249 | `					}` |
|  ! 0 |  250 | `				}` |
|  ! 0 |  251 | `				rc = SXERR_INVALID;` |
|  ! 0 |  252 | `			}` |
|  161 |  253 | `			pStream->zText++;` |
|    - |  254 | `			/* Delimit the tag */` |
|  927 |  255 | `			while( pStream->zText < pStream->zEnd && pStream->zText[0] != '>' ){` |
|  775 |  256 | `				c = pStream->zText[0];` |
|  775 |  257 | `				if( c >= 0xc0 ){` |
|    - |  258 | `					/* UTF-8 stream */` |
|  ! 0 |  259 | `					pStream->zText++;` |
|  ! 0 |  260 | `					SX_JMP_UTF8(pStream->zText,pStream->zEnd);` |
|  ! 0 |  261 | `				}else{` |
|  775 |  262 | `					if( c == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '>' ){` |
|    9 |  263 | `						pStream->zText++;` |
|    9 |  264 | `						if( pToken->nType != SXML_TOK_START_TAG ){` |
|  ! 0 |  265 | `							if( pParse->xError ){` |
|  ! 0 |  266 | `								rc = pParse->xError("Unexpected closing tag,expecting '>'",` |
|  ! 0 |  267 | `									SXML_ERROR_SYNTAX,pToken,pParse->pUserData);` |
|  ! 0 |  268 | `								if( rc == SXERR_ABORT ){` |
|  ! 0 |  269 | `									return SXERR_ABORT;` |
|    - |  270 | `								}` |
|  ! 0 |  271 | `							}` |
|    - |  272 | `							/* Ignore the token */` |
|  ! 0 |  273 | `							rc = SXERR_INVALID;` |
|  ! 0 |  274 | `						}else{` |
|    9 |  275 | `							pToken->nType = SXML_TOK_START_END;` |
|    - |  276 | `						}` |
|    9 |  277 | `						break;` |
|    - |  278 | `					}` |
|  767 |  279 | `					if( pStream->zText[0] == '\n' ){` |
|    - |  280 | `						/* Increment line counter */` |
|  ! 0 |  281 | `						pStream->nLine++;` |
|  ! 0 |  282 | `					}` |
|    - |  283 | `					/* Advance the stream cursor */` |
|  767 |  284 | `					pStream->zText++;` |
|    - |  285 | `				}` |
|    1 |  286 | `			}` |
|  161 |  287 | `			if( rc != SXRET_OK ){` |
|    - |  288 | `				/* Tell the lexer to ignore this token */` |
|  ! 0 |  289 | `				return SXERR_CONTINUE;` |
|    - |  290 | `			}` |
|    - |  291 | `			/* Record token length */` |
|  161 |  292 | `			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  161 |  293 | `			if( pToken->nType == SXML_TOK_START_END && pStr->nByte > 0){` |
|    9 |  294 | `				pStr->nByte -= sizeof(char);` |
|    4 |  295 | `			}` |
|  161 |  296 | `			if ( pStream->zText < pStream->zEnd ){` |
|  161 |  297 | `				pStream->zText++;` |
|   81 |  298 | `			}else{` |
|  ! 0 |  299 | `				if( pParse->xError ){` |
|  ! 0 |  300 | `					rc = pParse->xError("End of input found,but closing tag '>' was not found",SXML_ERROR_UNCLOSED_TOKEN,pToken,pParse->pUserData);` |
|  ! 0 |  301 | `					if( rc == SXERR_ABORT ){` |
|  ! 0 |  302 | `						return SXERR_ABORT;` |
|    - |  303 | `					}` |
|  ! 0 |  304 | `				}` |
|    - |  305 | `			}` |
|    - |  306 | `		}` |
|   88 |  307 | `	}else{` |
|    - |  308 | `		/* Raw input */` |
|  201 |  309 | `		while( pStream->zText < pStream->zEnd ){` |
|  201 |  310 | `			c = pStream->zText[0];` |
|  201 |  311 | `			if( c < 0xc0 ){` |
|  201 |  312 | `				if( c == '<' ){` |
|   29 |  313 | `					break;` |
|  173 |  314 | `				}else if( c == '\n' ){` |
|    - |  315 | `					/* Increment line counter */` |
|  ! 0 |  316 | `					pStream->nLine++;` |
|  ! 0 |  317 | `				}` |
|    - |  318 | `				/* Advance the stream cursor */` |
|  173 |  319 | `				pStream->zText++;` |
|   87 |  320 | `			}else{` |
|    - |  321 | `				/* UTF-8 stream */` |
|  ! 0 |  322 | `				pStream->zText++;` |
|  ! 0 |  323 | `				SX_JMP_UTF8(pStream->zText,pStream->zEnd);` |
|    - |  324 | `			}` |
|    1 |  325 | `		}` |
|    - |  326 | `		/* Record token type,length */` |
|   29 |  327 | `		pToken->nType = SXML_TOK_RAW;` |
|   29 |  328 | `		pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|    - |  329 | `	}` |
|    - |  330 | `	/* Return to the lexer */` |
|  203 |  331 | `	return SXRET_OK;` |
|  120 |  332 |  |
|   12 |  333 | `static int XMLCheckDuplicateAttr(SyXMLRawStr *aSet,sxu32 nEntry,SyXMLRawStr *pEntry)` |
|    1 |  334 |  |
|    - |  335 | `	sxu32 n;` |
|   13 |  336 | `	for( n = 0 ; n < nEntry ; n += 2 ){` |
|    3 |  337 | `		SyXMLRawStr *pAttr = &aSet[n];` |
|    3 |  338 | `		if( pAttr->nByte == pEntry->nByte && SyMemcmp(pAttr->zString,pEntry->zString,pEntry->nByte) == 0 ){` |
|    - |  339 | `			/* Attribute found */` |
|    3 |  340 | `			return 1;` |
|    - |  341 | `		}` |
|  ! 0 |  342 | `	}` |
|    - |  343 | `	/* No duplicates */` |
|   11 |  344 | `	return 0;` |
|    7 |  345 |  |
|    4 |  346 | `static sxi32 XMLProcessNamesSpace(SyXMLParser *pParse,SyXMLRawStrNS *pTag,SyToken *pToken,SySet *pAttr)` |
|    1 |  347 |  |
|    - |  348 | `	SyXMLRawStr *pPrefix,*pUri; /* Namespace prefix/URI */` |
|    - |  349 | `	SyHashEntry *pEntry;` |
|    - |  350 | `	SyXMLRawStr *pDup;` |
|    - |  351 | `	sxi32 rc;` |
|    - |  352 | `	/* Extract the URI first */` |
|    5 |  353 | `	pUri = (SyXMLRawStr *)SySetPeek(pAttr);` |
|    - |  354 | `	/* Extract the prefix */` |
|    5 |  355 | `	pPrefix =  (SyXMLRawStr *)SySetAt(pAttr,SySetUsed(pAttr) - 2);` |
|    - |  356 | `	/* Prefix name */` |
|    5 |  357 | `	if( pPrefix->nByte == sizeof("xmlns")-1 ){` |
|    - |  358 | `		/* Default namespace */` |
|  ! 0 |  359 | `		pPrefix->nByte = 0;` |
|  ! 0 |  360 | `		pPrefix->zString = ""; /* Empty string */` |
|  ! 0 |  361 | `	}else{` |
|    5 |  362 | `		pPrefix->nByte   -= sizeof("xmlns")-1;` |
|    5 |  363 | `		pPrefix->zString += sizeof("xmlns")-1;` |
|    5 |  364 | `		if( pPrefix->zString[0] != ':' ){` |
|  ! 0 |  365 | `			return SXRET_OK;` |
|    - |  366 | `		}` |
|    5 |  367 | `		pPrefix->nByte--;` |
|    5 |  368 | `		pPrefix->zString++;` |
|    5 |  369 | `		if( pPrefix->nByte < 1 ){` |
|  ! 0 |  370 | `			if( pParse->xError ){` |
|  ! 0 |  371 | `				rc = pParse->xError("Invalid namespace name",SXML_ERROR_SYNTAX,pToken,pParse->pUserData);` |
|  ! 0 |  372 | `				if( rc == SXERR_ABORT ){` |
|  ! 0 |  373 | `					return SXERR_ABORT;` |
|    - |  374 | `				}` |
|  ! 0 |  375 | `			}` |
|    - |  376 | `			/* POP the last inserted two entries */` |
|  ! 0 |  377 | `			(void)SySetPop(pAttr);` |
|  ! 0 |  378 | `			(void)SySetPop(pAttr);` |
|  ! 0 |  379 | `			return SXERR_SYNTAX;` |
|    - |  380 | `		}` |
|    - |  381 | `	}` |
|    - |  382 | `	/* Invoke the namespace callback if available */` |
|    5 |  383 | `	if( pParse->xNameSpace ){` |
|    5 |  384 | `		rc = pParse->xNameSpace(pPrefix,pUri,pParse->pUserData);` |
|    5 |  385 | `		if( rc == SXERR_ABORT ){` |
|    - |  386 | `			/* User callback request an operation abort */` |
|  ! 0 |  387 | `			return SXERR_ABORT;` |
|    - |  388 | `		}` |
|    2 |  389 | `	}` |
|    - |  390 | `	/* Duplicate structure */` |
|    5 |  391 | `	pDup = (SyXMLRawStr *)SyMemBackendAlloc(pParse->pAllocator,sizeof(SyXMLRawStr));` |
|    5 |  392 | `	if( pDup == 0 ){` |
|  ! 0 |  393 | `		if( pParse->xError ){` |
|  ! 0 |  394 | `			pParse->xError("Out of memory",SXML_ERROR_NO_MEMORY,pToken,pParse->pUserData);` |
|  ! 0 |  395 | `		}` |
|    - |  396 | `		/* Abort processing immediately */` |
|  ! 0 |  397 | `		return SXERR_ABORT;` |
|    - |  398 | `	}` |
|    5 |  399 | `	*pDup = *pUri; /* Structure assignement */` |
|    - |  400 | `	/* Save the namespace */` |
|    5 |  401 | `	if( pPrefix->nByte == 0 ){` |
|  ! 0 |  402 | `		pPrefix->zString = "Default";` |
|  ! 0 |  403 | `		pPrefix->nByte = sizeof("Default")-1;` |
|  ! 0 |  404 | `	}` |
|    5 |  405 | `	SyHashInsert(&pParse->hns,(const void *)pPrefix->zString,pPrefix->nByte,pDup);` |
|    - |  406 | `	/* Peek the last inserted entry */` |
|    5 |  407 | `	pEntry = SyHashLastEntry(&pParse->hns);` |
|    - |  408 | `	/* Store in the corresponding tag container*/` |
|    5 |  409 | `	SySetPut(&pTag->sNSset,(const void *)&pEntry);` |
|    - |  410 | `	/* POP the last inserted two entries */` |
|    5 |  411 | `	(void)SySetPop(pAttr);` |
|    5 |  412 | `	(void)SySetPop(pAttr);` |
|    5 |  413 | `	return SXRET_OK;` |
|    3 |  414 |  |
|  456 |  415 | `static sxi32 XMLProcessStartTag(SyXMLParser *pParse,SyToken *pToken,SyXMLRawStrNS *pTag,SySet  *pAttrSet,SySet *pTagStack)` |
|    1 |  416 |  |
|  457 |  417 | `	SyString *pIn = &pToken->sData;` |
|    - |  418 | `	const char *zIn,*zCur,*zEnd;` |
|    - |  419 | `	SyXMLRawStr sEntry;` |
|    - |  420 | `	sxi32 rc;` |
|    - |  421 | `	int c;` |
|    - |  422 | `	/* Reset the working set */` |
|  457 |  423 | `	SySetReset(pAttrSet);` |
|    - |  424 | `	/* Delimit the raw tag */` |
|  457 |  425 | `	zIn = pIn->zString;` |
|  457 |  426 | `	zEnd = &zIn[pIn->nByte];` |
|  457 |  427 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|  ! 0 |  428 | `		zIn++;` |
|  ! 0 |  429 | `	}` |
|    - |  430 | `	/* Isolate tag name */` |
|  457 |  431 | `	sEntry.nLine = pTag->nLine = pToken->nLine;` |
|  457 |  432 | `	zCur = zIn;` |
|  645 |  433 | `	while( zIn < zEnd ){` |
|  575 |  434 | `		if( (unsigned char)zIn[0] >= 0xc0 ){` |
|    - |  435 | `			/* UTF-8 stream */` |
|  ! 0 |  436 | `			zIn++;` |
|  ! 0 |  437 | `			SX_JMP_UTF8(zIn,zEnd);` |
|  574 |  438 | `		}else if( SyisSpace(zIn[0])){` |
|   11 |  439 | `			break;` |
|  ! 0 |  440 | `		}else{` |
|  565 |  441 | `			if( IS_XML_DIRTY(zIn[0]) ){` |
|  376 |  442 | `				if( pParse->xError ){` |
|  ! 0 |  443 | `					rc = pParse->xError("Illegal character in XML name",SXML_ERROR_SYNTAX,pToken,pParse->pUserData);` |
|  ! 0 |  444 | `					if( rc == SXERR_ABORT ){` |
|  ! 0 |  445 | `						return SXERR_ABORT;` |
|    - |  446 | `					}` |
|  ! 0 |  447 | `				}` |
|  ! 0 |  448 | `			}` |
|  189 |  449 | `			zIn++;` |
|    - |  450 | `		}` |
|    1 |  451 | `	}` |
|   81 |  452 | `	if( zCur >= zIn ){` |
|  ! 0 |  453 | `		if( pParse->xError ){` |
|  ! 0 |  454 | `			rc = pParse->xError("Invalid XML name",SXML_ERROR_SYNTAX,pToken,pParse->pUserData);` |
|  ! 0 |  455 | `			if( rc == SXERR_ABORT ){` |
|  ! 0 |  456 | `				return SXERR_ABORT;` |
|    - |  457 | `			}` |
|  ! 0 |  458 | `		}` |
|  ! 0 |  459 | `		return SXERR_SYNTAX;` |
|    - |  460 | `	}` |
|   81 |  461 | `	pTag->zString = zCur;` |
|   81 |  462 | `	pTag->nByte = (sxu32)(zIn-zCur);` |
|    - |  463 | `	/* Process tag attribute */` |
|   50 |  464 | `	for(;;){` |
|   91 |  465 | `		int is_ns = 0;` |
|  103 |  466 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|   13 |  467 | `			zIn++;` |
|    1 |  468 | `		}` |
|   91 |  469 | `		if( zIn >= zEnd ){` |
|   79 |  470 | `			break;` |
|    - |  471 | `		}` |
|   13 |  472 | `		zCur = zIn;` |
|   77 |  473 | `		while( zIn < zEnd && zIn[0] != '=' ){` |
|   65 |  474 | `			if( (unsigned char)zIn[0] >= 0xc0 ){` |
|    - |  475 | `				/* UTF-8 stream */` |
|  ! 0 |  476 | `				zIn++;` |
|  ! 0 |  477 | `				SX_JMP_UTF8(zIn,zEnd);` |
|   64 |  478 | `			}else if( SyisSpace(zIn[0]) ){` |
|  ! 0 |  479 | `				break;` |
|  ! 0 |  480 | `			}else{` |
|   65 |  481 | `				zIn++;` |
|    - |  482 | `			}` |
|    1 |  483 | `		}` |
|   13 |  484 | `		if( zCur >= zIn ){` |
|  ! 0 |  485 | `			if( pParse->xError ){` |
|  ! 0 |  486 | `				rc = pParse->xError("Missing attribute name",SXML_ERROR_SYNTAX,pToken,pParse->pUserData);` |
|  ! 0 |  487 | `				if( rc == SXERR_ABORT ){` |
|  ! 0 |  488 | `					return SXERR_ABORT;` |
|    - |  489 | `				}` |
|  ! 0 |  490 | `			}` |
|  ! 0 |  491 | `			return SXERR_SYNTAX;` |
|    - |  492 | `		}` |
|    - |  493 | `		/* Store attribute name */` |
|   13 |  494 | `		sEntry.zString = zCur;` |
|   13 |  495 | `		sEntry.nByte = (sxu32)(zIn-zCur);` |
|   15 |  496 | `		if( (pParse->nFlags & SXML_ENABLE_NAMESPACE) && sEntry.nByte >= sizeof("xmlns") - 1 &&` |
|    4 |  497 | `			SyMemcmp(sEntry.zString,"xmlns",sizeof("xmlns") - 1) == 0 ){` |
|    5 |  498 | `				is_ns = 1;` |
|    2 |  499 | `		}` |
|   13 |  500 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|  ! 0 |  501 | `			zIn++;` |
|  ! 0 |  502 | `		}` |
|   13 |  503 | `		if( zIn >= zEnd \|\| zIn[0] != '=' ){` |
|  ! 0 |  504 | `			if( pParse->xError ){` |
|  ! 0 |  505 | `				rc = pParse->xError("Missing attribute value",SXML_ERROR_SYNTAX,pToken,pParse->pUserData);` |
|  ! 0 |  506 | `				if( rc == SXERR_ABORT ){` |
|  ! 0 |  507 | `					return SXERR_ABORT;` |
|    - |  508 | `				}` |
|  ! 0 |  509 | `			}` |
|  ! 0 |  510 | `			return SXERR_SYNTAX;` |
|    - |  511 | `		}` |
|   24 |  512 | `		while( sEntry.nByte > 0 && (unsigned char)zCur[sEntry.nByte - 1] < 0xc0` |
|   19 |  513 | `			&& SyisSpace(zCur[sEntry.nByte - 1])){` |
|  ! 0 |  514 | `				sEntry.nByte--;` |
|  ! 0 |  515 | `		}` |
|    - |  516 | `		/* Check for duplicates first */` |
|   13 |  517 | `		if( XMLCheckDuplicateAttr((SyXMLRawStr *)SySetBasePtr(pAttrSet),SySetUsed(pAttrSet),&sEntry) ){` |
|    3 |  518 | `			if( pParse->xError ){` |
|    3 |  519 | `				rc = pParse->xError("Duplicate attribute",SXML_ERROR_DUPLICATE_ATTRIBUTE,pToken,pParse->pUserData);` |
|    3 |  520 | `				if( rc == SXERR_ABORT ){` |
|    3 |  521 | `					return SXERR_ABORT;` |
|    - |  522 | `				}` |
|  ! 0 |  523 | `			}` |
|  ! 0 |  524 | `			return SXERR_SYNTAX;` |
|    - |  525 | `		}` |
|   11 |  526 | `		if( SXRET_OK != SySetPut(pAttrSet,(const void *)&sEntry) ){` |
|  ! 0 |  527 | `			return SXERR_ABORT;` |
|    - |  528 | `		}` |
|    - |  529 | `		/* Extract attribute value */` |
|   11 |  530 | `		zIn++; /* Jump the trailing '=' */` |
|   11 |  531 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|  ! 0 |  532 | `			zIn++;` |
|  ! 0 |  533 | `		}` |
|   11 |  534 | `		if( zIn >= zEnd ){` |
|  ! 0 |  535 | `			if( pParse->xError ){` |
|  ! 0 |  536 | `				rc = pParse->xError("Missing attribute value",SXML_ERROR_SYNTAX,pToken,pParse->pUserData);` |
|  ! 0 |  537 | `				if( rc == SXERR_ABORT ){` |
|  ! 0 |  538 | `					return SXERR_ABORT;` |
|    - |  539 | `				}` |
|  ! 0 |  540 | `			}` |
|  ! 0 |  541 | `			(void)SySetPop(pAttrSet);` |
|  ! 0 |  542 | `			return SXERR_SYNTAX;` |
|    - |  543 | `		}` |
|   11 |  544 | `		if( zIn[0] != '\'' && zIn[0] != '"' ){` |
|  ! 0 |  545 | `			if( pParse->xError ){` |
|  ! 0 |  546 | `				rc = pParse->xError("Missing quotes on attribute value",SXML_ERROR_SYNTAX,pToken,pParse->pUserData);` |
|  ! 0 |  547 | `				if( rc == SXERR_ABORT ){` |
|  ! 0 |  548 | `					return SXERR_ABORT;` |
|    - |  549 | `				}` |
|  ! 0 |  550 | `			}` |
|  ! 0 |  551 | `			(void)SySetPop(pAttrSet);` |
|  ! 0 |  552 | `			return SXERR_SYNTAX;` |
|    - |  553 | `		}` |
|   11 |  554 | `		c = zIn[0];` |
|   11 |  555 | `		zIn++;` |
|   11 |  556 | `		zCur = zIn;` |
|   89 |  557 | `		while( zIn < zEnd && zIn[0] != c ){` |
|   79 |  558 | `			zIn++;` |
|    1 |  559 | `		}` |
|   11 |  560 | `		if( zIn >= zEnd ){` |
|  ! 0 |  561 | `			if( pParse->xError ){` |
|  ! 0 |  562 | `				rc = pParse->xError("Missing quotes on attribute value",SXML_ERROR_SYNTAX,pToken,pParse->pUserData);` |
|  ! 0 |  563 | `				if( rc == SXERR_ABORT ){` |
|  ! 0 |  564 | `					return SXERR_ABORT;` |
|    - |  565 | `				}` |
|  ! 0 |  566 | `			}` |
|  ! 0 |  567 | `			(void)SySetPop(pAttrSet);` |
|  ! 0 |  568 | `			return SXERR_SYNTAX;` |
|    - |  569 | `		}` |
|    - |  570 | `		/* Store attribute value */` |
|   11 |  571 | `		sEntry.zString = zCur;` |
|   11 |  572 | `		sEntry.nByte = (sxu32)(zIn-zCur);` |
|   11 |  573 | `		if( SXRET_OK != SySetPut(pAttrSet,(const void *)&sEntry) ){` |
|  ! 0 |  574 | `			return SXERR_ABORT;` |
|    - |  575 | `		}` |
|   11 |  576 | `		zIn++;` |
|   11 |  577 | `		if( is_ns ){` |
|    - |  578 | `			/* Process namespace declaration */` |
|    5 |  579 | `			XMLProcessNamesSpace(pParse,pTag,pToken,pAttrSet);` |
|    2 |  580 | `		}` |
|    1 |  581 | `	}` |
|    - |  582 | `	/* Store in the tag stack */` |
|   79 |  583 | `	if( pToken->nType == SXML_TOK_START_TAG ){` |
|   71 |  584 | `		rc = SySetPut(pTagStack,(const void *)pTag);` |
|   35 |  585 | `	}` |
|   79 |  586 | `	return SXRET_OK;` |
|   41 |  587 |  |
|   12 |  588 | `static void XMLExtactPI(SyToken *pToken,SyXMLRawStr *pTarget,SyXMLRawStr *pData,int *pXML)` |
|    1 |  589 |  |
|   13 |  590 | `	SyString *pIn = &pToken->sData;` |
|    - |  591 | `	const char *zIn,*zCur,*zEnd;` |
|    - |  592 |  |
|   13 |  593 | `	pTarget->nLine = pData->nLine = pToken->nLine;` |
|    - |  594 | `	/* Nullify the entries first */` |
|   13 |  595 | `	pTarget->zString = pData->zString = 0;` |
|    - |  596 | `	/* Ignore leading and trailing white spaces */` |
|   13 |  597 | `	SyStringFullTrim(pIn);` |
|    - |  598 | `	/* Delimit the raw PI */` |
|   13 |  599 | `	zIn  = pIn->zString;` |
|   13 |  600 | `	zEnd = &zIn[pIn->nByte];` |
|   13 |  601 | `	if( pXML ){` |
|   13 |  602 | `		*pXML = 0;` |
|    6 |  603 | `	}` |
|    - |  604 | `	/* Extract the target */` |
|   13 |  605 | `	zCur = zIn;` |
|   61 |  606 | `	while( zIn < zEnd ){` |
|   61 |  607 | `		if( (unsigned char)zIn[0] >= 0xc0 ){` |
|    - |  608 | `			/* UTF-8 stream */` |
|  ! 0 |  609 | `			zIn++;` |
|  ! 0 |  610 | `			SX_JMP_UTF8(zIn,zEnd);` |
|   60 |  611 | `		}else if( SyisSpace(zIn[0])){` |
|   13 |  612 | `			break;` |
|  ! 0 |  613 | `		}else{` |
|   49 |  614 | `			zIn++;` |
|    - |  615 | `		}` |
|    1 |  616 | `	}` |
|   13 |  617 | `	if( zIn > zCur ){` |
|   13 |  618 | `		pTarget->zString = zCur;` |
|   13 |  619 | `		pTarget->nByte = (sxu32)(zIn-zCur);` |
|   13 |  620 | `		if( pXML && pTarget->nByte == sizeof("xml")-1 && SyStrnicmp(pTarget->zString,"xml",sizeof("xml")-1) == 0 ){` |
|    9 |  621 | `			*pXML = 1;` |
|    4 |  622 | `		}` |
|    6 |  623 | `	}` |
|    - |  624 | `	/* Extract the PI data  */` |
|   25 |  625 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|   13 |  626 | `		zIn++;` |
|    1 |  627 | `	}` |
|   13 |  628 | `	if( zIn < zEnd ){` |
|   13 |  629 | `		pData->zString = zIn;` |
|   13 |  630 | `		pData->nByte = (sxu32)(zEnd-zIn);` |
|    6 |  631 | `	}` |
|   13 |  632 |  |
|   54 |  633 | `static sxi32 XMLExtractEndTag(SyXMLParser *pParse,SyToken *pToken,SyXMLRawStrNS *pOut)` |
|    1 |  634 |  |
|   55 |  635 | `	SyString *pIn = &pToken->sData;` |
|   55 |  636 | `	const char *zEnd = &pIn->zString[pIn->nByte];` |
|   55 |  637 | `	const char *zIn = pIn->zString;` |
|    - |  638 | `	/* Ignore leading white spaces */` |
|   55 |  639 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|  ! 0 |  640 | `		zIn++;` |
|  ! 0 |  641 | `	}` |
|   55 |  642 | `	pOut->nLine = pToken->nLine;` |
|   55 |  643 | `	pOut->zString = zIn;` |
|   55 |  644 | `	pOut->nByte = (sxu32)(zEnd-zIn);` |
|    - |  645 | `	/* Ignore trailing white spaces */` |
|  108 |  646 | `	while( pOut->nByte > 0 && (unsigned char)pOut->zString[pOut->nByte - 1] < 0xc0` |
|   82 |  647 | `		&& SyisSpace(pOut->zString[pOut->nByte - 1]) ){` |
|  ! 0 |  648 | `			pOut->nByte--;` |
|  ! 0 |  649 | `	}` |
|   55 |  650 | `	if( pOut->nByte < 1 ){` |
|  ! 0 |  651 | `		if( pParse->xError ){` |
|    - |  652 | `			sxi32 rc;` |
|  ! 0 |  653 | `			rc  = pParse->xError("Invalid end tag name",SXML_ERROR_INVALID_TOKEN,pToken,pParse->pUserData);` |
|  ! 0 |  654 | `			if( rc == SXERR_ABORT ){` |
|  ! 0 |  655 | `				return SXERR_ABORT;` |
|    - |  656 | `			}` |
|  ! 0 |  657 | `		}` |
|  ! 0 |  658 | `		return SXERR_SYNTAX;` |
|    - |  659 | `	}` |
|   55 |  660 | `	return SXRET_OK;` |
|   28 |  661 |  |
|   28 |  662 | `static void TokenToXMLString(SyToken *pTok,SyXMLRawStrNS *pOut)` |
|    1 |  663 |  |
|    - |  664 | `	/* Remove leading and trailing white spaces first */` |
|   29 |  665 | `	SyStringFullTrim(&pTok->sData);` |
|   29 |  666 | `	pOut->zString = SyStringData(&pTok->sData);` |
|   29 |  667 | `	pOut->nByte = SyStringLength(&pTok->sData);` |
|   29 |  668 |  |
|   16 |  669 | `static sxi32 XMLExtractNS(SyXMLParser *pParse,SyToken *pToken,SyXMLRawStrNS *pTag,SyXMLRawStr *pnsUri)` |
|    1 |  670 |  |
|    - |  671 | `	SyXMLRawStr *pUri,sPrefix;` |
|    - |  672 | `	SyHashEntry *pEntry;` |
|    - |  673 | `	sxu32 nOfft;` |
|    - |  674 | `	sxi32 rc;` |
|    - |  675 | `	/* Extract a prefix if available */` |
|   17 |  676 | `	rc = SyByteFind(pTag->zString,pTag->nByte,':',&nOfft);` |
|   17 |  677 | `	if( rc != SXRET_OK ){` |
|    - |  678 | `		/* Check if there is a default namespace */` |
|    9 |  679 | `		pEntry = SyHashGet(&pParse->hns,"Default",sizeof("Default")-1);` |
|    9 |  680 | `		if( pEntry  ){` |
|    - |  681 | `			/* Extract the ns URI */` |
|  ! 0 |  682 | `			pUri = (SyXMLRawStr *)pEntry->pUserData;` |
|    - |  683 | `			/* Save the ns URI */` |
|  ! 0 |  684 | `			pnsUri->zString = pUri->zString;` |
|  ! 0 |  685 | `			pnsUri->nByte = pUri->nByte;` |
|  ! 0 |  686 | `		}` |
|    9 |  687 | `		return SXRET_OK;` |
|    - |  688 | `	}` |
|    9 |  689 | `	if( nOfft < 1 ){` |
|  ! 0 |  690 | `		if( pParse->xError ){` |
|  ! 0 |  691 | `			rc = pParse->xError("Empty prefix is not allowed according to XML namespace specification",` |
|  ! 0 |  692 | `				SXML_ERROR_SYNTAX,pToken,pParse->pUserData);` |
|  ! 0 |  693 | `			if( rc == SXERR_ABORT ){` |
|  ! 0 |  694 | `				return SXERR_ABORT;` |
|    - |  695 | `			}` |
|  ! 0 |  696 | `		}` |
|  ! 0 |  697 | `		return SXERR_SYNTAX;` |
|    - |  698 | `	}` |
|    9 |  699 | `	sPrefix.zString = pTag->zString;` |
|    9 |  700 | `	sPrefix.nByte = nOfft;` |
|    9 |  701 | `	sPrefix.nLine = pTag->nLine;` |
|    9 |  702 | `	pTag->zString += nOfft + 1;` |
|    9 |  703 | `	pTag->nByte -= nOfft;` |
|    9 |  704 | `	if( pTag->nByte < 1 ){` |
|  ! 0 |  705 | `		if( pParse->xError ){` |
|  ! 0 |  706 | `			rc = pParse->xError("Missing tag name",SXML_ERROR_SYNTAX,pToken,pParse->pUserData);` |
|  ! 0 |  707 | `			if( rc == SXERR_ABORT ){` |
|  ! 0 |  708 | `				return SXERR_ABORT;` |
|    - |  709 | `			}` |
|  ! 0 |  710 | `		}` |
|  ! 0 |  711 | `		return SXERR_SYNTAX;` |
|    - |  712 | `	}` |
|    - |  713 | `	/* Check if the prefix is already registered */` |
|    9 |  714 | `	pEntry = SyHashGet(&pParse->hns,sPrefix.zString,sPrefix.nByte);` |
|    9 |  715 | `	if( pEntry == 0 ){` |
|  ! 0 |  716 | `		if( pParse->xError ){` |
|  ! 0 |  717 | `			rc = pParse->xError("Namespace prefix is not defined",SXML_ERROR_SYNTAX,` |
|  ! 0 |  718 | `				pToken,pParse->pUserData);` |
|  ! 0 |  719 | `			if( rc == SXERR_ABORT ){` |
|  ! 0 |  720 | `				return SXERR_ABORT;` |
|    - |  721 | `			}` |
|  ! 0 |  722 | `		}` |
|  ! 0 |  723 | `		return SXERR_SYNTAX;` |
|    - |  724 | `	}` |
|    - |  725 | `	/* Extract the ns URI */` |
|    9 |  726 | `	pUri = (SyXMLRawStr *)pEntry->pUserData;` |
|    - |  727 | `	/* Save the ns URI */` |
|    9 |  728 | `	pnsUri->zString = pUri->zString;` |
|    9 |  729 | `	pnsUri->nByte = pUri->nByte;` |
|    - |  730 | `	/* All done */` |
|    9 |  731 | `	return SXRET_OK;` |
|    9 |  732 |  |
|   62 |  733 | `static sxi32 XMLnsUnlink(SyXMLParser *pParse,SyXMLRawStrNS *pLast,SyToken *pToken)` |
|    1 |  734 |  |
|    - |  735 | `	SyHashEntry **apEntry,*pEntry;` |
|    - |  736 | `	void *pUserData;` |
|    - |  737 | `	sxu32 n;` |
|    - |  738 | `	/* Release namespace entries */` |
|   63 |  739 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pLast->sNSset);` |
|   67 |  740 | `	for( n = 0 ; n < SySetUsed(&pLast->sNSset) ; ++n ){` |
|    5 |  741 | `		pEntry = apEntry[n];` |
|    - |  742 | `		/* Invoke the end namespace declaration callback */` |
|    5 |  743 | `		if( pParse->xNameSpaceEnd && (pParse->nFlags & SXML_ENABLE_NAMESPACE) && pToken ){` |
|    - |  744 | `			SyXMLRawStr sPrefix;` |
|    - |  745 | `			sxi32 rc;` |
|    5 |  746 | `			sPrefix.zString = (const char *)pEntry->pKey;` |
|    5 |  747 | `			sPrefix.nByte = pEntry->nKeyLen;` |
|    5 |  748 | `			sPrefix.nLine = pToken->nLine;` |
|    5 |  749 | `			rc = pParse->xNameSpaceEnd(&sPrefix,pParse->pUserData);` |
|    5 |  750 | `			if( rc == SXERR_ABORT ){` |
|  ! 0 |  751 | `				return SXERR_ABORT;` |
|    - |  752 | `			}` |
|    2 |  753 | `		}` |
|    5 |  754 | `		pUserData = pEntry->pUserData;` |
|    - |  755 | `		/* Remove from the namespace hashtable */` |
|    5 |  756 | `		SyHashDeleteEntry2(pEntry);` |
|    5 |  757 | `		SyMemBackendFree(pParse->pAllocator,pUserData);` |
|    3 |  758 | `	}` |
|   63 |  759 | `	SySetRelease(&pLast->sNSset);` |
|   63 |  760 | `	return SXRET_OK;` |
|   32 |  761 |  |
|    - |  762 | `/* Process XML tokens */` |
|   52 |  763 | `static sxi32  ProcessXML(SyXMLParser *pParse,SySet *pTagStack,SySet *pWorker)` |
|    1 |  764 |  |
|   53 |  765 | `	SySet *pTokenSet = &pParse->sToken;` |
|    - |  766 | `	SyXMLRawStrNS sEntry;` |
|    - |  767 | `	SyXMLRawStr sNs;` |
|    - |  768 | `	SyToken *pToken;` |
|    - |  769 | `	int bGotTag;` |
|    - |  770 | `	sxi32 rc;` |
|    - |  771 | `	/* Initialize fields */` |
|   53 |  772 | `	bGotTag = 0;` |
|    - |  773 | `	/* Start processing */` |
|   53 |  774 | `	if( pParse->xStartDoc && (SXERR_ABORT == pParse->xStartDoc(pParse->pUserData)) ){` |
|    - |  775 | `		/* User callback request an operation abort */` |
|  ! 0 |  776 | `		return SXERR_ABORT;` |
|    - |  777 | `	}` |
|    - |  778 | `	/* Reset the loop cursor */` |
|   53 |  779 | `	SySetResetCursor(pTokenSet);` |
|    - |  780 | `	/* Extract the current token */` |
|  227 |  781 | `	while( SXRET_OK == (SySetGetNextEntry(&(*pTokenSet),(void **)&pToken)) ){` |
|  185 |  782 | `		SyZero(&sEntry,sizeof(SyXMLRawStrNS));` |
|  185 |  783 | `		SyZero(&sNs,sizeof(SyXMLRawStr));` |
|  185 |  784 | `		SySetInit(&sEntry.sNSset,pParse->pAllocator,sizeof(SyHashEntry *));` |
|  185 |  785 | `		sEntry.nLine = sNs.nLine = pToken->nLine;` |
|  185 |  786 | `		switch(pToken->nType){` |
|    4 |  787 | `		case SXML_TOK_DOCTYPE:` |
|    9 |  788 | `			if( SySetUsed(pTagStack) > 1 \|\| bGotTag ){` |
|    3 |  789 | `				if( pParse->xError ){` |
|    3 |  790 | `					rc = pParse->xError("DOCTYPE must be declared first",SXML_ERROR_MISPLACED_XML_PI,pToken,pParse->pUserData);` |
|    3 |  791 | `					if( rc == SXERR_ABORT ){` |
|    3 |  792 | `						return SXERR_ABORT;` |
|    - |  793 | `					}` |
|  ! 0 |  794 | `				}` |
|  ! 0 |  795 | `				break;` |
|    - |  796 | `			}` |
|    - |  797 | `			/* Invoke the supplied callback if any */` |
|    7 |  798 | `			if( pParse->xDoctype ){` |
|  ! 0 |  799 | `				TokenToXMLString(pToken,&sEntry);` |
|  ! 0 |  800 | `				rc = pParse->xDoctype((SyXMLRawStr *)&sEntry,pParse->pUserData);` |
|  ! 0 |  801 | `				if( rc == SXERR_ABORT ){` |
|  ! 0 |  802 | `					return SXERR_ABORT;` |
|    - |  803 | `				}` |
|  ! 0 |  804 | `			}` |
|    7 |  805 | `			break;` |
|    3 |  806 | `		case SXML_TOK_CDATA:` |
|    7 |  807 | `			if( SySetUsed(pTagStack) < 1 ){` |
|  ! 0 |  808 | `				if( pParse->xError ){` |
|  ! 0 |  809 | `					rc = pParse->xError("CDATA without matching tag",SXML_ERROR_TAG_MISMATCH,pToken,pParse->pUserData);` |
|  ! 0 |  810 | `					if( rc == SXERR_ABORT ){` |
|  ! 0 |  811 | `						return SXERR_ABORT;` |
|    - |  812 | `					}` |
|  ! 0 |  813 | `				}` |
|  ! 0 |  814 | `			}` |
|    - |  815 | `			/* Invoke the supplied callback if any */` |
|    7 |  816 | `			if( pParse->xRaw ){` |
|    7 |  817 | `				TokenToXMLString(pToken,&sEntry);` |
|    7 |  818 | `				rc = pParse->xRaw((SyXMLRawStr *)&sEntry,pParse->pUserData);` |
|    7 |  819 | `				if( rc == SXERR_ABORT ){` |
|  ! 0 |  820 | `					return SXERR_ABORT;` |
|    - |  821 | `				}` |
|    3 |  822 | `			}` |
|    7 |  823 | `			break;` |
|    6 |  824 | `		case SXML_TOK_PI:{` |
|    - |  825 | `			SyXMLRawStr sTarget,sData;` |
|   13 |  826 | `			int isXML = 0;` |
|    - |  827 | `			/* Extract the target and data */` |
|   13 |  828 | `			XMLExtactPI(pToken,&sTarget,&sData,&isXML);` |
|   13 |  829 | `			if( isXML && SySetCursor(pTokenSet) > 1 ){` |
|    5 |  830 | `				if( pParse->xError ){` |
|    7 |  831 | `					rc = pParse->xError("Unexpected XML declaration. The XML declaration must be the first node in the document",` |
|    2 |  832 | `						SXML_ERROR_MISPLACED_XML_PI,pToken,pParse->pUserData);` |
|    5 |  833 | `					if( rc == SXERR_ABORT ){` |
|    5 |  834 | `						return SXERR_ABORT;` |
|    - |  835 | `					}` |
|  ! 0 |  836 | `				}` |
|    9 |  837 | `			}else if( pParse->xPi ){` |
|    - |  838 | `				/* Invoke the supplied callback*/` |
|    9 |  839 | `				rc = pParse->xPi(&sTarget,&sData,pParse->pUserData);` |
|    9 |  840 | `				if( rc == SXERR_ABORT ){` |
|  ! 0 |  841 | `					return SXERR_ABORT;` |
|    - |  842 | `				}` |
|    4 |  843 | `			}` |
|    9 |  844 | `			break;` |
|    - |  845 | `						 }` |
|   12 |  846 | `		case SXML_TOK_RAW:` |
|   25 |  847 | `			if( SySetUsed(pTagStack) < 1 ){` |
|    3 |  848 | `				if( pParse->xError ){` |
|    3 |  849 | `					rc = pParse->xError("Text (Raw data) without matching tag",SXML_ERROR_TAG_MISMATCH,pToken,pParse->pUserData);` |
|    3 |  850 | `					if( rc == SXERR_ABORT ){` |
|    3 |  851 | `						return SXERR_ABORT;` |
|    - |  852 | `					}` |
|  ! 0 |  853 | `				}` |
|  ! 0 |  854 | `				break;` |
|    - |  855 | `			}` |
|    - |  856 | `			/* Invoke the supplied callback if any */` |
|   23 |  857 | `			if( pParse->xRaw ){` |
|   23 |  858 | `				TokenToXMLString(pToken,&sEntry);` |
|   23 |  859 | `				rc = pParse->xRaw((SyXMLRawStr *)&sEntry,pParse->pUserData);` |
|   23 |  860 | `				if( rc == SXERR_ABORT ){` |
|  ! 0 |  861 | `					return SXERR_ABORT;` |
|    - |  862 | `				}` |
|   11 |  863 | `			}` |
|   23 |  864 | `			break;` |
|   27 |  865 | `		case SXML_TOK_END_TAG:{` |
|   55 |  866 | `			SyXMLRawStrNS *pLast = 0; /* cc warning */` |
|   55 |  867 | `			if( SySetUsed(pTagStack) < 1 ){` |
|  ! 0 |  868 | `				if( pParse->xError ){` |
|  ! 0 |  869 | `					rc = pParse->xError("Unexpected closing tag",SXML_ERROR_TAG_MISMATCH,pToken,pParse->pUserData);` |
|  ! 0 |  870 | `					if( rc == SXERR_ABORT ){` |
|  ! 0 |  871 | `						return SXERR_ABORT;` |
|    - |  872 | `					}` |
|  ! 0 |  873 | `				}` |
|  ! 0 |  874 | `				break;` |
|    - |  875 | `			}` |
|   55 |  876 | `			rc = XMLExtractEndTag(pParse,pToken,&sEntry);` |
|   55 |  877 | `			if( rc == SXRET_OK ){` |
|    - |  878 | `				/* Extract the last inserted entry */` |
|   55 |  879 | `				pLast = (SyXMLRawStrNS *)SySetPeek(pTagStack);` |
|   82 |  880 | `				if( pLast == 0 \|\| pLast->nByte != sEntry.nByte \|\|` |
|   54 |  881 | `					SyMemcmp(pLast->zString,sEntry.zString,sEntry.nByte) != 0 ){` |
|  ! 0 |  882 | `						if( pParse->xError ){` |
|  ! 0 |  883 | `							rc = pParse->xError("Unexpected closing tag",SXML_ERROR_TAG_MISMATCH,pToken,pParse->pUserData);` |
|  ! 0 |  884 | `							if( rc == SXERR_ABORT ){` |
|  ! 0 |  885 | `								return SXERR_ABORT;` |
|    - |  886 | `							}` |
|  ! 0 |  887 | `						}` |
|  ! 0 |  888 | `				}else{` |
|    - |  889 | `					/* Invoke the supplied callback if any */` |
|   55 |  890 | `					if( pParse->xEndTag ){` |
|   55 |  891 | `						rc = SXRET_OK;` |
|   55 |  892 | `						if( pParse->nFlags & SXML_ENABLE_NAMESPACE ){` |
|    - |  893 | `							/* Extract namespace URI */` |
|    9 |  894 | `							rc = XMLExtractNS(pParse,pToken,&sEntry,&sNs);` |
|    9 |  895 | `							if( rc == SXERR_ABORT ){` |
|  ! 0 |  896 | `								return SXERR_ABORT;` |
|    - |  897 | `							}` |
|    4 |  898 | `						}` |
|   55 |  899 | `						if( rc == SXRET_OK ){` |
|   55 |  900 | `							rc = pParse->xEndTag((SyXMLRawStr *)&sEntry,&sNs,pParse->pUserData);` |
|   55 |  901 | `							if( rc == SXERR_ABORT ){` |
|  ! 0 |  902 | `								return SXERR_ABORT;` |
|    - |  903 | `							}` |
|   27 |  904 | `						}` |
|   27 |  905 | `					}` |
|    1 |  906 | `				}` |
|   27 |  907 | `			}else if( rc == SXERR_ABORT ){` |
|  ! 0 |  908 | `				return SXERR_ABORT;` |
|    - |  909 | `			}` |
|   55 |  910 | `			if( pLast ){` |
|   55 |  911 | `				rc = XMLnsUnlink(pParse,pLast,pToken);` |
|   55 |  912 | `				(void)SySetPop(pTagStack);` |
|   55 |  913 | `				if( rc == SXERR_ABORT ){` |
|  ! 0 |  914 | `					return SXERR_ABORT;` |
|    - |  915 | `				}` |
|   27 |  916 | `			}` |
|   55 |  917 | `			break;` |
|    - |  918 | `							  }` |
|   40 |  919 | `		case SXML_TOK_START_TAG:` |
|    - |  920 | `		case SXML_TOK_START_END:` |
|   81 |  921 | `			if( SySetUsed(pTagStack) < 1 && bGotTag ){` |
|  ! 0 |  922 | `				if( pParse->xError ){` |
|  ! 0 |  923 | `					rc = pParse->xError("XML document cannot contain multiple root level elements documents",` |
|  ! 0 |  924 | `						SXML_ERROR_SYNTAX,pToken,pParse->pUserData);` |
|  ! 0 |  925 | `					if( rc == SXERR_ABORT ){` |
|  ! 0 |  926 | `						return SXERR_ABORT;` |
|    - |  927 | `					}` |
|  ! 0 |  928 | `				}` |
|  ! 0 |  929 | `				break;` |
|    - |  930 | `			}` |
|   81 |  931 | `			bGotTag = 1;` |
|    - |  932 | `			/* Extract the tag and it's supplied attribute */` |
|   81 |  933 | `			rc = XMLProcessStartTag(pParse,pToken,&sEntry,pWorker,pTagStack);` |
|   81 |  934 | `			if( rc == SXRET_OK ){` |
|   79 |  935 | `				if( pParse->nFlags & SXML_ENABLE_NAMESPACE ){` |
|    - |  936 | `					/* Extract namespace URI */` |
|    9 |  937 | `					rc = XMLExtractNS(pParse,pToken,&sEntry,&sNs);` |
|    4 |  938 | `				}` |
|   39 |  939 | `			}` |
|   81 |  940 | `			if( rc == SXRET_OK ){` |
|    - |  941 | `				/* Invoke the supplied callback */` |
|   79 |  942 | `				if( pParse->xStartTag ){` |
|  118 |  943 | `					rc = pParse->xStartTag((SyXMLRawStr *)&sEntry,&sNs,SySetUsed(pWorker),` |
|   78 |  944 | `						(SyXMLRawStr *)SySetBasePtr(pWorker),pParse->pUserData);` |
|   79 |  945 | `					if( rc == SXERR_ABORT ){` |
|  ! 0 |  946 | `						return SXERR_ABORT;` |
|    - |  947 | `					}` |
|   39 |  948 | `				}` |
|   79 |  949 | `				if( pToken->nType == SXML_TOK_START_END ){` |
|    9 |  950 | `					if ( pParse->xEndTag ){` |
|    9 |  951 | `						rc = pParse->xEndTag((SyXMLRawStr *)&sEntry,&sNs,pParse->pUserData);` |
|    9 |  952 | `						if( rc == SXERR_ABORT ){` |
|  ! 0 |  953 | `							return SXERR_ABORT;` |
|    - |  954 | `						}` |
|    4 |  955 | `					}` |
|    9 |  956 | `					rc = XMLnsUnlink(pParse,&sEntry,pToken);` |
|    9 |  957 | `					if( rc == SXERR_ABORT ){` |
|  ! 0 |  958 | `						return SXERR_ABORT;` |
|    - |  959 | `					}` |
|    5 |  960 | `				}` |
|   42 |  961 | `			}else if( rc == SXERR_ABORT ){` |
|    - |  962 | `				/* Abort processing immediately */` |
|    3 |  963 | `				return SXERR_ABORT;` |
|    - |  964 | `			}` |
|   78 |  965 | `			break;` |
|  ! 0 |  966 | `		default:` |
|    - |  967 | `			/* Can't happen */` |
|  ! 0 |  968 | `			break;` |
|    - |  969 | `		}` |
|    1 |  970 | `	}` |
|   43 |  971 | `	if( SySetUsed(pTagStack) > 0 && pParse->xError){` |
|    7 |  972 | `		pParse->xError("Missing closing tag",SXML_ERROR_SYNTAX,` |
|    4 |  973 | `			(SyToken *)SySetPeek(&pParse->sToken),pParse->pUserData);` |
|    2 |  974 | `	}` |
|   43 |  975 | `	if( pParse->xEndDoc ){` |
|  ! 0 |  976 | `		pParse->xEndDoc(pParse->pUserData);` |
|  ! 0 |  977 | `	}` |
|   43 |  978 | `	return SXRET_OK;` |
|   27 |  979 |  |
|   84 |  980 | `PH7_PRIVATE sxi32 SyXMLParserInit(SyXMLParser *pParser,SyMemBackend *pAllocator,sxi32 iFlags)` |
|    1 |  981 |  |
|    - |  982 | `	/* Zero the structure first */` |
|   85 |  983 | `	SyZero(pParser,sizeof(SyXMLParser));` |
|    - |  984 | `	/* Initialize fields */` |
|   85 |  985 | `	SySetInit(&pParser->sToken,pAllocator,sizeof(SyToken));` |
|   85 |  986 | `	SyLexInit(&pParser->sLex,&pParser->sToken,XML_Tokenize,pParser);` |
|   85 |  987 | `	SyHashInit(&pParser->hns,pAllocator,0,0);` |
|   85 |  988 | `	pParser->pAllocator = pAllocator;` |
|   85 |  989 | `	pParser->nFlags = iFlags;` |
|   85 |  990 | `	return SXRET_OK;` |
|    1 |  991 |  |
|   74 |  992 | `PH7_PRIVATE sxi32 SyXMLParserSetEventHandler(SyXMLParser *pParser,` |
|    - |  993 | `	void *pUserData,` |
|    - |  994 | `	ProcXMLStartTagHandler xStartTag,` |
|    - |  995 | `	ProcXMLTextHandler xRaw,` |
|    - |  996 | `	ProcXMLSyntaxErrorHandler xErr,` |
|    - |  997 | `	ProcXMLStartDocument xStartDoc,` |
|    - |  998 | `	ProcXMLEndTagHandler xEndTag,` |
|    - |  999 | `	ProcXMLPIHandler   xPi,` |
|    - | 1000 | `	ProcXMLEndDocument xEndDoc,` |
|    - | 1001 | `	ProcXMLDoctypeHandler xDoctype,` |
|    - | 1002 | `	ProcXMLNameSpaceStart xNameSpace,` |
|    - | 1003 | `	ProcXMLNameSpaceEnd   xNameSpaceEnd` |
|    1 | 1004 | `	){` |
|    - | 1005 | `	/* Install user callbacks */` |
|   75 | 1006 | `	if( xErr ){` |
|   75 | 1007 | `		pParser->xError = xErr;` |
|   37 | 1008 | `	}` |
|   75 | 1009 | `	if( xStartDoc ){` |
|  ! 0 | 1010 | `		pParser->xStartDoc = xStartDoc;` |
|  ! 0 | 1011 | `	}` |
|   75 | 1012 | `	if( xStartTag ){` |
|   75 | 1013 | `		pParser->xStartTag = xStartTag;` |
|   37 | 1014 | `	}` |
|   75 | 1015 | `	if( xRaw ){` |
|   75 | 1016 | `		pParser->xRaw = xRaw;` |
|   37 | 1017 | `	}` |
|   75 | 1018 | `	if( xEndTag ){` |
|   75 | 1019 | `		pParser->xEndTag = xEndTag;` |
|   37 | 1020 | `	}` |
|   75 | 1021 | `	if( xPi ){` |
|   75 | 1022 | `		pParser->xPi = xPi;` |
|   37 | 1023 | `	}` |
|   75 | 1024 | `	if( xEndDoc ){` |
|  ! 0 | 1025 | `		pParser->xEndDoc = xEndDoc;` |
|  ! 0 | 1026 | `	}` |
|   75 | 1027 | `	if( xDoctype ){` |
|  ! 0 | 1028 | `		pParser->xDoctype = xDoctype;` |
|  ! 0 | 1029 | `	}` |
|   75 | 1030 | `	if( xNameSpace ){` |
|   75 | 1031 | `		pParser->xNameSpace	= xNameSpace;` |
|   37 | 1032 | `	}` |
|   75 | 1033 | `	if( xNameSpaceEnd ){` |
|   75 | 1034 | `		pParser->xNameSpaceEnd = xNameSpaceEnd;` |
|   37 | 1035 | `	}` |
|   75 | 1036 | `	pParser->pUserData = pUserData;` |
|   75 | 1037 | `	return SXRET_OK;` |
|    1 | 1038 |  |
|    - | 1039 | `/* Process an XML chunk */` |
|   74 | 1040 | `PH7_PRIVATE sxi32 SyXMLProcess(SyXMLParser *pParser,const char *zInput,sxu32 nByte)` |
|    1 | 1041 |  |
|    - | 1042 | `	SySet sTagStack;` |
|    - | 1043 | `	SySet sWorker;` |
|    - | 1044 | `	sxi32 rc;` |
|    - | 1045 | `	/* Initialize working sets */` |
|   75 | 1046 | `	SySetInit(&sWorker,pParser->pAllocator,sizeof(SyXMLRawStr)); /* Tag container */` |
|   75 | 1047 | `	SySetInit(&sTagStack,pParser->pAllocator,sizeof(SyXMLRawStrNS)); /* Tag stack */` |
|    - | 1048 | `	/* Tokenize the entire input */` |
|   75 | 1049 | `	rc = SyLexTokenizeInput(&pParser->sLex,zInput,nByte,0,0,0);` |
|   75 | 1050 | `	if( rc == SXERR_ABORT ){` |
|    - | 1051 | `		/* Tokenize callback request an operation abort */` |
|   21 | 1052 | `		return SXERR_ABORT;` |
|    - | 1053 | `	}` |
|   55 | 1054 | `	if( SySetUsed(&pParser->sToken) < 1 ){` |
|    - | 1055 | `		/* Nothing to process [i.e: white spaces] */` |
|    3 | 1056 | `		rc = SXRET_OK;` |
|    2 | 1057 | `	}else{` |
|    - | 1058 | `		/* Process XML Tokens */` |
|   53 | 1059 | `		rc = ProcessXML(&(*pParser),&sTagStack,&sWorker);` |
|   53 | 1060 | `		if( pParser->nFlags & SXML_ENABLE_NAMESPACE ){` |
|    5 | 1061 | `			if( SySetUsed(&sTagStack) > 0  ){` |
|    - | 1062 | `				SyXMLRawStrNS *pEntry;` |
|    - | 1063 | `				SyHashEntry **apEntry;` |
|    - | 1064 | `				sxu32 n;` |
|  ! 0 | 1065 | `				SySetResetCursor(&sTagStack);` |
|  ! 0 | 1066 | `				while( SySetGetNextEntry(&sTagStack,(void **)&pEntry) == SXRET_OK ){` |
|    - | 1067 | `					/* Release namespace entries */` |
|  ! 0 | 1068 | `					apEntry = (SyHashEntry **)SySetBasePtr(&pEntry->sNSset);` |
|  ! 0 | 1069 | `					for( n = 0 ; n < SySetUsed(&pEntry->sNSset) ; ++n ){` |
|  ! 0 | 1070 | `						SyMemBackendFree(pParser->pAllocator,apEntry[n]->pUserData);` |
|  ! 0 | 1071 | `					}` |
|  ! 0 | 1072 | `					SySetRelease(&pEntry->sNSset);` |
|  ! 0 | 1073 | `				}` |
|  ! 0 | 1074 | `			}` |
|    2 | 1075 | `		}` |
|    - | 1076 | `	}` |
|    - | 1077 | `	/* Clean-up the mess left behind */` |
|   55 | 1078 | `	SySetRelease(&sWorker);` |
|   55 | 1079 | `	SySetRelease(&sTagStack);` |
|    - | 1080 | `	/* Processing result */` |
|   55 | 1081 | `	return rc;` |
|   38 | 1082 |  |
|   84 | 1083 | `PH7_PRIVATE sxi32 SyXMLParserRelease(SyXMLParser *pParser)` |
|    1 | 1084 |  |
|   85 | 1085 | `	SyLexRelease(&pParser->sLex);` |
|   85 | 1086 | `	SySetRelease(&pParser->sToken);` |
|   85 | 1087 | `	SyHashRelease(&pParser->hns);` |
|   85 | 1088 | `	return SXRET_OK;` |
|    1 | 1089 |  |
|    - | 1090 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    - | 1091 |  |
