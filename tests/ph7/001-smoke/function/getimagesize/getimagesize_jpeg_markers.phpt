--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The JPEG marker walk, and the APP payloads $image_info collects
--FILE--
<?php
/* JPEG is the only container getimagesize() reads by WALKING, and the only one
 * with a second answer. What the walk pins here: the first frame header wins
 * and every later one is skipped like any other segment; C4/C8/CC sit inside
 * the frame-header run and are NOT frame headers; a marker byte may be
 * preceded by a run of 0xFF padding, and anything that is not 0xFF before it
 * is COUNTED and reported; and the APP payloads are collected only when the
 * caller named $image_info, keeping the FIRST of each APPn number. A field the
 * file ran out of before is not zero -- php stores what its getc answered, and
 * that is -1 widened to an unsigned 32-bit reading. */
function gis_jpeg(string $label, string $bytes, bool $wantInfo = true): void {
    $msgs = [];
    set_error_handler(function ($no, $msg) use (&$msgs) { $msgs[] = "$no: $msg"; return true; });
    $info = null;
    $r = $wantInfo ? getimagesizefromstring($bytes, $info) : getimagesizefromstring($bytes);
    restore_error_handler();
    printf("%-16s %s\n", $label, $r === false ? 'false' : json_encode($r));
    if ($wantInfo) {
        printf("%-16s   info=%s\n", '', json_encode(array_map('bin2hex', $info)));
    }
    foreach ($msgs as $m) { printf("%-16s   %s\n", '', $m); }
}
function getimagesize_jpeg_markers(): void {
    $sof = fn(int $marker, int $len, int $bits, int $h, int $w, int $ch) =>
        "\xff" . chr($marker) . pack('n', $len) . chr($bits) . pack('n', $h) . pack('n', $w) . chr($ch);
    $app = fn(int $marker, string $payload) =>
        "\xff" . chr($marker) . pack('n', strlen($payload) + 2) . $payload;
    $tail = str_repeat("\x00", 16);

    echo "## one frame header is the whole answer\n";
    gis_jpeg('sof0', "\xff\xd8" . $sof(0xC0, 17, 8, 10, 20, 3) . $tail);
    gis_jpeg('sof2 progressive', "\xff\xd8" . $sof(0xC2, 17, 8, 10, 20, 3) . $tail);

    echo "## the second frame header is skipped, not read\n";
    gis_jpeg('two frames', "\xff\xd8" . $sof(0xC0, 17, 8, 10, 20, 3) . str_repeat("\x00", 9)
        . $sof(0xC1, 17, 8, 99, 99, 1) . str_repeat("\x00", 9) . "\xff\xd9");

    echo "## C4, C8 and CC are inside the run and are not frame headers\n";
    foreach ([0xC4, 0xC8, 0xCC] as $m) {
        gis_jpeg(sprintf('marker %02x', $m), "\xff\xd8" . "\xff" . chr($m) . pack('n', 4) . "ab"
            . $sof(0xC0, 17, 8, 10, 20, 3) . $tail);
    }

    echo "## padding before a marker: a run of 0xFF is silent, anything else is counted\n";
    gis_jpeg('ff run', "\xff\xd8\xff\xff" . substr($sof(0xC0, 17, 8, 10, 20, 3), 1) . $tail);
    gis_jpeg('extraneous', "\xff\xd8" . $app(0xE0, 'ab') . 'zzz'
        . $sof(0xC0, 17, 8, 10, 20, 3) . $tail);

    echo "## APP payloads: only with \$image_info, only the first of each number\n";
    gis_jpeg('one app', "\xff\xd8" . $app(0xE0, 'ab') . $sof(0xC0, 17, 8, 10, 20, 3) . $tail);
    gis_jpeg('duplicate app', "\xff\xd8" . $app(0xE0, 'ab') . $app(0xE0, 'cd')
        . $sof(0xC0, 17, 8, 10, 20, 3) . $tail);
    gis_jpeg('empty app', "\xff\xd8" . $app(0xE5, '') . $sof(0xC0, 17, 8, 10, 20, 3) . $tail);
    gis_jpeg('many apps', "\xff\xd8" . $app(0xE1, 'Exif') . $app(0xED, 'Photoshop')
        . $app(0xEF, 'last') . $sof(0xC0, 17, 8, 10, 20, 3) . $tail);
    gis_jpeg('no info arg', "\xff\xd8" . $app(0xE0, 'ab') . $sof(0xC0, 17, 8, 10, 20, 3) . $tail, false);
    gis_jpeg('truncated app', "\xff\xd8" . "\xff\xe0" . pack('n', 64) . 'ab');

    echo "## SOS and EOI both stop the walk where they stand\n";
    gis_jpeg('eoi first', "\xff\xd8\xff\xd9");
    gis_jpeg('sos first', "\xff\xd8\xff\xda" . pack('n', 12) . str_repeat("\x00", 10));
    gis_jpeg('sof then sos', "\xff\xd8" . $sof(0xC0, 17, 8, 10, 20, 3) . str_repeat("\x00", 9)
        . "\xff\xda" . pack('n', 12) . str_repeat("\x00", 10));

    echo "## a frame header the file ran out of: getc's -1, widened unsigned\n";
    gis_jpeg('no length', "\xff\xd8\xff\xc0");
    gis_jpeg('no bits', "\xff\xd8\xff\xc0" . pack('n', 17));
    gis_jpeg('no height', "\xff\xd8\xff\xc0" . pack('n', 17) . "\x08");
    gis_jpeg('no width', "\xff\xd8\xff\xc0" . pack('n', 17) . "\x08" . pack('n', 10));
    gis_jpeg('no channels', "\xff\xd8\xff\xc0" . pack('n', 17) . "\x08" . pack('n', 10) . pack('n', 20));

    echo "## a segment length below two cannot cover itself and ends the walk\n";
    gis_jpeg('short length', "\xff\xd8" . "\xff\xe0\x00\x01" . $sof(0xC0, 17, 8, 10, 20, 3) . $tail);
    gis_jpeg('frame len 7', "\xff\xd8" . "\xff\xc0" . pack('n', 7) . "\x08"
        . pack('n', 10) . pack('n', 20) . "\x03");
}
getimagesize_jpeg_markers();
--EXPECT--
## one frame header is the whole answer
sof0             {"0":20,"1":10,"2":2,"3":"width=\"20\" height=\"10\"","bits":8,"channels":3,"mime":"image\/jpeg","width_unit":"px","height_unit":"px"}
                   info=[]
