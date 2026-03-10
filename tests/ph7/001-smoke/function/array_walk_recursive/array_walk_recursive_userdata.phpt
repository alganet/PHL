--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_walk_recursive passes userdata to callback
--FILE--
<?php
$in = array('a' => array(1), 'b' => 2);
$output = '';
function walk_rec_ud($v, $k, $extra) {
    global $output;
    $output .= $k . ':' . $v . ':' . $extra . ' ';
}
array_walk_recursive($in, 'walk_rec_ud', 'Z');
echo trim($output);
?>
--EXPECT--
0:1:Z b:2:Z
--CLEAN--
<?php
unset($in, $output);
