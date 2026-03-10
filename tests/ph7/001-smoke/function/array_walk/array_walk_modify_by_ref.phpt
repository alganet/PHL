--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_walk modifies values when callback takes value by reference
--FILE--
<?php
$a = array('a' => 1, 'b' => 2, 'c' => 3);
array_walk($a, function(&$v, $k) { $v = $v * 2; });
echo $a['a'] . ',' . $a['b'] . ',' . $a['c'];
?>
--EXPECT--
2,4,6
--CLEAN--
<?php
unset($a);
