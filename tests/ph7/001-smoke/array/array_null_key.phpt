--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Array with null key (converted to empty string)
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test array with null key - should be treated as empty string
$arr = array(null => "value");
echo "Array with null key: " . $arr[""] . "\n";
echo "Count: " . count($arr) . "\n";
?>
--EXPECT--
Array with null key: value
Count: 1
--CLEAN--
<?php
unset($arr);
