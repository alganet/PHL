--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Power-assignment operator **= (PHP 5.6)
--FILE--
<?php
$a = 2;
$a **= 10;
echo $a, "\n";
echo is_int($a) ? "int\n" : "float\n";

$b = 3;
$b **= 0;
echo $b, "\n";
echo is_int($b) ? "int\n" : "float\n";

$c = 2;
$c **= 3;
$c **= 2;
echo $c, "\n";
echo is_int($c) ? "int\n" : "float\n";

$d = 10;
$d **= 3;
echo $d, "\n";
echo is_int($d) ? "int\n" : "float\n";

$e = 2.5;
$e **= 2;
echo $e, "\n";
echo is_float($e) ? "float\n" : "int\n";

$arr = [2, 3];
$arr[0] **= 3;
echo $arr[0], "\n";
echo is_int($arr[0]) ? "int\n" : "float\n";

class PowTestHost { public int $n = 2; }
$o = new PowTestHost();
$o->n **= 4;
echo $o->n, "\n";
echo is_int($o->n) ? "int\n" : "float\n";
?>
--EXPECT--
1024
int
1
int
64
int
1000
int
6.25
float
8
int
16
int
--CLEAN--
<?php
unset($a, $b, $c, $d, $e, $arr, $o);
