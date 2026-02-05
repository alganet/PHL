--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Return a reference to an array element from a function and change it.
--FILE--
<?php
function &ref_elem(&$arr, $k){ return $arr[$k]; }
$a = array('k' => 1);
$b_ref =& ref_elem($a, 'k');
$b_ref = 10;
echo $a['k'] . "\n"; // 10 now

// Store reference to array element into another array index
$arr2 = array();
$arr2['x'] =& ref_elem($a, 'k');
echo $arr2['x'] . "\n";
$a['k'] = 20;
echo $arr2['x'] . "\n";
?>
--EXPECT--
10
10
20
--CLEAN--
<?php
unset($a, $b_ref, $arr2);
