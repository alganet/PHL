--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
.= in-place append surfaces an out-of-memory fatal instead of a truncated result
--DESCRIPTION--
Run under a small per-allocation cap (PHL_MAX_ALLOC, set by `make test-stress`).
The OP_CAT_STORE in-place fast path appends directly into the lvalue's buffer;
when that buffer must grow past the cap the append must raise a fatal, not
silently truncate. Companion to concat_oom.phpt (which covers the OP_CAT `.`
path).
--SKIPIF--
<?php if (!getenv('PHL_MAX_ALLOC')) { echo "skip needs PHL_MAX_ALLOC cap (run: make test-stress)"; } ?>
--FILE--
<?php
$s = '';
$chunk = str_repeat('A', 100000);
/* Each chunk fits under the cap, but the accumulator's geometric growth soon
 * needs a single buffer larger than the cap -> the in-place append must fatal. */
for ($i = 0; $i < 100; $i++) { $s .= $chunk; }
echo "UNREACHABLE len=" . strlen($s) . "\n";
?>
--EXPECTF--
Error: PH7 is running out of memory in %s on line %d
--CLEAN--
<?php
