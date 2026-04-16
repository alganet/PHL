--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
strict_types accepts exact scalar types and int -> float widening
--FILE--
<?php
declare(strict_types=1);
function st_accept_i(int $x): int { return $x; }
function st_accept_f(float $x): float { return $x; }
function st_accept_s(string $x): string { return $x; }
function st_accept_b(bool $x): bool { return $x; }
echo st_accept_i(42), "\n";
echo st_accept_f(1.5), "\n";
echo st_accept_s("hi"), "\n";
echo st_accept_b(true) ? "t" : "f", "\n";
// int passed to a float param is the one implicit widening strict mode allows.
echo st_accept_f(7), "\n";
?>
--EXPECT--
42
1.5
hi
t
7
