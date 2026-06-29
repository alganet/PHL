--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Dynamic property writes on a stdClass: create, read, reassign, isset, unset
--FILE--
<?php
$sdw_o = new stdClass;
$sdw_o->a = 1;
$sdw_o->b = "two";
echo $sdw_o->a, $sdw_o->b, "\n";
$sdw_o->a = 99;            // reassign keeps the same property
echo $sdw_o->a, "\n";
echo json_encode($sdw_o), "\n";
echo isset($sdw_o->a) ? "set" : "no", "\n";
unset($sdw_o->a);
echo isset($sdw_o->a) ? "set" : "no", "\n";
?>
--EXPECT--
1two
99
{"a":99,"b":"two"}
set
no
--CLEAN--
<?php
unset($sdw_o);
