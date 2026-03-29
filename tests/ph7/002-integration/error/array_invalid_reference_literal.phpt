--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: array with invalid reference to literal
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$a = array(&1);
?>
--EXPECTF--
%s Fatal error:  array(): Expecting a variable after reference operator '&' %s
--CLEAN--
<?php
unset($a);
