--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A callable that resolved to __callStatic is a static closure, one that resolved to __call is not
--FILE--
<?php
class CrbtMagic {
    public static function __callStatic($n, $a) { return "callStatic $n@" . static::class; }
    public function __call($n, $a) { return "call $n@" . get_class($this); }
    public static function fromStatic() { return self::gone(...); }
    public function fromInstance() { return [self::gone(...), $this->gone(...)]; }
}
class CrbtKid extends CrbtMagic {}

function crbt($label, $f) {
    $r = new ReflectionFunction($f);
    echo $label, ": isStatic=", var_export($r->isStatic(), true), "\n";
    $u = Closure::bind($f, null, CrbtMagic::class);
    echo "  unbind: ", $u === null ? "NULL" : $u(), "\n";
    $b = Closure::bind($f, new CrbtKid, CrbtMagic::class);
    echo "  bind: ", $b === null ? "NULL" : $b(), "\n";
    $n = $f->bindTo(null);
    echo "  bindTo(null): ", $n === null ? "NULL" : $n(), "\n";
    $k = Closure::bind($f, null, CrbtKid::class);
    echo "  rescope: ", $k === null ? "NULL" : "kept", "\n";
}

/* __callStatic's trampoline: no receiver to unbind, none may be bound */
crbt("A::gone(...)", CrbtMagic::gone(...));
crbt("fromCallable('A::gone')", Closure::fromCallable('CrbtMagic::gone'));
crbt("self::gone(...) static", CrbtMagic::fromStatic());
/* __call's trampoline keeps the receiver the way a method does */
[$self, $this_] = (new CrbtMagic)->fromInstance();
crbt("self::gone(...) instance", $self);
crbt("\$this->gone(...)", $this_);
crbt("[\$o,'gone']", Closure::fromCallable([new CrbtMagic, 'gone']));
?>
--EXPECTF--
A::gone(...): isStatic=true
  unbind: callStatic gone@CrbtMagic
PHP Warning:  Cannot bind an instance to a static closure, this will be an error in PHP 9 in %s on line %d
  bind: NULL
  bindTo(null): callStatic gone@CrbtMagic
PHP Warning:  Cannot rebind scope of closure created from method, this will be an error in PHP 9 in %s on line %d
  rescope: NULL
fromCallable('A::gone'): isStatic=true
  unbind: callStatic gone@CrbtMagic
PHP Warning:  Cannot bind an instance to a static closure, this will be an error in PHP 9 in %s on line %d
  bind: NULL
  bindTo(null): callStatic gone@CrbtMagic
PHP Warning:  Cannot rebind scope of closure created from method, this will be an error in PHP 9 in %s on line %d
  rescope: NULL
self::gone(...) static: isStatic=true
  unbind: callStatic gone@CrbtMagic
PHP Warning:  Cannot bind an instance to a static closure, this will be an error in PHP 9 in %s on line %d
  bind: NULL
  bindTo(null): callStatic gone@CrbtMagic
PHP Warning:  Cannot rebind scope of closure created from method, this will be an error in PHP 9 in %s on line %d
  rescope: NULL
self::gone(...) instance: isStatic=false
PHP Warning:  Cannot unbind $this of method, this will be an error in PHP 9 in %s on line %d
  unbind: NULL
  bind: call gone@CrbtKid
PHP Warning:  Cannot unbind $this of method, this will be an error in PHP 9 in %s on line %d
  bindTo(null): NULL
PHP Warning:  Cannot unbind $this of method, this will be an error in PHP 9 in %s on line %d
  rescope: NULL
$this->gone(...): isStatic=false
PHP Warning:  Cannot unbind $this of method, this will be an error in PHP 9 in %s on line %d
  unbind: NULL
  bind: call gone@CrbtKid
PHP Warning:  Cannot unbind $this of method, this will be an error in PHP 9 in %s on line %d
  bindTo(null): NULL
PHP Warning:  Cannot unbind $this of method, this will be an error in PHP 9 in %s on line %d
  rescope: NULL
[$o,'gone']: isStatic=false
PHP Warning:  Cannot unbind $this of method, this will be an error in PHP 9 in %s on line %d
  unbind: NULL
  bind: call gone@CrbtKid
PHP Warning:  Cannot unbind $this of method, this will be an error in PHP 9 in %s on line %d
  bindTo(null): NULL
PHP Warning:  Cannot unbind $this of method, this will be an error in PHP 9 in %s on line %d
  rescope: NULL
