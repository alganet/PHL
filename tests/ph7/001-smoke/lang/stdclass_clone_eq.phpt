--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
clone and == on a dynamic-property stdClass (deep-copy independence; by-name ==)
--FILE--
<?php
$sce_a = (object)["x" => 1, "y" => 2];
$sce_a->z = 3;
$sce_b = clone $sce_a;
echo $sce_b->x, $sce_b->y, $sce_b->z, "\n";
$sce_b->x = 99;                 // clone is independent
echo $sce_a->x, ",", $sce_b->x, "\n";
// == compares by name (order-independent), and by full property set
var_export((object)["x"=>1,"y"=>2] == (object)["y"=>2,"x"=>1]);
echo "\n";
var_export((object)["x"=>1] == (object)["x"=>1,"y"=>9]);
echo "\n";
var_export($sce_a == clone $sce_a);
echo "\n";
?>
--EXPECT--
123
1,99
true
false
true
--CLEAN--
<?php
unset($sce_a, $sce_b);
