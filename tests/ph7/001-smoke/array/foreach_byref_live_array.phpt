--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
foreach by reference iterates the live array; by value iterates a snapshot
--FILE--
<?php
// By reference: appends made inside the body ARE visited (live array).
$a = [1, 2];
foreach ($a as &$v) {
    echo $v, ",";
    if ($v < 5) {
        $a[] = $v + 10;
    }
}
unset($v);
echo count($a), "\n";

// By reference: deleting an upcoming element skips it.
$b = [1, 2, 3];
foreach ($b as &$w) {
    echo $w, ",";
    unset($b[2]);
}
unset($w);
echo count($b), "\n";

// By reference: deleting the current and the next element mid-loop.
$c = [1, 2, 3, 4];
foreach ($c as $k => &$u) {
    echo $k, ":", $u, ",";
    if ($k == 1) {
        unset($c[1]);
        unset($c[2]);
    }
}
unset($u);
echo implode("/", array_keys($c)), "\n";

// A pre-existing value copy is protected from by-ref loop writes.
$d = [1, 2];
$e = $d;
foreach ($d as &$t) {
    $t *= 10;
}
unset($t);
echo implode(",", $d), " ", implode(",", $e), "\n";

// By reference: an element appended while the loop stands on the LAST
// element is still visited (worklist idiom).
$g = [1, 2];
$out = "";
foreach ($g as &$s) {
    $out .= $s . ",";
    if ($s == 2) {
        $g[] = 3;
    }
}
unset($s);
echo $out, "|", count($g), "\n";

// By reference: delete-current-then-append keeps the loop alive through an
// empty array (php visits every appended element).
$h = [1];
$n = 0;
foreach ($h as $k => &$r) {
    $keep = $r;
    echo $k, "=", $keep, ",";
    unset($h[$k]);
    if ($n++ < 3) {
        $h[] = $keep + 10;
    }
}
unset($r);
echo count($h), "\n";

// By value: the loop iterates a snapshot — appends are NOT visited.
$f = [1, 2];
foreach ($f as $x) {
    echo $x, ",";
    if ($x < 5) {
        $f[] = $x + 10;
    }
}
echo count($f), "\n";
?>
--EXPECT--
1,2,11,12,4
1,2,2
0:1,1:2,3:4,0/3
10,20 1,2
1,2,3,|3
0=1,1=11,2=21,3=31,0
1,2,4
--CLEAN--
<?php
unset($a, $b, $c, $d, $e, $f, $g, $h, $k, $keep, $n, $out, $x);
