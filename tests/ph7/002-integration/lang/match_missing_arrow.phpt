--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Match expression: missing '=>' between condition and result is a parse error
--FILE--
<?php
$r = match (1) { 1 2 };
echo "never\n";
?>
--EXPECTF--
%Asyntax error,%Aexpecting "=>"%A
--CLEAN--
<?php
