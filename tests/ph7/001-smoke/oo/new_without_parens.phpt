--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHP 8.4 new NwpC()->member without wrapping parens: (new NwpC())->...
--FILE--
<?php
class NwpC {
    const X = 9;
    public static $s = 5;
    public $p = 42;
    function m() { return [10, 20]; }
    function self() { return $this; }
}
class NwpD implements ArrayAccess {
    function offsetExists($o): bool { return true; }
    function offsetGet($o): mixed { return "g$o"; }
    function offsetSet($o, $v): void {}
    function offsetUnset($o): void {}
}
// method call directly on a new expression
echo new NwpC()->m()[1], "\n";
// property access
echo new NwpC()->p, "\n";
// static const / static prop via an instance
echo new NwpC()::X, "\n";
echo new NwpC()::$s, "\n";
// chained calls
echo new NwpC()->self()->p, "\n";
// subscript directly on a new expression
echo new NwpD()[5], "\n";
// dynamic class name
$c = "NwpC";
echo new $c()->m()[0], "\n";
// still binds tighter than instanceof
echo (new NwpC() instanceof NwpC) ? "yes" : "no", "\n";
?>
--EXPECT--
20
42
9
5
42
g5
10
yes
--CLEAN--
<?php
