--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A short ternary with no condition is still a parse error, parenthesised or not
--FILE--
<?php
echo (?: 5);
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected token "?"%A
--CLEAN--
<?php
