--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Closure::fromCallable([$obj,'A::m']) over a name only a catch-all answers wraps __callStatic, unbound
--DESCRIPTION--
php resolves a method half that names another class through THAT class's static-method
fallback, asked from fromCallable's own frame, which has no `$this`. So the catch-all is
__callStatic even when the class also declares __call, and the closure is static: no
bound object, scoped to the class that was named, with static:: the object's class. A
class with only __call has no such fallback and is refused. The closure used to keep the
object and run __call on it, or die at the call when only __callStatic was declared.
--FILE--
<?php
set_error_handler(fn($n) => $n === E_DEPRECATED);
class FcqS { static function __callStatic($n, $a) { echo "FcqS::__callStatic $n ", static::class, "\n"; } }
class FcqB {
    function __call($n, $a) { echo "FcqB->__call $n\n"; }
    static function __callStatic($n, $a) { echo "FcqB::__callStatic $n ", static::class, "\n"; }
}
class FcqC { function __call($n, $a) { echo "FcqC->__call $n\n"; } }
class FcqDS extends FcqS {}
class FcqDB extends FcqB {}
class FcqDC extends FcqC {}
foreach ([new FcqDS, new FcqDB, new FcqDC] as $o) {
    $p = get_parent_class($o);
    foreach ([[$o, "$p::zz"], [$o, 'parent::zz']] as $cb) {
        echo get_class($o), " ", $cb[1], ": ";
        try {
            $f = Closure::fromCallable($cb);
            $f(1);
            $r = new ReflectionFunction($f);
            var_dump($r->isStatic(), $r->getClosureThis(),
                $r->getClosureScopeClass()->getName(), $r->getClosureCalledClass()->getName());
        } catch (TypeError $e) {
            echo $e->getMessage(), "\n";
        }
    }
}
--EXPECT--
FcqDS FcqS::zz: FcqS::__callStatic zz FcqDS
bool(true)
NULL
string(4) "FcqS"
string(5) "FcqDS"
FcqDS parent::zz: FcqS::__callStatic zz FcqDS
bool(true)
NULL
string(4) "FcqS"
string(5) "FcqDS"
FcqDB FcqB::zz: FcqB::__callStatic zz FcqDB
bool(true)
NULL
string(4) "FcqB"
string(5) "FcqDB"
FcqDB parent::zz: FcqB::__callStatic zz FcqDB
bool(true)
NULL
string(4) "FcqB"
string(5) "FcqDB"
FcqDC FcqC::zz: Failed to create closure from callable: class FcqC does not have a method "zz"
FcqDC parent::zz: Failed to create closure from callable: class FcqC does not have a method "zz"
