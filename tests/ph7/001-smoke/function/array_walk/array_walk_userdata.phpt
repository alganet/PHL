--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_walk passes userdata as third argument to callback
--FILE--
<?php
$a = array(1, 2, 3);
$output = '';
function walk_userdata($v, $k, $extra) {
    global $output;
    $output .= $k . ':' . $v . ':' . $extra . ' ';
}
array_walk($a, 'walk_userdata', 'X');
echo trim($output);
?>
--EXPECT--
0:1:X 1:2:X 2:3:X
--CLEAN--
<?php
unset($a, $output);
