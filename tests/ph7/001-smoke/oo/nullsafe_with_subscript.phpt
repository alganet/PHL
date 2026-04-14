--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Nullsafe short-circuit spans a trailing subscript access
--FILE--
<?php
class NsfSubscriptBag { public $items = array("a", "b", "c"); }
$nsfSubscript_b = null;
$nsfSubscript_r = $nsfSubscript_b?->items[0];
echo ($nsfSubscript_r === null ? "yes" : "no"), "\n";
$nsfSubscript_c = new NsfSubscriptBag();
echo $nsfSubscript_c?->items[1], "\n";
?>
--EXPECT--
yes
b
--CLEAN--
<?php
unset($nsfSubscript_b, $nsfSubscript_c, $nsfSubscript_r);
