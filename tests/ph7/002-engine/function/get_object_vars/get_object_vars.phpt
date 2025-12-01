--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
get_object_vars returns public properties of an object
--FILE--
<?php
class C { public $a = 1; protected $b = 2; private $c = 3; }
$o = new C();
$vars = get_object_vars($o);
if (array_key_exists('a', $vars) && !array_key_exists('b', $vars) && !array_key_exists('c', $vars)) echo "OK\n"; else echo "FAIL\n";
?>
--EXPECT--
OK

--CLEAN--
<?php
unset($o, $vars);
?>
