--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Arrow function: nested, transitive capture
--FILE--
<?php
$adder = fn($x) => fn($y) => $x + $y;
$add5 = $adder(5);
echo $add5(3), "\n";
echo $add5(10), "\n";
?>
--EXPECT--
8
15
--CLEAN--
<?php
unset($adder, $add5);
