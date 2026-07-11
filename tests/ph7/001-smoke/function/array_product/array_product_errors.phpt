--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_product(): non-array TypeError, empty array is 1 (PHP 8)
--FILE--
<?php
foreach (["x", 5, 1.5, true, null, new stdClass] as $v) {
    try { array_product($v); } catch (\TypeError $e) { echo $e->getMessage(), "\n"; }
}
// The product of an empty array is the multiplicative identity 1.
var_dump(array_product([]));
var_dump(array_product([2, 3, 4]));
var_dump(array_product([1.5, 2]));
?>
--EXPECT--
array_product(): Argument #1 ($array) must be of type array, string given
array_product(): Argument #1 ($array) must be of type array, int given
array_product(): Argument #1 ($array) must be of type array, float given
array_product(): Argument #1 ($array) must be of type array, true given
array_product(): Argument #1 ($array) must be of type array, null given
array_product(): Argument #1 ($array) must be of type array, stdClass given
int(1)
int(24)
float(3)
--CLEAN--
<?php
