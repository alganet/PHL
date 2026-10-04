--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A literal X::__construct() asks for the constructor, not for a method
--DESCRIPTION--
php's compiler drops a literal `__construct` name from a static call, in any
case, and the call then asks the class for its constructor instead of looking a
method up. So none of the method-lookup refusals apply: a class without one is
"Cannot call constructor" (no __call/__callStatic fallback), and the only
visibility refusal is a PRIVATE constructor called with a `$this` whose class
is not the constructor's own -- the OBJECT's class, not the calling scope, so
`self::__construct()` inside the declaring class still refuses on a subclass
instance, and the message names the class the `::` resolved to. A protected
constructor, or any call with no `$this`, falls to the non-static rule. A
DYNAMIC name (`parent::$m()`) is an ordinary lookup and keeps its messages.
--FILE--
<?php
function scctTry($l, $f) {
    try { $f(); echo "$l: ok\n"; }
    catch (Throwable $e) { echo "$l: ", get_class($e), ": ", $e->getMessage(), "\n"; }
}
class ScctA {
    private function __construct() { echo " ScctA ctor\n"; }
    public static function mk() { return new ScctA; }
    public function selfOnThis() { self::__construct(); }
    public static function st() { self::__construct(); }
    public function inClosure() { return function () { ScctA::__construct(); }; }
}
class ScctB extends ScctA { public function __construct() { parent::__construct(); } }
class ScctB2 extends ScctA {
    public function __construct() {}
    public function byName() { ScctA::__construct(); }
    public function upper() { parent::__CONSTRUCT(); }
    public function dyn() { $m = '__construct'; parent::$m(); }
}
class ScctC extends ScctB2 { public function grand() { ScctA::__construct(); } }
class ScctNone {}
class ScctNoneKid extends ScctNone { public function __construct() { parent::__construct(); } }
class ScctMagic {
    public static function __callStatic($n, $a) { echo " callStatic $n\n"; }
    public function __call($n, $a) { echo " call $n\n"; }
    public function f() { self::__construct(); }
}
class ScctP { protected function __construct() { echo " ScctP ctor\n"; } }
class ScctQ { public function f() { ScctP::__construct(); } }
class ScctR extends ScctP { public function __construct() { parent::__construct(); } }
trait ScctT { private function __construct() { echo " ScctT ctor\n"; } public static function make() { return new static; } }
class ScctU { use ScctT; public function again() { self::__construct(); } }
class ScctV extends ScctU { public function __construct() { parent::__construct(); } }

scctTry('parent, private', fn() => new ScctB);
scctTry('by class name', fn() => (new ScctB2)->byName());
scctTry('upper case', fn() => (new ScctB2)->upper());
scctTry('dynamic name', fn() => (new ScctB2)->dyn());
scctTry('grandparent', fn() => (new ScctC)->grand());
scctTry('self on a subclass $this', fn() => (new ScctB2)->selfOnThis());
scctTry('self on its own $this', fn() => ScctA::mk()->selfOnThis());
scctTry('closure, own $this', fn() => (ScctA::mk()->inClosure())());
scctTry('closure, subclass $this', fn() => ((new ScctB2)->inClosure())());
scctTry('static method', fn() => ScctA::st());
scctTry('no constructor', fn() => new ScctNoneKid);
scctTry('no constructor, magic', fn() => (new ScctMagic)->f());
scctTry('no constructor, global', fn() => ScctNone::__construct());
scctTry('protected, unrelated', fn() => (new ScctQ)->f());
scctTry('protected, parent', fn() => new ScctR);
scctTry('private, global', fn() => ScctA::__construct());
scctTry('protected, global', fn() => ScctP::__construct());
scctTry('trait, own class', fn() => ScctU::make()->again());
scctTry('trait, subclass', fn() => new ScctV);
?>
--EXPECT--
parent, private: Error: Cannot call private ScctA::__construct()
by class name: Error: Cannot call private ScctA::__construct()
upper case: Error: Cannot call private ScctA::__construct()
dynamic name: Error: Call to private method ScctA::__construct() from scope ScctB2
grandparent: Error: Cannot call private ScctA::__construct()
self on a subclass $this: Error: Cannot call private ScctA::__construct()
 ScctA ctor
 ScctA ctor
self on its own $this: ok
 ScctA ctor
 ScctA ctor
closure, own $this: ok
closure, subclass $this: Error: Cannot call private ScctA::__construct()
static method: Error: Non-static method ScctA::__construct() cannot be called statically
no constructor: Error: Cannot call constructor
no constructor, magic: Error: Cannot call constructor
no constructor, global: Error: Cannot call constructor
protected, unrelated: Error: Non-static method ScctP::__construct() cannot be called statically
 ScctP ctor
protected, parent: ok
private, global: Error: Non-static method ScctA::__construct() cannot be called statically
protected, global: Error: Non-static method ScctP::__construct() cannot be called statically
 ScctT ctor
 ScctT ctor
trait, own class: ok
trait, subclass: Error: Cannot call private ScctU::__construct()
