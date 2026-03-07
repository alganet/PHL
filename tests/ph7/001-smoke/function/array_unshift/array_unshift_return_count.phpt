--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_unshift() returns new element count
--FILE--
<?php
$a = array('x', 'y');
echo array_unshift($a, 'a', 'b') . PHP_EOL;
?>
--EXPECT--
4
--CLEAN--
<?php
unset($a);
