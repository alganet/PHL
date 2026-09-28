/**
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
#include <errno.h>
/*
 * Section:
 *    ext/standard's image surface: the IMAGETYPE_* space, the two table
 *    lookups over it (image_type_to_mime_type / image_type_to_extension) and
 *    the container readers behind getimagesize()/getimagesizefromstring().
 * Status:
 *    Stable.
 *
 * php's image_type_to_* pair is one switch each, and the two tables do NOT
 * agree on how many names they know. Three rules fall out of that and are easy
 * to get backwards:
 *
 *   - the MIME lookup is total: every integer has an answer, and the one it
 *     gives a type it does not know is `application/octet-stream` -- which is
 *     also the honest answer for JPC, JPX and JB2, three types it DOES know.
 *     So a caller cannot tell "unknown type" from "raw codestream" by the mime
 *     string alone.
 *   - the EXTENSION lookup is partial: it answers FALSE for a type it does not
 *     know, and it knows four names the mime table does not distinguish
 *     (`.jpc`, `.jpx`, `.jb2`) plus one the mime table splits (WBMP is
 *     `image/vnd.wap.wbmp` but its extension is `.bmp`, BMP's own).
 *   - `$include_dot` is not a branch: php stores each extension WITH its dot
 *     and answers `&imgext[!inc_dot]`, so the flag is a one-byte offset.
 *
 * IMAGETYPE_SVG is not part of the fixed enum at all. php registers it at
 * module init from ext/libxml -- against ext/standard's module number, so the
 * constant belongs to `standard` even though the reader lives elsewhere -- and
 * bumps IMAGETYPE_COUNT as it does. A build without libxml therefore has
 * neither the constant nor the extra count, which is what the guards below
 * reproduce.
 */
#ifndef PH7_DISABLE_BUILTIN_FUNC
/*
 * The fixed part of php's image_filetype enum (ext/standard/php_image.h).
 * IMAGETYPE_JPEG2000 is a userland ALIAS for JPC and not a value of its own,
 * and IMAGETYPE_COUNT is the count rather than a type.
 */
#define PH7_IMG_UNKNOWN   0
#define PH7_IMG_GIF       1
#define PH7_IMG_JPEG      2
#define PH7_IMG_PNG       3
#define PH7_IMG_SWF       4
#define PH7_IMG_PSD       5
#define PH7_IMG_BMP       6
#define PH7_IMG_TIFF_II   7
#define PH7_IMG_TIFF_MM   8
#define PH7_IMG_JPC       9
#define PH7_IMG_JP2      10
#define PH7_IMG_JPX      11
#define PH7_IMG_JB2      12
#define PH7_IMG_SWC      13
#define PH7_IMG_IFF      14
#define PH7_IMG_WBMP     15
#define PH7_IMG_XBM      16
#define PH7_IMG_ICO      17
#define PH7_IMG_WEBP     18
#define PH7_IMG_AVIF     19
#define PH7_IMG_HEIF     20
#ifdef PH7_ENABLE_LIBXML
/* The one registered handler this build installs: php hands out the next id
 * past the fixed enum, so SVG is 21 and IMAGETYPE_COUNT becomes 22. */
#define PH7_IMG_SVG      21
#endif
/*
 * php's mime table. A type with no row answers "application/octet-stream",
 * which three rows below repeat on purpose (php words them that way).
 */
PH7_PRIVATE const char * PH7_ImageTypeMime(ph7_int64 iType)
{
	switch( iType ){
	case PH7_IMG_GIF:     return "image/gif";
	case PH7_IMG_JPEG:    return "image/jpeg";
	case PH7_IMG_PNG:     return "image/png";
	case PH7_IMG_SWF:
	case PH7_IMG_SWC:     return "application/x-shockwave-flash";
	case PH7_IMG_PSD:     return "image/psd";
	case PH7_IMG_BMP:     return "image/bmp";
	case PH7_IMG_TIFF_II:
	case PH7_IMG_TIFF_MM: return "image/tiff";
	case PH7_IMG_IFF:     return "image/iff";
	case PH7_IMG_WBMP:    return "image/vnd.wap.wbmp";
	case PH7_IMG_JPC:     return "application/octet-stream";
	case PH7_IMG_JP2:     return "image/jp2";
	case PH7_IMG_XBM:     return "image/xbm";
	case PH7_IMG_ICO:     return "image/vnd.microsoft.icon";
	case PH7_IMG_WEBP:    return "image/webp";
	case PH7_IMG_AVIF:    return "image/avif";
	case PH7_IMG_HEIF:    return "image/heif";
	default:
#ifdef PH7_ENABLE_LIBXML
		if( iType == PH7_IMG_SVG ){
			return "image/svg+xml";
		}
#endif
		break;
	}
	return "application/octet-stream";
}
/*
 * php's extension table. Every string carries its leading dot; the answer for
 * a type with no row is FALSE, which the caller spells rather than this.
 */
static const char * ImageTypeExt(ph7_int64 iType)
{
	switch( iType ){
	case PH7_IMG_GIF:     return ".gif";
	case PH7_IMG_JPEG:    return ".jpeg";
	case PH7_IMG_PNG:     return ".png";
	case PH7_IMG_SWF:
	case PH7_IMG_SWC:     return ".swf";
	case PH7_IMG_PSD:     return ".psd";
	case PH7_IMG_BMP:
	case PH7_IMG_WBMP:    return ".bmp";
	case PH7_IMG_TIFF_II:
	case PH7_IMG_TIFF_MM: return ".tiff";
	case PH7_IMG_IFF:     return ".iff";
	case PH7_IMG_JPC:     return ".jpc";
	case PH7_IMG_JP2:     return ".jp2";
	case PH7_IMG_JPX:     return ".jpx";
	case PH7_IMG_JB2:     return ".jb2";
	case PH7_IMG_XBM:     return ".xbm";
	case PH7_IMG_ICO:     return ".ico";
	case PH7_IMG_WEBP:    return ".webp";
	case PH7_IMG_AVIF:    return ".avif";
	case PH7_IMG_HEIF:    return ".heif";
	default:
#ifdef PH7_ENABLE_LIBXML
		if( iType == PH7_IMG_SVG ){
			return ".svg";
		}
#endif
		break;
	}
	return 0;
}
/*
 * string image_type_to_mime_type(int $image_type)
 */
PH7_PRIVATE int PH7_builtin_image_type_to_mime_type(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zMime;
	SXUNUSED(nArg); /* arity is enforced centrally */
	zMime = PH7_ImageTypeMime(ph7_value_to_int64(apArg[0]));
	ph7_result_string(pCtx,zMime,-1);
	return PH7_OK;
}
/*
 * string|false image_type_to_extension(int $image_type,bool $include_dot = true)
 */
PH7_PRIVATE int PH7_builtin_image_type_to_extension(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zExt;
	int bDot = TRUE;
	zExt = ImageTypeExt(ph7_value_to_int64(apArg[0]));
	if( zExt == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 1 ){
		bDot = ph7_value_to_bool(apArg[1]);
	}
	/* php answers &imgext[!inc_dot]: the flag is an offset, not a branch. */
	ph7_result_string(pCtx,bDot ? zExt : &zExt[1],-1);
	return PH7_OK;
}
/*
 * ---------------------------------------------------------------------------
 * The byte source every reader below works over.
 * ---------------------------------------------------------------------------
 * php reads an image through a php_stream, and getimagesize() opens one with
 * STREAM_MUST_SEEK so every handler may seek freely. The two doors this engine
 * has are an open vfs handle (getimagesize) and a script STRING
 * (getimagesizefromstring), so ImgReader carries both behind one small
 * read/seek/getc surface.
 *
 * Two properties of php's stream are contract here rather than convenience:
 *   - a FORWARD seek always succeeds, even past the end. php's memory stream
 *     says so outright and lseek(2) agrees, which is what makes php's own
 *     `if (php_stream_seek(...)) return NULL` branches unreachable;
 *   - EOF is a STICKY flag raised by a read that could not be filled, not a
 *     position test. Exactly one reader looks at it (JPEG-2000's component
 *     count), and it looks to tell "the file says zero components" from "the
 *     file ended where the count should be".
 * A handle is read AHEAD into a small window: php's JPEG marker hunt walks a
 * file one byte at a time, and a per-byte trip through the vfs would make a
 * large corrupt JPEG quadratic in syscalls.
 */
