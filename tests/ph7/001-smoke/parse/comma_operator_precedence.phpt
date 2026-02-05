--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--TEST--
Test comma operator at lowest precedence in complex expressions
--FILE--
<?php
// Test comma operator precedence (lowest, 22) in various contexts
$a = 1;
$b = 2;
$c = 3;

// Comma in assignment with arithmetic operations
$result1 = ($a + 1, $b * 2, $c - 1);
var_dump($result1); // Should be 2 (last expression value)

// Comma with function calls and operators
$result2 = (strlen('hello'), $a + $b, $c * 2);
var_dump($result2); // Should be 6

// Comma in conditional expression
$result3 = $a > 0 ? ($b++, $c + 1) : $c;
var_dump($result3); // Should be 4
var_dump($b); // Should be 3 (incremented)
?>
--EXPECT--
int(2)
int(6)
int(4)
int(3)
--CLEAN--
<?php
unset($a, $b, $c, $result1, $result2, $result3);
