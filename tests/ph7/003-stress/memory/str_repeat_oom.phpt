--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
str_repeat surfaces an out-of-memory fatal instead of a truncated string
--DESCRIPTION--
Run under a small per-allocation cap (PHL_MAX_ALLOC, set by `make test-stress`).
The allocation fails mid-build; the engine must raise a fatal, not return a
silently-truncated string with a success status.
--SKIPIF--
<?php if (!getenv('PHL_MAX_ALLOC')) { echo "skip needs PHL_MAX_ALLOC cap (run: make test-stress)"; } ?>
--FILE--
<?php
$s = str_repeat('A', 8000000);
echo "UNREACHABLE len=" . strlen($s) . "\n";
?>
--EXPECTF--
Error: PH7 is running out of memory in %s on line %d
--CLEAN--
<?php
