--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Deep recursion (under the cap): plain functions, arg/return integrity, branching
--FILE--
<?php
function chain(int $n, string $acc): string {
    if ($n === 0) {
        return $acc . "|end";
    }
    return chain($n - 1, $acc . "." . $n);
}
echo chain(25, "start"), "\n";
function fib(int $n): int {
    return $n < 2 ? $n : fib($n - 1) + fib($n - 2);
}
echo fib(16), "\n";
?>
--EXPECT--
start.25.24.23.22.21.20.19.18.17.16.15.14.13.12.11.10.9.8.7.6.5.4.3.2.1|end
987
--CLEAN--
<?php
