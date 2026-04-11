--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed property: array with default and append
--FILE--
<?php
class TpBag {
    public array $items = [];
}
$b = new TpBag();
$b->items[] = "a";
$b->items[] = "b";
$b->items[] = "c";
echo implode(",", $b->items), "\n";
echo count($b->items), "\n";
?>
--EXPECT--
a,b,c
3
--CLEAN--
<?php
unset($b);
