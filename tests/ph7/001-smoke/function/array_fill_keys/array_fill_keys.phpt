--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_fill_keys should build an array with given keys
--FILE--
<?php
$keys = array('a','b');
$a = array_fill_keys($keys, 'z');
echo implode(',', array_keys($a)) . PHP_EOL; // a,b
echo implode(',', array_values($a)) . PHP_EOL; // z,z
?>
--EXPECT--
a,b
z,z
--CLEAN--
<?php
unset($keys, $a);
