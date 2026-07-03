--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Deep recursion (under the cap): default args re-evaluated per call, named args, variadics
--FILE--
<?php
function down(int $n, int $step = 1): int {
    return $n <= 0 ? 0 : 1 + down($n - $step);
}
echo down(25), "\n";
function nsum(int $n, int $mul = 2): int {
    return $n === 0 ? 0 : $mul + nsum(n: $n - 1);
}
echo nsum(20), "\n";
function vcount(int $n, ...$rest): int {
    return $n === 0 ? count($rest) : vcount($n - 1, $n, ...$rest);
}
echo vcount(10), "\n";
?>
--EXPECT--
25
40
10
--CLEAN--
<?php
