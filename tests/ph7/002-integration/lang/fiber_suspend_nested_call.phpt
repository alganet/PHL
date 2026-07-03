--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
KNOWN DIVERGENCE (BYTECODE.md stage 4): Fiber::suspend() from a nested call — PHL discards the nested frame and resumes after the call in the fiber body; php resumes inside the callee. This twin pins PHL's current behavior and is DELETED when stage 4 lands (its _zend twin then becomes the cross-engine test).
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
--FILE--
<?php
function inner() {
    echo "inner:before\n";
    $got = Fiber::suspend("from-inner");
    echo "inner:after got=", $got, "\n";
    return "inner-ret";
}
function outer() {
    echo "outer:before\n";
    $r = inner();
    echo "outer:after r=", $r, "\n";
    return "outer-ret";
}
$f = new Fiber('outer');
$v = $f->start();
echo "main:suspended v=", $v, "\n";
$r = $f->resume("sent");
echo "main:resumed r=", ($r === null ? "NULL" : $r), "\n";
echo "main:ret=", $f->getReturn(), "\n";
?>
--EXPECT--
outer:before
inner:before
main:suspended v=from-inner
outer:after r=sent
main:resumed r=outer-ret
main:ret=outer-ret
--CLEAN--
<?php
