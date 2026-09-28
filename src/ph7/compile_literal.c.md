# src/ph7/compile_literal.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1190/1387 lines (85.80%)

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
|    1794 |   24 | `static int GenStateIsBaseDigit(int c, int base)` |
|       4 |   25 | `{` |
|       - |   26 | `	/* ASCII arithmetic, not <ctype.h>: the byte can be any value in a string` |
|       - |   27 | `	 * literal, and isdigit()/isxdigit() are both locale-dependent and undefined` |
|       - |   28 | `	 * for a negative char. */` |
|    1798 |   29 | `	if( base == 16 ){` |
|     113 |   30 | `		return (c >= '0' && c <= '9') \|\| (c >= 'a' && c <= 'f') \|\| (c >= 'A' && c <= 'F');` |
|       - |   31 | `	}` |
|    1686 |   32 | `	if( base == 8 ){ return c >= '0' && c <= '7'; }` |
|    1680 |   33 | `	if( base == 2 ){ return c == '0' \|\| c == '1'; }` |
|    1394 |   34 | `	return c >= '0' && c <= '9';` |
|     901 |   35 | `}` |
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
|  707350 |   52 | `static int GenStateFindBadNumericSeparator(` |
|       - |   53 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|       5 |   54 | `{` |
|  707355 |   55 | `	const char *z = pRaw->zString;` |
|  707355 |   56 | `	sxu32 n = pRaw->nByte;` |
|  707355 |   57 | `	int base = 10;` |
|       - |   58 | `	sxu32 i, start;` |
|  707355 |   59 | `	if( n < 2 ) return 0;` |
|  193829 |   60 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|     190 |   61 | `		base = 16;` |
|  193736 |   62 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|     287 |   63 | `		base = 2;` |
|     143 |   64 | `	}` |
|  692689 |   65 | `	for( i = 0; i < n; ++i ){` |
|  498875 |   66 | `		if( z[i] != '_' ) continue;` |
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
|  193819 |   83 | `	return 0;` |
|  353680 |   84 | `}` |
|       - |   85 | `/*` |
|       - |   86 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|       - |   87 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|       - |   88 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|       - |   89 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|       - |   90 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|       - |   91 | ` * so callers can bail from the current construct).` |
|       - |   92 | ` */` |
|  707350 |   93 | `PH7_PRIVATE sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|       5 |   94 | `{` |
|  707355 |   95 | `	const char *zBad = 0;` |
|  707355 |   96 | `	sxu32 nBad = 0;` |
|       - |   97 | `	SyString sBad;` |
|       - |   98 | `	sxi32 rc;` |
|  707355 |   99 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|  707345 |  100 | `		return SXRET_OK;` |
|       - |  101 | `	}` |
|      14 |  102 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|      14 |  103 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|       - |  104 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|      14 |  105 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  106 | `		return SXERR_ABORT;` |
|       - |  107 | `	}` |
|      14 |  108 | `	return SXERR_SYNTAX;` |
|  353680 |  109 | `}` |
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
|  707408 |  126 | `PH7_PRIVATE sxi32 GenStateStripNumericSeparators(` |
|       - |  127 | `	SyMemBackend *pAlloc,` |
|       - |  128 | `	const SyString *pToken,` |
|       - |  129 | `	char *zScratch, sxu32 nScratch,` |
|       - |  130 | `	SyString *pOut, char **pzAlloc)` |
|       5 |  131 | `{` |
|       - |  132 | `	sxu32 i, j;` |
|  707413 |  133 | `	int hasUnderscore = 0;` |
|       - |  134 | `	char *zBuf;` |
|  707413 |  135 | `	*pzAlloc = 0;` |
| 1717799 |  136 | `	for( i = 0; i < pToken->nByte; ++i ){` |
| 1010655 |  137 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|  505198 |  138 | `	}` |
|  707413 |  139 | `	if( !hasUnderscore ){` |
|  707149 |  140 | `		SyStringDupPtr(pOut, pToken);` |
|  707149 |  141 | `		return SXRET_OK;` |
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
|  353709 |  158 | `}` |
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
|  693790 |  194 | `static int GenStateIntLiteralOverflows(const SyString *pNum, ph7_real *pReal, int *pbDecimal)` |
|       5 |  195 | `{` |
|  693795 |  196 | `	const char *z = pNum->zString;` |
|  693795 |  197 | `	const char *zEnd = z + pNum->nByte;` |
|       - |  198 | `	const char *p, *q;` |
|       - |  199 | `	int n;` |
|  693795 |  200 | `	*pbDecimal = FALSE;` |
|  693795 |  201 | `	if( z >= zEnd ){` |
|     ! 0 |  202 | `		return FALSE;` |
|       - |  203 | `	}` |
|  693795 |  204 | `	if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|       - |  205 | `		/* Hexadecimal: INT64_MAX == 0x7FFF...F (16 digits, leading nibble 7). */` |
|     192 |  206 | `		p = z + 2;` |
|     208 |  207 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|    1106 |  208 | `		for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisHex(q[0]); q++ ){ n++; }` |
|     192 |  209 | `		if( n < 16 \|\| (n == 16 && SyHexToint(p[0]) < 8) ){` |
|     186 |  210 | `			return FALSE;` |
|       - |  211 | `		}` |
|       7 |  212 | `		{ ph7_real dv = 0;` |
|     103 |  213 | `		  for( q = p; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisHex(q[0]); q++ ){` |
|      97 |  214 | `			dv = dv * 16 + (ph7_real)SyHexToint(q[0]);` |
|      49 |  215 | `		  }` |
|       7 |  216 | `		  *pReal = dv;` |
|       - |  217 | `		}` |
|       7 |  218 | `		return TRUE;` |
|  693607 |  219 | `	}else if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'b' \|\| z[1] == 'B') ){` |
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
|  693321 |  234 | `	}else if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'o' \|\| z[1] == 'O') ){` |
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
|  693301 |  249 | `	}else if( z[0] == '0' ){` |
|       - |  250 | `		/* Octal: INT64_MAX == 0o777...7 (21 significant octal digits). Skip the` |
|       - |  251 | `		 * leading zeros (incl. the base '0'); a non-octal char such as the 8.1` |
|       - |  252 | `		 * "0o" marker ends the run and leaves it to the int path (as today). */` |
|  276665 |  253 | `		p = z;` |
|  553331 |  254 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|  294249 |  255 | `		for( q = p, n = 0; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){ n++; }` |
|  276665 |  256 | `		if( n <= 21 ){` |
|  276663 |  257 | `			return FALSE;` |
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
|  416641 |  270 | `	p = z;` |
|  416641 |  271 | `	while( p < zEnd && p[0] == '0' ){ p++; }` |
| 1086047 |  272 | `	for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisDigit(q[0]); q++ ){ n++; }` |
|  416641 |  273 | `	if( n > 19 \|\| (n == 19 && SyMemcmp(p, "9223372036854775807", 19) > 0) ){` |
|      27 |  274 | `		*pbDecimal = TRUE;` |
|      27 |  275 | `		return TRUE;` |
|       - |  276 | `	}` |
|  416615 |  277 | `	return FALSE;` |
|  346900 |  278 | `}` |
|  707316 |  279 | `PH7_PRIVATE sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  280 | `{` |
|  707321 |  281 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|  707321 |  282 | `	sxu32 nIdx = 0;` |
|       - |  283 | `	char zScratch[GEN_NUM_SCRATCH];` |
|  707321 |  284 | `	char *zAlloc = 0;` |
|       - |  285 | `	SyString sNum;` |
|       - |  286 | `	sxi32 rc;` |
|  353658 |  287 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|  707321 |  288 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|  707321 |  289 | `	if( rc != SXRET_OK ){` |
|       9 |  290 | `		return rc;` |
|       - |  291 | `	}` |
| 1060970 |  292 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|  353655 |  293 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|  707315 |  294 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  295 | `		return SXERR_ABORT;` |
|       - |  296 | `	}` |
|  707315 |  297 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|       - |  298 | `		ph7_value *pObj;` |
|       - |  299 | `		sxi64 iValue;` |
|  693735 |  300 | `		ph7_real rOverflow = 0;` |
|  693735 |  301 | `		int bDecimalOverflow = 0;` |
|  693735 |  302 | `		if( GenStateIntLiteralOverflows(&sNum,&rOverflow,&bDecimalOverflow) ){` |
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
|  693699 |  319 | `			iValue = PH7_TokenValueToInt64(&sNum);` |
|  693699 |  320 | `			pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|  693699 |  321 | `			if( pObj == 0 ){` |
|     ! 0 |  322 | `				if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |  323 | `				return SXERR_ABORT;` |
|       - |  324 | `			}` |
|  693699 |  325 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|       - |  326 | `		}` |
|  346870 |  327 | `	}else{` |
|       - |  328 | `		/* Real number */` |
|       - |  329 | `		ph7_value *pObj;` |
|       - |  330 | `		/* Reserve a new constant */` |
|   13585 |  331 | `		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   13585 |  332 | `		if( pObj == 0 ){` |
|     ! 0 |  333 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  334 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |  335 | `			return SXERR_ABORT;` |
|       - |  336 | `		}` |
|   13585 |  337 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|   13585 |  338 | `		PH7_MemObjToReal(pObj);` |
|       - |  339 | `	}` |
|  707315 |  340 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       - |  341 | `	/* Emit the load constant instruction */` |
|  707315 |  342 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  343 | `	/* Node successfully compiled */` |
|  707315 |  344 | `	return SXRET_OK;` |
|  353663 |  345 | `}` |
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
|  502048 |  357 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  358 | `{` |
|  502053 |  359 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|       - |  360 | `	const char *zIn,*zCur,*zEnd;` |
|       - |  361 | `	ph7_value *pObj;` |
|       - |  362 | `	sxu32 nIdx;` |
|       - |  363 | `	sxi32 bHasEsc;` |
|  502053 |  364 | `	nIdx = 0; /* Prevent compiler warning */` |
|       - |  365 | `	/* Delimit the string */` |
|  502053 |  366 | `	zIn  = pStr->zString;` |
|  502053 |  367 | `	zEnd = &zIn[pStr->nByte];` |
|  502053 |  368 | `	if( zIn >= zEnd ){` |
|       - |  369 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|       - |  370 | `		 * rather than reserving a new object each time. */` |
|   64477 |  371 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|   64477 |  372 | `		return SXRET_OK;` |
|       - |  373 | `	}` |
|       - |  374 | `	/* A single-quoted literal whose raw source holds a backslash unescapes to a` |
|       - |  375 | `	 * value that differs from that source (\\ -> \, \' -> '). The literal cache` |
|       - |  376 | `	 * keys FIND on the raw source text but INSTALL on the unescaped value, so` |
|       - |  377 | `	 * caching such a literal lets an unrelated one collide with it — e.g. '\\'` |
|       - |  378 | `	 * (raw \\, value \) would find the entry installed for '\\\\' (raw \\\\,` |
|       - |  379 | `	 * value \\) and load two backslashes. Only cache literals whose value equals` |
|       - |  380 | `	 * their source, i.e. those with no backslash to unescape. */` |
|  437581 |  381 | `	bHasEsc = 0;` |
|       - |  382 | `	{` |
|       - |  383 | `		const char *zScan;` |
| 7652473 |  384 | `		for( zScan = zIn ; zScan < zEnd ; zScan++ ){` |
| 7216049 |  385 | `			if( zScan[0] == '\\' ){ bHasEsc = 1; break; }` |
| 3607451 |  386 | `		}` |
|       - |  387 | `	}` |
|  437581 |  388 | `	if( !bHasEsc && SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|       - |  389 | `		/* Already processed,emit the load constant instruction` |
|       - |  390 | `		 * and return.` |
|       - |  391 | `		 */` |
|  208891 |  392 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|  208891 |  393 | `		return SXRET_OK;` |
|       - |  394 | `	}` |
|       - |  395 | `	/* Reserve a new constant */` |
|  228695 |  396 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  228695 |  397 | `	if( pObj == 0 ){` |
|     ! 0 |  398 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  399 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  400 | `		return SXERR_ABORT;` |
|       - |  401 | `	}` |
|  228695 |  402 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |  403 | `	/* Compile the node */` |
|  229365 |  404 | `	for(;;){` |
|  458735 |  405 | `		if( zIn >= zEnd ){` |
|       - |  406 | `			/* End of input */` |
|  228695 |  407 | `			break;` |
|       - |  408 | `		}` |
|  230045 |  409 | `		zCur = zIn;` |
| 6770833 |  410 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
| 6540793 |  411 | `			zIn++;` |
|       5 |  412 | `		}` |
|  230045 |  413 | `		if( zIn > zCur ){` |
|       - |  414 | `			/* Append raw contents*/` |
|  229683 |  415 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|  114839 |  416 | `		}` |
|  230045 |  417 | `		zIn++;` |
|  230045 |  418 | `		if( zIn < zEnd ){` |
|    1561 |  419 | `			if( zIn[0] == '\\' ){` |
|       - |  420 | `				/* A literal backslash */` |
|     301 |  421 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|    1413 |  422 | `			}else if( zIn[0] == '\'' ){` |
|       - |  423 | `				/* A single quote */` |
|      39 |  424 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|      21 |  425 | `			}else{` |
|       - |  426 | `				/* verbatim copy */` |
|    1229 |  427 | `				zIn--;` |
|    1229 |  428 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char)*2);` |
|    1229 |  429 | `				zIn++;` |
|       - |  430 | `			}` |
|     778 |  431 | `		}` |
|       - |  432 | `		/* Advance the stream cursor */` |
|  230045 |  433 | `		zIn++;` |
|       5 |  434 | `	}` |
|       - |  435 | `	/* Emit the load constant instruction */` |
|  228695 |  436 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|  228695 |  437 | `	if( !bHasEsc && pStr->nByte < 1024 ){` |
|       - |  438 | `		/* Install in the literal table (only when value == source; see above) */` |
|  227543 |  439 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|  113769 |  440 | `	}` |
|       - |  441 | `	/* Node successfully compiled */` |
|  228695 |  442 | `	return SXRET_OK;` |
|  251029 |  443 | `}` |
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
|     130 |  462 | `static sxi32 GenStateStripHeredocIndent(ph7_gen_state *pGen, SyString *pOut)` |
|       4 |  463 | `{` |
|     134 |  464 | `	SyString *pIn = &pGen->pIn->sData;` |
|     134 |  465 | `	sxu32 nIndent = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - |  466 | `	const char *zPrefix;` |
|       - |  467 | `	const char *z, *zEnd;` |
|       - |  468 | `	char *zBuf, *zDst;` |
|     134 |  469 | `	if( nIndent == 0 ){` |
|       - |  470 | `		/* Legacy column-0 marker: zero-copy fast path */` |
|      88 |  471 | `		*pOut = *pIn;` |
|      88 |  472 | `		return SXRET_OK;` |
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
|      69 |  540 | `}` |
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
|      54 |  555 | `PH7_PRIVATE sxi32 PH7_CompileNowDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       4 |  556 | `{` |
|       - |  557 | `	SyString sStripped;` |
|       - |  558 | `	SyString *pStr;` |
|       - |  559 | `	ph7_value *pObj;` |
|       - |  560 | `	sxu32 nIdx;` |
|       - |  561 | `	sxi32 rc;` |
|      58 |  562 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|      58 |  563 | `	if( rc != SXRET_OK ){` |
|       6 |  564 | `		return rc;` |
|       - |  565 | `	}` |
|      54 |  566 | `	pStr = &sStripped;` |
|      54 |  567 | `	nIdx = 0; /* Prevent compiler warning */` |
|      54 |  568 | `	if( pStr->nByte <= 0 ){` |
|       - |  569 | `		/* An empty nowdoc is the empty STRING, like '' -- loading NULL here made` |
|       - |  570 | `		 * strlen(<<<'EOD'EOD;) deprecation-warn about a null argument. */` |
|       7 |  571 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|       7 |  572 | `		return SXRET_OK;` |
|       - |  573 | `	}` |
|       - |  574 | `	/* Reserve a new constant */` |
|      48 |  575 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      48 |  576 | `	if( pObj == 0 ){` |
|     ! 0 |  577 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  578 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  579 | `		return SXERR_ABORT;` |
|       - |  580 | `	}` |
|       - |  581 | `	/* No processing is done here, simply a memcpy() operation */` |
|      48 |  582 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,pStr);` |
|       - |  583 | `	/* Emit the load constant instruction */` |
|      48 |  584 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  585 | `	/* Node successfully compiled */` |
|      48 |  586 | `	return SXRET_OK;` |
|      31 |  587 | `}` |
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
|    4642 |  610 | `static sxi32 GenStateProcessStringExpression(` |
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
|    4647 |  621 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       - |  622 | `	/* Preallocate some slots */` |
|    4647 |  623 | `	SySetAlloc(&sToken,0x08);` |
|       - |  624 | `	/* Tokenize the text */` |
|    4647 |  625 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken,0);` |
|       - |  626 | `	/* Swap delimiter */` |
|    4647 |  627 | `	pTmpIn  = pGen->pIn;` |
|    4647 |  628 | `	pTmpEnd = pGen->pEnd;` |
|    4647 |  629 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|    4647 |  630 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       - |  631 | ``	/* Compile the expression. An interpolated `"...$x..."` READS $x — php warns`` |
|       - |  632 | `	 * "Undefined variable $x" and substitutes the empty string — so ask for a` |
|       - |  633 | `	 * read-only load rather than letting the default vivify it silently. */` |
|    4647 |  634 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|       - |  635 | `	/* Restore token stream */` |
|    4647 |  636 | `	pGen->pIn  = pTmpIn;` |
|    4647 |  637 | `	pGen->pEnd = pTmpEnd;` |
|       - |  638 | `	/* Release the token set */` |
|    4647 |  639 | `	SySetRelease(&sToken);` |
|       - |  640 | `	/* Compilation result */` |
|    4647 |  641 | `	return rc;` |
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
|    4556 |  675 | `static void GenStateSkipStringLabel(const char **pz,const char *zEnd)` |
|       5 |  676 | `{` |
|    4561 |  677 | `	const char *zIn = *pz;` |
|   13888 |  678 | `	while( zIn < zEnd` |
|   18669 |  679 | `		&& ((unsigned char)zIn[0] >= 0x80 \|\| SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_') ){` |
|   14113 |  680 | `		zIn++;` |
|       5 |  681 | `	}` |
|    4561 |  682 | `	*pz = zIn;` |
|    4561 |  683 | `}` |
|       - |  684 | `/*` |
|       - |  685 | ` * Scan one php INTEGER literal at z in the flavour php's simple-syntax subscript` |
|       - |  686 | ` * accepts: LNUM, HNUM (0x...), BNUM (0b...) or ONUM (0o...), each allowing '_'` |
|       - |  687 | ` * separators BETWEEN digits. There is no float and no exponent in this grammar --` |
|       - |  688 | ` * "$a[1.5]" and "$a[1e2]" are php parse errors. Returns the byte after the` |
|       - |  689 | ` * literal, or z itself when the cursor is not on one.` |
|       - |  690 | ` */` |
|      70 |  691 | `static const char * GenStateScanOffsetNumber(const char *z,const char *zEnd)` |
|       1 |  692 | `{` |
|      71 |  693 | `	const char *zStart = z;` |
|      71 |  694 | `	int base = 10;` |
|      71 |  695 | `	if( z >= zEnd \|\| !GenStateIsBaseDigit((unsigned char)z[0],10) ){` |
|     ! 0 |  696 | `		return z;` |
|       - |  697 | `	}` |
|      71 |  698 | `	if( z[0] == '0' && &z[1] < zEnd ){` |
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
|     353 |  714 | `	while( z < zEnd ){` |
|     297 |  715 | `		if( GenStateIsBaseDigit((unsigned char)z[0],base) ){` |
|     279 |  716 | `			z++;` |
|     279 |  717 | `			continue;` |
|       - |  718 | `		}` |
|      18 |  719 | `		if( z[0] == '_' && z > zStart && GenStateIsBaseDigit((unsigned char)z[-1],base)` |
|       9 |  720 | `			&& &z[1] < zEnd && GenStateIsBaseDigit((unsigned char)z[1],base) ){` |
|       5 |  721 | `			z += 2;` |
|       5 |  722 | `			continue;` |
|       - |  723 | `		}` |
|      15 |  724 | `		break;` |
|     ! 0 |  725 | `	}` |
|      71 |  726 | `	return z;` |
|      36 |  727 | `}` |
|       - |  728 | `/*` |
|       - |  729 | ` * TRUE when the digit run [z,zEnd) is php's CANONICAL spelling of an INTEGER` |
|       - |  730 | ` * offset: "0", or [1-9][0-9]* that fits a signed 64-bit int. php carries every` |
|       - |  731 | ` * other spelling -- leading zeros, a base prefix, '_' separators, a magnitude` |
|       - |  732 | ` * past the int range -- as the raw TEXT, i.e. a STRING key. (zend also spells` |
|       - |  733 | ` * out any 19-digit run, but its hashmap folds that straight back to an integer` |
|       - |  734 | ` * key, so the two agree on everything an array can observe.)` |
|       - |  735 | ` */` |
|      56 |  736 | `static int GenStateOffsetIsCanonicalInt(const char *z,const char *zEnd,int bNeg)` |
|       1 |  737 | `{` |
|      57 |  738 | `	sxu32 n = (sxu32)(zEnd - z);` |
|       - |  739 | `	sxu32 i;` |
|      57 |  740 | `	if( n < 1 ){` |
|     ! 0 |  741 | `		return FALSE;` |
|       - |  742 | `	}` |
|      57 |  743 | `	if( z[0] == '0' ){` |
|       - |  744 | `		/* "0" alone is the integer key 0; "-0", "00" and "007" are text */` |
|      33 |  745 | `		return n == 1 && !bNeg;` |
|       - |  746 | `	}` |
|     231 |  747 | `	for( i = 0 ; i < n ; ++i ){` |
|     209 |  748 | `		if( !GenStateIsBaseDigit((unsigned char)z[i],10) ){` |
|       3 |  749 | `			return FALSE;` |
|       - |  750 | `		}` |
|     104 |  751 | `	}` |
|       - |  752 | `	/* INT64_MAX bounds BOTH signs here, not INT64_MIN: the rewrite re-emits a` |
|       - |  753 | `	 * canonical offset as SOURCE, and no php expression can spell INT64_MIN as a` |
|       - |  754 | `	 * literal (the '-' is unary minus over an out-of-range literal, which` |
|       - |  755 | `	 * promotes to a float). "-9223372036854775808" therefore takes the string` |
|       - |  756 | `	 * path, where the hashmap's numeric-string rule folds it back to the integer` |
|       - |  757 | `	 * key -- and where an ArrayAccess offsetGet() receives php's own string. */` |
|      23 |  758 | `	if( n > 19 \|\| (n == 19 && SyMemcmp(z,"9223372036854775807",19) > 0) ){` |
|       7 |  759 | `		return FALSE;` |
|       - |  760 | `	}` |
|      17 |  761 | `	return TRUE;` |
|      29 |  762 | `}` |
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
|     116 |  805 | `static sxi32 GenStateCompileStringOffset(` |
|       - |  806 | `	ph7_gen_state *pGen,` |
|       - |  807 | `	const char *zKey,` |
|       - |  808 | `	const char *zKeyEnd,` |
|       - |  809 | `	SyBlob *pOut,` |
|       - |  810 | `	int bHeredoc` |
|       - |  811 | `	)` |
|       3 |  812 | `{` |
|     119 |  813 | `	const char *z = zKey;` |
|     119 |  814 | `	int bNeg = 0;` |
|     119 |  815 | `	if( z >= zKeyEnd ){` |
|       3 |  816 | `		return GenStateOffsetSyntaxError(&(*pGen),z,zKeyEnd,0,bHeredoc);` |
|       - |  817 | `	}` |
|     117 |  818 | `	if( z[0] == '$' ){` |
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
|     109 |  834 | `	if( z[0] == '-' ){` |
|      15 |  835 | `		bNeg = 1;` |
|      15 |  836 | `		z++;` |
|       7 |  837 | `	}` |
|     109 |  838 | `	if( z < zKeyEnd && GenStateIsBaseDigit((unsigned char)z[0],10) ){` |
|      71 |  839 | `		const char *zNum = z;` |
|      71 |  840 | `		z = GenStateScanOffsetNumber(z,zKeyEnd);` |
|      71 |  841 | `		if( z != zKeyEnd ){` |
|      15 |  842 | `			return GenStateOffsetSyntaxError(&(*pGen),z,zKeyEnd,1,bHeredoc);` |
|       - |  843 | `		}` |
|       - |  844 | `		/* "-0" is php's string key "-0", not the integer 0: zend negates a LONG` |
|       - |  845 | `		 * num-string but spells a ZERO one back out as text. */` |
|      57 |  846 | `		if( GenStateOffsetIsCanonicalInt(zNum,zKeyEnd,bNeg) ){` |
|      29 |  847 | `			SyBlobAppend(pOut,"[",sizeof(char));` |
|      29 |  848 | `			SyBlobAppend(pOut,zKey,(sxu32)(zKeyEnd - zKey));` |
|      29 |  849 | `			SyBlobAppend(pOut,"]",sizeof(char));` |
|      15 |  850 | `		}else{` |
|      29 |  851 | `			SyBlobAppend(pOut,"['",sizeof(char)*2);` |
|      29 |  852 | `			SyBlobAppend(pOut,zKey,(sxu32)(zKeyEnd - zKey));` |
|      29 |  853 | `			SyBlobAppend(pOut,"']",sizeof(char)*2);` |
|       - |  854 | `		}` |
|      57 |  855 | `		return SXRET_OK;` |
|       - |  856 | `	}` |
|      39 |  857 | `	if( bNeg ){` |
|       - |  858 | `		/* php's '-' takes a NUMBER and nothing else */` |
|       5 |  859 | `		return GenStateOffsetSyntaxError(&(*pGen),z,zKeyEnd,2,bHeredoc);` |
|       - |  860 | `	}` |
|      35 |  861 | `	if( GEN_STRING_LABEL_START(z[0]) ){` |
|      27 |  862 | `		GenStateSkipStringLabel(&z,zKeyEnd);` |
|      27 |  863 | `		if( z != zKeyEnd ){` |
|       3 |  864 | `			return GenStateOffsetSyntaxError(&(*pGen),z,zKeyEnd,1,bHeredoc);` |
|       - |  865 | `		}` |
|       - |  866 | `		/* A bare word is the STRING key, never a constant */` |
|      25 |  867 | `		SyBlobAppend(pOut,"['",sizeof(char)*2);` |
|      25 |  868 | `		SyBlobAppend(pOut,zKey,(sxu32)(zKeyEnd - zKey));` |
|      25 |  869 | `		SyBlobAppend(pOut,"']",sizeof(char)*2);` |
|      25 |  870 | `		return SXRET_OK;` |
|       - |  871 | `	}` |
|       9 |  872 | `	return GenStateOffsetSyntaxError(&(*pGen),z,zKeyEnd,0,bHeredoc);` |
|      61 |  873 | `}` |
|       - |  874 | `/*` |
|       - |  875 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|       - |  876 | ` */` |
|   63452 |  877 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|       5 |  878 | `{` |
|       - |  879 | `	ph7_value *pConstObj;` |
|   63457 |  880 | `	sxu32 nIdx = 0;` |
|       - |  881 | `	/* Reserve a new constant */` |
|   63457 |  882 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   63457 |  883 | `	if( pConstObj == 0 ){` |
|     ! 0 |  884 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  885 | `		return 0;` |
|       - |  886 | `	}` |
|   63457 |  887 | `	(*pCount)++;` |
|   63457 |  888 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|       - |  889 | `	/* Emit the load constant instruction */` |
|   63457 |  890 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   63457 |  891 | `	return pConstObj;` |
|   31731 |  892 | `}` |
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
|   62810 |  938 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen,int bHeredoc)` |
|       5 |  939 | `{` |
|   62815 |  940 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|       - |  941 | `	const char *zIn,*zCur,*zEnd;` |
|   62815 |  942 | `	ph7_value *pObj = 0;` |
|       - |  943 | `	sxi32 iCons;` |
|       - |  944 | `	sxi32 nInterp;   /* how many of iCons came from an interpolated EXPRESSION */` |
|       - |  945 | `	sxi32 rc;` |
|       - |  946 | `	/* Delimit the string */` |
|   62815 |  947 | `	zIn  = pStr->zString;` |
|   62815 |  948 | `	zEnd = &zIn[pStr->nByte];` |
|   62815 |  949 | `	if( zIn >= zEnd ){` |
|       - |  950 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|       - |  951 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|       - |  952 | `		 * literal table from growing when many "" literals appear in the source.` |
|       - |  953 | `		 */` |
|    2063 |  954 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|    2063 |  955 | `		return SXRET_OK;` |
|       - |  956 | `	}` |
|   60757 |  957 | `	zCur = 0;` |
|       - |  958 | `	/* Compile the node */` |
|   60757 |  959 | `	iCons = 0;` |
|   60757 |  960 | `	nInterp = 0;` |
|   32656 |  961 | `	for(;;){` |
|   97885 |  962 | `		zCur = zIn;` |
|  462123 |  963 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  368921 |  964 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|     119 |  965 | `				break;` |
|  368700 |  966 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|    4464 |  967 | `				(GEN_STRING_LABEL_START(zIn[1]) \|\| zIn[1] == '{') ){` |
|    2229 |  968 | `					break;` |
|       - |  969 | `			}` |
|  364243 |  970 | `			zIn++;` |
|       5 |  971 | `		}` |
|   97885 |  972 | `		if( zIn > zCur ){` |
|   46025 |  973 | `			if( pObj == 0 ){` |
|   43775 |  974 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|   43775 |  975 | `				if( pObj == 0 ){` |
|     ! 0 |  976 | `					return SXERR_ABORT;` |
|       - |  977 | `				}` |
|   21885 |  978 | `			}` |
|   46025 |  979 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   23010 |  980 | `		}` |
|   97885 |  981 | `		if( zIn >= zEnd ){` |
|   60719 |  982 | `			break;` |
|       - |  983 | `		}` |
|   37171 |  984 | `		if( zIn[0] == '\\' ){` |
|   32493 |  985 | `			const char *zPtr = 0;` |
|       - |  986 | `			sxu32 n;` |
|   32493 |  987 | `			zIn++;` |
|   32493 |  988 | `			if( pObj == 0 ){` |
|   19687 |  989 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|   19687 |  990 | `				if( pObj == 0 ){` |
|     ! 0 |  991 | `					return SXERR_ABORT;` |
|       - |  992 | `				}` |
|    9841 |  993 | `			}` |
|   32493 |  994 | `			if( zIn >= zEnd ){` |
|       - |  995 | `				/* Lone backslash at the very end of the body: php keeps it */` |
|       3 |  996 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|       3 |  997 | `				break;` |
|       - |  998 | `			}` |
|   32491 |  999 | `			n = sizeof(char); /* size of conversion */` |
|   32491 | 1000 | `			switch( zIn[0] ){` |
|      90 | 1001 | `			case '$':` |
|       - | 1002 | `				/* Dollar sign */` |
|     185 | 1003 | `				PH7_MemObjStringAppend(pObj,"$",sizeof(char));` |
|     185 | 1004 | `				break;` |
|      85 | 1005 | `			case '\\':` |
|       - | 1006 | `				/* A literal backslash */` |
|     175 | 1007 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|     175 | 1008 | `				break;` |
|       1 | 1009 | `			case 'e':` |
|       - | 1010 | `				/* Escape (ESC) ASCII code 27 */` |
|       3 | 1011 | `				PH7_MemObjStringAppend(pObj,"\x1b",sizeof(char));` |
|       3 | 1012 | `				break;` |
|      11 | 1013 | `			case 'f':` |
|       - | 1014 | `				/* Form-feed (FF)[ctrl+l] ASCII code 12 */` |
|      23 | 1015 | `				PH7_MemObjStringAppend(pObj,"\f",sizeof(char));` |
|      23 | 1016 | `				break;` |
|   14126 | 1017 | `			case 'n':` |
|       - | 1018 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|   28257 | 1019 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|   28257 | 1020 | `				break;` |
|     420 | 1021 | `			case 'r':` |
|       - | 1022 | `				/* Carriage return (CR)[ctrl+m] ASCII code 13 */` |
|     845 | 1023 | `				PH7_MemObjStringAppend(pObj,"\r",sizeof(char));` |
|     845 | 1024 | `				break;` |
|      71 | 1025 | `			case 't':` |
|       - | 1026 | `				/* Horizontal tab (HT)[ctrl+i] ASCII code 9 */` |
|     147 | 1027 | `				PH7_MemObjStringAppend(pObj,"\t",sizeof(char));` |
|     147 | 1028 | `				break;` |
|      10 | 1029 | `			case 'v':` |
|       - | 1030 | `				/* Vertical tab(VT)[ctrl+k] ASCII code 11 */` |
|      21 | 1031 | `				PH7_MemObjStringAppend(pObj,"\v",sizeof(char));` |
|      21 | 1032 | `				break;` |
|     272 | 1033 | `			case '"':` |
|     549 | 1034 | `				if( bHeredoc ){` |
|       - | 1035 | `					/* No active quote char in a heredoc: php keeps \" verbatim */` |
|       5 | 1036 | `					PH7_MemObjStringAppend(pObj,"\\\"",sizeof(char)*2);` |
|       3 | 1037 | `				}else{` |
|       - | 1038 | `					/* Double quote */` |
|     545 | 1039 | `					PH7_MemObjStringAppend(pObj,"\"",sizeof(char));` |
|       - | 1040 | `				}` |
|     549 | 1041 | `				break;` |
|     198 | 1042 | `			case '0': case '1': case '2': case '3':` |
|       - | 1043 | `			case '4': case '5': case '6': case '7': {` |
|       - | 1044 | `				/* \[0-7]{1,3}: a character in octal notation. A value above \377` |
|       - | 1045 | `				 * warns and wraps to the low byte, matching php 8. */` |
|     401 | 1046 | `				int c = 0;` |
|       - | 1047 | `				char cOut;` |
|     853 | 1048 | `				for( zPtr = zIn ; zPtr < &zIn[3*sizeof(char)] ; zPtr++ ){` |
|     831 | 1049 | `					if( zPtr >= zEnd \|\| zPtr[0] < '0' \|\| zPtr[0] > '7' ){` |
|     192 | 1050 | `						break;` |
|       - | 1051 | `					}` |
|     457 | 1052 | `					c = c * 8 + (zPtr[0] - '0');` |
|     231 | 1053 | `				}` |
|     401 | 1054 | `				if( c > 0xFF ){` |
|       - | 1055 | `					SyString sSeq;` |
|       3 | 1056 | `					SyStringInitFromBuf(&sSeq,zIn,(sxu32)(zPtr-zIn));` |
|       3 | 1057 | `					PH7_GenCompileError(&(*pGen),E_WARNING,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|       - | 1058 | `						"Octal escape sequence overflow \\%z is greater than \\377",&sSeq);` |
|       3 | 1059 | `					c &= 0xFF;` |
|       1 | 1060 | `				}` |
|     401 | 1061 | `				cOut = (char)c; /* value byte, independent of host endianness */` |
|     401 | 1062 | `				PH7_MemObjStringAppend(pObj,&cOut,sizeof(char));` |
|     401 | 1063 | `				n = (sxu32)(zPtr-zIn);` |
|     401 | 1064 | `				break;` |
|       - | 1065 | `			}` |
|     839 | 1066 | `			case 'x':` |
|    2520 | 1067 | `				if( &zIn[1] < zEnd && SyisHex((unsigned char)zIn[1]) ){` |
|       - | 1068 | `					/* \x[0-9A-Fa-f]{1,2}: a character in hexadecimal notation */` |
|    1679 | 1069 | `					int c = SyHexToint(zIn[1]);` |
|       - | 1070 | `					char cOut;` |
|    1679 | 1071 | `					n += sizeof(char);` |
|    1679 | 1072 | `					if( &zIn[2] < zEnd && SyisHex((unsigned char)zIn[2]) ){` |
|    1675 | 1073 | `						c = (c << 4) + SyHexToint(zIn[2]);` |
|    1675 | 1074 | `						n += sizeof(char);` |
|     835 | 1075 | `					}` |
|    1679 | 1076 | `					cOut = (char)c; /* value byte, independent of host endianness */` |
|    1679 | 1077 | `					PH7_MemObjStringAppend(pObj,&cOut,sizeof(char));` |
|     842 | 1078 | `				}else{` |
|       - | 1079 | `					/* Not an escape: keep the backslash, as php does */` |
|       5 | 1080 | `					PH7_MemObjStringAppend(pObj,"\\x",sizeof(char)*2);` |
|       - | 1081 | `				}` |
|    1683 | 1082 | `				break;` |
|     105 | 1083 | `			case 'u':` |
|     210 | 1084 | `				if( &zIn[1] < zEnd && zIn[1] == '{'` |
|     310 | 1085 | `				 && !(&zIn[2] < zEnd && zIn[2] == '$') ){` |
|       - | 1086 | `					/* \u{codepoint}: UTF-8 encoding of the given codepoint (php 7+).` |
|       - | 1087 | `					 * php encodes surrogates verbatim, so the only invalid value` |
|       - | 1088 | `					 * is > U+10FFFF; malformed/empty braces are a compile error.` |
|       - | 1089 | `					 * "\u{$..." is excluded above: php treats it as a literal \u` |
|       - | 1090 | `					 * followed by {$...} curly interpolation. */` |
|     207 | 1091 | `					sxu32 nCp = 0;` |
|     207 | 1092 | `					zPtr = &zIn[2];` |
|     833 | 1093 | `					while( zPtr < zEnd && SyisHex((unsigned char)zPtr[0]) ){` |
|     629 | 1094 | `						if( nCp <= 0x10FFFF ){` |
|       - | 1095 | `							/* stop accumulating once out of range: keeps a long` |
|       - | 1096 | `							 * digit run from wrapping sxu32 */` |
|     629 | 1097 | `							nCp = nCp * 16 + (sxu32)SyHexToint(zPtr[0]);` |
|     313 | 1098 | `						}` |
|     629 | 1099 | `						zPtr++;` |
|       3 | 1100 | `					}` |
|     207 | 1101 | `					if( zPtr == &zIn[2] \|\| zPtr >= zEnd \|\| zPtr[0] != '}' ){` |
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
|     205 | 1115 | `					n = (sxu32)(&zPtr[1]-zIn); /* 'u{...}' incl. closing brace */` |
|     205 | 1116 | `					if( nCp > 0x10FFFF ){` |
|       3 | 1117 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|       - | 1118 | `							"Invalid UTF-8 codepoint escape sequence: Codepoint too large");` |
|       3 | 1119 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 1120 | `							return SXERR_ABORT;` |
|       - | 1121 | `						}` |
|       3 | 1122 | `						break;` |
|       - | 1123 | `					}` |
|       - | 1124 | `					{` |
|       - | 1125 | `						char zUtf[4];` |
|     202 | 1126 | `						sxu8 *zOut = (sxu8 *)zUtf;` |
|     202 | 1127 | `						SX_WRITE_UTF8(zOut,nCp);` |
|     202 | 1128 | `						PH7_MemObjStringAppend(pObj,zUtf,(sxu32)(zOut-(sxu8 *)zUtf));` |
|       - | 1129 | `					}` |
|     102 | 1130 | `				}else{` |
|       - | 1131 | `					/* Not an escape: keep the backslash, as php does */` |
|       7 | 1132 | `					PH7_MemObjStringAppend(pObj,"\\u",sizeof(char)*2);` |
|       - | 1133 | `				}` |
|     208 | 1134 | `				break;` |
|      15 | 1135 | `			default:` |
|       - | 1136 | `				/* Unrecognized escape: keep the backslash, as php does.` |
|       - | 1137 | `				 * zIn[-1] is the backslash itself, so both bytes are contiguous` |
|       - | 1138 | `				 * in the source buffer — one batched append. */` |
|      31 | 1139 | `				PH7_MemObjStringAppend(pObj,&zIn[-1],sizeof(char)*2);` |
|      30 | 1140 | `				break;` |
|       - | 1141 | `			}` |
|       - | 1142 | `			/* Advance the stream cursor */` |
|   32491 | 1143 | `			zIn += n;` |
|   32491 | 1144 | `			continue;` |
|       - | 1145 | `		}` |
|    4683 | 1146 | `		if( zIn[0] == '{' ){` |
|       - | 1147 | `			/* Curly syntax */` |
|       - | 1148 | `			const char *zExpr;` |
|     234 | 1149 | `			sxi32 iNest = 1;` |
|     234 | 1150 | `			zIn++;` |
|     234 | 1151 | `			zExpr = zIn;` |
|       - | 1152 | `			/* Synchronize with the next closing curly braces */` |
|    2170 | 1153 | `			while( zIn < zEnd ){` |
|    2170 | 1154 | `				if( zIn[0] == '{' ){` |
|       - | 1155 | `					/* Increment nesting level */` |
|       3 | 1156 | `					iNest++;` |
|    2169 | 1157 | `				}else if(zIn[0] == '}' ){` |
|       - | 1158 | `					/* Decrement nesting level */` |
|     236 | 1159 | `					iNest--;` |
|     236 | 1160 | `					if( iNest <= 0 ){` |
|     234 | 1161 | `						break;` |
|       - | 1162 | `					}` |
|       1 | 1163 | `				}` |
|    1940 | 1164 | `				zIn++;` |
|       4 | 1165 | `			}` |
|       - | 1166 | `			/* Process the expression */` |
|     234 | 1167 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|     234 | 1168 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1169 | `				return SXERR_ABORT;` |
|       - | 1170 | `			}` |
|     234 | 1171 | `			if( rc != SXERR_EMPTY ){` |
|     234 | 1172 | `				++iCons;` |
|     234 | 1173 | `				++nInterp;` |
|     115 | 1174 | `			}` |
|     234 | 1175 | `			if( zIn < zEnd ){` |
|       - | 1176 | `				/* Jump the trailing curly */` |
|     234 | 1177 | `				zIn++;` |
|     115 | 1178 | `			}` |
|     119 | 1179 | `		}else{` |
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
|    4453 | 1194 | `			const char *zExpr = zIn;` |
|    4453 | 1195 | `			int bSubscript = 0;` |
|       - | 1196 | `			/*` |
|       - | 1197 | `			 * "${...}" string interpolation (every form: ${name}, ${expr}, ${$x}) was` |
|       - | 1198 | `			 * DEPRECATED by php 8.2 in favor of the canonical "{$...}". PHL targets php's` |
|       - | 1199 | `			 * *non-deprecated* surface, so it is a hard parse error here — never silently` |
|       - | 1200 | `			 * rewritten. The canonical "{$var}" reaches this compiler by a different path` |
|       - | 1201 | `			 * and is unaffected. Checked before the scan: '{' is not an accessor, so the` |
|       - | 1202 | `			 * cursor would otherwise stop on the '$' and read the brace as literal text.` |
|       - | 1203 | `			 */` |
|    4453 | 1204 | `			if( &zIn[1] < zEnd && zIn[0] == '$' && zIn[1] == '{' ){` |
|       3 | 1205 | `				PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - | 1206 | `					"syntax error, \"${\" string interpolation was removed in php 8.2, use \"{$...}\" instead");` |
|       3 | 1207 | `				return SXERR_ABORT;` |
|       - | 1208 | `			}` |
|       - | 1209 | `			/* Jump leading dollars */` |
|    8897 | 1210 | `			while( zIn < zEnd && zIn[0] == '$' ){` |
|    4451 | 1211 | `				zIn++;` |
|       5 | 1212 | `			}` |
|       - | 1213 | `			/* Variable name */` |
|    4451 | 1214 | `			GenStateSkipStringLabel(&zIn,zEnd);` |
|       - | 1215 | `			/* …then at most ONE accessor */` |
|    4509 | 1216 | `			if( zIn < zEnd && zIn[0] == '[' ){` |
|     119 | 1217 | `				sxi32 iSquare = 1;` |
|     119 | 1218 | `				bSubscript = 1;` |
|     119 | 1219 | `				zIn++;` |
|     577 | 1220 | `				while( zIn < zEnd ){` |
|     575 | 1221 | `					if( zIn[0] == '[' ){` |
|       3 | 1222 | `						iSquare++;` |
|     574 | 1223 | `					}else if (zIn[0] == ']' ){` |
|     119 | 1224 | `						iSquare--;` |
|     119 | 1225 | `						if( iSquare <= 0 ){` |
|     117 | 1226 | `							break;` |
|       - | 1227 | `						}` |
|       1 | 1228 | `					}` |
|     461 | 1229 | `					zIn++;` |
|       3 | 1230 | `				}` |
|     119 | 1231 | `				if( zIn < zEnd ){` |
|     117 | 1232 | `					zIn++;` |
|      57 | 1233 | `				}` |
|    4391 | 1234 | `			}else if( &zIn[2] < zEnd && zIn[0] == '-' && zIn[1] == '>'` |
|     115 | 1235 | `				&& GEN_STRING_LABEL_START(zIn[2]) ){` |
|       - | 1236 | `				/* Member access operator '->'. php takes it only when a LABEL` |
|       - | 1237 | `				 * follows; with anything else -- a digit, a space, a '{', the end` |
|       - | 1238 | `				 * of the body -- the arrow is literal TEXT and the interpolation is` |
|       - | 1239 | `				 * just the variable. PHL swallowed the bare '->' and handed the` |
|       - | 1240 | `				 * compiler a dangling "$o->", fatalling` |
|       - | 1241 | `				 * "'->': Missing/Invalid member name" on source php RUNS. */` |
|      83 | 1242 | `				zIn += 2;` |
|      83 | 1243 | `				GenStateSkipStringLabel(&zIn,zEnd);` |
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
|    4451 | 1256 | `			if( bSubscript ){` |
|     119 | 1257 | `				const char *zBr = zExpr;` |
|       - | 1258 | `				SyBlob sSub;` |
|     381 | 1259 | `				while( zBr < zIn && zBr[0] != '[' ){` |
|     265 | 1260 | `					zBr++;` |
|       3 | 1261 | `				}` |
|     119 | 1262 | `				if( zIn <= zBr \|\| zIn[-1] != ']' ){` |
|       - | 1263 | `					/* Unterminated: the body ended inside the brackets. php names the` |
|       - | 1264 | `					  * closing quote it reached instead; there is no offending TOKEN to` |
|       - | 1265 | `					  * quote here, and zIn is one past the body, so never read it. */` |
|     ! 0 | 1266 | `					PH7_GenCompileError(&(*pGen),E_PARSE,GenStateStringEscLine(&(*pGen),zBr,bHeredoc),` |
|       - | 1267 | `						"syntax error, unexpected end of string, expecting \"-\" or identifier or variable or number");` |
|     ! 0 | 1268 | `					return SXERR_ABORT;` |
|       - | 1269 | `				}` |
|     119 | 1270 | `				SyBlobInit(&sSub,&pGen->pVm->sAllocator);` |
|     119 | 1271 | `				SyBlobAppend(&sSub,zExpr,(sxu32)(zBr - zExpr));` |
|     119 | 1272 | `				rc = GenStateCompileStringOffset(&(*pGen),&zBr[1],&zIn[-1],&sSub,bHeredoc);` |
|     119 | 1273 | `				if( rc != SXRET_OK ){` |
|      35 | 1274 | `					SyBlobRelease(&sSub);` |
|      35 | 1275 | `					return SXERR_ABORT;` |
|       - | 1276 | `				}` |
|     126 | 1277 | `				rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,` |
|      82 | 1278 | `					(const char *)SyBlobData(&sSub),` |
|      82 | 1279 | `					(const char *)SyBlobData(&sSub) + SyBlobLength(&sSub));` |
|      85 | 1280 | `				SyBlobRelease(&sSub);` |
|      85 | 1281 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1282 | `					return SXERR_ABORT;` |
|       - | 1283 | `				}` |
|      85 | 1284 | `				if( rc != SXERR_EMPTY ){` |
|      85 | 1285 | `					++iCons;` |
|      85 | 1286 | `					++nInterp;` |
|      41 | 1287 | `				}` |
|      85 | 1288 | `				pObj = 0;` |
|      85 | 1289 | `				continue;` |
|       - | 1290 | `			}` |
|       - | 1291 | `			/* Process the expression */` |
|    4335 | 1292 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|    4335 | 1293 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1294 | `				return SXERR_ABORT;` |
|       - | 1295 | `			}` |
|    4335 | 1296 | `			if( rc != SXERR_EMPTY ){` |
|    4335 | 1297 | `				++iCons;` |
|    4335 | 1298 | `				++nInterp;` |
|    2165 | 1299 | `			}` |
|       - | 1300 | `		}` |
|       - | 1301 | `		/* Invalidate the previously used constant */` |
|    4565 | 1302 | `		pObj = 0;` |
|       5 | 1303 | `	}/*for(;;)*/` |
|   60721 | 1304 | `	if( iCons > 1 ){` |
|       - | 1305 | `		/* Concatenate all compiled constants */` |
|    3463 | 1306 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|   58992 | 1307 | `	}else if( iCons == 1 && nInterp == 1 ){` |
|       - | 1308 | `		/* A string that is nothing but one interpolation ("$x") still has to` |
|       - | 1309 | `		 * PRODUCE A STRING. With no CAT to force the conversion the operand was` |
|       - | 1310 | ``		 * left on the stack untouched, so `$s = "$x"` handed back $x's own type:`` |
|       - | 1311 | `		 * "$arr" stayed an array (and skipped php's "Array to string conversion"` |
|       - | 1312 | `		 * warning), "$int" stayed an int, "$res" stayed a resource. */` |
|      87 | 1313 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CVT_STR,0,0,0,0);` |
|      42 | 1314 | `	}` |
|       - | 1315 | `	/* Node successfully compiled */` |
|   60721 | 1316 | `	return SXRET_OK;` |
|   31410 | 1317 | `}` |
|       - | 1318 | `/*` |
|       - | 1319 | ` * Compile a double quoted string.` |
|       - | 1320 | ` *  See the block-comment above for more information.` |
|       - | 1321 | ` */` |
|   62738 | 1322 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 | 1323 | `{` |
|       - | 1324 | `	sxi32 rc;` |
|   62743 | 1325 | `	rc = GenStateCompileString(&(*pGen),0/*bHeredoc*/);` |
|   31369 | 1326 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - | 1327 | `	/* Compilation result */` |
|   62743 | 1328 | `	return rc;` |
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
|  131408 | 1372 | `PH7_PRIVATE sxi32 GenStateCompileArrayEntry(` |
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
|  131413 | 1383 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|       - | 1384 | `	/* Compile the expression*/` |
|  131413 | 1385 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|       - | 1386 | `	/* Restore token stream */` |
|  131413 | 1387 | `	RE_SWAP_DELIMITER(pGen);` |
|  131413 | 1388 | `	return rc;` |
|       5 | 1389 | `}` |
|       - | 1390 | `/*` |
|       - | 1391 | ` * Expression tree validator callback for the 'array' language construct.` |
|       - | 1392 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|       - | 1393 | ` * an invalid expression tree and this function will generate the appropriate` |
|       - | 1394 | ` * error message.` |
|       - | 1395 | ` * See the routine responible of compiling the array language construct` |
|       - | 1396 | ` * for more inforation.` |
|       - | 1397 | ` */` |
|      82 | 1398 | `static sxi32 GenStateArrayNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       4 | 1399 | `{` |
|       - | 1400 | ``	/* `array(&$x)` is a full write target in php, call included: `[&f()]` is its`` |
|       - | 1401 | `	 * "Can't use function return value in write context", not the` |
|       - | 1402 | ``	 * reference-returning-function exemption `$r =& f()` gets. A nullsafe chain`` |
|       - | 1403 | `	 * here takes the WRITE wording too, not the reference one — php never asks` |
|       - | 1404 | ``	 * `zend_assert_not_short_circuited` on an array entry. */`` |
|       - | 1405 | `	sxi32 rc;` |
|      82 | 1406 | `	if( pRoot && pRoot->pOp && pRoot->pOp->iOp == EXPR_OP_SUBSCRIPT` |
|      17 | 1407 | `	 && SySetUsed(&pRoot->aNodeArgs) < 1 ){` |
|       - | 1408 | ``		/* `[&$a[]]`: an APPEND has nothing to take a reference OF, and php asks`` |
|       - | 1409 | ``		 * that before anything else about the base — so `[&f()[]]` is this and not`` |
|       - | 1410 | `		 * the call refusal. PHL ran the whole thing. */` |
|     ! 0 | 1411 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,` |
|     ! 0 | 1412 | `			pRoot->pStart ? pRoot->pStart->nLine : 0,"Cannot use [] for reading");` |
|     ! 0 | 1413 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - | 1414 | `	}` |
|      86 | 1415 | `	rc = GenStateWriteTargetCheck(&(*pGen),pRoot,0);` |
|      86 | 1416 | `	if( rc != SXRET_OK ){` |
|       3 | 1417 | `		return rc;` |
|       - | 1418 | `	}` |
|      84 | 1419 | `	if( pRoot->pOp ){` |
|      18 | 1420 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ &&` |
|      12 | 1421 | `			pRoot->pOp->iOp != EXPR_OP_FUNC_CALL /* function() [Symisc extension: i.e: array(&foo())] */` |
|      16 | 1422 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|       - | 1423 | `			/* Unexpected expression */` |
|      12 | 1424 | `			rc = PH7_GenSyntaxError(&(*pGen),pRoot->pStart,"\"->\" or \"?->\" or \"[\"");` |
|      12 | 1425 | `			if( rc != SXERR_ABORT ){` |
|      12 | 1426 | `				rc = SXERR_INVALID;` |
|       5 | 1427 | `			}` |
|       9 | 1428 | `		}` |
|      73 | 1429 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 1430 | `		/* Unexpected expression */` |
|       3 | 1431 | `		rc = PH7_GenSyntaxError(&(*pGen),pRoot->pStart,0);` |
|       3 | 1432 | `		if( rc != SXERR_ABORT ){` |
|       3 | 1433 | `			rc = SXERR_INVALID;` |
|       1 | 1434 | `		}` |
|       1 | 1435 | `	}` |
|      84 | 1436 | `	return rc;` |
|      45 | 1437 | `}` |
|       - | 1438 | `/*` |
|       - | 1439 | ` * Find the top-level '=>' (PH7_TK_ARRAY_OP) that separates an array/list entry's` |
|       - | 1440 | ` * key from its value within [pStart,pEnd). The scan skips any '=>' nested inside` |
|       - | 1441 | ` * brackets/parens/braces, inside an arrow-function signature (fn(...) =>), or` |
|       - | 1442 | ` * inside a match() {...} arm — none of which are key/value separators. Returns a` |
|       - | 1443 | ` * pointer to the '=>' token, or pEnd if the entry has no top-level separator.` |
|       - | 1444 | ` */` |
|  207414 | 1445 | `PH7_PRIVATE SyToken * GenStateFindTopLevelArrow(SyToken *pStart,SyToken *pEnd)` |
|       5 | 1446 | `{` |
|  207419 | 1447 | `	SyToken *pCur = pStart;` |
|  207419 | 1448 | `	sxi32 iNest = 0;` |
|       - | 1449 | ``	/* An entry that STARTS with a bare `yield` owns the FIRST '=>' after it:`` |
|       - | 1450 | `` 	 * php's grammar gives `yield expr => expr` to the yield, so `[yield 1 => 2]` `` |
|       - | 1451 | `	 * is ONE element (the yield's own result) keyed by nothing, and the generator` |
|       - | 1452 | `	 * yields key 1 value 2. Reading that '=>' as the entry separator instead built` |
|       - | 1453 | ``	 * `[(yield 1) => 2]` — a different array AND a different yielded pair, in`` |
|       - | 1454 | `	 * silence. A SECOND top-level '=>' is the entry separator again, which is what` |
|       - | 1455 | ``	 * makes `[yield 1 => 2 => 3]` parse. `yield from` takes an iterable and never a`` |
|       - | 1456 | `	 * pair, so its entry keeps the ordinary rule. */` |
|  207593 | 1457 | `	int bYieldOwnsArrow = (pStart < pEnd) && (pStart->nType & PH7_TK_KEYWORD)` |
|  103881 | 1458 | `		&& (sxu32)SX_PTR_TO_INT(pStart->pUserData) == PH7_TKWRD_YIELD` |
|  311123 | 1459 | `		&& !(&pStart[1] < pEnd && (pStart[1].nType & PH7_TK_ID)` |
|       8 | 1460 | `			&& pStart[1].sData.nByte == 4` |
|       2 | 1461 | `			&& SyStrnicmp(pStart[1].sData.zString, "from", 4) == 0);` |
|  527881 | 1462 | `	while( pCur < pEnd ){` |
|  358275 | 1463 | `		if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|   37633 | 1464 | `			if( bYieldOwnsArrow ){` |
|       9 | 1465 | `				bYieldOwnsArrow = 0;` |
|       9 | 1466 | `				pCur++;` |
|       9 | 1467 | `				continue;` |
|       - | 1468 | `			}` |
|   37625 | 1469 | `			return pCur;` |
|       - | 1470 | `		}` |
|       - | 1471 | `		/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|       - | 1472 | `		 * The '=>' inside an arrow function introduces the expression body,` |
|       - | 1473 | `		 * not an entry separator. Skip past the signature.` |
|       - | 1474 | `		 */` |
|  320647 | 1475 | `		if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
|     477 | 1476 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|     477 | 1477 | `			SyToken *pFn = pCur;` |
|       - | 1478 | ``			/* Only a real `[static] fn[&](` opens an arrow function; `$fn`,`` |
|       - | 1479 | ``			 * `C::fn` and friends are plain names whose '=>' IS the separator. */`` |
|     477 | 1480 | `			if( PH7_TokenOpensArrowFunc(pStart,pCur,pEnd) ){` |
|     191 | 1481 | `				if( nKw == PH7_TKWRD_STATIC ){` |
|     ! 0 | 1482 | `					pFn = &pCur[1];` |
|     ! 0 | 1483 | `				}` |
|     191 | 1484 | `				pCur = pFn + 1; /* past 'fn' */` |
|     191 | 1485 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_AMPER) ){` |
|     ! 0 | 1486 | `					pCur++;` |
|     ! 0 | 1487 | `				}` |
|     191 | 1488 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|     191 | 1489 | `					pCur++;` |
|     191 | 1490 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|       - | 1491 | `						PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|     191 | 1492 | `					if( pCur < pEnd ){` |
|     191 | 1493 | `						pCur++;` |
|      94 | 1494 | `					}` |
|      94 | 1495 | `				}` |
|     191 | 1496 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_COLON) ){` |
|     ! 0 | 1497 | `					pCur++;` |
|     ! 0 | 1498 | `					if( pCur < pEnd && (pCur->nType & PH7_TK_OP)` |
|     ! 0 | 1499 | `						&& pCur->sData.nByte == 1` |
|     ! 0 | 1500 | `						&& pCur->sData.zString[0] == '?' ){` |
|     ! 0 | 1501 | `						pCur++;` |
|     ! 0 | 1502 | `					}` |
|     ! 0 | 1503 | `					if( pCur < pEnd` |
|     ! 0 | 1504 | `						&& (pCur->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|     ! 0 | 1505 | `						pCur++;` |
|     ! 0 | 1506 | `					}` |
|     ! 0 | 1507 | `				}` |
|       - | 1508 | `				/* The rest of the entry is the arrow-function body — no outer` |
|       - | 1509 | `				 * key to extract. */` |
|     191 | 1510 | `				return pEnd;` |
|       - | 1511 | `			}` |
|       - | 1512 | `			/* Match expression (PHP 8.0): the '=>' inside match arms is not an` |
|       - | 1513 | `			 * entry separator. Skip past the full match span. */` |
|     288 | 1514 | `			if( nKw == PH7_TKWRD_MATCH ){` |
|      12 | 1515 | `				pCur++; /* past 'match' */` |
|      12 | 1516 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|      10 | 1517 | `					pCur++;` |
|      10 | 1518 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|       - | 1519 | `						PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|      10 | 1520 | `					if( pCur < pEnd ){` |
|      10 | 1521 | `						pCur++;` |
|       4 | 1522 | `					}` |
|       4 | 1523 | `				}` |
|      12 | 1524 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_OCB) ){` |
|      10 | 1525 | `					pCur++;` |
|      10 | 1526 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|       - | 1527 | `						PH7_TK_OCB,PH7_TK_CCB,&pCur);` |
|      10 | 1528 | `					if( pCur < pEnd ){` |
|      10 | 1529 | `						pCur++;` |
|       4 | 1530 | `					}` |
|       4 | 1531 | `				}` |
|      12 | 1532 | `				continue;` |
|       - | 1533 | `			}` |
|     137 | 1534 | `		}` |
|  320449 | 1535 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|    5863 | 1536 | `			iNest++;` |
|  317520 | 1537 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/) ){` |
|       - | 1538 | `			/* Don't worry about mismatched brackets here, the expression` |
|       - | 1539 | `			 * parser will shortly detect any syntax error. */` |
|    5863 | 1540 | `			iNest--;` |
|    2929 | 1541 | `		}` |
|  320449 | 1542 | `		pCur++;` |
|       5 | 1543 | `	}` |
|  169611 | 1544 | `	return pEnd;` |
|  103712 | 1545 | `}` |
|       - | 1546 | `/*` |
|       - | 1547 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|       - | 1548 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|       - | 1549 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|       - | 1550 | ` */` |
|  108688 | 1551 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|       5 | 1552 | `{` |
|       - | 1553 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|       - | 1554 | `	SyToken *pKey,*pCur;` |
|  108693 | 1555 | `	sxi32 iEmitRef = 0;` |
|  108693 | 1556 | `	sxi32 iSpread = 0;` |
|  108693 | 1557 | `	sxi32 nPair = 0;` |
|       - | 1558 | `	sxi32 rc;` |
|  108693 | 1559 | `	xValidator = 0;` |
|  115563 | 1560 | `	for(;;){` |
|       - | 1561 | `		/* Jump leading commas. Exactly ONE separates two entries; a second one (or a comma` |
|       - | 1562 | `		 * at the very start) means an EMPTY element, which php rejects outright — PH7 just` |
|       - | 1563 | ``		 * skipped them, so `array(,)` and `array(1,,2)` compiled silently. A TRAILING comma`` |
|       - | 1564 | `		 * is legal and is handled by the loop exiting on the next pass. */` |
|   61219 | 1565 | `		{` |
|  231131 | 1566 | `			int nSkip = 0;` |
|  327627 | 1567 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|   96501 | 1568 | `				nSkip++;` |
|   96501 | 1569 | `				pGen->pIn++;` |
|       5 | 1570 | `			}` |
|  231131 | 1571 | `			if( nSkip > 1 \|\| (nSkip > 0 && nPair < 1) ){` |
|     ! 0 | 1572 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn[-1].nLine,` |
|       - | 1573 | `					"Cannot use empty array elements in arrays");` |
|     ! 0 | 1574 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1575 | `					return SXERR_ABORT;` |
|       - | 1576 | `				}` |
|     ! 0 | 1577 | `				return SXRET_OK;` |
|       - | 1578 | `			}` |
|       - | 1579 | `		}` |
|  231131 | 1580 | `		pCur = pGen->pIn;` |
|  231131 | 1581 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|       - | 1582 | `			/* No more entry to process */` |
|  108675 | 1583 | `			break;` |
|       - | 1584 | `		}` |
|  122461 | 1585 | `		if( pCur >= pGen->pIn ){` |
|     ! 0 | 1586 | `			continue;` |
|       - | 1587 | `		}` |
|       - | 1588 | `		/* Compile the key if available */` |
|  122461 | 1589 | `		pKey = pCur;` |
|  122461 | 1590 | `		pCur = GenStateFindTopLevelArrow(pCur,pGen->pIn);` |
|  122461 | 1591 | `		rc = SXERR_EMPTY;` |
|  122461 | 1592 | `		if( pCur < pGen->pIn ){` |
|    8101 | 1593 | `			if( pKey == pCur ){` |
|       - | 1594 | ``				/* `array( => 2)`: the entry STARTS with '=>', so it has no key. php rejects`` |
|       - | 1595 | `				 * it; PH7 warned about a "Missing entry key" and compiled on, accepting` |
|       - | 1596 | ``				 * source php refuses. (The `else if` below could never see this: the arrow`` |
|       - | 1597 | `				 * IS found here, so control never reached it.)` |
|       - | 1598 | `				 * php names the literal's own closer, so short syntax expects ']'. */` |
|       3 | 1599 | `				const char *zClose = (pGen->pEnd && (pGen->pEnd->nType & PH7_TK_CSB))` |
|       - | 1600 | `					? "\"]\"" : "\")\"";` |
|       3 | 1601 | `				rc = PH7_GenSyntaxError(&(*pGen),pCur,zClose);` |
|       3 | 1602 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1603 | `					return SXERR_ABORT;` |
|       - | 1604 | `				}` |
|       3 | 1605 | `				return SXRET_OK;` |
|       - | 1606 | `			}` |
|    8099 | 1607 | `			if( &pCur[1] >= pGen->pIn ){` |
|       - | 1608 | ``				/* `array(1 => )`: php names the token that SHOULD have started the value —`` |
|       - | 1609 | `				 * the ')' or ']' closing the literal — not the '=>' it just read. Passing 0` |
|       - | 1610 | `				 * makes the helper reach for the token past this entry's slice. */` |
|      13 | 1611 | `				rc = PH7_GenSyntaxError(&(*pGen),0,0);` |
|      13 | 1612 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1613 | `					return SXERR_ABORT;` |
|       - | 1614 | `				}` |
|      13 | 1615 | `				return SXRET_OK;` |
|       - | 1616 | `			}` |
|       - | 1617 | `			/* Compile the expression holding the key */` |
|    8089 | 1618 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|       - | 1619 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|    8089 | 1620 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1621 | `				return SXERR_ABORT;` |
|       - | 1622 | `			}` |
|    8089 | 1623 | `			pCur++; /* Jump the '=>' operator */` |
|    4047 | 1624 | `		}else{` |
|       - | 1625 | `			/* Reset back the cursor and point to the entry value */` |
|  114365 | 1626 | `			pCur = pKey;` |
|       - | 1627 | `		}` |
|  122449 | 1628 | `		if( rc == SXERR_EMPTY ){` |
|       - | 1629 | `			/* No key given: load the nil, TAGGED so LOAD_MAP knows this is an absent key` |
|       - | 1630 | ``			 * (auto-index) rather than an explicit `null =>` one, which php deprecates. */`` |
|  114365 | 1631 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,PH7_LOADC_NOKEY,0 /* nil index */,0,0);` |
|   57180 | 1632 | `		}` |
|  122449 | 1633 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
|       - | 1634 | `			/* Insertion by reference, [i.e: $a = array(&$x);] */` |
|      90 | 1635 | `			xValidator = GenStateArrayNodeValidator; /* Only variable are allowed */` |
|      90 | 1636 | `			iEmitRef = 1;` |
|      90 | 1637 | `			pCur++; /* Jump the '&' token */` |
|      90 | 1638 | `			if( pCur >= pGen->pIn ){` |
|       - | 1639 | `				/* Missing value */` |
|       - | 1640 | ``				/* php reports the token that actually stopped it (`array(&)` -> the`` |
|       - | 1641 | `				 * ')'), not a hand-written "missing referenced variable" fatal. */` |
|       3 | 1642 | `				rc = PH7_GenSyntaxError(&(*pGen),pCur < pGen->pIn ? pCur : 0,0);` |
|       3 | 1643 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1644 | `					return SXERR_ABORT;` |
|       - | 1645 | `				}` |
|       3 | 1646 | `				return SXRET_OK;` |
|       - | 1647 | `			}` |
|      42 | 1648 | `		}` |
|       - | 1649 | `		/* Detect array unpack: '...$expr' as the entry value (PHP 7.4+, with` |
|       - | 1650 | `		 * string-key support since PHP 8.1). The parser strips the '...' inside` |
|       - | 1651 | `		 * ExprExtractNode; we only need to know it's there so we can emit` |
|       - | 1652 | `		 * PH7_OP_FLAG_SPREAD after the value, instructing LOAD_MAP to merge the` |
|       - | 1653 | `		 * resulting hashmap rather than insert it as a scalar entry. */` |
|  122447 | 1654 | `		iSpread = (pCur < pGen->pIn && (pCur->nType & PH7_TK_ELLIPSIS)) ? 1 : 0;` |
|  122447 | 1655 | `		if( iSpread && (rc != SXERR_EMPTY \|\| iEmitRef) ){` |
|       - | 1656 | `			/* '[k => ...$a]' and '[&...$a]' are syntax errors in PHP — the` |
|       - | 1657 | `			 * '...' token cannot follow either '=>' or '&' inside an array` |
|       - | 1658 | `			 * literal. Emit the same Parse-error wording PHP uses so the` |
|       - | 1659 | `			 * output is engine-portable. */` |
|       6 | 1660 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pCur->nLine,` |
|       - | 1661 | `				"syntax error, unexpected token \"...\"");` |
|       6 | 1662 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1663 | `				return SXERR_ABORT;` |
|       - | 1664 | `			}` |
|       6 | 1665 | `			return SXRET_OK;` |
|       - | 1666 | `		}` |
|       - | 1667 | ``		/* Compile indice value. A BY-REF element (`'k' => &$a[$i]`) is an`` |
|       - | 1668 | `		 * lvalue: php VIVIFIES a missing subscript when a reference is taken,` |
|       - | 1669 | `		 * so compile it in write context (LOAD_IDX iP2=1, create-if-missing)` |
|       - | 1670 | `		 * instead of a read-only load — which also keeps the undefined-key` |
|       - | 1671 | `		 * warning (a read-only diagnostic) from false-firing here. */` |
|  183662 | 1672 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,` |
|   61219 | 1673 | `			iEmitRef ? EXPR_FLAG_LOAD_IDX_STORE` |
|       - | 1674 | `			         : EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,` |
|   61219 | 1675 | `			xValidator);` |
|  122443 | 1676 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1677 | `			return SXERR_ABORT;` |
|       - | 1678 | `		}` |
|  122443 | 1679 | `		if( iSpread ){` |
|       - | 1680 | `			/* Mark the value on TOS as a spread source; LOAD_MAP merges it. */` |
|      80 | 1681 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FLAG_SPREAD,0,0,0,0);` |
|  122405 | 1682 | `		}else if( iEmitRef ){` |
|       - | 1683 | `			/* Emit the load reference instruction */` |
|      86 | 1684 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|      41 | 1685 | `		}` |
|  122443 | 1686 | `		xValidator = 0;` |
|  122443 | 1687 | `		iEmitRef = 0;` |
|  122443 | 1688 | `		iSpread = 0;` |
|  122443 | 1689 | `		nPair++;` |
|       5 | 1690 | `	}` |
|       - | 1691 | `	/* Emit the load map instruction */` |
|  108675 | 1692 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|       - | 1693 | `	/* Node successfully compiled */` |
|  108675 | 1694 | `	return SXRET_OK;` |
|   54349 | 1695 | `}` |
|       - | 1696 | `/*` |
|       - | 1697 | ` * Compile the 'array' language construct.` |
|       - | 1698 | ` *	 According to the PHP language reference manual` |
|       - | 1699 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|       - | 1700 | ` *   values to keys. This type is optimized for several different uses; it can` |
|       - | 1701 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|       - | 1702 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|       - | 1703 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|       - | 1704 | ` */` |
|   94068 | 1705 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 | 1706 | `{` |
|       - | 1707 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   94073 | 1708 | `	pGen->pIn += 2;` |
|   94073 | 1709 | `	pGen->pEnd--;` |
|   47034 | 1710 | `	SXUNUSED(iCompileFlag);` |
|       - | 1711 | ``	/* php: a stray token in an `array( ... )` element is `... expecting ")"`. */`` |
|       - | 1712 | `	{` |
|   94073 | 1713 | `		const char *zSave = pGen->zClauseCloser;` |
|       - | 1714 | `		sxi32 rc;` |
|   94073 | 1715 | `		pGen->zClauseCloser = "\")\"";` |
|   94073 | 1716 | `		rc = GenStateCompileArrayBody(pGen);` |
|   94073 | 1717 | `		pGen->zClauseCloser = zSave;` |
|   94073 | 1718 | `		return rc;` |
|       - | 1719 | `	}` |
|       5 | 1720 | `}` |
|       - | 1721 | `/*` |
|       - | 1722 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|       - | 1723 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|       - | 1724 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|       - | 1725 | ` */` |
|   14620 | 1726 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 | 1727 | `{` |
|       - | 1728 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|   14625 | 1729 | `	pGen->pIn++;` |
|   14625 | 1730 | `	pGen->pEnd--;` |
|    7310 | 1731 | `	SXUNUSED(iCompileFlag);` |
|       - | 1732 | ``	/* php: a stray token in a `[ ... ]` element is `... expecting "]"`. */`` |
|       - | 1733 | `	{` |
|   14625 | 1734 | `		const char *zSave = pGen->zClauseCloser;` |
|       - | 1735 | `		sxi32 rc;` |
|   14625 | 1736 | `		pGen->zClauseCloser = "\"]\"";` |
|   14625 | 1737 | `		rc = GenStateCompileArrayBody(pGen);` |
|   14625 | 1738 | `		pGen->zClauseCloser = zSave;` |
|   14625 | 1739 | `		return rc;` |
|       - | 1740 | `	}` |
|       5 | 1741 | `}` |
|       - | 1742 | `/*` |
|       - | 1743 | ` * Expression tree validator callback for the 'list' language construct.` |
|       - | 1744 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|       - | 1745 | ` * an invalid expression tree and this function will generate the appropriate` |
|       - | 1746 | ` * error message.` |
|       - | 1747 | ` * See the routine responible of compiling the list language construct` |
|       - | 1748 | ` * for more inforation.` |
|       - | 1749 | ` */` |
|     816 | 1750 | `static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 | 1751 | `{` |
|     821 | 1752 | `	sxi32 rc = GenStateWriteTargetCheck(&(*pGen),pRoot,0);` |
|     821 | 1753 | `	if( rc != SXRET_OK ){` |
|       3 | 1754 | `		return rc;` |
|       - | 1755 | `	}` |
|     819 | 1756 | `	if( pRoot->pOp ){` |
|      64 | 1757 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */` |
|      33 | 1758 | `			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){` |
|       - | 1759 | `				/* Unexpected expression */` |
|     ! 0 | 1760 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 1761 | `					"Assignments can only happen to writable values");` |
|     ! 0 | 1762 | `				if( rc != SXERR_ABORT ){` |
|     ! 0 | 1763 | `					rc = SXERR_INVALID;` |
|     ! 0 | 1764 | `				}` |
|       2 | 1765 | `		}` |
|     787 | 1766 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 1767 | `		/* Unexpected expression */` |
|       6 | 1768 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 1769 | `			"Assignments can only happen to writable values");` |
|       6 | 1770 | `		if( rc != SXERR_ABORT ){` |
|       6 | 1771 | `			rc = SXERR_INVALID;` |
|       2 | 1772 | `		}` |
|       2 | 1773 | `	}` |
|     819 | 1774 | `	return rc;` |
|     413 | 1775 | `}` |
|       - | 1776 | `/*` |
|       - | 1777 | ` * Compile the 'list' language construct.` |
|       - | 1778 | ` *  According to the PHP language reference` |
|       - | 1779 | ` *  list(): Assign variables as if they were an array.` |
|       - | 1780 | ` *  list() is used to assign a list of variables in one operation.` |
|       - | 1781 | ` *  Description` |
|       - | 1782 | ` *   array list (mixed $varname [, mixed $... ] )` |
|       - | 1783 | ` *   Like array(), this is not really a function, but a language construct.` |
|       - | 1784 | ` *   list() is used to assign a list of variables in one operation.` |
|       - | 1785 | ` *  Parameters` |
|       - | 1786 | ` *   $varname: A variable.` |
|       - | 1787 | ` *  Return Values` |
|       - | 1788 | ` *   The assigned array.` |
|       - | 1789 | ` */` |
|       - | 1790 | `/* Nested list entry recorded during first pass of list body compilation */` |
|       - | 1791 | `struct NestedListEntry {` |
|       - | 1792 | `	sxi32 nIndex;        /* Position in the outer list (0-based) */` |
|       - | 1793 | `	SyToken *pStart;     /* Token range: start of nested construct */` |
|       - | 1794 | `	SyToken *pEnd;       /* Token range: past closing delimiter */` |
|       - | 1795 | `	sxi32 isShort;       /* 1 if [...] form, 0 if list(...) form */` |
|       - | 1796 | `};` |
|       - | 1797 | `/*` |
|       - | 1798 | ` * Compile the body of a *keyed* list/short-list destructuring (PHP 7.1), where` |
|       - | 1799 | `` * every entry has the form `keyExpr => target`. The source array is on the stack`` |
|       - | 1800 | ` * top on entry and remains there on exit, mirroring the positional LOAD_LIST` |
|       - | 1801 | ` * path so the caller's teardown is unchanged. For each entry: DUP the source,` |
|       - | 1802 | ` * push the key, LOAD_IDX to fetch source[key] (NULL on a missing key, silently,` |
|       - | 1803 | ` * like a normal subscript read), then assign the fetched value to the target — a` |
|       - | 1804 | ` * nested [...]/list() recurses, a simple lvalue uses the same STORE fold as a` |
|       - | 1805 | ` * normal assignment (the value sits below the lvalue-load, exactly as in` |
|       - | 1806 | ` * GenStateEmitExprCode where the assignment RHS precedes the LHS load).` |
|       - | 1807 | ` */` |
|      44 | 1808 | `static sxi32 GenStateCompileKeyedListBody(ph7_gen_state *pGen)` |
|       4 | 1809 | `{` |
|       - | 1810 | `	SyToken *pNext;` |
|       - | 1811 | `	sxi32 rc;` |
|     102 | 1812 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|       - | 1813 | `		SyToken *pArrow,*pTarget;` |
|       - | 1814 | ``		/* Split `keyExpr => target` at the top-level '=>' */`` |
|      58 | 1815 | `		pArrow = GenStateFindTopLevelArrow(pGen->pIn,pNext);` |
|      58 | 1816 | `		pTarget = &pArrow[1];` |
|      58 | 1817 | `		if( pArrow <= pGen->pIn \|\| pTarget >= pNext ){` |
|       - | 1818 | ``			/* Empty key (`[ => $v]`) or empty value (`["k" =>]`): PHP rejects`` |
|       - | 1819 | `			 * both. Reject rather than silently emitting unbalanced bytecode. */` |
|     ! 0 | 1820 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - | 1821 | `				"Cannot use empty array entries in keyed array assignment");` |
|     ! 0 | 1822 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - | 1823 | `		}` |
|       - | 1824 | `		/* DUP the source array (it is on the stack top) */` |
|      58 | 1825 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|       - | 1826 | `		/* Compile the key expression; it is pushed above the DUP'd source */` |
|      58 | 1827 | `		rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pArrow,EXPR_FLAG_RDONLY_LOAD,0);` |
|      58 | 1828 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1829 | `			return SXERR_ABORT;` |
|       - | 1830 | `		}` |
|       - | 1831 | `		/* LOAD_IDX: pop the key, replace the DUP'd source with source[key].` |
|       - | 1832 | `		 * iP2=7 is the keyed-destructuring read context: an array source reads like` |
|       - | 1833 | ``		 * iP2=0 (missing key loads NULL silently, matching a normal `$arr[$k]` read;`` |
|       - | 1834 | `		 * PHP also emits an "Undefined array key" warning here, PHL omits it — §3.7),` |
|       - | 1835 | `		 * but a NON-array source yields NULL + a per-key "Cannot use <type> as array"` |
|       - | 1836 | `		 * warning instead of char-indexing a string (matching PHP's OP_LOAD_LIST path). */` |
|      58 | 1837 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,7,0,0);` |
|      58 | 1838 | `		if( pTarget < pNext && ( (pTarget->nType & PH7_TK_OSB)` |
|      52 | 1839 | `			\|\| ( (pTarget->nType & PH7_TK_KEYWORD)` |
|      27 | 1840 | `				&& SX_PTR_TO_INT(pTarget->pUserData) == PH7_TKWRD_LIST ) ) ){` |
|       - | 1841 | `			/* Nested destructuring:  ["k" => [ ... ]]  or  ["k" => list( ... )].` |
|       - | 1842 | `			 * Treat source[key] as the inner body's source, then drop the` |
|       - | 1843 | `			 * leftover it leaves behind (mirrors the positional nested path). */` |
|       5 | 1844 | `			sxi32 isShort = (pTarget->nType & PH7_TK_OSB) != 0;` |
|       5 | 1845 | `			SyToken *pSavedIn = pGen->pIn;` |
|       5 | 1846 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|       5 | 1847 | `			pGen->pIn = pTarget;` |
|       5 | 1848 | `			pGen->pEnd = pNext;` |
|       5 | 1849 | `			rc = isShort ? PH7_CompileShortList(&(*pGen),0)` |
|       2 | 1850 | `			             : PH7_CompileList(&(*pGen),0);` |
|       5 | 1851 | `			pGen->pIn = pSavedIn;` |
|       5 | 1852 | `			pGen->pEnd = pSavedEnd;` |
|       5 | 1853 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1854 | `				return SXERR_ABORT;` |
|       - | 1855 | `			}` |
|       5 | 1856 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       3 | 1857 | `		}else{` |
|       - | 1858 | `			/* Simple lvalue target ($v / $o->p / $a[i] / Cls::$s). source[key]` |
|       - | 1859 | `			 * is already on the stack as the value; compiling the target appends` |
|       - | 1860 | `			 * its lvalue-load, which we fold into a STORE just as a normal` |
|       - | 1861 | `			 * assignment does. */` |
|       - | 1862 | `			VmInstr *pInstr;` |
|      54 | 1863 | `			sxi32 iVmOp = PH7_OP_STORE;` |
|      54 | 1864 | `			sxi32 iP1 = 0, iP2 = 0;` |
|      54 | 1865 | `			void *p3 = 0;` |
|      54 | 1866 | `			rc = GenStateCompileArrayEntry(&(*pGen),pTarget,pNext,` |
|       - | 1867 | `				EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|      54 | 1868 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 1869 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - | 1870 | `			}` |
|      54 | 1871 | `			if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|      54 | 1872 | `				if( pInstr->iOp == PH7_OP_MEMBER ){` |
|       6 | 1873 | `					iP2 = 1; /* member store: keep MEMBER, store value below it */` |
|      51 | 1874 | `				}else if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       3 | 1875 | `					iVmOp = PH7_OP_STORE_IDX;` |
|       3 | 1876 | `					iP1 = pInstr->iP1;` |
|       3 | 1877 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       2 | 1878 | `				}else{` |
|      47 | 1879 | `					p3 = pInstr->p3; /* named store: $v = value */` |
|      47 | 1880 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - | 1881 | `				}` |
|      25 | 1882 | `			}` |
|      54 | 1883 | `			PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|       - | 1884 | `			/* STORE leaves the assigned value on the stack top; drop it so the` |
|       - | 1885 | `			 * source array is back on top for the next entry. */` |
|      54 | 1886 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       - | 1887 | `		}` |
|      58 | 1888 | `		pGen->pIn = &pNext[1];` |
|       4 | 1889 | `	}` |
|      48 | 1890 | `	return SXRET_OK;` |
|      26 | 1891 | `}` |
|       - | 1892 | `/*` |
|       - | 1893 | ` * Shared body for list() and short list [...] compilation.` |
|       - | 1894 | ` * Assumes pGen->pIn and pGen->pEnd are already positioned past` |
|       - | 1895 | ` * the opening delimiter and before the closing delimiter.` |
|       - | 1896 | ` */` |
|     444 | 1897 | `static sxi32 GenStateCompileListBody(ph7_gen_state *pGen)` |
|       5 | 1898 | `{` |
|       - | 1899 | `	SySet sNested; /* Dynamically-sized container of NestedListEntry */` |
|       - | 1900 | `	SyToken *pNext;` |
|       - | 1901 | `	SyToken *pClassifyIn;` |
|     449 | 1902 | `	sxi32 nKeyed = 0, nPositional = 0, nEmpty = 0;` |
|       - | 1903 | `	sxi32 nExpr;` |
|       - | 1904 | `	sxi32 rc;` |
|       - | 1905 | ``	/* First pass: classify entries as keyed (`k => v`), positional, or empty`` |
|       - | 1906 | `	 * skip slots ([,]). A list level must be entirely keyed or entirely` |
|       - | 1907 | `	 * positional — PHP fatals on a mix, and on an empty slot inside a keyed` |
|       - | 1908 | `	 * list. */` |
|     449 | 1909 | `	pClassifyIn = pGen->pIn;` |
|    1303 | 1910 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|     859 | 1911 | `		if( pGen->pIn >= pNext ){` |
|      19 | 1912 | `			nEmpty++;` |
|     850 | 1913 | `		}else if( GenStateFindTopLevelArrow(pGen->pIn,pNext) < pNext ){` |
|      58 | 1914 | `			nKeyed++;` |
|      31 | 1915 | `		}else{` |
|     787 | 1916 | `			nPositional++;` |
|       - | 1917 | `		}` |
|     859 | 1918 | `		pGen->pIn = &pNext[1];` |
|       5 | 1919 | `	}` |
|     449 | 1920 | `	pGen->pIn = pClassifyIn;` |
|     449 | 1921 | `	if( nKeyed > 0 && nEmpty > 0 ){` |
|     ! 0 | 1922 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - | 1923 | `			"Cannot use empty array entries in keyed array assignment");` |
|     ! 0 | 1924 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - | 1925 | `	}` |
|     449 | 1926 | `	if( nKeyed > 0 && nPositional > 0 ){` |
|     ! 0 | 1927 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - | 1928 | `			"Cannot mix keyed and unkeyed array entries in assignments");` |
|     ! 0 | 1929 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - | 1930 | `	}` |
|     449 | 1931 | `	if( nKeyed > 0 ){` |
|      48 | 1932 | `		return GenStateCompileKeyedListBody(pGen);` |
|       - | 1933 | `	}` |
|     405 | 1934 | `	nExpr = 0;` |
|     405 | 1935 | `	SySetInit(&sNested,&pGen->pVm->sAllocator,sizeof(struct NestedListEntry));` |
|    1205 | 1936 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|     805 | 1937 | `		if( pGen->pIn < pNext ){` |
|       - | 1938 | `			/* Check for nested list() */` |
|     787 | 1939 | `			if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       3 | 1940 | `				SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - | 1941 | `				/* Record this nested list for post-processing */` |
|       3 | 1942 | `				SyToken *pListEnd = 0;` |
|       3 | 1943 | `				if( &pGen->pIn[1] < pNext && (pGen->pIn[1].nType & PH7_TK_LPAREN) ){` |
|       3 | 1944 | `					PH7_DelimitNestedTokens(pGen->pIn+2,pNext,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       1 | 1945 | `				}` |
|       3 | 1946 | `				if( pListEnd ){` |
|       - | 1947 | `					struct NestedListEntry sEntry;` |
|       3 | 1948 | `					sEntry.nIndex = nExpr;` |
|       3 | 1949 | `					sEntry.pStart = pGen->pIn;` |
|       3 | 1950 | `					sEntry.pEnd = pListEnd + 1;` |
|       3 | 1951 | `					sEntry.isShort = 0;` |
|       3 | 1952 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|       1 | 1953 | `				}` |
|       - | 1954 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|       3 | 1955 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|     786 | 1956 | `			}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - | 1957 | `				/* Nested short destructuring [...] */` |
|      16 | 1958 | `				SyToken *pBracketEnd = 0;` |
|      16 | 1959 | `				PH7_DelimitNestedTokens(pGen->pIn+1,pNext,PH7_TK_OSB,PH7_TK_CSB,&pBracketEnd);` |
|      16 | 1960 | `				if( pBracketEnd ){` |
|       - | 1961 | `					struct NestedListEntry sEntry;` |
|      16 | 1962 | `					sEntry.nIndex = nExpr;` |
|      16 | 1963 | `					sEntry.pStart = pGen->pIn;` |
|      16 | 1964 | `					sEntry.pEnd = pBracketEnd + 1;` |
|      16 | 1965 | `					sEntry.isShort = 1;` |
|      16 | 1966 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|       7 | 1967 | `				}` |
|       - | 1968 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|      16 | 1969 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       9 | 1970 | `			}else{` |
|       - | 1971 | `				/* Compile the expression holding the variable */` |
|     771 | 1972 | `				rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|     771 | 1973 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 1974 | `					SySetRelease(&sNested);` |
|     ! 0 | 1975 | `					return SXRET_OK;` |
|       - | 1976 | `				}` |
|       - | 1977 | `				{` |
|       - | 1978 | `					/* A property target ($o->p / Cls::$s) is a PURE WRITE here — the` |
|       - | 1979 | `					 * value lands via the following OP_LOAD_LIST's direct slot store.` |
|       - | 1980 | `					 * Tag the member so OP_MEMBER skips the uninitialized-typed read` |
|       - | 1981 | `					 * Error / __get consult and vivifies a missing property (php` |
|       - | 1982 | `					 * assigns without reading). */` |
|     771 | 1983 | `					VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|     771 | 1984 | `					if( pLast && pLast->iOp == PH7_OP_MEMBER && pLast->iP2 == PH7_MEMBER_READ ){` |
|      56 | 1985 | `						pLast->iP2 = PH7_MEMBER_LIST_TARGET;` |
|      27 | 1986 | `					}` |
|       - | 1987 | `				}` |
|       - | 1988 | `			}` |
|     396 | 1989 | `		}else{` |
|       - | 1990 | `			/* Empty entry,load NULL */` |
|      19 | 1991 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);` |
|       - | 1992 | `		}` |
|     805 | 1993 | `		nExpr++;` |
|       - | 1994 | `		/* Advance the stream cursor */` |
|     805 | 1995 | `		pGen->pIn = &pNext[1];` |
|       5 | 1996 | `	}` |
|       - | 1997 | `	/* Emit the LOAD_LIST instruction */` |
|     405 | 1998 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,0,0,0);` |
|       - | 1999 | `	/* After LOAD_LIST, the source array is still on the stack top.` |
|       - | 2000 | `	 * For each nested entry, emit code to extract the sub-array` |
|       - | 2001 | `	 * at the corresponding index and recursively destructure it.` |
|       - | 2002 | `	 */` |
|     405 | 2003 | `	if( SySetUsed(&sNested) > 0 ){` |
|      16 | 2004 | `		struct NestedListEntry *apNested = (struct NestedListEntry *)SySetBasePtr(&sNested);` |
|       - | 2005 | `		sxu32 i;` |
|      32 | 2006 | `		for(i = 0; i < SySetUsed(&sNested); i++){` |
|      18 | 2007 | `			SyToken *pSavedIn = pGen->pIn;` |
|      18 | 2008 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|       - | 2009 | `			ph7_value *pIdx;` |
|       - | 2010 | `			sxu32 nConstIdx;` |
|       - | 2011 | `			/* DUP the source array (it's on stack top) */` |
|      18 | 2012 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|       - | 2013 | `			/* Push the integer index for this nested entry */` |
|      18 | 2014 | `			pIdx = PH7_ReserveConstObj(pGen->pVm,&nConstIdx);` |
|      18 | 2015 | `			if( pIdx == 0 ){` |
|     ! 0 | 2016 | `				PH7_GenCompileError(&(*pGen),E_ERROR,0,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 2017 | `				SySetRelease(&sNested);` |
|     ! 0 | 2018 | `				return SXERR_ABORT;` |
|       - | 2019 | `			}` |
|      18 | 2020 | `			PH7_MemObjInitFromInt(pGen->pVm,pIdx,(sxi64)apNested[i].nIndex);` |
|      18 | 2021 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nConstIdx,0,0);` |
|       - | 2022 | `			/* LOAD_IDX: pop index, replace DUP'd source with source[index].` |
|       - | 2023 | `			 * iP2=2 signals the VM to emit an "Undefined array key" warning` |
|       - | 2024 | `			 * when the key is missing (PHP-compatible list destructuring).` |
|       - | 2025 | `			 */` |
|      18 | 2026 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,2,0,0);` |
|       - | 2027 | `			/* Recursively compile the inner list */` |
|      18 | 2028 | `			pGen->pIn = apNested[i].pStart;` |
|      18 | 2029 | `			pGen->pEnd = apNested[i].pEnd;` |
|      18 | 2030 | `			if( apNested[i].isShort ){` |
|      16 | 2031 | `				rc = PH7_CompileShortList(&(*pGen),0);` |
|       9 | 2032 | `			}else{` |
|       3 | 2033 | `				rc = PH7_CompileList(&(*pGen),0);` |
|       - | 2034 | `			}` |
|      18 | 2035 | `			pGen->pIn = pSavedIn;` |
|      18 | 2036 | `			pGen->pEnd = pSavedEnd;` |
|      18 | 2037 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2038 | `				SySetRelease(&sNested);` |
|     ! 0 | 2039 | `				return SXERR_ABORT;` |
|       - | 2040 | `			}` |
|       - | 2041 | `			/* Pop the leftover source[index] from the inner LOAD_LIST */` |
|      18 | 2042 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      10 | 2043 | `		}` |
|       7 | 2044 | `	}` |
|     405 | 2045 | `	SySetRelease(&sNested);` |
|       - | 2046 | `	/* Node successfully compiled */` |
|     405 | 2047 | `	return SXRET_OK;` |
|     227 | 2048 | `}` |
|      50 | 2049 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 | 2050 | `{` |
|       - | 2051 | `	/* Jump the 'list' keyword, the leading '(' and exclude trailing ')' */` |
|      55 | 2052 | `	pGen->pIn += 2;` |
|      55 | 2053 | `	pGen->pEnd--;` |
|      25 | 2054 | `	SXUNUSED(iCompileFlag);` |
|      55 | 2055 | `	return GenStateCompileListBody(pGen);` |
|       5 | 2056 | `}` |
|     394 | 2057 | `PH7_PRIVATE sxi32 PH7_CompileShortList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 | 2058 | `{` |
|       - | 2059 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|     399 | 2060 | `	pGen->pIn++;` |
|     399 | 2061 | `	pGen->pEnd--;` |
|     197 | 2062 | `	SXUNUSED(iCompileFlag);` |
|     399 | 2063 | `	return GenStateCompileListBody(pGen);` |
|       5 | 2064 | `}` |
|       - | 2065 | `/*` |
|       - | 2066 | ` * assert() source-text rendering.` |
|       - | 2067 | ` *` |
|       - | 2068 | ` * php compiles a DIRECT assert() call with a copy of the argument's AST, and a` |
|       - | 2069 | `` * failing assertion reports zend_ast_export() of that AST — `assert(1 == 2)`,`` |
|       - | 2070 | `` * `assert($x)`, `assert('')` — which is exactly the information the message`` |
|       - | 2071 | ` * exists to carry. PHL has no AST copy at runtime, so the compiler renders the` |
|       - | 2072 | ` * argument's TOKEN SPAN here, at compile time, normalizing to php's export` |
|       - | 2073 | ` * shape (each rule probed against php 8.5):` |
|       - | 2074 | ` *   - literal values fold the way php's AST holds them: numbers render from` |
|       - | 2075 | ` *     their parsed VALUE (0x10 -> 16, 1e3 -> 1000.0, 1_000 -> 1000, an` |
|       - | 2076 | ` *     int64-overflowing literal -> float), strings render single-quoted with` |
|       - | 2077 | ` *     their PROCESSED contents (\ and ' re-escaped), array(...) -> [...].` |
|       - | 2078 | ` *   - one space around binary operators, ", " between arguments/elements, no` |
|       - | 2079 | `` *     space inside ()/[] or around ->/?->/::/casts, `and`/`or` -> `&&`/`\|\|`,`` |
|       - | 2080 | ` *     a trailing comma is dropped, redundant OUTERMOST parens are dropped.` |
|       - | 2081 | ` * Accepted divergences from zend_ast_export on exotic input (message text` |
|       - | 2082 | ` * only, never behavior): redundant INNER parens are kept (php re-derives` |
|       - | 2083 | ` * grouping from precedence), interpolated "$x" strings and heredocs render as` |
|       - | 2084 | `` * written (php exports its interpolation AST), `new C` does not grow php's`` |
|       - | 2085 | `` * trailing `()`, and constant folding beyond single literals is not applied`` |
|       - | 2086 | `` * (php renders `'' . ''` as `''`).`` |
|       - | 2087 | ` */` |
|       - | 2088 | `/* Spacing classes: a space is inserted between two tokens when either side` |
|       - | 2089 | ` * FORCEs one (binary operators, the slot after a comma) or both sides are` |
|       - | 2090 | ` * operand-like (WANT). Grouping punctuation and glue operators contribute` |
|       - | 2091 | ` * NONE on their tight side. */` |
|       - | 2092 | `#define ASRT_SP_NONE  0` |
|       - | 2093 | `#define ASRT_SP_WANT  1` |
|       - | 2094 | `#define ASRT_SP_FORCE 2` |
|       - | 2095 | `enum AssertTokClass {` |
|       - | 2096 | `	ASRT_START = 0, /* virtual class before the first token */` |
|       - | 2097 | `	ASRT_OPERAND,   /* literals, identifiers, keywords */` |
|       - | 2098 | `	ASRT_BINOP,     /* == + . && ? : => instanceof ... */` |
|       - | 2099 | `	ASRT_UNARY,     /* ! ~ @ - + & casts, '$', '...' — glue after */` |
|       - | 2100 | `	ASRT_OPEN,      /* ( [ */` |
|       - | 2101 | `	ASRT_CLOSE,     /* ) ] */` |
|       - | 2102 | `	ASRT_GLUE,      /* -> ?-> :: ++ -- \ — glue both sides */` |
|       - | 2103 | `	ASRT_COMMA      /* , — glue before, force after */` |
|       - | 2104 | `};` |
|       - | 2105 | `static const sxu8 aAsrtBefore[] = { ASRT_SP_NONE, ASRT_SP_WANT, ASRT_SP_FORCE, ASRT_SP_WANT,` |
|       - | 2106 | `	ASRT_SP_NONE, ASRT_SP_NONE, ASRT_SP_NONE, ASRT_SP_NONE };` |
|       - | 2107 | `static const sxu8 aAsrtAfter[]  = { ASRT_SP_NONE, ASRT_SP_WANT, ASRT_SP_FORCE, ASRT_SP_NONE,` |
|       - | 2108 | `	ASRT_SP_NONE, ASRT_SP_WANT, ASRT_SP_NONE, ASRT_SP_FORCE };` |
|       - | 2109 | `/*` |
|       - | 2110 | ` * Append one PROCESSED string-value byte, re-escaped for a single-quoted` |
|       - | 2111 | ` * rendering: php's export escapes only backslash and the quote itself; every` |
|       - | 2112 | ` * other byte (including control characters) is emitted raw.` |
|       - | 2113 | ` */` |
|      42 | 2114 | `static void AssertRenderQuotedByte(SyBlob *pOut,int c)` |
|       2 | 2115 | `{` |
|      44 | 2116 | `	char ch = (char)c;` |
|      44 | 2117 | `	if( c == '\\' \|\| c == '\'' ){` |
|       3 | 2118 | `		SyBlobAppend(pOut,"\\",1);` |
|       1 | 2119 | `	}` |
|      44 | 2120 | `	SyBlobAppend(pOut,&ch,1);` |
|      44 | 2121 | `}` |
|       - | 2122 | `/* Append the UTF-8 encoding of a \u{...} code point (value bytes, re-escaped). */` |
|     ! 0 | 2123 | `static void AssertRenderUtf8(SyBlob *pOut,sxu32 c)` |
|     ! 0 | 2124 | `{` |
|     ! 0 | 2125 | `	if( c < 0x80 ){` |
|     ! 0 | 2126 | `		AssertRenderQuotedByte(pOut,(int)c);` |
|     ! 0 | 2127 | `	}else if( c < 0x800 ){` |
|     ! 0 | 2128 | `		AssertRenderQuotedByte(pOut,(int)(0xc0 \| (c >> 6)));` |
|     ! 0 | 2129 | `		AssertRenderQuotedByte(pOut,(int)(0x80 \| (c & 0x3f)));` |
|     ! 0 | 2130 | `	}else if( c < 0x10000 ){` |
|     ! 0 | 2131 | `		AssertRenderQuotedByte(pOut,(int)(0xe0 \| (c >> 12)));` |
|     ! 0 | 2132 | `		AssertRenderQuotedByte(pOut,(int)(0x80 \| ((c >> 6) & 0x3f)));` |
|     ! 0 | 2133 | `		AssertRenderQuotedByte(pOut,(int)(0x80 \| (c & 0x3f)));` |
|     ! 0 | 2134 | `	}else{` |
|     ! 0 | 2135 | `		AssertRenderQuotedByte(pOut,(int)(0xf0 \| (c >> 18)));` |
|     ! 0 | 2136 | `		AssertRenderQuotedByte(pOut,(int)(0x80 \| ((c >> 12) & 0x3f)));` |
|     ! 0 | 2137 | `		AssertRenderQuotedByte(pOut,(int)(0x80 \| ((c >> 6) & 0x3f)));` |
|     ! 0 | 2138 | `		AssertRenderQuotedByte(pOut,(int)(0x80 \| (c & 0x3f)));` |
|       - | 2139 | `	}` |
|     ! 0 | 2140 | `}` |
|       - | 2141 | `/*` |
|       - | 2142 | ` * Render a single-quoted-source string (or nowdoc body): only \\ and \' are` |
|       - | 2143 | ` * escape sequences there; any other backslash is a literal byte.` |
|       - | 2144 | ` */` |
|       2 | 2145 | `static void AssertRenderSglString(SyBlob *pOut,const char *z,sxu32 n)` |
|       1 | 2146 | `{` |
|       3 | 2147 | `	sxu32 i = 0;` |
|       3 | 2148 | `	SyBlobAppend(pOut,"'",1);` |
|      11 | 2149 | `	while( i < n ){` |
|       9 | 2150 | `		if( z[i] == '\\' && i + 1 < n && (z[i+1] == '\\' \|\| z[i+1] == '\'') ){` |
|       3 | 2151 | `			AssertRenderQuotedByte(pOut,z[i+1]);` |
|       3 | 2152 | `			i += 2;` |
|       2 | 2153 | `		}else{` |
|       7 | 2154 | `			AssertRenderQuotedByte(pOut,z[i]);` |
|       7 | 2155 | `			i++;` |
|       - | 2156 | `		}` |
|       1 | 2157 | `	}` |
|       3 | 2158 | `	SyBlobAppend(pOut,"'",1);` |
|       3 | 2159 | `}` |
|       - | 2160 | `/*` |
|       - | 2161 | ` * Render a double-quoted-source string (or heredoc body) with php's escape` |
|       - | 2162 | ` * processing — the value bytes are what php's AST holds, and the export prints` |
|       - | 2163 | ` * them single-quoted. An UNKNOWN escape keeps the backslash and the character,` |
|       - | 2164 | ` * matching php's string semantics.` |
|       - | 2165 | ` */` |
|      12 | 2166 | `static void AssertRenderDblString(SyBlob *pOut,const char *z,sxu32 n)` |
|       3 | 2167 | `{` |
|      15 | 2168 | `	sxu32 i = 0;` |
|      15 | 2169 | `	SyBlobAppend(pOut,"'",1);` |
|      49 | 2170 | `	while( i < n ){` |
|      36 | 2171 | `		int c = z[i];` |
|       - | 2172 | `		int d;` |
|      36 | 2173 | `		if( c != '\\' \|\| i + 1 >= n ){` |
|      36 | 2174 | `			AssertRenderQuotedByte(pOut,c);` |
|      36 | 2175 | `			i++;` |
|      36 | 2176 | `			continue;` |
|       - | 2177 | `		}` |
|     ! 0 | 2178 | `		d = z[i+1];` |
|     ! 0 | 2179 | `		i += 2;` |
|     ! 0 | 2180 | `		switch(d){` |
|     ! 0 | 2181 | `		case 'n': AssertRenderQuotedByte(pOut,'\n'); break;` |
|     ! 0 | 2182 | `		case 't': AssertRenderQuotedByte(pOut,'\t'); break;` |
|     ! 0 | 2183 | `		case 'r': AssertRenderQuotedByte(pOut,'\r'); break;` |
|     ! 0 | 2184 | `		case 'v': AssertRenderQuotedByte(pOut,'\v'); break;` |
|     ! 0 | 2185 | `		case 'f': AssertRenderQuotedByte(pOut,'\f'); break;` |
|     ! 0 | 2186 | `		case 'e': AssertRenderQuotedByte(pOut,0x1b); break;` |
|     ! 0 | 2187 | `		case '\\': AssertRenderQuotedByte(pOut,'\\'); break;` |
|     ! 0 | 2188 | `		case '"': AssertRenderQuotedByte(pOut,'"'); break;` |
|     ! 0 | 2189 | `		case '$': AssertRenderQuotedByte(pOut,'$'); break;` |
|     ! 0 | 2190 | `		case 'x': case 'X': {` |
|       - | 2191 | `			/* Up to two hex digits; a bare \x is literal. */` |
|     ! 0 | 2192 | `			int nHex = 0, v = 0;` |
|     ! 0 | 2193 | `			while( nHex < 2 && i < n && (unsigned char)z[i] < 0x80 && SyisHex((unsigned char)z[i]) ){` |
|     ! 0 | 2194 | `				v = (v << 4) \| SyHexToint((unsigned char)z[i]);` |
|     ! 0 | 2195 | `				i++; nHex++;` |
|     ! 0 | 2196 | `			}` |
|     ! 0 | 2197 | `			if( nHex > 0 ){` |
|     ! 0 | 2198 | `				AssertRenderQuotedByte(pOut,v);` |
|     ! 0 | 2199 | `			}else{` |
|     ! 0 | 2200 | `				AssertRenderQuotedByte(pOut,'\\');` |
|     ! 0 | 2201 | `				AssertRenderQuotedByte(pOut,d);` |
|       - | 2202 | `			}` |
|     ! 0 | 2203 | `			break;` |
|       - | 2204 | `		}` |
|     ! 0 | 2205 | `		case 'u': {` |
|       - | 2206 | `			/* \u{HEX+} — anything else keeps the backslash (php). */` |
|     ! 0 | 2207 | `			if( i < n && z[i] == '{' ){` |
|     ! 0 | 2208 | `				sxu32 v = 0; sxu32 j = i + 1; int nHex = 0;` |
|     ! 0 | 2209 | `				while( j < n && (unsigned char)z[j] < 0x80 && SyisHex((unsigned char)z[j]) && nHex < 8 ){` |
|     ! 0 | 2210 | `					v = (v << 4) \| (sxu32)SyHexToint((unsigned char)z[j]);` |
|     ! 0 | 2211 | `					j++; nHex++;` |
|     ! 0 | 2212 | `				}` |
|     ! 0 | 2213 | `				if( nHex > 0 && j < n && z[j] == '}' ){` |
|     ! 0 | 2214 | `					AssertRenderUtf8(pOut,v);` |
|     ! 0 | 2215 | `					i = j + 1;` |
|     ! 0 | 2216 | `					break;` |
|       - | 2217 | `				}` |
|     ! 0 | 2218 | `			}` |
|     ! 0 | 2219 | `			AssertRenderQuotedByte(pOut,'\\');` |
|     ! 0 | 2220 | `			AssertRenderQuotedByte(pOut,d);` |
|     ! 0 | 2221 | `			break;` |
|       - | 2222 | `		}` |
|     ! 0 | 2223 | `		default:` |
|     ! 0 | 2224 | `			if( d >= '0' && d <= '7' ){` |
|       - | 2225 | `				/* Up to three octal digits (the first was d). */` |
|     ! 0 | 2226 | `				int nOct = 1, v = d - '0';` |
|     ! 0 | 2227 | `				while( nOct < 3 && i < n && z[i] >= '0' && z[i] <= '7' ){` |
|     ! 0 | 2228 | `					v = (v << 3) \| (z[i] - '0');` |
|     ! 0 | 2229 | `					i++; nOct++;` |
|     ! 0 | 2230 | `				}` |
|     ! 0 | 2231 | `				AssertRenderQuotedByte(pOut,v & 0xff);` |
|     ! 0 | 2232 | `			}else{` |
|     ! 0 | 2233 | `				AssertRenderQuotedByte(pOut,'\\');` |
|     ! 0 | 2234 | `				AssertRenderQuotedByte(pOut,d);` |
|       - | 2235 | `			}` |
|     ! 0 | 2236 | `			break;` |
|       - | 2237 | `		}` |
|     ! 0 | 2238 | `	}` |
|      15 | 2239 | `	SyBlobAppend(pOut,"'",1);` |
|      15 | 2240 | `}` |
|       - | 2241 | `/*` |
|       - | 2242 | ` * Append a double in php's AST-export shape: the shortest round-tripping` |
|       - | 2243 | ` * decimal, with a forced ".0" fraction when the digits alone look integral` |
|       - | 2244 | ` * (1e3 -> "1000.0", 1e20 -> "1.0E+20") — the var_export float shape.` |
|       - | 2245 | ` */` |
|       8 | 2246 | `static void AssertRenderReal(SyBlob *pOut,ph7_real rVal)` |
|       1 | 2247 | `{` |
|       - | 2248 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|       - | 2249 | `	/* No floating point: ph7_real IS sxi64, there is no shortest-round-trip` |
|       - | 2250 | `	 * decimal to search for and no ".0" to force, so the value renders as the` |
|       - | 2251 | `	 * integer it is -- the same shape the INTEGER arm below emits. Taking` |
|       - | 2252 | `	 * ph7_real rather than double is what keeps the two call sites from` |
|       - | 2253 | `	 * narrowing (MSVC /W4 makes that C4244, and /WX makes it an error). */` |
|       - | 2254 | `	SyBlobFormat(pOut,"%qd",(sxi64)rVal);` |
|       - | 2255 | `#else` |
|       9 | 2256 | `	sxu32 nBefore = SyBlobLength(pOut);` |
|       - | 2257 | `	const char *zOut;` |
|       - | 2258 | `	sxu32 i, nAfter;` |
|       9 | 2259 | `	int bPlain = 1;` |
|       9 | 2260 | `	PH7_AppendShortestReal(pOut,rVal);` |
|       9 | 2261 | `	zOut = (const char *)SyBlobData(pOut);` |
|       9 | 2262 | `	nAfter = SyBlobLength(pOut);` |
|      23 | 2263 | `	for( i = nBefore; i < nAfter; i++ ){` |
|      21 | 2264 | `		if( !((zOut[i] >= '0' && zOut[i] <= '9') \|\| zOut[i] == '-') ){` |
|       7 | 2265 | `			bPlain = 0;` |
|       7 | 2266 | `			break;` |
|       - | 2267 | `		}` |
|       8 | 2268 | `	}` |
|       9 | 2269 | `	if( bPlain ){` |
|       3 | 2270 | `		SyBlobAppend(pOut,".0",2);` |
|       1 | 2271 | `	}` |
|       - | 2272 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|       9 | 2273 | `}` |
|       - | 2274 | `/*` |
|       - | 2275 | ` * Render the token span [pIn, pEnd) — a direct assert() call's first argument —` |
|       - | 2276 | ` * into pOut in php's zend_ast_export shape (see the block comment above).` |
|       - | 2277 | ` * Total: every span renders to SOMETHING (unknown constructs fall back to` |
|       - | 2278 | ` * their raw token text), so the capture never aborts a compile.` |
|       - | 2279 | ` */` |
|      62 | 2280 | `PH7_PRIVATE void PH7_GenRenderAssertSpan(ph7_gen_state *pGen,SyToken *pIn,SyToken *pEnd,SyBlob *pOut)` |
|       5 | 2281 | `{` |
|       - | 2282 | `	sxu8 aParen[64]; /* 1 = this '(' depth is an array(...) literal rendered as [...] */` |
|      67 | 2283 | `	sxu32 nParen = 0;` |
|      67 | 2284 | `	int iPrev = ASRT_START;` |
|      67 | 2285 | ``	int bArrayOpen = 0; /* the next '(' belongs to a suppressed `array` keyword */`` |
|       - | 2286 | `	/* php drops every redundant paren when re-deriving source from the AST;` |
|       - | 2287 | `	 * dropping the OUTERMOST pair(s) is the token-level equivalent for the` |
|       - | 2288 | ``	 * common `assert((...))` spelling. */`` |
|      69 | 2289 | `	while( pIn < pEnd - 1 && (pIn->nType & PH7_TK_LPAREN) && (pEnd[-1].nType & PH7_TK_RPAREN) ){` |
|       - | 2290 | `		SyToken *p;` |
|       3 | 2291 | `		sxi32 iDepth = 0;` |
|       3 | 2292 | `		SyToken *pMatch = 0;` |
|      11 | 2293 | `		for( p = pIn; p < pEnd; p++ ){` |
|      11 | 2294 | `			if( p->nType & PH7_TK_LPAREN ){` |
|       3 | 2295 | `				iDepth++;` |
|      10 | 2296 | `			}else if( p->nType & PH7_TK_RPAREN ){` |
|       3 | 2297 | `				iDepth--;` |
|       3 | 2298 | `				if( iDepth == 0 ){ pMatch = p; break; }` |
|     ! 0 | 2299 | `			}` |
|       5 | 2300 | `		}` |
|       3 | 2301 | `		if( pMatch != &pEnd[-1] ){` |
|     ! 0 | 2302 | `			break;` |
|       - | 2303 | `		}` |
|       3 | 2304 | `		pIn++;` |
|       3 | 2305 | `		pEnd--;` |
|       1 | 2306 | `	}` |
|     267 | 2307 | `	for( ; pIn < pEnd ; pIn++ ){` |
|     205 | 2308 | `		SyToken *pTok = pIn;` |
|     205 | 2309 | `		const char *zTxt = pTok->sData.zString;` |
|     205 | 2310 | `		sxu32 nTxt = pTok->sData.nByte;` |
|       - | 2311 | `		int iCls;` |
|       - | 2312 | `		sxu32 nMark;` |
|       - | 2313 | `		/* --- classify + pre-token handling ------------------------------ */` |
|     205 | 2314 | `		if( pTok->nType & PH7_TK_LPAREN ){` |
|      15 | 2315 | `			iCls = ASRT_OPEN;` |
|     199 | 2316 | `		}else if( pTok->nType & PH7_TK_RPAREN ){` |
|      15 | 2317 | `			iCls = ASRT_CLOSE;` |
|     187 | 2318 | `		}else if( pTok->nType & (PH7_TK_OSB\|PH7_TK_CSB) ){` |
|      13 | 2319 | `			iCls = (pTok->nType & PH7_TK_OSB) ? ASRT_OPEN : ASRT_CLOSE;` |
|     175 | 2320 | `		}else if( pTok->nType & PH7_TK_COMMA ){` |
|       - | 2321 | `			/* php's export never prints a trailing comma. */` |
|       7 | 2322 | `			if( &pIn[1] < pEnd && (pIn[1].nType & (PH7_TK_RPAREN\|PH7_TK_CSB)) ){` |
|     ! 0 | 2323 | `				continue;` |
|       - | 2324 | `			}` |
|       7 | 2325 | `			iCls = ASRT_COMMA;` |
|     166 | 2326 | `		}else if( pTok->nType & PH7_TK_DOLLAR ){` |
|       3 | 2327 | `			iCls = ASRT_UNARY; /* operand-like before, glued to its name after */` |
|     162 | 2328 | `		}else if( pTok->nType & PH7_TK_NSSEP ){` |
|     ! 0 | 2329 | `			iCls = ASRT_GLUE;` |
|     161 | 2330 | `		}else if( pTok->nType & PH7_TK_ELLIPSIS ){` |
|     ! 0 | 2331 | `			iCls = ASRT_UNARY;` |
|     161 | 2332 | `		}else if( pTok->nType & PH7_TK_OP ){` |
|      46 | 2333 | `			iCls = ASRT_BINOP;` |
|      46 | 2334 | `			if( nTxt > 0 ){` |
|      46 | 2335 | `				int c0 = zTxt[0];` |
|      46 | 2336 | `				if( c0 == '(' ){` |
|     ! 0 | 2337 | ``					iCls = ASRT_UNARY; /* lexer-merged cast token `(int)` */`` |
|      60 | 2338 | `				}else if( nTxt == 2 && (SyMemcmp(zTxt,"->",2) == 0 \|\| SyMemcmp(zTxt,"::",2) == 0` |
|      28 | 2339 | `						\|\| SyMemcmp(zTxt,"++",2) == 0 \|\| SyMemcmp(zTxt,"--",2) == 0) ){` |
|     ! 0 | 2340 | `					iCls = ASRT_GLUE;` |
|      46 | 2341 | `				}else if( nTxt == 3 && SyMemcmp(zTxt,"?->",3) == 0 ){` |
|     ! 0 | 2342 | `					iCls = ASRT_GLUE;` |
|      46 | 2343 | `				}else if( nTxt == 1 && (c0 == '!' \|\| c0 == '~' \|\| c0 == '@') ){` |
|       3 | 2344 | `					iCls = ASRT_UNARY;` |
|      45 | 2345 | `				}else if( nTxt == 1 && (c0 == '-' \|\| c0 == '+' \|\| c0 == '&') ){` |
|       - | 2346 | `					/* Unary when nothing operand-like precedes. */` |
|       4 | 2347 | `					if( iPrev == ASRT_START \|\| iPrev == ASRT_BINOP \|\| iPrev == ASRT_UNARY` |
|       3 | 2348 | `					 \|\| iPrev == ASRT_OPEN \|\| iPrev == ASRT_COMMA ){` |
|       3 | 2349 | `						iCls = ASRT_UNARY;` |
|       2 | 2350 | `					}` |
|      42 | 2351 | `				}else if( pTok->nType & PH7_TK_ID ){` |
|       - | 2352 | `					/* Alpha operators: and/or normalize to php's export spelling;` |
|       - | 2353 | `					 * new/clone read as prefix keywords (operand spacing). */` |
|       3 | 2354 | `					if( nTxt == 3 && SyStrnicmp(zTxt,"and",3) == 0 ){` |
|       3 | 2355 | `						zTxt = "&&"; nTxt = 2;` |
|       1 | 2356 | `					}else if( nTxt == 2 && SyStrnicmp(zTxt,"or",2) == 0 ){` |
|     ! 0 | 2357 | `						zTxt = "\|\|"; nTxt = 2;` |
|     ! 0 | 2358 | `					}else if( (nTxt == 3 && SyStrnicmp(zTxt,"new",3) == 0)` |
|     ! 0 | 2359 | `						\|\| (nTxt == 5 && SyStrnicmp(zTxt,"clone",5) == 0) ){` |
|     ! 0 | 2360 | `						iCls = ASRT_OPERAND;` |
|     ! 0 | 2361 | `					}` |
|       1 | 2362 | `				}` |
|      24 | 2363 | `			}` |
|     139 | 2364 | `		}else if( pTok->nType & (PH7_TK_EQUAL\|PH7_TK_ARRAY_OP\|PH7_TK_COLON\|PH7_TK_AMPER) ){` |
|     ! 0 | 2365 | `			iCls = ASRT_BINOP;` |
|     117 | 2366 | `		}else if( pTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|      35 | 2367 | `			iCls = ASRT_OPERAND;` |
|       - | 2368 | ``			/* `array` `(` — php's AST holds one list node for both spellings and`` |
|       - | 2369 | ``			 * always exports `[...]`. Suppress the keyword (it lexes as a KEYWORD`` |
|       - | 2370 | `			 * token, not an ID); the '(' renders '['. */` |
|      30 | 2371 | `			if( nTxt == 5 && SyStrnicmp(zTxt,"array",5) == 0` |
|      17 | 2372 | `			 && &pIn[1] < pEnd && (pIn[1].nType & PH7_TK_LPAREN) ){` |
|       9 | 2373 | `				bArrayOpen = 1;` |
|       9 | 2374 | `				continue;` |
|       - | 2375 | `			}` |
|      16 | 2376 | `		}else{` |
|       - | 2377 | `			/* keywords (true/false/null/fn/match/...), numbers, strings,` |
|       - | 2378 | `			 * member names, '{'/'}' and anything unforeseen */` |
|      86 | 2379 | `			iCls = ASRT_OPERAND;` |
|       - | 2380 | `		}` |
|       - | 2381 | ``		/* Elvis `? :` — php exports the two-token form as `?:`. */`` |
|     194 | 2382 | `		if( iCls == ASRT_BINOP && nTxt == 1 && zTxt[0] == '?'` |
|      11 | 2383 | `		 && &pIn[1] < pEnd && (pIn[1].nType & PH7_TK_COLON) ){` |
|       3 | 2384 | `			nMark = SyBlobLength(pOut);` |
|       3 | 2385 | `			if( iPrev != ASRT_START && nMark > 0 ){` |
|       3 | 2386 | `				SyBlobAppend(pOut," ",1);` |
|       1 | 2387 | `			}` |
|       3 | 2388 | `			SyBlobAppend(pOut,"?:",2);` |
|       3 | 2389 | `			pIn++; /* consume the ':' */` |
|       3 | 2390 | `			iPrev = ASRT_BINOP;` |
|       3 | 2391 | `			continue;` |
|       - | 2392 | `		}` |
|       - | 2393 | `		/* --- spacing ---------------------------------------------------- */` |
|     197 | 2394 | `		if( iPrev != ASRT_START ){` |
|     134 | 2395 | `			int iAfter = aAsrtAfter[iPrev];` |
|     134 | 2396 | `			int iBefore = aAsrtBefore[iCls];` |
|     130 | 2397 | `			if( iAfter == ASRT_SP_FORCE \|\| iBefore == ASRT_SP_FORCE` |
|      69 | 2398 | `			 \|\| (iAfter == ASRT_SP_WANT && iBefore == ASRT_SP_WANT) ){` |
|      86 | 2399 | `				SyBlobAppend(pOut," ",1);` |
|      42 | 2400 | `			}` |
|      65 | 2401 | `		}` |
|       - | 2402 | `		/* --- emit ------------------------------------------------------- */` |
|     197 | 2403 | `		if( pTok->nType & PH7_TK_LPAREN ){` |
|      15 | 2404 | `			if( nParen < sizeof(aParen) ){` |
|      15 | 2405 | `				aParen[nParen] = (sxu8)bArrayOpen;` |
|       6 | 2406 | `			}` |
|      15 | 2407 | `			nParen++;` |
|      15 | 2408 | `			SyBlobAppend(pOut,bArrayOpen ? "[" : "(",1);` |
|      15 | 2409 | `			bArrayOpen = 0;` |
|     191 | 2410 | `		}else if( pTok->nType & PH7_TK_RPAREN ){` |
|      15 | 2411 | `			int bArr = 0;` |
|      15 | 2412 | `			if( nParen > 0 ){` |
|      15 | 2413 | `				nParen--;` |
|      15 | 2414 | `				if( nParen < sizeof(aParen) ){` |
|      15 | 2415 | `					bArr = aParen[nParen];` |
|       6 | 2416 | `				}` |
|       6 | 2417 | `			}` |
|      15 | 2418 | `			SyBlobAppend(pOut,bArr ? "]" : ")",1);` |
|     178 | 2419 | `		}else if( pTok->nType & (PH7_TK_INTEGER\|PH7_TK_REAL) ){` |
|       - | 2420 | `			char zScratch[GEN_NUM_SCRATCH];` |
|      71 | 2421 | `			char *zAlloc = 0;` |
|       - | 2422 | `			SyString sNum;` |
|     102 | 2423 | `			if( GenStateStripNumericSeparators(&pGen->pVm->sAllocator,&pTok->sData,` |
|      71 | 2424 | `					zScratch,sizeof(zScratch),&sNum,&zAlloc) != SXRET_OK ){` |
|     ! 0 | 2425 | `				SyBlobAppend(pOut,zTxt,nTxt); /* alloc failure: raw text */` |
|      71 | 2426 | `			}else if( pTok->nType & PH7_TK_INTEGER ){` |
|      63 | 2427 | `				ph7_real rOverflow = 0;` |
|      63 | 2428 | `				int bDecimalOverflow = 0;` |
|      63 | 2429 | `				if( GenStateIntLiteralOverflows(&sNum,&rOverflow,&bDecimalOverflow) ){` |
|     ! 0 | 2430 | `					if( bDecimalOverflow ){` |
|     ! 0 | 2431 | `						SyStrToReal(sNum.zString,sNum.nByte,(void *)&rOverflow,0);` |
|     ! 0 | 2432 | `					}` |
|     ! 0 | 2433 | `					AssertRenderReal(pOut,rOverflow);` |
|     ! 0 | 2434 | `				}else{` |
|      63 | 2435 | `					SyBlobFormat(pOut,"%qd",PH7_TokenValueToInt64(&sNum));` |
|       - | 2436 | `				}` |
|      33 | 2437 | `			}else{` |
|       9 | 2438 | `				ph7_real rVal = 0;` |
|       9 | 2439 | `				SyStrToReal(sNum.zString,sNum.nByte,(void *)&rVal,0);` |
|       9 | 2440 | `				AssertRenderReal(pOut,rVal);` |
|       - | 2441 | `			}` |
|      71 | 2442 | `			if( zAlloc ){` |
|     ! 0 | 2443 | `				SyMemBackendFree(&pGen->pVm->sAllocator,zAlloc);` |
|       3 | 2444 | `			}` |
|     138 | 2445 | `		}else if( pTok->nType & (PH7_TK_SSTR\|PH7_TK_NOWDOC) ){` |
|       3 | 2446 | `			AssertRenderSglString(pOut,zTxt,nTxt);` |
|     103 | 2447 | `		}else if( pTok->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
|      15 | 2448 | `			if( SyByteFind(zTxt,nTxt,'$',0) == SXRET_OK ){` |
|       - | 2449 | `				/* Interpolated: php exports its interpolation AST in a` |
|       - | 2450 | `				 * double-quoted form; the raw source is the token-level` |
|       - | 2451 | `				 * equivalent. */` |
|     ! 0 | 2452 | `				SyBlobAppend(pOut,"\"",1);` |
|     ! 0 | 2453 | `				SyBlobAppend(pOut,zTxt,nTxt);` |
|     ! 0 | 2454 | `				SyBlobAppend(pOut,"\"",1);` |
|     ! 0 | 2455 | `			}else{` |
|      15 | 2456 | `				AssertRenderDblString(pOut,zTxt,nTxt);` |
|       - | 2457 | `			}` |
|       9 | 2458 | `		}else{` |
|      90 | 2459 | `			SyBlobAppend(pOut,zTxt,nTxt);` |
|       - | 2460 | `		}` |
|     197 | 2461 | `		iPrev = iCls;` |
|     101 | 2462 | `	}` |
|      67 | 2463 | `}` |
|       - | 2464 |  |
