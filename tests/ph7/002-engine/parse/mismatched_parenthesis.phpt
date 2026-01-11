--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
mismatched parenthesis
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
echo (1 + 2;
?>
--EXPECTF--
%s 2 Error: Syntax error,mismatched '(','[','{' or '?'
Compile error
