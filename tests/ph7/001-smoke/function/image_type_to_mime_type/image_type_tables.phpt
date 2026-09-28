--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The IMAGETYPE space and its two disagreeing lookups
--FILE--
<?php
/* php answers an image type from two separate switches, and they do not know
 * the same set of names. The mime lookup is TOTAL -- every integer has an
 * answer -- and the one it gives an unknown type is the same
 * "application/octet-stream" it gives JPC, so the mime string cannot tell a
 * raw codestream from a file nothing recognized. The extension lookup is
 * PARTIAL -- an unknown type is FALSE -- and it splits three names the mime
 * lookup collapses (.jpc/.jpx/.jb2) while collapsing one the mime lookup
 * splits (WBMP is image/vnd.wap.wbmp and .bmp). $include_dot is not a branch
 * either: every stored extension carries its dot and the flag is a one-byte
 * offset into it. */
function image_type_tables(): void {
    echo "## every fixed type, both lookups, and the alias that is not a type\n";
    $named = [
        'GIF' => IMAGETYPE_GIF, 'JPEG' => IMAGETYPE_JPEG, 'PNG' => IMAGETYPE_PNG,
        'SWF' => IMAGETYPE_SWF, 'PSD' => IMAGETYPE_PSD, 'BMP' => IMAGETYPE_BMP,
        'TIFF_II' => IMAGETYPE_TIFF_II, 'TIFF_MM' => IMAGETYPE_TIFF_MM,
        'JPC' => IMAGETYPE_JPC, 'JP2' => IMAGETYPE_JP2, 'JPX' => IMAGETYPE_JPX,
        'JB2' => IMAGETYPE_JB2, 'SWC' => IMAGETYPE_SWC, 'IFF' => IMAGETYPE_IFF,
        'WBMP' => IMAGETYPE_WBMP, 'JPEG2000' => IMAGETYPE_JPEG2000,
        'XBM' => IMAGETYPE_XBM, 'ICO' => IMAGETYPE_ICO, 'WEBP' => IMAGETYPE_WEBP,
        'AVIF' => IMAGETYPE_AVIF, 'HEIF' => IMAGETYPE_HEIF, 'SVG' => IMAGETYPE_SVG,
        'UNKNOWN' => IMAGETYPE_UNKNOWN,
    ];
    foreach ($named as $name => $type) {
        printf("%-9s %2d %-30s %-6s %s\n", $name, $type,
            image_type_to_mime_type($type),
            var_export(image_type_to_extension($type), true),
            var_export(image_type_to_extension($type, false), true));
    }

    echo "## IMAGETYPE_JPEG2000 is an ALIAS for JPC and not a value of its own\n";
    var_dump(IMAGETYPE_JPEG2000 === IMAGETYPE_JPC);

    echo "## COUNT counts the fixed types plus every registered handler\n";
    var_dump(IMAGETYPE_COUNT === IMAGETYPE_SVG + 1);

    echo "## outside the space: the mime lookup still answers, the other does not\n";
    foreach ([-1, IMAGETYPE_COUNT, 100, PHP_INT_MAX, PHP_INT_MIN] as $type) {
        printf("%-21s %-26s %s\n", $type, image_type_to_mime_type($type),
            var_export(image_type_to_extension($type), true));
    }

    echo "## the flag is an offset, so anything falsy drops the dot\n";
    foreach ([true, false, 0, 1, '', '0', 'x'] as $flag) {
        printf("%-5s %s\n", var_export($flag, true),
            var_export(image_type_to_extension(IMAGETYPE_GIF, $flag), true));
    }

    echo "## the two arity refusals\n";
    foreach ([
        fn() => image_type_to_mime_type(),
        fn() => image_type_to_mime_type(1, 2),
        fn() => image_type_to_extension(),
        fn() => image_type_to_extension(1, true, 3),
        fn() => image_type_to_mime_type('x'),
        fn() => image_type_to_extension([1]),
    ] as $probe) {
        try { $probe(); } catch (Throwable $e) { printf("%s: %s\n", get_class($e), $e->getMessage()); }
    }
}
image_type_tables();
--EXPECT--
## every fixed type, both lookups, and the alias that is not a type
GIF        1 image/gif                      '.gif' 'gif'
JPEG       2 image/jpeg                     '.jpeg' 'jpeg'
PNG        3 image/png                      '.png' 'png'
SWF        4 application/x-shockwave-flash  '.swf' 'swf'
PSD        5 image/psd                      '.psd' 'psd'
BMP        6 image/bmp                      '.bmp' 'bmp'
TIFF_II    7 image/tiff                     '.tiff' 'tiff'
TIFF_MM    8 image/tiff                     '.tiff' 'tiff'
JPC        9 application/octet-stream       '.jpc' 'jpc'
JP2       10 image/jp2                      '.jp2' 'jp2'
JPX       11 application/octet-stream       '.jpx' 'jpx'
JB2       12 application/octet-stream       '.jb2' 'jb2'
SWC       13 application/x-shockwave-flash  '.swf' 'swf'
IFF       14 image/iff                      '.iff' 'iff'
WBMP      15 image/vnd.wap.wbmp             '.bmp' 'bmp'
JPEG2000   9 application/octet-stream       '.jpc' 'jpc'
XBM       16 image/xbm                      '.xbm' 'xbm'
ICO       17 image/vnd.microsoft.icon       '.ico' 'ico'
WEBP      18 image/webp                     '.webp' 'webp'
AVIF      19 image/avif                     '.avif' 'avif'
HEIF      20 image/heif                     '.heif' 'heif'
SVG       21 image/svg+xml                  '.svg' 'svg'
UNKNOWN    0 application/octet-stream       false  false
## IMAGETYPE_JPEG2000 is an ALIAS for JPC and not a value of its own
bool(true)
## COUNT counts the fixed types plus every registered handler
bool(true)
## outside the space: the mime lookup still answers, the other does not
-1                    application/octet-stream   false
22                    application/octet-stream   false
100                   application/octet-stream   false
9223372036854775807   application/octet-stream   false
-9223372036854775808  application/octet-stream   false
## the flag is an offset, so anything falsy drops the dot
true  '.gif'
false 'gif'
0     'gif'
1     '.gif'
''    'gif'
'0'   'gif'
'x'   '.gif'
## the two arity refusals
ArgumentCountError: image_type_to_mime_type() expects exactly 1 argument, 0 given
ArgumentCountError: image_type_to_mime_type() expects exactly 1 argument, 2 given
ArgumentCountError: image_type_to_extension() expects at least 1 argument, 0 given
ArgumentCountError: image_type_to_extension() expects at most 2 arguments, 3 given
TypeError: image_type_to_mime_type(): Argument #1 ($image_type) must be of type int, string given
TypeError: image_type_to_extension(): Argument #1 ($image_type) must be of type int, array given
