--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Arrow function: variadic parameters
--FILE--
<?php
$sum = fn(...$args) => array_sum($args);
echo $sum(1, 2, 3, 4), "\n";
echo $sum(), "\n";
?>
--EXPECT--
10
0
--CLEAN--
<?php
unset($sum);
