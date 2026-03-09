--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_map callback can change value types
--FILE--
<?php
$r = array_map('strlen', array('hi', 'hello'));
echo $r[0] . PHP_EOL;
echo $r[1] . PHP_EOL;
?>
--EXPECT--
2
5
--CLEAN--
<?php
unset($r);
