--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
First-class callable with a dynamic method/class name ($o->$m(...), C::$m(...), $cls::m(...))
--FILE--
<?php
class FccDyn {
  public $v = 5;
  function im($x){ return $this->v + $x; }
  static function sm($x){ return $x * 100; }
}
$o = new FccDyn();
$m = "im"; $f = $o->$m(...); echo $f(3), "\n";
$s = "sm"; $g = FccDyn::$s(...); echo $g(4), "\n";
$cls = "FccDyn"; $h = $cls::sm(...); echo $h(2), "\n";
?>
--EXPECT--
8
400
200
--CLEAN--
<?php
