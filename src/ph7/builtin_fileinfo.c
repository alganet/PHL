/**
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
#include <time.h>   /* gzip's header carries a modification time */
/*
 * Section:
 *    php's fileinfo extension: what a program asks a file's TYPE with.
 * Status:
 *    Stable.
 *
 * php's ext/fileinfo is a shell over libmagic plus the magic DATABASE php
 * bundles with it, so its contract has two halves that behave very differently
 * and are worth keeping apart.
 *
 * The API half -- the finfo class, the six functions, the eleven flags, which
 * argument each door screens and how -- is php's own, small, and reproduced
 * whole ("The php layer" below).
 *
 * The ANSWER half is a database, and a database cannot be "ported": php ships
 * ~3MB of compiled magic covering thousands of formats, most of which no php
 * program has ever asked about. So this is the ext/bcmath and ext/gettext
 * precedent again -- PHL carries its OWN signature set, derived from php 8.5.9's
 * answers rather than from libmagic's source, and everything outside it is
 * answered the way libmagic answers a file it does not recognise. What that
 * buys is the same thing deriving gettext bought: the extension is here on every
 * platform, it links nothing, and its answers do not move when the box's
 * /usr/share/misc/magic does.
 *
 * WHAT IS COVERED, and how exactly:
 *
 *   - The TEXT analysis is complete and exact. It is the whole of libmagic's
 *     encoding.c and ascmagic.c: which byte values count as text, the encoding
 *     ladder (ascii, UTF-8 with and without a BOM, BOM'd UTF-16/UTF-32,
 *     ISO-8859, non-ISO extended ASCII, then binary), the charset name each
 *     answers `FILEINFO_MIME_ENCODING` with, and the four annotations a
 *     description carries -- the longest line when it passes 300, the line
 *     terminators unless they are LF alone, an ESC anywhere, a BS anywhere.
 *     This is the path MOST real files take, and every byte of it matches.
 *   - The MIME TYPE and MIME ENCODING faces are exact for every format in the
 *     table below -- which is what a php program consumes: `mime_content_type()`
 *     and `FILEINFO_MIME_TYPE` are the two faces an upload validator, a mail
 *     builder or a static-file server reads.
 *   - The DESCRIPTION face (FILEINFO_NONE) is php's exactly where the
 *     description is a fixed string or a shallow parse -- text, the shebang
 *     scripts, the markup and data formats, PNG, GIF, gzip, bzip2, xz, zstd,
 *     the zip family, ELF, Java class, wasm, the fonts, ar, PDF's version. For
 *     the formats whose description ends in a deep parse of the container --
 *     JPEG past its JFIF header, TIFF's directory, ICO's icon list, an MP3's
 *     frame chain, Ogg's codec, WAV's audio parameters, ISO Media's brand
 *     list, a PDF's page count, RTF's code page, TrueType's table names --
 *     PHL answers the HEAD of php's own sentence and stops where the deep
 *     parse would begin. The type and the encoding are still exact there.
 *
 * WHAT IS NOT: a format outside the table is `data` /
 * `application/octet-stream` if it is binary and plain text if it is text,
 * which is what php answers for anything ITS database does not carry either --
 * the difference is only where the two tables end. `finfo_open()` with a
 * $magic_database argument is refused with php's own "Failed to load magic
 * database" diagnostic: the argument names a file in libmagic's format, and
 * this engine has no reader for one (see FinfoOpenCommon).
 *
 * THE PHP LAYER, all of it measured against php 8.5.9:
 *
 *   - finfo is an ordinary, non-final, subclassable class with no properties,
 *     no constants, no clone handler and no serialize handler -- so `clone`
 *     is "Trying to clone an uncloneable object" and serialize() is
 *     "Serialization of 'finfo' is not allowed". Its one slot is HIDDEN and
 *     holds the flags; an object built by newInstanceWithoutConstructor() has
 *     nothing in it and every verb answers php's `Invalid finfo object` Error.
 *   - The flags argument of file()/buffer() OVERRIDES the object's for that
 *     call only, and only when it is non-zero: FILEINFO_NONE means "keep
 *     mine", which is why `(new finfo(FILEINFO_MIME_TYPE))->file($p, 0)` still
 *     answers a mime type. php restores the object's flags afterwards.
 *   - An empty $filename is a ValueError, and it names ARGUMENT #2 -- with the
 *     name of whatever argument #2 is in the door it was raised through, so
 *     the method form says `Argument #2 ($flags)` and the function form says
 *     `Argument #2 ($filename)`. That is php's own arginfo lookup showing
 *     through a check written for the procedural signature, and it is
 *     reproduced rather than corrected. A NUL byte is the ordinary path
 *     refusal and names argument #1.
 *   - file() consults the STAT of the path before opening it: a directory is
 *     the string `directory` whatever the flags say, mime face included. What
 *     it opens is a php STREAM, so `php://`, `data://` and a userland wrapper
 *     all work, and a path that will not open is php's
 *     `%s(%s): Failed to open stream: %s` warning and false.
 *   - mime_content_type() takes a path OR an open stream, and a stream is read
 *     from its BEGINNING and left exactly where it was found.
 *   - finfo_close() is deprecated in 8.5 and does nothing but answer true.
 */
#ifndef PH7_DISABLE_BUILTIN_FUNC

/* php's FILEINFO_* -- libmagic's MAGIC_* values, which php re-exports as-is. */
#define FINFO_NONE            0x0000000
#define FINFO_SYMLINK         0x0000002
#define FINFO_DEVICES         0x0000008
#define FINFO_MIME_TYPE       0x0000010
#define FINFO_CONTINUE        0x0000020
#define FINFO_PRESERVE_ATIME  0x0000080
#define FINFO_RAW             0x0000100
#define FINFO_MIME_ENCODING   0x0000400
#define FINFO_MIME            (FINFO_MIME_TYPE|FINFO_MIME_ENCODING)
#define FINFO_APPLE           0x0000800
#define FINFO_EXTENSION       0x1000000

/* The hidden slot holding an instance's flags. NULL there means "never
 * constructed", which is php's `Invalid finfo object`. */
#define FINFO_FLAGS_SLOT "__flags"

/* How much of a file the analysis looks at. libmagic's own default parameter
 * (MAGIC_PARAM_BYTES_MAX) is 1MB and php does not change it, so a text file
 * whose first megabyte is clean reads as text under both. */
#define FINFO_READ_MAX 1048576

/*
 * ---------------------------------------------------------------------------
 * The TEXT analysis (libmagic's encoding.c + ascmagic.c)
 * ---------------------------------------------------------------------------
 * Every answer passes through here, not just the ones that end up as `ASCII
 * text`: the MIME ENCODING face is this and nothing else, so a PDF whose bytes
 * happen to be ASCII is `application/pdf; charset=us-ascii` while one carrying
 * a compressed stream is `charset=binary`.
 */
#define FINFO_ENC_BINARY    0
#define FINFO_ENC_ASCII     1
#define FINFO_ENC_UTF8      2
#define FINFO_ENC_UTF8_BOM  3
#define FINFO_ENC_UTF16LE   4
#define FINFO_ENC_UTF16BE   5
#define FINFO_ENC_UTF32LE   6
#define FINFO_ENC_UTF32BE   7
#define FINFO_ENC_LATIN1    8
#define FINFO_ENC_EXTENDED  9

typedef struct FinfoText FinfoText;
struct FinfoText {
	int iEnc;         /* FINFO_ENC_* */
	int bCRLF;        /* saw a CR LF pair */
	int bCR;          /* saw a CR that was not part of one */
	int bLF;          /* saw an LF that was not part of one */
	int bNEL;         /* saw U+0085, the Unicode NEL */
	int bEscape;      /* saw ESC */
	int bOverstrike;  /* saw BS */
	sxu32 nLongest;   /* the longest line, terminator excluded */
};

/*
 * libmagic's text_chars[]: which byte values may appear in a text file. T is
 * text in every encoding, I is text in ISO-8859 (a byte in the 0xA0..0xFF
 * range), X is text only in the "non-ISO extended ASCII" reading, and 0 is
 * never text -- which is what makes a NUL, a 0x7F DEL or a 0x1A anywhere turn
 * the whole file into `data`.
 */
#define FINFO_CH_NONE 0
#define FINFO_CH_T    1
#define FINFO_CH_I    2
#define FINFO_CH_X    3
static const sxu8 aFinfoTextChar[256] = {
	/*         BEL BS  HT  LF  VT  FF  CR                */
	0,0,0,0,0,0,0,1, 1,1,1,1,1,1,0,0,   /* 0x00 */
	/*                     ESC                           */
	0,0,0,0,0,0,0,0, 0,0,0,1,0,0,0,0,   /* 0x10 */
	1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,   /* 0x20 */
	1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,   /* 0x30 */
	1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,   /* 0x40 */
	1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,   /* 0x50 */
	1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,   /* 0x60 */
	1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,0,   /* 0x70 -- 0x7F DEL is not text */
	3,3,3,3,3,3,3,3, 3,3,3,3,3,3,3,3,   /* 0x80 */
	3,3,3,3,3,3,3,3, 3,3,3,3,3,3,3,3,   /* 0x90 */
	2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2,   /* 0xA0 */
	2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2,   /* 0xB0 */
	2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2,   /* 0xC0 */
	2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2,   /* 0xD0 */
	2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2,   /* 0xE0 */
	2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2    /* 0xF0 */
};

static int FinfoLooksAscii(const unsigned char *z,sxu32 n)
{
	sxu32 i;
	for( i = 0 ; i < n ; ++i ){
		if( aFinfoTextChar[z[i]] != FINFO_CH_T ){
			return 0;
		}
	}
	return 1;
}
static int FinfoLooksLatin1(const unsigned char *z,sxu32 n)
{
	sxu32 i;
	for( i = 0 ; i < n ; ++i ){
		sxu8 c = aFinfoTextChar[z[i]];
		if( c != FINFO_CH_T && c != FINFO_CH_I ){
			return 0;
		}
	}
	return 1;
}
static int FinfoLooksExtended(const unsigned char *z,sxu32 n)
{
	sxu32 i;
	for( i = 0 ; i < n ; ++i ){
		if( aFinfoTextChar[z[i]] == FINFO_CH_NONE ){
			return 0;
		}
	}
	return 1;
}
/*
 * A valid UTF-8 sequence, with libmagic's own screens: an overlong form, a
 * surrogate, a code point past U+10FFFF and a truncated tail at the END of the
 * buffer are all refusals (the last one included -- the buffer is what the
 * caller has, and libmagic does not assume more is coming). Answers how many
 * MULTIBYTE sequences were seen, so a pure-ASCII buffer scores 0 and loses to
 * the ascii test that ran before it.
 */
static int FinfoLooksUtf8(const unsigned char *z,sxu32 n,sxu32 *pnMulti)
{
	sxu32 i,nMulti = 0;
	for( i = 0 ; i < n ; ){
		unsigned int c = z[i];
		int nFollow;
		unsigned int uCode;
		if( c < 0x80 ){
			if( aFinfoTextChar[c] == FINFO_CH_NONE ){
				return 0;
			}
			i++;
			continue;
		}
		if( (c & 0xE0) == 0xC0 ){
			nFollow = 1; uCode = c & 0x1F;
		}else if( (c & 0xF0) == 0xE0 ){
			nFollow = 2; uCode = c & 0x0F;
		}else if( (c & 0xF8) == 0xF0 ){
			nFollow = 3; uCode = c & 0x07;
		}else{
			return 0;
		}
		if( i + (sxu32)nFollow >= n ){
			return 0;   /* a tail the buffer does not carry */
		}
		{
			int k;
			for( k = 1 ; k <= nFollow ; ++k ){
				if( (z[i+(sxu32)k] & 0xC0) != 0x80 ){
					return 0;
				}
				uCode = (uCode << 6) | (unsigned int)(z[i+(sxu32)k] & 0x3F);
			}
		}
		if( (nFollow == 1 && uCode < 0x80)
		 || (nFollow == 2 && uCode < 0x800)
		 || (nFollow == 3 && uCode < 0x10000)
		 || uCode > 0x10FFFF
		 || (uCode >= 0xD800 && uCode <= 0xDFFF) ){
			return 0;
		}
		i += (sxu32)nFollow + 1;
		nMulti++;
	}
	if( pnMulti ){
		*pnMulti = nMulti;
	}
	return 1;
}
/*
 * The line-shape half: what the annotations after `ASCII text` are made of.
 * Runs over CODE POINTS rather than bytes so a BOM'd UTF-16 file reports its
 * own terminators -- which is the only reason the NEL (U+0085) counter is here
 * at all, since libmagic only ever sees one in a wide encoding or as UTF-8.
 */
static void FinfoLineShape(FinfoText *pTx,unsigned int c,unsigned int cNext,int *pbSkip,sxu32 *pnLine)
{
	if( c == 0x0D ){
		if( cNext == 0x0A ){
			pTx->bCRLF = 1;
			*pbSkip = 1;
		}else{
			pTx->bCR = 1;
		}
		if( *pnLine > pTx->nLongest ){
			pTx->nLongest = *pnLine;
		}
		*pnLine = 0;
		return;
	}
	if( c == 0x0A ){
		pTx->bLF = 1;
		if( *pnLine > pTx->nLongest ){
			pTx->nLongest = *pnLine;
		}
		*pnLine = 0;
		return;
	}
	if( c == 0x85 ){
		pTx->bNEL = 1;
		if( *pnLine > pTx->nLongest ){
			pTx->nLongest = *pnLine;
		}
		*pnLine = 0;
		return;
	}
	if( c == 0x1B ){
		pTx->bEscape = 1;
	}else if( c == 0x08 ){
		pTx->bOverstrike = 1;
	}
	(*pnLine)++;
}
/*
 * The whole text reading of a buffer: the encoding ladder first, then the line
 * shape over whatever code points that encoding gives.
 */
