/**
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
#if defined(PH7_ENABLE_ZLIB) && !defined(PH7_DISABLE_BUILTIN_FUNC)
#include <zlib.h>
/*
 * Section:
 *    php's zlib extension: DEFLATE, the two framings around it, and the stream
 *    that reads a .gz file as though it were text.
 * Status:
 *    Stable.
 *
 * ext/zlib is the `php shells a C library` shape in its purest form: php's own
 * code here is argument screening, buffer growth and a stream wrapper, and
 * every BYTE it answers with comes out of libz. So the contract reproduced
 * below is libz's, and this unit links the library rather than deriving it --
 * the opposite call from ext/gettext or ext/bcmath, and for the reason those
 * two record: a derivation is right when the answer is a RULE (which catalog a
 * lookup reads, what 1/3 is to 20 digits) and wrong when the answer is a byte
 * stream a specific implementation produces. Nobody can re-derive zlib's
 * literal/length decisions, and a program that compresses under php and
 * decompresses under PHL needs exactly them.
 *
 * What that decision costs is worth stating: the compressed bytes are the
 * bytes of WHICHEVER libz this build was linked against. php has the same
 * property (its own answers move with the system zlib), so the two agree on
 * any box where they link the same library and may differ in the compressed
 * form -- never in the decompressed one -- where they do not. Nothing in the
 * corpus pins a compressed byte for that reason; the tests pin round trips,
 * framing headers and the diagnostics.
 *
 * THE THREE FRAMINGS. One deflate stream, three envelopes, and php exposes the
 * choice as an `$encoding`:
 *
 *   ZLIB_ENCODING_RAW     (-15)  no header, no checksum   gzdeflate/gzinflate
 *   ZLIB_ENCODING_DEFLATE  (15)  RFC 1950 zlib + adler32  gzcompress/gzuncompress
 *   ZLIB_ENCODING_GZIP     (31)  RFC 1952 gzip + crc32    gzencode/gzdecode
 *
 * The numbers are libz's own windowBits spelling (negative = raw, +16 = gzip),
 * which is why they are what they are. Each decoder is STRICT about its own
 * framing -- gzdecode() refuses a zlib stream, gzinflate() refuses a gzip one
 * -- and only zlib_decode() sniffs, because it asks libz for the automatic
 * mode (+32) and retries raw when that fails.
 *
 * THE BUFFER LOOP. php grows its output buffer by an eighth per round and
 * stops at $max_length, and that loop is OBSERVABLE: `gzuncompress($c, 22)` on
 * a 23-byte payload is `insufficient memory` and false, while the same call on
 * an 80000-byte payload with $max_length 79999 answers all 80000 bytes -- the
 * limit stops the buffer from GROWING again, it does not truncate. The loop
 * below is php's, round for round, so both answers come out here too.
 *
 * THE STREAM. gzopen() and compress.zlib:// are one device (`ZLIB` in php's
 * metadata), and its reader is libz's `gzread` rather than a plain inflate:
 *   - a file that does not start with the gzip magic is passed through BYTE
 *     FOR BYTE (php's `gzopen` on a plain text file reads the text), so the
 *     device answers a zlib-framed or raw-deflate file with its compressed
 *     bytes -- an asymmetry with gzdecode() that is libz's, and real;
 *   - a CONCATENATION of gzip members reads as one stream, which is what makes
 *     `gzopen($p,'a')` twice and then `gzfile($p)` answer both writes;
 *   - trailing bytes after the last member are ignored, and a truncated member
 *     ends the stream instead of failing it.
 * The two doors differ in one visible detail: gzopen() opens the device with no
 * wrapper (php reports no `wrapper_type` and no `uri` for it), compress.zlib://
 * goes through one.
 *
 * WHAT IS NOT HERE. php's `zlib.output_compression` compresses a WEB response
 * from the ini directive; the three directives are registered so `ini_get()`
 * answers them, and `ob_gzhandler()` is the handler a script installs by hand
 * -- both read the request's Accept-Encoding, so both answer false in a command
 * line exactly as php's do. bzip2 is a separate extension in php and is not in
 * this build, which is a build fact rather than a gap: `Phar::getSupportedCompression()`
 * and friends report what is actually here.
 */
/* php's three $encoding values, spelled as libz windowBits. */
#define PHL_Z_RAW      (-15)
#define PHL_Z_DEFLATE  (15)
#define PHL_Z_GZIP     (31)
/* zlib_decode()'s "work it out": libz's automatic zlib/gzip detection. */
#define PHL_Z_ANY      (15+32)
/* php's own memLevel for every one-shot encode. */
#define PHL_Z_MEMLEVEL 8
/* How much compressed input the stream device buffers per refill. */
#define PHL_Z_CHUNK    8192

/* ------------------------------------------------------------------ */
/* Argument screens                                                   */
/* ------------------------------------------------------------------ */
/*
 * php names the three encodings in one sentence wherever it refuses one, and
 * the sentence lists them in this order whatever the function is.
 */
static int ZlibScreenEncoding(ph7_context *pCtx,sxi64 iEnc,int iArg,const char *zParam)
{
	if( iEnc == PHL_Z_RAW || iEnc == PHL_Z_DEFLATE || iEnc == PHL_Z_GZIP ){
		return 0;
	}
	PH7_VmThrowException(pCtx,"ValueError",
		"%s(): Argument #%d ($%s) must be one of ZLIB_ENCODING_RAW, "
		"ZLIB_ENCODING_GZIP, or ZLIB_ENCODING_DEFLATE",
		ph7_function_name(pCtx),iArg,zParam);
	return -1;
}
static int ZlibScreenLevel(ph7_context *pCtx,sxi64 iLevel,int iArg,const char *zParam)
{
	if( iLevel >= -1 && iLevel <= 9 ){
		return 0;
	}
	PH7_VmThrowException(pCtx,"ValueError",
		"%s(): Argument #%d ($%s) must be between -1 and 9",
		ph7_function_name(pCtx),iArg,zParam);
	return -1;
}
/* ------------------------------------------------------------------ */
/* One-shot encode / decode                                           */
/* ------------------------------------------------------------------ */
/*
 * php's encoder: deflateBound() sizes the output in ONE go and a single
 * Z_FINISH fills it, so the answer is whatever libz emits for (level,
 * encoding, memLevel 8, default strategy) -- including the gzip header's XFL
 * byte, which libz sets from the level (0x04 fastest, 0x02 best, 0x00
 * otherwise) and its OS byte, which is the one libz was compiled for.
 */
static int ZlibEncodeBuf(ph7_vm *pVm,const unsigned char *zIn,sxu32 nIn,
	int iEnc,int iLevel,SyBlob *pOut,int *piStatus)
{
	z_stream z;
	unsigned char *zBuf;
	uLong nBound;
	int rc;
	SyZero(&z,sizeof(z));
	rc = deflateInit2(&z,iLevel,Z_DEFLATED,iEnc,PHL_Z_MEMLEVEL,Z_DEFAULT_STRATEGY);
	if( rc != Z_OK ){
		*piStatus = rc;
		return -1;
	}
	z.next_in = (Bytef *)zIn;
	z.avail_in = (uInt)nIn;
	nBound = deflateBound(&z,(uLong)nIn);
	/* deflateBound() does not account for a gzip header carrying no name or
	 * comment; php pads the same way libz's own examples do. */
	nBound += 32;
	zBuf = (unsigned char *)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)nBound);
	if( zBuf == 0 ){
		deflateEnd(&z);
		*piStatus = Z_MEM_ERROR;
		return -1;
	}
	z.next_out = (Bytef *)zBuf;
	z.avail_out = (uInt)nBound;
	rc = deflate(&z,Z_FINISH);
	if( rc != Z_STREAM_END ){
		SyMemBackendFree(&pVm->sAllocator,zBuf);
		deflateEnd(&z);
		*piStatus = rc == Z_OK ? Z_BUF_ERROR : rc;
		return -1;
	}
	SyBlobAppend(pOut,zBuf,(sxu32)(nBound - z.avail_out));
	SyMemBackendFree(&pVm->sAllocator,zBuf);
	deflateEnd(&z);
	*piStatus = Z_OK;
	return 0;
}
/*
 * php's decoder, round for round (`php_zlib_inflate_rounds`). The buffer
 * starts at the input length (or at $max_length when that is smaller), grows
 * by an eighth plus one each round, and the limit is a stop on the GROWTH
 * rather than a truncation -- which is why a payload that finishes inside the
 * round where the limit is reached comes back whole and one that does not is
 * `insufficient memory`. 100 rounds is php's own runaway guard.
 */
static int ZlibDecodeBuf(SyMemBackend *pAlloc,const unsigned char *zIn,sxu32 nIn,
	int iEnc,sxu32 nMax,SyBlob *pOut,int *piStatus)
{
	z_stream z;
	unsigned char *zBuf = 0,*zNew;
	sxu32 nSize,nUsed = 0;
	int rc,iRound = 0;
	SyZero(&z,sizeof(z));
	rc = inflateInit2(&z,iEnc);
	if( rc != Z_OK ){
		*piStatus = rc;
		return -1;
	}
	z.next_in = (Bytef *)zIn;
	z.avail_in = (uInt)nIn;
	/* php's first buffer is four times the input (a decent guess at the
	 * expansion ratio) and QUADRUPLES each round; $max_length caps only that
	 * first one. The pair of answers this produces is the observable part:
	 * a 23-byte payload with $max_length 22 fills the capped buffer, comes
	 * back for a second round and dies on the `max <= used` test above with
	 * php's `insufficient memory`, while an 80000-byte one with $max_length
	 * 79999 never reaches a round where used has caught up with the limit and
	 * answers all 80000 bytes. */
	nSize = nIn > 0 ? nIn * 4 : 1;
	if( nMax && nMax < nSize ){
		nSize = nMax;
	}
	for(;;){
		if( nMax && nMax <= nUsed ){
			rc = Z_MEM_ERROR;
			break;
		}
		zNew = (unsigned char *)(zBuf
			? SyMemBackendRealloc(pAlloc,zBuf,nSize + 1)
			: SyMemBackendAlloc(pAlloc,nSize + 1));
		if( zNew == 0 ){
			rc = Z_MEM_ERROR;
			break;
		}
		zBuf = zNew;
		z.next_out = (Bytef *)zBuf + nUsed;
		z.avail_out = (uInt)(nSize - nUsed);
		rc = inflate(&z,Z_NO_FLUSH);
		nUsed = nSize - z.avail_out;
		nSize = nSize < 0x20000000 ? nSize * 4 : SXU32_HIGH;
		if( rc != Z_BUF_ERROR && !(rc == Z_OK && z.avail_in > 0) ){
			break;
		}
		if( ++iRound >= 100 ){
			break;
		}
	}
	inflateEnd(&z);
	if( rc == Z_STREAM_END ){
		SyBlobAppend(pOut,zBuf,nUsed);
		SyMemBackendFree(pAlloc,zBuf);
		*piStatus = Z_OK;
		return 0;
	}
	if( zBuf ){
		SyMemBackendFree(pAlloc,zBuf);
	}
	/* Anything short of a finished stream is php's `data error`, whatever libz
	 * called it -- a truncated member ends the loop with Z_OK and reads as one
	 * under php too. The one status that keeps its own name is the limit. */
	*piStatus = rc == Z_MEM_ERROR ? Z_MEM_ERROR : Z_DATA_ERROR;
	return -1;
}
/*
 * The body behind gzcompress/gzdeflate/gzencode/zlib_encode. They differ in
 * one thing only: which encoding the third argument DEFAULTS to (zlib_encode
 * has no default -- its encoding is argument #2 and required).
 */
