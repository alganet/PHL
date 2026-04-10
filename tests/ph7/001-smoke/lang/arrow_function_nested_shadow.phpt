--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Arrow function: nested inner parameter shadows outer-scope variable
--FILE--
<?php
$y = 100;
$f = fn($x) => fn($y) => $x + $y;
echo $f(5)(3), "\n";

$v = 999;
$g = fn($v) => fn($v) => $v * 2;
echo $g(1)(5), "\n";
?>
--EXPECT--
8
10
--CLEAN--
<?php
unset($f, $g, $v, $y);
