--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
explode() surfaces an out-of-memory fatal instead of a truncated array
--DESCRIPTION--
Run under a small per-allocation cap (PHL_MAX_ALLOC, set by `make test-stress`).
The input stays under the cap (~900 KB) but explodes into ~450k elements, whose
hashmap bucket table reallocates past the cap, so explode must raise a fatal.
--SKIPIF--
<?php if (!getenv('PHL_MAX_ALLOC')) { echo "skip needs PHL_MAX_ALLOC cap (run: make test-stress)"; } ?>
--FILE--
<?php
$r = explode(',', str_repeat('a,', 450000));
echo "UNREACHABLE count=" . count($r) . "\n";
?>
--EXPECTF--
Error: PH7 is running out of memory in %s on line %d
--CLEAN--
<?php
