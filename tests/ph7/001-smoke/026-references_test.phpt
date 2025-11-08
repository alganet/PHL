--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
References
--FILE--
<?php
$a = 10;
$b = &$a; // reference
$b = 20;
echo $a; // 20
echo "\n";
$a = 30;
echo $b; // 30
echo "\n";
$array = array(1, 2, 3);
$c = &$array[1];
$c = 5;
echo $array[1]; // 5
?>
--EXPECT--
20
30
5
--CLEAN--
<?php
unset($a, $b, $array, $c);
?>