sof2 progressive {"0":20,"1":10,"2":2,"3":"width=\"20\" height=\"10\"","bits":8,"channels":3,"mime":"image\/jpeg","width_unit":"px","height_unit":"px"}
                   info=[]
## the second frame header is skipped, not read
two frames       {"0":20,"1":10,"2":2,"3":"width=\"20\" height=\"10\"","bits":8,"channels":3,"mime":"image\/jpeg","width_unit":"px","height_unit":"px"}
                   info=[]
## C4, C8 and CC are inside the run and are not frame headers
marker c4        {"0":20,"1":10,"2":2,"3":"width=\"20\" height=\"10\"","bits":8,"channels":3,"mime":"image\/jpeg","width_unit":"px","height_unit":"px"}
                   info=[]
marker c8        {"0":20,"1":10,"2":2,"3":"width=\"20\" height=\"10\"","bits":8,"channels":3,"mime":"image\/jpeg","width_unit":"px","height_unit":"px"}
                   info=[]
marker cc        {"0":20,"1":10,"2":2,"3":"width=\"20\" height=\"10\"","bits":8,"channels":3,"mime":"image\/jpeg","width_unit":"px","height_unit":"px"}
                   info=[]
## padding before a marker: a run of 0xFF is silent, anything else is counted
ff run           {"0":20,"1":10,"2":2,"3":"width=\"20\" height=\"10\"","bits":8,"channels":3,"mime":"image\/jpeg","width_unit":"px","height_unit":"px"}
                   info=[]
extraneous       {"0":20,"1":10,"2":2,"3":"width=\"20\" height=\"10\"","bits":8,"channels":3,"mime":"image\/jpeg","width_unit":"px","height_unit":"px"}
                   info={"APP0":"6162"}
                   2: getimagesizefromstring(): Corrupt JPEG data: 3 extraneous bytes before marker
## APP payloads: only with $image_info, only the first of each number
one app          {"0":20,"1":10,"2":2,"3":"width=\"20\" height=\"10\"","bits":8,"channels":3,"mime":"image\/jpeg","width_unit":"px","height_unit":"px"}
                   info={"APP0":"6162"}
duplicate app    {"0":20,"1":10,"2":2,"3":"width=\"20\" height=\"10\"","bits":8,"channels":3,"mime":"image\/jpeg","width_unit":"px","height_unit":"px"}
                   info={"APP0":"6162"}
empty app        {"0":20,"1":10,"2":2,"3":"width=\"20\" height=\"10\"","bits":8,"channels":3,"mime":"image\/jpeg","width_unit":"px","height_unit":"px"}
                   info={"APP5":""}
many apps        {"0":20,"1":10,"2":2,"3":"width=\"20\" height=\"10\"","bits":8,"channels":3,"mime":"image\/jpeg","width_unit":"px","height_unit":"px"}
                   info={"APP1":"45786966","APP13":"50686f746f73686f70","APP15":"6c617374"}
no info arg      {"0":20,"1":10,"2":2,"3":"width=\"20\" height=\"10\"","bits":8,"channels":3,"mime":"image\/jpeg","width_unit":"px","height_unit":"px"}
truncated app    false
                   info=[]
## SOS and EOI both stop the walk where they stand
eoi first        false
                   info=[]
sos first        false
                   info=[]
sof then sos     {"0":20,"1":10,"2":2,"3":"width=\"20\" height=\"10\"","bits":8,"channels":3,"mime":"image\/jpeg","width_unit":"px","height_unit":"px"}
                   info=[]
## a frame header the file ran out of: getc's -1, widened unsigned
no length        {"0":0,"1":0,"2":2,"3":"width=\"0\" height=\"0\"","bits":4294967295,"channels":4294967295,"mime":"image\/jpeg","width_unit":"px","height_unit":"px"}
                   info=[]
no bits          {"0":0,"1":0,"2":2,"3":"width=\"0\" height=\"0\"","bits":4294967295,"channels":4294967295,"mime":"image\/jpeg","width_unit":"px","height_unit":"px"}
                   info=[]
no height        {"0":0,"1":0,"2":2,"3":"width=\"0\" height=\"0\"","bits":8,"channels":4294967295,"mime":"image\/jpeg","width_unit":"px","height_unit":"px"}
                   info=[]
no width         {"0":0,"1":10,"2":2,"3":"width=\"0\" height=\"10\"","bits":8,"channels":4294967295,"mime":"image\/jpeg","width_unit":"px","height_unit":"px"}
                   info=[]
no channels      {"0":20,"1":10,"2":2,"3":"width=\"20\" height=\"10\"","bits":8,"channels":4294967295,"mime":"image\/jpeg","width_unit":"px","height_unit":"px"}
                   info=[]
## a segment length below two cannot cover itself and ends the walk
short length     false
                   info=[]
frame len 7      {"0":20,"1":10,"2":2,"3":"width=\"20\" height=\"10\"","bits":8,"channels":3,"mime":"image\/jpeg","width_unit":"px","height_unit":"px"}
                   info=[]
