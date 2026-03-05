--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_reverse accepts truthy non-bool value for preserve_keys
--FILE--
<?php
$r = array_reverse(array(10, 20, 30), 1);
$keys = array_keys($r);
echo $keys[0] . PHP_EOL;
echo $keys[1] . PHP_EOL;
echo $keys[2] . PHP_EOL;
?>
--EXPECT--
2
1
0
--CLEAN--
<?php
unset($r, $keys);
