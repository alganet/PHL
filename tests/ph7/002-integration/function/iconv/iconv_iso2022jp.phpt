--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: iconv() converts ISO-2022-JP, whose escape sequences shift what the bytes after them mean
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
/* ISO-2022-JP is the one code set this family carries that is not a function
 * of the bytes in front of the converter: an escape sequence shifts which
 * character set the following bytes belong to and it stays shifted until the
 * next one. So a decode can consume bytes and produce no character at all, and
 * an encode has to know which set it already stands in -- and get back to
 * ASCII before the string ends. */
function e($label, $s) {
    $r = @iconv("UTF-8", "ISO-2022-JP", $s);
    echo "E ", str_pad($label, 26), ": ", ($r === false ? "FALSE" : bin2hex($r)), "\n";
}
function d($label, $hex) {
    $r = @iconv("ISO-2022-JP", "UTF-8", hex2bin($hex));
    echo "D ", str_pad($label, 26), ": ", ($r === false ? "FALSE" : bin2hex($r)), "\n";
}
/* The three names glibc registers for the plain converter. */
foreach (["ISO-2022-JP", "iso-2022-jp", "ISO2022JP", "CSISO2022JP"] as $name) {
    echo "alias ", str_pad($name, 14), ": ", bin2hex(@iconv("UTF-8", $name, "a")), "\n";
}
/* ENCODE. A kanji run opens with ESC $ B and the string closes back in ASCII;
 * the two cells JIS X 0201 Roman moved -- the yen sign and the overline --
 * take their own shift, and everything the set cannot hold is a refusal. */
e("ascii only", "abc");
e("kanji", "\u{611b}");
e("two kanji", "\u{611b}\u{611b}");
e("ascii then kanji", "z\u{611b}");
e("kanji then ascii", "\u{611b}z");
e("kanji then newline", "\u{611b}\n\u{611b}");
e("kanji then tab", "\u{611b}\t\u{611b}");
e("yen", "\u{00a5}");
e("overline", "\u{203e}");
e("yen then plain ascii", "\u{00a5}a");
e("yen then backslash", "\u{00a5}\\");
e("yen then tilde", "\u{00a5}~");
e("yen then control", "\u{00a5}\x1b");
e("yen then DEL", "\u{00a5}\x7f");
e("kanji then yen", "\u{611b}\u{00a5}");
e("fullwidth A", "\u{ff21}");
e("wave dash", "\u{301c}");
e("halfwidth katakana", "\u{ff71}");
e("fullwidth tilde", "\u{ff5e}");
e("euro", "\u{20ac}");
e("euro //IGNORE is elsewhere", "x\u{20ac}y");
e("nul", "a\0b");
e("empty", "");
echo "E ", str_pad("euro //TRANSLIT", 26), ": ",
    bin2hex(@iconv("UTF-8", "ISO-2022-JP//TRANSLIT", "\u{611b}\u{20ac}\u{611b}")), "\n";
echo "E ", str_pad("euro //IGNORE", 26), ": ",
    bin2hex(@iconv("UTF-8", "ISO-2022-JP//IGNORE", "\u{611b}\u{20ac}\u{611b}")), "\n";
/* DECODE. The four shifts, and what happens to everything that looks like one
 * and is not: ESC is below 0x21, so it comes out as U+001B and the bytes after
 * it are read in whatever set is current. */
d("plain ascii", "616263");
d("kanji run", "1b2442302630261b2842");
d("no closing shift", "1b24423026");
d("shift with nothing after", "61611b2442");
d("truncated shift", "61611b");
d("two-byte shift", "61611b24");
d("1978 spelling", "1b2440302630261b2842");
d("roman yen and overline", "1b284a5c7e1b2842");
d("roman plain ascii", "1b284a41421b2842");
d("roman then kanji", "1b284a1b2442302630261b2842");
d("unknown ESC ( I", "1b28493131");
d("unknown ESC ( H", "1b28486161");
d("unknown ESC ( Z", "611b285a62");
d("unknown ESC $ ( D", "1b242844302630261b2842");
d("controls inside a run", "1b244230260a30261b2842");
d("space inside a run", "1b244220201b2842");
d("DEL inside a run", "1b24427f1b2842");
d("high byte in ascii", "6180");
d("high byte in a run", "1b2442f0f01b2842");
d("odd byte in a run", "1b2442306161");
d("lone lead byte", "1b244230");
d("unassigned cell", "1b2442742a1b2842");
d("//IGNORE steps one byte", "1b2442217f1b2842");
/* The string family counts and slices CHARACTERS, so a shift is neither. A
 * slice of a shifted encoding is not a slice of its bytes: it opens with the
 * shift it needs and closes back in ASCII on its own. */
