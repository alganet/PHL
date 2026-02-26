--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_combine should build an array from keys and values (basic)
--FILE--
<?php
$keys = array('a','b','c');
$vals = array(1,2,3);
$c = array_combine($keys, $vals);
// only check key order
echo implode(',', array_keys($c)) . PHP_EOL;
?>
--EXPECT--
a,b,c
--CLEAN--
<?php
unset($keys, $vals, $c);
