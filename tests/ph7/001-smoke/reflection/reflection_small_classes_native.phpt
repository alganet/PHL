--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The standalone reflectors: Constant, Extension, ZendExtension, Reference, Generator, Fiber
--FILE--
<?php
function reflSmallSay(callable $f) {
    try { $r = $f(); return is_object($r) ? get_class($r) : var_export($r, true); }
    catch (Throwable $e) { return get_class($e) . ': ' . $e->getMessage(); }
}

// php declares these final; a subclass is a compile-time fatal, so isFinal() is
// the observable part.
foreach (['ReflectionGenerator', 'ReflectionFiber', 'ReflectionReference'] as $c) {
    echo $c, ' final=', var_export((new ReflectionClass($c))->isFinal(), true), "\n";
}

define('REFL_SMALL_K', 42);
$rc = new ReflectionConstant('REFL_SMALL_K');
echo $rc->getName(), ' ', var_export($rc->getValue(), true),
     ' short=', $rc->getShortName(), ' ns=', var_export($rc->getNamespaceName(), true),
     ' deprecated=', var_export($rc->isDeprecated(), true),
     ' ext=', var_export($rc->getExtensionName(), true), "\n";

// An ENGINE constant belongs to the synthetic Core extension; a userland one to none.
$ri = new ReflectionConstant('PHP_INT_SIZE');
echo var_export($ri->getExtensionName(), true), ' ', get_class($ri->getExtension()),
     ' file=', var_export($ri->getFileName(), true), "\n";

echo reflSmallSay(fn() => new ReflectionConstant('REFL_NO_SUCH_CONSTANT')), "\n";
echo reflSmallSay(fn() => new ReflectionExtension('Core')->getName()), "\n";
echo reflSmallSay(fn() => new ReflectionExtension('nosuchextension')), "\n";
echo reflSmallSay(fn() => new ReflectionZendExtension('anything')), "\n";

// The declared `string $name` coerces exactly as php's does -- a float names a
// missing extension rather than being refused outright.
echo reflSmallSay(fn() => new ReflectionExtension(1.5)), "\n";
echo reflSmallSay(fn() => new ReflectionConstant(5)), "\n";

// ReflectionReference answers only for an element that IS a reference, and its
// constructor is private.
$arr = [1, 2]; $alias = &$arr[0]; $plain = [1, 2];
echo var_export(is_string(ReflectionReference::fromArrayElement($arr, 0)->getId()), true), "\n";
echo var_export(ReflectionReference::fromArrayElement($arr, 1), true), "\n";
echo var_export(ReflectionReference::fromArrayElement($plain, 0), true), "\n";
echo var_export(ReflectionReference::fromArrayElement($arr, 0)->getId()
             === ReflectionReference::fromArrayElement($arr, 0)->getId(), true), "\n";
echo reflSmallSay(fn() => ReflectionReference::fromArrayElement('notanarray', 0)), "\n";
echo reflSmallSay(fn() => ReflectionReference::fromArrayElement($arr, [])), "\n";

// ReflectionGenerator reads the coroutine's own frame.
function reflSmallGen() { yield 1; yield 2; }
class ReflSmallHost { public $tag = 'H'; public function m() { yield 7; } }
$g = reflSmallGen(); $g->current();
$rg = new ReflectionGenerator($g);
echo get_class($rg->getFunction()), ' ', $rg->getFunction()->getName(),
     ' this=', var_export($rg->getThis(), true),
     ' closed=', var_export($rg->isClosed(), true),
     ' self=', var_export($rg->getExecutingGenerator() === $g, true), "\n";
foreach ($g as $x) {}
echo 'closed=', var_export($rg->isClosed(), true), "\n";

$h = new ReflSmallHost; $gm = $h->m(); $gm->current();
$rm = new ReflectionGenerator($gm);
echo get_class($rm->getFunction()), ' ', $rm->getFunction()->getName(),
     '::', $rm->getFunction()->class, ' this=', $rm->getThis()->tag, "\n";

echo reflSmallSay(fn() => new ReflectionGenerator(1)), "\n";
echo reflSmallSay(fn() => new ReflectionFiber('x')), "\n";

$fib = new Fiber(function () { Fiber::suspend(1); });
$fib->start();
$rf = new ReflectionFiber($fib);
echo var_export($rf->getFiber() === $fib, true), ' ',
     var_export(is_callable($rf->getCallable()), true), "\n";
--EXPECT--
ReflectionGenerator final=true
ReflectionFiber final=true
ReflectionReference final=true
REFL_SMALL_K 42 short=REFL_SMALL_K ns='' deprecated=false ext=false
'Core' ReflectionExtension file=false
ReflectionException: Constant "REFL_NO_SUCH_CONSTANT" does not exist
'Core'
ReflectionException: Extension "nosuchextension" does not exist
ReflectionException: Zend Extension "anything" does not exist
ReflectionException: Extension "1.5" does not exist
ReflectionException: Constant "5" does not exist
true
NULL
NULL
true
TypeError: ReflectionReference::fromArrayElement(): Argument #1 ($array) must be of type array, string given
TypeError: ReflectionReference::fromArrayElement(): Argument #2 ($key) must be of type string|int, array given
ReflectionFunction reflSmallGen this=NULL closed=false self=true
closed=true
ReflectionMethod m::ReflSmallHost this=H
TypeError: ReflectionGenerator::__construct(): Argument #1 ($generator) must be of type Generator, int given
TypeError: ReflectionFiber::__construct(): Argument #1 ($fiber) must be of type Fiber, string given
true true
