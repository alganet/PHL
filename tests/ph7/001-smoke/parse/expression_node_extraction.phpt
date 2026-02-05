--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--TEST--
Test various expression node extractions and keyword handling
--FILE--
<?php
// Test expression node extraction for different token types
$a = 1;
$b = 2;

// Test array keyword in expression
$arr = array(1, 2, 3);
$result1 = count($arr) + $a;
var_dump($result1); // 4

// Test list keyword (though not in expression context)
list($x, $y) = array(10, 20);
$result2 = $x + $y + $b;
var_dump($result2); // 32

// Test function keyword in anonymous function
$func = function($param) {
    return $param * 2;
};
$result3 = $func($a) + $b;
var_dump($result3); // 4

// Test various literals and identifiers
$var_name = 'test';
$result4 = strlen($var_name) + $a;
var_dump($result4); // 5
?>
--EXPECT--
int(4)
int(32)
int(4)
int(5)
--CLEAN--
<?php
unset($a, $b, $arr, $result1, $result2, $func, $result3, $var_name, $result4);
