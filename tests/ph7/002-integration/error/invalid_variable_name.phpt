--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: Invalid variable name syntax
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$result = $(;
echo "Result: $result\n";
?>
--EXPECTF--
%s 2 Error:  Syntax error,mismatched '(','[','{' or '?'
Compile error
--CLEAN--
<?php
unset($result);
