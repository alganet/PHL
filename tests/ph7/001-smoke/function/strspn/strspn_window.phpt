--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
strspn() scans php's raw ($offset,$length) window, not the first whitespace token
--FILE--
<?php
// The scan is over raw bytes: a space is just another character, matched only
// when the mask holds it. PH7 used to skip leading spaces and stop at the next
// one, so the first three lines answered 3, 3 and 1.
echo strspn("  abc", "abc"), "\n";       // 0 -- leading space is not in the mask
echo strspn("  abc", " abc"), "\n";      // 5 -- space IS in the mask
echo strspn("a b c", "abc "), "\n";      // 5 -- the scan does not stop at a space
echo strspn("42 is the answer", "1234567890"), "\n"; // 2

// A negative $offset counts back from the end and clamps to the start.
echo strspn("abcabc", "abc", -2), "\n";  // 2
echo strspn("abcabc", "abc", -10), "\n"; // 6 -- clamped to offset 0, not "invalid"

// An $offset past the end leaves an empty window.
echo strspn("abcabc", "abc", 6), "\n";   // 0
echo strspn("abcabc", "abc", 7), "\n";   // 0

// A negative $length leaves that many bytes off the end of the remaining span;
// a zero-length window answers 0. PH7 ignored both and measured to the end.
echo strspn("abcabc", "abc", 0, 0), "\n";   // 0
echo strspn("abcabc", "abc", 0, -1), "\n";  // 5
echo strspn("abcabc", "abc", 0, -6), "\n";  // 0
echo strspn("abcabc", "abc", 0, -7), "\n";  // 0
echo strspn("abcabc", "abc", 2, -2), "\n";  // 2
echo strspn("abcabc", "abc", -3, -1), "\n"; // 2
echo strspn("abcabc", "abc", 0, 100), "\n"; // 6 -- clamped to the string

// An explicit null $length means "to the end of the string".
echo strspn("abcabc", "abc", 3, null), "\n"; // 3

// 64-bit offsets do not wrap into a valid window.
echo strspn("aaa", "a", PHP_INT_MAX), "\n";    // 0
echo strspn("aaa", "a", PHP_INT_MIN), "\n";    // 3
echo strspn("aaa", "a", 0, PHP_INT_MIN), "\n"; // 0

// The empty mask needs no special case: nothing matches, so the span is 0.
echo strspn("abc", ""), "\n";  // 0
echo strspn("", "abc"), "\n";  // 0

try {
    strspn("abc", "abc", "x");
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}
try {
    strspn("abc", "abc", 0, "x");
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
0
5
5
2
2
6
0
0
0
5
0
0
2
2
6
3
0
3
0
0
0
strspn(): Argument #3 ($offset) must be of type int, string given
strspn(): Argument #4 ($length) must be of type ?int, string given
--CLEAN--
<?php
