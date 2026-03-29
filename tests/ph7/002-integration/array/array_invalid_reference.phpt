--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array() with invalid reference expression
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$a = array(&$b + $c);
echo "Should not reach here\n";
?>
--EXPECTF--
%s Fatal error:  array(): Expecting a variable/array member/function call after reference operator '&' %s
--CLEAN--
<?php
unset($a);
