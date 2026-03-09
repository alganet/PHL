--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_map transforms string values with callback
--FILE--
<?php
$r = array_map('strtoupper', array('hello', 'world'));
echo $r[0] . PHP_EOL;
echo $r[1] . PHP_EOL;
?>
--EXPECT--
HELLO
WORLD
--CLEAN--
<?php
unset($r);
