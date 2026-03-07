--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_unshift() with only array argument returns count
--FILE--
<?php
$a = array('a', 'b', 'c');
echo array_unshift($a) . PHP_EOL;
?>
--EXPECT--
3
--CLEAN--
<?php
unset($a);
