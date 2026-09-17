/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
#include "compile_int.h"
/*
 * Section:
 *    Literal compilation: numeric literals (incl. PHP 7.4 numeric
 *    separators), simple/double-quoted strings, heredoc/nowdoc, array()
 *    and [] literals, list() destructuring and clone-call rewriting.
 * Status:
 *    Stable.
 */
/*
 * Return TRUE if c is a valid digit for the given numeric base.
 *   base 16 => SyisHex (0-9, a-f, A-F)
 *   base  2 => 0 or 1
 *   base 10 => SyisDigit (0-9, also used for octal literals which share the
 *              decimal scan in the lexer)
 */
static int GenStateIsBaseDigit(int c, int base)
{
	if( base == 16 ){ return SyisHex(c); }
	if( base == 2 ){ return c == '0' || c == '1'; }
	return SyisDigit(c);
}
/*
 * Given the raw text of a numeric literal token, locate a misplaced PHP 7.4
 * underscore separator so the caller can report the malformed portion with
 * the exact wording PHP uses:
 *
 *   syntax error, unexpected identifier "X"
 *
 * The lexer guarantees that every underscore it consumed as a separator is
 * surrounded by valid base digits; anything else sits in the trailing run
 * absorbed by the lexer specifically to let this validator see and report
 * it. That invariant means the malformed span is exactly [bad .. nByte) —
 * no forward rescan needed.
 *
 * Returns 1 and fills pBadStart / pBadLen when the literal is malformed;
 * returns 0 when it is well-formed.
 */
static int GenStateFindBadNumericSeparator(
	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)
{
	const char *z = pRaw->zString;
	sxu32 n = pRaw->nByte;
	int base = 10;
	sxu32 i, start;
	if( n < 2 ) return 0;
	if( z[0] == '0' && (z[1] == 'x' || z[1] == 'X') ){
		base = 16;
	}else if( z[0] == '0' && (z[1] == 'b' || z[1] == 'B') ){
		base = 2;
	}
	for( i = 0; i < n; ++i ){
		if( z[i] != '_' ) continue;
		if( i > 0 && i + 1 < n
			&& GenStateIsBaseDigit((unsigned char)z[i-1], base)
			&& GenStateIsBaseDigit((unsigned char)z[i+1], base) ){
			continue; /* well-placed separator */
		}
		/* First misplaced underscore — the lexer already absorbed the full
		 * malformed tail, so it runs from here to the end of the token. */
		start = i;
		if( start > 0 && (z[start-1] == 'x' || z[start-1] == 'X'
			|| z[start-1] == 'b' || z[start-1] == 'B') ){
			start--; /* include the base letter for 0x_... / 0b_... */
		}
		*pBadStart = &z[start];
		*pBadLen = n - start;
		return 1;
	}
	return 0;
}
/*
 * Emit the shared "syntax error, unexpected identifier" parse error when a
 * numeric-literal token contains a misplaced PHP 7.4 separator. Returns
 * SXRET_OK when the token is well-formed; on error propagates whatever
 * PH7_GenCompileError returned (SXERR_ABORT when the error count is
 * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned
 * so callers can bail from the current construct).
 */
PH7_PRIVATE sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)
{
	const char *zBad = 0;
	sxu32 nBad = 0;
	SyString sBad;
	sxi32 rc;
	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){
		return SXRET_OK;
	}
	SyStringInitFromBuf(&sBad, zBad, nBad);
	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,
		"syntax error, unexpected identifier \"%z\"", &sBad);
	if( rc == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	return SXERR_SYNTAX;
}
/*
 * Strip PHP 7.4 numeric literal separators (underscores between digits) from
 * a numeric token's text and yield a SyString suitable for the low-level
 * converters (SyStrToInt64 / SyStrToReal / etc.).
 *
 * Fast path: if the token contains no '_', *pOut aliases pToken with no copy
 * and *pzAlloc is set to NULL.
 * Stack path: if the cleaned bytes fit in zScratch, they are written there
 * and *pzAlloc is set to NULL.
 * Heap path: for literals larger than the scratch buffer, a fresh buffer is
 * allocated from pAlloc, returned via *pzAlloc, and must be released by the
 * caller with SyMemBackendFree once the converter is done.
 *
 * Returns SXRET_OK on success, SXERR_ABORT on allocator failure (in which
 * case *pOut is left untouched and the caller must not read it).
 */
PH7_PRIVATE sxi32 GenStateStripNumericSeparators(
	SyMemBackend *pAlloc,
	const SyString *pToken,
	char *zScratch, sxu32 nScratch,
	SyString *pOut, char **pzAlloc)
{
	sxu32 i, j;
	int hasUnderscore = 0;
	char *zBuf;
	*pzAlloc = 0;
	for( i = 0; i < pToken->nByte; ++i ){
		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }
	}
	if( !hasUnderscore ){
		SyStringDupPtr(pOut, pToken);
		return SXRET_OK;
	}
	if( pToken->nByte <= nScratch ){
		zBuf = zScratch;
	}else{
		zBuf = (char *)SyMemBackendAlloc(pAlloc, pToken->nByte);
		if( zBuf == 0 ){
			return SXERR_ABORT;
		}
		*pzAlloc = zBuf;
	}
	j = 0;
	for( i = 0; i < pToken->nByte; ++i ){
		if( pToken->zString[i] != '_' ){ zBuf[j++] = pToken->zString[i]; }
	}
	SyStringInitFromBuf(pOut, zBuf, j);
	return SXRET_OK;
}
/*
 * Compile a numeric [i.e: integer or real] literal.
 * Notes on the integer type.
 *  According to the PHP language reference manual
 *  Integers can be specified in decimal (base 10), hexadecimal (base 16), octal (base 8)
 *  or binary (base 2) notation, optionally preceded by a sign (- or +).
 *  To use octal notation, precede the number with a 0 (zero). To use hexadecimal
 *  notation precede the number with 0x. To use binary notation precede the number with 0b.
 * Symisc eXtension to the integer type.
 *  PH7 introduced platform-independant 64-bit integer unlike the standard PHP engine
 *  where the size of an integer is platform-dependent.That is,the size of an integer
 *  is 8 bytes and the maximum integer size is 0x7FFFFFFFFFFFFFFF for all platforms
 *  [i.e: either 32bit or 64bit].
 *  For more information on this powerfull extension please refer to the official
 *  documentation.
 */
/*
 * Determine whether an integer literal token exceeds the signed 64-bit range.
 * PHP promotes such a literal to a float (e.g. 9223372036854775808 ->
 * float(9.22...E+18), 0xFFFFFFFFFFFFFFFF -> float) rather than wrapping or
 * dropping digits. pNum is the separator-stripped token (unsigned; the sign of
 * a "-1" is a separate unary operator). Base detection mirrors
 * PH7_TokenValueToInt64. Returns TRUE on overflow: for a non-decimal base the
 * float value is accumulated into *pReal (dv = dv*base + digit); for decimal
 * *pbDecimal is set so the caller reuses strtod on the token for a
 * correctly-rounded value. Returns FALSE (value fits) for anything it cannot
 * confidently classify, so the int path stays in charge.
 *
 * The int/float CLASSIFICATION is php-exact for every base. VALUES are byte-exact
 * for decimal (strtod) and hex (php's zend_hex_strtod uses the same dv*16+digit
 * doubling). Octal/binary overflow values can differ from php by the low bit(s):
 * php's zend_{oct,bin}_strtod rounds differently than this doubling — e.g. php's
 * binary 2**63 is 2**63-1024 whereas this returns the exact 2**63. Recorded as a
 * residual; matching php exactly would need a port of those functions.
 */
