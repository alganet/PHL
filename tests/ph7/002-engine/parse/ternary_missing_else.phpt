--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Test ternary operator missing else syntax error
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test ternary operator without else part (should trigger syntax error)
$result = true ? "true";
?>
--EXPECTF--
%s 3 Error: Syntax error,mismatched '(','[','{' or '?'
Compile error