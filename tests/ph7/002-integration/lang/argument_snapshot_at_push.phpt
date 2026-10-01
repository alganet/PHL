--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A by-value argument keeps the value it had when it was passed
--FILE--
<?php
function show(...$a) { foreach ($a as $v) { var_dump($v); } }

echo "-- plain variable\n";
$x = 'first';
show($x, $x = 'secnd');

echo "-- the assignment is shorter\n";
$x = 'aaaaaaaaaaaa';
show($x, $x = 'bb');

echo "-- array element\n";
$a = ['first'];
show($a[0], $a[0] = 'secnd');

echo "-- property\n";
class P { public $p = 'first'; }
$o = new P();
show($o->p, $o->p = 'secnd');

echo "-- static property\n";
class Q { public static $s = 'first'; }
show(Q::$s, Q::$s = 'secnd');

echo "-- written through a reference by a later argument\n";
function bump(&$r) { $r = 'secnd'; return 1; }
$x = 'first';
show($x, bump($x));

echo "-- a nested call is the writer\n";
$x = 'first';
show($x, strtoupper($x = 'secnd'));

echo "-- method call\n";
class M { public function m($a, $b) { var_dump($a, $b); } }
$x = 'first';
(new M())->m($x, $x = 'secnd');

echo "-- static method call\n";
class N { public static function m($a, $b) { var_dump($a, $b); } }
$x = 'first';
N::m($x, $x = 'secnd');

echo "-- closure call\n";
$x = 'first';
(function ($a, $b) { var_dump($a, $b); })($x, $x = 'secnd');

echo "-- inside a function body\n";
function inner() { $x = 'first'; show($x, $x = 'secnd'); }
inner();

echo "-- three arguments, the writer is last\n";
$x = 'first';
$y = 'other';
show($x, $y, $x = $y = 'secnd');

echo "-- an unwritten argument is still itself\n";
$x = 'first';
show($x, strlen($x), $x = 'secnd');
?>
--EXPECT--
-- plain variable
string(5) "first"
string(5) "secnd"
-- the assignment is shorter
string(12) "aaaaaaaaaaaa"
string(2) "bb"
-- array element
string(5) "first"
string(5) "secnd"
-- property
string(5) "first"
string(5) "secnd"
-- static property
string(5) "first"
string(5) "secnd"
-- written through a reference by a later argument
string(5) "first"
int(1)
-- a nested call is the writer
string(5) "first"
string(5) "SECND"
-- method call
string(5) "first"
string(5) "secnd"
-- static method call
string(5) "first"
string(5) "secnd"
-- closure call
string(5) "first"
string(5) "secnd"
-- inside a function body
string(5) "first"
string(5) "secnd"
-- three arguments, the writer is last
string(5) "first"
string(5) "other"
string(5) "secnd"
-- an unwritten argument is still itself
string(5) "first"
int(5)
string(5) "secnd"
