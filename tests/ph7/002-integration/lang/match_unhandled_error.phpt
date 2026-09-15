--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Match expression: no matching arm and no default raises UnhandledMatchError
--FILE--
<?php
$r = match (99) { 1 => 'one', 2 => 'two' };
echo "never\n";
?>
--EXPECTF--
%APHP Fatal error:  Uncaught UnhandledMatchError: Unhandled match case of type int in %s:2%A
--CLEAN--
<?php
