/**
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
/*
 * Section:
 *    php's iconv extension: the character-set converter and the string,
 *    encoding-setting and MIME halves built on it.
 * Status:
 *    Stable.
 *
 * php's iconv is a thin shell over the C library's iconv(3), so its contract is
 * really glibc's -- and that is what is reproduced here, over PHL's own
 * converter rather than the platform's, so a Windows build answers what a Linux
 * one does. Three things had to be measured rather than read, because each is
 * the library's behaviour and not php's:
 *
 *   The NAME grammar. Everything before the first '/' is the code-set name,
 *   compared case-insensitively after every character outside
 *   [alnum] `_ - . , :` is DROPPED -- so `" ASCII "`, `"A SCII"` and `"ASCII\t"`
 *   all name ASCII. The segment between the first and second '/' must be empty;
 *   what follows is a list of error-handler tokens split on '/' and ',', and
 *   `TRANSLIT` anywhere in that list turns transliteration on. That is why
 *   `ASCII//TRANSLITX` is not transliteration and `ASCII//A/B/TRANSLIT` is.
 *
 *   //IGNORE is mostly NOT the library's. The library has one -- it drops a
 *   character the target cannot hold and carries on -- but it then reports the
 *   whole call as an illegal sequence anyway, so on its own it only turns one
 *   failure into another: `iconv("UTF-8","ASCII//IGNORE,X","a\u{4e2d}b")` is
 *   FALSE. What makes //IGNORE useful is php's own, in `_php_check_ignore()`,
 *   and that check is a case-SENSITIVE SUFFIX test on the TO charset only, for
 *   `//IGNORE` or `//IGNORE//TRANSLIT`, with a length guard that makes the bare
 *   string `"//IGNORE"` fail it. So `ASCII//ignore` ignores nothing,
 *   `UTF-8//IGNORE` as the FROM charset ignores nothing, and what php's does is
 *   byte-granular: on an illegal sequence it advances the input by ONE BYTE and
 *   converts again, and a failure with a single byte left is taken for success.
 *
 *   The UTF-8 accepted here is the ORIGINAL six-byte one, not Unicode's
 *   four-byte cut: `\xFC\x84\x80\x80\x80\x80` (U+4000000) converts, while
 *   overlong forms, the surrogate range and the lead bytes C0/C1/FE/FF do not.
 *   The difference between the two diagnostics is only WHERE the input ran out:
 *   a truncated but so-far-valid sequence at the END of the string is
 *   `Detected an incomplete multibyte character`, and everything else is
 *   `Detected an illegal character`.
 *
 * The transliteration table is glibc's own, swept out of it code point by code
 * point (3425 rows, 967 of them the EMPTY replacement that makes a combining
 * mark disappear, plus 55 whose ISO-8859-1 answer differs from their ASCII
 * one). A code point with no row is `?`. PHL models UTF-8, ISO-8859-1 and
 * US-ASCII only (§10's scope cut, the same one mb_ carries); a php-valid name
 * outside those three gets php's own "Wrong encoding" warning, which is what
 * php answers for a name the platform's iconv does not have either.
 *
 * Verified differentially against php 8.5 over 16000 randomized conversions
 * (every encoding pair, every suffix spelling, well-formed and malformed
 * input). The one recorded divergence is in §7.4: an IGNORE token that is NOT
 * php's suffix (`ASCII//TRANSLIT,IGNORE`, `ASCII//ignore`) reaches the
 * library's error handler, whose skip-and-continue behaviour on MALFORMED input
 * is neither documented nor stable across iconv implementations; PHL answers
 * php's own suffix rule there, so those spellings fail where php sometimes
 * recovers. Well-formed input agrees for every spelling.
 */
#ifndef PH7_DISABLE_BUILTIN_FUNC

/* --- Encodings --------------------------------------------------------- */

#define ICV_UTF8    0
#define ICV_LATIN1  1
#define ICV_ASCII   2

/*
 * php's own cap on an encoding NAME, checked before the name is looked at:
 * ICONV_CSNMAXLEN is 64 and the test is `>=`, so a 64-character name is already
 * "exceeds the maximum allowed length of 64 characters".
 */
#define ICV_CSNMAXLEN 64

/*
 * The names glibc registers for the three code sets PHL models, minus its
 * numeric OSF/IBM aliases (`OSF00010020`, `IBM903`…), which are recorded as
 * refused rather than implemented. Compared after ICV normalisation, so the
 * spelling here is the canonical upper-case one.
 */
