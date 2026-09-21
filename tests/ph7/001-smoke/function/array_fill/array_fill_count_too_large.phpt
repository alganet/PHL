--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_fill count above INT32_MAX is "is too large", negative stays ">= 0"
--FILE--
<?php
$afl_try = function ($fn) {
    try {
        var_dump(count($fn()));
    } catch (\ValueError $e) {
        echo $e->getMessage(), "\n";
    }
};
$afl_try(fn() => array_fill(0, PHP_INT_MAX, "x"));
$afl_try(fn() => array_fill(0, 2147483648, "x"));
$afl_try(fn() => array_fill(0, -2, "x"));
$afl_try(fn() => array_fill(-5, 3, "v"));
?>
--EXPECT--
array_fill(): Argument #2 ($count) is too large
array_fill(): Argument #2 ($count) is too large
array_fill(): Argument #2 ($count) must be greater than or equal to 0
int(3)
--CLEAN--
<?php
