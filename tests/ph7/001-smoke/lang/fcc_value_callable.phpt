--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
First-class callable over an arbitrary callable VALUE ($expr)(...) normalizes to a Closure
--FILE--
<?php
class FccVal {
  public $tag = "obj";
  function m($x){ return $this->tag . ":" . $x; }
  static function sm($x){ return "static:" . $x; }
}
class FccValInv { function __invoke($x){ return "inv:" . $x; } }

/* string function-name callable */
$f = "strlen";
$c = $f(...);
echo var_export($c instanceof Closure, true), "\n";
echo $c("hi"), "\n";

/* [object, method] array callable -> binds $this */
$o = new FccVal();
$cb = [$o, "m"];
$c2 = $cb(...);
echo var_export($c2 instanceof Closure, true), "\n";
echo $c2("a"), "\n";

/* [class-name, method] array callable -> static scope */
$cb2 = ["FccVal", "sm"];
$c3 = $cb2(...);
echo var_export($c3 instanceof Closure, true), "\n";
echo $c3("b"), "\n";

/* __invoke object */
$iv = new FccValInv();
$c4 = $iv(...);
echo var_export($c4 instanceof Closure, true), "\n";
echo $c4("z"), "\n";

/* an existing closure is idempotent: same instance back */
$cl = function ($x) { return "cl:" . $x; };
$c5 = ($cl)(...);
echo var_export($c5 instanceof Closure, true), "\n";
echo var_export($c5 === $cl, true), "\n";
echo $c5("q"), "\n";

/* a normalized value callable round-trips through the usual callback machinery */
echo implode(",", array_map($cb(...), ["x", "y"])), "\n";
?>
--EXPECT--
true
2
true
obj:a
true
static:b
true
inv:z
true
true
cl:q
obj:x,obj:y
--CLEAN--
<?php
