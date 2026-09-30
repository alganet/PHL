--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ext/fileinfo: the text analysis, and what each signature answers
--SKIPIF--
<?php
/* a capability guard: CI's Windows php is built without ext/fileinfo */
if (!extension_loaded('fileinfo')) {
    die("skip this php has no ext/fileinfo\n");
}
--FILE--
<?php
/* The ANSWER half. Two rules decide what is pinned here:
 *
 *   - the TEXT analysis is derived whole -- the encoding ladder, the charset
 *     name each rung answers with, and the four annotations -- so every one of
 *     its answers is pinned exactly;
 *   - a SIGNATURE's mime type and encoding are pinned exactly, and its
 *     description only where the whole sentence is the header (PNG, GIF, gzip,
 *     zip, PDF). The formats whose description ends in a deep parse of the
 *     container -- an Exif JPEG, TIFF's directory, an MP3 with no tag -- are
 *     asked for their TYPE here and nothing more. */
$none = new finfo(FILEINFO_NONE);
$mime = new finfo(FILEINFO_MIME_TYPE);
$enc  = new finfo(FILEINFO_MIME_ENCODING);
$both = new finfo(FILEINFO_MIME);

function t(string $label, string $bytes): void
{
    global $none, $mime, $enc;
    printf("%-30s %-58s %-26s %s\n", $label,
        var_export($none->buffer($bytes), true), $mime->buffer($bytes), $enc->buffer($bytes));
}

echo "-- the encoding ladder\n";
t('ascii',            "abc\ndef\n");
t('utf-8',            "caf\xc3\xa9\n");
t('utf-8 with a BOM', "\xef\xbb\xbfhello\n");
t('utf-16 le',        "\xff\xfeh\x00i\x00");
t('utf-16 be',        "\xfe\xff\x00h\x00i");
t('utf-32 le',        "\xff\xfe\x00\x00h\x00\x00\x00");
t('iso-8859',         "caf\xe9 na\xefve\n");
t('non-ISO extended', "\x80\x81abc\n");
t('a NUL is binary',  "abc\x00def\n");
t('and so is a DEL',  "a\x7fb\n");
t('empty',            '');
t('one byte',         'a');
t('two bytes',        'ab');

echo "-- the four annotations\n";
t('CRLF',             "abc\r\ndef\r\n");
t('CR alone',         "abc\rdef\r");
t('LF alone says nothing', "abc\ndef\n");
t('mixed',            "a\r\nb\nc\n");
t('CR and LF',        "a\nb\rc");
t('none at all',      'abc');
t('a NEL',            "a\xc2\x85b");
t('300 is not long',  str_repeat('a', 300) . "\n");
t('301 is',           str_repeat('a', 301) . "\n");
t('the LONGEST line', str_repeat('a', 400) . "\n" . str_repeat('b', 600) . "\n");
t('an escape',        "a\x1b[0mc\n");
t('overstriking',     "a\x08b\n");
t('all of them',      str_repeat('a', 400) . "\x1b\x08\r\n");

echo "-- the text formats\n";
t('php',              "<?php echo 1;\n");
t('php, short',       "<?\n");
t('php with a BOM',   "\xef\xbb\xbf<?php\n");
t('xml',              "<?xml version=\"1.0\"?>\n<a/>\n");
t('xml 1.1',          "<?xml version=\"1.1\"?><a/>");
t('xml with no version', "<?xml encoding=\"UTF-8\"?><a/>");
t('html',             "<html><head><title>t</title></head><body>x</body></html>\n");
t('html by DOCTYPE',  "<!DOCTYPE html>\n<html></html>\n");
t('svg',              '<svg xmlns="http://www.w3.org/2000/svg"><rect/></svg>');
t('json',             "{\"a\":{\"b\":[1,2,{}]}}\n");
t('json, an array',   "[1,2,3]\n");
t('not json',         "{\"a\":}");
t('a bare string is not', "\"x\"\n");
t('csv',              "a,b,c\n1,2,3\n");
t('two columns are not', "a,b\n1,2\n3,4\n");
t('nor is a ragged one', "a,b,c\n1,2\n");
t('nor an unterminated one', "a,b,c\n1,2,3");
t('a unified diff',   "--- a\n+++ b\n@@ -1 +1 @@\n-x\n+y\n");
t('a context diff',   "*** a\n--- b\n");
t('a playlist',       "#EXTM3U\nx.mp3\n");
t('mail',             "From: a@b\nTo: c@d\n\nbody\n");
t('postscript',       "%!PS-Adobe-3.0\n%%Title: x\n");
t('rtf',              "{\\rtf1\\ansi test}");

