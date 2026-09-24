/**
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
#ifndef PH7_DISABLE_BUILTIN_FUNC
/*
 * mb_* multibyte string functions, UTF-8 only (NEWPLAN band D; the recorded
 * §10 scope cut — php's full encoding zoo is out). Codepoint semantics match
 * php 8.5 byte-for-byte for UTF-8 input; case mapping is algorithmic over
 * ASCII, Latin-1, Latin Extended-A, Greek and Cyrillic (full Unicode tables
 * recorded as a residual — unmapped codepoints pass through unchanged).
 */

/* --- UTF-8 primitives ------------------------------------------------- */

/* Byte length of the ill-formed run at z[0..n-1]: the maximal prefix php's
 * decoder consumes before giving up — the lead byte plus every continuation
 * byte that is still in range for it. mbstring reports that whole prefix as ONE
 * character and substitutes ONE '?' for it, so "\xe0\xa0" (a truncated 3-byte
 * sequence) is one character, while "\xff\xfe" is two: neither byte can lead. */
static sxu32 MbUtf8BadLen(const unsigned char *z,sxu32 n)
{
	sxu32 c = z[0],need,iLow,iHigh,i;
	if( c >= 0xC2 && c <= 0xDF ){
		need = 2; iLow = 0x80; iHigh = 0xBF;
	}else if( c >= 0xE0 && c <= 0xEF ){
		need = 3; iLow = (c == 0xE0) ? 0xA0 : 0x80; iHigh = (c == 0xED) ? 0x9F : 0xBF;
	}else if( c >= 0xF0 && c <= 0xF4 ){
		need = 4; iLow = (c == 0xF0) ? 0x90 : 0x80; iHigh = (c == 0xF4) ? 0x8F : 0xBF;
	}else{
		return 1; /* 80..C1 or F5..FF: cannot lead anything */
	}
	for( i = 1 ; i < need && i < n ; ++i ){
		sxu32 lo = (i == 1) ? iLow : 0x80;
		sxu32 hi = (i == 1) ? iHigh : 0xBF;
		if( z[i] < lo || z[i] > hi ){
			break;
		}
	}
	return i;
}
/* Decode the character at z (n bytes available); *pLen = the bytes it occupies.
 * Returns the codepoint, or -1 when the sequence is ILL-FORMED — in which case
 * *pLen is the run above, which php's mbstring counts as one character and
 * re-encodes as '?'.
 *
 * This used to be byte-transparent: an undecodable byte came back AS ITSELF
 * with length 1, so mb_strtolower("\xff\xfe") answered the two bytes
 * re-encoded as UTF-8 (\xc3\xbf\xc3\xbe) — latin-1 semantics php does not have,
 * and characters PHL invented — where php answers "??". The over-long,
 * surrogate and past-U+10FFFF forms were accepted as well; validation is
 * PH7_Utf8ReadStrict's job now (the same reader json_encode uses). */
static sxi32 MbUtf8Decode(const unsigned char *z,sxu32 n,sxu32 *pLen)
{
	sxi32 iCp = PH7_Utf8ReadStrict(z,n,pLen);
	if( iCp < 0 ){
		*pLen = MbUtf8BadLen(z,n);
	}
	return iCp;
}
/* Encode cp into z (up to 4 bytes); returns the byte count */
static sxu32 MbUtf8Encode(sxu32 cp,unsigned char *z)
{
	if( cp < 0x80 ){
		z[0] = (unsigned char)cp;
		return 1;
	}
	if( cp < 0x800 ){
		z[0] = (unsigned char)(0xC0 | (cp >> 6));
		z[1] = (unsigned char)(0x80 | (cp & 0x3F));
		return 2;
	}
	if( cp < 0x10000 ){
		z[0] = (unsigned char)(0xE0 | (cp >> 12));
		z[1] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
		z[2] = (unsigned char)(0x80 | (cp & 0x3F));
		return 3;
	}
	z[0] = (unsigned char)(0xF0 | (cp >> 18));
	z[1] = (unsigned char)(0x80 | ((cp >> 12) & 0x3F));
	z[2] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
	z[3] = (unsigned char)(0x80 | (cp & 0x3F));
	return 4;
}
/* Byte offset of codepoint index iCp (clamped to the buffer end) */
static sxu32 MbUtf8Skip(const char *zIn,sxu32 nByte,sxu32 iCp)
{
	const unsigned char *z = (const unsigned char *)zIn;
	sxu32 i = 0,nLen;
	while( i < nByte && iCp > 0 ){
		MbUtf8Decode(&z[i],nByte - i,&nLen);
		i += nLen;
		iCp--;
	}
	return i;
}

/* --- Case mapping (ASCII, Latin-1, Latin Ext-A, Greek, Cyrillic) ------ */

static sxu32 MbToLower(sxu32 c)
{
	if( c < 0x80 ){ return (c >= 'A' && c <= 'Z') ? c + 0x20 : c; }
	if( c >= 0x00C0 && c <= 0x00DE && c != 0x00D7 ){ return c + 0x20; }
	if( c >= 0x0100 && c <= 0x0137 ){ return c | 1; }
	if( c >= 0x0139 && c <= 0x0148 ){ return ((c - 1) | 1) + 1; }
	if( c >= 0x014A && c <= 0x0177 ){ return c | 1; }
	if( c == 0x0178 ){ return 0x00FF; }
	if( c >= 0x0179 && c <= 0x017E ){ return ((c - 1) | 1) + 1; }
	if( c >= 0x0391 && c <= 0x03A9 && c != 0x03A2 ){ return c + 0x20; }
	if( c >= 0x0410 && c <= 0x042F ){ return c + 0x20; }
	if( c >= 0x0400 && c <= 0x040F ){ return c + 0x50; }
	return c;
}
static sxu32 MbToUpper(sxu32 c)
{
	if( c < 0x80 ){ return (c >= 'a' && c <= 'z') ? c - 0x20 : c; }
	if( c >= 0x00E0 && c <= 0x00FE && c != 0x00F7 ){ return c - 0x20; }
	if( c == 0x00FF ){ return 0x0178; }
	if( c >= 0x0100 && c <= 0x0137 ){ return c & ~(sxu32)1; }
	if( c >= 0x0139 && c <= 0x0148 ){ return ((c - 1) & ~(sxu32)1) + 1; }
	if( c >= 0x014A && c <= 0x0177 ){ return c & ~(sxu32)1; }
	if( c >= 0x0179 && c <= 0x017E ){ return ((c - 1) & ~(sxu32)1) + 1; }
	if( c == 0x017F ){ return 'S'; } /* long s */
	if( c >= 0x03B1 && c <= 0x03C9 && c != 0x03C2 ){ return c - 0x20; }
	if( c == 0x03C2 ){ return 0x03A3; } /* final sigma */
	if( c >= 0x0430 && c <= 0x044F ){ return c - 0x20; }
	if( c >= 0x0450 && c <= 0x045F ){ return c - 0x50; }
	return c;
}
/* A codepoint counts as a letter for title-case word boundaries */
static int MbIsAlnum(sxu32 c)
{
	if( c < 0x80 ){
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
	}
	/* non-ASCII letters: anything the case mapper knows, plus CJK & co —
	 * treat every non-ASCII codepoint as a word character (php's word
	 * boundary for TITLE mode is whitespace/punct, all ASCII) */
	return 1;
}

/* --- Encodings and the character walk ---------------------------------- */

