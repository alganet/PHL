--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A parameter default is a constant expression: a call in one is a compile-time fatal
--FILE--
<?php
function cesParam($x = strlen('a')) { return $x; }
echo "unreachable\n";
?>
--EXPECTF--
%AConstant expression contains invalid operations%A
--CLEAN--
<?php
