--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: mb_detect_encoding, ASCII/UTF-8 scope (strict + non-strict, list forms)
--FILE--
<?php
// default detect order is exactly ASCII, then UTF-8
echo mb_detect_encoding("hello"), "|", mb_detect_encoding("héllo"), "\n";
// strict: no candidate fully matches -> false; empty string is valid ASCII
echo var_export(mb_detect_encoding("\xff\xfe", null, true), true), "|", mb_detect_encoding("", null, true), "\n";
// explicit array list, and list order picks the earliest fully-valid candidate
echo mb_detect_encoding("héllo", ["ASCII","UTF-8"], true), "|", mb_detect_encoding("hello", ["UTF-8","ASCII"], true), "\n";
// comma-separated string list, whitespace around names tolerated
echo mb_detect_encoding("héllo", "ASCII, UTF-8", true), "\n";
// non-strict scores by fewest undecodable bytes, earliest on a tie
echo mb_detect_encoding("\xff\xfe"), "|", mb_detect_encoding("héllo\xff"), "\n";
// same input, strict -> false (the trailing byte breaks UTF-8, ASCII already out)
echo var_export(mb_detect_encoding("héllo\xff", null, true), true), "\n";
// encoding names are matched case-insensitively and returned canonicalised
echo mb_detect_encoding("hello", "ascii", true), "|", mb_detect_encoding("héllo", "utf8", true), "\n";
// an empty list and an out-of-scope name each raise php's ValueError
try { mb_detect_encoding("hi", [], true); } catch (ValueError $mbdeE) { echo $mbdeE->getMessage(), "\n"; }
try { mb_detect_encoding("hi", ["ASCII","BOGUS"], true); } catch (ValueError $mbdeE) { echo $mbdeE->getMessage(), "\n"; }
?>
--EXPECT--
ASCII|UTF-8
false|ASCII
UTF-8|UTF-8
UTF-8
ASCII|UTF-8
false
ASCII|UTF-8
mb_detect_encoding(): Argument #2 ($encodings) must specify at least one encoding
mb_detect_encoding(): Argument #2 ($encodings) contains invalid encoding "BOGUS"
