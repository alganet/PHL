--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ReflectionClass hierarchy: parent, interfaces, subclass checks
--FILE--
<?php
interface ReflHierA {}
interface ReflHierB extends ReflHierA {}
class ReflHierBase implements ReflHierB {}
class ReflHierKid extends ReflHierBase {}

$rc = new ReflectionClass('ReflHierKid');
$parent = $rc->getParentClass();
echo $parent->getName(), "\n";
echo $parent->getParentClass() === false ? 'no-grandparent' : 'grandparent', "\n";
$names = $rc->getInterfaceNames();
sort($names);
echo implode(',', $names), "\n";
$ifaces = $rc->getInterfaces();
ksort($ifaces);
foreach ($ifaces as $k => $v) {
    echo $k, '=', get_class($v), ':', $v->getName(), "\n";
}
echo $rc->isSubclassOf('ReflHierBase') ? 'sub-base' : 'not-sub-base', "\n";
echo $rc->isSubclassOf('ReflHierA') ? 'sub-a' : 'not-sub-a', "\n";
echo $rc->isSubclassOf('ReflHierKid') ? 'sub-self' : 'not-sub-self', "\n";
echo $rc->implementsInterface('ReflHierA') ? 'impl-a' : 'not-impl-a', "\n";
echo $rc->implementsInterface('ReflHierB') ? 'impl-b' : 'not-impl-b', "\n";
try {
    $rc->implementsInterface('ReflHierBase');
} catch (ReflectionException $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}
try {
    $rc->implementsInterface('ReflNoSuchIface');
} catch (ReflectionException $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}
$obj = new ReflHierKid();
echo $rc->isInstance($obj) ? 'inst-kid' : 'not-inst-kid', "\n";
$rb = new ReflectionClass('ReflHierBase');
echo $rb->isInstance($obj) ? 'inst-base' : 'not-inst-base', "\n";
$ra = new ReflectionClass('ReflHierA');
echo $ra->isInstance($obj) ? 'inst-a' : 'not-inst-a', "\n";
echo $ra->implementsInterface('ReflHierA') ? 'iface-impl-self' : 'iface-not-impl-self', "\n";
$rbi = new ReflectionClass('ReflHierB');
echo $rbi->implementsInterface('ReflHierA') ? 'b-impl-a' : 'b-not-impl-a', "\n";
?>
--EXPECT--
ReflHierBase
no-grandparent
ReflHierA,ReflHierB
ReflHierA=ReflectionClass:ReflHierA
ReflHierB=ReflectionClass:ReflHierB
sub-base
sub-a
not-sub-self
impl-a
impl-b
ReflectionException: ReflHierBase is not an interface
ReflectionException: Interface "ReflNoSuchIface" does not exist
inst-kid
inst-base
inst-a
iface-impl-self
b-impl-a
