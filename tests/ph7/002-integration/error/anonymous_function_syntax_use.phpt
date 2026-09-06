--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Syntax error: Invalid token after 'use' in anonymous function
--FILE--
<?php
$f = function() use invalid { };
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected identifier "invalid", expecting "("%A
--CLEAN--
<?php
unset($f);
