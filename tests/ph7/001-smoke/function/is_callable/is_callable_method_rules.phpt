--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
is_callable(): array-callable indices, visibility, static form, abstract, __call
--FILE--
<?php
function icmrShow(array $answers): void
{
    $out = [];
    foreach ($answers as $label => $v) { $out[] = $label . '=' . ($v ? 'T' : 'F'); }
    echo implode(' ', $out), "\n";
}

class IcmrBase
{
    private function priv() { return 'priv'; }
    protected function prot() { return 'prot'; }
    public function pub() { return 'pub'; }
    public static function st() { return 'st'; }

    public function fromInstance(): array
    {
        return [
            '[this,priv]' => is_callable([$this, 'priv']),
            'Base::priv' => is_callable('IcmrBase::priv'),
            'Base::pub' => is_callable('IcmrBase::pub'),
            '[Base,priv]' => is_callable(['IcmrBase', 'priv']),
        ];
    }
    public static function fromStatic(): array
    {
        return [
            'Base::priv' => is_callable('IcmrBase::priv'),
            'Base::pub' => is_callable('IcmrBase::pub'),
            'Base::st' => is_callable('IcmrBase::st'),
        ];
    }
}
class IcmrChild extends IcmrBase
{
    public function fromChild(): array
    {
        return [
            '[this,prot]' => is_callable([$this, 'prot']),
            '[this,priv]' => is_callable([$this, 'priv']),
            'Base::prot' => is_callable('IcmrBase::prot'),
        ];
    }
}
class IcmrOther
{
    public function fromOutside(): array
    {
        return [
            'Base::pub' => is_callable('IcmrBase::pub'),
            'Base::st' => is_callable('IcmrBase::st'),
            '[obj,priv]' => is_callable([new IcmrBase, 'priv']),
        ];
    }
}

$icmrObj = new IcmrBase;
// An array callable is [target, method] at INTEGER indices 0 and 1 — not the first two
// entries in insertion order. The rule is part of the SHAPE, so $syntax_only sees it too.
icmrShow([
    'keys-0-1' => is_callable([0 => 'IcmrBase', 1 => 'st']),
    'keys-a-b' => is_callable(['a' => 'IcmrBase', 'b' => 'st']),
    'keys-0-2' => is_callable([0 => 'IcmrBase', 2 => 'st']),
    'keys-1-0' => is_callable([1 => 'IcmrBase', 0 => 'st']),
    'obj-keys-x-y' => is_callable(['x' => $icmrObj, 'y' => 'pub']),
]);
icmrShow([
    'syntax keys-0-1' => is_callable([0 => 'IcmrBase', 1 => 'st'], true),
    'syntax keys-a-b' => is_callable(['a' => 'IcmrBase', 'b' => 'st'], true),
    'syntax obj-priv' => is_callable([$icmrObj, 'priv'], true),
    'syntax nonstatic' => is_callable(['IcmrBase', 'pub'], true),
]);
// Malformed shapes stay false in both modes.
icmrShow([
    'one entry' => is_callable(['IcmrBase']),
    'three entries' => is_callable(['IcmrBase', 'st', 'x']),
    'int method' => is_callable(['IcmrBase', 5]),
    'nested' => is_callable([['IcmrBase', 'st']]),
    'empty' => is_callable([]),
]);

// Visibility is decided from the CALLING scope, by the method's declaring class.
icmrShow($icmrObj->fromInstance());
icmrShow(IcmrBase::fromStatic());
icmrShow((new IcmrChild)->fromChild());
icmrShow((new IcmrOther)->fromOutside());
icmrShow([
    'global [obj,priv]' => is_callable([$icmrObj, 'priv']),
    'global [obj,prot]' => is_callable([$icmrObj, 'prot']),
    'global [obj,pub]' => is_callable([$icmrObj, 'pub']),
    'global Base::pub' => is_callable('IcmrBase::pub'),
    'global Base::st' => is_callable('IcmrBase::st'),
    'global [Base,st]' => is_callable(['IcmrBase', 'st']),
]);

// An abstract method (an interface's included) has no body, so it is not callable.
abstract class IcmrAbstract
{
    abstract public function am();
    public function cm() { return 'cm'; }
    public static function sm() { return 'sm'; }
}
interface IcmrIface { public function im(); }
trait IcmrTrait { public function tm() { return 'tm'; } }
class IcmrUser { use IcmrTrait; }
icmrShow([
    'abstract' => is_callable(['IcmrAbstract', 'am']),
    'concrete-nonstatic' => is_callable(['IcmrAbstract', 'cm']),
    'concrete-static' => is_callable(['IcmrAbstract', 'sm']),
    'interface' => is_callable(['IcmrIface', 'im']),
    'trait via class' => is_callable([new IcmrUser, 'tm']),
    'trait direct' => is_callable(['IcmrTrait', 'tm']),
    'missing' => is_callable(['IcmrBase', 'nope']),
    'no such class' => is_callable(['IcmrNope', 'm']),
]);

// __call/__callStatic answer for any name, including an inaccessible one.
class IcmrMagic { private function hidden() {} public function __call($n, $a) { return 'call'; } }
class IcmrMagicStatic { private static function hidden() {} public static function __callStatic($n, $a) { return 'static'; } }
class IcmrPlain { private function hidden() {} }
icmrShow([
    '__call any' => is_callable([new IcmrMagic, 'whatever']),
    '__call hidden' => is_callable([new IcmrMagic, 'hidden']),
    '__callStatic any' => is_callable(['IcmrMagicStatic', 'whatever']),
    '__callStatic hidden' => is_callable(['IcmrMagicStatic', 'hidden']),
    'plain hidden' => is_callable([new IcmrPlain, 'hidden']),
    '__call not static' => is_callable(['IcmrMagic', 'whatever']),
    '__callStatic not instance' => is_callable([new IcmrMagicStatic, 'whatever']),
]);

// The other callable kinds are unaffected.
enum IcmrEnum { case A; public function em() { return 'em'; } }
class IcmrInvoke { public function __invoke() { return 'inv'; } }
icmrShow([
    'enum method' => is_callable([IcmrEnum::A, 'em']),
    'enum cases()' => is_callable(['IcmrEnum', 'cases']),
    '__invoke object' => is_callable(new IcmrInvoke),
    'closure' => is_callable(function () {}),
    'fcc' => is_callable(strlen(...)),
    'function name' => is_callable('strlen'),
    'scalar' => is_callable(42),
]);
?>
--EXPECT--
keys-0-1=T keys-a-b=F keys-0-2=F keys-1-0=F obj-keys-x-y=F
syntax keys-0-1=T syntax keys-a-b=F syntax obj-priv=T syntax nonstatic=T
one entry=F three entries=F int method=F nested=F empty=F
[this,priv]=T Base::priv=T Base::pub=T [Base,priv]=T
Base::priv=F Base::pub=F Base::st=T
[this,prot]=T [this,priv]=F Base::prot=T
Base::pub=F Base::st=T [obj,priv]=F
global [obj,priv]=F global [obj,prot]=F global [obj,pub]=T global Base::pub=F global Base::st=T global [Base,st]=T
abstract=F concrete-nonstatic=F concrete-static=T interface=F trait via class=T trait direct=F missing=F no such class=F
__call any=T __call hidden=T __callStatic any=T __callStatic hidden=T plain hidden=F __call not static=F __callStatic not instance=F
enum method=T enum cases()=T __invoke object=T closure=T fcc=T function name=T scalar=F
--CLEAN--
<?php
