--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_map iterates to the longest array, padding short ones with NULL
--FILE--
<?php
$a = [1];
$b = [10, 20];
$r = array_map(function ($x, $y) { return [$x, $y]; }, $a, $b);
echo count($r), PHP_EOL;          // 2 (length of the longest array)
echo $r[0][0], ',', $r[0][1], PHP_EOL;                          // 1,10
echo (is_null($r[1][0]) ? 'NULL' : $r[1][0]), ',', $r[1][1], PHP_EOL; // NULL,20
?>
--EXPECT--
2
1,10
NULL,20
--CLEAN--
<?php
unset($a, $b, $r);
