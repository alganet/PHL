--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An unpacked argument binds the array element by reference, as php binds it
--FILE--
<?php
error_reporting(E_ALL);
set_error_handler(function ($no, $str) { echo "  [$no] $str\n"; return true; });

function r(&$x) { $x = 'W'; return 'ok'; }
function r2($m, &$n) { $n = 'W'; }
function v($x) { $x = 'CHANGED'; }

echo "== the element is written back ==\n";
$a = ['v'];
var_dump(r(...$a));
var_dump($a);

echo "== the by-ref parameter picks its own position ==\n";
$b = ['p', 'q'];
r2(...$b);
var_dump($b);

echo "== a builtin too ==\n";
$c = [[3, 1, 2]];
sort(...$c);
var_dump($c);

echo "== a by-VALUE parameter still gets a copy ==\n";
$d = ['keep'];
v(...$d);
var_dump($d);

echo "== a SHARED source array separates first ==\n";
$e = ['e0'];
$f = $e;
r(...$e);
var_dump($e, $f);

echo "== a TEMPORARY source is accepted, and writes nowhere ==\n";
var_dump(r(...['t']));
var_dump(r(...[1 + 1]));

echo "== only a plain VARIABLE source writes back, as in php ==\n";
$n = [['x']];        r(...$n[0]);   var_dump($n);
$o = new stdClass; $o->q = ['y'];  r(...$o->q);  var_dump($o->q);
class T { public static $s = ['z']; }
r(...T::$s);        var_dump(T::$s);
$p = ['p'];         r(...(array)$p); var_dump($p);
$q = ['q'];         $s = &$q; r(...$s); var_dump($q);

echo "== ordinary spreads are unaffected ==\n";
$g = [1, 2, 3];
var_dump(array_sum([...$g]));
$h = ['a', 'b'];
var_dump(sprintf('%s-%s', ...$h));
$i = [[1, 2], [3]];
var_dump(count(array_merge(...$i)));
var_dump(implode(',', [...[], 'z']));
?>
--EXPECT--
== the element is written back ==
string(2) "ok"
array(1) {
  [0]=>
  string(1) "W"
}
== the by-ref parameter picks its own position ==
array(2) {
  [0]=>
  string(1) "p"
  [1]=>
  string(1) "W"
}
== a builtin too ==
array(1) {
  [0]=>
  array(3) {
    [0]=>
    int(1)
    [1]=>
    int(2)
    [2]=>
    int(3)
  }
}
== a by-VALUE parameter still gets a copy ==
array(1) {
  [0]=>
  string(4) "keep"
}
== a SHARED source array separates first ==
array(1) {
  [0]=>
  string(1) "W"
}
array(1) {
  [0]=>
  string(2) "e0"
}
== a TEMPORARY source is accepted, and writes nowhere ==
string(2) "ok"
string(2) "ok"
== only a plain VARIABLE source writes back, as in php ==
array(1) {
  [0]=>
  array(1) {
    [0]=>
    string(1) "x"
  }
}
array(1) {
  [0]=>
  string(1) "y"
}
array(1) {
  [0]=>
  string(1) "z"
}
array(1) {
  [0]=>
  string(1) "p"
}
array(1) {
  [0]=>
  string(1) "W"
}
== ordinary spreads are unaffected ==
int(6)
string(3) "a-b"
int(3)
string(1) "z"