#define IMG_BUFSZ 4096
typedef struct ImgReader ImgReader;
struct ImgReader {
	ph7_context *pCtx;            /* whose name a read failure is reported under */
	const ph7_io_stream *pStream; /* vfs backend, or 0 for the memory one */
	void *pHandle;                /* the open vfs handle */
	const unsigned char *zData;   /* memory backend: the whole string */
	ph7_int64 nData;
	ph7_int64 iPos;               /* absolute logical position */
	ph7_int64 iRaw;               /* where the vfs handle itself stands */
	ph7_int64 iBufBase;           /* absolute offset of zBuf[0] */
	int nBuf;                     /* valid bytes in zBuf */
	int bEof;                     /* a read could not be filled */
	unsigned char zBuf[IMG_BUFSZ];
};
/*
 * Fill the window so that it covers p->iPos. Answers how many bytes are
 * available there (0 at the end of the handle).
 */
static int ImgFill(ImgReader *p)
{
	ph7_int64 n;
	if( p->iPos >= p->iBufBase && p->iPos < p->iBufBase + p->nBuf ){
		return (int)(p->iBufBase + p->nBuf - p->iPos);
	}
	if( p->iRaw != p->iPos ){
		if( p->pStream->xSeek == 0
		 || p->pStream->xSeek(p->pHandle,p->iPos,0/*SEEK_SET*/) != PH7_OK ){
			p->nBuf = 0;
			return 0;
		}
		p->iRaw = p->iPos;
	}
	p->iBufBase = p->iPos;
	p->nBuf = 0;
	n = p->pStream->xRead(p->pHandle,p->zBuf,(ph7_int64)sizeof(p->zBuf));
	if( n < 0 ){
		/* php's plain-file read op announces a device that refuses -- reading a
		 * DIRECTORY is the everyday way to reach it here -- and the count it
		 * reports is the stream CHUNK size php was filling, never what the
		 * caller asked for. Silent for every other wrapper, as php's is. */
		if( p->pStream == p->pCtx->pVm->pDefStream ){
			ph7_context_throw_error_format(p->pCtx,PH7_CTX_NOTICE,
				"Read of %u bytes failed with errno=%d %s",
				8192u,errno,VfsStrerror(errno));
		}
		return 0;
	}
	if( n > 0 ){
		p->nBuf = (int)n;
		p->iRaw += n;
	}
	return p->nBuf;
}
/*
 * Read up to nWant bytes. A short answer raises the sticky EOF flag, which is
 * php's own rule: its stream marks eof when a read cannot be filled.
 */
static ph7_int64 ImgRead(ImgReader *p,void *pOut,ph7_int64 nWant)
{
	unsigned char *zOut = (unsigned char *)pOut;
	ph7_int64 nGot = 0;
	if( nWant <= 0 ){
		return 0;
	}
	if( p->pStream == 0 ){
		nGot = p->nData - p->iPos;
		if( nGot < 0 ){
			nGot = 0;
		}
		if( nGot > nWant ){
			nGot = nWant;
		}
		if( nGot > 0 ){
			SyMemcpy(&p->zData[p->iPos],zOut,(sxu32)nGot);
		}
		p->iPos += nGot;
	}else{
		while( nGot < nWant ){
			int nAvail = ImgFill(p);
			int nCopy;
			if( nAvail < 1 ){
				break;
			}
			nCopy = (int)SXMIN((ph7_int64)nAvail,nWant - nGot);
			SyMemcpy(&p->zBuf[p->iPos - p->iBufBase],&zOut[nGot],(sxu32)nCopy);
			nGot += nCopy;
			p->iPos += nCopy;
		}
	}
	if( nGot < nWant ){
		p->bEof = TRUE;
	}
	return nGot;
}
/* A forward seek never fails; a rewind clears the sticky EOF, as php's does. */
static void ImgSeekCur(ImgReader *p,ph7_int64 nOfft)
{
	p->iPos += nOfft;
	if( p->iPos < 0 ){
		p->iPos = 0;
	}
}
static void ImgRewind(ImgReader *p)
{
	p->iPos = 0;
	p->bEof = FALSE;
}
/* php_stream_getc: the byte, or -1 at the end. */
static int ImgGetc(ImgReader *p)
{
	unsigned char c;
	if( ImgRead(p,&c,1) != 1 ){
		return -1;
	}
	return (int)c;
}
/* php_read2/php_read4: big-endian, and ZERO when the bytes are not there. */
static unsigned int ImgRead2(ImgReader *p)
{
	unsigned char a[2];
	if( ImgRead(p,a,2) != 2 ){
		return 0;
	}
	return ((unsigned int)a[0] << 8) + (unsigned int)a[1];
}
static unsigned int ImgRead4(ImgReader *p)
{
	unsigned char a[4];
	if( ImgRead(p,a,4) != 4 ){
		return 0;
	}
	return ((unsigned int)a[0] << 24) + ((unsigned int)a[1] << 16)
	     + ((unsigned int)a[2] << 8)  + (unsigned int)a[3];
}
/*
 * What a reader answers -- php's php_gfxinfo, with the same "0 means absent"
 * convention for `bits` and `channels`: a format with no answer for one of
 * them leaves it zero and the key never reaches the array. The two UNIT
 * strings are php 8.5's, and only a registered handler ever sets them.
 */
typedef struct ImgInfo ImgInfo;
struct ImgInfo {
	unsigned int nWidth;
	unsigned int nHeight;
	unsigned int nBits;
	unsigned int nChannels;
	const char *zWidthUnit;   /* 0 == php's "px" default */
	const char *zHeightUnit;
};
/* One diagnostic door, so every sentence below carries php's docref prefix. */
static void ImgThrowFmt(ph7_context *pCtx,sxi32 iErr,const char *zFmt,...)
{
	va_list ap;
	va_start(ap,zFmt);
	PH7_VmThrowErrorAp(pCtx->pVm,0,iErr,zFmt,ap);
	va_end(ap);
}
/*
 * GIF. The packed byte's high bit says whether there is a global colour
 * table at all, and only then do its low three bits mean a depth; the three
 * channels are php's own constant and not something the file says.
 */
