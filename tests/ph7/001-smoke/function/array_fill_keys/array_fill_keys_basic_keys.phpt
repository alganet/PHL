--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_fill_keys: resulting array has correct keys
--FILE--
<?php
$keys = array('a', 'b');
$a = array_fill_keys($keys, 'z');
echo implode(',', array_keys($a)) . PHP_EOL;
?>
--EXPECT--
a,b
--CLEAN--
<?php
unset($keys, $a);
