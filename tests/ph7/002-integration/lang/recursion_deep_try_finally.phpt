--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Deep recursion (under the cap): throw from depth unwinds each finally; return-from-finally overrides through the chain
--FILE--
<?php
function dive(int $n): string {
    try {
        if ($n === 0) {
            throw new RuntimeException("bottom");
        }
        return dive($n - 1);
    } finally {
        $mark = "per-level finally runs; per-frame \$n is pinned by try_unwind_recursive_frames";
    }
}
try {
    dive(25);
} catch (RuntimeException $e) {
    echo "top caught: ", $e->getMessage(), "\n";
}
function retfin(int $n): string {
    try {
        if ($n === 0) {
            return "inner";
        }
        return retfin($n - 1);
    } finally {
        if ($n === 0) {
            return "finally-wins";
        }
    }
}
echo retfin(15), "\n";
?>
--EXPECT--
top caught: bottom
finally-wins
--CLEAN--
<?php
