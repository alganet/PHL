--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
getFloat(-PHP_FLOAT_MAX, -PHP_FLOAT_MAX) answers its one double (PHL half)
--DESCRIPTION--
`getFloat($x, $x, ClosedClosed)` is legal and has exactly one reachable answer,
`$x`. It holds for every double PHL was swept over — and in php it holds for
every double but ONE. At `-PHP_FLOAT_MAX`, php's step count comes out as 2^63
rather than 0, which makes the draw's rejection window cover every value with
its top bit set: half of php's draws answer `-PHP_FLOAT_MAX` and the other half
raise `Random\BrokenRandomEngineError`, on a call that asked for the only float
there is. The count php computes there is an out-of-range double-to-integer
conversion, undefined in C and reproducible only by guessing at its assembly,
and the answer it produces is not one any caller could use. PHL answers the one
double for every input, this one included. Found by a 836500-row differential
over twenty endpoint values times four boundaries; it is the only row of it that
is not byte-identical. See getfloat_negative_dbl_max_zend.phpt for php's.
--SKIPIF--
<?php if (function_exists('zend_version')) echo "skip PHL-only half; see the _zend twin"; ?>
--FILE--
<?php
class DblMaxEngine implements Random\Engine
{
    public function __construct(private int $value) {}
    public function generate(): string { return pack('P', $this->value); }
}
$m = -PHP_FLOAT_MAX;
foreach ([0, 1, PHP_INT_MAX, PHP_INT_MIN, -1] as $bits) {
    $r = new Random\Randomizer(new DblMaxEngine($bits));
    try {
        printf("%016x %.17g\n", $bits, $r->getFloat($m, $m, Random\IntervalBoundary::ClosedClosed));
    } catch (Throwable $t) {
        printf("%016x %s\n", $bits, get_class($t));
    }
}
/* Every other magnitude, including the positive twin, is unremarkable. */
foreach ([PHP_FLOAT_MAX, -1e300, -1e-300, 0.0, -0.0] as $x) {
    $r = new Random\Randomizer(new DblMaxEngine(-1));
    printf("%.17g -> %.17g\n", $x, $r->getFloat($x, $x, Random\IntervalBoundary::ClosedClosed));
}
?>
--EXPECT--
0000000000000000 -1.7976931348623157e+308
0000000000000001 -1.7976931348623157e+308
7fffffffffffffff -1.7976931348623157e+308
8000000000000000 -1.7976931348623157e+308
ffffffffffffffff -1.7976931348623157e+308
1.7976931348623157e+308 -> 1.7976931348623157e+308
-1.0000000000000001e+300 -> -1.0000000000000001e+300
-1.0e-300 -> -1.0e-300
0 -> 0
-0 -> -0
