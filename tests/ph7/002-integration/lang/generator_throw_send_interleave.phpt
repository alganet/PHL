--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Generator::throw() interleaved with send()/next() around a yield-in-try loop
--FILE--
<?php
function g() {
    $a = 0;
    while (true) {
        try {
            $a = yield $a;
        } catch (Exception $e) {
            $a = "caught:" . $e->getMessage();
        }
    }
}
$g = g();
echo $g->current(), "\n";              // 0
echo $g->send(10), "\n";               // 10
echo $g->throw(new Exception("x")), "\n"; // caught:x
echo $g->send(20), "\n";               // 20
echo $g->throw(new RuntimeException("y")), "\n"; // caught:y
echo $g->send(30), "\n";               // 30
?>
--EXPECT--
0
10
caught:x
20
caught:y
30
