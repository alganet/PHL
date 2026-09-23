--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A by-reference variadic tail aliases its actuals
--DESCRIPTION--
php collects a `&...$xs` tail as REFERENCES to the caller's slots: the elements dump
with `&`, and a write through one lands on the actual. PHL's collection loop COPIED
each actual into the variadic array, so every write inside the callee went nowhere —
silently, for a plain variable as much as for an element, a property, a named entry
or an unpacked one. Its REFUSAL half (a non-variable in the tail) already shipped.
--FILE--
<?php
function bvtWrite(&...$xs) {
    var_dump($xs);
    $xs[0] = 'A';
    $xs[1] = 'B';
}
$a = 1; $b = 2;
bvtWrite($a, $b);
var_dump($a, $b);

echo "-- after a by-value head\n";
function bvtHead($first, &...$rest) { $rest[0] = 'R0'; }
$p = 'p'; $q = 'q';
bvtHead($p, $q);
var_dump($p, $q);

echo "-- an element actual\n";
function bvtOne(&...$xs) { $xs[0] = 'E'; }
$arr = ['k' => 'orig'];
bvtOne($arr['k']);
var_dump($arr['k']);

echo "-- a MISSING element is created\n";
$fresh = [];
bvtOne($fresh['new']);
var_dump($fresh);

echo "-- a property actual\n";
class BvtHolder { public $p = 'o'; }
$o = new BvtHolder;
bvtOne($o->p);
var_dump($o->p);

echo "-- a NAMED entry aliases too\n";
function bvtNamed(&...$xs) { foreach ($xs as $k => &$v) { $v = "N$k"; } }
$m = 'm';
bvtNamed(one: $m);
var_dump($m);

echo "-- an UNPACKED actual aliases its element\n";
$u = ['x', 'y'];
bvtOne(...$u);
var_dump($u);

echo "-- by-reference foreach inside the callee\n";
function bvtUp(&...$xs) { foreach ($xs as &$v) { $v = strtoupper($v); } }
$s1 = 'a'; $s2 = 'b';
bvtUp($s1, $s2);
var_dump($s1, $s2);

echo "-- the tail is still a reference set on the way out\n";
function bvtRet(&...$xs) { $xs[0] = 'W'; return $xs; }
$r0 = 'r';
$ret = bvtRet($r0);
var_dump($r0, $ret);

echo "-- unset() inside the callee only unbinds the element\n";
function bvtUnset(&...$xs) { unset($xs[0]); var_dump($xs); }
$d = 'd';
bvtUnset($d);
var_dump($d);

echo "-- the same variable twice\n";
function bvtTwice(&...$xs) { $xs[0] = 'X'; var_dump($xs[1]); $xs[1] = 'Y'; }
$h = 'h';
bvtTwice($h, $h);
var_dump($h);

echo "-- a call RESULT is php's notice, then the temporary\n";
function bvtId($v) { return $v; }
$e = 'e';
/* One interpreter for the whole corpus: register and restore around the probe. */
set_error_handler(function ($n, $m) { echo '<', $m, '>', "\n"; return true; });
bvtOne(bvtId($e));
restore_error_handler();
var_dump($e);
?>
--EXPECT--
array(2) {
  [0]=>
  &int(1)
  [1]=>
  &int(2)
}
string(1) "A"
string(1) "B"
-- after a by-value head
string(1) "p"
string(2) "R0"
-- an element actual
string(1) "E"
-- a MISSING element is created
array(1) {
  ["new"]=>
  string(1) "E"
}
-- a property actual
string(1) "E"
-- a NAMED entry aliases too
string(4) "None"
-- an UNPACKED actual aliases its element
array(2) {
  [0]=>
  string(1) "E"
  [1]=>
  string(1) "y"
}
-- by-reference foreach inside the callee
string(1) "A"
string(1) "B"
-- the tail is still a reference set on the way out
string(1) "W"
array(1) {
  [0]=>
  &string(1) "W"
}
-- unset() inside the callee only unbinds the element
array(0) {
}
string(1) "d"
-- the same variable twice
string(1) "X"
string(1) "Y"
-- a call RESULT is php's notice, then the temporary
<Only variables should be passed by reference>
string(1) "e"
--CLEAN--
<?php
