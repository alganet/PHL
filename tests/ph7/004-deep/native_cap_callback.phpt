--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Self-recursive C->PHP callback (usort in its own comparator) hits the native-nesting fatal (BYTECODE.md stage 3)
--DESCRIPTION--
Each C->PHP callback dispatch runs a fresh native VmByteCodeExec; a comparator
that re-enters the same builtin nests native frames the trampoline does not
flatten. The native guard must fire cleanly instead of overflowing the C
stack. phl-only: engine-internal cap fatal.
--SKIPIF--
<?php if (!getenv('PHL_MAX_RECURSION')) { echo "skip needs PHL_MAX_RECURSION (run: make test-stress)"; } ?>
--FILE--
<?php
function f(int $n): void {
    if ($n === 0) { return; }
    $a = [2, 1];
    usort($a, function ($x, $y) use ($n) { f($n - 1); return $x <=> $y; });
}
f(50000);
echo "UNREACHABLE\n";
?>
--EXPECTF--
%s Error:  Maximum native nesting depth reached
--CLEAN--
<?php
