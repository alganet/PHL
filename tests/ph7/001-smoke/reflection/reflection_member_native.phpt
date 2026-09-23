--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ReflectionProperty/ReflectionClassConstant report php's modifier bits and the declaring class
--DESCRIPTION--
php's $class property on a member reflector is the DECLARING class, not the one
the lookup went through, and its getModifiers() carries bits the declaration
only implies: readonly is also protected(set), and private(set) is also final.
Reflection::getModifierNames() renders them through one three-way switch per
visibility kind, so a mask with two visibility bits names neither.
--FILE--
<?php
class ReflMemBase2 {
    /** pdoc */
    public int $typed = 1;
    public $untyped;
    protected string $prot = 'p';
    private $priv = 'q';
    public static int $stInit = 5;
    public static int $stUninit;
    public readonly int $ro;
    public private(set) string $ps = 'a';
    public protected(set) string $prs = 'b';
    public int $hooked { get => 42; set(int $v) { $this->typed = $v; } }
    public int $virt { get => 7; }
    const BC = 'bc';
    final protected const BF = 'bf';
    const int BT = 3;
    public function __construct(public readonly int $prom = 9) { $this->ro = 1; }
}
class ReflMemKid2 extends ReflMemBase2 {}

foreach ((new ReflectionClass('ReflMemBase2'))->getProperties() as $p) {
    echo str_pad($p->getName(), 10), ' = ', str_pad((string)$p->getModifiers(), 5),
         ' [', implode(',', Reflection::getModifierNames($p->getModifiers())), "]\n";
}
echo 'ambiguous mask: ', json_encode(Reflection::getModifierNames(1 | 2 | 4 | 2048 | 4096)), "\n";

// The mangled name php stores the slot under.
foreach (['typed', 'prot', 'priv'] as $n) {
    echo $n, ' -> ', str_replace("\0", '@', (new ReflectionProperty('ReflMemBase2', $n))->getMangledName()), "\n";
}

// $class is the DECLARING class even when asked through the subclass.
$p = new ReflectionProperty('ReflMemKid2', 'typed');
$c = new ReflectionClassConstant('ReflMemKid2', 'BC');
echo 'p class=', $p->class, ' decl=', $p->getDeclaringClass()->getName(), "\n";
echo 'c class=', $c->class, ' decl=', $c->getDeclaringClass()->getName(), "\n";
// ...and a base PRIVATE is not reachable by name from the subclass at all.
try { new ReflectionProperty('ReflMemKid2', 'priv'); }
catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }

echo 'promoted=', var_export((new ReflectionProperty('ReflMemBase2', 'prom'))->isPromoted(), true),
     ' notPromoted=', var_export($p->isPromoted(), true), "\n";
echo 'doc=', trim($p->getDocComment()), ' nodoc=',
     var_export((new ReflectionProperty('ReflMemBase2', 'untyped'))->getDocComment(), true), "\n";
// (php 8.5 deprecates getDefaultValue() where there is none; the answer is
// what this pins, so the notice is silenced.)
$prev = error_reporting(E_ALL & ~E_DEPRECATED);
echo 'default typed=', json_encode([$p->hasDefaultValue(), $p->getDefaultValue()]),
     ' untyped=', json_encode([(new ReflectionProperty('ReflMemBase2', 'untyped'))->hasDefaultValue(),
                               (new ReflectionProperty('ReflMemBase2', 'untyped'))->getDefaultValue()]),
     ' readonly=', json_encode([(new ReflectionProperty('ReflMemBase2', 'ro'))->hasDefaultValue(),
                                (new ReflectionProperty('ReflMemBase2', 'ro'))->getDefaultValue()]), "\n";
error_reporting($prev);

