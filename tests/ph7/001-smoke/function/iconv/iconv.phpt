--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: iconv() converts between UTF-8, ISO-8859-1 and US-ASCII
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
/* iconv() converts between the three code sets PHL models: UTF-8, ISO-8859-1
 * and US-ASCII. Everything here is php's own answer -- the contract is really
 * the C library's iconv(3), so each rule was measured against it. */
$iconvW = function ($no, $str) { echo "  W: $str\n"; return true; };
set_error_handler($iconvW);
/* The two everyday directions: legacy in, legacy out. */
echo bin2hex(iconv("ISO-8859-1", "UTF-8", "caf\xE9")), "\n";
echo bin2hex(iconv("UTF-8", "ISO-8859-1", "caf\xC3\xA9")), "\n";
/* An identity conversion still DECODES, so it is not a byte copy. */
echo bin2hex(iconv("UTF-8", "UTF-8", "caf\xC3\xA9")), "\n";
echo var_export(iconv("UTF-8", "UTF-8", "a\xFFb"), true), "\n";
/* Latin-1 holds every byte, so nothing about it can be ill-formed. */
echo bin2hex(iconv("ISO-8859-1", "ISO-8859-1", "a\xE9\xFFb")), "\n";
echo bin2hex(iconv("ISO-8859-1", "UTF-8", "\x00\x7F\x80\xFF")), "\n";
/* ASCII refuses its own high bytes in either direction. */
echo var_export(iconv("ASCII", "UTF-8", "a\xE9b"), true), "\n";
echo var_export(iconv("UTF-8", "ASCII", "a\xC3\xA9b"), true), "\n";
echo bin2hex(iconv("ASCII", "ASCII", "abc")), "\n";
/* The empty string converts to the empty string, whatever the pair. */
echo var_export(iconv("UTF-8", "ASCII", ""), true), "|",
     var_export(iconv("ASCII", "ISO-8859-1", ""), true), "\n";
/* An embedded NUL is a character like any other. */
echo bin2hex(iconv("UTF-8", "ISO-8859-1", "a\x00b")), "\n";
/* The UTF-8 accepted is the original six-byte encoding, not Unicode's cut:
 * U+110000 and U+4000000 are characters here, while overlongs, the surrogate
 * range and the C0/C1/FE/FF lead bytes are not. */
foreach (["f4908080", "f5808080", "f7bfbfbf", "fbbfbfbfbf", "fc848080 8080",
          "c080", "c1bf", "e08080", "eda080", "f0808080", "f884808080",
          "ff", "fe", "80", "c328", "e28228"] as $iconvHex) {
    $iconvIn = hex2bin(str_replace(" ", "", $iconvHex));
    $iconvOut = iconv("UTF-8", "UTF-8", $iconvIn);
    echo str_replace(" ", "", $iconvHex), " => ",
         ($iconvOut === false ? "false" : bin2hex($iconvOut)), "\n";
}
/* A truncated but so-far-valid tail is the OTHER diagnostic. */
foreach (["c3", "e282", "f09f92", "61c3", "fd"] as $iconvHex) {
    echo $iconvHex, " => ",
         var_export(iconv("UTF-8", "ISO-8859-1", hex2bin($iconvHex)), true), "\n";
}
restore_error_handler();
?>
--EXPECT--
636166c3a9
636166e9
636166c3a9
  W: iconv(): Detected an illegal character in input string
false
61e9ff62
007fc280c3bf
  W: iconv(): Detected an illegal character in input string
false
  W: iconv(): Detected an illegal character in input string
false
616263
''|''
610062
f4908080 => f4908080
f5808080 => f5808080
f7bfbfbf => f7bfbfbf
fbbfbfbfbf => fbbfbfbfbf
fc8480808080 => fc8480808080
  W: iconv(): Detected an illegal character in input string
c080 => false
  W: iconv(): Detected an illegal character in input string
c1bf => false
  W: iconv(): Detected an illegal character in input string
e08080 => false
  W: iconv(): Detected an illegal character in input string
eda080 => false
  W: iconv(): Detected an illegal character in input string
f0808080 => false
  W: iconv(): Detected an illegal character in input string
f884808080 => false
  W: iconv(): Detected an illegal character in input string
ff => false
  W: iconv(): Detected an illegal character in input string
fe => false
  W: iconv(): Detected an illegal character in input string
80 => false
  W: iconv(): Detected an illegal character in input string
c328 => false
  W: iconv(): Detected an illegal character in input string
e28228 => false
c3 =>   W: iconv(): Detected an incomplete multibyte character in input string
false
e282 =>   W: iconv(): Detected an incomplete multibyte character in input string
false
f09f92 =>   W: iconv(): Detected an incomplete multibyte character in input string
false
61c3 =>   W: iconv(): Detected an incomplete multibyte character in input string
false
fd =>   W: iconv(): Detected an incomplete multibyte character in input string
false
