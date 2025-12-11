--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_chunk should return correct chunk counts and preserve keys when requested
--FILE--
<?php
$in = array(1,2,3,4,5);
$chunks = array_chunk($in, 2);
echo count($chunks) . PHP_EOL; // 3
$pres = array_chunk($in, 2, true);
echo implode(',', array_keys($pres[0])) . PHP_EOL; // '0,1'
// Edge case: size larger than array
$chunk2 = array_chunk($in, 10);
echo count($chunk2) . PHP_EOL; // 1
?>
--EXPECT--
3
0,1
1
--CLEAN--
<?php
unset($in,$chunks,$pres,$chunk2);
?>
