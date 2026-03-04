--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_push preserves existing elements in the array
--FILE--
<?php
$a = array('first', 'second');
array_push($a, 'third');
echo $a[0] . PHP_EOL;
echo $a[1] . PHP_EOL;
echo $a[2] . PHP_EOL;
?>
--EXPECT--
first
second
third
--CLEAN--
<?php
unset($a);
