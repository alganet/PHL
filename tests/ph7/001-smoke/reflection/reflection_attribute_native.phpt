--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ReflectionAttribute: the class php declares, the filters, and newInstance's screens
--FILE--
<?php
#[Attribute(Attribute::TARGET_ALL | Attribute::IS_REPEATABLE)]
class RAttrMark { public function __construct(public $v = 0) {} }
#[Attribute(Attribute::TARGET_ALL)]
class RAttrSub extends RAttrMark {}
#[Attribute(Attribute::TARGET_METHOD)]
class RAttrMethodOnly {}
class RAttrPlain {}

#[RAttrMark(1)] #[RAttrSub(2)] #[RAttrMark(v: 3)]
class RAttrHost {}

$rc = new ReflectionClass('RAttrHost');
$all = $rc->getAttributes();
echo count($all), "\n";
foreach ($all as $a) {
    echo $a->getName(), ' ', $a->getTarget(), ' ', var_export($a->isRepeated(), true),
        ' ', json_encode($a->getArguments()), "\n";
}
/* name filter: case-insensitive, and IS_INSTANCEOF reaches a subclass */
echo count($rc->getAttributes('RAttrMark')), ' ',
     count($rc->getAttributes('rattrMARK')), ' ',
     count($rc->getAttributes('RAttrMark', ReflectionAttribute::IS_INSTANCEOF)), ' ',
     count($rc->getAttributes('RAttrNope')), "\n";
/* isRepeated() asks about the TARGET, not about the filtered result */
echo var_export($rc->getAttributes('RAttrSub')[0]->isRepeated(), true), "\n";

/* The class itself */
$r = new ReflectionClass('ReflectionAttribute');
echo var_export($r->isFinal(), true), ' ', json_encode($r->getInterfaceNames()), ' ',
    json_encode($r->getConstants()), "\n";
$a0 = $all[0];
echo var_export($a0 instanceof Reflector, true), ' ',
     var_export($a0 instanceof Stringable, true), ' ',
     $a0->name, ' ', json_encode(get_object_vars($a0)), "\n";
try { $x = clone $a0; } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
try { $x = new ReflectionAttribute(); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
try { $a0->getName(1); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
$m = new ReflectionMethod('ReflectionAttribute', 'getArguments');
echo $m->getNumberOfParameters(), ' ', (string)$m->getReturnType(), ' ',
    (string)(new ReflectionMethod('ReflectionAttribute', 'getTarget'))->getReturnType(), ' ',
    var_export((new ReflectionMethod('ReflectionAttribute', '__construct'))->isPrivate(), true), "\n";

/* newInstance: the four screens, then the object */
$made = $all[2]->newInstance();
echo get_class($made), ' ', $made->v, "\n";
#[RAttrPlain] #[RAttrMethodOnly] #[RAttrGhost]
class RAttrBad {}
foreach ((new ReflectionClass('RAttrBad'))->getAttributes() as $a) {
    try { $a->newInstance(); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
}
#[RAttrSub] #[RAttrSub]
class RAttrTwice {}
try {
    (new ReflectionClass('RAttrTwice'))->getAttributes()[0]->newInstance();
} catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
class RAttrNone {}
echo count((new ReflectionClass('RAttrNone'))->getAttributes()), "\n";
--EXPECT--
3
RAttrMark 1 true [1]
RAttrSub 1 false [2]
RAttrMark 1 true {"v":3}
2 2 3 0
false
false ["Stringable","Reflector"] {"IS_INSTANCEOF":2}
true true RAttrMark {"name":"RAttrMark"}
Error: Trying to clone an uncloneable object of class ReflectionAttribute
Error: Call to private ReflectionAttribute::__construct() from global scope
ArgumentCountError: ReflectionAttribute::getName() expects exactly 0 arguments, 1 given
0 array int true
RAttrMark 3
Error: Attempting to use non-attribute class "RAttrPlain" as attribute
Error: Attribute "RAttrMethodOnly" cannot target class (allowed targets: method)
Error: Attribute class "RAttrGhost" not found
Error: Attribute "RAttrSub" must not be repeated
0
