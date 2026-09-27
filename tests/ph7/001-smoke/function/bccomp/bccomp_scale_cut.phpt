--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
bccomp()'s $scale is a CUT applied to both operands, not a tolerance
--DESCRIPTION--
bccomp() truncates both numbers to the scale FIRST and compares what is left, so
at the default scale of 0 it answers 0 for 1.1 against 1.2 -- both are 1. Trailing
zeros never matter ("1.10" equals "1.1"), and neither does the sign of zero.
--FILE--
<?php
function bccomp_scale_cut_probe(): void {
    $saved = bcscale();
    try {
        echo "## the default scale of 0 compares INTEGER parts\n";
        var_dump(bccomp('1.1', '1.2'), bccomp('1.1', '1.2', 1), bccomp('1.1', '1.2', 0));
        var_dump(bccomp('1.0001', '1.0002', 3), bccomp('1.0001', '1.0002', 4));
        echo "## trailing zeros and the sign of zero are not part of the VALUE\n";
        var_dump(bccomp('1.10', '1.1', 5), bccomp('0', '-0', 5), bccomp('1', '1.0000', 4));
        var_dump(bccomp('-0.0', '0.000', 9));
        echo "## ordering, both signs\n";
        foreach ([['-1', '1'], ['-1.5', '-1.4'], ['-1.4', '-1.5'], ['2', '2'],
                  ['0.1', '0'], ['-0.1', '0']] as [$a, $b]) {
            printf("%-6s %-6s %d\n", $a, $b, bccomp($a, $b, 3));
        }
        echo "## it reads the directive too\n";
        bcscale(2);
        var_dump(bccomp('1.111', '1.112'), bccomp('1.111', '1.112', 3));
        echo "## the refusals\n";
        try { bccomp('x', '1'); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
        try { bccomp('1', 'x'); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
        try { bccomp('1', '2', -1); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
    } finally {
        bcscale($saved);
    }
}
bccomp_scale_cut_probe();
?>
--EXPECT--
## the default scale of 0 compares INTEGER parts
int(0)
int(-1)
int(0)
int(0)
int(-1)
## trailing zeros and the sign of zero are not part of the VALUE
int(0)
int(0)
int(0)
int(0)
## ordering, both signs
-1     1      -1
-1.5   -1.4   -1
-1.4   -1.5   1
2      2      0
0.1    0      1
-0.1   0      -1
## it reads the directive too
int(0)
int(-1)
## the refusals
ValueError: bccomp(): Argument #1 ($num1) is not well-formed
ValueError: bccomp(): Argument #2 ($num2) is not well-formed
ValueError: bccomp(): Argument #3 ($scale) must be between 0 and 2147483647
