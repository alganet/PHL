--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Many deep re-suspend cycles from a nested call complete without leaking recursion depth (php-exact; BYTECODE.md stage 4 re-suspend accounting)
--FILE--
<?php
function worker() {
    $sum = 0;
    for ($i = 0; $i < 40; $i++) {
        $sum += (int) Fiber::suspend($i);
    }
    return $sum;
}
function driver() { return worker(); }
$f = new Fiber('driver');
$v = $f->start();
$cycles = 0;
while ($f->isSuspended()) { $v = $f->resume(1); $cycles++; }
echo "cycles=$cycles ret=", $f->getReturn(), "\n";
?>
--EXPECT--
cycles=40 ret=40
--CLEAN--
<?php
