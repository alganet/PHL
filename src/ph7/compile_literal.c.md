# src/ph7/compile_literal.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1348/1550 lines (86.97%)

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
|      702 |   24 | `static int GenStateIsBaseDigit(int c, int base)` |
|        3 |   25 | `{` |
|        - |   26 | `	/* ASCII arithmetic, not <ctype.h>: the byte can be any value in a string` |
|        - |   27 | `	 * literal, and isdigit()/isxdigit() are both locale-dependent and undefined` |
|        - |   28 | `	 * for a negative char. */` |
|      705 |   29 | `	if( base == 16 ){` |
|       13 |   30 | `		return (c >= '0' && c <= '9') \|\| (c >= 'a' && c <= 'f') \|\| (c >= 'A' && c <= 'F');` |
|        - |   31 | `	}` |
|      693 |   32 | `	if( base == 8 ){ return c >= '0' && c <= '7'; }` |
|      687 |   33 | `	if( base == 2 ){ return c == '0' \|\| c == '1'; }` |
|      681 |   34 | `	return c >= '0' && c <= '9';` |
|      354 |   35 | `}` |
|        - |   36 | `/*` |
|        - |   37 | ` * Strip PHP 7.4 numeric literal separators (underscores between digits) from` |
|        - |   38 | ` * a numeric token's text and yield a SyString suitable for the low-level` |
|        - |   39 | ` * converters (SyStrToInt64 / SyStrToReal / etc.).` |
|        - |   40 | ` *` |
|        - |   41 | ` * Fast path: if the token contains no '_', *pOut aliases pToken with no copy` |
|        - |   42 | ` * and *pzAlloc is set to NULL.` |
|        - |   43 | ` * Stack path: if the cleaned bytes fit in zScratch, they are written there` |
|        - |   44 | ` * and *pzAlloc is set to NULL.` |
|        - |   45 | ` * Heap path: for literals larger than the scratch buffer, a fresh buffer is` |
|        - |   46 | ` * allocated from pAlloc, returned via *pzAlloc, and must be released by the` |
|        - |   47 | ` * caller with SyMemBackendFree once the converter is done.` |
|        - |   48 | ` *` |
|        - |   49 | ` * Returns SXRET_OK on success, SXERR_ABORT on allocator failure (in which` |
|        - |   50 | ` * case *pOut is left untouched and the caller must not read it).` |
|        - |   51 | ` */` |
|  1005147 |   52 | `PH7_PRIVATE sxi32 GenStateStripNumericSeparators(` |
|        - |   53 | `	SyMemBackend *pAlloc,` |
|        - |   54 | `	const SyString *pToken,` |
|        - |   55 | `	char *zScratch, sxu32 nScratch,` |
|        - |   56 | `	SyString *pOut, char **pzAlloc)` |
|        5 |   57 | `{` |
|        - |   58 | `	sxu32 i, j;` |
|  1005152 |   59 | `	int hasUnderscore = 0;` |
|        - |   60 | `	char *zBuf;` |
|  1005152 |   61 | `	*pzAlloc = 0;` |
|  2445971 |   62 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|  1441092 |   63 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|   719410 |   64 | `	}` |
|  1005152 |   65 | `	if( !hasUnderscore ){` |
|  1004884 |   66 | `		SyStringDupPtr(pOut, pToken);` |
|  1004884 |   67 | `		return SXRET_OK;` |
|        - |   68 | `	}` |
|      270 |   69 | `	if( pToken->nByte <= nScratch ){` |
|      268 |   70 | `		zBuf = zScratch;` |
|      135 |   71 | `	}else{` |
|        3 |   72 | `		zBuf = (char *)SyMemBackendAlloc(pAlloc, pToken->nByte);` |
|        3 |   73 | `		if( zBuf == 0 ){` |
|      ! 0 |   74 | `			return SXERR_ABORT;` |
|        - |   75 | `		}` |
|        3 |   76 | `		*pzAlloc = zBuf;` |
|        - |   77 | `	}` |
|      270 |   78 | `	j = 0;` |
|     2992 |   79 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|     2724 |   80 | `		if( pToken->zString[i] != '_' ){ zBuf[j++] = pToken->zString[i]; }` |
|     1363 |   81 | `	}` |
|      270 |   82 | `	SyStringInitFromBuf(pOut, zBuf, j);` |
|      270 |   83 | `	return SXRET_OK;` |
|   501895 |   84 | `}` |
|        - |   85 | `/*` |
|        - |   86 | ` * Compile a numeric [i.e: integer or real] literal.` |
|        - |   87 | ` * Notes on the integer type.` |
|        - |   88 | ` *  According to the PHP language reference manual` |
|        - |   89 | ` *  Integers can be specified in decimal (base 10), hexadecimal (base 16), octal (base 8)` |
|        - |   90 | ` *  or binary (base 2) notation, optionally preceded by a sign (- or +).` |
|        - |   91 | ` *  To use octal notation, precede the number with a 0 (zero). To use hexadecimal` |
|        - |   92 | ` *  notation precede the number with 0x. To use binary notation precede the number with 0b.` |
|        - |   93 | ` * Symisc eXtension to the integer type.` |
|        - |   94 | ` *  PH7 introduced platform-independant 64-bit integer unlike the standard PHP engine` |
|        - |   95 | ` *  where the size of an integer is platform-dependent.That is,the size of an integer` |
|        - |   96 | ` *  is 8 bytes and the maximum integer size is 0x7FFFFFFFFFFFFFFF for all platforms` |
|        - |   97 | ` *  [i.e: either 32bit or 64bit].` |
|        - |   98 | ` *  For more information on this powerfull extension please refer to the official` |
|        - |   99 | ` *  documentation.` |
|        - |  100 | ` */` |
|        - |  101 | `/*` |
|        - |  102 | ` * Determine whether an integer literal token exceeds the signed 64-bit range.` |
|        - |  103 | ` * PHP promotes such a literal to a float (e.g. 9223372036854775808 ->` |
|        - |  104 | ` * float(9.22...E+18), 0xFFFFFFFFFFFFFFFF -> float) rather than wrapping or` |
|        - |  105 | ` * dropping digits. pNum is the separator-stripped token (unsigned; the sign of` |
|        - |  106 | ` * a "-1" is a separate unary operator). Base detection mirrors` |
|        - |  107 | ` * PH7_TokenValueToInt64. Returns TRUE on overflow: for a non-decimal base the` |
|        - |  108 | ` * float value is accumulated into *pReal (dv = dv*base + digit); for decimal` |
|        - |  109 | ` * *pbDecimal is set so the caller reuses strtod on the token for a` |
|        - |  110 | ` * correctly-rounded value. Returns FALSE (value fits) for anything it cannot` |
|        - |  111 | ` * confidently classify, so the int path stays in charge.` |
|        - |  112 | ` *` |
|        - |  113 | ` * The int/float CLASSIFICATION is php-exact for every base. VALUES are byte-exact` |
|        - |  114 | ` * for decimal (strtod) and hex (php's zend_hex_strtod uses the same dv*16+digit` |
|        - |  115 | ` * doubling). Octal/binary overflow values can differ from php by the low bit(s):` |
|        - |  116 | ` * php's zend_{oct,bin}_strtod rounds differently than this doubling — e.g. php's` |
|        - |  117 | ` * binary 2**63 is 2**63-1024 whereas this returns the exact 2**63. Recorded as a` |
|        - |  118 | ` * residual; matching php exactly would need a port of those functions.` |
|        - |  119 | ` */` |
|   985706 |  120 | `static int GenStateIntLiteralOverflows(const SyString *pNum, ph7_real *pReal, int *pbDecimal)` |
|        5 |  121 | `{` |
|   985711 |  122 | `	const char *z = pNum->zString;` |
|   985711 |  123 | `	const char *zEnd = z + pNum->nByte;` |
|        - |  124 | `	const char *p, *q;` |
|        - |  125 | `	int n;` |
|   985711 |  126 | `	*pbDecimal = FALSE;` |
|   985711 |  127 | `	if( z >= zEnd ){` |
|      ! 0 |  128 | `		return FALSE;` |
|        - |  129 | `	}` |
|   985711 |  130 | `	if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|        - |  131 | `		/* Hexadecimal: INT64_MAX == 0x7FFF...F (16 digits, leading nibble 7). */` |
|      744 |  132 | `		p = z + 2;` |
|      820 |  133 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|     3124 |  134 | `		for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisHex(q[0]); q++ ){ n++; }` |
|      744 |  135 | `		if( n < 16 \|\| (n == 16 && SyHexToint(p[0]) < 8) ){` |
|      738 |  136 | `			return FALSE;` |
|        - |  137 | `		}` |
|        7 |  138 | `		{ ph7_real dv = 0;` |
|      103 |  139 | `		  for( q = p; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisHex(q[0]); q++ ){` |
|       97 |  140 | `			dv = dv * 16 + (ph7_real)SyHexToint(q[0]);` |
|       49 |  141 | `		  }` |
|        7 |  142 | `		  *pReal = dv;` |
|        - |  143 | `		}` |
|        7 |  144 | `		return TRUE;` |
|   984972 |  145 | `	}else if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|        - |  146 | `		/* Binary: INT64_MAX needs 63 significant bits. */` |
|      287 |  147 | `		p = z + 2;` |
|      335 |  148 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|     2172 |  149 | `		for( q = p, n = 0; q < zEnd && (q[0] == '0' \|\| q[0] == '1'); q++ ){ n++; }` |
|      287 |  150 | `		if( n <= 63 ){` |
|      285 |  151 | `			return FALSE;` |
|        - |  152 | `		}` |
|        3 |  153 | `		{ ph7_real dv = 0;` |
|      195 |  154 | `		  for( q = p; q < zEnd && (q[0] == '0' \|\| q[0] == '1'); q++ ){` |
|      129 |  155 | `			dv = dv * 2 + (ph7_real)(q[0] - '0');` |
|       65 |  156 | `		  }` |
|        3 |  157 | `		  *pReal = dv;` |
|        - |  158 | `		}` |
|        3 |  159 | `		return TRUE;` |
|   984686 |  160 | `	}else if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'o' \|\| z[1] == 'O') ){` |
|        - |  161 | `		/* PHP 8.1 explicit octal 0o/0O: 21 significant octal digits fit in int64. */` |
|       21 |  162 | `		p = z + 2;` |
|       25 |  163 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|       97 |  164 | `		for( q = p, n = 0; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){ n++; }` |
|       21 |  165 | `		if( n <= 21 ){` |
|       21 |  166 | `			return FALSE;` |
|        - |  167 | `		}` |
|      ! 0 |  168 | `		{ ph7_real dv = 0;` |
|      ! 0 |  169 | `		  for( q = p; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){` |
|      ! 0 |  170 | `			dv = dv * 8 + (ph7_real)(q[0] - '0');` |
|      ! 0 |  171 | `		  }` |
|      ! 0 |  172 | `		  *pReal = dv;` |
|        - |  173 | `		}` |
|      ! 0 |  174 | `		return TRUE;` |
|   984666 |  175 | `	}else if( z[0] == '0' ){` |
|        - |  176 | `		/* Octal: INT64_MAX == 0o777...7 (21 significant octal digits). Skip the` |
|        - |  177 | `		 * leading zeros (incl. the base '0'); a non-octal char such as the 8.1` |
|        - |  178 | `		 * "0o" marker ends the run and leaves it to the int path (as today). */` |
|   355819 |  179 | `		p = z;` |
|   711645 |  180 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|   381939 |  181 | `		for( q = p, n = 0; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){ n++; }` |
|   355819 |  182 | `		if( n <= 21 ){` |
|   355817 |  183 | `			return FALSE;` |
|        - |  184 | `		}` |
|        3 |  185 | `		{ ph7_real dv = 0;` |
|       47 |  186 | `		  for( q = p; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){` |
|       45 |  187 | `			dv = dv * 8 + (ph7_real)(q[0] - '0');` |
|       23 |  188 | `		  }` |
|        3 |  189 | `		  *pReal = dv;` |
|        - |  190 | `		}` |
|        3 |  191 | `		return TRUE;` |
|        - |  192 | `	}` |
|        - |  193 | `	/* Decimal: overflow iff more than 19 significant digits, or exactly 19 that` |
|        - |  194 | `	 * compare greater than INT64_MAX. Defer the value to strtod (via the caller)` |
|        - |  195 | `	 * for php-exact rounding. */` |
|   628852 |  196 | `	p = z;` |
|   628852 |  197 | `	while( p < zEnd && p[0] == '0' ){ p++; }` |
|  1620678 |  198 | `	for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisDigit(q[0]); q++ ){ n++; }` |
|   628852 |  199 | `	if( n > 19 \|\| (n == 19 && SyMemcmp(p, "9223372036854775807", 19) > 0) ){` |
|       27 |  200 | `		*pbDecimal = TRUE;` |
|       27 |  201 | `		return TRUE;` |
|        - |  202 | `	}` |
|   628826 |  203 | `	return FALSE;` |
|   492186 |  204 | `}` |
|  1005033 |  205 | `PH7_PRIVATE sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  206 | `{` |
|  1005038 |  207 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|  1005038 |  208 | `	sxu32 nIdx = 0;` |
|        - |  209 | `	char zScratch[GEN_NUM_SCRATCH];` |
|  1005038 |  210 | `	char *zAlloc = 0;` |
|        - |  211 | `	SyString sNum;` |
|        - |  212 | `	sxi32 rc;` |
|   501833 |  213 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|  1506871 |  214 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|   501833 |  215 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|  1005038 |  216 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  217 | `		return SXERR_ABORT;` |
|        - |  218 | `	}` |
|  1005038 |  219 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|        - |  220 | `		ph7_value *pObj;` |
|        - |  221 | `		sxi64 iValue;` |
|   985651 |  222 | `		ph7_real rOverflow = 0;` |
|   985651 |  223 | `		int bDecimalOverflow = 0;` |
|   985651 |  224 | `		if( GenStateIntLiteralOverflows(&sNum,&rOverflow,&bDecimalOverflow) ){` |
|        - |  225 | `			/* Literal exceeds the signed 64-bit range: PHP represents it as a` |
|        - |  226 | `			 * float instead of wrapping/dropping digits. */` |
|       37 |  227 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       37 |  228 | `			if( pObj == 0 ){` |
|      ! 0 |  229 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |  230 | `				if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|      ! 0 |  231 | `				return SXERR_ABORT;` |
|        - |  232 | `			}` |
|       37 |  233 | `			if( bDecimalOverflow ){` |
|        - |  234 | `				/* strtod on the decimal token yields php-exact rounding. */` |
|       27 |  235 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|       27 |  236 | `				PH7_MemObjToReal(pObj);` |
|       14 |  237 | `			}else{` |
|       11 |  238 | `				PH7_MemObjInitFromReal(pGen->pVm,pObj,rOverflow);` |
|        - |  239 | `			}` |
|       19 |  240 | `		}else{` |
|   985615 |  241 | `			iValue = PH7_TokenValueToInt64(&sNum);` |
|   985615 |  242 | `			pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|   985615 |  243 | `			if( pObj == 0 ){` |
|      ! 0 |  244 | `				if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|      ! 0 |  245 | `				return SXERR_ABORT;` |
|        - |  246 | `			}` |
|   985615 |  247 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|        - |  248 | `		}` |
|   492156 |  249 | `	}else{` |
|        - |  250 | `		/* Real number */` |
|        - |  251 | `		ph7_value *pObj;` |
|        - |  252 | `		/* Reserve a new constant */` |
|    19392 |  253 | `		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    19392 |  254 | `		if( pObj == 0 ){` |
|      ! 0 |  255 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |  256 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|      ! 0 |  257 | `			return SXERR_ABORT;` |
|        - |  258 | `		}` |
|    19392 |  259 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|    19392 |  260 | `		PH7_MemObjToReal(pObj);` |
|        - |  261 | `	}` |
|  1005038 |  262 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|        - |  263 | `	/* Emit the load constant instruction */` |
|  1005038 |  264 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        - |  265 | `	/* Node successfully compiled */` |
|  1005038 |  266 | `	return SXRET_OK;` |
|   501838 |  267 | `}` |
|        - |  268 | `/*` |
|        - |  269 | ` * Compile a single quoted string.` |
|        - |  270 | ` * According to the PHP language reference manual:` |
|        - |  271 | ` *` |
|        - |  272 | ` *   The simplest way to specify a string is to enclose it in single quotes (the character ' ).` |
|        - |  273 | ` *   To specify a literal single quote, escape it with a backslash (\). To specify a literal` |
|        - |  274 | ` *   backslash, double it (\\). All other instances of backslash will be treated as a literal` |
|        - |  275 | ` *   backslash: this means that the other escape sequences you might be used to, such as \r` |
|        - |  276 | ` *   or \n, will be output literally as specified rather than having any special meaning.` |
|        - |  277 | ` *` |
|        - |  278 | ` */` |
|   786277 |  279 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  280 | `{` |
|   786282 |  281 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|        - |  282 | `	const char *zIn,*zCur,*zEnd;` |
|        - |  283 | `	ph7_value *pObj;` |
|        - |  284 | `	sxu32 nIdx;` |
|        - |  285 | `	sxi32 bHasEsc;` |
|   786282 |  286 | `	nIdx = 0; /* Prevent compiler warning */` |
|        - |  287 | `	/* Delimit the string */` |
|   786282 |  288 | `	zIn  = pStr->zString;` |
|   786282 |  289 | `	zEnd = &zIn[pStr->nByte];` |
|   786282 |  290 | `	if( zIn >= zEnd ){` |
|        - |  291 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|        - |  292 | `		 * rather than reserving a new object each time. */` |
|   112399 |  293 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|   112399 |  294 | `		return SXRET_OK;` |
|        - |  295 | `	}` |
|        - |  296 | `	/* A single-quoted literal whose raw source holds a backslash unescapes to a` |
|        - |  297 | `	 * value that differs from that source (\\ -> \, \' -> '). The literal cache` |
|        - |  298 | `	 * keys FIND on the raw source text but INSTALL on the unescaped value, so` |
|        - |  299 | `	 * caching such a literal lets an unrelated one collide with it — e.g. '\\'` |
|        - |  300 | `	 * (raw \\, value \) would find the entry installed for '\\\\' (raw \\\\,` |
|        - |  301 | `	 * value \\) and load two backslashes. Only cache literals whose value equals` |
|        - |  302 | `	 * their source, i.e. those with no backslash to unescape. */` |
|   673888 |  303 | `	bHasEsc = 0;` |
|        - |  304 | `	{` |
|        - |  305 | `		const char *zScan;` |
| 12098137 |  306 | `		for( zScan = zIn ; zScan < zEnd ; zScan++ ){` |
| 11434932 |  307 | `			if( zScan[0] == '\\' ){ bHasEsc = 1; break; }` |
|  5700884 |  308 | `		}` |
|        - |  309 | `	}` |
|   673888 |  310 | `	if( !bHasEsc && SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|        - |  311 | `		/* Already processed,emit the load constant instruction` |
|        - |  312 | `		 * and return.` |
|        - |  313 | `		 */` |
|   309999 |  314 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   309999 |  315 | `		return SXRET_OK;` |
|        - |  316 | `	}` |
|        - |  317 | `	/* Reserve a new constant */` |
|   363894 |  318 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   363894 |  319 | `	if( pObj == 0 ){` |
|      ! 0 |  320 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |  321 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|      ! 0 |  322 | `		return SXERR_ABORT;` |
|        - |  323 | `	}` |
|   363894 |  324 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|        - |  325 | `	/* Compile the node */` |
|   368520 |  326 | `	for(;;){` |
|   738860 |  327 | `		if( zIn >= zEnd ){` |
|        - |  328 | `			/* End of input */` |
|   363894 |  329 | `			break;` |
|        - |  330 | `		}` |
|   374971 |  331 | `		zCur = zIn;` |
| 10927132 |  332 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
| 10552166 |  333 | `			zIn++;` |
|        5 |  334 | `		}` |
|   374971 |  335 | `		if( zIn > zCur ){` |
|        - |  336 | `			/* Append raw contents*/` |
|   374336 |  337 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   186709 |  338 | `		}` |
|   374971 |  339 | `		zIn++;` |
|   374971 |  340 | `		if( zIn < zEnd ){` |
|    11507 |  341 | `			if( zIn[0] == '\\' ){` |
|        - |  342 | `				/* A literal backslash */` |
|     9001 |  343 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|     7003 |  344 | `			}else if( zIn[0] == '\'' ){` |
|        - |  345 | `				/* A single quote */` |
|      102 |  346 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|       53 |  347 | `			}else{` |
|        - |  348 | `				/* verbatim copy */` |
|     2414 |  349 | `				zIn--;` |
|     2414 |  350 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char)*2);` |
|     2414 |  351 | `				zIn++;` |
|        - |  352 | `			}` |
|     5744 |  353 | `		}` |
|        - |  354 | `		/* Advance the stream cursor */` |
|   374971 |  355 | `		zIn++;` |
|        5 |  356 | `	}` |
|        - |  357 | `	/* Emit the load constant instruction */` |
|   363894 |  358 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   363894 |  359 | `	if( !bHasEsc && pStr->nByte < 1024 ){` |
|        - |  360 | `		/* Install in the literal table (only when value == source; see above) */` |
|   353216 |  361 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|   176162 |  362 | `	}` |
|        - |  363 | `	/* Node successfully compiled */` |
|   363894 |  364 | `	return SXRET_OK;` |
|   392342 |  365 | `}` |
|        - |  366 | `/*` |
|        - |  367 | ` * PHP 7.3 flexible heredoc/nowdoc closing-marker indent stripping.` |
|        - |  368 | ` *` |
|        - |  369 | ` * When the lexer matched the closing marker with leading whitespace on its` |
|        - |  370 | ` * own line, it stored the indent count in pGen->pIn->pUserData. The marker's` |
|        - |  371 | ` * indent prefix bytes sit immediately after the stripped body (at` |
|        - |  372 | ` * pIn->sData.zString + pIn->sData.nByte + 1 for LF, +2 for CRLF) in the` |
|        - |  373 | ` * original source buffer — the buffer is stable through compilation.` |
|        - |  374 | ` *` |
|        - |  375 | `` * For each body line, we remove exactly `nIndent` leading bytes that must`` |
|        - |  376 | ` * byte-for-byte match the marker's prefix. Empty lines (0 bytes or bare \r)` |
|        - |  377 | ` * bypass validation. Mismatches raise the exact PHP 7.3+ parse errors:` |
|        - |  378 | ` *   - "Invalid body indentation level (expecting an indentation level of` |
|        - |  379 | ` *     at least N)" — line too short, or first differing byte is not` |
|        - |  380 | ` *     whitespace.` |
|        - |  381 | ` *   - "Invalid indentation - tabs and spaces cannot be mixed" — first` |
|        - |  382 | ` *     differing byte is whitespace but differs from the marker prefix.` |
|        - |  383 | ` */` |
|      142 |  384 | `static sxi32 GenStateStripHeredocIndent(ph7_gen_state *pGen, SyString *pOut)` |
|        5 |  385 | `{` |
|      147 |  386 | `	SyString *pIn = &pGen->pIn->sData;` |
|      147 |  387 | `	sxu32 nIndent = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        - |  388 | `	const char *zPrefix;` |
|        - |  389 | `	const char *z, *zEnd;` |
|        - |  390 | `	char *zBuf, *zDst;` |
|      147 |  391 | `	if( nIndent == 0 ){` |
|        - |  392 | `		/* Legacy column-0 marker: zero-copy fast path */` |
|      101 |  393 | `		*pOut = *pIn;` |
|      101 |  394 | `		return SXRET_OK;` |
|        - |  395 | `	}` |
|        - |  396 | `	/* Recover the marker indent prefix from the original source buffer.` |
|        - |  397 | `	 * Skip the terminator the lexer stripped: one '\n' plus an optional` |
|        - |  398 | `	 * preceding '\r'. Note: when the body is empty (pIn->nByte == 0) the` |
|        - |  399 | `	 * lexer stripped nothing, so this offset is one byte past the true` |
|        - |  400 | `	 * marker-indent start. That is harmless — the strip loop below never` |
|        - |  401 | `	 * runs (z == zEnd), and zPrefix is never dereferenced. */` |
|       50 |  402 | `	zPrefix = pIn->zString + pIn->nByte;` |
|       50 |  403 | `	if( zPrefix[0] == '\r' && zPrefix[1] == '\n' ){` |
|      ! 0 |  404 | `		zPrefix += 2;` |
|      ! 0 |  405 | `	}else{` |
|       50 |  406 | `		zPrefix += 1;` |
|        - |  407 | `	}` |
|        - |  408 | `	/* Allocate scratch buffer sized to the original body (always enough). */` |
|       50 |  409 | `	zBuf = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator, pIn->nByte + 1);` |
|       50 |  410 | `	if( zBuf == 0 ){` |
|      ! 0 |  411 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|      ! 0 |  412 | `		return SXERR_ABORT;` |
|        - |  413 | `	}` |
|       50 |  414 | `	zDst = zBuf;` |
|       50 |  415 | `	z = pIn->zString;` |
|       50 |  416 | `	zEnd = z + pIn->nByte;` |
|      135 |  417 | `	while( z < zEnd ){` |
|       74 |  418 | `		const char *zLine = z;` |
|        - |  419 | `		sxu32 nLine;` |
|        - |  420 | `		int bEmpty;` |
|      816 |  421 | `		while( z < zEnd && z[0] != '\n' ){` |
|      746 |  422 | `			z++;` |
|        4 |  423 | `		}` |
|       74 |  424 | `		nLine = (sxu32)(z - zLine);` |
|       74 |  425 | `		bEmpty = (nLine == 0) \|\| (nLine == 1 && zLine[0] == '\r');` |
|       74 |  426 | `		if( !bEmpty ){` |
|        - |  427 | `			sxu32 i;` |
|       70 |  428 | `			if( nLine < nIndent ){` |
|      ! 0 |  429 | `				PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - |  430 | `					"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|      ! 0 |  431 | `					nIndent);` |
|      ! 0 |  432 | `				return SXERR_ABORT;` |
|        - |  433 | `			}` |
|      280 |  434 | `			for( i = 0; i < nIndent; i++ ){` |
|      222 |  435 | `				if( zLine[i] != zPrefix[i] ){` |
|       10 |  436 | `					unsigned char c = (unsigned char)zLine[i];` |
|       10 |  437 | `					if( c == ' ' \|\| c == '\t' ){` |
|        5 |  438 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - |  439 | `							"Invalid indentation - tabs and spaces cannot be mixed");` |
|        3 |  440 | `					}else{` |
|        7 |  441 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - |  442 | `							"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|        2 |  443 | `							nIndent);` |
|        - |  444 | `					}` |
|       10 |  445 | `					return SXERR_ABORT;` |
|        - |  446 | `				}` |
|      108 |  447 | `			}` |
|       60 |  448 | `			SyMemcpy((const void *)(zLine + nIndent), (void *)zDst, nLine - nIndent);` |
|       60 |  449 | `			zDst += nLine - nIndent;` |
|       34 |  450 | `		}else if( nLine == 1 ){` |
|        - |  451 | `			/* Preserve the stray '\r' on an otherwise empty line */` |
|      ! 0 |  452 | `			*zDst++ = '\r';` |
|      ! 0 |  453 | `		}` |
|       64 |  454 | `		if( z < zEnd ){` |
|       25 |  455 | `			*zDst++ = '\n';` |
|       25 |  456 | `			z++;` |
|       12 |  457 | `		}` |
|        2 |  458 | `	}` |
|       40 |  459 | `	pOut->zString = zBuf;` |
|       40 |  460 | `	pOut->nByte = (sxu32)(zDst - zBuf);` |
|       40 |  461 | `	return SXRET_OK;` |
|       76 |  462 | `}` |
|        - |  463 | `/*` |
|        - |  464 | ` * Compile a nowdoc string.` |
|        - |  465 | ` * According to the PHP language reference manual:` |
|        - |  466 | ` *` |
|        - |  467 | ` *  Nowdocs are to single-quoted strings what heredocs are to double-quoted strings.` |
|        - |  468 | ` *  A nowdoc is specified similarly to a heredoc, but no parsing is done inside a nowdoc.` |
|        - |  469 | ` *  The construct is ideal for embedding PHP code or other large blocks of text without the` |
|        - |  470 | ` *  need for escaping. It shares some features in common with the SGML <![CDATA[ ]]>` |
|        - |  471 | ` *  construct, in that it declares a block of text which is not for parsing.` |
|        - |  472 | ` *  A nowdoc is identified with the same <<< sequence used for heredocs, but the identifier` |
|        - |  473 | ` *  which follows is enclosed in single quotes, e.g. <<<'EOT'. All the rules for heredoc` |
|        - |  474 | ` *  identifiers also apply to nowdoc identifiers, especially those regarding the appearance` |
|        - |  475 | ` *  of the closing identifier.` |
|        - |  476 | ` */` |
|       62 |  477 | `PH7_PRIVATE sxi32 PH7_CompileNowDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  478 | `{` |
|        - |  479 | `	SyString sStripped;` |
|        - |  480 | `	SyString *pStr;` |
|        - |  481 | `	ph7_value *pObj;` |
|        - |  482 | `	sxu32 nIdx;` |
|        - |  483 | `	sxi32 rc;` |
|       67 |  484 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|       67 |  485 | `	if( rc != SXRET_OK ){` |
|        6 |  486 | `		return rc;` |
|        - |  487 | `	}` |
|       62 |  488 | `	pStr = &sStripped;` |
|       62 |  489 | `	nIdx = 0; /* Prevent compiler warning */` |
|       62 |  490 | `	if( pStr->nByte <= 0 ){` |
|        - |  491 | `		/* An empty nowdoc is the empty STRING, like '' -- loading NULL here made` |
|        - |  492 | `		 * strlen(<<<'EOD'EOD;) deprecation-warn about a null argument. */` |
|        7 |  493 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|        7 |  494 | `		return SXRET_OK;` |
|        - |  495 | `	}` |
|        - |  496 | `	/* Reserve a new constant */` |
|       56 |  497 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       56 |  498 | `	if( pObj == 0 ){` |
|      ! 0 |  499 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|      ! 0 |  500 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|      ! 0 |  501 | `		return SXERR_ABORT;` |
|        - |  502 | `	}` |
|        - |  503 | `	/* No processing is done here, simply a memcpy() operation */` |
|       56 |  504 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,pStr);` |
|        - |  505 | `	/* Emit the load constant instruction */` |
|       56 |  506 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        - |  507 | `	/* Node successfully compiled */` |
|       56 |  508 | `	return SXRET_OK;` |
|       36 |  509 | `}` |
|        - |  510 | `/*` |
|        - |  511 | ` * Process variable expression [i.e: "$var","${var}"] embedded in a double quoted/heredoc string.` |
|        - |  512 | ` * According to the PHP language reference manual` |
|        - |  513 | ` *   When a string is specified in double quotes or with heredoc,variables are parsed within it.` |
|        - |  514 | ` *  There are two types of syntax: a simple one and a complex one. The simple syntax is the most` |
|        - |  515 | ` *  common and convenient. It provides a way to embed a variable, an array value, or an object` |
|        - |  516 | ` *  property in a string with a minimum of effort.` |
|        - |  517 | ` *  Simple syntax` |
|        - |  518 | ` *   If a dollar sign ($) is encountered, the parser will greedily take as many tokens as possible` |
|        - |  519 | ` *   to form a valid variable name. Enclose the variable name in curly braces to explicitly specify` |
|        - |  520 | ` *   the end of the name.` |
|        - |  521 | ` *   Similarly, an array index or an object property can be parsed. With array indices, the closing` |
|        - |  522 | ` *   square bracket (]) marks the end of the index. The same rules apply to object properties` |
|        - |  523 | ` *   as to simple variables.` |
|        - |  524 | ` *  Complex (curly) syntax` |
|        - |  525 | ` *   This isn't called complex because the syntax is complex, but because it allows for the use` |
|        - |  526 | ` *   of complex expressions.` |
|        - |  527 | ` *   Any scalar variable, array element or object property with a string representation can be` |
|        - |  528 | ` *   included via this syntax. Simply write the expression the same way as it would appear outside` |
|        - |  529 | ` *   the string, and then wrap it in { and }. Since { can not be escaped, this syntax will only` |
|        - |  530 | ` *   be recognised when the $ immediately follows the {. Use {\$ to get a literal {$` |
|        - |  531 | ` */` |
|     7139 |  532 | `static sxi32 GenStateProcessStringExpression(` |
|        - |  533 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - |  534 | `	sxu32 nLine,         /* Line number */` |
|        - |  535 | `	const char *zIn,     /* Raw expression */` |
|        - |  536 | `	const char *zEnd     /* End of the expression */` |
|        - |  537 | `	)` |
|        5 |  538 | `{` |
|        - |  539 | `	SyToken *pTmpIn,*pTmpEnd;` |
|        - |  540 | `	SySet sToken;` |
|        - |  541 | `	sxi32 rc;` |
|        - |  542 | `	/* Initialize the token set */` |
|     7144 |  543 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|        - |  544 | `	/* Preallocate some slots */` |
|     7144 |  545 | `	SySetAlloc(&sToken,0x08);` |
|        - |  546 | `	/* Tokenize the text */` |
|     7144 |  547 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken,0);` |
|        - |  548 | `	/* Swap delimiter */` |
|     7144 |  549 | `	pTmpIn  = pGen->pIn;` |
|     7144 |  550 | `	pTmpEnd = pGen->pEnd;` |
|     7144 |  551 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|     7144 |  552 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|        - |  553 | ``	/* Compile the expression. An interpolated `"...$x..."` READS $x — php warns`` |
|        - |  554 | `	 * "Undefined variable $x" and substitutes the empty string — so ask for a` |
|        - |  555 | `	 * read-only load rather than letting the default vivify it silently. */` |
|     7144 |  556 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|        - |  557 | `	/* Restore token stream */` |
|     7144 |  558 | `	pGen->pIn  = pTmpIn;` |
|     7144 |  559 | `	pGen->pEnd = pTmpEnd;` |
|        - |  560 | `	/* Release the token set */` |
|     7144 |  561 | `	SySetRelease(&sToken);` |
|        - |  562 | `	/* Compilation result */` |
|     7144 |  563 | `	return rc;` |
|        5 |  564 | `}` |
|        - |  565 | `/*` |
|        - |  566 | ` * Line number of a POSITION inside the string body being compiled: the` |
|        - |  567 | ` * token's line plus every newline before it. php reports the offending` |
|        - |  568 | ` * construct's own line, not the string's opening line, so every diagnostic` |
|        - |  569 | ` * raised from inside a body -- an escape sequence, a malformed subscript --` |
|        - |  570 | ` * goes through here. A heredoc body starts on the line after the '<<<'` |
|        - |  571 | ` * marker, hence the +1.` |
|        - |  572 | ` */` |
|       54 |  573 | `static sxu32 GenStateStringEscLine(ph7_gen_state *pGen,const char *zPos,int bHeredoc)` |
|        5 |  574 | `{` |
|       59 |  575 | `	const char *z = pGen->pIn->sData.zString;` |
|       59 |  576 | `	sxu32 nLine = pGen->pIn->nLine + (bHeredoc ? 1 : 0);` |
|      209 |  577 | `	for( ; z < zPos ; z++ ){` |
|      155 |  578 | `		if( z[0] == '\n' ){` |
|      ! 0 |  579 | `			nLine++;` |
|      ! 0 |  580 | `		}` |
|       80 |  581 | `	}` |
|       59 |  582 | `	return nLine;` |
|        5 |  583 | `}` |
|        - |  584 | `/*` |
|        - |  585 | ` * TRUE when c can OPEN a php label — the same byte class the engine's identifier` |
|        - |  586 | ` * scanner uses (LEX_LABEL_START in lex.c): [a-zA-Z_\x80-\xff].` |
|        - |  587 | ` */` |
|        - |  588 | `#define GEN_STRING_LABEL_START(c) \` |
|        - |  589 | `	( (unsigned char)(c) >= 0x80 \|\| SyisAlpha(c) \|\| (c) == '_' )` |
|        - |  590 | `/*` |
|        - |  591 | ` * Advance *pz over a php LABEL — the name half of "$name" and of the "->name"` |
|        - |  592 | ` * accessor inside a double-quoted string or a heredoc body. php's label is` |
|        - |  593 | ` * [a-zA-Z_\x80-\xff][a-zA-Z0-9_\x80-\xff]*, a flat byte set: a multibyte name` |
|        - |  594 | ` * is consumed because every byte of it is >= 0x80, no UTF-8 decoding involved.` |
|        - |  595 | ` * Stops at *pz when the cursor is not on a label byte.` |
|        - |  596 | ` */` |
|     7044 |  597 | `static void GenStateSkipStringLabel(const char **pz,const char *zEnd)` |
|        5 |  598 | `{` |
|     7049 |  599 | `	const char *zIn = *pz;` |
|    21340 |  600 | `	while( zIn < zEnd` |
|    28726 |  601 | `		&& ((unsigned char)zIn[0] >= 0x80 \|\| SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_') ){` |
|    21682 |  602 | `		zIn++;` |
|        5 |  603 | `	}` |
|     7049 |  604 | `	*pz = zIn;` |
|     7049 |  605 | `}` |
|        - |  606 | `/*` |
|        - |  607 | ` * Scan one php INTEGER literal at z in the flavour php's simple-syntax subscript` |
|        - |  608 | ` * accepts: LNUM, HNUM (0x...), BNUM (0b...) or ONUM (0o...), each allowing '_'` |
|        - |  609 | ` * separators BETWEEN digits. There is no float and no exponent in this grammar --` |
|        - |  610 | ` * "$a[1.5]" and "$a[1e2]" are php parse errors. Returns the byte after the` |
|        - |  611 | ` * literal, or z itself when the cursor is not on one.` |
|        - |  612 | ` */` |
|       70 |  613 | `static const char * GenStateScanOffsetNumber(const char *z,const char *zEnd)` |
|        1 |  614 | `{` |
|       71 |  615 | `	const char *zStart = z;` |
|       71 |  616 | `	int base = 10;` |
|       71 |  617 | `	if( z >= zEnd \|\| !GenStateIsBaseDigit((unsigned char)z[0],10) ){` |
|      ! 0 |  618 | `		return z;` |
|        - |  619 | `	}` |
|       71 |  620 | `	if( z[0] == '0' && &z[1] < zEnd ){` |
|       23 |  621 | `		int b = 0;` |
|       23 |  622 | `		if( z[1] == 'x' \|\| z[1] == 'X' ){` |
|        7 |  623 | `			b = 16;` |
|       20 |  624 | `		}else if( z[1] == 'b' \|\| z[1] == 'B' ){` |
|        3 |  625 | `			b = 2;` |
|       16 |  626 | `		}else if( z[1] == 'o' \|\| z[1] == 'O' ){` |
|        3 |  627 | `			b = 8;` |
|        1 |  628 | `		}` |
|        - |  629 | `		/* A prefix with no digit behind it is not a literal: php then matches the` |
|        - |  630 | `		 * lone "0" and lexes the rest as a label ("$a[0x]" is a parse error). */` |
|       23 |  631 | `		if( b && &z[2] < zEnd && GenStateIsBaseDigit((unsigned char)z[2],b) ){` |
|        9 |  632 | `			base = b;` |
|        9 |  633 | `			z += 2;` |
|        4 |  634 | `		}` |
|       11 |  635 | `	}` |
|      353 |  636 | `	while( z < zEnd ){` |
|      297 |  637 | `		if( GenStateIsBaseDigit((unsigned char)z[0],base) ){` |
|      279 |  638 | `			z++;` |
|      279 |  639 | `			continue;` |
|        - |  640 | `		}` |
|       18 |  641 | `		if( z[0] == '_' && z > zStart && GenStateIsBaseDigit((unsigned char)z[-1],base)` |
|        9 |  642 | `			&& &z[1] < zEnd && GenStateIsBaseDigit((unsigned char)z[1],base) ){` |
|        5 |  643 | `			z += 2;` |
|        5 |  644 | `			continue;` |
|        - |  645 | `		}` |
|       15 |  646 | `		break;` |
|      ! 0 |  647 | `	}` |
|       71 |  648 | `	return z;` |
|       36 |  649 | `}` |
|        - |  650 | `/*` |
|        - |  651 | ` * TRUE when the digit run [z,zEnd) is php's CANONICAL spelling of an INTEGER` |
|        - |  652 | ` * offset: "0", or [1-9][0-9]* that fits a signed 64-bit int. php carries every` |
|        - |  653 | ` * other spelling -- leading zeros, a base prefix, '_' separators, a magnitude` |
|        - |  654 | ` * past the int range -- as the raw TEXT, i.e. a STRING key. (zend also spells` |
|        - |  655 | ` * out any 19-digit run, but its hashmap folds that straight back to an integer` |
|        - |  656 | ` * key, so the two agree on everything an array can observe.)` |
|        - |  657 | ` */` |
|       56 |  658 | `static int GenStateOffsetIsCanonicalInt(const char *z,const char *zEnd,int bNeg)` |
|        1 |  659 | `{` |
|       57 |  660 | `	sxu32 n = (sxu32)(zEnd - z);` |
|        - |  661 | `	sxu32 i;` |
|       57 |  662 | `	if( n < 1 ){` |
|      ! 0 |  663 | `		return FALSE;` |
|        - |  664 | `	}` |
|       57 |  665 | `	if( z[0] == '0' ){` |
|        - |  666 | `		/* "0" alone is the integer key 0; "-0", "00" and "007" are text */` |
|       33 |  667 | `		return n == 1 && !bNeg;` |
|        - |  668 | `	}` |
|      231 |  669 | `	for( i = 0 ; i < n ; ++i ){` |
|      209 |  670 | `		if( !GenStateIsBaseDigit((unsigned char)z[i],10) ){` |
|        3 |  671 | `			return FALSE;` |
|        - |  672 | `		}` |
|      104 |  673 | `	}` |
|        - |  674 | `	/* INT64_MAX bounds BOTH signs here, not INT64_MIN: the rewrite re-emits a` |
|        - |  675 | `	 * canonical offset as SOURCE, and no php expression can spell INT64_MIN as a` |
|        - |  676 | `	 * literal (the '-' is unary minus over an out-of-range literal, which` |
|        - |  677 | `	 * promotes to a float). "-9223372036854775808" therefore takes the string` |
|        - |  678 | `	 * path, where the hashmap's numeric-string rule folds it back to the integer` |
|        - |  679 | `	 * key -- and where an ArrayAccess offsetGet() receives php's own string. */` |
|       23 |  680 | `	if( n > 19 \|\| (n == 19 && SyMemcmp(z,"9223372036854775807",19) > 0) ){` |
|        7 |  681 | `		return FALSE;` |
|        - |  682 | `	}` |
|       17 |  683 | `	return TRUE;` |
|       29 |  684 | `}` |
|        - |  685 | `/*` |
|        - |  686 | ` * php's parse error for a malformed simple-syntax subscript. zBad points at the` |
|        - |  687 | ` * first byte php would refuse; iExpect picks which of php's "expecting" tails` |
|        - |  688 | ` * applies -- 1 after an otherwise good offset, 2 after a lone '-', 0 at the` |
|        - |  689 | ` * offset's start. Always returns SXERR_ABORT so the caller can just pass it on.` |
|        - |  690 | ` */` |
|       34 |  691 | `static sxi32 GenStateOffsetSyntaxError(ph7_gen_state *pGen,const char *zBad,const char *zEnd,int iExpect,int bHeredoc)` |
|        1 |  692 | `{` |
|        - |  693 | `	SyString sTok;` |
|       35 |  694 | `	sxu32 n = (sxu32)(zEnd - zBad);` |
|       35 |  695 | `	if( n < 1 ){` |
|        - |  696 | `		/* Empty offset: name the ']' that zEnd points at */` |
|        3 |  697 | `		n = 1;` |
|        1 |  698 | `	}` |
|       35 |  699 | `	if( n > 16 ){` |
|      ! 0 |  700 | `		n = 16;` |
|      ! 0 |  701 | `	}` |
|       35 |  702 | `	SyStringInitFromBuf(&sTok,zBad,n);` |
|       35 |  703 | `	if( iExpect == 1 ){` |
|       19 |  704 | `		PH7_GenCompileError(&(*pGen),E_PARSE,GenStateStringEscLine(&(*pGen),zBad,bHeredoc),` |
|        - |  705 | `			"syntax error, unexpected token \"%z\", expecting \"]\"",&sTok);` |
|       26 |  706 | `	}else if( iExpect == 2 ){` |
|        5 |  707 | `		PH7_GenCompileError(&(*pGen),E_PARSE,GenStateStringEscLine(&(*pGen),zBad,bHeredoc),` |
|        - |  708 | `			"syntax error, unexpected token \"%z\", expecting number",&sTok);` |
|        3 |  709 | `	}else{` |
|       13 |  710 | `		PH7_GenCompileError(&(*pGen),E_PARSE,GenStateStringEscLine(&(*pGen),zBad,bHeredoc),` |
|        - |  711 | `			"syntax error, unexpected token \"%z\", expecting \"-\" or identifier or variable or number",&sTok);` |
|        - |  712 | `	}` |
|       35 |  713 | `	return SXERR_ABORT;` |
|        1 |  714 | `}` |
|        - |  715 | `/*` |
|        - |  716 | ` * Compile the SUBSCRIPT of a simple-syntax "$name[offset]" interpolation:` |
|        - |  717 | ` * [zKey,zKeyEnd) is the raw text between the brackets, and the php-equivalent` |
|        - |  718 | ` * "[...]" source is appended to pOut.` |
|        - |  719 | ` *` |
|        - |  720 | `` * php does NOT parse this as an expression. zend's `encaps_var_offset` grammar`` |
|        - |  721 | ` * admits exactly four things and nothing else -- a bare LABEL (always the STRING` |
|        - |  722 | ` * key, never a constant), an integer literal, '-' plus an integer literal, or a` |
|        - |  723 | ` * "$name" -- and only a canonical decimal is an INTEGER key. PH7 handed the text` |
|        - |  724 | ` * to the expression compiler, which read every integer SPELLING as a number and` |
|        - |  725 | ` * accepted shapes php rejects outright.` |
|        - |  726 | ` */` |
|      116 |  727 | `static sxi32 GenStateCompileStringOffset(` |
|        - |  728 | `	ph7_gen_state *pGen,` |
|        - |  729 | `	const char *zKey,` |
|        - |  730 | `	const char *zKeyEnd,` |
|        - |  731 | `	SyBlob *pOut,` |
|        - |  732 | `	int bHeredoc` |
|        - |  733 | `	)` |
|        3 |  734 | `{` |
|      119 |  735 | `	const char *z = zKey;` |
|      119 |  736 | `	int bNeg = 0;` |
|      119 |  737 | `	if( z >= zKeyEnd ){` |
|        3 |  738 | `		return GenStateOffsetSyntaxError(&(*pGen),z,zKeyEnd,0,bHeredoc);` |
|        - |  739 | `	}` |
|      117 |  740 | `	if( z[0] == '$' ){` |
|        - |  741 | `		/* "$name" -- the one offset php actually EVALUATES; pass it through */` |
|        9 |  742 | `		const char *zName = &z[1];` |
|        9 |  743 | `		if( zName >= zKeyEnd \|\| !GEN_STRING_LABEL_START(zName[0]) ){` |
|        3 |  744 | `			return GenStateOffsetSyntaxError(&(*pGen),z,zKeyEnd,0,bHeredoc);` |
|        - |  745 | `		}` |
|        7 |  746 | `		z = zName;` |
|        7 |  747 | `		GenStateSkipStringLabel(&z,zKeyEnd);` |
|        7 |  748 | `		if( z != zKeyEnd ){` |
|        3 |  749 | `			return GenStateOffsetSyntaxError(&(*pGen),z,zKeyEnd,1,bHeredoc);` |
|        - |  750 | `		}` |
|        5 |  751 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 |  752 | `		SyBlobAppend(pOut,zKey,(sxu32)(zKeyEnd - zKey));` |
|        5 |  753 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        5 |  754 | `		return SXRET_OK;` |
|        - |  755 | `	}` |
|      109 |  756 | `	if( z[0] == '-' ){` |
|       15 |  757 | `		bNeg = 1;` |
|       15 |  758 | `		z++;` |
|        7 |  759 | `	}` |
|      109 |  760 | `	if( z < zKeyEnd && GenStateIsBaseDigit((unsigned char)z[0],10) ){` |
|       71 |  761 | `		const char *zNum = z;` |
|       71 |  762 | `		z = GenStateScanOffsetNumber(z,zKeyEnd);` |
|       71 |  763 | `		if( z != zKeyEnd ){` |
|       15 |  764 | `			return GenStateOffsetSyntaxError(&(*pGen),z,zKeyEnd,1,bHeredoc);` |
|        - |  765 | `		}` |
|        - |  766 | `		/* "-0" is php's string key "-0", not the integer 0: zend negates a LONG` |
|        - |  767 | `		 * num-string but spells a ZERO one back out as text. */` |
|       57 |  768 | `		if( GenStateOffsetIsCanonicalInt(zNum,zKeyEnd,bNeg) ){` |
|       29 |  769 | `			SyBlobAppend(pOut,"[",sizeof(char));` |
|       29 |  770 | `			SyBlobAppend(pOut,zKey,(sxu32)(zKeyEnd - zKey));` |
|       29 |  771 | `			SyBlobAppend(pOut,"]",sizeof(char));` |
|       15 |  772 | `		}else{` |
|       29 |  773 | `			SyBlobAppend(pOut,"['",sizeof(char)*2);` |
|       29 |  774 | `			SyBlobAppend(pOut,zKey,(sxu32)(zKeyEnd - zKey));` |
|       29 |  775 | `			SyBlobAppend(pOut,"']",sizeof(char)*2);` |
|        - |  776 | `		}` |
|       57 |  777 | `		return SXRET_OK;` |
|        - |  778 | `	}` |
|       39 |  779 | `	if( bNeg ){` |
|        - |  780 | `		/* php's '-' takes a NUMBER and nothing else */` |
|        5 |  781 | `		return GenStateOffsetSyntaxError(&(*pGen),z,zKeyEnd,2,bHeredoc);` |
|        - |  782 | `	}` |
|       35 |  783 | `	if( GEN_STRING_LABEL_START(z[0]) ){` |
|       27 |  784 | `		GenStateSkipStringLabel(&z,zKeyEnd);` |
|       27 |  785 | `		if( z != zKeyEnd ){` |
|        3 |  786 | `			return GenStateOffsetSyntaxError(&(*pGen),z,zKeyEnd,1,bHeredoc);` |
|        - |  787 | `		}` |
|        - |  788 | `		/* A bare word is the STRING key, never a constant */` |
|       25 |  789 | `		SyBlobAppend(pOut,"['",sizeof(char)*2);` |
|       25 |  790 | `		SyBlobAppend(pOut,zKey,(sxu32)(zKeyEnd - zKey));` |
|       25 |  791 | `		SyBlobAppend(pOut,"']",sizeof(char)*2);` |
|       25 |  792 | `		return SXRET_OK;` |
|        - |  793 | `	}` |
|        9 |  794 | `	return GenStateOffsetSyntaxError(&(*pGen),z,zKeyEnd,0,bHeredoc);` |
|       61 |  795 | `}` |
|        - |  796 | `/*` |
|        - |  797 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|        - |  798 | ` */` |
|    90313 |  799 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|        5 |  800 | `{` |
|        - |  801 | `	ph7_value *pConstObj;` |
|    90318 |  802 | `	sxu32 nIdx = 0;` |
|        - |  803 | `	/* Reserve a new constant */` |
|    90318 |  804 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    90318 |  805 | `	if( pConstObj == 0 ){` |
|      ! 0 |  806 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|      ! 0 |  807 | `		return 0;` |
|        - |  808 | `	}` |
|    90318 |  809 | `	(*pCount)++;` |
|    90318 |  810 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|        - |  811 | `	/* Emit the load constant instruction */` |
|    90318 |  812 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    90318 |  813 | `	return pConstObj;` |
|    45058 |  814 | `}` |
|        - |  815 | `/*` |
|        - |  816 | ` * Compile a double quoted/heredoc string.` |
|        - |  817 | ` * According to the PHP language reference manual` |
|        - |  818 | ` * Heredoc` |
|        - |  819 | ` *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier` |
|        - |  820 | ` *  is provided, then a newline. The string itself follows, and then the same identifier again` |
|        - |  821 | ` *  to close the quotation.` |
|        - |  822 | ` *  The closing identifier must begin in the first column of the line. Also, the identifier must` |
|        - |  823 | ` *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric` |
|        - |  824 | ` *  characters and underscores, and must start with a non-digit character or underscore.` |
|        - |  825 | ` *  Warning` |
|        - |  826 | ` *  It is very important to note that the line with the closing identifier must contain` |
|        - |  827 | ` *  no other characters, except possibly a semicolon (;). That means especially that the identifier` |
|        - |  828 | ` *  may not be indented, and there may not be any spaces or tabs before or after the semicolon.` |
|        - |  829 | ` *  It's also important to realize that the first character before the closing identifier must` |
|        - |  830 | ` *  be a newline as defined by the local operating system. This is \n on UNIX systems, including Mac OS X.` |
|        - |  831 | ` *  The closing delimiter (possibly followed by a semicolon) must also be followed by a newline.` |
|        - |  832 | ` *  If this rule is broken and the closing identifier is not "clean", it will not be considered a closing` |
|        - |  833 | ` *  identifier, and PHP will continue looking for one. If a proper closing identifier is not found before` |
|        - |  834 | ` *  the end of the current file, a parse error will result at the last line.` |
|        - |  835 | ` *  Heredocs can not be used for initializing class properties.` |
|        - |  836 | ` * Double quoted` |
|        - |  837 | ` *  If the string is enclosed in double-quotes ("), PHP will interpret more escape sequences for special characters:` |
|        - |  838 | ` *  Escaped characters Sequence 	Meaning` |
|        - |  839 | ` *  \n linefeed (LF or 0x0A (10) in ASCII)` |
|        - |  840 | ` *  \r carriage return (CR or 0x0D (13) in ASCII)` |
|        - |  841 | ` *  \t horizontal tab (HT or 0x09 (9) in ASCII)` |
|        - |  842 | ` *  \v vertical tab (VT or 0x0B (11) in ASCII)` |
|        - |  843 | ` *  \e escape (ESC or 0x1B (27) in ASCII)` |
|        - |  844 | ` *  \f form feed (FF or 0x0C (12) in ASCII)` |
|        - |  845 | ` *  \\ backslash` |
|        - |  846 | ` *  \$ dollar sign` |
|        - |  847 | ` *  \" double-quote` |
|        - |  848 | ` *  \[0-7]{1,3} 	the sequence of characters matching the regular expression is a character in octal notation,` |
|        - |  849 | ` *      which silently overflows to fit in a byte (e.g. "\400" === "\000")` |
|        - |  850 | ` *  \x[0-9A-Fa-f]{1,2} 	the sequence of characters matching the regular expression is a character in hexadecimal notation` |
|        - |  851 | ` *  \u{[0-9A-Fa-f]+} 	the sequence of characters matching the regular expression is a Unicode codepoint,` |
|        - |  852 | ` *      which will be output to the string as that codepoint's UTF-8 representation` |
|        - |  853 | ` * As in single quoted strings, escaping any other character will result in the backslash being printed too.` |
|        - |  854 | ` * (The PH7-ism "\oNNN" octal form is gone: a literal "\o" now round-trips like php 8.)` |
|        - |  855 | ` * The most important feature of double-quoted strings is the fact that variable names will be expanded.` |
|        - |  856 | ` * See string parsing for details.` |
|        - |  857 | ` */` |
|        - |  858 | `/* bHeredoc: php strips the backslash from '\"' only when '"' is the active` |
|        - |  859 | ` * quote character; a heredoc has none, so '\"' stays verbatim there. */` |
|    88358 |  860 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen,int bHeredoc)` |
|        5 |  861 | `{` |
|    88363 |  862 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|        - |  863 | `	const char *zIn,*zCur,*zEnd;` |
|    88363 |  864 | `	ph7_value *pObj = 0;` |
|        - |  865 | `	sxi32 iCons;` |
|        - |  866 | `	sxi32 nInterp;   /* how many of iCons came from an interpolated EXPRESSION */` |
|        - |  867 | `	sxi32 rc;` |
|        - |  868 | `	/* Delimit the string */` |
|    88363 |  869 | `	zIn  = pStr->zString;` |
|    88363 |  870 | `	zEnd = &zIn[pStr->nByte];` |
|    88363 |  871 | `	if( zIn >= zEnd ){` |
|        - |  872 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|        - |  873 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|        - |  874 | `		 * literal table from growing when many "" literals appear in the source.` |
|        - |  875 | `		 */` |
|     2095 |  876 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     2095 |  877 | `		return SXRET_OK;` |
|        - |  878 | `	}` |
|    86273 |  879 | `	zCur = 0;` |
|        - |  880 | `	/* Compile the node */` |
|    86273 |  881 | `	iCons = 0;` |
|    86273 |  882 | `	nInterp = 0;` |
|    46553 |  883 | `	for(;;){` |
|   143525 |  884 | `		zCur = zIn;` |
|  1220259 |  885 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  1083914 |  886 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|      132 |  887 | `				break;` |
|  1083679 |  888 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|     6958 |  889 | `				(GEN_STRING_LABEL_START(zIn[1]) \|\| zIn[1] == '{') ){` |
|     3448 |  890 | `					break;` |
|        - |  891 | `			}` |
|  1076739 |  892 | `			zIn++;` |
|        5 |  893 | `		}` |
|   143525 |  894 | `		if( zIn > zCur ){` |
|    71188 |  895 | `			if( pObj == 0 ){` |
|    64934 |  896 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    64934 |  897 | `				if( pObj == 0 ){` |
|      ! 0 |  898 | `					return SXERR_ABORT;` |
|        - |  899 | `				}` |
|    32376 |  900 | `			}` |
|    71188 |  901 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    35491 |  902 | `		}` |
|   143525 |  903 | `		if( zIn >= zEnd ){` |
|    86235 |  904 | `			break;` |
|        - |  905 | `		}` |
|    57295 |  906 | `		if( zIn[0] == '\\' ){` |
|    50120 |  907 | `			const char *zPtr = 0;` |
|        - |  908 | `			sxu32 n;` |
|    50120 |  909 | `			zIn++;` |
|    50120 |  910 | `			if( pObj == 0 ){` |
|    25389 |  911 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    25389 |  912 | `				if( pObj == 0 ){` |
|      ! 0 |  913 | `					return SXERR_ABORT;` |
|        - |  914 | `				}` |
|    12677 |  915 | `			}` |
|    50120 |  916 | `			if( zIn >= zEnd ){` |
|        - |  917 | `				/* Lone backslash at the very end of the body: php keeps it */` |
|        3 |  918 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|        3 |  919 | `				break;` |
|        - |  920 | `			}` |
|    50118 |  921 | `			n = sizeof(char); /* size of conversion */` |
|    50118 |  922 | `			switch( zIn[0] ){` |
|      381 |  923 | `			case '$':` |
|        - |  924 | `				/* Dollar sign */` |
|      767 |  925 | `				PH7_MemObjStringAppend(pObj,"$",sizeof(char));` |
|      767 |  926 | `				break;` |
|      159 |  927 | `			case '\\':` |
|        - |  928 | `				/* A literal backslash */` |
|      323 |  929 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|      323 |  930 | `				break;` |
|        1 |  931 | `			case 'e':` |
|        - |  932 | `				/* Escape (ESC) ASCII code 27 */` |
|        3 |  933 | `				PH7_MemObjStringAppend(pObj,"\x1b",sizeof(char));` |
|        3 |  934 | `				break;` |
|       13 |  935 | `			case 'f':` |
|        - |  936 | `				/* Form-feed (FF)[ctrl+l] ASCII code 12 */` |
|       28 |  937 | `				PH7_MemObjStringAppend(pObj,"\f",sizeof(char));` |
|       28 |  938 | `				break;` |
|    20374 |  939 | `			case 'n':` |
|        - |  940 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|    40645 |  941 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|    40645 |  942 | `				break;` |
|      996 |  943 | `			case 'r':` |
|        - |  944 | `				/* Carriage return (CR)[ctrl+m] ASCII code 13 */` |
|     1996 |  945 | `				PH7_MemObjStringAppend(pObj,"\r",sizeof(char));` |
|     1996 |  946 | `				break;` |
|      132 |  947 | `			case 't':` |
|        - |  948 | `				/* Horizontal tab (HT)[ctrl+i] ASCII code 9 */` |
|      268 |  949 | `				PH7_MemObjStringAppend(pObj,"\t",sizeof(char));` |
|      268 |  950 | `				break;` |
|       18 |  951 | `			case 'v':` |
|        - |  952 | `				/* Vertical tab(VT)[ctrl+k] ASCII code 11 */` |
|       38 |  953 | `				PH7_MemObjStringAppend(pObj,"\v",sizeof(char));` |
|       38 |  954 | `				break;` |
|      558 |  955 | `			case '"':` |
|     1121 |  956 | `				if( bHeredoc ){` |
|        - |  957 | `					/* No active quote char in a heredoc: php keeps \" verbatim */` |
|        5 |  958 | `					PH7_MemObjStringAppend(pObj,"\\\"",sizeof(char)*2);` |
|        3 |  959 | `				}else{` |
|        - |  960 | `					/* Double quote */` |
|     1117 |  961 | `					PH7_MemObjStringAppend(pObj,"\"",sizeof(char));` |
|        - |  962 | `				}` |
|     1121 |  963 | `				break;` |
|      330 |  964 | `			case '0': case '1': case '2': case '3':` |
|        - |  965 | `			case '4': case '5': case '6': case '7': {` |
|        - |  966 | `				/* \[0-7]{1,3}: a character in octal notation. A value above \377` |
|        - |  967 | `				 * warns and wraps to the low byte, matching php 8. */` |
|      640 |  968 | `				int c = 0;` |
|        - |  969 | `				char cOut;` |
|     1385 |  970 | `				for( zPtr = zIn ; zPtr < &zIn[3*sizeof(char)] ; zPtr++ ){` |
|     1341 |  971 | `					if( zPtr >= zEnd \|\| zPtr[0] < '0' \|\| zPtr[0] > '7' ){` |
|      288 |  972 | `						break;` |
|        - |  973 | `					}` |
|      750 |  974 | `					c = c * 8 + (zPtr[0] - '0');` |
|      365 |  975 | `				}` |
|      640 |  976 | `				if( c > 0xFF ){` |
|        - |  977 | `					SyString sSeq;` |
|       19 |  978 | `					SyStringInitFromBuf(&sSeq,zIn,(sxu32)(zPtr-zIn));` |
|        - |  979 | `					/* php's E_COMPILE_WARNING (probe-verified against 8.5: hidden by` |
|        - |  980 | ``					 * `E_ALL & ~E_COMPILE_WARNING`, never offered to a user handler). */`` |
|       19 |  981 | `					PH7_GenCompileError(&(*pGen),128 /* E_COMPILE_WARNING */,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|        - |  982 | `						"Octal escape sequence overflow \\%z is greater than \\377",&sSeq);` |
|       19 |  983 | `					c &= 0xFF;` |
|        8 |  984 | `				}` |
|      640 |  985 | `				cOut = (char)c; /* value byte, independent of host endianness */` |
|      640 |  986 | `				PH7_MemObjStringAppend(pObj,&cOut,sizeof(char));` |
|      640 |  987 | `				n = (sxu32)(zPtr-zIn);` |
|      640 |  988 | `				break;` |
|        - |  989 | `			}` |
|     1939 |  990 | `			case 'x':` |
|     5818 |  991 | `				if( &zIn[1] < zEnd && SyisHex((unsigned char)zIn[1]) ){` |
|        - |  992 | `					/* \x[0-9A-Fa-f]{1,2}: a character in hexadecimal notation */` |
|     3877 |  993 | `					int c = SyHexToint(zIn[1]);` |
|        - |  994 | `					char cOut;` |
|     3877 |  995 | `					n += sizeof(char);` |
|     3877 |  996 | `					if( &zIn[2] < zEnd && SyisHex((unsigned char)zIn[2]) ){` |
|     3873 |  997 | `						c = (c << 4) + SyHexToint(zIn[2]);` |
|     3873 |  998 | `						n += sizeof(char);` |
|     1933 |  999 | `					}` |
|     3877 | 1000 | `					cOut = (char)c; /* value byte, independent of host endianness */` |
|     3877 | 1001 | `					PH7_MemObjStringAppend(pObj,&cOut,sizeof(char));` |
|     1940 | 1002 | `				}else{` |
|        - | 1003 | `					/* Not an escape: keep the backslash, as php does */` |
|        5 | 1004 | `					PH7_MemObjStringAppend(pObj,"\\x",sizeof(char)*2);` |
|        - | 1005 | `				}` |
|     3881 | 1006 | `				break;` |
|      209 | 1007 | `			case 'u':` |
|      416 | 1008 | `				if( &zIn[1] < zEnd && zIn[1] == '{'` |
|      621 | 1009 | `				 && !(&zIn[2] < zEnd && zIn[2] == '$') ){` |
|        - | 1010 | `					/* \u{codepoint}: UTF-8 encoding of the given codepoint (php 7+).` |
|        - | 1011 | `					 * php encodes surrogates verbatim, so the only invalid value` |
|        - | 1012 | `					 * is > U+10FFFF; malformed/empty braces are a compile error.` |
|        - | 1013 | `					 * "\u{$..." is excluded above: php treats it as a literal \u` |
|        - | 1014 | `					 * followed by {$...} curly interpolation. */` |
|      414 | 1015 | `					sxu32 nCp = 0;` |
|      414 | 1016 | `					zPtr = &zIn[2];` |
|     1832 | 1017 | `					while( zPtr < zEnd && SyisHex((unsigned char)zPtr[0]) ){` |
|     1422 | 1018 | `						if( nCp <= 0x10FFFF ){` |
|        - | 1019 | `							/* stop accumulating once out of range: keeps a long` |
|        - | 1020 | `							 * digit run from wrapping sxu32 */` |
|     1422 | 1021 | `							nCp = nCp * 16 + (sxu32)SyHexToint(zPtr[0]);` |
|      706 | 1022 | `						}` |
|     1422 | 1023 | `						zPtr++;` |
|        4 | 1024 | `					}` |
|      414 | 1025 | `					if( zPtr == &zIn[2] \|\| zPtr >= zEnd \|\| zPtr[0] != '}' ){` |
|        - | 1026 | `						/* Error recorded (nErr>0 fails the whole compile); consume the` |
|        - | 1027 | `						 * malformed sequence so later errors are still reported. */` |
|        3 | 1028 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|        - | 1029 | `							"Invalid UTF-8 codepoint escape sequence");` |
|        3 | 1030 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 1031 | `							return SXERR_ABORT;` |
|        - | 1032 | `						}` |
|        3 | 1033 | `						n = (sxu32)(zPtr-zIn);` |
|        3 | 1034 | `						if( zPtr < zEnd && zPtr[0] == '}' ){` |
|        3 | 1035 | `							n += sizeof(char);` |
|        1 | 1036 | `						}` |
|        3 | 1037 | `						break;` |
|        - | 1038 | `					}` |
|      412 | 1039 | `					n = (sxu32)(&zPtr[1]-zIn); /* 'u{...}' incl. closing brace */` |
|      412 | 1040 | `					if( nCp > 0x10FFFF ){` |
|        3 | 1041 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|        - | 1042 | `							"Invalid UTF-8 codepoint escape sequence: Codepoint too large");` |
|        3 | 1043 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 1044 | `							return SXERR_ABORT;` |
|        - | 1045 | `						}` |
|        3 | 1046 | `						break;` |
|        - | 1047 | `					}` |
|        - | 1048 | `					{` |
|        - | 1049 | `						char zUtf[4];` |
|      410 | 1050 | `						sxu8 *zOut = (sxu8 *)zUtf;` |
|      410 | 1051 | `						SX_WRITE_UTF8(zOut,nCp);` |
|      410 | 1052 | `						PH7_MemObjStringAppend(pObj,zUtf,(sxu32)(zOut-(sxu8 *)zUtf));` |
|        - | 1053 | `					}` |
|      206 | 1054 | `				}else{` |
|        - | 1055 | `					/* Not an escape: keep the backslash, as php does */` |
|        7 | 1056 | `					PH7_MemObjStringAppend(pObj,"\\u",sizeof(char)*2);` |
|        - | 1057 | `				}` |
|      416 | 1058 | `				break;` |
|       16 | 1059 | `			default:` |
|        - | 1060 | `				/* Unrecognized escape: keep the backslash, as php does.` |
|        - | 1061 | `				 * zIn[-1] is the backslash itself, so both bytes are contiguous` |
|        - | 1062 | `				 * in the source buffer — one batched append. */` |
|       33 | 1063 | `				PH7_MemObjStringAppend(pObj,&zIn[-1],sizeof(char)*2);` |
|       32 | 1064 | `				break;` |
|        - | 1065 | `			}` |
|        - | 1066 | `			/* Advance the stream cursor */` |
|    50118 | 1067 | `			zIn += n;` |
|    50118 | 1068 | `			continue;` |
|        - | 1069 | `		}` |
|     7180 | 1070 | `		if( zIn[0] == '{' ){` |
|        - | 1071 | `			/* Curly syntax */` |
|        - | 1072 | `			const char *zExpr;` |
|      260 | 1073 | `			sxi32 iNest = 1;` |
|      260 | 1074 | `			zIn++;` |
|      260 | 1075 | `			zExpr = zIn;` |
|        - | 1076 | `			/* Synchronize with the next closing curly braces */` |
|     2383 | 1077 | `			while( zIn < zEnd ){` |
|     2383 | 1078 | `				if( zIn[0] == '{' ){` |
|        - | 1079 | `					/* Increment nesting level */` |
|        3 | 1080 | `					iNest++;` |
|     2382 | 1081 | `				}else if(zIn[0] == '}' ){` |
|        - | 1082 | `					/* Decrement nesting level */` |
|      262 | 1083 | `					iNest--;` |
|      262 | 1084 | `					if( iNest <= 0 ){` |
|      260 | 1085 | `						break;` |
|        - | 1086 | `					}` |
|        1 | 1087 | `				}` |
|     2128 | 1088 | `				zIn++;` |
|        5 | 1089 | `			}` |
|        - | 1090 | `			/* Process the expression */` |
|      260 | 1091 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|      260 | 1092 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1093 | `				return SXERR_ABORT;` |
|        - | 1094 | `			}` |
|      260 | 1095 | `			if( rc != SXERR_EMPTY ){` |
|      260 | 1096 | `				++iCons;` |
|      260 | 1097 | `				++nInterp;` |
|      127 | 1098 | `			}` |
|      260 | 1099 | `			if( zIn < zEnd ){` |
|        - | 1100 | `				/* Jump the trailing curly */` |
|      260 | 1101 | `				zIn++;` |
|      127 | 1102 | `			}` |
|      132 | 1103 | `		}else{` |
|        - | 1104 | `			/*` |
|        - | 1105 | `			 * Simple syntax. php's simple "$var…" form is a LEXER rule, not an` |
|        - | 1106 | `			 * expression: it takes the variable name plus EXACTLY ONE accessor —` |
|        - | 1107 | `			 * "$var", "$var[offset]" or "$var->prop" — and stops there. Everything` |
|        - | 1108 | `			 * past that one accessor is literal text: a second subscript` |
|        - | 1109 | `			 * ("$o->p[0]" is the property then a literal "[0]"), a second arrow` |
|        - | 1110 | `			 * ("$o->p->q" is "$o->p" then a literal "->q"), any "::" at all` |
|        - | 1111 | `			 * ("$c::C" is the VALUE of $c then a literal "::C", never a class` |
|        - | 1112 | `			 * constant), and any "{…}" ("$x{'a'}" is $x then literal). Only the` |
|        - | 1113 | `			 * complex "{$expr}" form reaches those, and it is handled above.` |
|        - | 1114 | `			 *` |
|        - | 1115 | `			 * PHL used to loop here, greedily chaining accessors, so those four` |
|        - | 1116 | `			 * shapes silently answered something else than php on VALID source.` |
|        - | 1117 | `			 */` |
|     6925 | 1118 | `			const char *zExpr = zIn;` |
|     6925 | 1119 | `			int bSubscript = 0;` |
|        - | 1120 | `			/*` |
|        - | 1121 | `			 * "${...}" string interpolation (every form: ${name}, ${expr}, ${$x}) was` |
|        - | 1122 | `			 * DEPRECATED by php 8.2 in favor of the canonical "{$...}". PHL targets php's` |
|        - | 1123 | `			 * *non-deprecated* surface, so it is a hard parse error here — never silently` |
|        - | 1124 | `			 * rewritten. The canonical "{$var}" reaches this compiler by a different path` |
|        - | 1125 | `			 * and is unaffected. Checked before the scan: '{' is not an accessor, so the` |
|        - | 1126 | `			 * cursor would otherwise stop on the '$' and read the brace as literal text.` |
|        - | 1127 | `			 */` |
|     6925 | 1128 | `			if( &zIn[1] < zEnd && zIn[0] == '$' && zIn[1] == '{' ){` |
|        3 | 1129 | `				PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|        - | 1130 | `					"syntax error, \"${\" string interpolation was removed in php 8.2, use \"{$...}\" instead");` |
|        3 | 1131 | `				return SXERR_ABORT;` |
|        - | 1132 | `			}` |
|        - | 1133 | `			/* Jump leading dollars */` |
|    13841 | 1134 | `			while( zIn < zEnd && zIn[0] == '$' ){` |
|     6923 | 1135 | `				zIn++;` |
|        5 | 1136 | `			}` |
|        - | 1137 | `			/* Variable name */` |
|     6923 | 1138 | `			GenStateSkipStringLabel(&zIn,zEnd);` |
|        - | 1139 | `			/* …then at most ONE accessor */` |
|     6981 | 1140 | `			if( zIn < zEnd && zIn[0] == '[' ){` |
|      119 | 1141 | `				sxi32 iSquare = 1;` |
|      119 | 1142 | `				bSubscript = 1;` |
|      119 | 1143 | `				zIn++;` |
|      577 | 1144 | `				while( zIn < zEnd ){` |
|      575 | 1145 | `					if( zIn[0] == '[' ){` |
|        3 | 1146 | `						iSquare++;` |
|      574 | 1147 | `					}else if (zIn[0] == ']' ){` |
|      119 | 1148 | `						iSquare--;` |
|      119 | 1149 | `						if( iSquare <= 0 ){` |
|      117 | 1150 | `							break;` |
|        - | 1151 | `						}` |
|        1 | 1152 | `					}` |
|      461 | 1153 | `					zIn++;` |
|        3 | 1154 | `				}` |
|      119 | 1155 | `				if( zIn < zEnd ){` |
|      117 | 1156 | `					zIn++;` |
|       57 | 1157 | `				}` |
|     6863 | 1158 | `			}else if( &zIn[2] < zEnd && zIn[0] == '-' && zIn[1] == '>'` |
|      133 | 1159 | `				&& GEN_STRING_LABEL_START(zIn[2]) ){` |
|        - | 1160 | `				/* Member access operator '->'. php takes it only when a LABEL` |
|        - | 1161 | `				 * follows; with anything else -- a digit, a space, a '{', the end` |
|        - | 1162 | `				 * of the body -- the arrow is literal TEXT and the interpolation is` |
|        - | 1163 | `				 * just the variable. PHL swallowed the bare '->' and handed the` |
|        - | 1164 | `				 * compiler a dangling "$o->", fatalling` |
|        - | 1165 | `				 * "'->': Missing/Invalid member name" on source php RUNS. */` |
|       91 | 1166 | `				zIn += 2;` |
|       91 | 1167 | `				GenStateSkipStringLabel(&zIn,zEnd);` |
|     6761 | 1168 | `			}else if( &zIn[3] < zEnd && zIn[0] == '?' && zIn[1] == '-' && zIn[2] == '>'` |
|       15 | 1169 | `				&& GEN_STRING_LABEL_START(zIn[3]) ){` |
|        - | 1170 | `				/* php 8.0 gave the simple syntax the NULLSAFE arrow on the same terms` |
|        - | 1171 | `				 * as '->': exactly one property name, taken only when a LABEL follows.` |
|        - | 1172 | ``				 * Without it `"$o?->b"` cast the OBJECT to a string and appended four`` |
|        - | 1173 | `				 * literal bytes -- an uncatchable "could not be converted to string"` |
|        - | 1174 | `				 * on source php runs. */` |
|        9 | 1175 | `				zIn += 3;` |
|        9 | 1176 | `				GenStateSkipStringLabel(&zIn,zEnd);` |
|        4 | 1177 | `			}` |
|        - | 1178 | `			/*` |
|        - | 1179 | `			 * "$a[offset]" -- php parses a simple-syntax subscript with its OWN tiny` |
|        - | 1180 | ``			 * grammar (zend's `encaps_var_offset`), never as an expression, so rewrite`` |
|        - | 1181 | `			 * it into the equivalent php source and hand THAT to the compiler. PH7 fed` |
|        - | 1182 | `			 * the raw text straight in, which read every integer SPELLING as a number` |
|        - | 1183 | `			 * ("$a[007]" / "$a[0x1A]" / "$a[1_000]" / "$a[-0]" answered the integer` |
|        - | 1184 | `			 * keys 7/26/1000/0 where php reads the STRING keys "007"/"0x1A"/"1_000"/` |
|        - | 1185 | `			 * "-0"), read a non-ASCII bare word as a CONSTANT ("$a[\xc3\xa9]" raised` |
|        - | 1186 | `			 * "Undefined constant"), and quietly accepted every shape php rejects` |
|        - | 1187 | `			 * ("$a[ 0]", "$a[0 ]", "$a['x']", "$a[+1]", "$a[-$k]", "$a[[]", "$a[]").` |
|        - | 1188 | `			 */` |
|     6923 | 1189 | `			if( bSubscript ){` |
|      119 | 1190 | `				const char *zBr = zExpr;` |
|        - | 1191 | `				SyBlob sSub;` |
|      381 | 1192 | `				while( zBr < zIn && zBr[0] != '[' ){` |
|      265 | 1193 | `					zBr++;` |
|        3 | 1194 | `				}` |
|      119 | 1195 | `				if( zIn <= zBr \|\| zIn[-1] != ']' ){` |
|        - | 1196 | `					/* Unterminated: the body ended inside the brackets. php names the` |
|        - | 1197 | `					  * closing quote it reached instead; there is no offending TOKEN to` |
|        - | 1198 | `					  * quote here, and zIn is one past the body, so never read it. */` |
|      ! 0 | 1199 | `					PH7_GenCompileError(&(*pGen),E_PARSE,GenStateStringEscLine(&(*pGen),zBr,bHeredoc),` |
|        - | 1200 | `						"syntax error, unexpected end of string, expecting \"-\" or identifier or variable or number");` |
|      ! 0 | 1201 | `					return SXERR_ABORT;` |
|        - | 1202 | `				}` |
|      119 | 1203 | `				SyBlobInit(&sSub,&pGen->pVm->sAllocator);` |
|      119 | 1204 | `				SyBlobAppend(&sSub,zExpr,(sxu32)(zBr - zExpr));` |
|      119 | 1205 | `				rc = GenStateCompileStringOffset(&(*pGen),&zBr[1],&zIn[-1],&sSub,bHeredoc);` |
|      119 | 1206 | `				if( rc != SXRET_OK ){` |
|       35 | 1207 | `					SyBlobRelease(&sSub);` |
|       35 | 1208 | `					return SXERR_ABORT;` |
|        - | 1209 | `				}` |
|      126 | 1210 | `				rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,` |
|       82 | 1211 | `					(const char *)SyBlobData(&sSub),` |
|       82 | 1212 | `					(const char *)SyBlobData(&sSub) + SyBlobLength(&sSub));` |
|       85 | 1213 | `				SyBlobRelease(&sSub);` |
|       85 | 1214 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1215 | `					return SXERR_ABORT;` |
|        - | 1216 | `				}` |
|       85 | 1217 | `				if( rc != SXERR_EMPTY ){` |
|       85 | 1218 | `					++iCons;` |
|       85 | 1219 | `					++nInterp;` |
|       41 | 1220 | `				}` |
|       85 | 1221 | `				pObj = 0;` |
|       85 | 1222 | `				continue;` |
|        - | 1223 | `			}` |
|        - | 1224 | `			/* Process the expression */` |
|     6807 | 1225 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|     6807 | 1226 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1227 | `				return SXERR_ABORT;` |
|        - | 1228 | `			}` |
|     6807 | 1229 | `			if( rc != SXERR_EMPTY ){` |
|     6807 | 1230 | `				++iCons;` |
|     6807 | 1231 | `				++nInterp;` |
|     3384 | 1232 | `			}` |
|        - | 1233 | `		}` |
|        - | 1234 | `		/* Invalidate the previously used constant */` |
|     7062 | 1235 | `		pObj = 0;` |
|        5 | 1236 | `	}/*for(;;)*/` |
|    86237 | 1237 | `	if( iCons > 1 ){` |
|        - | 1238 | `		/* Concatenate all compiled constants */` |
|     5493 | 1239 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|    83480 | 1240 | `	}else if( iCons == 1 && nInterp == 1 ){` |
|        - | 1241 | `		/* A string that is nothing but one interpolation ("$x") still has to` |
|        - | 1242 | `		 * PRODUCE A STRING. With no CAT to force the conversion the operand was` |
|        - | 1243 | ``		 * left on the stack untouched, so `$s = "$x"` handed back $x's own type:`` |
|        - | 1244 | `		 * "$arr" stayed an array (and skipped php's "Array to string conversion"` |
|        - | 1245 | `		 * warning), "$int" stayed an int, "$res" stayed a resource. */` |
|       97 | 1246 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CVT_STR,0,0,0,0);` |
|       47 | 1247 | `	}` |
|        - | 1248 | `	/* Node successfully compiled */` |
|    86237 | 1249 | `	return SXRET_OK;` |
|    44092 | 1250 | `}` |
|        - | 1251 | `/*` |
|        - | 1252 | ` * Compile a double quoted string.` |
|        - | 1253 | ` *  See the block-comment above for more information.` |
|        - | 1254 | ` */` |
|    88282 | 1255 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 | 1256 | `{` |
|        - | 1257 | `	sxi32 rc;` |
|    88287 | 1258 | `	rc = GenStateCompileString(&(*pGen),0/*bHeredoc*/);` |
|    44049 | 1259 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        - | 1260 | `	/* Compilation result */` |
|    88287 | 1261 | `	return rc;` |
|        5 | 1262 | `}` |
|        - | 1263 | `/*` |
|        - | 1264 | ` * Compile a Heredoc string.` |
|        - | 1265 | ` *  See the block-comment above for more information.` |
|        - | 1266 | ` */` |
|       80 | 1267 | `PH7_PRIVATE sxi32 PH7_CompileHereDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 | 1268 | `{` |
|        - | 1269 | `	SyString sOrig, sStripped;` |
|        - | 1270 | `	sxi32 rc;` |
|       85 | 1271 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|       85 | 1272 | `	if( rc != SXRET_OK ){` |
|        6 | 1273 | `		return rc;` |
|        - | 1274 | `	}` |
|        - | 1275 | `	/* Temporarily swap in the dedented body so GenStateCompileString` |
|        - | 1276 | `	 * (which reads pGen->pIn->sData directly) sees the stripped content.` |
|        - | 1277 | `	 * Restore before returning so downstream code that references pIn is` |
|        - | 1278 | `	 * unaffected, including on the error path. */` |
|       81 | 1279 | `	sOrig = pGen->pIn->sData;` |
|       81 | 1280 | `	pGen->pIn->sData = sStripped;` |
|       81 | 1281 | `	rc = GenStateCompileString(&(*pGen),1/*bHeredoc*/);` |
|       81 | 1282 | `	pGen->pIn->sData = sOrig;` |
|       38 | 1283 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       81 | 1284 | `	return rc;` |
|       45 | 1285 | `}` |
|        - | 1286 | `/*` |
|        - | 1287 | ` * Compile an array entry whether it is a key or a value.` |
|        - | 1288 | ` *  Notes on array entries.` |
|        - | 1289 | ` *  According to the PHP language reference manual` |
|        - | 1290 | ` *  An array can be created by the array() language construct.` |
|        - | 1291 | ` *  It takes as parameters any number of comma-separated key => value pairs.` |
|        - | 1292 | ` *  array(  key =>  value` |
|        - | 1293 | ` *    , ...` |
|        - | 1294 | ` *    )` |
|        - | 1295 | ` *  A key may be either an integer or a string. If a key is the standard representation` |
|        - | 1296 | ` *  of an integer, it will be interpreted as such (i.e. "8" will be interpreted as 8, while` |
|        - | 1297 | ` *  "08" will be interpreted as "08"). Floats in key are truncated to integer.` |
|        - | 1298 | ` *  The indexed and associative array types are the same type in PHP, which can both` |
|        - | 1299 | ` *  contain integer and string indices.` |
|        - | 1300 | ` *  A value can be any PHP type.` |
|        - | 1301 | ` *  If a key is not specified for a value, the maximum of the integer indices is taken` |
|        - | 1302 | ` *  and the new key will be that value plus 1. If a key that already has an assigned value` |
|        - | 1303 | ` *  is specified, that value will be overwritten.` |
|        - | 1304 | ` */` |
|   192961 | 1305 | `PH7_PRIVATE sxi32 GenStateCompileArrayEntry(` |
|        - | 1306 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - | 1307 | `	SyToken *pIn,        /* Token stream */` |
|        - | 1308 | `	SyToken *pEnd,       /* End of the token stream */` |
|        - | 1309 | `	sxi32 iFlags,        /* Compilation flags */` |
|        - | 1310 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *) /* Expression tree validator callback */` |
|        - | 1311 | `	)` |
|        5 | 1312 | `{` |
|        - | 1313 | `	SyToken *pTmpIn,*pTmpEnd;` |
|        - | 1314 | `	sxi32 rc;` |
|        - | 1315 | `	/* Swap token stream */` |
|   192966 | 1316 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|        - | 1317 | `	/* Compile the expression*/` |
|   192966 | 1318 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|        - | 1319 | `	/* Restore token stream */` |
|   192966 | 1320 | `	RE_SWAP_DELIMITER(pGen);` |
|   192966 | 1321 | `	return rc;` |
|        5 | 1322 | `}` |
|        - | 1323 | `/*` |
|        - | 1324 | ` * Expression tree validator callback for the 'array' language construct.` |
|        - | 1325 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|        - | 1326 | ` * an invalid expression tree and this function will generate the appropriate` |
|        - | 1327 | ` * error message.` |
|        - | 1328 | ` * See the routine responible of compiling the array language construct` |
|        - | 1329 | ` * for more inforation.` |
|        - | 1330 | ` */` |
|      136 | 1331 | `static sxi32 GenStateArrayNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        5 | 1332 | `{` |
|        - | 1333 | ``	/* `array(&$x)` is a full write target in php, call included: `[&f()]` is its`` |
|        - | 1334 | `	 * "Can't use function return value in write context", not the` |
|        - | 1335 | ``	 * reference-returning-function exemption `$r =& f()` gets. A nullsafe chain`` |
|        - | 1336 | `	 * here takes the WRITE wording too, not the reference one — php never asks` |
|        - | 1337 | ``	 * `zend_assert_not_short_circuited` on an array entry. */`` |
|        - | 1338 | `	sxi32 rc;` |
|      136 | 1339 | `	if( pRoot && pRoot->pOp && pRoot->pOp->iOp == EXPR_OP_SUBSCRIPT` |
|       21 | 1340 | `	 && SySetUsed(&pRoot->aNodeArgs) < 1 ){` |
|        - | 1341 | ``		/* `[&$a[]]`: an APPEND has nothing to take a reference OF, and php asks`` |
|        - | 1342 | ``		 * that before anything else about the base — so `[&f()[]]` is this and not`` |
|        - | 1343 | `		 * the call refusal. PHL ran the whole thing. */` |
|      ! 0 | 1344 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,` |
|      ! 0 | 1345 | `			pRoot->pStart ? pRoot->pStart->nLine : 0,"Cannot use [] for reading");` |
|      ! 0 | 1346 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 1347 | `	}` |
|      141 | 1348 | `	rc = GenStateWriteTargetCheck(&(*pGen),pRoot,PH7_WTC_THISSRC);` |
|      141 | 1349 | `	if( rc != SXRET_OK ){` |
|        7 | 1350 | `		return rc;` |
|        - | 1351 | `	}` |
|      135 | 1352 | `	if( PH7_ExprNodeIsThis(pRoot) ){` |
|        - | 1353 | `` 		/* php has no slot to point at here, so `[&$this]` COPIES: `$a[0] = 5` `` |
|        - | 1354 | ``		 * through the entry leaves `$this` an object. Tell the caller not to`` |
|        - | 1355 | `		 * emit the reference load. */` |
|       11 | 1356 | `		pGen->bRefElemIsThis = 1;` |
|        5 | 1357 | `	}` |
|      135 | 1358 | `	if( pRoot->pOp ){` |
|       20 | 1359 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ &&` |
|       14 | 1360 | `			pRoot->pOp->iOp != EXPR_OP_FUNC_CALL /* function() [Symisc extension: i.e: array(&foo())] */` |
|       19 | 1361 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|        - | 1362 | `			/* Unexpected expression */` |
|       13 | 1363 | `			rc = PH7_GenSyntaxError(&(*pGen),pRoot->pStart,"\"->\" or \"?->\" or \"[\"");` |
|       13 | 1364 | `			if( rc != SXERR_ABORT ){` |
|       13 | 1365 | `				rc = SXERR_INVALID;` |
|        5 | 1366 | `			}` |
|       10 | 1367 | `		}` |
|      123 | 1368 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|        - | 1369 | `		/* Unexpected expression */` |
|        3 | 1370 | `		rc = PH7_GenSyntaxError(&(*pGen),pRoot->pStart,0);` |
|        3 | 1371 | `		if( rc != SXERR_ABORT ){` |
|        3 | 1372 | `			rc = SXERR_INVALID;` |
|        1 | 1373 | `		}` |
|        1 | 1374 | `	}` |
|      135 | 1375 | `	return rc;` |
|       73 | 1376 | `}` |
|        - | 1377 | `/*` |
|        - | 1378 | ` * Find the top-level '=>' (PH7_TK_ARRAY_OP) that separates an array/list entry's` |
|        - | 1379 | ` * key from its value within [pStart,pEnd). The scan skips any '=>' nested inside` |
|        - | 1380 | ` * brackets/parens/braces, inside an arrow-function signature (fn(...) =>), or` |
|        - | 1381 | ` * inside a match() {...} arm — none of which are key/value separators. Returns a` |
|        - | 1382 | ` * pointer to the '=>' token, or pEnd if the entry has no top-level separator.` |
|        - | 1383 | ` */` |
|   278405 | 1384 | `PH7_PRIVATE SyToken * GenStateFindTopLevelArrow(SyToken *pStart,SyToken *pEnd)` |
|        5 | 1385 | `{` |
|   278410 | 1386 | `	SyToken *pCur = pStart;` |
|   278410 | 1387 | `	sxi32 iNest = 0;` |
|        - | 1388 | ``	/* An entry that STARTS with a bare `yield` owns the FIRST '=>' after it:`` |
|        - | 1389 | `` 	 * php's grammar gives `yield expr => expr` to the yield, so `[yield 1 => 2]` `` |
|        - | 1390 | `	 * is ONE element (the yield's own result) keyed by nothing, and the generator` |
|        - | 1391 | `	 * yields key 1 value 2. Reading that '=>' as the entry separator instead built` |
|        - | 1392 | ``	 * `[(yield 1) => 2]` — a different array AND a different yielded pair, in`` |
|        - | 1393 | `	 * silence. A SECOND top-level '=>' is the entry separator again, which is what` |
|        - | 1394 | ``	 * makes `[yield 1 => 2 => 3]` parse. `yield from` takes an iterable and never a`` |
|        - | 1395 | `	 * pair, so its entry keeps the ordinary rule. */` |
|   278688 | 1396 | `	int bYieldOwnsArrow = (pStart < pEnd) && (pStart->nType & PH7_TK_KEYWORD)` |
|   139231 | 1397 | `		&& (sxu32)SX_PTR_TO_INT(pStart->pUserData) == PH7_TKWRD_YIELD` |
|   417859 | 1398 | `		&& !(&pStart[1] < pEnd && (pStart[1].nType & PH7_TK_ID)` |
|        8 | 1399 | `			&& pStart[1].sData.nByte == 4` |
|        2 | 1400 | `			&& SyStrnicmp(pStart[1].sData.zString, "from", 4) == 0);` |
|   698691 | 1401 | `	while( pCur < pEnd ){` |
|   452432 | 1402 | `		if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    31849 | 1403 | `			if( bYieldOwnsArrow ){` |
|        9 | 1404 | `				bYieldOwnsArrow = 0;` |
|        9 | 1405 | `				pCur++;` |
|        9 | 1406 | `				continue;` |
|        - | 1407 | `			}` |
|    31841 | 1408 | `			return pCur;` |
|        - | 1409 | `		}` |
|        - | 1410 | `		/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|        - | 1411 | `		 * The '=>' inside an arrow function introduces the expression body,` |
|        - | 1412 | `		 * not an entry separator. Skip past the signature.` |
|        - | 1413 | `		 */` |
|   420588 | 1414 | `		if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
|      741 | 1415 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|      741 | 1416 | `			SyToken *pFn = pCur;` |
|        - | 1417 | ``			/* Only a real `[static] fn[&](` opens an arrow function; `$fn`,`` |
|        - | 1418 | ``			 * `C::fn` and friends are plain names whose '=>' IS the separator. */`` |
|      741 | 1419 | `			if( PH7_TokenOpensArrowFunc(pStart,pCur,pEnd) ){` |
|      314 | 1420 | `				if( nKw == PH7_TKWRD_STATIC ){` |
|      ! 0 | 1421 | `					pFn = &pCur[1];` |
|      ! 0 | 1422 | `				}` |
|      314 | 1423 | `				pCur = pFn + 1; /* past 'fn' */` |
|      314 | 1424 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_AMPER) ){` |
|      ! 0 | 1425 | `					pCur++;` |
|      ! 0 | 1426 | `				}` |
|      314 | 1427 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|      314 | 1428 | `					pCur++;` |
|      314 | 1429 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|        - | 1430 | `						PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|      314 | 1431 | `					if( pCur < pEnd ){` |
|      314 | 1432 | `						pCur++;` |
|      155 | 1433 | `					}` |
|      155 | 1434 | `				}` |
|      314 | 1435 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_COLON) ){` |
|        3 | 1436 | `					pCur++;` |
|        2 | 1437 | `					if( pCur < pEnd && (pCur->nType & PH7_TK_OP)` |
|        1 | 1438 | `						&& pCur->sData.nByte == 1` |
|        1 | 1439 | `						&& pCur->sData.zString[0] == '?' ){` |
|      ! 0 | 1440 | `						pCur++;` |
|      ! 0 | 1441 | `					}` |
|        2 | 1442 | `					if( pCur < pEnd` |
|        3 | 1443 | `						&& (pCur->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|        3 | 1444 | `						pCur++;` |
|        1 | 1445 | `					}` |
|        1 | 1446 | `				}` |
|        - | 1447 | `				/* The rest of the entry is the arrow-function body — no outer` |
|        - | 1448 | `				 * key to extract. */` |
|      314 | 1449 | `				return pEnd;` |
|        - | 1450 | `			}` |
|        - | 1451 | `			/* Match expression (PHP 8.0): the '=>' inside match arms is not an` |
|        - | 1452 | `			 * entry separator. Skip past the full match span. */` |
|      431 | 1453 | `			if( nKw == PH7_TKWRD_MATCH ){` |
|       12 | 1454 | `				pCur++; /* past 'match' */` |
|       12 | 1455 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|       10 | 1456 | `					pCur++;` |
|       10 | 1457 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|        - | 1458 | `						PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|       10 | 1459 | `					if( pCur < pEnd ){` |
|       10 | 1460 | `						pCur++;` |
|        4 | 1461 | `					}` |
|        4 | 1462 | `				}` |
|       12 | 1463 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_OCB) ){` |
|       10 | 1464 | `					pCur++;` |
|       10 | 1465 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|        - | 1466 | `						PH7_TK_OCB,PH7_TK_CCB,&pCur);` |
|       10 | 1467 | `					if( pCur < pEnd ){` |
|       10 | 1468 | `						pCur++;` |
|        4 | 1469 | `					}` |
|        4 | 1470 | `				}` |
|       12 | 1471 | `				continue;` |
|        - | 1472 | `			}` |
|      208 | 1473 | `		}` |
|   420268 | 1474 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     8401 | 1475 | `			iNest++;` |
|   416043 | 1476 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/) ){` |
|        - | 1477 | `			/* Don't worry about mismatched brackets here, the expression` |
|        - | 1478 | `			 * parser will shortly detect any syntax error. */` |
|     8401 | 1479 | `			iNest--;` |
|     4171 | 1480 | `		}` |
|   420268 | 1481 | `		pCur++;` |
|        5 | 1482 | `	}` |
|   246264 | 1483 | `	return pEnd;` |
|   138958 | 1484 | `}` |
|        - | 1485 | `/*` |
|        - | 1486 | ` * Can this run of tokens RUN anything?` |
|        - | 1487 | ` *` |
|        - | 1488 | ` * The array literal is compiled entry by entry off the raw token stream -- there is no` |
|        - | 1489 | ` * expression tree to ask -- so the question GenStateArgRunsCode answers for a call's` |
|        - | 1490 | ` * arguments is answered here from the tokens. Only two shapes read without running: a` |
|        - | 1491 | `` * plain `$name`, and one literal or constant token. A bare identifier is a constant`` |
|        - | 1492 | ` * lookup, which is a table read in php too. Everything else -- an operator, a call, a` |
|        - | 1493 | ` * subscript, an interpolated string -- either writes or hands control to something that` |
|        - | 1494 | ` * can, and is answered YES so the entries already pushed are copied first.` |
|        - | 1495 | ` */` |
|     5828 | 1496 | `static int GenStateArrayEntryRunsCode(SyToken *pStart,SyToken *pEnd)` |
|        5 | 1497 | `{` |
|     5833 | 1498 | `	sxu32 nTok = (sxu32)(pEnd - pStart);` |
|     5833 | 1499 | `	if( nTok == 2 ){` |
|     1419 | 1500 | `		return !((pStart[0].nType & PH7_TK_DOLLAR)` |
|      707 | 1501 | `		      && (pStart[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)));` |
|        - | 1502 | `	}` |
|     5118 | 1503 | `	if( nTok == 1 ){` |
|     3188 | 1504 | `		return (pStart[0].nType` |
|     2128 | 1505 | `			& (PH7_TK_NUM\|PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_SSTR\|PH7_TK_NOWDOC)) == 0;` |
|        - | 1506 | `	}` |
|     2990 | 1507 | `	return 1;` |
|     2908 | 1508 | `}` |
|        - | 1509 | `/*` |
|        - | 1510 | ` * Can this run of tokens leave a VIEW of storage user code can still write to?` |
|        - | 1511 | ` *` |
|        - | 1512 | ` * Every spelling that reads writable storage -- a variable, an element, a property, a` |
|        - | 1513 | `` * static property -- carries a `$`. A constant, a class constant, an enum case and a`` |
|        - | 1514 | ` * literal are immutable, and a call hands back a value of its own; none of those can be` |
|        - | 1515 | ` * written under an entry that is already pushed, so a literal-only list pays nothing.` |
|        - | 1516 | ` */` |
|   185641 | 1517 | `static int GenStateArrayEntryMayAlias(SyToken *pStart,SyToken *pEnd)` |
|        5 | 1518 | `{` |
|   408293 | 1519 | `	while( pStart < pEnd ){` |
|   225581 | 1520 | `		if( pStart->nType & PH7_TK_DOLLAR ){` |
|     2934 | 1521 | `			return 1;` |
|        - | 1522 | `		}` |
|   222652 | 1523 | `		pStart++;` |
|        5 | 1524 | `	}` |
|   182717 | 1525 | `	return 0;` |
|    92636 | 1526 | `}` |
|        - | 1527 | `/*` |
|        - | 1528 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|        - | 1529 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|        - | 1530 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|        - | 1531 | ` */` |
|   125470 | 1532 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|        5 | 1533 | `{` |
|        - | 1534 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|        - | 1535 | `	SyToken *pKey,*pCur;` |
|   125475 | 1536 | `	sxi32 iEmitRef = 0;` |
|   125475 | 1537 | `	sxi32 iSpread = 0;` |
|   125475 | 1538 | `	sxi32 nPair = 0;` |
|        - | 1539 | `	sxi32 rc;` |
|        - | 1540 | `	/* An entry already pushed BORROWS its source's string bytes, so a later entry that` |
|        - | 1541 | `` 	 * runs code -- an assignment, a call, `++` -- writes through it: `[$x, $x = 'second']` `` |
|        - | 1542 | `	 * gave element 0 the assignment's bytes read through the old length ("secon"), and` |
|        - | 1543 | ``	 * `[$x => 1, ($x = 'new') => 2]` gave both entries the same KEY and lost one of them.`` |
|        - | 1544 | `	 * php builds each element where it is written and never sees the later write. So make` |
|        - | 1545 | `	 * everything pushed so far private before compiling an entry that can run something --` |
|        - | 1546 | `	 * only when something pushed can actually be a view, which a literal-only list never` |
|        - | 1547 | `	 * is. */` |
|   125475 | 1548 | `	sxi32 nPushed = 0;   /* stack slots this literal has pushed */` |
|   125475 | 1549 | `	int bAliasable = 0;  /* ...and whether any of them can be a view of live storage */` |
|   125475 | 1550 | `	xValidator = 0;` |
|   151391 | 1551 | `	for(;;){` |
|        - | 1552 | `		/* Jump leading commas. Exactly ONE separates two entries; a second one (or a comma` |
|        - | 1553 | `		 * at the very start) means an EMPTY element, which php rejects outright — PH7 just` |
|        - | 1554 | ``		 * skipped them, so `array(,)` and `array(1,,2)` compiled silently. A TRAILING comma`` |
|        - | 1555 | `		 * is legal and is handled by the loop exiting on the next pass. */` |
|    89123 | 1556 | `		{` |
|   303368 | 1557 | `			int nSkip = 0;` |
|   443838 | 1558 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|   140475 | 1559 | `				nSkip++;` |
|   140475 | 1560 | `				pGen->pIn++;` |
|        5 | 1561 | `			}` |
|   303368 | 1562 | `			if( nSkip > 1 \|\| (nSkip > 0 && nPair < 1) ){` |
|      ! 0 | 1563 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn[-1].nLine,` |
|        - | 1564 | `					"Cannot use empty array elements in arrays");` |
|      ! 0 | 1565 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1566 | `					return SXERR_ABORT;` |
|        - | 1567 | `				}` |
|      ! 0 | 1568 | `				return SXRET_OK;` |
|        - | 1569 | `			}` |
|        - | 1570 | `		}` |
|   303368 | 1571 | `		pCur = pGen->pIn;` |
|   303368 | 1572 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|        - | 1573 | `			/* No more entry to process */` |
|   125457 | 1574 | `			break;` |
|        - | 1575 | `		}` |
|   177916 | 1576 | `		if( pCur >= pGen->pIn ){` |
|      ! 0 | 1577 | `			continue;` |
|        - | 1578 | `		}` |
|        - | 1579 | `		/* Compile the key if available */` |
|   177916 | 1580 | `		pKey = pCur;` |
|   177916 | 1581 | `		pCur = GenStateFindTopLevelArrow(pCur,pGen->pIn);` |
|   177916 | 1582 | `		rc = SXERR_EMPTY;` |
|   177916 | 1583 | `		if( pCur < pGen->pIn ){` |
|    13593 | 1584 | `			if( pKey == pCur ){` |
|        - | 1585 | ``				/* `array( => 2)`: the entry STARTS with '=>', so it has no key. php rejects`` |
|        - | 1586 | `				 * it; PH7 warned about a "Missing entry key" and compiled on, accepting` |
|        - | 1587 | ``				 * source php refuses. (The `else if` below could never see this: the arrow`` |
|        - | 1588 | `				 * IS found here, so control never reached it.)` |
|        - | 1589 | `				 * php names the literal's own closer, so short syntax expects ']'. */` |
|        3 | 1590 | `				const char *zClose = (pGen->pEnd && (pGen->pEnd->nType & PH7_TK_CSB))` |
|        - | 1591 | `					? "\"]\"" : "\")\"";` |
|        3 | 1592 | `				rc = PH7_GenSyntaxError(&(*pGen),pCur,zClose);` |
|        3 | 1593 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1594 | `					return SXERR_ABORT;` |
|        - | 1595 | `				}` |
|        3 | 1596 | `				return SXRET_OK;` |
|        - | 1597 | `			}` |
|    13591 | 1598 | `			if( &pCur[1] >= pGen->pIn ){` |
|        - | 1599 | ``				/* `array(1 => )`: php names the token that SHOULD have started the value —`` |
|        - | 1600 | `				 * the ')' or ']' closing the literal — not the '=>' it just read. Passing 0` |
|        - | 1601 | `				 * makes the helper reach for the token past this entry's slice. */` |
|       13 | 1602 | `				rc = PH7_GenSyntaxError(&(*pGen),0,0);` |
|       13 | 1603 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1604 | `					return SXERR_ABORT;` |
|        - | 1605 | `				}` |
|       13 | 1606 | `				return SXRET_OK;` |
|        - | 1607 | `			}` |
|        - | 1608 | `			/* Compile the expression holding the key */` |
|    13581 | 1609 | `			if( nPushed > 0 && bAliasable && GenStateArrayEntryRunsCode(pKey,pCur) ){` |
|       24 | 1610 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_SNAPSHOT,nPushed,0,0,0);` |
|        9 | 1611 | `			}` |
|    13581 | 1612 | `			bAliasable = bAliasable \|\| GenStateArrayEntryMayAlias(pKey,pCur);` |
|    13581 | 1613 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|        - | 1614 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|    13581 | 1615 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1616 | `				return SXERR_ABORT;` |
|        - | 1617 | `			}` |
|    13581 | 1618 | `			nPushed++;` |
|    13581 | 1619 | `			pCur++; /* Jump the '=>' operator */` |
|     6769 | 1620 | `		}else{` |
|        - | 1621 | `			/* Reset back the cursor and point to the entry value */` |
|   164328 | 1622 | `			pCur = pKey;` |
|        - | 1623 | `		}` |
|   177904 | 1624 | `		if( rc == SXERR_EMPTY ){` |
|        - | 1625 | `			/* No key given: load the nil, TAGGED so LOAD_MAP knows this is an absent key` |
|        - | 1626 | ``			 * (auto-index) rather than an explicit `null =>` one, which php deprecates. */`` |
|   164328 | 1627 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,PH7_LOADC_NOKEY,0 /* nil index */,0,0);` |
|   164328 | 1628 | `			nPushed++;` |
|    82009 | 1629 | `		}` |
|   177904 | 1630 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
|        - | 1631 | `			/* Insertion by reference, [i.e: $a = array(&$x);] */` |
|      145 | 1632 | `			xValidator = GenStateArrayNodeValidator; /* Only variable are allowed */` |
|      145 | 1633 | `			iEmitRef = 1;` |
|      145 | 1634 | `			pCur++; /* Jump the '&' token */` |
|      145 | 1635 | `			if( pCur >= pGen->pIn ){` |
|        - | 1636 | `				/* Missing value */` |
|        - | 1637 | ``				/* php reports the token that actually stopped it (`array(&)` -> the`` |
|        - | 1638 | `				 * ')'), not a hand-written "missing referenced variable" fatal. */` |
|        3 | 1639 | `				rc = PH7_GenSyntaxError(&(*pGen),pCur < pGen->pIn ? pCur : 0,0);` |
|        3 | 1640 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1641 | `					return SXERR_ABORT;` |
|        - | 1642 | `				}` |
|        3 | 1643 | `				return SXRET_OK;` |
|        - | 1644 | `			}` |
|       69 | 1645 | `		}` |
|        - | 1646 | `		/* Detect array unpack: '...$expr' as the entry value (PHP 7.4+, with` |
|        - | 1647 | `		 * string-key support since PHP 8.1). The parser strips the '...' inside` |
|        - | 1648 | `		 * ExprExtractNode; we only need to know it's there so we can emit` |
|        - | 1649 | `		 * PH7_OP_FLAG_SPREAD after the value, instructing LOAD_MAP to merge the` |
|        - | 1650 | `		 * resulting hashmap rather than insert it as a scalar entry. */` |
|   177902 | 1651 | `		iSpread = (pCur < pGen->pIn && (pCur->nType & PH7_TK_ELLIPSIS)) ? 1 : 0;` |
|   177902 | 1652 | `		if( iSpread && (rc != SXERR_EMPTY \|\| iEmitRef) ){` |
|        - | 1653 | `			/* '[k => ...$a]' and '[&...$a]' are syntax errors in PHP — the` |
|        - | 1654 | `			 * '...' token cannot follow either '=>' or '&' inside an array` |
|        - | 1655 | `			 * literal. Emit the same Parse-error wording PHP uses so the` |
|        - | 1656 | `			 * output is engine-portable. */` |
|        6 | 1657 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pCur->nLine,` |
|        - | 1658 | `				"syntax error, unexpected token \"...\"");` |
|        6 | 1659 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1660 | `				return SXERR_ABORT;` |
|        - | 1661 | `			}` |
|        6 | 1662 | `			return SXRET_OK;` |
|        - | 1663 | `		}` |
|        - | 1664 | ``		/* Compile indice value. A BY-REF element (`'k' => &$a[$i]`) is an`` |
|        - | 1665 | `		 * lvalue: php VIVIFIES a missing subscript when a reference is taken,` |
|        - | 1666 | `		 * so compile it in write context (LOAD_IDX iP2=1, create-if-missing)` |
|        - | 1667 | `		 * instead of a read-only load — which also keeps the undefined-key` |
|        - | 1668 | `		 * warning (a read-only diagnostic) from false-firing here. A missing` |
|        - | 1669 | ``		 * PROPERTY (`[&$o->p]`) is created the same way (EXPR_FLAG_MEMBER_REFSRC). */`` |
|   177898 | 1670 | `		pGen->bRefElemIsThis = 0;` |
|   177898 | 1671 | `		if( nPushed > 0 && bAliasable && GenStateArrayEntryRunsCode(pCur,pGen->pIn) ){` |
|     3056 | 1672 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_SNAPSHOT,nPushed,0,0,0);` |
|     1522 | 1673 | `		}` |
|   177898 | 1674 | `		bAliasable = bAliasable \|\| GenStateArrayEntryMayAlias(pCur,pGen->pIn);` |
|   266668 | 1675 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,` |
|    88770 | 1676 | `			iEmitRef ? (EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_REFSRC)` |
|        - | 1677 | `			         : EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,` |
|    88770 | 1678 | `			xValidator);` |
|   177898 | 1679 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1680 | `			return SXERR_ABORT;` |
|        - | 1681 | `		}` |
|   177898 | 1682 | `		nPushed++;` |
|   177898 | 1683 | `		if( iSpread ){` |
|        - | 1684 | `			/* Mark the value on TOS as a spread source; LOAD_MAP merges it. */` |
|      122 | 1685 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FLAG_SPREAD,0,0,0,0);` |
|   177839 | 1686 | `		}else if( iEmitRef && !pGen->bRefElemIsThis ){` |
|        - | 1687 | `			/* Emit the load reference instruction */` |
|      131 | 1688 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|       63 | 1689 | `		}` |
|   177898 | 1690 | `		pGen->bRefElemIsThis = 0;` |
|   177898 | 1691 | `		xValidator = 0;` |
|   177898 | 1692 | `		iEmitRef = 0;` |
|   177898 | 1693 | `		iSpread = 0;` |
|   177898 | 1694 | `		nPair++;` |
|        5 | 1695 | `	}` |
|        - | 1696 | `	/* Emit the load map instruction */` |
|   125457 | 1697 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|        - | 1698 | `	/* Node successfully compiled */` |
|   125457 | 1699 | `	return SXRET_OK;` |
|    62626 | 1700 | `}` |
|        - | 1701 | `/*` |
|        - | 1702 | ` * Compile the 'array' language construct.` |
|        - | 1703 | ` *	 According to the PHP language reference manual` |
|        - | 1704 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|        - | 1705 | ` *   values to keys. This type is optimized for several different uses; it can` |
|        - | 1706 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|        - | 1707 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|        - | 1708 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|        - | 1709 | ` */` |
|   104700 | 1710 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 | 1711 | `{` |
|        - | 1712 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   104705 | 1713 | `	pGen->pIn += 2;` |
|   104705 | 1714 | `	pGen->pEnd--;` |
|    52284 | 1715 | `	SXUNUSED(iCompileFlag);` |
|        - | 1716 | ``	/* php: a stray token in an `array( ... )` element is `... expecting ")"`. */`` |
|        - | 1717 | `	{` |
|   104705 | 1718 | `		const char *zSave = pGen->zClauseCloser;` |
|        - | 1719 | `		sxi32 rc;` |
|   104705 | 1720 | `		pGen->zClauseCloser = "\")\"";` |
|   104705 | 1721 | `		rc = GenStateCompileArrayBody(pGen);` |
|   104705 | 1722 | `		pGen->zClauseCloser = zSave;` |
|   104705 | 1723 | `		return rc;` |
|        - | 1724 | `	}` |
|        5 | 1725 | `}` |
|        - | 1726 | `/*` |
|        - | 1727 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|        - | 1728 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|        - | 1729 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|        - | 1730 | ` */` |
|    20770 | 1731 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 | 1732 | `{` |
|        - | 1733 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|    20775 | 1734 | `	pGen->pIn++;` |
|    20775 | 1735 | `	pGen->pEnd--;` |
|    10337 | 1736 | `	SXUNUSED(iCompileFlag);` |
|        - | 1737 | ``	/* php: a stray token in a `[ ... ]` element is `... expecting "]"`. */`` |
|        - | 1738 | `	{` |
|    20775 | 1739 | `		const char *zSave = pGen->zClauseCloser;` |
|        - | 1740 | `		sxi32 rc;` |
|    20775 | 1741 | `		pGen->zClauseCloser = "\"]\"";` |
|    20775 | 1742 | `		rc = GenStateCompileArrayBody(pGen);` |
|    20775 | 1743 | `		pGen->zClauseCloser = zSave;` |
|    20775 | 1744 | `		return rc;` |
|        - | 1745 | `	}` |
|        5 | 1746 | `}` |
|        - | 1747 | `/*` |
|        - | 1748 | ` * Expression tree validator callback for the 'list' language construct.` |
|        - | 1749 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|        - | 1750 | ` * an invalid expression tree and this function will generate the appropriate` |
|        - | 1751 | ` * error message.` |
|        - | 1752 | ` * See the routine responible of compiling the list language construct` |
|        - | 1753 | ` * for more inforation.` |
|        - | 1754 | ` */` |
|     1278 | 1755 | `static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        5 | 1756 | `{` |
|     1283 | 1757 | `	sxi32 rc = GenStateWriteTargetCheck(&(*pGen),pRoot,0);` |
|     1283 | 1758 | `	if( rc != SXRET_OK ){` |
|        3 | 1759 | `		return rc;` |
|        - | 1760 | `	}` |
|     1281 | 1761 | `	if( pRoot->pOp ){` |
|       70 | 1762 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */` |
|       35 | 1763 | `			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){` |
|        - | 1764 | `				/* Unexpected expression */` |
|      ! 0 | 1765 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - | 1766 | `					"Assignments can only happen to writable values");` |
|      ! 0 | 1767 | `				if( rc != SXERR_ABORT ){` |
|      ! 0 | 1768 | `					rc = SXERR_INVALID;` |
|      ! 0 | 1769 | `				}` |
|        2 | 1770 | `		}` |
|     1246 | 1771 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|        - | 1772 | `		/* Unexpected expression */` |
|        5 | 1773 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - | 1774 | `			"Assignments can only happen to writable values");` |
|        5 | 1775 | `		if( rc != SXERR_ABORT ){` |
|        5 | 1776 | `			rc = SXERR_INVALID;` |
|        2 | 1777 | `		}` |
|        2 | 1778 | `	}` |
|     1281 | 1779 | `	return rc;` |
|      643 | 1780 | `}` |
|        - | 1781 | `/*` |
|        - | 1782 | ` * Compile the 'list' language construct.` |
|        - | 1783 | ` *  According to the PHP language reference` |
|        - | 1784 | ` *  list(): Assign variables as if they were an array.` |
|        - | 1785 | ` *  list() is used to assign a list of variables in one operation.` |
|        - | 1786 | ` *  Description` |
|        - | 1787 | ` *   array list (mixed $varname [, mixed $... ] )` |
|        - | 1788 | ` *   Like array(), this is not really a function, but a language construct.` |
|        - | 1789 | ` *   list() is used to assign a list of variables in one operation.` |
|        - | 1790 | ` *  Parameters` |
|        - | 1791 | ` *   $varname: A variable.` |
|        - | 1792 | ` *  Return Values` |
|        - | 1793 | ` *   The assigned array.` |
|        - | 1794 | ` */` |
|        - | 1795 | `/*` |
|        - | 1796 | ` * TRUE when a destructuring target list binds at least one element BY REFERENCE` |
|        - | 1797 | `` * (`[&$a]`, `list('k' => &$a)`, at any nesting depth). A `&` that OPENS an entry`` |
|        - | 1798 | ` * is the marker -- it can only follow the list's own delimiter, a comma or a` |
|        - | 1799 | `` * `=>`, everywhere else the token is the bitwise operator.`` |
|        - | 1800 | ` *` |
|        - | 1801 | ` * Three places ask. The list body settles its entries one at a time and in source` |
|        - | 1802 | ` * ORDER when the answer is yes; the assignment reads its SOURCE in write context` |
|        - | 1803 | `` * (php's BP_VAR_W), which is what keeps `[&$t] = $undef;` silent about what it is`` |
|        - | 1804 | ` * creating; and foreach fetches the ROW rather than a copy of it, so` |
|        - | 1805 | `` * `foreach ($e as [&$x]) { $x *= 10; }` changes $e.`` |
|        - | 1806 | ` */` |
|     1887 | 1807 | `PH7_PRIVATE int PH7_GenStateListSpanHasRef(SyToken *pStart,SyToken *pEnd)` |
|        5 | 1808 | `{` |
|        - | 1809 | `	SyToken *pTok;` |
|     9311 | 1810 | `	for( pTok = pStart ; pTok < pEnd ; pTok++ ){` |
|     7478 | 1811 | `		if( (pTok->nType & PH7_TK_AMPER) == 0 \|\| pTok == pStart ){` |
|     7420 | 1812 | `			continue;` |
|        - | 1813 | `		}` |
|       59 | 1814 | `		if( pTok[-1].nType & (PH7_TK_OSB\|PH7_TK_LPAREN\|PH7_TK_COMMA) ){` |
|       55 | 1815 | `			return 1;` |
|        - | 1816 | `		}` |
|        4 | 1817 | `		if( (pTok[-1].nType & PH7_TK_OP) && pTok[-1].sData.nByte == 2` |
|        1 | 1818 | `		 && SyMemcmp((const void *)pTok[-1].sData.zString,(const void *)"=>",2) == 0 ){` |
|      ! 0 | 1819 | `			return 1;` |
|        - | 1820 | `		}` |
|        3 | 1821 | `	}` |
|     1838 | 1822 | `	return 0;` |
|      947 | 1823 | `}` |
|        - | 1824 | `/* Nested list entry recorded during first pass of list body compilation */` |
|        - | 1825 | `struct NestedListEntry {` |
|        - | 1826 | `	sxi32 nIndex;        /* Position in the outer list (0-based) */` |
|        - | 1827 | `	SyToken *pStart;     /* Token range: start of nested construct */` |
|        - | 1828 | `	SyToken *pEnd;       /* Token range: past closing delimiter */` |
|        - | 1829 | `	sxi32 isShort;       /* 1 if [...] form, 0 if list(...) form */` |
|        - | 1830 | `};` |
|        - | 1831 | `/*` |
|        - | 1832 | `` * TRUE when one destructuring ENTRY binds by reference -- itself (`&$t`) or`` |
|        - | 1833 | `` * anywhere below it (`[[&$t]]`). php fetches such a position for WRITE, which is`` |
|        - | 1834 | `` * what makes a non-array source its `Cannot use a scalar value as an array` Error`` |
|        - | 1835 | ` * rather than the read's warning-and-NULL.` |
|        - | 1836 | ` */` |
|     1256 | 1837 | `static int GenStateEntryBindsRef(SyToken *pStart,SyToken *pEnd)` |
|        5 | 1838 | `{` |
|     1261 | 1839 | `	if( pStart >= pEnd ){` |
|      ! 0 | 1840 | `		return 0;` |
|        - | 1841 | `	}` |
|     1261 | 1842 | `	if( pStart->nType & PH7_TK_AMPER/*'&'*/ ){` |
|       49 | 1843 | `		return 1;` |
|        - | 1844 | `	}` |
|     1213 | 1845 | `	return PH7_GenStateListSpanHasRef(pStart,pEnd);` |
|      632 | 1846 | `}` |
|        - | 1847 | `/*` |
|        - | 1848 | ` * Give a destructuring SOURCE a name of its own, bound to it BY REFERENCE, and` |
|        - | 1849 | ` * answer that name. Every element is then fetched through the name.` |
|        - | 1850 | ` *` |
|        - | 1851 | ` * Reading the elements off the stack value instead cannot work once an entry` |
|        - | 1852 | ` * binds by reference: each fetch would be one more stack copy of the same array,` |
|        - | 1853 | ` * and a write-context fetch through a copy COW-separates -- so the second bind of` |
|        - | 1854 | `` * `[&$x, &$z] = $e` would land in a duplicate and never reach $e. Through a name`` |
|        - | 1855 | `` * each fetch is the `$t =& $e[k]` php compiles it as. A source with no slot`` |
|        - | 1856 | `` * behind it (a call result) gets the fresh variable php's `=&` gives one.`` |
|        - | 1857 | ` *` |
|        - | 1858 | ` * The source stays on the stack, where the caller's teardown expects it.` |
|        - | 1859 | ` */` |
|       54 | 1860 | `static sxi32 GenStateNameListSource(ph7_gen_state *pGen,SyString *pOut)` |
|        1 | 1861 | `{` |
|        - | 1862 | `	static int iListSrcCnt = 0;` |
|        - | 1863 | `	char zTmp[64];` |
|        - | 1864 | `	sxu32 nLen;` |
|        - | 1865 | `	char *zDup;` |
|       55 | 1866 | `	nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__list_src_%d__]",iListSrcCnt++);` |
|       55 | 1867 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       55 | 1868 | `	if( zDup == 0 ){` |
|      ! 0 | 1869 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn ? pGen->pIn->nLine : 0,` |
|        - | 1870 | `			"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1871 | `		return SXERR_ABORT;` |
|        - | 1872 | `	}` |
|       55 | 1873 | `	SyStringInitFromBuf(pOut,zDup,nLen);` |
|        - | 1874 | `	/* STORE_REF binds the name and leaves the source where it was. */` |
|       55 | 1875 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_STORE_REF,0,0,zDup,0);` |
|       55 | 1876 | `	return SXRET_OK;` |
|       28 | 1877 | `}` |
|        - | 1878 | `/* Drop that borrowed name; the targets keep whatever they bound through it. */` |
|       54 | 1879 | `static sxi32 GenStateDropListSourceName(ph7_gen_state *pGen,SyString *pName)` |
|        1 | 1880 | `{` |
|       55 | 1881 | `	SyString *pDup = (SyString *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(SyString));` |
|       55 | 1882 | `	if( pDup == 0 ){` |
|      ! 0 | 1883 | `		PH7_GenCompileError(&(*pGen),E_ERROR,0,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1884 | `		return SXERR_ABORT;` |
|        - | 1885 | `	}` |
|       55 | 1886 | `	*pDup = *pName;` |
|       55 | 1887 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_UNSET_VAR,0,0,(void *)pDup,0);` |
|       55 | 1888 | `	return SXRET_OK;` |
|       28 | 1889 | `}` |
|        - | 1890 | `/*` |
|        - | 1891 | ` * Store the SOURCE element already on the stack top into one destructuring` |
|        - | 1892 | ` * target: compiling the target appends its lvalue load, which folds into a` |
|        - | 1893 | ` * STORE exactly as an ordinary assignment's does, and the assigned value is` |
|        - | 1894 | ` * dropped so the source array is back on top for the next entry.` |
|        - | 1895 | ` */` |
|       68 | 1896 | `static sxi32 GenStateEmitListValueStore(ph7_gen_state *pGen,SyToken *pTarget,SyToken *pEnd)` |
|        3 | 1897 | `{` |
|        - | 1898 | `	VmInstr *pInstr;` |
|       71 | 1899 | `	sxi32 iVmOp = PH7_OP_STORE;` |
|       71 | 1900 | `	sxi32 iP1 = 0,iP2 = 0;` |
|       71 | 1901 | `	void *p3 = 0;` |
|        - | 1902 | `	sxi32 rc;` |
|       71 | 1903 | `	rc = GenStateCompileArrayEntry(&(*pGen),pTarget,pEnd,` |
|        - | 1904 | `		EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|       71 | 1905 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1906 | `		return rc;` |
|        - | 1907 | `	}` |
|       71 | 1908 | `	if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|       71 | 1909 | `		if( pInstr->iOp == PH7_OP_MEMBER ){` |
|        6 | 1910 | `			iP2 = 1; /* member store: keep MEMBER, store the value below it */` |
|       68 | 1911 | `		}else if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|        3 | 1912 | `			iVmOp = PH7_OP_STORE_IDX;` |
|        3 | 1913 | `			iP1 = pInstr->iP1;` |
|        3 | 1914 | `			(void)PH7_VmPopInstr(pGen->pVm);` |
|        2 | 1915 | `		}else{` |
|       64 | 1916 | `			p3 = pInstr->p3; /* named store: $v = value */` |
|       64 | 1917 | `			(void)PH7_VmPopInstr(pGen->pVm);` |
|        - | 1918 | `		}` |
|       34 | 1919 | `	}` |
|       71 | 1920 | `	PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|       71 | 1921 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       71 | 1922 | `	return SXRET_OK;` |
|       37 | 1923 | `}` |
|        - | 1924 | `/*` |
|        - | 1925 | ` * Bind one destructuring target to its SOURCE element BY REFERENCE, which is` |
|        - | 1926 | `` * exactly what php compiles `[&$t] = $src` into: `$t =& $src[k]`. The source`` |
|        - | 1927 | ` * element is already on the stack top -- the caller pushed the source, the key and` |
|        - | 1928 | ``  * a WRITE-context OP_LOAD_IDX, so a missing key vivifies silently the way a `=&` `` |
|        - | 1929 | ` * into one does -- and this compiles the target and folds its own load into the` |
|        - | 1930 | `` * STORE_REF, the same three shapes the `=&` operator folds.`` |
|        - | 1931 | ` */` |
|       52 | 1932 | `static sxi32 GenStateEmitListRefBind(ph7_gen_state *pGen,SyToken *pTarget,SyToken *pEnd)` |
|        1 | 1933 | `{` |
|        - | 1934 | `	VmInstr *pInstr;` |
|       53 | 1935 | `	sxi32 iVmOp = PH7_OP_STORE_REF,iP1 = 0,iP2 = 0;` |
|       53 | 1936 | `	void *p3 = 0;` |
|        - | 1937 | `	sxi32 rc;` |
|       53 | 1938 | `	if( pGen->bListSrcNotRef ){` |
|        - | 1939 | `		/* php asks this at COMPILE time, where it still knows how the right-hand` |
|        - | 1940 | `		 * side was written: a value with no slot behind it cannot be referenced,` |
|        - | 1941 | `		 * and the whole statement is refused rather than binding to a temporary. */` |
|       14 | 1942 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTarget->nLine,` |
|        - | 1943 | `			"Cannot assign reference to non referenceable value");` |
|       14 | 1944 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_INVALID;` |
|        - | 1945 | `	}` |
|       39 | 1946 | `	rc = GenStateCompileArrayEntry(&(*pGen),pTarget,pEnd,EXPR_FLAG_LOAD_IDX_STORE,` |
|        - | 1947 | `		GenStateListNodeValidator);` |
|       39 | 1948 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1949 | `		return rc;` |
|        - | 1950 | `	}` |
|       39 | 1951 | `	pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|       39 | 1952 | `	if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|        - | 1953 | `		/* A property target keeps its OP_MEMBER: the VM resolves and stashes the` |
|        - | 1954 | ``		 * slot there, exactly as `$o->p =& $x` does. */`` |
|        3 | 1955 | `		pInstr->iP2 = PH7_MEMBER_REF_TARGET;` |
|        3 | 1956 | `		iP2 = 1;` |
|       38 | 1957 | `	}else if( (pInstr = PH7_VmPopInstr(pGen->pVm)) != 0 ){` |
|       37 | 1958 | `		if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|        3 | 1959 | `			iVmOp = PH7_OP_STORE_IDX_REF;` |
|        3 | 1960 | `			iP1 = pInstr->iP1;` |
|        3 | 1961 | `			iP2 = pInstr->iP2;` |
|        3 | 1962 | `			p3  = pInstr->p3;` |
|        2 | 1963 | `		}else{` |
|       35 | 1964 | `			p3 = pInstr->p3;` |
|        - | 1965 | `		}` |
|       18 | 1966 | `	}` |
|       39 | 1967 | `	PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|        - | 1968 | `	/* The bind leaves the source element behind; drop it so the source array is` |
|        - | 1969 | `	 * back on the stack top for the next entry. */` |
|       39 | 1970 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       39 | 1971 | `	return SXRET_OK;` |
|       27 | 1972 | `}` |
|        - | 1973 | `/*` |
|        - | 1974 | ` * Compile the body of a *keyed* list/short-list destructuring (PHP 7.1), where` |
|        - | 1975 | `` * every entry has the form `keyExpr => target`. The source array is on the stack`` |
|        - | 1976 | ` * top on entry and remains there on exit, mirroring the positional LOAD_LIST` |
|        - | 1977 | ` * path so the caller's teardown is unchanged. For each entry: DUP the source,` |
|        - | 1978 | ` * push the key, LOAD_IDX to fetch source[key] (NULL on a missing key, silently,` |
|        - | 1979 | ` * like a normal subscript read), then assign the fetched value to the target — a` |
|        - | 1980 | ` * nested [...]/list() recurses, a simple lvalue uses the same STORE fold as a` |
|        - | 1981 | ` * normal assignment (the value sits below the lvalue-load, exactly as in` |
|        - | 1982 | ` * GenStateEmitExprCode where the assignment RHS precedes the LHS load).` |
|        - | 1983 | ` */` |
|       50 | 1984 | `static sxi32 GenStateCompileKeyedListBody(ph7_gen_state *pGen)` |
|        3 | 1985 | `{` |
|       53 | 1986 | `	SyString sTmp = { 0, 0 };` |
|        - | 1987 | `	SyToken *pNext,*pScanIn;` |
|       53 | 1988 | `	int bAnyRef = 0;` |
|        - | 1989 | `	sxi32 rc;` |
|        - | 1990 | `	/* Does any entry bind BY REFERENCE? If so the source is fetched through a NAME` |
|        - | 1991 | `	 * (GenStateNameListSource) rather than off the stack, for the reason recorded` |
|        - | 1992 | `	 * there. */` |
|       53 | 1993 | `	pScanIn = pGen->pIn;` |
|      115 | 1994 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|       65 | 1995 | `		SyToken *pArrow = GenStateFindTopLevelArrow(pGen->pIn,pNext);` |
|       65 | 1996 | `		if( pArrow < pNext && &pArrow[1] < pNext && (pArrow[1].nType & PH7_TK_AMPER/*'&'*/) ){` |
|        5 | 1997 | `			bAnyRef = 1;` |
|        2 | 1998 | `		}` |
|       65 | 1999 | `		pGen->pIn = &pNext[1];` |
|        3 | 2000 | `	}` |
|       53 | 2001 | `	pGen->pIn = pScanIn;` |
|       53 | 2002 | `	if( bAnyRef && GenStateNameListSource(pGen,&sTmp) != SXRET_OK ){` |
|      ! 0 | 2003 | `		return SXERR_ABORT;` |
|        - | 2004 | `	}` |
|      115 | 2005 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|        - | 2006 | `		SyToken *pArrow,*pTarget;` |
|        - | 2007 | ``		/* Split `keyExpr => target` at the top-level '=>' */`` |
|       65 | 2008 | `		pArrow = GenStateFindTopLevelArrow(pGen->pIn,pNext);` |
|       65 | 2009 | `		pTarget = &pArrow[1];` |
|       65 | 2010 | `		if( pArrow <= pGen->pIn \|\| pTarget >= pNext ){` |
|        - | 2011 | ``			/* Empty key (`[ => $v]`) or empty value (`["k" =>]`): PHP rejects`` |
|        - | 2012 | `			 * both. Reject rather than silently emitting unbalanced bytecode. */` |
|      ! 0 | 2013 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - | 2014 | `				"Cannot use empty array entries in keyed array assignment");` |
|      ! 0 | 2015 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 2016 | `		}` |
|        - | 2017 | `		/* Put the source array back on the stack: a DUP of the value sitting there,` |
|        - | 2018 | `		 * or -- when an entry of this list binds by reference -- a load of the name` |
|        - | 2019 | `		 * it was given, which is what makes a write-context fetch reach the source` |
|        - | 2020 | `		 * rather than a copy of it. */` |
|       65 | 2021 | `		if( bAnyRef ){` |
|        7 | 2022 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&sTmp),0);` |
|        4 | 2023 | `		}else{` |
|       59 | 2024 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|        - | 2025 | `		}` |
|        - | 2026 | `		/* Compile the key expression; it is pushed above the DUP'd source */` |
|       65 | 2027 | `		rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pArrow,EXPR_FLAG_RDONLY_LOAD,0);` |
|       65 | 2028 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2029 | `			return SXERR_ABORT;` |
|        - | 2030 | `		}` |
|       65 | 2031 | `		if( pTarget < pNext && (pTarget->nType & PH7_TK_AMPER/*'&'*/) ){` |
|        - | 2032 | ``			/* `['k' => &$t] = $src`: bind the target to the SOURCE element rather`` |
|        - | 2033 | `			 * than storing a copy into it. The fetch is a WRITE-context one, so a` |
|        - | 2034 | ``			 * missing key vivifies silently -- php's `$t =& $src['k']`. */`` |
|        5 | 2035 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,1,0,0);` |
|        5 | 2036 | `			rc = GenStateEmitListRefBind(&(*pGen),&pTarget[1],pNext);` |
|        5 | 2037 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2038 | `				return SXERR_ABORT;` |
|        - | 2039 | `			}` |
|        5 | 2040 | `			pGen->pIn = &pNext[1];` |
|        5 | 2041 | `			continue;` |
|        - | 2042 | `		}` |
|        - | 2043 | `		/* LOAD_IDX: pop the key, replace the DUP'd source with source[key].` |
|        - | 2044 | `		 * iP2=7 is the keyed-destructuring read context: an array source reads like` |
|        - | 2045 | ``		 * iP2=0 (a missing key loads NULL and warns `Undefined array key "k"`, php's`` |
|        - | 2046 | `		 * answer for the keyed spelling as much as for the positional one),` |
|        - | 2047 | `		 * but a NON-array source yields NULL + a per-key "Cannot use <type> as array"` |
|        - | 2048 | `		 * warning instead of char-indexing a string (matching PHP's OP_LOAD_LIST path). */` |
|       61 | 2049 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,7,0,0);` |
|       61 | 2050 | `		if( pTarget < pNext && ( (pTarget->nType & PH7_TK_OSB)` |
|       56 | 2051 | `			\|\| ( (pTarget->nType & PH7_TK_KEYWORD)` |
|       29 | 2052 | `				&& SX_PTR_TO_INT(pTarget->pUserData) == PH7_TKWRD_LIST ) ) ){` |
|        - | 2053 | `			/* Nested destructuring:  ["k" => [ ... ]]  or  ["k" => list( ... )].` |
|        - | 2054 | `			 * Treat source[key] as the inner body's source, then drop the` |
|        - | 2055 | `			 * leftover it leaves behind (mirrors the positional nested path). */` |
|        5 | 2056 | `			sxi32 isShort = (pTarget->nType & PH7_TK_OSB) != 0;` |
|        5 | 2057 | `			SyToken *pSavedIn = pGen->pIn;` |
|        5 | 2058 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|        5 | 2059 | `			pGen->pIn = pTarget;` |
|        5 | 2060 | `			pGen->pEnd = pNext;` |
|        5 | 2061 | `			rc = isShort ? PH7_CompileShortList(&(*pGen),0)` |
|        2 | 2062 | `			             : PH7_CompileList(&(*pGen),0);` |
|        5 | 2063 | `			pGen->pIn = pSavedIn;` |
|        5 | 2064 | `			pGen->pEnd = pSavedEnd;` |
|        5 | 2065 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2066 | `				return SXERR_ABORT;` |
|        - | 2067 | `			}` |
|        5 | 2068 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        3 | 2069 | `		}else{` |
|       57 | 2070 | `			rc = GenStateEmitListValueStore(&(*pGen),pTarget,pNext);` |
|       57 | 2071 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 2072 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 2073 | `			}` |
|        - | 2074 | `		}` |
|       61 | 2075 | `		pGen->pIn = &pNext[1];` |
|        3 | 2076 | `	}` |
|       53 | 2077 | `	if( bAnyRef ){` |
|        5 | 2078 | `		return GenStateDropListSourceName(pGen,&sTmp);` |
|        - | 2079 | `	}` |
|       49 | 2080 | `	return SXRET_OK;` |
|       28 | 2081 | `}` |
|        - | 2082 | `/*` |
|        - | 2083 | ` * Compile a POSITIONAL destructuring body one entry at a time, in SOURCE ORDER,` |
|        - | 2084 | ` * instead of pushing every target and letting OP_LOAD_LIST assign them together.` |
|        - | 2085 | ` *` |
|        - | 2086 | ` * This is the shape php always has, and it only matters once an entry binds BY` |
|        - | 2087 | ` * REFERENCE: the bind has to happen where it was written, because a later entry` |
|        - | 2088 | `` * may write THROUGH it. `[&$y, $y] = [1, 2]` binds $y to element 0 and then`` |
|        - | 2089 | ` * assigns element 1's value to $y -- which lands in element 0, so php answers` |
|        - | 2090 | `` * `[2, 2]`. Assigning first and binding afterwards answers `[1, 2]`.`` |
|        - | 2091 | ` *` |
|        - | 2092 | ` * The source array is on the stack top on entry and stays there, exactly as the` |
|        - | 2093 | ` * keyed body leaves it.` |
|        - | 2094 | ` */` |
|       50 | 2095 | `static sxi32 GenStateCompileSeqListBody(ph7_gen_state *pGen)` |
|        1 | 2096 | `{` |
|        - | 2097 | `	SyString sTmp;` |
|        - | 2098 | `	SyToken *pNext;` |
|       51 | 2099 | `	sxi32 nIndex = 0;` |
|        - | 2100 | `	sxi32 rc;` |
|       51 | 2101 | `	if( GenStateNameListSource(pGen,&sTmp) != SXRET_OK ){` |
|      ! 0 | 2102 | `		return SXERR_ABORT;` |
|        - | 2103 | `	}` |
|      119 | 2104 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|       69 | 2105 | `		SyToken *pTarget = pGen->pIn;` |
|        - | 2106 | `		int bRef,bNested,bShort;` |
|       69 | 2107 | `		if( pTarget >= pNext ){` |
|        - | 2108 | `			/* An empty slot ([, $b]) skips its index and touches nothing. */` |
|        3 | 2109 | `			pGen->pIn = &pNext[1];` |
|        3 | 2110 | `			nIndex++;` |
|        3 | 2111 | `			continue;` |
|        - | 2112 | `		}` |
|       67 | 2113 | `		bRef = (pTarget->nType & PH7_TK_AMPER/*'&'*/) != 0;` |
|       67 | 2114 | `		if( bRef ){` |
|       49 | 2115 | `			pTarget++;` |
|       49 | 2116 | `			if( pTarget >= pNext ){` |
|        - | 2117 | ``				/* `[&] = $src`: php names the token that stopped it. */`` |
|      ! 0 | 2118 | `				rc = PH7_GenSyntaxError(&(*pGen),pNext < pGen->pEnd ? pNext : 0,0);` |
|      ! 0 | 2119 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 2120 | `			}` |
|       24 | 2121 | `		}` |
|       67 | 2122 | `		bShort = (pTarget->nType & PH7_TK_OSB) != 0;` |
|       67 | 2123 | `		bNested = bShort \|\| ( (pTarget->nType & PH7_TK_KEYWORD)` |
|       31 | 2124 | `			&& SX_PTR_TO_INT(pTarget->pUserData) == PH7_TKWRD_LIST );` |
|       67 | 2125 | `		if( bNested && PH7_GenStateListSpanHasRef(pTarget,pNext) ){` |
|        - | 2126 | `			/* A nested level that binds by reference is a WRITE position for php as` |
|        - | 2127 | ``			 * much as a `&` target is: the bind below has to reach this element. */`` |
|        5 | 2128 | `			bRef = 1;` |
|        2 | 2129 | `		}` |
|        - | 2130 | `		/* Fetch source[index]. A by-REF position reads it in WRITE context -- which` |
|        - | 2131 | `		 * vivifies a missing key silently and makes a non-array source php's` |
|        - | 2132 | ``		 * `Cannot use a scalar value as an array` Error -- while every other one`` |
|        - | 2133 | `		 * takes the destructuring READ (iP2=7): a missing key warns, and a source` |
|        - | 2134 | ``		 * that is not an array at all warns `Cannot use <type> as array` once for`` |
|        - | 2135 | `		 * this position, which is what php's list assign says per position. */` |
|       67 | 2136 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&sTmp),0);` |
|        - | 2137 | `		{` |
|        - | 2138 | `			ph7_value *pIdx;` |
|        - | 2139 | `			sxu32 nConstIdx;` |
|       67 | 2140 | `			pIdx = PH7_ReserveConstObj(pGen->pVm,&nConstIdx);` |
|       67 | 2141 | `			if( pIdx == 0 ){` |
|      ! 0 | 2142 | `				PH7_GenCompileError(&(*pGen),E_ERROR,pTarget->nLine,` |
|        - | 2143 | `					"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 2144 | `				return SXERR_ABORT;` |
|        - | 2145 | `			}` |
|       67 | 2146 | `			PH7_MemObjInitFromInt(pGen->pVm,pIdx,(sxi64)nIndex);` |
|       67 | 2147 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,(sxi32)nConstIdx,0,0);` |
|        - | 2148 | `		}` |
|       67 | 2149 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,bRef ? 1 : 7,0,0);` |
|       67 | 2150 | `		if( bNested ){` |
|        5 | 2151 | `			SyToken *pSavedIn = pGen->pIn,*pSavedEnd = pGen->pEnd;` |
|        5 | 2152 | `			pGen->pIn = pTarget;` |
|        5 | 2153 | `			pGen->pEnd = pNext;` |
|        5 | 2154 | `			rc = bShort ? PH7_CompileShortList(&(*pGen),0) : PH7_CompileList(&(*pGen),0);` |
|        5 | 2155 | `			pGen->pIn = pSavedIn;` |
|        5 | 2156 | `			pGen->pEnd = pSavedEnd;` |
|        5 | 2157 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2158 | `				return SXERR_ABORT;` |
|        - | 2159 | `			}` |
|        - | 2160 | `			/* Drop the element the inner body left behind. */` |
|        5 | 2161 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       65 | 2162 | `		}else if( bRef ){` |
|       49 | 2163 | `			rc = GenStateEmitListRefBind(&(*pGen),pTarget,pNext);` |
|       49 | 2164 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2165 | `				return SXERR_ABORT;` |
|        - | 2166 | `			}` |
|       25 | 2167 | `		}else{` |
|       15 | 2168 | `			rc = GenStateEmitListValueStore(&(*pGen),pTarget,pNext);` |
|       15 | 2169 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2170 | `				return SXERR_ABORT;` |
|        - | 2171 | `			}` |
|        - | 2172 | `		}` |
|       67 | 2173 | `		pGen->pIn = &pNext[1];` |
|       67 | 2174 | `		nIndex++;` |
|        1 | 2175 | `	}` |
|       51 | 2176 | `	return GenStateDropListSourceName(pGen,&sTmp);` |
|       26 | 2177 | `}` |
|        - | 2178 | `/*` |
|        - | 2179 | ` * Shared body for list() and short list [...] compilation.` |
|        - | 2180 | ` * Assumes pGen->pIn and pGen->pEnd are already positioned past` |
|        - | 2181 | ` * the opening delimiter and before the closing delimiter.` |
|        - | 2182 | ` */` |
|      701 | 2183 | `static sxi32 GenStateCompileListBody(ph7_gen_state *pGen)` |
|        5 | 2184 | `{` |
|        - | 2185 | `	SySet sNested; /* Dynamically-sized container of NestedListEntry */` |
|        - | 2186 | `	SyToken *pNext;` |
|        - | 2187 | `	SyToken *pClassifyIn;` |
|      706 | 2188 | `	sxi32 nKeyed = 0, nPositional = 0, nEmpty = 0, nRefBind = 0;` |
|        - | 2189 | `	sxi32 nExpr;` |
|        - | 2190 | `	sxi32 rc;` |
|        - | 2191 | ``	/* First pass: classify entries as keyed (`k => v`), positional, or empty`` |
|        - | 2192 | `	 * skip slots ([,]). A list level must be entirely keyed or entirely` |
|        - | 2193 | `	 * positional — PHP fatals on a mix, and on an empty slot inside a keyed` |
|        - | 2194 | `	 * list. */` |
|      706 | 2195 | `	pClassifyIn = pGen->pIn;` |
|     2052 | 2196 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|     1351 | 2197 | `		if( pGen->pIn >= pNext ){` |
|       29 | 2198 | `			nEmpty++;` |
|     1337 | 2199 | `		}else if( GenStateFindTopLevelArrow(pGen->pIn,pNext) < pNext ){` |
|       65 | 2200 | `			nKeyed++;` |
|       34 | 2201 | `		}else{` |
|     1261 | 2202 | `			if( GenStateEntryBindsRef(pGen->pIn,pNext) ){` |
|       53 | 2203 | `				nRefBind++;` |
|       26 | 2204 | `			}` |
|     1261 | 2205 | `			nPositional++;` |
|        - | 2206 | `		}` |
|     1351 | 2207 | `		pGen->pIn = &pNext[1];` |
|        5 | 2208 | `	}` |
|      706 | 2209 | `	pGen->pIn = pClassifyIn;` |
|      706 | 2210 | `	if( nKeyed < 1 && nPositional < 1 ){` |
|        - | 2211 | ``		/* `[, ,] = $src` fills nothing: php refuses the whole construct rather than`` |
|        - | 2212 | `		 * running a destructure with no targets, which is what this used to do. */` |
|        4 | 2213 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn < pGen->pEnd ? pGen->pIn->nLine : 0,` |
|        - | 2214 | `			"Cannot use empty list");` |
|        4 | 2215 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 2216 | `	}` |
|      702 | 2217 | `	if( nKeyed > 0 && nEmpty > 0 ){` |
|      ! 0 | 2218 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - | 2219 | `			"Cannot use empty array entries in keyed array assignment");` |
|      ! 0 | 2220 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 2221 | `	}` |
|      702 | 2222 | `	if( nKeyed > 0 && nPositional > 0 ){` |
|      ! 0 | 2223 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - | 2224 | `			"Cannot mix keyed and unkeyed array entries in assignments");` |
|      ! 0 | 2225 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 2226 | `	}` |
|      702 | 2227 | `	if( nKeyed > 0 ){` |
|       53 | 2228 | `		return GenStateCompileKeyedListBody(pGen);` |
|        - | 2229 | `	}` |
|      652 | 2230 | `	if( nRefBind > 0 ){` |
|        - | 2231 | `		/* An entry binds BY REFERENCE, so the entries are settled one at a time and` |
|        - | 2232 | `		 * in SOURCE ORDER rather than pushed together for OP_LOAD_LIST to assign:` |
|        - | 2233 | `		 * a later entry may write THROUGH a bind an earlier one made. */` |
|       51 | 2234 | `		return GenStateCompileSeqListBody(pGen);` |
|        - | 2235 | `	}` |
|      602 | 2236 | `	nExpr = 0;` |
|      602 | 2237 | `	SySetInit(&sNested,&pGen->pVm->sAllocator,sizeof(struct NestedListEntry));` |
|     1812 | 2238 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|     1215 | 2239 | `		if( pGen->pIn < pNext ){` |
|        - | 2240 | `			/* Check for nested list() */` |
|     1195 | 2241 | `			if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        3 | 2242 | `				SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|        - | 2243 | `				/* Record this nested list for post-processing */` |
|        3 | 2244 | `				SyToken *pListEnd = 0;` |
|        3 | 2245 | `				if( &pGen->pIn[1] < pNext && (pGen->pIn[1].nType & PH7_TK_LPAREN) ){` |
|        3 | 2246 | `					PH7_DelimitNestedTokens(pGen->pIn+2,pNext,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|        1 | 2247 | `				}` |
|        3 | 2248 | `				if( pListEnd ){` |
|        - | 2249 | `					struct NestedListEntry sEntry;` |
|        3 | 2250 | `					sEntry.nIndex = nExpr;` |
|        3 | 2251 | `					sEntry.pStart = pGen->pIn;` |
|        3 | 2252 | `					sEntry.pEnd = pListEnd + 1;` |
|        3 | 2253 | `					sEntry.isShort = 0;` |
|        3 | 2254 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|        1 | 2255 | `				}` |
|        - | 2256 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|        3 | 2257 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|     1194 | 2258 | `			}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|        - | 2259 | `				/* Nested short destructuring [...] */` |
|       18 | 2260 | `				SyToken *pBracketEnd = 0;` |
|       18 | 2261 | `				PH7_DelimitNestedTokens(pGen->pIn+1,pNext,PH7_TK_OSB,PH7_TK_CSB,&pBracketEnd);` |
|       18 | 2262 | `				if( pBracketEnd ){` |
|        - | 2263 | `					struct NestedListEntry sEntry;` |
|       18 | 2264 | `					sEntry.nIndex = nExpr;` |
|       18 | 2265 | `					sEntry.pStart = pGen->pIn;` |
|       18 | 2266 | `					sEntry.pEnd = pBracketEnd + 1;` |
|       18 | 2267 | `					sEntry.isShort = 1;` |
|       18 | 2268 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|        8 | 2269 | `				}` |
|        - | 2270 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|       18 | 2271 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       10 | 2272 | `			}else{` |
|        - | 2273 | `				/* Compile the expression holding the variable */` |
|     1177 | 2274 | `				rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|     1177 | 2275 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 2276 | `					SySetRelease(&sNested);` |
|      ! 0 | 2277 | `					return SXRET_OK;` |
|        - | 2278 | `				}` |
|        - | 2279 | `				{` |
|        - | 2280 | `					/* A property target ($o->p / Cls::$s) is a PURE WRITE here — the` |
|        - | 2281 | `					 * value lands via the following OP_LOAD_LIST's direct slot store.` |
|        - | 2282 | `					 * Tag the member so OP_MEMBER skips the uninitialized-typed read` |
|        - | 2283 | `					 * Error / __get consult and vivifies a missing property (php` |
|        - | 2284 | `					 * assigns without reading). */` |
|     1177 | 2285 | `					VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|     1177 | 2286 | `					if( pLast && pLast->iOp == PH7_OP_MEMBER && pLast->iP2 == PH7_MEMBER_READ ){` |
|       58 | 2287 | `						pLast->iP2 = PH7_MEMBER_LIST_TARGET;` |
|       28 | 2288 | `					}` |
|        - | 2289 | `				}` |
|        - | 2290 | `			}` |
|      599 | 2291 | `		}else{` |
|        - | 2292 | `			/* Empty entry,load NULL */` |
|       21 | 2293 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);` |
|        - | 2294 | `		}` |
|     1215 | 2295 | `		nExpr++;` |
|        - | 2296 | `		/* Advance the stream cursor */` |
|     1215 | 2297 | `		pGen->pIn = &pNext[1];` |
|        5 | 2298 | `	}` |
|        - | 2299 | `	/* Emit the LOAD_LIST instruction. P2 is how many entries FILL a position -- the` |
|        - | 2300 | `	 * empty slots left out -- which is how many times php complains when the source` |
|        - | 2301 | `	 * is not an array at all: it asks the source once per position it means to fill. */` |
|      602 | 2302 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,nExpr - nEmpty,0,0);` |
|        - | 2303 | `	/* After LOAD_LIST, the source array is still on the stack top.` |
|        - | 2304 | `	 * For each nested entry, emit code to extract the sub-array` |
|        - | 2305 | `	 * at the corresponding index and recursively destructure it.` |
|        - | 2306 | `	 */` |
|      602 | 2307 | `	if( SySetUsed(&sNested) > 0 ){` |
|       18 | 2308 | `		struct NestedListEntry *apNested = (struct NestedListEntry *)SySetBasePtr(&sNested);` |
|        - | 2309 | `		sxu32 i;` |
|       36 | 2310 | `		for(i = 0; i < SySetUsed(&sNested); i++){` |
|       20 | 2311 | `			SyToken *pSavedIn = pGen->pIn;` |
|       20 | 2312 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|        - | 2313 | `			ph7_value *pIdx;` |
|        - | 2314 | `			sxu32 nConstIdx;` |
|        - | 2315 | `			/* DUP the source array (it's on stack top) */` |
|       20 | 2316 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|        - | 2317 | `			/* Push the integer index for this nested entry */` |
|       20 | 2318 | `			pIdx = PH7_ReserveConstObj(pGen->pVm,&nConstIdx);` |
|       20 | 2319 | `			if( pIdx == 0 ){` |
|      ! 0 | 2320 | `				PH7_GenCompileError(&(*pGen),E_ERROR,0,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 2321 | `				SySetRelease(&sNested);` |
|      ! 0 | 2322 | `				return SXERR_ABORT;` |
|        - | 2323 | `			}` |
|       20 | 2324 | `			PH7_MemObjInitFromInt(pGen->pVm,pIdx,(sxi64)apNested[i].nIndex);` |
|       20 | 2325 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nConstIdx,0,0);` |
|        - | 2326 | `			/* LOAD_IDX: pop index, replace DUP'd source with source[index].` |
|        - | 2327 | `			 * iP2=2 signals the VM to emit an "Undefined array key" warning` |
|        - | 2328 | `			 * when the key is missing (PHP-compatible list destructuring).` |
|        - | 2329 | `			 */` |
|       20 | 2330 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,2,0,0);` |
|        - | 2331 | `			/* Recursively compile the inner list */` |
|       20 | 2332 | `			pGen->pIn = apNested[i].pStart;` |
|       20 | 2333 | `			pGen->pEnd = apNested[i].pEnd;` |
|       20 | 2334 | `			if( apNested[i].isShort ){` |
|       18 | 2335 | `				rc = PH7_CompileShortList(&(*pGen),0);` |
|       10 | 2336 | `			}else{` |
|        3 | 2337 | `				rc = PH7_CompileList(&(*pGen),0);` |
|        - | 2338 | `			}` |
|       20 | 2339 | `			pGen->pIn = pSavedIn;` |
|       20 | 2340 | `			pGen->pEnd = pSavedEnd;` |
|       20 | 2341 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2342 | `				SySetRelease(&sNested);` |
|      ! 0 | 2343 | `				return SXERR_ABORT;` |
|        - | 2344 | `			}` |
|        - | 2345 | `			/* Pop the leftover source[index] from the inner LOAD_LIST */` |
|       20 | 2346 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       11 | 2347 | `		}` |
|        8 | 2348 | `	}` |
|      602 | 2349 | `	SySetRelease(&sNested);` |
|        - | 2350 | `	/* Node successfully compiled */` |
|      602 | 2351 | `	return SXRET_OK;` |
|      355 | 2352 | `}` |
|       66 | 2353 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 | 2354 | `{` |
|        - | 2355 | `	/* Jump the 'list' keyword, the leading '(' and exclude trailing ')' */` |
|       71 | 2356 | `	pGen->pIn += 2;` |
|       71 | 2357 | `	pGen->pEnd--;` |
|       33 | 2358 | `	SXUNUSED(iCompileFlag);` |
|       71 | 2359 | `	return GenStateCompileListBody(pGen);` |
|        5 | 2360 | `}` |
|      635 | 2361 | `PH7_PRIVATE sxi32 PH7_CompileShortList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 | 2362 | `{` |
|        - | 2363 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|      640 | 2364 | `	pGen->pIn++;` |
|      640 | 2365 | `	pGen->pEnd--;` |
|      317 | 2366 | `	SXUNUSED(iCompileFlag);` |
|      640 | 2367 | `	return GenStateCompileListBody(pGen);` |
|        5 | 2368 | `}` |
|        - | 2369 | `/*` |
|        - | 2370 | ` * assert() source-text rendering.` |
|        - | 2371 | ` *` |
|        - | 2372 | ` * php compiles a DIRECT assert() call with a copy of the argument's AST, and a` |
|        - | 2373 | `` * failing assertion reports zend_ast_export() of that AST — `assert(1 == 2)`,`` |
|        - | 2374 | `` * `assert($x)`, `assert('')` — which is exactly the information the message`` |
|        - | 2375 | ` * exists to carry. PHL has no AST copy at runtime, so the compiler renders the` |
|        - | 2376 | ` * argument's TOKEN SPAN here, at compile time, normalizing to php's export` |
|        - | 2377 | ` * shape (each rule probed against php 8.5):` |
|        - | 2378 | ` *   - literal values fold the way php's AST holds them: numbers render from` |
|        - | 2379 | ` *     their parsed VALUE (0x10 -> 16, 1e3 -> 1000.0, 1_000 -> 1000, an` |
|        - | 2380 | ` *     int64-overflowing literal -> float), strings render single-quoted with` |
|        - | 2381 | ` *     their PROCESSED contents (\ and ' re-escaped), array(...) -> [...].` |
|        - | 2382 | ` *   - one space around binary operators, ", " between arguments/elements, no` |
|        - | 2383 | `` *     space inside ()/[] or around ->/?->/::/casts, `and`/`or` -> `&&`/`\|\|`,`` |
|        - | 2384 | ` *     a trailing comma is dropped, redundant OUTERMOST parens are dropped.` |
|        - | 2385 | ` * Accepted divergences from zend_ast_export on exotic input (message text` |
|        - | 2386 | ` * only, never behavior): redundant INNER parens are kept (php re-derives` |
|        - | 2387 | ` * grouping from precedence), interpolated "$x" strings and heredocs render as` |
|        - | 2388 | `` * written (php exports its interpolation AST), `new C` does not grow php's`` |
|        - | 2389 | `` * trailing `()`, and constant folding beyond single literals is not applied`` |
|        - | 2390 | `` * (php renders `'' . ''` as `''`).`` |
|        - | 2391 | ` */` |
|        - | 2392 | `/* Spacing classes: a space is inserted between two tokens when either side` |
|        - | 2393 | ` * FORCEs one (binary operators, the slot after a comma) or both sides are` |
|        - | 2394 | ` * operand-like (WANT). Grouping punctuation and glue operators contribute` |
|        - | 2395 | ` * NONE on their tight side. */` |
|        - | 2396 | `#define ASRT_SP_NONE  0` |
|        - | 2397 | `#define ASRT_SP_WANT  1` |
|        - | 2398 | `#define ASRT_SP_FORCE 2` |
|        - | 2399 | `enum AssertTokClass {` |
|        - | 2400 | `	ASRT_START = 0, /* virtual class before the first token */` |
|        - | 2401 | `	ASRT_OPERAND,   /* literals, identifiers, keywords */` |
|        - | 2402 | `	ASRT_BINOP,     /* == + . && ? : => instanceof ... */` |
|        - | 2403 | `	ASRT_UNARY,     /* ! ~ @ - + & casts, '$', '...' — glue after */` |
|        - | 2404 | `	ASRT_OPEN,      /* ( [ */` |
|        - | 2405 | `	ASRT_CLOSE,     /* ) ] */` |
|        - | 2406 | `	ASRT_GLUE,      /* -> ?-> :: ++ -- \ — glue both sides */` |
|        - | 2407 | `	ASRT_COMMA      /* , — glue before, force after */` |
|        - | 2408 | `};` |
|        - | 2409 | `static const sxu8 aAsrtBefore[] = { ASRT_SP_NONE, ASRT_SP_WANT, ASRT_SP_FORCE, ASRT_SP_WANT,` |
|        - | 2410 | `	ASRT_SP_NONE, ASRT_SP_NONE, ASRT_SP_NONE, ASRT_SP_NONE };` |
|        - | 2411 | `static const sxu8 aAsrtAfter[]  = { ASRT_SP_NONE, ASRT_SP_WANT, ASRT_SP_FORCE, ASRT_SP_NONE,` |
|        - | 2412 | `	ASRT_SP_NONE, ASRT_SP_WANT, ASRT_SP_NONE, ASRT_SP_FORCE };` |
|        - | 2413 | `/*` |
|        - | 2414 | ` * Append one PROCESSED string-value byte, re-escaped for a single-quoted` |
|        - | 2415 | ` * rendering: php's export escapes only backslash and the quote itself; every` |
|        - | 2416 | ` * other byte (including control characters) is emitted raw.` |
|        - | 2417 | ` */` |
|       42 | 2418 | `static void AssertRenderQuotedByte(SyBlob *pOut,int c)` |
|        2 | 2419 | `{` |
|       44 | 2420 | `	char ch = (char)c;` |
|       44 | 2421 | `	if( c == '\\' \|\| c == '\'' ){` |
|        3 | 2422 | `		SyBlobAppend(pOut,"\\",1);` |
|        1 | 2423 | `	}` |
|       44 | 2424 | `	SyBlobAppend(pOut,&ch,1);` |
|       44 | 2425 | `}` |
|        - | 2426 | `/* Append the UTF-8 encoding of a \u{...} code point (value bytes, re-escaped). */` |
|      ! 0 | 2427 | `static void AssertRenderUtf8(SyBlob *pOut,sxu32 c)` |
|      ! 0 | 2428 | `{` |
|      ! 0 | 2429 | `	if( c < 0x80 ){` |
|      ! 0 | 2430 | `		AssertRenderQuotedByte(pOut,(int)c);` |
|      ! 0 | 2431 | `	}else if( c < 0x800 ){` |
|      ! 0 | 2432 | `		AssertRenderQuotedByte(pOut,(int)(0xc0 \| (c >> 6)));` |
|      ! 0 | 2433 | `		AssertRenderQuotedByte(pOut,(int)(0x80 \| (c & 0x3f)));` |
|      ! 0 | 2434 | `	}else if( c < 0x10000 ){` |
|      ! 0 | 2435 | `		AssertRenderQuotedByte(pOut,(int)(0xe0 \| (c >> 12)));` |
|      ! 0 | 2436 | `		AssertRenderQuotedByte(pOut,(int)(0x80 \| ((c >> 6) & 0x3f)));` |
|      ! 0 | 2437 | `		AssertRenderQuotedByte(pOut,(int)(0x80 \| (c & 0x3f)));` |
|      ! 0 | 2438 | `	}else{` |
|      ! 0 | 2439 | `		AssertRenderQuotedByte(pOut,(int)(0xf0 \| (c >> 18)));` |
|      ! 0 | 2440 | `		AssertRenderQuotedByte(pOut,(int)(0x80 \| ((c >> 12) & 0x3f)));` |
|      ! 0 | 2441 | `		AssertRenderQuotedByte(pOut,(int)(0x80 \| ((c >> 6) & 0x3f)));` |
|      ! 0 | 2442 | `		AssertRenderQuotedByte(pOut,(int)(0x80 \| (c & 0x3f)));` |
|        - | 2443 | `	}` |
|      ! 0 | 2444 | `}` |
|        - | 2445 | `/*` |
|        - | 2446 | ` * Render a single-quoted-source string (or nowdoc body): only \\ and \' are` |
|        - | 2447 | ` * escape sequences there; any other backslash is a literal byte.` |
|        - | 2448 | ` */` |
|        2 | 2449 | `static void AssertRenderSglString(SyBlob *pOut,const char *z,sxu32 n)` |
|        1 | 2450 | `{` |
|        3 | 2451 | `	sxu32 i = 0;` |
|        3 | 2452 | `	SyBlobAppend(pOut,"'",1);` |
|       11 | 2453 | `	while( i < n ){` |
|        9 | 2454 | `		if( z[i] == '\\' && i + 1 < n && (z[i+1] == '\\' \|\| z[i+1] == '\'') ){` |
|        3 | 2455 | `			AssertRenderQuotedByte(pOut,z[i+1]);` |
|        3 | 2456 | `			i += 2;` |
|        2 | 2457 | `		}else{` |
|        7 | 2458 | `			AssertRenderQuotedByte(pOut,z[i]);` |
|        7 | 2459 | `			i++;` |
|        - | 2460 | `		}` |
|        1 | 2461 | `	}` |
|        3 | 2462 | `	SyBlobAppend(pOut,"'",1);` |
|        3 | 2463 | `}` |
|        - | 2464 | `/*` |
|        - | 2465 | ` * Render a double-quoted-source string (or heredoc body) with php's escape` |
|        - | 2466 | ` * processing — the value bytes are what php's AST holds, and the export prints` |
|        - | 2467 | ` * them single-quoted. An UNKNOWN escape keeps the backslash and the character,` |
|        - | 2468 | ` * matching php's string semantics.` |
|        - | 2469 | ` */` |
|       12 | 2470 | `static void AssertRenderDblString(SyBlob *pOut,const char *z,sxu32 n)` |
|        3 | 2471 | `{` |
|       15 | 2472 | `	sxu32 i = 0;` |
|       15 | 2473 | `	SyBlobAppend(pOut,"'",1);` |
|       49 | 2474 | `	while( i < n ){` |
|       36 | 2475 | `		int c = z[i];` |
|        - | 2476 | `		int d;` |
|       36 | 2477 | `		if( c != '\\' \|\| i + 1 >= n ){` |
|       36 | 2478 | `			AssertRenderQuotedByte(pOut,c);` |
|       36 | 2479 | `			i++;` |
|       36 | 2480 | `			continue;` |
|        - | 2481 | `		}` |
|      ! 0 | 2482 | `		d = z[i+1];` |
|      ! 0 | 2483 | `		i += 2;` |
|      ! 0 | 2484 | `		switch(d){` |
|      ! 0 | 2485 | `		case 'n': AssertRenderQuotedByte(pOut,'\n'); break;` |
|      ! 0 | 2486 | `		case 't': AssertRenderQuotedByte(pOut,'\t'); break;` |
|      ! 0 | 2487 | `		case 'r': AssertRenderQuotedByte(pOut,'\r'); break;` |
|      ! 0 | 2488 | `		case 'v': AssertRenderQuotedByte(pOut,'\v'); break;` |
|      ! 0 | 2489 | `		case 'f': AssertRenderQuotedByte(pOut,'\f'); break;` |
|      ! 0 | 2490 | `		case 'e': AssertRenderQuotedByte(pOut,0x1b); break;` |
|      ! 0 | 2491 | `		case '\\': AssertRenderQuotedByte(pOut,'\\'); break;` |
|      ! 0 | 2492 | `		case '"': AssertRenderQuotedByte(pOut,'"'); break;` |
|      ! 0 | 2493 | `		case '$': AssertRenderQuotedByte(pOut,'$'); break;` |
|      ! 0 | 2494 | `		case 'x': case 'X': {` |
|        - | 2495 | `			/* Up to two hex digits; a bare \x is literal. */` |
|      ! 0 | 2496 | `			int nHex = 0, v = 0;` |
|      ! 0 | 2497 | `			while( nHex < 2 && i < n && (unsigned char)z[i] < 0x80 && SyisHex((unsigned char)z[i]) ){` |
|      ! 0 | 2498 | `				v = (v << 4) \| SyHexToint((unsigned char)z[i]);` |
|      ! 0 | 2499 | `				i++; nHex++;` |
|      ! 0 | 2500 | `			}` |
|      ! 0 | 2501 | `			if( nHex > 0 ){` |
|      ! 0 | 2502 | `				AssertRenderQuotedByte(pOut,v);` |
|      ! 0 | 2503 | `			}else{` |
|      ! 0 | 2504 | `				AssertRenderQuotedByte(pOut,'\\');` |
|      ! 0 | 2505 | `				AssertRenderQuotedByte(pOut,d);` |
|        - | 2506 | `			}` |
|      ! 0 | 2507 | `			break;` |
|        - | 2508 | `		}` |
|      ! 0 | 2509 | `		case 'u': {` |
|        - | 2510 | `			/* \u{HEX+} — anything else keeps the backslash (php). */` |
|      ! 0 | 2511 | `			if( i < n && z[i] == '{' ){` |
|      ! 0 | 2512 | `				sxu32 v = 0; sxu32 j = i + 1; int nHex = 0;` |
|      ! 0 | 2513 | `				while( j < n && (unsigned char)z[j] < 0x80 && SyisHex((unsigned char)z[j]) && nHex < 8 ){` |
|      ! 0 | 2514 | `					v = (v << 4) \| (sxu32)SyHexToint((unsigned char)z[j]);` |
|      ! 0 | 2515 | `					j++; nHex++;` |
|      ! 0 | 2516 | `				}` |
|      ! 0 | 2517 | `				if( nHex > 0 && j < n && z[j] == '}' ){` |
|      ! 0 | 2518 | `					AssertRenderUtf8(pOut,v);` |
|      ! 0 | 2519 | `					i = j + 1;` |
|      ! 0 | 2520 | `					break;` |
|        - | 2521 | `				}` |
|      ! 0 | 2522 | `			}` |
|      ! 0 | 2523 | `			AssertRenderQuotedByte(pOut,'\\');` |
|      ! 0 | 2524 | `			AssertRenderQuotedByte(pOut,d);` |
|      ! 0 | 2525 | `			break;` |
|        - | 2526 | `		}` |
|      ! 0 | 2527 | `		default:` |
|      ! 0 | 2528 | `			if( d >= '0' && d <= '7' ){` |
|        - | 2529 | `				/* Up to three octal digits (the first was d). */` |
|      ! 0 | 2530 | `				int nOct = 1, v = d - '0';` |
|      ! 0 | 2531 | `				while( nOct < 3 && i < n && z[i] >= '0' && z[i] <= '7' ){` |
|      ! 0 | 2532 | `					v = (v << 3) \| (z[i] - '0');` |
|      ! 0 | 2533 | `					i++; nOct++;` |
|      ! 0 | 2534 | `				}` |
|      ! 0 | 2535 | `				AssertRenderQuotedByte(pOut,v & 0xff);` |
|      ! 0 | 2536 | `			}else{` |
|      ! 0 | 2537 | `				AssertRenderQuotedByte(pOut,'\\');` |
|      ! 0 | 2538 | `				AssertRenderQuotedByte(pOut,d);` |
|        - | 2539 | `			}` |
|      ! 0 | 2540 | `			break;` |
|        - | 2541 | `		}` |
|      ! 0 | 2542 | `	}` |
|       15 | 2543 | `	SyBlobAppend(pOut,"'",1);` |
|       15 | 2544 | `}` |
|        - | 2545 | `/*` |
|        - | 2546 | ` * Append a double in php's AST-export shape: the shortest round-tripping` |
|        - | 2547 | ` * decimal, with a forced ".0" fraction when the digits alone look integral` |
|        - | 2548 | ` * (1e3 -> "1000.0", 1e20 -> "1.0E+20") — the var_export float shape.` |
|        - | 2549 | ` */` |
|        8 | 2550 | `static void AssertRenderReal(SyBlob *pOut,ph7_real rVal)` |
|        1 | 2551 | `{` |
|        - | 2552 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - | 2553 | `	/* No floating point: ph7_real IS sxi64, there is no shortest-round-trip` |
|        - | 2554 | `	 * decimal to search for and no ".0" to force, so the value renders as the` |
|        - | 2555 | `	 * integer it is -- the same shape the INTEGER arm below emits. Taking` |
|        - | 2556 | `	 * ph7_real rather than double is what keeps the two call sites from` |
|        - | 2557 | `	 * narrowing (MSVC /W4 makes that C4244, and /WX makes it an error). */` |
|        - | 2558 | `	SyBlobFormat(pOut,"%qd",(sxi64)rVal);` |
|        - | 2559 | `#else` |
|        9 | 2560 | `	sxu32 nBefore = SyBlobLength(pOut);` |
|        - | 2561 | `	const char *zOut;` |
|        - | 2562 | `	sxu32 i, nAfter;` |
|        9 | 2563 | `	int bPlain = 1;` |
|        9 | 2564 | `	PH7_AppendShortestReal(pOut,rVal);` |
|        9 | 2565 | `	zOut = (const char *)SyBlobData(pOut);` |
|        9 | 2566 | `	nAfter = SyBlobLength(pOut);` |
|       23 | 2567 | `	for( i = nBefore; i < nAfter; i++ ){` |
|       21 | 2568 | `		if( !((zOut[i] >= '0' && zOut[i] <= '9') \|\| zOut[i] == '-') ){` |
|        7 | 2569 | `			bPlain = 0;` |
|        7 | 2570 | `			break;` |
|        - | 2571 | `		}` |
|        8 | 2572 | `	}` |
|        9 | 2573 | `	if( bPlain ){` |
|        3 | 2574 | `		SyBlobAppend(pOut,".0",2);` |
|        1 | 2575 | `	}` |
|        - | 2576 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|        9 | 2577 | `}` |
|        - | 2578 | `/*` |
|        - | 2579 | ` * Render the token span [pIn, pEnd) — a direct assert() call's first argument —` |
|        - | 2580 | ` * into pOut in php's zend_ast_export shape (see the block comment above).` |
|        - | 2581 | ` * Total: every span renders to SOMETHING (unknown constructs fall back to` |
|        - | 2582 | ` * their raw token text), so the capture never aborts a compile.` |
|        - | 2583 | ` */` |
|       62 | 2584 | `PH7_PRIVATE void PH7_GenRenderAssertSpan(ph7_gen_state *pGen,SyToken *pIn,SyToken *pEnd,SyBlob *pOut)` |
|        5 | 2585 | `{` |
|        - | 2586 | `	sxu8 aParen[64]; /* 1 = this '(' depth is an array(...) literal rendered as [...] */` |
|       67 | 2587 | `	sxu32 nParen = 0;` |
|       67 | 2588 | `	int iPrev = ASRT_START;` |
|       67 | 2589 | ``	int bArrayOpen = 0; /* the next '(' belongs to a suppressed `array` keyword */`` |
|        - | 2590 | `	/* php drops every redundant paren when re-deriving source from the AST;` |
|        - | 2591 | `	 * dropping the OUTERMOST pair(s) is the token-level equivalent for the` |
|        - | 2592 | ``	 * common `assert((...))` spelling. */`` |
|       69 | 2593 | `	while( pIn < pEnd - 1 && (pIn->nType & PH7_TK_LPAREN) && (pEnd[-1].nType & PH7_TK_RPAREN) ){` |
|        - | 2594 | `		SyToken *p;` |
|        3 | 2595 | `		sxi32 iDepth = 0;` |
|        3 | 2596 | `		SyToken *pMatch = 0;` |
|       11 | 2597 | `		for( p = pIn; p < pEnd; p++ ){` |
|       11 | 2598 | `			if( p->nType & PH7_TK_LPAREN ){` |
|        3 | 2599 | `				iDepth++;` |
|       10 | 2600 | `			}else if( p->nType & PH7_TK_RPAREN ){` |
|        3 | 2601 | `				iDepth--;` |
|        3 | 2602 | `				if( iDepth == 0 ){ pMatch = p; break; }` |
|      ! 0 | 2603 | `			}` |
|        5 | 2604 | `		}` |
|        3 | 2605 | `		if( pMatch != &pEnd[-1] ){` |
|      ! 0 | 2606 | `			break;` |
|        - | 2607 | `		}` |
|        3 | 2608 | `		pIn++;` |
|        3 | 2609 | `		pEnd--;` |
|        1 | 2610 | `	}` |
|      267 | 2611 | `	for( ; pIn < pEnd ; pIn++ ){` |
|      205 | 2612 | `		SyToken *pTok = pIn;` |
|      205 | 2613 | `		const char *zTxt = pTok->sData.zString;` |
|      205 | 2614 | `		sxu32 nTxt = pTok->sData.nByte;` |
|        - | 2615 | `		int iCls;` |
|        - | 2616 | `		sxu32 nMark;` |
|        - | 2617 | `		/* --- classify + pre-token handling ------------------------------ */` |
|      205 | 2618 | `		if( pTok->nType & PH7_TK_LPAREN ){` |
|       15 | 2619 | `			iCls = ASRT_OPEN;` |
|      199 | 2620 | `		}else if( pTok->nType & PH7_TK_RPAREN ){` |
|       15 | 2621 | `			iCls = ASRT_CLOSE;` |
|      187 | 2622 | `		}else if( pTok->nType & (PH7_TK_OSB\|PH7_TK_CSB) ){` |
|       13 | 2623 | `			iCls = (pTok->nType & PH7_TK_OSB) ? ASRT_OPEN : ASRT_CLOSE;` |
|      175 | 2624 | `		}else if( pTok->nType & PH7_TK_COMMA ){` |
|        - | 2625 | `			/* php's export never prints a trailing comma. */` |
|        7 | 2626 | `			if( &pIn[1] < pEnd && (pIn[1].nType & (PH7_TK_RPAREN\|PH7_TK_CSB)) ){` |
|      ! 0 | 2627 | `				continue;` |
|        - | 2628 | `			}` |
|        7 | 2629 | `			iCls = ASRT_COMMA;` |
|      166 | 2630 | `		}else if( pTok->nType & PH7_TK_DOLLAR ){` |
|        3 | 2631 | `			iCls = ASRT_UNARY; /* operand-like before, glued to its name after */` |
|      162 | 2632 | `		}else if( pTok->nType & PH7_TK_NSSEP ){` |
|      ! 0 | 2633 | `			iCls = ASRT_GLUE;` |
|      161 | 2634 | `		}else if( pTok->nType & PH7_TK_ELLIPSIS ){` |
|      ! 0 | 2635 | `			iCls = ASRT_UNARY;` |
|      161 | 2636 | `		}else if( pTok->nType & PH7_TK_OP ){` |
|       46 | 2637 | `			iCls = ASRT_BINOP;` |
|       46 | 2638 | `			if( nTxt > 0 ){` |
|       46 | 2639 | `				int c0 = zTxt[0];` |
|       46 | 2640 | `				if( c0 == '(' ){` |
|      ! 0 | 2641 | ``					iCls = ASRT_UNARY; /* lexer-merged cast token `(int)` */`` |
|       60 | 2642 | `				}else if( nTxt == 2 && (SyMemcmp(zTxt,"->",2) == 0 \|\| SyMemcmp(zTxt,"::",2) == 0` |
|       28 | 2643 | `						\|\| SyMemcmp(zTxt,"++",2) == 0 \|\| SyMemcmp(zTxt,"--",2) == 0) ){` |
|      ! 0 | 2644 | `					iCls = ASRT_GLUE;` |
|       46 | 2645 | `				}else if( nTxt == 3 && SyMemcmp(zTxt,"?->",3) == 0 ){` |
|      ! 0 | 2646 | `					iCls = ASRT_GLUE;` |
|       46 | 2647 | `				}else if( nTxt == 1 && (c0 == '!' \|\| c0 == '~' \|\| c0 == '@') ){` |
|        3 | 2648 | `					iCls = ASRT_UNARY;` |
|       45 | 2649 | `				}else if( nTxt == 1 && (c0 == '-' \|\| c0 == '+' \|\| c0 == '&') ){` |
|        - | 2650 | `					/* Unary when nothing operand-like precedes. */` |
|        4 | 2651 | `					if( iPrev == ASRT_START \|\| iPrev == ASRT_BINOP \|\| iPrev == ASRT_UNARY` |
|        3 | 2652 | `					 \|\| iPrev == ASRT_OPEN \|\| iPrev == ASRT_COMMA ){` |
|        3 | 2653 | `						iCls = ASRT_UNARY;` |
|        2 | 2654 | `					}` |
|       42 | 2655 | `				}else if( pTok->nType & PH7_TK_ID ){` |
|        - | 2656 | `					/* Alpha operators: and/or normalize to php's export spelling;` |
|        - | 2657 | `					 * new/clone read as prefix keywords (operand spacing). */` |
|        3 | 2658 | `					if( nTxt == 3 && SyStrnicmp(zTxt,"and",3) == 0 ){` |
|        3 | 2659 | `						zTxt = "&&"; nTxt = 2;` |
|        1 | 2660 | `					}else if( nTxt == 2 && SyStrnicmp(zTxt,"or",2) == 0 ){` |
|      ! 0 | 2661 | `						zTxt = "\|\|"; nTxt = 2;` |
|      ! 0 | 2662 | `					}else if( (nTxt == 3 && SyStrnicmp(zTxt,"new",3) == 0)` |
|      ! 0 | 2663 | `						\|\| (nTxt == 5 && SyStrnicmp(zTxt,"clone",5) == 0) ){` |
|      ! 0 | 2664 | `						iCls = ASRT_OPERAND;` |
|      ! 0 | 2665 | `					}` |
|        1 | 2666 | `				}` |
|       24 | 2667 | `			}` |
|      139 | 2668 | `		}else if( pTok->nType & (PH7_TK_EQUAL\|PH7_TK_ARRAY_OP\|PH7_TK_COLON\|PH7_TK_AMPER) ){` |
|      ! 0 | 2669 | `			iCls = ASRT_BINOP;` |
|      117 | 2670 | `		}else if( pTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|       35 | 2671 | `			iCls = ASRT_OPERAND;` |
|        - | 2672 | ``			/* `array` `(` — php's AST holds one list node for both spellings and`` |
|        - | 2673 | ``			 * always exports `[...]`. Suppress the keyword (it lexes as a KEYWORD`` |
|        - | 2674 | `			 * token, not an ID); the '(' renders '['. */` |
|       30 | 2675 | `			if( nTxt == 5 && SyStrnicmp(zTxt,"array",5) == 0` |
|       17 | 2676 | `			 && &pIn[1] < pEnd && (pIn[1].nType & PH7_TK_LPAREN) ){` |
|        9 | 2677 | `				bArrayOpen = 1;` |
|        9 | 2678 | `				continue;` |
|        - | 2679 | `			}` |
|       16 | 2680 | `		}else{` |
|        - | 2681 | `			/* keywords (true/false/null/fn/match/...), numbers, strings,` |
|        - | 2682 | `			 * member names, '{'/'}' and anything unforeseen */` |
|       86 | 2683 | `			iCls = ASRT_OPERAND;` |
|        - | 2684 | `		}` |
|        - | 2685 | ``		/* Elvis `? :` — php exports the two-token form as `?:`. */`` |
|      194 | 2686 | `		if( iCls == ASRT_BINOP && nTxt == 1 && zTxt[0] == '?'` |
|       11 | 2687 | `		 && &pIn[1] < pEnd && (pIn[1].nType & PH7_TK_COLON) ){` |
|        3 | 2688 | `			nMark = SyBlobLength(pOut);` |
|        3 | 2689 | `			if( iPrev != ASRT_START && nMark > 0 ){` |
|        3 | 2690 | `				SyBlobAppend(pOut," ",1);` |
|        1 | 2691 | `			}` |
|        3 | 2692 | `			SyBlobAppend(pOut,"?:",2);` |
|        3 | 2693 | `			pIn++; /* consume the ':' */` |
|        3 | 2694 | `			iPrev = ASRT_BINOP;` |
|        3 | 2695 | `			continue;` |
|        - | 2696 | `		}` |
|        - | 2697 | `		/* --- spacing ---------------------------------------------------- */` |
|      197 | 2698 | `		if( iPrev != ASRT_START ){` |
|      134 | 2699 | `			int iAfter = aAsrtAfter[iPrev];` |
|      134 | 2700 | `			int iBefore = aAsrtBefore[iCls];` |
|      130 | 2701 | `			if( iAfter == ASRT_SP_FORCE \|\| iBefore == ASRT_SP_FORCE` |
|       69 | 2702 | `			 \|\| (iAfter == ASRT_SP_WANT && iBefore == ASRT_SP_WANT) ){` |
|       86 | 2703 | `				SyBlobAppend(pOut," ",1);` |
|       42 | 2704 | `			}` |
|       65 | 2705 | `		}` |
|        - | 2706 | `		/* --- emit ------------------------------------------------------- */` |
|      197 | 2707 | `		if( pTok->nType & PH7_TK_LPAREN ){` |
|       15 | 2708 | `			if( nParen < sizeof(aParen) ){` |
|       15 | 2709 | `				aParen[nParen] = (sxu8)bArrayOpen;` |
|        6 | 2710 | `			}` |
|       15 | 2711 | `			nParen++;` |
|       15 | 2712 | `			SyBlobAppend(pOut,bArrayOpen ? "[" : "(",1);` |
|       15 | 2713 | `			bArrayOpen = 0;` |
|      191 | 2714 | `		}else if( pTok->nType & PH7_TK_RPAREN ){` |
|       15 | 2715 | `			int bArr = 0;` |
|       15 | 2716 | `			if( nParen > 0 ){` |
|       15 | 2717 | `				nParen--;` |
|       15 | 2718 | `				if( nParen < sizeof(aParen) ){` |
|       15 | 2719 | `					bArr = aParen[nParen];` |
|        6 | 2720 | `				}` |
|        6 | 2721 | `			}` |
|       15 | 2722 | `			SyBlobAppend(pOut,bArr ? "]" : ")",1);` |
|      178 | 2723 | `		}else if( pTok->nType & (PH7_TK_INTEGER\|PH7_TK_REAL) ){` |
|        - | 2724 | `			char zScratch[GEN_NUM_SCRATCH];` |
|       71 | 2725 | `			char *zAlloc = 0;` |
|        - | 2726 | `			SyString sNum;` |
|      102 | 2727 | `			if( GenStateStripNumericSeparators(&pGen->pVm->sAllocator,&pTok->sData,` |
|       71 | 2728 | `					zScratch,sizeof(zScratch),&sNum,&zAlloc) != SXRET_OK ){` |
|      ! 0 | 2729 | `				SyBlobAppend(pOut,zTxt,nTxt); /* alloc failure: raw text */` |
|       71 | 2730 | `			}else if( pTok->nType & PH7_TK_INTEGER ){` |
|       63 | 2731 | `				ph7_real rOverflow = 0;` |
|       63 | 2732 | `				int bDecimalOverflow = 0;` |
|       63 | 2733 | `				if( GenStateIntLiteralOverflows(&sNum,&rOverflow,&bDecimalOverflow) ){` |
|      ! 0 | 2734 | `					if( bDecimalOverflow ){` |
|      ! 0 | 2735 | `						SyStrToReal(sNum.zString,sNum.nByte,(void *)&rOverflow,0);` |
|      ! 0 | 2736 | `					}` |
|      ! 0 | 2737 | `					AssertRenderReal(pOut,rOverflow);` |
|      ! 0 | 2738 | `				}else{` |
|       63 | 2739 | `					SyBlobFormat(pOut,"%qd",PH7_TokenValueToInt64(&sNum));` |
|        - | 2740 | `				}` |
|       33 | 2741 | `			}else{` |
|        9 | 2742 | `				ph7_real rVal = 0;` |
|        9 | 2743 | `				SyStrToReal(sNum.zString,sNum.nByte,(void *)&rVal,0);` |
|        9 | 2744 | `				AssertRenderReal(pOut,rVal);` |
|        - | 2745 | `			}` |
|       71 | 2746 | `			if( zAlloc ){` |
|      ! 0 | 2747 | `				SyMemBackendFree(&pGen->pVm->sAllocator,zAlloc);` |
|        3 | 2748 | `			}` |
|      138 | 2749 | `		}else if( pTok->nType & (PH7_TK_SSTR\|PH7_TK_NOWDOC) ){` |
|        3 | 2750 | `			AssertRenderSglString(pOut,zTxt,nTxt);` |
|      103 | 2751 | `		}else if( pTok->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
|       15 | 2752 | `			if( SyByteFind(zTxt,nTxt,'$',0) == SXRET_OK ){` |
|        - | 2753 | `				/* Interpolated: php exports its interpolation AST in a` |
|        - | 2754 | `				 * double-quoted form; the raw source is the token-level` |
|        - | 2755 | `				 * equivalent. */` |
|      ! 0 | 2756 | `				SyBlobAppend(pOut,"\"",1);` |
|      ! 0 | 2757 | `				SyBlobAppend(pOut,zTxt,nTxt);` |
|      ! 0 | 2758 | `				SyBlobAppend(pOut,"\"",1);` |
|      ! 0 | 2759 | `			}else{` |
|       15 | 2760 | `				AssertRenderDblString(pOut,zTxt,nTxt);` |
|        - | 2761 | `			}` |
|        9 | 2762 | `		}else{` |
|       90 | 2763 | `			SyBlobAppend(pOut,zTxt,nTxt);` |
|        - | 2764 | `		}` |
|      197 | 2765 | `		iPrev = iCls;` |
|      101 | 2766 | `	}` |
|       67 | 2767 | `}` |
|        - | 2768 |  |
