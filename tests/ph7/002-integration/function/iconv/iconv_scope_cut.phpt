--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: iconv() models three code sets and names its own implementation (PHL half of the twin pair)
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PHL-pinned half of the twin pair";
}
?>
--FILE--
<?php
/* php's iconv is a shell over the platform's iconv(3), so which code sets it
 * knows is the platform's answer -- glibc has hundreds, musl has fewer, and
 * Windows' libiconv has a different set again. PHL converts with its own code
 * so a Windows build answers what a POSIX one does, and §10's scope cut fixes
 * that set at the three mb_ already models: UTF-8, ISO-8859-1 and US-ASCII.
 * Every other php-VALID name gets php's own "Wrong encoding" warning, which is
 * what php itself answers for a name its platform does not have. See the _zend
 * half for what glibc converts. */
set_error_handler(function ($no, $str) { echo "  W: $str\n"; return true; });
foreach (["SJIS", "EUC-JP", "KOI8-R", "WINDOWS-1252", "ISO-8859-15", "UTF-16LE",
          "UCS-2", "CP850", "MACINTOSH", "IBM903"] as $name) {
    echo str_pad($name, 14), " ", var_export(iconv("UTF-8", $name, "abc"), true), "\n";
}
restore_error_handler();
/* And what the extension says it IS. php reports the C library behind it
 * (`glibc`, its version); PHL reports itself and its own version, because a
 * program that branches on these has to see something true. */
var_dump(ICONV_IMPL, ICONV_VERSION === PHP_VERSION);
/* The two $mode bits iconv_mime_decode() reads are php's numbers either way. */
var_dump(ICONV_MIME_DECODE_STRICT, ICONV_MIME_DECODE_CONTINUE_ON_ERROR);
?>
--EXPECT--
SJIS             W: iconv(): Wrong encoding, conversion from "UTF-8" to "SJIS" is not allowed
false
EUC-JP           W: iconv(): Wrong encoding, conversion from "UTF-8" to "EUC-JP" is not allowed
false
KOI8-R           W: iconv(): Wrong encoding, conversion from "UTF-8" to "KOI8-R" is not allowed
false
WINDOWS-1252     W: iconv(): Wrong encoding, conversion from "UTF-8" to "WINDOWS-1252" is not allowed
false
ISO-8859-15      W: iconv(): Wrong encoding, conversion from "UTF-8" to "ISO-8859-15" is not allowed
false
UTF-16LE         W: iconv(): Wrong encoding, conversion from "UTF-8" to "UTF-16LE" is not allowed
false
UCS-2            W: iconv(): Wrong encoding, conversion from "UTF-8" to "UCS-2" is not allowed
false
CP850            W: iconv(): Wrong encoding, conversion from "UTF-8" to "CP850" is not allowed
false
MACINTOSH        W: iconv(): Wrong encoding, conversion from "UTF-8" to "MACINTOSH" is not allowed
false
IBM903           W: iconv(): Wrong encoding, conversion from "UTF-8" to "IBM903" is not allowed
false
string(3) "PHL"
bool(false)
int(1)
int(2)
--CLEAN--
<?php
