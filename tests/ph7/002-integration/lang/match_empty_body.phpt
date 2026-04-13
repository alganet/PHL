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
%AUncaught UnhandledMatchError: Unhandled match case%A
--CLEAN--
<?php
