/**
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
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
#endif /* PH7_DISABLE_BUILTIN_FUNC */
