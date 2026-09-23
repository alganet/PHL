--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A by-value return is a temporary and does not alias the call's argument
--FILE--
<?php
error_reporting(E_ALL);
set_error_handler(function ($no, $str) { echo "  [$no] $str\n"; return true; });

function r(&$x) { $x = 'W'; return 'ok'; }
function id($q) { return $q; }
function &refReturn() { static $v = 'R'; return $v; }
class K {
    public $p = 1;
    public function m($q) { return $q; }
    public static function sm($q) { return $q; }
}

echo "== the argument of the INNER call survives ==\n";
$a = 'a'; r(id($a));                 var_dump($a);
$b = 'b'; r(strtoupper($b));         var_dump($b);
$c = 'c'; r((new K)->m($c));         var_dump($c);
$d = 'd'; r(K::sm($d));              var_dump($d);
$e = 'e'; $f = fn($q) => $q; r($f($e)); var_dump($e);
$g = 'g'; r(id(id($g)));             var_dump($g);
$h = [3, 1]; array_pop(id($h));      var_dump(count($h));

echo "== a by-REFERENCE return still binds ==\n";
$r = &refReturn(); $r = 'Q';
var_dump(refReturn());

echo "== ordinary returns keep working ==\n";
$o = new K;
var_dump(id($o) === $o);
$arr = [1, 2, 3];
$copy = id($arr); $copy[] = 9;
var_dump(count($arr), count($copy));
$n = 5; $m = id($n); $m++;
var_dump($n, $m);
var_dump(strlen(id('abcd')));
var_dump(array_sum(id($arr)));
?>
--EXPECT--
== the argument of the INNER call survives ==
  [8] Only variables should be passed by reference
string(1) "a"
  [8] Only variables should be passed by reference
string(1) "b"
  [8] Only variables should be passed by reference
string(1) "c"
  [8] Only variables should be passed by reference
string(1) "d"
  [8] Only variables should be passed by reference
string(1) "e"
  [8] Only variables should be passed by reference
string(1) "g"
  [8] Only variables should be passed by reference
int(2)
== a by-REFERENCE return still binds ==
string(1) "Q"
== ordinary returns keep working ==
bool(true)
int(3)
int(4)
int(5)
int(6)
int(4)
int(6)
