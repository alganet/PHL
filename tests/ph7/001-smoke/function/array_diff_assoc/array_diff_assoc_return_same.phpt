--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An array passed alone should be returned untouched
--FILE--
<?php
$a = array('x' => 42, 'y' => 99);
$d = array_diff_assoc($a);
// echo count and keys to ensure the returned value equals the input
echo count($d) . ',' . implode(',', array_keys($d)) . PHP_EOL;
?>
--EXPECT--
2,x,y
--CLEAN--
<?php
unset($a, $d);
