--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Appending past an occupied PHP_INT_MAX key throws a catchable Error (php semantics; PHL reports the same message as a non-catchable runtime error — divergence recorded in PLAN.md §3)
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip";
}
--FILE--
<?php
$a = [PHP_INT_MAX - 1 => 'x'];
$a[] = 'y'; // lands on PHP_INT_MAX, like php
var_export(array_keys($a));
echo "\n";
try {
    $a[] = 'z';
    echo "appended\n";
} catch (Error $e) {
    echo "caught: ", $e->getMessage(), "\n";
}
?>
--EXPECT--
array (
  0 => 9223372036854775806,
  1 => 9223372036854775807,
)
caught: Cannot add element to the array as the next element is already occupied
--CLEAN--
<?php
unset($a);
