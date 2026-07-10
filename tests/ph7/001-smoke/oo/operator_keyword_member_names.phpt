--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Alpha operator-keywords (clone/new/and/or/xor/instanceof) as member names
--FILE--
<?php
// PHP lets any keyword — including the alpha-stream operators — be a method or
// property name reached through -> / ?-> / :: .
class OkmC {
    public $and = "prop";
    function clone() { return "m:clone"; }
    function and() { return "m:and"; }
    function or() { return "m:or"; }
    function xor() { return "m:xor"; }
    function instanceof() { return "m:instanceof"; }
    static function new() { return "s:new"; }
}
$o = new OkmC();
echo $o->clone(), "\n";
echo $o->and(), "\n";
echo $o->or(), "\n";
echo $o->xor(), "\n";
echo $o->instanceof(), "\n";
echo OkmC::new(), "\n";
echo $o?->clone(), "\n";
echo $o->and, "\n";           // property named 'and'
?>
--EXPECT--
m:clone
m:and
m:or
m:xor
m:instanceof
s:new
m:clone
prop
--CLEAN--
<?php
