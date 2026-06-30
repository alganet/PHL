--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Closure::fromCallable normalizes any callable to a Closure (idempotent on a Closure)
--FILE--
<?php
class Inc2FC {
  public $tag = "obj";
  function m($x){ return $this->tag . ":" . $x; }
  static function sm($x){ return "static:" . $x; }
}
class Inc2FCInv { function __invoke($x){ return "inv:" . $x; } }

$c1 = Closure::fromCallable("strlen");
echo var_export($c1 instanceof Closure, true), "\n";
echo $c1("hello"), "\n";

$o = new Inc2FC();
$c2 = Closure::fromCallable([$o, "m"]);
echo var_export($c2 instanceof Closure, true), "\n";
echo $c2("a"), "\n";

$c3 = Closure::fromCallable(["Inc2FC", "sm"]);
echo $c3("b"), "\n";

$iv = new Inc2FCInv();
$c4 = Closure::fromCallable($iv);
echo $c4("z"), "\n";

/* idempotent: passing a Closure returns a Closure (same callable) */
$cl = function ($x) { return "cl:" . $x; };
$c5 = Closure::fromCallable($cl);
echo var_export($c5 instanceof Closure, true), "\n";
echo $c5("q"), "\n";
?>
--EXPECT--
true
5
true
obj:a
static:b
inv:z
true
cl:q
--CLEAN--
<?php
