--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Spreading temporary arrays (literals, call results) into fixed parameters binds the values
--FILE--
<?php
function staSum($a, $b) { return $a + $b; }
function staMk() { return [7, 8]; }
class StaC {
    public function pair() { return [3, 4]; }
}

// Array literal spread (used to bind null/null: the temp map's slots were
// recycled before the elements were read).
echo staSum(...[1, 2]), "\n";
// Call-result spread (the parser used to lose the ... flag when the argument
// tree roots at a call node — no expansion at all).
echo staSum(...staMk()), "\n";
echo staSum(...array_values([10, 20])), "\n";
// Method-call spread (paren-headed argument) into a FIXED-arity function —
// max() can't distinguish spread-vs-single-array, staSum can.
echo staSum(...(new StaC)->pair()), "\n";
// Mixed positional + literal spread.
echo staSum(1, ...[2]), "\n";
// Nested container element survives the temp map's release.
$staNested = staMk();
echo implode(",", [...staMk(), 9]), "|", count($staNested), "\n";
// Element that is itself an array.
function staFirst($x) { return $x[0]; }
echo staFirst(...[[5, 6]]), "\n";
// Variable spread unchanged, source array intact afterwards.
$staArgs = [1, 2];
echo staSum(...$staArgs), "|", implode(",", $staArgs), "\n";
// Under-arity literal spread still counts correctly.
try {
    staSum(...[1]);
} catch (ArgumentCountError $e) {
    echo "count-ok\n";
}
// Constructor spread (used to fatal: OP_NEW ignored the expansion).
class StaK {
    public $v;
    public function __construct($a, $b) { $this->v = $a + $b; }
}
$staCtor = [10, 20];
echo (new StaK(...$staCtor))->v, "|", (new StaK(...[1, 2]))->v, "\n";
// Two spreads with an inner call between them (accumulator theft regression).
function staVar(...$all) { return implode(",", $all); }
echo staVar(...[1, 2], ...staMk()), "\n";
// Parenthesized spread argument.
$staParen = [1, 2];
echo staSum(...($staParen)), "|", staSum(...(true ? [3, 4] : [5, 6])), "\n";
// Named argument after a spread is legal.
function staNamed($a, $b = 0, $c = 0) { return "$a/$b/$c"; }
echo staNamed(...[1], c: 3), "\n";
?>
--EXPECT--
3
15
30
7
3
7,8,9|2
5
3|1,2
count-ok
30|3
1,2,7,8
3|7
1/0/3
--CLEAN--
<?php
unset($staNested, $staArgs, $staCtor, $staParen, $e);
