# src/ph7/compile_literal.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 949/1066 lines (89.02%)

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
|  3827492 |   45 | `static int GenStateFindBadNumericSeparator(` |
|        - |   46 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|        5 |   47 | `{` |
|  3827497 |   48 | `	const char *z = pRaw->zString;` |
|  3827497 |   49 | `	sxu32 n = pRaw->nByte;` |
|  3827497 |   50 | `	int base = 10;` |
|        - |   51 | `	sxu32 i, start;` |
|  3827497 |   52 | `	if( n < 2 ) return 0;` |
|   806259 |   53 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|   104915 |   54 | `		base = 16;` |
|   753804 |   55 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|      287 |   56 | `		base = 2;` |
|      143 |   57 | `	}` |
|  3070279 |   58 | `	for( i = 0; i < n; ++i ){` |
|  2264035 |   59 | `		if( z[i] != '_' ) continue;` |
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
|   806249 |   76 | `	return 0;` |
|  1913751 |   77 | `}` |
|        - |   78 | `/*` |
|        - |   79 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|        - |   80 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|        - |   81 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|        - |   82 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|        - |   83 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|        - |   84 | ` * so callers can bail from the current construct).` |
|        - |   85 | ` */` |
|  3827492 |   86 | `PH7_PRIVATE sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|        5 |   87 | `{` |
|  3827497 |   88 | `	const char *zBad = 0;` |
|  3827497 |   89 | `	sxu32 nBad = 0;` |
|        - |   90 | `	SyString sBad;` |
|        - |   91 | `	sxi32 rc;` |
|  3827497 |   92 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|  3827487 |   93 | `		return SXRET_OK;` |
|        - |   94 | `	}` |
|       14 |   95 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|       14 |   96 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|        - |   97 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|       14 |   98 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |   99 | `		return SXERR_ABORT;` |
|        - |  100 | `	}` |
|       14 |  101 | `	return SXERR_SYNTAX;` |
|  1913751 |  102 | `}` |
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
|  3827482 |  119 | `PH7_PRIVATE sxi32 GenStateStripNumericSeparators(` |
|        - |  120 | `	SyMemBackend *pAlloc,` |
|        - |  121 | `	const SyString *pToken,` |
|        - |  122 | `	char *zScratch, sxu32 nScratch,` |
|        - |  123 | `	SyString *pOut, char **pzAlloc)` |
|        5 |  124 | `{` |
|        - |  125 | `	sxu32 i, j;` |
|  3827487 |  126 | `	int hasUnderscore = 0;` |
|        - |  127 | `	char *zBuf;` |
|  3827487 |  128 | `	*pzAlloc = 0;` |
|  9110661 |  129 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|  5283439 |  130 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|  2641592 |  131 | `	}` |
|  3827487 |  132 | `	if( !hasUnderscore ){` |
|  3827227 |  133 | `		SyStringDupPtr(pOut, pToken);` |
|  3827227 |  134 | `		return SXRET_OK;` |
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
|  1913746 |  151 | `}` |
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
|  3818730 |  187 | `static int GenStateIntLiteralOverflows(const SyString *pNum, ph7_real *pReal, int *pbDecimal)` |
|        5 |  188 | `{` |
|  3818735 |  189 | `	const char *z = pNum->zString;` |
|  3818735 |  190 | `	const char *zEnd = z + pNum->nByte;` |
|        - |  191 | `	const char *p, *q;` |
|        - |  192 | `	int n;` |
|  3818735 |  193 | `	*pbDecimal = FALSE;` |
|  3818735 |  194 | `	if( z >= zEnd ){` |
|      ! 0 |  195 | `		return FALSE;` |
|        - |  196 | `	}` |
|  3818735 |  197 | `	if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|        - |  198 | `		/* Hexadecimal: INT64_MAX == 0x7FFF...F (16 digits, leading nibble 7). */` |
|   104915 |  199 | `		p = z + 2;` |
|   132097 |  200 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|   427605 |  201 | `		for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisHex(q[0]); q++ ){ n++; }` |
|   104915 |  202 | `		if( n < 16 \|\| (n == 16 && SyHexToint(p[0]) < 8) ){` |
|   104909 |  203 | `			return FALSE;` |
|        - |  204 | `		}` |
|        7 |  205 | `		{ ph7_real dv = 0;` |
|      103 |  206 | `		  for( q = p; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisHex(q[0]); q++ ){` |
|       97 |  207 | `			dv = dv * 16 + (ph7_real)SyHexToint(q[0]);` |
|       49 |  208 | `		  }` |
|        7 |  209 | `		  *pReal = dv;` |
|        - |  210 | `		}` |
|        7 |  211 | `		return TRUE;` |
|  3713825 |  212 | `	}else if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'b' \|\| z[1] == 'B') ){` |
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
|  3713539 |  227 | `	}else if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'o' \|\| z[1] == 'O') ){` |
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
|  3713519 |  242 | `	}else if( z[0] == '0' ){` |
|        - |  243 | `		/* Octal: INT64_MAX == 0o777...7 (21 significant octal digits). Skip the` |
|        - |  244 | `		 * leading zeros (incl. the base '0'); a non-octal char such as the 8.1` |
|        - |  245 | `		 * "0o" marker ends the run and leaves it to the int path (as today). */` |
|  1396123 |  246 | `		p = z;` |
|  2792247 |  247 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|  1408031 |  248 | `		for( q = p, n = 0; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){ n++; }` |
|  1396123 |  249 | `		if( n <= 21 ){` |
|  1396121 |  250 | `			return FALSE;` |
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
|  2317401 |  263 | `	p = z;` |
|  2317401 |  264 | `	while( p < zEnd && p[0] == '0' ){ p++; }` |
|  5603561 |  265 | `	for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisDigit(q[0]); q++ ){ n++; }` |
|  2317401 |  266 | `	if( n > 19 \|\| (n == 19 && SyMemcmp(p, "9223372036854775807", 19) > 0) ){` |
|       25 |  267 | `		*pbDecimal = TRUE;` |
|       25 |  268 | `		return TRUE;` |
|        - |  269 | `	}` |
|  2317377 |  270 | `	return FALSE;` |
|  1909370 |  271 | `}` |
|  3827464 |  272 | `PH7_PRIVATE sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  273 | `{` |
|  3827469 |  274 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|  3827469 |  275 | `	sxu32 nIdx = 0;` |
|        - |  276 | `	char zScratch[GEN_NUM_SCRATCH];` |
|  3827469 |  277 | `	char *zAlloc = 0;` |
|        - |  278 | `	SyString sNum;` |
|        - |  279 | `	sxi32 rc;` |
|  1913732 |  280 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|  3827469 |  281 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|  3827469 |  282 | `	if( rc != SXRET_OK ){` |
|        9 |  283 | `		return rc;` |
|        - |  284 | `	}` |
|  5741192 |  285 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|  1913729 |  286 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|  3827463 |  287 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  288 | `		return SXERR_ABORT;` |
|        - |  289 | `	}` |
|  3827463 |  290 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|        - |  291 | `		ph7_value *pObj;` |
|        - |  292 | `		sxi64 iValue;` |
|  3818735 |  293 | `		ph7_real rOverflow = 0;` |
|  3818735 |  294 | `		int bDecimalOverflow = 0;` |
|  3818735 |  295 | `		if( GenStateIntLiteralOverflows(&sNum,&rOverflow,&bDecimalOverflow) ){` |
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
|  3818701 |  312 | `			iValue = PH7_TokenValueToInt64(&sNum);` |
|  3818701 |  313 | `			pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|  3818701 |  314 | `			if( pObj == 0 ){` |
|      ! 0 |  315 | `				if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|      ! 0 |  316 | `				return SXERR_ABORT;` |
|        - |  317 | `			}` |
|  3818701 |  318 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|        - |  319 | `		}` |
|  1909370 |  320 | `	}else{` |
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
|  3827463 |  333 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|        - |  334 | `	/* Emit the load constant instruction */` |
|  3827463 |  335 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        - |  336 | `	/* Node successfully compiled */` |
|  3827463 |  337 | `	return SXRET_OK;` |
|  1913737 |  338 | `}` |
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
|  5564472 |  350 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  351 | `{` |
|  5564477 |  352 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|        - |  353 | `	const char *zIn,*zCur,*zEnd;` |
|        - |  354 | `	ph7_value *pObj;` |
|        - |  355 | `	sxu32 nIdx;` |
|        - |  356 | `	sxi32 bHasEsc;` |
|  5564477 |  357 | `	nIdx = 0; /* Prevent compiler warning */` |
|        - |  358 | `	/* Delimit the string */` |
|  5564477 |  359 | `	zIn  = pStr->zString;` |
|  5564477 |  360 | `	zEnd = &zIn[pStr->nByte];` |
|  5564477 |  361 | `	if( zIn >= zEnd ){` |
|        - |  362 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|        - |  363 | `		 * rather than reserving a new object each time. */` |
|   407869 |  364 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|   407869 |  365 | `		return SXRET_OK;` |
|        - |  366 | `	}` |
|        - |  367 | `	/* A single-quoted literal whose raw source holds a backslash unescapes to a` |
|        - |  368 | `	 * value that differs from that source (\\ -> \, \' -> '). The literal cache` |
|        - |  369 | `	 * keys FIND on the raw source text but INSTALL on the unescaped value, so` |
|        - |  370 | `	 * caching such a literal lets an unrelated one collide with it — e.g. '\\'` |
|        - |  371 | `	 * (raw \\, value \) would find the entry installed for '\\\\' (raw \\\\,` |
|        - |  372 | `	 * value \\) and load two backslashes. Only cache literals whose value equals` |
|        - |  373 | `	 * their source, i.e. those with no backslash to unescape. */` |
|  5156613 |  374 | `	bHasEsc = 0;` |
|        - |  375 | `	{` |
|        - |  376 | `		const char *zScan;` |
| 61623487 |  377 | `		for( zScan = zIn ; zScan < zEnd ; zScan++ ){` |
| 56548589 |  378 | `			if( zScan[0] == '\\' ){ bHasEsc = 1; break; }` |
| 28233442 |  379 | `		}` |
|        - |  380 | `	}` |
|  5156613 |  381 | `	if( !bHasEsc && SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|        - |  382 | `		/* Already processed,emit the load constant instruction` |
|        - |  383 | `		 * and return.` |
|        - |  384 | `		 */` |
|  3009123 |  385 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|  3009123 |  386 | `		return SXRET_OK;` |
|        - |  387 | `	}` |
|        - |  388 | `	/* Reserve a new constant */` |
|  2147495 |  389 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  2147495 |  390 | `	if( pObj == 0 ){` |
|      ! 0 |  391 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |  392 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|      ! 0 |  393 | `		return SXERR_ABORT;` |
|        - |  394 | `	}` |
|  2147495 |  395 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|        - |  396 | `	/* Compile the node */` |
|  2200009 |  397 | `	for(;;){` |
|  4400023 |  398 | `		if( zIn >= zEnd ){` |
|        - |  399 | `			/* End of input */` |
|  2147495 |  400 | `			break;` |
|        - |  401 | `		}` |
|  2252533 |  402 | `		zCur = zIn;` |
| 43530575 |  403 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
| 41278047 |  404 | `			zIn++;` |
|        5 |  405 | `		}` |
|  2252533 |  406 | `		if( zIn > zCur ){` |
|        - |  407 | `			/* Append raw contents*/` |
|  2209751 |  408 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|  1104873 |  409 | `		}` |
|  2252533 |  410 | `		zIn++;` |
|  2252533 |  411 | `		if( zIn < zEnd ){` |
|   143909 |  412 | `			if( zIn[0] == '\\' ){` |
|        - |  413 | `				/* A literal backslash */` |
|    35017 |  414 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|   126403 |  415 | `			}else if( zIn[0] == '\'' ){` |
|        - |  416 | `				/* A single quote */` |
|       15 |  417 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|        8 |  418 | `			}else{` |
|        - |  419 | `				/* verbatim copy */` |
|   108883 |  420 | `				zIn--;` |
|   108883 |  421 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char)*2);` |
|   108883 |  422 | `				zIn++;` |
|        - |  423 | `			}` |
|    71952 |  424 | `		}` |
|        - |  425 | `		/* Advance the stream cursor */` |
|  2252533 |  426 | `		zIn++;` |
|        5 |  427 | `	}` |
|        - |  428 | `	/* Emit the load constant instruction */` |
|  2147495 |  429 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|  2147495 |  430 | `	if( !bHasEsc && pStr->nByte < 1024 ){` |
|        - |  431 | `		/* Install in the literal table (only when value == source; see above) */` |
|  2065785 |  432 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|  1032890 |  433 | `	}` |
|        - |  434 | `	/* Node successfully compiled */` |
|  2147495 |  435 | `	return SXRET_OK;` |
|  2782241 |  436 | `}` |
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
|     2330 |  603 | `static sxi32 GenStateProcessStringExpression(` |
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
|     2335 |  614 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|        - |  615 | `	/* Preallocate some slots */` |
|     2335 |  616 | `	SySetAlloc(&sToken,0x08);` |
|        - |  617 | `	/* Tokenize the text */` |
|     2335 |  618 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken,0);` |
|        - |  619 | `	/* Swap delimiter */` |
|     2335 |  620 | `	pTmpIn  = pGen->pIn;` |
|     2335 |  621 | `	pTmpEnd = pGen->pEnd;` |
|     2335 |  622 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|     2335 |  623 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|        - |  624 | ``	/* Compile the expression. An interpolated `"...$x..."` READS $x — php warns`` |
|        - |  625 | `	 * "Undefined variable $x" and substitutes the empty string — so ask for a` |
|        - |  626 | `	 * read-only load rather than letting the default vivify it silently. */` |
|     2335 |  627 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|        - |  628 | `	/* Restore token stream */` |
|     2335 |  629 | `	pGen->pIn  = pTmpIn;` |
|     2335 |  630 | `	pGen->pEnd = pTmpEnd;` |
|        - |  631 | `	/* Release the token set */` |
|     2335 |  632 | `	SySetRelease(&sToken);` |
|        - |  633 | `	/* Compilation result */` |
|     2335 |  634 | `	return rc;` |
|        5 |  635 | `}` |
|        - |  636 | `/*` |
|        - |  637 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|        - |  638 | ` */` |
|   126514 |  639 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|        5 |  640 | `{` |
|        - |  641 | `	ph7_value *pConstObj;` |
|   126519 |  642 | `	sxu32 nIdx = 0;` |
|        - |  643 | `	/* Reserve a new constant */` |
|   126519 |  644 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   126519 |  645 | `	if( pConstObj == 0 ){` |
|      ! 0 |  646 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|      ! 0 |  647 | `		return 0;` |
|        - |  648 | `	}` |
|   126519 |  649 | `	(*pCount)++;` |
|   126519 |  650 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|        - |  651 | `	/* Emit the load constant instruction */` |
|   126519 |  652 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   126519 |  653 | `	return pConstObj;` |
|    63262 |  654 | `}` |
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
|   125322 |  717 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen,int bHeredoc)` |
|        5 |  718 | `{` |
|   125327 |  719 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|        - |  720 | `	const char *zIn,*zCur,*zEnd;` |
|   125327 |  721 | `	ph7_value *pObj = 0;` |
|        - |  722 | `	sxi32 iCons;` |
|        - |  723 | `	sxi32 nInterp;   /* how many of iCons came from an interpolated EXPRESSION */` |
|        - |  724 | `	sxi32 rc;` |
|        - |  725 | `	/* Delimit the string */` |
|   125327 |  726 | `	zIn  = pStr->zString;` |
|   125327 |  727 | `	zEnd = &zIn[pStr->nByte];` |
|   125327 |  728 | `	if( zIn >= zEnd ){` |
|        - |  729 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|        - |  730 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|        - |  731 | `		 * literal table from growing when many "" literals appear in the source.` |
|        - |  732 | `		 */` |
|      445 |  733 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|      445 |  734 | `		return SXRET_OK;` |
|        - |  735 | `	}` |
|   124887 |  736 | `	zCur = 0;` |
|        - |  737 | `	/* Compile the node */` |
|   124887 |  738 | `	iCons = 0;` |
|   124887 |  739 | `	nInterp = 0;` |
|    63605 |  740 | `	for(;;){` |
|   171689 |  741 | `		zCur = zIn;` |
|  1680055 |  742 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  1510703 |  743 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|       85 |  744 | `				break;` |
|  1510542 |  745 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|     2176 |  746 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|     1088 |  747 | `					break;` |
|        - |  748 | `			}` |
|  1508371 |  749 | `			zIn++;` |
|        5 |  750 | `		}` |
|   171689 |  751 | `		if( zIn > zCur ){` |
|    95837 |  752 | `			if( pObj == 0 ){` |
|    95153 |  753 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    95153 |  754 | `				if( pObj == 0 ){` |
|      ! 0 |  755 | `					return SXERR_ABORT;` |
|        - |  756 | `				}` |
|    47574 |  757 | `			}` |
|    95837 |  758 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    47916 |  759 | `		}` |
|   171689 |  760 | `		if( zIn >= zEnd ){` |
|   124883 |  761 | `			break;` |
|        - |  762 | `		}` |
|    46811 |  763 | `		if( zIn[0] == '\\' ){` |
|    44479 |  764 | `			const char *zPtr = 0;` |
|        - |  765 | `			sxu32 n;` |
|    44479 |  766 | `			zIn++;` |
|    44479 |  767 | `			if( pObj == 0 ){` |
|    31371 |  768 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    31371 |  769 | `				if( pObj == 0 ){` |
|      ! 0 |  770 | `					return SXERR_ABORT;` |
|        - |  771 | `				}` |
|    15683 |  772 | `			}` |
|    44479 |  773 | `			if( zIn >= zEnd ){` |
|        - |  774 | `				/* Lone backslash at the very end of the body: php keeps it */` |
|        3 |  775 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|        3 |  776 | `				break;` |
|        - |  777 | `			}` |
|    44477 |  778 | `			n = sizeof(char); /* size of conversion */` |
|    44477 |  779 | `			switch( zIn[0] ){` |
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
|    19708 |  796 | `			case 'n':` |
|        - |  797 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|    39421 |  798 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|    39421 |  799 | `				break;` |
|       27 |  800 | `			case 'r':` |
|        - |  801 | `				/* Carriage return (CR)[ctrl+m] ASCII code 13 */` |
|       59 |  802 | `				PH7_MemObjStringAppend(pObj,"\r",sizeof(char));` |
|       59 |  803 | `				break;` |
|     1973 |  804 | `			case 't':` |
|        - |  805 | `				/* Horizontal tab (HT)[ctrl+i] ASCII code 9 */` |
|     3951 |  806 | `				PH7_MemObjStringAppend(pObj,"\t",sizeof(char));` |
|     3951 |  807 | `				break;` |
|        3 |  808 | `			case 'v':` |
|        - |  809 | `				/* Vertical tab(VT)[ctrl+k] ASCII code 11 */` |
|        7 |  810 | `				PH7_MemObjStringAppend(pObj,"\v",sizeof(char));` |
|        7 |  811 | `				break;` |
|      147 |  812 | `			case '"':` |
|      299 |  813 | `				if( bHeredoc ){` |
|        - |  814 | `					/* No active quote char in a heredoc: php keeps \" verbatim */` |
|        5 |  815 | `					PH7_MemObjStringAppend(pObj,"\\\"",sizeof(char)*2);` |
|        3 |  816 | `				}else{` |
|        - |  817 | `					/* Double quote */` |
|      295 |  818 | `					PH7_MemObjStringAppend(pObj,"\"",sizeof(char));` |
|        - |  819 | `				}` |
|      299 |  820 | `				break;` |
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
|      234 |  845 | `			case 'x':` |
|      702 |  846 | `				if( &zIn[1] < zEnd && SyisHex((unsigned char)zIn[1]) ){` |
|        - |  847 | `					/* \x[0-9A-Fa-f]{1,2}: a character in hexadecimal notation */` |
|      466 |  848 | `					int c = SyHexToint(zIn[1]);` |
|        - |  849 | `					char cOut;` |
|      466 |  850 | `					n += sizeof(char);` |
|      466 |  851 | `					if( &zIn[2] < zEnd && SyisHex((unsigned char)zIn[2]) ){` |
|      462 |  852 | `						c = (c << 4) + SyHexToint(zIn[2]);` |
|      462 |  853 | `						n += sizeof(char);` |
|      230 |  854 | `					}` |
|      466 |  855 | `					cOut = (char)c; /* value byte, independent of host endianness */` |
|      466 |  856 | `					PH7_MemObjStringAppend(pObj,&cOut,sizeof(char));` |
|      234 |  857 | `				}else{` |
|        - |  858 | `					/* Not an escape: keep the backslash, as php does */` |
|        5 |  859 | `					PH7_MemObjStringAppend(pObj,"\\x",sizeof(char)*2);` |
|        - |  860 | `				}` |
|      470 |  861 | `				break;` |
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
|    44477 |  922 | `			zIn += n;` |
|    44477 |  923 | `			continue;` |
|        - |  924 | `		}` |
|     2337 |  925 | `		if( zIn[0] == '{' ){` |
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
|     2171 |  960 | `			const char *zExpr = zIn;` |
|        - |  961 | `			/* Assemble variable name */` |
|     1108 |  962 | `			for(;;){` |
|        - |  963 | `				/* Jump leading dollars */` |
|     4387 |  964 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|     2171 |  965 | `					zIn++;` |
|        5 |  966 | `				}` |
|     1108 |  967 | `				for(;;){` |
|    10489 |  968 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|     7165 |  969 | `						zIn++;` |
|        5 |  970 | `					}` |
|     2221 |  971 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|        - |  972 | `						/* UTF-8 stream */` |
|      ! 0 |  973 | `						zIn++;` |
|      ! 0 |  974 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  975 | `							zIn++;` |
|      ! 0 |  976 | `						}` |
|      ! 0 |  977 | `						continue;` |
|        - |  978 | `					}` |
|     2221 |  979 | `					break;` |
|      ! 0 |  980 | `				}` |
|     2221 |  981 | `				if( zIn >= zEnd ){` |
|      293 |  982 | `					break;` |
|        - |  983 | `				}` |
|     1933 |  984 | `				if( zIn[0] == '[' ){` |
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
|     1923 | 1002 | `				}else if(zIn[0] == '{' ){` |
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
|     1921 | 1020 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|        - | 1021 | `					/* Member access operator '->' */` |
|       53 | 1022 | `					zIn += 2;` |
|     1896 | 1023 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|        - | 1024 | `					/* Static member access operator '::' */` |
|      ! 0 | 1025 | `					zIn += 2;` |
|      ! 0 | 1026 | `				}else{` |
|      938 | 1027 | `					break;` |
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
|     2171 | 1039 | `				const char *zBr = zExpr;` |
|    11609 | 1040 | `				while( zBr < zIn && zBr[0] != '[' ){` |
|     9443 | 1041 | `					zBr++;` |
|        5 | 1042 | `				}` |
|     2171 | 1043 | `				if( zBr < zIn && zIn[-1] == ']' ){` |
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
|     2169 | 1084 | `			if( &zExpr[1] < zIn && zExpr[0] == '$' && zExpr[1] == '{' ){` |
|        3 | 1085 | `				PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|        - | 1086 | `					"syntax error, \"${\" string interpolation was removed in php 8.2, use \"{$...}\" instead");` |
|        3 | 1087 | `				return SXERR_ABORT;` |
|        - | 1088 | `			}` |
|        - | 1089 | `			/* Process the expression */` |
|     2167 | 1090 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|     2167 | 1091 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1092 | `				return SXERR_ABORT;` |
|        - | 1093 | `			}` |
|     2167 | 1094 | `			if( rc != SXERR_EMPTY ){` |
|     2167 | 1095 | `				++iCons;` |
|     2167 | 1096 | `				++nInterp;` |
|     1081 | 1097 | `			}` |
|        - | 1098 | `		}` |
|        - | 1099 | `		/* Invalidate the previously used constant */` |
|     2333 | 1100 | `		pObj = 0;` |
|        5 | 1101 | `	}/*for(;;)*/` |
|   124885 | 1102 | `	if( iCons > 1 ){` |
|        - | 1103 | `		/* Concatenate all compiled constants */` |
|     1599 | 1104 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|   124088 | 1105 | `	}else if( iCons == 1 && nInterp == 1 ){` |
|        - | 1106 | `		/* A string that is nothing but one interpolation ("$x") still has to` |
|        - | 1107 | `		 * PRODUCE A STRING. With no CAT to force the conversion the operand was` |
|        - | 1108 | ``		 * left on the stack untouched, so `$s = "$x"` handed back $x's own type:`` |
|        - | 1109 | `		 * "$arr" stayed an array (and skipped php's "Array to string conversion"` |
|        - | 1110 | `		 * warning), "$int" stayed an int, "$res" stayed a resource. */` |
|       13 | 1111 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CVT_STR,0,0,0,0);` |
|        6 | 1112 | `	}` |
|        - | 1113 | `	/* Node successfully compiled */` |
|   124885 | 1114 | `	return SXRET_OK;` |
|    62666 | 1115 | `}` |
|        - | 1116 | `/*` |
|        - | 1117 | ` * Compile a double quoted string.` |
|        - | 1118 | ` *  See the block-comment above for more information.` |
|        - | 1119 | ` */` |
|   125258 | 1120 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 | 1121 | `{` |
|        - | 1122 | `	sxi32 rc;` |
|   125263 | 1123 | `	rc = GenStateCompileString(&(*pGen),0/*bHeredoc*/);` |
|    62629 | 1124 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        - | 1125 | `	/* Compilation result */` |
|   125263 | 1126 | `	return rc;` |
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
|  1566710 | 1170 | `PH7_PRIVATE sxi32 GenStateCompileArrayEntry(` |
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
|  1566715 | 1181 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|        - | 1182 | `	/* Compile the expression*/` |
|  1566715 | 1183 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|        - | 1184 | `	/* Restore token stream */` |
|  1566715 | 1185 | `	RE_SWAP_DELIMITER(pGen);` |
|  1566715 | 1186 | `	return rc;` |
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
|        4 | 1197 | `{` |
|       40 | 1198 | `	sxi32 rc = SXRET_OK;` |
|       40 | 1199 | `	if( pRoot->pOp ){` |
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
|       40 | 1216 | `	return rc;` |
|        4 | 1217 | `}` |
|        - | 1218 | `/*` |
|        - | 1219 | ` * Find the top-level '=>' (PH7_TK_ARRAY_OP) that separates an array/list entry's` |
|        - | 1220 | ` * key from its value within [pStart,pEnd). The scan skips any '=>' nested inside` |
|        - | 1221 | ` * brackets/parens/braces, inside an arrow-function signature (fn(...) =>), or` |
|        - | 1222 | ` * inside a match() {...} arm — none of which are key/value separators. Returns a` |
|        - | 1223 | ` * pointer to the '=>' token, or pEnd if the entry has no top-level separator.` |
|        - | 1224 | ` */` |
|  1491630 | 1225 | `PH7_PRIVATE SyToken * GenStateFindTopLevelArrow(SyToken *pStart,SyToken *pEnd)` |
|        5 | 1226 | `{` |
|  1491635 | 1227 | `	SyToken *pCur = pStart;` |
|  1491635 | 1228 | `	sxi32 iNest = 0;` |
|  3810565 | 1229 | `	while( pCur < pEnd ){` |
|  2857221 | 1230 | `		if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|   538287 | 1231 | `			return pCur;` |
|        - | 1232 | `		}` |
|        - | 1233 | `		/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|        - | 1234 | `		 * The '=>' inside an arrow function introduces the expression body,` |
|        - | 1235 | `		 * not an entry separator. Skip past the signature.` |
|        - | 1236 | `		 */` |
|  2318939 | 1237 | `		if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
|    23403 | 1238 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|    23403 | 1239 | `			SyToken *pFn = pCur;` |
|    23398 | 1240 | `			if( nKw == PH7_TKWRD_STATIC && &pCur[1] < pEnd` |
|      ! 0 | 1241 | `				&& (pCur[1].nType & PH7_TK_KEYWORD)` |
|        5 | 1242 | `				&& SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ){` |
|      ! 0 | 1243 | `				pFn = &pCur[1];` |
|      ! 0 | 1244 | `				nKw = PH7_TKWRD_FN;` |
|      ! 0 | 1245 | `			}` |
|    23403 | 1246 | `			if( nKw == PH7_TKWRD_FN ){` |
|        5 | 1247 | `				pCur = pFn + 1; /* past 'fn' */` |
|        5 | 1248 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_AMPER) ){` |
|      ! 0 | 1249 | `					pCur++;` |
|      ! 0 | 1250 | `				}` |
|        5 | 1251 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|        5 | 1252 | `					pCur++;` |
|        5 | 1253 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|        - | 1254 | `						PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|        5 | 1255 | `					if( pCur < pEnd ){` |
|        5 | 1256 | `						pCur++;` |
|        2 | 1257 | `					}` |
|        2 | 1258 | `				}` |
|        5 | 1259 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_COLON) ){` |
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
|        5 | 1273 | `				return pEnd;` |
|        - | 1274 | `			}` |
|        - | 1275 | `			/* Match expression (PHP 8.0): the '=>' inside match arms is not an` |
|        - | 1276 | `			 * entry separator. Skip past the full match span. */` |
|    23399 | 1277 | `			if( nKw == PH7_TKWRD_MATCH ){` |
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
|    11696 | 1297 | `		}` |
|  2318933 | 1298 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|    55005 | 1299 | `			iNest++;` |
|  2291433 | 1300 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/) ){` |
|        - | 1301 | `			/* Don't worry about mismatched brackets here, the expression` |
|        - | 1302 | `			 * parser will shortly detect any syntax error. */` |
|    55005 | 1303 | `			iNest--;` |
|    27500 | 1304 | `		}` |
|  2318933 | 1305 | `		pCur++;` |
|        5 | 1306 | `	}` |
|   953349 | 1307 | `	return pEnd;` |
|   745820 | 1308 | `}` |
|        - | 1309 | `/*` |
|        - | 1310 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|        - | 1311 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|        - | 1312 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|        - | 1313 | ` */` |
|   672108 | 1314 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|        5 | 1315 | `{` |
|        - | 1316 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|        - | 1317 | `	SyToken *pKey,*pCur;` |
|   672113 | 1318 | `	sxi32 iEmitRef = 0;` |
|   672113 | 1319 | `	sxi32 iSpread = 0;` |
|   672113 | 1320 | `	sxi32 nPair = 0;` |
|        - | 1321 | `	sxi32 rc;` |
|   672113 | 1322 | `	xValidator = 0;` |
|   918254 | 1323 | `	for(;;){` |
|        - | 1324 | `		/* Jump leading commas. Exactly ONE separates two entries; a second one (or a comma` |
|        - | 1325 | `		 * at the very start) means an EMPTY element, which php rejects outright — PH7 just` |
|        - | 1326 | ``		 * skipped them, so `array(,)` and `array(1,,2)` compiled silently. A TRAILING comma`` |
|        - | 1327 | `		 * is legal and is handled by the loop exiting on the next pass. */` |
|   582200 | 1328 | `		{` |
|  1836513 | 1329 | `			int nSkip = 0;` |
|  2725543 | 1330 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|   889035 | 1331 | `				nSkip++;` |
|   889035 | 1332 | `				pGen->pIn++;` |
|        5 | 1333 | `			}` |
|  1836513 | 1334 | `			if( nSkip > 1 \|\| (nSkip > 0 && nPair < 1) ){` |
|      ! 0 | 1335 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn[-1].nLine,` |
|        - | 1336 | `					"Cannot use empty array elements in arrays");` |
|      ! 0 | 1337 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1338 | `					return SXERR_ABORT;` |
|        - | 1339 | `				}` |
|      ! 0 | 1340 | `				return SXRET_OK;` |
|        - | 1341 | `			}` |
|        - | 1342 | `		}` |
|  1836513 | 1343 | `		pCur = pGen->pIn;` |
|  1836513 | 1344 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|        - | 1345 | `			/* No more entry to process */` |
|   672095 | 1346 | `			break;` |
|        - | 1347 | `		}` |
|  1164423 | 1348 | `		if( pCur >= pGen->pIn ){` |
|      ! 0 | 1349 | `			continue;` |
|        - | 1350 | `		}` |
|        - | 1351 | `		/* Compile the key if available */` |
|  1164423 | 1352 | `		pKey = pCur;` |
|  1164423 | 1353 | `		pCur = GenStateFindTopLevelArrow(pCur,pGen->pIn);` |
|  1164423 | 1354 | `		rc = SXERR_EMPTY;` |
|  1164423 | 1355 | `		if( pCur < pGen->pIn ){` |
|   402021 | 1356 | `			if( pKey == pCur ){` |
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
|   402019 | 1370 | `			if( &pCur[1] >= pGen->pIn ){` |
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
|   402009 | 1381 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|        - | 1382 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|   402009 | 1383 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1384 | `				return SXERR_ABORT;` |
|        - | 1385 | `			}` |
|   402009 | 1386 | `			pCur++; /* Jump the '=>' operator */` |
|   201007 | 1387 | `		}else{` |
|        - | 1388 | `			/* Reset back the cursor and point to the entry value */` |
|   762407 | 1389 | `			pCur = pKey;` |
|        - | 1390 | `		}` |
|  1164411 | 1391 | `		if( rc == SXERR_EMPTY ){` |
|        - | 1392 | `			/* No key given: load the nil, TAGGED so LOAD_MAP knows this is an absent key` |
|        - | 1393 | ``			 * (auto-index) rather than an explicit `null =>` one, which php deprecates. */`` |
|   762407 | 1394 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,PH7_LOADC_NOKEY,0 /* nil index */,0,0);` |
|   381201 | 1395 | `		}` |
|  1164411 | 1396 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
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
|  1164409 | 1417 | `		iSpread = (pCur < pGen->pIn && (pCur->nType & PH7_TK_ELLIPSIS)) ? 1 : 0;` |
|  1164409 | 1418 | `		if( iSpread && (rc != SXERR_EMPTY \|\| iEmitRef) ){` |
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
|  1746605 | 1435 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,` |
|   582200 | 1436 | `			iEmitRef ? EXPR_FLAG_LOAD_IDX_STORE` |
|        - | 1437 | `			         : EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,` |
|   582200 | 1438 | `			xValidator);` |
|  1164405 | 1439 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1440 | `			return SXERR_ABORT;` |
|        - | 1441 | `		}` |
|  1164405 | 1442 | `		if( iSpread ){` |
|        - | 1443 | `			/* Mark the value on TOS as a spread source; LOAD_MAP merges it. */` |
|       73 | 1444 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FLAG_SPREAD,0,0,0,0);` |
|  1164370 | 1445 | `		}else if( iEmitRef ){` |
|        - | 1446 | `			/* Emit the load reference instruction */` |
|       40 | 1447 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|       18 | 1448 | `		}` |
|  1164405 | 1449 | `		xValidator = 0;` |
|  1164405 | 1450 | `		iEmitRef = 0;` |
|  1164405 | 1451 | `		iSpread = 0;` |
|  1164405 | 1452 | `		nPair++;` |
|        5 | 1453 | `	}` |
|        - | 1454 | `	/* Emit the load map instruction */` |
|   672095 | 1455 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|        - | 1456 | `	/* Node successfully compiled */` |
|   672095 | 1457 | `	return SXRET_OK;` |
|   336059 | 1458 | `}` |
|        - | 1459 | `/*` |
|        - | 1460 | ` * Compile the 'array' language construct.` |
|        - | 1461 | ` *	 According to the PHP language reference manual` |
|        - | 1462 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|        - | 1463 | ` *   values to keys. This type is optimized for several different uses; it can` |
|        - | 1464 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|        - | 1465 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|        - | 1466 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|        - | 1467 | ` */` |
|   428858 | 1468 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 | 1469 | `{` |
|        - | 1470 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   428863 | 1471 | `	pGen->pIn += 2;` |
|   428863 | 1472 | `	pGen->pEnd--;` |
|   214429 | 1473 | `	SXUNUSED(iCompileFlag);` |
|   428863 | 1474 | `	return GenStateCompileArrayBody(pGen);` |
|        5 | 1475 | `}` |
|        - | 1476 | `/*` |
|        - | 1477 | ` * Compile the PHP 8.5 clone(...) call form:` |
|        - | 1478 | `` *   clone($object)                          -> identical to the `clone $object` operator`` |
|        - | 1479 | ` *   clone($object, ['prop' => value, ...])  -> clone, run __clone(), then apply the` |
|        - | 1480 | ` *                                              property updates as scope-aware writes` |
|        - | 1481 | ` *   clone(object: $o, withProperties: [..]) -> the named-argument spelling` |
|        - | 1482 | ` * Codegen: compile the object argument and emit OP_CLONE (which clones and runs` |
|        - | 1483 | ` * __clone()); if a withProperties argument is present, compile it and emit` |
|        - | 1484 | ` * OP_CLONE_APPLY, which applies each update to the fresh clone AFTER __clone(),` |
|        - | 1485 | ` * honouring visibility / readonly-set-scope / typed-property enforcement in the` |
|        - | 1486 | ` * calling scope. The parser (ExprExtractNode) delimited this node's tokens as` |
|        - | 1487 | `` * `clone ( ... )`; pGen->pIn/pEnd point at the first/one-past-last of that range.`` |
|        - | 1488 | ` */` |
|       22 | 1489 | `PH7_PRIVATE sxi32 PH7_CompileCloneCall(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        2 | 1490 | `{` |
|        - | 1491 | `	SyToken *pIn,*pEnd,*pNext;` |
|       24 | 1492 | `	SyToken *pObjStart = 0,*pObjEnd = 0;` |
|       24 | 1493 | `	SyToken *pUpdStart = 0,*pUpdEnd = 0;` |
|       24 | 1494 | `	int nArg = 0;` |
|        - | 1495 | `	sxi32 rc;` |
|       11 | 1496 | `	SXUNUSED(iCompileFlag);` |
|        - | 1497 | `	/* pGen->pIn -> 'clone', pGen->pIn[1] -> '(', pGen->pEnd -> one past ')'. */` |
|       24 | 1498 | `	pIn  = pGen->pIn + 2;   /* skip 'clone' and the opening '(' */` |
|       24 | 1499 | `	pEnd = pGen->pEnd - 1;  /* exclude the closing ')' */` |
|        - | 1500 | `	/* clone(...) first-class-callable form: a lone ellipsis is the whole list. */` |
|       24 | 1501 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|      ! 0 | 1502 | `		return PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|        - | 1503 | `			"clone(...) first-class callable form is not yet supported");` |
|        - | 1504 | `	}` |
|        - | 1505 | `	/* Split the (at most two) comma-separated arguments, tolerating named labels. */` |
|       62 | 1506 | `	while( pIn < pEnd ){` |
|       40 | 1507 | `		SyToken *pArgStart,*pArgEnd,*pName = 0;` |
|       40 | 1508 | `		if( PH7_GetNextExpr(pIn,pEnd,&pNext) != SXRET_OK ){` |
|      ! 0 | 1509 | `			break;` |
|        - | 1510 | `		}` |
|       40 | 1511 | `		pArgStart = pIn;` |
|       40 | 1512 | `		pArgEnd   = pNext;` |
|        - | 1513 | `		/* Named-argument label: <ID\|keyword> ':' expr. A single ':' is PH7_TK_COLON;` |
|        - | 1514 | ``		 * '::' is a distinct operator token, so this never mis-fires on `A::B`. */`` |
|       38 | 1515 | `		if( (pArgEnd - pArgStart) >= 2` |
|       37 | 1516 | `			&& (pArgStart[0].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|       23 | 1517 | `			&& (pArgStart[1].nType & PH7_TK_COLON) ){` |
|        5 | 1518 | `			pName = pArgStart;` |
|        5 | 1519 | `			pArgStart += 2;` |
|        2 | 1520 | `		}` |
|       40 | 1521 | `		if( pName ){` |
|        - | 1522 | `` 			/* PHP named parameters are case-SENSITIVE, so `Object:`/`WITHPROPERTIES:` `` |
|        - | 1523 | `			 * must be rejected as unknown (SyMemcmp, not SyStrnicmp). */` |
|        4 | 1524 | `			if( pName->sData.nByte == sizeof("object")-1` |
|        4 | 1525 | `				&& SyMemcmp(pName->sData.zString,"object",sizeof("object")-1) == 0 ){` |
|        3 | 1526 | `				pObjStart = pArgStart; pObjEnd = pArgEnd;` |
|        4 | 1527 | `			}else if( pName->sData.nByte == sizeof("withProperties")-1` |
|        3 | 1528 | `				&& SyMemcmp(pName->sData.zString,"withProperties",sizeof("withProperties")-1) == 0 ){` |
|        3 | 1529 | `				pUpdStart = pArgStart; pUpdEnd = pArgEnd;` |
|        2 | 1530 | `			}else{` |
|      ! 0 | 1531 | `				return PH7_GenCompileError(pGen,E_ERROR,pName->nLine,` |
|      ! 0 | 1532 | `					"Unknown named parameter $%z",&pName->sData);` |
|        1 | 1533 | `			}` |
|       38 | 1534 | `		}else if( nArg == 0 ){` |
|       22 | 1535 | `			pObjStart = pArgStart; pObjEnd = pArgEnd;` |
|       25 | 1536 | `		}else if( nArg == 1 ){` |
|       15 | 1537 | `			pUpdStart = pArgStart; pUpdEnd = pArgEnd;` |
|        8 | 1538 | `		}else{` |
|      ! 0 | 1539 | `			return PH7_GenCompileError(pGen,E_ERROR,pArgStart->nLine,` |
|        - | 1540 | `				"clone() expects at most 2 arguments");` |
|        - | 1541 | `		}` |
|       40 | 1542 | `		nArg++;` |
|       40 | 1543 | `		pIn = pNext;` |
|       40 | 1544 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|       17 | 1545 | `			pIn++; /* step over the argument separator */` |
|        8 | 1546 | `		}` |
|        2 | 1547 | `	}` |
|       24 | 1548 | `	if( pObjStart == 0 \|\| pObjStart >= pObjEnd ){` |
|      ! 0 | 1549 | `		return PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 1550 | `			"clone() expects at least 1 argument, 0 given");` |
|        - | 1551 | `	}` |
|        - | 1552 | `	/* Object argument -> clone (+ __clone()). */` |
|       24 | 1553 | `	rc = GenStateCompileArrayEntry(pGen,pObjStart,pObjEnd,EXPR_FLAG_RDONLY_LOAD,0);` |
|       24 | 1554 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 1555 | `		return SXERR_ABORT;` |
|        - | 1556 | `	}` |
|       24 | 1557 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_CLONE,0,0,0,0);` |
|        - | 1558 | `	/* Property updates (evaluated after __clone runs). */` |
|       24 | 1559 | `	if( pUpdStart && pUpdStart < pUpdEnd ){` |
|       17 | 1560 | `		rc = GenStateCompileArrayEntry(pGen,pUpdStart,pUpdEnd,EXPR_FLAG_RDONLY_LOAD,0);` |
|       17 | 1561 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1562 | `			return SXERR_ABORT;` |
|        - | 1563 | `		}` |
|       17 | 1564 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CLONE_APPLY,0,0,0,0);` |
|        8 | 1565 | `	}` |
|       24 | 1566 | `	return SXRET_OK;` |
|       13 | 1567 | `}` |
|        - | 1568 | `/*` |
|        - | 1569 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|        - | 1570 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|        - | 1571 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|        - | 1572 | ` */` |
|   243250 | 1573 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 | 1574 | `{` |
|        - | 1575 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|   243255 | 1576 | `	pGen->pIn++;` |
|   243255 | 1577 | `	pGen->pEnd--;` |
|   121625 | 1578 | `	SXUNUSED(iCompileFlag);` |
|   243255 | 1579 | `	return GenStateCompileArrayBody(pGen);` |
|        5 | 1580 | `}` |
|        - | 1581 | `/*` |
|        - | 1582 | ` * Expression tree validator callback for the 'list' language construct.` |
|        - | 1583 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|        - | 1584 | ` * an invalid expression tree and this function will generate the appropriate` |
|        - | 1585 | ` * error message.` |
|        - | 1586 | ` * See the routine responible of compiling the list language construct` |
|        - | 1587 | ` * for more inforation.` |
|        - | 1588 | ` */` |
|      226 | 1589 | `static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        5 | 1590 | `{` |
|      231 | 1591 | `	sxi32 rc = SXRET_OK;` |
|      231 | 1592 | `	if( pRoot->pOp ){` |
|        4 | 1593 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */` |
|        2 | 1594 | `			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){` |
|        - | 1595 | `				/* Unexpected expression */` |
|      ! 0 | 1596 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - | 1597 | `					"Assignments can only happen to writable values");` |
|      ! 0 | 1598 | `				if( rc != SXERR_ABORT ){` |
|      ! 0 | 1599 | `					rc = SXERR_INVALID;` |
|      ! 0 | 1600 | `				}` |
|        1 | 1601 | `		}` |
|      229 | 1602 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|        - | 1603 | `		/* Unexpected expression */` |
|        6 | 1604 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - | 1605 | `			"Assignments can only happen to writable values");` |
|        6 | 1606 | `		if( rc != SXERR_ABORT ){` |
|        6 | 1607 | `			rc = SXERR_INVALID;` |
|        2 | 1608 | `		}` |
|        2 | 1609 | `	}` |
|      231 | 1610 | `	return rc;` |
|        5 | 1611 | `}` |
|        - | 1612 | `/*` |
|        - | 1613 | ` * Compile the 'list' language construct.` |
|        - | 1614 | ` *  According to the PHP language reference` |
|        - | 1615 | ` *  list(): Assign variables as if they were an array.` |
|        - | 1616 | ` *  list() is used to assign a list of variables in one operation.` |
|        - | 1617 | ` *  Description` |
|        - | 1618 | ` *   array list (mixed $varname [, mixed $... ] )` |
|        - | 1619 | ` *   Like array(), this is not really a function, but a language construct.` |
|        - | 1620 | ` *   list() is used to assign a list of variables in one operation.` |
|        - | 1621 | ` *  Parameters` |
|        - | 1622 | ` *   $varname: A variable.` |
|        - | 1623 | ` *  Return Values` |
|        - | 1624 | ` *   The assigned array.` |
|        - | 1625 | ` */` |
|        - | 1626 | `/* Nested list entry recorded during first pass of list body compilation */` |
|        - | 1627 | `struct NestedListEntry {` |
|        - | 1628 | `	sxi32 nIndex;        /* Position in the outer list (0-based) */` |
|        - | 1629 | `	SyToken *pStart;     /* Token range: start of nested construct */` |
|        - | 1630 | `	SyToken *pEnd;       /* Token range: past closing delimiter */` |
|        - | 1631 | `	sxi32 isShort;       /* 1 if [...] form, 0 if list(...) form */` |
|        - | 1632 | `};` |
|        - | 1633 | `/*` |
|        - | 1634 | ` * Compile the body of a *keyed* list/short-list destructuring (PHP 7.1), where` |
|        - | 1635 | `` * every entry has the form `keyExpr => target`. The source array is on the stack`` |
|        - | 1636 | ` * top on entry and remains there on exit, mirroring the positional LOAD_LIST` |
|        - | 1637 | ` * path so the caller's teardown is unchanged. For each entry: DUP the source,` |
|        - | 1638 | ` * push the key, LOAD_IDX to fetch source[key] (NULL on a missing key, silently,` |
|        - | 1639 | ` * like a normal subscript read), then assign the fetched value to the target — a` |
|        - | 1640 | ` * nested [...]/list() recurses, a simple lvalue uses the same STORE fold as a` |
|        - | 1641 | ` * normal assignment (the value sits below the lvalue-load, exactly as in` |
|        - | 1642 | ` * GenStateEmitExprCode where the assignment RHS precedes the LHS load).` |
|        - | 1643 | ` */` |
|       28 | 1644 | `static sxi32 GenStateCompileKeyedListBody(ph7_gen_state *pGen)` |
|        2 | 1645 | `{` |
|        - | 1646 | `	SyToken *pNext;` |
|        - | 1647 | `	sxi32 rc;` |
|       66 | 1648 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|        - | 1649 | `		SyToken *pArrow,*pTarget;` |
|        - | 1650 | ``		/* Split `keyExpr => target` at the top-level '=>' */`` |
|       38 | 1651 | `		pArrow = GenStateFindTopLevelArrow(pGen->pIn,pNext);` |
|       38 | 1652 | `		pTarget = &pArrow[1];` |
|       38 | 1653 | `		if( pArrow <= pGen->pIn \|\| pTarget >= pNext ){` |
|        - | 1654 | ``			/* Empty key (`[ => $v]`) or empty value (`["k" =>]`): PHP rejects`` |
|        - | 1655 | `			 * both. Reject rather than silently emitting unbalanced bytecode. */` |
|      ! 0 | 1656 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - | 1657 | `				"Cannot use empty array entries in keyed array assignment");` |
|      ! 0 | 1658 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 1659 | `		}` |
|        - | 1660 | `		/* DUP the source array (it is on the stack top) */` |
|       38 | 1661 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|        - | 1662 | `		/* Compile the key expression; it is pushed above the DUP'd source */` |
|       38 | 1663 | `		rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pArrow,EXPR_FLAG_RDONLY_LOAD,0);` |
|       38 | 1664 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1665 | `			return SXERR_ABORT;` |
|        - | 1666 | `		}` |
|        - | 1667 | `		/* LOAD_IDX: pop the key, replace the DUP'd source with source[key].` |
|        - | 1668 | `		 * iP2=7 is the keyed-destructuring read context: an array source reads like` |
|        - | 1669 | ``		 * iP2=0 (missing key loads NULL silently, matching a normal `$arr[$k]` read;`` |
|        - | 1670 | `		 * PHP also emits an "Undefined array key" warning here, PHL omits it — §3.7),` |
|        - | 1671 | `		 * but a NON-array source yields NULL + a per-key "Cannot use <type> as array"` |
|        - | 1672 | `		 * warning instead of char-indexing a string (matching PHP's OP_LOAD_LIST path). */` |
|       38 | 1673 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,7,0,0);` |
|       38 | 1674 | `		if( pTarget < pNext && ( (pTarget->nType & PH7_TK_OSB)` |
|       34 | 1675 | `			\|\| ( (pTarget->nType & PH7_TK_KEYWORD)` |
|       18 | 1676 | `				&& SX_PTR_TO_INT(pTarget->pUserData) == PH7_TKWRD_LIST ) ) ){` |
|        - | 1677 | `			/* Nested destructuring:  ["k" => [ ... ]]  or  ["k" => list( ... )].` |
|        - | 1678 | `			 * Treat source[key] as the inner body's source, then drop the` |
|        - | 1679 | `			 * leftover it leaves behind (mirrors the positional nested path). */` |
|        5 | 1680 | `			sxi32 isShort = (pTarget->nType & PH7_TK_OSB) != 0;` |
|        5 | 1681 | `			SyToken *pSavedIn = pGen->pIn;` |
|        5 | 1682 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|        5 | 1683 | `			pGen->pIn = pTarget;` |
|        5 | 1684 | `			pGen->pEnd = pNext;` |
|        5 | 1685 | `			rc = isShort ? PH7_CompileShortList(&(*pGen),0)` |
|        2 | 1686 | `			             : PH7_CompileList(&(*pGen),0);` |
|        5 | 1687 | `			pGen->pIn = pSavedIn;` |
|        5 | 1688 | `			pGen->pEnd = pSavedEnd;` |
|        5 | 1689 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1690 | `				return SXERR_ABORT;` |
|        - | 1691 | `			}` |
|        5 | 1692 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        3 | 1693 | `		}else{` |
|        - | 1694 | `			/* Simple lvalue target ($v / $o->p / $a[i] / Cls::$s). source[key]` |
|        - | 1695 | `			 * is already on the stack as the value; compiling the target appends` |
|        - | 1696 | `			 * its lvalue-load, which we fold into a STORE just as a normal` |
|        - | 1697 | `			 * assignment does. */` |
|        - | 1698 | `			VmInstr *pInstr;` |
|       34 | 1699 | `			sxi32 iVmOp = PH7_OP_STORE;` |
|       34 | 1700 | `			sxi32 iP1 = 0, iP2 = 0;` |
|       34 | 1701 | `			void *p3 = 0;` |
|       34 | 1702 | `			rc = GenStateCompileArrayEntry(&(*pGen),pTarget,pNext,` |
|        - | 1703 | `				EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|       34 | 1704 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1705 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 1706 | `			}` |
|       34 | 1707 | `			if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|       34 | 1708 | `				if( pInstr->iOp == PH7_OP_MEMBER ){` |
|        3 | 1709 | `					iP2 = 1; /* member store: keep MEMBER, store value below it */` |
|       33 | 1710 | `				}else if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|        3 | 1711 | `					iVmOp = PH7_OP_STORE_IDX;` |
|        3 | 1712 | `					iP1 = pInstr->iP1;` |
|        3 | 1713 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|        2 | 1714 | `				}else{` |
|       30 | 1715 | `					p3 = pInstr->p3; /* named store: $v = value */` |
|       30 | 1716 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|        - | 1717 | `				}` |
|       16 | 1718 | `			}` |
|       34 | 1719 | `			PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|        - | 1720 | `			/* STORE leaves the assigned value on the stack top; drop it so the` |
|        - | 1721 | `			 * source array is back on top for the next entry. */` |
|       34 | 1722 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        - | 1723 | `		}` |
|       38 | 1724 | `		pGen->pIn = &pNext[1];` |
|        2 | 1725 | `	}` |
|       30 | 1726 | `	return SXRET_OK;` |
|       16 | 1727 | `}` |
|        - | 1728 | `/*` |
|        - | 1729 | ` * Shared body for list() and short list [...] compilation.` |
|        - | 1730 | ` * Assumes pGen->pIn and pGen->pEnd are already positioned past` |
|        - | 1731 | ` * the opening delimiter and before the closing delimiter.` |
|        - | 1732 | ` */` |
|      134 | 1733 | `static sxi32 GenStateCompileListBody(ph7_gen_state *pGen)` |
|        5 | 1734 | `{` |
|        - | 1735 | `	SySet sNested; /* Dynamically-sized container of NestedListEntry */` |
|        - | 1736 | `	SyToken *pNext;` |
|        - | 1737 | `	SyToken *pClassifyIn;` |
|      139 | 1738 | `	sxi32 nKeyed = 0, nPositional = 0, nEmpty = 0;` |
|        - | 1739 | `	sxi32 nExpr;` |
|        - | 1740 | `	sxi32 rc;` |
|        - | 1741 | ``	/* First pass: classify entries as keyed (`k => v`), positional, or empty`` |
|        - | 1742 | `	 * skip slots ([,]). A list level must be entirely keyed or entirely` |
|        - | 1743 | `	 * positional — PHP fatals on a mix, and on an empty slot inside a keyed` |
|        - | 1744 | `	 * list. */` |
|      139 | 1745 | `	pClassifyIn = pGen->pIn;` |
|      395 | 1746 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|      261 | 1747 | `		if( pGen->pIn >= pNext ){` |
|       13 | 1748 | `			nEmpty++;` |
|      255 | 1749 | `		}else if( GenStateFindTopLevelArrow(pGen->pIn,pNext) < pNext ){` |
|       38 | 1750 | `			nKeyed++;` |
|       20 | 1751 | `		}else{` |
|      213 | 1752 | `			nPositional++;` |
|        - | 1753 | `		}` |
|      261 | 1754 | `		pGen->pIn = &pNext[1];` |
|        5 | 1755 | `	}` |
|      139 | 1756 | `	pGen->pIn = pClassifyIn;` |
|      139 | 1757 | `	if( nKeyed > 0 && nEmpty > 0 ){` |
|      ! 0 | 1758 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - | 1759 | `			"Cannot use empty array entries in keyed array assignment");` |
|      ! 0 | 1760 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 1761 | `	}` |
|      139 | 1762 | `	if( nKeyed > 0 && nPositional > 0 ){` |
|      ! 0 | 1763 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - | 1764 | `			"Cannot mix keyed and unkeyed array entries in assignments");` |
|      ! 0 | 1765 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 1766 | `	}` |
|      139 | 1767 | `	if( nKeyed > 0 ){` |
|       30 | 1768 | `		return GenStateCompileKeyedListBody(pGen);` |
|        - | 1769 | `	}` |
|      111 | 1770 | `	nExpr = 0;` |
|      111 | 1771 | `	SySetInit(&sNested,&pGen->pVm->sAllocator,sizeof(struct NestedListEntry));` |
|      331 | 1772 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|      225 | 1773 | `		if( pGen->pIn < pNext ){` |
|        - | 1774 | `			/* Check for nested list() */` |
|      213 | 1775 | `			if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        3 | 1776 | `				SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|        - | 1777 | `				/* Record this nested list for post-processing */` |
|        3 | 1778 | `				SyToken *pListEnd = 0;` |
|        3 | 1779 | `				if( &pGen->pIn[1] < pNext && (pGen->pIn[1].nType & PH7_TK_LPAREN) ){` |
|        3 | 1780 | `					PH7_DelimitNestedTokens(pGen->pIn+2,pNext,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|        1 | 1781 | `				}` |
|        3 | 1782 | `				if( pListEnd ){` |
|        - | 1783 | `					struct NestedListEntry sEntry;` |
|        3 | 1784 | `					sEntry.nIndex = nExpr;` |
|        3 | 1785 | `					sEntry.pStart = pGen->pIn;` |
|        3 | 1786 | `					sEntry.pEnd = pListEnd + 1;` |
|        3 | 1787 | `					sEntry.isShort = 0;` |
|        3 | 1788 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|        1 | 1789 | `				}` |
|        - | 1790 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|        3 | 1791 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|      212 | 1792 | `			}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|        - | 1793 | `				/* Nested short destructuring [...] */` |
|       13 | 1794 | `				SyToken *pBracketEnd = 0;` |
|       13 | 1795 | `				PH7_DelimitNestedTokens(pGen->pIn+1,pNext,PH7_TK_OSB,PH7_TK_CSB,&pBracketEnd);` |
|       13 | 1796 | `				if( pBracketEnd ){` |
|        - | 1797 | `					struct NestedListEntry sEntry;` |
|       13 | 1798 | `					sEntry.nIndex = nExpr;` |
|       13 | 1799 | `					sEntry.pStart = pGen->pIn;` |
|       13 | 1800 | `					sEntry.pEnd = pBracketEnd + 1;` |
|       13 | 1801 | `					sEntry.isShort = 1;` |
|       13 | 1802 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|        6 | 1803 | `				}` |
|        - | 1804 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|       13 | 1805 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|        7 | 1806 | `			}else{` |
|        - | 1807 | `				/* Compile the expression holding the variable */` |
|      199 | 1808 | `				rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|      199 | 1809 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 1810 | `					SySetRelease(&sNested);` |
|      ! 0 | 1811 | `					return SXRET_OK;` |
|        - | 1812 | `				}` |
|        - | 1813 | `			}` |
|      109 | 1814 | `		}else{` |
|        - | 1815 | `			/* Empty entry,load NULL */` |
|       13 | 1816 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);` |
|        - | 1817 | `		}` |
|      225 | 1818 | `		nExpr++;` |
|        - | 1819 | `		/* Advance the stream cursor */` |
|      225 | 1820 | `		pGen->pIn = &pNext[1];` |
|        5 | 1821 | `	}` |
|        - | 1822 | `	/* Emit the LOAD_LIST instruction */` |
|      111 | 1823 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,0,0,0);` |
|        - | 1824 | `	/* After LOAD_LIST, the source array is still on the stack top.` |
|        - | 1825 | `	 * For each nested entry, emit code to extract the sub-array` |
|        - | 1826 | `	 * at the corresponding index and recursively destructure it.` |
|        - | 1827 | `	 */` |
|      111 | 1828 | `	if( SySetUsed(&sNested) > 0 ){` |
|       13 | 1829 | `		struct NestedListEntry *apNested = (struct NestedListEntry *)SySetBasePtr(&sNested);` |
|        - | 1830 | `		sxu32 i;` |
|       27 | 1831 | `		for(i = 0; i < SySetUsed(&sNested); i++){` |
|       15 | 1832 | `			SyToken *pSavedIn = pGen->pIn;` |
|       15 | 1833 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|        - | 1834 | `			ph7_value *pIdx;` |
|        - | 1835 | `			sxu32 nConstIdx;` |
|        - | 1836 | `			/* DUP the source array (it's on stack top) */` |
|       15 | 1837 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|        - | 1838 | `			/* Push the integer index for this nested entry */` |
|       15 | 1839 | `			pIdx = PH7_ReserveConstObj(pGen->pVm,&nConstIdx);` |
|       15 | 1840 | `			if( pIdx == 0 ){` |
|      ! 0 | 1841 | `				PH7_GenCompileError(&(*pGen),E_ERROR,0,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1842 | `				SySetRelease(&sNested);` |
|      ! 0 | 1843 | `				return SXERR_ABORT;` |
|        - | 1844 | `			}` |
|       15 | 1845 | `			PH7_MemObjInitFromInt(pGen->pVm,pIdx,(sxi64)apNested[i].nIndex);` |
|       15 | 1846 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nConstIdx,0,0);` |
|        - | 1847 | `			/* LOAD_IDX: pop index, replace DUP'd source with source[index].` |
|        - | 1848 | `			 * iP2=2 signals the VM to emit an "Undefined array key" warning` |
|        - | 1849 | `			 * when the key is missing (PHP-compatible list destructuring).` |
|        - | 1850 | `			 */` |
|       15 | 1851 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,2,0,0);` |
|        - | 1852 | `			/* Recursively compile the inner list */` |
|       15 | 1853 | `			pGen->pIn = apNested[i].pStart;` |
|       15 | 1854 | `			pGen->pEnd = apNested[i].pEnd;` |
|       15 | 1855 | `			if( apNested[i].isShort ){` |
|       13 | 1856 | `				rc = PH7_CompileShortList(&(*pGen),0);` |
|        7 | 1857 | `			}else{` |
|        3 | 1858 | `				rc = PH7_CompileList(&(*pGen),0);` |
|        - | 1859 | `			}` |
|       15 | 1860 | `			pGen->pIn = pSavedIn;` |
|       15 | 1861 | `			pGen->pEnd = pSavedEnd;` |
|       15 | 1862 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1863 | `				SySetRelease(&sNested);` |
|      ! 0 | 1864 | `				return SXERR_ABORT;` |
|        - | 1865 | `			}` |
|        - | 1866 | `			/* Pop the leftover source[index] from the inner LOAD_LIST */` |
|       15 | 1867 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        8 | 1868 | `		}` |
|        6 | 1869 | `	}` |
|      111 | 1870 | `	SySetRelease(&sNested);` |
|        - | 1871 | `	/* Node successfully compiled */` |
|      111 | 1872 | `	return SXRET_OK;` |
|       72 | 1873 | `}` |
|       40 | 1874 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 | 1875 | `{` |
|        - | 1876 | `	/* Jump the 'list' keyword, the leading '(' and exclude trailing ')' */` |
|       45 | 1877 | `	pGen->pIn += 2;` |
|       45 | 1878 | `	pGen->pEnd--;` |
|       20 | 1879 | `	SXUNUSED(iCompileFlag);` |
|       45 | 1880 | `	return GenStateCompileListBody(pGen);` |
|        5 | 1881 | `}` |
|       94 | 1882 | `PH7_PRIVATE sxi32 PH7_CompileShortList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 | 1883 | `{` |
|        - | 1884 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|       99 | 1885 | `	pGen->pIn++;` |
|       99 | 1886 | `	pGen->pEnd--;` |
|       47 | 1887 | `	SXUNUSED(iCompileFlag);` |
|       99 | 1888 | `	return GenStateCompileListBody(pGen);` |
|        5 | 1889 | `}` |
|        - | 1890 |  |
