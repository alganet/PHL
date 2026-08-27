--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Self-recursive C->PHP callback (usort in its own comparator) hits the native-nesting fatal at the shipped default (BYTECODE.md stage 5)
--DESCRIPTION--
Each C->PHP callback dispatch runs a fresh native VmByteCodeExec; a comparator
that re-enters the same builtin nests native frames the trampoline does not
flatten. PHP call depth is unbounded by default (stage 5), so the native guard
is what fires — at its shipped default (256 host, the value proven safe on this
fattest re-entry path under ASan). phl-only: engine-internal cap fatal.
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip phl-only: engine-internal native-nesting cap, not expressible in php'; ?>
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
Error: Maximum native nesting depth reached in %s on line %d
--CLEAN--
<?php
