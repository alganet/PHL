--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: Syntax error in anonymous function declaration - invalid token after parameters
--FILE--
<?php
$func = function() if { };
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected token "if", expecting "{"%A
--CLEAN--
<?php
unset($func);
