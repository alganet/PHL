--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A global const name must be an identifier, not a reserved word
--FILE--
<?php
// php's `const` at file scope takes T_STRING: only a CLASS constant may carry a
// reserved word. PHL accepted both, so this compiled and read back.
const list = 1;
?>
--EXPECTF--
%Asyntax error, unexpected token "list", expecting identifier%A
