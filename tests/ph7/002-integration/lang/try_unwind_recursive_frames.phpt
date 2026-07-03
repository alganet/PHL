--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
KNOWN DIVERGENCE (BYTECODE.md stage 2): each recursion level pushes the SAME per-lexical-try ph7_exception, so unwind runs every catch/finally against the deepest frame ($n=0). Pinned until the activation-record rework makes try state per-activation; the _zend twin is the acceptance test.
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
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
unwind:0
unwind:0
unwind:0
unwind:0
unwind:0
unwind:0
unwind:0
unwind:0
unwind:0
unwind:0
unwind:0
unwind:0
unwind:0
unwind:0
unwind:0
unwind:0
unwind:0
unwind:0
unwind:0
unwind:0
unwind:0
unwind:0
unwind:0
unwind:0
unwind:0
top caught: bottom
relay:0
relay:0
relay:0
relay:0
relay:0
relay:0
relay:0
relay:0
relay:0
relay:0
relay:0
relay:0
relay:0
relay:0
relay:0
relay:0
relay:0
relay:0
relay:0
relay:0
relay:0
relay:0
relay:0
relay:0
relay:0
relay:0
relay top: relay-bottom
--CLEAN--
<?php