static int GenStateIntLiteralOverflows(const SyString *pNum, ph7_real *pReal, int *pbDecimal)
{
	const char *z = pNum->zString;
	const char *zEnd = z + pNum->nByte;
	const char *p, *q;
	int n;
	*pbDecimal = FALSE;
	if( z >= zEnd ){
		return FALSE;
	}
	if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'x' || z[1] == 'X') ){
		/* Hexadecimal: INT64_MAX == 0x7FFF...F (16 digits, leading nibble 7). */
		p = z + 2;
		while( p < zEnd && p[0] == '0' ){ p++; }
		for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisHex(q[0]); q++ ){ n++; }
		if( n < 16 || (n == 16 && SyHexToint(p[0]) < 8) ){
			return FALSE;
		}
		{ ph7_real dv = 0;
		  for( q = p; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisHex(q[0]); q++ ){
			dv = dv * 16 + (ph7_real)SyHexToint(q[0]);
		  }
		  *pReal = dv;
		}
		return TRUE;
	}else if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'b' || z[1] == 'B') ){
		/* Binary: INT64_MAX needs 63 significant bits. */
		p = z + 2;
		while( p < zEnd && p[0] == '0' ){ p++; }
		for( q = p, n = 0; q < zEnd && (q[0] == '0' || q[0] == '1'); q++ ){ n++; }
		if( n <= 63 ){
			return FALSE;
		}
		{ ph7_real dv = 0;
		  for( q = p; q < zEnd && (q[0] == '0' || q[0] == '1'); q++ ){
			dv = dv * 2 + (ph7_real)(q[0] - '0');
		  }
		  *pReal = dv;
		}
		return TRUE;
	}else if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'o' || z[1] == 'O') ){
		/* PHP 8.1 explicit octal 0o/0O: 21 significant octal digits fit in int64. */
		p = z + 2;
		while( p < zEnd && p[0] == '0' ){ p++; }
		for( q = p, n = 0; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){ n++; }
		if( n <= 21 ){
			return FALSE;
		}
		{ ph7_real dv = 0;
		  for( q = p; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){
			dv = dv * 8 + (ph7_real)(q[0] - '0');
		  }
		  *pReal = dv;
		}
		return TRUE;
	}else if( z[0] == '0' ){
		/* Octal: INT64_MAX == 0o777...7 (21 significant octal digits). Skip the
		 * leading zeros (incl. the base '0'); a non-octal char such as the 8.1
		 * "0o" marker ends the run and leaves it to the int path (as today). */
		p = z;
		while( p < zEnd && p[0] == '0' ){ p++; }
		for( q = p, n = 0; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){ n++; }
		if( n <= 21 ){
			return FALSE;
		}
		{ ph7_real dv = 0;
		  for( q = p; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){
			dv = dv * 8 + (ph7_real)(q[0] - '0');
		  }
		  *pReal = dv;
		}
		return TRUE;
	}
	/* Decimal: overflow iff more than 19 significant digits, or exactly 19 that
	 * compare greater than INT64_MAX. Defer the value to strtod (via the caller)
	 * for php-exact rounding. */
	p = z;
	while( p < zEnd && p[0] == '0' ){ p++; }
	for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisDigit(q[0]); q++ ){ n++; }
	if( n > 19 || (n == 19 && SyMemcmp(p, "9223372036854775807", 19) > 0) ){
		*pbDecimal = TRUE;
		return TRUE;
	}
	return FALSE;
}
PH7_PRIVATE sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)
{
	SyToken *pToken = pGen->pIn; /* Raw token */
	sxu32 nIdx = 0;
	char zScratch[GEN_NUM_SCRATCH];
	char *zAlloc = 0;
	SyString sNum;
	sxi32 rc;
	SXUNUSED(iCompileFlag); /* cc warning */
	rc = GenStateValidateNumericSeparator(pGen, pToken);
	if( rc != SXRET_OK ){
		return rc;
	}
	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,
		zScratch, sizeof(zScratch), &sNum, &zAlloc);
	if( rc != SXRET_OK ){
		return SXERR_ABORT;
	}
	if( pToken->nType & PH7_TK_INTEGER ){
		ph7_value *pObj;
		sxi64 iValue;
		ph7_real rOverflow = 0;
		int bDecimalOverflow = 0;
		if( GenStateIntLiteralOverflows(&sNum,&rOverflow,&bDecimalOverflow) ){
			/* Literal exceeds the signed 64-bit range: PHP represents it as a
			 * float instead of wrapping/dropping digits. */
			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);
			if( pObj == 0 ){
				PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");
				if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }
				return SXERR_ABORT;
			}
			if( bDecimalOverflow ){
				/* strtod on the decimal token yields php-exact rounding. */
				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);
				PH7_MemObjToReal(pObj);
			}else{
				PH7_MemObjInitFromReal(pGen->pVm,pObj,rOverflow);
			}
		}else{
			iValue = PH7_TokenValueToInt64(&sNum);
			pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);
			if( pObj == 0 ){
				if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }
				return SXERR_ABORT;
			}
			PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);
		}
	}else{
		/* Real number */
		ph7_value *pObj;
		/* Reserve a new constant */
		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);
		if( pObj == 0 ){
			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");
			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }
			return SXERR_ABORT;
		}
		PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);
		PH7_MemObjToReal(pObj);
	}
	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }
	/* Emit the load constant instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);
	/* Node successfully compiled */
	return SXRET_OK;
}
/*
 * Compile a single quoted string.
 * According to the PHP language reference manual:
 *
 *   The simplest way to specify a string is to enclose it in single quotes (the character ' ).
 *   To specify a literal single quote, escape it with a backslash (\). To specify a literal
 *   backslash, double it (\\). All other instances of backslash will be treated as a literal
 *   backslash: this means that the other escape sequences you might be used to, such as \r
 *   or \n, will be output literally as specified rather than having any special meaning.
 *
 */
PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)
{
	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */
	const char *zIn,*zCur,*zEnd;
	ph7_value *pObj;
	sxu32 nIdx;
	sxi32 bHasEsc;
	nIdx = 0; /* Prevent compiler warning */
	/* Delimit the string */
	zIn  = pStr->zString;
	zEnd = &zIn[pStr->nByte];
	if( zIn >= zEnd ){
		/* Empty string constant: just use the pre‑allocated index from the VM
		 * rather than reserving a new object each time. */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);
		return SXRET_OK;
	}
	/* A single-quoted literal whose raw source holds a backslash unescapes to a
	 * value that differs from that source (\\ -> \, \' -> '). The literal cache
	 * keys FIND on the raw source text but INSTALL on the unescaped value, so
	 * caching such a literal lets an unrelated one collide with it — e.g. '\\'
	 * (raw \\, value \) would find the entry installed for '\\\\' (raw \\\\,
	 * value \\) and load two backslashes. Only cache literals whose value equals
	 * their source, i.e. those with no backslash to unescape. */
	bHasEsc = 0;
	{
		const char *zScan;
		for( zScan = zIn ; zScan < zEnd ; zScan++ ){
			if( zScan[0] == '\\' ){ bHasEsc = 1; break; }
		}
	}
	if( !bHasEsc && SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){
		/* Already processed,emit the load constant instruction
		 * and return.
		 */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);
		return SXRET_OK;
	}
	/* Reserve a new constant */
	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);
	if( pObj == 0 ){
		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");
		SXUNUSED(iCompileFlag); /* cc warning */
		return SXERR_ABORT;
	}
	PH7_MemObjInitFromString(pGen->pVm,pObj,0);
	/* Compile the node */
	for(;;){
		if( zIn >= zEnd ){
			/* End of input */
			break;
		}
		zCur = zIn;
		while( zIn < zEnd && zIn[0] != '\\' ){
			zIn++;
		}
		if( zIn > zCur ){
			/* Append raw contents*/
			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));
		}
		zIn++;
		if( zIn < zEnd ){
			if( zIn[0] == '\\' ){
				/* A literal backslash */
				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));
			}else if( zIn[0] == '\'' ){
				/* A single quote */
				PH7_MemObjStringAppend(pObj,"'",sizeof(char));
			}else{
				/* verbatim copy */
				zIn--;
				PH7_MemObjStringAppend(pObj,zIn,sizeof(char)*2);
				zIn++;
			}
		}
		/* Advance the stream cursor */
		zIn++;
	}
	/* Emit the load constant instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);
	if( !bHasEsc && pStr->nByte < 1024 ){
		/* Install in the literal table (only when value == source; see above) */
		GenStateInstallLiteral(pGen,pObj,nIdx);
	}
	/* Node successfully compiled */
	return SXRET_OK;
}
/*
 * PHP 7.3 flexible heredoc/nowdoc closing-marker indent stripping.
 *
 * When the lexer matched the closing marker with leading whitespace on its
 * own line, it stored the indent count in pGen->pIn->pUserData. The marker's
 * indent prefix bytes sit immediately after the stripped body (at
 * pIn->sData.zString + pIn->sData.nByte + 1 for LF, +2 for CRLF) in the
 * original source buffer — the buffer is stable through compilation.
 *
 * For each body line, we remove exactly `nIndent` leading bytes that must
 * byte-for-byte match the marker's prefix. Empty lines (0 bytes or bare \r)
 * bypass validation. Mismatches raise the exact PHP 7.3+ parse errors:
 *   - "Invalid body indentation level (expecting an indentation level of
 *     at least N)" — line too short, or first differing byte is not
 *     whitespace.
 *   - "Invalid indentation - tabs and spaces cannot be mixed" — first
 *     differing byte is whitespace but differs from the marker prefix.
 */
