--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed class constants (PHP 8.3): scalar, array and int->float widening
--SKIPIF--
<?php if (function_exists('zend_version') && version_compare(PHP_VERSION, '8.5.0', '<')) echo 'skip Requires PHP 8.5+'; ?>
--FILE--
<?php
class TypedConstScalar {
    const int A = 1;
    const float B = 2;        // int literal widened to float
    const string D = "x";
    const bool E = true;
    const array F = [10, 20];
    const int G = 1024 / 4;   // computed int-valued expression stays accepted
    const int H = 4 / 2;      // evenly-dividing division stays accepted
    const int I = (int) 1.5;  // explicit int cast stays accepted
}
echo TypedConstScalar::A, "\n";
echo TypedConstScalar::B, "\n";
echo gettype(TypedConstScalar::B), "\n";     // double (the value was widened)
echo TypedConstScalar::D, "\n";
echo TypedConstScalar::E ? "true" : "false", "\n";
echo TypedConstScalar::F[0] + TypedConstScalar::F[1], "\n";
echo TypedConstScalar::G, "\n";              // 256 (echo renders whole values the same on both engines)
echo TypedConstScalar::H, "\n";              // 2
echo TypedConstScalar::I, "\n";              // 1
?>
--EXPECT--
1
2
double
x
true
30
256
2
1
--CLEAN--
<?php
