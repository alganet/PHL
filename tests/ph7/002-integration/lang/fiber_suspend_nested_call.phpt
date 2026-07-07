--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Fiber::suspend() from a nested userland call resumes INSIDE the callee (php-exact; BYTECODE.md stage 4 record-segment parking)
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
inner:after got=sent
outer:after r=inner-ret
main:resumed r=NULL
main:ret=outer-ret
--CLEAN--
<?php