static int ImgHandleGif(ImgReader *p,ImgInfo *pOut)
{
	unsigned char dim[5];
	ImgSeekCur(p,3);
	if( ImgRead(p,dim,sizeof(dim)) != (ph7_int64)sizeof(dim) ){
		return 0;
	}
	pOut->nWidth    = (unsigned int)dim[0] | ((unsigned int)dim[1] << 8);
	pOut->nHeight   = (unsigned int)dim[2] | ((unsigned int)dim[3] << 8);
	pOut->nBits     = (dim[4] & 0x80) ? (((unsigned int)dim[4] & 0x07) + 1) : 0;
	pOut->nChannels = 3; /* always */
	return 1;
}
/* PSD. Its header spells the HEIGHT first. */
static int ImgHandlePsd(ImgReader *p,ImgInfo *pOut)
{
	unsigned char dim[8];
	ImgSeekCur(p,11);
	if( ImgRead(p,dim,sizeof(dim)) != (ph7_int64)sizeof(dim) ){
		return 0;
	}
	pOut->nHeight = ((unsigned int)dim[0] << 24) + ((unsigned int)dim[1] << 16)
	              + ((unsigned int)dim[2] << 8)  + (unsigned int)dim[3];
	pOut->nWidth  = ((unsigned int)dim[4] << 24) + ((unsigned int)dim[5] << 16)
	              + ((unsigned int)dim[6] << 8)  + (unsigned int)dim[7];
	return 1;
}
/*
 * BMP. The DIB header's own SIZE decides which of two layouts to read, and
 * php accepts exactly three shapes: the 12-byte OS/2 core header, anything
 * from 13 to 64 bytes, and the two Windows extensions (108 and 124). A
 * height read from the modern layout is SIGNED -- negative means top-down --
 * and php takes its absolute value, so the one height no int32 can negate
 * (0x80000000) comes back unchanged. That is spelled in unsigned arithmetic
 * below because the signed spelling is undefined behaviour php gets away
 * with; the ANSWER is php's.
 */
static int ImgHandleBmp(ImgReader *p,ImgInfo *pOut)
{
	unsigned char dim[16];
	unsigned int nSize,nHeight;
	ImgSeekCur(p,11);
	if( ImgRead(p,dim,sizeof(dim)) != (ph7_int64)sizeof(dim) ){
		return 0;
	}
	nSize = ((unsigned int)dim[3] << 24) + ((unsigned int)dim[2] << 16)
	      + ((unsigned int)dim[1] << 8)  + (unsigned int)dim[0];
	if( nSize == 12 ){
		pOut->nWidth  = ((unsigned int)dim[5] << 8) + (unsigned int)dim[4];
		pOut->nHeight = ((unsigned int)dim[7] << 8) + (unsigned int)dim[6];
		pOut->nBits   = (unsigned int)dim[11];
		return 1;
	}
	if( nSize > 12 && (nSize <= 64 || nSize == 108 || nSize == 124) ){
		pOut->nWidth = ((unsigned int)dim[7] << 24) + ((unsigned int)dim[6] << 16)
		             + ((unsigned int)dim[5] << 8)  + (unsigned int)dim[4];
		nHeight      = ((unsigned int)dim[11] << 24) + ((unsigned int)dim[10] << 16)
		             + ((unsigned int)dim[9] << 8)  + (unsigned int)dim[8];
		if( nHeight & 0x80000000u ){
			/* abs() of the 32-bit signed reading: the two's complement negation,
			 * which leaves 0x80000000 exactly where it was. */
			nHeight = (unsigned int)(0u - nHeight);
		}
		pOut->nHeight = nHeight;
		pOut->nBits   = ((unsigned int)dim[15] << 8) + (unsigned int)dim[14];
		return 1;
	}
	return 0;
}
/*
 * SWF. The frame RECT is a bit field: five bits of WIDTH, then four values
 * of that width, in twips (a twentieth of a pixel).
 */
static unsigned long ImgSwfBits(const unsigned char *zBuf,unsigned int nPos,unsigned int nCount)
{
	unsigned long nRes = 0;
	unsigned int i;
	for( i = nPos ; i < nPos + nCount ; ++i ){
		nRes = nRes +
			((unsigned long)((zBuf[i / 8] >> (7 - (i % 8))) & 0x01) << (nCount - (i - nPos) - 1));
	}
	return nRes;
}
static int ImgHandleSwf(ImgReader *p,ImgInfo *pOut)
{
	unsigned char a[32];
	unsigned long nBits;
	ImgSeekCur(p,5);
	if( ImgRead(p,a,sizeof(a)) != (ph7_int64)sizeof(a) ){
		return 0;
	}
	nBits = ImgSwfBits(a,0,5);
	pOut->nWidth  = (unsigned int)((ImgSwfBits(a,5 + (unsigned int)nBits,(unsigned int)nBits)
		- ImgSwfBits(a,5,(unsigned int)nBits)) / 20);
	pOut->nHeight = (unsigned int)((ImgSwfBits(a,5 + 3 * (unsigned int)nBits,(unsigned int)nBits)
		- ImgSwfBits(a,5 + 2 * (unsigned int)nBits,(unsigned int)nBits)) / 20);
	return 1;
}
/* PNG. IHDR is fixed and first, so nine bytes past the signature is all of it. */
static int ImgHandlePng(ImgReader *p,ImgInfo *pOut)
{
	unsigned char dim[9];
	ImgSeekCur(p,8);
	if( ImgRead(p,dim,sizeof(dim)) < (ph7_int64)sizeof(dim) ){
		return 0;
	}
	pOut->nWidth  = ((unsigned int)dim[0] << 24) + ((unsigned int)dim[1] << 16)
	              + ((unsigned int)dim[2] << 8)  + (unsigned int)dim[3];
	pOut->nHeight = ((unsigned int)dim[4] << 24) + ((unsigned int)dim[5] << 16)
	              + ((unsigned int)dim[6] << 8)  + (unsigned int)dim[7];
	pOut->nBits   = (unsigned int)dim[8];
	return 1;
}
/*
 * ---------------------------------------------------------------------------
 * JPEG: a marker walk, and the only reader with a second answer.
 * ---------------------------------------------------------------------------
 * Every JPEG segment is 0xFF, a marker byte, then a big-endian length that
 * COUNTS ITSELF. Three rules of php's walk are worth spelling out because
 * each is visible from PHP:
 *   - the first marker is read with the 0xFF already in hand (the detection
 *     ladder consumed it), so the "extraneous bytes before marker" warning
 *     can only ever come from the SECOND marker onwards;
 *   - only the FIRST frame header answers. A second SOFn is skipped like any
 *     other segment, so a progressive file's later frames never move the size;
 *   - the APP payloads are collected only when the caller asked for
 *     $image_info, and only the FIRST of each APPn number is kept. Without
 *     that argument php returns at the frame header and never reads them at
 *     all.
 */
#define IMG_M_SOF0  0xC0
#define IMG_M_SOI   0xD8
#define IMG_M_EOI   0xD9
#define IMG_M_SOS   0xDA
#define IMG_M_APP0  0xE0
#define IMG_M_APP15 0xEF
/* Is this marker one of the thirteen frame headers php reads a size from?
 * C4 (define Huffman table) and CC (define arithmetic coding) sit inside the
 * C0..CF run and are NOT frame headers, which is what the two holes are. */
static int ImgJpegIsSof(unsigned int m)
{
	return (m >= 0xC0 && m <= 0xCF) && m != 0xC4 && m != 0xC8 && m != 0xCC;
}
/*
 * The next marker byte. `bFfRead` says the leading 0xFF is already consumed.
 * A run of 0xFF bytes is padding between the fill byte and the marker, and
 * anything before the first 0xFF is counted and reported.
 */
static unsigned int ImgJpegNextMarker(ph7_context *pCtx,ImgReader *p,int bFfRead)
{
	int c;
	if( !bFfRead ){
		sxi64 nExtra = 0;
		for(;;){
			c = ImgGetc(p);
			if( c < 0 ){
				return IMG_M_EOI; /* we hit the end */
			}
			if( c == 0xFF ){
				break;
			}
			nExtra++;
		}
		if( nExtra > 0 ){
			ImgThrowFmt(pCtx,PH7_CTX_WARNING,
				"%s(): Corrupt JPEG data: %qd extraneous bytes before marker",
				ph7_function_name(pCtx),nExtra);
		}
	}
	do {
		c = ImgGetc(p);
		if( c < 0 ){
			return IMG_M_EOI;
		}
	}while( c == 0xFF );
	return (unsigned int)c;
}
/* Skip a segment whose length word is where it should be. A length below two
 * cannot even cover itself, which is php's "stop here". */
