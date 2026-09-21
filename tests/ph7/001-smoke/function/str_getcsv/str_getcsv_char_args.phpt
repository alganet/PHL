--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
str_getcsv separator/enclosure must be a single character, escape empty or one (ValueError)
--FILE--
<?php
$sgc_try = function ($fn) {
    try {
        $r = $fn();
        echo "OK:", implode('|', $r), "\n";
    } catch (\ValueError $e) {
        echo $e->getMessage(), "\n";
    }
};
$sgc_try(fn() => str_getcsv("a,b", ",,"));
$sgc_try(fn() => str_getcsv("a,b", ""));
$sgc_try(fn() => str_getcsv("a,b", ",", "qq"));
$sgc_try(fn() => str_getcsv("a,b", ",", ""));
$sgc_try(fn() => str_getcsv("a,b", ",", '"', "ee"));
// empty escape is accepted: it disables escape processing (php 7.4 rules)
$sgc_try(fn() => str_getcsv('p\\q,r', ",", '"', ""));
// a non-string separator coerces like php's ZPP: int 5 separates on "5"
$sgc_try(fn() => str_getcsv("a5b", 5, '"', ""));
?>
--EXPECT--
str_getcsv(): Argument #2 ($separator) must be a single character
str_getcsv(): Argument #2 ($separator) must be a single character
str_getcsv(): Argument #3 ($enclosure) must be a single character
str_getcsv(): Argument #3 ($enclosure) must be a single character
str_getcsv(): Argument #4 ($escape) must be empty or a single character
OK:p\q|r
OK:a|b
