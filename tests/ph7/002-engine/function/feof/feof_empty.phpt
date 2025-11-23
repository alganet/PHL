--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Test feof() function with empty files
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "Zend php handles feof on empty files differently";
}
if (!function_exists('fopen') || !function_exists('feof') || !function_exists('fread') || !function_exists('fclose')) {
    echo 'skip: stream functions not available';
}
?>
--FILE--
<?php

// Test with empty file
$emptyFile = tempnam(sys_get_temp_dir(), 'ph7_empty_test');
file_put_contents($emptyFile, '');

$fp = fopen($emptyFile, 'r');
if (feof($fp)) {
    echo "Correctly detected EOF for empty file\n";
} else {
    echo "ERROR: Did not detect EOF for empty file\n";
}
fclose($fp);
unlink($emptyFile);

?>
--EXPECT--
Correctly detected EOF for empty file
--CLEAN--
<?php
// Cleanup handled in test
?>