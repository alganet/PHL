--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Syntax error in anonymous function with malformed body
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test malformed anonymous function syntax that triggers parsing error

// This should trigger a syntax error
$func = function() echo 'test';
?>
--EXPECTF--
%s %d Error:  Syntax error while declaring annonymous function
Compile error
--CLEAN--
<?php
unset($func);