static int ImgJpegSkipSegment(ImgReader *p)
{
	unsigned int nLen = ImgRead2(p);
	if( nLen < 2 ){
		return 0;
	}
	ImgSeekCur(p,(ph7_int64)(nLen - 2));
	return 1;
}
/* Collect one APPn payload under php's own "APP%d" key. */
static int ImgJpegReadApp(ph7_context *pCtx,ImgReader *p,unsigned int nMarker,ph7_value *pInfo)
{
	unsigned int nLen = ImgRead2(p);
	char zKey[16];
	char *zBuf;
	int rc = 1;
	if( nLen < 2 ){
		return 0;
	}
	nLen -= 2; /* the length counts itself */
	zBuf = (char *)ph7_context_alloc_chunk(pCtx,nLen > 0 ? nLen : 1,FALSE,FALSE);
	if( zBuf == 0 ){
		return 0;
	}
	if( ImgRead(p,zBuf,(ph7_int64)nLen) != (ph7_int64)nLen ){
		ph7_context_free_chunk(pCtx,zBuf);
		return 0;
	}
	SyBufferFormat(zKey,sizeof(zKey),"APP%u",nMarker - IMG_M_APP0);
	if( ph7_array_fetch(pInfo,zKey,-1) == 0 ){
		/* php keeps only the FIRST tag of each kind. */
		ph7_value *pVal = ph7_context_new_scalar(pCtx);
		if( pVal ){
			ph7_value_string(pVal,zBuf,(int)nLen);
			ph7_array_add_strkey_elem(pInfo,zKey,pVal);
			ph7_context_release_value(pCtx,pVal);
		}else{
			rc = 0;
		}
	}
	ph7_context_free_chunk(pCtx,zBuf);
	return rc;
}
static int ImgHandleJpeg(ph7_context *pCtx,ImgReader *p,ImgInfo *pOut,ph7_value *pInfo)
{
	unsigned int nMarker;
	int bFfRead = 1,bHave = 0;
	for(;;){
		nMarker = ImgJpegNextMarker(pCtx,p,bFfRead);
		bFfRead = 0;
		if( ImgJpegIsSof(nMarker) ){
			if( !bHave ){
				unsigned int nLen = ImgRead2(p);
				bHave = 1;
				pOut->nBits     = (unsigned int)ImgGetc(p);
				pOut->nHeight   = ImgRead2(p);
				pOut->nWidth    = ImgRead2(p);
				pOut->nChannels = (unsigned int)ImgGetc(p);
				if( pInfo == 0 || nLen < 8 ){
					return 1;
				}
				ImgSeekCur(p,(ph7_int64)(nLen - 8));
			}else if( !ImgJpegSkipSegment(p) ){
				return bHave;
			}
		}else if( nMarker >= IMG_M_APP0 && nMarker <= IMG_M_APP15 ){
			if( pInfo ){
				if( !ImgJpegReadApp(pCtx,p,nMarker,pInfo) ){
					return bHave;
				}
			}else if( !ImgJpegSkipSegment(p) ){
				return bHave;
			}
		}else if( nMarker == IMG_M_SOS || nMarker == IMG_M_EOI ){
			/* about to hit the image data, or at the end */
			return bHave;
		}else if( !ImgJpegSkipSegment(p) ){
			return bHave;
		}
	}
}
/*
 * ---------------------------------------------------------------------------
 * JPEG 2000: the raw codestream, and the box wrapper around it.
 * ---------------------------------------------------------------------------
 * A JPEG-2000 codestream may give every component its own depth, so there is
 * no single answer for "bit depth"; php reports the DEEPEST it saw. The one
 * place it consults end-of-file rather than a value is the component count: a
 * zero read at the end of a truncated file is a refusal, while a file that
 * genuinely says zero components is a size with no depth.
 */
#define IMG_J2K_SIZ 0x51
static int ImgHandleJpc(ph7_context *pCtx,ImgReader *p,ImgInfo *pOut)
{
	int iHighest,iDepth;
	unsigned int i;
	if( ImgGetc(p) != IMG_J2K_SIZ ){
		ImgThrowFmt(pCtx,PH7_CTX_WARNING,
			"%s(): JPEG2000 codestream corrupt(Expected SIZ marker not found after SOC)",
			ph7_function_name(pCtx));
		return 0;
	}
	ImgRead2(p); /* Lsiz */
	ImgRead2(p); /* Rsiz */
	pOut->nWidth  = ImgRead4(p); /* Xsiz */
	pOut->nHeight = ImgRead4(p); /* Ysiz */
	ImgSeekCur(p,24);            /* the four offsets and the two tile sizes */
	pOut->nChannels = ImgRead2(p); /* Csiz */
	if( (pOut->nChannels == 0 && p->bEof) || pOut->nChannels > 256 ){
		return 0;
	}
	iHighest = 0;
	for( i = 0 ; i < pOut->nChannels ; i++ ){
		iDepth = ImgGetc(p); /* Ssiz[i] */
		iDepth++;
		if( iDepth > iHighest ){
			iHighest = iDepth;
		}
		ImgGetc(p); /* XRsiz[i] */
		ImgGetc(p); /* YRsiz[i] */
	}
	pOut->nBits = (unsigned int)iHighest;
	return 1;
}
/*
 * JP2 is a BOX container and a file may hold several codestreams; php reads
 * the first "jp2c" at the root and hands its three leading bytes to the raw
 * reader as if they had been the file-type check.
 */
static int ImgHandleJp2(ph7_context *pCtx,ImgReader *p,ImgInfo *pOut)
{
	int bHave = 0;
	for(;;){
		unsigned int nLen;
		unsigned char zType[4];
		nLen = ImgRead4(p);                    /* LBox */
		if( ImgRead(p,zType,4) != 4 ){         /* TBox */
			break;                             /* a general "out of stream" */
		}
		if( nLen == 1 ){
			return 0;                          /* XLBoxes are not handled */
		}
		if( SyMemcmp(zType,"jp2c",4) == 0 ){
			ImgSeekCur(p,3); /* skip past where a file-type check would have read */
			bHave = ImgHandleJpc(pCtx,p,pOut);
			break;
		}
		if( (int)nLen <= 0 ){
			break;                             /* that was the last box */
		}
		/* php seeks LBox-8 in UNSIGNED 32-bit arithmetic, so a box shorter than
		 * its own header steps forward by nearly 4 GB rather than backwards. */
		ImgSeekCur(p,(ph7_int64)(sxu32)(nLen - 8));
	}
	if( !bHave ){
		ImgThrowFmt(pCtx,PH7_CTX_WARNING,"%s(): JP2 file has no codestreams at root level",
			ph7_function_name(pCtx));
	}
	return bHave;
}
/*
 * ---------------------------------------------------------------------------
 * TIFF: the first image file directory, and nothing else.
 * ---------------------------------------------------------------------------
 * php walks the IFD's entries for the four size tags and answers only when it
 * found BOTH a width and a height. The entry VALUE is read out of the twelve
 * byte record itself, so only the five formats that fit inline are read and
 * every other one is skipped -- an image whose size is stored out of line has
 * no answer at all. A signed reading is widened to the platform word before it
 * lands in the answer, so a negative width comes back as its unsigned 32-bit
 * reading.
 */
