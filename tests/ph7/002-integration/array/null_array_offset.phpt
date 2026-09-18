--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A null array offset deprecates and normalizes to the "" key (php parity), it does not throw
--DESCRIPTION--
php DEPRECATES a null array subscript (`$a[$nullVar]`, `$a[null]`, `[null => v]`) and then
reads/writes the EMPTY-STRING key "", in every context except unset(). PHL used to throw a
`TypeError: Cannot access offset of type null on array`, turning valid php — an undefined or
null-valued index, extremely common via `$a[$maybe ?? null]` — into a fatal. Now it emits the
E_DEPRECATED notice (errno 8192, routed through the installed handler) and coerces null to ""
through the same cast path isset() already used, so a miss yields NULL with the usual
`Undefined array key ""` warning and a hit returns the value. The error handler is used so the
assertion matches the message BODY on both engines regardless of the log-copy prefix (§6).
--FILE--
<?php
set_error_handler(function ($no, $str) { echo "  [$no] $str\n"; return true; });

echo "== read: undefined-var index, key missing ==\n";
$a = ['x'];
var_dump($a[$k]);

echo "== read: null index hits the \"\" key ==\n";
$b = ['x', '' => 'empty'];
$z = null;
var_dump($b[$z]);

echo "== write: null index stores under \"\" ==\n";
$c = [];
$c[$z] = 'v';
var_dump($c);

echo "== null and \"\" are the SAME key ==\n";
$c[''] = 'w';
var_dump($c);

echo "== null literal key ==\n";
var_dump([null => 'L', 'x']);

echo "== isset / empty deprecate but stay correct ==\n";
var_dump(isset($b[$z]), empty($b[$z]), isset($a[$z]));

echo "== ?? / coalesce: deprecates, yields value or default ==\n";
var_dump($b[$z] ?? 'D', $a[$z] ?? 'D');

echo "== unset: NO deprecation ==\n";
$d = ['' => 1, 'keep' => 2];
unset($d[$z]);
var_dump($d);
?>
--EXPECT--
== read: undefined-var index, key missing ==
  [2] Undefined variable $k
  [8192] Using null as an array offset is deprecated, use an empty string instead
  [2] Undefined array key ""
NULL
== read: null index hits the "" key ==
  [8192] Using null as an array offset is deprecated, use an empty string instead
string(5) "empty"
== write: null index stores under "" ==
  [8192] Using null as an array offset is deprecated, use an empty string instead
array(1) {
  [""]=>
  string(1) "v"
}
== null and "" are the SAME key ==
array(1) {
  [""]=>
  string(1) "w"
}
== null literal key ==
  [8192] Using null as an array offset is deprecated, use an empty string instead
array(2) {
  [""]=>
  string(1) "L"
  [0]=>
  string(1) "x"
}
== isset / empty deprecate but stay correct ==
  [8192] Using null as an array offset is deprecated, use an empty string instead
  [8192] Using null as an array offset is deprecated, use an empty string instead
  [8192] Using null as an array offset is deprecated, use an empty string instead
bool(true)
bool(false)
bool(false)
== ?? / coalesce: deprecates, yields value or default ==
  [8192] Using null as an array offset is deprecated, use an empty string instead
  [8192] Using null as an array offset is deprecated, use an empty string instead
string(5) "empty"
string(1) "D"
== unset: NO deprecation ==
array(1) {
  ["keep"]=>
  int(2)
}
--CLEAN--
<?php
unset($a, $b, $c, $d, $z, $k);
