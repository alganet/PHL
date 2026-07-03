--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
KNOWN DIVERGENCE (BYTECODE.md stage 4): Fiber::suspend() inside a C-callback (usort comparator) — PHL silently discards the suspension, corrupting the sort and completing the fiber; php suspends fine (full native-stack switch). Stage 4 turns PHL's side into a loud catchable FiberError; this twin pins today's behavior until then. The _zend twin documents php and is PERMANENT (scoped divergence — PHL cannot suspend across a native frame without coroutine stacks).
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
$v = $f->start();
echo "start returned: ", ($v === null ? "NULL" : $v), "\n";
while ($f->isSuspended()) {
    $f->resume();
}
echo "ret=", $f->getReturn(), "\n";
?>
--EXPECT--
sorted:2,3,1
start returned: done
ret=done
--CLEAN--
<?php
