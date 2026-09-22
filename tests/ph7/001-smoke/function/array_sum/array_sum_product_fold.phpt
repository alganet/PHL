--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_sum()/array_product() fold php's + and * over the elements, promoting on overflow
--FILE--
<?php
/* Both functions are the operator folded from an int identity, so the accumulator
 * promotes to float the moment the int result would not fit, every element is
 * classified on its own, and the operands the operator refuses report per-element
 * warnings. */
class BareFold {}
set_error_handler(function ($n, $s) { echo "  W: $s\n"; return true; });
function foldNumStr($label, $a) {
    echo $label, "\n  sum: ";
    $v = array_sum($a);
    echo (is_float($v) ? "float " : "int "), var_export($v, true), "\n  prod: ";
    $v = array_product($a);
    echo (is_float($v) ? "float " : "int "), var_export($v, true), "\n";
}
/* The int accumulator promotes instead of wrapping. */
foldNumStr('[PHP_INT_MAX, 1]', [PHP_INT_MAX, 1]);
foldNumStr('[PHP_INT_MAX, PHP_INT_MAX, 2]', [PHP_INT_MAX, PHP_INT_MAX, 2]);
foldNumStr('[PHP_INT_MIN, -1]', [PHP_INT_MIN, -1]);
foldNumStr('["9223372036854775808", 1]', ["9223372036854775808", 1]);
foldNumStr('["-9223372036854775809"]', ["-9223372036854775809"]);
/* Every element is classified on its own -- not just the first one. */
foldNumStr('[1, 2.5]', [1, 2.5]);
foldNumStr('["2.5", 2]', ["2.5", 2]);
foldNumStr('["1e3", 2]', ["1e3", 2]);
foldNumStr('[2.5, 1]', [2.5, 1]);
foldNumStr('[1, 2, 3]', [1, 2, 3]);
foldNumStr('["1", "2"]', ["1", "2"]);
foldNumStr('[null, 2]', [null, 2]);
foldNumStr('[true, false]', [true, false]);
/* A leading-numeric string contributes its prefix, behind php's warning. */
foldNumStr('["3abc", 2]', ["3abc", 2]);
/* The refused operands: an array and an object are skipped, a string with no
 * numeric prefix at all folds in as 0. */
foldNumStr('[[1], 2]', [[1], 2]);
foldNumStr('[[1, 2]]', [[1, 2]]);
foldNumStr('[new BareFold(), 2]', [new BareFold(), 2]);
foldNumStr('["abc", 2]', ["abc", 2]);
foldNumStr('["", 2]', ["", 2]);
/* A resource reports too, and folds in as its id (whose VALUE is not php's, so
 * assert the arithmetic rather than print it). */
$fh = fopen("php://memory", "r");
echo "resource\n";
var_dump(array_sum([$fh, 2]) === (int)$fh + 2);
var_dump(array_product([$fh, 2]) === (int)$fh * 2);
fclose($fh);
/* The identities of an empty array. */
echo "empty\n";
var_dump(array_sum([]), array_product([]));
restore_error_handler();
?>
--EXPECT--
[PHP_INT_MAX, 1]
  sum: float 9.223372036854776E+18
  prod: int 9223372036854775807
[PHP_INT_MAX, PHP_INT_MAX, 2]
  sum: float 1.8446744073709552E+19
  prod: float 1.7014118346046923E+38
[PHP_INT_MIN, -1]
  sum: float -9.223372036854776E+18
  prod: float 9.223372036854776E+18
["9223372036854775808", 1]
  sum: float 9.223372036854776E+18
  prod: float 9.223372036854776E+18
["-9223372036854775809"]
  sum: float -9.223372036854776E+18
  prod: float -9.223372036854776E+18
[1, 2.5]
  sum: float 3.5
  prod: float 2.5
["2.5", 2]
  sum: float 4.5
  prod: float 5.0
["1e3", 2]
  sum: float 1002.0
  prod: float 2000.0
[2.5, 1]
  sum: float 3.5
  prod: float 2.5
[1, 2, 3]
  sum: int 6
  prod: int 6
["1", "2"]
  sum: int 3
  prod: int 2
[null, 2]
  sum: int 2
  prod: int 0
[true, false]
  sum: int 1
  prod: int 0
["3abc", 2]
  sum:   W: A non-numeric value encountered
int 5
  prod:   W: A non-numeric value encountered
int 6
[[1], 2]
  sum:   W: array_sum(): Addition is not supported on type array
int 2
  prod:   W: array_product(): Multiplication is not supported on type array
int 2
[[1, 2]]
  sum:   W: array_sum(): Addition is not supported on type array
int 0
  prod:   W: array_product(): Multiplication is not supported on type array
int 1
[new BareFold(), 2]
  sum:   W: array_sum(): Addition is not supported on type BareFold
int 2
  prod:   W: array_product(): Multiplication is not supported on type BareFold
int 2
["abc", 2]
  sum:   W: array_sum(): Addition is not supported on type string
int 2
  prod:   W: array_product(): Multiplication is not supported on type string
int 0
["", 2]
  sum:   W: array_sum(): Addition is not supported on type string
int 2
  prod:   W: array_product(): Multiplication is not supported on type string
int 0
resource
  W: array_sum(): Addition is not supported on type resource
bool(true)
  W: array_product(): Multiplication is not supported on type resource
bool(true)
empty
int(0)
int(1)
--CLEAN--
<?php
