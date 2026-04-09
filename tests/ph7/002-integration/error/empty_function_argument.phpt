--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Syntax error: Empty function argument
--FILE--
<?php
func(, 2);
?>
--EXPECTF--
%s Parse error:  syntax error, unexpected token ","
--CLEAN--
<?php
