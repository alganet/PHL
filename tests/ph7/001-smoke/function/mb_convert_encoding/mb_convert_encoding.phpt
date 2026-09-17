--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: mb_convert_encoding, UTF-8 / ISO-8859-1 scope (supersedes utf8_encode/decode)
--FILE--
<?php
// ISO-8859-1 -> UTF-8: the utf8_encode() replacement path
echo bin2hex(mb_convert_encoding("caf\xe9", "UTF-8", "ISO-8859-1")), "\n";
// UTF-8 -> ISO-8859-1: the utf8_decode() replacement path
echo bin2hex(mb_convert_encoding("caf\xc3\xa9", "ISO-8859-1", "UTF-8")), "\n";
// a codepoint the target cannot hold substitutes '?' (0x3F)
echo bin2hex(mb_convert_encoding("\xe2\x82\xac", "ISO-8859-1", "UTF-8")), "\n";
// UTF-8 -> UTF-8 is an identity round-trip
echo bin2hex(mb_convert_encoding("caf\xc3\xa9", "UTF-8", "UTF-8")), "\n";
// latin1 / 8bit are ISO-8859-1 aliases; names are case-insensitive
echo bin2hex(mb_convert_encoding("\xe9", "UTF-8", "latin1")), "|",
     bin2hex(mb_convert_encoding("\xe9", "UTF-8", "8bit")), "\n";
// $from_encoding omitted falls back to the internal encoding (UTF-8)
echo bin2hex(mb_convert_encoding("\xc3\xa9", "ISO-8859-1")), "\n";
// invalid input bytes each substitute '?' during decode
echo bin2hex(mb_convert_encoding("a\xff\xfeb", "ISO-8859-1", "UTF-8")), "\n";
// ASCII substitutes past 0x7F in both directions
echo bin2hex(mb_convert_encoding("\xe9", "UTF-8", "ASCII")), "|",
     bin2hex(mb_convert_encoding("caf\xc3\xa9", "ASCII", "UTF-8")), "\n";
// the array form transcodes every element, preserving keys and nesting
var_export(mb_convert_encoding(["a" => "\xe9", "b" => ["\xea"]], "UTF-8", "ISO-8859-1"));
echo "\n";
// an unusable target / source encoding raises php's ValueError (distinct wording)
try { mb_convert_encoding("x", "BOGUS", "UTF-8"); } catch (ValueError $mbceE) { echo $mbceE->getMessage(), "\n"; }
try { mb_convert_encoding("x", "UTF-8", "BOGUS"); } catch (ValueError $mbceE) { echo $mbceE->getMessage(), "\n"; }
?>
--EXPECT--
636166c3a9
636166e9
3f
636166c3a9
c3a9|c3a9
e9
613f3f62
3f|6361663f
array (
  'a' => 'é',
  'b' => 
  array (
    0 => 'ê',
  ),
)
mb_convert_encoding(): Argument #2 ($to_encoding) must be a valid encoding, "BOGUS" given
mb_convert_encoding(): Argument #3 ($from_encoding) contains invalid encoding "BOGUS"
