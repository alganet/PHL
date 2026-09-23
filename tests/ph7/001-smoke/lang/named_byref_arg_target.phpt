--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A NAMED by-reference argument resolves against the formal its name picks
--DESCRIPTION--
A call argument whose callee is unknown at compile time rides a deferred carrier that
OP_CALL resolves once the callee is in hand: a by-REFERENCE parameter creates the
element or property, a by-VALUE one warns and creates nothing. NAMED arguments were
excluded from that mechanism because the resolver read the by-ref-ness POSITIONALLY,
and a named actual's position is not its formal's — so `r(x: $a['new'])` took formal
#1's by-value verdict, warned `Undefined array key` and handed the callee a NULL to
write into, where php creates the element.

The resolver looks the name up in the formals through the call's own argument map now,
so the named spelling gets the same verdict as the positional one — in both
directions: a by-VALUE named argument still warns and still leaves the target alone.
--FILE--
<?php
function nbaRef($a = 0, &$x = null) { $x = 'R'; }
function nbaVal($a = 0, $x = null) { var_dump($x); }

set_error_handler(function ($n, $m) { echo '<', $m, '>', "\n"; return true; });

echo "-- a by-reference named argument CREATES a missing element\n";
$a = [];
nbaRef(x: $a['new']);
var_dump($a);

echo "-- an existing element and a property\n";
$b = ['k' => 'o'];
nbaRef(x: $b['k']);
var_dump($b['k']);
class NbaHolder { public $p = 'o'; }
$o = new NbaHolder;
nbaRef(x: $o->p);
var_dump($o->p);

echo "-- a plain variable, and one mixed with a positional\n";
$c = 'c';
nbaRef(x: $c);
var_dump($c);
$d = [];
nbaRef(0, x: $d['new']);
var_dump($d);

echo "-- a by-VALUE named argument warns and creates nothing\n";
nbaVal(x: $nbaUndef);
var_dump(array_key_exists('nbaUndef', get_defined_vars()));
$e = [];
nbaVal(x: $e['no']);
var_dump($e);

echo "-- a method\n";
class NbaC { public function m($a = 0, &$x = null) { $x = 'M'; } }
$f = [];
(new NbaC)->m(x: $f['new']);
var_dump($f);

echo "-- a constructor\n";
class NbaCtor { public function __construct($a = 0, &$x = null) { $x = 'C'; } }
$g = [];
new NbaCtor(x: $g['new']);
var_dump($g);

echo "-- a by-reference variadic tail\n";
function nbaTail(&...$xs) { foreach ($xs as $k => &$v) { $v = "T$k"; } }
$h = 'h';
nbaTail(one: $h);
var_dump($h);

echo "-- a builtin's &\$count\n";
$s = 'aXb';
$n = 0;
var_dump(str_replace(search: 'X', replace: 'Y', subject: $s, count: $n));
var_dump($n);

restore_error_handler();
?>
--EXPECT--
-- a by-reference named argument CREATES a missing element
array(1) {
  ["new"]=>
  string(1) "R"
}
-- an existing element and a property
string(1) "R"
string(1) "R"
-- a plain variable, and one mixed with a positional
string(1) "R"
array(1) {
  ["new"]=>
  string(1) "R"
}
-- a by-VALUE named argument warns and creates nothing
<Undefined variable $nbaUndef>
NULL
bool(false)
<Undefined array key "no">
NULL
array(0) {
}
-- a method
array(1) {
  ["new"]=>
  string(1) "M"
}
-- a constructor
array(1) {
  ["new"]=>
  string(1) "C"
}
-- a by-reference variadic tail
string(4) "Tone"
-- a builtin's &$count
string(3) "aYb"
int(1)
--CLEAN--
<?php
