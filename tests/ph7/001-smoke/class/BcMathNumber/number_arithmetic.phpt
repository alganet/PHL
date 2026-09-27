--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
BcMath\Number: the scale each method computes, and the operators the class answers
--DESCRIPTION--
The object face of bcmath differs from the functions in two ways that run
through everything. It carries its OWN scale and never reads `bcmath.scale`: a
method with no $scale computes one -- max for add/sub/mod, the SUM for mul, 0
for powmod, and for the three that do not terminate (div, a negative pow, sqrt)
ten places past the RECEIVER's scale with the trailing zeros trimmed back to it,
which is why sqrt('0.25') is '0.50' and sqrt('9') is '3'. And it answers php's
operator handler, so `+ - * / % **`, unary minus and `++`/`--` all work, with a
string operand read by the bc grammar and refused by SIDE when it is not one.
--FILE--
<?php
use BcMath\Number;
function bcnumber_arith_probe(): void {
    $saved = bcscale();
    try {
        bcscale(7);   /* the directive must not reach a single answer below */
        echo "## the constructor keeps the written scale, and normalizes the rest\n";
        foreach (['1.500', '1.5', '5', '-0.0', '007', '', '.', '+1.5', '-0'] as $s) {
            $n = new Number($s);
            printf("%-8s -> %-8s scale=%d\n", var_export($s, true), (string)$n, $n->scale);
        }
        var_dump((string)new Number(5), (string)new Number(PHP_INT_MIN));

        echo "## add/sub/mul: max, max, SUM\n";
        foreach ([['1.5', '2.25'], ['1.50', '2.2'], ['1', '2'], ['1.0', '2']] as [$x, $y]) {
            $a = new Number($x); $b = new Number($y);
            printf("%-6s %-6s add=%s(%d) sub=%s(%d) mul=%s(%d)\n", $x, $y,
                $a->add($b), $a->add($b)->scale, $a->sub($b), $a->sub($b)->scale,
                $a->mul($b), $a->mul($b)->scale);
        }
        echo "## div / negative pow / sqrt: ten past the RECEIVER, trimmed back to it\n";
        foreach ([['1', '3'], ['1.5', '2.25'], ['1.0', '3'], ['1.00', '3.00'],
                  ['10', '2'], ['1', '8'], ['1.500', '1.00'], ['2.50', '0.5'],
                  ['0', '3'], ['1', '32768']] as [$x, $y]) {
            $r = (new Number($x))->div(new Number($y));
            printf("%-8s / %-8s = %-26s scale=%d\n", $x, $y, (string)$r, $r->scale);
        }
        foreach ([['1.5', 3], ['1.5', -3], ['2', 10], ['2', -1], ['1.50', 2], ['2.5', 0],
                  ['0.1', 3], ['1.50', -1], ['2.00', -2], ['2', -10]] as [$x, $e]) {
            $r = (new Number($x))->pow($e);
            printf("%-8s ** %-4d = %-26s scale=%d\n", $x, $e, (string)$r, $r->scale);
        }
        foreach (['2', '9', '0.25', '2.00', '9.0000', '152.2756', '1000000',
                  '0.250000', '1.21', '0.0001'] as $x) {
            $r = (new Number($x))->sqrt();
            printf("sqrt(%-10s) = %-26s scale=%d\n", $x, (string)$r, $r->scale);
        }
        echo "## mod/divmod keep max; the quotient is always an integer\n";
        foreach ([['10', '3'], ['10.5', '3'], ['1', '0.7'], ['-10', '3'],
                  ['10.00', '3'], ['10', '3.00']] as [$x, $y]) {
            $r = (new Number($x))->mod(new Number($y));
            $dm = (new Number($x))->divmod(new Number($y));
            printf("%-8s %% %-8s = %-8s scale=%d divmod=[%s(%d),%s(%d)]\n", $x, $y,
                (string)$r, $r->scale, (string)$dm[0], $dm[0]->scale,
                (string)$dm[1], $dm[1]->scale);
        }
        echo "## powmod answers at scale 0 whatever its operands' places are\n";
        $p = (new Number('4.00'))->powmod(3, 5);
        printf("%s(%d) %s %s\n", (string)$p, $p->scale,
            (string)(new Number('-5'))->powmod(3, 7),
            (string)(new Number('4'))->powmod(3, 5, 3));
        echo "## an explicit \$scale overrides every one of those rules\n";
        $a = new Number('1.5');
        foreach ([0, 1, 5] as $sc) {
            printf("sc=%d add=%s div=%s mod=%s pow=%s sqrt=%s\n", $sc,
                $a->add(new Number('3'), $sc), $a->div(new Number('3'), $sc),
                $a->mod(new Number('0.7'), $sc), $a->pow(3, $sc), $a->sqrt($sc));
        }
        echo "## round/floor/ceil, and compare's \$scale CUT\n";
        foreach ([-2, -1, 0, 1, 3] as $prec) {
            $r = (new Number('1234.5678'))->round($prec);
            printf("round %-3d -> %s (%d)\n", $prec, (string)$r, $r->scale);
        }
        var_dump((string)(new Number('1234.5678'))->floor(),
                 (string)(new Number('1234.5678'))->ceil(),
                 (string)(new Number('2.5'))->round(0, RoundingMode::HalfEven));
        var_dump((new Number('1.1'))->compare(new Number('1.2')),
                 (new Number('1.1'))->compare(new Number('1.2'), 0),
                 (new Number('1.10'))->compare(new Number('1.1')),
                 (new Number('1'))->compare('1.0000'),
                 (new Number('1'))->compare(1));

        echo "## the OPERATORS\n";
        $b = new Number('2.25');
        foreach ([['+', $a + $b], ['-', $a - $b], ['*', $a * $b], ['/', $a / $b],
                  ['%', $a % $b], ['**', $a ** 3]] as [$op, $r]) {
            printf("%-3s %s (%d)\n", $op, (string)$r, $r->scale);
        }
        var_dump((string)($a + 1), (string)(1 + $a), (string)($a + '1.5'),
                 (string)('3' - $a), (string)($a + true), (string)(-$a), (string)(+$a));
        $c = $a; $c++; $d = $a; $d--;
        var_dump((string)$c, (string)$d);
        $e = $a; $e += 1; $f = $a; $f *= 2; $g = $a; $g /= 2; $h = $a; $h **= 2;
        var_dump((string)$e, (string)$f, (string)$g, (string)$h);
        var_dump($a <=> $b, $a == $b, $a == '1.50', $a == new Number('1.500'),
                 $a < $b, $a < '2', $a < 2, $a == true, $a == null);
        echo "## strings, casts and the value/scale pair\n";
        var_dump((string)$a, 'x' . $a, $a->value, $a->scale, isset($a->value),
                 get_object_vars($a), (array)$a, json_encode(['n' => $a]));
        var_dump(serialize($a), (string)unserialize(serialize($a)), $a->__serialize());
        var_dump($a instanceof Stringable, (string)(clone $a));

        echo "## the refusals\n";
        foreach ([['add', 'x'], ['pow', '1.5'], ['compare', 'x']] as [$m, $arg]) {
            try { $a->$m($arg); } catch (Throwable $ex) {
                printf("%-8s %s: %s\n", $m, get_class($ex), $ex->getMessage());
            }
        }
        foreach ([fn() => new Number('1e3'), fn() => new Number([]),
                  fn() => $a->div(0), fn() => $a->mod(0), fn() => $a->divmod(0),
                  fn() => (new Number('-1'))->sqrt(), fn() => (new Number('0'))->pow(-1),
                  fn() => $a->powmod(-1, 5), fn() => (new Number('1.5'))->powmod(2, 5),
                  fn() => $a->powmod(2, 0), fn() => $a->add(1, -1),
                  fn() => $a->round(PHP_INT_MAX), fn() => $a->round(0, 1),
                  fn() => $a / 0, fn() => $a % 0, fn() => $a ** '2.5',
                  fn() => $a + 'abc', fn() => 'abc' + $a, fn() => $a + [],
                  fn() => $a | $b, fn() => ~$a] as $bad) {
            try { $bad(); } catch (Throwable $ex) {
                echo get_class($ex), ': ', $ex->getMessage(), "\n";
            }
        }
        try { $a->value = '2'; } catch (Throwable $ex) { echo get_class($ex), ': ', $ex->getMessage(), "\n"; }
        try { $a->x = '2'; } catch (Throwable $ex) { echo get_class($ex), ': ', $ex->getMessage(), "\n"; }
        try { unset($a->value); } catch (Throwable $ex) { echo get_class($ex), ': ', $ex->getMessage(), "\n"; }
        echo "## sorting and searching go through the same comparison\n";
        $list = [new Number('3'), new Number('1.5'), new Number('2')];
        sort($list);
        var_dump(implode(',', array_map(fn($v) => (string)$v, $list)));
        var_dump(in_array(new Number('1.50'), [new Number('1.5')]),
                 in_array(new Number('1.50'), [new Number('1.5')], true));
        var_dump((bool)new Number('0'), empty(new Number('0')));
        var_dump(bcscale());
    } finally {
        bcscale($saved);
    }
}
bcnumber_arith_probe();
?>
--EXPECT--
## the constructor keeps the written scale, and normalizes the rest
'1.500'  -> 1.500    scale=3
'1.5'    -> 1.5      scale=1
'5'      -> 5        scale=0
'-0.0'   -> 0.0      scale=1
'007'    -> 7        scale=0
''       -> 0        scale=0
'.'      -> 0        scale=0
'+1.5'   -> 1.5      scale=1
'-0'     -> 0        scale=0
string(1) "5"
string(20) "-9223372036854775808"
## add/sub/mul: max, max, SUM
1.5    2.25   add=3.75(2) sub=-0.75(2) mul=3.375(3)
1.50   2.2    add=3.70(2) sub=-0.70(2) mul=3.300(3)
1      2      add=3(0) sub=-1(0) mul=2(0)
1.0    2      add=3.0(1) sub=-1.0(1) mul=2.0(1)
## div / negative pow / sqrt: ten past the RECEIVER, trimmed back to it
1        / 3        = 0.3333333333               scale=10
1.5      / 2.25     = 0.66666666666              scale=11
1.0      / 3        = 0.33333333333              scale=11
1.00     / 3.00     = 0.333333333333             scale=12
10       / 2        = 5                          scale=0
1        / 8        = 0.125                      scale=3
1.500    / 1.00     = 1.500                      scale=3
2.50     / 0.5      = 5.00                       scale=2
0        / 3        = 0                          scale=0
1        / 32768    = 0.0000305175               scale=10
1.5      ** 3    = 3.375                      scale=3
1.5      ** -3   = 0.29629629629              scale=11
2        ** 10   = 1024                       scale=0
2        ** -1   = 0.5                        scale=1
1.50     ** 2    = 2.2500                     scale=4
2.5      ** 0    = 1                          scale=0
0.1      ** 3    = 0.001                      scale=3
1.50     ** -1   = 0.666666666666             scale=12
2.00     ** -2   = 0.25                       scale=2
2        ** -10  = 0.0009765625               scale=10
sqrt(2         ) = 1.4142135623               scale=10
sqrt(9         ) = 3                          scale=0
sqrt(0.25      ) = 0.50                       scale=2
sqrt(2.00      ) = 1.414213562373             scale=12
sqrt(9.0000    ) = 3.0000                     scale=4
sqrt(152.2756  ) = 12.3400                    scale=4
sqrt(1000000   ) = 1000                       scale=0
sqrt(0.250000  ) = 0.500000                   scale=6
sqrt(1.21      ) = 1.10                       scale=2
sqrt(0.0001    ) = 0.0100                     scale=4
## mod/divmod keep max; the quotient is always an integer
10       % 3        = 1        scale=0 divmod=[3(0),1(0)]
10.5     % 3        = 1.5      scale=1 divmod=[3(0),1.5(1)]
1        % 0.7      = 0.3      scale=1 divmod=[1(0),0.3(1)]
-10      % 3        = -1       scale=0 divmod=[-3(0),-1(0)]
10.00    % 3        = 1.00     scale=2 divmod=[3(0),1.00(2)]
10       % 3.00     = 1.00     scale=2 divmod=[3(0),1.00(2)]
## powmod answers at scale 0 whatever its operands' places are
4(0) -6 4.000
## an explicit $scale overrides every one of those rules
sc=0 add=4 div=0 mod=0 pow=3 sqrt=1
sc=1 add=4.5 div=0.5 mod=0.1 pow=3.3 sqrt=1.2
sc=5 add=4.50000 div=0.50000 mod=0.10000 pow=3.37500 sqrt=1.22474
## round/floor/ceil, and compare's $scale CUT
round -2  -> 1200 (0)
round -1  -> 1230 (0)
round 0   -> 1235 (0)
round 1   -> 1234.6 (1)
round 3   -> 1234.568 (3)
string(4) "1234"
string(4) "1235"
string(1) "2"
int(-1)
int(0)
int(0)
int(0)
int(0)
## the OPERATORS
+   3.75 (2)
-   -0.75 (2)
*   3.375 (3)
/   0.66666666666 (11)
%   1.50 (2)
**  3.375 (3)
string(3) "2.5"
string(3) "2.5"
string(3) "3.0"
string(3) "1.5"
string(3) "2.5"
string(4) "-1.5"
string(3) "1.5"
string(3) "2.5"
string(3) "0.5"
string(3) "2.5"
string(3) "3.0"
string(4) "0.75"
string(4) "2.25"
int(-1)
bool(false)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(false)
## strings, casts and the value/scale pair
string(3) "1.5"
string(4) "x1.5"
string(3) "1.5"
int(1)
bool(true)
array(2) {
  ["value"]=>
  string(3) "1.5"
  ["scale"]=>
  int(1)
}
array(2) {
  ["value"]=>
  string(3) "1.5"
  ["scale"]=>
  int(1)
}
string(31) "{"n":{"value":"1.5","scale":1}}"
string(47) "O:13:"BcMath\Number":1:{s:5:"value";s:3:"1.5";}"
string(3) "1.5"
array(1) {
  ["value"]=>
  string(3) "1.5"
}
bool(true)
string(3) "1.5"
## the refusals
add      ValueError: BcMath\Number::add(): Argument #1 ($num) is not well-formed
pow      ValueError: BcMath\Number::pow(): Argument #1 ($exponent) exponent cannot have a fractional part
compare  ValueError: BcMath\Number::compare(): Argument #1 ($num) is not well-formed
ValueError: BcMath\Number::__construct(): Argument #1 ($num) is not well-formed
TypeError: BcMath\Number::__construct(): Argument #1 ($num) must be of type string|int, array given
DivisionByZeroError: Division by zero
DivisionByZeroError: Modulo by zero
DivisionByZeroError: Division by zero
ValueError: Base number must be greater than or equal to 0
DivisionByZeroError: Negative power of zero
ValueError: Base number cannot have a fractional part
ValueError: Base number cannot have a fractional part
ValueError: Base number cannot have a fractional part
ValueError: BcMath\Number::add(): Argument #2 ($scale) must be between 0 and 2147483647
ValueError: BcMath\Number::round(): Argument #1 ($precision) must be between -9223372036854775808 and 2147483647
TypeError: BcMath\Number::round(): Argument #2 ($mode) must be of type RoundingMode, int given
DivisionByZeroError: Division by zero
DivisionByZeroError: Modulo by zero
ValueError: exponent cannot have a fractional part
ValueError: Right string operand cannot be converted to BcMath\Number
ValueError: Left string operand cannot be converted to BcMath\Number
TypeError: Unsupported operand types: BcMath\Number + array
TypeError: Unsupported operand types: BcMath\Number | BcMath\Number
TypeError: Cannot perform bitwise not on BcMath\Number
Error: Cannot modify readonly property BcMath\Number::$value
Error: Cannot create dynamic property BcMath\Number::$x
Error: Cannot unset readonly property BcMath\Number::$value
## sorting and searching go through the same comparison
string(7) "1.5,2,3"
bool(true)
bool(false)
bool(false)
bool(true)
int(7)
