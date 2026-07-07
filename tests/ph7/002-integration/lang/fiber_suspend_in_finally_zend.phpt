--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Fiber::suspend() inside a finally suspends and resumes fine on php (full native-stack switch); PHL's scoped divergence (see fiber_suspend_in_finally.phpt) makes this twin permanent
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip";
}
?>
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
$v = $f->start();
echo "suspended:", var_export($v, true), "\n";
$f->resume();
echo "ret=", $f->getReturn(), "\n";
?>
--EXPECT--
try
suspended:'in-finally'
ret=done
--CLEAN--
<?php
