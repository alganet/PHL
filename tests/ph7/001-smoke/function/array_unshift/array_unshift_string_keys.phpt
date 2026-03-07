--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_unshift() preserves string keys from original array
--FILE--
<?php
$a = array('key' => 'val');
array_unshift($a, 'first');
echo $a[0] . PHP_EOL;
echo $a['key'] . PHP_EOL;
echo count($a) . PHP_EOL;
?>
--EXPECT--
first
val
2
--CLEAN--
<?php
unset($a);
