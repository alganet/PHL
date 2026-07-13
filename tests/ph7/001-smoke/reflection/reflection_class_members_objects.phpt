--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ReflectionClass getProperty/getProperties/getMethod/getMethods/getConstructor
--FILE--
<?php
class ReflMembObj {
    public $a = 1;
    protected $b = 2;
    private $c = 3;
    public static $d = 4;
    public function __construct() {}
    public function pub() {}
    protected function prot() {}
    private static function privstat() {}
}

$rc = new ReflectionClass('ReflMembObj');
$gp = $rc->getProperty('b');
echo get_class($gp), ':', $gp->name, "\n";
try {
    $rc->getProperty('zz');
} catch (ReflectionException $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}
foreach ($rc->getProperties() as $p) { echo $p->name, ','; } echo "\n";
foreach ($rc->getProperties(ReflectionProperty::IS_PUBLIC) as $p) { echo $p->name, ','; } echo "\n";
foreach ($rc->getProperties(ReflectionProperty::IS_STATIC) as $p) { echo $p->name, ','; } echo "\n";
$gm = $rc->getMethod('PUB');
echo get_class($gm), ':', $gm->name, "\n";
try {
    $rc->getMethod('zz');
} catch (ReflectionException $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}
foreach ($rc->getMethods() as $m) { echo $m->name, ','; } echo "\n";
foreach ($rc->getMethods(ReflectionMethod::IS_STATIC) as $m) { echo $m->name, ','; } echo "\n";
echo $rc->getConstructor()->name, "\n";
$rnc = new ReflectionClass('stdClass');
echo $rnc->getConstructor() === null ? 'no-ctor' : 'ctor', "\n";

$obj = new stdClass();
$obj->dyn = 'dv';
$obj->dyn2 = 2;
$ro = new ReflectionObject($obj);
echo $ro->hasProperty('dyn') ? 'has-dyn' : 'no-dyn', "\n";
$rd = $ro->getProperty('dyn');
echo $rd->isDefault() ? 'default' : 'dynamic', ':', $rd->isDynamic() ? 'dyn' : 'not-dyn', ':', $rd->getValue($obj), "\n";
foreach ($ro->getProperties() as $p) { echo $p->name, ','; } echo "\n";
$rcPlain = new ReflectionClass('stdClass');
echo $rcPlain->hasProperty('dyn') ? 'has-dyn' : 'no-dyn', "\n";
?>
--EXPECT--
ReflectionProperty:b
ReflectionException: Property ReflMembObj::$zz does not exist
a,b,c,d,
a,d,
d,
ReflectionMethod:pub
ReflectionException: Method ReflMembObj::zz() does not exist
__construct,pub,prot,privstat,
privstat,
__construct
no-ctor
has-dyn
dynamic:dyn:dv
dyn,dyn2,
no-dyn
