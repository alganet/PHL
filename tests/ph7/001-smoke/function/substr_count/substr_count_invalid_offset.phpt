--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
substr_count with an out-of-range offset throws ValueError
--FILE--
<?php
try {
    substr_count("hello", "l", 10);
    echo "NO_ERROR\n";
} catch (\ValueError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
substr_count(): Argument #3 ($offset) must be contained in argument #1 ($haystack)
--CLEAN--
<?php
