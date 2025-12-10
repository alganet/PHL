--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHP: parse_ini_string with empty string
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo 'skip';
}
?>
--FILE--
<?php
// Test empty string
$empty = parse_ini_string("");
echo $empty === false ? "EMPTY_OK\n" : "EMPTY_FAIL\n";
?>
--EXPECT--
EMPTY_FAIL
