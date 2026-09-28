--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Each foreach over an object owns its cursor, and the body may add or remove properties
--FILE--
<?php
class ObjIterLive { public $p = 1; public $q = 2; public $r = 3; }

echo "== nested ==\n";
$oilA = new ObjIterLive();
foreach ($oilA as $k => $v) {
    echo "outer $k\n";
    foreach ($oilA as $k2 => $v2) { echo "  inner $k2\n"; }
}

echo "== unset the next one ==\n";
$oilB = new ObjIterLive();
foreach ($oilB as $k => $v) { echo "visit $k\n"; if ($k === 'p') { unset($oilB->q); } }

echo "== unset the current one ==\n";
$oilC = new ObjIterLive();
foreach ($oilC as $k => $v) { echo "visit $k\n"; unset($oilC->$k); }
print_r($oilC);

echo "== unset them all ==\n";
$oilD = new ObjIterLive();
foreach ($oilD as $k => $v) { echo "visit $k\n"; unset($oilD->p, $oilD->q, $oilD->r); }

echo "== the body appends ==\n";
$oilE = new stdClass();
$oilE->a = 1;
$oilN = 0;
foreach ($oilE as $k => $v) { echo "visit $k\n"; if (++$oilN < 4) { $oilE->{"n$oilN"} = $oilN; } }

echo "== break, then again ==\n";
$oilF = new ObjIterLive();
foreach ($oilF as $k => $v) { echo "first $k\n"; break; }
foreach ($oilF as $k => $v) { echo "second $k\n"; }

echo "== recursion ==\n";
function oilWalk(ObjIterLive $o, int $depth): void {
    foreach ($o as $k => $v) {
        echo str_repeat(' ', $depth), "d$depth $k\n";
        if ($depth < 1) { oilWalk($o, $depth + 1); }
    }
}
oilWalk(new ObjIterLive(), 0);

echo "== two suspended activations of one loop ==\n";
function oilGen(ObjIterLive $o) { foreach ($o as $k => $v) { yield $k => $v; } }
$oilShared = new ObjIterLive();
$oilG1 = oilGen($oilShared);
$oilG2 = oilGen($oilShared);
$oilG2->next();
while ($oilG1->valid() || $oilG2->valid()) {
    if ($oilG1->valid()) { echo "g1 ", $oilG1->key(), "\n"; $oilG1->next(); }
    if ($oilG2->valid()) { echo "g2 ", $oilG2->key(), "\n"; $oilG2->next(); }
}

echo "== by reference ==\n";
$oilH = new ObjIterLive();
foreach ($oilH as $k => &$v) { $v *= 10; }
unset($v);
print_r($oilH);
?>
--EXPECT--
== nested ==
outer p
  inner p
  inner q
  inner r
outer q
  inner p
  inner q
  inner r
outer r
  inner p
  inner q
  inner r
== unset the next one ==
visit p
visit r
== unset the current one ==
visit p
visit q
visit r
ObjIterLive Object
(
)
== unset them all ==
visit p
== the body appends ==
visit a
visit n1
visit n2
visit n3
== break, then again ==
first p
second p
second q
second r
== recursion ==
d0 p
 d1 p
 d1 q
 d1 r
d0 q
 d1 p
 d1 q
 d1 r
d0 r
 d1 p
 d1 q
 d1 r
== two suspended activations of one loop ==
g1 p
g2 q
g1 q
g2 r
g1 r
== by reference ==
ObjIterLive Object
(
    [p] => 10
    [q] => 20
    [r] => 30
)
--CLEAN--
<?php
unset($oilA, $oilB, $oilC, $oilD, $oilE, $oilF, $oilH, $oilN, $oilShared, $oilG1, $oilG2, $k, $v, $k2, $v2);
