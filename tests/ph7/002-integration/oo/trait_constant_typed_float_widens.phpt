--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A typed trait constant's int default composes with a float one where the type converts it
--DESCRIPTION--
php converts an int to float when it checks a typed constant's declaration, for a
type that admits float but not int, so `const float K = 1` and `const float K = 1.0`
hold the same value and compose -- against the class body and between two traits.
--FILE--
<?php
trait TctfA {
    const float K = 1;
    const ?float N = 2;
    const float|string S = 3;
    public const float X = 4.0;
}
class TctfC {
    use TctfA;
    const float K = 1.0;
    const ?float N = 2.0;
    const float|string S = 3.0;
    public const float X = 4;
}
var_dump(TctfC::K, TctfC::N, TctfC::S, TctfC::X);
trait TctfB { const float K = 1; }
trait TctfD { const float K = 1.0; }
class TctfE { use TctfB, TctfD; }
var_dump(TctfE::K);
?>
--EXPECT--
float(1)
float(2)
float(3)
float(4)
float(1)
