--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Match expression: an arm that opens on ',' is a parse error naming the closing brace
--FILE--
<?php
$x = 'a';
echo match ($x) { 'z' => 1, , 'a' => "hit" };
echo "never\n";
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected token ",", expecting "}"%A
