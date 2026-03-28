--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Invalid expressions
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test invalid syntax that triggers compile error
$var = ;

echo "Should not reach here\n";
?>
--EXPECTF--
%s Error:  '=': Missing/Invalid operand %s
--CLEAN--
<?php
unset($var);