static int ZlibEncodeCommon(ph7_context *pCtx,int nArg,ph7_value **apArg,
	int iDefEnc,int bEncFirst)
{
	const char *zIn;
	int nIn = 0,iStatus = 0;
	sxi64 iLevel = -1,iEnc = iDefEnc;
	SyBlob sOut;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zIn = ph7_value_to_string(apArg[0],&nIn);
	if( bEncFirst ){
		/* zlib_encode(string $data, int $encoding, int $level = -1) */
		if( nArg > 1 ){
			iEnc = ph7_value_to_int64(apArg[1]);
		}
		if( ZlibScreenEncoding(pCtx,iEnc,2,"encoding") != 0 ){
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		if( nArg > 2 ){
			iLevel = ph7_value_to_int64(apArg[2]);
		}
		if( ZlibScreenLevel(pCtx,iLevel,3,"level") != 0 ){
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
	}else{
		/* gz*(string $data, int $level = -1, int $encoding = <default>) */
		if( nArg > 1 ){
			iLevel = ph7_value_to_int64(apArg[1]);
		}
		if( ZlibScreenLevel(pCtx,iLevel,2,"level") != 0 ){
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		if( nArg > 2 ){
			iEnc = ph7_value_to_int64(apArg[2]);
		}
		if( ZlibScreenEncoding(pCtx,iEnc,3,"encoding") != 0 ){
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
	}
	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);
	if( ZlibEncodeBuf(pCtx->pVm,(const unsigned char *)zIn,(sxu32)nIn,
			(int)iEnc,(int)iLevel,&sOut,&iStatus) != 0 ){
		SyBlobRelease(&sOut);
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"%s",zError(iStatus));
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));
	SyBlobRelease(&sOut);
	return PH7_OK;
}
/*
 * The body behind gzuncompress/gzinflate/gzdecode/zlib_decode. iEnc 0 is
 * zlib_decode()'s "any": libz's automatic mode first (which knows zlib and
 * gzip), then a raw retry, which is how `zlib_decode(gzdeflate($s))` works.
 */
static int ZlibDecodeCommon(ph7_context *pCtx,int nArg,ph7_value **apArg,int iEnc)
{
	const char *zIn;
	int nIn = 0,iStatus = 0,rc;
	sxi64 iMax = 0;
	SyBlob sOut;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zIn = ph7_value_to_string(apArg[0],&nIn);
	if( nArg > 1 ){
		iMax = ph7_value_to_int64(apArg[1]);
		if( iMax < 0 ){
			ph7_result_bool(pCtx,0);
			return PH7_VmThrowException(pCtx,"ValueError",
				"%s(): Argument #2 ($max_length) must be greater than or equal to 0",
				ph7_function_name(pCtx));
		}
	}
	if( iMax > SXU32_HIGH ){
		iMax = SXU32_HIGH;
	}
	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);
	rc = ZlibDecodeBuf(&pCtx->pVm->sAllocator,(const unsigned char *)zIn,(sxu32)nIn,
		iEnc ? iEnc : PHL_Z_ANY,(sxu32)iMax,&sOut,&iStatus);
	if( rc != 0 && iEnc == 0 && iStatus != Z_MEM_ERROR ){
		/* php's second guess for the sniffing door: a headerless stream. */
		SyBlobRelease(&sOut);
		SyBlobInit(&sOut,&pCtx->pVm->sAllocator);
		rc = ZlibDecodeBuf(&pCtx->pVm->sAllocator,(const unsigned char *)zIn,(sxu32)nIn,
			PHL_Z_RAW,(sxu32)iMax,&sOut,&iStatus);
	}
	if( rc != 0 ){
		SyBlobRelease(&sOut);
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"%s",zError(iStatus));
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));
	SyBlobRelease(&sOut);
	return PH7_OK;
}
/* string|false gzcompress(string $data, int $level = -1, int $encoding = ZLIB_ENCODING_DEFLATE) */
static int PH7_builtin_gzcompress(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return ZlibEncodeCommon(pCtx,nArg,apArg,PHL_Z_DEFLATE,0);
}
/* string|false gzdeflate(string $data, int $level = -1, int $encoding = ZLIB_ENCODING_RAW) */
static int PH7_builtin_gzdeflate(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return ZlibEncodeCommon(pCtx,nArg,apArg,PHL_Z_RAW,0);
}
/* string|false gzencode(string $data, int $level = -1, int $encoding = ZLIB_ENCODING_GZIP) */
static int PH7_builtin_gzencode(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return ZlibEncodeCommon(pCtx,nArg,apArg,PHL_Z_GZIP,0);
}
/* string|false zlib_encode(string $data, int $encoding, int $level = -1) */
static int PH7_builtin_zlib_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return ZlibEncodeCommon(pCtx,nArg,apArg,PHL_Z_RAW,1);
}
/* string|false gzuncompress(string $data, int $max_length = 0) */
static int PH7_builtin_gzuncompress(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return ZlibDecodeCommon(pCtx,nArg,apArg,PHL_Z_DEFLATE);
}
/* string|false gzinflate(string $data, int $max_length = 0) */
static int PH7_builtin_gzinflate(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return ZlibDecodeCommon(pCtx,nArg,apArg,PHL_Z_RAW);
}
/* string|false gzdecode(string $data, int $max_length = 0) */
static int PH7_builtin_gzdecode(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return ZlibDecodeCommon(pCtx,nArg,apArg,PHL_Z_GZIP);
}
/* string|false zlib_decode(string $data, int $max_length = 0) */
static int PH7_builtin_zlib_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return ZlibDecodeCommon(pCtx,nArg,apArg,0);
}
/* ------------------------------------------------------------------ */
/* deflate_init / inflate_init and their contexts                     */
/* ------------------------------------------------------------------ */
/*
 * A DeflateContext or an InflateContext: php's two opaque handle classes,
 * final, uncloneable, unserializable, with no method and no property, whose
 * `new` is refused by name. The z_stream lives OUTSIDE the engine's allocator
 * (libz mallocs its own window), so the record is chained on a per-VM registry
 * and freed there as well as from the instance's release hook -- the ext/curl
 * rule, for the same reason.
 */
typedef struct phl_zctx phl_zctx;
struct phl_zctx {
	z_stream z;                    /* libz's own state */
	ph7_vm *pVm;
	ph7_class_instance *pOwner;    /* the object holding it, or 0 once released */
	int bInflate;                  /* 0 deflate, 1 inflate */
	int bInit;                     /* z is live */
	int iEnc;                      /* the $encoding it was created with */
	int iStatus;                   /* inflate_get_status() */
	sxu32 nReadLen;                /* inflate_get_read_len() */
	SyBlob sDict;                  /* the `dictionary` option's bytes, kept for the
	                                * Z_NEED_DICT a zlib-framed stream answers with */
	phl_zctx *pNext;               /* VM registry chain */
};
#define ZCTX_SLOT "__res"

static void ZctxFree(phl_zctx *pCtx)
{
	SyBlobRelease(&pCtx->sDict);
	if( pCtx->bInit ){
		if( pCtx->bInflate ){
			inflateEnd(&pCtx->z);
		}else{
			deflateEnd(&pCtx->z);
		}
		pCtx->bInit = 0;
	}
}
/* Free every context this VM still holds. Runs on VM reset (a reused VM must
 * not see the previous run's state) and again at release, before the allocator
 * holding the shells goes. */
static void ZctxVmSweep(ph7_vm *pVm)
{
	phl_zctx *p = (phl_zctx *)pVm->pZlibCtx;
	while( p ){
		phl_zctx *pNext = p->pNext;
		ZctxFree(p);
		SyMemBackendFree(&pVm->sAllocator,p);
		p = pNext;
	}
	pVm->pZlibCtx = 0;
}
PH7_PRIVATE void PH7_ZlibVmReset(ph7_vm *pVm)
{
	ZctxVmSweep(&(*pVm));
}
PH7_PRIVATE void PH7_ZlibVmRelease(ph7_vm *pVm)
{
	ZctxVmSweep(&(*pVm));
}
static phl_zctx * ZctxNew(ph7_vm *pVm,int bInflate)
{
	phl_zctx *p = (phl_zctx *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_zctx));
	if( p == 0 ){
		return 0;
	}
	SyZero(p,sizeof(*p));
	p->pVm = pVm;
	p->bInflate = bInflate;
	SyBlobInit(&p->sDict,&pVm->sAllocator);
	p->pNext = (phl_zctx *)pVm->pZlibCtx;
	pVm->pZlibCtx = p;
	return p;
}
/* The context an instance holds, or 0. */
static phl_zctx * ZctxOfInstance(ph7_class_instance *pThis)
{
	SyString sAttr;
	ph7_value *pRes;
	if( pThis == 0 ){
		return 0;
	}
	SyStringInitFromBuf(&sAttr,ZCTX_SLOT,sizeof(ZCTX_SLOT)-1);
	pRes = PH7_ClassInstanceFetchAttr(pThis,&sAttr);
	if( pRes == 0 || (pRes->iFlags & MEMOBJ_RES) == 0 ){
		return 0;
	}
	return (phl_zctx *)pRes->x.pOther;
}
static int ZctxAttach(ph7_class_instance *pThis,phl_zctx *pCtx)
{
	SyString sAttr;
	ph7_value *pRes;
	SyStringInitFromBuf(&sAttr,ZCTX_SLOT,sizeof(ZCTX_SLOT)-1);
	pRes = pThis ? PH7_ClassInstanceFetchAttr(pThis,&sAttr) : 0;
	if( pRes == 0 ){
		return -1;
	}
	PH7_MemObjRelease(pRes);
	pRes->x.pOther = pCtx;
	MemObjSetType(pRes,MEMOBJ_RES);
	pCtx->pOwner = pThis;
	return 0;
}
/* The instance is going: end the libz stream while the slot is still readable.
 * The shell stays on the registry, which frees it. */
static void ZctxInstanceRelease(ph7_vm *pVm,ph7_class_instance *pThis)
{
	phl_zctx *p = ZctxOfInstance(pThis);
	SXUNUSED(pVm);
	if( p == 0 || p->pOwner != pThis ){
		return;
	}
	ZctxFree(p);
	p->pOwner = 0;
}
/*
 * deflate_init()'s $options. php reads five keys and IGNORES every other one,
 * screens four of them itself with a sentence naming the KEY rather than the
 * argument, and hands `window` to a shared helper whose sentence names neither.
 */
struct ZlibOpt {
	int iLevel;
	int iMemory;
	int iWindow;
	int iStrategy;
	const char *zDict;   /* the concatenated dictionary, or 0 */
	sxu32 nDict;
};
static int ZlibOptBadValue(ph7_context *pCtx,const char *zKey,const char *zWhat)
{
	PH7_VmThrowException(pCtx,"ValueError","%s(): \"%s\" option must %s",
		ph7_function_name(pCtx),zKey,zWhat);
	return -1;
}
/*
 * php's dictionary option takes a STRING (used whole) or an ARRAY of strings
 * (concatenated with a NUL after each), and refuses an empty member or one
 * carrying a NUL -- with a sentence about Argument #2, not about the key.
 */
