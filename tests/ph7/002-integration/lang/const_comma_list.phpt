--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: top-level `const` declares a comma-separated list at once
--FILE--
<?php
const A = 1, B = 2, C = 3;
echo A, B, C, "\n";
// A later name in the same statement may reference an earlier one.
const D = 4, E = D * 10;
echo D, ",", E, "\n";
// Arrays and folded arithmetic in each initializer.
const F = [1, 2], G = 3 + 4;
echo F[0], F[1], ",", G, "\n";
// A single-const statement still works unchanged.
const H = 9;
echo H, "\n";
?>
--EXPECT--
123
4,40
12,7
9
--CLEAN--
<?php

