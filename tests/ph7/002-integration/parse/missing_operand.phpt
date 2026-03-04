--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Test missing operand error in expression parsing
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test missing operand after unary operator
$result = +;

// Test missing operand after binary operator
$result2 = 5 + ;

// Test missing operand in complex expression
$result3 = (2 * ) + 3;
?>
--EXPECTF--
%s %d Error:  '+': Missing operand
%s %d Error:  '+': Missing/Invalid operand
%s %d Error:  '*': Missing/Invalid operand
Compile error
--CLEAN--
<?php
unset($result, $result2, $result3);
