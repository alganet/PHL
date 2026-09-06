--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Syntax error: Missing opening parenthesis in anonymous function
--FILE--
<?php
$f = function echo 1;
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected token "echo", expecting "("%A
--CLEAN--
<?php
unset($f);