/*
 * The three encodings PHL models (the §10 scope cut — php's full encoding zoo
 * is out; a php-VALID name PHL does not model, e.g. SJIS, raises the same
 * ValueError php uses for a truly invalid name). They differ in exactly two
 * ways, and both matter to every function here: how many BYTES a character
 * takes, and which byte sequences are characters at all.
 *
 *   UTF-8    1..4 bytes per character; an ill-formed run is php's error
 *            character, which mbstring counts as one and re-encodes as '?'.
 *   LATIN1   one byte per character, and its VALUE is the code point — which
 *            is what makes 8bit/binary/ISO-8859-1 one encoding here: php's
 *            answers for the three are byte-identical on every function below.
 *   ASCII    one byte per character; a byte over 0x7F is an error character.
 */
#define MB_ENC_UTF8    0
#define MB_ENC_LATIN1  1
#define MB_ENC_ASCII   2
/* The code an error character carries. Not a code point (php's own marker is
 * not one either), so it compares equal to another error character and to
 * nothing else — a literal '?' in the haystack is NOT a match for an
 * undecodable needle, which is what folding through '?' used to make it. */
#define MB_BAD_CODE  0xFFFFFFFFu

/* Resolve an encoding name to an MB_ENC_* id, or -1 when it is outside PHL's
 * modelled set. Surrounding ASCII whitespace is trimmed (php accepts " UTF-8"). */
static int MbConvEncId(const char *z,int n)
{
	while( n > 0 && (z[0]==' '||z[0]=='\t'||z[0]=='\n'||z[0]=='\r') ){ z++; n--; }
	while( n > 0 && (z[n-1]==' '||z[n-1]=='\t'||z[n-1]=='\n'||z[n-1]=='\r') ){ n--; }
	if( (n==5 && SyStrnicmp(z,"UTF-8",5)==0) || (n==4 && SyStrnicmp(z,"UTF8",4)==0) ){
		return MB_ENC_UTF8;
	}
	if( (n==10 && SyStrnicmp(z,"ISO-8859-1",10)==0) || (n==9 && SyStrnicmp(z,"ISO8859-1",9)==0)
	 || (n==6 && SyStrnicmp(z,"latin1",6)==0) || (n==4 && SyStrnicmp(z,"8bit",4)==0)
	 || (n==6 && SyStrnicmp(z,"binary",6)==0) ){
		return MB_ENC_LATIN1;
	}
	if( (n==5 && SyStrnicmp(z,"ASCII",5)==0) || (n==8 && SyStrnicmp(z,"US-ASCII",8)==0) ){
		return MB_ENC_ASCII;
	}
	return -1;
}
/* Validate the optional $encoding argument: an MB_ENC_* id, or -1 after raising
 * php's ValueError. A missing/null argument is php's internal encoding, which
 * PHL fixes at UTF-8. */
static int MbEncodingArg(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgNo)
{
	const char *zEnc;
	int nEnc,iEnc;
	if( pArg == 0 || ph7_value_is_null(pArg) ){
		return MB_ENC_UTF8;
	}
	zEnc = ph7_value_to_string(pArg,&nEnc);
	iEnc = MbConvEncId(zEnc,nEnc);
	if( iEnc < 0 ){
		PH7_VmThrowException(pCtx,"ValueError",
			"%s(): Argument #%d ($encoding) must be a valid encoding, \"%.*s\" given",
			zFunc,iArgNo,nEnc,zEnc);
		return -1;
	}
	return iEnc;
}
/* Decode the character at z[0..n-1] under iEnc: its code (a code point, or
 * MB_BAD_CODE for an error character) with *pLen set to the bytes it spans. */
static sxu32 MbNextCode(const unsigned char *z,sxu32 n,int iEnc,sxu32 *pLen)
{
	sxi32 iCp;
	if( iEnc != MB_ENC_UTF8 ){
		*pLen = 1;
		return (iEnc == MB_ENC_ASCII && z[0] > 0x7F) ? MB_BAD_CODE : (sxu32)z[0];
	}
	iCp = MbUtf8Decode(z,n,pLen);
	return iCp < 0 ? MB_BAD_CODE : (sxu32)iCp;
}
/* Character count of zIn[0..nByte-1] under iEnc */
static sxu32 MbStrlen(const char *zIn,sxu32 nByte,int iEnc)
{
	const unsigned char *z = (const unsigned char *)zIn;
	sxu32 i = 0,nCp = 0,nLen;
	if( iEnc != MB_ENC_UTF8 ){
		return nByte;   /* one byte per character */
	}
	while( i < nByte ){
		MbUtf8Decode(&z[i],nByte - i,&nLen);
		i += nLen;
		nCp++;
	}
	return nCp;
}
/* Byte offset of character index iCp (clamped to the buffer end) */
static sxu32 MbSkip(const char *zIn,sxu32 nByte,sxu32 iCp,int iEnc)
{
	if( iEnc != MB_ENC_UTF8 ){
		return iCp < nByte ? iCp : nByte;
	}
	return MbUtf8Skip(zIn,nByte,iCp);
}
/*
 * A decoded string: one code per character plus the byte offset each one starts
 * at (nChar+1 entries, so the last is the buffer length). php works the same
 * way — mbstring converts to a wchar buffer and operates there — and it is what
 * lets a search answer in CHARACTERS while slicing in BYTES. The cost is two
 * words per input byte, which is why the buffer is capped well inside what the
 * allocator's byte count can express.
 */
#define MB_TEXT_MAX  0x0FFFFFFFu
typedef struct mb_text mb_text;
struct mb_text {
	const char *zIn;   /* the source buffer (not owned) */
	sxu32 nByte;
	sxu32 *aCode;      /* nChar codes */
	sxu32 *aOfft;      /* nChar+1 byte offsets */
	sxu32 nChar;
};
/* Decode zIn under iEnc into pText, lower-casing every code when bFold is set
 * (which is what makes an error character equal to any other one: they share
 * MB_BAD_CODE, and folding is the only mode php compares them loosely in). */
