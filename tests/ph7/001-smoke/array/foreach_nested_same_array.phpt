--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Nested foreach over the same array iterate independently (php: each loop has its own position)
--FILE--
<?php
// Regression: the inner loop used to rewind a cursor shared with the outer
// loop, re-yielding index 0 forever (silent infinite loop).
$a = [1, 2];
foreach ($a as $i => $x) {
    foreach ($a as $y) {
        echo $y;
    }
    echo "|", $i, ";";
}
echo "\n";

// Triple nesting
$b = ['x', 'y'];
foreach ($b as $p) {
    foreach ($b as $q) {
        foreach ($b as $r) {
            echo $r;
        }
        echo ",";
    }
    echo ";";
}
echo "\n";

// Keyed inner + outer over the same map with string keys
$m = ['a' => 1, 'b' => 2];
foreach ($m as $k1 => $v1) {
    foreach ($m as $k2 => $v2) {
        echo $k1, $k2, ":";
    }
}
echo "\n";

// foreach does not move the internal array pointer (php 8)
$c = [10, 20, 30];
next($c);
foreach ($c as $unused) {
}
echo current($c), "\n";
?>
--EXPECT--
12|0;12|1;
xy,xy,;xy,xy,;
aa:ab:ba:bb:
20
--CLEAN--
<?php
unset($a, $b, $c, $m, $i, $x, $y, $p, $q, $r, $k1, $v1, $k2, $v2, $unused);
