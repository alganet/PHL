--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
str_replace("search", "replace") throws ArgumentCountError with too few arguments (PHP 8)
--FILE--
<?php
try {
    str_replace("search", "replace");
} catch (\ArgumentCountError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
str_replace() expects at least 3 arguments, 2 given
