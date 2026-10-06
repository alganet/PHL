# src/ph7/builtin_zlib.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1147/1361 lines (84.28%)

[Root index](../../index.md) | [Directory index](index.md)

| Hits | Line | Source |
| ---: | ---: | :--- |
|    - |    1 | `/**` |
|    - |    2 | ` * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|    - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|    - |    4 | ` */` |
|    - |    5 | `#include "ph7int.h"` |
|    - |    6 | `#if defined(PH7_ENABLE_ZLIB) && !defined(PH7_DISABLE_BUILTIN_FUNC)` |
|    - |    7 | `#include <zlib.h>` |
|    - |    8 | `/*` |
|    - |    9 | ` * Section:` |
|    - |   10 | ` *    php's zlib extension: DEFLATE, the two framings around it, and the stream` |
|    - |   11 | ` *    that reads a .gz file as though it were text.` |
|    - |   12 | ` * Status:` |
|    - |   13 | ` *    Stable.` |
|    - |   14 | ` *` |
|    - |   15 | `` * ext/zlib is the `php shells a C library` shape in its purest form: php's own`` |
|    - |   16 | ` * code here is argument screening, buffer growth and a stream wrapper, and` |
|    - |   17 | ` * every BYTE it answers with comes out of libz. So the contract reproduced` |
|    - |   18 | ` * below is libz's, and this unit links the library rather than deriving it --` |
|    - |   19 | ` * the opposite call from ext/gettext or ext/bcmath, and for the reason those` |
|    - |   20 | ` * two record: a derivation is right when the answer is a RULE (which catalog a` |
|    - |   21 | ` * lookup reads, what 1/3 is to 20 digits) and wrong when the answer is a byte` |
|    - |   22 | ` * stream a specific implementation produces. Nobody can re-derive zlib's` |
|    - |   23 | ` * literal/length decisions, and a program that compresses under php and` |
|    - |   24 | ` * decompresses under PHL needs exactly them.` |
|    - |   25 | ` *` |
|    - |   26 | ` * What that decision costs is worth stating: the compressed bytes are the` |
|    - |   27 | ` * bytes of WHICHEVER libz this build was linked against. php has the same` |
|    - |   28 | ` * property (its own answers move with the system zlib), so the two agree on` |
|    - |   29 | ` * any box where they link the same library and may differ in the compressed` |
|    - |   30 | ` * form -- never in the decompressed one -- where they do not. Nothing in the` |
|    - |   31 | ` * corpus pins a compressed byte for that reason; the tests pin round trips,` |
|    - |   32 | ` * framing headers and the diagnostics.` |
|    - |   33 | ` *` |
|    - |   34 | ` * THE THREE FRAMINGS. One deflate stream, three envelopes, and php exposes the` |
|    - |   35 | `` * choice as an `$encoding`:`` |
|    - |   36 | ` *` |
|    - |   37 | ` *   ZLIB_ENCODING_RAW     (-15)  no header, no checksum   gzdeflate/gzinflate` |
|    - |   38 | ` *   ZLIB_ENCODING_DEFLATE  (15)  RFC 1950 zlib + adler32  gzcompress/gzuncompress` |
|    - |   39 | ` *   ZLIB_ENCODING_GZIP     (31)  RFC 1952 gzip + crc32    gzencode/gzdecode` |
|    - |   40 | ` *` |
|    - |   41 | ` * The numbers are libz's own windowBits spelling (negative = raw, +16 = gzip),` |
|    - |   42 | ` * which is why they are what they are. Each decoder is STRICT about its own` |
|    - |   43 | ` * framing -- gzdecode() refuses a zlib stream, gzinflate() refuses a gzip one` |
|    - |   44 | ` * -- and only zlib_decode() sniffs, because it asks libz for the automatic` |
|    - |   45 | ` * mode (+32) and retries raw when that fails.` |
|    - |   46 | ` *` |
|    - |   47 | ` * THE BUFFER LOOP. php grows its output buffer by an eighth per round and` |
|    - |   48 | `` * stops at $max_length, and that loop is OBSERVABLE: `gzuncompress($c, 22)` on`` |
|    - |   49 | `` * a 23-byte payload is `insufficient memory` and false, while the same call on`` |
|    - |   50 | ` * an 80000-byte payload with $max_length 79999 answers all 80000 bytes -- the` |
|    - |   51 | ` * limit stops the buffer from GROWING again, it does not truncate. The loop` |
|    - |   52 | ` * below is php's, round for round, so both answers come out here too.` |
|    - |   53 | ` *` |
|    - |   54 | `` * THE STREAM. gzopen() and compress.zlib:// are one device (`ZLIB` in php's`` |
|    - |   55 | `` * metadata), and its reader is libz's `gzread` rather than a plain inflate:`` |
|    - |   56 | ` *   - a file that does not start with the gzip magic is passed through BYTE` |
|    - |   57 | `` *     FOR BYTE (php's `gzopen` on a plain text file reads the text), so the`` |
|    - |   58 | ` *     device answers a zlib-framed or raw-deflate file with its compressed` |
|    - |   59 | ` *     bytes -- an asymmetry with gzdecode() that is libz's, and real;` |
|    - |   60 | ` *   - a CONCATENATION of gzip members reads as one stream, which is what makes` |
|    - |   61 | `` *     `gzopen($p,'a')` twice and then `gzfile($p)` answer both writes;`` |
|    - |   62 | ` *   - trailing bytes after the last member are ignored, and a truncated member` |
|    - |   63 | ` *     ends the stream instead of failing it.` |
|    - |   64 | ` * The two doors differ in one visible detail: gzopen() opens the device with no` |
|    - |   65 | `` * wrapper (php reports no `wrapper_type` and no `uri` for it), compress.zlib://`` |
|    - |   66 | ` * goes through one.` |
|    - |   67 | ` *` |
|    - |   68 | `` * WHAT IS NOT HERE. php's `zlib.output_compression` compresses a WEB response`` |
|    - |   69 | ``  * from the ini directive; the three directives are registered so `ini_get()` `` |
|    - |   70 | `` * answers them, and `ob_gzhandler()` is the handler a script installs by hand`` |
|    - |   71 | ` * -- both read the request's Accept-Encoding, so both answer false in a command` |
|    - |   72 | ` * line exactly as php's do. bzip2 is a separate extension in php and is not in` |
|    - |   73 | ``  * this build, which is a build fact rather than a gap: `Phar::getSupportedCompression()` `` |
|    - |   74 | ` * and friends report what is actually here.` |
|    - |   75 | ` */` |
|    - |   76 | `/* php's three $encoding values, spelled as libz windowBits. */` |
|    - |   77 | `#define PHL_Z_RAW      (-15)` |
|    - |   78 | `#define PHL_Z_DEFLATE  (15)` |
|    - |   79 | `#define PHL_Z_GZIP     (31)` |
|    - |   80 | `/* zlib_decode()'s "work it out": libz's automatic zlib/gzip detection. */` |
|    - |   81 | `#define PHL_Z_ANY      (15+32)` |
|    - |   82 | `/* php's own memLevel for every one-shot encode. */` |
|    - |   83 | `#define PHL_Z_MEMLEVEL 8` |
|    - |   84 | `/* How much compressed input the stream device buffers per refill. */` |
|    - |   85 | `#define PHL_Z_CHUNK    8192` |
|    - |   86 |  |
|    - |   87 | `/* ------------------------------------------------------------------ */` |
|    - |   88 | `/* Argument screens                                                   */` |
|    - |   89 | `/* ------------------------------------------------------------------ */` |
|    - |   90 | `/*` |
|    - |   91 | ` * php names the three encodings in one sentence wherever it refuses one, and` |
|    - |   92 | ` * the sentence lists them in this order whatever the function is.` |
|    - |   93 | ` */` |
|  128 |   94 | `static int ZlibScreenEncoding(ph7_context *pCtx,sxi64 iEnc,int iArg,const char *zParam)` |
|    4 |   95 | `{` |
|  132 |   96 | `	if( iEnc == PHL_Z_RAW \|\| iEnc == PHL_Z_DEFLATE \|\| iEnc == PHL_Z_GZIP ){` |
|  126 |   97 | `		return 0;` |
|    - |   98 | `	}` |
|   10 |   99 | `	PH7_VmThrowException(pCtx,"ValueError",` |
|    - |  100 | `		"%s(): Argument #%d ($%s) must be one of ZLIB_ENCODING_RAW, "` |
|    - |  101 | `		"ZLIB_ENCODING_GZIP, or ZLIB_ENCODING_DEFLATE",` |
|    3 |  102 | `		ph7_function_name(pCtx),iArg,zParam);` |
|    7 |  103 | `	return -1;` |
|   68 |  104 | `}` |
|   90 |  105 | `static int ZlibScreenLevel(ph7_context *pCtx,sxi64 iLevel,int iArg,const char *zParam)` |
|    3 |  106 | `{` |
|   93 |  107 | `	if( iLevel >= -1 && iLevel <= 9 ){` |
|   89 |  108 | `		return 0;` |
|    - |  109 | `	}` |
|    7 |  110 | `	PH7_VmThrowException(pCtx,"ValueError",` |
|    - |  111 | `		"%s(): Argument #%d ($%s) must be between -1 and 9",` |
|    2 |  112 | `		ph7_function_name(pCtx),iArg,zParam);` |
|    5 |  113 | `	return -1;` |
|   48 |  114 | `}` |
|    - |  115 | `/* ------------------------------------------------------------------ */` |
|    - |  116 | `/* One-shot encode / decode                                           */` |
|    - |  117 | `/* ------------------------------------------------------------------ */` |
|    - |  118 | `/*` |
|    - |  119 | ` * php's encoder: deflateBound() sizes the output in ONE go and a single` |
|    - |  120 | ` * Z_FINISH fills it, so the answer is whatever libz emits for (level,` |
|    - |  121 | ` * encoding, memLevel 8, default strategy) -- including the gzip header's XFL` |
|    - |  122 | ` * byte, which libz sets from the level (0x04 fastest, 0x02 best, 0x00` |
|    - |  123 | ` * otherwise) and its OS byte, which is the one libz was compiled for.` |
|    - |  124 | ` */` |
|   84 |  125 | `static int ZlibEncodeBuf(ph7_vm *pVm,const unsigned char *zIn,sxu32 nIn,` |
|    - |  126 | `	int iEnc,int iLevel,SyBlob *pOut,int *piStatus)` |
|    3 |  127 | `{` |
|    - |  128 | `	z_stream z;` |
|    - |  129 | `	unsigned char *zBuf;` |
|    - |  130 | `	uLong nBound;` |
|    - |  131 | `	int rc;` |
|   87 |  132 | `	SyZero(&z,sizeof(z));` |
|   87 |  133 | `	rc = deflateInit2(&z,iLevel,Z_DEFLATED,iEnc,PHL_Z_MEMLEVEL,Z_DEFAULT_STRATEGY);` |
|   87 |  134 | `	if( rc != Z_OK ){` |
|  ! 0 |  135 | `		*piStatus = rc;` |
|  ! 0 |  136 | `		return -1;` |
|    - |  137 | `	}` |
|   87 |  138 | `	z.next_in = (Bytef *)zIn;` |
|   87 |  139 | `	z.avail_in = (uInt)nIn;` |
|   87 |  140 | `	nBound = deflateBound(&z,(uLong)nIn);` |
|    - |  141 | `	/* deflateBound() does not account for a gzip header carrying no name or` |
|    - |  142 | `	 * comment; php pads the same way libz's own examples do. */` |
|   87 |  143 | `	nBound += 32;` |
|   87 |  144 | `	zBuf = (unsigned char *)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)nBound);` |
|   87 |  145 | `	if( zBuf == 0 ){` |
|  ! 0 |  146 | `		deflateEnd(&z);` |
|  ! 0 |  147 | `		*piStatus = Z_MEM_ERROR;` |
|  ! 0 |  148 | `		return -1;` |
|    - |  149 | `	}` |
|   87 |  150 | `	z.next_out = (Bytef *)zBuf;` |
|   87 |  151 | `	z.avail_out = (uInt)nBound;` |
|   87 |  152 | `	rc = deflate(&z,Z_FINISH);` |
|   87 |  153 | `	if( rc != Z_STREAM_END ){` |
|  ! 0 |  154 | `		SyMemBackendFree(&pVm->sAllocator,zBuf);` |
|  ! 0 |  155 | `		deflateEnd(&z);` |
|  ! 0 |  156 | `		*piStatus = rc == Z_OK ? Z_BUF_ERROR : rc;` |
|  ! 0 |  157 | `		return -1;` |
|    - |  158 | `	}` |
|   87 |  159 | `	SyBlobAppend(pOut,zBuf,(sxu32)(nBound - z.avail_out));` |
|   87 |  160 | `	SyMemBackendFree(&pVm->sAllocator,zBuf);` |
|   87 |  161 | `	deflateEnd(&z);` |
|   87 |  162 | `	*piStatus = Z_OK;` |
|   87 |  163 | `	return 0;` |
|   45 |  164 | `}` |
|    - |  165 | `/*` |
|    - |  166 | `` * php's decoder, round for round (`php_zlib_inflate_rounds`). The buffer`` |
|    - |  167 | ` * starts at the input length (or at $max_length when that is smaller), grows` |
|    - |  168 | ` * by an eighth plus one each round, and the limit is a stop on the GROWTH` |
|    - |  169 | ` * rather than a truncation -- which is why a payload that finishes inside the` |
|    - |  170 | ` * round where the limit is reached comes back whole and one that does not is` |
|    - |  171 | `` * `insufficient memory`. 100 rounds is php's own runaway guard.`` |
|    - |  172 | ` */` |
|   74 |  173 | `static int ZlibDecodeBuf(SyMemBackend *pAlloc,const unsigned char *zIn,sxu32 nIn,` |
|    - |  174 | `	int iEnc,sxu32 nMax,SyBlob *pOut,int *piStatus)` |
|    2 |  175 | `{` |
|    - |  176 | `	z_stream z;` |
|   76 |  177 | `	unsigned char *zBuf = 0,*zNew;` |
|   76 |  178 | `	sxu32 nSize,nUsed = 0;` |
|   76 |  179 | `	int rc,iRound = 0;` |
|   76 |  180 | `	SyZero(&z,sizeof(z));` |
|   76 |  181 | `	rc = inflateInit2(&z,iEnc);` |
|   76 |  182 | `	if( rc != Z_OK ){` |
|  ! 0 |  183 | `		*piStatus = rc;` |
|  ! 0 |  184 | `		return -1;` |
|    - |  185 | `	}` |
|   76 |  186 | `	z.next_in = (Bytef *)zIn;` |
|   76 |  187 | `	z.avail_in = (uInt)nIn;` |
|    - |  188 | `	/* php's first buffer is four times the input (a decent guess at the` |
|    - |  189 | `	 * expansion ratio) and QUADRUPLES each round; $max_length caps only that` |
|    - |  190 | `	 * first one. The pair of answers this produces is the observable part:` |
|    - |  191 | `	 * a 23-byte payload with $max_length 22 fills the capped buffer, comes` |
|    - |  192 | ``	 * back for a second round and dies on the `max <= used` test above with`` |
|    - |  193 | ``	 * php's `insufficient memory`, while an 80000-byte one with $max_length`` |
|    - |  194 | `	 * 79999 never reaches a round where used has caught up with the limit and` |
|    - |  195 | `	 * answers all 80000 bytes. */` |
|   76 |  196 | `	nSize = nIn > 0 ? nIn * 4 : 1;` |
|   76 |  197 | `	if( nMax && nMax < nSize ){` |
|    9 |  198 | `		nSize = nMax;` |
|    4 |  199 | `	}` |
|  340 |  200 | `	for(;;){` |
|  682 |  201 | `		if( nMax && nMax <= nUsed ){` |
|    5 |  202 | `			rc = Z_MEM_ERROR;` |
|    5 |  203 | `			break;` |
|    - |  204 | `		}` |
|  678 |  205 | `		zNew = (unsigned char *)(zBuf` |
|  602 |  206 | `			? SyMemBackendRealloc(pAlloc,zBuf,nSize + 1)` |
|   74 |  207 | `			: SyMemBackendAlloc(pAlloc,nSize + 1));` |
|  678 |  208 | `		if( zNew == 0 ){` |
|  ! 0 |  209 | `			rc = Z_MEM_ERROR;` |
|  ! 0 |  210 | `			break;` |
|    - |  211 | `		}` |
|  678 |  212 | `		zBuf = zNew;` |
|  678 |  213 | `		z.next_out = (Bytef *)zBuf + nUsed;` |
|  678 |  214 | `		z.avail_out = (uInt)(nSize - nUsed);` |
|  678 |  215 | `		rc = inflate(&z,Z_NO_FLUSH);` |
|  678 |  216 | `		nUsed = nSize - z.avail_out;` |
|  678 |  217 | `		nSize = nSize < 0x20000000 ? nSize * 4 : SXU32_HIGH;` |
|  678 |  218 | `		if( rc != Z_BUF_ERROR && !(rc == Z_OK && z.avail_in > 0) ){` |
|   34 |  219 | `			break;` |
|    - |  220 | `		}` |
|  613 |  221 | `		if( ++iRound >= 100 ){` |
|    7 |  222 | `			break;` |
|    - |  223 | `		}` |
|    1 |  224 | `	}` |
|   76 |  225 | `	inflateEnd(&z);` |
|   76 |  226 | `	if( rc == Z_STREAM_END ){` |
|   54 |  227 | `		SyBlobAppend(pOut,zBuf,nUsed);` |
|   54 |  228 | `		SyMemBackendFree(pAlloc,zBuf);` |
|   54 |  229 | `		*piStatus = Z_OK;` |
|   54 |  230 | `		return 0;` |
|    - |  231 | `	}` |
|   23 |  232 | `	if( zBuf ){` |
|   23 |  233 | `		SyMemBackendFree(pAlloc,zBuf);` |
|   11 |  234 | `	}` |
|    - |  235 | ``	/* Anything short of a finished stream is php's `data error`, whatever libz`` |
|    - |  236 | `	 * called it -- a truncated member ends the loop with Z_OK and reads as one` |
|    - |  237 | `	 * under php too. The one status that keeps its own name is the limit. */` |
|   23 |  238 | `	*piStatus = rc == Z_MEM_ERROR ? Z_MEM_ERROR : Z_DATA_ERROR;` |
|   23 |  239 | `	return -1;` |
|   39 |  240 | `}` |
|    - |  241 | `/*` |
|    - |  242 | ` * The body behind gzcompress/gzdeflate/gzencode/zlib_encode. They differ in` |
|    - |  243 | ` * one thing only: which encoding the third argument DEFAULTS to (zlib_encode` |
|    - |  244 | ` * has no default -- its encoding is argument #2 and required).` |
|    - |  245 | ` */` |
|   92 |  246 | `static int ZlibEncodeCommon(ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|    - |  247 | `	int iDefEnc,int bEncFirst)` |
|    3 |  248 | `{` |
|    - |  249 | `	const char *zIn;` |
|   95 |  250 | `	int nIn = 0,iStatus = 0;` |
|   95 |  251 | `	sxi64 iLevel = -1,iEnc = iDefEnc;` |
|    - |  252 | `	SyBlob sOut;` |
|   95 |  253 | `	if( nArg < 1 ){` |
|  ! 0 |  254 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  255 | `		return PH7_OK;` |
|    - |  256 | `	}` |
|   95 |  257 | `	zIn = ph7_value_to_string(apArg[0],&nIn);` |
|   95 |  258 | `	if( bEncFirst ){` |
|    - |  259 | `		/* zlib_encode(string $data, int $encoding, int $level = -1) */` |
|    5 |  260 | `		if( nArg > 1 ){` |
|    5 |  261 | `			iEnc = ph7_value_to_int64(apArg[1]);` |
|    2 |  262 | `		}` |
|    5 |  263 | `		if( ZlibScreenEncoding(pCtx,iEnc,2,"encoding") != 0 ){` |
|    3 |  264 | `			ph7_result_bool(pCtx,0);` |
|    3 |  265 | `			return PH7_OK;` |
|    - |  266 | `		}` |
|    3 |  267 | `		if( nArg > 2 ){` |
|  ! 0 |  268 | `			iLevel = ph7_value_to_int64(apArg[2]);` |
|  ! 0 |  269 | `		}` |
|    3 |  270 | `		if( ZlibScreenLevel(pCtx,iLevel,3,"level") != 0 ){` |
|  ! 0 |  271 | `			ph7_result_bool(pCtx,0);` |
|  ! 0 |  272 | `			return PH7_OK;` |
|    - |  273 | `		}` |
|    2 |  274 | `	}else{` |
|    - |  275 | `		/* gz*(string $data, int $level = -1, int $encoding = <default>) */` |
|   91 |  276 | `		if( nArg > 1 ){` |
|   23 |  277 | `			iLevel = ph7_value_to_int64(apArg[1]);` |
|   11 |  278 | `		}` |
|   91 |  279 | `		if( ZlibScreenLevel(pCtx,iLevel,2,"level") != 0 ){` |
|    5 |  280 | `			ph7_result_bool(pCtx,0);` |
|    5 |  281 | `			return PH7_OK;` |
|    - |  282 | `		}` |
|   87 |  283 | `		if( nArg > 2 ){` |
|    9 |  284 | `			iEnc = ph7_value_to_int64(apArg[2]);` |
|    4 |  285 | `		}` |
|   87 |  286 | `		if( ZlibScreenEncoding(pCtx,iEnc,3,"encoding") != 0 ){` |
|    3 |  287 | `			ph7_result_bool(pCtx,0);` |
|    3 |  288 | `			return PH7_OK;` |
|    - |  289 | `		}` |
|    - |  290 | `	}` |
|   87 |  291 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|  126 |  292 | `	if( ZlibEncodeBuf(pCtx->pVm,(const unsigned char *)zIn,(sxu32)nIn,` |
|   87 |  293 | `			(int)iEnc,(int)iLevel,&sOut,&iStatus) != 0 ){` |
|  ! 0 |  294 | `		SyBlobRelease(&sOut);` |
|  ! 0 |  295 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"%s",zError(iStatus));` |
|  ! 0 |  296 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  297 | `		return PH7_OK;` |
|    - |  298 | `	}` |
|   87 |  299 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|   87 |  300 | `	SyBlobRelease(&sOut);` |
|   87 |  301 | `	return PH7_OK;` |
|   49 |  302 | `}` |
|    - |  303 | `/*` |
|    - |  304 | ` * The body behind gzuncompress/gzinflate/gzdecode/zlib_decode. iEnc 0 is` |
|    - |  305 | ` * zlib_decode()'s "any": libz's automatic mode first (which knows zlib and` |
|    - |  306 | `` * gzip), then a raw retry, which is how `zlib_decode(gzdeflate($s))` works.`` |
|    - |  307 | ` */` |
|   74 |  308 | `static int ZlibDecodeCommon(ph7_context *pCtx,int nArg,ph7_value **apArg,int iEnc)` |
|    2 |  309 | `{` |
|    - |  310 | `	const char *zIn;` |
|   76 |  311 | `	int nIn = 0,iStatus = 0,rc;` |
|   76 |  312 | `	sxi64 iMax = 0;` |
|    - |  313 | `	SyBlob sOut;` |
|   76 |  314 | `	if( nArg < 1 ){` |
|  ! 0 |  315 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  316 | `		return PH7_OK;` |
|    - |  317 | `	}` |
|   76 |  318 | `	zIn = ph7_value_to_string(apArg[0],&nIn);` |
|   76 |  319 | `	if( nArg > 1 ){` |
|   17 |  320 | `		iMax = ph7_value_to_int64(apArg[1]);` |
|   17 |  321 | `		if( iMax < 0 ){` |
|    3 |  322 | `			ph7_result_bool(pCtx,0);` |
|    4 |  323 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|    - |  324 | `				"%s(): Argument #2 ($max_length) must be greater than or equal to 0",` |
|    1 |  325 | `				ph7_function_name(pCtx));` |
|    - |  326 | `		}` |
|    7 |  327 | `	}` |
|   74 |  328 | `	if( iMax > SXU32_HIGH ){` |
|  ! 0 |  329 | `		iMax = SXU32_HIGH;` |
|  ! 0 |  330 | `	}` |
|   74 |  331 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|  110 |  332 | `	rc = ZlibDecodeBuf(&pCtx->pVm->sAllocator,(const unsigned char *)zIn,(sxu32)nIn,` |
|   36 |  333 | `		iEnc ? iEnc : PHL_Z_ANY,(sxu32)iMax,&sOut,&iStatus);` |
|   74 |  334 | `	if( rc != 0 && iEnc == 0 && iStatus != Z_MEM_ERROR ){` |
|    - |  335 | `		/* php's second guess for the sniffing door: a headerless stream. */` |
|    3 |  336 | `		SyBlobRelease(&sOut);` |
|    3 |  337 | `		SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|    4 |  338 | `		rc = ZlibDecodeBuf(&pCtx->pVm->sAllocator,(const unsigned char *)zIn,(sxu32)nIn,` |
|    1 |  339 | `			PHL_Z_RAW,(sxu32)iMax,&sOut,&iStatus);` |
|    1 |  340 | `	}` |
|   74 |  341 | `	if( rc != 0 ){` |
|   21 |  342 | `		SyBlobRelease(&sOut);` |
|   21 |  343 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"%s",zError(iStatus));` |
|   21 |  344 | `		ph7_result_bool(pCtx,0);` |
|   21 |  345 | `		return PH7_OK;` |
|    - |  346 | `	}` |
|   54 |  347 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|   54 |  348 | `	SyBlobRelease(&sOut);` |
|   54 |  349 | `	return PH7_OK;` |
|   39 |  350 | `}` |
|    - |  351 | `/* string\|false gzcompress(string $data, int $level = -1, int $encoding = ZLIB_ENCODING_DEFLATE) */` |
|   36 |  352 | `static int PH7_builtin_gzcompress(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  353 | `{` |
|   38 |  354 | `	return ZlibEncodeCommon(pCtx,nArg,apArg,PHL_Z_DEFLATE,0);` |
|    2 |  355 | `}` |
|    - |  356 | `/* string\|false gzdeflate(string $data, int $level = -1, int $encoding = ZLIB_ENCODING_RAW) */` |
|   22 |  357 | `static int PH7_builtin_gzdeflate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  358 | `{` |
|   23 |  359 | `	return ZlibEncodeCommon(pCtx,nArg,apArg,PHL_Z_RAW,0);` |
|    1 |  360 | `}` |
|    - |  361 | `/* string\|false gzencode(string $data, int $level = -1, int $encoding = ZLIB_ENCODING_GZIP) */` |
|   30 |  362 | `static int PH7_builtin_gzencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    3 |  363 | `{` |
|   33 |  364 | `	return ZlibEncodeCommon(pCtx,nArg,apArg,PHL_Z_GZIP,0);` |
|    3 |  365 | `}` |
|    - |  366 | `/* string\|false zlib_encode(string $data, int $encoding, int $level = -1) */` |
|    4 |  367 | `static int PH7_builtin_zlib_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  368 | `{` |
|    5 |  369 | `	return ZlibEncodeCommon(pCtx,nArg,apArg,PHL_Z_RAW,1);` |
|    1 |  370 | `}` |
|    - |  371 | `/* string\|false gzuncompress(string $data, int $max_length = 0) */` |
|   34 |  372 | `static int PH7_builtin_gzuncompress(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  373 | `{` |
|   36 |  374 | `	return ZlibDecodeCommon(pCtx,nArg,apArg,PHL_Z_DEFLATE);` |
|    2 |  375 | `}` |
|    - |  376 | `/* string\|false gzinflate(string $data, int $max_length = 0) */` |
|   14 |  377 | `static int PH7_builtin_gzinflate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  378 | `{` |
|   16 |  379 | `	return ZlibDecodeCommon(pCtx,nArg,apArg,PHL_Z_RAW);` |
|    2 |  380 | `}` |
|    - |  381 | `/* string\|false gzdecode(string $data, int $max_length = 0) */` |
|   20 |  382 | `static int PH7_builtin_gzdecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  383 | `{` |
|   22 |  384 | `	return ZlibDecodeCommon(pCtx,nArg,apArg,PHL_Z_GZIP);` |
|    2 |  385 | `}` |
|    - |  386 | `/* string\|false zlib_decode(string $data, int $max_length = 0) */` |
|    6 |  387 | `static int PH7_builtin_zlib_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  388 | `{` |
|    7 |  389 | `	return ZlibDecodeCommon(pCtx,nArg,apArg,0);` |
|    1 |  390 | `}` |
|    - |  391 | `/* ------------------------------------------------------------------ */` |
|    - |  392 | `/* deflate_init / inflate_init and their contexts                     */` |
|    - |  393 | `/* ------------------------------------------------------------------ */` |
|    - |  394 | `/*` |
|    - |  395 | ` * A DeflateContext or an InflateContext: php's two opaque handle classes,` |
|    - |  396 | ` * final, uncloneable, unserializable, with no method and no property, whose` |
|    - |  397 | `` * `new` is refused by name. The z_stream lives OUTSIDE the engine's allocator`` |
|    - |  398 | ` * (libz mallocs its own window), so the record is chained on a per-VM registry` |
|    - |  399 | ` * and freed there as well as from the instance's release hook -- the ext/curl` |
|    - |  400 | ` * rule, for the same reason.` |
|    - |  401 | ` */` |
|    - |  402 | `typedef struct phl_zctx phl_zctx;` |
|    - |  403 | `struct phl_zctx {` |
|    - |  404 | `	z_stream z;                    /* libz's own state */` |
|    - |  405 | `	ph7_vm *pVm;` |
|    - |  406 | `	ph7_class_instance *pOwner;    /* the object holding it, or 0 once released */` |
|    - |  407 | `	int bInflate;                  /* 0 deflate, 1 inflate */` |
|    - |  408 | `	int bInit;                     /* z is live */` |
|    - |  409 | `	int iEnc;                      /* the $encoding it was created with */` |
|    - |  410 | `	int iStatus;                   /* inflate_get_status() */` |
|    - |  411 | `	sxu32 nReadLen;                /* inflate_get_read_len() */` |
|    - |  412 | ``	SyBlob sDict;                  /* the `dictionary` option's bytes, kept for the`` |
|    - |  413 | `	                                * Z_NEED_DICT a zlib-framed stream answers with */` |
|    - |  414 | `	phl_zctx *pNext;               /* VM registry chain */` |
|    - |  415 | `};` |
|    - |  416 | `#define ZCTX_SLOT "__res"` |
|    - |  417 |  |
|   44 |  418 | `static void ZctxFree(phl_zctx *pCtx)` |
|    2 |  419 | `{` |
|   46 |  420 | `	SyBlobRelease(&pCtx->sDict);` |
|   46 |  421 | `	if( pCtx->bInit ){` |
|   24 |  422 | `		if( pCtx->bInflate ){` |
|   11 |  423 | `			inflateEnd(&pCtx->z);` |
|    6 |  424 | `		}else{` |
|   14 |  425 | `			deflateEnd(&pCtx->z);` |
|    - |  426 | `		}` |
|   24 |  427 | `		pCtx->bInit = 0;` |
|   11 |  428 | `	}` |
|   46 |  429 | `}` |
|    - |  430 | `/* Free every context this VM still holds. Runs on VM reset (a reused VM must` |
|    - |  431 | ` * not see the previous run's state) and again at release, before the allocator` |
|    - |  432 | ` * holding the shells goes. */` |
| 7011 |  433 | `static void ZctxVmSweep(ph7_vm *pVm)` |
|    5 |  434 | `{` |
| 7016 |  435 | `	phl_zctx *p = (phl_zctx *)pVm->pZlibCtx;` |
| 7038 |  436 | `	while( p ){` |
|   24 |  437 | `		phl_zctx *pNext = p->pNext;` |
|   24 |  438 | `		ZctxFree(p);` |
|   24 |  439 | `		SyMemBackendFree(&pVm->sAllocator,p);` |
|   24 |  440 | `		p = pNext;` |
|    2 |  441 | `	}` |
| 7016 |  442 | `	pVm->pZlibCtx = 0;` |
| 7016 |  443 | `}` |
|   16 |  444 | `PH7_PRIVATE void PH7_ZlibVmReset(ph7_vm *pVm)` |
|  ! 0 |  445 | `{` |
|   16 |  446 | `	ZctxVmSweep(&(*pVm));` |
|   16 |  447 | `}` |
| 6995 |  448 | `PH7_PRIVATE void PH7_ZlibVmRelease(ph7_vm *pVm)` |
|    5 |  449 | `{` |
| 7000 |  450 | `	ZctxVmSweep(&(*pVm));` |
| 7000 |  451 | `}` |
|   22 |  452 | `static phl_zctx * ZctxNew(ph7_vm *pVm,int bInflate)` |
|    2 |  453 | `{` |
|   24 |  454 | `	phl_zctx *p = (phl_zctx *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_zctx));` |
|   24 |  455 | `	if( p == 0 ){` |
|  ! 0 |  456 | `		return 0;` |
|    - |  457 | `	}` |
|   24 |  458 | `	SyZero(p,sizeof(*p));` |
|   24 |  459 | `	p->pVm = pVm;` |
|   24 |  460 | `	p->bInflate = bInflate;` |
|   24 |  461 | `	SyBlobInit(&p->sDict,&pVm->sAllocator);` |
|   24 |  462 | `	p->pNext = (phl_zctx *)pVm->pZlibCtx;` |
|   24 |  463 | `	pVm->pZlibCtx = p;` |
|   24 |  464 | `	return p;` |
|   13 |  465 | `}` |
|    - |  466 | `/* The context an instance holds, or 0. */` |
|   54 |  467 | `static phl_zctx * ZctxOfInstance(ph7_class_instance *pThis)` |
|    2 |  468 | `{` |
|    - |  469 | `	SyString sAttr;` |
|    - |  470 | `	ph7_value *pRes;` |
|   56 |  471 | `	if( pThis == 0 ){` |
|  ! 0 |  472 | `		return 0;` |
|    - |  473 | `	}` |
|   56 |  474 | `	SyStringInitFromBuf(&sAttr,ZCTX_SLOT,sizeof(ZCTX_SLOT)-1);` |
|   56 |  475 | `	pRes = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|   56 |  476 | `	if( pRes == 0 \|\| (pRes->iFlags & MEMOBJ_RES) == 0 ){` |
|  ! 0 |  477 | `		return 0;` |
|    - |  478 | `	}` |
|   56 |  479 | `	return (phl_zctx *)pRes->x.pOther;` |
|   29 |  480 | `}` |
|   22 |  481 | `static int ZctxAttach(ph7_class_instance *pThis,phl_zctx *pCtx)` |
|    2 |  482 | `{` |
|    - |  483 | `	SyString sAttr;` |
|    - |  484 | `	ph7_value *pRes;` |
|   24 |  485 | `	SyStringInitFromBuf(&sAttr,ZCTX_SLOT,sizeof(ZCTX_SLOT)-1);` |
|   24 |  486 | `	pRes = pThis ? PH7_ClassInstanceFetchAttr(pThis,&sAttr) : 0;` |
|   24 |  487 | `	if( pRes == 0 ){` |
|  ! 0 |  488 | `		return -1;` |
|    - |  489 | `	}` |
|   24 |  490 | `	PH7_MemObjRelease(pRes);` |
|   24 |  491 | `	pRes->x.pOther = pCtx;` |
|   24 |  492 | `	MemObjSetType(pRes,MEMOBJ_RES);` |
|   24 |  493 | `	pCtx->pOwner = pThis;` |
|   24 |  494 | `	return 0;` |
|   13 |  495 | `}` |
|    - |  496 | `/* The instance is going: end the libz stream while the slot is still readable.` |
|    - |  497 | ` * The shell stays on the registry, which frees it. */` |
|   22 |  498 | `static void ZctxInstanceRelease(ph7_vm *pVm,ph7_class_instance *pThis)` |
|    2 |  499 | `{` |
|   24 |  500 | `	phl_zctx *p = ZctxOfInstance(pThis);` |
|   11 |  501 | `	SXUNUSED(pVm);` |
|   24 |  502 | `	if( p == 0 \|\| p->pOwner != pThis ){` |
|  ! 0 |  503 | `		return;` |
|    - |  504 | `	}` |
|   24 |  505 | `	ZctxFree(p);` |
|   24 |  506 | `	p->pOwner = 0;` |
|   13 |  507 | `}` |
|    - |  508 | `/*` |
|    - |  509 | ` * deflate_init()'s $options. php reads five keys and IGNORES every other one,` |
|    - |  510 | ` * screens four of them itself with a sentence naming the KEY rather than the` |
|    - |  511 | `` * argument, and hands `window` to a shared helper whose sentence names neither.`` |
|    - |  512 | ` */` |
|    - |  513 | `struct ZlibOpt {` |
|    - |  514 | `	int iLevel;` |
|    - |  515 | `	int iMemory;` |
|    - |  516 | `	int iWindow;` |
|    - |  517 | `	int iStrategy;` |
|    - |  518 | `	const char *zDict;   /* the concatenated dictionary, or 0 */` |
|    - |  519 | `	sxu32 nDict;` |
|    - |  520 | `};` |
|   10 |  521 | `static int ZlibOptBadValue(ph7_context *pCtx,const char *zKey,const char *zWhat)` |
|    1 |  522 | `{` |
|   16 |  523 | `	PH7_VmThrowException(pCtx,"ValueError","%s(): \"%s\" option must %s",` |
|    5 |  524 | `		ph7_function_name(pCtx),zKey,zWhat);` |
|   11 |  525 | `	return -1;` |
|    1 |  526 | `}` |
|    - |  527 | `/*` |
|    - |  528 | ` * php's dictionary option takes a STRING (used whole) or an ARRAY of strings` |
|    - |  529 | ` * (concatenated with a NUL after each), and refuses an empty member or one` |
|    - |  530 | ` * carrying a NUL -- with a sentence about Argument #2, not about the key.` |
|    - |  531 | ` */` |
|    8 |  532 | `static int ZlibOptDictionary(ph7_context *pCtx,ph7_value *pVal,SyBlob *pDict,int *pbSet)` |
|    1 |  533 | `{` |
|    9 |  534 | `	*pbSet = 0;` |
|    9 |  535 | `	if( ph7_value_is_array(pVal) ){` |
|    9 |  536 | `		ph7_hashmap *pMap = (ph7_hashmap *)pVal->x.pOther;` |
|    - |  537 | `		ph7_hashmap_node *pEntry;` |
|    9 |  538 | `		pMap->pCur = pMap->pFirst;` |
|   17 |  539 | `		while( (pEntry = PH7_HashmapGetNextEntry(pMap)) != 0 ){` |
|    - |  540 | `			ph7_value sVal;` |
|    - |  541 | `			const char *zStr;` |
|   13 |  542 | `			int nStr = 0;` |
|   13 |  543 | `			PH7_MemObjInit(pCtx->pVm,&sVal);` |
|   13 |  544 | `			PH7_HashmapExtractNodeValue(pEntry,&sVal,FALSE);` |
|   13 |  545 | `			zStr = ph7_value_to_string(&sVal,&nStr);` |
|   13 |  546 | `			if( nStr < 1 ){` |
|    3 |  547 | `				PH7_MemObjRelease(&sVal);` |
|    4 |  548 | `				PH7_VmThrowException(pCtx,"ValueError",` |
|    - |  549 | `					"%s(): Argument #2 ($options) must not contain empty strings",` |
|    1 |  550 | `					ph7_function_name(pCtx));` |
|    4 |  551 | `				return -1;` |
|    - |  552 | `			}` |
|   11 |  553 | `			if( SyByteFind(zStr,(sxu32)nStr,0,0) == SXRET_OK ){` |
|    3 |  554 | `				PH7_MemObjRelease(&sVal);` |
|    4 |  555 | `				PH7_VmThrowException(pCtx,"ValueError",` |
|    - |  556 | `					"%s(): Argument #2 ($options) must not contain strings with null bytes",` |
|    1 |  557 | `					ph7_function_name(pCtx));` |
|    3 |  558 | `				return -1;` |
|    - |  559 | `			}` |
|    9 |  560 | `			SyBlobAppend(pDict,zStr,(sxu32)nStr);` |
|    9 |  561 | `			SyBlobAppend(pDict,"\0",1);` |
|    9 |  562 | `			PH7_MemObjRelease(&sVal);` |
|    1 |  563 | `		}` |
|    5 |  564 | `		*pbSet = 1;` |
|    5 |  565 | `		return 0;` |
|    - |  566 | `	}` |
|    - |  567 | `	{` |
|    - |  568 | `		const char *zStr;` |
|  ! 0 |  569 | `		int nStr = 0;` |
|  ! 0 |  570 | `		zStr = ph7_value_to_string(pVal,&nStr);` |
|  ! 0 |  571 | `		if( nStr > 0 ){` |
|  ! 0 |  572 | `			SyBlobAppend(pDict,zStr,(sxu32)nStr);` |
|  ! 0 |  573 | `		}` |
|  ! 0 |  574 | `		*pbSet = 1;` |
|    - |  575 | `	}` |
|  ! 0 |  576 | `	return 0;` |
|    5 |  577 | `}` |
|   38 |  578 | `static int ZlibReadOptions(ph7_context *pCtx,ph7_value *pOpt,struct ZlibOpt *pOut,` |
|    - |  579 | `	SyBlob *pDict,int bDeflate)` |
|    2 |  580 | `{` |
|    - |  581 | `	ph7_hashmap *pMap;` |
|    - |  582 | `	ph7_hashmap_node *pEntry;` |
|    - |  583 | `	ph7_value sObjOpt;` |
|   40 |  584 | `	int bDict = 0,bObj = 0,rc = 0;` |
|   40 |  585 | `	pOut->iLevel = -1;` |
|   40 |  586 | `	pOut->iMemory = PHL_Z_MEMLEVEL;` |
|   40 |  587 | `	pOut->iWindow = 15;` |
|   40 |  588 | `	pOut->iStrategy = Z_DEFAULT_STRATEGY;` |
|   40 |  589 | `	pOut->zDict = 0;` |
|   40 |  590 | `	pOut->nDict = 0;` |
|   40 |  591 | `	if( pOpt == 0 ){` |
|   16 |  592 | `		return 0;` |
|    - |  593 | `	}` |
|   25 |  594 | `	if( ph7_value_is_object(pOpt) ){` |
|    - |  595 | ``		/* php's `object\|array $options`: an object is read by its PROPERTIES,`` |
|    - |  596 | `		 * and every screen below then answers exactly as it does for the array` |
|    - |  597 | `		 * with the same keys. */` |
|    5 |  598 | `		PH7_MemObjInit(pCtx->pVm,&sObjOpt);` |
|    5 |  599 | `		if( PH7_MemObjToHashmap(&sObjOpt) != SXRET_OK ){` |
|  ! 0 |  600 | `			PH7_MemObjRelease(&sObjOpt);` |
|  ! 0 |  601 | `			return 0;` |
|    - |  602 | `		}` |
|    6 |  603 | `		if( PH7_ClassInstanceToHashmap((ph7_class_instance *)pOpt->x.pOther,` |
|    7 |  604 | `				(ph7_hashmap *)sObjOpt.x.pOther) != SXRET_OK ){` |
|  ! 0 |  605 | `			PH7_MemObjRelease(&sObjOpt);` |
|  ! 0 |  606 | `			return 0;` |
|    - |  607 | `		}` |
|    5 |  608 | `		pOpt = &sObjOpt;` |
|    5 |  609 | `		bObj = 1;` |
|    2 |  610 | `	}` |
|   25 |  611 | `	if( !ph7_value_is_array(pOpt) ){` |
|  ! 0 |  612 | `		if( bObj ){` |
|  ! 0 |  613 | `			PH7_MemObjRelease(&sObjOpt);` |
|  ! 0 |  614 | `		}` |
|  ! 0 |  615 | `		return 0;` |
|    - |  616 | `	}` |
|   25 |  617 | `	pMap = (ph7_hashmap *)pOpt->x.pOther;` |
|   25 |  618 | `	pMap->pCur = pMap->pFirst;` |
|   33 |  619 | `	while( (pEntry = PH7_HashmapGetNextEntry(pMap)) != 0 ){` |
|    - |  620 | `		ph7_value sKey,sVal;` |
|    - |  621 | `		const char *zKey;` |
|   25 |  622 | `		int nKey = 0;` |
|    - |  623 | `		sxi64 iVal;` |
|   25 |  624 | `		PH7_MemObjInit(pCtx->pVm,&sKey);` |
|   25 |  625 | `		PH7_MemObjInit(pCtx->pVm,&sVal);` |
|   25 |  626 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|   25 |  627 | `		PH7_HashmapExtractNodeValue(pEntry,&sVal,FALSE);` |
|   25 |  628 | `		zKey = ph7_value_to_string(&sKey,&nKey);` |
|    - |  629 | `		/* PEEKED, not converted: ph7_value_to_int64() would retype this copy in` |
|    - |  630 | ``		 * place, and `dictionary` reads the SAME value as a string a few lines`` |
|    - |  631 | `		 * down -- which is how a dictionary silently became the single byte` |
|    - |  632 | `		 * "0" and the compressed bytes stopped matching php's. */` |
|   25 |  633 | `		iVal = PH7_ValuePeekInt64(&sVal);` |
|   25 |  634 | `		rc = 0;` |
|   25 |  635 | `		if( nKey == 5 && SyMemcmp(zKey,"level",5) == 0 ){` |
|    7 |  636 | `			if( iVal < -1 \|\| iVal > 9 ){` |
|    5 |  637 | `				PH7_MemObjRelease(&sKey); PH7_MemObjRelease(&sVal);` |
|    5 |  638 | `				rc = ZlibOptBadValue(pCtx,"level","be between -1 and 9");` |
|   11 |  639 | `				goto done;` |
|    - |  640 | `			}` |
|    3 |  641 | `			pOut->iLevel = (int)iVal;` |
|   20 |  642 | `		}else if( nKey == 6 && SyMemcmp(zKey,"memory",6) == 0 ){` |
|    3 |  643 | `			if( iVal < 1 \|\| iVal > 9 ){` |
|    3 |  644 | `				PH7_MemObjRelease(&sKey); PH7_MemObjRelease(&sVal);` |
|    3 |  645 | `				rc = ZlibOptBadValue(pCtx,"memory","be between 1 and 9");` |
|    3 |  646 | `				goto done;` |
|    - |  647 | `			}` |
|  ! 0 |  648 | `			pOut->iMemory = (int)iVal;` |
|   17 |  649 | `		}else if( nKey == 6 && SyMemcmp(zKey,"window",6) == 0 ){` |
|    5 |  650 | `			if( iVal < 8 \|\| iVal > 15 ){` |
|    5 |  651 | `				PH7_MemObjRelease(&sKey); PH7_MemObjRelease(&sVal);` |
|    5 |  652 | `				if( bDeflate ){` |
|    3 |  653 | `					rc = ZlibOptBadValue(pCtx,"window","be between 8 and 15");` |
|    3 |  654 | `					goto done;` |
|    - |  655 | `				}` |
|    - |  656 | `				/* inflate_init() reaches php's shared helper instead, whose` |
|    - |  657 | `				 * sentence names neither the function nor the argument. */` |
|    4 |  658 | `				PH7_VmThrowException(pCtx,"ValueError",` |
|    1 |  659 | `					"zlib window size (logarithm) (%qd) must be within 8..15",iVal);` |
|    3 |  660 | `				rc = -1;` |
|    3 |  661 | `				goto done;` |
|    - |  662 | `			}` |
|  ! 0 |  663 | `			pOut->iWindow = (int)iVal;` |
|   13 |  664 | `		}else if( nKey == 8 && SyMemcmp(zKey,"strategy",8) == 0 ){` |
|    2 |  665 | `			if( iVal != Z_FILTERED && iVal != Z_HUFFMAN_ONLY && iVal != Z_RLE` |
|    3 |  666 | `			 && iVal != Z_FIXED && iVal != Z_DEFAULT_STRATEGY ){` |
|    3 |  667 | `				PH7_MemObjRelease(&sKey); PH7_MemObjRelease(&sVal);` |
|    3 |  668 | `				rc = ZlibOptBadValue(pCtx,"strategy",` |
|    - |  669 | `					"be one of ZLIB_FILTERED, ZLIB_HUFFMAN_ONLY, ZLIB_RLE, "` |
|    - |  670 | `					"ZLIB_FIXED, or ZLIB_DEFAULT_STRATEGY");` |
|    3 |  671 | `				goto done;` |
|    - |  672 | `			}` |
|  ! 0 |  673 | `			pOut->iStrategy = (int)iVal;` |
|   11 |  674 | `		}else if( nKey == 10 && SyMemcmp(zKey,"dictionary",10) == 0 ){` |
|    9 |  675 | `			if( ZlibOptDictionary(pCtx,&sVal,pDict,&bDict) != 0 ){` |
|    5 |  676 | `				PH7_MemObjRelease(&sKey); PH7_MemObjRelease(&sVal);` |
|    5 |  677 | `				rc = -1;` |
|    5 |  678 | `				goto done;` |
|    - |  679 | `			}` |
|    2 |  680 | `		}` |
|    - |  681 | `		/* Any other key: php reads none of them and complains about none. */` |
|    9 |  682 | `		PH7_MemObjRelease(&sKey);` |
|    9 |  683 | `		PH7_MemObjRelease(&sVal);` |
|    1 |  684 | `	}` |
|   11 |  685 | `	if( bDict ){` |
|    5 |  686 | `		pOut->zDict = (const char *)SyBlobData(pDict);` |
|    5 |  687 | `		pOut->nDict = SyBlobLength(pDict);` |
|    2 |  688 | `	}` |
|    2 |  689 | `done:` |
|   25 |  690 | `	if( bObj ){` |
|    5 |  691 | `		PH7_MemObjRelease(&sObjOpt);` |
|    2 |  692 | `	}` |
|   25 |  693 | `	return rc;` |
|   21 |  694 | `}` |
|    - |  695 | ``/* The windowBits libz wants: the $encoding, retuned by the `window` option. */`` |
|   22 |  696 | `static int ZlibWindowBits(int iEnc,int iWindow)` |
|    2 |  697 | `{` |
|   24 |  698 | `	if( iEnc == PHL_Z_RAW ){` |
|   16 |  699 | `		return -iWindow;` |
|    - |  700 | `	}` |
|    9 |  701 | `	if( iEnc == PHL_Z_GZIP ){` |
|    5 |  702 | `		return iWindow + 16;` |
|    - |  703 | `	}` |
|    5 |  704 | `	return iWindow;` |
|   13 |  705 | `}` |
|   22 |  706 | `static ph7_class_instance * ZlibNewContextObject(ph7_vm *pVm,int bInflate)` |
|    2 |  707 | `{` |
|   24 |  708 | `	const char *zName = bInflate ? "InflateContext" : "DeflateContext";` |
|   35 |  709 | `	ph7_class *pClass = PH7_VmExtractClass(pVm,zName,` |
|   22 |  710 | `		(sxu32)SyStrlen(zName),FALSE,0);` |
|   24 |  711 | `	return pClass ? PH7_NewClassInstance(pVm,pClass) : 0;` |
|    2 |  712 | `}` |
|    - |  713 | `/* DeflateContext\|false deflate_init(int $encoding, array $options = []) */` |
|   28 |  714 | `static int PH7_builtin_deflate_init(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  715 | `{` |
|    - |  716 | `	struct ZlibOpt sOpt;` |
|    - |  717 | `	SyBlob sDict;` |
|    - |  718 | `	phl_zctx *pZ;` |
|    - |  719 | `	ph7_class_instance *pThis;` |
|    - |  720 | `	sxi64 iEnc;` |
|    - |  721 | `	int rc;` |
|   30 |  722 | `	if( nArg < 1 ){` |
|  ! 0 |  723 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  724 | `		return PH7_OK;` |
|    - |  725 | `	}` |
|   30 |  726 | `	iEnc = ph7_value_to_int64(apArg[0]);` |
|   30 |  727 | `	if( ZlibScreenEncoding(pCtx,iEnc,1,"encoding") != 0 ){` |
|    3 |  728 | `		ph7_result_bool(pCtx,0);` |
|    3 |  729 | `		return PH7_OK;` |
|    - |  730 | `	}` |
|   28 |  731 | `	SyBlobInit(&sDict,&pCtx->pVm->sAllocator);` |
|   28 |  732 | `	if( ZlibReadOptions(pCtx,nArg > 1 ? apArg[1] : 0,&sOpt,&sDict,1) != 0 ){` |
|   15 |  733 | `		SyBlobRelease(&sDict);` |
|   15 |  734 | `		ph7_result_bool(pCtx,0);` |
|   15 |  735 | `		return PH7_OK;` |
|    - |  736 | `	}` |
|   14 |  737 | `	pZ = ZctxNew(pCtx->pVm,0);` |
|   14 |  738 | `	if( pZ == 0 ){` |
|  ! 0 |  739 | `		SyBlobRelease(&sDict);` |
|  ! 0 |  740 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  741 | `		return PH7_OK;` |
|    - |  742 | `	}` |
|   14 |  743 | `	pZ->iEnc = (int)iEnc;` |
|   14 |  744 | `	rc = deflateInit2(&pZ->z,sOpt.iLevel,Z_DEFLATED,` |
|    - |  745 | `		ZlibWindowBits((int)iEnc,sOpt.iWindow),sOpt.iMemory,sOpt.iStrategy);` |
|   14 |  746 | `	if( rc != Z_OK ){` |
|  ! 0 |  747 | `		SyBlobRelease(&sDict);` |
|  ! 0 |  748 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"%s",zError(rc));` |
|  ! 0 |  749 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  750 | `		return PH7_OK;` |
|    - |  751 | `	}` |
|   14 |  752 | `	pZ->bInit = 1;` |
|   14 |  753 | `	if( sOpt.zDict && sOpt.nDict > 0 ){` |
|    3 |  754 | `		deflateSetDictionary(&pZ->z,(const Bytef *)sOpt.zDict,(uInt)sOpt.nDict);` |
|    1 |  755 | `	}` |
|   14 |  756 | `	SyBlobRelease(&sDict);` |
|   14 |  757 | `	pThis = ZlibNewContextObject(pCtx->pVm,0);` |
|   14 |  758 | `	if( pThis == 0 \|\| ZctxAttach(pThis,pZ) != 0 ){` |
|  ! 0 |  759 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  760 | `		return PH7_OK;` |
|    - |  761 | `	}` |
|   14 |  762 | `	PH7_NativeResultObject(pCtx,pThis);` |
|   14 |  763 | `	return PH7_OK;` |
|   16 |  764 | `}` |
|    - |  765 | `/* InflateContext\|false inflate_init(int $encoding, array $options = []) */` |
|   12 |  766 | `static int PH7_builtin_inflate_init(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  767 | `{` |
|    - |  768 | `	struct ZlibOpt sOpt;` |
|    - |  769 | `	SyBlob sDict;` |
|    - |  770 | `	phl_zctx *pZ;` |
|    - |  771 | `	ph7_class_instance *pThis;` |
|    - |  772 | `	sxi64 iEnc;` |
|    - |  773 | `	int rc;` |
|   13 |  774 | `	if( nArg < 1 ){` |
|  ! 0 |  775 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  776 | `		return PH7_OK;` |
|    - |  777 | `	}` |
|   13 |  778 | `	iEnc = ph7_value_to_int64(apArg[0]);` |
|   13 |  779 | `	if( ZlibScreenEncoding(pCtx,iEnc,1,"encoding") != 0 ){` |
|  ! 0 |  780 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  781 | `		return PH7_OK;` |
|    - |  782 | `	}` |
|   13 |  783 | `	SyBlobInit(&sDict,&pCtx->pVm->sAllocator);` |
|   13 |  784 | `	if( ZlibReadOptions(pCtx,nArg > 1 ? apArg[1] : 0,&sOpt,&sDict,0) != 0 ){` |
|    3 |  785 | `		SyBlobRelease(&sDict);` |
|    3 |  786 | `		ph7_result_bool(pCtx,0);` |
|    3 |  787 | `		return PH7_OK;` |
|    - |  788 | `	}` |
|   11 |  789 | `	pZ = ZctxNew(pCtx->pVm,1);` |
|   11 |  790 | `	if( pZ == 0 ){` |
|  ! 0 |  791 | `		SyBlobRelease(&sDict);` |
|  ! 0 |  792 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  793 | `		return PH7_OK;` |
|    - |  794 | `	}` |
|   11 |  795 | `	pZ->iEnc = (int)iEnc;` |
|   11 |  796 | `	rc = inflateInit2(&pZ->z,ZlibWindowBits((int)iEnc,sOpt.iWindow));` |
|   11 |  797 | `	if( rc != Z_OK ){` |
|  ! 0 |  798 | `		SyBlobRelease(&sDict);` |
|  ! 0 |  799 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"%s",zError(rc));` |
|  ! 0 |  800 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  801 | `		return PH7_OK;` |
|    - |  802 | `	}` |
|   11 |  803 | `	pZ->bInit = 1;` |
|   11 |  804 | `	if( sOpt.zDict && sOpt.nDict > 0 ){` |
|    3 |  805 | `		if( (int)iEnc == PHL_Z_RAW ){` |
|    - |  806 | `			/* A raw stream has no place to announce a dictionary, so libz takes` |
|    - |  807 | `			 * it up front. */` |
|  ! 0 |  808 | `			inflateSetDictionary(&pZ->z,(const Bytef *)sOpt.zDict,(uInt)sOpt.nDict);` |
|  ! 0 |  809 | `		}else{` |
|    - |  810 | `			/* A zlib-framed one ASKS, through Z_NEED_DICT, part way into the` |
|    - |  811 | `			 * first inflate -- so the bytes have to outlive this call. */` |
|    3 |  812 | `			SyBlobAppend(&pZ->sDict,sOpt.zDict,sOpt.nDict);` |
|    - |  813 | `		}` |
|    1 |  814 | `	}` |
|   11 |  815 | `	SyBlobRelease(&sDict);` |
|   11 |  816 | `	pThis = ZlibNewContextObject(pCtx->pVm,1);` |
|   11 |  817 | `	if( pThis == 0 \|\| ZctxAttach(pThis,pZ) != 0 ){` |
|  ! 0 |  818 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  819 | `		return PH7_OK;` |
|    - |  820 | `	}` |
|   11 |  821 | `	PH7_NativeResultObject(pCtx,pThis);` |
|   11 |  822 | `	return PH7_OK;` |
|    7 |  823 | `}` |
|    - |  824 | `/* The context an argument names, with php's TypeError for anything else. */` |
|   32 |  825 | `static phl_zctx * ZctxFromArg(ph7_context *pCtx,ph7_value *pArg,int bInflate)` |
|    1 |  826 | `{` |
|    - |  827 | `	ph7_class_instance *pThis;` |
|    - |  828 | `	phl_zctx *pZ;` |
|   33 |  829 | `	if( pArg == 0 \|\| !ph7_value_is_object(pArg) ){` |
|  ! 0 |  830 | `		return 0;` |
|    - |  831 | `	}` |
|   33 |  832 | `	pThis = (ph7_class_instance *)pArg->x.pOther;` |
|   33 |  833 | `	pZ = ZctxOfInstance(pThis);` |
|   33 |  834 | `	if( pZ == 0 \|\| pZ->bInflate != bInflate \|\| !pZ->bInit ){` |
|  ! 0 |  835 | `		return 0;` |
|    - |  836 | `	}` |
|   16 |  837 | `	SXUNUSED(pCtx);` |
|   33 |  838 | `	return pZ;` |
|   17 |  839 | `}` |
|   22 |  840 | `static int ZlibScreenFlush(ph7_context *pCtx,sxi64 iFlush)` |
|    1 |  841 | `{` |
|   22 |  842 | `	if( iFlush == Z_NO_FLUSH \|\| iFlush == Z_PARTIAL_FLUSH \|\| iFlush == Z_SYNC_FLUSH` |
|   18 |  843 | `	 \|\| iFlush == Z_FULL_FLUSH \|\| iFlush == Z_BLOCK \|\| iFlush == Z_FINISH ){` |
|   21 |  844 | `		return 0;` |
|    - |  845 | `	}` |
|    4 |  846 | `	PH7_VmThrowException(pCtx,"ValueError",` |
|    - |  847 | `		"%s(): Argument #3 ($flush_mode) must be one of ZLIB_NO_FLUSH, "` |
|    - |  848 | `		"ZLIB_PARTIAL_FLUSH, ZLIB_SYNC_FLUSH, ZLIB_FULL_FLUSH, ZLIB_BLOCK, "` |
|    1 |  849 | `		"or ZLIB_FINISH",ph7_function_name(pCtx));` |
|    3 |  850 | `	return -1;` |
|   12 |  851 | `}` |
|    - |  852 | `/* string\|false deflate_add(DeflateContext $context, string $data, int $flush_mode = ZLIB_SYNC_FLUSH) */` |
|    8 |  853 | `static int PH7_builtin_deflate_add(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  854 | `{` |
|    - |  855 | `	phl_zctx *pZ;` |
|    - |  856 | `	const char *zIn;` |
|    9 |  857 | `	int nIn = 0,rc;` |
|    9 |  858 | `	sxi64 iFlush = Z_SYNC_FLUSH;` |
|    - |  859 | `	SyBlob sOut;` |
|    - |  860 | `	unsigned char zBuf[PHL_Z_CHUNK];` |
|    9 |  861 | `	if( nArg < 2 ){` |
|  ! 0 |  862 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  863 | `		return PH7_OK;` |
|    - |  864 | `	}` |
|    9 |  865 | `	pZ = ZctxFromArg(pCtx,apArg[0],0);` |
|    9 |  866 | `	if( pZ == 0 ){` |
|  ! 0 |  867 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  868 | `		return PH7_OK;` |
|    - |  869 | `	}` |
|    9 |  870 | `	zIn = ph7_value_to_string(apArg[1],&nIn);` |
|    9 |  871 | `	if( nArg > 2 ){` |
|    9 |  872 | `		iFlush = ph7_value_to_int64(apArg[2]);` |
|    4 |  873 | `	}` |
|    9 |  874 | `	if( ZlibScreenFlush(pCtx,iFlush) != 0 ){` |
|    3 |  875 | `		ph7_result_bool(pCtx,0);` |
|    3 |  876 | `		return PH7_OK;` |
|    - |  877 | `	}` |
|    7 |  878 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|    7 |  879 | `	pZ->z.next_in = (Bytef *)zIn;` |
|    7 |  880 | `	pZ->z.avail_in = (uInt)nIn;` |
|    3 |  881 | `	do {` |
|    7 |  882 | `		pZ->z.next_out = (Bytef *)zBuf;` |
|    7 |  883 | `		pZ->z.avail_out = (uInt)sizeof(zBuf);` |
|    7 |  884 | `		rc = deflate(&pZ->z,(int)iFlush);` |
|    7 |  885 | `		if( rc != Z_OK && rc != Z_STREAM_END && rc != Z_BUF_ERROR ){` |
|  ! 0 |  886 | `			SyBlobRelease(&sOut);` |
|  ! 0 |  887 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"%s",zError(rc));` |
|  ! 0 |  888 | `			ph7_result_bool(pCtx,0);` |
|  ! 0 |  889 | `			return PH7_OK;` |
|    - |  890 | `		}` |
|    7 |  891 | `		SyBlobAppend(&sOut,zBuf,(sxu32)(sizeof(zBuf) - pZ->z.avail_out));` |
|    7 |  892 | `	} while( pZ->z.avail_out == 0 );` |
|    7 |  893 | `	if( iFlush == Z_FINISH ){` |
|    - |  894 | `		/* php restarts the stream, so the context can be used again -- which is` |
|    - |  895 | `		 * why a second deflate_add(..., ZLIB_FINISH) answers a fresh member` |
|    - |  896 | `		 * rather than an error. */` |
|    5 |  897 | `		deflateReset(&pZ->z);` |
|    2 |  898 | `	}` |
|    7 |  899 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|    7 |  900 | `	SyBlobRelease(&sOut);` |
|    7 |  901 | `	return PH7_OK;` |
|    5 |  902 | `}` |
|    - |  903 | `/* string\|false inflate_add(InflateContext $context, string $data, int $flush_mode = ZLIB_SYNC_FLUSH) */` |
|   14 |  904 | `static int PH7_builtin_inflate_add(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  905 | `{` |
|    - |  906 | `	phl_zctx *pZ;` |
|    - |  907 | `	const char *zIn;` |
|   15 |  908 | `	int nIn = 0,rc = Z_OK;` |
|   15 |  909 | `	sxi64 iFlush = Z_SYNC_FLUSH;` |
|    - |  910 | `	SyBlob sOut;` |
|    - |  911 | `	unsigned char zBuf[PHL_Z_CHUNK];` |
|   15 |  912 | `	if( nArg < 2 ){` |
|  ! 0 |  913 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  914 | `		return PH7_OK;` |
|    - |  915 | `	}` |
|   15 |  916 | `	pZ = ZctxFromArg(pCtx,apArg[0],1);` |
|   15 |  917 | `	if( pZ == 0 ){` |
|  ! 0 |  918 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  919 | `		return PH7_OK;` |
|    - |  920 | `	}` |
|   15 |  921 | `	zIn = ph7_value_to_string(apArg[1],&nIn);` |
|   15 |  922 | `	if( nArg > 2 ){` |
|    9 |  923 | `		iFlush = ph7_value_to_int64(apArg[2]);` |
|    4 |  924 | `	}` |
|   15 |  925 | `	if( ZlibScreenFlush(pCtx,iFlush) != 0 ){` |
|  ! 0 |  926 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  927 | `		return PH7_OK;` |
|    - |  928 | `	}` |
|   15 |  929 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|   15 |  930 | `	pZ->z.next_in = (Bytef *)zIn;` |
|   15 |  931 | `	pZ->z.avail_in = (uInt)nIn;` |
|   17 |  932 | `	while( nIn > 0 ){` |
|   15 |  933 | `		pZ->z.next_out = (Bytef *)zBuf;` |
|   15 |  934 | `		pZ->z.avail_out = (uInt)sizeof(zBuf);` |
|   15 |  935 | `		rc = inflate(&pZ->z,(int)iFlush);` |
|   15 |  936 | `		SyBlobAppend(&sOut,zBuf,(sxu32)(sizeof(zBuf) - pZ->z.avail_out));` |
|   14 |  937 | `		if( rc == Z_NEED_DICT && SyBlobLength(&pZ->sDict) > 0` |
|    3 |  938 | `		 && inflateSetDictionary(&pZ->z,(const Bytef *)SyBlobData(&pZ->sDict),` |
|    3 |  939 | `				(uInt)SyBlobLength(&pZ->sDict)) == Z_OK ){` |
|    - |  940 | `			/* The stream named a dictionary and inflate_init() was given one:` |
|    - |  941 | `			 * hand it over and carry on where libz stopped. */` |
|    3 |  942 | `			continue;` |
|    - |  943 | `		}` |
|   13 |  944 | `		if( rc == Z_STREAM_END ){` |
|    9 |  945 | `			break;` |
|    - |  946 | `		}` |
|    5 |  947 | `		if( rc != Z_OK && rc != Z_BUF_ERROR ){` |
|    3 |  948 | `			SyBlobRelease(&sOut);` |
|    3 |  949 | `			pZ->iStatus = rc;` |
|    3 |  950 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"%s",zError(rc));` |
|    3 |  951 | `			ph7_result_bool(pCtx,0);` |
|    3 |  952 | `			return PH7_OK;` |
|    - |  953 | `		}` |
|    3 |  954 | `		if( pZ->z.avail_out != 0 \|\| pZ->z.avail_in == 0 ){` |
|    2 |  955 | `			break;` |
|    - |  956 | `		}` |
|  ! 0 |  957 | `	}` |
|   13 |  958 | `	pZ->nReadLen = (sxu32)pZ->z.total_in;` |
|   13 |  959 | `	pZ->iStatus = rc == Z_STREAM_END ? Z_STREAM_END : Z_OK;` |
|   13 |  960 | `	if( rc == Z_STREAM_END ){` |
|    - |  961 | `		/* Same restart as the deflate side: php's context takes another member. */` |
|    9 |  962 | `		inflateReset(&pZ->z);` |
|    4 |  963 | `	}` |
|   13 |  964 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|   13 |  965 | `	SyBlobRelease(&sOut);` |
|   13 |  966 | `	return PH7_OK;` |
|    8 |  967 | `}` |
|    - |  968 | `/* int inflate_get_status(InflateContext $context) */` |
|    6 |  969 | `static int PH7_builtin_inflate_get_status(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  970 | `{` |
|    7 |  971 | `	phl_zctx *pZ = nArg > 0 ? ZctxFromArg(pCtx,apArg[0],1) : 0;` |
|    7 |  972 | `	ph7_result_int(pCtx,pZ ? pZ->iStatus : 0);` |
|    7 |  973 | `	return PH7_OK;` |
|    1 |  974 | `}` |
|    - |  975 | `/* int inflate_get_read_len(InflateContext $context) */` |
|    4 |  976 | `static int PH7_builtin_inflate_get_read_len(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  977 | `{` |
|    5 |  978 | `	phl_zctx *pZ = nArg > 0 ? ZctxFromArg(pCtx,apArg[0],1) : 0;` |
|    5 |  979 | `	ph7_result_int64(pCtx,pZ ? (sxi64)pZ->nReadLen : 0);` |
|    5 |  980 | `	return PH7_OK;` |
|    1 |  981 | `}` |
|    - |  982 | `/* ------------------------------------------------------------------ */` |
|    - |  983 | `/* The ZLIB device: gzopen() and compress.zlib://                      */` |
|    - |  984 | `/* ------------------------------------------------------------------ */` |
|    - |  985 | `/*` |
|    - |  986 | ` * One handle. The compressed side is an ordinary engine stream (so` |
|    - |  987 | ` * compress.zlib:// nests over anything the engine can open) and the` |
|    - |  988 | ` * uncompressed side is this record's own position.` |
|    - |  989 | ` */` |
|    - |  990 | `typedef struct phl_zstream phl_zstream;` |
|    - |  991 | `struct phl_zstream {` |
|    - |  992 | `	ph7_vm *pVm;` |
|    - |  993 | `	io_private *pInner;        /* the compressed stream underneath */` |
|    - |  994 | `	z_stream z;` |
|    - |  995 | `	int bWrite;                /* opened for writing */` |
|    - |  996 | `	int bInit;                 /* z is live */` |
|    - |  997 | `	int bTransparent;          /* reading a stream that is not gzip-framed */` |
|    - |  998 | `	int bEof;                  /* the compressed side is exhausted */` |
|    - |  999 | `	int bDone;                 /* no more members to read */` |
|    - | 1000 | `	int iLevel,iStrategy;      /* what the mode string asked for */` |
|    - | 1001 | `	ph7_int64 iPos;            /* UNCOMPRESSED position, which is what gztell() answers */` |
|    - | 1002 | `	unsigned char zIn[PHL_Z_CHUNK];` |
|    - | 1003 | `	sxu32 nIn,nInPos;          /* compressed bytes buffered / consumed */` |
|    - | 1004 | `	SyBlob sOut;               /* inflated bytes not yet handed to the caller */` |
|    - | 1005 | `	sxu32 nOutPos;` |
|    - | 1006 | `	SyBlob sUri;               /* what to reopen for a backwards seek */` |
|    - | 1007 | `};` |
|    - | 1008 | `/*` |
|    - | 1009 | ` * gzopen()'s mode string carries a compression LEVEL and a strategy letter` |
|    - | 1010 | ` * that the engine's own mode grammar knows nothing about, and xOpen() is handed` |
|    - | 1011 | ` * flags rather than the string. php passes the whole string to libz; here the` |
|    - | 1012 | ` * two extra pieces are armed on the VM by the door that parsed them and read` |
|    - | 1013 | ` * back by the open below. Nothing else can be between the two calls -- an open` |
|    - | 1014 | ` * is not re-entrant on one VM -- and the wrapper door leaves them at libz's` |
|    - | 1015 | ` * defaults.` |
|    - | 1016 | ` */` |
|  176 | 1017 | `PH7_PRIVATE void PH7_ZlibArmOpen(ph7_vm *pVm,int iLevel,int iStrategy)` |
|    2 | 1018 | `{` |
|  178 | 1019 | `	pVm->iZlibLevel = iLevel;` |
|  178 | 1020 | `	pVm->iZlibStrategy = iStrategy;` |
|  178 | 1021 | `}` |
|    - | 1022 | `/* gzopen() and its two whole-file doors, which name a FILE rather than a url. */` |
|   96 | 1023 | `PH7_PRIVATE void PH7_ZlibArmDirect(ph7_vm *pVm,int bDirect)` |
|    2 | 1024 | `{` |
|   98 | 1025 | `	pVm->bZlibDirect = bDirect;` |
|   98 | 1026 | `}` |
|   72 | 1027 | `static void ZStreamEndZ(phl_zstream *pZ)` |
|    2 | 1028 | `{` |
|   74 | 1029 | `	if( pZ->bInit ){` |
|   58 | 1030 | `		if( pZ->bWrite ){` |
|   24 | 1031 | `			deflateEnd(&pZ->z);` |
|   13 | 1032 | `		}else{` |
|   35 | 1033 | `			inflateEnd(&pZ->z);` |
|    - | 1034 | `		}` |
|   58 | 1035 | `		pZ->bInit = 0;` |
|   28 | 1036 | `	}` |
|   74 | 1037 | `}` |
|    - | 1038 | `/* Fill the compressed buffer from the stream underneath. */` |
|   14 | 1039 | `static int ZStreamFill(phl_zstream *pZ)` |
|    1 | 1040 | `{` |
|    - | 1041 | `	ph7_int64 nRead;` |
|   15 | 1042 | `	if( pZ->nInPos < pZ->nIn ){` |
|    7 | 1043 | `		return 0;` |
|    - | 1044 | `	}` |
|    9 | 1045 | `	if( pZ->bEof ){` |
|  ! 0 | 1046 | `		return -1;` |
|    - | 1047 | `	}` |
|    9 | 1048 | `	nRead = PH7_StreamRead(pZ->pInner,pZ->zIn,(ph7_int64)sizeof(pZ->zIn));` |
|    9 | 1049 | `	if( nRead < 1 ){` |
|    9 | 1050 | `		pZ->bEof = 1;` |
|    9 | 1051 | `		pZ->nIn = pZ->nInPos = 0;` |
|    9 | 1052 | `		return -1;` |
|    - | 1053 | `	}` |
|  ! 0 | 1054 | `	pZ->nIn = (sxu32)nRead;` |
|  ! 0 | 1055 | `	pZ->nInPos = 0;` |
|  ! 0 | 1056 | `	return 0;` |
|    8 | 1057 | `}` |
|    - | 1058 | `/* At least nWant bytes buffered, if the stream has them. */` |
|   76 | 1059 | `static int ZStreamPeek(phl_zstream *pZ,sxu32 nWant)` |
|    1 | 1060 | `{` |
|  117 | 1061 | `	while( pZ->nIn - pZ->nInPos < nWant ){` |
|    - | 1062 | `		ph7_int64 nRead;` |
|   73 | 1063 | `		if( pZ->bEof ){` |
|  ! 0 | 1064 | `			return -1;` |
|    - | 1065 | `		}` |
|   73 | 1066 | `		if( pZ->nInPos > 0 ){` |
|   33 | 1067 | `			SyMemcpy(&pZ->zIn[pZ->nInPos],pZ->zIn,pZ->nIn - pZ->nInPos);` |
|   33 | 1068 | `			pZ->nIn -= pZ->nInPos;` |
|   33 | 1069 | `			pZ->nInPos = 0;` |
|   16 | 1070 | `		}` |
|   73 | 1071 | `		if( pZ->nIn >= sizeof(pZ->zIn) ){` |
|  ! 0 | 1072 | `			return 0;` |
|    - | 1073 | `		}` |
|  109 | 1074 | `		nRead = PH7_StreamRead(pZ->pInner,&pZ->zIn[pZ->nIn],` |
|   72 | 1075 | `			(ph7_int64)(sizeof(pZ->zIn) - pZ->nIn));` |
|   73 | 1076 | `		if( nRead < 1 ){` |
|   33 | 1077 | `			pZ->bEof = 1;` |
|   33 | 1078 | `			return -1;` |
|    - | 1079 | `		}` |
|   41 | 1080 | `		pZ->nIn += (sxu32)nRead;` |
|    1 | 1081 | `	}` |
|   45 | 1082 | `	return 0;` |
|   39 | 1083 | `}` |
|    - | 1084 | `/*` |
|    - | 1085 | ` * Start (or restart) the inflate stream at a member boundary. libz parses the` |
|    - | 1086 | ` * gzip header itself with windowBits 16+15; what this decides is whether there` |
|    - | 1087 | ` * IS a member here at all -- two bytes of magic -- which is libz's transparent` |
|    - | 1088 | ` * mode and the reason gzopen() reads a plain text file as text.` |
|    - | 1089 | ` */` |
|   76 | 1090 | `static int ZStreamStartMember(phl_zstream *pZ,int bFirst)` |
|    1 | 1091 | `{` |
|    - | 1092 | `	const unsigned char *z;` |
|   77 | 1093 | `	if( ZStreamPeek(pZ,2) != 0 ){` |
|   33 | 1094 | `		pZ->bDone = 1;` |
|   33 | 1095 | `		return -1;` |
|    - | 1096 | `	}` |
|   45 | 1097 | `	z = &pZ->zIn[pZ->nInPos];` |
|   45 | 1098 | `	if( z[0] != 0x1F \|\| z[1] != 0x8B ){` |
|    7 | 1099 | `		if( bFirst ){` |
|    - | 1100 | `			/* Not compressed at all: the whole stream is passed through. */` |
|    7 | 1101 | `			pZ->bTransparent = 1;` |
|    7 | 1102 | `			return 0;` |
|    - | 1103 | `		}` |
|    - | 1104 | `		/* Bytes after the last member are not a member; libz stops there. */` |
|  ! 0 | 1105 | `		pZ->bDone = 1;` |
|  ! 0 | 1106 | `		return -1;` |
|    - | 1107 | `	}` |
|   39 | 1108 | `	if( pZ->bInit ){` |
|    5 | 1109 | `		if( inflateReset(&pZ->z) != Z_OK ){` |
|  ! 0 | 1110 | `			pZ->bDone = 1;` |
|  ! 0 | 1111 | `			return -1;` |
|    - | 1112 | `		}` |
|    5 | 1113 | `		return 0;` |
|    - | 1114 | `	}` |
|   35 | 1115 | `	SyZero(&pZ->z,sizeof(pZ->z));` |
|   35 | 1116 | `	if( inflateInit2(&pZ->z,PHL_Z_GZIP) != Z_OK ){` |
|  ! 0 | 1117 | `		pZ->bDone = 1;` |
|  ! 0 | 1118 | `		return -1;` |
|    - | 1119 | `	}` |
|   35 | 1120 | `	pZ->bInit = 1;` |
|   35 | 1121 | `	return 0;` |
|   39 | 1122 | `}` |
|    - | 1123 | `/* Inflate one buffer's worth into sOut. Answers 0 when it produced bytes or` |
|    - | 1124 | ` * hit the end, -1 when the stream is finished for good. */` |
|  114 | 1125 | `static int ZStreamPump(phl_zstream *pZ)` |
|    1 | 1126 | `{` |
|    - | 1127 | `	unsigned char zBuf[PHL_Z_CHUNK];` |
|    - | 1128 | `	int rc;` |
|  115 | 1129 | `	if( pZ->bDone ){` |
|   57 | 1130 | `		return -1;` |
|    - | 1131 | `	}` |
|   59 | 1132 | `	if( pZ->bTransparent ){` |
|   13 | 1133 | `		if( ZStreamFill(pZ) != 0 ){` |
|    7 | 1134 | `			pZ->bDone = 1;` |
|    7 | 1135 | `			return -1;` |
|    - | 1136 | `		}` |
|    7 | 1137 | `		SyBlobAppend(&pZ->sOut,&pZ->zIn[pZ->nInPos],pZ->nIn - pZ->nInPos);` |
|    7 | 1138 | `		pZ->nInPos = pZ->nIn;` |
|    7 | 1139 | `		return 0;` |
|    - | 1140 | `	}` |
|   47 | 1141 | `	if( !pZ->bInit && ZStreamStartMember(pZ,pZ->iPos == 0 && SyBlobLength(&pZ->sOut) == 0) != 0 ){` |
|  ! 0 | 1142 | `		return -1;` |
|    - | 1143 | `	}` |
|   47 | 1144 | `	if( pZ->bTransparent ){` |
|    7 | 1145 | `		return ZStreamPump(pZ);` |
|    - | 1146 | `	}` |
|   20 | 1147 | `	for(;;){` |
|   41 | 1148 | `		if( pZ->nInPos >= pZ->nIn && ZStreamFill(pZ) != 0 ){` |
|    - | 1149 | `			/* Truncated member: libz's gzread ends the stream where the bytes` |
|    - | 1150 | `			 * end rather than failing the read. */` |
|    3 | 1151 | `			pZ->bDone = 1;` |
|    3 | 1152 | `			return SyBlobLength(&pZ->sOut) > pZ->nOutPos ? 0 : -1;` |
|    - | 1153 | `		}` |
|   39 | 1154 | `		pZ->z.next_in = (Bytef *)&pZ->zIn[pZ->nInPos];` |
|   39 | 1155 | `		pZ->z.avail_in = (uInt)(pZ->nIn - pZ->nInPos);` |
|   39 | 1156 | `		pZ->z.next_out = (Bytef *)zBuf;` |
|   39 | 1157 | `		pZ->z.avail_out = (uInt)sizeof(zBuf);` |
|   39 | 1158 | `		rc = inflate(&pZ->z,Z_NO_FLUSH);` |
|   39 | 1159 | `		pZ->nInPos = pZ->nIn - pZ->z.avail_in;` |
|   39 | 1160 | `		if( sizeof(zBuf) - pZ->z.avail_out > 0 ){` |
|   39 | 1161 | `			SyBlobAppend(&pZ->sOut,zBuf,(sxu32)(sizeof(zBuf) - pZ->z.avail_out));` |
|   19 | 1162 | `		}` |
|   39 | 1163 | `		if( rc == Z_STREAM_END ){` |
|    - | 1164 | `			/* Another member may follow; anything else ends the stream. */` |
|   37 | 1165 | `			if( ZStreamStartMember(pZ,0) != 0 ){` |
|   33 | 1166 | `				pZ->bDone = 1;` |
|   16 | 1167 | `			}` |
|   37 | 1168 | `			return 0;` |
|    - | 1169 | `		}` |
|    3 | 1170 | `		if( rc != Z_OK ){` |
|  ! 0 | 1171 | `			pZ->bDone = 1;` |
|  ! 0 | 1172 | `			return SyBlobLength(&pZ->sOut) > pZ->nOutPos ? 0 : -1;` |
|    - | 1173 | `		}` |
|    3 | 1174 | `		if( sizeof(zBuf) - pZ->z.avail_out > 0 ){` |
|    3 | 1175 | `			return 0;` |
|    - | 1176 | `		}` |
|  ! 0 | 1177 | `	}` |
|   58 | 1178 | `}` |
|    - | 1179 | `/* Drop what the caller has already taken, so a long read does not keep the` |
|    - | 1180 | ` * whole file. */` |
|  108 | 1181 | `static void ZStreamTrimOut(phl_zstream *pZ)` |
|    1 | 1182 | `{` |
|  109 | 1183 | `	if( pZ->nOutPos > 0 && pZ->nOutPos >= SyBlobLength(&pZ->sOut) ){` |
|   39 | 1184 | `		SyBlobReset(&pZ->sOut);` |
|   39 | 1185 | `		pZ->nOutPos = 0;` |
|   19 | 1186 | `	}` |
|  109 | 1187 | `}` |
|   76 | 1188 | `static ph7_int64 ZStreamRead(void *pHandle,void *pBuffer,ph7_int64 nWant)` |
|    1 | 1189 | `{` |
|   77 | 1190 | `	phl_zstream *pZ = (phl_zstream *)pHandle;` |
|   77 | 1191 | `	sxu32 nCopy = 0;` |
|   77 | 1192 | `	unsigned char *zOut = (unsigned char *)pBuffer;` |
|   77 | 1193 | `	if( pZ == 0 \|\| pZ->bWrite \|\| nWant < 1 ){` |
|    3 | 1194 | `		return pZ && pZ->bWrite ? -1 : 0;` |
|    - | 1195 | `	}` |
|  167 | 1196 | `	while( nCopy < (sxu32)nWant ){` |
|  157 | 1197 | `		sxu32 nHave = SyBlobLength(&pZ->sOut) - pZ->nOutPos;` |
|  157 | 1198 | `		if( nHave == 0 ){` |
|  109 | 1199 | `			ZStreamTrimOut(pZ);` |
|  109 | 1200 | `			if( ZStreamPump(pZ) != 0 ){` |
|   65 | 1201 | `				break;` |
|    - | 1202 | `			}` |
|   45 | 1203 | `			continue;` |
|    - | 1204 | `		}` |
|   49 | 1205 | `		if( nHave > (sxu32)nWant - nCopy ){` |
|   11 | 1206 | `			nHave = (sxu32)nWant - nCopy;` |
|    5 | 1207 | `		}` |
|   49 | 1208 | `		SyMemcpy((const char *)SyBlobData(&pZ->sOut) + pZ->nOutPos,&zOut[nCopy],nHave);` |
|   49 | 1209 | `		pZ->nOutPos += nHave;` |
|   49 | 1210 | `		nCopy += nHave;` |
|    1 | 1211 | `	}` |
|   75 | 1212 | `	pZ->iPos += (ph7_int64)nCopy;` |
|   75 | 1213 | `	return (ph7_int64)nCopy;` |
|   39 | 1214 | `}` |
|   12 | 1215 | `static ph7_int64 ZStreamWrite(void *pHandle,const void *pData,ph7_int64 nLen)` |
|    1 | 1216 | `{` |
|   13 | 1217 | `	phl_zstream *pZ = (phl_zstream *)pHandle;` |
|    - | 1218 | `	unsigned char zBuf[PHL_Z_CHUNK];` |
|    - | 1219 | `	int rc;` |
|   13 | 1220 | `	if( pZ == 0 ){` |
|  ! 0 | 1221 | `		return -1;` |
|    - | 1222 | `	}` |
|   13 | 1223 | `	if( !pZ->bWrite ){` |
|    - | 1224 | `		/* php's gzwrite() on a read handle is a plain 0 -- no bytes, no` |
|    - | 1225 | `		 * diagnostic -- rather than the failed write a -1 would report. */` |
|    3 | 1226 | `		return 0;` |
|    - | 1227 | `	}` |
|   11 | 1228 | `	if( nLen < 1 ){` |
|  ! 0 | 1229 | `		return 0;` |
|    - | 1230 | `	}` |
|    - | 1231 | `	/*` |
|    - | 1232 | `	 * Z_SYNC_FLUSH per write, which is php's and is OBSERVABLE: the file grows` |
|    - | 1233 | `	 * by a sync marker at every gzwrite(), so a gzopen()+gzwrite()+gzclose()` |
|    - | 1234 | `	 * pair is six bytes longer than the same bytes through gzencode(). What it` |
|    - | 1235 | `	 * buys is a file that can be read up to the last completed write even if` |
|    - | 1236 | `	 * the writer never closed it.` |
|    - | 1237 | `	 */` |
|   11 | 1238 | `	pZ->z.next_in = (Bytef *)pData;` |
|   11 | 1239 | `	pZ->z.avail_in = (uInt)nLen;` |
|    5 | 1240 | `	for(;;){` |
|   11 | 1241 | `		pZ->z.next_out = (Bytef *)zBuf;` |
|   11 | 1242 | `		pZ->z.avail_out = (uInt)sizeof(zBuf);` |
|   11 | 1243 | `		rc = deflate(&pZ->z,Z_SYNC_FLUSH);` |
|   11 | 1244 | `		if( rc != Z_OK && rc != Z_BUF_ERROR ){` |
|  ! 0 | 1245 | `			return -1;` |
|    - | 1246 | `		}` |
|   11 | 1247 | `		if( sizeof(zBuf) - pZ->z.avail_out > 0 ){` |
|   16 | 1248 | `			PH7_StreamWrite(pZ->pInner,zBuf,` |
|   10 | 1249 | `				(ph7_int64)(sizeof(zBuf) - pZ->z.avail_out));` |
|    5 | 1250 | `		}` |
|   11 | 1251 | `		if( pZ->z.avail_out != 0 ){` |
|   11 | 1252 | `			break;` |
|    - | 1253 | `		}` |
|  ! 0 | 1254 | `	}` |
|   11 | 1255 | `	pZ->iPos += nLen;` |
|   11 | 1256 | `	return nLen;` |
|    7 | 1257 | `}` |
|    - | 1258 | `/* Finish the compressed side: the last deflate block and libz's trailer. */` |
|   66 | 1259 | `static void ZStreamFinishWrite(phl_zstream *pZ)` |
|    2 | 1260 | `{` |
|    - | 1261 | `	unsigned char zBuf[PHL_Z_CHUNK];` |
|    - | 1262 | `	int rc;` |
|   68 | 1263 | `	if( !pZ->bWrite \|\| !pZ->bInit ){` |
|   46 | 1264 | `		return;` |
|    - | 1265 | `	}` |
|   24 | 1266 | `	pZ->z.next_in = (Bytef *)"";` |
|   24 | 1267 | `	pZ->z.avail_in = 0;` |
|   11 | 1268 | `	do {` |
|   24 | 1269 | `		pZ->z.next_out = (Bytef *)zBuf;` |
|   24 | 1270 | `		pZ->z.avail_out = (uInt)sizeof(zBuf);` |
|   24 | 1271 | `		rc = deflate(&pZ->z,Z_FINISH);` |
|   24 | 1272 | `		if( sizeof(zBuf) - pZ->z.avail_out > 0 ){` |
|   35 | 1273 | `			PH7_StreamWrite(pZ->pInner,zBuf,` |
|   22 | 1274 | `				(ph7_int64)(sizeof(zBuf) - pZ->z.avail_out));` |
|   11 | 1275 | `		}` |
|   24 | 1276 | `	} while( rc == Z_OK );` |
|   35 | 1277 | `}` |
|   66 | 1278 | `static void ZStreamClose(void *pHandle)` |
|    2 | 1279 | `{` |
|   68 | 1280 | `	phl_zstream *pZ = (phl_zstream *)pHandle;` |
|    - | 1281 | `	ph7_vm *pVm;` |
|   68 | 1282 | `	if( pZ == 0 ){` |
|  ! 0 | 1283 | `		return;` |
|    - | 1284 | `	}` |
|   68 | 1285 | `	pVm = pZ->pVm;` |
|   68 | 1286 | `	ZStreamFinishWrite(pZ);` |
|   68 | 1287 | `	ZStreamEndZ(pZ);` |
|   68 | 1288 | `	if( pZ->pInner ){` |
|   68 | 1289 | `		if( pZ->pInner->pStream ){` |
|   68 | 1290 | `			PH7_StreamCloseHandle(pZ->pInner->pStream,pZ->pInner->pHandle);` |
|   33 | 1291 | `		}` |
|   68 | 1292 | `		SyBlobRelease(&pZ->pInner->sBuffer);` |
|   68 | 1293 | `		SyBlobRelease(&pZ->pInner->sFilt);` |
|   68 | 1294 | `		SyBlobRelease(&pZ->pInner->sUri);` |
|   68 | 1295 | `		SyMemBackendFree(&pVm->sAllocator,pZ->pInner);` |
|   68 | 1296 | `		pZ->pInner = 0;` |
|   33 | 1297 | `	}` |
|   68 | 1298 | `	SyBlobRelease(&pZ->sOut);` |
|   68 | 1299 | `	SyBlobRelease(&pZ->sUri);` |
|   68 | 1300 | `	SyMemBackendFree(&pVm->sAllocator,pZ);` |
|   35 | 1301 | `}` |
|    - | 1302 | `/*` |
|    - | 1303 | ` * php's ZLIB handle can be flock()ed: the lock belongs to the descriptor of the` |
|    - | 1304 | ` * file UNDERNEATH, which is where this one sends it. A device with no lock of` |
|    - | 1305 | ` * its own (a nested php:// or data:// stream) answers as it would on its own.` |
|    - | 1306 | ` */` |
|  ! 0 | 1307 | `static int ZStreamLock(void *pHandle,int iType)` |
|  ! 0 | 1308 | `{` |
|  ! 0 | 1309 | `	phl_zstream *pZ = (phl_zstream *)pHandle;` |
|  ! 0 | 1310 | `	if( pZ == 0 \|\| pZ->pInner == 0 \|\| pZ->pInner->pStream == 0` |
|  ! 0 | 1311 | `	 \|\| pZ->pInner->pStream->xLock == 0 ){` |
|  ! 0 | 1312 | `		return PH7_OK;` |
|    - | 1313 | `	}` |
|  ! 0 | 1314 | `	return pZ->pInner->pStream->xLock(pZ->pInner->pHandle,iType);` |
|  ! 0 | 1315 | `}` |
|   38 | 1316 | `static ph7_int64 ZStreamTell(void *pHandle)` |
|    1 | 1317 | `{` |
|   39 | 1318 | `	phl_zstream *pZ = (phl_zstream *)pHandle;` |
|   39 | 1319 | `	return pZ ? pZ->iPos : -1;` |
|    1 | 1320 | `}` |
|    - | 1321 | `/*` |
|    - | 1322 | ` * php's ZLIB stream reports itself SEEKABLE and means it the way libz does:` |
|    - | 1323 | ` * forward by reading and discarding, backwards by starting the file again.` |
|    - | 1324 | ` * SEEK_END is the one php refuses outright, because libz cannot know the` |
|    - | 1325 | ` * uncompressed length without decoding the whole member.` |
|    - | 1326 | ` */` |
|   10 | 1327 | `static int ZStreamSeek(void *pHandle,ph7_int64 iOfft,int whence)` |
|    1 | 1328 | `{` |
|   11 | 1329 | `	phl_zstream *pZ = (phl_zstream *)pHandle;` |
|    - | 1330 | `	ph7_int64 iTarget;` |
|   11 | 1331 | `	if( pZ == 0 ){` |
|  ! 0 | 1332 | `		return -1;` |
|    - | 1333 | `	}` |
|   11 | 1334 | `	if( whence == 2 /* SEEK_END */ ){` |
|    3 | 1335 | `		PH7_VmThrowError(pZ->pVm,pZ->pVm->pCalleeName,PH7_CTX_WARNING,` |
|    - | 1336 | `			"SEEK_END is not supported");` |
|    3 | 1337 | `		return -1;` |
|    - | 1338 | `	}` |
|    9 | 1339 | `	iTarget = whence == 1 /* SEEK_CUR */ ? pZ->iPos + iOfft : iOfft;` |
|    9 | 1340 | `	if( iTarget < 0 ){` |
|  ! 0 | 1341 | `		return -1;` |
|    - | 1342 | `	}` |
|    9 | 1343 | `	if( pZ->bWrite ){` |
|    - | 1344 | `		/* libz pads a forward seek with zeroes and refuses a backwards one. */` |
|  ! 0 | 1345 | `		if( iTarget < pZ->iPos ){` |
|  ! 0 | 1346 | `			return -1;` |
|    - | 1347 | `		}` |
|  ! 0 | 1348 | `		while( pZ->iPos < iTarget ){` |
|    - | 1349 | `			static const unsigned char zZero[256] = { 0 };` |
|  ! 0 | 1350 | `			ph7_int64 nStep = iTarget - pZ->iPos;` |
|  ! 0 | 1351 | `			if( nStep > (ph7_int64)sizeof(zZero) ){` |
|  ! 0 | 1352 | `				nStep = (ph7_int64)sizeof(zZero);` |
|  ! 0 | 1353 | `			}` |
|  ! 0 | 1354 | `			if( ZStreamWrite(pZ,zZero,nStep) != nStep ){` |
|  ! 0 | 1355 | `				return -1;` |
|    - | 1356 | `			}` |
|  ! 0 | 1357 | `		}` |
|  ! 0 | 1358 | `		return 0;` |
|    - | 1359 | `	}` |
|    9 | 1360 | `	if( iTarget < pZ->iPos ){` |
|    - | 1361 | `		/* Start the compressed side again and inflate forward. */` |
|    7 | 1362 | `		if( PH7_StreamSeekWrapped(pZ->pInner,0,0 /* SEEK_SET */) != PH7_OK ){` |
|  ! 0 | 1363 | `			return -1;` |
|    - | 1364 | `		}` |
|    7 | 1365 | `		ZStreamEndZ(pZ);` |
|    7 | 1366 | `		SyBlobReset(&pZ->sOut);` |
|    7 | 1367 | `		pZ->nOutPos = pZ->nIn = pZ->nInPos = 0;` |
|    7 | 1368 | `		pZ->bEof = pZ->bDone = pZ->bTransparent = 0;` |
|    7 | 1369 | `		pZ->iPos = 0;` |
|    3 | 1370 | `	}` |
|   15 | 1371 | `	while( pZ->iPos < iTarget ){` |
|    - | 1372 | `		unsigned char zSkip[PHL_Z_CHUNK];` |
|    9 | 1373 | `		ph7_int64 nStep = iTarget - pZ->iPos;` |
|    - | 1374 | `		ph7_int64 nGot;` |
|    9 | 1375 | `		if( nStep > (ph7_int64)sizeof(zSkip) ){` |
|  ! 0 | 1376 | `			nStep = (ph7_int64)sizeof(zSkip);` |
|  ! 0 | 1377 | `		}` |
|    9 | 1378 | `		nGot = ZStreamRead(pZ,zSkip,nStep);` |
|    9 | 1379 | `		if( nGot < 1 ){` |
|    - | 1380 | `			/* php's gzseek() past the end still MOVES: the position it reports` |
|    - | 1381 | `			 * is the one asked for, and the next read answers "". */` |
|    3 | 1382 | `			pZ->iPos = iTarget;` |
|    3 | 1383 | `			break;` |
|    - | 1384 | `		}` |
|    1 | 1385 | `	}` |
|    9 | 1386 | `	return 0;` |
|    6 | 1387 | `}` |
|    - | 1388 | `/* Nothing under a ZLIB handle has an fstat() answer: php's is a flat false. */` |
|    2 | 1389 | `static int ZStreamStat(void *pHandle,ph7_value *pArray,ph7_value *pWorker)` |
|    1 | 1390 | `{` |
|    1 | 1391 | `	SXUNUSED(pHandle);` |
|    1 | 1392 | `	SXUNUSED(pArray);` |
|    1 | 1393 | `	SXUNUSED(pWorker);` |
|    3 | 1394 | `	return -1;` |
|    1 | 1395 | `}` |
|    - | 1396 | `/*` |
|    - | 1397 | ` * int (*xOpen)(const char *,int,ph7_value *,void **)` |
|    - | 1398 | ` *` |
|    - | 1399 | `` * The name is whatever followed `compress.zlib://` (or the plain path gzopen()`` |
|    - | 1400 | ` * was given), and it is opened through the ordinary device dispatch -- so the` |
|    - | 1401 | ` * inner stream may itself be a wrapper, and a gzip file over http:// or data://` |
|    - | 1402 | ` * reads exactly like one on disk.` |
|    - | 1403 | ` */` |
|  100 | 1404 | `static int ZStreamOpen(const char *zName,int iMode,ph7_value *pResource,void **ppHandle)` |
|    2 | 1405 | `{` |
|    - | 1406 | `	const ph7_io_stream *pInnerDev;` |
|  102 | 1407 | `	ph7_vm *pVm = pResource ? pResource->pVm : 0;` |
|    - | 1408 | `	phl_zstream *pZ;` |
|    - | 1409 | `	io_private *pInner;` |
|  102 | 1410 | `	const char *zPath = zName;` |
|    - | 1411 | `	int iInnerMode,rc,bRefuse;` |
|  102 | 1412 | `	if( pVm == 0 ){` |
|  ! 0 | 1413 | `		return -1;` |
|    - | 1414 | `	}` |
|  102 | 1415 | `	pZ = (phl_zstream *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_zstream));` |
|  102 | 1416 | `	if( pZ == 0 ){` |
|  ! 0 | 1417 | `		return -1;` |
|    - | 1418 | `	}` |
|  102 | 1419 | `	SyZero(pZ,sizeof(*pZ));` |
|  102 | 1420 | `	pZ->pVm = pVm;` |
|  102 | 1421 | `	SyBlobInit(&pZ->sOut,&pVm->sAllocator);` |
|  102 | 1422 | `	SyBlobInit(&pZ->sUri,&pVm->sAllocator);` |
|  102 | 1423 | `	pZ->iLevel = pVm->iZlibLevel;` |
|  102 | 1424 | `	pZ->iStrategy = pVm->iZlibStrategy;` |
|    - | 1425 | `	/* The armed options describe THIS open and nothing after it. */` |
|  102 | 1426 | `	PH7_ZlibArmOpen(pVm,-1,Z_DEFAULT_STRATEGY);` |
|    - | 1427 | `	/*` |
|    - | 1428 | `` 	 * php's zlib wrapper takes ONE direction and nothing else: `r`, `w` and `a` `` |
|    - | 1429 | ``	 * with their b/t hints. A mode asking for both (`r+`, `w+`, `c+`) is refused`` |
|    - | 1430 | ``	 * before anything is opened, while `x` and `c` are refused only after php has`` |
|    - | 1431 | ``	 * opened the file underneath -- so a refused `x` still LEAVES the file it`` |
|    - | 1432 | `	 * created. The refusal itself is the wrapper's flat one either way.` |
|    - | 1433 | `	 */` |
|  102 | 1434 | `	if( (iMode & PH7_IO_OPEN_RDWR) != 0 ){` |
|    9 | 1435 | `		SyBlobRelease(&pZ->sOut);` |
|    9 | 1436 | `		SyBlobRelease(&pZ->sUri);` |
|    9 | 1437 | `		SyMemBackendFree(&pVm->sAllocator,pZ);` |
|    9 | 1438 | `		return -1;` |
|    - | 1439 | `	}` |
|  184 | 1440 | `	bRefuse = (iMode & PH7_IO_OPEN_EXCL) != 0` |
|  124 | 1441 | `		\|\| ((iMode & PH7_IO_OPEN_CREATE) != 0` |
|   60 | 1442 | `			&& (iMode & (PH7_IO_OPEN_TRUNC\|PH7_IO_OPEN_APPEND)) == 0);` |
|   94 | 1443 | `	pZ->bWrite = (iMode & (PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_APPEND\|PH7_IO_OPEN_RDWR)) != 0;` |
|  135 | 1444 | `	iInnerMode = bRefuse ? iMode` |
|  128 | 1445 | `		: (pZ->bWrite` |
|   26 | 1446 | `			? (iMode & PH7_IO_OPEN_APPEND` |
|    - | 1447 | `				? PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_APPEND` |
|   13 | 1448 | `				: PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_TRUNC)` |
|   54 | 1449 | `			: PH7_IO_OPEN_RDONLY);` |
|   94 | 1450 | `	pInner = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|   94 | 1451 | `	if( pInner == 0 ){` |
|  ! 0 | 1452 | `		SyBlobRelease(&pZ->sOut);` |
|  ! 0 | 1453 | `		SyBlobRelease(&pZ->sUri);` |
|  ! 0 | 1454 | `		SyMemBackendFree(&pVm->sAllocator,pZ);` |
|  ! 0 | 1455 | `		return -1;` |
|    - | 1456 | `	}` |
|   94 | 1457 | `	SyZero(pInner,sizeof(*pInner));` |
|   94 | 1458 | `	pInnerDev = PH7_VmGetStreamDevice(pVm,&zPath,(int)SyStrlen(zPath));` |
|   94 | 1459 | `	InitIOPrivate(pVm,pInnerDev,pInner);` |
|    - | 1460 | `	/*` |
|    - | 1461 | `	 * gzopen() names a FILE, and the failure a script reads from it is that` |
|    - | 1462 | `	 * file's own -- the errno for a path, "Connection refused" for an http://` |
|    - | 1463 | `	 * one -- so its inner open reports as though it WERE the outer one, where an` |
|    - | 1464 | `	 * open running inside another is otherwise kept from naming the failure its` |
|    - | 1465 | ``	 * caller will report. Through `compress.zlib://` the same open is a WRAPPER's`` |
|    - | 1466 | `	 * and php answers every one of those with a flat "operation failed".` |
|    - | 1467 | `	 */` |
|   94 | 1468 | `	if( pVm->bZlibDirect ){` |
|   48 | 1469 | `		pVm->nOpenDepth--;` |
|   23 | 1470 | `	}` |
|   94 | 1471 | `	pInner->pHandle = pInnerDev` |
|   92 | 1472 | `		? PH7_StreamOpenHandle(pVm,pInnerDev,zPath,iInnerMode,FALSE,0,FALSE,0,0)` |
|   46 | 1473 | `		: 0;` |
|   94 | 1474 | `	if( pVm->bZlibDirect ){` |
|   48 | 1475 | `		pVm->nOpenDepth++;` |
|   25 | 1476 | `	}else{` |
|    - | 1477 | `		/* php's WRAPPER never repeats what the file underneath said. */` |
|   48 | 1478 | `		PH7_StreamSetOpenError(pVm,"operation failed");` |
|    - | 1479 | `	}` |
|   94 | 1480 | `	if( pInner->pHandle == 0 ){` |
|   18 | 1481 | `		SyMemBackendFree(&pVm->sAllocator,pInner);` |
|   18 | 1482 | `		SyBlobRelease(&pZ->sOut);` |
|   18 | 1483 | `		SyBlobRelease(&pZ->sUri);` |
|   18 | 1484 | `		SyMemBackendFree(&pVm->sAllocator,pZ);` |
|   18 | 1485 | `		return -1;` |
|    - | 1486 | `	}` |
|   78 | 1487 | `	if( bRefuse ){` |
|    - | 1488 | `		/* The file exists now, which is what php leaves behind; the stream does` |
|    - | 1489 | `		 * not, because libz has no direction for the mode that made it. */` |
|   12 | 1490 | `		PH7_StreamCloseHandle(pInnerDev,pInner->pHandle);` |
|   12 | 1491 | `		SyMemBackendFree(&pVm->sAllocator,pInner);` |
|   12 | 1492 | `		SyBlobRelease(&pZ->sOut);` |
|   12 | 1493 | `		SyBlobRelease(&pZ->sUri);` |
|   12 | 1494 | `		SyMemBackendFree(&pVm->sAllocator,pZ);` |
|   12 | 1495 | `		PH7_StreamSetOpenError(pVm,"operation failed");` |
|   12 | 1496 | `		return -1;` |
|    - | 1497 | `	}` |
|   68 | 1498 | `	SetIOPrivateOpenedAs(pInner,zName,(int)SyStrlen(zName),pZ->bWrite ? "wb" : "rb",2);` |
|   68 | 1499 | `	pZ->pInner = pInner;` |
|   68 | 1500 | `	SyBlobAppend(&pZ->sUri,zName,(sxu32)SyStrlen(zName));` |
|   68 | 1501 | `	if( pZ->bWrite ){` |
|   24 | 1502 | `		SyZero(&pZ->z,sizeof(pZ->z));` |
|   24 | 1503 | `		rc = deflateInit2(&pZ->z,pZ->iLevel,Z_DEFLATED,PHL_Z_GZIP,` |
|    - | 1504 | `			PHL_Z_MEMLEVEL,pZ->iStrategy);` |
|   24 | 1505 | `		if( rc != Z_OK ){` |
|  ! 0 | 1506 | `			ZStreamClose((void *)pZ);` |
|  ! 0 | 1507 | `			return -1;` |
|    - | 1508 | `		}` |
|   24 | 1509 | `		pZ->bInit = 1;` |
|   11 | 1510 | `	}` |
|   68 | 1511 | `	*ppHandle = (void *)pZ;` |
|   68 | 1512 | `	return PH7_OK;` |
|   52 | 1513 | `}` |
|    - | 1514 | `PH7_PRIVATE const ph7_io_stream sZLIB_Stream = {` |
|    - | 1515 | `	"compress.zlib",` |
|    - | 1516 | `	PH7_IO_STREAM_VERSION,` |
|    - | 1517 | `	ZStreamOpen,   /* xOpen */` |
|    - | 1518 | `	0,             /* xOpenDir: php's is "not implemented" */` |
|    - | 1519 | `	ZStreamClose,  /* xClose */` |
|    - | 1520 | `	0,             /* xCloseDir */` |
|    - | 1521 | `	ZStreamRead,   /* xRead */` |
|    - | 1522 | `	0,             /* xReadDir */` |
|    - | 1523 | `	ZStreamWrite,  /* xWrite */` |
|    - | 1524 | `	ZStreamSeek,   /* xSeek */` |
|    - | 1525 | `	ZStreamLock,   /* xLock */` |
|    - | 1526 | `	0,             /* xRewindDir */` |
|    - | 1527 | `	ZStreamTell,   /* xTell */` |
|    - | 1528 | `	0,             /* xTrunc: php's "Can't truncate this stream!" */` |
|    - | 1529 | `	0,             /* xSync */` |
|    - | 1530 | `	ZStreamStat    /* xStat: php answers a flat false */` |
|    - | 1531 | `};` |
|  201 | 1532 | `PH7_PRIVATE int PH7_ZlibStreamIs(const ph7_io_stream *pStream)` |
|    5 | 1533 | `{` |
|  206 | 1534 | `	return pStream == &sZLIB_Stream;` |
|    5 | 1535 | `}` |
|    - | 1536 | `/* ------------------------------------------------------------------ */` |
|    - | 1537 | `/* gzopen() and the two whole-file readers                            */` |
|    - | 1538 | `/* ------------------------------------------------------------------ */` |
|    - | 1539 | `/*` |
|    - | 1540 | ` * php hands gzopen()'s mode string to libz, which reads it as: one of r/w/a,` |
|    - | 1541 | `` * an optional `b`, an optional LEVEL digit and an optional strategy letter`` |
|    - | 1542 | ` * (f filtered, h Huffman-only, R run-length, F fixed). php screens one thing` |
|    - | 1543 | `` * before libz sees it -- a `+` of any kind -- because a zlib stream cannot be`` |
|    - | 1544 | ` * read and written at once.` |
|    - | 1545 | ` */` |
|   34 | 1546 | `static int ZlibParseMode(const char *zMode,int nMode,int *piLevel,int *piStrategy,` |
|    - | 1547 | `	char *zClean,int nClean)` |
|    2 | 1548 | `{` |
|   36 | 1549 | `	int i,n = 0;` |
|   36 | 1550 | `	*piLevel = -1;` |
|   36 | 1551 | `	*piStrategy = Z_DEFAULT_STRATEGY;` |
|   82 | 1552 | `	for( i = 0 ; i < nMode && n + 1 < nClean ; ++i ){` |
|   48 | 1553 | `		int c = zMode[i];` |
|   48 | 1554 | `		if( c >= '0' && c <= '9' ){` |
|    3 | 1555 | `			*piLevel = c - '0';` |
|    3 | 1556 | `			continue;` |
|    - | 1557 | `		}` |
|   46 | 1558 | `		switch( c ){` |
|    3 | 1559 | `		case 'f': *piStrategy = Z_FILTERED; continue;` |
|  ! 0 | 1560 | `		case 'h': *piStrategy = Z_HUFFMAN_ONLY; continue;` |
|  ! 0 | 1561 | `		case 'R': *piStrategy = Z_RLE; continue;` |
|  ! 0 | 1562 | `		case 'F': *piStrategy = Z_FIXED; continue;` |
|  ! 0 | 1563 | `		case 'T': continue;   /* libz's "write transparently" */` |
|   42 | 1564 | `		default: break;` |
|    - | 1565 | `		}` |
|   44 | 1566 | `		zClean[n++] = (char)c;` |
|   23 | 1567 | `	}` |
|   36 | 1568 | `	zClean[n] = 0;` |
|   36 | 1569 | `	return n;` |
|    2 | 1570 | `}` |
|    - | 1571 | `/*` |
|    - | 1572 | ` * The open both gzopen() and the two whole-file doors go through. It is` |
|    - | 1573 | ` * fopen()'s sequence with the device fixed: no scheme is parsed off the path` |
|    - | 1574 | ` * (gzopen('f.gz') names a FILE), the compressed side is resolved by the` |
|    - | 1575 | ` * device's own open, and the handle is left with NO uri -- which is what makes` |
|    - | 1576 | `` * php report neither a `wrapper_type` nor a `uri` for a gzopen() stream, where`` |
|    - | 1577 | ` * the same file through compress.zlib:// reports both.` |
|    - | 1578 | ` */` |
|   32 | 1579 | `static io_private * ZlibOpenDevice(ph7_context *pCtx,ph7_value *pPath,` |
|    - | 1580 | `	const char *zMode,int nMode,int bUseInclude,int iForceFlags)` |
|    2 | 1581 | `{` |
|   34 | 1582 | `	const ph7_io_stream *pStream = &sZLIB_Stream;` |
|    - | 1583 | `	io_private *pDev;` |
|    - | 1584 | `	const char *zUri;` |
|   34 | 1585 | `	int nUri = 0,iFlags;` |
|   34 | 1586 | `	zUri = ph7_value_to_string(pPath,&nUri);` |
|   34 | 1587 | `	if( PH7_VfsEmptyPathRefused(pCtx,nUri) ){` |
|  ! 0 | 1588 | `		return 0;` |
|    - | 1589 | `	}` |
|   34 | 1590 | `	switch( zMode[0] ){` |
|   10 | 1591 | `	case 'w': iFlags = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_TRUNC; break;` |
|    5 | 1592 | `	case 'a': iFlags = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_APPEND; break;` |
|   22 | 1593 | `	default:  iFlags = PH7_IO_OPEN_RDONLY; break;` |
|    - | 1594 | `	}` |
|   34 | 1595 | `	if( iForceFlags != 0 ){` |
|    - | 1596 | ``		/* A mode php's grammar takes and libz has no direction for -- `x`. php`` |
|    - | 1597 | ``		 * still OPENS the file (so an existing one is `File exists` and a`` |
|    - | 1598 | `		 * missing one is created) and only then discovers it cannot compress` |
|    - | 1599 | `		 * through it, so the open here is the plain one, without this device` |
|    - | 1600 | `		 * over it. */` |
|    3 | 1601 | `		const char *zTail = zUri;` |
|    3 | 1602 | `		iFlags = iForceFlags;` |
|    3 | 1603 | `		pStream = PH7_VfsStreamDeviceOrFile(pCtx,&zTail,nUri);` |
|    3 | 1604 | `		zUri = zTail;` |
|    3 | 1605 | `		if( pStream == 0 ){` |
|  ! 0 | 1606 | `			VfsThrowNoDeviceWarning(pCtx,zUri,FALSE);` |
|  ! 0 | 1607 | `			return 0;` |
|    - | 1608 | `		}` |
|    1 | 1609 | `	}` |
|   34 | 1610 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|   34 | 1611 | `	if( pDev == 0 ){` |
|  ! 0 | 1612 | `		return 0;` |
|    - | 1613 | `	}` |
|   34 | 1614 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|   34 | 1615 | `	PH7_StreamArmOpenMode(pCtx->pVm,zMode,nMode);` |
|   34 | 1616 | `	PH7_ZlibArmDirect(pCtx->pVm,1);` |
|   50 | 1617 | `	pDev->pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zUri,iFlags,` |
|   16 | 1618 | `		bUseInclude,0,FALSE,0,ph7_function_name(pCtx));` |
|   34 | 1619 | `	PH7_ZlibArmDirect(pCtx->pVm,0);` |
|   34 | 1620 | `	if( pDev->pHandle == 0 ){` |
|   10 | 1621 | `		VfsThrowOpenWarning(pCtx,zUri);` |
|   10 | 1622 | `		PH7_StreamReleaseUnopened(pCtx,pDev);` |
|   10 | 1623 | `		return 0;` |
|    - | 1624 | `	}` |
|   25 | 1625 | `	SetIOPrivateOpenedAs(pDev,0,0,zMode,nMode);` |
|   25 | 1626 | `	return pDev;` |
|   18 | 1627 | `}` |
|    - | 1628 | `/* resource\|false gzopen(string $filename, string $mode, bool $use_include_path = false) */` |
|   36 | 1629 | `static int PH7_builtin_gzopen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 | 1630 | `{` |
|    - | 1631 | `	const char *zMode;` |
|    - | 1632 | `	char zClean[16];` |
|    - | 1633 | `	io_private *pDev;` |
|   38 | 1634 | `	int nMode = 0,nClean,iLevel,iStrategy,i;` |
|   38 | 1635 | `	if( nArg < 2 ){` |
|  ! 0 | 1636 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1637 | `		return PH7_OK;` |
|    - | 1638 | `	}` |
|   38 | 1639 | `	zMode = ph7_value_to_string(apArg[1],&nMode);` |
|   86 | 1640 | `	for( i = 0 ; i < nMode ; ++i ){` |
|   52 | 1641 | `		if( zMode[i] == '+' ){` |
|    3 | 1642 | `			ph7_context_throw_error(pCtx,PH7_CTX_WARNING,` |
|    - | 1643 | `				"Cannot open a zlib stream for reading and writing at the same time!");` |
|    3 | 1644 | `			ph7_result_bool(pCtx,0);` |
|    3 | 1645 | `			return PH7_OK;` |
|    - | 1646 | `		}` |
|   26 | 1647 | `	}` |
|   36 | 1648 | `	nClean = ZlibParseMode(zMode,nMode,&iLevel,&iStrategy,zClean,(int)sizeof(zClean));` |
|   36 | 1649 | `	if( nClean < 1 \|\| (zClean[0] != 'r' && zClean[0] != 'w' && zClean[0] != 'a') ){` |
|    - | 1650 | `		/* TWO refusals, and which one php raises says whose grammar the mode` |
|    - | 1651 | `		 * broke. A letter php's own fopen grammar does not know is that` |
|    - | 1652 | `		 * sentence, named for gzopen and the path. One php ACCEPTS and libz has` |
|    - | 1653 | ``		 * no direction for -- `x` -- is opened first and refused after, so an`` |
|    - | 1654 | ``		 * existing file reports `File exists` from the open and a missing one`` |
|    - | 1655 | `		 * is created and then reports libz's flat failure. */` |
|    5 | 1656 | `		int iFlags = 0;` |
|    5 | 1657 | `		if( PH7_StreamModeIsValid(zMode,nMode,&iFlags) ){` |
|    4 | 1658 | `			io_private *pTmp = ZlibOpenDevice(pCtx,apArg[0],zMode,nMode,` |
|    1 | 1659 | `				nArg > 2 ? ph7_value_to_bool(apArg[2]) : FALSE,` |
|    2 | 1660 | `				iFlags ? iFlags : PH7_IO_OPEN_RDONLY);` |
|    3 | 1661 | `			if( pTmp ){` |
|  ! 0 | 1662 | `				PH7_StreamCloseHandle(pTmp->pStream,pTmp->pHandle);` |
|  ! 0 | 1663 | `				MarkIOPrivateClosed(pTmp);` |
|  ! 0 | 1664 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"gzopen failed");` |
|  ! 0 | 1665 | `			}` |
|    2 | 1666 | `		}else{` |
|    3 | 1667 | `			int nPath = 0;` |
|    3 | 1668 | `			const char *zPath = ph7_value_to_string(apArg[0],&nPath);` |
|    4 | 1669 | `			PH7_VmThrowWarningFmt(pCtx->pVm,` |
|    - | 1670 | ``				"%s(%.*s): Failed to open stream: `%.*s' is not a valid mode for fopen",`` |
|    1 | 1671 | `				ph7_function_name(pCtx),nPath,zPath,nMode,zMode);` |
|    - | 1672 | `		}` |
|    5 | 1673 | `		ph7_result_bool(pCtx,0);` |
|    5 | 1674 | `		return PH7_OK;` |
|    - | 1675 | `	}` |
|   32 | 1676 | `	PH7_ZlibArmOpen(pCtx->pVm,iLevel,iStrategy);` |
|   47 | 1677 | `	pDev = ZlibOpenDevice(pCtx,apArg[0],zClean,nClean,` |
|   15 | 1678 | `		nArg > 2 ? ph7_value_to_bool(apArg[2]) : FALSE,0);` |
|   32 | 1679 | `	PH7_ZlibArmOpen(pCtx->pVm,-1,Z_DEFAULT_STRATEGY);` |
|   32 | 1680 | `	if( pDev == 0 ){` |
|    8 | 1681 | `		ph7_result_bool(pCtx,0);` |
|    8 | 1682 | `		return PH7_OK;` |
|    - | 1683 | `	}` |
|   25 | 1684 | `	ph7_result_resource(pCtx,pDev);` |
|   25 | 1685 | `	return PH7_OK;` |
|   20 | 1686 | `}` |
|    - | 1687 | `/*` |
|    - | 1688 | ` * The two whole-file doors. Both open the same device and read it to the end;` |
|    - | 1689 | ` * gzfile() splits on newlines the way file() does and readgzfile() prints.` |
|    - | 1690 | ` */` |
|   16 | 1691 | `static int ZlibWholeFile(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLines)` |
|    2 | 1692 | `{` |
|    - | 1693 | `	SyBlob sAll;` |
|    - | 1694 | `	void *pHandle;` |
|    - | 1695 | `	const char *zUri;` |
|   18 | 1696 | `	int nUri = 0;` |
|   18 | 1697 | `	if( nArg < 1 ){` |
|  ! 0 | 1698 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1699 | `		return PH7_OK;` |
|    - | 1700 | `	}` |
|   18 | 1701 | `	zUri = ph7_value_to_string(apArg[0],&nUri);` |
|   18 | 1702 | `	if( PH7_VfsEmptyPathRefused(pCtx,nUri) ){` |
|  ! 0 | 1703 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1704 | `		return PH7_OK;` |
|    - | 1705 | `	}` |
|    - | 1706 | `	/* No io_private: nothing here reaches PHP, so the device handle is opened,` |
|    - | 1707 | `	 * read and closed inside this call the way every other whole-file reader in` |
|    - | 1708 | `	 * the engine does it. */` |
|   18 | 1709 | `	PH7_ZlibArmOpen(pCtx->pVm,-1,Z_DEFAULT_STRATEGY);` |
|   18 | 1710 | `	PH7_StreamArmOpenMode(pCtx->pVm,"rb",2);` |
|   18 | 1711 | `	PH7_ZlibArmDirect(pCtx->pVm,1);` |
|   26 | 1712 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,&sZLIB_Stream,zUri,PH7_IO_OPEN_RDONLY,` |
|    8 | 1713 | `		nArg > 1 ? ph7_value_to_bool(apArg[1]) : FALSE,0,FALSE,0,` |
|    8 | 1714 | `		ph7_function_name(pCtx));` |
|   18 | 1715 | `	PH7_ZlibArmDirect(pCtx->pVm,0);` |
|   18 | 1716 | `	if( pHandle == 0 ){` |
|    8 | 1717 | `		VfsThrowOpenWarning(pCtx,zUri);` |
|    8 | 1718 | `		ph7_result_bool(pCtx,0);` |
|    8 | 1719 | `		return PH7_OK;` |
|    - | 1720 | `	}` |
|   11 | 1721 | `	SyBlobInit(&sAll,&pCtx->pVm->sAllocator);` |
|   15 | 1722 | `	for(;;){` |
|    - | 1723 | `		char zBuf[PHL_Z_CHUNK];` |
|   21 | 1724 | `		ph7_int64 nRead = ZStreamRead(pHandle,zBuf,(ph7_int64)sizeof(zBuf));` |
|   21 | 1725 | `		if( nRead < 1 ){` |
|   11 | 1726 | `			break;` |
|    - | 1727 | `		}` |
|   11 | 1728 | `		SyBlobAppend(&sAll,zBuf,(sxu32)nRead);` |
|    1 | 1729 | `	}` |
|   11 | 1730 | `	PH7_StreamCloseHandle(&sZLIB_Stream,pHandle);` |
|   11 | 1731 | `	if( bLines ){` |
|    9 | 1732 | `		const char *zData = (const char *)SyBlobData(&sAll);` |
|    9 | 1733 | `		sxu32 nLen = SyBlobLength(&sAll),nStart = 0,n;` |
|    9 | 1734 | `		ph7_value *pArray = ph7_context_new_array(pCtx);` |
|    9 | 1735 | `		ph7_value *pLine = ph7_context_new_scalar(pCtx);` |
|    9 | 1736 | `		if( pArray == 0 \|\| pLine == 0 ){` |
|  ! 0 | 1737 | `			SyBlobRelease(&sAll);` |
|  ! 0 | 1738 | `			ph7_result_bool(pCtx,0);` |
|  ! 0 | 1739 | `			return PH7_OK;` |
|    - | 1740 | `		}` |
|  127 | 1741 | `		for( n = 0 ; n < nLen ; ++n ){` |
|  119 | 1742 | `			if( zData[n] == '\n' ){` |
|   21 | 1743 | `				ph7_value_string(pLine,&zData[nStart],(int)(n - nStart + 1));` |
|   21 | 1744 | `				ph7_array_add_elem(pArray,0,pLine);` |
|   21 | 1745 | `				ph7_value_reset_string_cursor(pLine);` |
|   21 | 1746 | `				nStart = n + 1;` |
|   10 | 1747 | `			}` |
|   60 | 1748 | `		}` |
|    9 | 1749 | `		if( nStart < nLen ){` |
|  ! 0 | 1750 | `			ph7_value_string(pLine,&zData[nStart],(int)(nLen - nStart));` |
|  ! 0 | 1751 | `			ph7_array_add_elem(pArray,0,pLine);` |
|  ! 0 | 1752 | `		}` |
|    9 | 1753 | `		ph7_result_value(pCtx,pArray);` |
|    9 | 1754 | `		ph7_context_release_value(pCtx,pLine);` |
|    5 | 1755 | `	}else{` |
|    3 | 1756 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sAll),(int)SyBlobLength(&sAll));` |
|    3 | 1757 | `		ph7_result_int64(pCtx,(sxi64)SyBlobLength(&sAll));` |
|    - | 1758 | `	}` |
|   11 | 1759 | `	SyBlobRelease(&sAll);` |
|   11 | 1760 | `	return PH7_OK;` |
|   10 | 1761 | `}` |
|    - | 1762 | `/* array\|false gzfile(string $filename, bool $use_include_path = false) */` |
|   12 | 1763 | `static int PH7_builtin_gzfile(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 | 1764 | `{` |
|   14 | 1765 | `	return ZlibWholeFile(pCtx,nArg,apArg,1);` |
|    2 | 1766 | `}` |
|    - | 1767 | `/* int\|false readgzfile(string $filename, bool $use_include_path = false) */` |
|    4 | 1768 | `static int PH7_builtin_readgzfile(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1769 | `{` |
|    5 | 1770 | `	return ZlibWholeFile(pCtx,nArg,apArg,0);` |
|    1 | 1771 | `}` |
|    - | 1772 | `/* ------------------------------------------------------------------ */` |
|    - | 1773 | `/* The zlib.* stream filters                                          */` |
|    - | 1774 | `/* ------------------------------------------------------------------ */` |
|    - | 1775 | `/*` |
|    - | 1776 | `` * php registers ONE filter factory, `zlib.*`, which answers for exactly two`` |
|    - | 1777 | `` * names -- anything else under it (`zlib.bogus`, and `zlib.deflate.foo` as`` |
|    - | 1778 | ` * well) is php's "Unable to create or locate filter".` |
|    - | 1779 | ` *` |
|    - | 1780 | ` * Its $params are read the way php's are, which is not the way the deflate_init()` |
|    - | 1781 | ` * options are read at all: a bad value here is a WARNING and the default, not a` |
|    - | 1782 | ` * ValueError, and the only value that stops the filter being created is one libz` |
|    - | 1783 | `` * itself refuses (`window` 7 is inside php's accepted -15..47 range and fails in`` |
|    - | 1784 | `` * deflateInit2, which is why it reads as "no such filter" while `window` 100 is`` |
|    - | 1785 | `` * a warning and a default). `level` may also be given as a bare int instead of`` |
|    - | 1786 | ` * an array; anything else that is not an array is "Invalid filter parameter,` |
|    - | 1787 | ` * ignored" -- an explicitly passed NULL included.` |
|    - | 1788 | ` *` |
|    - | 1789 | ` * The flush mode each run uses is the chain's own: an ordinary run is` |
|    - | 1790 | `` * Z_NO_FLUSH (so `fwrite` twice and `fclose` produces ONE deflate stream with`` |
|    - | 1791 | ` * no sync markers in it), an fflush() is Z_SYNC_FLUSH, and the close is` |
|    - | 1792 | ` * Z_FINISH.` |
|    - | 1793 | ` */` |
|    - | 1794 | `typedef struct phl_zfilter phl_zfilter;` |
|    - | 1795 | `struct phl_zfilter {` |
|    - | 1796 | `	z_stream z;` |
|    - | 1797 | `	int bInflate;` |
|    - | 1798 | `	int bInit;` |
|    - | 1799 | `	int bDone;     /* Z_STREAM_END seen, or the stream failed */` |
|    - | 1800 | `};` |
|    6 | 1801 | `static void ZlibFilterWarn(phl_stream_filter *pFilter,const char *zFmt,sxi64 iVal)` |
|    1 | 1802 | `{` |
|    - | 1803 | `	char zMsg[128];` |
|    7 | 1804 | `	SyBufferFormat(zMsg,sizeof(zMsg),zFmt,iVal);` |
|    7 | 1805 | `	PH7_VmThrowError(pFilter->pVm,pFilter->pVm->pCalleeName,PH7_CTX_WARNING,zMsg);` |
|    7 | 1806 | `}` |
|   26 | 1807 | `PH7_PRIVATE int PH7_ZlibFilterCreate(phl_stream_filter *pFilter,ph7_value *pParams)` |
|    1 | 1808 | `{` |
|   27 | 1809 | `	const char *zName = (const char *)SyBlobData(&pFilter->sName);` |
|   27 | 1810 | `	int nName = (int)SyBlobLength(&pFilter->sName);` |
|    - | 1811 | `	phl_zfilter *pState;` |
|   27 | 1812 | `	int bInflate,iLevel = Z_DEFAULT_COMPRESSION,iMemory = PHL_Z_MEMLEVEL;` |
|   27 | 1813 | `	int iWindow = -MAX_WBITS,rc;` |
|   27 | 1814 | `	if( nName == (int)sizeof("zlib.deflate")-1 && SyMemcmp(zName,"zlib.deflate",12) == 0 ){` |
|   17 | 1815 | `		bInflate = 0;` |
|   19 | 1816 | `	}else if( nName == (int)sizeof("zlib.inflate")-1 && SyMemcmp(zName,"zlib.inflate",12) == 0 ){` |
|    7 | 1817 | `		bInflate = 1;` |
|    4 | 1818 | `	}else{` |
|    5 | 1819 | `		return -1;` |
|    - | 1820 | `	}` |
|   23 | 1821 | `	if( pParams != 0 ){` |
|   19 | 1822 | `		if( ph7_value_is_array(pParams) ){` |
|    - | 1823 | `			ph7_value *pVal;` |
|    - | 1824 | ``			/* Only `window` reaches the INFLATE side: php's filter reads`` |
|    - | 1825 | `			 * nothing else for it, so a level or a memory level given to` |
|    - | 1826 | `			 * zlib.inflate is ignored without a word -- and so is a $params` |
|    - | 1827 | `			 * that is not an array at all, which the deflate side warns` |
|    - | 1828 | `			 * about. */` |
|   15 | 1829 | `			if( !bInflate && (pVal = ph7_array_fetch(pParams,"level",-1)) != 0 ){` |
|    5 | 1830 | `				sxi64 i = PH7_ValuePeekInt64(pVal);` |
|    5 | 1831 | `				if( i < -1 \|\| i > 9 ){` |
|    3 | 1832 | `					ZlibFilterWarn(pFilter,"Invalid compression level specified. (%qd)",i);` |
|    2 | 1833 | `				}else{` |
|    3 | 1834 | `					iLevel = (int)i;` |
|    - | 1835 | `				}` |
|    2 | 1836 | `			}` |
|   15 | 1837 | `			if( !bInflate && (pVal = ph7_array_fetch(pParams,"memory",-1)) != 0 ){` |
|    3 | 1838 | `				sxi64 i = PH7_ValuePeekInt64(pVal);` |
|    3 | 1839 | `				if( i < 1 \|\| i > 9 ){` |
|    3 | 1840 | `					ZlibFilterWarn(pFilter,"Invalid parameter given for memory level (%qd)",i);` |
|    2 | 1841 | `				}else{` |
|  ! 0 | 1842 | `					iMemory = (int)i;` |
|    - | 1843 | `				}` |
|    1 | 1844 | `			}` |
|   15 | 1845 | `			if( (pVal = ph7_array_fetch(pParams,"window",-1)) != 0 ){` |
|    9 | 1846 | `				sxi64 i = PH7_ValuePeekInt64(pVal);` |
|    9 | 1847 | `				if( i < -MAX_WBITS \|\| i > MAX_WBITS + 32 ){` |
|    3 | 1848 | `					ZlibFilterWarn(pFilter,"Invalid parameter given for window size (%qd)",i);` |
|    2 | 1849 | `				}else{` |
|    7 | 1850 | `					iWindow = (int)i;` |
|    - | 1851 | `				}` |
|    5 | 1852 | `			}` |
|    - | 1853 | ``			/* `strategy` and `dictionary` are read by deflate_init()'s options`` |
|    - | 1854 | `			 * and by nothing here: php's filter ignores both without a word. */` |
|   12 | 1855 | `		}else if( ph7_value_is_int(pParams) ){` |
|  ! 0 | 1856 | `			sxi64 i = ph7_value_to_int64(pParams);` |
|  ! 0 | 1857 | `			if( !bInflate && (i < -1 \|\| i > 9) ){` |
|  ! 0 | 1858 | `				ZlibFilterWarn(pFilter,"Invalid compression level specified. (%qd)",i);` |
|  ! 0 | 1859 | `			}else if( !bInflate ){` |
|  ! 0 | 1860 | `				iLevel = (int)i;` |
|  ! 0 | 1861 | `			}` |
|    5 | 1862 | `		}else if( !bInflate ){` |
|    3 | 1863 | `			PH7_VmThrowError(pFilter->pVm,pFilter->pVm->pCalleeName,PH7_CTX_WARNING,` |
|    - | 1864 | `				"Invalid filter parameter, ignored");` |
|    1 | 1865 | `		}` |
|    9 | 1866 | `	}` |
|   23 | 1867 | `	pState = (phl_zfilter *)SyMemBackendAlloc(&pFilter->pVm->sAllocator,sizeof(phl_zfilter));` |
|   23 | 1868 | `	if( pState == 0 ){` |
|  ! 0 | 1869 | `		return -1;` |
|    - | 1870 | `	}` |
|   23 | 1871 | `	SyZero(pState,sizeof(*pState));` |
|   23 | 1872 | `	pState->bInflate = bInflate;` |
|   23 | 1873 | `	rc = bInflate` |
|    6 | 1874 | `		? inflateInit2(&pState->z,iWindow)` |
|   19 | 1875 | `		: deflateInit2(&pState->z,iLevel,Z_DEFLATED,iWindow,iMemory,Z_DEFAULT_STRATEGY);` |
|   23 | 1876 | `	if( rc != Z_OK ){` |
|    - | 1877 | `		/* libz refused the window: php's filter simply is not created. */` |
|    3 | 1878 | `		SyMemBackendFree(&pFilter->pVm->sAllocator,pState);` |
|    3 | 1879 | `		return -1;` |
|    - | 1880 | `	}` |
|   21 | 1881 | `	pState->bInit = 1;` |
|   21 | 1882 | `	pFilter->pPriv = (void *)pState;` |
|   21 | 1883 | `	return PH7_OK;` |
|   14 | 1884 | `}` |
|   26 | 1885 | `PH7_PRIVATE void PH7_ZlibFilterClose(phl_stream_filter *pFilter)` |
|    1 | 1886 | `{` |
|   27 | 1887 | `	phl_zfilter *pState = (phl_zfilter *)pFilter->pPriv;` |
|   27 | 1888 | `	if( pState == 0 ){` |
|    7 | 1889 | `		return;` |
|    - | 1890 | `	}` |
|   21 | 1891 | `	if( pState->bInit ){` |
|   21 | 1892 | `		if( pState->bInflate ){` |
|    7 | 1893 | `			inflateEnd(&pState->z);` |
|    4 | 1894 | `		}else{` |
|   15 | 1895 | `			deflateEnd(&pState->z);` |
|    - | 1896 | `		}` |
|   10 | 1897 | `	}` |
|   21 | 1898 | `	SyMemBackendFree(&pFilter->pVm->sAllocator,pState);` |
|   21 | 1899 | `	pFilter->pPriv = 0;` |
|   14 | 1900 | `}` |
|   26 | 1901 | `PH7_PRIVATE int PH7_ZlibFilterRun(phl_stream_filter *pFilter,phl_brigade *pIn,` |
|    - | 1902 | `	phl_brigade *pOut,int iFlags)` |
|    1 | 1903 | `{` |
|   27 | 1904 | `	phl_zfilter *pState = (phl_zfilter *)pFilter->pPriv;` |
|    - | 1905 | `	phl_bucket *pBucket;` |
|    - | 1906 | `	SyBlob sIn,sOut;` |
|    - | 1907 | `	unsigned char zBuf[PHL_Z_CHUNK];` |
|   27 | 1908 | `	int iFlush,rc = Z_OK;` |
|   27 | 1909 | `	if( pState == 0 ){` |
|  ! 0 | 1910 | `		return PHL_PSFS_ERR_FATAL;` |
|    - | 1911 | `	}` |
|   27 | 1912 | `	iFlush = iFlags == PHL_PSFS_FLAG_FLUSH_CLOSE ? Z_FINISH` |
|   18 | 1913 | `	       : (iFlags == PHL_PSFS_FLAG_FLUSH_INC ? Z_SYNC_FLUSH : Z_NO_FLUSH);` |
|   27 | 1914 | `	SyBlobInit(&sIn,&pFilter->pVm->sAllocator);` |
|   27 | 1915 | `	SyBlobInit(&sOut,&pFilter->pVm->sAllocator);` |
|   43 | 1916 | `	while( (pBucket = PH7_FilterBucketPop(pIn)) != 0 ){` |
|   17 | 1917 | `		if( SyBlobLength(&pBucket->sData) > 0 ){` |
|   17 | 1918 | `			SyBlobAppend(&sIn,SyBlobData(&pBucket->sData),SyBlobLength(&pBucket->sData));` |
|    8 | 1919 | `		}` |
|   17 | 1920 | `		PH7_FilterBucketFree(pFilter->pVm,pBucket);` |
|    1 | 1921 | `	}` |
|   27 | 1922 | `	if( pState->bDone ){` |
|    - | 1923 | `		/* Everything after the end of the stream is dropped, which is what libz` |
|    - | 1924 | `		 * does with the bytes behind a finished member. */` |
|    3 | 1925 | `		SyBlobRelease(&sIn);` |
|    3 | 1926 | `		SyBlobRelease(&sOut);` |
|    3 | 1927 | `		return PHL_PSFS_PASS_ON;` |
|    - | 1928 | `	}` |
|   25 | 1929 | `	pState->z.next_in = (Bytef *)SyBlobData(&sIn);` |
|   25 | 1930 | `	pState->z.avail_in = (uInt)SyBlobLength(&sIn);` |
|   12 | 1931 | `	for(;;){` |
|   25 | 1932 | `		pState->z.next_out = (Bytef *)zBuf;` |
|   25 | 1933 | `		pState->z.avail_out = (uInt)sizeof(zBuf);` |
|   37 | 1934 | `		rc = pState->bInflate` |
|    2 | 1935 | `			? inflate(&pState->z,iFlush)` |
|   23 | 1936 | `			: deflate(&pState->z,iFlush);` |
|   25 | 1937 | `		if( sizeof(zBuf) - pState->z.avail_out > 0 ){` |
|   21 | 1938 | `			SyBlobAppend(&sOut,zBuf,(sxu32)(sizeof(zBuf) - pState->z.avail_out));` |
|   10 | 1939 | `		}` |
|   25 | 1940 | `		if( rc == Z_STREAM_END ){` |
|   17 | 1941 | `			pState->bDone = 1;` |
|   17 | 1942 | `			break;` |
|    - | 1943 | `		}` |
|    9 | 1944 | `		if( rc != Z_OK && rc != Z_BUF_ERROR ){` |
|    - | 1945 | `			/* php's own notice, raised from whichever reader asked. */` |
|    - | 1946 | `			char zMsg[64];` |
|  ! 0 | 1947 | `			SyBufferFormat(zMsg,sizeof(zMsg),"zlib: %s",zError(rc));` |
|  ! 0 | 1948 | `			PH7_VmThrowError(pFilter->pVm,pFilter->pVm->pCalleeName,PH7_CTX_NOTICE,zMsg);` |
|  ! 0 | 1949 | `			pState->bDone = 1;` |
|  ! 0 | 1950 | `			SyBlobRelease(&sIn);` |
|  ! 0 | 1951 | `			SyBlobRelease(&sOut);` |
|  ! 0 | 1952 | `			return PHL_PSFS_ERR_FATAL;` |
|    - | 1953 | `		}` |
|    9 | 1954 | `		if( pState->z.avail_out != 0 ){` |
|    - | 1955 | `			/* Room left in the buffer: libz has nothing more for this run. */` |
|    9 | 1956 | `			if( pState->z.avail_in == 0 && iFlush == Z_NO_FLUSH ){` |
|    9 | 1957 | `				break;` |
|    - | 1958 | `			}` |
|  ! 0 | 1959 | `			if( rc == Z_BUF_ERROR ){` |
|  ! 0 | 1960 | `				break;` |
|    - | 1961 | `			}` |
|  ! 0 | 1962 | `			if( iFlush != Z_NO_FLUSH && pState->z.avail_in == 0 ){` |
|  ! 0 | 1963 | `				break;` |
|    - | 1964 | `			}` |
|  ! 0 | 1965 | `		}` |
|  ! 0 | 1966 | `	}` |
|   25 | 1967 | `	if( SyBlobLength(&sOut) > 0 ){` |
|   31 | 1968 | `		PH7_FilterBucketAppend(pOut,` |
|   20 | 1969 | `			PH7_FilterBucketNew(pFilter->pVm,SyBlobData(&sOut),SyBlobLength(&sOut)));` |
|   10 | 1970 | `	}` |
|   25 | 1971 | `	SyBlobRelease(&sIn);` |
|   25 | 1972 | `	SyBlobRelease(&sOut);` |
|   25 | 1973 | `	return PHL_PSFS_PASS_ON;` |
|   14 | 1974 | `}` |
|    - | 1975 | `/* ------------------------------------------------------------------ */` |
|    - | 1976 | `/* The output-compression pair                                        */` |
|    - | 1977 | `/* ------------------------------------------------------------------ */` |
|    - | 1978 | `/*` |
|    - | 1979 | ` * Both of these answer from the REQUEST: php compresses a response only when` |
|    - | 1980 | ` * the client said it would take one, so on a command line -- where there is no` |
|    - | 1981 | ` * Accept-Encoding at all -- ob_gzhandler() answers false and` |
|    - | 1982 | `` * zlib_get_coding_type() answers false as well. Under `phl -S` the header is`` |
|    - | 1983 | ` * there and the pair behaves as php's does.` |
|    - | 1984 | ` */` |
|    2 | 1985 | `static int ZlibAcceptedEncoding(ph7_vm *pVm)` |
|    1 | 1986 | `{` |
|    - | 1987 | `	ph7_value *pServer,*pVal;` |
|    - | 1988 | `	const char *zHdr;` |
|    3 | 1989 | `	int nHdr = 0;` |
|    3 | 1990 | `	pServer = PH7_VmExtractSuper(pVm,"_SERVER",sizeof("_SERVER")-1);` |
|    3 | 1991 | `	if( pServer == 0 \|\| !ph7_value_is_array(pServer) ){` |
|  ! 0 | 1992 | `		return 0;` |
|    - | 1993 | `	}` |
|    3 | 1994 | `	pVal = ph7_array_fetch(pServer,"HTTP_ACCEPT_ENCODING",` |
|    - | 1995 | `		sizeof("HTTP_ACCEPT_ENCODING")-1);` |
|    3 | 1996 | `	if( pVal == 0 ){` |
|    3 | 1997 | `		return 0;` |
|    - | 1998 | `	}` |
|  ! 0 | 1999 | `	zHdr = ph7_value_to_string(pVal,&nHdr);` |
|  ! 0 | 2000 | `	if( nHdr < 1 ){` |
|  ! 0 | 2001 | `		return 0;` |
|    - | 2002 | `	}` |
|    - | 2003 | `	/* php reads the two names in this order and ignores every q-value. */` |
|  ! 0 | 2004 | `	if( SyBlobSearch(zHdr,(sxu32)nHdr,"gzip",sizeof("gzip")-1,0) == SXRET_OK ){` |
|  ! 0 | 2005 | `		return PHL_Z_GZIP;` |
|    - | 2006 | `	}` |
|  ! 0 | 2007 | `	if( SyBlobSearch(zHdr,(sxu32)nHdr,"deflate",sizeof("deflate")-1,0) == SXRET_OK ){` |
|  ! 0 | 2008 | `		return PHL_Z_DEFLATE;` |
|    - | 2009 | `	}` |
|  ! 0 | 2010 | `	return 0;` |
|    2 | 2011 | `}` |
|    - | 2012 | `/* string\|false ob_gzhandler(string $data, int $flags) */` |
|    2 | 2013 | `static int PH7_builtin_ob_gzhandler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 2014 | `{` |
|    3 | 2015 | `	int iEnc = ZlibAcceptedEncoding(pCtx->pVm);` |
|    - | 2016 | `	const char *zIn;` |
|    3 | 2017 | `	int nIn = 0,iStatus = 0;` |
|    - | 2018 | `	SyBlob sOut;` |
|    3 | 2019 | `	if( nArg < 1 \|\| iEnc == 0 ){` |
|    3 | 2020 | `		ph7_result_bool(pCtx,0);` |
|    3 | 2021 | `		return PH7_OK;` |
|    - | 2022 | `	}` |
|  ! 0 | 2023 | `	zIn = ph7_value_to_string(apArg[0],&nIn);` |
|  ! 0 | 2024 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|  ! 0 | 2025 | `	if( ZlibEncodeBuf(pCtx->pVm,(const unsigned char *)zIn,(sxu32)nIn,iEnc,-1,` |
|  ! 0 | 2026 | `			&sOut,&iStatus) != 0 ){` |
|  ! 0 | 2027 | `		SyBlobRelease(&sOut);` |
|  ! 0 | 2028 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 2029 | `		return PH7_OK;` |
|    - | 2030 | `	}` |
|    - | 2031 | `	/* php's handler announces what it did, and adds the Vary that makes the` |
|    - | 2032 | `	 * answer cacheable per client. */` |
|  ! 0 | 2033 | `	PH7_VmAddResponseHeader(pCtx->pVm,"Content-Encoding",` |
|  ! 0 | 2034 | `		iEnc == PHL_Z_GZIP ? "gzip" : "deflate");` |
|  ! 0 | 2035 | `	PH7_VmAddResponseHeader(pCtx->pVm,"Vary","Accept-Encoding");` |
|  ! 0 | 2036 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|  ! 0 | 2037 | `	SyBlobRelease(&sOut);` |
|  ! 0 | 2038 | `	return PH7_OK;` |
|    2 | 2039 | `}` |
|    - | 2040 | `/* string\|false zlib_get_coding_type() */` |
|    2 | 2041 | `static int PH7_builtin_zlib_get_coding_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 2042 | `{` |
|    1 | 2043 | `	SXUNUSED(nArg);` |
|    1 | 2044 | `	SXUNUSED(apArg);` |
|    - | 2045 | `	/* php answers the coding the OUTPUT layer is compressing with, which is` |
|    - | 2046 | `	 * only ever set by zlib.output_compression -- off in this build's ini, as` |
|    - | 2047 | `	 * it is in php's own default. */` |
|    3 | 2048 | `	ph7_result_bool(pCtx,0);` |
|    3 | 2049 | `	return PH7_OK;` |
|    1 | 2050 | `}` |
|    - | 2051 | `/* ------------------------------------------------------------------ */` |
|    - | 2052 | `/* Registration                                                       */` |
|    - | 2053 | `/* ------------------------------------------------------------------ */` |
|    - | 2054 | `/*` |
|    - | 2055 | ` * The two context classes. php declares them final, uncloneable and` |
|    - | 2056 | `` * unserializable, with NO method and NO property, and refuses `new` with a`` |
|    - | 2057 | ` * sentence naming the factory -- exactly CurlHandle's shape.` |
|    - | 2058 | ` */` |
| 8445 | 2059 | `PH7_PRIVATE sxi32 PH7_VmInstallZlib(ph7_vm *pVm)` |
|    5 | 2060 | `{` |
|    - | 2061 | `	static const PH7_NativePropDef aProp[] = {` |
|    - | 2062 | `		{ ZCTX_SLOT, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN,` |
|    - | 2063 | `		  { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 }` |
|    - | 2064 | `	};` |
|    - | 2065 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|    - | 2066 | `		{ "DeflateContext", 0, 0,` |
|    - | 2067 | `		  PH7_CLASS_FINAL\|PH7_CLASS_NOINSTANTIATE\|PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|    - | 2068 | `		  0, 0, 0, 0, aProp, SX_ARRAYSIZE(aProp), ZctxInstanceRelease, 0, 0 },` |
|    - | 2069 | `		{ "InflateContext", 0, 0,` |
|    - | 2070 | `		  PH7_CLASS_FINAL\|PH7_CLASS_NOINSTANTIATE\|PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|    - | 2071 | `		  0, 0, 0, 0, aProp, SX_ARRAYSIZE(aProp), ZctxInstanceRelease, 0, 0 }` |
|    - | 2072 | `	};` |
|    - | 2073 | `	sxi32 rc;` |
| 8450 | 2074 | `	pVm->pZlibCtx = 0;` |
| 8450 | 2075 | `	pVm->iZlibLevel = -1;` |
| 8450 | 2076 | `	pVm->iZlibStrategy = Z_DEFAULT_STRATEGY;` |
| 8450 | 2077 | `	rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
| 8450 | 2078 | `	if( rc == SXRET_OK ){` |
| 8450 | 2079 | `		ph7_class *pClass = PH7_VmExtractClass(&(*pVm),"DeflateContext",` |
|    - | 2080 | `			sizeof("DeflateContext")-1,FALSE,0);` |
| 8450 | 2081 | `		if( pClass ){` |
| 8450 | 2082 | `			pClass->zNewRefusal =` |
|    - | 2083 | `				"Cannot directly construct DeflateContext, use deflate_init() instead";` |
| 8450 | 2084 | `			pClass->xCmp = PH7_NativeCmpOpaqueHandle;` |
| 4217 | 2085 | `		}` |
| 8450 | 2086 | `		pClass = PH7_VmExtractClass(&(*pVm),"InflateContext",` |
|    - | 2087 | `			sizeof("InflateContext")-1,FALSE,0);` |
| 8450 | 2088 | `		if( pClass ){` |
| 8450 | 2089 | `			pClass->zNewRefusal =` |
|    - | 2090 | `				"Cannot directly construct InflateContext, use inflate_init() instead";` |
| 8450 | 2091 | `			pClass->xCmp = PH7_NativeCmpOpaqueHandle;` |
| 4217 | 2092 | `		}` |
| 4217 | 2093 | `	}` |
| 8450 | 2094 | `	return rc;` |
|    5 | 2095 | `}` |
|    - | 2096 | `/* The functions this unit owns, in php's own registration order. The gz*` |
|    - | 2097 | ` * handle verbs are NOT here: php registers them as ALIASES of the ordinary` |
|    - | 2098 | ` * stream functions (which is why gzread() works on a plain fopen() handle and` |
|    - | 2099 | ` * fread() works on a gzopen() one), and builtin.c registers them that way. */` |
| 8445 | 2100 | `PH7_PRIVATE const ph7_builtin_func * PH7_ZlibFuncTable(sxu32 *pnEntry)` |
|    5 | 2101 | `{` |
|    - | 2102 | `	static const ph7_builtin_func aFunc[] = {` |
|    - | 2103 | `		{ "ob_gzhandler",          PH7_builtin_ob_gzhandler          },` |
|    - | 2104 | `		{ "zlib_get_coding_type",  PH7_builtin_zlib_get_coding_type  },` |
|    - | 2105 | `		{ "gzfile",                PH7_builtin_gzfile                },` |
|    - | 2106 | `		{ "gzopen",                PH7_builtin_gzopen                },` |
|    - | 2107 | `		{ "readgzfile",            PH7_builtin_readgzfile            },` |
|    - | 2108 | `		{ "zlib_encode",           PH7_builtin_zlib_encode           },` |
|    - | 2109 | `		{ "zlib_decode",           PH7_builtin_zlib_decode           },` |
|    - | 2110 | `		{ "gzdeflate",             PH7_builtin_gzdeflate             },` |
|    - | 2111 | `		{ "gzencode",              PH7_builtin_gzencode              },` |
|    - | 2112 | `		{ "gzcompress",            PH7_builtin_gzcompress            },` |
|    - | 2113 | `		{ "gzinflate",             PH7_builtin_gzinflate             },` |
|    - | 2114 | `		{ "gzdecode",              PH7_builtin_gzdecode              },` |
|    - | 2115 | `		{ "gzuncompress",          PH7_builtin_gzuncompress          },` |
|    - | 2116 | `		{ "deflate_init",          PH7_builtin_deflate_init          },` |
|    - | 2117 | `		{ "deflate_add",           PH7_builtin_deflate_add           },` |
|    - | 2118 | `		{ "inflate_init",          PH7_builtin_inflate_init          },` |
|    - | 2119 | `		{ "inflate_add",           PH7_builtin_inflate_add           },` |
|    - | 2120 | `		{ "inflate_get_status",    PH7_builtin_inflate_get_status    },` |
|    - | 2121 | `		{ "inflate_get_read_len",  PH7_builtin_inflate_get_read_len  }` |
|    - | 2122 | `	};` |
| 8450 | 2123 | `	*pnEntry = (sxu32)SX_ARRAYSIZE(aFunc);` |
| 8450 | 2124 | `	return aFunc;` |
|    5 | 2125 | `}` |
|    - | 2126 | `#endif /* PH7_ENABLE_ZLIB && !PH7_DISABLE_BUILTIN_FUNC */` |
|    - | 2127 |  |
