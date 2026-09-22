--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Two integer-shaped numeric strings past int64 do not compare through their doubles
--FILE--
<?php
/* Converting an out-of-range integer string to a double throws away the digits
 * that tell two of them apart, so php does not compare them that way. Same side
 * and same double: compare the raw BYTES. One side past the range and the other
 * an integer-shaped string that FITS: the overflowing side simply is the greater
 * (or lesser) one. Both rules are for a string against a STRING -- a string
 * against an int VALUE really does compare as doubles. */
function cmpNumStrOv($a, $b) {
    printf("%-3s %-3s %-3s %-3s | %s\n", var_export($a == $b, true), var_export($a < $b, true),
        var_export($a > $b, true), $a <=> $b, var_export($a === $b, true));
}
/* Same side, same double -> the bytes decide. */
cmpNumStrOv("9223372036854775808", "9223372036854775809");
cmpNumStrOv("9223372036854775809", "9223372036854775808");
cmpNumStrOv("9223372036854775808", "9223372036854775810");
cmpNumStrOv("9223372036854775808", "9223372036854775808");
cmpNumStrOv("-9223372036854775809", "-9223372036854775810");
cmpNumStrOv("-9223372036854775810", "-9223372036854775809");
/* The bytes are the RAW ones: a sign, a leading zero and whitespace all count. */
cmpNumStrOv("+9223372036854775808", "9223372036854775808");
cmpNumStrOv("00009223372036854775808", "9223372036854775808");
cmpNumStrOv("9223372036854775808 ", "9223372036854775808");
/* Two digit runs that both overflow to infinity are equal as doubles and still
 * distinct here. */
$inf1 = "1" . str_repeat("0", 400);
$inf2 = "2" . str_repeat("0", 400);
cmpNumStrOv($inf1, $inf2);
cmpNumStrOv($inf1, $inf1);
/* One side past the range, the other an integer string that fits. */
cmpNumStrOv("9223372036854775808", "9223372036854775807");
cmpNumStrOv("9223372036854775807", "9223372036854775808");
cmpNumStrOv("-9223372036854775808", "-9223372036854775809");
cmpNumStrOv("-9223372036854775809", "-9223372036854775808");
cmpNumStrOv("18446744073709551616", "1");
/* Everything else stays numeric. Opposite sides, unequal doubles, a float-SHAPED
 * operand of the very same magnitude, and an int/float VALUE on either side. */
cmpNumStrOv("-9223372036854775809", "9223372036854775808");
cmpNumStrOv("9223372036854775808", "18446744073709551616");
cmpNumStrOv("9223372036854775808", "9223372036854775808.0");
cmpNumStrOv("9223372036854775808", "9.223372036854775808e18");
cmpNumStrOv("9223372036854775808", 9223372036854775807);
cmpNumStrOv(9223372036854775807, "9223372036854775809");
cmpNumStrOv("9223372036854775808", 9.2233720368547758E+18);
/* Consumers of the same comparator. */
$x = "9223372036854775809";
switch ($x) { case "9223372036854775808": echo "matched\n"; break; default: echo "default\n"; }
var_dump(in_array("9223372036854775809", ["9223372036854775808"]));
var_dump(array_search("9223372036854775809", ["9223372036854775808"]));
echo max("9223372036854775808", "9223372036854775807"), "\n";
echo min("9223372036854775808", "9223372036854775807"), "\n";
$s = ["9223372036854775810", "9223372036854775808", "9223372036854775809"];
sort($s);
echo implode(",", $s), "\n";
sort($s, SORT_NUMERIC);
echo implode(",", $s), "\n";
?>
--EXPECT--
false true false -1  | false
false false true 1   | false
false true false -1  | false
true false false 0   | true
false true false -1  | false
false false true 1   | false
false true false -1  | false
false true false -1  | false
false false true 1   | false
false true false -1  | false
true false false 0   | true
false false true 1   | false
false true false -1  | false
false false true 1   | false
false true false -1  | false
false false true 1   | false
false true false -1  | false
false true false -1  | false
true false false 0   | false
true false false 0   | false
true false false 0   | false
true false false 0   | false
true false false 0   | false
default
bool(false)
bool(false)
9223372036854775808
9223372036854775807
9223372036854775808,9223372036854775809,9223372036854775810
9223372036854775808,9223372036854775809,9223372036854775810
--CLEAN--
<?php