static void FinfoTextScan(const unsigned char *z,sxu32 n,FinfoText *pTx)
{
	sxu32 i,nLine = 0,nMulti = 0;
	int bWide = 0,bSwap = 0,nUnit = 0;
	SyZero(pTx,sizeof(*pTx));
	pTx->iEnc = FINFO_ENC_BINARY;
	if( n < 1 ){
		pTx->iEnc = FINFO_ENC_ASCII;
		return;
	}
	if( FinfoLooksAscii(z,n) ){
		pTx->iEnc = FINFO_ENC_ASCII;
	}else if( n > 3 && z[0] == 0xEF && z[1] == 0xBB && z[2] == 0xBF
	       && FinfoLooksUtf8(z+3,n-3,&nMulti) ){
		pTx->iEnc = FINFO_ENC_UTF8_BOM;
	}else if( FinfoLooksUtf8(z,n,&nMulti) && nMulti > 0 ){
		pTx->iEnc = FINFO_ENC_UTF8;
	}else if( n >= 4 && z[0] == 0xFF && z[1] == 0xFE && z[2] == 0x00 && z[3] == 0x00 ){
		pTx->iEnc = FINFO_ENC_UTF32LE; bWide = 1; nUnit = 4; bSwap = 0;
	}else if( n >= 4 && z[0] == 0x00 && z[1] == 0x00 && z[2] == 0xFE && z[3] == 0xFF ){
		pTx->iEnc = FINFO_ENC_UTF32BE; bWide = 1; nUnit = 4; bSwap = 1;
	}else if( n >= 2 && z[0] == 0xFF && z[1] == 0xFE ){
		pTx->iEnc = FINFO_ENC_UTF16LE; bWide = 1; nUnit = 2; bSwap = 0;
	}else if( n >= 2 && z[0] == 0xFE && z[1] == 0xFF ){
		pTx->iEnc = FINFO_ENC_UTF16BE; bWide = 1; nUnit = 2; bSwap = 1;
	}else if( FinfoLooksLatin1(z,n) ){
		pTx->iEnc = FINFO_ENC_LATIN1;
	}else if( FinfoLooksExtended(z,n) ){
		pTx->iEnc = FINFO_ENC_EXTENDED;
	}else{
		return;   /* binary: nothing else is measured */
	}
	if( bWide ){
		/* The BOM itself is not part of the text. A wide encoding's own
		 * annotations are measured over its code points, which is why
		 * "\xff\xfeh\0i\0" answers "with no line terminators" rather than
		 * finding the NULs interesting. */
		for( i = (sxu32)nUnit ; i + (sxu32)nUnit <= n ; i += (sxu32)nUnit ){
			unsigned int c = 0,cNext = 0;
			int bSkip = 0,k;
			for( k = 0 ; k < nUnit ; ++k ){
				unsigned int b = z[i + (sxu32)(bSwap ? k : nUnit - 1 - k)];
				c = (c << 8) | b;
			}
			if( i + (sxu32)(2*nUnit) <= n ){
				for( k = 0 ; k < nUnit ; ++k ){
					unsigned int b = z[i + (sxu32)nUnit + (sxu32)(bSwap ? k : nUnit - 1 - k)];
					cNext = (cNext << 8) | b;
				}
			}
			FinfoLineShape(pTx,c,cNext,&bSkip,&nLine);
			if( bSkip ){
				i += (sxu32)nUnit;
			}
		}
	}else{
		sxu32 nStart = (pTx->iEnc == FINFO_ENC_UTF8_BOM) ? 3 : 0;
		for( i = nStart ; i < n ; ++i ){
			unsigned int c = z[i];
			int bSkip = 0;
			/* U+0085 reaches an 8-bit scan as the two bytes C2 85 in UTF-8, and
			 * as the single byte 0x85 in a Latin-1 file -- which libmagic does
			 * NOT count, since 0x85 is an ordinary high byte there. */
			if( (pTx->iEnc == FINFO_ENC_UTF8 || pTx->iEnc == FINFO_ENC_UTF8_BOM)
			 && c == 0xC2 && i + 1 < n && z[i+1] == 0x85 ){
				FinfoLineShape(pTx,0x85,0,&bSkip,&nLine);
				i++;
				continue;
			}
			if( c >= 0x80 && (pTx->iEnc == FINFO_ENC_UTF8 || pTx->iEnc == FINFO_ENC_UTF8_BOM) ){
				/* A multibyte character is ONE column, and never a terminator. */
				if( (c & 0xC0) != 0x80 ){
					nLine++;
				}
				continue;
			}
			FinfoLineShape(pTx,c,(i + 1 < n) ? z[i+1] : 0,&bSkip,&nLine);
			if( bSkip ){
				i++;
			}
		}
	}
	if( nLine > pTx->nLongest ){
		pTx->nLongest = nLine;
	}
}
/* The charset name FILEINFO_MIME_ENCODING answers with. */
static const char * FinfoCharset(int iEnc)
{
	switch( iEnc ){
		case FINFO_ENC_ASCII:    return "us-ascii";
		case FINFO_ENC_UTF8:
		case FINFO_ENC_UTF8_BOM: return "utf-8";
		case FINFO_ENC_UTF16LE:  return "utf-16le";
		case FINFO_ENC_UTF16BE:  return "utf-16be";
		case FINFO_ENC_UTF32LE:  return "utf-32le";
		case FINFO_ENC_UTF32BE:  return "utf-32be";
		case FINFO_ENC_LATIN1:   return "iso-8859-1";
		case FINFO_ENC_EXTENDED: return "unknown-8bit";
		default:                 break;
	}
	return "binary";
}
/* The head of a text description: the encoding's own name for itself. */
static const char * FinfoEncDesc(int iEnc)
{
	switch( iEnc ){
		case FINFO_ENC_ASCII:    return "ASCII text";
		case FINFO_ENC_UTF8:     return "Unicode text, UTF-8 text";
		case FINFO_ENC_UTF8_BOM: return "Unicode text, UTF-8 (with BOM) text";
		case FINFO_ENC_UTF16LE:  return "Unicode text, UTF-16, little-endian text";
		case FINFO_ENC_UTF16BE:  return "Unicode text, UTF-16, big-endian text";
		/* The two UCS-4 readings end there: libmagic prints no ` text` and no
		 * annotations after them. */
		case FINFO_ENC_UTF32LE:  return "Unicode text, UTF-32, little-endian";
		case FINFO_ENC_UTF32BE:  return "Unicode text, UTF-32, big-endian";
		case FINFO_ENC_LATIN1:   return "ISO-8859 text";
		case FINFO_ENC_EXTENDED: return "Non-ISO extended-ASCII text";
		default:                 break;
	}
	return "data";
}
/*
 * The annotations, in libmagic's order: the long line, then the terminators,
 * then the escapes, then the overstriking. A file whose only terminator is LF
 * says nothing at all -- that is the ordinary case and libmagic treats it as
 * the default rather than as news.
 */
static void FinfoTextDesc(const FinfoText *pTx,SyBlob *pOut)
{
	SyBlobAppend(pOut,FinfoEncDesc(pTx->iEnc),SyStrlen(FinfoEncDesc(pTx->iEnc)));
	if( pTx->iEnc == FINFO_ENC_UTF32LE || pTx->iEnc == FINFO_ENC_UTF32BE ){
		return;
	}
	if( pTx->nLongest > 300 ){
		SyBlobFormat(pOut,", with very long lines (%u)",pTx->nLongest);
	}
	if( !pTx->bCRLF && !pTx->bCR && !pTx->bLF && !pTx->bNEL ){
		SyBlobAppend(pOut,", with no line terminators",sizeof(", with no line terminators")-1);
	}else if( pTx->bCRLF || pTx->bCR || pTx->bNEL ){
		int bFirst = 1;
		SyBlobAppend(pOut,", with",sizeof(", with")-1);
		if( pTx->bCRLF ){
			SyBlobAppend(pOut," CRLF",5); bFirst = 0;
		}
		if( pTx->bCR ){
			SyBlobAppend(pOut,bFirst ? " CR" : ", CR",bFirst ? 3 : 4); bFirst = 0;
		}
		if( pTx->bLF ){
			SyBlobAppend(pOut,bFirst ? " LF" : ", LF",bFirst ? 3 : 4); bFirst = 0;
		}
		if( pTx->bNEL ){
			SyBlobAppend(pOut,bFirst ? " NEL" : ", NEL",bFirst ? 4 : 5);
		}
		SyBlobAppend(pOut," line terminators",sizeof(" line terminators")-1);
	}
	if( pTx->bEscape ){
		SyBlobAppend(pOut,", with escape sequences",sizeof(", with escape sequences")-1);
	}
	if( pTx->bOverstrike ){
		SyBlobAppend(pOut,", with overstriking",sizeof(", with overstriking")-1);
	}
}
/*
 * ---------------------------------------------------------------------------
 * The signature set
 * ---------------------------------------------------------------------------
 * One answer, three faces. zMime and zExt are what the MIME TYPE and EXTENSION
 * flags hand back; the description is built in sDesc, and bText asks for the
 * text analysis to be appended to it (which is how `HTML document, ASCII text`
 * and `PHP script, Unicode text, UTF-8 text` are made). bExec adds libmagic's
 * ` executable` after that, which only a #! line asks for.
 */
typedef struct FinfoAnswer FinfoAnswer;
struct FinfoAnswer {
	SyBlob sDesc;
	const char *zMime;
	const char *zExt;
	int bText;   /* 1: append ", " and the text analysis. 2: append it directly. 3:
	              * append the ENCODING NAME alone, with none of the annotations
	              * after it -- libmagic's CSV row is the only one that does, and
	              * a CSV file with CRLF endings is where it shows. Otherwise 2 --
	              * the row's own prefix already ends in the separator libmagic's
	              * entry carries (`CSV ` has a space where `PHP script, ` has a
	              * comma), which is why this is not a boolean. */
	int bExec;
};

static sxu32 FinfoBe16(const unsigned char *z){ return ((sxu32)z[0]<<8)|(sxu32)z[1]; }
static sxu32 FinfoLe16(const unsigned char *z){ return ((sxu32)z[1]<<8)|(sxu32)z[0]; }
static sxu32 FinfoBe32(const unsigned char *z)
{
	return ((sxu32)z[0]<<24)|((sxu32)z[1]<<16)|((sxu32)z[2]<<8)|(sxu32)z[3];
}
static sxu32 FinfoLe32(const unsigned char *z)
{
	return ((sxu32)z[3]<<24)|((sxu32)z[2]<<16)|((sxu32)z[1]<<8)|(sxu32)z[0];
}
/* Does the buffer carry this literal at this offset? */
static int FinfoAt(const unsigned char *z,sxu32 n,sxu32 nOfft,const char *zLit,sxu32 nLit)
{
	/* Subtraction rather than addition: some of these offsets are read OUT of
	 * the file being examined (a PE header pointer, an EBML element), and
	 * `nOfft + nLit` wraps for a crafted one. */
	if( nOfft > n || nLit > n - nOfft ){
		return 0;
	}
	return SyMemcmp(z + nOfft,zLit,nLit) == 0;
}
#define FINFO_AT(z,n,o,lit) FinfoAt(z,n,(sxu32)(o),lit,(sxu32)(sizeof(lit)-1))
/* Where this literal first appears within the first nLimit bytes, or -1. */
static sxi32 FinfoFind(const unsigned char *z,sxu32 n,sxu32 nLimit,const char *zLit,sxu32 nLit)
{
	sxu32 i;
	if( nLimit > n ){
		nLimit = n;
	}
	if( nLit < 1 || nLit > nLimit ){
		return -1;
	}
	for( i = 0 ; i + nLit <= nLimit ; ++i ){
		if( z[i] == (unsigned char)zLit[0] && SyMemcmp(z + i,zLit,nLit) == 0 ){
			return (sxi32)i;
		}
	}
	return -1;
}
#define FINFO_FIND(z,n,lim,lit) FinfoFind(z,n,(sxu32)(lim),lit,(sxu32)(sizeof(lit)-1))
/* Case-insensitive (ASCII) compare of a literal at an offset. */
static int FinfoAtNoCase(const unsigned char *z,sxu32 n,sxu32 nOfft,const char *zLit,sxu32 nLit)
{
	sxu32 i;
	if( nOfft > n || nLit > n - nOfft ){
		return 0;
	}
	for( i = 0 ; i < nLit ; ++i ){
		if( SyToLower(z[nOfft+i]) != SyToLower((unsigned char)zLit[i]) ){
			return 0;
		}
	}
	return 1;
}
#define FINFO_ATI(z,n,o,lit) FinfoAtNoCase(z,n,(sxu32)(o),lit,(sxu32)(sizeof(lit)-1))
static void FinfoSay(FinfoAnswer *pOut,const char *zText)
{
	SyBlobAppend(&pOut->sDesc,zText,SyStrlen(zText));
}
/*
 * ELF: what the header says about itself. php's answer stops at the version and
 * the OS ABI -- the note-walking that gives file(1) its `dynamically linked,
 * interpreter ..., BuildID[sha1]=...` tail is not in php's bundled database, so
 * a whole ELF answer is these five fields and nothing more.
 */
