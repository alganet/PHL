--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_pad(): non-array $array / non-int $length throw TypeError (PHP 8)
--FILE--
<?php
// Argument #1 ($array): value name follows php's convention (true/class-name).
foreach (["x", true, new stdClass] as $v) {
    try { array_pad($v, 3, 0); } catch (\TypeError $e) { echo $e->getMessage(), "\n"; }
}
// Argument #2 ($length): an array/object or a non-numeric string throws, where
// PHL previously padded to 0 silently.
try { array_pad([1, 2], [], 0); }          catch (\TypeError $e) { echo $e->getMessage(), "\n"; }
try { array_pad([1, 2], new stdClass, 0); } catch (\TypeError $e) { echo $e->getMessage(), "\n"; }
try { array_pad([1, 2], "12abc", 0); }     catch (\TypeError $e) { echo $e->getMessage(), "\n"; }
try { array_pad([1, 2], "", 0); }          catch (\TypeError $e) { echo $e->getMessage(), "\n"; }
// A numeric string is weak-coerced (incl. whitespace and exponent forms).
echo count(array_pad([1, 2], "5", 0)), "\n";
echo count(array_pad([1, 2], " 5 ", 0)), "\n";
echo count(array_pad([1, 2], "1e1", 0)), "\n";
echo count(array_pad([1, 2], 4, 0)), "\n";
?>
--EXPECT--
array_pad(): Argument #1 ($array) must be of type array, string given
array_pad(): Argument #1 ($array) must be of type array, true given
array_pad(): Argument #1 ($array) must be of type array, stdClass given
array_pad(): Argument #2 ($length) must be of type int, array given
array_pad(): Argument #2 ($length) must be of type int, stdClass given
array_pad(): Argument #2 ($length) must be of type int, string given
array_pad(): Argument #2 ($length) must be of type int, string given
5
5
10
4
--CLEAN--
<?php
