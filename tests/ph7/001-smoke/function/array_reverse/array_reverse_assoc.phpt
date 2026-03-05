--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_reverse reverses an associative array preserving string keys
--FILE--
<?php
$r = array_reverse(array('a' => 1, 'b' => 2, 'c' => 3));
$keys = array_keys($r);
echo $keys[0] . PHP_EOL;
echo $keys[1] . PHP_EOL;
echo $keys[2] . PHP_EOL;
?>
--EXPECT--
c
b
a
--CLEAN--
<?php
unset($r, $keys);
