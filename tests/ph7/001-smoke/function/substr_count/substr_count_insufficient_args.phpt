--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
substr_count with insufficient arguments
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test with 0 arguments
$result1 = substr_count();
echo $result1 === 0 ? 'PASS' : 'FAIL';

// Test with 1 argument
$result2 = substr_count('hello');
echo $result2 === 0 ? 'PASS' : 'FAIL';
?>
--EXPECT--
PASSPASS
--CLEAN--
<?php
unset($result1, $result2);
