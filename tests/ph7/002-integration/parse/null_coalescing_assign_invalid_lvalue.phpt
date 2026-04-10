--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Null coalescing assignment to a non-lvalue is a parse error
--FILE--
<?php
5 ??= 10;
?>
--EXPECTF--
%s Parse error:  syntax error, unexpected token "??=" %s
--CLEAN--
<?php
