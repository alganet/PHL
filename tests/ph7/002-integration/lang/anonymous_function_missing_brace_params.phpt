--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Syntax error in anonymous function with parameters but missing opening brace '{'
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test malformed anonymous function syntax that triggers parsing error for missing '{'

$func = function($x) ;
?>
--EXPECTF--
%s Error:  Syntax error while declaring annonymous function %s
--CLEAN--
<?php
unset($func);
