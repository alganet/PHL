--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_map re-indexes the result when several arrays are given
--DESCRIPTION--
A single array keeps its keys; with several arrays the result is re-indexed with
sequential integers even when an input array is associative.
--FILE--
<?php
$assoc = ['x' => 1, 'y' => 2];
$other = [10, 20];
$r = array_map(function ($a, $b) { return $a . ':' . $b; }, $assoc, $other);
echo implode(',', array_keys($r)), PHP_EOL;   // 0,1
echo $r[0], ',', $r[1], PHP_EOL;               // 1:10,2:20
?>
--EXPECT--
0,1
1:10,2:20
--CLEAN--
<?php
unset($assoc, $other, $r);
