--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The detection ladder's widenings, and what it says when a file is too short
--FILE--
<?php
/* php widens its look at the head THREE bytes at a time -- three, then four,
 * then twelve -- and tests after each widening, which is why two signatures
 * are matched on fewer bytes than they have (PSD on three of four, BMP on
 * two). The tail is ordered by cost: the two formats with no signature at all
 * are tried only once everything else has failed, and the "Error reading from"
 * notice for a file shorter than twelve bytes is raised BETWEEN them -- so a
 * four-byte WBMP is a size while a four-byte anything-else is a diagnostic.
 * That notice prints the ARGUMENT the caller wrote, which for the string form
 * is the DATA, as a C string and so stopping at its first NUL. */
function gis_ladder(string $label, string $bytes): void {
    $msgs = [];
    set_error_handler(function ($no, $msg) use (&$msgs) { $msgs[] = "$no: $msg"; return true; });
    $r = getimagesizefromstring($bytes);
    restore_error_handler();
    /* The notice quotes the DATA, so escape it: the bytes are the point. */
    $msgs = array_map(fn($m) => addcslashes($m, "\0..\37\177..\377"), $msgs);
    printf("%-18s %-8s %s\n", $label, $r === false ? 'false' : $r[2], implode(' | ', $msgs));
}
function getimagesize_ladder(): void {
    echo "## the three-byte tests, two of them shorter than the signature they name\n";
    gis_ladder('gif', "GIF" . str_repeat("\x00", 20));
    gis_ladder('psd on 3 bytes', "8BP" . str_repeat("\x00", 20));
    gis_ladder('bmp on 2 bytes', "BM" . str_repeat("\x00", 30));
    gis_ladder('swf', "FWS" . str_repeat("\x00", 40));
    gis_ladder('jpc', "\xff\x4f\xff" . str_repeat("\x00", 40));

    echo "## PNG is the one signature checked twice, and a failed re-check is loud\n";
    gis_ladder('png', "\x89PNG\r\n\x1a\n" . str_repeat("\x00", 20));
    gis_ladder('png mangled', "\x89PNG\n\n\x1a\n" . str_repeat("\x00", 20));
    gis_ladder('png short', "\x89PN");

    echo "## RIFF must say WEBP at byte eight or it is nothing at all\n";
    gis_ladder('riff webp', "RIFF" . pack('V', 0) . "WEBP" . "VP8 " . str_repeat("\x00", 14));
    gis_ladder('riff wave', "RIFF" . pack('V', 0) . "WAVE" . str_repeat("\x00", 14));
    gis_ladder('riff short', "RIFF" . pack('V', 0));

    echo "## the four-byte tests\n";
    gis_ladder('tiff ii', "II\x2a\x00" . str_repeat("\x00", 40));
    gis_ladder('tiff mm', "MM\x00\x2a" . str_repeat("\x00", 40));
    gis_ladder('iff', "FORM" . str_repeat("\x00", 40));
    gis_ladder('ico', "\x00\x00\x01\x00" . str_repeat("\x00", 40));
    gis_ladder('three bytes', "abc");

    echo "## the twelve-byte test, and the two signature-less formats behind it\n";
    gis_ladder('jp2', "\x00\x00\x00\x0cjP  \r\n\x87\n" . str_repeat("\x00", 20));
    gis_ladder('jp2 eleven', "\x00\x00\x00\x0cjP  \r\n\x87");
    gis_ladder('wbmp short', "\x00\x00\x4b\x32");
    gis_ladder('xbm needs 12', "#define x_width 3\n#define x_height 4\n");
    gis_ladder('xbm too short', "#define w 3\n");
    gis_ladder('nothing', "0123456789ab");

    echo "## the notice prints the ARGUMENT, as a C string\n";
    gis_ladder('two bytes', "ab");
    gis_ladder('empty', "");
    gis_ladder('leading nul', "\x00\x00\x04");
    gis_ladder('nul inside', "ab\x00cd");
}
getimagesize_ladder();
--EXPECT--
## the three-byte tests, two of them shorter than the signature they name
gif                1        
psd on 3 bytes     5        
bmp on 2 bytes     false    
swf                4        
jpc                false    2: getimagesizefromstring(): JPEG2000 codestream corrupt(Expected SIZ marker not found after SOC)
## PNG is the one signature checked twice, and a failed re-check is loud
png                3        
png mangled        false    2: getimagesizefromstring(): PNG file corrupted by ASCII conversion
png short          false    8: getimagesizefromstring(): Error reading from \211PN!
## RIFF must say WEBP at byte eight or it is nothing at all
riff webp          18       
riff wave          false    
riff short         false    8: getimagesizefromstring(): Error reading from RIFF!
## the four-byte tests
tiff ii            false    
tiff mm            false    
iff                false    
ico                false    
three bytes        false    8: getimagesizefromstring(): Error reading from abc!
## the twelve-byte test, and the two signature-less formats behind it
jp2                false    2: getimagesizefromstring(): JP2 file has no codestreams at root level
jp2 eleven         false    8: getimagesizefromstring(): Error reading from !
wbmp short         15       
xbm needs 12       16       
xbm too short      false    
nothing            false    
## the notice prints the ARGUMENT, as a C string
two bytes          false    8: getimagesizefromstring(): Error reading from ab!
empty              false    8: getimagesizefromstring(): Error reading from !
leading nul        false    8: getimagesizefromstring(): Error reading from !
nul inside         false    8: getimagesizefromstring(): Error reading from ab!