static int MbTextDecode(ph7_context *pCtx,mb_text *pText,const char *zIn,sxu32 nByte,
	int iEnc,int bFold)
{
	const unsigned char *z = (const unsigned char *)zIn;
	sxu32 i = 0,n = 0,nLen,nSlot;
	pText->zIn = zIn;
	pText->nByte = nByte;
	pText->nChar = 0;
	if( nByte > MB_TEXT_MAX ){
		return PH7_ContextMemoryError(pCtx);
	}
	/* One slot per byte is the upper bound on the character count */
	nSlot = nByte + 1;
	pText->aCode = (sxu32 *)ph7_context_alloc_chunk(pCtx,
		(unsigned int)(nSlot * 2 * sizeof(sxu32)),FALSE,TRUE);
	if( pText->aCode == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	pText->aOfft = &pText->aCode[nSlot];
	while( i < nByte ){
		sxu32 cp = MbNextCode(&z[i],nByte - i,iEnc,&nLen);
		if( bFold && cp != MB_BAD_CODE ){
			cp = MbToLower(cp);
		}
		pText->aCode[n] = cp;
		pText->aOfft[n] = i;
		i += nLen;
		n++;
	}
	pText->aOfft[n] = nByte;
	pText->nChar = n;
	return PH7_OK;
}
/* Does pN occur in pH starting at character i? bExactBad compares the raw bytes
 * of an error character rather than taking every one for equal, which is php's
 * rule for a case-SENSITIVE UTF-8 search: mb_strpos("a\xffb","\xfe") is false
 * there, where the same pair matches under mb_stripos (and under ASCII, whose
 * error character carries nothing to tell apart). */
static int MbTextMatchAt(const mb_text *pH,sxu32 i,const mb_text *pN,int bExactBad)
{
	sxu32 k;
	for( k = 0 ; k < pN->nChar ; ++k ){
		if( pH->aCode[i+k] != pN->aCode[k] ){
			return 0;
		}
		if( bExactBad && pH->aCode[i+k] == MB_BAD_CODE ){
			sxu32 nH = pH->aOfft[i+k+1] - pH->aOfft[i+k];
			sxu32 nN = pN->aOfft[k+1] - pN->aOfft[k];
			if( nH != nN || SyMemcmp(&pH->zIn[pH->aOfft[i+k]],&pN->zIn[pN->aOfft[k]],nH) != 0 ){
				return 0;
			}
		}
	}
	return 1;
}

/* --- The functions ----------------------------------------------------- */

/* int mb_strlen(string $string, ?string $encoding = null) */
static int PH7_builtin_mb_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn;
	int nByte,iEnc;
	if( nArg < 1 ){
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	iEnc = MbEncodingArg(pCtx,nArg > 1 ? apArg[1] : 0,"mb_strlen",2);
	if( iEnc < 0 ){
		return PH7_OK;
	}
	zIn = ph7_value_to_string(apArg[0],&nByte);
	ph7_result_int64(pCtx,(ph7_int64)MbStrlen(zIn,(sxu32)nByte,iEnc));
	return PH7_OK;
}
/* Set the call result to zIn[0..nByte-1] with every ill-formed run replaced by
 * '?', php's substitution character. A well-formed buffer copies verbatim. */
static void MbResultSubstituted(ph7_context *pCtx,const char *zIn,sxu32 nByte)
{
	const unsigned char *z = (const unsigned char *)zIn;
	SyBlob sOut;
	sxu32 i = 0,nLen;
	/* Well-formed is the overwhelmingly common case: check first and hand back
	 * the buffer as it stands rather than rebuilding it. */
	while( i < nByte && MbUtf8Decode(&z[i],nByte - i,&nLen) >= 0 ){
		i += nLen;
	}
	if( i >= nByte ){
		ph7_result_string(pCtx,zIn,(int)nByte);
		return;
	}
	i = 0;
	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);
	while( i < nByte ){
		if( MbUtf8Decode(&z[i],nByte - i,&nLen) < 0 ){
			SyBlobAppend(&sOut,"?",1);
		}else{
			SyBlobAppend(&sOut,&z[i],nLen);
		}
		i += nLen;
	}
	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));
	SyBlobRelease(&sOut);
}
/* string mb_substr(string $string, int $start, ?int $length = null,
 *                  ?string $encoding = null) */
static int PH7_builtin_mb_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn;
	int nByte,iEnc;
	sxi64 iStart,iLen;
	sxu32 nCp,iOfft,iEnd;
	int bLenSet = 0;
	if( nArg < 2 ){
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	iEnc = MbEncodingArg(pCtx,nArg > 3 ? apArg[3] : 0,"mb_substr",4);
	if( iEnc < 0 ){
		return PH7_OK;
	}
	zIn = ph7_value_to_string(apArg[0],&nByte);
	iStart = ph7_value_to_int64(apArg[1]);
	iLen = 0;
	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){
		iLen = ph7_value_to_int64(apArg[2]);
		bLenSet = 1;
	}
	nCp = MbStrlen(zIn,(sxu32)nByte,iEnc);
	if( iStart < 0 ){
		iStart = (sxi64)nCp + iStart;
		if( iStart < 0 ){ iStart = 0; }
	}
	if( iStart >= (sxi64)nCp ){
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	if( !bLenSet ){
		iLen = (sxi64)nCp - iStart;
	}else if( iLen < 0 ){
		iLen = ((sxi64)nCp - iStart) + iLen;
		if( iLen < 0 ){ iLen = 0; }
	}
	if( iStart + iLen > (sxi64)nCp ){
		iLen = (sxi64)nCp - iStart;
	}
	if( iEnc != MB_ENC_UTF8 ){
		/* One byte per character: the slice is the byte range, handed back as it
		 * stands (php substitutes nothing here — mb_substr("\xff",0,1,"ASCII")
		 * is the raw byte, error character or not). */
		ph7_result_string(pCtx,&zIn[iStart],(int)iLen);
		return PH7_OK;
	}
	iOfft = MbUtf8Skip(zIn,(sxu32)nByte,(sxu32)iStart);
	iEnd  = iOfft + MbUtf8Skip(&zIn[iOfft],(sxu32)nByte - iOfft,(sxu32)iLen);
	/* php decodes and re-encodes the slice rather than copying its bytes, so an
	 * undecodable run inside it comes out as '?' — mb_substr("ab\xffcd",2,1) is
	 * "?", not the raw \xff PHL used to hand back. */
	MbResultSubstituted(pCtx,&zIn[iOfft],iEnd - iOfft);
	return PH7_OK;
}
/* Append one code in iEnc's encoding. A code the target cannot hold — U+0178,
 * which is what upper-casing 0xFF produces, in a one-byte encoding — is php's
 * substitute character, the same '?' an error character gets. */
