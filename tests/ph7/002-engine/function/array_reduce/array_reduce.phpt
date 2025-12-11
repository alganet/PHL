--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_reduce should fold items from left to right using the callback
--FILE--
<?php
$in = array(1, 2, 3, 4);
$sum = array_reduce($in, function($carry, $item) { return $carry + $item; }, 0);
echo $sum . PHP_EOL; // 10
// reuse with string concat
$out = array_reduce(array('a', 'b', 'c'), function($carry, $item) { return $carry . $item; }, '');
echo $out . PHP_EOL; // 'abc'
?>
--EXPECT--
10
abc
--CLEAN--
<?php
unset($in, $sum, $out);
?>
