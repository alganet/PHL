--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_slice extracts a basic slice with offset and length
--FILE--
<?php
$a = array('a', 'b', 'c', 'd');
$r = array_slice($a, 1, 2);
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