static int FinfoElf(const unsigned char *z,sxu32 n,ph7_int64 nFile,FinfoAnswer *pOut)
{
	int bClass64,bLsb;
	sxu32 nType,nMachine,nVersion,nOsabi;
	const char *zMachine = 0;
	int bInterp = 0;
	if( n < 20 || !FINFO_AT(z,n,0,"\177ELF") ){
		return 0;
	}
	bClass64 = (z[4] == 2);
	bLsb = (z[5] == 1);
	nOsabi = z[7];
	nType = bLsb ? FinfoLe16(z+16) : FinfoBe16(z+16);
	nMachine = bLsb ? FinfoLe16(z+18) : FinfoBe16(z+18);
	nVersion = (n >= 24) ? (bLsb ? FinfoLe32(z+20) : FinfoBe32(z+20)) : 1;
	SyBlobFormat(&pOut->sDesc,"ELF %s-bit %sSB ",bClass64 ? "64" : "32",bLsb ? "L" : "M");
	/* An ET_DYN with a PT_INTERP is what a modern toolchain builds an ordinary
	 * program as, and php calls THAT a `pie executable` -- a shared library,
	 * which has the same object type and no interpreter, stays a `shared
	 * object`. Walking the program headers is the only way to tell them apart,
	 * and php only does it for a FILE: the same bytes handed to buffer() are a
	 * `shared object` there, which is measured rather than assumed. */
	if( nType == 3 && n > 64 && nFile >= 0 ){
		sxu32 nPhOff,nPhEntSize,nPhNum,i;
		if( bClass64 ){
			nPhOff = bLsb ? FinfoLe32(z+32) : FinfoBe32(z+36);
			nPhEntSize = bLsb ? FinfoLe16(z+54) : FinfoBe16(z+54);
			nPhNum = bLsb ? FinfoLe16(z+56) : FinfoBe16(z+56);
		}else{
			nPhOff = bLsb ? FinfoLe32(z+28) : FinfoBe32(z+28);
			nPhEntSize = bLsb ? FinfoLe16(z+42) : FinfoBe16(z+42);
			nPhNum = bLsb ? FinfoLe16(z+44) : FinfoBe16(z+44);
		}
		for( i = 0 ; i < nPhNum && nPhEntSize > 0 ; ++i ){
			sxu32 nAt = nPhOff + i*nPhEntSize;
			sxu32 nPType;
			if( nAt + 4 > n ){
				break;
			}
			nPType = bLsb ? FinfoLe32(z+nAt) : FinfoBe32(z+nAt);
			if( nPType == 3 ){   /* PT_INTERP */
				bInterp = 1;
				break;
			}
		}
	}
	switch( nType ){
		case 1: FinfoSay(pOut,"relocatable"); pOut->zMime = "application/x-object"; break;
		case 2: FinfoSay(pOut,"executable"); pOut->zMime = "application/x-executable"; break;
		case 3:
			if( bInterp ){
				FinfoSay(pOut,"pie executable");
				pOut->zMime = "application/x-pie-executable";
			}else{
				FinfoSay(pOut,"shared object");
				pOut->zMime = "application/x-sharedlib";
			}
			break;
		case 4: FinfoSay(pOut,"core file"); pOut->zMime = "application/x-coredump"; break;
		default: FinfoSay(pOut,"processor-specific"); pOut->zMime = "application/octet-stream"; break;
	}
	switch( nMachine ){
		case 3:   zMachine = "Intel 80386"; break;
		case 8:   zMachine = "MIPS"; break;
		case 20:  zMachine = "PowerPC"; break;
		case 21:  zMachine = "64-bit PowerPC"; break;
		case 22:  zMachine = "IBM S/390"; break;
		case 40:  zMachine = "ARM"; break;
		case 42:  zMachine = "Renesas SH"; break;
		case 43:  zMachine = "SPARC V9"; break;
		case 50:  zMachine = "Intel IA-64"; break;
		case 62:  zMachine = "x86-64"; break;
		case 183: zMachine = "ARM aarch64"; break;
		case 243: zMachine = "UCB RISC-V"; break;
		default:  break;
	}
	if( zMachine ){
		SyBlobFormat(&pOut->sDesc,", %s",zMachine);
	}
	SyBlobFormat(&pOut->sDesc,", version %u",nVersion);
	switch( nOsabi ){
		case 0:  FinfoSay(pOut," (SYSV)"); break;
		case 1:  FinfoSay(pOut," (HP-UX)"); break;
		case 2:  FinfoSay(pOut," (NetBSD)"); break;
		case 3:  FinfoSay(pOut," (GNU/Linux)"); break;
		case 6:  FinfoSay(pOut," (Solaris)"); break;
		case 9:  FinfoSay(pOut," (FreeBSD)"); break;
		case 12: FinfoSay(pOut," (OpenBSD)"); break;
		default: break;
	}
	return 1;
}
/*
 * The zip container is four formats: an OpenDocument or an EPUB names itself in
 * an uncompressed `mimetype` member the format REQUIRES to be first, a JAR
 * carries META-INF/ and an OOXML document [Content_Types].xml -- and everything
 * else is a plain archive.
 */
static int FinfoZip(const unsigned char *z,sxu32 n,FinfoAnswer *pOut)
{
	sxu32 nNameLen,nExtraLen,nVer,nMethod;
	const unsigned char *zName;
	if( FINFO_AT(z,n,0,"PK\005\006") ){
		FinfoSay(pOut,"Zip archive data (empty)");
		pOut->zMime = "application/zip";
		pOut->zExt = "zip/cbz";
		return 1;
	}
	if( n < 30 || !FINFO_AT(z,n,0,"PK\003\004") ){
		return 0;
	}
	nVer = FinfoLe16(z+4);
	nMethod = FinfoLe16(z+8);
	nNameLen = FinfoLe16(z+26);
	nExtraLen = FinfoLe16(z+28);
	zName = z + 30;
	if( 30 + nNameLen <= n ){
		if( nNameLen == 8 && SyMemcmp(zName,"mimetype",8) == 0 ){
			sxu32 nAt = 30 + nNameLen + nExtraLen;
			static const struct { const char *zType; const char *zDesc; const char *zExt; } aOdf[] = {
				{ "application/vnd.oasis.opendocument.text",         "OpenDocument Text", "odt" },
				{ "application/vnd.oasis.opendocument.spreadsheet",  "OpenDocument Spreadsheet", "ods" },
				{ "application/vnd.oasis.opendocument.presentation", "OpenDocument Presentation", "odp" },
				{ "application/vnd.oasis.opendocument.graphics",     "OpenDocument Drawing", "odg" },
				{ "application/epub+zip",                            "EPUB document", "epub" },
			};
			sxu32 i;
			for( i = 0 ; i < SX_ARRAYSIZE(aOdf) ; ++i ){
				sxu32 nLen = (sxu32)SyStrlen(aOdf[i].zType);
				if( nAt + nLen <= n && SyMemcmp(z + nAt,aOdf[i].zType,nLen) == 0 ){
					FinfoSay(pOut,aOdf[i].zDesc);
					pOut->zMime = aOdf[i].zType;
					pOut->zExt = aOdf[i].zExt;
					return 1;
				}
			}
		}
		if( nNameLen >= 9 && SyMemcmp(zName,"META-INF/",9) == 0 ){
			FinfoSay(pOut,"Java archive data (JAR)");
			pOut->zMime = "application/java-archive";
			pOut->zExt = "jar";
			return 1;
		}
		if( nNameLen == 19 && SyMemcmp(zName,"[Content_Types].xml",19) == 0 ){
			static const struct { const char *zDir; const char *zDesc; const char *zMime; const char *zExt; } aOox[] = {
				{ "word/",  "Microsoft Word 2007+",
				  "application/vnd.openxmlformats-officedocument.wordprocessingml.document", "docx" },
				{ "xl/",    "Microsoft Excel 2007+",
				  "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet", "xlsx" },
				{ "ppt/",   "Microsoft PowerPoint 2007+",
				  "application/vnd.openxmlformats-officedocument.presentationml.presentation", "pptx" },
			};
			sxu32 i;
			for( i = 0 ; i < SX_ARRAYSIZE(aOox) ; ++i ){
				if( FinfoFind(z,n,n,aOox[i].zDir,(sxu32)SyStrlen(aOox[i].zDir)) >= 0 ){
					FinfoSay(pOut,aOox[i].zDesc);
					pOut->zMime = aOox[i].zMime;
					pOut->zExt = aOox[i].zExt;
					return 1;
				}
			}
		}
	}
	SyBlobFormat(&pOut->sDesc,"Zip archive data, at least v%u.%u to extract",
		nVer / 10,nVer % 10);
	switch( nMethod ){
		case 0:  FinfoSay(pOut,", compression method=store"); break;
		case 6:  FinfoSay(pOut,", compression method=implode"); break;
		case 8:  FinfoSay(pOut,", compression method=deflate"); break;
		case 9:  FinfoSay(pOut,", compression method=deflate64"); break;
		case 12: FinfoSay(pOut,", compression method=bzip2"); break;
		case 14: FinfoSay(pOut,", compression method=LZMA"); break;
		case 93: FinfoSay(pOut,", compression method=Zstandard"); break;
		case 95: FinfoSay(pOut,", compression method=XZ"); break;
		case 98: FinfoSay(pOut,", compression method=PPMd"); break;
		default: break;
	}
	pOut->zMime = "application/zip";
	return 1;
}
/*
 * gzip carries a header full of optional fields, and php prints every one it
 * finds: the original NAME, the modification time in LOCAL time, the deflate
 * effort, the operating system that wrote it, and -- only when the whole FILE
 * is at hand -- the last four bytes, which hold the uncompressed length mod
 * 2^32. That last one is why `finfo::file()` and `finfo::buffer()` answer
 * differently for the same bytes.
 */
static void FinfoCtime(SyBlob *pOut,sxu32 nEpoch)
{
	static const char *azDay[] = { "Sun","Mon","Tue","Wed","Thu","Fri","Sat" };
	static const char *azMon[] = { "Jan","Feb","Mar","Apr","May","Jun",
	                               "Jul","Aug","Sep","Oct","Nov","Dec" };
	time_t t = (time_t)nEpoch;
	struct tm sTm;
	struct tm *pTm;
	/* UTC, not the box's zone: php's entry for this field is a little-endian
	 * DATE (`ledate`), and libmagic renders those in GMT -- only the `l`-prefixed
	 * spelling is local. Measured, because the two differ by hours here. The
	 * reentrant forms take their arguments in opposite orders, the same split
	 * builtin_calendar.c carries. */
#ifdef __WINNT__
	pTm = (gmtime_s(&sTm,&t) == 0) ? &sTm : 0;
#else
	pTm = gmtime_r(&t,&sTm);
#endif
	if( pTm == 0 ){
		SyBlobFormat(pOut,"%u",nEpoch);
		return;
	}
	SyBlobFormat(pOut,"%s %s %s%u %s%u:%s%u:%s%u %u",
		azDay[pTm->tm_wday % 7],azMon[pTm->tm_mon % 12],
		pTm->tm_mday < 10 ? " " : "",(sxu32)pTm->tm_mday,
		pTm->tm_hour < 10 ? "0" : "",(sxu32)pTm->tm_hour,
		pTm->tm_min < 10 ? "0" : "",(sxu32)pTm->tm_min,
		pTm->tm_sec < 10 ? "0" : "",(sxu32)pTm->tm_sec,
		(sxu32)(pTm->tm_year + 1900));
}
static int FinfoGzip(const unsigned char *z,sxu32 n,ph7_int64 nFile,FinfoAnswer *pOut)
{
	sxu32 nFlags,nMtime,nXfl,nOs;
	if( n < 10 || z[0] != 0x1F || z[1] != 0x8B ){
		return 0;
	}
	if( z[2] != 8 ){
		return 0;   /* only deflate has ever been used, and php names no other */
	}
	nFlags = z[3];
	nMtime = FinfoLe32(z+4);
	nXfl = z[8];
	nOs = z[9];
	FinfoSay(pOut,"gzip compressed data");
	if( nFlags & 0x08 ){    /* FNAME: a NUL-terminated original name */
		sxu32 i = 10;
		if( nFlags & 0x04 ){   /* FEXTRA sits before it */
			if( i + 2 <= n ){
				i += 2 + FinfoLe16(z+i);
			}
		}
		if( i < n ){
			sxu32 nStart = i;
			while( i < n && z[i] != 0 ){
				i++;
			}
			if( i > nStart ){
				SyBlobFormat(&pOut->sDesc,", was \"%.*s\"",(int)(i - nStart),(const char *)(z + nStart));
			}
		}
	}
	if( nMtime > 0 ){
		FinfoSay(pOut,", last modified: ");
		FinfoCtime(&pOut->sDesc,nMtime);
	}
	if( nXfl == 2 ){
		FinfoSay(pOut,", max compression");
	}else if( nXfl == 4 ){
		FinfoSay(pOut,", max speed");
	}
	switch( nOs ){
		case 0:  FinfoSay(pOut,", from FAT filesystem (MS-DOS, OS/2, NT)"); break;
		case 1:  FinfoSay(pOut,", from Amiga"); break;
		case 2:  FinfoSay(pOut,", from VMS"); break;
		case 3:  FinfoSay(pOut,", from Unix"); break;
		case 4:  FinfoSay(pOut,", from VM/CMS"); break;
		case 5:  FinfoSay(pOut,", from Atari"); break;
		case 6:  FinfoSay(pOut,", from HPFS filesystem (OS/2, NT)"); break;
		case 7:  FinfoSay(pOut,", from Macintosh"); break;
		case 8:  FinfoSay(pOut,", from Z-System"); break;
		case 9:  FinfoSay(pOut,", from CP/M"); break;
		case 10: FinfoSay(pOut,", from TOPS/20"); break;
		case 11: FinfoSay(pOut,", from NTFS filesystem (NT)"); break;
		case 12: FinfoSay(pOut,", from QDOS"); break;
		case 13: FinfoSay(pOut,", from Acorn RISCOS"); break;
		default: break;
	}
	/* The trailer is only readable when the object being examined IS the whole
	 * file -- which is the one thing buffer() cannot know. */
	if( nFile >= 18 && (ph7_int64)n >= nFile ){
		SyBlobFormat(&pOut->sDesc,", original size modulo 2^32 %u",FinfoLe32(z + (sxu32)nFile - 4));
		/* php's extension list hangs off that same branch of its entry, so the
		 * buffer door answers `???` for bytes the file door names. */
		pOut->zExt = "gz/tgz/tpz/ipk/vbox-extpack/svgz/blend/dia/gnucash/rdata/xoj";
	}
	pOut->zMime = "application/gzip";
	return 1;
}
/*
 * An MPEG audio frame header, which php spells out in full: the version, the
 * bitrate, the sample rate and the channel mode all live in four bytes, and
 * every one of them is a table lookup. Shared by the bare frame and by the
 * `contains:` clause an ID3 tag puts in front of one.
 */
