--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Double comma in function call is a parse error
--FILE--
<?php
function tcCdFoo($a, $b) { }
tcCdFoo(1,,);
?>
--EXPECTF--
%s Parse error:  syntax error, unexpected token ","
--CLEAN--
<?php
