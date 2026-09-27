--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A foreach target that is a class constant is a parse error
--FILE--
<?php
class WriteTargetForeachConst { const K = 5; }
foreach ([1] as WriteTargetForeachConst::K) {}
?>
--EXPECTF--
%Asyntax error, unexpected token ")", expecting "->" or "?->" or "["%A