static int FinfoMpegFrame(const unsigned char *z,sxu32 n,sxu32 nAt,const char *zLead,
	FinfoAnswer *pOut)
{
	static const sxu16 aRateV1[16] = { 0,32,40,48,56,64,80,96,112,128,160,192,224,256,320,0 };
	static const sxu16 aRateV2[16] = { 0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,0 };
	static const char *azHzV1[4]   = { ", 44.1 kHz", ", 48 kHz", ", 32 kHz", 0 };
	static const char *azHzV2[4]   = { ", 22.05 kHz", ", 24 kHz", ", 16 kHz", 0 };
	static const char *azHzV25[4]  = { ", 11.025 kHz", ", 12 kHz", ", 8 kHz", 0 };
	static const char *azMode[4]   = { ", Stereo", ", JntStereo", ", 2x Monaural", ", Monaural" };
	sxu32 nVer,nRate,nHz,nMode;
	if( nAt + 4 > n || z[nAt] != 0xFF || (z[nAt+1] & 0xE0) != 0xE0 ){
		return 0;
	}
	nVer = (z[nAt+1] >> 3) & 3;
	if( nVer == 1 || (z[nAt+1] & 0x06) != 0x02 ){
		return 0;   /* the reserved version, or a layer that is not III */
	}
	nRate = (z[nAt+2] >> 4) & 0x0F;
	nHz = (z[nAt+2] >> 2) & 3;
	nMode = (z[nAt+3] >> 6) & 3;
	if( nRate == 0 || nRate == 15 || nHz == 3 ){
		return 0;
	}
	/* Nothing is written before every screen above has passed: the ID3 caller
	 * only earns its `, contains: ` when a frame really follows the tag. */
	if( zLead ){
		FinfoSay(pOut,zLead);
	}
	FinfoSay(pOut,"MPEG ADTS, layer III");
	FinfoSay(pOut,nVer == 3 ? ", v1" : (nVer == 2 ? ", v2" : ", v2.5"));
	SyBlobFormat(&pOut->sDesc,", %u kbps",
		(sxu32)(nVer == 3 ? aRateV1[nRate] : aRateV2[nRate]));
	FinfoSay(pOut,nVer == 3 ? azHzV1[nHz] : (nVer == 2 ? azHzV2[nHz] : azHzV25[nHz]));
	FinfoSay(pOut,azMode[nMode]);
	return 1;
}
/*
 * Every other binary signature, in the order libmagic's strength ordering
 * happens to try them: a longer, more specific magic before a shorter one that
 * would also match.
 */
