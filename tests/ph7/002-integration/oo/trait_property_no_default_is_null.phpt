--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An untyped trait property with no default composes against the same property's null
--DESCRIPTION--
An untyped property declared without a default holds null, the same null
`= null` stores, and php compares two definitions of one property by the
value each default holds. PHL treated the missing default as a different
definition and refused `public $p;` against `public $p = null;` --
against the class body, between two traits, and for a static alike.
--FILE--
<?php
trait TpnNull { public $a; public $b = null; public static $s; var $v; }
class TpnNullC { use TpnNull; public $a = null; public $b; public static $s = NULL; public $v = \null; }
var_dump(get_object_vars(new TpnNullC), TpnNullC::$s);

trait TpnX { public $p; public static $s = null; }
trait TpnY { public $p = null; public static $s; }
class TpnXY { use TpnX, TpnY; }
var_dump(get_object_vars(new TpnXY), TpnXY::$s);

trait TpnTyped { public ?int $t = null; public ?TpnXY $o = null; }
class TpnTypedC { use TpnTyped; public int|null $t = null; public ?tpnxy $o = null; }
var_dump(get_object_vars(new TpnTypedC));
?>
--EXPECT--
array(3) {
  ["a"]=>
  NULL
  ["b"]=>
  NULL
  ["v"]=>
  NULL
}
NULL
array(1) {
  ["p"]=>
  NULL
}
NULL
array(2) {
  ["t"]=>
  NULL
  ["o"]=>
  NULL
}
