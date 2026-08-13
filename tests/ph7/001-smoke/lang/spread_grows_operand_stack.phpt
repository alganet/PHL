--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Argument unpacking expands past the operand-stack guard (grows the stack)
--FILE--
<?php
/* A spread of more than VM_STACK_GUARD (16) elements used to hit a PHL-native
 * "cumulative expansion exceeds stack guard" error and then bind the whole map
 * as a single argument. The operand stack now grows (realloc + pointer fixups)
 * at OP_SPREAD, so an unpack of any size — ordinary php code — works. Verified
 * byte-identical to php 8.5.7 across the call kinds and the named-key replay
 * that the growth must keep aligned. */

// Plain positional spread well past the old 16 cap.
function sgrowV(...$xs) { return count($xs) . "/" . array_sum($xs); }
echo sgrowV(...range(1, 20)), "\n";        // 20/210
echo sgrowV(...range(1, 200)), "\n";       // 200/20100

// Leading positional args + a growing spread.
function sgrowH($a, $b, ...$rest) { return "$a|$b|" . count($rest); }
echo sgrowH(1, 2, ...range(3, 40)), "\n";  // 1|2|38

// Two spreads in one call, both crossing the cap.
echo sgrowV(...range(1, 10), ...range(11, 30)), "\n"; // 30/465

// Named string keys must survive the realloc (aSpreadRun re-anchor).
function sgrowN(...$xs) { return json_encode($xs); }
$sgrowMap = [];
for ($i = 0; $i < 20; $i++) { $sgrowMap["k$i"] = $i; }
echo sgrowN(...$sgrowMap), "\n";

// A growing positional spread followed by a compile-time named arg.
function sgrowM($x, $y, ...$rest) { return "$x/$y/" . json_encode($rest); }
echo sgrowM(...range(1, 20), z: 99), "\n";

// Methods, static, closures, arrow fns.
class SgrowC {
    public function m(...$xs) { return count($xs); }
    public static function s(...$xs) { return count($xs); }
}
echo (new SgrowC)->m(...range(1, 30)), "\n";   // 30
echo SgrowC::s(...range(1, 25)), "\n";         // 25
$sgrowClosure = function (...$xs) { return count($xs); };
echo $sgrowClosure(...range(1, 25)), "\n";     // 25

// Generator body running a growing spread call.
function sgrowGen() { yield sgrowV(...range(1, 20)); }
foreach (sgrowGen() as $sgrowY) { echo $sgrowY, "\n"; } // 20/210

// call_user_func_array with a large argument array.
echo call_user_func_array("sgrowV", range(1, 50)), "\n"; // 50/1275

// A growing spread whose element is itself a growing-spread call result: the
// inner call's realloc + per-call spread scope must leave the outer intact.
function sgrowInner(...$xs) { return array_sum($xs); }
echo sgrowV(sgrowInner(...range(1, 30)), ...range(1, 20)), "\n"; // 21/675

// A growing spread FOLLOWED by many more pushes in the same call: only OP_SPREAD
// re-checks capacity, so the grown buffer must keep the body's original headroom
// (not just 16 slack) or the trailing pushes overflow it. Here a 100-element
// spread grows the stack, then a 60-element array literal (a named arg's value)
// pushes 60 transient slots — far past a 16-slot guard — before OP_LOAD_MAP.
function sgrowT($x = 0, ...$rest) { return count($rest) . ":" . count($rest['k']); }
echo sgrowT(...range(1, 100), k: [
    1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,
    21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
    41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,
]), "\n"; // 100:60
?>
--EXPECT--
20/210
200/20100
1|2|38
30/465
{"k0":0,"k1":1,"k2":2,"k3":3,"k4":4,"k5":5,"k6":6,"k7":7,"k8":8,"k9":9,"k10":10,"k11":11,"k12":12,"k13":13,"k14":14,"k15":15,"k16":16,"k17":17,"k18":18,"k19":19}
1/2/{"0":3,"1":4,"2":5,"3":6,"4":7,"5":8,"6":9,"7":10,"8":11,"9":12,"10":13,"11":14,"12":15,"13":16,"14":17,"15":18,"16":19,"17":20,"z":99}
30
25
25
20/210
50/1275
21/675
100:60
--CLEAN--
<?php