static void MbAppendCode(SyBlob *pOut,sxu32 cp,int iEnc)
{
	unsigned char zEnc[4];
	if( iEnc == MB_ENC_UTF8 ){
		SyBlobAppend(pOut,zEnc,MbUtf8Encode(cp,zEnc));
		return;
	}
	zEnc[0] = (unsigned char)((cp <= (iEnc == MB_ENC_ASCII ? 0x7Fu : 0xFFu)) ? cp : '?');
	SyBlobAppend(pOut,zEnc,1);
}
/* Shared case transform: iMode 0 = lower, 1 = upper, 2 = title */
static int MbCaseTransform(ph7_context *pCtx,const char *zIn,sxu32 nByte,int iMode,int iEnc)
{
	SyBlob sOut;
	const unsigned char *z = (const unsigned char *)zIn;
	sxu32 i = 0,nLen,cp,mapped;
	int bWordStart = 1;
	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);
	while( i < nByte ){
		sxu32 iCp = MbNextCode(&z[i],nByte - i,iEnc,&nLen);
		i += nLen;
		if( iCp == MB_BAD_CODE ){
			/* php substitutes '?' for an undecodable run. For TITLE mode the run
			 * is neither a word character nor a separator — it leaves the
			 * word-start flag exactly as it found it, so "\xffab" titles to
			 * "?Ab" (the run did not open a word, 'a' still does) while
			 * "a\xffb" titles to "A?b" ('b' is still mid-word). */
			SyBlobAppend(&sOut,"?",1);
			continue;
		}
		cp = iCp;
		if( iMode == 1 ){
			if( cp == 0x00DF ){ /* php: mb_strtoupper('ß') === 'SS' */
				SyBlobAppend(&sOut,"SS",2);
				continue;
			}
			mapped = MbToUpper(cp);
		}else if( iMode == 0 ){
			if( cp == 0x03A3 ){
				/* Greek capital sigma: final position lowers to ς, else σ */
				sxu32 nPeek,iNext = MB_BAD_CODE;
				if( i < nByte ){
					iNext = MbNextCode(&z[i],nByte - i,iEnc,&nPeek);
				}
				mapped = (iNext == MB_BAD_CODE || !MbIsAlnum(iNext) ) ? 0x03C2 : 0x03C3;
			}else{
				mapped = MbToLower(cp);
			}
		}else{
			if( MbIsAlnum(cp) ){
				mapped = bWordStart ? MbToUpper(cp) : MbToLower(cp);
				bWordStart = 0;
			}else{
				mapped = cp;
				bWordStart = 1;
			}
		}
		MbAppendCode(&sOut,mapped,iEnc);
	}
	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));
	SyBlobRelease(&sOut);
	return PH7_OK;
}
/* string mb_strtolower/mb_strtoupper(string $string, ?string $encoding) */
static int PH7_builtin_mb_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn,*zFunc;
	int nByte,iEnc;
	if( nArg < 1 ){
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	zFunc = ph7_function_name(pCtx);
	iEnc = MbEncodingArg(pCtx,nArg > 1 ? apArg[1] : 0,zFunc,2);
	if( iEnc < 0 ){
		return PH7_OK;
	}
	zIn = ph7_value_to_string(apArg[0],&nByte);
	return MbCaseTransform(pCtx,zIn,(sxu32)nByte,
		zFunc[sizeof("mb_strto")-1] == 'u' ? 1 : 0,iEnc); /* mb_strtoUpper */
}
/* string mb_convert_case(string $string, int $mode, ?string $encoding) */
static int PH7_builtin_mb_convert_case(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn;
	int nByte,iMode,iEnc;
	if( nArg < 2 ){
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	iEnc = MbEncodingArg(pCtx,nArg > 2 ? apArg[2] : 0,"mb_convert_case",3);
	if( iEnc < 0 ){
		return PH7_OK;
	}
	iMode = ph7_value_to_int(apArg[1]);
	if( iMode < 0 || iMode > 2 ){
		/* php has FOLD/SIMPLE variants 3-7; PHL's recorded scope is 0-2 */
		return PH7_VmThrowException(pCtx,"ValueError",
			"mb_convert_case(): Argument #2 ($mode) must be one of the MB_CASE_* constants");
	}
	zIn = ph7_value_to_string(apArg[0],&nByte);
	/* php: MB_CASE_UPPER=0, MB_CASE_LOWER=1, MB_CASE_TITLE=2 */
	return MbCaseTransform(pCtx,zIn,(sxu32)nByte,
		iMode == 0 ? 1 : (iMode == 1 ? 0 : 2),iEnc);
}
/* Shared search core: returns the character index of a match, or -1.
 *
 * iOfftCp is the LOWEST character index a match may start at; iMaxCp the highest,
 * or -1 for no upper bound. The pair is php's asymmetric strrpos rule (a negative
 * $offset is an upper bound counted back from the end, a non-negative one a lower
 * bound) — the same split StrRSearchWindow() applies to the 8-bit family. */
static sxi64 MbTextSearch(const mb_text *pH,const mb_text *pN,
	sxi64 iOfftCp,sxi64 iMaxCp,int bExactBad,int bReverse)
{
	sxi64 iFound = -1;
	sxu32 i,iFrom;
	if( pN->nChar == 0 || pN->nChar > pH->nChar ){
		return -1;
	}
	iFrom = (sxu32)(iOfftCp > 0 ? iOfftCp : 0);
	for( i = iFrom ; i + pN->nChar <= pH->nChar ; ++i ){
		if( iMaxCp >= 0 && (sxi64)i > iMaxCp ){
			break; /* past the window's upper bound; nothing later qualifies */
		}
		if( MbTextMatchAt(pH,i,pN,bExactBad) ){
			iFound = (sxi64)i;
			if( !bReverse ){
				break;
			}
			/* keep scanning for the last hit */
		}
	}
	return iFound;
}
/* mb_strpos / mb_stripos / mb_strrpos / mb_strripos */
static int PH7_builtin_mb_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zH,*zN,*zFunc;
	mb_text sH,sN;
	int nH,nN,iEnc,rc;
	sxi64 iOfft = 0,iMax = -1,iPos,nCp;
	int bFold,bRev;
	if( nArg < 2 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zFunc = ph7_function_name(pCtx);
	/* "mb_str" + "pos" / "ipos" / "rpos" / "ripos" */
	bRev  = zFunc[sizeof("mb_str")-1] == 'r';
	bFold = zFunc[sizeof("mb_str")-1+(bRev?1:0)] == 'i';
	iEnc = MbEncodingArg(pCtx,nArg > 3 ? apArg[3] : 0,zFunc,4);
	if( iEnc < 0 ){
		return PH7_OK;
	}
	zH = ph7_value_to_string(apArg[0],&nH);
	zN = ph7_value_to_string(apArg[1],&nN);
	nCp = (sxi64)MbStrlen(zH,(sxu32)nH,iEnc);
	if( nArg > 2 ){
		iOfft = ph7_value_to_int64(apArg[2]);
		/* php requires -strlen <= $offset <= strlen, in CODE POINTS here, and
		 * raises rather than answering "not found" — the same rule the 8-bit
		 * family got, which these three were left out of: mb_strpos("abc","c",7)
		 * answered false, and false is what a genuine miss answers too. */
		if( iOfft < 0 ? (iOfft < -nCp) : (iOfft > nCp) ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"%s(): Argument #3 ($offset) must be contained in argument #1 ($haystack)",
				zFunc);
		}
		if( bRev ){
			/* A negative offset is an UPPER bound on where the match may START,
			 * not a start position: mb_strrpos("áéíóú","í",-1) is 2 in php, where
			 * counting it forward answered false — and -5 is php's false, where
			 * counting it forward answered 2. */
			if( iOfft < 0 ){
				iMax = nCp + iOfft;
				iOfft = 0;
			}
		}else if( iOfft < 0 ){
			iOfft = nCp + iOfft;
		}
	}
	if( nN == 0 ){
		/* php 8 matches an EMPTY needle at the offset itself (and, searching
		 * backwards, at the last position the window allows) — `mb_strpos("abc","")`
		 * is 0 and `mb_strrpos("abc","")` is 3. These three answered false, which is
		 * also what a genuine miss answers; the 8-bit family already had the rule. */
		iPos = bRev ? (iMax >= 0 ? iMax : nCp) : iOfft;
		if( iPos > nCp ){
			iPos = nCp;
		}
		if( iPos < iOfft ){
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		ph7_result_int64(pCtx,iPos);
		return PH7_OK;
	}
	rc = MbTextDecode(pCtx,&sH,zH,(sxu32)nH,iEnc,bFold);
	if( rc == PH7_OK ){
		rc = MbTextDecode(pCtx,&sN,zN,(sxu32)nN,iEnc,bFold);
	}
	if( rc != PH7_OK ){
		return rc;   /* the decode already raised; do not raise a second time */
	}
	iPos = MbTextSearch(&sH,&sN,iOfft,iMax,iEnc == MB_ENC_UTF8 && !bFold,bRev);
	if( iPos < 0 ){
		ph7_result_bool(pCtx,0);
	}else{
		ph7_result_int64(pCtx,iPos);
	}
	return PH7_OK;
}
/* array mb_str_split(string $string, int $length = 1, ?string $encoding) */
static int PH7_builtin_mb_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn;
	int nByte,iEnc;
	sxi64 iChunk = 1;
	ph7_value *pArr,*pV;
	sxu32 i;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	iEnc = MbEncodingArg(pCtx,nArg > 2 ? apArg[2] : 0,"mb_str_split",3);
	if( iEnc < 0 ){
		return PH7_OK;
	}
	if( nArg > 1 ){
		iChunk = ph7_value_to_int64(apArg[1]);
		if( iChunk < 1 ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"mb_str_split(): Argument #2 ($length) must be greater than 0");
		}
		if( iChunk >= 0x40000000 ){
			/* php's own ceiling, and the reason it is not just "clamp to the
			 * string": the chunk count is what it allocates for. Without it the
			 * count was TRUNCATED into 32 bits here, so a $length of 2^32+1
			 * split into single characters. */
			return PH7_VmThrowException(pCtx,"ValueError",
				"mb_str_split(): Argument #2 ($length) is too large");
		}
	}
	zIn = ph7_value_to_string(apArg[0],&nByte);
	pArr = ph7_context_new_array(pCtx);
	pV = ph7_context_new_scalar(pCtx);
	if( pArr == 0 || pV == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	for( i = 0 ; i < (sxu32)nByte ; ){
		sxu32 iEnd = i + MbSkip(&zIn[i],(sxu32)nByte - i,(sxu32)iChunk,iEnc);
		ph7_value_string(pV,&zIn[i],(int)(iEnd - i));
		ph7_array_add_elem(pArr,0,pV);
		ph7_value_reset_string_cursor(pV);
		i = iEnd;
	}
	ph7_result_value(pCtx,pArr);
	return PH7_OK;
}
/* --- mb_trim / mb_ltrim / mb_rtrim (php 8.4) --------------------------- */

