--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
call_user_func_array() and Fiber::start() warn when a by-ref parameter gets a value
--DESCRIPTION--
Two sites hand a by-REFERENCE parameter something they cannot alias, and php says so
before the call: `call_user_func_array()`, whose argument-array element is a plain
VALUE unless the array itself holds a reference (`[&$v]`), and `Fiber::start()`,
whose own `...$args` are by value whatever the body declares.

PHL's VALUES were already php's at both — cufa aliases the array's own element, which
for a literal `[$v]` is a copy — so the only thing missing was the one diagnostic
that tells a caller its out-param will not come back. The callee is named the way
every other argument diagnostic names it: a method with its class, a closure with
php's `{closure:file:line}`, a builtin off its declared signature; a by-ref VARIADIC
element takes php's no-name wording, since many values share the one formal.
--FILE--
<?php
function bvgRef(&$x) { $x = 'W'; }
function bvgSecond($p, &$q) { $q = 'W'; }
function bvgTail(&...$xs) {}
class BvgC {
    public function m(&$x) { $x = 'M'; }
    public static function s(&$x) { $x = 'S'; }
    public function __invoke(&$x) { $x = 'V'; }
}

set_error_handler(function ($n, $m) { echo '<', $m, '>', "\n"; return true; });

echo "-- a plain element is a VALUE\n";
$a = 'a';
call_user_func_array('bvgRef', [$a]);
var_dump($a);

echo "-- a REFERENCE element binds, silently\n";
$b = 'b';
$args = [&$b];
call_user_func_array('bvgRef', $args);
var_dump($b);

echo "-- the by-ref position is the one reported\n";
call_user_func_array('bvgSecond', ['x', 'y']);

echo "-- a by-ref VARIADIC tail has no parameter name\n";
call_user_func_array('bvgTail', ['p', 'q']);

echo "-- a static method, both spellings\n";
call_user_func_array('BvgC::s', [$a]);
call_user_func_array(['BvgC', 's'], [$a]);

echo "-- an instance method and an __invoke object\n";
call_user_func_array([new BvgC, 'm'], [$a]);
call_user_func_array(new BvgC, [$a]);

echo "-- a Closure over a method pair keeps the class\n";
call_user_func_array(Closure::fromCallable([new BvgC, 'm']), [$a]);

echo "-- a BUILTIN names its signature's parameter\n";
$arr = [3, 1, 2];
call_user_func_array('sort', [$arr]);
var_dump($arr);

echo "-- a by-VALUE parameter says nothing\n";
call_user_func_array('strlen', ['abc']);

echo "-- Fiber::start() is by value whatever the body declares\n";
$c = 'c';
$fiber = new Fiber('bvgRef');
$fiber->start($c);
var_dump($c);

restore_error_handler();
?>
--EXPECT--
-- a plain element is a VALUE
<bvgRef(): Argument #1 ($x) must be passed by reference, value given>
string(1) "a"
-- a REFERENCE element binds, silently
string(1) "W"
-- the by-ref position is the one reported
<bvgSecond(): Argument #2 ($q) must be passed by reference, value given>
-- a by-ref VARIADIC tail has no parameter name
<bvgTail(): Argument #1 must be passed by reference, value given>
<bvgTail(): Argument #2 must be passed by reference, value given>
-- a static method, both spellings
<BvgC::s(): Argument #1 ($x) must be passed by reference, value given>
<BvgC::s(): Argument #1 ($x) must be passed by reference, value given>
-- an instance method and an __invoke object
<BvgC::m(): Argument #1 ($x) must be passed by reference, value given>
<BvgC::__invoke(): Argument #1 ($x) must be passed by reference, value given>
-- a Closure over a method pair keeps the class
<BvgC::m(): Argument #1 ($x) must be passed by reference, value given>
-- a BUILTIN names its signature's parameter
<sort(): Argument #1 ($array) must be passed by reference, value given>
array(3) {
  [0]=>
  int(3)
  [1]=>
  int(1)
  [2]=>
  int(2)
}
-- a by-VALUE parameter says nothing
-- Fiber::start() is by value whatever the body declares
<bvgRef(): Argument #1 ($x) must be passed by reference, value given>
string(1) "c"
--CLEAN--
<?php
