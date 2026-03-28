--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: Syntax error in anonymous function declaration - missing use paren
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$func = function() use $x { };
?>
--EXPECTF--
%s Error:  Syntax error while declaring annonymous function %s
--CLEAN--
<?php
unset($func);
