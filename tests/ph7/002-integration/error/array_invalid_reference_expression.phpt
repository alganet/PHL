--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: array with invalid reference to expression
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$a = array(&$b + 1);
?>
--EXPECTF--
%s Error:  array(): Expecting a variable/array member/function call after reference operator '&' %s
--CLEAN--
<?php
unset($a);
