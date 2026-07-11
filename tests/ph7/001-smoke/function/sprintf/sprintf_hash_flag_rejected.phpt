--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
sprintf() rejects the C '#' alternate-form flag with a ValueError (PHP 8)
--FILE--
<?php
// '#' is not a PHP format flag; php 8 reports it as an unknown specifier.
foreach (['%#x', '%#X', '%#o', '%#b'] as $fmt) {
    try {
        sprintf($fmt, 255);
        echo "NO_ERROR\n";
    } catch (\ValueError $e) {
        echo $e->getMessage(), "\n";
    }
}
?>
--EXPECT--
Unknown format specifier "#"
Unknown format specifier "#"
Unknown format specifier "#"
Unknown format specifier "#"
--CLEAN--
<?php
