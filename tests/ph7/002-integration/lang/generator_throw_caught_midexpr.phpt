--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Generator::throw() at a mid-expression yield ($x = yield) is caught and the abandoned expression is dropped cleanly
--FILE--
<?php
// The yield is the RHS of an assignment, so at resume the operand stack holds the
// half-built assignment. The injected throw must discard those operands (drain to
// the try base) before landing at the post-try pad.
function g() {
    $seen = "none";
    try {
        $x = (yield 1) . "!";   // mid-expression: yield feeds a concat then a store
        $seen = "x=$x";
    } catch (Exception $e) {
        $seen = "caught:" . $e->getMessage();
    }
    yield $seen;
}
$g = g();
echo $g->current(), "\n";
$r = $g->throw(new Exception("mid"));
echo "after=", $r, "\n";
?>
--EXPECT--
1
after=caught:mid
