--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Match expression: an arm with no condition before '=>' is a parse error
--FILE--
<?php
$x = 1;
echo match ($x) { => 1 };
echo "never\n";
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected token "=>", expecting "}"%A
