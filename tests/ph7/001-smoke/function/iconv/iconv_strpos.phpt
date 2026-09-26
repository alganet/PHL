--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: iconv_strpos()/iconv_strrpos() — argument order, and what an ill-formed haystack does NOT say
--FILE--
<?php
/* iconv_strpos() and iconv_strrpos() search in characters. They differ in more
 * than direction: strrpos has no $offset at all, it answers the empty needle
 * before it even looks at $encoding, and it has no out-of-bounds ValueError. */
$icvPW = function ($no, $str) { echo "  W: $str\n"; return true; };
set_error_handler($icvPW);
$icvH = "a\u{e9}b\u{e9}b";          /* 5 characters */
echo iconv_strpos($icvH, "b"), " ", iconv_strrpos($icvH, "b"), "\n";
echo iconv_strpos($icvH, "b", 3), " ", iconv_strpos($icvH, "b", -2), "\n";
echo iconv_strpos($icvH, "\u{e9}b"), " ", iconv_strrpos($icvH, "\u{e9}"), "\n";
echo var_export(iconv_strpos($icvH, "z"), true), " ",
     var_export(iconv_strrpos($icvH, "z"), true), "\n";
/* Overlapping matches count, and strrpos takes the LAST one. */
echo iconv_strpos("aaa", "aa"), " ", iconv_strrpos("aaaa", "aa"), "\n";
echo iconv_strpos("abcabc", "bc", 1), " ", iconv_strrpos("abcabc", "bc"), "\n";
echo iconv_strpos("abc", "abc"), " ", iconv_strrpos("abc", "abc"), "\n";
/* An empty needle is false, never 0. */
echo var_export(iconv_strpos($icvH, ""), true), " ",
     var_export(iconv_strrpos($icvH, ""), true), "\n";
/* An empty haystack finds nothing. */
echo var_export(iconv_strpos("", "a"), true), " ",
     var_export(iconv_strrpos("", "a"), true), "\n";
/* $offset may equal the length (no match) but not exceed it (ValueError), and
 * a negative one that is still negative after counting back is the same error. */
echo var_export(iconv_strpos("abc", "a", 3), true), "\n";
try { iconv_strpos("abc", "a", 4); } catch (ValueError $icvE) { echo $icvE->getMessage(), "\n"; }
try { iconv_strpos($icvH, "b", -99); } catch (ValueError $icvE) { echo $icvE->getMessage(), "\n"; }
/* But the empty needle answers first, so an out-of-range $offset with one is
 * not an error at all. */
echo var_export(iconv_strpos("abc", "", 99), true), "\n";
/* An ill-formed NEEDLE goes through a whole conversion, so it is loud. */
echo var_export(iconv_strpos($icvH, "\xFF"), true), "\n";
echo var_export(iconv_strrpos($icvH, "\xC3"), true), "\n";
/* An ill-formed HAYSTACK is not: the walk stops at the bad character, and php
 * only hears about it from the call that converted the one before -- so an
 * error at index 0 is silent, and one a full match ended the walk before is
 * silent too. */
echo var_export(iconv_strpos("\xFFabcd", "ab"), true), "\n";
echo var_export(iconv_strpos("ab\xFFcd", "ab"), true), "\n";
echo var_export(iconv_strpos("ab\xFFcd", "cd"), true), "\n";
echo var_export(iconv_strpos(str_repeat("x", 100) . "\xFF" . "yy", "xx"), true), "\n";
echo var_export(iconv_strrpos("\xFFabcd", "ab"), true), "\n";
/* And the count the ValueError is measured against is how far that walk got. */
try { iconv_strpos("\xFFabcd", "ab", 2); } catch (ValueError $icvE) { echo $icvE->getMessage(), "\n"; }
/* strrpos looks at the needle before the name, so an over-long $encoding is
 * silent there and warns in strpos. */
echo var_export(iconv_strrpos("abc", "", str_repeat("X", 64)), true), "\n";
echo var_export(iconv_strpos("abc", "b", 0, str_repeat("X", 64)), true), "\n";
/* And an UNKNOWN name is only heard about where a conversion would have been
 * opened, which the empty needle never reaches. */
echo var_export(iconv_strpos("abc", "", 0, "NOPE"), true), "\n";
echo var_export(iconv_strpos("abc", "b", 0, "NOPE"), true), "\n";
/* A one-byte encoding searches bytes. */
echo iconv_strrpos("a\xE9b\xE9", "\xE9", "ISO-8859-1"), " ",
     var_export(iconv_strpos("a\xE9b\xE9", "\xE9", 2, "ISO-8859-1"), true), "\n";
restore_error_handler();
?>
--EXPECT--
2 4
4 4
1 3
false false
0 2
1 4
0 0
false false
false false
false
iconv_strpos(): Argument #3 ($offset) must be contained in argument #1 ($haystack)
iconv_strpos(): Argument #3 ($offset) must be contained in argument #1 ($haystack)
false
  W: iconv_strpos(): Detected an illegal character in input string
false
  W: iconv_strrpos(): Detected an incomplete multibyte character in input string
false
false
  W: iconv_strpos(): Detected an illegal character in input string
false
  W: iconv_strpos(): Detected an illegal character in input string
false
0
false
iconv_strpos(): Argument #3 ($offset) must be contained in argument #1 ($haystack)
false
  W: iconv_strpos(): Encoding parameter exceeds the maximum allowed length of 64 characters
false
false
  W: iconv_strpos(): Wrong encoding, conversion from "NOPE" to "UCS-4LE" is not allowed
false
3 3