static sxi32 GenStateStripHeredocIndent(ph7_gen_state *pGen, SyString *pOut)
{
	SyString *pIn = &pGen->pIn->sData;
	sxu32 nIndent = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);
	const char *zPrefix;
	const char *z, *zEnd;
	char *zBuf, *zDst;
	if( nIndent == 0 ){
		/* Legacy column-0 marker: zero-copy fast path */
		*pOut = *pIn;
		return SXRET_OK;
	}
	/* Recover the marker indent prefix from the original source buffer.
	 * Skip the terminator the lexer stripped: one '\n' plus an optional
	 * preceding '\r'. Note: when the body is empty (pIn->nByte == 0) the
	 * lexer stripped nothing, so this offset is one byte past the true
	 * marker-indent start. That is harmless — the strip loop below never
	 * runs (z == zEnd), and zPrefix is never dereferenced. */
	zPrefix = pIn->zString + pIn->nByte;
	if( zPrefix[0] == '\r' && zPrefix[1] == '\n' ){
		zPrefix += 2;
	}else{
		zPrefix += 1;
	}
	/* Allocate scratch buffer sized to the original body (always enough). */
	zBuf = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator, pIn->nByte + 1);
	if( zBuf == 0 ){
		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");
		return SXERR_ABORT;
	}
	zDst = zBuf;
	z = pIn->zString;
	zEnd = z + pIn->nByte;
	while( z < zEnd ){
		const char *zLine = z;
		sxu32 nLine;
		int bEmpty;
		while( z < zEnd && z[0] != '\n' ){
			z++;
		}
		nLine = (sxu32)(z - zLine);
		bEmpty = (nLine == 0) || (nLine == 1 && zLine[0] == '\r');
		if( !bEmpty ){
			sxu32 i;
			if( nLine < nIndent ){
				PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,
					"Invalid body indentation level (expecting an indentation level of at least %u)",
					nIndent);
				return SXERR_ABORT;
			}
			for( i = 0; i < nIndent; i++ ){
				if( zLine[i] != zPrefix[i] ){
					unsigned char c = (unsigned char)zLine[i];
					if( c == ' ' || c == '\t' ){
						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,
							"Invalid indentation - tabs and spaces cannot be mixed");
					}else{
						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,
							"Invalid body indentation level (expecting an indentation level of at least %u)",
							nIndent);
					}
					return SXERR_ABORT;
				}
			}
			SyMemcpy((const void *)(zLine + nIndent), (void *)zDst, nLine - nIndent);
			zDst += nLine - nIndent;
		}else if( nLine == 1 ){
			/* Preserve the stray '\r' on an otherwise empty line */
			*zDst++ = '\r';
		}
		if( z < zEnd ){
			*zDst++ = '\n';
			z++;
		}
	}
	pOut->zString = zBuf;
	pOut->nByte = (sxu32)(zDst - zBuf);
	return SXRET_OK;
}
/*
 * Compile a nowdoc string.
 * According to the PHP language reference manual:
 *
 *  Nowdocs are to single-quoted strings what heredocs are to double-quoted strings.
 *  A nowdoc is specified similarly to a heredoc, but no parsing is done inside a nowdoc.
 *  The construct is ideal for embedding PHP code or other large blocks of text without the
 *  need for escaping. It shares some features in common with the SGML <![CDATA[ ]]>
 *  construct, in that it declares a block of text which is not for parsing.
 *  A nowdoc is identified with the same <<< sequence used for heredocs, but the identifier
 *  which follows is enclosed in single quotes, e.g. <<<'EOT'. All the rules for heredoc
 *  identifiers also apply to nowdoc identifiers, especially those regarding the appearance
 *  of the closing identifier.
 */
PH7_PRIVATE sxi32 PH7_CompileNowDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)
{
	SyString sStripped;
	SyString *pStr;
	ph7_value *pObj;
	sxu32 nIdx;
	sxi32 rc;
	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);
	if( rc != SXRET_OK ){
		return rc;
	}
	pStr = &sStripped;
	nIdx = 0; /* Prevent compiler warning */
	if( pStr->nByte <= 0 ){
		/* An empty nowdoc is the empty STRING, like '' -- loading NULL here made
		 * strlen(<<<'EOD'EOD;) deprecation-warn about a null argument. */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);
		return SXRET_OK;
	}
	/* Reserve a new constant */
	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);
	if( pObj == 0 ){
		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");
		SXUNUSED(iCompileFlag); /* cc warning */
		return SXERR_ABORT;
	}
	/* No processing is done here, simply a memcpy() operation */
	PH7_MemObjInitFromString(pGen->pVm,pObj,pStr);
	/* Emit the load constant instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);
	/* Node successfully compiled */
	return SXRET_OK;
}
/*
 * Process variable expression [i.e: "$var","${var}"] embedded in a double quoted/heredoc string.
 * According to the PHP language reference manual
 *   When a string is specified in double quotes or with heredoc,variables are parsed within it.
 *  There are two types of syntax: a simple one and a complex one. The simple syntax is the most
 *  common and convenient. It provides a way to embed a variable, an array value, or an object
 *  property in a string with a minimum of effort.
 *  Simple syntax
 *   If a dollar sign ($) is encountered, the parser will greedily take as many tokens as possible
 *   to form a valid variable name. Enclose the variable name in curly braces to explicitly specify
 *   the end of the name.
 *   Similarly, an array index or an object property can be parsed. With array indices, the closing
 *   square bracket (]) marks the end of the index. The same rules apply to object properties
 *   as to simple variables.
 *  Complex (curly) syntax
 *   This isn't called complex because the syntax is complex, but because it allows for the use
 *   of complex expressions.
 *   Any scalar variable, array element or object property with a string representation can be
 *   included via this syntax. Simply write the expression the same way as it would appear outside
 *   the string, and then wrap it in { and }. Since { can not be escaped, this syntax will only
 *   be recognised when the $ immediately follows the {. Use {\$ to get a literal {$
 */
static sxi32 GenStateProcessStringExpression(
	ph7_gen_state *pGen, /* Code generator state */
	sxu32 nLine,         /* Line number */
	const char *zIn,     /* Raw expression */
	const char *zEnd     /* End of the expression */
	)
{
	SyToken *pTmpIn,*pTmpEnd;
	SySet sToken;
	sxi32 rc;
	/* Initialize the token set */
	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));
	/* Preallocate some slots */
	SySetAlloc(&sToken,0x08);
	/* Tokenize the text */
	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken,0);
	/* Swap delimiter */
	pTmpIn  = pGen->pIn;
	pTmpEnd = pGen->pEnd;
	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);
	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];
	/* Compile the expression. An interpolated `"...$x..."` READS $x — php warns
	 * "Undefined variable $x" and substitutes the empty string — so ask for a
	 * read-only load rather than letting the default vivify it silently. */
	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);
	/* Restore token stream */
	pGen->pIn  = pTmpIn;
	pGen->pEnd = pTmpEnd;
	/* Release the token set */
	SySetRelease(&sToken);
	/* Compilation result */
	return rc;
}
/*
 * Reserve a new constant for a double quoted/heredoc string.
 */
static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)
{
	ph7_value *pConstObj;
	sxu32 nIdx = 0;
	/* Reserve a new constant */
	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);
	if( pConstObj == 0 ){
		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");
		return 0;
	}
	(*pCount)++;
	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);
	/* Emit the load constant instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);
	return pConstObj;
}
/*
 * Compile a double quoted/heredoc string.
 * According to the PHP language reference manual
 * Heredoc
 *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier
 *  is provided, then a newline. The string itself follows, and then the same identifier again
 *  to close the quotation.
 *  The closing identifier must begin in the first column of the line. Also, the identifier must
 *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric
 *  characters and underscores, and must start with a non-digit character or underscore.
 *  Warning
 *  It is very important to note that the line with the closing identifier must contain
 *  no other characters, except possibly a semicolon (;). That means especially that the identifier
 *  may not be indented, and there may not be any spaces or tabs before or after the semicolon.
 *  It's also important to realize that the first character before the closing identifier must
 *  be a newline as defined by the local operating system. This is \n on UNIX systems, including Mac OS X.
 *  The closing delimiter (possibly followed by a semicolon) must also be followed by a newline.
 *  If this rule is broken and the closing identifier is not "clean", it will not be considered a closing
 *  identifier, and PHP will continue looking for one. If a proper closing identifier is not found before
 *  the end of the current file, a parse error will result at the last line.
 *  Heredocs can not be used for initializing class properties.
 * Double quoted
 *  If the string is enclosed in double-quotes ("), PHP will interpret more escape sequences for special characters:
 *  Escaped characters Sequence 	Meaning
 *  \n linefeed (LF or 0x0A (10) in ASCII)
 *  \r carriage return (CR or 0x0D (13) in ASCII)
 *  \t horizontal tab (HT or 0x09 (9) in ASCII)
 *  \v vertical tab (VT or 0x0B (11) in ASCII)
 *  \e escape (ESC or 0x1B (27) in ASCII)
 *  \f form feed (FF or 0x0C (12) in ASCII)
 *  \\ backslash
 *  \$ dollar sign
 *  \" double-quote
 *  \[0-7]{1,3} 	the sequence of characters matching the regular expression is a character in octal notation,
 *      which silently overflows to fit in a byte (e.g. "\400" === "\000")
 *  \x[0-9A-Fa-f]{1,2} 	the sequence of characters matching the regular expression is a character in hexadecimal notation
 *  \u{[0-9A-Fa-f]+} 	the sequence of characters matching the regular expression is a Unicode codepoint,
 *      which will be output to the string as that codepoint's UTF-8 representation
 * As in single quoted strings, escaping any other character will result in the backslash being printed too.
 * (The PH7-ism "\oNNN" octal form is gone: a literal "\o" now round-trips like php 8.)
 * The most important feature of double-quoted strings is the fact that variable names will be expanded.
 * See string parsing for details.
 */
/*
 * Line number of an escape sequence inside the string body being compiled:
 * the token's line plus every newline before the escape (php reports the
 * escape's own line, not the string's opening line). A heredoc body starts
 * on the line after the '<<<' marker, hence the +1.
 */
