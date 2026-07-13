--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ReflectionMethod: modifiers, invoke, declaring class, prototype
--FILE--
<?php
class ReflMethBase {
    public function speak($w) { return 'base:' . $w; }
    public function shared() { return 'shared'; }
}
interface ReflMethIface { public function contract(); }
class ReflMethKid extends ReflMethBase implements ReflMethIface {
    public function speak($w) { return 'kid:' . $w; }
    public function contract() { return 'done'; }
    protected function hidden($x) { return 'hid:' . $x; }
    private function secret() { return 'sec'; }
    public static function make($v = 'M') { return 'made:' . $v; }
    final public function locked() {}
    public function __construct() {}
}

$rm = new ReflectionMethod('ReflMethKid', 'speak');
echo $rm->name, ' @ ', $rm->class, "\n";
echo $rm->getDeclaringClass()->name, "\n";
echo $rm->invoke(new ReflMethKid(), 'hi'), "\n";
echo $rm->getModifiers(), ':', implode(',', Reflection::getModifierNames($rm->getModifiers())), "\n";
echo $rm->isPublic() ? 'pub' : 'notpub', "\n";
echo $rm->isConstructor() ? 'ctor' : 'not-ctor', "\n";

$rs = new ReflectionMethod('ReflMethKid', 'shared');
echo $rs->getDeclaringClass()->name, "\n";

$rh = new ReflectionMethod('ReflMethKid', 'hidden');
echo $rh->isProtected() ? 'prot' : 'not-prot', "\n";
echo $rh->invoke(new ReflMethKid(), 'X'), "\n";
$rsec = new ReflectionMethod('ReflMethKid', 'secret');
echo $rsec->isPrivate() ? 'priv' : 'not-priv', "\n";
echo $rsec->invoke(new ReflMethKid()), "\n";

$rst = new ReflectionMethod('ReflMethKid', 'make');
echo $rst->isStatic() ? 'static' : 'not-static', "\n";
echo $rst->invoke(null), "\n";
echo $rst->invokeArgs(null, array('Z')), "\n";

$rl = new ReflectionMethod('ReflMethKid', 'locked');
echo $rl->isFinal() ? 'final' : 'not-final', "\n";

echo $rm->hasPrototype() ? 'has-proto' : 'no-proto', "\n";
echo $rm->getPrototype()->class, "\n";
$rc = new ReflectionMethod('ReflMethKid', 'contract');
echo $rc->getPrototype()->class, "\n";
$rb = new ReflectionMethod('ReflMethBase', 'speak');
echo $rb->hasPrototype() ? 'has-proto' : 'no-proto', "\n";
try {
    $rb->getPrototype();
} catch (ReflectionException $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}

$cfmn = ReflectionMethod::createFromMethodName('ReflMethKid::speak');
echo $cfmn->name, ' @ ', $cfmn->class, "\n";

try {
    $rm->invoke(null, 'x');
} catch (ReflectionException $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}
try {
    new ReflectionMethod('ReflMethKid', 'nosuch');
} catch (ReflectionException $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}
$cm = $rm->getClosure(new ReflMethKid());
echo $cm('go'), "\n";
$cs = $rst->getClosure();
echo $cs('Q'), "\n";
try {
    $rm->getClosure();
} catch (ValueError $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}
?>
--EXPECT--
speak @ ReflMethKid
ReflMethKid
kid:hi
1:public
pub
not-ctor
ReflMethBase
prot
hid:X
priv
sec
static
made:M
made:Z
final
has-proto
ReflMethBase
ReflMethIface
no-proto
ReflectionException: Method ReflMethBase::speak does not have a prototype
speak @ ReflMethKid
ReflectionException: Trying to invoke non static method ReflMethKid::speak() without an object
ReflectionException: Method ReflMethKid::nosuch() does not exist
kid:go
made:Q
ValueError: ReflectionMethod::getClosure(): Argument #1 ($object) cannot be null for non-static methods
