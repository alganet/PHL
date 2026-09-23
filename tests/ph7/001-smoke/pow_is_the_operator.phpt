--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
pow() is the ** operator: same operand contract, same result type
--DESCRIPTION--
php does not implement pow() separately — the function and the `**` operator are
the same ZEND_API pow_function — so they share an operand contract, a result TYPE
rule and every edge value. PHL's pow() read both arguments as doubles and returned
pow(), which shared none of it: `pow(2, 3)` answered float(8) where `2 ** 3`
answers int(8), a wrong type for the most ordinary call there is, and every
operand `**` refuses was accepted — pow('abc', 2) answered float(0), pow([1], 2)
float(1) and pow($obj, 2) float(1) after a conversion warning. The arithmetic now
lives in one place (PH7_MemObjPow) that the opcode and the builtin both call, and
the contract comes from VmArithOperandCheck, the same routine the operator uses.
--FILE--
<?php
function powShow($label, $fn) {
    try { $out = var_export($fn(), true); }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    echo $label, ' => ', str_replace("\n", '', $out), "\n";
}

/* The function and the operator answer the same thing, value for value. */
$powCases = [
    'int'            => [2, 3],
    'zero exponent'  => [2, 0],
    'negative exp'   => [2, -1],
    'overflow to float' => [2, 64],
    'negative base'  => [-2, 3],
    'zero zero'      => [0, 0],
    'float base'     => [1.5, 2],
    'numeric strings' => ['3', '2'],
    'float string'   => ['3.0', '2'],
    'bool base'      => [true, 3],
    'int max'        => [PHP_INT_MAX, 2],
    'fractional exp' => [2, 0.5],
    'huge exp'       => [2, PHP_INT_MAX],
];
foreach ($powCases as $powLabel => [$powA, $powB]) {
    powShow("$powLabel fn", fn() => pow($powA, $powB));
    powShow("$powLabel op", fn() => $powA ** $powB);
}

/* The operand contract, which pow() had none of. */
powShow('non-numeric string fn', fn() => pow('abc', 2));
powShow('non-numeric string op', fn() => 'abc' ** 2);
powShow('non-numeric exponent', fn() => pow(2, 'abc'));
powShow('array fn', fn() => pow([1], 2));
powShow('array exponent', fn() => pow(2, [1]));
powShow('object fn', fn() => pow(new stdClass, 2));
powShow('resource fn', function () { $f = fopen('php://memory', 'r'); return pow($f, 2); });

/* A LEADING-numeric string is php's warning-and-compute, not a refusal — the
 * same rule every arithmetic operand follows. */
set_error_handler(function ($n, $s) { echo '  W: ', $s, "\n"; return true; });
powShow('leading-numeric fn', fn() => pow('12abc', 2));
powShow('leading-numeric op', fn() => '12abc' ** 2);
restore_error_handler();

/* null is 0 in arithmetic, in both. */
powShow('null base', fn() => pow(null, 2));
powShow('null op', fn() => null ** 2);
--EXPECT--
int fn => 8
int op => 8
zero exponent fn => 1
zero exponent op => 1
negative exp fn => 0.5
negative exp op => 0.5
overflow to float fn => 1.8446744073709552E+19
overflow to float op => 1.8446744073709552E+19
negative base fn => -8
negative base op => -8
zero zero fn => 1
zero zero op => 1
float base fn => 2.25
float base op => 2.25
numeric strings fn => 9
numeric strings op => 9
float string fn => 9.0
float string op => 9.0
bool base fn => 1
bool base op => 1
int max fn => 8.507059173023462E+37
int max op => 8.507059173023462E+37
fractional exp fn => 1.4142135623730951
fractional exp op => 1.4142135623730951
huge exp fn => INF
huge exp op => INF
non-numeric string fn => TypeError: Unsupported operand types: string ** int
non-numeric string op => TypeError: Unsupported operand types: string ** int
non-numeric exponent => TypeError: Unsupported operand types: int ** string
array fn => TypeError: Unsupported operand types: array ** int
array exponent => TypeError: Unsupported operand types: int ** array
object fn => TypeError: Unsupported operand types: stdClass ** int
resource fn => TypeError: Unsupported operand types: resource ** int
  W: A non-numeric value encountered
leading-numeric fn => 144
  W: A non-numeric value encountered
leading-numeric op => 144
null base => 0
null op => 0
