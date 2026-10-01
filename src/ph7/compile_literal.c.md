# src/ph7/compile_literal.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1301/1512 lines (86.04%)

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
|     702 |   24 | `static int GenStateIsBaseDigit(int c, int base)` |
|       3 |   25 | `{` |
|       - |   26 | `	/* ASCII arithmetic, not <ctype.h>: the byte can be any value in a string` |
|       - |   27 | `	 * literal, and isdigit()/isxdigit() are both locale-dependent and undefined` |
|       - |   28 | `	 * for a negative char. */` |
|     705 |   29 | `	if( base == 16 ){` |
|      13 |   30 | `		return (c >= '0' && c <= '9') \|\| (c >= 'a' && c <= 'f') \|\| (c >= 'A' && c <= 'F');` |
|       - |   31 | `	}` |
|     693 |   32 | `	if( base == 8 ){ return c >= '0' && c <= '7'; }` |
|     687 |   33 | `	if( base == 2 ){ return c == '0' \|\| c == '1'; }` |
|     681 |   34 | `	return c >= '0' && c <= '9';` |
|     354 |   35 | `}` |
|       - |   36 | `/*` |
|       - |   37 | ` * Strip PHP 7.4 numeric literal separators (underscores between digits) from` |
|       - |   38 | ` * a numeric token's text and yield a SyString suitable for the low-level` |
|       - |   39 | ` * converters (SyStrToInt64 / SyStrToReal / etc.).` |
|       - |   40 | ` *` |
|       - |   41 | ` * Fast path: if the token contains no '_', *pOut aliases pToken with no copy` |
|       - |   42 | ` * and *pzAlloc is set to NULL.` |
|       - |   43 | ` * Stack path: if the cleaned bytes fit in zScratch, they are written there` |
|       - |   44 | ` * and *pzAlloc is set to NULL.` |
|       - |   45 | ` * Heap path: for literals larger than the scratch buffer, a fresh buffer is` |
|       - |   46 | ` * allocated from pAlloc, returned via *pzAlloc, and must be released by the` |
|       - |   47 | ` * caller with SyMemBackendFree once the converter is done.` |
|       - |   48 | ` *` |
|       - |   49 | ` * Returns SXRET_OK on success, SXERR_ABORT on allocator failure (in which` |
|       - |   50 | ` * case *pOut is left untouched and the caller must not read it).` |
|       - |   51 | ` */` |
|  772496 |   52 | `PH7_PRIVATE sxi32 GenStateStripNumericSeparators(` |
|       - |   53 | `	SyMemBackend *pAlloc,` |
|       - |   54 | `	const SyString *pToken,` |
|       - |   55 | `	char *zScratch, sxu32 nScratch,` |
|       - |   56 | `	SyString *pOut, char **pzAlloc)` |
|       5 |   57 | `{` |
|       - |   58 | `	sxu32 i, j;` |
|  772501 |   59 | `	int hasUnderscore = 0;` |
|       - |   60 | `	char *zBuf;` |
|  772501 |   61 | `	*pzAlloc = 0;` |
| 1896809 |   62 | `	for( i = 0; i < pToken->nByte; ++i ){` |
| 1124581 |   63 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|  561340 |   64 | `	}` |
|  772501 |   65 | `	if( !hasUnderscore ){` |
|  772233 |   66 | `		SyStringDupPtr(pOut, pToken);` |
|  772233 |   67 | `		return SXRET_OK;` |
|       - |   68 | `	}` |
|     270 |   69 | `	if( pToken->nByte <= nScratch ){` |
|     268 |   70 | `		zBuf = zScratch;` |
|     135 |   71 | `	}else{` |
|       3 |   72 | `		zBuf = (char *)SyMemBackendAlloc(pAlloc, pToken->nByte);` |
|       3 |   73 | `		if( zBuf == 0 ){` |
|     ! 0 |   74 | `			return SXERR_ABORT;` |
|       - |   75 | `		}` |
|       3 |   76 | `		*pzAlloc = zBuf;` |
|       - |   77 | `	}` |
|     270 |   78 | `	j = 0;` |
|    2992 |   79 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|    2724 |   80 | `		if( pToken->zString[i] != '_' ){ zBuf[j++] = pToken->zString[i]; }` |
|    1363 |   81 | `	}` |
|     270 |   82 | `	SyStringInitFromBuf(pOut, zBuf, j);` |
|     270 |   83 | `	return SXRET_OK;` |
|  385708 |   84 | `}` |
|       - |   85 | `/*` |
|       - |   86 | ` * Compile a numeric [i.e: integer or real] literal.` |
|       - |   87 | ` * Notes on the integer type.` |
|       - |   88 | ` *  According to the PHP language reference manual` |
|       - |   89 | ` *  Integers can be specified in decimal (base 10), hexadecimal (base 16), octal (base 8)` |
|       - |   90 | ` *  or binary (base 2) notation, optionally preceded by a sign (- or +).` |
|       - |   91 | ` *  To use octal notation, precede the number with a 0 (zero). To use hexadecimal` |
|       - |   92 | ` *  notation precede the number with 0x. To use binary notation precede the number with 0b.` |
|       - |   93 | ` * Symisc eXtension to the integer type.` |
|       - |   94 | ` *  PH7 introduced platform-independant 64-bit integer unlike the standard PHP engine` |
|       - |   95 | ` *  where the size of an integer is platform-dependent.That is,the size of an integer` |
|       - |   96 | ` *  is 8 bytes and the maximum integer size is 0x7FFFFFFFFFFFFFFF for all platforms` |
|       - |   97 | ` *  [i.e: either 32bit or 64bit].` |
|       - |   98 | ` *  For more information on this powerfull extension please refer to the official` |
|       - |   99 | ` *  documentation.` |
|       - |  100 | ` */` |
|       - |  101 | `/*` |
|       - |  102 | ` * Determine whether an integer literal token exceeds the signed 64-bit range.` |
|       - |  103 | ` * PHP promotes such a literal to a float (e.g. 9223372036854775808 ->` |
|       - |  104 | ` * float(9.22...E+18), 0xFFFFFFFFFFFFFFFF -> float) rather than wrapping or` |
|       - |  105 | ` * dropping digits. pNum is the separator-stripped token (unsigned; the sign of` |
|       - |  106 | ` * a "-1" is a separate unary operator). Base detection mirrors` |
|       - |  107 | ` * PH7_TokenValueToInt64. Returns TRUE on overflow: for a non-decimal base the` |
|       - |  108 | ` * float value is accumulated into *pReal (dv = dv*base + digit); for decimal` |
|       - |  109 | ` * *pbDecimal is set so the caller reuses strtod on the token for a` |
|       - |  110 | ` * correctly-rounded value. Returns FALSE (value fits) for anything it cannot` |
|       - |  111 | ` * confidently classify, so the int path stays in charge.` |
|       - |  112 | ` *` |
|       - |  113 | ` * The int/float CLASSIFICATION is php-exact for every base. VALUES are byte-exact` |
|       - |  114 | ` * for decimal (strtod) and hex (php's zend_hex_strtod uses the same dv*16+digit` |
|       - |  115 | ` * doubling). Octal/binary overflow values can differ from php by the low bit(s):` |
|       - |  116 | ` * php's zend_{oct,bin}_strtod rounds differently than this doubling — e.g. php's` |
|       - |  117 | ` * binary 2**63 is 2**63-1024 whereas this returns the exact 2**63. Recorded as a` |
|       - |  118 | ` * residual; matching php exactly would need a port of those functions.` |
|       - |  119 | ` */` |
|  756711 |  120 | `static int GenStateIntLiteralOverflows(const SyString *pNum, ph7_real *pReal, int *pbDecimal)` |
|       5 |  121 | `{` |
|  756716 |  122 | `	const char *z = pNum->zString;` |
|  756716 |  123 | `	const char *zEnd = z + pNum->nByte;` |
|       - |  124 | `	const char *p, *q;` |
|       - |  125 | `	int n;` |
|  756716 |  126 | `	*pbDecimal = FALSE;` |
|  756716 |  127 | `	if( z >= zEnd ){` |
|     ! 0 |  128 | `		return FALSE;` |
|       - |  129 | `	}` |
|  756716 |  130 | `	if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|       - |  131 | `		/* Hexadecimal: INT64_MAX == 0x7FFF...F (16 digits, leading nibble 7). */` |
|     323 |  132 | `		p = z + 2;` |
|     375 |  133 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|    1557 |  134 | `		for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisHex(q[0]); q++ ){ n++; }` |
|     323 |  135 | `		if( n < 16 \|\| (n == 16 && SyHexToint(p[0]) < 8) ){` |
|     317 |  136 | `			return FALSE;` |
|       - |  137 | `		}` |
|       7 |  138 | `		{ ph7_real dv = 0;` |
|     103 |  139 | `		  for( q = p; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisHex(q[0]); q++ ){` |
|      97 |  140 | `			dv = dv * 16 + (ph7_real)SyHexToint(q[0]);` |
|      49 |  141 | `		  }` |
|       7 |  142 | `		  *pReal = dv;` |
|       - |  143 | `		}` |
|       7 |  144 | `		return TRUE;` |
|  756397 |  145 | `	}else if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|       - |  146 | `		/* Binary: INT64_MAX needs 63 significant bits. */` |
|     287 |  147 | `		p = z + 2;` |
|     335 |  148 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|    2172 |  149 | `		for( q = p, n = 0; q < zEnd && (q[0] == '0' \|\| q[0] == '1'); q++ ){ n++; }` |
|     287 |  150 | `		if( n <= 63 ){` |
|     285 |  151 | `			return FALSE;` |
|       - |  152 | `		}` |
|       3 |  153 | `		{ ph7_real dv = 0;` |
|     195 |  154 | `		  for( q = p; q < zEnd && (q[0] == '0' \|\| q[0] == '1'); q++ ){` |
|     129 |  155 | `			dv = dv * 2 + (ph7_real)(q[0] - '0');` |
|      65 |  156 | `		  }` |
|       3 |  157 | `		  *pReal = dv;` |
|       - |  158 | `		}` |
|       3 |  159 | `		return TRUE;` |
|  756111 |  160 | `	}else if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'o' \|\| z[1] == 'O') ){` |
|       - |  161 | `		/* PHP 8.1 explicit octal 0o/0O: 21 significant octal digits fit in int64. */` |
|      21 |  162 | `		p = z + 2;` |
|      25 |  163 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|      97 |  164 | `		for( q = p, n = 0; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){ n++; }` |
|      21 |  165 | `		if( n <= 21 ){` |
|      21 |  166 | `			return FALSE;` |
|       - |  167 | `		}` |
|     ! 0 |  168 | `		{ ph7_real dv = 0;` |
|     ! 0 |  169 | `		  for( q = p; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){` |
|     ! 0 |  170 | `			dv = dv * 8 + (ph7_real)(q[0] - '0');` |
|     ! 0 |  171 | `		  }` |
|     ! 0 |  172 | `		  *pReal = dv;` |
|       - |  173 | `		}` |
|     ! 0 |  174 | `		return TRUE;` |
|  756091 |  175 | `	}else if( z[0] == '0' ){` |
|       - |  176 | `		/* Octal: INT64_MAX == 0o777...7 (21 significant octal digits). Skip the` |
|       - |  177 | `		 * leading zeros (incl. the base '0'); a non-octal char such as the 8.1` |
|       - |  178 | `		 * "0o" marker ends the run and leaves it to the int path (as today). */` |
|  283865 |  179 | `		p = z;` |
|  567737 |  180 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|  304759 |  181 | `		for( q = p, n = 0; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){ n++; }` |
|  283865 |  182 | `		if( n <= 21 ){` |
|  283863 |  183 | `			return FALSE;` |
|       - |  184 | `		}` |
|       3 |  185 | `		{ ph7_real dv = 0;` |
|      47 |  186 | `		  for( q = p; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){` |
|      45 |  187 | `			dv = dv * 8 + (ph7_real)(q[0] - '0');` |
|      23 |  188 | `		  }` |
|       3 |  189 | `		  *pReal = dv;` |
|       - |  190 | `		}` |
|       3 |  191 | `		return TRUE;` |
|       - |  192 | `	}` |
|       - |  193 | `	/* Decimal: overflow iff more than 19 significant digits, or exactly 19 that` |
|       - |  194 | `	 * compare greater than INT64_MAX. Defer the value to strtod (via the caller)` |
|       - |  195 | `	 * for php-exact rounding. */` |
|  472231 |  196 | `	p = z;` |
|  472231 |  197 | `	while( p < zEnd && p[0] == '0' ){ p++; }` |
| 1237942 |  198 | `	for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisDigit(q[0]); q++ ){ n++; }` |
|  472231 |  199 | `	if( n > 19 \|\| (n == 19 && SyMemcmp(p, "9223372036854775807", 19) > 0) ){` |
|      27 |  200 | `		*pbDecimal = TRUE;` |
|      27 |  201 | `		return TRUE;` |
|       - |  202 | `	}` |
|  472205 |  203 | `	return FALSE;` |
|  377825 |  204 | `}` |
|  772384 |  205 | `PH7_PRIVATE sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  206 | `{` |
|  772389 |  207 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|  772389 |  208 | `	sxu32 nIdx = 0;` |
|       - |  209 | `	char zScratch[GEN_NUM_SCRATCH];` |
|  772389 |  210 | `	char *zAlloc = 0;` |
|       - |  211 | `	SyString sNum;` |
|       - |  212 | `	sxi32 rc;` |
|  385647 |  213 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
| 1158036 |  214 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|  385647 |  215 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|  772389 |  216 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  217 | `		return SXERR_ABORT;` |
|       - |  218 | `	}` |
|  772389 |  219 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|       - |  220 | `		ph7_value *pObj;` |
|       - |  221 | `		sxi64 iValue;` |
|  756656 |  222 | `		ph7_real rOverflow = 0;` |
|  756656 |  223 | `		int bDecimalOverflow = 0;` |
|  756656 |  224 | `		if( GenStateIntLiteralOverflows(&sNum,&rOverflow,&bDecimalOverflow) ){` |
|       - |  225 | `			/* Literal exceeds the signed 64-bit range: PHP represents it as a` |
|       - |  226 | `			 * float instead of wrapping/dropping digits. */` |
|      37 |  227 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      37 |  228 | `			if( pObj == 0 ){` |
|     ! 0 |  229 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  230 | `				if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |  231 | `				return SXERR_ABORT;` |
|       - |  232 | `			}` |
|      37 |  233 | `			if( bDecimalOverflow ){` |
|       - |  234 | `				/* strtod on the decimal token yields php-exact rounding. */` |
|      27 |  235 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|      27 |  236 | `				PH7_MemObjToReal(pObj);` |
|      14 |  237 | `			}else{` |
|      11 |  238 | `				PH7_MemObjInitFromReal(pGen->pVm,pObj,rOverflow);` |
|       - |  239 | `			}` |
|      19 |  240 | `		}else{` |
|  756620 |  241 | `			iValue = PH7_TokenValueToInt64(&sNum);` |
|  756620 |  242 | `			pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|  756620 |  243 | `			if( pObj == 0 ){` |
|     ! 0 |  244 | `				if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |  245 | `				return SXERR_ABORT;` |
|       - |  246 | `			}` |
|  756620 |  247 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|       - |  248 | `		}` |
|  377795 |  249 | `	}else{` |
|       - |  250 | `		/* Real number */` |
|       - |  251 | `		ph7_value *pObj;` |
|       - |  252 | `		/* Reserve a new constant */` |
|   15738 |  253 | `		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   15738 |  254 | `		if( pObj == 0 ){` |
|     ! 0 |  255 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  256 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |  257 | `			return SXERR_ABORT;` |
|       - |  258 | `		}` |
|   15738 |  259 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|   15738 |  260 | `		PH7_MemObjToReal(pObj);` |
|       - |  261 | `	}` |
|  772389 |  262 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       - |  263 | `	/* Emit the load constant instruction */` |
|  772389 |  264 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  265 | `	/* Node successfully compiled */` |
|  772389 |  266 | `	return SXRET_OK;` |
|  385652 |  267 | `}` |
|       - |  268 | `/*` |
|       - |  269 | ` * Compile a single quoted string.` |
|       - |  270 | ` * According to the PHP language reference manual:` |
|       - |  271 | ` *` |
|       - |  272 | ` *   The simplest way to specify a string is to enclose it in single quotes (the character ' ).` |
|       - |  273 | ` *   To specify a literal single quote, escape it with a backslash (\). To specify a literal` |
|       - |  274 | ` *   backslash, double it (\\). All other instances of backslash will be treated as a literal` |
|       - |  275 | ` *   backslash: this means that the other escape sequences you might be used to, such as \r` |
|       - |  276 | ` *   or \n, will be output literally as specified rather than having any special meaning.` |
|       - |  277 | ` *` |
|       - |  278 | ` */` |
|  623519 |  279 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  280 | `{` |
|  623524 |  281 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|       - |  282 | `	const char *zIn,*zCur,*zEnd;` |
|       - |  283 | `	ph7_value *pObj;` |
|       - |  284 | `	sxu32 nIdx;` |
|       - |  285 | `	sxi32 bHasEsc;` |
|  623524 |  286 | `	nIdx = 0; /* Prevent compiler warning */` |
|       - |  287 | `	/* Delimit the string */` |
|  623524 |  288 | `	zIn  = pStr->zString;` |
|  623524 |  289 | `	zEnd = &zIn[pStr->nByte];` |
|  623524 |  290 | `	if( zIn >= zEnd ){` |
|       - |  291 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|       - |  292 | `		 * rather than reserving a new object each time. */` |
|   89249 |  293 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|   89249 |  294 | `		return SXRET_OK;` |
|       - |  295 | `	}` |
|       - |  296 | `	/* A single-quoted literal whose raw source holds a backslash unescapes to a` |
|       - |  297 | `	 * value that differs from that source (\\ -> \, \' -> '). The literal cache` |
|       - |  298 | `	 * keys FIND on the raw source text but INSTALL on the unescaped value, so` |
|       - |  299 | `	 * caching such a literal lets an unrelated one collide with it — e.g. '\\'` |
|       - |  300 | `	 * (raw \\, value \) would find the entry installed for '\\\\' (raw \\\\,` |
|       - |  301 | `	 * value \\) and load two backslashes. Only cache literals whose value equals` |
|       - |  302 | `	 * their source, i.e. those with no backslash to unescape. */` |
|  534280 |  303 | `	bHasEsc = 0;` |
|       - |  304 | `	{` |
|       - |  305 | `		const char *zScan;` |
| 9575126 |  306 | `		for( zScan = zIn ; zScan < zEnd ; zScan++ ){` |
| 9049043 |  307 | `			if( zScan[0] == '\\' ){ bHasEsc = 1; break; }` |
| 4510445 |  308 | `		}` |
|       - |  309 | `	}` |
|  534280 |  310 | `	if( !bHasEsc && SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|       - |  311 | `		/* Already processed,emit the load constant instruction` |
|       - |  312 | `		 * and return.` |
|       - |  313 | `		 */` |
|  247783 |  314 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|  247783 |  315 | `		return SXRET_OK;` |
|       - |  316 | `	}` |
|       - |  317 | `	/* Reserve a new constant */` |
|  286502 |  318 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  286502 |  319 | `	if( pObj == 0 ){` |
|     ! 0 |  320 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  321 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  322 | `		return SXERR_ABORT;` |
|       - |  323 | `	}` |
|  286502 |  324 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |  325 | `	/* Compile the node */` |
|  289891 |  326 | `	for(;;){` |
|  581444 |  327 | `		if( zIn >= zEnd ){` |
|       - |  328 | `			/* End of input */` |
|  286502 |  329 | `			break;` |
|       - |  330 | `		}` |
|  294947 |  331 | `		zCur = zIn;` |
| 8571665 |  332 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
| 8276723 |  333 | `			zIn++;` |
|       5 |  334 | `		}` |
|  294947 |  335 | `		if( zIn > zCur ){` |
|       - |  336 | `			/* Append raw contents*/` |
|  294438 |  337 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|  146800 |  338 | `		}` |
|  294947 |  339 | `		zIn++;` |
|  294947 |  340 | `		if( zIn < zEnd ){` |
|    8767 |  341 | `			if( zIn[0] == '\\' ){` |
|       - |  342 | `				/* A literal backslash */` |
|    7131 |  343 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|    5199 |  344 | `			}else if( zIn[0] == '\'' ){` |
|       - |  345 | `				/* A single quote */` |
|      77 |  346 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|      40 |  347 | `			}else{` |
|       - |  348 | `				/* verbatim copy */` |
|    1568 |  349 | `				zIn--;` |
|    1568 |  350 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char)*2);` |
|    1568 |  351 | `				zIn++;` |
|       - |  352 | `			}` |
|    4375 |  353 | `		}` |
|       - |  354 | `		/* Advance the stream cursor */` |
|  294947 |  355 | `		zIn++;` |
|       5 |  356 | `	}` |
|       - |  357 | `	/* Emit the load constant instruction */` |
|  286502 |  358 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|  286502 |  359 | `	if( !bHasEsc && pStr->nByte < 1024 ){` |
|       - |  360 | `		/* Install in the literal table (only when value == source; see above) */` |
|  278310 |  361 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|  138747 |  362 | `	}` |
|       - |  363 | `	/* Node successfully compiled */` |
|  286502 |  364 | `	return SXRET_OK;` |
|  311048 |  365 | `}` |
|       - |  366 | `/*` |
|       - |  367 | ` * PHP 7.3 flexible heredoc/nowdoc closing-marker indent stripping.` |
|       - |  368 | ` *` |
|       - |  369 | ` * When the lexer matched the closing marker with leading whitespace on its` |
|       - |  370 | ` * own line, it stored the indent count in pGen->pIn->pUserData. The marker's` |
|       - |  371 | ` * indent prefix bytes sit immediately after the stripped body (at` |
|       - |  372 | ` * pIn->sData.zString + pIn->sData.nByte + 1 for LF, +2 for CRLF) in the` |
|       - |  373 | ` * original source buffer — the buffer is stable through compilation.` |
|       - |  374 | ` *` |
|       - |  375 | `` * For each body line, we remove exactly `nIndent` leading bytes that must`` |
|       - |  376 | ` * byte-for-byte match the marker's prefix. Empty lines (0 bytes or bare \r)` |
|       - |  377 | ` * bypass validation. Mismatches raise the exact PHP 7.3+ parse errors:` |
|       - |  378 | ` *   - "Invalid body indentation level (expecting an indentation level of` |
|       - |  379 | ` *     at least N)" — line too short, or first differing byte is not` |
|       - |  380 | ` *     whitespace.` |
|       - |  381 | ` *   - "Invalid indentation - tabs and spaces cannot be mixed" — first` |
|       - |  382 | ` *     differing byte is whitespace but differs from the marker prefix.` |
|       - |  383 | ` */` |
|     136 |  384 | `static sxi32 GenStateStripHeredocIndent(ph7_gen_state *pGen, SyString *pOut)` |
|       5 |  385 | `{` |
|     141 |  386 | `	SyString *pIn = &pGen->pIn->sData;` |
|     141 |  387 | `	sxu32 nIndent = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - |  388 | `	const char *zPrefix;` |
|       - |  389 | `	const char *z, *zEnd;` |
|       - |  390 | `	char *zBuf, *zDst;` |
|     141 |  391 | `	if( nIndent == 0 ){` |
|       - |  392 | `		/* Legacy column-0 marker: zero-copy fast path */` |
|      95 |  393 | `		*pOut = *pIn;` |
|      95 |  394 | `		return SXRET_OK;` |
|       - |  395 | `	}` |
|       - |  396 | `	/* Recover the marker indent prefix from the original source buffer.` |
|       - |  397 | `	 * Skip the terminator the lexer stripped: one '\n' plus an optional` |
|       - |  398 | `	 * preceding '\r'. Note: when the body is empty (pIn->nByte == 0) the` |
|       - |  399 | `	 * lexer stripped nothing, so this offset is one byte past the true` |
|       - |  400 | `	 * marker-indent start. That is harmless — the strip loop below never` |
|       - |  401 | `	 * runs (z == zEnd), and zPrefix is never dereferenced. */` |
|      51 |  402 | `	zPrefix = pIn->zString + pIn->nByte;` |
|      51 |  403 | `	if( zPrefix[0] == '\r' && zPrefix[1] == '\n' ){` |
|     ! 0 |  404 | `		zPrefix += 2;` |
|     ! 0 |  405 | `	}else{` |
|      51 |  406 | `		zPrefix += 1;` |
|       - |  407 | `	}` |
|       - |  408 | `	/* Allocate scratch buffer sized to the original body (always enough). */` |
|      51 |  409 | `	zBuf = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator, pIn->nByte + 1);` |
|      51 |  410 | `	if( zBuf == 0 ){` |
|     ! 0 |  411 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  412 | `		return SXERR_ABORT;` |
|       - |  413 | `	}` |
|      51 |  414 | `	zDst = zBuf;` |
|      51 |  415 | `	z = pIn->zString;` |
|      51 |  416 | `	zEnd = z + pIn->nByte;` |
|     136 |  417 | `	while( z < zEnd ){` |
|      75 |  418 | `		const char *zLine = z;` |
|       - |  419 | `		sxu32 nLine;` |
|       - |  420 | `		int bEmpty;` |
|     817 |  421 | `		while( z < zEnd && z[0] != '\n' ){` |
|     747 |  422 | `			z++;` |
|       5 |  423 | `		}` |
|      75 |  424 | `		nLine = (sxu32)(z - zLine);` |
|      75 |  425 | `		bEmpty = (nLine == 0) \|\| (nLine == 1 && zLine[0] == '\r');` |
|      75 |  426 | `		if( !bEmpty ){` |
|       - |  427 | `			sxu32 i;` |
|      71 |  428 | `			if( nLine < nIndent ){` |
|     ! 0 |  429 | `				PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  430 | `					"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|     ! 0 |  431 | `					nIndent);` |
|     ! 0 |  432 | `				return SXERR_ABORT;` |
|       - |  433 | `			}` |
|     281 |  434 | `			for( i = 0; i < nIndent; i++ ){` |
|     223 |  435 | `				if( zLine[i] != zPrefix[i] ){` |
|      11 |  436 | `					unsigned char c = (unsigned char)zLine[i];` |
|      11 |  437 | `					if( c == ' ' \|\| c == '\t' ){` |
|       6 |  438 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  439 | `							"Invalid indentation - tabs and spaces cannot be mixed");` |
|       4 |  440 | `					}else{` |
|       8 |  441 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  442 | `							"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|       2 |  443 | `							nIndent);` |
|       - |  444 | `					}` |
|      11 |  445 | `					return SXERR_ABORT;` |
|       - |  446 | `				}` |
|     109 |  447 | `			}` |
|      60 |  448 | `			SyMemcpy((const void *)(zLine + nIndent), (void *)zDst, nLine - nIndent);` |
|      60 |  449 | `			zDst += nLine - nIndent;` |
|      34 |  450 | `		}else if( nLine == 1 ){` |
|       - |  451 | `			/* Preserve the stray '\r' on an otherwise empty line */` |
|     ! 0 |  452 | `			*zDst++ = '\r';` |
|     ! 0 |  453 | `		}` |
|      64 |  454 | `		if( z < zEnd ){` |
|      25 |  455 | `			*zDst++ = '\n';` |
|      25 |  456 | `			z++;` |
|      12 |  457 | `		}` |
|       2 |  458 | `	}` |
|      40 |  459 | `	pOut->zString = zBuf;` |
|      40 |  460 | `	pOut->nByte = (sxu32)(zDst - zBuf);` |
|      40 |  461 | `	return SXRET_OK;` |
|      73 |  462 | `}` |
|       - |  463 | `/*` |
|       - |  464 | ` * Compile a nowdoc string.` |
|       - |  465 | ` * According to the PHP language reference manual:` |
|       - |  466 | ` *` |
|       - |  467 | ` *  Nowdocs are to single-quoted strings what heredocs are to double-quoted strings.` |
|       - |  468 | ` *  A nowdoc is specified similarly to a heredoc, but no parsing is done inside a nowdoc.` |
|       - |  469 | ` *  The construct is ideal for embedding PHP code or other large blocks of text without the` |
|       - |  470 | ` *  need for escaping. It shares some features in common with the SGML <![CDATA[ ]]>` |
|       - |  471 | ` *  construct, in that it declares a block of text which is not for parsing.` |
|       - |  472 | ` *  A nowdoc is identified with the same <<< sequence used for heredocs, but the identifier` |
|       - |  473 | ` *  which follows is enclosed in single quotes, e.g. <<<'EOT'. All the rules for heredoc` |
|       - |  474 | ` *  identifiers also apply to nowdoc identifiers, especially those regarding the appearance` |
|       - |  475 | ` *  of the closing identifier.` |
|       - |  476 | ` */` |
|      56 |  477 | `PH7_PRIVATE sxi32 PH7_CompileNowDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  478 | `{` |
|       - |  479 | `	SyString sStripped;` |
|       - |  480 | `	SyString *pStr;` |
|       - |  481 | `	ph7_value *pObj;` |
|       - |  482 | `	sxu32 nIdx;` |
|       - |  483 | `	sxi32 rc;` |
|      61 |  484 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|      61 |  485 | `	if( rc != SXRET_OK ){` |
|       6 |  486 | `		return rc;` |
|       - |  487 | `	}` |
|      56 |  488 | `	pStr = &sStripped;` |
|      56 |  489 | `	nIdx = 0; /* Prevent compiler warning */` |
|      56 |  490 | `	if( pStr->nByte <= 0 ){` |
|       - |  491 | `		/* An empty nowdoc is the empty STRING, like '' -- loading NULL here made` |
|       - |  492 | `		 * strlen(<<<'EOD'EOD;) deprecation-warn about a null argument. */` |
|       7 |  493 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|       7 |  494 | `		return SXRET_OK;` |
|       - |  495 | `	}` |
|       - |  496 | `	/* Reserve a new constant */` |
|      50 |  497 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      50 |  498 | `	if( pObj == 0 ){` |
|     ! 0 |  499 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  500 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  501 | `		return SXERR_ABORT;` |
|       - |  502 | `	}` |
|       - |  503 | `	/* No processing is done here, simply a memcpy() operation */` |
|      50 |  504 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,pStr);` |
|       - |  505 | `	/* Emit the load constant instruction */` |
|      50 |  506 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  507 | `	/* Node successfully compiled */` |
|      50 |  508 | `	return SXRET_OK;` |
|      33 |  509 | `}` |
|       - |  510 | `/*` |
|       - |  511 | ` * Process variable expression [i.e: "$var","${var}"] embedded in a double quoted/heredoc string.` |
|       - |  512 | ` * According to the PHP language reference manual` |
|       - |  513 | ` *   When a string is specified in double quotes or with heredoc,variables are parsed within it.` |
|       - |  514 | ` *  There are two types of syntax: a simple one and a complex one. The simple syntax is the most` |
|       - |  515 | ` *  common and convenient. It provides a way to embed a variable, an array value, or an object` |
|       - |  516 | ` *  property in a string with a minimum of effort.` |
|       - |  517 | ` *  Simple syntax` |
|       - |  518 | ` *   If a dollar sign ($) is encountered, the parser will greedily take as many tokens as possible` |
|       - |  519 | ` *   to form a valid variable name. Enclose the variable name in curly braces to explicitly specify` |
|       - |  520 | ` *   the end of the name.` |
|       - |  521 | ` *   Similarly, an array index or an object property can be parsed. With array indices, the closing` |
|       - |  522 | ` *   square bracket (]) marks the end of the index. The same rules apply to object properties` |
|       - |  523 | ` *   as to simple variables.` |
|       - |  524 | ` *  Complex (curly) syntax` |
|       - |  525 | ` *   This isn't called complex because the syntax is complex, but because it allows for the use` |
|       - |  526 | ` *   of complex expressions.` |
|       - |  527 | ` *   Any scalar variable, array element or object property with a string representation can be` |
|       - |  528 | ` *   included via this syntax. Simply write the expression the same way as it would appear outside` |
|       - |  529 | ` *   the string, and then wrap it in { and }. Since { can not be escaped, this syntax will only` |
|       - |  530 | ` *   be recognised when the $ immediately follows the {. Use {\$ to get a literal {$` |
|       - |  531 | ` */` |
|    5789 |  532 | `static sxi32 GenStateProcessStringExpression(` |
|       - |  533 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  534 | `	sxu32 nLine,         /* Line number */` |
|       - |  535 | `	const char *zIn,     /* Raw expression */` |
|       - |  536 | `	const char *zEnd     /* End of the expression */` |
|       - |  537 | `	)` |
|       5 |  538 | `{` |
|       - |  539 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  540 | `	SySet sToken;` |
|       - |  541 | `	sxi32 rc;` |
|       - |  542 | `	/* Initialize the token set */` |
|    5794 |  543 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       - |  544 | `	/* Preallocate some slots */` |
|    5794 |  545 | `	SySetAlloc(&sToken,0x08);` |
|       - |  546 | `	/* Tokenize the text */` |
|    5794 |  547 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken,0);` |
|       - |  548 | `	/* Swap delimiter */` |
|    5794 |  549 | `	pTmpIn  = pGen->pIn;` |
|    5794 |  550 | `	pTmpEnd = pGen->pEnd;` |
|    5794 |  551 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|    5794 |  552 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       - |  553 | ``	/* Compile the expression. An interpolated `"...$x..."` READS $x — php warns`` |
|       - |  554 | `	 * "Undefined variable $x" and substitutes the empty string — so ask for a` |
|       - |  555 | `	 * read-only load rather than letting the default vivify it silently. */` |
|    5794 |  556 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|       - |  557 | `	/* Restore token stream */` |
|    5794 |  558 | `	pGen->pIn  = pTmpIn;` |
|    5794 |  559 | `	pGen->pEnd = pTmpEnd;` |
|       - |  560 | `	/* Release the token set */` |
|    5794 |  561 | `	SySetRelease(&sToken);` |
|       - |  562 | `	/* Compilation result */` |
|    5794 |  563 | `	return rc;` |
|       5 |  564 | `}` |
|       - |  565 | `/*` |
|       - |  566 | ` * Line number of a POSITION inside the string body being compiled: the` |
|       - |  567 | ` * token's line plus every newline before it. php reports the offending` |
|       - |  568 | ` * construct's own line, not the string's opening line, so every diagnostic` |
|       - |  569 | ` * raised from inside a body -- an escape sequence, a malformed subscript --` |
|       - |  570 | ` * goes through here. A heredoc body starts on the line after the '<<<'` |
|       - |  571 | ` * marker, hence the +1.` |
|       - |  572 | ` */` |
|      40 |  573 | `static sxu32 GenStateStringEscLine(ph7_gen_state *pGen,const char *zPos,int bHeredoc)` |
|       4 |  574 | `{` |
|      44 |  575 | `	const char *z = pGen->pIn->sData.zString;` |
|      44 |  576 | `	sxu32 nLine = pGen->pIn->nLine + (bHeredoc ? 1 : 0);` |
|     180 |  577 | `	for( ; z < zPos ; z++ ){` |
|     140 |  578 | `		if( z[0] == '\n' ){` |
|     ! 0 |  579 | `			nLine++;` |
|     ! 0 |  580 | `		}` |
|      72 |  581 | `	}` |
|      44 |  582 | `	return nLine;` |
|       4 |  583 | `}` |
|       - |  584 | `/*` |
|       - |  585 | ` * TRUE when c can OPEN a php label — the same byte class the engine's identifier` |
|       - |  586 | ` * scanner uses (LEX_LABEL_START in lex.c): [a-zA-Z_\x80-\xff].` |
|       - |  587 | ` */` |
|       - |  588 | `#define GEN_STRING_LABEL_START(c) \` |
|       - |  589 | `	( (unsigned char)(c) >= 0x80 \|\| SyisAlpha(c) \|\| (c) == '_' )` |
|       - |  590 | `/*` |
|       - |  591 | ` * Advance *pz over a php LABEL — the name half of "$name" and of the "->name"` |
|       - |  592 | ` * accessor inside a double-quoted string or a heredoc body. php's label is` |
|       - |  593 | ` * [a-zA-Z_\x80-\xff][a-zA-Z0-9_\x80-\xff]*, a flat byte set: a multibyte name` |
|       - |  594 | ` * is consumed because every byte of it is >= 0x80, no UTF-8 decoding involved.` |
|       - |  595 | ` * Stops at *pz when the cursor is not on a label byte.` |
|       - |  596 | ` */` |
|    5698 |  597 | `static void GenStateSkipStringLabel(const char **pz,const char *zEnd)` |
|       5 |  598 | `{` |
|    5703 |  599 | `	const char *zIn = *pz;` |
|   17307 |  600 | `	while( zIn < zEnd` |
|   23352 |  601 | `		&& ((unsigned char)zIn[0] >= 0x80 \|\| SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_') ){` |
|   17654 |  602 | `		zIn++;` |
|       5 |  603 | `	}` |
|    5703 |  604 | `	*pz = zIn;` |
|    5703 |  605 | `}` |
|       - |  606 | `/*` |
|       - |  607 | ` * Scan one php INTEGER literal at z in the flavour php's simple-syntax subscript` |
|       - |  608 | ` * accepts: LNUM, HNUM (0x...), BNUM (0b...) or ONUM (0o...), each allowing '_'` |
|       - |  609 | ` * separators BETWEEN digits. There is no float and no exponent in this grammar --` |
|       - |  610 | ` * "$a[1.5]" and "$a[1e2]" are php parse errors. Returns the byte after the` |
|       - |  611 | ` * literal, or z itself when the cursor is not on one.` |
|       - |  612 | ` */` |
|      70 |  613 | `static const char * GenStateScanOffsetNumber(const char *z,const char *zEnd)` |
|       1 |  614 | `{` |
|      71 |  615 | `	const char *zStart = z;` |
|      71 |  616 | `	int base = 10;` |
|      71 |  617 | `	if( z >= zEnd \|\| !GenStateIsBaseDigit((unsigned char)z[0],10) ){` |
|     ! 0 |  618 | `		return z;` |
|       - |  619 | `	}` |
|      71 |  620 | `	if( z[0] == '0' && &z[1] < zEnd ){` |
|      23 |  621 | `		int b = 0;` |
|      23 |  622 | `		if( z[1] == 'x' \|\| z[1] == 'X' ){` |
|       7 |  623 | `			b = 16;` |
|      20 |  624 | `		}else if( z[1] == 'b' \|\| z[1] == 'B' ){` |
|       3 |  625 | `			b = 2;` |
|      16 |  626 | `		}else if( z[1] == 'o' \|\| z[1] == 'O' ){` |
|       3 |  627 | `			b = 8;` |
|       1 |  628 | `		}` |
|       - |  629 | `		/* A prefix with no digit behind it is not a literal: php then matches the` |
|       - |  630 | `		 * lone "0" and lexes the rest as a label ("$a[0x]" is a parse error). */` |
|      23 |  631 | `		if( b && &z[2] < zEnd && GenStateIsBaseDigit((unsigned char)z[2],b) ){` |
|       9 |  632 | `			base = b;` |
|       9 |  633 | `			z += 2;` |
|       4 |  634 | `		}` |
|      11 |  635 | `	}` |
|     353 |  636 | `	while( z < zEnd ){` |
|     297 |  637 | `		if( GenStateIsBaseDigit((unsigned char)z[0],base) ){` |
|     279 |  638 | `			z++;` |
|     279 |  639 | `			continue;` |
|       - |  640 | `		}` |
|      18 |  641 | `		if( z[0] == '_' && z > zStart && GenStateIsBaseDigit((unsigned char)z[-1],base)` |
|       9 |  642 | `			&& &z[1] < zEnd && GenStateIsBaseDigit((unsigned char)z[1],base) ){` |
|       5 |  643 | `			z += 2;` |
|       5 |  644 | `			continue;` |
|       - |  645 | `		}` |
|      15 |  646 | `		break;` |
|     ! 0 |  647 | `	}` |
|      71 |  648 | `	return z;` |
|      36 |  649 | `}` |
|       - |  650 | `/*` |
|       - |  651 | ` * TRUE when the digit run [z,zEnd) is php's CANONICAL spelling of an INTEGER` |
|       - |  652 | ` * offset: "0", or [1-9][0-9]* that fits a signed 64-bit int. php carries every` |
|       - |  653 | ` * other spelling -- leading zeros, a base prefix, '_' separators, a magnitude` |
|       - |  654 | ` * past the int range -- as the raw TEXT, i.e. a STRING key. (zend also spells` |
|       - |  655 | ` * out any 19-digit run, but its hashmap folds that straight back to an integer` |
|       - |  656 | ` * key, so the two agree on everything an array can observe.)` |
|       - |  657 | ` */` |
|      56 |  658 | `static int GenStateOffsetIsCanonicalInt(const char *z,const char *zEnd,int bNeg)` |
|       1 |  659 | `{` |
|      57 |  660 | `	sxu32 n = (sxu32)(zEnd - z);` |
|       - |  661 | `	sxu32 i;` |
|      57 |  662 | `	if( n < 1 ){` |
|     ! 0 |  663 | `		return FALSE;` |
|       - |  664 | `	}` |
|      57 |  665 | `	if( z[0] == '0' ){` |
|       - |  666 | `		/* "0" alone is the integer key 0; "-0", "00" and "007" are text */` |
|      33 |  667 | `		return n == 1 && !bNeg;` |
|       - |  668 | `	}` |
|     231 |  669 | `	for( i = 0 ; i < n ; ++i ){` |
|     209 |  670 | `		if( !GenStateIsBaseDigit((unsigned char)z[i],10) ){` |
|       3 |  671 | `			return FALSE;` |
|       - |  672 | `		}` |
|     104 |  673 | `	}` |
|       - |  674 | `	/* INT64_MAX bounds BOTH signs here, not INT64_MIN: the rewrite re-emits a` |
|       - |  675 | `	 * canonical offset as SOURCE, and no php expression can spell INT64_MIN as a` |
|       - |  676 | `	 * literal (the '-' is unary minus over an out-of-range literal, which` |
|       - |  677 | `	 * promotes to a float). "-9223372036854775808" therefore takes the string` |
|       - |  678 | `	 * path, where the hashmap's numeric-string rule folds it back to the integer` |
|       - |  679 | `	 * key -- and where an ArrayAccess offsetGet() receives php's own string. */` |
|      23 |  680 | `	if( n > 19 \|\| (n == 19 && SyMemcmp(z,"9223372036854775807",19) > 0) ){` |
|       7 |  681 | `		return FALSE;` |
|       - |  682 | `	}` |
|      17 |  683 | `	return TRUE;` |
|      29 |  684 | `}` |
|       - |  685 | `/*` |
|       - |  686 | ` * php's parse error for a malformed simple-syntax subscript. zBad points at the` |
|       - |  687 | ` * first byte php would refuse; iExpect picks which of php's "expecting" tails` |
|       - |  688 | ` * applies -- 1 after an otherwise good offset, 2 after a lone '-', 0 at the` |
|       - |  689 | ` * offset's start. Always returns SXERR_ABORT so the caller can just pass it on.` |
|       - |  690 | ` */` |
|      34 |  691 | `static sxi32 GenStateOffsetSyntaxError(ph7_gen_state *pGen,const char *zBad,const char *zEnd,int iExpect,int bHeredoc)` |
|       1 |  692 | `{` |
|       - |  693 | `	SyString sTok;` |
|      35 |  694 | `	sxu32 n = (sxu32)(zEnd - zBad);` |
|      35 |  695 | `	if( n < 1 ){` |
|       - |  696 | `		/* Empty offset: name the ']' that zEnd points at */` |
|       3 |  697 | `		n = 1;` |
|       1 |  698 | `	}` |
|      35 |  699 | `	if( n > 16 ){` |
|     ! 0 |  700 | `		n = 16;` |
|     ! 0 |  701 | `	}` |
|      35 |  702 | `	SyStringInitFromBuf(&sTok,zBad,n);` |
|      35 |  703 | `	if( iExpect == 1 ){` |
|      19 |  704 | `		PH7_GenCompileError(&(*pGen),E_PARSE,GenStateStringEscLine(&(*pGen),zBad,bHeredoc),` |
|       - |  705 | `			"syntax error, unexpected token \"%z\", expecting \"]\"",&sTok);` |
|      26 |  706 | `	}else if( iExpect == 2 ){` |
|       5 |  707 | `		PH7_GenCompileError(&(*pGen),E_PARSE,GenStateStringEscLine(&(*pGen),zBad,bHeredoc),` |
|       - |  708 | `			"syntax error, unexpected token \"%z\", expecting number",&sTok);` |
|       3 |  709 | `	}else{` |
|      13 |  710 | `		PH7_GenCompileError(&(*pGen),E_PARSE,GenStateStringEscLine(&(*pGen),zBad,bHeredoc),` |
|       - |  711 | `			"syntax error, unexpected token \"%z\", expecting \"-\" or identifier or variable or number",&sTok);` |
|       - |  712 | `	}` |
|      35 |  713 | `	return SXERR_ABORT;` |
|       1 |  714 | `}` |
|       - |  715 | `/*` |
|       - |  716 | ` * Compile the SUBSCRIPT of a simple-syntax "$name[offset]" interpolation:` |
|       - |  717 | ` * [zKey,zKeyEnd) is the raw text between the brackets, and the php-equivalent` |
|       - |  718 | ` * "[...]" source is appended to pOut.` |
|       - |  719 | ` *` |
|       - |  720 | `` * php does NOT parse this as an expression. zend's `encaps_var_offset` grammar`` |
|       - |  721 | ` * admits exactly four things and nothing else -- a bare LABEL (always the STRING` |
|       - |  722 | ` * key, never a constant), an integer literal, '-' plus an integer literal, or a` |
|       - |  723 | ` * "$name" -- and only a canonical decimal is an INTEGER key. PH7 handed the text` |
|       - |  724 | ` * to the expression compiler, which read every integer SPELLING as a number and` |
|       - |  725 | ` * accepted shapes php rejects outright.` |
|       - |  726 | ` */` |
|     116 |  727 | `static sxi32 GenStateCompileStringOffset(` |
|       - |  728 | `	ph7_gen_state *pGen,` |
|       - |  729 | `	const char *zKey,` |
|       - |  730 | `	const char *zKeyEnd,` |
|       - |  731 | `	SyBlob *pOut,` |
|       - |  732 | `	int bHeredoc` |
|       - |  733 | `	)` |
|       3 |  734 | `{` |
|     119 |  735 | `	const char *z = zKey;` |
|     119 |  736 | `	int bNeg = 0;` |
|     119 |  737 | `	if( z >= zKeyEnd ){` |
|       3 |  738 | `		return GenStateOffsetSyntaxError(&(*pGen),z,zKeyEnd,0,bHeredoc);` |
|       - |  739 | `	}` |
|     117 |  740 | `	if( z[0] == '$' ){` |
|       - |  741 | `		/* "$name" -- the one offset php actually EVALUATES; pass it through */` |
|       9 |  742 | `		const char *zName = &z[1];` |
|       9 |  743 | `		if( zName >= zKeyEnd \|\| !GEN_STRING_LABEL_START(zName[0]) ){` |
|       3 |  744 | `			return GenStateOffsetSyntaxError(&(*pGen),z,zKeyEnd,0,bHeredoc);` |
|       - |  745 | `		}` |
|       7 |  746 | `		z = zName;` |
|       7 |  747 | `		GenStateSkipStringLabel(&z,zKeyEnd);` |
|       7 |  748 | `		if( z != zKeyEnd ){` |
|       3 |  749 | `			return GenStateOffsetSyntaxError(&(*pGen),z,zKeyEnd,1,bHeredoc);` |
|       - |  750 | `		}` |
|       5 |  751 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|       5 |  752 | `		SyBlobAppend(pOut,zKey,(sxu32)(zKeyEnd - zKey));` |
|       5 |  753 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|       5 |  754 | `		return SXRET_OK;` |
|       - |  755 | `	}` |
|     109 |  756 | `	if( z[0] == '-' ){` |
|      15 |  757 | `		bNeg = 1;` |
|      15 |  758 | `		z++;` |
|       7 |  759 | `	}` |
|     109 |  760 | `	if( z < zKeyEnd && GenStateIsBaseDigit((unsigned char)z[0],10) ){` |
|      71 |  761 | `		const char *zNum = z;` |
|      71 |  762 | `		z = GenStateScanOffsetNumber(z,zKeyEnd);` |
|      71 |  763 | `		if( z != zKeyEnd ){` |
|      15 |  764 | `			return GenStateOffsetSyntaxError(&(*pGen),z,zKeyEnd,1,bHeredoc);` |
|       - |  765 | `		}` |
|       - |  766 | `		/* "-0" is php's string key "-0", not the integer 0: zend negates a LONG` |
|       - |  767 | `		 * num-string but spells a ZERO one back out as text. */` |
|      57 |  768 | `		if( GenStateOffsetIsCanonicalInt(zNum,zKeyEnd,bNeg) ){` |
|      29 |  769 | `			SyBlobAppend(pOut,"[",sizeof(char));` |
|      29 |  770 | `			SyBlobAppend(pOut,zKey,(sxu32)(zKeyEnd - zKey));` |
|      29 |  771 | `			SyBlobAppend(pOut,"]",sizeof(char));` |
|      15 |  772 | `		}else{` |
|      29 |  773 | `			SyBlobAppend(pOut,"['",sizeof(char)*2);` |
|      29 |  774 | `			SyBlobAppend(pOut,zKey,(sxu32)(zKeyEnd - zKey));` |
|      29 |  775 | `			SyBlobAppend(pOut,"']",sizeof(char)*2);` |
|       - |  776 | `		}` |
|      57 |  777 | `		return SXRET_OK;` |
|       - |  778 | `	}` |
|      39 |  779 | `	if( bNeg ){` |
|       - |  780 | `		/* php's '-' takes a NUMBER and nothing else */` |
|       5 |  781 | `		return GenStateOffsetSyntaxError(&(*pGen),z,zKeyEnd,2,bHeredoc);` |
|       - |  782 | `	}` |
|      35 |  783 | `	if( GEN_STRING_LABEL_START(z[0]) ){` |
|      27 |  784 | `		GenStateSkipStringLabel(&z,zKeyEnd);` |
|      27 |  785 | `		if( z != zKeyEnd ){` |
|       3 |  786 | `			return GenStateOffsetSyntaxError(&(*pGen),z,zKeyEnd,1,bHeredoc);` |
|       - |  787 | `		}` |
|       - |  788 | `		/* A bare word is the STRING key, never a constant */` |
|      25 |  789 | `		SyBlobAppend(pOut,"['",sizeof(char)*2);` |
|      25 |  790 | `		SyBlobAppend(pOut,zKey,(sxu32)(zKeyEnd - zKey));` |
|      25 |  791 | `		SyBlobAppend(pOut,"']",sizeof(char)*2);` |
|      25 |  792 | `		return SXRET_OK;` |
|       - |  793 | `	}` |
|       9 |  794 | `	return GenStateOffsetSyntaxError(&(*pGen),z,zKeyEnd,0,bHeredoc);` |
|      61 |  795 | `}` |
|       - |  796 | `/*` |
|       - |  797 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|       - |  798 | ` */` |
|   77205 |  799 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|       5 |  800 | `{` |
|       - |  801 | `	ph7_value *pConstObj;` |
|   77210 |  802 | `	sxu32 nIdx = 0;` |
|       - |  803 | `	/* Reserve a new constant */` |
|   77210 |  804 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   77210 |  805 | `	if( pConstObj == 0 ){` |
|     ! 0 |  806 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  807 | `		return 0;` |
|       - |  808 | `	}` |
|   77210 |  809 | `	(*pCount)++;` |
|   77210 |  810 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|       - |  811 | `	/* Emit the load constant instruction */` |
|   77210 |  812 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   77210 |  813 | `	return pConstObj;` |
|   38506 |  814 | `}` |
|       - |  815 | `/*` |
|       - |  816 | ` * Compile a double quoted/heredoc string.` |
|       - |  817 | ` * According to the PHP language reference manual` |
|       - |  818 | ` * Heredoc` |
|       - |  819 | ` *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier` |
|       - |  820 | ` *  is provided, then a newline. The string itself follows, and then the same identifier again` |
|       - |  821 | ` *  to close the quotation.` |
|       - |  822 | ` *  The closing identifier must begin in the first column of the line. Also, the identifier must` |
|       - |  823 | ` *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric` |
|       - |  824 | ` *  characters and underscores, and must start with a non-digit character or underscore.` |
|       - |  825 | ` *  Warning` |
|       - |  826 | ` *  It is very important to note that the line with the closing identifier must contain` |
|       - |  827 | ` *  no other characters, except possibly a semicolon (;). That means especially that the identifier` |
|       - |  828 | ` *  may not be indented, and there may not be any spaces or tabs before or after the semicolon.` |
|       - |  829 | ` *  It's also important to realize that the first character before the closing identifier must` |
|       - |  830 | ` *  be a newline as defined by the local operating system. This is \n on UNIX systems, including Mac OS X.` |
|       - |  831 | ` *  The closing delimiter (possibly followed by a semicolon) must also be followed by a newline.` |
|       - |  832 | ` *  If this rule is broken and the closing identifier is not "clean", it will not be considered a closing` |
|       - |  833 | ` *  identifier, and PHP will continue looking for one. If a proper closing identifier is not found before` |
|       - |  834 | ` *  the end of the current file, a parse error will result at the last line.` |
|       - |  835 | ` *  Heredocs can not be used for initializing class properties.` |
|       - |  836 | ` * Double quoted` |
|       - |  837 | ` *  If the string is enclosed in double-quotes ("), PHP will interpret more escape sequences for special characters:` |
|       - |  838 | ` *  Escaped characters Sequence 	Meaning` |
|       - |  839 | ` *  \n linefeed (LF or 0x0A (10) in ASCII)` |
|       - |  840 | ` *  \r carriage return (CR or 0x0D (13) in ASCII)` |
|       - |  841 | ` *  \t horizontal tab (HT or 0x09 (9) in ASCII)` |
|       - |  842 | ` *  \v vertical tab (VT or 0x0B (11) in ASCII)` |
|       - |  843 | ` *  \e escape (ESC or 0x1B (27) in ASCII)` |
|       - |  844 | ` *  \f form feed (FF or 0x0C (12) in ASCII)` |
|       - |  845 | ` *  \\ backslash` |
|       - |  846 | ` *  \$ dollar sign` |
|       - |  847 | ` *  \" double-quote` |
|       - |  848 | ` *  \[0-7]{1,3} 	the sequence of characters matching the regular expression is a character in octal notation,` |
|       - |  849 | ` *      which silently overflows to fit in a byte (e.g. "\400" === "\000")` |
|       - |  850 | ` *  \x[0-9A-Fa-f]{1,2} 	the sequence of characters matching the regular expression is a character in hexadecimal notation` |
|       - |  851 | ` *  \u{[0-9A-Fa-f]+} 	the sequence of characters matching the regular expression is a Unicode codepoint,` |
|       - |  852 | ` *      which will be output to the string as that codepoint's UTF-8 representation` |
|       - |  853 | ` * As in single quoted strings, escaping any other character will result in the backslash being printed too.` |
|       - |  854 | ` * (The PH7-ism "\oNNN" octal form is gone: a literal "\o" now round-trips like php 8.)` |
|       - |  855 | ` * The most important feature of double-quoted strings is the fact that variable names will be expanded.` |
|       - |  856 | ` * See string parsing for details.` |
|       - |  857 | ` */` |
|       - |  858 | `/* bHeredoc: php strips the backslash from '\"' only when '"' is the active` |
|       - |  859 | ` * quote character; a heredoc has none, so '\"' stays verbatim there. */` |
|   75974 |  860 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen,int bHeredoc)` |
|       5 |  861 | `{` |
|   75979 |  862 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|       - |  863 | `	const char *zIn,*zCur,*zEnd;` |
|   75979 |  864 | `	ph7_value *pObj = 0;` |
|       - |  865 | `	sxi32 iCons;` |
|       - |  866 | `	sxi32 nInterp;   /* how many of iCons came from an interpolated EXPRESSION */` |
|       - |  867 | `	sxi32 rc;` |
|       - |  868 | `	/* Delimit the string */` |
|   75979 |  869 | `	zIn  = pStr->zString;` |
|   75979 |  870 | `	zEnd = &zIn[pStr->nByte];` |
|   75979 |  871 | `	if( zIn >= zEnd ){` |
|       - |  872 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|       - |  873 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|       - |  874 | `		 * literal table from growing when many "" literals appear in the source.` |
|       - |  875 | `		 */` |
|    2067 |  876 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|    2067 |  877 | `		return SXRET_OK;` |
|       - |  878 | `	}` |
|   73917 |  879 | `	zCur = 0;` |
|       - |  880 | `	/* Compile the node */` |
|   73917 |  881 | `	iCons = 0;` |
|   73917 |  882 | `	nInterp = 0;` |
|   39702 |  883 | `	for(;;){` |
|  119926 |  884 | `		zCur = zIn;` |
|  980627 |  885 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  866531 |  886 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|     126 |  887 | `				break;` |
|  866307 |  888 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|    5618 |  889 | `				(GEN_STRING_LABEL_START(zIn[1]) \|\| zIn[1] == '{') ){` |
|    2779 |  890 | `					break;` |
|       - |  891 | `			}` |
|  860706 |  892 | `			zIn++;` |
|       5 |  893 | `		}` |
|  119926 |  894 | `		if( zIn > zCur ){` |
|   58826 |  895 | `			if( pObj == 0 ){` |
|   55200 |  896 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|   55200 |  897 | `				if( pObj == 0 ){` |
|     ! 0 |  898 | `					return SXERR_ABORT;` |
|       - |  899 | `				}` |
|   27511 |  900 | `			}` |
|   58826 |  901 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   29312 |  902 | `		}` |
|  119926 |  903 | `		if( zIn >= zEnd ){` |
|   73879 |  904 | `			break;` |
|       - |  905 | `		}` |
|   46052 |  906 | `		if( zIn[0] == '\\' ){` |
|   40227 |  907 | `			const char *zPtr = 0;` |
|       - |  908 | `			sxu32 n;` |
|   40227 |  909 | `			zIn++;` |
|   40227 |  910 | `			if( pObj == 0 ){` |
|   22015 |  911 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|   22015 |  912 | `				if( pObj == 0 ){` |
|     ! 0 |  913 | `					return SXERR_ABORT;` |
|       - |  914 | `				}` |
|   10990 |  915 | `			}` |
|   40227 |  916 | `			if( zIn >= zEnd ){` |
|       - |  917 | `				/* Lone backslash at the very end of the body: php keeps it */` |
|       3 |  918 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|       3 |  919 | `				break;` |
|       - |  920 | `			}` |
|   40225 |  921 | `			n = sizeof(char); /* size of conversion */` |
|   40225 |  922 | `			switch( zIn[0] ){` |
|     173 |  923 | `			case '$':` |
|       - |  924 | `				/* Dollar sign */` |
|     351 |  925 | `				PH7_MemObjStringAppend(pObj,"$",sizeof(char));` |
|     351 |  926 | `				break;` |
|      97 |  927 | `			case '\\':` |
|       - |  928 | `				/* A literal backslash */` |
|     199 |  929 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|     199 |  930 | `				break;` |
|       1 |  931 | `			case 'e':` |
|       - |  932 | `				/* Escape (ESC) ASCII code 27 */` |
|       3 |  933 | `				PH7_MemObjStringAppend(pObj,"\x1b",sizeof(char));` |
|       3 |  934 | `				break;` |
|      11 |  935 | `			case 'f':` |
|       - |  936 | `				/* Form-feed (FF)[ctrl+l] ASCII code 12 */` |
|      23 |  937 | `				PH7_MemObjStringAppend(pObj,"\f",sizeof(char));` |
|      23 |  938 | `				break;` |
|   16636 |  939 | `			case 'n':` |
|       - |  940 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|   33170 |  941 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|   33170 |  942 | `				break;` |
|     832 |  943 | `			case 'r':` |
|       - |  944 | `				/* Carriage return (CR)[ctrl+m] ASCII code 13 */` |
|    1668 |  945 | `				PH7_MemObjStringAppend(pObj,"\r",sizeof(char));` |
|    1668 |  946 | `				break;` |
|      89 |  947 | `			case 't':` |
|       - |  948 | `				/* Horizontal tab (HT)[ctrl+i] ASCII code 9 */` |
|     182 |  949 | `				PH7_MemObjStringAppend(pObj,"\t",sizeof(char));` |
|     182 |  950 | `				break;` |
|      12 |  951 | `			case 'v':` |
|       - |  952 | `				/* Vertical tab(VT)[ctrl+k] ASCII code 11 */` |
|      25 |  953 | `				PH7_MemObjStringAppend(pObj,"\v",sizeof(char));` |
|      25 |  954 | `				break;` |
|     407 |  955 | `			case '"':` |
|     819 |  956 | `				if( bHeredoc ){` |
|       - |  957 | `					/* No active quote char in a heredoc: php keeps \" verbatim */` |
|       5 |  958 | `					PH7_MemObjStringAppend(pObj,"\\\"",sizeof(char)*2);` |
|       3 |  959 | `				}else{` |
|       - |  960 | `					/* Double quote */` |
|     815 |  961 | `					PH7_MemObjStringAppend(pObj,"\"",sizeof(char));` |
|       - |  962 | `				}` |
|     819 |  963 | `				break;` |
|     286 |  964 | `			case '0': case '1': case '2': case '3':` |
|       - |  965 | `			case '4': case '5': case '6': case '7': {` |
|       - |  966 | `				/* \[0-7]{1,3}: a character in octal notation. A value above \377` |
|       - |  967 | `				 * warns and wraps to the low byte, matching php 8. */` |
|     552 |  968 | `				int c = 0;` |
|       - |  969 | `				char cOut;` |
|    1181 |  970 | `				for( zPtr = zIn ; zPtr < &zIn[3*sizeof(char)] ; zPtr++ ){` |
|    1151 |  971 | `					if( zPtr >= zEnd \|\| zPtr[0] < '0' \|\| zPtr[0] > '7' ){` |
|     251 |  972 | `						break;` |
|       - |  973 | `					}` |
|     634 |  974 | `					c = c * 8 + (zPtr[0] - '0');` |
|     307 |  975 | `				}` |
|     552 |  976 | `				if( c > 0xFF ){` |
|       - |  977 | `					SyString sSeq;` |
|       3 |  978 | `					SyStringInitFromBuf(&sSeq,zIn,(sxu32)(zPtr-zIn));` |
|       3 |  979 | `					PH7_GenCompileError(&(*pGen),E_WARNING,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|       - |  980 | `						"Octal escape sequence overflow \\%z is greater than \\377",&sSeq);` |
|       3 |  981 | `					c &= 0xFF;` |
|       1 |  982 | `				}` |
|     552 |  983 | `				cOut = (char)c; /* value byte, independent of host endianness */` |
|     552 |  984 | `				PH7_MemObjStringAppend(pObj,&cOut,sizeof(char));` |
|     552 |  985 | `				n = (sxu32)(zPtr-zIn);` |
|     552 |  986 | `				break;` |
|       - |  987 | `			}` |
|    1513 |  988 | `			case 'x':` |
|    4540 |  989 | `				if( &zIn[1] < zEnd && SyisHex((unsigned char)zIn[1]) ){` |
|       - |  990 | `					/* \x[0-9A-Fa-f]{1,2}: a character in hexadecimal notation */` |
|    3025 |  991 | `					int c = SyHexToint(zIn[1]);` |
|       - |  992 | `					char cOut;` |
|    3025 |  993 | `					n += sizeof(char);` |
|    3025 |  994 | `					if( &zIn[2] < zEnd && SyisHex((unsigned char)zIn[2]) ){` |
|    3021 |  995 | `						c = (c << 4) + SyHexToint(zIn[2]);` |
|    3021 |  996 | `						n += sizeof(char);` |
|    1507 |  997 | `					}` |
|    3025 |  998 | `					cOut = (char)c; /* value byte, independent of host endianness */` |
|    3025 |  999 | `					PH7_MemObjStringAppend(pObj,&cOut,sizeof(char));` |
|    1514 | 1000 | `				}else{` |
|       - | 1001 | `					/* Not an escape: keep the backslash, as php does */` |
|       5 | 1002 | `					PH7_MemObjStringAppend(pObj,"\\x",sizeof(char)*2);` |
|       - | 1003 | `				}` |
|    3029 | 1004 | `				break;` |
|     107 | 1005 | `			case 'u':` |
|     212 | 1006 | `				if( &zIn[1] < zEnd && zIn[1] == '{'` |
|     315 | 1007 | `				 && !(&zIn[2] < zEnd && zIn[2] == '$') ){` |
|       - | 1008 | `					/* \u{codepoint}: UTF-8 encoding of the given codepoint (php 7+).` |
|       - | 1009 | `					 * php encodes surrogates verbatim, so the only invalid value` |
|       - | 1010 | `					 * is > U+10FFFF; malformed/empty braces are a compile error.` |
|       - | 1011 | `					 * "\u{$..." is excluded above: php treats it as a literal \u` |
|       - | 1012 | `					 * followed by {$...} curly interpolation. */` |
|     210 | 1013 | `					sxu32 nCp = 0;` |
|     210 | 1014 | `					zPtr = &zIn[2];` |
|     842 | 1015 | `					while( zPtr < zEnd && SyisHex((unsigned char)zPtr[0]) ){` |
|     636 | 1016 | `						if( nCp <= 0x10FFFF ){` |
|       - | 1017 | `							/* stop accumulating once out of range: keeps a long` |
|       - | 1018 | `							 * digit run from wrapping sxu32 */` |
|     636 | 1019 | `							nCp = nCp * 16 + (sxu32)SyHexToint(zPtr[0]);` |
|     313 | 1020 | `						}` |
|     636 | 1021 | `						zPtr++;` |
|       4 | 1022 | `					}` |
|     210 | 1023 | `					if( zPtr == &zIn[2] \|\| zPtr >= zEnd \|\| zPtr[0] != '}' ){` |
|       - | 1024 | `						/* Error recorded (nErr>0 fails the whole compile); consume the` |
|       - | 1025 | `						 * malformed sequence so later errors are still reported. */` |
|       3 | 1026 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|       - | 1027 | `							"Invalid UTF-8 codepoint escape sequence");` |
|       3 | 1028 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 1029 | `							return SXERR_ABORT;` |
|       - | 1030 | `						}` |
|       3 | 1031 | `						n = (sxu32)(zPtr-zIn);` |
|       3 | 1032 | `						if( zPtr < zEnd && zPtr[0] == '}' ){` |
|       3 | 1033 | `							n += sizeof(char);` |
|       1 | 1034 | `						}` |
|       3 | 1035 | `						break;` |
|       - | 1036 | `					}` |
|     208 | 1037 | `					n = (sxu32)(&zPtr[1]-zIn); /* 'u{...}' incl. closing brace */` |
|     208 | 1038 | `					if( nCp > 0x10FFFF ){` |
|       3 | 1039 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|       - | 1040 | `							"Invalid UTF-8 codepoint escape sequence: Codepoint too large");` |
|       3 | 1041 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 1042 | `							return SXERR_ABORT;` |
|       - | 1043 | `						}` |
|       3 | 1044 | `						break;` |
|       - | 1045 | `					}` |
|       - | 1046 | `					{` |
|       - | 1047 | `						char zUtf[4];` |
|     205 | 1048 | `						sxu8 *zOut = (sxu8 *)zUtf;` |
|     205 | 1049 | `						SX_WRITE_UTF8(zOut,nCp);` |
|     205 | 1050 | `						PH7_MemObjStringAppend(pObj,zUtf,(sxu32)(zOut-(sxu8 *)zUtf));` |
|       - | 1051 | `					}` |
|     103 | 1052 | `				}else{` |
|       - | 1053 | `					/* Not an escape: keep the backslash, as php does */` |
|       7 | 1054 | `					PH7_MemObjStringAppend(pObj,"\\u",sizeof(char)*2);` |
|       - | 1055 | `				}` |
|     211 | 1056 | `				break;` |
|      15 | 1057 | `			default:` |
|       - | 1058 | `				/* Unrecognized escape: keep the backslash, as php does.` |
|       - | 1059 | `				 * zIn[-1] is the backslash itself, so both bytes are contiguous` |
|       - | 1060 | `				 * in the source buffer — one batched append. */` |
|      31 | 1061 | `				PH7_MemObjStringAppend(pObj,&zIn[-1],sizeof(char)*2);` |
|      30 | 1062 | `				break;` |
|       - | 1063 | `			}` |
|       - | 1064 | `			/* Advance the stream cursor */` |
|   40225 | 1065 | `			zIn += n;` |
|   40225 | 1066 | `			continue;` |
|       - | 1067 | `		}` |
|    5830 | 1068 | `		if( zIn[0] == '{' ){` |
|       - | 1069 | `			/* Curly syntax */` |
|       - | 1070 | `			const char *zExpr;` |
|     248 | 1071 | `			sxi32 iNest = 1;` |
|     248 | 1072 | `			zIn++;` |
|     248 | 1073 | `			zExpr = zIn;` |
|       - | 1074 | `			/* Synchronize with the next closing curly braces */` |
|    2289 | 1075 | `			while( zIn < zEnd ){` |
|    2289 | 1076 | `				if( zIn[0] == '{' ){` |
|       - | 1077 | `					/* Increment nesting level */` |
|       3 | 1078 | `					iNest++;` |
|    2288 | 1079 | `				}else if(zIn[0] == '}' ){` |
|       - | 1080 | `					/* Decrement nesting level */` |
|     250 | 1081 | `					iNest--;` |
|     250 | 1082 | `					if( iNest <= 0 ){` |
|     248 | 1083 | `						break;` |
|       - | 1084 | `					}` |
|       1 | 1085 | `				}` |
|    2046 | 1086 | `				zIn++;` |
|       5 | 1087 | `			}` |
|       - | 1088 | `			/* Process the expression */` |
|     248 | 1089 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|     248 | 1090 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1091 | `				return SXERR_ABORT;` |
|       - | 1092 | `			}` |
|     248 | 1093 | `			if( rc != SXERR_EMPTY ){` |
|     248 | 1094 | `				++iCons;` |
|     248 | 1095 | `				++nInterp;` |
|     121 | 1096 | `			}` |
|     248 | 1097 | `			if( zIn < zEnd ){` |
|       - | 1098 | `				/* Jump the trailing curly */` |
|     248 | 1099 | `				zIn++;` |
|     121 | 1100 | `			}` |
|     126 | 1101 | `		}else{` |
|       - | 1102 | `			/*` |
|       - | 1103 | `			 * Simple syntax. php's simple "$var…" form is a LEXER rule, not an` |
|       - | 1104 | `			 * expression: it takes the variable name plus EXACTLY ONE accessor —` |
|       - | 1105 | `			 * "$var", "$var[offset]" or "$var->prop" — and stops there. Everything` |
|       - | 1106 | `			 * past that one accessor is literal text: a second subscript` |
|       - | 1107 | `			 * ("$o->p[0]" is the property then a literal "[0]"), a second arrow` |
|       - | 1108 | `			 * ("$o->p->q" is "$o->p" then a literal "->q"), any "::" at all` |
|       - | 1109 | `			 * ("$c::C" is the VALUE of $c then a literal "::C", never a class` |
|       - | 1110 | `			 * constant), and any "{…}" ("$x{'a'}" is $x then literal). Only the` |
|       - | 1111 | `			 * complex "{$expr}" form reaches those, and it is handled above.` |
|       - | 1112 | `			 *` |
|       - | 1113 | `			 * PHL used to loop here, greedily chaining accessors, so those four` |
|       - | 1114 | `			 * shapes silently answered something else than php on VALID source.` |
|       - | 1115 | `			 */` |
|    5587 | 1116 | `			const char *zExpr = zIn;` |
|    5587 | 1117 | `			int bSubscript = 0;` |
|       - | 1118 | `			/*` |
|       - | 1119 | `			 * "${...}" string interpolation (every form: ${name}, ${expr}, ${$x}) was` |
|       - | 1120 | `			 * DEPRECATED by php 8.2 in favor of the canonical "{$...}". PHL targets php's` |
|       - | 1121 | `			 * *non-deprecated* surface, so it is a hard parse error here — never silently` |
|       - | 1122 | `			 * rewritten. The canonical "{$var}" reaches this compiler by a different path` |
|       - | 1123 | `			 * and is unaffected. Checked before the scan: '{' is not an accessor, so the` |
|       - | 1124 | `			 * cursor would otherwise stop on the '$' and read the brace as literal text.` |
|       - | 1125 | `			 */` |
|    5587 | 1126 | `			if( &zIn[1] < zEnd && zIn[0] == '$' && zIn[1] == '{' ){` |
|       3 | 1127 | `				PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - | 1128 | `					"syntax error, \"${\" string interpolation was removed in php 8.2, use \"{$...}\" instead");` |
|       3 | 1129 | `				return SXERR_ABORT;` |
|       - | 1130 | `			}` |
|       - | 1131 | `			/* Jump leading dollars */` |
|   11165 | 1132 | `			while( zIn < zEnd && zIn[0] == '$' ){` |
|    5585 | 1133 | `				zIn++;` |
|       5 | 1134 | `			}` |
|       - | 1135 | `			/* Variable name */` |
|    5585 | 1136 | `			GenStateSkipStringLabel(&zIn,zEnd);` |
|       - | 1137 | `			/* …then at most ONE accessor */` |
|    5643 | 1138 | `			if( zIn < zEnd && zIn[0] == '[' ){` |
|     119 | 1139 | `				sxi32 iSquare = 1;` |
|     119 | 1140 | `				bSubscript = 1;` |
|     119 | 1141 | `				zIn++;` |
|     577 | 1142 | `				while( zIn < zEnd ){` |
|     575 | 1143 | `					if( zIn[0] == '[' ){` |
|       3 | 1144 | `						iSquare++;` |
|     574 | 1145 | `					}else if (zIn[0] == ']' ){` |
|     119 | 1146 | `						iSquare--;` |
|     119 | 1147 | `						if( iSquare <= 0 ){` |
|     117 | 1148 | `							break;` |
|       - | 1149 | `						}` |
|       1 | 1150 | `					}` |
|     461 | 1151 | `					zIn++;` |
|       3 | 1152 | `				}` |
|     119 | 1153 | `				if( zIn < zEnd ){` |
|     117 | 1154 | `					zIn++;` |
|      57 | 1155 | `				}` |
|    5525 | 1156 | `			}else if( &zIn[2] < zEnd && zIn[0] == '-' && zIn[1] == '>'` |
|     123 | 1157 | `				&& GEN_STRING_LABEL_START(zIn[2]) ){` |
|       - | 1158 | `				/* Member access operator '->'. php takes it only when a LABEL` |
|       - | 1159 | `				 * follows; with anything else -- a digit, a space, a '{', the end` |
|       - | 1160 | `				 * of the body -- the arrow is literal TEXT and the interpolation is` |
|       - | 1161 | `				 * just the variable. PHL swallowed the bare '->' and handed the` |
|       - | 1162 | `				 * compiler a dangling "$o->", fatalling` |
|       - | 1163 | `				 * "'->': Missing/Invalid member name" on source php RUNS. */` |
|      83 | 1164 | `				zIn += 2;` |
|      83 | 1165 | `				GenStateSkipStringLabel(&zIn,zEnd);` |
|    5427 | 1166 | `			}else if( &zIn[3] < zEnd && zIn[0] == '?' && zIn[1] == '-' && zIn[2] == '>'` |
|      15 | 1167 | `				&& GEN_STRING_LABEL_START(zIn[3]) ){` |
|       - | 1168 | `				/* php 8.0 gave the simple syntax the NULLSAFE arrow on the same terms` |
|       - | 1169 | `				 * as '->': exactly one property name, taken only when a LABEL follows.` |
|       - | 1170 | ``				 * Without it `"$o?->b"` cast the OBJECT to a string and appended four`` |
|       - | 1171 | `				 * literal bytes -- an uncatchable "could not be converted to string"` |
|       - | 1172 | `				 * on source php runs. */` |
|       9 | 1173 | `				zIn += 3;` |
|       9 | 1174 | `				GenStateSkipStringLabel(&zIn,zEnd);` |
|       4 | 1175 | `			}` |
|       - | 1176 | `			/*` |
|       - | 1177 | `			 * "$a[offset]" -- php parses a simple-syntax subscript with its OWN tiny` |
|       - | 1178 | ``			 * grammar (zend's `encaps_var_offset`), never as an expression, so rewrite`` |
|       - | 1179 | `			 * it into the equivalent php source and hand THAT to the compiler. PH7 fed` |
|       - | 1180 | `			 * the raw text straight in, which read every integer SPELLING as a number` |
|       - | 1181 | `			 * ("$a[007]" / "$a[0x1A]" / "$a[1_000]" / "$a[-0]" answered the integer` |
|       - | 1182 | `			 * keys 7/26/1000/0 where php reads the STRING keys "007"/"0x1A"/"1_000"/` |
|       - | 1183 | `			 * "-0"), read a non-ASCII bare word as a CONSTANT ("$a[\xc3\xa9]" raised` |
|       - | 1184 | `			 * "Undefined constant"), and quietly accepted every shape php rejects` |
|       - | 1185 | `			 * ("$a[ 0]", "$a[0 ]", "$a['x']", "$a[+1]", "$a[-$k]", "$a[[]", "$a[]").` |
|       - | 1186 | `			 */` |
|    5585 | 1187 | `			if( bSubscript ){` |
|     119 | 1188 | `				const char *zBr = zExpr;` |
|       - | 1189 | `				SyBlob sSub;` |
|     381 | 1190 | `				while( zBr < zIn && zBr[0] != '[' ){` |
|     265 | 1191 | `					zBr++;` |
|       3 | 1192 | `				}` |
|     119 | 1193 | `				if( zIn <= zBr \|\| zIn[-1] != ']' ){` |
|       - | 1194 | `					/* Unterminated: the body ended inside the brackets. php names the` |
|       - | 1195 | `					  * closing quote it reached instead; there is no offending TOKEN to` |
|       - | 1196 | `					  * quote here, and zIn is one past the body, so never read it. */` |
|     ! 0 | 1197 | `					PH7_GenCompileError(&(*pGen),E_PARSE,GenStateStringEscLine(&(*pGen),zBr,bHeredoc),` |
|       - | 1198 | `						"syntax error, unexpected end of string, expecting \"-\" or identifier or variable or number");` |
|     ! 0 | 1199 | `					return SXERR_ABORT;` |
|       - | 1200 | `				}` |
|     119 | 1201 | `				SyBlobInit(&sSub,&pGen->pVm->sAllocator);` |
|     119 | 1202 | `				SyBlobAppend(&sSub,zExpr,(sxu32)(zBr - zExpr));` |
|     119 | 1203 | `				rc = GenStateCompileStringOffset(&(*pGen),&zBr[1],&zIn[-1],&sSub,bHeredoc);` |
|     119 | 1204 | `				if( rc != SXRET_OK ){` |
|      35 | 1205 | `					SyBlobRelease(&sSub);` |
|      35 | 1206 | `					return SXERR_ABORT;` |
|       - | 1207 | `				}` |
|     126 | 1208 | `				rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,` |
|      82 | 1209 | `					(const char *)SyBlobData(&sSub),` |
|      82 | 1210 | `					(const char *)SyBlobData(&sSub) + SyBlobLength(&sSub));` |
|      85 | 1211 | `				SyBlobRelease(&sSub);` |
|      85 | 1212 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1213 | `					return SXERR_ABORT;` |
|       - | 1214 | `				}` |
|      85 | 1215 | `				if( rc != SXERR_EMPTY ){` |
|      85 | 1216 | `					++iCons;` |
|      85 | 1217 | `					++nInterp;` |
|      41 | 1218 | `				}` |
|      85 | 1219 | `				pObj = 0;` |
|      85 | 1220 | `				continue;` |
|       - | 1221 | `			}` |
|       - | 1222 | `			/* Process the expression */` |
|    5469 | 1223 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|    5469 | 1224 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1225 | `				return SXERR_ABORT;` |
|       - | 1226 | `			}` |
|    5469 | 1227 | `			if( rc != SXERR_EMPTY ){` |
|    5469 | 1228 | `				++iCons;` |
|    5469 | 1229 | `				++nInterp;` |
|    2715 | 1230 | `			}` |
|       - | 1231 | `		}` |
|       - | 1232 | `		/* Invalidate the previously used constant */` |
|    5712 | 1233 | `		pObj = 0;` |
|       5 | 1234 | `	}/*for(;;)*/` |
|   73881 | 1235 | `	if( iCons > 1 ){` |
|       - | 1236 | `		/* Concatenate all compiled constants */` |
|    4385 | 1237 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|   71678 | 1238 | `	}else if( iCons == 1 && nInterp == 1 ){` |
|       - | 1239 | `		/* A string that is nothing but one interpolation ("$x") still has to` |
|       - | 1240 | `		 * PRODUCE A STRING. With no CAT to force the conversion the operand was` |
|       - | 1241 | ``		 * left on the stack untouched, so `$s = "$x"` handed back $x's own type:`` |
|       - | 1242 | `		 * "$arr" stayed an array (and skipped php's "Array to string conversion"` |
|       - | 1243 | `		 * warning), "$int" stayed an int, "$res" stayed a resource. */` |
|      93 | 1244 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CVT_STR,0,0,0,0);` |
|      45 | 1245 | `	}` |
|       - | 1246 | `	/* Node successfully compiled */` |
|   73881 | 1247 | `	return SXRET_OK;` |
|   37902 | 1248 | `}` |
|       - | 1249 | `/*` |
|       - | 1250 | ` * Compile a double quoted string.` |
|       - | 1251 | ` *  See the block-comment above for more information.` |
|       - | 1252 | ` */` |
|   75898 | 1253 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 | 1254 | `{` |
|       - | 1255 | `	sxi32 rc;` |
|   75903 | 1256 | `	rc = GenStateCompileString(&(*pGen),0/*bHeredoc*/);` |
|   37859 | 1257 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - | 1258 | `	/* Compilation result */` |
|   75903 | 1259 | `	return rc;` |
|       5 | 1260 | `}` |
|       - | 1261 | `/*` |
|       - | 1262 | ` * Compile a Heredoc string.` |
|       - | 1263 | ` *  See the block-comment above for more information.` |
|       - | 1264 | ` */` |
|      80 | 1265 | `PH7_PRIVATE sxi32 PH7_CompileHereDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 | 1266 | `{` |
|       - | 1267 | `	SyString sOrig, sStripped;` |
|       - | 1268 | `	sxi32 rc;` |
|      85 | 1269 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|      85 | 1270 | `	if( rc != SXRET_OK ){` |
|       6 | 1271 | `		return rc;` |
|       - | 1272 | `	}` |
|       - | 1273 | `	/* Temporarily swap in the dedented body so GenStateCompileString` |
|       - | 1274 | `	 * (which reads pGen->pIn->sData directly) sees the stripped content.` |
|       - | 1275 | `	 * Restore before returning so downstream code that references pIn is` |
|       - | 1276 | `	 * unaffected, including on the error path. */` |
|      80 | 1277 | `	sOrig = pGen->pIn->sData;` |
|      80 | 1278 | `	pGen->pIn->sData = sStripped;` |
|      80 | 1279 | `	rc = GenStateCompileString(&(*pGen),1/*bHeredoc*/);` |
|      80 | 1280 | `	pGen->pIn->sData = sOrig;` |
|      38 | 1281 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|      80 | 1282 | `	return rc;` |
|      45 | 1283 | `}` |
|       - | 1284 | `/*` |
|       - | 1285 | ` * Compile an array entry whether it is a key or a value.` |
|       - | 1286 | ` *  Notes on array entries.` |
|       - | 1287 | ` *  According to the PHP language reference manual` |
|       - | 1288 | ` *  An array can be created by the array() language construct.` |
|       - | 1289 | ` *  It takes as parameters any number of comma-separated key => value pairs.` |
|       - | 1290 | ` *  array(  key =>  value` |
|       - | 1291 | ` *    , ...` |
|       - | 1292 | ` *    )` |
|       - | 1293 | ` *  A key may be either an integer or a string. If a key is the standard representation` |
|       - | 1294 | ` *  of an integer, it will be interpreted as such (i.e. "8" will be interpreted as 8, while` |
|       - | 1295 | ` *  "08" will be interpreted as "08"). Floats in key are truncated to integer.` |
|       - | 1296 | ` *  The indexed and associative array types are the same type in PHP, which can both` |
|       - | 1297 | ` *  contain integer and string indices.` |
|       - | 1298 | ` *  A value can be any PHP type.` |
|       - | 1299 | ` *  If a key is not specified for a value, the maximum of the integer indices is taken` |
|       - | 1300 | ` *  and the new key will be that value plus 1. If a key that already has an assigned value` |
|       - | 1301 | ` *  is specified, that value will be overwritten.` |
|       - | 1302 | ` */` |
|  154021 | 1303 | `PH7_PRIVATE sxi32 GenStateCompileArrayEntry(` |
|       - | 1304 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 1305 | `	SyToken *pIn,        /* Token stream */` |
|       - | 1306 | `	SyToken *pEnd,       /* End of the token stream */` |
|       - | 1307 | `	sxi32 iFlags,        /* Compilation flags */` |
|       - | 1308 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *) /* Expression tree validator callback */` |
|       - | 1309 | `	)` |
|       5 | 1310 | `{` |
|       - | 1311 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - | 1312 | `	sxi32 rc;` |
|       - | 1313 | `	/* Swap token stream */` |
|  154026 | 1314 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|       - | 1315 | `	/* Compile the expression*/` |
|  154026 | 1316 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|       - | 1317 | `	/* Restore token stream */` |
|  154026 | 1318 | `	RE_SWAP_DELIMITER(pGen);` |
|  154026 | 1319 | `	return rc;` |
|       5 | 1320 | `}` |
|       - | 1321 | `/*` |
|       - | 1322 | ` * Expression tree validator callback for the 'array' language construct.` |
|       - | 1323 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|       - | 1324 | ` * an invalid expression tree and this function will generate the appropriate` |
|       - | 1325 | ` * error message.` |
|       - | 1326 | ` * See the routine responible of compiling the array language construct` |
|       - | 1327 | ` * for more inforation.` |
|       - | 1328 | ` */` |
|      84 | 1329 | `static sxi32 GenStateArrayNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 | 1330 | `{` |
|       - | 1331 | ``	/* `array(&$x)` is a full write target in php, call included: `[&f()]` is its`` |
|       - | 1332 | `	 * "Can't use function return value in write context", not the` |
|       - | 1333 | ``	 * reference-returning-function exemption `$r =& f()` gets. A nullsafe chain`` |
|       - | 1334 | `	 * here takes the WRITE wording too, not the reference one — php never asks` |
|       - | 1335 | ``	 * `zend_assert_not_short_circuited` on an array entry. */`` |
|       - | 1336 | `	sxi32 rc;` |
|      84 | 1337 | `	if( pRoot && pRoot->pOp && pRoot->pOp->iOp == EXPR_OP_SUBSCRIPT` |
|      19 | 1338 | `	 && SySetUsed(&pRoot->aNodeArgs) < 1 ){` |
|       - | 1339 | ``		/* `[&$a[]]`: an APPEND has nothing to take a reference OF, and php asks`` |
|       - | 1340 | ``		 * that before anything else about the base — so `[&f()[]]` is this and not`` |
|       - | 1341 | `		 * the call refusal. PHL ran the whole thing. */` |
|     ! 0 | 1342 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,` |
|     ! 0 | 1343 | `			pRoot->pStart ? pRoot->pStart->nLine : 0,"Cannot use [] for reading");` |
|     ! 0 | 1344 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - | 1345 | `	}` |
|      89 | 1346 | `	rc = GenStateWriteTargetCheck(&(*pGen),pRoot,0);` |
|      89 | 1347 | `	if( rc != SXRET_OK ){` |
|       3 | 1348 | `		return rc;` |
|       - | 1349 | `	}` |
|      87 | 1350 | `	if( pRoot->pOp ){` |
|      20 | 1351 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ &&` |
|      14 | 1352 | `			pRoot->pOp->iOp != EXPR_OP_FUNC_CALL /* function() [Symisc extension: i.e: array(&foo())] */` |
|      19 | 1353 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|       - | 1354 | `			/* Unexpected expression */` |
|      14 | 1355 | `			rc = PH7_GenSyntaxError(&(*pGen),pRoot->pStart,"\"->\" or \"?->\" or \"[\"");` |
|      14 | 1356 | `			if( rc != SXERR_ABORT ){` |
|      14 | 1357 | `				rc = SXERR_INVALID;` |
|       5 | 1358 | `			}` |
|      10 | 1359 | `		}` |
|      74 | 1360 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 1361 | `		/* Unexpected expression */` |
|       3 | 1362 | `		rc = PH7_GenSyntaxError(&(*pGen),pRoot->pStart,0);` |
|       3 | 1363 | `		if( rc != SXERR_ABORT ){` |
|       3 | 1364 | `			rc = SXERR_INVALID;` |
|       1 | 1365 | `		}` |
|       1 | 1366 | `	}` |
|      87 | 1367 | `	return rc;` |
|      47 | 1368 | `}` |
|       - | 1369 | `/*` |
|       - | 1370 | ` * Find the top-level '=>' (PH7_TK_ARRAY_OP) that separates an array/list entry's` |
|       - | 1371 | ` * key from its value within [pStart,pEnd). The scan skips any '=>' nested inside` |
|       - | 1372 | ` * brackets/parens/braces, inside an arrow-function signature (fn(...) =>), or` |
|       - | 1373 | ` * inside a match() {...} arm — none of which are key/value separators. Returns a` |
|       - | 1374 | ` * pointer to the '=>' token, or pEnd if the entry has no top-level separator.` |
|       - | 1375 | ` */` |
|  215963 | 1376 | `PH7_PRIVATE SyToken * GenStateFindTopLevelArrow(SyToken *pStart,SyToken *pEnd)` |
|       5 | 1377 | `{` |
|  215968 | 1378 | `	SyToken *pCur = pStart;` |
|  215968 | 1379 | `	sxi32 iNest = 0;` |
|       - | 1380 | ``	/* An entry that STARTS with a bare `yield` owns the FIRST '=>' after it:`` |
|       - | 1381 | `` 	 * php's grammar gives `yield expr => expr` to the yield, so `[yield 1 => 2]` `` |
|       - | 1382 | `	 * is ONE element (the yield's own result) keyed by nothing, and the generator` |
|       - | 1383 | `	 * yields key 1 value 2. Reading that '=>' as the entry separator instead built` |
|       - | 1384 | ``	 * `[(yield 1) => 2]` — a different array AND a different yielded pair, in`` |
|       - | 1385 | `	 * silence. A SECOND top-level '=>' is the entry separator again, which is what` |
|       - | 1386 | ``	 * makes `[yield 1 => 2 => 3]` parse. `yield from` takes an iterable and never a`` |
|       - | 1387 | `	 * pair, so its entry keeps the ordinary rule. */` |
|  216183 | 1388 | `	int bYieldOwnsArrow = (pStart < pEnd) && (pStart->nType & PH7_TK_KEYWORD)` |
|  107976 | 1389 | `		&& (sxu32)SX_PTR_TO_INT(pStart->pUserData) == PH7_TKWRD_YIELD` |
|  324167 | 1390 | `		&& !(&pStart[1] < pEnd && (pStart[1].nType & PH7_TK_ID)` |
|       8 | 1391 | `			&& pStart[1].sData.nByte == 4` |
|       2 | 1392 | `			&& SyStrnicmp(pStart[1].sData.zString, "from", 4) == 0);` |
|  540444 | 1393 | `	while( pCur < pEnd ){` |
|  349186 | 1394 | `		if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|   24470 | 1395 | `			if( bYieldOwnsArrow ){` |
|       9 | 1396 | `				bYieldOwnsArrow = 0;` |
|       9 | 1397 | `				pCur++;` |
|       9 | 1398 | `				continue;` |
|       - | 1399 | `			}` |
|   24462 | 1400 | `			return pCur;` |
|       - | 1401 | `		}` |
|       - | 1402 | `		/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|       - | 1403 | `		 * The '=>' inside an arrow function introduces the expression body,` |
|       - | 1404 | `		 * not an entry separator. Skip past the signature.` |
|       - | 1405 | `		 */` |
|  324721 | 1406 | `		if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
|     569 | 1407 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|     569 | 1408 | `			SyToken *pFn = pCur;` |
|       - | 1409 | ``			/* Only a real `[static] fn[&](` opens an arrow function; `$fn`,`` |
|       - | 1410 | ``			 * `C::fn` and friends are plain names whose '=>' IS the separator. */`` |
|     569 | 1411 | `			if( PH7_TokenOpensArrowFunc(pStart,pCur,pEnd) ){` |
|     252 | 1412 | `				if( nKw == PH7_TKWRD_STATIC ){` |
|     ! 0 | 1413 | `					pFn = &pCur[1];` |
|     ! 0 | 1414 | `				}` |
|     252 | 1415 | `				pCur = pFn + 1; /* past 'fn' */` |
|     252 | 1416 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_AMPER) ){` |
|     ! 0 | 1417 | `					pCur++;` |
|     ! 0 | 1418 | `				}` |
|     252 | 1419 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|     252 | 1420 | `					pCur++;` |
|     252 | 1421 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|       - | 1422 | `						PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|     252 | 1423 | `					if( pCur < pEnd ){` |
|     252 | 1424 | `						pCur++;` |
|     124 | 1425 | `					}` |
|     124 | 1426 | `				}` |
|     252 | 1427 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_COLON) ){` |
|     ! 0 | 1428 | `					pCur++;` |
|     ! 0 | 1429 | `					if( pCur < pEnd && (pCur->nType & PH7_TK_OP)` |
|     ! 0 | 1430 | `						&& pCur->sData.nByte == 1` |
|     ! 0 | 1431 | `						&& pCur->sData.zString[0] == '?' ){` |
|     ! 0 | 1432 | `						pCur++;` |
|     ! 0 | 1433 | `					}` |
|     ! 0 | 1434 | `					if( pCur < pEnd` |
|     ! 0 | 1435 | `						&& (pCur->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|     ! 0 | 1436 | `						pCur++;` |
|     ! 0 | 1437 | `					}` |
|     ! 0 | 1438 | `				}` |
|       - | 1439 | `				/* The rest of the entry is the arrow-function body — no outer` |
|       - | 1440 | `				 * key to extract. */` |
|     252 | 1441 | `				return pEnd;` |
|       - | 1442 | `			}` |
|       - | 1443 | `			/* Match expression (PHP 8.0): the '=>' inside match arms is not an` |
|       - | 1444 | `			 * entry separator. Skip past the full match span. */` |
|     321 | 1445 | `			if( nKw == PH7_TKWRD_MATCH ){` |
|      12 | 1446 | `				pCur++; /* past 'match' */` |
|      12 | 1447 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|      10 | 1448 | `					pCur++;` |
|      10 | 1449 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|       - | 1450 | `						PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|      10 | 1451 | `					if( pCur < pEnd ){` |
|      10 | 1452 | `						pCur++;` |
|       4 | 1453 | `					}` |
|       4 | 1454 | `				}` |
|      12 | 1455 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_OCB) ){` |
|      10 | 1456 | `					pCur++;` |
|      10 | 1457 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|       - | 1458 | `						PH7_TK_OCB,PH7_TK_CCB,&pCur);` |
|      10 | 1459 | `					if( pCur < pEnd ){` |
|      10 | 1460 | `						pCur++;` |
|       4 | 1461 | `					}` |
|       4 | 1462 | `				}` |
|      12 | 1463 | `				continue;` |
|       - | 1464 | `			}` |
|     153 | 1465 | `		}` |
|  324463 | 1466 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|    7251 | 1467 | `			iNest++;` |
|  320813 | 1468 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/) ){` |
|       - | 1469 | `			/* Don't worry about mismatched brackets here, the expression` |
|       - | 1470 | `			 * parser will shortly detect any syntax error. */` |
|    7251 | 1471 | `			iNest--;` |
|    3596 | 1472 | `		}` |
|  324463 | 1473 | `		pCur++;` |
|       5 | 1474 | `	}` |
|  191263 | 1475 | `	return pEnd;` |
|  107766 | 1476 | `}` |
|       - | 1477 | `/*` |
|       - | 1478 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|       - | 1479 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|       - | 1480 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|       - | 1481 | ` */` |
|  100739 | 1482 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|       5 | 1483 | `{` |
|       - | 1484 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|       - | 1485 | `	SyToken *pKey,*pCur;` |
|  100744 | 1486 | `	sxi32 iEmitRef = 0;` |
|  100744 | 1487 | `	sxi32 iSpread = 0;` |
|  100744 | 1488 | `	sxi32 nPair = 0;` |
|       - | 1489 | `	sxi32 rc;` |
|  100744 | 1490 | `	xValidator = 0;` |
|  121558 | 1491 | `	for(;;){` |
|       - | 1492 | `		/* Jump leading commas. Exactly ONE separates two entries; a second one (or a comma` |
|       - | 1493 | `		 * at the very start) means an EMPTY element, which php rejects outright — PH7 just` |
|       - | 1494 | ``		 * skipped them, so `array(,)` and `array(1,,2)` compiled silently. A TRAILING comma`` |
|       - | 1495 | `		 * is legal and is handled by the loop exiting on the next pass. */` |
|   71616 | 1496 | `		{` |
|  243650 | 1497 | `			int nSkip = 0;` |
|  355940 | 1498 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|  112295 | 1499 | `				nSkip++;` |
|  112295 | 1500 | `				pGen->pIn++;` |
|       5 | 1501 | `			}` |
|  243650 | 1502 | `			if( nSkip > 1 \|\| (nSkip > 0 && nPair < 1) ){` |
|     ! 0 | 1503 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn[-1].nLine,` |
|       - | 1504 | `					"Cannot use empty array elements in arrays");` |
|     ! 0 | 1505 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1506 | `					return SXERR_ABORT;` |
|       - | 1507 | `				}` |
|     ! 0 | 1508 | `				return SXRET_OK;` |
|       - | 1509 | `			}` |
|       - | 1510 | `		}` |
|  243650 | 1511 | `		pCur = pGen->pIn;` |
|  243650 | 1512 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|       - | 1513 | `			/* No more entry to process */` |
|  100726 | 1514 | `			break;` |
|       - | 1515 | `		}` |
|  142929 | 1516 | `		if( pCur >= pGen->pIn ){` |
|     ! 0 | 1517 | `			continue;` |
|       - | 1518 | `		}` |
|       - | 1519 | `		/* Compile the key if available */` |
|  142929 | 1520 | `		pKey = pCur;` |
|  142929 | 1521 | `		pCur = GenStateFindTopLevelArrow(pCur,pGen->pIn);` |
|  142929 | 1522 | `		rc = SXERR_EMPTY;` |
|  142929 | 1523 | `		if( pCur < pGen->pIn ){` |
|    9940 | 1524 | `			if( pKey == pCur ){` |
|       - | 1525 | ``				/* `array( => 2)`: the entry STARTS with '=>', so it has no key. php rejects`` |
|       - | 1526 | `				 * it; PH7 warned about a "Missing entry key" and compiled on, accepting` |
|       - | 1527 | ``				 * source php refuses. (The `else if` below could never see this: the arrow`` |
|       - | 1528 | `				 * IS found here, so control never reached it.)` |
|       - | 1529 | `				 * php names the literal's own closer, so short syntax expects ']'. */` |
|       3 | 1530 | `				const char *zClose = (pGen->pEnd && (pGen->pEnd->nType & PH7_TK_CSB))` |
|       - | 1531 | `					? "\"]\"" : "\")\"";` |
|       3 | 1532 | `				rc = PH7_GenSyntaxError(&(*pGen),pCur,zClose);` |
|       3 | 1533 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1534 | `					return SXERR_ABORT;` |
|       - | 1535 | `				}` |
|       3 | 1536 | `				return SXRET_OK;` |
|       - | 1537 | `			}` |
|    9938 | 1538 | `			if( &pCur[1] >= pGen->pIn ){` |
|       - | 1539 | ``				/* `array(1 => )`: php names the token that SHOULD have started the value —`` |
|       - | 1540 | `				 * the ')' or ']' closing the literal — not the '=>' it just read. Passing 0` |
|       - | 1541 | `				 * makes the helper reach for the token past this entry's slice. */` |
|      14 | 1542 | `				rc = PH7_GenSyntaxError(&(*pGen),0,0);` |
|      14 | 1543 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1544 | `					return SXERR_ABORT;` |
|       - | 1545 | `				}` |
|      14 | 1546 | `				return SXRET_OK;` |
|       - | 1547 | `			}` |
|       - | 1548 | `			/* Compile the expression holding the key */` |
|    9928 | 1549 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|       - | 1550 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|    9928 | 1551 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1552 | `				return SXERR_ABORT;` |
|       - | 1553 | `			}` |
|    9928 | 1554 | `			pCur++; /* Jump the '=>' operator */` |
|    4943 | 1555 | `		}else{` |
|       - | 1556 | `			/* Reset back the cursor and point to the entry value */` |
|  132994 | 1557 | `			pCur = pKey;` |
|       - | 1558 | `		}` |
|  142917 | 1559 | `		if( rc == SXERR_EMPTY ){` |
|       - | 1560 | `			/* No key given: load the nil, TAGGED so LOAD_MAP knows this is an absent key` |
|       - | 1561 | ``			 * (auto-index) rather than an explicit `null =>` one, which php deprecates. */`` |
|  132994 | 1562 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,PH7_LOADC_NOKEY,0 /* nil index */,0,0);` |
|   66355 | 1563 | `		}` |
|  142917 | 1564 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
|       - | 1565 | `			/* Insertion by reference, [i.e: $a = array(&$x);] */` |
|      93 | 1566 | `			xValidator = GenStateArrayNodeValidator; /* Only variable are allowed */` |
|      93 | 1567 | `			iEmitRef = 1;` |
|      93 | 1568 | `			pCur++; /* Jump the '&' token */` |
|      93 | 1569 | `			if( pCur >= pGen->pIn ){` |
|       - | 1570 | `				/* Missing value */` |
|       - | 1571 | ``				/* php reports the token that actually stopped it (`array(&)` -> the`` |
|       - | 1572 | `				 * ')'), not a hand-written "missing referenced variable" fatal. */` |
|       3 | 1573 | `				rc = PH7_GenSyntaxError(&(*pGen),pCur < pGen->pIn ? pCur : 0,0);` |
|       3 | 1574 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1575 | `					return SXERR_ABORT;` |
|       - | 1576 | `				}` |
|       3 | 1577 | `				return SXRET_OK;` |
|       - | 1578 | `			}` |
|      43 | 1579 | `		}` |
|       - | 1580 | `		/* Detect array unpack: '...$expr' as the entry value (PHP 7.4+, with` |
|       - | 1581 | `		 * string-key support since PHP 8.1). The parser strips the '...' inside` |
|       - | 1582 | `		 * ExprExtractNode; we only need to know it's there so we can emit` |
|       - | 1583 | `		 * PH7_OP_FLAG_SPREAD after the value, instructing LOAD_MAP to merge the` |
|       - | 1584 | `		 * resulting hashmap rather than insert it as a scalar entry. */` |
|  142915 | 1585 | `		iSpread = (pCur < pGen->pIn && (pCur->nType & PH7_TK_ELLIPSIS)) ? 1 : 0;` |
|  142915 | 1586 | `		if( iSpread && (rc != SXERR_EMPTY \|\| iEmitRef) ){` |
|       - | 1587 | `			/* '[k => ...$a]' and '[&...$a]' are syntax errors in PHP — the` |
|       - | 1588 | `			 * '...' token cannot follow either '=>' or '&' inside an array` |
|       - | 1589 | `			 * literal. Emit the same Parse-error wording PHP uses so the` |
|       - | 1590 | `			 * output is engine-portable. */` |
|       6 | 1591 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pCur->nLine,` |
|       - | 1592 | `				"syntax error, unexpected token \"...\"");` |
|       6 | 1593 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1594 | `				return SXERR_ABORT;` |
|       - | 1595 | `			}` |
|       6 | 1596 | `			return SXRET_OK;` |
|       - | 1597 | `		}` |
|       - | 1598 | ``		/* Compile indice value. A BY-REF element (`'k' => &$a[$i]`) is an`` |
|       - | 1599 | `		 * lvalue: php VIVIFIES a missing subscript when a reference is taken,` |
|       - | 1600 | `		 * so compile it in write context (LOAD_IDX iP2=1, create-if-missing)` |
|       - | 1601 | `		 * instead of a read-only load — which also keeps the undefined-key` |
|       - | 1602 | `		 * warning (a read-only diagnostic) from false-firing here. A missing` |
|       - | 1603 | ``		 * PROPERTY (`[&$o->p]`) is created the same way (EXPR_FLAG_MEMBER_REFSRC). */`` |
|  214201 | 1604 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,` |
|   71290 | 1605 | `			iEmitRef ? (EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_REFSRC)` |
|       - | 1606 | `			         : EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,` |
|   71290 | 1607 | `			xValidator);` |
|  142911 | 1608 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1609 | `			return SXERR_ABORT;` |
|       - | 1610 | `		}` |
|  142911 | 1611 | `		if( iSpread ){` |
|       - | 1612 | `			/* Mark the value on TOS as a spread source; LOAD_MAP merges it. */` |
|      96 | 1613 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FLAG_SPREAD,0,0,0,0);` |
|  142865 | 1614 | `		}else if( iEmitRef ){` |
|       - | 1615 | `			/* Emit the load reference instruction */` |
|      89 | 1616 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|      42 | 1617 | `		}` |
|  142911 | 1618 | `		xValidator = 0;` |
|  142911 | 1619 | `		iEmitRef = 0;` |
|  142911 | 1620 | `		iSpread = 0;` |
|  142911 | 1621 | `		nPair++;` |
|       5 | 1622 | `	}` |
|       - | 1623 | `	/* Emit the load map instruction */` |
|  100726 | 1624 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|       - | 1625 | `	/* Node successfully compiled */` |
|  100726 | 1626 | `	return SXRET_OK;` |
|   50273 | 1627 | `}` |
|       - | 1628 | `/*` |
|       - | 1629 | ` * Compile the 'array' language construct.` |
|       - | 1630 | ` *	 According to the PHP language reference manual` |
|       - | 1631 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|       - | 1632 | ` *   values to keys. This type is optimized for several different uses; it can` |
|       - | 1633 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|       - | 1634 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|       - | 1635 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|       - | 1636 | ` */` |
|   83152 | 1637 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 | 1638 | `{` |
|       - | 1639 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   83157 | 1640 | `	pGen->pIn += 2;` |
|   83157 | 1641 | `	pGen->pEnd--;` |
|   41522 | 1642 | `	SXUNUSED(iCompileFlag);` |
|       - | 1643 | ``	/* php: a stray token in an `array( ... )` element is `... expecting ")"`. */`` |
|       - | 1644 | `	{` |
|   83157 | 1645 | `		const char *zSave = pGen->zClauseCloser;` |
|       - | 1646 | `		sxi32 rc;` |
|   83157 | 1647 | `		pGen->zClauseCloser = "\")\"";` |
|   83157 | 1648 | `		rc = GenStateCompileArrayBody(pGen);` |
|   83157 | 1649 | `		pGen->zClauseCloser = zSave;` |
|   83157 | 1650 | `		return rc;` |
|       - | 1651 | `	}` |
|       5 | 1652 | `}` |
|       - | 1653 | `/*` |
|       - | 1654 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|       - | 1655 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|       - | 1656 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|       - | 1657 | ` */` |
|   17587 | 1658 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 | 1659 | `{` |
|       - | 1660 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|   17592 | 1661 | `	pGen->pIn++;` |
|   17592 | 1662 | `	pGen->pEnd--;` |
|    8746 | 1663 | `	SXUNUSED(iCompileFlag);` |
|       - | 1664 | ``	/* php: a stray token in a `[ ... ]` element is `... expecting "]"`. */`` |
|       - | 1665 | `	{` |
|   17592 | 1666 | `		const char *zSave = pGen->zClauseCloser;` |
|       - | 1667 | `		sxi32 rc;` |
|   17592 | 1668 | `		pGen->zClauseCloser = "\"]\"";` |
|   17592 | 1669 | `		rc = GenStateCompileArrayBody(pGen);` |
|   17592 | 1670 | `		pGen->zClauseCloser = zSave;` |
|   17592 | 1671 | `		return rc;` |
|       - | 1672 | `	}` |
|       5 | 1673 | `}` |
|       - | 1674 | `/*` |
|       - | 1675 | ` * Expression tree validator callback for the 'list' language construct.` |
|       - | 1676 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|       - | 1677 | ` * an invalid expression tree and this function will generate the appropriate` |
|       - | 1678 | ` * error message.` |
|       - | 1679 | ` * See the routine responible of compiling the list language construct` |
|       - | 1680 | ` * for more inforation.` |
|       - | 1681 | ` */` |
|    1054 | 1682 | `static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 | 1683 | `{` |
|    1059 | 1684 | `	sxi32 rc = GenStateWriteTargetCheck(&(*pGen),pRoot,0);` |
|    1059 | 1685 | `	if( rc != SXRET_OK ){` |
|       3 | 1686 | `		return rc;` |
|       - | 1687 | `	}` |
|    1057 | 1688 | `	if( pRoot->pOp ){` |
|      70 | 1689 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */` |
|      35 | 1690 | `			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){` |
|       - | 1691 | `				/* Unexpected expression */` |
|     ! 0 | 1692 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 1693 | `					"Assignments can only happen to writable values");` |
|     ! 0 | 1694 | `				if( rc != SXERR_ABORT ){` |
|     ! 0 | 1695 | `					rc = SXERR_INVALID;` |
|     ! 0 | 1696 | `				}` |
|       2 | 1697 | `		}` |
|    1022 | 1698 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 1699 | `		/* Unexpected expression */` |
|       6 | 1700 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 1701 | `			"Assignments can only happen to writable values");` |
|       6 | 1702 | `		if( rc != SXERR_ABORT ){` |
|       6 | 1703 | `			rc = SXERR_INVALID;` |
|       2 | 1704 | `		}` |
|       2 | 1705 | `	}` |
|    1057 | 1706 | `	return rc;` |
|     531 | 1707 | `}` |
|       - | 1708 | `/*` |
|       - | 1709 | ` * Compile the 'list' language construct.` |
|       - | 1710 | ` *  According to the PHP language reference` |
|       - | 1711 | ` *  list(): Assign variables as if they were an array.` |
|       - | 1712 | ` *  list() is used to assign a list of variables in one operation.` |
|       - | 1713 | ` *  Description` |
|       - | 1714 | ` *   array list (mixed $varname [, mixed $... ] )` |
|       - | 1715 | ` *   Like array(), this is not really a function, but a language construct.` |
|       - | 1716 | ` *   list() is used to assign a list of variables in one operation.` |
|       - | 1717 | ` *  Parameters` |
|       - | 1718 | ` *   $varname: A variable.` |
|       - | 1719 | ` *  Return Values` |
|       - | 1720 | ` *   The assigned array.` |
|       - | 1721 | ` */` |
|       - | 1722 | `/*` |
|       - | 1723 | ` * TRUE when a destructuring target list binds at least one element BY REFERENCE` |
|       - | 1724 | `` * (`[&$a]`, `list('k' => &$a)`, at any nesting depth). A `&` that OPENS an entry`` |
|       - | 1725 | ` * is the marker -- it can only follow the list's own delimiter, a comma or a` |
|       - | 1726 | `` * `=>`, everywhere else the token is the bitwise operator.`` |
|       - | 1727 | ` *` |
|       - | 1728 | ` * Three places ask. The list body settles its entries one at a time and in source` |
|       - | 1729 | ` * ORDER when the answer is yes; the assignment reads its SOURCE in write context` |
|       - | 1730 | `` * (php's BP_VAR_W), which is what keeps `[&$t] = $undef;` silent about what it is`` |
|       - | 1731 | ` * creating; and foreach fetches the ROW rather than a copy of it, so` |
|       - | 1732 | `` * `foreach ($e as [&$x]) { $x *= 10; }` changes $e.`` |
|       - | 1733 | ` */` |
|    1557 | 1734 | `PH7_PRIVATE int PH7_GenStateListSpanHasRef(SyToken *pStart,SyToken *pEnd)` |
|       5 | 1735 | `{` |
|       - | 1736 | `	SyToken *pTok;` |
|    7751 | 1737 | `	for( pTok = pStart ; pTok < pEnd ; pTok++ ){` |
|    6248 | 1738 | `		if( (pTok->nType & PH7_TK_AMPER) == 0 \|\| pTok == pStart ){` |
|    6190 | 1739 | `			continue;` |
|       - | 1740 | `		}` |
|      59 | 1741 | `		if( pTok[-1].nType & (PH7_TK_OSB\|PH7_TK_LPAREN\|PH7_TK_COMMA) ){` |
|      55 | 1742 | `			return 1;` |
|       - | 1743 | `		}` |
|       4 | 1744 | `		if( (pTok[-1].nType & PH7_TK_OP) && pTok[-1].sData.nByte == 2` |
|       1 | 1745 | `		 && SyMemcmp((const void *)pTok[-1].sData.zString,(const void *)"=>",2) == 0 ){` |
|     ! 0 | 1746 | `			return 1;` |
|       - | 1747 | `		}` |
|       3 | 1748 | `	}` |
|    1508 | 1749 | `	return 0;` |
|     782 | 1750 | `}` |
|       - | 1751 | `/* Nested list entry recorded during first pass of list body compilation */` |
|       - | 1752 | `struct NestedListEntry {` |
|       - | 1753 | `	sxi32 nIndex;        /* Position in the outer list (0-based) */` |
|       - | 1754 | `	SyToken *pStart;     /* Token range: start of nested construct */` |
|       - | 1755 | `	SyToken *pEnd;       /* Token range: past closing delimiter */` |
|       - | 1756 | `	sxi32 isShort;       /* 1 if [...] form, 0 if list(...) form */` |
|       - | 1757 | `};` |
|       - | 1758 | `/*` |
|       - | 1759 | `` * TRUE when one destructuring ENTRY binds by reference -- itself (`&$t`) or`` |
|       - | 1760 | `` * anywhere below it (`[[&$t]]`). php fetches such a position for WRITE, which is`` |
|       - | 1761 | `` * what makes a non-array source its `Cannot use a scalar value as an array` Error`` |
|       - | 1762 | ` * rather than the read's warning-and-NULL.` |
|       - | 1763 | ` */` |
|    1032 | 1764 | `static int GenStateEntryBindsRef(SyToken *pStart,SyToken *pEnd)` |
|       5 | 1765 | `{` |
|    1037 | 1766 | `	if( pStart >= pEnd ){` |
|     ! 0 | 1767 | `		return 0;` |
|       - | 1768 | `	}` |
|    1037 | 1769 | `	if( pStart->nType & PH7_TK_AMPER/*'&'*/ ){` |
|      49 | 1770 | `		return 1;` |
|       - | 1771 | `	}` |
|     989 | 1772 | `	return PH7_GenStateListSpanHasRef(pStart,pEnd);` |
|     520 | 1773 | `}` |
|       - | 1774 | `/*` |
|       - | 1775 | ` * Give a destructuring SOURCE a name of its own, bound to it BY REFERENCE, and` |
|       - | 1776 | ` * answer that name. Every element is then fetched through the name.` |
|       - | 1777 | ` *` |
|       - | 1778 | ` * Reading the elements off the stack value instead cannot work once an entry` |
|       - | 1779 | ` * binds by reference: each fetch would be one more stack copy of the same array,` |
|       - | 1780 | ` * and a write-context fetch through a copy COW-separates -- so the second bind of` |
|       - | 1781 | `` * `[&$x, &$z] = $e` would land in a duplicate and never reach $e. Through a name`` |
|       - | 1782 | `` * each fetch is the `$t =& $e[k]` php compiles it as. A source with no slot`` |
|       - | 1783 | `` * behind it (a call result) gets the fresh variable php's `=&` gives one.`` |
|       - | 1784 | ` *` |
|       - | 1785 | ` * The source stays on the stack, where the caller's teardown expects it.` |
|       - | 1786 | ` */` |
|      54 | 1787 | `static sxi32 GenStateNameListSource(ph7_gen_state *pGen,SyString *pOut)` |
|       1 | 1788 | `{` |
|       - | 1789 | `	static int iListSrcCnt = 0;` |
|       - | 1790 | `	char zTmp[64];` |
|       - | 1791 | `	sxu32 nLen;` |
|       - | 1792 | `	char *zDup;` |
|      55 | 1793 | `	nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__list_src_%d__]",iListSrcCnt++);` |
|      55 | 1794 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|      55 | 1795 | `	if( zDup == 0 ){` |
|     ! 0 | 1796 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn ? pGen->pIn->nLine : 0,` |
|       - | 1797 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1798 | `		return SXERR_ABORT;` |
|       - | 1799 | `	}` |
|      55 | 1800 | `	SyStringInitFromBuf(pOut,zDup,nLen);` |
|       - | 1801 | `	/* STORE_REF binds the name and leaves the source where it was. */` |
|      55 | 1802 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_STORE_REF,0,0,zDup,0);` |
|      55 | 1803 | `	return SXRET_OK;` |
|      28 | 1804 | `}` |
|       - | 1805 | `/* Drop that borrowed name; the targets keep whatever they bound through it. */` |
|      54 | 1806 | `static sxi32 GenStateDropListSourceName(ph7_gen_state *pGen,SyString *pName)` |
|       1 | 1807 | `{` |
|      55 | 1808 | `	SyString *pDup = (SyString *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(SyString));` |
|      55 | 1809 | `	if( pDup == 0 ){` |
|     ! 0 | 1810 | `		PH7_GenCompileError(&(*pGen),E_ERROR,0,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1811 | `		return SXERR_ABORT;` |
|       - | 1812 | `	}` |
|      55 | 1813 | `	*pDup = *pName;` |
|      55 | 1814 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_UNSET_VAR,0,0,(void *)pDup,0);` |
|      55 | 1815 | `	return SXRET_OK;` |
|      28 | 1816 | `}` |
|       - | 1817 | `/*` |
|       - | 1818 | ` * Store the SOURCE element already on the stack top into one destructuring` |
|       - | 1819 | ` * target: compiling the target appends its lvalue load, which folds into a` |
|       - | 1820 | ` * STORE exactly as an ordinary assignment's does, and the assigned value is` |
|       - | 1821 | ` * dropped so the source array is back on top for the next entry.` |
|       - | 1822 | ` */` |
|      68 | 1823 | `static sxi32 GenStateEmitListValueStore(ph7_gen_state *pGen,SyToken *pTarget,SyToken *pEnd)` |
|       3 | 1824 | `{` |
|       - | 1825 | `	VmInstr *pInstr;` |
|      71 | 1826 | `	sxi32 iVmOp = PH7_OP_STORE;` |
|      71 | 1827 | `	sxi32 iP1 = 0,iP2 = 0;` |
|      71 | 1828 | `	void *p3 = 0;` |
|       - | 1829 | `	sxi32 rc;` |
|      71 | 1830 | `	rc = GenStateCompileArrayEntry(&(*pGen),pTarget,pEnd,` |
|       - | 1831 | `		EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|      71 | 1832 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1833 | `		return rc;` |
|       - | 1834 | `	}` |
|      71 | 1835 | `	if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|      71 | 1836 | `		if( pInstr->iOp == PH7_OP_MEMBER ){` |
|       6 | 1837 | `			iP2 = 1; /* member store: keep MEMBER, store the value below it */` |
|      68 | 1838 | `		}else if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       3 | 1839 | `			iVmOp = PH7_OP_STORE_IDX;` |
|       3 | 1840 | `			iP1 = pInstr->iP1;` |
|       3 | 1841 | `			(void)PH7_VmPopInstr(pGen->pVm);` |
|       2 | 1842 | `		}else{` |
|      64 | 1843 | `			p3 = pInstr->p3; /* named store: $v = value */` |
|      64 | 1844 | `			(void)PH7_VmPopInstr(pGen->pVm);` |
|       - | 1845 | `		}` |
|      34 | 1846 | `	}` |
|      71 | 1847 | `	PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|      71 | 1848 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      71 | 1849 | `	return SXRET_OK;` |
|      37 | 1850 | `}` |
|       - | 1851 | `/*` |
|       - | 1852 | ` * Bind one destructuring target to its SOURCE element BY REFERENCE, which is` |
|       - | 1853 | `` * exactly what php compiles `[&$t] = $src` into: `$t =& $src[k]`. The source`` |
|       - | 1854 | ` * element is already on the stack top -- the caller pushed the source, the key and` |
|       - | 1855 | ``  * a WRITE-context OP_LOAD_IDX, so a missing key vivifies silently the way a `=&` `` |
|       - | 1856 | ` * into one does -- and this compiles the target and folds its own load into the` |
|       - | 1857 | `` * STORE_REF, the same three shapes the `=&` operator folds.`` |
|       - | 1858 | ` */` |
|      52 | 1859 | `static sxi32 GenStateEmitListRefBind(ph7_gen_state *pGen,SyToken *pTarget,SyToken *pEnd)` |
|       1 | 1860 | `{` |
|       - | 1861 | `	VmInstr *pInstr;` |
|      53 | 1862 | `	sxi32 iVmOp = PH7_OP_STORE_REF,iP1 = 0,iP2 = 0;` |
|      53 | 1863 | `	void *p3 = 0;` |
|       - | 1864 | `	sxi32 rc;` |
|      53 | 1865 | `	if( pGen->bListSrcNotRef ){` |
|       - | 1866 | `		/* php asks this at COMPILE time, where it still knows how the right-hand` |
|       - | 1867 | `		 * side was written: a value with no slot behind it cannot be referenced,` |
|       - | 1868 | `		 * and the whole statement is refused rather than binding to a temporary. */` |
|      14 | 1869 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTarget->nLine,` |
|       - | 1870 | `			"Cannot assign reference to non referenceable value");` |
|      14 | 1871 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_INVALID;` |
|       - | 1872 | `	}` |
|      39 | 1873 | `	rc = GenStateCompileArrayEntry(&(*pGen),pTarget,pEnd,EXPR_FLAG_LOAD_IDX_STORE,` |
|       - | 1874 | `		GenStateListNodeValidator);` |
|      39 | 1875 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1876 | `		return rc;` |
|       - | 1877 | `	}` |
|      39 | 1878 | `	pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      39 | 1879 | `	if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 1880 | `		/* A property target keeps its OP_MEMBER: the VM resolves and stashes the` |
|       - | 1881 | ``		 * slot there, exactly as `$o->p =& $x` does. */`` |
|       3 | 1882 | `		pInstr->iP2 = PH7_MEMBER_REF_TARGET;` |
|       3 | 1883 | `		iP2 = 1;` |
|      38 | 1884 | `	}else if( (pInstr = PH7_VmPopInstr(pGen->pVm)) != 0 ){` |
|      37 | 1885 | `		if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       3 | 1886 | `			iVmOp = PH7_OP_STORE_IDX_REF;` |
|       3 | 1887 | `			iP1 = pInstr->iP1;` |
|       3 | 1888 | `			iP2 = pInstr->iP2;` |
|       3 | 1889 | `			p3  = pInstr->p3;` |
|       2 | 1890 | `		}else{` |
|      35 | 1891 | `			p3 = pInstr->p3;` |
|       - | 1892 | `		}` |
|      18 | 1893 | `	}` |
|      39 | 1894 | `	PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|       - | 1895 | `	/* The bind leaves the source element behind; drop it so the source array is` |
|       - | 1896 | `	 * back on the stack top for the next entry. */` |
|      39 | 1897 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      39 | 1898 | `	return SXRET_OK;` |
|      27 | 1899 | `}` |
|       - | 1900 | `/*` |
|       - | 1901 | ` * Compile the body of a *keyed* list/short-list destructuring (PHP 7.1), where` |
|       - | 1902 | `` * every entry has the form `keyExpr => target`. The source array is on the stack`` |
|       - | 1903 | ` * top on entry and remains there on exit, mirroring the positional LOAD_LIST` |
|       - | 1904 | ` * path so the caller's teardown is unchanged. For each entry: DUP the source,` |
|       - | 1905 | ` * push the key, LOAD_IDX to fetch source[key] (NULL on a missing key, silently,` |
|       - | 1906 | ` * like a normal subscript read), then assign the fetched value to the target — a` |
|       - | 1907 | ` * nested [...]/list() recurses, a simple lvalue uses the same STORE fold as a` |
|       - | 1908 | ` * normal assignment (the value sits below the lvalue-load, exactly as in` |
|       - | 1909 | ` * GenStateEmitExprCode where the assignment RHS precedes the LHS load).` |
|       - | 1910 | ` */` |
|      50 | 1911 | `static sxi32 GenStateCompileKeyedListBody(ph7_gen_state *pGen)` |
|       3 | 1912 | `{` |
|      53 | 1913 | `	SyString sTmp = { 0, 0 };` |
|       - | 1914 | `	SyToken *pNext,*pScanIn;` |
|      53 | 1915 | `	int bAnyRef = 0;` |
|       - | 1916 | `	sxi32 rc;` |
|       - | 1917 | `	/* Does any entry bind BY REFERENCE? If so the source is fetched through a NAME` |
|       - | 1918 | `	 * (GenStateNameListSource) rather than off the stack, for the reason recorded` |
|       - | 1919 | `	 * there. */` |
|      53 | 1920 | `	pScanIn = pGen->pIn;` |
|     115 | 1921 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|      65 | 1922 | `		SyToken *pArrow = GenStateFindTopLevelArrow(pGen->pIn,pNext);` |
|      65 | 1923 | `		if( pArrow < pNext && &pArrow[1] < pNext && (pArrow[1].nType & PH7_TK_AMPER/*'&'*/) ){` |
|       5 | 1924 | `			bAnyRef = 1;` |
|       2 | 1925 | `		}` |
|      65 | 1926 | `		pGen->pIn = &pNext[1];` |
|       3 | 1927 | `	}` |
|      53 | 1928 | `	pGen->pIn = pScanIn;` |
|      53 | 1929 | `	if( bAnyRef && GenStateNameListSource(pGen,&sTmp) != SXRET_OK ){` |
|     ! 0 | 1930 | `		return SXERR_ABORT;` |
|       - | 1931 | `	}` |
|     115 | 1932 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|       - | 1933 | `		SyToken *pArrow,*pTarget;` |
|       - | 1934 | ``		/* Split `keyExpr => target` at the top-level '=>' */`` |
|      65 | 1935 | `		pArrow = GenStateFindTopLevelArrow(pGen->pIn,pNext);` |
|      65 | 1936 | `		pTarget = &pArrow[1];` |
|      65 | 1937 | `		if( pArrow <= pGen->pIn \|\| pTarget >= pNext ){` |
|       - | 1938 | ``			/* Empty key (`[ => $v]`) or empty value (`["k" =>]`): PHP rejects`` |
|       - | 1939 | `			 * both. Reject rather than silently emitting unbalanced bytecode. */` |
|     ! 0 | 1940 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - | 1941 | `				"Cannot use empty array entries in keyed array assignment");` |
|     ! 0 | 1942 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - | 1943 | `		}` |
|       - | 1944 | `		/* Put the source array back on the stack: a DUP of the value sitting there,` |
|       - | 1945 | `		 * or -- when an entry of this list binds by reference -- a load of the name` |
|       - | 1946 | `		 * it was given, which is what makes a write-context fetch reach the source` |
|       - | 1947 | `		 * rather than a copy of it. */` |
|      65 | 1948 | `		if( bAnyRef ){` |
|       7 | 1949 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&sTmp),0);` |
|       4 | 1950 | `		}else{` |
|      59 | 1951 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|       - | 1952 | `		}` |
|       - | 1953 | `		/* Compile the key expression; it is pushed above the DUP'd source */` |
|      65 | 1954 | `		rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pArrow,EXPR_FLAG_RDONLY_LOAD,0);` |
|      65 | 1955 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1956 | `			return SXERR_ABORT;` |
|       - | 1957 | `		}` |
|      65 | 1958 | `		if( pTarget < pNext && (pTarget->nType & PH7_TK_AMPER/*'&'*/) ){` |
|       - | 1959 | ``			/* `['k' => &$t] = $src`: bind the target to the SOURCE element rather`` |
|       - | 1960 | `			 * than storing a copy into it. The fetch is a WRITE-context one, so a` |
|       - | 1961 | ``			 * missing key vivifies silently -- php's `$t =& $src['k']`. */`` |
|       5 | 1962 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,1,0,0);` |
|       5 | 1963 | `			rc = GenStateEmitListRefBind(&(*pGen),&pTarget[1],pNext);` |
|       5 | 1964 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1965 | `				return SXERR_ABORT;` |
|       - | 1966 | `			}` |
|       5 | 1967 | `			pGen->pIn = &pNext[1];` |
|       5 | 1968 | `			continue;` |
|       - | 1969 | `		}` |
|       - | 1970 | `		/* LOAD_IDX: pop the key, replace the DUP'd source with source[key].` |
|       - | 1971 | `		 * iP2=7 is the keyed-destructuring read context: an array source reads like` |
|       - | 1972 | ``		 * iP2=0 (a missing key loads NULL and warns `Undefined array key "k"`, php's`` |
|       - | 1973 | `		 * answer for the keyed spelling as much as for the positional one),` |
|       - | 1974 | `		 * but a NON-array source yields NULL + a per-key "Cannot use <type> as array"` |
|       - | 1975 | `		 * warning instead of char-indexing a string (matching PHP's OP_LOAD_LIST path). */` |
|      61 | 1976 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,7,0,0);` |
|      61 | 1977 | `		if( pTarget < pNext && ( (pTarget->nType & PH7_TK_OSB)` |
|      56 | 1978 | `			\|\| ( (pTarget->nType & PH7_TK_KEYWORD)` |
|      29 | 1979 | `				&& SX_PTR_TO_INT(pTarget->pUserData) == PH7_TKWRD_LIST ) ) ){` |
|       - | 1980 | `			/* Nested destructuring:  ["k" => [ ... ]]  or  ["k" => list( ... )].` |
|       - | 1981 | `			 * Treat source[key] as the inner body's source, then drop the` |
|       - | 1982 | `			 * leftover it leaves behind (mirrors the positional nested path). */` |
|       5 | 1983 | `			sxi32 isShort = (pTarget->nType & PH7_TK_OSB) != 0;` |
|       5 | 1984 | `			SyToken *pSavedIn = pGen->pIn;` |
|       5 | 1985 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|       5 | 1986 | `			pGen->pIn = pTarget;` |
|       5 | 1987 | `			pGen->pEnd = pNext;` |
|       5 | 1988 | `			rc = isShort ? PH7_CompileShortList(&(*pGen),0)` |
|       2 | 1989 | `			             : PH7_CompileList(&(*pGen),0);` |
|       5 | 1990 | `			pGen->pIn = pSavedIn;` |
|       5 | 1991 | `			pGen->pEnd = pSavedEnd;` |
|       5 | 1992 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1993 | `				return SXERR_ABORT;` |
|       - | 1994 | `			}` |
|       5 | 1995 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       3 | 1996 | `		}else{` |
|      57 | 1997 | `			rc = GenStateEmitListValueStore(&(*pGen),pTarget,pNext);` |
|      57 | 1998 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 1999 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - | 2000 | `			}` |
|       - | 2001 | `		}` |
|      61 | 2002 | `		pGen->pIn = &pNext[1];` |
|       3 | 2003 | `	}` |
|      53 | 2004 | `	if( bAnyRef ){` |
|       5 | 2005 | `		return GenStateDropListSourceName(pGen,&sTmp);` |
|       - | 2006 | `	}` |
|      49 | 2007 | `	return SXRET_OK;` |
|      28 | 2008 | `}` |
|       - | 2009 | `/*` |
|       - | 2010 | ` * Compile a POSITIONAL destructuring body one entry at a time, in SOURCE ORDER,` |
|       - | 2011 | ` * instead of pushing every target and letting OP_LOAD_LIST assign them together.` |
|       - | 2012 | ` *` |
|       - | 2013 | ` * This is the shape php always has, and it only matters once an entry binds BY` |
|       - | 2014 | ` * REFERENCE: the bind has to happen where it was written, because a later entry` |
|       - | 2015 | `` * may write THROUGH it. `[&$y, $y] = [1, 2]` binds $y to element 0 and then`` |
|       - | 2016 | ` * assigns element 1's value to $y -- which lands in element 0, so php answers` |
|       - | 2017 | `` * `[2, 2]`. Assigning first and binding afterwards answers `[1, 2]`.`` |
|       - | 2018 | ` *` |
|       - | 2019 | ` * The source array is on the stack top on entry and stays there, exactly as the` |
|       - | 2020 | ` * keyed body leaves it.` |
|       - | 2021 | ` */` |
|      50 | 2022 | `static sxi32 GenStateCompileSeqListBody(ph7_gen_state *pGen)` |
|       1 | 2023 | `{` |
|       - | 2024 | `	SyString sTmp;` |
|       - | 2025 | `	SyToken *pNext;` |
|      51 | 2026 | `	sxi32 nIndex = 0;` |
|       - | 2027 | `	sxi32 rc;` |
|      51 | 2028 | `	if( GenStateNameListSource(pGen,&sTmp) != SXRET_OK ){` |
|     ! 0 | 2029 | `		return SXERR_ABORT;` |
|       - | 2030 | `	}` |
|     119 | 2031 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|      69 | 2032 | `		SyToken *pTarget = pGen->pIn;` |
|       - | 2033 | `		int bRef,bNested,bShort;` |
|      69 | 2034 | `		if( pTarget >= pNext ){` |
|       - | 2035 | `			/* An empty slot ([, $b]) skips its index and touches nothing. */` |
|       3 | 2036 | `			pGen->pIn = &pNext[1];` |
|       3 | 2037 | `			nIndex++;` |
|       3 | 2038 | `			continue;` |
|       - | 2039 | `		}` |
|      67 | 2040 | `		bRef = (pTarget->nType & PH7_TK_AMPER/*'&'*/) != 0;` |
|      67 | 2041 | `		if( bRef ){` |
|      49 | 2042 | `			pTarget++;` |
|      49 | 2043 | `			if( pTarget >= pNext ){` |
|       - | 2044 | ``				/* `[&] = $src`: php names the token that stopped it. */`` |
|     ! 0 | 2045 | `				rc = PH7_GenSyntaxError(&(*pGen),pNext < pGen->pEnd ? pNext : 0,0);` |
|     ! 0 | 2046 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - | 2047 | `			}` |
|      24 | 2048 | `		}` |
|      67 | 2049 | `		bShort = (pTarget->nType & PH7_TK_OSB) != 0;` |
|      67 | 2050 | `		bNested = bShort \|\| ( (pTarget->nType & PH7_TK_KEYWORD)` |
|      31 | 2051 | `			&& SX_PTR_TO_INT(pTarget->pUserData) == PH7_TKWRD_LIST );` |
|      67 | 2052 | `		if( bNested && PH7_GenStateListSpanHasRef(pTarget,pNext) ){` |
|       - | 2053 | `			/* A nested level that binds by reference is a WRITE position for php as` |
|       - | 2054 | ``			 * much as a `&` target is: the bind below has to reach this element. */`` |
|       5 | 2055 | `			bRef = 1;` |
|       2 | 2056 | `		}` |
|       - | 2057 | `		/* Fetch source[index]. A by-REF position reads it in WRITE context -- which` |
|       - | 2058 | `		 * vivifies a missing key silently and makes a non-array source php's` |
|       - | 2059 | ``		 * `Cannot use a scalar value as an array` Error -- while every other one`` |
|       - | 2060 | `		 * takes the destructuring READ (iP2=7): a missing key warns, and a source` |
|       - | 2061 | ``		 * that is not an array at all warns `Cannot use <type> as array` once for`` |
|       - | 2062 | `		 * this position, which is what php's list assign says per position. */` |
|      67 | 2063 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&sTmp),0);` |
|       - | 2064 | `		{` |
|       - | 2065 | `			ph7_value *pIdx;` |
|       - | 2066 | `			sxu32 nConstIdx;` |
|      67 | 2067 | `			pIdx = PH7_ReserveConstObj(pGen->pVm,&nConstIdx);` |
|      67 | 2068 | `			if( pIdx == 0 ){` |
|     ! 0 | 2069 | `				PH7_GenCompileError(&(*pGen),E_ERROR,pTarget->nLine,` |
|       - | 2070 | `					"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 2071 | `				return SXERR_ABORT;` |
|       - | 2072 | `			}` |
|      67 | 2073 | `			PH7_MemObjInitFromInt(pGen->pVm,pIdx,(sxi64)nIndex);` |
|      67 | 2074 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,(sxi32)nConstIdx,0,0);` |
|       - | 2075 | `		}` |
|      67 | 2076 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,bRef ? 1 : 7,0,0);` |
|      67 | 2077 | `		if( bNested ){` |
|       5 | 2078 | `			SyToken *pSavedIn = pGen->pIn,*pSavedEnd = pGen->pEnd;` |
|       5 | 2079 | `			pGen->pIn = pTarget;` |
|       5 | 2080 | `			pGen->pEnd = pNext;` |
|       5 | 2081 | `			rc = bShort ? PH7_CompileShortList(&(*pGen),0) : PH7_CompileList(&(*pGen),0);` |
|       5 | 2082 | `			pGen->pIn = pSavedIn;` |
|       5 | 2083 | `			pGen->pEnd = pSavedEnd;` |
|       5 | 2084 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2085 | `				return SXERR_ABORT;` |
|       - | 2086 | `			}` |
|       - | 2087 | `			/* Drop the element the inner body left behind. */` |
|       5 | 2088 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      65 | 2089 | `		}else if( bRef ){` |
|      49 | 2090 | `			rc = GenStateEmitListRefBind(&(*pGen),pTarget,pNext);` |
|      49 | 2091 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2092 | `				return SXERR_ABORT;` |
|       - | 2093 | `			}` |
|      25 | 2094 | `		}else{` |
|      15 | 2095 | `			rc = GenStateEmitListValueStore(&(*pGen),pTarget,pNext);` |
|      15 | 2096 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2097 | `				return SXERR_ABORT;` |
|       - | 2098 | `			}` |
|       - | 2099 | `		}` |
|      67 | 2100 | `		pGen->pIn = &pNext[1];` |
|      67 | 2101 | `		nIndex++;` |
|       1 | 2102 | `	}` |
|      51 | 2103 | `	return GenStateDropListSourceName(pGen,&sTmp);` |
|      26 | 2104 | `}` |
|       - | 2105 | `/*` |
|       - | 2106 | ` * Shared body for list() and short list [...] compilation.` |
|       - | 2107 | ` * Assumes pGen->pIn and pGen->pEnd are already positioned past` |
|       - | 2108 | ` * the opening delimiter and before the closing delimiter.` |
|       - | 2109 | ` */` |
|     595 | 2110 | `static sxi32 GenStateCompileListBody(ph7_gen_state *pGen)` |
|       5 | 2111 | `{` |
|       - | 2112 | `	SySet sNested; /* Dynamically-sized container of NestedListEntry */` |
|       - | 2113 | `	SyToken *pNext;` |
|       - | 2114 | `	SyToken *pClassifyIn;` |
|     600 | 2115 | `	sxi32 nKeyed = 0, nPositional = 0, nEmpty = 0, nRefBind = 0;` |
|       - | 2116 | `	sxi32 nExpr;` |
|       - | 2117 | `	sxi32 rc;` |
|       - | 2118 | ``	/* First pass: classify entries as keyed (`k => v`), positional, or empty`` |
|       - | 2119 | `	 * skip slots ([,]). A list level must be entirely keyed or entirely` |
|       - | 2120 | `	 * positional — PHP fatals on a mix, and on an empty slot inside a keyed` |
|       - | 2121 | `	 * list. */` |
|     600 | 2122 | `	pClassifyIn = pGen->pIn;` |
|    1722 | 2123 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|    1127 | 2124 | `		if( pGen->pIn >= pNext ){` |
|      29 | 2125 | `			nEmpty++;` |
|    1113 | 2126 | `		}else if( GenStateFindTopLevelArrow(pGen->pIn,pNext) < pNext ){` |
|      65 | 2127 | `			nKeyed++;` |
|      34 | 2128 | `		}else{` |
|    1037 | 2129 | `			if( GenStateEntryBindsRef(pGen->pIn,pNext) ){` |
|      53 | 2130 | `				nRefBind++;` |
|      26 | 2131 | `			}` |
|    1037 | 2132 | `			nPositional++;` |
|       - | 2133 | `		}` |
|    1127 | 2134 | `		pGen->pIn = &pNext[1];` |
|       5 | 2135 | `	}` |
|     600 | 2136 | `	pGen->pIn = pClassifyIn;` |
|     600 | 2137 | `	if( nKeyed < 1 && nPositional < 1 ){` |
|       - | 2138 | ``		/* `[, ,] = $src` fills nothing: php refuses the whole construct rather than`` |
|       - | 2139 | `		 * running a destructure with no targets, which is what this used to do. */` |
|       4 | 2140 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn < pGen->pEnd ? pGen->pIn->nLine : 0,` |
|       - | 2141 | `			"Cannot use empty list");` |
|       4 | 2142 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - | 2143 | `	}` |
|     596 | 2144 | `	if( nKeyed > 0 && nEmpty > 0 ){` |
|     ! 0 | 2145 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - | 2146 | `			"Cannot use empty array entries in keyed array assignment");` |
|     ! 0 | 2147 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - | 2148 | `	}` |
|     596 | 2149 | `	if( nKeyed > 0 && nPositional > 0 ){` |
|     ! 0 | 2150 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - | 2151 | `			"Cannot mix keyed and unkeyed array entries in assignments");` |
|     ! 0 | 2152 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - | 2153 | `	}` |
|     596 | 2154 | `	if( nKeyed > 0 ){` |
|      53 | 2155 | `		return GenStateCompileKeyedListBody(pGen);` |
|       - | 2156 | `	}` |
|     546 | 2157 | `	if( nRefBind > 0 ){` |
|       - | 2158 | `		/* An entry binds BY REFERENCE, so the entries are settled one at a time and` |
|       - | 2159 | `		 * in SOURCE ORDER rather than pushed together for OP_LOAD_LIST to assign:` |
|       - | 2160 | `		 * a later entry may write THROUGH a bind an earlier one made. */` |
|      51 | 2161 | `		return GenStateCompileSeqListBody(pGen);` |
|       - | 2162 | `	}` |
|     496 | 2163 | `	nExpr = 0;` |
|     496 | 2164 | `	SySetInit(&sNested,&pGen->pVm->sAllocator,sizeof(struct NestedListEntry));` |
|    1482 | 2165 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|     991 | 2166 | `		if( pGen->pIn < pNext ){` |
|       - | 2167 | `			/* Check for nested list() */` |
|     971 | 2168 | `			if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       3 | 2169 | `				SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - | 2170 | `				/* Record this nested list for post-processing */` |
|       3 | 2171 | `				SyToken *pListEnd = 0;` |
|       3 | 2172 | `				if( &pGen->pIn[1] < pNext && (pGen->pIn[1].nType & PH7_TK_LPAREN) ){` |
|       3 | 2173 | `					PH7_DelimitNestedTokens(pGen->pIn+2,pNext,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       1 | 2174 | `				}` |
|       3 | 2175 | `				if( pListEnd ){` |
|       - | 2176 | `					struct NestedListEntry sEntry;` |
|       3 | 2177 | `					sEntry.nIndex = nExpr;` |
|       3 | 2178 | `					sEntry.pStart = pGen->pIn;` |
|       3 | 2179 | `					sEntry.pEnd = pListEnd + 1;` |
|       3 | 2180 | `					sEntry.isShort = 0;` |
|       3 | 2181 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|       1 | 2182 | `				}` |
|       - | 2183 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|       3 | 2184 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|     970 | 2185 | `			}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - | 2186 | `				/* Nested short destructuring [...] */` |
|      18 | 2187 | `				SyToken *pBracketEnd = 0;` |
|      18 | 2188 | `				PH7_DelimitNestedTokens(pGen->pIn+1,pNext,PH7_TK_OSB,PH7_TK_CSB,&pBracketEnd);` |
|      18 | 2189 | `				if( pBracketEnd ){` |
|       - | 2190 | `					struct NestedListEntry sEntry;` |
|      18 | 2191 | `					sEntry.nIndex = nExpr;` |
|      18 | 2192 | `					sEntry.pStart = pGen->pIn;` |
|      18 | 2193 | `					sEntry.pEnd = pBracketEnd + 1;` |
|      18 | 2194 | `					sEntry.isShort = 1;` |
|      18 | 2195 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|       8 | 2196 | `				}` |
|       - | 2197 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|      18 | 2198 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|      10 | 2199 | `			}else{` |
|       - | 2200 | `				/* Compile the expression holding the variable */` |
|     953 | 2201 | `				rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|     953 | 2202 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 2203 | `					SySetRelease(&sNested);` |
|     ! 0 | 2204 | `					return SXRET_OK;` |
|       - | 2205 | `				}` |
|       - | 2206 | `				{` |
|       - | 2207 | `					/* A property target ($o->p / Cls::$s) is a PURE WRITE here — the` |
|       - | 2208 | `					 * value lands via the following OP_LOAD_LIST's direct slot store.` |
|       - | 2209 | `					 * Tag the member so OP_MEMBER skips the uninitialized-typed read` |
|       - | 2210 | `					 * Error / __get consult and vivifies a missing property (php` |
|       - | 2211 | `					 * assigns without reading). */` |
|     953 | 2212 | `					VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|     953 | 2213 | `					if( pLast && pLast->iOp == PH7_OP_MEMBER && pLast->iP2 == PH7_MEMBER_READ ){` |
|      58 | 2214 | `						pLast->iP2 = PH7_MEMBER_LIST_TARGET;` |
|      28 | 2215 | `					}` |
|       - | 2216 | `				}` |
|       - | 2217 | `			}` |
|     487 | 2218 | `		}else{` |
|       - | 2219 | `			/* Empty entry,load NULL */` |
|      21 | 2220 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);` |
|       - | 2221 | `		}` |
|     991 | 2222 | `		nExpr++;` |
|       - | 2223 | `		/* Advance the stream cursor */` |
|     991 | 2224 | `		pGen->pIn = &pNext[1];` |
|       5 | 2225 | `	}` |
|       - | 2226 | `	/* Emit the LOAD_LIST instruction. P2 is how many entries FILL a position -- the` |
|       - | 2227 | `	 * empty slots left out -- which is how many times php complains when the source` |
|       - | 2228 | `	 * is not an array at all: it asks the source once per position it means to fill. */` |
|     496 | 2229 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,nExpr - nEmpty,0,0);` |
|       - | 2230 | `	/* After LOAD_LIST, the source array is still on the stack top.` |
|       - | 2231 | `	 * For each nested entry, emit code to extract the sub-array` |
|       - | 2232 | `	 * at the corresponding index and recursively destructure it.` |
|       - | 2233 | `	 */` |
|     496 | 2234 | `	if( SySetUsed(&sNested) > 0 ){` |
|      18 | 2235 | `		struct NestedListEntry *apNested = (struct NestedListEntry *)SySetBasePtr(&sNested);` |
|       - | 2236 | `		sxu32 i;` |
|      36 | 2237 | `		for(i = 0; i < SySetUsed(&sNested); i++){` |
|      20 | 2238 | `			SyToken *pSavedIn = pGen->pIn;` |
|      20 | 2239 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|       - | 2240 | `			ph7_value *pIdx;` |
|       - | 2241 | `			sxu32 nConstIdx;` |
|       - | 2242 | `			/* DUP the source array (it's on stack top) */` |
|      20 | 2243 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|       - | 2244 | `			/* Push the integer index for this nested entry */` |
|      20 | 2245 | `			pIdx = PH7_ReserveConstObj(pGen->pVm,&nConstIdx);` |
|      20 | 2246 | `			if( pIdx == 0 ){` |
|     ! 0 | 2247 | `				PH7_GenCompileError(&(*pGen),E_ERROR,0,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 2248 | `				SySetRelease(&sNested);` |
|     ! 0 | 2249 | `				return SXERR_ABORT;` |
|       - | 2250 | `			}` |
|      20 | 2251 | `			PH7_MemObjInitFromInt(pGen->pVm,pIdx,(sxi64)apNested[i].nIndex);` |
|      20 | 2252 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nConstIdx,0,0);` |
|       - | 2253 | `			/* LOAD_IDX: pop index, replace DUP'd source with source[index].` |
|       - | 2254 | `			 * iP2=2 signals the VM to emit an "Undefined array key" warning` |
|       - | 2255 | `			 * when the key is missing (PHP-compatible list destructuring).` |
|       - | 2256 | `			 */` |
|      20 | 2257 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,2,0,0);` |
|       - | 2258 | `			/* Recursively compile the inner list */` |
|      20 | 2259 | `			pGen->pIn = apNested[i].pStart;` |
|      20 | 2260 | `			pGen->pEnd = apNested[i].pEnd;` |
|      20 | 2261 | `			if( apNested[i].isShort ){` |
|      18 | 2262 | `				rc = PH7_CompileShortList(&(*pGen),0);` |
|      10 | 2263 | `			}else{` |
|       3 | 2264 | `				rc = PH7_CompileList(&(*pGen),0);` |
|       - | 2265 | `			}` |
|      20 | 2266 | `			pGen->pIn = pSavedIn;` |
|      20 | 2267 | `			pGen->pEnd = pSavedEnd;` |
|      20 | 2268 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2269 | `				SySetRelease(&sNested);` |
|     ! 0 | 2270 | `				return SXERR_ABORT;` |
|       - | 2271 | `			}` |
|       - | 2272 | `			/* Pop the leftover source[index] from the inner LOAD_LIST */` |
|      20 | 2273 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      11 | 2274 | `		}` |
|       8 | 2275 | `	}` |
|     496 | 2276 | `	SySetRelease(&sNested);` |
|       - | 2277 | `	/* Node successfully compiled */` |
|     496 | 2278 | `	return SXRET_OK;` |
|     302 | 2279 | `}` |
|      62 | 2280 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 | 2281 | `{` |
|       - | 2282 | `	/* Jump the 'list' keyword, the leading '(' and exclude trailing ')' */` |
|      67 | 2283 | `	pGen->pIn += 2;` |
|      67 | 2284 | `	pGen->pEnd--;` |
|      31 | 2285 | `	SXUNUSED(iCompileFlag);` |
|      67 | 2286 | `	return GenStateCompileListBody(pGen);` |
|       5 | 2287 | `}` |
|     533 | 2288 | `PH7_PRIVATE sxi32 PH7_CompileShortList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 | 2289 | `{` |
|       - | 2290 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|     538 | 2291 | `	pGen->pIn++;` |
|     538 | 2292 | `	pGen->pEnd--;` |
|     266 | 2293 | `	SXUNUSED(iCompileFlag);` |
|     538 | 2294 | `	return GenStateCompileListBody(pGen);` |
|       5 | 2295 | `}` |
|       - | 2296 | `/*` |
|       - | 2297 | ` * assert() source-text rendering.` |
|       - | 2298 | ` *` |
|       - | 2299 | ` * php compiles a DIRECT assert() call with a copy of the argument's AST, and a` |
|       - | 2300 | `` * failing assertion reports zend_ast_export() of that AST — `assert(1 == 2)`,`` |
|       - | 2301 | `` * `assert($x)`, `assert('')` — which is exactly the information the message`` |
|       - | 2302 | ` * exists to carry. PHL has no AST copy at runtime, so the compiler renders the` |
|       - | 2303 | ` * argument's TOKEN SPAN here, at compile time, normalizing to php's export` |
|       - | 2304 | ` * shape (each rule probed against php 8.5):` |
|       - | 2305 | ` *   - literal values fold the way php's AST holds them: numbers render from` |
|       - | 2306 | ` *     their parsed VALUE (0x10 -> 16, 1e3 -> 1000.0, 1_000 -> 1000, an` |
|       - | 2307 | ` *     int64-overflowing literal -> float), strings render single-quoted with` |
|       - | 2308 | ` *     their PROCESSED contents (\ and ' re-escaped), array(...) -> [...].` |
|       - | 2309 | ` *   - one space around binary operators, ", " between arguments/elements, no` |
|       - | 2310 | `` *     space inside ()/[] or around ->/?->/::/casts, `and`/`or` -> `&&`/`\|\|`,`` |
|       - | 2311 | ` *     a trailing comma is dropped, redundant OUTERMOST parens are dropped.` |
|       - | 2312 | ` * Accepted divergences from zend_ast_export on exotic input (message text` |
|       - | 2313 | ` * only, never behavior): redundant INNER parens are kept (php re-derives` |
|       - | 2314 | ` * grouping from precedence), interpolated "$x" strings and heredocs render as` |
|       - | 2315 | `` * written (php exports its interpolation AST), `new C` does not grow php's`` |
|       - | 2316 | `` * trailing `()`, and constant folding beyond single literals is not applied`` |
|       - | 2317 | `` * (php renders `'' . ''` as `''`).`` |
|       - | 2318 | ` */` |
|       - | 2319 | `/* Spacing classes: a space is inserted between two tokens when either side` |
|       - | 2320 | ` * FORCEs one (binary operators, the slot after a comma) or both sides are` |
|       - | 2321 | ` * operand-like (WANT). Grouping punctuation and glue operators contribute` |
|       - | 2322 | ` * NONE on their tight side. */` |
|       - | 2323 | `#define ASRT_SP_NONE  0` |
|       - | 2324 | `#define ASRT_SP_WANT  1` |
|       - | 2325 | `#define ASRT_SP_FORCE 2` |
|       - | 2326 | `enum AssertTokClass {` |
|       - | 2327 | `	ASRT_START = 0, /* virtual class before the first token */` |
|       - | 2328 | `	ASRT_OPERAND,   /* literals, identifiers, keywords */` |
|       - | 2329 | `	ASRT_BINOP,     /* == + . && ? : => instanceof ... */` |
|       - | 2330 | `	ASRT_UNARY,     /* ! ~ @ - + & casts, '$', '...' — glue after */` |
|       - | 2331 | `	ASRT_OPEN,      /* ( [ */` |
|       - | 2332 | `	ASRT_CLOSE,     /* ) ] */` |
|       - | 2333 | `	ASRT_GLUE,      /* -> ?-> :: ++ -- \ — glue both sides */` |
|       - | 2334 | `	ASRT_COMMA      /* , — glue before, force after */` |
|       - | 2335 | `};` |
|       - | 2336 | `static const sxu8 aAsrtBefore[] = { ASRT_SP_NONE, ASRT_SP_WANT, ASRT_SP_FORCE, ASRT_SP_WANT,` |
|       - | 2337 | `	ASRT_SP_NONE, ASRT_SP_NONE, ASRT_SP_NONE, ASRT_SP_NONE };` |
|       - | 2338 | `static const sxu8 aAsrtAfter[]  = { ASRT_SP_NONE, ASRT_SP_WANT, ASRT_SP_FORCE, ASRT_SP_NONE,` |
|       - | 2339 | `	ASRT_SP_NONE, ASRT_SP_WANT, ASRT_SP_NONE, ASRT_SP_FORCE };` |
|       - | 2340 | `/*` |
|       - | 2341 | ` * Append one PROCESSED string-value byte, re-escaped for a single-quoted` |
|       - | 2342 | ` * rendering: php's export escapes only backslash and the quote itself; every` |
|       - | 2343 | ` * other byte (including control characters) is emitted raw.` |
|       - | 2344 | ` */` |
|      42 | 2345 | `static void AssertRenderQuotedByte(SyBlob *pOut,int c)` |
|       2 | 2346 | `{` |
|      44 | 2347 | `	char ch = (char)c;` |
|      44 | 2348 | `	if( c == '\\' \|\| c == '\'' ){` |
|       3 | 2349 | `		SyBlobAppend(pOut,"\\",1);` |
|       1 | 2350 | `	}` |
|      44 | 2351 | `	SyBlobAppend(pOut,&ch,1);` |
|      44 | 2352 | `}` |
|       - | 2353 | `/* Append the UTF-8 encoding of a \u{...} code point (value bytes, re-escaped). */` |
|     ! 0 | 2354 | `static void AssertRenderUtf8(SyBlob *pOut,sxu32 c)` |
|     ! 0 | 2355 | `{` |
|     ! 0 | 2356 | `	if( c < 0x80 ){` |
|     ! 0 | 2357 | `		AssertRenderQuotedByte(pOut,(int)c);` |
|     ! 0 | 2358 | `	}else if( c < 0x800 ){` |
|     ! 0 | 2359 | `		AssertRenderQuotedByte(pOut,(int)(0xc0 \| (c >> 6)));` |
|     ! 0 | 2360 | `		AssertRenderQuotedByte(pOut,(int)(0x80 \| (c & 0x3f)));` |
|     ! 0 | 2361 | `	}else if( c < 0x10000 ){` |
|     ! 0 | 2362 | `		AssertRenderQuotedByte(pOut,(int)(0xe0 \| (c >> 12)));` |
|     ! 0 | 2363 | `		AssertRenderQuotedByte(pOut,(int)(0x80 \| ((c >> 6) & 0x3f)));` |
|     ! 0 | 2364 | `		AssertRenderQuotedByte(pOut,(int)(0x80 \| (c & 0x3f)));` |
|     ! 0 | 2365 | `	}else{` |
|     ! 0 | 2366 | `		AssertRenderQuotedByte(pOut,(int)(0xf0 \| (c >> 18)));` |
|     ! 0 | 2367 | `		AssertRenderQuotedByte(pOut,(int)(0x80 \| ((c >> 12) & 0x3f)));` |
|     ! 0 | 2368 | `		AssertRenderQuotedByte(pOut,(int)(0x80 \| ((c >> 6) & 0x3f)));` |
|     ! 0 | 2369 | `		AssertRenderQuotedByte(pOut,(int)(0x80 \| (c & 0x3f)));` |
|       - | 2370 | `	}` |
|     ! 0 | 2371 | `}` |
|       - | 2372 | `/*` |
|       - | 2373 | ` * Render a single-quoted-source string (or nowdoc body): only \\ and \' are` |
|       - | 2374 | ` * escape sequences there; any other backslash is a literal byte.` |
|       - | 2375 | ` */` |
|       2 | 2376 | `static void AssertRenderSglString(SyBlob *pOut,const char *z,sxu32 n)` |
|       1 | 2377 | `{` |
|       3 | 2378 | `	sxu32 i = 0;` |
|       3 | 2379 | `	SyBlobAppend(pOut,"'",1);` |
|      11 | 2380 | `	while( i < n ){` |
|       9 | 2381 | `		if( z[i] == '\\' && i + 1 < n && (z[i+1] == '\\' \|\| z[i+1] == '\'') ){` |
|       3 | 2382 | `			AssertRenderQuotedByte(pOut,z[i+1]);` |
|       3 | 2383 | `			i += 2;` |
|       2 | 2384 | `		}else{` |
|       7 | 2385 | `			AssertRenderQuotedByte(pOut,z[i]);` |
|       7 | 2386 | `			i++;` |
|       - | 2387 | `		}` |
|       1 | 2388 | `	}` |
|       3 | 2389 | `	SyBlobAppend(pOut,"'",1);` |
|       3 | 2390 | `}` |
|       - | 2391 | `/*` |
|       - | 2392 | ` * Render a double-quoted-source string (or heredoc body) with php's escape` |
|       - | 2393 | ` * processing — the value bytes are what php's AST holds, and the export prints` |
|       - | 2394 | ` * them single-quoted. An UNKNOWN escape keeps the backslash and the character,` |
|       - | 2395 | ` * matching php's string semantics.` |
|       - | 2396 | ` */` |
|      12 | 2397 | `static void AssertRenderDblString(SyBlob *pOut,const char *z,sxu32 n)` |
|       3 | 2398 | `{` |
|      15 | 2399 | `	sxu32 i = 0;` |
|      15 | 2400 | `	SyBlobAppend(pOut,"'",1);` |
|      49 | 2401 | `	while( i < n ){` |
|      36 | 2402 | `		int c = z[i];` |
|       - | 2403 | `		int d;` |
|      36 | 2404 | `		if( c != '\\' \|\| i + 1 >= n ){` |
|      36 | 2405 | `			AssertRenderQuotedByte(pOut,c);` |
|      36 | 2406 | `			i++;` |
|      36 | 2407 | `			continue;` |
|       - | 2408 | `		}` |
|     ! 0 | 2409 | `		d = z[i+1];` |
|     ! 0 | 2410 | `		i += 2;` |
|     ! 0 | 2411 | `		switch(d){` |
|     ! 0 | 2412 | `		case 'n': AssertRenderQuotedByte(pOut,'\n'); break;` |
|     ! 0 | 2413 | `		case 't': AssertRenderQuotedByte(pOut,'\t'); break;` |
|     ! 0 | 2414 | `		case 'r': AssertRenderQuotedByte(pOut,'\r'); break;` |
|     ! 0 | 2415 | `		case 'v': AssertRenderQuotedByte(pOut,'\v'); break;` |
|     ! 0 | 2416 | `		case 'f': AssertRenderQuotedByte(pOut,'\f'); break;` |
|     ! 0 | 2417 | `		case 'e': AssertRenderQuotedByte(pOut,0x1b); break;` |
|     ! 0 | 2418 | `		case '\\': AssertRenderQuotedByte(pOut,'\\'); break;` |
|     ! 0 | 2419 | `		case '"': AssertRenderQuotedByte(pOut,'"'); break;` |
|     ! 0 | 2420 | `		case '$': AssertRenderQuotedByte(pOut,'$'); break;` |
|     ! 0 | 2421 | `		case 'x': case 'X': {` |
|       - | 2422 | `			/* Up to two hex digits; a bare \x is literal. */` |
|     ! 0 | 2423 | `			int nHex = 0, v = 0;` |
|     ! 0 | 2424 | `			while( nHex < 2 && i < n && (unsigned char)z[i] < 0x80 && SyisHex((unsigned char)z[i]) ){` |
|     ! 0 | 2425 | `				v = (v << 4) \| SyHexToint((unsigned char)z[i]);` |
|     ! 0 | 2426 | `				i++; nHex++;` |
|     ! 0 | 2427 | `			}` |
|     ! 0 | 2428 | `			if( nHex > 0 ){` |
|     ! 0 | 2429 | `				AssertRenderQuotedByte(pOut,v);` |
|     ! 0 | 2430 | `			}else{` |
|     ! 0 | 2431 | `				AssertRenderQuotedByte(pOut,'\\');` |
|     ! 0 | 2432 | `				AssertRenderQuotedByte(pOut,d);` |
|       - | 2433 | `			}` |
|     ! 0 | 2434 | `			break;` |
|       - | 2435 | `		}` |
|     ! 0 | 2436 | `		case 'u': {` |
|       - | 2437 | `			/* \u{HEX+} — anything else keeps the backslash (php). */` |
|     ! 0 | 2438 | `			if( i < n && z[i] == '{' ){` |
|     ! 0 | 2439 | `				sxu32 v = 0; sxu32 j = i + 1; int nHex = 0;` |
|     ! 0 | 2440 | `				while( j < n && (unsigned char)z[j] < 0x80 && SyisHex((unsigned char)z[j]) && nHex < 8 ){` |
|     ! 0 | 2441 | `					v = (v << 4) \| (sxu32)SyHexToint((unsigned char)z[j]);` |
|     ! 0 | 2442 | `					j++; nHex++;` |
|     ! 0 | 2443 | `				}` |
|     ! 0 | 2444 | `				if( nHex > 0 && j < n && z[j] == '}' ){` |
|     ! 0 | 2445 | `					AssertRenderUtf8(pOut,v);` |
|     ! 0 | 2446 | `					i = j + 1;` |
|     ! 0 | 2447 | `					break;` |
|       - | 2448 | `				}` |
|     ! 0 | 2449 | `			}` |
|     ! 0 | 2450 | `			AssertRenderQuotedByte(pOut,'\\');` |
|     ! 0 | 2451 | `			AssertRenderQuotedByte(pOut,d);` |
|     ! 0 | 2452 | `			break;` |
|       - | 2453 | `		}` |
|     ! 0 | 2454 | `		default:` |
|     ! 0 | 2455 | `			if( d >= '0' && d <= '7' ){` |
|       - | 2456 | `				/* Up to three octal digits (the first was d). */` |
|     ! 0 | 2457 | `				int nOct = 1, v = d - '0';` |
|     ! 0 | 2458 | `				while( nOct < 3 && i < n && z[i] >= '0' && z[i] <= '7' ){` |
|     ! 0 | 2459 | `					v = (v << 3) \| (z[i] - '0');` |
|     ! 0 | 2460 | `					i++; nOct++;` |
|     ! 0 | 2461 | `				}` |
|     ! 0 | 2462 | `				AssertRenderQuotedByte(pOut,v & 0xff);` |
|     ! 0 | 2463 | `			}else{` |
|     ! 0 | 2464 | `				AssertRenderQuotedByte(pOut,'\\');` |
|     ! 0 | 2465 | `				AssertRenderQuotedByte(pOut,d);` |
|       - | 2466 | `			}` |
|     ! 0 | 2467 | `			break;` |
|       - | 2468 | `		}` |
|     ! 0 | 2469 | `	}` |
|      15 | 2470 | `	SyBlobAppend(pOut,"'",1);` |
|      15 | 2471 | `}` |
|       - | 2472 | `/*` |
|       - | 2473 | ` * Append a double in php's AST-export shape: the shortest round-tripping` |
|       - | 2474 | ` * decimal, with a forced ".0" fraction when the digits alone look integral` |
|       - | 2475 | ` * (1e3 -> "1000.0", 1e20 -> "1.0E+20") — the var_export float shape.` |
|       - | 2476 | ` */` |
|       8 | 2477 | `static void AssertRenderReal(SyBlob *pOut,ph7_real rVal)` |
|       1 | 2478 | `{` |
|       - | 2479 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|       - | 2480 | `	/* No floating point: ph7_real IS sxi64, there is no shortest-round-trip` |
|       - | 2481 | `	 * decimal to search for and no ".0" to force, so the value renders as the` |
|       - | 2482 | `	 * integer it is -- the same shape the INTEGER arm below emits. Taking` |
|       - | 2483 | `	 * ph7_real rather than double is what keeps the two call sites from` |
|       - | 2484 | `	 * narrowing (MSVC /W4 makes that C4244, and /WX makes it an error). */` |
|       - | 2485 | `	SyBlobFormat(pOut,"%qd",(sxi64)rVal);` |
|       - | 2486 | `#else` |
|       9 | 2487 | `	sxu32 nBefore = SyBlobLength(pOut);` |
|       - | 2488 | `	const char *zOut;` |
|       - | 2489 | `	sxu32 i, nAfter;` |
|       9 | 2490 | `	int bPlain = 1;` |
|       9 | 2491 | `	PH7_AppendShortestReal(pOut,rVal);` |
|       9 | 2492 | `	zOut = (const char *)SyBlobData(pOut);` |
|       9 | 2493 | `	nAfter = SyBlobLength(pOut);` |
|      23 | 2494 | `	for( i = nBefore; i < nAfter; i++ ){` |
|      21 | 2495 | `		if( !((zOut[i] >= '0' && zOut[i] <= '9') \|\| zOut[i] == '-') ){` |
|       7 | 2496 | `			bPlain = 0;` |
|       7 | 2497 | `			break;` |
|       - | 2498 | `		}` |
|       8 | 2499 | `	}` |
|       9 | 2500 | `	if( bPlain ){` |
|       3 | 2501 | `		SyBlobAppend(pOut,".0",2);` |
|       1 | 2502 | `	}` |
|       - | 2503 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|       9 | 2504 | `}` |
|       - | 2505 | `/*` |
|       - | 2506 | ` * Render the token span [pIn, pEnd) — a direct assert() call's first argument —` |
|       - | 2507 | ` * into pOut in php's zend_ast_export shape (see the block comment above).` |
|       - | 2508 | ` * Total: every span renders to SOMETHING (unknown constructs fall back to` |
|       - | 2509 | ` * their raw token text), so the capture never aborts a compile.` |
|       - | 2510 | ` */` |
|      62 | 2511 | `PH7_PRIVATE void PH7_GenRenderAssertSpan(ph7_gen_state *pGen,SyToken *pIn,SyToken *pEnd,SyBlob *pOut)` |
|       5 | 2512 | `{` |
|       - | 2513 | `	sxu8 aParen[64]; /* 1 = this '(' depth is an array(...) literal rendered as [...] */` |
|      67 | 2514 | `	sxu32 nParen = 0;` |
|      67 | 2515 | `	int iPrev = ASRT_START;` |
|      67 | 2516 | ``	int bArrayOpen = 0; /* the next '(' belongs to a suppressed `array` keyword */`` |
|       - | 2517 | `	/* php drops every redundant paren when re-deriving source from the AST;` |
|       - | 2518 | `	 * dropping the OUTERMOST pair(s) is the token-level equivalent for the` |
|       - | 2519 | ``	 * common `assert((...))` spelling. */`` |
|      69 | 2520 | `	while( pIn < pEnd - 1 && (pIn->nType & PH7_TK_LPAREN) && (pEnd[-1].nType & PH7_TK_RPAREN) ){` |
|       - | 2521 | `		SyToken *p;` |
|       3 | 2522 | `		sxi32 iDepth = 0;` |
|       3 | 2523 | `		SyToken *pMatch = 0;` |
|      11 | 2524 | `		for( p = pIn; p < pEnd; p++ ){` |
|      11 | 2525 | `			if( p->nType & PH7_TK_LPAREN ){` |
|       3 | 2526 | `				iDepth++;` |
|      10 | 2527 | `			}else if( p->nType & PH7_TK_RPAREN ){` |
|       3 | 2528 | `				iDepth--;` |
|       3 | 2529 | `				if( iDepth == 0 ){ pMatch = p; break; }` |
|     ! 0 | 2530 | `			}` |
|       5 | 2531 | `		}` |
|       3 | 2532 | `		if( pMatch != &pEnd[-1] ){` |
|     ! 0 | 2533 | `			break;` |
|       - | 2534 | `		}` |
|       3 | 2535 | `		pIn++;` |
|       3 | 2536 | `		pEnd--;` |
|       1 | 2537 | `	}` |
|     267 | 2538 | `	for( ; pIn < pEnd ; pIn++ ){` |
|     205 | 2539 | `		SyToken *pTok = pIn;` |
|     205 | 2540 | `		const char *zTxt = pTok->sData.zString;` |
|     205 | 2541 | `		sxu32 nTxt = pTok->sData.nByte;` |
|       - | 2542 | `		int iCls;` |
|       - | 2543 | `		sxu32 nMark;` |
|       - | 2544 | `		/* --- classify + pre-token handling ------------------------------ */` |
|     205 | 2545 | `		if( pTok->nType & PH7_TK_LPAREN ){` |
|      15 | 2546 | `			iCls = ASRT_OPEN;` |
|     199 | 2547 | `		}else if( pTok->nType & PH7_TK_RPAREN ){` |
|      15 | 2548 | `			iCls = ASRT_CLOSE;` |
|     187 | 2549 | `		}else if( pTok->nType & (PH7_TK_OSB\|PH7_TK_CSB) ){` |
|      13 | 2550 | `			iCls = (pTok->nType & PH7_TK_OSB) ? ASRT_OPEN : ASRT_CLOSE;` |
|     175 | 2551 | `		}else if( pTok->nType & PH7_TK_COMMA ){` |
|       - | 2552 | `			/* php's export never prints a trailing comma. */` |
|       7 | 2553 | `			if( &pIn[1] < pEnd && (pIn[1].nType & (PH7_TK_RPAREN\|PH7_TK_CSB)) ){` |
|     ! 0 | 2554 | `				continue;` |
|       - | 2555 | `			}` |
|       7 | 2556 | `			iCls = ASRT_COMMA;` |
|     166 | 2557 | `		}else if( pTok->nType & PH7_TK_DOLLAR ){` |
|       3 | 2558 | `			iCls = ASRT_UNARY; /* operand-like before, glued to its name after */` |
|     162 | 2559 | `		}else if( pTok->nType & PH7_TK_NSSEP ){` |
|     ! 0 | 2560 | `			iCls = ASRT_GLUE;` |
|     161 | 2561 | `		}else if( pTok->nType & PH7_TK_ELLIPSIS ){` |
|     ! 0 | 2562 | `			iCls = ASRT_UNARY;` |
|     161 | 2563 | `		}else if( pTok->nType & PH7_TK_OP ){` |
|      46 | 2564 | `			iCls = ASRT_BINOP;` |
|      46 | 2565 | `			if( nTxt > 0 ){` |
|      46 | 2566 | `				int c0 = zTxt[0];` |
|      46 | 2567 | `				if( c0 == '(' ){` |
|     ! 0 | 2568 | ``					iCls = ASRT_UNARY; /* lexer-merged cast token `(int)` */`` |
|      60 | 2569 | `				}else if( nTxt == 2 && (SyMemcmp(zTxt,"->",2) == 0 \|\| SyMemcmp(zTxt,"::",2) == 0` |
|      28 | 2570 | `						\|\| SyMemcmp(zTxt,"++",2) == 0 \|\| SyMemcmp(zTxt,"--",2) == 0) ){` |
|     ! 0 | 2571 | `					iCls = ASRT_GLUE;` |
|      46 | 2572 | `				}else if( nTxt == 3 && SyMemcmp(zTxt,"?->",3) == 0 ){` |
|     ! 0 | 2573 | `					iCls = ASRT_GLUE;` |
|      46 | 2574 | `				}else if( nTxt == 1 && (c0 == '!' \|\| c0 == '~' \|\| c0 == '@') ){` |
|       3 | 2575 | `					iCls = ASRT_UNARY;` |
|      45 | 2576 | `				}else if( nTxt == 1 && (c0 == '-' \|\| c0 == '+' \|\| c0 == '&') ){` |
|       - | 2577 | `					/* Unary when nothing operand-like precedes. */` |
|       4 | 2578 | `					if( iPrev == ASRT_START \|\| iPrev == ASRT_BINOP \|\| iPrev == ASRT_UNARY` |
|       3 | 2579 | `					 \|\| iPrev == ASRT_OPEN \|\| iPrev == ASRT_COMMA ){` |
|       3 | 2580 | `						iCls = ASRT_UNARY;` |
|       2 | 2581 | `					}` |
|      42 | 2582 | `				}else if( pTok->nType & PH7_TK_ID ){` |
|       - | 2583 | `					/* Alpha operators: and/or normalize to php's export spelling;` |
|       - | 2584 | `					 * new/clone read as prefix keywords (operand spacing). */` |
|       3 | 2585 | `					if( nTxt == 3 && SyStrnicmp(zTxt,"and",3) == 0 ){` |
|       3 | 2586 | `						zTxt = "&&"; nTxt = 2;` |
|       1 | 2587 | `					}else if( nTxt == 2 && SyStrnicmp(zTxt,"or",2) == 0 ){` |
|     ! 0 | 2588 | `						zTxt = "\|\|"; nTxt = 2;` |
|     ! 0 | 2589 | `					}else if( (nTxt == 3 && SyStrnicmp(zTxt,"new",3) == 0)` |
|     ! 0 | 2590 | `						\|\| (nTxt == 5 && SyStrnicmp(zTxt,"clone",5) == 0) ){` |
|     ! 0 | 2591 | `						iCls = ASRT_OPERAND;` |
|     ! 0 | 2592 | `					}` |
|       1 | 2593 | `				}` |
|      24 | 2594 | `			}` |
|     139 | 2595 | `		}else if( pTok->nType & (PH7_TK_EQUAL\|PH7_TK_ARRAY_OP\|PH7_TK_COLON\|PH7_TK_AMPER) ){` |
|     ! 0 | 2596 | `			iCls = ASRT_BINOP;` |
|     117 | 2597 | `		}else if( pTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|      35 | 2598 | `			iCls = ASRT_OPERAND;` |
|       - | 2599 | ``			/* `array` `(` — php's AST holds one list node for both spellings and`` |
|       - | 2600 | ``			 * always exports `[...]`. Suppress the keyword (it lexes as a KEYWORD`` |
|       - | 2601 | `			 * token, not an ID); the '(' renders '['. */` |
|      30 | 2602 | `			if( nTxt == 5 && SyStrnicmp(zTxt,"array",5) == 0` |
|      17 | 2603 | `			 && &pIn[1] < pEnd && (pIn[1].nType & PH7_TK_LPAREN) ){` |
|       9 | 2604 | `				bArrayOpen = 1;` |
|       9 | 2605 | `				continue;` |
|       - | 2606 | `			}` |
|      16 | 2607 | `		}else{` |
|       - | 2608 | `			/* keywords (true/false/null/fn/match/...), numbers, strings,` |
|       - | 2609 | `			 * member names, '{'/'}' and anything unforeseen */` |
|      86 | 2610 | `			iCls = ASRT_OPERAND;` |
|       - | 2611 | `		}` |
|       - | 2612 | ``		/* Elvis `? :` — php exports the two-token form as `?:`. */`` |
|     194 | 2613 | `		if( iCls == ASRT_BINOP && nTxt == 1 && zTxt[0] == '?'` |
|      11 | 2614 | `		 && &pIn[1] < pEnd && (pIn[1].nType & PH7_TK_COLON) ){` |
|       3 | 2615 | `			nMark = SyBlobLength(pOut);` |
|       3 | 2616 | `			if( iPrev != ASRT_START && nMark > 0 ){` |
|       3 | 2617 | `				SyBlobAppend(pOut," ",1);` |
|       1 | 2618 | `			}` |
|       3 | 2619 | `			SyBlobAppend(pOut,"?:",2);` |
|       3 | 2620 | `			pIn++; /* consume the ':' */` |
|       3 | 2621 | `			iPrev = ASRT_BINOP;` |
|       3 | 2622 | `			continue;` |
|       - | 2623 | `		}` |
|       - | 2624 | `		/* --- spacing ---------------------------------------------------- */` |
|     197 | 2625 | `		if( iPrev != ASRT_START ){` |
|     134 | 2626 | `			int iAfter = aAsrtAfter[iPrev];` |
|     134 | 2627 | `			int iBefore = aAsrtBefore[iCls];` |
|     130 | 2628 | `			if( iAfter == ASRT_SP_FORCE \|\| iBefore == ASRT_SP_FORCE` |
|      69 | 2629 | `			 \|\| (iAfter == ASRT_SP_WANT && iBefore == ASRT_SP_WANT) ){` |
|      86 | 2630 | `				SyBlobAppend(pOut," ",1);` |
|      42 | 2631 | `			}` |
|      65 | 2632 | `		}` |
|       - | 2633 | `		/* --- emit ------------------------------------------------------- */` |
|     197 | 2634 | `		if( pTok->nType & PH7_TK_LPAREN ){` |
|      15 | 2635 | `			if( nParen < sizeof(aParen) ){` |
|      15 | 2636 | `				aParen[nParen] = (sxu8)bArrayOpen;` |
|       6 | 2637 | `			}` |
|      15 | 2638 | `			nParen++;` |
|      15 | 2639 | `			SyBlobAppend(pOut,bArrayOpen ? "[" : "(",1);` |
|      15 | 2640 | `			bArrayOpen = 0;` |
|     191 | 2641 | `		}else if( pTok->nType & PH7_TK_RPAREN ){` |
|      15 | 2642 | `			int bArr = 0;` |
|      15 | 2643 | `			if( nParen > 0 ){` |
|      15 | 2644 | `				nParen--;` |
|      15 | 2645 | `				if( nParen < sizeof(aParen) ){` |
|      15 | 2646 | `					bArr = aParen[nParen];` |
|       6 | 2647 | `				}` |
|       6 | 2648 | `			}` |
|      15 | 2649 | `			SyBlobAppend(pOut,bArr ? "]" : ")",1);` |
|     178 | 2650 | `		}else if( pTok->nType & (PH7_TK_INTEGER\|PH7_TK_REAL) ){` |
|       - | 2651 | `			char zScratch[GEN_NUM_SCRATCH];` |
|      71 | 2652 | `			char *zAlloc = 0;` |
|       - | 2653 | `			SyString sNum;` |
|     102 | 2654 | `			if( GenStateStripNumericSeparators(&pGen->pVm->sAllocator,&pTok->sData,` |
|      71 | 2655 | `					zScratch,sizeof(zScratch),&sNum,&zAlloc) != SXRET_OK ){` |
|     ! 0 | 2656 | `				SyBlobAppend(pOut,zTxt,nTxt); /* alloc failure: raw text */` |
|      71 | 2657 | `			}else if( pTok->nType & PH7_TK_INTEGER ){` |
|      63 | 2658 | `				ph7_real rOverflow = 0;` |
|      63 | 2659 | `				int bDecimalOverflow = 0;` |
|      63 | 2660 | `				if( GenStateIntLiteralOverflows(&sNum,&rOverflow,&bDecimalOverflow) ){` |
|     ! 0 | 2661 | `					if( bDecimalOverflow ){` |
|     ! 0 | 2662 | `						SyStrToReal(sNum.zString,sNum.nByte,(void *)&rOverflow,0);` |
|     ! 0 | 2663 | `					}` |
|     ! 0 | 2664 | `					AssertRenderReal(pOut,rOverflow);` |
|     ! 0 | 2665 | `				}else{` |
|      63 | 2666 | `					SyBlobFormat(pOut,"%qd",PH7_TokenValueToInt64(&sNum));` |
|       - | 2667 | `				}` |
|      33 | 2668 | `			}else{` |
|       9 | 2669 | `				ph7_real rVal = 0;` |
|       9 | 2670 | `				SyStrToReal(sNum.zString,sNum.nByte,(void *)&rVal,0);` |
|       9 | 2671 | `				AssertRenderReal(pOut,rVal);` |
|       - | 2672 | `			}` |
|      71 | 2673 | `			if( zAlloc ){` |
|     ! 0 | 2674 | `				SyMemBackendFree(&pGen->pVm->sAllocator,zAlloc);` |
|       3 | 2675 | `			}` |
|     138 | 2676 | `		}else if( pTok->nType & (PH7_TK_SSTR\|PH7_TK_NOWDOC) ){` |
|       3 | 2677 | `			AssertRenderSglString(pOut,zTxt,nTxt);` |
|     103 | 2678 | `		}else if( pTok->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
|      15 | 2679 | `			if( SyByteFind(zTxt,nTxt,'$',0) == SXRET_OK ){` |
|       - | 2680 | `				/* Interpolated: php exports its interpolation AST in a` |
|       - | 2681 | `				 * double-quoted form; the raw source is the token-level` |
|       - | 2682 | `				 * equivalent. */` |
|     ! 0 | 2683 | `				SyBlobAppend(pOut,"\"",1);` |
|     ! 0 | 2684 | `				SyBlobAppend(pOut,zTxt,nTxt);` |
|     ! 0 | 2685 | `				SyBlobAppend(pOut,"\"",1);` |
|     ! 0 | 2686 | `			}else{` |
|      15 | 2687 | `				AssertRenderDblString(pOut,zTxt,nTxt);` |
|       - | 2688 | `			}` |
|       9 | 2689 | `		}else{` |
|      90 | 2690 | `			SyBlobAppend(pOut,zTxt,nTxt);` |
|       - | 2691 | `		}` |
|     197 | 2692 | `		iPrev = iCls;` |
|     101 | 2693 | `	}` |
|      67 | 2694 | `}` |
|       - | 2695 |  |
