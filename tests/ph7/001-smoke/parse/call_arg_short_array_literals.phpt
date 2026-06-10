--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Short-array literals as call arguments stay separate arguments
--DESCRIPTION--
Regression for the argument splitter (ExprProcessFuncArguments): a short-array
literal [...] consumes its own ']', so its '[' had no balancing node and the
nesting counter stayed positive, collapsing every subsequent argument into the
last literal. This affected any call, not just array builtins. Each parameter
must receive its own literal.
--FILE--
<?php
function call_arg_lit_three($a, $b, $c) {
    echo count($a), count($b), count($c), ' ', $a[0], $b[0], $c[0], "\n";
}
call_arg_lit_three([1], [2], [3]);

// A literal followed by an array subscript argument: the subscript's '['/']'
// are separate balancing nodes and must keep working alongside the fix.
$arr = [9, 8];
function call_arg_lit_subscript($x, $y) {
    echo $x[0], ',', $y, "\n";
}
call_arg_lit_subscript([1], $arr[0]);

// Nested literal as one argument is one argument.
function call_arg_lit_nested($x, $y) {
    echo count($x), ',', $x[1][0], ',', $y, "\n";
}
call_arg_lit_nested([1, [7]], 5);
?>
--EXPECT--
111 123
1,9
2,7,5
--CLEAN--
<?php
unset($arr);
