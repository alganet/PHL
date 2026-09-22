--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
what the already-defined rule does NOT reject: the same name one function boundary away
--FILE--
<?php
// a closure DECLARED inside a function carries its own label set, so the enclosing
// function's identical label is not a redeclaration
function lrbOuter() {
    seen:
    $c = function () { seen: return "in"; };
    return $c() . "-out";
}
echo lrbOuter(), "\n";
// a trait method and the using class's own method are two functions
trait LrbT { function m() { seen: return "t"; } }
class LrbCls {
    use LrbT;
    function n() { seen: return "n"; }
    static function s() { seen: return "s"; }
}
$o = new LrbCls();
echo $o->m(), $o->n(), LrbCls::s(), "\n";
// file scope is its own scope, beside every function in the file
seen:
echo "top\n";
// labels are case-sensitive, so these are two different names
SEEN:
seen2:
echo "case\n";
// a label inside a switch case and one after the switch, in different functions
function lrbSwitch($x) { switch ($x) { case 1: seen: return "sw"; } return "no"; }
function lrbAfter() { goto seen; echo "unreachable;"; seen: return "af"; }
echo lrbSwitch(1), lrbAfter(), "\n";
?>
--EXPECT--
in-out
tns
top
case
swaf
--CLEAN--
<?php