static const struct IcvEncName {
	const char *zName;
	int iEnc;
} aIcvEncName[] = {
	{ "UTF-8",            ICV_UTF8   },
	{ "UTF8",             ICV_UTF8   },
	{ "ISO-IR-193",       ICV_UTF8   },
	{ "ISO-8859-1",       ICV_LATIN1 },
	{ "ISO_8859-1",       ICV_LATIN1 },
	{ "ISO8859-1",        ICV_LATIN1 },
	{ "ISO88591",         ICV_LATIN1 },
	{ "ISO_8859-1:1987",  ICV_LATIN1 },
	{ "ISO-IR-100",       ICV_LATIN1 },
	{ "LATIN1",           ICV_LATIN1 },
	{ "L1",               ICV_LATIN1 },
	{ "CP819",            ICV_LATIN1 },
	{ "IBM819",           ICV_LATIN1 },
	{ "CSISOLATIN1",      ICV_LATIN1 },
	{ "8859_1",           ICV_LATIN1 },
	{ "ASCII",            ICV_ASCII  },
	{ "US-ASCII",         ICV_ASCII  },
	{ "US",               ICV_ASCII  },
	{ "ANSI_X3.4-1968",   ICV_ASCII  },
	{ "ANSI_X3.4-1986",   ICV_ASCII  },
	{ "ANSI_X3.4",        ICV_ASCII  },
	{ "ISO646-US",        ICV_ASCII  },
	{ "ISO_646.IRV:1991", ICV_ASCII  },
	{ "ISO-IR-6",         ICV_ASCII  },
	{ "CP367",            ICV_ASCII  },
	{ "IBM367",           ICV_ASCII  },
	{ "CSASCII",          ICV_ASCII  }
};

/* Is c one of the characters the name normaliser KEEPS? */
static int IcvNameChar(int c)
{
	return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
		|| c == '_' || c == '-' || c == '.' || c == ',' || c == ':';
}
static int IcvUpper(int c)
{
	return (c >= 'a' && c <= 'z') ? c - 'a' + 'A' : c;
}

/* What a parsed charset argument came to. */
typedef struct icv_cs icv_cs;
struct icv_cs {
	int iEnc;        /* ICV_* id, or -1 when the name is not one PHL models */
	int bTranslit;   /* a TRANSLIT token appeared in the error-handler list */
	int bIgnore;     /* an IGNORE token did -- the LIBRARY's ignore, not php's */
};

/*
 * How much of a charset argument the converter can SEE. php's ZPP hands the
 * whole php string down, but iconv_open() takes a C string, so a name stops at
 * its first NUL -- and so does the "Wrong encoding" message, which prints the
 * same pointer with %s.
 */
static int IcvNameLen(const char *z,int n)
{
	int i;
	for( i = 0 ; i < n && z[i] != 0 ; ++i ){}
	return i;
}
/* Copy z[0..n-1] into zOut, keeping only the characters the normaliser keeps
 * and upper-casing them. Answers the length written, capped at nOut. */
static int IcvNormalize(char *zOut,int nOut,const char *z,int n)
{
	int i,k = 0;
	for( i = 0 ; i < n ; ++i ){
		int c = (unsigned char)z[i];
		if( IcvNameChar(c) && k < nOut ){
			zOut[k++] = (char)IcvUpper(c);
		}
	}
	return k;
}
/*
 * Parse one charset argument. The code-set name is z[0..] up to the first '/',
 * normalised; the segment up to the second '/' must be empty; every remaining
 * '/'- or ','-separated token is an error handler, and `TRANSLIT` is the one
 * that means anything here.
 */
