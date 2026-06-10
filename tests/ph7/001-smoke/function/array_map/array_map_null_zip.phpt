--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_map with a NULL callback and several arrays zips them together
--DESCRIPTION--
With more than one array and a NULL callback, PHP builds an array whose i-th
element is an array of the i-th element of every input array.
--FILE--
<?php
$n = [1, 2];
$s = ['a', 'b'];
$r = array_map(null, $n, $s);
echo $r[0][0], ',', $r[0][1], PHP_EOL;   // 1,a
echo $r[1][0], ',', $r[1][1], PHP_EOL;   // 2,b
?>
--EXPECT--
1,a
2,b
--CLEAN--
<?php
unset($n, $s, $r);
