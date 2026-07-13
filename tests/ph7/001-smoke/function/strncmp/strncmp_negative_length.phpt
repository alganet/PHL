--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
strncmp() throws ValueError for a negative length (PHP 8)
--FILE--
<?php
try {
    strncmp("abc", "def", -1);
} catch (\ValueError $e) {
    echo $e->getMessage(), "\n";
}
// A non-negative length still compares normally.
echo strncmp("abc", "abd", 2), "\n";
?>
--EXPECT--
strncmp(): Argument #3 ($length) must be greater than or equal to 0
0
--CLEAN--
<?php
