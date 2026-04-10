--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Expression parsing edge cases
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test expression parsing edge cases that might trigger compile paths

// Complex nested expressions
$a = 1;
$b = 2;
$c = 3;
$result = ($a + $b) * $c / ($a - $b + $c);
var_dump($result);

// Unary operators in complex expressions
$value = -++$a;
var_dump($value);

// Ternary operator with complex conditions
$condition = ($a > 0) ? (($b < 10) ? $c : $a) : $b;
var_dump($condition);

// String concatenation with variables
$str1 = "hello";
$str2 = "world";
$combined = $str1 . " " . $str2 . " " . $a . $b . $c;
var_dump($combined);

// Array access with expressions
$array = array(1, 2, 3, 4, 5);
$index = $a + $b;
$value = $array[$index];
var_dump($value);

// Function call with complex arguments
$func_result = strlen($str1 . $str2);
var_dump($func_result);
?>
--EXPECT--
double(4.5)
int(-2)
int(3)
string(15) "hello world 223"
int(5)
int(10)
--CLEAN--
<?php
unset($a, $b, $c, $result, $value, $condition, $str1, $str2, $combined, $array, $index, $func_result);
