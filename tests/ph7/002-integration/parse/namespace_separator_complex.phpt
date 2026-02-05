--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: Complex namespace path parsing with multiple separators
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test complex namespace path that triggers assembly logic
$test = \namespace\path\to\some\constant;
?>
--EXPECTF--
%s
--CLEAN--
<?php
unset($test);
