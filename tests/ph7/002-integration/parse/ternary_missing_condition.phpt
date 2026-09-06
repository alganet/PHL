--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Test ternary operator missing condition syntax error
--FILE--
<?php
// Test ternary operator without condition (should trigger syntax error)
$result = ? "true" : "false";
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected token "?"%A
--CLEAN--
<?php
unset($result);
