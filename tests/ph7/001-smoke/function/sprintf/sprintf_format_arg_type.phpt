--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
printf-family $format: scalars coerce, array/object throw TypeError (PHP 8)
--FILE--
<?php
// Scalar $format coerces to string (int/float/bool).
echo sprintf(123), "\n";
echo sprintf(12.5), "\n";
echo sprintf(true), "\n";
// array / object $format is a catchable TypeError, checked left-to-right.
foreach ([[1], new stdClass] as $v) {
    try {
        sprintf($v);
    } catch (\TypeError $e) {
        echo $e->getMessage(), "\n";
    }
}
try {
    vsprintf([1], ['a']);
} catch (\TypeError $e) {
    echo $e->getMessage(), "\n";
}
// $format (#1) is reported before $values (#2) when both are bad.
try {
    vsprintf([1], 5);
} catch (\TypeError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
123
12.5
1
sprintf(): Argument #1 ($format) must be of type string, array given
sprintf(): Argument #1 ($format) must be of type string, stdClass given
vsprintf(): Argument #1 ($format) must be of type string, array given
vsprintf(): Argument #1 ($format) must be of type string, array given
--CLEAN--
<?php
