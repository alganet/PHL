--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An integer-shaped numeric string past the int64 range converts to float, not to a clamped int
--FILE--
<?php
/* php reads an integer-shaped numeric string whose digit run runs past the
 * signed 64-bit range as a FLOAT: the string->number conversion hands the digits
 * to the float reader instead of clamping them. Every arithmetic operator
 * inherits it, because they all go through that one conversion. Clamping instead
 * answered PHP_INT_MAX -- and on the negative side PHP_INT_MIN, a magnitude with
 * no relation to the input. The (int) CAST is php's separate rule and still
 * saturates (see function/intval/intval_out_of_range_saturates). */
function tyNumStrOv($v) { return is_float($v) ? "float" : (is_int($v) ? "int" : "?"); }

/* Out of range on both sides: every operator answers the double the (float) cast
 * has always answered. Values are asserted with === against that cast rather
 * than printed, so the float's rendering is not part of the expectation. */
foreach ([
    "9223372036854775808", "-9223372036854775809", "18446744073709551616",
    "-18446744073709551616", "99999999999999999999999999", " 9223372036854775808 ",
    "0009223372036854775808", "+9223372036854775808",
] as $s) {
    $f = (float)$s;
    printf("%-27s %-5s %s %s %s %s %s %s\n", trim($s), tyNumStrOv($s + 0),
        ($s + 0) === $f      ? "add" : "ADD",
        ($s - 0) === $f      ? "sub" : "SUB",
        ($s * 1) === $f      ? "mul" : "MUL",
        ($s / 1) === $f      ? "div" : "DIV",
        ($s ** 1) === $f     ? "pow" : "POW",
        (-$s) === -$f        ? "neg" : "NEG");
}

/* The boundary values themselves, and everything inside the range, stay int. */
foreach (["9223372036854775807", "-9223372036854775808", "0", "-0", "123",
          "  42  ", "007", "-007"] as $s) {
    echo trim($s), " ", tyNumStrOv($s + 0), " ", $s + 0, "\n";
}

/* A float-SHAPED string was always read as a float, whatever its magnitude. */
echo tyNumStrOv("9223372036854775808.0" + 0), " ", tyNumStrOv("1e19" + 0), " ",
     tyNumStrOv("1.5" + 0), "\n";

/* A leading-numeric string keeps php's warning and reads only its prefix, which
 * overflows on its own terms. */
echo tyNumStrOv(@("9223372036854775808abc" + 0)), " ",
     @("9223372036854775808abc" + 0) === (float)"9223372036854775808" ? "prefix" : "PREFIX", "\n";

/* php's other rules for the same strings are unchanged: the (int) cast saturates,
 * is_numeric() is true, and the string keeps its own bytes as an array KEY
 * because it is not a canonical int. */
$s = "9223372036854775808";
echo (int)$s, " ", intval($s), " ", var_export(is_numeric($s), true), "\n";
echo var_export(array_keys([$s => 1]), true), "\n";

/* Consumers of the same conversion: the comparison operators, max()/min() built
 * on them, and the numeric string sort flag. */
var_dump("18446744073709551616" > "9223372036854775808");
var_dump("9223372036854775808" <=> "18446744073709551616");
var_dump("9223372036854775808" == 9.2233720368547758E+18);
echo max("9223372036854775808", "18446744073709551616"), "\n";
echo min("9223372036854775808", "18446744073709551616"), "\n";
$a = ["18446744073709551616", "99999999999999999999999999", "9223372036854775808"];
sort($a, SORT_NUMERIC);
echo implode(",", $a), "\n";
?>
--EXPECT--
9223372036854775808         float add sub mul div pow neg
-9223372036854775809        float add sub mul div pow neg
18446744073709551616        float add sub mul div pow neg
-18446744073709551616       float add sub mul div pow neg
99999999999999999999999999  float add sub mul div pow neg
9223372036854775808         float add sub mul div pow neg
0009223372036854775808      float add sub mul div pow neg
+9223372036854775808        float add sub mul div pow neg
9223372036854775807 int 9223372036854775807
-9223372036854775808 int -9223372036854775808
0 int 0
-0 int 0
123 int 123
42 int 42
007 int 7
-007 int -7
float float float
float prefix
9223372036854775807 9223372036854775807 true
array (
  0 => '9223372036854775808',
)
bool(true)
int(-1)
bool(true)
18446744073709551616
9223372036854775808
9223372036854775808,18446744073709551616,99999999999999999999999999
--CLEAN--
<?php
