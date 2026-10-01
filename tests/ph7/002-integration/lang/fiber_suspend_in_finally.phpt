--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Fiber::suspend() inside a fiber's finally suspends and resumes: the finally body runs on the fiber's own native stack, so parking it needs nothing of the try machinery.
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
