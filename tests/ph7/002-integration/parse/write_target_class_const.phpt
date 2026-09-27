--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Assigning to a class constant is a parse error
--FILE--
<?php
// A class constant is a `constant` in php's grammar, not a `variable`, so the
// assignment operator is where php stops. PHL took every `::` for class-level
// STORAGE and reported a PH7-ism at RUNTIME, after the rest of the file ran.
class WriteTargetClassConst { const K = 5; }
WriteTargetClassConst::K = 5;
?>
--EXPECTF--
%Asyntax error, unexpected token "="%A
