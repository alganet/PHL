# src/ph7/builtin_image.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1370/1531 lines (89.48%)

[Root index](../../index.md) | [Directory index](index.md)

|  Hits | Line | Source |
| ----: | ---: | :--- |
|     - |    1 | `/**` |
|     - |    2 | ` * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|     - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|     - |    4 | ` */` |
|     - |    5 | `#include "ph7int.h"` |
|     - |    6 | `#include <errno.h>` |
|     - |    7 | `#ifdef PH7_ENABLE_LIBXML` |
|     - |    8 | `#include <libxml/xmlreader.h>` |
|     - |    9 | `#endif` |
|     - |   10 | `/*` |
|     - |   11 | ` * Section:` |
|     - |   12 | ` *    ext/standard's image surface: the IMAGETYPE_* space, the two table` |
|     - |   13 | ` *    lookups over it (image_type_to_mime_type / image_type_to_extension) and` |
|     - |   14 | ` *    the container readers behind getimagesize()/getimagesizefromstring().` |
|     - |   15 | ` * Status:` |
|     - |   16 | ` *    Stable.` |
|     - |   17 | ` *` |
|     - |   18 | ` * php's image_type_to_* pair is one switch each, and the two tables do NOT` |
|     - |   19 | ` * agree on how many names they know. Three rules fall out of that and are easy` |
|     - |   20 | ` * to get backwards:` |
|     - |   21 | ` *` |
|     - |   22 | ` *   - the MIME lookup is total: every integer has an answer, and the one it` |
|     - |   23 | `` *     gives a type it does not know is `application/octet-stream` -- which is`` |
|     - |   24 | ` *     also the honest answer for JPC, JPX and JB2, three types it DOES know.` |
|     - |   25 | ` *     So a caller cannot tell "unknown type" from "raw codestream" by the mime` |
|     - |   26 | ` *     string alone.` |
|     - |   27 | ` *   - the EXTENSION lookup is partial: it answers FALSE for a type it does not` |
|     - |   28 | ` *     know, and it splits three names the mime table collapses into` |
|     - |   29 | `` *     `application/octet-stream` (`.jpc`, `.jpx`, `.jb2`) while collapsing one`` |
|     - |   30 | `` *     the mime table splits (WBMP is `image/vnd.wap.wbmp` and `.bmp`, BMP's`` |
|     - |   31 | ` *     own).` |
|     - |   32 | `` *   - `$include_dot` is not a branch: php stores each extension WITH its dot`` |
|     - |   33 | `` *     and answers `&imgext[!inc_dot]`, so the flag is a one-byte offset.`` |
|     - |   34 | ` *` |
|     - |   35 | ` * IMAGETYPE_SVG is not part of the fixed enum at all. php registers it at` |
|     - |   36 | ` * module init from ext/libxml -- against ext/standard's module number, so the` |
|     - |   37 | `` * constant belongs to `standard` even though the reader lives elsewhere -- and`` |
|     - |   38 | ` * bumps IMAGETYPE_COUNT as it does. A build without libxml therefore has` |
|     - |   39 | ` * neither the constant nor the extra count, which is what the guards below` |
|     - |   40 | ` * reproduce.` |
|     - |   41 | ` */` |
|     - |   42 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|     - |   43 | `/*` |
|     - |   44 | ` * The fixed part of php's image_filetype enum (ext/standard/php_image.h).` |
|     - |   45 | ` * IMAGETYPE_JPEG2000 is a userland ALIAS for JPC and not a value of its own,` |
|     - |   46 | ` * and IMAGETYPE_COUNT is the count rather than a type.` |
|     - |   47 | ` */` |
|     - |   48 | `#define PH7_IMG_UNKNOWN   0` |
|     - |   49 | `#define PH7_IMG_GIF       1` |
|     - |   50 | `#define PH7_IMG_JPEG      2` |
|     - |   51 | `#define PH7_IMG_PNG       3` |
|     - |   52 | `#define PH7_IMG_SWF       4` |
|     - |   53 | `#define PH7_IMG_PSD       5` |
|     - |   54 | `#define PH7_IMG_BMP       6` |
|     - |   55 | `#define PH7_IMG_TIFF_II   7` |
|     - |   56 | `#define PH7_IMG_TIFF_MM   8` |
|     - |   57 | `#define PH7_IMG_JPC       9` |
|     - |   58 | `#define PH7_IMG_JP2      10` |
|     - |   59 | `#define PH7_IMG_JPX      11` |
|     - |   60 | `#define PH7_IMG_JB2      12` |
|     - |   61 | `#define PH7_IMG_SWC      13` |
|     - |   62 | `#define PH7_IMG_IFF      14` |
|     - |   63 | `#define PH7_IMG_WBMP     15` |
|     - |   64 | `#define PH7_IMG_XBM      16` |
|     - |   65 | `#define PH7_IMG_ICO      17` |
|     - |   66 | `#define PH7_IMG_WEBP     18` |
|     - |   67 | `#define PH7_IMG_AVIF     19` |
|     - |   68 | `#define PH7_IMG_HEIF     20` |
|     - |   69 | `#ifdef PH7_ENABLE_LIBXML` |
|     - |   70 | `/* The one registered handler this build installs: php hands out the next id` |
|     - |   71 | ` * past the fixed enum, so SVG is 21 and IMAGETYPE_COUNT becomes 22. */` |
|     - |   72 | `#define PH7_IMG_SVG      21` |
|     - |   73 | `#endif` |
|     - |   74 | `/*` |
|     - |   75 | ` * php's mime table. A type with no row answers "application/octet-stream",` |
|     - |   76 | ` * which three rows below repeat on purpose (php words them that way).` |
|     - |   77 | ` */` |
|   294 |   78 | `PH7_PRIVATE const char * PH7_ImageTypeMime(ph7_int64 iType)` |
|     3 |   79 | `{` |
|   297 |   80 | `	switch( iType ){` |
|    24 |   81 | `	case PH7_IMG_GIF:     return "image/gif";` |
|    43 |   82 | `	case PH7_IMG_JPEG:    return "image/jpeg";` |
|    11 |   83 | `	case PH7_IMG_PNG:     return "image/png";` |
|     6 |   84 | `	case PH7_IMG_SWF:` |
|    14 |   85 | `	case PH7_IMG_SWC:     return "application/x-shockwave-flash";` |
|     9 |   86 | `	case PH7_IMG_PSD:     return "image/psd";` |
|    21 |   87 | `	case PH7_IMG_BMP:     return "image/bmp";` |
|     5 |   88 | `	case PH7_IMG_TIFF_II:` |
|    11 |   89 | `	case PH7_IMG_TIFF_MM: return "image/tiff";` |
|     9 |   90 | `	case PH7_IMG_IFF:     return "image/iff";` |
|     9 |   91 | `	case PH7_IMG_WBMP:    return "image/vnd.wap.wbmp";` |
|     7 |   92 | `	case PH7_IMG_JPC:     return "application/octet-stream";` |
|     5 |   93 | `	case PH7_IMG_JP2:     return "image/jp2";` |
|    19 |   94 | `	case PH7_IMG_XBM:     return "image/xbm";` |
|     9 |   95 | `	case PH7_IMG_ICO:     return "image/vnd.microsoft.icon";` |
|    11 |   96 | `	case PH7_IMG_WEBP:    return "image/webp";` |
|    55 |   97 | `	case PH7_IMG_AVIF:    return "image/avif";` |
|     9 |   98 | `	case PH7_IMG_HEIF:    return "image/heif";` |
|    23 |   99 | `	default:` |
|     - |  100 | `#ifdef PH7_ENABLE_LIBXML` |
|    47 |  101 | `		if( iType == PH7_IMG_SVG ){` |
|    31 |  102 | `			return "image/svg+xml";` |
|     - |  103 | `		}` |
|     - |  104 | `#endif` |
|    16 |  105 | `		break;` |
|     - |  106 | `	}` |
|    17 |  107 | `	return "application/octet-stream";` |
|   150 |  108 | `}` |
|     - |  109 | `/*` |
|     - |  110 | ` * php's extension table. Every string carries its leading dot; the answer for` |
|     - |  111 | ` * a type with no row is FALSE, which the caller spells rather than this.` |
|     - |  112 | ` */` |
|   118 |  113 | `static const char * ImageTypeExt(ph7_int64 iType)` |
|     2 |  114 | `{` |
|   120 |  115 | `	switch( iType ){` |
|    19 |  116 | `	case PH7_IMG_GIF:     return ".gif";` |
|     5 |  117 | `	case PH7_IMG_JPEG:    return ".jpeg";` |
|     5 |  118 | `	case PH7_IMG_PNG:     return ".png";` |
|     5 |  119 | `	case PH7_IMG_SWF:` |
|    12 |  120 | `	case PH7_IMG_SWC:     return ".swf";` |
|     5 |  121 | `	case PH7_IMG_PSD:     return ".psd";` |
|     4 |  122 | `	case PH7_IMG_BMP:` |
|     9 |  123 | `	case PH7_IMG_WBMP:    return ".bmp";` |
|     4 |  124 | `	case PH7_IMG_TIFF_II:` |
|     9 |  125 | `	case PH7_IMG_TIFF_MM: return ".tiff";` |
|     5 |  126 | `	case PH7_IMG_IFF:     return ".iff";` |
|     9 |  127 | `	case PH7_IMG_JPC:     return ".jpc";` |
|     5 |  128 | `	case PH7_IMG_JP2:     return ".jp2";` |
|     5 |  129 | `	case PH7_IMG_JPX:     return ".jpx";` |
|     5 |  130 | `	case PH7_IMG_JB2:     return ".jb2";` |
|     5 |  131 | `	case PH7_IMG_XBM:     return ".xbm";` |
|     5 |  132 | `	case PH7_IMG_ICO:     return ".ico";` |
|     5 |  133 | `	case PH7_IMG_WEBP:    return ".webp";` |
|     5 |  134 | `	case PH7_IMG_AVIF:    return ".avif";` |
|     5 |  135 | `	case PH7_IMG_HEIF:    return ".heif";` |
|     9 |  136 | `	default:` |
|     - |  137 | `#ifdef PH7_ENABLE_LIBXML` |
|    19 |  138 | `		if( iType == PH7_IMG_SVG ){` |
|     5 |  139 | `			return ".svg";` |
|     - |  140 | `		}` |
|     - |  141 | `#endif` |
|    14 |  142 | `		break;` |
|     - |  143 | `	}` |
|    15 |  144 | `	return 0;` |
|    61 |  145 | `}` |
|     - |  146 | `/*` |
|     - |  147 | ` * string image_type_to_mime_type(int $image_type)` |
|     - |  148 | ` */` |
|    58 |  149 | `PH7_PRIVATE int PH7_builtin_image_type_to_mime_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  150 | `{` |
|     - |  151 | `	const char *zMime;` |
|    29 |  152 | `	SXUNUSED(nArg); /* arity is enforced centrally */` |
|    60 |  153 | `	zMime = PH7_ImageTypeMime(ph7_value_to_int64(apArg[0]));` |
|    60 |  154 | `	ph7_result_string(pCtx,zMime,-1);` |
|    60 |  155 | `	return PH7_OK;` |
|     2 |  156 | `}` |
|     - |  157 | `/*` |
|     - |  158 | ` * string\|false image_type_to_extension(int $image_type,bool $include_dot = true)` |
|     - |  159 | ` */` |
|   118 |  160 | `PH7_PRIVATE int PH7_builtin_image_type_to_extension(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  161 | `{` |
|     - |  162 | `	const char *zExt;` |
|   120 |  163 | `	int bDot = TRUE;` |
|   120 |  164 | `	zExt = ImageTypeExt(ph7_value_to_int64(apArg[0]));` |
|   120 |  165 | `	if( zExt == 0 ){` |
|    15 |  166 | `		ph7_result_bool(pCtx,0);` |
|    15 |  167 | `		return PH7_OK;` |
|     - |  168 | `	}` |
|   106 |  169 | `	if( nArg > 1 ){` |
|    59 |  170 | `		bDot = ph7_value_to_bool(apArg[1]);` |
|    29 |  171 | `	}` |
|     - |  172 | `	/* php answers &imgext[!inc_dot]: the flag is an offset, not a branch. */` |
|   106 |  173 | `	ph7_result_string(pCtx,bDot ? zExt : &zExt[1],-1);` |
|   106 |  174 | `	return PH7_OK;` |
|    61 |  175 | `}` |
|     - |  176 | `/*` |
|     - |  177 | ` * ---------------------------------------------------------------------------` |
|     - |  178 | ` * The byte source every reader below works over.` |
|     - |  179 | ` * ---------------------------------------------------------------------------` |
|     - |  180 | ` * php reads an image through a php_stream, and getimagesize() opens one with` |
|     - |  181 | ` * STREAM_MUST_SEEK so every handler may seek freely. The two doors this engine` |
|     - |  182 | ` * has are an open vfs handle (getimagesize) and a script STRING` |
|     - |  183 | ` * (getimagesizefromstring), so ImgReader carries both behind one small` |
|     - |  184 | ` * read/seek/getc surface.` |
|     - |  185 | ` *` |
|     - |  186 | ` * Two properties of php's stream are contract here rather than convenience:` |
|     - |  187 | ` *   - a FORWARD seek always succeeds, even past the end. php's memory stream` |
|     - |  188 | ` *     says so outright and lseek(2) agrees, which is what makes php's own` |
|     - |  189 | `` *     `if (php_stream_seek(...)) return NULL` branches unreachable;`` |
|     - |  190 | ` *   - EOF is a STICKY flag raised by a read that could not be filled, not a` |
|     - |  191 | ` *     position test. Exactly one reader looks at it (JPEG-2000's component` |
|     - |  192 | ` *     count), and it looks to tell "the file says zero components" from "the` |
|     - |  193 | ` *     file ended where the count should be".` |
|     - |  194 | ` * A handle is read AHEAD into a small window: php's JPEG marker hunt walks a` |
|     - |  195 | ` * file one byte at a time, and a per-byte trip through the vfs would make a` |
|     - |  196 | ` * large corrupt JPEG quadratic in syscalls.` |
|     - |  197 | ` */` |
|     - |  198 | `#define IMG_BUFSZ 4096` |
|     - |  199 | `typedef struct ImgReader ImgReader;` |
|     - |  200 | `struct ImgReader {` |
|     - |  201 | `	ph7_context *pCtx;            /* whose name a read failure is reported under */` |
|     - |  202 | `	const ph7_io_stream *pStream; /* vfs backend, or 0 for the memory one */` |
|     - |  203 | `	void *pHandle;                /* the open vfs handle */` |
|     - |  204 | `	const unsigned char *zData;   /* memory backend: the whole string */` |
|     - |  205 | `	ph7_int64 nData;` |
|     - |  206 | `	ph7_int64 iPos;               /* absolute logical position */` |
|     - |  207 | `	ph7_int64 iRaw;               /* where the vfs handle itself stands */` |
|     - |  208 | `	ph7_int64 iBufBase;           /* absolute offset of zBuf[0] */` |
|     - |  209 | `	int nBuf;                     /* valid bytes in zBuf */` |
|     - |  210 | `	int bEof;                     /* a read could not be filled */` |
|     - |  211 | `	unsigned char zBuf[IMG_BUFSZ];` |
|     - |  212 | `};` |
|     - |  213 | `/*` |
|     - |  214 | ` * Fill the window so that it covers p->iPos. Answers how many bytes are` |
|     - |  215 | ` * available there (0 at the end of the handle).` |
|     - |  216 | ` */` |
|    14 |  217 | `static int ImgFill(ImgReader *p)` |
|     1 |  218 | `{` |
|     - |  219 | `	ph7_int64 n;` |
|    15 |  220 | `	if( p->iPos >= p->iBufBase && p->iPos < p->iBufBase + p->nBuf ){` |
|     7 |  221 | `		return (int)(p->iBufBase + p->nBuf - p->iPos);` |
|     - |  222 | `	}` |
|     9 |  223 | `	if( p->iRaw != p->iPos ){` |
|   ! 0 |  224 | `		if( p->pStream->xSeek == 0` |
|   ! 0 |  225 | `		 \|\| p->pStream->xSeek(p->pHandle,p->iPos,0/*SEEK_SET*/) != PH7_OK ){` |
|   ! 0 |  226 | `			p->nBuf = 0;` |
|   ! 0 |  227 | `			return 0;` |
|     - |  228 | `		}` |
|   ! 0 |  229 | `		p->iRaw = p->iPos;` |
|   ! 0 |  230 | `	}` |
|     9 |  231 | `	p->iBufBase = p->iPos;` |
|     9 |  232 | `	p->nBuf = 0;` |
|     9 |  233 | `	n = p->pStream->xRead(p->pHandle,p->zBuf,(ph7_int64)sizeof(p->zBuf));` |
|     9 |  234 | `	if( n < 0 ){` |
|     - |  235 | `		/* php's plain-file read op announces a device that refuses -- reading a` |
|     - |  236 | `		 * DIRECTORY is the everyday way to reach it here -- and the count it` |
|     - |  237 | `		 * reports is the stream CHUNK size php was filling, never what the` |
|     - |  238 | `		 * caller asked for. Silent for every other wrapper, as php's is. */` |
|   ! 0 |  239 | `		if( p->pStream == p->pCtx->pVm->pDefStream ){` |
|   ! 0 |  240 | `			ph7_context_throw_error_format(p->pCtx,PH7_CTX_NOTICE,` |
|     - |  241 | `				"Read of %u bytes failed with errno=%d %s",` |
|   ! 0 |  242 | `				8192u,errno,VfsStrerror(errno));` |
|   ! 0 |  243 | `		}` |
|   ! 0 |  244 | `		return 0;` |
|     - |  245 | `	}` |
|     9 |  246 | `	if( n > 0 ){` |
|     7 |  247 | `		p->nBuf = (int)n;` |
|     7 |  248 | `		p->iRaw += n;` |
|     3 |  249 | `	}` |
|     9 |  250 | `	return p->nBuf;` |
|     8 |  251 | `}` |
|     - |  252 | `/*` |
|     - |  253 | ` * Read up to nWant bytes. A short answer raises the sticky EOF flag, which is` |
|     - |  254 | ` * php's own rule: its stream marks eof when a read cannot be filled.` |
|     - |  255 | ` */` |
| 10376 |  256 | `static ph7_int64 ImgRead(ImgReader *p,void *pOut,ph7_int64 nWant)` |
|     3 |  257 | `{` |
| 10379 |  258 | `	unsigned char *zOut = (unsigned char *)pOut;` |
| 10379 |  259 | `	ph7_int64 nGot = 0;` |
| 10379 |  260 | `	if( nWant <= 0 ){` |
|     3 |  261 | `		return 0;` |
|     - |  262 | `	}` |
| 10377 |  263 | `	if( p->pStream == 0 ){` |
| 10363 |  264 | `		nGot = p->nData - p->iPos;` |
| 10363 |  265 | `		if( nGot < 0 ){` |
|    15 |  266 | `			nGot = 0;` |
|     7 |  267 | `		}` |
| 10363 |  268 | `		if( nGot > nWant ){` |
|  9669 |  269 | `			nGot = nWant;` |
|  4812 |  270 | `		}` |
| 10363 |  271 | `		if( nGot > 0 ){` |
| 10035 |  272 | `			SyMemcpy(&p->zData[p->iPos],zOut,(sxu32)nGot);` |
|  4995 |  273 | `		}` |
| 10363 |  274 | `		p->iPos += nGot;` |
|  5163 |  275 | `	}else{` |
|    27 |  276 | `		while( nGot < nWant ){` |
|    15 |  277 | `			int nAvail = ImgFill(p);` |
|     - |  278 | `			int nCopy;` |
|    15 |  279 | `			if( nAvail < 1 ){` |
|     3 |  280 | `				break;` |
|     - |  281 | `			}` |
|    13 |  282 | `			nCopy = (int)SXMIN((ph7_int64)nAvail,nWant - nGot);` |
|    13 |  283 | `			SyMemcpy(&p->zBuf[p->iPos - p->iBufBase],&zOut[nGot],(sxu32)nCopy);` |
|    13 |  284 | `			nGot += nCopy;` |
|    13 |  285 | `			p->iPos += nCopy;` |
|     1 |  286 | `		}` |
|     - |  287 | `	}` |
| 10377 |  288 | `	if( nGot < nWant ){` |
|   458 |  289 | `		p->bEof = TRUE;` |
|   229 |  290 | `	}` |
| 10377 |  291 | `	return nGot;` |
|  5171 |  292 | `}` |
|     - |  293 | `/* A forward seek never fails; a rewind clears the sticky EOF, as php's does. */` |
|   414 |  294 | `static void ImgSeekCur(ImgReader *p,ph7_int64 nOfft)` |
|     2 |  295 | `{` |
|   416 |  296 | `	p->iPos += nOfft;` |
|   416 |  297 | `	if( p->iPos < 0 ){` |
|   ! 0 |  298 | `		p->iPos = 0;` |
|   ! 0 |  299 | `	}` |
|   416 |  300 | `}` |
|   630 |  301 | `static void ImgRewind(ImgReader *p)` |
|     1 |  302 | `{` |
|   631 |  303 | `	p->iPos = 0;` |
|   631 |  304 | `	p->bEof = FALSE;` |
|   631 |  305 | `}` |
|     - |  306 | `/* php_stream_getc: the byte, or -1 at the end. */` |
|  6024 |  307 | `static int ImgGetc(ImgReader *p)` |
|     1 |  308 | `{` |
|     - |  309 | `	unsigned char c;` |
|  6025 |  310 | `	if( ImgRead(p,&c,1) != 1 ){` |
|   197 |  311 | `		return -1;` |
|     - |  312 | `	}` |
|  5829 |  313 | `	return (int)c;` |
|  3013 |  314 | `}` |
|     - |  315 | `/* php_read2/php_read4: big-endian, and ZERO when the bytes are not there. */` |
|   162 |  316 | `static unsigned int ImgRead2(ImgReader *p)` |
|     1 |  317 | `{` |
|     - |  318 | `	unsigned char a[2];` |
|   163 |  319 | `	if( ImgRead(p,a,2) != 2 ){` |
|    17 |  320 | `		return 0;` |
|     - |  321 | `	}` |
|   147 |  322 | `	return ((unsigned int)a[0] << 8) + (unsigned int)a[1];` |
|    82 |  323 | `}` |
|    16 |  324 | `static unsigned int ImgRead4(ImgReader *p)` |
|     1 |  325 | `{` |
|     - |  326 | `	unsigned char a[4];` |
|    17 |  327 | `	if( ImgRead(p,a,4) != 4 ){` |
|     3 |  328 | `		return 0;` |
|     - |  329 | `	}` |
|    22 |  330 | `	return ((unsigned int)a[0] << 24) + ((unsigned int)a[1] << 16)` |
|    14 |  331 | `	     + ((unsigned int)a[2] << 8)  + (unsigned int)a[3];` |
|     9 |  332 | `}` |
|     - |  333 | `/*` |
|     - |  334 | ` * What a reader answers -- php's php_gfxinfo, with the same "0 means absent"` |
|     - |  335 | `` * convention for `bits` and `channels`: a format with no answer for one of`` |
|     - |  336 | ` * them leaves it zero and the key never reaches the array. The two UNIT` |
|     - |  337 | ` * strings are php 8.5's, and only a registered handler ever sets them.` |
|     - |  338 | ` */` |
|     - |  339 | `typedef struct ImgInfo ImgInfo;` |
|     - |  340 | `struct ImgInfo {` |
|     - |  341 | `	unsigned int nWidth;` |
|     - |  342 | `	unsigned int nHeight;` |
|     - |  343 | `	unsigned int nBits;` |
|     - |  344 | `	unsigned int nChannels;` |
|     - |  345 | `	const char *zWidthUnit;   /* 0 == php's "px" default */` |
|     - |  346 | `	const char *zHeightUnit;` |
|     - |  347 | `};` |
|     - |  348 | `/* One diagnostic door, so every sentence below carries php's docref prefix. */` |
|    40 |  349 | `static void ImgThrowFmt(ph7_context *pCtx,sxi32 iErr,const char *zFmt,...)` |
|     3 |  350 | `{` |
|     - |  351 | `	va_list ap;` |
|    43 |  352 | `	va_start(ap,zFmt);` |
|    43 |  353 | `	PH7_VmThrowErrorAp(pCtx->pVm,0,iErr,zFmt,ap);` |
|    43 |  354 | `	va_end(ap);` |
|    43 |  355 | `}` |
|     - |  356 | `/*` |
|     - |  357 | ` * GIF. The packed byte's high bit says whether there is a global colour` |
|     - |  358 | ` * table at all, and only then do its low three bits mean a depth; the three` |
|     - |  359 | ` * channels are php's own constant and not something the file says.` |
|     - |  360 | ` */` |
|    22 |  361 | `static int ImgHandleGif(ImgReader *p,ImgInfo *pOut)` |
|     2 |  362 | `{` |
|     - |  363 | `	unsigned char dim[5];` |
|    24 |  364 | `	ImgSeekCur(p,3);` |
|    24 |  365 | `	if( ImgRead(p,dim,sizeof(dim)) != (ph7_int64)sizeof(dim) ){` |
|     3 |  366 | `		return 0;` |
|     - |  367 | `	}` |
|    22 |  368 | `	pOut->nWidth    = (unsigned int)dim[0] \| ((unsigned int)dim[1] << 8);` |
|    22 |  369 | `	pOut->nHeight   = (unsigned int)dim[2] \| ((unsigned int)dim[3] << 8);` |
|    22 |  370 | `	pOut->nBits     = (dim[4] & 0x80) ? (((unsigned int)dim[4] & 0x07) + 1) : 0;` |
|    22 |  371 | `	pOut->nChannels = 3; /* always */` |
|    22 |  372 | `	return 1;` |
|    13 |  373 | `}` |
|     - |  374 | `/* PSD. Its header spells the HEIGHT first. */` |
|     8 |  375 | `static int ImgHandlePsd(ImgReader *p,ImgInfo *pOut)` |
|     1 |  376 | `{` |
|     - |  377 | `	unsigned char dim[8];` |
|     9 |  378 | `	ImgSeekCur(p,11);` |
|     9 |  379 | `	if( ImgRead(p,dim,sizeof(dim)) != (ph7_int64)sizeof(dim) ){` |
|     3 |  380 | `		return 0;` |
|     - |  381 | `	}` |
|    10 |  382 | `	pOut->nHeight = ((unsigned int)dim[0] << 24) + ((unsigned int)dim[1] << 16)` |
|     6 |  383 | `	              + ((unsigned int)dim[2] << 8)  + (unsigned int)dim[3];` |
|    10 |  384 | `	pOut->nWidth  = ((unsigned int)dim[4] << 24) + ((unsigned int)dim[5] << 16)` |
|     6 |  385 | `	              + ((unsigned int)dim[6] << 8)  + (unsigned int)dim[7];` |
|     7 |  386 | `	return 1;` |
|     5 |  387 | `}` |
|     - |  388 | `/*` |
|     - |  389 | ` * BMP. The DIB header's own SIZE decides which of two layouts to read, and` |
|     - |  390 | ` * php accepts exactly three shapes: the 12-byte OS/2 core header, anything` |
|     - |  391 | ` * from 13 to 64 bytes, and the two Windows extensions (108 and 124). A` |
|     - |  392 | ` * height read from the modern layout is SIGNED -- negative means top-down --` |
|     - |  393 | ` * and php takes its absolute value, so the one height no int32 can negate` |
|     - |  394 | ` * (0x80000000) comes back unchanged. That is spelled in unsigned arithmetic` |
|     - |  395 | ` * below because the signed spelling is undefined behaviour php gets away` |
|     - |  396 | ` * with; the ANSWER is php's.` |
|     - |  397 | ` */` |
|    26 |  398 | `static int ImgHandleBmp(ImgReader *p,ImgInfo *pOut)` |
|     1 |  399 | `{` |
|     - |  400 | `	unsigned char dim[16];` |
|     - |  401 | `	unsigned int nSize,nHeight;` |
|    27 |  402 | `	ImgSeekCur(p,11);` |
|    27 |  403 | `	if( ImgRead(p,dim,sizeof(dim)) != (ph7_int64)sizeof(dim) ){` |
|   ! 0 |  404 | `		return 0;` |
|     - |  405 | `	}` |
|    40 |  406 | `	nSize = ((unsigned int)dim[3] << 24) + ((unsigned int)dim[2] << 16)` |
|    26 |  407 | `	      + ((unsigned int)dim[1] << 8)  + (unsigned int)dim[0];` |
|    27 |  408 | `	if( nSize == 12 ){` |
|     5 |  409 | `		pOut->nWidth  = ((unsigned int)dim[5] << 8) + (unsigned int)dim[4];` |
|     5 |  410 | `		pOut->nHeight = ((unsigned int)dim[7] << 8) + (unsigned int)dim[6];` |
|     5 |  411 | `		pOut->nBits   = (unsigned int)dim[11];` |
|     5 |  412 | `		return 1;` |
|     - |  413 | `	}` |
|    23 |  414 | `	if( nSize > 12 && (nSize <= 64 \|\| nSize == 108 \|\| nSize == 124) ){` |
|    22 |  415 | `		pOut->nWidth = ((unsigned int)dim[7] << 24) + ((unsigned int)dim[6] << 16)` |
|    14 |  416 | `		             + ((unsigned int)dim[5] << 8)  + (unsigned int)dim[4];` |
|    22 |  417 | `		nHeight      = ((unsigned int)dim[11] << 24) + ((unsigned int)dim[10] << 16)` |
|    14 |  418 | `		             + ((unsigned int)dim[9] << 8)  + (unsigned int)dim[8];` |
|    15 |  419 | `		if( nHeight & 0x80000000u ){` |
|     - |  420 | `			/* abs() of the 32-bit signed reading: the two's complement negation,` |
|     - |  421 | `			 * which leaves 0x80000000 exactly where it was. */` |
|     5 |  422 | `			nHeight = (unsigned int)(0u - nHeight);` |
|     2 |  423 | `		}` |
|    15 |  424 | `		pOut->nHeight = nHeight;` |
|    15 |  425 | `		pOut->nBits   = ((unsigned int)dim[15] << 8) + (unsigned int)dim[14];` |
|    15 |  426 | `		return 1;` |
|     - |  427 | `	}` |
|     9 |  428 | `	return 0;` |
|    14 |  429 | `}` |
|     - |  430 | `/*` |
|     - |  431 | ` * SWF. The frame RECT is a bit field: five bits of WIDTH, then four values` |
|     - |  432 | ` * of that width, in twips (a twentieth of a pixel).` |
|     - |  433 | ` */` |
|    30 |  434 | `static unsigned long ImgSwfBits(const unsigned char *zBuf,unsigned int nPos,unsigned int nCount)` |
|     1 |  435 | `{` |
|    31 |  436 | `	unsigned long nRes = 0;` |
|     - |  437 | `	unsigned int i;` |
|   301 |  438 | `	for( i = nPos ; i < nPos + nCount ; ++i ){` |
|   406 |  439 | `		nRes = nRes +` |
|   270 |  440 | `			((unsigned long)((zBuf[i / 8] >> (7 - (i % 8))) & 0x01) << (nCount - (i - nPos) - 1));` |
|   136 |  441 | `	}` |
|    31 |  442 | `	return nRes;` |
|     1 |  443 | `}` |
|     8 |  444 | `static int ImgHandleSwf(ImgReader *p,ImgInfo *pOut)` |
|     1 |  445 | `{` |
|     - |  446 | `	unsigned char a[32];` |
|     - |  447 | `	unsigned long nBits;` |
|     9 |  448 | `	ImgSeekCur(p,5);` |
|     9 |  449 | `	if( ImgRead(p,a,sizeof(a)) != (ph7_int64)sizeof(a) ){` |
|     3 |  450 | `		return 0;` |
|     - |  451 | `	}` |
|     7 |  452 | `	nBits = ImgSwfBits(a,0,5);` |
|    10 |  453 | `	pOut->nWidth  = (unsigned int)((ImgSwfBits(a,5 + (unsigned int)nBits,(unsigned int)nBits)` |
|     6 |  454 | `		- ImgSwfBits(a,5,(unsigned int)nBits)) / 20);` |
|    10 |  455 | `	pOut->nHeight = (unsigned int)((ImgSwfBits(a,5 + 3 * (unsigned int)nBits,(unsigned int)nBits)` |
|     6 |  456 | `		- ImgSwfBits(a,5 + 2 * (unsigned int)nBits,(unsigned int)nBits)) / 20);` |
|     7 |  457 | `	return 1;` |
|     5 |  458 | `}` |
|     - |  459 | `/* PNG. IHDR is fixed and first, so nine bytes past the signature is all of it. */` |
|     8 |  460 | `static int ImgHandlePng(ImgReader *p,ImgInfo *pOut)` |
|     1 |  461 | `{` |
|     - |  462 | `	unsigned char dim[9];` |
|     9 |  463 | `	ImgSeekCur(p,8);` |
|     9 |  464 | `	if( ImgRead(p,dim,sizeof(dim)) < (ph7_int64)sizeof(dim) ){` |
|   ! 0 |  465 | `		return 0;` |
|     - |  466 | `	}` |
|    13 |  467 | `	pOut->nWidth  = ((unsigned int)dim[0] << 24) + ((unsigned int)dim[1] << 16)` |
|     8 |  468 | `	              + ((unsigned int)dim[2] << 8)  + (unsigned int)dim[3];` |
|    13 |  469 | `	pOut->nHeight = ((unsigned int)dim[4] << 24) + ((unsigned int)dim[5] << 16)` |
|     8 |  470 | `	              + ((unsigned int)dim[6] << 8)  + (unsigned int)dim[7];` |
|     9 |  471 | `	pOut->nBits   = (unsigned int)dim[8];` |
|     9 |  472 | `	return 1;` |
|     5 |  473 | `}` |
|     - |  474 | `/*` |
|     - |  475 | ` * ---------------------------------------------------------------------------` |
|     - |  476 | ` * JPEG: a marker walk, and the only reader with a second answer.` |
|     - |  477 | ` * ---------------------------------------------------------------------------` |
|     - |  478 | ` * Every JPEG segment is 0xFF, a marker byte, then a big-endian length that` |
|     - |  479 | ` * COUNTS ITSELF. Three rules of php's walk are worth spelling out because` |
|     - |  480 | ` * each is visible from PHP:` |
|     - |  481 | ` *   - the first marker is read with the 0xFF already in hand (the detection` |
|     - |  482 | ` *     ladder consumed it), so the "extraneous bytes before marker" warning` |
|     - |  483 | ` *     can only ever come from the SECOND marker onwards;` |
|     - |  484 | ` *   - only the FIRST frame header answers. A second SOFn is skipped like any` |
|     - |  485 | ` *     other segment, so a progressive file's later frames never move the size;` |
|     - |  486 | ` *   - the APP payloads are collected only when the caller asked for` |
|     - |  487 | ` *     $image_info, and only the FIRST of each APPn number is kept. Without` |
|     - |  488 | ` *     that argument php returns at the frame header and never reads them at` |
|     - |  489 | ` *     all.` |
|     - |  490 | ` */` |
|     - |  491 | `#define IMG_M_EOI   0xD9  /* end of image */` |
|     - |  492 | `#define IMG_M_SOS   0xDA  /* start of scan: the compressed data begins */` |
|     - |  493 | `#define IMG_M_APP0  0xE0` |
|     - |  494 | `#define IMG_M_APP15 0xEF` |
|     - |  495 | `/* Is this marker one of the thirteen frame headers php reads a size from?` |
|     - |  496 | ` * Three markers sit inside the C0..CF run and are NOT frame headers -- C4` |
|     - |  497 | ` * (define Huffman table), C8 (reserved) and CC (define arithmetic coding) --` |
|     - |  498 | ` * which is what the three holes below are. */` |
|   108 |  499 | `static int ImgJpegIsSof(unsigned int m)` |
|     1 |  500 | `{` |
|   109 |  501 | `	return (m >= 0xC0 && m <= 0xCF) && m != 0xC4 && m != 0xC8 && m != 0xCC;` |
|     1 |  502 | `}` |
|     - |  503 | `/*` |
|     - |  504 | `` * The next marker byte. `bFfRead` says the leading 0xFF is already consumed.`` |
|     - |  505 | ` * A run of 0xFF bytes is padding between the fill byte and the marker, and` |
|     - |  506 | ` * anything before the first 0xFF is counted and reported.` |
|     - |  507 | ` */` |
|   108 |  508 | `static unsigned int ImgJpegNextMarker(ph7_context *pCtx,ImgReader *p,int bFfRead)` |
|     1 |  509 | `{` |
|     - |  510 | `	int c;` |
|   109 |  511 | `	if( !bFfRead ){` |
|    61 |  512 | `		sxi64 nExtra = 0;` |
|   110 |  513 | `		for(;;){` |
|   221 |  514 | `			c = ImgGetc(p);` |
|   221 |  515 | `			if( c < 0 ){` |
|    31 |  516 | `				return IMG_M_EOI; /* we hit the end */` |
|     - |  517 | `			}` |
|   191 |  518 | `			if( c == 0xFF ){` |
|    31 |  519 | `				break;` |
|     - |  520 | `			}` |
|   161 |  521 | `			nExtra++;` |
|     1 |  522 | `		}` |
|    31 |  523 | `		if( nExtra > 0 ){` |
|     4 |  524 | `			ImgThrowFmt(pCtx,PH7_CTX_WARNING,` |
|     - |  525 | `				"%s(): Corrupt JPEG data: %qd extraneous bytes before marker",` |
|     1 |  526 | `				ph7_function_name(pCtx),nExtra);` |
|     1 |  527 | `		}` |
|    15 |  528 | `	}` |
|    39 |  529 | `	do {` |
|    81 |  530 | `		c = ImgGetc(p);` |
|    81 |  531 | `		if( c < 0 ){` |
|   ! 0 |  532 | `			return IMG_M_EOI;` |
|     - |  533 | `		}` |
|    81 |  534 | `	}while( c == 0xFF );` |
|    79 |  535 | `	return (unsigned int)c;` |
|    55 |  536 | `}` |
|     - |  537 | `/* Skip a segment whose length word is where it should be. A length below two` |
|     - |  538 | ` * cannot even cover itself, which is php's "stop here". */` |
|    10 |  539 | `static int ImgJpegSkipSegment(ImgReader *p)` |
|     1 |  540 | `{` |
|    11 |  541 | `	unsigned int nLen = ImgRead2(p);` |
|    11 |  542 | `	if( nLen < 2 ){` |
|   ! 0 |  543 | `		return 0;` |
|     - |  544 | `	}` |
|    11 |  545 | `	ImgSeekCur(p,(ph7_int64)(nLen - 2));` |
|    11 |  546 | `	return 1;` |
|     6 |  547 | `}` |
|     - |  548 | `/* Collect one APPn payload under php's own "APP%d" key. */` |
|    20 |  549 | `static int ImgJpegReadApp(ph7_context *pCtx,ImgReader *p,unsigned int nMarker,ph7_value *pInfo)` |
|     1 |  550 | `{` |
|    21 |  551 | `	unsigned int nLen = ImgRead2(p);` |
|     - |  552 | `	char zKey[16];` |
|     - |  553 | `	char *zBuf;` |
|    21 |  554 | `	int rc = 1;` |
|    21 |  555 | `	if( nLen < 2 ){` |
|     3 |  556 | `		return 0;` |
|     - |  557 | `	}` |
|    19 |  558 | `	nLen -= 2; /* the length counts itself */` |
|    19 |  559 | `	zBuf = (char *)ph7_context_alloc_chunk(pCtx,nLen > 0 ? nLen : 1,FALSE,FALSE);` |
|    19 |  560 | `	if( zBuf == 0 ){` |
|   ! 0 |  561 | `		return 0;` |
|     - |  562 | `	}` |
|    19 |  563 | `	if( ImgRead(p,zBuf,(ph7_int64)nLen) != (ph7_int64)nLen ){` |
|     3 |  564 | `		ph7_context_free_chunk(pCtx,zBuf);` |
|     3 |  565 | `		return 0;` |
|     - |  566 | `	}` |
|    17 |  567 | `	SyBufferFormat(zKey,sizeof(zKey),"APP%u",nMarker - IMG_M_APP0);` |
|    17 |  568 | `	if( ph7_array_fetch(pInfo,zKey,-1) == 0 ){` |
|     - |  569 | `		/* php keeps only the FIRST tag of each kind. */` |
|    15 |  570 | `		ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|    15 |  571 | `		if( pVal ){` |
|    15 |  572 | `			ph7_value_string(pVal,zBuf,(int)nLen);` |
|    15 |  573 | `			ph7_array_add_strkey_elem(pInfo,zKey,pVal);` |
|    15 |  574 | `			ph7_context_release_value(pCtx,pVal);` |
|     8 |  575 | `		}else{` |
|   ! 0 |  576 | `			rc = 0;` |
|     - |  577 | `		}` |
|     7 |  578 | `	}` |
|    17 |  579 | `	ph7_context_free_chunk(pCtx,zBuf);` |
|    17 |  580 | `	return rc;` |
|    11 |  581 | `}` |
|    48 |  582 | `static int ImgHandleJpeg(ph7_context *pCtx,ImgReader *p,ImgInfo *pOut,ph7_value *pInfo)` |
|     1 |  583 | `{` |
|     - |  584 | `	unsigned int nMarker;` |
|    49 |  585 | `	int bFfRead = 1,bHave = 0;` |
|    54 |  586 | `	for(;;){` |
|   109 |  587 | `		nMarker = ImgJpegNextMarker(pCtx,p,bFfRead);` |
|   109 |  588 | `		bFfRead = 0;` |
|   109 |  589 | `		if( ImgJpegIsSof(nMarker) ){` |
|    43 |  590 | `			if( !bHave ){` |
|    41 |  591 | `				unsigned int nLen = ImgRead2(p);` |
|    41 |  592 | `				bHave = 1;` |
|    41 |  593 | `				pOut->nBits     = (unsigned int)ImgGetc(p);` |
|    41 |  594 | `				pOut->nHeight   = ImgRead2(p);` |
|    41 |  595 | `				pOut->nWidth    = ImgRead2(p);` |
|    41 |  596 | `				pOut->nChannels = (unsigned int)ImgGetc(p);` |
|    41 |  597 | `				if( pInfo == 0 \|\| nLen < 8 ){` |
|     7 |  598 | `					return 1;` |
|     - |  599 | `				}` |
|    35 |  600 | `				ImgSeekCur(p,(ph7_int64)(nLen - 8));` |
|    20 |  601 | `			}else if( !ImgJpegSkipSegment(p) ){` |
|   ! 0 |  602 | `				return bHave;` |
|     1 |  603 | `			}` |
|    85 |  604 | `		}else if( nMarker >= IMG_M_APP0 && nMarker <= IMG_M_APP15 ){` |
|    23 |  605 | `			if( pInfo ){` |
|    21 |  606 | `				if( !ImgJpegReadApp(pCtx,p,nMarker,pInfo) ){` |
|     5 |  607 | `					return bHave;` |
|     1 |  608 | `				}` |
|    11 |  609 | `			}else if( !ImgJpegSkipSegment(p) ){` |
|   ! 0 |  610 | `				return bHave;` |
|     1 |  611 | `			}` |
|    54 |  612 | `		}else if( nMarker == IMG_M_SOS \|\| nMarker == IMG_M_EOI ){` |
|     - |  613 | `			/* about to hit the image data, or at the end */` |
|    39 |  614 | `			return bHave;` |
|     7 |  615 | `		}else if( !ImgJpegSkipSegment(p) ){` |
|   ! 0 |  616 | `			return bHave;` |
|     - |  617 | `		}` |
|     1 |  618 | `	}` |
|    25 |  619 | `}` |
|     - |  620 | `/*` |
|     - |  621 | ` * ---------------------------------------------------------------------------` |
|     - |  622 | ` * JPEG 2000: the raw codestream, and the box wrapper around it.` |
|     - |  623 | ` * ---------------------------------------------------------------------------` |
|     - |  624 | ` * A JPEG-2000 codestream may give every component its own depth, so there is` |
|     - |  625 | ` * no single answer for "bit depth"; php reports the DEEPEST it saw. The one` |
|     - |  626 | ` * place it consults end-of-file rather than a value is the component count: a` |
|     - |  627 | ` * zero read at the end of a truncated file is a refusal, while a file that` |
|     - |  628 | ` * genuinely says zero components is a size with no depth.` |
|     - |  629 | ` */` |
|     - |  630 | `#define IMG_J2K_SIZ 0x51` |
|     8 |  631 | `static int ImgHandleJpc(ph7_context *pCtx,ImgReader *p,ImgInfo *pOut)` |
|     1 |  632 | `{` |
|     - |  633 | `	int iHighest,iDepth;` |
|     - |  634 | `	unsigned int i;` |
|     9 |  635 | `	if( ImgGetc(p) != IMG_J2K_SIZ ){` |
|     7 |  636 | `		ImgThrowFmt(pCtx,PH7_CTX_WARNING,` |
|     - |  637 | `			"%s(): JPEG2000 codestream corrupt(Expected SIZ marker not found after SOC)",` |
|     2 |  638 | `			ph7_function_name(pCtx));` |
|     5 |  639 | `		return 0;` |
|     - |  640 | `	}` |
|     5 |  641 | `	ImgRead2(p); /* Lsiz */` |
|     5 |  642 | `	ImgRead2(p); /* Rsiz */` |
|     5 |  643 | `	pOut->nWidth  = ImgRead4(p); /* Xsiz */` |
|     5 |  644 | `	pOut->nHeight = ImgRead4(p); /* Ysiz */` |
|     5 |  645 | `	ImgSeekCur(p,24);            /* the four offsets and the two tile sizes */` |
|     5 |  646 | `	pOut->nChannels = ImgRead2(p); /* Csiz */` |
|     5 |  647 | `	if( (pOut->nChannels == 0 && p->bEof) \|\| pOut->nChannels > 256 ){` |
|   ! 0 |  648 | `		return 0;` |
|     - |  649 | `	}` |
|     5 |  650 | `	iHighest = 0;` |
|    17 |  651 | `	for( i = 0 ; i < pOut->nChannels ; i++ ){` |
|    13 |  652 | `		iDepth = ImgGetc(p); /* Ssiz[i] */` |
|    13 |  653 | `		iDepth++;` |
|    13 |  654 | `		if( iDepth > iHighest ){` |
|     9 |  655 | `			iHighest = iDepth;` |
|     4 |  656 | `		}` |
|    13 |  657 | `		ImgGetc(p); /* XRsiz[i] */` |
|    13 |  658 | `		ImgGetc(p); /* YRsiz[i] */` |
|     7 |  659 | `	}` |
|     5 |  660 | `	pOut->nBits = (unsigned int)iHighest;` |
|     5 |  661 | `	return 1;` |
|     5 |  662 | `}` |
|     - |  663 | `/*` |
|     - |  664 | ` * JP2 is a BOX container and a file may hold several codestreams; php reads` |
|     - |  665 | ` * the first "jp2c" at the root and hands its three leading bytes to the raw` |
|     - |  666 | ` * reader as if they had been the file-type check.` |
|     - |  667 | ` */` |
|     6 |  668 | `static int ImgHandleJp2(ph7_context *pCtx,ImgReader *p,ImgInfo *pOut)` |
|     1 |  669 | `{` |
|     7 |  670 | `	int bHave = 0;` |
|     5 |  671 | `	for(;;){` |
|     - |  672 | `		unsigned int nLen;` |
|     - |  673 | `		unsigned char zType[4];` |
|     9 |  674 | `		nLen = ImgRead4(p);                    /* LBox */` |
|     9 |  675 | `		if( ImgRead(p,zType,4) != 4 ){         /* TBox */` |
|     3 |  676 | `			break;                             /* a general "out of stream" */` |
|     - |  677 | `		}` |
|     7 |  678 | `		if( nLen == 1 ){` |
|   ! 0 |  679 | `			return 0;                          /* XLBoxes are not handled */` |
|     - |  680 | `		}` |
|     7 |  681 | `		if( SyMemcmp(zType,"jp2c",4) == 0 ){` |
|     3 |  682 | `			ImgSeekCur(p,3); /* skip past where a file-type check would have read */` |
|     3 |  683 | `			bHave = ImgHandleJpc(pCtx,p,pOut);` |
|     3 |  684 | `			break;` |
|     - |  685 | `		}` |
|     5 |  686 | `		if( (int)nLen <= 0 ){` |
|     3 |  687 | `			break;                             /* that was the last box */` |
|     - |  688 | `		}` |
|     - |  689 | `		/* php seeks LBox-8 in UNSIGNED 32-bit arithmetic, so a box shorter than` |
|     - |  690 | `		 * its own header steps forward by nearly 4 GB rather than backwards. */` |
|     3 |  691 | `		ImgSeekCur(p,(ph7_int64)(sxu32)(nLen - 8));` |
|     1 |  692 | `	}` |
|     7 |  693 | `	if( !bHave ){` |
|     7 |  694 | `		ImgThrowFmt(pCtx,PH7_CTX_WARNING,"%s(): JP2 file has no codestreams at root level",` |
|     2 |  695 | `			ph7_function_name(pCtx));` |
|     2 |  696 | `	}` |
|     7 |  697 | `	return bHave;` |
|     4 |  698 | `}` |
|     - |  699 | `/*` |
|     - |  700 | ` * ---------------------------------------------------------------------------` |
|     - |  701 | ` * TIFF: the first image file directory, and nothing else.` |
|     - |  702 | ` * ---------------------------------------------------------------------------` |
|     - |  703 | ` * php walks the IFD's entries for the four size tags and answers only when it` |
|     - |  704 | ` * found BOTH a width and a height. The entry VALUE is read out of the twelve` |
|     - |  705 | ` * byte record itself, so only the five formats that fit inline are read and` |
|     - |  706 | ` * every other one is skipped -- an image whose size is stored out of line has` |
|     - |  707 | ` * no answer at all. A signed reading is widened to the platform word before it` |
|     - |  708 | ` * lands in the answer, so a negative width comes back as its unsigned 32-bit` |
|     - |  709 | ` * reading.` |
|     - |  710 | ` */` |
|     - |  711 | `#define IMG_TIFF_TAG_WIDTH       0x0100` |
|     - |  712 | `#define IMG_TIFF_TAG_HEIGHT      0x0101` |
|     - |  713 | `#define IMG_TIFF_TAG_COMP_WIDTH  0xA002` |
|     - |  714 | `#define IMG_TIFF_TAG_COMP_HEIGHT 0xA003` |
|    80 |  715 | `static unsigned int ImgIfdGet16u(const unsigned char *z,int bMotorola)` |
|     1 |  716 | `{` |
|    81 |  717 | `	return bMotorola ? (((unsigned int)z[0] << 8) \| z[1]) : (((unsigned int)z[1] << 8) \| z[0]);` |
|     1 |  718 | `}` |
|    16 |  719 | `static sxi32 ImgIfdGet16s(const unsigned char *z,int bMotorola)` |
|     1 |  720 | `{` |
|    17 |  721 | `	return (sxi32)(sxi16)ImgIfdGet16u(z,bMotorola);` |
|     1 |  722 | `}` |
|    48 |  723 | `static unsigned int ImgIfdGet32u(const unsigned char *z,int bMotorola)` |
|     1 |  724 | `{` |
|    49 |  725 | `	if( bMotorola ){` |
|    49 |  726 | `		return ((unsigned int)z[0] << 24) \| ((unsigned int)z[1] << 16)` |
|    32 |  727 | `		     \| ((unsigned int)z[2] << 8)  \| (unsigned int)z[3];` |
|     - |  728 | `	}` |
|    25 |  729 | `	return ((unsigned int)z[3] << 24) \| ((unsigned int)z[2] << 16)` |
|    16 |  730 | `	     \| ((unsigned int)z[1] << 8)  \| (unsigned int)z[0];` |
|    25 |  731 | `}` |
|    28 |  732 | `static sxi32 ImgIfdGet32s(const unsigned char *z,int bMotorola)` |
|     1 |  733 | `{` |
|    29 |  734 | `	return (sxi32)ImgIfdGet32u(z,bMotorola);` |
|     1 |  735 | `}` |
|    16 |  736 | `static int ImgHandleTiff(ph7_context *pCtx,ImgReader *p,ImgInfo *pOut,int bMotorola)` |
|     1 |  737 | `{` |
|     - |  738 | `	unsigned char zPtr[4];` |
|     - |  739 | `	unsigned char *zDir;` |
|     - |  740 | `	sxu32 nDirSize;` |
|     - |  741 | `	int i,nEntries;` |
|    17 |  742 | `	sxu64 nWidth = 0,nHeight = 0;` |
|    17 |  743 | `	if( ImgRead(p,zPtr,4) != 4 ){` |
|   ! 0 |  744 | `		return 0;` |
|     - |  745 | `	}` |
|     - |  746 | `	/* The IFD offset is absolute; php spells the move as a relative one from` |
|     - |  747 | `	 * the eight bytes it has read, which is the same place. */` |
|    17 |  748 | `	p->iPos = (ph7_int64)ImgIfdGet32u(zPtr,bMotorola);` |
|    17 |  749 | `	if( ImgRead(p,zPtr,2) != 2 ){` |
|   ! 0 |  750 | `		return 0;` |
|     - |  751 | `	}` |
|    17 |  752 | `	nEntries = (int)ImgIfdGet16u(zPtr,bMotorola);` |
|     - |  753 | `	/* 2 for the count, twelve per entry, four for the offset of the next IFD */` |
|    17 |  754 | `	nDirSize = 2 + 12 * (sxu32)nEntries + 4;` |
|    17 |  755 | `	zDir = (unsigned char *)ph7_context_alloc_chunk(pCtx,nDirSize,FALSE,FALSE);` |
|    17 |  756 | `	if( zDir == 0 ){` |
|   ! 0 |  757 | `		return 0;` |
|     - |  758 | `	}` |
|    17 |  759 | `	if( ImgRead(p,&zDir[2],(ph7_int64)(nDirSize - 2)) != (ph7_int64)(nDirSize - 2) ){` |
|     7 |  760 | `		ph7_context_free_chunk(pCtx,zDir);` |
|     7 |  761 | `		return 0;` |
|     - |  762 | `	}` |
|    29 |  763 | `	for( i = 0 ; i < nEntries ; i++ ){` |
|    19 |  764 | `		const unsigned char *zEntry = &zDir[2 + i * 12];` |
|    19 |  765 | `		unsigned int nTag  = ImgIfdGet16u(zEntry,bMotorola);` |
|    19 |  766 | `		unsigned int nType = ImgIfdGet16u(&zEntry[2],bMotorola);` |
|     - |  767 | `		sxu64 nValue;` |
|    19 |  768 | `		switch( nType ){` |
|   ! 0 |  769 | `		case 1:  /* BYTE  */` |
|     - |  770 | `		case 6:  /* SBYTE */` |
|   ! 0 |  771 | `			nValue = (sxu64)zEntry[8];` |
|   ! 0 |  772 | `			break;` |
|     6 |  773 | `		case 3:  /* USHORT */` |
|    13 |  774 | `			nValue = (sxu64)ImgIfdGet16u(&zEntry[8],bMotorola);` |
|    13 |  775 | `			break;` |
|   ! 0 |  776 | `		case 8:  /* SSHORT */` |
|   ! 0 |  777 | `			nValue = (sxu64)(sxi64)ImgIfdGet16s(&zEntry[8],bMotorola);` |
|   ! 0 |  778 | `			break;` |
|     2 |  779 | `		case 4:  /* ULONG */` |
|     5 |  780 | `			nValue = (sxu64)ImgIfdGet32u(&zEntry[8],bMotorola);` |
|     5 |  781 | `			break;` |
|   ! 0 |  782 | `		case 9:  /* SLONG */` |
|   ! 0 |  783 | `			nValue = (sxu64)(sxi64)ImgIfdGet32s(&zEntry[8],bMotorola);` |
|   ! 0 |  784 | `			break;` |
|     1 |  785 | `		default:` |
|     3 |  786 | `			continue; /* the value is not in the record */` |
|     - |  787 | `		}` |
|    17 |  788 | `		if( nTag == IMG_TIFF_TAG_WIDTH \|\| nTag == IMG_TIFF_TAG_COMP_WIDTH ){` |
|     9 |  789 | `			nWidth = nValue;` |
|    13 |  790 | `		}else if( nTag == IMG_TIFF_TAG_HEIGHT \|\| nTag == IMG_TIFF_TAG_COMP_HEIGHT ){` |
|     9 |  791 | `			nHeight = nValue;` |
|     4 |  792 | `		}` |
|     9 |  793 | `	}` |
|    11 |  794 | `	ph7_context_free_chunk(pCtx,zDir);` |
|    11 |  795 | `	if( nWidth && nHeight ){` |
|     7 |  796 | `		pOut->nWidth  = (unsigned int)nWidth;` |
|     7 |  797 | `		pOut->nHeight = (unsigned int)nHeight;` |
|     7 |  798 | `		return 1;` |
|     - |  799 | `	}` |
|     5 |  800 | `	return 0;` |
|     9 |  801 | `}` |
|     - |  802 | `/*` |
|     - |  803 | ` * IFF. FORM containers hold chunks, each PADDED to an even length, and the` |
|     - |  804 | ` * one php wants is BMHD. A BMHD whose dimensions are out of range does not` |
|     - |  805 | ` * end the walk: php reads its nine bytes, refuses them and loops again from` |
|     - |  806 | ` * wherever that left the file -- which is nine bytes into a chunk rather` |
|     - |  807 | ` * than at a chunk header, so the walk carries on misaligned rather than` |
|     - |  808 | ` * stopping. That is reproduced, not corrected.` |
|     - |  809 | ` */` |
|    12 |  810 | `static int ImgHandleIff(ImgReader *p,ImgInfo *pOut)` |
|     1 |  811 | `{` |
|     - |  812 | `	unsigned char a[10];` |
|    13 |  813 | `	if( ImgRead(p,a,8) != 8 ){` |
|   ! 0 |  814 | `		return 0;` |
|     - |  815 | `	}` |
|    13 |  816 | `	if( SyMemcmp(&a[4],"ILBM",4) != 0 && SyMemcmp(&a[4],"PBM ",4) != 0 ){` |
|     5 |  817 | `		return 0;` |
|     - |  818 | `	}` |
|    12 |  819 | `	for(;;){` |
|     - |  820 | `		sxi32 iChunk,iSize;` |
|    17 |  821 | `		if( ImgRead(p,a,8) != 8 ){` |
|     3 |  822 | `			return 0;` |
|     - |  823 | `		}` |
|    15 |  824 | `		iChunk = ImgIfdGet32s(&a[0],1);` |
|    15 |  825 | `		iSize  = ImgIfdGet32s(&a[4],1);` |
|    15 |  826 | `		if( iSize < 0 ){` |
|   ! 0 |  827 | `			return 0;` |
|     - |  828 | `		}` |
|    15 |  829 | `		if( (iSize & 1) == 1 ){` |
|     3 |  830 | `			if( iSize == SXI32_HIGH ){` |
|   ! 0 |  831 | `				return 0;` |
|     - |  832 | `			}` |
|     3 |  833 | `			iSize++;` |
|     1 |  834 | `		}` |
|    15 |  835 | `		if( iChunk == 0x424d4844 ){ /* "BMHD" */` |
|     - |  836 | `			sxi32 iW,iH,iBits;` |
|     9 |  837 | `			if( iSize < 9 \|\| ImgRead(p,a,9) != 9 ){` |
|   ! 0 |  838 | `				return 0;` |
|     - |  839 | `			}` |
|     9 |  840 | `			iW    = ImgIfdGet16s(&a[0],1);` |
|     9 |  841 | `			iH    = ImgIfdGet16s(&a[2],1);` |
|     9 |  842 | `			iBits = (sxi32)(a[8] & 0xFF);` |
|     9 |  843 | `			if( iW > 0 && iH > 0 && iBits > 0 && iBits < 33 ){` |
|     7 |  844 | `				pOut->nWidth  = (unsigned int)iW;` |
|     7 |  845 | `				pOut->nHeight = (unsigned int)iH;` |
|     7 |  846 | `				pOut->nBits   = (unsigned int)iBits;` |
|     7 |  847 | `				return 1;` |
|     - |  848 | `			}` |
|     2 |  849 | `		}else{` |
|     7 |  850 | `			ImgSeekCur(p,(ph7_int64)iSize);` |
|     - |  851 | `		}` |
|     1 |  852 | `	}` |
|     7 |  853 | `}` |
|     - |  854 | `/*` |
|     - |  855 | ` * WBMP has no signature at all, so it is the fallback the ladder reaches` |
|     - |  856 | ` * only once every real one has failed -- and its whole shape is the screen:` |
|     - |  857 | ` * a zero type byte, an extended-header run terminated by a clear high bit,` |
|     - |  858 | ` * then two seven-bit-per-byte counts. php bounds both at 2048, which is what` |
|     - |  859 | ` * keeps an arbitrary file from parsing as a huge bitmap.` |
|     - |  860 | ` */` |
|   116 |  861 | `static int ImgGetWbmp(ImgReader *p,ImgInfo *pOut)` |
|     1 |  862 | `{` |
|   117 |  863 | `	int i,nWidth = 0,nHeight = 0;` |
|   117 |  864 | `	ImgRewind(p);` |
|   117 |  865 | `	if( ImgGetc(p) != 0 ){          /* type */` |
|    89 |  866 | `		return 0;` |
|     - |  867 | `	}` |
|    14 |  868 | `	do {                            /* the extended header */` |
|    29 |  869 | `		i = ImgGetc(p);` |
|    29 |  870 | `		if( i < 0 ){` |
|   ! 0 |  871 | `			return 0;` |
|     - |  872 | `		}` |
|    29 |  873 | `	}while( i & 0x80 );` |
|    14 |  874 | `	do {` |
|    35 |  875 | `		i = ImgGetc(p);` |
|    35 |  876 | `		if( i < 0 ){` |
|   ! 0 |  877 | `			return 0;` |
|     - |  878 | `		}` |
|    35 |  879 | `		nWidth = (nWidth << 7) \| (i & 0x7F);` |
|    35 |  880 | `		if( nWidth > 2048 ){` |
|     3 |  881 | `			return 0;` |
|     - |  882 | `		}` |
|    33 |  883 | `	}while( i & 0x80 );` |
|    13 |  884 | `	do {` |
|    27 |  885 | `		i = ImgGetc(p);` |
|    27 |  886 | `		if( i < 0 ){` |
|   ! 0 |  887 | `			return 0;` |
|     - |  888 | `		}` |
|    27 |  889 | `		nHeight = (nHeight << 7) \| (i & 0x7F);` |
|    27 |  890 | `		if( nHeight > 2048 ){` |
|   ! 0 |  891 | `			return 0;` |
|     - |  892 | `		}` |
|    27 |  893 | `	}while( i & 0x80 );` |
|    27 |  894 | `	if( !nHeight \|\| !nWidth ){` |
|    15 |  895 | `		return 0;` |
|     - |  896 | `	}` |
|    13 |  897 | `	if( pOut ){` |
|     7 |  898 | `		pOut->nWidth  = (unsigned int)nWidth;` |
|     7 |  899 | `		pOut->nHeight = (unsigned int)nHeight;` |
|     3 |  900 | `	}` |
|    13 |  901 | `	return 1;` |
|    59 |  902 | `}` |
|     - |  903 | `/*` |
|     - |  904 | `` * The digits at z read as a C `int`, which is what both text formats below`` |
|     - |  905 | ` * store their size in. Two rules come from the platform's own strtol and are` |
|     - |  906 | ` * reproduced here rather than called for, so a Windows build answers what a` |
|     - |  907 | ` * POSIX one does: a run too long for a 64-bit signed value SATURATES at that` |
|     - |  908 | ` * bound (LONG_MAX going up, LONG_MIN going down) instead of wrapping, and` |
|     - |  909 | `` * what lands in the `int` is the low 32 bits of the result.`` |
|     - |  910 | ` */` |
|   130 |  911 | `static sxu32 ImgDigitsToU32(const char *z,int n,int bNeg)` |
|     1 |  912 | `{` |
|   131 |  913 | `	sxu64 nMag = 0;` |
|   131 |  914 | `	sxu64 nBound = (sxu64)SXI64_HIGH + (sxu64)(bNeg ? 1 : 0);` |
|   131 |  915 | `	int i,bOver = 0;` |
|   491 |  916 | `	for( i = 0 ; i < n && z[i] >= '0' && z[i] <= '9' ; i++ ){` |
|   361 |  917 | `		if( bOver ){` |
|     7 |  918 | `			continue;` |
|     - |  919 | `		}` |
|   355 |  920 | `		if( nMag > (nBound - (sxu64)(z[i] - '0')) / 10 ){` |
|     7 |  921 | `			bOver = 1;` |
|     4 |  922 | `		}else{` |
|   349 |  923 | `			nMag = nMag * 10 + (sxu64)(z[i] - '0');` |
|     - |  924 | `		}` |
|   178 |  925 | `	}` |
|   131 |  926 | `	if( bOver ){` |
|     7 |  927 | `		nMag = nBound;` |
|     3 |  928 | `	}` |
|   131 |  929 | `	return (sxu32)(bNeg ? (sxu64)(0 - nMag) : nMag);` |
|     1 |  930 | `}` |
|     - |  931 | `/*` |
|     - |  932 | ` * XBM is C source, so php reads it a LINE at a time and runs one sscanf` |
|     - |  933 | `` * pattern over each: `#define %s %d`. Four rules of that pattern are visible`` |
|     - |  934 | ` * from PHP and easy to lose:` |
|     - |  935 | `` *   - the `#` is a LITERAL, and a literal never skips whitespace, so a line`` |
|     - |  936 | ` *     indented at all matches nothing;` |
|     - |  937 | ` *   - the name is one non-whitespace RUN, and the part that counts is what` |
|     - |  938 | ` *     follows its LAST underscore -- a name with no underscore is compared` |
|     - |  939 | `` *     whole. Since only whitespace ends the run, `x_width\x013` is ONE token`` |
|     - |  940 | ` *     and the line has no number in it at all;` |
|     - |  941 | ` *   - the whole line is read as a C string, so an embedded NUL ends it;` |
|     - |  942 | `` *   - the value is a C `int` widened UNSIGNED, so a negative define comes`` |
|     - |  943 | ` *     back as its 32-bit reading. The scan below does its own arithmetic` |
|     - |  944 | ` *     rather than calling the platform's sscanf, whose overflow answer is its` |
|     - |  945 | ` *     own: this reproduces the saturate-at-the-64-bit-bound-then-truncate that` |
|     - |  946 | ` *     the oracle shows, so a Windows build answers what a POSIX one does.` |
|     - |  947 | ` */` |
|   898 |  948 | `static int ImgIsSpace(int c)` |
|     1 |  949 | `{` |
|   899 |  950 | `	return c == ' ' \|\| c == '\t' \|\| c == '\n' \|\| c == '\v' \|\| c == '\f' \|\| c == '\r';` |
|     1 |  951 | `}` |
|   156 |  952 | `static int ImgXbmScanDefine(const char *zLine,int nLine,const char **pzName,int *pnName,sxu32 *pnVal)` |
|     1 |  953 | `{` |
|   157 |  954 | `	int i,bNeg = 0;` |
|     - |  955 | `	/* sscanf() reads a C string: the line stops at its first NUL. */` |
|  3919 |  956 | `	for( i = 0 ; i < nLine ; i++ ){` |
|  3775 |  957 | `		if( zLine[i] == 0 ){` |
|    13 |  958 | `			nLine = i;` |
|    13 |  959 | `			break;` |
|     - |  960 | `		}` |
|  1882 |  961 | `	}` |
|   157 |  962 | `	if( nLine < 7 \|\| SyMemcmp(zLine,"#define",7) != 0 ){` |
|    81 |  963 | `		return 0;` |
|     - |  964 | `	}` |
|    77 |  965 | `	i = 7;` |
|   153 |  966 | `	while( i < nLine && ImgIsSpace(zLine[i]) ){` |
|    77 |  967 | `		i++;` |
|     1 |  968 | `	}` |
|    77 |  969 | `	*pzName = &zLine[i];` |
|   597 |  970 | `	while( i < nLine && !ImgIsSpace(zLine[i]) ){` |
|   521 |  971 | `		i++;` |
|     1 |  972 | `	}` |
|    77 |  973 | `	*pnName = (int)(&zLine[i] - *pzName);` |
|    77 |  974 | `	if( *pnName < 1 ){` |
|   ! 0 |  975 | `		return 0;` |
|     - |  976 | `	}` |
|   153 |  977 | `	while( i < nLine && ImgIsSpace(zLine[i]) ){` |
|    77 |  978 | `		i++;` |
|     1 |  979 | `	}` |
|    77 |  980 | `	if( i < nLine && (zLine[i] == '-' \|\| zLine[i] == '+') ){` |
|     5 |  981 | `		bNeg = (zLine[i] == '-');` |
|     5 |  982 | `		i++;` |
|     2 |  983 | `	}` |
|    77 |  984 | `	if( i >= nLine \|\| zLine[i] < '0' \|\| zLine[i] > '9' ){` |
|     3 |  985 | `		return 0;` |
|     - |  986 | `	}` |
|    75 |  987 | `	*pnVal = ImgDigitsToU32(&zLine[i],nLine - i,bNeg);` |
|    75 |  988 | `	return 1;` |
|    79 |  989 | `}` |
|   110 |  990 | `static int ImgGetXbm(ph7_context *pCtx,ImgReader *p,ImgInfo *pOut)` |
|     1 |  991 | `{` |
|     - |  992 | `	SyBlob sLine;` |
|   111 |  993 | `	unsigned int nWidth = 0,nHeight = 0;` |
|   111 |  994 | `	int bDone = 0;` |
|   111 |  995 | `	ImgRewind(p);` |
|   111 |  996 | `	SyBlobInit(&sLine,&pCtx->pVm->sAllocator);` |
|   267 |  997 | `	while( !bDone ){` |
|     - |  998 | `		const char *zName,*zType;` |
|     - |  999 | `		int nName,nType,k;` |
|     - | 1000 | `		sxu32 nVal;` |
|   235 | 1001 | `		SyBlobReset(&sLine);` |
|  5173 | 1002 | `		for(;;){` |
|     - | 1003 | `			char c;` |
|  5291 | 1004 | `			int iByte = ImgGetc(p);` |
|  5291 | 1005 | `			if( iByte < 0 ){` |
|   153 | 1006 | `				break;` |
|     - | 1007 | `			}` |
|  5139 | 1008 | `			c = (char)iByte;` |
|  5139 | 1009 | `			SyBlobAppend(&sLine,&c,1);` |
|  5139 | 1010 | `			if( c == '\n' ){` |
|    83 | 1011 | `				break;` |
|     - | 1012 | `			}` |
|     1 | 1013 | `		}` |
|   235 | 1014 | `		if( SyBlobLength(&sLine) < 1 ){` |
|    79 | 1015 | `			break; /* php_stream_gets() answered nothing: the file is done */` |
|     - | 1016 | `		}` |
|   157 | 1017 | `		if( !ImgXbmScanDefine((const char *)SyBlobData(&sLine),(int)SyBlobLength(&sLine),` |
|     - | 1018 | `			&zName,&nName,&nVal) ){` |
|    83 | 1019 | `			continue;` |
|     - | 1020 | `		}` |
|    75 | 1021 | `		zType = zName;` |
|    75 | 1022 | `		nType = nName;` |
|   457 | 1023 | `		for( k = nName ; k > 0 ; k-- ){` |
|   443 | 1024 | `			if( zName[k-1] == '_' ){` |
|    61 | 1025 | `				zType = &zName[k];` |
|    61 | 1026 | `				nType = nName - k;` |
|    61 | 1027 | `				break;` |
|     - | 1028 | `			}` |
|   192 | 1029 | `		}` |
|    75 | 1030 | `		if( nType == 5 && SyMemcmp(zType,"width",5) == 0 ){` |
|    33 | 1031 | `			nWidth = (unsigned int)nVal;` |
|    33 | 1032 | `			if( nHeight ){` |
|     5 | 1033 | `				bDone = 1;` |
|     2 | 1034 | `			}` |
|    16 | 1035 | `		}` |
|    75 | 1036 | `		if( nType == 6 && SyMemcmp(zType,"height",6) == 0 ){` |
|    37 | 1037 | `			nHeight = (unsigned int)nVal;` |
|    37 | 1038 | `			if( nWidth ){` |
|    29 | 1039 | `				bDone = 1;` |
|    14 | 1040 | `			}` |
|    18 | 1041 | `		}` |
|     1 | 1042 | `	}` |
|   111 | 1043 | `	SyBlobRelease(&sLine);` |
|   111 | 1044 | `	if( nWidth && nHeight ){` |
|    33 | 1045 | `		if( pOut ){` |
|    17 | 1046 | `			pOut->nWidth  = nWidth;` |
|    17 | 1047 | `			pOut->nHeight = nHeight;` |
|     8 | 1048 | `		}` |
|    33 | 1049 | `		return 1;` |
|     - | 1050 | `	}` |
|    79 | 1051 | `	return 0;` |
|    56 | 1052 | `}` |
|     - | 1053 | `/*` |
|     - | 1054 | ` * ICO holds a DIRECTORY of images and php answers for the one with the` |
|     - | 1055 | ` * greatest colour depth, ties going to the LAST it read. The two dimension` |
|     - | 1056 | ` * bytes are single bytes, and zero means 256 -- which is what makes a` |
|     - | 1057 | ` * 256-pixel icon the only one whose stored size is smaller than its answer.` |
|     - | 1058 | ` */` |
|    12 | 1059 | `static int ImgHandleIco(ImgReader *p,ImgInfo *pOut)` |
|     1 | 1060 | `{` |
|     - | 1061 | `	unsigned char dim[16];` |
|     - | 1062 | `	int nIcons;` |
|    13 | 1063 | `	if( ImgRead(p,dim,2) != 2 ){` |
|   ! 0 | 1064 | `		return 0;` |
|     - | 1065 | `	}` |
|    13 | 1066 | `	nIcons = (int)(((unsigned int)dim[1] << 8) + (unsigned int)dim[0]);` |
|    13 | 1067 | `	if( nIcons < 1 \|\| nIcons > 255 ){` |
|     7 | 1068 | `		return 0;` |
|     - | 1069 | `	}` |
|    17 | 1070 | `	while( nIcons > 0 ){` |
|     - | 1071 | `		unsigned int nBits;` |
|    11 | 1072 | `		if( ImgRead(p,dim,sizeof(dim)) != (ph7_int64)sizeof(dim) ){` |
|   ! 0 | 1073 | `			break;` |
|     - | 1074 | `		}` |
|    11 | 1075 | `		nBits = ((unsigned int)dim[7] << 8) + (unsigned int)dim[6];` |
|    11 | 1076 | `		if( nBits >= pOut->nBits ){` |
|     9 | 1077 | `			pOut->nWidth  = (unsigned int)dim[0];` |
|     9 | 1078 | `			pOut->nHeight = (unsigned int)dim[1];` |
|     9 | 1079 | `			pOut->nBits   = nBits;` |
|     4 | 1080 | `		}` |
|    11 | 1081 | `		nIcons--;` |
|     1 | 1082 | `	}` |
|     7 | 1083 | `	if( pOut->nWidth == 0 ){` |
|     3 | 1084 | `		pOut->nWidth = 256;` |
|     1 | 1085 | `	}` |
|     7 | 1086 | `	if( pOut->nHeight == 0 ){` |
|     3 | 1087 | `		pOut->nHeight = 256;` |
|     1 | 1088 | `	}` |
|     7 | 1089 | `	return 1;` |
|     7 | 1090 | `}` |
|     - | 1091 | `/*` |
|     - | 1092 | ` * WEBP: three container flavours behind one RIFF chunk, each spelling its` |
|     - | 1093 | ` * size differently -- fourteen bits per axis for the lossy bitstream, a` |
|     - | 1094 | ` * packed 14/14 for the lossless one, and 24 bits MINUS ONE for the extended` |
|     - | 1095 | ` * header. All three are one byte per sample, which php states rather than` |
|     - | 1096 | ` * reads.` |
|     - | 1097 | ` */` |
|    10 | 1098 | `static int ImgHandleWebp(ImgReader *p,ImgInfo *pOut)` |
|     1 | 1099 | `{` |
|     - | 1100 | `	unsigned char zBuf[18];` |
|    11 | 1101 | `	if( ImgRead(p,zBuf,18) != 18 ){` |
|     3 | 1102 | `		return 0;` |
|     - | 1103 | `	}` |
|     9 | 1104 | `	if( SyMemcmp(zBuf,"VP8",3) != 0 ){` |
|   ! 0 | 1105 | `		return 0;` |
|     - | 1106 | `	}` |
|     9 | 1107 | `	switch( zBuf[3] ){` |
|     2 | 1108 | `	case ' ':` |
|     5 | 1109 | `		pOut->nWidth  = (unsigned int)zBuf[14] + (((unsigned int)zBuf[15] & 0x3F) << 8);` |
|     5 | 1110 | `		pOut->nHeight = (unsigned int)zBuf[16] + (((unsigned int)zBuf[17] & 0x3F) << 8);` |
|     5 | 1111 | `		break;` |
|     1 | 1112 | `	case 'L':` |
|     3 | 1113 | `		pOut->nWidth  = (unsigned int)zBuf[9] + (((unsigned int)zBuf[10] & 0x3F) << 8) + 1;` |
|     4 | 1114 | `		pOut->nHeight = ((unsigned int)zBuf[10] >> 6) + ((unsigned int)zBuf[11] << 2)` |
|     2 | 1115 | `		              + (((unsigned int)zBuf[12] & 0x0F) << 10) + 1;` |
|     3 | 1116 | `		break;` |
|     1 | 1117 | `	case 'X':` |
|     4 | 1118 | `		pOut->nWidth  = (unsigned int)zBuf[12] + ((unsigned int)zBuf[13] << 8)` |
|     2 | 1119 | `		              + ((unsigned int)zBuf[14] << 16) + 1;` |
|     4 | 1120 | `		pOut->nHeight = (unsigned int)zBuf[15] + ((unsigned int)zBuf[16] << 8)` |
|     2 | 1121 | `		              + ((unsigned int)zBuf[17] << 16) + 1;` |
|     3 | 1122 | `		break;` |
|   ! 0 | 1123 | `	default:` |
|   ! 0 | 1124 | `		return 0;` |
|     - | 1125 | `	}` |
|     9 | 1126 | `	pOut->nBits = 8; /* always one byte */` |
|     9 | 1127 | `	return 1;` |
|     6 | 1128 | `}` |
|     - | 1129 | `/*` |
|     - | 1130 | ` * ---------------------------------------------------------------------------` |
|     - | 1131 | ` * AVIF and HEIF: an ISO base media file, walked for its primary item.` |
|     - | 1132 | ` * ---------------------------------------------------------------------------` |
|     - | 1133 | ` * Neither format states a size in a header. Both are ISO/IEC 14496-12 BOX` |
|     - | 1134 | ` * files whose pictures are ITEMS, and the size of the one that matters lives` |
|     - | 1135 | ` * in a PROPERTY that a separate table associates with it -- so answering` |
|     - | 1136 | ``  * "how big is this image" means walking `meta`, remembering every `ispe` `` |
|     - | 1137 | `` * (extent), `pixi`/`av1C` (depth and channel count) and `auxC` (is this an`` |
|     - | 1138 | `` * alpha plane) in the order they appear inside `ipco`, and only then reading`` |
|     - | 1139 | `` * `ipma` to learn which of them belong to the item `pitm` named.`` |
|     - | 1140 | ` *` |
|     - | 1141 | ` * php gets this from libavifinfo (AOMedia, BSD-2-Clause), and the rules below` |
|     - | 1142 | ` * are that reader's rather than the standard's wherever the two differ -- they` |
|     - | 1143 | ` * are what a php program sees:` |
|     - | 1144 | `` *   - a property is addressed by its ONE-BASED position inside `ipco`, so the`` |
|     - | 1145 | ` *     association table is an index into a list nothing names;` |
|     - | 1146 | ` *   - every id and index is capped at 255 and every list at a small bound (16` |
|     - | 1147 | ` *     tiles, 32 associations, 8 of each property kind); what does not fit is` |
|     - | 1148 | ` *     DROPPED, and dropping is remembered because it changes a failure's kind` |
|     - | 1149 | ` *     rather than its answer;` |
|     - | 1150 | ``  *   - a tiled image carries no depth of its own, so the walk follows `dimg` `` |
|     - | 1151 | ` *     references into its tiles, at most three levels deep;` |
|     - | 1152 | `` *   - an alpha plane is not a channel of the item: `auxC` is parsed first and`` |
|     - | 1153 | ` *     its presence ADDS one to whatever channel count the properties gave;` |
|     - | 1154 | ` *   - the walk stops the moment the primary item's four numbers are all known,` |
|     - | 1155 | ` *     which is why a well-formed file is read in ~450 bytes and a malformed` |
|     - | 1156 | ` *     one is bounded at 4096 boxes instead of running to the end.` |
|     - | 1157 | ` * HEIF is the same walk: php identifies it by its ftyp brand and then hands` |
|     - | 1158 | ` * the file to the very same reader from the beginning.` |
|     - | 1159 | ` */` |
|     - | 1160 | `#define IMG_AVIF_MAX_READ      64` |
|     - | 1161 | `#define IMG_AVIF_MAX_NUM_BOXES 4096` |
|     - | 1162 | `#define IMG_AVIF_MAX_VALUE     255` |
|     - | 1163 | `#define IMG_AVIF_MAX_TILES     16` |
|     - | 1164 | `#define IMG_AVIF_MAX_PROPS     32` |
|     - | 1165 | `#define IMG_AVIF_MAX_FEATURES  8` |
|     - | 1166 | `/* The reader's own status space. Only kFound is an answer; kNotFound and` |
|     - | 1167 | ` * kTruncated mean "not enough data", kAborted "too complex", kInvalid "bad` |
|     - | 1168 | ` * file" -- and php treats all four alike, so only kFound matters here. */` |
|     - | 1169 | `#define IMG_AVIF_FOUND     0` |
|     - | 1170 | `#define IMG_AVIF_NOTFOUND  1` |
|     - | 1171 | `#define IMG_AVIF_TRUNCATED 2` |
|     - | 1172 | `#define IMG_AVIF_ABORTED   3` |
|     - | 1173 | `#define IMG_AVIF_INVALID   4` |
|     - | 1174 |  |
|     - | 1175 | `typedef struct ImgAvifTile ImgAvifTile;` |
|     - | 1176 | `struct ImgAvifTile { sxu8 nTile; sxu8 nParent; sxu8 nDimgIdx; };` |
|     - | 1177 | `typedef struct ImgAvifProp ImgAvifProp;` |
|     - | 1178 | `struct ImgAvifProp { sxu8 nIndex; sxu8 nItem; };` |
|     - | 1179 | `typedef struct ImgAvifDim ImgAvifDim;` |
|     - | 1180 | `struct ImgAvifDim { sxu8 nIndex; sxu32 nWidth; sxu32 nHeight; };` |
|     - | 1181 | `typedef struct ImgAvifChan ImgAvifChan;` |
|     - | 1182 | `struct ImgAvifChan { sxu8 nIndex; sxu8 nDepth; sxu8 nChannels; };` |
|     - | 1183 |  |
|     - | 1184 | `typedef struct ImgAvifFeat ImgAvifFeat;` |
|     - | 1185 | `struct ImgAvifFeat {` |
|     - | 1186 | `	sxu8 bHasPrimary;      /* "pitm" was parsed */` |
|     - | 1187 | `	sxu8 bHasAlpha;        /* an alpha "auxC" was parsed */` |
|     - | 1188 | `	sxu8 nGainmapIndex;    /* the gain map's auxC property index */` |
|     - | 1189 | `	sxu8 nPrimaryItem;` |
|     - | 1190 | `	sxu32 nWidth,nHeight,nDepth,nChannels;  /* the primary item's own */` |
|     - | 1191 | `	sxu8 bHasGainmap;` |
|     - | 1192 | `	sxu8 bSkipped;         /* a loop or an index was dropped */` |
|     - | 1193 | `	sxu8 nToneMappedItem;  /* the "tmap" item, > 0 when present */` |
|     - | 1194 | `	sxu8 bIinfParsed;` |
|     - | 1195 | `	sxu8 bIrefParsed;` |
|     - | 1196 | `	sxu8 nTiles;   ImgAvifTile aTile[IMG_AVIF_MAX_TILES];` |
|     - | 1197 | `	sxu8 nProps;   ImgAvifProp aProp[IMG_AVIF_MAX_PROPS];` |
|     - | 1198 | `	sxu8 nDimProps; ImgAvifDim  aDim[IMG_AVIF_MAX_FEATURES];` |
|     - | 1199 | `	sxu8 nChanProps; ImgAvifChan aChan[IMG_AVIF_MAX_FEATURES];` |
|     - | 1200 | `};` |
|     - | 1201 | `/* One box header, with the four-byte type and the FULL-box version/flags for` |
|     - | 1202 | ` * the nine types that carry them. */` |
|     - | 1203 | `typedef struct ImgAvifBox ImgAvifBox;` |
|     - | 1204 | `struct ImgAvifBox {` |
|     - | 1205 | `	sxu32 nSize;` |
|     - | 1206 | `	unsigned char zType[4];` |
|     - | 1207 | `	sxu32 nVersion;` |
|     - | 1208 | `	sxu32 nFlags;` |
|     - | 1209 | `	sxu32 nContent;   /* nSize minus the header */` |
|     - | 1210 | `};` |
|     - | 1211 | `/* The stream this walk reads through: one shared 64-byte window, exactly as` |
|     - | 1212 | ` * php's is, so a read invalidates the previous one. */` |
|     - | 1213 | `typedef struct ImgAvifStream ImgAvifStream;` |
|     - | 1214 | `struct ImgAvifStream {` |
|     - | 1215 | `	ImgReader *p;` |
|     - | 1216 | `	int bDead;                          /* a read or a skip already failed */` |
|     - | 1217 | `	unsigned char zBuf[IMG_AVIF_MAX_READ];` |
|     - | 1218 | `};` |
|  3352 | 1219 | `static sxu32 ImgAvifBE(const unsigned char *z,sxu32 nByte)` |
|     1 | 1220 | `{` |
|  3353 | 1221 | `	sxu32 v = 0,i;` |
| 12067 | 1222 | `	for( i = 0 ; i < nByte ; ++i ){` |
|  8715 | 1223 | `		v = (v << 8) \| z[i];` |
|  4358 | 1224 | `	}` |
|  3353 | 1225 | `	return v;` |
|     1 | 1226 | `}` |
|  2838 | 1227 | `static int ImgAvifRead(ImgAvifStream *s,sxu32 nByte,const unsigned char **ppData)` |
|     1 | 1228 | `{` |
|  2838 | 1229 | `	if( s->bDead \|\| nByte > IMG_AVIF_MAX_READ` |
|  2839 | 1230 | `	 \|\| ImgRead(s->p,s->zBuf,(ph7_int64)nByte) != (ph7_int64)nByte ){` |
|    25 | 1231 | `		s->bDead = TRUE;` |
|    25 | 1232 | `		return IMG_AVIF_TRUNCATED;` |
|     - | 1233 | `	}` |
|  2815 | 1234 | `	*ppData = s->zBuf;` |
|  2815 | 1235 | `	return IMG_AVIF_FOUND;` |
|  1420 | 1236 | `}` |
|   556 | 1237 | `static int ImgAvifSkip(ImgAvifStream *s,sxu32 nByte)` |
|     1 | 1238 | `{` |
|   557 | 1239 | `	if( nByte > 0 ){` |
|   285 | 1240 | `		if( s->bDead ){` |
|   ! 0 | 1241 | `			return IMG_AVIF_TRUNCATED;` |
|     - | 1242 | `		}` |
|   285 | 1243 | `		ImgSeekCur(s->p,(ph7_int64)nByte);` |
|   142 | 1244 | `	}` |
|   557 | 1245 | `	return IMG_AVIF_FOUND;` |
|   279 | 1246 | `}` |
|     - | 1247 | `/*` |
|     - | 1248 | ` * The features of one item, gathered from the associations already read.` |
|     - | 1249 | ` * Recurses into a tiled item's parts for the depth and channel count the` |
|     - | 1250 | ` * parent does not carry.` |
|     - | 1251 | ` */` |
|    72 | 1252 | `static int ImgAvifItemFeatures(ImgAvifFeat *f,sxu32 nTarget,sxu32 nDepth)` |
|     1 | 1253 | `{` |
|     - | 1254 | `	sxu32 i,j;` |
|   157 | 1255 | `	for( i = 0 ; i < f->nProps ; ++i ){` |
|     - | 1256 | `		sxu32 nIndex;` |
|   143 | 1257 | `		if( f->aProp[i].nItem != nTarget ){` |
|    15 | 1258 | `			continue;` |
|     - | 1259 | `		}` |
|   129 | 1260 | `		nIndex = f->aProp[i].nIndex;` |
|   129 | 1261 | `		if( nTarget == f->nPrimaryItem && (f->nWidth == 0 \|\| f->nHeight == 0) ){` |
|    83 | 1262 | `			for( j = 0 ; j < f->nDimProps ; ++j ){` |
|    73 | 1263 | `				if( f->aDim[j].nIndex != nIndex ){` |
|    11 | 1264 | `					continue;` |
|     - | 1265 | `				}` |
|    63 | 1266 | `				f->nWidth  = f->aDim[j].nWidth;` |
|    63 | 1267 | `				f->nHeight = f->aDim[j].nHeight;` |
|    63 | 1268 | `				if( f->nDepth != 0 && f->nChannels != 0 ){` |
|     3 | 1269 | `					return IMG_AVIF_FOUND;` |
|     - | 1270 | `				}` |
|    61 | 1271 | `				break;` |
|   ! 0 | 1272 | `			}` |
|    35 | 1273 | `		}` |
|   127 | 1274 | `		if( f->nDepth == 0 \|\| f->nChannels == 0 ){` |
|   191 | 1275 | `			for( j = 0 ; j < f->nChanProps ; ++j ){` |
|   127 | 1276 | `				if( f->aChan[j].nIndex != nIndex ){` |
|    65 | 1277 | `					continue;` |
|     - | 1278 | `				}` |
|    63 | 1279 | `				f->nDepth    = f->aChan[j].nDepth;` |
|    63 | 1280 | `				f->nChannels = f->aChan[j].nChannels;` |
|    63 | 1281 | `				if( f->nWidth != 0 && f->nHeight != 0 ){` |
|    57 | 1282 | `					return IMG_AVIF_FOUND;` |
|     - | 1283 | `				}` |
|     7 | 1284 | `				break;` |
|   ! 0 | 1285 | `			}` |
|    35 | 1286 | `		}` |
|    36 | 1287 | `	}` |
|    15 | 1288 | `	for( i = 0 ; i < f->nTiles && nDepth < 3 ; ++i ){` |
|     - | 1289 | `		int rc;` |
|     3 | 1290 | `		if( f->aTile[i].nParent != nTarget ){` |
|   ! 0 | 1291 | `			continue;` |
|     - | 1292 | `		}` |
|     3 | 1293 | `		rc = ImgAvifItemFeatures(f,f->aTile[i].nTile,nDepth + 1);` |
|     3 | 1294 | `		if( rc != IMG_AVIF_NOTFOUND ){` |
|     3 | 1295 | `			return rc;` |
|     - | 1296 | `		}` |
|   ! 0 | 1297 | `	}` |
|    13 | 1298 | `	return IMG_AVIF_NOTFOUND;` |
|    37 | 1299 | `}` |
|    78 | 1300 | `static int ImgAvifPrimaryFeatures(ImgAvifFeat *f)` |
|     1 | 1301 | `{` |
|     - | 1302 | `	sxu32 i;` |
|     - | 1303 | `	int rc;` |
|    79 | 1304 | `	if( !f->bHasPrimary ){` |
|   ! 0 | 1305 | `		return IMG_AVIF_NOTFOUND;` |
|     - | 1306 | `	}` |
|    79 | 1307 | `	if( f->nDimProps == 0 \|\| f->nChanProps == 0 ){` |
|     3 | 1308 | `		return IMG_AVIF_NOTFOUND;` |
|     - | 1309 | `	}` |
|     - | 1310 | `	/* A gain map is either a hidden input of a derived item (the HEIF scheme)` |
|     - | 1311 | `	 * or an auxiliary item of its own (Adobe's). */` |
|    77 | 1312 | `	if( f->nToneMappedItem ){` |
|     3 | 1313 | `		for( i = 0 ; i < f->nTiles ; ++i ){` |
|   ! 0 | 1314 | `			if( f->aTile[i].nParent == f->nToneMappedItem && f->aTile[i].nDimgIdx == 1 ){` |
|   ! 0 | 1315 | `				f->bHasGainmap = 1;` |
|   ! 0 | 1316 | `				break;` |
|     - | 1317 | `			}` |
|   ! 0 | 1318 | `		}` |
|     1 | 1319 | `	}` |
|    77 | 1320 | `	if( !f->bHasGainmap && f->nGainmapIndex > 0 ){` |
|     7 | 1321 | `		for( i = 0 ; i < f->nProps ; ++i ){` |
|     7 | 1322 | `			if( f->aProp[i].nIndex == f->nGainmapIndex ){` |
|     3 | 1323 | `				f->bHasGainmap = 1;` |
|     3 | 1324 | `				break;` |
|     - | 1325 | `			}` |
|     3 | 1326 | `		}` |
|     1 | 1327 | `	}` |
|     - | 1328 | `	/* Not finding one is only final once the tables that could still name one` |
|     - | 1329 | `	 * have been read. */` |
|    77 | 1330 | `	if( !f->bHasGainmap && (!f->bIinfParsed \|\| (f->nToneMappedItem && !f->bIrefParsed)) ){` |
|     7 | 1331 | `		return IMG_AVIF_NOTFOUND;` |
|     - | 1332 | `	}` |
|    71 | 1333 | `	rc = ImgAvifItemFeatures(f,f->nPrimaryItem,0);` |
|    71 | 1334 | `	if( rc != IMG_AVIF_FOUND ){` |
|    13 | 1335 | `		return rc;` |
|     - | 1336 | `	}` |
|     - | 1337 | `	/* "auxC" is read before the associations, so alpha is known by now. */` |
|    59 | 1338 | `	if( f->bHasAlpha ){` |
|     3 | 1339 | `		++f->nChannels;` |
|     1 | 1340 | `	}` |
|    59 | 1341 | `	return IMG_AVIF_FOUND;` |
|    40 | 1342 | `}` |
|     - | 1343 | `/*` |
|     - | 1344 | ` * One box header. A size of 1 means a 64-bit size follows the type and a size` |
|     - | 1345 | ` * of 0 means "to the end of the file", which is legal only at the top level.` |
|     - | 1346 | ` * The nine FULL boxes carry a version and flags, and a version this reader` |
|     - | 1347 | `` * does not know turns the box into a `skip` rather than a refusal.`` |
|     - | 1348 | ` */` |
|  1026 | 1349 | `static int ImgAvifParseBox(int iNest,ImgAvifStream *s,sxu32 nRemaining,` |
|     - | 1350 | `	sxu32 *pnBoxes,ImgAvifBox *pBox)` |
|     1 | 1351 | `{` |
|     - | 1352 | `	const unsigned char *zData;` |
|  1027 | 1353 | `	sxu32 nHeader = 8; /* 32-bit size + 32-bit type, at least */` |
|     - | 1354 | `	int rc,bFull;` |
|  1027 | 1355 | `	if( nHeader > nRemaining ){` |
|   ! 0 | 1356 | `		return IMG_AVIF_INVALID;` |
|     - | 1357 | `	}` |
|  1027 | 1358 | `	rc = ImgAvifRead(s,8,&zData);` |
|  1027 | 1359 | `	if( rc != IMG_AVIF_FOUND ){` |
|    21 | 1360 | `		return rc;` |
|     - | 1361 | `	}` |
|  1007 | 1362 | `	pBox->nSize = ImgAvifBE(zData,4);` |
|  1007 | 1363 | `	SyMemcpy(&zData[4],pBox->zType,4);` |
|  1007 | 1364 | `	if( pBox->nSize == 1 ){` |
|   ! 0 | 1365 | `		nHeader += 8;` |
|   ! 0 | 1366 | `		if( nHeader > nRemaining ){` |
|   ! 0 | 1367 | `			return IMG_AVIF_INVALID;` |
|     - | 1368 | `		}` |
|   ! 0 | 1369 | `		rc = ImgAvifRead(s,8,&zData);` |
|   ! 0 | 1370 | `		if( rc != IMG_AVIF_FOUND ){` |
|   ! 0 | 1371 | `			return rc;` |
|     - | 1372 | `		}` |
|   ! 0 | 1373 | `		if( ImgAvifBE(zData,4) != 0 ){` |
|   ! 0 | 1374 | `			return IMG_AVIF_ABORTED; /* no box past 4 GB */` |
|     - | 1375 | `		}` |
|   ! 0 | 1376 | `		pBox->nSize = ImgAvifBE(&zData[4],4);` |
|  1007 | 1377 | `	}else if( pBox->nSize == 0 ){` |
|   ! 0 | 1378 | `		if( iNest != 0 ){` |
|   ! 0 | 1379 | `			return IMG_AVIF_INVALID;` |
|     - | 1380 | `		}` |
|   ! 0 | 1381 | `		pBox->nSize = nRemaining;` |
|   ! 0 | 1382 | `	}` |
|  1007 | 1383 | `	if( pBox->nSize < nHeader \|\| pBox->nSize > nRemaining ){` |
|   ! 0 | 1384 | `		return IMG_AVIF_INVALID;` |
|     - | 1385 | `	}` |
|  1870 | 1386 | `	bFull = SyMemcmp(pBox->zType,"meta",4) == 0 \|\| SyMemcmp(pBox->zType,"pitm",4) == 0` |
|   863 | 1387 | `	     \|\| SyMemcmp(pBox->zType,"ipma",4) == 0 \|\| SyMemcmp(pBox->zType,"ispe",4) == 0` |
|   694 | 1388 | `	     \|\| SyMemcmp(pBox->zType,"pixi",4) == 0 \|\| SyMemcmp(pBox->zType,"iref",4) == 0` |
|   577 | 1389 | `	     \|\| SyMemcmp(pBox->zType,"auxC",4) == 0 \|\| SyMemcmp(pBox->zType,"iinf",4) == 0` |
|  1461 | 1390 | `	     \|\| SyMemcmp(pBox->zType,"infe",4) == 0;` |
|  1007 | 1391 | `	if( bFull ){` |
|   615 | 1392 | `		nHeader += 4;` |
|   307 | 1393 | `	}` |
|  1007 | 1394 | `	if( pBox->nSize < nHeader ){` |
|   ! 0 | 1395 | `		return IMG_AVIF_INVALID;` |
|     - | 1396 | `	}` |
|  1007 | 1397 | `	pBox->nContent = pBox->nSize - nHeader;` |
|     - | 1398 | `	/* A top-level "ftyp" is not counted, so this walk answers the same whether` |
|     - | 1399 | `	 * or not the identify pass already read one. */` |
|  1007 | 1400 | `	if( iNest != 0 \|\| SyMemcmp(pBox->zType,"ftyp",4) != 0 ){` |
|   893 | 1401 | `		++*pnBoxes;` |
|   893 | 1402 | `		if( *pnBoxes >= IMG_AVIF_MAX_NUM_BOXES ){` |
|   ! 0 | 1403 | `			return IMG_AVIF_ABORTED;` |
|     - | 1404 | `		}` |
|   446 | 1405 | `	}` |
|  1007 | 1406 | `	pBox->nVersion = 0;` |
|  1007 | 1407 | `	pBox->nFlags = 0;` |
|  1007 | 1408 | `	if( bFull ){` |
|     - | 1409 | `		int bKnown;` |
|   615 | 1410 | `		rc = ImgAvifRead(s,4,&zData);` |
|   615 | 1411 | `		if( rc != IMG_AVIF_FOUND ){` |
|     3 | 1412 | `			return rc;` |
|     - | 1413 | `		}` |
|   613 | 1414 | `		pBox->nVersion = ImgAvifBE(zData,1);` |
|   613 | 1415 | `		pBox->nFlags = ImgAvifBE(&zData[1],3);` |
|   920 | 1416 | `		bKnown = (SyMemcmp(pBox->zType,"meta",4) == 0 && pBox->nVersion <= 0)` |
|   564 | 1417 | `		      \|\| (SyMemcmp(pBox->zType,"pitm",4) == 0 && pBox->nVersion <= 1)` |
|   471 | 1418 | `		      \|\| (SyMemcmp(pBox->zType,"ipma",4) == 0 && pBox->nVersion <= 1)` |
|   387 | 1419 | `		      \|\| (SyMemcmp(pBox->zType,"ispe",4) == 0 && pBox->nVersion <= 0)` |
|   304 | 1420 | `		      \|\| (SyMemcmp(pBox->zType,"pixi",4) == 0 && pBox->nVersion <= 0)` |
|   224 | 1421 | `		      \|\| (SyMemcmp(pBox->zType,"iref",4) == 0 && pBox->nVersion <= 1)` |
|   187 | 1422 | `		      \|\| (SyMemcmp(pBox->zType,"auxC",4) == 0 && pBox->nVersion <= 0)` |
|   183 | 1423 | `		      \|\| (SyMemcmp(pBox->zType,"iinf",4) == 0 && pBox->nVersion <= 1)` |
|   918 | 1424 | `		      \|\| (SyMemcmp(pBox->zType,"infe",4) == 0 && pBox->nVersion >= 2 && pBox->nVersion <= 3);` |
|   613 | 1425 | `		if( !bKnown ){` |
|     3 | 1426 | `			SyMemcpy("skip",pBox->zType,4); /* an unparsable box is free space */` |
|     1 | 1427 | `		}` |
|   306 | 1428 | `	}` |
|  1005 | 1429 | `	return IMG_AVIF_FOUND;` |
|   514 | 1430 | `}` |
|     - | 1431 | `/*` |
|     - | 1432 | ` * "ipco": the property list itself. Its boxes are numbered from one in the` |
|     - | 1433 | ` * order they appear, and that number is what "ipma" associates with an item.` |
|     - | 1434 | ` */` |
|    88 | 1435 | `static int ImgAvifParseIpco(int iNest,ImgAvifStream *s,sxu32 nRemaining,` |
|     - | 1436 | `	sxu32 *pnBoxes,ImgAvifFeat *f)` |
|     1 | 1437 | `{` |
|    89 | 1438 | `	sxu32 nIndex = 1; /* one-based, and the walk's whole addressing scheme */` |
|    44 | 1439 | `	do {` |
|     - | 1440 | `		ImgAvifBox sBox;` |
|     - | 1441 | `		const unsigned char *zData;` |
|   179 | 1442 | `		int rc = ImgAvifParseBox(iNest,s,nRemaining,pnBoxes,&sBox);` |
|   179 | 1443 | `		if( rc != IMG_AVIF_FOUND ){` |
|   ! 0 | 1444 | `			return rc;` |
|     - | 1445 | `		}` |
|   179 | 1446 | `		if( SyMemcmp(sBox.zType,"ispe",4) == 0 ){` |
|     - | 1447 | `			sxu32 nWidth,nHeight;` |
|    89 | 1448 | `			if( sBox.nContent < 8 ){` |
|   ! 0 | 1449 | `				return IMG_AVIF_INVALID;` |
|     - | 1450 | `			}` |
|    89 | 1451 | `			rc = ImgAvifRead(s,8,&zData);` |
|    89 | 1452 | `			if( rc != IMG_AVIF_FOUND ){` |
|   ! 0 | 1453 | `				return rc;` |
|     - | 1454 | `			}` |
|    89 | 1455 | `			nWidth  = ImgAvifBE(zData,4);` |
|    89 | 1456 | `			nHeight = ImgAvifBE(&zData[4],4);` |
|    89 | 1457 | `			if( nWidth == 0 \|\| nHeight == 0 ){` |
|     5 | 1458 | `				return IMG_AVIF_INVALID;` |
|     - | 1459 | `			}` |
|    85 | 1460 | `			if( f->nDimProps < IMG_AVIF_MAX_FEATURES && nIndex <= IMG_AVIF_MAX_VALUE ){` |
|    85 | 1461 | `				f->aDim[f->nDimProps].nIndex  = (sxu8)nIndex;` |
|    85 | 1462 | `				f->aDim[f->nDimProps].nWidth  = nWidth;` |
|    85 | 1463 | `				f->aDim[f->nDimProps].nHeight = nHeight;` |
|    85 | 1464 | `				++f->nDimProps;` |
|    43 | 1465 | `			}else{` |
|   ! 0 | 1466 | `				f->bSkipped = 1;` |
|     - | 1467 | `			}` |
|    85 | 1468 | `			rc = ImgAvifSkip(s,sBox.nContent - 8);` |
|    85 | 1469 | `			if( rc != IMG_AVIF_FOUND ){` |
|   ! 0 | 1470 | `				return rc;` |
|     1 | 1471 | `			}` |
|   133 | 1472 | `		}else if( SyMemcmp(sBox.zType,"pixi",4) == 0 ){` |
|     - | 1473 | `			sxu32 nChan,nDepth,i;` |
|    73 | 1474 | `			if( sBox.nContent < 1 ){` |
|   ! 0 | 1475 | `				return IMG_AVIF_INVALID;` |
|     - | 1476 | `			}` |
|    73 | 1477 | `			rc = ImgAvifRead(s,1,&zData);` |
|    73 | 1478 | `			if( rc != IMG_AVIF_FOUND ){` |
|   ! 0 | 1479 | `				return rc;` |
|     - | 1480 | `			}` |
|    73 | 1481 | `			nChan = ImgAvifBE(zData,1);` |
|    73 | 1482 | `			if( nChan < 1 \|\| sBox.nContent < 1 + nChan ){` |
|     3 | 1483 | `				return IMG_AVIF_INVALID;` |
|     - | 1484 | `			}` |
|    71 | 1485 | `			rc = ImgAvifRead(s,1,&zData);` |
|    71 | 1486 | `			if( rc != IMG_AVIF_FOUND ){` |
|   ! 0 | 1487 | `				return rc;` |
|     - | 1488 | `			}` |
|    71 | 1489 | `			nDepth = ImgAvifBE(zData,1);` |
|    71 | 1490 | `			if( nDepth < 1 ){` |
|   ! 0 | 1491 | `				return IMG_AVIF_INVALID;` |
|     - | 1492 | `			}` |
|   205 | 1493 | `			for( i = 1 ; i < nChan ; ++i ){` |
|   137 | 1494 | `				rc = ImgAvifRead(s,1,&zData);` |
|   137 | 1495 | `				if( rc != IMG_AVIF_FOUND ){` |
|   ! 0 | 1496 | `					return rc;` |
|     - | 1497 | `				}` |
|     - | 1498 | `				/* every channel must state the same depth */` |
|   137 | 1499 | `				if( ImgAvifBE(zData,1) != nDepth ){` |
|     3 | 1500 | `					return IMG_AVIF_INVALID;` |
|     - | 1501 | `				}` |
|   135 | 1502 | `				if( i > 32 ){` |
|   ! 0 | 1503 | `					return IMG_AVIF_ABORTED;` |
|     - | 1504 | `				}` |
|    68 | 1505 | `			}` |
|    68 | 1506 | `			if( f->nChanProps < IMG_AVIF_MAX_FEATURES && nIndex <= IMG_AVIF_MAX_VALUE` |
|    69 | 1507 | `			 && nDepth <= IMG_AVIF_MAX_VALUE && nChan <= IMG_AVIF_MAX_VALUE ){` |
|    69 | 1508 | `				f->aChan[f->nChanProps].nIndex    = (sxu8)nIndex;` |
|    69 | 1509 | `				f->aChan[f->nChanProps].nDepth    = (sxu8)nDepth;` |
|    69 | 1510 | `				f->aChan[f->nChanProps].nChannels = (sxu8)nChan;` |
|    69 | 1511 | `				++f->nChanProps;` |
|    35 | 1512 | `			}else{` |
|   ! 0 | 1513 | `				f->bSkipped = 1;` |
|     - | 1514 | `			}` |
|    69 | 1515 | `			rc = ImgAvifSkip(s,sBox.nContent - (1 + nChan));` |
|    69 | 1516 | `			if( rc != IMG_AVIF_FOUND ){` |
|   ! 0 | 1517 | `				return rc;` |
|     1 | 1518 | `			}` |
|    53 | 1519 | `		}else if( SyMemcmp(sBox.zType,"av1C",4) == 0 ){` |
|     - | 1520 | `			int bHigh,bTwelve,bMono;` |
|    13 | 1521 | `			if( sBox.nContent < 3 ){` |
|   ! 0 | 1522 | `				return IMG_AVIF_INVALID;` |
|     - | 1523 | `			}` |
|    13 | 1524 | `			rc = ImgAvifRead(s,3,&zData);` |
|    13 | 1525 | `			if( rc != IMG_AVIF_FOUND ){` |
|   ! 0 | 1526 | `				return rc;` |
|     - | 1527 | `			}` |
|     - | 1528 | `			/* Only the third byte matters: two depth bits and a monochrome one. */` |
|    13 | 1529 | `			bHigh   = (zData[2] & 0x40) != 0;` |
|    13 | 1530 | `			bTwelve = (zData[2] & 0x20) != 0;` |
|    13 | 1531 | `			bMono   = (zData[2] & 0x10) != 0;` |
|    13 | 1532 | `			if( bTwelve && !bHigh ){` |
|     3 | 1533 | `				return IMG_AVIF_INVALID;` |
|     - | 1534 | `			}` |
|    11 | 1535 | `			if( f->nChanProps < IMG_AVIF_MAX_FEATURES && nIndex <= IMG_AVIF_MAX_VALUE ){` |
|    11 | 1536 | `				f->aChan[f->nChanProps].nIndex    = (sxu8)nIndex;` |
|    11 | 1537 | `				f->aChan[f->nChanProps].nDepth    = (sxu8)(bHigh ? (bTwelve ? 12 : 10) : 8);` |
|    11 | 1538 | `				f->aChan[f->nChanProps].nChannels = (sxu8)(bMono ? 1 : 3);` |
|    11 | 1539 | `				++f->nChanProps;` |
|     6 | 1540 | `			}else{` |
|   ! 0 | 1541 | `				f->bSkipped = 1;` |
|     - | 1542 | `			}` |
|    11 | 1543 | `			rc = ImgAvifSkip(s,sBox.nContent - 3);` |
|    11 | 1544 | `			if( rc != IMG_AVIF_FOUND ){` |
|   ! 0 | 1545 | `				return rc;` |
|     1 | 1546 | `			}` |
|    12 | 1547 | `		}else if( SyMemcmp(sBox.zType,"auxC",4) == 0 ){` |
|     - | 1548 | `			/* Two auxiliary kinds are recognized by their URN. The gain map's is` |
|     - | 1549 | `			 * the shorter, so it is read first and the alpha one continues from` |
|     - | 1550 | `			 * where it stopped. */` |
|     - | 1551 | `			static const char zGainmap[] = "urn:com:photo:aux:hdrgainmap";` |
|     - | 1552 | `			static const char zAlpha[]   = "urn:mpeg:mpegB:cicp:systems:auxiliary:alpha";` |
|     7 | 1553 | `			const sxu32 nGainmap = 29; /* both lengths include the terminator */` |
|     7 | 1554 | `			const sxu32 nAlpha   = 44;` |
|     7 | 1555 | `			sxu32 nRead = 0;` |
|     7 | 1556 | `			if( sBox.nContent >= nGainmap ){` |
|     7 | 1557 | `				rc = ImgAvifRead(s,nGainmap,&zData);` |
|     7 | 1558 | `				if( rc != IMG_AVIF_FOUND ){` |
|   ! 0 | 1559 | `					return rc;` |
|     - | 1560 | `				}` |
|     7 | 1561 | `				nRead = nGainmap;` |
|     7 | 1562 | `				if( SyMemcmp(zData,zGainmap,nGainmap) == 0 ){` |
|     3 | 1563 | `					if( nIndex <= IMG_AVIF_MAX_VALUE ){` |
|     3 | 1564 | `						f->nGainmapIndex = (sxu8)nIndex;` |
|     2 | 1565 | `					}else{` |
|   ! 0 | 1566 | `						f->bSkipped = 1;` |
|     1 | 1567 | `					}` |
|     6 | 1568 | `				}else if( sBox.nContent >= nAlpha && SyMemcmp(zData,zAlpha,nGainmap) == 0 ){` |
|     - | 1569 | `					const unsigned char *zTail;` |
|     3 | 1570 | `					rc = ImgAvifRead(s,nAlpha - nGainmap,&zTail);` |
|     3 | 1571 | `					if( rc != IMG_AVIF_FOUND ){` |
|   ! 0 | 1572 | `						return rc;` |
|     - | 1573 | `					}` |
|     3 | 1574 | `					nRead = nAlpha;` |
|     3 | 1575 | `					if( SyMemcmp(zTail,&zAlpha[nGainmap],nAlpha - nGainmap) == 0 ){` |
|     3 | 1576 | `						f->bHasAlpha = 1;` |
|     1 | 1577 | `					}` |
|     1 | 1578 | `				}` |
|     3 | 1579 | `			}` |
|     7 | 1580 | `			rc = ImgAvifSkip(s,sBox.nContent - nRead);` |
|     7 | 1581 | `			if( rc != IMG_AVIF_FOUND ){` |
|   ! 0 | 1582 | `				return rc;` |
|     - | 1583 | `			}` |
|     4 | 1584 | `		}else{` |
|   ! 0 | 1585 | `			rc = ImgAvifSkip(s,sBox.nContent);` |
|   ! 0 | 1586 | `			if( rc != IMG_AVIF_FOUND ){` |
|   ! 0 | 1587 | `				return rc;` |
|     - | 1588 | `			}` |
|     - | 1589 | `		}` |
|   169 | 1590 | `		++nIndex;` |
|   169 | 1591 | `		nRemaining -= sBox.nSize;` |
|   169 | 1592 | `	}while( nRemaining > 0 );` |
|    79 | 1593 | `	return IMG_AVIF_NOTFOUND;` |
|    45 | 1594 | `}` |
|     - | 1595 | `/*` |
|     - | 1596 | ` * "iprp": the property container. "ipco" holds the properties and "ipma"` |
|     - | 1597 | ` * links them to items -- and the link table is where the walk can finish,` |
|     - | 1598 | ` * because the moment the primary item's four numbers are known there is` |
|     - | 1599 | ` * nothing left to read.` |
|     - | 1600 | ` */` |
|    88 | 1601 | `static int ImgAvifParseIprp(int iNest,ImgAvifStream *s,sxu32 nRemaining,` |
|     - | 1602 | `	sxu32 *pnBoxes,ImgAvifFeat *f)` |
|     1 | 1603 | `{` |
|    44 | 1604 | `	do {` |
|     - | 1605 | `		ImgAvifBox sBox;` |
|     - | 1606 | `		const unsigned char *zData;` |
|   167 | 1607 | `		int rc = ImgAvifParseBox(iNest,s,nRemaining,pnBoxes,&sBox);` |
|   167 | 1608 | `		if( rc != IMG_AVIF_FOUND ){` |
|   ! 0 | 1609 | `			return rc;` |
|     - | 1610 | `		}` |
|   167 | 1611 | `		if( SyMemcmp(sBox.zType,"ipco",4) == 0 ){` |
|    89 | 1612 | `			rc = ImgAvifParseIpco(iNest + 1,s,sBox.nContent,pnBoxes,f);` |
|    89 | 1613 | `			if( rc != IMG_AVIF_NOTFOUND ){` |
|    11 | 1614 | `				return rc;` |
|     1 | 1615 | `			}` |
|   118 | 1616 | `		}else if( SyMemcmp(sBox.zType,"ipma",4) == 0 ){` |
|    77 | 1617 | `			sxu32 nRead = 4,nCount,nIdBytes,nIdxBytes,nEssential,e;` |
|    77 | 1618 | `			if( sBox.nContent < nRead ){` |
|   ! 0 | 1619 | `				return IMG_AVIF_INVALID;` |
|     - | 1620 | `			}` |
|    77 | 1621 | `			rc = ImgAvifRead(s,4,&zData);` |
|    77 | 1622 | `			if( rc != IMG_AVIF_FOUND ){` |
|   ! 0 | 1623 | `				return rc;` |
|     - | 1624 | `			}` |
|    77 | 1625 | `			nCount     = ImgAvifBE(zData,4);` |
|    77 | 1626 | `			nIdBytes   = (sBox.nVersion < 1) ? 2 : 4;` |
|    77 | 1627 | `			nIdxBytes  = (sBox.nFlags & 1) ? 2 : 1;` |
|    77 | 1628 | `			nEssential = (sBox.nFlags & 1) ? 0x8000 : 0x80;` |
|   157 | 1629 | `			for( e = 0 ; e < nCount ; ++e ){` |
|     - | 1630 | `				sxu32 nItem,nAssoc,a;` |
|    81 | 1631 | `				if( e >= IMG_AVIF_MAX_PROPS \|\| f->nProps >= IMG_AVIF_MAX_PROPS ){` |
|   ! 0 | 1632 | `					f->bSkipped = 1;` |
|   ! 0 | 1633 | `					break;` |
|     - | 1634 | `				}` |
|    81 | 1635 | `				nRead += nIdBytes + 1;` |
|    81 | 1636 | `				if( sBox.nContent < nRead ){` |
|   ! 0 | 1637 | `					return IMG_AVIF_INVALID;` |
|     - | 1638 | `				}` |
|    81 | 1639 | `				rc = ImgAvifRead(s,nIdBytes + 1,&zData);` |
|    81 | 1640 | `				if( rc != IMG_AVIF_FOUND ){` |
|   ! 0 | 1641 | `					return rc;` |
|     - | 1642 | `				}` |
|    81 | 1643 | `				nItem  = ImgAvifBE(zData,nIdBytes);` |
|    81 | 1644 | `				nAssoc = ImgAvifBE(&zData[nIdBytes],1);` |
|   237 | 1645 | `				for( a = 0 ; a < nAssoc ; ++a ){` |
|     - | 1646 | `					sxu32 nValue,nPropIdx;` |
|   157 | 1647 | `					if( a >= IMG_AVIF_MAX_PROPS \|\| f->nProps >= IMG_AVIF_MAX_PROPS ){` |
|   ! 0 | 1648 | `						f->bSkipped = 1;` |
|   ! 0 | 1649 | `						break;` |
|     - | 1650 | `					}` |
|   157 | 1651 | `					nRead += nIdxBytes;` |
|   157 | 1652 | `					if( sBox.nContent < nRead ){` |
|   ! 0 | 1653 | `						return IMG_AVIF_INVALID;` |
|     - | 1654 | `					}` |
|   157 | 1655 | `					rc = ImgAvifRead(s,nIdxBytes,&zData);` |
|   157 | 1656 | `					if( rc != IMG_AVIF_FOUND ){` |
|   ! 0 | 1657 | `						return rc;` |
|     - | 1658 | `					}` |
|   157 | 1659 | `					nValue = ImgAvifBE(zData,nIdxBytes);` |
|     - | 1660 | `					/* the top bit marks an ESSENTIAL property; the rest is the index */` |
|   157 | 1661 | `					nPropIdx = nValue & ~nEssential;` |
|   157 | 1662 | `					if( nPropIdx <= IMG_AVIF_MAX_VALUE && nItem <= IMG_AVIF_MAX_VALUE ){` |
|   157 | 1663 | `						f->aProp[f->nProps].nIndex = (sxu8)nPropIdx;` |
|   157 | 1664 | `						f->aProp[f->nProps].nItem  = (sxu8)nItem;` |
|   157 | 1665 | `						++f->nProps;` |
|    79 | 1666 | `					}else{` |
|   ! 0 | 1667 | `						f->bSkipped = 1;` |
|     - | 1668 | `					}` |
|    79 | 1669 | `				}` |
|    81 | 1670 | `				if( a < nAssoc ){` |
|   ! 0 | 1671 | `					break; /* do not read garbage */` |
|     - | 1672 | `				}` |
|    41 | 1673 | `			}` |
|    77 | 1674 | `			rc = ImgAvifPrimaryFeatures(f);` |
|    77 | 1675 | `			if( rc != IMG_AVIF_NOTFOUND ){` |
|    59 | 1676 | `				return rc;` |
|     - | 1677 | `			}` |
|    19 | 1678 | `			rc = ImgAvifSkip(s,sBox.nContent - nRead);` |
|    19 | 1679 | `			if( rc != IMG_AVIF_FOUND ){` |
|   ! 0 | 1680 | `				return rc;` |
|     - | 1681 | `			}` |
|    10 | 1682 | `		}else{` |
|     3 | 1683 | `			rc = ImgAvifSkip(s,sBox.nContent);` |
|     3 | 1684 | `			if( rc != IMG_AVIF_FOUND ){` |
|   ! 0 | 1685 | `				return rc;` |
|     - | 1686 | `			}` |
|     - | 1687 | `		}` |
|    99 | 1688 | `		nRemaining -= sBox.nSize;` |
|    99 | 1689 | `	}while( nRemaining != 0 );` |
|    21 | 1690 | `	return IMG_AVIF_NOTFOUND;` |
|    45 | 1691 | `}` |
|     - | 1692 | `/*` |
|     - | 1693 | ` * "iref": the reference table. Its "dimg" entries say which items a derived` |
|     - | 1694 | ` * one is made of, which is how a TILED image's depth and channel count are` |
|     - | 1695 | ` * found -- the parent carries neither.` |
|     - | 1696 | ` */` |
|     2 | 1697 | `static int ImgAvifParseIref(int iNest,ImgAvifStream *s,sxu32 nRemaining,` |
|     - | 1698 | `	sxu32 *pnBoxes,ImgAvifFeat *f)` |
|     1 | 1699 | `{` |
|     3 | 1700 | `	f->bIrefParsed = 1;` |
|     5 | 1701 | `	while( nRemaining > 0 ){` |
|     - | 1702 | `		ImgAvifBox sBox;` |
|     - | 1703 | `		const unsigned char *zData;` |
|     3 | 1704 | `		int rc = ImgAvifParseBox(iNest,s,nRemaining,pnBoxes,&sBox);` |
|     3 | 1705 | `		if( rc != IMG_AVIF_FOUND ){` |
|   ! 0 | 1706 | `			return rc;` |
|     - | 1707 | `		}` |
|     3 | 1708 | `		if( SyMemcmp(sBox.zType,"dimg",4) == 0 ){` |
|     3 | 1709 | `			sxu32 nIdBytes = (sBox.nVersion == 0) ? 2 : 4;` |
|     3 | 1710 | `			sxu32 nRead = nIdBytes + 2,nFrom,nCount,i;` |
|     3 | 1711 | `			if( sBox.nContent < nRead ){` |
|   ! 0 | 1712 | `				return IMG_AVIF_INVALID;` |
|     - | 1713 | `			}` |
|     3 | 1714 | `			rc = ImgAvifRead(s,nIdBytes + 2,&zData);` |
|     3 | 1715 | `			if( rc != IMG_AVIF_FOUND ){` |
|   ! 0 | 1716 | `				return rc;` |
|     - | 1717 | `			}` |
|     3 | 1718 | `			nFrom  = ImgAvifBE(zData,nIdBytes);` |
|     3 | 1719 | `			nCount = ImgAvifBE(&zData[nIdBytes],2);` |
|     5 | 1720 | `			for( i = 0 ; i < nCount ; ++i ){` |
|     - | 1721 | `				sxu32 nTo;` |
|     3 | 1722 | `				if( i >= IMG_AVIF_MAX_TILES ){` |
|   ! 0 | 1723 | `					f->bSkipped = 1;` |
|   ! 0 | 1724 | `					break;` |
|     - | 1725 | `				}` |
|     3 | 1726 | `				nRead += nIdBytes;` |
|     3 | 1727 | `				if( sBox.nContent < nRead ){` |
|   ! 0 | 1728 | `					return IMG_AVIF_INVALID;` |
|     - | 1729 | `				}` |
|     3 | 1730 | `				rc = ImgAvifRead(s,nIdBytes,&zData);` |
|     3 | 1731 | `				if( rc != IMG_AVIF_FOUND ){` |
|   ! 0 | 1732 | `					return rc;` |
|     - | 1733 | `				}` |
|     3 | 1734 | `				nTo = ImgAvifBE(zData,nIdBytes);` |
|     2 | 1735 | `				if( nFrom <= IMG_AVIF_MAX_VALUE && nTo <= IMG_AVIF_MAX_VALUE` |
|     3 | 1736 | `				 && f->nTiles < IMG_AVIF_MAX_TILES ){` |
|     3 | 1737 | `					f->aTile[f->nTiles].nTile    = (sxu8)nTo;` |
|     3 | 1738 | `					f->aTile[f->nTiles].nParent  = (sxu8)nFrom;` |
|     3 | 1739 | `					f->aTile[f->nTiles].nDimgIdx = (sxu8)i;` |
|     3 | 1740 | `					++f->nTiles;` |
|     2 | 1741 | `				}else{` |
|   ! 0 | 1742 | `					f->bSkipped = 1;` |
|     - | 1743 | `				}` |
|     2 | 1744 | `			}` |
|     3 | 1745 | `			rc = ImgAvifPrimaryFeatures(f);` |
|     3 | 1746 | `			if( rc != IMG_AVIF_NOTFOUND ){` |
|   ! 0 | 1747 | `				return rc;` |
|     - | 1748 | `			}` |
|     3 | 1749 | `			rc = ImgAvifSkip(s,sBox.nContent - nRead);` |
|     3 | 1750 | `			if( rc != IMG_AVIF_FOUND ){` |
|   ! 0 | 1751 | `				return rc;` |
|     - | 1752 | `			}` |
|     2 | 1753 | `		}else{` |
|   ! 0 | 1754 | `			rc = ImgAvifSkip(s,sBox.nContent);` |
|   ! 0 | 1755 | `			if( rc != IMG_AVIF_FOUND ){` |
|   ! 0 | 1756 | `				return rc;` |
|     - | 1757 | `			}` |
|     - | 1758 | `		}` |
|     3 | 1759 | `		nRemaining -= sBox.nSize;` |
|     1 | 1760 | `	}` |
|     3 | 1761 | `	return IMG_AVIF_NOTFOUND;` |
|     2 | 1762 | `}` |
|     - | 1763 | `/*` |
|     - | 1764 | ` * "iinf": the item list. Only one entry kind matters -- a "tmap" item says` |
|     - | 1765 | ` * the file carries a tone-mapped picture, and therefore a gain map.` |
|     - | 1766 | ` */` |
|    90 | 1767 | `static int ImgAvifParseIinf(int iNest,ImgAvifStream *s,sxu32 nRemaining,` |
|     - | 1768 | `	sxu32 nVersion,sxu32 *pnBoxes,ImgAvifFeat *f)` |
|     1 | 1769 | `{` |
|     - | 1770 | `	const unsigned char *zData;` |
|    91 | 1771 | `	sxu32 nCountBytes = (nVersion == 0) ? 2 : 4;` |
|     - | 1772 | `	sxu32 nCount,i;` |
|     - | 1773 | `	int rc;` |
|    91 | 1774 | `	f->bIinfParsed = 1;` |
|    91 | 1775 | `	if( nCountBytes > nRemaining ){` |
|   ! 0 | 1776 | `		return IMG_AVIF_INVALID;` |
|     - | 1777 | `	}` |
|    91 | 1778 | `	rc = ImgAvifRead(s,nCountBytes,&zData);` |
|    91 | 1779 | `	if( rc != IMG_AVIF_FOUND ){` |
|   ! 0 | 1780 | `		return rc;` |
|     - | 1781 | `	}` |
|    91 | 1782 | `	nRemaining -= nCountBytes;` |
|    91 | 1783 | `	nCount = ImgAvifBE(zData,nCountBytes);` |
|    91 | 1784 | `	for( i = 0 ; i < nCount ; ++i ){` |
|     - | 1785 | `		ImgAvifBox sBox;` |
|    91 | 1786 | `		rc = ImgAvifParseBox(iNest,s,nRemaining,pnBoxes,&sBox);` |
|    91 | 1787 | `		if( rc != IMG_AVIF_FOUND ){` |
|     3 | 1788 | `			return rc;` |
|     - | 1789 | `		}` |
|    89 | 1790 | `		if( SyMemcmp(sBox.zType,"infe",4) == 0 ){` |
|    89 | 1791 | `			sxu32 nIdBytes = (sBox.nVersion == 2) ? 2 : 4;` |
|     - | 1792 | `			sxu32 nItem;` |
|     - | 1793 | `			const unsigned char *zItemType;` |
|     - | 1794 | `			/* item_ID (16 or 32) + item_protection_index (16) + item_type (32) */` |
|    89 | 1795 | `			if( nIdBytes + 2 + 4 > sBox.nContent ){` |
|   ! 0 | 1796 | `				return IMG_AVIF_INVALID;` |
|     - | 1797 | `			}` |
|    89 | 1798 | `			rc = ImgAvifRead(s,nIdBytes,&zData);` |
|    89 | 1799 | `			if( rc != IMG_AVIF_FOUND ){` |
|   ! 0 | 1800 | `				return rc;` |
|     - | 1801 | `			}` |
|    89 | 1802 | `			nItem = ImgAvifBE(zData,nIdBytes);` |
|    89 | 1803 | `			rc = ImgAvifSkip(s,2); /* item_protection_index */` |
|    89 | 1804 | `			if( rc != IMG_AVIF_FOUND ){` |
|   ! 0 | 1805 | `				return rc;` |
|     - | 1806 | `			}` |
|    89 | 1807 | `			rc = ImgAvifRead(s,4,&zItemType);` |
|    89 | 1808 | `			if( rc != IMG_AVIF_FOUND ){` |
|   ! 0 | 1809 | `				return rc;` |
|     - | 1810 | `			}` |
|    89 | 1811 | `			if( SyMemcmp(zItemType,"tmap",4) == 0 ){` |
|     3 | 1812 | `				if( nItem <= IMG_AVIF_MAX_VALUE ){` |
|     3 | 1813 | `					f->nToneMappedItem = (sxu8)nItem;` |
|     2 | 1814 | `				}else{` |
|   ! 0 | 1815 | `					f->bSkipped = 1;` |
|     - | 1816 | `				}` |
|     1 | 1817 | `			}` |
|    89 | 1818 | `			rc = ImgAvifSkip(s,sBox.nContent - (nIdBytes + 2 + 4));` |
|    89 | 1819 | `			if( rc != IMG_AVIF_FOUND ){` |
|   ! 0 | 1820 | `				return rc;` |
|     - | 1821 | `			}` |
|    45 | 1822 | `		}else{` |
|   ! 0 | 1823 | `			rc = ImgAvifSkip(s,sBox.nContent);` |
|   ! 0 | 1824 | `			if( rc != IMG_AVIF_FOUND ){` |
|   ! 0 | 1825 | `				return rc;` |
|     - | 1826 | `			}` |
|     - | 1827 | `		}` |
|    89 | 1828 | `		nRemaining -= sBox.nSize;` |
|    89 | 1829 | `		if( nRemaining == 0 ){` |
|    89 | 1830 | `			break; /* an entry count bigger than the box says nothing more */` |
|     - | 1831 | `		}` |
|   ! 0 | 1832 | `	}` |
|    89 | 1833 | `	return IMG_AVIF_NOTFOUND;` |
|    46 | 1834 | `}` |
|     - | 1835 | `/*` |
|     - | 1836 | ` * "meta": where every table lives. There is at most one, so running out of it` |
|     - | 1837 | ` * without an answer is the end of the walk -- INVALID normally, and "too` |
|     - | 1838 | ` * complex" when something along the way had to be dropped.` |
|     - | 1839 | ` */` |
|    96 | 1840 | `static int ImgAvifParseMeta(int iNest,ImgAvifStream *s,sxu32 nRemaining,` |
|     - | 1841 | `	sxu32 *pnBoxes,ImgAvifFeat *f)` |
|     1 | 1842 | `{` |
|    48 | 1843 | `	do {` |
|     - | 1844 | `		ImgAvifBox sBox;` |
|     - | 1845 | `		const unsigned char *zData;` |
|   279 | 1846 | `		int rc = ImgAvifParseBox(iNest,s,nRemaining,pnBoxes,&sBox);` |
|   279 | 1847 | `		if( rc != IMG_AVIF_FOUND ){` |
|    42 | 1848 | `			return rc;` |
|     - | 1849 | `		}` |
|   273 | 1850 | `		if( SyMemcmp(sBox.zType,"pitm",4) == 0 ){` |
|    93 | 1851 | `			sxu32 nIdBytes = (sBox.nVersion == 0) ? 2 : 4;` |
|     - | 1852 | `			sxu32 nPrimary;` |
|    93 | 1853 | `			if( nIdBytes > nRemaining ){` |
|   ! 0 | 1854 | `				return IMG_AVIF_INVALID;` |
|     - | 1855 | `			}` |
|    93 | 1856 | `			rc = ImgAvifRead(s,nIdBytes,&zData);` |
|    93 | 1857 | `			if( rc != IMG_AVIF_FOUND ){` |
|   ! 0 | 1858 | `				return rc;` |
|     - | 1859 | `			}` |
|    93 | 1860 | `			nPrimary = ImgAvifBE(zData,nIdBytes);` |
|    93 | 1861 | `			if( nPrimary > IMG_AVIF_MAX_VALUE ){` |
|   ! 0 | 1862 | `				return IMG_AVIF_ABORTED;` |
|     - | 1863 | `			}` |
|    93 | 1864 | `			f->bHasPrimary = 1;` |
|    93 | 1865 | `			f->nPrimaryItem = (sxu8)nPrimary;` |
|    93 | 1866 | `			rc = ImgAvifSkip(s,sBox.nContent - nIdBytes);` |
|    93 | 1867 | `			if( rc != IMG_AVIF_FOUND ){` |
|   ! 0 | 1868 | `				return rc;` |
|     1 | 1869 | `			}` |
|   227 | 1870 | `		}else if( SyMemcmp(sBox.zType,"iprp",4) == 0 ){` |
|    89 | 1871 | `			rc = ImgAvifParseIprp(iNest + 1,s,sBox.nContent,pnBoxes,f);` |
|    89 | 1872 | `			if( rc != IMG_AVIF_NOTFOUND ){` |
|    69 | 1873 | `				return rc;` |
|     1 | 1874 | `			}` |
|   103 | 1875 | `		}else if( SyMemcmp(sBox.zType,"iref",4) == 0 ){` |
|     3 | 1876 | `			rc = ImgAvifParseIref(iNest + 1,s,sBox.nContent,pnBoxes,f);` |
|     3 | 1877 | `			if( rc != IMG_AVIF_NOTFOUND ){` |
|   ! 0 | 1878 | `				return rc;` |
|     1 | 1879 | `			}` |
|    92 | 1880 | `		}else if( SyMemcmp(sBox.zType,"iinf",4) == 0 ){` |
|    91 | 1881 | `			rc = ImgAvifParseIinf(iNest + 1,s,sBox.nContent,sBox.nVersion,pnBoxes,f);` |
|    91 | 1882 | `			if( rc != IMG_AVIF_NOTFOUND ){` |
|     3 | 1883 | `				return rc;` |
|     - | 1884 | `			}` |
|    45 | 1885 | `		}else{` |
|   ! 0 | 1886 | `			rc = ImgAvifSkip(s,sBox.nContent);` |
|   ! 0 | 1887 | `			if( rc != IMG_AVIF_FOUND ){` |
|   ! 0 | 1888 | `				return rc;` |
|     - | 1889 | `			}` |
|     - | 1890 | `		}` |
|   203 | 1891 | `		nRemaining -= sBox.nSize;` |
|   203 | 1892 | `	}while( nRemaining != 0 );` |
|    21 | 1893 | `	return f->bSkipped ? IMG_AVIF_ABORTED : IMG_AVIF_INVALID;` |
|    49 | 1894 | `}` |
|     - | 1895 | `/*` |
|     - | 1896 | ` * The file's own brands. An AVIF says so in "ftyp", in the major brand or any` |
|     - | 1897 | ` * compatible one -- with the minor VERSION, which sits in the same four-byte` |
|     - | 1898 | ` * grid, skipped rather than tested.` |
|     - | 1899 | ` */` |
|   208 | 1900 | `static int ImgAvifParseFtyp(ImgAvifStream *s)` |
|     1 | 1901 | `{` |
|     - | 1902 | `	ImgAvifBox sBox;` |
|   209 | 1903 | `	sxu32 nBoxes = 0,i;` |
|   209 | 1904 | `	int rc = ImgAvifParseBox(0,s,SXU32_HIGH,&nBoxes,&sBox);` |
|   209 | 1905 | `	if( rc != IMG_AVIF_FOUND ){` |
|    13 | 1906 | `		return rc;` |
|     - | 1907 | `	}` |
|   197 | 1908 | `	if( SyMemcmp(sBox.zType,"ftyp",4) != 0 ){` |
|    89 | 1909 | `		return IMG_AVIF_INVALID;` |
|     - | 1910 | `	}` |
|   109 | 1911 | `	if( sBox.nContent < 8 ){ /* major_brand and minor_version, at least */` |
|   ! 0 | 1912 | `		return IMG_AVIF_INVALID;` |
|     - | 1913 | `	}` |
|   153 | 1914 | `	for( i = 0 ; i + 4 <= sBox.nContent ; i += 4 ){` |
|     - | 1915 | `		const unsigned char *zData;` |
|   139 | 1916 | `		rc = ImgAvifRead(s,4,&zData);` |
|   139 | 1917 | `		if( rc != IMG_AVIF_FOUND ){` |
|    49 | 1918 | `			return rc;` |
|     - | 1919 | `		}` |
|   137 | 1920 | `		if( i == 4 ){` |
|    17 | 1921 | `			continue; /* the minor version is not a brand */` |
|     - | 1922 | `		}` |
|   121 | 1923 | `		if( SyMemcmp(zData,"avif",4) == 0 \|\| SyMemcmp(zData,"avis",4) == 0 ){` |
|    93 | 1924 | `			return ImgAvifSkip(s,sBox.nContent - (i + 4));` |
|     - | 1925 | `		}` |
|    29 | 1926 | `		if( i > 32 * 4 ){` |
|   ! 0 | 1927 | `			return IMG_AVIF_ABORTED; /* be reasonable */` |
|     - | 1928 | `		}` |
|    15 | 1929 | `	}` |
|    15 | 1930 | `	return IMG_AVIF_INVALID; /* no AVIF brand, no good */` |
|   105 | 1931 | `}` |
|     - | 1932 | `/* Skip top-level boxes until "meta", then walk it. */` |
|    98 | 1933 | `static int ImgAvifParseFile(ImgAvifStream *s,sxu32 *pnBoxes,ImgAvifFeat *f)` |
|     1 | 1934 | `{` |
|    55 | 1935 | `	for(;;){` |
|     - | 1936 | `		ImgAvifBox sBox;` |
|   105 | 1937 | `		int rc = ImgAvifParseBox(0,s,SXU32_HIGH,pnBoxes,&sBox);` |
|   105 | 1938 | `		if( rc != IMG_AVIF_FOUND ){` |
|    51 | 1939 | `			return rc;` |
|     - | 1940 | `		}` |
|   103 | 1941 | `		if( SyMemcmp(sBox.zType,"meta",4) == 0 ){` |
|    97 | 1942 | `			return ImgAvifParseMeta(1,s,sBox.nContent,pnBoxes,f);` |
|     - | 1943 | `		}` |
|     7 | 1944 | `		rc = ImgAvifSkip(s,sBox.nContent);` |
|     7 | 1945 | `		if( rc != IMG_AVIF_FOUND ){` |
|   ! 0 | 1946 | `			return rc;` |
|     - | 1947 | `		}` |
|     1 | 1948 | `	}` |
|    50 | 1949 | `}` |
|     - | 1950 | `/* Is the file an AVIF? Reads only the first "ftyp" box, leaving the stream` |
|     - | 1951 | ` * just past it -- which is where the feature walk expects to start. */` |
|   208 | 1952 | `static int ImgAvifIdentify(ImgReader *p)` |
|     1 | 1953 | `{` |
|     - | 1954 | `	ImgAvifStream sStream;` |
|   209 | 1955 | `	SyZero(&sStream,sizeof(sStream));` |
|   209 | 1956 | `	sStream.p = p;` |
|   209 | 1957 | `	return ImgAvifParseFtyp(&sStream) == IMG_AVIF_FOUND;` |
|     1 | 1958 | `}` |
|    98 | 1959 | `static int ImgHandleAvif(ImgReader *p,ImgInfo *pOut)` |
|     1 | 1960 | `{` |
|     - | 1961 | `	ImgAvifStream sStream;` |
|     - | 1962 | `	ImgAvifFeat sFeat;` |
|    99 | 1963 | `	sxu32 nBoxes = 0;` |
|    99 | 1964 | `	SyZero(&sStream,sizeof(sStream));` |
|    99 | 1965 | `	SyZero(&sFeat,sizeof(sFeat));` |
|    99 | 1966 | `	sStream.p = p;` |
|    99 | 1967 | `	if( ImgAvifParseFile(&sStream,&nBoxes,&sFeat) != IMG_AVIF_FOUND ){` |
|    41 | 1968 | `		return 0;` |
|     - | 1969 | `	}` |
|    59 | 1970 | `	pOut->nWidth    = sFeat.nWidth;` |
|    59 | 1971 | `	pOut->nHeight   = sFeat.nHeight;` |
|    59 | 1972 | `	pOut->nBits     = sFeat.nDepth;` |
|    59 | 1973 | `	pOut->nChannels = sFeat.nChannels;` |
|    59 | 1974 | `	return 1;` |
|    50 | 1975 | `}` |
|     - | 1976 | `#ifdef PH7_ENABLE_LIBXML` |
|     - | 1977 | `/*` |
|     - | 1978 | ` * ---------------------------------------------------------------------------` |
|     - | 1979 | ` * SVG: the one container that is TEXT, and the one whose size has a UNIT.` |
|     - | 1980 | ` * ---------------------------------------------------------------------------` |
|     - | 1981 | ` * php reads an SVG from ext/libxml rather than ext/standard: it is a` |
|     - | 1982 | ` * REGISTERED handler, which is why it sits past every signature in the ladder,` |
|     - | 1983 | ` * why its constant is the first past the fixed enum, and why a build without` |
|     - | 1984 | ` * libxml has neither. Four rules of that handler are visible from PHP:` |
|     - | 1985 | ` *   - it is a pull parse that stops at the FIRST element, and that element` |
|     - | 1986 | `` *     must be an `svg` -- case-insensitively and by LOCAL name, so a namespace`` |
|     - | 1987 | ` *     prefix does not matter. A declaration, a comment or a processing` |
|     - | 1988 | ` *     instruction before it is read past; an element inside anything else is` |
|     - | 1989 | ` *     not the root and ends the parse;` |
|     - | 1990 | `` *   - `width` and `height` must both be present and must both match`` |
|     - | 1991 | `` *     `[0-9]+[a-zA-Z]*`. That grammar is a GUARD rather than a parser -- it`` |
|     - | 1992 | ` *     exists so the unit it hands back cannot carry markup -- and it refuses a` |
|     - | 1993 | ` *     sign, a decimal point and a percentage while accepting a plain zero;` |
|     - | 1994 | `` *   - a unit that is not `px` is KEPT, and index 3 -- php's `width="..."`` |
|     - | 1995 | `` *     height="..."` string -- then disappears from the answer, because that`` |
|     - | 1996 | ` *     string means nothing outside pixels;` |
|     - | 1997 | ` *   - the whole thing is guarded by a one-byte look first: a file whose first` |
|     - | 1998 | `` *     byte is not `<` is refused before libxml is built at all.`` |
|     - | 1999 | ` */` |
|   208 | 2000 | `static int ImgSvgReadCb(void *pCookie,char *zBuf,int nLen)` |
|     1 | 2001 | `{` |
|   209 | 2002 | `	return (int)ImgRead((ImgReader *)pCookie,zBuf,(ph7_int64)nLen);` |
|     1 | 2003 | `}` |
|     - | 2004 | ``/* php's `[0-9]+[a-zA-Z]*`, answering where the unit starts (at the terminator`` |
|     - | 2005 | ` * when there is none). */` |
|   126 | 2006 | `static int ImgSvgDimension(const xmlChar *zIn,const xmlChar **pzUnit)` |
|     1 | 2007 | `{` |
|   127 | 2008 | `	if( !(*zIn >= '0' && *zIn <= '9') ){` |
|     9 | 2009 | `		return 0;` |
|     - | 2010 | `	}` |
|   119 | 2011 | `	zIn++;` |
|   267 | 2012 | `	while( *zIn ){` |
|   187 | 2013 | `		if( !(*zIn >= '0' && *zIn <= '9') ){` |
|    39 | 2014 | `			if( (*zIn >= 'a' && *zIn <= 'z') \|\| (*zIn >= 'A' && *zIn <= 'Z') ){` |
|    18 | 2015 | `				break;` |
|     - | 2016 | `			}` |
|     5 | 2017 | `			return 0;` |
|     - | 2018 | `		}` |
|   149 | 2019 | `		zIn++;` |
|     1 | 2020 | `	}` |
|   115 | 2021 | `	*pzUnit = zIn;` |
|   239 | 2022 | `	while( *zIn ){` |
|   127 | 2023 | `		if( !((*zIn >= 'a' && *zIn <= 'z') \|\| (*zIn >= 'A' && *zIn <= 'Z')) ){` |
|     3 | 2024 | `			return 0;` |
|     - | 2025 | `		}` |
|   125 | 2026 | `		zIn++;` |
|     1 | 2027 | `	}` |
|   113 | 2028 | `	return 1;` |
|    64 | 2029 | `}` |
|     - | 2030 | `/* Copy a unit out of libxml's string into storage the answer can keep. */` |
|    56 | 2031 | `static const char * ImgSvgUnit(ph7_context *pCtx,const xmlChar *zUnit)` |
|     1 | 2032 | `{` |
|    57 | 2033 | `	int nByte = (int)SyStrlen((const char *)zUnit);` |
|     - | 2034 | `	char *zOut;` |
|    57 | 2035 | `	if( nByte < 1 ){` |
|    41 | 2036 | `		return 0; /* no unit: php's "px" default stands */` |
|     - | 2037 | `	}` |
|    17 | 2038 | `	zOut = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)nByte + 1,FALSE,TRUE);` |
|    17 | 2039 | `	if( zOut == 0 ){` |
|   ! 0 | 2040 | `		return 0;` |
|     - | 2041 | `	}` |
|    17 | 2042 | `	SyMemcpy(zUnit,zOut,(sxu32)nByte);` |
|    17 | 2043 | `	zOut[nByte] = 0;` |
|    17 | 2044 | `	return zOut;` |
|    29 | 2045 | `}` |
|     - | 2046 | `/* Both the identify pass (pOut == 0) and the reader, exactly as php's one` |
|     - | 2047 | ` * function is. */` |
|   106 | 2048 | `static int ImgSvgHandle(ph7_context *pCtx,ImgReader *p,ImgInfo *pOut)` |
|     1 | 2049 | `{` |
|     - | 2050 | `	xmlTextReaderPtr pReader;` |
|   107 | 2051 | `	int bSvg = 0;` |
|   107 | 2052 | `	ImgRewind(p);` |
|   107 | 2053 | `	if( ImgGetc(p) != '<' ){` |
|    23 | 2054 | `		return 0; /* the cheap look php takes before it builds a parser */` |
|     - | 2055 | `	}` |
|    85 | 2056 | `	ImgRewind(p);` |
|    85 | 2057 | `	pReader = xmlReaderForIO(ImgSvgReadCb,0,(void *)p,0,0,` |
|     - | 2058 | `		XML_PARSE_NOWARNING \| XML_PARSE_NOERROR \| XML_PARSE_NONET);` |
|    85 | 2059 | `	if( pReader == 0 ){` |
|   ! 0 | 2060 | `		return 0;` |
|     - | 2061 | `	}` |
|    97 | 2062 | `	while( xmlTextReaderRead(pReader) == 1 ){` |
|     - | 2063 | `		const xmlChar *zName;` |
|     - | 2064 | `		xmlChar *zWidth,*zHeight;` |
|     - | 2065 | `		const xmlChar *zWu,*zHu;` |
|    95 | 2066 | `		if( xmlTextReaderNodeType(pReader) != XML_READER_TYPE_ELEMENT ){` |
|    13 | 2067 | `			continue;` |
|     - | 2068 | `		}` |
|     - | 2069 | `		/* the ROOT element decides, and nothing past it is read */` |
|    83 | 2070 | `		zName = xmlTextReaderConstLocalName(pReader);` |
|    82 | 2071 | `		if( zName == 0 \|\| SyStrlen((const char *)zName) != 3` |
|    81 | 2072 | `		 \|\| SyStrnicmp((const char *)zName,"svg",3) != 0 ){` |
|     4 | 2073 | `			break;` |
|     - | 2074 | `		}` |
|    77 | 2075 | `		zWidth  = xmlTextReaderGetAttribute(pReader,(const xmlChar *)"width");` |
|    77 | 2076 | `		zHeight = xmlTextReaderGetAttribute(pReader,(const xmlChar *)"height");` |
|    76 | 2077 | `		if( zWidth == 0 \|\| zHeight == 0` |
|    72 | 2078 | `		 \|\| !ImgSvgDimension(zWidth,&zWu) \|\| !ImgSvgDimension(zHeight,&zHu) ){` |
|    21 | 2079 | `			xmlFree(zWidth);` |
|    21 | 2080 | `			xmlFree(zHeight);` |
|    21 | 2081 | `			break;` |
|     - | 2082 | `		}` |
|    57 | 2083 | `		bSvg = 1;` |
|    57 | 2084 | `		if( pOut ){` |
|    43 | 2085 | `			pOut->nWidth  = ImgDigitsToU32((const char *)zWidth,` |
|    28 | 2086 | `				(int)SyStrlen((const char *)zWidth),FALSE);` |
|    43 | 2087 | `			pOut->nHeight = ImgDigitsToU32((const char *)zHeight,` |
|    28 | 2088 | `				(int)SyStrlen((const char *)zHeight),FALSE);` |
|    29 | 2089 | `			pOut->zWidthUnit  = ImgSvgUnit(pCtx,zWu);` |
|    29 | 2090 | `			pOut->zHeightUnit = ImgSvgUnit(pCtx,zHu);` |
|    14 | 2091 | `		}` |
|    57 | 2092 | `		xmlFree(zWidth);` |
|    57 | 2093 | `		xmlFree(zHeight);` |
|    57 | 2094 | `		break;` |
|   ! 0 | 2095 | `	}` |
|    85 | 2096 | `	xmlFreeTextReader(pReader);` |
|    85 | 2097 | `	return bSvg;` |
|    54 | 2098 | `}` |
|     - | 2099 | `#endif /* PH7_ENABLE_LIBXML */` |
|     - | 2100 | `/*` |
|     - | 2101 | ` * ---------------------------------------------------------------------------` |
|     - | 2102 | ` * The detection ladder.` |
|     - | 2103 | ` * ---------------------------------------------------------------------------` |
|     - | 2104 | ` * php reads THREE bytes, then four, then twelve, testing after each widening` |
|     - | 2105 | ` * -- and the position the ladder stops at is part of the contract, because` |
|     - | 2106 | ` * every reader below seeks RELATIVE to it. A ladder that read a fixed twelve` |
|     - | 2107 | ` * bytes up front would answer the same type and then read the wrong offsets.` |
|     - | 2108 | ` *` |
|     - | 2109 | ` * Two of the tests are shorter than the signature they name: PSD is matched on` |
|     - | 2110 | ` * three of its four bytes and BMP on two, so "8BPxx" is a PSD and "BM" alone` |
|     - | 2111 | ` * is a BMP. PNG is the one signature checked TWICE -- three bytes to enter the` |
|     - | 2112 | ` * branch and eight to confirm it -- and a file that entered and failed is not` |
|     - | 2113 | ` * "some other type" but php's own E_WARNING about an ASCII-mangled PNG.` |
|     - | 2114 | ` *` |
|     - | 2115 | ` * The tail is ordered by cost: the two formats with no signature at all` |
|     - | 2116 | ` * (WBMP's shape, XBM's C source) are tried only once everything else has` |
|     - | 2117 | ` * failed, and the "Error reading from" notice for a file shorter than twelve` |
|     - | 2118 | ` * bytes is raised BETWEEN them -- so a nine-byte WBMP is a size while a` |
|     - | 2119 | ` * nine-byte anything-else is a diagnostic.` |
|     - | 2120 | ` */` |
|     - | 2121 | ``/* php's `Error reading from %s!`, whose %s is the ARGUMENT the caller wrote --`` |
|     - | 2122 | ` * the file name for getimagesize() and the DATA itself for the string form,` |
|     - | 2123 | ` * both as a C string and so both stopping at the first NUL. */` |
|    26 | 2124 | `static void ImgThrowShortRead(ph7_context *pCtx,const char *zInput,int nInput)` |
|     2 | 2125 | `{` |
|    28 | 2126 | `	int n = 0;` |
|    80 | 2127 | `	while( n < nInput && zInput[n] != 0 ){` |
|    54 | 2128 | `		n++;` |
|     2 | 2129 | `	}` |
|    41 | 2130 | `	ImgThrowFmt(pCtx,PH7_CTX_NOTICE,"%s(): Error reading from %.*s!",` |
|    13 | 2131 | `		ph7_function_name(pCtx),n,zInput ? zInput : "");` |
|    28 | 2132 | `}` |
|   414 | 2133 | `static int ImgDetectType(ph7_context *pCtx,ImgReader *p,const char *zInput,int nInput)` |
|     3 | 2134 | `{` |
|     - | 2135 | `	unsigned char zSig[12];` |
|     - | 2136 | `	int bTwelve;` |
|   417 | 2137 | `	if( ImgRead(p,zSig,3) != 3 ){` |
|    10 | 2138 | `		ImgThrowShortRead(pCtx,zInput,nInput);` |
|    10 | 2139 | `		return PH7_IMG_UNKNOWN;` |
|     - | 2140 | `	}` |
|     - | 2141 | `	/* BYTES READ: 3 */` |
|   409 | 2142 | `	if( SyMemcmp(zSig,"GIF",3) == 0 ){` |
|    24 | 2143 | `		return PH7_IMG_GIF;` |
|   386 | 2144 | `	}else if( zSig[0] == 0xFF && zSig[1] == 0xD8 && zSig[2] == 0xFF ){` |
|    49 | 2145 | `		return PH7_IMG_JPEG;` |
|   338 | 2146 | `	}else if( zSig[0] == 0x89 && zSig[1] == 'P' && zSig[2] == 'N' ){` |
|    13 | 2147 | `		if( ImgRead(p,&zSig[3],5) != 5 ){` |
|     3 | 2148 | `			ImgThrowShortRead(pCtx,zInput,nInput);` |
|     3 | 2149 | `			return PH7_IMG_UNKNOWN;` |
|     - | 2150 | `		}` |
|    11 | 2151 | `		if( SyMemcmp(zSig,"\211PNG\r\n\032\n",8) == 0 ){` |
|     9 | 2152 | `			return PH7_IMG_PNG;` |
|     - | 2153 | `		}` |
|     4 | 2154 | `		ImgThrowFmt(pCtx,PH7_CTX_WARNING,"%s(): PNG file corrupted by ASCII conversion",` |
|     1 | 2155 | `			ph7_function_name(pCtx));` |
|     3 | 2156 | `		return PH7_IMG_UNKNOWN;` |
|   326 | 2157 | `	}else if( SyMemcmp(zSig,"FWS",3) == 0 ){` |
|     9 | 2158 | `		return PH7_IMG_SWF;` |
|   318 | 2159 | `	}else if( SyMemcmp(zSig,"CWS",3) == 0 ){` |
|     3 | 2160 | `		return PH7_IMG_SWC;` |
|   315 | 2161 | `	}else if( SyMemcmp(zSig,"8BP",3) == 0 ){` |
|     9 | 2162 | `		return PH7_IMG_PSD;` |
|   307 | 2163 | `	}else if( SyMemcmp(zSig,"BM",2) == 0 ){` |
|    27 | 2164 | `		return PH7_IMG_BMP;` |
|   281 | 2165 | `	}else if( zSig[0] == 0xFF && zSig[1] == 0x4F && zSig[2] == 0xFF ){` |
|     7 | 2166 | `		return PH7_IMG_JPC;` |
|   275 | 2167 | `	}else if( SyMemcmp(zSig,"RIF",3) == 0 ){` |
|    17 | 2168 | `		if( ImgRead(p,&zSig[3],9) != 9 ){` |
|     3 | 2169 | `			ImgThrowShortRead(pCtx,zInput,nInput);` |
|     3 | 2170 | `			return PH7_IMG_UNKNOWN;` |
|     - | 2171 | `		}` |
|    15 | 2172 | `		if( SyMemcmp(&zSig[8],"WEBP",4) == 0 ){` |
|    11 | 2173 | `			return PH7_IMG_WEBP;` |
|     - | 2174 | `		}` |
|     5 | 2175 | `		return PH7_IMG_UNKNOWN;` |
|     - | 2176 | `	}` |
|   259 | 2177 | `	if( ImgRead(p,&zSig[3],1) != 1 ){` |
|     5 | 2178 | `		ImgThrowShortRead(pCtx,zInput,nInput);` |
|     5 | 2179 | `		return PH7_IMG_UNKNOWN;` |
|     - | 2180 | `	}` |
|     - | 2181 | `	/* BYTES READ: 4 */` |
|   255 | 2182 | `	if( SyMemcmp(zSig,"II\052\000",4) == 0 ){` |
|    13 | 2183 | `		return PH7_IMG_TIFF_II;` |
|   243 | 2184 | `	}else if( SyMemcmp(zSig,"MM\000\052",4) == 0 ){` |
|     5 | 2185 | `		return PH7_IMG_TIFF_MM;` |
|   239 | 2186 | `	}else if( SyMemcmp(zSig,"FORM",4) == 0 ){` |
|    13 | 2187 | `		return PH7_IMG_IFF;` |
|   227 | 2188 | `	}else if( SyMemcmp(zSig,"\000\000\001\000",4) == 0 ){` |
|    13 | 2189 | `		return PH7_IMG_ICO;` |
|     - | 2190 | `	}` |
|     - | 2191 | `	/* WBMP may be shorter than twelve bytes, so the diagnostic waits. */` |
|   215 | 2192 | `	bTwelve = (ImgRead(p,&zSig[4],8) == 8);` |
|     - | 2193 | `	/* BYTES READ: 12 */` |
|   215 | 2194 | `	if( bTwelve && SyMemcmp(zSig,"\000\000\000\014jP  \r\n\207\n",12) == 0 ){` |
|     7 | 2195 | `		return PH7_IMG_JP2;` |
|     - | 2196 | `	}` |
|     - | 2197 | `	/* Neither of the next two has a signature to test: both are ISO base media` |
|     - | 2198 | `	 * files, told apart by the BRANDS inside their first box. The AVIF question` |
|     - | 2199 | `	 * is asked first on purpose (php's GH-20201) -- an AVIF also carries the` |
|     - | 2200 | ``	 * `mif1` compatible brand, so asking HEIF first would answer HEIF for every`` |
|     - | 2201 | `	 * AVIF there is. */` |
|   209 | 2202 | `	ImgRewind(p);` |
|   209 | 2203 | `	if( ImgAvifIdentify(p) ){` |
|    93 | 2204 | `		return PH7_IMG_AVIF;` |
|     - | 2205 | `	}` |
|   116 | 2206 | `	if( bTwelve && SyMemcmp(&zSig[4],"ftyp",4) == 0` |
|    58 | 2207 | `	 && (SyMemcmp(&zSig[8],"mif1",4) == 0 \|\| SyMemcmp(&zSig[8],"heic",4) == 0` |
|    11 | 2208 | `	  \|\| SyMemcmp(&zSig[8],"heix",4) == 0) ){` |
|     7 | 2209 | `		return PH7_IMG_HEIF;` |
|     - | 2210 | `	}` |
|   111 | 2211 | `	if( ImgGetWbmp(p,0) ){` |
|     7 | 2212 | `		return PH7_IMG_WBMP;` |
|     - | 2213 | `	}` |
|   105 | 2214 | `	if( !bTwelve ){` |
|    11 | 2215 | `		ImgThrowShortRead(pCtx,zInput,nInput);` |
|    11 | 2216 | `		return PH7_IMG_UNKNOWN;` |
|     - | 2217 | `	}` |
|    95 | 2218 | `	if( ImgGetXbm(pCtx,p,0) ){` |
|    17 | 2219 | `		return PH7_IMG_XBM;` |
|     - | 2220 | `	}` |
|     - | 2221 | `#ifdef PH7_ENABLE_LIBXML` |
|     - | 2222 | `	/* php consults its handler REGISTRY last, and this build registers one. */` |
|    79 | 2223 | `	if( ImgSvgHandle(pCtx,p,0) ){` |
|    29 | 2224 | `		return PH7_IMG_SVG;` |
|     - | 2225 | `	}` |
|     - | 2226 | `#endif` |
|    51 | 2227 | `	return PH7_IMG_UNKNOWN;` |
|   210 | 2228 | `}` |
|     - | 2229 | `/*` |
|     - | 2230 | ` * ---------------------------------------------------------------------------` |
|     - | 2231 | ` * The answer.` |
|     - | 2232 | ` * ---------------------------------------------------------------------------` |
|     - | 2233 | ` * php's array is index 0/1/2, an optional index 3, the two optional counts,` |
|     - | 2234 | ` * the mime string and php 8.5's two unit names. Two rules are worth stating:` |
|     - | 2235 | ` *` |
|     - | 2236 | ` *   - index 3 is a CONVENIENCE string for an <img> tag, so it exists only` |
|     - | 2237 | ` *     while both units are pixels. A reader that answers in centimetres` |
|     - | 2238 | ` *     leaves the array one entry shorter.` |
|     - | 2239 | ` *   - the size is stored as an unsigned 32-bit reading but PRINTED into index` |
|     - | 2240 | ` *     3 with a signed conversion, so a width past 2^31 is a positive integer` |
|     - | 2241 | ` *     at index 0 and a NEGATIVE number inside the tag php builds from it.` |
|     - | 2242 | ` *     Both are php's, and reproducing one without the other would be worse` |
|     - | 2243 | ` *     than reproducing neither.` |
|     - | 2244 | ` */` |
|   236 | 2245 | `static void ImgBuildAnswer(ph7_context *pCtx,int iType,ImgInfo *pInfo)` |
|     2 | 2246 | `{` |
|     - | 2247 | `	ph7_value *pArray,*pVal;` |
|     - | 2248 | `	char zTag[128];` |
|     - | 2249 | `	int nTag;` |
|   238 | 2250 | `	pArray = ph7_context_new_array(pCtx);` |
|   238 | 2251 | `	pVal   = ph7_context_new_scalar(pCtx);` |
|   238 | 2252 | `	if( pArray == 0 \|\| pVal == 0 ){` |
|   ! 0 | 2253 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2254 | `		return;` |
|     - | 2255 | `	}` |
|   238 | 2256 | `	ph7_value_int64(pVal,(ph7_int64)pInfo->nWidth);` |
|   238 | 2257 | `	ph7_array_add_intkey_elem(pArray,0,pVal);` |
|   238 | 2258 | `	ph7_value_int64(pVal,(ph7_int64)pInfo->nHeight);` |
|   238 | 2259 | `	ph7_array_add_intkey_elem(pArray,1,pVal);` |
|   238 | 2260 | `	ph7_value_int(pVal,iType);` |
|   238 | 2261 | `	ph7_array_add_intkey_elem(pArray,2,pVal);` |
|   236 | 2262 | `	if( (pInfo->zWidthUnit == 0 \|\| SyStrncmp(pInfo->zWidthUnit,"px",3) == 0)` |
|   236 | 2263 | `	 && (pInfo->zHeightUnit == 0 \|\| SyStrncmp(pInfo->zHeightUnit,"px",3) == 0) ){` |
|   458 | 2264 | `		nTag = (int)SyBufferFormat(zTag,sizeof(zTag),"width=\"%d\" height=\"%d\"",` |
|   228 | 2265 | `			(int)pInfo->nWidth,(int)pInfo->nHeight);` |
|   230 | 2266 | `		ph7_value_reset_string_cursor(pVal);` |
|   230 | 2267 | `		ph7_value_string(pVal,zTag,nTag);` |
|   230 | 2268 | `		ph7_array_add_intkey_elem(pArray,3,pVal);` |
|   114 | 2269 | `	}` |
|   238 | 2270 | `	if( pInfo->nBits != 0 ){` |
|   158 | 2271 | `		ph7_value_int64(pVal,(ph7_int64)pInfo->nBits);` |
|   158 | 2272 | `		ph7_array_add_strkey_elem(pArray,"bits",pVal);` |
|    78 | 2273 | `	}` |
|   238 | 2274 | `	if( pInfo->nChannels != 0 ){` |
|   124 | 2275 | `		ph7_value_int64(pVal,(ph7_int64)pInfo->nChannels);` |
|   124 | 2276 | `		ph7_array_add_strkey_elem(pArray,"channels",pVal);` |
|    61 | 2277 | `	}` |
|   238 | 2278 | `	ph7_value_reset_string_cursor(pVal);` |
|   238 | 2279 | `	ph7_value_string(pVal,PH7_ImageTypeMime(iType),-1);` |
|   238 | 2280 | `	ph7_array_add_strkey_elem(pArray,"mime",pVal);` |
|   238 | 2281 | `	ph7_value_reset_string_cursor(pVal);` |
|   238 | 2282 | `	ph7_value_string(pVal,pInfo->zWidthUnit ? pInfo->zWidthUnit : "px",-1);` |
|   238 | 2283 | `	ph7_array_add_strkey_elem(pArray,"width_unit",pVal);` |
|   238 | 2284 | `	ph7_value_reset_string_cursor(pVal);` |
|   238 | 2285 | `	ph7_value_string(pVal,pInfo->zHeightUnit ? pInfo->zHeightUnit : "px",-1);` |
|   238 | 2286 | `	ph7_array_add_strkey_elem(pArray,"height_unit",pVal);` |
|   238 | 2287 | `	ph7_context_release_value(pCtx,pVal);` |
|   238 | 2288 | `	ph7_result_value(pCtx,pArray);` |
|   120 | 2289 | `}` |
|     - | 2290 | `/*` |
|     - | 2291 | ` * Detect, then dispatch. The reader a type names is the ONLY thing that runs;` |
|     - | 2292 | ` * an unrecognized file answers false with no further reading, which is why the` |
|     - | 2293 | ` * ladder's own diagnostics are the only ones a script sees for one.` |
|     - | 2294 | ` */` |
|   414 | 2295 | `static int ImgReadAny(ph7_context *pCtx,ImgReader *p,const char *zInput,int nInput,ph7_value *pInfo)` |
|     3 | 2296 | `{` |
|     - | 2297 | `	ImgInfo sInfo;` |
|   417 | 2298 | `	int iType,bHave = 0;` |
|   417 | 2299 | `	SyZero(&sInfo,sizeof(sInfo));` |
|   417 | 2300 | `	iType = ImgDetectType(pCtx,p,zInput,nInput);` |
|   417 | 2301 | `	switch( iType ){` |
|    24 | 2302 | `	case PH7_IMG_GIF:     bHave = ImgHandleGif(p,&sInfo);           break;` |
|    49 | 2303 | `	case PH7_IMG_JPEG:    bHave = ImgHandleJpeg(pCtx,p,&sInfo,pInfo); break;` |
|     9 | 2304 | `	case PH7_IMG_PNG:     bHave = ImgHandlePng(p,&sInfo);           break;` |
|     9 | 2305 | `	case PH7_IMG_SWF:     bHave = ImgHandleSwf(p,&sInfo);           break;` |
|     1 | 2306 | `	case PH7_IMG_SWC:` |
|     - | 2307 | `		/* php reads a compressed SWF through zlib and says so when the build it` |
|     - | 2308 | `		 * runs on has none. This engine links no zlib (a scope cut, so the` |
|     - | 2309 | ``		 * `compress.zlib` stream filter is absent for the same reason), which`` |
|     - | 2310 | `		 * makes php's own no-zlib sentence the honest answer rather than a` |
|     - | 2311 | `		 * stub: the TYPE is still IMAGETYPE_SWC and the size is still refused. */` |
|     4 | 2312 | `		ImgThrowFmt(pCtx,PH7_CTX_NOTICE,` |
|     - | 2313 | `			"%s(): The image is a compressed SWF file, but you do not have a static version of the zlib extension enabled",` |
|     1 | 2314 | `			ph7_function_name(pCtx));` |
|     3 | 2315 | `		break;` |
|     9 | 2316 | `	case PH7_IMG_PSD:     bHave = ImgHandlePsd(p,&sInfo);           break;` |
|    27 | 2317 | `	case PH7_IMG_BMP:     bHave = ImgHandleBmp(p,&sInfo);           break;` |
|    13 | 2318 | `	case PH7_IMG_TIFF_II: bHave = ImgHandleTiff(pCtx,p,&sInfo,0);   break;` |
|     5 | 2319 | `	case PH7_IMG_TIFF_MM: bHave = ImgHandleTiff(pCtx,p,&sInfo,1);   break;` |
|     7 | 2320 | `	case PH7_IMG_JPC:     bHave = ImgHandleJpc(pCtx,p,&sInfo);      break;` |
|     7 | 2321 | `	case PH7_IMG_JP2:     bHave = ImgHandleJp2(pCtx,p,&sInfo);      break;` |
|    13 | 2322 | `	case PH7_IMG_IFF:     bHave = ImgHandleIff(p,&sInfo);           break;` |
|     7 | 2323 | `	case PH7_IMG_WBMP:    bHave = ImgGetWbmp(p,&sInfo);             break;` |
|    17 | 2324 | `	case PH7_IMG_XBM:     bHave = ImgGetXbm(pCtx,p,&sInfo);         break;` |
|    13 | 2325 | `	case PH7_IMG_ICO:     bHave = ImgHandleIco(p,&sInfo);           break;` |
|    11 | 2326 | `	case PH7_IMG_WEBP:    bHave = ImgHandleWebp(p,&sInfo);          break;` |
|    46 | 2327 | `	case PH7_IMG_AVIF:` |
|     - | 2328 | `		/* The identify pass stopped just past the "ftyp" box, which is exactly` |
|     - | 2329 | `		 * where the feature walk wants to start. */` |
|    93 | 2330 | `		bHave = ImgHandleAvif(p,&sInfo);` |
|    93 | 2331 | `		break;` |
|     - | 2332 | `#ifdef PH7_ENABLE_LIBXML` |
|    29 | 2333 | `	case PH7_IMG_SVG:     bHave = ImgSvgHandle(pCtx,p,&sInfo);      break;` |
|     - | 2334 | `#endif` |
|     3 | 2335 | `	case PH7_IMG_HEIF:` |
|     - | 2336 | `		/* Told apart by its brand alone, so the same walk runs -- from the` |
|     - | 2337 | `		 * beginning, skipping the "ftyp" on its way to "meta". */` |
|     7 | 2338 | `		ImgRewind(p);` |
|     7 | 2339 | `		bHave = ImgHandleAvif(p,&sInfo);` |
|     6 | 2340 | `		break;` |
|    41 | 2341 | `	default:` |
|    82 | 2342 | `		break;` |
|     - | 2343 | `	}` |
|   417 | 2344 | `	if( !bHave ){` |
|   181 | 2345 | `		ph7_result_bool(pCtx,0);` |
|   181 | 2346 | `		return PH7_OK;` |
|     - | 2347 | `	}` |
|   238 | 2348 | `	ImgBuildAnswer(pCtx,iType,&sInfo);` |
|   238 | 2349 | `	return PH7_OK;` |
|   210 | 2350 | `}` |
|     - | 2351 | `/*` |
|     - | 2352 | ` * array\|false getimagesize(string $filename,&$image_info = null)` |
|     - | 2353 | ` * array\|false getimagesizefromstring(string $string,&$image_info = null)` |
|     - | 2354 | ` *` |
|     - | 2355 | ` * The out-parameter is created EMPTY before anything is opened, so a caller` |
|     - | 2356 | ` * that names it reads back an array even when the whole call fails -- and the` |
|     - | 2357 | ` * NUL-byte refusal on $filename is the path rule (VmBuiltinPathMask), which` |
|     - | 2358 | ` * the string form does not carry: its argument is data, and php's own` |
|     - | 2359 | ` * diagnostic prints it as a C string, stopping at the first NUL.` |
|     - | 2360 | ` */` |
|   420 | 2361 | `static int ImgSizeCommon(ph7_context *pCtx,int nArg,ph7_value **apArg,int bFromString)` |
|     3 | 2362 | `{` |
|     - | 2363 | `	ImgReader sReader;` |
|   423 | 2364 | `	ph7_value *pInfo = 0;` |
|     - | 2365 | `	const char *zOrig,*zName;` |
|   423 | 2366 | `	int nOrig,rc = PH7_OK;` |
|   423 | 2367 | `	SyZero(&sReader,sizeof(sReader));` |
|   423 | 2368 | `	sReader.pCtx = pCtx;` |
|   423 | 2369 | `	zOrig = ph7_value_to_string(apArg[0],&nOrig);` |
|     - | 2370 | `	/* php initialises the out-parameter BEFORE it opens anything, so a caller` |
|     - | 2371 | `	 * that names one reads back an empty array even when the call fails. */` |
|   423 | 2372 | `	if( nArg > 1 ){` |
|    52 | 2373 | `		pInfo = ph7_context_new_array(pCtx);` |
|    52 | 2374 | `		if( pInfo == 0 ){` |
|   ! 0 | 2375 | `			return PH7_OK;` |
|     - | 2376 | `		}` |
|    25 | 2377 | `	}` |
|   423 | 2378 | `	if( bFromString ){` |
|   409 | 2379 | `		sReader.zData = (const unsigned char *)zOrig;` |
|   409 | 2380 | `		sReader.nData = (ph7_int64)nOrig;` |
|   409 | 2381 | `		rc = ImgReadAny(pCtx,&sReader,zOrig,nOrig,pInfo);` |
|   206 | 2382 | `	}else{` |
|     - | 2383 | `		const ph7_io_stream *pStream;` |
|     - | 2384 | `		void *pHandle;` |
|    15 | 2385 | `		zName = zOrig;` |
|    15 | 2386 | `		if( PH7_VfsEmptyPathRefused(pCtx,nOrig) ){` |
|     2 | 2387 | `			return PH7_OK;` |
|     - | 2388 | `		}` |
|    13 | 2389 | `		pStream = PH7_VfsStreamDeviceOrFile(pCtx,&zName,nOrig);` |
|    13 | 2390 | `		if( pStream == 0 ){` |
|   ! 0 | 2391 | `			VfsThrowNoDeviceWarning(pCtx,zName,FALSE);` |
|   ! 0 | 2392 | `			ph7_result_bool(pCtx,0);` |
|   ! 0 | 2393 | `		}else{` |
|    19 | 2394 | `			pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zName,PH7_IO_OPEN_RDONLY,` |
|     6 | 2395 | `				FALSE,0,FALSE,0,ph7_function_name(pCtx));` |
|    13 | 2396 | `			if( pHandle == 0 ){` |
|     5 | 2397 | `				VfsThrowOpenWarning(pCtx,zName);` |
|     5 | 2398 | `				ph7_result_bool(pCtx,0);` |
|     3 | 2399 | `			}else{` |
|     9 | 2400 | `				sReader.pStream = pStream;` |
|     9 | 2401 | `				sReader.pHandle = pHandle;` |
|     9 | 2402 | `				rc = ImgReadAny(pCtx,&sReader,zOrig,nOrig,pInfo);` |
|     9 | 2403 | `				PH7_StreamCloseHandle(pStream,pHandle);` |
|     - | 2404 | `			}` |
|     - | 2405 | `		}` |
|     - | 2406 | `	}` |
|   421 | 2407 | `	if( pInfo ){` |
|    52 | 2408 | `		PH7_VmStoreArgByRef(pCtx->pVm,apArg[1],pInfo);` |
|    52 | 2409 | `		ph7_context_release_value(pCtx,pInfo);` |
|    25 | 2410 | `	}` |
|   421 | 2411 | `	return rc;` |
|   213 | 2412 | `}` |
|    14 | 2413 | `PH7_PRIVATE int PH7_builtin_getimagesize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2414 | `{` |
|    15 | 2415 | `	return ImgSizeCommon(pCtx,nArg,apArg,FALSE);` |
|     1 | 2416 | `}` |
|   406 | 2417 | `PH7_PRIVATE int PH7_builtin_getimagesizefromstring(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 | 2418 | `{` |
|   409 | 2419 | `	return ImgSizeCommon(pCtx,nArg,apArg,TRUE);` |
|     3 | 2420 | `}` |
|     - | 2421 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     - | 2422 |  |
