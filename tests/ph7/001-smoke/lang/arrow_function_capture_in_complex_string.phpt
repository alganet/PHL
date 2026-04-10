--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Arrow function: captures variables inside curly-syntax interpolation
--FILE--
<?php
$arr = ['k' => 'v'];
$f = fn() => "value: {$arr['k']}";
echo $f(), "\n";

class AfCsObj { public $prop = "hi"; }
$o = new AfCsObj();
$g = fn() => "prop: {$o->prop}";
echo $g(), "\n";
?>
--EXPECT--
value: v
prop: hi
--CLEAN--
<?php
unset($arr, $f, $o, $g);
