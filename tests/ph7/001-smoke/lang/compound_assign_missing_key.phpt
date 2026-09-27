--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A compound assign READS the element first, so a missing key warns
--DESCRIPTION--
php compiles a read-modify-write target (`$a[k] += v`, `$a[k] .= v`, `$a[k]++`) as a
BP_VAR_RW fetch: it hands back a writable slot exactly as a plain write does, but it READ
the element to do it, so a key that was not there warns `Undefined array key` before it is
created. PHL compiled every assignment target the same way — create-if-missing, silently —
so the read half of a read-modify-write said nothing where the same read through
`$q = $a[9]` warned, and a missing PROPERTY under the same operator did warn.

Every LEVEL of the chain carries the context, which is why `$a['x']['y'] += 1` warns twice.
A plain `=`, a `??=`, a `=&` bind, a by-reference argument and an append create in silence
in php too, and stay silent here.

$GLOBALS rides the same warning: its keys ARE the global symbol table, so php words a miss
`Undefined global variable $x`, with the subscript spelled raw rather than folded and
quoted the way an array key is.
--FILE--
<?php
set_error_handler(function ($n, $m) { echo '<', $m, '>', "\n"; return true; });

echo "-- every compound operator warns once, then seeds the key\n";
foreach (['+=', '-=', '*=', '.=', '|=', '<<=', '%='] as $camOp) {
    $camA = [];
    eval('$camA[9] ' . $camOp . ' 2;');
    echo $camOp, ' => ', var_export($camA[9], true), "\n";
}

echo "-- ++ and -- read the element too\n";
$camB = []; $camB[7]++; var_dump($camB);
$camC = []; ++$camC['k']; var_dump($camC);
$camD = []; $camD[3]--; var_dump($camD);

echo "-- the key is rendered as the key the lookup used\n";
$camE = []; $camE['s'] .= 'x';
$camF = []; $camF[false] += 1;
$camG = []; $camG['10'] += 1;

echo "-- every level of a chain warns\n";
$camH = []; $camH['x']['y'] += 1; echo json_encode($camH), "\n";
$camI = ['x' => []]; $camI['x']['y'] += 1; echo json_encode($camI), "\n";

echo "-- an undefined BASE variable warns before its key does\n";
$camJ['x'] += 1; echo json_encode($camJ), "\n";

echo "-- an unset key is missing again\n";
$camK = [9 => 1]; unset($camK[9]); $camK[9] += 1; echo json_encode($camK), "\n";

echo "-- a present key says nothing\n";
$camL = [9 => 1]; $camL[9] += 1; $camL[9] .= '!'; echo json_encode($camL), "\n";

echo "-- the contexts that create in silence\n";
$camM = []; $camM[9] = 1;
$camN = []; $camN[9] ??= 1;
$camO = []; $camP =& $camO[9]; $camP += 1;
$camQ = []; $camQ[] += 1;
$camR = function (&$x) { $x += 1; }; $camS = []; $camR($camS[9]);
echo json_encode([$camM, $camN, $camO, $camQ, $camS]), "\n";

echo "-- \$GLOBALS names the global variable it did not find\n";
$camT = $GLOBALS['camUndefinedGlobal'];
$GLOBALS['camUndefinedGlobal2'] += 1;
var_dump($GLOBALS['camUndefinedGlobal2']);

restore_error_handler();
?>
--EXPECT--
-- every compound operator warns once, then seeds the key
<Undefined array key 9>
+= => 2
<Undefined array key 9>
-= => -2
<Undefined array key 9>
*= => 0
<Undefined array key 9>
.= => '2'
<Undefined array key 9>
|= => 2
<Undefined array key 9>
<<= => 0
<Undefined array key 9>
%= => 0
-- ++ and -- read the element too
<Undefined array key 7>
array(1) {
  [7]=>
  int(1)
}
<Undefined array key "k">
array(1) {
  ["k"]=>
  int(1)
}
<Undefined array key 3>
<Decrement on type null has no effect, this will change in the next major version of PHP>
array(1) {
  [3]=>
  NULL
}
-- the key is rendered as the key the lookup used
<Undefined array key "s">
<Undefined array key 0>
<Undefined array key 10>
-- every level of a chain warns
<Undefined array key "x">
<Undefined array key "y">
{"x":{"y":1}}
<Undefined array key "y">
{"x":{"y":1}}
-- an undefined BASE variable warns before its key does
<Undefined variable $camJ>
<Undefined array key "x">
{"x":1}
-- an unset key is missing again
<Undefined array key 9>
{"9":1}
-- a present key says nothing
{"9":"2!"}
-- the contexts that create in silence
[{"9":1},{"9":1},{"9":1},[1],{"9":1}]
-- $GLOBALS names the global variable it did not find
<Undefined global variable $camUndefinedGlobal>
<Undefined global variable $camUndefinedGlobal2>
int(1)
