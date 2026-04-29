--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Short array syntax: spread of empty array is a no-op
--FILE--
<?php
$a = [1, ...[], 2];
echo count($a), "\n";
foreach ($a as $k => $v) {
    echo $k, "=", $v, "\n";
}

$b = [...[]];
echo count($b), "\n";

$c = ["x" => 10, ...[], "y" => 20];
foreach ($c as $k => $v) {
    echo $k, "=", $v, "\n";
}
?>
--EXPECT--
2
0=1
1=2
0
x=10
y=20
--CLEAN--
<?php
unset($a, $b, $c);
