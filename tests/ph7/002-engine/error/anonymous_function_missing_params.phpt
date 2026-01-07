--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: Syntax error while declaring anonymous function
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$f = function {
    echo "hello";
};
?>
--EXPECTF--
%s 2 Error: Missing opening parenthesis '(' while declaring annonymous function
Compile error
