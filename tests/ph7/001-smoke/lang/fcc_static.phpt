--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
First-class callable of a static method: Cls::m(...) and self/parent/static
--FILE--
<?php
class FccBase {
  static function who(){ return "base"; }
  function selfWho(){ $f = self::who(...); return $f(); }
}
class FccChild extends FccBase {
  static function who(){ return "child"; }
  function parentWho(){ $f = parent::who(...); return $f(); }
  function staticWho(){ $f = static::who(...); return $f(); }
}
$f = FccBase::who(...);
echo $f(), "\n";
$b = new FccBase();
echo $b->selfWho(), "\n";
$c = new FccChild();
echo $c->parentWho(), "\n";
echo $c->staticWho(), "\n";
?>
--EXPECT--
base
base
base
child
--CLEAN--
<?php
