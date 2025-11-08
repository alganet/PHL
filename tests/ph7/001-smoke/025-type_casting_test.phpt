--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Type casting
--FILE--
<?php
$string = "123";
$int = (int) $string;
echo $int; // 123
echo "\n";
$float = 3.14;
$int2 = (int) $float;
echo $int2; // 3
echo "\n";
$num = 456;
$string2 = (string) $num;
echo $string2; // 456
echo "\n";
$bool = (bool) 0;
echo $bool ? "true" : "false"; // false
?>
--EXPECT--
123
3
456
false
--CLEAN--
<?php
unset($string, $int, $float, $int2, $num, $string2, $bool);
?>