static void IcvParseCharset(const char *z,int n,icv_cs *pCs)
{
	char zBuf[ICV_CSNMAXLEN];
	int iFirst,iSecond,nBuf,k;
	pCs->iEnc = -1;
	pCs->bTranslit = 0;
	pCs->bIgnore = 0;
	n = IcvNameLen(z,n);
	for( iFirst = 0 ; iFirst < n && z[iFirst] != '/' ; ++iFirst ){}
	nBuf = IcvNormalize(zBuf,(int)sizeof(zBuf),z,iFirst);
	if( nBuf == 0 ){
		/* php hands the name straight to iconv_open(), where an empty CODE SET
		 * means the LOCALE's charset -- which is why `"//IGNORE"` on its own
		 * names one. PHL has no locale and one charset it is written in, so the
		 * empty name is UTF-8 here: what php answers on any UTF-8 locale,
		 * deterministically rather than by environment. */
		pCs->iEnc = ICV_UTF8;
	}
	for( k = 0 ; nBuf > 0 && k < (int)SX_ARRAYSIZE(aIcvEncName) ; ++k ){
		const char *zCand = aIcvEncName[k].zName;
		int nCand = (int)SyStrlen(zCand);
		if( nCand == nBuf && SyMemcmp(zCand,zBuf,(sxu32)nCand) == 0 ){
			pCs->iEnc = aIcvEncName[k].iEnc;
			break;
		}
	}
	if( iFirst >= n ){
		return;
	}
	/* Between the first and second '/' is the segment that carries no handler,
	 * and it has to be empty: `ASCII/x/TRANSLIT` names nothing at all, which is
	 * why php answers "Wrong encoding" for it while `ASCII/ /TRANSLIT` -- whose
	 * space the normaliser drops -- converts. */
	for( iSecond = iFirst + 1 ; iSecond < n && z[iSecond] != '/' ; ++iSecond ){}
	if( IcvNormalize(zBuf,(int)sizeof(zBuf),&z[iFirst+1],iSecond - iFirst - 1) > 0 ){
		pCs->iEnc = -1;
	}
	/* Everything past it is a '/'- or ','-separated list of error handlers. */
	nBuf = 0;
	for( k = iSecond + 1 ; k <= n ; ++k ){
		int c = (k < n) ? (unsigned char)z[k] : '/';
		if( c == '/' || c == ',' ){
			if( nBuf == 8 && SyMemcmp(zBuf,"TRANSLIT",8) == 0 ){
				pCs->bTranslit = 1;
			}
			if( nBuf == 6 && SyMemcmp(zBuf,"IGNORE",6) == 0 ){
				pCs->bIgnore = 1;
			}
			nBuf = 0;
			continue;
		}
		if( IcvNameChar(c) ){
			if( nBuf < (int)sizeof(zBuf) ){
				zBuf[nBuf++] = (char)IcvUpper(c);
			}else{
				nBuf = (int)sizeof(zBuf) + 1;
			}
		}
	}
}

/*
 * php's `_php_check_ignore()`: a case-SENSITIVE suffix test on the RAW name,
 * with the length guard that keeps the bare "//IGNORE" from matching.
 */
static int IcvCheckIgnore(const char *z,int n)
{
	n = IcvNameLen(z,n);
	if( n >= 9 && SyMemcmp(&z[n-8],"//IGNORE",8) == 0 ){
		return 1;
	}
	if( n >= 19 && SyMemcmp(&z[n-18],"//IGNORE//TRANSLIT",18) == 0 ){
		return 1;
	}
	return 0;
}

/* --- Transliteration --------------------------------------------------- */

/*
 * One transliteration row: the source code point and the offset/length of its
 * replacement in the shared blob beside it. glibc's table gives an ORDERED list
 * of candidate replacements and takes the first that fits the target, which for
 * the two targets PHL can miss with comes to exactly two columns: the general
 * (ASCII) answer here, and the 55 rows whose ISO-8859-1 answer is a different
 * string.
 */
typedef struct icv_translit icv_translit;
struct icv_translit {
	sxu32 cp;
	sxu16 iOfft;
	sxu16 nByte;
};
#include "builtin_iconv_translit.h"

/* Binary-search a cp-sorted transliteration table. */
static const icv_translit * IcvTranslitFind(const icv_translit *aTab,int nTab,sxu32 cp)
{
	int lo = 0,hi = nTab - 1;
	while( lo <= hi ){
		int mid = lo + (hi - lo) / 2;
		if( aTab[mid].cp == cp ){
			return &aTab[mid];
		}
		if( aTab[mid].cp < cp ){
			lo = mid + 1;
		}else{
			hi = mid - 1;
		}
	}
	return 0;
}

/* --- The converter ----------------------------------------------------- */

/* php's php_iconv_err_t, in php's own order of preference. */
#define ICV_OK            0
#define ICV_ILLEGAL_CHAR  1   /* an incomplete sequence at the end of the input */
#define ICV_ILLEGAL_SEQ   2   /* anything else the source encoding refuses */
#define ICV_WRONG_CHARSET 3
#define ICV_TOO_BIG       4
#define ICV_MALFORMED     5
#define ICV_OUT_BY_BOUNDS 6

