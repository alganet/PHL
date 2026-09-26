--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: iconv() charset names — aliases, normalisation, the //TRANSLIT token and php's length cap
--SKIPIF--
<?php
// The rows are glibc's own tables and grammar, which PHL reproduces on every
// platform; a php over another iconv (libiconv on macOS and Windows) answers
// its library's, so only the oracle is skipped there.
if (function_exists('zend_version') && (!defined('ICONV_IMPL') || ICONV_IMPL !== 'glibc')) {
    echo 'skip the oracle iconv is not glibc';
}
?>
--FILE--
<?php
/* What NAMES a charset, and what the suffix after it asks for. Both are the C
 * library's grammar rather than php's: the code-set name is everything before
 * the first '/', compared case-insensitively after every character outside
 * [alnum] `_ - . , :` is dropped; the segment up to the second '/' must be
 * empty; and what follows is a '/'- and ','-separated list of error handlers. */
$iconvNameW = function ($no, $str) { echo "  W: $str\n"; return true; };
set_error_handler($iconvNameW);
/* Every alias of the three code sets, spelled as a program would spell it. */
foreach (["UTF-8", "utf-8", "UTF8", "utf8", "ISO-IR-193"] as $iconvN) {
    echo $iconvN, " ", bin2hex(iconv($iconvN, "ISO-8859-1", "\xC3\xA9")), "\n";
}
foreach (["ISO-8859-1", "iso-8859-1", "ISO_8859-1", "ISO8859-1", "ISO88591",
          "ISO_8859-1:1987", "ISO-IR-100", "LATIN1", "latin1", "L1", "CP819",
          "IBM819", "csISOLatin1", "8859_1"] as $iconvN) {
    echo $iconvN, " ", bin2hex(iconv($iconvN, "UTF-8", "\xE9")), "\n";
}
foreach (["ASCII", "ascii", "US-ASCII", "US", "ANSI_X3.4-1968", "ANSI_X3.4-1986",
          "ANSI_X3.4", "ISO646-US", "ISO_646.IRV:1991", "ISO-IR-6", "CP367",
          "IBM367", "CSASCII"] as $iconvN) {
    echo $iconvN, " ", bin2hex(iconv($iconvN, "UTF-8", "a")), "\n";
}
/* Characters the normaliser drops: a name survives spaces anywhere in it. */
foreach ([" ASCII ", "ASCII\t", "A SCII", "AS CI I"] as $iconvN) {
    echo var_export($iconvN, true), " ", bin2hex(iconv("UTF-8", $iconvN, "a")), "\n";
}
/* Characters it does not: a hyphen inside the name is part of the name. */
echo var_export(iconv("UTF-8", "as-cii", "a"), true), "\n";
/* The suffix grammar. TRANSLIT is recognised as a whole TOKEN, anywhere in the
 * handler list -- never as a substring, and never in the code-set name. */
foreach (["ASCII//TRANSLIT", "ASCII//translit", "ASCII//TRANSLIT/x",
          "ASCII//A/B/TRANSLIT", "ASCII//TRANSLIT,FOO", "ASCII//FOO,TRANSLIT",
          "ASCII/ /TRANSLIT", "ASCII//TRANSLITX", "ASCII//XTRANSLIT",
          "ASCII//FOO", "ASCII//", "ASCII/", "ASCII-TRANSLIT",
          "ASCII/x/TRANSLIT", "A/SCII//TRANSLIT"] as $iconvN) {
    echo str_pad(var_export($iconvN, true), 24), " ",
         var_export(iconv("UTF-8", $iconvN, "a\xC3\xA9b"), true), "\n";
}
/* A name over php's 64-character cap is refused before it is looked at. */
echo var_export(iconv("UTF-8", "ASCII" . str_repeat("X", 58), "ab"), true), "\n";
echo var_export(iconv("UTF-8", "ASCII" . str_repeat("X", 59), "ab"), true), "\n";
echo var_export(iconv(str_repeat("X", 64), "UTF-8", "ab"), true), "\n";
/* And a name stops at its first NUL, because the library takes a C string. */
echo var_export(iconv("UTF-8", "AS\x00CII", "x"), true), "\n";
echo var_export(iconv("UTF-8", "ASCII\x00ZZ", "x"), true), "\n";
/* An unknown name names the PAIR in the warning, in argument order. */
echo var_export(iconv("NOPE", "UTF-8", "x"), true), "\n";
echo var_export(iconv("UTF-8", "NOPE", "x"), true), "\n";
restore_error_handler();
?>
--EXPECT--
UTF-8 e9
utf-8 e9
UTF8 e9
utf8 e9
ISO-IR-193 e9
ISO-8859-1 c3a9
iso-8859-1 c3a9
ISO_8859-1 c3a9
ISO8859-1 c3a9
ISO88591 c3a9
ISO_8859-1:1987 c3a9
ISO-IR-100 c3a9
LATIN1 c3a9
latin1 c3a9
L1 c3a9
CP819 c3a9
IBM819 c3a9
csISOLatin1 c3a9
8859_1 c3a9
ASCII 61
ascii 61
US-ASCII 61
US 61
ANSI_X3.4-1968 61
ANSI_X3.4-1986 61
ANSI_X3.4 61
ISO646-US 61
ISO_646.IRV:1991 61
ISO-IR-6 61
CP367 61
IBM367 61
CSASCII 61
' ASCII ' 61
'ASCII	' 61
'A SCII' 61
'AS CI I' 61
  W: iconv(): Wrong encoding, conversion from "UTF-8" to "as-cii" is not allowed
false
'ASCII//TRANSLIT'        'aeb'
'ASCII//translit'        'aeb'
'ASCII//TRANSLIT/x'      'aeb'
'ASCII//A/B/TRANSLIT'    'aeb'
'ASCII//TRANSLIT,FOO'    'aeb'
'ASCII//FOO,TRANSLIT'    'aeb'
'ASCII/ /TRANSLIT'       'aeb'
'ASCII//TRANSLITX'         W: iconv(): Detected an illegal character in input string
false
'ASCII//XTRANSLIT'         W: iconv(): Detected an illegal character in input string
false
'ASCII//FOO'               W: iconv(): Detected an illegal character in input string
false
'ASCII//'                  W: iconv(): Detected an illegal character in input string
false
'ASCII/'                   W: iconv(): Detected an illegal character in input string
false
'ASCII-TRANSLIT'           W: iconv(): Wrong encoding, conversion from "UTF-8" to "ASCII-TRANSLIT" is not allowed
false
'ASCII/x/TRANSLIT'         W: iconv(): Wrong encoding, conversion from "UTF-8" to "ASCII/x/TRANSLIT" is not allowed
false
'A/SCII//TRANSLIT'         W: iconv(): Wrong encoding, conversion from "UTF-8" to "A/SCII//TRANSLIT" is not allowed
false
  W: iconv(): Wrong encoding, conversion from "UTF-8" to "ASCIIXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX" is not allowed
false
  W: iconv(): Encoding parameter exceeds the maximum allowed length of 64 characters
false
  W: iconv(): Encoding parameter exceeds the maximum allowed length of 64 characters
false
  W: iconv(): Wrong encoding, conversion from "UTF-8" to "AS" is not allowed
false
'x'
  W: iconv(): Wrong encoding, conversion from "NOPE" to "UTF-8" is not allowed
false
  W: iconv(): Wrong encoding, conversion from "UTF-8" to "NOPE" is not allowed
false
