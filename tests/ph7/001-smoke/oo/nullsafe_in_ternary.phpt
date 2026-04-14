--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Nullsafe expression inside a ternary condition short-circuits within the condition scope
--FILE--
<?php
class NsfTernaryFlag { public $on = false; }
$nsfTernary_a = null;
echo ($nsfTernary_a?->on ? "t" : "f"), "\n";
$nsfTernary_b = new NsfTernaryFlag();
echo ($nsfTernary_b?->on ? "t" : "f"), "\n";
$nsfTernary_b->on = true;
echo ($nsfTernary_b?->on ? "t" : "f"), "\n";
?>
--EXPECT--
f
f
t
--CLEAN--
<?php
unset($nsfTernary_a, $nsfTernary_b);
