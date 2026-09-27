--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A PREFIX decrement stops past the constant, not at the operator
--FILE--
<?php
// php has already shifted `C::K` as a constant here, so it reports the token
// that follows it and still asks for the dereference that would have made it a
// variable.
class WriteTargetClassConstPre { const K = 5; }
--WriteTargetClassConstPre::K;
?>
--EXPECTF--
%Asyntax error, unexpected token ";", expecting "->" or "?->" or "["%A
