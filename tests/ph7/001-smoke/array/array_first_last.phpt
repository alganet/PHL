--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_first/array_last return first/last value, NULL for empty, pointer untouched
--FILE--
<?php
// value of the first / last element (keys ignored)
echo array_first([3 => "a", 7 => "b"]), "\n";
echo array_last([3 => "a", 7 => "b"]), "\n";
// list form
echo array_first([10, 20, 30]), "\n";
echo array_last([10, 20, 30]), "\n";
// empty array -> NULL
var_export(array_first([]));
echo "\n";
var_export(array_last([]));
echo "\n";
// internal pointer is left untouched (unlike reset()/end())
$a = [1, 2, 3];
next($a);
array_first($a);
array_last($a);
echo current($a), "\n";
// argument validation matches php
try { array_first(); } catch (\Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
try { array_last("x"); } catch (\Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
?>
--EXPECT--
a
b
10
30
NULL
NULL
2
ArgumentCountError: array_first() expects exactly 1 argument, 0 given
TypeError: array_last(): Argument #1 ($array) must be of type array, string given
--CLEAN--
<?php
