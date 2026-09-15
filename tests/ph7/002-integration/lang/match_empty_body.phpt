--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Match expression: empty body raises UnhandledMatchError at runtime
--FILE--
<?php
$r = match (1) { };
echo "never\n";
?>
--EXPECTF--
%APHP Fatal error:  Uncaught UnhandledMatchError: Unhandled match case of type int in %s:2%A
--CLEAN--
<?php
