--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Match expression: two commas in a condition list is a parse error expecting '=>'
--FILE--
<?php
$x = 'a';
echo match ($x) { 'a', , 'b' => "hit" };
echo "never\n";
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected token ",", expecting "=>"%A
