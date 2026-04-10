--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Arrow function: auto-captures outer variables by value
--FILE--
<?php
$mult = 10;
$f = fn($x) => $x * $mult;
echo $f(3), "\n";
?>
--EXPECT--
30
--CLEAN--
<?php
unset($f, $mult);
