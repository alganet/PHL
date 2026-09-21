--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
`fn` names a variable/member/label wherever php expects a name, and still opens arrow functions
--FILE--
<?php
// A variable, property or constant merely SPELLED like the arrow-function
// keyword is a name — the '=>' after it belongs to the array/foreach entry.
$fn = 'k';
$afkArr = [$fn => 1];
echo "array-key-var: ", $afkArr['k'], "\n";

class AfkHolder
{
    public $fn = 'k';
    public static $fn2 = 'k';
    public function fn() { return 'k'; }
}
$afkObj = new AfkHolder;
echo "array-key-prop: ", [$afkObj->fn => 2]['k'], "\n";
echo "array-key-nullsafe: ", [$afkObj?->fn => 3]['k'], "\n";
echo "array-key-static: ", [AfkHolder::$fn2 => 4]['k'], "\n";
echo "array-key-method: ", [$afkObj->fn() => 5]['k'], "\n";

$afkCallable = 'strtoupper';
echo "array-key-varcall: ", [$afkCallable('k') => 6]['K'], "\n";
$fn2 = 'strtoupper';
echo "array-key-fncall: ", [$fn2('k') => 7]['K'], "\n";

foreach (['k' => 8] as $fn => $afkVal) {
    echo "foreach-key: $fn=$afkVal\n";
}
[$fn => $afkDest] = ['k' => 9];
echo "destructure-key: ", $afkDest, "\n";
list($fn => $afkDest2) = ['k' => 10];
echo "list-key: ", $afkDest2, "\n";
echo "nested: ", ['outer' => [$fn => 11]]['outer']['k'], "\n";

function afkNamed($fn) { return $fn; }
echo "named-arg: ", afkNamed(fn: 12), "\n";

// ... and the real arrow function is untouched in every spelling.
$afkA = fn($x) => $x + 1;
$afkB = static fn($x) => $x * 2;
$afkC = fn(int $x): int => $x * 3;
$afkD = fn($x) => fn($y) => $x . $y;
echo "arrow: ", $afkA(1), $afkB(2), $afkC(3), $afkD('a')('b'), "\n";
// an arrow function as an array VALUE, and one whose call is the KEY
echo "arrow-in-array: ", (['k' => fn() => 13]['k'])(), " ", [(fn() => 'k')() => 14]['k'], "\n";
?>
--EXPECT--
array-key-var: 1
array-key-prop: 2
array-key-nullsafe: 3
array-key-static: 4
array-key-method: 5
array-key-varcall: 6
array-key-fncall: 7
foreach-key: k=8
destructure-key: 9
list-key: 10
nested: 11
named-arg: 12
arrow: 249ab
arrow-in-array: 13 14
--CLEAN--
<?php
