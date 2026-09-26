--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: iconv() //IGNORE — php's suffix rule, and what a truncated tail does under it
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
/* //IGNORE. Two different mechanisms wear this name, and only one of them is
 * useful. The C library's drops a character the TARGET cannot hold and then
 * reports the whole conversion as failed anyway; php's -- a case-SENSITIVE
 * SUFFIX test for `//IGNORE` or `//IGNORE//TRANSLIT` on the TO charset only --
 * is what turns that into a value. So the spelling matters. */
$iconvIgW = function ($no, $str) { echo "  W: $str\n"; return true; };
set_error_handler($iconvIgW);
/* The everyday call: everything the target cannot hold just goes. */
echo var_export(iconv("UTF-8", "ASCII//IGNORE", "a\u{e9}b\u{4e2d}c"), true), "\n";
echo var_export(iconv("UTF-8", "ISO-8859-1//IGNORE", "a\u{20ac}b"), true), "\n";
/* Ill-formed INPUT is dropped too, one byte at a time. */
echo var_export(iconv("UTF-8", "ISO-8859-1//IGNORE", "a\xFFb"), true), "\n";
echo var_export(iconv("UTF-8", "ASCII//IGNORE", "a\x80\xC0\x80b"), true), "\n";
/* Both suffix spellings php accepts, and the ones it does not. */
foreach (["ASCII//IGNORE", "ASCII//IGNORE//TRANSLIT", "ASCII//TRANSLIT//IGNORE",
          "ASCII//IGNORE,TRANSLIT"] as $iconvN) {
    echo str_pad($iconvN, 26), " ", var_export(iconv("UTF-8", $iconvN, "a\u{e9}b"), true), "\n";
}
/* On the FROM charset the suffix asks for nothing at all. */
echo var_export(iconv("UTF-8//IGNORE", "ISO-8859-1", "a\xFFb"), true), "\n";
/* php's check has a length guard, so the bare suffix is not one. */
echo var_export(iconv("UTF-8", "//IGNORE", "a\u{e9}b"), true), "\n";
/* TRANSLIT wins where it has an answer: ignoring is what is left over. */
echo var_export(iconv("UTF-8", "ASCII//TRANSLIT//IGNORE", "a\u{e9}\u{4e2d}\u{300}b"), true), "\n";
echo var_export(iconv("UTF-8", "ASCII//IGNORE", "a\u{e9}\u{4e2d}\u{300}b"), true), "\n";
/* A TRUNCATED tail is not an illegal sequence and is not ignored -- unless a
 * character has already been dropped for being unconvertible, which is the one
 * place the library's mechanism becomes visible through php's. */
foreach (["6162fd", "e188a2fd", "e188a26162fd", "88a282fd", "61ff62fd"] as $iconvHex) {
    echo $iconvHex, " => ",
         var_export(iconv("UTF-8", "ISO-8859-1//IGNORE", hex2bin($iconvHex)), true), "\n";
}
/* Ignoring everything leaves the empty string, not a failure. */
echo var_export(iconv("UTF-8", "ASCII//IGNORE", "\u{e9}\u{e8}\u{ea}"), true), "\n";
restore_error_handler();
?>
--EXPECT--
'abc'
'ab'
'ab'
'ab'
ASCII//IGNORE              'ab'
ASCII//IGNORE//TRANSLIT    'aeb'
ASCII//TRANSLIT//IGNORE    'aeb'
ASCII//IGNORE,TRANSLIT     'aeb'
  W: iconv(): Detected an illegal character in input string
false
'aéb'
'ae?b'
'ab'
6162fd =>   W: iconv(): Detected an incomplete multibyte character in input string
false
e188a2fd => ''
e188a26162fd => 'ab'
88a282fd =>   W: iconv(): Detected an incomplete multibyte character in input string
false
61ff62fd =>   W: iconv(): Detected an incomplete multibyte character in input string
false
''
