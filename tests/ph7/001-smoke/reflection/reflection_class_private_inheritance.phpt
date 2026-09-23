--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A base class's PRIVATE members are off the subclass's listing but a method lookup still finds one
--DESCRIPTION--
php gives two different answers about an inherited private member, and the
difference is deliberate. getMethods()/getProperties()/getReflectionConstants()
report only what the subclass's own surface exposes, so a parent's private is
absent from all three; hasMethod()/getMethod() read the function table, where
the parent's private method really does sit, so they find it and report the
PARENT as its declaring class. Properties and constants have no such second
answer: a parent's private is invisible to hasProperty()/getProperty() and to
hasConstant()/getConstant() too.
--FILE--
<?php
class ReflPrivBase {
    public $basePub = 1;
    private $basePriv = 2;
    public const B_PUB = 'p';
    private const B_PRIV = 'q';
    public function basePubM() {}
    private function basePrivM() {}
}
class ReflPrivKid extends ReflPrivBase {
    public $kidPub = 3;
    private $kidPriv = 4;
    private function kidPrivM() {}
}

$r = new ReflectionClass('ReflPrivKid');

// The LISTING: the kid's own privates are in, the base's are not.
echo 'methods: ', implode(',', array_map(fn($m) => $m->getName(), $r->getMethods())), "\n";
echo 'props: ',   implode(',', array_map(fn($p) => $p->getName(), $r->getProperties())), "\n";
echo 'consts: ',  implode(',', array_keys($r->getConstants())), "\n";

// The LOOKUP: a method disagrees with the listing, and the others do not.
echo 'hasMethod basePrivM=', var_export($r->hasMethod('basePrivM'), true),
     ' declaredBy=', $r->getMethod('basePrivM')->getDeclaringClass()->getName(), "\n";
echo 'hasProperty basePriv=', var_export($r->hasProperty('basePriv'), true), "\n";
// (getConstant('B_PRIV') is left out: php 8.5 deprecates reading a constant
// that does not exist, and the notice is not what this test is about.)
echo 'hasConstant B_PRIV=', var_export($r->hasConstant('B_PRIV'), true), "\n";
try { $r->getProperty('basePriv'); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }

// A private CONSTRUCTOR is inherited the same way: not listed, but it is still
// the constructor `new` would reach, so the subclass is not instantiable.
class ReflPrivCtor { private function __construct() {} }
class ReflPrivCtorKid extends ReflPrivCtor {}
$rk = new ReflectionClass('ReflPrivCtorKid');
echo 'ctor=', $rk->getConstructor()->getDeclaringClass()->getName(),
     ' instantiable=', var_export($rk->isInstantiable(), true),
     ' listed=', var_export(array_map(fn($m) => $m->getName(), $rk->getMethods()), true), "\n";
?>
--EXPECT--
methods: kidPrivM,basePubM
props: kidPub,kidPriv,basePub
consts: B_PUB
hasMethod basePrivM=true declaredBy=ReflPrivBase
hasProperty basePriv=false
hasConstant B_PRIV=false
ReflectionException: Property ReflPrivKid::$basePriv does not exist
ctor=ReflPrivCtor instantiable=false listed=array (
)
