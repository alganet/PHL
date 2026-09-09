--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Assignment binds to the immediate lvalue inside a comparison (false !== $x = f())
--FILE--
<?php
$s = "a\\b\\c"; $out = [];
if (false !== $pos = strrpos($s, "\\")) { $out[] = "pos=$pos"; }
$n = 0; $t = "aXbXcX"; while (false !== $i = strpos($t, "X")) { $n++; $t = substr($t, $i + 1); }
$out[] = "loops=$n";
$a = 5; $b = $a == 5; $out[] = "b=" . var_export($b, true);
$c = 3; if ($x = $c + 1) { $out[] = "x=$x"; }
$o = new stdClass; $o->v = 0; if (0 === $o->v = 9) {} $out[] = "ov={$o->v}";
$r = true && $y = 7; $out[] = "y=$y r=" . var_export($r, true);
echo implode("\n", $out), "\n";
--EXPECT--
pos=3
loops=3
b=true
x=4
ov=9
y=7 r=true
--CLEAN--
<?php