#define MB_TRIM_LEFT  1
#define MB_TRIM_RIGHT 2

/*
 * The character set a trim walks against. php builds a hash of code points;
 * this splits it in two so the common case costs nothing: a 256-bit map for
 * everything below U+0100 (which is where a hand-written trim set almost
 * always lives) and a linear array for the rest, whose length is the number of
 * DISTINCT high code points in $characters. php's own fast path is a linear
 * scan of up to four, so the shape is not a departure.
 */
typedef struct mb_trim_set mb_trim_set;
struct mb_trim_set {
	unsigned char aLow[32];   /* bitmap of U+0000 .. U+00FF */
	sxu32 *aHigh;             /* the rest, in encounter order */
	sxu32 nHigh;
	sxu32 nAlloc;
};
static int MbTrimSetAdd(ph7_context *pCtx,mb_trim_set *pSet,sxu32 cp)
{
	sxu32 i;
	if( cp < 256 ){
		pSet->aLow[cp >> 3] |= (unsigned char)(1 << (cp & 7));
		return PH7_OK;
	}
	for( i = 0 ; i < pSet->nHigh ; ++i ){
		if( pSet->aHigh[i] == cp ){
			return PH7_OK;
		}
	}
	if( pSet->nHigh >= pSet->nAlloc ){
		sxu32 nNew = pSet->nAlloc ? pSet->nAlloc * 2 : 16;
		sxu32 *aNew = (sxu32 *)ph7_context_alloc_chunk(pCtx,
			(unsigned int)(nNew * sizeof(sxu32)),FALSE,TRUE);
		if( aNew == 0 ){
			return PH7_ContextMemoryError(pCtx);
		}
		if( pSet->nHigh > 0 ){
			SyMemcpy(pSet->aHigh,aNew,pSet->nHigh * (sxu32)sizeof(sxu32));
		}
		pSet->aHigh = aNew;
		pSet->nAlloc = nNew;
	}
	pSet->aHigh[pSet->nHigh++] = cp;
	return PH7_OK;
}
static int MbTrimSetHas(const mb_trim_set *pSet,sxu32 cp)
{
	sxu32 i;
	if( cp < 256 ){
		return (pSet->aLow[cp >> 3] & (1 << (cp & 7))) != 0;
	}
	for( i = 0 ; i < pSet->nHigh ; ++i ){
		if( pSet->aHigh[i] == cp ){
			return 1;
		}
	}
	return 0;
}
/*
 * string mb_trim(string $string, ?string $characters = null, ?string $encoding = null)
 * string mb_ltrim(...) / string mb_rtrim(...)
 *  Strip whole CHARACTERS -- there is no `a..z` range syntax here, unlike
 *  trim() -- from one or both ends, defaulting to php's Unicode whitespace set.
 *  Where a chunk implementation compared the encoded bytes, this decodes: an
 *  ill-formed run is ONE character that compares equal to every other
 *  ill-formed run, which is what makes mb_trim("\xff\xfeab\xff", "\xff")
 *  answer "ab" rather than leaving the bytes it could not read in place.
 */
