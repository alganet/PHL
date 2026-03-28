--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Syntax error: Missing opening parenthesis in anonymous function
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$f = function echo 1;
?>
--EXPECTF--
%s Error:  Missing opening parenthesis '(' while declaring annonymous function %s
--CLEAN--
<?php
unset($f);