#define IMG_TIFF_TAG_WIDTH       0x0100
#define IMG_TIFF_TAG_HEIGHT      0x0101
#define IMG_TIFF_TAG_COMP_WIDTH  0xA002
#define IMG_TIFF_TAG_COMP_HEIGHT 0xA003
static unsigned int ImgIfdGet16u(const unsigned char *z,int bMotorola)
{
	return bMotorola ? (((unsigned int)z[0] << 8) | z[1]) : (((unsigned int)z[1] << 8) | z[0]);
}
static sxi32 ImgIfdGet16s(const unsigned char *z,int bMotorola)
{
	return (sxi32)(sxi16)ImgIfdGet16u(z,bMotorola);
}
static unsigned int ImgIfdGet32u(const unsigned char *z,int bMotorola)
{
	if( bMotorola ){
		return ((unsigned int)z[0] << 24) | ((unsigned int)z[1] << 16)
		     | ((unsigned int)z[2] << 8)  | (unsigned int)z[3];
	}
	return ((unsigned int)z[3] << 24) | ((unsigned int)z[2] << 16)
	     | ((unsigned int)z[1] << 8)  | (unsigned int)z[0];
}
static sxi32 ImgIfdGet32s(const unsigned char *z,int bMotorola)
{
	return (sxi32)ImgIfdGet32u(z,bMotorola);
}
static int ImgHandleTiff(ph7_context *pCtx,ImgReader *p,ImgInfo *pOut,int bMotorola)
{
	unsigned char zPtr[4];
	unsigned char *zDir;
	sxu32 nDirSize;
	int i,nEntries;
	sxu64 nWidth = 0,nHeight = 0;
	if( ImgRead(p,zPtr,4) != 4 ){
		return 0;
	}
	/* The IFD offset is absolute; php spells the move as a relative one from
	 * the eight bytes it has read, which is the same place. */
	p->iPos = (ph7_int64)ImgIfdGet32u(zPtr,bMotorola);
	if( ImgRead(p,zPtr,2) != 2 ){
		return 0;
	}
	nEntries = (int)ImgIfdGet16u(zPtr,bMotorola);
	/* 2 for the count, twelve per entry, four for the offset of the next IFD */
	nDirSize = 2 + 12 * (sxu32)nEntries + 4;
	zDir = (unsigned char *)ph7_context_alloc_chunk(pCtx,nDirSize,FALSE,FALSE);
	if( zDir == 0 ){
		return 0;
	}
	if( ImgRead(p,&zDir[2],(ph7_int64)(nDirSize - 2)) != (ph7_int64)(nDirSize - 2) ){
		ph7_context_free_chunk(pCtx,zDir);
		return 0;
	}
	for( i = 0 ; i < nEntries ; i++ ){
		const unsigned char *zEntry = &zDir[2 + i * 12];
		unsigned int nTag  = ImgIfdGet16u(zEntry,bMotorola);
		unsigned int nType = ImgIfdGet16u(&zEntry[2],bMotorola);
		sxu64 nValue;
		switch( nType ){
		case 1:  /* BYTE  */
		case 6:  /* SBYTE */
			nValue = (sxu64)zEntry[8];
			break;
		case 3:  /* USHORT */
			nValue = (sxu64)ImgIfdGet16u(&zEntry[8],bMotorola);
			break;
		case 8:  /* SSHORT */
			nValue = (sxu64)(sxi64)ImgIfdGet16s(&zEntry[8],bMotorola);
			break;
		case 4:  /* ULONG */
			nValue = (sxu64)ImgIfdGet32u(&zEntry[8],bMotorola);
			break;
		case 9:  /* SLONG */
			nValue = (sxu64)(sxi64)ImgIfdGet32s(&zEntry[8],bMotorola);
			break;
		default:
			continue; /* the value is not in the record */
		}
		if( nTag == IMG_TIFF_TAG_WIDTH || nTag == IMG_TIFF_TAG_COMP_WIDTH ){
			nWidth = nValue;
		}else if( nTag == IMG_TIFF_TAG_HEIGHT || nTag == IMG_TIFF_TAG_COMP_HEIGHT ){
			nHeight = nValue;
		}
	}
	ph7_context_free_chunk(pCtx,zDir);
	if( nWidth && nHeight ){
		pOut->nWidth  = (unsigned int)nWidth;
		pOut->nHeight = (unsigned int)nHeight;
		return 1;
	}
	return 0;
}
/*
 * IFF. FORM containers hold chunks, each PADDED to an even length, and the
 * one php wants is BMHD. A BMHD whose dimensions are out of range does not
 * end the walk: php reads its nine bytes, refuses them and loops again from
 * wherever that left the file -- which is nine bytes into a chunk rather
 * than at a chunk header, so the walk carries on misaligned rather than
 * stopping. That is reproduced, not corrected.
 */
static int ImgHandleIff(ImgReader *p,ImgInfo *pOut)
{
	unsigned char a[10];
	if( ImgRead(p,a,8) != 8 ){
		return 0;
	}
	if( SyMemcmp(&a[4],"ILBM",4) != 0 && SyMemcmp(&a[4],"PBM ",4) != 0 ){
		return 0;
	}
	for(;;){
		sxi32 iChunk,iSize;
		if( ImgRead(p,a,8) != 8 ){
			return 0;
		}
		iChunk = ImgIfdGet32s(&a[0],1);
		iSize  = ImgIfdGet32s(&a[4],1);
		if( iSize < 0 ){
			return 0;
		}
		if( (iSize & 1) == 1 ){
			if( iSize == SXI32_HIGH ){
				return 0;
			}
			iSize++;
		}
		if( iChunk == 0x424d4844 ){ /* "BMHD" */
			sxi32 iW,iH,iBits;
			if( iSize < 9 || ImgRead(p,a,9) != 9 ){
				return 0;
			}
			iW    = ImgIfdGet16s(&a[0],1);
			iH    = ImgIfdGet16s(&a[2],1);
			iBits = (sxi32)(a[8] & 0xFF);
			if( iW > 0 && iH > 0 && iBits > 0 && iBits < 33 ){
				pOut->nWidth  = (unsigned int)iW;
				pOut->nHeight = (unsigned int)iH;
				pOut->nBits   = (unsigned int)iBits;
				return 1;
			}
		}else{
			ImgSeekCur(p,(ph7_int64)iSize);
		}
	}
}
/*
 * WBMP has no signature at all, so it is the fallback the ladder reaches
 * only once every real one has failed -- and its whole shape is the screen:
 * a zero type byte, an extended-header run terminated by a clear high bit,
 * then two seven-bit-per-byte counts. php bounds both at 2048, which is what
 * keeps an arbitrary file from parsing as a huge bitmap.
 */
static int ImgGetWbmp(ImgReader *p,ImgInfo *pOut)
{
	int i,nWidth = 0,nHeight = 0;
	ImgRewind(p);
	if( ImgGetc(p) != 0 ){          /* type */
		return 0;
	}
	do {                            /* the extended header */
		i = ImgGetc(p);
		if( i < 0 ){
			return 0;
		}
	}while( i & 0x80 );
	do {
		i = ImgGetc(p);
		if( i < 0 ){
			return 0;
		}
		nWidth = (nWidth << 7) | (i & 0x7F);
		if( nWidth > 2048 ){
			return 0;
		}
	}while( i & 0x80 );
	do {
		i = ImgGetc(p);
		if( i < 0 ){
			return 0;
		}
		nHeight = (nHeight << 7) | (i & 0x7F);
		if( nHeight > 2048 ){
			return 0;
		}
	}while( i & 0x80 );
	if( !nHeight || !nWidth ){
		return 0;
	}
	if( pOut ){
		pOut->nWidth  = (unsigned int)nWidth;
		pOut->nHeight = (unsigned int)nHeight;
	}
	return 1;
}
/*
 * XBM is C source, so php reads it a LINE at a time and runs one sscanf
 * pattern over each: `#define %s %d`. Four rules of that pattern are visible
 * from PHP and easy to lose:
 *   - the `#` is a LITERAL, and a literal never skips whitespace, so a line
 *     indented at all matches nothing;
 *   - the name is one non-whitespace RUN, and the part that counts is what
 *     follows its LAST underscore -- a name with no underscore is compared
 *     whole. Since only whitespace ends the run, `x_width\x013` is ONE token
 *     and the line has no number in it at all;
 *   - the whole line is read as a C string, so an embedded NUL ends it;
 *   - the value is a C `int` widened UNSIGNED, so a negative define comes
 *     back as its 32-bit reading. The scan below does its own arithmetic
 *     rather than calling the platform's sscanf, whose overflow answer is its
 *     own: this reproduces the saturate-at-the-64-bit-bound-then-truncate that
 *     the oracle shows, so a Windows build answers what a POSIX one does.
 */
