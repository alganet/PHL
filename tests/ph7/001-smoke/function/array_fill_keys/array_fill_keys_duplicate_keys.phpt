--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_fill_keys: duplicate keys produce single entry
--FILE--
<?php
$a = array_fill_keys(array('a', 'b', 'a'), 1);
echo count($a) . PHP_EOL;
?>
--EXPECT--
2
--CLEAN--
<?php
unset($a);