static int FinfoBinary(const unsigned char *z,sxu32 n,ph7_int64 nFile,FinfoAnswer *pOut)
{
	if( n < 4 ){
		return 0;
	}
	if( FINFO_AT(z,n,0,"\211PNG\r\n\032\n") ){
		FinfoSay(pOut,"PNG image data");
		if( n >= 33 && FINFO_AT(z,n,12,"IHDR") ){
			static const char *azColor[] = { " grayscale", 0, "/color RGB", " colormap",
			                                 " gray+alpha", 0, "/color RGBA" };
			sxu32 nColor = z[25];
			SyBlobFormat(&pOut->sDesc,", %u x %u, %u-bit",FinfoBe32(z+16),FinfoBe32(z+20),(sxu32)z[24]);
			if( nColor < SX_ARRAYSIZE(azColor) && azColor[nColor] ){
				FinfoSay(pOut,azColor[nColor]);
			}
			FinfoSay(pOut,z[28] ? ", interlaced" : ", non-interlaced");
		}
		pOut->zMime = "image/png";
		pOut->zExt = "png";
		return 1;
	}
	if( FINFO_AT(z,n,0,"GIF87a") || FINFO_AT(z,n,0,"GIF89a") ){
		SyBlobFormat(&pOut->sDesc,"GIF image data, version %.3s",(const char *)(z + 3));
		if( n >= 10 ){
			SyBlobFormat(&pOut->sDesc,", %u x %u",FinfoLe16(z+6),FinfoLe16(z+8));
		}
		pOut->zMime = "image/gif";
		pOut->zExt = "gif";
		return 1;
	}
	if( z[0] == 0xFF && z[1] == 0xD8 && z[2] == 0xFF ){
		FinfoSay(pOut,"JPEG image data");
		/* The JFIF APP0 sits at a fixed offset and its four fields are read
		 * there; the comment and the frame are found by walking the segment
		 * chain, which is what php's own entry does. */
		if( FINFO_AT(z,n,6,"JFIF\0") && n >= 18 ){
			SyBlobFormat(&pOut->sDesc,", JFIF standard %u.%s%u",(sxu32)z[11],
				z[12] < 10 ? "0" : "",(sxu32)z[12]);
			switch( z[13] ){
				case 0: FinfoSay(pOut,", aspect ratio"); break;
				case 1: FinfoSay(pOut,", resolution (DPI)"); break;
				case 2: FinfoSay(pOut,", resolution (DPCM)"); break;
				default: break;
			}
			SyBlobFormat(&pOut->sDesc,", density %ux%u",FinfoBe16(z+14),FinfoBe16(z+16));
			SyBlobFormat(&pOut->sDesc,", segment length %u",FinfoBe16(z+4));
		}
		{
			sxu32 i = 2;
			while( i + 4 <= n && z[i] == 0xFF ){
				sxu32 nMark = z[i+1];
				sxu32 nLen = FinfoBe16(z+i+2);
				if( nMark == 0xD8 || nMark == 0x01 || (nMark >= 0xD0 && nMark <= 0xD7) ){
					i += 2;
					continue;
				}
				if( nMark == 0xDA || nMark == 0xD9 || nLen < 2 ){
					break;   /* the entropy-coded scan: nothing else is a segment */
				}
				if( nMark == 0xFE && i + 4 < n ){
					sxu32 nText = nLen - 2;
					if( i + 4 + nText > n ){
						nText = n - (i + 4);
					}
					while( nText > 0 && z[i+4+nText-1] <= ' ' ){
						nText--;
					}
					SyBlobFormat(&pOut->sDesc,", comment: \"%.*s\"",(int)nText,
						(const char *)(z + i + 4));
				}
				if( (nMark >= 0xC0 && nMark <= 0xCF)
				 && nMark != 0xC4 && nMark != 0xC8 && nMark != 0xCC && i + 10 <= n ){
					switch( nMark ){
						case 0xC0: FinfoSay(pOut,", baseline"); break;
						case 0xC1: FinfoSay(pOut,", extended sequential"); break;
						case 0xC2: FinfoSay(pOut,", progressive"); break;
						default:   FinfoSay(pOut,", lossless"); break;
					}
					SyBlobFormat(&pOut->sDesc,", precision %u",(sxu32)z[i+4]);
					SyBlobFormat(&pOut->sDesc,", %ux%u",FinfoBe16(z+i+7),FinfoBe16(z+i+5));
					SyBlobFormat(&pOut->sDesc,", components %u",(sxu32)z[i+9]);
					break;
				}
				i += 2 + nLen;
			}
		}
		pOut->zMime = "image/jpeg";
		pOut->zExt = "jpeg/jpg/jpe/jfif";
		return 1;
	}
	if( FINFO_AT(z,n,0,"%PDF-") ){
		sxi32 nPages;
		if( n >= 8 ){
			SyBlobFormat(&pOut->sDesc,"PDF document, version %c.%c",z[5],z[7]);
		}else{
			FinfoSay(pOut,"PDF document");
		}
		/* The page count is the `/Count` beside the document's `/Pages`, which
		 * is what php's entry looks for and why a PDF that names neither says
		 * only its version. */
		nPages = FINFO_FIND(z,n,1024,"/Pages");
		if( nPages >= 0 ){
			sxu32 nLimit = (sxu32)nPages + 70;
			sxi32 nCount;
			if( nLimit > n ){
				nLimit = n;
			}
			nCount = FinfoFind(z + nPages,nLimit - (sxu32)nPages,nLimit - (sxu32)nPages,
				"/Count",6);
			if( nCount >= 0 ){
				sxu32 i = (sxu32)nPages + (sxu32)nCount + 6;
				sxu32 nVal = 0;
				int bAny = 0;
				while( i < n && z[i] == ' ' ){
					i++;
				}
				while( i < n && z[i] >= '0' && z[i] <= '9' ){
					nVal = nVal * 10 + (sxu32)(z[i] - '0');
					bAny = 1;
					i++;
				}
				if( bAny ){
					SyBlobFormat(&pOut->sDesc,", %u page(s)",nVal);
				}
			}
		}
		pOut->zMime = "application/pdf";
		pOut->zExt = "pdf";
		return 1;
	}
	if( FINFO_AT(z,n,0,"BM") && n >= 34 && FinfoLe32(z+14) == 40 ){
		sxu32 nXres,nYres;
		SyBlobFormat(&pOut->sDesc,"PC bitmap, Windows 3.x format, %u x %u x %u",
			FinfoLe32(z+18),FinfoLe32(z+22),FinfoLe16(z+28));
		nXres = (n >= 46) ? FinfoLe32(z+38) : 0;
		nYres = (n >= 46) ? FinfoLe32(z+42) : 0;
		if( nXres || nYres ){
			SyBlobFormat(&pOut->sDesc,", resolution %u x %u px/m",nXres,nYres);
		}
		SyBlobFormat(&pOut->sDesc,", cbSize %u, bits offset %u",FinfoLe32(z+2),FinfoLe32(z+10));
		pOut->zMime = "image/bmp";
		pOut->zExt = "bmp";
		return 1;
	}
	if( (FINFO_AT(z,n,0,"II\052\000") || FINFO_AT(z,n,0,"MM\000\052")) && n >= 8 ){
		int bLe = (z[0] == 'I');
		sxu32 nIfd = bLe ? FinfoLe32(z+4) : FinfoBe32(z+4);
		SyBlobFormat(&pOut->sDesc,"TIFF image data, %s-endian",bLe ? "little" : "big");
		if( nIfd <= n && n - nIfd >= 2 ){
			SyBlobFormat(&pOut->sDesc,", direntries=%u",
				bLe ? FinfoLe16(z+nIfd) : FinfoBe16(z+nIfd));
		}
		pOut->zMime = "image/tiff";
		pOut->zExt = "tif/tiff";
		return 1;
	}
	if( FINFO_AT(z,n,0,"RIFF") && n >= 12 ){
		FinfoSay(pOut,"RIFF (little-endian) data");
		if( FINFO_AT(z,n,8,"WEBP") ){
			FinfoSay(pOut,", Web/P image");
			pOut->zMime = "image/webp";
			pOut->zExt = "webp";
		}else if( FINFO_AT(z,n,8,"WAVE") ){
			FinfoSay(pOut,", WAVE audio");
			if( FINFO_AT(z,n,12,"fmt ") && n >= 36 ){
				sxu32 nFmt = FinfoLe16(z+20),nChan = FinfoLe16(z+22),nRate = FinfoLe32(z+24);
				if( nFmt == 1 ){
					FinfoSay(pOut,", Microsoft PCM");
					if( n >= 36 ){
						SyBlobFormat(&pOut->sDesc,", %u bit",FinfoLe16(z+34));
					}
				}
				if( nChan == 1 ){
					FinfoSay(pOut,", mono");
				}else if( nChan == 2 ){
					FinfoSay(pOut,", stereo");
				}else{
					SyBlobFormat(&pOut->sDesc,", %u channels",nChan);
				}
				SyBlobFormat(&pOut->sDesc," %u Hz",nRate);
			}
			pOut->zMime = "audio/x-wav";
			pOut->zExt = "wav/wave";
		}else if( FINFO_AT(z,n,8,"AVI ") ){
			FinfoSay(pOut,", AVI");
			pOut->zMime = "video/x-msvideo";
			pOut->zExt = "avi/divx";
		}else{
			pOut->zMime = "application/octet-stream";
		}
		return 1;
	}
	if( FINFO_AT(z,n,4,"ftyp") && n >= 12 ){
		static const struct { const char *zBrand; const char *zDesc; const char *zMime; const char *zExt; } aFtyp[] = {
			{ "isom", "ISO Media, MP4 Base Media v1 [ISO 14496-12:2003]", "video/mp4", "mp4" },
			{ "mp41", "ISO Media, MP4 v1 [ISO 14496-1:ch13]", "video/mp4", 0 },
			{ "mp42", "ISO Media, MP4 v2 [ISO 14496-14]", "video/mp4", 0 },
			{ "M4A ", "ISO Media, Apple iTunes ALAC/AAC-LC (.M4A) Audio", "audio/x-m4a", 0 },
			{ "M4V ", "ISO Media, Apple iTunes Video (.M4V) Video", "video/x-m4v", 0 },
			{ "qt  ", "ISO Media, Apple QuickTime movie", "video/quicktime", 0 },
			{ "avif", "ISO Media, AVIF Image", "image/avif", 0 },
			{ "avis", "ISO Media, AVIF Image Sequence", "image/avif", 0 },
			{ "heic", "ISO Media, HEIF Image HEVC Main or Main Still Picture Profile", "image/heic", 0 },
			{ "heix", "ISO Media, HEIF Image HEVC Main 10 Profile", "image/heic", 0 },
			{ "mif1", "ISO Media, HEIF Image", "image/heif", 0 },
			{ "3gp4", "ISO Media, MPEG v4 system, 3GPP", "video/3gpp", 0 },
		};
		sxu32 i;
		for( i = 0 ; i < SX_ARRAYSIZE(aFtyp) ; ++i ){
			if( SyMemcmp(z + 8,aFtyp[i].zBrand,4) == 0 ){
				FinfoSay(pOut,aFtyp[i].zDesc);
				pOut->zMime = aFtyp[i].zMime;
				pOut->zExt = aFtyp[i].zExt;
				return 1;
			}
		}
		FinfoSay(pOut,"ISO Media");
		pOut->zMime = "video/mp4";
		return 1;
	}
	if( FINFO_AT(z,n,0,"\032\105\337\243") ){   /* EBML */
		/* The DocType ELEMENT decides which of the two this is, and php believes
		 * nothing without it: a buffer that merely carries the word `webm`
		 * somewhere is `data` under php too. The element is 0x4282, a
		 * length byte with its size marker, then the name. */
		sxi32 nAt = FINFO_FIND(z,n,64,"\102\202");
		if( nAt >= 0 && (sxu32)nAt + 3 < n ){
			sxu32 nName = (sxu32)nAt + 3;
			if( FinfoAt(z,n,nName,"webm",4) ){
				FinfoSay(pOut,"WebM");
				pOut->zMime = "video/webm";
				return 1;
			}
			if( FinfoAt(z,n,nName,"matroska",8) ){
				FinfoSay(pOut,"Matroska data");
				pOut->zMime = "video/x-matroska";
				return 1;
			}
		}
		return 0;
	}
	if( FINFO_AT(z,n,0,"OggS") ){
		FinfoSay(pOut,"Ogg data");
		if( FINFO_FIND(z,n,64,"\001vorbis") >= 0 ){
			FinfoSay(pOut,", Vorbis audio");
			pOut->zMime = "audio/ogg";
		}else if( FINFO_FIND(z,n,64,"OpusHead") >= 0 ){
			FinfoSay(pOut,", Opus audio");
			pOut->zMime = "audio/ogg";
		}else if( FINFO_FIND(z,n,64,"\200theora") >= 0 ){
			FinfoSay(pOut,", Theora video");
			pOut->zMime = "video/ogg";
		}else{
			pOut->zMime = "application/octet-stream";
		}
		return 1;
	}
	if( FINFO_AT(z,n,0,"fLaC") ){
		FinfoSay(pOut,"FLAC audio bitstream data");
		pOut->zMime = "audio/flac";
		return 1;
	}
	if( FINFO_AT(z,n,0,"ID3") && n >= 10 ){
		/* The tag says nothing about what follows it, and php's answer says so:
		 * a bare ID3 header is `application/octet-stream` and only the MPEG
		 * frame after the tag makes it audio. The length is syncsafe -- seven
		 * bits per byte. */
		sxu32 nTag = 10 + ((((sxu32)z[6] & 0x7F) << 21) | (((sxu32)z[7] & 0x7F) << 14)
		                 | (((sxu32)z[8] & 0x7F) << 7) | ((sxu32)z[9] & 0x7F));
		SyBlobFormat(&pOut->sDesc,"Audio file with ID3 version 2.%u.%u",(sxu32)z[3],(sxu32)z[4]);
		pOut->zMime = FinfoMpegFrame(z,n,nTag,", contains: ",pOut)
			? "audio/mpeg" : "application/octet-stream";
		return 1;
	}
	/* A bare MPEG audio frame. The screen inside FinfoMpegFrame is narrow on
	 * purpose: anything looser reads a UTF-16 BOM as audio, which is exactly
	 * what it did before. */
	if( FinfoMpegFrame(z,n,0,0,pOut) ){
		pOut->zMime = "audio/mpeg";
		return 1;
	}
	if( FINFO_AT(z,n,0,"BZh") && z[3] >= '1' && z[3] <= '9' ){
		SyBlobFormat(&pOut->sDesc,"bzip2 compressed data, block size = %c00k",z[3]);
		pOut->zMime = "application/x-bzip2";
		pOut->zExt = "bz2";
		return 1;
	}
	if( FINFO_AT(z,n,0,"\375" "7zXZ\000") ){
		FinfoSay(pOut,"XZ compressed data");
		if( n >= 8 ){
			switch( z[7] & 0x0F ){
				case 0x00: FinfoSay(pOut,", checksum NONE"); break;
				case 0x01: FinfoSay(pOut,", checksum CRC32"); break;
				case 0x04: FinfoSay(pOut,", checksum CRC64"); break;
				case 0x0A: FinfoSay(pOut,", checksum SHA-256"); break;
				default: break;
			}
		}
		pOut->zMime = "application/x-xz";
		pOut->zExt = "xz";
		return 1;
	}
	if( z[0] == 0x28 && z[1] == 0xB5 && z[2] == 0x2F && z[3] == 0xFD ){
		FinfoSay(pOut,"Zstandard compressed data (v0.8+)");
		if( n >= 5 && (z[4] & 0x03) == 0 ){
			FinfoSay(pOut,", Dictionary ID: None");
		}
		pOut->zMime = "application/zstd";
		pOut->zExt = "zst";
		return 1;
	}
	if( FINFO_AT(z,n,0,"7z\274\257\047\034") && n >= 8 ){
		SyBlobFormat(&pOut->sDesc,"7-zip archive data, version %u.%u",(sxu32)z[6],(sxu32)z[7]);
		pOut->zMime = "application/x-7z-compressed";
		pOut->zExt = "7z/cb7";
		return 1;
	}
	if( FINFO_AT(z,n,0,"Rar!\032\007") ){
		FinfoSay(pOut,"RAR archive data");
		if( n >= 8 && z[6] == 0x01 && z[7] == 0x00 ){
			FinfoSay(pOut,", v5");
			pOut->zExt = "rar";
		}else{
			pOut->zExt = "rar/cbr";
		}
		pOut->zMime = "application/vnd.rar";
		return 1;
	}
	if( n >= 3 && z[0] == 0x1F && z[1] == 0x9D ){
		SyBlobFormat(&pOut->sDesc,"compress'd data %u bits",(sxu32)(z[2] & 0x1F));
		pOut->zMime = "application/x-compress";
		pOut->zExt = "Z";
		return 1;
	}
	if( FINFO_AT(z,n,0,"\004\042M\030") ){
		FinfoSay(pOut,"LZ4 compressed data (v1.4+)");
		pOut->zMime = "application/x-lz4";
		pOut->zExt = "lz4";
		return 1;
	}
	if( FINFO_AT(z,n,0,"PK") ){
		return FinfoZip(z,n,pOut);
	}
	if( FinfoElf(z,n,nFile,pOut) ){
		return 1;
	}
	if( FINFO_AT(z,n,0,"\312\376\272\276") && n >= 8 ){
		{
			sxu32 nMajor = FinfoBe16(z+6);
			SyBlobFormat(&pOut->sDesc,"compiled Java class data, version %u.%u",
				nMajor,FinfoBe16(z+4));
			/* The platform NAME the version belongs to, which php prints from 1.2
			 * on: 46..52 are the 1.x line and everything after is the plain
			 * release number. */
			if( nMajor >= 46 && nMajor <= 52 ){
				SyBlobFormat(&pOut->sDesc," (Java 1.%u)",nMajor - 44);
			}else if( nMajor > 52 ){
				SyBlobFormat(&pOut->sDesc," (Java %u)",nMajor - 44);
			}
		}
		pOut->zMime = "application/x-java-applet";
		pOut->zExt = "class";
		return 1;
	}
	if( FINFO_AT(z,n,0,"\000asm") && n >= 8 ){
		SyBlobFormat(&pOut->sDesc,"WebAssembly (wasm) binary module version 0x%u (MVP)",
			FinfoLe32(z+4));
		pOut->zMime = "application/wasm";
		pOut->zExt = "wasm";
		return 1;
	}
	if( FINFO_AT(z,n,0,"MZ") ){
		sxu32 nPe = (n >= 64) ? FinfoLe32(z+60) : 0;
		if( nPe > 0 && FINFO_AT(z,n,nPe,"PE\0\0") && nPe <= n && n - nPe >= 26 ){
			sxu32 nMachine = FinfoLe16(z + nPe + 4);
			sxu32 nSect = FinfoLe16(z + nPe + 6);
			sxu32 nChar = FinfoLe16(z + nPe + 22);
			sxu32 nMagic = FinfoLe16(z + nPe + 24);
			SyBlobFormat(&pOut->sDesc,"PE32%s executable",nMagic == 0x20B ? "+" : "");
			if( n - nPe >= 76 ){
				SyBlobFormat(&pOut->sDesc," for MS Windows %u.%s%u",
					(sxu32)FinfoLe16(z + nPe + 72),
					FinfoLe16(z + nPe + 74) < 10 ? "0" : "",
					(sxu32)FinfoLe16(z + nPe + 74));
			}
			if( nChar & 0x2000 ){
				FinfoSay(pOut," (DLL)");
			}
			if( nMachine == 0x8664 ){
				FinfoSay(pOut,", x86-64");
			}else if( nMachine == 0x14C ){
				FinfoSay(pOut,", Intel i386");
			}else if( nMachine == 0xAA64 ){
				FinfoSay(pOut,", Aarch64");
			}
			SyBlobFormat(&pOut->sDesc,", %u sections",nSect);
			pOut->zMime = "application/vnd.microsoft.portable-executable";
			pOut->zExt = (nChar & 0x2000) ? "dll/cpl/tlb/ocx/acm/ax/ime" : "exe";
		}else{
			FinfoSay(pOut,"MS-DOS executable, MZ for MS-DOS");
			pOut->zMime = "application/x-dosexec";
			pOut->zExt = "exe/com/vlm/drv";
		}
		return 1;
	}
	if( FINFO_AT(z,n,0,"!<arch>\n") ){
		if( FINFO_AT(z,n,8,"debian-binary") ){
			FinfoSay(pOut,"Debian binary package");
			pOut->zMime = "application/vnd.debian.binary-package";
			pOut->zExt = "deb/udeb";
		}else{
			FinfoSay(pOut,"current ar archive");
			pOut->zMime = "application/x-archive";
			pOut->zExt = "a/lib/ar";
		}
		return 1;
	}
	if( FINFO_AT(z,n,0,"SQLite format 3\000") ){
		FinfoSay(pOut,"SQLite 3.x database");
		if( n >= 100 ){
			static const char *azEnc[] = { "unknown 0", "UTF-8", "UTF-16le", "UTF-16be" };
			sxu32 nEnc = FinfoBe32(z+56);
			SyBlobFormat(&pOut->sDesc,
				", last written using SQLite version %u, file counter %u, database pages %u, "
				"cookie %u, schema %u, %s encoding, version-valid-for %u",
				FinfoBe32(z+96),FinfoBe32(z+24),FinfoBe32(z+28),FinfoBe32(z+40),
				FinfoBe32(z+44),azEnc[nEnc < 4 ? nEnc : 0],FinfoBe32(z+92));
		}
		pOut->zMime = "application/vnd.sqlite3";
		pOut->zExt = "/sqlite/sqlite3/db/db3/dbe/sdb/help/ide/localstorage/sqlar/xowa/mbtiles";
		return 1;
	}
	if( FINFO_AT(z,n,257,"ustar") && n >= 512 ){
		/* php validates the header CHECKSUM before it believes any of this --
		 * the field is the octal sum of the 512-byte record with the field
		 * itself read as spaces -- so a file that merely carries `ustar` at
		 * offset 257 is not an archive. */
		sxu32 i,nSum = 0,nWant = 0;
		int bDigits = 0;
		for( i = 0 ; i < 512 ; ++i ){
			nSum += (i >= 148 && i < 156) ? (sxu32)' ' : (sxu32)z[i];
		}
		for( i = 148 ; i < 156 ; ++i ){
			if( z[i] >= '0' && z[i] <= '7' ){
				nWant = nWant * 8 + (sxu32)(z[i] - '0');
				bDigits = 1;
			}else if( z[i] == ' ' || z[i] == 0 ){
				if( bDigits ){
					break;
				}
			}else{
				bDigits = 0;
				break;
			}
		}
		if( bDigits && nSum == nWant ){
			if( FINFO_AT(z,n,257,"ustar  \000") ){
				FinfoSay(pOut,"POSIX tar archive (GNU)");
			}else{
				FinfoSay(pOut,"POSIX tar archive");
			}
			pOut->zMime = "application/x-tar";
			pOut->zExt = "tar/gtar";
			return 1;
		}
	}
	if( FINFO_AT(z,n,0,"OTTO") ){
		FinfoSay(pOut,"OpenType font data");
		pOut->zMime = "application/vnd.ms-opentype";
		return 1;
	}
	if( FINFO_AT(z,n,0,"\000\001\000\000\000") ){
		FinfoSay(pOut,"TrueType Font data");
		if( n >= 16 ){
			SyBlobFormat(&pOut->sDesc,", %u tables, 1st \"%.4s\"",FinfoBe16(z+4),(const char *)(z+12));
		}
		pOut->zMime = "font/sfnt";
		pOut->zExt = "ttf/tte";
		return 1;
	}
	if( FINFO_AT(z,n,0,"wOFF") && n >= 24 ){
		FinfoSay(pOut,"Web Open Font Format");
		FinfoSay(pOut,FINFO_AT(z,n,4,"OTTO") ? ", CFF" : ", TrueType");
		SyBlobFormat(&pOut->sDesc,", length %u, version %u.%u",
			FinfoBe32(z+8),FinfoBe16(z+20),FinfoBe16(z+22));
		pOut->zMime = "font/woff";
		return 1;
	}
	if( FINFO_AT(z,n,0,"wOF2") && n >= 28 ){
		FinfoSay(pOut,"Web Open Font Format (Version 2)");
		FinfoSay(pOut,FINFO_AT(z,n,4,"OTTO") ? ", CFF" : ", TrueType");
		SyBlobFormat(&pOut->sDesc,", length %u, version %u.%u",
			FinfoBe32(z+8),FinfoBe16(z+24),FinfoBe16(z+26));
		pOut->zMime = "font/woff2";
		pOut->zExt = "woff2";
		return 1;
	}
	if( FINFO_AT(z,n,0,"8BPS") && n >= 26 && FinfoBe16(z+4) == 1
	 && FinfoBe16(z+12) >= 1 && FinfoBe16(z+12) <= 56
	 && FinfoBe32(z+14) > 0 && FinfoBe32(z+18) > 0 ){
		static const char *azMode[10] = { "Bitmap", "Grayscale", "Indexed", "RGB", "CMYK",
		                                  0, 0, "Multichannel", "Duotone", "Lab" };
		sxu32 nMode = FinfoBe16(z+24);
		SyBlobFormat(&pOut->sDesc,"Adobe Photoshop Image, %u x %u",
			FinfoBe32(z+14),FinfoBe32(z+18));
		if( nMode < SX_ARRAYSIZE(azMode) && azMode[nMode] ){
			SyBlobFormat(&pOut->sDesc,", %s",azMode[nMode]);
		}
		SyBlobFormat(&pOut->sDesc,", %ux %u-bit channels",FinfoBe16(z+12),FinfoBe16(z+22));
		pOut->zMime = "image/vnd.adobe.photoshop";
		pOut->zExt = "psd";
		return 1;
	}
	if( FINFO_AT(z,n,0,"\320\317\021\340\241\261\032\341") && n >= 512 ){
		FinfoSay(pOut,"Composite Document File V2 Document");
		pOut->zMime = "application/x-ole-storage";
		return 1;
	}
	if( FINFO_AT(z,n,0,"\000\000\001\000") && n >= 22 && FinfoLe16(z+4) > 0
	 && FinfoLe32(z+18) <= n && FinfoLe32(z+14) <= n - FinfoLe32(z+18) ){
		/* php believes an icon directory only when the first image it points at
		 * is really there, so a bare 22-byte header is `data` under both. */
		sxu32 nIcon = FinfoLe16(z+4),i,nShow;
		SyBlobFormat(&pOut->sDesc,"MS Windows icon resource - %u icon%s",nIcon,nIcon == 1 ? "" : "s");
		nShow = nIcon > 2 ? 2 : nIcon;
		for( i = 0 ; i < nShow ; ++i ){
			sxu32 nAt = 6 + i*16;
			if( nAt + 16 > n ){
				break;
			}
			SyBlobFormat(&pOut->sDesc,", %ux%u, %u bits/pixel",
				z[nAt] ? (sxu32)z[nAt] : 256u,z[nAt+1] ? (sxu32)z[nAt+1] : 256u,
				FinfoLe16(z+nAt+6));
		}
		pOut->zMime = "image/vnd.microsoft.icon";
		pOut->zExt = "ico";
		return 1;
	}
	return 0;
}
/*
 * ---------------------------------------------------------------------------
 * The TEXT formats
 * ---------------------------------------------------------------------------
 * These are the rows libmagic keeps in the text half of its database: they
 * match on characters rather than bytes, and their description is a PREFIX
 * that the text analysis finishes -- which is why a php file reads
 * `PHP script, ASCII text` and the same file saved as UTF-8 with a BOM reads
 * `PHP script, Unicode text, UTF-8 (with BOM) text`.
 */
