# src/ph7/compile_literal.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1226/1428 lines (85.85%)

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
|        - |   19 | ` *   base  8 => 0-7` |
|        - |   20 | ` *   base  2 => 0 or 1` |
|        - |   21 | ` *   base 10 => SyisDigit (0-9, also used for octal literals which share the` |
|        - |   22 | ` *              decimal scan in the lexer)` |
|        - |   23 | ` */` |
|     1780 |   24 | `static int GenStateIsBaseDigit(int c, int base)` |
|        5 |   25 | `{` |
|        - |   26 | `	/* ASCII arithmetic, not <ctype.h>: the byte can be any value in a string` |
|        - |   27 | `	 * literal, and isdigit()/isxdigit() are both locale-dependent and undefined` |
|        - |   28 | `	 * for a negative char. */` |
|     1785 |   29 | `	if( base == 16 ){` |
|      113 |   30 | `		return (c >= '0' && c <= '9') \|\| (c >= 'a' && c <= 'f') \|\| (c >= 'A' && c <= 'F');` |
|        - |   31 | `	}` |
|     1673 |   32 | `	if( base == 8 ){ return c >= '0' && c <= '7'; }` |
|     1667 |   33 | `	if( base == 2 ){ return c == '0' \|\| c == '1'; }` |
|     1381 |   34 | `	return c >= '0' && c <= '9';` |
|      895 |   35 | `}` |
|        - |   36 | `/*` |
|        - |   37 | ` * Given the raw text of a numeric literal token, locate a misplaced PHP 7.4` |
|        - |   38 | ` * underscore separator so the caller can report the malformed portion with` |
|        - |   39 | ` * the exact wording PHP uses:` |
|        - |   40 | ` *` |
|        - |   41 | ` *   syntax error, unexpected identifier "X"` |
|        - |   42 | ` *` |
|        - |   43 | ` * The lexer guarantees that every underscore it consumed as a separator is` |
|        - |   44 | ` * surrounded by valid base digits; anything else sits in the trailing run` |
|        - |   45 | ` * absorbed by the lexer specifically to let this validator see and report` |
|        - |   46 | ` * it. That invariant means the malformed span is exactly [bad .. nByte) —` |
|        - |   47 | ` * no forward rescan needed.` |
|        - |   48 | ` *` |
|        - |   49 | ` * Returns 1 and fills pBadStart / pBadLen when the literal is malformed;` |
|        - |   50 | ` * returns 0 when it is well-formed.` |
|        - |   51 | ` */` |
|  4491728 |   52 | `static int GenStateFindBadNumericSeparator(` |
|        - |   53 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|        5 |   54 | `{` |
|  4491733 |   55 | `	const char *z = pRaw->zString;` |
|  4491733 |   56 | `	sxu32 n = pRaw->nByte;` |
|  4491733 |   57 | `	int base = 10;` |
|        - |   58 | `	sxu32 i, start;` |
|  4491733 |   59 | `	if( n < 2 ) return 0;` |
|   939337 |   60 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|   122359 |   61 | `		base = 16;` |
|   878160 |   62 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|      287 |   63 | `		base = 2;` |
|      143 |   64 | `	}` |
|  3576679 |   65 | `	for( i = 0; i < n; ++i ){` |
|  2637357 |   66 | `		if( z[i] != '_' ) continue;` |
|      552 |   67 | `		if( i > 0 && i + 1 < n` |
|      549 |   68 | `			&& GenStateIsBaseDigit((unsigned char)z[i-1], base)` |
|      551 |   69 | `			&& GenStateIsBaseDigit((unsigned char)z[i+1], base) ){` |
|      544 |   70 | `			continue; /* well-placed separator */` |
|        - |   71 | `		}` |
|        - |   72 | `		/* First misplaced underscore — the lexer already absorbed the full` |
|        - |   73 | `		 * malformed tail, so it runs from here to the end of the token. */` |
|       14 |   74 | `		start = i;` |
|       19 |   75 | `		if( start > 0 && (z[start-1] == 'x' \|\| z[start-1] == 'X'` |
|       10 |   76 | `			\|\| z[start-1] == 'b' \|\| z[start-1] == 'B') ){` |
|      ! 0 |   77 | `			start--; /* include the base letter for 0x_... / 0b_... */` |
|      ! 0 |   78 | `		}` |
|       14 |   79 | `		*pBadStart = &z[start];` |
|       14 |   80 | `		*pBadLen = n - start;` |
|       14 |   81 | `		return 1;` |
|      ! 0 |   82 | `	}` |
|   939327 |   83 | `	return 0;` |
|  2245869 |   84 | `}` |
|        - |   85 | `/*` |
|        - |   86 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|        - |   87 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|        - |   88 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|        - |   89 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|        - |   90 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|        - |   91 | ` * so callers can bail from the current construct).` |
|        - |   92 | ` */` |
|  4491728 |   93 | `PH7_PRIVATE sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|        5 |   94 | `{` |
|  4491733 |   95 | `	const char *zBad = 0;` |
|  4491733 |   96 | `	sxu32 nBad = 0;` |
|        - |   97 | `	SyString sBad;` |
|        - |   98 | `	sxi32 rc;` |
|  4491733 |   99 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|  4491723 |  100 | `		return SXRET_OK;` |
|        - |  101 | `	}` |
|       14 |  102 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|       14 |  103 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|        - |  104 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|       14 |  105 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  106 | `		return SXERR_ABORT;` |
|        - |  107 | `	}` |
|       14 |  108 | `	return SXERR_SYNTAX;` |
|  2245869 |  109 | `}` |
|        - |  110 | `/*` |
|        - |  111 | ` * Strip PHP 7.4 numeric literal separators (underscores between digits) from` |
|        - |  112 | ` * a numeric token's text and yield a SyString suitable for the low-level` |
|        - |  113 | ` * converters (SyStrToInt64 / SyStrToReal / etc.).` |
|        - |  114 | ` *` |
|        - |  115 | ` * Fast path: if the token contains no '_', *pOut aliases pToken with no copy` |
|        - |  116 | ` * and *pzAlloc is set to NULL.` |
|        - |  117 | ` * Stack path: if the cleaned bytes fit in zScratch, they are written there` |
|        - |  118 | ` * and *pzAlloc is set to NULL.` |
|        - |  119 | ` * Heap path: for literals larger than the scratch buffer, a fresh buffer is` |
|        - |  120 | ` * allocated from pAlloc, returned via *pzAlloc, and must be released by the` |
|        - |  121 | ` * caller with SyMemBackendFree once the converter is done.` |
|        - |  122 | ` *` |
|        - |  123 | ` * Returns SXRET_OK on success, SXERR_ABORT on allocator failure (in which` |
|        - |  124 | ` * case *pOut is left untouched and the caller must not read it).` |
|        - |  125 | ` */` |
|  4491786 |  126 | `PH7_PRIVATE sxi32 GenStateStripNumericSeparators(` |
|        - |  127 | `	SyMemBackend *pAlloc,` |
|        - |  128 | `	const SyString *pToken,` |
|        - |  129 | `	char *zScratch, sxu32 nScratch,` |
|        - |  130 | `	SyString *pOut, char **pzAlloc)` |
|        5 |  131 | `{` |
|        - |  132 | `	sxu32 i, j;` |
|  4491791 |  133 | `	int hasUnderscore = 0;` |
|        - |  134 | `	char *zBuf;` |
|  4491791 |  135 | `	*pzAlloc = 0;` |
| 10679529 |  136 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|  6188007 |  137 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|  3093874 |  138 | `	}` |
|  4491791 |  139 | `	if( !hasUnderscore ){` |
|  4491527 |  140 | `		SyStringDupPtr(pOut, pToken);` |
|  4491527 |  141 | `		return SXRET_OK;` |
|        - |  142 | `	}` |
|      266 |  143 | `	if( pToken->nByte <= nScratch ){` |
|      264 |  144 | `		zBuf = zScratch;` |
|      133 |  145 | `	}else{` |
|        3 |  146 | `		zBuf = (char *)SyMemBackendAlloc(pAlloc, pToken->nByte);` |
|        3 |  147 | `		if( zBuf == 0 ){` |
|      ! 0 |  148 | `			return SXERR_ABORT;` |
|        - |  149 | `		}` |
|        3 |  150 | `		*pzAlloc = zBuf;` |
|        - |  151 | `	}` |
|      266 |  152 | `	j = 0;` |
|     2974 |  153 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|     2710 |  154 | `		if( pToken->zString[i] != '_' ){ zBuf[j++] = pToken->zString[i]; }` |
|     1356 |  155 | `	}` |
|      266 |  156 | `	SyStringInitFromBuf(pOut, zBuf, j);` |
|      266 |  157 | `	return SXRET_OK;` |
|  2245898 |  158 | `}` |
|        - |  159 | `/*` |
|        - |  160 | ` * Compile a numeric [i.e: integer or real] literal.` |
|        - |  161 | ` * Notes on the integer type.` |
|        - |  162 | ` *  According to the PHP language reference manual` |
|        - |  163 | ` *  Integers can be specified in decimal (base 10), hexadecimal (base 16), octal (base 8)` |
|        - |  164 | ` *  or binary (base 2) notation, optionally preceded by a sign (- or +).` |
|        - |  165 | ` *  To use octal notation, precede the number with a 0 (zero). To use hexadecimal` |
|        - |  166 | ` *  notation precede the number with 0x. To use binary notation precede the number with 0b.` |
|        - |  167 | ` * Symisc eXtension to the integer type.` |
|        - |  168 | ` *  PH7 introduced platform-independant 64-bit integer unlike the standard PHP engine` |
|        - |  169 | ` *  where the size of an integer is platform-dependent.That is,the size of an integer` |
|        - |  170 | ` *  is 8 bytes and the maximum integer size is 0x7FFFFFFFFFFFFFFF for all platforms` |
|        - |  171 | ` *  [i.e: either 32bit or 64bit].` |
|        - |  172 | ` *  For more information on this powerfull extension please refer to the official` |
|        - |  173 | ` *  documentation.` |
|        - |  174 | ` */` |
|        - |  175 | `/*` |
|        - |  176 | ` * Determine whether an integer literal token exceeds the signed 64-bit range.` |
|        - |  177 | ` * PHP promotes such a literal to a float (e.g. 9223372036854775808 ->` |
|        - |  178 | ` * float(9.22...E+18), 0xFFFFFFFFFFFFFFFF -> float) rather than wrapping or` |
|        - |  179 | ` * dropping digits. pNum is the separator-stripped token (unsigned; the sign of` |
|        - |  180 | ` * a "-1" is a separate unary operator). Base detection mirrors` |
|        - |  181 | ` * PH7_TokenValueToInt64. Returns TRUE on overflow: for a non-decimal base the` |
|        - |  182 | ` * float value is accumulated into *pReal (dv = dv*base + digit); for decimal` |
|        - |  183 | ` * *pbDecimal is set so the caller reuses strtod on the token for a` |
|        - |  184 | ` * correctly-rounded value. Returns FALSE (value fits) for anything it cannot` |
|        - |  185 | ` * confidently classify, so the int path stays in charge.` |
|        - |  186 | ` *` |
|        - |  187 | ` * The int/float CLASSIFICATION is php-exact for every base. VALUES are byte-exact` |
|        - |  188 | ` * for decimal (strtod) and hex (php's zend_hex_strtod uses the same dv*16+digit` |
|        - |  189 | ` * doubling). Octal/binary overflow values can differ from php by the low bit(s):` |
|        - |  190 | ` * php's zend_{oct,bin}_strtod rounds differently than this doubling — e.g. php's` |
|        - |  191 | ` * binary 2**63 is 2**63-1024 whereas this returns the exact 2**63. Recorded as a` |
|        - |  192 | ` * residual; matching php exactly would need a port of those functions.` |
|        - |  193 | ` */` |
|  4481442 |  194 | `static int GenStateIntLiteralOverflows(const SyString *pNum, ph7_real *pReal, int *pbDecimal)` |
|        5 |  195 | `{` |
|  4481447 |  196 | `	const char *z = pNum->zString;` |
|  4481447 |  197 | `	const char *zEnd = z + pNum->nByte;` |
|        - |  198 | `	const char *p, *q;` |
|        - |  199 | `	int n;` |
|  4481447 |  200 | `	*pbDecimal = FALSE;` |
|  4481447 |  201 | `	if( z >= zEnd ){` |
|      ! 0 |  202 | `		return FALSE;` |
|        - |  203 | `	}` |
|  4481447 |  204 | `	if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|        - |  205 | `		/* Hexadecimal: INT64_MAX == 0x7FFF...F (16 digits, leading nibble 7). */` |
|   122361 |  206 | `		p = z + 2;` |
|   154065 |  207 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|   498677 |  208 | `		for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisHex(q[0]); q++ ){ n++; }` |
|   122361 |  209 | `		if( n < 16 \|\| (n == 16 && SyHexToint(p[0]) < 8) ){` |
|   122355 |  210 | `			return FALSE;` |
|        - |  211 | `		}` |
|        7 |  212 | `		{ ph7_real dv = 0;` |
|      103 |  213 | `		  for( q = p; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisHex(q[0]); q++ ){` |
|       97 |  214 | `			dv = dv * 16 + (ph7_real)SyHexToint(q[0]);` |
|       49 |  215 | `		  }` |
|        7 |  216 | `		  *pReal = dv;` |
|        - |  217 | `		}` |
|        7 |  218 | `		return TRUE;` |
|  4359091 |  219 | `	}else if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|        - |  220 | `		/* Binary: INT64_MAX needs 63 significant bits. */` |
|      287 |  221 | `		p = z + 2;` |
|      335 |  222 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|     2172 |  223 | `		for( q = p, n = 0; q < zEnd && (q[0] == '0' \|\| q[0] == '1'); q++ ){ n++; }` |
|      287 |  224 | `		if( n <= 63 ){` |
|      285 |  225 | `			return FALSE;` |
|        - |  226 | `		}` |
|        3 |  227 | `		{ ph7_real dv = 0;` |
|      195 |  228 | `		  for( q = p; q < zEnd && (q[0] == '0' \|\| q[0] == '1'); q++ ){` |
|      129 |  229 | `			dv = dv * 2 + (ph7_real)(q[0] - '0');` |
|       65 |  230 | `		  }` |
|        3 |  231 | `		  *pReal = dv;` |
|        - |  232 | `		}` |
|        3 |  233 | `		return TRUE;` |
|  4358805 |  234 | `	}else if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'o' \|\| z[1] == 'O') ){` |
|        - |  235 | `		/* PHP 8.1 explicit octal 0o/0O: 21 significant octal digits fit in int64. */` |
|       21 |  236 | `		p = z + 2;` |
|       25 |  237 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|       97 |  238 | `		for( q = p, n = 0; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){ n++; }` |
|       21 |  239 | `		if( n <= 21 ){` |
|       21 |  240 | `			return FALSE;` |
|        - |  241 | `		}` |
|      ! 0 |  242 | `		{ ph7_real dv = 0;` |
|      ! 0 |  243 | `		  for( q = p; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){` |
|      ! 0 |  244 | `			dv = dv * 8 + (ph7_real)(q[0] - '0');` |
|      ! 0 |  245 | `		  }` |
|      ! 0 |  246 | `		  *pReal = dv;` |
|        - |  247 | `		}` |
|      ! 0 |  248 | `		return TRUE;` |
|  4358785 |  249 | `	}else if( z[0] == '0' ){` |
|        - |  250 | `		/* Octal: INT64_MAX == 0o777...7 (21 significant octal digits). Skip the` |
|        - |  251 | `		 * leading zeros (incl. the base '0'); a non-octal char such as the 8.1` |
|        - |  252 | `		 * "0o" marker ends the run and leaves it to the int path (as today). */` |
|  1655685 |  253 | `		p = z;` |
|  3311371 |  254 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|  1669531 |  255 | `		for( q = p, n = 0; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){ n++; }` |
|  1655685 |  256 | `		if( n <= 21 ){` |
|  1655683 |  257 | `			return FALSE;` |
|        - |  258 | `		}` |
|        3 |  259 | `		{ ph7_real dv = 0;` |
|       47 |  260 | `		  for( q = p; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){` |
|       45 |  261 | `			dv = dv * 8 + (ph7_real)(q[0] - '0');` |
|       23 |  262 | `		  }` |
|        3 |  263 | `		  *pReal = dv;` |
|        - |  264 | `		}` |
|        3 |  265 | `		return TRUE;` |
|        - |  266 | `	}` |
|        - |  267 | `	/* Decimal: overflow iff more than 19 significant digits, or exactly 19 that` |
|        - |  268 | `	 * compare greater than INT64_MAX. Defer the value to strtod (via the caller)` |
|        - |  269 | `	 * for php-exact rounding. */` |
|  2703105 |  270 | `	p = z;` |
|  2703105 |  271 | `	while( p < zEnd && p[0] == '0' ){ p++; }` |
|  6534043 |  272 | `	for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisDigit(q[0]); q++ ){ n++; }` |
|  2703105 |  273 | `	if( n > 19 \|\| (n == 19 && SyMemcmp(p, "9223372036854775807", 19) > 0) ){` |
|       27 |  274 | `		*pbDecimal = TRUE;` |
|       27 |  275 | `		return TRUE;` |
|        - |  276 | `	}` |
|  2703079 |  277 | `	return FALSE;` |
|  2240726 |  278 | `}` |
|  4491694 |  279 | `PH7_PRIVATE sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  280 | `{` |
|  4491699 |  281 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|  4491699 |  282 | `	sxu32 nIdx = 0;` |
|        - |  283 | `	char zScratch[GEN_NUM_SCRATCH];` |
|  4491699 |  284 | `	char *zAlloc = 0;` |
|        - |  285 | `	SyString sNum;` |
|        - |  286 | `	sxi32 rc;` |
|  2245847 |  287 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|  4491699 |  288 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|  4491699 |  289 | `	if( rc != SXRET_OK ){` |
|        9 |  290 | `		return rc;` |
|        - |  291 | `	}` |
|  6737537 |  292 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|  2245844 |  293 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|  4491693 |  294 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  295 | `		return SXERR_ABORT;` |
|        - |  296 | `	}` |
|  4491693 |  297 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|        - |  298 | `		ph7_value *pObj;` |
|        - |  299 | `		sxi64 iValue;` |
|  4481387 |  300 | `		ph7_real rOverflow = 0;` |
|  4481387 |  301 | `		int bDecimalOverflow = 0;` |
|  4481387 |  302 | `		if( GenStateIntLiteralOverflows(&sNum,&rOverflow,&bDecimalOverflow) ){` |
|        - |  303 | `			/* Literal exceeds the signed 64-bit range: PHP represents it as a` |
|        - |  304 | `			 * float instead of wrapping/dropping digits. */` |
|       37 |  305 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       37 |  306 | `			if( pObj == 0 ){` |
|      ! 0 |  307 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |  308 | `				if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|      ! 0 |  309 | `				return SXERR_ABORT;` |
|        - |  310 | `			}` |
|       37 |  311 | `			if( bDecimalOverflow ){` |
|        - |  312 | `				/* strtod on the decimal token yields php-exact rounding. */` |
|       27 |  313 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|       27 |  314 | `				PH7_MemObjToReal(pObj);` |
|       14 |  315 | `			}else{` |
|       11 |  316 | `				PH7_MemObjInitFromReal(pGen->pVm,pObj,rOverflow);` |
|        - |  317 | `			}` |
|       19 |  318 | `		}else{` |
|  4481351 |  319 | `			iValue = PH7_TokenValueToInt64(&sNum);` |
|  4481351 |  320 | `			pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|  4481351 |  321 | `			if( pObj == 0 ){` |
|      ! 0 |  322 | `				if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|      ! 0 |  323 | `				return SXERR_ABORT;` |
|        - |  324 | `			}` |
|  4481351 |  325 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|        - |  326 | `		}` |
|  2240696 |  327 | `	}else{` |
|        - |  328 | `		/* Real number */` |
|        - |  329 | `		ph7_value *pObj;` |
|        - |  330 | `		/* Reserve a new constant */` |
|    10311 |  331 | `		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    10311 |  332 | `		if( pObj == 0 ){` |
|      ! 0 |  333 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |  334 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|      ! 0 |  335 | `			return SXERR_ABORT;` |
|        - |  336 | `		}` |
|    10311 |  337 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|    10311 |  338 | `		PH7_MemObjToReal(pObj);` |
|        - |  339 | `	}` |
|  4491693 |  340 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|        - |  341 | `	/* Emit the load constant instruction */` |
|  4491693 |  342 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        - |  343 | `	/* Node successfully compiled */` |
|  4491693 |  344 | `	return SXRET_OK;` |
|  2245852 |  345 | `}` |
|        - |  346 | `/*` |
|        - |  347 | ` * Compile a single quoted string.` |
|        - |  348 | ` * According to the PHP language reference manual:` |
|        - |  349 | ` *` |
|        - |  350 | ` *   The simplest way to specify a string is to enclose it in single quotes (the character ' ).` |
|        - |  351 | ` *   To specify a literal single quote, escape it with a backslash (\). To specify a literal` |
|        - |  352 | ` *   backslash, double it (\\). All other instances of backslash will be treated as a literal` |
|        - |  353 | ` *   backslash: this means that the other escape sequences you might be used to, such as \r` |
|        - |  354 | ` *   or \n, will be output literally as specified rather than having any special meaning.` |
|        - |  355 | ` *` |
|        - |  356 | ` */` |
|  6557370 |  357 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  358 | `{` |
|  6557375 |  359 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|        - |  360 | `	const char *zIn,*zCur,*zEnd;` |
|        - |  361 | `	ph7_value *pObj;` |
|        - |  362 | `	sxu32 nIdx;` |
|        - |  363 | `	sxi32 bHasEsc;` |
|  6557375 |  364 | `	nIdx = 0; /* Prevent compiler warning */` |
|        - |  365 | `	/* Delimit the string */` |
|  6557375 |  366 | `	zIn  = pStr->zString;` |
|  6557375 |  367 | `	zEnd = &zIn[pStr->nByte];` |
|  6557375 |  368 | `	if( zIn >= zEnd ){` |
|        - |  369 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|        - |  370 | `		 * rather than reserving a new object each time. */` |
|   484837 |  371 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|   484837 |  372 | `		return SXRET_OK;` |
|        - |  373 | `	}` |
|        - |  374 | `	/* A single-quoted literal whose raw source holds a backslash unescapes to a` |
|        - |  375 | `	 * value that differs from that source (\\ -> \, \' -> '). The literal cache` |
|        - |  376 | `	 * keys FIND on the raw source text but INSTALL on the unescaped value, so` |
|        - |  377 | `	 * caching such a literal lets an unrelated one collide with it — e.g. '\\'` |
|        - |  378 | `	 * (raw \\, value \) would find the entry installed for '\\\\' (raw \\\\,` |
|        - |  379 | `	 * value \\) and load two backslashes. Only cache literals whose value equals` |
|        - |  380 | `	 * their source, i.e. those with no backslash to unescape. */` |
|  6072543 |  381 | `	bHasEsc = 0;` |
|        - |  382 | `	{` |
|        - |  383 | `		const char *zScan;` |
| 74045155 |  384 | `		for( zScan = zIn ; zScan < zEnd ; zScan++ ){` |
| 68068073 |  385 | `			if( zScan[0] == '\\' ){ bHasEsc = 1; break; }` |
| 33986311 |  386 | `		}` |
|        - |  387 | `	}` |
|  6072543 |  388 | `	if( !bHasEsc && SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|        - |  389 | `		/* Already processed,emit the load constant instruction` |
|        - |  390 | `		 * and return.` |
|        - |  391 | `		 */` |
|  3529245 |  392 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|  3529245 |  393 | `		return SXRET_OK;` |
|        - |  394 | `	}` |
|        - |  395 | `	/* Reserve a new constant */` |
|  2543303 |  396 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  2543303 |  397 | `	if( pObj == 0 ){` |
|      ! 0 |  398 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |  399 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|      ! 0 |  400 | `		return SXERR_ABORT;` |
|        - |  401 | `	}` |
|  2543303 |  402 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|        - |  403 | `	/* Compile the node */` |
|  2604644 |  404 | `	for(;;){` |
|  5209293 |  405 | `		if( zIn >= zEnd ){` |
|        - |  406 | `			/* End of input */` |
|  2543303 |  407 | `			break;` |
|        - |  408 | `		}` |
|  2665995 |  409 | `		zCur = zIn;` |
| 52778435 |  410 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
| 50112445 |  411 | `			zIn++;` |
|        5 |  412 | `		}` |
|  2665995 |  413 | `		if( zIn > zCur ){` |
|        - |  414 | `			/* Append raw contents*/` |
|  2616035 |  415 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|  1308015 |  416 | `		}` |
|  2665995 |  417 | `		zIn++;` |
|  2665995 |  418 | `		if( zIn < zEnd ){` |
|   168033 |  419 | `			if( zIn[0] == '\\' ){` |
|        - |  420 | `				/* A literal backslash */` |
|    40901 |  421 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|   147585 |  422 | `			}else if( zIn[0] == '\'' ){` |
|        - |  423 | `				/* A single quote */` |
|       18 |  424 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|       10 |  425 | `			}else{` |
|        - |  426 | `				/* verbatim copy */` |
|   127121 |  427 | `				zIn--;` |
|   127121 |  428 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char)*2);` |
|   127121 |  429 | `				zIn++;` |
|        - |  430 | `			}` |
|    84014 |  431 | `		}` |
|        - |  432 | `		/* Advance the stream cursor */` |
|  2665995 |  433 | `		zIn++;` |
|        5 |  434 | `	}` |
|        - |  435 | `	/* Emit the load constant instruction */` |
|  2543303 |  436 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|  2543303 |  437 | `	if( !bHasEsc && pStr->nByte < 1024 ){` |
|        - |  438 | `		/* Install in the literal table (only when value == source; see above) */` |
|  2447847 |  439 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|  1223921 |  440 | `	}` |
|        - |  441 | `	/* Node successfully compiled */` |
|  2543303 |  442 | `	return SXRET_OK;` |
|  3278690 |  443 | `}` |
|        - |  444 | `/*` |
|        - |  445 | ` * PHP 7.3 flexible heredoc/nowdoc closing-marker indent stripping.` |
|        - |  446 | ` *` |
|        - |  447 | ` * When the lexer matched the closing marker with leading whitespace on its` |
|        - |  448 | ` * own line, it stored the indent count in pGen->pIn->pUserData. The marker's` |
|        - |  449 | ` * indent prefix bytes sit immediately after the stripped body (at` |
|        - |  450 | ` * pIn->sData.zString + pIn->sData.nByte + 1 for LF, +2 for CRLF) in the` |
|        - |  451 | ` * original source buffer — the buffer is stable through compilation.` |
|        - |  452 | ` *` |
|        - |  453 | `` * For each body line, we remove exactly `nIndent` leading bytes that must`` |
|        - |  454 | ` * byte-for-byte match the marker's prefix. Empty lines (0 bytes or bare \r)` |
|        - |  455 | ` * bypass validation. Mismatches raise the exact PHP 7.3+ parse errors:` |
|        - |  456 | ` *   - "Invalid body indentation level (expecting an indentation level of` |
|        - |  457 | ` *     at least N)" — line too short, or first differing byte is not` |
|        - |  458 | ` *     whitespace.` |
|        - |  459 | ` *   - "Invalid indentation - tabs and spaces cannot be mixed" — first` |
|        - |  460 | ` *     differing byte is whitespace but differs from the marker prefix.` |
|        - |  461 | ` */` |
|      128 |  462 | `static sxi32 GenStateStripHeredocIndent(ph7_gen_state *pGen, SyString *pOut)` |
|        5 |  463 | `{` |
|      133 |  464 | `	SyString *pIn = &pGen->pIn->sData;` |
|      133 |  465 | `	sxu32 nIndent = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        - |  466 | `	const char *zPrefix;` |
|        - |  467 | `	const char *z, *zEnd;` |
|        - |  468 | `	char *zBuf, *zDst;` |
|      133 |  469 | `	if( nIndent == 0 ){` |
|        - |  470 | `		/* Legacy column-0 marker: zero-copy fast path */` |
|       86 |  471 | `		*pOut = *pIn;` |
|       86 |  472 | `		return SXRET_OK;` |
|        - |  473 | `	}` |
|        - |  474 | `	/* Recover the marker indent prefix from the original source buffer.` |
|        - |  475 | `	 * Skip the terminator the lexer stripped: one '\n' plus an optional` |
|        - |  476 | `	 * preceding '\r'. Note: when the body is empty (pIn->nByte == 0) the` |
|        - |  477 | `	 * lexer stripped nothing, so this offset is one byte past the true` |
|        - |  478 | `	 * marker-indent start. That is harmless — the strip loop below never` |
|        - |  479 | `	 * runs (z == zEnd), and zPrefix is never dereferenced. */` |
|       49 |  480 | `	zPrefix = pIn->zString + pIn->nByte;` |
|       49 |  481 | `	if( zPrefix[0] == '\r' && zPrefix[1] == '\n' ){` |
|      ! 0 |  482 | `		zPrefix += 2;` |
|      ! 0 |  483 | `	}else{` |
|       49 |  484 | `		zPrefix += 1;` |
|        - |  485 | `	}` |
|        - |  486 | `	/* Allocate scratch buffer sized to the original body (always enough). */` |
|       49 |  487 | `	zBuf = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator, pIn->nByte + 1);` |
|       49 |  488 | `	if( zBuf == 0 ){` |
|      ! 0 |  489 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|      ! 0 |  490 | `		return SXERR_ABORT;` |
|        - |  491 | `	}` |
|       49 |  492 | `	zDst = zBuf;` |
|       49 |  493 | `	z = pIn->zString;` |
|       49 |  494 | `	zEnd = z + pIn->nByte;` |
|      134 |  495 | `	while( z < zEnd ){` |
|       73 |  496 | `		const char *zLine = z;` |
|        - |  497 | `		sxu32 nLine;` |
|        - |  498 | `		int bEmpty;` |
|      815 |  499 | `		while( z < zEnd && z[0] != '\n' ){` |
|      745 |  500 | `			z++;` |
|        3 |  501 | `		}` |
|       73 |  502 | `		nLine = (sxu32)(z - zLine);` |
|       73 |  503 | `		bEmpty = (nLine == 0) \|\| (nLine == 1 && zLine[0] == '\r');` |
|       73 |  504 | `		if( !bEmpty ){` |
|        - |  505 | `			sxu32 i;` |
|       69 |  506 | `			if( nLine < nIndent ){` |
|      ! 0 |  507 | `				PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - |  508 | `					"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|      ! 0 |  509 | `					nIndent);` |
|      ! 0 |  510 | `				return SXERR_ABORT;` |
|        - |  511 | `			}` |
|      279 |  512 | `			for( i = 0; i < nIndent; i++ ){` |
|      221 |  513 | `				if( zLine[i] != zPrefix[i] ){` |
|       10 |  514 | `					unsigned char c = (unsigned char)zLine[i];` |
|       10 |  515 | `					if( c == ' ' \|\| c == '\t' ){` |
|        5 |  516 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - |  517 | `							"Invalid indentation - tabs and spaces cannot be mixed");` |
|        3 |  518 | `					}else{` |
|        7 |  519 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - |  520 | `							"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|        2 |  521 | `							nIndent);` |
|        - |  522 | `					}` |
|       10 |  523 | `					return SXERR_ABORT;` |
|        - |  524 | `				}` |
|      108 |  525 | `			}` |
|       60 |  526 | `			SyMemcpy((const void *)(zLine + nIndent), (void *)zDst, nLine - nIndent);` |
|       60 |  527 | `			zDst += nLine - nIndent;` |
|       34 |  528 | `		}else if( nLine == 1 ){` |
|        - |  529 | `			/* Preserve the stray '\r' on an otherwise empty line */` |
|      ! 0 |  530 | `			*zDst++ = '\r';` |
|      ! 0 |  531 | `		}` |
|       64 |  532 | `		if( z < zEnd ){` |
|       25 |  533 | `			*zDst++ = '\n';` |
|       25 |  534 | `			z++;` |
|       12 |  535 | `		}` |
|        2 |  536 | `	}` |
|       40 |  537 | `	pOut->zString = zBuf;` |
|       40 |  538 | `	pOut->nByte = (sxu32)(zDst - zBuf);` |
|       40 |  539 | `	return SXRET_OK;` |
|       69 |  540 | `}` |
|        - |  541 | `/*` |
|        - |  542 | ` * Compile a nowdoc string.` |
|        - |  543 | ` * According to the PHP language reference manual:` |
|        - |  544 | ` *` |
|        - |  545 | ` *  Nowdocs are to single-quoted strings what heredocs are to double-quoted strings.` |
|        - |  546 | ` *  A nowdoc is specified similarly to a heredoc, but no parsing is done inside a nowdoc.` |
|        - |  547 | ` *  The construct is ideal for embedding PHP code or other large blocks of text without the` |
|        - |  548 | ` *  need for escaping. It shares some features in common with the SGML <![CDATA[ ]]>` |
|        - |  549 | ` *  construct, in that it declares a block of text which is not for parsing.` |
|        - |  550 | ` *  A nowdoc is identified with the same <<< sequence used for heredocs, but the identifier` |
|        - |  551 | ` *  which follows is enclosed in single quotes, e.g. <<<'EOT'. All the rules for heredoc` |
|        - |  552 | ` *  identifiers also apply to nowdoc identifiers, especially those regarding the appearance` |
|        - |  553 | ` *  of the closing identifier.` |
|        - |  554 | ` */` |
|       52 |  555 | `PH7_PRIVATE sxi32 PH7_CompileNowDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  556 | `{` |
|        - |  557 | `	SyString sStripped;` |
|        - |  558 | `	SyString *pStr;` |
|        - |  559 | `	ph7_value *pObj;` |
|        - |  560 | `	sxu32 nIdx;` |
|        - |  561 | `	sxi32 rc;` |
|       57 |  562 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|       57 |  563 | `	if( rc != SXRET_OK ){` |
|        6 |  564 | `		return rc;` |
|        - |  565 | `	}` |
|       51 |  566 | `	pStr = &sStripped;` |
|       51 |  567 | `	nIdx = 0; /* Prevent compiler warning */` |
|       51 |  568 | `	if( pStr->nByte <= 0 ){` |
|        - |  569 | `		/* An empty nowdoc is the empty STRING, like '' -- loading NULL here made` |
|        - |  570 | `		 * strlen(<<<'EOD'EOD;) deprecation-warn about a null argument. */` |
|        7 |  571 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|        7 |  572 | `		return SXRET_OK;` |
|        - |  573 | `	}` |
|        - |  574 | `	/* Reserve a new constant */` |
|       45 |  575 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       45 |  576 | `	if( pObj == 0 ){` |
|      ! 0 |  577 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|      ! 0 |  578 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|      ! 0 |  579 | `		return SXERR_ABORT;` |
|        - |  580 | `	}` |
|        - |  581 | `	/* No processing is done here, simply a memcpy() operation */` |
|       45 |  582 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,pStr);` |
|        - |  583 | `	/* Emit the load constant instruction */` |
|       45 |  584 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        - |  585 | `	/* Node successfully compiled */` |
|       45 |  586 | `	return SXRET_OK;` |
|       31 |  587 | `}` |
|        - |  588 | `/*` |
|        - |  589 | ` * Process variable expression [i.e: "$var","${var}"] embedded in a double quoted/heredoc string.` |
|        - |  590 | ` * According to the PHP language reference manual` |
|        - |  591 | ` *   When a string is specified in double quotes or with heredoc,variables are parsed within it.` |
|        - |  592 | ` *  There are two types of syntax: a simple one and a complex one. The simple syntax is the most` |
|        - |  593 | ` *  common and convenient. It provides a way to embed a variable, an array value, or an object` |
|        - |  594 | ` *  property in a string with a minimum of effort.` |
|        - |  595 | ` *  Simple syntax` |
|        - |  596 | ` *   If a dollar sign ($) is encountered, the parser will greedily take as many tokens as possible` |
|        - |  597 | ` *   to form a valid variable name. Enclose the variable name in curly braces to explicitly specify` |
|        - |  598 | ` *   the end of the name.` |
|        - |  599 | ` *   Similarly, an array index or an object property can be parsed. With array indices, the closing` |
|        - |  600 | ` *   square bracket (]) marks the end of the index. The same rules apply to object properties` |
|        - |  601 | ` *   as to simple variables.` |
|        - |  602 | ` *  Complex (curly) syntax` |
|        - |  603 | ` *   This isn't called complex because the syntax is complex, but because it allows for the use` |
|        - |  604 | ` *   of complex expressions.` |
|        - |  605 | ` *   Any scalar variable, array element or object property with a string representation can be` |
|        - |  606 | ` *   included via this syntax. Simply write the expression the same way as it would appear outside` |
|        - |  607 | ` *   the string, and then wrap it in { and }. Since { can not be escaped, this syntax will only` |
|        - |  608 | ` *   be recognised when the $ immediately follows the {. Use {\$ to get a literal {$` |
|        - |  609 | ` */` |
|     2974 |  610 | `static sxi32 GenStateProcessStringExpression(` |
|        - |  611 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - |  612 | `	sxu32 nLine,         /* Line number */` |
|        - |  613 | `	const char *zIn,     /* Raw expression */` |
|        - |  614 | `	const char *zEnd     /* End of the expression */` |
|        - |  615 | `	)` |
|        5 |  616 | `{` |
|        - |  617 | `	SyToken *pTmpIn,*pTmpEnd;` |
|        - |  618 | `	SySet sToken;` |
|        - |  619 | `	sxi32 rc;` |
|        - |  620 | `	/* Initialize the token set */` |
|     2979 |  621 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|        - |  622 | `	/* Preallocate some slots */` |
|     2979 |  623 | `	SySetAlloc(&sToken,0x08);` |
|        - |  624 | `	/* Tokenize the text */` |
|     2979 |  625 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken,0);` |
|        - |  626 | `	/* Swap delimiter */` |
|     2979 |  627 | `	pTmpIn  = pGen->pIn;` |
|     2979 |  628 | `	pTmpEnd = pGen->pEnd;` |
|     2979 |  629 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|     2979 |  630 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|        - |  631 | ``	/* Compile the expression. An interpolated `"...$x..."` READS $x — php warns`` |
|        - |  632 | `	 * "Undefined variable $x" and substitutes the empty string — so ask for a` |
|        - |  633 | `	 * read-only load rather than letting the default vivify it silently. */` |
|     2979 |  634 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|        - |  635 | `	/* Restore token stream */` |
|     2979 |  636 | `	pGen->pIn  = pTmpIn;` |
|     2979 |  637 | `	pGen->pEnd = pTmpEnd;` |
|        - |  638 | `	/* Release the token set */` |
|     2979 |  639 | `	SySetRelease(&sToken);` |
|        - |  640 | `	/* Compilation result */` |
|     2979 |  641 | `	return rc;` |
|        5 |  642 | `}` |
|        - |  643 | `/*` |
|        - |  644 | ` * Line number of a POSITION inside the string body being compiled: the` |
|        - |  645 | ` * token's line plus every newline before it. php reports the offending` |
|        - |  646 | ` * construct's own line, not the string's opening line, so every diagnostic` |
|        - |  647 | ` * raised from inside a body -- an escape sequence, a malformed subscript --` |
|        - |  648 | ` * goes through here. A heredoc body starts on the line after the '<<<'` |
|        - |  649 | ` * marker, hence the +1.` |
|        - |  650 | ` */` |
|       40 |  651 | `static sxu32 GenStateStringEscLine(ph7_gen_state *pGen,const char *zPos,int bHeredoc)` |
|        4 |  652 | `{` |
|       44 |  653 | `	const char *z = pGen->pIn->sData.zString;` |
|       44 |  654 | `	sxu32 nLine = pGen->pIn->nLine + (bHeredoc ? 1 : 0);` |
|      180 |  655 | `	for( ; z < zPos ; z++ ){` |
|      140 |  656 | `		if( z[0] == '\n' ){` |
|      ! 0 |  657 | `			nLine++;` |
|      ! 0 |  658 | `		}` |
|       72 |  659 | `	}` |
|       44 |  660 | `	return nLine;` |
|        4 |  661 | `}` |
|        - |  662 | `/*` |
|        - |  663 | ` * TRUE when c can OPEN a php label — the same byte class the engine's identifier` |
|        - |  664 | ` * scanner uses (LEX_LABEL_START in lex.c): [a-zA-Z_\x80-\xff].` |
|        - |  665 | ` */` |
|        - |  666 | `#define GEN_STRING_LABEL_START(c) \` |
|        - |  667 | `	( (unsigned char)(c) >= 0x80 \|\| SyisAlpha(c) \|\| (c) == '_' )` |
|        - |  668 | `/*` |
|        - |  669 | ` * Advance *pz over a php LABEL — the name half of "$name" and of the "->name"` |
|        - |  670 | ` * accessor inside a double-quoted string or a heredoc body. php's label is` |
|        - |  671 | ` * [a-zA-Z_\x80-\xff][a-zA-Z0-9_\x80-\xff]*, a flat byte set: a multibyte name` |
|        - |  672 | ` * is consumed because every byte of it is >= 0x80, no UTF-8 decoding involved.` |
|        - |  673 | ` * Stops at *pz when the cursor is not on a label byte.` |
|        - |  674 | ` */` |
|     2936 |  675 | `static void GenStateSkipStringLabel(const char **pz,const char *zEnd)` |
|        5 |  676 | `{` |
|     2941 |  677 | `	const char *zIn = *pz;` |
|     8648 |  678 | `	while( zIn < zEnd` |
|    11429 |  679 | `		&& ((unsigned char)zIn[0] >= 0x80 \|\| SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_') ){` |
|     8493 |  680 | `		zIn++;` |
|        5 |  681 | `	}` |
|     2941 |  682 | `	*pz = zIn;` |
|     2941 |  683 | `}` |
|        - |  684 | `/*` |
|        - |  685 | ` * Scan one php INTEGER literal at z in the flavour php's simple-syntax subscript` |
|        - |  686 | ` * accepts: LNUM, HNUM (0x...), BNUM (0b...) or ONUM (0o...), each allowing '_'` |
|        - |  687 | ` * separators BETWEEN digits. There is no float and no exponent in this grammar --` |
|        - |  688 | ` * "$a[1.5]" and "$a[1e2]" are php parse errors. Returns the byte after the` |
|        - |  689 | ` * literal, or z itself when the cursor is not on one.` |
|        - |  690 | ` */` |
|       66 |  691 | `static const char * GenStateScanOffsetNumber(const char *z,const char *zEnd)` |
|        1 |  692 | `{` |
|       67 |  693 | `	const char *zStart = z;` |
|       67 |  694 | `	int base = 10;` |
|       67 |  695 | `	if( z >= zEnd \|\| !GenStateIsBaseDigit((unsigned char)z[0],10) ){` |
|      ! 0 |  696 | `		return z;` |
|        - |  697 | `	}` |
|       67 |  698 | `	if( z[0] == '0' && &z[1] < zEnd ){` |
|       23 |  699 | `		int b = 0;` |
|       23 |  700 | `		if( z[1] == 'x' \|\| z[1] == 'X' ){` |
|        7 |  701 | `			b = 16;` |
|       20 |  702 | `		}else if( z[1] == 'b' \|\| z[1] == 'B' ){` |
|        3 |  703 | `			b = 2;` |
|       16 |  704 | `		}else if( z[1] == 'o' \|\| z[1] == 'O' ){` |
|        3 |  705 | `			b = 8;` |
|        1 |  706 | `		}` |
|        - |  707 | `		/* A prefix with no digit behind it is not a literal: php then matches the` |
|        - |  708 | `		 * lone "0" and lexes the rest as a label ("$a[0x]" is a parse error). */` |
|       23 |  709 | `		if( b && &z[2] < zEnd && GenStateIsBaseDigit((unsigned char)z[2],b) ){` |
|        9 |  710 | `			base = b;` |
|        9 |  711 | `			z += 2;` |
|        4 |  712 | `		}` |
|       11 |  713 | `	}` |
|      345 |  714 | `	while( z < zEnd ){` |
|      293 |  715 | `		if( GenStateIsBaseDigit((unsigned char)z[0],base) ){` |
|      275 |  716 | `			z++;` |
|      275 |  717 | `			continue;` |
|        - |  718 | `		}` |
|       18 |  719 | `		if( z[0] == '_' && z > zStart && GenStateIsBaseDigit((unsigned char)z[-1],base)` |
|        9 |  720 | `			&& &z[1] < zEnd && GenStateIsBaseDigit((unsigned char)z[1],base) ){` |
|        5 |  721 | `			z += 2;` |
|        5 |  722 | `			continue;` |
|        - |  723 | `		}` |
|       15 |  724 | `		break;` |
|      ! 0 |  725 | `	}` |
|       67 |  726 | `	return z;` |
|       34 |  727 | `}` |
|        - |  728 | `/*` |
|        - |  729 | ` * TRUE when the digit run [z,zEnd) is php's CANONICAL spelling of an INTEGER` |
|        - |  730 | ` * offset: "0", or [1-9][0-9]* that fits a signed 64-bit int. php carries every` |
|        - |  731 | ` * other spelling -- leading zeros, a base prefix, '_' separators, a magnitude` |
|        - |  732 | ` * past the int range -- as the raw TEXT, i.e. a STRING key. (zend also spells` |
|        - |  733 | ` * out any 19-digit run, but its hashmap folds that straight back to an integer` |
|        - |  734 | ` * key, so the two agree on everything an array can observe.)` |
|        - |  735 | ` */` |
|       52 |  736 | `static int GenStateOffsetIsCanonicalInt(const char *z,const char *zEnd,int bNeg)` |
|        1 |  737 | `{` |
|       53 |  738 | `	sxu32 n = (sxu32)(zEnd - z);` |
|        - |  739 | `	sxu32 i;` |
|       53 |  740 | `	if( n < 1 ){` |
|      ! 0 |  741 | `		return FALSE;` |
|        - |  742 | `	}` |
|       53 |  743 | `	if( z[0] == '0' ){` |
|        - |  744 | `		/* "0" alone is the integer key 0; "-0", "00" and "007" are text */` |
|       31 |  745 | `		return n == 1 && !bNeg;` |
|        - |  746 | `	}` |
|      227 |  747 | `	for( i = 0 ; i < n ; ++i ){` |
|      207 |  748 | `		if( !GenStateIsBaseDigit((unsigned char)z[i],10) ){` |
|        3 |  749 | `			return FALSE;` |
|        - |  750 | `		}` |
|      103 |  751 | `	}` |
|        - |  752 | `	/* INT64_MAX bounds BOTH signs here, not INT64_MIN: the rewrite re-emits a` |
|        - |  753 | `	 * canonical offset as SOURCE, and no php expression can spell INT64_MIN as a` |
|        - |  754 | `	 * literal (the '-' is unary minus over an out-of-range literal, which` |
|        - |  755 | `	 * promotes to a float). "-9223372036854775808" therefore takes the string` |
|        - |  756 | `	 * path, where the hashmap's numeric-string rule folds it back to the integer` |
|        - |  757 | `	 * key -- and where an ArrayAccess offsetGet() receives php's own string. */` |
|       21 |  758 | `	if( n > 19 \|\| (n == 19 && SyMemcmp(z,"9223372036854775807",19) > 0) ){` |
|        7 |  759 | `		return FALSE;` |
|        - |  760 | `	}` |
|       15 |  761 | `	return TRUE;` |
|       27 |  762 | `}` |
|        - |  763 | `/*` |
|        - |  764 | ` * php's parse error for a malformed simple-syntax subscript. zBad points at the` |
|        - |  765 | ` * first byte php would refuse; iExpect picks which of php's "expecting" tails` |
|        - |  766 | ` * applies -- 1 after an otherwise good offset, 2 after a lone '-', 0 at the` |
|        - |  767 | ` * offset's start. Always returns SXERR_ABORT so the caller can just pass it on.` |
|        - |  768 | ` */` |
|       34 |  769 | `static sxi32 GenStateOffsetSyntaxError(ph7_gen_state *pGen,const char *zBad,const char *zEnd,int iExpect,int bHeredoc)` |
|        1 |  770 | `{` |
|        - |  771 | `	SyString sTok;` |
|       35 |  772 | `	sxu32 n = (sxu32)(zEnd - zBad);` |
|       35 |  773 | `	if( n < 1 ){` |
|        - |  774 | `		/* Empty offset: name the ']' that zEnd points at */` |
|        3 |  775 | `		n = 1;` |
|        1 |  776 | `	}` |
|       35 |  777 | `	if( n > 16 ){` |
|      ! 0 |  778 | `		n = 16;` |
|      ! 0 |  779 | `	}` |
|       35 |  780 | `	SyStringInitFromBuf(&sTok,zBad,n);` |
|       35 |  781 | `	if( iExpect == 1 ){` |
|       19 |  782 | `		PH7_GenCompileError(&(*pGen),E_PARSE,GenStateStringEscLine(&(*pGen),zBad,bHeredoc),` |
|        - |  783 | `			"syntax error, unexpected token \"%z\", expecting \"]\"",&sTok);` |
|       26 |  784 | `	}else if( iExpect == 2 ){` |
|        5 |  785 | `		PH7_GenCompileError(&(*pGen),E_PARSE,GenStateStringEscLine(&(*pGen),zBad,bHeredoc),` |
|        - |  786 | `			"syntax error, unexpected token \"%z\", expecting number",&sTok);` |
|        3 |  787 | `	}else{` |
|       13 |  788 | `		PH7_GenCompileError(&(*pGen),E_PARSE,GenStateStringEscLine(&(*pGen),zBad,bHeredoc),` |
|        - |  789 | `			"syntax error, unexpected token \"%z\", expecting \"-\" or identifier or variable or number",&sTok);` |
|        - |  790 | `	}` |
|       35 |  791 | `	return SXERR_ABORT;` |
|        1 |  792 | `}` |
|        - |  793 | `/*` |
|        - |  794 | ` * Compile the SUBSCRIPT of a simple-syntax "$name[offset]" interpolation:` |
|        - |  795 | ` * [zKey,zKeyEnd) is the raw text between the brackets, and the php-equivalent` |
|        - |  796 | ` * "[...]" source is appended to pOut.` |
|        - |  797 | ` *` |
|        - |  798 | `` * php does NOT parse this as an expression. zend's `encaps_var_offset` grammar`` |
|        - |  799 | ` * admits exactly four things and nothing else -- a bare LABEL (always the STRING` |
|        - |  800 | ` * key, never a constant), an integer literal, '-' plus an integer literal, or a` |
|        - |  801 | ` * "$name" -- and only a canonical decimal is an INTEGER key. PH7 handed the text` |
|        - |  802 | ` * to the expression compiler, which read every integer SPELLING as a number and` |
|        - |  803 | ` * accepted shapes php rejects outright.` |
|        - |  804 | ` */` |
|      112 |  805 | `static sxi32 GenStateCompileStringOffset(` |
|        - |  806 | `	ph7_gen_state *pGen,` |
|        - |  807 | `	const char *zKey,` |
|        - |  808 | `	const char *zKeyEnd,` |
|        - |  809 | `	SyBlob *pOut,` |
|        - |  810 | `	int bHeredoc` |
|        - |  811 | `	)` |
|        3 |  812 | `{` |
|      115 |  813 | `	const char *z = zKey;` |
|      115 |  814 | `	int bNeg = 0;` |
|      115 |  815 | `	if( z >= zKeyEnd ){` |
|        3 |  816 | `		return GenStateOffsetSyntaxError(&(*pGen),z,zKeyEnd,0,bHeredoc);` |
|        - |  817 | `	}` |
|      113 |  818 | `	if( z[0] == '$' ){` |
|        - |  819 | `		/* "$name" -- the one offset php actually EVALUATES; pass it through */` |
|        9 |  820 | `		const char *zName = &z[1];` |
|        9 |  821 | `		if( zName >= zKeyEnd \|\| !GEN_STRING_LABEL_START(zName[0]) ){` |
|        3 |  822 | `			return GenStateOffsetSyntaxError(&(*pGen),z,zKeyEnd,0,bHeredoc);` |
|        - |  823 | `		}` |
|        7 |  824 | `		z = zName;` |
|        7 |  825 | `		GenStateSkipStringLabel(&z,zKeyEnd);` |
|        7 |  826 | `		if( z != zKeyEnd ){` |
|        3 |  827 | `			return GenStateOffsetSyntaxError(&(*pGen),z,zKeyEnd,1,bHeredoc);` |
|        - |  828 | `		}` |
|        5 |  829 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 |  830 | `		SyBlobAppend(pOut,zKey,(sxu32)(zKeyEnd - zKey));` |
|        5 |  831 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        5 |  832 | `		return SXRET_OK;` |
|        - |  833 | `	}` |
|      105 |  834 | `	if( z[0] == '-' ){` |
|       15 |  835 | `		bNeg = 1;` |
|       15 |  836 | `		z++;` |
|        7 |  837 | `	}` |
|      105 |  838 | `	if( z < zKeyEnd && GenStateIsBaseDigit((unsigned char)z[0],10) ){` |
|       67 |  839 | `		const char *zNum = z;` |
|       67 |  840 | `		z = GenStateScanOffsetNumber(z,zKeyEnd);` |
|       67 |  841 | `		if( z != zKeyEnd ){` |
|       15 |  842 | `			return GenStateOffsetSyntaxError(&(*pGen),z,zKeyEnd,1,bHeredoc);` |
|        - |  843 | `		}` |
|        - |  844 | `		/* "-0" is php's string key "-0", not the integer 0: zend negates a LONG` |
|        - |  845 | `		 * num-string but spells a ZERO one back out as text. */` |
|       53 |  846 | `		if( GenStateOffsetIsCanonicalInt(zNum,zKeyEnd,bNeg) ){` |
|       25 |  847 | `			SyBlobAppend(pOut,"[",sizeof(char));` |
|       25 |  848 | `			SyBlobAppend(pOut,zKey,(sxu32)(zKeyEnd - zKey));` |
|       25 |  849 | `			SyBlobAppend(pOut,"]",sizeof(char));` |
|       13 |  850 | `		}else{` |
|       29 |  851 | `			SyBlobAppend(pOut,"['",sizeof(char)*2);` |
|       29 |  852 | `			SyBlobAppend(pOut,zKey,(sxu32)(zKeyEnd - zKey));` |
|       29 |  853 | `			SyBlobAppend(pOut,"']",sizeof(char)*2);` |
|        - |  854 | `		}` |
|       53 |  855 | `		return SXRET_OK;` |
|        - |  856 | `	}` |
|       39 |  857 | `	if( bNeg ){` |
|        - |  858 | `		/* php's '-' takes a NUMBER and nothing else */` |
|        5 |  859 | `		return GenStateOffsetSyntaxError(&(*pGen),z,zKeyEnd,2,bHeredoc);` |
|        - |  860 | `	}` |
|       35 |  861 | `	if( GEN_STRING_LABEL_START(z[0]) ){` |
|       27 |  862 | `		GenStateSkipStringLabel(&z,zKeyEnd);` |
|       27 |  863 | `		if( z != zKeyEnd ){` |
|        3 |  864 | `			return GenStateOffsetSyntaxError(&(*pGen),z,zKeyEnd,1,bHeredoc);` |
|        - |  865 | `		}` |
|        - |  866 | `		/* A bare word is the STRING key, never a constant */` |
|       25 |  867 | `		SyBlobAppend(pOut,"['",sizeof(char)*2);` |
|       25 |  868 | `		SyBlobAppend(pOut,zKey,(sxu32)(zKeyEnd - zKey));` |
|       25 |  869 | `		SyBlobAppend(pOut,"']",sizeof(char)*2);` |
|       25 |  870 | `		return SXRET_OK;` |
|        - |  871 | `	}` |
|        9 |  872 | `	return GenStateOffsetSyntaxError(&(*pGen),z,zKeyEnd,0,bHeredoc);` |
|       59 |  873 | `}` |
|        - |  874 | `/*` |
|        - |  875 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|        - |  876 | ` */` |
|   150118 |  877 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|        5 |  878 | `{` |
|        - |  879 | `	ph7_value *pConstObj;` |
|   150123 |  880 | `	sxu32 nIdx = 0;` |
|        - |  881 | `	/* Reserve a new constant */` |
|   150123 |  882 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   150123 |  883 | `	if( pConstObj == 0 ){` |
|      ! 0 |  884 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|      ! 0 |  885 | `		return 0;` |
|        - |  886 | `	}` |
|   150123 |  887 | `	(*pCount)++;` |
|   150123 |  888 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|        - |  889 | `	/* Emit the load constant instruction */` |
|   150123 |  890 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   150123 |  891 | `	return pConstObj;` |
|    75064 |  892 | `}` |
|        - |  893 | `/*` |
|        - |  894 | ` * Compile a double quoted/heredoc string.` |
|        - |  895 | ` * According to the PHP language reference manual` |
|        - |  896 | ` * Heredoc` |
|        - |  897 | ` *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier` |
|        - |  898 | ` *  is provided, then a newline. The string itself follows, and then the same identifier again` |
|        - |  899 | ` *  to close the quotation.` |
|        - |  900 | ` *  The closing identifier must begin in the first column of the line. Also, the identifier must` |
|        - |  901 | ` *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric` |
|        - |  902 | ` *  characters and underscores, and must start with a non-digit character or underscore.` |
|        - |  903 | ` *  Warning` |
|        - |  904 | ` *  It is very important to note that the line with the closing identifier must contain` |
|        - |  905 | ` *  no other characters, except possibly a semicolon (;). That means especially that the identifier` |
|        - |  906 | ` *  may not be indented, and there may not be any spaces or tabs before or after the semicolon.` |
|        - |  907 | ` *  It's also important to realize that the first character before the closing identifier must` |
|        - |  908 | ` *  be a newline as defined by the local operating system. This is \n on UNIX systems, including Mac OS X.` |
|        - |  909 | ` *  The closing delimiter (possibly followed by a semicolon) must also be followed by a newline.` |
|        - |  910 | ` *  If this rule is broken and the closing identifier is not "clean", it will not be considered a closing` |
|        - |  911 | ` *  identifier, and PHP will continue looking for one. If a proper closing identifier is not found before` |
|        - |  912 | ` *  the end of the current file, a parse error will result at the last line.` |
|        - |  913 | ` *  Heredocs can not be used for initializing class properties.` |
|        - |  914 | ` * Double quoted` |
|        - |  915 | ` *  If the string is enclosed in double-quotes ("), PHP will interpret more escape sequences for special characters:` |
|        - |  916 | ` *  Escaped characters Sequence 	Meaning` |
|        - |  917 | ` *  \n linefeed (LF or 0x0A (10) in ASCII)` |
|        - |  918 | ` *  \r carriage return (CR or 0x0D (13) in ASCII)` |
|        - |  919 | ` *  \t horizontal tab (HT or 0x09 (9) in ASCII)` |
|        - |  920 | ` *  \v vertical tab (VT or 0x0B (11) in ASCII)` |
|        - |  921 | ` *  \e escape (ESC or 0x1B (27) in ASCII)` |
|        - |  922 | ` *  \f form feed (FF or 0x0C (12) in ASCII)` |
|        - |  923 | ` *  \\ backslash` |
|        - |  924 | ` *  \$ dollar sign` |
|        - |  925 | ` *  \" double-quote` |
|        - |  926 | ` *  \[0-7]{1,3} 	the sequence of characters matching the regular expression is a character in octal notation,` |
|        - |  927 | ` *      which silently overflows to fit in a byte (e.g. "\400" === "\000")` |
|        - |  928 | ` *  \x[0-9A-Fa-f]{1,2} 	the sequence of characters matching the regular expression is a character in hexadecimal notation` |
|        - |  929 | ` *  \u{[0-9A-Fa-f]+} 	the sequence of characters matching the regular expression is a Unicode codepoint,` |
|        - |  930 | ` *      which will be output to the string as that codepoint's UTF-8 representation` |
|        - |  931 | ` * As in single quoted strings, escaping any other character will result in the backslash being printed too.` |
|        - |  932 | ` * (The PH7-ism "\oNNN" octal form is gone: a literal "\o" now round-trips like php 8.)` |
|        - |  933 | ` * The most important feature of double-quoted strings is the fact that variable names will be expanded.` |
|        - |  934 | ` * See string parsing for details.` |
|        - |  935 | ` */` |
|        - |  936 | `/* bHeredoc: php strips the backslash from '\"' only when '"' is the active` |
|        - |  937 | ` * quote character; a heredoc has none, so '\"' stays verbatim there. */` |
|   148792 |  938 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen,int bHeredoc)` |
|        5 |  939 | `{` |
|   148797 |  940 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|        - |  941 | `	const char *zIn,*zCur,*zEnd;` |
|   148797 |  942 | `	ph7_value *pObj = 0;` |
|        - |  943 | `	sxi32 iCons;` |
|        - |  944 | `	sxi32 nInterp;   /* how many of iCons came from an interpolated EXPRESSION */` |
|        - |  945 | `	sxi32 rc;` |
|        - |  946 | `	/* Delimit the string */` |
|   148797 |  947 | `	zIn  = pStr->zString;` |
|   148797 |  948 | `	zEnd = &zIn[pStr->nByte];` |
|   148797 |  949 | `	if( zIn >= zEnd ){` |
|        - |  950 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|        - |  951 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|        - |  952 | `		 * literal table from growing when many "" literals appear in the source.` |
|        - |  953 | `		 */` |
|      561 |  954 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|      561 |  955 | `		return SXRET_OK;` |
|        - |  956 | `	}` |
|   148241 |  957 | `	zCur = 0;` |
|        - |  958 | `	/* Compile the node */` |
|   148241 |  959 | `	iCons = 0;` |
|   148241 |  960 | `	nInterp = 0;` |
|    75566 |  961 | `	for(;;){` |
|   203643 |  962 | `		zCur = zIn;` |
|  1981997 |  963 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  1781369 |  964 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|       92 |  965 | `				break;` |
|  1781197 |  966 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|     2842 |  967 | `				(GEN_STRING_LABEL_START(zIn[1]) \|\| zIn[1] == '{') ){` |
|     1421 |  968 | `					break;` |
|        - |  969 | `			}` |
|  1778359 |  970 | `			zIn++;` |
|        5 |  971 | `		}` |
|   203643 |  972 | `		if( zIn > zCur ){` |
|   114371 |  973 | `			if( pObj == 0 ){` |
|   113453 |  974 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|   113453 |  975 | `				if( pObj == 0 ){` |
|      ! 0 |  976 | `					return SXERR_ABORT;` |
|        - |  977 | `				}` |
|    56724 |  978 | `			}` |
|   114371 |  979 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    57183 |  980 | `		}` |
|   203643 |  981 | `		if( zIn >= zEnd ){` |
|   148203 |  982 | `			break;` |
|        - |  983 | `		}` |
|    55445 |  984 | `		if( zIn[0] == '\\' ){` |
|    52435 |  985 | `			const char *zPtr = 0;` |
|        - |  986 | `			sxu32 n;` |
|    52435 |  987 | `			zIn++;` |
|    52435 |  988 | `			if( pObj == 0 ){` |
|    36675 |  989 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    36675 |  990 | `				if( pObj == 0 ){` |
|      ! 0 |  991 | `					return SXERR_ABORT;` |
|        - |  992 | `				}` |
|    18335 |  993 | `			}` |
|    52435 |  994 | `			if( zIn >= zEnd ){` |
|        - |  995 | `				/* Lone backslash at the very end of the body: php keeps it */` |
|        3 |  996 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|        3 |  997 | `				break;` |
|        - |  998 | `			}` |
|    52433 |  999 | `			n = sizeof(char); /* size of conversion */` |
|    52433 | 1000 | `			switch( zIn[0] ){` |
|       64 | 1001 | `			case '$':` |
|        - | 1002 | `				/* Dollar sign */` |
|      133 | 1003 | `				PH7_MemObjStringAppend(pObj,"$",sizeof(char));` |
|      133 | 1004 | `				break;` |
|       79 | 1005 | `			case '\\':` |
|        - | 1006 | `				/* A literal backslash */` |
|      163 | 1007 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|      163 | 1008 | `				break;` |
|        1 | 1009 | `			case 'e':` |
|        - | 1010 | `				/* Escape (ESC) ASCII code 27 */` |
|        3 | 1011 | `				PH7_MemObjStringAppend(pObj,"\x1b",sizeof(char));` |
|        3 | 1012 | `				break;` |
|        4 | 1013 | `			case 'f':` |
|        - | 1014 | `				/* Form-feed (FF)[ctrl+l] ASCII code 12 */` |
|        9 | 1015 | `				PH7_MemObjStringAppend(pObj,"\f",sizeof(char));` |
|        9 | 1016 | `				break;` |
|    23080 | 1017 | `			case 'n':` |
|        - | 1018 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|    46165 | 1019 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|    46165 | 1020 | `				break;` |
|       38 | 1021 | `			case 'r':` |
|        - | 1022 | `				/* Carriage return (CR)[ctrl+m] ASCII code 13 */` |
|       81 | 1023 | `				PH7_MemObjStringAppend(pObj,"\r",sizeof(char));` |
|       81 | 1024 | `				break;` |
|     2302 | 1025 | `			case 't':` |
|        - | 1026 | `				/* Horizontal tab (HT)[ctrl+i] ASCII code 9 */` |
|     4609 | 1027 | `				PH7_MemObjStringAppend(pObj,"\t",sizeof(char));` |
|     4609 | 1028 | `				break;` |
|        3 | 1029 | `			case 'v':` |
|        - | 1030 | `				/* Vertical tab(VT)[ctrl+k] ASCII code 11 */` |
|        7 | 1031 | `				PH7_MemObjStringAppend(pObj,"\v",sizeof(char));` |
|        7 | 1032 | `				break;` |
|      195 | 1033 | `			case '"':` |
|      395 | 1034 | `				if( bHeredoc ){` |
|        - | 1035 | `					/* No active quote char in a heredoc: php keeps \" verbatim */` |
|        5 | 1036 | `					PH7_MemObjStringAppend(pObj,"\\\"",sizeof(char)*2);` |
|        3 | 1037 | `				}else{` |
|        - | 1038 | `					/* Double quote */` |
|      391 | 1039 | `					PH7_MemObjStringAppend(pObj,"\"",sizeof(char));` |
|        - | 1040 | `				}` |
|      395 | 1041 | `				break;` |
|       27 | 1042 | `			case '0': case '1': case '2': case '3':` |
|        - | 1043 | `			case '4': case '5': case '6': case '7': {` |
|        - | 1044 | `				/* \[0-7]{1,3}: a character in octal notation. A value above \377` |
|        - | 1045 | `				 * warns and wraps to the low byte, matching php 8. */` |
|       57 | 1046 | `				int c = 0;` |
|        - | 1047 | `				char cOut;` |
|      157 | 1048 | `				for( zPtr = zIn ; zPtr < &zIn[3*sizeof(char)] ; zPtr++ ){` |
|      135 | 1049 | `					if( zPtr >= zEnd \|\| zPtr[0] < '0' \|\| zPtr[0] > '7' ){` |
|       18 | 1050 | `						break;` |
|        - | 1051 | `					}` |
|      103 | 1052 | `					c = c * 8 + (zPtr[0] - '0');` |
|       53 | 1053 | `				}` |
|       57 | 1054 | `				if( c > 0xFF ){` |
|        - | 1055 | `					SyString sSeq;` |
|        3 | 1056 | `					SyStringInitFromBuf(&sSeq,zIn,(sxu32)(zPtr-zIn));` |
|        3 | 1057 | `					PH7_GenCompileError(&(*pGen),E_WARNING,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|        - | 1058 | `						"Octal escape sequence overflow \\%z is greater than \\377",&sSeq);` |
|        3 | 1059 | `					c &= 0xFF;` |
|        1 | 1060 | `				}` |
|       57 | 1061 | `				cOut = (char)c; /* value byte, independent of host endianness */` |
|       57 | 1062 | `				PH7_MemObjStringAppend(pObj,&cOut,sizeof(char));` |
|       57 | 1063 | `				n = (sxu32)(zPtr-zIn);` |
|       57 | 1064 | `				break;` |
|        - | 1065 | `			}` |
|      390 | 1066 | `			case 'x':` |
|     1172 | 1067 | `				if( &zIn[1] < zEnd && SyisHex((unsigned char)zIn[1]) ){` |
|        - | 1068 | `					/* \x[0-9A-Fa-f]{1,2}: a character in hexadecimal notation */` |
|      780 | 1069 | `					int c = SyHexToint(zIn[1]);` |
|        - | 1070 | `					char cOut;` |
|      780 | 1071 | `					n += sizeof(char);` |
|      780 | 1072 | `					if( &zIn[2] < zEnd && SyisHex((unsigned char)zIn[2]) ){` |
|      776 | 1073 | `						c = (c << 4) + SyHexToint(zIn[2]);` |
|      776 | 1074 | `						n += sizeof(char);` |
|      386 | 1075 | `					}` |
|      780 | 1076 | `					cOut = (char)c; /* value byte, independent of host endianness */` |
|      780 | 1077 | `					PH7_MemObjStringAppend(pObj,&cOut,sizeof(char));` |
|      392 | 1078 | `				}else{` |
|        - | 1079 | `					/* Not an escape: keep the backslash, as php does */` |
|        5 | 1080 | `					PH7_MemObjStringAppend(pObj,"\\x",sizeof(char)*2);` |
|        - | 1081 | `				}` |
|      784 | 1082 | `				break;` |
|       16 | 1083 | `			case 'u':` |
|       32 | 1084 | `				if( &zIn[1] < zEnd && zIn[1] == '{'` |
|       43 | 1085 | `				 && !(&zIn[2] < zEnd && zIn[2] == '$') ){` |
|        - | 1086 | `					/* \u{codepoint}: UTF-8 encoding of the given codepoint (php 7+).` |
|        - | 1087 | `					 * php encodes surrogates verbatim, so the only invalid value` |
|        - | 1088 | `					 * is > U+10FFFF; malformed/empty braces are a compile error.` |
|        - | 1089 | `					 * "\u{$..." is excluded above: php treats it as a literal \u` |
|        - | 1090 | `					 * followed by {$...} curly interpolation. */` |
|       29 | 1091 | `					sxu32 nCp = 0;` |
|       29 | 1092 | `					zPtr = &zIn[2];` |
|      123 | 1093 | `					while( zPtr < zEnd && SyisHex((unsigned char)zPtr[0]) ){` |
|       96 | 1094 | `						if( nCp <= 0x10FFFF ){` |
|        - | 1095 | `							/* stop accumulating once out of range: keeps a long` |
|        - | 1096 | `							 * digit run from wrapping sxu32 */` |
|       96 | 1097 | `							nCp = nCp * 16 + (sxu32)SyHexToint(zPtr[0]);` |
|       47 | 1098 | `						}` |
|       96 | 1099 | `						zPtr++;` |
|        2 | 1100 | `					}` |
|       29 | 1101 | `					if( zPtr == &zIn[2] \|\| zPtr >= zEnd \|\| zPtr[0] != '}' ){` |
|        - | 1102 | `						/* Error recorded (nErr>0 fails the whole compile); consume the` |
|        - | 1103 | `						 * malformed sequence so later errors are still reported. */` |
|        3 | 1104 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|        - | 1105 | `							"Invalid UTF-8 codepoint escape sequence");` |
|        3 | 1106 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 1107 | `							return SXERR_ABORT;` |
|        - | 1108 | `						}` |
|        3 | 1109 | `						n = (sxu32)(zPtr-zIn);` |
|        3 | 1110 | `						if( zPtr < zEnd && zPtr[0] == '}' ){` |
|        3 | 1111 | `							n += sizeof(char);` |
|        1 | 1112 | `						}` |
|        3 | 1113 | `						break;` |
|        - | 1114 | `					}` |
|       26 | 1115 | `					n = (sxu32)(&zPtr[1]-zIn); /* 'u{...}' incl. closing brace */` |
|       26 | 1116 | `					if( nCp > 0x10FFFF ){` |
|        3 | 1117 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|        - | 1118 | `							"Invalid UTF-8 codepoint escape sequence: Codepoint too large");` |
|        3 | 1119 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 1120 | `							return SXERR_ABORT;` |
|        - | 1121 | `						}` |
|        3 | 1122 | `						break;` |
|        - | 1123 | `					}` |
|        - | 1124 | `					{` |
|        - | 1125 | `						char zUtf[4];` |
|       23 | 1126 | `						sxu8 *zOut = (sxu8 *)zUtf;` |
|       23 | 1127 | `						SX_WRITE_UTF8(zOut,nCp);` |
|       23 | 1128 | `						PH7_MemObjStringAppend(pObj,zUtf,(sxu32)(zOut-(sxu8 *)zUtf));` |
|        - | 1129 | `					}` |
|       12 | 1130 | `				}else{` |
|        - | 1131 | `					/* Not an escape: keep the backslash, as php does */` |
|        7 | 1132 | `					PH7_MemObjStringAppend(pObj,"\\u",sizeof(char)*2);` |
|        - | 1133 | `				}` |
|       29 | 1134 | `				break;` |
|       15 | 1135 | `			default:` |
|        - | 1136 | `				/* Unrecognized escape: keep the backslash, as php does.` |
|        - | 1137 | `				 * zIn[-1] is the backslash itself, so both bytes are contiguous` |
|        - | 1138 | `				 * in the source buffer — one batched append. */` |
|       31 | 1139 | `				PH7_MemObjStringAppend(pObj,&zIn[-1],sizeof(char)*2);` |
|       30 | 1140 | `				break;` |
|        - | 1141 | `			}` |
|        - | 1142 | `			/* Advance the stream cursor */` |
|    52433 | 1143 | `			zIn += n;` |
|    52433 | 1144 | `			continue;` |
|        - | 1145 | `		}` |
|     3015 | 1146 | `		if( zIn[0] == '{' ){` |
|        - | 1147 | `			/* Curly syntax */` |
|        - | 1148 | `			const char *zExpr;` |
|      181 | 1149 | `			sxi32 iNest = 1;` |
|      181 | 1150 | `			zIn++;` |
|      181 | 1151 | `			zExpr = zIn;` |
|        - | 1152 | `			/* Synchronize with the next closing curly braces */` |
|     1607 | 1153 | `			while( zIn < zEnd ){` |
|     1607 | 1154 | `				if( zIn[0] == '{' ){` |
|        - | 1155 | `					/* Increment nesting level */` |
|        3 | 1156 | `					iNest++;` |
|     1606 | 1157 | `				}else if(zIn[0] == '}' ){` |
|        - | 1158 | `					/* Decrement nesting level */` |
|      183 | 1159 | `					iNest--;` |
|      183 | 1160 | `					if( iNest <= 0 ){` |
|      181 | 1161 | `						break;` |
|        - | 1162 | `					}` |
|        1 | 1163 | `				}` |
|     1429 | 1164 | `				zIn++;` |
|        3 | 1165 | `			}` |
|        - | 1166 | `			/* Process the expression */` |
|      181 | 1167 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|      181 | 1168 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1169 | `				return SXERR_ABORT;` |
|        - | 1170 | `			}` |
|      181 | 1171 | `			if( rc != SXERR_EMPTY ){` |
|      181 | 1172 | `				++iCons;` |
|      181 | 1173 | `				++nInterp;` |
|       89 | 1174 | `			}` |
|      181 | 1175 | `			if( zIn < zEnd ){` |
|        - | 1176 | `				/* Jump the trailing curly */` |
|      181 | 1177 | `				zIn++;` |
|       89 | 1178 | `			}` |
|       92 | 1179 | `		}else{` |
|        - | 1180 | `			/*` |
|        - | 1181 | `			 * Simple syntax. php's simple "$var…" form is a LEXER rule, not an` |
|        - | 1182 | `			 * expression: it takes the variable name plus EXACTLY ONE accessor —` |
|        - | 1183 | `			 * "$var", "$var[offset]" or "$var->prop" — and stops there. Everything` |
|        - | 1184 | `			 * past that one accessor is literal text: a second subscript` |
|        - | 1185 | `			 * ("$o->p[0]" is the property then a literal "[0]"), a second arrow` |
|        - | 1186 | `			 * ("$o->p->q" is "$o->p" then a literal "->q"), any "::" at all` |
|        - | 1187 | `			 * ("$c::C" is the VALUE of $c then a literal "::C", never a class` |
|        - | 1188 | `			 * constant), and any "{…}" ("$x{'a'}" is $x then literal). Only the` |
|        - | 1189 | `			 * complex "{$expr}" form reaches those, and it is handled above.` |
|        - | 1190 | `			 *` |
|        - | 1191 | `			 * PHL used to loop here, greedily chaining accessors, so those four` |
|        - | 1192 | `			 * shapes silently answered something else than php on VALID source.` |
|        - | 1193 | `			 */` |
|     2837 | 1194 | `			const char *zExpr = zIn;` |
|     2837 | 1195 | `			int bSubscript = 0;` |
|        - | 1196 | `			/*` |
|        - | 1197 | `			 * "${...}" string interpolation (every form: ${name}, ${expr}, ${$x}) was` |
|        - | 1198 | `			 * DEPRECATED by php 8.2 in favor of the canonical "{$...}". PHL targets php's` |
|        - | 1199 | `			 * *non-deprecated* surface, so it is a hard parse error here — never silently` |
|        - | 1200 | `			 * rewritten. The canonical "{$var}" reaches this compiler by a different path` |
|        - | 1201 | `			 * and is unaffected. Checked before the scan: '{' is not an accessor, so the` |
|        - | 1202 | `			 * cursor would otherwise stop on the '$' and read the brace as literal text.` |
|        - | 1203 | `			 */` |
|     2837 | 1204 | `			if( &zIn[1] < zEnd && zIn[0] == '$' && zIn[1] == '{' ){` |
|        3 | 1205 | `				PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|        - | 1206 | `					"syntax error, \"${\" string interpolation was removed in php 8.2, use \"{$...}\" instead");` |
|        3 | 1207 | `				return SXERR_ABORT;` |
|        - | 1208 | `			}` |
|        - | 1209 | `			/* Jump leading dollars */` |
|     5665 | 1210 | `			while( zIn < zEnd && zIn[0] == '$' ){` |
|     2835 | 1211 | `				zIn++;` |
|        5 | 1212 | `			}` |
|        - | 1213 | `			/* Variable name */` |
|     2835 | 1214 | `			GenStateSkipStringLabel(&zIn,zEnd);` |
|        - | 1215 | `			/* …then at most ONE accessor */` |
|     2891 | 1216 | `			if( zIn < zEnd && zIn[0] == '[' ){` |
|      115 | 1217 | `				sxi32 iSquare = 1;` |
|      115 | 1218 | `				bSubscript = 1;` |
|      115 | 1219 | `				zIn++;` |
|      569 | 1220 | `				while( zIn < zEnd ){` |
|      567 | 1221 | `					if( zIn[0] == '[' ){` |
|        3 | 1222 | `						iSquare++;` |
|      566 | 1223 | `					}else if (zIn[0] == ']' ){` |
|      115 | 1224 | `						iSquare--;` |
|      115 | 1225 | `						if( iSquare <= 0 ){` |
|      113 | 1226 | `							break;` |
|        - | 1227 | `						}` |
|        1 | 1228 | `					}` |
|      457 | 1229 | `					zIn++;` |
|        3 | 1230 | `				}` |
|      115 | 1231 | `				if( zIn < zEnd ){` |
|      113 | 1232 | `					zIn++;` |
|       55 | 1233 | `				}` |
|     2777 | 1234 | `			}else if( &zIn[2] < zEnd && zIn[0] == '-' && zIn[1] == '>'` |
|      110 | 1235 | `				&& GEN_STRING_LABEL_START(zIn[2]) ){` |
|        - | 1236 | `				/* Member access operator '->'. php takes it only when a LABEL` |
|        - | 1237 | `				 * follows; with anything else -- a digit, a space, a '{', the end` |
|        - | 1238 | `				 * of the body -- the arrow is literal TEXT and the interpolation is` |
|        - | 1239 | `				 * just the variable. PHL swallowed the bare '->' and handed the` |
|        - | 1240 | `				 * compiler a dangling "$o->", fatalling` |
|        - | 1241 | `				 * "'->': Missing/Invalid member name" on source php RUNS. */` |
|       79 | 1242 | `				zIn += 2;` |
|       79 | 1243 | `				GenStateSkipStringLabel(&zIn,zEnd);` |
|       38 | 1244 | `			}` |
|        - | 1245 | `			/*` |
|        - | 1246 | `			 * "$a[offset]" -- php parses a simple-syntax subscript with its OWN tiny` |
|        - | 1247 | ``			 * grammar (zend's `encaps_var_offset`), never as an expression, so rewrite`` |
|        - | 1248 | `			 * it into the equivalent php source and hand THAT to the compiler. PH7 fed` |
|        - | 1249 | `			 * the raw text straight in, which read every integer SPELLING as a number` |
|        - | 1250 | `			 * ("$a[007]" / "$a[0x1A]" / "$a[1_000]" / "$a[-0]" answered the integer` |
|        - | 1251 | `			 * keys 7/26/1000/0 where php reads the STRING keys "007"/"0x1A"/"1_000"/` |
|        - | 1252 | `			 * "-0"), read a non-ASCII bare word as a CONSTANT ("$a[\xc3\xa9]" raised` |
|        - | 1253 | `			 * "Undefined constant"), and quietly accepted every shape php rejects` |
|        - | 1254 | `			 * ("$a[ 0]", "$a[0 ]", "$a['x']", "$a[+1]", "$a[-$k]", "$a[[]", "$a[]").` |
|        - | 1255 | `			 */` |
|     2835 | 1256 | `			if( bSubscript ){` |
|      115 | 1257 | `				const char *zBr = zExpr;` |
|        - | 1258 | `				SyBlob sSub;` |
|      357 | 1259 | `				while( zBr < zIn && zBr[0] != '[' ){` |
|      245 | 1260 | `					zBr++;` |
|        3 | 1261 | `				}` |
|      115 | 1262 | `				if( zIn <= zBr \|\| zIn[-1] != ']' ){` |
|        - | 1263 | `					/* Unterminated: the body ended inside the brackets. php names the` |
|        - | 1264 | `					  * closing quote it reached instead; there is no offending TOKEN to` |
|        - | 1265 | `					  * quote here, and zIn is one past the body, so never read it. */` |
|      ! 0 | 1266 | `					PH7_GenCompileError(&(*pGen),E_PARSE,GenStateStringEscLine(&(*pGen),zBr,bHeredoc),` |
|        - | 1267 | `						"syntax error, unexpected end of string, expecting \"-\" or identifier or variable or number");` |
|      ! 0 | 1268 | `					return SXERR_ABORT;` |
|        - | 1269 | `				}` |
|      115 | 1270 | `				SyBlobInit(&sSub,&pGen->pVm->sAllocator);` |
|      115 | 1271 | `				SyBlobAppend(&sSub,zExpr,(sxu32)(zBr - zExpr));` |
|      115 | 1272 | `				rc = GenStateCompileStringOffset(&(*pGen),&zBr[1],&zIn[-1],&sSub,bHeredoc);` |
|      115 | 1273 | `				if( rc != SXRET_OK ){` |
|       35 | 1274 | `					SyBlobRelease(&sSub);` |
|       35 | 1275 | `					return SXERR_ABORT;` |
|        - | 1276 | `				}` |
|      120 | 1277 | `				rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,` |
|       78 | 1278 | `					(const char *)SyBlobData(&sSub),` |
|       78 | 1279 | `					(const char *)SyBlobData(&sSub) + SyBlobLength(&sSub));` |
|       81 | 1280 | `				SyBlobRelease(&sSub);` |
|       81 | 1281 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1282 | `					return SXERR_ABORT;` |
|        - | 1283 | `				}` |
|       81 | 1284 | `				if( rc != SXERR_EMPTY ){` |
|       81 | 1285 | `					++iCons;` |
|       81 | 1286 | `					++nInterp;` |
|       39 | 1287 | `				}` |
|       81 | 1288 | `				pObj = 0;` |
|       81 | 1289 | `				continue;` |
|        - | 1290 | `			}` |
|        - | 1291 | `			/* Process the expression */` |
|     2723 | 1292 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|     2723 | 1293 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1294 | `				return SXERR_ABORT;` |
|        - | 1295 | `			}` |
|     2723 | 1296 | `			if( rc != SXERR_EMPTY ){` |
|     2723 | 1297 | `				++iCons;` |
|     2723 | 1298 | `				++nInterp;` |
|     1359 | 1299 | `			}` |
|        - | 1300 | `		}` |
|        - | 1301 | `		/* Invalidate the previously used constant */` |
|     2901 | 1302 | `		pObj = 0;` |
|        5 | 1303 | `	}/*for(;;)*/` |
|   148205 | 1304 | `	if( iCons > 1 ){` |
|        - | 1305 | `		/* Concatenate all compiled constants */` |
|     2089 | 1306 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|   147163 | 1307 | `	}else if( iCons == 1 && nInterp == 1 ){` |
|        - | 1308 | `		/* A string that is nothing but one interpolation ("$x") still has to` |
|        - | 1309 | `		 * PRODUCE A STRING. With no CAT to force the conversion the operand was` |
|        - | 1310 | ``		 * left on the stack untouched, so `$s = "$x"` handed back $x's own type:`` |
|        - | 1311 | `		 * "$arr" stayed an array (and skipped php's "Array to string conversion"` |
|        - | 1312 | `		 * warning), "$int" stayed an int, "$res" stayed a resource. */` |
|       80 | 1313 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CVT_STR,0,0,0,0);` |
|       38 | 1314 | `	}` |
|        - | 1315 | `	/* Node successfully compiled */` |
|   148205 | 1316 | `	return SXRET_OK;` |
|    74401 | 1317 | `}` |
|        - | 1318 | `/*` |
|        - | 1319 | ` * Compile a double quoted string.` |
|        - | 1320 | ` *  See the block-comment above for more information.` |
|        - | 1321 | ` */` |
|   148720 | 1322 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 | 1323 | `{` |
|        - | 1324 | `	sxi32 rc;` |
|   148725 | 1325 | `	rc = GenStateCompileString(&(*pGen),0/*bHeredoc*/);` |
|    74360 | 1326 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        - | 1327 | `	/* Compilation result */` |
|   148725 | 1328 | `	return rc;` |
|        5 | 1329 | `}` |
|        - | 1330 | `/*` |
|        - | 1331 | ` * Compile a Heredoc string.` |
|        - | 1332 | ` *  See the block-comment above for more information.` |
|        - | 1333 | ` */` |
|       76 | 1334 | `PH7_PRIVATE sxi32 PH7_CompileHereDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 | 1335 | `{` |
|        - | 1336 | `	SyString sOrig, sStripped;` |
|        - | 1337 | `	sxi32 rc;` |
|       81 | 1338 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|       81 | 1339 | `	if( rc != SXRET_OK ){` |
|        6 | 1340 | `		return rc;` |
|        - | 1341 | `	}` |
|        - | 1342 | `	/* Temporarily swap in the dedented body so GenStateCompileString` |
|        - | 1343 | `	 * (which reads pGen->pIn->sData directly) sees the stripped content.` |
|        - | 1344 | `	 * Restore before returning so downstream code that references pIn is` |
|        - | 1345 | `	 * unaffected, including on the error path. */` |
|       77 | 1346 | `	sOrig = pGen->pIn->sData;` |
|       77 | 1347 | `	pGen->pIn->sData = sStripped;` |
|       77 | 1348 | `	rc = GenStateCompileString(&(*pGen),1/*bHeredoc*/);` |
|       77 | 1349 | `	pGen->pIn->sData = sOrig;` |
|       36 | 1350 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       77 | 1351 | `	return rc;` |
|       43 | 1352 | `}` |
|        - | 1353 | `/*` |
|        - | 1354 | ` * Compile an array entry whether it is a key or a value.` |
|        - | 1355 | ` *  Notes on array entries.` |
|        - | 1356 | ` *  According to the PHP language reference manual` |
|        - | 1357 | ` *  An array can be created by the array() language construct.` |
|        - | 1358 | ` *  It takes as parameters any number of comma-separated key => value pairs.` |
|        - | 1359 | ` *  array(  key =>  value` |
|        - | 1360 | ` *    , ...` |
|        - | 1361 | ` *    )` |
|        - | 1362 | ` *  A key may be either an integer or a string. If a key is the standard representation` |
|        - | 1363 | ` *  of an integer, it will be interpreted as such (i.e. "8" will be interpreted as 8, while` |
|        - | 1364 | ` *  "08" will be interpreted as "08"). Floats in key are truncated to integer.` |
|        - | 1365 | ` *  The indexed and associative array types are the same type in PHP, which can both` |
|        - | 1366 | ` *  contain integer and string indices.` |
|        - | 1367 | ` *  A value can be any PHP type.` |
|        - | 1368 | ` *  If a key is not specified for a value, the maximum of the integer indices is taken` |
|        - | 1369 | ` *  and the new key will be that value plus 1. If a key that already has an assigned value` |
|        - | 1370 | ` *  is specified, that value will be overwritten.` |
|        - | 1371 | ` */` |
|  1866244 | 1372 | `PH7_PRIVATE sxi32 GenStateCompileArrayEntry(` |
|        - | 1373 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - | 1374 | `	SyToken *pIn,        /* Token stream */` |
|        - | 1375 | `	SyToken *pEnd,       /* End of the token stream */` |
|        - | 1376 | `	sxi32 iFlags,        /* Compilation flags */` |
|        - | 1377 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *) /* Expression tree validator callback */` |
|        - | 1378 | `	)` |
|        5 | 1379 | `{` |
|        - | 1380 | `	SyToken *pTmpIn,*pTmpEnd;` |
|        - | 1381 | `	sxi32 rc;` |
|        - | 1382 | `	/* Swap token stream */` |
|  1866249 | 1383 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|        - | 1384 | `	/* Compile the expression*/` |
|  1866249 | 1385 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|        - | 1386 | `	/* Restore token stream */` |
|  1866249 | 1387 | `	RE_SWAP_DELIMITER(pGen);` |
|  1866249 | 1388 | `	return rc;` |
|        5 | 1389 | `}` |
|        - | 1390 | `/*` |
|        - | 1391 | ` * Expression tree validator callback for the 'array' language construct.` |
|        - | 1392 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|        - | 1393 | ` * an invalid expression tree and this function will generate the appropriate` |
|        - | 1394 | ` * error message.` |
|        - | 1395 | ` * See the routine responible of compiling the array language construct` |
|        - | 1396 | ` * for more inforation.` |
|        - | 1397 | ` */` |
|       38 | 1398 | `static sxi32 GenStateArrayNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        5 | 1399 | `{` |
|       43 | 1400 | `	sxi32 rc = SXRET_OK;` |
|       43 | 1401 | `	if( pRoot->pOp ){` |
|       16 | 1402 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ &&` |
|       12 | 1403 | `			pRoot->pOp->iOp != EXPR_OP_FUNC_CALL /* function() [Symisc extension: i.e: array(&foo())] */` |
|       17 | 1404 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|        - | 1405 | `			/* Unexpected expression */` |
|       13 | 1406 | `			rc = PH7_GenSyntaxError(&(*pGen),pRoot->pStart,"\"->\" or \"?->\" or \"[\"");` |
|       13 | 1407 | `			if( rc != SXERR_ABORT ){` |
|       13 | 1408 | `				rc = SXERR_INVALID;` |
|        5 | 1409 | `			}` |
|       10 | 1410 | `		}` |
|       32 | 1411 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|        - | 1412 | `		/* Unexpected expression */` |
|        3 | 1413 | `		rc = PH7_GenSyntaxError(&(*pGen),pRoot->pStart,0);` |
|        3 | 1414 | `		if( rc != SXERR_ABORT ){` |
|        3 | 1415 | `			rc = SXERR_INVALID;` |
|        1 | 1416 | `		}` |
|        1 | 1417 | `	}` |
|       43 | 1418 | `	return rc;` |
|        5 | 1419 | `}` |
|        - | 1420 | `/*` |
|        - | 1421 | ` * Find the top-level '=>' (PH7_TK_ARRAY_OP) that separates an array/list entry's` |
|        - | 1422 | ` * key from its value within [pStart,pEnd). The scan skips any '=>' nested inside` |
|        - | 1423 | ` * brackets/parens/braces, inside an arrow-function signature (fn(...) =>), or` |
|        - | 1424 | ` * inside a match() {...} arm — none of which are key/value separators. Returns a` |
|        - | 1425 | ` * pointer to the '=>' token, or pEnd if the entry has no top-level separator.` |
|        - | 1426 | ` */` |
|  1764728 | 1427 | `PH7_PRIVATE SyToken * GenStateFindTopLevelArrow(SyToken *pStart,SyToken *pEnd)` |
|        5 | 1428 | `{` |
|  1764733 | 1429 | `	SyToken *pCur = pStart;` |
|  1764733 | 1430 | `	sxi32 iNest = 0;` |
|  4492047 | 1431 | `	while( pCur < pEnd ){` |
|  3360245 | 1432 | `		if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|   632927 | 1433 | `			return pCur;` |
|        - | 1434 | `		}` |
|        - | 1435 | `		/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|        - | 1436 | `		 * The '=>' inside an arrow function introduces the expression body,` |
|        - | 1437 | `		 * not an entry separator. Skip past the signature.` |
|        - | 1438 | `		 */` |
|  2727323 | 1439 | `		if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
|    27327 | 1440 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|    27327 | 1441 | `			SyToken *pFn = pCur;` |
|        - | 1442 | ``			/* Only a real `[static] fn[&](` opens an arrow function; `$fn`,`` |
|        - | 1443 | ``			 * `C::fn` and friends are plain names whose '=>' IS the separator. */`` |
|    27327 | 1444 | `			if( PH7_TokenOpensArrowFunc(pStart,pCur,pEnd) ){` |
|        5 | 1445 | `				if( nKw == PH7_TKWRD_STATIC ){` |
|      ! 0 | 1446 | `					pFn = &pCur[1];` |
|      ! 0 | 1447 | `				}` |
|        5 | 1448 | `				pCur = pFn + 1; /* past 'fn' */` |
|        5 | 1449 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_AMPER) ){` |
|      ! 0 | 1450 | `					pCur++;` |
|      ! 0 | 1451 | `				}` |
|        5 | 1452 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|        5 | 1453 | `					pCur++;` |
|        5 | 1454 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|        - | 1455 | `						PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|        5 | 1456 | `					if( pCur < pEnd ){` |
|        5 | 1457 | `						pCur++;` |
|        2 | 1458 | `					}` |
|        2 | 1459 | `				}` |
|        5 | 1460 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_COLON) ){` |
|      ! 0 | 1461 | `					pCur++;` |
|      ! 0 | 1462 | `					if( pCur < pEnd && (pCur->nType & PH7_TK_OP)` |
|      ! 0 | 1463 | `						&& pCur->sData.nByte == 1` |
|      ! 0 | 1464 | `						&& pCur->sData.zString[0] == '?' ){` |
|      ! 0 | 1465 | `						pCur++;` |
|      ! 0 | 1466 | `					}` |
|      ! 0 | 1467 | `					if( pCur < pEnd` |
|      ! 0 | 1468 | `						&& (pCur->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|      ! 0 | 1469 | `						pCur++;` |
|      ! 0 | 1470 | `					}` |
|      ! 0 | 1471 | `				}` |
|        - | 1472 | `				/* The rest of the entry is the arrow-function body — no outer` |
|        - | 1473 | `				 * key to extract. */` |
|        5 | 1474 | `				return pEnd;` |
|        - | 1475 | `			}` |
|        - | 1476 | `			/* Match expression (PHP 8.0): the '=>' inside match arms is not an` |
|        - | 1477 | `			 * entry separator. Skip past the full match span. */` |
|    27323 | 1478 | `			if( nKw == PH7_TKWRD_MATCH ){` |
|        3 | 1479 | `				pCur++; /* past 'match' */` |
|        3 | 1480 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|        3 | 1481 | `					pCur++;` |
|        3 | 1482 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|        - | 1483 | `						PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|        3 | 1484 | `					if( pCur < pEnd ){` |
|        3 | 1485 | `						pCur++;` |
|        1 | 1486 | `					}` |
|        1 | 1487 | `				}` |
|        3 | 1488 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_OCB) ){` |
|        3 | 1489 | `					pCur++;` |
|        3 | 1490 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|        - | 1491 | `						PH7_TK_OCB,PH7_TK_CCB,&pCur);` |
|        3 | 1492 | `					if( pCur < pEnd ){` |
|        3 | 1493 | `						pCur++;` |
|        1 | 1494 | `					}` |
|        1 | 1495 | `				}` |
|        3 | 1496 | `				continue;` |
|        - | 1497 | `			}` |
|    13658 | 1498 | `		}` |
|  2727317 | 1499 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|    64699 | 1500 | `			iNest++;` |
|  2694970 | 1501 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/) ){` |
|        - | 1502 | `			/* Don't worry about mismatched brackets here, the expression` |
|        - | 1503 | `			 * parser will shortly detect any syntax error. */` |
|    64699 | 1504 | `			iNest--;` |
|    32347 | 1505 | `		}` |
|  2727317 | 1506 | `		pCur++;` |
|        5 | 1507 | `	}` |
|  1131807 | 1508 | `	return pEnd;` |
|   882369 | 1509 | `}` |
|        - | 1510 | `/*` |
|        - | 1511 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|        - | 1512 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|        - | 1513 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|        - | 1514 | ` */` |
|   785514 | 1515 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|        5 | 1516 | `{` |
|        - | 1517 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|        - | 1518 | `	SyToken *pKey,*pCur;` |
|   785519 | 1519 | `	sxi32 iEmitRef = 0;` |
|   785519 | 1520 | `	sxi32 iSpread = 0;` |
|   785519 | 1521 | `	sxi32 nPair = 0;` |
|        - | 1522 | `	sxi32 rc;` |
|   785519 | 1523 | `	xValidator = 0;` |
|  1086438 | 1524 | `	for(;;){` |
|        - | 1525 | `		/* Jump leading commas. Exactly ONE separates two entries; a second one (or a comma` |
|        - | 1526 | `		 * at the very start) means an EMPTY element, which php rejects outright — PH7 just` |
|        - | 1527 | ``		 * skipped them, so `array(,)` and `array(1,,2)` compiled silently. A TRAILING comma`` |
|        - | 1528 | `		 * is legal and is handled by the loop exiting on the next pass. */` |
|   693681 | 1529 | `		{` |
|  2172881 | 1530 | `			int nSkip = 0;` |
|  3228665 | 1531 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|  1055789 | 1532 | `				nSkip++;` |
|  1055789 | 1533 | `				pGen->pIn++;` |
|        5 | 1534 | `			}` |
|  2172881 | 1535 | `			if( nSkip > 1 \|\| (nSkip > 0 && nPair < 1) ){` |
|      ! 0 | 1536 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn[-1].nLine,` |
|        - | 1537 | `					"Cannot use empty array elements in arrays");` |
|      ! 0 | 1538 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1539 | `					return SXERR_ABORT;` |
|        - | 1540 | `				}` |
|      ! 0 | 1541 | `				return SXRET_OK;` |
|        - | 1542 | `			}` |
|        - | 1543 | `		}` |
|  2172881 | 1544 | `		pCur = pGen->pIn;` |
|  2172881 | 1545 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|        - | 1546 | `			/* No more entry to process */` |
|   785501 | 1547 | `			break;` |
|        - | 1548 | `		}` |
|  1387385 | 1549 | `		if( pCur >= pGen->pIn ){` |
|      ! 0 | 1550 | `			continue;` |
|        - | 1551 | `		}` |
|        - | 1552 | `		/* Compile the key if available */` |
|  1387385 | 1553 | `		pKey = pCur;` |
|  1387385 | 1554 | `		pCur = GenStateFindTopLevelArrow(pCur,pGen->pIn);` |
|  1387385 | 1555 | `		rc = SXERR_EMPTY;` |
|  1387385 | 1556 | `		if( pCur < pGen->pIn ){` |
|   478513 | 1557 | `			if( pKey == pCur ){` |
|        - | 1558 | ``				/* `array( => 2)`: the entry STARTS with '=>', so it has no key. php rejects`` |
|        - | 1559 | `				 * it; PH7 warned about a "Missing entry key" and compiled on, accepting` |
|        - | 1560 | ``				 * source php refuses. (The `else if` below could never see this: the arrow`` |
|        - | 1561 | `				 * IS found here, so control never reached it.)` |
|        - | 1562 | `				 * php names the literal's own closer, so short syntax expects ']'. */` |
|        3 | 1563 | `				const char *zClose = (pGen->pEnd && (pGen->pEnd->nType & PH7_TK_CSB))` |
|        - | 1564 | `					? "\"]\"" : "\")\"";` |
|        3 | 1565 | `				rc = PH7_GenSyntaxError(&(*pGen),pCur,zClose);` |
|        3 | 1566 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1567 | `					return SXERR_ABORT;` |
|        - | 1568 | `				}` |
|        3 | 1569 | `				return SXRET_OK;` |
|        - | 1570 | `			}` |
|   478511 | 1571 | `			if( &pCur[1] >= pGen->pIn ){` |
|        - | 1572 | ``				/* `array(1 => )`: php names the token that SHOULD have started the value —`` |
|        - | 1573 | `				 * the ')' or ']' closing the literal — not the '=>' it just read. Passing 0` |
|        - | 1574 | `				 * makes the helper reach for the token past this entry's slice. */` |
|       13 | 1575 | `				rc = PH7_GenSyntaxError(&(*pGen),0,0);` |
|       13 | 1576 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1577 | `					return SXERR_ABORT;` |
|        - | 1578 | `				}` |
|       13 | 1579 | `				return SXRET_OK;` |
|        - | 1580 | `			}` |
|        - | 1581 | `			/* Compile the expression holding the key */` |
|   478501 | 1582 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|        - | 1583 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|   478501 | 1584 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1585 | `				return SXERR_ABORT;` |
|        - | 1586 | `			}` |
|   478501 | 1587 | `			pCur++; /* Jump the '=>' operator */` |
|   239253 | 1588 | `		}else{` |
|        - | 1589 | `			/* Reset back the cursor and point to the entry value */` |
|   908877 | 1590 | `			pCur = pKey;` |
|        - | 1591 | `		}` |
|  1387373 | 1592 | `		if( rc == SXERR_EMPTY ){` |
|        - | 1593 | `			/* No key given: load the nil, TAGGED so LOAD_MAP knows this is an absent key` |
|        - | 1594 | ``			 * (auto-index) rather than an explicit `null =>` one, which php deprecates. */`` |
|   908877 | 1595 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,PH7_LOADC_NOKEY,0 /* nil index */,0,0);` |
|   454436 | 1596 | `		}` |
|  1387373 | 1597 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
|        - | 1598 | `			/* Insertion by reference, [i.e: $a = array(&$x);] */` |
|       47 | 1599 | `			xValidator = GenStateArrayNodeValidator; /* Only variable are allowed */` |
|       47 | 1600 | `			iEmitRef = 1;` |
|       47 | 1601 | `			pCur++; /* Jump the '&' token */` |
|       47 | 1602 | `			if( pCur >= pGen->pIn ){` |
|        - | 1603 | `				/* Missing value */` |
|        - | 1604 | ``				/* php reports the token that actually stopped it (`array(&)` -> the`` |
|        - | 1605 | `				 * ')'), not a hand-written "missing referenced variable" fatal. */` |
|        3 | 1606 | `				rc = PH7_GenSyntaxError(&(*pGen),pCur < pGen->pIn ? pCur : 0,0);` |
|        3 | 1607 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1608 | `					return SXERR_ABORT;` |
|        - | 1609 | `				}` |
|        3 | 1610 | `				return SXRET_OK;` |
|        - | 1611 | `			}` |
|       20 | 1612 | `		}` |
|        - | 1613 | `		/* Detect array unpack: '...$expr' as the entry value (PHP 7.4+, with` |
|        - | 1614 | `		 * string-key support since PHP 8.1). The parser strips the '...' inside` |
|        - | 1615 | `		 * ExprExtractNode; we only need to know it's there so we can emit` |
|        - | 1616 | `		 * PH7_OP_FLAG_SPREAD after the value, instructing LOAD_MAP to merge the` |
|        - | 1617 | `		 * resulting hashmap rather than insert it as a scalar entry. */` |
|  1387371 | 1618 | `		iSpread = (pCur < pGen->pIn && (pCur->nType & PH7_TK_ELLIPSIS)) ? 1 : 0;` |
|  1387371 | 1619 | `		if( iSpread && (rc != SXERR_EMPTY \|\| iEmitRef) ){` |
|        - | 1620 | `			/* '[k => ...$a]' and '[&...$a]' are syntax errors in PHP — the` |
|        - | 1621 | `			 * '...' token cannot follow either '=>' or '&' inside an array` |
|        - | 1622 | `			 * literal. Emit the same Parse-error wording PHP uses so the` |
|        - | 1623 | `			 * output is engine-portable. */` |
|        6 | 1624 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pCur->nLine,` |
|        - | 1625 | `				"syntax error, unexpected token \"...\"");` |
|        6 | 1626 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1627 | `				return SXERR_ABORT;` |
|        - | 1628 | `			}` |
|        6 | 1629 | `			return SXRET_OK;` |
|        - | 1630 | `		}` |
|        - | 1631 | ``		/* Compile indice value. A BY-REF element (`'k' => &$a[$i]`) is an`` |
|        - | 1632 | `		 * lvalue: php VIVIFIES a missing subscript when a reference is taken,` |
|        - | 1633 | `		 * so compile it in write context (LOAD_IDX iP2=1, create-if-missing)` |
|        - | 1634 | `		 * instead of a read-only load — which also keeps the undefined-key` |
|        - | 1635 | `		 * warning (a read-only diagnostic) from false-firing here. */` |
|  2081048 | 1636 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,` |
|   693681 | 1637 | `			iEmitRef ? EXPR_FLAG_LOAD_IDX_STORE` |
|        - | 1638 | `			         : EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,` |
|   693681 | 1639 | `			xValidator);` |
|  1387367 | 1640 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1641 | `			return SXERR_ABORT;` |
|        - | 1642 | `		}` |
|  1387367 | 1643 | `		if( iSpread ){` |
|        - | 1644 | `			/* Mark the value on TOS as a spread source; LOAD_MAP merges it. */` |
|       73 | 1645 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FLAG_SPREAD,0,0,0,0);` |
|  1387332 | 1646 | `		}else if( iEmitRef ){` |
|        - | 1647 | `			/* Emit the load reference instruction */` |
|       43 | 1648 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|       19 | 1649 | `		}` |
|  1387367 | 1650 | `		xValidator = 0;` |
|  1387367 | 1651 | `		iEmitRef = 0;` |
|  1387367 | 1652 | `		iSpread = 0;` |
|  1387367 | 1653 | `		nPair++;` |
|        5 | 1654 | `	}` |
|        - | 1655 | `	/* Emit the load map instruction */` |
|   785501 | 1656 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|        - | 1657 | `	/* Node successfully compiled */` |
|   785501 | 1658 | `	return SXRET_OK;` |
|   392762 | 1659 | `}` |
|        - | 1660 | `/*` |
|        - | 1661 | ` * Compile the 'array' language construct.` |
|        - | 1662 | ` *	 According to the PHP language reference manual` |
|        - | 1663 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|        - | 1664 | ` *   values to keys. This type is optimized for several different uses; it can` |
|        - | 1665 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|        - | 1666 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|        - | 1667 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|        - | 1668 | ` */` |
|   490866 | 1669 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 | 1670 | `{` |
|        - | 1671 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   490871 | 1672 | `	pGen->pIn += 2;` |
|   490871 | 1673 | `	pGen->pEnd--;` |
|   245433 | 1674 | `	SXUNUSED(iCompileFlag);` |
|        - | 1675 | ``	/* php: a stray token in an `array( ... )` element is `... expecting ")"`. */`` |
|        - | 1676 | `	{` |
|   490871 | 1677 | `		const char *zSave = pGen->zClauseCloser;` |
|        - | 1678 | `		sxi32 rc;` |
|   490871 | 1679 | `		pGen->zClauseCloser = "\")\"";` |
|   490871 | 1680 | `		rc = GenStateCompileArrayBody(pGen);` |
|   490871 | 1681 | `		pGen->zClauseCloser = zSave;` |
|   490871 | 1682 | `		return rc;` |
|        - | 1683 | `	}` |
|        5 | 1684 | `}` |
|        - | 1685 | `/*` |
|        - | 1686 | ` * Compile the PHP 8.5 clone(...) call form:` |
|        - | 1687 | `` *   clone($object)                          -> identical to the `clone $object` operator`` |
|        - | 1688 | ` *   clone($object, ['prop' => value, ...])  -> clone, run __clone(), then apply the` |
|        - | 1689 | ` *                                              property updates as scope-aware writes` |
|        - | 1690 | ` *   clone(object: $o, withProperties: [..]) -> the named-argument spelling` |
|        - | 1691 | ` * Codegen: compile the object argument and emit OP_CLONE (which clones and runs` |
|        - | 1692 | ` * __clone()); if a withProperties argument is present, compile it and emit` |
|        - | 1693 | ` * OP_CLONE_APPLY, which applies each update to the fresh clone AFTER __clone(),` |
|        - | 1694 | ` * honouring visibility / readonly-set-scope / typed-property enforcement in the` |
|        - | 1695 | ` * calling scope. The parser (ExprExtractNode) delimited this node's tokens as` |
|        - | 1696 | `` * `clone ( ... )`; pGen->pIn/pEnd point at the first/one-past-last of that range.`` |
|        - | 1697 | ` */` |
|       22 | 1698 | `PH7_PRIVATE sxi32 PH7_CompileCloneCall(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        2 | 1699 | `{` |
|        - | 1700 | `	SyToken *pIn,*pEnd,*pNext;` |
|       24 | 1701 | `	SyToken *pObjStart = 0,*pObjEnd = 0;` |
|       24 | 1702 | `	SyToken *pUpdStart = 0,*pUpdEnd = 0;` |
|       24 | 1703 | `	int nArg = 0;` |
|        - | 1704 | `	sxi32 rc;` |
|       11 | 1705 | `	SXUNUSED(iCompileFlag);` |
|        - | 1706 | `	/* pGen->pIn -> 'clone', pGen->pIn[1] -> '(', pGen->pEnd -> one past ')'. */` |
|       24 | 1707 | `	pIn  = pGen->pIn + 2;   /* skip 'clone' and the opening '(' */` |
|       24 | 1708 | `	pEnd = pGen->pEnd - 1;  /* exclude the closing ')' */` |
|        - | 1709 | `	/* clone(...) first-class-callable form: a lone ellipsis is the whole list. */` |
|       24 | 1710 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|      ! 0 | 1711 | `		return PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|        - | 1712 | `			"clone(...) first-class callable form is not yet supported");` |
|        - | 1713 | `	}` |
|        - | 1714 | `	/* Split the (at most two) comma-separated arguments, tolerating named labels. */` |
|       62 | 1715 | `	while( pIn < pEnd ){` |
|       40 | 1716 | `		SyToken *pArgStart,*pArgEnd,*pName = 0;` |
|       40 | 1717 | `		if( PH7_GetNextExpr(pIn,pEnd,&pNext) != SXRET_OK ){` |
|      ! 0 | 1718 | `			break;` |
|        - | 1719 | `		}` |
|       40 | 1720 | `		pArgStart = pIn;` |
|       40 | 1721 | `		pArgEnd   = pNext;` |
|        - | 1722 | `		/* Named-argument label: <ID\|keyword> ':' expr. A single ':' is PH7_TK_COLON;` |
|        - | 1723 | ``		 * '::' is a distinct operator token, so this never mis-fires on `A::B`. */`` |
|       38 | 1724 | `		if( (pArgEnd - pArgStart) >= 2` |
|       37 | 1725 | `			&& (pArgStart[0].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|       23 | 1726 | `			&& (pArgStart[1].nType & PH7_TK_COLON) ){` |
|        5 | 1727 | `			pName = pArgStart;` |
|        5 | 1728 | `			pArgStart += 2;` |
|        2 | 1729 | `		}` |
|       40 | 1730 | `		if( pName ){` |
|        - | 1731 | `` 			/* PHP named parameters are case-SENSITIVE, so `Object:`/`WITHPROPERTIES:` `` |
|        - | 1732 | `			 * must be rejected as unknown (SyMemcmp, not SyStrnicmp). */` |
|        4 | 1733 | `			if( pName->sData.nByte == sizeof("object")-1` |
|        4 | 1734 | `				&& SyMemcmp(pName->sData.zString,"object",sizeof("object")-1) == 0 ){` |
|        3 | 1735 | `				pObjStart = pArgStart; pObjEnd = pArgEnd;` |
|        4 | 1736 | `			}else if( pName->sData.nByte == sizeof("withProperties")-1` |
|        3 | 1737 | `				&& SyMemcmp(pName->sData.zString,"withProperties",sizeof("withProperties")-1) == 0 ){` |
|        3 | 1738 | `				pUpdStart = pArgStart; pUpdEnd = pArgEnd;` |
|        2 | 1739 | `			}else{` |
|      ! 0 | 1740 | `				return PH7_GenCompileError(pGen,E_ERROR,pName->nLine,` |
|      ! 0 | 1741 | `					"Unknown named parameter $%z",&pName->sData);` |
|        1 | 1742 | `			}` |
|       38 | 1743 | `		}else if( nArg == 0 ){` |
|       22 | 1744 | `			pObjStart = pArgStart; pObjEnd = pArgEnd;` |
|       25 | 1745 | `		}else if( nArg == 1 ){` |
|       15 | 1746 | `			pUpdStart = pArgStart; pUpdEnd = pArgEnd;` |
|        8 | 1747 | `		}else{` |
|      ! 0 | 1748 | `			return PH7_GenCompileError(pGen,E_ERROR,pArgStart->nLine,` |
|        - | 1749 | `				"clone() expects at most 2 arguments");` |
|        - | 1750 | `		}` |
|       40 | 1751 | `		nArg++;` |
|       40 | 1752 | `		pIn = pNext;` |
|       40 | 1753 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|       17 | 1754 | `			pIn++; /* step over the argument separator */` |
|        8 | 1755 | `		}` |
|        2 | 1756 | `	}` |
|       24 | 1757 | `	if( pObjStart == 0 \|\| pObjStart >= pObjEnd ){` |
|      ! 0 | 1758 | `		return PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 1759 | `			"clone() expects at least 1 argument, 0 given");` |
|        - | 1760 | `	}` |
|        - | 1761 | `	/* Object argument -> clone (+ __clone()). */` |
|       24 | 1762 | `	rc = GenStateCompileArrayEntry(pGen,pObjStart,pObjEnd,EXPR_FLAG_RDONLY_LOAD,0);` |
|       24 | 1763 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 1764 | `		return SXERR_ABORT;` |
|        - | 1765 | `	}` |
|       24 | 1766 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_CLONE,0,0,0,0);` |
|        - | 1767 | `	/* Property updates (evaluated after __clone runs). */` |
|       24 | 1768 | `	if( pUpdStart && pUpdStart < pUpdEnd ){` |
|       17 | 1769 | `		rc = GenStateCompileArrayEntry(pGen,pUpdStart,pUpdEnd,EXPR_FLAG_RDONLY_LOAD,0);` |
|       17 | 1770 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1771 | `			return SXERR_ABORT;` |
|        - | 1772 | `		}` |
|       17 | 1773 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CLONE_APPLY,0,0,0,0);` |
|        8 | 1774 | `	}` |
|       24 | 1775 | `	return SXRET_OK;` |
|       13 | 1776 | `}` |
|        - | 1777 | `/*` |
|        - | 1778 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|        - | 1779 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|        - | 1780 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|        - | 1781 | ` */` |
|   294648 | 1782 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 | 1783 | `{` |
|        - | 1784 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|   294653 | 1785 | `	pGen->pIn++;` |
|   294653 | 1786 | `	pGen->pEnd--;` |
|   147324 | 1787 | `	SXUNUSED(iCompileFlag);` |
|        - | 1788 | ``	/* php: a stray token in a `[ ... ]` element is `... expecting "]"`. */`` |
|        - | 1789 | `	{` |
|   294653 | 1790 | `		const char *zSave = pGen->zClauseCloser;` |
|        - | 1791 | `		sxi32 rc;` |
|   294653 | 1792 | `		pGen->zClauseCloser = "\"]\"";` |
|   294653 | 1793 | `		rc = GenStateCompileArrayBody(pGen);` |
|   294653 | 1794 | `		pGen->zClauseCloser = zSave;` |
|   294653 | 1795 | `		return rc;` |
|        - | 1796 | `	}` |
|        5 | 1797 | `}` |
|        - | 1798 | `/*` |
|        - | 1799 | ` * Expression tree validator callback for the 'list' language construct.` |
|        - | 1800 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|        - | 1801 | ` * an invalid expression tree and this function will generate the appropriate` |
|        - | 1802 | ` * error message.` |
|        - | 1803 | ` * See the routine responible of compiling the list language construct` |
|        - | 1804 | ` * for more inforation.` |
|        - | 1805 | ` */` |
|      298 | 1806 | `static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        5 | 1807 | `{` |
|      303 | 1808 | `	sxi32 rc = SXRET_OK;` |
|      303 | 1809 | `	if( pRoot->pOp ){` |
|       46 | 1810 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */` |
|       26 | 1811 | `			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){` |
|        - | 1812 | `				/* Unexpected expression */` |
|      ! 0 | 1813 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - | 1814 | `					"Assignments can only happen to writable values");` |
|      ! 0 | 1815 | `				if( rc != SXERR_ABORT ){` |
|      ! 0 | 1816 | `					rc = SXERR_INVALID;` |
|      ! 0 | 1817 | `				}` |
|        2 | 1818 | `		}` |
|      280 | 1819 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|        - | 1820 | `		/* Unexpected expression */` |
|        6 | 1821 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - | 1822 | `			"Assignments can only happen to writable values");` |
|        6 | 1823 | `		if( rc != SXERR_ABORT ){` |
|        6 | 1824 | `			rc = SXERR_INVALID;` |
|        2 | 1825 | `		}` |
|        2 | 1826 | `	}` |
|      303 | 1827 | `	return rc;` |
|        5 | 1828 | `}` |
|        - | 1829 | `/*` |
|        - | 1830 | ` * Compile the 'list' language construct.` |
|        - | 1831 | ` *  According to the PHP language reference` |
|        - | 1832 | ` *  list(): Assign variables as if they were an array.` |
|        - | 1833 | ` *  list() is used to assign a list of variables in one operation.` |
|        - | 1834 | ` *  Description` |
|        - | 1835 | ` *   array list (mixed $varname [, mixed $... ] )` |
|        - | 1836 | ` *   Like array(), this is not really a function, but a language construct.` |
|        - | 1837 | ` *   list() is used to assign a list of variables in one operation.` |
|        - | 1838 | ` *  Parameters` |
|        - | 1839 | ` *   $varname: A variable.` |
|        - | 1840 | ` *  Return Values` |
|        - | 1841 | ` *   The assigned array.` |
|        - | 1842 | ` */` |
|        - | 1843 | `/* Nested list entry recorded during first pass of list body compilation */` |
|        - | 1844 | `struct NestedListEntry {` |
|        - | 1845 | `	sxi32 nIndex;        /* Position in the outer list (0-based) */` |
|        - | 1846 | `	SyToken *pStart;     /* Token range: start of nested construct */` |
|        - | 1847 | `	SyToken *pEnd;       /* Token range: past closing delimiter */` |
|        - | 1848 | `	sxi32 isShort;       /* 1 if [...] form, 0 if list(...) form */` |
|        - | 1849 | `};` |
|        - | 1850 | `/*` |
|        - | 1851 | ` * Compile the body of a *keyed* list/short-list destructuring (PHP 7.1), where` |
|        - | 1852 | `` * every entry has the form `keyExpr => target`. The source array is on the stack`` |
|        - | 1853 | ` * top on entry and remains there on exit, mirroring the positional LOAD_LIST` |
|        - | 1854 | ` * path so the caller's teardown is unchanged. For each entry: DUP the source,` |
|        - | 1855 | ` * push the key, LOAD_IDX to fetch source[key] (NULL on a missing key, silently,` |
|        - | 1856 | ` * like a normal subscript read), then assign the fetched value to the target — a` |
|        - | 1857 | ` * nested [...]/list() recurses, a simple lvalue uses the same STORE fold as a` |
|        - | 1858 | ` * normal assignment (the value sits below the lvalue-load, exactly as in` |
|        - | 1859 | ` * GenStateEmitExprCode where the assignment RHS precedes the LHS load).` |
|        - | 1860 | ` */` |
|       34 | 1861 | `static sxi32 GenStateCompileKeyedListBody(ph7_gen_state *pGen)` |
|        2 | 1862 | `{` |
|        - | 1863 | `	SyToken *pNext;` |
|        - | 1864 | `	sxi32 rc;` |
|       78 | 1865 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|        - | 1866 | `		SyToken *pArrow,*pTarget;` |
|        - | 1867 | ``		/* Split `keyExpr => target` at the top-level '=>' */`` |
|       44 | 1868 | `		pArrow = GenStateFindTopLevelArrow(pGen->pIn,pNext);` |
|       44 | 1869 | `		pTarget = &pArrow[1];` |
|       44 | 1870 | `		if( pArrow <= pGen->pIn \|\| pTarget >= pNext ){` |
|        - | 1871 | ``			/* Empty key (`[ => $v]`) or empty value (`["k" =>]`): PHP rejects`` |
|        - | 1872 | `			 * both. Reject rather than silently emitting unbalanced bytecode. */` |
|      ! 0 | 1873 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - | 1874 | `				"Cannot use empty array entries in keyed array assignment");` |
|      ! 0 | 1875 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 1876 | `		}` |
|        - | 1877 | `		/* DUP the source array (it is on the stack top) */` |
|       44 | 1878 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|        - | 1879 | `		/* Compile the key expression; it is pushed above the DUP'd source */` |
|       44 | 1880 | `		rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pArrow,EXPR_FLAG_RDONLY_LOAD,0);` |
|       44 | 1881 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1882 | `			return SXERR_ABORT;` |
|        - | 1883 | `		}` |
|        - | 1884 | `		/* LOAD_IDX: pop the key, replace the DUP'd source with source[key].` |
|        - | 1885 | `		 * iP2=7 is the keyed-destructuring read context: an array source reads like` |
|        - | 1886 | ``		 * iP2=0 (missing key loads NULL silently, matching a normal `$arr[$k]` read;`` |
|        - | 1887 | `		 * PHP also emits an "Undefined array key" warning here, PHL omits it — §3.7),` |
|        - | 1888 | `		 * but a NON-array source yields NULL + a per-key "Cannot use <type> as array"` |
|        - | 1889 | `		 * warning instead of char-indexing a string (matching PHP's OP_LOAD_LIST path). */` |
|       44 | 1890 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,7,0,0);` |
|       44 | 1891 | `		if( pTarget < pNext && ( (pTarget->nType & PH7_TK_OSB)` |
|       40 | 1892 | `			\|\| ( (pTarget->nType & PH7_TK_KEYWORD)` |
|       21 | 1893 | `				&& SX_PTR_TO_INT(pTarget->pUserData) == PH7_TKWRD_LIST ) ) ){` |
|        - | 1894 | `			/* Nested destructuring:  ["k" => [ ... ]]  or  ["k" => list( ... )].` |
|        - | 1895 | `			 * Treat source[key] as the inner body's source, then drop the` |
|        - | 1896 | `			 * leftover it leaves behind (mirrors the positional nested path). */` |
|        5 | 1897 | `			sxi32 isShort = (pTarget->nType & PH7_TK_OSB) != 0;` |
|        5 | 1898 | `			SyToken *pSavedIn = pGen->pIn;` |
|        5 | 1899 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|        5 | 1900 | `			pGen->pIn = pTarget;` |
|        5 | 1901 | `			pGen->pEnd = pNext;` |
|        5 | 1902 | `			rc = isShort ? PH7_CompileShortList(&(*pGen),0)` |
|        2 | 1903 | `			             : PH7_CompileList(&(*pGen),0);` |
|        5 | 1904 | `			pGen->pIn = pSavedIn;` |
|        5 | 1905 | `			pGen->pEnd = pSavedEnd;` |
|        5 | 1906 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1907 | `				return SXERR_ABORT;` |
|        - | 1908 | `			}` |
|        5 | 1909 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        3 | 1910 | `		}else{` |
|        - | 1911 | `			/* Simple lvalue target ($v / $o->p / $a[i] / Cls::$s). source[key]` |
|        - | 1912 | `			 * is already on the stack as the value; compiling the target appends` |
|        - | 1913 | `			 * its lvalue-load, which we fold into a STORE just as a normal` |
|        - | 1914 | `			 * assignment does. */` |
|        - | 1915 | `			VmInstr *pInstr;` |
|       40 | 1916 | `			sxi32 iVmOp = PH7_OP_STORE;` |
|       40 | 1917 | `			sxi32 iP1 = 0, iP2 = 0;` |
|       40 | 1918 | `			void *p3 = 0;` |
|       40 | 1919 | `			rc = GenStateCompileArrayEntry(&(*pGen),pTarget,pNext,` |
|        - | 1920 | `				EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|       40 | 1921 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1922 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 1923 | `			}` |
|       40 | 1924 | `			if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|       40 | 1925 | `				if( pInstr->iOp == PH7_OP_MEMBER ){` |
|        6 | 1926 | `					iP2 = 1; /* member store: keep MEMBER, store value below it */` |
|       38 | 1927 | `				}else if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|        3 | 1928 | `					iVmOp = PH7_OP_STORE_IDX;` |
|        3 | 1929 | `					iP1 = pInstr->iP1;` |
|        3 | 1930 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|        2 | 1931 | `				}else{` |
|       34 | 1932 | `					p3 = pInstr->p3; /* named store: $v = value */` |
|       34 | 1933 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|        - | 1934 | `				}` |
|       19 | 1935 | `			}` |
|       40 | 1936 | `			PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|        - | 1937 | `			/* STORE leaves the assigned value on the stack top; drop it so the` |
|        - | 1938 | `			 * source array is back on top for the next entry. */` |
|       40 | 1939 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        - | 1940 | `		}` |
|       44 | 1941 | `		pGen->pIn = &pNext[1];` |
|        2 | 1942 | `	}` |
|       36 | 1943 | `	return SXRET_OK;` |
|       19 | 1944 | `}` |
|        - | 1945 | `/*` |
|        - | 1946 | ` * Shared body for list() and short list [...] compilation.` |
|        - | 1947 | ` * Assumes pGen->pIn and pGen->pEnd are already positioned past` |
|        - | 1948 | ` * the opening delimiter and before the closing delimiter.` |
|        - | 1949 | ` */` |
|      196 | 1950 | `static sxi32 GenStateCompileListBody(ph7_gen_state *pGen)` |
|        5 | 1951 | `{` |
|        - | 1952 | `	SySet sNested; /* Dynamically-sized container of NestedListEntry */` |
|        - | 1953 | `	SyToken *pNext;` |
|        - | 1954 | `	SyToken *pClassifyIn;` |
|      201 | 1955 | `	sxi32 nKeyed = 0, nPositional = 0, nEmpty = 0;` |
|        - | 1956 | `	sxi32 nExpr;` |
|        - | 1957 | `	sxi32 rc;` |
|        - | 1958 | ``	/* First pass: classify entries as keyed (`k => v`), positional, or empty`` |
|        - | 1959 | `	 * skip slots ([,]). A list level must be entirely keyed or entirely` |
|        - | 1960 | `	 * positional — PHP fatals on a mix, and on an empty slot inside a keyed` |
|        - | 1961 | `	 * list. */` |
|      201 | 1962 | `	pClassifyIn = pGen->pIn;` |
|      531 | 1963 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|      335 | 1964 | `		if( pGen->pIn >= pNext ){` |
|       13 | 1965 | `			nEmpty++;` |
|      329 | 1966 | `		}else if( GenStateFindTopLevelArrow(pGen->pIn,pNext) < pNext ){` |
|       44 | 1967 | `			nKeyed++;` |
|       23 | 1968 | `		}else{` |
|      281 | 1969 | `			nPositional++;` |
|        - | 1970 | `		}` |
|      335 | 1971 | `		pGen->pIn = &pNext[1];` |
|        5 | 1972 | `	}` |
|      201 | 1973 | `	pGen->pIn = pClassifyIn;` |
|      201 | 1974 | `	if( nKeyed > 0 && nEmpty > 0 ){` |
|      ! 0 | 1975 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - | 1976 | `			"Cannot use empty array entries in keyed array assignment");` |
|      ! 0 | 1977 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 1978 | `	}` |
|      201 | 1979 | `	if( nKeyed > 0 && nPositional > 0 ){` |
|      ! 0 | 1980 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - | 1981 | `			"Cannot mix keyed and unkeyed array entries in assignments");` |
|      ! 0 | 1982 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 1983 | `	}` |
|      201 | 1984 | `	if( nKeyed > 0 ){` |
|       36 | 1985 | `		return GenStateCompileKeyedListBody(pGen);` |
|        - | 1986 | `	}` |
|      167 | 1987 | `	nExpr = 0;` |
|      167 | 1988 | `	SySetInit(&sNested,&pGen->pVm->sAllocator,sizeof(struct NestedListEntry));` |
|      455 | 1989 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|      293 | 1990 | `		if( pGen->pIn < pNext ){` |
|        - | 1991 | `			/* Check for nested list() */` |
|      281 | 1992 | `			if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        3 | 1993 | `				SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|        - | 1994 | `				/* Record this nested list for post-processing */` |
|        3 | 1995 | `				SyToken *pListEnd = 0;` |
|        3 | 1996 | `				if( &pGen->pIn[1] < pNext && (pGen->pIn[1].nType & PH7_TK_LPAREN) ){` |
|        3 | 1997 | `					PH7_DelimitNestedTokens(pGen->pIn+2,pNext,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|        1 | 1998 | `				}` |
|        3 | 1999 | `				if( pListEnd ){` |
|        - | 2000 | `					struct NestedListEntry sEntry;` |
|        3 | 2001 | `					sEntry.nIndex = nExpr;` |
|        3 | 2002 | `					sEntry.pStart = pGen->pIn;` |
|        3 | 2003 | `					sEntry.pEnd = pListEnd + 1;` |
|        3 | 2004 | `					sEntry.isShort = 0;` |
|        3 | 2005 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|        1 | 2006 | `				}` |
|        - | 2007 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|        3 | 2008 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|      280 | 2009 | `			}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|        - | 2010 | `				/* Nested short destructuring [...] */` |
|       16 | 2011 | `				SyToken *pBracketEnd = 0;` |
|       16 | 2012 | `				PH7_DelimitNestedTokens(pGen->pIn+1,pNext,PH7_TK_OSB,PH7_TK_CSB,&pBracketEnd);` |
|       16 | 2013 | `				if( pBracketEnd ){` |
|        - | 2014 | `					struct NestedListEntry sEntry;` |
|       16 | 2015 | `					sEntry.nIndex = nExpr;` |
|       16 | 2016 | `					sEntry.pStart = pGen->pIn;` |
|       16 | 2017 | `					sEntry.pEnd = pBracketEnd + 1;` |
|       16 | 2018 | `					sEntry.isShort = 1;` |
|       16 | 2019 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|        7 | 2020 | `				}` |
|        - | 2021 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|       16 | 2022 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|        9 | 2023 | `			}else{` |
|        - | 2024 | `				/* Compile the expression holding the variable */` |
|      265 | 2025 | `				rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|      265 | 2026 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 2027 | `					SySetRelease(&sNested);` |
|      ! 0 | 2028 | `					return SXRET_OK;` |
|        - | 2029 | `				}` |
|        - | 2030 | `				{` |
|        - | 2031 | `					/* A property target ($o->p / Cls::$s) is a PURE WRITE here — the` |
|        - | 2032 | `					 * value lands via the following OP_LOAD_LIST's direct slot store.` |
|        - | 2033 | `					 * Tag the member so OP_MEMBER skips the uninitialized-typed read` |
|        - | 2034 | `					 * Error / __get consult and vivifies a missing property (php` |
|        - | 2035 | `					 * assigns without reading). */` |
|      265 | 2036 | `					VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|      265 | 2037 | `					if( pLast && pLast->iOp == PH7_OP_MEMBER && pLast->iP2 == PH7_MEMBER_READ ){` |
|       41 | 2038 | `						pLast->iP2 = PH7_MEMBER_LIST_TARGET;` |
|       20 | 2039 | `					}` |
|        - | 2040 | `				}` |
|        - | 2041 | `			}` |
|      143 | 2042 | `		}else{` |
|        - | 2043 | `			/* Empty entry,load NULL */` |
|       13 | 2044 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);` |
|        - | 2045 | `		}` |
|      293 | 2046 | `		nExpr++;` |
|        - | 2047 | `		/* Advance the stream cursor */` |
|      293 | 2048 | `		pGen->pIn = &pNext[1];` |
|        5 | 2049 | `	}` |
|        - | 2050 | `	/* Emit the LOAD_LIST instruction */` |
|      167 | 2051 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,0,0,0);` |
|        - | 2052 | `	/* After LOAD_LIST, the source array is still on the stack top.` |
|        - | 2053 | `	 * For each nested entry, emit code to extract the sub-array` |
|        - | 2054 | `	 * at the corresponding index and recursively destructure it.` |
|        - | 2055 | `	 */` |
|      167 | 2056 | `	if( SySetUsed(&sNested) > 0 ){` |
|       16 | 2057 | `		struct NestedListEntry *apNested = (struct NestedListEntry *)SySetBasePtr(&sNested);` |
|        - | 2058 | `		sxu32 i;` |
|       32 | 2059 | `		for(i = 0; i < SySetUsed(&sNested); i++){` |
|       18 | 2060 | `			SyToken *pSavedIn = pGen->pIn;` |
|       18 | 2061 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|        - | 2062 | `			ph7_value *pIdx;` |
|        - | 2063 | `			sxu32 nConstIdx;` |
|        - | 2064 | `			/* DUP the source array (it's on stack top) */` |
|       18 | 2065 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|        - | 2066 | `			/* Push the integer index for this nested entry */` |
|       18 | 2067 | `			pIdx = PH7_ReserveConstObj(pGen->pVm,&nConstIdx);` |
|       18 | 2068 | `			if( pIdx == 0 ){` |
|      ! 0 | 2069 | `				PH7_GenCompileError(&(*pGen),E_ERROR,0,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 2070 | `				SySetRelease(&sNested);` |
|      ! 0 | 2071 | `				return SXERR_ABORT;` |
|        - | 2072 | `			}` |
|       18 | 2073 | `			PH7_MemObjInitFromInt(pGen->pVm,pIdx,(sxi64)apNested[i].nIndex);` |
|       18 | 2074 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nConstIdx,0,0);` |
|        - | 2075 | `			/* LOAD_IDX: pop index, replace DUP'd source with source[index].` |
|        - | 2076 | `			 * iP2=2 signals the VM to emit an "Undefined array key" warning` |
|        - | 2077 | `			 * when the key is missing (PHP-compatible list destructuring).` |
|        - | 2078 | `			 */` |
|       18 | 2079 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,2,0,0);` |
|        - | 2080 | `			/* Recursively compile the inner list */` |
|       18 | 2081 | `			pGen->pIn = apNested[i].pStart;` |
|       18 | 2082 | `			pGen->pEnd = apNested[i].pEnd;` |
|       18 | 2083 | `			if( apNested[i].isShort ){` |
|       16 | 2084 | `				rc = PH7_CompileShortList(&(*pGen),0);` |
|        9 | 2085 | `			}else{` |
|        3 | 2086 | `				rc = PH7_CompileList(&(*pGen),0);` |
|        - | 2087 | `			}` |
|       18 | 2088 | `			pGen->pIn = pSavedIn;` |
|       18 | 2089 | `			pGen->pEnd = pSavedEnd;` |
|       18 | 2090 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2091 | `				SySetRelease(&sNested);` |
|      ! 0 | 2092 | `				return SXERR_ABORT;` |
|        - | 2093 | `			}` |
|        - | 2094 | `			/* Pop the leftover source[index] from the inner LOAD_LIST */` |
|       18 | 2095 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       10 | 2096 | `		}` |
|        7 | 2097 | `	}` |
|      167 | 2098 | `	SySetRelease(&sNested);` |
|        - | 2099 | `	/* Node successfully compiled */` |
|      167 | 2100 | `	return SXRET_OK;` |
|      103 | 2101 | `}` |
|       46 | 2102 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 | 2103 | `{` |
|        - | 2104 | `	/* Jump the 'list' keyword, the leading '(' and exclude trailing ')' */` |
|       51 | 2105 | `	pGen->pIn += 2;` |
|       51 | 2106 | `	pGen->pEnd--;` |
|       23 | 2107 | `	SXUNUSED(iCompileFlag);` |
|       51 | 2108 | `	return GenStateCompileListBody(pGen);` |
|        5 | 2109 | `}` |
|      150 | 2110 | `PH7_PRIVATE sxi32 PH7_CompileShortList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 | 2111 | `{` |
|        - | 2112 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|      155 | 2113 | `	pGen->pIn++;` |
|      155 | 2114 | `	pGen->pEnd--;` |
|       75 | 2115 | `	SXUNUSED(iCompileFlag);` |
|      155 | 2116 | `	return GenStateCompileListBody(pGen);` |
|        5 | 2117 | `}` |
|        - | 2118 | `/*` |
|        - | 2119 | ` * assert() source-text rendering.` |
|        - | 2120 | ` *` |
|        - | 2121 | ` * php compiles a DIRECT assert() call with a copy of the argument's AST, and a` |
|        - | 2122 | `` * failing assertion reports zend_ast_export() of that AST — `assert(1 == 2)`,`` |
|        - | 2123 | `` * `assert($x)`, `assert('')` — which is exactly the information the message`` |
|        - | 2124 | ` * exists to carry. PHL has no AST copy at runtime, so the compiler renders the` |
|        - | 2125 | ` * argument's TOKEN SPAN here, at compile time, normalizing to php's export` |
|        - | 2126 | ` * shape (each rule probed against php 8.5):` |
|        - | 2127 | ` *   - literal values fold the way php's AST holds them: numbers render from` |
|        - | 2128 | ` *     their parsed VALUE (0x10 -> 16, 1e3 -> 1000.0, 1_000 -> 1000, an` |
|        - | 2129 | ` *     int64-overflowing literal -> float), strings render single-quoted with` |
|        - | 2130 | ` *     their PROCESSED contents (\ and ' re-escaped), array(...) -> [...].` |
|        - | 2131 | ` *   - one space around binary operators, ", " between arguments/elements, no` |
|        - | 2132 | `` *     space inside ()/[] or around ->/?->/::/casts, `and`/`or` -> `&&`/`\|\|`,`` |
|        - | 2133 | ` *     a trailing comma is dropped, redundant OUTERMOST parens are dropped.` |
|        - | 2134 | ` * Accepted divergences from zend_ast_export on exotic input (message text` |
|        - | 2135 | ` * only, never behavior): redundant INNER parens are kept (php re-derives` |
|        - | 2136 | ` * grouping from precedence), interpolated "$x" strings and heredocs render as` |
|        - | 2137 | `` * written (php exports its interpolation AST), `new C` does not grow php's`` |
|        - | 2138 | `` * trailing `()`, and constant folding beyond single literals is not applied`` |
|        - | 2139 | `` * (php renders `'' . ''` as `''`).`` |
|        - | 2140 | ` */` |
|        - | 2141 | `/* Spacing classes: a space is inserted between two tokens when either side` |
|        - | 2142 | ` * FORCEs one (binary operators, the slot after a comma) or both sides are` |
|        - | 2143 | ` * operand-like (WANT). Grouping punctuation and glue operators contribute` |
|        - | 2144 | ` * NONE on their tight side. */` |
|        - | 2145 | `#define ASRT_SP_NONE  0` |
|        - | 2146 | `#define ASRT_SP_WANT  1` |
|        - | 2147 | `#define ASRT_SP_FORCE 2` |
|        - | 2148 | `enum AssertTokClass {` |
|        - | 2149 | `	ASRT_START = 0, /* virtual class before the first token */` |
|        - | 2150 | `	ASRT_OPERAND,   /* literals, identifiers, keywords */` |
|        - | 2151 | `	ASRT_BINOP,     /* == + . && ? : => instanceof ... */` |
|        - | 2152 | `	ASRT_UNARY,     /* ! ~ @ - + & casts, '$', '...' — glue after */` |
|        - | 2153 | `	ASRT_OPEN,      /* ( [ */` |
|        - | 2154 | `	ASRT_CLOSE,     /* ) ] */` |
|        - | 2155 | `	ASRT_GLUE,      /* -> ?-> :: ++ -- \ — glue both sides */` |
|        - | 2156 | `	ASRT_COMMA      /* , — glue before, force after */` |
|        - | 2157 | `};` |
|        - | 2158 | `static const sxu8 aAsrtBefore[] = { ASRT_SP_NONE, ASRT_SP_WANT, ASRT_SP_FORCE, ASRT_SP_WANT,` |
|        - | 2159 | `	ASRT_SP_NONE, ASRT_SP_NONE, ASRT_SP_NONE, ASRT_SP_NONE };` |
|        - | 2160 | `static const sxu8 aAsrtAfter[]  = { ASRT_SP_NONE, ASRT_SP_WANT, ASRT_SP_FORCE, ASRT_SP_NONE,` |
|        - | 2161 | `	ASRT_SP_NONE, ASRT_SP_WANT, ASRT_SP_NONE, ASRT_SP_FORCE };` |
|        - | 2162 | `/*` |
|        - | 2163 | ` * Append one PROCESSED string-value byte, re-escaped for a single-quoted` |
|        - | 2164 | ` * rendering: php's export escapes only backslash and the quote itself; every` |
|        - | 2165 | ` * other byte (including control characters) is emitted raw.` |
|        - | 2166 | ` */` |
|       42 | 2167 | `static void AssertRenderQuotedByte(SyBlob *pOut,int c)` |
|        2 | 2168 | `{` |
|       44 | 2169 | `	char ch = (char)c;` |
|       44 | 2170 | `	if( c == '\\' \|\| c == '\'' ){` |
|        3 | 2171 | `		SyBlobAppend(pOut,"\\",1);` |
|        1 | 2172 | `	}` |
|       44 | 2173 | `	SyBlobAppend(pOut,&ch,1);` |
|       44 | 2174 | `}` |
|        - | 2175 | `/* Append the UTF-8 encoding of a \u{...} code point (value bytes, re-escaped). */` |
|      ! 0 | 2176 | `static void AssertRenderUtf8(SyBlob *pOut,sxu32 c)` |
|      ! 0 | 2177 | `{` |
|      ! 0 | 2178 | `	if( c < 0x80 ){` |
|      ! 0 | 2179 | `		AssertRenderQuotedByte(pOut,(int)c);` |
|      ! 0 | 2180 | `	}else if( c < 0x800 ){` |
|      ! 0 | 2181 | `		AssertRenderQuotedByte(pOut,(int)(0xc0 \| (c >> 6)));` |
|      ! 0 | 2182 | `		AssertRenderQuotedByte(pOut,(int)(0x80 \| (c & 0x3f)));` |
|      ! 0 | 2183 | `	}else if( c < 0x10000 ){` |
|      ! 0 | 2184 | `		AssertRenderQuotedByte(pOut,(int)(0xe0 \| (c >> 12)));` |
|      ! 0 | 2185 | `		AssertRenderQuotedByte(pOut,(int)(0x80 \| ((c >> 6) & 0x3f)));` |
|      ! 0 | 2186 | `		AssertRenderQuotedByte(pOut,(int)(0x80 \| (c & 0x3f)));` |
|      ! 0 | 2187 | `	}else{` |
|      ! 0 | 2188 | `		AssertRenderQuotedByte(pOut,(int)(0xf0 \| (c >> 18)));` |
|      ! 0 | 2189 | `		AssertRenderQuotedByte(pOut,(int)(0x80 \| ((c >> 12) & 0x3f)));` |
|      ! 0 | 2190 | `		AssertRenderQuotedByte(pOut,(int)(0x80 \| ((c >> 6) & 0x3f)));` |
|      ! 0 | 2191 | `		AssertRenderQuotedByte(pOut,(int)(0x80 \| (c & 0x3f)));` |
|        - | 2192 | `	}` |
|      ! 0 | 2193 | `}` |
|        - | 2194 | `/*` |
|        - | 2195 | ` * Render a single-quoted-source string (or nowdoc body): only \\ and \' are` |
|        - | 2196 | ` * escape sequences there; any other backslash is a literal byte.` |
|        - | 2197 | ` */` |
|        2 | 2198 | `static void AssertRenderSglString(SyBlob *pOut,const char *z,sxu32 n)` |
|        1 | 2199 | `{` |
|        3 | 2200 | `	sxu32 i = 0;` |
|        3 | 2201 | `	SyBlobAppend(pOut,"'",1);` |
|       11 | 2202 | `	while( i < n ){` |
|        9 | 2203 | `		if( z[i] == '\\' && i + 1 < n && (z[i+1] == '\\' \|\| z[i+1] == '\'') ){` |
|        3 | 2204 | `			AssertRenderQuotedByte(pOut,z[i+1]);` |
|        3 | 2205 | `			i += 2;` |
|        2 | 2206 | `		}else{` |
|        7 | 2207 | `			AssertRenderQuotedByte(pOut,z[i]);` |
|        7 | 2208 | `			i++;` |
|        - | 2209 | `		}` |
|        1 | 2210 | `	}` |
|        3 | 2211 | `	SyBlobAppend(pOut,"'",1);` |
|        3 | 2212 | `}` |
|        - | 2213 | `/*` |
|        - | 2214 | ` * Render a double-quoted-source string (or heredoc body) with php's escape` |
|        - | 2215 | ` * processing — the value bytes are what php's AST holds, and the export prints` |
|        - | 2216 | ` * them single-quoted. An UNKNOWN escape keeps the backslash and the character,` |
|        - | 2217 | ` * matching php's string semantics.` |
|        - | 2218 | ` */` |
|       12 | 2219 | `static void AssertRenderDblString(SyBlob *pOut,const char *z,sxu32 n)` |
|        3 | 2220 | `{` |
|       15 | 2221 | `	sxu32 i = 0;` |
|       15 | 2222 | `	SyBlobAppend(pOut,"'",1);` |
|       49 | 2223 | `	while( i < n ){` |
|       36 | 2224 | `		int c = z[i];` |
|        - | 2225 | `		int d;` |
|       36 | 2226 | `		if( c != '\\' \|\| i + 1 >= n ){` |
|       36 | 2227 | `			AssertRenderQuotedByte(pOut,c);` |
|       36 | 2228 | `			i++;` |
|       36 | 2229 | `			continue;` |
|        - | 2230 | `		}` |
|      ! 0 | 2231 | `		d = z[i+1];` |
|      ! 0 | 2232 | `		i += 2;` |
|      ! 0 | 2233 | `		switch(d){` |
|      ! 0 | 2234 | `		case 'n': AssertRenderQuotedByte(pOut,'\n'); break;` |
|      ! 0 | 2235 | `		case 't': AssertRenderQuotedByte(pOut,'\t'); break;` |
|      ! 0 | 2236 | `		case 'r': AssertRenderQuotedByte(pOut,'\r'); break;` |
|      ! 0 | 2237 | `		case 'v': AssertRenderQuotedByte(pOut,'\v'); break;` |
|      ! 0 | 2238 | `		case 'f': AssertRenderQuotedByte(pOut,'\f'); break;` |
|      ! 0 | 2239 | `		case 'e': AssertRenderQuotedByte(pOut,0x1b); break;` |
|      ! 0 | 2240 | `		case '\\': AssertRenderQuotedByte(pOut,'\\'); break;` |
|      ! 0 | 2241 | `		case '"': AssertRenderQuotedByte(pOut,'"'); break;` |
|      ! 0 | 2242 | `		case '$': AssertRenderQuotedByte(pOut,'$'); break;` |
|      ! 0 | 2243 | `		case 'x': case 'X': {` |
|        - | 2244 | `			/* Up to two hex digits; a bare \x is literal. */` |
|      ! 0 | 2245 | `			int nHex = 0, v = 0;` |
|      ! 0 | 2246 | `			while( nHex < 2 && i < n && (unsigned char)z[i] < 0x80 && SyisHex((unsigned char)z[i]) ){` |
|      ! 0 | 2247 | `				v = (v << 4) \| SyHexToint((unsigned char)z[i]);` |
|      ! 0 | 2248 | `				i++; nHex++;` |
|      ! 0 | 2249 | `			}` |
|      ! 0 | 2250 | `			if( nHex > 0 ){` |
|      ! 0 | 2251 | `				AssertRenderQuotedByte(pOut,v);` |
|      ! 0 | 2252 | `			}else{` |
|      ! 0 | 2253 | `				AssertRenderQuotedByte(pOut,'\\');` |
|      ! 0 | 2254 | `				AssertRenderQuotedByte(pOut,d);` |
|        - | 2255 | `			}` |
|      ! 0 | 2256 | `			break;` |
|        - | 2257 | `		}` |
|      ! 0 | 2258 | `		case 'u': {` |
|        - | 2259 | `			/* \u{HEX+} — anything else keeps the backslash (php). */` |
|      ! 0 | 2260 | `			if( i < n && z[i] == '{' ){` |
|      ! 0 | 2261 | `				sxu32 v = 0; sxu32 j = i + 1; int nHex = 0;` |
|      ! 0 | 2262 | `				while( j < n && (unsigned char)z[j] < 0x80 && SyisHex((unsigned char)z[j]) && nHex < 8 ){` |
|      ! 0 | 2263 | `					v = (v << 4) \| (sxu32)SyHexToint((unsigned char)z[j]);` |
|      ! 0 | 2264 | `					j++; nHex++;` |
|      ! 0 | 2265 | `				}` |
|      ! 0 | 2266 | `				if( nHex > 0 && j < n && z[j] == '}' ){` |
|      ! 0 | 2267 | `					AssertRenderUtf8(pOut,v);` |
|      ! 0 | 2268 | `					i = j + 1;` |
|      ! 0 | 2269 | `					break;` |
|        - | 2270 | `				}` |
|      ! 0 | 2271 | `			}` |
|      ! 0 | 2272 | `			AssertRenderQuotedByte(pOut,'\\');` |
|      ! 0 | 2273 | `			AssertRenderQuotedByte(pOut,d);` |
|      ! 0 | 2274 | `			break;` |
|        - | 2275 | `		}` |
|      ! 0 | 2276 | `		default:` |
|      ! 0 | 2277 | `			if( d >= '0' && d <= '7' ){` |
|        - | 2278 | `				/* Up to three octal digits (the first was d). */` |
|      ! 0 | 2279 | `				int nOct = 1, v = d - '0';` |
|      ! 0 | 2280 | `				while( nOct < 3 && i < n && z[i] >= '0' && z[i] <= '7' ){` |
|      ! 0 | 2281 | `					v = (v << 3) \| (z[i] - '0');` |
|      ! 0 | 2282 | `					i++; nOct++;` |
|      ! 0 | 2283 | `				}` |
|      ! 0 | 2284 | `				AssertRenderQuotedByte(pOut,v & 0xff);` |
|      ! 0 | 2285 | `			}else{` |
|      ! 0 | 2286 | `				AssertRenderQuotedByte(pOut,'\\');` |
|      ! 0 | 2287 | `				AssertRenderQuotedByte(pOut,d);` |
|        - | 2288 | `			}` |
|      ! 0 | 2289 | `			break;` |
|        - | 2290 | `		}` |
|      ! 0 | 2291 | `	}` |
|       15 | 2292 | `	SyBlobAppend(pOut,"'",1);` |
|       15 | 2293 | `}` |
|        - | 2294 | `/*` |
|        - | 2295 | ` * Append a double in php's AST-export shape: the shortest round-tripping` |
|        - | 2296 | ` * decimal, with a forced ".0" fraction when the digits alone look integral` |
|        - | 2297 | ` * (1e3 -> "1000.0", 1e20 -> "1.0E+20") — the var_export float shape.` |
|        - | 2298 | ` */` |
|        8 | 2299 | `static void AssertRenderReal(SyBlob *pOut,ph7_real rVal)` |
|        1 | 2300 | `{` |
|        - | 2301 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - | 2302 | `	/* No floating point: ph7_real IS sxi64, there is no shortest-round-trip` |
|        - | 2303 | `	 * decimal to search for and no ".0" to force, so the value renders as the` |
|        - | 2304 | `	 * integer it is -- the same shape the INTEGER arm below emits. Taking` |
|        - | 2305 | `	 * ph7_real rather than double is what keeps the two call sites from` |
|        - | 2306 | `	 * narrowing (MSVC /W4 makes that C4244, and /WX makes it an error). */` |
|        - | 2307 | `	SyBlobFormat(pOut,"%qd",(sxi64)rVal);` |
|        - | 2308 | `#else` |
|        9 | 2309 | `	sxu32 nBefore = SyBlobLength(pOut);` |
|        - | 2310 | `	const char *zOut;` |
|        - | 2311 | `	sxu32 i, nAfter;` |
|        9 | 2312 | `	int bPlain = 1;` |
|        9 | 2313 | `	PH7_AppendShortestReal(pOut,rVal);` |
|        9 | 2314 | `	zOut = (const char *)SyBlobData(pOut);` |
|        9 | 2315 | `	nAfter = SyBlobLength(pOut);` |
|       23 | 2316 | `	for( i = nBefore; i < nAfter; i++ ){` |
|       21 | 2317 | `		if( !((zOut[i] >= '0' && zOut[i] <= '9') \|\| zOut[i] == '-') ){` |
|        7 | 2318 | `			bPlain = 0;` |
|        7 | 2319 | `			break;` |
|        - | 2320 | `		}` |
|        8 | 2321 | `	}` |
|        9 | 2322 | `	if( bPlain ){` |
|        3 | 2323 | `		SyBlobAppend(pOut,".0",2);` |
|        1 | 2324 | `	}` |
|        - | 2325 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|        9 | 2326 | `}` |
|        - | 2327 | `/*` |
|        - | 2328 | ` * Render the token span [pIn, pEnd) — a direct assert() call's first argument —` |
|        - | 2329 | ` * into pOut in php's zend_ast_export shape (see the block comment above).` |
|        - | 2330 | ` * Total: every span renders to SOMETHING (unknown constructs fall back to` |
|        - | 2331 | ` * their raw token text), so the capture never aborts a compile.` |
|        - | 2332 | ` */` |
|       62 | 2333 | `PH7_PRIVATE void PH7_GenRenderAssertSpan(ph7_gen_state *pGen,SyToken *pIn,SyToken *pEnd,SyBlob *pOut)` |
|        5 | 2334 | `{` |
|        - | 2335 | `	sxu8 aParen[64]; /* 1 = this '(' depth is an array(...) literal rendered as [...] */` |
|       67 | 2336 | `	sxu32 nParen = 0;` |
|       67 | 2337 | `	int iPrev = ASRT_START;` |
|       67 | 2338 | ``	int bArrayOpen = 0; /* the next '(' belongs to a suppressed `array` keyword */`` |
|        - | 2339 | `	/* php drops every redundant paren when re-deriving source from the AST;` |
|        - | 2340 | `	 * dropping the OUTERMOST pair(s) is the token-level equivalent for the` |
|        - | 2341 | ``	 * common `assert((...))` spelling. */`` |
|       69 | 2342 | `	while( pIn < pEnd - 1 && (pIn->nType & PH7_TK_LPAREN) && (pEnd[-1].nType & PH7_TK_RPAREN) ){` |
|        - | 2343 | `		SyToken *p;` |
|        3 | 2344 | `		sxi32 iDepth = 0;` |
|        3 | 2345 | `		SyToken *pMatch = 0;` |
|       11 | 2346 | `		for( p = pIn; p < pEnd; p++ ){` |
|       11 | 2347 | `			if( p->nType & PH7_TK_LPAREN ){` |
|        3 | 2348 | `				iDepth++;` |
|       10 | 2349 | `			}else if( p->nType & PH7_TK_RPAREN ){` |
|        3 | 2350 | `				iDepth--;` |
|        3 | 2351 | `				if( iDepth == 0 ){ pMatch = p; break; }` |
|      ! 0 | 2352 | `			}` |
|        5 | 2353 | `		}` |
|        3 | 2354 | `		if( pMatch != &pEnd[-1] ){` |
|      ! 0 | 2355 | `			break;` |
|        - | 2356 | `		}` |
|        3 | 2357 | `		pIn++;` |
|        3 | 2358 | `		pEnd--;` |
|        1 | 2359 | `	}` |
|      267 | 2360 | `	for( ; pIn < pEnd ; pIn++ ){` |
|      205 | 2361 | `		SyToken *pTok = pIn;` |
|      205 | 2362 | `		const char *zTxt = pTok->sData.zString;` |
|      205 | 2363 | `		sxu32 nTxt = pTok->sData.nByte;` |
|        - | 2364 | `		int iCls;` |
|        - | 2365 | `		sxu32 nMark;` |
|        - | 2366 | `		/* --- classify + pre-token handling ------------------------------ */` |
|      205 | 2367 | `		if( pTok->nType & PH7_TK_LPAREN ){` |
|       15 | 2368 | `			iCls = ASRT_OPEN;` |
|      199 | 2369 | `		}else if( pTok->nType & PH7_TK_RPAREN ){` |
|       15 | 2370 | `			iCls = ASRT_CLOSE;` |
|      187 | 2371 | `		}else if( pTok->nType & (PH7_TK_OSB\|PH7_TK_CSB) ){` |
|       13 | 2372 | `			iCls = (pTok->nType & PH7_TK_OSB) ? ASRT_OPEN : ASRT_CLOSE;` |
|      175 | 2373 | `		}else if( pTok->nType & PH7_TK_COMMA ){` |
|        - | 2374 | `			/* php's export never prints a trailing comma. */` |
|        7 | 2375 | `			if( &pIn[1] < pEnd && (pIn[1].nType & (PH7_TK_RPAREN\|PH7_TK_CSB)) ){` |
|      ! 0 | 2376 | `				continue;` |
|        - | 2377 | `			}` |
|        7 | 2378 | `			iCls = ASRT_COMMA;` |
|      166 | 2379 | `		}else if( pTok->nType & PH7_TK_DOLLAR ){` |
|        3 | 2380 | `			iCls = ASRT_UNARY; /* operand-like before, glued to its name after */` |
|      162 | 2381 | `		}else if( pTok->nType & PH7_TK_NSSEP ){` |
|      ! 0 | 2382 | `			iCls = ASRT_GLUE;` |
|      161 | 2383 | `		}else if( pTok->nType & PH7_TK_ELLIPSIS ){` |
|      ! 0 | 2384 | `			iCls = ASRT_UNARY;` |
|      161 | 2385 | `		}else if( pTok->nType & PH7_TK_OP ){` |
|       46 | 2386 | `			iCls = ASRT_BINOP;` |
|       46 | 2387 | `			if( nTxt > 0 ){` |
|       46 | 2388 | `				int c0 = zTxt[0];` |
|       46 | 2389 | `				if( c0 == '(' ){` |
|      ! 0 | 2390 | ``					iCls = ASRT_UNARY; /* lexer-merged cast token `(int)` */`` |
|       60 | 2391 | `				}else if( nTxt == 2 && (SyMemcmp(zTxt,"->",2) == 0 \|\| SyMemcmp(zTxt,"::",2) == 0` |
|       28 | 2392 | `						\|\| SyMemcmp(zTxt,"++",2) == 0 \|\| SyMemcmp(zTxt,"--",2) == 0) ){` |
|      ! 0 | 2393 | `					iCls = ASRT_GLUE;` |
|       46 | 2394 | `				}else if( nTxt == 3 && SyMemcmp(zTxt,"?->",3) == 0 ){` |
|      ! 0 | 2395 | `					iCls = ASRT_GLUE;` |
|       46 | 2396 | `				}else if( nTxt == 1 && (c0 == '!' \|\| c0 == '~' \|\| c0 == '@') ){` |
|        3 | 2397 | `					iCls = ASRT_UNARY;` |
|       45 | 2398 | `				}else if( nTxt == 1 && (c0 == '-' \|\| c0 == '+' \|\| c0 == '&') ){` |
|        - | 2399 | `					/* Unary when nothing operand-like precedes. */` |
|        4 | 2400 | `					if( iPrev == ASRT_START \|\| iPrev == ASRT_BINOP \|\| iPrev == ASRT_UNARY` |
|        3 | 2401 | `					 \|\| iPrev == ASRT_OPEN \|\| iPrev == ASRT_COMMA ){` |
|        3 | 2402 | `						iCls = ASRT_UNARY;` |
|        2 | 2403 | `					}` |
|       42 | 2404 | `				}else if( pTok->nType & PH7_TK_ID ){` |
|        - | 2405 | `					/* Alpha operators: and/or normalize to php's export spelling;` |
|        - | 2406 | `					 * new/clone read as prefix keywords (operand spacing). */` |
|        3 | 2407 | `					if( nTxt == 3 && SyStrnicmp(zTxt,"and",3) == 0 ){` |
|        3 | 2408 | `						zTxt = "&&"; nTxt = 2;` |
|        1 | 2409 | `					}else if( nTxt == 2 && SyStrnicmp(zTxt,"or",2) == 0 ){` |
|      ! 0 | 2410 | `						zTxt = "\|\|"; nTxt = 2;` |
|      ! 0 | 2411 | `					}else if( (nTxt == 3 && SyStrnicmp(zTxt,"new",3) == 0)` |
|      ! 0 | 2412 | `						\|\| (nTxt == 5 && SyStrnicmp(zTxt,"clone",5) == 0) ){` |
|      ! 0 | 2413 | `						iCls = ASRT_OPERAND;` |
|      ! 0 | 2414 | `					}` |
|        1 | 2415 | `				}` |
|       24 | 2416 | `			}` |
|      139 | 2417 | `		}else if( pTok->nType & (PH7_TK_EQUAL\|PH7_TK_ARRAY_OP\|PH7_TK_COLON\|PH7_TK_AMPER) ){` |
|      ! 0 | 2418 | `			iCls = ASRT_BINOP;` |
|      117 | 2419 | `		}else if( pTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|       35 | 2420 | `			iCls = ASRT_OPERAND;` |
|        - | 2421 | ``			/* `array` `(` — php's AST holds one list node for both spellings and`` |
|        - | 2422 | ``			 * always exports `[...]`. Suppress the keyword (it lexes as a KEYWORD`` |
|        - | 2423 | `			 * token, not an ID); the '(' renders '['. */` |
|       30 | 2424 | `			if( nTxt == 5 && SyStrnicmp(zTxt,"array",5) == 0` |
|       17 | 2425 | `			 && &pIn[1] < pEnd && (pIn[1].nType & PH7_TK_LPAREN) ){` |
|        9 | 2426 | `				bArrayOpen = 1;` |
|        9 | 2427 | `				continue;` |
|        - | 2428 | `			}` |
|       16 | 2429 | `		}else{` |
|        - | 2430 | `			/* keywords (true/false/null/fn/match/...), numbers, strings,` |
|        - | 2431 | `			 * member names, '{'/'}' and anything unforeseen */` |
|       86 | 2432 | `			iCls = ASRT_OPERAND;` |
|        - | 2433 | `		}` |
|        - | 2434 | ``		/* Elvis `? :` — php exports the two-token form as `?:`. */`` |
|      194 | 2435 | `		if( iCls == ASRT_BINOP && nTxt == 1 && zTxt[0] == '?'` |
|       11 | 2436 | `		 && &pIn[1] < pEnd && (pIn[1].nType & PH7_TK_COLON) ){` |
|        3 | 2437 | `			nMark = SyBlobLength(pOut);` |
|        3 | 2438 | `			if( iPrev != ASRT_START && nMark > 0 ){` |
|        3 | 2439 | `				SyBlobAppend(pOut," ",1);` |
|        1 | 2440 | `			}` |
|        3 | 2441 | `			SyBlobAppend(pOut,"?:",2);` |
|        3 | 2442 | `			pIn++; /* consume the ':' */` |
|        3 | 2443 | `			iPrev = ASRT_BINOP;` |
|        3 | 2444 | `			continue;` |
|        - | 2445 | `		}` |
|        - | 2446 | `		/* --- spacing ---------------------------------------------------- */` |
|      197 | 2447 | `		if( iPrev != ASRT_START ){` |
|      134 | 2448 | `			int iAfter = aAsrtAfter[iPrev];` |
|      134 | 2449 | `			int iBefore = aAsrtBefore[iCls];` |
|      130 | 2450 | `			if( iAfter == ASRT_SP_FORCE \|\| iBefore == ASRT_SP_FORCE` |
|       69 | 2451 | `			 \|\| (iAfter == ASRT_SP_WANT && iBefore == ASRT_SP_WANT) ){` |
|       86 | 2452 | `				SyBlobAppend(pOut," ",1);` |
|       42 | 2453 | `			}` |
|       65 | 2454 | `		}` |
|        - | 2455 | `		/* --- emit ------------------------------------------------------- */` |
|      197 | 2456 | `		if( pTok->nType & PH7_TK_LPAREN ){` |
|       15 | 2457 | `			if( nParen < sizeof(aParen) ){` |
|       15 | 2458 | `				aParen[nParen] = (sxu8)bArrayOpen;` |
|        6 | 2459 | `			}` |
|       15 | 2460 | `			nParen++;` |
|       15 | 2461 | `			SyBlobAppend(pOut,bArrayOpen ? "[" : "(",1);` |
|       15 | 2462 | `			bArrayOpen = 0;` |
|      191 | 2463 | `		}else if( pTok->nType & PH7_TK_RPAREN ){` |
|       15 | 2464 | `			int bArr = 0;` |
|       15 | 2465 | `			if( nParen > 0 ){` |
|       15 | 2466 | `				nParen--;` |
|       15 | 2467 | `				if( nParen < sizeof(aParen) ){` |
|       15 | 2468 | `					bArr = aParen[nParen];` |
|        6 | 2469 | `				}` |
|        6 | 2470 | `			}` |
|       15 | 2471 | `			SyBlobAppend(pOut,bArr ? "]" : ")",1);` |
|      178 | 2472 | `		}else if( pTok->nType & (PH7_TK_INTEGER\|PH7_TK_REAL) ){` |
|        - | 2473 | `			char zScratch[GEN_NUM_SCRATCH];` |
|       71 | 2474 | `			char *zAlloc = 0;` |
|        - | 2475 | `			SyString sNum;` |
|      102 | 2476 | `			if( GenStateStripNumericSeparators(&pGen->pVm->sAllocator,&pTok->sData,` |
|       71 | 2477 | `					zScratch,sizeof(zScratch),&sNum,&zAlloc) != SXRET_OK ){` |
|      ! 0 | 2478 | `				SyBlobAppend(pOut,zTxt,nTxt); /* alloc failure: raw text */` |
|       71 | 2479 | `			}else if( pTok->nType & PH7_TK_INTEGER ){` |
|       63 | 2480 | `				ph7_real rOverflow = 0;` |
|       63 | 2481 | `				int bDecimalOverflow = 0;` |
|       63 | 2482 | `				if( GenStateIntLiteralOverflows(&sNum,&rOverflow,&bDecimalOverflow) ){` |
|      ! 0 | 2483 | `					if( bDecimalOverflow ){` |
|      ! 0 | 2484 | `						SyStrToReal(sNum.zString,sNum.nByte,(void *)&rOverflow,0);` |
|      ! 0 | 2485 | `					}` |
|      ! 0 | 2486 | `					AssertRenderReal(pOut,rOverflow);` |
|      ! 0 | 2487 | `				}else{` |
|       63 | 2488 | `					SyBlobFormat(pOut,"%qd",PH7_TokenValueToInt64(&sNum));` |
|        - | 2489 | `				}` |
|       33 | 2490 | `			}else{` |
|        9 | 2491 | `				ph7_real rVal = 0;` |
|        9 | 2492 | `				SyStrToReal(sNum.zString,sNum.nByte,(void *)&rVal,0);` |
|        9 | 2493 | `				AssertRenderReal(pOut,rVal);` |
|        - | 2494 | `			}` |
|       71 | 2495 | `			if( zAlloc ){` |
|      ! 0 | 2496 | `				SyMemBackendFree(&pGen->pVm->sAllocator,zAlloc);` |
|        3 | 2497 | `			}` |
|      138 | 2498 | `		}else if( pTok->nType & (PH7_TK_SSTR\|PH7_TK_NOWDOC) ){` |
|        3 | 2499 | `			AssertRenderSglString(pOut,zTxt,nTxt);` |
|      103 | 2500 | `		}else if( pTok->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
|       15 | 2501 | `			if( SyByteFind(zTxt,nTxt,'$',0) == SXRET_OK ){` |
|        - | 2502 | `				/* Interpolated: php exports its interpolation AST in a` |
|        - | 2503 | `				 * double-quoted form; the raw source is the token-level` |
|        - | 2504 | `				 * equivalent. */` |
|      ! 0 | 2505 | `				SyBlobAppend(pOut,"\"",1);` |
|      ! 0 | 2506 | `				SyBlobAppend(pOut,zTxt,nTxt);` |
|      ! 0 | 2507 | `				SyBlobAppend(pOut,"\"",1);` |
|      ! 0 | 2508 | `			}else{` |
|       15 | 2509 | `				AssertRenderDblString(pOut,zTxt,nTxt);` |
|        - | 2510 | `			}` |
|        9 | 2511 | `		}else{` |
|       90 | 2512 | `			SyBlobAppend(pOut,zTxt,nTxt);` |
|        - | 2513 | `		}` |
|      197 | 2514 | `		iPrev = iCls;` |
|      101 | 2515 | `	}` |
|       67 | 2516 | `}` |
|        - | 2517 |  |
