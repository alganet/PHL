--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--TEST--
array() with invalid reference expression
--FILE--
<?php
$a = array(&$b + $c);
echo "Should not reach here\n";
?>
--EXPECTF--
%s 2 Error: array(): Expecting a variable/array member/function call after reference operator '&'
Compile error
--CLEAN--
<?php
unset($a);
