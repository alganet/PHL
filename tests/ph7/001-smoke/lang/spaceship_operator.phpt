--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Spaceship operator <=> (three-way comparison)
--FILE--
<?php
// Integer comparisons
echo "int_lt: " . (1 <=> 2) . "\n";
echo "int_gt: " . (2 <=> 1) . "\n";
echo "int_eq: " . (1 <=> 1) . "\n";

// Float comparisons
echo "float_lt: " . (1.5 <=> 2.5) . "\n";
echo "float_gt: " . (2.5 <=> 1.5) . "\n";
echo "float_eq: " . (1.5 <=> 1.5) . "\n";

// String comparisons
echo "str_lt: " . ("apple" <=> "banana") . "\n";
echo "str_gt: " . ("banana" <=> "apple") . "\n";
echo "str_eq: " . ("hello" <=> "hello") . "\n";

// Mixed type (int vs numeric string - type juggling)
echo "mixed_eq: " . (10 <=> "10") . "\n";
echo "mixed_lt: " . (1 <=> "2") . "\n";
echo "mixed_gt: " . (2 <=> "1") . "\n";

// Null comparisons
echo "null_vs_zero: " . (null <=> 0) . "\n";
echo "null_vs_one: " . (null <=> 1) . "\n";
echo "null_vs_null: " . (null <=> null) . "\n";

// Boolean comparisons
echo "bool_true_false: " . (true <=> false) . "\n";
echo "bool_false_true: " . (false <=> true) . "\n";
echo "bool_same: " . (true <=> true) . "\n";

// Array comparisons
echo "arr_fewer: " . ([1] <=> [1,2]) . "\n";
echo "arr_more: " . ([1,2] <=> [1]) . "\n";
echo "arr_eq: " . ([1,2] <=> [1,2]) . "\n";

// NaN comparisons
echo "nan_vs_int: " . (NAN <=> 1) . "\n";
echo "int_vs_nan: " . (1 <=> NAN) . "\n";
echo "nan_vs_nan: " . (NAN <=> NAN) . "\n";

// Use in expression context (result as integer)
$result = (3 <=> 5) + 10;
echo "expr: " . $result . "\n";
?>
--EXPECT--
int_lt: -1
int_gt: 1
int_eq: 0
float_lt: -1
float_gt: 1
float_eq: 0
str_lt: -1
str_gt: 1
str_eq: 0
mixed_eq: 0
mixed_lt: -1
mixed_gt: 1
null_vs_zero: 0
null_vs_one: -1
null_vs_null: 0
bool_true_false: 1
bool_false_true: -1
bool_same: 0
arr_fewer: -1
arr_more: 1
arr_eq: 0
nan_vs_int: 1
int_vs_nan: 1
nan_vs_nan: 1
expr: 9
