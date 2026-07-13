--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ReflectionProperty: visibility bypass, statics, readonly, initialization
--FILE--
<?php
class ReflPropBox {
    public $pub = 'p';
    protected $prot = 7;
    private $priv = array('k' => 1);
    public static $stat = 'st';
    public readonly int $ro;
    public ?string $typed = null;
    public int $noinit;
    public function __construct() { $this->ro = 42; }
}
$o = new ReflPropBox();

$rp = new ReflectionProperty('ReflPropBox', 'prot');
echo $rp->name, ' @ ', $rp->class, "\n";
echo $rp->getValue($o), "\n";
$rp->setValue($o, 99);
echo $rp->getValue($o), ':', json_encode(get_object_vars($o)['pub']), "\n";
echo $rp->getModifiers(), ':', implode(',', Reflection::getModifierNames($rp->getModifiers())), "\n";
echo $rp->isProtected() ? 'prot' : 'x', $rp->isStatic() ? ':static' : ':inst', $rp->isReadOnly() ? ':ro' : ':rw', "\n";
echo $rp->isDefault() ? 'default' : 'dynamic', "\n";
echo $rp->getDeclaringClass()->name, "\n";

$rpriv = new ReflectionProperty('ReflPropBox', 'priv');
echo json_encode($rpriv->getValue($o)), "\n";
echo $rpriv->getRawValue($o) === $rpriv->getValue($o) ? 'raw-same' : 'raw-diff', "\n";

$rs = new ReflectionProperty('ReflPropBox', 'stat');
echo $rs->getValue(), "\n";
$rs->setValue(null, 'changed');
echo ReflPropBox::$stat, "\n";

$rr = new ReflectionProperty('ReflPropBox', 'ro');
echo $rr->isReadOnly() ? 'ro' : 'rw', "\n";
echo $rr->getValue($o), "\n";
try {
    $rr->setValue($o, 1);
} catch (Error $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}

$rn = new ReflectionProperty('ReflPropBox', 'noinit');
echo $rn->isInitialized($o) ? 'init' : 'uninit', "\n";
echo $rn->hasDefaultValue() ? 'hasdef' : 'nodef', "\n";
try {
    $rn->getValue($o);
} catch (Error $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}
$rn->setValue($o, 5);
echo $rn->isInitialized($o) ? 'init' : 'uninit', ':', $rn->getValue($o), "\n";

$rt = new ReflectionProperty('ReflPropBox', 'typed');
echo $rt->isInitialized($o) ? 'init' : 'uninit', "\n";
echo $rt->hasDefaultValue() ? 'hasdef' : 'nodef', "\n";
echo $rt->getDefaultValue() === null ? 'null-def' : 'other', "\n";
$rpu = new ReflectionProperty('ReflPropBox', 'pub');
echo $rpu->getDefaultValue(), "\n";
echo $rpu->hasType() ? 'typed' : 'untyped', "\n";
echo $rt->hasType() ? 'typed' : 'untyped', "\n";

try {
    new ReflectionProperty('ReflPropBox', 'nope');
} catch (ReflectionException $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}
try {
    new ReflectionProperty('ReflPropNoCls', 'x');
} catch (ReflectionException $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}
?>
--EXPECT--
prot @ ReflPropBox
7
99:"p"
2:protected
prot:inst:rw
default
ReflPropBox
{"k":1}
raw-same
st
changed
ro
42
Error: Cannot modify readonly property ReflPropBox::$ro
uninit
nodef
Error: Typed property ReflPropBox::$noinit must not be accessed before initialization
init:5
init
hasdef
null-def
p
untyped
typed
ReflectionException: Property ReflPropBox::$nope does not exist
ReflectionException: Class "ReflPropNoCls" does not exist
