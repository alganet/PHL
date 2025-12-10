--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
print_r outputs variable information
--SKIPIF--
<?php
if (!function_exists('print_r')) { echo 'skip: print_r not available'; }
if (function_exists('zend_version')) { echo 'skip: PHP print_r output differs'; }
?>
--FILE--
<?php
$output = print_r("test", true);
if (strpos($output, 'test') !== false) { 
    echo "print_r_string_ok\n"; 
} else { 
    echo "print_r_string_failed\n"; 
}
$array = array('a' => 1, 'b' => 2);
$output = print_r($array, true);
if (strpos($output, 'a] =>') !== false && strpos($output, 'b] =>') !== false) { 
    echo "print_r_array_ok\n"; 
} else { 
    echo "print_r_array_failed\n"; 
}
?>
--EXPECT--
print_r_string_ok
print_r_array_ok
