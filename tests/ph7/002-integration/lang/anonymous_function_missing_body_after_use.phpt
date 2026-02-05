--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: Syntax error in anonymous function declaration - missing body after use
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$func = function() use ($x) ;
?>
--EXPECTF--
%s %d Error: Syntax error while declaring annonymous function
Compile error
--CLEAN--
<?php
unset($func);
