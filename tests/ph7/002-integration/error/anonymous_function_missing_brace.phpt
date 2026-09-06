--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Syntax error: Missing opening brace in anonymous function
--FILE--
<?php
$f = function();
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected token ";", expecting "{"%A
--CLEAN--
<?php
unset($f);
