--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
min() domain errors: ArgumentCountError / ValueError / TypeError (PHP 8)
--FILE--
<?php
try { min(); } catch (\ArgumentCountError $e) { echo $e->getMessage(), "\n"; }
try { min([]); } catch (\ValueError $e) { echo $e->getMessage(), "\n"; }
foreach ([5, "x", 1.5, true, null] as $v) {
    try { min($v); } catch (\TypeError $e) { echo $e->getMessage(), "\n"; }
}
// valid usage is unaffected
echo min([3, 1, 2]), " ", min(3, 1, 2), "\n";
// a `false` element mid-array must not stop iteration early
echo var_export(min([5, false, 2]), true), "\n";
?>
--EXPECT--
min() expects at least 1 argument, 0 given
min(): Argument #1 ($value) must contain at least one element
min(): Argument #1 ($value) must be of type array, int given
min(): Argument #1 ($value) must be of type array, string given
min(): Argument #1 ($value) must be of type array, float given
min(): Argument #1 ($value) must be of type array, true given
min(): Argument #1 ($value) must be of type array, null given
1 1
false
--CLEAN--
<?php
