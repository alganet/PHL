--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
substr_count with offset and length parameters
--FILE--
<?php
$haystack = "abababababa";
$needle = "aba";

// Valid offset and length windows.
echo "substr_count with offset 0, length 6: ", substr_count($haystack, $needle, 0, 6), "\n"; // "ababab" -> 1
echo "substr_count with offset 2, length 8: ", substr_count($haystack, $needle, 2, 8), "\n"; // "abababab" -> 2

// A length past the end of the haystack throws.
try {
    substr_count($haystack, $needle, 0, 100);
    echo "NO_ERROR\n";
} catch (\ValueError $e) {
    echo "substr_count with large length: ", $e->getMessage(), "\n";
}
?>
--EXPECT--
substr_count with offset 0, length 6: 1
substr_count with offset 2, length 8: 2
substr_count with large length: substr_count(): Argument #4 ($length) must be contained in argument #1 ($haystack)
--CLEAN--
<?php
