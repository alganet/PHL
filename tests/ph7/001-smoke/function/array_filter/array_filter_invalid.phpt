--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--TEST--
array_filter with invalid arguments should return NULL
--FILE--
<?php
// No arguments
$result1 = array_filter();
if ($result1 === null) echo "NULL\n"; else echo "NOT NULL\n";

// Non-array first argument
$result2 = array_filter("not an array");
if ($result2 === null) echo "NULL\n"; else echo "NOT NULL\n";
?>
--EXPECT--
NULL
NULL
--CLEAN--
<?php
unset($result1, $result2);
