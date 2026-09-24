--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: mb_str_pad (characters), mb_strcut (bytes) and mb_strimwidth (columns)
--FILE--
<?php
// mb_str_pad counts CHARACTERS, and STR_PAD_BOTH puts the smaller half left
echo mb_str_pad("á", 5), "|", mb_str_pad("á", 5, "-", STR_PAD_LEFT), "|", mb_str_pad("á", 5, "áb", STR_PAD_BOTH), "\n";
echo mb_str_pad("abc", 2), "|", mb_str_pad("日", 4, "ab"), "|", bin2hex(mb_str_pad("\xff", 3, "-")), "\n";
echo bin2hex(mb_str_pad("á", 5, "-", STR_PAD_RIGHT, "8bit")), "\n";
try { mb_str_pad("abc", 5, ""); } catch (ValueError $mbp) { echo $mbp->getMessage(), "\n"; }
try { mb_str_pad("abc", 5, "xy", 9); } catch (ValueError $mbp) { echo $mbp->getMessage(), "\n"; }

// mb_strcut counts BYTES and rounds both ends BACK to a character boundary
echo mb_strcut("áéí", 1), "|", mb_strcut("áéí", 2), "|", mb_strcut("áéí", 0, 3), "|[", mb_strcut("áéí", 0, 1), "]\n";
// a negative $length leaves that many bytes off the end, measured from the
// $start as GIVEN rather than from the rounded one
echo mb_strcut("áéí", 1, -2), "|", mb_strcut("áéí", -3), "|[", mb_strcut("áéí", 7), "]|", mb_strcut("abc", 1, 1, "8bit"), "\n";
echo bin2hex(mb_strcut("a\xffb", 0, 2)), "\n";

// mb_strimwidth counts COLUMNS, and the marker's width comes out of the budget
echo mb_strimwidth("hello world", 0, 7), "|", mb_strimwidth("hello world", 0, 7, "..."), "\n";
echo mb_strimwidth("日本語テキスト", 0, 8, "..."), "|", mb_strimwidth("hello", 0, 10, "..."), "|", mb_strimwidth("hello world", 2, 5, ".."), "\n";
// a marker wider than the width leaves nothing of the string
echo mb_strimwidth("hello", 0, 2, "..."), "|", mb_strimwidth("hello", 0, 0, ".."), "|", mb_strimwidth("hello", -3, 4, ""), "\n";
// error characters are written as '?' here even under ASCII, where a slice is
// handed back raw
echo bin2hex(mb_strimwidth("a\xffb", 0, 2, "")), "|", bin2hex(mb_strimwidth("á", 0, 4, "", "ASCII")), "|", bin2hex(mb_substr("á", 0, 1, "ASCII")), "\n";
try { mb_strimwidth("hello", 9, 3); } catch (ValueError $mbp) { echo $mbp->getMessage(), "\n"; }
// (@ silences php's deprecation for a negative width; PHL has no deprecation sites — §10)
try { @mb_strimwidth("hello", 0, -6); } catch (ValueError $mbp) { echo $mbp->getMessage(), "\n"; }

// mb_strwidth answers from Unicode's East Asian Width, which is where the emoji
// blocks became two columns
echo mb_strwidth("😀"), "|", mb_strwidth("日本"), "|", mb_strwidth("ａ"), "|", mb_strwidth("héllo"), "\n";
?>
--EXPECT--
á    |----á|ábááb
abc|日aba|ff2d2d
c3a12d2d2d
mb_str_pad(): Argument #3 ($pad_string) must not be empty
mb_str_pad(): Argument #4 ($pad_type) must be STR_PAD_LEFT, STR_PAD_RIGHT, or STR_PAD_BOTH
áéí|éí|á|[]
á|éí|[]|b
61ff
hello w|hell...
日本...|hello|llo..
...|..|llo
613f|3f3f|c3
mb_strimwidth(): Argument #2 ($start) is out of range
mb_strimwidth(): Argument #3 ($width) is out of range
2|4|2|5
--CLEAN--
<?php
