--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_unshift() on empty array adds the element
--FILE--
<?php
$a = array();
array_unshift($a, 'x');
echo $a[0] . PHP_EOL;
echo count($a) . PHP_EOL;
?>
--EXPECT--
x
1
--CLEAN--
<?php
unset($a);
