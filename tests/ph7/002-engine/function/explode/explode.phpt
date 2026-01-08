--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
explode function tests
--FILE--
<?php
// Test basic explode
$array = explode(",", "a,b,c");
echo count($array) . "\n";
echo $array[0] . "\n";
echo $array[1] . "\n";
echo $array[2] . "\n";

// Test with no delimiter in string
$array2 = explode(",", "abc");
echo count($array2) . "\n";
echo $array2[0] . "\n";

// Test with limit
$array3 = explode(",", "a,b,c,d", 2);
echo count($array3) . "\n";
echo $array3[0] . "\n";
echo $array3[1] . "\n";
?>
--EXPECT--
3
a
b
c
1
abc
2
a
b,c,d