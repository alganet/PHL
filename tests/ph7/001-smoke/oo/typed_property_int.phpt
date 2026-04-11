--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed property: int
--FILE--
<?php
class TpCounter {
    public int $n = 0;
}
$c = new TpCounter();
echo $c->n, "\n";
$c->n = 42;
echo $c->n, "\n";
$c->n = $c->n + 1;
echo $c->n, "\n";
?>
--EXPECT--
0
42
43
--CLEAN--
<?php
unset($c);
