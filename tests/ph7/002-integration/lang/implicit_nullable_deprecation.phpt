--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php 8.4 implicit-nullable parameter compile deprecation
--FILE--
<?php
function inpF(int $x = null) { return $x; }
class InpC { function m(string $s = null) { return $s; } }
echo inpF(null) === null ? "null-ok" : "bad", "\n";
echo (new InpC)->m() === null ? "default-ok" : "bad", "\n";
?>
--EXPECTF--
%AinpF(): Implicitly marking parameter $x as nullable is deprecated, the explicit nullable type must be used instead in %s on line %d%AInpC::m(): Implicitly marking parameter $s as nullable is deprecated, the explicit nullable type must be used instead in %s on line %d%Anull-ok
default-ok
