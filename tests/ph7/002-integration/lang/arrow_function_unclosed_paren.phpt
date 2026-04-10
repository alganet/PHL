--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Arrow function: unclosed parameter list
--FILE--
<?php
$f = fn($x => $x;
?>
--EXPECTF--
%A Parse error:%Asyntax error, unexpected %A, expecting ")"%A
--CLEAN--
<?php