/*
 * Decode the character at z[0..n-1] under iEnc. Answers its byte length with
 * *pCp set to the code point, or 0 with *pErr set to the diagnostic the
 * sequence earns. The UTF-8 accepted is the original six-byte encoding, minus
 * overlongs, the surrogate range and the C0/C1/FE/FF lead bytes -- glibc's set,
 * not Unicode's.
 */
static int IcvDecode(const unsigned char *z,int n,int iEnc,sxu32 *pCp,int *pErr)
{
	static const struct { unsigned char iLow,iHigh; int nSeq; sxu32 iMin; } aLead[] = {
		{ 0xC2,0xDF,2,0x80       },
		{ 0xE0,0xEF,3,0x800      },
		{ 0xF0,0xF7,4,0x10000    },
		{ 0xF8,0xFB,5,0x200000   },
		{ 0xFC,0xFD,6,0x4000000  }
	};
	unsigned int c = z[0];
	int i,k;
	if( iEnc == ICV_LATIN1 ){
		*pCp = c;
		return 1;
	}
	if( iEnc == ICV_ASCII ){
		if( c > 0x7F ){
			*pErr = ICV_ILLEGAL_SEQ;
			return 0;
		}
		*pCp = c;
		return 1;
	}
	if( c < 0x80 ){
		*pCp = c;
		return 1;
	}
	for( k = 0 ; k < (int)SX_ARRAYSIZE(aLead) ; ++k ){
		int nSeq;
		sxu32 cp;
		if( c < aLead[k].iLow || c > aLead[k].iHigh ){
			continue;
		}
		nSeq = aLead[k].nSeq;
		cp = c & (sxu32)(0x7F >> nSeq);
		for( i = 1 ; i < nSeq ; ++i ){
			if( i >= n ){
				/* Ran out mid-sequence with everything so far valid: php's
				 * "incomplete multibyte character", not its "illegal" one. */
				*pErr = ICV_ILLEGAL_CHAR;
				return 0;
			}
			if( (z[i] & 0xC0) != 0x80 ){
				*pErr = ICV_ILLEGAL_SEQ;
				return 0;
			}
			cp = (cp << 6) | (sxu32)(z[i] & 0x3F);
		}
		if( cp < aLead[k].iMin || (cp >= 0xD800 && cp <= 0xDFFF) ){
			*pErr = ICV_ILLEGAL_SEQ;   /* overlong, or a lone surrogate */
			return 0;
		}
		*pCp = cp;
		return nSeq;
	}
	*pErr = ICV_ILLEGAL_SEQ;
	return 0;
}

/* Write cp in iEnc when the encoding can hold it; 0 when it cannot. */
static int IcvEncodeDirect(SyBlob *pOut,sxu32 cp,int iEnc)
{
	unsigned char zEnc[6];
	int n = 0;
	if( iEnc == ICV_ASCII ){
		if( cp > 0x7F ){
			return 0;
		}
	}else if( iEnc == ICV_LATIN1 ){
		if( cp > 0xFF ){
			return 0;
		}
	}else{
		/* The six-byte encoding, so every code point IcvDecode() produced can
		 * be written back. */
		if( cp < 0x80 ){
			zEnc[n++] = (unsigned char)cp;
		}else if( cp < 0x800 ){
			zEnc[n++] = (unsigned char)(0xC0 | (cp >> 6));
			zEnc[n++] = (unsigned char)(0x80 | (cp & 0x3F));
		}else if( cp < 0x10000 ){
			zEnc[n++] = (unsigned char)(0xE0 | (cp >> 12));
			zEnc[n++] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
			zEnc[n++] = (unsigned char)(0x80 | (cp & 0x3F));
		}else if( cp < 0x200000 ){
			zEnc[n++] = (unsigned char)(0xF0 | (cp >> 18));
			zEnc[n++] = (unsigned char)(0x80 | ((cp >> 12) & 0x3F));
			zEnc[n++] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
			zEnc[n++] = (unsigned char)(0x80 | (cp & 0x3F));
		}else if( cp < 0x4000000 ){
			zEnc[n++] = (unsigned char)(0xF8 | (cp >> 24));
			zEnc[n++] = (unsigned char)(0x80 | ((cp >> 18) & 0x3F));
			zEnc[n++] = (unsigned char)(0x80 | ((cp >> 12) & 0x3F));
			zEnc[n++] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
			zEnc[n++] = (unsigned char)(0x80 | (cp & 0x3F));
		}else{
			zEnc[n++] = (unsigned char)(0xFC | (cp >> 30));
			zEnc[n++] = (unsigned char)(0x80 | ((cp >> 24) & 0x3F));
			zEnc[n++] = (unsigned char)(0x80 | ((cp >> 18) & 0x3F));
			zEnc[n++] = (unsigned char)(0x80 | ((cp >> 12) & 0x3F));
			zEnc[n++] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
			zEnc[n++] = (unsigned char)(0x80 | (cp & 0x3F));
		}
		SyBlobAppend(pOut,zEnc,(sxu32)n);
		return 1;
	}
	zEnc[0] = (unsigned char)cp;
	SyBlobAppend(pOut,zEnc,1);
	return 1;
}

