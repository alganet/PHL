--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
getFloat() draws uniformly over an interval's REPRESENTABLE doubles
--DESCRIPTION--
`$min + $span * nextFloat()` is not uniform over the doubles of an interval --
the ones near zero are packed thousands of times more densely than the ones at
the far end, and that scaling can never produce most of them. php 8.3 answers
with Goualard's gamma-section, and every part of it is observable. The grid is
the multiples of one gamma; the gamma is the spacing just BELOW the endpoint of
LARGER magnitude, so every step lands on a double that exists -- which is why
[0,1) steps by 2^-53 and [1,2) by 2^-52, half as fine, even though both spans
are 1.0. That same endpoint is where the counting starts, so the answers walk
DOWN from $max when $max is the larger and UP from $min when $min is. And
Random\IntervalBoundary says which ends are reachable, which changes the number
of steps and therefore the whole sequence.
--FILE--
<?php
class GetFloatEngine implements Random\Engine
{
    public function __construct(private int $value) {}
    public function generate(): string { return pack('P', $this->value); }
}
function gf(int $bits, float $min, float $max, Random\IntervalBoundary $b): string {
    $r = new Random\Randomizer(new GetFloatEngine($bits));
    try { return sprintf('%.17g', $r->getFloat($min, $max, $b)); }
    catch (Throwable $t) { return get_class($t) . ': ' . $t->getMessage(); }
}
$CO = Random\IntervalBoundary::ClosedOpen;
$CC = Random\IntervalBoundary::ClosedClosed;
$OC = Random\IntervalBoundary::OpenClosed;
$OO = Random\IntervalBoundary::OpenOpen;

/* The grid, walked from the top of [0,1). */
foreach ([0, 1, 2, 1 << 52, (1 << 53) - 1, 1 << 53] as $bits) {
    printf("%016x  %s %s %s %s\n", $bits,
        gf($bits, 0.0, 1.0, $CO), gf($bits, 0.0, 1.0, $CC),
        gf($bits, 0.0, 1.0, $OC), gf($bits, 0.0, 1.0, $OO));
}

/* The gamma comes from the larger endpoint, so the same span steps differently. */
foreach ([[0.0, 1.0], [1.0, 2.0], [0.0, 100.0], [3.0, 4.0], [0.0, 0.5]] as [$a, $b]) {
    printf("[%.17g,%.17g] step=%.17g\n", $a, $b,
        abs((float)gf(1, $a, $b, $CO) - (float)gf(0, $a, $b, $CO)));
}

/* The larger magnitude also decides which END the walk starts from. */
printf("%s %s\n", gf(0, 0.0, 1.0, $CO), gf(0, -1.0, 0.0, $CO));
printf("%s %s\n", gf(0, -5.5, 3.25, $CO), gf(0, -3.0, 5.5, $CO));

/* The far endpoint is reachable because the overshoot is clamped back. */
$g = 2 ** -53;
$count = (int)(1.0 / $g) - (int)(0.1 / $g);
printf("%s %s\n", gf($count - 1, 0.1, 1.0, $CO), gf($count - 2, 0.1, 1.0, $CO));

/* Screens, in php's order. */
$r = new Random\Randomizer(new Random\Engine\Mt19937(1));
foreach ([[NAN, 1.0, $CO], [0.0, INF, $CO], [1.0, 0.0, $CO], [5.0, 5.0, $CO],
          [5.0, 5.0, $CC], [1.0, 1.0 + PHP_FLOAT_EPSILON, $OO]] as [$a, $b, $bd]) {
    try { var_dump($r->getFloat($a, $b, $bd)); }
    catch (Throwable $t) { echo get_class($t), ': ', $t->getMessage(), "\n"; }
}
?>
--EXPECT--
0000000000000000  0.99999999999999989 1 1 0.99999999999999989
0000000000000001  0.99999999999999978 0.99999999999999989 0.99999999999999989 0.99999999999999978
0000000000000002  0.99999999999999967 0.99999999999999978 0.99999999999999978 0.99999999999999967
0010000000000000  0.49999999999999989 0.5 0.5 0.49999999999999989
001fffffffffffff  0 1.1102230246251565e-16 1.1102230246251565e-16 0.99999999999999989
0020000000000000  0.99999999999999989 0 1 0.99999999999999978
[0,1] step=1.1102230246251565e-16
[1,2] step=2.2204460492503131e-16
[0,100] step=1.4210854715202004e-14
[3,4] step=4.4408920985006262e-16
[0,0.5] step=5.5511151231257827e-17
0.99999999999999989 -1
-5.5 5.4999999999999991
0.10000000000000001 0.10000000000000009
ValueError: Random\Randomizer::getFloat(): Argument #1 ($min) must be finite
ValueError: Random\Randomizer::getFloat(): Argument #2 ($max) must be finite
ValueError: Random\Randomizer::getFloat(): Argument #2 ($max) must be greater than argument #1 ($min)
ValueError: Random\Randomizer::getFloat(): Argument #2 ($max) must be greater than argument #1 ($min)
float(5)
ValueError: The given interval is empty, there are no floats between argument #1 ($min) and argument #2 ($max)
