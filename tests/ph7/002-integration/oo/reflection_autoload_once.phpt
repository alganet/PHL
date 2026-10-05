--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A Reflection door asks each autoloader once, and a throwing loader's exception is php's answer
--DESCRIPTION--
Every Reflection door that takes a class NAME resolved it through a lookup that
already runs the autoloaders, then ran them all a second time on a miss: two
loaders meant four calls where php makes two.

A loader that THROWS gets one of two answers, per door. ReflectionClass,
ReflectionMethod and ReflectionEnum raise nothing of their own over a pending
exception, so the loader's is the one the caller catches; here the constructor
went on to throw "does not exist" as well, after the catch had already run in
place for the loader's, and that second exception came back uncaught. The other
doors (ReflectionProperty, ReflectionClassConstant, the enum cases,
ReflectionParameter, isSubclassOf(), implementsInterface()) throw their
ReflectionException regardless, with the loader's exception as its $previous.
ReflectionParameter's [class, method] pair said the METHOD did not exist where
php names the missing class.

Its own process: the autoloaders are global state.
--FILE--
<?php
$mode = 'quiet';
$n = 0;
spl_autoload_register(function ($c) use (&$mode, &$n) {
    $n++;
    echo "load $c\n";
    if ($mode === 'throw') {
        throw new RuntimeException("loader refused $c");
    }
});
spl_autoload_register(function ($c) use (&$n) {
    $n++;
    echo "load2 $c\n";
});
class RaoK { public $p; const C = 1; function f($a) {} }
$cases = [
    'ReflectionClass' => fn() => new ReflectionClass('RaoM1'),
    'ReflectionMethod' => fn() => new ReflectionMethod('RaoM2', 'f'),
    'createFromMethodName' => fn() => ReflectionMethod::createFromMethodName('RaoM3::f'),
    'ReflectionProperty' => fn() => new ReflectionProperty('RaoM4', 'p'),
    'ReflectionClassConstant' => fn() => new ReflectionClassConstant('RaoM5', 'C'),
    'ReflectionEnum' => fn() => new ReflectionEnum('RaoM6'),
    'ReflectionEnumUnitCase' => fn() => new ReflectionEnumUnitCase('RaoM7', 'A'),
    'ReflectionEnumBackedCase' => fn() => new ReflectionEnumBackedCase('RaoM8', 'A'),
    'ReflectionParameter' => fn() => new ReflectionParameter(['RaoM9', 'f'], 0),
    'isSubclassOf' => fn() => (new ReflectionClass('RaoK'))->isSubclassOf('RaoM10'),
    'implementsInterface' => fn() => (new ReflectionClass('RaoK'))->implementsInterface('RaoM11'),
];
foreach (['quiet', 'throw'] as $mode) {
    echo "== $mode\n";
    foreach ($cases as $label => $fn) {
        $n = 0;
        try {
            $fn();
            echo "$label: no exception\n";
        } catch (Throwable $e) {
            $p = $e->getPrevious();
            echo "$label: ", get_class($e), ': ', $e->getMessage(),
                $p ? ' <- ' . get_class($p) . ': ' . $p->getMessage() : '',
                " (loader calls: $n)\n";
        }
        echo "$label: after\n";
    }
}
echo "done\n";
--EXPECT--
== quiet
load RaoM1
load2 RaoM1
ReflectionClass: ReflectionException: Class "RaoM1" does not exist (loader calls: 2)
ReflectionClass: after
load RaoM2
load2 RaoM2
ReflectionMethod: ReflectionException: Class "RaoM2" does not exist (loader calls: 2)
ReflectionMethod: after
load RaoM3
load2 RaoM3
createFromMethodName: ReflectionException: Class "RaoM3" does not exist (loader calls: 2)
createFromMethodName: after
load RaoM4
load2 RaoM4
ReflectionProperty: ReflectionException: Class "RaoM4" does not exist (loader calls: 2)
ReflectionProperty: after
load RaoM5
load2 RaoM5
ReflectionClassConstant: ReflectionException: Class "RaoM5" does not exist (loader calls: 2)
ReflectionClassConstant: after
load RaoM6
load2 RaoM6
ReflectionEnum: ReflectionException: Class "RaoM6" does not exist (loader calls: 2)
ReflectionEnum: after
load RaoM7
load2 RaoM7
ReflectionEnumUnitCase: ReflectionException: Class "RaoM7" does not exist (loader calls: 2)
ReflectionEnumUnitCase: after
load RaoM8
load2 RaoM8
ReflectionEnumBackedCase: ReflectionException: Class "RaoM8" does not exist (loader calls: 2)
ReflectionEnumBackedCase: after
load RaoM9
load2 RaoM9
ReflectionParameter: ReflectionException: Class "RaoM9" does not exist (loader calls: 2)
ReflectionParameter: after
load RaoM10
load2 RaoM10
isSubclassOf: ReflectionException: Class "RaoM10" does not exist (loader calls: 2)
isSubclassOf: after
load RaoM11
load2 RaoM11
implementsInterface: ReflectionException: Interface "RaoM11" does not exist (loader calls: 2)
implementsInterface: after
== throw
load RaoM1
ReflectionClass: RuntimeException: loader refused RaoM1 (loader calls: 1)
ReflectionClass: after
load RaoM2
ReflectionMethod: RuntimeException: loader refused RaoM2 (loader calls: 1)
ReflectionMethod: after
load RaoM3
createFromMethodName: RuntimeException: loader refused RaoM3 (loader calls: 1)
createFromMethodName: after
load RaoM4
ReflectionProperty: ReflectionException: Class "RaoM4" does not exist <- RuntimeException: loader refused RaoM4 (loader calls: 1)
ReflectionProperty: after
load RaoM5
ReflectionClassConstant: ReflectionException: Class "RaoM5" does not exist <- RuntimeException: loader refused RaoM5 (loader calls: 1)
ReflectionClassConstant: after
load RaoM6
ReflectionEnum: RuntimeException: loader refused RaoM6 (loader calls: 1)
ReflectionEnum: after
load RaoM7
ReflectionEnumUnitCase: ReflectionException: Class "RaoM7" does not exist <- RuntimeException: loader refused RaoM7 (loader calls: 1)
ReflectionEnumUnitCase: after
load RaoM8
ReflectionEnumBackedCase: ReflectionException: Class "RaoM8" does not exist <- RuntimeException: loader refused RaoM8 (loader calls: 1)
ReflectionEnumBackedCase: after
load RaoM9
ReflectionParameter: ReflectionException: Class "RaoM9" does not exist <- RuntimeException: loader refused RaoM9 (loader calls: 1)
ReflectionParameter: after
load RaoM10
isSubclassOf: ReflectionException: Class "RaoM10" does not exist <- RuntimeException: loader refused RaoM10 (loader calls: 1)
isSubclassOf: after
load RaoM11
implementsInterface: ReflectionException: Interface "RaoM11" does not exist <- RuntimeException: loader refused RaoM11 (loader calls: 1)
implementsInterface: after
done