static int ImgIsSpace(int c)
{
	return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}
static int ImgXbmScanDefine(const char *zLine,int nLine,const char **pzName,int *pnName,sxu32 *pnVal)
{
	int i,bNeg = 0,bDigit = 0,bOver = 0;
	sxu64 nMag = 0,nBound;
	/* sscanf() reads a C string: the line stops at its first NUL. */
	for( i = 0 ; i < nLine ; i++ ){
		if( zLine[i] == 0 ){
			nLine = i;
			break;
		}
	}
	if( nLine < 7 || SyMemcmp(zLine,"#define",7) != 0 ){
		return 0;
	}
	i = 7;
	while( i < nLine && ImgIsSpace(zLine[i]) ){
		i++;
	}
	*pzName = &zLine[i];
	while( i < nLine && !ImgIsSpace(zLine[i]) ){
		i++;
	}
	*pnName = (int)(&zLine[i] - *pzName);
	if( *pnName < 1 ){
		return 0;
	}
	while( i < nLine && ImgIsSpace(zLine[i]) ){
		i++;
	}
	if( i < nLine && (zLine[i] == '-' || zLine[i] == '+') ){
		bNeg = (zLine[i] == '-');
		i++;
	}
	/* strtol()'s bound: LONG_MAX going up, LONG_MIN going down. */
	nBound = (sxu64)SXI64_HIGH + (sxu64)(bNeg ? 1 : 0);
	while( i < nLine && zLine[i] >= '0' && zLine[i] <= '9' ){
		bDigit = 1;
		if( !bOver ){
			if( nMag > (nBound - (sxu64)(zLine[i] - '0')) / 10 ){
				bOver = 1;
			}else{
				nMag = nMag * 10 + (sxu64)(zLine[i] - '0');
			}
		}
		i++;
	}
	if( !bDigit ){
		return 0;
	}
	if( bOver ){
		nMag = nBound;
	}
	/* The magnitude lands in a C `int`: keep its low 32 bits, negated first
	 * when the define carried a sign. */
	*pnVal = (sxu32)(bNeg ? (sxu64)(0 - nMag) : nMag);
	return 1;
}
static int ImgGetXbm(ph7_context *pCtx,ImgReader *p,ImgInfo *pOut)
{
	SyBlob sLine;
	unsigned int nWidth = 0,nHeight = 0;
	int bDone = 0;
	ImgRewind(p);
	SyBlobInit(&sLine,&pCtx->pVm->sAllocator);
	while( !bDone ){
		const char *zName,*zType;
		int nName,nType,k;
		sxu32 nVal;
		SyBlobReset(&sLine);
		for(;;){
			char c;
			int iByte = ImgGetc(p);
			if( iByte < 0 ){
				break;
			}
			c = (char)iByte;
			SyBlobAppend(&sLine,&c,1);
			if( c == '\n' ){
				break;
			}
		}
		if( SyBlobLength(&sLine) < 1 ){
			break; /* php_stream_gets() answered nothing: the file is done */
		}
		if( !ImgXbmScanDefine((const char *)SyBlobData(&sLine),(int)SyBlobLength(&sLine),
			&zName,&nName,&nVal) ){
			continue;
		}
		zType = zName;
		nType = nName;
		for( k = nName ; k > 0 ; k-- ){
			if( zName[k-1] == '_' ){
				zType = &zName[k];
				nType = nName - k;
				break;
			}
		}
		if( nType == 5 && SyMemcmp(zType,"width",5) == 0 ){
			nWidth = (unsigned int)nVal;
			if( nHeight ){
				bDone = 1;
			}
		}
		if( nType == 6 && SyMemcmp(zType,"height",6) == 0 ){
			nHeight = (unsigned int)nVal;
			if( nWidth ){
				bDone = 1;
			}
		}
	}
	SyBlobRelease(&sLine);
	if( nWidth && nHeight ){
		if( pOut ){
			pOut->nWidth  = nWidth;
			pOut->nHeight = nHeight;
		}
		return 1;
	}
	return 0;
}
/*
 * ICO holds a DIRECTORY of images and php answers for the one with the
 * greatest colour depth, ties going to the LAST it read. The two dimension
 * bytes are single bytes, and zero means 256 -- which is what makes a
 * 256-pixel icon the only one whose stored size is smaller than its answer.
 */
static int ImgHandleIco(ImgReader *p,ImgInfo *pOut)
{
	unsigned char dim[16];
	int nIcons;
	if( ImgRead(p,dim,2) != 2 ){
		return 0;
	}
	nIcons = (int)(((unsigned int)dim[1] << 8) + (unsigned int)dim[0]);
	if( nIcons < 1 || nIcons > 255 ){
		return 0;
	}
	while( nIcons > 0 ){
		unsigned int nBits;
		if( ImgRead(p,dim,sizeof(dim)) != (ph7_int64)sizeof(dim) ){
			break;
		}
		nBits = ((unsigned int)dim[7] << 8) + (unsigned int)dim[6];
		if( nBits >= pOut->nBits ){
			pOut->nWidth  = (unsigned int)dim[0];
			pOut->nHeight = (unsigned int)dim[1];
			pOut->nBits   = nBits;
		}
		nIcons--;
	}
	if( pOut->nWidth == 0 ){
		pOut->nWidth = 256;
	}
	if( pOut->nHeight == 0 ){
		pOut->nHeight = 256;
	}
	return 1;
}
/*
 * WEBP: three container flavours behind one RIFF chunk, each spelling its
 * size differently -- fourteen bits per axis for the lossy bitstream, a
 * packed 14/14 for the lossless one, and 24 bits MINUS ONE for the extended
 * header. All three are one byte per sample, which php states rather than
 * reads.
 */
