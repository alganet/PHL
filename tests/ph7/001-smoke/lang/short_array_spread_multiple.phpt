--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Short array syntax: multiple spreads, later string key overwrites
--FILE--
<?php
$a = ["k" => 1, "x" => 2];
$b = ["k" => 9, "y" => 3];
$c = [...$a, ...$b];
echo count($c), "\n";
foreach ($c as $k => $v) {
    echo $k, "=", $v, "\n";
}

$d = [...[1, 2], ...[3, 4]];
echo count($d), "\n";
foreach ($d as $k => $v) {
    echo $k, "=", $v, "\n";
}
?>
--EXPECT--
3
k=9
x=2
y=3
4
0=1
1=2
2=3
3=4
--CLEAN--
<?php
unset($a, $b, $c, $d);
