--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
substr_count length: out-of-range throws ValueError, negative counts from the end
--FILE--
<?php
// Length greater than the (offset) haystack throws.
try {
    substr_count('abc', 'b', 0, 10);
    echo "NO_ERROR\n";
} catch (\ValueError $e) {
    echo $e->getMessage(), "\n";
}
// Negative length is relative to the end: "abc" with length -1 => "ab" => one 'b'.
echo "result2=" . substr_count('abc', 'b', 0, -1) . "\n";
?>
--EXPECT--
substr_count(): Argument #4 ($length) must be contained in argument #1 ($haystack)
result2=1
--CLEAN--
<?php
