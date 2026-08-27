--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
sprintf() surfaces an out-of-memory fatal instead of a truncated result
--DESCRIPTION--
Run under a small per-allocation cap (PHL_MAX_ALLOC, set by `make test-stress`).
Large string arguments grow the result buffer past the cap as the %s fields are
appended (a single field's width is clamped to the format buffer, so width
padding alone cannot trigger it), so sprintf must raise a fatal.
--SKIPIF--
<?php if (!getenv('PHL_MAX_ALLOC')) { echo "skip needs PHL_MAX_ALLOC cap (run: make test-stress)"; } ?>
--FILE--
<?php
$s = str_repeat('A', 600000);
$out = sprintf('%s%s', $s, $s);
echo "UNREACHABLE len=" . strlen($out) . "\n";
?>
--EXPECTF--
Error: PH7 is running out of memory in %s on line %d
--CLEAN--
<?php
