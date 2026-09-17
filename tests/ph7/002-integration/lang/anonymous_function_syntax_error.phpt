--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
an unterminated closure use-list names the stray "{" and expects ")" (was a bare skip: PHL scanned the use-list as nested brackets and named the ";")

--FILE--
<?php
$func = function() use ($x { };
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected token "{", expecting ")"%A
--CLEAN--
<?php
unset($func);
