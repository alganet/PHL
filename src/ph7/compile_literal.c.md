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
|  3825212 |   45 | `static int GenStateFindBadNumericSeparator(` |
|        - |   46 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|        5 |   47 | `{` |
|  3825217 |   48 | `	const char *z = pRaw->zString;` |
|  3825217 |   49 | `	sxu32 n = pRaw->nByte;` |
|  3825217 |   50 | `	int base = 10;` |
|        - |   51 | `	sxu32 i, start;` |
|  3825217 |   52 | `	if( n < 2 ) return 0;` |
|   805787 |   53 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|   104857 |   54 | `		base = 16;` |
|   753361 |   55 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|      286 |   56 | `		base = 2;` |
|      142 |   57 | `	}` |
|  3068443 |   58 | `	for( i = 0; i < n; ++i ){` |
|  2262675 |   59 | `		if( z[i] != '_' ) continue;` |
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
|   805773 |   76 | `	return 0;` |
|  1912611 |   77 | `}` |
|        - |   78 | `/*` |
|        - |   79 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|        - |   80 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|        - |   81 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|        - |   82 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|        - |   83 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|        - |   84 | ` * so callers can bail from the current construct).` |
|        - |   85 | ` */` |
|  3825212 |   86 | `PH7_PRIVATE sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|        5 |   87 | `{` |
|  3825217 |   88 | `	const char *zBad = 0;` |
|  3825217 |   89 | `	sxu32 nBad = 0;` |
|        - |   90 | `	SyString sBad;` |
|        - |   91 | `	sxi32 rc;` |
|  3825217 |   92 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|  3825203 |   93 | `		return SXRET_OK;` |
|        - |   94 | `	}` |
|       18 |   95 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|       18 |   96 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|        - |   97 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|       18 |   98 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |   99 | `		return SXERR_ABORT;` |
|        - |  100 | `	}` |
|       18 |  101 | `	return SXERR_SYNTAX;` |
|  1912611 |  102 | `}` |
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
|  3825198 |  119 | `PH7_PRIVATE sxi32 GenStateStripNumericSeparators(` |
|        - |  120 | `	SyMemBackend *pAlloc,` |
|        - |  121 | `	const SyString *pToken,` |
|        - |  122 | `	char *zScratch, sxu32 nScratch,` |
|        - |  123 | `	SyString *pOut, char **pzAlloc)` |
|        5 |  124 | `{` |
|        - |  125 | `	sxu32 i, j;` |
|  3825203 |  126 | `	int hasUnderscore = 0;` |
|        - |  127 | `	char *zBuf;` |
|  3825203 |  128 | `	*pzAlloc = 0;` |
|  9105213 |  129 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|  5280269 |  130 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|  2640010 |  131 | `	}` |
|  3825203 |  132 | `	if( !hasUnderscore ){` |
|  3824949 |  133 | `		SyStringDupPtr(pOut, pToken);` |
|  3824949 |  134 | `		return SXRET_OK;` |
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
|  1912604 |  151 | `}` |
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
|  3816458 |  187 | `static int GenStateIntLiteralOverflows(const SyString *pNum, ph7_real *pReal, int *pbDecimal)` |
|        5 |  188 | `{` |
|  3816463 |  189 | `	const char *z = pNum->zString;` |
|  3816463 |  190 | `	const char *zEnd = z + pNum->nByte;` |
|        - |  191 | `	const char *p, *q;` |
|        - |  192 | `	int n;` |
|  3816463 |  193 | `	*pbDecimal = FALSE;` |
|  3816463 |  194 | `	if( z >= zEnd ){` |
|      ! 0 |  195 | `		return FALSE;` |
|        - |  196 | `	}` |
|  3816463 |  197 | `	if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|        - |  198 | `		/* Hexadecimal: INT64_MAX == 0x7FFF...F (16 digits, leading nibble 7). */` |
|   104855 |  199 | `		p = z + 2;` |
|   132023 |  200 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|   427369 |  201 | `		for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisHex(q[0]); q++ ){ n++; }` |
|   104855 |  202 | `		if( n < 16 \|\| (n == 16 && SyHexToint(p[0]) < 8) ){` |
|   104849 |  203 | `			return FALSE;` |
|        - |  204 | `		}` |
|        7 |  205 | `		{ ph7_real dv = 0;` |
|      103 |  206 | `		  for( q = p; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisHex(q[0]); q++ ){` |
|       97 |  207 | `			dv = dv * 16 + (ph7_real)SyHexToint(q[0]);` |
|       49 |  208 | `		  }` |
|        7 |  209 | `		  *pReal = dv;` |
|        - |  210 | `		}` |
|        7 |  211 | `		return TRUE;` |
|  3711613 |  212 | `	}else if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'b' \|\| z[1] == 'B') ){` |
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
|  3711331 |  227 | `	}else if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'o' \|\| z[1] == 'O') ){` |
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
|  3711315 |  242 | `	}else if( z[0] == '0' ){` |
|        - |  243 | `		/* Octal: INT64_MAX == 0o777...7 (21 significant octal digits). Skip the` |
|        - |  244 | `		 * leading zeros (incl. the base '0'); a non-octal char such as the 8.1` |
|        - |  245 | `		 * "0o" marker ends the run and leaves it to the int path (as today). */` |
|  1395355 |  246 | `		p = z;` |
|  2790707 |  247 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|  1407251 |  248 | `		for( q = p, n = 0; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){ n++; }` |
|  1395355 |  249 | `		if( n <= 21 ){` |
|  1395353 |  250 | `			return FALSE;` |
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
|  2315965 |  263 | `	p = z;` |
|  2315965 |  264 | `	while( p < zEnd && p[0] == '0' ){ p++; }` |
|  5600123 |  265 | `	for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisDigit(q[0]); q++ ){ n++; }` |
|  2315965 |  266 | `	if( n > 19 \|\| (n == 19 && SyMemcmp(p, "9223372036854775807", 19) > 0) ){` |
|       25 |  267 | `		*pbDecimal = TRUE;` |
|       25 |  268 | `		return TRUE;` |
|        - |  269 | `	}` |
|  2315941 |  270 | `	return FALSE;` |
|  1908234 |  271 | `}` |
|  3825184 |  272 | `PH7_PRIVATE sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  273 | `{` |
|  3825189 |  274 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|  3825189 |  275 | `	sxu32 nIdx = 0;` |
|        - |  276 | `	char zScratch[GEN_NUM_SCRATCH];` |
|  3825189 |  277 | `	char *zAlloc = 0;` |
|        - |  278 | `	SyString sNum;` |
|        - |  279 | `	sxi32 rc;` |
|  1912592 |  280 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|  3825189 |  281 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|  3825189 |  282 | `	if( rc != SXRET_OK ){` |
|       14 |  283 | `		return rc;` |
|        - |  284 | `	}` |
|  5737766 |  285 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|  1912587 |  286 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|  3825179 |  287 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  288 | `		return SXERR_ABORT;` |
|        - |  289 | `	}` |
|  3825179 |  290 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|        - |  291 | `		ph7_value *pObj;` |
|        - |  292 | `		sxi64 iValue;` |
|  3816463 |  293 | `		ph7_real rOverflow = 0;` |
|  3816463 |  294 | `		int bDecimalOverflow = 0;` |
|  3816463 |  295 | `		if( GenStateIntLiteralOverflows(&sNum,&rOverflow,&bDecimalOverflow) ){` |
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
|  3816429 |  312 | `			iValue = PH7_TokenValueToInt64(&sNum);` |
|  3816429 |  313 | `			pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|  3816429 |  314 | `			if( pObj == 0 ){` |
|      ! 0 |  315 | `				if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|      ! 0 |  316 | `				return SXERR_ABORT;` |
|        - |  317 | `			}` |
|  3816429 |  318 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|        - |  319 | `		}` |
|  1908234 |  320 | `	}else{` |
|        - |  321 | `		/* Real number */` |
|        - |  322 | `		ph7_value *pObj;` |
|        - |  323 | `		/* Reserve a new constant */` |
|     8721 |  324 | `		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     8721 |  325 | `		if( pObj == 0 ){` |
|      ! 0 |  326 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |  327 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|      ! 0 |  328 | `			return SXERR_ABORT;` |
|        - |  329 | `		}` |
|     8721 |  330 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|     8721 |  331 | `		PH7_MemObjToReal(pObj);` |
|        - |  332 | `	}` |
|  3825179 |  333 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|        - |  334 | `	/* Emit the load constant instruction */` |
|  3825179 |  335 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        - |  336 | `	/* Node successfully compiled */` |
|  3825179 |  337 | `	return SXRET_OK;` |
|  1912597 |  338 | `}` |
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
|  5561418 |  350 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  351 | `{` |
|  5561423 |  352 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|        - |  353 | `	const char *zIn,*zCur,*zEnd;` |
|        - |  354 | `	ph7_value *pObj;` |
|        - |  355 | `	sxu32 nIdx;` |
|        - |  356 | `	sxi32 bHasEsc;` |
|  5561423 |  357 | `	nIdx = 0; /* Prevent compiler warning */` |
|        - |  358 | `	/* Delimit the string */` |
|  5561423 |  359 | `	zIn  = pStr->zString;` |
|  5561423 |  360 | `	zEnd = &zIn[pStr->nByte];` |
|  5561423 |  361 | `	if( zIn >= zEnd ){` |
|        - |  362 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|        - |  363 | `		 * rather than reserving a new object each time. */` |
|   407625 |  364 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|   407625 |  365 | `		return SXRET_OK;` |
|        - |  366 | `	}` |
|        - |  367 | `	/* A single-quoted literal whose raw source holds a backslash unescapes to a` |
|        - |  368 | `	 * value that differs from that source (\\ -> \, \' -> '). The literal cache` |
|        - |  369 | `	 * keys FIND on the raw source text but INSTALL on the unescaped value, so` |
|        - |  370 | `	 * caching such a literal lets an unrelated one collide with it — e.g. '\\'` |
|        - |  371 | `	 * (raw \\, value \) would find the entry installed for '\\\\' (raw \\\\,` |
|        - |  372 | `	 * value \\) and load two backslashes. Only cache literals whose value equals` |
|        - |  373 | `	 * their source, i.e. those with no backslash to unescape. */` |
|  5153803 |  374 | `	bHasEsc = 0;` |
|        - |  375 | `	{` |
|        - |  376 | `		const char *zScan;` |
| 61585927 |  377 | `		for( zScan = zIn ; zScan < zEnd ; zScan++ ){` |
| 56513791 |  378 | `			if( zScan[0] == '\\' ){ bHasEsc = 1; break; }` |
| 28216067 |  379 | `		}` |
|        - |  380 | `	}` |
|  5153803 |  381 | `	if( !bHasEsc && SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|        - |  382 | `		/* Already processed,emit the load constant instruction` |
|        - |  383 | `		 * and return.` |
|        - |  384 | `		 */` |
|  3007513 |  385 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|  3007513 |  386 | `		return SXRET_OK;` |
|        - |  387 | `	}` |
|        - |  388 | `	/* Reserve a new constant */` |
|  2146295 |  389 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  2146295 |  390 | `	if( pObj == 0 ){` |
|      ! 0 |  391 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |  392 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|      ! 0 |  393 | `		return SXERR_ABORT;` |
|        - |  394 | `	}` |
|  2146295 |  395 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|        - |  396 | `	/* Compile the node */` |
|  2198780 |  397 | `	for(;;){` |
|  4397565 |  398 | `		if( zIn >= zEnd ){` |
|        - |  399 | `			/* End of input */` |
|  2146295 |  400 | `			break;` |
|        - |  401 | `		}` |
|  2251275 |  402 | `		zCur = zIn;` |
| 43501889 |  403 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
| 41250619 |  404 | `			zIn++;` |
|        5 |  405 | `		}` |
|  2251275 |  406 | `		if( zIn > zCur ){` |
|        - |  407 | `			/* Append raw contents*/` |
|  2208521 |  408 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|  1104258 |  409 | `		}` |
|  2251275 |  410 | `		zIn++;` |
|  2251275 |  411 | `		if( zIn < zEnd ){` |
|   143829 |  412 | `			if( zIn[0] == '\\' ){` |
|        - |  413 | `				/* A literal backslash */` |
|    34993 |  414 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|   126335 |  415 | `			}else if( zIn[0] == '\'' ){` |
|        - |  416 | `				/* A single quote */` |
|       15 |  417 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|        8 |  418 | `			}else{` |
|        - |  419 | `				/* verbatim copy */` |
|   108827 |  420 | `				zIn--;` |
|   108827 |  421 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char)*2);` |
|   108827 |  422 | `				zIn++;` |
|        - |  423 | `			}` |
|    71912 |  424 | `		}` |
|        - |  425 | `		/* Advance the stream cursor */` |
|  2251275 |  426 | `		zIn++;` |
|        5 |  427 | `	}` |
|        - |  428 | `	/* Emit the load constant instruction */` |
|  2146295 |  429 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|  2146295 |  430 | `	if( !bHasEsc && pStr->nByte < 1024 ){` |
|        - |  431 | `		/* Install in the literal table (only when value == source; see above) */` |
|  2064633 |  432 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|  1032314 |  433 | `	}` |
|        - |  434 | `	/* Node successfully compiled */` |
|  2146295 |  435 | `	return SXRET_OK;` |
|  2780714 |  436 | `}` |
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
|     2354 |  603 | `static sxi32 GenStateProcessStringExpression(` |
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
|     2359 |  614 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|        - |  615 | `	/* Preallocate some slots */` |
|     2359 |  616 | `	SySetAlloc(&sToken,0x08);` |
|        - |  617 | `	/* Tokenize the text */` |
|     2359 |  618 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken,0);` |
|        - |  619 | `	/* Swap delimiter */` |
|     2359 |  620 | `	pTmpIn  = pGen->pIn;` |
|     2359 |  621 | `	pTmpEnd = pGen->pEnd;` |
|     2359 |  622 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|     2359 |  623 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|        - |  624 | `	/* Compile the expression */` |
|     2359 |  625 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - |  626 | `	/* Restore token stream */` |
|     2359 |  627 | `	pGen->pIn  = pTmpIn;` |
|     2359 |  628 | `	pGen->pEnd = pTmpEnd;` |
|        - |  629 | `	/* Release the token set */` |
|     2359 |  630 | `	SySetRelease(&sToken);` |
|        - |  631 | `	/* Compilation result */` |
|     2359 |  632 | `	return rc;` |
|        5 |  633 | `}` |
|        - |  634 | `/*` |
|        - |  635 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|        - |  636 | ` */` |
|   126262 |  637 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|        5 |  638 | `{` |
|        - |  639 | `	ph7_value *pConstObj;` |
|   126267 |  640 | `	sxu32 nIdx = 0;` |
|        - |  641 | `	/* Reserve a new constant */` |
|   126267 |  642 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   126267 |  643 | `	if( pConstObj == 0 ){` |
|      ! 0 |  644 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|      ! 0 |  645 | `		return 0;` |
|        - |  646 | `	}` |
|   126267 |  647 | `	(*pCount)++;` |
|   126267 |  648 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|        - |  649 | `	/* Emit the load constant instruction */` |
|   126267 |  650 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   126267 |  651 | `	return pConstObj;` |
|    63136 |  652 | `}` |
|        - |  653 | `/*` |
|        - |  654 | ` * Compile a double quoted/heredoc string.` |
|        - |  655 | ` * According to the PHP language reference manual` |
|        - |  656 | ` * Heredoc` |
|        - |  657 | ` *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier` |
|        - |  658 | ` *  is provided, then a newline. The string itself follows, and then the same identifier again` |
|        - |  659 | ` *  to close the quotation.` |
|        - |  660 | ` *  The closing identifier must begin in the first column of the line. Also, the identifier must` |
|        - |  661 | ` *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric` |
|        - |  662 | ` *  characters and underscores, and must start with a non-digit character or underscore.` |
|        - |  663 | ` *  Warning` |
|        - |  664 | ` *  It is very important to note that the line with the closing identifier must contain` |
|        - |  665 | ` *  no other characters, except possibly a semicolon (;). That means especially that the identifier` |
|        - |  666 | ` *  may not be indented, and there may not be any spaces or tabs before or after the semicolon.` |
|        - |  667 | ` *  It's also important to realize that the first character before the closing identifier must` |
|        - |  668 | ` *  be a newline as defined by the local operating system. This is \n on UNIX systems, including Mac OS X.` |
|        - |  669 | ` *  The closing delimiter (possibly followed by a semicolon) must also be followed by a newline.` |
|        - |  670 | ` *  If this rule is broken and the closing identifier is not "clean", it will not be considered a closing` |
|        - |  671 | ` *  identifier, and PHP will continue looking for one. If a proper closing identifier is not found before` |
|        - |  672 | ` *  the end of the current file, a parse error will result at the last line.` |
|        - |  673 | ` *  Heredocs can not be used for initializing class properties.` |
|        - |  674 | ` * Double quoted` |
|        - |  675 | ` *  If the string is enclosed in double-quotes ("), PHP will interpret more escape sequences for special characters:` |
|        - |  676 | ` *  Escaped characters Sequence 	Meaning` |
|        - |  677 | ` *  \n linefeed (LF or 0x0A (10) in ASCII)` |
|        - |  678 | ` *  \r carriage return (CR or 0x0D (13) in ASCII)` |
|        - |  679 | ` *  \t horizontal tab (HT or 0x09 (9) in ASCII)` |
|        - |  680 | ` *  \v vertical tab (VT or 0x0B (11) in ASCII)` |
|        - |  681 | ` *  \e escape (ESC or 0x1B (27) in ASCII)` |
|        - |  682 | ` *  \f form feed (FF or 0x0C (12) in ASCII)` |
|        - |  683 | ` *  \\ backslash` |
|        - |  684 | ` *  \$ dollar sign` |
|        - |  685 | ` *  \" double-quote` |
|        - |  686 | ` *  \[0-7]{1,3} 	the sequence of characters matching the regular expression is a character in octal notation,` |
|        - |  687 | ` *      which silently overflows to fit in a byte (e.g. "\400" === "\000")` |
|        - |  688 | ` *  \x[0-9A-Fa-f]{1,2} 	the sequence of characters matching the regular expression is a character in hexadecimal notation` |
|        - |  689 | ` *  \u{[0-9A-Fa-f]+} 	the sequence of characters matching the regular expression is a Unicode codepoint,` |
|        - |  690 | ` *      which will be output to the string as that codepoint's UTF-8 representation` |
|        - |  691 | ` * As in single quoted strings, escaping any other character will result in the backslash being printed too.` |
|        - |  692 | ` * (The PH7-ism "\oNNN" octal form is gone: a literal "\o" now round-trips like php 8.)` |
|        - |  693 | ` * The most important feature of double-quoted strings is the fact that variable names will be expanded.` |
|        - |  694 | ` * See string parsing for details.` |
|        - |  695 | ` */` |
|        - |  696 | `/*` |
|        - |  697 | ` * Line number of an escape sequence inside the string body being compiled:` |
|        - |  698 | ` * the token's line plus every newline before the escape (php reports the` |
|        - |  699 | ` * escape's own line, not the string's opening line). A heredoc body starts` |
|        - |  700 | ` * on the line after the '<<<' marker, hence the +1.` |
|        - |  701 | ` */` |
|        6 |  702 | `static sxu32 GenStateStringEscLine(ph7_gen_state *pGen,const char *zPos,int bHeredoc)` |
|        3 |  703 | `{` |
|        9 |  704 | `	const char *z = pGen->pIn->sData.zString;` |
|        9 |  705 | `	sxu32 nLine = pGen->pIn->nLine + (bHeredoc ? 1 : 0);` |
|       15 |  706 | `	for( ; z < zPos ; z++ ){` |
|        9 |  707 | `		if( z[0] == '\n' ){` |
|      ! 0 |  708 | `			nLine++;` |
|      ! 0 |  709 | `		}` |
|        6 |  710 | `	}` |
|        9 |  711 | `	return nLine;` |
|        3 |  712 | `}` |
|        - |  713 | `/* bHeredoc: php strips the backslash from '\"' only when '"' is the active` |
|        - |  714 | ` * quote character; a heredoc has none, so '\"' stays verbatim there. */` |
|   124978 |  715 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen,int bHeredoc)` |
|        5 |  716 | `{` |
|   124983 |  717 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|        - |  718 | `	const char *zIn,*zCur,*zEnd;` |
|   124983 |  719 | `	ph7_value *pObj = 0;` |
|        - |  720 | `	sxi32 iCons;` |
|        - |  721 | `	sxi32 rc;` |
|        - |  722 | `	/* Delimit the string */` |
|   124983 |  723 | `	zIn  = pStr->zString;` |
|   124983 |  724 | `	zEnd = &zIn[pStr->nByte];` |
|   124983 |  725 | `	if( zIn >= zEnd ){` |
|        - |  726 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|        - |  727 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|        - |  728 | `		 * literal table from growing when many "" literals appear in the source.` |
|        - |  729 | `		 */` |
|      407 |  730 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|      407 |  731 | `		return SXRET_OK;` |
|        - |  732 | `	}` |
|   124581 |  733 | `	zCur = 0;` |
|        - |  734 | `	/* Compile the node */` |
|   124581 |  735 | `	iCons = 0;` |
|    63464 |  736 | `	for(;;){` |
|   171285 |  737 | `		zCur = zIn;` |
|  1678709 |  738 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  1509785 |  739 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|       84 |  740 | `				break;` |
|  1509628 |  741 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|     2204 |  742 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|     1102 |  743 | `					break;` |
|        - |  744 | `			}` |
|  1507429 |  745 | `			zIn++;` |
|        5 |  746 | `		}` |
|   171285 |  747 | `		if( zIn > zCur ){` |
|    95677 |  748 | `			if( pObj == 0 ){` |
|    95025 |  749 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    95025 |  750 | `				if( pObj == 0 ){` |
|      ! 0 |  751 | `					return SXERR_ABORT;` |
|        - |  752 | `				}` |
|    47510 |  753 | `			}` |
|    95677 |  754 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    47836 |  755 | `		}` |
|   171285 |  756 | `		if( zIn >= zEnd ){` |
|   124577 |  757 | `			break;` |
|        - |  758 | `		}` |
|    46713 |  759 | `		if( zIn[0] == '\\' ){` |
|    44357 |  760 | `			const char *zPtr = 0;` |
|        - |  761 | `			sxu32 n;` |
|    44357 |  762 | `			zIn++;` |
|    44357 |  763 | `			if( pObj == 0 ){` |
|    31247 |  764 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    31247 |  765 | `				if( pObj == 0 ){` |
|      ! 0 |  766 | `					return SXERR_ABORT;` |
|        - |  767 | `				}` |
|    15621 |  768 | `			}` |
|    44357 |  769 | `			if( zIn >= zEnd ){` |
|        - |  770 | `				/* Lone backslash at the very end of the body: php keeps it */` |
|        3 |  771 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|        3 |  772 | `				break;` |
|        - |  773 | `			}` |
|    44355 |  774 | `			n = sizeof(char); /* size of conversion */` |
|    44355 |  775 | `			switch( zIn[0] ){` |
|       17 |  776 | `			case '$':` |
|        - |  777 | `				/* Dollar sign */` |
|       37 |  778 | `				PH7_MemObjStringAppend(pObj,"$",sizeof(char));` |
|       37 |  779 | `				break;` |
|       62 |  780 | `			case '\\':` |
|        - |  781 | `				/* A literal backslash */` |
|      129 |  782 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|      129 |  783 | `				break;` |
|        1 |  784 | `			case 'e':` |
|        - |  785 | `				/* Escape (ESC) ASCII code 27 */` |
|        3 |  786 | `				PH7_MemObjStringAppend(pObj,"\x1b",sizeof(char));` |
|        3 |  787 | `				break;` |
|        4 |  788 | `			case 'f':` |
|        - |  789 | `				/* Form-feed (FF)[ctrl+l] ASCII code 12 */` |
|        9 |  790 | `				PH7_MemObjStringAppend(pObj,"\f",sizeof(char));` |
|        9 |  791 | `				break;` |
|    19667 |  792 | `			case 'n':` |
|        - |  793 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|    39339 |  794 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|    39339 |  795 | `				break;` |
|       27 |  796 | `			case 'r':` |
|        - |  797 | `				/* Carriage return (CR)[ctrl+m] ASCII code 13 */` |
|       59 |  798 | `				PH7_MemObjStringAppend(pObj,"\r",sizeof(char));` |
|       59 |  799 | `				break;` |
|     1971 |  800 | `			case 't':` |
|        - |  801 | `				/* Horizontal tab (HT)[ctrl+i] ASCII code 9 */` |
|     3947 |  802 | `				PH7_MemObjStringAppend(pObj,"\t",sizeof(char));` |
|     3947 |  803 | `				break;` |
|        3 |  804 | `			case 'v':` |
|        - |  805 | `				/* Vertical tab(VT)[ctrl+k] ASCII code 11 */` |
|        7 |  806 | `				PH7_MemObjStringAppend(pObj,"\v",sizeof(char));` |
|        7 |  807 | `				break;` |
|      147 |  808 | `			case '"':` |
|      299 |  809 | `				if( bHeredoc ){` |
|        - |  810 | `					/* No active quote char in a heredoc: php keeps \" verbatim */` |
|        5 |  811 | `					PH7_MemObjStringAppend(pObj,"\\\"",sizeof(char)*2);` |
|        3 |  812 | `				}else{` |
|        - |  813 | `					/* Double quote */` |
|      295 |  814 | `					PH7_MemObjStringAppend(pObj,"\"",sizeof(char));` |
|        - |  815 | `				}` |
|      299 |  816 | `				break;` |
|       24 |  817 | `			case '0': case '1': case '2': case '3':` |
|        - |  818 | `			case '4': case '5': case '6': case '7': {` |
|        - |  819 | `				/* \[0-7]{1,3}: a character in octal notation. A value above \377` |
|        - |  820 | `				 * warns and wraps to the low byte, matching php 8. */` |
|       50 |  821 | `				int c = 0;` |
|        - |  822 | `				char cOut;` |
|      144 |  823 | `				for( zPtr = zIn ; zPtr < &zIn[3*sizeof(char)] ; zPtr++ ){` |
|      122 |  824 | `					if( zPtr >= zEnd \|\| zPtr[0] < '0' \|\| zPtr[0] > '7' ){` |
|       14 |  825 | `						break;` |
|        - |  826 | `					}` |
|       96 |  827 | `					c = c * 8 + (zPtr[0] - '0');` |
|       49 |  828 | `				}` |
|       50 |  829 | `				if( c > 0xFF ){` |
|        - |  830 | `					SyString sSeq;` |
|        3 |  831 | `					SyStringInitFromBuf(&sSeq,zIn,(sxu32)(zPtr-zIn));` |
|        3 |  832 | `					PH7_GenCompileError(&(*pGen),E_WARNING,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|        - |  833 | `						"Octal escape sequence overflow \\%z is greater than \\377",&sSeq);` |
|        3 |  834 | `					c &= 0xFF;` |
|        1 |  835 | `				}` |
|       50 |  836 | `				cOut = (char)c; /* value byte, independent of host endianness */` |
|       50 |  837 | `				PH7_MemObjStringAppend(pObj,&cOut,sizeof(char));` |
|       50 |  838 | `				n = (sxu32)(zPtr-zIn);` |
|       50 |  839 | `				break;` |
|        - |  840 | `			}` |
|      228 |  841 | `			case 'x':` |
|      684 |  842 | `				if( &zIn[1] < zEnd && SyisHex((unsigned char)zIn[1]) ){` |
|        - |  843 | `					/* \x[0-9A-Fa-f]{1,2}: a character in hexadecimal notation */` |
|      454 |  844 | `					int c = SyHexToint(zIn[1]);` |
|        - |  845 | `					char cOut;` |
|      454 |  846 | `					n += sizeof(char);` |
|      454 |  847 | `					if( &zIn[2] < zEnd && SyisHex((unsigned char)zIn[2]) ){` |
|      450 |  848 | `						c = (c << 4) + SyHexToint(zIn[2]);` |
|      450 |  849 | `						n += sizeof(char);` |
|      224 |  850 | `					}` |
|      454 |  851 | `					cOut = (char)c; /* value byte, independent of host endianness */` |
|      454 |  852 | `					PH7_MemObjStringAppend(pObj,&cOut,sizeof(char));` |
|      228 |  853 | `				}else{` |
|        - |  854 | `					/* Not an escape: keep the backslash, as php does */` |
|        5 |  855 | `					PH7_MemObjStringAppend(pObj,"\\x",sizeof(char)*2);` |
|        - |  856 | `				}` |
|      458 |  857 | `				break;` |
|        9 |  858 | `			case 'u':` |
|       18 |  859 | `				if( &zIn[1] < zEnd && zIn[1] == '{'` |
|       22 |  860 | `				 && !(&zIn[2] < zEnd && zIn[2] == '$') ){` |
|        - |  861 | `					/* \u{codepoint}: UTF-8 encoding of the given codepoint (php 7+).` |
|        - |  862 | `					 * php encodes surrogates verbatim, so the only invalid value` |
|        - |  863 | `					 * is > U+10FFFF; malformed/empty braces are a compile error.` |
|        - |  864 | `					 * "\u{$..." is excluded above: php treats it as a literal \u` |
|        - |  865 | `					 * followed by {$...} curly interpolation. */` |
|       15 |  866 | `					sxu32 nCp = 0;` |
|       15 |  867 | `					zPtr = &zIn[2];` |
|       59 |  868 | `					while( zPtr < zEnd && SyisHex((unsigned char)zPtr[0]) ){` |
|       46 |  869 | `						if( nCp <= 0x10FFFF ){` |
|        - |  870 | `							/* stop accumulating once out of range: keeps a long` |
|        - |  871 | `							 * digit run from wrapping sxu32 */` |
|       46 |  872 | `							nCp = nCp * 16 + (sxu32)SyHexToint(zPtr[0]);` |
|       22 |  873 | `						}` |
|       46 |  874 | `						zPtr++;` |
|        2 |  875 | `					}` |
|       15 |  876 | `					if( zPtr == &zIn[2] \|\| zPtr >= zEnd \|\| zPtr[0] != '}' ){` |
|        - |  877 | `						/* Error recorded (nErr>0 fails the whole compile); consume the` |
|        - |  878 | `						 * malformed sequence so later errors are still reported. */` |
|        3 |  879 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|        - |  880 | `							"Invalid UTF-8 codepoint escape sequence");` |
|        3 |  881 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 |  882 | `							return SXERR_ABORT;` |
|        - |  883 | `						}` |
|        3 |  884 | `						n = (sxu32)(zPtr-zIn);` |
|        3 |  885 | `						if( zPtr < zEnd && zPtr[0] == '}' ){` |
|        3 |  886 | `							n += sizeof(char);` |
|        1 |  887 | `						}` |
|        3 |  888 | `						break;` |
|        - |  889 | `					}` |
|       12 |  890 | `					n = (sxu32)(&zPtr[1]-zIn); /* 'u{...}' incl. closing brace */` |
|       12 |  891 | `					if( nCp > 0x10FFFF ){` |
|        3 |  892 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|        - |  893 | `							"Invalid UTF-8 codepoint escape sequence: Codepoint too large");` |
|        3 |  894 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 |  895 | `							return SXERR_ABORT;` |
|        - |  896 | `						}` |
|        3 |  897 | `						break;` |
|        - |  898 | `					}` |
|        - |  899 | `					{` |
|        - |  900 | `						char zUtf[4];` |
|        9 |  901 | `						sxu8 *zOut = (sxu8 *)zUtf;` |
|        9 |  902 | `						SX_WRITE_UTF8(zOut,nCp);` |
|        9 |  903 | `						PH7_MemObjStringAppend(pObj,zUtf,(sxu32)(zOut-(sxu8 *)zUtf));` |
|        - |  904 | `					}` |
|        5 |  905 | `				}else{` |
|        - |  906 | `					/* Not an escape: keep the backslash, as php does */` |
|        7 |  907 | `					PH7_MemObjStringAppend(pObj,"\\u",sizeof(char)*2);` |
|        - |  908 | `				}` |
|       15 |  909 | `				break;` |
|       15 |  910 | `			default:` |
|        - |  911 | `				/* Unrecognized escape: keep the backslash, as php does.` |
|        - |  912 | `				 * zIn[-1] is the backslash itself, so both bytes are contiguous` |
|        - |  913 | `				 * in the source buffer — one batched append. */` |
|       31 |  914 | `				PH7_MemObjStringAppend(pObj,&zIn[-1],sizeof(char)*2);` |
|       30 |  915 | `				break;` |
|        - |  916 | `			}` |
|        - |  917 | `			/* Advance the stream cursor */` |
|    44355 |  918 | `			zIn += n;` |
|    44355 |  919 | `			continue;` |
|        - |  920 | `		}` |
|     2361 |  921 | `		if( zIn[0] == '{' ){` |
|        - |  922 | `			/* Curly syntax */` |
|        - |  923 | `			const char *zExpr;` |
|      165 |  924 | `			sxi32 iNest = 1;` |
|      165 |  925 | `			zIn++;` |
|      165 |  926 | `			zExpr = zIn;` |
|        - |  927 | `			/* Synchronize with the next closing curly braces */` |
|     1523 |  928 | `			while( zIn < zEnd ){` |
|     1523 |  929 | `				if( zIn[0] == '{' ){` |
|        - |  930 | `					/* Increment nesting level */` |
|        3 |  931 | `					iNest++;` |
|     1522 |  932 | `				}else if(zIn[0] == '}' ){` |
|        - |  933 | `					/* Decrement nesting level */` |
|      167 |  934 | `					iNest--;` |
|      167 |  935 | `					if( iNest <= 0 ){` |
|      165 |  936 | `						break;` |
|        - |  937 | `					}` |
|        1 |  938 | `				}` |
|     1361 |  939 | `				zIn++;` |
|        3 |  940 | `			}` |
|        - |  941 | `			/* Process the expression */` |
|      165 |  942 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|      165 |  943 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  944 | `				return SXERR_ABORT;` |
|        - |  945 | `			}` |
|      165 |  946 | `			if( rc != SXERR_EMPTY ){` |
|      165 |  947 | `				++iCons;` |
|       81 |  948 | `			}` |
|      165 |  949 | `			if( zIn < zEnd ){` |
|        - |  950 | `				/* Jump the trailing curly */` |
|      165 |  951 | `				zIn++;` |
|       81 |  952 | `			}` |
|       84 |  953 | `		}else{` |
|        - |  954 | `			/* Simple syntax */` |
|     2199 |  955 | `			const char *zExpr = zIn;` |
|        - |  956 | `			/* Assemble variable name */` |
|     1122 |  957 | `			for(;;){` |
|        - |  958 | `				/* Jump leading dollars */` |
|     4443 |  959 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|     2199 |  960 | `					zIn++;` |
|        5 |  961 | `				}` |
|     1122 |  962 | `				for(;;){` |
|    10737 |  963 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|     7371 |  964 | `						zIn++;` |
|        5 |  965 | `					}` |
|     2249 |  966 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|        - |  967 | `						/* UTF-8 stream */` |
|      ! 0 |  968 | `						zIn++;` |
|      ! 0 |  969 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  970 | `							zIn++;` |
|      ! 0 |  971 | `						}` |
|      ! 0 |  972 | `						continue;` |
|        - |  973 | `					}` |
|     2249 |  974 | `					break;` |
|      ! 0 |  975 | `				}` |
|     2249 |  976 | `				if( zIn >= zEnd ){` |
|      277 |  977 | `					break;` |
|        - |  978 | `				}` |
|     1977 |  979 | `				if( zIn[0] == '[' ){` |
|       12 |  980 | `					sxi32 iSquare = 1;` |
|       12 |  981 | `					zIn++;` |
|       28 |  982 | `					while( zIn < zEnd ){` |
|       28 |  983 | `						if( zIn[0] == '[' ){` |
|      ! 0 |  984 | `							iSquare++;` |
|       28 |  985 | `						}else if (zIn[0] == ']' ){` |
|       12 |  986 | `							iSquare--;` |
|       12 |  987 | `							if( iSquare <= 0 ){` |
|       12 |  988 | `								break;` |
|        - |  989 | `							}` |
|      ! 0 |  990 | `						}` |
|       18 |  991 | `						zIn++;` |
|        2 |  992 | `					}` |
|       12 |  993 | `					if( zIn < zEnd ){` |
|       12 |  994 | `						zIn++;` |
|        5 |  995 | `					}` |
|       12 |  996 | `					break;` |
|     1967 |  997 | `				}else if(zIn[0] == '{' ){` |
|        3 |  998 | `					sxi32 iCurly = 1;` |
|        3 |  999 | `					zIn++;` |
|       11 | 1000 | `					while( zIn < zEnd ){` |
|       11 | 1001 | `						if( zIn[0] == '{' ){` |
|      ! 0 | 1002 | `							iCurly++;` |
|       11 | 1003 | `						}else if (zIn[0] == '}' ){` |
|        3 | 1004 | `							iCurly--;` |
|        3 | 1005 | `							if( iCurly <= 0 ){` |
|        3 | 1006 | `								break;` |
|        - | 1007 | `							}` |
|      ! 0 | 1008 | `						}` |
|        9 | 1009 | `						zIn++;` |
|        1 | 1010 | `					}` |
|        3 | 1011 | `					if( zIn < zEnd ){` |
|        3 | 1012 | `						zIn++;` |
|        1 | 1013 | `					}` |
|        3 | 1014 | `					break;` |
|     1965 | 1015 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|        - | 1016 | `					/* Member access operator '->' */` |
|       53 | 1017 | `					zIn += 2;` |
|     1940 | 1018 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|        - | 1019 | `					/* Static member access operator '::' */` |
|      ! 0 | 1020 | `					zIn += 2;` |
|      ! 0 | 1021 | `				}else{` |
|      960 | 1022 | `					break;` |
|        - | 1023 | `				}` |
|        3 | 1024 | `			}` |
|        - | 1025 | `			/*` |
|        - | 1026 | `			 * "$a[name]" — php's SIMPLE syntax takes an unquoted subscript as the string key` |
|        - | 1027 | `			 * 'name', never as a constant. PH7 handed "$a[name]" straight to the expression` |
|        - | 1028 | `			 * compiler, where the bare word only resolved because an unknown constant used to` |
|        - | 1029 | `			 * fall back to its own name as a string. With undefined constants now a real` |
|        - | 1030 | `			 * Error, quote the key here so the simple syntax keeps meaning what php means.` |
|        - | 1031 | `			 * A numeric ($a[0]) or variable ($a[$k]) subscript is already unambiguous.` |
|        - | 1032 | `			 */` |
|        - | 1033 | `			{` |
|     2199 | 1034 | `				const char *zBr = zExpr;` |
|    11871 | 1035 | `				while( zBr < zIn && zBr[0] != '[' ){` |
|     9677 | 1036 | `					zBr++;` |
|        5 | 1037 | `				}` |
|     2199 | 1038 | `				if( zBr < zIn && zIn[-1] == ']' ){` |
|       12 | 1039 | `					const char *zKey = &zBr[1];` |
|       12 | 1040 | `					const char *zKeyEnd = &zIn[-1];` |
|       12 | 1041 | `					const char *zScan = zKey;` |
|       12 | 1042 | `					int bBare = (zKey < zKeyEnd) && !SyisDigit(zKey[0]);` |
|       20 | 1043 | `					while( bBare && zScan < zKeyEnd ){` |
|        9 | 1044 | `						if( !SyisAlphaNum(zScan[0]) && zScan[0] != '_' ){` |
|      ! 0 | 1045 | `							bBare = 0;` |
|      ! 0 | 1046 | `						}` |
|        9 | 1047 | `						zScan++;` |
|        1 | 1048 | `					}` |
|       12 | 1049 | `					if( bBare ){` |
|        - | 1050 | `						SyBlob sSub;` |
|        3 | 1051 | `						SyBlobInit(&sSub,&pGen->pVm->sAllocator);` |
|        3 | 1052 | `						SyBlobAppend(&sSub,zExpr,(sxu32)(zBr - zExpr));` |
|        3 | 1053 | `						SyBlobAppend(&sSub,"['",2);` |
|        3 | 1054 | `						SyBlobAppend(&sSub,zKey,(sxu32)(zKeyEnd - zKey));` |
|        3 | 1055 | `						SyBlobAppend(&sSub,"']",2);` |
|        4 | 1056 | `						rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,` |
|        2 | 1057 | `							(const char *)SyBlobData(&sSub),` |
|        2 | 1058 | `							(const char *)SyBlobData(&sSub) + SyBlobLength(&sSub));` |
|        3 | 1059 | `						SyBlobRelease(&sSub);` |
|        3 | 1060 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 1061 | `							return SXERR_ABORT;` |
|        - | 1062 | `						}` |
|        3 | 1063 | `						if( rc != SXERR_EMPTY ){` |
|        3 | 1064 | `							++iCons;` |
|        1 | 1065 | `						}` |
|        3 | 1066 | `						pObj = 0;` |
|        3 | 1067 | `						continue;` |
|        - | 1068 | `					}` |
|        4 | 1069 | `				}` |
|        - | 1070 | `			}` |
|        - | 1071 | `			/*` |
|        - | 1072 | `			 * "${...}" string interpolation (every form: ${name}, ${expr}, ${$x}) was` |
|        - | 1073 | `			 * DEPRECATED by php 8.2 in favor of the canonical "{$...}". PHL targets php's` |
|        - | 1074 | `			 * *non-deprecated* surface, so it is a hard parse error here — never silently` |
|        - | 1075 | `			 * rewritten. The canonical "{$var}" reaches this compiler by a different path` |
|        - | 1076 | `			 * and is unaffected.` |
|        - | 1077 | `			 */` |
|     2197 | 1078 | `			if( &zExpr[1] < zIn && zExpr[0] == '$' && zExpr[1] == '{' ){` |
|        3 | 1079 | `				PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|        - | 1080 | `					"syntax error, \"${\" string interpolation was removed in php 8.2, use \"{$...}\" instead");` |
|        3 | 1081 | `				return SXERR_ABORT;` |
|        - | 1082 | `			}` |
|        - | 1083 | `			/* Process the expression */` |
|     2195 | 1084 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|     2195 | 1085 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1086 | `				return SXERR_ABORT;` |
|        - | 1087 | `			}` |
|     2195 | 1088 | `			if( rc != SXERR_EMPTY ){` |
|     2195 | 1089 | `				++iCons;` |
|     1095 | 1090 | `			}` |
|        - | 1091 | `		}` |
|        - | 1092 | `		/* Invalidate the previously used constant */` |
|     2357 | 1093 | `		pObj = 0;` |
|        5 | 1094 | `	}/*for(;;)*/` |
|   124579 | 1095 | `	if( iCons > 1 ){` |
|        - | 1096 | `		/* Concatenate all compiled constants */` |
|     1637 | 1097 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|      816 | 1098 | `	}` |
|        - | 1099 | `	/* Node successfully compiled */` |
|   124579 | 1100 | `	return SXRET_OK;` |
|    62494 | 1101 | `}` |
|        - | 1102 | `/*` |
|        - | 1103 | ` * Compile a double quoted string.` |
|        - | 1104 | ` *  See the block-comment above for more information.` |
|        - | 1105 | ` */` |
|   124914 | 1106 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 | 1107 | `{` |
|        - | 1108 | `	sxi32 rc;` |
|   124919 | 1109 | `	rc = GenStateCompileString(&(*pGen),0/*bHeredoc*/);` |
|    62457 | 1110 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        - | 1111 | `	/* Compilation result */` |
|   124919 | 1112 | `	return rc;` |
|        5 | 1113 | `}` |
|        - | 1114 | `/*` |
|        - | 1115 | ` * Compile a Heredoc string.` |
|        - | 1116 | ` *  See the block-comment above for more information.` |
|        - | 1117 | ` */` |
|       68 | 1118 | `PH7_PRIVATE sxi32 PH7_CompileHereDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 | 1119 | `{` |
|        - | 1120 | `	SyString sOrig, sStripped;` |
|        - | 1121 | `	sxi32 rc;` |
|       73 | 1122 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|       73 | 1123 | `	if( rc != SXRET_OK ){` |
|        6 | 1124 | `		return rc;` |
|        - | 1125 | `	}` |
|        - | 1126 | `	/* Temporarily swap in the dedented body so GenStateCompileString` |
|        - | 1127 | `	 * (which reads pGen->pIn->sData directly) sees the stripped content.` |
|        - | 1128 | `	 * Restore before returning so downstream code that references pIn is` |
|        - | 1129 | `	 * unaffected, including on the error path. */` |
|       68 | 1130 | `	sOrig = pGen->pIn->sData;` |
|       68 | 1131 | `	pGen->pIn->sData = sStripped;` |
|       68 | 1132 | `	rc = GenStateCompileString(&(*pGen),1/*bHeredoc*/);` |
|       68 | 1133 | `	pGen->pIn->sData = sOrig;` |
|       32 | 1134 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       68 | 1135 | `	return rc;` |
|       39 | 1136 | `}` |
|        - | 1137 | `/*` |
|        - | 1138 | ` * Compile an array entry whether it is a key or a value.` |
|        - | 1139 | ` *  Notes on array entries.` |
|        - | 1140 | ` *  According to the PHP language reference manual` |
|        - | 1141 | ` *  An array can be created by the array() language construct.` |
|        - | 1142 | ` *  It takes as parameters any number of comma-separated key => value pairs.` |
|        - | 1143 | ` *  array(  key =>  value` |
|        - | 1144 | ` *    , ...` |
|        - | 1145 | ` *    )` |
|        - | 1146 | ` *  A key may be either an integer or a string. If a key is the standard representation` |
|        - | 1147 | ` *  of an integer, it will be interpreted as such (i.e. "8" will be interpreted as 8, while` |
|        - | 1148 | ` *  "08" will be interpreted as "08"). Floats in key are truncated to integer.` |
|        - | 1149 | ` *  The indexed and associative array types are the same type in PHP, which can both` |
|        - | 1150 | ` *  contain integer and string indices.` |
|        - | 1151 | ` *  A value can be any PHP type.` |
|        - | 1152 | ` *  If a key is not specified for a value, the maximum of the integer indices is taken` |
|        - | 1153 | ` *  and the new key will be that value plus 1. If a key that already has an assigned value` |
|        - | 1154 | ` *  is specified, that value will be overwritten.` |
|        - | 1155 | ` */` |
|  1565498 | 1156 | `PH7_PRIVATE sxi32 GenStateCompileArrayEntry(` |
|        - | 1157 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - | 1158 | `	SyToken *pIn,        /* Token stream */` |
|        - | 1159 | `	SyToken *pEnd,       /* End of the token stream */` |
|        - | 1160 | `	sxi32 iFlags,        /* Compilation flags */` |
|        - | 1161 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *) /* Expression tree validator callback */` |
|        - | 1162 | `	)` |
|        5 | 1163 | `{` |
|        - | 1164 | `	SyToken *pTmpIn,*pTmpEnd;` |
|        - | 1165 | `	sxi32 rc;` |
|        - | 1166 | `	/* Swap token stream */` |
|  1565503 | 1167 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|        - | 1168 | `	/* Compile the expression*/` |
|  1565503 | 1169 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|        - | 1170 | `	/* Restore token stream */` |
|  1565503 | 1171 | `	RE_SWAP_DELIMITER(pGen);` |
|  1565503 | 1172 | `	return rc;` |
|        5 | 1173 | `}` |
|        - | 1174 | `/*` |
|        - | 1175 | ` * Expression tree validator callback for the 'array' language construct.` |
|        - | 1176 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|        - | 1177 | ` * an invalid expression tree and this function will generate the appropriate` |
|        - | 1178 | ` * error message.` |
|        - | 1179 | ` * See the routine responible of compiling the array language construct` |
|        - | 1180 | ` * for more inforation.` |
|        - | 1181 | ` */` |
|       36 | 1182 | `static sxi32 GenStateArrayNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        5 | 1183 | `{` |
|       41 | 1184 | `	sxi32 rc = SXRET_OK;` |
|       41 | 1185 | `	if( pRoot->pOp ){` |
|       14 | 1186 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ &&` |
|       12 | 1187 | `			pRoot->pOp->iOp != EXPR_OP_FUNC_CALL /* function() [Symisc extension: i.e: array(&foo())] */` |
|       17 | 1188 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|        - | 1189 | `			/* Unexpected expression */` |
|       14 | 1190 | `			rc = PH7_GenSyntaxError(&(*pGen),pRoot->pStart,"\"->\" or \"?->\" or \"[\"");` |
|       14 | 1191 | `			if( rc != SXERR_ABORT ){` |
|       14 | 1192 | `				rc = SXERR_INVALID;` |
|        5 | 1193 | `			}` |
|       10 | 1194 | `		}` |
|       31 | 1195 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|        - | 1196 | `		/* Unexpected expression */` |
|        3 | 1197 | `		rc = PH7_GenSyntaxError(&(*pGen),pRoot->pStart,0);` |
|        3 | 1198 | `		if( rc != SXERR_ABORT ){` |
|        3 | 1199 | `			rc = SXERR_INVALID;` |
|        1 | 1200 | `		}` |
|        1 | 1201 | `	}` |
|       41 | 1202 | `	return rc;` |
|        5 | 1203 | `}` |
|        - | 1204 | `/*` |
|        - | 1205 | ` * Find the top-level '=>' (PH7_TK_ARRAY_OP) that separates an array/list entry's` |
|        - | 1206 | ` * key from its value within [pStart,pEnd). The scan skips any '=>' nested inside` |
|        - | 1207 | ` * brackets/parens/braces, inside an arrow-function signature (fn(...) =>), or` |
|        - | 1208 | ` * inside a match() {...} arm — none of which are key/value separators. Returns a` |
|        - | 1209 | ` * pointer to the '=>' token, or pEnd if the entry has no top-level separator.` |
|        - | 1210 | ` */` |
|  1490478 | 1211 | `PH7_PRIVATE SyToken * GenStateFindTopLevelArrow(SyToken *pStart,SyToken *pEnd)` |
|        5 | 1212 | `{` |
|  1490483 | 1213 | `	SyToken *pCur = pStart;` |
|  1490483 | 1214 | `	sxi32 iNest = 0;` |
|  3807687 | 1215 | `	while( pCur < pEnd ){` |
|  2855149 | 1216 | `		if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|   537941 | 1217 | `			return pCur;` |
|        - | 1218 | `		}` |
|        - | 1219 | `		/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|        - | 1220 | `		 * The '=>' inside an arrow function introduces the expression body,` |
|        - | 1221 | `		 * not an entry separator. Skip past the signature.` |
|        - | 1222 | `		 */` |
|  2317213 | 1223 | `		if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
|    23391 | 1224 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|    23391 | 1225 | `			SyToken *pFn = pCur;` |
|    23386 | 1226 | `			if( nKw == PH7_TKWRD_STATIC && &pCur[1] < pEnd` |
|      ! 0 | 1227 | `				&& (pCur[1].nType & PH7_TK_KEYWORD)` |
|        5 | 1228 | `				&& SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ){` |
|      ! 0 | 1229 | `				pFn = &pCur[1];` |
|      ! 0 | 1230 | `				nKw = PH7_TKWRD_FN;` |
|      ! 0 | 1231 | `			}` |
|    23391 | 1232 | `			if( nKw == PH7_TKWRD_FN ){` |
|        5 | 1233 | `				pCur = pFn + 1; /* past 'fn' */` |
|        5 | 1234 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_AMPER) ){` |
|      ! 0 | 1235 | `					pCur++;` |
|      ! 0 | 1236 | `				}` |
|        5 | 1237 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|        5 | 1238 | `					pCur++;` |
|        5 | 1239 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|        - | 1240 | `						PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|        5 | 1241 | `					if( pCur < pEnd ){` |
|        5 | 1242 | `						pCur++;` |
|        2 | 1243 | `					}` |
|        2 | 1244 | `				}` |
|        5 | 1245 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_COLON) ){` |
|      ! 0 | 1246 | `					pCur++;` |
|      ! 0 | 1247 | `					if( pCur < pEnd && (pCur->nType & PH7_TK_OP)` |
|      ! 0 | 1248 | `						&& pCur->sData.nByte == 1` |
|      ! 0 | 1249 | `						&& pCur->sData.zString[0] == '?' ){` |
|      ! 0 | 1250 | `						pCur++;` |
|      ! 0 | 1251 | `					}` |
|      ! 0 | 1252 | `					if( pCur < pEnd` |
|      ! 0 | 1253 | `						&& (pCur->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|      ! 0 | 1254 | `						pCur++;` |
|      ! 0 | 1255 | `					}` |
|      ! 0 | 1256 | `				}` |
|        - | 1257 | `				/* The rest of the entry is the arrow-function body — no outer` |
|        - | 1258 | `				 * key to extract. */` |
|        5 | 1259 | `				return pEnd;` |
|        - | 1260 | `			}` |
|        - | 1261 | `			/* Match expression (PHP 8.0): the '=>' inside match arms is not an` |
|        - | 1262 | `			 * entry separator. Skip past the full match span. */` |
|    23387 | 1263 | `			if( nKw == PH7_TKWRD_MATCH ){` |
|        3 | 1264 | `				pCur++; /* past 'match' */` |
|        3 | 1265 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|        3 | 1266 | `					pCur++;` |
|        3 | 1267 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|        - | 1268 | `						PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|        3 | 1269 | `					if( pCur < pEnd ){` |
|        3 | 1270 | `						pCur++;` |
|        1 | 1271 | `					}` |
|        1 | 1272 | `				}` |
|        3 | 1273 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_OCB) ){` |
|        3 | 1274 | `					pCur++;` |
|        3 | 1275 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|        - | 1276 | `						PH7_TK_OCB,PH7_TK_CCB,&pCur);` |
|        3 | 1277 | `					if( pCur < pEnd ){` |
|        3 | 1278 | `						pCur++;` |
|        1 | 1279 | `					}` |
|        1 | 1280 | `				}` |
|        3 | 1281 | `				continue;` |
|        - | 1282 | `			}` |
|    11690 | 1283 | `		}` |
|  2317207 | 1284 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|    54945 | 1285 | `			iNest++;` |
|  2289737 | 1286 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/) ){` |
|        - | 1287 | `			/* Don't worry about mismatched brackets here, the expression` |
|        - | 1288 | `			 * parser will shortly detect any syntax error. */` |
|    54945 | 1289 | `			iNest--;` |
|    27470 | 1290 | `		}` |
|  2317207 | 1291 | `		pCur++;` |
|        5 | 1292 | `	}` |
|   952543 | 1293 | `	return pEnd;` |
|   745244 | 1294 | `}` |
|        - | 1295 | `/*` |
|        - | 1296 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|        - | 1297 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|        - | 1298 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|        - | 1299 | ` */` |
|   671642 | 1300 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|        5 | 1301 | `{` |
|        - | 1302 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|        - | 1303 | `	SyToken *pKey,*pCur;` |
|   671647 | 1304 | `	sxi32 iEmitRef = 0;` |
|   671647 | 1305 | `	sxi32 iSpread = 0;` |
|   671647 | 1306 | `	sxi32 nPair = 0;` |
|        - | 1307 | `	sxi32 rc;` |
|   671647 | 1308 | `	xValidator = 0;` |
|   917549 | 1309 | `	for(;;){` |
|        - | 1310 | `		/* Jump leading commas. Exactly ONE separates two entries; a second one (or a comma` |
|        - | 1311 | `		 * at the very start) means an EMPTY element, which php rejects outright — PH7 just` |
|        - | 1312 | ``		 * skipped them, so `array(,)` and `array(1,,2)` compiled silently. A TRAILING comma`` |
|        - | 1313 | `		 * is legal and is handled by the loop exiting on the next pass. */` |
|   581728 | 1314 | `		{` |
|  1835103 | 1315 | `			int nSkip = 0;` |
|  2723431 | 1316 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|   888333 | 1317 | `				nSkip++;` |
|   888333 | 1318 | `				pGen->pIn++;` |
|        5 | 1319 | `			}` |
|  1835103 | 1320 | `			if( nSkip > 1 \|\| (nSkip > 0 && nPair < 1) ){` |
|      ! 0 | 1321 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn[-1].nLine,` |
|        - | 1322 | `					"Cannot use empty array elements in arrays");` |
|      ! 0 | 1323 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1324 | `					return SXERR_ABORT;` |
|        - | 1325 | `				}` |
|      ! 0 | 1326 | `				return SXRET_OK;` |
|        - | 1327 | `			}` |
|        - | 1328 | `		}` |
|  1835103 | 1329 | `		pCur = pGen->pIn;` |
|  1835103 | 1330 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|        - | 1331 | `			/* No more entry to process */` |
|   671629 | 1332 | `			break;` |
|        - | 1333 | `		}` |
|  1163479 | 1334 | `		if( pCur >= pGen->pIn ){` |
|      ! 0 | 1335 | `			continue;` |
|        - | 1336 | `		}` |
|        - | 1337 | `		/* Compile the key if available */` |
|  1163479 | 1338 | `		pKey = pCur;` |
|  1163479 | 1339 | `		pCur = GenStateFindTopLevelArrow(pCur,pGen->pIn);` |
|  1163479 | 1340 | `		rc = SXERR_EMPTY;` |
|  1163479 | 1341 | `		if( pCur < pGen->pIn ){` |
|   401767 | 1342 | `			if( pKey == pCur ){` |
|        - | 1343 | ``				/* `array( => 2)`: the entry STARTS with '=>', so it has no key. php rejects`` |
|        - | 1344 | `				 * it; PH7 warned about a "Missing entry key" and compiled on, accepting` |
|        - | 1345 | ``				 * source php refuses. (The `else if` below could never see this: the arrow`` |
|        - | 1346 | `				 * IS found here, so control never reached it.)` |
|        - | 1347 | `				 * php names the literal's own closer, so short syntax expects ']'. */` |
|        3 | 1348 | `				const char *zClose = (pGen->pEnd && (pGen->pEnd->nType & PH7_TK_CSB))` |
|        - | 1349 | `					? "\"]\"" : "\")\"";` |
|        3 | 1350 | `				rc = PH7_GenSyntaxError(&(*pGen),pCur,zClose);` |
|        3 | 1351 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1352 | `					return SXERR_ABORT;` |
|        - | 1353 | `				}` |
|        3 | 1354 | `				return SXRET_OK;` |
|        - | 1355 | `			}` |
|   401765 | 1356 | `			if( &pCur[1] >= pGen->pIn ){` |
|        - | 1357 | ``				/* `array(1 => )`: php names the token that SHOULD have started the value —`` |
|        - | 1358 | `				 * the ')' or ']' closing the literal — not the '=>' it just read. Passing 0` |
|        - | 1359 | `				 * makes the helper reach for the token past this entry's slice. */` |
|       14 | 1360 | `				rc = PH7_GenSyntaxError(&(*pGen),0,0);` |
|       14 | 1361 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1362 | `					return SXERR_ABORT;` |
|        - | 1363 | `				}` |
|       14 | 1364 | `				return SXRET_OK;` |
|        - | 1365 | `			}` |
|        - | 1366 | `			/* Compile the expression holding the key */` |
|   401755 | 1367 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|        - | 1368 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|   401755 | 1369 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1370 | `				return SXERR_ABORT;` |
|        - | 1371 | `			}` |
|   401755 | 1372 | `			pCur++; /* Jump the '=>' operator */` |
|   200880 | 1373 | `		}else{` |
|        - | 1374 | `			/* Reset back the cursor and point to the entry value */` |
|   761717 | 1375 | `			pCur = pKey;` |
|        - | 1376 | `		}` |
|  1163467 | 1377 | `		if( rc == SXERR_EMPTY ){` |
|        - | 1378 | `			/* No key given: load the nil, TAGGED so LOAD_MAP knows this is an absent key` |
|        - | 1379 | ``			 * (auto-index) rather than an explicit `null =>` one, which php deprecates. */`` |
|   761717 | 1380 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,PH7_LOADC_NOKEY,0 /* nil index */,0,0);` |
|   380856 | 1381 | `		}` |
|  1163467 | 1382 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
|        - | 1383 | `			/* Insertion by reference, [i.e: $a = array(&$x);] */` |
|       45 | 1384 | `			xValidator = GenStateArrayNodeValidator; /* Only variable are allowed */` |
|       45 | 1385 | `			iEmitRef = 1;` |
|       45 | 1386 | `			pCur++; /* Jump the '&' token */` |
|       45 | 1387 | `			if( pCur >= pGen->pIn ){` |
|        - | 1388 | `				/* Missing value */` |
|        3 | 1389 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing referenced variable");` |
|        3 | 1390 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1391 | `					return SXERR_ABORT;` |
|        - | 1392 | `				}` |
|        3 | 1393 | `				return SXRET_OK;` |
|        - | 1394 | `			}` |
|       19 | 1395 | `		}` |
|        - | 1396 | `		/* Detect array unpack: '...$expr' as the entry value (PHP 7.4+, with` |
|        - | 1397 | `		 * string-key support since PHP 8.1). The parser strips the '...' inside` |
|        - | 1398 | `		 * ExprExtractNode; we only need to know it's there so we can emit` |
|        - | 1399 | `		 * PH7_OP_FLAG_SPREAD after the value, instructing LOAD_MAP to merge the` |
|        - | 1400 | `		 * resulting hashmap rather than insert it as a scalar entry. */` |
|  1163465 | 1401 | `		iSpread = (pCur < pGen->pIn && (pCur->nType & PH7_TK_ELLIPSIS)) ? 1 : 0;` |
|  1163465 | 1402 | `		if( iSpread && (rc != SXERR_EMPTY \|\| iEmitRef) ){` |
|        - | 1403 | `			/* '[k => ...$a]' and '[&...$a]' are syntax errors in PHP — the` |
|        - | 1404 | `			 * '...' token cannot follow either '=>' or '&' inside an array` |
|        - | 1405 | `			 * literal. Emit the same Parse-error wording PHP uses so the` |
|        - | 1406 | `			 * output is engine-portable. */` |
|        6 | 1407 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pCur->nLine,` |
|        - | 1408 | `				"syntax error, unexpected token \"...\"");` |
|        6 | 1409 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1410 | `				return SXERR_ABORT;` |
|        - | 1411 | `			}` |
|        6 | 1412 | `			return SXRET_OK;` |
|        - | 1413 | `		}` |
|        - | 1414 | ``		/* Compile indice value. A BY-REF element (`'k' => &$a[$i]`) is an`` |
|        - | 1415 | `		 * lvalue: php VIVIFIES a missing subscript when a reference is taken,` |
|        - | 1416 | `		 * so compile it in write context (LOAD_IDX iP2=1, create-if-missing)` |
|        - | 1417 | `		 * instead of a read-only load — which also keeps the undefined-key` |
|        - | 1418 | `		 * warning (a read-only diagnostic) from false-firing here. */` |
|  1745189 | 1419 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,` |
|   581728 | 1420 | `			iEmitRef ? EXPR_FLAG_LOAD_IDX_STORE` |
|        - | 1421 | `			         : EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,` |
|   581728 | 1422 | `			xValidator);` |
|  1163461 | 1423 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1424 | `			return SXERR_ABORT;` |
|        - | 1425 | `		}` |
|  1163461 | 1426 | `		if( iSpread ){` |
|        - | 1427 | `			/* Mark the value on TOS as a spread source; LOAD_MAP merges it. */` |
|       73 | 1428 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FLAG_SPREAD,0,0,0,0);` |
|  1163426 | 1429 | `		}else if( iEmitRef ){` |
|        - | 1430 | `			/* Emit the load reference instruction */` |
|       41 | 1431 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|       18 | 1432 | `		}` |
|  1163461 | 1433 | `		xValidator = 0;` |
|  1163461 | 1434 | `		iEmitRef = 0;` |
|  1163461 | 1435 | `		iSpread = 0;` |
|  1163461 | 1436 | `		nPair++;` |
|        5 | 1437 | `	}` |
|        - | 1438 | `	/* Emit the load map instruction */` |
|   671629 | 1439 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|        - | 1440 | `	/* Node successfully compiled */` |
|   671629 | 1441 | `	return SXRET_OK;` |
|   335826 | 1442 | `}` |
|        - | 1443 | `/*` |
|        - | 1444 | ` * Compile the 'array' language construct.` |
|        - | 1445 | ` *	 According to the PHP language reference manual` |
|        - | 1446 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|        - | 1447 | ` *   values to keys. This type is optimized for several different uses; it can` |
|        - | 1448 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|        - | 1449 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|        - | 1450 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|        - | 1451 | ` */` |
|   428614 | 1452 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 | 1453 | `{` |
|        - | 1454 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   428619 | 1455 | `	pGen->pIn += 2;` |
|   428619 | 1456 | `	pGen->pEnd--;` |
|   214307 | 1457 | `	SXUNUSED(iCompileFlag);` |
|   428619 | 1458 | `	return GenStateCompileArrayBody(pGen);` |
|        5 | 1459 | `}` |
|        - | 1460 | `/*` |
|        - | 1461 | ` * Compile the PHP 8.5 clone(...) call form:` |
|        - | 1462 | `` *   clone($object)                          -> identical to the `clone $object` operator`` |
|        - | 1463 | ` *   clone($object, ['prop' => value, ...])  -> clone, run __clone(), then apply the` |
|        - | 1464 | ` *                                              property updates as scope-aware writes` |
|        - | 1465 | ` *   clone(object: $o, withProperties: [..]) -> the named-argument spelling` |
|        - | 1466 | ` * Codegen: compile the object argument and emit OP_CLONE (which clones and runs` |
|        - | 1467 | ` * __clone()); if a withProperties argument is present, compile it and emit` |
|        - | 1468 | ` * OP_CLONE_APPLY, which applies each update to the fresh clone AFTER __clone(),` |
|        - | 1469 | ` * honouring visibility / readonly-set-scope / typed-property enforcement in the` |
|        - | 1470 | ` * calling scope. The parser (ExprExtractNode) delimited this node's tokens as` |
|        - | 1471 | `` * `clone ( ... )`; pGen->pIn/pEnd point at the first/one-past-last of that range.`` |
|        - | 1472 | ` */` |
|       22 | 1473 | `PH7_PRIVATE sxi32 PH7_CompileCloneCall(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        2 | 1474 | `{` |
|        - | 1475 | `	SyToken *pIn,*pEnd,*pNext;` |
|       24 | 1476 | `	SyToken *pObjStart = 0,*pObjEnd = 0;` |
|       24 | 1477 | `	SyToken *pUpdStart = 0,*pUpdEnd = 0;` |
|       24 | 1478 | `	int nArg = 0;` |
|        - | 1479 | `	sxi32 rc;` |
|       11 | 1480 | `	SXUNUSED(iCompileFlag);` |
|        - | 1481 | `	/* pGen->pIn -> 'clone', pGen->pIn[1] -> '(', pGen->pEnd -> one past ')'. */` |
|       24 | 1482 | `	pIn  = pGen->pIn + 2;   /* skip 'clone' and the opening '(' */` |
|       24 | 1483 | `	pEnd = pGen->pEnd - 1;  /* exclude the closing ')' */` |
|        - | 1484 | `	/* clone(...) first-class-callable form: a lone ellipsis is the whole list. */` |
|       24 | 1485 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|      ! 0 | 1486 | `		return PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|        - | 1487 | `			"clone(...) first-class callable form is not yet supported");` |
|        - | 1488 | `	}` |
|        - | 1489 | `	/* Split the (at most two) comma-separated arguments, tolerating named labels. */` |
|       62 | 1490 | `	while( pIn < pEnd ){` |
|       40 | 1491 | `		SyToken *pArgStart,*pArgEnd,*pName = 0;` |
|       40 | 1492 | `		if( PH7_GetNextExpr(pIn,pEnd,&pNext) != SXRET_OK ){` |
|      ! 0 | 1493 | `			break;` |
|        - | 1494 | `		}` |
|       40 | 1495 | `		pArgStart = pIn;` |
|       40 | 1496 | `		pArgEnd   = pNext;` |
|        - | 1497 | `		/* Named-argument label: <ID\|keyword> ':' expr. A single ':' is PH7_TK_COLON;` |
|        - | 1498 | ``		 * '::' is a distinct operator token, so this never mis-fires on `A::B`. */`` |
|       38 | 1499 | `		if( (pArgEnd - pArgStart) >= 2` |
|       37 | 1500 | `			&& (pArgStart[0].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|       23 | 1501 | `			&& (pArgStart[1].nType & PH7_TK_COLON) ){` |
|        5 | 1502 | `			pName = pArgStart;` |
|        5 | 1503 | `			pArgStart += 2;` |
|        2 | 1504 | `		}` |
|       40 | 1505 | `		if( pName ){` |
|        - | 1506 | `` 			/* PHP named parameters are case-SENSITIVE, so `Object:`/`WITHPROPERTIES:` `` |
|        - | 1507 | `			 * must be rejected as unknown (SyMemcmp, not SyStrnicmp). */` |
|        4 | 1508 | `			if( pName->sData.nByte == sizeof("object")-1` |
|        4 | 1509 | `				&& SyMemcmp(pName->sData.zString,"object",sizeof("object")-1) == 0 ){` |
|        3 | 1510 | `				pObjStart = pArgStart; pObjEnd = pArgEnd;` |
|        4 | 1511 | `			}else if( pName->sData.nByte == sizeof("withProperties")-1` |
|        3 | 1512 | `				&& SyMemcmp(pName->sData.zString,"withProperties",sizeof("withProperties")-1) == 0 ){` |
|        3 | 1513 | `				pUpdStart = pArgStart; pUpdEnd = pArgEnd;` |
|        2 | 1514 | `			}else{` |
|      ! 0 | 1515 | `				return PH7_GenCompileError(pGen,E_ERROR,pName->nLine,` |
|      ! 0 | 1516 | `					"Unknown named parameter $%z",&pName->sData);` |
|        1 | 1517 | `			}` |
|       38 | 1518 | `		}else if( nArg == 0 ){` |
|       22 | 1519 | `			pObjStart = pArgStart; pObjEnd = pArgEnd;` |
|       25 | 1520 | `		}else if( nArg == 1 ){` |
|       15 | 1521 | `			pUpdStart = pArgStart; pUpdEnd = pArgEnd;` |
|        8 | 1522 | `		}else{` |
|      ! 0 | 1523 | `			return PH7_GenCompileError(pGen,E_ERROR,pArgStart->nLine,` |
|        - | 1524 | `				"clone() expects at most 2 arguments");` |
|        - | 1525 | `		}` |
|       40 | 1526 | `		nArg++;` |
|       40 | 1527 | `		pIn = pNext;` |
|       40 | 1528 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|       17 | 1529 | `			pIn++; /* step over the argument separator */` |
|        8 | 1530 | `		}` |
|        2 | 1531 | `	}` |
|       24 | 1532 | `	if( pObjStart == 0 \|\| pObjStart >= pObjEnd ){` |
|      ! 0 | 1533 | `		return PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 1534 | `			"clone() expects at least 1 argument, 0 given");` |
|        - | 1535 | `	}` |
|        - | 1536 | `	/* Object argument -> clone (+ __clone()). */` |
|       24 | 1537 | `	rc = GenStateCompileArrayEntry(pGen,pObjStart,pObjEnd,EXPR_FLAG_RDONLY_LOAD,0);` |
|       24 | 1538 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 1539 | `		return SXERR_ABORT;` |
|        - | 1540 | `	}` |
|       24 | 1541 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_CLONE,0,0,0,0);` |
|        - | 1542 | `	/* Property updates (evaluated after __clone runs). */` |
|       24 | 1543 | `	if( pUpdStart && pUpdStart < pUpdEnd ){` |
|       17 | 1544 | `		rc = GenStateCompileArrayEntry(pGen,pUpdStart,pUpdEnd,EXPR_FLAG_RDONLY_LOAD,0);` |
|       17 | 1545 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1546 | `			return SXERR_ABORT;` |
|        - | 1547 | `		}` |
|       17 | 1548 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CLONE_APPLY,0,0,0,0);` |
|        8 | 1549 | `	}` |
|       24 | 1550 | `	return SXRET_OK;` |
|       13 | 1551 | `}` |
|        - | 1552 | `/*` |
|        - | 1553 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|        - | 1554 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|        - | 1555 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|        - | 1556 | ` */` |
|   243028 | 1557 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 | 1558 | `{` |
|        - | 1559 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|   243033 | 1560 | `	pGen->pIn++;` |
|   243033 | 1561 | `	pGen->pEnd--;` |
|   121514 | 1562 | `	SXUNUSED(iCompileFlag);` |
|   243033 | 1563 | `	return GenStateCompileArrayBody(pGen);` |
|        5 | 1564 | `}` |
|        - | 1565 | `/*` |
|        - | 1566 | ` * Expression tree validator callback for the 'list' language construct.` |
|        - | 1567 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|        - | 1568 | ` * an invalid expression tree and this function will generate the appropriate` |
|        - | 1569 | ` * error message.` |
|        - | 1570 | ` * See the routine responible of compiling the list language construct` |
|        - | 1571 | ` * for more inforation.` |
|        - | 1572 | ` */` |
|      218 | 1573 | `static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        5 | 1574 | `{` |
|      223 | 1575 | `	sxi32 rc = SXRET_OK;` |
|      223 | 1576 | `	if( pRoot->pOp ){` |
|        4 | 1577 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */` |
|        2 | 1578 | `			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){` |
|        - | 1579 | `				/* Unexpected expression */` |
|      ! 0 | 1580 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - | 1581 | `					"Assignments can only happen to writable values");` |
|      ! 0 | 1582 | `				if( rc != SXERR_ABORT ){` |
|      ! 0 | 1583 | `					rc = SXERR_INVALID;` |
|      ! 0 | 1584 | `				}` |
|        1 | 1585 | `		}` |
|      221 | 1586 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|        - | 1587 | `		/* Unexpected expression */` |
|        6 | 1588 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - | 1589 | `			"Assignments can only happen to writable values");` |
|        6 | 1590 | `		if( rc != SXERR_ABORT ){` |
|        6 | 1591 | `			rc = SXERR_INVALID;` |
|        2 | 1592 | `		}` |
|        2 | 1593 | `	}` |
|      223 | 1594 | `	return rc;` |
|        5 | 1595 | `}` |
|        - | 1596 | `/*` |
|        - | 1597 | ` * Compile the 'list' language construct.` |
|        - | 1598 | ` *  According to the PHP language reference` |
|        - | 1599 | ` *  list(): Assign variables as if they were an array.` |
|        - | 1600 | ` *  list() is used to assign a list of variables in one operation.` |
|        - | 1601 | ` *  Description` |
|        - | 1602 | ` *   array list (mixed $varname [, mixed $... ] )` |
|        - | 1603 | ` *   Like array(), this is not really a function, but a language construct.` |
|        - | 1604 | ` *   list() is used to assign a list of variables in one operation.` |
|        - | 1605 | ` *  Parameters` |
|        - | 1606 | ` *   $varname: A variable.` |
|        - | 1607 | ` *  Return Values` |
|        - | 1608 | ` *   The assigned array.` |
|        - | 1609 | ` */` |
|        - | 1610 | `/* Nested list entry recorded during first pass of list body compilation */` |
|        - | 1611 | `struct NestedListEntry {` |
|        - | 1612 | `	sxi32 nIndex;        /* Position in the outer list (0-based) */` |
|        - | 1613 | `	SyToken *pStart;     /* Token range: start of nested construct */` |
|        - | 1614 | `	SyToken *pEnd;       /* Token range: past closing delimiter */` |
|        - | 1615 | `	sxi32 isShort;       /* 1 if [...] form, 0 if list(...) form */` |
|        - | 1616 | `};` |
|        - | 1617 | `/*` |
|        - | 1618 | ` * Compile the body of a *keyed* list/short-list destructuring (PHP 7.1), where` |
|        - | 1619 | `` * every entry has the form `keyExpr => target`. The source array is on the stack`` |
|        - | 1620 | ` * top on entry and remains there on exit, mirroring the positional LOAD_LIST` |
|        - | 1621 | ` * path so the caller's teardown is unchanged. For each entry: DUP the source,` |
|        - | 1622 | ` * push the key, LOAD_IDX to fetch source[key] (NULL on a missing key, silently,` |
|        - | 1623 | ` * like a normal subscript read), then assign the fetched value to the target — a` |
|        - | 1624 | ` * nested [...]/list() recurses, a simple lvalue uses the same STORE fold as a` |
|        - | 1625 | ` * normal assignment (the value sits below the lvalue-load, exactly as in` |
|        - | 1626 | ` * GenStateEmitExprCode where the assignment RHS precedes the LHS load).` |
|        - | 1627 | ` */` |
|       22 | 1628 | `static sxi32 GenStateCompileKeyedListBody(ph7_gen_state *pGen)` |
|        1 | 1629 | `{` |
|        - | 1630 | `	SyToken *pNext;` |
|        - | 1631 | `	sxi32 rc;` |
|       53 | 1632 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|        - | 1633 | `		SyToken *pArrow,*pTarget;` |
|        - | 1634 | ``		/* Split `keyExpr => target` at the top-level '=>' */`` |
|       31 | 1635 | `		pArrow = GenStateFindTopLevelArrow(pGen->pIn,pNext);` |
|       31 | 1636 | `		pTarget = &pArrow[1];` |
|       31 | 1637 | `		if( pArrow <= pGen->pIn \|\| pTarget >= pNext ){` |
|        - | 1638 | ``			/* Empty key (`[ => $v]`) or empty value (`["k" =>]`): PHP rejects`` |
|        - | 1639 | `			 * both. Reject rather than silently emitting unbalanced bytecode. */` |
|      ! 0 | 1640 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - | 1641 | `				"Cannot use empty array entries in keyed array assignment");` |
|      ! 0 | 1642 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 1643 | `		}` |
|        - | 1644 | `		/* DUP the source array (it is on the stack top) */` |
|       31 | 1645 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|        - | 1646 | `		/* Compile the key expression; it is pushed above the DUP'd source */` |
|       31 | 1647 | `		rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pArrow,EXPR_FLAG_RDONLY_LOAD,0);` |
|       31 | 1648 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1649 | `			return SXERR_ABORT;` |
|        - | 1650 | `		}` |
|        - | 1651 | `		/* LOAD_IDX: pop the key, replace the DUP'd source with source[key].` |
|        - | 1652 | `		 * iP2=7 is the keyed-destructuring read context: an array source reads like` |
|        - | 1653 | ``		 * iP2=0 (missing key loads NULL silently, matching a normal `$arr[$k]` read;`` |
|        - | 1654 | `		 * PHP also emits an "Undefined array key" warning here, PHL omits it — §3.7),` |
|        - | 1655 | `		 * but a NON-array source yields NULL + a per-key "Cannot use <type> as array"` |
|        - | 1656 | `		 * warning instead of char-indexing a string (matching PHP's OP_LOAD_LIST path). */` |
|       31 | 1657 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,7,0,0);` |
|       31 | 1658 | `		if( pTarget < pNext && ( (pTarget->nType & PH7_TK_OSB)` |
|       28 | 1659 | `			\|\| ( (pTarget->nType & PH7_TK_KEYWORD)` |
|       15 | 1660 | `				&& SX_PTR_TO_INT(pTarget->pUserData) == PH7_TKWRD_LIST ) ) ){` |
|        - | 1661 | `			/* Nested destructuring:  ["k" => [ ... ]]  or  ["k" => list( ... )].` |
|        - | 1662 | `			 * Treat source[key] as the inner body's source, then drop the` |
|        - | 1663 | `			 * leftover it leaves behind (mirrors the positional nested path). */` |
|        5 | 1664 | `			sxi32 isShort = (pTarget->nType & PH7_TK_OSB) != 0;` |
|        5 | 1665 | `			SyToken *pSavedIn = pGen->pIn;` |
|        5 | 1666 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|        5 | 1667 | `			pGen->pIn = pTarget;` |
|        5 | 1668 | `			pGen->pEnd = pNext;` |
|        5 | 1669 | `			rc = isShort ? PH7_CompileShortList(&(*pGen),0)` |
|        2 | 1670 | `			             : PH7_CompileList(&(*pGen),0);` |
|        5 | 1671 | `			pGen->pIn = pSavedIn;` |
|        5 | 1672 | `			pGen->pEnd = pSavedEnd;` |
|        5 | 1673 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1674 | `				return SXERR_ABORT;` |
|        - | 1675 | `			}` |
|        5 | 1676 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        3 | 1677 | `		}else{` |
|        - | 1678 | `			/* Simple lvalue target ($v / $o->p / $a[i] / Cls::$s). source[key]` |
|        - | 1679 | `			 * is already on the stack as the value; compiling the target appends` |
|        - | 1680 | `			 * its lvalue-load, which we fold into a STORE just as a normal` |
|        - | 1681 | `			 * assignment does. */` |
|        - | 1682 | `			VmInstr *pInstr;` |
|       27 | 1683 | `			sxi32 iVmOp = PH7_OP_STORE;` |
|       27 | 1684 | `			sxi32 iP1 = 0, iP2 = 0;` |
|       27 | 1685 | `			void *p3 = 0;` |
|       27 | 1686 | `			rc = GenStateCompileArrayEntry(&(*pGen),pTarget,pNext,` |
|        - | 1687 | `				EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|       27 | 1688 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1689 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 1690 | `			}` |
|       27 | 1691 | `			if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|       27 | 1692 | `				if( pInstr->iOp == PH7_OP_MEMBER ){` |
|        3 | 1693 | `					iP2 = 1; /* member store: keep MEMBER, store value below it */` |
|       26 | 1694 | `				}else if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|        3 | 1695 | `					iVmOp = PH7_OP_STORE_IDX;` |
|        3 | 1696 | `					iP1 = pInstr->iP1;` |
|        3 | 1697 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|        2 | 1698 | `				}else{` |
|       23 | 1699 | `					p3 = pInstr->p3; /* named store: $v = value */` |
|       23 | 1700 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|        - | 1701 | `				}` |
|       13 | 1702 | `			}` |
|       27 | 1703 | `			PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|        - | 1704 | `			/* STORE leaves the assigned value on the stack top; drop it so the` |
|        - | 1705 | `			 * source array is back on top for the next entry. */` |
|       27 | 1706 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        - | 1707 | `		}` |
|       31 | 1708 | `		pGen->pIn = &pNext[1];` |
|        1 | 1709 | `	}` |
|       23 | 1710 | `	return SXRET_OK;` |
|       12 | 1711 | `}` |
|        - | 1712 | `/*` |
|        - | 1713 | ` * Shared body for list() and short list [...] compilation.` |
|        - | 1714 | ` * Assumes pGen->pIn and pGen->pEnd are already positioned past` |
|        - | 1715 | ` * the opening delimiter and before the closing delimiter.` |
|        - | 1716 | ` */` |
|      126 | 1717 | `static sxi32 GenStateCompileListBody(ph7_gen_state *pGen)` |
|        5 | 1718 | `{` |
|        - | 1719 | `	SySet sNested; /* Dynamically-sized container of NestedListEntry */` |
|        - | 1720 | `	SyToken *pNext;` |
|        - | 1721 | `	SyToken *pClassifyIn;` |
|      131 | 1722 | `	sxi32 nKeyed = 0, nPositional = 0, nEmpty = 0;` |
|        - | 1723 | `	sxi32 nExpr;` |
|        - | 1724 | `	sxi32 rc;` |
|        - | 1725 | ``	/* First pass: classify entries as keyed (`k => v`), positional, or empty`` |
|        - | 1726 | `	 * skip slots ([,]). A list level must be entirely keyed or entirely` |
|        - | 1727 | `	 * positional — PHP fatals on a mix, and on an empty slot inside a keyed` |
|        - | 1728 | `	 * list. */` |
|      131 | 1729 | `	pClassifyIn = pGen->pIn;` |
|      379 | 1730 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|      253 | 1731 | `		if( pGen->pIn >= pNext ){` |
|       13 | 1732 | `			nEmpty++;` |
|      247 | 1733 | `		}else if( GenStateFindTopLevelArrow(pGen->pIn,pNext) < pNext ){` |
|       31 | 1734 | `			nKeyed++;` |
|       16 | 1735 | `		}else{` |
|      211 | 1736 | `			nPositional++;` |
|        - | 1737 | `		}` |
|      253 | 1738 | `		pGen->pIn = &pNext[1];` |
|        5 | 1739 | `	}` |
|      131 | 1740 | `	pGen->pIn = pClassifyIn;` |
|      131 | 1741 | `	if( nKeyed > 0 && nEmpty > 0 ){` |
|      ! 0 | 1742 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - | 1743 | `			"Cannot use empty array entries in keyed array assignment");` |
|      ! 0 | 1744 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 1745 | `	}` |
|      131 | 1746 | `	if( nKeyed > 0 && nPositional > 0 ){` |
|      ! 0 | 1747 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - | 1748 | `			"Cannot mix keyed and unkeyed array entries in assignments");` |
|      ! 0 | 1749 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 1750 | `	}` |
|      131 | 1751 | `	if( nKeyed > 0 ){` |
|       23 | 1752 | `		return GenStateCompileKeyedListBody(pGen);` |
|        - | 1753 | `	}` |
|      109 | 1754 | `	nExpr = 0;` |
|      109 | 1755 | `	SySetInit(&sNested,&pGen->pVm->sAllocator,sizeof(struct NestedListEntry));` |
|      327 | 1756 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|      223 | 1757 | `		if( pGen->pIn < pNext ){` |
|        - | 1758 | `			/* Check for nested list() */` |
|      211 | 1759 | `			if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        3 | 1760 | `				SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|        - | 1761 | `				/* Record this nested list for post-processing */` |
|        3 | 1762 | `				SyToken *pListEnd = 0;` |
|        3 | 1763 | `				if( &pGen->pIn[1] < pNext && (pGen->pIn[1].nType & PH7_TK_LPAREN) ){` |
|        3 | 1764 | `					PH7_DelimitNestedTokens(pGen->pIn+2,pNext,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|        1 | 1765 | `				}` |
|        3 | 1766 | `				if( pListEnd ){` |
|        - | 1767 | `					struct NestedListEntry sEntry;` |
|        3 | 1768 | `					sEntry.nIndex = nExpr;` |
|        3 | 1769 | `					sEntry.pStart = pGen->pIn;` |
|        3 | 1770 | `					sEntry.pEnd = pListEnd + 1;` |
|        3 | 1771 | `					sEntry.isShort = 0;` |
|        3 | 1772 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|        1 | 1773 | `				}` |
|        - | 1774 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|        3 | 1775 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|      210 | 1776 | `			}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|        - | 1777 | `				/* Nested short destructuring [...] */` |
|       13 | 1778 | `				SyToken *pBracketEnd = 0;` |
|       13 | 1779 | `				PH7_DelimitNestedTokens(pGen->pIn+1,pNext,PH7_TK_OSB,PH7_TK_CSB,&pBracketEnd);` |
|       13 | 1780 | `				if( pBracketEnd ){` |
|        - | 1781 | `					struct NestedListEntry sEntry;` |
|       13 | 1782 | `					sEntry.nIndex = nExpr;` |
|       13 | 1783 | `					sEntry.pStart = pGen->pIn;` |
|       13 | 1784 | `					sEntry.pEnd = pBracketEnd + 1;` |
|       13 | 1785 | `					sEntry.isShort = 1;` |
|       13 | 1786 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|        6 | 1787 | `				}` |
|        - | 1788 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|       13 | 1789 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|        7 | 1790 | `			}else{` |
|        - | 1791 | `				/* Compile the expression holding the variable */` |
|      197 | 1792 | `				rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|      197 | 1793 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 1794 | `					SySetRelease(&sNested);` |
|      ! 0 | 1795 | `					return SXRET_OK;` |
|        - | 1796 | `				}` |
|        - | 1797 | `			}` |
|      108 | 1798 | `		}else{` |
|        - | 1799 | `			/* Empty entry,load NULL */` |
|       13 | 1800 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);` |
|        - | 1801 | `		}` |
|      223 | 1802 | `		nExpr++;` |
|        - | 1803 | `		/* Advance the stream cursor */` |
|      223 | 1804 | `		pGen->pIn = &pNext[1];` |
|        5 | 1805 | `	}` |
|        - | 1806 | `	/* Emit the LOAD_LIST instruction */` |
|      109 | 1807 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,0,0,0);` |
|        - | 1808 | `	/* After LOAD_LIST, the source array is still on the stack top.` |
|        - | 1809 | `	 * For each nested entry, emit code to extract the sub-array` |
|        - | 1810 | `	 * at the corresponding index and recursively destructure it.` |
|        - | 1811 | `	 */` |
|      109 | 1812 | `	if( SySetUsed(&sNested) > 0 ){` |
|       13 | 1813 | `		struct NestedListEntry *apNested = (struct NestedListEntry *)SySetBasePtr(&sNested);` |
|        - | 1814 | `		sxu32 i;` |
|       27 | 1815 | `		for(i = 0; i < SySetUsed(&sNested); i++){` |
|       15 | 1816 | `			SyToken *pSavedIn = pGen->pIn;` |
|       15 | 1817 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|        - | 1818 | `			ph7_value *pIdx;` |
|        - | 1819 | `			sxu32 nConstIdx;` |
|        - | 1820 | `			/* DUP the source array (it's on stack top) */` |
|       15 | 1821 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|        - | 1822 | `			/* Push the integer index for this nested entry */` |
|       15 | 1823 | `			pIdx = PH7_ReserveConstObj(pGen->pVm,&nConstIdx);` |
|       15 | 1824 | `			if( pIdx == 0 ){` |
|      ! 0 | 1825 | `				PH7_GenCompileError(&(*pGen),E_ERROR,0,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1826 | `				SySetRelease(&sNested);` |
|      ! 0 | 1827 | `				return SXERR_ABORT;` |
|        - | 1828 | `			}` |
|       15 | 1829 | `			PH7_MemObjInitFromInt(pGen->pVm,pIdx,(sxi64)apNested[i].nIndex);` |
|       15 | 1830 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nConstIdx,0,0);` |
|        - | 1831 | `			/* LOAD_IDX: pop index, replace DUP'd source with source[index].` |
|        - | 1832 | `			 * iP2=2 signals the VM to emit an "Undefined array key" warning` |
|        - | 1833 | `			 * when the key is missing (PHP-compatible list destructuring).` |
|        - | 1834 | `			 */` |
|       15 | 1835 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,2,0,0);` |
|        - | 1836 | `			/* Recursively compile the inner list */` |
|       15 | 1837 | `			pGen->pIn = apNested[i].pStart;` |
|       15 | 1838 | `			pGen->pEnd = apNested[i].pEnd;` |
|       15 | 1839 | `			if( apNested[i].isShort ){` |
|       13 | 1840 | `				rc = PH7_CompileShortList(&(*pGen),0);` |
|        7 | 1841 | `			}else{` |
|        3 | 1842 | `				rc = PH7_CompileList(&(*pGen),0);` |
|        - | 1843 | `			}` |
|       15 | 1844 | `			pGen->pIn = pSavedIn;` |
|       15 | 1845 | `			pGen->pEnd = pSavedEnd;` |
|       15 | 1846 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1847 | `				SySetRelease(&sNested);` |
|      ! 0 | 1848 | `				return SXERR_ABORT;` |
|        - | 1849 | `			}` |
|        - | 1850 | `			/* Pop the leftover source[index] from the inner LOAD_LIST */` |
|       15 | 1851 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        8 | 1852 | `		}` |
|        6 | 1853 | `	}` |
|      109 | 1854 | `	SySetRelease(&sNested);` |
|        - | 1855 | `	/* Node successfully compiled */` |
|      109 | 1856 | `	return SXRET_OK;` |
|       68 | 1857 | `}` |
|       40 | 1858 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 | 1859 | `{` |
|        - | 1860 | `	/* Jump the 'list' keyword, the leading '(' and exclude trailing ')' */` |
|       45 | 1861 | `	pGen->pIn += 2;` |
|       45 | 1862 | `	pGen->pEnd--;` |
|       20 | 1863 | `	SXUNUSED(iCompileFlag);` |
|       45 | 1864 | `	return GenStateCompileListBody(pGen);` |
|        5 | 1865 | `}` |
|       86 | 1866 | `PH7_PRIVATE sxi32 PH7_CompileShortList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        3 | 1867 | `{` |
|        - | 1868 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|       89 | 1869 | `	pGen->pIn++;` |
|       89 | 1870 | `	pGen->pEnd--;` |
|       43 | 1871 | `	SXUNUSED(iCompileFlag);` |
|       89 | 1872 | `	return GenStateCompileListBody(pGen);` |
|        3 | 1873 | `}` |
|        - | 1874 |  |
