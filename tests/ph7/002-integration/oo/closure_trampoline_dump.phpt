--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A call trampoline closure dumps its function, receiver and signature
--FILE--
<?php
class CtdBase {
    public function __call($n, $a) { return "call $n"; }
    public static function __callStatic($n, $a) { return "callStatic $n"; }
    public function inside() { return static::viaStatic(...); }
}
class CtdKid extends CtdBase {}

$o = new CtdKid;
echo "-- the (...) syntax: one variadic \$arguments\n";
var_dump($o->gone(...));
var_dump(CtdKid::gone(...));
var_dump($o->inside());
$f = [$o, 'dyn'];
var_dump($f(...));
$s = 'CtdKid::dynStatic';
var_dump($s(...));
echo "-- fromCallable: no parameters\n";
var_dump(Closure::fromCallable([$o, 'gone']));
var_dump(Closure::fromCallable('CtdKid::gone'));
echo "-- a rebind keeps what minted it\n";
var_dump($o->gone(...)->bindTo(new CtdBase));
var_dump(Closure::fromCallable([$o, 'gone'])->bindTo(new CtdBase));
echo "-- a real method callable is unchanged\n";
var_dump($o->inside(...));
--EXPECTF--
-- the (...) syntax: one variadic $arguments
object(Closure)#%d (3) {
  ["function"]=>
  string(13) "CtdBase::gone"
  ["this"]=>
  object(CtdKid)#%d (0) {
  }
  ["parameter"]=>
  array(1) {
    ["$arguments"]=>
    string(10) "<optional>"
  }
}
object(Closure)#%d (2) {
  ["function"]=>
  string(13) "CtdBase::gone"
  ["parameter"]=>
  array(1) {
    ["$arguments"]=>
    string(10) "<optional>"
  }
}
object(Closure)#%d (3) {
  ["function"]=>
  string(18) "CtdBase::viaStatic"
  ["this"]=>
  object(CtdKid)#%d (0) {
  }
  ["parameter"]=>
  array(1) {
    ["$arguments"]=>
    string(10) "<optional>"
  }
}
object(Closure)#%d (3) {
  ["function"]=>
  string(12) "CtdBase::dyn"
  ["this"]=>
  object(CtdKid)#%d (0) {
  }
  ["parameter"]=>
  array(1) {
    ["$arguments"]=>
    string(10) "<optional>"
  }
}
object(Closure)#%d (2) {
  ["function"]=>
  string(18) "CtdBase::dynStatic"
  ["parameter"]=>
  array(1) {
    ["$arguments"]=>
    string(10) "<optional>"
  }
}
-- fromCallable: no parameters
object(Closure)#%d (2) {
  ["function"]=>
  string(13) "CtdBase::gone"
  ["this"]=>
  object(CtdKid)#%d (0) {
  }
}
object(Closure)#%d (1) {
  ["function"]=>
  string(13) "CtdBase::gone"
}
-- a rebind keeps what minted it
object(Closure)#%d (3) {
  ["function"]=>
  string(13) "CtdBase::gone"
  ["this"]=>
  object(CtdBase)#%d (0) {
  }
  ["parameter"]=>
  array(1) {
    ["$arguments"]=>
    string(10) "<optional>"
  }
}
object(Closure)#%d (2) {
  ["function"]=>
  string(13) "CtdBase::gone"
  ["this"]=>
  object(CtdBase)#%d (0) {
  }
}
-- a real method callable is unchanged
object(Closure)#%d (2) {
  ["function"]=>
  string(15) "CtdBase::inside"
  ["this"]=>
  object(CtdKid)#%d (0) {
  }
}
