--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
bcround()'s eight RoundingMode rules, and bcfloor()/bcceil() as two of them
--DESCRIPTION--
bcround() is the one bc* function that does NOT truncate: it decides from the
dropped digits alone. The four HALF_* rules ask whether the tail is above, below
or exactly half, and only the exact half tells them apart; the other four never
look at the tail's size, only at whether it is empty and which way the sign
points. bcfloor() and bcceil() ARE bcround at precision 0 under the two infinity
modes. The precision may be NEGATIVE, and then the answer is the kept part with
that many zeros put back -- so AwayFromZero grows the number as the precision
falls.
--FILE--
<?php
function bcround_modes_probe(): void {
    echo "## bcfloor / bcceil\n";
    foreach (['1.5', '-1.5', '1.0', '-0.5', '0.5', '-0', '2', '-2.000', '.', '-.9',
              '1.9999999999999999999', '1.0000000000000000001',
              '123456789012345678901234567890.5'] as $v) {
        printf("%-34s floor=%-32s ceil=%s\n", $v, bcfloor($v), bcceil($v));
    }
    echo "## the default mode, HalfAwayFromZero\n";
    foreach (['1.5', '-1.5', '2.5', '-2.5', '9.99', '99.5', '-99.5', '-0.0001',
              '-0.00001', '1.2345'] as $v) {
        printf("%-10s %s\n", $v, bcround($v));
    }
    echo "## the precision, positive and negative\n";
    foreach ([['1.2345', 2], ['1.2345', -2], ['1234.5', -2], ['1.5', 5], ['0.0', 3],
              ['1234.5678', -2], ['1234.5678', 1], ['1234.5678', 3], ['0', -5],
              ['0.0001', -5], ['6', -1], ['4', -1], ['1', -3]] as [$v, $p]) {
        printf("%-12s %-4d %s\n", $v, $p, bcround($v, $p));
    }
    echo "## every mode over the cases that tell them apart\n";
    $cases = [['0.5', 0], ['-0.5', 0], ['1.5', 0], ['2.5', 0], ['-2.5', 0],
              ['1.45', 1], ['1.55', 1], ['15', -1], ['25', -1], ['1.1', 0],
              ['-1.1', 0], ['1', -3], ['-1', -3], ['0.5', -1], ['1.4999', 0]];
    foreach (RoundingMode::cases() as $mode) {
        $out = [];
        foreach ($cases as [$v, $p]) {
            $out[] = "$v/$p=" . bcround($v, $p, $mode);
        }
        printf("%-18s %s\n", $mode->name, implode(' ', $out));
    }
    echo "## the refusals\n";
    foreach ([['bcround', ['x']], ['bcfloor', ['x']], ['bcceil', ['x']]] as [$fn, $args]) {
        try { $fn(...$args); } catch (Throwable $e) {
            printf("%-8s %s: %s\n", $fn, get_class($e), $e->getMessage());
        }
    }
    foreach ([PHP_INT_MAX, 2147483648] as $bad) {
        try { bcround('1', $bad); } catch (Throwable $e) {
            echo get_class($e), ': ', $e->getMessage(), "\n";
        }
    }
    /* PHP_INT_MIN is IN range: it drops every digit and answers zero. */
    var_dump(bcround('1', PHP_INT_MIN), bcround('1.5', PHP_INT_MIN));
    foreach ([1, null, 'HalfEven'] as $bad) {
        try { bcround('1.5', 0, $bad); } catch (Throwable $e) {
            printf("%-10s %s: %s\n", var_export($bad, true), get_class($e), $e->getMessage());
        }
    }
    try { bcround('1.5', 2, RoundingMode::HalfEven, 4); } catch (Throwable $e) {
        echo get_class($e), ': ', $e->getMessage(), "\n";
    }
    try { bcfloor('1.5', 2); } catch (Throwable $e) {
        echo get_class($e), ': ', $e->getMessage(), "\n";
    }
    try { bcround(); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
    try { bcfloor(); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
    echo "## a negative precision grows the number under the non-half rules\n";
    var_dump(bcround('1', -20, RoundingMode::AwayFromZero),
             bcround('0', -100, RoundingMode::AwayFromZero),
             bcround('0.5', -1, RoundingMode::AwayFromZero),
             bcround('1', -3, RoundingMode::PositiveInfinity),
             bcround('-1', -3, RoundingMode::NegativeInfinity));
}
bcround_modes_probe();
?>
--EXPECT--
## bcfloor / bcceil
1.5                                floor=1                                ceil=2
-1.5                               floor=-2                               ceil=-1
1.0                                floor=1                                ceil=1
-0.5                               floor=-1                               ceil=0
0.5                                floor=0                                ceil=1
-0                                 floor=0                                ceil=0
2                                  floor=2                                ceil=2
-2.000                             floor=-2                               ceil=-2
.                                  floor=0                                ceil=0
-.9                                floor=-1                               ceil=0
1.9999999999999999999              floor=1                                ceil=2
1.0000000000000000001              floor=1                                ceil=2
123456789012345678901234567890.5   floor=123456789012345678901234567890   ceil=123456789012345678901234567891
## the default mode, HalfAwayFromZero
1.5        2
-1.5       -2
2.5        3
-2.5       -3
9.99       10
99.5       100
-99.5      -100
-0.0001    0
-0.00001   0
1.2345     1
## the precision, positive and negative
1.2345       2    1.23
1.2345       -2   0
1234.5       -2   1200
1.5          5    1.50000
0.0          3    0.000
1234.5678    -2   1200
1234.5678    1    1234.6
1234.5678    3    1234.568
0            -5   0
0.0001       -5   0
6            -1   10
4            -1   0
1            -3   0
## every mode over the cases that tell them apart
HalfAwayFromZero   0.5/0=1 -0.5/0=-1 1.5/0=2 2.5/0=3 -2.5/0=-3 1.45/1=1.5 1.55/1=1.6 15/-1=20 25/-1=30 1.1/0=1 -1.1/0=-1 1/-3=0 -1/-3=0 0.5/-1=0 1.4999/0=1
HalfTowardsZero    0.5/0=0 -0.5/0=0 1.5/0=1 2.5/0=2 -2.5/0=-2 1.45/1=1.4 1.55/1=1.5 15/-1=10 25/-1=20 1.1/0=1 -1.1/0=-1 1/-3=0 -1/-3=0 0.5/-1=0 1.4999/0=1
HalfEven           0.5/0=0 -0.5/0=0 1.5/0=2 2.5/0=2 -2.5/0=-2 1.45/1=1.4 1.55/1=1.6 15/-1=20 25/-1=20 1.1/0=1 -1.1/0=-1 1/-3=0 -1/-3=0 0.5/-1=0 1.4999/0=1
HalfOdd            0.5/0=1 -0.5/0=-1 1.5/0=1 2.5/0=3 -2.5/0=-3 1.45/1=1.5 1.55/1=1.5 15/-1=10 25/-1=30 1.1/0=1 -1.1/0=-1 1/-3=0 -1/-3=0 0.5/-1=0 1.4999/0=1
TowardsZero        0.5/0=0 -0.5/0=0 1.5/0=1 2.5/0=2 -2.5/0=-2 1.45/1=1.4 1.55/1=1.5 15/-1=10 25/-1=20 1.1/0=1 -1.1/0=-1 1/-3=0 -1/-3=0 0.5/-1=0 1.4999/0=1
AwayFromZero       0.5/0=1 -0.5/0=-1 1.5/0=2 2.5/0=3 -2.5/0=-3 1.45/1=1.5 1.55/1=1.6 15/-1=20 25/-1=30 1.1/0=2 -1.1/0=-2 1/-3=1000 -1/-3=-1000 0.5/-1=10 1.4999/0=2
NegativeInfinity   0.5/0=0 -0.5/0=-1 1.5/0=1 2.5/0=2 -2.5/0=-3 1.45/1=1.4 1.55/1=1.5 15/-1=10 25/-1=20 1.1/0=1 -1.1/0=-2 1/-3=0 -1/-3=-1000 0.5/-1=0 1.4999/0=1
PositiveInfinity   0.5/0=1 -0.5/0=0 1.5/0=2 2.5/0=3 -2.5/0=-2 1.45/1=1.5 1.55/1=1.6 15/-1=20 25/-1=30 1.1/0=2 -1.1/0=-1 1/-3=1000 -1/-3=0 0.5/-1=10 1.4999/0=2
## the refusals
bcround  ValueError: bcround(): Argument #1 ($num) is not well-formed
bcfloor  ValueError: bcfloor(): Argument #1 ($num) is not well-formed
bcceil   ValueError: bcceil(): Argument #1 ($num) is not well-formed
ValueError: bcround(): Argument #2 ($precision) must be between -9223372036854775808 and 2147483647
ValueError: bcround(): Argument #2 ($precision) must be between -9223372036854775808 and 2147483647
string(1) "0"
string(1) "0"
1          TypeError: bcround(): Argument #3 ($mode) must be of type RoundingMode, int given
NULL       TypeError: bcround(): Argument #3 ($mode) must be of type RoundingMode, null given
'HalfEven' TypeError: bcround(): Argument #3 ($mode) must be of type RoundingMode, string given
ArgumentCountError: bcround() expects at most 3 arguments, 4 given
ArgumentCountError: bcfloor() expects exactly 1 argument, 2 given
ArgumentCountError: bcround() expects at least 1 argument, 0 given
ArgumentCountError: bcfloor() expects exactly 1 argument, 0 given
## a negative precision grows the number under the non-half rules
string(21) "100000000000000000000"
string(1) "0"
string(2) "10"
string(4) "1000"
string(5) "-1000"
