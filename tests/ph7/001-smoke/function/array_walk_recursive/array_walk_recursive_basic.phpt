--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_walk_recursive visits nested array leaf elements
--FILE--
<?php
$in = array('a' => array('x' => 1, 'y' => 2), 'b' => array('z' => 3));
$keys = array();
function walk_rec_basic($v, $k) {
    global $keys;
    $keys[] = $k;
}
array_walk_recursive($in, 'walk_rec_basic');
echo implode(',', $keys);
?>
--EXPECT--
x,y,z
--CLEAN--
<?php
unset($in, $keys);
