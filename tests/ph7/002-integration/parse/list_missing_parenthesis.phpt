--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
an unclosed list( is a syntax error expecting ")" (was a bare skip freezing PHL's invented "list: Missing closing parenthesis")

--FILE--
<?php
$a = list(1,2 ;
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected token ";", expecting ")"%A
--CLEAN--
<?php
unset($a);
