--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_slice with preserve_keys true keeps original numeric keys
--FILE--
<?php
$a = array('a', 'b', 'c', 'd');
$r = array_slice($a, 1, 2, true);
$k = array_keys($r);
echo $k[0] . PHP_EOL;
echo $k[1] . PHP_EOL;
?>
--EXPECT--
1
2
--CLEAN--
<?php
unset($a, $r, $k);
