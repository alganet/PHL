--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Backtick quoted string processing
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip php only deprecates the backtick operator, and the stock php.ini masks E_DEPRECATED'; ?>
--FILE--
<?php
// Test backtick quoted string processing - this should trigger uncovered lexer code
// According to the lexer code, backtick strings are disabled but the processing path exists
echo `echo "test"`;
?>
--EXPECTF--
%ADeprecated:%AThe backtick (`) operator is deprecated, use shell_exec() instead%Atest%A
--CLEAN--
<?php

