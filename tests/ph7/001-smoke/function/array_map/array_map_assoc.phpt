--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_map preserves keys of an associative array
--FILE--
<?php
$r = array_map(function($v) { return $v + 1; }, array('x' => 1, 'y' => 2));
echo $r['x'] . PHP_EOL;
echo $r['y'] . PHP_EOL;
?>
--EXPECT--
2
3
--CLEAN--
<?php
unset($r);
