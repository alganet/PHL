--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_walk_recursive with flat array visits all elements
--FILE--
<?php
$a = array('a' => 1, 'b' => 2);
$output = '';
function walk_rec_flat($v, $k) {
    global $output;
    $output .= $k . ':' . $v . ' ';
}
array_walk_recursive($a, 'walk_rec_flat');
echo trim($output);
?>
--EXPECT--
a:1 b:2
--CLEAN--
<?php
unset($a, $output);
