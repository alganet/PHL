--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
substr_compare('abc', 'a') throws ArgumentCountError with too few arguments (PHP 8)
--FILE--
<?php
try {
    substr_compare('abc', 'a');
} catch (\ArgumentCountError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
substr_compare() expects at least 3 arguments, 2 given
