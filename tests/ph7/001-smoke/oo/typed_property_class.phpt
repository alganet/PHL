--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed property: user class type
--FILE--
<?php
class TpPoint {
    public int $x = 0;
    public int $y = 0;
}
class TpBox {
    public TpPoint $tl;
    public TpPoint $br;
}
$b = new TpBox();
$b->tl = new TpPoint();
$b->br = new TpPoint();
$b->tl->x = 1; $b->tl->y = 2;
$b->br->x = 10; $b->br->y = 20;
echo $b->tl->x, ",", $b->tl->y, " -> ", $b->br->x, ",", $b->br->y, "\n";
?>
--EXPECT--
1,2 -> 10,20
--CLEAN--
<?php
unset($b);
