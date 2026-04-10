--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Numeric literal separator in assignment expressions
--FILE--
<?php
$a = 1_000;
echo $a . "\n";
$a += 2_000;
echo $a . "\n";
$a -= 1_500;
echo $a . "\n";
$a *= 1_0;
echo $a . "\n";
$a /= 5_0;
echo $a . "\n";
$b = 0xFF_FF;
echo $b . "\n";
$c = 0b1010_1010;
echo $c . "\n";
$d = 1_234.567;
echo $d . "\n";
?>
--EXPECT--
1000
3000
1500
15000
300
65535
170
1234.567
--CLEAN--
<?php

