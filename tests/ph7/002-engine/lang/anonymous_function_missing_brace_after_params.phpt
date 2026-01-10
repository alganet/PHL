--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Anonymous function syntax error: missing brace after parameters
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$func = function($x, $y)
echo "test";
?>
--EXPECTF--
%s %d Error: Syntax error while declaring annonymous function
Compile error