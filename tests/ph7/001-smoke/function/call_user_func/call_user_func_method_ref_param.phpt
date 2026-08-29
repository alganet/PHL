--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
call_user_func_array with object method that takes a reference parameter

--FILE--
<?php
class CufcRefC { public function inc(&$x){ $x += 2; } }
$c = new CufcRefC();
$a = 3;
call_user_func_array(array($c,'inc'), array(&$a));
echo $a . "\n";
?>
--EXPECT--
5
--CLEAN--
<?php
unset($c, $a);