static int PH7_builtin_mb_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	/* php's trim_default_chars[], in its own order (mb_trim_default_chars()) */
	static const sxu32 aDefault[] = {
		0x20, 0x0C, 0x0A, 0x0D, 0x09, 0x0B, 0x00, 0xA0, 0x1680,
		0x2000, 0x2001, 0x2002, 0x2003, 0x2004, 0x2005, 0x2006, 0x2007,
		0x2008, 0x2009, 0x200A, 0x2028, 0x2029, 0x202F, 0x205F, 0x3000,
		0x85, 0x180E
	};
	const char *zFunc = ph7_function_name(pCtx);
	const char *zIn;
	mb_trim_set sSet;
	int nByte,iEnc,iMode;
	sxu32 i,iLeft,iRight,nLen;
	if( nArg < 1 ){
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* One body, three names: "mb_|l|trim" and "mb_|r|trim" against "mb_|t|rim". */
	iMode = (zFunc[3] == 'l') ? MB_TRIM_LEFT
		: ((zFunc[3] == 'r') ? MB_TRIM_RIGHT : (MB_TRIM_LEFT|MB_TRIM_RIGHT));
	iEnc = MbEncodingArg(pCtx,nArg > 2 ? apArg[2] : 0,zFunc,3);
	if( iEnc < 0 ){
		return PH7_OK;
	}
	SyZero(&sSet,sizeof(sSet));
	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){
		const char *zWhat = ph7_value_to_string(apArg[1],&nByte);
		for( i = 0 ; i < (sxu32)nByte ; i += nLen ){
			/* Every error character decodes to the same member, php's error
			 * marker, so one bad byte in $characters strips them all. */
			sxu32 cp = MbNextCode((const unsigned char *)&zWhat[i],(sxu32)nByte - i,iEnc,&nLen);
			if( MbTrimSetAdd(pCtx,&sSet,cp) != PH7_OK ){
				return PH7_ContextMemoryError(pCtx);
			}
		}
	}else{
		for( i = 0 ; i < SX_ARRAYSIZE(aDefault) ; ++i ){
			if( MbTrimSetAdd(pCtx,&sSet,aDefault[i]) != PH7_OK ){
				return PH7_ContextMemoryError(pCtx);
			}
		}
	}
	zIn = ph7_value_to_string(apArg[0],&nByte);
	iLeft = 0;
	iRight = (sxu32)nByte;
	if( iMode & MB_TRIM_LEFT ){
		while( iLeft < iRight ){
			sxu32 cp = MbNextCode((const unsigned char *)&zIn[iLeft],iRight - iLeft,iEnc,&nLen);
			if( !MbTrimSetHas(&sSet,cp) ){
				break;
			}
			iLeft += nLen;
		}
	}
	if( iMode & MB_TRIM_RIGHT ){
		/* UTF-8 has no backwards reader here, so re-walk from the left edge and
		 * keep the offset where the CURRENT run of trim characters began; the
		 * last one still open when the walk ends is the trailing run. */
		sxu32 iRun = iRight;
		int bInRun = 0;
		for( i = iLeft ; i < iRight ; i += nLen ){
			sxu32 cp = MbNextCode((const unsigned char *)&zIn[i],iRight - i,iEnc,&nLen);
			if( MbTrimSetHas(&sSet,cp) ){
				if( !bInRun ){
					iRun = i;
					bInRun = 1;
				}
			}else{
				bInRun = 0;
			}
		}
		if( bInRun ){
			iRight = iRun;
		}
	}
	if( iEnc != MB_ENC_UTF8 || (iLeft == 0 && iRight == (sxu32)nByte) ){
		/* php hands the ORIGINAL string back when it trimmed nothing
		 * (trim_each_wchar()'s zend_string_copy), so an ill-formed run survives
		 * a no-op trim and is substituted only when something was sliced off. */
		ph7_result_string(pCtx,&zIn[iLeft],(int)(iRight - iLeft));
		return PH7_OK;
	}
	/* What it does keep is decoded and re-encoded, so an ill-formed run left in
	 * the middle comes back as '?' -- the same rule mb_substr() follows. */
	MbResultSubstituted(pCtx,&zIn[iLeft],iRight - iLeft);
	return PH7_OK;
}
/* string|bool mb_internal_encoding(?string $encoding = null) */
static int PH7_builtin_mb_internal_encoding(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	if( nArg < 1 || ph7_value_is_null(apArg[0]) ){
		ph7_result_string(pCtx,"UTF-8",sizeof("UTF-8")-1);
		return PH7_OK;
	}
	if( MbEncodingArg(pCtx,apArg[0],"mb_internal_encoding",1) < 0 ){
		return PH7_OK;
	}
	/* Only the UTF-8 family is accepted, and it is already the default */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/* Is every character of zIn[0..nByte-1] a character of iEnc? LATIN1 answers yes
 * to any buffer at all — every byte is a code point there, which is what makes
 * mb_check_encoding("\xff","8bit") true where the ASCII and UTF-8 answers are
 * false. */
static int MbBufferIsValid(const char *zIn,sxu32 nByte,int iEnc)
{
	const unsigned char *z = (const unsigned char *)zIn;
	sxu32 i = 0,nLen;
	if( iEnc == MB_ENC_LATIN1 ){
		return 1;
	}
	while( i < nByte ){
		if( MbNextCode(&z[i],nByte - i,iEnc,&nLen) == MB_BAD_CODE ){
			return 0;
		}
		i += nLen;
	}
	return 1;
}
/* ph7_array_walk() callback: fold one element's validity into *pbOk. */
static int MbCheckWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)
{
	int *paState = (int *)pUserData;   /* [0] = ok so far, [1] = the encoding */
	const char *zIn;
	int nByte;
	SXUNUSED(pKey);
	if( ph7_value_is_array(pData) ){
		return ph7_array_walk(pData,MbCheckWalker,pUserData);
	}
	zIn = ph7_value_to_string(pData,&nByte);
	if( !MbBufferIsValid(zIn,(sxu32)nByte,paState[1]) ){
		paState[0] = 0;
		return SXERR_ABORT;
	}
	return PH7_OK;
}
/* bool mb_check_encoding(array|string|null $value = null, ?string $encoding = null) */
static int PH7_builtin_mb_check_encoding(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn;
	int nByte,iEnc;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,1);
		return PH7_OK;
	}
	iEnc = MbEncodingArg(pCtx,nArg > 1 ? apArg[1] : 0,"mb_check_encoding",2);
	if( iEnc < 0 ){
		return PH7_OK;
	}
	if( ph7_value_is_array(apArg[0]) ){
		/* php checks EVERY element (the declared type is array|string|null);
		 * stringifying the array checked the six bytes of the word "Array". */
		int aState[2];
		aState[0] = 1;
		aState[1] = iEnc;
		ph7_array_walk(apArg[0],MbCheckWalker,aState);
		ph7_result_bool(pCtx,aState[0]);
		return PH7_OK;
	}
	zIn = ph7_value_to_string(apArg[0],&nByte);
	ph7_result_bool(pCtx,MbBufferIsValid(zIn,(sxu32)nByte,iEnc));
	return PH7_OK;
}
/* php's East Asian wide/fullwidth set: two columns, everything else one. */
static int MbCodeWidth(sxu32 cp)
{
	if( (cp >= 0x1100 && cp <= 0x115F) || (cp >= 0x2E80 && cp <= 0xA4CF)
	 || (cp >= 0xAC00 && cp <= 0xD7A3) || (cp >= 0xF900 && cp <= 0xFAFF)
	 || (cp >= 0xFE30 && cp <= 0xFE4F) || (cp >= 0xFF00 && cp <= 0xFF60)
	 || (cp >= 0xFFE0 && cp <= 0xFFE6) || cp >= 0x20000 ){
		return 2;
	}
	return 1;
}
/* int mb_strwidth(string $string, ?string $encoding = null) */
static int PH7_builtin_mb_strwidth(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const unsigned char *z;
	const char *zIn;
	int nByte,iEnc;
	sxu32 i = 0,nLen,cp;
	ph7_int64 nWidth = 0;
	if( nArg < 1 ){
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	iEnc = MbEncodingArg(pCtx,nArg > 1 ? apArg[1] : 0,"mb_strwidth",2);
	if( iEnc < 0 ){
		return PH7_OK;
	}
	zIn = ph7_value_to_string(apArg[0],&nByte);
	z = (const unsigned char *)zIn;
	while( i < (sxu32)nByte ){
		cp = MbNextCode(&z[i],(sxu32)nByte - i,iEnc,&nLen);
		i += nLen;
		/* An error character stands in for '?': one column */
		if( cp == MB_BAD_CODE ){
			cp = (sxu32)'?';
		}
		nWidth += MbCodeWidth(cp);
	}
	ph7_result_int64(pCtx,nWidth);
	return PH7_OK;
}

/* string|false mb_chr(int $codepoint, ?string $encoding = null) */
static int PH7_builtin_mb_chr(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi64 cp;
	unsigned char zOut[4];
	sxu32 n;
	int iEnc;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	iEnc = MbEncodingArg(pCtx,nArg > 1 ? apArg[1] : 0,"mb_chr",2);
	if( iEnc < 0 ){
		return PH7_OK;
	}
	cp = ph7_value_to_int64(apArg[0]);
	/* php rejects negatives, code points past U+10FFFF and the UTF-16
	 * surrogate range with FALSE — and, in a one-byte encoding, everything the
	 * encoding cannot hold: mb_chr(233,"ASCII") is false where mb_chr(233,"8bit")
	 * is the single byte 0xE9. */
	if( cp < 0 || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)
	 || (iEnc == MB_ENC_LATIN1 && cp > 0xFF) || (iEnc == MB_ENC_ASCII && cp > 0x7F) ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( iEnc != MB_ENC_UTF8 ){
		zOut[0] = (unsigned char)cp;
		ph7_result_string(pCtx,(const char *)zOut,1);
		return PH7_OK;
	}
	n = MbUtf8Encode((sxu32)cp,zOut);
	ph7_result_string(pCtx,(const char *)zOut,(int)n);
	return PH7_OK;
}
/* int|false mb_ord(string $string, ?string $encoding = null) */
static int PH7_builtin_mb_ord(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn;
	const unsigned char *z;
	int nByte,iEnc;
	sxu32 cp,nLen;
	iEnc = MbEncodingArg(pCtx,nArg > 1 ? apArg[1] : 0,"mb_ord",2);
	if( iEnc < 0 ){
		return PH7_OK;
	}
	zIn = ph7_value_to_string(apArg[0],&nByte);
	if( nByte < 1 ){
		/* php throws on an empty string rather than returning FALSE */
		return PH7_VmThrowException(pCtx,"ValueError",
			"mb_ord(): Argument #1 ($string) must not be empty");
	}
	z = (const unsigned char *)zIn;
	/* An error character answers FALSE — a malformed UTF-8 first character
	 * (invalid lead / truncated / bad continuation / over-long / surrogate), or a
	 * byte over 0x7F under ASCII. Under a one-byte encoding whose code point IS
	 * the byte there is no such thing, so mb_ord("\xff","8bit") is 255. */
	cp = MbNextCode(z,(sxu32)nByte,iEnc,&nLen);
	if( cp == MB_BAD_CODE ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_int64(pCtx,(sxi64)cp);
	return PH7_OK;
}
/*
 * mb_detect_encoding(string $string, array|string|null $encodings = null,
 *                    bool $strict = false): string|false
 *
 * PHL's detectable set is ASCII, UTF-8 and ISO-8859-1 (the §10 scope cut).
 * php's default detect order is exactly ASCII,UTF-8, so the null/default path
 * is byte-identical. A candidate encoding php supports but PHL does not (e.g.
 * SJIS) raises the same ValueError php uses for a truly invalid name — a
 * recorded scope divergence, not silent.
 *
 * Two numbers decide it, and both are php's: a candidate that cannot decode the
 * input at all loses to one that can, and among those that can, the one that
 * reads the FEWEST characters wins — which is why "\xc3\xa1" is UTF-8 (one
 * character) rather than the ISO-8859-1 listed ahead of it (two), while a pure
 * ASCII string, where every candidate reads the same count, is the first one
 * named. In strict mode a candidate with any error at all answers false.
 * `8bit`/`binary` are php's non-detectable pair: named, counted, never chosen.
 */
#define MB_DETECT_NEVER  3   /* 8bit / binary: a candidate php never selects */
static int MbAsciiErrors(const unsigned char *z,int n)
{
	int i,e = 0;
	for( i = 0 ; i < n ; i++ ){
		if( z[i] >= 0x80 ){ e++; }
	}
	return e;
}
static int MbUtf8Errors(const unsigned char *z,int n)
{
	sxu32 i = 0,nLen;
	int e = 0;
	while( i < (sxu32)n ){
		if( MbUtf8Decode(&z[i],(sxu32)n - i,&nLen) < 0 ){ e++; i++; }
		else{ i += nLen; }
	}
	return e;
}
/* Map an encoding name to PHL's detectable set: 0 = ASCII, 1 = UTF-8,
 * 2 = ISO-8859-1, MB_DETECT_NEVER = 8bit/binary, -1 = out of scope. Surrounding
 * ASCII whitespace is trimmed (php accepts "ASCII, UTF-8"). */
static int MbDetectEncId(const char *z,int n)
{
	while( n > 0 && (z[0]==' '||z[0]=='\t'||z[0]=='\n'||z[0]=='\r') ){ z++; n--; }
	while( n > 0 && (z[n-1]==' '||z[n-1]=='\t'||z[n-1]=='\n'||z[n-1]=='\r') ){ n--; }
	if( (n == 5 && SyStrnicmp(z,"ASCII",5) == 0)
	 || (n == 8 && SyStrnicmp(z,"US-ASCII",8) == 0) ){
		return 0;
	}
	if( (n == 5 && SyStrnicmp(z,"UTF-8",5) == 0)
	 || (n == 4 && SyStrnicmp(z,"UTF8",4) == 0) ){
		return 1;
	}
	if( (n == 10 && SyStrnicmp(z,"ISO-8859-1",10) == 0)
	 || (n == 9 && SyStrnicmp(z,"ISO8859-1",9) == 0)
	 || (n == 6 && SyStrnicmp(z,"latin1",6) == 0) ){
		return 2;
	}
	if( (n == 4 && SyStrnicmp(z,"8bit",4) == 0)
	 || (n == 6 && SyStrnicmp(z,"binary",6) == 0) ){
		return MB_DETECT_NEVER;
	}
	return -1;
}
/* Per-detection running state, shared by the array walker and the string path. */
typedef struct mb_detect_state mb_detect_state;
struct mb_detect_state {
	ph7_context *pCtx;
	int aErr[3];      /* precomputed [ASCII], [UTF-8], [ISO-8859-1] error counts */
	int aChar[3];     /* and the character count each one reads */
	int iBestEnc;     /* winning encoding id, -1 until the first that can win */
	int iBestErr;     /* its error count */
	int iBestChar;    /* and its character count */
	int nSeen;        /* candidates considered (0 -> "must specify at least one") */
	int bError;       /* an out-of-scope name threw -> abort */
	int rc;           /* the throw's propagation code (PH7_ABORT/PH7_EXCEPTION) */
};
/* Fold one candidate encoding name into the running best. Returns SXERR_ABORT
 * (and throws) when the name is outside PHL's detectable scope. */
static int MbDetectConsider(mb_detect_state *pState,const char *zName,int nName)
{
	int enc = MbDetectEncId(zName,nName);
	if( enc < 0 ){
		pState->bError = 1;
		pState->rc = PH7_VmThrowException(pState->pCtx,"ValueError",
			"mb_detect_encoding(): Argument #2 ($encodings) contains invalid encoding \"%.*s\"",
			nName,zName);
		return SXERR_ABORT;
	}
	pState->nSeen++;
	if( enc == MB_DETECT_NEVER ){
		return PH7_OK;
	}
	if( pState->iBestEnc < 0
	 || pState->aErr[enc] < pState->iBestErr
	 || (pState->aErr[enc] == pState->iBestErr && pState->aChar[enc] < pState->iBestChar) ){
		pState->iBestEnc = enc;
		pState->iBestErr = pState->aErr[enc];
		pState->iBestChar = pState->aChar[enc];
	}
	return PH7_OK;
}
/* ph7_array_walk() callback over the $encodings array. */
static int MbDetectWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)
{
	mb_detect_state *pState = (mb_detect_state *)pUserData;
	const char *zName;
	int nName;
	SXUNUSED(pKey);
	zName = ph7_value_to_string(pData,&nName);
	return MbDetectConsider(pState,zName,nName);
}
static int PH7_builtin_mb_detect_encoding(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn;
	int nByte,bStrict = 0;
	mb_detect_state sState;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zIn = ph7_value_to_string(apArg[0],&nByte);
	if( nArg > 2 ){ bStrict = ph7_value_to_bool(apArg[2]); }
	sState.pCtx = pCtx;
	sState.aErr[0] = MbAsciiErrors((const unsigned char *)zIn,nByte);
	sState.aErr[1] = MbUtf8Errors((const unsigned char *)zIn,nByte);
	sState.aErr[2] = 0;   /* every byte is a character of ISO-8859-1 */
	sState.aChar[0] = nByte;
	sState.aChar[1] = (int)MbStrlen(zIn,(sxu32)nByte,MB_ENC_UTF8);
	sState.aChar[2] = nByte;
	sState.iBestEnc = -1;
	sState.iBestErr = 0;
	sState.iBestChar = 0;
	sState.nSeen = 0;
	sState.bError = 0;
	sState.rc = PH7_OK;
	if( nArg < 2 || ph7_value_is_null(apArg[1]) ){
		/* php's default detect order is exactly ASCII, then UTF-8 */
		MbDetectConsider(&sState,"ASCII",5);
		MbDetectConsider(&sState,"UTF-8",5);
	}else if( ph7_value_is_array(apArg[1]) ){
		if( ph7_array_count(apArg[1]) == 0 ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"mb_detect_encoding(): Argument #2 ($encodings) must specify at least one encoding");
		}
		ph7_array_walk(apArg[1],MbDetectWalker,&sState);
		if( sState.bError ){ return sState.rc; }
	}else{
		/* comma-separated list, e.g. "ASCII, UTF-8" */
		const char *z2;
		int n2,i,iStart = 0;
		z2 = ph7_value_to_string(apArg[1],&n2);
		for( i = 0 ; i <= n2 ; i++ ){
			if( i == n2 || z2[i] == ',' ){
				const char *zTok = &z2[iStart];
				int nTok = i - iStart,t = nTok;
				/* ignore an empty / all-whitespace token */
				while( t > 0 && (zTok[0]==' '||zTok[0]=='\t'||zTok[0]=='\n'||zTok[0]=='\r') ){ zTok++; t--; }
				if( t > 0 && MbDetectConsider(&sState,&z2[iStart],nTok) == SXERR_ABORT ){
					return sState.rc;
				}
				iStart = i + 1;
			}
		}
	}
	if( sState.nSeen == 0 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"mb_detect_encoding(): Argument #2 ($encodings) must specify at least one encoding");
	}
	if( sState.iBestEnc < 0 || (bStrict && sState.iBestErr > 0) ){
		/* nothing but 8bit/binary was named, or the best candidate still had an
		 * error and the caller asked for a strict answer */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( sState.iBestEnc == 2 ){
		ph7_result_string(pCtx,"ISO-8859-1",sizeof("ISO-8859-1")-1);
	}else{
		ph7_result_string(pCtx,sState.iBestEnc == 0 ? "ASCII" : "UTF-8",5);
	}
	return PH7_OK;
}
/*
 * mb_convert_encoding(array|string $string, string $to_encoding,
 *                     array|string|null $from_encoding = null): array|string
 *
 * PHL's encoding scope is UTF-8, the byte encodings (8bit/binary/ASCII) and
 * ISO-8859-1 (the §10 scope cut — php's full encoding zoo is out; a php-valid
 * name PHL does not model, e.g. SJIS, raises the same ValueError php uses for a
 * truly invalid name). ISO-8859-1 is carried because it is the documented
 * replacement path for the removed utf8_encode()/utf8_decode() builtins:
 * mb_convert_encoding($s,'UTF-8','ISO-8859-1') and its inverse. Conversion is
 * codepoint-exact for the modelled encodings; a source byte or codepoint that
 * cannot be represented in the target maps to '?' (0x3F), php's default
 * substitute character.
 */
