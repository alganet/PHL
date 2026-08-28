--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: mb_* multibyte functions, UTF-8 (band D)
--FILE--
<?php
$s = "héllo wörld żółć 日本語 🎉";
echo mb_strlen($s), "|", strlen($s), "\n";
echo mb_substr($s, 6, 5), "|", mb_substr($s, -3), "|", mb_substr($s, 17), "\n";
echo mb_substr($s, 0, -18), "|", mb_substr("abc", 5), "|", mb_substr("abc", 1, 100), "\n";
echo mb_strtoupper("héllo żółć"), "|", mb_strtolower("HÉLLO ŻÓŁĆ"), "\n";
echo mb_strtoupper("straße"), "\n";
echo mb_strtoupper("привет"), "|", mb_strtolower("ПРИВЕТ"), "\n";
echo mb_strtoupper("αβγσ"), "|", mb_strtolower("ΑΒΓΣ"), "\n";
echo mb_strpos($s, "wörld"), "|", var_export(mb_strpos($s, "zzz"), true), "|", mb_strpos($s, "ö"), "\n";
echo mb_stripos($s, "WÖRLD"), "|", mb_strrpos("abcabc", "b"), "|", mb_strpos("aXbXc", "X", 2), "\n";
print_r(mb_str_split("żółć"));
print_r(mb_str_split("abcdef", 2));
print_r(mb_str_split("日本語ab", 2));
echo mb_convert_case("héllo wörld foo", MB_CASE_TITLE), "|", mb_convert_case("a b", MB_CASE_UPPER), "|", mb_convert_case("A B", MB_CASE_LOWER), "\n";
echo MB_CASE_UPPER, MB_CASE_LOWER, MB_CASE_TITLE, "\n";
echo mb_internal_encoding(), "|", var_export(mb_internal_encoding("UTF-8"), true), "\n";
try { mb_internal_encoding("KLINGON"); } catch (ValueError $e) { echo $e->getMessage(), "\n"; }
var_export(mb_check_encoding("valid utf8 é", "UTF-8")); var_export(mb_check_encoding("\xFF\xFE", "UTF-8")); echo "\n";
echo mb_strwidth("日本語abc"), "\n";
echo mb_strlen("héllo", "8bit"), "\n";
try { mb_str_split("x", 0); } catch (ValueError $e) { echo $e->getMessage(), "\n"; }
?>
--EXPECT--
22|37
wörld|語 🎉|日本語 🎉
héll||bc
HÉLLO ŻÓŁĆ|héllo żółć
STRASSE
ПРИВЕТ|привет
ΑΒΓΣ|αβγς
6|false|7
6|4|3
Array
(
    [0] => ż
    [1] => ó
    [2] => ł
    [3] => ć
)
Array
(
    [0] => ab
    [1] => cd
    [2] => ef
)
Array
(
    [0] => 日本
    [1] => 語a
    [2] => b
)
Héllo Wörld Foo|A B|a b
012
UTF-8|true
mb_internal_encoding(): Argument #1 ($encoding) must be a valid encoding, "KLINGON" given
truefalse
9
6
mb_str_split(): Argument #2 ($length) must be greater than 0
--CLEAN--
<?php
unset($s);
