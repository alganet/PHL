--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Throw from 50k deep unwinds 50k per-frame finallys to a top-level catch (BYTECODE.md stage 3)
--DESCRIPTION--
Every level opens its own try/finally (per-activation exception state, stage
2b); the throw at the bottom must run each level's finally exactly once on
the way out and reach the catch with the counter intact.
--SKIPIF--
<?php if (!getenv('PHL_MAX_RECURSION')) { echo "skip needs PHL_MAX_RECURSION (run: make test-stress)"; } ?>
--FILE--
<?php
$finallys = 0;
function diveAndThrow(int $n): void {
    global $finallys;
    try {
        if ($n === 0) {
            throw new RuntimeException("from the bottom");
        }
        diveAndThrow($n - 1);
    } finally {
        $finallys++;
    }
}
try {
    diveAndThrow(50000);
    echo "UNREACHABLE\n";
} catch (RuntimeException $e) {
    echo "caught: ", $e->getMessage(), "\n";
}
echo "finallys=", $finallys, "\n";
?>
--EXPECT--
caught: from the bottom
finallys=50001
--CLEAN--
<?php
unset($finallys);
