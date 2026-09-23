--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A parenthesised property callee makes a first-class callable of its VALUE
--FILE--
<?php
class FpvcHolder {
    public $cb;
    public $name = 'strtoupper';
    public $pair;
    public array $arr = [];
    public static $scb;
    public static $sname = 'strrev';
    public function m($x) { return "m($x)"; }
}
class FpvcTarget {
    public function pub($x) { return "pub($x)"; }
}
$o = new FpvcHolder();
$o->cb = fn($x) => "cb($x)";
$o->pair = [new FpvcTarget(), 'pub'];
$o->arr['k'] = fn() => 'arr';
FpvcHolder::$scb = fn() => 'scb';

/* `($o->p)(...)` is the property's VALUE made first-class -- php's variable invocation --
 * not a method named by the property. The un-parenthesised `$o->m(...)` stays a method. */
$f = ($o->cb)(...);
echo "closure-prop:", $f(1), "\n";
echo "identity:", var_export($f === $o->cb, true), "\n";
echo "string-prop:", (($o->name)(...))('ab'), "\n";
echo "array-prop:", (($o->pair)(...))(2), "\n";
echo "element-prop:", (($o->arr['k'])(...))(), "\n";
echo "static-prop:", ((FpvcHolder::$scb)(...))(), "\n";
echo "static-string-prop:", ((FpvcHolder::$sname)(...))('abc'), "\n";
echo "method:", (($o->m(...))(3)), "\n";
echo "is-closure:", var_export(($o->name)(...) instanceof Closure, true), "\n";

/* The same value invoked directly still works, with and without the ellipsis. */
echo "direct:", ($o->cb)(4), "\n";
echo "static-direct:", (FpvcHolder::$scb)(), "\n";

/* A dynamic method name keeps the method reading. */
$mn = 'm';
echo "dynamic-method:", (($o->$mn(...))(5)), "\n";
?>
--EXPECT--
closure-prop:cb(1)
identity:true
string-prop:AB
array-prop:pub(2)
element-prop:arr
static-prop:scb
static-string-prop:cba
method:m(3)
is-closure:true
direct:cb(4)
static-direct:scb
dynamic-method:m(5)
--CLEAN--
<?php
