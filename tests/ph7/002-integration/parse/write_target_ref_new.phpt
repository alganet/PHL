--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Binding a reference to `new` is php 4 syntax and a parse error
--FILE--
<?php
// php enters its `&new` production and then wants the argument list. PHL bound
// a copy in silence.
class WriteTargetRefNew {}
$r =& new WriteTargetRefNew;
?>
--EXPECTF--
%Asyntax error, unexpected token ";", expecting "("%A
