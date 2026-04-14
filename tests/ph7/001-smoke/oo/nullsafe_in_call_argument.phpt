--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Nullsafe result as a function argument is isolated from surrounding chain
--FILE--
<?php
class NsfCallArgThing { public $v = 7; }
function nsfCallArg_label($x) { return $x === null ? "null" : "val:$x"; }
$nsfCallArg_a = null;
echo nsfCallArg_label($nsfCallArg_a?->v), "\n";
$nsfCallArg_b = new NsfCallArgThing();
echo nsfCallArg_label($nsfCallArg_b?->v), "\n";
?>
--EXPECT--
null
val:7
--CLEAN--
<?php
unset($nsfCallArg_a, $nsfCallArg_b);
