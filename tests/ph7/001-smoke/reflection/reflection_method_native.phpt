--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ReflectionMethod invokes past visibility, resolves prototypes, and reflects closures
--FILE--
<?php
abstract class ReflMBase {
    public function over($x = 1) { return 'base' . $x; }
    private function hidden() { return 'h'; }
    abstract public function must();
    public static function stat() { return 'S'; }
}
interface ReflMI { public function fromIface(); }
class ReflMKid extends ReflMBase implements ReflMI {
    final public function over($x = 1) { return 'kid' . $x; }
    public function must() {}
    public function fromIface() {}
    public function own() { return 'o'; }
}

$m = new ReflectionMethod('ReflMKid', 'over');
echo $m->class, '::', $m->getName(), ' declared=', $m->getDeclaringClass()->getName(),
     ' modifiers=', $m->getModifiers(),
     ' [', implode(',', Reflection::getModifierNames($m->getModifiers())), "]\n";
echo 'proto over=', $m->hasPrototype() ? $m->getPrototype()->class : '-',
     ' iface=', (new ReflectionMethod('ReflMKid', 'fromIface'))->getPrototype()->class,
     ' own=', var_export((new ReflectionMethod('ReflMKid', 'own'))->hasPrototype(), true), "\n";
try { (new ReflectionMethod('ReflMKid', 'own'))->getPrototype(); }
catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }

// Invocation ignores visibility (php 8.1+) but not the receiver's type.
echo $m->invoke(new ReflMKid(), 7), ' ', $m->invokeArgs(new ReflMKid(), [8]), "\n";
echo (new ReflectionMethod('ReflMBase', 'hidden'))->invoke(new ReflMKid()), "\n";
echo (new ReflectionMethod('ReflMBase', 'stat'))->invoke(null), ' ',
     ((new ReflectionMethod('ReflMBase', 'stat'))->getClosure())(), "\n";
try { $m->invoke(null, 1); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
try { $m->invoke(new stdClass(), 1); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
try { $m->getClosure(null); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
echo ($m->getClosure(new ReflMKid()))(4), "\n";

// The one-argument "C::m" form is deprecated in favour of the static factory,
// which splits the name itself and so does not re-trip the notice.
$prev = error_reporting(E_ALL & ~E_DEPRECATED);
echo (new ReflectionMethod('ReflMKid::over'))->getName(), "\n";
error_reporting($prev);
echo ReflectionMethod::createFromMethodName('ReflMKid::own')->class, ' ',
     (new ReflectionMethod('ReflMKid', 'OVER'))->getName(), "\n";
try { new ReflectionMethod('ReflMKid', '__construct'); }
catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }

// A Closure reflects as a function: its captures, its scope, its parameters.
$captured = 5;
$clo = function (int $q, $w = 2) use ($captured) { return $q + $captured; };
$rc = new ReflectionFunction($clo);
echo 'anon=', var_export($rc->isAnonymous(), true),
     ' closure=', var_export($rc->isClosure(), true),
     ' static=', var_export($rc->isStatic(), true),
     ' used=', json_encode($rc->getClosureUsedVariables()),
     ' params=', json_encode(array_map(fn($p) => $p->getName(), $rc->getParameters())),
     ' invoke=', $rc->invoke(1), "\n";
echo 'staticFn=', var_export((new ReflectionFunction(static fn($z) => $z))->isStatic(), true), "\n";

$bound = Closure::bind(function () { return 1; }, new ReflMKid(), ReflMKid::class);
$rb = new ReflectionFunction($bound);
echo 'this=', get_class($rb->getClosureThis()), ' scope=', $rb->getClosureScopeClass()->getName(), "\n";

// A first-class callable is a Closure over a method, and reflects as one.
$fcc = (new ReflMKid())->over(...);
echo 'fcc=', json_encode(array_map(fn($p) => $p->getName(), (new ReflectionFunction($fcc))->getParameters())),
     ' call=', (new ReflectionFunction($fcc))->invoke(3), "\n";

try { new ReflectionMethod('ReflMKid', 'nope'); }
catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
try { new ReflectionMethod('ReflMNope', 'x'); }
catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
?>
--EXPECT--
ReflMKid::over declared=ReflMKid modifiers=33 [final,public]
proto over=ReflMBase iface=ReflMI own=false
ReflectionException: Method ReflMKid::own does not have a prototype
kid7 kid8
h
S S
ReflectionException: Trying to invoke non static method ReflMKid::over() without an object
ReflectionException: Given object is not an instance of the class this method was declared in
ValueError: ReflectionMethod::getClosure(): Argument #1 ($object) cannot be null for non-static methods
kid4
over
ReflMKid over
ReflectionException: Method ReflMKid::__construct() does not exist
anon=true closure=true static=false used={"captured":5} params=["q","w"] invoke=6
staticFn=true
this=ReflMKid scope=ReflMKid
fcc=["x"] call=kid3
ReflectionException: Method ReflMKid::nope() does not exist
ReflectionException: Class "ReflMNope" does not exist
