--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
SCOPED DIVERGENCE (BYTECODE.md stage 4, PERMANENT): Fiber::suspend() inside a C-callback (usort comparator) raises a catchable FiberError — PHL cannot suspend across a native frame without coroutine stacks. php suspends fine (full native-stack switch); its behavior is the permanent _zend twin. This twin pins PHL's loud, catchable failure (never the old silent sort corruption).
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
--FILE--
<?php
$f = new Fiber(function () {
    $a = [3, 1, 2];
    usort($a, function ($x, $y) {
        Fiber::suspend("in-comparator");
        return $x <=> $y;
    });
    echo "sorted:", implode(",", $a), "\n";
    return "done";
});
try {
    $v = $f->start();
    echo "start returned: ", ($v === null ? "NULL" : $v), "\n";
    while ($f->isSuspended()) {
        $f->resume();
    }
    echo "ret=", $f->getReturn(), "\n";
} catch (FiberError $e) {
    echo "FiberError: ", $e->getMessage(), "\n";
}
?>
--EXPECT--
FiberError: Cannot suspend across an internal call boundary
--CLEAN--
<?php
