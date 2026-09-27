--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
bcpow()/bcpowmod(): the EXACT power, then the cut -- and two different exponent limits
--DESCRIPTION--
bcpow() raises exactly and truncates once at the end, which is visible:
bcpow('1.5','10',2) is '57.66', the exact 57.6650390625 cut, where a chain of
squarings each truncated to two places would answer '57.60'. A negative exponent
is the reciprocal of that exact power, divided at the scale. bcpowmod() reduces
at every step instead, so it takes an exponent of ANY width where bcpow() refuses
one past a machine integer -- and its remainder is the truncated-division one, so
a negative base with an odd exponent comes back negative.
--FILE--
<?php
function bcpow_exact_probe(): void {
    echo "## exact, THEN cut -- not a chain of truncated squarings\n";
    foreach ([['1.5', '10', 2], ['1.5', '10', 20], ['1.1', '20', 3], ['0.5', '10', 3],
              ['2.5', '5', 2], ['2.5', '5', 0], ['1.5', '3', 4], ['0.1', '3', 3],
              ['1.0000000001', '3', 40]] as [$a, $e, $s]) {
        printf("%-14s ** %-3s sc=%-2d %s\n", $a, $e, $s, bcpow($a, $e, $s));
    }
    echo "## integers, signs and the zero exponent\n";
    var_dump(bcpow('2', '10'), bcpow('2', '64'), bcpow('-2', '3'), bcpow('-1.5', '3', 1));
    var_dump(bcpow('2', '0'), bcpow('0', '0'), bcpow('1', '1.0'));
    echo "## a negative exponent is 1 divided by the exact power\n";
    var_dump(bcpow('2', '-3', 5), bcpow('2', '-1', 1), bcpow('1.5', '-10', 5),
             bcpow('1.5', '-10', 20));
    echo "## the refusals\n";
    foreach ([['0', '-1'], ['2', '2.9'], ['2', '-2.9'],
              ['1', '99999999999999999999999'], ['1', '-99999999999999999999999'],
              ['x', '1.5']] as [$a, $e]) {
        try { bcpow($a, $e); } catch (Throwable $ex) {
            printf("%-4s ** %-24s %s: %s\n", $a, $e, get_class($ex), $ex->getMessage());
        }
    }
    try { bcpow('2', '1.5', -1); } catch (Throwable $ex) {
        echo get_class($ex), ': ', $ex->getMessage(), "\n";
    }

    echo "## bcpowmod: the modulus's sign is ignored, the BASE's is kept\n";
    foreach ([['4', '3', '5'], ['3', '100', '7'], ['0', '5', '7'], ['5', '0', '1'],
              ['-5', '3', '7'], ['-4', '3', '5'], ['-4', '2', '5'], ['4', '3', '-5'],
              ['0', '0', '5'], ['123456789', '987654321', '1000000007']] as [$a, $e, $m]) {
        printf("(%s ** %s) %% %-12s = %s\n", $a, $e, $m, bcpowmod($a, $e, $m));
    }
    echo "## and an exponent of ANY width, where bcpow() refuses one\n";
    var_dump(bcpowmod('3', '99999999999999999999999', '7'),
             bcpowmod('3', '9223372036854775807', '7'),
             bcpowmod('4', '3', '5', 3));
    echo "## its refusals, in php's order: #1's fraction, #2's, #2's SIGN, then #3's\n";
    foreach ([['4.5', '3', '5'], ['4', '3.5', '5'], ['4', '-3', '5'], ['4', '3', '5.5'],
              ['4', '3', '0'], ['1.5', '-2.5', '0.5'], ['1', '-2.5', '5'],
              ['1', '-2', '0'], ['1', '-2', '0.5'], ['1', '2', '0.0']] as [$a, $e, $m]) {
        try { bcpowmod($a, $e, $m); } catch (Throwable $ex) {
            printf("%-5s %-6s %-5s %s: %s\n", $a, $e, $m, get_class($ex), $ex->getMessage());
        }
    }
    try { bcpowmod('1.5', '2', '3', -1); } catch (Throwable $ex) {
        echo get_class($ex), ': ', $ex->getMessage(), "\n";
    }
}
bcpow_exact_probe();
?>
--EXPECT--
## exact, THEN cut -- not a chain of truncated squarings
1.5            ** 10  sc=2  57.66
1.5            ** 10  sc=20 57.66503906250000000000
1.1            ** 20  sc=3  6.727
0.5            ** 10  sc=3  0.000
2.5            ** 5   sc=2  97.65
2.5            ** 5   sc=0  97
1.5            ** 3   sc=4  3.3750
0.1            ** 3   sc=3  0.001
1.0000000001   ** 3   sc=40 1.0000000003000000000300000000010000000000
## integers, signs and the zero exponent
string(4) "1024"
string(20) "18446744073709551616"
string(2) "-8"
string(4) "-3.3"
string(1) "1"
string(1) "1"
string(1) "1"
## a negative exponent is 1 divided by the exact power
string(7) "0.12500"
string(3) "0.5"
string(7) "0.01734"
string(22) "0.01734152991583261359"
## the refusals
0    ** -1                       DivisionByZeroError: Negative power of zero
2    ** 2.9                      ValueError: bcpow(): Argument #2 ($exponent) cannot have a fractional part
2    ** -2.9                     ValueError: bcpow(): Argument #2 ($exponent) cannot have a fractional part
1    ** 99999999999999999999999  ValueError: bcpow(): Argument #2 ($exponent) is too large
1    ** -99999999999999999999999 ValueError: bcpow(): Argument #2 ($exponent) is too large
x    ** 1.5                      ValueError: bcpow(): Argument #1 ($num) is not well-formed
ValueError: bcpow(): Argument #3 ($scale) must be between 0 and 2147483647
## bcpowmod: the modulus's sign is ignored, the BASE's is kept
(4 ** 3) % 5            = 4
(3 ** 100) % 7            = 4
(0 ** 5) % 7            = 0
(5 ** 0) % 1            = 0
(-5 ** 3) % 7            = -6
(-4 ** 3) % 5            = -4
(-4 ** 2) % 5            = 1
(4 ** 3) % -5           = 4
(0 ** 0) % 5            = 1
(123456789 ** 987654321) % 1000000007   = 652541198
## and an exponent of ANY width, where bcpow() refuses one
string(1) "6"
string(1) "3"
string(5) "4.000"
## its refusals, in php's order: #1's fraction, #2's, #2's SIGN, then #3's
4.5   3      5     ValueError: bcpowmod(): Argument #1 ($num) cannot have a fractional part
4     3.5    5     ValueError: bcpowmod(): Argument #2 ($exponent) cannot have a fractional part
4     -3     5     ValueError: bcpowmod(): Argument #2 ($exponent) must be greater than or equal to 0
4     3      5.5   ValueError: bcpowmod(): Argument #3 ($modulus) cannot have a fractional part
4     3      0     DivisionByZeroError: Modulo by zero
1.5   -2.5   0.5   ValueError: bcpowmod(): Argument #1 ($num) cannot have a fractional part
1     -2.5   5     ValueError: bcpowmod(): Argument #2 ($exponent) cannot have a fractional part
1     -2     0     ValueError: bcpowmod(): Argument #2 ($exponent) must be greater than or equal to 0
1     -2     0.5   ValueError: bcpowmod(): Argument #2 ($exponent) must be greater than or equal to 0
1     2      0.0   DivisionByZeroError: Modulo by zero
ValueError: bcpowmod(): Argument #4 ($scale) must be between 0 and 2147483647
