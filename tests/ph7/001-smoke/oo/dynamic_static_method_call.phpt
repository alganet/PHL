--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Dynamic static method call Class::$var() uses the variable's value as the method name
--FILE--
<?php
class DsmcCalc {
    public static function add(int $a, int $b): int { return $a + $b; }
    public static function mul(int $a, int $b): int { return $a * $b; }
    public static $store = 'a static property';
}
$op = 'add';
echo DsmcCalc::$op(2, 3), "\n";              // -> add
$op = 'mul';
echo DsmcCalc::$op(2, 3), "\n";              // -> mul

$cls = 'DsmcCalc';
$m = 'add';
echo $cls::$m(10, 20), "\n";                 // dynamic class + dynamic method

$obj = new DsmcCalc();
echo $obj::$m(1, 1), "\n";                    // dynamic method via instance

// A same-named static property is unaffected (no call).
echo DsmcCalc::$store, "\n";                  // static property access
?>
--EXPECT--
5
6
30
2
a static property
--CLEAN--
<?php
