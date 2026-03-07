--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_slice with length exceeding remaining elements clamps to end
--FILE--
<?php
$a = array('a', 'b', 'c');
$r = array_slice($a, 1, 100);
echo $r[0] . PHP_EOL;
echo $r[1] . PHP_EOL;
echo count($r) . PHP_EOL;
?>
--EXPECT--
b
c
2
--CLEAN--
<?php
unset($a, $r);
