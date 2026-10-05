--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Reflection invoke()/invokeArgs() and Closure::call() warn and copy for a by-ref parameter
--DESCRIPTION--
php reaches the target of ReflectionFunction::invoke()/invokeArgs(),
ReflectionMethod::invoke()/invokeArgs() and Closure::call() through
zend_call_function, which binds a by-REFERENCE parameter only to a reference.
invoke()'s and call()'s own `...$args` are by value, so every by-ref formal
warns `must be passed by reference, value given` and the callee gets a copy;
an invokeArgs() element warns unless the array holds a reference there.

PHL said nothing at any of the five, and invoke()/call() handed the callee the
caller's variable itself, so `invoke($v)` into `&$x` rewrote $v and
`(new ReflectionFunction('sort'))->invoke($a)` sorted $a. A method is named
with its declaring class, a named argument by the formal it picks, and an
error handler that throws on the warning stops the call before the callee runs.
--FILE--
<?php
function bvfRef(&$x) { $x = 'W'; }
function bvfSecond($p, &$q) { $q = 'W'; }
function bvfTail(&...$xs) { $xs[0] = 'W'; }
class BvfBase {
    public function m(&$x) { $x = 'M'; }
    private static function s(&$x) { $x = 'S'; }
}
class BvfChild extends BvfBase {}
class BvfScope {}

set_error_handler(function ($n, $m) { echo '<', $m, '>', "\n"; return true; });

$v = 1; (new ReflectionFunction('bvfRef'))->invoke($v); var_dump($v);
$v = 1; (new ReflectionFunction('bvfSecond'))->invoke(q: $v, p: 0); var_dump($v);
$v = 1; $w = 1; (new ReflectionFunction('bvfTail'))->invoke($v, $w); var_dump($v, $w);
$a = [3, 1, 2]; (new ReflectionFunction('sort'))->invoke($a); echo implode(',', $a), "\n";
$v = 1; (new ReflectionFunction(fn(&$x) => $x = 'F'))->invoke($v); var_dump($v);

$v = 1; (new ReflectionFunction('bvfRef'))->invokeArgs([$v]); var_dump($v);
$v = 1; (new ReflectionFunction('bvfRef'))->invokeArgs([&$v]); var_dump($v);
$v = 1; (new ReflectionFunction('bvfSecond'))->invokeArgs(['q' => $v, 'p' => 0]); var_dump($v);
$v = 1; (new ReflectionFunction('bvfSecond'))->invokeArgs(['q' => &$v, 'p' => 0]); var_dump($v);
$a = [3, 1, 2]; (new ReflectionFunction('sort'))->invokeArgs([$a]); echo implode(',', $a), "\n";

$v = 1; (new ReflectionMethod('BvfChild', 'm'))->invoke(new BvfChild, $v); var_dump($v);
$v = 1; (new ReflectionMethod('BvfBase', 's'))->invoke(null, $v); var_dump($v);
$v = 1; (new ReflectionMethod('BvfBase', 's'))->invokeArgs(null, [$v]); var_dump($v);
$v = 1; (new ReflectionMethod('BvfBase', 's'))->invokeArgs(null, [&$v]); var_dump($v);

$c = function ($p, &$q) { $q = 'C'; };
$v = 1; $c->call(new BvfScope, 0, $v); var_dump($v);
$v = 1; $c->call(new BvfScope, 0, q: $v); var_dump($v);
$v = 1; $c->__invoke(0, $v); var_dump($v);
$v = 1; call_user_func_array(Closure::bind(fn(&$x) => 0, null, BvfScope::class), [$v]);

echo "-- a throwing handler stops the call --\n";
set_error_handler(function ($n, $m) { throw new Exception($m); });
function bvfLoud(&$x) { echo "entered\n"; }
$v = 1;
try { (new ReflectionFunction('bvfLoud'))->invoke($v); } catch (Exception $e) { echo 'caught: ', $e->getMessage(), "\n"; }
try { (new ReflectionFunction('bvfLoud'))->invokeArgs([$v]); } catch (Exception $e) { echo 'caught: ', $e->getMessage(), "\n"; }
try { (function (&$x) { echo "entered\n"; })->call(new BvfScope, $v); } catch (Exception $e) { echo 'caught: ', preg_replace('/\{closure:[^}]*\}/', '{closure}', $e->getMessage()), "\n"; }
--EXPECTF--
<bvfRef(): Argument #1 ($x) must be passed by reference, value given>
int(1)
<bvfSecond(): Argument #2 ($q) must be passed by reference, value given>
int(1)
<bvfTail(): Argument #1 must be passed by reference, value given>
<bvfTail(): Argument #2 must be passed by reference, value given>
int(1)
int(1)
<sort(): Argument #1 ($array) must be passed by reference, value given>
3,1,2
<{closure:%s}(): Argument #1 ($x) must be passed by reference, value given>
int(1)
<bvfRef(): Argument #1 ($x) must be passed by reference, value given>
int(1)
string(1) "W"
<bvfSecond(): Argument #2 ($q) must be passed by reference, value given>
int(1)
string(1) "W"
<sort(): Argument #1 ($array) must be passed by reference, value given>
3,1,2
<BvfBase::m(): Argument #1 ($x) must be passed by reference, value given>
int(1)
<BvfBase::s(): Argument #1 ($x) must be passed by reference, value given>
int(1)
<BvfBase::s(): Argument #1 ($x) must be passed by reference, value given>
int(1)
string(1) "S"
<BvfScope::{closure:%s}(): Argument #2 ($q) must be passed by reference, value given>
int(1)
<BvfScope::{closure:%s}(): Argument #2 ($q) must be passed by reference, value given>
int(1)
string(1) "C"
<BvfScope::{closure:%s}(): Argument #1 ($x) must be passed by reference, value given>
-- a throwing handler stops the call --
caught: bvfLoud(): Argument #1 ($x) must be passed by reference, value given
caught: bvfLoud(): Argument #1 ($x) must be passed by reference, value given
caught: BvfScope::{closure}(): Argument #1 ($x) must be passed by reference, value given