/*
 * Write cp in iEnc, transliterating when bTranslit is set. Answers 0 only when
 * the encoding cannot hold cp and transliteration is off -- the '?' a
 * transliterated miss produces is glibc's own answer, and it is a SUCCESS, as
 * is the EMPTY replacement a combining mark transliterates to.
 */
static int IcvEncode(SyBlob *pOut,sxu32 cp,int iEnc,int bTranslit)
{
	const icv_translit *pRow;
	if( IcvEncodeDirect(pOut,cp,iEnc) ){
		return 1;
	}
	if( !bTranslit ){
		return 0;
	}
	if( iEnc == ICV_LATIN1
	 && (pRow = IcvTranslitFind(aIcvTranslit1,(int)SX_ARRAYSIZE(aIcvTranslit1),cp)) != 0 ){
		SyBlobAppend(pOut,&zIcvTranslit1[pRow->iOfft],pRow->nByte);
		return 1;
	}
	pRow = IcvTranslitFind(aIcvTranslit,(int)SX_ARRAYSIZE(aIcvTranslit),cp);
	if( pRow ){
		SyBlobAppend(pOut,&zIcvTranslit[pRow->iOfft],pRow->nByte);
		return 1;
	}
	SyBlobAppend(pOut,"?",1);
	return 1;
}

/*
 * One iconv(3) CALL over zIn[*pi..nIn-1]: convert until the input runs out or
 * the library would stop, leaving *pi where it stopped and answering the status
 * php's loop then reads.
 *
 * There are TWO ignore mechanisms and they are not the same one. The LIBRARY's
 * (`pTo->bIgnore`, an IGNORE token anywhere in the error-handler list) drops a
 * character the TARGET cannot hold and carries on, and reports the whole call
 * as an illegal sequence AFTERWARDS -- so on its own it only ever turns one
 * failure into another, which is why `iconv("UTF-8","ASCII//IGNORE,X",…)` is
 * still FALSE. php's own (`bIgnore` in IcvConvert()) is what makes //IGNORE
 * useful, and it lives one level up.
 *
 * *pbDropped is that afterwards, and it is STICKY across the calls php makes:
 * once a character has been dropped because the target could not hold it, a
 * TRUNCATED tail is reported as an illegal sequence rather than an incomplete
 * character -- which is what lets
 * `iconv("UTF-8","ISO-8859-1//IGNORE","\xE1\x88\xA2ab\xFD")` answer "ab"
 * while the same string without the unconvertible character in front of it is a
 * hard failure.
 */
static int IcvConvertCall(SyBlob *pOut,const char *zIn,int nIn,int *pi,
	int iFrom,const icv_cs *pTo,int *pbDropped)
{
	const unsigned char *z = (const unsigned char *)zIn;
	int i = *pi,rc = ICV_OK;
	while( i < nIn ){
		sxu32 cp = 0;
		int err = ICV_OK;
		int nSeq = IcvDecode(&z[i],nIn - i,iFrom,&cp,&err);
		if( nSeq == 0 ){
			rc = err;
			break;
		}
		if( IcvEncode(pOut,cp,pTo->iEnc,pTo->bTranslit) ){
			i += nSeq;
			continue;
		}
		if( pTo->bIgnore ){
			i += nSeq;
			*pbDropped = 1;
			continue;
		}
		rc = ICV_ILLEGAL_SEQ;
		break;
	}
	if( *pbDropped && (rc == ICV_OK || rc == ICV_ILLEGAL_CHAR) ){
		rc = ICV_ILLEGAL_SEQ;
	}
	*pi = i;
	return rc;
}

