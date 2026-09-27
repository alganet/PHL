--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
bcsqrt(): the truncated root, and the radicand's OWN scale is not the answer's
--DESCRIPTION--
bcsqrt() answers floor(sqrt(num) * 10^scale) / 10^scale -- a truncation, never a
rounding, so sqrt(2) at ten places ends 623 and not 624. The scale is the
argument's alone: a radicand carrying more places than the scale asks for is cut
first, which is why sqrt('0.25') at scale 0 is '0' and not '0.5'.
--FILE--
<?php
function bcsqrt_scale_probe(): void {
    $saved = bcscale();
    try {
        foreach ([['2', 10], ['2', 50], ['0', 0], ['9', 0], ['9', 3], ['0.25', 3],
                  ['0.25', 0], ['152.2756', 2], ['2.00', 0], ['2.00', 1], ['0.99', 1],
                  ['4', 5], ['1000000', 0], ['0.00000000000000000001', 15],
                  ['123456789012345678901234567890', 10]] as [$n, $s]) {
            printf("sqrt(%-30s) sc=%-2d %s\n", $n, $s, bcsqrt($n, $s));
        }
        echo "## it is a TRUNCATION: the square of the answer never exceeds the input\n";
        foreach (['2', '3', '5', '7', '10', '99', '0.5'] as $n) {
            $r = bcsqrt($n, 12);
            printf("%-5s %-16s r*r<=n:%s (r+1e-12)*(r+1e-12)>n:%s\n", $n, $r,
                var_export(bccomp(bcmul($r, $r, 24), $n, 24) <= 0, true),
                var_export(bccomp(bcmul(bcadd($r, '0.000000000001', 12),
                                        bcadd($r, '0.000000000001', 12), 24), $n, 24) > 0, true));
        }
        echo "## the directive supplies the scale when the argument does not\n";
        bcscale(7);
        var_dump(bcsqrt('2'), bcsqrt('9'));
        echo "## the refusals\n";
        foreach ([['-1', null], ['x', null]] as [$n, $s]) {
            try { bcsqrt($n); } catch (Throwable $e) {
                printf("%-4s %s: %s\n", $n, get_class($e), $e->getMessage());
            }
        }
        var_dump(bcsqrt('-0', 3));
        try { bcsqrt('-1', -1); } catch (Throwable $e) {
            echo get_class($e), ': ', $e->getMessage(), "\n";
        }
    } finally {
        bcscale($saved);
    }
}
bcsqrt_scale_probe();
?>
--EXPECT--
sqrt(2                             ) sc=10 1.4142135623
sqrt(2                             ) sc=50 1.41421356237309504880168872420969807856967187537694
sqrt(0                             ) sc=0  0
sqrt(9                             ) sc=0  3
sqrt(9                             ) sc=3  3.000
sqrt(0.25                          ) sc=3  0.500
sqrt(0.25                          ) sc=0  0
sqrt(152.2756                      ) sc=2  12.34
sqrt(2.00                          ) sc=0  1
sqrt(2.00                          ) sc=1  1.4
sqrt(0.99                          ) sc=1  0.9
sqrt(4                             ) sc=5  2.00000
sqrt(1000000                       ) sc=0  1000
sqrt(0.00000000000000000001        ) sc=15 0.000000000100000
sqrt(123456789012345678901234567890) sc=10 351364182882014.4253111222
## it is a TRUNCATION: the square of the answer never exceeds the input
2     1.414213562373   r*r<=n:true (r+1e-12)*(r+1e-12)>n:true
3     1.732050807568   r*r<=n:true (r+1e-12)*(r+1e-12)>n:true
5     2.236067977499   r*r<=n:true (r+1e-12)*(r+1e-12)>n:true
7     2.645751311064   r*r<=n:true (r+1e-12)*(r+1e-12)>n:true
10    3.162277660168   r*r<=n:true (r+1e-12)*(r+1e-12)>n:true
99    9.949874371066   r*r<=n:true (r+1e-12)*(r+1e-12)>n:true
0.5   0.707106781186   r*r<=n:true (r+1e-12)*(r+1e-12)>n:true
## the directive supplies the scale when the argument does not
string(9) "1.4142135"
string(9) "3.0000000"
## the refusals
-1   ValueError: bcsqrt(): Argument #1 ($num) must be greater than or equal to 0
x    ValueError: bcsqrt(): Argument #1 ($num) is not well-formed
string(5) "0.000"
ValueError: bcsqrt(): Argument #2 ($scale) must be between 0 and 2147483647
