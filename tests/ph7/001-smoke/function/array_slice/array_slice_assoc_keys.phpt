--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_slice always preserves string keys regardless of preserve_keys
--FILE--
<?php
$a = array('x' => 1, 'y' => 2, 'z' => 3);
$r = array_slice($a, 1);
$k = array_keys($r);
echo $k[0] . PHP_EOL;
echo $k[1] . PHP_EOL;
echo $r['y'] . PHP_EOL;
echo $r['z'] . PHP_EOL;
?>
--EXPECT--
y
z
2
3
--CLEAN--
<?php
unset($a, $r, $k);
