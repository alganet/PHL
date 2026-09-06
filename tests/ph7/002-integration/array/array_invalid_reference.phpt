--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array() with invalid reference expression
--FILE--
<?php
$a = array(&$b + $c);
echo "Should not reach here\n";
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected token "+", expecting "->" or "?->" or "["%A
--CLEAN--
<?php
unset($a);
