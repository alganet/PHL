--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_walk_recursive should visit nested array elements
--FILE--
<?php
// Avoid closure use of pass by reference which is disabled; use named function instead
$in = array('a' => array('x' => 1, 'y' => 2), 'b' => array('z' => 3));
$keys = array();
function _ph7_walker($v, $k) {
	global $keys;
	$keys[] = $k;
}
array_walk_recursive($in, '_ph7_walker');
echo implode(',', $keys) . PHP_EOL; // 'x,y,z'
?>
--EXPECT--
x,y,z
--CLEAN--
<?php
unset($in,$keys);
?>
