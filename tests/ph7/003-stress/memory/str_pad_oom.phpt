--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
str_pad() surfaces an out-of-memory fatal instead of a truncated result
--DESCRIPTION--
Run under a small per-allocation cap (PHL_MAX_ALLOC, set by `make test-stress`).
Padding to ~2 MB grows the result buffer past the cap, so str_pad must raise a
fatal rather than return a silently-truncated string.
--SKIPIF--
<?php if (!getenv('PHL_MAX_ALLOC')) { echo "skip needs PHL_MAX_ALLOC cap (run: make test-stress)"; } ?>
--FILE--
<?php
$s = str_pad('x', 2000000, 'AB');
echo "UNREACHABLE len=" . strlen($s) . "\n";
?>
--EXPECTF--
Error: PH7 is running out of memory in %s on line %d
--CLEAN--
<?php
