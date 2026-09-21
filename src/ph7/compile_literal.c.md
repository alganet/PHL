# src/ph7/compile_literal.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1135/1344 lines (84.45%)

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
|     1092 |   23 | `static int GenStateIsBaseDigit(int c, int base)` |
|        3 |   24 | `{` |
|     1095 |   25 | `	if( base == 16 ){ return SyisHex(c); }` |
|      995 |   26 | `	if( base == 2 ){ return c == '0' \|\| c == '1'; }` |
|      715 |   27 | `	return SyisDigit(c);` |
|      549 |   28 | `}` |
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
|  4119200 |   45 | `static int GenStateFindBadNumericSeparator(` |
|        - |   46 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|        5 |   47 | `{` |
|  4119205 |   48 | `	const char *z = pRaw->zString;` |
|  4119205 |   49 | `	sxu32 n = pRaw->nByte;` |
|  4119205 |   50 | `	int base = 10;` |
|        - |   51 | `	sxu32 i, start;` |
|  4119205 |   52 | `	if( n < 2 ) return 0;` |
|   859449 |   53 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|   111883 |   54 | `		base = 16;` |
|   803510 |   55 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|      287 |   56 | `		base = 2;` |
|      143 |   57 | `	}` |
|  3272597 |   58 | `	for( i = 0; i < n; ++i ){` |
|  2413163 |   59 | `		if( z[i] != '_' ) continue;` |
|      552 |   60 | `		if( i > 0 && i + 1 < n` |
|      549 |   61 | `			&& GenStateIsBaseDigit((unsigned char)z[i-1], base)` |
|      551 |   62 | `			&& GenStateIsBaseDigit((unsigned char)z[i+1], base) ){` |
|      544 |   63 | `			continue; /* well-placed separator */` |
|        - |   64 | `		}` |
|        - |   65 | `		/* First misplaced underscore — the lexer already absorbed the full` |
|        - |   66 | `		 * malformed tail, so it runs from here to the end of the token. */` |
|       14 |   67 | `		start = i;` |
|       19 |   68 | `		if( start > 0 && (z[start-1] == 'x' \|\| z[start-1] == 'X'` |
|       10 |   69 | `			\|\| z[start-1] == 'b' \|\| z[start-1] == 'B') ){` |
|      ! 0 |   70 | `			start--; /* include the base letter for 0x_... / 0b_... */` |
|      ! 0 |   71 | `		}` |
|       14 |   72 | `		*pBadStart = &z[start];` |
|       14 |   73 | `		*pBadLen = n - start;` |
|       14 |   74 | `		return 1;` |
|      ! 0 |   75 | `	}` |
|   859439 |   76 | `	return 0;` |
|  2059605 |   77 | `}` |
|        - |   78 | `/*` |
|        - |   79 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|        - |   80 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|        - |   81 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|        - |   82 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|        - |   83 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|        - |   84 | ` * so callers can bail from the current construct).` |
|        - |   85 | ` */` |
|  4119200 |   86 | `PH7_PRIVATE sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|        5 |   87 | `{` |
|  4119205 |   88 | `	const char *zBad = 0;` |
|  4119205 |   89 | `	sxu32 nBad = 0;` |
|        - |   90 | `	SyString sBad;` |
|        - |   91 | `	sxi32 rc;` |
|  4119205 |   92 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|  4119195 |   93 | `		return SXRET_OK;` |
|        - |   94 | `	}` |
|       14 |   95 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|       14 |   96 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|        - |   97 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|       14 |   98 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |   99 | `		return SXERR_ABORT;` |
|        - |  100 | `	}` |
|       14 |  101 | `	return SXERR_SYNTAX;` |
|  2059605 |  102 | `}` |
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
|  4119258 |  119 | `PH7_PRIVATE sxi32 GenStateStripNumericSeparators(` |
|        - |  120 | `	SyMemBackend *pAlloc,` |
|        - |  121 | `	const SyString *pToken,` |
|        - |  122 | `	char *zScratch, sxu32 nScratch,` |
|        - |  123 | `	SyString *pOut, char **pzAlloc)` |
|        5 |  124 | `{` |
|        - |  125 | `	sxu32 i, j;` |
|  4119263 |  126 | `	int hasUnderscore = 0;` |
|        - |  127 | `	char *zBuf;` |
|  4119263 |  128 | `	*pzAlloc = 0;` |
|  9790167 |  129 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|  5671173 |  130 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|  2835457 |  131 | `	}` |
|  4119263 |  132 | `	if( !hasUnderscore ){` |
|  4118999 |  133 | `		SyStringDupPtr(pOut, pToken);` |
|  4118999 |  134 | `		return SXRET_OK;` |
|        - |  135 | `	}` |
|      266 |  136 | `	if( pToken->nByte <= nScratch ){` |
|      264 |  137 | `		zBuf = zScratch;` |
|      133 |  138 | `	}else{` |
|        3 |  139 | `		zBuf = (char *)SyMemBackendAlloc(pAlloc, pToken->nByte);` |
|        3 |  140 | `		if( zBuf == 0 ){` |
|      ! 0 |  141 | `			return SXERR_ABORT;` |
|        - |  142 | `		}` |
|        3 |  143 | `		*pzAlloc = zBuf;` |
|        - |  144 | `	}` |
|      266 |  145 | `	j = 0;` |
|     2974 |  146 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|     2710 |  147 | `		if( pToken->zString[i] != '_' ){ zBuf[j++] = pToken->zString[i]; }` |
|     1356 |  148 | `	}` |
|      266 |  149 | `	SyStringInitFromBuf(pOut, zBuf, j);` |
|      266 |  150 | `	return SXRET_OK;` |
|  2059634 |  151 | `}` |
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
|  4109868 |  187 | `static int GenStateIntLiteralOverflows(const SyString *pNum, ph7_real *pReal, int *pbDecimal)` |
|        5 |  188 | `{` |
|  4109873 |  189 | `	const char *z = pNum->zString;` |
|  4109873 |  190 | `	const char *zEnd = z + pNum->nByte;` |
|        - |  191 | `	const char *p, *q;` |
|        - |  192 | `	int n;` |
|  4109873 |  193 | `	*pbDecimal = FALSE;` |
|  4109873 |  194 | `	if( z >= zEnd ){` |
|      ! 0 |  195 | `		return FALSE;` |
|        - |  196 | `	}` |
|  4109873 |  197 | `	if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|        - |  198 | `		/* Hexadecimal: INT64_MAX == 0x7FFF...F (16 digits, leading nibble 7). */` |
|   111885 |  199 | `		p = z + 2;` |
|   140873 |  200 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|   455997 |  201 | `		for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisHex(q[0]); q++ ){ n++; }` |
|   111885 |  202 | `		if( n < 16 \|\| (n == 16 && SyHexToint(p[0]) < 8) ){` |
|   111879 |  203 | `			return FALSE;` |
|        - |  204 | `		}` |
|        7 |  205 | `		{ ph7_real dv = 0;` |
|      103 |  206 | `		  for( q = p; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisHex(q[0]); q++ ){` |
|       97 |  207 | `			dv = dv * 16 + (ph7_real)SyHexToint(q[0]);` |
|       49 |  208 | `		  }` |
|        7 |  209 | `		  *pReal = dv;` |
|        - |  210 | `		}` |
|        7 |  211 | `		return TRUE;` |
|  3997993 |  212 | `	}else if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|        - |  213 | `		/* Binary: INT64_MAX needs 63 significant bits. */` |
|      287 |  214 | `		p = z + 2;` |
|      335 |  215 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|     2172 |  216 | `		for( q = p, n = 0; q < zEnd && (q[0] == '0' \|\| q[0] == '1'); q++ ){ n++; }` |
|      287 |  217 | `		if( n <= 63 ){` |
|      285 |  218 | `			return FALSE;` |
|        - |  219 | `		}` |
|        3 |  220 | `		{ ph7_real dv = 0;` |
|      195 |  221 | `		  for( q = p; q < zEnd && (q[0] == '0' \|\| q[0] == '1'); q++ ){` |
|      129 |  222 | `			dv = dv * 2 + (ph7_real)(q[0] - '0');` |
|       65 |  223 | `		  }` |
|        3 |  224 | `		  *pReal = dv;` |
|        - |  225 | `		}` |
|        3 |  226 | `		return TRUE;` |
|  3997707 |  227 | `	}else if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'o' \|\| z[1] == 'O') ){` |
|        - |  228 | `		/* PHP 8.1 explicit octal 0o/0O: 21 significant octal digits fit in int64. */` |
|       21 |  229 | `		p = z + 2;` |
|       25 |  230 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|       97 |  231 | `		for( q = p, n = 0; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){ n++; }` |
|       21 |  232 | `		if( n <= 21 ){` |
|       21 |  233 | `			return FALSE;` |
|        - |  234 | `		}` |
|      ! 0 |  235 | `		{ ph7_real dv = 0;` |
|      ! 0 |  236 | `		  for( q = p; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){` |
|      ! 0 |  237 | `			dv = dv * 8 + (ph7_real)(q[0] - '0');` |
|      ! 0 |  238 | `		  }` |
|      ! 0 |  239 | `		  *pReal = dv;` |
|        - |  240 | `		}` |
|      ! 0 |  241 | `		return TRUE;` |
|  3997687 |  242 | `	}else if( z[0] == '0' ){` |
|        - |  243 | `		/* Octal: INT64_MAX == 0o777...7 (21 significant octal digits). Skip the` |
|        - |  244 | `		 * leading zeros (incl. the base '0'); a non-octal char such as the 8.1` |
|        - |  245 | `		 * "0o" marker ends the run and leaves it to the int path (as today). */` |
|  1501369 |  246 | `		p = z;` |
|  3002739 |  247 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|  1514051 |  248 | `		for( q = p, n = 0; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){ n++; }` |
|  1501369 |  249 | `		if( n <= 21 ){` |
|  1501367 |  250 | `			return FALSE;` |
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
|  2496323 |  263 | `	p = z;` |
|  2496323 |  264 | `	while( p < zEnd && p[0] == '0' ){ p++; }` |
|  6024995 |  265 | `	for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisDigit(q[0]); q++ ){ n++; }` |
|  2496323 |  266 | `	if( n > 19 \|\| (n == 19 && SyMemcmp(p, "9223372036854775807", 19) > 0) ){` |
|       25 |  267 | `		*pbDecimal = TRUE;` |
|       25 |  268 | `		return TRUE;` |
|        - |  269 | `	}` |
|  2496299 |  270 | `	return FALSE;` |
|  2054939 |  271 | `}` |
|  4119172 |  272 | `PH7_PRIVATE sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  273 | `{` |
|  4119177 |  274 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|  4119177 |  275 | `	sxu32 nIdx = 0;` |
|        - |  276 | `	char zScratch[GEN_NUM_SCRATCH];` |
|  4119177 |  277 | `	char *zAlloc = 0;` |
|        - |  278 | `	SyString sNum;` |
|        - |  279 | `	sxi32 rc;` |
|  2059586 |  280 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|  4119177 |  281 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|  4119177 |  282 | `	if( rc != SXRET_OK ){` |
|        9 |  283 | `		return rc;` |
|        - |  284 | `	}` |
|  6178754 |  285 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|  2059583 |  286 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|  4119171 |  287 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  288 | `		return SXERR_ABORT;` |
|        - |  289 | `	}` |
|  4119171 |  290 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|        - |  291 | `		ph7_value *pObj;` |
|        - |  292 | `		sxi64 iValue;` |
|  4109813 |  293 | `		ph7_real rOverflow = 0;` |
|  4109813 |  294 | `		int bDecimalOverflow = 0;` |
|  4109813 |  295 | `		if( GenStateIntLiteralOverflows(&sNum,&rOverflow,&bDecimalOverflow) ){` |
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
|  4109779 |  312 | `			iValue = PH7_TokenValueToInt64(&sNum);` |
|  4109779 |  313 | `			pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|  4109779 |  314 | `			if( pObj == 0 ){` |
|      ! 0 |  315 | `				if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|      ! 0 |  316 | `				return SXERR_ABORT;` |
|        - |  317 | `			}` |
|  4109779 |  318 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|        - |  319 | `		}` |
|  2054909 |  320 | `	}else{` |
|        - |  321 | `		/* Real number */` |
|        - |  322 | `		ph7_value *pObj;` |
|        - |  323 | `		/* Reserve a new constant */` |
|     9363 |  324 | `		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     9363 |  325 | `		if( pObj == 0 ){` |
|      ! 0 |  326 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |  327 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|      ! 0 |  328 | `			return SXERR_ABORT;` |
|        - |  329 | `		}` |
|     9363 |  330 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|     9363 |  331 | `		PH7_MemObjToReal(pObj);` |
|        - |  332 | `	}` |
|  4119171 |  333 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|        - |  334 | `	/* Emit the load constant instruction */` |
|  4119171 |  335 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        - |  336 | `	/* Node successfully compiled */` |
|  4119171 |  337 | `	return SXRET_OK;` |
|  2059591 |  338 | `}` |
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
|  5988678 |  350 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  351 | `{` |
|  5988683 |  352 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|        - |  353 | `	const char *zIn,*zCur,*zEnd;` |
|        - |  354 | `	ph7_value *pObj;` |
|        - |  355 | `	sxu32 nIdx;` |
|        - |  356 | `	sxi32 bHasEsc;` |
|  5988683 |  357 | `	nIdx = 0; /* Prevent compiler warning */` |
|        - |  358 | `	/* Delimit the string */` |
|  5988683 |  359 | `	zIn  = pStr->zString;` |
|  5988683 |  360 | `	zEnd = &zIn[pStr->nByte];` |
|  5988683 |  361 | `	if( zIn >= zEnd ){` |
|        - |  362 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|        - |  363 | `		 * rather than reserving a new object each time. */` |
|   443277 |  364 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|   443277 |  365 | `		return SXRET_OK;` |
|        - |  366 | `	}` |
|        - |  367 | `	/* A single-quoted literal whose raw source holds a backslash unescapes to a` |
|        - |  368 | `	 * value that differs from that source (\\ -> \, \' -> '). The literal cache` |
|        - |  369 | `	 * keys FIND on the raw source text but INSTALL on the unescaped value, so` |
|        - |  370 | `	 * caching such a literal lets an unrelated one collide with it — e.g. '\\'` |
|        - |  371 | `	 * (raw \\, value \) would find the entry installed for '\\\\' (raw \\\\,` |
|        - |  372 | `	 * value \\) and load two backslashes. Only cache literals whose value equals` |
|        - |  373 | `	 * their source, i.e. those with no backslash to unescape. */` |
|  5545411 |  374 | `	bHasEsc = 0;` |
|        - |  375 | `	{` |
|        - |  376 | `		const char *zScan;` |
| 66846635 |  377 | `		for( zScan = zIn ; zScan < zEnd ; zScan++ ){` |
| 61388423 |  378 | `			if( zScan[0] == '\\' ){ bHasEsc = 1; break; }` |
| 30650617 |  379 | `		}` |
|        - |  380 | `	}` |
|  5545411 |  381 | `	if( !bHasEsc && SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|        - |  382 | `		/* Already processed,emit the load constant instruction` |
|        - |  383 | `		 * and return.` |
|        - |  384 | `		 */` |
|  3226095 |  385 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|  3226095 |  386 | `		return SXRET_OK;` |
|        - |  387 | `	}` |
|        - |  388 | `	/* Reserve a new constant */` |
|  2319321 |  389 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  2319321 |  390 | `	if( pObj == 0 ){` |
|      ! 0 |  391 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |  392 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|      ! 0 |  393 | `		return SXERR_ABORT;` |
|        - |  394 | `	}` |
|  2319321 |  395 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|        - |  396 | `	/* Compile the node */` |
|  2375348 |  397 | `	for(;;){` |
|  4750701 |  398 | `		if( zIn >= zEnd ){` |
|        - |  399 | `			/* End of input */` |
|  2319321 |  400 | `			break;` |
|        - |  401 | `		}` |
|  2431385 |  402 | `		zCur = zIn;` |
| 47394215 |  403 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
| 44962835 |  404 | `			zIn++;` |
|        5 |  405 | `		}` |
|  2431385 |  406 | `		if( zIn > zCur ){` |
|        - |  407 | `			/* Append raw contents*/` |
|  2385709 |  408 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|  1192852 |  409 | `		}` |
|  2431385 |  410 | `		zIn++;` |
|  2431385 |  411 | `		if( zIn < zEnd ){` |
|   153525 |  412 | `			if( zIn[0] == '\\' ){` |
|        - |  413 | `				/* A literal backslash */` |
|    37395 |  414 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|   134830 |  415 | `			}else if( zIn[0] == '\'' ){` |
|        - |  416 | `				/* A single quote */` |
|       18 |  417 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|       10 |  418 | `			}else{` |
|        - |  419 | `				/* verbatim copy */` |
|   116119 |  420 | `				zIn--;` |
|   116119 |  421 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char)*2);` |
|   116119 |  422 | `				zIn++;` |
|        - |  423 | `			}` |
|    76760 |  424 | `		}` |
|        - |  425 | `		/* Advance the stream cursor */` |
|  2431385 |  426 | `		zIn++;` |
|        5 |  427 | `	}` |
|        - |  428 | `	/* Emit the load constant instruction */` |
|  2319321 |  429 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|  2319321 |  430 | `	if( !bHasEsc && pStr->nByte < 1024 ){` |
|        - |  431 | `		/* Install in the literal table (only when value == source; see above) */` |
|  2232127 |  432 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|  1116061 |  433 | `	}` |
|        - |  434 | `	/* Node successfully compiled */` |
|  2319321 |  435 | `	return SXRET_OK;` |
|  2994344 |  436 | `}` |
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
|       79 |  464 | `		*pOut = *pIn;` |
|       79 |  465 | `		return SXRET_OK;` |
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
|       50 |  559 | `	pStr = &sStripped;` |
|       50 |  560 | `	nIdx = 0; /* Prevent compiler warning */` |
|       50 |  561 | `	if( pStr->nByte <= 0 ){` |
|        - |  562 | `		/* An empty nowdoc is the empty STRING, like '' -- loading NULL here made` |
|        - |  563 | `		 * strlen(<<<'EOD'EOD;) deprecation-warn about a null argument. */` |
|        7 |  564 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|        7 |  565 | `		return SXRET_OK;` |
|        - |  566 | `	}` |
|        - |  567 | `	/* Reserve a new constant */` |
|       44 |  568 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       44 |  569 | `	if( pObj == 0 ){` |
|      ! 0 |  570 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|      ! 0 |  571 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|      ! 0 |  572 | `		return SXERR_ABORT;` |
|        - |  573 | `	}` |
|        - |  574 | `	/* No processing is done here, simply a memcpy() operation */` |
|       44 |  575 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,pStr);` |
|        - |  576 | `	/* Emit the load constant instruction */` |
|       44 |  577 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        - |  578 | `	/* Node successfully compiled */` |
|       44 |  579 | `	return SXRET_OK;` |
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
|     2456 |  603 | `static sxi32 GenStateProcessStringExpression(` |
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
|     2461 |  614 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|        - |  615 | `	/* Preallocate some slots */` |
|     2461 |  616 | `	SySetAlloc(&sToken,0x08);` |
|        - |  617 | `	/* Tokenize the text */` |
|     2461 |  618 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken,0);` |
|        - |  619 | `	/* Swap delimiter */` |
|     2461 |  620 | `	pTmpIn  = pGen->pIn;` |
|     2461 |  621 | `	pTmpEnd = pGen->pEnd;` |
|     2461 |  622 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|     2461 |  623 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|        - |  624 | ``	/* Compile the expression. An interpolated `"...$x..."` READS $x — php warns`` |
|        - |  625 | `	 * "Undefined variable $x" and substitutes the empty string — so ask for a` |
|        - |  626 | `	 * read-only load rather than letting the default vivify it silently. */` |
|     2461 |  627 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|        - |  628 | `	/* Restore token stream */` |
|     2461 |  629 | `	pGen->pIn  = pTmpIn;` |
|     2461 |  630 | `	pGen->pEnd = pTmpEnd;` |
|        - |  631 | `	/* Release the token set */` |
|     2461 |  632 | `	SySetRelease(&sToken);` |
|        - |  633 | `	/* Compilation result */` |
|     2461 |  634 | `	return rc;` |
|        5 |  635 | `}` |
|        - |  636 | `/*` |
|        - |  637 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|        - |  638 | ` */` |
|   135564 |  639 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|        5 |  640 | `{` |
|        - |  641 | `	ph7_value *pConstObj;` |
|   135569 |  642 | `	sxu32 nIdx = 0;` |
|        - |  643 | `	/* Reserve a new constant */` |
|   135569 |  644 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   135569 |  645 | `	if( pConstObj == 0 ){` |
|      ! 0 |  646 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|      ! 0 |  647 | `		return 0;` |
|        - |  648 | `	}` |
|   135569 |  649 | `	(*pCount)++;` |
|   135569 |  650 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|        - |  651 | `	/* Emit the load constant instruction */` |
|   135569 |  652 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   135569 |  653 | `	return pConstObj;` |
|    67787 |  654 | `}` |
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
|   134370 |  717 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen,int bHeredoc)` |
|        5 |  718 | `{` |
|   134375 |  719 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|        - |  720 | `	const char *zIn,*zCur,*zEnd;` |
|   134375 |  721 | `	ph7_value *pObj = 0;` |
|        - |  722 | `	sxi32 iCons;` |
|        - |  723 | `	sxi32 nInterp;   /* how many of iCons came from an interpolated EXPRESSION */` |
|        - |  724 | `	sxi32 rc;` |
|        - |  725 | `	/* Delimit the string */` |
|   134375 |  726 | `	zIn  = pStr->zString;` |
|   134375 |  727 | `	zEnd = &zIn[pStr->nByte];` |
|   134375 |  728 | `	if( zIn >= zEnd ){` |
|        - |  729 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|        - |  730 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|        - |  731 | `		 * literal table from growing when many "" literals appear in the source.` |
|        - |  732 | `		 */` |
|      511 |  733 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|      511 |  734 | `		return SXRET_OK;` |
|        - |  735 | `	}` |
|   133869 |  736 | `	zCur = 0;` |
|        - |  737 | `	/* Compile the node */` |
|   133869 |  738 | `	iCons = 0;` |
|   133869 |  739 | `	nInterp = 0;` |
|    68159 |  740 | `	for(;;){` |
|   184019 |  741 | `		zCur = zIn;` |
|  1797925 |  742 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  1616369 |  743 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|       87 |  744 | `				break;` |
|  1616206 |  745 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|     2300 |  746 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|     1150 |  747 | `					break;` |
|        - |  748 | `			}` |
|  1613911 |  749 | `			zIn++;` |
|        5 |  750 | `		}` |
|   184019 |  751 | `		if( zIn > zCur ){` |
|   102681 |  752 | `			if( pObj == 0 ){` |
|   101955 |  753 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|   101955 |  754 | `				if( pObj == 0 ){` |
|      ! 0 |  755 | `					return SXERR_ABORT;` |
|        - |  756 | `				}` |
|    50975 |  757 | `			}` |
|   102681 |  758 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    51338 |  759 | `		}` |
|   184019 |  760 | `		if( zIn >= zEnd ){` |
|   133865 |  761 | `			break;` |
|        - |  762 | `		}` |
|    50159 |  763 | `		if( zIn[0] == '\\' ){` |
|    47701 |  764 | `			const char *zPtr = 0;` |
|        - |  765 | `			sxu32 n;` |
|    47701 |  766 | `			zIn++;` |
|    47701 |  767 | `			if( pObj == 0 ){` |
|    33619 |  768 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    33619 |  769 | `				if( pObj == 0 ){` |
|      ! 0 |  770 | `					return SXERR_ABORT;` |
|        - |  771 | `				}` |
|    16807 |  772 | `			}` |
|    47701 |  773 | `			if( zIn >= zEnd ){` |
|        - |  774 | `				/* Lone backslash at the very end of the body: php keeps it */` |
|        3 |  775 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|        3 |  776 | `				break;` |
|        - |  777 | `			}` |
|    47699 |  778 | `			n = sizeof(char); /* size of conversion */` |
|    47699 |  779 | `			switch( zIn[0] ){` |
|       28 |  780 | `			case '$':` |
|        - |  781 | `				/* Dollar sign */` |
|       59 |  782 | `				PH7_MemObjStringAppend(pObj,"$",sizeof(char));` |
|       59 |  783 | `				break;` |
|       75 |  784 | `			case '\\':` |
|        - |  785 | `				/* A literal backslash */` |
|      155 |  786 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|      155 |  787 | `				break;` |
|        1 |  788 | `			case 'e':` |
|        - |  789 | `				/* Escape (ESC) ASCII code 27 */` |
|        3 |  790 | `				PH7_MemObjStringAppend(pObj,"\x1b",sizeof(char));` |
|        3 |  791 | `				break;` |
|        4 |  792 | `			case 'f':` |
|        - |  793 | `				/* Form-feed (FF)[ctrl+l] ASCII code 12 */` |
|        9 |  794 | `				PH7_MemObjStringAppend(pObj,"\f",sizeof(char));` |
|        9 |  795 | `				break;` |
|    21142 |  796 | `			case 'n':` |
|        - |  797 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|    42289 |  798 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|    42289 |  799 | `				break;` |
|       37 |  800 | `			case 'r':` |
|        - |  801 | `				/* Carriage return (CR)[ctrl+m] ASCII code 13 */` |
|       79 |  802 | `				PH7_MemObjStringAppend(pObj,"\r",sizeof(char));` |
|       79 |  803 | `				break;` |
|     2103 |  804 | `			case 't':` |
|        - |  805 | `				/* Horizontal tab (HT)[ctrl+i] ASCII code 9 */` |
|     4211 |  806 | `				PH7_MemObjStringAppend(pObj,"\t",sizeof(char));` |
|     4211 |  807 | `				break;` |
|        3 |  808 | `			case 'v':` |
|        - |  809 | `				/* Vertical tab(VT)[ctrl+k] ASCII code 11 */` |
|        7 |  810 | `				PH7_MemObjStringAppend(pObj,"\v",sizeof(char));` |
|        7 |  811 | `				break;` |
|      157 |  812 | `			case '"':` |
|      319 |  813 | `				if( bHeredoc ){` |
|        - |  814 | `					/* No active quote char in a heredoc: php keeps \" verbatim */` |
|        5 |  815 | `					PH7_MemObjStringAppend(pObj,"\\\"",sizeof(char)*2);` |
|        3 |  816 | `				}else{` |
|        - |  817 | `					/* Double quote */` |
|      315 |  818 | `					PH7_MemObjStringAppend(pObj,"\"",sizeof(char));` |
|        - |  819 | `				}` |
|      319 |  820 | `				break;` |
|       26 |  821 | `			case '0': case '1': case '2': case '3':` |
|        - |  822 | `			case '4': case '5': case '6': case '7': {` |
|        - |  823 | `				/* \[0-7]{1,3}: a character in octal notation. A value above \377` |
|        - |  824 | `				 * warns and wraps to the low byte, matching php 8. */` |
|       54 |  825 | `				int c = 0;` |
|        - |  826 | `				char cOut;` |
|      152 |  827 | `				for( zPtr = zIn ; zPtr < &zIn[3*sizeof(char)] ; zPtr++ ){` |
|      130 |  828 | `					if( zPtr >= zEnd \|\| zPtr[0] < '0' \|\| zPtr[0] > '7' ){` |
|       17 |  829 | `						break;` |
|        - |  830 | `					}` |
|      100 |  831 | `					c = c * 8 + (zPtr[0] - '0');` |
|       51 |  832 | `				}` |
|       54 |  833 | `				if( c > 0xFF ){` |
|        - |  834 | `					SyString sSeq;` |
|        3 |  835 | `					SyStringInitFromBuf(&sSeq,zIn,(sxu32)(zPtr-zIn));` |
|        3 |  836 | `					PH7_GenCompileError(&(*pGen),E_WARNING,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|        - |  837 | `						"Octal escape sequence overflow \\%z is greater than \\377",&sSeq);` |
|        3 |  838 | `					c &= 0xFF;` |
|        1 |  839 | `				}` |
|       54 |  840 | `				cOut = (char)c; /* value byte, independent of host endianness */` |
|       54 |  841 | `				PH7_MemObjStringAppend(pObj,&cOut,sizeof(char));` |
|       54 |  842 | `				n = (sxu32)(zPtr-zIn);` |
|       54 |  843 | `				break;` |
|        - |  844 | `			}` |
|      247 |  845 | `			case 'x':` |
|      741 |  846 | `				if( &zIn[1] < zEnd && SyisHex((unsigned char)zIn[1]) ){` |
|        - |  847 | `					/* \x[0-9A-Fa-f]{1,2}: a character in hexadecimal notation */` |
|      492 |  848 | `					int c = SyHexToint(zIn[1]);` |
|        - |  849 | `					char cOut;` |
|      492 |  850 | `					n += sizeof(char);` |
|      492 |  851 | `					if( &zIn[2] < zEnd && SyisHex((unsigned char)zIn[2]) ){` |
|      488 |  852 | `						c = (c << 4) + SyHexToint(zIn[2]);` |
|      488 |  853 | `						n += sizeof(char);` |
|      243 |  854 | `					}` |
|      492 |  855 | `					cOut = (char)c; /* value byte, independent of host endianness */` |
|      492 |  856 | `					PH7_MemObjStringAppend(pObj,&cOut,sizeof(char));` |
|      247 |  857 | `				}else{` |
|        - |  858 | `					/* Not an escape: keep the backslash, as php does */` |
|        5 |  859 | `					PH7_MemObjStringAppend(pObj,"\\x",sizeof(char)*2);` |
|        - |  860 | `				}` |
|      496 |  861 | `				break;` |
|        9 |  862 | `			case 'u':` |
|       18 |  863 | `				if( &zIn[1] < zEnd && zIn[1] == '{'` |
|       22 |  864 | `				 && !(&zIn[2] < zEnd && zIn[2] == '$') ){` |
|        - |  865 | `					/* \u{codepoint}: UTF-8 encoding of the given codepoint (php 7+).` |
|        - |  866 | `					 * php encodes surrogates verbatim, so the only invalid value` |
|        - |  867 | `					 * is > U+10FFFF; malformed/empty braces are a compile error.` |
|        - |  868 | `					 * "\u{$..." is excluded above: php treats it as a literal \u` |
|        - |  869 | `					 * followed by {$...} curly interpolation. */` |
|       15 |  870 | `					sxu32 nCp = 0;` |
|       15 |  871 | `					zPtr = &zIn[2];` |
|       59 |  872 | `					while( zPtr < zEnd && SyisHex((unsigned char)zPtr[0]) ){` |
|       46 |  873 | `						if( nCp <= 0x10FFFF ){` |
|        - |  874 | `							/* stop accumulating once out of range: keeps a long` |
|        - |  875 | `							 * digit run from wrapping sxu32 */` |
|       46 |  876 | `							nCp = nCp * 16 + (sxu32)SyHexToint(zPtr[0]);` |
|       22 |  877 | `						}` |
|       46 |  878 | `						zPtr++;` |
|        2 |  879 | `					}` |
|       15 |  880 | `					if( zPtr == &zIn[2] \|\| zPtr >= zEnd \|\| zPtr[0] != '}' ){` |
|        - |  881 | `						/* Error recorded (nErr>0 fails the whole compile); consume the` |
|        - |  882 | `						 * malformed sequence so later errors are still reported. */` |
|        3 |  883 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|        - |  884 | `							"Invalid UTF-8 codepoint escape sequence");` |
|        3 |  885 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 |  886 | `							return SXERR_ABORT;` |
|        - |  887 | `						}` |
|        3 |  888 | `						n = (sxu32)(zPtr-zIn);` |
|        3 |  889 | `						if( zPtr < zEnd && zPtr[0] == '}' ){` |
|        3 |  890 | `							n += sizeof(char);` |
|        1 |  891 | `						}` |
|        3 |  892 | `						break;` |
|        - |  893 | `					}` |
|       12 |  894 | `					n = (sxu32)(&zPtr[1]-zIn); /* 'u{...}' incl. closing brace */` |
|       12 |  895 | `					if( nCp > 0x10FFFF ){` |
|        3 |  896 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|        - |  897 | `							"Invalid UTF-8 codepoint escape sequence: Codepoint too large");` |
|        3 |  898 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 |  899 | `							return SXERR_ABORT;` |
|        - |  900 | `						}` |
|        3 |  901 | `						break;` |
|        - |  902 | `					}` |
|        - |  903 | `					{` |
|        - |  904 | `						char zUtf[4];` |
|        9 |  905 | `						sxu8 *zOut = (sxu8 *)zUtf;` |
|        9 |  906 | `						SX_WRITE_UTF8(zOut,nCp);` |
|        9 |  907 | `						PH7_MemObjStringAppend(pObj,zUtf,(sxu32)(zOut-(sxu8 *)zUtf));` |
|        - |  908 | `					}` |
|        5 |  909 | `				}else{` |
|        - |  910 | `					/* Not an escape: keep the backslash, as php does */` |
|        7 |  911 | `					PH7_MemObjStringAppend(pObj,"\\u",sizeof(char)*2);` |
|        - |  912 | `				}` |
|       15 |  913 | `				break;` |
|       15 |  914 | `			default:` |
|        - |  915 | `				/* Unrecognized escape: keep the backslash, as php does.` |
|        - |  916 | `				 * zIn[-1] is the backslash itself, so both bytes are contiguous` |
|        - |  917 | `				 * in the source buffer — one batched append. */` |
|       31 |  918 | `				PH7_MemObjStringAppend(pObj,&zIn[-1],sizeof(char)*2);` |
|       30 |  919 | `				break;` |
|        - |  920 | `			}` |
|        - |  921 | `			/* Advance the stream cursor */` |
|    47699 |  922 | `			zIn += n;` |
|    47699 |  923 | `			continue;` |
|        - |  924 | `		}` |
|     2463 |  925 | `		if( zIn[0] == '{' ){` |
|        - |  926 | `			/* Curly syntax */` |
|        - |  927 | `			const char *zExpr;` |
|      171 |  928 | `			sxi32 iNest = 1;` |
|      171 |  929 | `			zIn++;` |
|      171 |  930 | `			zExpr = zIn;` |
|        - |  931 | `			/* Synchronize with the next closing curly braces */` |
|     1561 |  932 | `			while( zIn < zEnd ){` |
|     1561 |  933 | `				if( zIn[0] == '{' ){` |
|        - |  934 | `					/* Increment nesting level */` |
|        3 |  935 | `					iNest++;` |
|     1560 |  936 | `				}else if(zIn[0] == '}' ){` |
|        - |  937 | `					/* Decrement nesting level */` |
|      173 |  938 | `					iNest--;` |
|      173 |  939 | `					if( iNest <= 0 ){` |
|      171 |  940 | `						break;` |
|        - |  941 | `					}` |
|        1 |  942 | `				}` |
|     1393 |  943 | `				zIn++;` |
|        3 |  944 | `			}` |
|        - |  945 | `			/* Process the expression */` |
|      171 |  946 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|      171 |  947 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  948 | `				return SXERR_ABORT;` |
|        - |  949 | `			}` |
|      171 |  950 | `			if( rc != SXERR_EMPTY ){` |
|      171 |  951 | `				++iCons;` |
|      171 |  952 | `				++nInterp;` |
|       84 |  953 | `			}` |
|      171 |  954 | `			if( zIn < zEnd ){` |
|        - |  955 | `				/* Jump the trailing curly */` |
|      171 |  956 | `				zIn++;` |
|       84 |  957 | `			}` |
|       87 |  958 | `		}else{` |
|        - |  959 | `			/* Simple syntax */` |
|     2295 |  960 | `			const char *zExpr = zIn;` |
|        - |  961 | `			/* Assemble variable name */` |
|     1170 |  962 | `			for(;;){` |
|        - |  963 | `				/* Jump leading dollars */` |
|     4635 |  964 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|     2295 |  965 | `					zIn++;` |
|        5 |  966 | `				}` |
|     1170 |  967 | `				for(;;){` |
|    10971 |  968 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|     7461 |  969 | `						zIn++;` |
|        5 |  970 | `					}` |
|     2345 |  971 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|        - |  972 | `						/* UTF-8 stream */` |
|      ! 0 |  973 | `						zIn++;` |
|      ! 0 |  974 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  975 | `							zIn++;` |
|      ! 0 |  976 | `						}` |
|      ! 0 |  977 | `						continue;` |
|        - |  978 | `					}` |
|     2345 |  979 | `					break;` |
|      ! 0 |  980 | `				}` |
|     2345 |  981 | `				if( zIn >= zEnd ){` |
|      317 |  982 | `					break;` |
|        - |  983 | `				}` |
|     2033 |  984 | `				if( zIn[0] == '[' ){` |
|       12 |  985 | `					sxi32 iSquare = 1;` |
|       12 |  986 | `					zIn++;` |
|       28 |  987 | `					while( zIn < zEnd ){` |
|       28 |  988 | `						if( zIn[0] == '[' ){` |
|      ! 0 |  989 | `							iSquare++;` |
|       28 |  990 | `						}else if (zIn[0] == ']' ){` |
|       12 |  991 | `							iSquare--;` |
|       12 |  992 | `							if( iSquare <= 0 ){` |
|       12 |  993 | `								break;` |
|        - |  994 | `							}` |
|      ! 0 |  995 | `						}` |
|       18 |  996 | `						zIn++;` |
|        2 |  997 | `					}` |
|       12 |  998 | `					if( zIn < zEnd ){` |
|       12 |  999 | `						zIn++;` |
|        5 | 1000 | `					}` |
|       12 | 1001 | `					break;` |
|     2023 | 1002 | `				}else if(zIn[0] == '{' ){` |
|        3 | 1003 | `					sxi32 iCurly = 1;` |
|        3 | 1004 | `					zIn++;` |
|       11 | 1005 | `					while( zIn < zEnd ){` |
|       11 | 1006 | `						if( zIn[0] == '{' ){` |
|      ! 0 | 1007 | `							iCurly++;` |
|       11 | 1008 | `						}else if (zIn[0] == '}' ){` |
|        3 | 1009 | `							iCurly--;` |
|        3 | 1010 | `							if( iCurly <= 0 ){` |
|        3 | 1011 | `								break;` |
|        - | 1012 | `							}` |
|      ! 0 | 1013 | `						}` |
|        9 | 1014 | `						zIn++;` |
|        1 | 1015 | `					}` |
|        3 | 1016 | `					if( zIn < zEnd ){` |
|        3 | 1017 | `						zIn++;` |
|        1 | 1018 | `					}` |
|        3 | 1019 | `					break;` |
|     2021 | 1020 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|        - | 1021 | `					/* Member access operator '->' */` |
|       53 | 1022 | `					zIn += 2;` |
|     1996 | 1023 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|        - | 1024 | `					/* Static member access operator '::' */` |
|      ! 0 | 1025 | `					zIn += 2;` |
|      ! 0 | 1026 | `				}else{` |
|      988 | 1027 | `					break;` |
|        - | 1028 | `				}` |
|        3 | 1029 | `			}` |
|        - | 1030 | `			/*` |
|        - | 1031 | `			 * "$a[name]" — php's SIMPLE syntax takes an unquoted subscript as the string key` |
|        - | 1032 | `			 * 'name', never as a constant. PH7 handed "$a[name]" straight to the expression` |
|        - | 1033 | `			 * compiler, where the bare word only resolved because an unknown constant used to` |
|        - | 1034 | `			 * fall back to its own name as a string. With undefined constants now a real` |
|        - | 1035 | `			 * Error, quote the key here so the simple syntax keeps meaning what php means.` |
|        - | 1036 | `			 * A numeric ($a[0]) or variable ($a[$k]) subscript is already unambiguous.` |
|        - | 1037 | `			 */` |
|        - | 1038 | `			{` |
|     2295 | 1039 | `				const char *zBr = zExpr;` |
|    12153 | 1040 | `				while( zBr < zIn && zBr[0] != '[' ){` |
|     9863 | 1041 | `					zBr++;` |
|        5 | 1042 | `				}` |
|     2295 | 1043 | `				if( zBr < zIn && zIn[-1] == ']' ){` |
|       12 | 1044 | `					const char *zKey = &zBr[1];` |
|       12 | 1045 | `					const char *zKeyEnd = &zIn[-1];` |
|       12 | 1046 | `					const char *zScan = zKey;` |
|       12 | 1047 | `					int bBare = (zKey < zKeyEnd) && !SyisDigit(zKey[0]);` |
|       20 | 1048 | `					while( bBare && zScan < zKeyEnd ){` |
|        9 | 1049 | `						if( !SyisAlphaNum(zScan[0]) && zScan[0] != '_' ){` |
|      ! 0 | 1050 | `							bBare = 0;` |
|      ! 0 | 1051 | `						}` |
|        9 | 1052 | `						zScan++;` |
|        1 | 1053 | `					}` |
|       12 | 1054 | `					if( bBare ){` |
|        - | 1055 | `						SyBlob sSub;` |
|        3 | 1056 | `						SyBlobInit(&sSub,&pGen->pVm->sAllocator);` |
|        3 | 1057 | `						SyBlobAppend(&sSub,zExpr,(sxu32)(zBr - zExpr));` |
|        3 | 1058 | `						SyBlobAppend(&sSub,"['",2);` |
|        3 | 1059 | `						SyBlobAppend(&sSub,zKey,(sxu32)(zKeyEnd - zKey));` |
|        3 | 1060 | `						SyBlobAppend(&sSub,"']",2);` |
|        4 | 1061 | `						rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,` |
|        2 | 1062 | `							(const char *)SyBlobData(&sSub),` |
|        2 | 1063 | `							(const char *)SyBlobData(&sSub) + SyBlobLength(&sSub));` |
|        3 | 1064 | `						SyBlobRelease(&sSub);` |
|        3 | 1065 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 1066 | `							return SXERR_ABORT;` |
|        - | 1067 | `						}` |
|        3 | 1068 | `						if( rc != SXERR_EMPTY ){` |
|        3 | 1069 | `							++iCons;` |
|        3 | 1070 | `							++nInterp;` |
|        1 | 1071 | `						}` |
|        3 | 1072 | `						pObj = 0;` |
|        3 | 1073 | `						continue;` |
|        - | 1074 | `					}` |
|        4 | 1075 | `				}` |
|        - | 1076 | `			}` |
|        - | 1077 | `			/*` |
|        - | 1078 | `			 * "${...}" string interpolation (every form: ${name}, ${expr}, ${$x}) was` |
|        - | 1079 | `			 * DEPRECATED by php 8.2 in favor of the canonical "{$...}". PHL targets php's` |
|        - | 1080 | `			 * *non-deprecated* surface, so it is a hard parse error here — never silently` |
|        - | 1081 | `			 * rewritten. The canonical "{$var}" reaches this compiler by a different path` |
|        - | 1082 | `			 * and is unaffected.` |
|        - | 1083 | `			 */` |
|     2293 | 1084 | `			if( &zExpr[1] < zIn && zExpr[0] == '$' && zExpr[1] == '{' ){` |
|        3 | 1085 | `				PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|        - | 1086 | `					"syntax error, \"${\" string interpolation was removed in php 8.2, use \"{$...}\" instead");` |
|        3 | 1087 | `				return SXERR_ABORT;` |
|        - | 1088 | `			}` |
|        - | 1089 | `			/* Process the expression */` |
|     2291 | 1090 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|     2291 | 1091 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1092 | `				return SXERR_ABORT;` |
|        - | 1093 | `			}` |
|     2291 | 1094 | `			if( rc != SXERR_EMPTY ){` |
|     2291 | 1095 | `				++iCons;` |
|     2291 | 1096 | `				++nInterp;` |
|     1143 | 1097 | `			}` |
|        - | 1098 | `		}` |
|        - | 1099 | `		/* Invalidate the previously used constant */` |
|     2459 | 1100 | `		pObj = 0;` |
|        5 | 1101 | `	}/*for(;;)*/` |
|   133867 | 1102 | `	if( iCons > 1 ){` |
|        - | 1103 | `		/* Concatenate all compiled constants */` |
|     1683 | 1104 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|   133028 | 1105 | `	}else if( iCons == 1 && nInterp == 1 ){` |
|        - | 1106 | `		/* A string that is nothing but one interpolation ("$x") still has to` |
|        - | 1107 | `		 * PRODUCE A STRING. With no CAT to force the conversion the operand was` |
|        - | 1108 | ``		 * left on the stack untouched, so `$s = "$x"` handed back $x's own type:`` |
|        - | 1109 | `		 * "$arr" stayed an array (and skipped php's "Array to string conversion"` |
|        - | 1110 | `		 * warning), "$int" stayed an int, "$res" stayed a resource. */` |
|       18 | 1111 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CVT_STR,0,0,0,0);` |
|        8 | 1112 | `	}` |
|        - | 1113 | `	/* Node successfully compiled */` |
|   133867 | 1114 | `	return SXRET_OK;` |
|    67190 | 1115 | `}` |
|        - | 1116 | `/*` |
|        - | 1117 | ` * Compile a double quoted string.` |
|        - | 1118 | ` *  See the block-comment above for more information.` |
|        - | 1119 | ` */` |
|   134306 | 1120 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 | 1121 | `{` |
|        - | 1122 | `	sxi32 rc;` |
|   134311 | 1123 | `	rc = GenStateCompileString(&(*pGen),0/*bHeredoc*/);` |
|    67153 | 1124 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        - | 1125 | `	/* Compilation result */` |
|   134311 | 1126 | `	return rc;` |
|        5 | 1127 | `}` |
|        - | 1128 | `/*` |
|        - | 1129 | ` * Compile a Heredoc string.` |
|        - | 1130 | ` *  See the block-comment above for more information.` |
|        - | 1131 | ` */` |
|       68 | 1132 | `PH7_PRIVATE sxi32 PH7_CompileHereDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        4 | 1133 | `{` |
|        - | 1134 | `	SyString sOrig, sStripped;` |
|        - | 1135 | `	sxi32 rc;` |
|       72 | 1136 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|       72 | 1137 | `	if( rc != SXRET_OK ){` |
|        6 | 1138 | `		return rc;` |
|        - | 1139 | `	}` |
|        - | 1140 | `	/* Temporarily swap in the dedented body so GenStateCompileString` |
|        - | 1141 | `	 * (which reads pGen->pIn->sData directly) sees the stripped content.` |
|        - | 1142 | `	 * Restore before returning so downstream code that references pIn is` |
|        - | 1143 | `	 * unaffected, including on the error path. */` |
|       67 | 1144 | `	sOrig = pGen->pIn->sData;` |
|       67 | 1145 | `	pGen->pIn->sData = sStripped;` |
|       67 | 1146 | `	rc = GenStateCompileString(&(*pGen),1/*bHeredoc*/);` |
|       67 | 1147 | `	pGen->pIn->sData = sOrig;` |
|       32 | 1148 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       67 | 1149 | `	return rc;` |
|       38 | 1150 | `}` |
|        - | 1151 | `/*` |
|        - | 1152 | ` * Compile an array entry whether it is a key or a value.` |
|        - | 1153 | ` *  Notes on array entries.` |
|        - | 1154 | ` *  According to the PHP language reference manual` |
|        - | 1155 | ` *  An array can be created by the array() language construct.` |
|        - | 1156 | ` *  It takes as parameters any number of comma-separated key => value pairs.` |
|        - | 1157 | ` *  array(  key =>  value` |
|        - | 1158 | ` *    , ...` |
|        - | 1159 | ` *    )` |
|        - | 1160 | ` *  A key may be either an integer or a string. If a key is the standard representation` |
|        - | 1161 | ` *  of an integer, it will be interpreted as such (i.e. "8" will be interpreted as 8, while` |
|        - | 1162 | ` *  "08" will be interpreted as "08"). Floats in key are truncated to integer.` |
|        - | 1163 | ` *  The indexed and associative array types are the same type in PHP, which can both` |
|        - | 1164 | ` *  contain integer and string indices.` |
|        - | 1165 | ` *  A value can be any PHP type.` |
|        - | 1166 | ` *  If a key is not specified for a value, the maximum of the integer indices is taken` |
|        - | 1167 | ` *  and the new key will be that value plus 1. If a key that already has an assigned value` |
|        - | 1168 | ` *  is specified, that value will be overwritten.` |
|        - | 1169 | ` */` |
|  1704118 | 1170 | `PH7_PRIVATE sxi32 GenStateCompileArrayEntry(` |
|        - | 1171 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - | 1172 | `	SyToken *pIn,        /* Token stream */` |
|        - | 1173 | `	SyToken *pEnd,       /* End of the token stream */` |
|        - | 1174 | `	sxi32 iFlags,        /* Compilation flags */` |
|        - | 1175 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *) /* Expression tree validator callback */` |
|        - | 1176 | `	)` |
|        5 | 1177 | `{` |
|        - | 1178 | `	SyToken *pTmpIn,*pTmpEnd;` |
|        - | 1179 | `	sxi32 rc;` |
|        - | 1180 | `	/* Swap token stream */` |
|  1704123 | 1181 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|        - | 1182 | `	/* Compile the expression*/` |
|  1704123 | 1183 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|        - | 1184 | `	/* Restore token stream */` |
|  1704123 | 1185 | `	RE_SWAP_DELIMITER(pGen);` |
|  1704123 | 1186 | `	return rc;` |
|        5 | 1187 | `}` |
|        - | 1188 | `/*` |
|        - | 1189 | ` * Expression tree validator callback for the 'array' language construct.` |
|        - | 1190 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|        - | 1191 | ` * an invalid expression tree and this function will generate the appropriate` |
|        - | 1192 | ` * error message.` |
|        - | 1193 | ` * See the routine responible of compiling the array language construct` |
|        - | 1194 | ` * for more inforation.` |
|        - | 1195 | ` */` |
|       36 | 1196 | `static sxi32 GenStateArrayNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        5 | 1197 | `{` |
|       41 | 1198 | `	sxi32 rc = SXRET_OK;` |
|       41 | 1199 | `	if( pRoot->pOp ){` |
|       14 | 1200 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ &&` |
|       12 | 1201 | `			pRoot->pOp->iOp != EXPR_OP_FUNC_CALL /* function() [Symisc extension: i.e: array(&foo())] */` |
|       16 | 1202 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|        - | 1203 | `			/* Unexpected expression */` |
|       13 | 1204 | `			rc = PH7_GenSyntaxError(&(*pGen),pRoot->pStart,"\"->\" or \"?->\" or \"[\"");` |
|       13 | 1205 | `			if( rc != SXERR_ABORT ){` |
|       13 | 1206 | `				rc = SXERR_INVALID;` |
|        5 | 1207 | `			}` |
|        9 | 1208 | `		}` |
|       31 | 1209 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|        - | 1210 | `		/* Unexpected expression */` |
|        3 | 1211 | `		rc = PH7_GenSyntaxError(&(*pGen),pRoot->pStart,0);` |
|        3 | 1212 | `		if( rc != SXERR_ABORT ){` |
|        3 | 1213 | `			rc = SXERR_INVALID;` |
|        1 | 1214 | `		}` |
|        1 | 1215 | `	}` |
|       41 | 1216 | `	return rc;` |
|        5 | 1217 | `}` |
|        - | 1218 | `/*` |
|        - | 1219 | ` * Find the top-level '=>' (PH7_TK_ARRAY_OP) that separates an array/list entry's` |
|        - | 1220 | ` * key from its value within [pStart,pEnd). The scan skips any '=>' nested inside` |
|        - | 1221 | ` * brackets/parens/braces, inside an arrow-function signature (fn(...) =>), or` |
|        - | 1222 | ` * inside a match() {...} arm — none of which are key/value separators. Returns a` |
|        - | 1223 | ` * pointer to the '=>' token, or pEnd if the entry has no top-level separator.` |
|        - | 1224 | ` */` |
|  1611618 | 1225 | `PH7_PRIVATE SyToken * GenStateFindTopLevelArrow(SyToken *pStart,SyToken *pEnd)` |
|        5 | 1226 | `{` |
|  1611623 | 1227 | `	SyToken *pCur = pStart;` |
|  1611623 | 1228 | `	sxi32 iNest = 0;` |
|  4101945 | 1229 | `	while( pCur < pEnd ){` |
|  3068601 | 1230 | `		if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|   578275 | 1231 | `			return pCur;` |
|        - | 1232 | `		}` |
|        - | 1233 | `		/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|        - | 1234 | `		 * The '=>' inside an arrow function introduces the expression body,` |
|        - | 1235 | `		 * not an entry separator. Skip past the signature.` |
|        - | 1236 | `		 */` |
|  2490331 | 1237 | `		if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
|    24991 | 1238 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|    24991 | 1239 | `			SyToken *pFn = pCur;` |
|        - | 1240 | ``			/* Only a real `[static] fn[&](` opens an arrow function; `$fn`,`` |
|        - | 1241 | ``			 * `C::fn` and friends are plain names whose '=>' IS the separator. */`` |
|    24991 | 1242 | `			if( PH7_TokenOpensArrowFunc(pStart,pCur,pEnd) ){` |
|        5 | 1243 | `				if( nKw == PH7_TKWRD_STATIC ){` |
|      ! 0 | 1244 | `					pFn = &pCur[1];` |
|      ! 0 | 1245 | `				}` |
|        5 | 1246 | `				pCur = pFn + 1; /* past 'fn' */` |
|        5 | 1247 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_AMPER) ){` |
|      ! 0 | 1248 | `					pCur++;` |
|      ! 0 | 1249 | `				}` |
|        5 | 1250 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|        5 | 1251 | `					pCur++;` |
|        5 | 1252 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|        - | 1253 | `						PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|        5 | 1254 | `					if( pCur < pEnd ){` |
|        5 | 1255 | `						pCur++;` |
|        2 | 1256 | `					}` |
|        2 | 1257 | `				}` |
|        5 | 1258 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_COLON) ){` |
|      ! 0 | 1259 | `					pCur++;` |
|      ! 0 | 1260 | `					if( pCur < pEnd && (pCur->nType & PH7_TK_OP)` |
|      ! 0 | 1261 | `						&& pCur->sData.nByte == 1` |
|      ! 0 | 1262 | `						&& pCur->sData.zString[0] == '?' ){` |
|      ! 0 | 1263 | `						pCur++;` |
|      ! 0 | 1264 | `					}` |
|      ! 0 | 1265 | `					if( pCur < pEnd` |
|      ! 0 | 1266 | `						&& (pCur->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|      ! 0 | 1267 | `						pCur++;` |
|      ! 0 | 1268 | `					}` |
|      ! 0 | 1269 | `				}` |
|        - | 1270 | `				/* The rest of the entry is the arrow-function body — no outer` |
|        - | 1271 | `				 * key to extract. */` |
|        5 | 1272 | `				return pEnd;` |
|        - | 1273 | `			}` |
|        - | 1274 | `			/* Match expression (PHP 8.0): the '=>' inside match arms is not an` |
|        - | 1275 | `			 * entry separator. Skip past the full match span. */` |
|    24987 | 1276 | `			if( nKw == PH7_TKWRD_MATCH ){` |
|        3 | 1277 | `				pCur++; /* past 'match' */` |
|        3 | 1278 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|        3 | 1279 | `					pCur++;` |
|        3 | 1280 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|        - | 1281 | `						PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|        3 | 1282 | `					if( pCur < pEnd ){` |
|        3 | 1283 | `						pCur++;` |
|        1 | 1284 | `					}` |
|        1 | 1285 | `				}` |
|        3 | 1286 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_OCB) ){` |
|        3 | 1287 | `					pCur++;` |
|        3 | 1288 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|        - | 1289 | `						PH7_TK_OCB,PH7_TK_CCB,&pCur);` |
|        3 | 1290 | `					if( pCur < pEnd ){` |
|        3 | 1291 | `						pCur++;` |
|        1 | 1292 | `					}` |
|        1 | 1293 | `				}` |
|        3 | 1294 | `				continue;` |
|        - | 1295 | `			}` |
|    12490 | 1296 | `		}` |
|  2490325 | 1297 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|    58749 | 1298 | `			iNest++;` |
|  2460953 | 1299 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/) ){` |
|        - | 1300 | `			/* Don't worry about mismatched brackets here, the expression` |
|        - | 1301 | `			 * parser will shortly detect any syntax error. */` |
|    58749 | 1302 | `			iNest--;` |
|    29372 | 1303 | `		}` |
|  2490325 | 1304 | `		pCur++;` |
|        5 | 1305 | `	}` |
|  1033349 | 1306 | `	return pEnd;` |
|   805814 | 1307 | `}` |
|        - | 1308 | `/*` |
|        - | 1309 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|        - | 1310 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|        - | 1311 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|        - | 1312 | ` */` |
|   725356 | 1313 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|        5 | 1314 | `{` |
|        - | 1315 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|        - | 1316 | `	SyToken *pKey,*pCur;` |
|   725361 | 1317 | `	sxi32 iEmitRef = 0;` |
|   725361 | 1318 | `	sxi32 iSpread = 0;` |
|   725361 | 1319 | `	sxi32 nPair = 0;` |
|        - | 1320 | `	sxi32 rc;` |
|   725361 | 1321 | `	xValidator = 0;` |
|   996017 | 1322 | `	for(;;){` |
|        - | 1323 | `		/* Jump leading commas. Exactly ONE separates two entries; a second one (or a comma` |
|        - | 1324 | `		 * at the very start) means an EMPTY element, which php rejects outright — PH7 just` |
|        - | 1325 | ``		 * skipped them, so `array(,)` and `array(1,,2)` compiled silently. A TRAILING comma`` |
|        - | 1326 | `		 * is legal and is handled by the loop exiting on the next pass. */` |
|   633339 | 1327 | `		{` |
|  1992039 | 1328 | `			int nSkip = 0;` |
|  2956511 | 1329 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|   964477 | 1330 | `				nSkip++;` |
|   964477 | 1331 | `				pGen->pIn++;` |
|        5 | 1332 | `			}` |
|  1992039 | 1333 | `			if( nSkip > 1 \|\| (nSkip > 0 && nPair < 1) ){` |
|      ! 0 | 1334 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn[-1].nLine,` |
|        - | 1335 | `					"Cannot use empty array elements in arrays");` |
|      ! 0 | 1336 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1337 | `					return SXERR_ABORT;` |
|        - | 1338 | `				}` |
|      ! 0 | 1339 | `				return SXRET_OK;` |
|        - | 1340 | `			}` |
|        - | 1341 | `		}` |
|  1992039 | 1342 | `		pCur = pGen->pIn;` |
|  1992039 | 1343 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|        - | 1344 | `			/* No more entry to process */` |
|   725343 | 1345 | `			break;` |
|        - | 1346 | `		}` |
|  1266701 | 1347 | `		if( pCur >= pGen->pIn ){` |
|      ! 0 | 1348 | `			continue;` |
|        - | 1349 | `		}` |
|        - | 1350 | `		/* Compile the key if available */` |
|  1266701 | 1351 | `		pKey = pCur;` |
|  1266701 | 1352 | `		pCur = GenStateFindTopLevelArrow(pCur,pGen->pIn);` |
|  1266701 | 1353 | `		rc = SXERR_EMPTY;` |
|  1266701 | 1354 | `		if( pCur < pGen->pIn ){` |
|   437083 | 1355 | `			if( pKey == pCur ){` |
|        - | 1356 | ``				/* `array( => 2)`: the entry STARTS with '=>', so it has no key. php rejects`` |
|        - | 1357 | `				 * it; PH7 warned about a "Missing entry key" and compiled on, accepting` |
|        - | 1358 | ``				 * source php refuses. (The `else if` below could never see this: the arrow`` |
|        - | 1359 | `				 * IS found here, so control never reached it.)` |
|        - | 1360 | `				 * php names the literal's own closer, so short syntax expects ']'. */` |
|        3 | 1361 | `				const char *zClose = (pGen->pEnd && (pGen->pEnd->nType & PH7_TK_CSB))` |
|        - | 1362 | `					? "\"]\"" : "\")\"";` |
|        3 | 1363 | `				rc = PH7_GenSyntaxError(&(*pGen),pCur,zClose);` |
|        3 | 1364 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1365 | `					return SXERR_ABORT;` |
|        - | 1366 | `				}` |
|        3 | 1367 | `				return SXRET_OK;` |
|        - | 1368 | `			}` |
|   437081 | 1369 | `			if( &pCur[1] >= pGen->pIn ){` |
|        - | 1370 | ``				/* `array(1 => )`: php names the token that SHOULD have started the value —`` |
|        - | 1371 | `				 * the ')' or ']' closing the literal — not the '=>' it just read. Passing 0` |
|        - | 1372 | `				 * makes the helper reach for the token past this entry's slice. */` |
|       12 | 1373 | `				rc = PH7_GenSyntaxError(&(*pGen),0,0);` |
|       12 | 1374 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1375 | `					return SXERR_ABORT;` |
|        - | 1376 | `				}` |
|       12 | 1377 | `				return SXRET_OK;` |
|        - | 1378 | `			}` |
|        - | 1379 | `			/* Compile the expression holding the key */` |
|   437071 | 1380 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|        - | 1381 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|   437071 | 1382 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1383 | `				return SXERR_ABORT;` |
|        - | 1384 | `			}` |
|   437071 | 1385 | `			pCur++; /* Jump the '=>' operator */` |
|   218538 | 1386 | `		}else{` |
|        - | 1387 | `			/* Reset back the cursor and point to the entry value */` |
|   829623 | 1388 | `			pCur = pKey;` |
|        - | 1389 | `		}` |
|  1266689 | 1390 | `		if( rc == SXERR_EMPTY ){` |
|        - | 1391 | `			/* No key given: load the nil, TAGGED so LOAD_MAP knows this is an absent key` |
|        - | 1392 | ``			 * (auto-index) rather than an explicit `null =>` one, which php deprecates. */`` |
|   829623 | 1393 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,PH7_LOADC_NOKEY,0 /* nil index */,0,0);` |
|   414809 | 1394 | `		}` |
|  1266689 | 1395 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
|        - | 1396 | `			/* Insertion by reference, [i.e: $a = array(&$x);] */` |
|       45 | 1397 | `			xValidator = GenStateArrayNodeValidator; /* Only variable are allowed */` |
|       45 | 1398 | `			iEmitRef = 1;` |
|       45 | 1399 | `			pCur++; /* Jump the '&' token */` |
|       45 | 1400 | `			if( pCur >= pGen->pIn ){` |
|        - | 1401 | `				/* Missing value */` |
|        - | 1402 | ``				/* php reports the token that actually stopped it (`array(&)` -> the`` |
|        - | 1403 | `				 * ')'), not a hand-written "missing referenced variable" fatal. */` |
|        3 | 1404 | `				rc = PH7_GenSyntaxError(&(*pGen),pCur < pGen->pIn ? pCur : 0,0);` |
|        3 | 1405 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1406 | `					return SXERR_ABORT;` |
|        - | 1407 | `				}` |
|        3 | 1408 | `				return SXRET_OK;` |
|        - | 1409 | `			}` |
|       19 | 1410 | `		}` |
|        - | 1411 | `		/* Detect array unpack: '...$expr' as the entry value (PHP 7.4+, with` |
|        - | 1412 | `		 * string-key support since PHP 8.1). The parser strips the '...' inside` |
|        - | 1413 | `		 * ExprExtractNode; we only need to know it's there so we can emit` |
|        - | 1414 | `		 * PH7_OP_FLAG_SPREAD after the value, instructing LOAD_MAP to merge the` |
|        - | 1415 | `		 * resulting hashmap rather than insert it as a scalar entry. */` |
|  1266687 | 1416 | `		iSpread = (pCur < pGen->pIn && (pCur->nType & PH7_TK_ELLIPSIS)) ? 1 : 0;` |
|  1266687 | 1417 | `		if( iSpread && (rc != SXERR_EMPTY \|\| iEmitRef) ){` |
|        - | 1418 | `			/* '[k => ...$a]' and '[&...$a]' are syntax errors in PHP — the` |
|        - | 1419 | `			 * '...' token cannot follow either '=>' or '&' inside an array` |
|        - | 1420 | `			 * literal. Emit the same Parse-error wording PHP uses so the` |
|        - | 1421 | `			 * output is engine-portable. */` |
|        6 | 1422 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pCur->nLine,` |
|        - | 1423 | `				"syntax error, unexpected token \"...\"");` |
|        6 | 1424 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1425 | `				return SXERR_ABORT;` |
|        - | 1426 | `			}` |
|        6 | 1427 | `			return SXRET_OK;` |
|        - | 1428 | `		}` |
|        - | 1429 | ``		/* Compile indice value. A BY-REF element (`'k' => &$a[$i]`) is an`` |
|        - | 1430 | `		 * lvalue: php VIVIFIES a missing subscript when a reference is taken,` |
|        - | 1431 | `		 * so compile it in write context (LOAD_IDX iP2=1, create-if-missing)` |
|        - | 1432 | `		 * instead of a read-only load — which also keeps the undefined-key` |
|        - | 1433 | `		 * warning (a read-only diagnostic) from false-firing here. */` |
|  1900022 | 1434 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,` |
|   633339 | 1435 | `			iEmitRef ? EXPR_FLAG_LOAD_IDX_STORE` |
|        - | 1436 | `			         : EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,` |
|   633339 | 1437 | `			xValidator);` |
|  1266683 | 1438 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1439 | `			return SXERR_ABORT;` |
|        - | 1440 | `		}` |
|  1266683 | 1441 | `		if( iSpread ){` |
|        - | 1442 | `			/* Mark the value on TOS as a spread source; LOAD_MAP merges it. */` |
|       73 | 1443 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FLAG_SPREAD,0,0,0,0);` |
|  1266648 | 1444 | `		}else if( iEmitRef ){` |
|        - | 1445 | `			/* Emit the load reference instruction */` |
|       41 | 1446 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|       18 | 1447 | `		}` |
|  1266683 | 1448 | `		xValidator = 0;` |
|  1266683 | 1449 | `		iEmitRef = 0;` |
|  1266683 | 1450 | `		iSpread = 0;` |
|  1266683 | 1451 | `		nPair++;` |
|        5 | 1452 | `	}` |
|        - | 1453 | `	/* Emit the load map instruction */` |
|   725343 | 1454 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|        - | 1455 | `	/* Node successfully compiled */` |
|   725343 | 1456 | `	return SXRET_OK;` |
|   362683 | 1457 | `}` |
|        - | 1458 | `/*` |
|        - | 1459 | ` * Compile the 'array' language construct.` |
|        - | 1460 | ` *	 According to the PHP language reference manual` |
|        - | 1461 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|        - | 1462 | ` *   values to keys. This type is optimized for several different uses; it can` |
|        - | 1463 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|        - | 1464 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|        - | 1465 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|        - | 1466 | ` */` |
|   457242 | 1467 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 | 1468 | `{` |
|        - | 1469 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   457247 | 1470 | `	pGen->pIn += 2;` |
|   457247 | 1471 | `	pGen->pEnd--;` |
|   228621 | 1472 | `	SXUNUSED(iCompileFlag);` |
|        - | 1473 | ``	/* php: a stray token in an `array( ... )` element is `... expecting ")"`. */`` |
|        - | 1474 | `	{` |
|   457247 | 1475 | `		const char *zSave = pGen->zClauseCloser;` |
|        - | 1476 | `		sxi32 rc;` |
|   457247 | 1477 | `		pGen->zClauseCloser = "\")\"";` |
|   457247 | 1478 | `		rc = GenStateCompileArrayBody(pGen);` |
|   457247 | 1479 | `		pGen->zClauseCloser = zSave;` |
|   457247 | 1480 | `		return rc;` |
|        - | 1481 | `	}` |
|        5 | 1482 | `}` |
|        - | 1483 | `/*` |
|        - | 1484 | ` * Compile the PHP 8.5 clone(...) call form:` |
|        - | 1485 | `` *   clone($object)                          -> identical to the `clone $object` operator`` |
|        - | 1486 | ` *   clone($object, ['prop' => value, ...])  -> clone, run __clone(), then apply the` |
|        - | 1487 | ` *                                              property updates as scope-aware writes` |
|        - | 1488 | ` *   clone(object: $o, withProperties: [..]) -> the named-argument spelling` |
|        - | 1489 | ` * Codegen: compile the object argument and emit OP_CLONE (which clones and runs` |
|        - | 1490 | ` * __clone()); if a withProperties argument is present, compile it and emit` |
|        - | 1491 | ` * OP_CLONE_APPLY, which applies each update to the fresh clone AFTER __clone(),` |
|        - | 1492 | ` * honouring visibility / readonly-set-scope / typed-property enforcement in the` |
|        - | 1493 | ` * calling scope. The parser (ExprExtractNode) delimited this node's tokens as` |
|        - | 1494 | `` * `clone ( ... )`; pGen->pIn/pEnd point at the first/one-past-last of that range.`` |
|        - | 1495 | ` */` |
|       22 | 1496 | `PH7_PRIVATE sxi32 PH7_CompileCloneCall(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        2 | 1497 | `{` |
|        - | 1498 | `	SyToken *pIn,*pEnd,*pNext;` |
|       24 | 1499 | `	SyToken *pObjStart = 0,*pObjEnd = 0;` |
|       24 | 1500 | `	SyToken *pUpdStart = 0,*pUpdEnd = 0;` |
|       24 | 1501 | `	int nArg = 0;` |
|        - | 1502 | `	sxi32 rc;` |
|       11 | 1503 | `	SXUNUSED(iCompileFlag);` |
|        - | 1504 | `	/* pGen->pIn -> 'clone', pGen->pIn[1] -> '(', pGen->pEnd -> one past ')'. */` |
|       24 | 1505 | `	pIn  = pGen->pIn + 2;   /* skip 'clone' and the opening '(' */` |
|       24 | 1506 | `	pEnd = pGen->pEnd - 1;  /* exclude the closing ')' */` |
|        - | 1507 | `	/* clone(...) first-class-callable form: a lone ellipsis is the whole list. */` |
|       24 | 1508 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|      ! 0 | 1509 | `		return PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|        - | 1510 | `			"clone(...) first-class callable form is not yet supported");` |
|        - | 1511 | `	}` |
|        - | 1512 | `	/* Split the (at most two) comma-separated arguments, tolerating named labels. */` |
|       62 | 1513 | `	while( pIn < pEnd ){` |
|       40 | 1514 | `		SyToken *pArgStart,*pArgEnd,*pName = 0;` |
|       40 | 1515 | `		if( PH7_GetNextExpr(pIn,pEnd,&pNext) != SXRET_OK ){` |
|      ! 0 | 1516 | `			break;` |
|        - | 1517 | `		}` |
|       40 | 1518 | `		pArgStart = pIn;` |
|       40 | 1519 | `		pArgEnd   = pNext;` |
|        - | 1520 | `		/* Named-argument label: <ID\|keyword> ':' expr. A single ':' is PH7_TK_COLON;` |
|        - | 1521 | ``		 * '::' is a distinct operator token, so this never mis-fires on `A::B`. */`` |
|       38 | 1522 | `		if( (pArgEnd - pArgStart) >= 2` |
|       37 | 1523 | `			&& (pArgStart[0].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|       23 | 1524 | `			&& (pArgStart[1].nType & PH7_TK_COLON) ){` |
|        5 | 1525 | `			pName = pArgStart;` |
|        5 | 1526 | `			pArgStart += 2;` |
|        2 | 1527 | `		}` |
|       40 | 1528 | `		if( pName ){` |
|        - | 1529 | `` 			/* PHP named parameters are case-SENSITIVE, so `Object:`/`WITHPROPERTIES:` `` |
|        - | 1530 | `			 * must be rejected as unknown (SyMemcmp, not SyStrnicmp). */` |
|        4 | 1531 | `			if( pName->sData.nByte == sizeof("object")-1` |
|        4 | 1532 | `				&& SyMemcmp(pName->sData.zString,"object",sizeof("object")-1) == 0 ){` |
|        3 | 1533 | `				pObjStart = pArgStart; pObjEnd = pArgEnd;` |
|        4 | 1534 | `			}else if( pName->sData.nByte == sizeof("withProperties")-1` |
|        3 | 1535 | `				&& SyMemcmp(pName->sData.zString,"withProperties",sizeof("withProperties")-1) == 0 ){` |
|        3 | 1536 | `				pUpdStart = pArgStart; pUpdEnd = pArgEnd;` |
|        2 | 1537 | `			}else{` |
|      ! 0 | 1538 | `				return PH7_GenCompileError(pGen,E_ERROR,pName->nLine,` |
|      ! 0 | 1539 | `					"Unknown named parameter $%z",&pName->sData);` |
|        1 | 1540 | `			}` |
|       38 | 1541 | `		}else if( nArg == 0 ){` |
|       22 | 1542 | `			pObjStart = pArgStart; pObjEnd = pArgEnd;` |
|       25 | 1543 | `		}else if( nArg == 1 ){` |
|       15 | 1544 | `			pUpdStart = pArgStart; pUpdEnd = pArgEnd;` |
|        8 | 1545 | `		}else{` |
|      ! 0 | 1546 | `			return PH7_GenCompileError(pGen,E_ERROR,pArgStart->nLine,` |
|        - | 1547 | `				"clone() expects at most 2 arguments");` |
|        - | 1548 | `		}` |
|       40 | 1549 | `		nArg++;` |
|       40 | 1550 | `		pIn = pNext;` |
|       40 | 1551 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|       17 | 1552 | `			pIn++; /* step over the argument separator */` |
|        8 | 1553 | `		}` |
|        2 | 1554 | `	}` |
|       24 | 1555 | `	if( pObjStart == 0 \|\| pObjStart >= pObjEnd ){` |
|      ! 0 | 1556 | `		return PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 1557 | `			"clone() expects at least 1 argument, 0 given");` |
|        - | 1558 | `	}` |
|        - | 1559 | `	/* Object argument -> clone (+ __clone()). */` |
|       24 | 1560 | `	rc = GenStateCompileArrayEntry(pGen,pObjStart,pObjEnd,EXPR_FLAG_RDONLY_LOAD,0);` |
|       24 | 1561 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 1562 | `		return SXERR_ABORT;` |
|        - | 1563 | `	}` |
|       24 | 1564 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_CLONE,0,0,0,0);` |
|        - | 1565 | `	/* Property updates (evaluated after __clone runs). */` |
|       24 | 1566 | `	if( pUpdStart && pUpdStart < pUpdEnd ){` |
|       17 | 1567 | `		rc = GenStateCompileArrayEntry(pGen,pUpdStart,pUpdEnd,EXPR_FLAG_RDONLY_LOAD,0);` |
|       17 | 1568 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1569 | `			return SXERR_ABORT;` |
|        - | 1570 | `		}` |
|       17 | 1571 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CLONE_APPLY,0,0,0,0);` |
|        8 | 1572 | `	}` |
|       24 | 1573 | `	return SXRET_OK;` |
|       13 | 1574 | `}` |
|        - | 1575 | `/*` |
|        - | 1576 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|        - | 1577 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|        - | 1578 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|        - | 1579 | ` */` |
|   268114 | 1580 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 | 1581 | `{` |
|        - | 1582 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|   268119 | 1583 | `	pGen->pIn++;` |
|   268119 | 1584 | `	pGen->pEnd--;` |
|   134057 | 1585 | `	SXUNUSED(iCompileFlag);` |
|        - | 1586 | ``	/* php: a stray token in a `[ ... ]` element is `... expecting "]"`. */`` |
|        - | 1587 | `	{` |
|   268119 | 1588 | `		const char *zSave = pGen->zClauseCloser;` |
|        - | 1589 | `		sxi32 rc;` |
|   268119 | 1590 | `		pGen->zClauseCloser = "\"]\"";` |
|   268119 | 1591 | `		rc = GenStateCompileArrayBody(pGen);` |
|   268119 | 1592 | `		pGen->zClauseCloser = zSave;` |
|   268119 | 1593 | `		return rc;` |
|        - | 1594 | `	}` |
|        5 | 1595 | `}` |
|        - | 1596 | `/*` |
|        - | 1597 | ` * Expression tree validator callback for the 'list' language construct.` |
|        - | 1598 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|        - | 1599 | ` * an invalid expression tree and this function will generate the appropriate` |
|        - | 1600 | ` * error message.` |
|        - | 1601 | ` * See the routine responible of compiling the list language construct` |
|        - | 1602 | ` * for more inforation.` |
|        - | 1603 | ` */` |
|      286 | 1604 | `static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        5 | 1605 | `{` |
|      291 | 1606 | `	sxi32 rc = SXRET_OK;` |
|      291 | 1607 | `	if( pRoot->pOp ){` |
|       46 | 1608 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */` |
|       26 | 1609 | `			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){` |
|        - | 1610 | `				/* Unexpected expression */` |
|      ! 0 | 1611 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - | 1612 | `					"Assignments can only happen to writable values");` |
|      ! 0 | 1613 | `				if( rc != SXERR_ABORT ){` |
|      ! 0 | 1614 | `					rc = SXERR_INVALID;` |
|      ! 0 | 1615 | `				}` |
|        2 | 1616 | `		}` |
|      268 | 1617 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|        - | 1618 | `		/* Unexpected expression */` |
|        6 | 1619 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - | 1620 | `			"Assignments can only happen to writable values");` |
|        6 | 1621 | `		if( rc != SXERR_ABORT ){` |
|        6 | 1622 | `			rc = SXERR_INVALID;` |
|        2 | 1623 | `		}` |
|        2 | 1624 | `	}` |
|      291 | 1625 | `	return rc;` |
|        5 | 1626 | `}` |
|        - | 1627 | `/*` |
|        - | 1628 | ` * Compile the 'list' language construct.` |
|        - | 1629 | ` *  According to the PHP language reference` |
|        - | 1630 | ` *  list(): Assign variables as if they were an array.` |
|        - | 1631 | ` *  list() is used to assign a list of variables in one operation.` |
|        - | 1632 | ` *  Description` |
|        - | 1633 | ` *   array list (mixed $varname [, mixed $... ] )` |
|        - | 1634 | ` *   Like array(), this is not really a function, but a language construct.` |
|        - | 1635 | ` *   list() is used to assign a list of variables in one operation.` |
|        - | 1636 | ` *  Parameters` |
|        - | 1637 | ` *   $varname: A variable.` |
|        - | 1638 | ` *  Return Values` |
|        - | 1639 | ` *   The assigned array.` |
|        - | 1640 | ` */` |
|        - | 1641 | `/* Nested list entry recorded during first pass of list body compilation */` |
|        - | 1642 | `struct NestedListEntry {` |
|        - | 1643 | `	sxi32 nIndex;        /* Position in the outer list (0-based) */` |
|        - | 1644 | `	SyToken *pStart;     /* Token range: start of nested construct */` |
|        - | 1645 | `	SyToken *pEnd;       /* Token range: past closing delimiter */` |
|        - | 1646 | `	sxi32 isShort;       /* 1 if [...] form, 0 if list(...) form */` |
|        - | 1647 | `};` |
|        - | 1648 | `/*` |
|        - | 1649 | ` * Compile the body of a *keyed* list/short-list destructuring (PHP 7.1), where` |
|        - | 1650 | `` * every entry has the form `keyExpr => target`. The source array is on the stack`` |
|        - | 1651 | ` * top on entry and remains there on exit, mirroring the positional LOAD_LIST` |
|        - | 1652 | ` * path so the caller's teardown is unchanged. For each entry: DUP the source,` |
|        - | 1653 | ` * push the key, LOAD_IDX to fetch source[key] (NULL on a missing key, silently,` |
|        - | 1654 | ` * like a normal subscript read), then assign the fetched value to the target — a` |
|        - | 1655 | ` * nested [...]/list() recurses, a simple lvalue uses the same STORE fold as a` |
|        - | 1656 | ` * normal assignment (the value sits below the lvalue-load, exactly as in` |
|        - | 1657 | ` * GenStateEmitExprCode where the assignment RHS precedes the LHS load).` |
|        - | 1658 | ` */` |
|       34 | 1659 | `static sxi32 GenStateCompileKeyedListBody(ph7_gen_state *pGen)` |
|        3 | 1660 | `{` |
|        - | 1661 | `	SyToken *pNext;` |
|        - | 1662 | `	sxi32 rc;` |
|       79 | 1663 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|        - | 1664 | `		SyToken *pArrow,*pTarget;` |
|        - | 1665 | ``		/* Split `keyExpr => target` at the top-level '=>' */`` |
|       45 | 1666 | `		pArrow = GenStateFindTopLevelArrow(pGen->pIn,pNext);` |
|       45 | 1667 | `		pTarget = &pArrow[1];` |
|       45 | 1668 | `		if( pArrow <= pGen->pIn \|\| pTarget >= pNext ){` |
|        - | 1669 | ``			/* Empty key (`[ => $v]`) or empty value (`["k" =>]`): PHP rejects`` |
|        - | 1670 | `			 * both. Reject rather than silently emitting unbalanced bytecode. */` |
|      ! 0 | 1671 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - | 1672 | `				"Cannot use empty array entries in keyed array assignment");` |
|      ! 0 | 1673 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 1674 | `		}` |
|        - | 1675 | `		/* DUP the source array (it is on the stack top) */` |
|       45 | 1676 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|        - | 1677 | `		/* Compile the key expression; it is pushed above the DUP'd source */` |
|       45 | 1678 | `		rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pArrow,EXPR_FLAG_RDONLY_LOAD,0);` |
|       45 | 1679 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1680 | `			return SXERR_ABORT;` |
|        - | 1681 | `		}` |
|        - | 1682 | `		/* LOAD_IDX: pop the key, replace the DUP'd source with source[key].` |
|        - | 1683 | `		 * iP2=7 is the keyed-destructuring read context: an array source reads like` |
|        - | 1684 | ``		 * iP2=0 (missing key loads NULL silently, matching a normal `$arr[$k]` read;`` |
|        - | 1685 | `		 * PHP also emits an "Undefined array key" warning here, PHL omits it — §3.7),` |
|        - | 1686 | `		 * but a NON-array source yields NULL + a per-key "Cannot use <type> as array"` |
|        - | 1687 | `		 * warning instead of char-indexing a string (matching PHP's OP_LOAD_LIST path). */` |
|       45 | 1688 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,7,0,0);` |
|       45 | 1689 | `		if( pTarget < pNext && ( (pTarget->nType & PH7_TK_OSB)` |
|       40 | 1690 | `			\|\| ( (pTarget->nType & PH7_TK_KEYWORD)` |
|       21 | 1691 | `				&& SX_PTR_TO_INT(pTarget->pUserData) == PH7_TKWRD_LIST ) ) ){` |
|        - | 1692 | `			/* Nested destructuring:  ["k" => [ ... ]]  or  ["k" => list( ... )].` |
|        - | 1693 | `			 * Treat source[key] as the inner body's source, then drop the` |
|        - | 1694 | `			 * leftover it leaves behind (mirrors the positional nested path). */` |
|        5 | 1695 | `			sxi32 isShort = (pTarget->nType & PH7_TK_OSB) != 0;` |
|        5 | 1696 | `			SyToken *pSavedIn = pGen->pIn;` |
|        5 | 1697 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|        5 | 1698 | `			pGen->pIn = pTarget;` |
|        5 | 1699 | `			pGen->pEnd = pNext;` |
|        5 | 1700 | `			rc = isShort ? PH7_CompileShortList(&(*pGen),0)` |
|        2 | 1701 | `			             : PH7_CompileList(&(*pGen),0);` |
|        5 | 1702 | `			pGen->pIn = pSavedIn;` |
|        5 | 1703 | `			pGen->pEnd = pSavedEnd;` |
|        5 | 1704 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1705 | `				return SXERR_ABORT;` |
|        - | 1706 | `			}` |
|        5 | 1707 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        3 | 1708 | `		}else{` |
|        - | 1709 | `			/* Simple lvalue target ($v / $o->p / $a[i] / Cls::$s). source[key]` |
|        - | 1710 | `			 * is already on the stack as the value; compiling the target appends` |
|        - | 1711 | `			 * its lvalue-load, which we fold into a STORE just as a normal` |
|        - | 1712 | `			 * assignment does. */` |
|        - | 1713 | `			VmInstr *pInstr;` |
|       41 | 1714 | `			sxi32 iVmOp = PH7_OP_STORE;` |
|       41 | 1715 | `			sxi32 iP1 = 0, iP2 = 0;` |
|       41 | 1716 | `			void *p3 = 0;` |
|       41 | 1717 | `			rc = GenStateCompileArrayEntry(&(*pGen),pTarget,pNext,` |
|        - | 1718 | `				EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|       41 | 1719 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1720 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 1721 | `			}` |
|       41 | 1722 | `			if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|       41 | 1723 | `				if( pInstr->iOp == PH7_OP_MEMBER ){` |
|        6 | 1724 | `					iP2 = 1; /* member store: keep MEMBER, store value below it */` |
|       38 | 1725 | `				}else if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|        3 | 1726 | `					iVmOp = PH7_OP_STORE_IDX;` |
|        3 | 1727 | `					iP1 = pInstr->iP1;` |
|        3 | 1728 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|        2 | 1729 | `				}else{` |
|       34 | 1730 | `					p3 = pInstr->p3; /* named store: $v = value */` |
|       34 | 1731 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|        - | 1732 | `				}` |
|       19 | 1733 | `			}` |
|       41 | 1734 | `			PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|        - | 1735 | `			/* STORE leaves the assigned value on the stack top; drop it so the` |
|        - | 1736 | `			 * source array is back on top for the next entry. */` |
|       41 | 1737 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        - | 1738 | `		}` |
|       45 | 1739 | `		pGen->pIn = &pNext[1];` |
|        3 | 1740 | `	}` |
|       37 | 1741 | `	return SXRET_OK;` |
|       20 | 1742 | `}` |
|        - | 1743 | `/*` |
|        - | 1744 | ` * Shared body for list() and short list [...] compilation.` |
|        - | 1745 | ` * Assumes pGen->pIn and pGen->pEnd are already positioned past` |
|        - | 1746 | ` * the opening delimiter and before the closing delimiter.` |
|        - | 1747 | ` */` |
|      190 | 1748 | `static sxi32 GenStateCompileListBody(ph7_gen_state *pGen)` |
|        5 | 1749 | `{` |
|        - | 1750 | `	SySet sNested; /* Dynamically-sized container of NestedListEntry */` |
|        - | 1751 | `	SyToken *pNext;` |
|        - | 1752 | `	SyToken *pClassifyIn;` |
|      195 | 1753 | `	sxi32 nKeyed = 0, nPositional = 0, nEmpty = 0;` |
|        - | 1754 | `	sxi32 nExpr;` |
|        - | 1755 | `	sxi32 rc;` |
|        - | 1756 | ``	/* First pass: classify entries as keyed (`k => v`), positional, or empty`` |
|        - | 1757 | `	 * skip slots ([,]). A list level must be entirely keyed or entirely` |
|        - | 1758 | `	 * positional — PHP fatals on a mix, and on an empty slot inside a keyed` |
|        - | 1759 | `	 * list. */` |
|      195 | 1760 | `	pClassifyIn = pGen->pIn;` |
|      513 | 1761 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|      323 | 1762 | `		if( pGen->pIn >= pNext ){` |
|       13 | 1763 | `			nEmpty++;` |
|      317 | 1764 | `		}else if( GenStateFindTopLevelArrow(pGen->pIn,pNext) < pNext ){` |
|       45 | 1765 | `			nKeyed++;` |
|       24 | 1766 | `		}else{` |
|      269 | 1767 | `			nPositional++;` |
|        - | 1768 | `		}` |
|      323 | 1769 | `		pGen->pIn = &pNext[1];` |
|        5 | 1770 | `	}` |
|      195 | 1771 | `	pGen->pIn = pClassifyIn;` |
|      195 | 1772 | `	if( nKeyed > 0 && nEmpty > 0 ){` |
|      ! 0 | 1773 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - | 1774 | `			"Cannot use empty array entries in keyed array assignment");` |
|      ! 0 | 1775 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 1776 | `	}` |
|      195 | 1777 | `	if( nKeyed > 0 && nPositional > 0 ){` |
|      ! 0 | 1778 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - | 1779 | `			"Cannot mix keyed and unkeyed array entries in assignments");` |
|      ! 0 | 1780 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 1781 | `	}` |
|      195 | 1782 | `	if( nKeyed > 0 ){` |
|       37 | 1783 | `		return GenStateCompileKeyedListBody(pGen);` |
|        - | 1784 | `	}` |
|      161 | 1785 | `	nExpr = 0;` |
|      161 | 1786 | `	SySetInit(&sNested,&pGen->pVm->sAllocator,sizeof(struct NestedListEntry));` |
|      437 | 1787 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|      281 | 1788 | `		if( pGen->pIn < pNext ){` |
|        - | 1789 | `			/* Check for nested list() */` |
|      269 | 1790 | `			if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        3 | 1791 | `				SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|        - | 1792 | `				/* Record this nested list for post-processing */` |
|        3 | 1793 | `				SyToken *pListEnd = 0;` |
|        3 | 1794 | `				if( &pGen->pIn[1] < pNext && (pGen->pIn[1].nType & PH7_TK_LPAREN) ){` |
|        3 | 1795 | `					PH7_DelimitNestedTokens(pGen->pIn+2,pNext,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|        1 | 1796 | `				}` |
|        3 | 1797 | `				if( pListEnd ){` |
|        - | 1798 | `					struct NestedListEntry sEntry;` |
|        3 | 1799 | `					sEntry.nIndex = nExpr;` |
|        3 | 1800 | `					sEntry.pStart = pGen->pIn;` |
|        3 | 1801 | `					sEntry.pEnd = pListEnd + 1;` |
|        3 | 1802 | `					sEntry.isShort = 0;` |
|        3 | 1803 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|        1 | 1804 | `				}` |
|        - | 1805 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|        3 | 1806 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|      268 | 1807 | `			}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|        - | 1808 | `				/* Nested short destructuring [...] */` |
|       16 | 1809 | `				SyToken *pBracketEnd = 0;` |
|       16 | 1810 | `				PH7_DelimitNestedTokens(pGen->pIn+1,pNext,PH7_TK_OSB,PH7_TK_CSB,&pBracketEnd);` |
|       16 | 1811 | `				if( pBracketEnd ){` |
|        - | 1812 | `					struct NestedListEntry sEntry;` |
|       16 | 1813 | `					sEntry.nIndex = nExpr;` |
|       16 | 1814 | `					sEntry.pStart = pGen->pIn;` |
|       16 | 1815 | `					sEntry.pEnd = pBracketEnd + 1;` |
|       16 | 1816 | `					sEntry.isShort = 1;` |
|       16 | 1817 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|        7 | 1818 | `				}` |
|        - | 1819 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|       16 | 1820 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|        9 | 1821 | `			}else{` |
|        - | 1822 | `				/* Compile the expression holding the variable */` |
|      253 | 1823 | `				rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|      253 | 1824 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 1825 | `					SySetRelease(&sNested);` |
|      ! 0 | 1826 | `					return SXRET_OK;` |
|        - | 1827 | `				}` |
|        - | 1828 | `				{` |
|        - | 1829 | `					/* A property target ($o->p / Cls::$s) is a PURE WRITE here — the` |
|        - | 1830 | `					 * value lands via the following OP_LOAD_LIST's direct slot store.` |
|        - | 1831 | `					 * Tag the member so OP_MEMBER skips the uninitialized-typed read` |
|        - | 1832 | `					 * Error / __get consult and vivifies a missing property (php` |
|        - | 1833 | `					 * assigns without reading). */` |
|      253 | 1834 | `					VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|      253 | 1835 | `					if( pLast && pLast->iOp == PH7_OP_MEMBER && pLast->iP2 == PH7_MEMBER_READ ){` |
|       41 | 1836 | `						pLast->iP2 = PH7_MEMBER_LIST_TARGET;` |
|       20 | 1837 | `					}` |
|        - | 1838 | `				}` |
|        - | 1839 | `			}` |
|      137 | 1840 | `		}else{` |
|        - | 1841 | `			/* Empty entry,load NULL */` |
|       13 | 1842 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);` |
|        - | 1843 | `		}` |
|      281 | 1844 | `		nExpr++;` |
|        - | 1845 | `		/* Advance the stream cursor */` |
|      281 | 1846 | `		pGen->pIn = &pNext[1];` |
|        5 | 1847 | `	}` |
|        - | 1848 | `	/* Emit the LOAD_LIST instruction */` |
|      161 | 1849 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,0,0,0);` |
|        - | 1850 | `	/* After LOAD_LIST, the source array is still on the stack top.` |
|        - | 1851 | `	 * For each nested entry, emit code to extract the sub-array` |
|        - | 1852 | `	 * at the corresponding index and recursively destructure it.` |
|        - | 1853 | `	 */` |
|      161 | 1854 | `	if( SySetUsed(&sNested) > 0 ){` |
|       16 | 1855 | `		struct NestedListEntry *apNested = (struct NestedListEntry *)SySetBasePtr(&sNested);` |
|        - | 1856 | `		sxu32 i;` |
|       32 | 1857 | `		for(i = 0; i < SySetUsed(&sNested); i++){` |
|       18 | 1858 | `			SyToken *pSavedIn = pGen->pIn;` |
|       18 | 1859 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|        - | 1860 | `			ph7_value *pIdx;` |
|        - | 1861 | `			sxu32 nConstIdx;` |
|        - | 1862 | `			/* DUP the source array (it's on stack top) */` |
|       18 | 1863 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|        - | 1864 | `			/* Push the integer index for this nested entry */` |
|       18 | 1865 | `			pIdx = PH7_ReserveConstObj(pGen->pVm,&nConstIdx);` |
|       18 | 1866 | `			if( pIdx == 0 ){` |
|      ! 0 | 1867 | `				PH7_GenCompileError(&(*pGen),E_ERROR,0,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1868 | `				SySetRelease(&sNested);` |
|      ! 0 | 1869 | `				return SXERR_ABORT;` |
|        - | 1870 | `			}` |
|       18 | 1871 | `			PH7_MemObjInitFromInt(pGen->pVm,pIdx,(sxi64)apNested[i].nIndex);` |
|       18 | 1872 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nConstIdx,0,0);` |
|        - | 1873 | `			/* LOAD_IDX: pop index, replace DUP'd source with source[index].` |
|        - | 1874 | `			 * iP2=2 signals the VM to emit an "Undefined array key" warning` |
|        - | 1875 | `			 * when the key is missing (PHP-compatible list destructuring).` |
|        - | 1876 | `			 */` |
|       18 | 1877 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,2,0,0);` |
|        - | 1878 | `			/* Recursively compile the inner list */` |
|       18 | 1879 | `			pGen->pIn = apNested[i].pStart;` |
|       18 | 1880 | `			pGen->pEnd = apNested[i].pEnd;` |
|       18 | 1881 | `			if( apNested[i].isShort ){` |
|       16 | 1882 | `				rc = PH7_CompileShortList(&(*pGen),0);` |
|        9 | 1883 | `			}else{` |
|        3 | 1884 | `				rc = PH7_CompileList(&(*pGen),0);` |
|        - | 1885 | `			}` |
|       18 | 1886 | `			pGen->pIn = pSavedIn;` |
|       18 | 1887 | `			pGen->pEnd = pSavedEnd;` |
|       18 | 1888 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1889 | `				SySetRelease(&sNested);` |
|      ! 0 | 1890 | `				return SXERR_ABORT;` |
|        - | 1891 | `			}` |
|        - | 1892 | `			/* Pop the leftover source[index] from the inner LOAD_LIST */` |
|       18 | 1893 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       10 | 1894 | `		}` |
|        7 | 1895 | `	}` |
|      161 | 1896 | `	SySetRelease(&sNested);` |
|        - | 1897 | `	/* Node successfully compiled */` |
|      161 | 1898 | `	return SXRET_OK;` |
|      100 | 1899 | `}` |
|       46 | 1900 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 | 1901 | `{` |
|        - | 1902 | `	/* Jump the 'list' keyword, the leading '(' and exclude trailing ')' */` |
|       51 | 1903 | `	pGen->pIn += 2;` |
|       51 | 1904 | `	pGen->pEnd--;` |
|       23 | 1905 | `	SXUNUSED(iCompileFlag);` |
|       51 | 1906 | `	return GenStateCompileListBody(pGen);` |
|        5 | 1907 | `}` |
|      144 | 1908 | `PH7_PRIVATE sxi32 PH7_CompileShortList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        4 | 1909 | `{` |
|        - | 1910 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|      148 | 1911 | `	pGen->pIn++;` |
|      148 | 1912 | `	pGen->pEnd--;` |
|       72 | 1913 | `	SXUNUSED(iCompileFlag);` |
|      148 | 1914 | `	return GenStateCompileListBody(pGen);` |
|        4 | 1915 | `}` |
|        - | 1916 | `/*` |
|        - | 1917 | ` * assert() source-text rendering.` |
|        - | 1918 | ` *` |
|        - | 1919 | ` * php compiles a DIRECT assert() call with a copy of the argument's AST, and a` |
|        - | 1920 | `` * failing assertion reports zend_ast_export() of that AST — `assert(1 == 2)`,`` |
|        - | 1921 | `` * `assert($x)`, `assert('')` — which is exactly the information the message`` |
|        - | 1922 | ` * exists to carry. PHL has no AST copy at runtime, so the compiler renders the` |
|        - | 1923 | ` * argument's TOKEN SPAN here, at compile time, normalizing to php's export` |
|        - | 1924 | ` * shape (each rule probed against php 8.5):` |
|        - | 1925 | ` *   - literal values fold the way php's AST holds them: numbers render from` |
|        - | 1926 | ` *     their parsed VALUE (0x10 -> 16, 1e3 -> 1000.0, 1_000 -> 1000, an` |
|        - | 1927 | ` *     int64-overflowing literal -> float), strings render single-quoted with` |
|        - | 1928 | ` *     their PROCESSED contents (\ and ' re-escaped), array(...) -> [...].` |
|        - | 1929 | ` *   - one space around binary operators, ", " between arguments/elements, no` |
|        - | 1930 | `` *     space inside ()/[] or around ->/?->/::/casts, `and`/`or` -> `&&`/`\|\|`,`` |
|        - | 1931 | ` *     a trailing comma is dropped, redundant OUTERMOST parens are dropped.` |
|        - | 1932 | ` * Accepted divergences from zend_ast_export on exotic input (message text` |
|        - | 1933 | ` * only, never behavior): redundant INNER parens are kept (php re-derives` |
|        - | 1934 | ` * grouping from precedence), interpolated "$x" strings and heredocs render as` |
|        - | 1935 | `` * written (php exports its interpolation AST), `new C` does not grow php's`` |
|        - | 1936 | `` * trailing `()`, and constant folding beyond single literals is not applied`` |
|        - | 1937 | `` * (php renders `'' . ''` as `''`).`` |
|        - | 1938 | ` */` |
|        - | 1939 | `/* Spacing classes: a space is inserted between two tokens when either side` |
|        - | 1940 | ` * FORCEs one (binary operators, the slot after a comma) or both sides are` |
|        - | 1941 | ` * operand-like (WANT). Grouping punctuation and glue operators contribute` |
|        - | 1942 | ` * NONE on their tight side. */` |
|        - | 1943 | `#define ASRT_SP_NONE  0` |
|        - | 1944 | `#define ASRT_SP_WANT  1` |
|        - | 1945 | `#define ASRT_SP_FORCE 2` |
|        - | 1946 | `enum AssertTokClass {` |
|        - | 1947 | `	ASRT_START = 0, /* virtual class before the first token */` |
|        - | 1948 | `	ASRT_OPERAND,   /* literals, identifiers, keywords */` |
|        - | 1949 | `	ASRT_BINOP,     /* == + . && ? : => instanceof ... */` |
|        - | 1950 | `	ASRT_UNARY,     /* ! ~ @ - + & casts, '$', '...' — glue after */` |
|        - | 1951 | `	ASRT_OPEN,      /* ( [ */` |
|        - | 1952 | `	ASRT_CLOSE,     /* ) ] */` |
|        - | 1953 | `	ASRT_GLUE,      /* -> ?-> :: ++ -- \ — glue both sides */` |
|        - | 1954 | `	ASRT_COMMA      /* , — glue before, force after */` |
|        - | 1955 | `};` |
|        - | 1956 | `static const sxu8 aAsrtBefore[] = { ASRT_SP_NONE, ASRT_SP_WANT, ASRT_SP_FORCE, ASRT_SP_WANT,` |
|        - | 1957 | `	ASRT_SP_NONE, ASRT_SP_NONE, ASRT_SP_NONE, ASRT_SP_NONE };` |
|        - | 1958 | `static const sxu8 aAsrtAfter[]  = { ASRT_SP_NONE, ASRT_SP_WANT, ASRT_SP_FORCE, ASRT_SP_NONE,` |
|        - | 1959 | `	ASRT_SP_NONE, ASRT_SP_WANT, ASRT_SP_NONE, ASRT_SP_FORCE };` |
|        - | 1960 | `/*` |
|        - | 1961 | ` * Append one PROCESSED string-value byte, re-escaped for a single-quoted` |
|        - | 1962 | ` * rendering: php's export escapes only backslash and the quote itself; every` |
|        - | 1963 | ` * other byte (including control characters) is emitted raw.` |
|        - | 1964 | ` */` |
|       42 | 1965 | `static void AssertRenderQuotedByte(SyBlob *pOut,int c)` |
|        2 | 1966 | `{` |
|       44 | 1967 | `	char ch = (char)c;` |
|       44 | 1968 | `	if( c == '\\' \|\| c == '\'' ){` |
|        3 | 1969 | `		SyBlobAppend(pOut,"\\",1);` |
|        1 | 1970 | `	}` |
|       44 | 1971 | `	SyBlobAppend(pOut,&ch,1);` |
|       44 | 1972 | `}` |
|        - | 1973 | `/* Append the UTF-8 encoding of a \u{...} code point (value bytes, re-escaped). */` |
|      ! 0 | 1974 | `static void AssertRenderUtf8(SyBlob *pOut,sxu32 c)` |
|      ! 0 | 1975 | `{` |
|      ! 0 | 1976 | `	if( c < 0x80 ){` |
|      ! 0 | 1977 | `		AssertRenderQuotedByte(pOut,(int)c);` |
|      ! 0 | 1978 | `	}else if( c < 0x800 ){` |
|      ! 0 | 1979 | `		AssertRenderQuotedByte(pOut,(int)(0xc0 \| (c >> 6)));` |
|      ! 0 | 1980 | `		AssertRenderQuotedByte(pOut,(int)(0x80 \| (c & 0x3f)));` |
|      ! 0 | 1981 | `	}else if( c < 0x10000 ){` |
|      ! 0 | 1982 | `		AssertRenderQuotedByte(pOut,(int)(0xe0 \| (c >> 12)));` |
|      ! 0 | 1983 | `		AssertRenderQuotedByte(pOut,(int)(0x80 \| ((c >> 6) & 0x3f)));` |
|      ! 0 | 1984 | `		AssertRenderQuotedByte(pOut,(int)(0x80 \| (c & 0x3f)));` |
|      ! 0 | 1985 | `	}else{` |
|      ! 0 | 1986 | `		AssertRenderQuotedByte(pOut,(int)(0xf0 \| (c >> 18)));` |
|      ! 0 | 1987 | `		AssertRenderQuotedByte(pOut,(int)(0x80 \| ((c >> 12) & 0x3f)));` |
|      ! 0 | 1988 | `		AssertRenderQuotedByte(pOut,(int)(0x80 \| ((c >> 6) & 0x3f)));` |
|      ! 0 | 1989 | `		AssertRenderQuotedByte(pOut,(int)(0x80 \| (c & 0x3f)));` |
|        - | 1990 | `	}` |
|      ! 0 | 1991 | `}` |
|        - | 1992 | `/*` |
|        - | 1993 | ` * Render a single-quoted-source string (or nowdoc body): only \\ and \' are` |
|        - | 1994 | ` * escape sequences there; any other backslash is a literal byte.` |
|        - | 1995 | ` */` |
|        2 | 1996 | `static void AssertRenderSglString(SyBlob *pOut,const char *z,sxu32 n)` |
|        1 | 1997 | `{` |
|        3 | 1998 | `	sxu32 i = 0;` |
|        3 | 1999 | `	SyBlobAppend(pOut,"'",1);` |
|       11 | 2000 | `	while( i < n ){` |
|        9 | 2001 | `		if( z[i] == '\\' && i + 1 < n && (z[i+1] == '\\' \|\| z[i+1] == '\'') ){` |
|        3 | 2002 | `			AssertRenderQuotedByte(pOut,z[i+1]);` |
|        3 | 2003 | `			i += 2;` |
|        2 | 2004 | `		}else{` |
|        7 | 2005 | `			AssertRenderQuotedByte(pOut,z[i]);` |
|        7 | 2006 | `			i++;` |
|        - | 2007 | `		}` |
|        1 | 2008 | `	}` |
|        3 | 2009 | `	SyBlobAppend(pOut,"'",1);` |
|        3 | 2010 | `}` |
|        - | 2011 | `/*` |
|        - | 2012 | ` * Render a double-quoted-source string (or heredoc body) with php's escape` |
|        - | 2013 | ` * processing — the value bytes are what php's AST holds, and the export prints` |
|        - | 2014 | ` * them single-quoted. An UNKNOWN escape keeps the backslash and the character,` |
|        - | 2015 | ` * matching php's string semantics.` |
|        - | 2016 | ` */` |
|       12 | 2017 | `static void AssertRenderDblString(SyBlob *pOut,const char *z,sxu32 n)` |
|        3 | 2018 | `{` |
|       15 | 2019 | `	sxu32 i = 0;` |
|       15 | 2020 | `	SyBlobAppend(pOut,"'",1);` |
|       49 | 2021 | `	while( i < n ){` |
|       36 | 2022 | `		int c = z[i];` |
|        - | 2023 | `		int d;` |
|       36 | 2024 | `		if( c != '\\' \|\| i + 1 >= n ){` |
|       36 | 2025 | `			AssertRenderQuotedByte(pOut,c);` |
|       36 | 2026 | `			i++;` |
|       36 | 2027 | `			continue;` |
|        - | 2028 | `		}` |
|      ! 0 | 2029 | `		d = z[i+1];` |
|      ! 0 | 2030 | `		i += 2;` |
|      ! 0 | 2031 | `		switch(d){` |
|      ! 0 | 2032 | `		case 'n': AssertRenderQuotedByte(pOut,'\n'); break;` |
|      ! 0 | 2033 | `		case 't': AssertRenderQuotedByte(pOut,'\t'); break;` |
|      ! 0 | 2034 | `		case 'r': AssertRenderQuotedByte(pOut,'\r'); break;` |
|      ! 0 | 2035 | `		case 'v': AssertRenderQuotedByte(pOut,'\v'); break;` |
|      ! 0 | 2036 | `		case 'f': AssertRenderQuotedByte(pOut,'\f'); break;` |
|      ! 0 | 2037 | `		case 'e': AssertRenderQuotedByte(pOut,0x1b); break;` |
|      ! 0 | 2038 | `		case '\\': AssertRenderQuotedByte(pOut,'\\'); break;` |
|      ! 0 | 2039 | `		case '"': AssertRenderQuotedByte(pOut,'"'); break;` |
|      ! 0 | 2040 | `		case '$': AssertRenderQuotedByte(pOut,'$'); break;` |
|      ! 0 | 2041 | `		case 'x': case 'X': {` |
|        - | 2042 | `			/* Up to two hex digits; a bare \x is literal. */` |
|      ! 0 | 2043 | `			int nHex = 0, v = 0;` |
|      ! 0 | 2044 | `			while( nHex < 2 && i < n && (unsigned char)z[i] < 0x80 && SyisHex((unsigned char)z[i]) ){` |
|      ! 0 | 2045 | `				v = (v << 4) \| SyHexToint((unsigned char)z[i]);` |
|      ! 0 | 2046 | `				i++; nHex++;` |
|      ! 0 | 2047 | `			}` |
|      ! 0 | 2048 | `			if( nHex > 0 ){` |
|      ! 0 | 2049 | `				AssertRenderQuotedByte(pOut,v);` |
|      ! 0 | 2050 | `			}else{` |
|      ! 0 | 2051 | `				AssertRenderQuotedByte(pOut,'\\');` |
|      ! 0 | 2052 | `				AssertRenderQuotedByte(pOut,d);` |
|        - | 2053 | `			}` |
|      ! 0 | 2054 | `			break;` |
|        - | 2055 | `		}` |
|      ! 0 | 2056 | `		case 'u': {` |
|        - | 2057 | `			/* \u{HEX+} — anything else keeps the backslash (php). */` |
|      ! 0 | 2058 | `			if( i < n && z[i] == '{' ){` |
|      ! 0 | 2059 | `				sxu32 v = 0; sxu32 j = i + 1; int nHex = 0;` |
|      ! 0 | 2060 | `				while( j < n && (unsigned char)z[j] < 0x80 && SyisHex((unsigned char)z[j]) && nHex < 8 ){` |
|      ! 0 | 2061 | `					v = (v << 4) \| (sxu32)SyHexToint((unsigned char)z[j]);` |
|      ! 0 | 2062 | `					j++; nHex++;` |
|      ! 0 | 2063 | `				}` |
|      ! 0 | 2064 | `				if( nHex > 0 && j < n && z[j] == '}' ){` |
|      ! 0 | 2065 | `					AssertRenderUtf8(pOut,v);` |
|      ! 0 | 2066 | `					i = j + 1;` |
|      ! 0 | 2067 | `					break;` |
|        - | 2068 | `				}` |
|      ! 0 | 2069 | `			}` |
|      ! 0 | 2070 | `			AssertRenderQuotedByte(pOut,'\\');` |
|      ! 0 | 2071 | `			AssertRenderQuotedByte(pOut,d);` |
|      ! 0 | 2072 | `			break;` |
|        - | 2073 | `		}` |
|      ! 0 | 2074 | `		default:` |
|      ! 0 | 2075 | `			if( d >= '0' && d <= '7' ){` |
|        - | 2076 | `				/* Up to three octal digits (the first was d). */` |
|      ! 0 | 2077 | `				int nOct = 1, v = d - '0';` |
|      ! 0 | 2078 | `				while( nOct < 3 && i < n && z[i] >= '0' && z[i] <= '7' ){` |
|      ! 0 | 2079 | `					v = (v << 3) \| (z[i] - '0');` |
|      ! 0 | 2080 | `					i++; nOct++;` |
|      ! 0 | 2081 | `				}` |
|      ! 0 | 2082 | `				AssertRenderQuotedByte(pOut,v & 0xff);` |
|      ! 0 | 2083 | `			}else{` |
|      ! 0 | 2084 | `				AssertRenderQuotedByte(pOut,'\\');` |
|      ! 0 | 2085 | `				AssertRenderQuotedByte(pOut,d);` |
|        - | 2086 | `			}` |
|      ! 0 | 2087 | `			break;` |
|        - | 2088 | `		}` |
|      ! 0 | 2089 | `	}` |
|       15 | 2090 | `	SyBlobAppend(pOut,"'",1);` |
|       15 | 2091 | `}` |
|        - | 2092 | `/*` |
|        - | 2093 | ` * Append a double in php's AST-export shape: the shortest round-tripping` |
|        - | 2094 | ` * decimal, with a forced ".0" fraction when the digits alone look integral` |
|        - | 2095 | ` * (1e3 -> "1000.0", 1e20 -> "1.0E+20") — the var_export float shape.` |
|        - | 2096 | ` */` |
|        8 | 2097 | `static void AssertRenderReal(SyBlob *pOut,ph7_real rVal)` |
|        1 | 2098 | `{` |
|        - | 2099 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - | 2100 | `	/* No floating point: ph7_real IS sxi64, there is no shortest-round-trip` |
|        - | 2101 | `	 * decimal to search for and no ".0" to force, so the value renders as the` |
|        - | 2102 | `	 * integer it is -- the same shape the INTEGER arm below emits. Taking` |
|        - | 2103 | `	 * ph7_real rather than double is what keeps the two call sites from` |
|        - | 2104 | `	 * narrowing (MSVC /W4 makes that C4244, and /WX makes it an error). */` |
|        - | 2105 | `	SyBlobFormat(pOut,"%qd",(sxi64)rVal);` |
|        - | 2106 | `#else` |
|        9 | 2107 | `	sxu32 nBefore = SyBlobLength(pOut);` |
|        - | 2108 | `	const char *zOut;` |
|        - | 2109 | `	sxu32 i, nAfter;` |
|        9 | 2110 | `	int bPlain = 1;` |
|        9 | 2111 | `	PH7_AppendShortestReal(pOut,rVal);` |
|        9 | 2112 | `	zOut = (const char *)SyBlobData(pOut);` |
|        9 | 2113 | `	nAfter = SyBlobLength(pOut);` |
|       23 | 2114 | `	for( i = nBefore; i < nAfter; i++ ){` |
|       21 | 2115 | `		if( !((zOut[i] >= '0' && zOut[i] <= '9') \|\| zOut[i] == '-') ){` |
|        7 | 2116 | `			bPlain = 0;` |
|        7 | 2117 | `			break;` |
|        - | 2118 | `		}` |
|        8 | 2119 | `	}` |
|        9 | 2120 | `	if( bPlain ){` |
|        3 | 2121 | `		SyBlobAppend(pOut,".0",2);` |
|        1 | 2122 | `	}` |
|        - | 2123 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|        9 | 2124 | `}` |
|        - | 2125 | `/*` |
|        - | 2126 | ` * Render the token span [pIn, pEnd) — a direct assert() call's first argument —` |
|        - | 2127 | ` * into pOut in php's zend_ast_export shape (see the block comment above).` |
|        - | 2128 | ` * Total: every span renders to SOMETHING (unknown constructs fall back to` |
|        - | 2129 | ` * their raw token text), so the capture never aborts a compile.` |
|        - | 2130 | ` */` |
|       62 | 2131 | `PH7_PRIVATE void PH7_GenRenderAssertSpan(ph7_gen_state *pGen,SyToken *pIn,SyToken *pEnd,SyBlob *pOut)` |
|        5 | 2132 | `{` |
|        - | 2133 | `	sxu8 aParen[64]; /* 1 = this '(' depth is an array(...) literal rendered as [...] */` |
|       67 | 2134 | `	sxu32 nParen = 0;` |
|       67 | 2135 | `	int iPrev = ASRT_START;` |
|       67 | 2136 | ``	int bArrayOpen = 0; /* the next '(' belongs to a suppressed `array` keyword */`` |
|        - | 2137 | `	/* php drops every redundant paren when re-deriving source from the AST;` |
|        - | 2138 | `	 * dropping the OUTERMOST pair(s) is the token-level equivalent for the` |
|        - | 2139 | ``	 * common `assert((...))` spelling. */`` |
|       69 | 2140 | `	while( pIn < pEnd - 1 && (pIn->nType & PH7_TK_LPAREN) && (pEnd[-1].nType & PH7_TK_RPAREN) ){` |
|        - | 2141 | `		SyToken *p;` |
|        3 | 2142 | `		sxi32 iDepth = 0;` |
|        3 | 2143 | `		SyToken *pMatch = 0;` |
|       11 | 2144 | `		for( p = pIn; p < pEnd; p++ ){` |
|       11 | 2145 | `			if( p->nType & PH7_TK_LPAREN ){` |
|        3 | 2146 | `				iDepth++;` |
|       10 | 2147 | `			}else if( p->nType & PH7_TK_RPAREN ){` |
|        3 | 2148 | `				iDepth--;` |
|        3 | 2149 | `				if( iDepth == 0 ){ pMatch = p; break; }` |
|      ! 0 | 2150 | `			}` |
|        5 | 2151 | `		}` |
|        3 | 2152 | `		if( pMatch != &pEnd[-1] ){` |
|      ! 0 | 2153 | `			break;` |
|        - | 2154 | `		}` |
|        3 | 2155 | `		pIn++;` |
|        3 | 2156 | `		pEnd--;` |
|        1 | 2157 | `	}` |
|      267 | 2158 | `	for( ; pIn < pEnd ; pIn++ ){` |
|      205 | 2159 | `		SyToken *pTok = pIn;` |
|      205 | 2160 | `		const char *zTxt = pTok->sData.zString;` |
|      205 | 2161 | `		sxu32 nTxt = pTok->sData.nByte;` |
|        - | 2162 | `		int iCls;` |
|        - | 2163 | `		sxu32 nMark;` |
|        - | 2164 | `		/* --- classify + pre-token handling ------------------------------ */` |
|      205 | 2165 | `		if( pTok->nType & PH7_TK_LPAREN ){` |
|       15 | 2166 | `			iCls = ASRT_OPEN;` |
|      199 | 2167 | `		}else if( pTok->nType & PH7_TK_RPAREN ){` |
|       15 | 2168 | `			iCls = ASRT_CLOSE;` |
|      187 | 2169 | `		}else if( pTok->nType & (PH7_TK_OSB\|PH7_TK_CSB) ){` |
|       13 | 2170 | `			iCls = (pTok->nType & PH7_TK_OSB) ? ASRT_OPEN : ASRT_CLOSE;` |
|      175 | 2171 | `		}else if( pTok->nType & PH7_TK_COMMA ){` |
|        - | 2172 | `			/* php's export never prints a trailing comma. */` |
|        7 | 2173 | `			if( &pIn[1] < pEnd && (pIn[1].nType & (PH7_TK_RPAREN\|PH7_TK_CSB)) ){` |
|      ! 0 | 2174 | `				continue;` |
|        - | 2175 | `			}` |
|        7 | 2176 | `			iCls = ASRT_COMMA;` |
|      166 | 2177 | `		}else if( pTok->nType & PH7_TK_DOLLAR ){` |
|        3 | 2178 | `			iCls = ASRT_UNARY; /* operand-like before, glued to its name after */` |
|      162 | 2179 | `		}else if( pTok->nType & PH7_TK_NSSEP ){` |
|      ! 0 | 2180 | `			iCls = ASRT_GLUE;` |
|      161 | 2181 | `		}else if( pTok->nType & PH7_TK_ELLIPSIS ){` |
|      ! 0 | 2182 | `			iCls = ASRT_UNARY;` |
|      161 | 2183 | `		}else if( pTok->nType & PH7_TK_OP ){` |
|       46 | 2184 | `			iCls = ASRT_BINOP;` |
|       46 | 2185 | `			if( nTxt > 0 ){` |
|       46 | 2186 | `				int c0 = zTxt[0];` |
|       46 | 2187 | `				if( c0 == '(' ){` |
|      ! 0 | 2188 | ``					iCls = ASRT_UNARY; /* lexer-merged cast token `(int)` */`` |
|       60 | 2189 | `				}else if( nTxt == 2 && (SyMemcmp(zTxt,"->",2) == 0 \|\| SyMemcmp(zTxt,"::",2) == 0` |
|       28 | 2190 | `						\|\| SyMemcmp(zTxt,"++",2) == 0 \|\| SyMemcmp(zTxt,"--",2) == 0) ){` |
|      ! 0 | 2191 | `					iCls = ASRT_GLUE;` |
|       46 | 2192 | `				}else if( nTxt == 3 && SyMemcmp(zTxt,"?->",3) == 0 ){` |
|      ! 0 | 2193 | `					iCls = ASRT_GLUE;` |
|       46 | 2194 | `				}else if( nTxt == 1 && (c0 == '!' \|\| c0 == '~' \|\| c0 == '@') ){` |
|        3 | 2195 | `					iCls = ASRT_UNARY;` |
|       45 | 2196 | `				}else if( nTxt == 1 && (c0 == '-' \|\| c0 == '+' \|\| c0 == '&') ){` |
|        - | 2197 | `					/* Unary when nothing operand-like precedes. */` |
|        4 | 2198 | `					if( iPrev == ASRT_START \|\| iPrev == ASRT_BINOP \|\| iPrev == ASRT_UNARY` |
|        3 | 2199 | `					 \|\| iPrev == ASRT_OPEN \|\| iPrev == ASRT_COMMA ){` |
|        3 | 2200 | `						iCls = ASRT_UNARY;` |
|        2 | 2201 | `					}` |
|       42 | 2202 | `				}else if( pTok->nType & PH7_TK_ID ){` |
|        - | 2203 | `					/* Alpha operators: and/or normalize to php's export spelling;` |
|        - | 2204 | `					 * new/clone read as prefix keywords (operand spacing). */` |
|        3 | 2205 | `					if( nTxt == 3 && SyStrnicmp(zTxt,"and",3) == 0 ){` |
|        3 | 2206 | `						zTxt = "&&"; nTxt = 2;` |
|        1 | 2207 | `					}else if( nTxt == 2 && SyStrnicmp(zTxt,"or",2) == 0 ){` |
|      ! 0 | 2208 | `						zTxt = "\|\|"; nTxt = 2;` |
|      ! 0 | 2209 | `					}else if( (nTxt == 3 && SyStrnicmp(zTxt,"new",3) == 0)` |
|      ! 0 | 2210 | `						\|\| (nTxt == 5 && SyStrnicmp(zTxt,"clone",5) == 0) ){` |
|      ! 0 | 2211 | `						iCls = ASRT_OPERAND;` |
|      ! 0 | 2212 | `					}` |
|        1 | 2213 | `				}` |
|       24 | 2214 | `			}` |
|      139 | 2215 | `		}else if( pTok->nType & (PH7_TK_EQUAL\|PH7_TK_ARRAY_OP\|PH7_TK_COLON\|PH7_TK_AMPER) ){` |
|      ! 0 | 2216 | `			iCls = ASRT_BINOP;` |
|      117 | 2217 | `		}else if( pTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|       35 | 2218 | `			iCls = ASRT_OPERAND;` |
|        - | 2219 | ``			/* `array` `(` — php's AST holds one list node for both spellings and`` |
|        - | 2220 | ``			 * always exports `[...]`. Suppress the keyword (it lexes as a KEYWORD`` |
|        - | 2221 | `			 * token, not an ID); the '(' renders '['. */` |
|       30 | 2222 | `			if( nTxt == 5 && SyStrnicmp(zTxt,"array",5) == 0` |
|       17 | 2223 | `			 && &pIn[1] < pEnd && (pIn[1].nType & PH7_TK_LPAREN) ){` |
|        9 | 2224 | `				bArrayOpen = 1;` |
|        9 | 2225 | `				continue;` |
|        - | 2226 | `			}` |
|       16 | 2227 | `		}else{` |
|        - | 2228 | `			/* keywords (true/false/null/fn/match/...), numbers, strings,` |
|        - | 2229 | `			 * member names, '{'/'}' and anything unforeseen */` |
|       86 | 2230 | `			iCls = ASRT_OPERAND;` |
|        - | 2231 | `		}` |
|        - | 2232 | ``		/* Elvis `? :` — php exports the two-token form as `?:`. */`` |
|      194 | 2233 | `		if( iCls == ASRT_BINOP && nTxt == 1 && zTxt[0] == '?'` |
|       11 | 2234 | `		 && &pIn[1] < pEnd && (pIn[1].nType & PH7_TK_COLON) ){` |
|        3 | 2235 | `			nMark = SyBlobLength(pOut);` |
|        3 | 2236 | `			if( iPrev != ASRT_START && nMark > 0 ){` |
|        3 | 2237 | `				SyBlobAppend(pOut," ",1);` |
|        1 | 2238 | `			}` |
|        3 | 2239 | `			SyBlobAppend(pOut,"?:",2);` |
|        3 | 2240 | `			pIn++; /* consume the ':' */` |
|        3 | 2241 | `			iPrev = ASRT_BINOP;` |
|        3 | 2242 | `			continue;` |
|        - | 2243 | `		}` |
|        - | 2244 | `		/* --- spacing ---------------------------------------------------- */` |
|      197 | 2245 | `		if( iPrev != ASRT_START ){` |
|      134 | 2246 | `			int iAfter = aAsrtAfter[iPrev];` |
|      134 | 2247 | `			int iBefore = aAsrtBefore[iCls];` |
|      130 | 2248 | `			if( iAfter == ASRT_SP_FORCE \|\| iBefore == ASRT_SP_FORCE` |
|       69 | 2249 | `			 \|\| (iAfter == ASRT_SP_WANT && iBefore == ASRT_SP_WANT) ){` |
|       86 | 2250 | `				SyBlobAppend(pOut," ",1);` |
|       42 | 2251 | `			}` |
|       65 | 2252 | `		}` |
|        - | 2253 | `		/* --- emit ------------------------------------------------------- */` |
|      197 | 2254 | `		if( pTok->nType & PH7_TK_LPAREN ){` |
|       15 | 2255 | `			if( nParen < sizeof(aParen) ){` |
|       15 | 2256 | `				aParen[nParen] = (sxu8)bArrayOpen;` |
|        6 | 2257 | `			}` |
|       15 | 2258 | `			nParen++;` |
|       15 | 2259 | `			SyBlobAppend(pOut,bArrayOpen ? "[" : "(",1);` |
|       15 | 2260 | `			bArrayOpen = 0;` |
|      191 | 2261 | `		}else if( pTok->nType & PH7_TK_RPAREN ){` |
|       15 | 2262 | `			int bArr = 0;` |
|       15 | 2263 | `			if( nParen > 0 ){` |
|       15 | 2264 | `				nParen--;` |
|       15 | 2265 | `				if( nParen < sizeof(aParen) ){` |
|       15 | 2266 | `					bArr = aParen[nParen];` |
|        6 | 2267 | `				}` |
|        6 | 2268 | `			}` |
|       15 | 2269 | `			SyBlobAppend(pOut,bArr ? "]" : ")",1);` |
|      178 | 2270 | `		}else if( pTok->nType & (PH7_TK_INTEGER\|PH7_TK_REAL) ){` |
|        - | 2271 | `			char zScratch[GEN_NUM_SCRATCH];` |
|       71 | 2272 | `			char *zAlloc = 0;` |
|        - | 2273 | `			SyString sNum;` |
|      102 | 2274 | `			if( GenStateStripNumericSeparators(&pGen->pVm->sAllocator,&pTok->sData,` |
|       71 | 2275 | `					zScratch,sizeof(zScratch),&sNum,&zAlloc) != SXRET_OK ){` |
|      ! 0 | 2276 | `				SyBlobAppend(pOut,zTxt,nTxt); /* alloc failure: raw text */` |
|       71 | 2277 | `			}else if( pTok->nType & PH7_TK_INTEGER ){` |
|       63 | 2278 | `				ph7_real rOverflow = 0;` |
|       63 | 2279 | `				int bDecimalOverflow = 0;` |
|       63 | 2280 | `				if( GenStateIntLiteralOverflows(&sNum,&rOverflow,&bDecimalOverflow) ){` |
|      ! 0 | 2281 | `					if( bDecimalOverflow ){` |
|      ! 0 | 2282 | `						SyStrToReal(sNum.zString,sNum.nByte,(void *)&rOverflow,0);` |
|      ! 0 | 2283 | `					}` |
|      ! 0 | 2284 | `					AssertRenderReal(pOut,rOverflow);` |
|      ! 0 | 2285 | `				}else{` |
|       63 | 2286 | `					SyBlobFormat(pOut,"%qd",PH7_TokenValueToInt64(&sNum));` |
|        - | 2287 | `				}` |
|       33 | 2288 | `			}else{` |
|        9 | 2289 | `				ph7_real rVal = 0;` |
|        9 | 2290 | `				SyStrToReal(sNum.zString,sNum.nByte,(void *)&rVal,0);` |
|        9 | 2291 | `				AssertRenderReal(pOut,rVal);` |
|        - | 2292 | `			}` |
|       71 | 2293 | `			if( zAlloc ){` |
|      ! 0 | 2294 | `				SyMemBackendFree(&pGen->pVm->sAllocator,zAlloc);` |
|        3 | 2295 | `			}` |
|      138 | 2296 | `		}else if( pTok->nType & (PH7_TK_SSTR\|PH7_TK_NOWDOC) ){` |
|        3 | 2297 | `			AssertRenderSglString(pOut,zTxt,nTxt);` |
|      103 | 2298 | `		}else if( pTok->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
|       15 | 2299 | `			if( SyByteFind(zTxt,nTxt,'$',0) == SXRET_OK ){` |
|        - | 2300 | `				/* Interpolated: php exports its interpolation AST in a` |
|        - | 2301 | `				 * double-quoted form; the raw source is the token-level` |
|        - | 2302 | `				 * equivalent. */` |
|      ! 0 | 2303 | `				SyBlobAppend(pOut,"\"",1);` |
|      ! 0 | 2304 | `				SyBlobAppend(pOut,zTxt,nTxt);` |
|      ! 0 | 2305 | `				SyBlobAppend(pOut,"\"",1);` |
|      ! 0 | 2306 | `			}else{` |
|       15 | 2307 | `				AssertRenderDblString(pOut,zTxt,nTxt);` |
|        - | 2308 | `			}` |
|        9 | 2309 | `		}else{` |
|       90 | 2310 | `			SyBlobAppend(pOut,zTxt,nTxt);` |
|        - | 2311 | `		}` |
|      197 | 2312 | `		iPrev = iCls;` |
|      101 | 2313 | `	}` |
|       67 | 2314 | `}` |
|        - | 2315 |  |
