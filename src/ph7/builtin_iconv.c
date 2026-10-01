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
 * US-ASCII only (the scope cut, the same one mb_ carries); a php-valid name
 * outside those three gets php's own "Wrong encoding" warning, which is what
 * php answers for a name the platform's iconv does not have either.
 *
 * Verified differentially against php 8.5 over 16000 randomized conversions
 * (every encoding pair, every suffix spelling, well-formed and malformed
 * input). The one recorded divergence: an IGNORE token that is NOT
 * php's suffix (`ASCII//TRANSLIT,IGNORE`, `ASCII//ignore`) reaches the
 * library's error handler, whose skip-and-continue behaviour on MALFORMED input
 * is neither documented nor stable across iconv implementations; PHL answers
 * php's own suffix rule there, so those spellings fail where php sometimes
 * recovers. Well-formed input agrees for every spelling.
 */
#ifndef PH7_DISABLE_BUILTIN_FUNC

/* --- Encodings --------------------------------------------------------- */

#define ICV_UTF8       0
#define ICV_LATIN1     1
#define ICV_ASCII      2
#define ICV_ISO2022JP  3

/*
 * ISO-2022-JP is the one code set here that is not a function of the bytes in
 * front of the converter: an escape sequence shifts which set the following
 * bytes are read in, and it stays shifted until the next one. So a decode may
 * consume bytes and hand back NO character, and an encode has to know which
 * set it is already in and return to ASCII before the string ends.
 *
 * Only the four shifts glibc's plain ISO-2022-JP converter answers are
 * recognised -- ASCII, JIS X 0201 Roman, and the two spellings of JIS X 0208.
 * ESC ( I (halfwidth katakana) is NOT one of them, which is why a halfwidth
 * katakana is unconvertible in this direction, and every other escape is left
 * to the pass-through arm below rather than refused.
 */
#define ICV_G0_ASCII  0
#define ICV_G0_ROMAN  1
#define ICV_G0_KANJI  2

/* A decode that consumed a shift and produced nothing. Above every code point
 * IcvDecode() can answer -- the six-byte UTF-8 form tops out at 0x7FFFFFFF. */
#define ICV_NOCHAR    0xFFFFFFFF

/*
 * php's own cap on an encoding NAME, checked before the name is looked at:
 * ICONV_CSNMAXLEN is 64 and the test is `>=`, so a 64-character name is already
 * "exceeds the maximum allowed length of 64 characters".
 */
#define ICV_CSNMAXLEN 64

/*
 * The names glibc registers for the code sets PHL models, minus its
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
	{ "CSASCII",          ICV_ASCII  },
#ifdef PH7_ENABLE_JIS
	/* glibc registers exactly these three for the plain converter. Its
	 * ISO-2022-JP-2 and -3 are separate converters with wider set repertoires,
	 * so they stay unknown here rather than being answered by a narrower one. */
	{ "ISO-2022-JP",      ICV_ISO2022JP },
	{ "ISO2022JP",        ICV_ISO2022JP },
	{ "CSISO2022JP",      ICV_ISO2022JP }
#endif
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
	/* The name as it was GIVEN, kept because the string family reports an
	 * unknown one late -- at the point a conversion would have been opened --
	 * and by then the argument that carried it may be gone. Bounded by the
	 * length cap, which is checked before any of this. */
	char zName[ICV_CSNMAXLEN];
	int nName;
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
	pCs->nName = n < ICV_CSNMAXLEN ? n : ICV_CSNMAXLEN;
	SyMemcpy(z,pCs->zName,(sxu32)pCs->nName);
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
#define ICV_UNKNOWN_ERR   7
#define ICV_TOO_LONG      8

/*
 * Decode the character at z[0..n-1] under iEnc. Answers its byte length with
 * *pCp set to the code point, or 0 with *pErr set to the diagnostic the
 * sequence earns. The UTF-8 accepted is the original six-byte encoding, minus
 * overlongs, the surrogate range and the C0/C1/FE/FF lead bytes -- glibc's set,
 * not Unicode's.
 *
 * *pG0 is the shift state ISO-2022-JP reads in and carries forward; every other
 * code set leaves it alone. A shift consumes its three bytes and answers
 * ICV_NOCHAR, so a caller counts characters by what it stores, not by how many
 * times it went round.
 */
static int IcvDecode(const unsigned char *z,int n,int iEnc,sxu32 *pCp,int *pErr,int *pG0)
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
#ifdef PH7_ENABLE_JIS
	if( iEnc == ICV_ISO2022JP ){
		sxu32 cp;
		if( c == 0x1B ){
			/* An escape needs three bytes before it can be told from an
			 * unrecognised one, so two bytes at the end of the input are an
			 * INCOMPLETE character rather than an illegal sequence -- which is
			 * what makes a truncated shift refuse even under //IGNORE. */
			if( n < 3 ){
				*pErr = ICV_ILLEGAL_CHAR;
				return 0;
			}
			if( z[1] == '(' && (z[2] == 'B' || z[2] == 'J') ){
				*pG0 = z[2] == 'B' ? ICV_G0_ASCII : ICV_G0_ROMAN;
				*pCp = ICV_NOCHAR;
				return 3;
			}
			if( z[1] == '$' && (z[2] == 'B' || z[2] == '@') ){
				/* The 1978 and 1983 spellings name the same cells. */
				*pG0 = ICV_G0_KANJI;
				*pCp = ICV_NOCHAR;
				return 3;
			}
			/* Anything else is not a shift at all: 0x1B is below 0x21, so it
			 * falls into the pass-through arm below and comes out as U+001B
			 * with the two bytes after it read in whatever set is current. */
		}
		if( c > 0x7F ){
			*pErr = ICV_ILLEGAL_SEQ;
			return 0;
		}
		if( *pG0 == ICV_G0_ASCII || c < 0x21 || c == 0x7F ){
			/* The controls, the space and DEL sit outside every shifted set
			 * and are read as themselves wherever the converter stands. */
			*pCp = c;
			return 1;
		}
		if( *pG0 == ICV_G0_ROMAN ){
			*pCp = PH7_JisX0201RomanToUni((int)c);
			return 1;
		}
		if( n < 2 ){
			*pErr = ICV_ILLEGAL_CHAR;
			return 0;
		}
		cp = PH7_JisX0208ToUni((int)c,(int)z[1]);
		if( cp == 0 ){
			*pErr = ICV_ILLEGAL_SEQ;
			return 0;
		}
		*pCp = cp;
		return 2;
	}
#endif /* PH7_ENABLE_JIS */
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

/*
 * Write cp in iEnc when the encoding can hold it; 0 when it cannot. *pG0 is
 * the shift state, read and advanced for ISO-2022-JP and untouched by every
 * other code set.
 */
