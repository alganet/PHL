# src/ph7/compile_literal.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 945/1060 lines (89.15%)

[Root index](../../index.md) | [Directory index](index.md)

|     Hits | Line | Source |
| -------: | ---: | :--- |
|        - |    1 | `/**` |
|        - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|        - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|        - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|        - |    5 | ` */` |
|        - |    6 | `#include "ph7int.h"` |
|        - |    7 | `#include "compile_int.h"` |
|        - |    8 | `/*` |
|        - |    9 | ` * Section:` |
|        - |   10 | ` *    Literal compilation: numeric literals (incl. PHP 7.4 numeric` |
|        - |   11 | ` *    separators), simple/double-quoted strings, heredoc/nowdoc, array()` |
|        - |   12 | ` *    and [] literals, list() destructuring and clone-call rewriting.` |
|        - |   13 | ` * Status:` |
|        - |   14 | ` *    Stable.` |
|        - |   15 | ` */` |
|        - |   16 | `/*` |
|        - |   17 | ` * Return TRUE if c is a valid digit for the given numeric base.` |
|        - |   18 | ` *   base 16 => SyisHex (0-9, a-f, A-F)` |
|        - |   19 | ` *   base  2 => 0 or 1` |
|        - |   20 | ` *   base 10 => SyisDigit (0-9, also used for octal literals which share the` |
|        - |   21 | ` *              decimal scan in the lexer)` |
|        - |   22 | ` */` |
|     1080 |   23 | `static int GenStateIsBaseDigit(int c, int base)` |
|        5 |   24 | `{` |
|     1085 |   25 | `	if( base == 16 ){ return SyisHex(c); }` |
|      986 |   26 | `	if( base == 2 ){ return c == '0' \|\| c == '1'; }` |
|      707 |   27 | `	return SyisDigit(c);` |
|      545 |   28 | `}` |
|        - |   29 | `/*` |
|        - |   30 | ` * Given the raw text of a numeric literal token, locate a misplaced PHP 7.4` |
|        - |   31 | ` * underscore separator so the caller can report the malformed portion with` |
|        - |   32 | ` * the exact wording PHP uses:` |
|        - |   33 | ` *` |
|        - |   34 | ` *   syntax error, unexpected identifier "X"` |
|        - |   35 | ` *` |
|        - |   36 | ` * The lexer guarantees that every underscore it consumed as a separator is` |
|        - |   37 | ` * surrounded by valid base digits; anything else sits in the trailing run` |
|        - |   38 | ` * absorbed by the lexer specifically to let this validator see and report` |
|        - |   39 | ` * it. That invariant means the malformed span is exactly [bad .. nByte) —` |
|        - |   40 | ` * no forward rescan needed.` |
|        - |   41 | ` *` |
|        - |   42 | ` * Returns 1 and fills pBadStart / pBadLen when the literal is malformed;` |
|        - |   43 | ` * returns 0 when it is well-formed.` |
|        - |   44 | ` */` |
|  3821374 |   45 | `static int GenStateFindBadNumericSeparator(` |
|        - |   46 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|        5 |   47 | `{` |
|  3821379 |   48 | `	const char *z = pRaw->zString;` |
|  3821379 |   49 | `	sxu32 n = pRaw->nByte;` |
|  3821379 |   50 | `	int base = 10;` |
|        - |   51 | `	sxu32 i, start;` |
|  3821379 |   52 | `	if( n < 2 ) return 0;` |
|   804985 |   53 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|   104749 |   54 | `		base = 16;` |
|   752613 |   55 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|      286 |   56 | `		base = 2;` |
|      142 |   57 | `	}` |
|  3065413 |   58 | `	for( i = 0; i < n; ++i ){` |
|  2260447 |   59 | `		if( z[i] != '_' ) continue;` |
|      548 |   60 | `		if( i > 0 && i + 1 < n` |
|      545 |   61 | `			&& GenStateIsBaseDigit((unsigned char)z[i-1], base)` |
|      545 |   62 | `			&& GenStateIsBaseDigit((unsigned char)z[i+1], base) ){` |
|      535 |   63 | `			continue; /* well-placed separator */` |
|        - |   64 | `		}` |
|        - |   65 | `		/* First misplaced underscore — the lexer already absorbed the full` |
|        - |   66 | `		 * malformed tail, so it runs from here to the end of the token. */` |
|       18 |   67 | `		start = i;` |
|       23 |   68 | `		if( start > 0 && (z[start-1] == 'x' \|\| z[start-1] == 'X'` |
|       12 |   69 | `			\|\| z[start-1] == 'b' \|\| z[start-1] == 'B') ){` |
|        6 |   70 | `			start--; /* include the base letter for 0x_... / 0b_... */` |
|        2 |   71 | `		}` |
|       18 |   72 | `		*pBadStart = &z[start];` |
|       18 |   73 | `		*pBadLen = n - start;` |
|       18 |   74 | `		return 1;` |
|      ! 0 |   75 | `	}` |
|   804971 |   76 | `	return 0;` |
|  1910692 |   77 | `}` |
|        - |   78 | `/*` |
|        - |   79 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|        - |   80 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|        - |   81 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|        - |   82 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|        - |   83 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|        - |   84 | ` * so callers can bail from the current construct).` |
|        - |   85 | ` */` |
|  3821374 |   86 | `PH7_PRIVATE sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|        5 |   87 | `{` |
|  3821379 |   88 | `	const char *zBad = 0;` |
|  3821379 |   89 | `	sxu32 nBad = 0;` |
|        - |   90 | `	SyString sBad;` |
|        - |   91 | `	sxi32 rc;` |
|  3821379 |   92 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|  3821365 |   93 | `		return SXRET_OK;` |
|        - |   94 | `	}` |
|       18 |   95 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|       18 |   96 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|        - |   97 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|       18 |   98 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |   99 | `		return SXERR_ABORT;` |
|        - |  100 | `	}` |
|       18 |  101 | `	return SXERR_SYNTAX;` |
|  1910692 |  102 | `}` |
|        - |  103 | `/*` |
|        - |  104 | ` * Strip PHP 7.4 numeric literal separators (underscores between digits) from` |
|        - |  105 | ` * a numeric token's text and yield a SyString suitable for the low-level` |
|        - |  106 | ` * converters (SyStrToInt64 / SyStrToReal / etc.).` |
|        - |  107 | ` *` |
|        - |  108 | ` * Fast path: if the token contains no '_', *pOut aliases pToken with no copy` |
|        - |  109 | ` * and *pzAlloc is set to NULL.` |
|        - |  110 | ` * Stack path: if the cleaned bytes fit in zScratch, they are written there` |
|        - |  111 | ` * and *pzAlloc is set to NULL.` |
|        - |  112 | ` * Heap path: for literals larger than the scratch buffer, a fresh buffer is` |
|        - |  113 | ` * allocated from pAlloc, returned via *pzAlloc, and must be released by the` |
|        - |  114 | ` * caller with SyMemBackendFree once the converter is done.` |
|        - |  115 | ` *` |
|        - |  116 | ` * Returns SXRET_OK on success, SXERR_ABORT on allocator failure (in which` |
|        - |  117 | ` * case *pOut is left untouched and the caller must not read it).` |
|        - |  118 | ` */` |
|  3821360 |  119 | `PH7_PRIVATE sxi32 GenStateStripNumericSeparators(` |
|        - |  120 | `	SyMemBackend *pAlloc,` |
|        - |  121 | `	const SyString *pToken,` |
|        - |  122 | `	char *zScratch, sxu32 nScratch,` |
|        - |  123 | `	SyString *pOut, char **pzAlloc)` |
|        5 |  124 | `{` |
|        - |  125 | `	sxu32 i, j;` |
|  3821365 |  126 | `	int hasUnderscore = 0;` |
|        - |  127 | `	char *zBuf;` |
|  3821365 |  128 | `	*pzAlloc = 0;` |
|  9096111 |  129 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|  5275005 |  130 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|  2637378 |  131 | `	}` |
|  3821365 |  132 | `	if( !hasUnderscore ){` |
|  3821111 |  133 | `		SyStringDupPtr(pOut, pToken);` |
|  3821111 |  134 | `		return SXRET_OK;` |
|        - |  135 | `	}` |
|      255 |  136 | `	if( pToken->nByte <= nScratch ){` |
|      253 |  137 | `		zBuf = zScratch;` |
|      127 |  138 | `	}else{` |
|        3 |  139 | `		zBuf = (char *)SyMemBackendAlloc(pAlloc, pToken->nByte);` |
|        3 |  140 | `		if( zBuf == 0 ){` |
|      ! 0 |  141 | `			return SXERR_ABORT;` |
|        - |  142 | `		}` |
|        3 |  143 | `		*pzAlloc = zBuf;` |
|        - |  144 | `	}` |
|      255 |  145 | `	j = 0;` |
|     2913 |  146 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|     2659 |  147 | `		if( pToken->zString[i] != '_' ){ zBuf[j++] = pToken->zString[i]; }` |
|     1330 |  148 | `	}` |
|      255 |  149 | `	SyStringInitFromBuf(pOut, zBuf, j);` |
|      255 |  150 | `	return SXRET_OK;` |
|  1910685 |  151 | `}` |
|        - |  152 | `/*` |
|        - |  153 | ` * Compile a numeric [i.e: integer or real] literal.` |
|        - |  154 | ` * Notes on the integer type.` |
|        - |  155 | ` *  According to the PHP language reference manual` |
|        - |  156 | ` *  Integers can be specified in decimal (base 10), hexadecimal (base 16), octal (base 8)` |
|        - |  157 | ` *  or binary (base 2) notation, optionally preceded by a sign (- or +).` |
|        - |  158 | ` *  To use octal notation, precede the number with a 0 (zero). To use hexadecimal` |
|        - |  159 | ` *  notation precede the number with 0x. To use binary notation precede the number with 0b.` |
|        - |  160 | ` * Symisc eXtension to the integer type.` |
|        - |  161 | ` *  PH7 introduced platform-independant 64-bit integer unlike the standard PHP engine` |
|        - |  162 | ` *  where the size of an integer is platform-dependent.That is,the size of an integer` |
|        - |  163 | ` *  is 8 bytes and the maximum integer size is 0x7FFFFFFFFFFFFFFF for all platforms` |
|        - |  164 | ` *  [i.e: either 32bit or 64bit].` |
|        - |  165 | ` *  For more information on this powerfull extension please refer to the official` |
|        - |  166 | ` *  documentation.` |
|        - |  167 | ` */` |
|        - |  168 | `/*` |
|        - |  169 | ` * Determine whether an integer literal token exceeds the signed 64-bit range.` |
|        - |  170 | ` * PHP promotes such a literal to a float (e.g. 9223372036854775808 ->` |
|        - |  171 | ` * float(9.22...E+18), 0xFFFFFFFFFFFFFFFF -> float) rather than wrapping or` |
|        - |  172 | ` * dropping digits. pNum is the separator-stripped token (unsigned; the sign of` |
|        - |  173 | ` * a "-1" is a separate unary operator). Base detection mirrors` |
|        - |  174 | ` * PH7_TokenValueToInt64. Returns TRUE on overflow: for a non-decimal base the` |
|        - |  175 | ` * float value is accumulated into *pReal (dv = dv*base + digit); for decimal` |
|        - |  176 | ` * *pbDecimal is set so the caller reuses strtod on the token for a` |
|        - |  177 | ` * correctly-rounded value. Returns FALSE (value fits) for anything it cannot` |
|        - |  178 | ` * confidently classify, so the int path stays in charge.` |
|        - |  179 | ` *` |
|        - |  180 | ` * The int/float CLASSIFICATION is php-exact for every base. VALUES are byte-exact` |
|        - |  181 | ` * for decimal (strtod) and hex (php's zend_hex_strtod uses the same dv*16+digit` |
|        - |  182 | ` * doubling). Octal/binary overflow values can differ from php by the low bit(s):` |
|        - |  183 | ` * php's zend_{oct,bin}_strtod rounds differently than this doubling — e.g. php's` |
|        - |  184 | ` * binary 2**63 is 2**63-1024 whereas this returns the exact 2**63. Recorded as a` |
|        - |  185 | ` * residual; matching php exactly would need a port of those functions.` |
|        - |  186 | ` */` |
|  3812626 |  187 | `static int GenStateIntLiteralOverflows(const SyString *pNum, ph7_real *pReal, int *pbDecimal)` |
|        5 |  188 | `{` |
|  3812631 |  189 | `	const char *z = pNum->zString;` |
|  3812631 |  190 | `	const char *zEnd = z + pNum->nByte;` |
|        - |  191 | `	const char *p, *q;` |
|        - |  192 | `	int n;` |
|  3812631 |  193 | `	*pbDecimal = FALSE;` |
|  3812631 |  194 | `	if( z >= zEnd ){` |
|      ! 0 |  195 | `		return FALSE;` |
|        - |  196 | `	}` |
|  3812631 |  197 | `	if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|        - |  198 | `		/* Hexadecimal: INT64_MAX == 0x7FFF...F (16 digits, leading nibble 7). */` |
|   104747 |  199 | `		p = z + 2;` |
|   131887 |  200 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|   426929 |  201 | `		for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisHex(q[0]); q++ ){ n++; }` |
|   104747 |  202 | `		if( n < 16 \|\| (n == 16 && SyHexToint(p[0]) < 8) ){` |
|   104741 |  203 | `			return FALSE;` |
|        - |  204 | `		}` |
|        7 |  205 | `		{ ph7_real dv = 0;` |
|      103 |  206 | `		  for( q = p; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisHex(q[0]); q++ ){` |
|       97 |  207 | `			dv = dv * 16 + (ph7_real)SyHexToint(q[0]);` |
|       49 |  208 | `		  }` |
|        7 |  209 | `		  *pReal = dv;` |
|        - |  210 | `		}` |
|        7 |  211 | `		return TRUE;` |
|  3707889 |  212 | `	}else if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|        - |  213 | `		/* Binary: INT64_MAX needs 63 significant bits. */` |
|      283 |  214 | `		p = z + 2;` |
|      331 |  215 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|     2158 |  216 | `		for( q = p, n = 0; q < zEnd && (q[0] == '0' \|\| q[0] == '1'); q++ ){ n++; }` |
|      283 |  217 | `		if( n <= 63 ){` |
|      281 |  218 | `			return FALSE;` |
|        - |  219 | `		}` |
|        3 |  220 | `		{ ph7_real dv = 0;` |
|      195 |  221 | `		  for( q = p; q < zEnd && (q[0] == '0' \|\| q[0] == '1'); q++ ){` |
|      129 |  222 | `			dv = dv * 2 + (ph7_real)(q[0] - '0');` |
|       65 |  223 | `		  }` |
|        3 |  224 | `		  *pReal = dv;` |
|        - |  225 | `		}` |
|        3 |  226 | `		return TRUE;` |
|  3707607 |  227 | `	}else if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'o' \|\| z[1] == 'O') ){` |
|        - |  228 | `		/* PHP 8.1 explicit octal 0o/0O: 21 significant octal digits fit in int64. */` |
|       17 |  229 | `		p = z + 2;` |
|       21 |  230 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|       85 |  231 | `		for( q = p, n = 0; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){ n++; }` |
|       17 |  232 | `		if( n <= 21 ){` |
|       17 |  233 | `			return FALSE;` |
|        - |  234 | `		}` |
|      ! 0 |  235 | `		{ ph7_real dv = 0;` |
|      ! 0 |  236 | `		  for( q = p; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){` |
|      ! 0 |  237 | `			dv = dv * 8 + (ph7_real)(q[0] - '0');` |
|      ! 0 |  238 | `		  }` |
|      ! 0 |  239 | `		  *pReal = dv;` |
|        - |  240 | `		}` |
|      ! 0 |  241 | `		return TRUE;` |
|  3707591 |  242 | `	}else if( z[0] == '0' ){` |
|        - |  243 | `		/* Octal: INT64_MAX == 0o777...7 (21 significant octal digits). Skip the` |
|        - |  244 | `		 * leading zeros (incl. the base '0'); a non-octal char such as the 8.1` |
|        - |  245 | `		 * "0o" marker ends the run and leaves it to the int path (as today). */` |
|  1393921 |  246 | `		p = z;` |
|  2787839 |  247 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|  1405805 |  248 | `		for( q = p, n = 0; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){ n++; }` |
|  1393921 |  249 | `		if( n <= 21 ){` |
|  1393919 |  250 | `			return FALSE;` |
|        - |  251 | `		}` |
|        3 |  252 | `		{ ph7_real dv = 0;` |
|       47 |  253 | `		  for( q = p; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){` |
|       45 |  254 | `			dv = dv * 8 + (ph7_real)(q[0] - '0');` |
|       23 |  255 | `		  }` |
|        3 |  256 | `		  *pReal = dv;` |
|        - |  257 | `		}` |
|        3 |  258 | `		return TRUE;` |
|        - |  259 | `	}` |
|        - |  260 | `	/* Decimal: overflow iff more than 19 significant digits, or exactly 19 that` |
|        - |  261 | `	 * compare greater than INT64_MAX. Defer the value to strtod (via the caller)` |
|        - |  262 | `	 * for php-exact rounding. */` |
|  2313675 |  263 | `	p = z;` |
|  2313675 |  264 | `	while( p < zEnd && p[0] == '0' ){ p++; }` |
|  5594609 |  265 | `	for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisDigit(q[0]); q++ ){ n++; }` |
|  2313675 |  266 | `	if( n > 19 \|\| (n == 19 && SyMemcmp(p, "9223372036854775807", 19) > 0) ){` |
|       25 |  267 | `		*pbDecimal = TRUE;` |
|       25 |  268 | `		return TRUE;` |
|        - |  269 | `	}` |
|  2313651 |  270 | `	return FALSE;` |
|  1906318 |  271 | `}` |
|  3821346 |  272 | `PH7_PRIVATE sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  273 | `{` |
|  3821351 |  274 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|  3821351 |  275 | `	sxu32 nIdx = 0;` |
|        - |  276 | `	char zScratch[GEN_NUM_SCRATCH];` |
|  3821351 |  277 | `	char *zAlloc = 0;` |
|        - |  278 | `	SyString sNum;` |
|        - |  279 | `	sxi32 rc;` |
|  1910673 |  280 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|  3821351 |  281 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|  3821351 |  282 | `	if( rc != SXRET_OK ){` |
|       14 |  283 | `		return rc;` |
|        - |  284 | `	}` |
|  5732009 |  285 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|  1910668 |  286 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|  3821341 |  287 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  288 | `		return SXERR_ABORT;` |
|        - |  289 | `	}` |
|  3821341 |  290 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|        - |  291 | `		ph7_value *pObj;` |
|        - |  292 | `		sxi64 iValue;` |
|  3812631 |  293 | `		ph7_real rOverflow = 0;` |
|  3812631 |  294 | `		int bDecimalOverflow = 0;` |
|  3812631 |  295 | `		if( GenStateIntLiteralOverflows(&sNum,&rOverflow,&bDecimalOverflow) ){` |
|        - |  296 | `			/* Literal exceeds the signed 64-bit range: PHP represents it as a` |
|        - |  297 | `			 * float instead of wrapping/dropping digits. */` |
|       35 |  298 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       35 |  299 | `			if( pObj == 0 ){` |
|      ! 0 |  300 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |  301 | `				if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|      ! 0 |  302 | `				return SXERR_ABORT;` |
|        - |  303 | `			}` |
|       35 |  304 | `			if( bDecimalOverflow ){` |
|        - |  305 | `				/* strtod on the decimal token yields php-exact rounding. */` |
|       25 |  306 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|       25 |  307 | `				PH7_MemObjToReal(pObj);` |
|       13 |  308 | `			}else{` |
|       11 |  309 | `				PH7_MemObjInitFromReal(pGen->pVm,pObj,rOverflow);` |
|        - |  310 | `			}` |
|       18 |  311 | `		}else{` |
|  3812597 |  312 | `			iValue = PH7_TokenValueToInt64(&sNum);` |
|  3812597 |  313 | `			pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|  3812597 |  314 | `			if( pObj == 0 ){` |
|      ! 0 |  315 | `				if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|      ! 0 |  316 | `				return SXERR_ABORT;` |
|        - |  317 | `			}` |
|  3812597 |  318 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|        - |  319 | `		}` |
|  1906318 |  320 | `	}else{` |
|        - |  321 | `		/* Real number */` |
|        - |  322 | `		ph7_value *pObj;` |
|        - |  323 | `		/* Reserve a new constant */` |
|     8715 |  324 | `		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     8715 |  325 | `		if( pObj == 0 ){` |
|      ! 0 |  326 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |  327 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|      ! 0 |  328 | `			return SXERR_ABORT;` |
|        - |  329 | `		}` |
|     8715 |  330 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|     8715 |  331 | `		PH7_MemObjToReal(pObj);` |
|        - |  332 | `	}` |
|  3821341 |  333 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|        - |  334 | `	/* Emit the load constant instruction */` |
|  3821341 |  335 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        - |  336 | `	/* Node successfully compiled */` |
|  3821341 |  337 | `	return SXRET_OK;` |
|  1910678 |  338 | `}` |
|        - |  339 | `/*` |
|        - |  340 | ` * Compile a single quoted string.` |
|        - |  341 | ` * According to the PHP language reference manual:` |
|        - |  342 | ` *` |
|        - |  343 | ` *   The simplest way to specify a string is to enclose it in single quotes (the character ' ).` |
|        - |  344 | ` *   To specify a literal single quote, escape it with a backslash (\). To specify a literal` |
|        - |  345 | ` *   backslash, double it (\\). All other instances of backslash will be treated as a literal` |
|        - |  346 | ` *   backslash: this means that the other escape sequences you might be used to, such as \r` |
|        - |  347 | ` *   or \n, will be output literally as specified rather than having any special meaning.` |
|        - |  348 | ` *` |
|        - |  349 | ` */` |
|  5555836 |  350 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  351 | `{` |
|  5555841 |  352 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|        - |  353 | `	const char *zIn,*zCur,*zEnd;` |
|        - |  354 | `	ph7_value *pObj;` |
|        - |  355 | `	sxu32 nIdx;` |
|        - |  356 | `	sxi32 bHasEsc;` |
|  5555841 |  357 | `	nIdx = 0; /* Prevent compiler warning */` |
|        - |  358 | `	/* Delimit the string */` |
|  5555841 |  359 | `	zIn  = pStr->zString;` |
|  5555841 |  360 | `	zEnd = &zIn[pStr->nByte];` |
|  5555841 |  361 | `	if( zIn >= zEnd ){` |
|        - |  362 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|        - |  363 | `		 * rather than reserving a new object each time. */` |
|   407211 |  364 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|   407211 |  365 | `		return SXRET_OK;` |
|        - |  366 | `	}` |
|        - |  367 | `	/* A single-quoted literal whose raw source holds a backslash unescapes to a` |
|        - |  368 | `	 * value that differs from that source (\\ -> \, \' -> '). The literal cache` |
|        - |  369 | `	 * keys FIND on the raw source text but INSTALL on the unescaped value, so` |
|        - |  370 | `	 * caching such a literal lets an unrelated one collide with it — e.g. '\\'` |
|        - |  371 | `	 * (raw \\, value \) would find the entry installed for '\\\\' (raw \\\\,` |
|        - |  372 | `	 * value \\) and load two backslashes. Only cache literals whose value equals` |
|        - |  373 | `	 * their source, i.e. those with no backslash to unescape. */` |
|  5148635 |  374 | `	bHasEsc = 0;` |
|        - |  375 | `	{` |
|        - |  376 | `		const char *zScan;` |
| 61523359 |  377 | `		for( zScan = zIn ; zScan < zEnd ; zScan++ ){` |
| 56456309 |  378 | `			if( zScan[0] == '\\' ){ bHasEsc = 1; break; }` |
| 28187367 |  379 | `		}` |
|        - |  380 | `	}` |
|  5148635 |  381 | `	if( !bHasEsc && SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|        - |  382 | `		/* Already processed,emit the load constant instruction` |
|        - |  383 | `		 * and return.` |
|        - |  384 | `		 */` |
|  3004515 |  385 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|  3004515 |  386 | `		return SXRET_OK;` |
|        - |  387 | `	}` |
|        - |  388 | `	/* Reserve a new constant */` |
|  2144125 |  389 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  2144125 |  390 | `	if( pObj == 0 ){` |
|      ! 0 |  391 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |  392 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|      ! 0 |  393 | `		return SXERR_ABORT;` |
|        - |  394 | `	}` |
|  2144125 |  395 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|        - |  396 | `	/* Compile the node */` |
|  2196556 |  397 | `	for(;;){` |
|  4393117 |  398 | `		if( zIn >= zEnd ){` |
|        - |  399 | `			/* End of input */` |
|  2144125 |  400 | `			break;` |
|        - |  401 | `		}` |
|  2248997 |  402 | `		zCur = zIn;` |
| 43457565 |  403 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
| 41208573 |  404 | `			zIn++;` |
|        5 |  405 | `		}` |
|  2248997 |  406 | `		if( zIn > zCur ){` |
|        - |  407 | `			/* Append raw contents*/` |
|  2206285 |  408 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|  1103140 |  409 | `		}` |
|  2248997 |  410 | `		zIn++;` |
|  2248997 |  411 | `		if( zIn < zEnd ){` |
|   143683 |  412 | `			if( zIn[0] == '\\' ){` |
|        - |  413 | `				/* A literal backslash */` |
|    34959 |  414 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|   126206 |  415 | `			}else if( zIn[0] == '\'' ){` |
|        - |  416 | `				/* A single quote */` |
|       15 |  417 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|        8 |  418 | `			}else{` |
|        - |  419 | `				/* verbatim copy */` |
|   108715 |  420 | `				zIn--;` |
|   108715 |  421 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char)*2);` |
|   108715 |  422 | `				zIn++;` |
|        - |  423 | `			}` |
|    71839 |  424 | `		}` |
|        - |  425 | `		/* Advance the stream cursor */` |
|  2248997 |  426 | `		zIn++;` |
|        5 |  427 | `	}` |
|        - |  428 | `	/* Emit the load constant instruction */` |
|  2144125 |  429 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|  2144125 |  430 | `	if( !bHasEsc && pStr->nByte < 1024 ){` |
|        - |  431 | `		/* Install in the literal table (only when value == source; see above) */` |
|  2062545 |  432 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|  1031270 |  433 | `	}` |
|        - |  434 | `	/* Node successfully compiled */` |
|  2144125 |  435 | `	return SXRET_OK;` |
|  2777923 |  436 | `}` |
|        - |  437 | `/*` |
|        - |  438 | ` * PHP 7.3 flexible heredoc/nowdoc closing-marker indent stripping.` |
|        - |  439 | ` *` |
|        - |  440 | ` * When the lexer matched the closing marker with leading whitespace on its` |
|        - |  441 | ` * own line, it stored the indent count in pGen->pIn->pUserData. The marker's` |
|        - |  442 | ` * indent prefix bytes sit immediately after the stripped body (at` |
|        - |  443 | ` * pIn->sData.zString + pIn->sData.nByte + 1 for LF, +2 for CRLF) in the` |
|        - |  444 | ` * original source buffer — the buffer is stable through compilation.` |
|        - |  445 | ` *` |
|        - |  446 | `` * For each body line, we remove exactly `nIndent` leading bytes that must`` |
|        - |  447 | ` * byte-for-byte match the marker's prefix. Empty lines (0 bytes or bare \r)` |
|        - |  448 | ` * bypass validation. Mismatches raise the exact PHP 7.3+ parse errors:` |
|        - |  449 | ` *   - "Invalid body indentation level (expecting an indentation level of` |
|        - |  450 | ` *     at least N)" — line too short, or first differing byte is not` |
|        - |  451 | ` *     whitespace.` |
|        - |  452 | ` *   - "Invalid indentation - tabs and spaces cannot be mixed" — first` |
|        - |  453 | ` *     differing byte is whitespace but differs from the marker prefix.` |
|        - |  454 | ` */` |
|      120 |  455 | `static sxi32 GenStateStripHeredocIndent(ph7_gen_state *pGen, SyString *pOut)` |
|        5 |  456 | `{` |
|      125 |  457 | `	SyString *pIn = &pGen->pIn->sData;` |
|      125 |  458 | `	sxu32 nIndent = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        - |  459 | `	const char *zPrefix;` |
|        - |  460 | `	const char *z, *zEnd;` |
|        - |  461 | `	char *zBuf, *zDst;` |
|      125 |  462 | `	if( nIndent == 0 ){` |
|        - |  463 | `		/* Legacy column-0 marker: zero-copy fast path */` |
|       80 |  464 | `		*pOut = *pIn;` |
|       80 |  465 | `		return SXRET_OK;` |
|        - |  466 | `	}` |
|        - |  467 | `	/* Recover the marker indent prefix from the original source buffer.` |
|        - |  468 | `	 * Skip the terminator the lexer stripped: one '\n' plus an optional` |
|        - |  469 | `	 * preceding '\r'. Note: when the body is empty (pIn->nByte == 0) the` |
|        - |  470 | `	 * lexer stripped nothing, so this offset is one byte past the true` |
|        - |  471 | `	 * marker-indent start. That is harmless — the strip loop below never` |
|        - |  472 | `	 * runs (z == zEnd), and zPrefix is never dereferenced. */` |
|       49 |  473 | `	zPrefix = pIn->zString + pIn->nByte;` |
|       49 |  474 | `	if( zPrefix[0] == '\r' && zPrefix[1] == '\n' ){` |
|      ! 0 |  475 | `		zPrefix += 2;` |
|      ! 0 |  476 | `	}else{` |
|       49 |  477 | `		zPrefix += 1;` |
|        - |  478 | `	}` |
|        - |  479 | `	/* Allocate scratch buffer sized to the original body (always enough). */` |
|       49 |  480 | `	zBuf = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator, pIn->nByte + 1);` |
|       49 |  481 | `	if( zBuf == 0 ){` |
|      ! 0 |  482 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|      ! 0 |  483 | `		return SXERR_ABORT;` |
|        - |  484 | `	}` |
|       49 |  485 | `	zDst = zBuf;` |
|       49 |  486 | `	z = pIn->zString;` |
|       49 |  487 | `	zEnd = z + pIn->nByte;` |
|      131 |  488 | `	while( z < zEnd ){` |
|       73 |  489 | `		const char *zLine = z;` |
|        - |  490 | `		sxu32 nLine;` |
|        - |  491 | `		int bEmpty;` |
|      801 |  492 | `		while( z < zEnd && z[0] != '\n' ){` |
|      733 |  493 | `			z++;` |
|        5 |  494 | `		}` |
|       73 |  495 | `		nLine = (sxu32)(z - zLine);` |
|       73 |  496 | `		bEmpty = (nLine == 0) \|\| (nLine == 1 && zLine[0] == '\r');` |
|       73 |  497 | `		if( !bEmpty ){` |
|        - |  498 | `			sxu32 i;` |
|       69 |  499 | `			if( nLine < nIndent ){` |
|      ! 0 |  500 | `				PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - |  501 | `					"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|      ! 0 |  502 | `					nIndent);` |
|      ! 0 |  503 | `				return SXERR_ABORT;` |
|        - |  504 | `			}` |
|      271 |  505 | `			for( i = 0; i < nIndent; i++ ){` |
|      215 |  506 | `				if( zLine[i] != zPrefix[i] ){` |
|       12 |  507 | `					unsigned char c = (unsigned char)zLine[i];` |
|       12 |  508 | `					if( c == ' ' \|\| c == '\t' ){` |
|        6 |  509 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - |  510 | `							"Invalid indentation - tabs and spaces cannot be mixed");` |
|        4 |  511 | `					}else{` |
|        8 |  512 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - |  513 | `							"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|        2 |  514 | `							nIndent);` |
|        - |  515 | `					}` |
|       12 |  516 | `					return SXERR_ABORT;` |
|        - |  517 | `				}` |
|      104 |  518 | `			}` |
|       57 |  519 | `			SyMemcpy((const void *)(zLine + nIndent), (void *)zDst, nLine - nIndent);` |
|       57 |  520 | `			zDst += nLine - nIndent;` |
|       33 |  521 | `		}else if( nLine == 1 ){` |
|        - |  522 | `			/* Preserve the stray '\r' on an otherwise empty line */` |
|      ! 0 |  523 | `			*zDst++ = '\r';` |
|      ! 0 |  524 | `		}` |
|       61 |  525 | `		if( z < zEnd ){` |
|       25 |  526 | `			*zDst++ = '\n';` |
|       25 |  527 | `			z++;` |
|       12 |  528 | `		}` |
|        1 |  529 | `	}` |
|       37 |  530 | `	pOut->zString = zBuf;` |
|       37 |  531 | `	pOut->nByte = (sxu32)(zDst - zBuf);` |
|       37 |  532 | `	return SXRET_OK;` |
|       65 |  533 | `}` |
|        - |  534 | `/*` |
|        - |  535 | ` * Compile a nowdoc string.` |
|        - |  536 | ` * According to the PHP language reference manual:` |
|        - |  537 | ` *` |
|        - |  538 | ` *  Nowdocs are to single-quoted strings what heredocs are to double-quoted strings.` |
|        - |  539 | ` *  A nowdoc is specified similarly to a heredoc, but no parsing is done inside a nowdoc.` |
|        - |  540 | ` *  The construct is ideal for embedding PHP code or other large blocks of text without the` |
|        - |  541 | ` *  need for escaping. It shares some features in common with the SGML <![CDATA[ ]]>` |
|        - |  542 | ` *  construct, in that it declares a block of text which is not for parsing.` |
|        - |  543 | ` *  A nowdoc is identified with the same <<< sequence used for heredocs, but the identifier` |
|        - |  544 | ` *  which follows is enclosed in single quotes, e.g. <<<'EOT'. All the rules for heredoc` |
|        - |  545 | ` *  identifiers also apply to nowdoc identifiers, especially those regarding the appearance` |
|        - |  546 | ` *  of the closing identifier.` |
|        - |  547 | ` */` |
|       52 |  548 | `PH7_PRIVATE sxi32 PH7_CompileNowDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        4 |  549 | `{` |
|        - |  550 | `	SyString sStripped;` |
|        - |  551 | `	SyString *pStr;` |
|        - |  552 | `	ph7_value *pObj;` |
|        - |  553 | `	sxu32 nIdx;` |
|        - |  554 | `	sxi32 rc;` |
|       56 |  555 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|       56 |  556 | `	if( rc != SXRET_OK ){` |
|        6 |  557 | `		return rc;` |
|        - |  558 | `	}` |
|       51 |  559 | `	pStr = &sStripped;` |
|       51 |  560 | `	nIdx = 0; /* Prevent compiler warning */` |
|       51 |  561 | `	if( pStr->nByte <= 0 ){` |
|        - |  562 | `		/* An empty nowdoc is the empty STRING, like '' -- loading NULL here made` |
|        - |  563 | `		 * strlen(<<<'EOD'EOD;) deprecation-warn about a null argument. */` |
|        7 |  564 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|        7 |  565 | `		return SXRET_OK;` |
|        - |  566 | `	}` |
|        - |  567 | `	/* Reserve a new constant */` |
|       45 |  568 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       45 |  569 | `	if( pObj == 0 ){` |
|      ! 0 |  570 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|      ! 0 |  571 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|      ! 0 |  572 | `		return SXERR_ABORT;` |
|        - |  573 | `	}` |
|        - |  574 | `	/* No processing is done here, simply a memcpy() operation */` |
|       45 |  575 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,pStr);` |
|        - |  576 | `	/* Emit the load constant instruction */` |
|       45 |  577 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        - |  578 | `	/* Node successfully compiled */` |
|       45 |  579 | `	return SXRET_OK;` |
|       30 |  580 | `}` |
|        - |  581 | `/*` |
|        - |  582 | ` * Process variable expression [i.e: "$var","${var}"] embedded in a double quoted/heredoc string.` |
|        - |  583 | ` * According to the PHP language reference manual` |
|        - |  584 | ` *   When a string is specified in double quotes or with heredoc,variables are parsed within it.` |
|        - |  585 | ` *  There are two types of syntax: a simple one and a complex one. The simple syntax is the most` |
|        - |  586 | ` *  common and convenient. It provides a way to embed a variable, an array value, or an object` |
|        - |  587 | ` *  property in a string with a minimum of effort.` |
|        - |  588 | ` *  Simple syntax` |
|        - |  589 | ` *   If a dollar sign ($) is encountered, the parser will greedily take as many tokens as possible` |
|        - |  590 | ` *   to form a valid variable name. Enclose the variable name in curly braces to explicitly specify` |
|        - |  591 | ` *   the end of the name.` |
|        - |  592 | ` *   Similarly, an array index or an object property can be parsed. With array indices, the closing` |
|        - |  593 | ` *   square bracket (]) marks the end of the index. The same rules apply to object properties` |
|        - |  594 | ` *   as to simple variables.` |
|        - |  595 | ` *  Complex (curly) syntax` |
|        - |  596 | ` *   This isn't called complex because the syntax is complex, but because it allows for the use` |
|        - |  597 | ` *   of complex expressions.` |
|        - |  598 | ` *   Any scalar variable, array element or object property with a string representation can be` |
|        - |  599 | ` *   included via this syntax. Simply write the expression the same way as it would appear outside` |
|        - |  600 | ` *   the string, and then wrap it in { and }. Since { can not be escaped, this syntax will only` |
|        - |  601 | ` *   be recognised when the $ immediately follows the {. Use {\$ to get a literal {$` |
|        - |  602 | ` */` |
|     2306 |  603 | `static sxi32 GenStateProcessStringExpression(` |
|        - |  604 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - |  605 | `	sxu32 nLine,         /* Line number */` |
|        - |  606 | `	const char *zIn,     /* Raw expression */` |
|        - |  607 | `	const char *zEnd     /* End of the expression */` |
|        - |  608 | `	)` |
|        5 |  609 | `{` |
|        - |  610 | `	SyToken *pTmpIn,*pTmpEnd;` |
|        - |  611 | `	SySet sToken;` |
|        - |  612 | `	sxi32 rc;` |
|        - |  613 | `	/* Initialize the token set */` |
|     2311 |  614 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|        - |  615 | `	/* Preallocate some slots */` |
|     2311 |  616 | `	SySetAlloc(&sToken,0x08);` |
|        - |  617 | `	/* Tokenize the text */` |
|     2311 |  618 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken,0);` |
|        - |  619 | `	/* Swap delimiter */` |
|     2311 |  620 | `	pTmpIn  = pGen->pIn;` |
|     2311 |  621 | `	pTmpEnd = pGen->pEnd;` |
|     2311 |  622 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|     2311 |  623 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|        - |  624 | ``	/* Compile the expression. An interpolated `"...$x..."` READS $x — php warns`` |
|        - |  625 | `	 * "Undefined variable $x" and substitutes the empty string — so ask for a` |
|        - |  626 | `	 * read-only load rather than letting the default vivify it silently. */` |
|     2311 |  627 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|        - |  628 | `	/* Restore token stream */` |
|     2311 |  629 | `	pGen->pIn  = pTmpIn;` |
|     2311 |  630 | `	pGen->pEnd = pTmpEnd;` |
|        - |  631 | `	/* Release the token set */` |
|     2311 |  632 | `	SySetRelease(&sToken);` |
|        - |  633 | `	/* Compilation result */` |
|     2311 |  634 | `	return rc;` |
|        5 |  635 | `}` |
|        - |  636 | `/*` |
|        - |  637 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|        - |  638 | ` */` |
|   126132 |  639 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|        5 |  640 | `{` |
|        - |  641 | `	ph7_value *pConstObj;` |
|   126137 |  642 | `	sxu32 nIdx = 0;` |
|        - |  643 | `	/* Reserve a new constant */` |
|   126137 |  644 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   126137 |  645 | `	if( pConstObj == 0 ){` |
|      ! 0 |  646 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|      ! 0 |  647 | `		return 0;` |
|        - |  648 | `	}` |
|   126137 |  649 | `	(*pCount)++;` |
|   126137 |  650 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|        - |  651 | `	/* Emit the load constant instruction */` |
|   126137 |  652 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   126137 |  653 | `	return pConstObj;` |
|    63071 |  654 | `}` |
|        - |  655 | `/*` |
|        - |  656 | ` * Compile a double quoted/heredoc string.` |
|        - |  657 | ` * According to the PHP language reference manual` |
|        - |  658 | ` * Heredoc` |
|        - |  659 | ` *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier` |
|        - |  660 | ` *  is provided, then a newline. The string itself follows, and then the same identifier again` |
|        - |  661 | ` *  to close the quotation.` |
|        - |  662 | ` *  The closing identifier must begin in the first column of the line. Also, the identifier must` |
|        - |  663 | ` *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric` |
|        - |  664 | ` *  characters and underscores, and must start with a non-digit character or underscore.` |
|        - |  665 | ` *  Warning` |
|        - |  666 | ` *  It is very important to note that the line with the closing identifier must contain` |
|        - |  667 | ` *  no other characters, except possibly a semicolon (;). That means especially that the identifier` |
|        - |  668 | ` *  may not be indented, and there may not be any spaces or tabs before or after the semicolon.` |
|        - |  669 | ` *  It's also important to realize that the first character before the closing identifier must` |
|        - |  670 | ` *  be a newline as defined by the local operating system. This is \n on UNIX systems, including Mac OS X.` |
|        - |  671 | ` *  The closing delimiter (possibly followed by a semicolon) must also be followed by a newline.` |
|        - |  672 | ` *  If this rule is broken and the closing identifier is not "clean", it will not be considered a closing` |
|        - |  673 | ` *  identifier, and PHP will continue looking for one. If a proper closing identifier is not found before` |
|        - |  674 | ` *  the end of the current file, a parse error will result at the last line.` |
|        - |  675 | ` *  Heredocs can not be used for initializing class properties.` |
|        - |  676 | ` * Double quoted` |
|        - |  677 | ` *  If the string is enclosed in double-quotes ("), PHP will interpret more escape sequences for special characters:` |
|        - |  678 | ` *  Escaped characters Sequence 	Meaning` |
|        - |  679 | ` *  \n linefeed (LF or 0x0A (10) in ASCII)` |
|        - |  680 | ` *  \r carriage return (CR or 0x0D (13) in ASCII)` |
|        - |  681 | ` *  \t horizontal tab (HT or 0x09 (9) in ASCII)` |
|        - |  682 | ` *  \v vertical tab (VT or 0x0B (11) in ASCII)` |
|        - |  683 | ` *  \e escape (ESC or 0x1B (27) in ASCII)` |
|        - |  684 | ` *  \f form feed (FF or 0x0C (12) in ASCII)` |
|        - |  685 | ` *  \\ backslash` |
|        - |  686 | ` *  \$ dollar sign` |
|        - |  687 | ` *  \" double-quote` |
|        - |  688 | ` *  \[0-7]{1,3} 	the sequence of characters matching the regular expression is a character in octal notation,` |
|        - |  689 | ` *      which silently overflows to fit in a byte (e.g. "\400" === "\000")` |
|        - |  690 | ` *  \x[0-9A-Fa-f]{1,2} 	the sequence of characters matching the regular expression is a character in hexadecimal notation` |
|        - |  691 | ` *  \u{[0-9A-Fa-f]+} 	the sequence of characters matching the regular expression is a Unicode codepoint,` |
|        - |  692 | ` *      which will be output to the string as that codepoint's UTF-8 representation` |
|        - |  693 | ` * As in single quoted strings, escaping any other character will result in the backslash being printed too.` |
|        - |  694 | ` * (The PH7-ism "\oNNN" octal form is gone: a literal "\o" now round-trips like php 8.)` |
|        - |  695 | ` * The most important feature of double-quoted strings is the fact that variable names will be expanded.` |
|        - |  696 | ` * See string parsing for details.` |
|        - |  697 | ` */` |
|        - |  698 | `/*` |
|        - |  699 | ` * Line number of an escape sequence inside the string body being compiled:` |
|        - |  700 | ` * the token's line plus every newline before the escape (php reports the` |
|        - |  701 | ` * escape's own line, not the string's opening line). A heredoc body starts` |
|        - |  702 | ` * on the line after the '<<<' marker, hence the +1.` |
|        - |  703 | ` */` |
|        6 |  704 | `static sxu32 GenStateStringEscLine(ph7_gen_state *pGen,const char *zPos,int bHeredoc)` |
|        3 |  705 | `{` |
|        9 |  706 | `	const char *z = pGen->pIn->sData.zString;` |
|        9 |  707 | `	sxu32 nLine = pGen->pIn->nLine + (bHeredoc ? 1 : 0);` |
|       15 |  708 | `	for( ; z < zPos ; z++ ){` |
|        9 |  709 | `		if( z[0] == '\n' ){` |
|      ! 0 |  710 | `			nLine++;` |
|      ! 0 |  711 | `		}` |
|        6 |  712 | `	}` |
|        9 |  713 | `	return nLine;` |
|        3 |  714 | `}` |
|        - |  715 | `/* bHeredoc: php strips the backslash from '\"' only when '"' is the active` |
|        - |  716 | ` * quote character; a heredoc has none, so '\"' stays verbatim there. */` |
|   124938 |  717 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen,int bHeredoc)` |
|        5 |  718 | `{` |
|   124943 |  719 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|        - |  720 | `	const char *zIn,*zCur,*zEnd;` |
|   124943 |  721 | `	ph7_value *pObj = 0;` |
|        - |  722 | `	sxi32 iCons;` |
|        - |  723 | `	sxi32 rc;` |
|        - |  724 | `	/* Delimit the string */` |
|   124943 |  725 | `	zIn  = pStr->zString;` |
|   124943 |  726 | `	zEnd = &zIn[pStr->nByte];` |
|   124943 |  727 | `	if( zIn >= zEnd ){` |
|        - |  728 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|        - |  729 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|        - |  730 | `		 * literal table from growing when many "" literals appear in the source.` |
|        - |  731 | `		 */` |
|      445 |  732 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|      445 |  733 | `		return SXRET_OK;` |
|        - |  734 | `	}` |
|   124503 |  735 | `	zCur = 0;` |
|        - |  736 | `	/* Compile the node */` |
|   124503 |  737 | `	iCons = 0;` |
|    63401 |  738 | `	for(;;){` |
|   171077 |  739 | `		zCur = zIn;` |
|  1675435 |  740 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  1506671 |  741 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|       84 |  742 | `				break;` |
|  1506514 |  743 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|     2156 |  744 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|     1078 |  745 | `					break;` |
|        - |  746 | `			}` |
|  1504363 |  747 | `			zIn++;` |
|        5 |  748 | `		}` |
|   171077 |  749 | `		if( zIn > zCur ){` |
|    95565 |  750 | `			if( pObj == 0 ){` |
|    94911 |  751 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    94911 |  752 | `				if( pObj == 0 ){` |
|      ! 0 |  753 | `					return SXERR_ABORT;` |
|        - |  754 | `				}` |
|    47453 |  755 | `			}` |
|    95565 |  756 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    47780 |  757 | `		}` |
|   171077 |  758 | `		if( zIn >= zEnd ){` |
|   124499 |  759 | `			break;` |
|        - |  760 | `		}` |
|    46583 |  761 | `		if( zIn[0] == '\\' ){` |
|    44275 |  762 | `			const char *zPtr = 0;` |
|        - |  763 | `			sxu32 n;` |
|    44275 |  764 | `			zIn++;` |
|    44275 |  765 | `			if( pObj == 0 ){` |
|    31231 |  766 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    31231 |  767 | `				if( pObj == 0 ){` |
|      ! 0 |  768 | `					return SXERR_ABORT;` |
|        - |  769 | `				}` |
|    15613 |  770 | `			}` |
|    44275 |  771 | `			if( zIn >= zEnd ){` |
|        - |  772 | `				/* Lone backslash at the very end of the body: php keeps it */` |
|        3 |  773 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|        3 |  774 | `				break;` |
|        - |  775 | `			}` |
|    44273 |  776 | `			n = sizeof(char); /* size of conversion */` |
|    44273 |  777 | `			switch( zIn[0] ){` |
|       17 |  778 | `			case '$':` |
|        - |  779 | `				/* Dollar sign */` |
|       37 |  780 | `				PH7_MemObjStringAppend(pObj,"$",sizeof(char));` |
|       37 |  781 | `				break;` |
|       62 |  782 | `			case '\\':` |
|        - |  783 | `				/* A literal backslash */` |
|      129 |  784 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|      129 |  785 | `				break;` |
|        1 |  786 | `			case 'e':` |
|        - |  787 | `				/* Escape (ESC) ASCII code 27 */` |
|        3 |  788 | `				PH7_MemObjStringAppend(pObj,"\x1b",sizeof(char));` |
|        3 |  789 | `				break;` |
|        4 |  790 | `			case 'f':` |
|        - |  791 | `				/* Form-feed (FF)[ctrl+l] ASCII code 12 */` |
|        9 |  792 | `				PH7_MemObjStringAppend(pObj,"\f",sizeof(char));` |
|        9 |  793 | `				break;` |
|    19625 |  794 | `			case 'n':` |
|        - |  795 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|    39255 |  796 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|    39255 |  797 | `				break;` |
|       27 |  798 | `			case 'r':` |
|        - |  799 | `				/* Carriage return (CR)[ctrl+m] ASCII code 13 */` |
|       59 |  800 | `				PH7_MemObjStringAppend(pObj,"\r",sizeof(char));` |
|       59 |  801 | `				break;` |
|     1970 |  802 | `			case 't':` |
|        - |  803 | `				/* Horizontal tab (HT)[ctrl+i] ASCII code 9 */` |
|     3945 |  804 | `				PH7_MemObjStringAppend(pObj,"\t",sizeof(char));` |
|     3945 |  805 | `				break;` |
|        3 |  806 | `			case 'v':` |
|        - |  807 | `				/* Vertical tab(VT)[ctrl+k] ASCII code 11 */` |
|        7 |  808 | `				PH7_MemObjStringAppend(pObj,"\v",sizeof(char));` |
|        7 |  809 | `				break;` |
|      147 |  810 | `			case '"':` |
|      299 |  811 | `				if( bHeredoc ){` |
|        - |  812 | `					/* No active quote char in a heredoc: php keeps \" verbatim */` |
|        5 |  813 | `					PH7_MemObjStringAppend(pObj,"\\\"",sizeof(char)*2);` |
|        3 |  814 | `				}else{` |
|        - |  815 | `					/* Double quote */` |
|      295 |  816 | `					PH7_MemObjStringAppend(pObj,"\"",sizeof(char));` |
|        - |  817 | `				}` |
|      299 |  818 | `				break;` |
|       26 |  819 | `			case '0': case '1': case '2': case '3':` |
|        - |  820 | `			case '4': case '5': case '6': case '7': {` |
|        - |  821 | `				/* \[0-7]{1,3}: a character in octal notation. A value above \377` |
|        - |  822 | `				 * warns and wraps to the low byte, matching php 8. */` |
|       55 |  823 | `				int c = 0;` |
|        - |  824 | `				char cOut;` |
|      153 |  825 | `				for( zPtr = zIn ; zPtr < &zIn[3*sizeof(char)] ; zPtr++ ){` |
|      131 |  826 | `					if( zPtr >= zEnd \|\| zPtr[0] < '0' \|\| zPtr[0] > '7' ){` |
|       17 |  827 | `						break;` |
|        - |  828 | `					}` |
|      101 |  829 | `					c = c * 8 + (zPtr[0] - '0');` |
|       52 |  830 | `				}` |
|       55 |  831 | `				if( c > 0xFF ){` |
|        - |  832 | `					SyString sSeq;` |
|        3 |  833 | `					SyStringInitFromBuf(&sSeq,zIn,(sxu32)(zPtr-zIn));` |
|        3 |  834 | `					PH7_GenCompileError(&(*pGen),E_WARNING,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|        - |  835 | `						"Octal escape sequence overflow \\%z is greater than \\377",&sSeq);` |
|        3 |  836 | `					c &= 0xFF;` |
|        1 |  837 | `				}` |
|       55 |  838 | `				cOut = (char)c; /* value byte, independent of host endianness */` |
|       55 |  839 | `				PH7_MemObjStringAppend(pObj,&cOut,sizeof(char));` |
|       55 |  840 | `				n = (sxu32)(zPtr-zIn);` |
|       55 |  841 | `				break;` |
|        - |  842 | `			}` |
|      228 |  843 | `			case 'x':` |
|      684 |  844 | `				if( &zIn[1] < zEnd && SyisHex((unsigned char)zIn[1]) ){` |
|        - |  845 | `					/* \x[0-9A-Fa-f]{1,2}: a character in hexadecimal notation */` |
|      454 |  846 | `					int c = SyHexToint(zIn[1]);` |
|        - |  847 | `					char cOut;` |
|      454 |  848 | `					n += sizeof(char);` |
|      454 |  849 | `					if( &zIn[2] < zEnd && SyisHex((unsigned char)zIn[2]) ){` |
|      450 |  850 | `						c = (c << 4) + SyHexToint(zIn[2]);` |
|      450 |  851 | `						n += sizeof(char);` |
|      224 |  852 | `					}` |
|      454 |  853 | `					cOut = (char)c; /* value byte, independent of host endianness */` |
|      454 |  854 | `					PH7_MemObjStringAppend(pObj,&cOut,sizeof(char));` |
|      228 |  855 | `				}else{` |
|        - |  856 | `					/* Not an escape: keep the backslash, as php does */` |
|        5 |  857 | `					PH7_MemObjStringAppend(pObj,"\\x",sizeof(char)*2);` |
|        - |  858 | `				}` |
|      458 |  859 | `				break;` |
|        9 |  860 | `			case 'u':` |
|       18 |  861 | `				if( &zIn[1] < zEnd && zIn[1] == '{'` |
|       22 |  862 | `				 && !(&zIn[2] < zEnd && zIn[2] == '$') ){` |
|        - |  863 | `					/* \u{codepoint}: UTF-8 encoding of the given codepoint (php 7+).` |
|        - |  864 | `					 * php encodes surrogates verbatim, so the only invalid value` |
|        - |  865 | `					 * is > U+10FFFF; malformed/empty braces are a compile error.` |
|        - |  866 | `					 * "\u{$..." is excluded above: php treats it as a literal \u` |
|        - |  867 | `					 * followed by {$...} curly interpolation. */` |
|       15 |  868 | `					sxu32 nCp = 0;` |
|       15 |  869 | `					zPtr = &zIn[2];` |
|       59 |  870 | `					while( zPtr < zEnd && SyisHex((unsigned char)zPtr[0]) ){` |
|       46 |  871 | `						if( nCp <= 0x10FFFF ){` |
|        - |  872 | `							/* stop accumulating once out of range: keeps a long` |
|        - |  873 | `							 * digit run from wrapping sxu32 */` |
|       46 |  874 | `							nCp = nCp * 16 + (sxu32)SyHexToint(zPtr[0]);` |
|       22 |  875 | `						}` |
|       46 |  876 | `						zPtr++;` |
|        2 |  877 | `					}` |
|       15 |  878 | `					if( zPtr == &zIn[2] \|\| zPtr >= zEnd \|\| zPtr[0] != '}' ){` |
|        - |  879 | `						/* Error recorded (nErr>0 fails the whole compile); consume the` |
|        - |  880 | `						 * malformed sequence so later errors are still reported. */` |
|        3 |  881 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|        - |  882 | `							"Invalid UTF-8 codepoint escape sequence");` |
|        3 |  883 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 |  884 | `							return SXERR_ABORT;` |
|        - |  885 | `						}` |
|        3 |  886 | `						n = (sxu32)(zPtr-zIn);` |
|        3 |  887 | `						if( zPtr < zEnd && zPtr[0] == '}' ){` |
|        3 |  888 | `							n += sizeof(char);` |
|        1 |  889 | `						}` |
|        3 |  890 | `						break;` |
|        - |  891 | `					}` |
|       12 |  892 | `					n = (sxu32)(&zPtr[1]-zIn); /* 'u{...}' incl. closing brace */` |
|       12 |  893 | `					if( nCp > 0x10FFFF ){` |
|        3 |  894 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|        - |  895 | `							"Invalid UTF-8 codepoint escape sequence: Codepoint too large");` |
|        3 |  896 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 |  897 | `							return SXERR_ABORT;` |
|        - |  898 | `						}` |
|        3 |  899 | `						break;` |
|        - |  900 | `					}` |
|        - |  901 | `					{` |
|        - |  902 | `						char zUtf[4];` |
|        9 |  903 | `						sxu8 *zOut = (sxu8 *)zUtf;` |
|        9 |  904 | `						SX_WRITE_UTF8(zOut,nCp);` |
|        9 |  905 | `						PH7_MemObjStringAppend(pObj,zUtf,(sxu32)(zOut-(sxu8 *)zUtf));` |
|        - |  906 | `					}` |
|        5 |  907 | `				}else{` |
|        - |  908 | `					/* Not an escape: keep the backslash, as php does */` |
|        7 |  909 | `					PH7_MemObjStringAppend(pObj,"\\u",sizeof(char)*2);` |
|        - |  910 | `				}` |
|       15 |  911 | `				break;` |
|       15 |  912 | `			default:` |
|        - |  913 | `				/* Unrecognized escape: keep the backslash, as php does.` |
|        - |  914 | `				 * zIn[-1] is the backslash itself, so both bytes are contiguous` |
|        - |  915 | `				 * in the source buffer — one batched append. */` |
|       31 |  916 | `				PH7_MemObjStringAppend(pObj,&zIn[-1],sizeof(char)*2);` |
|       30 |  917 | `				break;` |
|        - |  918 | `			}` |
|        - |  919 | `			/* Advance the stream cursor */` |
|    44273 |  920 | `			zIn += n;` |
|    44273 |  921 | `			continue;` |
|        - |  922 | `		}` |
|     2313 |  923 | `		if( zIn[0] == '{' ){` |
|        - |  924 | `			/* Curly syntax */` |
|        - |  925 | `			const char *zExpr;` |
|      165 |  926 | `			sxi32 iNest = 1;` |
|      165 |  927 | `			zIn++;` |
|      165 |  928 | `			zExpr = zIn;` |
|        - |  929 | `			/* Synchronize with the next closing curly braces */` |
|     1523 |  930 | `			while( zIn < zEnd ){` |
|     1523 |  931 | `				if( zIn[0] == '{' ){` |
|        - |  932 | `					/* Increment nesting level */` |
|        3 |  933 | `					iNest++;` |
|     1522 |  934 | `				}else if(zIn[0] == '}' ){` |
|        - |  935 | `					/* Decrement nesting level */` |
|      167 |  936 | `					iNest--;` |
|      167 |  937 | `					if( iNest <= 0 ){` |
|      165 |  938 | `						break;` |
|        - |  939 | `					}` |
|        1 |  940 | `				}` |
|     1361 |  941 | `				zIn++;` |
|        3 |  942 | `			}` |
|        - |  943 | `			/* Process the expression */` |
|      165 |  944 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|      165 |  945 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  946 | `				return SXERR_ABORT;` |
|        - |  947 | `			}` |
|      165 |  948 | `			if( rc != SXERR_EMPTY ){` |
|      165 |  949 | `				++iCons;` |
|       81 |  950 | `			}` |
|      165 |  951 | `			if( zIn < zEnd ){` |
|        - |  952 | `				/* Jump the trailing curly */` |
|      165 |  953 | `				zIn++;` |
|       81 |  954 | `			}` |
|       84 |  955 | `		}else{` |
|        - |  956 | `			/* Simple syntax */` |
|     2151 |  957 | `			const char *zExpr = zIn;` |
|        - |  958 | `			/* Assemble variable name */` |
|     1098 |  959 | `			for(;;){` |
|        - |  960 | `				/* Jump leading dollars */` |
|     4347 |  961 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|     2151 |  962 | `					zIn++;` |
|        5 |  963 | `				}` |
|     1098 |  964 | `				for(;;){` |
|    10363 |  965 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|     7069 |  966 | `						zIn++;` |
|        5 |  967 | `					}` |
|     2201 |  968 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|        - |  969 | `						/* UTF-8 stream */` |
|      ! 0 |  970 | `						zIn++;` |
|      ! 0 |  971 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  972 | `							zIn++;` |
|      ! 0 |  973 | `						}` |
|      ! 0 |  974 | `						continue;` |
|        - |  975 | `					}` |
|     2201 |  976 | `					break;` |
|      ! 0 |  977 | `				}` |
|     2201 |  978 | `				if( zIn >= zEnd ){` |
|      279 |  979 | `					break;` |
|        - |  980 | `				}` |
|     1927 |  981 | `				if( zIn[0] == '[' ){` |
|       12 |  982 | `					sxi32 iSquare = 1;` |
|       12 |  983 | `					zIn++;` |
|       28 |  984 | `					while( zIn < zEnd ){` |
|       28 |  985 | `						if( zIn[0] == '[' ){` |
|      ! 0 |  986 | `							iSquare++;` |
|       28 |  987 | `						}else if (zIn[0] == ']' ){` |
|       12 |  988 | `							iSquare--;` |
|       12 |  989 | `							if( iSquare <= 0 ){` |
|       12 |  990 | `								break;` |
|        - |  991 | `							}` |
|      ! 0 |  992 | `						}` |
|       18 |  993 | `						zIn++;` |
|        2 |  994 | `					}` |
|       12 |  995 | `					if( zIn < zEnd ){` |
|       12 |  996 | `						zIn++;` |
|        5 |  997 | `					}` |
|       12 |  998 | `					break;` |
|     1917 |  999 | `				}else if(zIn[0] == '{' ){` |
|        3 | 1000 | `					sxi32 iCurly = 1;` |
|        3 | 1001 | `					zIn++;` |
|       11 | 1002 | `					while( zIn < zEnd ){` |
|       11 | 1003 | `						if( zIn[0] == '{' ){` |
|      ! 0 | 1004 | `							iCurly++;` |
|       11 | 1005 | `						}else if (zIn[0] == '}' ){` |
|        3 | 1006 | `							iCurly--;` |
|        3 | 1007 | `							if( iCurly <= 0 ){` |
|        3 | 1008 | `								break;` |
|        - | 1009 | `							}` |
|      ! 0 | 1010 | `						}` |
|        9 | 1011 | `						zIn++;` |
|        1 | 1012 | `					}` |
|        3 | 1013 | `					if( zIn < zEnd ){` |
|        3 | 1014 | `						zIn++;` |
|        1 | 1015 | `					}` |
|        3 | 1016 | `					break;` |
|     1915 | 1017 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|        - | 1018 | `					/* Member access operator '->' */` |
|       53 | 1019 | `					zIn += 2;` |
|     1890 | 1020 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|        - | 1021 | `					/* Static member access operator '::' */` |
|      ! 0 | 1022 | `					zIn += 2;` |
|      ! 0 | 1023 | `				}else{` |
|      935 | 1024 | `					break;` |
|        - | 1025 | `				}` |
|        3 | 1026 | `			}` |
|        - | 1027 | `			/*` |
|        - | 1028 | `			 * "$a[name]" — php's SIMPLE syntax takes an unquoted subscript as the string key` |
|        - | 1029 | `			 * 'name', never as a constant. PH7 handed "$a[name]" straight to the expression` |
|        - | 1030 | `			 * compiler, where the bare word only resolved because an unknown constant used to` |
|        - | 1031 | `			 * fall back to its own name as a string. With undefined constants now a real` |
|        - | 1032 | `			 * Error, quote the key here so the simple syntax keeps meaning what php means.` |
|        - | 1033 | `			 * A numeric ($a[0]) or variable ($a[$k]) subscript is already unambiguous.` |
|        - | 1034 | `			 */` |
|        - | 1035 | `			{` |
|     2151 | 1036 | `				const char *zBr = zExpr;` |
|    11473 | 1037 | `				while( zBr < zIn && zBr[0] != '[' ){` |
|     9327 | 1038 | `					zBr++;` |
|        5 | 1039 | `				}` |
|     2151 | 1040 | `				if( zBr < zIn && zIn[-1] == ']' ){` |
|       12 | 1041 | `					const char *zKey = &zBr[1];` |
|       12 | 1042 | `					const char *zKeyEnd = &zIn[-1];` |
|       12 | 1043 | `					const char *zScan = zKey;` |
|       12 | 1044 | `					int bBare = (zKey < zKeyEnd) && !SyisDigit(zKey[0]);` |
|       20 | 1045 | `					while( bBare && zScan < zKeyEnd ){` |
|        9 | 1046 | `						if( !SyisAlphaNum(zScan[0]) && zScan[0] != '_' ){` |
|      ! 0 | 1047 | `							bBare = 0;` |
|      ! 0 | 1048 | `						}` |
|        9 | 1049 | `						zScan++;` |
|        1 | 1050 | `					}` |
|       12 | 1051 | `					if( bBare ){` |
|        - | 1052 | `						SyBlob sSub;` |
|        3 | 1053 | `						SyBlobInit(&sSub,&pGen->pVm->sAllocator);` |
|        3 | 1054 | `						SyBlobAppend(&sSub,zExpr,(sxu32)(zBr - zExpr));` |
|        3 | 1055 | `						SyBlobAppend(&sSub,"['",2);` |
|        3 | 1056 | `						SyBlobAppend(&sSub,zKey,(sxu32)(zKeyEnd - zKey));` |
|        3 | 1057 | `						SyBlobAppend(&sSub,"']",2);` |
|        4 | 1058 | `						rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,` |
|        2 | 1059 | `							(const char *)SyBlobData(&sSub),` |
|        2 | 1060 | `							(const char *)SyBlobData(&sSub) + SyBlobLength(&sSub));` |
|        3 | 1061 | `						SyBlobRelease(&sSub);` |
|        3 | 1062 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 1063 | `							return SXERR_ABORT;` |
|        - | 1064 | `						}` |
|        3 | 1065 | `						if( rc != SXERR_EMPTY ){` |
|        3 | 1066 | `							++iCons;` |
|        1 | 1067 | `						}` |
|        3 | 1068 | `						pObj = 0;` |
|        3 | 1069 | `						continue;` |
|        - | 1070 | `					}` |
|        4 | 1071 | `				}` |
|        - | 1072 | `			}` |
|        - | 1073 | `			/*` |
|        - | 1074 | `			 * "${...}" string interpolation (every form: ${name}, ${expr}, ${$x}) was` |
|        - | 1075 | `			 * DEPRECATED by php 8.2 in favor of the canonical "{$...}". PHL targets php's` |
|        - | 1076 | `			 * *non-deprecated* surface, so it is a hard parse error here — never silently` |
|        - | 1077 | `			 * rewritten. The canonical "{$var}" reaches this compiler by a different path` |
|        - | 1078 | `			 * and is unaffected.` |
|        - | 1079 | `			 */` |
|     2149 | 1080 | `			if( &zExpr[1] < zIn && zExpr[0] == '$' && zExpr[1] == '{' ){` |
|        3 | 1081 | `				PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|        - | 1082 | `					"syntax error, \"${\" string interpolation was removed in php 8.2, use \"{$...}\" instead");` |
|        3 | 1083 | `				return SXERR_ABORT;` |
|        - | 1084 | `			}` |
|        - | 1085 | `			/* Process the expression */` |
|     2147 | 1086 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|     2147 | 1087 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1088 | `				return SXERR_ABORT;` |
|        - | 1089 | `			}` |
|     2147 | 1090 | `			if( rc != SXERR_EMPTY ){` |
|     2147 | 1091 | `				++iCons;` |
|     1071 | 1092 | `			}` |
|        - | 1093 | `		}` |
|        - | 1094 | `		/* Invalidate the previously used constant */` |
|     2309 | 1095 | `		pObj = 0;` |
|        5 | 1096 | `	}/*for(;;)*/` |
|   124501 | 1097 | `	if( iCons > 1 ){` |
|        - | 1098 | `		/* Concatenate all compiled constants */` |
|     1585 | 1099 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|      790 | 1100 | `	}` |
|        - | 1101 | `	/* Node successfully compiled */` |
|   124501 | 1102 | `	return SXRET_OK;` |
|    62474 | 1103 | `}` |
|        - | 1104 | `/*` |
|        - | 1105 | ` * Compile a double quoted string.` |
|        - | 1106 | ` *  See the block-comment above for more information.` |
|        - | 1107 | ` */` |
|   124874 | 1108 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 | 1109 | `{` |
|        - | 1110 | `	sxi32 rc;` |
|   124879 | 1111 | `	rc = GenStateCompileString(&(*pGen),0/*bHeredoc*/);` |
|    62437 | 1112 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        - | 1113 | `	/* Compilation result */` |
|   124879 | 1114 | `	return rc;` |
|        5 | 1115 | `}` |
|        - | 1116 | `/*` |
|        - | 1117 | ` * Compile a Heredoc string.` |
|        - | 1118 | ` *  See the block-comment above for more information.` |
|        - | 1119 | ` */` |
|       68 | 1120 | `PH7_PRIVATE sxi32 PH7_CompileHereDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 | 1121 | `{` |
|        - | 1122 | `	SyString sOrig, sStripped;` |
|        - | 1123 | `	sxi32 rc;` |
|       73 | 1124 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|       73 | 1125 | `	if( rc != SXRET_OK ){` |
|        6 | 1126 | `		return rc;` |
|        - | 1127 | `	}` |
|        - | 1128 | `	/* Temporarily swap in the dedented body so GenStateCompileString` |
|        - | 1129 | `	 * (which reads pGen->pIn->sData directly) sees the stripped content.` |
|        - | 1130 | `	 * Restore before returning so downstream code that references pIn is` |
|        - | 1131 | `	 * unaffected, including on the error path. */` |
|       68 | 1132 | `	sOrig = pGen->pIn->sData;` |
|       68 | 1133 | `	pGen->pIn->sData = sStripped;` |
|       68 | 1134 | `	rc = GenStateCompileString(&(*pGen),1/*bHeredoc*/);` |
|       68 | 1135 | `	pGen->pIn->sData = sOrig;` |
|       32 | 1136 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       68 | 1137 | `	return rc;` |
|       39 | 1138 | `}` |
|        - | 1139 | `/*` |
|        - | 1140 | ` * Compile an array entry whether it is a key or a value.` |
|        - | 1141 | ` *  Notes on array entries.` |
|        - | 1142 | ` *  According to the PHP language reference manual` |
|        - | 1143 | ` *  An array can be created by the array() language construct.` |
|        - | 1144 | ` *  It takes as parameters any number of comma-separated key => value pairs.` |
|        - | 1145 | ` *  array(  key =>  value` |
|        - | 1146 | ` *    , ...` |
|        - | 1147 | ` *    )` |
|        - | 1148 | ` *  A key may be either an integer or a string. If a key is the standard representation` |
|        - | 1149 | ` *  of an integer, it will be interpreted as such (i.e. "8" will be interpreted as 8, while` |
|        - | 1150 | ` *  "08" will be interpreted as "08"). Floats in key are truncated to integer.` |
|        - | 1151 | ` *  The indexed and associative array types are the same type in PHP, which can both` |
|        - | 1152 | ` *  contain integer and string indices.` |
|        - | 1153 | ` *  A value can be any PHP type.` |
|        - | 1154 | ` *  If a key is not specified for a value, the maximum of the integer indices is taken` |
|        - | 1155 | ` *  and the new key will be that value plus 1. If a key that already has an assigned value` |
|        - | 1156 | ` *  is specified, that value will be overwritten.` |
|        - | 1157 | ` */` |
|  1564086 | 1158 | `PH7_PRIVATE sxi32 GenStateCompileArrayEntry(` |
|        - | 1159 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - | 1160 | `	SyToken *pIn,        /* Token stream */` |
|        - | 1161 | `	SyToken *pEnd,       /* End of the token stream */` |
|        - | 1162 | `	sxi32 iFlags,        /* Compilation flags */` |
|        - | 1163 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *) /* Expression tree validator callback */` |
|        - | 1164 | `	)` |
|        5 | 1165 | `{` |
|        - | 1166 | `	SyToken *pTmpIn,*pTmpEnd;` |
|        - | 1167 | `	sxi32 rc;` |
|        - | 1168 | `	/* Swap token stream */` |
|  1564091 | 1169 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|        - | 1170 | `	/* Compile the expression*/` |
|  1564091 | 1171 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|        - | 1172 | `	/* Restore token stream */` |
|  1564091 | 1173 | `	RE_SWAP_DELIMITER(pGen);` |
|  1564091 | 1174 | `	return rc;` |
|        5 | 1175 | `}` |
|        - | 1176 | `/*` |
|        - | 1177 | ` * Expression tree validator callback for the 'array' language construct.` |
|        - | 1178 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|        - | 1179 | ` * an invalid expression tree and this function will generate the appropriate` |
|        - | 1180 | ` * error message.` |
|        - | 1181 | ` * See the routine responible of compiling the array language construct` |
|        - | 1182 | ` * for more inforation.` |
|        - | 1183 | ` */` |
|       36 | 1184 | `static sxi32 GenStateArrayNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        4 | 1185 | `{` |
|       40 | 1186 | `	sxi32 rc = SXRET_OK;` |
|       40 | 1187 | `	if( pRoot->pOp ){` |
|       14 | 1188 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ &&` |
|       12 | 1189 | `			pRoot->pOp->iOp != EXPR_OP_FUNC_CALL /* function() [Symisc extension: i.e: array(&foo())] */` |
|       16 | 1190 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|        - | 1191 | `			/* Unexpected expression */` |
|       13 | 1192 | `			rc = PH7_GenSyntaxError(&(*pGen),pRoot->pStart,"\"->\" or \"?->\" or \"[\"");` |
|       13 | 1193 | `			if( rc != SXERR_ABORT ){` |
|       13 | 1194 | `				rc = SXERR_INVALID;` |
|        5 | 1195 | `			}` |
|        9 | 1196 | `		}` |
|       31 | 1197 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|        - | 1198 | `		/* Unexpected expression */` |
|        3 | 1199 | `		rc = PH7_GenSyntaxError(&(*pGen),pRoot->pStart,0);` |
|        3 | 1200 | `		if( rc != SXERR_ABORT ){` |
|        3 | 1201 | `			rc = SXERR_INVALID;` |
|        1 | 1202 | `		}` |
|        1 | 1203 | `	}` |
|       40 | 1204 | `	return rc;` |
|        4 | 1205 | `}` |
|        - | 1206 | `/*` |
|        - | 1207 | ` * Find the top-level '=>' (PH7_TK_ARRAY_OP) that separates an array/list entry's` |
|        - | 1208 | ` * key from its value within [pStart,pEnd). The scan skips any '=>' nested inside` |
|        - | 1209 | ` * brackets/parens/braces, inside an arrow-function signature (fn(...) =>), or` |
|        - | 1210 | ` * inside a match() {...} arm — none of which are key/value separators. Returns a` |
|        - | 1211 | ` * pointer to the '=>' token, or pEnd if the entry has no top-level separator.` |
|        - | 1212 | ` */` |
|  1489154 | 1213 | `PH7_PRIVATE SyToken * GenStateFindTopLevelArrow(SyToken *pStart,SyToken *pEnd)` |
|        5 | 1214 | `{` |
|  1489159 | 1215 | `	SyToken *pCur = pStart;` |
|  1489159 | 1216 | `	sxi32 iNest = 0;` |
|  3804239 | 1217 | `	while( pCur < pEnd ){` |
|  2852489 | 1218 | `		if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|   537405 | 1219 | `			return pCur;` |
|        - | 1220 | `		}` |
|        - | 1221 | `		/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|        - | 1222 | `		 * The '=>' inside an arrow function introduces the expression body,` |
|        - | 1223 | `		 * not an entry separator. Skip past the signature.` |
|        - | 1224 | `		 */` |
|  2315089 | 1225 | `		if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
|    23367 | 1226 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|    23367 | 1227 | `			SyToken *pFn = pCur;` |
|    23362 | 1228 | `			if( nKw == PH7_TKWRD_STATIC && &pCur[1] < pEnd` |
|      ! 0 | 1229 | `				&& (pCur[1].nType & PH7_TK_KEYWORD)` |
|        5 | 1230 | `				&& SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ){` |
|      ! 0 | 1231 | `				pFn = &pCur[1];` |
|      ! 0 | 1232 | `				nKw = PH7_TKWRD_FN;` |
|      ! 0 | 1233 | `			}` |
|    23367 | 1234 | `			if( nKw == PH7_TKWRD_FN ){` |
|        5 | 1235 | `				pCur = pFn + 1; /* past 'fn' */` |
|        5 | 1236 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_AMPER) ){` |
|      ! 0 | 1237 | `					pCur++;` |
|      ! 0 | 1238 | `				}` |
|        5 | 1239 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|        5 | 1240 | `					pCur++;` |
|        5 | 1241 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|        - | 1242 | `						PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|        5 | 1243 | `					if( pCur < pEnd ){` |
|        5 | 1244 | `						pCur++;` |
|        2 | 1245 | `					}` |
|        2 | 1246 | `				}` |
|        5 | 1247 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_COLON) ){` |
|      ! 0 | 1248 | `					pCur++;` |
|      ! 0 | 1249 | `					if( pCur < pEnd && (pCur->nType & PH7_TK_OP)` |
|      ! 0 | 1250 | `						&& pCur->sData.nByte == 1` |
|      ! 0 | 1251 | `						&& pCur->sData.zString[0] == '?' ){` |
|      ! 0 | 1252 | `						pCur++;` |
|      ! 0 | 1253 | `					}` |
|      ! 0 | 1254 | `					if( pCur < pEnd` |
|      ! 0 | 1255 | `						&& (pCur->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|      ! 0 | 1256 | `						pCur++;` |
|      ! 0 | 1257 | `					}` |
|      ! 0 | 1258 | `				}` |
|        - | 1259 | `				/* The rest of the entry is the arrow-function body — no outer` |
|        - | 1260 | `				 * key to extract. */` |
|        5 | 1261 | `				return pEnd;` |
|        - | 1262 | `			}` |
|        - | 1263 | `			/* Match expression (PHP 8.0): the '=>' inside match arms is not an` |
|        - | 1264 | `			 * entry separator. Skip past the full match span. */` |
|    23363 | 1265 | `			if( nKw == PH7_TKWRD_MATCH ){` |
|        3 | 1266 | `				pCur++; /* past 'match' */` |
|        3 | 1267 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|        3 | 1268 | `					pCur++;` |
|        3 | 1269 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|        - | 1270 | `						PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|        3 | 1271 | `					if( pCur < pEnd ){` |
|        3 | 1272 | `						pCur++;` |
|        1 | 1273 | `					}` |
|        1 | 1274 | `				}` |
|        3 | 1275 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_OCB) ){` |
|        3 | 1276 | `					pCur++;` |
|        3 | 1277 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|        - | 1278 | `						PH7_TK_OCB,PH7_TK_CCB,&pCur);` |
|        3 | 1279 | `					if( pCur < pEnd ){` |
|        3 | 1280 | `						pCur++;` |
|        1 | 1281 | `					}` |
|        1 | 1282 | `				}` |
|        3 | 1283 | `				continue;` |
|        - | 1284 | `			}` |
|    11678 | 1285 | `		}` |
|  2315083 | 1286 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|    54895 | 1287 | `			iNest++;` |
|  2287638 | 1288 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/) ){` |
|        - | 1289 | `			/* Don't worry about mismatched brackets here, the expression` |
|        - | 1290 | `			 * parser will shortly detect any syntax error. */` |
|    54895 | 1291 | `			iNest--;` |
|    27445 | 1292 | `		}` |
|  2315083 | 1293 | `		pCur++;` |
|        5 | 1294 | `	}` |
|   951755 | 1295 | `	return pEnd;` |
|   744582 | 1296 | `}` |
|        - | 1297 | `/*` |
|        - | 1298 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|        - | 1299 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|        - | 1300 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|        - | 1301 | ` */` |
|   670990 | 1302 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|        5 | 1303 | `{` |
|        - | 1304 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|        - | 1305 | `	SyToken *pKey,*pCur;` |
|   670995 | 1306 | `	sxi32 iEmitRef = 0;` |
|   670995 | 1307 | `	sxi32 iSpread = 0;` |
|   670995 | 1308 | `	sxi32 nPair = 0;` |
|        - | 1309 | `	sxi32 rc;` |
|   670995 | 1310 | `	xValidator = 0;` |
|   916719 | 1311 | `	for(;;){` |
|        - | 1312 | `		/* Jump leading commas. Exactly ONE separates two entries; a second one (or a comma` |
|        - | 1313 | `		 * at the very start) means an EMPTY element, which php rejects outright — PH7 just` |
|        - | 1314 | ``		 * skipped them, so `array(,)` and `array(1,,2)` compiled silently. A TRAILING comma`` |
|        - | 1315 | `		 * is legal and is handled by the loop exiting on the next pass. */` |
|   581224 | 1316 | `		{` |
|  1833443 | 1317 | `			int nSkip = 0;` |
|  2721013 | 1318 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|   887575 | 1319 | `				nSkip++;` |
|   887575 | 1320 | `				pGen->pIn++;` |
|        5 | 1321 | `			}` |
|  1833443 | 1322 | `			if( nSkip > 1 \|\| (nSkip > 0 && nPair < 1) ){` |
|      ! 0 | 1323 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn[-1].nLine,` |
|        - | 1324 | `					"Cannot use empty array elements in arrays");` |
|      ! 0 | 1325 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1326 | `					return SXERR_ABORT;` |
|        - | 1327 | `				}` |
|      ! 0 | 1328 | `				return SXRET_OK;` |
|        - | 1329 | `			}` |
|        - | 1330 | `		}` |
|  1833443 | 1331 | `		pCur = pGen->pIn;` |
|  1833443 | 1332 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|        - | 1333 | `			/* No more entry to process */` |
|   670977 | 1334 | `			break;` |
|        - | 1335 | `		}` |
|  1162471 | 1336 | `		if( pCur >= pGen->pIn ){` |
|      ! 0 | 1337 | `			continue;` |
|        - | 1338 | `		}` |
|        - | 1339 | `		/* Compile the key if available */` |
|  1162471 | 1340 | `		pKey = pCur;` |
|  1162471 | 1341 | `		pCur = GenStateFindTopLevelArrow(pCur,pGen->pIn);` |
|  1162471 | 1342 | `		rc = SXERR_EMPTY;` |
|  1162471 | 1343 | `		if( pCur < pGen->pIn ){` |
|   401363 | 1344 | `			if( pKey == pCur ){` |
|        - | 1345 | ``				/* `array( => 2)`: the entry STARTS with '=>', so it has no key. php rejects`` |
|        - | 1346 | `				 * it; PH7 warned about a "Missing entry key" and compiled on, accepting` |
|        - | 1347 | ``				 * source php refuses. (The `else if` below could never see this: the arrow`` |
|        - | 1348 | `				 * IS found here, so control never reached it.)` |
|        - | 1349 | `				 * php names the literal's own closer, so short syntax expects ']'. */` |
|        3 | 1350 | `				const char *zClose = (pGen->pEnd && (pGen->pEnd->nType & PH7_TK_CSB))` |
|        - | 1351 | `					? "\"]\"" : "\")\"";` |
|        3 | 1352 | `				rc = PH7_GenSyntaxError(&(*pGen),pCur,zClose);` |
|        3 | 1353 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1354 | `					return SXERR_ABORT;` |
|        - | 1355 | `				}` |
|        3 | 1356 | `				return SXRET_OK;` |
|        - | 1357 | `			}` |
|   401361 | 1358 | `			if( &pCur[1] >= pGen->pIn ){` |
|        - | 1359 | ``				/* `array(1 => )`: php names the token that SHOULD have started the value —`` |
|        - | 1360 | `				 * the ')' or ']' closing the literal — not the '=>' it just read. Passing 0` |
|        - | 1361 | `				 * makes the helper reach for the token past this entry's slice. */` |
|       13 | 1362 | `				rc = PH7_GenSyntaxError(&(*pGen),0,0);` |
|       13 | 1363 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1364 | `					return SXERR_ABORT;` |
|        - | 1365 | `				}` |
|       13 | 1366 | `				return SXRET_OK;` |
|        - | 1367 | `			}` |
|        - | 1368 | `			/* Compile the expression holding the key */` |
|   401351 | 1369 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|        - | 1370 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|   401351 | 1371 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1372 | `				return SXERR_ABORT;` |
|        - | 1373 | `			}` |
|   401351 | 1374 | `			pCur++; /* Jump the '=>' operator */` |
|   200678 | 1375 | `		}else{` |
|        - | 1376 | `			/* Reset back the cursor and point to the entry value */` |
|   761113 | 1377 | `			pCur = pKey;` |
|        - | 1378 | `		}` |
|  1162459 | 1379 | `		if( rc == SXERR_EMPTY ){` |
|        - | 1380 | `			/* No key given: load the nil, TAGGED so LOAD_MAP knows this is an absent key` |
|        - | 1381 | ``			 * (auto-index) rather than an explicit `null =>` one, which php deprecates. */`` |
|   761113 | 1382 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,PH7_LOADC_NOKEY,0 /* nil index */,0,0);` |
|   380554 | 1383 | `		}` |
|  1162459 | 1384 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
|        - | 1385 | `			/* Insertion by reference, [i.e: $a = array(&$x);] */` |
|       45 | 1386 | `			xValidator = GenStateArrayNodeValidator; /* Only variable are allowed */` |
|       45 | 1387 | `			iEmitRef = 1;` |
|       45 | 1388 | `			pCur++; /* Jump the '&' token */` |
|       45 | 1389 | `			if( pCur >= pGen->pIn ){` |
|        - | 1390 | `				/* Missing value */` |
|        3 | 1391 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing referenced variable");` |
|        3 | 1392 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1393 | `					return SXERR_ABORT;` |
|        - | 1394 | `				}` |
|        3 | 1395 | `				return SXRET_OK;` |
|        - | 1396 | `			}` |
|       19 | 1397 | `		}` |
|        - | 1398 | `		/* Detect array unpack: '...$expr' as the entry value (PHP 7.4+, with` |
|        - | 1399 | `		 * string-key support since PHP 8.1). The parser strips the '...' inside` |
|        - | 1400 | `		 * ExprExtractNode; we only need to know it's there so we can emit` |
|        - | 1401 | `		 * PH7_OP_FLAG_SPREAD after the value, instructing LOAD_MAP to merge the` |
|        - | 1402 | `		 * resulting hashmap rather than insert it as a scalar entry. */` |
|  1162457 | 1403 | `		iSpread = (pCur < pGen->pIn && (pCur->nType & PH7_TK_ELLIPSIS)) ? 1 : 0;` |
|  1162457 | 1404 | `		if( iSpread && (rc != SXERR_EMPTY \|\| iEmitRef) ){` |
|        - | 1405 | `			/* '[k => ...$a]' and '[&...$a]' are syntax errors in PHP — the` |
|        - | 1406 | `			 * '...' token cannot follow either '=>' or '&' inside an array` |
|        - | 1407 | `			 * literal. Emit the same Parse-error wording PHP uses so the` |
|        - | 1408 | `			 * output is engine-portable. */` |
|        6 | 1409 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pCur->nLine,` |
|        - | 1410 | `				"syntax error, unexpected token \"...\"");` |
|        6 | 1411 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1412 | `				return SXERR_ABORT;` |
|        - | 1413 | `			}` |
|        6 | 1414 | `			return SXRET_OK;` |
|        - | 1415 | `		}` |
|        - | 1416 | ``		/* Compile indice value. A BY-REF element (`'k' => &$a[$i]`) is an`` |
|        - | 1417 | `		 * lvalue: php VIVIFIES a missing subscript when a reference is taken,` |
|        - | 1418 | `		 * so compile it in write context (LOAD_IDX iP2=1, create-if-missing)` |
|        - | 1419 | `		 * instead of a read-only load — which also keeps the undefined-key` |
|        - | 1420 | `		 * warning (a read-only diagnostic) from false-firing here. */` |
|  1743677 | 1421 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,` |
|   581224 | 1422 | `			iEmitRef ? EXPR_FLAG_LOAD_IDX_STORE` |
|        - | 1423 | `			         : EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,` |
|   581224 | 1424 | `			xValidator);` |
|  1162453 | 1425 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1426 | `			return SXERR_ABORT;` |
|        - | 1427 | `		}` |
|  1162453 | 1428 | `		if( iSpread ){` |
|        - | 1429 | `			/* Mark the value on TOS as a spread source; LOAD_MAP merges it. */` |
|       73 | 1430 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FLAG_SPREAD,0,0,0,0);` |
|  1162418 | 1431 | `		}else if( iEmitRef ){` |
|        - | 1432 | `			/* Emit the load reference instruction */` |
|       40 | 1433 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|       18 | 1434 | `		}` |
|  1162453 | 1435 | `		xValidator = 0;` |
|  1162453 | 1436 | `		iEmitRef = 0;` |
|  1162453 | 1437 | `		iSpread = 0;` |
|  1162453 | 1438 | `		nPair++;` |
|        5 | 1439 | `	}` |
|        - | 1440 | `	/* Emit the load map instruction */` |
|   670977 | 1441 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|        - | 1442 | `	/* Node successfully compiled */` |
|   670977 | 1443 | `	return SXRET_OK;` |
|   335500 | 1444 | `}` |
|        - | 1445 | `/*` |
|        - | 1446 | ` * Compile the 'array' language construct.` |
|        - | 1447 | ` *	 According to the PHP language reference manual` |
|        - | 1448 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|        - | 1449 | ` *   values to keys. This type is optimized for several different uses; it can` |
|        - | 1450 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|        - | 1451 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|        - | 1452 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|        - | 1453 | ` */` |
|   428180 | 1454 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 | 1455 | `{` |
|        - | 1456 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   428185 | 1457 | `	pGen->pIn += 2;` |
|   428185 | 1458 | `	pGen->pEnd--;` |
|   214090 | 1459 | `	SXUNUSED(iCompileFlag);` |
|   428185 | 1460 | `	return GenStateCompileArrayBody(pGen);` |
|        5 | 1461 | `}` |
|        - | 1462 | `/*` |
|        - | 1463 | ` * Compile the PHP 8.5 clone(...) call form:` |
|        - | 1464 | `` *   clone($object)                          -> identical to the `clone $object` operator`` |
|        - | 1465 | ` *   clone($object, ['prop' => value, ...])  -> clone, run __clone(), then apply the` |
|        - | 1466 | ` *                                              property updates as scope-aware writes` |
|        - | 1467 | ` *   clone(object: $o, withProperties: [..]) -> the named-argument spelling` |
|        - | 1468 | ` * Codegen: compile the object argument and emit OP_CLONE (which clones and runs` |
|        - | 1469 | ` * __clone()); if a withProperties argument is present, compile it and emit` |
|        - | 1470 | ` * OP_CLONE_APPLY, which applies each update to the fresh clone AFTER __clone(),` |
|        - | 1471 | ` * honouring visibility / readonly-set-scope / typed-property enforcement in the` |
|        - | 1472 | ` * calling scope. The parser (ExprExtractNode) delimited this node's tokens as` |
|        - | 1473 | `` * `clone ( ... )`; pGen->pIn/pEnd point at the first/one-past-last of that range.`` |
|        - | 1474 | ` */` |
|       22 | 1475 | `PH7_PRIVATE sxi32 PH7_CompileCloneCall(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        2 | 1476 | `{` |
|        - | 1477 | `	SyToken *pIn,*pEnd,*pNext;` |
|       24 | 1478 | `	SyToken *pObjStart = 0,*pObjEnd = 0;` |
|       24 | 1479 | `	SyToken *pUpdStart = 0,*pUpdEnd = 0;` |
|       24 | 1480 | `	int nArg = 0;` |
|        - | 1481 | `	sxi32 rc;` |
|       11 | 1482 | `	SXUNUSED(iCompileFlag);` |
|        - | 1483 | `	/* pGen->pIn -> 'clone', pGen->pIn[1] -> '(', pGen->pEnd -> one past ')'. */` |
|       24 | 1484 | `	pIn  = pGen->pIn + 2;   /* skip 'clone' and the opening '(' */` |
|       24 | 1485 | `	pEnd = pGen->pEnd - 1;  /* exclude the closing ')' */` |
|        - | 1486 | `	/* clone(...) first-class-callable form: a lone ellipsis is the whole list. */` |
|       24 | 1487 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|      ! 0 | 1488 | `		return PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|        - | 1489 | `			"clone(...) first-class callable form is not yet supported");` |
|        - | 1490 | `	}` |
|        - | 1491 | `	/* Split the (at most two) comma-separated arguments, tolerating named labels. */` |
|       62 | 1492 | `	while( pIn < pEnd ){` |
|       40 | 1493 | `		SyToken *pArgStart,*pArgEnd,*pName = 0;` |
|       40 | 1494 | `		if( PH7_GetNextExpr(pIn,pEnd,&pNext) != SXRET_OK ){` |
|      ! 0 | 1495 | `			break;` |
|        - | 1496 | `		}` |
|       40 | 1497 | `		pArgStart = pIn;` |
|       40 | 1498 | `		pArgEnd   = pNext;` |
|        - | 1499 | `		/* Named-argument label: <ID\|keyword> ':' expr. A single ':' is PH7_TK_COLON;` |
|        - | 1500 | ``		 * '::' is a distinct operator token, so this never mis-fires on `A::B`. */`` |
|       38 | 1501 | `		if( (pArgEnd - pArgStart) >= 2` |
|       37 | 1502 | `			&& (pArgStart[0].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|       23 | 1503 | `			&& (pArgStart[1].nType & PH7_TK_COLON) ){` |
|        5 | 1504 | `			pName = pArgStart;` |
|        5 | 1505 | `			pArgStart += 2;` |
|        2 | 1506 | `		}` |
|       40 | 1507 | `		if( pName ){` |
|        - | 1508 | `` 			/* PHP named parameters are case-SENSITIVE, so `Object:`/`WITHPROPERTIES:` `` |
|        - | 1509 | `			 * must be rejected as unknown (SyMemcmp, not SyStrnicmp). */` |
|        4 | 1510 | `			if( pName->sData.nByte == sizeof("object")-1` |
|        4 | 1511 | `				&& SyMemcmp(pName->sData.zString,"object",sizeof("object")-1) == 0 ){` |
|        3 | 1512 | `				pObjStart = pArgStart; pObjEnd = pArgEnd;` |
|        4 | 1513 | `			}else if( pName->sData.nByte == sizeof("withProperties")-1` |
|        3 | 1514 | `				&& SyMemcmp(pName->sData.zString,"withProperties",sizeof("withProperties")-1) == 0 ){` |
|        3 | 1515 | `				pUpdStart = pArgStart; pUpdEnd = pArgEnd;` |
|        2 | 1516 | `			}else{` |
|      ! 0 | 1517 | `				return PH7_GenCompileError(pGen,E_ERROR,pName->nLine,` |
|      ! 0 | 1518 | `					"Unknown named parameter $%z",&pName->sData);` |
|        1 | 1519 | `			}` |
|       38 | 1520 | `		}else if( nArg == 0 ){` |
|       22 | 1521 | `			pObjStart = pArgStart; pObjEnd = pArgEnd;` |
|       25 | 1522 | `		}else if( nArg == 1 ){` |
|       15 | 1523 | `			pUpdStart = pArgStart; pUpdEnd = pArgEnd;` |
|        8 | 1524 | `		}else{` |
|      ! 0 | 1525 | `			return PH7_GenCompileError(pGen,E_ERROR,pArgStart->nLine,` |
|        - | 1526 | `				"clone() expects at most 2 arguments");` |
|        - | 1527 | `		}` |
|       40 | 1528 | `		nArg++;` |
|       40 | 1529 | `		pIn = pNext;` |
|       40 | 1530 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|       17 | 1531 | `			pIn++; /* step over the argument separator */` |
|        8 | 1532 | `		}` |
|        2 | 1533 | `	}` |
|       24 | 1534 | `	if( pObjStart == 0 \|\| pObjStart >= pObjEnd ){` |
|      ! 0 | 1535 | `		return PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 1536 | `			"clone() expects at least 1 argument, 0 given");` |
|        - | 1537 | `	}` |
|        - | 1538 | `	/* Object argument -> clone (+ __clone()). */` |
|       24 | 1539 | `	rc = GenStateCompileArrayEntry(pGen,pObjStart,pObjEnd,EXPR_FLAG_RDONLY_LOAD,0);` |
|       24 | 1540 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 1541 | `		return SXERR_ABORT;` |
|        - | 1542 | `	}` |
|       24 | 1543 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_CLONE,0,0,0,0);` |
|        - | 1544 | `	/* Property updates (evaluated after __clone runs). */` |
|       24 | 1545 | `	if( pUpdStart && pUpdStart < pUpdEnd ){` |
|       17 | 1546 | `		rc = GenStateCompileArrayEntry(pGen,pUpdStart,pUpdEnd,EXPR_FLAG_RDONLY_LOAD,0);` |
|       17 | 1547 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1548 | `			return SXERR_ABORT;` |
|        - | 1549 | `		}` |
|       17 | 1550 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CLONE_APPLY,0,0,0,0);` |
|        8 | 1551 | `	}` |
|       24 | 1552 | `	return SXRET_OK;` |
|       13 | 1553 | `}` |
|        - | 1554 | `/*` |
|        - | 1555 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|        - | 1556 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|        - | 1557 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|        - | 1558 | ` */` |
|   242810 | 1559 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 | 1560 | `{` |
|        - | 1561 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|   242815 | 1562 | `	pGen->pIn++;` |
|   242815 | 1563 | `	pGen->pEnd--;` |
|   121405 | 1564 | `	SXUNUSED(iCompileFlag);` |
|   242815 | 1565 | `	return GenStateCompileArrayBody(pGen);` |
|        5 | 1566 | `}` |
|        - | 1567 | `/*` |
|        - | 1568 | ` * Expression tree validator callback for the 'list' language construct.` |
|        - | 1569 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|        - | 1570 | ` * an invalid expression tree and this function will generate the appropriate` |
|        - | 1571 | ` * error message.` |
|        - | 1572 | ` * See the routine responible of compiling the list language construct` |
|        - | 1573 | ` * for more inforation.` |
|        - | 1574 | ` */` |
|      218 | 1575 | `static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        5 | 1576 | `{` |
|      223 | 1577 | `	sxi32 rc = SXRET_OK;` |
|      223 | 1578 | `	if( pRoot->pOp ){` |
|        4 | 1579 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */` |
|        2 | 1580 | `			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){` |
|        - | 1581 | `				/* Unexpected expression */` |
|      ! 0 | 1582 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - | 1583 | `					"Assignments can only happen to writable values");` |
|      ! 0 | 1584 | `				if( rc != SXERR_ABORT ){` |
|      ! 0 | 1585 | `					rc = SXERR_INVALID;` |
|      ! 0 | 1586 | `				}` |
|        1 | 1587 | `		}` |
|      221 | 1588 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|        - | 1589 | `		/* Unexpected expression */` |
|        6 | 1590 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - | 1591 | `			"Assignments can only happen to writable values");` |
|        6 | 1592 | `		if( rc != SXERR_ABORT ){` |
|        6 | 1593 | `			rc = SXERR_INVALID;` |
|        2 | 1594 | `		}` |
|        2 | 1595 | `	}` |
|      223 | 1596 | `	return rc;` |
|        5 | 1597 | `}` |
|        - | 1598 | `/*` |
|        - | 1599 | ` * Compile the 'list' language construct.` |
|        - | 1600 | ` *  According to the PHP language reference` |
|        - | 1601 | ` *  list(): Assign variables as if they were an array.` |
|        - | 1602 | ` *  list() is used to assign a list of variables in one operation.` |
|        - | 1603 | ` *  Description` |
|        - | 1604 | ` *   array list (mixed $varname [, mixed $... ] )` |
|        - | 1605 | ` *   Like array(), this is not really a function, but a language construct.` |
|        - | 1606 | ` *   list() is used to assign a list of variables in one operation.` |
|        - | 1607 | ` *  Parameters` |
|        - | 1608 | ` *   $varname: A variable.` |
|        - | 1609 | ` *  Return Values` |
|        - | 1610 | ` *   The assigned array.` |
|        - | 1611 | ` */` |
|        - | 1612 | `/* Nested list entry recorded during first pass of list body compilation */` |
|        - | 1613 | `struct NestedListEntry {` |
|        - | 1614 | `	sxi32 nIndex;        /* Position in the outer list (0-based) */` |
|        - | 1615 | `	SyToken *pStart;     /* Token range: start of nested construct */` |
|        - | 1616 | `	SyToken *pEnd;       /* Token range: past closing delimiter */` |
|        - | 1617 | `	sxi32 isShort;       /* 1 if [...] form, 0 if list(...) form */` |
|        - | 1618 | `};` |
|        - | 1619 | `/*` |
|        - | 1620 | ` * Compile the body of a *keyed* list/short-list destructuring (PHP 7.1), where` |
|        - | 1621 | `` * every entry has the form `keyExpr => target`. The source array is on the stack`` |
|        - | 1622 | ` * top on entry and remains there on exit, mirroring the positional LOAD_LIST` |
|        - | 1623 | ` * path so the caller's teardown is unchanged. For each entry: DUP the source,` |
|        - | 1624 | ` * push the key, LOAD_IDX to fetch source[key] (NULL on a missing key, silently,` |
|        - | 1625 | ` * like a normal subscript read), then assign the fetched value to the target — a` |
|        - | 1626 | ` * nested [...]/list() recurses, a simple lvalue uses the same STORE fold as a` |
|        - | 1627 | ` * normal assignment (the value sits below the lvalue-load, exactly as in` |
|        - | 1628 | ` * GenStateEmitExprCode where the assignment RHS precedes the LHS load).` |
|        - | 1629 | ` */` |
|       22 | 1630 | `static sxi32 GenStateCompileKeyedListBody(ph7_gen_state *pGen)` |
|        1 | 1631 | `{` |
|        - | 1632 | `	SyToken *pNext;` |
|        - | 1633 | `	sxi32 rc;` |
|       53 | 1634 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|        - | 1635 | `		SyToken *pArrow,*pTarget;` |
|        - | 1636 | ``		/* Split `keyExpr => target` at the top-level '=>' */`` |
|       31 | 1637 | `		pArrow = GenStateFindTopLevelArrow(pGen->pIn,pNext);` |
|       31 | 1638 | `		pTarget = &pArrow[1];` |
|       31 | 1639 | `		if( pArrow <= pGen->pIn \|\| pTarget >= pNext ){` |
|        - | 1640 | ``			/* Empty key (`[ => $v]`) or empty value (`["k" =>]`): PHP rejects`` |
|        - | 1641 | `			 * both. Reject rather than silently emitting unbalanced bytecode. */` |
|      ! 0 | 1642 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - | 1643 | `				"Cannot use empty array entries in keyed array assignment");` |
|      ! 0 | 1644 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 1645 | `		}` |
|        - | 1646 | `		/* DUP the source array (it is on the stack top) */` |
|       31 | 1647 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|        - | 1648 | `		/* Compile the key expression; it is pushed above the DUP'd source */` |
|       31 | 1649 | `		rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pArrow,EXPR_FLAG_RDONLY_LOAD,0);` |
|       31 | 1650 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1651 | `			return SXERR_ABORT;` |
|        - | 1652 | `		}` |
|        - | 1653 | `		/* LOAD_IDX: pop the key, replace the DUP'd source with source[key].` |
|        - | 1654 | `		 * iP2=7 is the keyed-destructuring read context: an array source reads like` |
|        - | 1655 | ``		 * iP2=0 (missing key loads NULL silently, matching a normal `$arr[$k]` read;`` |
|        - | 1656 | `		 * PHP also emits an "Undefined array key" warning here, PHL omits it — §3.7),` |
|        - | 1657 | `		 * but a NON-array source yields NULL + a per-key "Cannot use <type> as array"` |
|        - | 1658 | `		 * warning instead of char-indexing a string (matching PHP's OP_LOAD_LIST path). */` |
|       31 | 1659 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,7,0,0);` |
|       31 | 1660 | `		if( pTarget < pNext && ( (pTarget->nType & PH7_TK_OSB)` |
|       28 | 1661 | `			\|\| ( (pTarget->nType & PH7_TK_KEYWORD)` |
|       15 | 1662 | `				&& SX_PTR_TO_INT(pTarget->pUserData) == PH7_TKWRD_LIST ) ) ){` |
|        - | 1663 | `			/* Nested destructuring:  ["k" => [ ... ]]  or  ["k" => list( ... )].` |
|        - | 1664 | `			 * Treat source[key] as the inner body's source, then drop the` |
|        - | 1665 | `			 * leftover it leaves behind (mirrors the positional nested path). */` |
|        5 | 1666 | `			sxi32 isShort = (pTarget->nType & PH7_TK_OSB) != 0;` |
|        5 | 1667 | `			SyToken *pSavedIn = pGen->pIn;` |
|        5 | 1668 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|        5 | 1669 | `			pGen->pIn = pTarget;` |
|        5 | 1670 | `			pGen->pEnd = pNext;` |
|        5 | 1671 | `			rc = isShort ? PH7_CompileShortList(&(*pGen),0)` |
|        2 | 1672 | `			             : PH7_CompileList(&(*pGen),0);` |
|        5 | 1673 | `			pGen->pIn = pSavedIn;` |
|        5 | 1674 | `			pGen->pEnd = pSavedEnd;` |
|        5 | 1675 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1676 | `				return SXERR_ABORT;` |
|        - | 1677 | `			}` |
|        5 | 1678 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        3 | 1679 | `		}else{` |
|        - | 1680 | `			/* Simple lvalue target ($v / $o->p / $a[i] / Cls::$s). source[key]` |
|        - | 1681 | `			 * is already on the stack as the value; compiling the target appends` |
|        - | 1682 | `			 * its lvalue-load, which we fold into a STORE just as a normal` |
|        - | 1683 | `			 * assignment does. */` |
|        - | 1684 | `			VmInstr *pInstr;` |
|       27 | 1685 | `			sxi32 iVmOp = PH7_OP_STORE;` |
|       27 | 1686 | `			sxi32 iP1 = 0, iP2 = 0;` |
|       27 | 1687 | `			void *p3 = 0;` |
|       27 | 1688 | `			rc = GenStateCompileArrayEntry(&(*pGen),pTarget,pNext,` |
|        - | 1689 | `				EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|       27 | 1690 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1691 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 1692 | `			}` |
|       27 | 1693 | `			if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|       27 | 1694 | `				if( pInstr->iOp == PH7_OP_MEMBER ){` |
|        3 | 1695 | `					iP2 = 1; /* member store: keep MEMBER, store value below it */` |
|       26 | 1696 | `				}else if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|        3 | 1697 | `					iVmOp = PH7_OP_STORE_IDX;` |
|        3 | 1698 | `					iP1 = pInstr->iP1;` |
|        3 | 1699 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|        2 | 1700 | `				}else{` |
|       23 | 1701 | `					p3 = pInstr->p3; /* named store: $v = value */` |
|       23 | 1702 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|        - | 1703 | `				}` |
|       13 | 1704 | `			}` |
|       27 | 1705 | `			PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|        - | 1706 | `			/* STORE leaves the assigned value on the stack top; drop it so the` |
|        - | 1707 | `			 * source array is back on top for the next entry. */` |
|       27 | 1708 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        - | 1709 | `		}` |
|       31 | 1710 | `		pGen->pIn = &pNext[1];` |
|        1 | 1711 | `	}` |
|       23 | 1712 | `	return SXRET_OK;` |
|       12 | 1713 | `}` |
|        - | 1714 | `/*` |
|        - | 1715 | ` * Shared body for list() and short list [...] compilation.` |
|        - | 1716 | ` * Assumes pGen->pIn and pGen->pEnd are already positioned past` |
|        - | 1717 | ` * the opening delimiter and before the closing delimiter.` |
|        - | 1718 | ` */` |
|      126 | 1719 | `static sxi32 GenStateCompileListBody(ph7_gen_state *pGen)` |
|        5 | 1720 | `{` |
|        - | 1721 | `	SySet sNested; /* Dynamically-sized container of NestedListEntry */` |
|        - | 1722 | `	SyToken *pNext;` |
|        - | 1723 | `	SyToken *pClassifyIn;` |
|      131 | 1724 | `	sxi32 nKeyed = 0, nPositional = 0, nEmpty = 0;` |
|        - | 1725 | `	sxi32 nExpr;` |
|        - | 1726 | `	sxi32 rc;` |
|        - | 1727 | ``	/* First pass: classify entries as keyed (`k => v`), positional, or empty`` |
|        - | 1728 | `	 * skip slots ([,]). A list level must be entirely keyed or entirely` |
|        - | 1729 | `	 * positional — PHP fatals on a mix, and on an empty slot inside a keyed` |
|        - | 1730 | `	 * list. */` |
|      131 | 1731 | `	pClassifyIn = pGen->pIn;` |
|      379 | 1732 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|      253 | 1733 | `		if( pGen->pIn >= pNext ){` |
|       13 | 1734 | `			nEmpty++;` |
|      247 | 1735 | `		}else if( GenStateFindTopLevelArrow(pGen->pIn,pNext) < pNext ){` |
|       31 | 1736 | `			nKeyed++;` |
|       16 | 1737 | `		}else{` |
|      211 | 1738 | `			nPositional++;` |
|        - | 1739 | `		}` |
|      253 | 1740 | `		pGen->pIn = &pNext[1];` |
|        5 | 1741 | `	}` |
|      131 | 1742 | `	pGen->pIn = pClassifyIn;` |
|      131 | 1743 | `	if( nKeyed > 0 && nEmpty > 0 ){` |
|      ! 0 | 1744 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - | 1745 | `			"Cannot use empty array entries in keyed array assignment");` |
|      ! 0 | 1746 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 1747 | `	}` |
|      131 | 1748 | `	if( nKeyed > 0 && nPositional > 0 ){` |
|      ! 0 | 1749 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - | 1750 | `			"Cannot mix keyed and unkeyed array entries in assignments");` |
|      ! 0 | 1751 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 1752 | `	}` |
|      131 | 1753 | `	if( nKeyed > 0 ){` |
|       23 | 1754 | `		return GenStateCompileKeyedListBody(pGen);` |
|        - | 1755 | `	}` |
|      109 | 1756 | `	nExpr = 0;` |
|      109 | 1757 | `	SySetInit(&sNested,&pGen->pVm->sAllocator,sizeof(struct NestedListEntry));` |
|      327 | 1758 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|      223 | 1759 | `		if( pGen->pIn < pNext ){` |
|        - | 1760 | `			/* Check for nested list() */` |
|      211 | 1761 | `			if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        3 | 1762 | `				SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|        - | 1763 | `				/* Record this nested list for post-processing */` |
|        3 | 1764 | `				SyToken *pListEnd = 0;` |
|        3 | 1765 | `				if( &pGen->pIn[1] < pNext && (pGen->pIn[1].nType & PH7_TK_LPAREN) ){` |
|        3 | 1766 | `					PH7_DelimitNestedTokens(pGen->pIn+2,pNext,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|        1 | 1767 | `				}` |
|        3 | 1768 | `				if( pListEnd ){` |
|        - | 1769 | `					struct NestedListEntry sEntry;` |
|        3 | 1770 | `					sEntry.nIndex = nExpr;` |
|        3 | 1771 | `					sEntry.pStart = pGen->pIn;` |
|        3 | 1772 | `					sEntry.pEnd = pListEnd + 1;` |
|        3 | 1773 | `					sEntry.isShort = 0;` |
|        3 | 1774 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|        1 | 1775 | `				}` |
|        - | 1776 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|        3 | 1777 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|      210 | 1778 | `			}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|        - | 1779 | `				/* Nested short destructuring [...] */` |
|       13 | 1780 | `				SyToken *pBracketEnd = 0;` |
|       13 | 1781 | `				PH7_DelimitNestedTokens(pGen->pIn+1,pNext,PH7_TK_OSB,PH7_TK_CSB,&pBracketEnd);` |
|       13 | 1782 | `				if( pBracketEnd ){` |
|        - | 1783 | `					struct NestedListEntry sEntry;` |
|       13 | 1784 | `					sEntry.nIndex = nExpr;` |
|       13 | 1785 | `					sEntry.pStart = pGen->pIn;` |
|       13 | 1786 | `					sEntry.pEnd = pBracketEnd + 1;` |
|       13 | 1787 | `					sEntry.isShort = 1;` |
|       13 | 1788 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|        6 | 1789 | `				}` |
|        - | 1790 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|       13 | 1791 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|        7 | 1792 | `			}else{` |
|        - | 1793 | `				/* Compile the expression holding the variable */` |
|      197 | 1794 | `				rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|      197 | 1795 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 1796 | `					SySetRelease(&sNested);` |
|      ! 0 | 1797 | `					return SXRET_OK;` |
|        - | 1798 | `				}` |
|        - | 1799 | `			}` |
|      108 | 1800 | `		}else{` |
|        - | 1801 | `			/* Empty entry,load NULL */` |
|       13 | 1802 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);` |
|        - | 1803 | `		}` |
|      223 | 1804 | `		nExpr++;` |
|        - | 1805 | `		/* Advance the stream cursor */` |
|      223 | 1806 | `		pGen->pIn = &pNext[1];` |
|        5 | 1807 | `	}` |
|        - | 1808 | `	/* Emit the LOAD_LIST instruction */` |
|      109 | 1809 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,0,0,0);` |
|        - | 1810 | `	/* After LOAD_LIST, the source array is still on the stack top.` |
|        - | 1811 | `	 * For each nested entry, emit code to extract the sub-array` |
|        - | 1812 | `	 * at the corresponding index and recursively destructure it.` |
|        - | 1813 | `	 */` |
|      109 | 1814 | `	if( SySetUsed(&sNested) > 0 ){` |
|       13 | 1815 | `		struct NestedListEntry *apNested = (struct NestedListEntry *)SySetBasePtr(&sNested);` |
|        - | 1816 | `		sxu32 i;` |
|       27 | 1817 | `		for(i = 0; i < SySetUsed(&sNested); i++){` |
|       15 | 1818 | `			SyToken *pSavedIn = pGen->pIn;` |
|       15 | 1819 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|        - | 1820 | `			ph7_value *pIdx;` |
|        - | 1821 | `			sxu32 nConstIdx;` |
|        - | 1822 | `			/* DUP the source array (it's on stack top) */` |
|       15 | 1823 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|        - | 1824 | `			/* Push the integer index for this nested entry */` |
|       15 | 1825 | `			pIdx = PH7_ReserveConstObj(pGen->pVm,&nConstIdx);` |
|       15 | 1826 | `			if( pIdx == 0 ){` |
|      ! 0 | 1827 | `				PH7_GenCompileError(&(*pGen),E_ERROR,0,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1828 | `				SySetRelease(&sNested);` |
|      ! 0 | 1829 | `				return SXERR_ABORT;` |
|        - | 1830 | `			}` |
|       15 | 1831 | `			PH7_MemObjInitFromInt(pGen->pVm,pIdx,(sxi64)apNested[i].nIndex);` |
|       15 | 1832 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nConstIdx,0,0);` |
|        - | 1833 | `			/* LOAD_IDX: pop index, replace DUP'd source with source[index].` |
|        - | 1834 | `			 * iP2=2 signals the VM to emit an "Undefined array key" warning` |
|        - | 1835 | `			 * when the key is missing (PHP-compatible list destructuring).` |
|        - | 1836 | `			 */` |
|       15 | 1837 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,2,0,0);` |
|        - | 1838 | `			/* Recursively compile the inner list */` |
|       15 | 1839 | `			pGen->pIn = apNested[i].pStart;` |
|       15 | 1840 | `			pGen->pEnd = apNested[i].pEnd;` |
|       15 | 1841 | `			if( apNested[i].isShort ){` |
|       13 | 1842 | `				rc = PH7_CompileShortList(&(*pGen),0);` |
|        7 | 1843 | `			}else{` |
|        3 | 1844 | `				rc = PH7_CompileList(&(*pGen),0);` |
|        - | 1845 | `			}` |
|       15 | 1846 | `			pGen->pIn = pSavedIn;` |
|       15 | 1847 | `			pGen->pEnd = pSavedEnd;` |
|       15 | 1848 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1849 | `				SySetRelease(&sNested);` |
|      ! 0 | 1850 | `				return SXERR_ABORT;` |
|        - | 1851 | `			}` |
|        - | 1852 | `			/* Pop the leftover source[index] from the inner LOAD_LIST */` |
|       15 | 1853 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        8 | 1854 | `		}` |
|        6 | 1855 | `	}` |
|      109 | 1856 | `	SySetRelease(&sNested);` |
|        - | 1857 | `	/* Node successfully compiled */` |
|      109 | 1858 | `	return SXRET_OK;` |
|       68 | 1859 | `}` |
|       40 | 1860 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 | 1861 | `{` |
|        - | 1862 | `	/* Jump the 'list' keyword, the leading '(' and exclude trailing ')' */` |
|       45 | 1863 | `	pGen->pIn += 2;` |
|       45 | 1864 | `	pGen->pEnd--;` |
|       20 | 1865 | `	SXUNUSED(iCompileFlag);` |
|       45 | 1866 | `	return GenStateCompileListBody(pGen);` |
|        5 | 1867 | `}` |
|       86 | 1868 | `PH7_PRIVATE sxi32 PH7_CompileShortList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        3 | 1869 | `{` |
|        - | 1870 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|       89 | 1871 | `	pGen->pIn++;` |
|       89 | 1872 | `	pGen->pEnd--;` |
|       43 | 1873 | `	SXUNUSED(iCompileFlag);` |
|       89 | 1874 | `	return GenStateCompileListBody(pGen);` |
|        3 | 1875 | `}` |
|        - | 1876 |  |
