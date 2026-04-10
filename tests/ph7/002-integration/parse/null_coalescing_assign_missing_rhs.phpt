--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Null coalescing assignment without a right operand is a parse error
--FILE--
<?php
$a ??= ;
?>
--EXPECTF--
%s Parse error:  syntax error, unexpected token %s
--CLEAN--
<?php
