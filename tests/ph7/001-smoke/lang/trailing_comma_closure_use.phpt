--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Trailing comma in closure use list
--FILE--
<?php
$x = 10;
$y = 20;
$f = function() use ($x, $y,) { return $x + $y; };
echo $f() . "\n";
?>
--EXPECT--
30
--CLEAN--
<?php
