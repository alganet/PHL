--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: iconv() //TRANSLIT — the replacement, the empty rule and the '?' with no rule
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
/* //TRANSLIT. A code point the target cannot hold has THREE possible answers,
 * not two: the table's replacement, the EMPTY string (which is how every
 * combining mark disappears), or '?' when there is no rule at all. */
$iconvTrW = function ($no, $str) { echo "  W: $str\n"; return true; };
set_error_handler($iconvTrW);
/* One accented letter loses its accent; ß spells itself out. */
foreach (["\u{e9}", "\u{fc}", "\u{c5}", "\u{df}", "\u{e6}", "\u{d8}",
          "\u{101}", "\u{142}", "\u{1e9e}"] as $iconvC) {
    echo bin2hex($iconvC), " => ", var_export(iconv("UTF-8", "ASCII//TRANSLIT", $iconvC), true), "\n";
}
/* Punctuation and currency spell out; a Han character has no rule and is '?'. */
foreach (["\u{201c}q\u{201d}", "\u{2018}q\u{2019}", "\u{2013}", "\u{2026}",
          "\u{20ac}", "\u{a3}", "\u{a9}", "\u{ab}x\u{bb}", "\u{4e2d}",
          "\u{1f4a9}"] as $iconvC) {
    echo bin2hex($iconvC), " => ", var_export(iconv("UTF-8", "ASCII//TRANSLIT", $iconvC), true), "\n";
}
/* A combining mark transliterates to NOTHING, so `e` + U+0301 is `e`. */
echo var_export(iconv("UTF-8", "ASCII//TRANSLIT", "e\u{301}"), true), "\n";
echo var_export(iconv("UTF-8", "ASCII//TRANSLIT", "a\u{300}\u{308}b"), true), "\n";
/* ISO-8859-1 holds what it holds and transliterates the rest -- and its answer
 * is not always the ASCII one: Ǣ is Æ here and AE there, because the table
 * lists candidates in order and the first that FITS is taken. */
foreach (["\u{e9}", "\u{20ac}", "\u{1e2}", "\u{1fc}", "\u{1fe}", "\u{2032}",
          "\u{2103}", "\u{4e2d}"] as $iconvC) {
    echo bin2hex($iconvC), " l1=", bin2hex(iconv("UTF-8", "ISO-8859-1//TRANSLIT", $iconvC)),
         " ascii=", bin2hex(iconv("UTF-8", "ASCII//TRANSLIT", $iconvC)), "\n";
}
/* Transliteration decides nothing about ill-formed INPUT: that is still a
 * hard failure, with the same two diagnostics as without the suffix. */
echo var_export(iconv("UTF-8", "ASCII//TRANSLIT", "a\xFFb"), true), "\n";
echo var_export(iconv("UTF-8", "ASCII//TRANSLIT", "ab\xC3"), true), "\n";
/* The suffix on the FROM charset asks for nothing. */
echo var_export(iconv("UTF-8//TRANSLIT", "ASCII", "a\u{e9}b"), true), "\n";
/* A whole word, which is what a slug generator actually calls this for. */
echo iconv("UTF-8", "ASCII//TRANSLIT", "Cr\u{e8}me Br\u{fb}l\u{e9}e \u{2014} 5\u{20ac}"), "\n";
restore_error_handler();
?>
--EXPECT--
c3a9 => 'e'
c3bc => 'u'
c385 => 'A'
c39f => 'ss'
c3a6 => 'ae'
c398 => 'O'
c481 => 'a'
c582 => 'l'
e1ba9e => 'SS'
e2809c71e2809d => '"q"'
e2809871e28099 => '\'q\''
e28093 => '-'
e280a6 => '...'
e282ac => 'EUR'
c2a3 => 'GBP'
c2a9 => '(C)'
c2ab78c2bb => '<<x>>'
e4b8ad => '?'
f09f92a9 => '?'
'e'
'ab'
c3a9 l1=e9 ascii=65
e282ac l1=455552 ascii=455552
c7a2 l1=c6 ascii=4145
c7bc l1=c6 ascii=4145
c7be l1=d8 ascii=4f
e280b2 l1=b4 ascii=3f
e28483 l1=b043 ascii=3f
e4b8ad l1=3f ascii=3f
  W: iconv(): Detected an illegal character in input string
false
  W: iconv(): Detected an incomplete multibyte character in input string
false
  W: iconv(): Detected an illegal character in input string
false
Creme Brulee -- 5EUR
