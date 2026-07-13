--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_key_first/array_key_last return first/last key, NULL for empty, pointer untouched
--FILE--
<?php
// int and string keys, insertion order
echo array_key_first([3 => "a", "x" => "b"]), "\n";
echo array_key_last([3 => "a", "x" => "b"]), "\n";
// list form
echo array_key_first([10, 20, 30]), "\n";
echo array_key_last([10, 20, 30]), "\n";
// canonicalized keys keep their integer identity
$akfl = [true => "t", -7 => "neg"];
var_export(array_key_first($akfl));
echo "\n";
var_export(array_key_last($akfl));
echo "\n";
// empty array -> NULL
var_export(array_key_first([]));
echo "\n";
var_export(array_key_last([]));
echo "\n";
// internal pointer is left untouched (unlike reset()/end())
$akfl2 = [1, 2, 3];
next($akfl2);
array_key_first($akfl2);
array_key_last($akfl2);
echo current($akfl2), "\n";
// argument validation matches php
try { array_key_first(); } catch (\Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
try { array_key_last("x"); } catch (\Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
?>
--EXPECT--
3
x
0
2
1
-7
NULL
NULL
2
ArgumentCountError: array_key_first() expects exactly 1 argument, 0 given
TypeError: array_key_last(): Argument #1 ($array) must be of type array, string given
--CLEAN--
<?php
