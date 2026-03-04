--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_fill_keys: empty input array returns empty array
--FILE--
<?php
$a = array_fill_keys(array(), 'x');
echo count($a) . PHP_EOL;
?>
--EXPECT--
0
--CLEAN--
<?php
unset($a);