$jp = iconv("UTF-8", "ISO-2022-JP", "a\u{611b}\u{3044}b");
var_dump(bin2hex($jp), iconv_strlen($jp, "ISO-2022-JP"));
var_dump(bin2hex(iconv_substr($jp, 1, 2, "ISO-2022-JP")));
var_dump(bin2hex(iconv_substr($jp, 0, 2, "ISO-2022-JP")));
var_dump(bin2hex(iconv_substr($jp, 2, 2, "ISO-2022-JP")));
var_dump(iconv_strpos($jp, iconv("UTF-8", "ISO-2022-JP", "\u{3044}"), 0, "ISO-2022-JP"));
var_dump(iconv_mime_encode("S", "\u{611b}\u{3044}",
    ["input-charset" => "UTF-8", "output-charset" => "ISO-2022-JP"]));
/* And the round trip a template engine asks for. */
$s = "愛していますか？";
var_dump(iconv("ISO-2022-JP", "UTF-8", iconv("UTF-8", "ISO-2022-JP", $s)) === $s);
?>
--EXPECT--
alias ISO-2022-JP   : 61
alias iso-2022-jp   : 61
alias ISO2022JP     : 61
alias CSISO2022JP   : 61
E ascii only                : 616263
E kanji                     : 1b244230261b2842
E two kanji                 : 1b2442302630261b2842
E ascii then kanji          : 7a1b244230261b2842
E kanji then ascii          : 1b244230261b28427a
E kanji then newline        : 1b244230261b28420a1b244230261b2842
E kanji then tab            : 1b244230261b2842091b244230261b2842
E yen                       : 1b284a5c1b2842
E overline                  : 1b284a7e1b2842
E yen then plain ascii      : 1b284a5c611b2842
E yen then backslash        : 1b284a5c1b28425c
E yen then tilde            : 1b284a5c1b28427e
E yen then control          : 1b284a5c1b28421b
E yen then DEL              : 1b284a5c1b28427f
E kanji then yen            : 1b244230261b284a5c1b2842
E fullwidth A               : 1b244223411b2842
E wave dash                 : 1b244221411b2842
E halfwidth katakana        : FALSE
E fullwidth tilde           : FALSE
E euro                      : FALSE
E euro //IGNORE is elsewhere: FALSE
E nul                       : 610062
E empty                     : 
E euro //TRANSLIT           : 1b244230261b28424555521b244230261b2842
E euro //IGNORE             : 1b2442302630261b2842
D plain ascii               : 616263
D kanji run                 : e6849be6849b
D no closing shift          : e6849b
D shift with nothing after  : 6161
D truncated shift           : FALSE
D two-byte shift            : FALSE
D 1978 spelling             : e6849be6849b
D roman yen and overline    : c2a5e280be
D roman plain ascii         : 4142
D roman then kanji          : e6849be6849b
D unknown ESC ( I           : 1b28493131
D unknown ESC ( H           : 1b28486161
D unknown ESC ( Z           : 611b285a62
D unknown ESC $ ( D         : 1b24284430263026
D controls inside a run     : e6849b0ae6849b
D space inside a run        : 2020
D DEL inside a run          : 7f
D high byte in ascii        : FALSE
D high byte in a run        : FALSE
D odd byte in a run         : FALSE
D lone lead byte            : FALSE
D unassigned cell           : FALSE
D //IGNORE steps one byte   : FALSE
string(24) "611b2442302624241b284262"
int(4)
string(20) "1b2442302624241b2842"
string(18) "611b244230261b2842"
string(18) "1b244224241b284262"
int(2)
string(37) "S: =?ISO-2022-JP?B?GyRCMCYkJBsoQg==?="
bool(true)
--CLEAN--
<?php
