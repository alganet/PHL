--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed class constants (PHP 8.3): scalar, array and int->float widening
--FILE--
<?php
class TypedConstScalar {
    const int A = 1;
    const float B = 2;        // int literal widened to float
    const string D = "x";
    const bool E = true;
    const array F = [10, 20];
}
echo TypedConstScalar::A, "\n";
echo TypedConstScalar::B, "\n";
echo gettype(TypedConstScalar::B), "\n";     // double (the value was widened)
echo TypedConstScalar::D, "\n";
echo TypedConstScalar::E ? "true" : "false", "\n";
echo TypedConstScalar::F[0] + TypedConstScalar::F[1], "\n";
?>
--EXPECT--
1
2
double
x
true
30
--CLEAN--
<?php
