--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The comma operator is a parse error (php has no comma operator)
--FILE--
<?php
$a = 1;
$b = 2;
$x = ($a++, $b);
echo $x;
?>
--EXPECTF--
%s Parse error:  syntax error, unexpected token ","%A
--CLEAN--
<?php