/*
 * The whole of php_iconv_string(): drive IcvConvertCall() the way php's loop
 * drives iconv(3). php's own `//IGNORE` handling sits HERE and is byte-granular
 * -- on an illegal sequence it steps the input forward by one byte and converts
 * again, and a failure with a single byte left is taken for a success.
 */
static int IcvConvert(SyBlob *pOut,const char *zIn,int nIn,int iFrom,const icv_cs *pTo,int bIgnore)
{
	int i = 0,bDropped = 0;
	for(;;){
		int rc = IcvConvertCall(pOut,zIn,nIn,&i,iFrom,pTo,&bDropped);
		if( rc == ICV_OK ){
			return ICV_OK;
		}
		if( bIgnore && rc == ICV_ILLEGAL_SEQ ){
			if( nIn - i <= 1 ){
				return ICV_OK;
			}
			i++;
			continue;
		}
		return rc;
	}
}

/* --- Diagnostics ------------------------------------------------------- */

/*
 * php's `_php_iconv_show_error()`. The context prefixes the function name, so
 * these are the message bodies only; the two decoder diagnostics are E_NOTICE
 * and everything else E_WARNING, which is php's own split.
 */
static void IcvShowError(ph7_context *pCtx,int err,const char *zTo,int nTo,
	const char *zFrom,int nFrom)
{
	switch( err ){
	case ICV_OK:
		break;
	case ICV_WRONG_CHARSET:
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"Wrong encoding, conversion from \"%.*s\" to \"%.*s\" is not allowed",
			nFrom,zFrom,nTo,zTo);
		break;
	case ICV_ILLEGAL_CHAR:
		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,
			"Detected an incomplete multibyte character in input string");
		break;
	case ICV_ILLEGAL_SEQ:
		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,
			"Detected an illegal character in input string");
		break;
	case ICV_TOO_BIG:
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Buffer length exceeded");
		break;
	case ICV_MALFORMED:
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Malformed string");
		break;
	default:
		break;
	}
}

/*
 * Read one charset ARGUMENT: php's length cap first (it is checked before the
 * name is looked at, so an over-long name never reaches the "Wrong encoding"
 * message), then the grammar. Answers 0 after raising the length warning.
 */
static int IcvCharsetArg(ph7_context *pCtx,const char *z,int n,icv_cs *pCs)
{
	if( n >= ICV_CSNMAXLEN ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"Encoding parameter exceeds the maximum allowed length of %d characters",
			ICV_CSNMAXLEN);
		return 0;
	}
	IcvParseCharset(z,n,pCs);
	return 1;
}

/* --- The functions ----------------------------------------------------- */

/* string|false iconv(string $from_encoding, string $to_encoding, string $string) */
static int PH7_builtin_iconv(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zFrom,*zTo,*zIn;
	int nFrom,nTo,nIn,err;
	icv_cs sFrom,sTo;
	SyBlob sOut;
	if( nArg < 3 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zFrom = ph7_value_to_string(apArg[0],&nFrom);
	zTo = ph7_value_to_string(apArg[1],&nTo);
	zIn = ph7_value_to_string(apArg[2],&nIn);
	if( !IcvCharsetArg(pCtx,zFrom,nFrom,&sFrom) || !IcvCharsetArg(pCtx,zTo,nTo,&sTo) ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( sFrom.iEnc < 0 || sTo.iEnc < 0 ){
		IcvShowError(pCtx,ICV_WRONG_CHARSET,
			zTo,IcvNameLen(zTo,nTo),zFrom,IcvNameLen(zFrom,nFrom));
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);
	err = IcvConvert(&sOut,zIn,nIn,sFrom.iEnc,&sTo,IcvCheckIgnore(zTo,nTo));
	if( err != ICV_OK ){
		SyBlobRelease(&sOut);
		IcvShowError(pCtx,err,zTo,nTo,zFrom,nFrom);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));
	SyBlobRelease(&sOut);
	return PH7_OK;
}

PH7_PRIVATE int PH7_builtin_iconv_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_iconv(pCtx,nArg,apArg); }

#endif /* PH7_DISABLE_BUILTIN_FUNC */
