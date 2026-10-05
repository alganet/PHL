--TEST--
forward_static_call() refuses a caller with no class scope; forward_static_call_array() does not
--FILE--
<?php
// php asks the frame IMMEDIATELY above the call: a userland frame answers its
// own scope (a method's class, a closure's creation-site or bound scope, the
// scope an include or eval runs in), an internal caller answers its own class,
// and a fiber runs a C body from an entry frame of its own that has none. The
// refusal comes after the callback screen, and _array() never asks.
function f(...$a) { return 'f(' . implode(',', $a) . ')'; }
function t($label, $c) {
    try {
        $r = json_encode($c());
    } catch (Throwable $e) {
        $r = get_class($e) . ': ' . $e->getMessage();
    }
    echo $label, ': ', $r, "\n";
}
class A {
    static function direct() { return forward_static_call('f', 1); }
    static function viaCuf() { return call_user_func('forward_static_call', 'f', 2); }
    static function viaCufSpread() { return call_user_func(...['forward_static_call', 'f', 3]); }
    static function viaMap() { return array_map('forward_static_call', ['f']); }
    static function closure() { return (function () { return forward_static_call('f', 4); })(); }
    static function inEval() { return eval('return forward_static_call("f", 5);'); }
    static function fiberBody() { $fb = new Fiber(function () { return forward_static_call('f', 6); }); $fb->start(); return $fb->getReturn(); }
    static function fiberNative() { $fb = new Fiber('forward_static_call'); $fb->start('f', 7); return $fb->getReturn(); }
    static function trace() { return array_map('forward_static_call', ['f']); }
}
trait T { static function t() { return forward_static_call('f', 8); } }
class U { use T; }
t('method', fn() => A::direct());
t('folded call_user_func in a method', fn() => A::viaCuf());
t('unfolded call_user_func in a method', fn() => A::viaCufSpread());
t('array_map in a method', fn() => A::viaMap());
t('closure in a method', fn() => A::closure());
t('eval in a method', fn() => A::inEval());
t('fiber closure in a method', fn() => A::fiberBody());
t('fiber native body in a method', fn() => A::fiberNative());
t('trait method', fn() => U::t());
t('top level', fn() => forward_static_call('f', 9));
t('top level, _array', fn() => forward_static_call_array('f', [10]));
t('top level, bad callback first', fn() => forward_static_call('nope'));
t('top level, first-class callable', fn() => forward_static_call(...)('f', 11));
t('top level, ReflectionFunction::invoke', fn() => (new ReflectionFunction('forward_static_call'))->invoke('f', 12));
t('closure bound to a class', fn() => Closure::bind(function () { return forward_static_call('f', 13); }, null, A::class)());
t('closure bound to no class', fn() => Closure::bind(function () { return forward_static_call('f', 14); }, null, null)());
t('Closure::call', fn() => (function () { return forward_static_call('f', 15); })->call(new A));
t('generator at top level', function () { foreach ((function () { yield forward_static_call('f', 16); })() as $v) { return $v; } });
try {
    A::trace();
} catch (Error $e) {
    $tr = explode("\n", $e->getTraceAsString());
    echo $e->getLine(), "\n", str_replace(__FILE__, 'FILE', $tr[0] . "\n" . $tr[1]), "\n";
}
--EXPECT--
method: "f(1)"
folded call_user_func in a method: "f(2)"
unfolded call_user_func in a method: Error: Cannot call forward_static_call() when no class scope is active
array_map in a method: Error: Cannot call forward_static_call() when no class scope is active
closure in a method: "f(4)"
eval in a method: "f(5)"
fiber closure in a method: "f(6)"
fiber native body in a method: Error: Cannot call forward_static_call() when no class scope is active
trait method: "f(8)"
top level: Error: Cannot call forward_static_call() when no class scope is active
top level, _array: "f(10)"
top level, bad callback first: TypeError: forward_static_call(): Argument #1 ($callback) must be a valid callback, function "nope" not found or invalid function name
top level, first-class callable: Error: Cannot call forward_static_call() when no class scope is active
top level, ReflectionFunction::invoke: "f(12)"
closure bound to a class: "f(13)"
closure bound to no class: Error: Cannot call forward_static_call() when no class scope is active
Closure::call: "f(15)"
generator at top level: Error: Cannot call forward_static_call() when no class scope is active
25
#0 [internal function]: forward_static_call()
#1 FILE(25): array_map()
