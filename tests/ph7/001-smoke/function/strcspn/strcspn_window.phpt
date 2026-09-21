--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
strcspn() scans php's raw ($offset,$length) window, not the first whitespace token
--FILE--
<?php
// Raw-byte scan: whitespace is ordinary input, so the run continues past it
// unless the mask names it. PH7 answered 3 and 1 for the first two lines.
echo strcspn("  abc", "z"), "\n";   // 5
echo strcspn("a b c", "z"), "\n";   // 5
echo strcspn("a b c", " "), "\n";   // 1 -- the mask DOES name the space

// A negative $offset counts back from the end and clamps to the start.
echo strcspn("abcabc", "z", -2), "\n";  // 2
echo strcspn("abcabc", "z", -10), "\n"; // 6

// An $offset past the end leaves an empty window.
echo strcspn("abcabc", "z", 6), "\n";   // 0
echo strcspn("abcabc", "z", 7), "\n";   // 0

// Zero and negative $length, which PH7 ignored entirely.
echo strcspn("abcabc", "z", 0, 0), "\n";   // 0
echo strcspn("abcabc", "z", 0, -1), "\n";  // 5
echo strcspn("abcabc", "z", 0, -6), "\n";  // 0
echo strcspn("abcabc", "z", 0, -7), "\n";  // 0
echo strcspn("abcabc", "z", 2, -2), "\n";  // 2
echo strcspn("abcabc", "z", 0, 100), "\n"; // 6

// An empty mask matches nothing, so the answer is the WINDOW's length -- PH7
// short-circuited to the whole string length and ignored the window.
echo strcspn("abcdef", ""), "\n";        // 6
echo strcspn("abcdef", "", 2), "\n";     // 4
echo strcspn("abcdef", "", 0, 2), "\n";  // 2
echo strcspn("", ""), "\n";              // 0

try {
    strcspn("abc", "z", "x");
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
5
5
1
2
6
0
0
0
5
0
0
2
6
6
4
2
0
strcspn(): Argument #3 ($offset) must be of type int, string given
--CLEAN--
<?php
