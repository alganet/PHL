--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Iterating a dynamic-property stdClass: foreach, get_object_vars, (array), json
--FILE--
<?php
$sci_o = (object)["x" => 10, "y" => 20];
$sci_o->z = 30;
foreach ($sci_o as $sci_k => $sci_v) { echo "$sci_k=$sci_v;"; }
echo "\n";
$sci_gv = get_object_vars($sci_o);
echo implode(",", array_keys($sci_gv)), " => ", implode(",", array_values($sci_gv)), "\n";
$sci_a = (array)$sci_o;
echo $sci_a["x"], $sci_a["y"], $sci_a["z"], "\n";
echo json_encode($sci_o), "\n";
?>
--EXPECT--
x=10;y=20;z=30;
x,y,z => 10,20,30
102030
{"x":10,"y":20,"z":30}
--CLEAN--
<?php
unset($sci_o, $sci_k, $sci_v, $sci_gv, $sci_a);
