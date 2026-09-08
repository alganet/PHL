--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
str_increment and str_decrement follow php's Perl-style alphanumeric counting
--FILE--
<?php
$inc = [];
foreach (["a", "z", "Z", "0", "9", "az", "zz", "ZZ", "99", "a0", "Zz9", "Az"] as $v) {
    $inc[] = str_increment($v);
}
echo implode(" ", $inc), "\n";
$dec = [];
foreach (["b", "z", "Z", "1", "9", "ba", "zz", "b0", "Aa0", "10", "1000", "z0"] as $v) {
    $dec[] = str_decrement($v);
}
echo implode(" ", $dec), "\n";
foreach (["", "a b"] as $v) {
    try { str_increment($v); } catch (\ValueError $e) { echo $e->getMessage(), "\n"; }
}
foreach (["", "0", "a", "00", "0a"] as $v) {
    try { str_decrement($v); } catch (\ValueError $e) { echo $e->getMessage(), "\n"; }
}
?>
--EXPECT--
b aa AA 1 10 ba aaa AAA 100 a1 AAa0 Ba
a y Y 0 8 az zy a9 z9 9 999 y9
str_increment(): Argument #1 ($string) must not be empty
str_increment(): Argument #1 ($string) must be composed only of alphanumeric ASCII characters
str_decrement(): Argument #1 ($string) must not be empty
str_decrement(): Argument #1 ($string) "0" is out of decrement range
str_decrement(): Argument #1 ($string) "a" is out of decrement range
str_decrement(): Argument #1 ($string) "00" is out of decrement range
str_decrement(): Argument #1 ($string) "0a" is out of decrement range
--CLEAN--
<?php
