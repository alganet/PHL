--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Numeric literal separator in array literals
--FILE--
<?php
$arr = [1_000, 2_000, 3_000];
echo $arr[0] . "\n";
echo $arr[1] . "\n";
echo $arr[2] . "\n";
$keyed = [1_000 => 'a', 2_000 => 'b', 0xFF_FF => 'c'];
echo $keyed[1000] . "\n";
echo $keyed[2000] . "\n";
echo $keyed[65535] . "\n";
$mixed = [0b1_0 => 'two', 0_755 => 'octal', 0xCAFE_F00D => 'cafe'];
echo $mixed[2] . "\n";
echo $mixed[493] . "\n";
echo $mixed[3405705229] . "\n";
?>
--EXPECT--
1000
2000
3000
a
b
c
two
octal
cafe
--CLEAN--
<?php

