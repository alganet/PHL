--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: array() with missing entry value should produce a compile error
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$x = array(1 => );
?>
--EXPECTF--
%s 2 Error:  array(): Missing entry value
Compile error
--CLEAN--
<?php
unset($x);