/* A strict JSON reader: libmagic accepts an OBJECT or an ARRAY and nothing
 * else, so a bare `123` or `"x"` stays plain text. */
static int FinfoJsonValue(const unsigned char *z,sxu32 n,sxu32 *pi,int nDepth);
static void FinfoJsonSpace(const unsigned char *z,sxu32 n,sxu32 *pi)
{
	while( *pi < n && (z[*pi] == ' ' || z[*pi] == '\t' || z[*pi] == '\r' || z[*pi] == '\n') ){
		(*pi)++;
	}
}
static int FinfoJsonString(const unsigned char *z,sxu32 n,sxu32 *pi)
{
	sxu32 i = *pi;
	if( i >= n || z[i] != '"' ){
		return 0;
	}
	for( i++ ; i < n ; ++i ){
		if( z[i] == '\\' ){
			i++;
			continue;
		}
		if( z[i] == '"' ){
			*pi = i + 1;
			return 1;
		}
		if( z[i] < 0x20 ){
			return 0;
		}
	}
	return 0;
}
static int FinfoJsonNumber(const unsigned char *z,sxu32 n,sxu32 *pi)
{
	sxu32 i = *pi,nStart;
	if( i < n && z[i] == '-' ){
		i++;
	}
	nStart = i;
	while( i < n && z[i] >= '0' && z[i] <= '9' ){
		i++;
	}
	if( i == nStart ){
		return 0;
	}
	if( i < n && z[i] == '.' ){
		i++;
		nStart = i;
		while( i < n && z[i] >= '0' && z[i] <= '9' ){
			i++;
		}
		if( i == nStart ){
			return 0;
		}
	}
	if( i < n && (z[i] == 'e' || z[i] == 'E') ){
		i++;
		if( i < n && (z[i] == '+' || z[i] == '-') ){
			i++;
		}
		nStart = i;
		while( i < n && z[i] >= '0' && z[i] <= '9' ){
			i++;
		}
		if( i == nStart ){
			return 0;
		}
	}
	*pi = i;
	return 1;
}
static int FinfoJsonValue(const unsigned char *z,sxu32 n,sxu32 *pi,int nDepth)
{
	if( nDepth > 128 ){
		return 0;
	}
	FinfoJsonSpace(z,n,pi);
	if( *pi >= n ){
		return 0;
	}
	switch( z[*pi] ){
		case '{': {
			(*pi)++;
			FinfoJsonSpace(z,n,pi);
			if( *pi < n && z[*pi] == '}' ){
				(*pi)++;
				return 1;
			}
			for(;;){
				FinfoJsonSpace(z,n,pi);
				if( !FinfoJsonString(z,n,pi) ){
					return 0;
				}
				FinfoJsonSpace(z,n,pi);
				if( *pi >= n || z[*pi] != ':' ){
					return 0;
				}
				(*pi)++;
				if( !FinfoJsonValue(z,n,pi,nDepth+1) ){
					return 0;
				}
				FinfoJsonSpace(z,n,pi);
				if( *pi < n && z[*pi] == ',' ){
					(*pi)++;
					continue;
				}
				if( *pi < n && z[*pi] == '}' ){
					(*pi)++;
					return 1;
				}
				return 0;
			}
		}
		case '[': {
			(*pi)++;
			FinfoJsonSpace(z,n,pi);
			if( *pi < n && z[*pi] == ']' ){
				(*pi)++;
				return 1;
			}
			for(;;){
				if( !FinfoJsonValue(z,n,pi,nDepth+1) ){
					return 0;
				}
				FinfoJsonSpace(z,n,pi);
				if( *pi < n && z[*pi] == ',' ){
					(*pi)++;
					continue;
				}
				if( *pi < n && z[*pi] == ']' ){
					(*pi)++;
					return 1;
				}
				return 0;
			}
		}
		case '"': return FinfoJsonString(z,n,pi);
		case 't': if( FinfoAt(z,n,*pi,"true",4) ){ *pi += 4; return 1; } return 0;
		case 'f': if( FinfoAt(z,n,*pi,"false",5) ){ *pi += 5; return 1; } return 0;
		case 'n': if( FinfoAt(z,n,*pi,"null",4) ){ *pi += 4; return 1; } return 0;
		default:  return FinfoJsonNumber(z,n,pi);
	}
}
static int FinfoIsJson(const unsigned char *z,sxu32 n)
{
	sxu32 i = 0;
	FinfoJsonSpace(z,n,&i);
	if( i >= n || (z[i] != '{' && z[i] != '[') ){
		return 0;
	}
	if( !FinfoJsonValue(z,n,&i,0) ){
		return 0;
	}
	FinfoJsonSpace(z,n,&i);
	return i >= n;
}
/*
 * libmagic's CSV reader, as its answers describe it: at least TWO complete
 * records, every one of them the same number of fields, at least three of
 * them, comma-separated, and the last record terminated -- a file whose final
 * line has no newline is not a CSV, which is measurable and reproduced.
 */
static int FinfoIsCsv(const unsigned char *z,sxu32 n)
{
	sxu32 i = 0,nRec = 0,nField = 0,nWant = 0;
	int bQuote = 0,bAny = 0;
	if( n < 4 ){
		return 0;
	}
	nField = 1;
	for( i = 0 ; i < n ; ++i ){
		if( bQuote ){
			if( z[i] == '"' ){
				if( i + 1 < n && z[i+1] == '"' ){
					i++;
					continue;
				}
				bQuote = 0;
			}
			continue;
		}
		if( z[i] == '"' ){
			bQuote = 1;
			bAny = 1;
			continue;
		}
		if( z[i] == ',' ){
			nField++;
			continue;
		}
		if( z[i] == '\n' ){
			if( nField < 3 ){
				return 0;
			}
			if( nRec == 0 ){
				nWant = nField;
			}else if( nField != nWant ){
				return 0;
			}
			nRec++;
			nField = 1;
			bAny = 0;
			continue;
		}
		if( z[i] == '\r' ){
			continue;
		}
		bAny = 1;
	}
	if( bQuote || bAny || nField != 1 ){
		return 0;   /* an unterminated final record */
	}
	return nRec >= 2;
}
/*
 * The `#!` line. Which interpreters get a name of their own is the database's
 * business rather than a rule, and the two lists are not the same: a direct
 * path knows sh/ksh/csh/awk/php and an `env` line does not, while both know
 * bash, zsh, python, perl, ruby, node, lua and tclsh. Anything else is
 * libmagic's generic `a %s script`, whose %s is the whole first word for a
 * direct path and just the interpreter word after an `env`.
 */
