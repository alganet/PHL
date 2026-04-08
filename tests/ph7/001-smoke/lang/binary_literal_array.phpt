--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Binary literal in arrays
--FILE--
<?php
$arr = [0b001, 0b010, 0b100];
echo implode(",", $arr) . "\n";

$arr2 = [0b1 => "one", 0b10 => "two", 0b100 => "four"];
echo $arr2[1] . "\n";
echo $arr2[2] . "\n";
echo $arr2[4] . "\n";

$arr3 = array(0b1010 => "ten");
echo $arr3[10] . "\n";

echo count([0b0, 0b1, 0b10, 0b11]) . "\n";
?>
--EXPECT--
1,2,4
one
two
four
ten
4
--CLEAN--
<?php

