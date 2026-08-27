--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
str_pad() raises an OOM fatal on its no-padding (verbatim) return path
--DESCRIPTION--
Run under a small per-allocation cap (PHL_MAX_ALLOC, set by `make test-stress`).
When the pad length is <= the input length, str_pad returns the input verbatim;
that append must also raise a fatal on OOM, not a truncated string.
--SKIPIF--
<?php if (!getenv('PHL_MAX_ALLOC')) { echo "skip needs PHL_MAX_ALLOC cap (run: make test-stress)"; } ?>
--FILE--
<?php
$big = str_repeat('A', 2000000);
$s = str_pad($big, 5); // pad length < input length -> verbatim return path
echo "UNREACHABLE len=" . strlen($s) . "\n";
?>
--EXPECTF--
Error: PH7 is running out of memory in %s on line %d
--CLEAN--
<?php
