--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Test ternary operator missing else syntax error
--FILE--
<?php
// Test ternary operator without else part (should trigger syntax error)
$result = true ? "true";
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected token ";"%A
--CLEAN--
<?php
unset($result);
