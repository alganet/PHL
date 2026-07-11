--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_filter(): non-array $array throws TypeError (PHP 8)
--FILE--
<?php
foreach (["not an array", 5, 1.5, true, false, null, new stdClass] as $v) {
    try { array_filter($v); } catch (\TypeError $e) { echo $e->getMessage(), "\n"; }
}
?>
--EXPECT--
array_filter(): Argument #1 ($array) must be of type array, string given
array_filter(): Argument #1 ($array) must be of type array, int given
array_filter(): Argument #1 ($array) must be of type array, float given
array_filter(): Argument #1 ($array) must be of type array, true given
array_filter(): Argument #1 ($array) must be of type array, false given
array_filter(): Argument #1 ($array) must be of type array, null given
array_filter(): Argument #1 ($array) must be of type array, stdClass given
--CLEAN--
<?php
