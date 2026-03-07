--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_unshift() prepends a single element
--FILE--
<?php
$a = array('b', 'c');
array_unshift($a, 'a');
echo $a[0] . PHP_EOL;
echo $a[1] . PHP_EOL;
echo $a[2] . PHP_EOL;
?>
--EXPECT--
a
b
c
--CLEAN--
<?php
unset($a);
