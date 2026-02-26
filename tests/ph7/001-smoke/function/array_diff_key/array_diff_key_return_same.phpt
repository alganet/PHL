--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
With only one argument array_diff_key should return the original array
--FILE--
<?php
$a = array('x' => 10, 'y' => 20);
$r = array_diff_key($a);
echo count($r) . ',' . implode(',', array_keys($r)) . PHP_EOL;
?>
--EXPECT--
2,x,y
--CLEAN--
<?php
unset($a, $r);
