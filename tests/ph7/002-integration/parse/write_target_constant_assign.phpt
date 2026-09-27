--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Assigning to a global constant is a parse error
--FILE--
<?php
define("WRITE_TARGET_ASSIGN_K", 1);
WRITE_TARGET_ASSIGN_K = 5;
?>
--EXPECTF--
%Asyntax error, unexpected token "="%A
