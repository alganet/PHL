--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_reverse with preserve_keys=false reindexes numeric keys
--FILE--
<?php
$r = array_reverse(array(10, 20, 30), false);
$keys = array_keys($r);
echo $keys[0] . PHP_EOL;
echo $keys[1] . PHP_EOL;
echo $keys[2] . PHP_EOL;
?>
--EXPECT--
0
1
2
--CLEAN--
<?php
unset($r, $keys);
