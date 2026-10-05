--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A trait property default spelled differently but holding the same value composes
--DESCRIPTION--
php compares two definitions of one property by the VALUE each default folded
to, with ===. An int written into a float property was converted when the
default was checked, so `float $p = 1` and `float $p = 1.0` both hold float(1),
and `1+1` folds to the same 2 as `2`. PHL compared the compiled instructions and
refused both compositions.
--FILE--
<?php
trait TpdsFloat { public float $a = 1; public static float $s = 1; public ?float $n = 2; }
class TpdsFloatC { use TpdsFloat; public float $a = 1.0; public static float $s = 1.0; public ?float $n = 2.0; }
var_dump((new TpdsFloatC)->a, TpdsFloatC::$s, (new TpdsFloatC)->n);

trait TpdsX { public float $a = 1.0; public float|string $u = 3.0; }
trait TpdsY { public float $a = 1; public float|string $u = 3; }
class TpdsXY { use TpdsX, TpdsY; }
var_dump((new TpdsXY)->a, (new TpdsXY)->u);

trait TpdsExpr { public $e = 1+1; public $s = "a"."b"; public $m = [1+1, "k" => -2]; public float $f = 2*3; }
class TpdsExprC { use TpdsExpr; public $e = 2; public $s = "ab"; public $m = [2, "k" => -2]; public float $f = 6.0; }
var_dump((new TpdsExprC)->e, (new TpdsExprC)->s, (new TpdsExprC)->m);

trait TpdsConst { const K = 2 * 2; }
class TpdsConstC { use TpdsConst; const K = 4; }
var_dump(TpdsConstC::K);
?>
--EXPECT--
float(1)
float(1)
float(2)
float(1)
float(3)
int(2)
string(2) "ab"
array(2) {
  [0]=>
  int(2)
  ["k"]=>
  int(-2)
}
int(4)
