--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
AVIF and HEIF: the ISO base media walk that finds the primary item
--FILE--
<?php
/* Neither format states a size in a header. Both are box files whose pictures
 * are ITEMS, and the size of the one that matters lives in a PROPERTY that a
 * separate table associates with it -- so the answer takes `pitm` (which item),
 * `ipco` (the numbered property list), `ipma` (which properties belong to
 * which item) and, for a tiled picture, `iref` (which items it is made of).
 * The cases below pin the parts of that walk a program can see. */
function gis_box(string $type, string $payload): string {
    return pack('N', strlen($payload) + 8) . $type . $payload;
}
function gis_full(string $type, int $ver, int $flags, string $payload): string {
    return pack('N', strlen($payload) + 12) . $type . chr($ver) . substr(pack('N', $flags), 1) . $payload;
}
function gis_avif(array $o = []): string {
    $ipco = $o['ipco'] ?? (gis_full('ispe', 0, 0, pack('N', $o['w'] ?? 64) . pack('N', $o['h'] ?? 48))
        . gis_full('pixi', 0, 0, chr($o['nc'] ?? 3) . str_repeat(chr($o['bd'] ?? 8), $o['nc'] ?? 3)));
    $ipma = $o['ipma'] ?? gis_full('ipma', 0, 0, pack('N', 1) . pack('n', $o['item'] ?? 1)
        . chr(count($o['assoc'] ?? [1, 2]))
        . implode('', array_map('chr', $o['assoc'] ?? [1, 2])));
    $iinf = $o['iinf'] ?? gis_full('iinf', 0, 0, pack('n', 1)
        . gis_full('infe', 2, 0, pack('n', 1) . pack('n', 0) . 'av01' . "\0"));
    $parts = [gis_full('pitm', 0, 0, pack('n', $o['pitm'] ?? 1)), $iinf];
    if (isset($o['iref'])) { $parts[] = $o['iref']; }
    $parts[] = gis_box('iprp', gis_box('ipco', $ipco) . $ipma);
    if (($o['order'] ?? '') === 'iinf-last') {
        $parts = [gis_full('pitm', 0, 0, pack('n', $o['pitm'] ?? 1)),
                  gis_box('iprp', gis_box('ipco', $ipco) . $ipma), $iinf];
    }
    return gis_box('ftyp', $o['ftyp'] ?? (($o['brand'] ?? 'avif') . pack('N', 0) . 'mif1'))
        . gis_full('meta', 0, 0, implode('', $parts))
        . gis_box('mdat', str_repeat("\x00", 8));
}
function gis_avif_show(string $label, string $bytes): void {
    $r = @getimagesizefromstring($bytes);
    printf("%-20s %s\n", $label, $r === false ? 'false'
        : sprintf('%dx%d type=%d bits=%s channels=%s', $r[0], $r[1], $r[2],
            $r['bits'] ?? '-', $r['channels'] ?? '-'));
}
function getimagesize_avif_heif(): void {
    echo "## the brands, and why AVIF is asked before HEIF\n";
    foreach (['avif', 'avis', 'mif1', 'heic', 'heix', 'isom', 'msf1'] as $brand) {
        gis_avif_show("brand $brand", gis_avif(['brand' => $brand]));
    }
    /* An AVIF also carries `mif1`, so asking the HEIF question first would
     * answer HEIF for every AVIF there is. */
    gis_avif_show('avif then mif1', gis_avif(['ftyp' => 'avif' . pack('N', 0) . 'mif1']));
    gis_avif_show('mif1 then avif', gis_avif(['ftyp' => 'mif1' . pack('N', 0) . 'avif']));
    /* The minor VERSION sits in the same four-byte grid and is skipped, so an
     * `avif` standing exactly there is not a brand. */
    gis_avif_show('avif as version', gis_avif(['ftyp' => 'isom' . 'avif' . 'mif1']));
    gis_avif_show('no compatible', gis_avif(['ftyp' => 'isom' . pack('N', 0)]));

    echo "## the extent property, and the two zeros it refuses\n";
    gis_avif_show('1x1', gis_avif(['w' => 1, 'h' => 1]));
    gis_avif_show('4000x3000', gis_avif(['w' => 4000, 'h' => 3000]));
    gis_avif_show('zero width', gis_avif(['w' => 0]));
    gis_avif_show('zero height', gis_avif(['h' => 0]));

    echo "## the depth and channel count, from pixi or from av1C\n";
    foreach ([[1, 8], [3, 8], [3, 10], [4, 12], [2, 1], [3, 255]] as [$nc, $bd]) {
        gis_avif_show("pixi $nc x $bd", gis_avif(['nc' => $nc, 'bd' => $bd]));
    }
    gis_avif_show('pixi zero chan', gis_avif(['ipco' =>
        gis_full('ispe', 0, 0, pack('N', 8) . pack('N', 8)) . gis_full('pixi', 0, 0, chr(0))]));
    gis_avif_show('pixi mismatch', gis_avif(['ipco' =>
        gis_full('ispe', 0, 0, pack('N', 8) . pack('N', 8))
        . gis_full('pixi', 0, 0, chr(3) . chr(8) . chr(8) . chr(10))]));
    /* av1C states depth in two bits and monochrome in a third. */
    foreach ([0x00, 0x40, 0x60, 0x10, 0x50, 0x20] as $flags) {
        gis_avif_show(sprintf('av1C %02x', $flags), gis_avif(['ipco' =>
            gis_full('ispe', 0, 0, pack('N', 8) . pack('N', 8))
            . gis_box('av1C', chr(0x81) . chr(0x0c) . chr($flags) . chr(0))]));
    }

    echo "## an alpha plane is not a channel of the item: it ADDS one\n";
    $alpha = "urn:mpeg:mpegB:cicp:systems:auxiliary:alpha\0";
    $gain  = "urn:com:photo:aux:hdrgainmap\0";
    gis_avif_show('with alpha', gis_avif([
        'ipco' => gis_full('ispe', 0, 0, pack('N', 8) . pack('N', 8))
            . gis_full('pixi', 0, 0, chr(3) . chr(8) . chr(8) . chr(8))
            . gis_full('auxC', 0, 0, $alpha),
        'assoc' => [1, 2, 3]]));
    gis_avif_show('with gain map', gis_avif([
        'ipco' => gis_full('ispe', 0, 0, pack('N', 8) . pack('N', 8))
            . gis_full('pixi', 0, 0, chr(3) . chr(8) . chr(8) . chr(8))
            . gis_full('auxC', 0, 0, $gain),
        'assoc' => [1, 2, 3]]));
    gis_avif_show('unknown auxC', gis_avif([
        'ipco' => gis_full('ispe', 0, 0, pack('N', 8) . pack('N', 8))
            . gis_full('pixi', 0, 0, chr(3) . chr(8) . chr(8) . chr(8))
            . gis_full('auxC', 0, 0, "urn:something:else:entirely\0\0"),
        'assoc' => [1, 2, 3]]));

    echo "## the association table addresses properties by POSITION, from one\n";
    gis_avif_show('assoc 1,2', gis_avif(['assoc' => [1, 2]]));
    gis_avif_show('assoc 2,1', gis_avif(['assoc' => [2, 1]]));
    gis_avif_show('assoc 1 only', gis_avif(['assoc' => [1]]));
    gis_avif_show('assoc 0', gis_avif(['assoc' => [0, 2]]));
    gis_avif_show('assoc 9', gis_avif(['assoc' => [9, 2]]));
    /* The top bit of an index marks the property ESSENTIAL and is masked off. */
    gis_avif_show('essential bit', gis_avif(['assoc' => [1 | 0x80, 2 | 0x80]]));
    gis_avif_show('other item', gis_avif(['item' => 7]));
    gis_avif_show('pitm elsewhere', gis_avif(['pitm' => 7]));
    /* ipma version 1 spells item ids in four bytes; flag 1 spells indices in two. */
    gis_avif_show('ipma v1', gis_avif(['ipma' => gis_full('ipma', 1, 0,
        pack('N', 1) . pack('N', 1) . chr(2) . chr(1) . chr(2))]));
    gis_avif_show('ipma flag 1', gis_avif(['ipma' => gis_full('ipma', 0, 1,
        pack('N', 1) . pack('n', 1) . chr(2) . pack('n', 1) . pack('n', 2 | 0x8000))]));
    gis_avif_show('ipma v9', gis_avif(['ipma' => gis_full('ipma', 9, 0,
        pack('N', 1) . pack('n', 1) . chr(2) . chr(1) . chr(2))]));

    echo "## a tiled picture takes its depth from the tiles iref names\n";
    $tiled = gis_avif([
        'ipco' => gis_full('ispe', 0, 0, pack('N', 64) . pack('N', 48))
            . gis_full('pixi', 0, 0, chr(3) . chr(10) . chr(10) . chr(10)),
        'ipma' => gis_full('ipma', 0, 0, pack('N', 2)
            . pack('n', 1) . chr(1) . chr(1)
            . pack('n', 2) . chr(1) . chr(2)),
        'iref' => gis_full('iref', 0, 0, gis_box('dimg', pack('n', 1) . pack('n', 1) . pack('n', 2))),
    ]);
    gis_avif_show('tiled', $tiled);
    gis_avif_show('tiled no iref', gis_avif([
        'ipco' => gis_full('ispe', 0, 0, pack('N', 64) . pack('N', 48))
            . gis_full('pixi', 0, 0, chr(3) . chr(10) . chr(10) . chr(10)),
        'ipma' => gis_full('ipma', 0, 0, pack('N', 2)
            . pack('n', 1) . chr(1) . chr(1)
            . pack('n', 2) . chr(1) . chr(2))]));

    echo "## the gain-map search must be finished before an answer is possible\n";
    gis_avif_show('iinf last', gis_avif(['order' => 'iinf-last']));
    gis_avif_show('no iinf', gis_avif(['iinf' => '']));
    gis_avif_show('tmap no iref', gis_avif(['iinf' => gis_full('iinf', 0, 0, pack('n', 1)
        . gis_full('infe', 2, 0, pack('n', 2) . pack('n', 0) . 'tmap' . "\0"))]));

    echo "## a truncated walk is no answer at all\n";
    $whole = gis_avif();
    foreach ([0, 8, 16, 32, 40, 60, 80] as $cut) {
        gis_avif_show("first $cut bytes", substr($whole, 0, $cut));
    }
}
getimagesize_avif_heif();
--EXPECT--
## the brands, and why AVIF is asked before HEIF
brand avif           64x48 type=19 bits=8 channels=3
brand avis           64x48 type=19 bits=8 channels=3
brand mif1           64x48 type=20 bits=8 channels=3
brand heic           64x48 type=20 bits=8 channels=3
brand heix           64x48 type=20 bits=8 channels=3
brand isom           false
brand msf1           false
avif then mif1       64x48 type=19 bits=8 channels=3
mif1 then avif       64x48 type=19 bits=8 channels=3
avif as version      false
no compatible        false
## the extent property, and the two zeros it refuses
1x1                  1x1 type=19 bits=8 channels=3
4000x3000            4000x3000 type=19 bits=8 channels=3
zero width           false
zero height          false
## the depth and channel count, from pixi or from av1C
pixi 1 x 8           64x48 type=19 bits=8 channels=1
pixi 3 x 8           64x48 type=19 bits=8 channels=3
pixi 3 x 10          64x48 type=19 bits=10 channels=3
pixi 4 x 12          64x48 type=19 bits=12 channels=4
pixi 2 x 1           64x48 type=19 bits=1 channels=2
pixi 3 x 255         64x48 type=19 bits=255 channels=3
pixi zero chan       false
pixi mismatch        false
av1C 00              8x8 type=19 bits=8 channels=3
av1C 40              8x8 type=19 bits=10 channels=3
av1C 60              8x8 type=19 bits=12 channels=3
av1C 10              8x8 type=19 bits=8 channels=1
av1C 50              8x8 type=19 bits=10 channels=1
av1C 20              false
## an alpha plane is not a channel of the item: it ADDS one
with alpha           8x8 type=19 bits=8 channels=4
with gain map        8x8 type=19 bits=8 channels=3
unknown auxC         8x8 type=19 bits=8 channels=3
## the association table addresses properties by POSITION, from one
assoc 1,2            64x48 type=19 bits=8 channels=3
assoc 2,1            64x48 type=19 bits=8 channels=3
assoc 1 only         false
assoc 0              false
assoc 9              false
essential bit        64x48 type=19 bits=8 channels=3
other item           false
pitm elsewhere       false
ipma v1              64x48 type=19 bits=8 channels=3
ipma flag 1          64x48 type=19 bits=8 channels=3
ipma v9              false
## a tiled picture takes its depth from the tiles iref names
tiled                64x48 type=19 bits=10 channels=3
tiled no iref        false
## the gain-map search must be finished before an answer is possible
iinf last            false
no iinf              false
tmap no iref         false
## a truncated walk is no answer at all
first 0 bytes        false
first 8 bytes        false
first 16 bytes       false
first 32 bytes       false
first 40 bytes       false
first 60 bytes       false
first 80 bytes       false
