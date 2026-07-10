--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
str_shuffle() throws ArgumentCountError with too few arguments (PHP 8)
--FILE--
<?php
try {
    str_shuffle();
} catch (\ArgumentCountError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
str_shuffle() expects exactly 1 argument, 0 given
