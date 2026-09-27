--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
bcadd/bcsub/bcmul: php's number grammar, and the exact result TRUNCATED to the scale
--DESCRIPTION--
ext/bcmath reads a number with a grammar of its own -- `[+-]? DIGIT* ('.' DIGIT*)?`
and nothing else -- so "1e3" and " 1" are refused while "", "+", "-" and "." are
all valid and all mean zero. Every result is the EXACT one cut toward zero to the
scale and then padded, which is why bcmul('1.5','2.25',2) is '3.37' rather than a
rounded '3.38', and why a result that truncates to zero loses its sign.
--FILE--
<?php
function bcadd_grammar_probe(): void {
    echo "## the grammar: what parses, and to what\n";
    $inputs = ['', ' 1', '1 ', '+1', '-1', '.', '-', '+', '1.', '.5', '-.5', '-.',
               '1e3', '0x10', '007', '1.2.3', '--1', '1,5', 'abc', 'INF', '1_0', '.-'];
    foreach ($inputs as $s) {
        printf('%-8s ', var_export($s, true));
        try { echo var_export(bcadd($s, '0', 3), true), "\n"; }
        catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
    }
    echo "## a NUL ends the number and whatever follows it is ignored\n";
    var_dump(bcadd("1\0" . '2', '0', 3), bcadd("\0" . '1', '0', 3));

    echo "## the exact answer, cut toward zero and padded -- never rounded\n";
    foreach ([0, 1, 2, 5] as $sc) {
        foreach ([['1.5', '2.25'], ['-3.75', '1.5'], ['0.001', '100'], ['-0', '0']] as [$a, $b]) {
            printf("sc=%d %-6s %-5s add=%-12s sub=%-12s mul=%s\n", $sc, $a, $b,
                bcadd($a, $b, $sc), bcsub($a, $b, $sc), bcmul($a, $b, $sc));
        }
    }
    echo "## a result that truncates to zero drops the sign\n";
    var_dump(bcadd('-0.001', '0', 2), bcsub('0.001', '0.001', 5), bcmul('-1', '0', 3));

    echo "## the digits are unbounded, and the carries/borrows go all the way\n";
    var_dump(bcadd('12345678901234567890123456789012345678901234567890', '1'));
    var_dump(bcsub('1000000000000000000000', '1'));
    var_dump(bcmul('99999999999999999999', '99999999999999999999'));
    var_dump(bcmul('-99999999999999999999', '99999999999999999999'));
    var_dump(bcadd('0.00000000000000000001', '0.00000000000000000009', 25));

    echo "## the refusals\n";
    foreach ([['bcadd', ['x', '1']], ['bcadd', ['1', 'x']], ['bcsub', ['1', 'x']],
              ['bcmul', ['1', 'x']]] as [$fn, $args]) {
        try { $fn(...$args); } catch (Throwable $e) {
            printf("%-6s %s: %s\n", $fn, get_class($e), $e->getMessage());
        }
    }
    foreach ([-1, PHP_INT_MAX] as $bad) {
        try { bcadd('1', '2', $bad); } catch (Throwable $e) {
            echo get_class($e), ': ', $e->getMessage(), "\n";
        }
    }
    echo "## \$scale is screened FIRST, before either number is even read\n";
    foreach (['bcadd', 'bcsub', 'bcmul', 'bccomp'] as $fn) {
        try { $fn('x', 'y', -1); } catch (Throwable $e) {
            printf("%-7s %s: %s\n", $fn, get_class($e), $e->getMessage());
        }
    }
    try { bcadd('1'); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
    try { bcadd('1', '2', 3, 4); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
    try { bcadd([], '2'); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
    echo "## an int or a bool coerces to its string, which the grammar then reads\n";
    var_dump(bcadd(1, 2), bcadd(true, '2'));
}
bcadd_grammar_probe();
?>
--EXPECT--
## the grammar: what parses, and to what
''       '0.000'
' 1'     ValueError: bcadd(): Argument #1 ($num1) is not well-formed
'1 '     ValueError: bcadd(): Argument #1 ($num1) is not well-formed
'+1'     '1.000'
'-1'     '-1.000'
'.'      '0.000'
'-'      '0.000'
'+'      '0.000'
'1.'     '1.000'
'.5'     '0.500'
'-.5'    '-0.500'
'-.'     '0.000'
'1e3'    ValueError: bcadd(): Argument #1 ($num1) is not well-formed
'0x10'   ValueError: bcadd(): Argument #1 ($num1) is not well-formed
'007'    '7.000'
'1.2.3'  ValueError: bcadd(): Argument #1 ($num1) is not well-formed
'--1'    ValueError: bcadd(): Argument #1 ($num1) is not well-formed
'1,5'    ValueError: bcadd(): Argument #1 ($num1) is not well-formed
'abc'    ValueError: bcadd(): Argument #1 ($num1) is not well-formed
'INF'    ValueError: bcadd(): Argument #1 ($num1) is not well-formed
'1_0'    ValueError: bcadd(): Argument #1 ($num1) is not well-formed
'.-'     ValueError: bcadd(): Argument #1 ($num1) is not well-formed
## a NUL ends the number and whatever follows it is ignored
string(5) "1.000"
string(5) "0.000"
## the exact answer, cut toward zero and padded -- never rounded
sc=0 1.5    2.25  add=3            sub=0            mul=3
sc=0 -3.75  1.5   add=-2           sub=-5           mul=-5
sc=0 0.001  100   add=100          sub=-99          mul=0
sc=0 -0     0     add=0            sub=0            mul=0
sc=1 1.5    2.25  add=3.7          sub=-0.7         mul=3.3
sc=1 -3.75  1.5   add=-2.2         sub=-5.2         mul=-5.6
sc=1 0.001  100   add=100.0        sub=-99.9        mul=0.1
sc=1 -0     0     add=0.0          sub=0.0          mul=0.0
sc=2 1.5    2.25  add=3.75         sub=-0.75        mul=3.37
sc=2 -3.75  1.5   add=-2.25        sub=-5.25        mul=-5.62
sc=2 0.001  100   add=100.00       sub=-99.99       mul=0.10
sc=2 -0     0     add=0.00         sub=0.00         mul=0.00
sc=5 1.5    2.25  add=3.75000      sub=-0.75000     mul=3.37500
sc=5 -3.75  1.5   add=-2.25000     sub=-5.25000     mul=-5.62500
sc=5 0.001  100   add=100.00100    sub=-99.99900    mul=0.10000
sc=5 -0     0     add=0.00000      sub=0.00000      mul=0.00000
## a result that truncates to zero drops the sign
string(4) "0.00"
string(7) "0.00000"
string(5) "0.000"
## the digits are unbounded, and the carries/borrows go all the way
string(50) "12345678901234567890123456789012345678901234567891"
string(21) "999999999999999999999"
string(40) "9999999999999999999800000000000000000001"
string(41) "-9999999999999999999800000000000000000001"
string(27) "0.0000000000000000001000000"
## the refusals
bcadd  ValueError: bcadd(): Argument #1 ($num1) is not well-formed
bcadd  ValueError: bcadd(): Argument #2 ($num2) is not well-formed
bcsub  ValueError: bcsub(): Argument #2 ($num2) is not well-formed
bcmul  ValueError: bcmul(): Argument #2 ($num2) is not well-formed
ValueError: bcadd(): Argument #3 ($scale) must be between 0 and 2147483647
ValueError: bcadd(): Argument #3 ($scale) must be between 0 and 2147483647
## $scale is screened FIRST, before either number is even read
bcadd   ValueError: bcadd(): Argument #3 ($scale) must be between 0 and 2147483647
bcsub   ValueError: bcsub(): Argument #3 ($scale) must be between 0 and 2147483647
bcmul   ValueError: bcmul(): Argument #3 ($scale) must be between 0 and 2147483647
bccomp  ValueError: bccomp(): Argument #3 ($scale) must be between 0 and 2147483647
ArgumentCountError: bcadd() expects at least 2 arguments, 1 given
ArgumentCountError: bcadd() expects at most 3 arguments, 4 given
TypeError: bcadd(): Argument #1 ($num1) must be of type string, array given
## an int or a bool coerces to its string, which the grammar then reads
string(1) "3"
string(1) "3"
