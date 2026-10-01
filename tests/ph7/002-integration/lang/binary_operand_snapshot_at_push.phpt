--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A binary operator sees the operand it was given, and re-reads a plain variable
--FILE--
<?php
echo "-- a plain variable is read AT the operator\n";
$x = 'old';
var_dump($x . ($x = 'new'));
$n = 1;
var_dump($n - ($n = 5));
$n = 1;
var_dump($n < ($n = 5));
$n = 1;
var_dump($n <=> ($n = 5));

echo "-- ...and so is a parenthesised one, and one reached through a reference\n";
$x = 'old';
var_dump(($x) . ($x = 'new'));
$x = 'old';
$r = &$x;
var_dump($r . ($x = 'new'));

echo "-- every other operand is the value it had when it was pushed\n";
$a = ['old'];
var_dump($a[0] . ($a[0] = 'new'));
class P { public $p = 'old'; public static $s = 'old'; }
$o = new P();
var_dump($o->p . ($o->p = 'new'));
var_dump(P::$s . (P::$s = 'new'));
$x = 'old';
$nm = 'x';
var_dump($$nm . ($x = 'new'));
$x = 'old';
var_dump((true ? $x : 'z') . ($x = 'new'));
$x = 'old';
var_dump(((string)$x) . ($x = 'new'));

echo "-- the writer does not have to be an assignment\n";
$a = ['old'];
function bump(&$r) { $r = 'new'; return 'B'; }
var_dump($a[0] . bump($a[0]));
$a = ['old'];
var_dump($a[0] . strtoupper($a[0] = 'new'));

echo "-- a longer assignment used to be read through the old length\n";
$a = ['old'];
var_dump($a[0] . ($a[0] = 'muchlonger'));
$x = 'ab';
var_dump(strlen($x . ($x = str_repeat('c', 4000))));

echo "-- a chain reads left to right\n";
$x = 'a';
var_dump($x . ($x = 'b') . ($x = 'c'));
$n = 1;
var_dump($n + ($n = 2) + ($n = 3));

echo "-- nothing runs on the right, nothing is copied\n";
$x = 'old';
var_dump($x . $x);
$a = ['old'];
var_dump($a[0] . $a[0]);
?>
--EXPECT--
-- a plain variable is read AT the operator
string(6) "newnew"
int(0)
bool(false)
int(0)
-- ...and so is a parenthesised one, and one reached through a reference
string(6) "newnew"
string(6) "newnew"
-- every other operand is the value it had when it was pushed
string(6) "oldnew"
string(6) "oldnew"
string(6) "oldnew"
string(6) "oldnew"
string(6) "oldnew"
string(6) "oldnew"
-- the writer does not have to be an assignment
string(4) "oldB"
string(6) "oldNEW"
-- a longer assignment used to be read through the old length
string(13) "oldmuchlonger"
int(8000)
-- a chain reads left to right
string(3) "bbc"
int(7)
-- nothing runs on the right, nothing is copied
string(6) "oldold"
string(6) "oldold"
