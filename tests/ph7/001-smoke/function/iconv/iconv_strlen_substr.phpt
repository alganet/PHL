--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: iconv_strlen() and iconv_substr() count and slice in characters
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
/* iconv_strlen() and iconv_substr() count and slice in CHARACTERS of a named
 * encoding. The `?string $encoding = null` all four of these share is php's
 * INTERNAL encoding, which is `default_charset` here (the scope policy removes the
 * deprecated `iconv.*` directives that would otherwise override it). */
$icvSW = function ($no, $str) { echo "  W: $str\n"; return true; };
set_error_handler($icvSW);
$icvU = "a\u{e9}b\u{4e2d}c";        /* 5 characters, 8 bytes */
$icvL = "a\xE9b\xFFc";              /* 5 characters, 5 bytes, all valid Latin-1 */
echo strlen($icvU), " ", iconv_strlen($icvU), "\n";
echo iconv_strlen(""), " ", iconv_strlen("abc"), " ", iconv_strlen($icvU, "UTF-8"), "\n";
echo iconv_strlen($icvL, "ISO-8859-1"), " ", var_export(iconv_strlen($icvL, "ASCII"), true), "\n";
/* The six-byte code points count as one character each. */
echo iconv_strlen("\xF4\x90\x80\x80"), " ", iconv_strlen("\xFC\x84\x80\x80\x80\x80"), "\n";
/* Ill-formed input is FALSE with the same two diagnostics iconv() raises, and
 * the conversion named in the third one is the one into the wide intermediate. */
echo var_export(iconv_strlen("a\xFFb"), true), "\n";
echo var_export(iconv_strlen("ab\xC3"), true), "\n";
echo var_export(iconv_strlen("abc", "NOPE"), true), "\n";
echo var_export(iconv_strlen("abc", str_repeat("X", 64)), true), "\n";
echo "--- substr\n";
foreach ([[0, null], [1, 3], [1, null], [-2, null], [-2, 1], [-99, null], [99, null],
          [5, null], [0, 0], [0, -1], [1, -1], [1, -99], [0, 99], [2, 99], [-99, 2],
          [PHP_INT_MAX, null], [PHP_INT_MIN, null], [0, PHP_INT_MAX], [0, PHP_INT_MIN]] as $icvC) {
    echo str_pad($icvC[0] . "," . var_export($icvC[1], true), 26), " ",
         bin2hex(iconv_substr($icvU, $icvC[0], $icvC[1])), "\n";
}
/* A slice of a one-byte encoding is a byte slice, and it can carry any byte. */
echo bin2hex(iconv_substr($icvL, 1, 2, "ISO-8859-1")), "\n";
/* An ill-formed string has no slice at all, not a truncated one. */
echo var_export(iconv_substr("a\xFFb", 0, 2), true), "\n";
echo var_export(iconv_substr("ab\xC3", 0, 2), true), "\n";
echo var_export(iconv_substr("abc", 0, 1, "NOPE"), true), "\n";
echo var_export(iconv_substr("", 0, null), true), "\n";
restore_error_handler();
?>
--EXPECT--
8 5
0 3 5
5   W: iconv_strlen(): Detected an illegal character in input string
false
1 1
  W: iconv_strlen(): Detected an illegal character in input string
false
  W: iconv_strlen(): Detected an incomplete multibyte character in input string
false
  W: iconv_strlen(): Wrong encoding, conversion from "NOPE" to "UCS-4LE" is not allowed
false
  W: iconv_strlen(): Encoding parameter exceeds the maximum allowed length of 64 characters
false
--- substr
0,NULL                     61c3a962e4b8ad63
1,3                        c3a962e4b8ad
1,NULL                     c3a962e4b8ad63
-2,NULL                    e4b8ad63
-2,1                       e4b8ad
-99,NULL                   61c3a962e4b8ad63
99,NULL                    
5,NULL                     
0,0                        
0,-1                       61c3a962e4b8ad
1,-1                       c3a962e4b8ad
1,-99                      
0,99                       61c3a962e4b8ad63
2,99                       62e4b8ad63
-99,2                      61c3a9
9223372036854775807,NULL   
-9223372036854775808,NULL  61c3a962e4b8ad63
0,9223372036854775807      61c3a962e4b8ad63
0,-9223372036854775807-1   
e962
  W: iconv_substr(): Detected an illegal character in input string
false
  W: iconv_substr(): Detected an incomplete multibyte character in input string
false
  W: iconv_substr(): Wrong encoding, conversion from "NOPE" to "UCS-4LE" is not allowed
false
''
