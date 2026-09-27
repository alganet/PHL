--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Incrementing a class constant is a parse error
--FILE--
<?php
// PHL ran this one in complete silence.
class WriteTargetClassConstIncr { const K = 5; }
WriteTargetClassConstIncr::K++;
?>
--EXPECTF--
%Asyntax error, unexpected token "++"%A
