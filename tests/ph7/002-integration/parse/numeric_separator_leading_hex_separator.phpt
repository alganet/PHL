--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Hexadecimal literal with underscore immediately after prefix is a parse error
--FILE--
<?php
$x = 0x_1F;
?>
--EXPECTF--
%s Parse error:  syntax error, unexpected identifier "x_1F" %s
--CLEAN--
<?php
