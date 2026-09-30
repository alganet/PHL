--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A closure or arrow-fn at STATEMENT position is an expression statement
--DESCRIPTION--
php compiles `function () {…};` and `fn (…) => …;` at statement position as
ordinary expression statements — the closure is built and discarded — and 176
files across the vendor trees write one. PHL sent the bare `function` keyword to
PH7_CompileFunction, which demands a NAME (`syntax error, unexpected token "(",
expecting "("`), and `fn` was not a statement keyword at all (`Unexpected keyword
'fn'`). The `static` forms already worked, which is what made the gap look
narrower than it was; the `(` where a named function has its name is what tells a
closure from a declaration.
The refusals that go with this rule are in
002-integration/parse/closure_literal_not_dereferencable.phpt — they need a
process of their own.
--FILE--
<?php
$ces_log = [];

function () { $GLOBALS['ces_log'][] = 'never run'; };
fn ($a) => $a;
static function () { $GLOBALS['ces_log'][] = 'never run either'; };
static fn () => 1;
function &() { $GLOBALS['ces_log'][] = 'nor this'; };

echo "five statements compiled, none of them ran: ", count($ces_log), "\n";

// The named declaration beside them still declares.
function ces_named() { return 'named'; }
echo ces_named(), "\n";

// And the same literals still work where they are USED.
$ces_f = function () { return 'assigned'; };
echo $ces_f(), "\n";
$ces_g = fn ($n) => $n * 2;
echo $ces_g(21), "\n";
echo (function () { return 'parenthesised IIFE'; })(), "\n";
echo array_map(fn ($n) => $n + 1, [1, 2, 3])[2], "\n";
call_user_func(function () { echo "passed as an argument\n"; });
?>
--EXPECT--
five statements compiled, none of them ran: 0
named
assigned
42
parenthesised IIFE
4
passed as an argument
--CLEAN--
<?php
unset($ces_log, $ces_f, $ces_g);
