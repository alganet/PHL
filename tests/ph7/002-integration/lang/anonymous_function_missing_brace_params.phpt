--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Syntax error in anonymous function with parameters but missing opening brace '{'
--FILE--
<?php
// Test malformed anonymous function syntax that triggers parsing error for missing '{'

$func = function($x) ;
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected token ";", expecting "{"%A
--CLEAN--
<?php
unset($func);
