--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
By-ref call argument that is an array element or object property vivifies and binds
--FILE--
<?php
set_error_handler(function ($no, $str) { echo "  [$no] $str\n"; return true; });
function r(&$x) { $x = 7; }

echo "== by-ref: vivify a missing element ==\n";
$a = [];
r($a["k"]);
var_dump($a);

echo "== by-ref: append an int key ==\n";
$b = [1, 2];
r($b[5]);
var_dump($b);

echo "== by-ref: vivify a whole nested chain ==\n";
$c = [];
r($c[0][1]);
var_dump($c);

echo "== by-ref: vivify an undefined base variable, silently ==\n";
r($u["k"]);
var_dump($u);

echo "== by-ref: vivify a missing property ==\n";
$o = new stdClass;
r($o->p);
var_dump((array)$o);

echo "== by-ref: present element/property are aliased + overwritten ==\n";
$d = ["k" => 1];
r($d["k"]);
$e = new stdClass; $e->p = 1;
r($e->p);
var_dump($d, (array)$e);

echo "== method / late-declared / dynamic dispatch all bind ==\n";
class C { function m(&$x) { $x = 11; } }
$m = []; (new C)->m($m["k"]); var_dump($m);
$g = []; late($g["k"]); function late(&$x) { $x = 22; } var_dump($g);
$n = "r"; $h = []; $n($h["k"]); var_dump($h);

echo "== by-VALUE element/property: warn, do NOT vivify ==\n";
function f($x) {}
$va = [];
f($va["k"]);
var_dump($va);
$vo = new stdClass;
f($vo->p);
var_dump((array)$vo);

echo "== builtin by-ref out-param into an element still works ==\n";
$pm = [];
preg_match('/(a)/', 'a', $pm['m']);
var_dump($pm['m'][0]);
?>
--EXPECT--
== by-ref: vivify a missing element ==
array(1) {
  ["k"]=>
  int(7)
}
== by-ref: append an int key ==
array(3) {
  [0]=>
  int(1)
  [1]=>
  int(2)
  [5]=>
  int(7)
}
== by-ref: vivify a whole nested chain ==
array(1) {
  [0]=>
  array(1) {
    [1]=>
    int(7)
  }
}
== by-ref: vivify an undefined base variable, silently ==
array(1) {
  ["k"]=>
  int(7)
}
== by-ref: vivify a missing property ==
array(1) {
  ["p"]=>
  int(7)
}
== by-ref: present element/property are aliased + overwritten ==
array(1) {
  ["k"]=>
  int(7)
}
array(1) {
  ["p"]=>
  int(7)
}
== method / late-declared / dynamic dispatch all bind ==
array(1) {
  ["k"]=>
  int(11)
}
array(1) {
  ["k"]=>
  int(22)
}
array(1) {
  ["k"]=>
  int(7)
}
== by-VALUE element/property: warn, do NOT vivify ==
  [2] Undefined array key "k"
array(0) {
}
  [2] Undefined property: stdClass::$p
array(0) {
}
== builtin by-ref out-param into an element still works ==
string(1) "a"
