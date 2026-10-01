--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A bare `static` in an expression is a parse error naming the token after it
--FILE--
<?php
$x = static;
echo "should not reach here\n";
?>
--EXPECTF--
%s Parse error:  syntax error, unexpected token ";", expecting "::" in %s on line 2
