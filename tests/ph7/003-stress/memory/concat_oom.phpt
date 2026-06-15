--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
String concatenation surfaces an out-of-memory fatal instead of a truncated result
--DESCRIPTION--
Run under a small per-allocation cap (PHL_MAX_ALLOC, set by `make test-stress`).
Each operand builds under the cap; concatenating them needs a buffer past the
cap, so the OP_CAT path must raise a fatal, not produce a truncated string.
--SKIPIF--
<?php if (!getenv('PHL_MAX_ALLOC')) { echo "skip needs PHL_MAX_ALLOC cap (run: make test-stress)"; } ?>
--FILE--
<?php
$a = str_repeat('A', 500000);
$b = str_repeat('B', 500000);
$c = $a . $b;
echo "UNREACHABLE len=" . strlen($c) . "\n";
?>
--EXPECTF--
%s Error:  PH7 is running out of memory
--CLEAN--
<?php