static int IcvEncodeDirect(SyBlob *pOut,sxu32 cp,int iEnc,int *pG0)
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
#ifdef PH7_ENABLE_JIS
	}else if( iEnc == ICV_ISO2022JP ){
		int iRow,iCell,iByte;
		if( cp < 0x80 ){
			/* An ASCII character is written where it stands when the current
			 * set spells it the same way. JIS X 0201 Roman covers 0x21..0x7E
			 * and spells all but two of them the same way, so the backslash
			 * and the tilde -- the two cells that set moved -- cost a shift
			 * back, and so do the controls, the space and DEL, which sit
			 * outside every shifted set in this direction too. */
			if( *pG0 == ICV_G0_KANJI
			 || (*pG0 == ICV_G0_ROMAN
			  && (cp < 0x21 || cp == 0x7F || cp == 0x5C || cp == 0x7E)) ){
				SyBlobAppend(pOut,"\033(B",3);
				*pG0 = ICV_G0_ASCII;
			}
			zEnc[0] = (unsigned char)cp;
			SyBlobAppend(pOut,zEnc,1);
			return 1;
		}
		if( PH7_JisX0201RomanFromUni(cp,&iByte) ){
			/* Only the yen sign and the overline reach here: everything else
			 * this set holds is below 0x80 and was answered above. */
			if( *pG0 != ICV_G0_ROMAN ){
				SyBlobAppend(pOut,"\033(J",3);
				*pG0 = ICV_G0_ROMAN;
			}
			zEnc[0] = (unsigned char)iByte;
			SyBlobAppend(pOut,zEnc,1);
			return 1;
		}
		if( PH7_JisX0208FromUni(cp,&iRow,&iCell) ){
			if( *pG0 != ICV_G0_KANJI ){
				SyBlobAppend(pOut,"\033$B",3);
				*pG0 = ICV_G0_KANJI;
			}
			zEnc[0] = (unsigned char)iRow;
			zEnc[1] = (unsigned char)iCell;
			SyBlobAppend(pOut,zEnc,2);
			return 1;
		}
		/* The halfwidth katakana land here, and so does everything JIS X 0208
		 * has no cell for. */
		return 0;
#endif /* PH7_ENABLE_JIS */
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
 * Return a stateful target to ASCII. Every byte a caller is about to write
 * outside IcvEncodeDirect() -- a transliteration, the '?' a miss produces, the
 * tail of a finished string -- is ASCII, and ISO-2022-JP cannot carry one
 * while it stands in another set.
 */
static void IcvEncodeShiftAscii(SyBlob *pOut,int iEnc,int *pG0)
{
#ifdef PH7_ENABLE_JIS
	if( iEnc == ICV_ISO2022JP && *pG0 != ICV_G0_ASCII ){
		SyBlobAppend(pOut,"\033(B",3);
		*pG0 = ICV_G0_ASCII;
	}
#else
	SXUNUSED(pOut); SXUNUSED(iEnc); SXUNUSED(pG0);
#endif
}

/*
 * Write cp in iEnc, transliterating when bTranslit is set. Answers 0 only when
 * the encoding cannot hold cp and transliteration is off -- the '?' a
 * transliterated miss produces is glibc's own answer, and it is a SUCCESS, as
 * is the EMPTY replacement a combining mark transliterates to.
 */
static int IcvEncode(SyBlob *pOut,sxu32 cp,int iEnc,int bTranslit,int *pG0)
{
	const icv_translit *pRow;
	if( IcvEncodeDirect(pOut,cp,iEnc,pG0) ){
		return 1;
	}
	if( !bTranslit ){
		return 0;
	}
	/* Everything below writes bytes of its own, and they are ASCII. */
	IcvEncodeShiftAscii(pOut,iEnc,pG0);
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
	int iFrom,const icv_cs *pTo,int *pbDropped,int *pG0In,int *pG0Out)
{
	const unsigned char *z = (const unsigned char *)zIn;
	int i = *pi,rc = ICV_OK;
	while( i < nIn ){
		sxu32 cp = 0;
		int err = ICV_OK;
		int nSeq = IcvDecode(&z[i],nIn - i,iFrom,&cp,&err,pG0In);
		if( nSeq == 0 ){
			rc = err;
			break;
		}
		if( cp == ICV_NOCHAR ){
			/* A shift: it moved the source's state and produced nothing. */
			i += nSeq;
			continue;
		}
		if( IcvEncode(pOut,cp,pTo->iEnc,pTo->bTranslit,pG0Out) ){
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
	/* Both states live across the whole string and across every call the
	 * //IGNORE loop makes: php reuses one conversion descriptor, so stepping
	 * over a bad byte does not put the converter back in ASCII. */
	int iG0In = ICV_G0_ASCII,iG0Out = ICV_G0_ASCII;
	for(;;){
		int rc = IcvConvertCall(pOut,zIn,nIn,&i,iFrom,pTo,&bDropped,&iG0In,&iG0Out);
		if( rc == ICV_OK ){
			IcvEncodeShiftAscii(pOut,pTo->iEnc,&iG0Out);
			return ICV_OK;
		}
		if( bIgnore && rc == ICV_ILLEGAL_SEQ ){
			if( nIn - i <= 1 ){
				IcvEncodeShiftAscii(pOut,pTo->iEnc,&iG0Out);
				return ICV_OK;
			}
			i++;
			continue;
		}
		return rc;
	}
}

/*
 * Convert from zIn[*pi..] until the OUTPUT would pass iRoom bytes, leaving *pi
 * on a character boundary and *pnTook set to how many characters went. This is
 * how the MIME encoder fills one encoded word: it has a byte budget for the
 * line and has to stop on a whole character, which is exactly what iconv(3)
 * does when its output buffer runs out.
 */
static int IcvConvertBounded(SyBlob *pOut,const char *zIn,int nIn,int *pi,
	int iFrom,const icv_cs *pTo,sxi64 iRoom,int *pnTook)
{
	const unsigned char *z = (const unsigned char *)zIn;
	int i = *pi,nTook = 0;
	int iG0In = ICV_G0_ASCII,iG0Out = ICV_G0_ASCII;
	if( pTo->iEnc == ICV_ISO2022JP ){
		/* The word has to end back in ASCII, so those three bytes are spoken
		 * for before the first character is written. */
		iRoom -= 3;
	}
	while( i < nIn ){
		sxu32 cp = 0;
		int err = ICV_OK;
		sxu32 nBefore = SyBlobLength(pOut);
		int iG0Before = iG0Out;
		int nSeq = IcvDecode(&z[i],nIn - i,iFrom,&cp,&err,&iG0In);
		if( nSeq == 0 ){
			*pi = i;
			*pnTook = nTook;
			return err;
		}
		if( cp == ICV_NOCHAR ){
			i += nSeq;
			continue;
		}
		if( !IcvEncode(pOut,cp,pTo->iEnc,pTo->bTranslit,&iG0Out) ){
			*pi = i;
			*pnTook = nTook;
			return ICV_ILLEGAL_SEQ;
		}
		if( (sxi64)SyBlobLength(pOut) > iRoom ){
			/* One character too many: give the bytes back and stop here. */
			SyBlobLength(pOut) = nBefore;
			iG0Out = iG0Before;
			break;
		}
		i += nSeq;
		nTook++;
	}
	IcvEncodeShiftAscii(pOut,pTo->iEnc,&iG0Out);
	*pi = i;
	*pnTook = nTook;
	return ICV_OK;
}

/*
 * One buffer converted between two charsets NAMED the way a MO catalog's
 * `Content-Type` header names them: ext/gettext's whole conversion door.
 *
 * Transliteration is on unconditionally because glibc's gettext opens its own
 * conversion that way (`norm_add_slashes (outcharset, "TRANSLIT")` in
 * loadmsgcat.c), which is why a translated string a narrower target cannot hold
 * comes back approximated rather than refused. Answers -1 when either name is
 * outside the code sets PHL models, which is the case php answers the
 * msgid UNTRANSLATED for -- the same thing its iconv_open() failure does.
 */
PH7_PRIVATE int PH7_IconvTranslate(SyBlob *pOut,const char *zIn,int nIn,
	const char *zFrom,int nFrom,const char *zTo,int nTo)
{
	icv_cs sFrom,sTo;
	if( nFrom < 1 || nTo < 1 ){
		return -1;
	}
	IcvParseCharset(zFrom,nFrom,&sFrom);
	IcvParseCharset(zTo,nTo,&sTo);
	if( sFrom.iEnc < 0 || sTo.iEnc < 0 ){
		return -1;
	}
	if( sFrom.iEnc == sTo.iEnc ){
		/* Same code set: glibc's conversion is a copy, and so is this. */
		SyBlobAppend(pOut,zIn,(sxu32)nIn);
		return PH7_OK;
	}
	sTo.bTranslit = 1;
	sTo.bIgnore = 0;
	return IcvConvert(pOut,zIn,nIn,sFrom.iEnc,&sTo,0) == ICV_OK ? PH7_OK : -1;
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
	case ICV_UNKNOWN_ERR:
		/* php prints `errno` here, and nothing has SET it -- the number is
		 * whatever the last libc call in the process left behind (22 and 84
		 * are what the same input produces on this box, in the same run). PHL
		 * has no stale errno to leak, so it says 0; twin-paired. */
		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Unknown error (0)");
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


/* --- The string quartet ------------------------------------------------ */

/*
 * php's own name for the wide intermediate every one of these four converts
 * THROUGH, and the one that shows up in their "Wrong encoding" message: the
 * conversion that fails is the one INTO it, so the message always reads
 * `from "<the argument>" to "UCS-4LE"`.
 */
#define ICV_SUPERSET "UCS-4LE"

/*
 * The `?string $encoding = null` the string family shares. A missing or null
 * argument is php's INTERNAL encoding, which is `iconv.internal_encoding`
 * falling back to `default_charset` -- and since the scope policy removes the deprecated
 * `iconv.*` directives, `default_charset` is the whole of it here.
 *
 * Only php's LENGTH cap is a diagnostic at this point (answers 0 for it). An
 * unknown NAME is not, because php does not learn of one until it opens a
 * conversion -- which is why `iconv_strpos($h, "", 0, "NOPE")` is a silent
 * false while the same call with a needle warns. IcvEncReady() is that moment.
 */
static int IcvStrEncArg(ph7_context *pCtx,ph7_value *pArg,icv_cs *pCs)
{
	SyBlob sIni;
	int rc;
	if( pArg != 0 && !ph7_value_is_null(pArg) ){
		int nEnc;
		const char *zEnc = ph7_value_to_string(pArg,&nEnc);
		return IcvCharsetArg(pCtx,zEnc,nEnc,pCs);
	}
	SyBlobInit(&sIni,&pCtx->pVm->sAllocator);
	PH7_VmIniGetStr(pCtx->pVm,"default_charset",&sIni);
	rc = IcvCharsetArg(pCtx,(const char *)SyBlobData(&sIni),(int)SyBlobLength(&sIni),pCs);
	SyBlobRelease(&sIni);
	return rc;
}
/*
 * The "Wrong encoding" php raises where it would have opened the conversion.
 * The conversion the string family opens is the one INTO the wide intermediate,
 * so the message always names UCS-4LE as the target. Answers 0 after raising.
 */
static int IcvEncReady(ph7_context *pCtx,const icv_cs *pCs)
{
	if( pCs->iEnc >= 0 ){
		return 1;
	}
	IcvShowError(pCtx,ICV_WRONG_CHARSET,ICV_SUPERSET,(int)sizeof(ICV_SUPERSET)-1,
		pCs->zName,pCs->nName);
	return 0;
}

/*
 * A decoded string: one code point per character plus the byte offset each one
 * starts at (nChar+1 entries, so the last is the buffer length). php works the
 * same way -- it converts to UCS-4 and operates there -- and it is what lets a
 * search answer in CHARACTERS while a slice is taken in BYTES.
 */
typedef struct icv_text icv_text;
struct icv_text {
	const char *zIn;
	int nByte;
	sxu32 *aCode;
	int *aOfft;
	int nChar;
};
#define ICV_TEXT_MAX 0x0FFFFFFF

/*
 * Decode zIn under iEnc into pText. Answers PH7_OK, or PH7_OK with *pErr set to
 * the diagnostic the input earns -- in which case pText is not usable but was
 * still allocated, so IcvTextRelease() is safe either way.
 */
static int IcvTextDecode(ph7_context *pCtx,icv_text *pText,const char *zIn,int nByte,
	int iEnc,int *pErr)
{
	const unsigned char *z = (const unsigned char *)zIn;
	int i = 0,n = 0,nSlot,iG0 = ICV_G0_ASCII;
	pText->zIn = zIn;
	pText->nByte = nByte;
	pText->nChar = 0;
	pText->aCode = 0;
	pText->aOfft = 0;
	*pErr = ICV_OK;
	if( nByte > ICV_TEXT_MAX ){
		return PH7_ContextMemoryError(pCtx);
	}
	nSlot = nByte + 1;
	pText->aCode = (sxu32 *)ph7_context_alloc_chunk(pCtx,
		(unsigned int)((sxu32)nSlot * (sizeof(sxu32) + sizeof(int))),FALSE,TRUE);
	if( pText->aCode == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	pText->aOfft = (int *)&pText->aCode[nSlot];
	while( i < nByte ){
		sxu32 cp = 0;
		int nSeq = IcvDecode(&z[i],nByte - i,iEnc,&cp,pErr,&iG0);
		if( nSeq == 0 ){
			/* The good PREFIX is the answer here, not zero: php's own walk
			 * stops at the bad character and everything before it has already
			 * been converted, so nChar is how far a search got and the count
			 * an out-of-bounds $offset is measured against. */
			pText->aOfft[n] = i;
			pText->nChar = n;
			return PH7_OK;
		}
		if( cp == ICV_NOCHAR ){
			/* A shift is not a character: it is not counted, and no offset
			 * points at it. */
			i += nSeq;
			continue;
		}
		pText->aCode[n] = cp;
		pText->aOfft[n] = i;
		i += nSeq;
		n++;
	}
	pText->aOfft[n] = nByte;
	pText->nChar = n;
	return PH7_OK;
}
static void IcvTextRelease(ph7_context *pCtx,icv_text *pText)
{
	if( pText->aCode ){
		ph7_context_free_chunk(pCtx,pText->aCode);
		pText->aCode = 0;
	}
}

/* int|false iconv_strlen(string $string, ?string $encoding = null) */
static int PH7_builtin_iconv_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn;
	int nIn,err = ICV_OK;
	icv_cs sCs;
	icv_text sText;
	if( nArg < 1 || !IcvStrEncArg(pCtx,nArg > 1 ? apArg[1] : 0,&sCs)
	 || !IcvEncReady(pCtx,&sCs) ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zIn = ph7_value_to_string(apArg[0],&nIn);
	if( IcvTextDecode(pCtx,&sText,zIn,nIn,sCs.iEnc,&err) != PH7_OK ){
		return PH7_OK;
	}
	IcvTextRelease(pCtx,&sText);
	if( err != ICV_OK ){
		IcvShowError(pCtx,err,ICV_SUPERSET,(int)sizeof(ICV_SUPERSET)-1,"",0);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_int(pCtx,sText.nChar);
	return PH7_OK;
}

/*
 * string|false iconv_substr(string $string, int $offset, ?int $length = null,
 *                           ?string $encoding = null)
 *
 * php clamps in the order its own code does, and the order is visible: a null
 * $length starts life as the string's BYTE count and is then clamped to the
 * character count, so it can never reach past the end however the two differ.
 */
static int PH7_builtin_iconv_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn;
	int nIn,err = ICV_OK,iStart,iStop;
	sxi64 iOfft,iLen;
	icv_cs sCs;
	icv_text sText;
	if( nArg < 2 || !IcvStrEncArg(pCtx,nArg > 3 ? apArg[3] : 0,&sCs)
	 || !IcvEncReady(pCtx,&sCs) ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zIn = ph7_value_to_string(apArg[0],&nIn);
	if( PH7_IntArgResolve(pCtx,apArg[1],"iconv_substr",2,"$offset","int",&iOfft) != PH7_OK ){
		return PH7_OK;
	}
	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){
		if( PH7_IntArgResolve(pCtx,apArg[2],"iconv_substr",3,"$length","?int",&iLen) != PH7_OK ){
			return PH7_OK;
		}
	}else{
		iLen = nIn;
	}
	if( IcvTextDecode(pCtx,&sText,zIn,nIn,sCs.iEnc,&err) != PH7_OK ){
		return PH7_OK;
	}
	if( err != ICV_OK ){
		IcvTextRelease(pCtx,&sText);
		IcvShowError(pCtx,err,ICV_SUPERSET,(int)sizeof(ICV_SUPERSET)-1,"",0);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( iOfft < 0 ){
		iOfft += sText.nChar;
		if( iOfft < 0 ){
			iOfft = 0;
		}
	}else if( iOfft > sText.nChar ){
		iOfft = sText.nChar;
	}
	if( iLen < 0 ){
		iLen += sText.nChar - iOfft;
		if( iLen < 0 ){
			iLen = 0;
		}
	}else if( iLen > sText.nChar ){
		iLen = sText.nChar;
	}
	if( iOfft + iLen > sText.nChar ){
		iLen = sText.nChar - iOfft;
	}
#ifdef PH7_ENABLE_JIS
	if( sCs.iEnc == ICV_ISO2022JP ){
		/* A slice of a SHIFTED encoding is not a slice of its bytes. php goes
		 * through code points and back, so the answer carries the shifts it
		 * needs of its own and none of the ones it inherited -- a substring
		 * that starts inside a kanji run opens with the shift into it, and one
		 * that ends there closes with the shift back out. */
		SyBlob sOut;
		int k,iG0 = ICV_G0_ASCII;
		SyBlobInit(&sOut,&pCtx->pVm->sAllocator);
		for( k = 0 ; k < (int)iLen ; ++k ){
			IcvEncodeDirect(&sOut,sText.aCode[iOfft + k],sCs.iEnc,&iG0);
		}
		IcvEncodeShiftAscii(&sOut,sCs.iEnc,&iG0);
		ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));
		SyBlobRelease(&sOut);
		IcvTextRelease(pCtx,&sText);
		return PH7_OK;
	}
#endif /* PH7_ENABLE_JIS */
	iStart = sText.aOfft[iOfft];
	iStop = sText.aOfft[iOfft + iLen];
	ph7_result_string(pCtx,&zIn[iStart],iStop - iStart);
	IcvTextRelease(pCtx,&sText);
	return PH7_OK;
}

/*
 * The search both position builtins run: the first match at or after iFrom, or
 * the LAST one when bReverse is set. Answers the character index or -1.
 */
static int IcvSearch(const icv_text *pH,const icv_text *pN,int iFrom,int bReverse)
{
	int i,iFound = -1;
	if( pN->nChar < 1 || pN->nChar > pH->nChar ){
		return -1;
	}
	for( i = iFrom ; i + pN->nChar <= pH->nChar ; ++i ){
		int k;
		for( k = 0 ; k < pN->nChar && pH->aCode[i+k] == pN->aCode[k] ; ++k ){}
		if( k == pN->nChar ){
			if( !bReverse ){
				return i;
			}
			iFound = i;
		}
	}
	return iFound;
}

/*
 * int|false iconv_strpos(string $haystack, string $needle, int $offset = 0,
 *                        ?string $encoding = null)
 * int|false iconv_strrpos(string $haystack, string $needle,
 *                         ?string $encoding = null)
 *
 * One body, because php's two differ only in four places: strrpos has no
 * $offset at all, it tests the empty needle BEFORE the encoding is looked at
 * (so an over-long name is silent there and warns in strpos), it keeps looking
 * after a match instead of stopping at the first, and it has no out-of-bounds
 * ValueError to raise.
 *
 * The ORDER below is php's and it is visible from the outside, because the two
 * halves of the search report differently. The NEEDLE goes through a whole
 * conversion, so an ill-formed one raises the same two diagnostics anything
 * else does. The HAYSTACK is walked one character at a time into a buffer
 * exactly one wide, and php's loop leaves that walk the moment a character
 * fails to convert -- WITHOUT recording why. So an ill-formed haystack is
 * silent: the search simply cannot see past the bad byte, and
 * `iconv_strpos("ab\xFFcd","cd")` is FALSE with nothing said. What it does
 * decide is the count the out-of-bounds ValueError is measured against, which
 * is why an $offset past the first bad byte raises where the same call on a
 * clean string answers false.
 */
static int IcvStrposBody(ph7_context *pCtx,int nArg,ph7_value **apArg,int bReverse)
{
	const char *zH,*zN;
	int nH,nN,err = ICV_OK,iScanned,iFound;
	sxi64 iOfft = 0;
	icv_cs sCs;
	icv_text sH,sN;
	if( nArg < 2 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zH = ph7_value_to_string(apArg[0],&nH);
	zN = ph7_value_to_string(apArg[1],&nN);
	if( bReverse && nN < 1 ){
		/* php's order: the empty needle answers false before the name is
		 * measured, which is why an over-long $encoding is silent here. */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( !IcvStrEncArg(pCtx,nArg > (bReverse ? 2 : 3) ? apArg[bReverse ? 2 : 3] : 0,&sCs) ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( !bReverse && nArg > 2
	 && PH7_IntArgResolve(pCtx,apArg[2],"iconv_strpos",3,"$offset","int",&iOfft) != PH7_OK ){
		/* $offset is php's plain `int`, so a null is the deprecation the scope policy turns
		 * into a TypeError -- and it has to be REFUSED here rather than skipped,
		 * which is what treating a null argument as "not passed" would do. */
		return PH7_OK;
	}
	if( iOfft < 0 ){
		/* A negative offset counts from the end, so THIS is the one path that
		 * measures the haystack before the needle -- and the one place an
		 * ill-formed haystack is heard about at all. */
		if( !IcvEncReady(pCtx,&sCs) ){
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		if( IcvTextDecode(pCtx,&sH,zH,nH,sCs.iEnc,&err) != PH7_OK ){
			return PH7_OK;
		}
		IcvTextRelease(pCtx,&sH);
		if( err != ICV_OK ){
			IcvShowError(pCtx,err,ICV_SUPERSET,(int)sizeof(ICV_SUPERSET)-1,"",0);
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		iOfft += sH.nChar;
		if( iOfft < 0 ){
			PH7_VmThrowException(pCtx,"ValueError",
				"iconv_strpos(): Argument #3 ($offset) must be contained in argument #1 ($haystack)");
			return PH7_OK;
		}
	}
	if( nN < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* php converts the NEEDLE whole before it scans, so an ill-formed needle is
	 * the same two diagnostics as an ill-formed argument anywhere else. */
	if( !IcvEncReady(pCtx,&sCs) ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( IcvTextDecode(pCtx,&sN,zN,nN,sCs.iEnc,&err) != PH7_OK ){
		return PH7_OK;
	}
	if( err != ICV_OK ){
		IcvTextRelease(pCtx,&sN);
		IcvShowError(pCtx,err,ICV_SUPERSET,(int)sizeof(ICV_SUPERSET)-1,"",0);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( IcvTextDecode(pCtx,&sH,zH,nH,sCs.iEnc,&err) != PH7_OK ){
		IcvTextRelease(pCtx,&sN);
		return PH7_OK;
	}
	iScanned = sH.nChar;
	iFound = IcvSearch(&sH,&sN,bReverse ? 0 : (int)iOfft,bReverse);
	IcvTextRelease(pCtx,&sN);
	IcvTextRelease(pCtx,&sH);
	if( err != ICV_OK ){
		/* Whether the walk SAYS anything about the bad character depends on how
		 * far it got, because php hears about it from the call that converted
		 * the character BEFORE it. An error at index 0 is therefore silent, and
		 * so is one the walk never reached -- a full match ends the walk, so
		 * `iconv_strpos(str_repeat("x",100)."\xFF","xx")` answers 0 with
		 * nothing said while `iconv_strpos("ab\xFF","ab")` answers FALSE with
		 * the notice. */
		int iStop = (!bReverse && iFound >= 0) ? iFound + sN.nChar - 1 : iScanned;
		if( iScanned >= 1 && iStop >= iScanned - 1 ){
			IcvShowError(pCtx,err,ICV_SUPERSET,(int)sizeof(ICV_SUPERSET)-1,"",0);
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
	}
	if( !bReverse && iOfft > iScanned ){
		PH7_VmThrowException(pCtx,"ValueError",
			"iconv_strpos(): Argument #3 ($offset) must be contained in argument #1 ($haystack)");
		return PH7_OK;
	}
	if( iFound < 0 ){
		ph7_result_bool(pCtx,0);
	}else{
		ph7_result_int(pCtx,iFound);
	}
	return PH7_OK;
}
static int PH7_builtin_iconv_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return IcvStrposBody(pCtx,nArg,apArg,0);
}
static int PH7_builtin_iconv_strrpos(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return IcvStrposBody(pCtx,nArg,apArg,1);
}

/* --- The encoding settings --------------------------------------------- */

/*
 * array|string|false iconv_get_encoding(string $type = "all")
 *
 * php answers `iconv.input_encoding` / `output_encoding` / `internal_encoding`,
 * each falling back to `default_charset`. The scope policy removes all three of those
 * directives -- every one of them is deprecated, which is also why this
 * function's SETTER counterpart is not here at all (see the twin pair in
 * 002-integration) -- so `default_charset` is what all three answer, and
 * moving it moves them together, exactly as php does when the iconv directives
 * are left unset. $type matches case-insensitively; anything else is FALSE,
 * with no diagnostic of any kind.
 */
static int PH7_builtin_iconv_get_encoding(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	static const char * const azType[] = { "input_encoding", "output_encoding", "internal_encoding" };
	SyBlob sIni;
	const char *zType = "all";
	int nType = 3,k;
	if( nArg > 0 ){
		zType = ph7_value_to_string(apArg[0],&nType);
	}
	SyBlobInit(&sIni,&pCtx->pVm->sAllocator);
	PH7_VmIniGetStr(pCtx->pVm,"default_charset",&sIni);
	if( nType == 3 && SyStrnicmp(zType,"all",3) == 0 ){
		ph7_value *pArray = ph7_context_new_array(pCtx);
		ph7_value *pVal = ph7_context_new_scalar(pCtx);
		if( pArray == 0 || pVal == 0 ){
			SyBlobRelease(&sIni);
			return PH7_ContextMemoryError(pCtx);
		}
		ph7_value_string(pVal,(const char *)SyBlobData(&sIni),(int)SyBlobLength(&sIni));
		for( k = 0 ; k < (int)SX_ARRAYSIZE(azType) ; ++k ){
			ph7_array_add_strkey_elem(pArray,azType[k],pVal);
		}
		ph7_result_value(pCtx,pArray);
		ph7_context_release_value(pCtx,pVal);
		SyBlobRelease(&sIni);
		return PH7_OK;
	}
	for( k = 0 ; k < (int)SX_ARRAYSIZE(azType) ; ++k ){
		if( nType == (int)SyStrlen(azType[k]) && SyStrnicmp(zType,azType[k],(sxu32)nType) == 0 ){
			ph7_result_string(pCtx,(const char *)SyBlobData(&sIni),(int)SyBlobLength(&sIni));
			SyBlobRelease(&sIni);
			return PH7_OK;
		}
	}
	SyBlobRelease(&sIni);
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}

/* --- MIME header words -------------------------------------------------- */

/*
 * Convert zIn into pOut, appending NOTHING unless the whole run converts. This
 * is php's _php_iconv_appendl(), which builds its own buffer and only hands it
 * over on success -- so a header run that fails part-way leaves no partial
 * bytes behind, which is visible whenever CONTINUE_ON_ERROR lets the scan go on
 * afterwards.
 */
static int IcvAppendConv(ph7_context *pCtx,SyBlob *pOut,const char *zIn,int nIn,
	int iFrom,const icv_cs *pTo)
{
	SyBlob sTmp;
	int rc;
	SyBlobInit(&sTmp,&pCtx->pVm->sAllocator);
	rc = IcvConvert(&sTmp,zIn,nIn,iFrom,pTo,0);
	if( rc == ICV_OK ){
		SyBlobAppend(pOut,SyBlobData(&sTmp),SyBlobLength(&sTmp));
	}
	SyBlobRelease(&sTmp);
	return rc;
}


/*
 * base64, in the two shapes RFC 2047 needs. The DECODER is php's
 * php_base64_decode() in its non-strict mode: every byte outside the alphabet
 * is skipped, and a partial final group contributes what its bits allow, so
 * "!!!" decodes to the empty string rather than failing.
 */
static const signed char aIcvB64[128] = {
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
	52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-2,-1,-1,
	-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
	15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
	-1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
	41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1
};
static void IcvBase64Encode(SyBlob *pOut,const unsigned char *z,int n)
{
	static const char zAlpha[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	char zGrp[4];
	int i = 0;
	while( i + 2 < n ){
		sxu32 v = ((sxu32)z[i] << 16) | ((sxu32)z[i+1] << 8) | z[i+2];
		zGrp[0] = zAlpha[(v >> 18) & 0x3F]; zGrp[1] = zAlpha[(v >> 12) & 0x3F];
		zGrp[2] = zAlpha[(v >> 6) & 0x3F];  zGrp[3] = zAlpha[v & 0x3F];
		SyBlobAppend(pOut,zGrp,4);
		i += 3;
	}
	if( i < n ){
		sxu32 v = (sxu32)z[i] << 16;
		int nRem = n - i;
		if( nRem > 1 ){
			v |= (sxu32)z[i+1] << 8;
		}
		zGrp[0] = zAlpha[(v >> 18) & 0x3F];
		zGrp[1] = zAlpha[(v >> 12) & 0x3F];
		zGrp[2] = nRem > 1 ? zAlpha[(v >> 6) & 0x3F] : '=';
		zGrp[3] = '=';
		SyBlobAppend(pOut,zGrp,4);
	}
}
/* How many base64 characters n input bytes become. */
static int IcvBase64Len(int n)
{
	return ((n + 2) / 3) * 4;
}
static void IcvBase64Decode(SyBlob *pOut,const char *z,int n)
{
	sxu32 v = 0;
	int i,nBit = 0;
	for( i = 0 ; i < n ; ++i ){
		int c = (unsigned char)z[i];
		int d = (c < 128) ? aIcvB64[c] : -1;
		if( d < 0 ){
			/* php's non-strict decoder skips EVERYTHING outside the alphabet,
			 * and that includes the padding: `=` is counted and stepped over,
			 * not treated as the end, so ":!!!=ZZ" still decodes its "ZZ". */
			continue;
		}
		v = (v << 6) | (sxu32)d;
		nBit += 6;
		if( nBit >= 8 ){
			unsigned char b = (unsigned char)((v >> (nBit - 8)) & 0xFF);
			SyBlobAppend(pOut,&b,1);
			nBit -= 8;
		}
	}
}

/*
 * quoted-printable, php's php_quot_print_decode() with `replace_us_by_ws` on --
 * which is the `Q` of RFC 2047, where `_` stands for a space. Answers 0 for the
 * strings php refuses (a `=` followed by one hex digit and a non-hex one, and a
 * soft break that runs off the end); those are what make its caller fall back
 * to the raw encoded word. Stops at a NUL, as php's does.
 */
static int IcvQPrintDecode(SyBlob *pOut,const char *z,int n,int bUnderscore)
{
	int i = 0;
	while( i < n && z[i] != 0 ){
		int c = (unsigned char)z[i];
		if( c != '=' ){
			unsigned char b = (unsigned char)((bUnderscore && c == '_') ? ' ' : c);
			SyBlobAppend(pOut,&b,1);
			i++;
			continue;
		}
		i++;
		if( i >= n || z[i] == 0 ){
			break;
		}
		if( SyisHex(z[i]) ){
			int hi = SyHexToint(z[i]);
			if( i + 1 >= n || !SyisHex(z[i+1]) ){
				return 0;
			}
			{
				unsigned char b = (unsigned char)((hi << 4) | SyHexToint(z[i+1]));
				SyBlobAppend(pOut,&b,1);
			}
			i += 2;
			continue;
		}
		/* A soft line break: any run of spaces and tabs, then the newline. */
		while( z[i] == ' ' || z[i] == '\t' ){
			i++;
			if( i >= n || z[i] == 0 ){
				return 0;
			}
		}
		if( z[i] != '\r' && z[i] != '\n' ){
			return 0;
		}
		if( z[i] == '\r' && i + 1 < n && z[i+1] == '\n' ){
			i++;
		}
		i++;
	}
	return 1;
}

/* php's qp_table: 1 = the byte may be written as itself, 3 = it needs `=XX`. */
static int IcvQPrintCost(int c)
{
	if( c < 0x21 || c > 0x7E || c == '=' || c == '?' || c == '_' ){
		return 3;
	}
	return 1;
}

#define ICV_SCHEME_B64 0
#define ICV_SCHEME_QP  1

/*
 * php's _php_iconv_mime_encode(). The line budget is a single running counter
 * that every piece of the header subtracts from, and the two schemes spend it
 * differently: base64 works out how many INPUT bytes fit a line by arithmetic
 * (`(char_cnt - 2) / 4 * 3`, minus a four-byte reserve), while quoted-printable
 * converts a candidate run, PRICES it through the table above, and shrinks the
 * run until the price fits -- which is why a value made of three-byte
 * characters wraps where it does. The counter is charged the field name's RAW
 * byte length even when the name is dropped for not being ASCII.
 */
static int IcvMimeEncode(ph7_context *pCtx,SyBlob *pOut,
	const char *zName,int nName,const char *zVal,int nVal,
	sxi64 iMaxLine,const char *zLf,int nLf,int iScheme,
	const icv_cs *pTo,const char *zToName,int nToName,int iFrom)
{
	SyBlob sChunk;
	sxi64 iBudget;
	int i = 0,rc = ICV_OK;
	if( (sxi64)nName + 2 >= iMaxLine || (sxi64)nToName + 12 >= iMaxLine ){
		return ICV_TOO_BIG;
	}
	iBudget = iMaxLine;
	SyBlobInit(&sChunk,&pCtx->pVm->sAllocator);
	/* The field NAME goes out in ASCII, and php ignores whether that worked --
	 * so a name carrying a byte ASCII cannot hold contributes NOTHING (the
	 * append is all-or-nothing) while still costing its raw length. */
	{
		icv_cs sAscii;
		sAscii.iEnc = ICV_ASCII;
		sAscii.bTranslit = 0;
		sAscii.bIgnore = 0;
		sAscii.nName = 0;
		(void)IcvAppendConv(pCtx,pOut,zName,nName,iFrom,&sAscii);
	}
	iBudget -= nName;
	SyBlobAppend(pOut,": ",2);
	iBudget -= 2;
	do{
		/* `=?`, the charset, `?`, the scheme letter, `?` and the closing `?=`;
		 * base64 needs one more character than quoted-printable can get away
		 * with, which is what makes the two minimums differ. */
		sxi64 iMinWord = 7 + nToName + (iScheme == ICV_SCHEME_B64 ? 4 : 3);
		if( iBudget < iMinWord + nLf + 1 ){
			SyBlobAppend(pOut,zLf,(sxu32)nLf);
			SyBlobAppend(pOut," ",1);
			iBudget = iMaxLine - 1;
		}
		SyBlobAppend(pOut,"=?",2);
		SyBlobAppend(pOut,zToName,(sxu32)nToName);
		SyBlobAppend(pOut,"?",1);
		SyBlobAppend(pOut,iScheme == ICV_SCHEME_B64 ? "B" : "Q",1);
		SyBlobAppend(pOut,"?",1);
		iBudget -= 2 + nToName + 3;
		if( iScheme == ICV_SCHEME_B64 ){
			sxi64 iRoom = (iBudget - 2) / 4 * 3 - 4;
			int nTook;
			if( iRoom <= 0 ){
				rc = ICV_TOO_BIG;
				break;
			}
			SyBlobReset(&sChunk);
			rc = IcvConvertBounded(&sChunk,zVal,nVal,&i,iFrom,pTo,iRoom,&nTook);
			if( rc != ICV_OK ){
				break;
			}
			if( nTook == 0 && i < nVal ){
				/* The line has room for a word but not for one CHARACTER of the
				 * value: php's iconv comes back E2BIG having consumed nothing,
				 * and refuses rather than emitting an empty word forever. */
				rc = ICV_TOO_BIG;
				break;
			}
			if( IcvBase64Len((int)SyBlobLength(&sChunk)) > iBudget ){
				rc = ICV_UNKNOWN_ERR;
				break;
			}
			IcvBase64Encode(pOut,(const unsigned char *)SyBlobData(&sChunk),
				(int)SyBlobLength(&sChunk));
			iBudget -= IcvBase64Len((int)SyBlobLength(&sChunk));
		}else{
			sxi64 iRoom = iBudget - 2;
			int nTook = 0,k,iCost = 0;
			for(;;){
				int iStart = i;
				if( iRoom <= 0 ){
					rc = ICV_UNKNOWN_ERR;
					break;
				}
				SyBlobReset(&sChunk);
				rc = IcvConvertBounded(&sChunk,zVal,nVal,&i,iFrom,pTo,iRoom,&nTook);
				if( rc != ICV_OK ){
					break;
				}
				if( nTook == 0 && i < nVal ){
					/* Not even one character fits: php's own E2BIG-with-no-progress
					 * refusal, which is what keeps this from emitting an empty
					 * encoded word for ever. */
					rc = ICV_UNKNOWN_ERR;
					break;
				}
				iCost = 0;
				for( k = 0 ; k < (int)SyBlobLength(&sChunk) ; ++k ){
					iCost += IcvQPrintCost(((const unsigned char *)SyBlobData(&sChunk))[k]);
				}
				if( iCost <= iBudget - 2 ){
					break;
				}
				iRoom -= ((iCost - (iBudget - 2)) + 2) / 3;
				i = iStart;
			}
			if( rc != ICV_OK ){
				break;
			}
			for( k = 0 ; k < (int)SyBlobLength(&sChunk) ; ++k ){
				int c = ((const unsigned char *)SyBlobData(&sChunk))[k];
				if( IcvQPrintCost(c) == 1 ){
					char b = (char)c;
					SyBlobAppend(pOut,&b,1);
					iBudget--;
				}else{
					static const char zHex[] = "0123456789ABCDEF";
					char zEsc[3];
					zEsc[0] = '='; zEsc[1] = zHex[(c >> 4) & 0x0F]; zEsc[2] = zHex[c & 0x0F];
					SyBlobAppend(pOut,zEsc,3);
					iBudget -= 3;
				}
			}
		}
		SyBlobAppend(pOut,"?=",2);
		iBudget -= 2;
	}while( i < nVal );
	SyBlobRelease(&sChunk);
	return rc;
}

/*
 * php's _php_iconv_mime_decode(), a thirteen-state scanner over the header
 * text. What makes it a scanner rather than a matcher is what it does with the
 * things RFC 2047 does not allow: an encoded word that turns out not to be one
 * is re-emitted as the RAW TEXT it was, from the '=' the scan started at, and
 * the whitespace BETWEEN two encoded words is dropped while whitespace between
 * a word and plain text is kept -- which is why `=?..?= =?..?=` joins and
 * `=?..?= x` does not.
 *
 * $mode carries two bits. STRICT (1) refuses the non-RFC forms php otherwise
 * accepts -- an encoded word not followed by whitespace, above all -- and
 * CONTINUE_ON_ERROR (2) turns every refusal into "emit the raw word and carry
 * on". *pNext is left at the byte the scan stopped on, which is how
 * iconv_mime_decode_headers() walks a whole header block one field at a time.
 */
static int IcvMimeDecode(ph7_context *pCtx,SyBlob *pOut,const char *zIn,int nIn,
	const icv_cs *pTo,int *pNext,int *pbTouched,int iMode)
{
	SyBlob sWord;
	const char *z = zIn;
	int i = 0,rc = ICV_OK;
	int iState = 0,iScheme = ICV_SCHEME_B64,nLeft,bTouched = 0;
	int iWord = -1,iSpaces = -1,iCsName = -1,nCsName = 0,iText = -1,nText = 0;
	icv_cs sWordCs;
	int bStrict = (iMode & 1) != 0,bGoOn = (iMode & 2) != 0;
	SyBlobInit(&sWord,&pCtx->pVm->sAllocator);
	sWordCs.iEnc = -1;
	sWordCs.bTranslit = 0;
	sWordCs.bIgnore = 0;
	sWordCs.nName = 0;
	/*
	 * Emit z[iFrom..iTo) as literal text. php converts it from US-ASCII into the
	 * output charset, so a byte over 0x7F cannot go -- and what happens then is
	 * NOT one rule but three, because php checks the conversion's answer at some
	 * of these sites and not others. HARD fails whatever $mode says; CHECKED is
	 * the one CONTINUE_ON_ERROR was written for; SILENT drops the byte and says
	 * nothing, which is why a stray high byte AFTER a complete encoded word
	 * disappears while the same byte inside one is a failure.
	 */
/* php's _php_iconv_appendl() is ALL OR NOTHING: it converts into a buffer of
 * its own and only hands that to the output when the whole run went, so a run
 * that fails part-way appends nothing at all. And with a NULL source it appends
 * nothing and answers success, which is what an already-consumed `encoded_word`
 * becomes -- load-bearing, because state 9 stays in state 9 and re-runs for
 * every character after a word it could not convert. */
#define ICV_RAW_AT(iFrom,iTo,eMode) do { \
		int rcRaw = (iFrom) < 0 ? ICV_OK \
			: IcvAppendConv(pCtx,pOut,&z[iFrom],(iTo) - (iFrom),ICV_ASCII,pTo); \
		bTouched = 1; \
		if( (eMode) != 2 ){ \
			/* php's `err` is a RUNNING variable at the checked sites: a later \
			 * successful append assigns SUCCESS over an earlier failure, so a \
			 * refusal recorded mid-scan can still be forgotten. The silent \
			 * sites never touch it at all. */ \
			rc = rcRaw; \
			if( rcRaw != ICV_OK && ((eMode) == 1 || !bGoOn) ){ \
				goto done; \
			} \
			if( rcRaw != ICV_OK ){ rc = ICV_OK; } \
		} \
	} while(0)
#define ICV_RAW(iFrom,iTo)        ICV_RAW_AT(iFrom,iTo,0)
#define ICV_RAW_HARD(iFrom,iTo)   ICV_RAW_AT(iFrom,iTo,1)
#define ICV_RAW_SILENT(iFrom,iTo) ICV_RAW_AT(iFrom,iTo,2)
	/*
	 * php walks with a POSITION and a separate COUNT, and the two go out of step
	 * on purpose: several states rewind the position by one (`--p1`) without
	 * giving the count back, so the scan re-reads a byte and then stops one byte
	 * SHORT of the end. That is not an accident of style -- it is what makes
	 * `iconv_mime_decode("UHLDvGZ1bmc=\r\n")` a "Malformed string": the final
	 * "\n" is never reached, so the scanner ends mid-EOL instead of after one.
	 */
	nLeft = nIn;
	while( nLeft > 0 ){
		int c = (unsigned char)z[i];
		int bEos = 0;
		switch( iState ){
		case 0:   /* anything at all */
			if( c == '\r' ){ iState = 7; break; }
			if( c == '\n' ){ iState = 8; break; }
			if( c == '=' ){ iWord = i; iState = 1; break; }
			if( c == ' ' || c == '\t' ){ iSpaces = i; iState = 11; break; }
			ICV_RAW(i,i+1);
			iWord = -1;
			if( bStrict ){ iState = 12; }
			break;
		case 1:   /* after '=': expecting '?' */
			if( c != '?' ){
				if( c == '\r' || c == '\n' ){ i--; }
				ICV_RAW_HARD(iWord,i+1);
				iWord = -1;
				iState = bStrict ? 12 : 0;
				break;
			}
			iCsName = i + 1;
			iState = 2;
			break;
		case 2:   /* the charset name */
			if( c == '\r' || c == '\n' ){
				i--;
				ICV_RAW_HARD(iWord,i+1);
				iCsName = -1;
				iState = bStrict ? 12 : 0;
				break;
			}
			if( c != '?' && c != '*' ){
				break;
			}
			nCsName = i - iCsName;
			if( nCsName > ICV_CSNMAXLEN + 15 ){
				/* php's own 80-byte scratch buffer for the name. */
				if( !bGoOn ){ rc = ICV_MALFORMED; goto done; }
				ICV_RAW_HARD(iWord,i+1);
				iWord = -1;
				iState = bStrict ? 12 : 0;
				break;
			}
			IcvParseCharset(&z[iCsName],nCsName,&sWordCs);
			if( sWordCs.iEnc < 0 ){
				if( !bGoOn ){
					rc = ICV_WRONG_CHARSET;
					goto done;
				}
				/* php skips to the end of the word and hands it over raw. */
				{
					int nQ = 2;
					while( nQ > 0 && nLeft > 1 ){
						if( z[++i] == '?' ){ nQ--; }
						nLeft--;
					}
					if( i + 1 < nIn && z[i+1] == '=' ){
						i++;
						if( nLeft > 1 ){ nLeft--; }
					}
				}
				ICV_RAW_HARD(iWord,i+1);
				iState = 12;
				break;
			}
			iState = (c == '*') ? 10 : 3;
			break;
		case 3:   /* the scheme letter */
			if( c == 'b' || c == 'B' ){ iScheme = ICV_SCHEME_B64; iState = 4; break; }
			if( c == 'q' || c == 'Q' ){ iScheme = ICV_SCHEME_QP; iState = 4; break; }
			if( !bGoOn ){ rc = ICV_MALFORMED; goto done; }
			ICV_RAW_HARD(iWord,i+1);
			iWord = -1;
			iState = bStrict ? 12 : 0;
			break;
		case 4:   /* expecting '?' */
			if( c != '?' ){
				if( !bGoOn ){ rc = ICV_MALFORMED; goto done; }
				ICV_RAW_HARD(iWord,i+1);
				iWord = -1;
				iState = bStrict ? 12 : 0;
				break;
			}
			iText = i + 1;
			iState = 5;
			break;
		case 5:   /* the encoded text */
			if( c == '?' ){
				nText = i - iText;
				iState = 6;
			}
			break;
		case 6:   /* expecting the closing '=' */
			if( c != '=' ){
				if( !bGoOn ){ rc = ICV_MALFORMED; goto done; }
				ICV_RAW_HARD(iWord,i+1);
				iWord = -1;
				iState = bStrict ? 12 : 0;
				break;
			}
			iState = 9;
			if( nLeft == 1 ){
				bEos = 1;
			}else{
				break;
			}
			/* fall through -- the word ended with the string */
			/* FALLTHROUGH */
		case 9:   /* what follows a complete word */
			if( !bEos && c != '\r' && c != '\n' && c != ' ' && c != '\t' && bStrict ){
				ICV_RAW_HARD(iWord,i+1);
				iState = 12;
				break;
			}
			SyBlobReset(&sWord);
			if( iScheme == ICV_SCHEME_B64 ){
				IcvBase64Decode(&sWord,&z[iText],nText);
			}else if( !IcvQPrintDecode(&sWord,&z[iText],nText,1) ){
				if( !bGoOn ){ rc = ICV_UNKNOWN_ERR; goto done; }
				ICV_RAW_HARD(iWord,i+1);
				iWord = -1;
				iState = bStrict ? 12 : 0;
				break;
			}
			{
				int rcW;
				bTouched = 1;
				rcW = IcvAppendConv(pCtx,pOut,(const char *)SyBlobData(&sWord),
					(int)SyBlobLength(&sWord),sWordCs.iEnc,pTo);
				if( rcW != ICV_OK ){
					if( !bGoOn ){ rc = rcW; goto done; }
					/* php hands the raw word over and carries on. If THAT will
					 * not convert either it keeps the failure in `err` and stays
					 * in this state, so the next character re-runs the whole
					 * branch; when it DOES convert the state moves on below. */
				rc = iWord < 0 ? ICV_OK
						: IcvAppendConv(pCtx,pOut,&z[iWord],i - iWord,ICV_ASCII,pTo);
					bTouched = 1;
					iWord = -1;
					if( rc != ICV_OK ){
						break;
					}
				}
			}
			if( bEos ){
				iState = 0;
				break;
			}
			if( c == '\r' ){ iState = 7; break; }
			if( c == '\n' ){ iState = 8; break; }
			if( c == '=' ){ iWord = i; iState = 1; break; }
			if( c == ' ' || c == '\t' ){ iSpaces = i; iState = 11; break; }
			bTouched = 1;
			ICV_RAW_SILENT(i,i+1);
			iState = 12;
			break;
		case 7:   /* after CR: expecting LF */
			if( c == '\n' ){
				iState = 8;
			}else{
				SyBlobAppend(pOut,"\r",1);
				bTouched = 1;
				ICV_RAW_SILENT(i,i+1);
				iState = 0;
			}
			break;
		case 8:   /* after a newline: is the next line a continuation? */
			if( c != ' ' && c != '\t' ){
				/* The field ended here. Rewinding the position and setting the
				 * count to one leaves *pNext pointing AT this character, so the
				 * caller's next field starts on it. */
				nLeft = 1;
				i--;
				break;
			}
			if( iWord < 0 ){
				SyBlobAppend(pOut," ",1);
				bTouched = 1;
			}
			iSpaces = -1;
			iState = 11;
			break;
		case 10:  /* a language tag after the charset: dismissed */
			if( c == '?' ){ iState = 3; }
			break;
		case 11:  /* a run of whitespace */
			if( c == '\r' ){ iState = 7; break; }
			if( c == '\n' ){ iState = 8; break; }
			if( c == '=' ){
				/* Whitespace BETWEEN two encoded words disappears; whitespace
				 * that followed plain text does not. */
				if( iSpaces >= 0 && iWord < 0 ){
					bTouched = 1;
					ICV_RAW_SILENT(iSpaces,i);
					iSpaces = -1;
				}
				iWord = i;
				iState = 1;
				break;
			}
			if( c == ' ' || c == '\t' ){
				break;
			}
			if( iSpaces >= 0 ){
				ICV_RAW_SILENT(iSpaces,i);
			}
			iSpaces = -1;
			bTouched = 1;
			ICV_RAW_SILENT(i,i+1);
			iWord = -1;
			iState = bStrict ? 12 : 0;
			break;
		case 12:  /* plain text */
			if( c == '\r' ){ iState = 7; break; }
			if( c == '\n' ){ iState = 8; break; }
			if( c == ' ' || c == '\t' ){ iSpaces = i; iState = 11; break; }
			if( c == '=' && !bStrict ){ iWord = i; iState = 1; break; }
			bTouched = 1;
			ICV_RAW_SILENT(i,i+1);
			break;
		}
		nLeft--;
		i++;
	}
	switch( iState ){
	case 0: case 8: case 11: case 12:
		break;
	default:
		if( bGoOn ){
			if( iState == 1 ){
				SyBlobAppend(pOut,"=",1);
			}
			/* CONTINUE_ON_ERROR clears whatever the scan recorded on its way
			 * through, which is why a header full of bytes the output charset
			 * cannot hold comes back as the empty string rather than FALSE. */
			rc = ICV_OK;
		}else{
			rc = ICV_MALFORMED;
		}
		break;
	}
done:
	SyBlobRelease(&sWord);
	if( pNext ){
		*pNext = i;
	}
	if( pbTouched ){
		*pbTouched = bTouched;
	}
	return rc;
}
#undef ICV_RAW_AT
#undef ICV_RAW
#undef ICV_RAW_HARD
#undef ICV_RAW_SILENT

/*
 * Read one option out of an OPTIONS array. php reads three of the five as
 * STRINGS ONLY -- a non-string `scheme`, `input-charset` or `output-charset` is
 * not coerced, it is ignored -- so those need no conversion at all and the
 * array is never touched. The other two ARE coerced, and a coercion has to go
 * through a scratch copy: `ph7_value_to_xxx()` converts the value it is handed,
 * which would rewrite the caller's own array (a recorded defect, four
 * builtins share it).
 */
static const char * IcvOptionRawStr(ph7_value *pOpt,const char *zKey,int *pnOut)
{
	ph7_value *pV = ph7_array_fetch(pOpt,zKey,(int)SyStrlen(zKey));
	if( pV == 0 || !ph7_value_is_string(pV) ){
		return 0;
	}
	return ph7_value_to_string(pV,pnOut);
}
static int IcvOptionInt(ph7_context *pCtx,ph7_value *pOpt,const char *zKey,sxi64 *pOut)
{
	ph7_value *pV = ph7_array_fetch(pOpt,zKey,(int)SyStrlen(zKey));
	if( pV == 0 ){
		return 0;
	}
	SXUNUSED(pCtx);
	*pOut = PH7_ValuePeekInt64(pV);
	return 1;
}
/* Copy one option into pOut as a string, coercing whatever it is. */
static int IcvOptionCopyStr(ph7_context *pCtx,ph7_value *pOpt,const char *zKey,SyBlob *pOut)
{
	ph7_value *pV = ph7_array_fetch(pOpt,zKey,(int)SyStrlen(zKey));
	ph7_value sScratch,*pCopy;
	const char *z;
	int n = 0;
	if( pV == 0 ){
		return 0;
	}
	PH7_MemObjInit(pCtx->pVm,&sScratch);
	pCopy = PH7_ValuePeek(pV,&sScratch);
	z = ph7_value_to_string(pCopy,&n);
	if( n > 0 ){
		SyBlobAppend(pOut,z,(sxu32)n);
	}
	PH7_MemObjRelease(&sScratch);
	return 1;
}

/*
 * string|false iconv_mime_encode(string $field_name, string $field_value,
 *                                array $options = [])
 *
 * The five options php reads, and the exact way it reads them: `scheme` is one
 * LETTER (its first, case-insensitively, and anything but B/b/Q/q leaves the
 * default alone rather than failing), `input-charset` and `output-charset` are
 * only honoured when they are non-empty STRINGS, `line-length` goes through an
 * ordinary int cast, and `line-break-chars` is taken as a string whatever it is.
 */
static int PH7_builtin_iconv_mime_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zName,*zVal,*zLf = "\r\n",*zIn = 0,*zOut = 0;
	int nName,nVal,nLf = 2,nIn = 0,nOut = 0,iScheme = ICV_SCHEME_B64,err;
	sxi64 iMaxLine = 76;
	icv_cs sFrom,sTo;
	SyBlob sOut,sIniLf,sIniIn,sIniOut,sIniCharset;
	ph7_value *pOpt;
	if( nArg < 2 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zName = ph7_value_to_string(apArg[0],&nName);
	zVal = ph7_value_to_string(apArg[1],&nVal);
	SyBlobInit(&sIniLf,&pCtx->pVm->sAllocator);
	SyBlobInit(&sIniIn,&pCtx->pVm->sAllocator);
	SyBlobInit(&sIniOut,&pCtx->pVm->sAllocator);
	SyBlobInit(&sIniCharset,&pCtx->pVm->sAllocator);
	PH7_VmIniGetStr(pCtx->pVm,"default_charset",&sIniCharset);
	zIn = (const char *)SyBlobData(&sIniCharset);
	nIn = (int)SyBlobLength(&sIniCharset);
	zOut = zIn;
	nOut = nIn;
	pOpt = (nArg > 2 && ph7_value_is_array(apArg[2])) ? apArg[2] : 0;
	if( pOpt ){
		const char *zS;
		int nS = 0;
		if( (zS = IcvOptionRawStr(pOpt,"scheme",&nS)) != 0 && nS > 0 ){
			/* One LETTER, case-insensitively, and anything but B or Q leaves the
			 * default where it was rather than failing. */
			int c = IcvUpper((unsigned char)zS[0]);
			if( c == 'B' ){ iScheme = ICV_SCHEME_B64; }
			else if( c == 'Q' ){ iScheme = ICV_SCHEME_QP; }
		}
		if( (zS = IcvOptionRawStr(pOpt,"input-charset",&nS)) != 0 ){
			if( nS >= ICV_CSNMAXLEN ){
				err = ICV_TOO_LONG;
				goto fail;
			}
			if( nS > 0 ){
				SyBlobAppend(&sIniIn,zS,(sxu32)nS);
				zIn = (const char *)SyBlobData(&sIniIn);
				nIn = nS;
			}
		}
		if( (zS = IcvOptionRawStr(pOpt,"output-charset",&nS)) != 0 ){
			if( nS >= ICV_CSNMAXLEN ){
				err = ICV_TOO_LONG;
				goto fail;
			}
			if( nS > 0 ){
				SyBlobAppend(&sIniOut,zS,(sxu32)nS);
				zOut = (const char *)SyBlobData(&sIniOut);
				nOut = nS;
			}
		}
		IcvOptionInt(pCtx,pOpt,"line-length",&iMaxLine);
		if( IcvOptionCopyStr(pCtx,pOpt,"line-break-chars",&sIniLf) ){
			zLf = (const char *)SyBlobData(&sIniLf);
			nLf = (int)SyBlobLength(&sIniLf);
		}
	}
	/* php measures the LINE BUDGET before it opens a converter, so a header
	 * that cannot fit is refused whatever the charsets say. (php compares the
	 * budget as a size_t, so a NEGATIVE `line-length` is a huge one there and it
	 * dies in the allocator instead -- "Possible integer overflow in memory
	 * allocation"; PHL compares signed, so it lands on the same refusal every
	 * other impossible budget gets. Recorded.) */
	if( (sxi64)nName + 2 >= iMaxLine || (sxi64)nOut + 12 >= iMaxLine ){
		err = ICV_TOO_BIG;
		goto fail;
	}
	IcvParseCharset(zIn,nIn,&sFrom);
	IcvParseCharset(zOut,nOut,&sTo);
	if( sFrom.iEnc < 0 || sTo.iEnc < 0 ){
		err = ICV_WRONG_CHARSET;
		goto fail;
	}
	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);
	err = IcvMimeEncode(pCtx,&sOut,zName,nName,zVal,nVal,iMaxLine,zLf,nLf,
		iScheme,&sTo,zOut,nOut,sFrom.iEnc);
	if( err == ICV_OK ){
		ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));
	}
	SyBlobRelease(&sOut);
	if( err != ICV_OK ){
		goto fail;
	}
	SyBlobRelease(&sIniLf); SyBlobRelease(&sIniIn);
	SyBlobRelease(&sIniOut); SyBlobRelease(&sIniCharset);
	return PH7_OK;
fail:
	if( err == ICV_TOO_LONG ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"Encoding parameter exceeds the maximum allowed length of %d characters",
			ICV_CSNMAXLEN);
	}else{
		IcvShowError(pCtx,err,zOut,nOut,zIn,nIn);
	}
	ph7_result_bool(pCtx,0);
	SyBlobRelease(&sIniLf); SyBlobRelease(&sIniIn);
	SyBlobRelease(&sIniOut); SyBlobRelease(&sIniCharset);
	return PH7_OK;
}

/* Resolve the `?string $encoding = null` the two decoders share: php's
 * internal encoding, with only the length cap raised here. */
static int IcvMimeDecodeArgs(ph7_context *pCtx,int nArg,ph7_value **apArg,
	icv_cs *pCs,sxi64 *pMode)
{
	*pMode = 0;
	if( nArg > 1 && !ph7_value_is_null(apArg[1])
	 && PH7_IntArgResolve(pCtx,apArg[1],"iconv_mime_decode",2,"$mode","int",pMode) != PH7_OK ){
		return 0;
	}
	if( !IcvStrEncArg(pCtx,nArg > 2 ? apArg[2] : 0,pCs) ){
		return 0;
	}
	if( pCs->iEnc < 0 ){
		/* php names the source `"???"` here: the decoder learns the real one
		 * word by word, so there is nothing to report but the target. */
		IcvShowError(pCtx,ICV_WRONG_CHARSET,pCs->zName,pCs->nName,"???",3);
		return 0;
	}
	return 1;
}

/* string|false iconv_mime_decode(string $string, int $mode = 0,
 *                                ?string $encoding = null) */
static int PH7_builtin_iconv_mime_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn;
	int nIn,err;
	sxi64 iMode;
	icv_cs sCs;
	SyBlob sOut;
	if( nArg < 1 || !IcvMimeDecodeArgs(pCtx,nArg,apArg,&sCs,&iMode) ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zIn = ph7_value_to_string(apArg[0],&nIn);
	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);
	err = IcvMimeDecode(pCtx,&sOut,zIn,nIn,&sCs,0,0,(int)iMode);
	if( err == ICV_OK ){
		ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));
	}else{
		IcvShowError(pCtx,err,sCs.zName,sCs.nName,"???",3);
		ph7_result_bool(pCtx,0);
	}
	SyBlobRelease(&sOut);
	return PH7_OK;
}

/*
 * array|false iconv_mime_decode_headers(string $headers, int $mode = 0,
 *                                       ?string $encoding = null)
 *
 * Decode one field at a time, splitting each at its FIRST ':' -- a line with
 * none is dropped entirely, which is what ends the walk at the blank line that
 * separates headers from a body. A name seen twice becomes a LIST, and the
 * first value is kept as a plain string until the second arrives.
 */
static int PH7_builtin_iconv_mime_decode_headers(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn;
	int nIn,i = 0,err = ICV_OK;
	sxi64 iMode;
	icv_cs sCs;
	ph7_value *pArray,*pVal;
	if( nArg < 1 || !IcvMimeDecodeArgs(pCtx,nArg,apArg,&sCs,&iMode) ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zIn = ph7_value_to_string(apArg[0],&nIn);
	pArray = ph7_context_new_array(pCtx);
	pVal = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pVal == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	while( i < nIn ){
		SyBlob sLine;
		const char *zLine;
		int nLine,iColon,iNext = 0,k,bTouched = 0;
		SyBlobInit(&sLine,&pCtx->pVm->sAllocator);
		err = IcvMimeDecode(pCtx,&sLine,&zIn[i],nIn - i,&sCs,&iNext,&bTouched,(int)iMode);
		if( err != ICV_OK ){
			SyBlobRelease(&sLine);
			break;
		}
		nLine = (int)SyBlobLength(&sLine);
		if( !bTouched ){
			/* php's buffer is still NULL here -- nothing was appended AT ALL,
			 * not even zero bytes -- and it leaves the loop. That is the blank
			 * line that ends a header block, so a body beyond it is never read;
			 * a field whose value is EMPTY did append and keeps the walk going. */
			SyBlobRelease(&sLine);
			break;
		}
		zLine = (const char *)SyBlobData(&sLine);
		for( iColon = 0 ; iColon < nLine && zLine[iColon] != ':' ; ++iColon ){}
		if( iColon < nLine ){
			ph7_value *pSlot;
			k = iColon + 1;
			while( k < nLine && (zLine[k] == ' ' || zLine[k] == '\t') ){ k++; }
			ph7_value_string(pVal,&zLine[k],nLine - k);
			pSlot = ph7_array_fetch(pArray,zLine,iColon);
			if( pSlot == 0 ){
				PH7_HashmapInsertRawKey((ph7_hashmap *)pArray->x.pOther,
					zLine,(sxu32)iColon,pVal);
			}else{
				/* A name seen twice becomes a LIST -- and the first value was
				 * stored as a plain string, so it has to be lifted into one
				 * now, which is php's own shape for a repeated header. */
				if( !ph7_value_is_array(pSlot) ){
					ph7_value *pList = ph7_context_new_array(pCtx);
					if( pList == 0 ){
						SyBlobRelease(&sLine);
						ph7_context_release_value(pCtx,pVal);
						return PH7_ContextMemoryError(pCtx);
					}
					ph7_array_add_strkey_elem(pList,0,pSlot);
					ph7_array_add_strkey_elem(pList,0,pVal);
					PH7_HashmapInsertRawKey((ph7_hashmap *)pArray->x.pOther,
						zLine,(sxu32)iColon,pList);
					ph7_context_release_value(pCtx,pList);
				}else{
					ph7_array_add_strkey_elem(pSlot,0,pVal);
				}
			}
			ph7_value_reset_string_cursor(pVal);
		}
		SyBlobRelease(&sLine);
		if( iNext <= 0 ){
			break;
		}
		i += iNext;
	}
	ph7_context_release_value(pCtx,pVal);
	if( err != ICV_OK ){
		IcvShowError(pCtx,err,sCs.zName,sCs.nName,"???",3);
		ph7_result_bool(pCtx,0);
	}else{
		ph7_result_value(pCtx,pArray);
	}
	return PH7_OK;
}

PH7_PRIVATE int PH7_builtin_iconv_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_iconv(pCtx,nArg,apArg); }
PH7_PRIVATE int PH7_builtin_iconv_get_encoding_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_iconv_get_encoding(pCtx,nArg,apArg); }
PH7_PRIVATE int PH7_builtin_iconv_mime_encode_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_iconv_mime_encode(pCtx,nArg,apArg); }
PH7_PRIVATE int PH7_builtin_iconv_mime_decode_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_iconv_mime_decode(pCtx,nArg,apArg); }
PH7_PRIVATE int PH7_builtin_iconv_mime_decode_headers_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_iconv_mime_decode_headers(pCtx,nArg,apArg); }
PH7_PRIVATE int PH7_builtin_iconv_strlen_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_iconv_strlen(pCtx,nArg,apArg); }
PH7_PRIVATE int PH7_builtin_iconv_substr_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_iconv_substr(pCtx,nArg,apArg); }
PH7_PRIVATE int PH7_builtin_iconv_strpos_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_iconv_strpos(pCtx,nArg,apArg); }
PH7_PRIVATE int PH7_builtin_iconv_strrpos_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_iconv_strrpos(pCtx,nArg,apArg); }

#endif /* PH7_DISABLE_BUILTIN_FUNC */
