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

/* Decode the codepoint at z (n bytes available); *pLen = sequence length.
 * Invalid lead bytes decode as themselves with length 1 (byte-transparent,
 * so malformed input degrades instead of exploding). */
static sxu32 MbUtf8Decode(const unsigned char *z,sxu32 n,sxu32 *pLen)
{
	sxu32 c = z[0];
	if( c < 0x80 ){
		*pLen = 1;
		return c;
	}
	if( (c & 0xE0) == 0xC0 && n >= 2 && (z[1] & 0xC0) == 0x80 ){
		*pLen = 2;
		return ((c & 0x1F) << 6) | (z[1] & 0x3F);
	}
	if( (c & 0xF0) == 0xE0 && n >= 3 && (z[1] & 0xC0) == 0x80 && (z[2] & 0xC0) == 0x80 ){
		*pLen = 3;
		return ((c & 0x0F) << 12) | ((z[1] & 0x3F) << 6) | (z[2] & 0x3F);
	}
	if( (c & 0xF8) == 0xF0 && n >= 4 && (z[1] & 0xC0) == 0x80 && (z[2] & 0xC0) == 0x80
	 && (z[3] & 0xC0) == 0x80 ){
		*pLen = 4;
		return ((c & 0x07) << 18) | ((z[1] & 0x3F) << 12) | ((z[2] & 0x3F) << 6) | (z[3] & 0x3F);
	}
	*pLen = 1;
	return c;
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
/* Codepoint count of a UTF-8 buffer */
static sxu32 MbUtf8Strlen(const char *zIn,sxu32 nByte)
{
	const unsigned char *z = (const unsigned char *)zIn;
	sxu32 i = 0,nCp = 0,nLen;
	while( i < nByte ){
		MbUtf8Decode(&z[i],nByte - i,&nLen);
		i += nLen;
		nCp++;
	}
	return nCp;
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

/* --- Shared argument handling ----------------------------------------- */

/* Validate the optional $encoding argument: UTF-8 aliases → 0, "8bit"-style
 * byte encodings → 1, anything else raises php's ValueError and returns -1.
 * (php accepts dozens of encodings; PHL's recorded scope is UTF-8 — a
 * php-VALID encoding like SJIS gets the same loud ValueError.) */
static int MbEncodingArg(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgNo)
{
	const char *zEnc;
	int nEnc;
	if( pArg == 0 || ph7_value_is_null(pArg) ){
		return 0;
	}
	zEnc = ph7_value_to_string(pArg,&nEnc);
	if( (nEnc == 5 && SyStrnicmp(zEnc,"UTF-8",5) == 0)
	 || (nEnc == 4 && SyStrnicmp(zEnc,"UTF8",4) == 0) ){
		return 0;
	}
	if( (nEnc == 4 && SyStrnicmp(zEnc,"8bit",4) == 0)
	 || (nEnc == 5 && SyStrnicmp(zEnc,"ASCII",5) == 0)
	 || (nEnc == 6 && SyStrnicmp(zEnc,"binary",6) == 0) ){
		return 1;
	}
	PH7_VmThrowException(pCtx,"ValueError",
		"%s(): Argument #%d ($encoding) must be a valid encoding, \"%.*s\" given",
		zFunc,iArgNo,nEnc,zEnc);
	return -1;
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
	ph7_result_int64(pCtx,iEnc == 1 ? (ph7_int64)nByte
		: (ph7_int64)MbUtf8Strlen(zIn,(sxu32)nByte));
	return PH7_OK;
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
	nCp = (iEnc == 1) ? (sxu32)nByte : MbUtf8Strlen(zIn,(sxu32)nByte);
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
	if( iEnc == 1 ){
		ph7_result_string(pCtx,&zIn[iStart],(int)iLen);
		return PH7_OK;
	}
	iOfft = MbUtf8Skip(zIn,(sxu32)nByte,(sxu32)iStart);
	iEnd  = iOfft + MbUtf8Skip(&zIn[iOfft],(sxu32)nByte - iOfft,(sxu32)iLen);
	ph7_result_string(pCtx,&zIn[iOfft],(int)(iEnd - iOfft));
	return PH7_OK;
}
/* Shared case transform: iMode 0 = lower, 1 = upper, 2 = title */
static int MbCaseTransform(ph7_context *pCtx,const char *zIn,sxu32 nByte,int iMode)
{
	SyBlob sOut;
	const unsigned char *z = (const unsigned char *)zIn;
	sxu32 i = 0,nLen,cp,mapped;
	unsigned char zEnc[4];
	int bWordStart = 1;
	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);
	while( i < nByte ){
		cp = MbUtf8Decode(&z[i],nByte - i,&nLen);
		i += nLen;
		if( iMode == 1 ){
			if( cp == 0x00DF ){ /* php: mb_strtoupper('ß') === 'SS' */
				SyBlobAppend(&sOut,"SS",2);
				continue;
			}
			mapped = MbToUpper(cp);
		}else if( iMode == 0 ){
			if( cp == 0x03A3 ){
				/* Greek capital sigma: final position lowers to ς, else σ */
				sxu32 nPeek,cpNext = 0;
				if( i < nByte ){
					cpNext = MbUtf8Decode(&z[i],nByte - i,&nPeek);
				}
				mapped = (i >= nByte || !MbIsAlnum(cpNext) ) ? 0x03C2 : 0x03C3;
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
		SyBlobAppend(&sOut,zEnc,MbUtf8Encode(mapped,zEnc));
	}
	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));
	SyBlobRelease(&sOut);
	return PH7_OK;
}
/* string mb_strtolower/mb_strtoupper(string $string, ?string $encoding) */
static int PH7_builtin_mb_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn,*zFunc;
	int nByte;
	if( nArg < 1 ){
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	zFunc = ph7_function_name(pCtx);
	if( MbEncodingArg(pCtx,nArg > 1 ? apArg[1] : 0,zFunc,2) < 0 ){
		return PH7_OK;
	}
	zIn = ph7_value_to_string(apArg[0],&nByte);
	return MbCaseTransform(pCtx,zIn,(sxu32)nByte,
		zFunc[sizeof("mb_strto")-1] == 'u' ? 1 : 0); /* mb_strtoUpper */
}
/* string mb_convert_case(string $string, int $mode, ?string $encoding) */
static int PH7_builtin_mb_convert_case(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn;
	int nByte,iMode;
	if( nArg < 2 ){
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	if( MbEncodingArg(pCtx,nArg > 2 ? apArg[2] : 0,"mb_convert_case",3) < 0 ){
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
		iMode == 0 ? 1 : (iMode == 1 ? 0 : 2));
}
/* Shared search core: returns the codepoint index or -1 */
static sxi64 MbSearch(const char *zH,sxu32 nH,const char *zN,sxu32 nN,
	sxi64 iOfftCp,int bCaseFold,int bReverse,ph7_context *pCtx)
{
	sxu32 iByte,i;
	SyBlob sFh,sFn;
	const char *zHay = zH,*zNee = zN;
	sxi64 iFound = -1;
	if( nN == 0 || nN > nH ){
		return -1;
	}
	if( bCaseFold ){
		/* fold both through the case mapper */
		const unsigned char *z;
		sxu32 k,nLen,cp;
		unsigned char zEnc[4];
		SyBlobInit(&sFh,&pCtx->pVm->sAllocator);
		SyBlobInit(&sFn,&pCtx->pVm->sAllocator);
		z = (const unsigned char *)zH;
		for( k = 0 ; k < nH ; ){
			cp = MbUtf8Decode(&z[k],nH - k,&nLen);
			k += nLen;
			SyBlobAppend(&sFh,zEnc,MbUtf8Encode(MbToLower(cp),zEnc));
		}
		z = (const unsigned char *)zN;
		for( k = 0 ; k < nN ; ){
			cp = MbUtf8Decode(&z[k],nN - k,&nLen);
			k += nLen;
			SyBlobAppend(&sFn,zEnc,MbUtf8Encode(MbToLower(cp),zEnc));
		}
		zHay = (const char *)SyBlobData(&sFh);
		nH = SyBlobLength(&sFh);
		zNee = (const char *)SyBlobData(&sFn);
		nN = SyBlobLength(&sFn);
	}
	iByte = MbUtf8Skip(zHay,nH,(sxu32)(iOfftCp > 0 ? iOfftCp : 0));
	for( i = iByte ; i + nN <= nH ; ){
		if( SyMemcmp(&zHay[i],zNee,nN) == 0 ){
			iFound = (sxi64)MbUtf8Strlen(zHay,i);
			if( !bReverse ){
				break;
			}
			/* keep scanning for the last hit */
		}
		{
			sxu32 nStep;
			MbUtf8Decode((const unsigned char *)&zHay[i],nH - i,&nStep);
			i += nStep;
		}
	}
	if( bCaseFold ){
		SyBlobRelease(&sFh);
		SyBlobRelease(&sFn);
	}
	return iFound;
}
/* mb_strpos / mb_stripos / mb_strrpos */
static int PH7_builtin_mb_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zH,*zN,*zFunc;
	int nH,nN;
	sxi64 iOfft = 0,iPos;
	int bFold,bRev;
	if( nArg < 2 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zFunc = ph7_function_name(pCtx);
	bFold = zFunc[sizeof("mb_str")-1] == 'i';   /* mb_strIpos */
	bRev  = zFunc[sizeof("mb_str")-1] == 'r';   /* mb_strRpos */
	if( MbEncodingArg(pCtx,nArg > 3 ? apArg[3] : 0,zFunc,4) < 0 ){
		return PH7_OK;
	}
	zH = ph7_value_to_string(apArg[0],&nH);
	zN = ph7_value_to_string(apArg[1],&nN);
	if( nArg > 2 ){
		iOfft = ph7_value_to_int64(apArg[2]);
		if( iOfft < 0 ){
			iOfft = (sxi64)MbUtf8Strlen(zH,(sxu32)nH) + iOfft;
			if( iOfft < 0 ){ iOfft = 0; }
		}
	}
	iPos = MbSearch(zH,(sxu32)nH,zN,(sxu32)nN,iOfft,bFold,bRev,pCtx);
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
	int nByte;
	sxi64 iChunk = 1;
	ph7_value *pArr,*pV;
	sxu32 i;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( MbEncodingArg(pCtx,nArg > 2 ? apArg[2] : 0,"mb_str_split",3) < 0 ){
		return PH7_OK;
	}
	if( nArg > 1 ){
		iChunk = ph7_value_to_int64(apArg[1]);
		if( iChunk < 1 ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"mb_str_split(): Argument #2 ($length) must be greater than 0");
		}
	}
	zIn = ph7_value_to_string(apArg[0],&nByte);
	pArr = ph7_context_new_array(pCtx);
	pV = ph7_context_new_scalar(pCtx);
	if( pArr == 0 || pV == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	for( i = 0 ; i < (sxu32)nByte ; ){
		sxu32 iEnd = i + MbUtf8Skip(&zIn[i],(sxu32)nByte - i,(sxu32)iChunk);
		ph7_value_string(pV,&zIn[i],(int)(iEnd - i));
		ph7_array_add_elem(pArr,0,pV);
		ph7_value_reset_string_cursor(pV);
		i = iEnd;
	}
	ph7_result_value(pCtx,pArr);
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
/* bool mb_check_encoding(string $value, ?string $encoding = null) */
static int PH7_builtin_mb_check_encoding(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const unsigned char *z;
	const char *zIn;
	int nByte;
	sxu32 i = 0,nLen,cp;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,1);
		return PH7_OK;
	}
	if( MbEncodingArg(pCtx,nArg > 1 ? apArg[1] : 0,"mb_check_encoding",2) < 0 ){
		return PH7_OK;
	}
	zIn = ph7_value_to_string(apArg[0],&nByte);
	z = (const unsigned char *)zIn;
	while( i < (sxu32)nByte ){
		cp = MbUtf8Decode(&z[i],(sxu32)nByte - i,&nLen);
		if( nLen == 1 && cp >= 0x80 ){
			/* a lead/continuation byte that failed to decode */
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		i += nLen;
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/* int mb_strwidth(string $string, ?string $encoding = null) */
static int PH7_builtin_mb_strwidth(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const unsigned char *z;
	const char *zIn;
	int nByte;
	sxu32 i = 0,nLen,cp;
	ph7_int64 nWidth = 0;
	if( nArg < 1 ){
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	if( MbEncodingArg(pCtx,nArg > 1 ? apArg[1] : 0,"mb_strwidth",2) < 0 ){
		return PH7_OK;
	}
	zIn = ph7_value_to_string(apArg[0],&nByte);
	z = (const unsigned char *)zIn;
	while( i < (sxu32)nByte ){
		cp = MbUtf8Decode(&z[i],(sxu32)nByte - i,&nLen);
		i += nLen;
		/* php's East Asian wide/fullwidth set */
		if( (cp >= 0x1100 && cp <= 0x115F) || (cp >= 0x2E80 && cp <= 0xA4CF)
		 || (cp >= 0xAC00 && cp <= 0xD7A3) || (cp >= 0xF900 && cp <= 0xFAFF)
		 || (cp >= 0xFE30 && cp <= 0xFE4F) || (cp >= 0xFF00 && cp <= 0xFF60)
		 || (cp >= 0xFFE0 && cp <= 0xFFE6) || cp >= 0x20000 ){
			nWidth += 2;
		}else{
			nWidth += 1;
		}
	}
	ph7_result_int64(pCtx,nWidth);
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
PH7_PRIVATE int PH7_builtin_mb_internal_encoding_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_internal_encoding(pCtx,nArg,apArg); }
PH7_PRIVATE int PH7_builtin_mb_check_encoding_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_check_encoding(pCtx,nArg,apArg); }
PH7_PRIVATE int PH7_builtin_mb_strwidth_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_mb_strwidth(pCtx,nArg,apArg); }

#endif /* PH7_DISABLE_BUILTIN_FUNC */