echo "-- the #! line\n";
foreach (['/bin/sh', '/bin/bash', '/bin/zsh', '/bin/ksh', '/bin/csh', '/bin/tcsh',
          '/usr/bin/php', '/usr/bin/php8', '/usr/bin/python3', '/usr/bin/perl',
          '/usr/bin/ruby', '/usr/bin/node', '/usr/bin/awk', '/usr/bin/lua',
          '/usr/bin/tclsh', '/bin/sed', '/opt/bin/weird',
          '/usr/bin/env python3', '/usr/bin/env bash', '/usr/bin/env sh',
          '/usr/bin/env ksh', '/usr/bin/env php', '/usr/bin/env foobar'] as $interpreter) {
    t("#!$interpreter", "#!$interpreter\nbody\n");
}
t('#! with a space',  "#! /bin/sh\nx\n");

echo "-- the signatures, whole\n";
$png = "\x89PNG\r\n\x1a\n\x00\x00\x00\x0dIHDR" . pack('NN', 16, 32) . "\x08\x06\x00\x00\x01"
     . str_repeat("\x00", 8);
t('png',              $png);
t('png, a colormap',  "\x89PNG\r\n\x1a\n\x00\x00\x00\x0dIHDR" . pack('NN', 460, 460)
                      . "\x08\x03\x00\x00\x00" . str_repeat("\x00", 8));
t('gif',              "GIF89a" . pack('vv', 400, 250) . str_repeat("\x00", 16));
t('gif 87a',          "GIF87a" . pack('vv', 16, 32) . str_repeat("\x00", 16));
t('gzip',             "\x1f\x8b\x08\x00" . pack('V', 123456) . "\x02\x03" . str_repeat("\x00", 20));
t('gzip with a name', "\x1f\x8b\x08\x08" . pack('V', 0) . "\x00\x03name.txt\x00"
                      . str_repeat("\x00", 20));
t('an empty zip',     "PK\x05\x06" . str_repeat("\x00", 18));
t('bzip2',            "BZh91AY&SY" . str_repeat("\x00", 20));
t('xz',               "\xfd7zXZ\x00\x00\x04" . str_repeat("\x00", 20));
t('zstd',             "\x28\xb5\x2f\xfd" . str_repeat("\x00", 20));
t('7-zip',            "7z\xbc\xaf\x27\x1c\x00\x04" . str_repeat("\x00", 20));
t('rar',              "Rar!\x1a\x07\x00" . str_repeat("\x00", 20));
t('rar 5',            "Rar!\x1a\x07\x01\x00" . str_repeat("\x00", 20));
t('pdf',              "%PDF-1.4\n1 0 obj\n<</Type/Catalog/Pages 2 0 R>>\nendobj\n"
                      . "2 0 obj\n<</Type/Pages/Count 3>>\nendobj\n");
t('a java class',     "\xca\xfe\xba\xbe\x00\x00\x00\x34" . str_repeat("\x00", 20));
t('webassembly',      "\x00asm\x01\x00\x00\x00" . str_repeat("\x00", 20));
t('sqlite',           "SQLite format 3\x00\x10\x00\x01\x01\x00\x40\x20\x20"
                      . str_repeat("\x00", 80));
t('a webp',           "RIFF" . pack('V', 36) . "WEBPVP8 " . str_repeat("\x00", 20));
t('a wave',           "RIFF" . pack('V', 36) . "WAVEfmt " . pack('VvvVVvv', 16, 1, 2, 44100, 176400, 4, 16)
                      . "data\x00\x00\x00\x00");
t('an mp3 frame',     "\xff\xfb\x50\xc0" . str_repeat("\x00", 40));
t('an mp4',           "\x00\x00\x00\x18ftypmp42\x00\x00\x00\x00mp42isom");
t('an avif',          "\x00\x00\x00\x1cftypavif\x00\x00\x00\x00avifmif1miaf");

