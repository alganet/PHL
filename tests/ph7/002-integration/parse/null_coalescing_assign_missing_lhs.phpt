--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Null coalescing assignment without a left operand is a parse error
--FILE--
<?php
??= 5;
?>
--EXPECTF--
%s Parse error:  syntax error, unexpected token "??=%s
--CLEAN--
<?php
