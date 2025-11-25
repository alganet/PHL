--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
shuffle() should randomize array order but preserve values; verify by sorting afterwards
--FILE--
<?php
$arr = array(1,2,3,4,5);
$res = shuffle($arr);
// shuffle() should return TRUE on success
echo ($res ? 'TRUE' : 'FALSE') . PHP_EOL;
// Sort the array again for deterministic output and verify values are preserved
sort($arr);
foreach($arr as $v){ echo $v . PHP_EOL; }
?>
--EXPECT--
TRUE
1
2
3
4
5
--CLEAN--
<?php
unset($arr);
?>
