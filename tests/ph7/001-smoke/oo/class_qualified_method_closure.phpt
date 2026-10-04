--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A class-qualified method closure keeps the method it named, not the receiver's override
--DESCRIPTION--
`parent::m(...)`, `A::m(...)` and ReflectionMethod::getClosure() hand back a
closure over the RESOLVED method, bound to the calling `$this`. php keeps that
function, so invoking the closure -- directly, as a callback, through a fiber or
after bindTo() -- runs it, and not whatever the receiver's own class declares
under the name; `static::` inside it is still the receiver's class. The same
closure reports the resolved class as its scope.
A literal `X::__construct(...)` is the class's constructor, as for the call: no
constructor is "Cannot call constructor", the one visibility refusal is a
private constructor reached with a `$this` of another class, and without a
fitting `$this` it is the non-static Error.
--FILE--
<?php
function cqmcTry($l, $f) {
    try { $r = $f(); echo "$l: ok", $r === null ? "" : " " . var_export($r, true), "\n"; }
    catch (Throwable $e) { echo "$l: ", get_class($e), ": ", $e->getMessage(), "\n"; }
}
class CqmcP {
    function __construct() { echo "> CqmcP ctor ", static::class, "\n"; }
    function foo($x) { echo "> CqmcP::foo $x ", static::class, " ", self::class, "\n"; return 7; }
    function byRef(&$r) { $r = 'CqmcP'; }
}
class CqmcQ extends CqmcP {
    function __construct() { echo "> CqmcQ ctor\n"; }
    function foo($x) { echo "> CqmcQ::foo $x\n"; return 8; }
    function byRef(&$r) { $r = 'CqmcQ'; }
    function run() {
        $d = parent::foo(...);
        cqmcTry('direct', fn() => $d(1));
        cqmcTry('call_user_func', fn() => call_user_func($d, 2));
        cqmcTry('array_map', fn() => array_map($d, [3])[0]);
        cqmcTry('__invoke', fn() => $d->__invoke(4));
        cqmcTry('bindTo', fn() => $d->bindTo(new CqmcR)(5));
        cqmcTry('named', fn() => $d(x: 6));
        $f = new Fiber($d); $f->start(7);
        $r = new ReflectionFunction($d);
        echo "scope ", $r->getClosureScopeClass()->name, " called ", $r->getClosureCalledClass()->name,
            " this ", get_class($r->getClosureThis()), "\n";
        echo "function ", (new ReflectionFunction($d))->getClosureScopeClass()->name, "::", (new ReflectionFunction($d))->name, "\n";
        cqmcTry('A::foo', fn() => CqmcP::foo(...)(8));
        cqmcTry('self::foo', fn() => self::foo(...)(9));
        cqmcTry('static::foo', fn() => static::foo(...)(10));
        $b = parent::byRef(...); $v = null; $b($v); echo "byRef $v\n";
        $c = parent::__construct(...); $c();
        $c = parent::__CONSTRUCT(...); $c();
    }
}
class CqmcR extends CqmcQ {}
(new CqmcR)->run();
$g = (new ReflectionMethod('CqmcP', 'foo'))->getClosure(new CqmcQ);
cqmcTry('getClosure', fn() => $g(11));
cqmcTry('getClosure cb', fn() => call_user_func($g, 12));
echo "getClosure scope ", (new ReflectionFunction($g))->getClosureScopeClass()->name, "\n";

class CqmcA { private function __construct() { echo "> CqmcA ctor\n"; }
    function selfFcc() { return self::__construct(...); } }
class CqmcB extends CqmcA { function __construct() {} function f() { return parent::__construct(...); } }
class CqmcPr { protected function __construct() { echo "> CqmcPr ctor\n"; } }
class CqmcPr2 extends CqmcPr { function __construct() {} function f() { return parent::__construct(...); } }
class CqmcN {}
class CqmcM extends CqmcN {
    function __call($n, $a) { echo "__call $n\n"; }
    static function __callStatic($n, $a) { echo "__callStatic $n\n"; }
    function f() { return parent::__construct(...); }
    function dyn() { $m = '__construct'; return parent::$m(...); }
}
cqmcTry('private parent', fn() => (new CqmcB)->f());
cqmcTry('private self on subclass', fn() => (new ReflectionClass('CqmcB'))->newInstanceWithoutConstructor()->selfFcc());
cqmcTry('private self on own', fn() => (new ReflectionClass('CqmcA'))->newInstanceWithoutConstructor()->selfFcc()());
cqmcTry('protected parent', fn() => (new CqmcPr2)->f()());
cqmcTry('none', fn() => (new CqmcM)->f());
cqmcTry('none dynamic', fn() => (new CqmcM)->dyn()());
cqmcTry('global private', fn() => CqmcA::__construct(...));
cqmcTry('global protected', fn() => CqmcPr::__construct(...));
cqmcTry('global none', fn() => CqmcN::__construct(...));
cqmcTry('global public', fn() => CqmcP::__construct(...));
?>
--EXPECT--
> CqmcQ ctor
> CqmcP::foo 1 CqmcR CqmcP
direct: ok 7
> CqmcP::foo 2 CqmcR CqmcP
call_user_func: ok 7
> CqmcP::foo 3 CqmcR CqmcP
array_map: ok 7
> CqmcP::foo 4 CqmcR CqmcP
__invoke: ok 7
> CqmcQ ctor
> CqmcP::foo 5 CqmcR CqmcP
bindTo: ok 7
> CqmcP::foo 6 CqmcR CqmcP
named: ok 7
> CqmcP::foo 7 CqmcR CqmcP
scope CqmcP called CqmcR this CqmcR
function CqmcP::foo
> CqmcP::foo 8 CqmcR CqmcP
A::foo: ok 7
> CqmcQ::foo 9
self::foo: ok 8
> CqmcQ::foo 10
static::foo: ok 8
byRef CqmcP
> CqmcP ctor CqmcR
> CqmcP ctor CqmcR
> CqmcQ ctor
> CqmcP::foo 11 CqmcQ CqmcP
getClosure: ok 7
> CqmcP::foo 12 CqmcQ CqmcP
getClosure cb: ok 7
getClosure scope CqmcP
private parent: Error: Cannot call private CqmcA::__construct()
private self on subclass: Error: Cannot call private CqmcA::__construct()
> CqmcA ctor
private self on own: ok
> CqmcPr ctor
protected parent: ok
none: Error: Cannot call constructor
none dynamic: Error: Call to undefined method CqmcN::__construct()
global private: Error: Non-static method CqmcA::__construct() cannot be called statically
global protected: Error: Non-static method CqmcPr::__construct() cannot be called statically
global none: Error: Cannot call constructor
global public: Error: Non-static method CqmcP::__construct() cannot be called statically
