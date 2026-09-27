--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Binding a reference to `clone` is a parse error
--FILE--
<?php
class WriteTargetRefClone {}
$o = new WriteTargetRefClone;
$r =& clone $o;
?>
--EXPECTF--
%Asyntax error, unexpected token "clone"%A