static sxu32 GenStateStringEscLine(ph7_gen_state *pGen,const char *zPos,int bHeredoc)
{
	const char *z = pGen->pIn->sData.zString;
	sxu32 nLine = pGen->pIn->nLine + (bHeredoc ? 1 : 0);
	for( ; z < zPos ; z++ ){
		if( z[0] == '\n' ){
			nLine++;
		}
	}
	return nLine;
}
/* bHeredoc: php strips the backslash from '\"' only when '"' is the active
 * quote character; a heredoc has none, so '\"' stays verbatim there. */
static sxi32 GenStateCompileString(ph7_gen_state *pGen,int bHeredoc)
{
	SyString *pStr = &pGen->pIn->sData; /* Raw token value */
	const char *zIn,*zCur,*zEnd;
	ph7_value *pObj = 0;
	sxi32 iCons;
	sxi32 nInterp;   /* how many of iCons came from an interpolated EXPRESSION */
	sxi32 rc;
	/* Delimit the string */
	zIn  = pStr->zString;
	zEnd = &zIn[pStr->nByte];
	if( zIn >= zEnd ){
		/* Empty string: use the shared constant reserved at VM initialization.
		 * This avoids creating a new literal for every occurrence and keeps the
		 * literal table from growing when many "" literals appear in the source.
		 */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);
		return SXRET_OK;
	}
	zCur = 0;
	/* Compile the node */
	iCons = 0;
	nInterp = 0;
	for(;;){
		zCur = zIn;
		while( zIn < zEnd && zIn[0] != '\\'  ){
			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){
				break;
			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&
				(((unsigned char)zIn[1] >= 0xc0 || SyisAlpha(zIn[1]) || zIn[1] == '{' || zIn[1] == '_')) ){
					break;
			}
			zIn++;
		}
		if( zIn > zCur ){
			if( pObj == 0 ){
				pObj = GenStateNewStrObj(&(*pGen),&iCons);
				if( pObj == 0 ){
					return SXERR_ABORT;
				}
			}
			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));
		}
		if( zIn >= zEnd ){
			break;
		}
		if( zIn[0] == '\\' ){
			const char *zPtr = 0;
			sxu32 n;
			zIn++;
			if( pObj == 0 ){
				pObj = GenStateNewStrObj(&(*pGen),&iCons);
				if( pObj == 0 ){
					return SXERR_ABORT;
				}
			}
			if( zIn >= zEnd ){
				/* Lone backslash at the very end of the body: php keeps it */
				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));
				break;
			}
			n = sizeof(char); /* size of conversion */
			switch( zIn[0] ){
			case '$':
				/* Dollar sign */
				PH7_MemObjStringAppend(pObj,"$",sizeof(char));
				break;
			case '\\':
				/* A literal backslash */
				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));
				break;
			case 'e':
				/* Escape (ESC) ASCII code 27 */
				PH7_MemObjStringAppend(pObj,"\x1b",sizeof(char));
				break;
			case 'f':
				/* Form-feed (FF)[ctrl+l] ASCII code 12 */
				PH7_MemObjStringAppend(pObj,"\f",sizeof(char));
				break;
			case 'n':
				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */
				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));
				break;
			case 'r':
				/* Carriage return (CR)[ctrl+m] ASCII code 13 */
				PH7_MemObjStringAppend(pObj,"\r",sizeof(char));
				break;
			case 't':
				/* Horizontal tab (HT)[ctrl+i] ASCII code 9 */
				PH7_MemObjStringAppend(pObj,"\t",sizeof(char));
				break;
			case 'v':
				/* Vertical tab(VT)[ctrl+k] ASCII code 11 */
				PH7_MemObjStringAppend(pObj,"\v",sizeof(char));
				break;
			case '"':
				if( bHeredoc ){
					/* No active quote char in a heredoc: php keeps \" verbatim */
					PH7_MemObjStringAppend(pObj,"\\\"",sizeof(char)*2);
				}else{
					/* Double quote */
					PH7_MemObjStringAppend(pObj,"\"",sizeof(char));
				}
				break;
			case '0': case '1': case '2': case '3':
			case '4': case '5': case '6': case '7': {
				/* \[0-7]{1,3}: a character in octal notation. A value above \377
				 * warns and wraps to the low byte, matching php 8. */
				int c = 0;
				char cOut;
				for( zPtr = zIn ; zPtr < &zIn[3*sizeof(char)] ; zPtr++ ){
					if( zPtr >= zEnd || zPtr[0] < '0' || zPtr[0] > '7' ){
						break;
					}
					c = c * 8 + (zPtr[0] - '0');
				}
				if( c > 0xFF ){
					SyString sSeq;
					SyStringInitFromBuf(&sSeq,zIn,(sxu32)(zPtr-zIn));
					PH7_GenCompileError(&(*pGen),E_WARNING,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),
						"Octal escape sequence overflow \\%z is greater than \\377",&sSeq);
					c &= 0xFF;
				}
				cOut = (char)c; /* value byte, independent of host endianness */
				PH7_MemObjStringAppend(pObj,&cOut,sizeof(char));
				n = (sxu32)(zPtr-zIn);
				break;
			}
			case 'x':
				if( &zIn[1] < zEnd && SyisHex((unsigned char)zIn[1]) ){
					/* \x[0-9A-Fa-f]{1,2}: a character in hexadecimal notation */
					int c = SyHexToint(zIn[1]);
					char cOut;
					n += sizeof(char);
					if( &zIn[2] < zEnd && SyisHex((unsigned char)zIn[2]) ){
						c = (c << 4) + SyHexToint(zIn[2]);
						n += sizeof(char);
					}
					cOut = (char)c; /* value byte, independent of host endianness */
					PH7_MemObjStringAppend(pObj,&cOut,sizeof(char));
				}else{
					/* Not an escape: keep the backslash, as php does */
					PH7_MemObjStringAppend(pObj,"\\x",sizeof(char)*2);
				}
				break;
			case 'u':
				if( &zIn[1] < zEnd && zIn[1] == '{'
				 && !(&zIn[2] < zEnd && zIn[2] == '$') ){
					/* \u{codepoint}: UTF-8 encoding of the given codepoint (php 7+).
					 * php encodes surrogates verbatim, so the only invalid value
					 * is > U+10FFFF; malformed/empty braces are a compile error.
					 * "\u{$..." is excluded above: php treats it as a literal \u
					 * followed by {$...} curly interpolation. */
					sxu32 nCp = 0;
					zPtr = &zIn[2];
					while( zPtr < zEnd && SyisHex((unsigned char)zPtr[0]) ){
						if( nCp <= 0x10FFFF ){
							/* stop accumulating once out of range: keeps a long
							 * digit run from wrapping sxu32 */
							nCp = nCp * 16 + (sxu32)SyHexToint(zPtr[0]);
						}
						zPtr++;
					}
					if( zPtr == &zIn[2] || zPtr >= zEnd || zPtr[0] != '}' ){
						/* Error recorded (nErr>0 fails the whole compile); consume the
						 * malformed sequence so later errors are still reported. */
						rc = PH7_GenCompileError(&(*pGen),E_ERROR,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),
							"Invalid UTF-8 codepoint escape sequence");
						if( rc == SXERR_ABORT ){
							return SXERR_ABORT;
						}
						n = (sxu32)(zPtr-zIn);
						if( zPtr < zEnd && zPtr[0] == '}' ){
							n += sizeof(char);
						}
						break;
					}
					n = (sxu32)(&zPtr[1]-zIn); /* 'u{...}' incl. closing brace */
					if( nCp > 0x10FFFF ){
						rc = PH7_GenCompileError(&(*pGen),E_ERROR,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),
							"Invalid UTF-8 codepoint escape sequence: Codepoint too large");
						if( rc == SXERR_ABORT ){
							return SXERR_ABORT;
						}
						break;
					}
					{
						char zUtf[4];
						sxu8 *zOut = (sxu8 *)zUtf;
						SX_WRITE_UTF8(zOut,nCp);
						PH7_MemObjStringAppend(pObj,zUtf,(sxu32)(zOut-(sxu8 *)zUtf));
					}
				}else{
					/* Not an escape: keep the backslash, as php does */
					PH7_MemObjStringAppend(pObj,"\\u",sizeof(char)*2);
				}
				break;
			default:
				/* Unrecognized escape: keep the backslash, as php does.
				 * zIn[-1] is the backslash itself, so both bytes are contiguous
				 * in the source buffer — one batched append. */
				PH7_MemObjStringAppend(pObj,&zIn[-1],sizeof(char)*2);
				break;
			}
			/* Advance the stream cursor */
			zIn += n;
			continue;
		}
		if( zIn[0] == '{' ){
			/* Curly syntax */
			const char *zExpr;
			sxi32 iNest = 1;
			zIn++;
			zExpr = zIn;
			/* Synchronize with the next closing curly braces */
			while( zIn < zEnd ){
				if( zIn[0] == '{' ){
					/* Increment nesting level */
					iNest++;
				}else if(zIn[0] == '}' ){
					/* Decrement nesting level */
					iNest--;
					if( iNest <= 0 ){
						break;
					}
				}
				zIn++;
			}
			/* Process the expression */
			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			if( rc != SXERR_EMPTY ){
				++iCons;
				++nInterp;
			}
			if( zIn < zEnd ){
				/* Jump the trailing curly */
				zIn++;
			}
		}else{
			/* Simple syntax */
			const char *zExpr = zIn;
			/* Assemble variable name */
			for(;;){
				/* Jump leading dollars */
				while( zIn < zEnd && zIn[0] == '$' ){
					zIn++;
				}
				for(;;){
					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) || zIn[0] == '_' ) ){
						zIn++;
					}
					if((unsigned char)zIn[0] >= 0xc0 ){
						/* UTF-8 stream */
						zIn++;
						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){
							zIn++;
						}
						continue;
					}
					break;
				}
				if( zIn >= zEnd ){
					break;
				}
				if( zIn[0] == '[' ){
					sxi32 iSquare = 1;
					zIn++;
					while( zIn < zEnd ){
						if( zIn[0] == '[' ){
							iSquare++;
						}else if (zIn[0] == ']' ){
							iSquare--;
							if( iSquare <= 0 ){
								break;
							}
						}
						zIn++;
					}
					if( zIn < zEnd ){
						zIn++;
					}
					break;
				}else if(zIn[0] == '{' ){
					sxi32 iCurly = 1;
					zIn++;
					while( zIn < zEnd ){
						if( zIn[0] == '{' ){
							iCurly++;
						}else if (zIn[0] == '}' ){
							iCurly--;
							if( iCurly <= 0 ){
								break;
							}
						}
						zIn++;
					}
					if( zIn < zEnd ){
						zIn++;
					}
					break;
				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){
					/* Member access operator '->' */
					zIn += 2;
				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){
					/* Static member access operator '::' */
					zIn += 2;
				}else{
					break;
				}
			}
			/*
			 * "$a[name]" — php's SIMPLE syntax takes an unquoted subscript as the string key
			 * 'name', never as a constant. PH7 handed "$a[name]" straight to the expression
			 * compiler, where the bare word only resolved because an unknown constant used to
			 * fall back to its own name as a string. With undefined constants now a real
			 * Error, quote the key here so the simple syntax keeps meaning what php means.
			 * A numeric ($a[0]) or variable ($a[$k]) subscript is already unambiguous.
			 */
			{
				const char *zBr = zExpr;
				while( zBr < zIn && zBr[0] != '[' ){
					zBr++;
				}
				if( zBr < zIn && zIn[-1] == ']' ){
					const char *zKey = &zBr[1];
					const char *zKeyEnd = &zIn[-1];
					const char *zScan = zKey;
					int bBare = (zKey < zKeyEnd) && !SyisDigit(zKey[0]);
					while( bBare && zScan < zKeyEnd ){
						if( !SyisAlphaNum(zScan[0]) && zScan[0] != '_' ){
							bBare = 0;
						}
						zScan++;
					}
					if( bBare ){
						SyBlob sSub;
						SyBlobInit(&sSub,&pGen->pVm->sAllocator);
						SyBlobAppend(&sSub,zExpr,(sxu32)(zBr - zExpr));
						SyBlobAppend(&sSub,"['",2);
						SyBlobAppend(&sSub,zKey,(sxu32)(zKeyEnd - zKey));
						SyBlobAppend(&sSub,"']",2);
						rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,
							(const char *)SyBlobData(&sSub),
							(const char *)SyBlobData(&sSub) + SyBlobLength(&sSub));
						SyBlobRelease(&sSub);
						if( rc == SXERR_ABORT ){
							return SXERR_ABORT;
						}
						if( rc != SXERR_EMPTY ){
							++iCons;
							++nInterp;
						}
						pObj = 0;
						continue;
					}
				}
			}
			/*
			 * "${...}" string interpolation (every form: ${name}, ${expr}, ${$x}) was
			 * DEPRECATED by php 8.2 in favor of the canonical "{$...}". PHL targets php's
			 * *non-deprecated* surface, so it is a hard parse error here — never silently
			 * rewritten. The canonical "{$var}" reaches this compiler by a different path
			 * and is unaffected.
			 */
			if( &zExpr[1] < zIn && zExpr[0] == '$' && zExpr[1] == '{' ){
				PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,
					"syntax error, \"${\" string interpolation was removed in php 8.2, use \"{$...}\" instead");
				return SXERR_ABORT;
			}
			/* Process the expression */
			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			if( rc != SXERR_EMPTY ){
				++iCons;
				++nInterp;
			}
		}
		/* Invalidate the previously used constant */
		pObj = 0;
	}/*for(;;)*/
	if( iCons > 1 ){
		/* Concatenate all compiled constants */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);
	}else if( iCons == 1 && nInterp == 1 ){
		/* A string that is nothing but one interpolation ("$x") still has to
		 * PRODUCE A STRING. With no CAT to force the conversion the operand was
		 * left on the stack untouched, so `$s = "$x"` handed back $x's own type:
		 * "$arr" stayed an array (and skipped php's "Array to string conversion"
		 * warning), "$int" stayed an int, "$res" stayed a resource. */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CVT_STR,0,0,0,0);
	}
	/* Node successfully compiled */
	return SXRET_OK;
}
/*
 * Compile a double quoted string.
 *  See the block-comment above for more information.
 */
PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)
{
	sxi32 rc;
	rc = GenStateCompileString(&(*pGen),0/*bHeredoc*/);
	SXUNUSED(iCompileFlag); /* cc warning */
	/* Compilation result */
	return rc;
}
/*
 * Compile a Heredoc string.
 *  See the block-comment above for more information.
 */
PH7_PRIVATE sxi32 PH7_CompileHereDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)
{
	SyString sOrig, sStripped;
	sxi32 rc;
	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);
	if( rc != SXRET_OK ){
		return rc;
	}
	/* Temporarily swap in the dedented body so GenStateCompileString
	 * (which reads pGen->pIn->sData directly) sees the stripped content.
	 * Restore before returning so downstream code that references pIn is
	 * unaffected, including on the error path. */
	sOrig = pGen->pIn->sData;
	pGen->pIn->sData = sStripped;
	rc = GenStateCompileString(&(*pGen),1/*bHeredoc*/);
	pGen->pIn->sData = sOrig;
	SXUNUSED(iCompileFlag); /* cc warning */
	return rc;
}
/*
 * Compile an array entry whether it is a key or a value.
 *  Notes on array entries.
 *  According to the PHP language reference manual
 *  An array can be created by the array() language construct.
 *  It takes as parameters any number of comma-separated key => value pairs.
 *  array(  key =>  value
 *    , ...
 *    )
 *  A key may be either an integer or a string. If a key is the standard representation
 *  of an integer, it will be interpreted as such (i.e. "8" will be interpreted as 8, while
 *  "08" will be interpreted as "08"). Floats in key are truncated to integer.
 *  The indexed and associative array types are the same type in PHP, which can both
 *  contain integer and string indices.
 *  A value can be any PHP type.
 *  If a key is not specified for a value, the maximum of the integer indices is taken
 *  and the new key will be that value plus 1. If a key that already has an assigned value
 *  is specified, that value will be overwritten.
 */
PH7_PRIVATE sxi32 GenStateCompileArrayEntry(
	ph7_gen_state *pGen, /* Code generator state */
	SyToken *pIn,        /* Token stream */
	SyToken *pEnd,       /* End of the token stream */
	sxi32 iFlags,        /* Compilation flags */
	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *) /* Expression tree validator callback */
	)
{
	SyToken *pTmpIn,*pTmpEnd;
	sxi32 rc;
	/* Swap token stream */
	SWAP_DELIMITER(pGen,pIn,pEnd);
	/* Compile the expression*/
	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);
	/* Restore token stream */
	RE_SWAP_DELIMITER(pGen);
	return rc;
}
/*
 * Expression tree validator callback for the 'array' language construct.
 * Return SXRET_OK if the tree is valid. Any other return value indicates
 * an invalid expression tree and this function will generate the appropriate
 * error message.
 * See the routine responible of compiling the array language construct
 * for more inforation.
 */
static sxi32 GenStateArrayNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)
{
	sxi32 rc = SXRET_OK;
	if( pRoot->pOp ){
		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ &&
			pRoot->pOp->iOp != EXPR_OP_FUNC_CALL /* function() [Symisc extension: i.e: array(&foo())] */
			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){
			/* Unexpected expression */
			rc = PH7_GenSyntaxError(&(*pGen),pRoot->pStart,"\"->\" or \"?->\" or \"[\"");
			if( rc != SXERR_ABORT ){
				rc = SXERR_INVALID;
			}
		}
	}else if( pRoot->xCode != PH7_CompileVariable ){
		/* Unexpected expression */
		rc = PH7_GenSyntaxError(&(*pGen),pRoot->pStart,0);
		if( rc != SXERR_ABORT ){
			rc = SXERR_INVALID;
		}
	}
	return rc;
}
/*
 * Find the top-level '=>' (PH7_TK_ARRAY_OP) that separates an array/list entry's
 * key from its value within [pStart,pEnd). The scan skips any '=>' nested inside
 * brackets/parens/braces, inside an arrow-function signature (fn(...) =>), or
 * inside a match() {...} arm — none of which are key/value separators. Returns a
 * pointer to the '=>' token, or pEnd if the entry has no top-level separator.
 */
