--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_walk()/array_walk_recursive() walk an object's property table
--FILE--
<?php
class AwPlain {
    public $a = 1;
    protected $b = 2;
    private $c = 3;
    public int $t = 7;
    public int $u;
    public static $s = 9;
}

echo "== keys and values ==\n";
$awO = new AwPlain();
var_dump(array_walk($awO, function ($v, $k) { echo bin2hex($k), " => ", var_export($v, true), "\n"; }));

echo "== the third argument ==\n";
array_walk($awO, function ($v, $k, $arg) { echo "$arg:", bin2hex($k), " "; }, 'x');
echo "\n";

echo "== write back through the slot ==\n";
class AwWrite { public $x = 1; protected $y = 2; }
$awW = new AwWrite();
array_walk($awW, function (&$v, $k) { $v = $v * 10; });
print_r($awW);

echo "== a typed property enforces its type on the write ==\n";
class AwTyped { public int $n = 1; public ?string $s = null; }
$awT = new AwTyped();
array_walk($awT, function (&$v, $k) { if ($k === 'n') { $v = '42'; } else { $v = 5; } });
print_r($awT);
$awT2 = new AwTyped();
try {
    array_walk($awT2, function (&$v, $k) { echo "visit $k\n"; $v = 'no'; });
} catch (\Throwable $e) {
    echo get_class($e), ": ", $e->getMessage(), "\n";
}
print_r($awT2);

echo "== the body may unset ==\n";
class AwUnset { public $p = 1; public $q = 2; public $r = 3; }
$awU = new AwUnset();
array_walk($awU, function ($v, $k) use ($awU) { echo "visit $k\n"; if ($k === 'p') { unset($awU->q); } });

echo "== nested walks are independent ==\n";
$awN = new AwUnset();
array_walk($awN, function ($v, $k) use ($awN) {
    echo "outer $k\n";
    array_walk($awN, function ($v2, $k2) { echo "  inner $k2\n"; });
});

echo "== recursive descends into arrays only ==\n";
class AwRec { public $arr = [1, [2, 3]]; public $obj; public $z = 4; }
$awR = new AwRec();
$awR->obj = new AwUnset();
var_dump(array_walk_recursive($awR, function ($v, $k) { echo $k, " => ", gettype($v), "\n"; }));

echo "== an internal object with no property table ==\n";
$awA = new ArrayObject([1, 2, 3]);
var_dump(array_walk($awA, function ($v, $k) { echo "seen\n"; }));

echo "== what is still not an object or an array ==\n";
foreach ([null, 'abc', 1, 1.5, true] as $awBad) {
    try { array_walk($awBad, function ($v, $k) {}); }
    catch (\Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
    try { array_walk_recursive($awBad, function ($v, $k) {}); }
    catch (\Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
}
?>
--EXPECT--
== keys and values ==
61 => 1
002a0062 => 2
004177506c61696e0063 => 3
74 => 7
bool(true)
== the third argument ==
x:61 x:002a0062 x:004177506c61696e0063 x:74 
== write back through the slot ==
AwWrite Object
(
    [x] => 10
    [y:protected] => 20
)
== a typed property enforces its type on the write ==
AwTyped Object
(
    [n] => 42
    [s] => 5
)
visit n
TypeError: Cannot assign string to reference held by property AwTyped::$n of type int
AwTyped Object
(
    [n] => 1
    [s] => 
)
== the body may unset ==
visit p
visit r
== nested walks are independent ==
outer p
  inner p
  inner q
  inner r
outer q
  inner p
  inner q
  inner r
outer r
  inner p
  inner q
  inner r
== recursive descends into arrays only ==
0 => integer
0 => integer
1 => integer
obj => object
z => integer
bool(true)
== an internal object with no property table ==
bool(true)
== what is still not an object or an array ==
TypeError: array_walk(): Argument #1 ($array) must be of type array, null given
TypeError: array_walk_recursive(): Argument #1 ($array) must be of type array, null given
TypeError: array_walk(): Argument #1 ($array) must be of type array, string given
TypeError: array_walk_recursive(): Argument #1 ($array) must be of type array, string given
TypeError: array_walk(): Argument #1 ($array) must be of type array, int given
TypeError: array_walk_recursive(): Argument #1 ($array) must be of type array, int given
TypeError: array_walk(): Argument #1 ($array) must be of type array, float given
TypeError: array_walk_recursive(): Argument #1 ($array) must be of type array, float given
TypeError: array_walk(): Argument #1 ($array) must be of type array, true given
TypeError: array_walk_recursive(): Argument #1 ($array) must be of type array, true given
--CLEAN--
<?php
unset($awO, $awW, $awT, $awT2, $awU, $awN, $awR, $awA, $awBad);
