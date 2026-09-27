--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A string offset cannot be reached INTO, and php names the reach
--DESCRIPTION--
php reaches a string offset through a marker rather than a real slot, and lets whichever
opcode CONSUMES it name the refusal (`zend_wrong_string_offset_error`): a subscript is
`Cannot use string offset as an array`, a property is `... as an object`, a compound assign
and `++`/`--` have their own wordings, a reference is `Cannot create references to/from
string offsets`, and an append is `[] operator not supported for strings`.

PHL had the direct refusals but not the reach-inside ones: the character it read still
carried the BASE STRING's slot, so `$s = 'ab'; $s[0][1] = 'x';` left `$s === 'ax'` and
`$a['k'][0][1] = 'x'` rewrote the ELEMENT — a write php refuses, performed in silence. The
read-modify-write spellings of an APPEND did the same (`$s[] .= 'x'` appended to the whole
string, `$s[]++` incremented it).

php also never READS the character in a write-context fetch, so the offset's SHAPE
diagnostics are all it says: `$s[5] .= 'x'` is the assign-op Error with nothing about
offset 5, where PHL announced an `Uninitialized string offset 5` first.
--FILE--
<?php
set_error_handler(function ($n, $m) { echo '<', $m, '>', "\n"; return true; });
function soT($label, $fn) {
    echo $label, "\n";
    try { $r = $fn(); echo '  => ', var_export($r, true), "\n"; }
    catch (\Throwable $e) { echo '  [', get_class($e), ': ', $e->getMessage(), "]\n"; }
}

echo "-- reaching INSIDE an offset with a subscript\n";
soT('$s[0][1] = ', function () { $s = 'ab'; $s[0][1] = 'x'; return $s; });
soT('$s[0][1] +=', function () { $s = 'ab'; $s[0][1] += 1; return $s; });
soT('$s[0][1] ??=', function () { $s = 'ab'; $s[0][1] ??= 1; return $s; });
soT('$s[0][1]++', function () { $s = 'ab'; $s[0][1]++; return $s; });
soT('$s[0][] =', function () { $s = 'ab'; $s[0][] = 'x'; return $s; });
soT('unset($s[0][1])', function () { $s = 'ab'; unset($s[0][1]); return $s; });
soT('$a[k][0][1] =', function () { $a = ['k' => 'str']; $a['k'][0][1] = 'x'; return $a; });
soT('f()[0][1] =', function () { $f = fn() => 'ab'; $f()[0][1] = 'x'; return 'ran'; });

echo "-- reaching INSIDE an offset with a property\n";
soT('$s[0]->p =', function () { $s = 'ab'; $s[0]->p = 1; return $s; });
soT('$s[0]->p +=', function () { $s = 'ab'; $s[0]->p += 1; return $s; });
soT('$s[0]->p ??=', function () { $s = 'ab'; $s[0]->p ??= 1; return $s; });
soT('unset($s[0]->p)', function () { $s = 'ab'; unset($s[0]->p); return $s; });
soT('$s[0]->p()', function () { $s = 'ab'; return $s[0]->p(); });

echo "-- an APPEND to a string, in every spelling\n";
soT('$s[] =', function () { $s = 'ab'; $s[] = 'x'; return $s; });
soT('$s[] .=', function () { $s = 'ab'; $s[] .= 'x'; return $s; });
soT('$s[] ++', function () { $s = 'ab'; $s[]++; return $s; });

echo "-- a reference to an offset, and to something inside one\n";
soT('$r =& $s[0]', function () { $s = 'ab'; $r =& $s[0]; return $s; });
soT('$r =& $s[0][1]', function () { $s = 'ab'; $r =& $s[0][1]; return $s; });
soT('byref f($s[0])', function () { $s = 'ab'; $f = function (&$x) {}; $f($s[0]); return $s; });
soT('byref f($s[0][1])', function () { $s = 'ab'; $f = function (&$x) {}; $f($s[0][1]); return $s; });
soT('foreach ($s[0] as &$v)', function () { $s = 'ab'; foreach ($s[0] as &$v) {} return $s; });
soT('$r =& $s[0]->p', function () { $s = 'ab'; $r =& $s[0]->p; return $s; });
soT('byref f($s[0]->p)', function () { $s = 'ab'; $f = function (&$x) {}; $f($s[0]->p); return $s; });