/* Transcode one byte buffer from idFrom to idTo, appending to pOut. Input that
 * cannot be represented in the target substitutes '?' (0x3F), php's default. */
static void MbConvertBuffer(SyBlob *pOut,const char *zIn,sxu32 nByte,int idFrom,int idTo)
{
	const unsigned char *z = (const unsigned char *)zIn;
	sxu32 i = 0,nLen,cp;
	unsigned char zEnc[4];
	while( i < nByte ){
		if( idFrom == MB_ENC_UTF8 ){
			sxi32 iCp = MbUtf8Decode(&z[i],nByte - i,&nLen);
			cp = (iCp < 0) ? (sxu32)'?' : (sxu32)iCp; /* invalid sequence */
			i += nLen;
		}else{
			cp = z[i];
			i++;
			if( idFrom == MB_ENC_ASCII && cp > 0x7F ){ cp = '?'; }
		}
		if( idTo == MB_ENC_UTF8 ){
			SyBlobAppend(pOut,zEnc,MbUtf8Encode(cp,zEnc));
		}else{
			sxu32 iMax = (idTo == MB_ENC_ASCII) ? 0x7F : 0xFF;
			zEnc[0] = (unsigned char)((cp <= iMax) ? cp : '?');
			SyBlobAppend(pOut,zEnc,1);
		}
	}
}
/* Build a converted copy of pIn as a fresh context value: a string is
 * transcoded; an array is rebuilt element by element (keys preserved, nested
 * arrays recursed) to match php's array form. Returns 0 on allocation failure. */
