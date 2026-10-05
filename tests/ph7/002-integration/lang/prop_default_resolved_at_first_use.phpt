--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A typed property default the compiler could not fold is held to its type when the class first resolves
--FILE--
<?php
const K = 1;
function t(callable $f) {
    try { $f(); } catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
}

echo "-- before anything resolves the class, the default is the constant's own int\n";
class A { public float $f = K; public ?float $n = K; public float|string $u = K; public int|float $i = K; public static float $s = K; }
var_dump((new ReflectionProperty('A', 'f'))->getDefaultValue());
echo "-- get_class_vars() resolves it, and every reader sees the float after\n";
var_dump(get_class_vars('A'));
var_dump((new ReflectionProperty('A', 'f'))->getDefaultValue());
var_dump((new ReflectionProperty('A', 'n'))->getDefaultValue());
echo "-- a static's default is not rewritten; its live slot is\n";
var_dump((new ReflectionProperty('A', 's'))->getDefaultValue(), A::$s);
echo (new ReflectionProperty('A', 'f'));

echo "-- getDefaultProperties() resolves too\n";
class B { public float $f = K; }
var_dump((new ReflectionClass('B'))->getDefaultProperties());

echo "-- so does a static access, and `new` of a subclass resolves its parent\n";
class C { public float $f = K; public static $s = 1; }
C::$s;
var_dump((new ReflectionProperty('C', 'f'))->getDefaultValue());
class D { public float $f = K; } class E extends D {}
new E;
var_dump((new ReflectionProperty('D', 'f'))->getDefaultValue());

echo "-- a default its type refuses throws wherever the class resolves\n";
class Bad { public string $s = K; public static $x = 1; }
t(fn() => get_class_vars('Bad'));
t(fn() => (new ReflectionClass('Bad'))->getDefaultProperties());
t(fn() => Bad::$x);
t(fn() => Bad::$x);
t(fn() => new Bad);
var_dump((new ReflectionProperty('Bad', 's'))->getDefaultValue());

echo "-- the instance defaults answer before the statics, the parent's before the child's\n";
class Both { public static string $st = K; public string $in = K; }
t(fn() => Both::$st);
class P { public string $p = K; } class Q extends P { public string $q = K; }
t(fn() => get_class_vars('Q'));
?>
--EXPECT--
-- before anything resolves the class, the default is the constant's own int
int(1)
-- get_class_vars() resolves it, and every reader sees the float after
array(5) {
  ["f"]=>
  float(1)
  ["n"]=>
  float(1)
  ["u"]=>
  float(1)
  ["i"]=>
  int(1)
  ["s"]=>
  int(1)
}
float(1)
float(1)
-- a static's default is not rewritten; its live slot is
int(1)
float(1)
Property [ public float $f = 1.0 ]
-- getDefaultProperties() resolves too
array(1) {
  ["f"]=>
  float(1)
}
-- so does a static access, and `new` of a subclass resolves its parent
float(1)
float(1)
-- a default its type refuses throws wherever the class resolves
TypeError: Cannot assign int to property Bad::$s of type string
TypeError: Cannot assign int to property Bad::$s of type string
TypeError: Cannot assign int to property Bad::$s of type string
TypeError: Cannot assign int to property Bad::$s of type string
TypeError: Cannot assign int to property Bad::$s of type string
int(1)
-- the instance defaults answer before the statics, the parent's before the child's
TypeError: Cannot assign int to property Both::$in of type string
TypeError: Cannot assign int to property P::$p of type string