// Hooks are reflected through PropertyHookType, which is an enum.
$h = new ReflectionProperty('ReflMemBase2', 'hooked');
echo 'hooks=', json_encode(array_keys($h->getHooks())),
     ' get=', var_export($h->hasHook(PropertyHookType::Get), true),
     ' set=', var_export($h->hasHook(PropertyHookType::Set), true),
     ' getHook=', get_class($h->getHook(PropertyHookType::Get)),
     ' virtSet=', var_export((new ReflectionProperty('ReflMemBase2', 'virt'))->getHook(PropertyHookType::Set), true), "\n";
echo 'enum: ', json_encode(array_map(fn($e) => [$e->name, $e->value], PropertyHookType::cases())),
     ' from=', PropertyHookType::from('set')->name,
     ' identity=', var_export(PropertyHookType::Get === PropertyHookType::Get, true),
     ' backed=', var_export(PropertyHookType::Get instanceof BackedEnum, true), "\n";

// Values: instance, static, and the two receiver rules php enforces.
$o = new ReflMemBase2();
echo 'get=', $p->getValue($o), ' private=', (new ReflectionProperty('ReflMemBase2', 'priv'))->getValue($o),
     ' static=', (new ReflectionProperty('ReflMemBase2', 'stInit'))->getValue(), "\n";
$p->setValue($o, 77);
echo 'set=', $p->getValue($o), ' init=', var_export($p->isInitialized($o), true),
     ' staticUninit=', var_export((new ReflectionProperty('ReflMemBase2', 'stUninit'))->isInitialized(), true), "\n";
try { $p->getValue(); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
try { $p->setValue($o, 'nope'); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }

// Class constants.
echo 'BC=', var_export($c->getValue(), true), ' mods=', $c->getModifiers(),
     ' BF=', (new ReflectionClassConstant('ReflMemBase2', 'BF'))->getModifiers(),
     ' typed=', json_encode([(new ReflectionClassConstant('ReflMemBase2', 'BT'))->hasType(),
                             (string)(new ReflectionClassConstant('ReflMemBase2', 'BT'))->getType()]),
     ' enumCase=', var_export((new ReflectionClassConstant('PropertyHookType', 'Get'))->isEnumCase(), true), "\n";
try { new ReflectionClassConstant('ReflMemBase2', 'NOPE'); }
catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }

// Both reflectors are uncloneable, as php's are.
foreach ([$p, $c] as $r) {
    try { $x = clone $r; echo "cloned\n"; } catch (Throwable $e) { echo get_class($e), "\n"; }
}
?>
--EXPECT--
typed      = 1     [public]
untyped    = 1     [public]
prot       = 2     [protected]
priv       = 4     [private]
stInit     = 17    [public,static]
stUninit   = 17    [public,static]
ro         = 2177  [public,protected(set),readonly]
ps         = 4129  [final,public,private(set)]
prs        = 2049  [public,protected(set)]
hooked     = 513   [virtual,public]
virt       = 513   [virtual,public]
prom       = 2177  [public,protected(set),readonly]
ambiguous mask: []
typed -> typed
prot -> @*@prot
priv -> @ReflMemBase2@priv
p class=ReflMemBase2 decl=ReflMemBase2
c class=ReflMemBase2 decl=ReflMemBase2
ReflectionException: Property ReflMemKid2::$priv does not exist
promoted=true notPromoted=false
doc=/** pdoc */ nodoc=false
default typed=[true,1] untyped=[true,null] readonly=[false,null]
hooks=["get","set"] get=true set=true getHook=ReflectionMethod virtSet=NULL
enum: [["Get","get"],["Set","set"]] from=Set identity=true backed=true
get=1 private=q static=5
set=77 init=true staticUninit=false
TypeError: ReflectionProperty::getValue(): Argument #1 ($object) must be provided for instance properties
TypeError: Cannot assign string to property ReflMemBase2::$typed of type int
BC='bc' mods=1 BF=34 typed=[true,"int"] enumCase=true
ReflectionException: Constant ReflMemBase2::NOPE does not exist
Error
Error
