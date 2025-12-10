--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
var_dump outputs variable information
--SKIPIF--
<?php
if (!function_exists('var_dump')) { echo 'skip: var_dump not available'; }
if (function_exists('zend_version')) { echo 'skip: PHP var_dump output differs'; }
?>
--FILE--
<?php
ob_start();
var_dump("test");
$output = ob_get_clean();
if (strpos($output, 'string') !== false && strpos($output, 'test') !== false) { 
    echo "var_dump_string_ok\n"; 
} else { 
    echo "var_dump_string_failed\n"; 
}
ob_start();
var_dump(42);
$output = ob_get_clean();
if (strpos($output, 'int(42)') !== false) { 
    echo "var_dump_int_ok\n"; 
} else { 
    echo "var_dump_int_failed\n"; 
}
?>
--EXPECT--
var_dump_string_ok
var_dump_int_ok
