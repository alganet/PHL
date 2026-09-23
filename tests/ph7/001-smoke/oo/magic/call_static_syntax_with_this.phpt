--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
self::/static::/parent:: reach __call when the caller has a compatible $this
--DESCRIPTION--
php's get_static_method_fallback: a `C::m()` naming a method the class cannot
answer directly — missing, or present but inaccessible — routes to __call, not
__callStatic, when the CALLING frame has a `$this` that is an instance of C.
`::` does not make the call static. PHL always took __callStatic, so a class
declaring both ran the wrong handler and a class declaring only __call got
"Call to undefined method". The handler comes from the OBJECT's class, so
`parent::m()` from a child that overrides __call runs the CHILD's. From a real
static context (a static method, the top level) the __callStatic answer stands.
A CLOSURE body counts as having that $this — php binds one to every closure
created inside a method — which is also why is_callable(['C','m']) is true there.
--FILE--
<?php
class CsswP {
    private function csswPriv() { return 'real'; }
    private static function csswPrivStatic() { return 'realStatic'; }
    public function __call($n, $a) { return "P::__call($n) on " . get_class($this); }
    public static function __callStatic($n, $a) { return "P::__callStatic($n)"; }
}
class CsswK extends CsswP {
    public function __call($n, $a) { return "K::__call($n) on " . get_class($this); }
    public static function __callStatic($n, $a) { return "K::__callStatic($n)"; }
    public function fromInstance() {
        return implode("\n", [self::a(), static::b(), parent::c(), CsswK::d(), CsswP::e(),
                              self::csswPriv(), self::csswPrivStatic()]);
    }
    public static function fromStatic() {
        return implode("\n", [self::f(), static::g(), parent::h(), CsswP::i()]);
    }
}
echo (new CsswK)->fromInstance(), "\n~~\n";
echo CsswK::fromStatic(), "\n~~\n";

/* Only __call declared: an instance context dispatches, a static one does not
 * fall back to a __callStatic that is not there. */
class CsswOnlyCall {
    public function __call($n, $a) { return "onlyCall($n)"; }
    public function inst() { return self::x(); }
    public static function stat() { return self::y(); }
}
echo (new CsswOnlyCall)->inst(), "\n";
try { CsswOnlyCall::stat(); } catch (Error $e) { echo $e->getMessage(), "\n"; }
try { CsswOnlyCall::z(); } catch (Error $e) { echo $e->getMessage(), "\n"; }

/* Only __callStatic declared: the instance context has no __call to prefer. */
class CsswOnlyStatic {
    public static function __callStatic($n, $a) { return "onlyStatic($n)"; }
    public function inst() { return self::x(); }
}
echo (new CsswOnlyStatic)->inst(), "\n";

/* An UNRELATED class named from inside a method: $this is not an instance of it. */
class CsswOther {
    public function __call($n, $a) { return "other::__call($n)"; }
    public static function __callStatic($n, $a) { return "other::__callStatic($n)"; }
}
class CsswCaller { public function go() { return CsswOther::w(); } }
echo (new CsswCaller)->go(), "\n";

/* A closure body carries the $this php bound to it. */
class CsswClosures {
    public function __call($n, $a) { return "__call($n)"; }
    public static function __callStatic($n, $a) { return "__callStatic($n)"; }
    public function viaArrow() { $f = fn() => self::p(); return $f(); }
    public function viaClosure() { $f = function () { return self::q(); }; return $f(); }
    public function viaStatic() { $f = static function () { return self::r(); }; return $f(); }
    public function callable1() { $f = fn() => is_callable([CsswClosures::class, 'viaArrow']); return $f(); }
}
$c = new CsswClosures;
echo $c->viaArrow(), "\n", $c->viaClosure(), "\n", $c->viaStatic(), "\n";
var_dump($c->callable1());
echo "end\n";
?>
--EXPECT--
K::__call(a) on CsswK
K::__call(b) on CsswK
K::__call(c) on CsswK
K::__call(d) on CsswK
K::__call(e) on CsswK
K::__call(csswPriv) on CsswK
K::__call(csswPrivStatic) on CsswK
~~
K::__callStatic(f)
K::__callStatic(g)
P::__callStatic(h)
P::__callStatic(i)
~~
onlyCall(x)
Call to undefined method CsswOnlyCall::y()
Call to undefined method CsswOnlyCall::z()
onlyStatic(x)
other::__callStatic(w)
__call(p)
__call(q)
__callStatic(r)
bool(true)
end
