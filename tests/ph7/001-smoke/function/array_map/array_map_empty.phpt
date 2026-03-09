--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_map on empty array returns empty array
--FILE--
<?php
$r = array_map(function($v) { return $v; }, array());
echo is_array($r) ? 'array' : 'not array';
echo PHP_EOL;
echo count($r);
?>
--EXPECT--
array
0
--CLEAN--
<?php
unset($r);
