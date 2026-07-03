--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Fiber::suspend() inside a usort comparator suspends and resumes mid-sort (php full-stack-switch semantics; PHL's scoped divergence — see BYTECODE.md stage 4 — makes this twin permanent)
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
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
start returned: in-comparator
sorted:1,2,3
ret=done
--CLEAN--
<?php
