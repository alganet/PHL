--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ReflectionGenerator and ReflectionFiber basics
--FILE--
<?php
function reflGen5() { $x = yield 1; yield 2; return 'done'; }
class ReflGenC { public function mg() { yield 'm'; } }
function reflGenOuter() { yield from reflGen5(); }

$g = reflGen5();
$g->current();
$rg = new ReflectionGenerator($g);
echo get_class($rg->getFunction()), ':', $rg->getFunction()->getName(), "\n";
echo $rg->getThis() === null ? 'no-this' : 'this', "\n";
echo $rg->getExecutingGenerator() === $g ? 'same' : 'other', "\n";
echo $rg->isClosed() ? 'closed' : 'open', "\n";

$o = new ReflGenC();
$mg = $o->mg();
$mg->current();
$rmg = new ReflectionGenerator($mg);
$f = $rmg->getFunction();
echo get_class($f), ':', $f->getName(), ' @ ', $f->class, "\n";
echo $rmg->getThis() === $o ? 'this-obj' : 'other', "\n";

$t = reflGen5();
$t->current(); $t->next(); $t->next();
echo $t->valid() ? 'valid' : 'invalid', "\n";
$rt = new ReflectionGenerator($t);
echo $rt->isClosed() ? 'closed' : 'open', "\n";

$og = reflGenOuter();
$og->current();
$rog = new ReflectionGenerator($og);
$eg = $rog->getExecutingGenerator();
echo $eg === $og ? 'outer' : 'inner', "\n";
echo (new ReflectionGenerator($eg))->getFunction()->getName(), "\n";

try {
    new ReflectionGenerator('x');
} catch (TypeError $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}

$fb = new Fiber(function () { Fiber::suspend(1); return 2; });
$rf = new ReflectionFiber($fb);
echo get_class($rf->getFiber()), "\n";
echo is_callable($rf->getCallable()) ? 'callable' : 'not-callable', "\n";
try {
    new ReflectionFiber(5);
} catch (TypeError $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}
?>
--EXPECT--
ReflectionFunction:reflGen5
no-this
same
open
ReflectionMethod:mg @ ReflGenC
this-obj
invalid
closed
inner
reflGen5
TypeError: ReflectionGenerator::__construct(): Argument #1 ($generator) must be of type Generator, string given
Fiber
callable
TypeError: ReflectionFiber::__construct(): Argument #1 ($fiber) must be of type Fiber, int given
