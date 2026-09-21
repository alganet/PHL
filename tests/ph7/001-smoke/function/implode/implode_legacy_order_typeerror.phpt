--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
implode legacy swapped order throws TypeError; the single-array form stays legal
--SKIPIF--
skip: flaky
--FILE--
<?php
$ilo_try = function ($fn) {
    try {
        var_dump($fn());
    } catch (\TypeError $e) {
        echo $e->getMessage(), "\n";
    }
};
$ilo_try(fn() => implode([1, 2], ","));
$ilo_try(fn() => implode([1, 2], null));
$ilo_try(fn() => implode([1, 2], [3]));
$ilo_try(fn() => implode([1, 2]));
$ilo_try(fn() => implode(",", [1, 2]));
$ilo_try(fn() => implode(",", "nope"));
?>
--EXPECT--
implode(): Argument #1 ($separator) must be of type string, array given
implode(): Argument #1 ($separator) must be of type string, array given
implode(): Argument #1 ($separator) must be of type string, array given
string(2) "12"
string(3) "1,2"
implode(): Argument #2 ($array) must be of type ?array, string given
--CLEAN--
<?php
