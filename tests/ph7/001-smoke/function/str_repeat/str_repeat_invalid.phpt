--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
str_repeat() throws ArgumentCountError with too few arguments (PHP 8)
--FILE--
<?php
try {
    str_repeat();
} catch (\ArgumentCountError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
str_repeat() expects exactly 2 arguments, 0 given
