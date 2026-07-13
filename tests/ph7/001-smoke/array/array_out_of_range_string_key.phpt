--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An out-of-range numeric string array key stays a string key (PHP canonical rule)
--FILE--
<?php
/* PHP integerizes a string key only when (string)(int)$k === $k. A magnitude
 * beyond the signed 64-bit range therefore stays a STRING key instead of being
 * saturated to PHP_INT_MAX/MIN and colliding with the real boundary key. The
 * in-range boundary keys are still integers. */
$a = [];
$a["9223372036854775808"]  = "over_max";   // > PHP_INT_MAX -> string key
$a["9223372036854775807"]  = "int_max";    // == PHP_INT_MAX -> int key
$a["-9223372036854775809"] = "under_min";  // < PHP_INT_MIN -> string key
$a["-9223372036854775808"] = "int_min";    // == PHP_INT_MIN -> int key
$a["99999999999999999999"] = "huge";       // 20 digits -> string key
echo count($a), "\n";
foreach ($a as $k => $v) {
    echo (is_int($k) ? "int" : "str"), " ", $k, " => ", $v, "\n";
}
?>
--EXPECT--
5
str 9223372036854775808 => over_max
int 9223372036854775807 => int_max
str -9223372036854775809 => under_min
int -9223372036854775808 => int_min
str 99999999999999999999 => huge
