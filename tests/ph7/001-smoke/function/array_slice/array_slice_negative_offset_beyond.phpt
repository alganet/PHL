--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_slice with negative offset beyond start clamps to zero
--FILE--
<?php
$a = array('a', 'b', 'c', 'd');
$r = array_slice($a, -10);
echo $r[0] . PHP_EOL;
echo $r[1] . PHP_EOL;
echo $r[2] . PHP_EOL;
echo $r[3] . PHP_EOL;
echo count($r) . PHP_EOL;
?>
--EXPECT--
a
b
c
d
4
--CLEAN--
<?php
unset($a, $r);
