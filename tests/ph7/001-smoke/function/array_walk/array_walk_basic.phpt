--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_walk calls callback for each element with value and key
--FILE--
<?php
$a = array('a' => 1, 'b' => 2, 'c' => 3);
$collect = array();
function walk_basic($v, $k) {
    global $collect;
    $collect[] = $k . ':' . $v;
}
array_walk($a, 'walk_basic');
echo implode(',', $collect);
?>
--EXPECT--
a:1,b:2,c:3
--CLEAN--
<?php
unset($a, $collect);