static int ImgHandleWebp(ImgReader *p,ImgInfo *pOut)
{
	unsigned char zBuf[18];
	if( ImgRead(p,zBuf,18) != 18 ){
		return 0;
	}
	if( SyMemcmp(zBuf,"VP8",3) != 0 ){
		return 0;
	}
	switch( zBuf[3] ){
	case ' ':
		pOut->nWidth  = (unsigned int)zBuf[14] + (((unsigned int)zBuf[15] & 0x3F) << 8);
		pOut->nHeight = (unsigned int)zBuf[16] + (((unsigned int)zBuf[17] & 0x3F) << 8);
		break;
	case 'L':
		pOut->nWidth  = (unsigned int)zBuf[9] + (((unsigned int)zBuf[10] & 0x3F) << 8) + 1;
		pOut->nHeight = ((unsigned int)zBuf[10] >> 6) + ((unsigned int)zBuf[11] << 2)
		              + (((unsigned int)zBuf[12] & 0x0F) << 10) + 1;
		break;
	case 'X':
		pOut->nWidth  = (unsigned int)zBuf[12] + ((unsigned int)zBuf[13] << 8)
		              + ((unsigned int)zBuf[14] << 16) + 1;
		pOut->nHeight = (unsigned int)zBuf[15] + ((unsigned int)zBuf[16] << 8)
		              + ((unsigned int)zBuf[17] << 16) + 1;
		break;
	default:
		return 0;
	}
	pOut->nBits = 8; /* always one byte */
	return 1;
}
/*
 * ---------------------------------------------------------------------------
 * The detection ladder.
 * ---------------------------------------------------------------------------
 * php reads THREE bytes, then four, then twelve, testing after each widening
 * -- and the position the ladder stops at is part of the contract, because
 * every reader below seeks RELATIVE to it. A ladder that read a fixed twelve
 * bytes up front would answer the same type and then read the wrong offsets.
 *
 * Two of the tests are shorter than the signature they name: PSD is matched on
 * three of its four bytes and BMP on two, so "8BPxx" is a PSD and "BM" alone
 * is a BMP. PNG is the one signature checked TWICE -- three bytes to enter the
 * branch and eight to confirm it -- and a file that entered and failed is not
 * "some other type" but php's own E_WARNING about an ASCII-mangled PNG.
 *
 * The tail is ordered by cost: the two formats with no signature at all
 * (WBMP's shape, XBM's C source) are tried only once everything else has
 * failed, and the "Error reading from" notice for a file shorter than twelve
 * bytes is raised BETWEEN them -- so a nine-byte WBMP is a size while a
 * nine-byte anything-else is a diagnostic.
 */
/* php's `Error reading from %s!`, whose %s is the ARGUMENT the caller wrote --
 * the file name for getimagesize() and the DATA itself for the string form,
 * both as a C string and so both stopping at the first NUL. */
static void ImgThrowShortRead(ph7_context *pCtx,const char *zInput,int nInput)
{
	int n = 0;
	while( n < nInput && zInput[n] != 0 ){
		n++;
	}
	ImgThrowFmt(pCtx,PH7_CTX_NOTICE,"%s(): Error reading from %.*s!",
		ph7_function_name(pCtx),n,zInput ? zInput : "");
}
static int ImgDetectType(ph7_context *pCtx,ImgReader *p,const char *zInput,int nInput)
{
	unsigned char zSig[12];
	int bTwelve;
	if( ImgRead(p,zSig,3) != 3 ){
		ImgThrowShortRead(pCtx,zInput,nInput);
		return PH7_IMG_UNKNOWN;
	}
	/* BYTES READ: 3 */
	if( SyMemcmp(zSig,"GIF",3) == 0 ){
		return PH7_IMG_GIF;
	}else if( zSig[0] == 0xFF && zSig[1] == 0xD8 && zSig[2] == 0xFF ){
		return PH7_IMG_JPEG;
	}else if( zSig[0] == 0x89 && zSig[1] == 'P' && zSig[2] == 'N' ){
		if( ImgRead(p,&zSig[3],5) != 5 ){
			ImgThrowShortRead(pCtx,zInput,nInput);
			return PH7_IMG_UNKNOWN;
		}
		if( SyMemcmp(zSig,"\211PNG\r\n\032\n",8) == 0 ){
			return PH7_IMG_PNG;
		}
		ImgThrowFmt(pCtx,PH7_CTX_WARNING,"%s(): PNG file corrupted by ASCII conversion",
			ph7_function_name(pCtx));
		return PH7_IMG_UNKNOWN;
	}else if( SyMemcmp(zSig,"FWS",3) == 0 ){
		return PH7_IMG_SWF;
	}else if( SyMemcmp(zSig,"CWS",3) == 0 ){
		return PH7_IMG_SWC;
	}else if( SyMemcmp(zSig,"8BP",3) == 0 ){
		return PH7_IMG_PSD;
	}else if( SyMemcmp(zSig,"BM",2) == 0 ){
		return PH7_IMG_BMP;
	}else if( zSig[0] == 0xFF && zSig[1] == 0x4F && zSig[2] == 0xFF ){
		return PH7_IMG_JPC;
	}else if( SyMemcmp(zSig,"RIF",3) == 0 ){
		if( ImgRead(p,&zSig[3],9) != 9 ){
			ImgThrowShortRead(pCtx,zInput,nInput);
			return PH7_IMG_UNKNOWN;
		}
		if( SyMemcmp(&zSig[8],"WEBP",4) == 0 ){
			return PH7_IMG_WEBP;
		}
		return PH7_IMG_UNKNOWN;
	}
	if( ImgRead(p,&zSig[3],1) != 1 ){
		ImgThrowShortRead(pCtx,zInput,nInput);
		return PH7_IMG_UNKNOWN;
	}
	/* BYTES READ: 4 */
	if( SyMemcmp(zSig,"II\052\000",4) == 0 ){
		return PH7_IMG_TIFF_II;
	}else if( SyMemcmp(zSig,"MM\000\052",4) == 0 ){
		return PH7_IMG_TIFF_MM;
	}else if( SyMemcmp(zSig,"FORM",4) == 0 ){
		return PH7_IMG_IFF;
	}else if( SyMemcmp(zSig,"\000\000\001\000",4) == 0 ){
		return PH7_IMG_ICO;
	}
	/* WBMP may be shorter than twelve bytes, so the diagnostic waits. */
	bTwelve = (ImgRead(p,&zSig[4],8) == 8);
	/* BYTES READ: 12 */
	if( bTwelve && SyMemcmp(zSig,"\000\000\000\014jP  \r\n\207\n",12) == 0 ){
		return PH7_IMG_JP2;
	}
	if( ImgGetWbmp(p,0) ){
		return PH7_IMG_WBMP;
	}
	if( !bTwelve ){
		ImgThrowShortRead(pCtx,zInput,nInput);
		return PH7_IMG_UNKNOWN;
	}
	if( ImgGetXbm(pCtx,p,0) ){
		return PH7_IMG_XBM;
	}
	return PH7_IMG_UNKNOWN;
}
/*
 * ---------------------------------------------------------------------------
 * The answer.
 * ---------------------------------------------------------------------------
 * php's array is index 0/1/2, an optional index 3, the two optional counts,
 * the mime string and php 8.5's two unit names. Two rules are worth stating:
 *
 *   - index 3 is a CONVENIENCE string for an <img> tag, so it exists only
 *     while both units are pixels. A reader that answers in centimetres
 *     leaves the array one entry shorter.
 *   - the size is stored as an unsigned 32-bit reading but PRINTED into index
 *     3 with a signed conversion, so a width past 2^31 is a positive integer
 *     at index 0 and a NEGATIVE number inside the tag php builds from it.
 *     Both are php's, and reproducing one without the other would be worse
 *     than reproducing neither.
 */
