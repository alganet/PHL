--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_slice with length zero returns empty array
--FILE--
<?php
$a = array('a', 'b', 'c');
$r = array_slice($a, 1, 0);
echo count($r) . PHP_EOL;
?>
--EXPECT--
0
--CLEAN--
<?php
unset($a, $r);
