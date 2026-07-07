--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A fiber that suspends repeatedly from inside the same nested call resumes in place each cycle (php-exact; BYTECODE.md stage 4 segment re-park)
--FILE--
<?php
function worker() {
    $sum = 0;
    for ($i = 1; $i <= 3; $i++) {
        $got = Fiber::suspend("yield-$i");
        $sum += (int) $got;
        echo "worker got=$got sum=$sum\n";
    }
    return "sum=$sum";
}
function driver() {
    return worker();
}
$f = new Fiber('driver');
$v = $f->start();
while ($f->isSuspended()) {
    echo "main sees $v\n";
    $v = $f->resume(10);
}
echo "ret=", $f->getReturn(), "\n";
?>
--EXPECT--
main sees yield-1
worker got=10 sum=10
main sees yield-2
worker got=10 sum=20
main sees yield-3
worker got=10 sum=30
ret=sum=30
--CLEAN--
<?php
