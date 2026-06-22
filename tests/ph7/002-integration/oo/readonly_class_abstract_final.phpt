--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
the mutually-exclusive abstract+final class modifiers are rejected
--FILE--
<?php
abstract final class RoAbsFin {}
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Cannot use the final modifier on an abstract class%A
--CLEAN--
<?php
