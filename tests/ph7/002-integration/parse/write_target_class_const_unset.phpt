--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
unset() of a class constant is a parse error
--FILE--
<?php
class WriteTargetClassConstUnset { const K = 5; }
unset(WriteTargetClassConstUnset::K);
?>
--EXPECTF--
%Asyntax error, unexpected token ")", expecting "->" or "?->" or "["%A
