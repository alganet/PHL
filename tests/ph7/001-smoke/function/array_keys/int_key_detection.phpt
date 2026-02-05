--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Verify that '0' is treated as integer key while '00' is treated as string key
--FILE--
<?php
$a = array();
$a['0'] = 'string0';
$a[0] = 'int0';
// '0' should behave as integer key; last assignment overwrites
echo count($a) . PHP_EOL;
echo $a[0] . PHP_EOL;

$b = array();
$b['00'] = 'str00';
$b[0] = 'int0';
// '00' should be a string key; both entries exist
echo count($b) . PHP_EOL;
foreach(array_keys($b) as $k){ echo $k . PHP_EOL; }
?>
--EXPECT--
1
int0
2
00
0
--CLEAN--
<?php
unset($a, $b);
