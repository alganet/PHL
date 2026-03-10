--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_walk_recursive modifies leaf values when callback takes value by reference
--FILE--
<?php
$in = array('a' => array('x' => 1, 'y' => 2), 'b' => 3);
array_walk_recursive($in, function(&$v, $k) { $v = $v * 10; });
echo $in['a']['x'] . ',' . $in['a']['y'] . ',' . $in['b'];
?>
--EXPECT--
10,20,30
--CLEAN--
<?php
unset($in);
