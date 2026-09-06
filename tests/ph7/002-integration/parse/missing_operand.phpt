--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Test missing operand error in expression parsing
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
%AParse error:%Asyntax error, unexpected token ";"%A
--CLEAN--
<?php
unset($result, $result2, $result3);
