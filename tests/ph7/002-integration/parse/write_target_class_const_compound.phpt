--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A compound assign to a class constant is a parse error
--FILE--
<?php
class WriteTargetClassConstCompound { const K = 5; }
WriteTargetClassConstCompound::K += 5;
?>
--EXPECTF--
%Asyntax error, unexpected token "+="%A
