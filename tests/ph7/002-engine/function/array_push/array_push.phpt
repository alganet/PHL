--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_push should append values to the array and return new count
--FILE--
<?php
$a = array();
$count = array_push($a, 'a', 'b');
echo $count . PHP_EOL; // 2
echo implode(',', $a) . PHP_EOL; // a,b
?>
--EXPECT--
2
a,b
--CLEAN--
<?php
unset($a,$count);
?>
