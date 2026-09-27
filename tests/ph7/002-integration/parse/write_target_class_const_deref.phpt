--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A subscript of a class constant is php's temporary
--FILE--
<?php
// `C::K[0]` subscripts a COPY of the constant. php refuses the write; PHL
// performed it on the copy and answered nothing at all.
class WriteTargetClassConstDeref { const AK = [1,2]; }
WriteTargetClassConstDeref::AK[0] = 5;
?>
--EXPECTF--
%ACannot use temporary expression in write context%A
