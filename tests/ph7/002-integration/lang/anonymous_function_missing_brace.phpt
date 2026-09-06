--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Syntax error in anonymous function with malformed body
--FILE--
<?php
// Test malformed anonymous function syntax that triggers parsing error

// This should trigger a syntax error
$func = function() echo 'test';
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected token "echo", expecting "{"%A
--CLEAN--
<?php
unset($func);
