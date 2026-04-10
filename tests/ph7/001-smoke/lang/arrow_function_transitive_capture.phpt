--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Arrow function: inner arrow references outer scope var (transitive capture)
--FILE--
<?php
$z = 7;
$outer = fn() => fn() => $z;
echo $outer()(), "\n";

$a = 1; $b = 2; $c = 3;
$deep = fn() => fn() => fn() => $a + $b + $c;
echo $deep()()(), "\n";
?>
--EXPECT--
7
6
--CLEAN--
<?php
unset($outer, $deep, $a, $b, $c, $z);
