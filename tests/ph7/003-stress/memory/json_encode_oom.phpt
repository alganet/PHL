--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
json_encode() surfaces an out-of-memory fatal instead of a truncated string
--DESCRIPTION--
Run under a small per-allocation cap (PHL_MAX_ALLOC, set by `make test-stress`).
Encoding two long strings grows the JSON result buffer past the cap, so
json_encode must raise a fatal (not a JSON error, not a truncated string).
--SKIPIF--
<?php if (!getenv('PHL_MAX_ALLOC')) { echo "skip needs PHL_MAX_ALLOC cap (run: make test-stress)"; } ?>
--FILE--
<?php
$s = json_encode([str_repeat('A', 600000), str_repeat('B', 600000)]);
echo "UNREACHABLE len=" . strlen($s) . "\n";
?>
--EXPECTF--
Error: PH7 is running out of memory in %s on line %d
--CLEAN--
<?php
