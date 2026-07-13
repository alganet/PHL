--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
strncasecmp() throws ValueError for a negative length (PHP 8)
--FILE--
<?php
try {
    strncasecmp("abc", "ABD", -1);
} catch (\ValueError $e) {
    echo $e->getMessage(), "\n";
}
// A non-negative length still compares case-insensitively.
echo strncasecmp("ABC", "abd", 2), "\n";
?>
--EXPECT--
strncasecmp(): Argument #3 ($length) must be greater than or equal to 0
0
--CLEAN--
<?php
