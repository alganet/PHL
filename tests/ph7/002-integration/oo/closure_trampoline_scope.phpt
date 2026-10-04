--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A call trampoline closure is scoped to the class that declared its catch-all
--FILE--
<?php
class CtsInst {
    public function __call($n, $a) { return "call $n@" . static::class; }
    public function mk() { return $this->gone(...); }
}
class CtsStat extends CtsInst {
    public static function __callStatic($n, $a) { return "callStatic $n@" . static::class; }
}
class CtsKid extends CtsStat {}
trait CtsCatch {
    public static function __callStatic($n, $a) { return "trait $n@" . static::class; }
}
class CtsUser { use CtsCatch; }
class CtsUserKid extends CtsUser {}

function cts($label, $f, $this_) {
    $r = new ReflectionFunction($f);
    echo $label, ": ", $f(), " scope=", $r->getClosureScopeClass()?->name,
        " called=", $r->getClosureCalledClass()?->name, "\n";
    foreach ([CtsInst::class, CtsStat::class, CtsKid::class] as $s) {
        $b = @Closure::bind($f, $this_, $s);
        echo "  bind to $s: ", $b === null ? "NULL" : $b(), "\n";
    }
}

/* __callStatic declared by the middle class, reached through the leaf */
cts("CtsKid::gone(...)", CtsKid::gone(...), null);
/* __call declared by the root, reached through a leaf instance */
cts("\$kid->mk()", (new CtsKid)->mk(), new CtsKid);
cts("fromCallable([\$kid, 'gone'])", Closure::fromCallable([new CtsKid, 'gone']), new CtsKid);
cts("fromCallable(['CtsKid', 'gone'])", Closure::fromCallable(['CtsKid', 'gone']), null);

/* a trait's catch-all is composed into the using class */
$f = CtsUserKid::gone(...);
$r = new ReflectionFunction($f);
echo "CtsUserKid::gone(...): ", $f(), " scope=", $r->getClosureScopeClass()?->name, "\n";
$b = Closure::bind($f, null, CtsUser::class);
echo "  bind to CtsUser: ", $b === null ? "NULL" : $b(), "\n";
--EXPECT--
CtsKid::gone(...): callStatic gone@CtsKid scope=CtsStat called=CtsKid
  bind to CtsInst: NULL
  bind to CtsStat: callStatic gone@CtsStat
  bind to CtsKid: NULL
$kid->mk(): call gone@CtsKid scope=CtsInst called=CtsKid
  bind to CtsInst: call gone@CtsKid
  bind to CtsStat: NULL
  bind to CtsKid: NULL
fromCallable([$kid, 'gone']): call gone@CtsKid scope=CtsInst called=CtsKid
  bind to CtsInst: call gone@CtsKid
  bind to CtsStat: NULL
  bind to CtsKid: NULL
fromCallable(['CtsKid', 'gone']): callStatic gone@CtsKid scope=CtsStat called=CtsKid
  bind to CtsInst: NULL
  bind to CtsStat: callStatic gone@CtsStat
  bind to CtsKid: NULL
CtsUserKid::gone(...): trait gone@CtsUserKid scope=CtsUser
  bind to CtsUser: trait gone@CtsUser
