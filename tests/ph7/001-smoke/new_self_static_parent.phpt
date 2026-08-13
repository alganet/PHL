--TEST--
new self() / new static() / new parent() with LSB, plus closure class-scope inheritance
--FILE--
<?php
class NspA {
    static function mkSelf() { return new self(); }
    static function mkStatic() { return new static(); }
    function dupSelf() { return new self(); }
    function dupStatic() { return new static(); }
}
class NspB extends NspA {}
echo get_class(NspA::mkSelf()), get_class(NspA::mkStatic()), "\n";
echo get_class(NspB::mkSelf()), get_class(NspB::mkStatic()), "\n";
$nspB = new NspB();
echo get_class($nspB->dupSelf()), get_class($nspB->dupStatic()), "\n";
// parent, with constructor arguments
class NspP { public $v; function __construct($v = 0) { $this->v = $v; } }
class NspC extends NspP {
    static function mk($v) { return new parent($v); }
}
$nspP = NspC::mk(5);
echo get_class($nspP), $nspP->v, "\n";
// constructor args and spread flow through static
class NspD {
    public $a; public $b;
    function __construct($a, $b) { $this->a = $a; $this->b = $b; }
    static function of($a, $b) { return new static($a, $b); }
}
class NspE extends NspD {}
$nspE = NspE::of(1, 2);
echo get_class($nspE), $nspE->a, $nspE->b, "\n";
class NspF {
    public $s;
    function __construct(...$xs) { $this->s = implode(",", $xs); }
    static function all(array $xs) { return new static(...$xs); }
}
echo NspF::all([7, 8, 9])->s, "\n";
// method chain on new self()
class NspG { function m() { return "chain"; } static function go() { return new self()->m(); } }
echo NspG::go(), "\n";
// static in a trait method resolves to the using class
trait NspT { static function make() { return new static(); } }
class NspH { use NspT; }
echo get_class(NspH::make()), "\n";
// new static() from an abstract base instantiates the called subclass
abstract class NspQ { static function mk() { return new static(); } }
class NspR extends NspQ {}
echo get_class(NspR::mk()), "\n";
// closures inherit the class scope of their creation site
class NspI {
    const X = 9;
    function viaClosure() { $f = function () { return [get_class(new self()), self::X]; }; return $f(); }
}
[$nspCls, $nspConst] = (new NspI())->viaClosure();
echo $nspCls, $nspConst, "\n";
// a bind() scope override replaces the closure's class scope
class NspS { const T = "s1"; }
$nspF2 = function () { return self::T; };
$nspG2 = Closure::bind($nspF2, null, NspS::class);
echo $nspG2(), "\n";
// instanceof round trip
echo NspB::mkStatic() instanceof NspB ? "T" : "F", NspB::mkSelf() instanceof NspB ? "T" : "F", "\n";
--EXPECT--
NspANspA
NspANspB
NspANspB
NspP5
NspE12
7,8,9
chain
NspH
NspR
NspI9
s1
TF
