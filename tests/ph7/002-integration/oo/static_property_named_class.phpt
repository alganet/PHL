--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A static property named $class is a property, not the ::class pseudo-constant
--FILE--
<?php
class A { public static $class = 'sv'; public static $x = 1; }
var_dump(A::$class);
$c = 'A'; var_dump($c::$class);
$o = new A; var_dump($o::$class);
A::$class = 'nv'; var_dump(A::$class);
var_dump(A::class, $o::class);
$r = &A::$class; $r = 'ref'; var_dump(A::$class);
class A2 { public static $class = ['k'=>1]; public static function f(){ var_dump(self::$class, static::$class, static::class, self::class); } }
class B2 extends A2 { public static function g(){ var_dump(parent::$class, parent::class); } }
class N2 {}
$n = 'class';
var_dump(A2::$$n, A2::${'class'}, B2::$class);
A2::f(); B2::g();
A2::$class['k']++; var_dump(A2::$class['k']);
A2::$class[] = 2; var_dump(count(A2::$class));
var_dump(A2::$class['zz'] ?? 'def', empty(A2::$class), isset(N2::$class), N2::$class ?? 'nd');
var_dump(A2::CLASS, (new A2)::class);
try { var_dump(N2::$class); } catch (Error $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
try { unset(A2::$class); } catch (Error $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
try { var_dump($n::$class); } catch (Error $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
$f = fn() => A2::$class['k']; var_dump($f());
function &r2() { return A2::$class; } $x = &r2(); $x = 'byref'; var_dump(A2::$class);
var_dump((new ReflectionClass('A2'))->getStaticPropertyValue('class'));
--EXPECT--
string(2) "sv"
string(2) "sv"
string(2) "sv"
string(2) "nv"
string(1) "A"
string(1) "A"
string(3) "ref"
array(1) {
  ["k"]=>
  int(1)
}
array(1) {
  ["k"]=>
  int(1)
}
array(1) {
  ["k"]=>
  int(1)
}
array(1) {
  ["k"]=>
  int(1)
}
array(1) {
  ["k"]=>
  int(1)
}
string(2) "A2"
string(2) "A2"
array(1) {
  ["k"]=>
  int(1)
}
string(2) "A2"
int(2)
int(2)
string(3) "def"
bool(false)
bool(false)
string(2) "nd"
string(2) "A2"
string(2) "A2"
Error: Access to undeclared static property N2::$class
Error: Attempt to unset static property A2::$class
Error: Class "class" not found
int(2)
string(5) "byref"
string(5) "byref"
