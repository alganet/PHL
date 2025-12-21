--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Test comma operator in various expressions
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Comma in assignment
$a = ($b = 5, $b + 10);
echo "assignment: $a\n";

// Comma in function call
function test($x, $y) { return $x + $y; }
$result = test($c = 2, $c * 3);
echo "function_call: $result\n";

// Comma in complex expression
$d = 10;
$e = ($d *= 2, $d + 5, $d - 3);
echo "complex: $e\n";
?>
--EXPECT--
assignment: 15
function_call: 8
complex: 17