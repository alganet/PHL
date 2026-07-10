--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
wordwrap throws ValueError for an empty break or width 0 with cut (PHP 8)
--FILE--
<?php
try {
    wordwrap("hello", 5, "");
} catch (\ValueError $e) {
    echo $e->getMessage(), "\n";
}
try {
    wordwrap("hello", 0, "\n", true);
} catch (\ValueError $e) {
    echo $e->getMessage(), "\n";
}
// Empty input short-circuits before the argument checks.
var_dump(wordwrap("", 0, "-", true));
?>
--EXPECT--
wordwrap(): Argument #3 ($break) must not be empty
wordwrap(): Argument #4 ($cut_long_words) cannot be true when argument #2 ($width) is 0
string(0) ""
--CLEAN--
<?php