static int FinfoShebang(const unsigned char *z,sxu32 n,FinfoAnswer *pOut)
{
	static const struct {
		const char *zName; const char *zDesc; const char *zEnvDesc; const char *zMime; int bEnv;
	} aInterp[] = {
		{ "sh",      "POSIX shell script",         0,             "text/x-shellscript",   0 },
		{ "bash",    "Bourne-Again shell script",  0,             "text/x-shellscript",   1 },
		{ "zsh",     "Paul Falstad's zsh script",  0,             "text/x-shellscript",   1 },
		{ "ksh",     "Korn shell script",          0,             "text/x-shellscript",   0 },
		{ "csh",     "C shell script",             0,             "text/x-shellscript",   0 },
		{ "tcsh",    "Tenex C shell script",       0,             "text/x-shellscript",   0 },
		{ "php",     "PHP script",                 0,             "text/x-php",           0 },
		{ "python",  "Python script",              0,             "text/x-script.python", 1 },
		{ "ruby",    "Ruby script",                0,             "text/x-ruby",          1 },
		{ "node",    "Node.js script executable, ",0,             "application/javascript", 1 },
		{ "awk",     "awk script",                 0,             "text/x-awk",           0 },
		{ "gawk",    "awk script",                 0,             "text/x-awk",           0 },
		{ "lua",     "Lua script",                 0,             "text/x-lua",           1 },
		/* tclsh is the one name the two doors describe differently, which is a
		 * fact about the database rather than a rule: a direct path reads
		 * `Tcl/Tk script` and the env form `Tcl script`. */
		{ "tclsh",   "Tcl/Tk script",              "Tcl script",  "text/x-tcl",           1 },
		{ "wish",    "Tcl/Tk script",              "Tcl script",  "text/x-tcl",           1 },
	};
	sxu32 i,nStart,nEnd,nWordStart,nWordEnd;
	int bEnv = 0;
	const char *zWord;
	sxu32 nWord;
	if( n < 3 || z[0] != '#' || z[1] != '!' ){
		return 0;
	}
	i = 2;
	while( i < n && (z[i] == ' ' || z[i] == '\t') ){
		i++;
	}
	nStart = i;
	while( i < n && z[i] != ' ' && z[i] != '\t' && z[i] != '\n' && z[i] != '\r' ){
		i++;
	}
	nEnd = i;
	if( nEnd == nStart ){
		return 0;
	}
	/* `#!/usr/bin/env foo` -- and libmagic takes the NEXT word whatever lies
	 * between, newline included, which is why `#!/usr/bin/env` alone answers
	 * with the first word of the line under it. */
	if( (nEnd - nStart >= 4 && SyMemcmp(z + nEnd - 4,"/env",4) == 0)
	 || (nEnd - nStart == 3 && SyMemcmp(z + nStart,"env",3) == 0) ){
		while( i < n && (z[i] == ' ' || z[i] == '\t' || z[i] == '\n' || z[i] == '\r') ){
			i++;
		}
		nStart = i;
		while( i < n && z[i] != ' ' && z[i] != '\t' && z[i] != '\n' && z[i] != '\r' ){
			i++;
		}
		nEnd = i;
		bEnv = 1;
		if( nEnd == nStart ){
			return 0;
		}
	}
	/* The BASENAME is what the table is keyed on, and a trailing version digit
	 * is part of the name php recognises (`python3`, `php8`). */
	nWordStart = nStart;
	for( i = nStart ; i < nEnd ; ++i ){
		if( z[i] == '/' ){
			nWordStart = i + 1;
		}
	}
	nWordEnd = nEnd;
	zWord = (const char *)(z + nWordStart);
	nWord = nWordEnd - nWordStart;
	for( i = 0 ; i < SX_ARRAYSIZE(aInterp) ; ++i ){
		sxu32 nLen = (sxu32)SyStrlen(aInterp[i].zName);
		if( nWord < nLen || SyMemcmp(zWord,aInterp[i].zName,nLen) != 0 ){
			continue;
		}
		/* Only a VERSION may follow the name: `python3` is python, `phpunit`
		 * is not php. */
		{
			sxu32 k;
			int bOk = 1;
			for( k = nLen ; k < nWord ; ++k ){
				if( (zWord[k] < '0' || zWord[k] > '9') && zWord[k] != '.' ){
					bOk = 0;
					break;
				}
			}
			if( !bOk ){
				continue;
			}
		}
		if( bEnv && !aInterp[i].bEnv ){
			break;   /* the env form does not know this one: fall to the generic row */
		}
		FinfoSay(pOut,(bEnv && aInterp[i].zEnvDesc) ? aInterp[i].zEnvDesc : aInterp[i].zDesc);
		pOut->zMime = aInterp[i].zMime;
		/* Node spells the whole thing itself: its row ends in the separator and
		 * puts `executable` BEFORE the text rather than after it. */
		if( SyMemcmp(aInterp[i].zName,"node",4) == 0 ){
			pOut->bText = 2;
		}else{
			pOut->bText = 1;
			pOut->bExec = 1;
		}
		return 1;
	}
	/* perl carries its whole sentence in the row -- no text analysis is
	 * appended to it at all, which no other interpreter does. */
	if( nWord >= 4 && SyMemcmp(zWord,"perl",4) == 0 ){
		FinfoSay(pOut,"Perl script text executable");
		pOut->zMime = "text/x-perl";
		return 1;
	}
	/* The generic row: the whole first word for a direct path, the interpreter
	 * word alone after an `env`. */
	SyBlobFormat(&pOut->sDesc,"a %.*s script",(int)(nEnd - nStart),(const char *)(z + nStart));
	pOut->zMime = "text/plain";
	pOut->bText = 1;
	pOut->bExec = 1;
	return 1;
}
static int FinfoTextFormat(const unsigned char *z,sxu32 n,FinfoAnswer *pOut)
{
	sxu32 nWs = 0;
	if( FinfoShebang(z,n,pOut) ){
		return 1;
	}
	/* A UTF-8 BOM is not part of the text as far as these markers go: a php
	 * file saved with one is still `PHP script`. */
	if( n > 3 && z[0] == 0xEF && z[1] == 0xBB && z[2] == 0xBF ){
		z += 3;
		n -= 3;
	}
	if( FINFO_FIND(z,n,4096,"<svg") >= 0 ){
		FinfoSay(pOut,"SVG Scalable Vector Graphics image, ");
		pOut->zMime = "image/svg+xml";
		pOut->zExt = "svg";
		pOut->bText = 2;
		return 1;
	}
	if( FINFO_AT(z,n,0,"<?xml") ){
		sxi32 nVer = FINFO_FIND(z,n,64,"version=\"");
		if( nVer >= 0 && (sxu32)nVer + 12 <= n ){
			SyBlobFormat(&pOut->sDesc,"XML %.3s document, ",(const char *)(z + nVer + 9));
		}else{
			FinfoSay(pOut,"XML document, ");
		}
		pOut->zMime = "text/xml";
		pOut->bText = 2;
		return 1;
	}
	if( FINFO_ATI(z,n,0,"<?php") || FINFO_AT(z,n,0,"<?\n") || FINFO_AT(z,n,0,"<?\r") ){
		FinfoSay(pOut,"PHP script, ");
		pOut->zMime = "text/x-php";
		pOut->bText = 2;
		return 1;
	}
	while( nWs < n && (z[nWs] == ' ' || z[nWs] == '\t' || z[nWs] == '\n' || z[nWs] == '\r') ){
		nWs++;
	}
	if( FINFO_ATI(z,n,nWs,"<!doctype") || FINFO_ATI(z,n,nWs,"<html")
	 || FINFO_ATI(z,n,nWs,"<head") || FINFO_ATI(z,n,nWs,"<title")
	 || FINFO_ATI(z,n,nWs,"<script") || FINFO_ATI(z,n,nWs,"<style")
	 || FINFO_ATI(z,n,nWs,"<table") || FINFO_ATI(z,n,nWs,"<a href=") ){
		FinfoSay(pOut,"HTML document, ");
		pOut->zMime = "text/html";
		pOut->bText = 2;
		return 1;
	}
	if( FINFO_AT(z,n,0,"{\\rtf") && n >= 6 ){
		SyBlobFormat(&pOut->sDesc,"Rich Text Format data, version %c",z[5]);
		if( FINFO_FIND(z,n,64,"\\ansi") >= 0 ){
			FinfoSay(pOut,", ANSI");
		}else if( FINFO_FIND(z,n,64,"\\mac") >= 0 ){
			FinfoSay(pOut,", Apple Macintosh");
		}else if( FINFO_FIND(z,n,64,"\\pca") >= 0 ){
			FinfoSay(pOut,", IBM PS/2 codepage 850");
		}else if( FINFO_FIND(z,n,64,"\\pc") >= 0 ){
			FinfoSay(pOut,", IBM PC, code page 437");
		}
		pOut->zMime = "text/rtf";
		pOut->zExt = "rtf";
		return 1;
	}
	if( FINFO_AT(z,n,0,"%!") || FINFO_AT(z,n,0,"\004%!") ){
		FinfoSay(pOut,"PostScript document text");
		if( FINFO_AT(z,n,0,"%!PS-Adobe-") && n >= 14 ){
			SyBlobFormat(&pOut->sDesc," conforming DSC level %.3s",(const char *)(z + 11));
		}
		pOut->zMime = "application/postscript";
		return 1;
	}
	if( FINFO_AT(z,n,0,"--- ") && FINFO_FIND(z,n,4096,"\n+++ ") >= 0 ){
		FinfoSay(pOut,"unified diff output, ");
		pOut->zMime = "text/x-diff";
		pOut->zExt = "diff/patch/dif/pch/rej";
		pOut->bText = 2;
		return 1;
	}
	if( FINFO_AT(z,n,0,"*** ") && FINFO_FIND(z,n,4096,"\n--- ") >= 0 ){
		FinfoSay(pOut,"context diff output, ");
		pOut->zMime = "text/x-diff";
		pOut->zExt = "diff/patch";
		pOut->bText = 2;
		return 1;
	}
	if( FINFO_AT(z,n,0,"#EXTM3U") ){
		FinfoSay(pOut,"M3U playlist, ");
		pOut->zMime = "audio/x-mpegurl";
		pOut->bText = 2;
		return 1;
	}
	if( FINFO_AT(z,n,0,"From:") || FINFO_AT(z,n,0,"Return-Path:")
	 || FINFO_AT(z,n,0,"Received:") || FINFO_AT(z,n,0,"Message-ID:")
	 || FINFO_AT(z,n,0,"Path:") ){
		FinfoSay(pOut,"news or mail, ");
		pOut->zMime = "message/rfc822";
		pOut->bText = 2;
		return 1;
	}
	if( FinfoIsJson(z,n) ){
		FinfoSay(pOut,"JSON text data");
		pOut->zMime = "application/json";
		return 1;
	}
	if( FinfoIsCsv(z,n) ){
		FinfoSay(pOut,"CSV ");
		pOut->zMime = "text/csv";
		pOut->bText = 3;
		return 1;
	}
	/* The three JavaScript hints php's database actually fires on. Its
	 * detection is a keyword lottery -- `function x(){}` is plain text under
	 * php too -- so only the measured ones are here. */
	if( FINFO_AT(z,n,nWs,"'use strict'") || FINFO_AT(z,n,nWs,"\"use strict\"")
	 || FINFO_FIND(z,n,8192,"export default") >= 0
	 || FINFO_FIND(z,n,8192,"(typeof ") >= 0
	 || FINFO_FIND(z,n,8192,"jQuery(") >= 0 ){
		FinfoSay(pOut,"JavaScript source, ");
		pOut->zMime = "application/javascript";
		pOut->zExt = "js";
		pOut->bText = 2;
		return 1;
	}
	return 0;
}
/*
 * The whole reading of a buffer, in libmagic's own order: the two degenerate
 * sizes first, then the binary signatures, then the text ones, then the plain
 * text analysis -- and `data` for everything that is none of those.
 */
static void FinfoAnalyze(const unsigned char *z,sxu32 n,ph7_int64 nFile,
	FinfoAnswer *pOut,FinfoText *pTx)
{
	FinfoTextScan(z,n,pTx);
	pOut->zMime = 0;
	pOut->zExt = 0;
	pOut->bText = 0;
	pOut->bExec = 0;
	if( n < 1 ){
		pTx->iEnc = FINFO_ENC_BINARY;
		FinfoSay(pOut,"empty");
		pOut->zMime = "application/x-empty";
		return;
	}
	if( n < 2 ){
		pTx->iEnc = FINFO_ENC_BINARY;
		FinfoSay(pOut,"very short file (no magic)");
		pOut->zMime = "application/octet-stream";
		return;
	}
	if( !FinfoGzip(z,n,nFile,pOut) && !FinfoBinary(z,n,nFile,pOut)
	 && !(pTx->iEnc != FINFO_ENC_BINARY && FinfoTextFormat(z,n,pOut)) ){
		if( pTx->iEnc == FINFO_ENC_BINARY ){
			FinfoSay(pOut,"data");
			pOut->zMime = "application/octet-stream";
			return;
		}
		pOut->bText = 2;
		pOut->zMime = "text/plain";
	}
	if( pOut->bText ){
		if( pOut->bText == 1 ){
			SyBlobAppend(&pOut->sDesc,", ",2);
		}
		if( pOut->bText == 3 ){
			FinfoSay(pOut,FinfoEncDesc(pTx->iEnc));
		}else{
			FinfoTextDesc(pTx,&pOut->sDesc);
		}
		if( pOut->bExec ){
			FinfoSay(pOut," executable");
		}
	}
	if( pOut->zMime == 0 ){
		pOut->zMime = "application/octet-stream";
	}
}
/*
 * Which of the three faces the flags ask for. libmagic checks them in this
 * order, and only one ever answers: EXTENSION, then the two MIME halves
 * (together they are `type; charset=encoding`), then the description.
 */
static void FinfoRender(const FinfoAnswer *pAns,const FinfoText *pTx,int iFlags,SyBlob *pOut)
{
	if( iFlags & FINFO_EXTENSION ){
		SyBlobAppend(pOut,pAns->zExt ? pAns->zExt : "???",
			(sxu32)SyStrlen(pAns->zExt ? pAns->zExt : "???"));
		return;
	}
	if( (iFlags & FINFO_MIME_TYPE) && (iFlags & FINFO_MIME_ENCODING) ){
		SyBlobFormat(pOut,"%s; charset=%s",pAns->zMime,FinfoCharset(pTx->iEnc));
		return;
	}
	if( iFlags & FINFO_MIME_TYPE ){
		SyBlobAppend(pOut,pAns->zMime,(sxu32)SyStrlen(pAns->zMime));
		return;
	}
	if( iFlags & FINFO_MIME_ENCODING ){
		SyBlobAppend(pOut,FinfoCharset(pTx->iEnc),(sxu32)SyStrlen(FinfoCharset(pTx->iEnc)));
		return;
	}
	SyBlobAppend(pOut,SyBlobData(&pAns->sDesc),SyBlobLength(&pAns->sDesc));
}
/*
 * ---------------------------------------------------------------------------
 * The php layer
 * ---------------------------------------------------------------------------
 */
/* The flags an instance carries, or php's `Invalid finfo object` for one that
 * was never constructed (ReflectionClass::newInstanceWithoutConstructor). */
static int FinfoFlagsOf(ph7_context *pCtx,ph7_class_instance *pThis,int *piFlags)
{
	SyString sAttr;
	ph7_value *pSlot;
	if( pThis ){
		SyStringInitFromBuf(&sAttr,FINFO_FLAGS_SLOT,sizeof(FINFO_FLAGS_SLOT)-1);
		pSlot = PH7_ClassInstanceFetchAttr(pThis,&sAttr);
		if( pSlot && (pSlot->iFlags & MEMOBJ_INT) ){
			*piFlags = (int)pSlot->x.iVal;
			return 1;
		}
	}
	PH7_VmThrowException(pCtx,"Error","Invalid finfo object");
	return 0;
}
static int FinfoSetFlagsOf(ph7_class_instance *pThis,int iFlags)
{
	SyString sAttr;
	ph7_value *pSlot;
	if( pThis == 0 ){
		return -1;
	}
	SyStringInitFromBuf(&sAttr,FINFO_FLAGS_SLOT,sizeof(FINFO_FLAGS_SLOT)-1);
	pSlot = PH7_ClassInstanceFetchAttr(pThis,&sAttr);
	if( pSlot == 0 ){
		return -1;
	}
	PH7_MemObjRelease(pSlot);
	pSlot->x.iVal = (ph7_int64)iFlags;
	MemObjSetType(pSlot,MEMOBJ_INT);
	return 0;
}
/* The finfo an argument names, with php's TypeError for anything else. The
 * signature table has already screened the type, so a miss here can only be a
 * SUBCLASS instance with nothing in its slot. */
static ph7_class_instance * FinfoArgObject(ph7_value *pArg)
{
	return (pArg->iFlags & MEMOBJ_OBJ) ? (ph7_class_instance *)pArg->x.pOther : 0;
}
/* Analyse a buffer and answer the face the flags ask for. nFile is the size of
 * the whole object when the caller knows it (only a FILE does). */
static void FinfoAnswerBuffer(ph7_context *pCtx,const unsigned char *z,sxu32 n,
	ph7_int64 nFile,int iFlags)
{
	FinfoAnswer sAns;
	FinfoText sTx;
	SyBlob sOut;
	SyBlobInit(&sAns.sDesc,&pCtx->pVm->sAllocator);
	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);
	FinfoAnalyze(z,n,nFile,&sAns,&sTx);
	FinfoRender(&sAns,&sTx,iFlags,&sOut);
	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));
	SyBlobRelease(&sAns.sDesc);
	SyBlobRelease(&sOut);
}
/*
 * Is this path a directory? php asks before it opens anything, and answers the
 * bare string `directory` when the answer is yes -- through every face, mime
 * included. A userland wrapper's url_stat() answers for the paths it owns.
 */
static int FinfoPathIsDir(ph7_context *pCtx,const char *zPath)
{
	const ph7_vfs *pVfs = pCtx->pVm->pEngine->pVfs;
	/* The OS's own stat and nothing else: a path a userland wrapper owns is not
	 * asked, which is measurable -- a wrapper whose url_stat() reports a
	 * DIRECTORY still has its bytes read and named under php. */
	return (pVfs && pVfs->xIsdir) ? (pVfs->xIsdir(zPath) == PH7_OK) : 0;
}
#ifndef PH7_DISABLE_DISK_IO
/*
 * php reads the file through a php_stream, so every wrapper this engine
 * carries answers -- and so does the one a script registered itself. At most
 * FINFO_READ_MAX bytes, which is libmagic's own parameter.
 */
