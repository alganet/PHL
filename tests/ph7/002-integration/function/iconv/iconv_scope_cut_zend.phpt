--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php: iconv() knows whatever the platform's iconv(3) knows (zend half of the twin pair)
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip zend-pinned half of the twin pair";
}
?>
--FILE--
<?php
/* php's iconv is a shell over the platform's iconv(3), so WHICH code sets it
 * knows is the platform's answer and not php's: glibc has hundreds, musl far
 * fewer, and the Windows build's libiconv a different set again. That is why
 * PHL converts with its own code and §10 fixes the set at UTF-8, ISO-8859-1
 * and US-ASCII — see the PHL half, which pins the exact refusal for each name
 * below. This half only pins the SHAPE of the divergence, since pinning
 * glibc's own list here would make the corpus depend on which libc built php. */
$extra = ["SJIS", "EUC-JP", "KOI8-R", "WINDOWS-1252", "ISO-8859-15", "UTF-16LE",
          "UCS-2", "CP850", "MACINTOSH"];
$accepted = 0;
foreach ($extra as $name) {
    if (@iconv("UTF-8", $name, "abc") !== false) {
        $accepted++;
    }
}
var_dump($accepted > 0);
/* And the extension names the C library behind it, never the engine. */
var_dump(ICONV_IMPL !== 'PHL', is_string(ICONV_IMPL), ICONV_VERSION === PHP_VERSION);
/* The two $mode bits iconv_mime_decode() reads are php's numbers either way. */
var_dump(ICONV_MIME_DECODE_STRICT, ICONV_MIME_DECODE_CONTINUE_ON_ERROR);
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(false)
int(1)
int(2)
--CLEAN--
<?php
