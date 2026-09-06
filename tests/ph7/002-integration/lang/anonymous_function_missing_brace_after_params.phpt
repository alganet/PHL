--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Anonymous function syntax error: missing brace after parameters
--FILE--
<?php
$func = function($x, $y)
echo "test";
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected token "echo", expecting "{"%A
--CLEAN--
<?php
unset($func);
