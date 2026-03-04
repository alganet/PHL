--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: Unexpected token errors to cover uncovered lines in parse.c ExprVerifyNodes
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test cases that trigger unexpected token errors in ExprVerifyNodes
// Covers lines like 432 for unexpected '}', 408 for unexpected ']', etc.

// Unexpected closing brace
$a = 1; }

// Unexpected closing bracket
$array = [1, 2]; ]

// Unexpected closing parenthesis
$func = function() { return 1; }; )

// Nested unexpected tokens
if (true) {
    $x = 10;
} }

// Another case
foreach ([1,2] as $val) {
    echo $val;
} ]

?>
--EXPECTF--
%s %d Error:  Syntax error: Unexpected token '}'
%s %d Error:  Syntax error: Unexpected token ']'
%s %d Error:  Syntax error: Unexpected token ';'
%s %d Error:  Syntax error: Unexpected token '}'
Compile error
--CLEAN--
<?php
unset($a, $array, $func, $x);
