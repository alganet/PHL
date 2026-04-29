--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Short array syntax: spread copies entries (mutating result does not bleed into source)
--FILE--
<?php
// Mutating the spread result must not affect the source (and vice versa).
$a = [1, 2, 3];
$b = [...$a];
$b[0] = 99;
echo "a:";
foreach ($a as $v) echo " ", $v;
echo "\n";
echo "b:";
foreach ($b as $v) echo " ", $v;
echo "\n";

// String-keyed source.
$s = ["x" => 1, "y" => 2];
$t = [...$s];
$t["x"] = 99;
echo "s.x=", $s["x"], " t.x=", $t["x"], "\n";

// Mutating source after spread must not bleed forward.
$c = [10, 20];
$d = [...$c];
$c[0] = 0;
echo "c[0]=", $c[0], " d[0]=", $d[0], "\n";

// Top-level COW with intermediate spread (deep mutation of $g[0] still
// hits a shared inner array — same as array_merge — so this asserts the
// expected, non-deep semantics, not deep cloning).
$inner = [1, 2];
$f = [$inner];
$g = [...$f];
$g[0][0] = 99;
echo "f[0][0]=", $f[0][0], " g[0][0]=", $g[0][0], "\n";
?>
--EXPECT--
a: 1 2 3
b: 99 2 3
s.x=1 t.x=99
c[0]=0 d[0]=10
f[0][0]=1 g[0][0]=99
--CLEAN--
<?php
unset($a, $b, $s, $t, $c, $d, $inner, $f, $g);
