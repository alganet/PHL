--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Every reflector refuses `clone` and `serialize()`, as php's do
--FILE--
<?php
#[Attribute] class RStMark {}
#[RStMark] class RStHost { const K = 1; public $p = 1; public function m(?int $a, int|string $b) {} }
enum RStEnum: int { case A = 1; }
function rStFn(?int $a) {}

$gen = (function () { yield 1; })();
/* the reference lives in its own scope: the smoke runner shares one interpreter
 * and a top-level `$r = &$a[0]` does not survive into it */
function rStRef() { $a = [1]; $r = &$a[0]; return ReflectionReference::fromArrayElement($a, 0); }
$rm = new ReflectionMethod('RStHost', 'm');

$objs = [
    new ReflectionClass('RStHost'),
    new ReflectionObject(new RStHost),
    $rm,
    new ReflectionFunction('rStFn'),
    $rm->getParameters()[0],
    $rm->getParameters()[0]->getType(),
    $rm->getParameters()[1]->getType(),
    new ReflectionProperty('RStHost', 'p'),
    new ReflectionClassConstant('RStHost', 'K'),
    (new ReflectionClass('RStHost'))->getAttributes()[0],
    new ReflectionEnum('RStEnum'),
    new ReflectionEnumBackedCase('RStEnum', 'A'),
    new ReflectionConstant('PHP_EOL'),
    new ReflectionExtension('Core'),
    new ReflectionGenerator($gen),
    rStRef(),
];
foreach ($objs as $o) {
    $n = get_class($o);
    try { $c = clone $o; echo "$n: CLONED\n"; } catch (Throwable $e) { echo $e->getMessage(), "\n"; }
    try { serialize($o); echo "$n: SERIALIZED\n"; } catch (Throwable $e) { echo $e->getMessage(), "\n"; }
}
/* The two that php DOES let through */
echo strlen(serialize(new ReflectionException('x'))) > 0 ? "exception ok\n" : "exception no\n";
echo serialize(new Reflection()), "\n";
--EXPECT--
Trying to clone an uncloneable object of class ReflectionClass
Serialization of 'ReflectionClass' is not allowed
Trying to clone an uncloneable object of class ReflectionObject
Serialization of 'ReflectionObject' is not allowed
Trying to clone an uncloneable object of class ReflectionMethod
Serialization of 'ReflectionMethod' is not allowed
Trying to clone an uncloneable object of class ReflectionFunction
Serialization of 'ReflectionFunction' is not allowed
Trying to clone an uncloneable object of class ReflectionParameter
Serialization of 'ReflectionParameter' is not allowed
Trying to clone an uncloneable object of class ReflectionNamedType
Serialization of 'ReflectionNamedType' is not allowed
Trying to clone an uncloneable object of class ReflectionUnionType
Serialization of 'ReflectionUnionType' is not allowed
Trying to clone an uncloneable object of class ReflectionProperty
Serialization of 'ReflectionProperty' is not allowed
Trying to clone an uncloneable object of class ReflectionClassConstant
Serialization of 'ReflectionClassConstant' is not allowed
Trying to clone an uncloneable object of class ReflectionAttribute
Serialization of 'ReflectionAttribute' is not allowed
Trying to clone an uncloneable object of class ReflectionEnum
Serialization of 'ReflectionEnum' is not allowed
Trying to clone an uncloneable object of class ReflectionEnumBackedCase
Serialization of 'ReflectionEnumBackedCase' is not allowed
Trying to clone an uncloneable object of class ReflectionConstant
Serialization of 'ReflectionConstant' is not allowed
Trying to clone an uncloneable object of class ReflectionExtension
Serialization of 'ReflectionExtension' is not allowed
Trying to clone an uncloneable object of class ReflectionGenerator
Serialization of 'ReflectionGenerator' is not allowed
Trying to clone an uncloneable object of class ReflectionReference
Serialization of 'ReflectionReference' is not allowed
exception ok
O:10:"Reflection":0:{}
