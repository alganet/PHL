--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: substr_compare length bounds checking
--DESCRIPTION--
The `$length` that runs past the end of `$haystack` is clamped, and the two
remainders are compared. Assert the SIGN, not the magnitude: php returns
memcmp's raw return value, which the C library leaves implementation-defined —
glibc's vectorized path answers a word difference (-16842754 was observed here)
where its byte-at-a-time path answers -2, and which path runs depends on buffer
alignment. An `=== -2` assertion therefore passed or failed with the process's
memory layout: it flipped on nothing more than an extra environment variable,
and made `make test-compat` intermittently red for a whole session. PHL answers
-2 consistently; both engines agree on the sign, which is all php documents.
--FILE--
<?php
// Test length that exceeds remaining string after offset
$result = substr_compare('abc', 'def', 1, 10);
echo "Length exceeds remaining: " . ($result < 0 ? "PASS" : "FAIL") . "\n";
?>
--EXPECT--
Length exceeds remaining: PASS
--CLEAN--
<?php
unset($result);
