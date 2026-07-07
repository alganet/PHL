--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Repeated deep re-suspend from inside a nested try runs the finally once and completes (php-exact; BYTECODE.md stage 4 — the parked try wrapper is not freed on re-suspend)
--FILE--
<?php
function worker() {
    try {
        for ($i = 0; $i < 5; $i++) {
            Fiber::suspend("y$i");
        }
    } finally {
        echo "worker-finally\n";
    }
    return "wdone";
}
function driver() { return worker(); }
$f = new Fiber('driver');
$v = $f->start();
while ($f->isSuspended()) { $v = $f->resume(); }
echo "ret=", $f->getReturn(), "\n";
?>
--EXPECT--
worker-finally
ret=wdone
--CLEAN--
<?php
