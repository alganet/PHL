--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A parameter after a variadic one is a compile fatal, so nothing in the file runs
--FILE--
<?php
echo "start\n";
function q(...$a, $b) {}
echo "end\n";
?>
--EXPECTF--
%s Fatal error:  Only the last parameter can be variadic in %s
--CLEAN--
<?php
