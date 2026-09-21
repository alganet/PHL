--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
str_pad $pad_type outside LEFT/RIGHT/BOTH throws ValueError once padding is required
--FILE--
<?php
$spt_try = function ($fn) {
    try {
        var_dump($fn());
    } catch (\ValueError $e) {
        echo $e->getMessage(), "\n";
    }
};
$spt_try(fn() => str_pad("a", 5, " ", 3));
$spt_try(fn() => str_pad("a", 5, " ", -1));
// no padding required: php returns the input before validating $pad_type
$spt_try(fn() => str_pad("abc", 2, " ", 9));
// the three valid types
$spt_try(fn() => str_pad("a", 5, "xy", STR_PAD_LEFT));
$spt_try(fn() => str_pad("a", 5, "xy", STR_PAD_RIGHT));
$spt_try(fn() => str_pad("a", 6, "xy", STR_PAD_BOTH));
?>
--EXPECT--
str_pad(): Argument #4 ($pad_type) must be STR_PAD_LEFT, STR_PAD_RIGHT, or STR_PAD_BOTH
str_pad(): Argument #4 ($pad_type) must be STR_PAD_LEFT, STR_PAD_RIGHT, or STR_PAD_BOTH
string(3) "abc"
string(5) "xyxya"
string(5) "axyxy"
string(6) "xyaxyx"
--CLEAN--
<?php
