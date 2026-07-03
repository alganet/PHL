--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
50k-deep linear recursion completes on the heap (iterative executor; BYTECODE.md stage 3)
--DESCRIPTION--
Runs under PHL_MAX_RECURSION (set by `make test-stress` for this directory).
Before the trampoline this depth overflowed the native stack at ~7.7k frames;
now PHP call frames are heap records and depth is a policy knob.
--SKIPIF--
<?php if (!getenv('PHL_MAX_RECURSION')) { echo "skip needs PHL_MAX_RECURSION (run: make test-stress)"; } ?>
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
