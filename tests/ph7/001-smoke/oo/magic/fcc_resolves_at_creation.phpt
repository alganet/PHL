--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A first-class callable resolves its member at creation, receiver included
--DESCRIPTION--
php builds `$o->m(...)` / `C::m(...)` through the same member lookup a real call
goes through, so every refusal a call would raise happens at CREATION — an
undefined method, an inaccessible one, an abstract one, and a non-static one
named through a class with no receiver to run on. PHL handed back a Closure for
all four and discovered the problem only when (and if) it was invoked; one that
is built and dropped reported nothing at all. The receiver is the other half of
the same lookup: php binds the CALLING frame's `$this` when the resolved method
is non-static and that object is an instance of the named class, which is what
makes `self::m(...)` inside an instance method a working callable. PHL bound
only the scope, so it could never run.
--FILE--
<?php
abstract class FrcAbs {
    abstract public function frcAb();
    public function frcReal($x) { return "real($x) this=" . (isset($this) ? get_class($this) : '-'); }
    private function frcPriv($x) { return "priv($x)"; }
    protected function frcProt($x) { return "prot($x)"; }
    public static function frcStatic($x) { return "static($x)"; }
}
class FrcImpl extends FrcAbs {
    public function frcAb() { return 'ab'; }
    public function fromInstance() {
        return implode("\n", [
            (self::frcReal(...))(1),
            (static::frcReal(...))(2),
            (FrcAbs::frcReal(...))(3),
            (self::frcProt(...))(4),
            (self::frcStatic(...))(5),
            ($this->frcReal(...))(6),
        ]);
    }
    public static function fromStatic() {
        try { return (self::frcReal(...))(7); }
        catch (Throwable $e) { return get_class($e) . ': ' . $e->getMessage(); }
    }
}
class FrcMagic {
    public function __call($n, $a) { return "__call($n)"; }
    public static function __callStatic($n, $a) { return "__callStatic($n)"; }
    public function inside() { $c = self::frcMiss(...); return $c(); }
}
function frcShow($label, $fn) {
    try { $v = $fn(); $out = is_object($v) ? 'Closure' : var_export($v, true); }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    echo $label, ' => ', $out, "\n";
}

/* A non-static method named through a class binds the caller's $this. */
echo (new FrcImpl)->fromInstance(), "\n";
echo FrcImpl::fromStatic(), "\n";

/* Creation refuses what a call would refuse. */
$impl = new FrcImpl;
frcShow('obj missing',    fn() => $impl->frcZz(...));
frcShow('obj private',    fn() => $impl->frcPriv(...));
frcShow('obj protected',  fn() => $impl->frcProt(...));
frcShow('obj implemented',fn() => $impl->frcAb(...));
frcShow('cls abstract',   fn() => FrcAbs::frcAb(...));
frcShow('cls nonstatic',  fn() => FrcImpl::frcReal(...));
frcShow('cls static',     fn() => FrcImpl::frcStatic(...));
frcShow('cls missing',    fn() => FrcImpl::frcZz(...));
frcShow('unknown class',  fn() => FrcNoSuch::frcZz(...));
frcShow('interface',      fn() => Countable::count(...));

/* A catch-all still answers, and the static spelling picks the receiver a call would. */
$m = new FrcMagic;
echo ($m->frcZz(...))(), "\n";
echo (FrcMagic::frcZz(...))(), "\n";
echo $m->inside(), "\n";
echo "end\n";
?>
--EXPECT--
real(1) this=FrcImpl
real(2) this=FrcImpl
real(3) this=FrcImpl
prot(4)
static(5)
real(6) this=FrcImpl
Error: Non-static method FrcAbs::frcReal() cannot be called statically
obj missing => Error: Call to undefined method FrcImpl::frcZz()
obj private => Error: Call to private method FrcAbs::frcPriv() from global scope
obj protected => Error: Call to protected method FrcAbs::frcProt() from global scope
obj implemented => Closure
cls abstract => Error: Cannot call abstract method FrcAbs::frcAb()
cls nonstatic => Error: Non-static method FrcAbs::frcReal() cannot be called statically
cls static => Closure
cls missing => Error: Call to undefined method FrcImpl::frcZz()
unknown class => Error: Class "FrcNoSuch" not found
interface => Error: Cannot call abstract method Countable::count()
__call(frcZz)
__callStatic(frcZz)
__call(frcMiss)
end
