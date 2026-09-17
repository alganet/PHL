--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: Unexpected token errors to cover uncovered lines in parse.c ExprVerifyNodes
--SKIPIF--
<?php
// php ABORTS at the first compile error; PHL keeps compiling and reports every one
// it finds (then "Error count limit reached" past 15). That is a deliberate engine
// difference, not a fidelity gap -- reporting the whole batch is more useful for an
// embedded engine -- so the two can never agree on this output. The FIRST error's
// text is what has to match php, and that is asserted by the single-error tests in
// this directory; this test exists to pin PHL's continuation behavior.
if (function_exists('zend_version')) { echo 'skip php aborts at the first compile error; PHL reports all (engine design)'; }
?>
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
%AParse error:%AUnmatched '}'%AParse error:%AUnmatched ']'%AParse error:%Asyntax error, unexpected token ";"%A
--CLEAN--
<?php
unset($a, $array, $func, $x);
