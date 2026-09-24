--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A non-static method named through :: is refused unless the CALLING frame holds a compatible $this
--DESCRIPTION--
php decides this where the call is written: a non-static method reached through
a class name runs only when the calling frame's own $this is an instance of
that class — which is what makes self::/parent::/static:: and C::m() from
inside a C method work, and every other spelling an Error. PHL ran the body
regardless, with $this unset (so every property read inside it was a fresh
error) or, for the $obj::m() form, bound to the object the :: was written on —
where php binds the CALLER's, so the method ran on the wrong object. The
callable spellings (call_user_func, is_callable, first-class callables) already
refused it; only the direct syntax did not.
--FILE--
<?php
class ScNs {
    public $tag;
    public function __construct($tag = 'x') { $this->tag = $tag; }
    public function m() { return "m:" . (isset($this) ? $this->tag : "no-this"); }
    public static function s() { return "s"; }
    public function viaSelf() { return self::m(); }
    public function viaStatic() { return static::m(); }
    public function viaName() { return ScNs::m(); }
    public function viaObject(ScNs $other) { return $other::m(); }
    public static function viaStaticMethod() { return ScNs::m(); }
}
class ScNsKid extends ScNs {
    public function viaParent() { return parent::m(); }
}
class ScNsOther {
    public function viaForeign() { return ScNs::m(); }
}
interface ScNsIface { public function im(); }

// Allowed: the calling frame holds a $this the class accepts.
$o = new ScNs('A');
echo $o->viaSelf(), " ", $o->viaName(), "\n";
echo (new ScNsKid('K'))->viaStatic(), " ", (new ScNsKid('K'))->viaParent(), "\n";
// …and the receiver is the CALLER's $this, never the object the :: was on.
echo $o->viaObject(new ScNs('B')), "\n";
// Refused everywhere else, naming the DECLARING class.
foreach (['global' => fn() => ScNs::m(),
          'through a subclass' => fn() => ScNsKid::m(),
          'on an object' => fn() => $GLOBALS['o']::m(),
          'variable class' => function () { $c = 'ScNs'; return $c::m(); },
          'from a static method' => fn() => ScNs::viaStaticMethod(),
          'from another class' => fn() => (new ScNsOther)->viaForeign()] as $how => $call) {
    try { $r = $call(); }
    catch (Error $e) { $r = $e->getMessage(); }
    echo $how, ": ", $r, "\n";
}
// A closure declared in a method carries php's $this too, so the same call
// works inside one — and answers the CALLER's object, not the captured one.
$viaClosure = function (ScNs $other) { return [ScNs::m(), self::m(), static::m(), $other::m()]; };
echo implode(" ", Closure::bind($viaClosure, $o, ScNs::class)(new ScNs('B'))), "\n";
// A static method is unaffected, and so is the callable route's own wording.
echo ScNs::s(), " ", var_export(is_callable(['ScNs', 'm']), true), "\n";
try { call_user_func(['ScNs', 'm']); } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
// An interface's methods are all abstract, which is what php reports for them.
try { ScNsIface::im(); } catch (Error $e) { echo $e->getMessage(), "\n"; }
?>
--EXPECT--
m:A m:A
m:K m:K
m:A
global: Non-static method ScNs::m() cannot be called statically
through a subclass: Non-static method ScNs::m() cannot be called statically
on an object: Non-static method ScNs::m() cannot be called statically
variable class: Non-static method ScNs::m() cannot be called statically
from a static method: Non-static method ScNs::m() cannot be called statically
from another class: Non-static method ScNs::m() cannot be called statically
m:A m:A m:A m:A
s false
call_user_func(): Argument #1 ($callback) must be a valid callback, non-static method ScNs::m() cannot be called statically
Cannot call abstract method ScNsIface::im()
--CLEAN--
<?php