PH7_PRIVATE SyToken * GenStateFindTopLevelArrow(SyToken *pStart,SyToken *pEnd)
{
	SyToken *pCur = pStart;
	sxi32 iNest = 0;
	while( pCur < pEnd ){
		if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){
			return pCur;
		}
		/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.
		 * The '=>' inside an arrow function introduces the expression body,
		 * not an entry separator. Skip past the signature.
		 */
		if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){
			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pCur->pUserData);
			SyToken *pFn = pCur;
			if( nKw == PH7_TKWRD_STATIC && &pCur[1] < pEnd
				&& (pCur[1].nType & PH7_TK_KEYWORD)
				&& SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ){
				pFn = &pCur[1];
				nKw = PH7_TKWRD_FN;
			}
			if( nKw == PH7_TKWRD_FN ){
				pCur = pFn + 1; /* past 'fn' */
				if( pCur < pEnd && (pCur->nType & PH7_TK_AMPER) ){
					pCur++;
				}
				if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){
					pCur++;
					PH7_DelimitNestedTokens(pCur,pEnd,
						PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);
					if( pCur < pEnd ){
						pCur++;
					}
				}
				if( pCur < pEnd && (pCur->nType & PH7_TK_COLON) ){
					pCur++;
					if( pCur < pEnd && (pCur->nType & PH7_TK_OP)
						&& pCur->sData.nByte == 1
						&& pCur->sData.zString[0] == '?' ){
						pCur++;
					}
					if( pCur < pEnd
						&& (pCur->nType & (PH7_TK_KEYWORD|PH7_TK_ID)) ){
						pCur++;
					}
				}
				/* The rest of the entry is the arrow-function body — no outer
				 * key to extract. */
				return pEnd;
			}
			/* Match expression (PHP 8.0): the '=>' inside match arms is not an
			 * entry separator. Skip past the full match span. */
			if( nKw == PH7_TKWRD_MATCH ){
				pCur++; /* past 'match' */
				if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){
					pCur++;
					PH7_DelimitNestedTokens(pCur,pEnd,
						PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);
					if( pCur < pEnd ){
						pCur++;
					}
				}
				if( pCur < pEnd && (pCur->nType & PH7_TK_OCB) ){
					pCur++;
					PH7_DelimitNestedTokens(pCur,pEnd,
						PH7_TK_OCB,PH7_TK_CCB,&pCur);
					if( pCur < pEnd ){
						pCur++;
					}
				}
				continue;
			}
		}
		if( pCur->nType & (PH7_TK_LPAREN/*'('*/|PH7_TK_OSB/*'['*/|PH7_TK_OCB/*'{'*/) ){
			iNest++;
		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/|PH7_TK_CSB/*']'*/|PH7_TK_CCB/*'}'*/) ){
			/* Don't worry about mismatched brackets here, the expression
			 * parser will shortly detect any syntax error. */
			iNest--;
		}
		pCur++;
	}
	return pEnd;
}
/*
 * Compile the body of an array literal (shared by array() and short syntax []).
 * Assumes pGen->pIn points to the first content token and pGen->pEnd points
 * one past the last content token (i.e. the delimiters have been excluded).
 */
static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)
{
	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */
	SyToken *pKey,*pCur;
	sxi32 iEmitRef = 0;
	sxi32 iSpread = 0;
	sxi32 nPair = 0;
	sxi32 rc;
	xValidator = 0;
	for(;;){
		/* Jump leading commas. Exactly ONE separates two entries; a second one (or a comma
		 * at the very start) means an EMPTY element, which php rejects outright — PH7 just
		 * skipped them, so `array(,)` and `array(1,,2)` compiled silently. A TRAILING comma
		 * is legal and is handled by the loop exiting on the next pass. */
		{
			int nSkip = 0;
			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){
				nSkip++;
				pGen->pIn++;
			}
			if( nSkip > 1 || (nSkip > 0 && nPair < 1) ){
				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn[-1].nLine,
					"Cannot use empty array elements in arrays");
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
				return SXRET_OK;
			}
		}
		pCur = pGen->pIn;
		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){
			/* No more entry to process */
			break;
		}
		if( pCur >= pGen->pIn ){
			continue;
		}
		/* Compile the key if available */
		pKey = pCur;
		pCur = GenStateFindTopLevelArrow(pCur,pGen->pIn);
		rc = SXERR_EMPTY;
		if( pCur < pGen->pIn ){
			if( pKey == pCur ){
				/* `array( => 2)`: the entry STARTS with '=>', so it has no key. php rejects
				 * it; PH7 warned about a "Missing entry key" and compiled on, accepting
				 * source php refuses. (The `else if` below could never see this: the arrow
				 * IS found here, so control never reached it.)
				 * php names the literal's own closer, so short syntax expects ']'. */
				const char *zClose = (pGen->pEnd && (pGen->pEnd->nType & PH7_TK_CSB))
					? "\"]\"" : "\")\"";
				rc = PH7_GenSyntaxError(&(*pGen),pCur,zClose);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
				return SXRET_OK;
			}
			if( &pCur[1] >= pGen->pIn ){
				/* `array(1 => )`: php names the token that SHOULD have started the value —
				 * the ')' or ']' closing the literal — not the '=>' it just read. Passing 0
				 * makes the helper reach for the token past this entry's slice. */
				rc = PH7_GenSyntaxError(&(*pGen),0,0);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
				return SXRET_OK;
			}
			/* Compile the expression holding the key */
			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,
				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			pCur++; /* Jump the '=>' operator */
		}else{
			/* Reset back the cursor and point to the entry value */
			pCur = pKey;
		}
		if( rc == SXERR_EMPTY ){
			/* No key given: load the nil, TAGGED so LOAD_MAP knows this is an absent key
			 * (auto-index) rather than an explicit `null =>` one, which php deprecates. */
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,PH7_LOADC_NOKEY,0 /* nil index */,0,0);
		}
		if( pCur->nType & PH7_TK_AMPER /*'&'*/){
			/* Insertion by reference, [i.e: $a = array(&$x);] */
			xValidator = GenStateArrayNodeValidator; /* Only variable are allowed */
			iEmitRef = 1;
			pCur++; /* Jump the '&' token */
			if( pCur >= pGen->pIn ){
				/* Missing value */
				/* php reports the token that actually stopped it (`array(&)` -> the
				 * ')'), not a hand-written "missing referenced variable" fatal. */
				rc = PH7_GenSyntaxError(&(*pGen),pCur < pGen->pIn ? pCur : 0,0);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
				return SXRET_OK;
			}
		}
		/* Detect array unpack: '...$expr' as the entry value (PHP 7.4+, with
		 * string-key support since PHP 8.1). The parser strips the '...' inside
		 * ExprExtractNode; we only need to know it's there so we can emit
		 * PH7_OP_FLAG_SPREAD after the value, instructing LOAD_MAP to merge the
		 * resulting hashmap rather than insert it as a scalar entry. */
		iSpread = (pCur < pGen->pIn && (pCur->nType & PH7_TK_ELLIPSIS)) ? 1 : 0;
		if( iSpread && (rc != SXERR_EMPTY || iEmitRef) ){
			/* '[k => ...$a]' and '[&...$a]' are syntax errors in PHP — the
			 * '...' token cannot follow either '=>' or '&' inside an array
			 * literal. Emit the same Parse-error wording PHP uses so the
			 * output is engine-portable. */
			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pCur->nLine,
				"syntax error, unexpected token \"...\"");
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			return SXRET_OK;
		}
		/* Compile indice value. A BY-REF element (`'k' => &$a[$i]`) is an
		 * lvalue: php VIVIFIES a missing subscript when a reference is taken,
		 * so compile it in write context (LOAD_IDX iP2=1, create-if-missing)
		 * instead of a read-only load — which also keeps the undefined-key
		 * warning (a read-only diagnostic) from false-firing here. */
		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,
			iEmitRef ? EXPR_FLAG_LOAD_IDX_STORE
			         : EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,
			xValidator);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		if( iSpread ){
			/* Mark the value on TOS as a spread source; LOAD_MAP merges it. */
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FLAG_SPREAD,0,0,0,0);
		}else if( iEmitRef ){
			/* Emit the load reference instruction */
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);
		}
		xValidator = 0;
		iEmitRef = 0;
		iSpread = 0;
		nPair++;
	}
	/* Emit the load map instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);
	/* Node successfully compiled */
	return SXRET_OK;
}
/*
 * Compile the 'array' language construct.
 *	 According to the PHP language reference manual
 *   An array in PHP is actually an ordered map. A map is a type that associates
 *   values to keys. This type is optimized for several different uses; it can
 *   be treated as an array, list (vector), hash table (an implementation of a map)
 *   dictionary, collection, stack, queue, and probably more. As array values can be
 *   other arrays, trees and multidimensional arrays are also possible.
 */
PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)
{
	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */
	pGen->pIn += 2;
	pGen->pEnd--;
	SXUNUSED(iCompileFlag);
	/* php: a stray token in an `array( ... )` element is `... expecting ")"`. */
	{
		const char *zSave = pGen->zClauseCloser;
		sxi32 rc;
		pGen->zClauseCloser = "\")\"";
		rc = GenStateCompileArrayBody(pGen);
		pGen->zClauseCloser = zSave;
		return rc;
	}
}
/*
 * Compile the PHP 8.5 clone(...) call form:
 *   clone($object)                          -> identical to the `clone $object` operator
 *   clone($object, ['prop' => value, ...])  -> clone, run __clone(), then apply the
 *                                              property updates as scope-aware writes
 *   clone(object: $o, withProperties: [..]) -> the named-argument spelling
 * Codegen: compile the object argument and emit OP_CLONE (which clones and runs
 * __clone()); if a withProperties argument is present, compile it and emit
 * OP_CLONE_APPLY, which applies each update to the fresh clone AFTER __clone(),
 * honouring visibility / readonly-set-scope / typed-property enforcement in the
 * calling scope. The parser (ExprExtractNode) delimited this node's tokens as
 * `clone ( ... )`; pGen->pIn/pEnd point at the first/one-past-last of that range.
 */
PH7_PRIVATE sxi32 PH7_CompileCloneCall(ph7_gen_state *pGen,sxi32 iCompileFlag)
{
	SyToken *pIn,*pEnd,*pNext;
	SyToken *pObjStart = 0,*pObjEnd = 0;
	SyToken *pUpdStart = 0,*pUpdEnd = 0;
	int nArg = 0;
	sxi32 rc;
	SXUNUSED(iCompileFlag);
	/* pGen->pIn -> 'clone', pGen->pIn[1] -> '(', pGen->pEnd -> one past ')'. */
	pIn  = pGen->pIn + 2;   /* skip 'clone' and the opening '(' */
	pEnd = pGen->pEnd - 1;  /* exclude the closing ')' */
	/* clone(...) first-class-callable form: a lone ellipsis is the whole list. */
	if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){
		return PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,
			"clone(...) first-class callable form is not yet supported");
	}
	/* Split the (at most two) comma-separated arguments, tolerating named labels. */
	while( pIn < pEnd ){
		SyToken *pArgStart,*pArgEnd,*pName = 0;
		if( PH7_GetNextExpr(pIn,pEnd,&pNext) != SXRET_OK ){
			break;
		}
		pArgStart = pIn;
		pArgEnd   = pNext;
		/* Named-argument label: <ID|keyword> ':' expr. A single ':' is PH7_TK_COLON;
		 * '::' is a distinct operator token, so this never mis-fires on `A::B`. */
		if( (pArgEnd - pArgStart) >= 2
			&& (pArgStart[0].nType & (PH7_TK_ID|PH7_TK_KEYWORD))
			&& (pArgStart[1].nType & PH7_TK_COLON) ){
			pName = pArgStart;
			pArgStart += 2;
		}
		if( pName ){
			/* PHP named parameters are case-SENSITIVE, so `Object:`/`WITHPROPERTIES:`
			 * must be rejected as unknown (SyMemcmp, not SyStrnicmp). */
			if( pName->sData.nByte == sizeof("object")-1
				&& SyMemcmp(pName->sData.zString,"object",sizeof("object")-1) == 0 ){
				pObjStart = pArgStart; pObjEnd = pArgEnd;
			}else if( pName->sData.nByte == sizeof("withProperties")-1
				&& SyMemcmp(pName->sData.zString,"withProperties",sizeof("withProperties")-1) == 0 ){
				pUpdStart = pArgStart; pUpdEnd = pArgEnd;
			}else{
				return PH7_GenCompileError(pGen,E_ERROR,pName->nLine,
					"Unknown named parameter $%z",&pName->sData);
			}
		}else if( nArg == 0 ){
			pObjStart = pArgStart; pObjEnd = pArgEnd;
		}else if( nArg == 1 ){
			pUpdStart = pArgStart; pUpdEnd = pArgEnd;
		}else{
			return PH7_GenCompileError(pGen,E_ERROR,pArgStart->nLine,
				"clone() expects at most 2 arguments");
		}
		nArg++;
		pIn = pNext;
		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){
			pIn++; /* step over the argument separator */
		}
	}
	if( pObjStart == 0 || pObjStart >= pObjEnd ){
		return PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
			"clone() expects at least 1 argument, 0 given");
	}
	/* Object argument -> clone (+ __clone()). */
	rc = GenStateCompileArrayEntry(pGen,pObjStart,pObjEnd,EXPR_FLAG_RDONLY_LOAD,0);
	if( rc == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_CLONE,0,0,0,0);
	/* Property updates (evaluated after __clone runs). */
	if( pUpdStart && pUpdStart < pUpdEnd ){
		rc = GenStateCompileArrayEntry(pGen,pUpdStart,pUpdEnd,EXPR_FLAG_RDONLY_LOAD,0);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CLONE_APPLY,0,0,0,0);
	}
	return SXRET_OK;
}
/*
 * Compile a short array literal using the PHP 5.4 bracket syntax.
 * [1, 2, 3] is equivalent to array(1, 2, 3).
 * ['key' => 'value'] is equivalent to array('key' => 'value').
 */
PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)
{
	/* Jump the leading '[', exclude trailing ']'. */
	pGen->pIn++;
	pGen->pEnd--;
	SXUNUSED(iCompileFlag);
	/* php: a stray token in a `[ ... ]` element is `... expecting "]"`. */
	{
		const char *zSave = pGen->zClauseCloser;
		sxi32 rc;
		pGen->zClauseCloser = "\"]\"";
		rc = GenStateCompileArrayBody(pGen);
		pGen->zClauseCloser = zSave;
		return rc;
	}
}
/*
 * Expression tree validator callback for the 'list' language construct.
 * Return SXRET_OK if the tree is valid. Any other return value indicates
 * an invalid expression tree and this function will generate the appropriate
 * error message.
 * See the routine responible of compiling the list language construct
 * for more inforation.
 */
static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)
{
	sxi32 rc = SXRET_OK;
	if( pRoot->pOp ){
		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */
			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){
				/* Unexpected expression */
				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,
					"Assignments can only happen to writable values");
				if( rc != SXERR_ABORT ){
					rc = SXERR_INVALID;
				}
		}
	}else if( pRoot->xCode != PH7_CompileVariable ){
		/* Unexpected expression */
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,
			"Assignments can only happen to writable values");
		if( rc != SXERR_ABORT ){
			rc = SXERR_INVALID;
		}
	}
	return rc;
}
/*
 * Compile the 'list' language construct.
 *  According to the PHP language reference
 *  list(): Assign variables as if they were an array.
 *  list() is used to assign a list of variables in one operation.
 *  Description
 *   array list (mixed $varname [, mixed $... ] )
 *   Like array(), this is not really a function, but a language construct.
 *   list() is used to assign a list of variables in one operation.
 *  Parameters
 *   $varname: A variable.
 *  Return Values
 *   The assigned array.
 */
/* Nested list entry recorded during first pass of list body compilation */
struct NestedListEntry {
	sxi32 nIndex;        /* Position in the outer list (0-based) */
	SyToken *pStart;     /* Token range: start of nested construct */
	SyToken *pEnd;       /* Token range: past closing delimiter */
	sxi32 isShort;       /* 1 if [...] form, 0 if list(...) form */
};
/*
 * Compile the body of a *keyed* list/short-list destructuring (PHP 7.1), where
 * every entry has the form `keyExpr => target`. The source array is on the stack
 * top on entry and remains there on exit, mirroring the positional LOAD_LIST
 * path so the caller's teardown is unchanged. For each entry: DUP the source,
 * push the key, LOAD_IDX to fetch source[key] (NULL on a missing key, silently,
 * like a normal subscript read), then assign the fetched value to the target — a
 * nested [...]/list() recurses, a simple lvalue uses the same STORE fold as a
 * normal assignment (the value sits below the lvalue-load, exactly as in
 * GenStateEmitExprCode where the assignment RHS precedes the LHS load).
 */
