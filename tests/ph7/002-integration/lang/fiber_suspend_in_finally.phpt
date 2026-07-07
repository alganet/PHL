--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
SCOPED DIVERGENCE (BYTECODE.md stage 4, PLAN.md §3.9): Fiber::suspend() inside a fiber's catch/finally raises a catchable FiberError — fibers use the legacy (non-inline) try machinery, so catch/finally run on a nested native activation PH7_SUSPEND cannot cross. php suspends fine (permanent _zend twin).
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
--FILE--
<?php
$f = new Fiber(function () {
    try {
        echo "try\n";
    } finally {
        Fiber::suspend("in-finally");
    }
    return "done";
});
try {
    $v = $f->start();
    echo "suspended:", var_export($v, true), "\n";
    $f->resume();
    echo "ret=", $f->getReturn(), "\n";
} catch (FiberError $e) {
    echo "FiberError: ", $e->getMessage(), "\n";
}
?>
--EXPECT--
try
FiberError: Cannot suspend across an internal call boundary
--CLEAN--
<?php
