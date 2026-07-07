--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
str_pad("hello") throws ArgumentCountError with too few arguments (PHP 8)
--FILE--
<?php
try {
    str_pad("hello");
} catch (\ArgumentCountError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
str_pad() expects at least 2 arguments, 1 given
