--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A method takes no `readonly` modifier
--DESCRIPTION--
The method half of the same per-kind screen.
--FILE--
<?php
class MmrC { public readonly function f() {} }
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Cannot use the readonly modifier on a method %s
--CLEAN--
<?php
