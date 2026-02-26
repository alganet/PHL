--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Providing only the main array and callback should return the input
--FILE--
<?php
$a = array('x'=>1,'y'=>2);
$r = array_diff_uassoc($a, function($a,$b){return 0;});
echo count($r) . ',' . implode(',', array_keys($r)) . PHP_EOL;
?>
--EXPECT--
2,x,y
--CLEAN--
<?php
unset($a, $r);
