--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
foreach splits on its own `as`, not on one nested inside the iterated expression
--FILE--
<?php
/* a closure carrying a foreach of its own */
foreach ([function () { foreach ([1, 2] as $k) { echo "inner$k "; } }] as $f) { $f(); }
echo "\n";

/* two levels down, through a call */
foreach ([fn() => array_map(function () { foreach ([3] as $q) { echo "deep$q "; } }, [0])] as $f) { $f(); }
echo "\n";

/* an immediately-invoked closure whose foreach produces the iterated array */
foreach ((function () { foreach ([9] as $x) { return [$x, $x + 1]; } })() as $v) { echo $v, ' '; }
echo "\n";

/* a match arm in the header */
foreach (match (1) { 1 => [7, 8] } as $v) { echo $v, ' '; }
echo "\n";

/* `as` as a NAME is still not a separator */
$as = ['n1'];
foreach ($as as $v) { echo $v, ' '; }
$o = new stdClass;
$o->as = 'n2';
foreach ([$o->as] as $v) { echo $v, ' '; }
echo "\n";

/* the key/value split is unaffected */
foreach ([['k' => 'v']] as ['k' => $v]) { echo $v, "\n"; }
foreach ([[1, 2]] as [$a, $b]) { echo $a + $b, "\n"; }
?>
--EXPECT--
inner1 inner2 
deep3 
9 10 
7 8 
n1 n2 
v
3
--CLEAN--
<?php
