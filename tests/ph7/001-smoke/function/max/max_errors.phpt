--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
max() domain errors: ArgumentCountError / ValueError / TypeError (PHP 8)
--FILE--
<?php
try { max(); } catch (\ArgumentCountError $e) { echo $e->getMessage(), "\n"; }
try { max([]); } catch (\ValueError $e) { echo $e->getMessage(), "\n"; }
foreach ([5, "x", false, new stdClass] as $v) {
    try { max($v); } catch (\TypeError $e) { echo $e->getMessage(), "\n"; }
}
// valid usage is unaffected
echo max([3, 1, 2]), " ", max(3, 1, 2), "\n";
// a `false` element mid-array must not stop iteration early
echo var_export(max([-5, false, -2]), true), "\n";
?>
--EXPECT--
max() expects at least 1 argument, 0 given
max(): Argument #1 ($value) must contain at least one element
max(): Argument #1 ($value) must be of type array, int given
max(): Argument #1 ($value) must be of type array, string given
max(): Argument #1 ($value) must be of type array, false given
max(): Argument #1 ($value) must be of type array, stdClass given
3 3
-2
--CLEAN--
<?php
