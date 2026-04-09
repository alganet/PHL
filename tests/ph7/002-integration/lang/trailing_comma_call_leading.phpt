--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Leading comma in function call is a parse error
--FILE--
<?php
function tcClFoo($a) { }
tcClFoo(,);
?>
--EXPECTF--
%s Parse error:  syntax error, unexpected token ","
--CLEAN--
<?php
