--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Anonymous function syntax error: missing '{'
--FILE--
<?php
$func = function() echo "test";
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected token "echo", expecting "{"%A
--CLEAN--
<?php
unset($func);
