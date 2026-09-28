--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An unimplemented method is attributed to the interface that asked for it
--DESCRIPTION--
An interface reaches its parents through TWO containers -- pBase for the first
name after `extends`, aInterface for every one after it -- and the origin walk
followed only the first, so it stopped at the restating interface.
--FILE--
<?php
interface IaoA { public function a(): int; }
interface IaoS { public function g(): int; }
interface IaoB extends IaoA, IaoS {}
class IaoC implements IaoB { public function a(): int { return 1; } }
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Class IaoC contains 1 abstract method and must therefore be declared abstract or implement the remaining method (IaoS::g) %s
--CLEAN--
<?php
