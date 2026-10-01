--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php: iconv() knows whatever the platform's iconv(3) knows (zend half of the twin pair)
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip zend-pinned half of the twin pair";
} elseif (!defined('ICONV_IMPL') || ICONV_IMPL !== 'glibc') {
    // the ISO-2022-JP row is glibc's spelling; libiconv (macOS, Windows) refuses it
    echo 'skip the oracle iconv is not glibc';
}
?>
--FILE--
<?php
/* php's iconv is a shell over the platform's iconv(3), so WHICH code sets it
 * knows is the platform's answer and not php's: glibc has hundreds, musl far
 * fewer, and the Windows build's libiconv a different set again. That is why
 * PHL converts with its own code and the scope policy fixes the set at UTF-8, ISO-8859-1
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
/* And glibc's //TRANSLIT reaches a halfwidth katakana's fullwidth form through
 * the compatibility decomposition it carries for every character, so it lands
 * in JIS X 0208 rather than on the '?' PHL answers -- see the PHL half. */
var_dump(bin2hex(iconv("UTF-8", "ISO-2022-JP//TRANSLIT", "\u{ff71}")));
/* And the extension names the C library behind it, never the engine. */
var_dump(ICONV_IMPL !== 'PHL', is_string(ICONV_IMPL), ICONV_VERSION === PHP_VERSION);
/* The two $mode bits iconv_mime_decode() reads are php's numbers either way. */
var_dump(ICONV_MIME_DECODE_STRICT, ICONV_MIME_DECODE_CONTINUE_ON_ERROR);
/* php's answer to the two conversions the scope policy refuses in this family's int
 * parameters: E_DEPRECATED and the value it would have coerced to. */
set_error_handler(function ($no, $str) { echo "  D: $str\n"; return true; });
foreach ([
    'substr null offset' => fn () => iconv_substr("abc", null),
    'strpos null offset' => fn () => iconv_strpos("abc", "b", null),
    'substr lossy float' => fn () => iconv_substr("abcdef", 1.5),
    'substr exact float' => fn () => iconv_substr("abcdef", 1.0),
] as $label => $case) {
    echo str_pad($label, 20), " ";
    try { var_export($case()); } catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(); }
    echo "\n";
}
restore_error_handler();
?>
--EXPECT--
bool(true)
string(16) "1b244225221b2842"
bool(true)
bool(true)
bool(false)
int(1)
int(2)
substr null offset     D: iconv_substr(): Passing null to parameter #2 ($offset) of type int is deprecated
'abc'
strpos null offset     D: iconv_strpos(): Passing null to parameter #3 ($offset) of type int is deprecated
1
substr lossy float     D: Implicit conversion from float 1.5 to int loses precision
'bcdef'
substr exact float   'bcdef'
--CLEAN--
<?php
