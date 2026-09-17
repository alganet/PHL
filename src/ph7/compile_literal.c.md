# src/ph7/compile_literal.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 957/1074 lines (89.11%)

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
|     1088 |   23 | `static int GenStateIsBaseDigit(int c, int base)` |
|        3 |   24 | `{` |
|     1091 |   25 | `	if( base == 16 ){ return SyisHex(c); }` |
|      991 |   26 | `	if( base == 2 ){ return c == '0' \|\| c == '1'; }` |
|      711 |   27 | `	return SyisDigit(c);` |
|      547 |   28 | `}` |
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
|  3809962 |   45 | `static int GenStateFindBadNumericSeparator(` |
|        - |   46 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|        5 |   47 | `{` |
|  3809967 |   48 | `	const char *z = pRaw->zString;` |
|  3809967 |   49 | `	sxu32 n = pRaw->nByte;` |
|  3809967 |   50 | `	int base = 10;` |
|        - |   51 | `	sxu32 i, start;` |
|  3809967 |   52 | `	if( n < 2 ) return 0;` |
|   802625 |   53 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|   104429 |   54 | `		base = 16;` |
|   750413 |   55 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|      287 |   56 | `		base = 2;` |
|      143 |   57 | `	}` |
|  3056591 |   58 | `	for( i = 0; i < n; ++i ){` |
|  2253981 |   59 | `		if( z[i] != '_' ) continue;` |
|      550 |   60 | `		if( i > 0 && i + 1 < n` |
|      547 |   61 | `			&& GenStateIsBaseDigit((unsigned char)z[i-1], base)` |
|      549 |   62 | `			&& GenStateIsBaseDigit((unsigned char)z[i+1], base) ){` |
|      541 |   63 | `			continue; /* well-placed separator */` |
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
|   802615 |   76 | `	return 0;` |
|  1904986 |   77 | `}` |
|        - |   78 | `/*` |
|        - |   79 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|        - |   80 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|        - |   81 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|        - |   82 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|        - |   83 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|        - |   84 | ` * so callers can bail from the current construct).` |
|        - |   85 | ` */` |
|  3809962 |   86 | `PH7_PRIVATE sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|        5 |   87 | `{` |
|  3809967 |   88 | `	const char *zBad = 0;` |
|  3809967 |   89 | `	sxu32 nBad = 0;` |
|        - |   90 | `	SyString sBad;` |
|        - |   91 | `	sxi32 rc;` |
|  3809967 |   92 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|  3809957 |   93 | `		return SXRET_OK;` |
|        - |   94 | `	}` |
|       14 |   95 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|       14 |   96 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|        - |   97 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|       14 |   98 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |   99 | `		return SXERR_ABORT;` |
|        - |  100 | `	}` |
|       14 |  101 | `	return SXERR_SYNTAX;` |
|  1904986 |  102 | `}` |
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
|  3809952 |  119 | `PH7_PRIVATE sxi32 GenStateStripNumericSeparators(` |
|        - |  120 | `	SyMemBackend *pAlloc,` |
|        - |  121 | `	const SyString *pToken,` |
|        - |  122 | `	char *zScratch, sxu32 nScratch,` |
|        - |  123 | `	SyString *pOut, char **pzAlloc)` |
|        5 |  124 | `{` |
|        - |  125 | `	sxu32 i, j;` |
|  3809957 |  126 | `	int hasUnderscore = 0;` |
|        - |  127 | `	char *zBuf;` |
|  3809957 |  128 | `	*pzAlloc = 0;` |
|  9069181 |  129 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|  5259489 |  130 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|  2629617 |  131 | `	}` |
|  3809957 |  132 | `	if( !hasUnderscore ){` |
|  3809697 |  133 | `		SyStringDupPtr(pOut, pToken);` |
|  3809697 |  134 | `		return SXRET_OK;` |
|        - |  135 | `	}` |
|      261 |  136 | `	if( pToken->nByte <= nScratch ){` |
|      259 |  137 | `		zBuf = zScratch;` |
|      130 |  138 | `	}else{` |
|        3 |  139 | `		zBuf = (char *)SyMemBackendAlloc(pAlloc, pToken->nByte);` |
|        3 |  140 | `		if( zBuf == 0 ){` |
|      ! 0 |  141 | `			return SXERR_ABORT;` |
|        - |  142 | `		}` |
|        3 |  143 | `		*pzAlloc = zBuf;` |
|        - |  144 | `	}` |
|      261 |  145 | `	j = 0;` |
|     2949 |  146 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|     2689 |  147 | `		if( pToken->zString[i] != '_' ){ zBuf[j++] = pToken->zString[i]; }` |
|     1345 |  148 | `	}` |
|      261 |  149 | `	SyStringInitFromBuf(pOut, zBuf, j);` |
|      261 |  150 | `	return SXRET_OK;` |
|  1904981 |  151 | `}` |
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
|  3801200 |  187 | `static int GenStateIntLiteralOverflows(const SyString *pNum, ph7_real *pReal, int *pbDecimal)` |
|        5 |  188 | `{` |
|  3801205 |  189 | `	const char *z = pNum->zString;` |
|  3801205 |  190 | `	const char *zEnd = z + pNum->nByte;` |
|        - |  191 | `	const char *p, *q;` |
|        - |  192 | `	int n;` |
|  3801205 |  193 | `	*pbDecimal = FALSE;` |
|  3801205 |  194 | `	if( z >= zEnd ){` |
|      ! 0 |  195 | `		return FALSE;` |
|        - |  196 | `	}` |
|  3801205 |  197 | `	if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|        - |  198 | `		/* Hexadecimal: INT64_MAX == 0x7FFF...F (16 digits, leading nibble 7). */` |
|   104429 |  199 | `		p = z + 2;` |
|   131485 |  200 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|   425625 |  201 | `		for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisHex(q[0]); q++ ){ n++; }` |
|   104429 |  202 | `		if( n < 16 \|\| (n == 16 && SyHexToint(p[0]) < 8) ){` |
|   104423 |  203 | `			return FALSE;` |
|        - |  204 | `		}` |
|        7 |  205 | `		{ ph7_real dv = 0;` |
|      103 |  206 | `		  for( q = p; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisHex(q[0]); q++ ){` |
|       97 |  207 | `			dv = dv * 16 + (ph7_real)SyHexToint(q[0]);` |
|       49 |  208 | `		  }` |
|        7 |  209 | `		  *pReal = dv;` |
|        - |  210 | `		}` |
|        7 |  211 | `		return TRUE;` |
|  3696781 |  212 | `	}else if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'b' \|\| z[1] == 'B') ){` |
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
|  3696495 |  227 | `	}else if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'o' \|\| z[1] == 'O') ){` |
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
|  3696475 |  242 | `	}else if( z[0] == '0' ){` |
|        - |  243 | `		/* Octal: INT64_MAX == 0o777...7 (21 significant octal digits). Skip the` |
|        - |  244 | `		 * leading zeros (incl. the base '0'); a non-octal char such as the 8.1` |
|        - |  245 | `		 * "0o" marker ends the run and leaves it to the int path (as today). */` |
|  1389675 |  246 | `		p = z;` |
|  2779351 |  247 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|  1401529 |  248 | `		for( q = p, n = 0; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){ n++; }` |
|  1389675 |  249 | `		if( n <= 21 ){` |
|  1389673 |  250 | `			return FALSE;` |
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
|  2306805 |  263 | `	p = z;` |
|  2306805 |  264 | `	while( p < zEnd && p[0] == '0' ){ p++; }` |
|  5578023 |  265 | `	for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisDigit(q[0]); q++ ){ n++; }` |
|  2306805 |  266 | `	if( n > 19 \|\| (n == 19 && SyMemcmp(p, "9223372036854775807", 19) > 0) ){` |
|       25 |  267 | `		*pbDecimal = TRUE;` |
|       25 |  268 | `		return TRUE;` |
|        - |  269 | `	}` |
|  2306781 |  270 | `	return FALSE;` |
|  1900605 |  271 | `}` |
|  3809934 |  272 | `PH7_PRIVATE sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  273 | `{` |
|  3809939 |  274 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|  3809939 |  275 | `	sxu32 nIdx = 0;` |
|        - |  276 | `	char zScratch[GEN_NUM_SCRATCH];` |
|  3809939 |  277 | `	char *zAlloc = 0;` |
|        - |  278 | `	SyString sNum;` |
|        - |  279 | `	sxi32 rc;` |
|  1904967 |  280 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|  3809939 |  281 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|  3809939 |  282 | `	if( rc != SXRET_OK ){` |
|        9 |  283 | `		return rc;` |
|        - |  284 | `	}` |
|  5714897 |  285 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|  1904964 |  286 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|  3809933 |  287 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  288 | `		return SXERR_ABORT;` |
|        - |  289 | `	}` |
|  3809933 |  290 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|        - |  291 | `		ph7_value *pObj;` |
|        - |  292 | `		sxi64 iValue;` |
|  3801205 |  293 | `		ph7_real rOverflow = 0;` |
|  3801205 |  294 | `		int bDecimalOverflow = 0;` |
|  3801205 |  295 | `		if( GenStateIntLiteralOverflows(&sNum,&rOverflow,&bDecimalOverflow) ){` |
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
|  3801171 |  312 | `			iValue = PH7_TokenValueToInt64(&sNum);` |
|  3801171 |  313 | `			pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|  3801171 |  314 | `			if( pObj == 0 ){` |
|      ! 0 |  315 | `				if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|      ! 0 |  316 | `				return SXERR_ABORT;` |
|        - |  317 | `			}` |
|  3801171 |  318 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|        - |  319 | `		}` |
|  1900605 |  320 | `	}else{` |
|        - |  321 | `		/* Real number */` |
|        - |  322 | `		ph7_value *pObj;` |
|        - |  323 | `		/* Reserve a new constant */` |
|     8733 |  324 | `		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     8733 |  325 | `		if( pObj == 0 ){` |
|      ! 0 |  326 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |  327 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|      ! 0 |  328 | `			return SXERR_ABORT;` |
|        - |  329 | `		}` |
|     8733 |  330 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|     8733 |  331 | `		PH7_MemObjToReal(pObj);` |
|        - |  332 | `	}` |
|  3809933 |  333 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|        - |  334 | `	/* Emit the load constant instruction */` |
|  3809933 |  335 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        - |  336 | `	/* Node successfully compiled */` |
|  3809933 |  337 | `	return SXRET_OK;` |
|  1904972 |  338 | `}` |
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
|  5538560 |  350 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  351 | `{` |
|  5538565 |  352 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|        - |  353 | `	const char *zIn,*zCur,*zEnd;` |
|        - |  354 | `	ph7_value *pObj;` |
|        - |  355 | `	sxu32 nIdx;` |
|        - |  356 | `	sxi32 bHasEsc;` |
|  5538565 |  357 | `	nIdx = 0; /* Prevent compiler warning */` |
|        - |  358 | `	/* Delimit the string */` |
|  5538565 |  359 | `	zIn  = pStr->zString;` |
|  5538565 |  360 | `	zEnd = &zIn[pStr->nByte];` |
|  5538565 |  361 | `	if( zIn >= zEnd ){` |
|        - |  362 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|        - |  363 | `		 * rather than reserving a new object each time. */` |
|   405981 |  364 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|   405981 |  365 | `		return SXRET_OK;` |
|        - |  366 | `	}` |
|        - |  367 | `	/* A single-quoted literal whose raw source holds a backslash unescapes to a` |
|        - |  368 | `	 * value that differs from that source (\\ -> \, \' -> '). The literal cache` |
|        - |  369 | `	 * keys FIND on the raw source text but INSTALL on the unescaped value, so` |
|        - |  370 | `	 * caching such a literal lets an unrelated one collide with it — e.g. '\\'` |
|        - |  371 | `	 * (raw \\, value \) would find the entry installed for '\\\\' (raw \\\\,` |
|        - |  372 | `	 * value \\) and load two backslashes. Only cache literals whose value equals` |
|        - |  373 | `	 * their source, i.e. those with no backslash to unescape. */` |
|  5132589 |  374 | `	bHasEsc = 0;` |
|        - |  375 | `	{` |
|        - |  376 | `		const char *zScan;` |
| 61333471 |  377 | `		for( zScan = zIn ; zScan < zEnd ; zScan++ ){` |
| 56282259 |  378 | `			if( zScan[0] == '\\' ){ bHasEsc = 1; break; }` |
| 28100446 |  379 | `		}` |
|        - |  380 | `	}` |
|  5132589 |  381 | `	if( !bHasEsc && SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|        - |  382 | `		/* Already processed,emit the load constant instruction` |
|        - |  383 | `		 * and return.` |
|        - |  384 | `		 */` |
|  2995089 |  385 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|  2995089 |  386 | `		return SXRET_OK;` |
|        - |  387 | `	}` |
|        - |  388 | `	/* Reserve a new constant */` |
|  2137505 |  389 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  2137505 |  390 | `	if( pObj == 0 ){` |
|      ! 0 |  391 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |  392 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|      ! 0 |  393 | `		return SXERR_ABORT;` |
|        - |  394 | `	}` |
|  2137505 |  395 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|        - |  396 | `	/* Compile the node */` |
|  2189796 |  397 | `	for(;;){` |
|  4379597 |  398 | `		if( zIn >= zEnd ){` |
|        - |  399 | `			/* End of input */` |
|  2137505 |  400 | `			break;` |
|        - |  401 | `		}` |
|  2242097 |  402 | `		zCur = zIn;` |
| 43326709 |  403 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
| 41084617 |  404 | `			zIn++;` |
|        5 |  405 | `		}` |
|  2242097 |  406 | `		if( zIn > zCur ){` |
|        - |  407 | `			/* Append raw contents*/` |
|  2199469 |  408 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|  1099732 |  409 | `		}` |
|  2242097 |  410 | `		zIn++;` |
|  2242097 |  411 | `		if( zIn < zEnd ){` |
|   143287 |  412 | `			if( zIn[0] == '\\' ){` |
|        - |  413 | `				/* A literal backslash */` |
|    34899 |  414 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|   125840 |  415 | `			}else if( zIn[0] == '\'' ){` |
|        - |  416 | `				/* A single quote */` |
|       15 |  417 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|        8 |  418 | `			}else{` |
|        - |  419 | `				/* verbatim copy */` |
|   108379 |  420 | `				zIn--;` |
|   108379 |  421 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char)*2);` |
|   108379 |  422 | `				zIn++;` |
|        - |  423 | `			}` |
|    71641 |  424 | `		}` |
|        - |  425 | `		/* Advance the stream cursor */` |
|  2242097 |  426 | `		zIn++;` |
|        5 |  427 | `	}` |
|        - |  428 | `	/* Emit the load constant instruction */` |
|  2137505 |  429 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|  2137505 |  430 | `	if( !bHasEsc && pStr->nByte < 1024 ){` |
|        - |  431 | `		/* Install in the literal table (only when value == source; see above) */` |
|  2056133 |  432 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|  1028064 |  433 | `	}` |
|        - |  434 | `	/* Node successfully compiled */` |
|  2137505 |  435 | `	return SXRET_OK;` |
|  2769285 |  436 | `}` |
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
|        3 |  549 | `{` |
|        - |  550 | `	SyString sStripped;` |
|        - |  551 | `	SyString *pStr;` |
|        - |  552 | `	ph7_value *pObj;` |
|        - |  553 | `	sxu32 nIdx;` |
|        - |  554 | `	sxi32 rc;` |
|       55 |  555 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|       55 |  556 | `	if( rc != SXRET_OK ){` |
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
|       29 |  580 | `}` |
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
|     2332 |  603 | `static sxi32 GenStateProcessStringExpression(` |
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
|     2337 |  614 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|        - |  615 | `	/* Preallocate some slots */` |
|     2337 |  616 | `	SySetAlloc(&sToken,0x08);` |
|        - |  617 | `	/* Tokenize the text */` |
|     2337 |  618 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken,0);` |
|        - |  619 | `	/* Swap delimiter */` |
|     2337 |  620 | `	pTmpIn  = pGen->pIn;` |
|     2337 |  621 | `	pTmpEnd = pGen->pEnd;` |
|     2337 |  622 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|     2337 |  623 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|        - |  624 | ``	/* Compile the expression. An interpolated `"...$x..."` READS $x — php warns`` |
|        - |  625 | `	 * "Undefined variable $x" and substitutes the empty string — so ask for a` |
|        - |  626 | `	 * read-only load rather than letting the default vivify it silently. */` |
|     2337 |  627 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|        - |  628 | `	/* Restore token stream */` |
|     2337 |  629 | `	pGen->pIn  = pTmpIn;` |
|     2337 |  630 | `	pGen->pEnd = pTmpEnd;` |
|        - |  631 | `	/* Release the token set */` |
|     2337 |  632 | `	SySetRelease(&sToken);` |
|        - |  633 | `	/* Compilation result */` |
|     2337 |  634 | `	return rc;` |
|        5 |  635 | `}` |
|        - |  636 | `/*` |
|        - |  637 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|        - |  638 | ` */` |
|   126228 |  639 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|        5 |  640 | `{` |
|        - |  641 | `	ph7_value *pConstObj;` |
|   126233 |  642 | `	sxu32 nIdx = 0;` |
|        - |  643 | `	/* Reserve a new constant */` |
|   126233 |  644 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   126233 |  645 | `	if( pConstObj == 0 ){` |
|      ! 0 |  646 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|      ! 0 |  647 | `		return 0;` |
|        - |  648 | `	}` |
|   126233 |  649 | `	(*pCount)++;` |
|   126233 |  650 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|        - |  651 | `	/* Emit the load constant instruction */` |
|   126233 |  652 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   126233 |  653 | `	return pConstObj;` |
|    63119 |  654 | `}` |
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
|   125034 |  717 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen,int bHeredoc)` |
|        5 |  718 | `{` |
|   125039 |  719 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|        - |  720 | `	const char *zIn,*zCur,*zEnd;` |
|   125039 |  721 | `	ph7_value *pObj = 0;` |
|        - |  722 | `	sxi32 iCons;` |
|        - |  723 | `	sxi32 nInterp;   /* how many of iCons came from an interpolated EXPRESSION */` |
|        - |  724 | `	sxi32 rc;` |
|        - |  725 | `	/* Delimit the string */` |
|   125039 |  726 | `	zIn  = pStr->zString;` |
|   125039 |  727 | `	zEnd = &zIn[pStr->nByte];` |
|   125039 |  728 | `	if( zIn >= zEnd ){` |
|        - |  729 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|        - |  730 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|        - |  731 | `		 * literal table from growing when many "" literals appear in the source.` |
|        - |  732 | `		 */` |
|      445 |  733 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|      445 |  734 | `		return SXRET_OK;` |
|        - |  735 | `	}` |
|   124599 |  736 | `	zCur = 0;` |
|        - |  737 | `	/* Compile the node */` |
|   124599 |  738 | `	iCons = 0;` |
|   124599 |  739 | `	nInterp = 0;` |
|    63462 |  740 | `	for(;;){` |
|   171341 |  741 | `		zCur = zIn;` |
|  1673689 |  742 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  1504687 |  743 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|       85 |  744 | `				break;` |
|  1504526 |  745 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|     2178 |  746 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|     1089 |  747 | `					break;` |
|        - |  748 | `			}` |
|  1502353 |  749 | `			zIn++;` |
|        5 |  750 | `		}` |
|   171341 |  751 | `		if( zIn > zCur ){` |
|    95587 |  752 | `			if( pObj == 0 ){` |
|    94901 |  753 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    94901 |  754 | `				if( pObj == 0 ){` |
|      ! 0 |  755 | `					return SXERR_ABORT;` |
|        - |  756 | `				}` |
|    47448 |  757 | `			}` |
|    95587 |  758 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    47791 |  759 | `		}` |
|   171341 |  760 | `		if( zIn >= zEnd ){` |
|   124595 |  761 | `			break;` |
|        - |  762 | `		}` |
|    46751 |  763 | `		if( zIn[0] == '\\' ){` |
|    44417 |  764 | `			const char *zPtr = 0;` |
|        - |  765 | `			sxu32 n;` |
|    44417 |  766 | `			zIn++;` |
|    44417 |  767 | `			if( pObj == 0 ){` |
|    31337 |  768 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    31337 |  769 | `				if( pObj == 0 ){` |
|      ! 0 |  770 | `					return SXERR_ABORT;` |
|        - |  771 | `				}` |
|    15666 |  772 | `			}` |
|    44417 |  773 | `			if( zIn >= zEnd ){` |
|        - |  774 | `				/* Lone backslash at the very end of the body: php keeps it */` |
|        3 |  775 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|        3 |  776 | `				break;` |
|        - |  777 | `			}` |
|    44415 |  778 | `			n = sizeof(char); /* size of conversion */` |
|    44415 |  779 | `			switch( zIn[0] ){` |
|       27 |  780 | `			case '$':` |
|        - |  781 | `				/* Dollar sign */` |
|       57 |  782 | `				PH7_MemObjStringAppend(pObj,"$",sizeof(char));` |
|       57 |  783 | `				break;` |
|       62 |  784 | `			case '\\':` |
|        - |  785 | `				/* A literal backslash */` |
|      129 |  786 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|      129 |  787 | `				break;` |
|        1 |  788 | `			case 'e':` |
|        - |  789 | `				/* Escape (ESC) ASCII code 27 */` |
|        3 |  790 | `				PH7_MemObjStringAppend(pObj,"\x1b",sizeof(char));` |
|        3 |  791 | `				break;` |
|        4 |  792 | `			case 'f':` |
|        - |  793 | `				/* Form-feed (FF)[ctrl+l] ASCII code 12 */` |
|        9 |  794 | `				PH7_MemObjStringAppend(pObj,"\f",sizeof(char));` |
|        9 |  795 | `				break;` |
|    19673 |  796 | `			case 'n':` |
|        - |  797 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|    39351 |  798 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|    39351 |  799 | `				break;` |
|       27 |  800 | `			case 'r':` |
|        - |  801 | `				/* Carriage return (CR)[ctrl+m] ASCII code 13 */` |
|       59 |  802 | `				PH7_MemObjStringAppend(pObj,"\r",sizeof(char));` |
|       59 |  803 | `				break;` |
|     1964 |  804 | `			case 't':` |
|        - |  805 | `				/* Horizontal tab (HT)[ctrl+i] ASCII code 9 */` |
|     3933 |  806 | `				PH7_MemObjStringAppend(pObj,"\t",sizeof(char));` |
|     3933 |  807 | `				break;` |
|        3 |  808 | `			case 'v':` |
|        - |  809 | `				/* Vertical tab(VT)[ctrl+k] ASCII code 11 */` |
|        7 |  810 | `				PH7_MemObjStringAppend(pObj,"\v",sizeof(char));` |
|        7 |  811 | `				break;` |
|      147 |  812 | `			case '"':` |
|      298 |  813 | `				if( bHeredoc ){` |
|        - |  814 | `					/* No active quote char in a heredoc: php keeps \" verbatim */` |
|        5 |  815 | `					PH7_MemObjStringAppend(pObj,"\\\"",sizeof(char)*2);` |
|        3 |  816 | `				}else{` |
|        - |  817 | `					/* Double quote */` |
|      294 |  818 | `					PH7_MemObjStringAppend(pObj,"\"",sizeof(char));` |
|        - |  819 | `				}` |
|      298 |  820 | `				break;` |
|       26 |  821 | `			case '0': case '1': case '2': case '3':` |
|        - |  822 | `			case '4': case '5': case '6': case '7': {` |
|        - |  823 | `				/* \[0-7]{1,3}: a character in octal notation. A value above \377` |
|        - |  824 | `				 * warns and wraps to the low byte, matching php 8. */` |
|       55 |  825 | `				int c = 0;` |
|        - |  826 | `				char cOut;` |
|      153 |  827 | `				for( zPtr = zIn ; zPtr < &zIn[3*sizeof(char)] ; zPtr++ ){` |
|      131 |  828 | `					if( zPtr >= zEnd \|\| zPtr[0] < '0' \|\| zPtr[0] > '7' ){` |
|       17 |  829 | `						break;` |
|        - |  830 | `					}` |
|      101 |  831 | `					c = c * 8 + (zPtr[0] - '0');` |
|       52 |  832 | `				}` |
|       55 |  833 | `				if( c > 0xFF ){` |
|        - |  834 | `					SyString sSeq;` |
|        3 |  835 | `					SyStringInitFromBuf(&sSeq,zIn,(sxu32)(zPtr-zIn));` |
|        3 |  836 | `					PH7_GenCompileError(&(*pGen),E_WARNING,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|        - |  837 | `						"Octal escape sequence overflow \\%z is greater than \\377",&sSeq);` |
|        3 |  838 | `					c &= 0xFF;` |
|        1 |  839 | `				}` |
|       55 |  840 | `				cOut = (char)c; /* value byte, independent of host endianness */` |
|       55 |  841 | `				PH7_MemObjStringAppend(pObj,&cOut,sizeof(char));` |
|       55 |  842 | `				n = (sxu32)(zPtr-zIn);` |
|       55 |  843 | `				break;` |
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
|    44415 |  922 | `			zIn += n;` |
|    44415 |  923 | `			continue;` |
|        - |  924 | `		}` |
|     2339 |  925 | `		if( zIn[0] == '{' ){` |
|        - |  926 | `			/* Curly syntax */` |
|        - |  927 | `			const char *zExpr;` |
|      168 |  928 | `			sxi32 iNest = 1;` |
|      168 |  929 | `			zIn++;` |
|      168 |  930 | `			zExpr = zIn;` |
|        - |  931 | `			/* Synchronize with the next closing curly braces */` |
|     1542 |  932 | `			while( zIn < zEnd ){` |
|     1542 |  933 | `				if( zIn[0] == '{' ){` |
|        - |  934 | `					/* Increment nesting level */` |
|        3 |  935 | `					iNest++;` |
|     1541 |  936 | `				}else if(zIn[0] == '}' ){` |
|        - |  937 | `					/* Decrement nesting level */` |
|      170 |  938 | `					iNest--;` |
|      170 |  939 | `					if( iNest <= 0 ){` |
|      168 |  940 | `						break;` |
|        - |  941 | `					}` |
|        1 |  942 | `				}` |
|     1376 |  943 | `				zIn++;` |
|        2 |  944 | `			}` |
|        - |  945 | `			/* Process the expression */` |
|      168 |  946 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|      168 |  947 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  948 | `				return SXERR_ABORT;` |
|        - |  949 | `			}` |
|      168 |  950 | `			if( rc != SXERR_EMPTY ){` |
|      168 |  951 | `				++iCons;` |
|      168 |  952 | `				++nInterp;` |
|       83 |  953 | `			}` |
|      168 |  954 | `			if( zIn < zEnd ){` |
|        - |  955 | `				/* Jump the trailing curly */` |
|      168 |  956 | `				zIn++;` |
|       83 |  957 | `			}` |
|       85 |  958 | `		}else{` |
|        - |  959 | `			/* Simple syntax */` |
|     2173 |  960 | `			const char *zExpr = zIn;` |
|        - |  961 | `			/* Assemble variable name */` |
|     1109 |  962 | `			for(;;){` |
|        - |  963 | `				/* Jump leading dollars */` |
|     4391 |  964 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|     2173 |  965 | `					zIn++;` |
|        5 |  966 | `				}` |
|     1109 |  967 | `				for(;;){` |
|    10494 |  968 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|     7167 |  969 | `						zIn++;` |
|        5 |  970 | `					}` |
|     2223 |  971 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|        - |  972 | `						/* UTF-8 stream */` |
|      ! 0 |  973 | `						zIn++;` |
|      ! 0 |  974 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  975 | `							zIn++;` |
|      ! 0 |  976 | `						}` |
|      ! 0 |  977 | `						continue;` |
|        - |  978 | `					}` |
|     2223 |  979 | `					break;` |
|      ! 0 |  980 | `				}` |
|     2223 |  981 | `				if( zIn >= zEnd ){` |
|      293 |  982 | `					break;` |
|        - |  983 | `				}` |
|     1935 |  984 | `				if( zIn[0] == '[' ){` |
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
|     1925 | 1002 | `				}else if(zIn[0] == '{' ){` |
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
|     1923 | 1020 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|        - | 1021 | `					/* Member access operator '->' */` |
|       53 | 1022 | `					zIn += 2;` |
|     1898 | 1023 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|        - | 1024 | `					/* Static member access operator '::' */` |
|      ! 0 | 1025 | `					zIn += 2;` |
|      ! 0 | 1026 | `				}else{` |
|      939 | 1027 | `					break;` |
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
|     2173 | 1039 | `				const char *zBr = zExpr;` |
|    11615 | 1040 | `				while( zBr < zIn && zBr[0] != '[' ){` |
|     9447 | 1041 | `					zBr++;` |
|        5 | 1042 | `				}` |
|     2173 | 1043 | `				if( zBr < zIn && zIn[-1] == ']' ){` |
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
|     2171 | 1084 | `			if( &zExpr[1] < zIn && zExpr[0] == '$' && zExpr[1] == '{' ){` |
|        3 | 1085 | `				PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|        - | 1086 | `					"syntax error, \"${\" string interpolation was removed in php 8.2, use \"{$...}\" instead");` |
|        3 | 1087 | `				return SXERR_ABORT;` |
|        - | 1088 | `			}` |
|        - | 1089 | `			/* Process the expression */` |
|     2169 | 1090 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|     2169 | 1091 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1092 | `				return SXERR_ABORT;` |
|        - | 1093 | `			}` |
|     2169 | 1094 | `			if( rc != SXERR_EMPTY ){` |
|     2169 | 1095 | `				++iCons;` |
|     2169 | 1096 | `				++nInterp;` |
|     1082 | 1097 | `			}` |
|        - | 1098 | `		}` |
|        - | 1099 | `		/* Invalidate the previously used constant */` |
|     2335 | 1100 | `		pObj = 0;` |
|        5 | 1101 | `	}/*for(;;)*/` |
|   124597 | 1102 | `	if( iCons > 1 ){` |
|        - | 1103 | `		/* Concatenate all compiled constants */` |
|     1601 | 1104 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|   123799 | 1105 | `	}else if( iCons == 1 && nInterp == 1 ){` |
|        - | 1106 | `		/* A string that is nothing but one interpolation ("$x") still has to` |
|        - | 1107 | `		 * PRODUCE A STRING. With no CAT to force the conversion the operand was` |
|        - | 1108 | ``		 * left on the stack untouched, so `$s = "$x"` handed back $x's own type:`` |
|        - | 1109 | `		 * "$arr" stayed an array (and skipped php's "Array to string conversion"` |
|        - | 1110 | `		 * warning), "$int" stayed an int, "$res" stayed a resource. */` |
|       13 | 1111 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CVT_STR,0,0,0,0);` |
|        6 | 1112 | `	}` |
|        - | 1113 | `	/* Node successfully compiled */` |
|   124597 | 1114 | `	return SXRET_OK;` |
|    62522 | 1115 | `}` |
|        - | 1116 | `/*` |
|        - | 1117 | ` * Compile a double quoted string.` |
|        - | 1118 | ` *  See the block-comment above for more information.` |
|        - | 1119 | ` */` |
|   124970 | 1120 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 | 1121 | `{` |
|        - | 1122 | `	sxi32 rc;` |
|   124975 | 1123 | `	rc = GenStateCompileString(&(*pGen),0/*bHeredoc*/);` |
|    62485 | 1124 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        - | 1125 | `	/* Compilation result */` |
|   124975 | 1126 | `	return rc;` |
|        5 | 1127 | `}` |
|        - | 1128 | `/*` |
|        - | 1129 | ` * Compile a Heredoc string.` |
|        - | 1130 | ` *  See the block-comment above for more information.` |
|        - | 1131 | ` */` |
|       68 | 1132 | `PH7_PRIVATE sxi32 PH7_CompileHereDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 | 1133 | `{` |
|        - | 1134 | `	SyString sOrig, sStripped;` |
|        - | 1135 | `	sxi32 rc;` |
|       73 | 1136 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|       73 | 1137 | `	if( rc != SXRET_OK ){` |
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
|       39 | 1150 | `}` |
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
|  1559564 | 1170 | `PH7_PRIVATE sxi32 GenStateCompileArrayEntry(` |
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
|  1559569 | 1181 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|        - | 1182 | `	/* Compile the expression*/` |
|  1559569 | 1183 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|        - | 1184 | `	/* Restore token stream */` |
|  1559569 | 1185 | `	RE_SWAP_DELIMITER(pGen);` |
|  1559569 | 1186 | `	return rc;` |
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
|       17 | 1202 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|        - | 1203 | `			/* Unexpected expression */` |
|       14 | 1204 | `			rc = PH7_GenSyntaxError(&(*pGen),pRoot->pStart,"\"->\" or \"?->\" or \"[\"");` |
|       14 | 1205 | `			if( rc != SXERR_ABORT ){` |
|       14 | 1206 | `				rc = SXERR_INVALID;` |
|        5 | 1207 | `			}` |
|       10 | 1208 | `		}` |
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
|  1484824 | 1225 | `PH7_PRIVATE SyToken * GenStateFindTopLevelArrow(SyToken *pStart,SyToken *pEnd)` |
|        5 | 1226 | `{` |
|  1484829 | 1227 | `	SyToken *pCur = pStart;` |
|  1484829 | 1228 | `	sxi32 iNest = 0;` |
|  3793137 | 1229 | `	while( pCur < pEnd ){` |
|  2844121 | 1230 | `		if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|   535807 | 1231 | `			return pCur;` |
|        - | 1232 | `		}` |
|        - | 1233 | `		/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|        - | 1234 | `		 * The '=>' inside an arrow function introduces the expression body,` |
|        - | 1235 | `		 * not an entry separator. Skip past the signature.` |
|        - | 1236 | `		 */` |
|  2308319 | 1237 | `		if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
|    23297 | 1238 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|    23297 | 1239 | `			SyToken *pFn = pCur;` |
|    23292 | 1240 | `			if( nKw == PH7_TKWRD_STATIC && &pCur[1] < pEnd` |
|      ! 0 | 1241 | `				&& (pCur[1].nType & PH7_TK_KEYWORD)` |
|        5 | 1242 | `				&& SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ){` |
|      ! 0 | 1243 | `				pFn = &pCur[1];` |
|      ! 0 | 1244 | `				nKw = PH7_TKWRD_FN;` |
|      ! 0 | 1245 | `			}` |
|    23297 | 1246 | `			if( nKw == PH7_TKWRD_FN ){` |
|        8 | 1247 | `				pCur = pFn + 1; /* past 'fn' */` |
|        8 | 1248 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_AMPER) ){` |
|      ! 0 | 1249 | `					pCur++;` |
|      ! 0 | 1250 | `				}` |
|        8 | 1251 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|        5 | 1252 | `					pCur++;` |
|        5 | 1253 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|        - | 1254 | `						PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|        5 | 1255 | `					if( pCur < pEnd ){` |
|        5 | 1256 | `						pCur++;` |
|        2 | 1257 | `					}` |
|        2 | 1258 | `				}` |
|        8 | 1259 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_COLON) ){` |
|      ! 0 | 1260 | `					pCur++;` |
|      ! 0 | 1261 | `					if( pCur < pEnd && (pCur->nType & PH7_TK_OP)` |
|      ! 0 | 1262 | `						&& pCur->sData.nByte == 1` |
|      ! 0 | 1263 | `						&& pCur->sData.zString[0] == '?' ){` |
|      ! 0 | 1264 | `						pCur++;` |
|      ! 0 | 1265 | `					}` |
|      ! 0 | 1266 | `					if( pCur < pEnd` |
|      ! 0 | 1267 | `						&& (pCur->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|      ! 0 | 1268 | `						pCur++;` |
|      ! 0 | 1269 | `					}` |
|      ! 0 | 1270 | `				}` |
|        - | 1271 | `				/* The rest of the entry is the arrow-function body — no outer` |
|        - | 1272 | `				 * key to extract. */` |
|        8 | 1273 | `				return pEnd;` |
|        - | 1274 | `			}` |
|        - | 1275 | `			/* Match expression (PHP 8.0): the '=>' inside match arms is not an` |
|        - | 1276 | `			 * entry separator. Skip past the full match span. */` |
|    23291 | 1277 | `			if( nKw == PH7_TKWRD_MATCH ){` |
|        3 | 1278 | `				pCur++; /* past 'match' */` |
|        3 | 1279 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|        3 | 1280 | `					pCur++;` |
|        3 | 1281 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|        - | 1282 | `						PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|        3 | 1283 | `					if( pCur < pEnd ){` |
|        3 | 1284 | `						pCur++;` |
|        1 | 1285 | `					}` |
|        1 | 1286 | `				}` |
|        3 | 1287 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_OCB) ){` |
|        3 | 1288 | `					pCur++;` |
|        3 | 1289 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|        - | 1290 | `						PH7_TK_OCB,PH7_TK_CCB,&pCur);` |
|        3 | 1291 | `					if( pCur < pEnd ){` |
|        3 | 1292 | `						pCur++;` |
|        1 | 1293 | `					}` |
|        1 | 1294 | `				}` |
|        3 | 1295 | `				continue;` |
|        - | 1296 | `			}` |
|    11642 | 1297 | `		}` |
|  2308311 | 1298 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|    54753 | 1299 | `			iNest++;` |
|  2280937 | 1300 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/) ){` |
|        - | 1301 | `			/* Don't worry about mismatched brackets here, the expression` |
|        - | 1302 | `			 * parser will shortly detect any syntax error. */` |
|    54753 | 1303 | `			iNest--;` |
|    27374 | 1304 | `		}` |
|  2308311 | 1305 | `		pCur++;` |
|        5 | 1306 | `	}` |
|   949021 | 1307 | `	return pEnd;` |
|   742417 | 1308 | `}` |
|        - | 1309 | `/*` |
|        - | 1310 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|        - | 1311 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|        - | 1312 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|        - | 1313 | ` */` |
|   669030 | 1314 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|        5 | 1315 | `{` |
|        - | 1316 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|        - | 1317 | `	SyToken *pKey,*pCur;` |
|   669035 | 1318 | `	sxi32 iEmitRef = 0;` |
|   669035 | 1319 | `	sxi32 iSpread = 0;` |
|   669035 | 1320 | `	sxi32 nPair = 0;` |
|        - | 1321 | `	sxi32 rc;` |
|   669035 | 1322 | `	xValidator = 0;` |
|   914067 | 1323 | `	for(;;){` |
|        - | 1324 | `		/* Jump leading commas. Exactly ONE separates two entries; a second one (or a comma` |
|        - | 1325 | `		 * at the very start) means an EMPTY element, which php rejects outright — PH7 just` |
|        - | 1326 | ``		 * skipped them, so `array(,)` and `array(1,,2)` compiled silently. A TRAILING comma`` |
|        - | 1327 | `		 * is legal and is handled by the loop exiting on the next pass. */` |
|   579552 | 1328 | `		{` |
|  1828139 | 1329 | `			int nSkip = 0;` |
|  2713119 | 1330 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|   884985 | 1331 | `				nSkip++;` |
|   884985 | 1332 | `				pGen->pIn++;` |
|        5 | 1333 | `			}` |
|  1828139 | 1334 | `			if( nSkip > 1 \|\| (nSkip > 0 && nPair < 1) ){` |
|      ! 0 | 1335 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn[-1].nLine,` |
|        - | 1336 | `					"Cannot use empty array elements in arrays");` |
|      ! 0 | 1337 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1338 | `					return SXERR_ABORT;` |
|        - | 1339 | `				}` |
|      ! 0 | 1340 | `				return SXRET_OK;` |
|        - | 1341 | `			}` |
|        - | 1342 | `		}` |
|  1828139 | 1343 | `		pCur = pGen->pIn;` |
|  1828139 | 1344 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|        - | 1345 | `			/* No more entry to process */` |
|   669017 | 1346 | `			break;` |
|        - | 1347 | `		}` |
|  1159127 | 1348 | `		if( pCur >= pGen->pIn ){` |
|      ! 0 | 1349 | `			continue;` |
|        - | 1350 | `		}` |
|        - | 1351 | `		/* Compile the key if available */` |
|  1159127 | 1352 | `		pKey = pCur;` |
|  1159127 | 1353 | `		pCur = GenStateFindTopLevelArrow(pCur,pGen->pIn);` |
|  1159127 | 1354 | `		rc = SXERR_EMPTY;` |
|  1159127 | 1355 | `		if( pCur < pGen->pIn ){` |
|   400171 | 1356 | `			if( pKey == pCur ){` |
|        - | 1357 | ``				/* `array( => 2)`: the entry STARTS with '=>', so it has no key. php rejects`` |
|        - | 1358 | `				 * it; PH7 warned about a "Missing entry key" and compiled on, accepting` |
|        - | 1359 | ``				 * source php refuses. (The `else if` below could never see this: the arrow`` |
|        - | 1360 | `				 * IS found here, so control never reached it.)` |
|        - | 1361 | `				 * php names the literal's own closer, so short syntax expects ']'. */` |
|        3 | 1362 | `				const char *zClose = (pGen->pEnd && (pGen->pEnd->nType & PH7_TK_CSB))` |
|        - | 1363 | `					? "\"]\"" : "\")\"";` |
|        3 | 1364 | `				rc = PH7_GenSyntaxError(&(*pGen),pCur,zClose);` |
|        3 | 1365 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1366 | `					return SXERR_ABORT;` |
|        - | 1367 | `				}` |
|        3 | 1368 | `				return SXRET_OK;` |
|        - | 1369 | `			}` |
|   400169 | 1370 | `			if( &pCur[1] >= pGen->pIn ){` |
|        - | 1371 | ``				/* `array(1 => )`: php names the token that SHOULD have started the value —`` |
|        - | 1372 | `				 * the ')' or ']' closing the literal — not the '=>' it just read. Passing 0` |
|        - | 1373 | `				 * makes the helper reach for the token past this entry's slice. */` |
|       13 | 1374 | `				rc = PH7_GenSyntaxError(&(*pGen),0,0);` |
|       13 | 1375 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1376 | `					return SXERR_ABORT;` |
|        - | 1377 | `				}` |
|       13 | 1378 | `				return SXRET_OK;` |
|        - | 1379 | `			}` |
|        - | 1380 | `			/* Compile the expression holding the key */` |
|   400159 | 1381 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|        - | 1382 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|   400159 | 1383 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1384 | `				return SXERR_ABORT;` |
|        - | 1385 | `			}` |
|   400159 | 1386 | `			pCur++; /* Jump the '=>' operator */` |
|   200082 | 1387 | `		}else{` |
|        - | 1388 | `			/* Reset back the cursor and point to the entry value */` |
|   758961 | 1389 | `			pCur = pKey;` |
|        - | 1390 | `		}` |
|  1159115 | 1391 | `		if( rc == SXERR_EMPTY ){` |
|        - | 1392 | `			/* No key given: load the nil, TAGGED so LOAD_MAP knows this is an absent key` |
|        - | 1393 | ``			 * (auto-index) rather than an explicit `null =>` one, which php deprecates. */`` |
|   758961 | 1394 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,PH7_LOADC_NOKEY,0 /* nil index */,0,0);` |
|   379478 | 1395 | `		}` |
|  1159115 | 1396 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
|        - | 1397 | `			/* Insertion by reference, [i.e: $a = array(&$x);] */` |
|       45 | 1398 | `			xValidator = GenStateArrayNodeValidator; /* Only variable are allowed */` |
|       45 | 1399 | `			iEmitRef = 1;` |
|       45 | 1400 | `			pCur++; /* Jump the '&' token */` |
|       45 | 1401 | `			if( pCur >= pGen->pIn ){` |
|        - | 1402 | `				/* Missing value */` |
|        - | 1403 | ``				/* php reports the token that actually stopped it (`array(&)` -> the`` |
|        - | 1404 | `				 * ')'), not a hand-written "missing referenced variable" fatal. */` |
|        3 | 1405 | `				rc = PH7_GenSyntaxError(&(*pGen),pCur < pGen->pIn ? pCur : 0,0);` |
|        3 | 1406 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1407 | `					return SXERR_ABORT;` |
|        - | 1408 | `				}` |
|        3 | 1409 | `				return SXRET_OK;` |
|        - | 1410 | `			}` |
|       19 | 1411 | `		}` |
|        - | 1412 | `		/* Detect array unpack: '...$expr' as the entry value (PHP 7.4+, with` |
|        - | 1413 | `		 * string-key support since PHP 8.1). The parser strips the '...' inside` |
|        - | 1414 | `		 * ExprExtractNode; we only need to know it's there so we can emit` |
|        - | 1415 | `		 * PH7_OP_FLAG_SPREAD after the value, instructing LOAD_MAP to merge the` |
|        - | 1416 | `		 * resulting hashmap rather than insert it as a scalar entry. */` |
|  1159113 | 1417 | `		iSpread = (pCur < pGen->pIn && (pCur->nType & PH7_TK_ELLIPSIS)) ? 1 : 0;` |
|  1159113 | 1418 | `		if( iSpread && (rc != SXERR_EMPTY \|\| iEmitRef) ){` |
|        - | 1419 | `			/* '[k => ...$a]' and '[&...$a]' are syntax errors in PHP — the` |
|        - | 1420 | `			 * '...' token cannot follow either '=>' or '&' inside an array` |
|        - | 1421 | `			 * literal. Emit the same Parse-error wording PHP uses so the` |
|        - | 1422 | `			 * output is engine-portable. */` |
|        6 | 1423 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pCur->nLine,` |
|        - | 1424 | `				"syntax error, unexpected token \"...\"");` |
|        6 | 1425 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1426 | `				return SXERR_ABORT;` |
|        - | 1427 | `			}` |
|        6 | 1428 | `			return SXRET_OK;` |
|        - | 1429 | `		}` |
|        - | 1430 | ``		/* Compile indice value. A BY-REF element (`'k' => &$a[$i]`) is an`` |
|        - | 1431 | `		 * lvalue: php VIVIFIES a missing subscript when a reference is taken,` |
|        - | 1432 | `		 * so compile it in write context (LOAD_IDX iP2=1, create-if-missing)` |
|        - | 1433 | `		 * instead of a read-only load — which also keeps the undefined-key` |
|        - | 1434 | `		 * warning (a read-only diagnostic) from false-firing here. */` |
|  1738661 | 1435 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,` |
|   579552 | 1436 | `			iEmitRef ? EXPR_FLAG_LOAD_IDX_STORE` |
|        - | 1437 | `			         : EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,` |
|   579552 | 1438 | `			xValidator);` |
|  1159109 | 1439 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1440 | `			return SXERR_ABORT;` |
|        - | 1441 | `		}` |
|  1159109 | 1442 | `		if( iSpread ){` |
|        - | 1443 | `			/* Mark the value on TOS as a spread source; LOAD_MAP merges it. */` |
|       73 | 1444 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FLAG_SPREAD,0,0,0,0);` |
|  1159074 | 1445 | `		}else if( iEmitRef ){` |
|        - | 1446 | `			/* Emit the load reference instruction */` |
|       41 | 1447 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|       18 | 1448 | `		}` |
|  1159109 | 1449 | `		xValidator = 0;` |
|  1159109 | 1450 | `		iEmitRef = 0;` |
|  1159109 | 1451 | `		iSpread = 0;` |
|  1159109 | 1452 | `		nPair++;` |
|        5 | 1453 | `	}` |
|        - | 1454 | `	/* Emit the load map instruction */` |
|   669017 | 1455 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|        - | 1456 | `	/* Node successfully compiled */` |
|   669017 | 1457 | `	return SXRET_OK;` |
|   334520 | 1458 | `}` |
|        - | 1459 | `/*` |
|        - | 1460 | ` * Compile the 'array' language construct.` |
|        - | 1461 | ` *	 According to the PHP language reference manual` |
|        - | 1462 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|        - | 1463 | ` *   values to keys. This type is optimized for several different uses; it can` |
|        - | 1464 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|        - | 1465 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|        - | 1466 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|        - | 1467 | ` */` |
|   426878 | 1468 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 | 1469 | `{` |
|        - | 1470 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   426883 | 1471 | `	pGen->pIn += 2;` |
|   426883 | 1472 | `	pGen->pEnd--;` |
|   213439 | 1473 | `	SXUNUSED(iCompileFlag);` |
|        - | 1474 | ``	/* php: a stray token in an `array( ... )` element is `... expecting ")"`. */`` |
|        - | 1475 | `	{` |
|   426883 | 1476 | `		const char *zSave = pGen->zClauseCloser;` |
|        - | 1477 | `		sxi32 rc;` |
|   426883 | 1478 | `		pGen->zClauseCloser = "\")\"";` |
|   426883 | 1479 | `		rc = GenStateCompileArrayBody(pGen);` |
|   426883 | 1480 | `		pGen->zClauseCloser = zSave;` |
|   426883 | 1481 | `		return rc;` |
|        - | 1482 | `	}` |
|        5 | 1483 | `}` |
|        - | 1484 | `/*` |
|        - | 1485 | ` * Compile the PHP 8.5 clone(...) call form:` |
|        - | 1486 | `` *   clone($object)                          -> identical to the `clone $object` operator`` |
|        - | 1487 | ` *   clone($object, ['prop' => value, ...])  -> clone, run __clone(), then apply the` |
|        - | 1488 | ` *                                              property updates as scope-aware writes` |
|        - | 1489 | ` *   clone(object: $o, withProperties: [..]) -> the named-argument spelling` |
|        - | 1490 | ` * Codegen: compile the object argument and emit OP_CLONE (which clones and runs` |
|        - | 1491 | ` * __clone()); if a withProperties argument is present, compile it and emit` |
|        - | 1492 | ` * OP_CLONE_APPLY, which applies each update to the fresh clone AFTER __clone(),` |
|        - | 1493 | ` * honouring visibility / readonly-set-scope / typed-property enforcement in the` |
|        - | 1494 | ` * calling scope. The parser (ExprExtractNode) delimited this node's tokens as` |
|        - | 1495 | `` * `clone ( ... )`; pGen->pIn/pEnd point at the first/one-past-last of that range.`` |
|        - | 1496 | ` */` |
|       22 | 1497 | `PH7_PRIVATE sxi32 PH7_CompileCloneCall(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        2 | 1498 | `{` |
|        - | 1499 | `	SyToken *pIn,*pEnd,*pNext;` |
|       24 | 1500 | `	SyToken *pObjStart = 0,*pObjEnd = 0;` |
|       24 | 1501 | `	SyToken *pUpdStart = 0,*pUpdEnd = 0;` |
|       24 | 1502 | `	int nArg = 0;` |
|        - | 1503 | `	sxi32 rc;` |
|       11 | 1504 | `	SXUNUSED(iCompileFlag);` |
|        - | 1505 | `	/* pGen->pIn -> 'clone', pGen->pIn[1] -> '(', pGen->pEnd -> one past ')'. */` |
|       24 | 1506 | `	pIn  = pGen->pIn + 2;   /* skip 'clone' and the opening '(' */` |
|       24 | 1507 | `	pEnd = pGen->pEnd - 1;  /* exclude the closing ')' */` |
|        - | 1508 | `	/* clone(...) first-class-callable form: a lone ellipsis is the whole list. */` |
|       24 | 1509 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|      ! 0 | 1510 | `		return PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|        - | 1511 | `			"clone(...) first-class callable form is not yet supported");` |
|        - | 1512 | `	}` |
|        - | 1513 | `	/* Split the (at most two) comma-separated arguments, tolerating named labels. */` |
|       62 | 1514 | `	while( pIn < pEnd ){` |
|       40 | 1515 | `		SyToken *pArgStart,*pArgEnd,*pName = 0;` |
|       40 | 1516 | `		if( PH7_GetNextExpr(pIn,pEnd,&pNext) != SXRET_OK ){` |
|      ! 0 | 1517 | `			break;` |
|        - | 1518 | `		}` |
|       40 | 1519 | `		pArgStart = pIn;` |
|       40 | 1520 | `		pArgEnd   = pNext;` |
|        - | 1521 | `		/* Named-argument label: <ID\|keyword> ':' expr. A single ':' is PH7_TK_COLON;` |
|        - | 1522 | ``		 * '::' is a distinct operator token, so this never mis-fires on `A::B`. */`` |
|       38 | 1523 | `		if( (pArgEnd - pArgStart) >= 2` |
|       37 | 1524 | `			&& (pArgStart[0].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|       23 | 1525 | `			&& (pArgStart[1].nType & PH7_TK_COLON) ){` |
|        5 | 1526 | `			pName = pArgStart;` |
|        5 | 1527 | `			pArgStart += 2;` |
|        2 | 1528 | `		}` |
|       40 | 1529 | `		if( pName ){` |
|        - | 1530 | `` 			/* PHP named parameters are case-SENSITIVE, so `Object:`/`WITHPROPERTIES:` `` |
|        - | 1531 | `			 * must be rejected as unknown (SyMemcmp, not SyStrnicmp). */` |
|        4 | 1532 | `			if( pName->sData.nByte == sizeof("object")-1` |
|        4 | 1533 | `				&& SyMemcmp(pName->sData.zString,"object",sizeof("object")-1) == 0 ){` |
|        3 | 1534 | `				pObjStart = pArgStart; pObjEnd = pArgEnd;` |
|        4 | 1535 | `			}else if( pName->sData.nByte == sizeof("withProperties")-1` |
|        3 | 1536 | `				&& SyMemcmp(pName->sData.zString,"withProperties",sizeof("withProperties")-1) == 0 ){` |
|        3 | 1537 | `				pUpdStart = pArgStart; pUpdEnd = pArgEnd;` |
|        2 | 1538 | `			}else{` |
|      ! 0 | 1539 | `				return PH7_GenCompileError(pGen,E_ERROR,pName->nLine,` |
|      ! 0 | 1540 | `					"Unknown named parameter $%z",&pName->sData);` |
|        1 | 1541 | `			}` |
|       38 | 1542 | `		}else if( nArg == 0 ){` |
|       22 | 1543 | `			pObjStart = pArgStart; pObjEnd = pArgEnd;` |
|       25 | 1544 | `		}else if( nArg == 1 ){` |
|       15 | 1545 | `			pUpdStart = pArgStart; pUpdEnd = pArgEnd;` |
|        8 | 1546 | `		}else{` |
|      ! 0 | 1547 | `			return PH7_GenCompileError(pGen,E_ERROR,pArgStart->nLine,` |
|        - | 1548 | `				"clone() expects at most 2 arguments");` |
|        - | 1549 | `		}` |
|       40 | 1550 | `		nArg++;` |
|       40 | 1551 | `		pIn = pNext;` |
|       40 | 1552 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|       17 | 1553 | `			pIn++; /* step over the argument separator */` |
|        8 | 1554 | `		}` |
|        2 | 1555 | `	}` |
|       24 | 1556 | `	if( pObjStart == 0 \|\| pObjStart >= pObjEnd ){` |
|      ! 0 | 1557 | `		return PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 1558 | `			"clone() expects at least 1 argument, 0 given");` |
|        - | 1559 | `	}` |
|        - | 1560 | `	/* Object argument -> clone (+ __clone()). */` |
|       24 | 1561 | `	rc = GenStateCompileArrayEntry(pGen,pObjStart,pObjEnd,EXPR_FLAG_RDONLY_LOAD,0);` |
|       24 | 1562 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 1563 | `		return SXERR_ABORT;` |
|        - | 1564 | `	}` |
|       24 | 1565 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_CLONE,0,0,0,0);` |
|        - | 1566 | `	/* Property updates (evaluated after __clone runs). */` |
|       24 | 1567 | `	if( pUpdStart && pUpdStart < pUpdEnd ){` |
|       17 | 1568 | `		rc = GenStateCompileArrayEntry(pGen,pUpdStart,pUpdEnd,EXPR_FLAG_RDONLY_LOAD,0);` |
|       17 | 1569 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1570 | `			return SXERR_ABORT;` |
|        - | 1571 | `		}` |
|       17 | 1572 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CLONE_APPLY,0,0,0,0);` |
|        8 | 1573 | `	}` |
|       24 | 1574 | `	return SXRET_OK;` |
|       13 | 1575 | `}` |
|        - | 1576 | `/*` |
|        - | 1577 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|        - | 1578 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|        - | 1579 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|        - | 1580 | ` */` |
|   242152 | 1581 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 | 1582 | `{` |
|        - | 1583 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|   242157 | 1584 | `	pGen->pIn++;` |
|   242157 | 1585 | `	pGen->pEnd--;` |
|   121076 | 1586 | `	SXUNUSED(iCompileFlag);` |
|        - | 1587 | ``	/* php: a stray token in a `[ ... ]` element is `... expecting "]"`. */`` |
|        - | 1588 | `	{` |
|   242157 | 1589 | `		const char *zSave = pGen->zClauseCloser;` |
|        - | 1590 | `		sxi32 rc;` |
|   242157 | 1591 | `		pGen->zClauseCloser = "\"]\"";` |
|   242157 | 1592 | `		rc = GenStateCompileArrayBody(pGen);` |
|   242157 | 1593 | `		pGen->zClauseCloser = zSave;` |
|   242157 | 1594 | `		return rc;` |
|        - | 1595 | `	}` |
|        5 | 1596 | `}` |
|        - | 1597 | `/*` |
|        - | 1598 | ` * Expression tree validator callback for the 'list' language construct.` |
|        - | 1599 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|        - | 1600 | ` * an invalid expression tree and this function will generate the appropriate` |
|        - | 1601 | ` * error message.` |
|        - | 1602 | ` * See the routine responible of compiling the list language construct` |
|        - | 1603 | ` * for more inforation.` |
|        - | 1604 | ` */` |
|      226 | 1605 | `static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        5 | 1606 | `{` |
|      231 | 1607 | `	sxi32 rc = SXRET_OK;` |
|      231 | 1608 | `	if( pRoot->pOp ){` |
|        4 | 1609 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */` |
|        2 | 1610 | `			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){` |
|        - | 1611 | `				/* Unexpected expression */` |
|      ! 0 | 1612 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - | 1613 | `					"Assignments can only happen to writable values");` |
|      ! 0 | 1614 | `				if( rc != SXERR_ABORT ){` |
|      ! 0 | 1615 | `					rc = SXERR_INVALID;` |
|      ! 0 | 1616 | `				}` |
|        1 | 1617 | `		}` |
|      229 | 1618 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|        - | 1619 | `		/* Unexpected expression */` |
|        6 | 1620 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - | 1621 | `			"Assignments can only happen to writable values");` |
|        6 | 1622 | `		if( rc != SXERR_ABORT ){` |
|        6 | 1623 | `			rc = SXERR_INVALID;` |
|        2 | 1624 | `		}` |
|        2 | 1625 | `	}` |
|      231 | 1626 | `	return rc;` |
|        5 | 1627 | `}` |
|        - | 1628 | `/*` |
|        - | 1629 | ` * Compile the 'list' language construct.` |
|        - | 1630 | ` *  According to the PHP language reference` |
|        - | 1631 | ` *  list(): Assign variables as if they were an array.` |
|        - | 1632 | ` *  list() is used to assign a list of variables in one operation.` |
|        - | 1633 | ` *  Description` |
|        - | 1634 | ` *   array list (mixed $varname [, mixed $... ] )` |
|        - | 1635 | ` *   Like array(), this is not really a function, but a language construct.` |
|        - | 1636 | ` *   list() is used to assign a list of variables in one operation.` |
|        - | 1637 | ` *  Parameters` |
|        - | 1638 | ` *   $varname: A variable.` |
|        - | 1639 | ` *  Return Values` |
|        - | 1640 | ` *   The assigned array.` |
|        - | 1641 | ` */` |
|        - | 1642 | `/* Nested list entry recorded during first pass of list body compilation */` |
|        - | 1643 | `struct NestedListEntry {` |
|        - | 1644 | `	sxi32 nIndex;        /* Position in the outer list (0-based) */` |
|        - | 1645 | `	SyToken *pStart;     /* Token range: start of nested construct */` |
|        - | 1646 | `	SyToken *pEnd;       /* Token range: past closing delimiter */` |
|        - | 1647 | `	sxi32 isShort;       /* 1 if [...] form, 0 if list(...) form */` |
|        - | 1648 | `};` |
|        - | 1649 | `/*` |
|        - | 1650 | ` * Compile the body of a *keyed* list/short-list destructuring (PHP 7.1), where` |
|        - | 1651 | `` * every entry has the form `keyExpr => target`. The source array is on the stack`` |
|        - | 1652 | ` * top on entry and remains there on exit, mirroring the positional LOAD_LIST` |
|        - | 1653 | ` * path so the caller's teardown is unchanged. For each entry: DUP the source,` |
|        - | 1654 | ` * push the key, LOAD_IDX to fetch source[key] (NULL on a missing key, silently,` |
|        - | 1655 | ` * like a normal subscript read), then assign the fetched value to the target — a` |
|        - | 1656 | ` * nested [...]/list() recurses, a simple lvalue uses the same STORE fold as a` |
|        - | 1657 | ` * normal assignment (the value sits below the lvalue-load, exactly as in` |
|        - | 1658 | ` * GenStateEmitExprCode where the assignment RHS precedes the LHS load).` |
|        - | 1659 | ` */` |
|       28 | 1660 | `static sxi32 GenStateCompileKeyedListBody(ph7_gen_state *pGen)` |
|        2 | 1661 | `{` |
|        - | 1662 | `	SyToken *pNext;` |
|        - | 1663 | `	sxi32 rc;` |
|       66 | 1664 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|        - | 1665 | `		SyToken *pArrow,*pTarget;` |
|        - | 1666 | ``		/* Split `keyExpr => target` at the top-level '=>' */`` |
|       38 | 1667 | `		pArrow = GenStateFindTopLevelArrow(pGen->pIn,pNext);` |
|       38 | 1668 | `		pTarget = &pArrow[1];` |
|       38 | 1669 | `		if( pArrow <= pGen->pIn \|\| pTarget >= pNext ){` |
|        - | 1670 | ``			/* Empty key (`[ => $v]`) or empty value (`["k" =>]`): PHP rejects`` |
|        - | 1671 | `			 * both. Reject rather than silently emitting unbalanced bytecode. */` |
|      ! 0 | 1672 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - | 1673 | `				"Cannot use empty array entries in keyed array assignment");` |
|      ! 0 | 1674 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 1675 | `		}` |
|        - | 1676 | `		/* DUP the source array (it is on the stack top) */` |
|       38 | 1677 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|        - | 1678 | `		/* Compile the key expression; it is pushed above the DUP'd source */` |
|       38 | 1679 | `		rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pArrow,EXPR_FLAG_RDONLY_LOAD,0);` |
|       38 | 1680 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1681 | `			return SXERR_ABORT;` |
|        - | 1682 | `		}` |
|        - | 1683 | `		/* LOAD_IDX: pop the key, replace the DUP'd source with source[key].` |
|        - | 1684 | `		 * iP2=7 is the keyed-destructuring read context: an array source reads like` |
|        - | 1685 | ``		 * iP2=0 (missing key loads NULL silently, matching a normal `$arr[$k]` read;`` |
|        - | 1686 | `		 * PHP also emits an "Undefined array key" warning here, PHL omits it — §3.7),` |
|        - | 1687 | `		 * but a NON-array source yields NULL + a per-key "Cannot use <type> as array"` |
|        - | 1688 | `		 * warning instead of char-indexing a string (matching PHP's OP_LOAD_LIST path). */` |
|       38 | 1689 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,7,0,0);` |
|       38 | 1690 | `		if( pTarget < pNext && ( (pTarget->nType & PH7_TK_OSB)` |
|       34 | 1691 | `			\|\| ( (pTarget->nType & PH7_TK_KEYWORD)` |
|       18 | 1692 | `				&& SX_PTR_TO_INT(pTarget->pUserData) == PH7_TKWRD_LIST ) ) ){` |
|        - | 1693 | `			/* Nested destructuring:  ["k" => [ ... ]]  or  ["k" => list( ... )].` |
|        - | 1694 | `			 * Treat source[key] as the inner body's source, then drop the` |
|        - | 1695 | `			 * leftover it leaves behind (mirrors the positional nested path). */` |
|        5 | 1696 | `			sxi32 isShort = (pTarget->nType & PH7_TK_OSB) != 0;` |
|        5 | 1697 | `			SyToken *pSavedIn = pGen->pIn;` |
|        5 | 1698 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|        5 | 1699 | `			pGen->pIn = pTarget;` |
|        5 | 1700 | `			pGen->pEnd = pNext;` |
|        5 | 1701 | `			rc = isShort ? PH7_CompileShortList(&(*pGen),0)` |
|        2 | 1702 | `			             : PH7_CompileList(&(*pGen),0);` |
|        5 | 1703 | `			pGen->pIn = pSavedIn;` |
|        5 | 1704 | `			pGen->pEnd = pSavedEnd;` |
|        5 | 1705 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1706 | `				return SXERR_ABORT;` |
|        - | 1707 | `			}` |
|        5 | 1708 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        3 | 1709 | `		}else{` |
|        - | 1710 | `			/* Simple lvalue target ($v / $o->p / $a[i] / Cls::$s). source[key]` |
|        - | 1711 | `			 * is already on the stack as the value; compiling the target appends` |
|        - | 1712 | `			 * its lvalue-load, which we fold into a STORE just as a normal` |
|        - | 1713 | `			 * assignment does. */` |
|        - | 1714 | `			VmInstr *pInstr;` |
|       34 | 1715 | `			sxi32 iVmOp = PH7_OP_STORE;` |
|       34 | 1716 | `			sxi32 iP1 = 0, iP2 = 0;` |
|       34 | 1717 | `			void *p3 = 0;` |
|       34 | 1718 | `			rc = GenStateCompileArrayEntry(&(*pGen),pTarget,pNext,` |
|        - | 1719 | `				EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|       34 | 1720 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1721 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 1722 | `			}` |
|       34 | 1723 | `			if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|       34 | 1724 | `				if( pInstr->iOp == PH7_OP_MEMBER ){` |
|        3 | 1725 | `					iP2 = 1; /* member store: keep MEMBER, store value below it */` |
|       33 | 1726 | `				}else if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|        3 | 1727 | `					iVmOp = PH7_OP_STORE_IDX;` |
|        3 | 1728 | `					iP1 = pInstr->iP1;` |
|        3 | 1729 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|        2 | 1730 | `				}else{` |
|       30 | 1731 | `					p3 = pInstr->p3; /* named store: $v = value */` |
|       30 | 1732 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|        - | 1733 | `				}` |
|       16 | 1734 | `			}` |
|       34 | 1735 | `			PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|        - | 1736 | `			/* STORE leaves the assigned value on the stack top; drop it so the` |
|        - | 1737 | `			 * source array is back on top for the next entry. */` |
|       34 | 1738 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        - | 1739 | `		}` |
|       38 | 1740 | `		pGen->pIn = &pNext[1];` |
|        2 | 1741 | `	}` |
|       30 | 1742 | `	return SXRET_OK;` |
|       16 | 1743 | `}` |
|        - | 1744 | `/*` |
|        - | 1745 | ` * Shared body for list() and short list [...] compilation.` |
|        - | 1746 | ` * Assumes pGen->pIn and pGen->pEnd are already positioned past` |
|        - | 1747 | ` * the opening delimiter and before the closing delimiter.` |
|        - | 1748 | ` */` |
|      134 | 1749 | `static sxi32 GenStateCompileListBody(ph7_gen_state *pGen)` |
|        5 | 1750 | `{` |
|        - | 1751 | `	SySet sNested; /* Dynamically-sized container of NestedListEntry */` |
|        - | 1752 | `	SyToken *pNext;` |
|        - | 1753 | `	SyToken *pClassifyIn;` |
|      139 | 1754 | `	sxi32 nKeyed = 0, nPositional = 0, nEmpty = 0;` |
|        - | 1755 | `	sxi32 nExpr;` |
|        - | 1756 | `	sxi32 rc;` |
|        - | 1757 | ``	/* First pass: classify entries as keyed (`k => v`), positional, or empty`` |
|        - | 1758 | `	 * skip slots ([,]). A list level must be entirely keyed or entirely` |
|        - | 1759 | `	 * positional — PHP fatals on a mix, and on an empty slot inside a keyed` |
|        - | 1760 | `	 * list. */` |
|      139 | 1761 | `	pClassifyIn = pGen->pIn;` |
|      395 | 1762 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|      261 | 1763 | `		if( pGen->pIn >= pNext ){` |
|       13 | 1764 | `			nEmpty++;` |
|      255 | 1765 | `		}else if( GenStateFindTopLevelArrow(pGen->pIn,pNext) < pNext ){` |
|       38 | 1766 | `			nKeyed++;` |
|       20 | 1767 | `		}else{` |
|      213 | 1768 | `			nPositional++;` |
|        - | 1769 | `		}` |
|      261 | 1770 | `		pGen->pIn = &pNext[1];` |
|        5 | 1771 | `	}` |
|      139 | 1772 | `	pGen->pIn = pClassifyIn;` |
|      139 | 1773 | `	if( nKeyed > 0 && nEmpty > 0 ){` |
|      ! 0 | 1774 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - | 1775 | `			"Cannot use empty array entries in keyed array assignment");` |
|      ! 0 | 1776 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 1777 | `	}` |
|      139 | 1778 | `	if( nKeyed > 0 && nPositional > 0 ){` |
|      ! 0 | 1779 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - | 1780 | `			"Cannot mix keyed and unkeyed array entries in assignments");` |
|      ! 0 | 1781 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 1782 | `	}` |
|      139 | 1783 | `	if( nKeyed > 0 ){` |
|       30 | 1784 | `		return GenStateCompileKeyedListBody(pGen);` |
|        - | 1785 | `	}` |
|      111 | 1786 | `	nExpr = 0;` |
|      111 | 1787 | `	SySetInit(&sNested,&pGen->pVm->sAllocator,sizeof(struct NestedListEntry));` |
|      331 | 1788 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|      225 | 1789 | `		if( pGen->pIn < pNext ){` |
|        - | 1790 | `			/* Check for nested list() */` |
|      213 | 1791 | `			if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        3 | 1792 | `				SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|        - | 1793 | `				/* Record this nested list for post-processing */` |
|        3 | 1794 | `				SyToken *pListEnd = 0;` |
|        3 | 1795 | `				if( &pGen->pIn[1] < pNext && (pGen->pIn[1].nType & PH7_TK_LPAREN) ){` |
|        3 | 1796 | `					PH7_DelimitNestedTokens(pGen->pIn+2,pNext,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|        1 | 1797 | `				}` |
|        3 | 1798 | `				if( pListEnd ){` |
|        - | 1799 | `					struct NestedListEntry sEntry;` |
|        3 | 1800 | `					sEntry.nIndex = nExpr;` |
|        3 | 1801 | `					sEntry.pStart = pGen->pIn;` |
|        3 | 1802 | `					sEntry.pEnd = pListEnd + 1;` |
|        3 | 1803 | `					sEntry.isShort = 0;` |
|        3 | 1804 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|        1 | 1805 | `				}` |
|        - | 1806 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|        3 | 1807 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|      212 | 1808 | `			}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|        - | 1809 | `				/* Nested short destructuring [...] */` |
|       13 | 1810 | `				SyToken *pBracketEnd = 0;` |
|       13 | 1811 | `				PH7_DelimitNestedTokens(pGen->pIn+1,pNext,PH7_TK_OSB,PH7_TK_CSB,&pBracketEnd);` |
|       13 | 1812 | `				if( pBracketEnd ){` |
|        - | 1813 | `					struct NestedListEntry sEntry;` |
|       13 | 1814 | `					sEntry.nIndex = nExpr;` |
|       13 | 1815 | `					sEntry.pStart = pGen->pIn;` |
|       13 | 1816 | `					sEntry.pEnd = pBracketEnd + 1;` |
|       13 | 1817 | `					sEntry.isShort = 1;` |
|       13 | 1818 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|        6 | 1819 | `				}` |
|        - | 1820 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|       13 | 1821 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|        7 | 1822 | `			}else{` |
|        - | 1823 | `				/* Compile the expression holding the variable */` |
|      199 | 1824 | `				rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|      199 | 1825 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 1826 | `					SySetRelease(&sNested);` |
|      ! 0 | 1827 | `					return SXRET_OK;` |
|        - | 1828 | `				}` |
|        - | 1829 | `			}` |
|      109 | 1830 | `		}else{` |
|        - | 1831 | `			/* Empty entry,load NULL */` |
|       13 | 1832 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);` |
|        - | 1833 | `		}` |
|      225 | 1834 | `		nExpr++;` |
|        - | 1835 | `		/* Advance the stream cursor */` |
|      225 | 1836 | `		pGen->pIn = &pNext[1];` |
|        5 | 1837 | `	}` |
|        - | 1838 | `	/* Emit the LOAD_LIST instruction */` |
|      111 | 1839 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,0,0,0);` |
|        - | 1840 | `	/* After LOAD_LIST, the source array is still on the stack top.` |
|        - | 1841 | `	 * For each nested entry, emit code to extract the sub-array` |
|        - | 1842 | `	 * at the corresponding index and recursively destructure it.` |
|        - | 1843 | `	 */` |
|      111 | 1844 | `	if( SySetUsed(&sNested) > 0 ){` |
|       13 | 1845 | `		struct NestedListEntry *apNested = (struct NestedListEntry *)SySetBasePtr(&sNested);` |
|        - | 1846 | `		sxu32 i;` |
|       27 | 1847 | `		for(i = 0; i < SySetUsed(&sNested); i++){` |
|       15 | 1848 | `			SyToken *pSavedIn = pGen->pIn;` |
|       15 | 1849 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|        - | 1850 | `			ph7_value *pIdx;` |
|        - | 1851 | `			sxu32 nConstIdx;` |
|        - | 1852 | `			/* DUP the source array (it's on stack top) */` |
|       15 | 1853 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|        - | 1854 | `			/* Push the integer index for this nested entry */` |
|       15 | 1855 | `			pIdx = PH7_ReserveConstObj(pGen->pVm,&nConstIdx);` |
|       15 | 1856 | `			if( pIdx == 0 ){` |
|      ! 0 | 1857 | `				PH7_GenCompileError(&(*pGen),E_ERROR,0,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1858 | `				SySetRelease(&sNested);` |
|      ! 0 | 1859 | `				return SXERR_ABORT;` |
|        - | 1860 | `			}` |
|       15 | 1861 | `			PH7_MemObjInitFromInt(pGen->pVm,pIdx,(sxi64)apNested[i].nIndex);` |
|       15 | 1862 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nConstIdx,0,0);` |
|        - | 1863 | `			/* LOAD_IDX: pop index, replace DUP'd source with source[index].` |
|        - | 1864 | `			 * iP2=2 signals the VM to emit an "Undefined array key" warning` |
|        - | 1865 | `			 * when the key is missing (PHP-compatible list destructuring).` |
|        - | 1866 | `			 */` |
|       15 | 1867 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,2,0,0);` |
|        - | 1868 | `			/* Recursively compile the inner list */` |
|       15 | 1869 | `			pGen->pIn = apNested[i].pStart;` |
|       15 | 1870 | `			pGen->pEnd = apNested[i].pEnd;` |
|       15 | 1871 | `			if( apNested[i].isShort ){` |
|       13 | 1872 | `				rc = PH7_CompileShortList(&(*pGen),0);` |
|        7 | 1873 | `			}else{` |
|        3 | 1874 | `				rc = PH7_CompileList(&(*pGen),0);` |
|        - | 1875 | `			}` |
|       15 | 1876 | `			pGen->pIn = pSavedIn;` |
|       15 | 1877 | `			pGen->pEnd = pSavedEnd;` |
|       15 | 1878 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1879 | `				SySetRelease(&sNested);` |
|      ! 0 | 1880 | `				return SXERR_ABORT;` |
|        - | 1881 | `			}` |
|        - | 1882 | `			/* Pop the leftover source[index] from the inner LOAD_LIST */` |
|       15 | 1883 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        8 | 1884 | `		}` |
|        6 | 1885 | `	}` |
|      111 | 1886 | `	SySetRelease(&sNested);` |
|        - | 1887 | `	/* Node successfully compiled */` |
|      111 | 1888 | `	return SXRET_OK;` |
|       72 | 1889 | `}` |
|       40 | 1890 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 | 1891 | `{` |
|        - | 1892 | `	/* Jump the 'list' keyword, the leading '(' and exclude trailing ')' */` |
|       45 | 1893 | `	pGen->pIn += 2;` |
|       45 | 1894 | `	pGen->pEnd--;` |
|       20 | 1895 | `	SXUNUSED(iCompileFlag);` |
|       45 | 1896 | `	return GenStateCompileListBody(pGen);` |
|        5 | 1897 | `}` |
|       94 | 1898 | `PH7_PRIVATE sxi32 PH7_CompileShortList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 | 1899 | `{` |
|        - | 1900 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|       99 | 1901 | `	pGen->pIn++;` |
|       99 | 1902 | `	pGen->pEnd--;` |
|       47 | 1903 | `	SXUNUSED(iCompileFlag);` |
|       99 | 1904 | `	return GenStateCompileListBody(pGen);` |
|        5 | 1905 | `}` |
|        - | 1906 |  |
