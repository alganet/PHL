--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
sprintf() throws ValueError for an unknown format specifier (PHP 8)
--FILE--
<?php
// PHP 8 raises a catchable ValueError for an unknown conversion specifier.
foreach (['%q', '%y', '%v'] as $fmt) {
    try {
        sprintf($fmt, "test");
        echo "NO_ERROR\n";
    } catch (\ValueError $e) {
        echo $e->getMessage(), "\n";
    }
}
?>
--EXPECT--
Unknown format specifier "q"
Unknown format specifier "y"
Unknown format specifier "v"
--CLEAN--
<?php
