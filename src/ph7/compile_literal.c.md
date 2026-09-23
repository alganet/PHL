# src/ph7/compile_literal.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1178/1373 lines (85.80%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits | Line | Source |
| ------: | ---: | :--- |
|       - |    1 | `/**` |
|       - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|       - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |    5 | ` */` |
|       - |    6 | `#include "ph7int.h"` |
|       - |    7 | `#include "compile_int.h"` |
|       - |    8 | `/*` |
|       - |    9 | ` * Section:` |
|       - |   10 | ` *    Literal compilation: numeric literals (incl. PHP 7.4 numeric` |
|       - |   11 | ` *    separators), simple/double-quoted strings, heredoc/nowdoc, array()` |
|       - |   12 | ` *    and [] literals, list() destructuring and clone-call rewriting.` |
|       - |   13 | ` * Status:` |
|       - |   14 | ` *    Stable.` |
|       - |   15 | ` */` |
|       - |   16 | `/*` |
|       - |   17 | ` * Return TRUE if c is a valid digit for the given numeric base.` |
|       - |   18 | ` *   base 16 => SyisHex (0-9, a-f, A-F)` |
|       - |   19 | ` *   base  8 => 0-7` |
|       - |   20 | ` *   base  2 => 0 or 1` |
|       - |   21 | ` *   base 10 => SyisDigit (0-9, also used for octal literals which share the` |
|       - |   22 | ` *              decimal scan in the lexer)` |
|       - |   23 | ` */` |
|    1780 |   24 | `static int GenStateIsBaseDigit(int c, int base)` |
|       4 |   25 | `{` |
|       - |   26 | `	/* ASCII arithmetic, not <ctype.h>: the byte can be any value in a string` |
|       - |   27 | `	 * literal, and isdigit()/isxdigit() are both locale-dependent and undefined` |
|       - |   28 | `	 * for a negative char. */` |
|    1784 |   29 | `	if( base == 16 ){` |
|     113 |   30 | `		return (c >= '0' && c <= '9') \|\| (c >= 'a' && c <= 'f') \|\| (c >= 'A' && c <= 'F');` |
|       - |   31 | `	}` |
|    1672 |   32 | `	if( base == 8 ){ return c >= '0' && c <= '7'; }` |
|    1666 |   33 | `	if( base == 2 ){ return c == '0' \|\| c == '1'; }` |
|    1380 |   34 | `	return c >= '0' && c <= '9';` |
|     894 |   35 | `}` |
|       - |   36 | `/*` |
|       - |   37 | ` * Given the raw text of a numeric literal token, locate a misplaced PHP 7.4` |
|       - |   38 | ` * underscore separator so the caller can report the malformed portion with` |
|       - |   39 | ` * the exact wording PHP uses:` |
|       - |   40 | ` *` |
|       - |   41 | ` *   syntax error, unexpected identifier "X"` |
|       - |   42 | ` *` |
|       - |   43 | ` * The lexer guarantees that every underscore it consumed as a separator is` |
|       - |   44 | ` * surrounded by valid base digits; anything else sits in the trailing run` |
|       - |   45 | ` * absorbed by the lexer specifically to let this validator see and report` |
|       - |   46 | ` * it. That invariant means the malformed span is exactly [bad .. nByte) —` |
|       - |   47 | ` * no forward rescan needed.` |
|       - |   48 | ` *` |
|       - |   49 | ` * Returns 1 and fills pBadStart / pBadLen when the literal is malformed;` |
|       - |   50 | ` * returns 0 when it is well-formed.` |
|       - |   51 | ` */` |
|  657440 |   52 | `static int GenStateFindBadNumericSeparator(` |
|       - |   53 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|       5 |   54 | `{` |
|  657445 |   55 | `	const char *z = pRaw->zString;` |
|  657445 |   56 | `	sxu32 n = pRaw->nByte;` |
|  657445 |   57 | `	int base = 10;` |
|       - |   58 | `	sxu32 i, start;` |
|  657445 |   59 | `	if( n < 2 ) return 0;` |
|  231189 |   60 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|     100 |   61 | `		base = 16;` |
|  231140 |   62 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|     287 |   63 | `		base = 2;` |
|     143 |   64 | `	}` |
|  789995 |   65 | `	for( i = 0; i < n; ++i ){` |
|  558821 |   66 | `		if( z[i] != '_' ) continue;` |
|     552 |   67 | `		if( i > 0 && i + 1 < n` |
|     549 |   68 | `			&& GenStateIsBaseDigit((unsigned char)z[i-1], base)` |
|     551 |   69 | `			&& GenStateIsBaseDigit((unsigned char)z[i+1], base) ){` |
|     544 |   70 | `			continue; /* well-placed separator */` |
|       - |   71 | `		}` |
|       - |   72 | `		/* First misplaced underscore — the lexer already absorbed the full` |
|       - |   73 | `		 * malformed tail, so it runs from here to the end of the token. */` |
|      14 |   74 | `		start = i;` |
|      19 |   75 | `		if( start > 0 && (z[start-1] == 'x' \|\| z[start-1] == 'X'` |
|      10 |   76 | `			\|\| z[start-1] == 'b' \|\| z[start-1] == 'B') ){` |
|     ! 0 |   77 | `			start--; /* include the base letter for 0x_... / 0b_... */` |
|     ! 0 |   78 | `		}` |
|      14 |   79 | `		*pBadStart = &z[start];` |
|      14 |   80 | `		*pBadLen = n - start;` |
|      14 |   81 | `		return 1;` |
|     ! 0 |   82 | `	}` |
|  231179 |   83 | `	return 0;` |
|  328725 |   84 | `}` |
|       - |   85 | `/*` |
|       - |   86 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|       - |   87 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|       - |   88 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|       - |   89 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|       - |   90 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|       - |   91 | ` * so callers can bail from the current construct).` |
|       - |   92 | ` */` |
|  657440 |   93 | `PH7_PRIVATE sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|       5 |   94 | `{` |
|  657445 |   95 | `	const char *zBad = 0;` |
|  657445 |   96 | `	sxu32 nBad = 0;` |
|       - |   97 | `	SyString sBad;` |
|       - |   98 | `	sxi32 rc;` |
|  657445 |   99 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|  657435 |  100 | `		return SXRET_OK;` |
|       - |  101 | `	}` |
|      14 |  102 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|      14 |  103 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|       - |  104 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|      14 |  105 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  106 | `		return SXERR_ABORT;` |
|       - |  107 | `	}` |
|      14 |  108 | `	return SXERR_SYNTAX;` |
|  328725 |  109 | `}` |
|       - |  110 | `/*` |
|       - |  111 | ` * Strip PHP 7.4 numeric literal separators (underscores between digits) from` |
|       - |  112 | ` * a numeric token's text and yield a SyString suitable for the low-level` |
|       - |  113 | ` * converters (SyStrToInt64 / SyStrToReal / etc.).` |
|       - |  114 | ` *` |
|       - |  115 | ` * Fast path: if the token contains no '_', *pOut aliases pToken with no copy` |
|       - |  116 | ` * and *pzAlloc is set to NULL.` |
|       - |  117 | ` * Stack path: if the cleaned bytes fit in zScratch, they are written there` |
|       - |  118 | ` * and *pzAlloc is set to NULL.` |
|       - |  119 | ` * Heap path: for literals larger than the scratch buffer, a fresh buffer is` |
|       - |  120 | ` * allocated from pAlloc, returned via *pzAlloc, and must be released by the` |
|       - |  121 | ` * caller with SyMemBackendFree once the converter is done.` |
|       - |  122 | ` *` |
|       - |  123 | ` * Returns SXRET_OK on success, SXERR_ABORT on allocator failure (in which` |
|       - |  124 | ` * case *pOut is left untouched and the caller must not read it).` |
|       - |  125 | ` */` |
|  657498 |  126 | `PH7_PRIVATE sxi32 GenStateStripNumericSeparators(` |
|       - |  127 | `	SyMemBackend *pAlloc,` |
|       - |  128 | `	const SyString *pToken,` |
|       - |  129 | `	char *zScratch, sxu32 nScratch,` |
|       - |  130 | `	SyString *pOut, char **pzAlloc)` |
|       5 |  131 | `{` |
|       - |  132 | `	sxu32 i, j;` |
|  657503 |  133 | `	int hasUnderscore = 0;` |
|       - |  134 | `	char *zBuf;` |
|  657503 |  135 | `	*pzAlloc = 0;` |
| 1640565 |  136 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|  983331 |  137 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|  491536 |  138 | `	}` |
|  657503 |  139 | `	if( !hasUnderscore ){` |
|  657239 |  140 | `		SyStringDupPtr(pOut, pToken);` |
|  657239 |  141 | `		return SXRET_OK;` |
|       - |  142 | `	}` |
|     266 |  143 | `	if( pToken->nByte <= nScratch ){` |
|     264 |  144 | `		zBuf = zScratch;` |
|     133 |  145 | `	}else{` |
|       3 |  146 | `		zBuf = (char *)SyMemBackendAlloc(pAlloc, pToken->nByte);` |
|       3 |  147 | `		if( zBuf == 0 ){` |
|     ! 0 |  148 | `			return SXERR_ABORT;` |
|       - |  149 | `		}` |
|       3 |  150 | `		*pzAlloc = zBuf;` |
|       - |  151 | `	}` |
|     266 |  152 | `	j = 0;` |
|    2974 |  153 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|    2710 |  154 | `		if( pToken->zString[i] != '_' ){ zBuf[j++] = pToken->zString[i]; }` |
|    1356 |  155 | `	}` |
|     266 |  156 | `	SyStringInitFromBuf(pOut, zBuf, j);` |
|     266 |  157 | `	return SXRET_OK;` |
|  328754 |  158 | `}` |
|       - |  159 | `/*` |
|       - |  160 | ` * Compile a numeric [i.e: integer or real] literal.` |
|       - |  161 | ` * Notes on the integer type.` |
|       - |  162 | ` *  According to the PHP language reference manual` |
|       - |  163 | ` *  Integers can be specified in decimal (base 10), hexadecimal (base 16), octal (base 8)` |
|       - |  164 | ` *  or binary (base 2) notation, optionally preceded by a sign (- or +).` |
|       - |  165 | ` *  To use octal notation, precede the number with a 0 (zero). To use hexadecimal` |
|       - |  166 | ` *  notation precede the number with 0x. To use binary notation precede the number with 0b.` |
|       - |  167 | ` * Symisc eXtension to the integer type.` |
|       - |  168 | ` *  PH7 introduced platform-independant 64-bit integer unlike the standard PHP engine` |
|       - |  169 | ` *  where the size of an integer is platform-dependent.That is,the size of an integer` |
|       - |  170 | ` *  is 8 bytes and the maximum integer size is 0x7FFFFFFFFFFFFFFF for all platforms` |
|       - |  171 | ` *  [i.e: either 32bit or 64bit].` |
|       - |  172 | ` *  For more information on this powerfull extension please refer to the official` |
|       - |  173 | ` *  documentation.` |
|       - |  174 | ` */` |
|       - |  175 | `/*` |
|       - |  176 | ` * Determine whether an integer literal token exceeds the signed 64-bit range.` |
|       - |  177 | ` * PHP promotes such a literal to a float (e.g. 9223372036854775808 ->` |
|       - |  178 | ` * float(9.22...E+18), 0xFFFFFFFFFFFFFFFF -> float) rather than wrapping or` |
|       - |  179 | ` * dropping digits. pNum is the separator-stripped token (unsigned; the sign of` |
|       - |  180 | ` * a "-1" is a separate unary operator). Base detection mirrors` |
|       - |  181 | ` * PH7_TokenValueToInt64. Returns TRUE on overflow: for a non-decimal base the` |
|       - |  182 | ` * float value is accumulated into *pReal (dv = dv*base + digit); for decimal` |
|       - |  183 | ` * *pbDecimal is set so the caller reuses strtod on the token for a` |
|       - |  184 | ` * correctly-rounded value. Returns FALSE (value fits) for anything it cannot` |
|       - |  185 | ` * confidently classify, so the int path stays in charge.` |
|       - |  186 | ` *` |
|       - |  187 | ` * The int/float CLASSIFICATION is php-exact for every base. VALUES are byte-exact` |
|       - |  188 | ` * for decimal (strtod) and hex (php's zend_hex_strtod uses the same dv*16+digit` |
|       - |  189 | ` * doubling). Octal/binary overflow values can differ from php by the low bit(s):` |
|       - |  190 | ` * php's zend_{oct,bin}_strtod rounds differently than this doubling — e.g. php's` |
|       - |  191 | ` * binary 2**63 is 2**63-1024 whereas this returns the exact 2**63. Recorded as a` |
|       - |  192 | ` * residual; matching php exactly would need a port of those functions.` |
|       - |  193 | ` */` |
|  646656 |  194 | `static int GenStateIntLiteralOverflows(const SyString *pNum, ph7_real *pReal, int *pbDecimal)` |
|       5 |  195 | `{` |
|  646661 |  196 | `	const char *z = pNum->zString;` |
|  646661 |  197 | `	const char *zEnd = z + pNum->nByte;` |
|       - |  198 | `	const char *p, *q;` |
|       - |  199 | `	int n;` |
|  646661 |  200 | `	*pbDecimal = FALSE;` |
|  646661 |  201 | `	if( z >= zEnd ){` |
|     ! 0 |  202 | `		return FALSE;` |
|       - |  203 | `	}` |
|  646661 |  204 | `	if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|       - |  205 | `		/* Hexadecimal: INT64_MAX == 0x7FFF...F (16 digits, leading nibble 7). */` |
|     102 |  206 | `		p = z + 2;` |
|     110 |  207 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|     594 |  208 | `		for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisHex(q[0]); q++ ){ n++; }` |
|     102 |  209 | `		if( n < 16 \|\| (n == 16 && SyHexToint(p[0]) < 8) ){` |
|      96 |  210 | `			return FALSE;` |
|       - |  211 | `		}` |
|       7 |  212 | `		{ ph7_real dv = 0;` |
|     103 |  213 | `		  for( q = p; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisHex(q[0]); q++ ){` |
|      97 |  214 | `			dv = dv * 16 + (ph7_real)SyHexToint(q[0]);` |
|      49 |  215 | `		  }` |
|       7 |  216 | `		  *pReal = dv;` |
|       - |  217 | `		}` |
|       7 |  218 | `		return TRUE;` |
|  646561 |  219 | `	}else if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|       - |  220 | `		/* Binary: INT64_MAX needs 63 significant bits. */` |
|     287 |  221 | `		p = z + 2;` |
|     335 |  222 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|    2172 |  223 | `		for( q = p, n = 0; q < zEnd && (q[0] == '0' \|\| q[0] == '1'); q++ ){ n++; }` |
|     287 |  224 | `		if( n <= 63 ){` |
|     285 |  225 | `			return FALSE;` |
|       - |  226 | `		}` |
|       3 |  227 | `		{ ph7_real dv = 0;` |
|     195 |  228 | `		  for( q = p; q < zEnd && (q[0] == '0' \|\| q[0] == '1'); q++ ){` |
|     129 |  229 | `			dv = dv * 2 + (ph7_real)(q[0] - '0');` |
|      65 |  230 | `		  }` |
|       3 |  231 | `		  *pReal = dv;` |
|       - |  232 | `		}` |
|       3 |  233 | `		return TRUE;` |
|  646275 |  234 | `	}else if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'o' \|\| z[1] == 'O') ){` |
|       - |  235 | `		/* PHP 8.1 explicit octal 0o/0O: 21 significant octal digits fit in int64. */` |
|      21 |  236 | `		p = z + 2;` |
|      25 |  237 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|      97 |  238 | `		for( q = p, n = 0; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){ n++; }` |
|      21 |  239 | `		if( n <= 21 ){` |
|      21 |  240 | `			return FALSE;` |
|       - |  241 | `		}` |
|     ! 0 |  242 | `		{ ph7_real dv = 0;` |
|     ! 0 |  243 | `		  for( q = p; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){` |
|     ! 0 |  244 | `			dv = dv * 8 + (ph7_real)(q[0] - '0');` |
|     ! 0 |  245 | `		  }` |
|     ! 0 |  246 | `		  *pReal = dv;` |
|       - |  247 | `		}` |
|     ! 0 |  248 | `		return TRUE;` |
|  646255 |  249 | `	}else if( z[0] == '0' ){` |
|       - |  250 | `		/* Octal: INT64_MAX == 0o777...7 (21 significant octal digits). Skip the` |
|       - |  251 | `		 * leading zeros (incl. the base '0'); a non-octal char such as the 8.1` |
|       - |  252 | `		 * "0o" marker ends the run and leaves it to the int path (as today). */` |
|  227647 |  253 | `		p = z;` |
|  455295 |  254 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|  241925 |  255 | `		for( q = p, n = 0; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){ n++; }` |
|  227647 |  256 | `		if( n <= 21 ){` |
|  227645 |  257 | `			return FALSE;` |
|       - |  258 | `		}` |
|       3 |  259 | `		{ ph7_real dv = 0;` |
|      47 |  260 | `		  for( q = p; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){` |
|      45 |  261 | `			dv = dv * 8 + (ph7_real)(q[0] - '0');` |
|      23 |  262 | `		  }` |
|       3 |  263 | `		  *pReal = dv;` |
|       - |  264 | `		}` |
|       3 |  265 | `		return TRUE;` |
|       - |  266 | `	}` |
|       - |  267 | `	/* Decimal: overflow iff more than 19 significant digits, or exactly 19 that` |
|       - |  268 | `	 * compare greater than INT64_MAX. Defer the value to strtod (via the caller)` |
|       - |  269 | `	 * for php-exact rounding. */` |
|  418613 |  270 | `	p = z;` |
|  418613 |  271 | `	while( p < zEnd && p[0] == '0' ){ p++; }` |
| 1122849 |  272 | `	for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisDigit(q[0]); q++ ){ n++; }` |
|  418613 |  273 | `	if( n > 19 \|\| (n == 19 && SyMemcmp(p, "9223372036854775807", 19) > 0) ){` |
|      27 |  274 | `		*pbDecimal = TRUE;` |
|      27 |  275 | `		return TRUE;` |
|       - |  276 | `	}` |
|  418587 |  277 | `	return FALSE;` |
|  323333 |  278 | `}` |
|  657406 |  279 | `PH7_PRIVATE sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  280 | `{` |
|  657411 |  281 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|  657411 |  282 | `	sxu32 nIdx = 0;` |
|       - |  283 | `	char zScratch[GEN_NUM_SCRATCH];` |
|  657411 |  284 | `	char *zAlloc = 0;` |
|       - |  285 | `	SyString sNum;` |
|       - |  286 | `	sxi32 rc;` |
|  328703 |  287 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|  657411 |  288 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|  657411 |  289 | `	if( rc != SXRET_OK ){` |
|       9 |  290 | `		return rc;` |
|       - |  291 | `	}` |
|  986105 |  292 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|  328700 |  293 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|  657405 |  294 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  295 | `		return SXERR_ABORT;` |
|       - |  296 | `	}` |
|  657405 |  297 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|       - |  298 | `		ph7_value *pObj;` |
|       - |  299 | `		sxi64 iValue;` |
|  646601 |  300 | `		ph7_real rOverflow = 0;` |
|  646601 |  301 | `		int bDecimalOverflow = 0;` |
|  646601 |  302 | `		if( GenStateIntLiteralOverflows(&sNum,&rOverflow,&bDecimalOverflow) ){` |
|       - |  303 | `			/* Literal exceeds the signed 64-bit range: PHP represents it as a` |
|       - |  304 | `			 * float instead of wrapping/dropping digits. */` |
|      37 |  305 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      37 |  306 | `			if( pObj == 0 ){` |
|     ! 0 |  307 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  308 | `				if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |  309 | `				return SXERR_ABORT;` |
|       - |  310 | `			}` |
|      37 |  311 | `			if( bDecimalOverflow ){` |
|       - |  312 | `				/* strtod on the decimal token yields php-exact rounding. */` |
|      27 |  313 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|      27 |  314 | `				PH7_MemObjToReal(pObj);` |
|      14 |  315 | `			}else{` |
|      11 |  316 | `				PH7_MemObjInitFromReal(pGen->pVm,pObj,rOverflow);` |
|       - |  317 | `			}` |
|      19 |  318 | `		}else{` |
|  646565 |  319 | `			iValue = PH7_TokenValueToInt64(&sNum);` |
|  646565 |  320 | `			pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|  646565 |  321 | `			if( pObj == 0 ){` |
|     ! 0 |  322 | `				if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |  323 | `				return SXERR_ABORT;` |
|       - |  324 | `			}` |
|  646565 |  325 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|       - |  326 | `		}` |
|  323303 |  327 | `	}else{` |
|       - |  328 | `		/* Real number */` |
|       - |  329 | `		ph7_value *pObj;` |
|       - |  330 | `		/* Reserve a new constant */` |
|   10809 |  331 | `		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   10809 |  332 | `		if( pObj == 0 ){` |
|     ! 0 |  333 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  334 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |  335 | `			return SXERR_ABORT;` |
|       - |  336 | `		}` |
|   10809 |  337 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|   10809 |  338 | `		PH7_MemObjToReal(pObj);` |
|       - |  339 | `	}` |
|  657405 |  340 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       - |  341 | `	/* Emit the load constant instruction */` |
|  657405 |  342 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  343 | `	/* Node successfully compiled */` |
|  657405 |  344 | `	return SXRET_OK;` |
|  328708 |  345 | `}` |
|       - |  346 | `/*` |
|       - |  347 | ` * Compile a single quoted string.` |
|       - |  348 | ` * According to the PHP language reference manual:` |
|       - |  349 | ` *` |
|       - |  350 | ` *   The simplest way to specify a string is to enclose it in single quotes (the character ' ).` |
|       - |  351 | ` *   To specify a literal single quote, escape it with a backslash (\). To specify a literal` |
|       - |  352 | ` *   backslash, double it (\\). All other instances of backslash will be treated as a literal` |
|       - |  353 | ` *   backslash: this means that the other escape sequences you might be used to, such as \r` |
|       - |  354 | ` *   or \n, will be output literally as specified rather than having any special meaning.` |
|       - |  355 | ` *` |
|       - |  356 | ` */` |
|  345788 |  357 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  358 | `{` |
|  345793 |  359 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|       - |  360 | `	const char *zIn,*zCur,*zEnd;` |
|       - |  361 | `	ph7_value *pObj;` |
|       - |  362 | `	sxu32 nIdx;` |
|       - |  363 | `	sxi32 bHasEsc;` |
|  345793 |  364 | `	nIdx = 0; /* Prevent compiler warning */` |
|       - |  365 | `	/* Delimit the string */` |
|  345793 |  366 | `	zIn  = pStr->zString;` |
|  345793 |  367 | `	zEnd = &zIn[pStr->nByte];` |
|  345793 |  368 | `	if( zIn >= zEnd ){` |
|       - |  369 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|       - |  370 | `		 * rather than reserving a new object each time. */` |
|   61227 |  371 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|   61227 |  372 | `		return SXRET_OK;` |
|       - |  373 | `	}` |
|       - |  374 | `	/* A single-quoted literal whose raw source holds a backslash unescapes to a` |
|       - |  375 | `	 * value that differs from that source (\\ -> \, \' -> '). The literal cache` |
|       - |  376 | `	 * keys FIND on the raw source text but INSTALL on the unescaped value, so` |
|       - |  377 | `	 * caching such a literal lets an unrelated one collide with it — e.g. '\\'` |
|       - |  378 | `	 * (raw \\, value \) would find the entry installed for '\\\\' (raw \\\\,` |
|       - |  379 | `	 * value \\) and load two backslashes. Only cache literals whose value equals` |
|       - |  380 | `	 * their source, i.e. those with no backslash to unescape. */` |
|  284571 |  381 | `	bHasEsc = 0;` |
|       - |  382 | `	{` |
|       - |  383 | `		const char *zScan;` |
| 5471161 |  384 | `		for( zScan = zIn ; zScan < zEnd ; zScan++ ){` |
| 5187065 |  385 | `			if( zScan[0] == '\\' ){ bHasEsc = 1; break; }` |
| 2593300 |  386 | `		}` |
|       - |  387 | `	}` |
|  284571 |  388 | `	if( !bHasEsc && SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|       - |  389 | `		/* Already processed,emit the load constant instruction` |
|       - |  390 | `		 * and return.` |
|       - |  391 | `		 */` |
|  131991 |  392 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|  131991 |  393 | `		return SXRET_OK;` |
|       - |  394 | `	}` |
|       - |  395 | `	/* Reserve a new constant */` |
|  152585 |  396 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  152585 |  397 | `	if( pObj == 0 ){` |
|     ! 0 |  398 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  399 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  400 | `		return SXERR_ABORT;` |
|       - |  401 | `	}` |
|  152585 |  402 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |  403 | `	/* Compile the node */` |
|  152854 |  404 | `	for(;;){` |
|  305713 |  405 | `		if( zIn >= zEnd ){` |
|       - |  406 | `			/* End of input */` |
|  152585 |  407 | `			break;` |
|       - |  408 | `		}` |
|  153133 |  409 | `		zCur = zIn;` |
| 4861347 |  410 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
| 4708219 |  411 | `			zIn++;` |
|       5 |  412 | `		}` |
|  153133 |  413 | `		if( zIn > zCur ){` |
|       - |  414 | `			/* Append raw contents*/` |
|  152955 |  415 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   76475 |  416 | `		}` |
|  153133 |  417 | `		zIn++;` |
|  153133 |  418 | `		if( zIn < zEnd ){` |
|     631 |  419 | `			if( zIn[0] == '\\' ){` |
|       - |  420 | `				/* A literal backslash */` |
|     169 |  421 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|     549 |  422 | `			}else if( zIn[0] == '\'' ){` |
|       - |  423 | `				/* A single quote */` |
|      22 |  424 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|      12 |  425 | `			}else{` |
|       - |  426 | `				/* verbatim copy */` |
|     447 |  427 | `				zIn--;` |
|     447 |  428 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char)*2);` |
|     447 |  429 | `				zIn++;` |
|       - |  430 | `			}` |
|     313 |  431 | `		}` |
|       - |  432 | `		/* Advance the stream cursor */` |
|  153133 |  433 | `		zIn++;` |
|       5 |  434 | `	}` |
|       - |  435 | `	/* Emit the load constant instruction */` |
|  152585 |  436 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|  152585 |  437 | `	if( !bHasEsc && pStr->nByte < 1024 ){` |
|       - |  438 | `		/* Install in the literal table (only when value == source; see above) */` |
|  152115 |  439 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|   76055 |  440 | `	}` |
|       - |  441 | `	/* Node successfully compiled */` |
|  152585 |  442 | `	return SXRET_OK;` |
|  172899 |  443 | `}` |
|       - |  444 | `/*` |
|       - |  445 | ` * PHP 7.3 flexible heredoc/nowdoc closing-marker indent stripping.` |
|       - |  446 | ` *` |
|       - |  447 | ` * When the lexer matched the closing marker with leading whitespace on its` |
|       - |  448 | ` * own line, it stored the indent count in pGen->pIn->pUserData. The marker's` |
|       - |  449 | ` * indent prefix bytes sit immediately after the stripped body (at` |
|       - |  450 | ` * pIn->sData.zString + pIn->sData.nByte + 1 for LF, +2 for CRLF) in the` |
|       - |  451 | ` * original source buffer — the buffer is stable through compilation.` |
|       - |  452 | ` *` |
|       - |  453 | `` * For each body line, we remove exactly `nIndent` leading bytes that must`` |
|       - |  454 | ` * byte-for-byte match the marker's prefix. Empty lines (0 bytes or bare \r)` |
|       - |  455 | ` * bypass validation. Mismatches raise the exact PHP 7.3+ parse errors:` |
|       - |  456 | ` *   - "Invalid body indentation level (expecting an indentation level of` |
|       - |  457 | ` *     at least N)" — line too short, or first differing byte is not` |
|       - |  458 | ` *     whitespace.` |
|       - |  459 | ` *   - "Invalid indentation - tabs and spaces cannot be mixed" — first` |
|       - |  460 | ` *     differing byte is whitespace but differs from the marker prefix.` |
|       - |  461 | ` */` |
|     128 |  462 | `static sxi32 GenStateStripHeredocIndent(ph7_gen_state *pGen, SyString *pOut)` |
|       4 |  463 | `{` |
|     132 |  464 | `	SyString *pIn = &pGen->pIn->sData;` |
|     132 |  465 | `	sxu32 nIndent = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - |  466 | `	const char *zPrefix;` |
|       - |  467 | `	const char *z, *zEnd;` |
|       - |  468 | `	char *zBuf, *zDst;` |
|     132 |  469 | `	if( nIndent == 0 ){` |
|       - |  470 | `		/* Legacy column-0 marker: zero-copy fast path */` |
|      86 |  471 | `		*pOut = *pIn;` |
|      86 |  472 | `		return SXRET_OK;` |
|       - |  473 | `	}` |
|       - |  474 | `	/* Recover the marker indent prefix from the original source buffer.` |
|       - |  475 | `	 * Skip the terminator the lexer stripped: one '\n' plus an optional` |
|       - |  476 | `	 * preceding '\r'. Note: when the body is empty (pIn->nByte == 0) the` |
|       - |  477 | `	 * lexer stripped nothing, so this offset is one byte past the true` |
|       - |  478 | `	 * marker-indent start. That is harmless — the strip loop below never` |
|       - |  479 | `	 * runs (z == zEnd), and zPrefix is never dereferenced. */` |
|      49 |  480 | `	zPrefix = pIn->zString + pIn->nByte;` |
|      49 |  481 | `	if( zPrefix[0] == '\r' && zPrefix[1] == '\n' ){` |
|     ! 0 |  482 | `		zPrefix += 2;` |
|     ! 0 |  483 | `	}else{` |
|      49 |  484 | `		zPrefix += 1;` |
|       - |  485 | `	}` |
|       - |  486 | `	/* Allocate scratch buffer sized to the original body (always enough). */` |
|      49 |  487 | `	zBuf = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator, pIn->nByte + 1);` |
|      49 |  488 | `	if( zBuf == 0 ){` |
|     ! 0 |  489 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  490 | `		return SXERR_ABORT;` |
|       - |  491 | `	}` |
|      49 |  492 | `	zDst = zBuf;` |
|      49 |  493 | `	z = pIn->zString;` |
|      49 |  494 | `	zEnd = z + pIn->nByte;` |
|     134 |  495 | `	while( z < zEnd ){` |
|      73 |  496 | `		const char *zLine = z;` |
|       - |  497 | `		sxu32 nLine;` |
|       - |  498 | `		int bEmpty;` |
|     815 |  499 | `		while( z < zEnd && z[0] != '\n' ){` |
|     745 |  500 | `			z++;` |
|       3 |  501 | `		}` |
|      73 |  502 | `		nLine = (sxu32)(z - zLine);` |
|      73 |  503 | `		bEmpty = (nLine == 0) \|\| (nLine == 1 && zLine[0] == '\r');` |
|      73 |  504 | `		if( !bEmpty ){` |
|       - |  505 | `			sxu32 i;` |
|      69 |  506 | `			if( nLine < nIndent ){` |
|     ! 0 |  507 | `				PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  508 | `					"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|     ! 0 |  509 | `					nIndent);` |
|     ! 0 |  510 | `				return SXERR_ABORT;` |
|       - |  511 | `			}` |
|     279 |  512 | `			for( i = 0; i < nIndent; i++ ){` |
|     221 |  513 | `				if( zLine[i] != zPrefix[i] ){` |
|      10 |  514 | `					unsigned char c = (unsigned char)zLine[i];` |
|      10 |  515 | `					if( c == ' ' \|\| c == '\t' ){` |
|       5 |  516 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  517 | `							"Invalid indentation - tabs and spaces cannot be mixed");` |
|       3 |  518 | `					}else{` |
|       7 |  519 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  520 | `							"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|       2 |  521 | `							nIndent);` |
|       - |  522 | `					}` |
|      10 |  523 | `					return SXERR_ABORT;` |
|       - |  524 | `				}` |
|     107 |  525 | `			}` |
|      60 |  526 | `			SyMemcpy((const void *)(zLine + nIndent), (void *)zDst, nLine - nIndent);` |
|      60 |  527 | `			zDst += nLine - nIndent;` |
|      34 |  528 | `		}else if( nLine == 1 ){` |
|       - |  529 | `			/* Preserve the stray '\r' on an otherwise empty line */` |
|     ! 0 |  530 | `			*zDst++ = '\r';` |
|     ! 0 |  531 | `		}` |
|      64 |  532 | `		if( z < zEnd ){` |
|      25 |  533 | `			*zDst++ = '\n';` |
|      25 |  534 | `			z++;` |
|      12 |  535 | `		}` |
|       2 |  536 | `	}` |
|      40 |  537 | `	pOut->zString = zBuf;` |
|      40 |  538 | `	pOut->nByte = (sxu32)(zDst - zBuf);` |
|      40 |  539 | `	return SXRET_OK;` |
|      68 |  540 | `}` |
|       - |  541 | `/*` |
|       - |  542 | ` * Compile a nowdoc string.` |
|       - |  543 | ` * According to the PHP language reference manual:` |
|       - |  544 | ` *` |
|       - |  545 | ` *  Nowdocs are to single-quoted strings what heredocs are to double-quoted strings.` |
|       - |  546 | ` *  A nowdoc is specified similarly to a heredoc, but no parsing is done inside a nowdoc.` |
|       - |  547 | ` *  The construct is ideal for embedding PHP code or other large blocks of text without the` |
|       - |  548 | ` *  need for escaping. It shares some features in common with the SGML <![CDATA[ ]]>` |
|       - |  549 | ` *  construct, in that it declares a block of text which is not for parsing.` |
|       - |  550 | ` *  A nowdoc is identified with the same <<< sequence used for heredocs, but the identifier` |
|       - |  551 | ` *  which follows is enclosed in single quotes, e.g. <<<'EOT'. All the rules for heredoc` |
|       - |  552 | ` *  identifiers also apply to nowdoc identifiers, especially those regarding the appearance` |
|       - |  553 | ` *  of the closing identifier.` |
|       - |  554 | ` */` |
|      52 |  555 | `PH7_PRIVATE sxi32 PH7_CompileNowDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       4 |  556 | `{` |
|       - |  557 | `	SyString sStripped;` |
|       - |  558 | `	SyString *pStr;` |
|       - |  559 | `	ph7_value *pObj;` |
|       - |  560 | `	sxu32 nIdx;` |
|       - |  561 | `	sxi32 rc;` |
|      56 |  562 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|      56 |  563 | `	if( rc != SXRET_OK ){` |
|       6 |  564 | `		return rc;` |
|       - |  565 | `	}` |
|      51 |  566 | `	pStr = &sStripped;` |
|      51 |  567 | `	nIdx = 0; /* Prevent compiler warning */` |
|      51 |  568 | `	if( pStr->nByte <= 0 ){` |
|       - |  569 | `		/* An empty nowdoc is the empty STRING, like '' -- loading NULL here made` |
|       - |  570 | `		 * strlen(<<<'EOD'EOD;) deprecation-warn about a null argument. */` |
|       7 |  571 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|       7 |  572 | `		return SXRET_OK;` |
|       - |  573 | `	}` |
|       - |  574 | `	/* Reserve a new constant */` |
|      45 |  575 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      45 |  576 | `	if( pObj == 0 ){` |
|     ! 0 |  577 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  578 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  579 | `		return SXERR_ABORT;` |
|       - |  580 | `	}` |
|       - |  581 | `	/* No processing is done here, simply a memcpy() operation */` |
|      45 |  582 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,pStr);` |
|       - |  583 | `	/* Emit the load constant instruction */` |
|      45 |  584 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  585 | `	/* Node successfully compiled */` |
|      45 |  586 | `	return SXRET_OK;` |
|      30 |  587 | `}` |
|       - |  588 | `/*` |
|       - |  589 | ` * Process variable expression [i.e: "$var","${var}"] embedded in a double quoted/heredoc string.` |
|       - |  590 | ` * According to the PHP language reference manual` |
|       - |  591 | ` *   When a string is specified in double quotes or with heredoc,variables are parsed within it.` |
|       - |  592 | ` *  There are two types of syntax: a simple one and a complex one. The simple syntax is the most` |
|       - |  593 | ` *  common and convenient. It provides a way to embed a variable, an array value, or an object` |
|       - |  594 | ` *  property in a string with a minimum of effort.` |
|       - |  595 | ` *  Simple syntax` |
|       - |  596 | ` *   If a dollar sign ($) is encountered, the parser will greedily take as many tokens as possible` |
|       - |  597 | ` *   to form a valid variable name. Enclose the variable name in curly braces to explicitly specify` |
|       - |  598 | ` *   the end of the name.` |
|       - |  599 | ` *   Similarly, an array index or an object property can be parsed. With array indices, the closing` |
|       - |  600 | ` *   square bracket (]) marks the end of the index. The same rules apply to object properties` |
|       - |  601 | ` *   as to simple variables.` |
|       - |  602 | ` *  Complex (curly) syntax` |
|       - |  603 | ` *   This isn't called complex because the syntax is complex, but because it allows for the use` |
|       - |  604 | ` *   of complex expressions.` |
|       - |  605 | ` *   Any scalar variable, array element or object property with a string representation can be` |
|       - |  606 | ` *   included via this syntax. Simply write the expression the same way as it would appear outside` |
|       - |  607 | ` *   the string, and then wrap it in { and }. Since { can not be escaped, this syntax will only` |
|       - |  608 | ` *   be recognised when the $ immediately follows the {. Use {\$ to get a literal {$` |
|       - |  609 | ` */` |
|    3450 |  610 | `static sxi32 GenStateProcessStringExpression(` |
|       - |  611 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  612 | `	sxu32 nLine,         /* Line number */` |
|       - |  613 | `	const char *zIn,     /* Raw expression */` |
|       - |  614 | `	const char *zEnd     /* End of the expression */` |
|       - |  615 | `	)` |
|       5 |  616 | `{` |
|       - |  617 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  618 | `	SySet sToken;` |
|       - |  619 | `	sxi32 rc;` |
|       - |  620 | `	/* Initialize the token set */` |
|    3455 |  621 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       - |  622 | `	/* Preallocate some slots */` |
|    3455 |  623 | `	SySetAlloc(&sToken,0x08);` |
|       - |  624 | `	/* Tokenize the text */` |
|    3455 |  625 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken,0);` |
|       - |  626 | `	/* Swap delimiter */` |
|    3455 |  627 | `	pTmpIn  = pGen->pIn;` |
|    3455 |  628 | `	pTmpEnd = pGen->pEnd;` |
|    3455 |  629 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|    3455 |  630 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       - |  631 | ``	/* Compile the expression. An interpolated `"...$x..."` READS $x — php warns`` |
|       - |  632 | `	 * "Undefined variable $x" and substitutes the empty string — so ask for a` |
|       - |  633 | `	 * read-only load rather than letting the default vivify it silently. */` |
|    3455 |  634 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|       - |  635 | `	/* Restore token stream */` |
|    3455 |  636 | `	pGen->pIn  = pTmpIn;` |
|    3455 |  637 | `	pGen->pEnd = pTmpEnd;` |
|       - |  638 | `	/* Release the token set */` |
|    3455 |  639 | `	SySetRelease(&sToken);` |
|       - |  640 | `	/* Compilation result */` |
|    3455 |  641 | `	return rc;` |
|       5 |  642 | `}` |
|       - |  643 | `/*` |
|       - |  644 | ` * Line number of a POSITION inside the string body being compiled: the` |
|       - |  645 | ` * token's line plus every newline before it. php reports the offending` |
|       - |  646 | ` * construct's own line, not the string's opening line, so every diagnostic` |
|       - |  647 | ` * raised from inside a body -- an escape sequence, a malformed subscript --` |
|       - |  648 | ` * goes through here. A heredoc body starts on the line after the '<<<'` |
|       - |  649 | ` * marker, hence the +1.` |
|       - |  650 | ` */` |
|      40 |  651 | `static sxu32 GenStateStringEscLine(ph7_gen_state *pGen,const char *zPos,int bHeredoc)` |
|       4 |  652 | `{` |
|      44 |  653 | `	const char *z = pGen->pIn->sData.zString;` |
|      44 |  654 | `	sxu32 nLine = pGen->pIn->nLine + (bHeredoc ? 1 : 0);` |
|     180 |  655 | `	for( ; z < zPos ; z++ ){` |
|     140 |  656 | `		if( z[0] == '\n' ){` |
|     ! 0 |  657 | `			nLine++;` |
|     ! 0 |  658 | `		}` |
|      72 |  659 | `	}` |
|      44 |  660 | `	return nLine;` |
|       4 |  661 | `}` |
|       - |  662 | `/*` |
|       - |  663 | ` * TRUE when c can OPEN a php label — the same byte class the engine's identifier` |
|       - |  664 | ` * scanner uses (LEX_LABEL_START in lex.c): [a-zA-Z_\x80-\xff].` |
|       - |  665 | ` */` |
|       - |  666 | `#define GEN_STRING_LABEL_START(c) \` |
|       - |  667 | `	( (unsigned char)(c) >= 0x80 \|\| SyisAlpha(c) \|\| (c) == '_' )` |
|       - |  668 | `/*` |
|       - |  669 | ` * Advance *pz over a php LABEL — the name half of "$name" and of the "->name"` |
|       - |  670 | ` * accessor inside a double-quoted string or a heredoc body. php's label is` |
|       - |  671 | ` * [a-zA-Z_\x80-\xff][a-zA-Z0-9_\x80-\xff]*, a flat byte set: a multibyte name` |
|       - |  672 | ` * is consumed because every byte of it is >= 0x80, no UTF-8 decoding involved.` |
|       - |  673 | ` * Stops at *pz when the cursor is not on a label byte.` |
|       - |  674 | ` */` |
|    3388 |  675 | `static void GenStateSkipStringLabel(const char **pz,const char *zEnd)` |
|       5 |  676 | `{` |
|    3393 |  677 | `	const char *zIn = *pz;` |
|    9956 |  678 | `	while( zIn < zEnd` |
|   13141 |  679 | `		&& ((unsigned char)zIn[0] >= 0x80 \|\| SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_') ){` |
|    9753 |  680 | `		zIn++;` |
|       5 |  681 | `	}` |
|    3393 |  682 | `	*pz = zIn;` |
|    3393 |  683 | `}` |
|       - |  684 | `/*` |
|       - |  685 | ` * Scan one php INTEGER literal at z in the flavour php's simple-syntax subscript` |
|       - |  686 | ` * accepts: LNUM, HNUM (0x...), BNUM (0b...) or ONUM (0o...), each allowing '_'` |
|       - |  687 | ` * separators BETWEEN digits. There is no float and no exponent in this grammar --` |
|       - |  688 | ` * "$a[1.5]" and "$a[1e2]" are php parse errors. Returns the byte after the` |
|       - |  689 | ` * literal, or z itself when the cursor is not on one.` |
|       - |  690 | ` */` |
|      66 |  691 | `static const char * GenStateScanOffsetNumber(const char *z,const char *zEnd)` |
|       1 |  692 | `{` |
|      67 |  693 | `	const char *zStart = z;` |
|      67 |  694 | `	int base = 10;` |
|      67 |  695 | `	if( z >= zEnd \|\| !GenStateIsBaseDigit((unsigned char)z[0],10) ){` |
|     ! 0 |  696 | `		return z;` |
|       - |  697 | `	}` |
|      67 |  698 | `	if( z[0] == '0' && &z[1] < zEnd ){` |
|      23 |  699 | `		int b = 0;` |
|      23 |  700 | `		if( z[1] == 'x' \|\| z[1] == 'X' ){` |
|       7 |  701 | `			b = 16;` |
|      20 |  702 | `		}else if( z[1] == 'b' \|\| z[1] == 'B' ){` |
|       3 |  703 | `			b = 2;` |
|      16 |  704 | `		}else if( z[1] == 'o' \|\| z[1] == 'O' ){` |
|       3 |  705 | `			b = 8;` |
|       1 |  706 | `		}` |
|       - |  707 | `		/* A prefix with no digit behind it is not a literal: php then matches the` |
|       - |  708 | `		 * lone "0" and lexes the rest as a label ("$a[0x]" is a parse error). */` |
|      23 |  709 | `		if( b && &z[2] < zEnd && GenStateIsBaseDigit((unsigned char)z[2],b) ){` |
|       9 |  710 | `			base = b;` |
|       9 |  711 | `			z += 2;` |
|       4 |  712 | `		}` |
|      11 |  713 | `	}` |
|     345 |  714 | `	while( z < zEnd ){` |
|     293 |  715 | `		if( GenStateIsBaseDigit((unsigned char)z[0],base) ){` |
|     275 |  716 | `			z++;` |
|     275 |  717 | `			continue;` |
|       - |  718 | `		}` |
|      18 |  719 | `		if( z[0] == '_' && z > zStart && GenStateIsBaseDigit((unsigned char)z[-1],base)` |
|       9 |  720 | `			&& &z[1] < zEnd && GenStateIsBaseDigit((unsigned char)z[1],base) ){` |
|       5 |  721 | `			z += 2;` |
|       5 |  722 | `			continue;` |
|       - |  723 | `		}` |
|      15 |  724 | `		break;` |
|     ! 0 |  725 | `	}` |
|      67 |  726 | `	return z;` |
|      34 |  727 | `}` |
|       - |  728 | `/*` |
|       - |  729 | ` * TRUE when the digit run [z,zEnd) is php's CANONICAL spelling of an INTEGER` |
|       - |  730 | ` * offset: "0", or [1-9][0-9]* that fits a signed 64-bit int. php carries every` |
|       - |  731 | ` * other spelling -- leading zeros, a base prefix, '_' separators, a magnitude` |
|       - |  732 | ` * past the int range -- as the raw TEXT, i.e. a STRING key. (zend also spells` |
|       - |  733 | ` * out any 19-digit run, but its hashmap folds that straight back to an integer` |
|       - |  734 | ` * key, so the two agree on everything an array can observe.)` |
|       - |  735 | ` */` |
|      52 |  736 | `static int GenStateOffsetIsCanonicalInt(const char *z,const char *zEnd,int bNeg)` |
|       1 |  737 | `{` |
|      53 |  738 | `	sxu32 n = (sxu32)(zEnd - z);` |
|       - |  739 | `	sxu32 i;` |
|      53 |  740 | `	if( n < 1 ){` |
|     ! 0 |  741 | `		return FALSE;` |
|       - |  742 | `	}` |
|      53 |  743 | `	if( z[0] == '0' ){` |
|       - |  744 | `		/* "0" alone is the integer key 0; "-0", "00" and "007" are text */` |
|      31 |  745 | `		return n == 1 && !bNeg;` |
|       - |  746 | `	}` |
|     227 |  747 | `	for( i = 0 ; i < n ; ++i ){` |
|     207 |  748 | `		if( !GenStateIsBaseDigit((unsigned char)z[i],10) ){` |
|       3 |  749 | `			return FALSE;` |
|       - |  750 | `		}` |
|     103 |  751 | `	}` |
|       - |  752 | `	/* INT64_MAX bounds BOTH signs here, not INT64_MIN: the rewrite re-emits a` |
|       - |  753 | `	 * canonical offset as SOURCE, and no php expression can spell INT64_MIN as a` |
|       - |  754 | `	 * literal (the '-' is unary minus over an out-of-range literal, which` |
|       - |  755 | `	 * promotes to a float). "-9223372036854775808" therefore takes the string` |
|       - |  756 | `	 * path, where the hashmap's numeric-string rule folds it back to the integer` |
|       - |  757 | `	 * key -- and where an ArrayAccess offsetGet() receives php's own string. */` |
|      21 |  758 | `	if( n > 19 \|\| (n == 19 && SyMemcmp(z,"9223372036854775807",19) > 0) ){` |
|       7 |  759 | `		return FALSE;` |
|       - |  760 | `	}` |
|      15 |  761 | `	return TRUE;` |
|      27 |  762 | `}` |
|       - |  763 | `/*` |
|       - |  764 | ` * php's parse error for a malformed simple-syntax subscript. zBad points at the` |
|       - |  765 | ` * first byte php would refuse; iExpect picks which of php's "expecting" tails` |
|       - |  766 | ` * applies -- 1 after an otherwise good offset, 2 after a lone '-', 0 at the` |
|       - |  767 | ` * offset's start. Always returns SXERR_ABORT so the caller can just pass it on.` |
|       - |  768 | ` */` |
|      34 |  769 | `static sxi32 GenStateOffsetSyntaxError(ph7_gen_state *pGen,const char *zBad,const char *zEnd,int iExpect,int bHeredoc)` |
|       1 |  770 | `{` |
|       - |  771 | `	SyString sTok;` |
|      35 |  772 | `	sxu32 n = (sxu32)(zEnd - zBad);` |
|      35 |  773 | `	if( n < 1 ){` |
|       - |  774 | `		/* Empty offset: name the ']' that zEnd points at */` |
|       3 |  775 | `		n = 1;` |
|       1 |  776 | `	}` |
|      35 |  777 | `	if( n > 16 ){` |
|     ! 0 |  778 | `		n = 16;` |
|     ! 0 |  779 | `	}` |
|      35 |  780 | `	SyStringInitFromBuf(&sTok,zBad,n);` |
|      35 |  781 | `	if( iExpect == 1 ){` |
|      19 |  782 | `		PH7_GenCompileError(&(*pGen),E_PARSE,GenStateStringEscLine(&(*pGen),zBad,bHeredoc),` |
|       - |  783 | `			"syntax error, unexpected token \"%z\", expecting \"]\"",&sTok);` |
|      26 |  784 | `	}else if( iExpect == 2 ){` |
|       5 |  785 | `		PH7_GenCompileError(&(*pGen),E_PARSE,GenStateStringEscLine(&(*pGen),zBad,bHeredoc),` |
|       - |  786 | `			"syntax error, unexpected token \"%z\", expecting number",&sTok);` |
|       3 |  787 | `	}else{` |
|      13 |  788 | `		PH7_GenCompileError(&(*pGen),E_PARSE,GenStateStringEscLine(&(*pGen),zBad,bHeredoc),` |
|       - |  789 | `			"syntax error, unexpected token \"%z\", expecting \"-\" or identifier or variable or number",&sTok);` |
|       - |  790 | `	}` |
|      35 |  791 | `	return SXERR_ABORT;` |
|       1 |  792 | `}` |
|       - |  793 | `/*` |
|       - |  794 | ` * Compile the SUBSCRIPT of a simple-syntax "$name[offset]" interpolation:` |
|       - |  795 | ` * [zKey,zKeyEnd) is the raw text between the brackets, and the php-equivalent` |
|       - |  796 | ` * "[...]" source is appended to pOut.` |
|       - |  797 | ` *` |
|       - |  798 | `` * php does NOT parse this as an expression. zend's `encaps_var_offset` grammar`` |
|       - |  799 | ` * admits exactly four things and nothing else -- a bare LABEL (always the STRING` |
|       - |  800 | ` * key, never a constant), an integer literal, '-' plus an integer literal, or a` |
|       - |  801 | ` * "$name" -- and only a canonical decimal is an INTEGER key. PH7 handed the text` |
|       - |  802 | ` * to the expression compiler, which read every integer SPELLING as a number and` |
|       - |  803 | ` * accepted shapes php rejects outright.` |
|       - |  804 | ` */` |
|     112 |  805 | `static sxi32 GenStateCompileStringOffset(` |
|       - |  806 | `	ph7_gen_state *pGen,` |
|       - |  807 | `	const char *zKey,` |
|       - |  808 | `	const char *zKeyEnd,` |
|       - |  809 | `	SyBlob *pOut,` |
|       - |  810 | `	int bHeredoc` |
|       - |  811 | `	)` |
|       2 |  812 | `{` |
|     114 |  813 | `	const char *z = zKey;` |
|     114 |  814 | `	int bNeg = 0;` |
|     114 |  815 | `	if( z >= zKeyEnd ){` |
|       3 |  816 | `		return GenStateOffsetSyntaxError(&(*pGen),z,zKeyEnd,0,bHeredoc);` |
|       - |  817 | `	}` |
|     112 |  818 | `	if( z[0] == '$' ){` |
|       - |  819 | `		/* "$name" -- the one offset php actually EVALUATES; pass it through */` |
|       9 |  820 | `		const char *zName = &z[1];` |
|       9 |  821 | `		if( zName >= zKeyEnd \|\| !GEN_STRING_LABEL_START(zName[0]) ){` |
|       3 |  822 | `			return GenStateOffsetSyntaxError(&(*pGen),z,zKeyEnd,0,bHeredoc);` |
|       - |  823 | `		}` |
|       7 |  824 | `		z = zName;` |
|       7 |  825 | `		GenStateSkipStringLabel(&z,zKeyEnd);` |
|       7 |  826 | `		if( z != zKeyEnd ){` |
|       3 |  827 | `			return GenStateOffsetSyntaxError(&(*pGen),z,zKeyEnd,1,bHeredoc);` |
|       - |  828 | `		}` |
|       5 |  829 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|       5 |  830 | `		SyBlobAppend(pOut,zKey,(sxu32)(zKeyEnd - zKey));` |
|       5 |  831 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|       5 |  832 | `		return SXRET_OK;` |
|       - |  833 | `	}` |
|     104 |  834 | `	if( z[0] == '-' ){` |
|      15 |  835 | `		bNeg = 1;` |
|      15 |  836 | `		z++;` |
|       7 |  837 | `	}` |
|     104 |  838 | `	if( z < zKeyEnd && GenStateIsBaseDigit((unsigned char)z[0],10) ){` |
|      67 |  839 | `		const char *zNum = z;` |
|      67 |  840 | `		z = GenStateScanOffsetNumber(z,zKeyEnd);` |
|      67 |  841 | `		if( z != zKeyEnd ){` |
|      15 |  842 | `			return GenStateOffsetSyntaxError(&(*pGen),z,zKeyEnd,1,bHeredoc);` |
|       - |  843 | `		}` |
|       - |  844 | `		/* "-0" is php's string key "-0", not the integer 0: zend negates a LONG` |
|       - |  845 | `		 * num-string but spells a ZERO one back out as text. */` |
|      53 |  846 | `		if( GenStateOffsetIsCanonicalInt(zNum,zKeyEnd,bNeg) ){` |
|      25 |  847 | `			SyBlobAppend(pOut,"[",sizeof(char));` |
|      25 |  848 | `			SyBlobAppend(pOut,zKey,(sxu32)(zKeyEnd - zKey));` |
|      25 |  849 | `			SyBlobAppend(pOut,"]",sizeof(char));` |
|      13 |  850 | `		}else{` |
|      29 |  851 | `			SyBlobAppend(pOut,"['",sizeof(char)*2);` |
|      29 |  852 | `			SyBlobAppend(pOut,zKey,(sxu32)(zKeyEnd - zKey));` |
|      29 |  853 | `			SyBlobAppend(pOut,"']",sizeof(char)*2);` |
|       - |  854 | `		}` |
|      53 |  855 | `		return SXRET_OK;` |
|       - |  856 | `	}` |
|      38 |  857 | `	if( bNeg ){` |
|       - |  858 | `		/* php's '-' takes a NUMBER and nothing else */` |
|       5 |  859 | `		return GenStateOffsetSyntaxError(&(*pGen),z,zKeyEnd,2,bHeredoc);` |
|       - |  860 | `	}` |
|      34 |  861 | `	if( GEN_STRING_LABEL_START(z[0]) ){` |
|      26 |  862 | `		GenStateSkipStringLabel(&z,zKeyEnd);` |
|      26 |  863 | `		if( z != zKeyEnd ){` |
|       3 |  864 | `			return GenStateOffsetSyntaxError(&(*pGen),z,zKeyEnd,1,bHeredoc);` |
|       - |  865 | `		}` |
|       - |  866 | `		/* A bare word is the STRING key, never a constant */` |
|      24 |  867 | `		SyBlobAppend(pOut,"['",sizeof(char)*2);` |
|      24 |  868 | `		SyBlobAppend(pOut,zKey,(sxu32)(zKeyEnd - zKey));` |
|      24 |  869 | `		SyBlobAppend(pOut,"']",sizeof(char)*2);` |
|      24 |  870 | `		return SXRET_OK;` |
|       - |  871 | `	}` |
|       9 |  872 | `	return GenStateOffsetSyntaxError(&(*pGen),z,zKeyEnd,0,bHeredoc);` |
|      58 |  873 | `}` |
|       - |  874 | `/*` |
|       - |  875 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|       - |  876 | ` */` |
|   46662 |  877 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|       5 |  878 | `{` |
|       - |  879 | `	ph7_value *pConstObj;` |
|   46667 |  880 | `	sxu32 nIdx = 0;` |
|       - |  881 | `	/* Reserve a new constant */` |
|   46667 |  882 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   46667 |  883 | `	if( pConstObj == 0 ){` |
|     ! 0 |  884 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  885 | `		return 0;` |
|       - |  886 | `	}` |
|   46667 |  887 | `	(*pCount)++;` |
|   46667 |  888 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|       - |  889 | `	/* Emit the load constant instruction */` |
|   46667 |  890 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   46667 |  891 | `	return pConstObj;` |
|   23336 |  892 | `}` |
|       - |  893 | `/*` |
|       - |  894 | ` * Compile a double quoted/heredoc string.` |
|       - |  895 | ` * According to the PHP language reference manual` |
|       - |  896 | ` * Heredoc` |
|       - |  897 | ` *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier` |
|       - |  898 | ` *  is provided, then a newline. The string itself follows, and then the same identifier again` |
|       - |  899 | ` *  to close the quotation.` |
|       - |  900 | ` *  The closing identifier must begin in the first column of the line. Also, the identifier must` |
|       - |  901 | ` *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric` |
|       - |  902 | ` *  characters and underscores, and must start with a non-digit character or underscore.` |
|       - |  903 | ` *  Warning` |
|       - |  904 | ` *  It is very important to note that the line with the closing identifier must contain` |
|       - |  905 | ` *  no other characters, except possibly a semicolon (;). That means especially that the identifier` |
|       - |  906 | ` *  may not be indented, and there may not be any spaces or tabs before or after the semicolon.` |
|       - |  907 | ` *  It's also important to realize that the first character before the closing identifier must` |
|       - |  908 | ` *  be a newline as defined by the local operating system. This is \n on UNIX systems, including Mac OS X.` |
|       - |  909 | ` *  The closing delimiter (possibly followed by a semicolon) must also be followed by a newline.` |
|       - |  910 | ` *  If this rule is broken and the closing identifier is not "clean", it will not be considered a closing` |
|       - |  911 | ` *  identifier, and PHP will continue looking for one. If a proper closing identifier is not found before` |
|       - |  912 | ` *  the end of the current file, a parse error will result at the last line.` |
|       - |  913 | ` *  Heredocs can not be used for initializing class properties.` |
|       - |  914 | ` * Double quoted` |
|       - |  915 | ` *  If the string is enclosed in double-quotes ("), PHP will interpret more escape sequences for special characters:` |
|       - |  916 | ` *  Escaped characters Sequence 	Meaning` |
|       - |  917 | ` *  \n linefeed (LF or 0x0A (10) in ASCII)` |
|       - |  918 | ` *  \r carriage return (CR or 0x0D (13) in ASCII)` |
|       - |  919 | ` *  \t horizontal tab (HT or 0x09 (9) in ASCII)` |
|       - |  920 | ` *  \v vertical tab (VT or 0x0B (11) in ASCII)` |
|       - |  921 | ` *  \e escape (ESC or 0x1B (27) in ASCII)` |
|       - |  922 | ` *  \f form feed (FF or 0x0C (12) in ASCII)` |
|       - |  923 | ` *  \\ backslash` |
|       - |  924 | ` *  \$ dollar sign` |
|       - |  925 | ` *  \" double-quote` |
|       - |  926 | ` *  \[0-7]{1,3} 	the sequence of characters matching the regular expression is a character in octal notation,` |
|       - |  927 | ` *      which silently overflows to fit in a byte (e.g. "\400" === "\000")` |
|       - |  928 | ` *  \x[0-9A-Fa-f]{1,2} 	the sequence of characters matching the regular expression is a character in hexadecimal notation` |
|       - |  929 | ` *  \u{[0-9A-Fa-f]+} 	the sequence of characters matching the regular expression is a Unicode codepoint,` |
|       - |  930 | ` *      which will be output to the string as that codepoint's UTF-8 representation` |
|       - |  931 | ` * As in single quoted strings, escaping any other character will result in the backslash being printed too.` |
|       - |  932 | ` * (The PH7-ism "\oNNN" octal form is gone: a literal "\o" now round-trips like php 8.)` |
|       - |  933 | ` * The most important feature of double-quoted strings is the fact that variable names will be expanded.` |
|       - |  934 | ` * See string parsing for details.` |
|       - |  935 | ` */` |
|       - |  936 | `/* bHeredoc: php strips the backslash from '\"' only when '"' is the active` |
|       - |  937 | ` * quote character; a heredoc has none, so '\"' stays verbatim there. */` |
|   45152 |  938 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen,int bHeredoc)` |
|       5 |  939 | `{` |
|   45157 |  940 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|       - |  941 | `	const char *zIn,*zCur,*zEnd;` |
|   45157 |  942 | `	ph7_value *pObj = 0;` |
|       - |  943 | `	sxi32 iCons;` |
|       - |  944 | `	sxi32 nInterp;   /* how many of iCons came from an interpolated EXPRESSION */` |
|       - |  945 | `	sxi32 rc;` |
|       - |  946 | `	/* Delimit the string */` |
|   45157 |  947 | `	zIn  = pStr->zString;` |
|   45157 |  948 | `	zEnd = &zIn[pStr->nByte];` |
|   45157 |  949 | `	if( zIn >= zEnd ){` |
|       - |  950 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|       - |  951 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|       - |  952 | `		 * literal table from growing when many "" literals appear in the source.` |
|       - |  953 | `		 */` |
|     599 |  954 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     599 |  955 | `		return SXRET_OK;` |
|       - |  956 | `	}` |
|   44563 |  957 | `	zCur = 0;` |
|       - |  958 | `	/* Compile the node */` |
|   44563 |  959 | `	iCons = 0;` |
|   44563 |  960 | `	nInterp = 0;` |
|   23965 |  961 | `	for(;;){` |
|   71753 |  962 | `		zCur = zIn;` |
|  309187 |  963 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  240925 |  964 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|     106 |  965 | `				break;` |
|  240725 |  966 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|    3290 |  967 | `				(GEN_STRING_LABEL_START(zIn[1]) \|\| zIn[1] == '{') ){` |
|    1645 |  968 | `					break;` |
|       - |  969 | `			}` |
|  237439 |  970 | `			zIn++;` |
|       5 |  971 | `		}` |
|   71753 |  972 | `		if( zIn > zCur ){` |
|   31865 |  973 | `			if( pObj == 0 ){` |
|   30637 |  974 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|   30637 |  975 | `				if( pObj == 0 ){` |
|     ! 0 |  976 | `					return SXERR_ABORT;` |
|       - |  977 | `				}` |
|   15316 |  978 | `			}` |
|   31865 |  979 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   15930 |  980 | `		}` |
|   71753 |  981 | `		if( zIn >= zEnd ){` |
|   44525 |  982 | `			break;` |
|       - |  983 | `		}` |
|   27233 |  984 | `		if( zIn[0] == '\\' ){` |
|   23747 |  985 | `			const char *zPtr = 0;` |
|       - |  986 | `			sxu32 n;` |
|   23747 |  987 | `			zIn++;` |
|   23747 |  988 | `			if( pObj == 0 ){` |
|   16035 |  989 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|   16035 |  990 | `				if( pObj == 0 ){` |
|     ! 0 |  991 | `					return SXERR_ABORT;` |
|       - |  992 | `				}` |
|    8015 |  993 | `			}` |
|   23747 |  994 | `			if( zIn >= zEnd ){` |
|       - |  995 | `				/* Lone backslash at the very end of the body: php keeps it */` |
|       3 |  996 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|       3 |  997 | `				break;` |
|       - |  998 | `			}` |
|   23745 |  999 | `			n = sizeof(char); /* size of conversion */` |
|   23745 | 1000 | `			switch( zIn[0] ){` |
|      74 | 1001 | `			case '$':` |
|       - | 1002 | `				/* Dollar sign */` |
|     153 | 1003 | `				PH7_MemObjStringAppend(pObj,"$",sizeof(char));` |
|     153 | 1004 | `				break;` |
|      80 | 1005 | `			case '\\':` |
|       - | 1006 | `				/* A literal backslash */` |
|     165 | 1007 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|     165 | 1008 | `				break;` |
|       1 | 1009 | `			case 'e':` |
|       - | 1010 | `				/* Escape (ESC) ASCII code 27 */` |
|       3 | 1011 | `				PH7_MemObjStringAppend(pObj,"\x1b",sizeof(char));` |
|       3 | 1012 | `				break;` |
|       5 | 1013 | `			case 'f':` |
|       - | 1014 | `				/* Form-feed (FF)[ctrl+l] ASCII code 12 */` |
|      11 | 1015 | `				PH7_MemObjStringAppend(pObj,"\f",sizeof(char));` |
|      11 | 1016 | `				break;` |
|   10831 | 1017 | `			case 'n':` |
|       - | 1018 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|   21667 | 1019 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|   21667 | 1020 | `				break;` |
|      41 | 1021 | `			case 'r':` |
|       - | 1022 | `				/* Carriage return (CR)[ctrl+m] ASCII code 13 */` |
|      87 | 1023 | `				PH7_MemObjStringAppend(pObj,"\r",sizeof(char));` |
|      87 | 1024 | `				break;` |
|      41 | 1025 | `			case 't':` |
|       - | 1026 | `				/* Horizontal tab (HT)[ctrl+i] ASCII code 9 */` |
|      87 | 1027 | `				PH7_MemObjStringAppend(pObj,"\t",sizeof(char));` |
|      87 | 1028 | `				break;` |
|       3 | 1029 | `			case 'v':` |
|       - | 1030 | `				/* Vertical tab(VT)[ctrl+k] ASCII code 11 */` |
|       7 | 1031 | `				PH7_MemObjStringAppend(pObj,"\v",sizeof(char));` |
|       7 | 1032 | `				break;` |
|     208 | 1033 | `			case '"':` |
|     421 | 1034 | `				if( bHeredoc ){` |
|       - | 1035 | `					/* No active quote char in a heredoc: php keeps \" verbatim */` |
|       5 | 1036 | `					PH7_MemObjStringAppend(pObj,"\\\"",sizeof(char)*2);` |
|       3 | 1037 | `				}else{` |
|       - | 1038 | `					/* Double quote */` |
|     417 | 1039 | `					PH7_MemObjStringAppend(pObj,"\"",sizeof(char));` |
|       - | 1040 | `				}` |
|     421 | 1041 | `				break;` |
|     103 | 1042 | `			case '0': case '1': case '2': case '3':` |
|       - | 1043 | `			case '4': case '5': case '6': case '7': {` |
|       - | 1044 | `				/* \[0-7]{1,3}: a character in octal notation. A value above \377` |
|       - | 1045 | `				 * warns and wraps to the low byte, matching php 8. */` |
|     209 | 1046 | `				int c = 0;` |
|       - | 1047 | `				char cOut;` |
|     461 | 1048 | `				for( zPtr = zIn ; zPtr < &zIn[3*sizeof(char)] ; zPtr++ ){` |
|     439 | 1049 | `					if( zPtr >= zEnd \|\| zPtr[0] < '0' \|\| zPtr[0] > '7' ){` |
|      94 | 1050 | `						break;` |
|       - | 1051 | `					}` |
|     255 | 1052 | `					c = c * 8 + (zPtr[0] - '0');` |
|     129 | 1053 | `				}` |
|     209 | 1054 | `				if( c > 0xFF ){` |
|       - | 1055 | `					SyString sSeq;` |
|       3 | 1056 | `					SyStringInitFromBuf(&sSeq,zIn,(sxu32)(zPtr-zIn));` |
|       3 | 1057 | `					PH7_GenCompileError(&(*pGen),E_WARNING,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|       - | 1058 | `						"Octal escape sequence overflow \\%z is greater than \\377",&sSeq);` |
|       3 | 1059 | `					c &= 0xFF;` |
|       1 | 1060 | `				}` |
|     209 | 1061 | `				cOut = (char)c; /* value byte, independent of host endianness */` |
|     209 | 1062 | `				PH7_MemObjStringAppend(pObj,&cOut,sizeof(char));` |
|     209 | 1063 | `				n = (sxu32)(zPtr-zIn);` |
|     209 | 1064 | `				break;` |
|       - | 1065 | `			}` |
|     438 | 1066 | `			case 'x':` |
|    1316 | 1067 | `				if( &zIn[1] < zEnd && SyisHex((unsigned char)zIn[1]) ){` |
|       - | 1068 | `					/* \x[0-9A-Fa-f]{1,2}: a character in hexadecimal notation */` |
|     876 | 1069 | `					int c = SyHexToint(zIn[1]);` |
|       - | 1070 | `					char cOut;` |
|     876 | 1071 | `					n += sizeof(char);` |
|     876 | 1072 | `					if( &zIn[2] < zEnd && SyisHex((unsigned char)zIn[2]) ){` |
|     872 | 1073 | `						c = (c << 4) + SyHexToint(zIn[2]);` |
|     872 | 1074 | `						n += sizeof(char);` |
|     434 | 1075 | `					}` |
|     876 | 1076 | `					cOut = (char)c; /* value byte, independent of host endianness */` |
|     876 | 1077 | `					PH7_MemObjStringAppend(pObj,&cOut,sizeof(char));` |
|     440 | 1078 | `				}else{` |
|       - | 1079 | `					/* Not an escape: keep the backslash, as php does */` |
|       5 | 1080 | `					PH7_MemObjStringAppend(pObj,"\\x",sizeof(char)*2);` |
|       - | 1081 | `				}` |
|     880 | 1082 | `				break;` |
|      30 | 1083 | `			case 'u':` |
|      60 | 1084 | `				if( &zIn[1] < zEnd && zIn[1] == '{'` |
|      85 | 1085 | `				 && !(&zIn[2] < zEnd && zIn[2] == '$') ){` |
|       - | 1086 | `					/* \u{codepoint}: UTF-8 encoding of the given codepoint (php 7+).` |
|       - | 1087 | `					 * php encodes surrogates verbatim, so the only invalid value` |
|       - | 1088 | `					 * is > U+10FFFF; malformed/empty braces are a compile error.` |
|       - | 1089 | `					 * "\u{$..." is excluded above: php treats it as a literal \u` |
|       - | 1090 | `					 * followed by {$...} curly interpolation. */` |
|      57 | 1091 | `					sxu32 nCp = 0;` |
|      57 | 1092 | `					zPtr = &zIn[2];` |
|     269 | 1093 | `					while( zPtr < zEnd && SyisHex((unsigned char)zPtr[0]) ){` |
|     214 | 1094 | `						if( nCp <= 0x10FFFF ){` |
|       - | 1095 | `							/* stop accumulating once out of range: keeps a long` |
|       - | 1096 | `							 * digit run from wrapping sxu32 */` |
|     214 | 1097 | `							nCp = nCp * 16 + (sxu32)SyHexToint(zPtr[0]);` |
|     106 | 1098 | `						}` |
|     214 | 1099 | `						zPtr++;` |
|       2 | 1100 | `					}` |
|      57 | 1101 | `					if( zPtr == &zIn[2] \|\| zPtr >= zEnd \|\| zPtr[0] != '}' ){` |
|       - | 1102 | `						/* Error recorded (nErr>0 fails the whole compile); consume the` |
|       - | 1103 | `						 * malformed sequence so later errors are still reported. */` |
|       3 | 1104 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|       - | 1105 | `							"Invalid UTF-8 codepoint escape sequence");` |
|       3 | 1106 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 1107 | `							return SXERR_ABORT;` |
|       - | 1108 | `						}` |
|       3 | 1109 | `						n = (sxu32)(zPtr-zIn);` |
|       3 | 1110 | `						if( zPtr < zEnd && zPtr[0] == '}' ){` |
|       3 | 1111 | `							n += sizeof(char);` |
|       1 | 1112 | `						}` |
|       3 | 1113 | `						break;` |
|       - | 1114 | `					}` |
|      54 | 1115 | `					n = (sxu32)(&zPtr[1]-zIn); /* 'u{...}' incl. closing brace */` |
|      54 | 1116 | `					if( nCp > 0x10FFFF ){` |
|       3 | 1117 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|       - | 1118 | `							"Invalid UTF-8 codepoint escape sequence: Codepoint too large");` |
|       3 | 1119 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 1120 | `							return SXERR_ABORT;` |
|       - | 1121 | `						}` |
|       3 | 1122 | `						break;` |
|       - | 1123 | `					}` |
|       - | 1124 | `					{` |
|       - | 1125 | `						char zUtf[4];` |
|      51 | 1126 | `						sxu8 *zOut = (sxu8 *)zUtf;` |
|      51 | 1127 | `						SX_WRITE_UTF8(zOut,nCp);` |
|      51 | 1128 | `						PH7_MemObjStringAppend(pObj,zUtf,(sxu32)(zOut-(sxu8 *)zUtf));` |
|       - | 1129 | `					}` |
|      26 | 1130 | `				}else{` |
|       - | 1131 | `					/* Not an escape: keep the backslash, as php does */` |
|       7 | 1132 | `					PH7_MemObjStringAppend(pObj,"\\u",sizeof(char)*2);` |
|       - | 1133 | `				}` |
|      57 | 1134 | `				break;` |
|      15 | 1135 | `			default:` |
|       - | 1136 | `				/* Unrecognized escape: keep the backslash, as php does.` |
|       - | 1137 | `				 * zIn[-1] is the backslash itself, so both bytes are contiguous` |
|       - | 1138 | `				 * in the source buffer — one batched append. */` |
|      31 | 1139 | `				PH7_MemObjStringAppend(pObj,&zIn[-1],sizeof(char)*2);` |
|      30 | 1140 | `				break;` |
|       - | 1141 | `			}` |
|       - | 1142 | `			/* Advance the stream cursor */` |
|   23745 | 1143 | `			zIn += n;` |
|   23745 | 1144 | `			continue;` |
|       - | 1145 | `		}` |
|    3491 | 1146 | `		if( zIn[0] == '{' ){` |
|       - | 1147 | `			/* Curly syntax */` |
|       - | 1148 | `			const char *zExpr;` |
|     209 | 1149 | `			sxi32 iNest = 1;` |
|     209 | 1150 | `			zIn++;` |
|     209 | 1151 | `			zExpr = zIn;` |
|       - | 1152 | `			/* Synchronize with the next closing curly braces */` |
|    1807 | 1153 | `			while( zIn < zEnd ){` |
|    1807 | 1154 | `				if( zIn[0] == '{' ){` |
|       - | 1155 | `					/* Increment nesting level */` |
|       3 | 1156 | `					iNest++;` |
|    1806 | 1157 | `				}else if(zIn[0] == '}' ){` |
|       - | 1158 | `					/* Decrement nesting level */` |
|     211 | 1159 | `					iNest--;` |
|     211 | 1160 | `					if( iNest <= 0 ){` |
|     209 | 1161 | `						break;` |
|       - | 1162 | `					}` |
|       1 | 1163 | `				}` |
|    1601 | 1164 | `				zIn++;` |
|       3 | 1165 | `			}` |
|       - | 1166 | `			/* Process the expression */` |
|     209 | 1167 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|     209 | 1168 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1169 | `				return SXERR_ABORT;` |
|       - | 1170 | `			}` |
|     209 | 1171 | `			if( rc != SXERR_EMPTY ){` |
|     209 | 1172 | `				++iCons;` |
|     209 | 1173 | `				++nInterp;` |
|     103 | 1174 | `			}` |
|     209 | 1175 | `			if( zIn < zEnd ){` |
|       - | 1176 | `				/* Jump the trailing curly */` |
|     209 | 1177 | `				zIn++;` |
|     103 | 1178 | `			}` |
|     106 | 1179 | `		}else{` |
|       - | 1180 | `			/*` |
|       - | 1181 | `			 * Simple syntax. php's simple "$var…" form is a LEXER rule, not an` |
|       - | 1182 | `			 * expression: it takes the variable name plus EXACTLY ONE accessor —` |
|       - | 1183 | `			 * "$var", "$var[offset]" or "$var->prop" — and stops there. Everything` |
|       - | 1184 | `			 * past that one accessor is literal text: a second subscript` |
|       - | 1185 | `			 * ("$o->p[0]" is the property then a literal "[0]"), a second arrow` |
|       - | 1186 | `			 * ("$o->p->q" is "$o->p" then a literal "->q"), any "::" at all` |
|       - | 1187 | `			 * ("$c::C" is the VALUE of $c then a literal "::C", never a class` |
|       - | 1188 | `			 * constant), and any "{…}" ("$x{'a'}" is $x then literal). Only the` |
|       - | 1189 | `			 * complex "{$expr}" form reaches those, and it is handled above.` |
|       - | 1190 | `			 *` |
|       - | 1191 | `			 * PHL used to loop here, greedily chaining accessors, so those four` |
|       - | 1192 | `			 * shapes silently answered something else than php on VALID source.` |
|       - | 1193 | `			 */` |
|    3285 | 1194 | `			const char *zExpr = zIn;` |
|    3285 | 1195 | `			int bSubscript = 0;` |
|       - | 1196 | `			/*` |
|       - | 1197 | `			 * "${...}" string interpolation (every form: ${name}, ${expr}, ${$x}) was` |
|       - | 1198 | `			 * DEPRECATED by php 8.2 in favor of the canonical "{$...}". PHL targets php's` |
|       - | 1199 | `			 * *non-deprecated* surface, so it is a hard parse error here — never silently` |
|       - | 1200 | `			 * rewritten. The canonical "{$var}" reaches this compiler by a different path` |
|       - | 1201 | `			 * and is unaffected. Checked before the scan: '{' is not an accessor, so the` |
|       - | 1202 | `			 * cursor would otherwise stop on the '$' and read the brace as literal text.` |
|       - | 1203 | `			 */` |
|    3285 | 1204 | `			if( &zIn[1] < zEnd && zIn[0] == '$' && zIn[1] == '{' ){` |
|       3 | 1205 | `				PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - | 1206 | `					"syntax error, \"${\" string interpolation was removed in php 8.2, use \"{$...}\" instead");` |
|       3 | 1207 | `				return SXERR_ABORT;` |
|       - | 1208 | `			}` |
|       - | 1209 | `			/* Jump leading dollars */` |
|    6561 | 1210 | `			while( zIn < zEnd && zIn[0] == '$' ){` |
|    3283 | 1211 | `				zIn++;` |
|       5 | 1212 | `			}` |
|       - | 1213 | `			/* Variable name */` |
|    3283 | 1214 | `			GenStateSkipStringLabel(&zIn,zEnd);` |
|       - | 1215 | `			/* …then at most ONE accessor */` |
|    3339 | 1216 | `			if( zIn < zEnd && zIn[0] == '[' ){` |
|     114 | 1217 | `				sxi32 iSquare = 1;` |
|     114 | 1218 | `				bSubscript = 1;` |
|     114 | 1219 | `				zIn++;` |
|     568 | 1220 | `				while( zIn < zEnd ){` |
|     566 | 1221 | `					if( zIn[0] == '[' ){` |
|       3 | 1222 | `						iSquare++;` |
|     565 | 1223 | `					}else if (zIn[0] == ']' ){` |
|     114 | 1224 | `						iSquare--;` |
|     114 | 1225 | `						if( iSquare <= 0 ){` |
|     112 | 1226 | `							break;` |
|       - | 1227 | `						}` |
|       1 | 1228 | `					}` |
|     456 | 1229 | `					zIn++;` |
|       2 | 1230 | `				}` |
|     114 | 1231 | `				if( zIn < zEnd ){` |
|     112 | 1232 | `					zIn++;` |
|      55 | 1233 | `				}` |
|    3224 | 1234 | `			}else if( &zIn[2] < zEnd && zIn[0] == '-' && zIn[1] == '>'` |
|     114 | 1235 | `				&& GEN_STRING_LABEL_START(zIn[2]) ){` |
|       - | 1236 | `				/* Member access operator '->'. php takes it only when a LABEL` |
|       - | 1237 | `				 * follows; with anything else -- a digit, a space, a '{', the end` |
|       - | 1238 | `				 * of the body -- the arrow is literal TEXT and the interpolation is` |
|       - | 1239 | `				 * just the variable. PHL swallowed the bare '->' and handed the` |
|       - | 1240 | `				 * compiler a dangling "$o->", fatalling` |
|       - | 1241 | `				 * "'->': Missing/Invalid member name" on source php RUNS. */` |
|      84 | 1242 | `				zIn += 2;` |
|      84 | 1243 | `				GenStateSkipStringLabel(&zIn,zEnd);` |
|      40 | 1244 | `			}` |
|       - | 1245 | `			/*` |
|       - | 1246 | `			 * "$a[offset]" -- php parses a simple-syntax subscript with its OWN tiny` |
|       - | 1247 | ``			 * grammar (zend's `encaps_var_offset`), never as an expression, so rewrite`` |
|       - | 1248 | `			 * it into the equivalent php source and hand THAT to the compiler. PH7 fed` |
|       - | 1249 | `			 * the raw text straight in, which read every integer SPELLING as a number` |
|       - | 1250 | `			 * ("$a[007]" / "$a[0x1A]" / "$a[1_000]" / "$a[-0]" answered the integer` |
|       - | 1251 | `			 * keys 7/26/1000/0 where php reads the STRING keys "007"/"0x1A"/"1_000"/` |
|       - | 1252 | `			 * "-0"), read a non-ASCII bare word as a CONSTANT ("$a[\xc3\xa9]" raised` |
|       - | 1253 | `			 * "Undefined constant"), and quietly accepted every shape php rejects` |
|       - | 1254 | `			 * ("$a[ 0]", "$a[0 ]", "$a['x']", "$a[+1]", "$a[-$k]", "$a[[]", "$a[]").` |
|       - | 1255 | `			 */` |
|    3283 | 1256 | `			if( bSubscript ){` |
|     114 | 1257 | `				const char *zBr = zExpr;` |
|       - | 1258 | `				SyBlob sSub;` |
|     356 | 1259 | `				while( zBr < zIn && zBr[0] != '[' ){` |
|     244 | 1260 | `					zBr++;` |
|       2 | 1261 | `				}` |
|     114 | 1262 | `				if( zIn <= zBr \|\| zIn[-1] != ']' ){` |
|       - | 1263 | `					/* Unterminated: the body ended inside the brackets. php names the` |
|       - | 1264 | `					  * closing quote it reached instead; there is no offending TOKEN to` |
|       - | 1265 | `					  * quote here, and zIn is one past the body, so never read it. */` |
|     ! 0 | 1266 | `					PH7_GenCompileError(&(*pGen),E_PARSE,GenStateStringEscLine(&(*pGen),zBr,bHeredoc),` |
|       - | 1267 | `						"syntax error, unexpected end of string, expecting \"-\" or identifier or variable or number");` |
|     ! 0 | 1268 | `					return SXERR_ABORT;` |
|       - | 1269 | `				}` |
|     114 | 1270 | `				SyBlobInit(&sSub,&pGen->pVm->sAllocator);` |
|     114 | 1271 | `				SyBlobAppend(&sSub,zExpr,(sxu32)(zBr - zExpr));` |
|     114 | 1272 | `				rc = GenStateCompileStringOffset(&(*pGen),&zBr[1],&zIn[-1],&sSub,bHeredoc);` |
|     114 | 1273 | `				if( rc != SXRET_OK ){` |
|      35 | 1274 | `					SyBlobRelease(&sSub);` |
|      35 | 1275 | `					return SXERR_ABORT;` |
|       - | 1276 | `				}` |
|     119 | 1277 | `				rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,` |
|      78 | 1278 | `					(const char *)SyBlobData(&sSub),` |
|      78 | 1279 | `					(const char *)SyBlobData(&sSub) + SyBlobLength(&sSub));` |
|      80 | 1280 | `				SyBlobRelease(&sSub);` |
|      80 | 1281 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1282 | `					return SXERR_ABORT;` |
|       - | 1283 | `				}` |
|      80 | 1284 | `				if( rc != SXERR_EMPTY ){` |
|      80 | 1285 | `					++iCons;` |
|      80 | 1286 | `					++nInterp;` |
|      39 | 1287 | `				}` |
|      80 | 1288 | `				pObj = 0;` |
|      80 | 1289 | `				continue;` |
|       - | 1290 | `			}` |
|       - | 1291 | `			/* Process the expression */` |
|    3171 | 1292 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|    3171 | 1293 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1294 | `				return SXERR_ABORT;` |
|       - | 1295 | `			}` |
|    3171 | 1296 | `			if( rc != SXERR_EMPTY ){` |
|    3171 | 1297 | `				++iCons;` |
|    3171 | 1298 | `				++nInterp;` |
|    1583 | 1299 | `			}` |
|       - | 1300 | `		}` |
|       - | 1301 | `		/* Invalidate the previously used constant */` |
|    3377 | 1302 | `		pObj = 0;` |
|       5 | 1303 | `	}/*for(;;)*/` |
|   44527 | 1304 | `	if( iCons > 1 ){` |
|       - | 1305 | `		/* Concatenate all compiled constants */` |
|    2491 | 1306 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|   43284 | 1307 | `	}else if( iCons == 1 && nInterp == 1 ){` |
|       - | 1308 | `		/* A string that is nothing but one interpolation ("$x") still has to` |
|       - | 1309 | `		 * PRODUCE A STRING. With no CAT to force the conversion the operand was` |
|       - | 1310 | ``		 * left on the stack untouched, so `$s = "$x"` handed back $x's own type:`` |
|       - | 1311 | `		 * "$arr" stayed an array (and skipped php's "Array to string conversion"` |
|       - | 1312 | `		 * warning), "$int" stayed an int, "$res" stayed a resource. */` |
|      81 | 1313 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CVT_STR,0,0,0,0);` |
|      39 | 1314 | `	}` |
|       - | 1315 | `	/* Node successfully compiled */` |
|   44527 | 1316 | `	return SXRET_OK;` |
|   22581 | 1317 | `}` |
|       - | 1318 | `/*` |
|       - | 1319 | ` * Compile a double quoted string.` |
|       - | 1320 | ` *  See the block-comment above for more information.` |
|       - | 1321 | ` */` |
|   45080 | 1322 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 | 1323 | `{` |
|       - | 1324 | `	sxi32 rc;` |
|   45085 | 1325 | `	rc = GenStateCompileString(&(*pGen),0/*bHeredoc*/);` |
|   22540 | 1326 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - | 1327 | `	/* Compilation result */` |
|   45085 | 1328 | `	return rc;` |
|       5 | 1329 | `}` |
|       - | 1330 | `/*` |
|       - | 1331 | ` * Compile a Heredoc string.` |
|       - | 1332 | ` *  See the block-comment above for more information.` |
|       - | 1333 | ` */` |
|      76 | 1334 | `PH7_PRIVATE sxi32 PH7_CompileHereDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       4 | 1335 | `{` |
|       - | 1336 | `	SyString sOrig, sStripped;` |
|       - | 1337 | `	sxi32 rc;` |
|      80 | 1338 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|      80 | 1339 | `	if( rc != SXRET_OK ){` |
|       6 | 1340 | `		return rc;` |
|       - | 1341 | `	}` |
|       - | 1342 | `	/* Temporarily swap in the dedented body so GenStateCompileString` |
|       - | 1343 | `	 * (which reads pGen->pIn->sData directly) sees the stripped content.` |
|       - | 1344 | `	 * Restore before returning so downstream code that references pIn is` |
|       - | 1345 | `	 * unaffected, including on the error path. */` |
|      76 | 1346 | `	sOrig = pGen->pIn->sData;` |
|      76 | 1347 | `	pGen->pIn->sData = sStripped;` |
|      76 | 1348 | `	rc = GenStateCompileString(&(*pGen),1/*bHeredoc*/);` |
|      76 | 1349 | `	pGen->pIn->sData = sOrig;` |
|      36 | 1350 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|      76 | 1351 | `	return rc;` |
|      42 | 1352 | `}` |
|       - | 1353 | `/*` |
|       - | 1354 | ` * Compile an array entry whether it is a key or a value.` |
|       - | 1355 | ` *  Notes on array entries.` |
|       - | 1356 | ` *  According to the PHP language reference manual` |
|       - | 1357 | ` *  An array can be created by the array() language construct.` |
|       - | 1358 | ` *  It takes as parameters any number of comma-separated key => value pairs.` |
|       - | 1359 | ` *  array(  key =>  value` |
|       - | 1360 | ` *    , ...` |
|       - | 1361 | ` *    )` |
|       - | 1362 | ` *  A key may be either an integer or a string. If a key is the standard representation` |
|       - | 1363 | ` *  of an integer, it will be interpreted as such (i.e. "8" will be interpreted as 8, while` |
|       - | 1364 | ` *  "08" will be interpreted as "08"). Floats in key are truncated to integer.` |
|       - | 1365 | ` *  The indexed and associative array types are the same type in PHP, which can both` |
|       - | 1366 | ` *  contain integer and string indices.` |
|       - | 1367 | ` *  A value can be any PHP type.` |
|       - | 1368 | ` *  If a key is not specified for a value, the maximum of the integer indices is taken` |
|       - | 1369 | ` *  and the new key will be that value plus 1. If a key that already has an assigned value` |
|       - | 1370 | ` *  is specified, that value will be overwritten.` |
|       - | 1371 | ` */` |
|  141890 | 1372 | `PH7_PRIVATE sxi32 GenStateCompileArrayEntry(` |
|       - | 1373 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 1374 | `	SyToken *pIn,        /* Token stream */` |
|       - | 1375 | `	SyToken *pEnd,       /* End of the token stream */` |
|       - | 1376 | `	sxi32 iFlags,        /* Compilation flags */` |
|       - | 1377 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *) /* Expression tree validator callback */` |
|       - | 1378 | `	)` |
|       5 | 1379 | `{` |
|       - | 1380 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - | 1381 | `	sxi32 rc;` |
|       - | 1382 | `	/* Swap token stream */` |
|  141895 | 1383 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|       - | 1384 | `	/* Compile the expression*/` |
|  141895 | 1385 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|       - | 1386 | `	/* Restore token stream */` |
|  141895 | 1387 | `	RE_SWAP_DELIMITER(pGen);` |
|  141895 | 1388 | `	return rc;` |
|       5 | 1389 | `}` |
|       - | 1390 | `/*` |
|       - | 1391 | ` * Expression tree validator callback for the 'array' language construct.` |
|       - | 1392 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|       - | 1393 | ` * an invalid expression tree and this function will generate the appropriate` |
|       - | 1394 | ` * error message.` |
|       - | 1395 | ` * See the routine responible of compiling the array language construct` |
|       - | 1396 | ` * for more inforation.` |
|       - | 1397 | ` */` |
|      80 | 1398 | `static sxi32 GenStateArrayNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 | 1399 | `{` |
|      85 | 1400 | `	sxi32 rc = GenStateWriteTargetCheck(&(*pGen),pRoot,0);` |
|      85 | 1401 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1402 | `		return rc;` |
|       - | 1403 | `	}` |
|      85 | 1404 | `	if( pRoot->pOp ){` |
|      18 | 1405 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ &&` |
|      12 | 1406 | `			pRoot->pOp->iOp != EXPR_OP_FUNC_CALL /* function() [Symisc extension: i.e: array(&foo())] */` |
|      16 | 1407 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|       - | 1408 | `			/* Unexpected expression */` |
|      13 | 1409 | `			rc = PH7_GenSyntaxError(&(*pGen),pRoot->pStart,"\"->\" or \"?->\" or \"[\"");` |
|      13 | 1410 | `			if( rc != SXERR_ABORT ){` |
|      13 | 1411 | `				rc = SXERR_INVALID;` |
|       5 | 1412 | `			}` |
|       9 | 1413 | `		}` |
|      73 | 1414 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 1415 | `		/* Unexpected expression */` |
|       3 | 1416 | `		rc = PH7_GenSyntaxError(&(*pGen),pRoot->pStart,0);` |
|       3 | 1417 | `		if( rc != SXERR_ABORT ){` |
|       3 | 1418 | `			rc = SXERR_INVALID;` |
|       1 | 1419 | `		}` |
|       1 | 1420 | `	}` |
|      85 | 1421 | `	return rc;` |
|      45 | 1422 | `}` |
|       - | 1423 | `/*` |
|       - | 1424 | ` * Find the top-level '=>' (PH7_TK_ARRAY_OP) that separates an array/list entry's` |
|       - | 1425 | ` * key from its value within [pStart,pEnd). The scan skips any '=>' nested inside` |
|       - | 1426 | ` * brackets/parens/braces, inside an arrow-function signature (fn(...) =>), or` |
|       - | 1427 | ` * inside a match() {...} arm — none of which are key/value separators. Returns a` |
|       - | 1428 | ` * pointer to the '=>' token, or pEnd if the entry has no top-level separator.` |
|       - | 1429 | ` */` |
|  190790 | 1430 | `PH7_PRIVATE SyToken * GenStateFindTopLevelArrow(SyToken *pStart,SyToken *pEnd)` |
|       5 | 1431 | `{` |
|  190795 | 1432 | `	SyToken *pCur = pStart;` |
|  190795 | 1433 | `	sxi32 iNest = 0;` |
|  446955 | 1434 | `	while( pCur < pEnd ){` |
|  284221 | 1435 | `		if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|   28025 | 1436 | `			return pCur;` |
|       - | 1437 | `		}` |
|       - | 1438 | `		/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|       - | 1439 | `		 * The '=>' inside an arrow function introduces the expression body,` |
|       - | 1440 | `		 * not an entry separator. Skip past the signature.` |
|       - | 1441 | `		 */` |
|  256201 | 1442 | `		if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
|     243 | 1443 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|     243 | 1444 | `			SyToken *pFn = pCur;` |
|       - | 1445 | ``			/* Only a real `[static] fn[&](` opens an arrow function; `$fn`,`` |
|       - | 1446 | ``			 * `C::fn` and friends are plain names whose '=>' IS the separator. */`` |
|     243 | 1447 | `			if( PH7_TokenOpensArrowFunc(pStart,pCur,pEnd) ){` |
|      37 | 1448 | `				if( nKw == PH7_TKWRD_STATIC ){` |
|     ! 0 | 1449 | `					pFn = &pCur[1];` |
|     ! 0 | 1450 | `				}` |
|      37 | 1451 | `				pCur = pFn + 1; /* past 'fn' */` |
|      37 | 1452 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_AMPER) ){` |
|     ! 0 | 1453 | `					pCur++;` |
|     ! 0 | 1454 | `				}` |
|      37 | 1455 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|      37 | 1456 | `					pCur++;` |
|      37 | 1457 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|       - | 1458 | `						PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|      37 | 1459 | `					if( pCur < pEnd ){` |
|      37 | 1460 | `						pCur++;` |
|      18 | 1461 | `					}` |
|      18 | 1462 | `				}` |
|      37 | 1463 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_COLON) ){` |
|     ! 0 | 1464 | `					pCur++;` |
|     ! 0 | 1465 | `					if( pCur < pEnd && (pCur->nType & PH7_TK_OP)` |
|     ! 0 | 1466 | `						&& pCur->sData.nByte == 1` |
|     ! 0 | 1467 | `						&& pCur->sData.zString[0] == '?' ){` |
|     ! 0 | 1468 | `						pCur++;` |
|     ! 0 | 1469 | `					}` |
|     ! 0 | 1470 | `					if( pCur < pEnd` |
|     ! 0 | 1471 | `						&& (pCur->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|     ! 0 | 1472 | `						pCur++;` |
|     ! 0 | 1473 | `					}` |
|     ! 0 | 1474 | `				}` |
|       - | 1475 | `				/* The rest of the entry is the arrow-function body — no outer` |
|       - | 1476 | `				 * key to extract. */` |
|      37 | 1477 | `				return pEnd;` |
|       - | 1478 | `			}` |
|       - | 1479 | `			/* Match expression (PHP 8.0): the '=>' inside match arms is not an` |
|       - | 1480 | `			 * entry separator. Skip past the full match span. */` |
|     207 | 1481 | `			if( nKw == PH7_TKWRD_MATCH ){` |
|       5 | 1482 | `				pCur++; /* past 'match' */` |
|       5 | 1483 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|       3 | 1484 | `					pCur++;` |
|       3 | 1485 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|       - | 1486 | `						PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|       3 | 1487 | `					if( pCur < pEnd ){` |
|       3 | 1488 | `						pCur++;` |
|       1 | 1489 | `					}` |
|       1 | 1490 | `				}` |
|       5 | 1491 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_OCB) ){` |
|       3 | 1492 | `					pCur++;` |
|       3 | 1493 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|       - | 1494 | `						PH7_TK_OCB,PH7_TK_CCB,&pCur);` |
|       3 | 1495 | `					if( pCur < pEnd ){` |
|       3 | 1496 | `						pCur++;` |
|       1 | 1497 | `					}` |
|       1 | 1498 | `				}` |
|       5 | 1499 | `				continue;` |
|       - | 1500 | `			}` |
|      99 | 1501 | `		}` |
|  256161 | 1502 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|    2749 | 1503 | `			iNest++;` |
|  254789 | 1504 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/) ){` |
|       - | 1505 | `			/* Don't worry about mismatched brackets here, the expression` |
|       - | 1506 | `			 * parser will shortly detect any syntax error. */` |
|    2749 | 1507 | `			iNest--;` |
|    1372 | 1508 | `		}` |
|  256161 | 1509 | `		pCur++;` |
|       5 | 1510 | `	}` |
|  162739 | 1511 | `	return pEnd;` |
|   95400 | 1512 | `}` |
|       - | 1513 | `/*` |
|       - | 1514 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|       - | 1515 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|       - | 1516 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|       - | 1517 | ` */` |
|   70016 | 1518 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|       5 | 1519 | `{` |
|       - | 1520 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|       - | 1521 | `	SyToken *pKey,*pCur;` |
|   70021 | 1522 | `	sxi32 iEmitRef = 0;` |
|   70021 | 1523 | `	sxi32 iSpread = 0;` |
|   70021 | 1524 | `	sxi32 nPair = 0;` |
|       - | 1525 | `	sxi32 rc;` |
|   70021 | 1526 | `	xValidator = 0;` |
|  103694 | 1527 | `	for(;;){` |
|       - | 1528 | `		/* Jump leading commas. Exactly ONE separates two entries; a second one (or a comma` |
|       - | 1529 | `		 * at the very start) means an EMPTY element, which php rejects outright — PH7 just` |
|       - | 1530 | ``		 * skipped them, so `array(,)` and `array(1,,2)` compiled silently. A TRAILING comma`` |
|       - | 1531 | `		 * is legal and is handled by the loop exiting on the next pass. */` |
|   68686 | 1532 | `		{` |
|  207393 | 1533 | `			int nSkip = 0;` |
|  327627 | 1534 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|  120239 | 1535 | `				nSkip++;` |
|  120239 | 1536 | `				pGen->pIn++;` |
|       5 | 1537 | `			}` |
|  207393 | 1538 | `			if( nSkip > 1 \|\| (nSkip > 0 && nPair < 1) ){` |
|     ! 0 | 1539 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn[-1].nLine,` |
|       - | 1540 | `					"Cannot use empty array elements in arrays");` |
|     ! 0 | 1541 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1542 | `					return SXERR_ABORT;` |
|       - | 1543 | `				}` |
|     ! 0 | 1544 | `				return SXRET_OK;` |
|       - | 1545 | `			}` |
|       - | 1546 | `		}` |
|  207393 | 1547 | `		pCur = pGen->pIn;` |
|  207393 | 1548 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|       - | 1549 | `			/* No more entry to process */` |
|   70003 | 1550 | `			break;` |
|       - | 1551 | `		}` |
|  137395 | 1552 | `		if( pCur >= pGen->pIn ){` |
|     ! 0 | 1553 | `			continue;` |
|       - | 1554 | `		}` |
|       - | 1555 | `		/* Compile the key if available */` |
|  137395 | 1556 | `		pKey = pCur;` |
|  137395 | 1557 | `		pCur = GenStateFindTopLevelArrow(pCur,pGen->pIn);` |
|  137395 | 1558 | `		rc = SXERR_EMPTY;` |
|  137395 | 1559 | `		if( pCur < pGen->pIn ){` |
|    4119 | 1560 | `			if( pKey == pCur ){` |
|       - | 1561 | ``				/* `array( => 2)`: the entry STARTS with '=>', so it has no key. php rejects`` |
|       - | 1562 | `				 * it; PH7 warned about a "Missing entry key" and compiled on, accepting` |
|       - | 1563 | ``				 * source php refuses. (The `else if` below could never see this: the arrow`` |
|       - | 1564 | `				 * IS found here, so control never reached it.)` |
|       - | 1565 | `				 * php names the literal's own closer, so short syntax expects ']'. */` |
|       3 | 1566 | `				const char *zClose = (pGen->pEnd && (pGen->pEnd->nType & PH7_TK_CSB))` |
|       - | 1567 | `					? "\"]\"" : "\")\"";` |
|       3 | 1568 | `				rc = PH7_GenSyntaxError(&(*pGen),pCur,zClose);` |
|       3 | 1569 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1570 | `					return SXERR_ABORT;` |
|       - | 1571 | `				}` |
|       3 | 1572 | `				return SXRET_OK;` |
|       - | 1573 | `			}` |
|    4117 | 1574 | `			if( &pCur[1] >= pGen->pIn ){` |
|       - | 1575 | ``				/* `array(1 => )`: php names the token that SHOULD have started the value —`` |
|       - | 1576 | `				 * the ')' or ']' closing the literal — not the '=>' it just read. Passing 0` |
|       - | 1577 | `				 * makes the helper reach for the token past this entry's slice. */` |
|      14 | 1578 | `				rc = PH7_GenSyntaxError(&(*pGen),0,0);` |
|      14 | 1579 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1580 | `					return SXERR_ABORT;` |
|       - | 1581 | `				}` |
|      14 | 1582 | `				return SXRET_OK;` |
|       - | 1583 | `			}` |
|       - | 1584 | `			/* Compile the expression holding the key */` |
|    4107 | 1585 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|       - | 1586 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|    4107 | 1587 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1588 | `				return SXERR_ABORT;` |
|       - | 1589 | `			}` |
|    4107 | 1590 | `			pCur++; /* Jump the '=>' operator */` |
|    2056 | 1591 | `		}else{` |
|       - | 1592 | `			/* Reset back the cursor and point to the entry value */` |
|  133281 | 1593 | `			pCur = pKey;` |
|       - | 1594 | `		}` |
|  137383 | 1595 | `		if( rc == SXERR_EMPTY ){` |
|       - | 1596 | `			/* No key given: load the nil, TAGGED so LOAD_MAP knows this is an absent key` |
|       - | 1597 | ``			 * (auto-index) rather than an explicit `null =>` one, which php deprecates. */`` |
|  133281 | 1598 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,PH7_LOADC_NOKEY,0 /* nil index */,0,0);` |
|   66638 | 1599 | `		}` |
|  137383 | 1600 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
|       - | 1601 | `			/* Insertion by reference, [i.e: $a = array(&$x);] */` |
|      89 | 1602 | `			xValidator = GenStateArrayNodeValidator; /* Only variable are allowed */` |
|      89 | 1603 | `			iEmitRef = 1;` |
|      89 | 1604 | `			pCur++; /* Jump the '&' token */` |
|      89 | 1605 | `			if( pCur >= pGen->pIn ){` |
|       - | 1606 | `				/* Missing value */` |
|       - | 1607 | ``				/* php reports the token that actually stopped it (`array(&)` -> the`` |
|       - | 1608 | `				 * ')'), not a hand-written "missing referenced variable" fatal. */` |
|       3 | 1609 | `				rc = PH7_GenSyntaxError(&(*pGen),pCur < pGen->pIn ? pCur : 0,0);` |
|       3 | 1610 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1611 | `					return SXERR_ABORT;` |
|       - | 1612 | `				}` |
|       3 | 1613 | `				return SXRET_OK;` |
|       - | 1614 | `			}` |
|      41 | 1615 | `		}` |
|       - | 1616 | `		/* Detect array unpack: '...$expr' as the entry value (PHP 7.4+, with` |
|       - | 1617 | `		 * string-key support since PHP 8.1). The parser strips the '...' inside` |
|       - | 1618 | `		 * ExprExtractNode; we only need to know it's there so we can emit` |
|       - | 1619 | `		 * PH7_OP_FLAG_SPREAD after the value, instructing LOAD_MAP to merge the` |
|       - | 1620 | `		 * resulting hashmap rather than insert it as a scalar entry. */` |
|  137381 | 1621 | `		iSpread = (pCur < pGen->pIn && (pCur->nType & PH7_TK_ELLIPSIS)) ? 1 : 0;` |
|  137381 | 1622 | `		if( iSpread && (rc != SXERR_EMPTY \|\| iEmitRef) ){` |
|       - | 1623 | `			/* '[k => ...$a]' and '[&...$a]' are syntax errors in PHP — the` |
|       - | 1624 | `			 * '...' token cannot follow either '=>' or '&' inside an array` |
|       - | 1625 | `			 * literal. Emit the same Parse-error wording PHP uses so the` |
|       - | 1626 | `			 * output is engine-portable. */` |
|       6 | 1627 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pCur->nLine,` |
|       - | 1628 | `				"syntax error, unexpected token \"...\"");` |
|       6 | 1629 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1630 | `				return SXERR_ABORT;` |
|       - | 1631 | `			}` |
|       6 | 1632 | `			return SXRET_OK;` |
|       - | 1633 | `		}` |
|       - | 1634 | ``		/* Compile indice value. A BY-REF element (`'k' => &$a[$i]`) is an`` |
|       - | 1635 | `		 * lvalue: php VIVIFIES a missing subscript when a reference is taken,` |
|       - | 1636 | `		 * so compile it in write context (LOAD_IDX iP2=1, create-if-missing)` |
|       - | 1637 | `		 * instead of a read-only load — which also keeps the undefined-key` |
|       - | 1638 | `		 * warning (a read-only diagnostic) from false-firing here. */` |
|  206063 | 1639 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,` |
|   68686 | 1640 | `			iEmitRef ? EXPR_FLAG_LOAD_IDX_STORE` |
|       - | 1641 | `			         : EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,` |
|   68686 | 1642 | `			xValidator);` |
|  137377 | 1643 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1644 | `			return SXERR_ABORT;` |
|       - | 1645 | `		}` |
|  137377 | 1646 | `		if( iSpread ){` |
|       - | 1647 | `			/* Mark the value on TOS as a spread source; LOAD_MAP merges it. */` |
|      77 | 1648 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FLAG_SPREAD,0,0,0,0);` |
|  137340 | 1649 | `		}else if( iEmitRef ){` |
|       - | 1650 | `			/* Emit the load reference instruction */` |
|      85 | 1651 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|      40 | 1652 | `		}` |
|  137377 | 1653 | `		xValidator = 0;` |
|  137377 | 1654 | `		iEmitRef = 0;` |
|  137377 | 1655 | `		iSpread = 0;` |
|  137377 | 1656 | `		nPair++;` |
|       5 | 1657 | `	}` |
|       - | 1658 | `	/* Emit the load map instruction */` |
|   70003 | 1659 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|       - | 1660 | `	/* Node successfully compiled */` |
|   70003 | 1661 | `	return SXRET_OK;` |
|   35013 | 1662 | `}` |
|       - | 1663 | `/*` |
|       - | 1664 | ` * Compile the 'array' language construct.` |
|       - | 1665 | ` *	 According to the PHP language reference manual` |
|       - | 1666 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|       - | 1667 | ` *   values to keys. This type is optimized for several different uses; it can` |
|       - | 1668 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|       - | 1669 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|       - | 1670 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|       - | 1671 | ` */` |
|   62554 | 1672 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 | 1673 | `{` |
|       - | 1674 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   62559 | 1675 | `	pGen->pIn += 2;` |
|   62559 | 1676 | `	pGen->pEnd--;` |
|   31277 | 1677 | `	SXUNUSED(iCompileFlag);` |
|       - | 1678 | ``	/* php: a stray token in an `array( ... )` element is `... expecting ")"`. */`` |
|       - | 1679 | `	{` |
|   62559 | 1680 | `		const char *zSave = pGen->zClauseCloser;` |
|       - | 1681 | `		sxi32 rc;` |
|   62559 | 1682 | `		pGen->zClauseCloser = "\")\"";` |
|   62559 | 1683 | `		rc = GenStateCompileArrayBody(pGen);` |
|   62559 | 1684 | `		pGen->zClauseCloser = zSave;` |
|   62559 | 1685 | `		return rc;` |
|       - | 1686 | `	}` |
|       5 | 1687 | `}` |
|       - | 1688 | `/*` |
|       - | 1689 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|       - | 1690 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|       - | 1691 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|       - | 1692 | ` */` |
|    7462 | 1693 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 | 1694 | `{` |
|       - | 1695 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|    7467 | 1696 | `	pGen->pIn++;` |
|    7467 | 1697 | `	pGen->pEnd--;` |
|    3731 | 1698 | `	SXUNUSED(iCompileFlag);` |
|       - | 1699 | ``	/* php: a stray token in a `[ ... ]` element is `... expecting "]"`. */`` |
|       - | 1700 | `	{` |
|    7467 | 1701 | `		const char *zSave = pGen->zClauseCloser;` |
|       - | 1702 | `		sxi32 rc;` |
|    7467 | 1703 | `		pGen->zClauseCloser = "\"]\"";` |
|    7467 | 1704 | `		rc = GenStateCompileArrayBody(pGen);` |
|    7467 | 1705 | `		pGen->zClauseCloser = zSave;` |
|    7467 | 1706 | `		return rc;` |
|       - | 1707 | `	}` |
|       5 | 1708 | `}` |
|       - | 1709 | `/*` |
|       - | 1710 | ` * Expression tree validator callback for the 'list' language construct.` |
|       - | 1711 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|       - | 1712 | ` * an invalid expression tree and this function will generate the appropriate` |
|       - | 1713 | ` * error message.` |
|       - | 1714 | ` * See the routine responible of compiling the list language construct` |
|       - | 1715 | ` * for more inforation.` |
|       - | 1716 | ` */` |
|     360 | 1717 | `static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 | 1718 | `{` |
|     365 | 1719 | `	sxi32 rc = GenStateWriteTargetCheck(&(*pGen),pRoot,0);` |
|     365 | 1720 | `	if( rc != SXRET_OK ){` |
|       3 | 1721 | `		return rc;` |
|       - | 1722 | `	}` |
|     363 | 1723 | `	if( pRoot->pOp ){` |
|      54 | 1724 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */` |
|      28 | 1725 | `			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){` |
|       - | 1726 | `				/* Unexpected expression */` |
|     ! 0 | 1727 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 1728 | `					"Assignments can only happen to writable values");` |
|     ! 0 | 1729 | `				if( rc != SXERR_ABORT ){` |
|     ! 0 | 1730 | `					rc = SXERR_INVALID;` |
|     ! 0 | 1731 | `				}` |
|       2 | 1732 | `		}` |
|     336 | 1733 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 1734 | `		/* Unexpected expression */` |
|       6 | 1735 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 1736 | `			"Assignments can only happen to writable values");` |
|       6 | 1737 | `		if( rc != SXERR_ABORT ){` |
|       6 | 1738 | `			rc = SXERR_INVALID;` |
|       2 | 1739 | `		}` |
|       2 | 1740 | `	}` |
|     363 | 1741 | `	return rc;` |
|     185 | 1742 | `}` |
|       - | 1743 | `/*` |
|       - | 1744 | ` * Compile the 'list' language construct.` |
|       - | 1745 | ` *  According to the PHP language reference` |
|       - | 1746 | ` *  list(): Assign variables as if they were an array.` |
|       - | 1747 | ` *  list() is used to assign a list of variables in one operation.` |
|       - | 1748 | ` *  Description` |
|       - | 1749 | ` *   array list (mixed $varname [, mixed $... ] )` |
|       - | 1750 | ` *   Like array(), this is not really a function, but a language construct.` |
|       - | 1751 | ` *   list() is used to assign a list of variables in one operation.` |
|       - | 1752 | ` *  Parameters` |
|       - | 1753 | ` *   $varname: A variable.` |
|       - | 1754 | ` *  Return Values` |
|       - | 1755 | ` *   The assigned array.` |
|       - | 1756 | ` */` |
|       - | 1757 | `/* Nested list entry recorded during first pass of list body compilation */` |
|       - | 1758 | `struct NestedListEntry {` |
|       - | 1759 | `	sxi32 nIndex;        /* Position in the outer list (0-based) */` |
|       - | 1760 | `	SyToken *pStart;     /* Token range: start of nested construct */` |
|       - | 1761 | `	SyToken *pEnd;       /* Token range: past closing delimiter */` |
|       - | 1762 | `	sxi32 isShort;       /* 1 if [...] form, 0 if list(...) form */` |
|       - | 1763 | `};` |
|       - | 1764 | `/*` |
|       - | 1765 | ` * Compile the body of a *keyed* list/short-list destructuring (PHP 7.1), where` |
|       - | 1766 | `` * every entry has the form `keyExpr => target`. The source array is on the stack`` |
|       - | 1767 | ` * top on entry and remains there on exit, mirroring the positional LOAD_LIST` |
|       - | 1768 | ` * path so the caller's teardown is unchanged. For each entry: DUP the source,` |
|       - | 1769 | ` * push the key, LOAD_IDX to fetch source[key] (NULL on a missing key, silently,` |
|       - | 1770 | ` * like a normal subscript read), then assign the fetched value to the target — a` |
|       - | 1771 | ` * nested [...]/list() recurses, a simple lvalue uses the same STORE fold as a` |
|       - | 1772 | ` * normal assignment (the value sits below the lvalue-load, exactly as in` |
|       - | 1773 | ` * GenStateEmitExprCode where the assignment RHS precedes the LHS load).` |
|       - | 1774 | ` */` |
|      34 | 1775 | `static sxi32 GenStateCompileKeyedListBody(ph7_gen_state *pGen)` |
|       3 | 1776 | `{` |
|       - | 1777 | `	SyToken *pNext;` |
|       - | 1778 | `	sxi32 rc;` |
|      79 | 1779 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|       - | 1780 | `		SyToken *pArrow,*pTarget;` |
|       - | 1781 | ``		/* Split `keyExpr => target` at the top-level '=>' */`` |
|      45 | 1782 | `		pArrow = GenStateFindTopLevelArrow(pGen->pIn,pNext);` |
|      45 | 1783 | `		pTarget = &pArrow[1];` |
|      45 | 1784 | `		if( pArrow <= pGen->pIn \|\| pTarget >= pNext ){` |
|       - | 1785 | ``			/* Empty key (`[ => $v]`) or empty value (`["k" =>]`): PHP rejects`` |
|       - | 1786 | `			 * both. Reject rather than silently emitting unbalanced bytecode. */` |
|     ! 0 | 1787 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - | 1788 | `				"Cannot use empty array entries in keyed array assignment");` |
|     ! 0 | 1789 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - | 1790 | `		}` |
|       - | 1791 | `		/* DUP the source array (it is on the stack top) */` |
|      45 | 1792 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|       - | 1793 | `		/* Compile the key expression; it is pushed above the DUP'd source */` |
|      45 | 1794 | `		rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pArrow,EXPR_FLAG_RDONLY_LOAD,0);` |
|      45 | 1795 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1796 | `			return SXERR_ABORT;` |
|       - | 1797 | `		}` |
|       - | 1798 | `		/* LOAD_IDX: pop the key, replace the DUP'd source with source[key].` |
|       - | 1799 | `		 * iP2=7 is the keyed-destructuring read context: an array source reads like` |
|       - | 1800 | ``		 * iP2=0 (missing key loads NULL silently, matching a normal `$arr[$k]` read;`` |
|       - | 1801 | `		 * PHP also emits an "Undefined array key" warning here, PHL omits it — §3.7),` |
|       - | 1802 | `		 * but a NON-array source yields NULL + a per-key "Cannot use <type> as array"` |
|       - | 1803 | `		 * warning instead of char-indexing a string (matching PHP's OP_LOAD_LIST path). */` |
|      45 | 1804 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,7,0,0);` |
|      45 | 1805 | `		if( pTarget < pNext && ( (pTarget->nType & PH7_TK_OSB)` |
|      40 | 1806 | `			\|\| ( (pTarget->nType & PH7_TK_KEYWORD)` |
|      21 | 1807 | `				&& SX_PTR_TO_INT(pTarget->pUserData) == PH7_TKWRD_LIST ) ) ){` |
|       - | 1808 | `			/* Nested destructuring:  ["k" => [ ... ]]  or  ["k" => list( ... )].` |
|       - | 1809 | `			 * Treat source[key] as the inner body's source, then drop the` |
|       - | 1810 | `			 * leftover it leaves behind (mirrors the positional nested path). */` |
|       5 | 1811 | `			sxi32 isShort = (pTarget->nType & PH7_TK_OSB) != 0;` |
|       5 | 1812 | `			SyToken *pSavedIn = pGen->pIn;` |
|       5 | 1813 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|       5 | 1814 | `			pGen->pIn = pTarget;` |
|       5 | 1815 | `			pGen->pEnd = pNext;` |
|       5 | 1816 | `			rc = isShort ? PH7_CompileShortList(&(*pGen),0)` |
|       2 | 1817 | `			             : PH7_CompileList(&(*pGen),0);` |
|       5 | 1818 | `			pGen->pIn = pSavedIn;` |
|       5 | 1819 | `			pGen->pEnd = pSavedEnd;` |
|       5 | 1820 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1821 | `				return SXERR_ABORT;` |
|       - | 1822 | `			}` |
|       5 | 1823 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       3 | 1824 | `		}else{` |
|       - | 1825 | `			/* Simple lvalue target ($v / $o->p / $a[i] / Cls::$s). source[key]` |
|       - | 1826 | `			 * is already on the stack as the value; compiling the target appends` |
|       - | 1827 | `			 * its lvalue-load, which we fold into a STORE just as a normal` |
|       - | 1828 | `			 * assignment does. */` |
|       - | 1829 | `			VmInstr *pInstr;` |
|      41 | 1830 | `			sxi32 iVmOp = PH7_OP_STORE;` |
|      41 | 1831 | `			sxi32 iP1 = 0, iP2 = 0;` |
|      41 | 1832 | `			void *p3 = 0;` |
|      41 | 1833 | `			rc = GenStateCompileArrayEntry(&(*pGen),pTarget,pNext,` |
|       - | 1834 | `				EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|      41 | 1835 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 1836 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - | 1837 | `			}` |
|      41 | 1838 | `			if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|      41 | 1839 | `				if( pInstr->iOp == PH7_OP_MEMBER ){` |
|       6 | 1840 | `					iP2 = 1; /* member store: keep MEMBER, store value below it */` |
|      38 | 1841 | `				}else if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       3 | 1842 | `					iVmOp = PH7_OP_STORE_IDX;` |
|       3 | 1843 | `					iP1 = pInstr->iP1;` |
|       3 | 1844 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       2 | 1845 | `				}else{` |
|      34 | 1846 | `					p3 = pInstr->p3; /* named store: $v = value */` |
|      34 | 1847 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - | 1848 | `				}` |
|      19 | 1849 | `			}` |
|      41 | 1850 | `			PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|       - | 1851 | `			/* STORE leaves the assigned value on the stack top; drop it so the` |
|       - | 1852 | `			 * source array is back on top for the next entry. */` |
|      41 | 1853 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       - | 1854 | `		}` |
|      45 | 1855 | `		pGen->pIn = &pNext[1];` |
|       3 | 1856 | `	}` |
|      37 | 1857 | `	return SXRET_OK;` |
|      20 | 1858 | `}` |
|       - | 1859 | `/*` |
|       - | 1860 | ` * Shared body for list() and short list [...] compilation.` |
|       - | 1861 | ` * Assumes pGen->pIn and pGen->pEnd are already positioned past` |
|       - | 1862 | ` * the opening delimiter and before the closing delimiter.` |
|       - | 1863 | ` */` |
|     230 | 1864 | `static sxi32 GenStateCompileListBody(ph7_gen_state *pGen)` |
|       5 | 1865 | `{` |
|       - | 1866 | `	SySet sNested; /* Dynamically-sized container of NestedListEntry */` |
|       - | 1867 | `	SyToken *pNext;` |
|       - | 1868 | `	SyToken *pClassifyIn;` |
|     235 | 1869 | `	sxi32 nKeyed = 0, nPositional = 0, nEmpty = 0;` |
|       - | 1870 | `	sxi32 nExpr;` |
|       - | 1871 | `	sxi32 rc;` |
|       - | 1872 | ``	/* First pass: classify entries as keyed (`k => v`), positional, or empty`` |
|       - | 1873 | `	 * skip slots ([,]). A list level must be entirely keyed or entirely` |
|       - | 1874 | `	 * positional — PHP fatals on a mix, and on an empty slot inside a keyed` |
|       - | 1875 | `	 * list. */` |
|     235 | 1876 | `	pClassifyIn = pGen->pIn;` |
|     627 | 1877 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|     397 | 1878 | `		if( pGen->pIn >= pNext ){` |
|      13 | 1879 | `			nEmpty++;` |
|     391 | 1880 | `		}else if( GenStateFindTopLevelArrow(pGen->pIn,pNext) < pNext ){` |
|      45 | 1881 | `			nKeyed++;` |
|      24 | 1882 | `		}else{` |
|     343 | 1883 | `			nPositional++;` |
|       - | 1884 | `		}` |
|     397 | 1885 | `		pGen->pIn = &pNext[1];` |
|       5 | 1886 | `	}` |
|     235 | 1887 | `	pGen->pIn = pClassifyIn;` |
|     235 | 1888 | `	if( nKeyed > 0 && nEmpty > 0 ){` |
|     ! 0 | 1889 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - | 1890 | `			"Cannot use empty array entries in keyed array assignment");` |
|     ! 0 | 1891 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - | 1892 | `	}` |
|     235 | 1893 | `	if( nKeyed > 0 && nPositional > 0 ){` |
|     ! 0 | 1894 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - | 1895 | `			"Cannot mix keyed and unkeyed array entries in assignments");` |
|     ! 0 | 1896 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - | 1897 | `	}` |
|     235 | 1898 | `	if( nKeyed > 0 ){` |
|      37 | 1899 | `		return GenStateCompileKeyedListBody(pGen);` |
|       - | 1900 | `	}` |
|     201 | 1901 | `	nExpr = 0;` |
|     201 | 1902 | `	SySetInit(&sNested,&pGen->pVm->sAllocator,sizeof(struct NestedListEntry));` |
|     551 | 1903 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|     355 | 1904 | `		if( pGen->pIn < pNext ){` |
|       - | 1905 | `			/* Check for nested list() */` |
|     343 | 1906 | `			if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       3 | 1907 | `				SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - | 1908 | `				/* Record this nested list for post-processing */` |
|       3 | 1909 | `				SyToken *pListEnd = 0;` |
|       3 | 1910 | `				if( &pGen->pIn[1] < pNext && (pGen->pIn[1].nType & PH7_TK_LPAREN) ){` |
|       3 | 1911 | `					PH7_DelimitNestedTokens(pGen->pIn+2,pNext,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       1 | 1912 | `				}` |
|       3 | 1913 | `				if( pListEnd ){` |
|       - | 1914 | `					struct NestedListEntry sEntry;` |
|       3 | 1915 | `					sEntry.nIndex = nExpr;` |
|       3 | 1916 | `					sEntry.pStart = pGen->pIn;` |
|       3 | 1917 | `					sEntry.pEnd = pListEnd + 1;` |
|       3 | 1918 | `					sEntry.isShort = 0;` |
|       3 | 1919 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|       1 | 1920 | `				}` |
|       - | 1921 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|       3 | 1922 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|     342 | 1923 | `			}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - | 1924 | `				/* Nested short destructuring [...] */` |
|      16 | 1925 | `				SyToken *pBracketEnd = 0;` |
|      16 | 1926 | `				PH7_DelimitNestedTokens(pGen->pIn+1,pNext,PH7_TK_OSB,PH7_TK_CSB,&pBracketEnd);` |
|      16 | 1927 | `				if( pBracketEnd ){` |
|       - | 1928 | `					struct NestedListEntry sEntry;` |
|      16 | 1929 | `					sEntry.nIndex = nExpr;` |
|      16 | 1930 | `					sEntry.pStart = pGen->pIn;` |
|      16 | 1931 | `					sEntry.pEnd = pBracketEnd + 1;` |
|      16 | 1932 | `					sEntry.isShort = 1;` |
|      16 | 1933 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|       7 | 1934 | `				}` |
|       - | 1935 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|      16 | 1936 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       9 | 1937 | `			}else{` |
|       - | 1938 | `				/* Compile the expression holding the variable */` |
|     327 | 1939 | `				rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|     327 | 1940 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 1941 | `					SySetRelease(&sNested);` |
|     ! 0 | 1942 | `					return SXRET_OK;` |
|       - | 1943 | `				}` |
|       - | 1944 | `				{` |
|       - | 1945 | `					/* A property target ($o->p / Cls::$s) is a PURE WRITE here — the` |
|       - | 1946 | `					 * value lands via the following OP_LOAD_LIST's direct slot store.` |
|       - | 1947 | `					 * Tag the member so OP_MEMBER skips the uninitialized-typed read` |
|       - | 1948 | `					 * Error / __get consult and vivifies a missing property (php` |
|       - | 1949 | `					 * assigns without reading). */` |
|     327 | 1950 | `					VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|     327 | 1951 | `					if( pLast && pLast->iOp == PH7_OP_MEMBER && pLast->iP2 == PH7_MEMBER_READ ){` |
|      46 | 1952 | `						pLast->iP2 = PH7_MEMBER_LIST_TARGET;` |
|      22 | 1953 | `					}` |
|       - | 1954 | `				}` |
|       - | 1955 | `			}` |
|     174 | 1956 | `		}else{` |
|       - | 1957 | `			/* Empty entry,load NULL */` |
|      13 | 1958 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);` |
|       - | 1959 | `		}` |
|     355 | 1960 | `		nExpr++;` |
|       - | 1961 | `		/* Advance the stream cursor */` |
|     355 | 1962 | `		pGen->pIn = &pNext[1];` |
|       5 | 1963 | `	}` |
|       - | 1964 | `	/* Emit the LOAD_LIST instruction */` |
|     201 | 1965 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,0,0,0);` |
|       - | 1966 | `	/* After LOAD_LIST, the source array is still on the stack top.` |
|       - | 1967 | `	 * For each nested entry, emit code to extract the sub-array` |
|       - | 1968 | `	 * at the corresponding index and recursively destructure it.` |
|       - | 1969 | `	 */` |
|     201 | 1970 | `	if( SySetUsed(&sNested) > 0 ){` |
|      16 | 1971 | `		struct NestedListEntry *apNested = (struct NestedListEntry *)SySetBasePtr(&sNested);` |
|       - | 1972 | `		sxu32 i;` |
|      32 | 1973 | `		for(i = 0; i < SySetUsed(&sNested); i++){` |
|      18 | 1974 | `			SyToken *pSavedIn = pGen->pIn;` |
|      18 | 1975 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|       - | 1976 | `			ph7_value *pIdx;` |
|       - | 1977 | `			sxu32 nConstIdx;` |
|       - | 1978 | `			/* DUP the source array (it's on stack top) */` |
|      18 | 1979 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|       - | 1980 | `			/* Push the integer index for this nested entry */` |
|      18 | 1981 | `			pIdx = PH7_ReserveConstObj(pGen->pVm,&nConstIdx);` |
|      18 | 1982 | `			if( pIdx == 0 ){` |
|     ! 0 | 1983 | `				PH7_GenCompileError(&(*pGen),E_ERROR,0,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1984 | `				SySetRelease(&sNested);` |
|     ! 0 | 1985 | `				return SXERR_ABORT;` |
|       - | 1986 | `			}` |
|      18 | 1987 | `			PH7_MemObjInitFromInt(pGen->pVm,pIdx,(sxi64)apNested[i].nIndex);` |
|      18 | 1988 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nConstIdx,0,0);` |
|       - | 1989 | `			/* LOAD_IDX: pop index, replace DUP'd source with source[index].` |
|       - | 1990 | `			 * iP2=2 signals the VM to emit an "Undefined array key" warning` |
|       - | 1991 | `			 * when the key is missing (PHP-compatible list destructuring).` |
|       - | 1992 | `			 */` |
|      18 | 1993 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,2,0,0);` |
|       - | 1994 | `			/* Recursively compile the inner list */` |
|      18 | 1995 | `			pGen->pIn = apNested[i].pStart;` |
|      18 | 1996 | `			pGen->pEnd = apNested[i].pEnd;` |
|      18 | 1997 | `			if( apNested[i].isShort ){` |
|      16 | 1998 | `				rc = PH7_CompileShortList(&(*pGen),0);` |
|       9 | 1999 | `			}else{` |
|       3 | 2000 | `				rc = PH7_CompileList(&(*pGen),0);` |
|       - | 2001 | `			}` |
|      18 | 2002 | `			pGen->pIn = pSavedIn;` |
|      18 | 2003 | `			pGen->pEnd = pSavedEnd;` |
|      18 | 2004 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2005 | `				SySetRelease(&sNested);` |
|     ! 0 | 2006 | `				return SXERR_ABORT;` |
|       - | 2007 | `			}` |
|       - | 2008 | `			/* Pop the leftover source[index] from the inner LOAD_LIST */` |
|      18 | 2009 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      10 | 2010 | `		}` |
|       7 | 2011 | `	}` |
|     201 | 2012 | `	SySetRelease(&sNested);` |
|       - | 2013 | `	/* Node successfully compiled */` |
|     201 | 2014 | `	return SXRET_OK;` |
|     120 | 2015 | `}` |
|      48 | 2016 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 | 2017 | `{` |
|       - | 2018 | `	/* Jump the 'list' keyword, the leading '(' and exclude trailing ')' */` |
|      53 | 2019 | `	pGen->pIn += 2;` |
|      53 | 2020 | `	pGen->pEnd--;` |
|      24 | 2021 | `	SXUNUSED(iCompileFlag);` |
|      53 | 2022 | `	return GenStateCompileListBody(pGen);` |
|       5 | 2023 | `}` |
|     182 | 2024 | `PH7_PRIVATE sxi32 PH7_CompileShortList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 | 2025 | `{` |
|       - | 2026 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|     187 | 2027 | `	pGen->pIn++;` |
|     187 | 2028 | `	pGen->pEnd--;` |
|      91 | 2029 | `	SXUNUSED(iCompileFlag);` |
|     187 | 2030 | `	return GenStateCompileListBody(pGen);` |
|       5 | 2031 | `}` |
|       - | 2032 | `/*` |
|       - | 2033 | ` * assert() source-text rendering.` |
|       - | 2034 | ` *` |
|       - | 2035 | ` * php compiles a DIRECT assert() call with a copy of the argument's AST, and a` |
|       - | 2036 | `` * failing assertion reports zend_ast_export() of that AST — `assert(1 == 2)`,`` |
|       - | 2037 | `` * `assert($x)`, `assert('')` — which is exactly the information the message`` |
|       - | 2038 | ` * exists to carry. PHL has no AST copy at runtime, so the compiler renders the` |
|       - | 2039 | ` * argument's TOKEN SPAN here, at compile time, normalizing to php's export` |
|       - | 2040 | ` * shape (each rule probed against php 8.5):` |
|       - | 2041 | ` *   - literal values fold the way php's AST holds them: numbers render from` |
|       - | 2042 | ` *     their parsed VALUE (0x10 -> 16, 1e3 -> 1000.0, 1_000 -> 1000, an` |
|       - | 2043 | ` *     int64-overflowing literal -> float), strings render single-quoted with` |
|       - | 2044 | ` *     their PROCESSED contents (\ and ' re-escaped), array(...) -> [...].` |
|       - | 2045 | ` *   - one space around binary operators, ", " between arguments/elements, no` |
|       - | 2046 | `` *     space inside ()/[] or around ->/?->/::/casts, `and`/`or` -> `&&`/`\|\|`,`` |
|       - | 2047 | ` *     a trailing comma is dropped, redundant OUTERMOST parens are dropped.` |
|       - | 2048 | ` * Accepted divergences from zend_ast_export on exotic input (message text` |
|       - | 2049 | ` * only, never behavior): redundant INNER parens are kept (php re-derives` |
|       - | 2050 | ` * grouping from precedence), interpolated "$x" strings and heredocs render as` |
|       - | 2051 | `` * written (php exports its interpolation AST), `new C` does not grow php's`` |
|       - | 2052 | `` * trailing `()`, and constant folding beyond single literals is not applied`` |
|       - | 2053 | `` * (php renders `'' . ''` as `''`).`` |
|       - | 2054 | ` */` |
|       - | 2055 | `/* Spacing classes: a space is inserted between two tokens when either side` |
|       - | 2056 | ` * FORCEs one (binary operators, the slot after a comma) or both sides are` |
|       - | 2057 | ` * operand-like (WANT). Grouping punctuation and glue operators contribute` |
|       - | 2058 | ` * NONE on their tight side. */` |
|       - | 2059 | `#define ASRT_SP_NONE  0` |
|       - | 2060 | `#define ASRT_SP_WANT  1` |
|       - | 2061 | `#define ASRT_SP_FORCE 2` |
|       - | 2062 | `enum AssertTokClass {` |
|       - | 2063 | `	ASRT_START = 0, /* virtual class before the first token */` |
|       - | 2064 | `	ASRT_OPERAND,   /* literals, identifiers, keywords */` |
|       - | 2065 | `	ASRT_BINOP,     /* == + . && ? : => instanceof ... */` |
|       - | 2066 | `	ASRT_UNARY,     /* ! ~ @ - + & casts, '$', '...' — glue after */` |
|       - | 2067 | `	ASRT_OPEN,      /* ( [ */` |
|       - | 2068 | `	ASRT_CLOSE,     /* ) ] */` |
|       - | 2069 | `	ASRT_GLUE,      /* -> ?-> :: ++ -- \ — glue both sides */` |
|       - | 2070 | `	ASRT_COMMA      /* , — glue before, force after */` |
|       - | 2071 | `};` |
|       - | 2072 | `static const sxu8 aAsrtBefore[] = { ASRT_SP_NONE, ASRT_SP_WANT, ASRT_SP_FORCE, ASRT_SP_WANT,` |
|       - | 2073 | `	ASRT_SP_NONE, ASRT_SP_NONE, ASRT_SP_NONE, ASRT_SP_NONE };` |
|       - | 2074 | `static const sxu8 aAsrtAfter[]  = { ASRT_SP_NONE, ASRT_SP_WANT, ASRT_SP_FORCE, ASRT_SP_NONE,` |
|       - | 2075 | `	ASRT_SP_NONE, ASRT_SP_WANT, ASRT_SP_NONE, ASRT_SP_FORCE };` |
|       - | 2076 | `/*` |
|       - | 2077 | ` * Append one PROCESSED string-value byte, re-escaped for a single-quoted` |
|       - | 2078 | ` * rendering: php's export escapes only backslash and the quote itself; every` |
|       - | 2079 | ` * other byte (including control characters) is emitted raw.` |
|       - | 2080 | ` */` |
|      42 | 2081 | `static void AssertRenderQuotedByte(SyBlob *pOut,int c)` |
|       2 | 2082 | `{` |
|      44 | 2083 | `	char ch = (char)c;` |
|      44 | 2084 | `	if( c == '\\' \|\| c == '\'' ){` |
|       3 | 2085 | `		SyBlobAppend(pOut,"\\",1);` |
|       1 | 2086 | `	}` |
|      44 | 2087 | `	SyBlobAppend(pOut,&ch,1);` |
|      44 | 2088 | `}` |
|       - | 2089 | `/* Append the UTF-8 encoding of a \u{...} code point (value bytes, re-escaped). */` |
|     ! 0 | 2090 | `static void AssertRenderUtf8(SyBlob *pOut,sxu32 c)` |
|     ! 0 | 2091 | `{` |
|     ! 0 | 2092 | `	if( c < 0x80 ){` |
|     ! 0 | 2093 | `		AssertRenderQuotedByte(pOut,(int)c);` |
|     ! 0 | 2094 | `	}else if( c < 0x800 ){` |
|     ! 0 | 2095 | `		AssertRenderQuotedByte(pOut,(int)(0xc0 \| (c >> 6)));` |
|     ! 0 | 2096 | `		AssertRenderQuotedByte(pOut,(int)(0x80 \| (c & 0x3f)));` |
|     ! 0 | 2097 | `	}else if( c < 0x10000 ){` |
|     ! 0 | 2098 | `		AssertRenderQuotedByte(pOut,(int)(0xe0 \| (c >> 12)));` |
|     ! 0 | 2099 | `		AssertRenderQuotedByte(pOut,(int)(0x80 \| ((c >> 6) & 0x3f)));` |
|     ! 0 | 2100 | `		AssertRenderQuotedByte(pOut,(int)(0x80 \| (c & 0x3f)));` |
|     ! 0 | 2101 | `	}else{` |
|     ! 0 | 2102 | `		AssertRenderQuotedByte(pOut,(int)(0xf0 \| (c >> 18)));` |
|     ! 0 | 2103 | `		AssertRenderQuotedByte(pOut,(int)(0x80 \| ((c >> 12) & 0x3f)));` |
|     ! 0 | 2104 | `		AssertRenderQuotedByte(pOut,(int)(0x80 \| ((c >> 6) & 0x3f)));` |
|     ! 0 | 2105 | `		AssertRenderQuotedByte(pOut,(int)(0x80 \| (c & 0x3f)));` |
|       - | 2106 | `	}` |
|     ! 0 | 2107 | `}` |
|       - | 2108 | `/*` |
|       - | 2109 | ` * Render a single-quoted-source string (or nowdoc body): only \\ and \' are` |
|       - | 2110 | ` * escape sequences there; any other backslash is a literal byte.` |
|       - | 2111 | ` */` |
|       2 | 2112 | `static void AssertRenderSglString(SyBlob *pOut,const char *z,sxu32 n)` |
|       1 | 2113 | `{` |
|       3 | 2114 | `	sxu32 i = 0;` |
|       3 | 2115 | `	SyBlobAppend(pOut,"'",1);` |
|      11 | 2116 | `	while( i < n ){` |
|       9 | 2117 | `		if( z[i] == '\\' && i + 1 < n && (z[i+1] == '\\' \|\| z[i+1] == '\'') ){` |
|       3 | 2118 | `			AssertRenderQuotedByte(pOut,z[i+1]);` |
|       3 | 2119 | `			i += 2;` |
|       2 | 2120 | `		}else{` |
|       7 | 2121 | `			AssertRenderQuotedByte(pOut,z[i]);` |
|       7 | 2122 | `			i++;` |
|       - | 2123 | `		}` |
|       1 | 2124 | `	}` |
|       3 | 2125 | `	SyBlobAppend(pOut,"'",1);` |
|       3 | 2126 | `}` |
|       - | 2127 | `/*` |
|       - | 2128 | ` * Render a double-quoted-source string (or heredoc body) with php's escape` |
|       - | 2129 | ` * processing — the value bytes are what php's AST holds, and the export prints` |
|       - | 2130 | ` * them single-quoted. An UNKNOWN escape keeps the backslash and the character,` |
|       - | 2131 | ` * matching php's string semantics.` |
|       - | 2132 | ` */` |
|      12 | 2133 | `static void AssertRenderDblString(SyBlob *pOut,const char *z,sxu32 n)` |
|       3 | 2134 | `{` |
|      15 | 2135 | `	sxu32 i = 0;` |
|      15 | 2136 | `	SyBlobAppend(pOut,"'",1);` |
|      49 | 2137 | `	while( i < n ){` |
|      36 | 2138 | `		int c = z[i];` |
|       - | 2139 | `		int d;` |
|      36 | 2140 | `		if( c != '\\' \|\| i + 1 >= n ){` |
|      36 | 2141 | `			AssertRenderQuotedByte(pOut,c);` |
|      36 | 2142 | `			i++;` |
|      36 | 2143 | `			continue;` |
|       - | 2144 | `		}` |
|     ! 0 | 2145 | `		d = z[i+1];` |
|     ! 0 | 2146 | `		i += 2;` |
|     ! 0 | 2147 | `		switch(d){` |
|     ! 0 | 2148 | `		case 'n': AssertRenderQuotedByte(pOut,'\n'); break;` |
|     ! 0 | 2149 | `		case 't': AssertRenderQuotedByte(pOut,'\t'); break;` |
|     ! 0 | 2150 | `		case 'r': AssertRenderQuotedByte(pOut,'\r'); break;` |
|     ! 0 | 2151 | `		case 'v': AssertRenderQuotedByte(pOut,'\v'); break;` |
|     ! 0 | 2152 | `		case 'f': AssertRenderQuotedByte(pOut,'\f'); break;` |
|     ! 0 | 2153 | `		case 'e': AssertRenderQuotedByte(pOut,0x1b); break;` |
|     ! 0 | 2154 | `		case '\\': AssertRenderQuotedByte(pOut,'\\'); break;` |
|     ! 0 | 2155 | `		case '"': AssertRenderQuotedByte(pOut,'"'); break;` |
|     ! 0 | 2156 | `		case '$': AssertRenderQuotedByte(pOut,'$'); break;` |
|     ! 0 | 2157 | `		case 'x': case 'X': {` |
|       - | 2158 | `			/* Up to two hex digits; a bare \x is literal. */` |
|     ! 0 | 2159 | `			int nHex = 0, v = 0;` |
|     ! 0 | 2160 | `			while( nHex < 2 && i < n && (unsigned char)z[i] < 0x80 && SyisHex((unsigned char)z[i]) ){` |
|     ! 0 | 2161 | `				v = (v << 4) \| SyHexToint((unsigned char)z[i]);` |
|     ! 0 | 2162 | `				i++; nHex++;` |
|     ! 0 | 2163 | `			}` |
|     ! 0 | 2164 | `			if( nHex > 0 ){` |
|     ! 0 | 2165 | `				AssertRenderQuotedByte(pOut,v);` |
|     ! 0 | 2166 | `			}else{` |
|     ! 0 | 2167 | `				AssertRenderQuotedByte(pOut,'\\');` |
|     ! 0 | 2168 | `				AssertRenderQuotedByte(pOut,d);` |
|       - | 2169 | `			}` |
|     ! 0 | 2170 | `			break;` |
|       - | 2171 | `		}` |
|     ! 0 | 2172 | `		case 'u': {` |
|       - | 2173 | `			/* \u{HEX+} — anything else keeps the backslash (php). */` |
|     ! 0 | 2174 | `			if( i < n && z[i] == '{' ){` |
|     ! 0 | 2175 | `				sxu32 v = 0; sxu32 j = i + 1; int nHex = 0;` |
|     ! 0 | 2176 | `				while( j < n && (unsigned char)z[j] < 0x80 && SyisHex((unsigned char)z[j]) && nHex < 8 ){` |
|     ! 0 | 2177 | `					v = (v << 4) \| (sxu32)SyHexToint((unsigned char)z[j]);` |
|     ! 0 | 2178 | `					j++; nHex++;` |
|     ! 0 | 2179 | `				}` |
|     ! 0 | 2180 | `				if( nHex > 0 && j < n && z[j] == '}' ){` |
|     ! 0 | 2181 | `					AssertRenderUtf8(pOut,v);` |
|     ! 0 | 2182 | `					i = j + 1;` |
|     ! 0 | 2183 | `					break;` |
|       - | 2184 | `				}` |
|     ! 0 | 2185 | `			}` |
|     ! 0 | 2186 | `			AssertRenderQuotedByte(pOut,'\\');` |
|     ! 0 | 2187 | `			AssertRenderQuotedByte(pOut,d);` |
|     ! 0 | 2188 | `			break;` |
|       - | 2189 | `		}` |
|     ! 0 | 2190 | `		default:` |
|     ! 0 | 2191 | `			if( d >= '0' && d <= '7' ){` |
|       - | 2192 | `				/* Up to three octal digits (the first was d). */` |
|     ! 0 | 2193 | `				int nOct = 1, v = d - '0';` |
|     ! 0 | 2194 | `				while( nOct < 3 && i < n && z[i] >= '0' && z[i] <= '7' ){` |
|     ! 0 | 2195 | `					v = (v << 3) \| (z[i] - '0');` |
|     ! 0 | 2196 | `					i++; nOct++;` |
|     ! 0 | 2197 | `				}` |
|     ! 0 | 2198 | `				AssertRenderQuotedByte(pOut,v & 0xff);` |
|     ! 0 | 2199 | `			}else{` |
|     ! 0 | 2200 | `				AssertRenderQuotedByte(pOut,'\\');` |
|     ! 0 | 2201 | `				AssertRenderQuotedByte(pOut,d);` |
|       - | 2202 | `			}` |
|     ! 0 | 2203 | `			break;` |
|       - | 2204 | `		}` |
|     ! 0 | 2205 | `	}` |
|      15 | 2206 | `	SyBlobAppend(pOut,"'",1);` |
|      15 | 2207 | `}` |
|       - | 2208 | `/*` |
|       - | 2209 | ` * Append a double in php's AST-export shape: the shortest round-tripping` |
|       - | 2210 | ` * decimal, with a forced ".0" fraction when the digits alone look integral` |
|       - | 2211 | ` * (1e3 -> "1000.0", 1e20 -> "1.0E+20") — the var_export float shape.` |
|       - | 2212 | ` */` |
|       8 | 2213 | `static void AssertRenderReal(SyBlob *pOut,ph7_real rVal)` |
|       1 | 2214 | `{` |
|       - | 2215 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|       - | 2216 | `	/* No floating point: ph7_real IS sxi64, there is no shortest-round-trip` |
|       - | 2217 | `	 * decimal to search for and no ".0" to force, so the value renders as the` |
|       - | 2218 | `	 * integer it is -- the same shape the INTEGER arm below emits. Taking` |
|       - | 2219 | `	 * ph7_real rather than double is what keeps the two call sites from` |
|       - | 2220 | `	 * narrowing (MSVC /W4 makes that C4244, and /WX makes it an error). */` |
|       - | 2221 | `	SyBlobFormat(pOut,"%qd",(sxi64)rVal);` |
|       - | 2222 | `#else` |
|       9 | 2223 | `	sxu32 nBefore = SyBlobLength(pOut);` |
|       - | 2224 | `	const char *zOut;` |
|       - | 2225 | `	sxu32 i, nAfter;` |
|       9 | 2226 | `	int bPlain = 1;` |
|       9 | 2227 | `	PH7_AppendShortestReal(pOut,rVal);` |
|       9 | 2228 | `	zOut = (const char *)SyBlobData(pOut);` |
|       9 | 2229 | `	nAfter = SyBlobLength(pOut);` |
|      23 | 2230 | `	for( i = nBefore; i < nAfter; i++ ){` |
|      21 | 2231 | `		if( !((zOut[i] >= '0' && zOut[i] <= '9') \|\| zOut[i] == '-') ){` |
|       7 | 2232 | `			bPlain = 0;` |
|       7 | 2233 | `			break;` |
|       - | 2234 | `		}` |
|       8 | 2235 | `	}` |
|       9 | 2236 | `	if( bPlain ){` |
|       3 | 2237 | `		SyBlobAppend(pOut,".0",2);` |
|       1 | 2238 | `	}` |
|       - | 2239 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|       9 | 2240 | `}` |
|       - | 2241 | `/*` |
|       - | 2242 | ` * Render the token span [pIn, pEnd) — a direct assert() call's first argument —` |
|       - | 2243 | ` * into pOut in php's zend_ast_export shape (see the block comment above).` |
|       - | 2244 | ` * Total: every span renders to SOMETHING (unknown constructs fall back to` |
|       - | 2245 | ` * their raw token text), so the capture never aborts a compile.` |
|       - | 2246 | ` */` |
|      62 | 2247 | `PH7_PRIVATE void PH7_GenRenderAssertSpan(ph7_gen_state *pGen,SyToken *pIn,SyToken *pEnd,SyBlob *pOut)` |
|       5 | 2248 | `{` |
|       - | 2249 | `	sxu8 aParen[64]; /* 1 = this '(' depth is an array(...) literal rendered as [...] */` |
|      67 | 2250 | `	sxu32 nParen = 0;` |
|      67 | 2251 | `	int iPrev = ASRT_START;` |
|      67 | 2252 | ``	int bArrayOpen = 0; /* the next '(' belongs to a suppressed `array` keyword */`` |
|       - | 2253 | `	/* php drops every redundant paren when re-deriving source from the AST;` |
|       - | 2254 | `	 * dropping the OUTERMOST pair(s) is the token-level equivalent for the` |
|       - | 2255 | ``	 * common `assert((...))` spelling. */`` |
|      69 | 2256 | `	while( pIn < pEnd - 1 && (pIn->nType & PH7_TK_LPAREN) && (pEnd[-1].nType & PH7_TK_RPAREN) ){` |
|       - | 2257 | `		SyToken *p;` |
|       3 | 2258 | `		sxi32 iDepth = 0;` |
|       3 | 2259 | `		SyToken *pMatch = 0;` |
|      11 | 2260 | `		for( p = pIn; p < pEnd; p++ ){` |
|      11 | 2261 | `			if( p->nType & PH7_TK_LPAREN ){` |
|       3 | 2262 | `				iDepth++;` |
|      10 | 2263 | `			}else if( p->nType & PH7_TK_RPAREN ){` |
|       3 | 2264 | `				iDepth--;` |
|       3 | 2265 | `				if( iDepth == 0 ){ pMatch = p; break; }` |
|     ! 0 | 2266 | `			}` |
|       5 | 2267 | `		}` |
|       3 | 2268 | `		if( pMatch != &pEnd[-1] ){` |
|     ! 0 | 2269 | `			break;` |
|       - | 2270 | `		}` |
|       3 | 2271 | `		pIn++;` |
|       3 | 2272 | `		pEnd--;` |
|       1 | 2273 | `	}` |
|     267 | 2274 | `	for( ; pIn < pEnd ; pIn++ ){` |
|     205 | 2275 | `		SyToken *pTok = pIn;` |
|     205 | 2276 | `		const char *zTxt = pTok->sData.zString;` |
|     205 | 2277 | `		sxu32 nTxt = pTok->sData.nByte;` |
|       - | 2278 | `		int iCls;` |
|       - | 2279 | `		sxu32 nMark;` |
|       - | 2280 | `		/* --- classify + pre-token handling ------------------------------ */` |
|     205 | 2281 | `		if( pTok->nType & PH7_TK_LPAREN ){` |
|      15 | 2282 | `			iCls = ASRT_OPEN;` |
|     199 | 2283 | `		}else if( pTok->nType & PH7_TK_RPAREN ){` |
|      15 | 2284 | `			iCls = ASRT_CLOSE;` |
|     187 | 2285 | `		}else if( pTok->nType & (PH7_TK_OSB\|PH7_TK_CSB) ){` |
|      13 | 2286 | `			iCls = (pTok->nType & PH7_TK_OSB) ? ASRT_OPEN : ASRT_CLOSE;` |
|     175 | 2287 | `		}else if( pTok->nType & PH7_TK_COMMA ){` |
|       - | 2288 | `			/* php's export never prints a trailing comma. */` |
|       7 | 2289 | `			if( &pIn[1] < pEnd && (pIn[1].nType & (PH7_TK_RPAREN\|PH7_TK_CSB)) ){` |
|     ! 0 | 2290 | `				continue;` |
|       - | 2291 | `			}` |
|       7 | 2292 | `			iCls = ASRT_COMMA;` |
|     166 | 2293 | `		}else if( pTok->nType & PH7_TK_DOLLAR ){` |
|       3 | 2294 | `			iCls = ASRT_UNARY; /* operand-like before, glued to its name after */` |
|     162 | 2295 | `		}else if( pTok->nType & PH7_TK_NSSEP ){` |
|     ! 0 | 2296 | `			iCls = ASRT_GLUE;` |
|     161 | 2297 | `		}else if( pTok->nType & PH7_TK_ELLIPSIS ){` |
|     ! 0 | 2298 | `			iCls = ASRT_UNARY;` |
|     161 | 2299 | `		}else if( pTok->nType & PH7_TK_OP ){` |
|      46 | 2300 | `			iCls = ASRT_BINOP;` |
|      46 | 2301 | `			if( nTxt > 0 ){` |
|      46 | 2302 | `				int c0 = zTxt[0];` |
|      46 | 2303 | `				if( c0 == '(' ){` |
|     ! 0 | 2304 | ``					iCls = ASRT_UNARY; /* lexer-merged cast token `(int)` */`` |
|      60 | 2305 | `				}else if( nTxt == 2 && (SyMemcmp(zTxt,"->",2) == 0 \|\| SyMemcmp(zTxt,"::",2) == 0` |
|      28 | 2306 | `						\|\| SyMemcmp(zTxt,"++",2) == 0 \|\| SyMemcmp(zTxt,"--",2) == 0) ){` |
|     ! 0 | 2307 | `					iCls = ASRT_GLUE;` |
|      46 | 2308 | `				}else if( nTxt == 3 && SyMemcmp(zTxt,"?->",3) == 0 ){` |
|     ! 0 | 2309 | `					iCls = ASRT_GLUE;` |
|      46 | 2310 | `				}else if( nTxt == 1 && (c0 == '!' \|\| c0 == '~' \|\| c0 == '@') ){` |
|       3 | 2311 | `					iCls = ASRT_UNARY;` |
|      45 | 2312 | `				}else if( nTxt == 1 && (c0 == '-' \|\| c0 == '+' \|\| c0 == '&') ){` |
|       - | 2313 | `					/* Unary when nothing operand-like precedes. */` |
|       4 | 2314 | `					if( iPrev == ASRT_START \|\| iPrev == ASRT_BINOP \|\| iPrev == ASRT_UNARY` |
|       3 | 2315 | `					 \|\| iPrev == ASRT_OPEN \|\| iPrev == ASRT_COMMA ){` |
|       3 | 2316 | `						iCls = ASRT_UNARY;` |
|       2 | 2317 | `					}` |
|      42 | 2318 | `				}else if( pTok->nType & PH7_TK_ID ){` |
|       - | 2319 | `					/* Alpha operators: and/or normalize to php's export spelling;` |
|       - | 2320 | `					 * new/clone read as prefix keywords (operand spacing). */` |
|       3 | 2321 | `					if( nTxt == 3 && SyStrnicmp(zTxt,"and",3) == 0 ){` |
|       3 | 2322 | `						zTxt = "&&"; nTxt = 2;` |
|       1 | 2323 | `					}else if( nTxt == 2 && SyStrnicmp(zTxt,"or",2) == 0 ){` |
|     ! 0 | 2324 | `						zTxt = "\|\|"; nTxt = 2;` |
|     ! 0 | 2325 | `					}else if( (nTxt == 3 && SyStrnicmp(zTxt,"new",3) == 0)` |
|     ! 0 | 2326 | `						\|\| (nTxt == 5 && SyStrnicmp(zTxt,"clone",5) == 0) ){` |
|     ! 0 | 2327 | `						iCls = ASRT_OPERAND;` |
|     ! 0 | 2328 | `					}` |
|       1 | 2329 | `				}` |
|      24 | 2330 | `			}` |
|     139 | 2331 | `		}else if( pTok->nType & (PH7_TK_EQUAL\|PH7_TK_ARRAY_OP\|PH7_TK_COLON\|PH7_TK_AMPER) ){` |
|     ! 0 | 2332 | `			iCls = ASRT_BINOP;` |
|     117 | 2333 | `		}else if( pTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|      35 | 2334 | `			iCls = ASRT_OPERAND;` |
|       - | 2335 | ``			/* `array` `(` — php's AST holds one list node for both spellings and`` |
|       - | 2336 | ``			 * always exports `[...]`. Suppress the keyword (it lexes as a KEYWORD`` |
|       - | 2337 | `			 * token, not an ID); the '(' renders '['. */` |
|      30 | 2338 | `			if( nTxt == 5 && SyStrnicmp(zTxt,"array",5) == 0` |
|      17 | 2339 | `			 && &pIn[1] < pEnd && (pIn[1].nType & PH7_TK_LPAREN) ){` |
|       9 | 2340 | `				bArrayOpen = 1;` |
|       9 | 2341 | `				continue;` |
|       - | 2342 | `			}` |
|      16 | 2343 | `		}else{` |
|       - | 2344 | `			/* keywords (true/false/null/fn/match/...), numbers, strings,` |
|       - | 2345 | `			 * member names, '{'/'}' and anything unforeseen */` |
|      86 | 2346 | `			iCls = ASRT_OPERAND;` |
|       - | 2347 | `		}` |
|       - | 2348 | ``		/* Elvis `? :` — php exports the two-token form as `?:`. */`` |
|     194 | 2349 | `		if( iCls == ASRT_BINOP && nTxt == 1 && zTxt[0] == '?'` |
|      11 | 2350 | `		 && &pIn[1] < pEnd && (pIn[1].nType & PH7_TK_COLON) ){` |
|       3 | 2351 | `			nMark = SyBlobLength(pOut);` |
|       3 | 2352 | `			if( iPrev != ASRT_START && nMark > 0 ){` |
|       3 | 2353 | `				SyBlobAppend(pOut," ",1);` |
|       1 | 2354 | `			}` |
|       3 | 2355 | `			SyBlobAppend(pOut,"?:",2);` |
|       3 | 2356 | `			pIn++; /* consume the ':' */` |
|       3 | 2357 | `			iPrev = ASRT_BINOP;` |
|       3 | 2358 | `			continue;` |
|       - | 2359 | `		}` |
|       - | 2360 | `		/* --- spacing ---------------------------------------------------- */` |
|     197 | 2361 | `		if( iPrev != ASRT_START ){` |
|     134 | 2362 | `			int iAfter = aAsrtAfter[iPrev];` |
|     134 | 2363 | `			int iBefore = aAsrtBefore[iCls];` |
|     130 | 2364 | `			if( iAfter == ASRT_SP_FORCE \|\| iBefore == ASRT_SP_FORCE` |
|      69 | 2365 | `			 \|\| (iAfter == ASRT_SP_WANT && iBefore == ASRT_SP_WANT) ){` |
|      86 | 2366 | `				SyBlobAppend(pOut," ",1);` |
|      42 | 2367 | `			}` |
|      65 | 2368 | `		}` |
|       - | 2369 | `		/* --- emit ------------------------------------------------------- */` |
|     197 | 2370 | `		if( pTok->nType & PH7_TK_LPAREN ){` |
|      15 | 2371 | `			if( nParen < sizeof(aParen) ){` |
|      15 | 2372 | `				aParen[nParen] = (sxu8)bArrayOpen;` |
|       6 | 2373 | `			}` |
|      15 | 2374 | `			nParen++;` |
|      15 | 2375 | `			SyBlobAppend(pOut,bArrayOpen ? "[" : "(",1);` |
|      15 | 2376 | `			bArrayOpen = 0;` |
|     191 | 2377 | `		}else if( pTok->nType & PH7_TK_RPAREN ){` |
|      15 | 2378 | `			int bArr = 0;` |
|      15 | 2379 | `			if( nParen > 0 ){` |
|      15 | 2380 | `				nParen--;` |
|      15 | 2381 | `				if( nParen < sizeof(aParen) ){` |
|      15 | 2382 | `					bArr = aParen[nParen];` |
|       6 | 2383 | `				}` |
|       6 | 2384 | `			}` |
|      15 | 2385 | `			SyBlobAppend(pOut,bArr ? "]" : ")",1);` |
|     178 | 2386 | `		}else if( pTok->nType & (PH7_TK_INTEGER\|PH7_TK_REAL) ){` |
|       - | 2387 | `			char zScratch[GEN_NUM_SCRATCH];` |
|      71 | 2388 | `			char *zAlloc = 0;` |
|       - | 2389 | `			SyString sNum;` |
|     102 | 2390 | `			if( GenStateStripNumericSeparators(&pGen->pVm->sAllocator,&pTok->sData,` |
|      71 | 2391 | `					zScratch,sizeof(zScratch),&sNum,&zAlloc) != SXRET_OK ){` |
|     ! 0 | 2392 | `				SyBlobAppend(pOut,zTxt,nTxt); /* alloc failure: raw text */` |
|      71 | 2393 | `			}else if( pTok->nType & PH7_TK_INTEGER ){` |
|      63 | 2394 | `				ph7_real rOverflow = 0;` |
|      63 | 2395 | `				int bDecimalOverflow = 0;` |
|      63 | 2396 | `				if( GenStateIntLiteralOverflows(&sNum,&rOverflow,&bDecimalOverflow) ){` |
|     ! 0 | 2397 | `					if( bDecimalOverflow ){` |
|     ! 0 | 2398 | `						SyStrToReal(sNum.zString,sNum.nByte,(void *)&rOverflow,0);` |
|     ! 0 | 2399 | `					}` |
|     ! 0 | 2400 | `					AssertRenderReal(pOut,rOverflow);` |
|     ! 0 | 2401 | `				}else{` |
|      63 | 2402 | `					SyBlobFormat(pOut,"%qd",PH7_TokenValueToInt64(&sNum));` |
|       - | 2403 | `				}` |
|      33 | 2404 | `			}else{` |
|       9 | 2405 | `				ph7_real rVal = 0;` |
|       9 | 2406 | `				SyStrToReal(sNum.zString,sNum.nByte,(void *)&rVal,0);` |
|       9 | 2407 | `				AssertRenderReal(pOut,rVal);` |
|       - | 2408 | `			}` |
|      71 | 2409 | `			if( zAlloc ){` |
|     ! 0 | 2410 | `				SyMemBackendFree(&pGen->pVm->sAllocator,zAlloc);` |
|       3 | 2411 | `			}` |
|     138 | 2412 | `		}else if( pTok->nType & (PH7_TK_SSTR\|PH7_TK_NOWDOC) ){` |
|       3 | 2413 | `			AssertRenderSglString(pOut,zTxt,nTxt);` |
|     103 | 2414 | `		}else if( pTok->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
|      15 | 2415 | `			if( SyByteFind(zTxt,nTxt,'$',0) == SXRET_OK ){` |
|       - | 2416 | `				/* Interpolated: php exports its interpolation AST in a` |
|       - | 2417 | `				 * double-quoted form; the raw source is the token-level` |
|       - | 2418 | `				 * equivalent. */` |
|     ! 0 | 2419 | `				SyBlobAppend(pOut,"\"",1);` |
|     ! 0 | 2420 | `				SyBlobAppend(pOut,zTxt,nTxt);` |
|     ! 0 | 2421 | `				SyBlobAppend(pOut,"\"",1);` |
|     ! 0 | 2422 | `			}else{` |
|      15 | 2423 | `				AssertRenderDblString(pOut,zTxt,nTxt);` |
|       - | 2424 | `			}` |
|       9 | 2425 | `		}else{` |
|      90 | 2426 | `			SyBlobAppend(pOut,zTxt,nTxt);` |
|       - | 2427 | `		}` |
|     197 | 2428 | `		iPrev = iCls;` |
|     101 | 2429 | `	}` |
|      67 | 2430 | `}` |
|       - | 2431 |  |
