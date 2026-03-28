--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Syntax error: Missing opening brace in anonymous function
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$f = function();
?>
--EXPECTF--
%s Error:  Syntax error while declaring annonymous function %s
--CLEAN--
<?php
unset($f);