echo "-- the type alone, where the description carries a deeper parse\n";
foreach ([
    'a jpeg'   => "\xff\xd8\xff\xe0\x00\x10JFIF\x00\x01\x01\x00\x00\x01\x00\x01\x00\x00"
                  . "\xff\xc0\x00\x11\x08\x00\x64\x00\x64\x03\x01\x22\x00\x02\x11\x01\x03\x11\x01\xff\xd9",
    'a tiff'   => "II\x2a\x00\x08\x00\x00\x00\x0c\x00" . str_repeat("\x00", 30),
    'an icon'  => "\x00\x00\x01\x00\x01\x00\x10\x10\x00\x00\x01\x00\x20\x00\x04\x00\x00\x00\x16\x00\x00\x00"
                  . str_repeat("\x00", 4),
    'a bitmap' => "BM" . pack('V', 70) . "\x00\x00\x00\x00" . pack('V', 54) . pack('V', 40)
                  . pack('VVvv', 10, 10, 1, 24) . str_repeat("\x00", 40),
    'a truetype font' => "\x00\x01\x00\x00\x00\x0f\x00\x80\x00\x03\x00\x70OS/2" . str_repeat("\x00", 30),
    'an opentype font' => "OTTO\x00\x0a" . str_repeat("\x00", 30),
    'a woff'   => "wOFF\x00\x01\x00\x00" . pack('N', 7716) . str_repeat("\x00", 8)
                  . pack('nn', 1, 0) . str_repeat("\x00", 8),
    'flac'     => "fLaC\x00\x00\x00\x22" . str_repeat("\x00", 30),
    'ogg'      => "OggS\x00\x02" . str_repeat("\x00", 22) . "\x01vorbis" . str_repeat("\x00", 10),
    'matroska' => "\x1a\x45\xdf\xa3" . str_repeat("\x00", 8) . "webm" . str_repeat("\x00", 20),
    'an ar archive' => "!<arch>\n" . str_repeat(' ', 50),
] as $label => $bytes) {
    printf("%-22s %-30s %s\n", $label, $mime->buffer($bytes), $enc->buffer($bytes));
}

echo "-- the two mime faces together, and the extension one\n";
var_dump($both->buffer("hello\n"));
var_dump($both->buffer($png));
var_dump($both->buffer(''));
$extension = new finfo(FILEINFO_EXTENSION);
var_dump($extension->buffer($png), $extension->buffer("hello\n"),
         $extension->buffer("GIF89a" . pack('vv', 1, 1) . str_repeat("\x00", 16)));
