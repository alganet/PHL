--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_fill should populate array with repeated values
--FILE--
<?php
$a = array_fill(0, 3, 'x');
echo count($a) . PHP_EOL; // 3
echo implode(',', $a) . PHP_EOL; // x,x,x
?>
--EXPECT--
3
x,x,x
--CLEAN--
<?php
unset($a);
?>
