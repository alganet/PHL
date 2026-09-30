--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
self and __CLASS__ inside a trait method name the class that USED the trait
--DESCRIPTION--
php composes a trait method INTO the using class, so `self` inside it is that
class for every instance — `class Base { use T; } class Kid extends Base {}`
answers Base from a Kid instance too. PHL shares a trait method by pointer and
its declaring class stays the TRAIT, so every resolution site fell back to the
RUNTIME class as the nearest stand-in: `self::CONST` and `self::$static` read
Kid's where php reads Base's, silently, and `__CLASS__` answered the trait's own
name. The same rule governs `instanceof self` (which answered a silent FALSE for
every operand, the trait being nobody's class) and the class a return-type
TypeError names. `__TRAIT__` and `__METHOD__` do name the trait — php's own asymmetry, and
PHL already matched those. A trait composed into another trait flattens the same
way: the answer is still the CLASS that used the outer one.
--FILE--
<?php
trait TsucT {
    public function whoSelf() { return self::class; }
    public function whoStatic() { return static::class; }
    public function whoClass() { return __CLASS__; }
    public function whoTrait() { return __TRAIT__; }
    public function whoMethod() { return __METHOD__; }
    public function konst() { return self::TSUC_K; }
    public function stat() { return self::$tsucS; }
    public function newSelf() { return get_class(new self); }
    public function par() { return parent::class; }
    public function callSelf() { return self::tsucHelper(); }
    public static function sWhoSelf() { return self::class; }
}
class TsucGrand { public function tsucHelper() { return 'Grand.helper'; } }
class TsucBase extends TsucGrand {
    use TsucT;
    const TSUC_K = 'Base.K';
    public static $tsucS = 'Base.s';
    public function tsucHelper() { return 'Base.helper'; }
}
class TsucKid extends TsucBase {
    const TSUC_K = 'Kid.K';
    public static $tsucS = 'Kid.s';
    public function tsucHelper() { return 'Kid.helper'; }
}
foreach ([new TsucBase, new TsucKid] as $o) {
    echo get_class($o), ': self=', $o->whoSelf(), ' static=', $o->whoStatic(),
         ' __CLASS__=', $o->whoClass(), ' __TRAIT__=', $o->whoTrait(),
         ' __METHOD__=', $o->whoMethod(), "\n";
    echo '  K=', $o->konst(), ' s=', $o->stat(), ' new=', $o->newSelf(),
         ' parent=', $o->par(), ' callSelf=', $o->callSelf(), "\n";
}
echo 'static ctx: ', TsucBase::sWhoSelf(), ' ', TsucKid::sWhoSelf(), "\n";

/* A trait used by ANOTHER trait still resolves to the composing CLASS. */
trait TsucInner { public function inner() { return self::class . '/' . __CLASS__; } }
trait TsucOuter { use TsucInner; public function outer() { return self::class; } }
class TsucHost { use TsucOuter; }
class TsucHostKid extends TsucHost {}
echo (new TsucHost)->inner(), ' ', (new TsucHostKid)->inner(), "\n";
echo (new TsucHost)->outer(), ' ', (new TsucHostKid)->outer(), "\n";

/* A second, unrelated composer keeps its own answer. */
class TsucOther { use TsucT; const TSUC_K = 'Other.K'; public static $tsucS = 'Other.s';
    public function tsucHelper() { return 'Other.helper'; } }
$x = new TsucOther;
echo $x->whoSelf(), ' ', $x->konst(), ' ', $x->stat(), ' ', $x->callSelf(), "\n";
/* `instanceof self` is the same rule, and it was the one door that never asked
 * it: the operand was compared against the TRAIT, and nothing is ever an
 * instance of a trait, so every answer was a silent FALSE -- including for an
 * object of the very class that used it. The same false came out of a closure
 * or arrow function declared in a trait body, whose scope is the trait too. */
trait TsuT {
    public function instOf(object $x): bool { return $x instanceof self; }
    public function instOfParent(object $x): bool { return $x instanceof parent; }
    public function instOfStatic(object $x): bool { return $x instanceof static; }
    public function closureInstOf(object $x): bool {
        $f = function () use ($x) { return $x instanceof self; };
        return $f();
    }
    public function arrowInstOf(object $x): bool {
        $f = fn() => $x instanceof self;
        return $f();
    }
    public function wantsSelf(self $x): string { return 'ok ' . get_class($x); }
    public function givesSelf(object $x): self { return $x; }
}
class TsuBase {}
class TsuUser extends TsuBase { use TsuT; }
class TsuKid extends TsuUser {}
class TsuOther { use TsuT; }
$tsuU = new TsuUser; $tsuK = new TsuKid; $tsuB = new TsuBase; $tsuO = new TsuOther;
echo 'instOf: user=', var_export($tsuU->instOf($tsuU), true),
     ' kid=', var_export($tsuU->instOf($tsuK), true),
     ' base=', var_export($tsuU->instOf($tsuB), true),
     ' other=', var_export($tsuU->instOf($tsuO), true), "\n";
echo 'from kid: ', var_export($tsuK->instOf($tsuU), true),
     ' from other: ', var_export($tsuO->instOf($tsuU), true), "\n";
echo 'parent(base)=', var_export($tsuU->instOfParent($tsuB), true),
     ' static(user) from kid=', var_export($tsuK->instOfStatic($tsuU), true), "\n";
echo 'closure=', var_export($tsuU->closureInstOf($tsuK), true),
     ' arrow=', var_export($tsuU->arrowInstOf($tsuK), true), "\n";

/* Both type-check diagnostics of one trait method name the SAME class: the
 * return-side one used to report the trait where the argument-side one, handed
 * an already-resolved owner, reported the composing class. */
echo $tsuU->wantsSelf($tsuK), "\n";
try { $tsuU->wantsSelf($tsuB); } catch (TypeError $e) {
    echo 'arg: ', strstr($e->getMessage(), ', ', true), "\n";
}
try { $tsuU->givesSelf($tsuB); } catch (TypeError $e) {
    echo 'ret: ', $e->getMessage(), "\n";
}

/* A trait used by BOTH a class and its subclass composes TWICE, and each copy
 * has its own `self`. */
trait TsuU2 { public function isSelf(object $x): bool { return $x instanceof self; } }
class TsuP { use TsuU2; }
class TsuC extends TsuP { use TsuU2; }
echo 'P->isSelf(C)=', var_export((new TsuP)->isSelf(new TsuC), true),
     ' C->isSelf(P)=', var_export((new TsuC)->isSelf(new TsuP), true), "\n";
echo "end\n";
?>
--EXPECT--
TsucBase: self=TsucBase static=TsucBase __CLASS__=TsucBase __TRAIT__=TsucT __METHOD__=TsucT::whoMethod
  K=Base.K s=Base.s new=TsucBase parent=TsucGrand callSelf=Base.helper
TsucKid: self=TsucBase static=TsucKid __CLASS__=TsucBase __TRAIT__=TsucT __METHOD__=TsucT::whoMethod
  K=Base.K s=Base.s new=TsucBase parent=TsucGrand callSelf=Base.helper
static ctx: TsucBase TsucBase
TsucHost/TsucHost TsucHost/TsucHost
TsucHost TsucHost
TsucOther Other.K Other.s Other.helper
instOf: user=true kid=true base=false other=false
from kid: true from other: false
parent(base)=true static(user) from kid=false
closure=true arrow=true
ok TsuKid
arg: TsuUser::wantsSelf(): Argument #1 ($x) must be of type TsuUser
ret: TsuUser::givesSelf(): Return value must be of type TsuUser, TsuBase returned
P->isSelf(C)=true C->isSelf(P)=false
end
