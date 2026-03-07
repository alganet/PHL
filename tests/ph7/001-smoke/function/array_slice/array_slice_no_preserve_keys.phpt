--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_slice without preserve_keys resets numeric keys to zero
--FILE--
<?php
$a = array('a', 'b', 'c', 'd');
$r = array_slice($a, 2);
$k = array_keys($r);
echo $k[0] . PHP_EOL;
echo $k[1] . PHP_EOL;
?>
--EXPECT--
0
1
--CLEAN--
<?php
unset($a, $r, $k);
