--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_pad should pad arrays with a value at the end or beginning based on positive/negative size
--FILE--
<?php
$a = array(1,2);
$p = array_pad($a, 4, 'x');
echo implode(',', $p) . PHP_EOL; // 1,2,x,x
$q = array_pad($a, -4, 'x');
echo implode(',', $q) . PHP_EOL; // x,x,1,2
$r = array_pad($a, 1, 'x');
echo implode(',', $r) . PHP_EOL; // 1,2
?>
--EXPECT--
1,2,x,x
x,x,1,2
1,2
--CLEAN--
<?php
unset($a,$p,$q,$r);
?>
