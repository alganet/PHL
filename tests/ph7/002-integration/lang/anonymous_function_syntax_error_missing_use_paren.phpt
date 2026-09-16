--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
closure use() without parentheses names the offending variable (was a bare skip freezing the nameless variable \"$\")
--FILE--
<?php
$func = function() use $x { };
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected variable "$x", expecting "("%A
--CLEAN--
<?php
unset($func);
