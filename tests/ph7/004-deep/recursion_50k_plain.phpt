--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
50k-deep linear recursion completes on the heap (iterative executor; BYTECODE.md stage 3)
--DESCRIPTION--
Runs at the stock uncapped host default (BYTECODE.md stage 5; the deep tier no
longer needs PHL_MAX_RECURSION). Before the trampoline this depth overflowed the
native stack at ~7.7k frames; now PHP call frames are heap records and depth is
memory-bound. phl-only: this depth trips the php oracle's stack / xdebug
max_nesting_level (256), so it cannot be asserted cross-engine.
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip phl-only deep-recursion probe: depth exceeds the php oracle stack / xdebug nesting limit'; ?>
--FILE--
<?php
function down(int $n, int $acc): int {
    if ($n === 0) {
        return $acc;
    }
    return down($n - 1, $acc + 1);
}
echo "depth=", down(50000, 0), "\n";
// fib-shaped (tree) recursion for call-shape variety: depth 20, 2^21-ish calls
function fib(int $n): int {
    return $n < 2 ? $n : fib($n - 1) + fib($n - 2);
}
echo "fib(20)=", fib(20), "\n";
?>
--EXPECT--
depth=50000
fib(20)=6765
--CLEAN--
<?php