static sxi32 GenStateCompileKeyedListBody(ph7_gen_state *pGen)
{
	SyToken *pNext;
	sxi32 rc;
	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){
		SyToken *pArrow,*pTarget;
		/* Split `keyExpr => target` at the top-level '=>' */
		pArrow = GenStateFindTopLevelArrow(pGen->pIn,pNext);
		pTarget = &pArrow[1];
		if( pArrow <= pGen->pIn || pTarget >= pNext ){
			/* Empty key (`[ => $v]`) or empty value (`["k" =>]`): PHP rejects
			 * both. Reject rather than silently emitting unbalanced bytecode. */
			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,
				"Cannot use empty array entries in keyed array assignment");
			return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;
		}
		/* DUP the source array (it is on the stack top) */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);
		/* Compile the key expression; it is pushed above the DUP'd source */
		rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pArrow,EXPR_FLAG_RDONLY_LOAD,0);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		/* LOAD_IDX: pop the key, replace the DUP'd source with source[key].
		 * iP2=7 is the keyed-destructuring read context: an array source reads like
		 * iP2=0 (missing key loads NULL silently, matching a normal `$arr[$k]` read;
		 * PHP also emits an "Undefined array key" warning here, PHL omits it — §3.7),
		 * but a NON-array source yields NULL + a per-key "Cannot use <type> as array"
		 * warning instead of char-indexing a string (matching PHP's OP_LOAD_LIST path). */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,7,0,0);
		if( pTarget < pNext && ( (pTarget->nType & PH7_TK_OSB)
			|| ( (pTarget->nType & PH7_TK_KEYWORD)
				&& SX_PTR_TO_INT(pTarget->pUserData) == PH7_TKWRD_LIST ) ) ){
			/* Nested destructuring:  ["k" => [ ... ]]  or  ["k" => list( ... )].
			 * Treat source[key] as the inner body's source, then drop the
			 * leftover it leaves behind (mirrors the positional nested path). */
			sxi32 isShort = (pTarget->nType & PH7_TK_OSB) != 0;
			SyToken *pSavedIn = pGen->pIn;
			SyToken *pSavedEnd = pGen->pEnd;
			pGen->pIn = pTarget;
			pGen->pEnd = pNext;
			rc = isShort ? PH7_CompileShortList(&(*pGen),0)
			             : PH7_CompileList(&(*pGen),0);
			pGen->pIn = pSavedIn;
			pGen->pEnd = pSavedEnd;
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);
		}else{
			/* Simple lvalue target ($v / $o->p / $a[i] / Cls::$s). source[key]
			 * is already on the stack as the value; compiling the target appends
			 * its lvalue-load, which we fold into a STORE just as a normal
			 * assignment does. */
			VmInstr *pInstr;
			sxi32 iVmOp = PH7_OP_STORE;
			sxi32 iP1 = 0, iP2 = 0;
			void *p3 = 0;
			rc = GenStateCompileArrayEntry(&(*pGen),pTarget,pNext,
				EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);
			if( rc != SXRET_OK ){
				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;
			}
			if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){
				if( pInstr->iOp == PH7_OP_MEMBER ){
					iP2 = 1; /* member store: keep MEMBER, store value below it */
				}else if( pInstr->iOp == PH7_OP_LOAD_IDX ){
					iVmOp = PH7_OP_STORE_IDX;
					iP1 = pInstr->iP1;
					(void)PH7_VmPopInstr(pGen->pVm);
				}else{
					p3 = pInstr->p3; /* named store: $v = value */
					(void)PH7_VmPopInstr(pGen->pVm);
				}
			}
			PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);
			/* STORE leaves the assigned value on the stack top; drop it so the
			 * source array is back on top for the next entry. */
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);
		}
		pGen->pIn = &pNext[1];
	}
	return SXRET_OK;
}
/*
 * Shared body for list() and short list [...] compilation.
 * Assumes pGen->pIn and pGen->pEnd are already positioned past
 * the opening delimiter and before the closing delimiter.
 */
static sxi32 GenStateCompileListBody(ph7_gen_state *pGen)
{
	SySet sNested; /* Dynamically-sized container of NestedListEntry */
	SyToken *pNext;
	SyToken *pClassifyIn;
	sxi32 nKeyed = 0, nPositional = 0, nEmpty = 0;
	sxi32 nExpr;
	sxi32 rc;
	/* First pass: classify entries as keyed (`k => v`), positional, or empty
	 * skip slots ([,]). A list level must be entirely keyed or entirely
	 * positional — PHP fatals on a mix, and on an empty slot inside a keyed
	 * list. */
	pClassifyIn = pGen->pIn;
	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){
		if( pGen->pIn >= pNext ){
			nEmpty++;
		}else if( GenStateFindTopLevelArrow(pGen->pIn,pNext) < pNext ){
			nKeyed++;
		}else{
			nPositional++;
		}
		pGen->pIn = &pNext[1];
	}
	pGen->pIn = pClassifyIn;
	if( nKeyed > 0 && nEmpty > 0 ){
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,
			"Cannot use empty array entries in keyed array assignment");
		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;
	}
	if( nKeyed > 0 && nPositional > 0 ){
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,
			"Cannot mix keyed and unkeyed array entries in assignments");
		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;
	}
	if( nKeyed > 0 ){
		return GenStateCompileKeyedListBody(pGen);
	}
	nExpr = 0;
	SySetInit(&sNested,&pGen->pVm->sAllocator,sizeof(struct NestedListEntry));
	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){
		if( pGen->pIn < pNext ){
			/* Check for nested list() */
			if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&
				SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){
				/* Record this nested list for post-processing */
				SyToken *pListEnd = 0;
				if( &pGen->pIn[1] < pNext && (pGen->pIn[1].nType & PH7_TK_LPAREN) ){
					PH7_DelimitNestedTokens(pGen->pIn+2,pNext,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);
				}
				if( pListEnd ){
					struct NestedListEntry sEntry;
					sEntry.nIndex = nExpr;
					sEntry.pStart = pGen->pIn;
					sEntry.pEnd = pListEnd + 1;
					sEntry.isShort = 0;
					SySetPut(&sNested,(const void *)&sEntry);
				}
				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */
				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);
			}else if( pGen->pIn->nType & PH7_TK_OSB ){
				/* Nested short destructuring [...] */
				SyToken *pBracketEnd = 0;
				PH7_DelimitNestedTokens(pGen->pIn+1,pNext,PH7_TK_OSB,PH7_TK_CSB,&pBracketEnd);
				if( pBracketEnd ){
					struct NestedListEntry sEntry;
					sEntry.nIndex = nExpr;
					sEntry.pStart = pGen->pIn;
					sEntry.pEnd = pBracketEnd + 1;
					sEntry.isShort = 1;
					SySetPut(&sNested,(const void *)&sEntry);
				}
				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */
				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);
			}else{
				/* Compile the expression holding the variable */
				rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);
				if( rc != SXRET_OK ){
					SySetRelease(&sNested);
					return SXRET_OK;
				}
			}
		}else{
			/* Empty entry,load NULL */
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);
		}
		nExpr++;
		/* Advance the stream cursor */
		pGen->pIn = &pNext[1];
	}
	/* Emit the LOAD_LIST instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,0,0,0);
	/* After LOAD_LIST, the source array is still on the stack top.
	 * For each nested entry, emit code to extract the sub-array
	 * at the corresponding index and recursively destructure it.
	 */
	if( SySetUsed(&sNested) > 0 ){
		struct NestedListEntry *apNested = (struct NestedListEntry *)SySetBasePtr(&sNested);
		sxu32 i;
		for(i = 0; i < SySetUsed(&sNested); i++){
			SyToken *pSavedIn = pGen->pIn;
			SyToken *pSavedEnd = pGen->pEnd;
			ph7_value *pIdx;
			sxu32 nConstIdx;
			/* DUP the source array (it's on stack top) */
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);
			/* Push the integer index for this nested entry */
			pIdx = PH7_ReserveConstObj(pGen->pVm,&nConstIdx);
			if( pIdx == 0 ){
				PH7_GenCompileError(&(*pGen),E_ERROR,0,"Fatal, PH7 engine is running out of memory");
				SySetRelease(&sNested);
				return SXERR_ABORT;
			}
			PH7_MemObjInitFromInt(pGen->pVm,pIdx,(sxi64)apNested[i].nIndex);
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nConstIdx,0,0);
			/* LOAD_IDX: pop index, replace DUP'd source with source[index].
			 * iP2=2 signals the VM to emit an "Undefined array key" warning
			 * when the key is missing (PHP-compatible list destructuring).
			 */
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,2,0,0);
			/* Recursively compile the inner list */
			pGen->pIn = apNested[i].pStart;
			pGen->pEnd = apNested[i].pEnd;
			if( apNested[i].isShort ){
				rc = PH7_CompileShortList(&(*pGen),0);
			}else{
				rc = PH7_CompileList(&(*pGen),0);
			}
			pGen->pIn = pSavedIn;
			pGen->pEnd = pSavedEnd;
			if( rc == SXERR_ABORT ){
				SySetRelease(&sNested);
				return SXERR_ABORT;
			}
			/* Pop the leftover source[index] from the inner LOAD_LIST */
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);
		}
	}
	SySetRelease(&sNested);
	/* Node successfully compiled */
	return SXRET_OK;
}
PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)
{
	/* Jump the 'list' keyword, the leading '(' and exclude trailing ')' */
	pGen->pIn += 2;
	pGen->pEnd--;
	SXUNUSED(iCompileFlag);
	return GenStateCompileListBody(pGen);
}
PH7_PRIVATE sxi32 PH7_CompileShortList(ph7_gen_state *pGen,sxi32 iCompileFlag)
{
	/* Jump the leading '[', exclude trailing ']'. */
	pGen->pIn++;
	pGen->pEnd--;
	SXUNUSED(iCompileFlag);
	return GenStateCompileListBody(pGen);
}
