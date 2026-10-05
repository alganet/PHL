--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Named arguments: ReflectionClass::newInstance() on a class with no constructor counts only positional arguments, newInstanceArgs() counts every key
--FILE--
<?php
class NrniPlain {}
class NrniChild extends NrniPlain {}
class NrniCtor { function __construct(public $a = 0) {} }
$nrniCases = [
    'one name'        => fn() => (new ReflectionClass('NrniPlain'))->newInstance(a: 1),
    'two names'       => fn() => (new ReflectionClass('NrniPlain'))->newInstance(a: 1, b: 2),
    'spread key'      => fn() => (new ReflectionClass('NrniPlain'))->newInstance(...['a' => 1]),
    'spread list'     => fn() => (new ReflectionClass('NrniPlain'))->newInstance(...[1]),
    'pos then name'   => fn() => (new ReflectionClass('NrniPlain'))->newInstance(1, a: 1),
    'inherited none'  => fn() => (new ReflectionClass('NrniChild'))->newInstance(x: 1),
    'stdClass'        => fn() => (new ReflectionClass('stdClass'))->newInstance(a: 1),
    'args keyed'      => fn() => (new ReflectionClass('NrniPlain'))->newInstanceArgs(['a' => 1]),
    'args empty'      => fn() => (new ReflectionClass('NrniPlain'))->newInstanceArgs([]),
    'ctor named'      => fn() => (new ReflectionClass('NrniCtor'))->newInstance(a: 5)->a,
    'ctor unknown'    => fn() => (new ReflectionClass('NrniCtor'))->newInstance(zz: 5),
];
foreach ($nrniCases as $nrniLabel => $nrniCase) {
    try {
        $nrniR = $nrniCase();
        echo $nrniLabel, ': ', is_object($nrniR) ? get_class($nrniR) : var_export($nrniR, true), "\n";
    } catch (Throwable $e) {
        echo $nrniLabel, ': ', get_class($e), ': ', $e->getMessage(), "\n";
    }
}
?>
--EXPECT--
one name: NrniPlain
two names: NrniPlain
spread key: NrniPlain
spread list: ReflectionException: Class NrniPlain does not have a constructor, so you cannot pass any constructor arguments
pos then name: ReflectionException: Class NrniPlain does not have a constructor, so you cannot pass any constructor arguments
inherited none: NrniChild
stdClass: stdClass
args keyed: ReflectionException: Class NrniPlain does not have a constructor, so you cannot pass any constructor arguments
args empty: NrniPlain
ctor named: 5
ctor unknown: Error: Unknown named parameter $zz
