--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The fifteen container headers getimagesize() reads
--FILE--
<?php
/* Each format below is read from a FIXED offset that the detection ladder's
 * own stopping point decides, so the header is the whole contract. What the
 * cases pin, beyond the offsets, is where the formats disagree about the same
 * question: a GIF states three channels and a depth only when it carries a
 * colour table, a BMP's DIB header SIZE picks between two layouts and its
 * height is signed, an ICO answers for its deepest entry with zero meaning
 * 256, a WEBP spells its size three different ways, and a TIFF answers only
 * when the directory carried BOTH tags. */
function gis_show(string $label, string $bytes): void {
    $r = @getimagesizefromstring($bytes);
    printf("%-14s %s\n", $label, $r === false ? 'false' : json_encode($r));
}
function getimagesize_containers(): void {
    echo "## GIF: the packed byte carries the depth only with a colour table\n";
    foreach ([0x00, 0x80, 0x87, 0xf0, 0xff] as $packed) {
        gis_show('gif ' . sprintf('%02x', $packed),
            "GIF89a" . pack('v', 7) . pack('v', 9) . chr($packed));
    }
    gis_show('gif short', "GIF89a\x07\x00\x09");

    echo "## PNG: IHDR is fixed, and a 32-bit width is unsigned at [0] and signed in [3]\n";
    $ihdr = fn(string $w, string $h) =>
        "\x89PNG\r\n\x1a\n" . "\x00\x00\x00\x0dIHDR" . $w . $h . "\x08\x02\x00\x00\x00";
    gis_show('png', $ihdr("\x00\x00\x00\x0a", "\x00\x00\x00\x14"));
    gis_show('png wide', $ihdr("\xff\xff\xff\xff", "\x00\x00\x00\x0a"));
    gis_show('png hi', $ihdr("\x80\x00\x00\x00", "\x7f\xff\xff\xff"));

    echo "## BMP: the DIB size picks the layout, and the height is |signed|\n";
    $bmp = fn(int $w, int $h, int $bits, int $size = 40) =>
        "BM" . str_repeat("\x00", 8) . "\x36\x00\x00\x00"
        . pack('V', $size) . pack('V', $w) . pack('V', $h) . pack('v', 1) . pack('v', $bits)
        . str_repeat("\x00", 8);
    foreach ([11, 12, 13, 40, 64, 65, 108, 124, 125] as $size) {
        gis_show("bmp size $size", $bmp(3, 4, 1, $size));
    }
    gis_show('bmp top-down', $bmp(10, -5, 24));
    gis_show('bmp minheight', $bmp(10, -2147483648, 24));
    gis_show('bmp core', "BM" . str_repeat("\x00", 8) . "\x1a\x00\x00\x00"
        . pack('V', 12) . pack('v', 7) . pack('v', 9) . pack('v', 1) . pack('v', 8)
        . str_repeat("\x00", 8));

    echo "## PSD is matched on THREE of its four signature bytes, and states height first\n";
    gis_show('psd', "8BPS\x00\x01" . str_repeat("\x00", 6) . "\x00\x03"
        . pack('N', 11) . pack('N', 22));
    gis_show('psd 3-byte', "8BPx\x00\x01" . str_repeat("\x00", 6) . "\x00\x03"
        . pack('N', 11) . pack('N', 22));
    gis_show('psd short', "8BPS\x00\x01" . str_repeat("\x00", 6) . "\x00\x03" . pack('N', 11));

    echo "## SWF: a bit-packed RECT in twips (a twentieth of a pixel)\n";
    $rect = function (int $nbits, array $vals): string {
        $bits = str_pad(decbin($nbits), 5, '0', STR_PAD_LEFT);
        foreach ($vals as $v) {
            $bits .= str_pad(decbin($v & ((1 << $nbits) - 1)), $nbits, '0', STR_PAD_LEFT);
        }
        $bits = str_pad($bits, 256, '0');
        $out = '';
        for ($i = 0; $i < 256; $i += 8) { $out .= chr(bindec(substr($bits, $i, 8))); }
        return $out;
    };
    gis_show('swf', "FWS\x06" . pack('V', 0) . $rect(15, [0, 11000, 0, 8000]));
    gis_show('swf offset', "FWS\x06" . pack('V', 0) . $rect(15, [200, 11200, 100, 8100]));
    gis_show('swf short', "FWS\x06" . pack('V', 0) . "\x0f");

    echo "## ICO: the deepest entry wins, and a stored zero means 256\n";
    $icodir = fn(array $e) => "\x00\x00\x01\x00" . pack('v', count($e))
        . implode('', array_map(
            fn($x) => chr($x[0]) . chr($x[1]) . "\x00\x00\x01\x00" . pack('v', $x[2]) . str_repeat("\x00", 8),
            $e));
    gis_show('ico one', $icodir([[16, 16, 8]]));
    gis_show('ico deepest', $icodir([[16, 16, 8], [32, 32, 32], [64, 64, 4]]));
    gis_show('ico 256', $icodir([[0, 0, 8]]));
    gis_show('ico none', "\x00\x00\x01\x00" . pack('v', 0));
    gis_show('ico 256 dir', "\x00\x00\x01\x00" . pack('v', 256));

    echo "## WEBP: three flavours, three spellings of the same size\n";
    gis_show('webp lossy', "RIFF" . pack('V', 0) . "WEBP" . "VP8 " . str_repeat("\x00", 10)
        . pack('v', 0x0107) . pack('v', 0x0209));
    gis_show('webp lossless', "RIFF" . pack('V', 0) . "WEBP" . "VP8L"
        . "\x00\x00\x00\x00\x2f" . "\x00\x40\x1a\x00" . "\x00\x00\x00\x00\x00");
    gis_show('webp extended', "RIFF" . pack('V', 0) . "WEBP" . "VP8X"
        . str_repeat("\x00", 8) . "\x09\x00\x00" . "\x04\x00\x00");
    gis_show('webp other', "RIFF" . pack('V', 0) . "WEBP" . "VP9 " . str_repeat("\x00", 10));
    gis_show('riff other', "RIFF" . pack('V', 0) . "AVI " . str_repeat("\x00", 10));

    echo "## TIFF: both byte orders, and only when the directory carried BOTH tags\n";
    $entry = fn(int $tag, int $type, int $val) =>
        pack('v', $tag) . pack('v', $type) . pack('V', 1) . pack('V', $val);
    gis_show('tiff ii', "II\x2a\x00" . pack('V', 8) . pack('v', 2)
        . $entry(0x0100, 3, 7) . $entry(0x0101, 3, 9) . pack('V', 0));
    gis_show('tiff width only', "II\x2a\x00" . pack('V', 8) . pack('v', 1)
        . $entry(0x0100, 3, 7) . pack('V', 0));
    gis_show('tiff exif tags', "II\x2a\x00" . pack('V', 8) . pack('v', 2)
        . $entry(0xA002, 4, 5) . $entry(0xA003, 4, 6) . pack('V', 0));
    gis_show('tiff rational', "II\x2a\x00" . pack('V', 8) . pack('v', 2)
        . $entry(0x0100, 5, 7) . $entry(0x0101, 3, 9) . pack('V', 0));
    gis_show('tiff addr 0', "II\x2a\x00" . pack('V', 0) . str_repeat("\x00", 40));
    gis_show('tiff mm', "MM\x00\x2a" . pack('N', 8) . pack('n', 2)
        . pack('n', 0x0100) . pack('n', 3) . pack('N', 1) . pack('N', 7 << 16)
        . pack('n', 0x0101) . pack('n', 3) . pack('N', 1) . pack('N', 9 << 16)
        . pack('N', 0));

    echo "## IFF: FORM chunks, even-padded, and a BMHD php refuses does not stop the walk\n";
    $bmhd = fn(int $w, int $h, int $bits) => "BMHD" . pack('N', 20)
        . pack('n', $w) . pack('n', $h) . "\x00\x00\x00\x00" . chr($bits) . str_repeat("\x00", 11);
    gis_show('iff ilbm', "FORM" . pack('N', 100) . "ILBM" . $bmhd(4, 1, 4));
    gis_show('iff pbm', "FORM" . pack('N', 100) . "PBM " . $bmhd(4, 1, 4));
    gis_show('iff other', "FORM" . pack('N', 100) . "8SVX" . $bmhd(4, 1, 4));
    gis_show('iff bad bmhd', "FORM" . pack('N', 100) . "ILBM" . $bmhd(0, 1, 4) . str_repeat("j", 40));
    gis_show('iff odd chunk', "FORM" . pack('N', 100) . "ILBM"
        . "JUNK" . pack('N', 3) . "abcd" . $bmhd(4, 1, 4));

    echo "## JPEG 2000: the raw codestream and the box wrapper over it\n";
    $siz = "\xff\x4f\xff\x51" . pack('n', 41) . pack('n', 0) . pack('N', 5) . pack('N', 6)
        . str_repeat("\x00", 24) . pack('n', 3) . "\x07\x01\x01\x0b\x01\x01\x07\x01\x01";
    gis_show('jpc', $siz);
    gis_show('jpc no siz', "\xff\x4f\xff\x52junk");
    gis_show('jp2', "\x00\x00\x00\x0cjP  \r\n\x87\n"
        . pack('N', strlen($siz) + 5) . "jp2c" . $siz);
    gis_show('jp2 empty', "\x00\x00\x00\x0cjP  \r\n\x87\n"
        . pack('N', 16) . "ftypjp2 " . str_repeat("\x00", 4));

    echo "## WBMP and XBM have no signature, so they are the ladder's last two tries\n";
    gis_show('wbmp', "\x00\x00" . chr(75) . chr(50));
    gis_show('wbmp multibyte', "\x00\x00" . "\x90\x00" . chr(50));
    gis_show('wbmp too wide', "\x00\x00" . "\xff\x7f" . chr(50));
    gis_show('wbmp zero', "\x00\x00\x00\x32");
    gis_show('xbm', "#define x_width 75\n#define x_height 50\n");
    gis_show('xbm reversed', "#define height 50\n#define width 75\n");
    gis_show('xbm indented', " #define x_width 3\n#define x_height 50\n");
    gis_show('xbm no under', "#define w 75\n#define h 50\n");
    gis_show('xbm negative', "#define x_width -3\n#define x_height 50\n");
    gis_show('xbm no newline', "#define x_width 3\n#define x_height 50");
    gis_show('xbm glued', "#define x_width\x013\n#define x_height 50\n");
    gis_show('xbm nul', "#define x_width 3\x00junk\n#define x_height 50\n");
    gis_show('xbm wraps', "#define x_width 4294967297\n#define x_height 50\n");
    gis_show('xbm saturates', "#define x_width 99999999999999999999\n#define x_height 50\n");
}
getimagesize_containers();
--EXPECT--
## GIF: the packed byte carries the depth only with a colour table
gif 00         {"0":7,"1":9,"2":1,"3":"width=\"7\" height=\"9\"","channels":3,"mime":"image\/gif","width_unit":"px","height_unit":"px"}
gif 80         {"0":7,"1":9,"2":1,"3":"width=\"7\" height=\"9\"","bits":1,"channels":3,"mime":"image\/gif","width_unit":"px","height_unit":"px"}
gif 87         {"0":7,"1":9,"2":1,"3":"width=\"7\" height=\"9\"","bits":8,"channels":3,"mime":"image\/gif","width_unit":"px","height_unit":"px"}
gif f0         {"0":7,"1":9,"2":1,"3":"width=\"7\" height=\"9\"","bits":1,"channels":3,"mime":"image\/gif","width_unit":"px","height_unit":"px"}
gif ff         {"0":7,"1":9,"2":1,"3":"width=\"7\" height=\"9\"","bits":8,"channels":3,"mime":"image\/gif","width_unit":"px","height_unit":"px"}
gif short      false
## PNG: IHDR is fixed, and a 32-bit width is unsigned at [0] and signed in [3]
png            {"0":10,"1":20,"2":3,"3":"width=\"10\" height=\"20\"","bits":8,"mime":"image\/png","width_unit":"px","height_unit":"px"}
png wide       {"0":4294967295,"1":10,"2":3,"3":"width=\"-1\" height=\"10\"","bits":8,"mime":"image\/png","width_unit":"px","height_unit":"px"}
png hi         {"0":2147483648,"1":2147483647,"2":3,"3":"width=\"-2147483648\" height=\"2147483647\"","bits":8,"mime":"image\/png","width_unit":"px","height_unit":"px"}
## BMP: the DIB size picks the layout, and the height is |signed|
bmp size 11    false
bmp size 12    {"0":3,"1":0,"2":6,"3":"width=\"3\" height=\"0\"","mime":"image\/bmp","width_unit":"px","height_unit":"px"}
bmp size 13    {"0":3,"1":4,"2":6,"3":"width=\"3\" height=\"4\"","bits":1,"mime":"image\/bmp","width_unit":"px","height_unit":"px"}
bmp size 40    {"0":3,"1":4,"2":6,"3":"width=\"3\" height=\"4\"","bits":1,"mime":"image\/bmp","width_unit":"px","height_unit":"px"}
bmp size 64    {"0":3,"1":4,"2":6,"3":"width=\"3\" height=\"4\"","bits":1,"mime":"image\/bmp","width_unit":"px","height_unit":"px"}
bmp size 65    false
bmp size 108   {"0":3,"1":4,"2":6,"3":"width=\"3\" height=\"4\"","bits":1,"mime":"image\/bmp","width_unit":"px","height_unit":"px"}
bmp size 124   {"0":3,"1":4,"2":6,"3":"width=\"3\" height=\"4\"","bits":1,"mime":"image\/bmp","width_unit":"px","height_unit":"px"}
bmp size 125   false
bmp top-down   {"0":10,"1":5,"2":6,"3":"width=\"10\" height=\"5\"","bits":24,"mime":"image\/bmp","width_unit":"px","height_unit":"px"}
bmp minheight  {"0":10,"1":2147483648,"2":6,"3":"width=\"10\" height=\"-2147483648\"","bits":24,"mime":"image\/bmp","width_unit":"px","height_unit":"px"}
bmp core       {"0":7,"1":9,"2":6,"3":"width=\"7\" height=\"9\"","mime":"image\/bmp","width_unit":"px","height_unit":"px"}
## PSD is matched on THREE of its four signature bytes, and states height first
psd            {"0":22,"1":11,"2":5,"3":"width=\"22\" height=\"11\"","mime":"image\/psd","width_unit":"px","height_unit":"px"}
psd 3-byte     {"0":22,"1":11,"2":5,"3":"width=\"22\" height=\"11\"","mime":"image\/psd","width_unit":"px","height_unit":"px"}
psd short      false
## SWF: a bit-packed RECT in twips (a twentieth of a pixel)
swf            {"0":550,"1":400,"2":4,"3":"width=\"550\" height=\"400\"","mime":"application\/x-shockwave-flash","width_unit":"px","height_unit":"px"}
swf offset     {"0":550,"1":400,"2":4,"3":"width=\"550\" height=\"400\"","mime":"application\/x-shockwave-flash","width_unit":"px","height_unit":"px"}
swf short      false
## ICO: the deepest entry wins, and a stored zero means 256
ico one        {"0":16,"1":16,"2":17,"3":"width=\"16\" height=\"16\"","bits":8,"mime":"image\/vnd.microsoft.icon","width_unit":"px","height_unit":"px"}
ico deepest    {"0":32,"1":32,"2":17,"3":"width=\"32\" height=\"32\"","bits":32,"mime":"image\/vnd.microsoft.icon","width_unit":"px","height_unit":"px"}
ico 256        {"0":256,"1":256,"2":17,"3":"width=\"256\" height=\"256\"","bits":8,"mime":"image\/vnd.microsoft.icon","width_unit":"px","height_unit":"px"}
ico none       false
ico 256 dir    false
## WEBP: three flavours, three spellings of the same size
webp lossy     {"0":263,"1":521,"2":18,"3":"width=\"263\" height=\"521\"","bits":8,"mime":"image\/webp","width_unit":"px","height_unit":"px"}
webp lossless  {"0":1,"1":106,"2":18,"3":"width=\"1\" height=\"106\"","bits":8,"mime":"image\/webp","width_unit":"px","height_unit":"px"}
webp extended  {"0":10,"1":5,"2":18,"3":"width=\"10\" height=\"5\"","bits":8,"mime":"image\/webp","width_unit":"px","height_unit":"px"}
webp other     false
riff other     false
## TIFF: both byte orders, and only when the directory carried BOTH tags
tiff ii        {"0":7,"1":9,"2":7,"3":"width=\"7\" height=\"9\"","mime":"image\/tiff","width_unit":"px","height_unit":"px"}
tiff width only false
tiff exif tags {"0":5,"1":6,"2":7,"3":"width=\"5\" height=\"6\"","mime":"image\/tiff","width_unit":"px","height_unit":"px"}
tiff rational  false
tiff addr 0    false
tiff mm        {"0":7,"1":9,"2":8,"3":"width=\"7\" height=\"9\"","mime":"image\/tiff","width_unit":"px","height_unit":"px"}
## IFF: FORM chunks, even-padded, and a BMHD php refuses does not stop the walk
iff ilbm       {"0":4,"1":1,"2":14,"3":"width=\"4\" height=\"1\"","bits":4,"mime":"image\/iff","width_unit":"px","height_unit":"px"}
iff pbm        {"0":4,"1":1,"2":14,"3":"width=\"4\" height=\"1\"","bits":4,"mime":"image\/iff","width_unit":"px","height_unit":"px"}
iff other      false
iff bad bmhd   false
iff odd chunk  {"0":4,"1":1,"2":14,"3":"width=\"4\" height=\"1\"","bits":4,"mime":"image\/iff","width_unit":"px","height_unit":"px"}
## JPEG 2000: the raw codestream and the box wrapper over it
jpc            {"0":5,"1":6,"2":9,"3":"width=\"5\" height=\"6\"","bits":12,"channels":3,"mime":"application\/octet-stream","width_unit":"px","height_unit":"px"}
jpc no siz     false
jp2            {"0":5,"1":6,"2":10,"3":"width=\"5\" height=\"6\"","bits":12,"channels":3,"mime":"image\/jp2","width_unit":"px","height_unit":"px"}
jp2 empty      false
## WBMP and XBM have no signature, so they are the ladder's last two tries
wbmp           {"0":75,"1":50,"2":15,"3":"width=\"75\" height=\"50\"","mime":"image\/vnd.wap.wbmp","width_unit":"px","height_unit":"px"}
wbmp multibyte {"0":2048,"1":50,"2":15,"3":"width=\"2048\" height=\"50\"","mime":"image\/vnd.wap.wbmp","width_unit":"px","height_unit":"px"}
wbmp too wide  false
wbmp zero      false
xbm            {"0":75,"1":50,"2":16,"3":"width=\"75\" height=\"50\"","mime":"image\/xbm","width_unit":"px","height_unit":"px"}
xbm reversed   {"0":75,"1":50,"2":16,"3":"width=\"75\" height=\"50\"","mime":"image\/xbm","width_unit":"px","height_unit":"px"}
xbm indented   false
xbm no under   false
xbm negative   {"0":4294967293,"1":50,"2":16,"3":"width=\"-3\" height=\"50\"","mime":"image\/xbm","width_unit":"px","height_unit":"px"}
xbm no newline {"0":3,"1":50,"2":16,"3":"width=\"3\" height=\"50\"","mime":"image\/xbm","width_unit":"px","height_unit":"px"}
xbm glued      false
xbm nul        {"0":3,"1":50,"2":16,"3":"width=\"3\" height=\"50\"","mime":"image\/xbm","width_unit":"px","height_unit":"px"}
xbm wraps      {"0":1,"1":50,"2":16,"3":"width=\"1\" height=\"50\"","mime":"image\/xbm","width_unit":"px","height_unit":"px"}
xbm saturates  {"0":4294967295,"1":50,"2":16,"3":"width=\"-1\" height=\"50\"","mime":"image\/xbm","width_unit":"px","height_unit":"px"}
