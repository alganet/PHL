--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Intersection and DNF types are enforced on properties and return types
--FILE--
<?php
interface PrA {}
interface PrB {}
class PrAB implements PrA, PrB {}
class PrBox {
    public PrA&PrB $both;
    public int|(PrA&PrB) $either = 1;
}
function pr_make(): PrA&PrB { return new PrAB; }
$b = new PrBox;
$b->both = pr_make();
echo get_class($b->both), "\n";
echo $b->either, "\n";          // initial int
$b->either = new PrAB;          // DNF object branch
echo get_class($b->either), "\n";
try {
    $b->both = new class implements PrA {};
} catch (\TypeError $e) {
    echo "prop rejected\n";
}
?>
--EXPECT--
PrAB
1
PrAB
prop rejected
--CLEAN--
<?php
