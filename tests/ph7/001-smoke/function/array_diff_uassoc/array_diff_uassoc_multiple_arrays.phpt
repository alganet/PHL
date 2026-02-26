--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_diff_uassoc should work with more than two arrays
--FILE--
<?php
$array1 = array('a' => 1, 'b' => 2, 'c' => 3);
$array2 = array('a' => 1);
$array3 = array('c' => 3);
$result = array_diff_uassoc($array1, $array2, $array3,
    function($a, $b) { return strcmp($a, $b); }
);
echo implode(',', array_keys($result)) . PHP_EOL; // expecting 'b'
?>
--EXPECT--
b
--CLEAN--
<?php
unset($array1, $array2, $array3, $result);
