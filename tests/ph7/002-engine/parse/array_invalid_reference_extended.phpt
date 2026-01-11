--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array() with extended invalid reference expressions
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$a = array(&$b++);
$a = array(&$c->d);
$a = array(&$e + $f);
echo "Should not reach here\n";
?>
--EXPECTF--
%s 2 Error: array(): Expecting a variable/array member/function call after reference operator '&'
%s 4 Error: array(): Expecting a variable/array member/function call after reference operator '&'
Compile error