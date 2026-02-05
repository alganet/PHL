--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Test ternary operator missing condition syntax error
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test ternary operator without condition (should trigger syntax error)
$result = ? "true" : "false";
?>
--EXPECTF--
%s 3 Error: '?': Syntax error
Compile error
--CLEAN--
<?php
unset($result);
