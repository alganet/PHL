--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: `/` and `%` by zero throw a catchable DivisionByZeroError
--FILE--
<?php
function dz_probe(callable $fn): string {
    try {
        $fn();
        return "no-throw";
    } catch (DivisionByZeroError $e) {
        return get_class($e) . ": " . $e->getMessage();
    }
}
echo dz_probe(fn() => 1 / 0), "\n";
echo dz_probe(fn() => 7.5 / 0), "\n";
echo dz_probe(fn() => 0 / 0), "\n";
echo dz_probe(fn() => 1 % 0), "\n";
echo dz_probe(fn() => 5 % 0), "\n";
echo dz_probe(function () { $x = 5; $x /= 0; }), "\n";
echo dz_probe(function () { $x = 5; $x %= 0; }), "\n";
echo dz_probe(function () { $a = [10]; $a[0] %= 0; }), "\n";
// DivisionByZeroError is an ArithmeticError is an Error
try {
    $r = 1 % 0;
} catch (ArithmeticError $e) {
    echo "base-catch: ", $e->getMessage(), "\n";
}
// execution continues normally after a caught division-by-zero
echo "after: ", 10 % 3, " ", 10 / 4, "\n";
?>
--EXPECT--
DivisionByZeroError: Division by zero
DivisionByZeroError: Division by zero
DivisionByZeroError: Division by zero
DivisionByZeroError: Modulo by zero
DivisionByZeroError: Modulo by zero
DivisionByZeroError: Division by zero
DivisionByZeroError: Modulo by zero
DivisionByZeroError: Modulo by zero
base-catch: Modulo by zero
after: 1 2.5
--CLEAN--
<?php