static int ZlibOptDictionary(ph7_context *pCtx,ph7_value *pVal,SyBlob *pDict,int *pbSet)
{
	*pbSet = 0;
	if( ph7_value_is_array(pVal) ){
		ph7_hashmap *pMap = (ph7_hashmap *)pVal->x.pOther;
		ph7_hashmap_node *pEntry;
		pMap->pCur = pMap->pFirst;
		while( (pEntry = PH7_HashmapGetNextEntry(pMap)) != 0 ){
			ph7_value sVal;
			const char *zStr;
			int nStr = 0;
			PH7_MemObjInit(pCtx->pVm,&sVal);
			PH7_HashmapExtractNodeValue(pEntry,&sVal,FALSE);
			zStr = ph7_value_to_string(&sVal,&nStr);
			if( nStr < 1 ){
				PH7_MemObjRelease(&sVal);
				PH7_VmThrowException(pCtx,"ValueError",
					"%s(): Argument #2 ($options) must not contain empty strings",
					ph7_function_name(pCtx));
				return -1;
			}
			if( SyByteFind(zStr,(sxu32)nStr,0,0) == SXRET_OK ){
				PH7_MemObjRelease(&sVal);
				PH7_VmThrowException(pCtx,"ValueError",
					"%s(): Argument #2 ($options) must not contain strings with null bytes",
					ph7_function_name(pCtx));
				return -1;
			}
			SyBlobAppend(pDict,zStr,(sxu32)nStr);
			SyBlobAppend(pDict,"\0",1);
			PH7_MemObjRelease(&sVal);
		}
		*pbSet = 1;
		return 0;
	}
	{
		const char *zStr;
		int nStr = 0;
		zStr = ph7_value_to_string(pVal,&nStr);
		if( nStr > 0 ){
			SyBlobAppend(pDict,zStr,(sxu32)nStr);
		}
		*pbSet = 1;
	}
	return 0;
}
static int ZlibReadOptions(ph7_context *pCtx,ph7_value *pOpt,struct ZlibOpt *pOut,
	SyBlob *pDict,int bDeflate)
{
	ph7_hashmap *pMap;
	ph7_hashmap_node *pEntry;
	ph7_value sObjOpt;
	int bDict = 0,bObj = 0,rc = 0;
	pOut->iLevel = -1;
	pOut->iMemory = PHL_Z_MEMLEVEL;
	pOut->iWindow = 15;
	pOut->iStrategy = Z_DEFAULT_STRATEGY;
	pOut->zDict = 0;
	pOut->nDict = 0;
	if( pOpt == 0 ){
		return 0;
	}
	if( ph7_value_is_object(pOpt) ){
		/* php's `object|array $options`: an object is read by its PROPERTIES,
		 * and every screen below then answers exactly as it does for the array
		 * with the same keys. */
		PH7_MemObjInit(pCtx->pVm,&sObjOpt);
		if( PH7_MemObjToHashmap(&sObjOpt) != SXRET_OK ){
			PH7_MemObjRelease(&sObjOpt);
			return 0;
		}
		if( PH7_ClassInstanceToHashmap((ph7_class_instance *)pOpt->x.pOther,
				(ph7_hashmap *)sObjOpt.x.pOther) != SXRET_OK ){
			PH7_MemObjRelease(&sObjOpt);
			return 0;
		}
		pOpt = &sObjOpt;
		bObj = 1;
	}
	if( !ph7_value_is_array(pOpt) ){
		if( bObj ){
			PH7_MemObjRelease(&sObjOpt);
		}
		return 0;
	}
	pMap = (ph7_hashmap *)pOpt->x.pOther;
	pMap->pCur = pMap->pFirst;
	while( (pEntry = PH7_HashmapGetNextEntry(pMap)) != 0 ){
		ph7_value sKey,sVal;
		const char *zKey;
		int nKey = 0;
		sxi64 iVal;
		PH7_MemObjInit(pCtx->pVm,&sKey);
		PH7_MemObjInit(pCtx->pVm,&sVal);
		PH7_HashmapExtractNodeKey(pEntry,&sKey);
		PH7_HashmapExtractNodeValue(pEntry,&sVal,FALSE);
		zKey = ph7_value_to_string(&sKey,&nKey);
		/* PEEKED, not converted: ph7_value_to_int64() would retype this copy in
		 * place, and `dictionary` reads the SAME value as a string a few lines
		 * down -- which is how a dictionary silently became the single byte
		 * "0" and the compressed bytes stopped matching php's. */
		iVal = PH7_ValuePeekInt64(&sVal);
		rc = 0;
		if( nKey == 5 && SyMemcmp(zKey,"level",5) == 0 ){
			if( iVal < -1 || iVal > 9 ){
				PH7_MemObjRelease(&sKey); PH7_MemObjRelease(&sVal);
				rc = ZlibOptBadValue(pCtx,"level","be between -1 and 9");
				goto done;
			}
			pOut->iLevel = (int)iVal;
		}else if( nKey == 6 && SyMemcmp(zKey,"memory",6) == 0 ){
			if( iVal < 1 || iVal > 9 ){
				PH7_MemObjRelease(&sKey); PH7_MemObjRelease(&sVal);
				rc = ZlibOptBadValue(pCtx,"memory","be between 1 and 9");
				goto done;
			}
			pOut->iMemory = (int)iVal;
		}else if( nKey == 6 && SyMemcmp(zKey,"window",6) == 0 ){
			if( iVal < 8 || iVal > 15 ){
				PH7_MemObjRelease(&sKey); PH7_MemObjRelease(&sVal);
				if( bDeflate ){
					rc = ZlibOptBadValue(pCtx,"window","be between 8 and 15");
					goto done;
				}
				/* inflate_init() reaches php's shared helper instead, whose
				 * sentence names neither the function nor the argument. */
				PH7_VmThrowException(pCtx,"ValueError",
					"zlib window size (logarithm) (%qd) must be within 8..15",iVal);
				rc = -1;
				goto done;
			}
			pOut->iWindow = (int)iVal;
		}else if( nKey == 8 && SyMemcmp(zKey,"strategy",8) == 0 ){
			if( iVal != Z_FILTERED && iVal != Z_HUFFMAN_ONLY && iVal != Z_RLE
			 && iVal != Z_FIXED && iVal != Z_DEFAULT_STRATEGY ){
				PH7_MemObjRelease(&sKey); PH7_MemObjRelease(&sVal);
				rc = ZlibOptBadValue(pCtx,"strategy",
					"be one of ZLIB_FILTERED, ZLIB_HUFFMAN_ONLY, ZLIB_RLE, "
					"ZLIB_FIXED, or ZLIB_DEFAULT_STRATEGY");
				goto done;
			}
			pOut->iStrategy = (int)iVal;
		}else if( nKey == 10 && SyMemcmp(zKey,"dictionary",10) == 0 ){
			if( ZlibOptDictionary(pCtx,&sVal,pDict,&bDict) != 0 ){
				PH7_MemObjRelease(&sKey); PH7_MemObjRelease(&sVal);
				rc = -1;
				goto done;
			}
		}
		/* Any other key: php reads none of them and complains about none. */
		PH7_MemObjRelease(&sKey);
		PH7_MemObjRelease(&sVal);
	}
	if( bDict ){
		pOut->zDict = (const char *)SyBlobData(pDict);
		pOut->nDict = SyBlobLength(pDict);
	}
done:
	if( bObj ){
		PH7_MemObjRelease(&sObjOpt);
	}
	return rc;
}
/* The windowBits libz wants: the $encoding, retuned by the `window` option. */
static int ZlibWindowBits(int iEnc,int iWindow)
{
	if( iEnc == PHL_Z_RAW ){
		return -iWindow;
	}
	if( iEnc == PHL_Z_GZIP ){
		return iWindow + 16;
	}
	return iWindow;
}
static ph7_class_instance * ZlibNewContextObject(ph7_vm *pVm,int bInflate)
{
	const char *zName = bInflate ? "InflateContext" : "DeflateContext";
	ph7_class *pClass = PH7_VmExtractClass(pVm,zName,
		(sxu32)SyStrlen(zName),FALSE,0);
	return pClass ? PH7_NewClassInstance(pVm,pClass) : 0;
}
/* DeflateContext|false deflate_init(int $encoding, array $options = []) */
static int PH7_builtin_deflate_init(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	struct ZlibOpt sOpt;
	SyBlob sDict;
	phl_zctx *pZ;
	ph7_class_instance *pThis;
	sxi64 iEnc;
	int rc;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	iEnc = ph7_value_to_int64(apArg[0]);
	if( ZlibScreenEncoding(pCtx,iEnc,1,"encoding") != 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	SyBlobInit(&sDict,&pCtx->pVm->sAllocator);
	if( ZlibReadOptions(pCtx,nArg > 1 ? apArg[1] : 0,&sOpt,&sDict,1) != 0 ){
		SyBlobRelease(&sDict);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pZ = ZctxNew(pCtx->pVm,0);
	if( pZ == 0 ){
		SyBlobRelease(&sDict);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pZ->iEnc = (int)iEnc;
	rc = deflateInit2(&pZ->z,sOpt.iLevel,Z_DEFLATED,
		ZlibWindowBits((int)iEnc,sOpt.iWindow),sOpt.iMemory,sOpt.iStrategy);
	if( rc != Z_OK ){
		SyBlobRelease(&sDict);
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"%s",zError(rc));
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pZ->bInit = 1;
	if( sOpt.zDict && sOpt.nDict > 0 ){
		deflateSetDictionary(&pZ->z,(const Bytef *)sOpt.zDict,(uInt)sOpt.nDict);
	}
	SyBlobRelease(&sDict);
	pThis = ZlibNewContextObject(pCtx->pVm,0);
	if( pThis == 0 || ZctxAttach(pThis,pZ) != 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	PH7_NativeResultObject(pCtx,pThis);
	return PH7_OK;
}
/* InflateContext|false inflate_init(int $encoding, array $options = []) */
static int PH7_builtin_inflate_init(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	struct ZlibOpt sOpt;
	SyBlob sDict;
	phl_zctx *pZ;
	ph7_class_instance *pThis;
	sxi64 iEnc;
	int rc;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	iEnc = ph7_value_to_int64(apArg[0]);
	if( ZlibScreenEncoding(pCtx,iEnc,1,"encoding") != 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	SyBlobInit(&sDict,&pCtx->pVm->sAllocator);
	if( ZlibReadOptions(pCtx,nArg > 1 ? apArg[1] : 0,&sOpt,&sDict,0) != 0 ){
		SyBlobRelease(&sDict);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pZ = ZctxNew(pCtx->pVm,1);
	if( pZ == 0 ){
		SyBlobRelease(&sDict);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pZ->iEnc = (int)iEnc;
	rc = inflateInit2(&pZ->z,ZlibWindowBits((int)iEnc,sOpt.iWindow));
	if( rc != Z_OK ){
		SyBlobRelease(&sDict);
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"%s",zError(rc));
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pZ->bInit = 1;
	if( sOpt.zDict && sOpt.nDict > 0 ){
		if( (int)iEnc == PHL_Z_RAW ){
			/* A raw stream has no place to announce a dictionary, so libz takes
			 * it up front. */
			inflateSetDictionary(&pZ->z,(const Bytef *)sOpt.zDict,(uInt)sOpt.nDict);
		}else{
			/* A zlib-framed one ASKS, through Z_NEED_DICT, part way into the
			 * first inflate -- so the bytes have to outlive this call. */
			SyBlobAppend(&pZ->sDict,sOpt.zDict,sOpt.nDict);
		}
	}
	SyBlobRelease(&sDict);
	pThis = ZlibNewContextObject(pCtx->pVm,1);
	if( pThis == 0 || ZctxAttach(pThis,pZ) != 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	PH7_NativeResultObject(pCtx,pThis);
	return PH7_OK;
}
/* The context an argument names, with php's TypeError for anything else. */
static phl_zctx * ZctxFromArg(ph7_context *pCtx,ph7_value *pArg,int bInflate)
{
	ph7_class_instance *pThis;
	phl_zctx *pZ;
	if( pArg == 0 || !ph7_value_is_object(pArg) ){
		return 0;
	}
	pThis = (ph7_class_instance *)pArg->x.pOther;
	pZ = ZctxOfInstance(pThis);
	if( pZ == 0 || pZ->bInflate != bInflate || !pZ->bInit ){
		return 0;
	}
	SXUNUSED(pCtx);
	return pZ;
}
static int ZlibScreenFlush(ph7_context *pCtx,sxi64 iFlush)
{
	if( iFlush == Z_NO_FLUSH || iFlush == Z_PARTIAL_FLUSH || iFlush == Z_SYNC_FLUSH
	 || iFlush == Z_FULL_FLUSH || iFlush == Z_BLOCK || iFlush == Z_FINISH ){
		return 0;
	}
	PH7_VmThrowException(pCtx,"ValueError",
		"%s(): Argument #3 ($flush_mode) must be one of ZLIB_NO_FLUSH, "
		"ZLIB_PARTIAL_FLUSH, ZLIB_SYNC_FLUSH, ZLIB_FULL_FLUSH, ZLIB_BLOCK, "
		"or ZLIB_FINISH",ph7_function_name(pCtx));
	return -1;
}
/* string|false deflate_add(DeflateContext $context, string $data, int $flush_mode = ZLIB_SYNC_FLUSH) */
static int PH7_builtin_deflate_add(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_zctx *pZ;
	const char *zIn;
	int nIn = 0,rc;
	sxi64 iFlush = Z_SYNC_FLUSH;
	SyBlob sOut;
	unsigned char zBuf[PHL_Z_CHUNK];
	if( nArg < 2 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pZ = ZctxFromArg(pCtx,apArg[0],0);
	if( pZ == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zIn = ph7_value_to_string(apArg[1],&nIn);
	if( nArg > 2 ){
		iFlush = ph7_value_to_int64(apArg[2]);
	}
	if( ZlibScreenFlush(pCtx,iFlush) != 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);
	pZ->z.next_in = (Bytef *)zIn;
	pZ->z.avail_in = (uInt)nIn;
	do {
		pZ->z.next_out = (Bytef *)zBuf;
		pZ->z.avail_out = (uInt)sizeof(zBuf);
		rc = deflate(&pZ->z,(int)iFlush);
		if( rc != Z_OK && rc != Z_STREAM_END && rc != Z_BUF_ERROR ){
			SyBlobRelease(&sOut);
			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"%s",zError(rc));
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		SyBlobAppend(&sOut,zBuf,(sxu32)(sizeof(zBuf) - pZ->z.avail_out));
	} while( pZ->z.avail_out == 0 );
	if( iFlush == Z_FINISH ){
		/* php restarts the stream, so the context can be used again -- which is
		 * why a second deflate_add(..., ZLIB_FINISH) answers a fresh member
		 * rather than an error. */
		deflateReset(&pZ->z);
	}
	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));
	SyBlobRelease(&sOut);
	return PH7_OK;
}
/* string|false inflate_add(InflateContext $context, string $data, int $flush_mode = ZLIB_SYNC_FLUSH) */
static int PH7_builtin_inflate_add(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_zctx *pZ;
	const char *zIn;
	int nIn = 0,rc = Z_OK;
	sxi64 iFlush = Z_SYNC_FLUSH;
	SyBlob sOut;
	unsigned char zBuf[PHL_Z_CHUNK];
	if( nArg < 2 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pZ = ZctxFromArg(pCtx,apArg[0],1);
	if( pZ == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zIn = ph7_value_to_string(apArg[1],&nIn);
	if( nArg > 2 ){
		iFlush = ph7_value_to_int64(apArg[2]);
	}
	if( ZlibScreenFlush(pCtx,iFlush) != 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);
	pZ->z.next_in = (Bytef *)zIn;
	pZ->z.avail_in = (uInt)nIn;
	while( nIn > 0 ){
		pZ->z.next_out = (Bytef *)zBuf;
		pZ->z.avail_out = (uInt)sizeof(zBuf);
		rc = inflate(&pZ->z,(int)iFlush);
		SyBlobAppend(&sOut,zBuf,(sxu32)(sizeof(zBuf) - pZ->z.avail_out));
		if( rc == Z_NEED_DICT && SyBlobLength(&pZ->sDict) > 0
		 && inflateSetDictionary(&pZ->z,(const Bytef *)SyBlobData(&pZ->sDict),
				(uInt)SyBlobLength(&pZ->sDict)) == Z_OK ){
			/* The stream named a dictionary and inflate_init() was given one:
			 * hand it over and carry on where libz stopped. */
			continue;
		}
		if( rc == Z_STREAM_END ){
			break;
		}
		if( rc != Z_OK && rc != Z_BUF_ERROR ){
			SyBlobRelease(&sOut);
			pZ->iStatus = rc;
			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"%s",zError(rc));
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		if( pZ->z.avail_out != 0 || pZ->z.avail_in == 0 ){
			break;
		}
	}
	pZ->nReadLen = (sxu32)pZ->z.total_in;
	pZ->iStatus = rc == Z_STREAM_END ? Z_STREAM_END : Z_OK;
	if( rc == Z_STREAM_END ){
		/* Same restart as the deflate side: php's context takes another member. */
		inflateReset(&pZ->z);
	}
	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));
	SyBlobRelease(&sOut);
	return PH7_OK;
}
/* int inflate_get_status(InflateContext $context) */
static int PH7_builtin_inflate_get_status(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_zctx *pZ = nArg > 0 ? ZctxFromArg(pCtx,apArg[0],1) : 0;
	ph7_result_int(pCtx,pZ ? pZ->iStatus : 0);
	return PH7_OK;
}
/* int inflate_get_read_len(InflateContext $context) */
static int PH7_builtin_inflate_get_read_len(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_zctx *pZ = nArg > 0 ? ZctxFromArg(pCtx,apArg[0],1) : 0;
	ph7_result_int64(pCtx,pZ ? (sxi64)pZ->nReadLen : 0);
	return PH7_OK;
}
/* ------------------------------------------------------------------ */
/* The ZLIB device: gzopen() and compress.zlib://                      */
/* ------------------------------------------------------------------ */
/*
 * One handle. The compressed side is an ordinary engine stream (so
 * compress.zlib:// nests over anything the engine can open) and the
 * uncompressed side is this record's own position.
 */
typedef struct phl_zstream phl_zstream;
struct phl_zstream {
	ph7_vm *pVm;
	io_private *pInner;        /* the compressed stream underneath */
	z_stream z;
	int bWrite;                /* opened for writing */
	int bInit;                 /* z is live */
	int bTransparent;          /* reading a stream that is not gzip-framed */
	int bEof;                  /* the compressed side is exhausted */
	int bDone;                 /* no more members to read */
	int iLevel,iStrategy;      /* what the mode string asked for */
	ph7_int64 iPos;            /* UNCOMPRESSED position, which is what gztell() answers */
	unsigned char zIn[PHL_Z_CHUNK];
	sxu32 nIn,nInPos;          /* compressed bytes buffered / consumed */
	SyBlob sOut;               /* inflated bytes not yet handed to the caller */
	sxu32 nOutPos;
	SyBlob sUri;               /* what to reopen for a backwards seek */
};
/*
 * gzopen()'s mode string carries a compression LEVEL and a strategy letter
 * that the engine's own mode grammar knows nothing about, and xOpen() is handed
 * flags rather than the string. php passes the whole string to libz; here the
 * two extra pieces are armed on the VM by the door that parsed them and read
 * back by the open below. Nothing else can be between the two calls -- an open
 * is not re-entrant on one VM -- and the wrapper door leaves them at libz's
 * defaults.
 */
PH7_PRIVATE void PH7_ZlibArmOpen(ph7_vm *pVm,int iLevel,int iStrategy)
{
	pVm->iZlibLevel = iLevel;
	pVm->iZlibStrategy = iStrategy;
}
/* gzopen() and its two whole-file doors, which name a FILE rather than a url. */
PH7_PRIVATE void PH7_ZlibArmDirect(ph7_vm *pVm,int bDirect)
{
	pVm->bZlibDirect = bDirect;
}
static void ZStreamEndZ(phl_zstream *pZ)
{
	if( pZ->bInit ){
		if( pZ->bWrite ){
			deflateEnd(&pZ->z);
		}else{
			inflateEnd(&pZ->z);
		}
		pZ->bInit = 0;
	}
}
/* Fill the compressed buffer from the stream underneath. */
static int ZStreamFill(phl_zstream *pZ)
{
	ph7_int64 nRead;
	if( pZ->nInPos < pZ->nIn ){
		return 0;
	}
	if( pZ->bEof ){
		return -1;
	}
	nRead = PH7_StreamRead(pZ->pInner,pZ->zIn,(ph7_int64)sizeof(pZ->zIn));
	if( nRead < 1 ){
		pZ->bEof = 1;
		pZ->nIn = pZ->nInPos = 0;
		return -1;
	}
	pZ->nIn = (sxu32)nRead;
	pZ->nInPos = 0;
	return 0;
}
/* At least nWant bytes buffered, if the stream has them. */
static int ZStreamPeek(phl_zstream *pZ,sxu32 nWant)
{
	while( pZ->nIn - pZ->nInPos < nWant ){
		ph7_int64 nRead;
		if( pZ->bEof ){
			return -1;
		}
		if( pZ->nInPos > 0 ){
			SyMemcpy(&pZ->zIn[pZ->nInPos],pZ->zIn,pZ->nIn - pZ->nInPos);
			pZ->nIn -= pZ->nInPos;
			pZ->nInPos = 0;
		}
		if( pZ->nIn >= sizeof(pZ->zIn) ){
			return 0;
		}
		nRead = PH7_StreamRead(pZ->pInner,&pZ->zIn[pZ->nIn],
			(ph7_int64)(sizeof(pZ->zIn) - pZ->nIn));
		if( nRead < 1 ){
			pZ->bEof = 1;
			return -1;
		}
		pZ->nIn += (sxu32)nRead;
	}
	return 0;
}
/*
 * Start (or restart) the inflate stream at a member boundary. libz parses the
 * gzip header itself with windowBits 16+15; what this decides is whether there
 * IS a member here at all -- two bytes of magic -- which is libz's transparent
 * mode and the reason gzopen() reads a plain text file as text.
 */
static int ZStreamStartMember(phl_zstream *pZ,int bFirst)
{
	const unsigned char *z;
	if( ZStreamPeek(pZ,2) != 0 ){
		pZ->bDone = 1;
		return -1;
	}
	z = &pZ->zIn[pZ->nInPos];
	if( z[0] != 0x1F || z[1] != 0x8B ){
		if( bFirst ){
			/* Not compressed at all: the whole stream is passed through. */
			pZ->bTransparent = 1;
			return 0;
		}
		/* Bytes after the last member are not a member; libz stops there. */
		pZ->bDone = 1;
		return -1;
	}
	if( pZ->bInit ){
		if( inflateReset(&pZ->z) != Z_OK ){
			pZ->bDone = 1;
			return -1;
		}
		return 0;
	}
	SyZero(&pZ->z,sizeof(pZ->z));
	if( inflateInit2(&pZ->z,PHL_Z_GZIP) != Z_OK ){
		pZ->bDone = 1;
		return -1;
	}
	pZ->bInit = 1;
	return 0;
}
/* Inflate one buffer's worth into sOut. Answers 0 when it produced bytes or
 * hit the end, -1 when the stream is finished for good. */
static int ZStreamPump(phl_zstream *pZ)
{
	unsigned char zBuf[PHL_Z_CHUNK];
	int rc;
	if( pZ->bDone ){
		return -1;
	}
	if( pZ->bTransparent ){
		if( ZStreamFill(pZ) != 0 ){
			pZ->bDone = 1;
			return -1;
		}
		SyBlobAppend(&pZ->sOut,&pZ->zIn[pZ->nInPos],pZ->nIn - pZ->nInPos);
		pZ->nInPos = pZ->nIn;
		return 0;
	}
	if( !pZ->bInit && ZStreamStartMember(pZ,pZ->iPos == 0 && SyBlobLength(&pZ->sOut) == 0) != 0 ){
		return -1;
	}
	if( pZ->bTransparent ){
		return ZStreamPump(pZ);
	}
	for(;;){
		if( pZ->nInPos >= pZ->nIn && ZStreamFill(pZ) != 0 ){
			/* Truncated member: libz's gzread ends the stream where the bytes
			 * end rather than failing the read. */
			pZ->bDone = 1;
			return SyBlobLength(&pZ->sOut) > pZ->nOutPos ? 0 : -1;
		}
		pZ->z.next_in = (Bytef *)&pZ->zIn[pZ->nInPos];
		pZ->z.avail_in = (uInt)(pZ->nIn - pZ->nInPos);
		pZ->z.next_out = (Bytef *)zBuf;
		pZ->z.avail_out = (uInt)sizeof(zBuf);
		rc = inflate(&pZ->z,Z_NO_FLUSH);
		pZ->nInPos = pZ->nIn - pZ->z.avail_in;
		if( sizeof(zBuf) - pZ->z.avail_out > 0 ){
			SyBlobAppend(&pZ->sOut,zBuf,(sxu32)(sizeof(zBuf) - pZ->z.avail_out));
		}
		if( rc == Z_STREAM_END ){
			/* Another member may follow; anything else ends the stream. */
			if( ZStreamStartMember(pZ,0) != 0 ){
				pZ->bDone = 1;
			}
			return 0;
		}
		if( rc != Z_OK ){
			pZ->bDone = 1;
			return SyBlobLength(&pZ->sOut) > pZ->nOutPos ? 0 : -1;
		}
		if( sizeof(zBuf) - pZ->z.avail_out > 0 ){
			return 0;
		}
	}
}
/* Drop what the caller has already taken, so a long read does not keep the
 * whole file. */
static void ZStreamTrimOut(phl_zstream *pZ)
{
	if( pZ->nOutPos > 0 && pZ->nOutPos >= SyBlobLength(&pZ->sOut) ){
		SyBlobReset(&pZ->sOut);
		pZ->nOutPos = 0;
	}
}
static ph7_int64 ZStreamRead(void *pHandle,void *pBuffer,ph7_int64 nWant)
{
	phl_zstream *pZ = (phl_zstream *)pHandle;
	sxu32 nCopy = 0;
	unsigned char *zOut = (unsigned char *)pBuffer;
	if( pZ == 0 || pZ->bWrite || nWant < 1 ){
		return pZ && pZ->bWrite ? -1 : 0;
	}
	while( nCopy < (sxu32)nWant ){
		sxu32 nHave = SyBlobLength(&pZ->sOut) - pZ->nOutPos;
		if( nHave == 0 ){
			ZStreamTrimOut(pZ);
			if( ZStreamPump(pZ) != 0 ){
				break;
			}
			continue;
		}
		if( nHave > (sxu32)nWant - nCopy ){
			nHave = (sxu32)nWant - nCopy;
		}
		SyMemcpy((const char *)SyBlobData(&pZ->sOut) + pZ->nOutPos,&zOut[nCopy],nHave);
		pZ->nOutPos += nHave;
		nCopy += nHave;
	}
	pZ->iPos += (ph7_int64)nCopy;
	return (ph7_int64)nCopy;
}
static ph7_int64 ZStreamWrite(void *pHandle,const void *pData,ph7_int64 nLen)
{
	phl_zstream *pZ = (phl_zstream *)pHandle;
	unsigned char zBuf[PHL_Z_CHUNK];
	int rc;
	if( pZ == 0 ){
		return -1;
	}
	if( !pZ->bWrite ){
		/* php's gzwrite() on a read handle is a plain 0 -- no bytes, no
		 * diagnostic -- rather than the failed write a -1 would report. */
		return 0;
	}
	if( nLen < 1 ){
		return 0;
	}
	/*
	 * Z_SYNC_FLUSH per write, which is php's and is OBSERVABLE: the file grows
	 * by a sync marker at every gzwrite(), so a gzopen()+gzwrite()+gzclose()
	 * pair is six bytes longer than the same bytes through gzencode(). What it
	 * buys is a file that can be read up to the last completed write even if
	 * the writer never closed it.
	 */
	pZ->z.next_in = (Bytef *)pData;
	pZ->z.avail_in = (uInt)nLen;
	for(;;){
		pZ->z.next_out = (Bytef *)zBuf;
		pZ->z.avail_out = (uInt)sizeof(zBuf);
		rc = deflate(&pZ->z,Z_SYNC_FLUSH);
		if( rc != Z_OK && rc != Z_BUF_ERROR ){
			return -1;
		}
		if( sizeof(zBuf) - pZ->z.avail_out > 0 ){
			PH7_StreamWrite(pZ->pInner,zBuf,
				(ph7_int64)(sizeof(zBuf) - pZ->z.avail_out));
		}
		if( pZ->z.avail_out != 0 ){
			break;
		}
	}
	pZ->iPos += nLen;
	return nLen;
}
/* Finish the compressed side: the last deflate block and libz's trailer. */
static void ZStreamFinishWrite(phl_zstream *pZ)
{
	unsigned char zBuf[PHL_Z_CHUNK];
	int rc;
	if( !pZ->bWrite || !pZ->bInit ){
		return;
	}
	pZ->z.next_in = (Bytef *)"";
	pZ->z.avail_in = 0;
	do {
		pZ->z.next_out = (Bytef *)zBuf;
		pZ->z.avail_out = (uInt)sizeof(zBuf);
		rc = deflate(&pZ->z,Z_FINISH);
		if( sizeof(zBuf) - pZ->z.avail_out > 0 ){
			PH7_StreamWrite(pZ->pInner,zBuf,
				(ph7_int64)(sizeof(zBuf) - pZ->z.avail_out));
		}
	} while( rc == Z_OK );
}
static void ZStreamClose(void *pHandle)
{
	phl_zstream *pZ = (phl_zstream *)pHandle;
	ph7_vm *pVm;
	if( pZ == 0 ){
		return;
	}
	pVm = pZ->pVm;
	ZStreamFinishWrite(pZ);
	ZStreamEndZ(pZ);
	if( pZ->pInner ){
		if( pZ->pInner->pStream ){
			PH7_StreamCloseHandle(pZ->pInner->pStream,pZ->pInner->pHandle);
		}
		SyBlobRelease(&pZ->pInner->sBuffer);
		SyBlobRelease(&pZ->pInner->sFilt);
		SyBlobRelease(&pZ->pInner->sUri);
		SyMemBackendFree(&pVm->sAllocator,pZ->pInner);
		pZ->pInner = 0;
	}
	SyBlobRelease(&pZ->sOut);
	SyBlobRelease(&pZ->sUri);
	SyMemBackendFree(&pVm->sAllocator,pZ);
}
/*
 * php's ZLIB handle can be flock()ed: the lock belongs to the descriptor of the
 * file UNDERNEATH, which is where this one sends it. A device with no lock of
 * its own (a nested php:// or data:// stream) answers as it would on its own.
 */
static int ZStreamLock(void *pHandle,int iType)
{
	phl_zstream *pZ = (phl_zstream *)pHandle;
	if( pZ == 0 || pZ->pInner == 0 || pZ->pInner->pStream == 0
	 || pZ->pInner->pStream->xLock == 0 ){
		return PH7_OK;
	}
	return pZ->pInner->pStream->xLock(pZ->pInner->pHandle,iType);
}
static ph7_int64 ZStreamTell(void *pHandle)
{
	phl_zstream *pZ = (phl_zstream *)pHandle;
	return pZ ? pZ->iPos : -1;
}
/*
 * php's ZLIB stream reports itself SEEKABLE and means it the way libz does:
 * forward by reading and discarding, backwards by starting the file again.
 * SEEK_END is the one php refuses outright, because libz cannot know the
 * uncompressed length without decoding the whole member.
 */
static int ZStreamSeek(void *pHandle,ph7_int64 iOfft,int whence)
{
	phl_zstream *pZ = (phl_zstream *)pHandle;
	ph7_int64 iTarget;
	if( pZ == 0 ){
		return -1;
	}
	if( whence == 2 /* SEEK_END */ ){
		PH7_VmThrowError(pZ->pVm,pZ->pVm->pCalleeName,PH7_CTX_WARNING,
			"SEEK_END is not supported");
		return -1;
	}
	iTarget = whence == 1 /* SEEK_CUR */ ? pZ->iPos + iOfft : iOfft;
	if( iTarget < 0 ){
		return -1;
	}
	if( pZ->bWrite ){
		/* libz pads a forward seek with zeroes and refuses a backwards one. */
		if( iTarget < pZ->iPos ){
			return -1;
		}
		while( pZ->iPos < iTarget ){
			static const unsigned char zZero[256] = { 0 };
			ph7_int64 nStep = iTarget - pZ->iPos;
			if( nStep > (ph7_int64)sizeof(zZero) ){
				nStep = (ph7_int64)sizeof(zZero);
			}
			if( ZStreamWrite(pZ,zZero,nStep) != nStep ){
				return -1;
			}
		}
		return 0;
	}
	if( iTarget < pZ->iPos ){
		/* Start the compressed side again and inflate forward. */
		if( PH7_StreamSeekWrapped(pZ->pInner,0,0 /* SEEK_SET */) != PH7_OK ){
			return -1;
		}
		ZStreamEndZ(pZ);
		SyBlobReset(&pZ->sOut);
		pZ->nOutPos = pZ->nIn = pZ->nInPos = 0;
		pZ->bEof = pZ->bDone = pZ->bTransparent = 0;
		pZ->iPos = 0;
	}
	while( pZ->iPos < iTarget ){
		unsigned char zSkip[PHL_Z_CHUNK];
		ph7_int64 nStep = iTarget - pZ->iPos;
		ph7_int64 nGot;
		if( nStep > (ph7_int64)sizeof(zSkip) ){
			nStep = (ph7_int64)sizeof(zSkip);
		}
		nGot = ZStreamRead(pZ,zSkip,nStep);
		if( nGot < 1 ){
			/* php's gzseek() past the end still MOVES: the position it reports
			 * is the one asked for, and the next read answers "". */
			pZ->iPos = iTarget;
			break;
		}
	}
	return 0;
}
/* Nothing under a ZLIB handle has an fstat() answer: php's is a flat false. */
static int ZStreamStat(void *pHandle,ph7_value *pArray,ph7_value *pWorker)
{
	SXUNUSED(pHandle);
	SXUNUSED(pArray);
	SXUNUSED(pWorker);
	return -1;
}
/*
 * int (*xOpen)(const char *,int,ph7_value *,void **)
 *
 * The name is whatever followed `compress.zlib://` (or the plain path gzopen()
 * was given), and it is opened through the ordinary device dispatch -- so the
 * inner stream may itself be a wrapper, and a gzip file over http:// or data://
 * reads exactly like one on disk.
 */
static int ZStreamOpen(const char *zName,int iMode,ph7_value *pResource,void **ppHandle)
{
	const ph7_io_stream *pInnerDev;
	ph7_vm *pVm = pResource ? pResource->pVm : 0;
	phl_zstream *pZ;
	io_private *pInner;
	const char *zPath = zName;
	int iInnerMode,rc,bRefuse;
	if( pVm == 0 ){
		return -1;
	}
	pZ = (phl_zstream *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_zstream));
	if( pZ == 0 ){
		return -1;
	}
	SyZero(pZ,sizeof(*pZ));
	pZ->pVm = pVm;
	SyBlobInit(&pZ->sOut,&pVm->sAllocator);
	SyBlobInit(&pZ->sUri,&pVm->sAllocator);
	pZ->iLevel = pVm->iZlibLevel;
	pZ->iStrategy = pVm->iZlibStrategy;
	/* The armed options describe THIS open and nothing after it. */
	PH7_ZlibArmOpen(pVm,-1,Z_DEFAULT_STRATEGY);
	/*
	 * php's zlib wrapper takes ONE direction and nothing else: `r`, `w` and `a`
	 * with their b/t hints. A mode asking for both (`r+`, `w+`, `c+`) is refused
	 * before anything is opened, while `x` and `c` are refused only after php has
	 * opened the file underneath -- so a refused `x` still LEAVES the file it
	 * created. The refusal itself is the wrapper's flat one either way.
	 */
	if( (iMode & PH7_IO_OPEN_RDWR) != 0 ){
		SyBlobRelease(&pZ->sOut);
		SyBlobRelease(&pZ->sUri);
		SyMemBackendFree(&pVm->sAllocator,pZ);
		return -1;
	}
	bRefuse = (iMode & PH7_IO_OPEN_EXCL) != 0
		|| ((iMode & PH7_IO_OPEN_CREATE) != 0
			&& (iMode & (PH7_IO_OPEN_TRUNC|PH7_IO_OPEN_APPEND)) == 0);
	pZ->bWrite = (iMode & (PH7_IO_OPEN_WRONLY|PH7_IO_OPEN_APPEND|PH7_IO_OPEN_RDWR)) != 0;
	iInnerMode = bRefuse ? iMode
		: (pZ->bWrite
			? (iMode & PH7_IO_OPEN_APPEND
				? PH7_IO_OPEN_WRONLY|PH7_IO_OPEN_CREATE|PH7_IO_OPEN_APPEND
				: PH7_IO_OPEN_WRONLY|PH7_IO_OPEN_CREATE|PH7_IO_OPEN_TRUNC)
			: PH7_IO_OPEN_RDONLY);
	pInner = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));
	if( pInner == 0 ){
		SyBlobRelease(&pZ->sOut);
		SyBlobRelease(&pZ->sUri);
		SyMemBackendFree(&pVm->sAllocator,pZ);
		return -1;
	}
	SyZero(pInner,sizeof(*pInner));
	pInnerDev = PH7_VmGetStreamDevice(pVm,&zPath,(int)SyStrlen(zPath));
	InitIOPrivate(pVm,pInnerDev,pInner);
	/*
	 * gzopen() names a FILE, and the failure a script reads from it is that
	 * file's own -- the errno for a path, "Connection refused" for an http://
	 * one -- so its inner open reports as though it WERE the outer one, where an
	 * open running inside another is otherwise kept from naming the failure its
	 * caller will report. Through `compress.zlib://` the same open is a WRAPPER's
	 * and php answers every one of those with a flat "operation failed".
	 */
	if( pVm->bZlibDirect ){
		pVm->nOpenDepth--;
	}
	pInner->pHandle = pInnerDev
		? PH7_StreamOpenHandle(pVm,pInnerDev,zPath,iInnerMode,FALSE,0,FALSE,0,0)
		: 0;
	if( pVm->bZlibDirect ){
		pVm->nOpenDepth++;
	}else{
		/* php's WRAPPER never repeats what the file underneath said. */
		PH7_StreamSetOpenError(pVm,"operation failed");
	}
	if( pInner->pHandle == 0 ){
		SyMemBackendFree(&pVm->sAllocator,pInner);
		SyBlobRelease(&pZ->sOut);
		SyBlobRelease(&pZ->sUri);
		SyMemBackendFree(&pVm->sAllocator,pZ);
		return -1;
	}
	if( bRefuse ){
		/* The file exists now, which is what php leaves behind; the stream does
		 * not, because libz has no direction for the mode that made it. */
		PH7_StreamCloseHandle(pInnerDev,pInner->pHandle);
		SyMemBackendFree(&pVm->sAllocator,pInner);
		SyBlobRelease(&pZ->sOut);
		SyBlobRelease(&pZ->sUri);
		SyMemBackendFree(&pVm->sAllocator,pZ);
		PH7_StreamSetOpenError(pVm,"operation failed");
		return -1;
	}
	SetIOPrivateOpenedAs(pInner,zName,(int)SyStrlen(zName),pZ->bWrite ? "wb" : "rb",2);
	pZ->pInner = pInner;
	SyBlobAppend(&pZ->sUri,zName,(sxu32)SyStrlen(zName));
	if( pZ->bWrite ){
		SyZero(&pZ->z,sizeof(pZ->z));
		rc = deflateInit2(&pZ->z,pZ->iLevel,Z_DEFLATED,PHL_Z_GZIP,
			PHL_Z_MEMLEVEL,pZ->iStrategy);
		if( rc != Z_OK ){
			ZStreamClose((void *)pZ);
			return -1;
		}
		pZ->bInit = 1;
	}
	*ppHandle = (void *)pZ;
	return PH7_OK;
}
PH7_PRIVATE const ph7_io_stream sZLIB_Stream = {
	"compress.zlib",
	PH7_IO_STREAM_VERSION,
	ZStreamOpen,   /* xOpen */
	0,             /* xOpenDir: php's is "not implemented" */
	ZStreamClose,  /* xClose */
	0,             /* xCloseDir */
	ZStreamRead,   /* xRead */
	0,             /* xReadDir */
	ZStreamWrite,  /* xWrite */
	ZStreamSeek,   /* xSeek */
	ZStreamLock,   /* xLock */
	0,             /* xRewindDir */
	ZStreamTell,   /* xTell */
	0,             /* xTrunc: php's "Can't truncate this stream!" */
	0,             /* xSync */
	ZStreamStat    /* xStat: php answers a flat false */
};
PH7_PRIVATE int PH7_ZlibStreamIs(const ph7_io_stream *pStream)
{
	return pStream == &sZLIB_Stream;
}
/* ------------------------------------------------------------------ */
/* gzopen() and the two whole-file readers                            */
/* ------------------------------------------------------------------ */
/*
 * php hands gzopen()'s mode string to libz, which reads it as: one of r/w/a,
 * an optional `b`, an optional LEVEL digit and an optional strategy letter
 * (f filtered, h Huffman-only, R run-length, F fixed). php screens one thing
 * before libz sees it -- a `+` of any kind -- because a zlib stream cannot be
 * read and written at once.
 */
static int ZlibParseMode(const char *zMode,int nMode,int *piLevel,int *piStrategy,
	char *zClean,int nClean)
{
	int i,n = 0;
	*piLevel = -1;
	*piStrategy = Z_DEFAULT_STRATEGY;
	for( i = 0 ; i < nMode && n + 1 < nClean ; ++i ){
		int c = zMode[i];
		if( c >= '0' && c <= '9' ){
			*piLevel = c - '0';
			continue;
		}
		switch( c ){
		case 'f': *piStrategy = Z_FILTERED; continue;
		case 'h': *piStrategy = Z_HUFFMAN_ONLY; continue;
		case 'R': *piStrategy = Z_RLE; continue;
		case 'F': *piStrategy = Z_FIXED; continue;
		case 'T': continue;   /* libz's "write transparently" */
		default: break;
		}
		zClean[n++] = (char)c;
	}
	zClean[n] = 0;
	return n;
}
/*
 * The open both gzopen() and the two whole-file doors go through. It is
 * fopen()'s sequence with the device fixed: no scheme is parsed off the path
 * (gzopen('f.gz') names a FILE), the compressed side is resolved by the
 * device's own open, and the handle is left with NO uri -- which is what makes
 * php report neither a `wrapper_type` nor a `uri` for a gzopen() stream, where
 * the same file through compress.zlib:// reports both.
 */
static io_private * ZlibOpenDevice(ph7_context *pCtx,ph7_value *pPath,
	const char *zMode,int nMode,int bUseInclude,int iForceFlags)
{
	const ph7_io_stream *pStream = &sZLIB_Stream;
	io_private *pDev;
	const char *zUri;
	int nUri = 0,iFlags;
	zUri = ph7_value_to_string(pPath,&nUri);
	if( PH7_VfsEmptyPathRefused(pCtx,nUri) ){
		return 0;
	}
	switch( zMode[0] ){
	case 'w': iFlags = PH7_IO_OPEN_WRONLY|PH7_IO_OPEN_CREATE|PH7_IO_OPEN_TRUNC; break;
	case 'a': iFlags = PH7_IO_OPEN_WRONLY|PH7_IO_OPEN_CREATE|PH7_IO_OPEN_APPEND; break;
	default:  iFlags = PH7_IO_OPEN_RDONLY; break;
	}
	if( iForceFlags != 0 ){
		/* A mode php's grammar takes and libz has no direction for -- `x`. php
		 * still OPENS the file (so an existing one is `File exists` and a
		 * missing one is created) and only then discovers it cannot compress
		 * through it, so the open here is the plain one, without this device
		 * over it. */
		const char *zTail = zUri;
		iFlags = iForceFlags;
		pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zTail,nUri);
		zUri = zTail;
		if( pStream == 0 ){
			VfsThrowNoDeviceWarning(pCtx,zUri,FALSE);
			return 0;
		}
	}
	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);
	if( pDev == 0 ){
		return 0;
	}
	InitIOPrivate(pCtx->pVm,pStream,pDev);
	PH7_StreamArmOpenMode(pCtx->pVm,zMode,nMode);
	PH7_ZlibArmDirect(pCtx->pVm,1);
	pDev->pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zUri,iFlags,
		bUseInclude,0,FALSE,0,ph7_function_name(pCtx));
	PH7_ZlibArmDirect(pCtx->pVm,0);
	if( pDev->pHandle == 0 ){
		VfsThrowOpenWarning(pCtx,zUri);
		PH7_StreamReleaseUnopened(pCtx,pDev);
		return 0;
	}
	SetIOPrivateOpenedAs(pDev,0,0,zMode,nMode);
	return pDev;
}
/* resource|false gzopen(string $filename, string $mode, bool $use_include_path = false) */
static int PH7_builtin_gzopen(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zMode;
	char zClean[16];
	io_private *pDev;
	int nMode = 0,nClean,iLevel,iStrategy,i;
	if( nArg < 2 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zMode = ph7_value_to_string(apArg[1],&nMode);
	for( i = 0 ; i < nMode ; ++i ){
		if( zMode[i] == '+' ){
			ph7_context_throw_error(pCtx,PH7_CTX_WARNING,
				"Cannot open a zlib stream for reading and writing at the same time!");
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
	}
	nClean = ZlibParseMode(zMode,nMode,&iLevel,&iStrategy,zClean,(int)sizeof(zClean));
	if( nClean < 1 || (zClean[0] != 'r' && zClean[0] != 'w' && zClean[0] != 'a') ){
		/* TWO refusals, and which one php raises says whose grammar the mode
		 * broke. A letter php's own fopen grammar does not know is that
		 * sentence, named for gzopen and the path. One php ACCEPTS and libz has
		 * no direction for -- `x` -- is opened first and refused after, so an
		 * existing file reports `File exists` from the open and a missing one
		 * is created and then reports libz's flat failure. */
		int iFlags = 0;
		if( PH7_StreamModeIsValid(zMode,nMode,&iFlags) ){
			io_private *pTmp = ZlibOpenDevice(pCtx,apArg[0],zMode,nMode,
				nArg > 2 ? ph7_value_to_bool(apArg[2]) : FALSE,
				iFlags ? iFlags : PH7_IO_OPEN_RDONLY);
			if( pTmp ){
				PH7_StreamCloseHandle(pTmp->pStream,pTmp->pHandle);
				MarkIOPrivateClosed(pTmp);
				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"gzopen failed");
			}
		}else{
			int nPath = 0;
			const char *zPath = ph7_value_to_string(apArg[0],&nPath);
			PH7_VmThrowWarningFmt(pCtx->pVm,
				"%s(%.*s): Failed to open stream: `%.*s' is not a valid mode for fopen",
				ph7_function_name(pCtx),nPath,zPath,nMode,zMode);
		}
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	PH7_ZlibArmOpen(pCtx->pVm,iLevel,iStrategy);
	pDev = ZlibOpenDevice(pCtx,apArg[0],zClean,nClean,
		nArg > 2 ? ph7_value_to_bool(apArg[2]) : FALSE,0);
	PH7_ZlibArmOpen(pCtx->pVm,-1,Z_DEFAULT_STRATEGY);
	if( pDev == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_resource(pCtx,pDev);
	return PH7_OK;
}
/*
 * The two whole-file doors. Both open the same device and read it to the end;
 * gzfile() splits on newlines the way file() does and readgzfile() prints.
 */
static int ZlibWholeFile(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLines)
{
	SyBlob sAll;
	void *pHandle;
	const char *zUri;
	int nUri = 0;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zUri = ph7_value_to_string(apArg[0],&nUri);
	if( PH7_VfsEmptyPathRefused(pCtx,nUri) ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* No io_private: nothing here reaches PHP, so the device handle is opened,
	 * read and closed inside this call the way every other whole-file reader in
	 * the engine does it. */
	PH7_ZlibArmOpen(pCtx->pVm,-1,Z_DEFAULT_STRATEGY);
	PH7_StreamArmOpenMode(pCtx->pVm,"rb",2);
	PH7_ZlibArmDirect(pCtx->pVm,1);
	pHandle = PH7_StreamOpenHandle(pCtx->pVm,&sZLIB_Stream,zUri,PH7_IO_OPEN_RDONLY,
		nArg > 1 ? ph7_value_to_bool(apArg[1]) : FALSE,0,FALSE,0,
		ph7_function_name(pCtx));
	PH7_ZlibArmDirect(pCtx->pVm,0);
	if( pHandle == 0 ){
		VfsThrowOpenWarning(pCtx,zUri);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	SyBlobInit(&sAll,&pCtx->pVm->sAllocator);
	for(;;){
		char zBuf[PHL_Z_CHUNK];
		ph7_int64 nRead = ZStreamRead(pHandle,zBuf,(ph7_int64)sizeof(zBuf));
		if( nRead < 1 ){
			break;
		}
		SyBlobAppend(&sAll,zBuf,(sxu32)nRead);
	}
	PH7_StreamCloseHandle(&sZLIB_Stream,pHandle);
	if( bLines ){
		const char *zData = (const char *)SyBlobData(&sAll);
		sxu32 nLen = SyBlobLength(&sAll),nStart = 0,n;
		ph7_value *pArray = ph7_context_new_array(pCtx);
		ph7_value *pLine = ph7_context_new_scalar(pCtx);
		if( pArray == 0 || pLine == 0 ){
			SyBlobRelease(&sAll);
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		for( n = 0 ; n < nLen ; ++n ){
			if( zData[n] == '\n' ){
				ph7_value_string(pLine,&zData[nStart],(int)(n - nStart + 1));
				ph7_array_add_elem(pArray,0,pLine);
				ph7_value_reset_string_cursor(pLine);
				nStart = n + 1;
			}
		}
		if( nStart < nLen ){
			ph7_value_string(pLine,&zData[nStart],(int)(nLen - nStart));
			ph7_array_add_elem(pArray,0,pLine);
		}
		ph7_result_value(pCtx,pArray);
		ph7_context_release_value(pCtx,pLine);
	}else{
		ph7_context_output(pCtx,(const char *)SyBlobData(&sAll),(int)SyBlobLength(&sAll));
		ph7_result_int64(pCtx,(sxi64)SyBlobLength(&sAll));
	}
	SyBlobRelease(&sAll);
	return PH7_OK;
}
/* array|false gzfile(string $filename, bool $use_include_path = false) */
static int PH7_builtin_gzfile(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return ZlibWholeFile(pCtx,nArg,apArg,1);
}
/* int|false readgzfile(string $filename, bool $use_include_path = false) */
static int PH7_builtin_readgzfile(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return ZlibWholeFile(pCtx,nArg,apArg,0);
}
/* ------------------------------------------------------------------ */
/* The zlib.* stream filters                                          */
/* ------------------------------------------------------------------ */
/*
 * php registers ONE filter factory, `zlib.*`, which answers for exactly two
 * names -- anything else under it (`zlib.bogus`, and `zlib.deflate.foo` as
 * well) is php's "Unable to create or locate filter".
 *
 * Its $params are read the way php's are, which is not the way the deflate_init()
 * options are read at all: a bad value here is a WARNING and the default, not a
 * ValueError, and the only value that stops the filter being created is one libz
 * itself refuses (`window` 7 is inside php's accepted -15..47 range and fails in
 * deflateInit2, which is why it reads as "no such filter" while `window` 100 is
 * a warning and a default). `level` may also be given as a bare int instead of
 * an array; anything else that is not an array is "Invalid filter parameter,
 * ignored" -- an explicitly passed NULL included.
 *
 * The flush mode each run uses is the chain's own: an ordinary run is
 * Z_NO_FLUSH (so `fwrite` twice and `fclose` produces ONE deflate stream with
 * no sync markers in it), an fflush() is Z_SYNC_FLUSH, and the close is
 * Z_FINISH.
 */
typedef struct phl_zfilter phl_zfilter;
struct phl_zfilter {
	z_stream z;
	int bInflate;
	int bInit;
	int bDone;     /* Z_STREAM_END seen, or the stream failed */
};
static void ZlibFilterWarn(phl_stream_filter *pFilter,const char *zFmt,sxi64 iVal)
{
	char zMsg[128];
	SyBufferFormat(zMsg,sizeof(zMsg),zFmt,iVal);
	PH7_VmThrowError(pFilter->pVm,pFilter->pVm->pCalleeName,PH7_CTX_WARNING,zMsg);
}
PH7_PRIVATE int PH7_ZlibFilterCreate(phl_stream_filter *pFilter,ph7_value *pParams)
{
	const char *zName = (const char *)SyBlobData(&pFilter->sName);
	int nName = (int)SyBlobLength(&pFilter->sName);
	phl_zfilter *pState;
	int bInflate,iLevel = Z_DEFAULT_COMPRESSION,iMemory = PHL_Z_MEMLEVEL;
	int iWindow = -MAX_WBITS,rc;
	if( nName == (int)sizeof("zlib.deflate")-1 && SyMemcmp(zName,"zlib.deflate",12) == 0 ){
		bInflate = 0;
	}else if( nName == (int)sizeof("zlib.inflate")-1 && SyMemcmp(zName,"zlib.inflate",12) == 0 ){
		bInflate = 1;
	}else{
		return -1;
	}
	if( pParams != 0 ){
		if( ph7_value_is_array(pParams) ){
			ph7_value *pVal;
			/* Only `window` reaches the INFLATE side: php's filter reads
			 * nothing else for it, so a level or a memory level given to
			 * zlib.inflate is ignored without a word -- and so is a $params
			 * that is not an array at all, which the deflate side warns
			 * about. */
			if( !bInflate && (pVal = ph7_array_fetch(pParams,"level",-1)) != 0 ){
				sxi64 i = PH7_ValuePeekInt64(pVal);
				if( i < -1 || i > 9 ){
					ZlibFilterWarn(pFilter,"Invalid compression level specified. (%qd)",i);
				}else{
					iLevel = (int)i;
				}
			}
			if( !bInflate && (pVal = ph7_array_fetch(pParams,"memory",-1)) != 0 ){
				sxi64 i = PH7_ValuePeekInt64(pVal);
				if( i < 1 || i > 9 ){
					ZlibFilterWarn(pFilter,"Invalid parameter given for memory level (%qd)",i);
				}else{
					iMemory = (int)i;
				}
			}
			if( (pVal = ph7_array_fetch(pParams,"window",-1)) != 0 ){
				sxi64 i = PH7_ValuePeekInt64(pVal);
				if( i < -MAX_WBITS || i > MAX_WBITS + 32 ){
					ZlibFilterWarn(pFilter,"Invalid parameter given for window size (%qd)",i);
				}else{
					iWindow = (int)i;
				}
			}
			/* `strategy` and `dictionary` are read by deflate_init()'s options
			 * and by nothing here: php's filter ignores both without a word. */
		}else if( ph7_value_is_int(pParams) ){
			sxi64 i = ph7_value_to_int64(pParams);
			if( !bInflate && (i < -1 || i > 9) ){
				ZlibFilterWarn(pFilter,"Invalid compression level specified. (%qd)",i);
			}else if( !bInflate ){
				iLevel = (int)i;
			}
		}else if( !bInflate ){
			PH7_VmThrowError(pFilter->pVm,pFilter->pVm->pCalleeName,PH7_CTX_WARNING,
				"Invalid filter parameter, ignored");
		}
	}
	pState = (phl_zfilter *)SyMemBackendAlloc(&pFilter->pVm->sAllocator,sizeof(phl_zfilter));
	if( pState == 0 ){
		return -1;
	}
	SyZero(pState,sizeof(*pState));
	pState->bInflate = bInflate;
	rc = bInflate
		? inflateInit2(&pState->z,iWindow)
		: deflateInit2(&pState->z,iLevel,Z_DEFLATED,iWindow,iMemory,Z_DEFAULT_STRATEGY);
	if( rc != Z_OK ){
		/* libz refused the window: php's filter simply is not created. */
		SyMemBackendFree(&pFilter->pVm->sAllocator,pState);
		return -1;
	}
	pState->bInit = 1;
	pFilter->pPriv = (void *)pState;
	return PH7_OK;
}
PH7_PRIVATE void PH7_ZlibFilterClose(phl_stream_filter *pFilter)
{
	phl_zfilter *pState = (phl_zfilter *)pFilter->pPriv;
	if( pState == 0 ){
		return;
	}
	if( pState->bInit ){
		if( pState->bInflate ){
			inflateEnd(&pState->z);
		}else{
			deflateEnd(&pState->z);
		}
	}
	SyMemBackendFree(&pFilter->pVm->sAllocator,pState);
	pFilter->pPriv = 0;
}
PH7_PRIVATE int PH7_ZlibFilterRun(phl_stream_filter *pFilter,phl_brigade *pIn,
	phl_brigade *pOut,int iFlags)
{
	phl_zfilter *pState = (phl_zfilter *)pFilter->pPriv;
	phl_bucket *pBucket;
	SyBlob sIn,sOut;
	unsigned char zBuf[PHL_Z_CHUNK];
	int iFlush,rc = Z_OK;
	if( pState == 0 ){
		return PHL_PSFS_ERR_FATAL;
	}
	iFlush = iFlags == PHL_PSFS_FLAG_FLUSH_CLOSE ? Z_FINISH
	       : (iFlags == PHL_PSFS_FLAG_FLUSH_INC ? Z_SYNC_FLUSH : Z_NO_FLUSH);
	SyBlobInit(&sIn,&pFilter->pVm->sAllocator);
	SyBlobInit(&sOut,&pFilter->pVm->sAllocator);
	while( (pBucket = PH7_FilterBucketPop(pIn)) != 0 ){
		if( SyBlobLength(&pBucket->sData) > 0 ){
			SyBlobAppend(&sIn,SyBlobData(&pBucket->sData),SyBlobLength(&pBucket->sData));
		}
		PH7_FilterBucketFree(pFilter->pVm,pBucket);
	}
	if( pState->bDone ){
		/* Everything after the end of the stream is dropped, which is what libz
		 * does with the bytes behind a finished member. */
		SyBlobRelease(&sIn);
		SyBlobRelease(&sOut);
		return PHL_PSFS_PASS_ON;
	}
	pState->z.next_in = (Bytef *)SyBlobData(&sIn);
	pState->z.avail_in = (uInt)SyBlobLength(&sIn);
	for(;;){
		pState->z.next_out = (Bytef *)zBuf;
		pState->z.avail_out = (uInt)sizeof(zBuf);
		rc = pState->bInflate
			? inflate(&pState->z,iFlush)
			: deflate(&pState->z,iFlush);
		if( sizeof(zBuf) - pState->z.avail_out > 0 ){
			SyBlobAppend(&sOut,zBuf,(sxu32)(sizeof(zBuf) - pState->z.avail_out));
		}
		if( rc == Z_STREAM_END ){
			pState->bDone = 1;
			break;
		}
		if( rc != Z_OK && rc != Z_BUF_ERROR ){
			/* php's own notice, raised from whichever reader asked. */
			char zMsg[64];
			SyBufferFormat(zMsg,sizeof(zMsg),"zlib: %s",zError(rc));
			PH7_VmThrowError(pFilter->pVm,pFilter->pVm->pCalleeName,PH7_CTX_NOTICE,zMsg);
			pState->bDone = 1;
			SyBlobRelease(&sIn);
			SyBlobRelease(&sOut);
			return PHL_PSFS_ERR_FATAL;
		}
		if( pState->z.avail_out != 0 ){
			/* Room left in the buffer: libz has nothing more for this run. */
			if( pState->z.avail_in == 0 && iFlush == Z_NO_FLUSH ){
				break;
			}
			if( rc == Z_BUF_ERROR ){
				break;
			}
			if( iFlush != Z_NO_FLUSH && pState->z.avail_in == 0 ){
				break;
			}
		}
	}
	if( SyBlobLength(&sOut) > 0 ){
		PH7_FilterBucketAppend(pOut,
			PH7_FilterBucketNew(pFilter->pVm,SyBlobData(&sOut),SyBlobLength(&sOut)));
	}
	SyBlobRelease(&sIn);
	SyBlobRelease(&sOut);
	return PHL_PSFS_PASS_ON;
}
/* ------------------------------------------------------------------ */
/* The output-compression pair                                        */
/* ------------------------------------------------------------------ */
/*
 * Both of these answer from the REQUEST: php compresses a response only when
 * the client said it would take one, so on a command line -- where there is no
 * Accept-Encoding at all -- ob_gzhandler() answers false and
 * zlib_get_coding_type() answers false as well. Under `phl -S` the header is
 * there and the pair behaves as php's does.
 */
static int ZlibAcceptedEncoding(ph7_vm *pVm)
{
	ph7_value *pServer,*pVal;
	const char *zHdr;
	int nHdr = 0;
	pServer = PH7_VmExtractSuper(pVm,"_SERVER",sizeof("_SERVER")-1);
	if( pServer == 0 || !ph7_value_is_array(pServer) ){
		return 0;
	}
	pVal = ph7_array_fetch(pServer,"HTTP_ACCEPT_ENCODING",
		sizeof("HTTP_ACCEPT_ENCODING")-1);
	if( pVal == 0 ){
		return 0;
	}
	zHdr = ph7_value_to_string(pVal,&nHdr);
	if( nHdr < 1 ){
		return 0;
	}
	/* php reads the two names in this order and ignores every q-value. */
	if( SyBlobSearch(zHdr,(sxu32)nHdr,"gzip",sizeof("gzip")-1,0) == SXRET_OK ){
		return PHL_Z_GZIP;
	}
	if( SyBlobSearch(zHdr,(sxu32)nHdr,"deflate",sizeof("deflate")-1,0) == SXRET_OK ){
		return PHL_Z_DEFLATE;
	}
	return 0;
}
/* string|false ob_gzhandler(string $data, int $flags) */
static int PH7_builtin_ob_gzhandler(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int iEnc = ZlibAcceptedEncoding(pCtx->pVm);
	const char *zIn;
	int nIn = 0,iStatus = 0;
	SyBlob sOut;
	if( nArg < 1 || iEnc == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zIn = ph7_value_to_string(apArg[0],&nIn);
	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);
	if( ZlibEncodeBuf(pCtx->pVm,(const unsigned char *)zIn,(sxu32)nIn,iEnc,-1,
			&sOut,&iStatus) != 0 ){
		SyBlobRelease(&sOut);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* php's handler announces what it did, and adds the Vary that makes the
	 * answer cacheable per client. */
	PH7_VmAddResponseHeader(pCtx->pVm,"Content-Encoding",
		iEnc == PHL_Z_GZIP ? "gzip" : "deflate");
	PH7_VmAddResponseHeader(pCtx->pVm,"Vary","Accept-Encoding");
	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));
	SyBlobRelease(&sOut);
	return PH7_OK;
}
/* string|false zlib_get_coding_type() */
static int PH7_builtin_zlib_get_coding_type(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	/* php answers the coding the OUTPUT layer is compressing with, which is
	 * only ever set by zlib.output_compression -- off in this build's ini, as
	 * it is in php's own default. */
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}
/* ------------------------------------------------------------------ */
/* Registration                                                       */
/* ------------------------------------------------------------------ */
/*
 * The two context classes. php declares them final, uncloneable and
 * unserializable, with NO method and NO property, and refuses `new` with a
 * sentence naming the factory -- exactly CurlHandle's shape.
 */
PH7_PRIVATE sxi32 PH7_VmInstallZlib(ph7_vm *pVm)
{
	static const PH7_NativePropDef aProp[] = {
		{ ZCTX_SLOT, PH7_MOD_PRIVATE|PH7_MOD_HIDDEN,
		  { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 }
	};
	static const PH7_NativeClassSpec aSpec[] = {
		{ "DeflateContext", 0, 0,
		  PH7_CLASS_FINAL|PH7_CLASS_NOINSTANTIATE|PH7_CLASS_NOCLONE|PH7_CLASS_NOSERIALIZE,
		  0, 0, 0, 0, aProp, SX_ARRAYSIZE(aProp), ZctxInstanceRelease, 0, 0 },
		{ "InflateContext", 0, 0,
		  PH7_CLASS_FINAL|PH7_CLASS_NOINSTANTIATE|PH7_CLASS_NOCLONE|PH7_CLASS_NOSERIALIZE,
		  0, 0, 0, 0, aProp, SX_ARRAYSIZE(aProp), ZctxInstanceRelease, 0, 0 }
	};
	sxi32 rc;
	pVm->pZlibCtx = 0;
	pVm->iZlibLevel = -1;
	pVm->iZlibStrategy = Z_DEFAULT_STRATEGY;
	rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));
	if( rc == SXRET_OK ){
		ph7_class *pClass = PH7_VmExtractClass(&(*pVm),"DeflateContext",
			sizeof("DeflateContext")-1,FALSE,0);
		if( pClass ){
			pClass->zNewRefusal =
				"Cannot directly construct DeflateContext, use deflate_init() instead";
			pClass->xCmp = PH7_NativeCmpOpaqueHandle;
		}
		pClass = PH7_VmExtractClass(&(*pVm),"InflateContext",
			sizeof("InflateContext")-1,FALSE,0);
		if( pClass ){
			pClass->zNewRefusal =
				"Cannot directly construct InflateContext, use inflate_init() instead";
			pClass->xCmp = PH7_NativeCmpOpaqueHandle;
		}
	}
	return rc;
}
/* The functions this unit owns, in php's own registration order. The gz*
 * handle verbs are NOT here: php registers them as ALIASES of the ordinary
 * stream functions (which is why gzread() works on a plain fopen() handle and
 * fread() works on a gzopen() one), and builtin.c registers them that way. */
PH7_PRIVATE const ph7_builtin_func * PH7_ZlibFuncTable(sxu32 *pnEntry)
{
	static const ph7_builtin_func aFunc[] = {
		{ "ob_gzhandler",          PH7_builtin_ob_gzhandler          },
		{ "zlib_get_coding_type",  PH7_builtin_zlib_get_coding_type  },
		{ "gzfile",                PH7_builtin_gzfile                },
		{ "gzopen",                PH7_builtin_gzopen                },
		{ "readgzfile",            PH7_builtin_readgzfile            },
		{ "zlib_encode",           PH7_builtin_zlib_encode           },
		{ "zlib_decode",           PH7_builtin_zlib_decode           },
		{ "gzdeflate",             PH7_builtin_gzdeflate             },
		{ "gzencode",              PH7_builtin_gzencode              },
		{ "gzcompress",            PH7_builtin_gzcompress            },
		{ "gzinflate",             PH7_builtin_gzinflate             },
		{ "gzdecode",              PH7_builtin_gzdecode              },
		{ "gzuncompress",          PH7_builtin_gzuncompress          },
		{ "deflate_init",          PH7_builtin_deflate_init          },
		{ "deflate_add",           PH7_builtin_deflate_add           },
		{ "inflate_init",          PH7_builtin_inflate_init          },
		{ "inflate_add",           PH7_builtin_inflate_add           },
		{ "inflate_get_status",    PH7_builtin_inflate_get_status    },
		{ "inflate_get_read_len",  PH7_builtin_inflate_get_read_len  }
	};
	*pnEntry = (sxu32)SX_ARRAYSIZE(aFunc);
	return aFunc;
}
#endif /* PH7_ENABLE_ZLIB && !PH7_DISABLE_BUILTIN_FUNC */
