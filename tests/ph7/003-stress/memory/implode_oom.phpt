--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
implode() surfaces an out-of-memory fatal instead of a truncated joined string
--DESCRIPTION--
Run under a small per-allocation cap (PHL_MAX_ALLOC, set by `make test-stress`).
Each piece builds under the cap; joining them needs a result buffer past the
cap, so implode must raise a fatal, not return a truncated string.
--SKIPIF--
<?php if (!getenv('PHL_MAX_ALLOC')) { echo "skip needs PHL_MAX_ALLOC cap (run: make test-stress)"; } ?>
--FILE--
<?php
$s = implode(',', [str_repeat('A', 600000), str_repeat('B', 600000)]);
echo "UNREACHABLE len=" . strlen($s) . "\n";
?>
--EXPECTF--
%s Error:  PH7 is running out of memory
--CLEAN--
<?php
