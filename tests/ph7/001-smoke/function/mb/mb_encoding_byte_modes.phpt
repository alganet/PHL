--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: the mb_* $encoding argument is APPLIED (8bit/binary/ISO-8859-1 count bytes, ASCII has error characters)
--FILE--
<?php
// One byte per character: every count, offset and slice is a BYTE there.
echo mb_strlen("áb"), "|", mb_strlen("áb", "8bit"), "|", mb_strlen("áb", "ISO-8859-1"), "\n";
echo mb_strpos("áb", "b"), "|", mb_strpos("áb", "b", 0, "8bit"), "|", mb_strpos("áb", "b", 0, "ASCII"), "\n";
echo var_export(mb_strpos("ábc", "b", 2, "8bit"), true), "|", mb_strrpos("a\xe1a\xe1", "\xe1", 0, "8bit"), "\n";
echo bin2hex(mb_substr("ábc", 1, 1, "8bit")), "|", bin2hex(mb_substr("ábc", 1, 1)), "\n";
echo implode(",", array_map("bin2hex", mb_str_split("áb", 1, "8bit"))), "\n";
echo mb_strwidth("áb", "8bit"), "|", mb_strwidth("áb"), "\n";

// A byte over 0x7F is a character of 8bit/ISO-8859-1 and an ERROR character of
// ASCII and UTF-8 -- which is the whole of what mb_check_encoding answers.
var_dump(mb_check_encoding("\xff", "8bit"), mb_check_encoding("\xff", "binary"),
         mb_check_encoding("\xff", "ASCII"), mb_check_encoding("\xff"));
// and it checks EVERY element of an array, not the six bytes of the word "Array"
var_dump(mb_check_encoding(["ok", "fine"], "ASCII"), mb_check_encoding(["ok", "\xff"], "ASCII"));

// Case mapping in a one-byte encoding is Latin-1's: the byte IS the code point.
// A result the encoding cannot hold (0xFF upper-cases to U+0178) substitutes.
echo bin2hex(mb_strtolower("\xC1\xE1z", "8bit")), "|", bin2hex(mb_strtoupper("\xE1z", "ISO-8859-1")), "\n";
echo bin2hex(mb_strtoupper("\xff", "8bit")), "|", bin2hex(mb_strtoupper("\xdf", "8bit")), "\n";
// ASCII has no such code points at all, so they are error characters -> '?'
echo bin2hex(mb_strtolower("\xC1\xE1z", "ASCII")), "|", bin2hex(mb_convert_case("\xc1", 0, "ASCII")), "\n";

// mb_ord/mb_chr answer in the same terms
var_dump(mb_ord("\xff", "8bit"), mb_ord("\xff", "ASCII"), mb_ord("á"));
echo bin2hex(mb_chr(233, "8bit")), "|", var_export(mb_chr(233, "ASCII"), true), "|", var_export(mb_chr(256, "8bit"), true), "\n";
echo bin2hex(mb_chr(65, "ASCII")), "|", bin2hex(mb_chr(233)), "\n";

// mb_trim's default set is code points, so 0xA0 is NBSP under 8bit and an error
// character (not in the set) under ASCII
echo bin2hex(mb_trim("\xa0a\xa0", null, "8bit")), "|", bin2hex(mb_trim("\xa0a\xa0", null, "ASCII")), "\n";

// ISO-8859-1 is detectable, and every byte string is valid in it; 8bit/binary
// are named, counted and never chosen
echo mb_detect_encoding("\xff", ["UTF-8", "ISO-8859-1"]), "|", mb_detect_encoding("abc", ["ISO-8859-1", "ASCII"]), "\n";
// a candidate that reads FEWER characters wins a tie, whatever the order
echo mb_detect_encoding("á", ["ISO-8859-1", "UTF-8"]), "|", mb_detect_encoding("abc", ["8bit", "ASCII"]), "\n";
var_dump(mb_detect_encoding("abc", ["8bit"]), mb_detect_encoding("\xff", "binary"));

// an encoding outside the modelled set is still php's loud ValueError
try { mb_strlen("x", "BOGUS"); } catch (ValueError $mbenc) { echo $mbenc->getMessage(), "\n"; }
// a chunk count that does not fit 32 bits used to TRUNCATE into single characters
try { mb_str_split("abcde", 4294967297); } catch (ValueError $mbenc) { echo $mbenc->getMessage(), "\n"; }
echo count(mb_str_split("abcde", 1073741823)), "\n";
?>
--EXPECT--
2|3|3
1|2|2
2|3
a1|62
c3,a1,62
3|2
bool(true)
bool(true)
bool(false)
bool(false)
bool(true)
bool(false)
e1e17a|c15a
3f|5353
3f3f7a|3f
int(255)
bool(false)
int(225)
e9|false|false
41|c3a9
61|a061a0
ISO-8859-1|ISO-8859-1
UTF-8|ASCII
bool(false)
bool(false)
mb_strlen(): Argument #2 ($encoding) must be a valid encoding, "BOGUS" given
mb_str_split(): Argument #2 ($length) is too large
1
--CLEAN--
<?php
