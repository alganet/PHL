--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: strcspn with offset and length parameters
--FILE--
<?php
// Test strcspn with offset parameter
echo strcspn("abcdef", "cd", 2) . "\n";  // offset 2, starts at 'c' -> 0

// Test with offset that starts before match
echo strcspn("abcdef", "cd", 1) . "\n";  // offset 1, starts at 'b' -> 1

// Test with empty mask returns string length
echo strcspn("abcdef", "") . "\n"; // 6

// Test with only offset (no length)
echo strcspn("abcdef", "ab", 2) . "\n"; // 4 (from offset 2, "cdef")

// Test offset at position 0
echo strcspn("abcdef", "a", 0) . "\n"; // 0

// Test offset beyond string length
echo strcspn("abc", "xyz", 10) . "\n"; // 0
?>
--EXPECT--
0
1
6
4
0
0
--CLEAN--
<?php