echo "-- the write fetch resolves the offset but never READS it\n";
soT('$s[5] .=', function () { $s = 'ab'; $s[5] .= 'x'; return $s; });
soT('$s[5]++', function () { $s = 'ab'; $s[5]++; return $s; });
soT('$s["1x"] .=', function () { $s = 'ab'; $s['1x'] .= 'x'; return $s; });
soT('$s[1.5] .=', function () { $s = 'ab'; $s[1.5] .= 'x'; return $s; });
soT('$s["k"] .=', function () { $s = 'ab'; $s['k'] .= 'x'; return $s; });

echo "-- what php still allows\n";
soT('read $s[0][1]', function () { $s = 'ab'; return $s[0][1]; });
soT('read $s[0][0]', function () { $s = 'ab'; return $s[0][0]; });
soT('isset($s[0][1])', function () { $s = 'ab'; return isset($s[0][1]); });
soT('empty($s[0][1])', function () { $s = 'ab'; return empty($s[0][1]); });
soT('$s[0][1] ?? d', function () { $s = 'ab'; return $s[0][1] ?? 'd'; });
soT('byval f($s[0][1])', function () { $s = 'ab'; $f = fn($x) => var_export($x, true); return $f($s[0][1]); });
soT('$s[1] = ', function () { $s = 'ab'; $s[1] = 'X'; return $s; });
soT('$s[5] = ', function () { $s = 'ab'; $s[5] = 'X'; return $s; });
soT('unset($s[0])', function () { $s = 'ab'; unset($s[0]); return $s; });

restore_error_handler();
?>
--EXPECT--
-- reaching INSIDE an offset with a subscript
$s[0][1] = 
  [Error: Cannot use string offset as an array]
$s[0][1] +=
  [Error: Cannot use string offset as an array]
$s[0][1] ??=
  [Error: Cannot use string offset as an array]
$s[0][1]++
  [Error: Cannot use string offset as an array]
$s[0][] =
  [Error: Cannot use string offset as an array]
unset($s[0][1])
  [Error: Cannot use string offset as an array]
$a[k][0][1] =
  [Error: Cannot use string offset as an array]
f()[0][1] =
  [Error: Cannot use string offset as an array]
-- reaching INSIDE an offset with a property
$s[0]->p =
  [Error: Cannot use string offset as an object]
$s[0]->p +=
  [Error: Cannot use string offset as an object]
$s[0]->p ??=
  [Error: Cannot use string offset as an object]
unset($s[0]->p)
  [Error: Cannot use string offset as an object]
$s[0]->p()
  [Error: Call to a member function p() on string]
-- an APPEND to a string, in every spelling
$s[] =
  [Error: [] operator not supported for strings]
$s[] .=
  [Error: [] operator not supported for strings]
$s[] ++
  [Error: [] operator not supported for strings]
-- a reference to an offset, and to something inside one
$r =& $s[0]
  [Error: Cannot create references to/from string offsets]
$r =& $s[0][1]
  [Error: Cannot use string offset as an array]
byref f($s[0])
  [Error: Cannot create references to/from string offsets]
byref f($s[0][1])
  [Error: Cannot use string offset as an array]
foreach ($s[0] as &$v)
  [Error: Cannot create references to/from string offsets]
$r =& $s[0]->p
  [Error: Cannot use string offset as an object]
byref f($s[0]->p)
  [Error: Cannot use string offset as an object]
-- the write fetch resolves the offset but never READS it
$s[5] .=
  [Error: Cannot use assign-op operators with string offsets]
$s[5]++
  [Error: Cannot increment/decrement string offsets]
$s["1x"] .=
<Illegal string offset "1x">
  [Error: Cannot use assign-op operators with string offsets]
$s[1.5] .=
<String offset cast occurred>
  [Error: Cannot use assign-op operators with string offsets]
$s["k"] .=
  [TypeError: Cannot access offset of type string on string]
-- what php still allows
read $s[0][1]
<Uninitialized string offset 1>
  => ''
read $s[0][0]
  => 'a'
isset($s[0][1])
  => false
empty($s[0][1])
  => true
$s[0][1] ?? d
  => 'd'
byval f($s[0][1])
<Uninitialized string offset 1>
  => '\'\''
$s[1] = 
  => 'aX'
$s[5] = 
  => 'ab   X'
unset($s[0])
  [Error: Cannot unset string offsets]
