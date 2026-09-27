--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A lossy float reaching BcMath\Number is refused (PHL half of the §10 twin)
--DESCRIPTION--
php reaches BcMath\Number's `int` arm for a FLOAT through an implicit conversion
it DEPRECATES when precision is lost, so `new Number(1.5)` is 1 there and
`$n + 1.5` adds 1. §10 removes php's deprecated surface, so PHL refuses the
conversion -- with the very wording php itself uses for the floats IT cannot
convert either (NAN, INF, 1e20). An INTEGRAL float still converts, in both.
In a COMPARISON, which cannot throw, the refusal takes php's own shape for a
pair it will not order: uncomparable, which is 1 from either side.
See number_lossy_float_zend.phpt for what php answers.
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip PHL-only half of the twin'; ?>
--FILE--
<?php
use BcMath\Number;
$a = new Number('1.5');
foreach ([fn() => new Number(1.5), fn() => $a->add(1.5), fn() => $a + 1.5,
          fn() => $a - 1.5, fn() => $a * 1.5, fn() => $a ** 2.5,
          fn() => $a + NAN, fn() => $a + INF, fn() => $a + 1e20,
          fn() => new Number(null), fn() => $a->add(null)] as $bad) {
    try { $bad(); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
}
echo "## an INTEGRAL float is not lossy and converts, here as in php\n";
var_dump((string)new Number(2.0), (string)($a + 2.0), (string)($a + -2.0),
         (string)$a->add(3.0));
echo "## a comparison cannot throw: the refusal is php's UNCOMPARABLE\n";
var_dump($a == 1.5, $a < 2.5, $a > 2.5, $a <=> 1.5, 1.5 <=> $a);
var_dump($a == 2.0, $a < 2.0, $a <=> 1.0);
?>
--EXPECT--
TypeError: BcMath\Number::__construct(): Argument #1 ($num) must be of type string|int, float given
TypeError: BcMath\Number::add(): Argument #1 ($num) must be of type int, string, or BcMath\Number, float given
TypeError: Unsupported operand types: BcMath\Number + float
TypeError: Unsupported operand types: BcMath\Number - float
TypeError: Unsupported operand types: BcMath\Number * float
TypeError: Unsupported operand types: BcMath\Number ** float
TypeError: Unsupported operand types: BcMath\Number + float
TypeError: Unsupported operand types: BcMath\Number + float
TypeError: Unsupported operand types: BcMath\Number + float
TypeError: BcMath\Number::__construct(): Argument #1 ($num) must be of type string|int, null given
TypeError: BcMath\Number::add(): Argument #1 ($num) must be of type int, string, or BcMath\Number, null given
## an INTEGRAL float is not lossy and converts, here as in php
string(1) "2"
string(3) "3.5"
string(4) "-0.5"
string(3) "4.5"
## a comparison cannot throw: the refusal is php's UNCOMPARABLE
bool(false)
bool(false)
bool(false)
int(1)
int(1)
bool(false)
bool(true)
int(1)
