--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
try/catch/finally state during unwind through recursive frames — each level's catch/finally sees its own $n (per-activation try state)
--FILE--
<?php
function dive(int $n): string {
    try {
        if ($n === 0) {
            throw new RuntimeException("bottom");
        }
        return dive($n - 1);
    } finally {
        if ($n % 8 === 0) {
            echo "unwind:", $n, "\n";
        }
    }
}
try {
    dive(25);
} catch (RuntimeException $e) {
    echo "top caught: ", $e->getMessage(), "\n";
}
function relay(int $n): string {
    try {
        if ($n === 0) {
            throw new RuntimeException("relay-bottom");
        }
        return relay($n - 1);
    } catch (RuntimeException $e) {
        if ($n % 8 === 0) {
            echo "relay:", $n, "\n";
        }
        throw $e;
    }
}
try {
    relay(25);
} catch (RuntimeException $e) {
    echo "relay top: ", $e->getMessage(), "\n";
}
--EXPECT--
unwind:0
unwind:8
unwind:16
unwind:24
top caught: bottom
relay:0
relay:8
relay:16
relay:24
relay top: relay-bottom
--CLEAN--
<?php
