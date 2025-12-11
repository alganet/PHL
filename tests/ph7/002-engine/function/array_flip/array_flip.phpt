--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_flip should swap keys and values; duplicates keep last key
--FILE--
<?php
$a = array('a' => 'v1', 'b' => 'v2', 'c' => 'v1');
$flip = array_flip($a);
// v1 => 'c' (last), v2 => 'b'
echo ($flip['v1'] === 'c' ? 'ok' : 'fail') . PHP_EOL;
echo ($flip['v2'] === 'b' ? 'ok' : 'fail') . PHP_EOL;
?>
--EXPECT--
ok
ok
--CLEAN--
<?php
unset($a,$flip);
?>
