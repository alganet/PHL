--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
bcdiv/bcmod/bcdivmod: truncated division, so the remainder takes the DIVIDEND's sign
--DESCRIPTION--
bcdiv() cuts toward zero at the scale rather than rounding, and bcmod()/bcdivmod()
are built on a quotient truncated to an INTEGER whatever $scale says -- which is
what gives the remainder the sign of the dividend and not of the divisor, so
-10 % 3 is -1 and 10 % -3 is 1. A zero divisor is a DivisionByZeroError worded
from the operation: bcdivmod() says "Division by zero" where bcmod() says
"Modulo by zero".
--FILE--
<?php
function bcdiv_truncation_probe(): void {
    echo "## the quotient is CUT at the scale, never rounded\n";
    foreach ([['1', '3', 5], ['2', '3', 5], ['-1', '3', 5], ['1', '-3', 5],
              ['-1', '-3', 5], ['-7', '2', 5], ['1', '3', 0], ['2', '3', 0],
              ['10', '2', 3], ['1', '32768', 20], ['0', '5', 2]] as [$a, $b, $s]) {
        printf("%-6s / %-8s sc=%-2d %s\n", $a, $b, $s, bcdiv($a, $b, $s));
    }
    echo "## the point moves with BOTH operands' scales\n";
    var_dump(bcdiv('1', '0.5', 3), bcdiv('2', '0.25', 0), bcdiv('0.001', '0.0001', 4));
    var_dump(bcdiv('123456789012345678901234567890', '987654321', 10));

    echo "## the remainder's sign is the DIVIDEND's, in all four combinations\n";
    foreach ([['10', '3'], ['-10', '3'], ['10', '-3'], ['-10', '-3']] as [$a, $b]) {
        printf("%-4s %% %-4s = %-4s divmod = [%s]\n", $a, $b,
            bcmod($a, $b), implode(', ', bcdivmod($a, $b)));
    }
    echo "## the quotient stays an INTEGER whatever \$scale says\n";
    foreach ([['10.5', '3', 2], ['1', '0.7', 3], ['1', '3', 5], ['10', '0.0003', 6],
              ['-0.5', '1', 3], ['100', '7', 3]] as [$a, $b, $s]) {
        printf("%-6s %% %-8s sc=%d -> %-12s divmod = [%s]\n", $a, $b, $s,
            bcmod($a, $b, $s), implode(', ', bcdivmod($a, $b, $s)));
    }
    echo "## a zero divisor, worded from the operation\n";
    foreach ([['bcdiv', '0'], ['bcmod', '0'], ['bcdivmod', '0'],
              ['bcmod', '0.0'], ['bcdiv', '-0'], ['bcdiv', '.']] as [$fn, $b]) {
        try { $fn('1', $b); } catch (Throwable $e) {
            printf("%-9s by %-4s %s: %s\n", $fn, $b, get_class($e), $e->getMessage());
        }
    }
    echo "## and \$scale is screened before the divisor is even looked at\n";
    foreach (['bcdiv', 'bcmod', 'bcdivmod'] as $fn) {
        try { $fn('1', '0', -1); } catch (Throwable $e) {
            printf("%-9s %s: %s\n", $fn, get_class($e), $e->getMessage());
        }
    }
}
bcdiv_truncation_probe();
?>
--EXPECT--
## the quotient is CUT at the scale, never rounded
1      / 3        sc=5  0.33333
2      / 3        sc=5  0.66666
-1     / 3        sc=5  -0.33333
1      / -3       sc=5  -0.33333
-1     / -3       sc=5  0.33333
-7     / 2        sc=5  -3.50000
1      / 3        sc=0  0
2      / 3        sc=0  0
10     / 2        sc=3  5.000
1      / 32768    sc=20 0.00003051757812500000
0      / 5        sc=2  0.00
## the point moves with BOTH operands' scales
string(5) "2.000"
string(1) "8"
string(7) "10.0000"
string(32) "124999998873437499901.5820312398"
## the remainder's sign is the DIVIDEND's, in all four combinations
10   % 3    = 1    divmod = [3, 1]
-10  % 3    = -1   divmod = [-3, -1]
10   % -3   = 1    divmod = [-3, 1]
-10  % -3   = -1   divmod = [3, -1]
## the quotient stays an INTEGER whatever $scale says
10.5   % 3        sc=2 -> 1.50         divmod = [3, 1.50]
1      % 0.7      sc=3 -> 0.300        divmod = [1, 0.300]
1      % 3        sc=5 -> 1.00000      divmod = [0, 1.00000]
10     % 0.0003   sc=6 -> 0.000100     divmod = [33333, 0.000100]
-0.5   % 1        sc=3 -> -0.500       divmod = [0, -0.500]
100    % 7        sc=3 -> 2.000        divmod = [14, 2.000]
## a zero divisor, worded from the operation
bcdiv     by 0    DivisionByZeroError: Division by zero
bcmod     by 0    DivisionByZeroError: Modulo by zero
bcdivmod  by 0    DivisionByZeroError: Division by zero
bcmod     by 0.0  DivisionByZeroError: Modulo by zero
bcdiv     by -0   DivisionByZeroError: Division by zero
bcdiv     by .    DivisionByZeroError: Division by zero
## and $scale is screened before the divisor is even looked at
bcdiv     ValueError: bcdiv(): Argument #3 ($scale) must be between 0 and 2147483647
bcmod     ValueError: bcmod(): Argument #3 ($scale) must be between 0 and 2147483647
bcdivmod  ValueError: bcdivmod(): Argument #3 ($scale) must be between 0 and 2147483647
