--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Arrow function: nested arrow's default values capture outer vars, param names do not leak
--FILE--
<?php
// The inner arrow's $x and $y are parameter names and must NOT be captured
// into the outer closure env. The default-value expressions, however, are
// evaluated in the outer frame and any free variable there must be captured.
$limit = 100;
$outer = fn() => fn($x = 0, $y = 5) => $x + $y + $limit;
$inner = $outer();
echo $inner(), "\n";
echo $inner(1, 2), "\n";

// Sanity: an outer-scope $x with the same name as the nested arrow's param
// must not affect the inner arrow's parameter binding.
$x = 999;
$wrap = fn() => fn($x = 10) => $x * 2;
$ff = $wrap();
echo $ff(), "\n";
echo $ff(7), "\n";
?>
--EXPECT--
105
103
20
14
--CLEAN--
<?php
unset($limit, $outer, $inner, $x, $wrap, $ff);
