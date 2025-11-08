--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
More array operations
--FILE--
<?php
$array = array(1, 2, 3);
array_push($array, 4);
echo count($array); // 4
echo "\n";
$popped = array_pop($array);
echo $popped; // 4
echo "\n";
echo count($array); // 3
echo "\n";
array_unshift($array, 0);
echo $array[0]; // 0
echo "\n";
$shifted = array_shift($array);
echo $shifted; // 0
?>
--EXPECT--
4
4
3
0
0
--CLEAN--
<?php
unset($array, $popped, $shifted);
?>