static void ImgBuildAnswer(ph7_context *pCtx,int iType,ImgInfo *pInfo)
{
	ph7_value *pArray,*pVal;
	char zTag[128];
	int nTag;
	pArray = ph7_context_new_array(pCtx);
	pVal   = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pVal == 0 ){
		ph7_result_bool(pCtx,0);
		return;
	}
	ph7_value_int64(pVal,(ph7_int64)pInfo->nWidth);
	ph7_array_add_intkey_elem(pArray,0,pVal);
	ph7_value_int64(pVal,(ph7_int64)pInfo->nHeight);
	ph7_array_add_intkey_elem(pArray,1,pVal);
	ph7_value_int(pVal,iType);
	ph7_array_add_intkey_elem(pArray,2,pVal);
	if( (pInfo->zWidthUnit == 0 || SyStrncmp(pInfo->zWidthUnit,"px",3) == 0)
	 && (pInfo->zHeightUnit == 0 || SyStrncmp(pInfo->zHeightUnit,"px",3) == 0) ){
		nTag = (int)SyBufferFormat(zTag,sizeof(zTag),"width=\"%d\" height=\"%d\"",
			(int)pInfo->nWidth,(int)pInfo->nHeight);
		ph7_value_reset_string_cursor(pVal);
		ph7_value_string(pVal,zTag,nTag);
		ph7_array_add_intkey_elem(pArray,3,pVal);
	}
	if( pInfo->nBits != 0 ){
		ph7_value_int64(pVal,(ph7_int64)pInfo->nBits);
		ph7_array_add_strkey_elem(pArray,"bits",pVal);
	}
	if( pInfo->nChannels != 0 ){
		ph7_value_int64(pVal,(ph7_int64)pInfo->nChannels);
		ph7_array_add_strkey_elem(pArray,"channels",pVal);
	}
	ph7_value_reset_string_cursor(pVal);
	ph7_value_string(pVal,PH7_ImageTypeMime(iType),-1);
	ph7_array_add_strkey_elem(pArray,"mime",pVal);
	ph7_value_reset_string_cursor(pVal);
	ph7_value_string(pVal,pInfo->zWidthUnit ? pInfo->zWidthUnit : "px",-1);
	ph7_array_add_strkey_elem(pArray,"width_unit",pVal);
	ph7_value_reset_string_cursor(pVal);
	ph7_value_string(pVal,pInfo->zHeightUnit ? pInfo->zHeightUnit : "px",-1);
	ph7_array_add_strkey_elem(pArray,"height_unit",pVal);
	ph7_context_release_value(pCtx,pVal);
	ph7_result_value(pCtx,pArray);
}
/*
 * Detect, then dispatch. The reader a type names is the ONLY thing that runs;
 * an unrecognized file answers false with no further reading, which is why the
 * ladder's own diagnostics are the only ones a script sees for one.
 */
static int ImgReadAny(ph7_context *pCtx,ImgReader *p,const char *zInput,int nInput,ph7_value *pInfo)
{
	ImgInfo sInfo;
	int iType,bHave = 0;
	SyZero(&sInfo,sizeof(sInfo));
	iType = ImgDetectType(pCtx,p,zInput,nInput);
	switch( iType ){
	case PH7_IMG_GIF:     bHave = ImgHandleGif(p,&sInfo);           break;
	case PH7_IMG_JPEG:    bHave = ImgHandleJpeg(pCtx,p,&sInfo,pInfo); break;
	case PH7_IMG_PNG:     bHave = ImgHandlePng(p,&sInfo);           break;
	case PH7_IMG_SWF:     bHave = ImgHandleSwf(p,&sInfo);           break;
	case PH7_IMG_SWC:
		/* php reads a compressed SWF through zlib and says so when the build it
		 * runs on has none. This engine links no zlib (a §10 scope cut, so the
		 * `compress.zlib` stream filter is absent for the same reason), which
		 * makes php's own no-zlib sentence the honest answer rather than a
		 * stub: the TYPE is still IMAGETYPE_SWC and the size is still refused. */
		ImgThrowFmt(pCtx,PH7_CTX_NOTICE,
			"%s(): The image is a compressed SWF file, but you do not have a static version of the zlib extension enabled",
			ph7_function_name(pCtx));
		break;
	case PH7_IMG_PSD:     bHave = ImgHandlePsd(p,&sInfo);           break;
	case PH7_IMG_BMP:     bHave = ImgHandleBmp(p,&sInfo);           break;
	case PH7_IMG_TIFF_II: bHave = ImgHandleTiff(pCtx,p,&sInfo,0);   break;
	case PH7_IMG_TIFF_MM: bHave = ImgHandleTiff(pCtx,p,&sInfo,1);   break;
	case PH7_IMG_JPC:     bHave = ImgHandleJpc(pCtx,p,&sInfo);      break;
	case PH7_IMG_JP2:     bHave = ImgHandleJp2(pCtx,p,&sInfo);      break;
	case PH7_IMG_IFF:     bHave = ImgHandleIff(p,&sInfo);           break;
	case PH7_IMG_WBMP:    bHave = ImgGetWbmp(p,&sInfo);             break;
	case PH7_IMG_XBM:     bHave = ImgGetXbm(pCtx,p,&sInfo);         break;
	case PH7_IMG_ICO:     bHave = ImgHandleIco(p,&sInfo);           break;
	case PH7_IMG_WEBP:    bHave = ImgHandleWebp(p,&sInfo);          break;
	default:
		break;
	}
	if( !bHave ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ImgBuildAnswer(pCtx,iType,&sInfo);
	return PH7_OK;
}
/*
 * array|false getimagesize(string $filename,&$image_info = null)
 * array|false getimagesizefromstring(string $string,&$image_info = null)
 *
 * The out-parameter is created EMPTY before anything is opened, so a caller
 * that names it reads back an array even when the whole call fails -- and the
 * NUL-byte refusal on $filename is the path rule (VmBuiltinPathMask), which
 * the string form does not carry: its argument is data, and php's own
 * diagnostic prints it as a C string, stopping at the first NUL.
 */
static int ImgSizeCommon(ph7_context *pCtx,int nArg,ph7_value **apArg,int bFromString)
{
	ImgReader sReader;
	ph7_value *pInfo = 0;
	const char *zOrig,*zName;
	int nOrig,rc = PH7_OK;
	SyZero(&sReader,sizeof(sReader));
	sReader.pCtx = pCtx;
	zOrig = ph7_value_to_string(apArg[0],&nOrig);
	/* php initialises the out-parameter BEFORE it opens anything, so a caller
	 * that names one reads back an empty array even when the call fails. */
	if( nArg > 1 ){
		pInfo = ph7_context_new_array(pCtx);
		if( pInfo == 0 ){
			return PH7_OK;
		}
	}
	if( bFromString ){
		sReader.zData = (const unsigned char *)zOrig;
		sReader.nData = (ph7_int64)nOrig;
		rc = ImgReadAny(pCtx,&sReader,zOrig,nOrig,pInfo);
	}else{
		const ph7_io_stream *pStream;
		void *pHandle;
		zName = zOrig;
		pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zName,nOrig);
		if( pStream == 0 ){
			VfsThrowNoDeviceWarning(pCtx,zName,FALSE);
			ph7_result_bool(pCtx,0);
		}else{
			pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zName,PH7_IO_OPEN_RDONLY,
				FALSE,0,FALSE,0,ph7_function_name(pCtx));
			if( pHandle == 0 ){
				VfsThrowOpenWarning(pCtx,zName);
				ph7_result_bool(pCtx,0);
			}else{
				sReader.pStream = pStream;
				sReader.pHandle = pHandle;
				rc = ImgReadAny(pCtx,&sReader,zOrig,nOrig,pInfo);
				PH7_StreamCloseHandle(pStream,pHandle);
			}
		}
	}
	if( pInfo ){
		PH7_VmStoreArgByRef(pCtx->pVm,apArg[1],pInfo);
		ph7_context_release_value(pCtx,pInfo);
	}
	return rc;
}
PH7_PRIVATE int PH7_builtin_getimagesize(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return ImgSizeCommon(pCtx,nArg,apArg,FALSE);
}
PH7_PRIVATE int PH7_builtin_getimagesizefromstring(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return ImgSizeCommon(pCtx,nArg,apArg,TRUE);
}
#endif /* PH7_DISABLE_BUILTIN_FUNC */
