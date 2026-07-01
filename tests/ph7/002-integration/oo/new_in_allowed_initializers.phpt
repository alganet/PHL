--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
`new` remains allowed in global-constant, parameter-default and static-local initializers
--FILE--
<?php
class X { public $v = 7; }
const GC = new X();
function pd($a = new X()) { return $a->v; }
function sl() { static $s = new X(); return $s->v; }
echo get_class(GC), " ", pd(), " ", sl(), "\n";
?>
--EXPECT--
X 7 7
--CLEAN--
<?php
