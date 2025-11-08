--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Ternary operator
--FILE--
<?php
$a = 5;
$b = 10;
$max = ($a > $b) ? $a : $b;
echo $max; // 10
echo "\n";
$status = ($a < $b) ? "less" : "greater";
echo $status; // less
?>
--EXPECT--
10
less
--CLEAN--
<?php
unset($a, $b, $max, $status);
?>