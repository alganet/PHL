--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Array type casting for scalar values
--FILE--
<?php
// Test casting scalar values to array

// String to array
$str = "hello";
$arr_str = (array)$str;
echo "string to array: " . (is_array($arr_str) && count($arr_str) == 1 && $arr_str[0] === "hello" ? 'ok' : 'fail') . "\n";

// Int to array
$int = 42;
$arr_int = (array)$int;
echo "int to array: " . (is_array($arr_int) && count($arr_int) == 1 && $arr_int[0] === 42 ? 'ok' : 'fail') . "\n";

// Float to array
$float = 3.14;
$arr_float = (array)$float;
echo "float to array: " . (is_array($arr_float) && count($arr_float) == 1 && $arr_float[0] == 3.14 ? 'ok' : 'fail') . "\n";

// Bool to array
$bool = true;
$arr_bool = (array)$bool;
echo "bool to array: " . (is_array($arr_bool) && count($arr_bool) == 1 && $arr_bool[0] === true ? 'ok' : 'fail') . "\n";

// Null to array
$null = null;
$arr_null = (array)$null;
echo "null to array: " . (is_array($arr_null) && count($arr_null) == 0 ? 'ok' : 'fail') . "\n";
?>
--EXPECT--
string to array: ok
int to array: ok
float to array: ok
bool to array: ok
null to array: ok