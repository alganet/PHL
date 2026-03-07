--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_slice with offset beyond array end returns empty array
--FILE--
<?php
$a = array('a', 'b', 'c', 'd');
$r = array_slice($a, 10);
echo count($r) . PHP_EOL;
?>
--EXPECT--
0
--CLEAN--
<?php
unset($a, $r);