--EXPECT--
-- the encoding ladder
ascii                          'ASCII text'                                               text/plain                 us-ascii
utf-8                          'Unicode text, UTF-8 text'                                 text/plain                 utf-8
utf-8 with a BOM               'Unicode text, UTF-8 (with BOM) text'                      text/plain                 utf-8
utf-16 le                      'Unicode text, UTF-16, little-endian text, with no line terminators' text/plain                 utf-16le
utf-16 be                      'Unicode text, UTF-16, big-endian text, with no line terminators' text/plain                 utf-16be
utf-32 le                      'Unicode text, UTF-32, little-endian'                      text/plain                 utf-32le
iso-8859                       'ISO-8859 text'                                            text/plain                 iso-8859-1
non-ISO extended               'Non-ISO extended-ASCII text'                              text/plain                 unknown-8bit
a NUL is binary                'data'                                                     application/octet-stream   binary
and so is a DEL                'data'                                                     application/octet-stream   binary
empty                          'empty'                                                    application/x-empty        binary
one byte                       'very short file (no magic)'                               application/octet-stream   binary
two bytes                      'ASCII text, with no line terminators'                     text/plain                 us-ascii
-- the four annotations
CRLF                           'ASCII text, with CRLF line terminators'                   text/plain                 us-ascii
CR alone                       'ASCII text, with CR line terminators'                     text/plain                 us-ascii
LF alone says nothing          'ASCII text'                                               text/plain                 us-ascii
mixed                          'ASCII text, with CRLF, LF line terminators'               text/plain                 us-ascii
CR and LF                      'ASCII text, with CR, LF line terminators'                 text/plain                 us-ascii
none at all                    'ASCII text, with no line terminators'                     text/plain                 us-ascii
a NEL                          'Unicode text, UTF-8 text, with NEL line terminators'      text/plain                 utf-8
300 is not long                'ASCII text'                                               text/plain                 us-ascii
301 is                         'ASCII text, with very long lines (301)'                   text/plain                 us-ascii
the LONGEST line               'ASCII text, with very long lines (600)'                   text/plain                 us-ascii
an escape                      'ASCII text, with escape sequences'                        text/plain                 us-ascii
overstriking                   'ASCII text, with overstriking'                            text/plain                 us-ascii
all of them                    'ASCII text, with very long lines (402), with CRLF line terminators, with escape sequences, with overstriking' text/plain                 us-ascii
-- the text formats
php                            'PHP script, ASCII text'                                   text/x-php                 us-ascii
php, short                     'PHP script, ASCII text'                                   text/x-php                 us-ascii
php with a BOM                 'PHP script, Unicode text, UTF-8 (with BOM) text'          text/x-php                 utf-8
xml                            'XML 1.0 document, ASCII text'                             text/xml                   us-ascii
xml 1.1                        'XML 1.1 document, ASCII text, with no line terminators'   text/xml                   us-ascii
xml with no version            'XML document, ASCII text, with no line terminators'       text/xml                   us-ascii
html                           'HTML document, ASCII text'                                text/html                  us-ascii
html by DOCTYPE                'HTML document, ASCII text'                                text/html                  us-ascii
svg                            'SVG Scalable Vector Graphics image, ASCII text, with no line terminators' image/svg+xml              us-ascii
json                           'JSON text data'                                           application/json           us-ascii
json, an array                 'JSON text data'                                           application/json           us-ascii
not json                       'ASCII text, with no line terminators'                     text/plain                 us-ascii
a bare string is not           'ASCII text'                                               text/plain                 us-ascii
csv                            'CSV ASCII text'                                           text/csv                   us-ascii
two columns are not            'ASCII text'                                               text/plain                 us-ascii
nor is a ragged one            'ASCII text'                                               text/plain                 us-ascii
nor an unterminated one        'ASCII text'                                               text/plain                 us-ascii
a unified diff                 'unified diff output, ASCII text'                          text/x-diff                us-ascii
a context diff                 'context diff output, ASCII text'                          text/x-diff                us-ascii
a playlist                     'M3U playlist, ASCII text'                                 audio/x-mpegurl            us-ascii
mail                           'news or mail, ASCII text'                                 message/rfc822             us-ascii
postscript                     'PostScript document text conforming DSC level 3.0'        application/postscript     us-ascii
rtf                            'Rich Text Format data, version 1, ANSI'                   text/rtf                   us-ascii
-- the #! line
#!/bin/sh                      'POSIX shell script, ASCII text executable'                text/x-shellscript         us-ascii
#!/bin/bash                    'Bourne-Again shell script, ASCII text executable'         text/x-shellscript         us-ascii
#!/bin/zsh                     'Paul Falstad\'s zsh script, ASCII text executable'        text/x-shellscript         us-ascii
#!/bin/ksh                     'Korn shell script, ASCII text executable'                 text/x-shellscript         us-ascii
#!/bin/csh                     'C shell script, ASCII text executable'                    text/x-shellscript         us-ascii
#!/bin/tcsh                    'Tenex C shell script, ASCII text executable'              text/x-shellscript         us-ascii
#!/usr/bin/php                 'PHP script, ASCII text executable'                        text/x-php                 us-ascii
#!/usr/bin/php8                'PHP script, ASCII text executable'                        text/x-php                 us-ascii
#!/usr/bin/python3             'Python script, ASCII text executable'                     text/x-script.python       us-ascii
#!/usr/bin/perl                'Perl script text executable'                              text/x-perl                us-ascii
#!/usr/bin/ruby                'Ruby script, ASCII text executable'                       text/x-ruby                us-ascii
#!/usr/bin/node                'Node.js script executable, ASCII text'                    application/javascript     us-ascii
#!/usr/bin/awk                 'awk script, ASCII text executable'                        text/x-awk                 us-ascii
#!/usr/bin/lua                 'Lua script, ASCII text executable'                        text/x-lua                 us-ascii
#!/usr/bin/tclsh               'Tcl/Tk script, ASCII text executable'                     text/x-tcl                 us-ascii
#!/bin/sed                     'a /bin/sed script, ASCII text executable'                 text/plain                 us-ascii
#!/opt/bin/weird               'a /opt/bin/weird script, ASCII text executable'           text/plain                 us-ascii
#!/usr/bin/env python3         'Python script, ASCII text executable'                     text/x-script.python       us-ascii
#!/usr/bin/env bash            'Bourne-Again shell script, ASCII text executable'         text/x-shellscript         us-ascii
#!/usr/bin/env sh              'a sh script, ASCII text executable'                       text/plain                 us-ascii
#!/usr/bin/env ksh             'a ksh script, ASCII text executable'                      text/plain                 us-ascii
#!/usr/bin/env php             'a php script, ASCII text executable'                      text/plain                 us-ascii
#!/usr/bin/env foobar          'a foobar script, ASCII text executable'                   text/plain                 us-ascii
#! with a space                'POSIX shell script, ASCII text executable'                text/x-shellscript         us-ascii
-- the signatures, whole
png                            'PNG image data, 16 x 32, 8-bit/color RGBA, interlaced'    image/png                  binary
png, a colormap                'PNG image data, 460 x 460, 8-bit colormap, non-interlaced' image/png                  binary
gif                            'GIF image data, version 89a, 400 x 250'                   image/gif                  binary
gif 87a                        'GIF image data, version 87a, 16 x 32'                     image/gif                  binary
gzip                           'gzip compressed data, last modified: Fri Jan  2 10:17:36 1970, max compression, from Unix' application/gzip           binary
gzip with a name               'gzip compressed data, was "name.txt", from Unix'          application/gzip           binary
an empty zip                   'Zip archive data (empty)'                                 application/zip            binary
bzip2                          'bzip2 compressed data, block size = 900k'                 application/x-bzip2        binary
xz                             'XZ compressed data, checksum CRC64'                       application/x-xz           binary
zstd                           'Zstandard compressed data (v0.8+), Dictionary ID: None'   application/zstd           binary
7-zip                          '7-zip archive data, version 0.4'                          application/x-7z-compressed binary
rar                            'RAR archive data'                                         application/vnd.rar        binary
rar 5                          'RAR archive data, v5'                                     application/vnd.rar        binary
pdf                            'PDF document, version 1.4, 3 page(s)'                     application/pdf            us-ascii
a java class                   'compiled Java class data, version 52.0 (Java 1.8)'        application/x-java-applet  binary
webassembly                    'WebAssembly (wasm) binary module version 0x1 (MVP)'       application/wasm           binary
sqlite                         'SQLite 3.x database, last written using SQLite version 0, file counter 0, database pages 0, cookie 0, schema 0, unknown 0 encoding, version-valid-for 0' application/vnd.sqlite3    binary
a webp                         'RIFF (little-endian) data, Web/P image'                   image/webp                 binary
a wave                         'RIFF (little-endian) data, WAVE audio, Microsoft PCM, 16 bit, stereo 44100 Hz' audio/x-wav                binary
an mp3 frame                   'MPEG ADTS, layer III, v1, 64 kbps, 44.1 kHz, Monaural'    audio/mpeg                 binary
an mp4                         'ISO Media, MP4 v2 [ISO 14496-14]'                         video/mp4                  binary
an avif                        'ISO Media, AVIF Image'                                    image/avif                 binary
-- the type alone, where the description carries a deeper parse
a jpeg                 image/jpeg                     binary
a tiff                 image/tiff                     binary
an icon                image/vnd.microsoft.icon       binary
a bitmap               image/bmp                      binary
a truetype font        font/sfnt                      binary
an opentype font       application/vnd.ms-opentype    binary
a woff                 font/woff                      binary
flac                   audio/flac                     binary
ogg                    audio/ogg                      binary
matroska               application/octet-stream       binary
an ar archive          application/x-archive          us-ascii
-- the two mime faces together, and the extension one
string(28) "text/plain; charset=us-ascii"
string(25) "image/png; charset=binary"
string(35) "application/x-empty; charset=binary"
string(3) "png"
string(3) "???"
string(3) "gif"
