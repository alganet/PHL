--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Which of the calls the ENGINE makes for you bind with the calling file's strict_types
--DESCRIPTION--
php reads strict_types off the frame that MADE the call, not off the file the callee
lives in and not off the file that is lexically nearest. An internal function has no
strict_types of its own, so every call an internal function makes for you -- array_map's
callback, a comparator, an output-buffer handler -- binds WEAKLY however strict the file
that reached the builtin is. `call_user_func` and `call_user_func_array` are the two
exceptions php's compiler rewrites into ordinary calls, so they carry the caller's mode.

PHL had the rule and two doors on the wrong side of it. Reflection's `invoke`,
`invokeArgs` and `newInstance` family went through the class-method dispatcher, which
never set the internal-caller latch, so they bound STRICTLY: PHPUnit's mock builder
reaches every original constructor exactly that way (`getConstructor()->invokeArgs()`
from a file that declares strict_types=1), and monolog's TelegramBotHandler test died
on a TypeError php does not raise. The AUTOLOADER went the other way -- it was dispatched
as an internal callback and bound weakly, where php's autoload call is the one engine
call that is NOT weak: it reads the strict_types of the code whose class reference
triggered it, and names that file and line in its own diagnostic. An autoloader
declaring `bool` was handed `true` instead of the class name.
--FILE--
<?php declare(strict_types=1);
/* Every door the ENGINE uses to reach userland, called from a file that declares
 * strict_types=1, handing an int to a `bool` parameter of the userland callable.
 * "weak" means the door coerced it; "STRICT" means it refused. */
function probe(string $label, callable $fn): void {
    try { $fn(); echo str_pad($label, 34), " weak\n"; }
    catch (TypeError $e) { echo str_pad($label, 34), " STRICT\n"; }
}
function b1(bool $b) {}
function b2(bool $x, bool $y): int { return 0; }
class K {
    public function m(bool $b) {}
    public static function s(bool $b) {}
    public function __invoke(bool $b) {}
}
class Ctor { public function __construct(bool $b) {} }
#[Attribute] class At { public function __construct(public bool $b) {} }
#[At(1)] class Target {}
$k = new K();

echo "== php's two FORWARDS carry the caller's mode ==\n";
probe('call_user_func', fn() => call_user_func('b1', 1));
probe('call_user_func_array', fn() => call_user_func_array('b1', [1]));
probe('call_user_func on a method', fn() => call_user_func([$k, 'm'], 1));

echo "== a call the caller SPELLS is the caller's ==\n";
probe('direct function call', fn() => b1(1));
probe('direct method call', fn() => $k->m(1));
probe('direct static call', fn() => K::s(1));
probe('direct new', fn() => new Ctor(1));
probe('__invoke by call syntax', fn() => $k(1));
probe('a bound closure, called', fn() => (function (bool $b) {})->bindTo($k, K::class)(1));
probe('an attribute instantiated', fn() => (new ReflectionClass('Target'))->getAttributes()[0]->newInstance());

echo "== an INTERNAL function reaching for a callback binds weakly ==\n";
probe('array_map', fn() => array_map('b1', [1]));
probe('array_map, two arrays', fn() => array_map('b2', [1], [2]));
probe('array_map on a static name', fn() => array_map('K::s', [1]));
probe('array_map on an __invoke', fn() => array_map($k, [1]));
probe('array_filter', fn() => array_filter([1], 'b1'));
probe('array_reduce', fn() => array_reduce([1], function ($c, bool $i) { return $c; }));
probe('array_walk', function () { $a = [1]; array_walk($a, function (bool $v, $key) {}); });
probe('usort', function () { $a = [1, 2]; usort($a, 'b2'); });
probe('uasort', function () { $a = ['x' => 1, 'y' => 2]; uasort($a, 'b2'); });
probe('uksort', function () { $a = [1 => 'x', 2 => 'y']; uksort($a, 'b2'); });
probe('array_udiff', fn() => array_udiff([1], [2], 'b2'));
probe('iterator_apply', fn() => iterator_apply(new ArrayIterator([1]), function (bool $b) { return false; }, [1]));
probe('an output-buffer handler', function () { ob_start(function (bool $buf) { return ''; }); echo 'x'; ob_end_clean(); });
probe('an error handler', function () { set_error_handler(function (bool $n, $s) { return true; }); @trigger_error('x', E_USER_NOTICE); restore_error_handler(); });
probe('Closure::call', fn() => (function (bool $b) {})->call($k, 1));
probe('Fiber::start', function () { $f = new Fiber(function (bool $b) {}); $f->start(1); });

echo "== Reflection invokes from an internal frame, so weakly too ==\n";
probe('ReflectionFunction::invoke', fn() => (new ReflectionFunction('b1'))->invoke(1));
probe('ReflectionFunction::invokeArgs', fn() => (new ReflectionFunction('b1'))->invokeArgs([1]));
probe('ReflectionMethod::invoke', fn() => (new ReflectionMethod('K', 'm'))->invoke($k, 1));
probe('ReflectionMethod::invokeArgs', fn() => (new ReflectionMethod('K', 'm'))->invokeArgs($k, [1]));
probe('ReflectionMethod on a static', fn() => (new ReflectionMethod('K', 's'))->invoke(null, 1));
probe('ReflectionClass::newInstance', fn() => (new ReflectionClass('Ctor'))->newInstance(1));
probe('ReflectionClass::newInstanceArgs', fn() => (new ReflectionClass('Ctor'))->newInstanceArgs([1]));

echo "== the AUTOLOADER is the one engine call that is not weak ==\n";
/* `new` rather than class_exists(): the two ask the engine the same question, and a
 * loaded xdebug answers the class_exists() one from a frame of its own. */
spl_autoload_register(function (bool $c) { echo "the autoloader ran with "; var_dump($c); });
try { new NoSuchClassAtAllEither(); } catch (TypeError $e) { echo "new: TypeError\n"; }
echo "done\n";
?>
--EXPECT--
== php's two FORWARDS carry the caller's mode ==
call_user_func                     STRICT
call_user_func_array               STRICT
call_user_func on a method         STRICT
== a call the caller SPELLS is the caller's ==
direct function call               STRICT
direct method call                 STRICT
direct static call                 STRICT
direct new                         STRICT
__invoke by call syntax            STRICT
a bound closure, called            STRICT
an attribute instantiated          STRICT
== an INTERNAL function reaching for a callback binds weakly ==
array_map                          weak
array_map, two arrays              weak
array_map on a static name         weak
array_map on an __invoke           weak
array_filter                       weak
array_reduce                       weak
array_walk                         weak
usort                              weak
uasort                             weak
uksort                             weak
array_udiff                        weak
iterator_apply                     weak
an output-buffer handler           weak
an error handler                   weak
Closure::call                      weak
Fiber::start                       weak
== Reflection invokes from an internal frame, so weakly too ==
ReflectionFunction::invoke         weak
ReflectionFunction::invokeArgs     weak
ReflectionMethod::invoke           weak
ReflectionMethod::invokeArgs       weak
ReflectionMethod on a static       weak
ReflectionClass::newInstance       weak
ReflectionClass::newInstanceArgs   weak
== the AUTOLOADER is the one engine call that is not weak ==
new: TypeError
done
