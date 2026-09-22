--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
a class method whose body is a C routine dispatches like any other method
--FILE--
<?php
/* Until VM_FUNC_NATIVE, a ph7_class_method could only ever hold BYTECODE, so a
 * builtin class reached the engine through a GLOBAL thunk that its one-line PHP
 * method forwarded to (`__closure_bindTo($this, ...)`). Closure::bindTo/bind/
 * fromCallable are the first three methods carrying a C body directly, and this
 * pins the properties that must not differ from a bytecode method: the receiver
 * arrives as $this, a static one is dispatched without adopting the CALLER's
 * $this, and the thunks are gone from the global namespace. */
class NmdCounter { private $n = 41; public $tag = 'host'; }

/* instance native method: the receiver is the closure itself */
$peek = function () { return $this->n + 1; };
$bound = $peek->bindTo(new NmdCounter(), NmdCounter::class);
var_dump($bound());

/* static native method, called from GLOBAL scope */
$b2 = Closure::bind($peek, new NmdCounter(), NmdCounter::class);
var_dump($b2());

/* static native method, called from INSIDE an instance context. The method path
 * adopts the caller's $this when the target slot holds a class name rather than
 * an object -- harmless for a bytecode method, but a native one would then read
 * its argument list one slot off and bind the wrong closure. */
class NmdHost {
    public $tag = 'from-instance';
    public function make() {
        $c = function () { return $this->tag; };
        return Closure::bind($c, $this, self::class);
    }
}
var_dump((new NmdHost())->make()());

/* a native method reached from a PHP method of the same class (Closure::call
 * stays bytecode and now calls $this->bindTo() rather than the thunk) */
$g = function () { return $this->tag; };
var_dump($g->call(new NmdCounter()));

/* static native method taking a plain value */
$up = Closure::fromCallable('strtoupper');
var_dump($up('native'));

/* the global thunks the prelude used to forward to no longer exist */
var_dump(function_exists('__closure_bindTo'), function_exists('__closure_fromCallable'));

/* and the methods still look like ordinary internal methods to reflection */
$r = new ReflectionMethod('Closure', 'bindTo');
var_dump($r->isStatic(), $r->isPublic(), $r->isInternal(), $r->getDeclaringClass()->getName());
var_dump((new ReflectionMethod('Closure', 'bind'))->isStatic());
var_dump(method_exists('Closure', 'bindTo'), method_exists('Closure', 'fromCallable'));
?>
--EXPECT--
int(42)
int(42)
string(13) "from-instance"
string(4) "host"
string(6) "NATIVE"
bool(false)
bool(false)
bool(false)
bool(true)
bool(true)
string(7) "Closure"
bool(true)
bool(true)
bool(true)
--CLEAN--
<?php
