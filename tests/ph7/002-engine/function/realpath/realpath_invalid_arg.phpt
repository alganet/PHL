--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
realpath() invalid argument type (expect false)
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
?>
--FILE--
<?php
// Passing an array should be invalid
echo (realpath(array()) === false ? 'false' : 'true') . "\n";
?>
--EXPECT--
false

