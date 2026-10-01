--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An array element pointing at its own array survives the copy that outlives the name
--FILE--
<?php
// An element that points at the array it lives in is a CYCLE, and once the
// variable's name is gone the cycle is the only thing holding the value. A copy
// of such an array has to keep the element a reference -- flattening it into a
// value copy makes a structure that grows one level per read, so every guard
// that detects recursion by revisiting the same array misses it and the walk
// never ends.
function self_cycle_walk(array &$d, int $n): string {
    if ($n > 4) { return "stop\n"; }
    $d[] = "mark$n";
    $child = $d[0] ?? null;
    return "level $n: " . (is_array($child) ? implode(',', array_keys($child)) : 'scalar') . "\n"
         . (is_array($child) ? self_cycle_walk($d[0], $n + 1) : '');
}
function self_cycle_built(): array { $a = []; $a[0] = &$a; return [$a]; }

echo "-- returned from the function that built it\n";
$returned = self_cycle_built();
echo self_cycle_walk($returned, 1);

echo "-- name unset in place\n";
$here = []; $here[0] = &$here; $wrapped = [$here]; unset($here);
echo self_cycle_walk($wrapped, 1);

echo "-- still named\n";
$named = []; $named[0] = &$named; $kept = [$named];
echo self_cycle_walk($kept, 1);
?>
--EXPECT--
-- returned from the function that built it
level 1: 0
level 2: 0
level 3: 0,1
level 4: 0,1,2
stop
-- name unset in place
level 1: 0
level 2: 0
level 3: 0,1
level 4: 0,1,2
stop
-- still named
level 1: 0
level 2: 0
level 3: 0,1
level 4: 0,1,2
stop
--CLEAN--
<?php
unset($returned, $here, $wrapped, $named, $kept);