static ph7_value * MbConvertNew(ph7_context *pCtx,ph7_value *pIn,int idFrom,int idTo)
{
	if( ph7_value_is_array(pIn) ){
		ph7_hashmap *pMap = (ph7_hashmap *)pIn->x.pOther;
		ph7_hashmap_node *pEntry = pMap->pFirst;
		ph7_value *pArr = ph7_context_new_array(pCtx);
		ph7_value sKey;
		sxu32 n;
		if( pArr == 0 ){
			return 0;
		}
		PH7_MemObjInit(pCtx->pVm,&sKey);
		for( n = 0 ; n < pMap->nEntry ; n++ ){
			ph7_value *pData = HashmapExtractNodeValue(pEntry);
			if( pData ){
				ph7_value *pConv = MbConvertNew(pCtx,pData,idFrom,idTo);
				if( pConv ){
					PH7_HashmapExtractNodeKey(pEntry,&sKey);
					ph7_array_add_elem(pArr,&sKey,pConv);
					PH7_MemObjRelease(&sKey);
					ph7_context_release_value(pCtx,pConv);
				}
			}
			pEntry = pEntry->pPrev; /* forward walk (reverse link) */
		}
		return pArr;
	}else{
		SyBlob sOut;
		const char *zIn;
		int nByte;
		ph7_value *pVal = ph7_context_new_scalar(pCtx);
		if( pVal == 0 ){
			return 0;
		}
		zIn = ph7_value_to_string(pIn,&nByte);
		SyBlobInit(&sOut,&pCtx->pVm->sAllocator);
		MbConvertBuffer(&sOut,zIn,(sxu32)nByte,idFrom,idTo);
		ph7_value_string(pVal,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));
		SyBlobRelease(&sOut);
		return pVal;
	}
}
static int PH7_builtin_mb_convert_encoding(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zTo,*zFrom;
	int nTo,nFrom,idTo,idFrom;
	ph7_value *pResult;
	if( nArg < 2 ){
		/* the arity guard fires first; stay defensive */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zTo = ph7_value_to_string(apArg[1],&nTo);
	idTo = MbConvEncId(zTo,nTo);
	if( idTo < 0 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"mb_convert_encoding(): Argument #2 ($to_encoding) must be a valid encoding, \"%.*s\" given",
			nTo,zTo);
	}
	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){
		/* php also accepts an array / comma list here for source detection;
		 * PHL's modelled set makes detection trivial, so a single name is taken
		 * (a list falls out of scope and hits the same loud ValueError). */
		zFrom = ph7_value_to_string(apArg[2],&nFrom);
		idFrom = MbConvEncId(zFrom,nFrom);
		if( idFrom < 0 ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"mb_convert_encoding(): Argument #3 ($from_encoding) contains invalid encoding \"%.*s\"",
				nFrom,zFrom);
		}
	}else{
		/* php falls back to the internal encoding, which PHL fixes at UTF-8 */
		idFrom = MB_ENC_UTF8;
	}
	pResult = MbConvertNew(pCtx,apArg[0],idFrom,idTo);
	if( pResult == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	ph7_result_value(pCtx,pResult);
	return PH7_OK;
}
/*
 * Install the mb_* functions (called from PH7_RegisterBuiltInFunction's
 * table in builtin.c via these PH7_PRIVATE symbols).
 */
PH7_PRIVATE int PH7_builtin_mb_strlen_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_strlen(pCtx,nArg,apArg); }
PH7_PRIVATE int PH7_builtin_mb_substr_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_substr(pCtx,nArg,apArg); }
PH7_PRIVATE int PH7_builtin_mb_case_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_strtolower(pCtx,nArg,apArg); }
PH7_PRIVATE int PH7_builtin_mb_convert_case_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_convert_case(pCtx,nArg,apArg); }
PH7_PRIVATE int PH7_builtin_mb_strpos_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_strpos(pCtx,nArg,apArg); }
PH7_PRIVATE int PH7_builtin_mb_str_split_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_str_split(pCtx,nArg,apArg); }
PH7_PRIVATE int PH7_builtin_mb_trim_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_trim(pCtx,nArg,apArg); }
PH7_PRIVATE int PH7_builtin_mb_internal_encoding_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_internal_encoding(pCtx,nArg,apArg); }
PH7_PRIVATE int PH7_builtin_mb_check_encoding_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_check_encoding(pCtx,nArg,apArg); }
PH7_PRIVATE int PH7_builtin_mb_strwidth_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_strwidth(pCtx,nArg,apArg); }
PH7_PRIVATE int PH7_builtin_mb_chr_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_chr(pCtx,nArg,apArg); }
PH7_PRIVATE int PH7_builtin_mb_ord_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_ord(pCtx,nArg,apArg); }
PH7_PRIVATE int PH7_builtin_mb_detect_encoding_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_detect_encoding(pCtx,nArg,apArg); }
PH7_PRIVATE int PH7_builtin_mb_convert_encoding_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_convert_encoding(pCtx,nArg,apArg); }

#endif /* PH7_DISABLE_BUILTIN_FUNC */
