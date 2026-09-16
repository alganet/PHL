--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
isset() with no operand is a parse error on the ')' (the isset twin of empty_no_args)
--FILE--
<?php
var_dump(isset());
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected token ")"%A
--CLEAN--
<?php
