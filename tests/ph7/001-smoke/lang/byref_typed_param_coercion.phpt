--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A typed by-reference parameter's coercion is what the reference holds
--DESCRIPTION--
php converts a weak-mode actual to the declared type, and a by-REFERENCE parameter
then holds the conversion — so `$v = 1.0; f($v);` with `function f(int &$x)` leaves
BOTH views int(1). PHL ran the declared-type check on the operand-stack copy while
the binder aliases the caller's slot by index, so the conversion reached neither the
callee (which reads through the alias) nor the caller: both stayed float, for every
pair of types and for the named, element, property and generator spellings alike.

The write-back is skipped whenever the check changed nothing — an untyped parameter,
or one the actual already satisfies, copies nothing.
--FILE--
<?php
function btcInt(int &$x)      { var_dump($x); }
function btcFloat(float &$y)  { var_dump($y); }
function btcString(string &$s){ var_dump($s); }
function btcBool(bool &$b)    { var_dump($b); }
function btcPlain(&$u)        { var_dump($u); }

echo "-- a whole float into int\n";
$a = 1.0; btcInt($a); var_dump($a);

echo "-- a numeric string into int\n";
$b = "5"; btcInt($b); var_dump($b);

echo "-- an int into float\n";
$c = 3; btcFloat($c); var_dump($c);

echo "-- an int into string\n";
$d = 42; btcString($d); var_dump($d);

echo "-- an int into bool\n";
$e = 1; btcBool($e); var_dump($e);

echo "-- an actual that already fits is untouched\n";
$f = 7; btcInt($f); var_dump($f);

echo "-- an UNTYPED by-reference parameter coerces nothing\n";
$g = 1.0; btcPlain($g); var_dump($g);

echo "-- an element actual\n";
$arr = ['k' => 2.0]; btcInt($arr['k']); var_dump($arr);

echo "-- a property actual\n";
class BtcHolder { public $p = 2.0; }
$o = new BtcHolder; btcInt($o->p); var_dump($o->p);

echo "-- a NAMED actual\n";
$h = 4.0; btcInt(x: $h); var_dump($h);

echo "-- a nullable and a union parameter\n";
function btcNullable(?int &$n) { var_dump($n); }
function btcUnion(int|string &$w) { var_dump($w); }
$i = 2.0; btcNullable($i); var_dump($i);
$j = 2.0; btcUnion($j); var_dump($j);

echo "-- the callee's own write still lands\n";
function btcWrite(int &$z) { $z = 99; }
$k = 1.0; btcWrite($k); var_dump($k);

echo "-- a GENERATOR's typed by-reference parameter\n";
function btcGen(int &$x) { var_dump($x); yield 1; }
$l = 5.0;
$it = btcGen($l);
var_dump($l);
$it->current();
var_dump($l);
?>
--EXPECT--
-- a whole float into int
int(1)
int(1)
-- a numeric string into int
int(5)
int(5)
-- an int into float
float(3)
float(3)
-- an int into string
string(2) "42"
string(2) "42"
-- an int into bool
bool(true)
bool(true)
-- an actual that already fits is untouched
int(7)
int(7)
-- an UNTYPED by-reference parameter coerces nothing
float(1)
float(1)
-- an element actual
int(2)
array(1) {
  ["k"]=>
  int(2)
}
-- a property actual
int(2)
int(2)
-- a NAMED actual
int(4)
int(4)
-- a nullable and a union parameter
int(2)
int(2)
int(2)
int(2)
-- the callee's own write still lands
int(99)
-- a GENERATOR's typed by-reference parameter
int(5)
int(5)
int(5)
--CLEAN--
<?php
