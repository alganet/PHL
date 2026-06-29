--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
(object) cast of an array exposes keys as dynamic properties (insertion order)
--FILE--
<?php
$oca_o = (object)["id" => 1, "name" => "x", "age" => 3];
echo $oca_o->id, " ", $oca_o->name, " ", $oca_o->age, "\n";
echo count(get_object_vars($oca_o)), "\n";
echo json_encode($oca_o), "\n";
echo implode(",", array_keys((array)$oca_o)), "\n";
// integer key becomes a string property name
$oca_n = (object)[0 => "a", 1 => "b"];
echo $oca_n->{'0'}, $oca_n->{'1'}, "\n";
?>
--EXPECT--
1 x 3
3
{"id":1,"name":"x","age":3}
id,name,age
ab
--CLEAN--
<?php
unset($oca_o, $oca_n);