static int FinfoReadPath(ph7_context *pCtx,const char *zPath,int nPath,SyBlob *pOut,
	ph7_int64 *pnFile)
{
	const ph7_io_stream *pStream;
	void *pHandle;
	char zBuf[8192];
	const char *zName = zPath;
	ph7_int64 nTotal = 0;
	*pnFile = -1;
	/* php resolves the wrapper once for its open_basedir check and once to open,
	 * so a scheme nothing implements is named TWICE before the open failure --
	 * which is this door's own diagnostic shape and nothing else's. */
	{
		const char *zProbe = zPath;
		if( PH7_VmGetStreamDevice(pCtx->pVm,&zProbe,nPath) == 0 ){
			VfsThrowUnknownWrapperWarning(pCtx,zPath);
		}
	}
	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zName,nPath);
	if( pStream == 0 ){
		VfsThrowNoDeviceWarning(pCtx,zName,FALSE);
		return -1;
	}
	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zName,PH7_IO_OPEN_RDONLY,
		FALSE,0,FALSE,0,ph7_function_name(pCtx));
	if( pHandle == 0 ){
		VfsThrowOpenWarning(pCtx,zName);
		return -1;
	}
	for(;;){
		ph7_int64 n;
		ph7_int64 nChunk = (ph7_int64)sizeof(zBuf);
		if( nTotal + nChunk > (ph7_int64)FINFO_READ_MAX ){
			nChunk = (ph7_int64)FINFO_READ_MAX - nTotal;
		}
		if( nChunk < 1 ){
			break;
		}
		n = pStream->xRead ? pStream->xRead(pHandle,zBuf,nChunk) : -1;
		if( n < 1 ){
			if( n == 0 || nTotal > 0 ){
				/* A clean end, or a device that stopped answering after giving
				 * something: what was read IS the file as far as this goes. */
			}
			break;
		}
		SyBlobAppend(pOut,zBuf,(sxu32)n);
		nTotal += n;
	}
	/* The whole size, when the stream can say -- gzip's trailer is the one
	 * answer that needs it, and a stream that cannot seek simply has none. */
	if( pStream->xSeek && pStream->xTell && nTotal < (ph7_int64)FINFO_READ_MAX ){
		*pnFile = nTotal;
	}
	/* php asks the stream for a descriptor once it has the bytes, and a
	 * userland wrapper hears about it. */
	PH7_StreamUserCast(pCtx,pStream,pHandle);
	PH7_StreamCloseHandle(pStream,pHandle);
	return 0;
}
#endif /* PH7_DISABLE_DISK_IO */
/*
 * The body of finfo::file() and finfo_file(): the directory shortcut, then the
 * stream, then the analysis. iArgPos is which argument number the empty-name
 * ValueError names -- php's check is written for the procedural signature, so
 * it says #2 in both doors and picks up the METHOD's name for #2 in the method
 * one.
 */
static int FinfoFileCommon(ph7_context *pCtx,int nArg,ph7_value **apArg,int iFlags,int nSkip,
	int iErrPos,const char *zErrName)
{
	const char *zPath;
	int nPath;
	SyBlob sData;
	ph7_int64 nFile = -1;
	if( nArg > nSkip + 1 && ph7_value_is_int(apArg[nSkip+1]) ){
		int iCall = (int)ph7_value_to_int(apArg[nSkip+1]);
		if( iCall != 0 ){
			iFlags = iCall;   /* this call only; the object keeps its own */
		}
	}
	zPath = ph7_value_to_string(apArg[nSkip],&nPath);
	if( nPath < 1 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"%s(): Argument #%d ($%s) must not be empty",ph7_function_name(pCtx),
			iErrPos,zErrName);
	}
	if( FinfoPathIsDir(pCtx,zPath) ){
		ph7_result_string(pCtx,"directory",sizeof("directory")-1);
		return PH7_OK;
	}
#ifndef PH7_DISABLE_DISK_IO
	SyBlobInit(&sData,&pCtx->pVm->sAllocator);
	if( FinfoReadPath(pCtx,zPath,nPath,&sData,&nFile) != 0 ){
		SyBlobRelease(&sData);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	FinfoAnswerBuffer(pCtx,(const unsigned char *)SyBlobData(&sData),
		SyBlobLength(&sData),nFile,iFlags);
	SyBlobRelease(&sData);
#else
	SXUNUSED(sData);
	SXUNUSED(nFile);
	ph7_result_bool(pCtx,0);
#endif
	return PH7_OK;
}
/*
 * php refuses a $magic_database it cannot load with a warning naming the door
 * and the path, and false -- or, from the constructor, an Exception carrying
 * that same sentence. PHL takes that path for EVERY non-empty database
 * argument: the file is in libmagic's compiled or source format and this
 * engine carries its own table instead of a reader for one.
 */
static int FinfoRefuseDatabase(ph7_context *pCtx,const char *zDb,int bCtor)
{
	if( bCtor ){
		return PH7_VmThrowException(pCtx,"Exception",
			"%s(): Failed to load magic database at \"%s\"",ph7_function_name(pCtx),zDb);
	}
	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
		"Failed to load magic database at \"%s\"",zDb);
	return PH7_OK;
}
/* finfo::__construct(int $flags = FILEINFO_NONE, ?string $magic_database = null) */
static int vm_builtin_finfo_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	int iFlags = FINFO_NONE;
	if( nArg > 0 ){
		iFlags = (int)ph7_value_to_int(apArg[0]);
	}
	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){
		int nDb;
		const char *zDb = ph7_value_to_string(apArg[1],&nDb);
		if( nDb > 0 ){
			return FinfoRefuseDatabase(pCtx,zDb,TRUE);
		}
	}
	FinfoSetFlagsOf(pThis,iFlags);
	return PH7_OK;
}
/* finfo::file(string $filename, int $flags = FILEINFO_NONE, $context = null) */
static int vm_builtin_finfo_file(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int iFlags;
	if( !FinfoFlagsOf(pCtx,PH7_ContextThis(pCtx),&iFlags) ){
		return PH7_OK;
	}
	/* php's check is written for the PROCEDURAL signature, so it names argument
	 * #2 -- and picks up whatever argument #2 is called in the door it was
	 * raised through, which is `$flags` here and `$filename` there. */
	return FinfoFileCommon(pCtx,nArg,apArg,iFlags,0,2,"flags");
}
/* finfo::buffer(string $string, int $flags = FILEINFO_NONE, $context = null) */
static int vm_builtin_finfo_buffer(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zData;
	int nData,iFlags;
	if( !FinfoFlagsOf(pCtx,PH7_ContextThis(pCtx),&iFlags) ){
		return PH7_OK;
	}
	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){
		int iCall = (int)ph7_value_to_int(apArg[1]);
		if( iCall != 0 ){
			iFlags = iCall;
		}
	}
	zData = ph7_value_to_string(apArg[0],&nData);
	FinfoAnswerBuffer(pCtx,(const unsigned char *)zData,(sxu32)(nData < 0 ? 0 : nData),-1,iFlags);
	return PH7_OK;
}
/* finfo::set_flags(int $flags): true */
static int vm_builtin_finfo_set_flags(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int iFlags;
	if( !FinfoFlagsOf(pCtx,PH7_ContextThis(pCtx),&iFlags) ){
		return PH7_OK;
	}
	SXUNUSED(nArg);
	FinfoSetFlagsOf(PH7_ContextThis(pCtx),(int)ph7_value_to_int(apArg[0]));
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/* finfo|false finfo_open(int $flags = FILEINFO_NONE, ?string $magic_database = null) */
PH7_PRIVATE int PH7_builtin_finfo_open(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pObj;
	ph7_class *pClass;
	int iFlags = FINFO_NONE;
	if( nArg > 0 ){
		iFlags = (int)ph7_value_to_int(apArg[0]);
	}
	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){
		int nDb;
		const char *zDb = ph7_value_to_string(apArg[1],&nDb);
		if( nDb > 0 ){
			FinfoRefuseDatabase(pCtx,zDb,FALSE);
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
	}
	pClass = PH7_VmExtractClass(pCtx->pVm,"finfo",sizeof("finfo")-1,FALSE,0);
	pObj = pClass ? PH7_NewClassInstance(pCtx->pVm,pClass) : 0;
	if( pObj == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	FinfoSetFlagsOf(pObj,iFlags);
	PH7_NativeResultObject(pCtx,pObj);
	return PH7_OK;
}
/* true finfo_close(finfo $finfo) -- deprecated in 8.5 and does nothing. */
PH7_PRIVATE int PH7_builtin_finfo_close(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg); SXUNUSED(apArg);
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/* true finfo_set_flags(finfo $finfo,int $flags) */
PH7_PRIVATE int PH7_builtin_finfo_set_flags(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = FinfoArgObject(apArg[0]);
	int iFlags;
	SXUNUSED(nArg);
	if( !FinfoFlagsOf(pCtx,pThis,&iFlags) ){
		return PH7_OK;
	}
	FinfoSetFlagsOf(pThis,(int)ph7_value_to_int(apArg[1]));
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/* string|false finfo_file(finfo $finfo,string $filename,int $flags = 0,$context = null) */
PH7_PRIVATE int PH7_builtin_finfo_file(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int iFlags;
	if( !FinfoFlagsOf(pCtx,FinfoArgObject(apArg[0]),&iFlags) ){
		return PH7_OK;
	}
	return FinfoFileCommon(pCtx,nArg,apArg,iFlags,1,2,"filename");
}
/* string|false finfo_buffer(finfo $finfo,string $string,int $flags = 0,$context = null) */
PH7_PRIVATE int PH7_builtin_finfo_buffer(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zData;
	int nData,iFlags;
	if( !FinfoFlagsOf(pCtx,FinfoArgObject(apArg[0]),&iFlags) ){
		return PH7_OK;
	}
	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){
		int iCall = (int)ph7_value_to_int(apArg[2]);
		if( iCall != 0 ){
			iFlags = iCall;
		}
	}
	zData = ph7_value_to_string(apArg[1],&nData);
	FinfoAnswerBuffer(pCtx,(const unsigned char *)zData,(sxu32)(nData < 0 ? 0 : nData),-1,iFlags);
	return PH7_OK;
}
/*
 * string|false mime_content_type(resource|string $filename)
 *
 * The one door that takes an OPEN stream as well as a path, and it reads such a
 * stream from the BEGINNING and puts it back exactly where it found it -- so a
 * script that has already read a header can still ask.
 */
PH7_PRIVATE int PH7_builtin_mime_content_type(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	/* The parameter is declared untyped and screened by hand -- php's own
	 * arrangement, and the message says the union its body accepts. */
	if( !ph7_value_is_resource(apArg[0]) && !ph7_value_is_string(apArg[0])
	 && !PH7_ArgSatisfiesString(apArg[0]) ){
		char zGiven[64];
		return PH7_VmThrowException(pCtx,"TypeError",
			"mime_content_type(): Argument #1 ($filename) must be of type resource|string, %s given",
			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));
	}
	if( ph7_value_is_resource(apArg[0]) ){
#ifndef PH7_DISABLE_DISK_IO
		io_private *pDev = (io_private *)ph7_value_to_resource(apArg[0]);
		SyBlob sData;
		char zBuf[8192];
		ph7_int64 nTotal = 0,nBack;
		if( IO_PRIVATE_INVALID(pDev) || pDev->iMagic == IO_PRIVATE_CLOSED_MAGIC
		 || pDev->pStream == 0 || pDev->pStream->xRead == 0 ){
			return PH7_VmThrowException(pCtx,"TypeError",
				"mime_content_type(): supplied resource is not a valid stream resource");
		}
		nBack = PH7_StreamLogicalTell(pDev);
		if( pDev->pStream->xSeek ){
			pDev->pStream->xSeek(pDev->pHandle,0,0 /* SEEK_SET */);
		}
		SyBlobInit(&sData,&pCtx->pVm->sAllocator);
		for(;;){
			ph7_int64 nChunk = (ph7_int64)sizeof(zBuf);
			ph7_int64 n;
			if( nTotal + nChunk > (ph7_int64)FINFO_READ_MAX ){
				nChunk = (ph7_int64)FINFO_READ_MAX - nTotal;
			}
			if( nChunk < 1 ){
				break;
			}
			n = pDev->pStream->xRead(pDev->pHandle,zBuf,nChunk);
			if( n < 1 ){
				break;
			}
			SyBlobAppend(&sData,zBuf,(sxu32)n);
			nTotal += n;
		}
		if( pDev->pStream->xSeek && nBack >= 0 ){
			pDev->pStream->xSeek(pDev->pHandle,nBack,0 /* SEEK_SET */);
		}
		FinfoAnswerBuffer(pCtx,(const unsigned char *)SyBlobData(&sData),
			SyBlobLength(&sData),nTotal,FINFO_MIME_TYPE);
		SyBlobRelease(&sData);
#else
		ph7_result_bool(pCtx,0);
#endif
		return PH7_OK;
	}
	/* The one door whose own signature puts the name first, so php's numbering
	 * and its arginfo agree here. */
	return FinfoFileCommon(pCtx,nArg,apArg,FINFO_MIME_TYPE,0,1,"filename");
}
/*
 * finfo: an ordinary class with one hidden slot. php declares no clone and no
 * serialize handler for it, which is what makes both refusals engine-level
 * rather than a body's.
 */
PH7_PRIVATE sxi32 PH7_VmInstallFileinfo(ph7_vm *pVm)
{
	static const PH7_NativeMethodDef aMethod[] = {
		{ "__construct", PH7_MOD_PUBLIC,
		  "int $flags = FILEINFO_NONE, ?string $magic_database = null", 0,
		  vm_builtin_finfo_construct },
		{ "file", PH7_MOD_PUBLIC,
		  "string $filename, int $flags = FILEINFO_NONE, $context = null", "@string|false",
		  vm_builtin_finfo_file },
		{ "buffer", PH7_MOD_PUBLIC,
		  "string $string, int $flags = FILEINFO_NONE, $context = null", "@string|false",
		  vm_builtin_finfo_buffer },
		{ "set_flags", PH7_MOD_PUBLIC, "int $flags", "@true", vm_builtin_finfo_set_flags },
	};
	static const PH7_NativePropDef aProp[] = {
		{ FINFO_FLAGS_SLOT, PH7_MOD_PRIVATE|PH7_MOD_HIDDEN,
		  { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
	};
	static const PH7_NativeClassSpec sSpec = {
		"finfo", 0, 0, PH7_CLASS_NOCLONE|PH7_CLASS_NOSERIALIZE,
		aMethod, SX_ARRAYSIZE(aMethod), 0, 0,
		aProp, SX_ARRAYSIZE(aProp), 0, 0, 0
	};
	return PH7_InstallNativeClasses(&(*pVm),&sSpec,1);
}
#endif /* PH7_DISABLE_BUILTIN_FUNC */
