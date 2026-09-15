--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
TypeError: userland callees carry php's ", called in FILE on line N" suffix; internals do not
--FILE--
<?php
declare(strict_types=1);
function tecs(int $x) {}
class TECS { function m(int $x) {} }
$fn = function (int $x) {};

try { tecs("s"); } catch (\TypeError $e) { echo $e->getMessage(), "\n"; }
try { (new TECS)->m("s"); } catch (\TypeError $e) { echo $e->getMessage(), "\n"; }
try { $fn("s"); } catch (\TypeError $e) { echo $e->getMessage(), "\n"; }
try { strlen([]); } catch (\TypeError $e) { echo $e->getMessage(), "\n"; }
?>
--EXPECTF--
tecs(): Argument #1 ($x) must be of type int, string given, called in %s on line 7
TECS::m(): Argument #1 ($x) must be of type int, string given, called in %s on line 8
{closure:%s:5}(): Argument #1 ($x) must be of type int, string given, called in %s on line 9
strlen(): Argument #1 ($string) must be of type string, array given
